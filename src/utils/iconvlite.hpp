/*
Iconv Lite
Simple cpp functions to convert strings from cp1251 to utf8
*/

#pragma once

#include <string>

namespace utils{

std::string cp1251_to_utf8(const std::string &s);

} // namespace utils