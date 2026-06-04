#pragma once

#include <QHash>
#include <QString>

namespace rwtd {

class AesKeyCache {
public:
    explicit AesKeyCache(QString path = {});

    QByteArray keyForFlow(const QString &flowId) const;
    void rememberKey(const QString &flowId, const QByteArray &key);

private:
    void load();
    void save() const;

    QString m_path;
    QHash<QString, QByteArray> m_keys;
};

} // namespace rwtd

