#include "stdafx.h"
#include "metadata_adapter.h"

namespace djmeta_foobar {

djmeta::MetadataDocument metadata_from_file_info(const file_info& info) {
    djmeta::MetadataDocument document;
    const t_size field_count = info.meta_get_count();
    document.fields.reserve(static_cast<std::size_t>(field_count));

    for (t_size field_index = 0; field_index < field_count; ++field_index) {
        const char* name = info.meta_enum_name(field_index);
        if (!name || !*name) continue;

        djmeta::MetadataField field;
        field.name = name;

        const t_size value_count = info.meta_enum_value_count(field_index);
        field.values.reserve(static_cast<std::size_t>(value_count));
        for (t_size value_index = 0; value_index < value_count; ++value_index) {
            const char* value = info.meta_enum_value(field_index, value_index);
            field.values.emplace_back(value ? value : "");
        }
        document.fields.push_back(std::move(field));
    }
    return document;
}

} // namespace djmeta_foobar
