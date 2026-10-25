/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_MOVIE_H
#define STG_MOVIE_H

#include <memory>
#include <vector>

#include "common/stream.h"
#include "common/types.h"

namespace STG {

/**
 * Cinepak ('cvid'), the codec of the game's movies: frames made of
 * horizontal strips, each 4 x 4 block drawn from codebooks of 2 x 2 YUV
 * cells, one cell stretched over the block (V1) or four (V4). Codebooks
 * carry over between frames, and down from one strip to the next unless
 * the frame says otherwise.
 */
class CinepakDecoder {
public:
	CinepakDecoder(int width, int height);

	/** Decode a frame onto the picture; false if it is damaged. */
	bool decode(const byte *data, uint32 size);

	int getWidth() const { return _width; }
	int getHeight() const { return _height; }
	/** The picture, 0xAARRGGBB, row by row. */
	const std::vector<uint32> &getPicture() const { return _picture; }

private:
	struct Cell {
		byte y[4];
		int8 u, v;
	};
	struct Strip {
		Cell v1[256], v4[256];
	};

	void decodeCodebook(Cell *codebook, int chunk, const byte *data, const byte *end);
	bool decodeVectors(const Strip &strip, int chunk, int top, int bottom, const byte *data, const byte *end);
	uint32 colour(const Cell &cell, int i) const;

	int _width, _height;
	std::vector<uint32> _picture;
	std::vector<Strip> _strips;
};

/**
 * The game's movies (MOVIE/MOV_nn.AVI): Video for Windows files with a
 * Cinepak video stream (640 x 320, 24 frames a second) and 16-bit PCM
 * sound (22050 Hz stereo).
 */
class Movie {
public:
	bool load(std::unique_ptr<Common::SeekableReadStream> stream);

	int getWidth() const { return _width; }
	int getHeight() const { return _height; }
	int getFrameCount() const { return (int)_frames.size(); }
	/** Microseconds a frame. */
	uint32 getFrameTime() const { return _frameTime; }

	/** Decode the next frame; false at the end. */
	bool nextFrame();
	const std::vector<uint32> &getPicture() const { return _decoder->getPicture(); }
	int getCurrentFrame() const { return _current; }

	/** The whole soundtrack, as the file has it; empty if there is none. */
	const std::vector<byte> &getAudio() const { return _audio; }
	int getAudioRate() const { return _audioRate; }
	int getAudioChannels() const { return _audioChannels; }
	int getAudioBits() const { return _audioBits; }

private:
	struct Chunk {
		uint32 offset, size;
	};
	bool readList(uint32 end, int depth);

	std::unique_ptr<Common::SeekableReadStream> _stream;
	std::unique_ptr<CinepakDecoder> _decoder;
	int _width = 0, _height = 0;
	uint32 _frameTime = 41666;
	uint32 _videoHandler = 0;
	int _streams = 0, _videoStream = -1, _audioStream = -1;
	int _audioRate = 0, _audioChannels = 0, _audioBits = 0;
	std::vector<Chunk> _frames;
	std::vector<byte> _audio;
	int _current = -1;
};

} // End of namespace STG

#endif
