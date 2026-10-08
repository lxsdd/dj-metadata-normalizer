#include "djmeta/table_layout.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace {
void expect(bool check, const char* what) {
    if (!check) { std::cerr << "FAIL: " << what << "\n"; std::exit(1); }
}

djmeta::BatchPreviewInputRow row(std::string name, std::string profile, std::string dest) {
    djmeta::BatchPreviewInputRow r;
    r.source_path = std::move(name);
    r.profile = std::move(profile);
    r.destination_root = "Z:/Music";
    r.raw_relative_path = std::move(dest);
    r.physical_source_qualified = true;
    return r;
}
void test_layout_roundtrip_and_invalid() {
    auto layout = djmeta::default_batch_table_layout();
    layout.order = {3, 0, 2, 1};
    layout.widths = {177, 120, 520, 88};
    layout.visible_mask = 0b0101;
    layout.sort_column = 2;
    layout.sort_descending = true;
    const auto encoded = djmeta::serialize_batch_table_layout(layout);
    expect(!encoded.empty(), "valid layout must serialize");
    auto decoded = djmeta::parse_batch_table_layout(encoded);
    expect(decoded.order == layout.order && decoded.widths == layout.widths &&
           decoded.visible_mask == layout.visible_mask &&
           decoded.sort_column == layout.sort_column &&
           decoded.sort_descending == layout.sort_descending,
           "layout must roundtrip exactly");
    auto bad = layout;
    bad.order = {1, 1, 2, 3};
    expect(!djmeta::valid_batch_table_layout(bad) &&
           djmeta::serialize_batch_table_layout(bad).empty(),
           "duplicate column IDs cannot be saved");
    bad = layout;
    bad.visible_mask = 0;
    expect(!djmeta::valid_batch_table_layout(bad), "cannot hide every column");
    bad = layout;
    bad.widths[0] = 4000;
    expect(!djmeta::valid_batch_table_layout(bad), "oversized width invalid");
    for (const std::string invalid : {
        "", "v2|0,1,2,3|220,130,360,180|15|0|0",
        "v1|0,1,1,3|220,130,360,180|15|0|0",
        "v1|0,1,2,3|220,130,360,180|0|0|0",
        "v1|0,1,2,3|220,130,360,180|15|4|0",
        "v1|0,1,2,3|220,130,360,180|15|0|2",
        "v1|0,1,2,3|220,130,360,180|15|0|0|tail",
        "v1|0,1,2,3|220,130,360,180|15|0|-1",
        "v1|0,1,2,x|220,130,360,180|15|0|0",
        "v1|0,1,2,3|220,130,-2,180|15|0|0",
    }) {
        const auto fallback = djmeta::parse_batch_table_layout(invalid);
        expect(fallback.order == djmeta::default_batch_table_layout().order &&
               fallback.visible_mask == 15 && fallback.sort_column == 0,
               "malformed settings must load safe defaults");
    }
}
void test_sort_stable_and_inputs_immutable() {
    const std::vector<djmeta::BatchPreviewInputRow> rows = {
        row("z.flac", "Singles", "Z"),
        row("B.flac", "Albums", "X"),
        row("a.flac", "Singles", "Y"),
        row("A.flac", "Albums", "X"),
    };
    const auto review = djmeta::describe_batch_preview(rows);
    const auto asc = djmeta::sort_batch_table_view(rows, review, 0, false);
    expect((asc == std::vector<std::size_t>{2,3,1,0}),
           "ascending ASCII insensitive sort must be stable");
    const auto desc = djmeta::sort_batch_table_view(rows, review, 0, true);
    expect((desc == std::vector<std::size_t>{0,1,2,3}),
           "descending selection must preserve equal-key original order");
    const auto profile = djmeta::sort_batch_table_view(rows, review, 1, false);
    expect((profile == std::vector<std::size_t>{1,3,0,2}),
           "sort by logical profile, not visual column position");
    const auto target = djmeta::sort_batch_table_view(rows, review, 2, false);
    expect((target == std::vector<std::size_t>{1,3,2,0}),
           "raw target sort with stable ties");
    const auto off = djmeta::sort_batch_table_view(rows, review, -1, false);
    expect((off == std::vector<std::size_t>{0,1,2,3}), "explicit natural order");
    expect(rows[0].source_path == "z.flac" && rows[1].source_path == "B.flac",
           "sort must not mutate data or index identities");
}
void test_large_virtual_mapping() {
    std::vector<djmeta::BatchPreviewInputRow> rows;
    for (int i=0;i<20000;i++)
        rows.push_back(row("file" + std::to_string(i), "Singles",
                           "file" + std::to_string(i)));
    const auto result = djmeta::describe_batch_preview(rows);
    const auto view = djmeta::sort_batch_table_view(rows,result,0,false);
    expect(view.size()==rows.size(), "virtual sort lost rows");
    auto unique = view;
    std::sort(unique.begin(),unique.end());
    for (std::size_t i=0;i<unique.size();++i)
        expect(unique[i] == i, "row mapping must remain a permutation");
    // User selections track underlying row IDs rather than sorted positions.
    const std::size_t original_selected = 19900;
    const auto pos = std::find(view.begin(),view.end(),original_selected);
    expect(pos != view.end() && rows[*pos].source_path=="file19900",
           "selection identity lost through view sorting");
}
}

int main() {
    test_layout_roundtrip_and_invalid();
    test_sort_stable_and_inputs_immutable();
    test_large_virtual_mapping();
    std::cout << "PASS: layout roundtrip, strict validation, stable virtual sort of 20,000 rows\n";
}
