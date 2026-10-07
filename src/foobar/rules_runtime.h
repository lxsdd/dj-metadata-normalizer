#pragma once

#include <string>

namespace djmeta_foobar {

struct loaded_rules_text {
    std::string json;
    std::string source_label;
};

loaded_rules_text load_rules_text();

} // namespace djmeta_foobar
