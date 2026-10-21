/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_AUDIO_MIXER_H
#define COMMON_AUDIO_MIXER_H

#include <memory>
#include <mutex>
#include <vector>

#include "common/audio/audiostream.h"
#include "common/types.h"

namespace Audio {

class Channel;

/** Identifies a playing sound. Stays valid (but inactive) after the sound ends. */
struct SoundHandle {
	uint32 val = 0xFFFFFFFF;
};

/**
 * A software mixer producing 16-bit stereo output at a fixed rate.
 *
 * Engines start and stop sounds; the backend's audio callback calls
 * mixCallback() from its own thread to pull mixed samples. All public
 * methods are thread safe.
 */
class Mixer {
public:
	enum SoundType {
		kPlainSoundType = 0,
		kMusicSoundType = 1,
		kSFXSoundType = 2,
		kSpeechSoundType = 3
	};

	static const int kMaxChannels = 32;
	static const int kMaxChannelVolume = 255;
	static const int kMaxMixerVolume = 256;

	explicit Mixer(uint outputRate);
	~Mixer();

	uint getOutputRate() const { return _outputRate; }

	/** Set by the backend once its audio device is running. */
	void setReady(bool ready);
	bool isReady() const;

	/**
	 * Start playing a stream. The mixer takes ownership.
	 * @param balance  -127 (left) .. 0 (centre) .. 127 (right)
	 * @param id       Optional caller-chosen id; playing another sound with
	 *                 the same id stops the first.
	 */
	void playStream(SoundType type, SoundHandle *handle, std::unique_ptr<AudioStream> stream,
	                int id = -1, byte volume = kMaxChannelVolume, int8 balance = 0);

	void stopAll();
	void stopID(int id);
	void stopHandle(SoundHandle handle);

	void pauseAll(bool paused);
	void pauseHandle(SoundHandle handle, bool paused);

	bool isSoundHandleActive(SoundHandle handle) const;
	bool isSoundTypeActive(SoundType type) const;

	void setChannelVolume(SoundHandle handle, byte volume);
	void setChannelBalance(SoundHandle handle, int8 balance);

	/** Volume for a whole category, 0..kMaxMixerVolume. */
	void setVolumeForSoundType(SoundType type, int volume);
	int getVolumeForSoundType(SoundType type) const;

	void muteSoundType(SoundType type, bool mute);

	/**
	 * Fill samples with numFrames interleaved stereo frames. Called by the
	 * backend from its audio thread.
	 */
	void mixCallback(int16 *samples, uint numFrames);

private:
	int findChannel(SoundHandle handle) const;

	mutable std::mutex _mutex;
	uint _outputRate;
	bool _ready = false;
	bool _paused = false;
	uint32 _handleSeed = 0;
	std::unique_ptr<Channel> _channels[kMaxChannels];
	int _typeVolume[4];
	bool _typeMuted[4];
	std::vector<int32> _mixBuffer;
};

} // End of namespace Audio

#endif
