#pragma once

#include "djmeta/batch_preview.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace djmeta {

inline constexpr int kBatchPreviewColumnCount = 4;

struct BatchTableLayout {
    // Logical column IDs are stable even after drag/drop rearrangement.
    std::array<int, kBatchPreviewColumnCount> order{0, 1, 2, 3};
    // Stored at 96-DPI logical units. Width is remembered when hidden.
    std::array<int, kBatchPreviewColumnCount> widths{220, 130, 360, 180};
    unsigned visible_mask = 15u;
    int sort_column = 0;
    bool sort_descending = false;
};

BatchTableLayout default_batch_table_layout();

// Strict versioned roundtrip; unknown or malformed settings fail closed to
// default without accepting invalid column permutations / hiding everything.
std::string serialize_batch_table_layout(const BatchTableLayout& layout);
BatchTableLayout parse_batch_table_layout(std::string_view serialized);
bool valid_batch_table_layout(const BatchTableLayout& layout);

// Stable deterministic view mapping. Input items are never reordered;
// selected row IDs are mapped through this permutation in the foobar UI.
std::vector<std::size_t> sort_batch_table_view(
    const std::vector<BatchPreviewInputRow>& rows,
    const BatchPreviewTable& review,
    int sort_column,
    bool descending);

} // namespace djmeta
