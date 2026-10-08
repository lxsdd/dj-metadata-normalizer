#include "stdafx.h"

#include "batch_table_settings.h"

#include <SDK/cfg_var.h>

namespace djmeta_foobar {
namespace {

constexpr GUID kBatchTableLayoutGuid =
    {0xbca40f8b, 0x927b, 0x4fe6, {0xa1, 0x75, 0x16, 0x09, 0x63, 0xd1, 0x47, 0x21}};

cfg_string g_batch_table_layout(
    kBatchTableLayoutGuid, "v1|0,1,2,3|220,130,360,180|15|0|0");

} // namespace

djmeta::BatchTableLayout load_batch_table_layout() {
    const auto persisted = g_batch_table_layout.get();
    return djmeta::parse_batch_table_layout(persisted.c_str());
}

void store_batch_table_layout(const djmeta::BatchTableLayout& layout) {
    const auto encoded = djmeta::serialize_batch_table_layout(layout);
    if (encoded.empty())
        throw std::invalid_argument("Invalid preview table layout.");
    g_batch_table_layout.set(encoded.c_str());
}

} // namespace djmeta_foobar
