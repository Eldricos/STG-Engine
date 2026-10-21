/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "common/audio/audiostream.h"

namespace Audio {

namespace {

class LoopingAudioStream : public AudioStream {
public:
	LoopingAudioStream(std::unique_ptr<RewindableAudioStream> stream, uint loops)
		: _stream(std::move(stream)), _loops(loops) {}

	int readBuffer(int16 *buffer, int numSamples) override {
		int total = 0;
		while (total < numSamples && !_done) {
			int n = _stream->readBuffer(buffer + total, numSamples - total);
			total += n;
			if (_stream->endOfData()) {
				_completed++;
				if ((_loops && _completed >= _loops) || !_stream->rewind()) {
					_done = true;
				} else if (n == 0 && _stream->endOfData()) {
					// An empty stream would otherwise spin forever.
					_done = true;
				}
			}
		}
		return total;
	}

	bool isStereo() const override { return _stream->isStereo(); }
	int getRate() const override { return _stream->getRate(); }
	bool endOfData() const override { return _done; }

private:
	std::unique_ptr<RewindableAudioStream> _stream;
	uint _loops;
	uint _completed = 0;
	bool _done = false;
};

} // End of anonymous namespace

std::unique_ptr<AudioStream> makeLoopingAudioStream(std::unique_ptr<RewindableAudioStream> stream, uint loops) {
	if (loops == 1)
		return stream;
	return std::unique_ptr<AudioStream>(new LoopingAudioStream(std::move(stream), loops));
}

} // End of namespace Audio
