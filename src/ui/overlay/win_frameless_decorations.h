#pragma once

class QWidget;

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace app::overlay {

#ifdef Q_OS_WIN
LRESULT hitTestResizeBorder(HWND hwnd, LPARAM lParam, int borderWidth = 8);
#endif

void applyWindowRoundedCorners(QWidget *window);
void refreshWindowChrome(QWidget *window);

} // namespace app::overlay
