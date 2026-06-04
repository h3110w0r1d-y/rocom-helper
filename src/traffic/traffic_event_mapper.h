#pragma once

#include "events/app_event.h"
#include "rwtd/rwtd_types.h"

#include <QObject>

namespace app {

class TrafficEventMapper : public QObject {
    Q_OBJECT

public:
    explicit TrafficEventMapper(QObject *parent = nullptr);

public slots:
    void mapDecodedAction(const rwtd::DecodedAction &action);

signals:
    void eventCreated(const app::AppEvent &event);
};

} // namespace app

