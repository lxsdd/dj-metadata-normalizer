#include "legacy_routing_profiles.h"

#include <cstdlib>
#include <iostream>
#include <string_view>

void check(bool ok) {
    if (!ok) { std::cerr << "FAIL: legacy foobar move-route fixture drift\n"; std::exit(1); }
}

int main() {
    using namespace djmeta_foobar;
    check(legacy_move_route_count == 3);
    check(std::string_view(legacy_move_routes[0].name) == "Singles");
    check(std::string_view(legacy_move_routes[1].name) == "Alben");
    check(std::string_view(legacy_move_routes[2].name) == "Livesets");
    check(std::string_view(legacy_move_routes[0].destination_root) == R"(Z:\Music\Singles)");
    check(std::string_view(legacy_move_routes[1].destination_root) == R"(Z:\Music\Alben)");
    check(std::string_view(legacy_move_routes[2].destination_root) == R"(Z:\Music\Livesets)");
    check(std::string_view(legacy_move_routes[0].foobar_titleformat) ==
        R"(%album artist%/%album%/%artist% - %title%)");
    check(std::string_view(legacy_move_routes[1].foobar_titleformat) ==
        R"(%album artist%/%album%[ '('%date%')']/%tracknumber%. %artist% - %title%)");
    check(std::string_view(legacy_move_routes[2].foobar_titleformat) ==
        R"(%artist%\%artist% - %album%)");
    std::cout << "PASS: 3 legacy move routes unchanged\n";
    return 0;
}
