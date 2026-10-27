#include "referencecasingsdialog.h"
#include "database.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMessageBox>
#include <QHeaderView>

ReferenceCasingsDialog::ReferenceCasingsDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Справочник обсадных труб");
    resize(900, 500);
    setupUI();
    loadData();
}

void ReferenceCasingsDialog::setupUI() {
    QVBoxLayout *layout = new QVBoxLayout(this);
    tableWidget = new QTableWidget();
    tableWidget->setColumnCount(5);
    tableWidget->setHorizontalHeaderLabels(
        {"Шифр", "Наружный диаметр, мм",
         "Толщина стенки, мм", "Масса 1 м, кг", "Примечание"});
    tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableWidget->setAlternatingRowColors(true);
    tableWidget->horizontalHeader()->setStretchLastSection(true);

    QHBoxLayout *btns = new QHBoxLayout();
    QPushButton *bAdd = new QPushButton("Добавить");
    QPushButton *bDel = new QPushButton("Удалить");
    QPushButton *bSave = new QPushButton("Сохранить");
    QPushButton *bClose = new QPushButton("Закрыть");
    btns->addWidget(bAdd); btns->addWidget(bDel);
    btns->addWidget(bSave); btns->addStretch(); btns->addWidget(bClose);

    layout->addWidget(tableWidget);
    layout->addLayout(btns);

    connect(bAdd, &QPushButton::clicked, this, &ReferenceCasingsDialog::addRow);
    connect(bDel, &QPushButton::clicked, this, &ReferenceCasingsDialog::deleteRow);
    connect(bSave, &QPushButton::clicked, this, &ReferenceCasingsDialog::saveChanges);
    connect(bClose, &QPushButton::clicked, this, &QDialog::accept);
}

void ReferenceCasingsDialog::loadData() {
    const auto& casings = Database::instance().getCasings();
    tableWidget->setRowCount(casings.size());
    for (int i = 0; i < casings.size(); ++i) {
        tableWidget->setItem(i, 0, new QTableWidgetItem(casings[i].name));
        tableWidget->setItem(i, 1, new QTableWidgetItem(QString::number(casings[i].outerDiameter)));
        tableWidget->setItem(i, 2, new QTableWidgetItem(QString::number(casings[i].wallThickness)));
        tableWidget->setItem(i, 3, new QTableWidgetItem(QString::number(casings[i].massPerMeter)));
        tableWidget->setItem(i, 4, new QTableWidgetItem(casings[i].note));
    }
}

void ReferenceCasingsDialog::addRow() {
    int r = tableWidget->rowCount();
    tableWidget->insertRow(r);
    tableWidget->setItem(r, 0, new QTableWidgetItem("Новая труба"));
    tableWidget->setItem(r, 1, new QTableWidgetItem("245.0"));
    tableWidget->setItem(r, 2, new QTableWidgetItem("10.0"));
    tableWidget->setItem(r, 3, new QTableWidgetItem("59.5"));
    tableWidget->setItem(r, 4, new QTableWidgetItem(""));
}

void ReferenceCasingsDialog::deleteRow() {
    int r = tableWidget->currentRow();
    if (r >= 0) tableWidget->removeRow(r);
    else QMessageBox::warning(this, "Внимание", "Выберите строку");
}

void ReferenceCasingsDialog::saveChanges() {
    QVector<CasingRecord> newCasings;
    int id = 1;
    for (int r = 0; r < tableWidget->rowCount(); ++r) {
        auto get = [&](int c){ auto* it = tableWidget->item(r,c); return it?it->text():QString(); };
        CasingRecord c;
        c.id = id++;
        c.name = get(0);
        c.outerDiameter = get(1).toDouble();
        c.wallThickness = get(2).toDouble();
        c.massPerMeter = get(3).toDouble();
        c.note = get(4);
        newCasings.append(c);
    }
    Database::instance().getCasings() = newCasings;
    Database::instance().saveCasings();
    accept();
}