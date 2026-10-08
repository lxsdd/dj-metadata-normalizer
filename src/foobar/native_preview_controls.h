#pragma once

// SDK-independent native Win32 layout adapter shared by production dialogs
// and resource-backed runtime tests. IDs come from the real dialog resource.
#include "resource.h"
#include <windows.h>

namespace djmeta_foobar {

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
