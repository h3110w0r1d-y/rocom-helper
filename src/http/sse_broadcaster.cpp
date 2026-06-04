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

    const QByteArray json = QJsonDocument(appEventToJson(event)).toJson(QJsonDocument::Compact);
    QByteArray payload("event: app-event\n"
                       "data: ");
    payload.append(json);
    payload.append("\n\n");
    emit ssePayloadReady(payload);
}

} // namespace app
