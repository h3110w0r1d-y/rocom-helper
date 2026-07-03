#pragma once

#include <QJsonObject>
#include <QWidget>

class QLabel;
class QPushButton;

namespace app {

class BoxHintWindow : public QWidget {
    Q_OBJECT

public:
    explicit BoxHintWindow(QWidget *parent = nullptr);

public slots:
    void updateHint(const QJsonObject &payload);

signals:
    void resetRequested();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void clearHint();
    void resetCount();

    QLabel *m_kindLabel = nullptr;
    QLabel *m_attrLabel = nullptr;
    QLabel *m_countLabel = nullptr;
    QPushButton *m_resetCountButton = nullptr;
    int m_boxHintId = 0;
};

} // namespace app
