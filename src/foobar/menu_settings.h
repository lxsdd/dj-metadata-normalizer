#pragma once

#include <string>

namespace djmeta_foobar {

// Order matches context_menu.cpp item indices:
// 0 normalizer, 1 singles, 2 albums, 3 live sets, 4 primary Prepare.
constexpr unsigned menu_caption_count = 5;

const char* default_menu_caption(unsigned index);
std::string effective_menu_caption(unsigned index);
void set_menu_caption(unsigned index, const char* utf8);

} // namespace djmeta_foobar
