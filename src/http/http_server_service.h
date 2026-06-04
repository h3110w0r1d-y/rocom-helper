#pragma once

#include "events/app_event.h"

#include <QByteArray>
#include <QHttpServer>
#include <QHttpServerResponder>
#include <QObject>
#include <QTcpServer>

#include <memory>
#include <vector>

namespace app {

class DatabaseService;

class HttpServerService : public QObject {
    Q_OBJECT

public:
    explicit HttpServerService(DatabaseService *database, QObject *parent = nullptr);

    bool start(quint16 port = 18080);
    void stop();

public slots:
    void rememberLastSsePayload(const QByteArray &payload);

signals:
    void eventCreated(const app::AppEvent &event);
    void statusChanged(const QString &message);
    void errorOccurred(const QString &message);

private:
    void setupRoutes();
    QHttpServerResponse jsonResponse(const QJsonObject &object) const;

    DatabaseService *m_database = nullptr;
    QHttpServer m_server;
    QTcpServer m_tcpServer;
    QByteArray m_lastSsePayload;
    std::vector<std::unique_ptr<QHttpServerResponder>> m_sseClients;
};

} // namespace app
