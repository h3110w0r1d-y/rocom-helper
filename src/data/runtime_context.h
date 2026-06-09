#pragma once

#include <QByteArray>
#include <QString>

namespace app {

struct RuntimeContext {
    QByteArray seed;
    QByteArray httpKey;
    QByteArray resourceKey;
    QByteArray trafficKey;

    bool isValid() const;
    QString webResourceRoot() const;
    QString webIndexPath() const;
    QString petFilterUrl() const;
    QString trafficSchemaRoot() const;
};

RuntimeContext makeRuntimeContext(const QByteArray &seed);

} // namespace app
