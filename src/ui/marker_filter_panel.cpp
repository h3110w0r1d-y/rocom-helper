#include "marker_filter_panel.h"

#include <QHeaderView>
#include <QHash>
#include <QIcon>
#include <QSignalBlocker>

namespace app {
namespace {

constexpr int MarkerTypeRole = Qt::UserRole + 1;
constexpr int MarkerSubtypeRole = Qt::UserRole + 2;
constexpr int IsTypeItemRole = Qt::UserRole + 3;

} // namespace

MarkerFilterPanel::MarkerFilterPanel(QWidget *parent)
    : QTreeWidget(parent)
{
    setColumnCount(1);
    setHeaderLabel(QStringLiteral("点位显示"));
    setFixedHeight(150);
    setRootIsDecorated(true);
    setAlternatingRowColors(true);
    setUniformRowHeights(true);
    setIndentation(16);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    header()->setStretchLastSection(true);

    connect(this, &QTreeWidget::itemChanged, this, &MarkerFilterPanel::onItemChanged);
}

void MarkerFilterPanel::setMarkerTypes(const MarkerTypeMap &markerTypes)
{
    QSignalBlocker blocker(this);
    m_updating = true;

    QHash<QString, bool> expandedStates;
    for (int index = 0; index < topLevelItemCount(); ++index) {
        QTreeWidgetItem *item = topLevelItem(index);
        if (item != nullptr) {
            expandedStates.insert(item->data(0, MarkerTypeRole).toString(), item->isExpanded());
        }
    }

    clear();

    for (const MarkerTypeConfig &markerType : markerTypes) {
        auto *typeItem = new QTreeWidgetItem(this);
        typeItem->setText(0, markerType.name);
        typeItem->setIcon(0, QIcon(QStringLiteral(":/icon/") + markerType.icon));
        typeItem->setData(0, MarkerTypeRole, markerType.key);
        typeItem->setData(0, IsTypeItemRole, true);
        typeItem->setFlags(typeItem->flags() | Qt::ItemIsUserCheckable);
        typeItem->setCheckState(0, markerType.visible ? Qt::Checked : Qt::Unchecked);

        for (const MarkerSubtypeConfig &subtype : markerType.subtypes) {
            auto *subtypeItem = new QTreeWidgetItem(typeItem);
            subtypeItem->setText(0, subtypeDisplayName(subtype.key));
            subtypeItem->setToolTip(0, QStringLiteral("%1/%2").arg(markerType.name, subtypeDisplayName(subtype.key)));
            subtypeItem->setData(0, MarkerTypeRole, markerType.key);
            subtypeItem->setData(0, MarkerSubtypeRole, subtype.key);
            subtypeItem->setData(0, IsTypeItemRole, false);
            subtypeItem->setFlags(subtypeItem->flags() | Qt::ItemIsUserCheckable);
            subtypeItem->setCheckState(0, subtype.visible ? Qt::Checked : Qt::Unchecked);
        }

        if (typeItem->childCount() > 0) {
            typeItem->setExpanded(markerType.visible && expandedStates.value(markerType.key, true));
            updateChildrenEnabled(typeItem, markerType.visible);
        }
    }

    m_updating = false;
}

void MarkerFilterPanel::onItemChanged(QTreeWidgetItem *item, int column)
{
    if (m_updating || item == nullptr || column != 0) {
        return;
    }

    const bool isTypeItem = item->data(0, IsTypeItemRole).toBool();
    const QString markerType = item->data(0, MarkerTypeRole).toString();
    if (markerType.isEmpty()) {
        return;
    }

    if (isTypeItem) {
        const Qt::CheckState state = item->checkState(0);
        const bool visible = state == Qt::Checked;
        m_updating = true;
        updateChildrenEnabled(item, visible);
        item->setExpanded(visible);
        m_updating = false;
        emit typeVisibilityChanged(markerType, visible);
        return;
    }

    const QString subtype = item->data(0, MarkerSubtypeRole).toString();
    const bool visible = item->checkState(0) == Qt::Checked;
    emit subtypeVisibilityChanged(markerType, subtype, visible);
}

void MarkerFilterPanel::updateChildrenEnabled(QTreeWidgetItem *parentItem, bool enabled)
{
    if (parentItem == nullptr) {
        return;
    }
    for (int index = 0; index < parentItem->childCount(); ++index) {
        QTreeWidgetItem *child = parentItem->child(index);
        child->setDisabled(!enabled);
    }
}

QString MarkerFilterPanel::subtypeDisplayName(const QString &subtype)
{
    return subtype.isEmpty() ? QStringLiteral("默认") : subtype;
}

} // namespace app
