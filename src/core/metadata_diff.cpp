#include "djmeta/metadata_diff.h"

#include <algorithm>
#include <numeric>
#include <string_view>
#include <vector>

namespace djmeta {
namespace {
std::string join(const std::vector<std::string>& values) {
    std::string result;
    for (const auto& value : values) {
        if (!result.empty()) result += ", ";
        result += value;
    }
    return result;
}
int cmp_folded(std::string_view a, std::string_view b) {
    const auto len = (std::min)(a.size(),b.size());
    for (std::size_t i=0; i<len; ++i) {
        unsigned char x = static_cast<unsigned char>(a[i]);
        unsigned char y = static_cast<unsigned char>(b[i]);
        if (x >= 'A' && x <= 'Z') x += ('a'-'A');
        if (y >= 'A' && y <= 'Z') y += ('a'-'A');
        if (x != y) return x < y ? -1 : 1;
    }
    return a.size() == b.size() ? 0 : a.size() < b.size() ? -1 : 1;
}
std::string_view key(const MetadataDiffRow& row,
                     const std::vector<std::string>& names, int column) {
    switch(column) {
        case 0: return row.source_index < names.size() ? 
                   std::string_view(names[row.source_index]) : std::string_view();
        case 1: return row.field;
        case 2: return row.original;
        case 3: return row.proposed;
        case 4: return to_string(row.safety);
        case 5: return row.rule_ids;
        default: return {};
    }
}
} // namespace

std::vector<MetadataDiffRow> describe_metadata_diffs(
    const std::vector<AnalysisResult>& analyses) {
    std::vector<MetadataDiffRow> rows;
    for (std::size_t i=0; i<analyses.size(); ++i) {
        for (std::size_t j = 0; j < analyses[i].proposals.size(); ++j) {
            const Proposal& proposal = analyses[i].proposals[j];
            if (proposal.original_value == proposal.proposed_value) continue;
            rows.push_back({
                i, proposal.field_index, proposal.value_index,
                proposal.field, proposal.original_value, proposal.proposed_value,
                proposal.safety, join(proposal.rule_ids), join(proposal.rationales), j
            });
        }
    }
    return rows;
}
std::vector<std::size_t> sort_metadata_diff_rows(
    const std::vector<MetadataDiffRow>& rows,
    const std::vector<std::string>& names,
    int sort_column, bool descending) {
    std::vector<std::size_t> result(rows.size());
    std::iota(result.begin(), result.end(), std::size_t{0});
    if (sort_column < 0 || sort_column > 5) return result;
    std::stable_sort(result.begin(), result.end(), [&](std::size_t a, std::size_t b) {
        const int c = cmp_folded(key(rows[a],names,sort_column),
                                 key(rows[b],names,sort_column));
        return descending ? c > 0 : c < 0;
    });
    return result;
}

} // namespace djmeta
