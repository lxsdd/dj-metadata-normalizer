#include "djmeta/staging.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool ok, const char* message) {
    if (!ok) { std::cerr << "FAIL: " << message << "\n"; std::exit(1); }
}

djmeta::Proposal proposal(std::size_t field, const char* name,
                          const char* raw, const char* desired,
                          djmeta::SafetyClass safety) {
    djmeta::Proposal p;
    p.field_index = field;
    p.field = name;
    p.value_index = 0;
    p.original_value = raw;
    p.proposed_value = desired;
    p.safety = safety;
    return p;
}

void test_safe_only_and_semantic_exclusion() {
    const djmeta::MetadataDocument input{{
        {"TITLE", {"  Song  "}},
        {"ARTIST", {"Tiësto"}},
        {"DATE", {"1998"}},
        {"DATE_RAW", {"1998-06-15"}}
    }};
    djmeta::AnalysisResult analysis;
    analysis.input_fingerprint = djmeta::fingerprint(input);
    analysis.proposals.push_back(
        proposal(0, "TITLE", "  Song  ", "Song", djmeta::SafetyClass::Safe));
    analysis.proposals.push_back(
        proposal(1, "ARTIST", "Tiësto", "Tiesto", djmeta::SafetyClass::Review));

    const auto staged = djmeta::stage_safe_only(input, analysis);
    require(staged.safe_proposals_applied == 1 && staged.unresolved_proposals == 1,
            "SAFE and REVIEW must be counted separately");
    require(staged.document.fields[0].values[0] == "Song", "SAFE proposal not applied");
    require(staged.document.fields[1].values[0] == "Tiësto",
            "REVIEW proposal entered routing input");
    require(staged.document.fields[2].values[0] == "1998" &&
            staged.document.fields[3].values[0] == "1998-06-15",
            "year and complete date must remain independent");
    require(input.fields[0].values[0] == "  Song  ", "staging mutated raw input");
}

void test_mixed_chain_is_not_automatically_selected() {
    const djmeta::MetadataDocument input{{{"TITLE", {"  Song (Mix)  "}}}};
    djmeta::AnalysisResult analysis;
    analysis.input_fingerprint = djmeta::fingerprint(input);
    analysis.proposals.push_back(
        proposal(0, "TITLE", "  Song (Mix)  ", "Song (Remix)", djmeta::SafetyClass::Review));
    const auto staged = djmeta::stage_safe_only(input, analysis);
    require(staged.document == input && staged.unresolved_proposals == 1,
            "mixed SAFE+REVIEW chain must be wholly deferred");
}

void test_identity_guards() {
    const djmeta::MetadataDocument input{{{"TITLE", {" A "}}}};
    djmeta::AnalysisResult analysis;
    analysis.input_fingerprint = djmeta::fingerprint(input);
    analysis.proposals.push_back(
        proposal(0, "TITLE", " A ", "A", djmeta::SafetyClass::Safe));

    auto changed = input;
    changed.fields[0].values[0] = " B ";
    try {
        (void)djmeta::stage_safe_only(changed, analysis);
        require(false, "stale input accepted");
    } catch (const std::invalid_argument&) {}

    analysis.proposals[0].field_index = 5;
    try {
        (void)djmeta::stage_safe_only(input, analysis);
        require(false, "invalid field index accepted");
    } catch (const std::invalid_argument&) {}
}

void test_defense_in_depth_structural_proposals() {
    const djmeta::MetadataDocument input{{
        {"CUESHEET", {"FILE \"song.mp3\" MP3\r\n  TRACK 01 AUDIO"}},
        {"LYRICS", {"Verse 1\nVerse 2"}},
        {"COMMENT", {"Line 1\r\nLine 2"}},
        {"TITLE", {" A "}}
    }};
    djmeta::AnalysisResult a;
    a.input_fingerprint = djmeta::fingerprint(input);
    auto make = [&](std::size_t index, const char* replacement, djmeta::SafetyClass safety) {
        auto p = proposal(index, input.fields[index].name.c_str(),
                          input.fields[index].values[0].c_str(), replacement, safety);
        return p;
    };
    for (const auto cue_safety : {djmeta::SafetyClass::Safe, djmeta::SafetyClass::Review}) {
        a.proposals = {make(0, "forged cue", cue_safety)};
        try { (void)djmeta::stage_safe_only(input, a);
              require(false, "forged CUE proposal passed staging");
        } catch (const std::invalid_argument&) {}
    }
    for (const std::size_t index : {1u, 2u}) {
        a.proposals = {make(index, "flattened", djmeta::SafetyClass::Safe)};
        try { (void)djmeta::stage_safe_only(input, a);
              require(false, "forged multiline SAFE proposal passed staging");
        } catch (const std::invalid_argument&) {}
    }
    a.proposals = {make(3, "A", djmeta::SafetyClass::Safe)};
    const auto allowed = djmeta::stage_safe_only(input, a);
    require(allowed.document.fields[3].values[0] == "A" &&
            allowed.document.fields[0] == input.fields[0],
            "safe scalar TITLE change must preserve unrelated CUE bytes");
    a.proposals.push_back(a.proposals[0]);
    try { (void)djmeta::stage_safe_only(input, a);
          require(false, "duplicate staging target permitted");
    } catch (const std::invalid_argument&) {}
    a.proposals = {make(3, "A", djmeta::SafetyClass::Safe)};
    a.proposals.push_back(make(3, "Review", djmeta::SafetyClass::Review));
    try { (void)djmeta::stage_safe_only(input, a);
          require(false, "deferred duplicate proposal bypassed whole-batch validation");
    } catch (const std::invalid_argument&) {}
}

void test_malformed_utf8_safe_boundary() {
    const djmeta::MetadataDocument original{{
        {"TITLE", {"  Old Title  "}},
        {"COMMENT", {std::string("Old \xC2", 5)}}
    }};
    djmeta::AnalysisResult a;
    a.input_fingerprint = djmeta::fingerprint(original);
    a.proposals = {proposal(0, "TITLE", "  Old Title  ",
                           "Old Title", djmeta::SafetyClass::Safe)};
    const auto pass = djmeta::stage_safe_only(original, a);
    require(pass.document.fields[0].values[0] == "Old Title" &&
            pass.document.fields[1] == original.fields[1],
            "valid SAFE changes preserve unrelated malformed legacy bytes");
    a.proposals[0].proposed_value = std::string("New \xC0\xAF", 6);
    try { (void)djmeta::stage_safe_only(original, a);
        require(false, "forged SAFE invalid UTF-8 postimage accepted");
    } catch (const std::invalid_argument&) {}
    for (const auto& bad_text : {
        std::string("New\nline"),
        std::string("New\0hidden", 10)}) {
        a.proposals[0].proposed_value = bad_text;
        try {
            (void)djmeta::stage_safe_only(original, a);
            require(false, "forged SAFE control or multiline postimage accepted");
        } catch (const std::invalid_argument&) {}
    }
    a.proposals = {proposal(1, "COMMENT", original.fields[1].values[0].c_str(),
                           "Old", djmeta::SafetyClass::Safe)};
    try { (void)djmeta::stage_safe_only(original, a);
        require(false, "partial SAFE edit of malformed source accepted");
    } catch (const std::invalid_argument&) {}
}

void test_true_multivalue_and_duplicate_fields() {
    const djmeta::MetadataDocument input{{
        {"ARTIST", {" A ", " B "}},
        {"ARTIST", {" C "}}
    }};
    djmeta::AnalysisResult a;
    a.input_fingerprint = djmeta::fingerprint(input);
    auto p = proposal(1, "ARTIST", " C ", "C", djmeta::SafetyClass::Safe);
    a.proposals.push_back(p);
    const auto staged = djmeta::stage_safe_only(input, a);
    require(staged.document.fields.size() == 2, "duplicate field entry collapsed");
    require(staged.document.fields[0].values.size() == 2 &&
            staged.document.fields[0].values[0] == " A " &&
            staged.document.fields[1].values[0] == "C",
            "multivalue/duplicate field identity not retained");
}
}

int main() {
    test_safe_only_and_semantic_exclusion();
    test_mixed_chain_is_not_automatically_selected();
    test_identity_guards();
    test_true_multivalue_and_duplicate_fields();
    test_defense_in_depth_structural_proposals();
    test_malformed_utf8_safe_boundary();
    std::cout << "PASS: SAFE-only staging four suites\n";
}
