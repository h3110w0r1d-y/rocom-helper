#include "app_version.h"

#include "app_version_generated.h"

namespace app {

QString appVersionString()
{
    return QStringLiteral(ROCO_APP_VERSION_STRING);
}

QString appVersionDisplay()
{
    return QStringLiteral("v%1").arg(appVersionString());
}

QString appWindowTitle()
{
    return QStringLiteral("洛克助手插件 %1").arg(appVersionDisplay());
}

} // namespace app
