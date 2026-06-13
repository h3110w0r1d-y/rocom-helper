#include "win_frameless_decorations.h"

#include <QWidget>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif
#endif

namespace app::overlay {

#ifdef Q_OS_WIN
LRESULT hitTestResizeBorder(HWND hwnd, LPARAM lParam, int borderWidth)
{
    if (hwnd == nullptr || borderWidth <= 0) {
        return HTCLIENT;
    }

    RECT windowRect{};
    if (!GetWindowRect(hwnd, &windowRect)) {
        return HTCLIENT;
    }

    const LONG x = GET_X_LPARAM(lParam);
    const LONG y = GET_Y_LPARAM(lParam);

    const bool left = x < windowRect.left + borderWidth;
    const bool right = x >= windowRect.right - borderWidth;
    const bool top = y < windowRect.top + borderWidth;
    const bool bottom = y >= windowRect.bottom - borderWidth;

    if (left && top) {
        return HTTOPLEFT;
    }
    if (right && top) {
        return HTTOPRIGHT;
    }
    if (left && bottom) {
        return HTBOTTOMLEFT;
    }
    if (right && bottom) {
        return HTBOTTOMRIGHT;
    }
    if (left) {
        return HTLEFT;
    }
    if (right) {
        return HTRIGHT;
    }
    if (top) {
        return HTTOP;
    }
    if (bottom) {
        return HTBOTTOM;
    }

    return HTCLIENT;
}
#endif

void applyWindowRoundedCorners(QWidget *window)
{
    if (window == nullptr) {
        return;
    }

#ifdef Q_OS_WIN
    const WId windowId = window->winId();
    if (windowId == 0) {
        return;
    }

    HWND hwnd = reinterpret_cast<HWND>(windowId);
    const int roundPreference = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &roundPreference, sizeof(roundPreference));
#else
    Q_UNUSED(window);
#endif
}

void refreshWindowChrome(QWidget *window)
{
    applyWindowRoundedCorners(window);
}

} // namespace app::overlay
