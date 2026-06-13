#pragma once

#include <QPointer>
#include <QPoint>
#include <QSize>
#include <QWidget>

namespace app {

class OverlayControlHandle : public QWidget {
    Q_OBJECT

public:
    explicit OverlayControlHandle(QWidget *hostWindow, const QSize &size, QWidget *parent = nullptr);

    void setAnchorOffset(const QPoint &anchorOffset);
    void syncToHost();

signals:
    void exitOverlayRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    bool hitExitButton(const QPoint &localPos) const;
    QRect exitButtonRect() const;
    QRect dragAreaRect() const;

    QPointer<QWidget> m_hostWindow;
    QPoint m_anchorOffset;
    QPoint m_dragStartGlobal;
    QPoint m_hostStartTopLeft;
    bool m_dragging = false;
};

} // namespace app
