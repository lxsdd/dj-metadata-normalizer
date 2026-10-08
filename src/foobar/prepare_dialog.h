#pragma once

#include <SDK/foobar2000.h>

namespace djmeta_foobar {

// Native, modal, read-only Prepare Tracks editor. It only chooses a
// one-time route profile for a preview; nothing is persisted or written.
void show_prepare_tracks_dialog(const metadb_handle_list& handles);

} // namespace djmeta_foobar
