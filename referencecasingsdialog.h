// referencecasingsdialog.h
#ifndef REFERENCECASINGSDIALOG_H
#define REFERENCECASINGSDIALOG_H
#include <QDialog>
#include <QTableWidget>
class ReferenceCasingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit ReferenceCasingsDialog(QWidget *parent = nullptr);
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