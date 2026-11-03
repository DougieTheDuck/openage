// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "output.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QIODevice>
#include <QMediaDevices>

#include "../log/log.h"
#include "mixer.h"
#include "pcm.h"

namespace openage::audio {

namespace {

/** Endless PCM source that QAudioSink pulls from; it asks the mixer each time. */
class MixerDevice : public QIODevice {
public:
	explicit MixerDevice(std::shared_ptr<Mixer> mixer) :
		mixer{std::move(mixer)} {}

	bool isSequential() const override {
		return true;
	}

	qint64 bytesAvailable() const override {
		return QIODevice::bytesAvailable() + (1 << 20);
	}

protected:
	qint64 readData(char *data, qint64 max_size) override {
		constexpr qint64 frame_bytes = OUTPUT_CHANNELS * sizeof(int16_t);
		const qint64 frames = max_size / frame_bytes;
		if (frames <= 0) {
			return 0;
		}
		this->mixer->render(reinterpret_cast<int16_t *>(data), static_cast<size_t>(frames));
		return frames * frame_bytes;
	}

	qint64 writeData(const char *, qint64) override {
		return -1;
	}

private:
	std::shared_ptr<Mixer> mixer;
};

} // namespace

std::unique_ptr<Output> Output::open(const std::shared_ptr<Mixer> &mixer) {
	const QAudioDevice device = QMediaDevices::defaultAudioOutput();
	if (device.isNull()) {
		log::log(INFO << "Audio: no output device, sound is off");
		return nullptr;
	}

	QAudioFormat format;
	format.setSampleRate(OUTPUT_RATE);
	format.setChannelCount(OUTPUT_CHANNELS);
	format.setSampleFormat(QAudioFormat::Int16);
	if (not device.isFormatSupported(format)) {
		// PipeWire/WASAPI usually convert anyway; try before giving up.
		log::log(WARN << "Audio: device " << device.description().toStdString()
		              << " does not report 48 kHz stereo 16-bit support; trying anyway");
	}

	auto output = std::unique_ptr<Output>{new Output{}};
	output->sink = std::make_unique<QAudioSink>(device, format);
	// ~100 ms of headroom rides out a slow frame without crackling.
	output->sink->setBufferSize(OUTPUT_RATE / 10 * OUTPUT_CHANNELS * static_cast<qsizetype>(sizeof(int16_t)));
	output->source = std::make_unique<MixerDevice>(mixer);
	output->source->open(QIODevice::ReadOnly);
	output->sink->start(output->source.get());

	if (output->sink->error() != QAudio::NoError) {
		log::log(WARN << "Audio: could not start output on "
		              << device.description().toStdString()
		              << " (error " << static_cast<int>(output->sink->error()) << "), sound is off");
		return nullptr;
	}

	log::log(INFO << "Audio: playing on " << device.description().toStdString());
	return output;
}

Output::~Output() {
	if (this->sink) {
		this->sink->stop();
	}
	this->sink.reset();
	this->source.reset();
}

} // namespace openage::audio
