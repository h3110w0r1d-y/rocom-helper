#include "event_dispatcher.h"

namespace app {

EventDispatcher::EventDispatcher(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<app::AppEvent>("app::AppEvent");
}

void EventDispatcher::dispatch(const AppEvent &event)
{
    emit eventDispatched(event);
}

} // namespace app

