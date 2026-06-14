#pragma once

#include "events/app_event.h"

#include <QByteArray>
#include <QHash>
#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponder>
#include <QJsonArray>
#include <QObject>
#include <QTcpServer>

#include <memory>
#include <vector>

namespace app {

class DatabaseService;
class DataCenter;

class HttpServerService : public QObject {
    Q_OBJECT

public:
    explicit HttpServerService(DatabaseService *database, DataCenter *dataCenter, QObject *parent = nullptr);

    bool start(quint16 port);
    bool restart(quint16 port);
    void stop();
    bool isListening() const;
    quint16 currentPort() const;

public slots:
    void rememberLastSsePayload(quint64 uid, const QByteArray &payload);

signals:
    void eventCreated(const app::AppEvent &event);
    void statusChanged(const QString &message);
    void errorOccurred(const QString &message);

private:
    struct SseClient {
        std::unique_ptr<QHttpServerResponder> responder;
        quint64 uid = 0;
    };

    void setupRoutes();
    static quint64 uidFromRequest(const QHttpServerRequest &request);
    QHttpServerResponse staticWebResponse(const QHttpServerRequest &request) const;
    QHttpServerResponse jsonResponse(const QJsonObject &object) const;
    QHttpServerResponse jsonArrayResponse(const QJsonArray &array) const;
    void acceptSseClient(QHttpServerResponder &&responder, quint64 uid);

    DatabaseService *m_database = nullptr;
    DataCenter *m_dataCenter = nullptr;
    QHttpServer m_server;
    QTcpServer m_tcpServer;
    quint16 m_port = 0;
    QHash<quint64, QByteArray> m_lastSsePayloadByUid;
    std::vector<SseClient> m_sseClients;
};

} // namespace app
