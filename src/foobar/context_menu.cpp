#include "stdafx.h"

#include "guid.h"
#include "preview.h"

namespace djmeta_foobar {
namespace {

contextmenu_group_popup_factory g_context_group(
    guids::context_group,
    contextmenu_groups::root,
    "DJ Library",
    0);

class normalizer_context_menu : public contextmenu_item_simple {
public:
    GUID get_parent() override { return guids::context_group; }
    unsigned get_num_items() override { return 1; }

    void get_item_name(unsigned index, pfc::string_base& output) override {
        if (index != 0) return;
        output = "Metadaten normalisieren (Vorschau)...";
    }

    GUID get_item_guid(unsigned index) override {
        if (index != 0) return pfc::guid_null;
        return guids::preview_normalization;
    }

    bool get_item_description(unsigned index, pfc::string_base& output) override {
        if (index != 0) return false;
        output =
            "Analysiert die ausgewählten Tracks mit dem gemeinsamen DJ-Metadata-Normalizer-Regelbestand "
            "und zeigt Original- und Vorschlagswerte. Diese Entwicklungsstufe schreibt keine Tags.";
        return true;
    }

    bool context_get_display(
        unsigned index,
        metadb_handle_list_cref data,
        pfc::string_base& output,
        unsigned& display_flags,
        const GUID& caller) override {
        if (index != 0 || data.get_count() == 0) return false;
        return contextmenu_item_simple::context_get_display(
            index, data, output, display_flags, caller);
    }

    void context_command(
        unsigned index,
        metadb_handle_list_cref data,
        const GUID&) override {
        if (index != 0 || data.get_count() == 0) return;

        metadb_handle_list retained = data;
        completion_notify::ptr notify = fb2k::makeCompletionNotify(
            [retained](unsigned status) {
                if (status != metadb_io::load_info_success) {
                    popup_message::g_show(
                        "Die Metadaten konnten nicht vollständig geladen werden. "
                        "Es wurde nichts verändert.",
                        "DJ Metadata Normalizer");
                    return;
                }
                show_normalization_preview(retained);
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
