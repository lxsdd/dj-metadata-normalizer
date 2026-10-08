#include "djmeta/staging.h"

#include <stdexcept>

namespace djmeta {

StagedMetadata stage_safe_only(
    const MetadataDocument& original,
    const AnalysisResult& analysis) {
    if (fingerprint(original) != analysis.input_fingerprint)
        throw std::invalid_argument("staging input no longer matches analysis fingerprint");

    StagedMetadata staged;
    staged.document = original;

    for (const Proposal& proposal : analysis.proposals) {
        if (proposal.safety != SafetyClass::Safe) {
            ++staged.unresolved_proposals;
            continue;
        }
        if (proposal.field_index >= staged.document.fields.size())
            throw std::invalid_argument("SAFE proposal field index out of range");

        MetadataField& field = staged.document.fields[proposal.field_index];
        if (field.name != proposal.field ||
            proposal.value_index >= field.values.size() ||
            field.values[proposal.value_index] != proposal.original_value)
            throw std::invalid_argument("SAFE proposal identity does not match original metadata");

        field.values[proposal.value_index] = proposal.proposed_value;
        ++staged.safe_proposals_applied;
    }
    return staged;
}

} // namespace djmeta
