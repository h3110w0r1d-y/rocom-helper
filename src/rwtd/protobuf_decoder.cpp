#include "protobuf_decoder.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/json/json.h>
#include <google/protobuf/message.h>

namespace rwtd {

DynamicProtobufDecoder::DynamicProtobufDecoder() = default;
DynamicProtobufDecoder::~DynamicProtobufDecoder() = default;

bool DynamicProtobufDecoder::load(const QString &dataDir)
{
    m_error.clear();
    m_opcodeToMessageName.clear();
    m_factory.reset();
    m_pool.reset();

    if (!loadOpcodeMap(dataDir + QStringLiteral("/proto.json"))) {
        return false;
    }
    if (!loadDescriptorSet(dataDir + QStringLiteral("/all.pb"))) {
        return false;
    }
    return true;
}

bool DynamicProtobufDecoder::isAvailable() const
{
    return m_pool != nullptr && m_factory != nullptr && !m_opcodeToMessageName.isEmpty();
}

QString DynamicProtobufDecoder::errorString() const
{
    return m_error;
}

QString DynamicProtobufDecoder::messageNameForOpcode(quint32 opcode) const
{
    return m_opcodeToMessageName.value(opcode);
}

QSet<quint32> DynamicProtobufDecoder::knownOpcodes() const
{
    QSet<quint32> out;
    for (auto it = m_opcodeToMessageName.begin(); it != m_opcodeToMessageName.end(); ++it) {
        out.insert(it.key());
    }
    return out;
}

bool DynamicProtobufDecoder::decode(quint32 opcode, const QByteArray &payload, QJsonObject *outObject) const
{
    if (!isAvailable() || outObject == nullptr) {
        return false;
    }

    QString fullName = m_opcodeToMessageName.value(opcode);
    if (fullName.startsWith(QLatin1Char('.'))) {
        fullName.remove(0, 1);
    }
    if (fullName.isEmpty()) {
        return false;
    }

    const google::protobuf::Descriptor *descriptor = m_pool->FindMessageTypeByName(fullName.toStdString());
    if (descriptor == nullptr) {
        return false;
    }

    const google::protobuf::Message *prototype = m_factory->GetPrototype(descriptor);
    if (prototype == nullptr) {
        return false;
    }

    std::unique_ptr<google::protobuf::Message> message(prototype->New());
    if (!message->ParseFromArray(payload.constData(), payload.size())) {
        return false;
    }

    google::protobuf::json::PrintOptions options;
    options.preserve_proto_field_names = true;
    options.always_print_enums_as_ints = true;

    std::string json;
    const auto status = google::protobuf::json::MessageToJsonString(*message, &json, options);
    if (!status.ok()) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(json));
    if (!doc.isObject()) {
        return false;
    }

    *outObject = doc.object();
    return true;
}

bool DynamicProtobufDecoder::loadOpcodeMap(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_error = QStringLiteral("无法读取 proto.json: %1").arg(path);
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        m_error = QStringLiteral("proto.json 不是 JSON 对象: %1").arg(path);
        return false;
    }

    const QJsonObject object = doc.object();
    for (auto it = object.begin(); it != object.end(); ++it) {
        bool ok = false;
        const quint32 opcode = it.key().toUInt(&ok);
        if (ok && it.value().isString()) {
            m_opcodeToMessageName.insert(opcode, it.value().toString());
        }
    }

    if (m_opcodeToMessageName.isEmpty()) {
        m_error = QStringLiteral("proto.json 没有可用 opcode 映射: %1").arg(path);
        return false;
    }
    return true;
}

bool DynamicProtobufDecoder::loadDescriptorSet(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_error = QStringLiteral("无法读取 all.pb: %1").arg(path);
        return false;
    }

    const QByteArray raw = file.readAll();
    google::protobuf::FileDescriptorSet descriptorSet;
    if (!descriptorSet.ParseFromArray(raw.constData(), raw.size())) {
        m_error = QStringLiteral("all.pb 不是有效 FileDescriptorSet: %1").arg(path);
        return false;
    }

    m_pool = std::make_unique<google::protobuf::DescriptorPool>(google::protobuf::DescriptorPool::generated_pool());

    QList<int> pending;
    for (int i = 0; i < descriptorSet.file_size(); ++i) {
        pending.append(i);
    }

    QString lastFailure;
    while (!pending.isEmpty()) {
        QList<int> nextPending;
        int addedCount = 0;

        for (const int index : pending) {
            const google::protobuf::FileDescriptorProto &fileProto = descriptorSet.file(index);
            if (m_pool->BuildFile(fileProto) != nullptr) {
                ++addedCount;
            } else {
                lastFailure = QString::fromStdString(fileProto.name());
                nextPending.append(index);
            }
        }

        if (addedCount == 0) {
            m_error = QStringLiteral("无法加载 protobuf 描述依赖，最后失败文件: %1").arg(lastFailure);
            m_pool.reset();
            return false;
        }
        pending = nextPending;
    }

    m_factory = std::make_unique<google::protobuf::DynamicMessageFactory>(m_pool.get());
    return true;
}

} // namespace rwtd

