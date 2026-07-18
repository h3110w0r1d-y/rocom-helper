#include "http_api_client.h"

#include "app_version.h"
#include "data/map_types.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QtMath>

namespace app {
namespace {

int intValue(const QJsonObject &object, const QString &key, int defaultValue = 0)
{
    const QJsonValue value = object.value(key);
    if (value.isDouble()) {
        return qRound(value.toDouble());
    }
    if (value.isString()) {
        bool ok = false;
        const double number = value.toString().toDouble(&ok);
        return ok ? qRound(number) : defaultValue;
    }
    return defaultValue;
}

double doubleValue(const QJsonObject &object, const QString &key, double defaultValue = 0.0)
{
    const QJsonValue value = object.value(key);
    if (value.isDouble()) {
        return value.toDouble();
    }
    if (value.isString()) {
        bool ok = false;
        const double number = value.toString().toDouble(&ok);
        return ok ? number : defaultValue;
    }
    return defaultValue;
}

bool boolValue(const QJsonObject &object, const QString &key, bool defaultValue = false)
{
    const QJsonValue value = object.value(key);
    if (value.isBool()) {
        return value.toBool();
    }
    if (value.isDouble()) {
        return !qFuzzyIsNull(value.toDouble());
    }
    if (value.isString()) {
        const QString text = value.toString().trimmed().toLower();
        if (text == QStringLiteral("true") || text == QStringLiteral("1")) {
            return true;
        }
        if (text == QStringLiteral("false") || text == QStringLiteral("0")) {
            return false;
        }
    }
    return defaultValue;
}

} // namespace

HttpApiClient::HttpApiClient(QObject *parent)
    : QObject(parent)
{
    m_reconnectTimer.setSingleShot(true);
    m_reconnectTimer.setInterval(3000);
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this] {
        openMapEvents();
    });
}

HttpApiClient::~HttpApiClient()
{
    stop();
}

void HttpApiClient::setEndpoint(const QString &host, int port, quint64 uid)
{
    m_host = normalizeHost(host);
    m_port = clampPort(port);
    m_uid = uid;
}

void HttpApiClient::start()
{
    if (m_running) {
        return;
    }
    m_running = true;
    emit statusChanged(QStringLiteral("正在获取用户列表 %1:%2").arg(m_host).arg(m_port));
    fetchUsers();
}

void HttpApiClient::stop()
{
    m_running = false;
    m_reconnectTimer.stop();
    closeMapEvents();
    emit statusChanged(QStringLiteral("未连接"));
}

void HttpApiClient::restart()
{
    const bool wasRunning = m_running;
    stop();
    if (wasRunning) {
        start();
    }
}

bool HttpApiClient::isRunning() const
{
    return m_running;
}

QString HttpApiClient::host() const
{
    return m_host;
}

int HttpApiClient::port() const
{
    return m_port;
}

quint64 HttpApiClient::uid() const
{
    return m_uid;
}

QUrl HttpApiClient::apiUrl(const QString &path, bool includeUid) const
{
    QUrl url;
    url.setScheme(QStringLiteral("http"));
    url.setHost(m_host);
    url.setPort(m_port);
    url.setPath(path);
    if (includeUid) {
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("uid"), QString::number(m_uid));
        url.setQuery(query);
    }
    return url;
}

void HttpApiClient::fetchUsers()
{
    QNetworkRequest request(apiUrl(QStringLiteral("/api/users")));
    request.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        handleUsersReply(reply);
    });
}

void HttpApiClient::handleUsersReply(QNetworkReply *reply)
{
    reply->deleteLater();
    if (!m_running) {
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        m_running = false;
        emit errorOccurred(QStringLiteral("读取用户列表失败: %1").arg(reply->errorString()));
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isArray() || doc.array().isEmpty()) {
        m_running = false;
        emit errorOccurred(QStringLiteral("服务端没有可用的 UID"));
        return;
    }

    emit usersLoaded(doc.array());
    if (m_uid == 0) {
        m_running = false;
        emit errorOccurred(QStringLiteral("用户列表中没有有效的 UID"));
        return;
    }

    emit statusChanged(QStringLiteral("正在校验服务端版本 %1:%2").arg(m_host).arg(m_port));
    fetchServerVersion();
}

void HttpApiClient::fetchServerVersion()
{
    QNetworkRequest request(apiUrl(QStringLiteral("/api/version")));
    request.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        handleVersionReply(reply);
    });
}

void HttpApiClient::handleVersionReply(QNetworkReply *reply)
{
    reply->deleteLater();
    if (!m_running) {
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        m_running = false;
        emit errorOccurred(QStringLiteral("读取服务端版本失败: %1").arg(reply->errorString()));
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    const QString serverVersion = doc.isObject()
        ? doc.object().value(QStringLiteral("version")).toString()
        : QString();
    if (serverVersion != appVersionString()) {
        m_running = false;
        emit errorOccurred(QStringLiteral("服务端版本不一致: 服务端 %1，插件 %2")
                               .arg(serverVersion.isEmpty() ? QStringLiteral("未知") : serverVersion,
                                    appVersionString()));
        return;
    }

    startAfterVersionChecked();
}

void HttpApiClient::startAfterVersionChecked()
{
    if (!m_running) {
        return;
    }
    emit statusChanged(QStringLiteral("正在连接 %1:%2").arg(m_host).arg(m_port));
    fetchInitialMapPosition();
    fetchInitialMapMarkers();
    openMapEvents();
}

void HttpApiClient::createMapMarker(const QJsonObject &marker)
{
    QNetworkRequest request(apiUrl(QStringLiteral("/api/map-markers")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = m_network.post(request, QJsonDocument(marker).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        handleMarkerMutationReply(reply, EventType::MapMarkerAdded);
    });
}

void HttpApiClient::updateMapMarker(const QString &markerId, const QJsonObject &marker)
{
    const QString id = markerId.trimmed();
    if (id.isEmpty()) {
        return;
    }

    QNetworkRequest request(apiUrl(QStringLiteral("/api/map-markers/%1").arg(id)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = m_network.sendCustomRequest(
        request,
        QByteArrayLiteral("PUT"),
        QJsonDocument(marker).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        handleMarkerMutationReply(reply, EventType::MapMarkerUpdated);
    });
}

void HttpApiClient::deleteMapMarker(const QString &markerId)
{
    const QString id = markerId.trimmed();
    if (id.isEmpty()) {
        return;
    }

    QNetworkRequest request(apiUrl(QStringLiteral("/api/map-markers/%1").arg(id)));
    request.setRawHeader("Accept", "application/json");
    QNetworkReply *reply = m_network.deleteResource(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        handleMarkerMutationReply(reply, EventType::MapMarkerDeleted);
    });
}

void HttpApiClient::fetchInitialMapPosition()
{
    QNetworkRequest request(apiUrl(QStringLiteral("/api/memory/map.player_position_changed"), true));
    request.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        handleMemoryReply(reply);
    });
}

void HttpApiClient::fetchInitialMapMarkers()
{
    QNetworkRequest request(apiUrl(QStringLiteral("/api/map-markers")));
    request.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        handleMapMarkersReply(reply);
    });
}

void HttpApiClient::openMapEvents()
{
    openSseEvents(SseChannel::Map);
}

void HttpApiClient::closeMapEvents()
{
    closeSseEvents(SseChannel::Map);
}

void HttpApiClient::scheduleReconnect()
{
    if (!m_running) {
        return;
    }
    emit statusChanged(QStringLiteral("连接断开，准备重连"));
    m_reconnectTimer.start();
}

void HttpApiClient::handleMemoryReply(QNetworkReply *reply)
{
    reply->deleteLater();
    if (!m_running) {
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(QStringLiteral("读取地图快照失败: %1").arg(reply->errorString()));
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject()) {
        return;
    }
    const QJsonObject latest = doc.object().value(QStringLiteral("latest")).toObject();
    if (!latest.isEmpty()) {
        publishPlayerPosition(latest);
    }
}

void HttpApiClient::handleMapMarkersReply(QNetworkReply *reply)
{
    reply->deleteLater();
    if (!m_running) {
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(QStringLiteral("读取地图标注失败: %1").arg(reply->errorString()));
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isArray()) {
        emit errorOccurred(QStringLiteral("地图标注列表 JSON 无效"));
        return;
    }
    emit mapMarkersLoaded(doc.array());
}

void HttpApiClient::handleMarkerMutationReply(QNetworkReply *reply, EventType eventType)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(QStringLiteral("同步地图标注失败: %1").arg(reply->errorString()));
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (eventType == EventType::MapMarkerDeleted) {
        if (doc.isObject()) {
            publishMapMarkerEvent(eventType, doc.object());
        }
        return;
    }
    if (!doc.isObject()) {
        emit errorOccurred(QStringLiteral("地图标注响应 JSON 无效"));
        return;
    }
    publishMapMarkerEvent(eventType, doc.object());
}

void HttpApiClient::openSseEvents(SseChannel channel)
{
    SseState *state = sseState(channel);
    if (!m_running || state == nullptr || state->reply != nullptr) {
        return;
    }

    state->lineBuffer.clear();
    state->currentEvent.clear();
    state->currentData.clear();

    QNetworkRequest request(apiUrl(QStringLiteral("/api/events"), true));
    request.setRawHeader("Accept", "text/event-stream");
    request.setRawHeader("Cache-Control", "no-cache");

    state->reply = m_network.get(request);
    connect(state->reply, &QNetworkReply::readyRead, this, [this, channel] {
        handleSseBytes(channel);
    });
    connect(state->reply, &QNetworkReply::finished, this, [this, channel] {
        SseState *finishedState = sseState(channel);
        if (finishedState == nullptr || finishedState->reply == nullptr) {
            return;
        }
        const QString errorText = finishedState->reply->error() == QNetworkReply::NoError
            ? QString()
            : finishedState->reply->errorString();
        closeSseEvents(channel);
        if (m_running) {
            if (!errorText.isEmpty()) {
                emit errorOccurred(QStringLiteral("%1 SSE 连接断开: %2")
                                       .arg(sseChannelName(channel), errorText));
            }
            scheduleReconnect();
        }
    });
}

void HttpApiClient::closeSseEvents(SseChannel channel)
{
    SseState *state = sseState(channel);
    if (state == nullptr || state->reply == nullptr) {
        return;
    }
    QNetworkReply *reply = state->reply;
    state->reply = nullptr;
    state->lineBuffer.clear();
    state->currentEvent.clear();
    state->currentData.clear();
    reply->disconnect(this);
    reply->abort();
    reply->deleteLater();
}

void HttpApiClient::handleSseBytes(SseChannel channel)
{
    SseState *state = sseState(channel);
    if (state == nullptr || state->reply == nullptr) {
        return;
    }
    state->lineBuffer.append(state->reply->readAll());
    int newline = -1;
    while ((newline = state->lineBuffer.indexOf('\n')) >= 0) {
        QByteArray line = state->lineBuffer.left(newline);
        state->lineBuffer.remove(0, newline + 1);
        if (line.endsWith('\r')) {
            line.chop(1);
        }
        processSseLine(channel, line);
    }
}

void HttpApiClient::processSseLine(SseChannel channel, const QByteArray &line)
{
    SseState *state = sseState(channel);
    if (state == nullptr) {
        return;
    }
    if (line.isEmpty()) {
        flushSseEvent(channel);
        return;
    }
    if (line.startsWith(':')) {
        return;
    }
    if (line.startsWith("event:")) {
        state->currentEvent = QString::fromUtf8(line.mid(6).trimmed());
        return;
    }
    if (line.startsWith("data:")) {
        QByteArray data = line.mid(5);
        if (data.startsWith(' ')) {
            data.remove(0, 1);
        }
        if (!state->currentData.isEmpty()) {
            state->currentData.append('\n');
        }
        state->currentData.append(data);
    }
}

void HttpApiClient::flushSseEvent(SseChannel channel)
{
    SseState *state = sseState(channel);
    if (state == nullptr) {
        return;
    }
    const QString eventName = state->currentEvent.isEmpty() ? QStringLiteral("message") : state->currentEvent;
    const QByteArray data = state->currentData;
    state->currentEvent.clear();
    state->currentData.clear();
    if (!data.isEmpty()) {
        handleSseEvent(channel, eventName, data);
    }
}

void HttpApiClient::handleSseEvent(SseChannel channel, const QString &eventName, const QByteArray &data)
{
    if (eventName == QStringLiteral("ready")) {
        emit statusChanged(QStringLiteral("已连接 %1:%2").arg(m_host).arg(m_port));
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        emit errorOccurred(QStringLiteral("%1 事件 JSON 无效").arg(sseChannelName(channel)));
        return;
    }
    const QJsonObject envelope = doc.object();
    const QJsonObject payload = envelope.value(QStringLiteral("data")).isObject()
        ? envelope.value(QStringLiteral("data")).toObject()
        : envelope;
    if (eventName == eventTypeName(EventType::PlayerPositionChanged)) {
        publishPlayerPosition(payload);
    } else if (eventName == eventTypeName(EventType::MapMarkerAdded)) {
        publishMapMarkerEvent(EventType::MapMarkerAdded, payload);
    } else if (eventName == eventTypeName(EventType::MapMarkerUpdated)) {
        publishMapMarkerEvent(EventType::MapMarkerUpdated, payload);
    } else if (eventName == eventTypeName(EventType::MapMarkerDeleted)) {
        publishMapMarkerEvent(EventType::MapMarkerDeleted, payload);
    } else if (eventName == eventTypeName(EventType::MapMarkerVisibilityChanged)) {
        publishMapMarkerEvent(EventType::MapMarkerVisibilityChanged, payload);
    }
}

void HttpApiClient::publishPlayerPosition(const QJsonObject &payload)
{
    PlayerPositionPayload position;
    position.visible = boolValue(payload, QStringLiteral("visible"), true);
    position.rotation = doubleValue(payload, QStringLiteral("rotation"));
    position.ctrlRotation = doubleValue(payload, QStringLiteral("ctrl_rotation"));
    position.gameX = intValue(payload, QStringLiteral("game_x"));
    position.gameY = intValue(payload, QStringLiteral("game_y"));
    position.gameZ = intValue(payload, QStringLiteral("game_z"));

    AppEvent event = makePlayerPositionChangedEvent(EventSource::Http, EventFlag::UpdateUi, position);
    event.uid = m_uid;
    event.payload = payload;
    emit eventCreated(event);
}

void HttpApiClient::publishMapMarkerEvent(EventType eventType, const QJsonObject &payload)
{
    AppEvent event;
    event.type = eventType;
    event.source = EventSource::Http;
    event.flags = EventFlag::UpdateUi;
    event.occurredAt = QDateTime::currentDateTimeUtc();
    event.name = eventTypeName(eventType);
    event.uid = 0;
    event.payload = payload;
    emit eventCreated(event);
}

void HttpApiClient::publishSimpleEvent(EventType eventType, const QJsonObject &payload)
{
    AppEvent event;
    event.type = eventType;
    event.source = EventSource::Http;
    event.flags = EventFlag::UpdateUi;
    event.occurredAt = QDateTime::currentDateTimeUtc();
    event.name = eventTypeName(eventType);
    event.uid = 0;
    event.payload = payload;
    emit eventCreated(event);
}

SseState *HttpApiClient::sseState(SseChannel channel)
{
    switch (channel) {
    case SseChannel::Map:
        return &m_mapSse;
    }
    return nullptr;
}

QString HttpApiClient::sseChannelName(SseChannel channel) const
{
    switch (channel) {
    case SseChannel::Map:
        return QStringLiteral("地图");
    }
    return QStringLiteral("SSE");
}

QString HttpApiClient::normalizeHost(QString host)
{
    host = host.trimmed();
    if (host.startsWith(QStringLiteral("http://"))) {
        host.remove(0, 7);
    } else if (host.startsWith(QStringLiteral("https://"))) {
        host.remove(0, 8);
    }
    const int slash = host.indexOf(QLatin1Char('/'));
    if (slash >= 0) {
        host = host.left(slash);
    }
    const int colon = host.indexOf(QLatin1Char(':'));
    if (colon >= 0) {
        host = host.left(colon);
    }
    return host.isEmpty() ? QStringLiteral("127.0.0.1") : host;
}

int HttpApiClient::clampPort(int port)
{
    if (port < MinHttpPort || port > MaxHttpPort) {
        return DefaultHttpPort;
    }
    return port;
}

} // namespace app
