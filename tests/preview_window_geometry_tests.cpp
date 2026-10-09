#include "djmeta/preview_window_geometry.h"

#include <cstdlib>
#include <iostream>
#include <string>

static void check(bool yes, const char* description) {
    if (!yes) { std::cerr << "FAIL: " << description << "\n"; std::exit(1); }
}
int main() {
    using namespace djmeta;
    for (const PreviewWindowSize original : {
        PreviewWindowSize{750, 520}, PreviewWindowSize{1600, 900},
        PreviewWindowSize{8192, 8192}, PreviewWindowSize{320, 240}
    }) {
        const auto serialized = serialize_preview_window_size(original);
        const auto restored = parse_preview_window_size(serialized);
        check(restored && *restored == original,
              "persisted logical dimensions roundtrip exactly, no precision drift");
        check(serialized == serialize_preview_window_size(*restored),
              "stable canonical encoding prevents unnecessary config writes");
    }
    for (const std::string bad : {
        "", "v1", "v2|700|500", "v1|700", "v1|700|500|extra",
        "v1||500", "v1|700|", "v1|x|500", "v1|700|x",
        "v1|-700|500", "v1|+700|500", "v1|700|500 ", "v1|700|500\n",
        "v1|319|500", "v1|700|239", "v1|8193|500",
        "v1|700|8193", "v1|999999999999999999999999|600",
        "v1|700|999999999999999999999999"
    }) {
        check(!parse_preview_window_size(bad),
              "bad, stale, oversized or unsupported profile cfg fails closed");
    }
    check(serialize_preview_window_size({1, 2}).empty(),
          "out-of-range size is not persisted");
    std::cout << "PASS: foobar preview window dimension persistence policy\n";
}
