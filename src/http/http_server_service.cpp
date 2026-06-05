#include "http_server_service.h"

#include "storage/database_service.h"

#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QHttpServerRequest>
#include <QJsonDocument>
#include <QMimeDatabase>

#include <utility>

namespace app {

HttpServerService::HttpServerService(DatabaseService *database, QObject *parent)
    : QObject(parent)
    , m_database(database)
{
    setupRoutes();
}

bool HttpServerService::start(quint16 port)
{
    if (m_tcpServer.isListening()) {
        return true;
    }

    if (!m_tcpServer.listen(QHostAddress::LocalHost, port)) {
        emit errorOccurred(QStringLiteral("HTTP 服务启动失败: %1").arg(m_tcpServer.errorString()));
        return false;
    }
    if (!m_server.bind(&m_tcpServer)) {
        emit errorOccurred(QStringLiteral("HTTP 服务绑定失败"));
        m_tcpServer.close();
        return false;
    }

    emit statusChanged(QStringLiteral("HTTP 服务监听 http://127.0.0.1:%1").arg(port));
    return true;
}

void HttpServerService::stop()
{
    for (const std::unique_ptr<QHttpServerResponder> &client : m_sseClients) {
        if (!client->isResponseCanceled()) {
            client->writeEndChunked(QByteArray());
        }
    }
    m_sseClients.clear();
    m_tcpServer.close();
    emit statusChanged(QStringLiteral("HTTP 服务已停止"));
}

void HttpServerService::rememberLastSsePayload(const QByteArray &payload)
{
    m_lastSsePayload = payload;

    for (auto it = m_sseClients.begin(); it != m_sseClients.end();) {
        if ((*it)->isResponseCanceled()) {
            it = m_sseClients.erase(it);
            continue;
        }
        (*it)->writeChunk(payload);
        ++it;
    }
}

void HttpServerService::setupRoutes()
{
    m_server.setMissingHandler(this, [this](const QHttpServerRequest &request,
                                            QHttpServerResponder &responder) {
        if (request.url().path().startsWith(QStringLiteral("/api"))) {
            responder.write(QHttpServerResponder::StatusCode::NotFound);
            return;
        }

        responder.sendResponse(staticWebResponse(request));
    });

    m_server.route(QStringLiteral("/api/health"), QHttpServerRequest::Method::Get, this, [this] {
        return jsonResponse({
            {QStringLiteral("ok"), true},
            {QStringLiteral("database_ready"), m_database != nullptr && m_database->isOpen()},
        });
    });

    m_server.route(QStringLiteral("/api/pet-info"), QHttpServerRequest::Method::Get, this, [this] {
        return jsonArrayResponse(m_database != nullptr ? m_database->queryPetInfo() : QJsonArray());
    });

    m_server.route(QStringLiteral("/api/box-info"), QHttpServerRequest::Method::Get, this, [this] {
        return jsonArrayResponse(m_database != nullptr ? m_database->queryBoxInfo() : QJsonArray());
    });

    m_server.route(QStringLiteral("/events"), QHttpServerRequest::Method::Get, this,
                   [this](QHttpServerResponder &&responder) {
        acceptSseClient(std::move(responder));
    });
    m_server.route(QStringLiteral("/api/events"), QHttpServerRequest::Method::Get, this,
                   [this](QHttpServerResponder &&responder) {
        acceptSseClient(std::move(responder));
    });
}

QHttpServerResponse HttpServerService::staticWebResponse(const QHttpServerRequest &request) const
{
    static constexpr auto indexResourcePath = ":/web/index.html";

    QString path = QDir::cleanPath(request.url().path());
    if (path == QStringLiteral(".") || path == QStringLiteral("/")) {
        path = QStringLiteral("/index.html");
    }

    QString resourcePath = QStringLiteral(":/web") + path;
    QFile resourceFile(resourcePath);
    if (!resourceFile.exists() || !resourceFile.open(QIODevice::ReadOnly)) {
        resourcePath = QString::fromLatin1(indexResourcePath);
        resourceFile.setFileName(resourcePath);
        if (!resourceFile.open(QIODevice::ReadOnly)) {
            return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);
        }
    }

    QByteArray mimeType = QMimeDatabase().mimeTypeForFile(resourcePath).name().toUtf8();
    if (resourcePath.endsWith(QStringLiteral(".html"), Qt::CaseInsensitive)) {
        mimeType = QByteArrayLiteral("text/html; charset=utf-8");
    } else if (resourcePath.endsWith(QStringLiteral(".js"), Qt::CaseInsensitive)) {
        mimeType = QByteArrayLiteral("text/javascript; charset=utf-8");
    } else if (resourcePath.endsWith(QStringLiteral(".css"), Qt::CaseInsensitive)) {
        mimeType = QByteArrayLiteral("text/css; charset=utf-8");
    }

    return QHttpServerResponse(mimeType, resourceFile.readAll());
}

QHttpServerResponse HttpServerService::jsonResponse(const QJsonObject &object) const
{
    return QHttpServerResponse(object);
}

QHttpServerResponse HttpServerService::jsonArrayResponse(const QJsonArray &array) const
{
    return QHttpServerResponse(
        QByteArrayLiteral("application/json"),
        QJsonDocument(array).toJson(QJsonDocument::Compact));
}

void HttpServerService::acceptSseClient(QHttpServerResponder &&responder)
{
    const QByteArray payload = m_lastSsePayload.isEmpty()
        ? QByteArray("event: ready\ndata: {\"ok\":true}\n\n")
        : m_lastSsePayload;
    auto client = std::make_unique<QHttpServerResponder>(std::move(responder));
    client->writeBeginChunked(QByteArrayLiteral("text/event-stream"));
    client->writeChunk(payload);
    m_sseClients.push_back(std::move(client));
}

} // namespace app
