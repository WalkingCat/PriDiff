#include "stdafx.h"
#include "strutils.h"

template<UINT codepage> std::wstring mb2unicode(const std::string_view sv) {
	std::wstring ret;

	int len = MultiByteToWideChar(codepage, MB_ERR_INVALID_CHARS, sv.data(), sv.length(), nullptr, 0);

	if (len > 0) {
		ret.resize(len);
		MultiByteToWideChar(codepage, MB_ERR_INVALID_CHARS, sv.data(), sv.length(), const_cast<wchar_t*>(ret.c_str()), len);
	}

	return ret;
}
std::wstring utf82utf16(const std::string_view sv) { return mb2unicode<CP_UTF8>(sv); }
std::wstring ansi2utf16(const std::string_view sv) { return mb2unicode<CP_ACP>(sv); }
