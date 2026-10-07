#include "djmeta/c_api.h"

#include "djmeta/interop.h"
#include "djmeta/normalizer.h"
#include "djmeta/rule_loader.h"

#include <cstdlib>
#include <cstring>
#include <exception>
#include <new>
#include <string>

namespace {

char* copy_string(const std::string& value) {
    char* out = static_cast<char*>(std::malloc(value.size() + 1));
    if (!out) return nullptr;
    std::memcpy(out, value.data(), value.size());
    out[value.size()] = '\0';
    return out;
}

int set_error(char** error_utf8, int status, const std::string& message) {
    if (error_utf8) {
        *error_utf8 = copy_string(message);
        if (!*error_utf8) return DJMETA_STATUS_OUT_OF_MEMORY;
    }
    return status;
}

} // namespace

extern "C" {

unsigned int djmeta_abi_version(void) {
    return 1u;
}

int djmeta_analyze_json_v1(
    const char* metadata_vectors_json_utf8,
    const char* ruleset_json_utf8,
    char** analysis_json_utf8,
    char** error_utf8) {

    if (analysis_json_utf8) *analysis_json_utf8 = nullptr;
    if (error_utf8) *error_utf8 = nullptr;

    if (!metadata_vectors_json_utf8 || !ruleset_json_utf8 || !analysis_json_utf8)
        return set_error(error_utf8, DJMETA_STATUS_INVALID_ARGUMENT, "null required argument");

    try {
        const djmeta::MetadataDocument metadata =
            djmeta::parse_metadata_vectors_json(metadata_vectors_json_utf8);
        const djmeta::Ruleset ruleset =
            djmeta::parse_ruleset_json(ruleset_json_utf8);
        const djmeta::AnalysisResult result =
            djmeta::Engine{}.analyze(metadata, ruleset.rules, ruleset.revision);

        const std::string json = djmeta::serialize_analysis_json(result);
        *analysis_json_utf8 = copy_string(json);
        if (!*analysis_json_utf8)
            return set_error(error_utf8, DJMETA_STATUS_OUT_OF_MEMORY, "out of memory");
        return DJMETA_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return set_error(error_utf8, DJMETA_STATUS_OUT_OF_MEMORY, "out of memory");
    } catch (const std::exception& ex) {
        return set_error(error_utf8, DJMETA_STATUS_ANALYSIS_ERROR, ex.what());
    } catch (...) {
        return set_error(error_utf8, DJMETA_STATUS_INTERNAL_ERROR, "unknown internal error");
    }
}

void djmeta_free_string_v1(char* value) {
    std::free(value);
}

} // extern "C"
