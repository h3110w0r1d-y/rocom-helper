#include "http_server_service.h"

#include "storage/database_service.h"

#include <QHostAddress>
#include <QHttpServerRequest>
#include <QJsonDocument>

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
    // Example query flow: HTTP -> DatabaseService -> HTTP response.
    m_server.route(QStringLiteral("/api/example-state"), QHttpServerRequest::Method::Get, this, [this] {
        return jsonResponse(m_database != nullptr ? m_database->queryExampleState()
                                                  : QJsonObject{{QStringLiteral("ok"), false}});
    });

    // Example command flow: HTTP -> AppEvent -> Dispatcher -> DB/UI/SSE.
    m_server.route(QStringLiteral("/api/example-command"), QHttpServerRequest::Method::Post, this,
                   [this](const QHttpServerRequest &request) {
                       const QJsonDocument doc = QJsonDocument::fromJson(request.body());

                       AppEvent event;
                       event.type = EventType::ExampleHttpCommand;
                       event.source = EventSource::Http;
                       event.flags = EventFlag::Persist | EventFlag::UpdateUi | EventFlag::PushSse;
                       event.name = eventTypeName(event.type);
                       event.payload = doc.isObject()
                           ? doc.object()
                           : QJsonObject{{QStringLiteral("rawBody"), QString::fromUtf8(request.body())}};
                       emit eventCreated(event);

                       return jsonResponse({
                           {QStringLiteral("ok"), true},
                           {QStringLiteral("acceptedEvent"), event.name},
                       });
                   });

    // Example SSE flow: AppEvent -> SseBroadcaster -> every connected HTTP client.
    m_server.route(QStringLiteral("/events"), QHttpServerRequest::Method::Get, this,
                   [this](QHttpServerResponder &&responder) {
        const QByteArray payload = m_lastSsePayload.isEmpty()
            ? QByteArray("event: ready\ndata: {\"ok\":true}\n\n")
            : m_lastSsePayload;
        auto client = std::make_unique<QHttpServerResponder>(std::move(responder));
        client->writeBeginChunked(QByteArrayLiteral("text/event-stream"));
        client->writeChunk(payload);
        m_sseClients.push_back(std::move(client));
    });
}

QHttpServerResponse HttpServerService::jsonResponse(const QJsonObject &object) const
{
    return QHttpServerResponse(object);
}

} // namespace app
