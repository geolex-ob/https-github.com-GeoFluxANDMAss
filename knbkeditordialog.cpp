#include "knbkeditordialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QCoreApplication>
#include <QMessageBox>
#include <QStringConverter>
#include <cmath>

KNBKEditorDialog::KNBKEditorDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Редактор справочника КНБК");
    resize(900, 500);

    auto *layout = new QVBoxLayout(this);

    auto *btnLayout = new QHBoxLayout();
    auto *addBtn = new QPushButton("+ Добавить", this);
    auto *delBtn = new QPushButton("- Удалить", this);
    auto *saveBtn = new QPushButton("Сохранить и закрыть", this);
    
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(delBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(saveBtn);

    m_table = new QTableWidget(0, 6, this);
    m_table->setHorizontalHeaderLabels({"Шифр", "Длина, м", "Наружный d, мм", "Внутренний d, мм", "Объем 1 м, л", "Вес 1 м, кг"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    layout->addLayout(btnLayout);
    layout->addWidget(m_table);

    connect(addBtn, &QPushButton::clicked, this, &KNBKEditorDialog::addRow);
    connect(delBtn, &QPushButton::clicked, this, &KNBKEditorDialog::removeRow);
    connect(saveBtn, &QPushButton::clicked, this, &KNBKEditorDialog::saveAndClose);
    connect(m_table, &QTableWidget::itemChanged, this, &KNBKEditorDialog::onTableItemChanged);

    loadCSV();
}

QString KNBKEditorDialog::getDataPath(const QString &fileName)
{
    QString appDir = QCoreApplication::applicationDirPath();
    QDir dataDir(appDir + "/data");
    if (!dataDir.exists()) dataDir.mkpath(".");
    return dataDir.filePath(fileName);
}

void KNBKEditorDialog::addRow()
{
    int row = m_table->rowCount();
    m_table->insertRow(row);
    for (int i = 0; i < m_table->columnCount(); ++i) {
        m_table->setItem(row, i, new QTableWidgetItem(""));
    }
    
    if (m_table->item(row, 4)) {
        m_table->item(row, 4)->setFlags(m_table->item(row, 4)->flags() | Qt::ItemIsEditable);
        m_table->item(row, 4)->setBackground(QBrush(Qt::white));
    }
    if (m_table->item(row, 5)) {
        m_table->item(row, 5)->setFlags(m_table->item(row, 5)->flags() & ~Qt::ItemIsEditable);
        m_table->item(row, 5)->setBackground(QBrush(Qt::lightGray));
    }
}

void KNBKEditorDialog::removeRow()
{
    int row = m_table->currentRow();
    if (row >= 0) {
        m_table->removeRow(row);
    }
}

void KNBKEditorDialog::onTableItemChanged(QTableWidgetItem *item)
{
    if (item && (item->column() == 2 || item->column() == 3)) {
        recalculateRow(item->row());
    }
}

void KNBKEditorDialog::recalculateRow(int row)
{
    if (!m_table->item(row, 2) || !m_table->item(row, 3)) return;

    bool ok1, ok2;
    double outerD_mm = m_table->item(row, 2)->text().replace(",", ".").toDouble(&ok1);
    double innerD_mm = m_table->item(row, 3)->text().replace(",", ".").toDouble(&ok2);

    if (ok1 && ok2 && outerD_mm > 0 && innerD_mm > 0) {
        double d_out = outerD_mm / 1000.0;
        double d_in = innerD_mm / 1000.0;

        double v_inner_m3 = M_PI * std::pow(d_in, 2) / 4.0;
        double v_inner_l = v_inner_m3 * 1000.0;

        double v_metal_m3 = M_PI * (std::pow(d_out, 2) - std::pow(d_in, 2)) / 4.0;
        double weight_kg = v_metal_m3 * 7850.0;

        m_table->blockSignals(true);
        
        if (m_table->item(row, 4) && m_table->item(row, 4)->text().isEmpty()) {
            m_table->item(row, 4)->setText(QString::number(v_inner_l, 'f', 2));
        }
        if (m_table->item(row, 5)) {
            m_table->item(row, 5)->setText(QString::number(weight_kg, 'f', 2));
        }
        
        m_table->blockSignals(false);
    } else {
        m_table->blockSignals(true);
        if (m_table->item(row, 4)) m_table->item(row, 4)->setText("");
        if (m_table->item(row, 5)) m_table->item(row, 5)->setText("");
        m_table->blockSignals(false);
    }
}

void KNBKEditorDialog::loadCSV()
{
    QFile file(getDataPath("knbk.csv"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    m_table->blockSignals(true); 
    m_table->setRowCount(0);
    if (!in.atEnd()) in.readLine(); 

    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList fields = line.split(";");
        
        int row = m_table->rowCount();
        m_table->insertRow(row);
        
        for (int col = 0; col < m_table->columnCount(); ++col) {
            QString val = (col < fields.size()) ? fields[col] : "";
            QTableWidgetItem *item = new QTableWidgetItem(val);
            
            if (col == 4) {
                item->setFlags(item->flags() | Qt::ItemIsEditable);
                item->setBackground(QBrush(Qt::white));
            } else if (col == 5) {
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                item->setBackground(QBrush(Qt::lightGray));
            }
            m_table->setItem(row, col, item);
        }
        
        if (fields.size() < 6 || fields[4].isEmpty() || fields[5].isEmpty()) {
            recalculateRow(row);
        }
    }
    m_table->blockSignals(false);
    file.close();
}

void KNBKEditorDialog::saveCSV()
{
    QFile file(getDataPath("knbk.csv"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось сохранить файл: " + getDataPath("knbk.csv"));
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    QStringList headers;
    for (int col = 0; col < m_table->columnCount(); ++col) {
        if (m_table->horizontalHeaderItem(col)) {
            headers << m_table->horizontalHeaderItem(col)->text();
        } else {
            headers << "";
        }
    }
    out << headers.join(";") << "\n";

    for (int row = 0; row < m_table->rowCount(); ++row) {
        QStringList rowData;
        for (int col = 0; col < m_table->columnCount(); ++col) {
            QTableWidgetItem *item = m_table->item(row, col);
            rowData << (item ? item->text() : "");
        }
        out << rowData.join(";") << "\n";
    }
    file.close();
}

void KNBKEditorDialog::saveAndClose()
{
    saveCSV();
    accept();
}