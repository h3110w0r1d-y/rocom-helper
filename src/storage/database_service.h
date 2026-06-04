#pragma once

#include "data/map_types.h"
#include "events/app_event.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QSqlDatabase>
#include <QString>

namespace app {

class DatabaseService : public QObject {
    Q_OBJECT

public:
    explicit DatabaseService(QObject *parent = nullptr);
    ~DatabaseService() override;

    bool open(const QString &path = {});
    bool isOpen() const;
    QJsonArray queryMapMarkers() const;
    QJsonArray queryPetInfo() const;
    QJsonArray queryBoxInfo() const;

public slots:
    void handleEvent(const app::AppEvent &event);
    void upsertMarker(const app::MapMarker &marker);
    void deleteMarker(const QString &markerId);

signals:
    void errorOccurred(const QString &message);

private:
    bool ensureSchema();
    void handlePetInfoEvent(const app::AppEvent &event);
    void handleBoxInfoEvent(const app::AppEvent &event);
    void savePetInfo(int petId, const QJsonObject &data);
    void replaceBoxes(const QJsonArray &boxes);
    void changeBoxSlot(int boxId, int pos, int value);
    QString defaultDatabasePath() const;

    QString m_connectionName;
    QSqlDatabase m_db;
};

} // namespace app
