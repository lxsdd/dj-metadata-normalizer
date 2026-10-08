#pragma once

// Win32 ListView adapter shared by the production foobar preview and
// real-resource Windows GUI tests. Stores widths in logical 96-DPI units;
// the settings themselves live in foobar cfg_string (no registry/sidecar).
#include "djmeta/review_grid_layout.h"
#include <windows.h>
#include <commctrl.h>
#include <algorithm>
#include <array>
#include <cstddef>

namespace djmeta_foobar {

template<std::size_t N>
void show_review_grid_sort_arrow(HWND list, const djmeta::ReviewGridLayout<N>& layout) {
    const HWND header = ListView_GetHeader(list);
    if (!header) return;
    for (std::size_t i = 0; i < N; ++i) {
        HDITEMW item{};
        item.mask = HDI_FORMAT;
        if (!SendMessageW(header, HDM_GETITEMW, static_cast<WPARAM>(i),
                          reinterpret_cast<LPARAM>(&item))) continue;
        item.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        if (static_cast<int>(i) == layout.sort_column)
            item.fmt |= layout.sort_descending ? HDF_SORTDOWN : HDF_SORTUP;
        SendMessageW(header, HDM_SETITEMW, static_cast<WPARAM>(i),
                     reinterpret_cast<LPARAM>(&item));
    }
}

template<std::size_t N>
void apply_review_grid_controls(HWND list, const djmeta::ReviewGridLayout<N>& layout,
                                int dpi) {
    if (!list || !djmeta::valid_review_grid_layout(layout)) return;
    const int scale = dpi > 0 ? dpi : 96;
    for (std::size_t i = 0; i < N; ++i) {
        const int pixels = (layout.visible_mask & (1u << i))
            ? MulDiv(layout.widths[i], scale, 96) : 0;
        ListView_SetColumnWidth(list, static_cast<int>(i), pixels);
    }
    ListView_SetColumnOrderArray(list, static_cast<int>(N), layout.order.data());
    show_review_grid_sort_arrow(list, layout);
}

template<std::size_t N>
void capture_review_grid_controls(HWND list, djmeta::ReviewGridLayout<N>& layout,
                                  int dpi) {
    if (!list) return;
    std::array<int, N> order{};
    if (ListView_GetColumnOrderArray(list, static_cast<int>(N), order.data()))
        layout.order = order;
    const int scale = dpi > 0 ? dpi : 96;
    for (std::size_t i = 0; i < N; ++i) {
        // Zero width represents hidden, not a new remembered width.
        if ((layout.visible_mask & (1u << i)) == 0) continue;
        const int actual = ListView_GetColumnWidth(list, static_cast<int>(i));
        if (actual > 0)
            layout.widths[i] = (std::max)(48, (std::min)(3000,
                                     MulDiv(actual, 96, scale)));
    }
}

} // namespace djmeta_foobar
