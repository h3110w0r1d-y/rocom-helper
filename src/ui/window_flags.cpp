#include "window_flags.h"

#include <QEvent>
#include <QPointer>
#include <QTimer>

#ifdef Q_OS_MACOS
#include <objc/message.h>
#include <objc/runtime.h>
#endif

namespace app {
namespace {

#ifdef Q_OS_MACOS

enum MacWindowButton {
    MacWindowCloseButton = 0,
    MacWindowMiniaturizeButton = 1,
    MacWindowZoomButton = 2,
};

void setMacStandardButtonHidden(QWidget *window, MacWindowButton button, bool hidden)
{
    if (window == nullptr) {
        return;
    }

    const WId windowId = window->winId();
    auto *view = reinterpret_cast<void *>(windowId);
    if (view == nullptr) {
        return;
    }

    auto sendObject = reinterpret_cast<void *(*)(void *, SEL)>(objc_msgSend);
    void *nativeWindow = sendObject(view, sel_registerName("window"));
    if (nativeWindow == nullptr) {
        return;
    }

    auto sendButton = reinterpret_cast<void *(*)(void *, SEL, long)>(objc_msgSend);
    void *nativeButton = sendButton(nativeWindow, sel_registerName("standardWindowButton:"), button);
    if (nativeButton == nullptr) {
        return;
    }

    auto sendBool = reinterpret_cast<void (*)(void *, SEL, bool)>(objc_msgSend);
    sendBool(nativeButton, sel_registerName("setHidden:"), hidden);
}

void applyMacCloseOnlyButtons(QWidget *window)
{
    setMacStandardButtonHidden(window, MacWindowMiniaturizeButton, true);
    setMacStandardButtonHidden(window, MacWindowZoomButton, true);
    setMacStandardButtonHidden(window, MacWindowCloseButton, false);
}

void scheduleApplyMacCloseOnlyButtons(QWidget *window)
{
    if (window == nullptr || !window->isVisible()) {
        return;
    }

    QPointer<QWidget> guardedWindow(window);
    QTimer::singleShot(0, window, [guardedWindow] {
        if (guardedWindow != nullptr && guardedWindow->isVisible()) {
            applyMacCloseOnlyButtons(guardedWindow);
        }
    });
    QTimer::singleShot(50, window, [guardedWindow] {
        if (guardedWindow != nullptr && guardedWindow->isVisible()) {
            applyMacCloseOnlyButtons(guardedWindow);
        }
    });
}

class MacCloseOnlyButtonFilter final : public QObject {
public:
    explicit MacCloseOnlyButtonFilter(QWidget *window)
        : QObject(window)
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        auto *window = qobject_cast<QWidget *>(watched);
        if (window == nullptr) {
            return QObject::eventFilter(watched, event);
        }

        switch (event->type()) {
        case QEvent::Show:
            scheduleApplyMacCloseOnlyButtons(window);
            break;
        default:
            break;
        }

        return QObject::eventFilter(watched, event);
    }
};

#endif

} // namespace

Qt::WindowFlags closeOnlyWindowFlags(bool staysOnTop)
{
    Qt::WindowFlags flags = Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint;
    if (staysOnTop) {
        flags |= Qt::WindowStaysOnTopHint;
    }
    return flags;
}

void setCloseOnlyWindowControls(QWidget *window, bool staysOnTop)
{
    if (window == nullptr) {
        return;
    }

    window->setWindowFlags(closeOnlyWindowFlags(staysOnTop));

#ifdef Q_OS_MACOS
    constexpr const char *filterProperty = "_rocoCloseOnlyButtonFilterInstalled";
    if (!window->property(filterProperty).toBool()) {
        window->installEventFilter(new MacCloseOnlyButtonFilter(window));
        window->setProperty(filterProperty, true);
    }
    if (window->isVisible()) {
        scheduleApplyMacCloseOnlyButtons(window);
    }
#endif
}

} // namespace app
