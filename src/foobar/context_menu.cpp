#include "stdafx.h"

#include "guid.h"
#include "preview.h"
#include "routing_preview.h"

namespace djmeta_foobar {
namespace {

contextmenu_group_popup_factory g_context_group(
    guids::context_group,
    contextmenu_groups::root,
    "DJ Metadata Normalizer",
    0);

class normalizer_context_menu : public contextmenu_item_simple {
public:
    GUID get_parent() override { return guids::context_group; }
    unsigned get_num_items() override { return 4; }

    // Inherit the user's existing Preferences > Display > Context Menu
    // visibility choices. New optional routes start out hidden but can be
    // enabled in foobar; this is a constant SDK default, not a runtime veto.
    t_enabled_state get_enabled_state(unsigned index) override {
        return index == 0 ? contextmenu_item::DEFAULT_ON
                          : contextmenu_item::DEFAULT_OFF;
    }

    void get_item_name(unsigned index, pfc::string_base& output) override {
        switch (index) {
        case 0: output = "Metadaten normalisieren (Vorschau)..."; break;
        case 1: output = "Vorbereiten: Singles (Vorschau)..."; break;
        case 2: output = "Vorbereiten: Alben (Vorschau)..."; break;
        case 3: output = "Vorbereiten: Livesets (Vorschau)..."; break;
        default: break;
        }
    }

    GUID get_item_guid(unsigned index) override {
        switch (index) {
        case 0: return guids::preview_normalization;
        case 1: return guids::preview_route_singles;
        case 2: return guids::preview_route_alben;
        case 3: return guids::preview_route_livesets;
        default: return pfc::guid_null;
        }
    }

    bool get_item_description(unsigned index, pfc::string_base& output) override {
        if (index > 3) return false;
        if (index == 0) {
            output =
                "Analysiert die ausgewählten Tracks mit dem gemeinsamen Normalizer-Regelbestand "
                "und zeigt Original- und Vorschlagswerte. Schreibt keine Tags.";
        } else {
            output =
                "Berechnet die gewählte historische foobar-File-Operations-Vorlage gegen "
                "eine SAFE-Metadatenvorschau. Keine Tags, keine Dateiumbenennung, kein Move/Copy.";
        }
        return true;
    }

    bool context_get_display(
        unsigned index,
        metadb_handle_list_cref data,
        pfc::string_base& output,
        unsigned& display_flags,
        const GUID& caller) override {
        if (index > 3 || data.get_count() == 0) return false;
        return contextmenu_item_simple::context_get_display(
            index, data, output, display_flags, caller);
    }

    void context_command(
        unsigned index,
        metadb_handle_list_cref data,
        const GUID&) override {
        if (index > 3 || data.get_count() == 0) return;

        metadb_handle_list retained = data;
        completion_notify::ptr notify = fb2k::makeCompletionNotify(
            [retained, index](unsigned status) {
                if (status != metadb_io::load_info_success) {
                    popup_message::g_show(
                        "Die Metadaten konnten nicht vollständig geladen werden. "
                        "Es wurde nichts verändert.",
                        "DJ Metadata Normalizer");
                    return;
                }
                if (index == 0) show_normalization_preview(retained);
                else show_legacy_route_preview(retained, index - 1);
            });

        metadb_io_v2::get()->load_info_async(
            retained,
            metadb_io::load_info_check_if_changed,
            core_api::get_main_window(),
            metadb_io_v2::op_flag_delay_ui,
            notify);
    }
};

contextmenu_item_factory_t<normalizer_context_menu> g_context_item_factory;

} // namespace
} // namespace djmeta_foobar
