#include "stdafx.h"

#include <SDK/coreDarkMode.h>

#include "batch_preview_dialog.h"
#include "legacy_routing_profiles.h"
#include "metadata_adapter.h"
#include "resource.h"
#include "routing_preview.h"
#include "rules_runtime.h"
#include "titleformat_planner.h"

#include "djmeta/batch_preview.h"
#include "djmeta/staging.h"

#include <commctrl.h>

#include <algorithm>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace djmeta_foobar {
namespace {

struct PreviewEntry {
    metadb_handle_ptr handle;
    djmeta::MetadataDocument staged;
    djmeta::BatchPreviewInputRow input;
    std::string input_fingerprint;
};

struct PreviewState {
    std::vector<PreviewEntry> entries;
    djmeta::BatchPreviewTable table;
    RoutePreviewChoice current_choice;
    std::wstring cell_buffer;
    fb2k::CCoreDarkModeHooks dark;
    HWND list = nullptr;
};

std::wstring from_utf8(std::string_view text) {
    if (text.empty()) return {};
    if (text.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
        throw std::invalid_argument("Preview text is too long.");
    const int length = static_cast<int>(text.size());
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                          text.data(), length, nullptr, 0);
    if (count <= 0) return L"[Invalid UTF-8]";
    std::wstring out(static_cast<std::size_t>(count), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                            text.data(), length, out.data(), count) != count)
        return L"[Invalid UTF-8]";
    return out;
}

std::string to_utf8(std::wstring_view text) {
    if (text.empty()) return {};
    if (text.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
        throw std::invalid_argument("Preview input is too long.");
    const int size = static_cast<int>(text.size());
    const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        text.data(), size, nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) throw std::invalid_argument("Invalid Unicode in preview input.");
    std::string out(static_cast<std::size_t>(bytes), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        text.data(), size, out.data(), bytes, nullptr, nullptr) != bytes)
        throw std::runtime_error("Unicode conversion failed.");
    return out;
}

std::wstring read_control(HWND window, int id) {
    const HWND child = GetDlgItem(window, id);
    if (!child) throw std::runtime_error("Missing batch editor field.");
    const int count = GetWindowTextLengthW(child);
    if (count < 0 || count > 16384)
        throw std::invalid_argument("Preview field exceeds 16,384 characters.");
    std::wstring out(static_cast<std::size_t>(count) + 1, L'\0');
    const int read = GetWindowTextW(child, out.data(), count + 1);
    out.resize(static_cast<std::size_t>(read));
    return out;
}

bool valid_user_text(std::wstring_view text) {
    if (text.empty()) return false;
    for (wchar_t ch : text) if (ch < 0x20 || ch == 0x7f) return false;
    return true;
}

RoutePreviewChoice read_choice(HWND window) {
    const std::wstring name = read_control(window, IDC_BATCH_PROFILE_NAME);
    const std::wstring root = read_control(window, IDC_BATCH_DESTINATION);
    const std::wstring script = read_control(window, IDC_BATCH_PATTERN);
    if (!valid_user_text(name) || !valid_user_text(root) ||
        !valid_user_text(script))
        throw std::invalid_argument(
            "Profile, destination and naming expression are required and "
            "must not contain control characters.");
    return {to_utf8(name), to_utf8(root), to_utf8(script)};
}

void load_profile(HWND window, int index) {
    if (index >= 0 && static_cast<std::size_t>(index) < legacy_move_route_count) {
        const auto& route = legacy_move_routes[index];
        SetDlgItemTextW(window, IDC_BATCH_PROFILE_NAME,
                        from_utf8(route.name).c_str());
        SetDlgItemTextW(window, IDC_BATCH_DESTINATION,
                        from_utf8(route.destination_root).c_str());
        SetDlgItemTextW(window, IDC_BATCH_PATTERN,
                        from_utf8(route.foobar_titleformat).c_str());
    }
    // "Custom" retains the current editable values.
}

void verify_snapshot(const PreviewEntry& entry) {
    const auto info = entry.handle->get_info_ref();
    const auto latest = metadata_from_file_info(info->info());
    if (djmeta::fingerprint(latest) != entry.input_fingerprint)
        throw std::runtime_error(
            "Metadata changed since this preview was created. "
            "Close and reopen Prepare Tracks.");
}

void update_table(PreviewState& state) {
    std::vector<djmeta::BatchPreviewInputRow> inputs;
    inputs.reserve(state.entries.size());
    for (const auto& entry : state.entries) inputs.push_back(entry.input);
    state.table = djmeta::describe_batch_preview(inputs);
    if (state.list) {
        ListView_SetItemCountEx(state.list, static_cast<int>(state.entries.size()),
            LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
        InvalidateRect(state.list, nullptr, FALSE);
    }
}

std::string status_text(const djmeta::BatchPreviewRow& row) {
    if (row.issues.empty()) return "Qualified inputs (not approved)";
    for (const auto& issue : row.issues) {
        if (issue == "PHYSICAL_SOURCE_UNQUALIFIED") return "Physical source: REVIEW";
        if (issue == "TARGET_EXPRESSION_EMPTY") return "Empty target: REVIEW";
        if (issue == "DUPLICATE_RAW_TARGET") return "Duplicate raw target";
        if (issue == "UNAPPROVED_METADATA_PROPOSALS") return "Metadata: REVIEW";
    }
    return "CUE / filesystem unchecked";
}

std::wstring cell_text(PreviewState& state, std::size_t row, int column) {
    if (row >= state.entries.size() || row >= state.table.rows.size()) return {};
    const auto& entry = state.entries[row];
    const auto& summary = state.table.rows[row];
    switch (column) {
    case 0: return from_utf8(entry.input.source_path);
    case 1: return from_utf8(entry.input.profile);
    case 2: return from_utf8(summary.raw_destination);
    case 3: return from_utf8(status_text(summary));
    default: return {};
    }
}

void add_column(HWND list, int index, const wchar_t* name, int width) {
    LVCOLUMNW column = {};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.pszText = const_cast<LPWSTR>(name);
    column.cx = width;
    column.iSubItem = index;
    if (ListView_InsertColumn(list, index, &column) < 0)
        throw std::runtime_error("Unable to add a preview table column.");
}

void apply_to_rows(HWND dialog, PreviewState& state, bool all) {
    const RoutePreviewChoice choice = read_choice(dialog);
    std::vector<std::size_t> selected;
    if (all) {
        for (std::size_t i = 0; i < state.entries.size(); ++i)
            selected.push_back(i);
    } else {
        int index = -1;
        while ((index = ListView_GetNextItem(state.list, index, LVNI_SELECTED)) >= 0)
            selected.push_back(static_cast<std::size_t>(index));
        if (selected.empty()) {
            MessageBoxW(dialog, L"Select one or more rows first.",
                        L"Prepare Tracks", MB_OK | MB_ICONINFORMATION);
            return;
        }
    }

    // Transactional: every evaluation must succeed before any displayed
    // batch row is updated. No metadata or filesystem state is modified.
    auto candidate = state.entries;
    for (const auto index : selected) {
        PreviewEntry& entry = candidate[index];
        verify_snapshot(entry);
        entry.input.profile = choice.display_name;
        entry.input.destination_root = choice.destination_root;
        entry.input.raw_relative_path.clear();
        entry.input.filesystem_target_checked = false;
        entry.input.cue_dependencies_checked = false;
        if (!entry.input.physical_source_qualified) continue;

        const auto current = entry.handle->get_info_ref();
        entry.input.raw_relative_path = evaluate_titleformat_against_canonical(
            entry.handle->get_location(), current->info(),
            entry.staged, choice.titleformat_expression);
    }

    // No mixed stale snapshots: refuse the entire preview update if any
    // selected or unselected source metadata has changed.
    for (const auto& entry : candidate) verify_snapshot(entry);

    state.entries = std::move(candidate);
    state.current_choice = choice;
    update_table(state);
}

std::vector<PreviewEntry> capture_preview(
    const metadb_handle_list& handles,
    const RoutePreviewChoice& choice) {

    const auto loaded = load_rules_text();
    const auto rules = djmeta::parse_ruleset_json(loaded.json);

    std::map<std::string, std::size_t> count_per_path;
    for (t_size i = 0; i < handles.get_count(); ++i)
        ++count_per_path[std::string(handles[i]->get_path())];

    std::vector<PreviewEntry> entries;
    entries.reserve(static_cast<std::size_t>(handles.get_count()));
    for (t_size i = 0; i < handles.get_count(); ++i) {
        const metadb_handle_ptr handle = handles[i];
        const auto info_ref = handle->get_info_ref();
        const file_info& info = info_ref->info();
        const auto original = metadata_from_file_info(info);
        const auto result = djmeta::Engine{}.analyze(
            original, rules.rules, rules.revision);
        auto staged = djmeta::stage_safe_only(original, result);

        PreviewEntry entry;
        entry.handle = handle;
        entry.input_fingerprint = result.input_fingerprint;
        entry.staged = std::move(staged.document);
        entry.input.source_path = handle->get_path();
        entry.input.physical_id = handle->get_path();
        entry.input.profile = choice.display_name;
        entry.input.destination_root = choice.destination_root;
        entry.input.semantic_proposals_pending = staged.unresolved_proposals > 0;
        entry.input.physical_source_qualified =
            handle->get_subsong_index() == 0 &&
            count_per_path[entry.input.source_path] == 1;
        // Unverified until the host-backed source/target and CUE preflight
        // resolves actual file identities. Never infer safe from raw paths.
        entry.input.filesystem_target_checked = false;
        entry.input.cue_dependencies_checked = false;
        if (entry.input.physical_source_qualified) {
            entry.input.raw_relative_path = evaluate_titleformat_against_canonical(
                handle->get_location(), info, entry.staged,
                choice.titleformat_expression);
        }
        entries.push_back(std::move(entry));
    }
    for (const auto& entry : entries) verify_snapshot(entry);
    return entries;
}

INT_PTR CALLBACK batch_dialog_proc(HWND dialog, UINT message, WPARAM wp, LPARAM lp) {
    if (message == WM_INITDIALOG) {
        auto* state = reinterpret_cast<PreviewState*>(lp);
        SetWindowLongPtrW(dialog, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        state->dark.AddDialogWithControls(dialog);
        state->list = GetDlgItem(dialog, IDC_BATCH_LIST);
        if (!state->list) return FALSE;

        ListView_SetExtendedListViewStyle(state->list,
            LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        add_column(state->list, 0, L"Source file", 170);
        add_column(state->list, 1, L"Profile", 82);
        add_column(state->list, 2, L"Proposed raw target", 275);
        add_column(state->list, 3, L"Status", 160);
        for (const int id : {IDC_BATCH_PROFILE_NAME, IDC_BATCH_DESTINATION,
                             IDC_BATCH_PATTERN})
            SendDlgItemMessageW(dialog, id, EM_LIMITTEXT, 16384, 0);
        for (const wchar_t* profile : {L"Singles", L"Albums", L"Live Sets", L"Custom"})
            SendDlgItemMessageW(dialog, IDC_BATCH_PROFILE_PICKER, CB_ADDSTRING,
                0, reinterpret_cast<LPARAM>(profile));
        SendDlgItemMessageW(dialog, IDC_BATCH_PROFILE_PICKER, CB_SETCURSEL, 3, 0);
        SetDlgItemTextW(dialog, IDC_BATCH_PROFILE_NAME,
                        from_utf8(state->current_choice.display_name).c_str());
        SetDlgItemTextW(dialog, IDC_BATCH_DESTINATION,
                        from_utf8(state->current_choice.destination_root).c_str());
        SetDlgItemTextW(dialog, IDC_BATCH_PATTERN,
                        from_utf8(state->current_choice.titleformat_expression).c_str());
        update_table(*state);
        return TRUE;
    }

    auto* state = reinterpret_cast<PreviewState*>(
        GetWindowLongPtrW(dialog, GWLP_USERDATA));
    if (!state) return FALSE;

    if (message == WM_NOTIFY) {
        const auto* header = reinterpret_cast<const NMHDR*>(lp);
        if (header && header->idFrom == IDC_BATCH_LIST &&
            header->code == LVN_GETDISPINFOW) {
            auto* info = reinterpret_cast<NMLVDISPINFOW*>(lp);
            if ((info->item.mask & LVIF_TEXT) != 0 && info->item.iItem >= 0) {
                state->cell_buffer = cell_text(*state,
                    static_cast<std::size_t>(info->item.iItem), info->item.iSubItem);
                info->item.pszText = state->cell_buffer.data();
            }
            return TRUE;
        }
    }

    if (message == 0x02E0u /* WM_DPICHANGED */) {
        const auto* dimensions = reinterpret_cast<const RECT*>(lp);
        if (dimensions) SetWindowPos(dialog, nullptr,
            dimensions->left, dimensions->top,
            dimensions->right - dimensions->left,
            dimensions->bottom - dimensions->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        return TRUE;
    }

    if (message != WM_COMMAND) return FALSE;
    const int id = LOWORD(wp);
    try {
        if (id == IDC_BATCH_PROFILE_PICKER && HIWORD(wp) == CBN_SELCHANGE) {
            const auto sel = SendDlgItemMessageW(
                dialog, IDC_BATCH_PROFILE_PICKER, CB_GETCURSEL, 0, 0);
            load_profile(dialog, static_cast<int>(sel));
            return TRUE;
        }
        if (id == IDC_BATCH_APPLY_SELECTED && HIWORD(wp) == BN_CLICKED) {
            apply_to_rows(dialog, *state, false);
            return TRUE;
        }
        if (id == IDC_BATCH_APPLY_ALL && HIWORD(wp) == BN_CLICKED) {
            apply_to_rows(dialog, *state, true);
            return TRUE;
        }
        if (id == IDCANCEL) {
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        }
    } catch (const std::exception& error) {
        const std::wstring message_text = from_utf8(error.what());
        MessageBoxW(dialog, message_text.c_str(), L"Batch Preview",
                    MB_OK | MB_ICONWARNING);
        return TRUE;
    }
    return FALSE;
}

} // namespace

void show_batch_preview_dialog(
    const metadb_handle_list& handles,
    const RoutePreviewChoice& initial_choice) {
    try {
        if (handles.get_count() > static_cast<t_size>((std::numeric_limits<int>::max)()))
            throw std::runtime_error("Too many tracks for the batch preview table.");

        PreviewState state;
        state.current_choice = initial_choice;
        state.entries = capture_preview(handles, initial_choice);
        update_table(state);

        INITCOMMONCONTROLSEX controls = {};
        controls.dwSize = sizeof(controls);
        controls.dwICC = ICC_LISTVIEW_CLASSES;
        if (!InitCommonControlsEx(&controls))
            throw std::runtime_error("Unable to initialize the native list view.");

        const INT_PTR result = DialogBoxParamW(
            core_api::get_my_instance(),
            MAKEINTRESOURCEW(IDD_BATCH_PREVIEW),
            core_api::get_main_window(), batch_dialog_proc,
            reinterpret_cast<LPARAM>(&state));
        if (result == -1)
            throw std::runtime_error("Unable to open the batch preview dialog.");
    } catch (const std::exception& error) {
        std::string message =
            "Unable to prepare the read-only batch preview. Nothing was changed.\n\n";
        message += error.what();
        popup_message::g_show(message.c_str(), "DJ Metadata Normalizer");
    }
}

} // namespace djmeta_foobar
