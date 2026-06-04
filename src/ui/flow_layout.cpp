#include "flow_layout.h"

#include <QLayoutItem>
#include <QResizeEvent>
#include <QSizePolicy>

namespace app {

FlowLayout::FlowLayout(QWidget *parent, int margin, int spacing)
    : QLayout(parent)
{
    setContentsMargins(margin, margin, margin, margin);
    setSpacing(spacing);
}

FlowLayout::~FlowLayout()
{
    QLayoutItem *item = nullptr;
    while ((item = takeAt(0)) != nullptr) {
        delete item;
    }
}

void FlowLayout::addItem(QLayoutItem *item)
{
    m_items.append(item);
    invalidate();
}

int FlowLayout::count() const
{
    return m_items.size();
}

QLayoutItem *FlowLayout::itemAt(int index) const
{
    return index >= 0 && index < m_items.size() ? m_items.at(index) : nullptr;
}

QLayoutItem *FlowLayout::takeAt(int index)
{
    if (index < 0 || index >= m_items.size()) {
        return nullptr;
    }
    QLayoutItem *item = m_items.takeAt(index);
    invalidate();
    return item;
}

Qt::Orientations FlowLayout::expandingDirections() const
{
    return {};
}

bool FlowLayout::hasHeightForWidth() const
{
    return true;
}

int FlowLayout::heightForWidth(int width) const
{
    return doLayout(QRect(0, 0, width, 0), true);
}

void FlowLayout::setGeometry(const QRect &rect)
{
    QLayout::setGeometry(rect);
    doLayout(rect, false);
}

QSize FlowLayout::sizeHint() const
{
    return minimumSize();
}

QSize FlowLayout::minimumSize() const
{
    QSize size;
    for (QLayoutItem *item : m_items) {
        size = size.expandedTo(item->minimumSize());
    }
    const QMargins margins = contentsMargins();
    size += QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
    return size;
}

int FlowLayout::doLayout(const QRect &rect, bool testOnly) const
{
    const QMargins margins = contentsMargins();
    const QRect effectiveRect = rect.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom());
    int x = effectiveRect.x();
    int y = effectiveRect.y();
    int lineHeight = 0;

    for (QLayoutItem *item : m_items) {
        QWidget *widget = item->widget();
        if (widget != nullptr && widget->isHidden()) {
            continue;
        }
        const QSize itemSize = item->sizeHint();
        const int nextX = x + itemSize.width() + spacing();
        if (nextX - spacing() > effectiveRect.right() && lineHeight > 0) {
            x = effectiveRect.x();
            y += lineHeight + spacing();
            lineHeight = 0;
        }
        if (!testOnly) {
            item->setGeometry(QRect(QPoint(x, y), itemSize));
        }
        x += itemSize.width() + spacing();
        lineHeight = qMax(lineHeight, itemSize.height());
    }
    return y + lineHeight - rect.y() + margins.bottom();
}

FlowWidget::FlowWidget(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

bool FlowWidget::hasHeightForWidth() const
{
    const QLayout *layout = this->layout();
    return layout != nullptr && layout->hasHeightForWidth();
}

int FlowWidget::heightForWidth(int width) const
{
    const QLayout *layout = this->layout();
    if (layout != nullptr && layout->hasHeightForWidth()) {
        return layout->heightForWidth(width);
    }
    return QWidget::heightForWidth(width);
}

QSize FlowWidget::sizeHint() const
{
    QSize hint = QWidget::sizeHint();
    if (hasHeightForWidth() && width() > 0) {
        hint.setHeight(heightForWidth(width()));
    }
    return hint;
}

void FlowWidget::refreshHeight()
{
    if (!hasHeightForWidth() || width() <= 0) {
        return;
    }
    const int height = heightForWidth(width());
    if (height <= 0) {
        return;
    }
    if (minimumHeight() != height || maximumHeight() != height) {
        setMinimumHeight(height);
        setMaximumHeight(height);
        updateGeometry();
        if (parentWidget() != nullptr && parentWidget()->layout() != nullptr) {
            parentWidget()->layout()->invalidate();
        }
    }
}

void FlowWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    refreshHeight();
}

} // namespace app
