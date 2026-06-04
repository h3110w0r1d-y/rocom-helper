#pragma once

#include <QLayout>
#include <QWidget>

namespace app {

class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget *parent = nullptr, int margin = 0, int spacing = 6);
    ~FlowLayout() override;

    void addItem(QLayoutItem *item) override;
    int count() const override;
    QLayoutItem *itemAt(int index) const override;
    QLayoutItem *takeAt(int index) override;
    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    void setGeometry(const QRect &rect) override;
    QSize sizeHint() const override;
    QSize minimumSize() const override;

private:
    int doLayout(const QRect &rect, bool testOnly) const;

    QList<QLayoutItem *> m_items;
};

class FlowWidget : public QWidget {
    Q_OBJECT

public:
    explicit FlowWidget(QWidget *parent = nullptr);

    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    QSize sizeHint() const override;
    void refreshHeight();

protected:
    void resizeEvent(QResizeEvent *event) override;
};

} // namespace app
