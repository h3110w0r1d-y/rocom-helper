#pragma once

#include <QByteArray>
#include <QString>

namespace app {

struct RuntimeContext {
    QByteArray seed;

    bool isValid() const;
    QString startupDisclaimer() const;
};

RuntimeContext makeRuntimeContext(const QByteArray &seed);

} // namespace app
