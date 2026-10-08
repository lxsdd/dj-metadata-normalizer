#include "djmeta/review_decisions.h"

#include <set>
#include <stdexcept>
#include <utility>

namespace djmeta {
namespace {
bool valid_manual_value(const std::string& text) {
    if (text.empty()) return false; // deletion is never implicit
    for (unsigned char ch : text)
        if (ch == 0 || ch == 0x7f || (ch < 0x20 && ch != '\t'))
            return false;
    return true;
}
}

ReviewProjection project_review_decisions(
    const MetadataDocument& original,
    const AnalysisResult& analysis,
    const std::vector<ReviewDecision>& decisions) {

    if (fingerprint(original) != analysis.input_fingerprint)
        throw std::invalid_argument("review input no longer matches captured metadata");
    if (!decisions.empty() && decisions.size() != analysis.proposals.size())
        throw std::invalid_argument("review decision count does not match proposals");

    // Validate the entire projection before making even an in-memory edit.
    // Repeated targets imply conflicting engine output, never last-write-wins.
    std::set<std::pair<std::size_t, std::size_t>> targets;
    for (std::size_t i = 0; i < analysis.proposals.size(); ++i) {
        const auto& proposal = analysis.proposals[i];
        if (proposal.field_index >= original.fields.size())
            throw std::invalid_argument("review field index out of range");
        const auto& field = original.fields[proposal.field_index];
        if (field.name != proposal.field || proposal.value_index >= field.values.size() ||
            field.values[proposal.value_index] != proposal.original_value)
            throw std::invalid_argument("review proposal does not match original field/value");
        if (!targets.emplace(proposal.field_index, proposal.value_index).second)
            throw std::invalid_argument("review has two proposals for one value");
        const auto& decision = decisions.empty() ? ReviewDecision{} : decisions[i];
        switch (decision.action) {
        case ReviewAction::Pending:
        case ReviewAction::Accept:
        case ReviewAction::Reject:
            if (!decision.manual_value.empty())
                throw std::invalid_argument("unused manual value in non-manual decision");
            break;
        case ReviewAction::ManualValue:
            if (!valid_manual_value(decision.manual_value))
                throw std::invalid_argument("manual replacement is empty or contains control characters");
            break;
        default:
            throw std::invalid_argument("unknown review action");
        }
    }

    ReviewProjection projected;
    projected.document = original;
    for (std::size_t i = 0; i < analysis.proposals.size(); ++i) {
        const Proposal& proposal = analysis.proposals[i];
        const auto& decision = decisions.empty() ? ReviewDecision{} : decisions[i];
        std::string& output = projected.document.fields[proposal.field_index].values[proposal.value_index];
        switch (decision.action) {
        case ReviewAction::Pending:
            if (proposal.safety == SafetyClass::Safe) {
                output = proposal.proposed_value;
                ++projected.automatic_safe;
            } else {
                ++projected.unresolved_semantic;
            }
            break;
        case ReviewAction::Accept:
            output = proposal.proposed_value;
            ++projected.explicitly_accepted;
            break;
        case ReviewAction::Reject:
            ++projected.explicitly_rejected;
            break;
        case ReviewAction::ManualValue:
            output = decision.manual_value;
            ++projected.manually_replaced;
            break;
        }
    }
    return projected;
}

} // namespace djmeta
