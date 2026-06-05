#pragma once

#include "data/map_types.h"

#include <QTreeWidget>

namespace app {

class MarkerFilterPanel : public QTreeWidget {
    Q_OBJECT

public:
    explicit MarkerFilterPanel(QWidget *parent = nullptr);

    void setMarkerTypes(const MarkerTypeMap &markerTypes);

signals:
    void typeVisibilityChanged(const QString &markerType, bool visible);
    void subtypeVisibilityChanged(const QString &markerType, const QString &subtype, bool visible);

private:
    void onItemChanged(QTreeWidgetItem *item, int column);
    void updateChildrenEnabled(QTreeWidgetItem *parentItem, bool enabled);
    static QString subtypeDisplayName(const QString &subtype);

    bool m_updating = false;
};

} // namespace app
