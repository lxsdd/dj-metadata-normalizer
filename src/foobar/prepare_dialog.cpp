#include "stdafx.h"

#include <SDK/coreDarkMode.h>

#include "legacy_routing_profiles.h"
#include "prepare_dialog.h"
#include "batch_preview_dialog.h"
#include "resource.h"
#include "routing_preview.h"

#include <limits>
#include <string>

namespace djmeta_foobar {
namespace {

struct PrepareDialogState {
    RoutePreviewChoice choice;
    bool approved = false;
    fb2k::CCoreDarkModeHooks dark_mode;
};

std::wstring from_utf8(const char* input) {
    if (input == nullptr || *input == '\0') return {};
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                          input, -1, nullptr, 0);
    if (count <= 0) throw std::runtime_error("Unable to display UTF-8 text.");
    std::wstring text(static_cast<std::size_t>(count), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                            input, -1, text.data(), count) != count)
        throw std::runtime_error("UTF-8-Konvertierung fehlgeschlagen.");
    text.pop_back(); // terminal NUL
    return text;
}

std::string to_utf8(const std::wstring& input) {
    if (input.empty()) return {};
    if (input.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
        throw std::invalid_argument("Input is too long.");
    const int len = static_cast<int>(input.size());
    const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                          input.data(), len, nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) throw std::invalid_argument("Invalid characters in input.");
    std::string result(static_cast<std::size_t>(bytes), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                            input.data(), len, result.data(), bytes,
                            nullptr, nullptr) != bytes)
        throw std::runtime_error("Text conversion failed.");
    return result;
}

std::wstring read_text(HWND dialog, int id) {
    HWND control = GetDlgItem(dialog, id);
    if (!control) throw std::runtime_error("Input control not found.");
    const int len = GetWindowTextLengthW(control);
    if (len < 0 || len > 16384) throw std::invalid_argument("Input is too long.");
    std::wstring text(static_cast<std::size_t>(len) + 1, L'\0');
    const int received = GetWindowTextW(control, text.data(), len + 1);
    text.resize(static_cast<std::size_t>(received));
    return text;
}

bool invalid_control_chars(const std::wstring& value) {
    for (const wchar_t ch : value)
        if (ch < 0x20 || ch == 0x7f) return true;
    return false;
}

void populate_selection(HWND dialog, int selection) {
    if (selection >= 0 && static_cast<std::size_t>(selection) < legacy_move_route_count) {
        const LegacyMoveRoute& item = legacy_move_routes[selection];
        const auto caption = from_utf8(item.name);
        const auto target = from_utf8(item.destination_root);
        const auto format = from_utf8(item.foobar_titleformat);
        SetDlgItemTextW(dialog, IDC_PREPARE_NAME, caption.c_str());
        SetDlgItemTextW(dialog, IDC_PREPARE_DESTINATION, target.c_str());
        SetDlgItemTextW(dialog, IDC_PREPARE_PATTERN, format.c_str());
    } else {
        SetDlgItemTextW(dialog, IDC_PREPARE_NAME, L"Custom");
        SetDlgItemTextW(dialog, IDC_PREPARE_DESTINATION, L"");
        SetDlgItemTextW(dialog, IDC_PREPARE_PATTERN, L"");
    }
}

void accept_choice(HWND dialog, PrepareDialogState& state) {
    const std::wstring name = read_text(dialog, IDC_PREPARE_NAME);
    const std::wstring destination = read_text(dialog, IDC_PREPARE_DESTINATION);
    const std::wstring pattern = read_text(dialog, IDC_PREPARE_PATTERN);

    if (name.empty() || destination.empty() || pattern.empty() ||
        invalid_control_chars(name) || invalid_control_chars(destination) ||
        invalid_control_chars(pattern)) {
        MessageBoxW(dialog,
            L"Profile name, destination and naming expression are required "
            L"and may not contain control characters.",
            L"DJ Metadata Normalizer", MB_OK | MB_ICONWARNING);
        return;
    }

    state.choice.display_name = to_utf8(name);
    state.choice.destination_root = to_utf8(destination);
    state.choice.titleformat_expression = to_utf8(pattern);
    state.approved = true;
    EndDialog(dialog, IDOK);
}

INT_PTR CALLBACK prepare_dialog_proc(HWND dialog, UINT message, WPARAM wp, LPARAM lp) {
    if (message == WM_INITDIALOG) {
        auto* state = reinterpret_cast<PrepareDialogState*>(lp);
        SetWindowLongPtrW(dialog, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        state->dark_mode.AddDialogWithControls(dialog);
        SendDlgItemMessageW(dialog, IDC_PREPARE_PROFILE, CB_ADDSTRING, 0,
                            reinterpret_cast<LPARAM>(L"Singles"));
        SendDlgItemMessageW(dialog, IDC_PREPARE_PROFILE, CB_ADDSTRING, 0,
                            reinterpret_cast<LPARAM>(L"Alben"));
        SendDlgItemMessageW(dialog, IDC_PREPARE_PROFILE, CB_ADDSTRING, 0,
                            reinterpret_cast<LPARAM>(L"Livesets"));
        SendDlgItemMessageW(dialog, IDC_PREPARE_PROFILE, CB_ADDSTRING, 0,
                            reinterpret_cast<LPARAM>(L"Custom"));
        SendDlgItemMessageW(dialog, IDC_PREPARE_PROFILE, CB_SETCURSEL, 0, 0);
        for (const int id : {IDC_PREPARE_NAME, IDC_PREPARE_DESTINATION, IDC_PREPARE_PATTERN})
            SendDlgItemMessageW(dialog, id, EM_LIMITTEXT, 16384, 0);
        populate_selection(dialog, 0);
        return TRUE;
    }

    if (message == 0x02E0u /* WM_DPICHANGED */) {
        const RECT* suggestion = reinterpret_cast<const RECT*>(lp);
        if (suggestion) {
            SetWindowPos(dialog, nullptr, suggestion->left, suggestion->top,
                         suggestion->right - suggestion->left,
                         suggestion->bottom - suggestion->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return TRUE;
    }

    if (message != WM_COMMAND) return FALSE;
    auto* state = reinterpret_cast<PrepareDialogState*>(
        GetWindowLongPtrW(dialog, GWLP_USERDATA));
    if (!state) return FALSE;

    const int id = LOWORD(wp);
    try {
        if (id == IDC_PREPARE_PROFILE && HIWORD(wp) == CBN_SELCHANGE) {
            const auto current = SendDlgItemMessageW(
                dialog, IDC_PREPARE_PROFILE, CB_GETCURSEL, 0, 0);
            populate_selection(dialog, static_cast<int>(current));
            return TRUE;
        }
        if (id == IDOK && HIWORD(wp) == BN_CLICKED) {
            accept_choice(dialog, *state);
            return TRUE;
        }
        if (id == IDCANCEL) {
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        }
    } catch (const std::exception&) {
        MessageBoxW(dialog, L"Unable to apply the dialog input.",
                    L"DJ Metadata Normalizer", MB_OK | MB_ICONERROR);
        return TRUE;
    }
    return FALSE;
}

} // namespace

void show_prepare_tracks_dialog(const metadb_handle_list& handles) {
    PrepareDialogState state;
    const INT_PTR status = DialogBoxParamW(
        core_api::get_my_instance(),
        MAKEINTRESOURCEW(IDD_PREPARE_TRACKS),
        core_api::get_main_window(),
        prepare_dialog_proc,
        reinterpret_cast<LPARAM>(&state));
    if (status == -1) {
        popup_message::g_show(
            "Unable to open the Prepare Tracks dialog. "
            "Nothing was changed.",
            "DJ Metadata Normalizer");
        return;
    }
    if (status == IDOK && state.approved)
        show_batch_preview_dialog(handles, state.choice);
}

} // namespace djmeta_foobar
