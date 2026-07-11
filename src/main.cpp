#include "app_version.h"
#include "ui/main_window.h"

#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLockFile>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>
#include <iterator>
#include <vector>

#include "data/runtime_context.h"

namespace {

constexpr int UpdateCheckTimeoutMs = 10000;
constexpr uint32_t UpdateCheckRsaExponent = 65537;

struct UpdateCheckResult {
    bool allowed = false;
    QString message;
    app::RuntimeContext context;
};

class BigUInt {
public:
    static BigUInt fromBigEndian(const QByteArray &bytes)
    {
        BigUInt value;
        value.m_limbs.resize((bytes.size() + 3) / 4);
        int byteIndex = bytes.size();
        for (size_t limbIndex = 0; limbIndex < value.m_limbs.size(); ++limbIndex) {
            uint32_t limb = 0;
            for (int shift = 0; shift < 32 && byteIndex > 0; shift += 8) {
                --byteIndex;
                limb |= static_cast<uint32_t>(static_cast<unsigned char>(bytes.at(byteIndex))) << shift;
            }
            value.m_limbs[limbIndex] = limb;
        }
        value.normalize();
        return value;
    }

    static BigUInt one()
    {
        BigUInt value;
        value.m_limbs = {1};
        return value;
    }

    QByteArray toBigEndian(int size) const
    {
        QByteArray bytes(size, '\0');
        int outIndex = size;
        for (uint32_t limb : m_limbs) {
            for (int shift = 0; shift < 32 && outIndex > 0; shift += 8) {
                --outIndex;
                bytes[outIndex] = static_cast<char>((limb >> shift) & 0xffU);
            }
        }
        return bytes;
    }

    int bitLength() const
    {
        if (m_limbs.empty()) {
            return 0;
        }
        uint32_t high = m_limbs.back();
        int highBits = 0;
        while (high != 0) {
            ++highBits;
            high >>= 1U;
        }
        return static_cast<int>((m_limbs.size() - 1) * 32 + highBits);
    }

    bool testBit(int bit) const
    {
        const size_t limbIndex = static_cast<size_t>(bit / 32);
        if (limbIndex >= m_limbs.size()) {
            return false;
        }
        return ((m_limbs[limbIndex] >> (bit % 32)) & 1U) != 0;
    }

    int compare(const BigUInt &other) const
    {
        if (m_limbs.size() != other.m_limbs.size()) {
            return m_limbs.size() < other.m_limbs.size() ? -1 : 1;
        }
        for (size_t i = m_limbs.size(); i > 0; --i) {
            const uint32_t lhs = m_limbs[i - 1];
            const uint32_t rhs = other.m_limbs[i - 1];
            if (lhs != rhs) {
                return lhs < rhs ? -1 : 1;
            }
        }
        return 0;
    }

    void subtract(const BigUInt &other)
    {
        uint64_t borrow = 0;
        for (size_t i = 0; i < m_limbs.size(); ++i) {
            const uint64_t rhs = (i < other.m_limbs.size() ? other.m_limbs[i] : 0) + borrow;
            if (m_limbs[i] >= rhs) {
                m_limbs[i] = static_cast<uint32_t>(m_limbs[i] - rhs);
                borrow = 0;
            } else {
                m_limbs[i] = static_cast<uint32_t>((uint64_t(1) << 32) + m_limbs[i] - rhs);
                borrow = 1;
            }
        }
        normalize();
    }

    void add(const BigUInt &other)
    {
        const size_t count = std::max(m_limbs.size(), other.m_limbs.size());
        m_limbs.resize(count);
        uint64_t carry = 0;
        for (size_t i = 0; i < count; ++i) {
            const uint64_t sum = static_cast<uint64_t>(m_limbs[i])
                + (i < other.m_limbs.size() ? other.m_limbs[i] : 0)
                + carry;
            m_limbs[i] = static_cast<uint32_t>(sum);
            carry = sum >> 32;
        }
        if (carry != 0) {
            m_limbs.push_back(static_cast<uint32_t>(carry));
        }
    }

private:
    void normalize()
    {
        while (!m_limbs.empty() && m_limbs.back() == 0) {
            m_limbs.pop_back();
        }
    }

    std::vector<uint32_t> m_limbs;
};

BigUInt addMod(BigUInt lhs, const BigUInt &rhs, const BigUInt &modulus)
{
    lhs.add(rhs);
    if (lhs.compare(modulus) >= 0) {
        lhs.subtract(modulus);
    }
    return lhs;
}

BigUInt reduceMod(const BigUInt &value, const BigUInt &modulus)
{
    BigUInt result;
    for (int bit = value.bitLength() - 1; bit >= 0; --bit) {
        result = addMod(result, result, modulus);
        if (value.testBit(bit)) {
            result = addMod(result, BigUInt::one(), modulus);
        }
    }
    return result;
}

BigUInt multiplyMod(const BigUInt &lhs, const BigUInt &rhs, const BigUInt &modulus)
{
    BigUInt result;
    BigUInt addend = reduceMod(lhs, modulus);
    for (int bit = 0; bit < rhs.bitLength(); ++bit) {
        if (rhs.testBit(bit)) {
            result = addMod(result, addend, modulus);
        }
        addend = addMod(addend, addend, modulus);
    }
    return result;
}

BigUInt powMod(BigUInt base, uint32_t exponent, const BigUInt &modulus)
{
    BigUInt result = BigUInt::one();
    base = reduceMod(base, modulus);
    while (exponent != 0) {
        if ((exponent & 1U) != 0) {
            result = multiplyMod(result, base, modulus);
        }
        exponent >>= 1U;
        if (exponent != 0) {
            base = multiplyMod(base, base, modulus);
        }
    }
    return result;
}

QString decodedUpdateCheckUrl()
{
    static constexpr unsigned char key = 0x5a;
    static constexpr unsigned char data[] = {
        0x32, 0x2E, 0x2E, 0x2A, 0x29, 0x60, 0x75, 0x75, 0x32, 0x69, 0x6B, 0x6B,
        0x6A, 0x2D, 0x6A, 0x28, 0x6B, 0x3E, 0x74, 0x39, 0x35, 0x37, 0x75, 0x28,
        0x35, 0x39, 0x35, 0x77, 0x32, 0x3F, 0x36, 0x2A, 0x3F, 0x28, 0x77, 0x2A,
        0x36, 0x2F, 0x3D, 0x33, 0x34, 0x75, 0x2C, 0x69, 0x74, 0x69, 0x74, 0x6A,
        0x74, 0x2A, 0x32, 0x2A,
    };

    QByteArray url;
    url.reserve(static_cast<int>(std::size(data)));
    for (unsigned char ch : data) {
        url.append(static_cast<char>(ch ^ key));
    }
    return QString::fromLatin1(url);
}

QByteArray decodedUpdateCheckModulus()
{
    static constexpr unsigned char key = 0xa7;
    static constexpr unsigned char data[] = {
        0xC7, 0xE5, 0xEA, 0x0F, 0xBB, 0x66, 0xD5, 0x6C, 0x81, 0xE0, 0x47, 0x6D,
        0xF3, 0x9C, 0x43, 0x86, 0x60, 0x0F, 0x01, 0x71, 0x3F, 0xAF, 0xE7, 0x41,
        0x01, 0x2A, 0xF0, 0x08, 0x97, 0x86, 0xFB, 0xC5, 0xA3, 0x7D, 0xB9, 0xD8,
        0x19, 0xCB, 0x71, 0xBB, 0xCE, 0x2A, 0x64, 0xC5, 0x8E, 0xAB, 0x9E, 0x47,
        0xB7, 0xFE, 0xE5, 0x14, 0xAA, 0x66, 0x4A, 0x12, 0x0A, 0x4F, 0xDF, 0x86,
        0x7E, 0xEB, 0xCB, 0x62, 0x9A, 0xD9, 0xFA, 0x84, 0xAC, 0x98, 0x27, 0xB9,
        0x28, 0x45, 0xBD, 0xF9, 0x8F, 0x3F, 0x60, 0x22, 0x56, 0xBB, 0x04, 0x7D,
        0x0E, 0xC8, 0x6A, 0xD8, 0xB0, 0x8A, 0xF9, 0xF7, 0x1C, 0xFB, 0x8D, 0xE9,
        0x36, 0x4D, 0xF9, 0x7E, 0xA0, 0x15, 0x23, 0xB1, 0xE2, 0x99, 0x75, 0xCB,
        0x10, 0xD9, 0x89, 0xC9, 0x8A, 0xB7, 0x73, 0x87, 0x5F, 0xA6, 0xC8, 0xCB,
        0x2A, 0xB7, 0x55, 0x8D, 0xBA, 0xC0, 0x73, 0xFC, 0x29, 0xB7, 0xF0, 0xAF,
        0x67, 0xAD, 0x07, 0x6F, 0x61, 0x53, 0x6F, 0x7F, 0xD4, 0x9C, 0x23, 0xE0,
        0x34, 0x71, 0x84, 0x02, 0xDE, 0x82, 0xFB, 0x0F, 0x30, 0x1F, 0x0A, 0x83,
        0x8C, 0x87, 0x26, 0x95, 0x0C, 0xC8, 0xA9, 0x4D, 0xB3, 0xDA, 0x54, 0x47,
        0x78, 0xBC, 0xE8, 0x84, 0xA9, 0x4D, 0x1D, 0x3B, 0xE6, 0x2A, 0x00, 0xF0,
        0x19, 0xD9, 0x00, 0x73, 0x41, 0xF8, 0xBD, 0x99, 0x5B, 0x90, 0xFF, 0xFC,
        0x6A, 0xC8, 0x0C, 0x8A, 0x5B, 0x96, 0xD9, 0x49, 0xFD, 0x09, 0xB2, 0x34,
        0x71, 0x4C, 0x8D, 0x29, 0x05, 0x2B, 0x8E, 0xFA, 0x10, 0x14, 0xAD, 0x5A,
        0x79, 0x28, 0xD0, 0xCE, 0x96, 0x2B, 0x4B, 0x04, 0xCF, 0xBA, 0x76, 0xBE,
        0x2B, 0x77, 0xBD, 0x7C, 0x55, 0xF9, 0x81, 0x8A, 0x6C, 0xCD, 0xDC, 0x05,
        0xC9, 0xF1, 0x83, 0xE8, 0x91, 0xF5, 0xD5, 0xB9, 0x63, 0x0B, 0x48, 0x0B,
        0xAF, 0x91, 0x78, 0x45, 0x83, 0x93, 0xFA, 0x72, 0x51, 0xE8, 0xCC, 0x94,
        0x3F, 0xAB, 0x10, 0x25, 0x99, 0x2D, 0x00, 0x33, 0x36, 0x2D, 0x66, 0x4C,
        0xFE, 0x9C, 0xDC, 0xB5, 0x2B, 0x01, 0xB8, 0x34, 0x66, 0x88, 0x09, 0xF2,
        0xF7, 0x38, 0x69, 0xA6, 0x73, 0x07, 0xA0, 0xD8, 0xAC, 0x9A, 0x71, 0x1F,
        0xE8, 0x03, 0x13, 0xBE, 0xBB, 0x0E, 0xDE, 0xB9, 0xDE, 0x59, 0xE9, 0x62,
        0x5E, 0x47, 0xD8, 0x9C, 0x20, 0x18, 0x84, 0x8A, 0xB1, 0x4B, 0x76, 0xE8,
        0x45, 0x2E, 0x73, 0x0F, 0xF4, 0x43, 0xD1, 0xE6, 0x95, 0xD2, 0x0D, 0xDA,
        0xB4, 0x9F, 0x77, 0xE4, 0xAF, 0xE4, 0x05, 0xDC, 0x72, 0x7F, 0x6D, 0x3F,
        0xCD, 0xB3, 0x90, 0x45, 0x27, 0x0C, 0xF6, 0xA4, 0xF8, 0x91, 0x60, 0x72,
        0xF9, 0x4B, 0x3F, 0xC6, 0x83, 0x71, 0xCF, 0x3E, 0xDA, 0xE0, 0x21, 0x33,
        0x0A, 0xE9, 0x2E, 0xEE, 0xD2, 0x09, 0x87, 0x42, 0x8F, 0x02, 0x01, 0xC1,
        0xDD, 0x8C, 0xA7, 0xCE, 0xDA, 0xF4, 0xE5, 0xE3, 0x8C, 0x04, 0x5D, 0x79,
        0xEA, 0x88, 0x3C, 0x9C, 0x26, 0x68, 0x7A, 0x2F, 0x29, 0x07, 0xC9, 0x98,
        0x49, 0x39, 0xB0, 0x4E, 0x13, 0x57, 0xA5, 0x04, 0x9B, 0x33, 0x6C, 0x64,
        0x22, 0x00, 0xB2, 0x8B, 0x58, 0xB9, 0x01, 0x43, 0x4A, 0x2D, 0x8E, 0xE4,
        0x7D, 0x31, 0x74, 0x59, 0xB8, 0x47, 0x8C, 0x32, 0xAA, 0x5D, 0x83, 0xB3,
        0x26, 0x20, 0x1E, 0x0A, 0x5C, 0x5E, 0xFE, 0x8A, 0xA8, 0x1F, 0x62, 0x3F,
        0x2C, 0xE6, 0x55, 0x7B, 0xB0, 0x6D, 0xDA, 0xC9, 0x12, 0x63, 0xCE, 0x27,
        0xEF, 0x76, 0x70, 0x2B, 0x91, 0x54, 0x70, 0x7A, 0x08, 0x7D, 0xBA, 0xEE,
        0xDF, 0xC3, 0xED, 0xC4, 0xF5, 0x15, 0xA3, 0x2F, 0xCB, 0x4C, 0xD7, 0xCD,
        0xD9, 0xA8, 0xEF, 0x25, 0x7A, 0x8A, 0x1F, 0xA0, 0x57, 0xF6, 0x06, 0x61,
        0x79, 0x72, 0xF0, 0x46, 0xA7, 0x30, 0x90, 0xEE,
    };

    QByteArray modulus;
    modulus.reserve(static_cast<int>(std::size(data)));
    for (unsigned char ch : data) {
        modulus.append(static_cast<char>(ch ^ key));
    }
    return modulus;
}

QByteArray rsaPublicDecryptPkcs1V15(const QByteArray &ciphertext)
{
    const BigUInt modulus = BigUInt::fromBigEndian(decodedUpdateCheckModulus());
    const int modulusSize = (modulus.bitLength() + 7) / 8;
    if (ciphertext.size() != modulusSize) {
        return {};
    }

    const BigUInt cipher = BigUInt::fromBigEndian(ciphertext);
    if (cipher.compare(modulus) >= 0) {
        return {};
    }

    const QByteArray encoded = powMod(cipher, UpdateCheckRsaExponent, modulus).toBigEndian(modulusSize);
    if (encoded.size() < 11
        || static_cast<unsigned char>(encoded.at(0)) != 0x00
        || static_cast<unsigned char>(encoded.at(1)) != 0x02) {
        return {};
    }

    int separator = -1;
    for (int i = 2; i < encoded.size(); ++i) {
        if (encoded.at(i) == '\0') {
            separator = i;
            break;
        }
    }
    if (separator < 10) {
        return {};
    }
    return encoded.mid(separator + 1);
}

app::RuntimeContext runtimeContextFromUpdateCheckContent(QByteArray content)
{
    content = content.trimmed();
    const QByteArray payload = rsaPublicDecryptPkcs1V15(QByteArray::fromBase64(content));
    if (payload.isEmpty() || payload.size() > 4096) {
        return {};
    }

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        return {};
    }
    const QJsonObject object = doc.object();
    if (object.value(QStringLiteral("v")).toString() != app::appVersionString()) {
        return {};
    }

    const QByteArray seed = QByteArray::fromHex(object.value(QStringLiteral("s")).toString().toLatin1());
    return app::makeRuntimeContext(seed);
}

[[maybe_unused]] UpdateCheckResult checkUpdateGate()
{
    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl(decodedUpdateCheckUrl())};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = manager.get(request);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    timeout.start(UpdateCheckTimeoutMs);
    loop.exec();

    UpdateCheckResult result;
    if (!timeout.isActive()) {
        reply->abort();
        reply->deleteLater();
        result.message = QStringLiteral("error");
        return result;
    }

    timeout.stop();
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray contentBytes = reply->readAll();
    const QString content = QString::fromUtf8(contentBytes).trimmed();
    const QNetworkReply::NetworkError error = reply->error();
    reply->deleteLater();

    if (error == QNetworkReply::NoError && statusCode == 200) {
        result.context = runtimeContextFromUpdateCheckContent(contentBytes);
        if (result.context.isValid()) {
            result.allowed = true;
            return result;
        }
    }

    result.message = content.isEmpty() ? QStringLiteral("检查更新失败，软件退出") : content;
    return result;
}

class SingleInstanceGuard {
public:
    explicit SingleInstanceGuard(const QString &lockFileName)
        : m_lockFile(QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath(lockFileName))
    {
    }

    bool acquire()
    {
        return m_lockFile.tryLock();
    }

private:
    QLockFile m_lockFile;
};

} // namespace

int main(int argc, char *argv[])
{
    QImageReader::setAllocationLimit(1024);

    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/app.png")));
    QCoreApplication::setApplicationName(QStringLiteral("roco_helper_plugin"));
    QCoreApplication::setApplicationVersion(app::appVersionString());

    SingleInstanceGuard instanceGuard(QStringLiteral("roco_helper_plugin.lock"));
    if (!instanceGuard.acquire()) {
        QMessageBox::warning(nullptr,
                             app::appWindowTitle(),
                             QStringLiteral("程序已在运行，请勿重复启动。"));
        return 0;
    }

    app::MainWindow window;
    window.show();

    return app.exec();
}
