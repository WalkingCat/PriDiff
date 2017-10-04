#include "stdafx.h"
#include "pri_parser.h"
#include "strutils.h"
#include "fileutils.h"

using namespace std;
using namespace pri;

enum diff_options {
	diffNone = 0x0,
	diffOld = 0x1,
	diffNew = 0x2,
	diffOutput = 0x40000000,
	diffHelp = 0x80000000,
};

const struct { const wchar_t* arg; const wchar_t* arg_alt; const wchar_t* params_desc; const wchar_t* description; const diff_options options; } cmd_options[] = {
	{ L"?",		L"help",			nullptr,		L"show this help",						diffHelp },
	{ L"n",		L"new",				L"<filename>",	L"specify new file(s)",					diffNew },
	{ L"o",		L"old",				L"<filename>",	L"specify old file(s)",					diffOld },
};

void print_usage() {
	printf_s("\tUsage: pridiff [options]\n\n");
	for (auto o = begin(cmd_options); o != end(cmd_options); ++o) {
		if (o->arg != nullptr) printf_s("\t-%S", o->arg); else printf_s("\t");

		int len = 0;
		if (o->arg_alt != nullptr) {
			len = wcslen(o->arg_alt);
			printf_s("\t--%S", o->arg_alt);
		} else printf_s("\t");

		if (len < 6) printf_s("\t");

		if (o->params_desc != nullptr) len += printf_s(" %S", o->params_desc);

		if (len < 14) printf_s("\t");

		printf_s("\t: %S\n", o->description);
	}
}

map<wstring, wstring> find_files(const wchar_t* pattern);
template<typename TKey, typename TValue, typename TFunc> void diff_maps(const map<TKey, TValue>& new_map, const map<TKey, TValue>& old_map, TFunc& func);
struct pri_resource_t {
	std::map<std::wstring, std::wstring> values;
};

std::map<std::wstring, pri_resource_t> get_pri_resources(const wstring& pri_file);

int wmain(int argc, wchar_t* argv[])
{
	int options = diffNone;
	const wchar_t* err_arg = nullptr;
	wstring new_files_pattern, old_files_pattern;

	printf_s("\n PriDiff v0.1 https://github.com/WalkingCat/PriDiff\n\n");

	for (int i = 1; i < argc; ++i) {
		const wchar_t* arg = argv[i];
		if ((arg[0] == '-') || ((arg[0] == '/'))) {
			diff_options curent_option = diffNone;
			if ((arg[0] == '-') && (arg[1] == '-')) {
				for (auto o = begin(cmd_options); o != end(cmd_options); ++o) {
					if ((o->arg_alt != nullptr) && (wcscmp(arg + 2, o->arg_alt) == 0)) { curent_option = o->options; }
				}
			} else {
				for (auto o = begin(cmd_options); o != end(cmd_options); ++o) {
					if ((o->arg != nullptr) && (wcscmp(arg + 1, o->arg) == 0)) { curent_option = o->options; }
				}
			}

			bool valid = false;
			if (curent_option != diffNone) {
				valid = true;
				if (curent_option == diffNew) {
					if ((i + 1) < argc) new_files_pattern = argv[++i];
					else valid = false;
				} else if (curent_option == diffOld) {
					if ((i + 1) < argc) old_files_pattern = argv[++i];
					else valid = false;
				} else options = (options | curent_option);
			}
			if (!valid && (err_arg == nullptr)) err_arg = arg;
		} else { if (new_files_pattern.empty()) new_files_pattern = arg; else err_arg = arg; }
	}

	if ((new_files_pattern.empty() && old_files_pattern.empty()) || (err_arg != nullptr) || (options & diffHelp)) {
		if (err_arg != nullptr) printf_s("\tError in option: %S\n\n", err_arg);
		print_usage();
		return 0;
	}

	auto out = stdout;
	setmode_to_utf16(out);

	auto new_files = find_files(new_files_pattern.c_str());
	auto old_files = find_files(old_files_pattern.c_str());

	fwprintf_s(out, L" new files: %ws%ws\n", new_files_pattern.c_str(), !new_files.empty() ? L"" : L" (NOT EXISTS!)");
	fwprintf_s(out, L" old files: %ws%ws\n", old_files_pattern.c_str(), !old_files.empty() ? L"" : L" (NOT EXISTS!)");

	fwprintf_s(out, L"\n");

	if (new_files.empty() & old_files.empty()) return 0; // at least one of them must exists

	if ((new_files.size() == 1) && (old_files.size() == 1)) {
		// allows diff single files with different names
		auto& new_file_name = new_files.begin()->first;
		auto& old_file_name = old_files.begin()->first;
		if (new_file_name != old_file_name) {
			auto diff_file_names = new_file_name + L" <=> " + old_file_name;
			auto new_file = new_files.begin()->second;
			new_files.clear();
			new_files[diff_file_names] = new_file;
			auto old_file = old_files.begin()->second;
			old_files.clear();
			old_files[diff_file_names] = old_file;
		}
	}

	fwprintf_s(out, L" diff legends: +: added, -: removed, *: changed, $: changed (original)\n");

	diff_maps(new_files, old_files,
		[&](const wstring& file_name, const wstring * new_file, const wstring * old_file) {
			bool printed_file_name = false;
			auto print_file_name = [&](const wchar_t prefix) {
				if (!printed_file_name) {
					fwprintf_s(out, L"\n %wc FILE: %ws\n", prefix, file_name.c_str());
					printed_file_name = true;
				}
			};

			if (new_file == nullptr) {
				print_file_name('-');
				return;
			}

			diff_maps(get_pri_resources(*new_file), (old_file != nullptr) ? get_pri_resources(*old_file) : map<wstring, pri_resource_t>(),
				[&](const wstring& res_name, const pri_resource_t* new_res, const pri_resource_t* old_res) {
					bool printed_res_name = false;
					auto print_res_name = [&](const wchar_t prefix) {
						if (!printed_res_name) {
							print_file_name((old_file == nullptr) ? L'+' : L'*');
							fwprintf_s(out, L"  %wc %ws\n", prefix, res_name.c_str());
							printed_res_name = true;
						}
					};

					if (new_res == nullptr) {
						print_res_name('-');
						return;
					}

					diff_maps(new_res->values, old_res ? old_res->values : map<wstring, wstring>(),
						[&](const wstring& qualifier, const wstring* new_value, const wstring* old_value) {
							auto print_res_value = [&](const wchar_t prefix, bool old = false) {
								print_res_name((old_res == nullptr) ? L'+' : L'*');
								auto value_text = old ? *old_value : *new_value;
								if (!qualifier.empty()) {
									value_text = L"[" + qualifier + L"]" + value_text;
								}
								fwprintf_s(out, L"   %wc %ws\n", prefix, value_text.c_str());
							};

							if (new_value == nullptr) {
								print_res_value('-', true);
								return;
							}

							if (old_value) {
								if (*new_value != *old_value) {
									print_res_value('*');
									print_res_value('$', true);
								}
							} else {
								print_res_value('+');
							}
						}
					);
				}
			);
		}
	);

	return 0;
}

map<wstring, wstring> find_files(const wchar_t * pattern)
{
	map<wstring, wstring> ret;
	wchar_t path[MAX_PATH] = {};
	wcscpy_s(path, pattern);
	WIN32_FIND_DATA fd;
	HANDLE find = ::FindFirstFile(pattern, &fd);
	if (find != INVALID_HANDLE_VALUE) {
		do {
			if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
				PathRemoveFileSpec(path);
				PathCombine(path, path, fd.cFileName);
				ret[fd.cFileName] = path;
			}
		} while (::FindNextFile(find, &fd));
		::FindClose(find);
	}
	return ret;
}

std::map<std::wstring, pri_resource_t> get_pri_resources(const wstring& pri_file) {
	std::map<std::wstring, pri_resource_t> pri_resources;

	auto& pri_data = parse_pri_file(pri_file.c_str());
	
	for (auto& hierarchical_schema_section : pri_data.hierarchical_schema_sections) {
		for (auto& item_pair : hierarchical_schema_section.second.resource_map_items) {
			auto& scope = hierarchical_schema_section.second.resource_map_scopes[item_pair.second.scope_index];
			pri_resources[scope.name + L"/" + item_pair.second.name].values.clear();
		}
	}

	for (auto& resource_map_section_pair : pri_data.resource_map_sections) {
		const auto& resource_map_section = resource_map_section_pair.second;
		for (auto& candidate_set : resource_map_section.candidate_sets) {
			auto& hierarchical_schema_section = pri_data.hierarchical_schema_sections[candidate_set.schema_section_index];
			auto& item = hierarchical_schema_section.resource_map_items[candidate_set.resource_map_item_index];
			auto& scope = hierarchical_schema_section.resource_map_scopes[item.scope_index];
			auto& pri_resource = pri_resources[scope.name + L"/" + item.name];
			for (auto& candidate : candidate_set.candidates) {
				wstring qualifier_text; //TODO
				wstring value_text;
				if (candidate.type == 0x00) {
					value_text = L"[Data]";
				} else if (candidate.type == 0x01) {
					if (candidate.type_1.source_file_index == uint16_t(-1)) {
						auto& data_items = pri_data.data_item_sections[candidate.type_1.data_item_section_index].data_items;
						auto& data_item = data_items[candidate.type_1.data_item_index];
						switch (candidate.value_type) {
						case resource_value_type_t::AsciiPath:
						case resource_value_type_t::AsciiString:
							value_text = ansi2utf16(string((char*)data_item.data(), data_item.size()));
							break;
						case resource_value_type_t::Utf8Path:
						case resource_value_type_t::Utf8String:
							value_text = utf82utf16(string((char*)data_item.data(), data_item.size()));
							break;
						case resource_value_type_t::Path:
						case resource_value_type_t::String:
							value_text = wstring((wchar_t*)data_item.data(), data_item.size());
							break;
						case resource_value_type_t::EmbeddedData:
							//TODO: calc sha1 hash
							value_text = L"[EmbeddedData(Size=" + to_wstring(data_item.size()) + L")]";
							break;
						default:
							value_text = L"[DataItem(" + to_wstring(candidate.type_1.data_item_section_index) +
								L"-" + to_wstring(candidate.type_1.data_item_index) + L")]";
							break;
						}
					} else {
						value_text = L"[ExternalFile]";
					}
				}
				pri_resource.values[qualifier_text] = value_text;
			}
		}
	}
	return pri_resources;
}

template<typename TKey, typename TValue, typename TFunc>
void diff_maps(const map<TKey, TValue>& new_map, const map<TKey, TValue>& old_map, TFunc& func)
{
	auto new_it = new_map.begin();
	auto old_it = old_map.begin();

	while ((new_it != new_map.end()) || (old_it != old_map.end())) {
		int diff = 0;
		if (new_it != new_map.end()) {
			if (old_it != old_map.end()) {
				if (new_it->first > old_it->first) {
					diff = -1;
				} else if (new_it->first < old_it->first) {
					diff = 1;
				}
			} else diff = 1;
		} else {
			if (old_it != old_map.end())
				diff = -1;
		}

		if (diff > 0) {
			func(new_it->first, &new_it->second, nullptr);
			++new_it;
		} else if (diff < 0) {
			func(old_it->first, nullptr, &old_it->second);
			++old_it;
		} else {
			func(old_it->first, &new_it->second, &old_it->second);
			++new_it;
			++old_it;
		}
	}
}
