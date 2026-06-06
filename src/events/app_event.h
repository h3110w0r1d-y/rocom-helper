#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QMetaType>
#include <QString>

namespace app {

enum class EventType {
    Unknown,
    RawTrafficDecoded,
    PlayerPositionChanged,
    MapMarkerAdded,
    MapMarkerMoved,
    MapMarkerUpdated,
    MapMarkerDeleted,
    MapMarkerVisibilityChanged,
    MapMarkerTypeVisibilityChanged,
    CatchRecordAdded,
    ShinyPetDetected,
    BoxHintUpdated,
    PetInfoReload,
    PetInfoChanged,
    PetInfoDeleted,
    BoxInfoReload,
    BoxInfoChanged,
};

enum class EventSource {
    Unknown,
    Traffic,
    Http,
    Ui,
};

enum EventFlag {
    Persist = 0x01,
    UpdateUi = 0x02,
    PushSse = 0x04,
};
Q_DECLARE_FLAGS(EventFlags, EventFlag)

struct AppEvent {
    EventType type = EventType::Unknown;
    EventSource source = EventSource::Unknown;
    EventFlags flags;
    QDateTime occurredAt = QDateTime::currentDateTimeUtc();
    QString name;
    QJsonObject payload;
};

inline QString eventTypeName(EventType type)
{
    switch (type) {
    case EventType::RawTrafficDecoded:
        return QStringLiteral("traffic.raw_decoded");
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
    case EventType::CatchRecordAdded:
        return QStringLiteral("catch.record_added");
    case EventType::ShinyPetDetected:
        return QStringLiteral("catch.shiny_pet_detected");
    case EventType::BoxHintUpdated:
        return QStringLiteral("box.hint_updated");
    case EventType::PetInfoReload:
        return QStringLiteral("pet_info.reload");
    case EventType::PetInfoChanged:
        return QStringLiteral("pet_info.changed");
    case EventType::PetInfoDeleted:
        return QStringLiteral("pet_info.deleted");
    case EventType::BoxInfoReload:
        return QStringLiteral("box.reload");
    case EventType::BoxInfoChanged:
        return QStringLiteral("box.changed");
    case EventType::Unknown:
        break;
    }
    return QStringLiteral("unknown");
}

inline QString eventSourceName(EventSource source)
{
    switch (source) {
    case EventSource::Traffic:
        return QStringLiteral("traffic");
    case EventSource::Http:
        return QStringLiteral("http");
    case EventSource::Ui:
        return QStringLiteral("ui");
    case EventSource::Unknown:
        break;
    }
    return QStringLiteral("unknown");
}

inline QJsonObject appEventToJson(const AppEvent &event)
{
    return {
        {QStringLiteral("type"), eventTypeName(event.type)},
        {QStringLiteral("source"), eventSourceName(event.source)},
        {QStringLiteral("occurredAt"), event.occurredAt.toString(Qt::ISODateWithMs)},
        {QStringLiteral("name"), event.name},
        {QStringLiteral("payload"), event.payload},
    };
}

} // namespace app

Q_DECLARE_OPERATORS_FOR_FLAGS(app::EventFlags)
Q_DECLARE_METATYPE(app::AppEvent)
Q_DECLARE_METATYPE(app::EventType)
Q_DECLARE_METATYPE(app::EventSource)
Q_DECLARE_METATYPE(app::EventFlags)
