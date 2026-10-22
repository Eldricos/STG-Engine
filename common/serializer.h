/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_SERIALIZER_H
#define COMMON_SERIALIZER_H

#include <cstring>
#include <vector>

#include "common/str.h"
#include "common/stream.h"

namespace Common {

/**
 * Reads or writes the same fields in the same order, so one sync()
 * function per class both saves and loads it, as ScummVM's does. Values
 * are little endian.
 */
class Serializer {
public:
	/** One of in and out is null. */
	Serializer(ReadStream *in, WriteStream *out) : _in(in), _out(out) {}

	bool isLoading() const { return _in != nullptr; }
	bool isSaving() const { return _out != nullptr; }
	/** A read ran past the end or a stream failed. */
	bool err() const { return _failed || (_in && _in->err()) || (_out && _out->err()); }

	void syncBytes(void *data, uint32 size) {
		if (_in) {
			if (_in->read(data, size) != size)
				_failed = true;
		} else if (_out->write(data, size) != size) {
			_failed = true;
		}
	}

	template<typename T>
	void syncAsInt(T &value, int bytes) {
		byte buffer[8] = {};
		if (_out) {
			uint64 v = (uint64)(int64)value;
			for (int i = 0; i < bytes; i++)
				buffer[i] = (byte)(v >> (8 * i));
		}
		syncBytes(buffer, bytes);
		if (_in) {
			uint64 v = 0;
			for (int i = 0; i < bytes; i++)
				v |= (uint64)buffer[i] << (8 * i);
			// Sign-extend from the stored width.
			if (bytes < 8 && (v >> (8 * bytes - 1)) & 1 && (T)-1 < (T)0)
				v |= ~0ull << (8 * bytes);
			value = (T)v;
		}
	}

	void syncAsByte(byte &value) { syncAsInt(value, 1); }
	void syncAsBool(bool &value) {
		byte b = value ? 1 : 0;
		syncAsInt(b, 1);
		value = b != 0;
	}
	void syncAsUint16(uint16 &value) { syncAsInt(value, 2); }
	void syncAsSint16(int16 &value) { syncAsInt(value, 2); }
	void syncAsUint32(uint32 &value) { syncAsInt(value, 4); }
	void syncAsSint32(int32 &value) { syncAsInt(value, 4); }
	void syncAsInt(int &value) { syncAsInt(value, 4); }
	void syncAsUint64(uint64 &value) { syncAsInt(value, 8); }
	void syncAsDouble(double &value) {
		uint64 bits;
		std::memcpy(&bits, &value, sizeof bits);
		syncAsInt(bits, 8);
		std::memcpy(&value, &bits, sizeof bits);
	}
	void syncString(String &value) {
		uint32 size = (uint32)value.size();
		syncAsUint32(size);
		if (_in) {
			if (size > (1u << 20)) {
				_failed = true;
				return;
			}
			value.resize(size);
		}
		if (size)
			syncBytes(&value[0], size);
	}

	/** A count, then each element through sync(serializer, element). */
	template<typename T, typename F>
	void syncVector(std::vector<T> &values, F sync) {
		uint32 size = (uint32)values.size();
		syncAsUint32(size);
		if (_in) {
			if (size > (1u << 24)) {
				_failed = true;
				return;
			}
			values.resize(size);
		}
		for (T &value : values) {
			if (err())
				return;
			sync(*this, value);
		}
	}

	/**
	 * A count that must match one already known (the same level's tables,
	 * say); a mismatch fails the load.
	 */
	void syncCount(uint32 count) {
		uint32 stored = count;
		syncAsUint32(stored);
		if (stored != count)
			_failed = true;
	}

	void fail() { _failed = true; }

private:
	ReadStream *_in;
	WriteStream *_out;
	bool _failed = false;
};

} // End of namespace Common

#endif
