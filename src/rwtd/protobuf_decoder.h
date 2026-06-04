#pragma once

#include "rwtd_types.h"

#include <QHash>
#include <QSet>

#include <memory>

namespace google::protobuf {
class DescriptorPool;
class DynamicMessageFactory;
} // namespace google::protobuf

namespace rwtd {

class DynamicProtobufDecoder {
public:
    DynamicProtobufDecoder();
    ~DynamicProtobufDecoder();

    bool load(const QString &dataDir);
    bool isAvailable() const;
    QString errorString() const;
    QString messageNameForOpcode(quint32 opcode) const;
    QSet<quint32> knownOpcodes() const;

    bool decode(quint32 opcode, const QByteArray &payload, QJsonObject *outObject) const;

private:
    bool loadOpcodeMap(const QString &path);
    bool loadDescriptorSet(const QString &path);

    QHash<quint32, QString> m_opcodeToMessageName;
    QString m_error;
    std::unique_ptr<google::protobuf::DescriptorPool> m_pool;
    std::unique_ptr<google::protobuf::DynamicMessageFactory> m_factory;
};

} // namespace rwtd

