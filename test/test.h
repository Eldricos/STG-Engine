/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef TEST_TEST_H
#define TEST_TEST_H

// A minimal test harness so the tests build with nothing but a compiler.

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "common/fs.h"

namespace Test {

struct TestCase {
	const char *name;
	std::function<void()> func;
};

std::vector<TestCase> &registry();
void reportFailure(const char *file, int line, const std::string &message);

struct Registrar {
	Registrar(const char *name, std::function<void()> func) { registry().push_back({ name, func }); }
};

/** A fresh, empty directory under the system temp directory, removed on destruction. */
class TempDir {
public:
	TempDir();
	~TempDir();
	const Common::FSNode &node() const { return _node; }
	/** Write a file relative to the directory, creating subdirectories. */
	void writeFile(const std::string &name, const std::vector<unsigned char> &data) const;
	void writeFile(const std::string &name, const std::string &data) const;

private:
	Common::FSNode _node;
};

/**
 * Build an 8-bit .BMP with a two colour palette (black, orange).
 * pixels holds top-down rows, or the raw RLE8 data if rle is set.
 */
std::vector<unsigned char> makeBMP8(int width, int height, const std::vector<unsigned char> &pixels, bool rle);

/** A small ZIP file (see test_zip.cpp) containing README.TXT, SHORT.TXT, Data/Log.txt and RAW.BIN. */
extern const unsigned char kTestZip[];
extern const size_t kTestZipSize;

} // End of namespace Test

#define TEST_CONCAT2(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT2(a, b)

#define TEST(name)                                                                  \
	static void name();                                                             \
	static Test::Registrar TEST_CONCAT(registrar_, name)(#name, name);              \
	static void name()

#define CHECK(cond)                                                                 \
	do {                                                                            \
		if (!(cond))                                                                \
			Test::reportFailure(__FILE__, __LINE__, "CHECK(" #cond ") failed");    \
	} while (0)

#define CHECK_EQ(a, b)                                                              \
	do {                                                                            \
		auto _a = (a);                                                              \
		auto _b = (b);                                                              \
		if (!(_a == _b))                                                            \
			Test::reportFailure(__FILE__, __LINE__, "CHECK_EQ(" #a ", " #b ") failed"); \
	} while (0)

#endif
