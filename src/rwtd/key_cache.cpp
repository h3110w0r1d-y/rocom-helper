#include "key_cache.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

#include <utility>

namespace rwtd {

AesKeyCache::AesKeyCache(QString path)
    : m_path(std::move(path))
{
    load();
}

QByteArray AesKeyCache::keyForFlow(const QString &flowId) const
{
    QByteArray key = m_keys.value(flowId);
    if (key.isEmpty()) {
        key = m_keys.value(QStringLiteral("_last"));
    }
    return key.size() == 16 ? key : QByteArray();
}

void AesKeyCache::rememberKey(const QString &flowId, const QByteArray &key)
{
    if (m_path.isEmpty() || key.size() != 16) {
        return;
    }
    if (m_keys.value(flowId) == key && m_keys.value(QStringLiteral("_last")) == key) {
        return;
    }

    m_keys.insert(flowId, key);
    m_keys.insert(QStringLiteral("_last"), key);
    save();
}

void AesKeyCache::load()
{
    m_keys.clear();
    if (m_path.isEmpty()) {
        return;
    }

    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return;
    }

    const QJsonObject object = doc.object();
    for (auto it = object.begin(); it != object.end(); ++it) {
        const QByteArray key = QByteArray::fromHex(it.value().toString().toLatin1());
        if (key.size() == 16) {
            m_keys.insert(it.key(), key);
        }
    }
}

void AesKeyCache::save() const
{
    const QFileInfo info(m_path);
    if (!QDir().mkpath(info.absolutePath())) {
        return;
    }

    QJsonObject object;
    for (auto it = m_keys.begin(); it != m_keys.end(); ++it) {
        object.insert(it.key(), QString::fromLatin1(it.value().toHex()));
    }

    QFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
}

} // namespace rwtd
