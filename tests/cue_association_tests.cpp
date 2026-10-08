#include "djmeta/cue_association.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace djmeta;

void check(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

void test_single() {
    const std::string cue = "REM GENRE House\r\nFILE \"old.mp3\" MP3\r\nTRACK 01 AUDIO\r\n";
    const auto plan = qualify_external_cue_association(cue,
        {{0,"old.mp3","id://old","Artist - Mix.mp3",true,true}},
        {{"phys-A","id://old","id://target"}});
    check(plan.qualified && plan.changes_references, "single association should qualify");
    check(plan.changes.size()==1 && plan.related_selected_audio_ids.size()==1 &&
          plan.related_selected_audio_ids[0]=="phys-A", "single association details");
    check(plan.proposed_cue_bytes == "REM GENRE House\r\nFILE \"Artist - Mix.mp3\" MP3\r\nTRACK 01 AUDIO\r\n", "preserved unrelated cue bytes");
    check(plan.original_cue_fingerprint != plan.proposed_cue_fingerprint, "cue postimage hash");
}
void test_multifile_unselected() {
    const std::string cue = "FILE \"a.flac\" WAVE\nFILE \"b.flac\" WAVE\n";
    const auto plan = qualify_external_cue_association(cue,
       {{0,"a.flac","src-A","a-new.flac",true,true},
        {1,"b.flac","src-unselected","b.flac",true,true}},
       {{"A","src-A","dst-A"}});
    check(plan.qualified && plan.changes.size()==1, "multi-file mixed selection");
    check(plan.proposed_cue_bytes=="FILE \"a-new.flac\" WAVE\nFILE \"b.flac\" WAVE\n", "nonselected FILE unchanged");
    const auto rejected = qualify_external_cue_association(cue,
       {{0,"a.flac","src-A","a-new.flac",true,true},
        {1,"b.flac","src-unselected","b.flac",true,false}},
       {{"A","src-A","dst-A"}});
    check(!rejected.qualified, "unselected reference must be verified after cue move");
}
void test_missing_and_ambiguous() {
    const std::string cue="FILE \"old.flac\" WAVE\n";
    check(!qualify_external_cue_association(cue, {}, {}).qualified, "missing host coverage");
    check(!qualify_external_cue_association(cue, {{0,"old.flac","src","next.flac",false,true}}, {}).qualified, "ambiguous host path");
    check(!qualify_external_cue_association(cue, {{0,"old.flac","src","next.flac",true,false}}, {}).qualified, "unchecked host target");
    check(!qualify_external_cue_association(cue, {{0,"wrong.flac","src","next.flac",true,true}}, {}).qualified, "stale expected file name");
    check(!qualify_external_cue_association(cue, {{1,"old.flac","src","next.flac",true,true}}, {}).qualified, "reference index out of range");
    check(!qualify_external_cue_association(cue, {{0,"old.flac","src","next.flac",true,true}}, {{"A","src","to"},{"B","src","to2"}}).qualified, "two selected physical source identities");
    check(!qualify_external_cue_association(cue, {{0,"old.flac","src","next.flac",true,true}}, {{"A","src","to"},{"A","other","to2"}}).qualified, "two selected physical IDs");
}
void test_exact_nochange_and_unsafe_postimage() {
    const std::string cue="FILE old.flac WAVE\n";
    const auto same=qualify_external_cue_association(cue, {{0,"old.flac","old","old.flac",true,true}}, {});
    check(same.qualified && !same.changes_references && same.proposed_cue_bytes == cue, "unchanged verified cue");
    const auto bad=qualify_external_cue_association(cue, {{0,"old.flac","old","evil\nFILE new.flac WAVE",true,true}}, {});
    check(!bad.qualified, "dangerous reference postimage");
}
void test_unsupported_source_encoding() {
    const std::string bad = std::string("\xff\xfe",2) + "F\0I\0";
    check(!qualify_external_cue_association(bad, {}, {}).qualified, "UTF16 cannot be rewritten as UTF8");
}
int main() {
    test_single(); test_multifile_unselected(); test_missing_and_ambiguous();
    test_exact_nochange_and_unsafe_postimage(); test_unsupported_source_encoding();
    std::cout << "PASS: 5 cue association test suites\n";
}
