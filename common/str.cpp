/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "common/str.h"

#include <cctype>
#include <cstdarg>
#include <cstdio>

namespace Common {

String toLower(const String &str) {
	String result = str;
	for (char &c : result)
		c = (char)tolower((unsigned char)c);
	return result;
}

String toUpper(const String &str) {
	String result = str;
	for (char &c : result)
		c = (char)toupper((unsigned char)c);
	return result;
}

String trim(const String &str) {
	size_t start = 0, end = str.size();
	while (start < end && isspace((unsigned char)str[start]))
		start++;
	while (end > start && isspace((unsigned char)str[end - 1]))
		end--;
	return str.substr(start, end - start);
}

bool equalsIgnoreCase(const String &a, const String &b) {
	if (a.size() != b.size())
		return false;
	for (size_t i = 0; i < a.size(); i++) {
		if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
			return false;
	}
	return true;
}

bool hasSuffixIgnoreCase(const String &str, const String &suffix) {
	if (suffix.size() > str.size())
		return false;
	return equalsIgnoreCase(str.substr(str.size() - suffix.size()), suffix);
}

StringArray split(const String &str, char separator) {
	StringArray result;
	size_t start = 0;
	while (true) {
		size_t pos = str.find(separator, start);
		if (pos == String::npos) {
			result.push_back(str.substr(start));
			break;
		}
		result.push_back(str.substr(start, pos - start));
		start = pos + 1;
	}
	return result;
}

String format(const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	va_list copy;
	va_copy(copy, va);
	int len = vsnprintf(nullptr, 0, fmt, copy);
	va_end(copy);

	String result;
	if (len > 0) {
		result.resize(len + 1);
		vsnprintf(&result[0], len + 1, fmt, va);
		result.resize(len);
	}
	va_end(va);
	return result;
}

static bool matchHelper(const char *str, const char *pat) {
	while (*pat) {
		if (*pat == '*') {
			// Collapse runs of '*' and try every possible split.
			while (*pat == '*')
				pat++;
			if (!*pat)
				return true;
			for (; *str; str++) {
				if (matchHelper(str, pat))
					return true;
			}
			return false;
		}
		if (!*str)
			return false;
		if (*pat != '?' && tolower((unsigned char)*pat) != tolower((unsigned char)*str))
			return false;
		pat++;
		str++;
	}
	return *str == 0;
}

bool matchString(const String &str, const String &pattern) {
	return matchHelper(str.c_str(), pattern.c_str());
}

} // End of namespace Common
