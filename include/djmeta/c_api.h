#pragma once

#include <stddef.h>

#if defined(_WIN32)
#if defined(DJMETA_BUILD_DLL)
#define DJMETA_API __declspec(dllexport)
#else
#define DJMETA_API __declspec(dllimport)
#endif
#else
#define DJMETA_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum {
    DJMETA_STATUS_OK = 0,
    DJMETA_STATUS_INVALID_ARGUMENT = 1,
    DJMETA_STATUS_ANALYSIS_ERROR = 2,
    DJMETA_STATUS_OUT_OF_MEMORY = 3,
    DJMETA_STATUS_INTERNAL_ERROR = 4
};

DJMETA_API unsigned int djmeta_abi_version(void);

DJMETA_API int djmeta_analyze_json_v1(
    const char* metadata_vectors_json_utf8,
    const char* ruleset_json_utf8,
    char** analysis_json_utf8,
    char** error_utf8);

DJMETA_API void djmeta_free_string_v1(char* value);

#ifdef __cplusplus
}
#endif
