#pragma once

#include <cstddef>

namespace djmeta_foobar {

// Historical 2026-10-08 foo_fileops MOVE presets supplied by the user.
// Explicitly selected per preview; never used to auto-route or perform I/O.
// Stored here for initial read-only integration; future editable profiles
// must use foobar-native configuration and retain the original source fixture.
struct LegacyMoveRoute {
    const char* name;
    const char* destination_root;
    const char* foobar_titleformat;
};

inline constexpr LegacyMoveRoute legacy_move_routes[] = {
    {"Singles", R"(Z:\Music\Singles)", R"(%album artist%/%album%/%artist% - %title%)"},
    {"Alben", R"(Z:\Music\Alben)", R"(%album artist%/%album%[ '('%date%')']/%tracknumber%. %artist% - %title%)"},
    {"Livesets", R"(Z:\Music\Livesets)", R"(%artist%\%artist% - %album%)"},
};

inline constexpr std::size_t legacy_move_route_count =
    sizeof(legacy_move_routes) / sizeof(legacy_move_routes[0]);

} // namespace djmeta_foobar
