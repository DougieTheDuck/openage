// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <memory>

class QAudioSink;
class QIODevice;

namespace openage::audio {

class Mixer;

/**
 * Plays a Mixer on the system's default output with QAudioSink in pull mode.
 * Create and destroy it on a thread that runs a Qt event loop.
 */
class Output {
public:
	/**
	 * Open the default audio device.
	 * @returns nullptr (after logging why) when there is no usable device
	 */
	static std::unique_ptr<Output> open(const std::shared_ptr<Mixer> &mixer);

	~Output();

	Output(const Output &) = delete;
	Output &operator=(const Output &) = delete;

private:
	Output() = default;

	std::unique_ptr<QIODevice> source;
	std::unique_ptr<QAudioSink> sink;
};

} // namespace openage::audio
