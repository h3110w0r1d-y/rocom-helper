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

QByteArray keyFromJsonValue(const QJsonValue &value)
{
    const QByteArray key = QByteArray::fromHex(value.toString().toLatin1());
    return key.size() == 16 ? key : QByteArray();
}

bool entryFromJsonObject(const QJsonObject &object, QString *flowId, QByteArray *key)
{
    if (flowId == nullptr || key == nullptr) {
        return false;
    }
    const QString parsedFlowId = object.value(QStringLiteral("flow_id")).toString().trimmed();
    const QByteArray parsedKey = keyFromJsonValue(object.value(QStringLiteral("key")));
    if (parsedFlowId.isEmpty() || parsedKey.size() != 16) {
        return false;
    }
    *flowId = parsedFlowId;
    *key = parsedKey;
    return true;
}

} // namespace

AesKeyCache::AesKeyCache(QString path)
    : m_path(std::move(path))
{
    load();
}

QByteArray AesKeyCache::keyForFlow(const QString &flowId) const
{
    if (m_entry.flowId == flowId && m_entry.key.size() == 16) {
        return m_entry.key;
    }
    return QByteArray();
}

void AesKeyCache::rememberKey(const QString &flowId, const QByteArray &key)
{
    if (m_path.isEmpty() || key.size() != 16) {
        return;
    }
    if (m_entry.flowId == flowId && m_entry.key == key) {
        return;
    }

    m_entry = {flowId, key};
    save();
}

void AesKeyCache::load()
{
    m_entry = {};
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
            QString flowId;
            QByteArray key;
            if (entryFromJsonObject(value.toObject(), &flowId, &key)) {
                m_entry = {flowId, key};
                return;
            }
        }
        return;
    }

    if (doc.isObject()) {
        const QJsonObject object = doc.object();
        Entry latest;
        bool hasLatest = false;

        for (auto it = object.begin(); it != object.end(); ++it) {
            if (it.key() == QStringLiteral("_last")) {
                continue;
            }
            const QByteArray key = keyFromJsonValue(it.value());
            if (key.size() == 16) {
                latest = {it.key(), key};
                hasLatest = true;
            }
        }

        if (hasLatest) {
            m_entry = latest;
        }
    }
}

void AesKeyCache::save() const
{
    const QFileInfo info(m_path);
    if (!QDir().mkpath(info.absolutePath())) {
        return;
    }

    QJsonArray array;
    if (!m_entry.flowId.isEmpty() && m_entry.key.size() == 16) {
        array.append(QJsonObject{
            {QStringLiteral("flow_id"), m_entry.flowId},
            {QStringLiteral("key"), QString::fromLatin1(m_entry.key.toHex())},
        });
    }

    QFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
}

} // namespace rwtd
