#include "key_cache.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <utility>

namespace rwtd {
namespace {

constexpr qsizetype MaxCachedKeys = 10;

QByteArray keyFromJsonValue(const QJsonValue &value)
{
    const QByteArray key = QByteArray::fromHex(value.toString().toLatin1());
    return key.size() == 16 ? key : QByteArray();
}

} // namespace

AesKeyCache::AesKeyCache(QString path)
    : m_path(std::move(path))
{
    load();
}

QByteArray AesKeyCache::keyForFlow(const QString &flowId) const
{
    for (const Entry &entry : m_entries) {
        if (entry.flowId == flowId && entry.key.size() == 16) {
            return entry.key;
        }
    }
    if (m_legacyLastKey.size() == 16) {
        return m_legacyLastKey;
    }

    return m_entries.isEmpty() || m_entries.constFirst().key.size() != 16
        ? QByteArray()
        : m_entries.constFirst().key;
}

void AesKeyCache::rememberKey(const QString &flowId, const QByteArray &key)
{
    if (m_path.isEmpty() || key.size() != 16) {
        return;
    }
    m_legacyLastKey.clear();

    for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
        if (it->flowId == flowId) {
            if (it == m_entries.begin() && it->key == key) {
                return;
            }
            Entry entry{flowId, key};
            m_entries.erase(it);
            m_entries.prepend(entry);
            trim();
            save();
            return;
        }
    }

    m_entries.prepend({flowId, key});
    trim();
    save();
}

void AesKeyCache::load()
{
    m_entries.clear();
    m_legacyLastKey.clear();
    if (m_path.isEmpty()) {
        return;
    }

    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isArray()) {
        const QJsonArray array = doc.array();
        for (const QJsonValue &value : array) {
            if (!value.isObject()) {
                continue;
            }
            const QJsonObject object = value.toObject();
            const QString flowId = object.value(QStringLiteral("flow_id")).toString().trimmed();
            const QByteArray key = keyFromJsonValue(object.value(QStringLiteral("key")));
            if (!flowId.isEmpty() && key.size() == 16) {
                m_entries.append({flowId, key});
            }
        }
        trim();
        return;
    }

    if (doc.isObject()) {
        const QJsonObject object = doc.object();
        m_legacyLastKey = keyFromJsonValue(object.value(QStringLiteral("_last")));

        for (auto it = object.begin(); it != object.end(); ++it) {
            if (it.key() == QStringLiteral("_last")) {
                continue;
            }
            const QByteArray key = keyFromJsonValue(it.value());
            if (key.size() == 16) {
                m_entries.append({it.key(), key});
            }
        }
        trim();
    }
}

void AesKeyCache::save() const
{
    const QFileInfo info(m_path);
    if (!QDir().mkpath(info.absolutePath())) {
        return;
    }

    QJsonArray array;
    for (const Entry &entry : m_entries) {
        if (entry.flowId.isEmpty() || entry.key.size() != 16) {
            continue;
        }
        array.append(QJsonObject{
            {QStringLiteral("flow_id"), entry.flowId},
            {QStringLiteral("key"), QString::fromLatin1(entry.key.toHex())},
        });
    }

    QFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
}

void AesKeyCache::trim()
{
    while (m_entries.size() > MaxCachedKeys) {
        m_entries.removeLast();
    }
}

} // namespace rwtd
