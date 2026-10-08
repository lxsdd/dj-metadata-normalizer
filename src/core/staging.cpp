#include "djmeta/staging.h"
#include "djmeta/structural_guard.h"

#include <stdexcept>
#include <set>
#include <utility>

namespace djmeta {

StagedMetadata stage_safe_only(
    const MetadataDocument& original,
    const AnalysisResult& analysis) {
    if (fingerprint(original) != analysis.input_fingerprint)
        throw std::invalid_argument("staging input no longer matches analysis fingerprint");

    // Validate ALL proposal identities before constructing output. A REVIEW
    // proposal cannot evade invalid source checks by being deferred, and
    // duplicate targets must not create order-dependent behavior.
    std::set<std::pair<std::size_t, std::size_t>> targets;
    for (const Proposal& proposal : analysis.proposals) {
        if (proposal.field_index >= original.fields.size())
            throw std::invalid_argument("staging proposal field index out of range");
        const MetadataField& source = original.fields[proposal.field_index];
        if (source.name != proposal.field ||
            proposal.value_index >= source.values.size() ||
            source.values[proposal.value_index] != proposal.original_value)
            throw std::invalid_argument("staging proposal does not match original field/value");
        if (!targets.emplace(proposal.field_index, proposal.value_index).second)
            throw std::invalid_argument("staging has duplicate field/value proposals");
        if (is_protected_cue_metadata(source.name))
            throw std::invalid_argument("embedded CUE must use a dedicated editor");
        if (proposal.safety == SafetyClass::Safe) {
            if (!safe_scalar_metadata_field(source.name, proposal.original_value) ||
                !safe_scalar_metadata_field(source.name, proposal.proposed_value))
                throw std::invalid_argument("unsafe automatic edit of structural metadata");
            if (!valid_utf8_metadata_text(proposal.original_value) ||
                !valid_utf8_metadata_text(proposal.proposed_value))
                throw std::invalid_argument("SAFE proposal contains malformed UTF-8");
        }
    }

    StagedMetadata staged;
    staged.document = original;
    for (const Proposal& proposal : analysis.proposals) {
        if (proposal.safety != SafetyClass::Safe) {
            ++staged.unresolved_proposals;
            continue;
        }
        staged.document.fields[proposal.field_index].values[proposal.value_index] =
            proposal.proposed_value;
        ++staged.safe_proposals_applied;
    }
    return staged;
}

} // namespace djmeta
