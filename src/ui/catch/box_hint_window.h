#pragma once

#include <QJsonObject>
#include <QWidget>

class QLabel;
class QPushButton;

namespace app {

class OverlayHostController;

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
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

private:
    void clearHint();
    void resetCount();

    QLabel *m_kindLabel = nullptr;
    QLabel *m_attrLabel = nullptr;
    QLabel *m_countLabel = nullptr;
    QPushButton *m_resetCountButton = nullptr;
    OverlayHostController *m_overlayHost = nullptr;
    int m_boxHintId = 0;
};

} // namespace app
