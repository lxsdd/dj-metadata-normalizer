#include "stdafx.h"

#include "titleformat_planner.h"

#include <SDK/file_info_impl.h>

namespace djmeta_foobar {
namespace {

file_info_impl project_canonical_metadata(
    const file_info& original_info,
    const djmeta::MetadataDocument& canonical_metadata) {

    // Retain duration, technical info and ReplayGain so host title-formatting
    // functions continue to see the same non-metadata context.
    file_info_impl projected(original_info);
    projected.meta_remove_all();

    for (const djmeta::MetadataField& field : canonical_metadata.fields) {
        if (field.name.empty() || field.values.empty()) continue;

        const std::string& first = field.values.front();
        const t_size index = projected.__meta_add_unsafe_ex(
            field.name.data(),
            static_cast<t_size>(field.name.size()),
            first.data(),
            static_cast<t_size>(first.size()));

        for (std::size_t value_index = 1; value_index < field.values.size(); ++value_index) {
            const std::string& value = field.values[value_index];
            projected.meta_add_value_ex(
                index,
                value.data(),
                static_cast<t_size>(value.size()));
        }
    }
    return projected;
}

} // namespace

std::string TitleformatBatchEvaluator::evaluate(
    const playable_location& location,
    const file_info& original_info,
    const djmeta::MetadataDocument& canonical_metadata,
    std::string_view expression) {

    const std::string expression_text(expression);
    auto found = compiled_.find(expression_text);
    if (found == compiled_.end()) {
        titleformat_object::ptr compiled;
        if (!titleformat_compiler::get()->compile(compiled, expression_text.c_str()))
            throw std::invalid_argument("Invalid foobar2000 title-formatting expression.");
        found = compiled_.emplace(expression_text, compiled).first;
    }

    file_info_impl projected =
        project_canonical_metadata(original_info, canonical_metadata);
    pfc::string8 output;
    const titleformat_object::ptr& compiled = found->second;
    compiled->run_simple(location, &projected, output);
    return std::string(output.c_str(), output.get_length());
}

std::string evaluate_titleformat_against_canonical(
    const playable_location& location,
    const file_info& original_info,
    const djmeta::MetadataDocument& canonical_metadata,
    std::string_view expression) {
    TitleformatBatchEvaluator one_shot;
    return one_shot.evaluate(location, original_info, canonical_metadata, expression);
}

} // namespace djmeta_foobar
