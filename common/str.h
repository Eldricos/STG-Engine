/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_STR_H
#define COMMON_STR_H

#include <string>
#include <vector>

namespace Common {

typedef std::string String;
typedef std::vector<std::string> StringArray;

String toLower(const String &str);
String toUpper(const String &str);
String trim(const String &str);
bool equalsIgnoreCase(const String &a, const String &b);
bool hasSuffixIgnoreCase(const String &str, const String &suffix);
StringArray split(const String &str, char separator);
String format(const char *fmt, ...);

/**
 * Simple glob matching supporting '*' and '?', case-insensitive.
 * Old DOS/Windows titles are not consistent with filename case.
 */
bool matchString(const String &str, const String &pattern);

} // End of namespace Common

#endif
