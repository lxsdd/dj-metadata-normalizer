#include "../src/foobar/native_preview_controls.h"
#include "../src/foobar/legacy_routing_profiles.h"

#include <commctrl.h>
#include <cstdlib>
#include <cwchar>
#include <iostream>

namespace {
void check(bool value, const char* message) {
    if (!value) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}
INT_PTR CALLBACK test_dialog_proc(HWND, UINT, WPARAM, LPARAM) {
    return FALSE;
}
bool centers_match(HWND dialog, int label_id, int control_id) {
    HWND label = GetDlgItem(dialog, label_id);
    HWND input = GetDlgItem(dialog, control_id);
    if (!label || !input) return false;
    RECT a{}, b{};
    if (!GetWindowRect(label, &a) || !GetWindowRect(input, &b))
        return false;
    COMBOBOXINFO info{};
    info.cbSize = sizeof(info);
    if (GetComboBoxInfo(input, &info)) b = info.rcItem;
    const int offset = (a.top + a.bottom) - (b.top + b.bottom);
    return offset >= -2 && offset <= 2;
}
}

int main() {
    INITCOMMONCONTROLSEX controls{sizeof(controls),
                                  ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES};
    check(InitCommonControlsEx(&controls) != FALSE,
          "native listview and tabs initialization");
    HWND dialog = CreateDialogParamW(GetModuleHandleW(nullptr),
        MAKEINTRESOURCEW(IDD_BATCH_PREVIEW), nullptr, test_dialog_proc, 0);
    check(dialog != nullptr, "actual production dialog template instantiation");

    HWND combo = GetDlgItem(dialog, IDC_BATCH_PROFILE_PICKER);
    check(combo && !GetDlgItem(dialog, IDC_BATCH_PROFILE_NAME),
          "single routing profile picker; no duplicate name editor");
    const auto style = static_cast<DWORD>(GetWindowLongPtrW(combo, GWL_STYLE));
    check((style & 0x3u) == CBS_DROPDOWN,
          "routing profile picker is editable with real Win32 control");
    check(GetDlgItem(dialog, IDC_BATCH_DESTINATION) &&
          GetDlgItem(dialog, IDC_BATCH_PATTERN) &&
          GetDlgItem(dialog, IDC_BATCH_ROUTE_LABEL),
          "route profile is accompanied by destination and naming expression");

    const wchar_t* names[] = {L"Singles", L"Alben", L"Livesets"};
    for (const auto* name : names)
        check(SendMessageW(combo, CB_ADDSTRING, 0,
              reinterpret_cast<LPARAM>(name)) != CB_ERR,
              "historical route name accepted");
    check(SendMessageW(combo, CB_SETCURSEL, 1, 0) == 1,
          "historical preset can be selected");
    wchar_t value[128]{};
    GetWindowTextW(combo, value, 128);
    check(std::wcscmp(value, L"Alben") == 0,
          "exact user-owned preset name preserved");
    check(SetWindowTextW(combo, L"My DJ profile") != FALSE,
          "custom profile name editable");
    GetWindowTextW(combo, value, 128);
    check(std::wcscmp(value, L"My DJ profile") == 0,
          "custom profile text survives in the actual control");

    HWND review_scope = GetDlgItem(dialog, IDC_METADATA_REVIEW_SCOPE);
    HWND manual = GetDlgItem(dialog, IDC_METADATA_MANUAL_INPUT);
    check(review_scope && manual &&
          (static_cast<DWORD>(GetWindowLongPtrW(review_scope, GWL_STYLE)) & 0x3u) ==
              CBS_DROPDOWNLIST,
          "review scope is the real non-editable Windows selection control");
    check(SetWindowTextW(manual, L"Custom review value") != FALSE,
          "manual value control accepts editor text");
    wchar_t manual_text[128]{};
    GetWindowTextW(manual, manual_text, 128);
    check(std::wcscmp(manual_text, L"Custom review value") == 0,
          "manual value roundtrip uses the actual Win32 control");
    RECT client{};
    GetClientRect(dialog, &client);
    for (int id : {IDC_METADATA_ACCEPT, IDC_METADATA_REJECT,
                   IDC_METADATA_RESET, IDC_METADATA_USE_VALUE}) {
        HWND button = GetDlgItem(dialog, id);
        check(button != nullptr, "production review action exists");
        RECT bounds{};
        GetWindowRect(button, &bounds);
        MapWindowPoints(HWND_DESKTOP, dialog,
                        reinterpret_cast<POINT*>(&bounds), 2);
        check(bounds.left >= 0 && bounds.right <= client.right &&
              bounds.top >= 0 && bounds.bottom <= client.bottom,
              "review actions fit inside minimum-sized native dialog");
    }

    for (int id : {IDC_METADATA_TRACK_LIST, IDC_METADATA_LIST}) {
        HWND list = GetDlgItem(dialog, id);
        check(list != nullptr, "actual master/detail list control present");
        const auto flags = static_cast<DWORD>(GetWindowLongPtrW(list, GWL_STYLE));
        check((flags & LVS_TYPEMASK) == LVS_REPORT &&
              (flags & LVS_OWNERDATA) != 0,
              "master/detail controls support virtualized report view");
    }
    HWND track_filter = GetDlgItem(dialog, IDC_METADATA_TRACK_FILTER);
    check(track_filter != nullptr &&
          (static_cast<DWORD>(GetWindowLongPtrW(track_filter, GWL_STYLE)) & 0x3u) == CBS_DROPDOWNLIST,
          "real native status filter is a non-editable combo");
    djmeta_foobar::align_native_preview_form(dialog);
    const int rows[][2] = {
        {IDC_METADATA_FILTER_LABEL, IDC_METADATA_FILTER},
        {IDC_METADATA_TRACK_FILTER_LABEL, IDC_METADATA_TRACK_FILTER},
        {IDC_BATCH_ROUTE_LABEL, IDC_BATCH_PROFILE_PICKER},
        {IDC_BATCH_DEST_LABEL, IDC_BATCH_DESTINATION},
        {IDC_BATCH_PATTERN_LABEL, IDC_BATCH_PATTERN},
    };
    for (const auto& row : rows)
        check(centers_match(dialog, row[0], row[1]),
              "labels align to text portion of actual Windows input controls");
    check(GetDlgItem(dialog, IDC_BATCH_APPLY_ALL) &&
          GetDlgItem(dialog, IDC_BATCH_APPLY_SELECTED),
          "preview actions exist in production resource");

    DestroyWindow(dialog);
    std::cout << "PASS: real Win32 review scope, manual input, action bounds; "
                 "native preview resource, editable profile, "
                 "virtual master/detail controls and shared label alignment\n";
    return 0;
}
