#include "win_mouse_passthrough.h"

#include <QWidget>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace app::overlay {

bool setMousePassthrough(QWidget *window, bool enabled)
{
    if (window == nullptr) {
        return false;
    }

#ifdef Q_OS_WIN
    const WId windowId = window->winId();
    if (windowId == 0) {
        return false;
    }

    HWND hwnd = reinterpret_cast<HWND>(windowId);
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (enabled) {
        style |= WS_EX_TRANSPARENT | WS_EX_LAYERED;
    } else {
        style &= ~(WS_EX_TRANSPARENT | WS_EX_LAYERED);
    }
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, style);
    SetWindowPos(hwnd,
                 nullptr,
                 0,
                 0,
                 0,
                 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    return true;
#else
    Q_UNUSED(enabled);
    return false;
#endif
}

} // namespace app::overlay
