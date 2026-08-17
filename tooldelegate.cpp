#include "tooldelegate.h"
#include "tool.h"

ToolCalcMethodDelegate::ToolCalcMethodDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

QWidget *ToolCalcMethodDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    QComboBox *comboBox = new QComboBox(parent);
    comboBox->addItem("По плотности");
    comboBox->addItem("По размерам");
    return comboBox;
}

void ToolCalcMethodDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const {
    QComboBox *comboBox = qobject_cast<QComboBox*>(editor);
    if (comboBox) {
        int value = index.model()->data(index, Qt::UserRole).toInt();
        comboBox->setCurrentIndex(value);
    }
}

void ToolCalcMethodDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const {
    QComboBox *comboBox = qobject_cast<QComboBox*>(editor);
    if (comboBox) {
        model->setData(index, comboBox->currentIndex(), Qt::EditRole);
    }
}

void ToolCalcMethodDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    editor->setGeometry(option.rect);
}
