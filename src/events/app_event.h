#pragma once

#include "data/map_types.h"

#include <QDateTime>
#include <QJsonObject>
#include <QMetaType>
#include <QString>

#include <optional>

namespace app {

enum class EventType {
    Unknown,
    PlayerPositionChanged,
    MapMarkerAdded,
    MapMarkerMoved,
    MapMarkerUpdated,
    MapMarkerDeleted,
    MapMarkerVisibilityChanged,
    MapMarkerTypeVisibilityChanged,
    ShinyPetDetected,
    BoxHintUpdated,
};

enum class EventSource {
    Unknown,
    Http,
    Ui,
};

enum EventFlag {
    UpdateUi = 0x01,
};
Q_DECLARE_FLAGS(EventFlags, EventFlag)

struct AppEvent {
    EventType type = EventType::Unknown;
    EventSource source = EventSource::Unknown;
    EventFlags flags;
    quint64 uid = 0;
    QDateTime occurredAt = QDateTime::currentDateTimeUtc();
    QString name;
    QJsonObject payload;
    std::optional<PlayerPositionPayload> playerPosition;
};

inline QString eventTypeName(EventType type)
{
    switch (type) {
    case EventType::PlayerPositionChanged:
        return QStringLiteral("map.player_position_changed");
    case EventType::MapMarkerAdded:
        return QStringLiteral("map.marker_added");
    case EventType::MapMarkerMoved:
        return QStringLiteral("map.marker_moved");
    case EventType::MapMarkerUpdated:
        return QStringLiteral("map.marker_updated");
    case EventType::MapMarkerDeleted:
        return QStringLiteral("map.marker_deleted");
    case EventType::MapMarkerVisibilityChanged:
        return QStringLiteral("map.marker_visibility_changed");
    case EventType::MapMarkerTypeVisibilityChanged:
        return QStringLiteral("map.marker_type_visibility_changed");
    case EventType::ShinyPetDetected:
        return QStringLiteral("catch.shiny_pet_detected");
    case EventType::BoxHintUpdated:
        return QStringLiteral("box.hint_updated");
    case EventType::Unknown:
        break;
    }
    return QStringLiteral("unknown");
}

inline QString eventSourceName(EventSource source)
{
    switch (source) {
    case EventSource::Http:
        return QStringLiteral("http");
    case EventSource::Ui:
        return QStringLiteral("ui");
    case EventSource::Unknown:
        break;
    }
    return QStringLiteral("unknown");
}

inline AppEvent makePlayerPositionChangedEvent(
    EventSource source,
    EventFlags flags,
    const PlayerPositionPayload &position)
{
    AppEvent event;
    event.type = EventType::PlayerPositionChanged;
    event.source = source;
    event.flags = flags;
    event.occurredAt = QDateTime::currentDateTimeUtc();
    event.name = eventTypeName(EventType::PlayerPositionChanged);
    event.playerPosition = position;
    return event;
}

inline QJsonObject appEventToJson(const AppEvent &event)
{
    QJsonObject payload = event.payload;
    return {
        {QStringLiteral("type"), eventTypeName(event.type)},
        {QStringLiteral("source"), eventSourceName(event.source)},
        {QStringLiteral("occurredAt"), event.occurredAt.toString(Qt::ISODateWithMs)},
        {QStringLiteral("name"), event.name},
        {QStringLiteral("payload"), payload},
    };
}

} // namespace app

Q_DECLARE_OPERATORS_FOR_FLAGS(app::EventFlags)
Q_DECLARE_METATYPE(app::AppEvent)
Q_DECLARE_METATYPE(app::EventType)
Q_DECLARE_METATYPE(app::EventSource)
Q_DECLARE_METATYPE(app::EventFlags)
