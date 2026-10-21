/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "common/audio/mixer.h"
#include "common/debug.h"

#include <algorithm>
#include <cstring>

namespace Audio {

/**
 * One playing stream, resampled to the output rate with linear
 * interpolation and mixed with its volume and balance.
 */
class Channel {
public:
	Channel(Mixer::SoundType type, std::unique_ptr<AudioStream> stream, uint32 handle, int id,
	        byte volume, int8 balance, uint outputRate)
		: _type(type), _stream(std::move(stream)), _handle(handle), _id(id), _volume(volume), _balance(balance) {
		_stereo = _stream->isStereo();
		int rate = _stream->getRate();
		if (rate <= 0)
			rate = (int)outputRate;
		_step = (uint32)(((uint64)rate << 16) / outputRate);
		// Prime the interpolator with the first two frames.
		_finished = !readFrame(_cur) || !readFrame(_next);
	}

	Mixer::SoundType getType() const { return _type; }
	uint32 getHandle() const { return _handle; }
	int getId() const { return _id; }
	bool isFinished() const { return _finished; }

	void setVolume(byte volume) { _volume = volume; }
	void setBalance(int8 balance) { _balance = balance; }
	void pause(bool paused) { _paused = paused; }

	/** Add numFrames stereo frames into the accumulator. */
	void mix(int32 *out, uint numFrames, int typeVolume) {
		if (_paused || _finished)
			return;

		// Per-side gain in 8.8 fixed point, derived from volume, balance and the category volume.
		int vol = _volume * typeVolume / Mixer::kMaxMixerVolume;
		int left = vol, right = vol;
		if (_balance > 0)
			left = vol * (127 - _balance) / 127;
		else if (_balance < 0)
			right = vol * (127 + _balance) / 127;

		for (uint i = 0; i < numFrames; i++) {
			int32 frac = (int32)(_frac & 0xFFFF);
			int32 l = _cur[0] + (((_next[0] - _cur[0]) * frac) >> 16);
			int32 r = _cur[1] + (((_next[1] - _cur[1]) * frac) >> 16);
			out[i * 2 + 0] += (l * left) >> 8;
			out[i * 2 + 1] += (r * right) >> 8;

			_frac += _step;
			while (_frac >= 0x10000) {
				_frac -= 0x10000;
				_cur[0] = _next[0];
				_cur[1] = _next[1];
				if (!readFrame(_next)) {
					_finished = true;
					return;
				}
			}
		}
	}

private:
	bool readFrame(int32 frame[2]) {
		uint needed = _stereo ? 2 : 1;
		if (_bufPos + needed > _bufLen) {
			// Keep any leftover half frame, then refill.
			uint leftover = _bufLen - _bufPos;
			memmove(_buffer, _buffer + _bufPos, leftover * sizeof(int16));
			int n = _stream->readBuffer(_buffer + leftover, kBufferSize - leftover);
			_bufLen = leftover + (n > 0 ? n : 0);
			_bufPos = 0;
			if (_bufLen < needed)
				return false;
		}
		frame[0] = _buffer[_bufPos];
		frame[1] = _stereo ? _buffer[_bufPos + 1] : frame[0];
		_bufPos += needed;
		return true;
	}

	static const uint kBufferSize = 512;

	Mixer::SoundType _type;
	std::unique_ptr<AudioStream> _stream;
	uint32 _handle;
	int _id;
	byte _volume;
	int8 _balance;
	bool _stereo;
	bool _paused = false;
	bool _finished = false;

	uint32 _step;
	uint32 _frac = 0;
	int32 _cur[2] = { 0, 0 };
	int32 _next[2] = { 0, 0 };

	int16 _buffer[kBufferSize];
	uint _bufPos = 0;
	uint _bufLen = 0;
};

Mixer::Mixer(uint outputRate) : _outputRate(outputRate) {
	for (int i = 0; i < 4; i++) {
		_typeVolume[i] = kMaxMixerVolume;
		_typeMuted[i] = false;
	}
}

Mixer::~Mixer() {
	stopAll();
}

void Mixer::setReady(bool ready) {
	std::lock_guard<std::mutex> lock(_mutex);
	_ready = ready;
}

bool Mixer::isReady() const {
	std::lock_guard<std::mutex> lock(_mutex);
	return _ready;
}

int Mixer::findChannel(SoundHandle handle) const {
	int index = (int)(handle.val % kMaxChannels);
	if (handle.val == 0xFFFFFFFF || !_channels[index] || _channels[index]->getHandle() != handle.val)
		return -1;
	return index;
}

void Mixer::playStream(SoundType type, SoundHandle *handle, std::unique_ptr<AudioStream> stream,
                       int id, byte volume, int8 balance) {
	if (!stream)
		return;

	std::lock_guard<std::mutex> lock(_mutex);

	if (id != -1) {
		for (auto &channel : _channels) {
			if (channel && channel->getId() == id)
				channel.reset();
		}
	}

	int index = -1;
	for (int i = 0; i < kMaxChannels; i++) {
		if (!_channels[i]) {
			index = i;
			break;
		}
	}
	if (index == -1) {
		warning("Mixer: no free channel, sound dropped");
		return;
	}

	// The handle encodes the channel index so lookups are O(1), and a
	// sequence number so stale handles don't match a reused channel.
	uint32 handleVal = index + _handleSeed * kMaxChannels;
	_handleSeed++;
	_channels[index].reset(new Channel(type, std::move(stream), handleVal, id, volume, balance, _outputRate));
	if (handle)
		handle->val = handleVal;
}

void Mixer::stopAll() {
	std::lock_guard<std::mutex> lock(_mutex);
	for (auto &channel : _channels)
		channel.reset();
}

void Mixer::stopID(int id) {
	std::lock_guard<std::mutex> lock(_mutex);
	for (auto &channel : _channels) {
		if (channel && channel->getId() == id)
			channel.reset();
	}
}

void Mixer::stopHandle(SoundHandle handle) {
	std::lock_guard<std::mutex> lock(_mutex);
	int index = findChannel(handle);
	if (index != -1)
		_channels[index].reset();
}

void Mixer::pauseAll(bool paused) {
	std::lock_guard<std::mutex> lock(_mutex);
	_paused = paused;
}

void Mixer::pauseHandle(SoundHandle handle, bool paused) {
	std::lock_guard<std::mutex> lock(_mutex);
	int index = findChannel(handle);
	if (index != -1)
		_channels[index]->pause(paused);
}

bool Mixer::isSoundHandleActive(SoundHandle handle) const {
	std::lock_guard<std::mutex> lock(_mutex);
	return findChannel(handle) != -1;
}

bool Mixer::isSoundTypeActive(SoundType type) const {
	std::lock_guard<std::mutex> lock(_mutex);
	for (const auto &channel : _channels) {
		if (channel && channel->getType() == type)
			return true;
	}
	return false;
}

void Mixer::setChannelVolume(SoundHandle handle, byte volume) {
	std::lock_guard<std::mutex> lock(_mutex);
	int index = findChannel(handle);
	if (index != -1)
		_channels[index]->setVolume(volume);
}

void Mixer::setChannelBalance(SoundHandle handle, int8 balance) {
	std::lock_guard<std::mutex> lock(_mutex);
	int index = findChannel(handle);
	if (index != -1)
		_channels[index]->setBalance(balance);
}

void Mixer::setVolumeForSoundType(SoundType type, int volume) {
	std::lock_guard<std::mutex> lock(_mutex);
	_typeVolume[type] = std::clamp(volume, 0, (int)kMaxMixerVolume);
}

int Mixer::getVolumeForSoundType(SoundType type) const {
	std::lock_guard<std::mutex> lock(_mutex);
	return _typeVolume[type];
}

void Mixer::muteSoundType(SoundType type, bool mute) {
	std::lock_guard<std::mutex> lock(_mutex);
	_typeMuted[type] = mute;
}

void Mixer::mixCallback(int16 *samples, uint numFrames) {
	std::lock_guard<std::mutex> lock(_mutex);

	memset(samples, 0, numFrames * 2 * sizeof(int16));
	if (!_ready || _paused)
		return;

	_mixBuffer.assign(numFrames * 2, 0);
	for (auto &channel : _channels) {
		if (!channel)
			continue;
		SoundType type = channel->getType();
		channel->mix(_mixBuffer.data(), numFrames, _typeMuted[type] ? 0 : _typeVolume[type]);
		if (channel->isFinished())
			channel.reset();
	}

	for (uint i = 0; i < numFrames * 2; i++)
		samples[i] = (int16)std::clamp<int32>(_mixBuffer[i], -32768, 32767);
}

} // End of namespace Audio
