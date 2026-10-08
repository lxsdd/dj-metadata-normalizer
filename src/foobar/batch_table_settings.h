#pragma once

#include "djmeta/table_layout.h"

namespace djmeta_foobar {

// foobar-native profile configuration, not external JSON/registry.
djmeta::BatchTableLayout load_batch_table_layout();
void store_batch_table_layout(const djmeta::BatchTableLayout& layout);

} // namespace djmeta_foobar
