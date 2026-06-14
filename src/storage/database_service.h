#pragma once

#include "data/map_types.h"
#include "events/app_event.h"

#include <QByteArray>
#include <QHash>
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
    void resetMapMarkerVisibility();
    QJsonArray queryMapMarkers() const;
    QJsonArray queryPetInfo(quint64 uid) const;
    QJsonArray queryBoxInfo(quint64 uid) const;
    QJsonArray queryUsers() const;
    QHash<QString, QByteArray> allFlowKeys() const;
    quint64 uidForFlow(const QString &flowId) const;

public slots:
    void handleEvent(const app::AppEvent &event);
    void upsertMarker(const app::MapMarker &marker);
    void deleteMarker(const QString &markerId);
    void rememberFlowKey(const QString &flowId, const QByteArray &key);
    void registerUserLogin(const QString &flowId, quint64 uid, const QString &nameBase64, quint32 avatar);

signals:
    void errorOccurred(const QString &message);
    void usersChanged();

private:
    bool ensureSchema();
    void loadUsers();
    void ensureUserTables(quint64 uid);
    void handlePetInfoEvent(const app::AppEvent &event);
    void handleBoxInfoEvent(const app::AppEvent &event);
    void savePetInfo(quint64 uid, int petId, const QJsonObject &data);
    void replaceBoxes(quint64 uid, const QJsonArray &boxes);
    void replaceBox(quint64 uid, int boxId, const QJsonArray &data);
    void changeBoxSlot(quint64 uid, int boxId, int pos, int value);
    QString defaultDatabasePath() const;

    QString m_connectionName;
    QSqlDatabase m_db;
    QHash<QString, QByteArray> m_flowKeys;
    QHash<QString, quint64> m_flowUid;
};

} // namespace app
