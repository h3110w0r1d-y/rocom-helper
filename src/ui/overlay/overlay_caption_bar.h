#pragma once

#include <QPointer>
#include <QPoint>
#include <QString>
#include <QWidget>

class QPushButton;

namespace app {

class OverlayHostController;

class OverlayCaptionBar : public QWidget {
    Q_OBJECT

public:
    explicit OverlayCaptionBar(OverlayHostController *controller, QWidget *hostWindow, QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setOverlayButtonEnabled(bool enabled);
    void setOverlayActive(bool active);
    void setOverlayButtonToolTip(const QString &toolTip);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    bool isDragArea(const QPoint &localPos) const;

    OverlayHostController *m_controller = nullptr;
    QPointer<QWidget> m_hostWindow;
    QPushButton *m_overlayButton = nullptr;
    QPushButton *m_closeButton = nullptr;
    QPoint m_dragStartGlobal;
    QPoint m_hostStartTopLeft;
    bool m_dragging = false;
};

} // namespace app
