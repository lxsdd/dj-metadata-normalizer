#include "stdafx.h"

#include "legacy_routing_profiles.h"
#include "metadata_adapter.h"
#include "routing_preview.h"
#include "rules_runtime.h"
#include "djmeta/staging.h"
#include "titleformat_planner.h"

#include <map>
#include <string>
#include <vector>

namespace djmeta_foobar {
namespace {

struct ProposedRoute {
    metadb_handle_ptr handle;
    std::string input_fingerprint;
    std::string raw_relative_path;
    std::size_t safe_proposals = 0;
    std::size_t unresolved_proposals = 0;
    bool requires_physical_selection = false;
    bool titleformat_empty = false;
};

std::string single_line(std::string_view text, std::size_t max_bytes = 240) {
    std::string out;
    const std::size_t limit = (std::min)(text.size(), max_bytes);
    out.reserve(limit + 3);
    for (std::size_t i = 0; i < limit; ++i) {
        const unsigned char ch = static_cast<unsigned char>(text[i]);
        out.push_back(ch == 0 || ch == '\r' || ch == '\n' || ch == '\t'
            ? ' ' : static_cast<char>(ch));
    }
    if (text.size() > limit) out += "...";
    return out;
}


} // namespace

void show_custom_route_preview(
    const metadb_handle_list& handles,
    const RoutePreviewChoice& route) {
    try {
        if (route.display_name.empty() ||
            route.destination_root.empty() ||
            route.titleformat_expression.empty())
            throw std::invalid_argument("Routingprofil, Zielordner und Dateinamensmuster muessen gesetzt sein.");

        const auto loaded = load_rules_text();
        const djmeta::Ruleset ruleset = djmeta::parse_ruleset_json(loaded.json);

        // Exact string equality only. This catches repeated foobar subsongs
        // under the same observed path but does NOT establish physical file
        // identity under Windows aliases, junctions, or case differences.
        std::map<std::string, std::size_t> path_counts;
        for (t_size i = 0; i < handles.get_count(); ++i)
            ++path_counts[std::string(handles[i]->get_path())];

        std::vector<ProposedRoute> proposed;
        proposed.reserve(static_cast<std::size_t>(handles.get_count()));
        std::size_t physically_ambiguous = 0;
        std::size_t total_safe = 0;
        std::size_t total_unresolved = 0;

        for (t_size i = 0; i < handles.get_count(); ++i) {
            const metadb_handle_ptr& handle = handles[i];
            const auto info_container = handle->get_info_ref();
            const file_info& info = info_container->info();
            const djmeta::MetadataDocument original = metadata_from_file_info(info);
            const auto analysis = djmeta::Engine{}.analyze(
                original, ruleset.rules, ruleset.revision);

            ProposedRoute entry;
            entry.handle = handle;
            entry.input_fingerprint = analysis.input_fingerprint;
            entry.requires_physical_selection =
                handle->get_subsong_index() != 0 ||
                path_counts[std::string(handle->get_path())] != 1;

            if (entry.requires_physical_selection) {
                ++physically_ambiguous;
            } else {
                const auto staged = djmeta::stage_safe_only(original, analysis);
                entry.safe_proposals = staged.safe_proposals_applied;
                entry.unresolved_proposals = staged.unresolved_proposals;
                total_safe += entry.safe_proposals;
                total_unresolved += entry.unresolved_proposals;
                // Compile/evaluate using foobar's actual titleformat compiler
                // and an in-memory file_info; no custom parser and no writes.
                entry.raw_relative_path = evaluate_titleformat_against_canonical(
                    handle->get_location(), info, staged.document,
                    route.titleformat_expression);
                entry.titleformat_empty = entry.raw_relative_path.empty();
            }
            proposed.push_back(std::move(entry));
        }

        // Early warning for byte-identical proposed raw targets. Real
        // Windows/fileops destination identity is NOT established here.
        std::map<std::string, std::size_t> raw_target_counts;
        for (const auto& entry : proposed) {
            if (!entry.requires_physical_selection && !entry.titleformat_empty) {
                const std::string full = std::string(route.destination_root) +
                    "\\" + entry.raw_relative_path;
                ++raw_target_counts[full];
            }
        }
        std::size_t raw_collision_items = 0;
        for (const auto& entry : proposed) {
            if (!entry.requires_physical_selection && !entry.titleformat_empty) {
                const std::string full = std::string(route.destination_root) +
                    "\\" + entry.raw_relative_path;
                if (raw_target_counts[full] > 1) ++raw_collision_items;
            }
        }

        // Stale-input guard across the whole selection: rerun on change,
        // never show a plausible routing preview from mixed snapshots.
        for (const auto& entry : proposed) {
            const auto latest = metadata_from_file_info(
                entry.handle->get_info_ref()->info());
            if (djmeta::fingerprint(latest) != entry.input_fingerprint)
                throw std::runtime_error(
                    "Metadaten haben sich waehrend der Planung geaendert. "
                    "Bitte die Vorschau erneut oeffnen.");
        }

        std::string report = "Vorbereiten (NUR VORSCHAU) - ";
        report += route.display_name;
        report += "\nRegelrevision: ";
        report += ruleset.revision;
        report += "\nAusgewaehlte Eintraege: ";
        report += std::to_string(proposed.size());
        report += "\nPhysisch nicht eindeutig: ";
        report += std::to_string(physically_ambiguous);
        report += "\nSimulierte SAFE-Tagvorschlaege: ";
        report += std::to_string(total_safe);
        report += "\nNicht uebernommene CONFIDENT/REVIEW-Vorschlaege: ";
        report += std::to_string(total_unresolved);
        report += "\nGleiche rohe Zielnamen (Eintraege): ";
        report += std::to_string(raw_collision_items);
        report += "\n\nManuell gewaehltes Preset: ";
        report += route.display_name;
        report += "\nZielordner (Referenz): ";
        report += route.destination_root;
        report += "\nfoobar Title Formatting: ";
        report += route.titleformat_expression;
        report += "\n\nRoh-Auswertung der Dateinamen/-unterordner:";

        constexpr std::size_t kDetailLimit = 35;
        std::size_t count = 0;
        for (const auto& entry : proposed) {
            if (count++ >= kDetailLimit) break;
            report += "\n\n";
            report += single_line(entry.handle->get_path());
            if (entry.requires_physical_selection) {
                report += "\n  NICHT GEPLANT: virtueller Subsong oder mehrfach ausgewaehlte Quelldatei.";
                continue;
            }
            if (entry.titleformat_empty) {
                report += "\n  PRUEFEN: Dateinamensmuster ergab einen leeren Wert.";
                continue;
            }
            report += "\n  -> ";
            report += single_line(route.destination_root);
            report += "\\";
            report += single_line(entry.raw_relative_path);
            const std::string full_raw_target = std::string(route.destination_root) +
                "\\" + entry.raw_relative_path;
            if (raw_target_counts[full_raw_target] > 1)
                report += "\n  KONFLIKTHINWEIS: gleicher Roh-Zielname mehrfach vorhanden.";
            if (entry.unresolved_proposals != 0)
                report += "\n  Hinweis: fachliche Tagvorschlaege sind noch nicht freigegeben.";
        }
        if (proposed.size() > kDetailLimit) {
            report += "\n\n... weitere ";
            report += std::to_string(proposed.size() - kDetailLimit);
            report += " Eintraege werden hier nicht einzeln angezeigt.";
        }

        report += "\n\nWICHTIG: Dies sind Rohwerte aus Title Formatting, ";
        report += "KEINE geprueften File-Operations-Zielpfade.";
        report += "\nAuch unterschiedliche Rohpfade koennen dasselbe Windows-Ziel bezeichnen.";
        report += "\nDateinamen-Sanitizing, existierende Ziele, externe CUE-Dateien, ";
        report += "Begleitdateien und Zeitstempelrichtlinien sind hier noch NICHT geprueft.";
        report += "\nAndere Routen koennen im Kontextmenue manuell gewaehlt werden.";
        report += "\nEs wurden keine Tags geschrieben und keine Dateien veraendert.";
        popup_message::g_show(report.c_str(), "DJ Metadata Normalizer - Routing");
    } catch (const std::exception& error) {
        std::string message =
            "Routing-Vorschau fehlgeschlagen. Nichts wurde veraendert.\n\n";
        message += error.what();
        popup_message::g_show(message.c_str(), "DJ Metadata Normalizer");
    }
}

void show_legacy_route_preview(const metadb_handle_list& handles, std::size_t route_index) {
    if (route_index >= legacy_move_route_count) {
        popup_message::g_show(
            "Unbekanntes Routing-Vorschauprofil. Es wurde nichts veraendert.",
            "DJ Metadata Normalizer");
        return;
    }
    const LegacyMoveRoute& legacy = legacy_move_routes[route_index];
    const RoutePreviewChoice choice{
        legacy.name, legacy.destination_root, legacy.foobar_titleformat};
    show_custom_route_preview(handles, choice);
}

} // namespace djmeta_foobar
