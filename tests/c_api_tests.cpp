#include "djmeta/c_api.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}

void test_success() {
    const char* metadata =
        R"([{"name":"TITLE","values":["  A   Title  "]},{"name":"ARTIST","values":["Artist"]}])";
    const char* rules =
        R"({"schema_version":1,"ruleset_id":"test","revision":"r1","rules":[)"
        R"({"id":"safe.trim","enabled":true,"priority":10,"fields":["*"],"match":{"kind":"always"},"transform":{"kind":"trim_whitespace"},"safety":"SAFE","source":{"kind":"builtin","rationale":"trim"}},)"
        R"({"id":"safe.collapse","enabled":true,"priority":20,"fields":["*"],"match":{"kind":"always"},"transform":{"kind":"collapse_whitespace"},"safety":"SAFE","source":{"kind":"builtin","rationale":"collapse"}}]})";

    char* output = nullptr;
    char* error = nullptr;
    const int status = djmeta_analyze_json_v1(metadata, rules, &output, &error);
    require(status == DJMETA_STATUS_OK, "C ABI analysis failed");
    require(output != nullptr, "C ABI did not return preview JSON");
    require(error == nullptr, "C ABI returned error on success");

    const std::string json(output);
    require(json.find("\"schema_version\":1") != std::string::npos, "analysis JSON schema missing");
    require(json.find("\"ruleset_revision\":\"r1\"") != std::string::npos, "analysis JSON revision missing");
    require(json.find("\"original\":\"  A   Title  \"") != std::string::npos, "analysis JSON original missing");
    require(json.find("\"proposed\":\"A Title\"") != std::string::npos, "analysis JSON proposal missing");
    require(json.find("\"rule_ids\":[\"safe.trim\",\"safe.collapse\"]") != std::string::npos,
            "analysis JSON rule provenance missing");
    require(json.find("\"canonical_preview\":[{\"name\":\"TITLE\",\"values\":[\"A Title\"]}") != std::string::npos,
            "analysis JSON canonical preview missing");

    djmeta_free_string_v1(output);
}

void test_schema_v2_unicode_whitespace() {
    const char* metadata =
        R"([{"name":"TITLE","values":["\u00a0A\u202f  B\u3000"]}])";
    const char* rules =
        R"({"schema_version":2,"ruleset_id":"test-v2","revision":"r2","rules":[)"
        R"({"id":"safe.unicode","enabled":true,"priority":5,"fields":["*"],"match":{"kind":"always"},"transform":{"kind":"normalize_unicode_whitespace"},"safety":"SAFE","source":{"kind":"builtin","rationale":"unicode whitespace"}},)"
        R"({"id":"safe.trim","enabled":true,"priority":10,"fields":["*"],"match":{"kind":"always"},"transform":{"kind":"trim_whitespace"},"safety":"SAFE","source":{"kind":"builtin","rationale":"trim"}},)"
        R"({"id":"safe.collapse","enabled":true,"priority":20,"fields":["*"],"match":{"kind":"always"},"transform":{"kind":"collapse_whitespace"},"safety":"SAFE","source":{"kind":"builtin","rationale":"collapse"}}]})";

    char* output = nullptr;
    char* error = nullptr;
    const int status = djmeta_analyze_json_v1(metadata, rules, &output, &error);
    require(status == DJMETA_STATUS_OK, "C ABI schema-v2 analysis failed");
    require(output != nullptr && error == nullptr, "C ABI schema-v2 output contract failed");

    const std::string json(output);
    require(json.find("\"ruleset_revision\":\"r2\"") != std::string::npos,
            "schema-v2 revision missing from analysis JSON");
    require(json.find("\"proposed\":\"A B\"") != std::string::npos,
            "schema-v2 unicode whitespace proposal missing");
    require(json.find("safe.unicode") != std::string::npos,
            "schema-v2 unicode rule provenance missing");
    djmeta_free_string_v1(output);
}

void test_errors_do_not_cross_abi() {
    char* output = nullptr;
    char* error = nullptr;
    int status = djmeta_analyze_json_v1("not-json", "{}", &output, &error);
    require(status == DJMETA_STATUS_ANALYSIS_ERROR, "invalid JSON should return analysis error");
    require(output == nullptr, "invalid input returned analysis output");
    require(error != nullptr && std::string(error).find("ruleset JSON") != std::string::npos,
            "invalid input did not return diagnostic text");
    djmeta_free_string_v1(error);

    error = nullptr;
    status = djmeta_analyze_json_v1(nullptr, "{}", &output, &error);
    require(status == DJMETA_STATUS_INVALID_ARGUMENT, "null input should return invalid argument");
    require(error != nullptr, "null input should return diagnostic text");
    djmeta_free_string_v1(error);
}

} // namespace

int main() {
    require(djmeta_abi_version() == 1u, "unexpected native ABI version");
    test_success();
    test_schema_v2_unicode_whitespace();
    test_errors_do_not_cross_abi();
    std::cout << "PASS: djmeta native analysis ABI\n";
    return 0;
}
