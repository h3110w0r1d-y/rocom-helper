#pragma once

#include "events/app_event.h"
#include "rwtd/rwtd_types.h"

#include <QObject>

namespace rwtd {
class OpcodeFilter;
}

namespace app {

class DatabaseService;

class TrafficEventMapper : public QObject {
    Q_OBJECT

public:
    explicit TrafficEventMapper(QObject *parent = nullptr);

    void setOpcodeFilter(rwtd::OpcodeFilter *filter);
    void setDatabaseService(DatabaseService *database);

public slots:
    void mapDecodedAction(const rwtd::DecodedAction &action);

signals:
    void eventCreated(const app::AppEvent &event);

private:
    rwtd::OpcodeFilter *m_opcodeFilter = nullptr;
    DatabaseService *m_database = nullptr;
};

} // namespace app
