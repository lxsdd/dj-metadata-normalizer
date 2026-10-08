#include "djmeta/batch_preview.h"

#include <map>
#include <string>
#include <vector>

namespace djmeta {
BatchPreviewTable describe_batch_preview(
    const std::vector<BatchPreviewInputRow>& items) {
    BatchPreviewTable out;
    out.rows.resize(items.size());
    std::map<std::string, std::size_t> counts;
    // Deliberately raw, case-sensitive counts only. Windows canonicalization
    // remains a responsibility of the future foobar host filesystem planner.
    for (const auto& item : items) {
        if (item.physical_source_qualified &&
            !item.destination_root.empty() && !item.raw_relative_path.empty()) {
            ++counts[item.destination_root + "\\" + item.raw_relative_path];
        }
    }
    for (std::size_t i = 0; i < items.size(); ++i) {
        const auto& item = items[i];
        auto& row = out.rows[i];
        if (!item.destination_root.empty() && !item.raw_relative_path.empty())
            row.raw_destination =
                item.destination_root + "\\" + item.raw_relative_path;
        if (!item.physical_source_qualified)
            row.issues.emplace_back("PHYSICAL_SOURCE_UNQUALIFIED");
        if (row.raw_destination.empty())
            row.issues.emplace_back("TARGET_EXPRESSION_EMPTY");
        if (!row.raw_destination.empty() && counts[row.raw_destination] > 1) {
            row.has_raw_target_collision = true;
            row.issues.emplace_back("DUPLICATE_RAW_TARGET");
            ++out.raw_collision_items;
        }
        if (item.semantic_proposals_pending)
            row.issues.emplace_back("UNAPPROVED_METADATA_PROPOSALS");
        if (!item.cue_dependencies_checked)
            row.issues.emplace_back("CUE_DEPENDENCIES_UNCHECKED");
        if (!item.filesystem_target_checked)
            row.issues.emplace_back("FILESYSTEM_TARGET_UNCHECKED");
        if (!row.issues.empty())
            ++out.unqualified_rows;
    }
    return out;
}

} // namespace djmeta
