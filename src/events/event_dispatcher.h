#pragma once

#include "app_event.h"

#include <QObject>

namespace app {

class EventDispatcher : public QObject {
    Q_OBJECT

public:
    explicit EventDispatcher(QObject *parent = nullptr);

public slots:
    void dispatch(const app::AppEvent &event);

signals:
    void eventDispatched(const app::AppEvent &event);
};

} // namespace app

