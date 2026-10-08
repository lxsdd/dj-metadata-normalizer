#include "djmeta/batch_preview.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

void check(bool ok, const char* message) {
    if (!ok) { std::cerr << "FAIL: " << message << "\n"; std::exit(1); }
}
djmeta::BatchPreviewInputRow sample(const std::string& path) {
    djmeta::BatchPreviewInputRow row;
    row.source_path = path;
    row.physical_id = path;
    row.profile = "Singles";
    row.destination_root = "Z:\\Music\\Singles";
    row.raw_relative_path = "Test/Artist - Track";
    row.physical_source_qualified = true;
    return row;
}
bool contains(const djmeta::BatchPreviewRow& row, const std::string& text) {
    for (const auto& issue : row.issues) if (issue == text) return true;
    return false;
}

int main() {
    using djmeta::raw_relative_path_lexically_safe;
    for (const std::string& valid : {
             std::string("Album/Artist - Track.mp3"),
             std::string("A\\B\\C.flac"),
             std::string("Bj\xC3\xB6rk.mp3"),
             std::string(".profile")})
        check(raw_relative_path_lexically_safe(valid),
              "legitimate relative path must pass lexical checks");
    for (const std::string& dangerous : {
             std::string("../escape.mp3"), std::string("A/../B.mp3"),
             std::string("./X.mp3"), std::string("/absolute.mp3"),
             std::string("C:\\\\absolute.mp3"), std::string("a//b.mp3"),
             std::string("a/"), std::string("foo:bar.mp3"),
             std::string("NUL.mp3"), std::string("con"),
             std::string("A/LpT9.txt"), std::string("x. "),
             std::string("x."), std::string("name?.mp3"),
             std::string("name|x"), std::string("line\nfeed.mp3"),
             std::string("nul\0byte.mp3", 12), std::string("\xC3\x28", 2)})
        check(!raw_relative_path_lexically_safe(dangerous),
              "unsafe Title Formatting output must not resemble an approved target");

    auto a = sample("a.mp3"), b = sample("b.mp3");
    const std::vector<djmeta::BatchPreviewInputRow> batch = {a,b};
    const auto c = djmeta::describe_batch_preview(batch);
    check(c.rows.size() == 2 && c.raw_collision_items == 2,
          "duplicate raw targets were not reported for both rows");
    check(contains(c.rows[0], "DUPLICATE_RAW_TARGET"),
          "duplicate target issue missing");
    check(contains(c.rows[1], "CUE_DEPENDENCIES_UNCHECKED") &&
          contains(c.rows[1], "FILESYSTEM_TARGET_UNCHECKED"),
          "unverified host/cue gates must remain visible");
    check(c.unqualified_rows == 2, "unqualified rows must be counted");

    b.raw_relative_path = "Other/Artist - Track";
    b.semantic_proposals_pending = true;
    const auto differing = djmeta::describe_batch_preview({a,b});
    check(differing.raw_collision_items == 0 &&
          contains(differing.rows[1], "UNAPPROVED_METADATA_PROPOSALS"),
          "semantic REVIEW must be visible on a unique raw target");

    a.physical_source_qualified = false;
    a.raw_relative_path.clear();
    const auto invalid = djmeta::describe_batch_preview({a});
    check(contains(invalid.rows[0], "PHYSICAL_SOURCE_UNQUALIFIED") &&
          contains(invalid.rows[0], "TARGET_EXPRESSION_EMPTY"),
          "virtual/duplicate physical audio must not look ready");

    a = sample("a.mp3");
    a.raw_relative_path = "Album/../target.mp3";
    const auto unsafe = djmeta::describe_batch_preview({a});
    check(contains(unsafe.rows[0], "UNSAFE_RAW_RELATIVE_TARGET"),
          "unsafe raw paths must appear in batch preview diagnostics");
    a = sample("a.mp3");
    a.cue_dependencies_checked = true;
    a.filesystem_target_checked = true;
    const auto qualified = djmeta::describe_batch_preview({a});
    check(qualified.rows[0].issues.empty(), "fully qualified hints should clear");
    check(batch[0].physical_id == "a.mp3" && batch[1].physical_id == "b.mp3",
          "preview generation must not mutate original input");
    std::cout << "PASS: read-only batch preview statuses and raw collisions\n";
}
