#include "djmeta/physical_selection.h"

#include <map>
#include <string>
#include <vector>

namespace djmeta {

std::vector<PhysicalSelectionResult> qualify_physical_selection(
    const std::vector<PhysicalSelectionEvidence>& selected) {

    std::map<std::string, std::size_t> physical_counts;
    for (const auto& item : selected) {
        // Count every actually observed physical identity. Do not discard a
        // known alias merely because another independent check rejected it.
        if (!item.host_physical_key.empty())
            ++physical_counts[item.host_physical_key];
    }

    std::vector<PhysicalSelectionResult> result;
    result.reserve(selected.size());
    for (const auto& item : selected) {
        PhysicalSelectionResult check;
        if (!item.physical_candidate)
            check.reason = "VIRTUAL_OR_DUPLICATED_HOST_SELECTION";
        else if (item.host_physical_key.empty() || item.source_guard.empty())
            check.reason = "HOST_SOURCE_NOT_INSPECTED";
        else if (physical_counts[item.host_physical_key] != 1)
            check.reason = "DUPLICATE_HOST_PHYSICAL_ID";
        else
            check.qualified = true;
        result.push_back(std::move(check));
    }
    return result;
}

} // namespace djmeta
