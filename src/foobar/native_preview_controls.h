#pragma once

// SDK-independent native Win32 layout adapter shared by production dialogs
// and resource-backed runtime tests. IDs come from the real dialog resource.
#include "resource.h"
#include <windows.h>
#include <commctrl.h>
#include <cstddef>
#include <set>
#include <vector>

namespace djmeta_foobar {


/*
 * This small Win32 event/identity adapter is called by the production
 * dialog procedure as well as the resource-backed Windows smoke test.
 * All identities refer to underlying sources, never to sorted UI rows.
 * No host SDK or filesystem write API is needed here.
 */
enum class PreviewCommand {
    None, Accept, Reject, Reset, ManualValue, TrackFilterChanged, FocusFilterChanged
};

inline PreviewCommand native_preview_command(UINT message, WPARAM wp) {
    if (message != WM_COMMAND) return PreviewCommand::None;
    if (HIWORD(wp) == BN_CLICKED) {
        switch (LOWORD(wp)) {
        case IDC_METADATA_ACCEPT: return PreviewCommand::Accept;
        case IDC_METADATA_REJECT: return PreviewCommand::Reject;
        case IDC_METADATA_RESET: return PreviewCommand::Reset;
        case IDC_METADATA_USE_VALUE: return PreviewCommand::ManualValue;
        default: break;
        }
    }
    if (HIWORD(wp) == CBN_SELCHANGE) {
        switch (LOWORD(wp)) {
        case IDC_METADATA_TRACK_FILTER: return PreviewCommand::TrackFilterChanged;
        case IDC_METADATA_FILTER: return PreviewCommand::FocusFilterChanged;
        default: break;
        }
    }
    return PreviewCommand::None;
}

inline std::vector<std::size_t> selected_native_view_ids(
    HWND list, const std::vector<std::size_t>& view_order) {
    std::vector<std::size_t> ids;
    if (!list) return ids;
    int row = -1;
    while ((row = ListView_GetNextItem(list, row, LVNI_SELECTED)) >= 0) {
        if (static_cast<std::size_t>(row) < view_order.size())
            ids.push_back(view_order[static_cast<std::size_t>(row)]);
    }
    return ids;
}

// Preserve multiselection and focus after sorting/filtering a virtual grid.
// The caller must supply the new item count before restoring selections.
inline void restore_native_view_selection(
    HWND list, const std::vector<std::size_t>& view_order,
    const std::vector<std::size_t>& selected_ids, std::size_t focused_id) {
    if (!list) return;
    const std::set<std::size_t> selected(selected_ids.begin(), selected_ids.end());
    ListView_SetItemState(list, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    for (std::size_t row = 0; row < view_order.size(); ++row) {
        const bool chosen = selected.count(view_order[row]) != 0;
        if (!chosen && view_order[row] != focused_id) continue;
        const UINT state = (chosen ? LVIS_SELECTED : 0u) |
                           (view_order[row] == focused_id ? LVIS_FOCUSED : 0u);
        ListView_SetItemState(list, static_cast<int>(row), state,
                              LVIS_SELECTED | LVIS_FOCUSED);
    }
}

inline bool align_label_to_input(HWND dialog, int label_id, int input_id) {
    const HWND label = GetDlgItem(dialog, label_id);
    const HWND input = GetDlgItem(dialog, input_id);
    if (!label || !input) return false;
    RECT label_rect{}, input_rect{};
    if (!GetWindowRect(label, &label_rect) ||
        !GetWindowRect(input, &input_rect)) return false;

    // A combo box's RC height includes its drop-down list. Use the editable
    // field's actual screen rectangle, not the expanded list rectangle.
    COMBOBOXINFO combo{};
    combo.cbSize = sizeof(combo);
    if (GetComboBoxInfo(input, &combo)) input_rect = combo.rcItem;

    MapWindowPoints(HWND_DESKTOP, dialog,
                    reinterpret_cast<POINT*>(&label_rect), 2);
    MapWindowPoints(HWND_DESKTOP, dialog,
                    reinterpret_cast<POINT*>(&input_rect), 2);
    const int label_height = label_rect.bottom - label_rect.top;
    const int target_y = input_rect.top +
        ((input_rect.bottom - input_rect.top) - label_height) / 2;
    return SetWindowPos(label, nullptr, label_rect.left, target_y, 0, 0,
                        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE) != 0;
}

inline void align_native_preview_form(HWND dialog) {
    constexpr int rows[][2] = {
        {IDC_METADATA_FILTER_LABEL, IDC_METADATA_FILTER},
        {IDC_METADATA_TRACK_FILTER_LABEL, IDC_METADATA_TRACK_FILTER},
        {IDC_BATCH_ROUTE_LABEL, IDC_BATCH_PROFILE_PICKER},
        {IDC_BATCH_DEST_LABEL, IDC_BATCH_DESTINATION},
        {IDC_BATCH_PATTERN_LABEL, IDC_BATCH_PATTERN},
    };
    for (const auto& row : rows)
        align_label_to_input(dialog, row[0], row[1]);
}

} // namespace djmeta_foobar
