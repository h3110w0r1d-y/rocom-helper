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
        0x36, 0x2F, 0x3D, 0x33, 0x34, 0x75, 0x2C, 0x69, 0x74, 0x68, 0x74, 0x6B,
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
        0xC9, 0xAF, 0xBB, 0x23, 0xA1, 0x94, 0xF5, 0xDC, 0x45, 0x1D, 0x00, 0x9C,
        0x14, 0x33, 0x1A, 0x7A, 0x05, 0x9E, 0x87, 0x1D, 0xA5, 0xE5, 0x7E, 0xD7,
        0xF7, 0x98, 0x95, 0x47, 0x25, 0x79, 0x78, 0x31, 0xE1, 0x20, 0xB4, 0x3F,
        0x90, 0x28, 0xAD, 0xF2, 0x05, 0x3D, 0xCA, 0xB5, 0x2B, 0xDC, 0x5A, 0xE5,
        0x84, 0xF5, 0x4B, 0x97, 0x97, 0x5E, 0x0D, 0x45, 0x58, 0xF7, 0x13, 0x78,
        0x39, 0x30, 0x39, 0x7B, 0x09, 0x45, 0xA2, 0xA9, 0x4A, 0x10, 0x68, 0xB1,
        0xF9, 0xC5, 0x5D, 0x71, 0x60, 0xE4, 0x34, 0xCA, 0x4D, 0x55, 0x62, 0xB3,
        0xD5, 0xEF, 0x22, 0x57, 0x48, 0xBC, 0x46, 0x39, 0xE2, 0x8C, 0xD5, 0xC3,
        0xAD, 0xC8, 0x42, 0x32, 0x71, 0x3B, 0xA7, 0x03, 0xF2, 0x62, 0x25, 0xF1,
        0x6A, 0x57, 0x47, 0x3C, 0x40, 0xE3, 0x33, 0x00, 0x27, 0x3A, 0x4C, 0x26,
        0xA4, 0x95, 0x34, 0x58, 0x2C, 0xB2, 0x10, 0xE4, 0x1C, 0x90, 0xD7, 0x8C,
        0x27, 0x87, 0x13, 0x61, 0x91, 0x0D, 0x08, 0x51, 0x88, 0xC0, 0x1C, 0x72,
        0xA7, 0xD8, 0xD9, 0x71, 0xCB, 0xBE, 0xCB, 0xDD, 0xBC, 0xCF, 0x82, 0xBE,
        0x48, 0x95, 0x9D, 0x9E, 0x3E, 0x91, 0x00, 0x76, 0x5B, 0xD8, 0x0E, 0x13,
        0x94, 0xAE, 0x19, 0xCD, 0xB8, 0x50, 0x65, 0x94, 0xF9, 0x50, 0x71, 0xB7,
        0xFF, 0xE9, 0x4B, 0x8D, 0x06, 0x83, 0x7C, 0x79, 0x03, 0x80, 0xAA, 0xCD,
        0x55, 0x28, 0x6F, 0xF3, 0xB4, 0xC5, 0x73, 0xD7, 0x0C, 0x66, 0x6B, 0xA9,
        0x88, 0x4E, 0x02, 0x33, 0x42, 0x5C, 0x3D, 0x4B, 0x4F, 0x33, 0xF7, 0x90,
        0x36, 0xA1, 0x52, 0xEF, 0x3E, 0x26, 0xB7, 0xDE, 0x05, 0xD5, 0x32, 0xB6,
        0xE6, 0xD2, 0x5C, 0xCB, 0x13, 0x99, 0x30, 0x9D, 0xD5, 0x21, 0x5B, 0x48,
        0x39, 0x99, 0x15, 0x40, 0x92, 0x46, 0x8B, 0x5B, 0xF7, 0xDD, 0x6A, 0x18,
        0x5D, 0xD4, 0x8F, 0x7D, 0xB7, 0x4D, 0x6F, 0xF7, 0x27, 0x49, 0x4D, 0x9B,
        0xCD, 0x4C, 0x09, 0xF6, 0x23, 0xDD, 0xDC, 0x72, 0xAB, 0xAC, 0xA6, 0xBA,
        0x2B, 0x45, 0x4D, 0x08, 0x62, 0x91, 0xE2, 0x03, 0x51, 0x61, 0x9E, 0x0A,
        0x08, 0x49, 0xAF, 0x70, 0xD2, 0xFF, 0x0D, 0x85, 0x6B, 0x3C, 0x1F, 0xC1,
        0x30, 0xE4, 0xFF, 0x18, 0x7A, 0xAC, 0x30, 0xE0, 0xD1, 0xF3, 0xEA, 0x8E,
        0xD2, 0xB3, 0x0A, 0xA5, 0xB2, 0xF2, 0x62, 0x7C, 0x94, 0x9C, 0x0C, 0xF9,
        0xDF, 0xFF, 0x0D, 0x29, 0x17, 0xEA, 0xBA, 0xC8, 0x04, 0x80, 0xB1, 0xD8,
        0x3A, 0x59, 0x60, 0xB5, 0xC0, 0xEA, 0xEF, 0x8F, 0x41, 0xB2, 0x58, 0x3C,
        0x44, 0xBF, 0x48, 0x30, 0x45, 0xAB, 0x18, 0x73, 0x8B, 0x8C, 0x20, 0x20,
        0x18, 0x9F, 0xB0, 0x5A, 0xED, 0x31, 0xB4, 0x17, 0xD1, 0x04, 0x23, 0x07,
        0xC5, 0xB1, 0x4E, 0x8E, 0x71, 0x06, 0x64, 0x2C, 0xCE, 0x9A, 0x14, 0x96,
        0xC2, 0x77, 0x37, 0x30, 0xC9, 0x24, 0xE4, 0x21, 0x22, 0x32, 0x62, 0xE0,
        0x7E, 0x14, 0x4E, 0x54, 0x84, 0x25, 0x22, 0x11, 0x3D, 0x5B, 0xF8, 0xFF,
        0x43, 0x41, 0xC7, 0x82, 0x8B, 0x83, 0xFD, 0x34, 0x24, 0xC1, 0x30, 0x80,
        0x2A, 0x81, 0x7A, 0x20, 0xA3, 0xCF, 0xDD, 0xF1, 0xA3, 0x5C, 0x30, 0x3F,
        0xCF, 0x6A, 0xDA, 0x98, 0x1E, 0xED, 0x98, 0xA0, 0xF1, 0x53, 0x5A, 0x37,
        0x92, 0x99, 0x85, 0x59, 0x60, 0x5A, 0x10, 0xD3, 0x3E, 0x21, 0x7B, 0xC7,
        0xD1, 0x51, 0x37, 0xA5, 0x45, 0x78, 0xC6, 0x6E, 0x28, 0x91, 0x69, 0x1B,
        0xEA, 0xFC, 0xE1, 0xDF, 0xA4, 0x81, 0x88, 0x84, 0x40, 0x59, 0x69, 0xA3,
        0x81, 0x48, 0x24, 0xBD, 0xAE, 0x68, 0xC6, 0x81, 0x76, 0xFD, 0x25, 0xC4,
        0xDA, 0x7E, 0x4D, 0xBC, 0xD3, 0x85, 0x2B, 0x27, 0x9C, 0x08, 0x3F, 0x2E,
        0x4E, 0xA4, 0x36, 0x99, 0xE9, 0x7F, 0x1D, 0x66,
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
