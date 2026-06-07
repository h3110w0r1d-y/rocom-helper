#pragma once

#include <QList>
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
    void trim();

    QString m_path;
    QList<Entry> m_entries;
    QByteArray m_legacyLastKey;
};

} // namespace rwtd
