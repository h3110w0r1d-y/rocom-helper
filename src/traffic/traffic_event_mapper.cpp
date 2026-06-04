#include "traffic_event_mapper.h"

namespace app {

TrafficEventMapper::TrafficEventMapper(QObject *parent)
    : QObject(parent)
{
}

void TrafficEventMapper::mapDecodedAction(const rwtd::DecodedAction &action)
{
    // Example: every decoded traffic action becomes one business event.
    // Replace this with opcode-specific mapping when business rules are known.
    AppEvent event;
    event.type = EventType::ExampleTrafficDetected;
    event.source = EventSource::Traffic;
    event.flags = EventFlag::Persist | EventFlag::UpdateUi | EventFlag::PushSse;
    event.name = eventTypeName(event.type);
    event.payload = {
        {QStringLiteral("flowId"), action.flowId},
        {QStringLiteral("direction"), rwtd::trafficDirectionName(action.direction)},
        {QStringLiteral("opcode"), static_cast<int>(action.opcode)},
        {QStringLiteral("messageName"), action.messageName},
        {QStringLiteral("decoded"), action.payload},
    };
    emit eventCreated(event);
}

} // namespace app

