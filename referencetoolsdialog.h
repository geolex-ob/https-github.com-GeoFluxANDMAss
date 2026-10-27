// referencetoolsdialog.h
#ifndef REFERENCETOOLSDIALOG_H
#define REFERENCETOOLSDIALOG_H
#include <QDialog>
#include <QTableWidget>
class ReferenceToolsDialog : public QDialog {
    Q_OBJECT
public:
    explicit ReferenceToolsDialog(QWidget *parent = nullptr);
private slots:
    void addRow();
    void deleteRow();
    void saveChanges();
private:
    void setupUI();
    void loadData();
    QTableWidget *tableWidget;
};
#endif