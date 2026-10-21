/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "test/test.h"

#include <cstring>
#include <filesystem>
#include <random>

namespace Test {

static int g_failures = 0;

std::vector<TestCase> &registry() {
	static std::vector<TestCase> tests;
	return tests;
}

void reportFailure(const char *file, int line, const std::string &message) {
	fprintf(stderr, "  %s:%d: %s\n", file, line, message.c_str());
	g_failures++;
}

TempDir::TempDir() {
	std::random_device rd;
	std::filesystem::path path = std::filesystem::temp_directory_path() / ("stg-test-" + std::to_string(rd()));
	std::filesystem::create_directories(path);
	_node = Common::FSNode(path);
}

TempDir::~TempDir() {
	std::error_code ec;
	std::filesystem::remove_all(_node.getNativePath(), ec);
}

void TempDir::writeFile(const std::string &name, const std::vector<unsigned char> &data) const {
	std::filesystem::path path = _node.getNativePath() / name;
	std::filesystem::create_directories(path.parent_path());
	FILE *f = fopen(path.string().c_str(), "wb");
	if (!f)
		return;
	fwrite(data.data(), 1, data.size(), f);
	fclose(f);
}

void TempDir::writeFile(const std::string &name, const std::string &data) const {
	writeFile(name, std::vector<unsigned char>(data.begin(), data.end()));
}

} // End of namespace Test

int main(int argc, char *argv[]) {
	const char *filter = argc > 1 ? argv[1] : nullptr;
	int run = 0, failed = 0;
	for (const Test::TestCase &test : Test::registry()) {
		if (filter && !strstr(test.name, filter))
			continue;
		int before = Test::g_failures;
		test.func();
		run++;
		bool ok = Test::g_failures == before;
		if (!ok)
			failed++;
		printf("[%s] %s\n", ok ? " OK " : "FAIL", test.name);
	}
	printf("%d tests, %d failed\n", run, failed);
	return failed ? 1 : 0;
}
