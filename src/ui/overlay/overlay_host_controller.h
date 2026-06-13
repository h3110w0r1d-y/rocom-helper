#pragma once

#include <QObject>
#include <QPointer>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QWidget>

#include <functional>

class QAbstractNativeEventFilter;
class QEvent;

namespace app {

class OverlayCaptionBar;
class OverlayControlHandle;

struct OverlayHostOptions {
    QString title;
    bool staysOnTop = false;
    QPoint handleAnchorOffset{-28, -6};
    QSize handleSize{52, 24};
    std::function<bool()> canEnterOverlay;
    std::function<void(bool overlayEnabled)> onOverlayChanged;
};

class OverlayHostController : public QObject {
    Q_OBJECT

public:
    explicit OverlayHostController(QWidget *host, OverlayHostOptions options, QObject *parent = nullptr);
    ~OverlayHostController() override;

    void install();
    QWidget *captionBar() const;
    bool isOverlayEnabled() const;
    void setOverlayEnabled(bool enabled);
    void setStaysOnTop(bool enabled);
    bool staysOnTop() const;
    void setTitle(const QString &title);
    void refreshOverlayButton();
    void syncHandleGeometry();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void onHostMoveOrResize();
    void onHostShow();
    void onHostHide();

signals:
    void overlayEnabledChanged(bool enabled);

private:
    friend class OverlayCaptionBar;

    void applyDecoratedWindowFlags();
    void enterOverlay();
    void leaveOverlay();
    bool canEnterOverlayNow() const;

    QPointer<QWidget> m_host;
    OverlayHostOptions m_options;
    OverlayCaptionBar *m_captionBar = nullptr;
    OverlayControlHandle *m_handle = nullptr;
    bool m_overlayEnabled = false;
    bool m_staysOnTop = false;
    bool m_syncingGeometry = false;
    bool m_installed = false;
#ifdef Q_OS_WIN
    bool m_resizeBorderEnabled = true;
    QAbstractNativeEventFilter *m_resizeFilter = nullptr;
#endif
};

} // namespace app
