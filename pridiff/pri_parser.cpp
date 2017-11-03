#include "stdafx.h"
#include "pri_parser.h"
#include "file_reader.h"
#include "strutils.h"

using namespace std;
using namespace pri;

pri_data_t pri::parse_pri_file(const wchar_t* filename) {
	file_reader file(filename, L"rb");

	uint64_t header_mark = file.read<uint64_t>();

	file.skip<uint32_t>();

	uint32_t file_size = file.read<uint32_t>();

	uint32_t sec_dir_begin_offset = file.read<uint32_t>();
	uint32_t sec_dir_end_offset = file.read<uint32_t>();
	uint16_t sections_count = file.read<uint16_t>();

	file.reset(file_size - 16);

	uint32_t end_magic = file.read<uint32_t>();

	file.read<uint32_t>();

	uint64_t end_mark = file.read<uint64_t>();

	struct section_id {
		char value[16];
		section_id() { memset(value, 0, sizeof(value)); }
		section_id(const char _value[17]) { memcpy_s(value, sizeof(value), _value, sizeof(value)); }
		bool operator <(const section_id& that) const {
			return memcmp(value, that.value, sizeof(value)) < 0;
		}
		bool operator ==(const section_id& that) const {
			return memcmp(value, that.value, sizeof(value)) == 0;
		}
	};

	struct section_dir_t {
		section_id id;
		uint32_t d1, d2;
		byte_span_t byte_span;
	};

	uint32_t v8 = sec_dir_begin_offset + sections_count * sizeof(section_dir_t);

	vector<section_dir_t> sections;

	file.reset(sec_dir_begin_offset);
	for (uint32_t i = 0; i < sections_count; i++) {
		section_dir_t s = file.read<section_dir_t>();
		sections.push_back(s);
	}

	pri_data_t pri_data;

	for (uint16_t section_index = 0; section_index < sections.size(); ++section_index) {
		auto& section = sections[section_index];
		file.reset(sec_dir_end_offset + section.byte_span.offset);

		auto id = file.read<section_id>();
		file.skip(8);
		uint32_t sec_length = file.read<uint32_t>();
		file.skip<uint32_t>();

		const auto section_offset = file.offset();

		if (id == "[mrm_decn_info]\0") {
			uint16_t distinct_qualifiers_count = file.read<uint16_t>();
			uint16_t qualifiers_count = file.read<uint16_t>();
			uint16_t qualifier_sets_count = file.read<uint16_t>();
			uint16_t decisions_count = file.read<uint16_t>();
			uint16_t index_table_entries_count = file.read<uint16_t>();
			uint16_t total_data_length = file.read<uint16_t>();

			struct decision_info_t {
				uint16_t firstQualifierSetIndexIndex, numQualifierSetsInDecision;
			};
			vector<decision_info_t> decision_infos;
			for (uint16_t i = 0; i < decisions_count; ++i) {
				decision_infos.push_back(file.read<decision_info_t>());
			}

			struct qualifier_set_info_t {
				uint16_t firstQualifierIndexIndex, qualifiers_in_set_count;
			};
			vector<qualifier_set_info_t> qualifier_set_infos;
			for (uint16_t i = 0; i < qualifier_sets_count; ++i) {
				qualifier_set_infos.push_back(file.read<qualifier_set_info_t>());
			}

			struct qualifier_info_t {
				uint16_t index, priority, fallbackScore;
			};
			vector<qualifier_info_t> qualifier_infos;
			for (uint16_t i = 0; i < qualifiers_count; ++i) {
				qualifier_infos.push_back(file.read<qualifier_info_t>());
				file.skip<uint16_t>();
			}

			struct distinct_qualifier_info_t {
				qualifier_type_t qualifierType;
				uint32_t operandValueOffset;
			};
			vector<distinct_qualifier_info_t>distinct_qualifier_infos;
			for (uint16_t i = 0; i < distinct_qualifiers_count; ++i) {
				distinct_qualifier_info_t dqi;
				file.skip<uint16_t>();
				dqi.qualifierType = file.read<qualifier_type_t>();
				file.skip<uint16_t>();
				file.skip<uint16_t>();
				dqi.operandValueOffset = file.read<uint32_t>();
				distinct_qualifier_infos.push_back(dqi);
			}

			vector<uint16_t> index_table;
			for (uint16_t i = 0; i < index_table_entries_count; ++i) {
				index_table.push_back(file.read<uint16_t>());
			}

			vector<qualifier_t> qualifiers;
			const auto offset = file.offset();
			for (uint16_t i = 0; i < qualifiers_count; ++i) {
				auto& distinct_qualifier_info = distinct_qualifier_infos[qualifier_infos[i].index];
				file.reset(offset + distinct_qualifier_info.operandValueOffset * 2);
				qualifier_t qualifier;
				qualifier.type = distinct_qualifier_info.qualifierType;
				wchar_t c = L'\0';
				do {
					c = file.read<wchar_t>();
					qualifier.value.push_back(c);
				} while (c != L'\0');
				qualifiers.push_back(qualifier);
			}

			vector<qualifier_set_t> qualifierSets;
			for (uint16_t i = 0; i < qualifier_sets_count; ++i) {
				auto& qualifierSetInfo = qualifier_set_infos[i];
				qualifier_set_t qualifierSet;
				for (int j = 0; j < qualifierSetInfo.qualifiers_in_set_count; j++) {
					qualifierSet.qualifiers.push_back(qualifiers[index_table[qualifierSetInfo.firstQualifierIndexIndex + j]]);
				}
				qualifierSets.emplace_back(move(qualifierSet));
			}

			std::vector<decision_t> decisions;
			for (uint16_t i = 0; i < decisions_count; ++i) {
				decision_t decision;
				for (int j = 0; j < decision_infos[i].numQualifierSetsInDecision; j++) {
					decision.qualifier_sets.push_back(qualifierSets[index_table[decision_infos[i].firstQualifierSetIndexIndex + j]]);
				}
				decisions.emplace_back(move(decision));
			}

			pri_data.decision_info_sections[section_index] = move(decisions);
		} else if ((id == "[mrm_hschema]  \0") || (id == "[mrm_hschemaex] ")) {
			file.read<uint16_t>();
			auto uniqueNameLength = file.read<uint16_t>();
			auto nameLength = file.read<uint16_t>();
			file.read<uint16_t>();

			bool extendedHNames = false;
			if (id == "[mrm_hschemaex] ") {
				char hnames_type[16] = {};
				file.read(hnames_type, sizeof(hnames_type));
				if (memcpy_s(hnames_type, sizeof(hnames_type), "[def_hnamesx]  \0", sizeof(hnames_type)) == 0) {
					extendedHNames = true;
				} // else "[def_hnames]   \0"
			}

			auto majorVersion = file.read<uint16_t>();
			auto minorVersion = file.read<uint16_t>();
			file.read<uint32_t>();
			auto checksum = file.read<uint32_t>();
			auto scopes_count = file.read<uint32_t>();
			auto items_count = file.read<uint32_t>();

			wstring unique_name;
			while (true) {
				wchar_t c = file.read<wchar_t>();
				if (c != L'\0') unique_name.push_back(c);
				else break;
			}
			wstring name;
			while (true) {
				wchar_t c = file.read<wchar_t>();
				if (c != L'\0') name.push_back(c);
				else break;
			}

			file.read<uint16_t>();
			auto maxFullPathLength = file.read<uint16_t>();
			file.read<uint16_t>();
			file.read<uint32_t>();// numScopes + numItems;
			file.read<uint32_t>();// numScopes;
			file.read<uint32_t>();// numItems;
			auto unicode_data_length = file.read<uint32_t>();
			file.read<uint32_t>();

			if (extendedHNames)
				file.read<uint32_t>(); // meaning unknown

			struct ScopeAndItemInfo {
				uint16_t parent;
				uint16_t fullPathLength;
				bool isScope;
				bool name_is_ascii;
				uint32_t name_offset;
				uint16_t index;
			};
			vector<ScopeAndItemInfo> scope_and_item_infos;
			for (uint32_t i = 0; i < scopes_count + items_count; ++i) {
				ScopeAndItemInfo scopeAndItemInfo;
				scopeAndItemInfo.parent = file.read<uint16_t>();
				scopeAndItemInfo.fullPathLength = file.read<uint16_t>();
				wchar_t uppercaseFirstChar = file.read<wchar_t>();
				auto nameLength2 = file.read<BYTE>();
				auto flags = file.read<BYTE>();
				scopeAndItemInfo.name_offset = file.read<uint16_t>() | uint32_t((flags & 0xF) << 16);
				scopeAndItemInfo.index = file.read<uint16_t>();
				scopeAndItemInfo.isScope = (flags & 0x10) != 0;
				scopeAndItemInfo.name_is_ascii = (flags & 0x20) != 0;

				scope_and_item_infos.push_back(scopeAndItemInfo);
			}

			struct ScopeExInfo {
				uint16_t scopeIndex, childCount, firstChildIndex;
			};
			vector<ScopeExInfo> scopeExInfos;
			for (uint32_t i = 0; i < scopes_count; ++i) {
				scopeExInfos.push_back(file.read<ScopeExInfo>());
				file.read<uint16_t>();
			}

			vector<uint16_t> itemIndexPropertyToIndex;
			for (uint32_t i = 0; i < items_count; ++i) {
				itemIndexPropertyToIndex.push_back(file.read<uint16_t>());
			}

			auto unicode_data_offset = file.offset();
			auto ascii_data_offset = unicode_data_offset + unicode_data_length * sizeof(wchar_t);

			for (uint32_t i = 0; i < scopes_count + items_count; ++i)
			{
				long pos = scope_and_item_infos[i].name_is_ascii ?
					(ascii_data_offset + scope_and_item_infos[i].name_offset) :
					(unicode_data_offset + scope_and_item_infos[i].name_offset * sizeof(wchar_t));

				file.reset(pos);

				auto& section = pri_data.hierarchical_schema_sections[section_index];
				wstring name;
				if (scope_and_item_infos[i].fullPathLength != 0) {
					if (scope_and_item_infos[i].name_is_ascii) {
						string name_ascii;
						while (true) {
							auto c = file.read<char>();
							if (c != '\0') name_ascii.push_back(c);
							else break;
						}
						name = ansi2utf16(name_ascii);
					} else {
						wstring name;
						while (true) {
							auto c = file.read<wchar_t>();
							if (c != L'\0') name.push_back(c);
							else break;
						}
					}
				}

				if (scope_and_item_infos[i].isScope) {
					auto& scope = section.resource_map_scopes[scope_and_item_infos[i].index];
					scope.name = name;
				} else {
					auto& item = section.resource_map_items[scope_and_item_infos[i].index];
					item.name = name;
					item.scope_index = scope_and_item_infos[i].parent;
				}
			}
		} else if ((id == "[mrm_res_map__]\0") || (id == "[mrm_res_map2_]\0")) {
			auto env_res_len = file.read<uint16_t>();
			auto env_res_num = file.read<uint16_t>();

			auto hschema_sect_idx = file.read<uint16_t>();
			auto hschema_ref_len = file.read<uint16_t>();
			auto decninfo_sect_idx = file.read<uint16_t>();
			auto res_value_type_count = file.read<uint16_t>();
			auto ItemToItemInfoGroupCount = file.read<uint16_t>();
			auto itemInfoGroupCount = file.read<uint16_t>();
			auto itemInfoCount = file.read<uint32_t>();
			auto numCandidates = file.read<uint32_t>();
			auto dataLength = file.read<uint32_t>();
			auto largeTableLength = file.read<uint32_t>();

			file.skip(env_res_len);

			file.skip(hschema_ref_len);

			vector<resource_value_type_t> res_value_types;
			for (uint16_t i = 0; i < res_value_type_count; ++i) {
				file.skip<uint32_t>(); // 4
				res_value_types.push_back(file.read<resource_value_type_t>());
			}

			struct item_to_item_info_group_t {
				uint16_t firstItem, itemInfoGroup;
			};
			vector<item_to_item_info_group_t> item_item_info_group_map;
			for (uint16_t i = 0; i < ItemToItemInfoGroupCount; ++i) {
				item_item_info_group_map.push_back(file.read<item_to_item_info_group_t>());
			}

			struct item_info_group_t {
				uint16_t size;
				uint16_t first_item_info;
			};
			vector<item_info_group_t> item_info_groups;
			for (uint16_t i = 0; i < itemInfoGroupCount; ++i) {
				item_info_groups.push_back(file.read<item_info_group_t>());
			}

			struct item_info {
				uint16_t decision;
				uint16_t first_candidate;
			};
			vector<item_info> item_infos;
			for (uint32_t i = 0; i < itemInfoCount; ++i) {
				item_infos.push_back(file.read<item_info>());
			}

			file.skip(largeTableLength);

			struct candidate_type_0 {
				uint16_t size;
				uint32_t offset;
			};

			struct candidate_type_1 {
				uint16_t source_file_index;
				uint16_t data_item_index;
				uint16_t data_item_section;
			};

			struct candidate_info {
				unsigned char type;
				resource_value_type_t value_type;
				union {
					candidate_type_0 type_0;
					candidate_type_1 type_1;
				};
			};
			vector <candidate_info> candidate_infos;
			for (uint32_t i = 0; i < numCandidates; ++i) {
				candidate_info info;
				info.type = file.read<unsigned char>();
				info.value_type = res_value_types[file.read<unsigned char>()];
				if (info.type == 0x00) {
					info.type_0 = file.read <candidate_type_0>();
				} else if (info.type == 0x01) {
					info.type_1 = file.read <candidate_type_1>();
				}
				candidate_infos.push_back(info);
			}

			const auto string_data_offset = file.offset();

			vector<candidate_set_t> candidate_sets;

			for (auto& item_to_item_info_group : item_item_info_group_map)
			{
				item_info_group_t itemInfoGroup;

				if (item_to_item_info_group.itemInfoGroup < item_info_groups.size()) {
					itemInfoGroup = item_info_groups[item_to_item_info_group.itemInfoGroup];
				} else {
					itemInfoGroup.size = 1;
					itemInfoGroup.first_item_info = item_to_item_info_group.itemInfoGroup - (uint16_t)item_info_groups.size();
				}

				for (uint16_t itemInfoIndex = itemInfoGroup.first_item_info; itemInfoIndex < itemInfoGroup.first_item_info + itemInfoGroup.size; itemInfoIndex++)
				{
					item_info& itemInfo = item_infos[itemInfoIndex];

					uint16_t decisionIndex = itemInfo.decision;

					const auto& decision = pri_data.decision_info_sections[decninfo_sect_idx][decisionIndex];

					vector<candidate_t> candidates;

					for (size_t i = 0; i < decision.qualifier_sets.size(); i++)
					{
						candidate_info& candidateInfo = candidate_infos[itemInfo.first_candidate + i];
						candidate_t candidate;
						candidate.type = candidateInfo.type;
						candidate.value_type = candidateInfo.value_type;
						if (candidateInfo.type == 0x01) {
							if (candidateInfo.type_1.source_file_index == 0)
								candidate.type_1.source_file_index = uint16_t(-1);
							else
								candidate.type_1.source_file_index = candidateInfo.type_1.source_file_index - 1;
							candidate.type_1.data_item_section_index = candidateInfo.type_1.data_item_section;
							candidate.type_1.data_item_index = candidateInfo.type_1.data_item_index;
						} else if (candidateInfo.type == 0x00) {
							candidate.type_0.data_span.offset = section_offset + string_data_offset + candidateInfo.type_0.offset;
							candidate.type_0.data_span.size = candidateInfo.type_0.size;
						}

						candidates.emplace_back(move(candidate));
					}

					candidate_set_t candidate_set;
					candidate_set.schema_section_index = hschema_sect_idx;
					candidate_set.resource_map_item_index = item_to_item_info_group.firstItem + (itemInfoIndex - itemInfoGroup.first_item_info);
					candidate_set.decision_index = decisionIndex;
					candidate_set.candidates = move(candidates);
					candidate_sets.emplace_back(move(candidate_set));
				}
			}

			pri_data.resource_map_sections[section_index].candidate_sets = move(candidate_sets);
		} else if (id == "[mrm_dataitem] \0") {
			file.read<uint32_t>();
			const auto strings_count = file.read<uint16_t>();
			const auto blobs_count = file.read<uint16_t>();
			const auto total_length = file.read<uint32_t>();

			vector<byte_span_t> string_spans, blob_spans;
			for (uint16_t i = 0; i < strings_count; ++i) {
				auto offset = file.read<uint16_t>();
				auto length = file.read<uint16_t>();
				string_spans.emplace_back(offset, length);
			}
			for (uint16_t i = 0; i < blobs_count; ++i) {
				auto offset = file.read<uint32_t>();
				auto length = file.read<uint32_t>();
				blob_spans.emplace_back(offset, length);
			}

			const auto data_offset = file.offset();

			vector<vector<uint8_t>> data_items;
			for (auto& string_span : string_spans) {
				vector<uint8_t> data_item(string_span.size);
				file.reset(data_offset + string_span.offset);
				file.read(data_item.data(), string_span.size);
				data_items.emplace_back(move(data_item));
			}
			for (auto& blob_span : blob_spans) {
				vector<uint8_t> data_item(blob_span.size);
				file.reset(data_offset + blob_span.offset);
				file.read(data_item.data(), blob_span.size);
				data_items.emplace_back(move(data_item));
			}

			pri_data.data_item_sections[section_index].data_items = move(data_items);
		}
	}

	return pri_data;
}