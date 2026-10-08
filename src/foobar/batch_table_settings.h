#pragma once

#include "djmeta/table_layout.h"
#include "djmeta/review_grid_layout.h"

namespace djmeta_foobar {

// foobar-native profile configuration, not external JSON/registry.
djmeta::BatchTableLayout load_batch_table_layout();
void store_batch_table_layout(const djmeta::BatchTableLayout& layout);

// Separate foobar cfg variables; file-grid layouts remain backward compatible.
djmeta::ReviewGridLayout<4> default_track_grid_layout();
djmeta::ReviewGridLayout<5> default_detail_grid_layout();
djmeta::ReviewGridLayout<4> load_track_grid_layout();
djmeta::ReviewGridLayout<5> load_detail_grid_layout();
void store_track_grid_layout(const djmeta::ReviewGridLayout<4>& layout);
void store_detail_grid_layout(const djmeta::ReviewGridLayout<5>& layout);

} // namespace djmeta_foobar
