#ifndef TOOLDELEGATE_H
#define TOOLDELEGATE_H

#include <QStyledItemDelegate>
#include <QComboBox>

class ToolCalcMethodDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit ToolCalcMethodDelegate(QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

#endif // TOOLDELEGATE_H
