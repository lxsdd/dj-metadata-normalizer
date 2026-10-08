#include "djmeta/physical_selection.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
void require(bool okay, const char* why) {
    if (!okay) { std::cerr << "FAIL: " << why << "\n"; std::exit(1); }
}
}

int main() {
    using djmeta::PhysicalSelectionEvidence;
    using djmeta::qualify_physical_selection;

    const auto clean = qualify_physical_selection({
        {true, "volume1:fileA", "versionA"},
        {true, "volume1:fileB", "versionB"},
        {true, "volume2:fileA", "versionC"},
    });
    require(clean.size() == 3 && clean[0].qualified &&
            clean[1].qualified && clean[2].qualified,
            "independent files across volumes are qualified");

    // Two different source paths that refer to one file must both be blocked.
    const auto aliased = qualify_physical_selection({
        {true, "volume1:fileA", "versionA"},
        {true, "volume1:fileA", "versionA"},
        {true, "volume1:fileB", "versionB"},
    });
    require(!aliased[0].qualified && !aliased[1].qualified &&
            aliased[0].reason == "DUPLICATE_HOST_PHYSICAL_ID" &&
            aliased[1].reason == "DUPLICATE_HOST_PHYSICAL_ID" &&
            aliased[2].qualified, "hardlink aliases cannot become two operations");

    const auto incomplete = qualify_physical_selection({
        {true, "", ""},
        {true, "volume1:fileC", ""},
        {false, "volume1:fileD", "snapshotD"},
        {true, "volume1:fileE", "snapshotE"},
    });
    require(incomplete[0].reason == "HOST_SOURCE_NOT_INSPECTED" &&
            incomplete[1].reason == "HOST_SOURCE_NOT_INSPECTED" &&
            incomplete[2].reason == "VIRTUAL_OR_DUPLICATED_HOST_SELECTION" &&
            incomplete[3].qualified, "unknown physical evidence fails closed");

    const auto virtual_alias = qualify_physical_selection({
        {false, "volume1:fileA", "versionA"},
        {true, "volume1:fileA", "versionA"},
    });
    require(!virtual_alias[0].qualified && !virtual_alias[1].qualified &&
            virtual_alias[1].reason == "DUPLICATE_HOST_PHYSICAL_ID",
            "a known virtual alias still disqualifies its physical twin");

    require(qualify_physical_selection({}).empty(), "empty selection is safe");
    std::cout << "PASS: read-only physical-selection qualification\n";
}
