#include "stdafx.h"
#include "pri_parser.h"
#include "strutils.h"
#include "fileutils.h"

using namespace std;
using namespace pri;

struct pri_resource_t {
	std::map<std::wstring, std::wstring> values;
};

std::map<std::wstring, pri_resource_t> get_pri_resources(const wstring& pri_file);

int wmain(int argc, wchar_t* argv[])
{
	prepare_unicode_output();

	wprintf_s(L"\n PriDiff v0.2 https://github.com/WalkingCat/PriDiff\n\n");

	const auto& params = init_diff_params(argc, argv, L"*.pri");

	if (params.show_help || (!params.error.empty()) || (params.new_files_pattern.empty() && params.old_files_pattern.empty())) {
		if (!params.error.empty()) {
			printf_s("\t%ls\n\n", params.error.c_str());
		}
		if (params.show_help) print_cmdl_usage(L"pridiff", diff_cmdl::options, diff_cmdl::default_option);
		return 0;
	}

	auto out = params.out;
	fwprintf_s(out, L"\n legends: +: added, -: removed, *: changed, $: changed (original)\n");

	const map<wstring, wstring> empty_files;
	diff_maps(params.new_file_groups, params.old_file_groups,
		[&](const wstring& group_name, const map<wstring, wstring>* new_files, const map<wstring, wstring>* old_files) {
			bool printed_group_name = false;
			wchar_t printed_group_prefix = L' ';
			auto print_group_name = [&](const wchar_t prefix) {
				if (!printed_group_name) {
					fwprintf_s(out, L"\n %lc %ls (\n", prefix, group_name.c_str());
					printed_group_name = true;
					printed_group_prefix = prefix;
				}
			};

			bool printed_previous_file_name = false;
			diff_maps(new_files ? *new_files : empty_files, old_files ? *old_files : empty_files,
				[&](const wstring& file_name, const wstring * new_file, const wstring * old_file) {
					bool printed_file_name = false;
					auto print_file_name = [&](const wchar_t prefix) {
						if (!printed_file_name) {
							print_group_name(new_files ? old_files ? L'*' : L'+' : L'-');
							if (printed_previous_file_name) {
								fwprintf_s(out, L"\n");
							}
							fwprintf_s(out, L"   %lc %ls\n", prefix, file_name.c_str());
							printed_previous_file_name = printed_file_name = true;
						}
					};

					if (new_file == nullptr) {
						print_file_name('-');
						return;
					}

					if (old_file == nullptr) {
						print_file_name('+');
					}

					diff_maps(get_pri_resources(*new_file), (old_file != nullptr) ? get_pri_resources(*old_file) : map<wstring, pri_resource_t>(),
						[&](const wstring& res_name, const pri_resource_t* new_res, const pri_resource_t* old_res) {
							bool printed_res_name = false;
							auto print_res_name = [&](const wchar_t prefix) {
								if (!printed_res_name) {
									print_file_name(L'*');
									fwprintf_s(out, L"     %lc %ls\n", prefix, res_name.c_str());
									printed_res_name = true;
								}
							};

							if (new_res == nullptr) {
								print_res_name('-');
								return;
							}

							if (old_res == nullptr) {
								print_res_name('+');
							}

							diff_maps(new_res->values, old_res ? old_res->values : map<wstring, wstring>(),
								[&](const wstring& qualifier, const wstring* new_value, const wstring* old_value) {
									auto print_res_value = [&](const wchar_t prefix, bool old = false) {
										print_res_name(L'*');
										auto value_text = old ? *old_value : *new_value;
										if (!qualifier.empty()) {
											value_text = L"[" + qualifier + L"]" + value_text;
										}
										fwprintf_s(out, L"       %lc %ls\n", prefix, value_text.c_str());
									};

									if (new_value == nullptr) {
										print_res_value('-', true);
										return;
									}

									if (old_value) {
										if (*new_value != *old_value) {
											print_res_value('*', false);
											print_res_value('$', true);
										}
									} else {
										print_res_value('+', false);
									}
								}
							);
						}
					);
				}
			);
		}
	);

	return 0;
}


std::wstring get_scope_name(pri::resource_map_scope& scope, pri::hierarchical_schema_section_t& section) {
	auto scope_name = scope.name;
	auto parent_scope_index = scope.parent_scope_index;
	while (parent_scope_index > 0) {
		auto& parent_scope = section.resource_map_scopes[parent_scope_index];
		scope_name = parent_scope.name + L"/" + scope_name;
		parent_scope_index = parent_scope.parent_scope_index;
	}
	return scope_name;
}

std::map<std::wstring, pri_resource_t> get_pri_resources(const wstring& pri_file) {
	std::map<std::wstring, pri_resource_t> pri_resources;

	try {
		auto& pri_data = parse_pri_file(pri_file.c_str());

		for (auto& hierarchical_schema_section : pri_data.hierarchical_schema_sections) {
			for (auto& item_pair : hierarchical_schema_section.second.resource_map_items) {
				auto& scope = hierarchical_schema_section.second.resource_map_scopes[item_pair.second.scope_index];
				pri_resources[get_scope_name(scope, hierarchical_schema_section.second) + L"/" + item_pair.second.name].values.clear();
			}
		}

		for (auto& resource_map_section_pair : pri_data.resource_map_sections) {
			const auto& resource_map_section = resource_map_section_pair.second;
			for (auto& candidate_set : resource_map_section.candidate_sets) {
				auto& hierarchical_schema_section = pri_data.hierarchical_schema_sections[candidate_set.schema_section_index];
				auto& item = hierarchical_schema_section.resource_map_items[candidate_set.resource_map_item_index];
				auto& scope = hierarchical_schema_section.resource_map_scopes[item.scope_index];
				auto& pri_resource = pri_resources[get_scope_name(scope, hierarchical_schema_section) + L"/" + item.name];
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
									value_text = ansi2utf16(string_view((const char*)data_item.data(), data_item.size()));
									break;
								case resource_value_type_t::Utf8Path:
								case resource_value_type_t::Utf8String:
									value_text = utf82utf16(string_view((const char*)data_item.data(), data_item.size()));
									break;
								case resource_value_type_t::Path:
								case resource_value_type_t::String:
									value_text = wstring((const wchar_t*)data_item.data(), data_item.size() / sizeof(wchar_t));
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
	} catch (...) {}

	return pri_resources;
}
