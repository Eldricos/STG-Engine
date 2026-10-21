/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/probe.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <vector>

#include "common/endian.h"
#include "common/stream.h"
#include "common/zip.h"

namespace {

/** How much of each file is read; enough for AVI headers. */
const uint32 kProbeReadSize = 64 * 1024;

Common::String fourCC(uint32 tagBE) {
	Common::String s;
	for (int shift = 24; shift >= 0; shift -= 8) {
		char c = (char)((tagBE >> shift) & 0xFF);
		s += isprint((unsigned char)c) ? c : '.';
	}
	return s;
}

Common::String fourCCLE(const byte *p) {
	return fourCC(READ_BE_UINT32(p));
}

const char *waveFormatName(uint16 tag) {
	switch (tag) {
	case 0x0001: return "PCM";
	case 0x0002: return "MS ADPCM";
	case 0x0011: return "IMA ADPCM";
	case 0x0055: return "MP3";
	default: return "unknown";
	}
}

void describeWaveFormat(const byte *p, uint32 size, Common::String &out) {
	if (size < 16)
		return;
	uint16 tag = READ_LE_UINT16(p);
	out += Common::format("audio 0x%04x (%s), %u Hz, %u ch, %u bit", tag, waveFormatName(tag), READ_LE_UINT32(p + 4),
	                      READ_LE_UINT16(p + 2), READ_LE_UINT16(p + 14));
}

/** Walk the RIFF chunk tree of an AVI's header and describe its streams. */
void describeAVI(const std::vector<byte> &data, Common::String &out) {
	Common::String lastType;
	// Flat walk over chunks, descending into LISTs, within what was read.
	std::vector<std::pair<size_t, size_t>> stack = { { 12, data.size() } };
	while (!stack.empty()) {
		size_t pos = stack.back().first;
		size_t end = stack.back().second;
		stack.pop_back();
		while (pos + 8 <= end) {
			uint32 tag = READ_BE_UINT32(&data[pos]);
			uint32 size = READ_LE_UINT32(&data[pos + 4]);
			size_t body = pos + 8;
			size_t next = body + size + (size & 1);
			if (tag == MKTAG('L', 'I', 'S', 'T') && body + 4 <= data.size()) {
				uint32 listType = READ_BE_UINT32(&data[body]);
				if (listType == MKTAG('m', 'o', 'v', 'i'))
					return; // frame data follows; headers are done
				stack.push_back({ next, end });
				stack.push_back({ body + 4, std::min(next, data.size()) });
				break;
			}
			if (body + size > data.size())
				return;
			const byte *p = &data[body];
			if (tag == MKTAG('a', 'v', 'i', 'h') && size >= 40) {
				uint32 usPerFrame = READ_LE_UINT32(p);
				out += Common::format("\n    %ux%u, %u frames, %.2f fps, %u streams", READ_LE_UINT32(p + 32),
				                      READ_LE_UINT32(p + 36), READ_LE_UINT32(p + 16),
				                      usPerFrame ? 1000000.0 / usPerFrame : 0.0, READ_LE_UINT32(p + 24));
			} else if (tag == MKTAG('s', 't', 'r', 'h') && size >= 36) {
				lastType = fourCCLE(p);
				out += Common::format("\n    stream '%s' handler '%s', rate %u/%u, length %u", lastType.c_str(),
				                      fourCCLE(p + 4).c_str(), READ_LE_UINT32(p + 24), READ_LE_UINT32(p + 20),
				                      READ_LE_UINT32(p + 32));
			} else if (tag == MKTAG('s', 't', 'r', 'f')) {
				out += "\n      format: ";
				if (lastType == "vids" && size >= 20) {
					out += Common::format("video '%s', %dx%d, %u bpp", fourCCLE(p + 16).c_str(), (int32)READ_LE_UINT32(p + 4),
					                      (int32)READ_LE_UINT32(p + 8), READ_LE_UINT16(p + 14));
				} else if (lastType == "auds") {
					describeWaveFormat(p, size, out);
				} else {
					out += Common::format("%u bytes", size);
				}
			}
			pos = next;
		}
	}
}

bool looksLikeText(const std::vector<byte> &data) {
	size_t n = std::min<size_t>(data.size(), 512);
	if (n == 0)
		return false;
	for (size_t i = 0; i < n; i++) {
		byte c = data[i];
		if (c < 9 || (c > 13 && c < 32 && c != 26))
			return false;
	}
	return true;
}

/** Heuristic: does the file start with a count followed by ascending offsets? */
Common::String offsetTableHint(const std::vector<byte> &data, int64 fileSize) {
	for (size_t start : { (size_t)0, (size_t)4, (size_t)8 }) {
		if (start + 4 > data.size())
			break;
		uint32 count = READ_LE_UINT32(&data[start]);
		if (count < 2 || count > 100000)
			continue;
		// Try entry strides of 4, 8, 12 and 16 bytes with the offset first.
		for (size_t stride : { 4, 8, 12, 16 }) {
			size_t needed = std::min<size_t>(count, 16);
			if (start + 4 + needed * stride > data.size())
				continue;
			uint32 prev = 0;
			bool ok = true;
			for (size_t i = 0; i < needed && ok; i++) {
				uint32 off = READ_LE_UINT32(&data[start + 4 + i * stride]);
				ok = off > prev && (int64)off <= fileSize;
				prev = off;
			}
			if (ok)
				return Common::format("\n  hint: dword at 0x%zx (%u) may be an entry count, followed by ascending "
				                      "offsets with a %zu byte stride",
				                      start, count, stride);
		}
	}
	return Common::String();
}

void hexDump(const std::vector<byte> &data, uint32 bytes, Common::String &out) {
	size_t n = std::min<size_t>(data.size(), bytes);
	for (size_t row = 0; row < n; row += 16) {
		out += Common::format("\n  %08zx ", row);
		for (size_t i = 0; i < 16; i++) {
			if (row + i < n)
				out += Common::format(" %02x", data[row + i]);
			else
				out += "   ";
			if (i == 7)
				out += " ";
		}
		out += "  |";
		for (size_t i = 0; i < 16 && row + i < n; i++)
			out += isprint(data[row + i]) ? (char)data[row + i] : '.';
		out += "|";
	}
	out += "\n  LE dwords:";
	for (size_t i = 0; i + 4 <= std::min<size_t>(n, 64); i += 4)
		out += Common::format(" %u", READ_LE_UINT32(&data[i]));
}

/** Identify a format from the leading bytes of a file or archive member. */
void describeData(const std::vector<byte> &data, int64 fullSize, uint32 dumpBytes, Common::String &out) {
	if (data.empty()) {
		out += "empty";
		return;
	}

	auto has = [&](size_t offset, const char *magic) {
		size_t len = strlen(magic);
		return data.size() >= offset + len && memcmp(&data[offset], magic, len) == 0;
	};

	if (has(0, "RIFF") && data.size() >= 12) {
		Common::String form = fourCCLE(&data[8]);
		out += "RIFF " + form;
		if (form == "AVI ") {
			describeAVI(data, out);
		} else if (form == "WAVE") {
			for (size_t pos = 12; pos + 8 <= data.size();) {
				uint32 size = READ_LE_UINT32(&data[pos + 4]);
				if (READ_BE_UINT32(&data[pos]) == MKTAG('f', 'm', 't', ' ') && pos + 8 + size <= data.size()) {
					out += ", ";
					describeWaveFormat(&data[pos + 8], size, out);
					break;
				}
				pos += 8 + size + (size & 1);
			}
		}
		return;
	}
	if (has(0, "SGEN") && data.size() >= 40) {
		// Star Trek: Generations location script; see engines/stg/script.h.
		out += Common::format("SGEN script v%u, %u bytes of code, sections", READ_LE_UINT16(&data[4]),
		                      READ_LE_UINT32(&data[12]));
		uint64 namesAt = 40 + (uint64)READ_LE_UINT32(&data[12]);
		for (int i = 0; i < 6; i++) {
			uint32 size = READ_LE_UINT32(&data[16 + i * 4]);
			out += Common::format(" %u", size);
			namesAt += size;
		}
		if (namesAt < (uint64)fullSize && fullSize <= (int64)data.size()) {
			out += "\n  resources:";
			for (size_t i = (size_t)namesAt; i < data.size();) {
				Common::String name((const char *)&data[i]);
				if (!name.empty())
					out += " " + name;
				i += name.size() + 1;
			}
		}
		return;
	}
	if (has(0, "MZ")) {
		bool pe = false;
		if (data.size() >= 0x40) {
			uint32 peOffset = READ_LE_UINT32(&data[0x3C]);
			pe = peOffset + 4 <= data.size() && memcmp(&data[peOffset], "PE\0\0", 4) == 0;
		}
		out += pe ? "Windows PE executable" : "DOS executable";
		return;
	}
	if (has(0, "BM") && data.size() >= 30) {
		out += Common::format("BMP %dx%d, %u bpp", (int32)READ_LE_UINT32(&data[18]), (int32)READ_LE_UINT32(&data[22]),
		                      READ_LE_UINT16(&data[28]));
		return;
	}
	if (data.size() >= 12 && (READ_LE_UINT16(&data[4]) == 0xAF11 || READ_LE_UINT16(&data[4]) == 0xAF12)) {
		out += Common::format("FLIC animation %ux%u, %u frames", READ_LE_UINT16(&data[8]), READ_LE_UINT16(&data[10]),
		                      READ_LE_UINT16(&data[6]));
		return;
	}
	if (has(0, "Creative Voice File")) {
		out += "Creative Voice (VOC)";
		return;
	}
	if (has(0, "FORM")) {
		out += "IFF FORM " + (data.size() >= 12 ? fourCCLE(&data[8]) : Common::String());
		return;
	}
	if (has(0, "PK\x03\x04")) {
		out += "ZIP archive";
		return;
	}
	if (has(0, "\x13\x5d\x65\x8c")) {
		out += "InstallShield compressed archive";
		return;
	}
	if (looksLikeText(data)) {
		out += "text";
		// Short text files (INI files and the like) are shown in full.
		if (data.size() <= 1024) {
			for (const Common::String &line : Common::split(Common::String(data.begin(), data.end()), '\n'))
				out += "\n  | " + Common::trim(line);
		}
		return;
	}

	out += "unknown";
	out += offsetTableHint(data, fullSize);
	hexDump(data, dumpBytes, out);
}

Common::String extensionOf(const Common::String &name) {
	size_t dot = name.rfind('.');
	size_t slash = name.rfind('/');
	if (dot == Common::String::npos || (slash != Common::String::npos && dot < slash))
		return "(none)";
	return Common::toUpper(name.substr(dot + 1));
}

/** Members larger than this are not decompressed just to sample them. */
const uint32 kMaxSampleSize = 32 * 1024 * 1024;

void describeZip(const Common::FSNode &file, uint32 dumpBytes, bool listAll, Common::String &out) {
	std::shared_ptr<Common::ZipArchive> zip = Common::ZipArchive::open(file);
	if (!zip) {
		out += "ZIP archive (could not read the central directory)";
		return;
	}

	const std::vector<Common::ZipArchive::Entry> &entries = zip->getEntries();
	uint64 total = 0;
	int encrypted = 0;
	std::map<Common::String, int> methods;
	struct ExtInfo {
		int count = 0;
		uint64 size = 0;
		const Common::ZipArchive::Entry *sample = nullptr;
	};
	std::map<Common::String, ExtInfo> extensions;
	for (const Common::ZipArchive::Entry &entry : entries) {
		total += entry.size;
		encrypted += entry.isEncrypted();
		methods[Common::ZipArchive::getMethodName(entry.method)]++;
		ExtInfo &info = extensions[extensionOf(entry.name)];
		info.count++;
		info.size += entry.size;
		if (!info.sample)
			info.sample = &entry;
	}

	out += Common::format("ZIP archive, %d members, %llu bytes uncompressed", (int)entries.size(),
	                      (unsigned long long)total);
	for (const auto &m : methods)
		out += Common::format(", %d %s", m.second, m.first.c_str());
	if (encrypted)
		out += Common::format(", %d ENCRYPTED", encrypted);

	for (const auto &ext : extensions) {
		const ExtInfo &info = ext.second;
		out += Common::format("\n  .%-6s %5d files %12llu bytes   e.g. %s: ", ext.first.c_str(), info.count,
		                      (unsigned long long)info.size, info.sample->name.c_str());
		if (info.sample->size > kMaxSampleSize) {
			out += "too large to sample";
			continue;
		}
		std::vector<byte> data;
		if (!zip->extract(*info.sample, data)) {
			out += Common::format("could not extract (%s)", Common::ZipArchive::getMethodName(info.sample->method));
			continue;
		}
		int64 fullSize = (int64)data.size();
		if (data.size() > kProbeReadSize)
			data.resize(kProbeReadSize);
		Common::String desc;
		describeData(data, fullSize, dumpBytes, desc);
		// Indent the sample's own detail lines under its extension line.
		size_t pos = 0;
		while ((pos = desc.find('\n', pos)) != Common::String::npos) {
			desc.insert(pos + 1, "    ");
			pos += 5;
		}
		out += desc;
	}

	if (listAll) {
		out += "\n  Members:";
		for (const Common::ZipArchive::Entry &entry : entries)
			out += Common::format("\n    %-40s %10u  %s", entry.name.c_str(), entry.size,
			                      Common::ZipArchive::getMethodName(entry.method));
	}
}

} // End of anonymous namespace

Common::String probeFile(const Common::FSNode &file, const Common::String &name, uint32 dumpBytes, bool listMembers) {
	int64 fileSize = file.getFileSize();
	Common::String out = Common::format("%s (%lld bytes): ", name.c_str(), (long long)fileSize);

	std::unique_ptr<Common::SeekableReadStream> stream = file.createReadStream();
	if (!stream)
		return out + "unreadable";
	std::vector<byte> data(std::min<int64>(fileSize, kProbeReadSize));
	data.resize(stream->read(data.data(), (uint32)data.size()));

	// Scripts keep their resource list at the very end, so read them whole.
	if (data.size() >= 4 && memcmp(data.data(), "SGEN", 4) == 0 && fileSize <= 4 * 1024 * 1024) {
		stream->seek(0);
		data = stream->readAll();
	}

	if (data.size() >= 4 && memcmp(data.data(), "PK\x03\x04", 4) == 0)
		describeZip(file, dumpBytes, listMembers, out);
	else
		describeData(data, fileSize, dumpBytes, out);
	return out;
}
