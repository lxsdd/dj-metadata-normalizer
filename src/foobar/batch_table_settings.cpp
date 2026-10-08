#include "stdafx.h"

#include "batch_table_settings.h"

#include <SDK/cfg_var.h>

namespace djmeta_foobar {
namespace {

constexpr GUID kBatchTableLayoutGuid =
    {0xbca40f8b, 0x927b, 0x4fe6, {0xa1, 0x75, 0x16, 0x09, 0x63, 0xd1, 0x47, 0x21}};

cfg_string g_batch_table_layout(
    kBatchTableLayoutGuid, "v1|0,1,2,3|220,130,360,180|15|0|0");

// Stable, independent foobar-native settings for review master and detail.
// Never reuse the existing file-grid GUID or change its stored schema.
constexpr GUID kTrackGridGuid =
    {0x912ddaa2, 0x8203, 0x4c92, {0x96, 0xc0, 0xd7, 0xac, 0x83, 0x92, 0x14, 0x11}};
constexpr GUID kDetailGridGuid =
    {0x493e92a1, 0x612e, 0x4bdf, {0xae, 0x50, 0x7e, 0x28, 0x18, 0x85, 0x4f, 0x42}};

cfg_string g_track_grid_layout(kTrackGridGuid,
    "v1|0,1,2,3|150,48,48,48|15|0|0");
cfg_string g_detail_grid_layout(kDetailGridGuid,
    "v1|0,1,2,3,4|80,105,105,60,75|31|0|0");

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

djmeta::ReviewGridLayout<4> default_track_grid_layout() {
    return djmeta::default_review_grid_layout<4>({150, 48, 48, 48});
}
djmeta::ReviewGridLayout<5> default_detail_grid_layout() {
    return djmeta::default_review_grid_layout<5>({80, 105, 105, 60, 75});
}
djmeta::ReviewGridLayout<4> load_track_grid_layout() {
    return djmeta::parse_review_grid_layout(
        std::string_view(g_track_grid_layout.get().c_str()), default_track_grid_layout());
}
djmeta::ReviewGridLayout<5> load_detail_grid_layout() {
    return djmeta::parse_review_grid_layout(
        std::string_view(g_detail_grid_layout.get().c_str()), default_detail_grid_layout());
}
void store_track_grid_layout(const djmeta::ReviewGridLayout<4>& layout) {
    const auto text = djmeta::serialize_review_grid_layout(layout);
    if (text.empty()) throw std::invalid_argument("Invalid track grid layout.");
    g_track_grid_layout.set(text.c_str());
}
void store_detail_grid_layout(const djmeta::ReviewGridLayout<5>& layout) {
    const auto text = djmeta::serialize_review_grid_layout(layout);
    if (text.empty()) throw std::invalid_argument("Invalid detail grid layout.");
    g_detail_grid_layout.set(text.c_str());
}

} // namespace djmeta_foobar
