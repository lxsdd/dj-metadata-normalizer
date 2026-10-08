#include "djmeta/external_cue.h"

#include <cstdlib>
#include <iostream>
#include <string>

using djmeta::CueSyntaxStatus;
using djmeta::CueTextEncoding;
using djmeta::inspect_external_cue;

void check(bool ok) { if (!ok) { std::cerr << "FAIL: external CUE test assertion\n"; std::exit(1); } }

void basic_crlf() {
    const std::string data = "REM COMMENT \"FILE ignored.wav WAVE\"\r\n"
                             "FILE \"Artist - Album.flac\" WAVE\r\n"
                             "  TRACK 01 AUDIO\r\n";
    const auto r=inspect_external_cue(data);
    check(r.status == CueSyntaxStatus::Parsed && r.references.size() == 1);
    check(r.references[0].line_number == 2);
    check(r.references[0].filename == "Artist - Album.flac");
    check(data.substr(r.references[0].filename_begin,
                       r.references[0].filename_end-r.references[0].filename_begin) ==
           r.references[0].filename);
    check(r.references[0].file_type == "WAVE" && r.references[0].quoted);
}
void multi_file_and_lf() {
    const std::string data = "file \"disc 1/track 1.flac\" WAVE\n"
                             "FILE track2.mp3 MP3\n";
    const auto r=inspect_external_cue(data);
    check(r.status == CueSyntaxStatus::Parsed && r.multiple_files);
    check(r.references.size() == 2 && r.references[1].filename == "track2.mp3");
}
void unicode_bom_and_offsets() {
    const std::string data = std::string("\xef\xbb\xbf") +
                             "FILE \"Beyonc\xc3\xa9.flac\" WAVE\n";
    const auto r=inspect_external_cue(data);
    check(r.status == CueSyntaxStatus::Parsed && r.encoding == CueTextEncoding::Utf8Bom);
    check(r.references[0].filename == "Beyonc\xc3\xa9.flac");
    check(data.substr(r.references[0].filename_begin,
                       r.references[0].filename_end-r.references[0].filename_begin) ==
           r.references[0].filename);
}
void unsafe_and_unsupported() {
    check(inspect_external_cue("FILE \"broken.flac WAVE\n").status == CueSyntaxStatus::Invalid);
    check(inspect_external_cue("FILE \"track.flac\"\n").status == CueSyntaxStatus::Invalid);
    check(inspect_external_cue("FILE \"track.flac\" WAVE extra\n").status == CueSyntaxStatus::Invalid);
    check(inspect_external_cue("TRACK 01 AUDIO\n").status == CueSyntaxStatus::Invalid);
    check(inspect_external_cue(R"(FILE "C:\music\t.flac" WAVE)" "\n").status == CueSyntaxStatus::NeedsReview);
    check(inspect_external_cue("FILE \"x.flac\" CUSTOM\n").status == CueSyntaxStatus::NeedsReview);
    const auto utf16 = inspect_external_cue(std::string("\xff\xfe", 2) + std::string("F\0I\0", 4));
    check(utf16.status == CueSyntaxStatus::NeedsReview &&
           utf16.encoding == CueTextEncoding::UnsupportedUtf16);
    const auto invalid = inspect_external_cue(std::string("FILE \"",6) + std::string("\xc0\xaf", 2) + ".flac\" WAVE\n");
    check(invalid.status == CueSyntaxStatus::NeedsReview && invalid.references.empty());
    const auto nul = inspect_external_cue(std::string("FILE \"",6) + std::string(1,'\0') + "x\" WAVE\n");
    check(nul.status == CueSyntaxStatus::NeedsReview);
}
void rewrite_preview_preserves_unrelated_bytes() {
    const std::string input = "REM GENRE House\r\n"
        "FILE \"old.flac\" WAVE\r\n"
        "  TRACK 01 AUDIO\r\n"
        "  INDEX 01 00:00:00\r\n";
    const auto out = djmeta::preview_external_cue_reference_rewrite(
        input, {{0, "old.flac", "Artist - Album.flac"}});
    check(out.eligible && out.changed);
    check(out.proposed_bytes == "REM GENRE House\r\n"
        "FILE \"Artist - Album.flac\" WAVE\r\n"
        "  TRACK 01 AUDIO\r\n"
        "  INDEX 01 00:00:00\r\n");
    check(input.find("old.flac") != std::string::npos);
}
void rewrite_preview_multifile_and_quotes() {
    const std::string input = "FILE old1.flac WAVE\nFILE old2.flac WAVE\n";
    const auto out = djmeta::preview_external_cue_reference_rewrite(
        input, {{1, "old2.flac", "New Title.flac"}});
    check(out.eligible && out.changed);
    check(out.proposed_bytes == "FILE old1.flac WAVE\nFILE \"New Title.flac\" WAVE\n");

    const auto unchanged = djmeta::preview_external_cue_reference_rewrite(input, {});
    check(unchanged.eligible && !unchanged.changed && unchanged.proposed_bytes == input);
}
void rewrite_preview_rejects_stale_or_unsafe() {
    const std::string input = "FILE \"old.flac\" WAVE\n";
    check(!djmeta::preview_external_cue_reference_rewrite(
        input, {{0, "different.flac", "new.flac"}}).eligible);
    check(!djmeta::preview_external_cue_reference_rewrite(
        input, {{5, "old.flac", "new.flac"}}).eligible);
    check(!djmeta::preview_external_cue_reference_rewrite(
        input, {{0, "old.flac", "new.flac"}, {0, "old.flac", "x.flac"}}).eligible);
    check(!djmeta::preview_external_cue_reference_rewrite(
        input, {{0, "old.flac", "unsafe\nFILE \"evil.flac\" WAVE"}}).eligible);
    check(!djmeta::preview_external_cue_reference_rewrite(
        input, {{0, "old.flac", "quote\"name.flac"}}).eligible);
    check(!djmeta::preview_external_cue_reference_rewrite(
        "FILE \"C:\\old.flac\" WAVE\n", {{0, "C:\\old.flac", "safe.flac"}}).eligible);
}
int main() {
    basic_crlf(); multi_file_and_lf(); unicode_bom_and_offsets(); unsafe_and_unsupported();
    rewrite_preview_preserves_unrelated_bytes(); rewrite_preview_multifile_and_quotes();
    rewrite_preview_rejects_stale_or_unsafe();
    std::cout << "PASS: external CUE inventory (read-only) 7 suites\n";
}
