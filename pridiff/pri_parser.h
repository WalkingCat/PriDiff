#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>

namespace pri {
	struct byte_span_t {
		uint32_t offset, size;
		byte_span_t() = default;
		byte_span_t(const byte_span_t&) = default;
		byte_span_t(byte_span_t&&) = default;
		byte_span_t(uint32_t _offset, uint32_t _size) : offset(_offset), size(_size) {}
	};

	enum class qualifier_type_t : uint16_t
	{
		Language,
		Contrast,
		Scale,
		HomeRegion,
		TargetSize,
		LayoutDirection,
		Theme,
		AlternateForm,
		DXFeatureLevel,
		Configuration,
		DeviceFamily,
		Custom
	};

	struct qualifier_t {
		qualifier_type_t type;
		std::wstring value;
	};

	struct qualifier_set_t {
		std::vector<qualifier_t> qualifiers;
	};

	struct decision_t {
		std::vector<qualifier_set_t> qualifier_sets;
	};
	////////////////////////////////////////////////////////////////////////////////
	enum class resource_value_type_t : uint32_t
	{
		String,
		Path,
		EmbeddedData,
		AsciiString,
		Utf8String,
		AsciiPath,
		Utf8Path
	};

	struct candidate_t {
		resource_value_type_t value_type;
		unsigned char type;
		union {
			struct type_1_t {
				uint16_t source_file_index;
				uint16_t data_item_section_index;
				uint16_t data_item_index;
			} type_1;
			struct type_0_t {
				byte_span_t data_span;
			} type_0;
		};
	};

	struct candidate_set_t {
		uint16_t schema_section_index;
		uint16_t resource_map_item_index;
		uint16_t decision_index;
		std::vector<candidate_t> candidates;
	};

	struct resource_map_scope {
		std::wstring name;
	};
	struct resource_map_item {
		uint16_t scope_index;
		std::wstring name;
	};
	////////////////////////////////////////////////////////////////////////////////
	struct resource_map_section_t {
		std::vector<candidate_set_t> candidate_sets;
	};
	struct hierarchical_schema_section_t {
		std::map<uint16_t, resource_map_scope> resource_map_scopes;
		std::map<uint16_t, resource_map_item> resource_map_items;
	};
	struct data_item_section_t {
		std::vector<std::vector<uint8_t>> data_items;
	};
	struct pri_data_t {
		std::map<uint16_t, std::vector<decision_t>> decision_info_sections;
		std::map<uint16_t, resource_map_section_t> resource_map_sections;
		std::map<uint16_t, hierarchical_schema_section_t> hierarchical_schema_sections;
		std::map<uint16_t, data_item_section_t> data_item_sections;
	};

	pri_data_t parse_pri_file(const wchar_t* filename);
}