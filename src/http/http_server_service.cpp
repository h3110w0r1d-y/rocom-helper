#include "http_server_service.h"

#include "data/data_center.h"
#include "storage/database_service.h"

#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QHttpServerRequest>
#include <QJsonDocument>
#include <QMimeDatabase>
#include <QUrlQuery>

#include <utility>

namespace app {

HttpServerService::HttpServerService(DatabaseService *database, DataCenter *dataCenter, QObject *parent)
    : QObject(parent)
    , m_database(database)
    , m_dataCenter(dataCenter)
{
    setupRoutes();
}

bool HttpServerService::start(quint16 port)
{
    if (m_tcpServer.isListening()) {
        return m_port == port;
    }
    if (m_dataCenter == nullptr || !m_dataCenter->runtimeContext().isValid()) {
        emit errorOccurred(QStringLiteral("HTTP 服务启动失败"));
        return false;
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

    m_port = port;
    emit statusChanged(QStringLiteral("HTTP 服务监听 http://127.0.0.1:%1").arg(port));
    return true;
}

bool HttpServerService::restart(quint16 port)
{
    if (m_tcpServer.isListening() && m_port == port) {
        return true;
    }
    stop();
    return start(port);
}

void HttpServerService::stop()
{
    for (const SseClient &client : m_sseClients) {
        if (!client.responder->isResponseCanceled()) {
            client.responder->writeEndChunked(QByteArray());
        }
    }
    m_sseClients.clear();
    m_tcpServer.close();
    m_port = 0;
    emit statusChanged(QStringLiteral("HTTP 服务已停止"));
}

bool HttpServerService::isListening() const
{
    return m_tcpServer.isListening();
}

quint16 HttpServerService::currentPort() const
{
    return m_port;
}

void HttpServerService::rememberLastSsePayload(quint64 uid, const QByteArray &payload)
{
    m_lastSsePayloadByUid.insert(uid, payload);

    for (auto it = m_sseClients.begin(); it != m_sseClients.end();) {
        if (it->responder->isResponseCanceled()) {
            it = m_sseClients.erase(it);
            continue;
        }
        // uid == 0 events are global and broadcast to everyone; otherwise only
        // clients subscribed to that uid (or unfiltered clients) receive it.
        const bool deliver = uid == 0 || it->uid == 0 || it->uid == uid;
        if (deliver) {
            it->responder->writeChunk(payload);
        }
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
            {QStringLiteral("port"), static_cast<int>(m_port)},
            {QStringLiteral("database_ready"), m_database != nullptr && m_database->isOpen()},
        });
    });

    m_server.route(QStringLiteral("/api/pet-info"), QHttpServerRequest::Method::Get, this,
                   [this](const QHttpServerRequest &request) {
        const quint64 uid = uidFromRequest(request);
        return jsonArrayResponse(m_database != nullptr ? m_database->queryPetInfo(uid) : QJsonArray());
    });

    m_server.route(QStringLiteral("/api/box-info"), QHttpServerRequest::Method::Get, this,
                   [this](const QHttpServerRequest &request) {
        const quint64 uid = uidFromRequest(request);
        return jsonArrayResponse(m_database != nullptr ? m_database->queryBoxInfo(uid) : QJsonArray());
    });

    m_server.route(QStringLiteral("/api/users"), QHttpServerRequest::Method::Get, this, [this] {
        return jsonArrayResponse(m_database != nullptr ? m_database->queryUsers() : QJsonArray());
    });

    m_server.route(QStringLiteral("/events"), QHttpServerRequest::Method::Get, this,
                   [this](const QHttpServerRequest &request, QHttpServerResponder &&responder) {
        acceptSseClient(std::move(responder), uidFromRequest(request));
    });
    m_server.route(QStringLiteral("/api/events"), QHttpServerRequest::Method::Get, this,
                   [this](const QHttpServerRequest &request, QHttpServerResponder &&responder) {
        acceptSseClient(std::move(responder), uidFromRequest(request));
    });
}

quint64 HttpServerService::uidFromRequest(const QHttpServerRequest &request)
{
    const QString value = request.query().queryItemValue(QStringLiteral("uid"));
    if (value.isEmpty()) {
        return 0;
    }
    bool ok = false;
    const quint64 uid = value.toULongLong(&ok);
    return ok ? uid : 0;
}

QHttpServerResponse HttpServerService::staticWebResponse(const QHttpServerRequest &request) const
{
    if (m_dataCenter == nullptr) {
        return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);
    }
    const RuntimeContext &context = m_dataCenter->runtimeContext();
    const QString webRoot = context.webResourceRoot();
    const QString indexPath = context.webIndexPath();
    if (webRoot.isEmpty() || indexPath.isEmpty()) {
        return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);
    }

    QString path = QDir::cleanPath(request.url().path());
    if (path == QStringLiteral(".") || path == QStringLiteral("/")) {
        path = indexPath;
    }

    QString resourcePath = webRoot + path;
    QFile resourceFile(resourcePath);
    if (!resourceFile.exists() || !resourceFile.open(QIODevice::ReadOnly)) {
        resourcePath = webRoot + indexPath;
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

void HttpServerService::acceptSseClient(QHttpServerResponder &&responder, quint64 uid)
{
    auto responderPtr = std::make_unique<QHttpServerResponder>(std::move(responder));
    responderPtr->writeBeginChunked(QByteArrayLiteral("text/event-stream"));
    responderPtr->writeChunk(QByteArray("event: ready\ndata: {\"ok\":true}\n\n"));

    const QByteArray globalPayload = m_lastSsePayloadByUid.value(0);
    if (!globalPayload.isEmpty()) {
        responderPtr->writeChunk(globalPayload);
    }
    if (uid != 0) {
        const QByteArray userPayload = m_lastSsePayloadByUid.value(uid);
        if (!userPayload.isEmpty()) {
            responderPtr->writeChunk(userPayload);
        }
    }

    m_sseClients.push_back(SseClient{std::move(responderPtr), uid});
}

} // namespace app
