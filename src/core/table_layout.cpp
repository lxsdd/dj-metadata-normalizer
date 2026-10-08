#include "djmeta/table_layout.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

namespace djmeta {
namespace {

bool read_int(std::string_view value, int& output) {
    if (value.empty()) return false;
    const auto [p, error] = std::from_chars(value.data(), value.data() + value.size(), output);
    return error == std::errc{} && p == value.data() + value.size();
}

std::vector<std::string_view> split(std::string_view text, char delimiter) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    for (;;) {
        const auto end = text.find(delimiter, start);
        parts.emplace_back(text.substr(start,
            end == std::string_view::npos ? end : end - start));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return parts;
}

bool parse_four(std::string_view text,
                std::array<int, kBatchPreviewColumnCount>& values) {
    const auto parts = split(text, ',');
    if (parts.size() != values.size()) return false;
    for (std::size_t i = 0; i < values.size(); ++i)
        if (!read_int(parts[i], values[i])) return false;
    return true;
}

int compare_sort_keys(std::string_view a, std::string_view b) {
    // ASCII case-fold only. Non-ASCII UTF-8 bytes stay intact. Windows locale
    // collation may later be used by the foobar adapter without changing
    // which logical row each virtual ListView index identifies.
    const std::size_t n = (std::min)(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) {
        unsigned char left = static_cast<unsigned char>(a[i]);
        unsigned char right = static_cast<unsigned char>(b[i]);
        if (left >= 'A' && left <= 'Z') left += 'a' - 'A';
        if (right >= 'A' && right <= 'Z') right += 'a' - 'A';
        if (left < right) return -1;
        if (left > right) return 1;
    }
    if (a.size() < b.size()) return -1;
    if (a.size() > b.size()) return 1;
    return 0;
}

std::string_view first_issue(const BatchPreviewRow& row) {
    static constexpr std::string_view kClear = "NO_ISSUES";
    if (row.issues.empty()) return kClear;
    return row.issues.front();
}

std::string_view sort_key(
    const BatchPreviewInputRow& row, const BatchPreviewRow& result, int col) {
    switch (col) {
    case 0: return row.source_path;
    case 1: return row.profile;
    case 2: return result.raw_destination;
    case 3: return first_issue(result);
    default: return row.source_path;
    }
}

} // namespace

BatchTableLayout default_batch_table_layout() { return {}; }

bool valid_batch_table_layout(const BatchTableLayout& layout) {
    if (layout.visible_mask == 0 || layout.visible_mask > 15u ||
        layout.sort_column < -1 || layout.sort_column >= kBatchPreviewColumnCount)
        return false;
    std::array<bool, kBatchPreviewColumnCount> seen{};
    for (int i = 0; i < kBatchPreviewColumnCount; ++i) {
        const int column = layout.order[static_cast<std::size_t>(i)];
        if (column < 0 || column >= kBatchPreviewColumnCount ||
            seen[static_cast<std::size_t>(column)])
            return false;
        seen[static_cast<std::size_t>(column)] = true;
        if (layout.widths[static_cast<std::size_t>(i)] < 48 ||
            layout.widths[static_cast<std::size_t>(i)] > 3000)
            return false;
    }
    return true;
}

std::string serialize_batch_table_layout(const BatchTableLayout& layout) {
    if (!valid_batch_table_layout(layout)) return {};
    std::string output = "v1|";
    for (std::size_t i = 0; i < layout.order.size(); ++i) {
        if (i) output += ',';
        output += std::to_string(layout.order[i]);
    }
    output += '|';
    for (std::size_t i = 0; i < layout.widths.size(); ++i) {
        if (i) output += ',';
        output += std::to_string(layout.widths[i]);
    }
    output += '|';
    output += std::to_string(layout.visible_mask);
    output += '|';
    output += std::to_string(layout.sort_column);
    output += '|';
    output += layout.sort_descending ? '1' : '0';
    return output;
}

BatchTableLayout parse_batch_table_layout(std::string_view serialized) {
    const auto parts = split(serialized, '|');
    BatchTableLayout parsed{};
    if (parts.size() != 6 || parts[0] != "v1" ||
        !parse_four(parts[1], parsed.order) ||
        !parse_four(parts[2], parsed.widths))
        return default_batch_table_layout();
    int mask = 0, descending = 0;
    if (!read_int(parts[3], mask) ||
        !read_int(parts[4], parsed.sort_column) ||
        !read_int(parts[5], descending) ||
        mask < 0 || mask > 15 ||
        (descending != 0 && descending != 1))
        return default_batch_table_layout();
    parsed.visible_mask = static_cast<unsigned>(mask);
    parsed.sort_descending = descending != 0;
    return valid_batch_table_layout(parsed)
        ? parsed : default_batch_table_layout();
}

std::vector<std::size_t> sort_batch_table_view(
    const std::vector<BatchPreviewInputRow>& rows,
    const BatchPreviewTable& review,
    int sort_column, bool descending) {

    std::vector<std::size_t> mapping(rows.size());
    std::iota(mapping.begin(), mapping.end(), std::size_t{0});
    if (sort_column < 0 || sort_column >= kBatchPreviewColumnCount ||
        review.rows.size() != rows.size())
        return mapping;

    std::stable_sort(mapping.begin(), mapping.end(),
        [&](std::size_t a, std::size_t b) {
            const int cmp = compare_sort_keys(
                sort_key(rows[a], review.rows[a], sort_column),
                sort_key(rows[b], review.rows[b], sort_column));
            return descending ? cmp > 0 : cmp < 0;
        });
    return mapping;
}

} // namespace djmeta
