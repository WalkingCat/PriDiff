#pragma once
#include <string>

std::wstring utf82utf16(const std::string_view sv);
std::wstring ansi2utf16(const std::string_view sv);

