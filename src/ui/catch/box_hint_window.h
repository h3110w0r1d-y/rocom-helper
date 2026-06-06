#pragma once

#include <QJsonObject>
#include <QWidget>

class QLabel;

namespace app {

class DataCenter;

class BoxHintWindow : public QWidget {
    Q_OBJECT

public:
    explicit BoxHintWindow(DataCenter *dataCenter, QWidget *parent = nullptr);

public slots:
    void updateHint(const QJsonObject &payload);

private:
    void clearHint();

    QLabel *m_kindLabel = nullptr;
    QLabel *m_attrLabel = nullptr;
};

} // namespace app
