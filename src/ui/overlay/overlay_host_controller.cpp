#include "overlay_host_controller.h"

#include "overlay_caption_bar.h"
#include "overlay_control_handle.h"
#include "win_frameless_decorations.h"
#include "win_mouse_passthrough.h"

#include <QEvent>
#include <QWidget>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace app {

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

    applyDecoratedWindowFlags();
    m_host->installEventFilter(this);
    m_host->show();
    refreshWindowChrome();

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
        refreshWindowChrome();
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

void OverlayHostController::refreshWindowChrome()
{
#ifdef Q_OS_WIN
    if (m_host != nullptr && !m_overlayEnabled) {
        overlay::refreshWindowChrome(m_host);
    }
#endif
}

bool OverlayHostController::handleNativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    if (!m_installed || m_overlayEnabled || !m_resizeBorderEnabled || m_host == nullptr) {
        return false;
    }
    if (eventType != "windows_generic_MSG" || message == nullptr || result == nullptr) {
        return false;
    }

    auto *msg = static_cast<MSG *>(message);
    if (msg->message != WM_NCHITTEST) {
        return false;
    }

    const WId windowId = m_host->winId();
    if (windowId == 0 || msg->hwnd != reinterpret_cast<HWND>(windowId)) {
        return false;
    }

    const LRESULT hit = overlay::hitTestResizeBorder(reinterpret_cast<HWND>(windowId), msg->lParam);
    if (hit == HTCLIENT) {
        return false;
    }

    *result = hit;
    return true;
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
    return false;
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
        return;
    }

    refreshWindowChrome();
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
    m_host->setAttribute(Qt::WA_TranslucentBackground, false);
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
    refreshWindowChrome();

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
