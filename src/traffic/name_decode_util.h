#pragma once

#include <QByteArray>
#include <QString>

#include <string>

namespace app {

inline QString decodeBytesName(const QByteArray &rawBytes)
{
    if (rawBytes.isEmpty()) {
        return {};
    }
    const QByteArray decoded = QByteArray::fromBase64(rawBytes);
    if (!decoded.isEmpty()) {
        return QString::fromUtf8(decoded);
    }
    return QString::fromUtf8(rawBytes);
}

inline QString decodeBytesName(const std::string &rawBytes)
{
    return decodeBytesName(QByteArray(rawBytes.data(), static_cast<int>(rawBytes.size())));
}

} // namespace app
