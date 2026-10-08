#include "djmeta/review_decisions.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(bool value, const char* msg) {
    if (!value) {
        std::cerr << "FAIL: " << msg << '\n';
        std::exit(1);
    }
}
template<typename F> void should_reject(F action, const char* msg) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    check(false, msg);
}
}

int main() {
    using namespace djmeta;
    const MetadataDocument original {{
        {"TITLE", {" Old Title "}},
        {"ARTIST", {"Original Artist"}},
        {"VERSION", {"Mix A"}},
        {"CUSTOM", {"Uno", " Dos "}},
    }};
    AnalysisResult analysis;
    analysis.input_fingerprint = fingerprint(original);
    analysis.ruleset_revision = "test-v1";
    auto proposal = [&](std::size_t field, std::size_t value, const char* target,
                        SafetyClass safety) {
        Proposal p;
        p.field_index = field;
        p.field = original.fields[field].name;
        p.value_index = value;
        p.original_value = original.fields[field].values[value];
        p.proposed_value = target;
        p.safety = safety;
        p.rule_ids.push_back("stable.test.rule");
        return p;
    };
    analysis.proposals = {
        proposal(0, 0, "Old Title", SafetyClass::Safe),
        proposal(1, 0, "Canonical Artist", SafetyClass::Confident),
        proposal(2, 0, "Mix B", SafetyClass::Review),
        proposal(3, 1, "Dos", SafetyClass::Safe),
    };
    const auto automatic = project_review_decisions(original, analysis);
    check(automatic.document.fields[0].values[0] == "Old Title" &&
          automatic.document.fields[1].values[0] == "Original Artist" &&
          automatic.document.fields[2].values[0] == "Mix A" &&
          automatic.document.fields[3].values[0] == "Uno" &&
          automatic.document.fields[3].values[1] == "Dos" &&
          automatic.automatic_safe == 2 &&
          automatic.unresolved_semantic == 2,
          "only SAFE default preview, unrelated fields preserved");

    std::vector<ReviewDecision> decisions(analysis.proposals.size());
    decisions[0].action = ReviewAction::Reject;
    decisions[1].action = ReviewAction::Accept;
    decisions[2].action = ReviewAction::ManualValue;
    decisions[2].manual_value = "Custom Edit";
    const auto edited = project_review_decisions(original, analysis, decisions);
    check(edited.document.fields[0].values[0] == " Old Title " &&
          edited.document.fields[1].values[0] == "Canonical Artist" &&
          edited.document.fields[2].values[0] == "Custom Edit" &&
          edited.document.fields[3].values[1] == "Dos" &&
          edited.automatic_safe == 1 &&
          edited.explicitly_accepted == 1 &&
          edited.explicitly_rejected == 1 &&
          edited.manually_replaced == 1 &&
          edited.unresolved_semantic == 0,
          "Accept/Reject/Manual and unchanged SAFE are deterministic");

    decisions[2] = ReviewDecision{ReviewAction::Pending, ""};
    const auto pending = project_review_decisions(original, analysis, decisions);
    check(pending.unresolved_semantic == 1 &&
          pending.document.fields[2].values[0] == "Mix A",
          "unresolved REVIEW never leaks into projection");
    check(original.fields[0].values[0] == " Old Title " &&
          original.fields[1].values[0] == "Original Artist",
          "projection never mutates user originals");

    auto stale = original;
    stale.fields[0].values[0] = "Changed elsewhere";
    should_reject([&]{ (void)project_review_decisions(stale, analysis); },
                  "reject stale fingerprints");
    should_reject([&]{ (void)project_review_decisions(original, analysis, {ReviewDecision{}}); },
                  "reject decision cardinality mismatch");
    auto invalid = analysis;
    invalid.proposals[0].original_value = "not the original";
    should_reject([&]{ (void)project_review_decisions(original, invalid); },
                  "reject proposal source mismatch");
    invalid = analysis;
    invalid.proposals.push_back(invalid.proposals[0]);
    should_reject([&]{ (void)project_review_decisions(original, invalid); },
                  "reject colliding field/value proposals");
    decisions[2].action = ReviewAction::ManualValue;
    decisions[2].manual_value.clear();
    should_reject([&]{ (void)project_review_decisions(original, analysis, decisions); },
                  "manual empty value is not an implicit tag deletion");
    decisions[2].manual_value = "Bad\nValue";
    should_reject([&]{ (void)project_review_decisions(original, analysis, decisions); },
                  "unsafe control characters rejected in manual input");

    AnalysisResult none;
    none.input_fingerprint = fingerprint(original);
    const auto zero = project_review_decisions(original, none);
    check(zero.document == original && zero.automatic_safe == 0 &&
          zero.unresolved_semantic == 0,
          "no-op selection and empty proposals retain metadata");

    std::cout << "PASS: non-writing proposal decisions and atomic stale/conflict/unsafe guards\n";
}
