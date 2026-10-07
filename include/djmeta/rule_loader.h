#pragma once

#include "djmeta/normalizer.h"

#include <string>
#include <string_view>
#include <vector>

namespace djmeta {

struct Ruleset {
    int schema_version = 0;
    std::string id;
    std::string revision;
    std::vector<Rule> rules;
};

Ruleset parse_ruleset_json(std::string_view json);

} // namespace djmeta
