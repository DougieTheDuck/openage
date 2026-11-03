// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace openage::audio {

/** Every decoded sound is interleaved stereo int16 at this rate. */
constexpr int OUTPUT_RATE = 48000;
constexpr int OUTPUT_CHANNELS = 2;

/** A fully decoded sound effect: interleaved stereo int16 at OUTPUT_RATE. */
struct PcmClip {
	std::vector<int16_t> samples{};

	size_t frames() const {
		return this->samples.size() / OUTPUT_CHANNELS;
	}
};

/**
 * Decode a whole .opus or .wav file held in memory.
 * WAV may be 8/16/24-bit integer or 32-bit float PCM, mono or stereo, at any
 * rate; it is converted to OUTPUT_RATE stereo.
 *
 * @param bytes the file contents
 * @param name file name, used to pick the format by suffix and in errors
 * @throws audio::Error when the data cannot be decoded
 */
std::shared_ptr<const PcmClip> decode_clip(const std::string &bytes, const std::string &name);

/** A source that is read incrementally, used for long music tracks. */
class Stream {
public:
	virtual ~Stream() = default;

	/**
	 * Read up to `frames` stereo frames into `out`.
	 * @returns frames written; 0 means the end of the track.
	 */
	virtual size_t read(int16_t *out, size_t frames) = 0;

	/** Seek back to the start. */
	virtual void rewind() = 0;
};

/**
 * Open a music track held in memory. Opus is decoded while it plays; WAV is
 * decoded up front.
 *
 * @throws audio::Error when the data cannot be decoded
 */
std::unique_ptr<Stream> open_stream(std::string bytes, const std::string &name);

/** Stream over an already-decoded clip. */
class ClipStream : public Stream {
public:
	explicit ClipStream(std::shared_ptr<const PcmClip> clip);
	size_t read(int16_t *out, size_t frames) override;
	void rewind() override;

private:
	std::shared_ptr<const PcmClip> clip;
	size_t position = 0;
};

/** Whether a file name has a suffix decode_clip understands. */
bool is_supported_sound_file(const std::string &name);

} // namespace openage::audio
