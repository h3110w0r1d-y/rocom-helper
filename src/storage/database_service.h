#pragma once

#include "events/app_event.h"

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
    QJsonObject queryExampleState() const;

public slots:
    void handleEvent(const app::AppEvent &event);

signals:
    void eventPersisted(const app::AppEvent &event, qint64 id);
    void errorOccurred(const QString &message);

private:
    bool ensureSchema();
    qint64 insertEvent(const app::AppEvent &event);
    QString defaultDatabasePath() const;

    QString m_connectionName;
    QSqlDatabase m_db;
};

} // namespace app

