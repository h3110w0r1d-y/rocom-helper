#pragma once

#include <QByteArray>
#include <QString>

namespace app {

struct RuntimeContext {
    QByteArray seed;
    QByteArray resourceKey;
    QByteArray trafficKey;

    bool isValid() const;
    QString webResourceRoot() const;
    QString webIndexPath() const;
    QString trafficSchemaRoot() const;
    QString startupDisclaimer() const;
};

RuntimeContext makeRuntimeContext(const QByteArray &seed);
QString localPetFilterUrl(int port);

} // namespace app
