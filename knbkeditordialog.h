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
    // Метка "пользователь ввёл значение вручную" (не перетирать авто-расчётом).
    // Значения флагов < Qt::ItemIsUserMinFlag — системные, поэтому используем
    // пользовательские роли data(), а не флаги.
    static int const ManualMassRole = Qt::UserRole + 101;
    static int const ManualVolRole  = Qt::UserRole + 102;
    bool isManual(QTableWidgetItem *item, int role) const;
    void setManual(QTableWidgetItem *item, int role, bool manual);
};