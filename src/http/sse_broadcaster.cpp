#include "sse_broadcaster.h"

#include <QJsonDocument>

namespace app {

SseBroadcaster::SseBroadcaster(QObject *parent)
    : QObject(parent)
{
}

void SseBroadcaster::handleEvent(const AppEvent &event)
{
    if (!event.flags.testFlag(EventFlag::PushSse)) {
        return;
    }

    QString sseEventName = QStringLiteral("app-event");
    QJsonObject data = appEventToJson(event);
    switch (event.type) {
    case EventType::PetInfoReload:
        if (event.payload.value(QStringLiteral("page_no")).toInt(1)
            < event.payload.value(QStringLiteral("total_page")).toInt(1)) {
            return;
        }
        sseEventName = QStringLiteral("pet_info_reload");
        data = {};
        break;
    case EventType::PetInfoChanged:
        sseEventName = QStringLiteral("pet_info_changed");
        data = event.payload;
        break;
    case EventType::PetInfoDeleted:
        sseEventName = QStringLiteral("pet_info_deleted");
        data = event.payload;
        break;
    case EventType::BoxInfoReload:
        sseEventName = QStringLiteral("box_reload");
        data = {};
        break;
    case EventType::BoxInfoChanged:
        sseEventName = QStringLiteral("box_changed");
        data = event.payload;
        break;
    case EventType::BoxInfoBoxReplaced:
        sseEventName = QStringLiteral("box_replaced");
        data = event.payload;
        break;
    default:
        break;
    }

    const QByteArray json = QJsonDocument(data).toJson(QJsonDocument::Compact);
    QByteArray payload("event: ");
    payload.append(sseEventName.toUtf8());
    payload.append("\ndata: ");
    payload.append(json);
    payload.append("\n\n");
    emit ssePayloadReady(event.uid, payload);
}

} // namespace app
