#include "stdafx.h"

#include "menu_settings.h"

#include <SDK/cfg_var.h>

#include <array>
#include <stdexcept>
#include <string>

namespace djmeta_foobar {
namespace {
constexpr const char* kDefaults[menu_caption_count] = {
    "Metadaten normalisieren (Vorschau)...",
    "Vorbereiten: Singles (Vorschau)...",
    "Vorbereiten: Alben (Vorschau)...",
    "Vorbereiten: Livesets (Vorschau)...",
    "Tracks vorbereiten (Vorschau)...",
};

// Dedicated, stable GUID per caption. The SDK maps these to the active
// foobar profile; no independent registry or config.json.
const GUID guid_caption_metadata =
    {0x0aa00d72, 0x673f, 0x4511, {0xa1, 0x5f, 0x13, 0x20, 0x3a, 0x80, 0xb5, 0xc1}};
const GUID guid_caption_singles =
    {0x9df4b0e6, 0x5bea, 0x40f5, {0x84, 0xb1, 0x9a, 0xc1, 0x12, 0xf4, 0x10, 0x42}};
const GUID guid_caption_alben =
    {0x66a2c184, 0x17c9, 0x407d, {0x83, 0x3c, 0x7d, 0x91, 0xea, 0xf0, 0xae, 0x23}};
const GUID guid_caption_livesets =
    {0x4a611c1b, 0x13f8, 0x4ec3, {0x84, 0xbb, 0x46, 0x5e, 0x4b, 0xb1, 0x36, 0x12}};
const GUID guid_caption_primary =
    {0x29725de3, 0xa4bc, 0x4e04, {0xbb, 0xc2, 0xe5, 0xa8, 0xf1, 0x83, 0x79, 0x11}};

cfg_string g_caption_metadata(guid_caption_metadata, kDefaults[0]);
cfg_string g_caption_singles(guid_caption_singles, kDefaults[1]);
cfg_string g_caption_alben(guid_caption_alben, kDefaults[2]);
cfg_string g_caption_livesets(guid_caption_livesets, kDefaults[3]);
cfg_string g_caption_primary(guid_caption_primary, kDefaults[4]);

std::array<cfg_string*, menu_caption_count> captions = {
    &g_caption_metadata, &g_caption_singles, &g_caption_alben,
    &g_caption_livesets, &g_caption_primary
};
} // namespace

const char* default_menu_caption(unsigned index) {
    if (index >= menu_caption_count) throw std::out_of_range("invalid caption index");
    return kDefaults[index];
}

std::string effective_menu_caption(unsigned index) {
    if (index >= menu_caption_count) throw std::out_of_range("invalid caption index");
    const auto stored = captions[index]->get();
    return std::string(stored.c_str());
}

void set_menu_caption(unsigned index, const char* utf8) {
    if (index >= menu_caption_count || !utf8 || !*utf8)
        throw std::invalid_argument("invalid caption value");
    captions[index]->set(utf8);
}

} // namespace djmeta_foobar
