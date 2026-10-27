// referencetoolsdialog.cpp
#include "referencetoolsdialog.h"
#include "database.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMessageBox>
#include <QHeaderView>

ReferenceToolsDialog::ReferenceToolsDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Справочник инструментов (КНБК)");
    resize(800, 500);
    setupUI();
    loadData();
}

void ReferenceToolsDialog::setupUI() {
    QVBoxLayout *layout = new QVBoxLayout(this);
    tableWidget = new QTableWidget();
    tableWidget->setColumnCount(5);
    tableWidget->setHorizontalHeaderLabels(
        {"Название (Шифр)", "Наружный диаметр, мм",
         "Внутренний диаметр, мм", "Масса 1 м, кг", "Объём 1 м, л"});
    tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableWidget->setAlternatingRowColors(true);
    tableWidget->horizontalHeader()->setStretchLastSection(true);

    QHBoxLayout *btns = new QHBoxLayout();
    QPushButton *bAdd = new QPushButton("➕ Добавить");
    QPushButton *bDel = new QPushButton("🗑️ Удалить");
    QPushButton *bSave = new QPushButton("💾 Сохранить");
    QPushButton *bClose = new QPushButton("Закрыть");
    btns->addWidget(bAdd); btns->addWidget(bDel);
    btns->addWidget(bSave); btns->addStretch(); btns->addWidget(bClose);

    layout->addWidget(tableWidget);
    layout->addLayout(btns);

    connect(bAdd, &QPushButton::clicked, this, &ReferenceToolsDialog::addRow);
    connect(bDel, &QPushButton::clicked, this, &ReferenceToolsDialog::deleteRow);
    connect(bSave, &QPushButton::clicked, this, &ReferenceToolsDialog::saveChanges);
    connect(bClose, &QPushButton::clicked, this, &QDialog::accept);
}

void ReferenceToolsDialog::loadData() {
    const auto& tools = Database::instance().getTools();
    tableWidget->setRowCount(tools.size());
    for (int i = 0; i < tools.size(); ++i) {
        tableWidget->setItem(i, 0, new QTableWidgetItem(tools[i].name));
        tableWidget->setItem(i, 1, new QTableWidgetItem(QString::number(tools[i].outerDiameter)));
        tableWidget->setItem(i, 2, new QTableWidgetItem(QString::number(tools[i].innerDiameter)));
        tableWidget->setItem(i, 3, new QTableWidgetItem(QString::number(tools[i].massPerMeter)));
        tableWidget->setItem(i, 4, new QTableWidgetItem(QString::number(tools[i].volumePerMeter)));
    }
}

void ReferenceToolsDialog::addRow() {
    int r = tableWidget->rowCount();
    tableWidget->insertRow(r);
    tableWidget->setItem(r, 0, new QTableWidgetItem("Новый инструмент"));
    tableWidget->setItem(r, 1, new QTableWidgetItem("89.0"));
    tableWidget->setItem(r, 2, new QTableWidgetItem("76.0"));
    tableWidget->setItem(r, 3, new QTableWidgetItem("13.4"));
    tableWidget->setItem(r, 4, new QTableWidgetItem("1.707"));
}

void ReferenceToolsDialog::deleteRow() {
    int r = tableWidget->currentRow();
    if (r >= 0) tableWidget->removeRow(r);
    else QMessageBox::warning(this, "Внимание", "Выберите строку");
}

void ReferenceToolsDialog::saveChanges() {
    QVector<ToolRecord> newTools;
    int id = 1;
    for (int r = 0; r < tableWidget->rowCount(); ++r) {
        auto get = [&](int c){ auto* it = tableWidget->item(r,c); return it?it->text():QString(); };
        ToolRecord t;
        t.id = id++;
        t.name = get(0);
        t.outerDiameter = get(1).toDouble();
        t.innerDiameter = get(2).toDouble();
        t.massPerMeter = get(3).toDouble();
        t.volumePerMeter = get(4).toDouble();
        newTools.append(t);
    }
    Database::instance().getTools() = newTools;
    Database::instance().saveTools();
    accept();
}