// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "pcm.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <opus/opusfile.h>

#include "../log/log.h"
#include "error.h"

namespace openage::audio {

namespace {

std::string lower_suffix(const std::string &name) {
	const auto dot = name.rfind('.');
	if (dot == std::string::npos) {
		return {};
	}
	std::string suffix = name.substr(dot + 1);
	std::transform(suffix.begin(), suffix.end(), suffix.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return suffix;
}

uint32_t read_u32(const std::string &bytes, size_t at) {
	return static_cast<uint32_t>(static_cast<uint8_t>(bytes[at]))
	       | static_cast<uint32_t>(static_cast<uint8_t>(bytes[at + 1])) << 8
	       | static_cast<uint32_t>(static_cast<uint8_t>(bytes[at + 2])) << 16
	       | static_cast<uint32_t>(static_cast<uint8_t>(bytes[at + 3])) << 24;
}

uint16_t read_u16(const std::string &bytes, size_t at) {
	return static_cast<uint16_t>(static_cast<uint8_t>(bytes[at])
	                             | static_cast<uint8_t>(bytes[at + 1]) << 8);
}

/** One sample from a WAV data chunk, as a float in [-1, 1]. */
float wav_sample(const char *data, uint16_t format, uint16_t bits) {
	const auto *bytes = reinterpret_cast<const uint8_t *>(data);
	if (format == 3) {
		float value;
		std::memcpy(&value, data, sizeof(value));
		return std::isfinite(value) ? std::clamp(value, -1.0f, 1.0f) : 0.0f;
	}
	switch (bits) {
	case 8:
		return (static_cast<float>(bytes[0]) - 128.0f) / 128.0f;
	case 16:
		return static_cast<float>(static_cast<int16_t>(bytes[0] | bytes[1] << 8)) / 32768.0f;
	case 24: {
		int32_t value = bytes[0] | bytes[1] << 8 | bytes[2] << 16;
		if (value & 0x800000) {
			value -= 0x1000000;
		}
		return static_cast<float>(value) / 8388608.0f;
	}
	default:
		return 0.0f;
	}
}

int16_t to_int16(float value) {
	return static_cast<int16_t>(std::lround(std::clamp(value, -1.0f, 1.0f) * 32767.0f));
}

std::shared_ptr<const PcmClip> decode_wav(const std::string &bytes, const std::string &name) {
	if (bytes.size() < 12 or bytes.compare(0, 4, "RIFF") != 0 or bytes.compare(8, 4, "WAVE") != 0) {
		throw Error{MSG(err) << "Not a RIFF/WAVE file: " << name};
	}
	uint16_t format = 0;
	uint16_t channels = 0;
	uint32_t rate = 0;
	uint16_t bits = 0;
	const char *data = nullptr;
	size_t data_size = 0;

	size_t at = 12;
	while (at + 8 <= bytes.size()) {
		const auto chunk = bytes.substr(at, 4);
		const size_t size = read_u32(bytes, at + 4);
		const size_t body = at + 8;
		const size_t available = std::min(size, bytes.size() - body);
		if (chunk == "fmt " and available >= 16) {
			format = read_u16(bytes, body);
			channels = read_u16(bytes, body + 2);
			rate = read_u32(bytes, body + 4);
			bits = read_u16(bytes, body + 14);
			// WAVE_FORMAT_EXTENSIBLE carries the real format in its sub-GUID.
			if (format == 0xFFFE and available >= 26) {
				format = read_u16(bytes, body + 24);
			}
		}
		else if (chunk == "data") {
			data = bytes.data() + body;
			data_size = available;
		}
		at = body + size + (size & 1);
	}

	const bool integer = format == 1 and (bits == 8 or bits == 16 or bits == 24);
	const bool floating = format == 3 and bits == 32;
	if (not data or not(integer or floating) or channels < 1 or channels > 2
	    or rate < 4000 or rate > 192000) {
		throw Error{MSG(err) << "Unsupported WAV (need PCM 8/16/24-bit or float, "
		                     << "mono/stereo): " << name};
	}

	const size_t stride = static_cast<size_t>(bits / 8) * channels;
	const size_t in_frames = data_size / stride;
	auto frame = [&](size_t index, size_t channel) {
		const size_t source = std::min<size_t>(channel, channels - 1);
		return wav_sample(data + index * stride + source * (bits / 8), format, bits);
	};

	auto clip = std::make_shared<PcmClip>();
	if (in_frames == 0) {
		return clip;
	}
	// Linear resampling is ample for short game effects.
	const double step = static_cast<double>(rate) / OUTPUT_RATE;
	const auto out_frames = static_cast<size_t>(std::floor((in_frames - 1) / step)) + 1;
	clip->samples.resize(out_frames * OUTPUT_CHANNELS);
	for (size_t i = 0; i < out_frames; ++i) {
		const double position = i * step;
		const auto left = static_cast<size_t>(position);
		const size_t right = std::min(left + 1, in_frames - 1);
		const auto t = static_cast<float>(position - left);
		for (size_t c = 0; c < OUTPUT_CHANNELS; ++c) {
			const float value = frame(left, c) * (1.0f - t) + frame(right, c) * t;
			clip->samples[i * OUTPUT_CHANNELS + c] = to_int16(value);
		}
	}
	return clip;
}

struct OpusDeleter {
	void operator()(OggOpusFile *file) const {
		op_free(file);
	}
};
using opus_ptr = std::unique_ptr<OggOpusFile, OpusDeleter>;

opus_ptr open_opus(const std::string &bytes, const std::string &name) {
	int error = 0;
	OggOpusFile *file = op_open_memory(
		reinterpret_cast<const unsigned char *>(bytes.data()), bytes.size(), &error);
	if (not file) {
		throw Error{MSG(err) << "Could not open Opus file " << name << " (opusfile error " << error << ")"};
	}
	return opus_ptr{file};
}

/** Opus decoded on demand, so a long track never sits in memory as PCM. */
class OpusStream : public Stream {
public:
	OpusStream(std::string bytes, const std::string &name) :
		bytes{std::move(bytes)},
		name{name},
		file{open_opus(this->bytes, name)} {}

	size_t read(int16_t *out, size_t frames) override {
		size_t written = 0;
		while (written < frames) {
			const int request = static_cast<int>(
				std::min<size_t>(frames - written, 5760) * OUTPUT_CHANNELS);
			const int got = op_read_stereo(this->file.get(), out + written * OUTPUT_CHANNELS, request);
			if (got == OP_HOLE) {
				continue;
			}
			if (got <= 0) {
				if (got < 0) {
					log::log(WARN << "Opus decode error " << got << " in " << this->name);
				}
				break;
			}
			written += static_cast<size_t>(got);
		}
		return written;
	}

	void rewind() override {
		if (op_pcm_seek(this->file.get(), 0) != 0) {
			// A file that can't seek is reopened instead.
			this->file = open_opus(this->bytes, this->name);
		}
	}

private:
	/** opusfile reads from this buffer, so it must outlive `file`. */
	std::string bytes;
	std::string name;
	opus_ptr file;
};

} // namespace

bool is_supported_sound_file(const std::string &name) {
	const auto suffix = lower_suffix(name);
	return suffix == "opus" or suffix == "wav";
}

std::shared_ptr<const PcmClip> decode_clip(const std::string &bytes, const std::string &name) {
	const auto suffix = lower_suffix(name);
	if (suffix == "wav") {
		return decode_wav(bytes, name);
	}
	if (suffix != "opus") {
		throw Error{MSG(err) << "Unsupported sound file type: " << name};
	}
	auto stream = OpusStream{bytes, name};
	auto clip = std::make_shared<PcmClip>();
	std::vector<int16_t> chunk(5760 * OUTPUT_CHANNELS);
	while (true) {
		const size_t got = stream.read(chunk.data(), 5760);
		if (got == 0) {
			break;
		}
		clip->samples.insert(clip->samples.end(), chunk.begin(), chunk.begin() + got * OUTPUT_CHANNELS);
	}
	return clip;
}

std::unique_ptr<Stream> open_stream(std::string bytes, const std::string &name) {
	if (lower_suffix(name) == "opus") {
		return std::make_unique<OpusStream>(std::move(bytes), name);
	}
	return std::make_unique<ClipStream>(decode_clip(bytes, name));
}

ClipStream::ClipStream(std::shared_ptr<const PcmClip> clip) :
	clip{std::move(clip)} {}

size_t ClipStream::read(int16_t *out, size_t frames) {
	const size_t available = this->clip->frames() - this->position;
	const size_t count = std::min(frames, available);
	std::copy_n(this->clip->samples.data() + this->position * OUTPUT_CHANNELS,
	            count * OUTPUT_CHANNELS,
	            out);
	this->position += count;
	return count;
}

void ClipStream::rewind() {
	this->position = 0;
}

} // namespace openage::audio
