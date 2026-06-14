#pragma once

#include "events/app_event.h"

#include <QByteArray>
#include <QObject>

namespace app {

class SseBroadcaster : public QObject {
    Q_OBJECT

public:
    explicit SseBroadcaster(QObject *parent = nullptr);

public slots:
    void handleEvent(const app::AppEvent &event);

signals:
    void ssePayloadReady(quint64 uid, const QByteArray &payload);
};

} // namespace app
