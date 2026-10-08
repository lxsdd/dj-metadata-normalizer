#include "stdafx.h"

#include <SDK/coreDarkMode.h>

#include "batch_preview_dialog.h"
#include "batch_table_settings.h"
#include "legacy_routing_profiles.h"
#include "metadata_adapter.h"
#include "resource.h"
#include "routing_preview.h"
#include "rules_runtime.h"
#include "titleformat_planner.h"

#include "djmeta/batch_preview.h"
#include "djmeta/metadata_diff.h"
#include "djmeta/table_layout.h"
#include "djmeta/staging.h"

#include <commctrl.h>

#include <algorithm>
#include <limits>
#include <map>
#include <set>
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

struct ResizableControl {
    HWND window = nullptr;
    RECT original{};
    bool stretch_width = false;
    bool stretch_height = false;
    bool shift_down = false;
    bool shift_right = false;
};

struct PreviewState {
    HWND dialog = nullptr;
    int initial_client_width = 0;
    int initial_client_height = 0;
    int initial_list_bottom = 0;
    int initial_window_width = 0;
    int initial_window_height = 0;
    std::vector<ResizableControl> resize_controls;
    std::vector<PreviewEntry> entries;
    djmeta::BatchPreviewTable table;
    RoutePreviewChoice current_choice;
    std::wstring cell_buffer;
    fb2k::CCoreDarkModeHooks dark;
    HWND list = nullptr;
    HWND metadata_list = nullptr;
    HWND tabs = nullptr;
    bool show_metadata = true;
    int metadata_sort_column = 0;
    bool metadata_sort_descending = false;
    std::vector<djmeta::AnalysisResult> analyses;
    std::vector<djmeta::MetadataDiffRow> metadata_rows;
    std::vector<std::size_t> metadata_view_order;
    std::vector<std::string> source_labels;
    djmeta::BatchTableLayout layout = djmeta::default_batch_table_layout();
    // Visible ListView item index -> underlying input row identity.
    std::vector<std::size_t> view_order;
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
    // Preserve underlying identities, NOT virtual screen positions, when
    // a sort or user override causes displayed rows to change order.
    std::set<std::size_t> selected_entries;
    if (state.list) {
        int view_row = -1;
        while ((view_row = ListView_GetNextItem(state.list, view_row, LVNI_SELECTED)) >= 0) {
            const auto index = static_cast<std::size_t>(view_row);
            if (index < state.view_order.size())
                selected_entries.insert(state.view_order[index]);
        }
    }

    std::vector<djmeta::BatchPreviewInputRow> inputs;
    inputs.reserve(state.entries.size());
    for (const auto& entry : state.entries) inputs.push_back(entry.input);
    state.table = djmeta::describe_batch_preview(inputs);
    state.view_order = djmeta::sort_batch_table_view(
        inputs, state.table, state.layout.sort_column, state.layout.sort_descending);

    if (state.list) {
        ListView_SetItemCountEx(state.list, static_cast<int>(state.entries.size()),
            LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
        if (!selected_entries.empty()) {
            ListView_SetItemState(state.list, -1, 0, LVIS_SELECTED);
            for (std::size_t i = 0; i < state.view_order.size(); ++i)
                if (selected_entries.count(state.view_order[i]))
                    ListView_SetItemState(state.list, static_cast<int>(i),
                        LVIS_SELECTED, LVIS_SELECTED);
        }
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

// Strip foobar's file:// locator only for presentation, never for identity.
std::string display_file_path(const std::string& path) {
    if (path.compare(0, 8, "file:///") == 0)
        return path.substr(8);
    if (path.compare(0, 7, "file://") == 0)
        return path.substr(7);
    return path;
}

std::wstring cell_text(PreviewState& state, std::size_t row, int column) {
    if (row >= state.entries.size() || row >= state.table.rows.size()) return {};
    const auto& entry = state.entries[row];
    const auto& summary = state.table.rows[row];
    switch (column) {
    case 0: return from_utf8(display_file_path(entry.input.source_path));
    case 1: return from_utf8(entry.input.profile);
    case 2: return from_utf8(summary.raw_destination);
    case 3: return from_utf8(status_text(summary));
    default: return {};
    }
}

std::wstring metadata_cell_text(PreviewState& state,
                                 std::size_t row, int column) {
    if (row >= state.metadata_view_order.size()) return {};
    const auto index = state.metadata_view_order[row];
    if (index >= state.metadata_rows.size()) return {};
    const auto& item = state.metadata_rows[index];
    switch (column) {
    case 0: return item.source_index < state.source_labels.size()
        ? from_utf8(display_file_path(state.source_labels[item.source_index]))
        : std::wstring{};
    case 1: return from_utf8(item.field);
    case 2: return from_utf8(item.original);
    case 3: return from_utf8(item.proposed);
    case 4: return from_utf8(djmeta::to_string(item.safety));
    case 5: return from_utf8(item.rule_ids);
    default: return {};
    }
}

void update_metadata_table(PreviewState& state) {
    state.metadata_view_order = djmeta::sort_metadata_diff_rows(
        state.metadata_rows, state.source_labels,
        state.metadata_sort_column, state.metadata_sort_descending);
    if (state.metadata_list) {
        ListView_SetItemCountEx(state.metadata_list,
            static_cast<int>(state.metadata_rows.size()),
            LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
        InvalidateRect(state.metadata_list, nullptr, FALSE);
    }
}

void show_preview_page(HWND dialog, PreviewState& state, bool metadata) {
    state.show_metadata = metadata;
    ShowWindow(state.metadata_list, metadata ? SW_SHOW : SW_HIDE);
    ShowWindow(state.list, metadata ? SW_HIDE : SW_SHOW);
    for (int id : {IDC_BATCH_PROFILE_PICKER, IDC_BATCH_PROFILE_NAME,
                   IDC_BATCH_DESTINATION, IDC_BATCH_PATTERN,
                   IDC_BATCH_APPLY_SELECTED, IDC_BATCH_APPLY_ALL}) {
        EnableWindow(GetDlgItem(dialog, id), metadata ? FALSE : TRUE);
    }
    if (metadata) {
        const std::wstring caption =
            L"Metadata proposals: " + std::to_wstring(state.metadata_rows.size()) +
            L". SAFE values are staged for filename preview; no tags are written.";
        SetDlgItemTextW(dialog, IDC_BATCH_HINT, caption.c_str());
    } else {
        SetDlgItemTextW(dialog, IDC_BATCH_HINT,
            L"File targets and CUE dependencies are not verified. "
            L"Routing changes affect this preview only.");
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


// All column IDs are logical IDs; the user may rearrange their visual order.
constexpr const wchar_t* kBatchColumnNames[djmeta::kBatchPreviewColumnCount] = {
    L"Source file", L"Profile", L"Proposed raw target", L"Status"
};
constexpr unsigned kColumnMenuBase = 41000u;
constexpr unsigned kColumnMenuReset = 41020u;

int current_dpi(HWND window) {
    HDC device = GetDC(window);
    if (!device) return 96;
    const int dpi = GetDeviceCaps(device, LOGPIXELSX);
    ReleaseDC(window, device);
    return dpi > 0 ? dpi : 96;
}
int to_pixels(HWND window, int logical) {
    return MulDiv(logical, current_dpi(window), 96);
}
int to_logical(HWND window, int pixels) {
    return MulDiv(pixels, 96, current_dpi(window));
}

void show_sort_arrow(PreviewState& state) {
    if (!state.list) return;
    const HWND header = ListView_GetHeader(state.list);
    if (!header) return;
    for (int col = 0; col < djmeta::kBatchPreviewColumnCount; ++col) {
        HDITEMW header_item{};
        header_item.mask = HDI_FORMAT;
        if (SendMessageW(header, HDM_GETITEMW, static_cast<WPARAM>(col),
                reinterpret_cast<LPARAM>(&header_item)) == 0) continue;
        header_item.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        if (col == state.layout.sort_column)
            header_item.fmt |= state.layout.sort_descending ? HDF_SORTDOWN : HDF_SORTUP;
        SendMessageW(header, HDM_SETITEMW, static_cast<WPARAM>(col),
            reinterpret_cast<LPARAM>(&header_item));
    }
}

void apply_column_layout(PreviewState& state) {
    if (!state.list) return;
    for (int col = 0; col < djmeta::kBatchPreviewColumnCount; ++col) {
        const unsigned flag = 1u << static_cast<unsigned>(col);
        const int width = (state.layout.visible_mask & flag) != 0
            ? to_pixels(state.list, state.layout.widths[static_cast<std::size_t>(col)])
            : 0;
        ListView_SetColumnWidth(state.list, col, width);
    }
    ListView_SetColumnOrderArray(
        state.list, djmeta::kBatchPreviewColumnCount, state.layout.order.data());
    show_sort_arrow(state);
}

void capture_column_layout(PreviewState& state) {
    if (!state.list) return;
    std::array<int, djmeta::kBatchPreviewColumnCount> current_order{};
    if (ListView_GetColumnOrderArray(
            state.list, djmeta::kBatchPreviewColumnCount,
            current_order.data())) {
        state.layout.order = current_order;
    }
    for (int col = 0; col < djmeta::kBatchPreviewColumnCount; ++col) {
        if ((state.layout.visible_mask & (1u << static_cast<unsigned>(col))) == 0)
            continue; // preserve remembered width of hidden columns
        const int pixels = ListView_GetColumnWidth(state.list, col);
        if (pixels > 0) {
            const int width = to_logical(state.list, pixels);
            state.layout.widths[static_cast<std::size_t>(col)] =
                (std::max)(48, (std::min)(3000, width));
        }
    }
}

void show_column_menu(HWND dialog, PreviewState& state, LPARAM pointer) {
    if (!state.list) return;
    capture_column_layout(state);
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    for (unsigned col = 0; col < djmeta::kBatchPreviewColumnCount; ++col) {
        const unsigned bit = 1u << col;
        const bool shown = (state.layout.visible_mask & bit) != 0;
        const UINT flags = MF_STRING |
            (shown ? MF_CHECKED : MF_UNCHECKED) |
            (shown && state.layout.visible_mask == bit ? MF_GRAYED : 0u);
        AppendMenuW(menu, flags, kColumnMenuBase + col, kBatchColumnNames[col]);
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kColumnMenuReset, L"Reset column layout");

    POINT location{static_cast<SHORT>(LOWORD(pointer)),
                   static_cast<SHORT>(HIWORD(pointer))};
    if (location.x == -1 && location.y == -1) GetCursorPos(&location);
    const UINT selected = TrackPopupMenu(menu,
        TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, location.x, location.y,
        0, dialog, nullptr);
    DestroyMenu(menu);

    if (selected >= kColumnMenuBase &&
        selected < kColumnMenuBase + djmeta::kBatchPreviewColumnCount) {
        const unsigned column = selected - kColumnMenuBase;
        const unsigned bit = 1u << column;
        if ((state.layout.visible_mask & bit) != 0) {
            if (state.layout.visible_mask == bit) return; // one visible minimum
            state.layout.visible_mask &= ~bit;
        } else {
            state.layout.visible_mask |= bit;
        }
        apply_column_layout(state);
    } else if (selected == kColumnMenuReset) {
        state.layout = djmeta::default_batch_table_layout();
        apply_column_layout(state);
        update_table(state);
    }
}

// The header is a child of the ListView, not the dialog. Subclassing it
// ensures keyboard/mouse WM_CONTEXTMENU reliably reaches our column picker.
LRESULT CALLBACK batch_header_proc(
    HWND header, UINT message, WPARAM wp, LPARAM lp,
    UINT_PTR subclass_id, DWORD_PTR ref_data) {
    auto* state = reinterpret_cast<PreviewState*>(ref_data);
    if (message == WM_CONTEXTMENU && state && state->list) {
        show_column_menu(GetParent(state->list), *state, lp);
        return 0;
    }
    if (message == WM_NCDESTROY)
        RemoveWindowSubclass(header, batch_header_proc, subclass_id);
    return DefSubclassProc(header, message, wp, lp);
}

void capture_resize_layout(PreviewState& state) {
    const HWND dialog = state.dialog;
    if (!dialog || !state.list) return;
    RECT client{}, window_rect{}, list_rect{};
    GetClientRect(dialog, &client);
    GetWindowRect(dialog, &window_rect);
    GetWindowRect(state.list, &list_rect);
    MapWindowPoints(HWND_DESKTOP, dialog,
                    reinterpret_cast<POINT*>(&list_rect), 2);
    state.initial_list_bottom = list_rect.bottom;
    state.initial_client_width = client.right;
    state.initial_client_height = client.bottom;
    state.initial_window_width = window_rect.right - window_rect.left;
    state.initial_window_height = window_rect.bottom - window_rect.top;
    state.resize_controls.clear();

    EnumChildWindows(dialog, [](HWND control, LPARAM state_ptr) -> BOOL {
        auto& current = *reinterpret_cast<PreviewState*>(state_ptr);
        if (GetParent(control) != current.dialog) return TRUE;
        ResizableControl layout;
        layout.window = control;
        GetWindowRect(control, &layout.original);
        MapWindowPoints(HWND_DESKTOP, current.dialog,
                        reinterpret_cast<POINT*>(&layout.original), 2);
        const int id = GetDlgCtrlID(control);
        layout.stretch_width =
            id == IDC_BATCH_LIST || id == IDC_BATCH_PROFILE_NAME ||
            id == IDC_BATCH_DESTINATION || id == IDC_BATCH_PATTERN ||
            (id == -1 && layout.original.right >
             current.initial_client_width - 24);
        layout.stretch_height = id == IDC_BATCH_LIST;
        layout.shift_down = id != IDC_BATCH_LIST &&
            layout.original.top >= current.initial_list_bottom;
        layout.shift_right = id == IDC_BATCH_APPLY_SELECTED ||
                             id == IDC_BATCH_APPLY_ALL || id == IDCANCEL;
        current.resize_controls.push_back(layout);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&state));
}

void resize_batch_dialog(PreviewState& state, int width, int height) {
    if (state.resize_controls.empty() || width < 1 || height < 1) return;
    const int dx = width - state.initial_client_width;
    const int dy = height - state.initial_client_height;
    for (const auto& child : state.resize_controls) {
        if (!IsWindow(child.window)) continue;
        const RECT& orig = child.original;
        const int x = orig.left + (child.shift_right ? dx : 0);
        const int y = orig.top + (child.shift_down ? dy : 0);
        const int w = (orig.right - orig.left) + (child.stretch_width ? dx : 0);
        const int h = (orig.bottom - orig.top) + (child.stretch_height ? dy : 0);
        SetWindowPos(child.window, nullptr, x, y,
                     (std::max)(8, w), (std::max)(8, h),
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
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
            if (static_cast<std::size_t>(index) < state.view_order.size())
                selected.push_back(state.view_order[static_cast<std::size_t>(index)]);
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
    const RoutePreviewChoice& choice,
    std::vector<djmeta::AnalysisResult>& analyses) {

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
        auto result = djmeta::Engine{}.analyze(
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
        analyses.push_back(std::move(result));
    }
    for (const auto& entry : entries) verify_snapshot(entry);
    return entries;
}

INT_PTR CALLBACK batch_dialog_proc(HWND dialog, UINT message, WPARAM wp, LPARAM lp) {
    if (message == WM_INITDIALOG) {
        auto* state = reinterpret_cast<PreviewState*>(lp);
        SetWindowLongPtrW(dialog, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        try {
        state->dark.AddDialogWithControls(dialog);
        state->list = GetDlgItem(dialog, IDC_BATCH_LIST);
        state->metadata_list = GetDlgItem(dialog, IDC_METADATA_LIST);
        state->tabs = GetDlgItem(dialog, IDC_BATCH_TABS);
        if (!state->list || !state->metadata_list || !state->tabs) return FALSE;
        for (const wchar_t* name : {L"Metadata changes", L"File locations"}) {
            TCITEMW tab{};
            tab.mask = TCIF_TEXT;
            tab.pszText = const_cast<wchar_t*>(name);
            TabCtrl_InsertItem(state->tabs, TabCtrl_GetItemCount(state->tabs), &tab);
        }
        TabCtrl_SetCurSel(state->tabs, 0);
        ListView_SetExtendedListViewStyle(state->metadata_list,
            LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP);
        add_column(state->metadata_list, 0, L"Source file", 240);
        add_column(state->metadata_list, 1, L"Field", 110);
        add_column(state->metadata_list, 2, L"Original", 235);
        add_column(state->metadata_list, 3, L"Proposed", 235);
        add_column(state->metadata_list, 4, L"Safety", 95);
        add_column(state->metadata_list, 5, L"Rule IDs", 200);

        ListView_SetExtendedListViewStyle(state->list,
            LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER |
            LVS_EX_HEADERDRAGDROP);
        add_column(state->list, 0, L"Source file", 170);
        add_column(state->list, 1, L"Profile", 82);
        add_column(state->list, 2, L"Proposed raw target", 275);
        add_column(state->list, 3, L"Status", 160);
        state->layout = load_batch_table_layout();
        apply_column_layout(*state);
        const HWND batch_header = ListView_GetHeader(state->list);
        if (batch_header &&
            !SetWindowSubclass(batch_header, batch_header_proc, 1,
                reinterpret_cast<DWORD_PTR>(state)))
            throw std::runtime_error("Unable to attach batch column menu.");
        for (const int id : {IDC_BATCH_PROFILE_NAME, IDC_BATCH_DESTINATION,
                             IDC_BATCH_PATTERN})
            SendDlgItemMessageW(dialog, id, EM_LIMITTEXT, 16384, 0);
        for (const wchar_t* profile : {L"Singles", L"Albums", L"Live Sets", L"Custom"})
            SendDlgItemMessageW(dialog, IDC_BATCH_PROFILE_PICKER, CB_ADDSTRING,
                0, reinterpret_cast<LPARAM>(profile));
        int initial_profile = 3;
        for (std::size_t i = 0; i < legacy_move_route_count; ++i) {
            const auto& profile = legacy_move_routes[i];
            if (state->current_choice.display_name == profile.name &&
                state->current_choice.destination_root == profile.destination_root &&
                state->current_choice.titleformat_expression == profile.foobar_titleformat) {
                initial_profile = static_cast<int>(i);
                break;
            }
        }
        SendDlgItemMessageW(dialog, IDC_BATCH_PROFILE_PICKER, CB_SETCURSEL,
                            static_cast<WPARAM>(initial_profile), 0);
        SetDlgItemTextW(dialog, IDC_BATCH_PROFILE_NAME,
                        from_utf8(state->current_choice.display_name).c_str());
        SetDlgItemTextW(dialog, IDC_BATCH_DESTINATION,
                        from_utf8(state->current_choice.destination_root).c_str());
        SetDlgItemTextW(dialog, IDC_BATCH_PATTERN,
                        from_utf8(state->current_choice.titleformat_expression).c_str());
        update_table(*state);
        update_metadata_table(*state);
        show_preview_page(dialog, *state, true);
        state->dialog = dialog;
        capture_resize_layout(*state);
        return TRUE;
        } catch (const std::exception&) {
            MessageBoxW(dialog, L"Unable to initialize the batch preview table.",
                        L"Prepare Tracks", MB_OK | MB_ICONERROR);
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        }
    }

    auto* state = reinterpret_cast<PreviewState*>(
        GetWindowLongPtrW(dialog, GWLP_USERDATA));
    if (!state) return FALSE;

    if (message == WM_GETMINMAXINFO && state->initial_window_width > 0) {
        auto* limits = reinterpret_cast<MINMAXINFO*>(lp);
        limits->ptMinTrackSize.x = state->initial_window_width;
        limits->ptMinTrackSize.y = state->initial_window_height;
        return TRUE;
    }
    if (message == WM_SIZE && state->initial_client_width > 0) {
        resize_batch_dialog(*state, LOWORD(lp), HIWORD(lp));
        return TRUE;
    }

    if (message == WM_NOTIFY) {
        const auto* header = reinterpret_cast<const NMHDR*>(lp);
        if (header && header->idFrom == IDC_BATCH_LIST &&
            header->code == LVN_COLUMNCLICK) {
            const auto* click = reinterpret_cast<const NMLISTVIEW*>(lp);
            if (click->iSubItem >= 0 &&
                click->iSubItem < djmeta::kBatchPreviewColumnCount) {
                if (state->layout.sort_column == click->iSubItem)
                    state->layout.sort_descending = !state->layout.sort_descending;
                else {
                    state->layout.sort_column = click->iSubItem;
                    state->layout.sort_descending = false;
                }
                update_table(*state); // preserves selected underlying row IDs
                show_sort_arrow(*state);
            }
            return TRUE;
        }
        if (header && header->idFrom == IDC_BATCH_LIST &&
            header->code == LVN_GETDISPINFOW) {
            auto* info = reinterpret_cast<NMLVDISPINFOW*>(lp);
            if ((info->item.mask & LVIF_TEXT) != 0 && info->item.iItem >= 0) {
                const auto view_index = static_cast<std::size_t>(info->item.iItem);
                if (view_index < state->view_order.size())
                    state->cell_buffer = cell_text(*state,
                        state->view_order[view_index], info->item.iSubItem);
                else
                    state->cell_buffer.clear();
                info->item.pszText = state->cell_buffer.data();
            }
            return TRUE;
        }
    }

    if (message == WM_CONTEXTMENU &&
        reinterpret_cast<HWND>(wp) == ListView_GetHeader(state->list)) {
        // Compatibility fallback if the control forwards header events.
        show_column_menu(dialog, *state, lp);
        return TRUE;
    }
    if (message == WM_DESTROY) {
        // UI layout is independent of preview Cancel/Close. Save only display
        // preferences, never route edits, media metadata or file operations.
        try {
            capture_column_layout(*state);
            store_batch_table_layout(state->layout);
        } catch (const std::exception&) {
            // Invalid profile display state is non-critical; retain defaults.
        }
        return FALSE;
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
        state.entries = capture_preview(handles, initial_choice, state.analyses);
        state.metadata_rows = djmeta::describe_metadata_diffs(state.analyses);
        for (const auto& item : state.entries)
            state.source_labels.push_back(item.input.source_path);
        update_table(state);
        update_metadata_table(state);

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
