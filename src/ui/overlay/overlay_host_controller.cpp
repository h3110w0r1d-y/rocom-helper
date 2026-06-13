#include "overlay_host_controller.h"

#include "overlay_caption_bar.h"
#include "overlay_control_handle.h"
#include "win_mouse_passthrough.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QEvent>
#include <QWidget>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

namespace app {
namespace {

#ifdef Q_OS_WIN
constexpr int kResizeBorder = 8;

class FramelessResizeFilter final : public QAbstractNativeEventFilter {
public:
    FramelessResizeFilter(QWidget *host, bool *enabledFlag)
        : m_host(host)
        , m_enabledFlag(enabledFlag)
    {
    }

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override
    {
        if (m_enabledFlag == nullptr || !*m_enabledFlag || m_host == nullptr || eventType != "windows_generic_MSG") {
            return false;
        }

        auto *msg = static_cast<MSG *>(message);
        if (msg->message != WM_NCHITTEST) {
            return false;
        }

        const LONG x = GET_X_LPARAM(msg->lParam);
        const LONG y = GET_Y_LPARAM(msg->lParam);
        const QPoint local = m_host->mapFromGlobal(QPoint(x, y));
        const int width = m_host->width();
        const int height = m_host->height();

        const bool left = local.x() < kResizeBorder;
        const bool right = local.x() >= width - kResizeBorder;
        const bool top = local.y() < kResizeBorder;
        const bool bottom = local.y() >= height - kResizeBorder;

        if (top && left) {
            *result = HTTOPLEFT;
            return true;
        }
        if (top && right) {
            *result = HTTOPRIGHT;
            return true;
        }
        if (bottom && left) {
            *result = HTBOTTOMLEFT;
            return true;
        }
        if (bottom && right) {
            *result = HTBOTTOMRIGHT;
            return true;
        }
        if (left) {
            *result = HTLEFT;
            return true;
        }
        if (right) {
            *result = HTRIGHT;
            return true;
        }
        if (top) {
            *result = HTTOP;
            return true;
        }
        if (bottom) {
            *result = HTBOTTOM;
            return true;
        }

        return false;
    }

private:
    QPointer<QWidget> m_host;
    bool *m_enabledFlag = nullptr;
};
#endif

} // namespace

OverlayHostController::OverlayHostController(QWidget *host, OverlayHostOptions options, QObject *parent)
    : QObject(parent)
    , m_host(host)
    , m_options(std::move(options))
    , m_staysOnTop(m_options.staysOnTop)
{
}

OverlayHostController::~OverlayHostController()
{
#ifdef Q_OS_WIN
    if (m_overlayEnabled) {
        leaveOverlay();
    }
    if (m_handle != nullptr) {
        m_handle->close();
        m_handle = nullptr;
    }
    if (m_resizeFilter != nullptr) {
        qApp->removeNativeEventFilter(m_resizeFilter);
        delete m_resizeFilter;
        m_resizeFilter = nullptr;
    }
#endif
}

void OverlayHostController::install()
{
#ifdef Q_OS_WIN
    if (m_installed || m_host == nullptr) {
        return;
    }

    m_captionBar = new OverlayCaptionBar(this, m_host, m_host);
    m_captionBar->setTitle(m_options.title.isEmpty() ? m_host->windowTitle() : m_options.title);
    refreshOverlayButton();

    m_resizeFilter = new FramelessResizeFilter(m_host, &m_resizeBorderEnabled);
    qApp->installNativeEventFilter(m_resizeFilter);

    applyDecoratedWindowFlags();
    m_host->installEventFilter(this);
    m_host->show();

    m_installed = true;
#endif
}

bool OverlayHostController::eventFilter(QObject *watched, QEvent *event)
{
#ifdef Q_OS_WIN
    if (watched != m_host) {
        return QObject::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::Move:
    case QEvent::Resize:
        onHostMoveOrResize();
        break;
    case QEvent::Hide:
        onHostHide();
        break;
    case QEvent::Show:
        onHostShow();
        break;
    default:
        break;
    }
#endif

    return QObject::eventFilter(watched, event);
}

QWidget *OverlayHostController::captionBar() const
{
    return m_captionBar;
}

bool OverlayHostController::isOverlayEnabled() const
{
    return m_overlayEnabled;
}

void OverlayHostController::setOverlayEnabled(bool enabled)
{
#ifdef Q_OS_WIN
    if (enabled == m_overlayEnabled) {
        return;
    }

    if (enabled) {
        if (!canEnterOverlayNow()) {
            return;
        }
        enterOverlay();
    } else {
        leaveOverlay();
    }
#else
    Q_UNUSED(enabled);
#endif
}

void OverlayHostController::setStaysOnTop(bool enabled)
{
#ifdef Q_OS_WIN
    if (m_staysOnTop == enabled) {
        return;
    }

    m_staysOnTop = enabled;
    m_options.staysOnTop = enabled;

    if (m_overlayEnabled) {
        applyDecoratedWindowFlags();
        overlay::setMousePassthrough(m_host, true);
        m_host->show();
        syncHandleGeometry();
        if (m_handle != nullptr) {
            m_handle->setWindowFlag(Qt::WindowStaysOnTopHint, enabled);
            m_handle->show();
        }
    } else {
        applyDecoratedWindowFlags();
        m_host->show();
    }
#else
    Q_UNUSED(enabled);
#endif
}

bool OverlayHostController::staysOnTop() const
{
    return m_staysOnTop;
}

void OverlayHostController::setTitle(const QString &title)
{
    m_options.title = title;
    if (m_captionBar != nullptr) {
        m_captionBar->setTitle(title);
    }
}

void OverlayHostController::refreshOverlayButton()
{
#ifdef Q_OS_WIN
    if (m_captionBar == nullptr) {
        return;
    }

    const bool canEnter = canEnterOverlayNow();
    m_captionBar->setOverlayButtonEnabled(canEnter || m_overlayEnabled);
    m_captionBar->setOverlayActive(m_overlayEnabled);
#endif
}

void OverlayHostController::syncHandleGeometry()
{
#ifdef Q_OS_WIN
    if (m_handle == nullptr || m_host == nullptr || m_syncingGeometry) {
        return;
    }

    m_syncingGeometry = true;
    m_handle->syncToHost();
    m_handle->raise();
    m_syncingGeometry = false;
#endif
}

void OverlayHostController::onHostMoveOrResize()
{
#ifdef Q_OS_WIN
    if (m_overlayEnabled) {
        syncHandleGeometry();
    }
#endif
}

void OverlayHostController::onHostShow()
{
#ifdef Q_OS_WIN
    if (m_overlayEnabled && m_handle != nullptr) {
        syncHandleGeometry();
        m_handle->show();
    }
#endif
}

void OverlayHostController::onHostHide()
{
#ifdef Q_OS_WIN
    if (m_handle != nullptr) {
        m_handle->hide();
    }
#endif
}

void OverlayHostController::applyDecoratedWindowFlags()
{
#ifdef Q_OS_WIN
    if (m_host == nullptr) {
        return;
    }

    Qt::WindowFlags flags = Qt::Window | Qt::FramelessWindowHint;
    if (m_staysOnTop) {
        flags |= Qt::WindowStaysOnTopHint;
    }
    m_host->setWindowFlags(flags);
#endif
}

void OverlayHostController::enterOverlay()
{
#ifdef Q_OS_WIN
    if (m_host == nullptr) {
        return;
    }

    if (m_captionBar != nullptr) {
        m_captionBar->hide();
    }

    applyDecoratedWindowFlags();
    m_host->show();
    overlay::setMousePassthrough(m_host, true);

    if (m_handle == nullptr) {
        m_handle = new OverlayControlHandle(m_host, m_options.handleSize);
        m_handle->setAnchorOffset(m_options.handleAnchorOffset);
        connect(m_handle, &OverlayControlHandle::exitOverlayRequested, this, [this] {
            setOverlayEnabled(false);
        });
        connect(m_handle, &OverlayControlHandle::destroyed, this, [this] {
            m_handle = nullptr;
        });
    }

    if (m_staysOnTop) {
        m_handle->setWindowFlag(Qt::WindowStaysOnTopHint, true);
    }

    syncHandleGeometry();
    m_handle->show();
    m_handle->raise();

    m_resizeBorderEnabled = false;

    m_overlayEnabled = true;
    refreshOverlayButton();

    if (m_options.onOverlayChanged) {
        m_options.onOverlayChanged(true);
    }
    emit overlayEnabledChanged(true);
#endif
}

void OverlayHostController::leaveOverlay()
{
#ifdef Q_OS_WIN
    if (m_host == nullptr) {
        return;
    }

    overlay::setMousePassthrough(m_host, false);

    if (m_handle != nullptr) {
        m_handle->hide();
    }

    applyDecoratedWindowFlags();
    m_host->show();

    if (m_captionBar != nullptr) {
        m_captionBar->show();
    }

    m_resizeBorderEnabled = true;

    m_overlayEnabled = false;
    refreshOverlayButton();

    if (m_options.onOverlayChanged) {
        m_options.onOverlayChanged(false);
    }
    emit overlayEnabledChanged(false);
#endif
}

bool OverlayHostController::canEnterOverlayNow() const
{
    if (m_options.canEnterOverlay) {
        return m_options.canEnterOverlay();
    }
    return true;
}

} // namespace app
