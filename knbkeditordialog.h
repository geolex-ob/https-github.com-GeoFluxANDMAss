#pragma once

#include <QDialog>
#include <QTableWidget>

class KNBKEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit KNBKEditorDialog(QWidget *parent = nullptr);

private slots:
    void addRow();
    void removeRow();
    void saveAndClose();
    void onTableItemChanged(QTableWidgetItem *item);

private:
    QTableWidget *m_table;
    QString getDataPath(const QString &fileName);
    void loadCSV();
    void saveCSV();
    void recalculateRow(int row);
};