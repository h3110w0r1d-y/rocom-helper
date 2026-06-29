#pragma once

#include "events/app_event.h"

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>
#include <QUrl>

class QNetworkReply;

namespace app {

class HttpApiClient : public QObject {
    Q_OBJECT

public:
    explicit HttpApiClient(QObject *parent = nullptr);
    ~HttpApiClient() override;

    void setEndpoint(const QString &host, int port, quint64 uid = 0);
    void start();
    void stop();
    void restart();
    bool isRunning() const;

    QString host() const;
    int port() const;
    quint64 uid() const;

signals:
    void eventCreated(const app::AppEvent &event);
    void statusChanged(const QString &message);
    void errorOccurred(const QString &message);

private:
    QUrl apiUrl(const QString &path, bool includeUid = false) const;
    void fetchInitialMapPosition();
    void openMapEvents();
    void closeMapEvents();
    void scheduleReconnect();
    void handleMemoryReply(QNetworkReply *reply);
    void handleSseBytes();
    void processSseLine(const QByteArray &line);
    void flushSseEvent();
    void handleSseEvent(const QString &eventName, const QByteArray &data);
    void publishPlayerPosition(const QJsonObject &payload);
    static QString normalizeHost(QString host);
    static int clampPort(int port);

    QNetworkAccessManager m_network;
    QNetworkReply *m_sseReply = nullptr;
    QTimer m_reconnectTimer;
    QString m_host = QStringLiteral("127.0.0.1");
    int m_port = 4939;
    quint64 m_uid = 0;
    bool m_running = false;
    QByteArray m_lineBuffer;
    QString m_currentEvent;
    QByteArray m_currentData;
};

} // namespace app
