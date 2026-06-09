#pragma once

#include <QWidget>

namespace app {

Qt::WindowFlags closeOnlyWindowFlags(bool staysOnTop = false);
void setCloseOnlyWindowControls(QWidget *window, bool staysOnTop = false);

} // namespace app
