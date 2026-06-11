#pragma once

#include <QByteArray>
#include <QString>

namespace rwtd {

class AesKeyCache {
public:
    explicit AesKeyCache(QString path = {});

    QByteArray keyForFlow(const QString &flowId) const;
    void rememberKey(const QString &flowId, const QByteArray &key);

private:
    struct Entry {
        QString flowId;
        QByteArray key;
    };

    void load();
    void save() const;

    QString m_path;
    Entry m_entry;
};

} // namespace rwtd
