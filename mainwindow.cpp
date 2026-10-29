#include "mainwindow.h"
#include "knbkeditordialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QCoreApplication>
#include <QMessageBox>
#include <QMenuBar>
#include <QStatusBar>
#include <QStringConverter>
#include <cmath>
#include <algorithm>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("Расчеты ГТИ");
    resize(1200, 800);

    auto *fileMenu = menuBar()->addMenu("Файл");
    fileMenu->addAction("Сохранить конструкцию", this, &MainWindow::saveConstruction);
    fileMenu->addSeparator();
    fileMenu->addAction("Выход", this, &QWidget::close);

    auto *refMenu = menuBar()->addMenu("Справочники");
    refMenu->addAction("Редактор КНБК", this, &MainWindow::openKNBKEditor);
    
    auto *helpMenu = menuBar()->addMenu("О программе");
    helpMenu->addAction("О программе", [this](){
        QMessageBox::about(this, "О программе", "ПGeoFluxANDMAss.\nВерсия 0.0.2");
    });

    statusBar()->showMessage("Готово");

    setupUI();
    
    loadKNBKReference();
    loadCSV(getDataPath("well_construction.csv"), m_constructionTable);
    loadCSV(getDataPath("knbk.csv"), m_knbkTable);
}

MainWindow::~MainWindow() {}

QString MainWindow::getDataPath(const QString &fileName)
{
    QString appDir = QCoreApplication::applicationDirPath();
    QDir dataDir(appDir + "/data");
    if (!dataDir.exists()) dataDir.mkpath(".");
    return dataDir.filePath(fileName);
}

void MainWindow::setupUI()
{
    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    auto *topLayout = new QHBoxLayout();

    // 1. Информация
    auto *infoGroup = new QGroupBox("Информация", central);
    auto *infoLayout = new QFormLayout(infoGroup);
    infoLayout->setSpacing(10);

    auto addInfoField = [&](const QString &label, const QString &defaultValue = "") {
        auto *le = new QLineEdit(defaultValue);
        m_infoInputs[label] = le;
        infoLayout->addRow(label, le);
    };

    addInfoField("Производительность, л/сек:");
    addInfoField("Вес тальблока, т:");
    addInfoField("Вес квадрата, т:");
    addInfoField("Коэфф. кавернозности:", "1.0");
    addInfoField("Глубина спуска, м:", "0");
    addInfoField("Диаметр долота, мм:");
    addInfoField("Плотность, г/см3:", "1.0");

    // 2. Конструкция скважины
    auto *constrGroup = new QGroupBox("Конструкция скважины", central);
    auto *constrLayout = new QVBoxLayout(constrGroup);
    
    auto *constrBtnLayout = new QHBoxLayout();
    auto *addConstrBtn = new QPushButton("+ Добавить", constrGroup);
    auto *delConstrBtn = new QPushButton("- Удалить", constrGroup);
    constrBtnLayout->addWidget(addConstrBtn);
    constrBtnLayout->addWidget(delConstrBtn);
    constrBtnLayout->addStretch();
    
    m_constructionTable = new QTableWidget(0, 7, constrGroup);
    m_constructionTable->setHorizontalHeaderLabels({"Шифр", "Верх, м", "Низ, м", "Наружный d, мм", "Толщина, мм", "Масса 1 м, кг", "Примечание"});
    m_constructionTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    
    constrLayout->addLayout(constrBtnLayout);
    constrLayout->addWidget(m_constructionTable);

    topLayout->addWidget(infoGroup, 1);
    topLayout->addWidget(constrGroup, 2);

    // НИЖНЯЯ ЧАСТЬ
    auto *bottomLayout = new QHBoxLayout();

    // 3. КНБК
    auto *knbkGroup = new QGroupBox("КНБК", central);
    auto *knbkLayout = new QVBoxLayout(knbkGroup);

    auto *knbkBtnLayout = new QHBoxLayout();
    auto *addKrbkBtn = new QPushButton("+ Добавить", knbkGroup);
    auto *delKrbkBtn = new QPushButton("- Удалить", knbkGroup);
    knbkBtnLayout->addWidget(addKrbkBtn);
    knbkBtnLayout->addWidget(delKrbkBtn);
    knbkBtnLayout->addStretch();

    m_knbkTable = new QTableWidget(0, 9, knbkGroup);
    m_knbkTable->setHorizontalHeaderLabels({"Шифр", "Длина, м", "Наружный d, мм", "Внутренний d, мм", "Масса 1 м, кг", "Объем 1 м, л", "Режим", "Масса всего, т", "Объем всего, м3"});
    m_knbkTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    
    knbkLayout->addLayout(knbkBtnLayout);
    knbkLayout->addWidget(m_knbkTable);

    // 4. Результаты
    auto *resGroup = new QGroupBox("Результаты расчета", central);
    auto *resLayout = new QVBoxLayout(resGroup);

    auto *resGrid = new QGridLayout();
    resGrid->setColumnStretch(0, 0); 
    resGrid->setColumnStretch(1, 1); 

    QStringList resLabels = {
        "Вес инструмента в растворе с тальблоком, т:",
        "Вес инструмента в р-ре с квадратом, т:",
        "Вес инструмента в воздухе, т:",
        "Объем инструмента, куб.м:",
        "Объем скважины до гл. спуска, куб.м:",
        "Объем трубного пространства, куб.м:",
        "Объем затрубного пространства, куб.м:",
        "Время выхода газа, мин:",
        "Полное время цикла, мин:"
    };
    
    int row = 0;
    for (const auto &label : resLabels) {
        auto *lbl = new QLabel(label, resGroup);
        auto *le = new QLineEdit();
        le->setReadOnly(true);
        le->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        le->setMinimumWidth(150);
        
        m_resultOutputs[label] = le;
        
        resGrid->addWidget(lbl, row, 0);
        resGrid->addWidget(le, row, 1);
        row++;
    }
    
    auto *calcBtn = new QPushButton("Рассчитать", resGroup);
    calcBtn->setFixedWidth(150);
    calcBtn->setFixedHeight(35);

    resLayout->addLayout(resGrid);
    resLayout->addStretch(); 
    resLayout->addWidget(calcBtn, 0, Qt::AlignRight);

    bottomLayout->addWidget(knbkGroup, 2); 
    bottomLayout->addWidget(resGroup, 1);

    mainLayout->addLayout(topLayout, 1);
    mainLayout->addLayout(bottomLayout, 2);

    connect(addConstrBtn, &QPushButton::clicked, this, &MainWindow::addConstructionRow);
    connect(delConstrBtn, &QPushButton::clicked, this, &MainWindow::removeConstructionRow);
    connect(addKrbkBtn, &QPushButton::clicked, this, &MainWindow::addKNBKRow);
    connect(delKrbkBtn, &QPushButton::clicked, this, &MainWindow::removeKNBKRow);
    connect(calcBtn, &QPushButton::clicked, this, &MainWindow::calculate);
}

void MainWindow::setupKNBKCombo(int row)
{
    QComboBox *combo = new QComboBox(m_knbkTable);
    combo->addItem(""); 
    combo->addItems(m_knbkReference.keys());
    
    combo->setProperty("row", row);
    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onKNBKComboChanged);
    
    m_knbkTable->setCellWidget(row, 0, combo);
    
    for (int col = 1; col < m_knbkTable->columnCount(); ++col) {
        if (col != 6) { 
            m_knbkTable->setItem(row, col, new QTableWidgetItem(""));
        }
    }
}

void MainWindow::setupKNBKModeCombo(int row)
{
    QComboBox *modeCombo = new QComboBox(m_knbkTable);
    modeCombo->addItem("Справочник");
    modeCombo->addItem("Вручную");
    
    modeCombo->setProperty("row", row);
    connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onKNBKModeChanged);
    
    m_knbkTable->setCellWidget(row, 6, modeCombo);
}

void MainWindow::onKNBKComboChanged(int index)
{
    Q_UNUSED(index);
    QComboBox *combo = qobject_cast<QComboBox*>(sender());
    if (!combo) return;

    int row = combo->property("row").toInt();
    QString code = combo->currentText();

    if (m_knbkReference.contains(code)) {
        const KNBKData &data = m_knbkReference[code];
        
        m_knbkTable->blockSignals(true);
        if(m_knbkTable->item(row, 1)) m_knbkTable->item(row, 1)->setText(data.length);
        if(m_knbkTable->item(row, 2)) m_knbkTable->item(row, 2)->setText(data.outerD);
        if(m_knbkTable->item(row, 3)) m_knbkTable->item(row, 3)->setText(data.innerD);
        if(m_knbkTable->item(row, 4)) m_knbkTable->item(row, 4)->setText(data.mass1m);
        if(m_knbkTable->item(row, 5)) m_knbkTable->item(row, 5)->setText(data.vol1m);
        m_knbkTable->blockSignals(false);
        
        statusBar()->showMessage("Данные КНБК подтянуты из справочника", 2000);
    } else {
        m_knbkTable->blockSignals(true);
        for(int col = 1; col <= 5; ++col) {
            if(m_knbkTable->item(row, col)) m_knbkTable->item(row, col)->setText("");
        }
        m_knbkTable->blockSignals(false);
    }
}

void MainWindow::onKNBKModeChanged(int index)
{
    Q_UNUSED(index);
    QComboBox *modeCombo = qobject_cast<QComboBox*>(sender());
    if (!modeCombo) return;

    int row = modeCombo->property("row").toInt();
    bool isManual = (modeCombo->currentText() == "Вручную");

    QTableWidgetItem *volItem = m_knbkTable->item(row, 5);
    if (volItem) {
        if (isManual) {
            volItem->setFlags(volItem->flags() | Qt::ItemIsEditable);
            volItem->setBackground(QBrush(Qt::white));
        } else {
            volItem->setFlags(volItem->flags() & ~Qt::ItemIsEditable);
            volItem->setBackground(QBrush(Qt::lightGray));
            QComboBox *codeCombo = qobject_cast<QComboBox*>(m_knbkTable->cellWidget(row, 0));
            if (codeCombo && m_knbkReference.contains(codeCombo->currentText())) {
                volItem->setText(m_knbkReference[codeCombo->currentText()].vol1m);
            }
        }
    }
}

void MainWindow::addConstructionRow()
{
    int row = m_constructionTable->rowCount();
    m_constructionTable->insertRow(row);
    for (int i = 0; i < m_constructionTable->columnCount(); ++i) {
        m_constructionTable->setItem(row, i, new QTableWidgetItem(""));
    }
}

void MainWindow::removeConstructionRow()
{
    int row = m_constructionTable->currentRow();
    if (row >= 0) m_constructionTable->removeRow(row);
}

void MainWindow::addKNBKRow()
{
    int row = m_knbkTable->rowCount();
    m_knbkTable->insertRow(row);
    setupKNBKCombo(row);
    setupKNBKModeCombo(row);
}

void MainWindow::removeKNBKRow()
{
    int row = m_knbkTable->currentRow();
    if (row >= 0) m_knbkTable->removeRow(row);
}

// ================= РАБОТА С CSV =================

void MainWindow::saveConstruction()
{
    saveCSV(getDataPath("well_construction.csv"), m_constructionTable);
    statusBar()->showMessage("Конструкция скважины сохранена", 3000);
}

void MainWindow::saveCSV(const QString &filePath, QTableWidget *table)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось сохранить файл: " + filePath);
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    QStringList headers;
    for (int col = 0; col < table->columnCount(); ++col) {
        headers << table->horizontalHeaderItem(col)->text();
    }
    out << headers.join(";") << "\n";

    for (int row = 0; row < table->rowCount(); ++row) {
        QStringList rowData;
        for (int col = 0; col < table->columnCount(); ++col) {
            QString text;
            if (col == 0 && table == m_knbkTable) {
                QComboBox *combo = qobject_cast<QComboBox*>(table->cellWidget(row, col));
                if (combo) text = combo->currentText();
            } else if (col == 6 && table == m_knbkTable) {
                QComboBox *combo = qobject_cast<QComboBox*>(table->cellWidget(row, col));
                if (combo) text = combo->currentText();
            } else {
                QTableWidgetItem *item = table->item(row, col);
                if (item) text = item->text();
            }
            rowData << text;
        }
        out << rowData.join(";") << "\n";
    }
    file.close();
}

void MainWindow::loadCSV(const QString &filePath, QTableWidget *table)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    table->setRowCount(0);
    if (!in.atEnd()) in.readLine(); 

    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList fields = line.split(";");
        
        int row = table->rowCount();
        table->insertRow(row);
        
        if (table == m_knbkTable) {
            setupKNBKCombo(row);
            setupKNBKModeCombo(row);
            
            if (fields.size() > 0) {
                QComboBox *combo = qobject_cast<QComboBox*>(table->cellWidget(row, 0));
                if (combo) {
                    int idx = combo->findText(fields[0]);
                    if (idx >= 0) {
                        combo->blockSignals(true);
                        combo->setCurrentIndex(idx);
                        combo->blockSignals(false);
                    }
                }
            }
            
            if (fields.size() > 6) {
                QComboBox *modeCombo = qobject_cast<QComboBox*>(table->cellWidget(row, 6));
                if (modeCombo) {
                    int idx = modeCombo->findText(fields[6]);
                    if (idx >= 0) {
                        modeCombo->blockSignals(true);
                        modeCombo->setCurrentIndex(idx);
                        modeCombo->blockSignals(false);
                    }
                }
            }
            
            for (int col = 1; col < table->columnCount() && col < fields.size(); ++col) {
                if (col != 6 && table->item(row, col)) {
                    table->item(row, col)->setText(fields[col]);
                }
            }
        } else {
            for (int col = 0; col < table->columnCount() && col < fields.size(); ++col) {
                table->setItem(row, col, new QTableWidgetItem(fields[col]));
            }
        }
    }
    file.close();
}

void MainWindow::loadKNBKReference()
{
    m_knbkReference.clear();
    QFile file(getDataPath("knbk.csv"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    
    if (!in.atEnd()) in.readLine();

    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList fields = line.split(";");
        if (fields.size() >= 5) {
            KNBKData data;
            data.length = fields[1];
            data.outerD = fields[2];
            data.innerD = fields[3];
            data.mass1m = fields[4];
            data.vol1m = (fields.size() >= 6) ? fields[5] : "0";
            m_knbkReference[fields[0]] = data;
        }
    }
    file.close();
}

void MainWindow::refreshKNBKCombos()
{
    for (int row = 0; row < m_knbkTable->rowCount(); ++row) {
        QComboBox *combo = qobject_cast<QComboBox*>(m_knbkTable->cellWidget(row, 0));
        if (combo) {
            QString currentText = combo->currentText();
            combo->blockSignals(true);
            combo->clear();
            combo->addItem("");
            combo->addItems(m_knbkReference.keys());
            
            int idx = combo->findText(currentText);
            if (idx >= 0) combo->setCurrentIndex(idx);
            
            combo->blockSignals(false);
        }
    }
}

void MainWindow::openKNBKEditor()
{
    KNBKEditorDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        loadKNBKReference();
        refreshKNBKCombos();
        statusBar()->showMessage("Справочник КНБК обновлен", 3000);
    }
}

// ================= РАСЧЕТ =================

void MainWindow::calculate()
{
    auto getInfoVal = [&](const QString &key) -> double {
        if (!m_infoInputs.contains(key)) return 0.0;
        bool ok;
        double val = m_infoInputs[key]->text().replace(",", ".").toDouble(&ok);
        return ok ? val : 0.0;
    };

    double q_pump = getInfoVal("Производительность, л/сек:");
    double weight_travel_block = getInfoVal("Вес таль блока, т:");
    double weight_square = getInfoVal("Вес квадрата, т:");
    double cavern_coeff = getInfoVal("Коэфф. кавернозности:");
    double depth = getInfoVal("Глубина спуска, м:");
    double bit_diameter = getInfoVal("Диаметр долота, мм:");
    double density = getInfoVal("Плотность, г/см3:");

    if (density <= 0) density = 1.0;
    if (cavern_coeff <= 0) cavern_coeff = 1.0;
    
    if (q_pump <= 0) {
        QMessageBox::warning(this, "Ошибка", "Производительность насоса должна быть больше 0");
        return;
    }

    double total_weight_air_t = 0.0;
    double total_volume_tool_m3 = 0.0; 
    double total_volume_metal_m3 = 0.0; 

    // Блок сбора КНБК
    for (int row = 0; row < m_knbkTable->rowCount(); ++row) {
        auto getVal = [&](int col) -> double {
            QTableWidgetItem *item = m_knbkTable->item(row, col);
            if (!item) return 0.0;
            bool ok;
            return item->text().replace(",", ".").toDouble(&ok);
        };

        // Длина строго из таблицы (пользователь мог её изменить)
        double length = getVal(1);
        double outer_d = getVal(2) / 1000.0;
        double inner_d = getVal(3) / 1000.0;
        
        // Вес погонного метра берем из таблицы (туда он попадает из справочника при выборе)
        double mass_1m = getVal(4); 
        
        // Объем погонного метра
        QComboBox *modeCombo = qobject_cast<QComboBox*>(m_knbkTable->cellWidget(row, 6));
        bool isManual = (modeCombo && modeCombo->currentText() == "Вручную");
        
        double vol_1m_l = 0.0;
        if (isManual) {
            vol_1m_l = getVal(5); // Ручной ввод из таблицы
        } else {
            QComboBox *codeCombo = qobject_cast<QComboBox*>(m_knbkTable->cellWidget(row, 0));
            if (codeCombo && m_knbkReference.contains(codeCombo->currentText())) {
                vol_1m_l = m_knbkReference[codeCombo->currentText()].vol1m.toDouble();
            } else {
                vol_1m_l = getVal(5);
            }
        }

        // Вес в воздухе = Длина (из таблицы) * Вес 1м / 1000
        double weight_air = length * mass_1m / 1000.0;
        total_weight_air_t += weight_air;

        // Объем инструмента = Длина (из таблицы) * Объем 1м (л) / 1000
        double v_inner = (vol_1m_l / 1000.0) * length;
        total_volume_tool_m3 += v_inner;

        double v_outer = M_PI * std::pow(outer_d, 2) / 4.0 * length;
        total_volume_metal_m3 += (v_outer - v_inner);

        m_knbkTable->blockSignals(true);
        if (m_knbkTable->item(row, 7)) m_knbkTable->item(row, 7)->setText(QString::number(weight_air, 'f', 3));
        if (m_knbkTable->item(row, 8)) m_knbkTable->item(row, 8)->setText(QString::number(v_inner, 'f', 4));
        m_knbkTable->blockSignals(false);
    }

    // Блок сбора конструкции скважины и расчета объемов
    struct Casing { double top; double bottom; double innerD; };
    QList<Casing> casings;

    for (int row = 0; row < m_constructionTable->rowCount(); ++row) {
        auto getVal = [&](int col) -> double {
            QTableWidgetItem *item = m_constructionTable->item(row, col);
            if (!item) return 0.0;
            bool ok;
            return item->text().replace(",", ".").toDouble(&ok);
        };

        double top = getVal(1);
        double bottom = getVal(2);
        double outerD_mm = getVal(3);
        double thickness_mm = getVal(4);

        if (outerD_mm > 0 && thickness_mm > 0) {
            Casing c;
            c.top = top;
            c.bottom = bottom;
            c.innerD = (outerD_mm - 2.0 * thickness_mm) / 1000.0;
            casings.append(c);
        }
    }

    std::sort(casings.begin(), casings.end(), [](const Casing &a, const Casing &b) {
        return a.bottom < b.bottom;
    });

    double v_well = 0.0;
    double v_annulus = 0.0;
    double current_depth = 0.0;
    double d_bit_m = bit_diameter / 1000.0;

    for (const auto &c : casings) {
        if (current_depth >= depth) break;
        double top = std::max(current_depth, c.top);
        double bottom = std::min(depth, c.bottom);
        if (bottom > top) {
            double length = bottom - top;
            double v_interval_well = M_PI * std::pow(c.innerD, 2) / 4.0 * length;
            v_well += v_interval_well;
            double metal_per_m = total_volume_metal_m3 / depth; 
            double v_metal_interval = metal_per_m * length;
            v_annulus += (v_interval_well - v_metal_interval);
            current_depth = bottom;
        }
    }

    if (current_depth < depth) {
        double length = depth - current_depth;
        double v_open_well = M_PI * std::pow(d_bit_m, 2) / 4.0 * length * cavern_coeff;
        v_well += v_open_well;
        double metal_per_m = total_volume_metal_m3 / depth;
        double v_metal_open = metal_per_m * length;
        v_annulus += (v_open_well - v_metal_open);
    }

    if (v_annulus < 0) v_annulus = 0;

    double buoyancy_factor = 1.0 - (density / 7.85);
    double weight_in_mud_t = total_weight_air_t * buoyancy_factor;

    double q_pump_m3_min = q_pump * 60.0 / 1000.0;
    double time_gas_min = v_annulus / q_pump_m3_min;
    double time_down_min = total_volume_tool_m3 / q_pump_m3_min;
    double full_cycle_time_min = time_down_min + time_gas_min;

    auto setRes = [&](const QString &key, double val) {
        if (m_resultOutputs.contains(key)) {
            m_resultOutputs[key]->setText(QString::number(val, 'f', 2));
        }
    };

    setRes("Вес инструмента в растворе с тальблоком, т:", weight_in_mud_t + weight_travel_block);
    setRes("Вес инструмента в р-ре с квадратом, т:", weight_in_mud_t + weight_square);
    setRes("Вес инструмента в воздухе, т:", total_weight_air_t);
    setRes("Объем инструмента, куб.м:", total_volume_tool_m3); 
    setRes("Объем скважины до гл. спуска, куб.м:", v_well);
    setRes("Объем трубного пространства, куб.м:", total_volume_tool_m3);
    setRes("Объем затрубного пространства, куб.м:", v_annulus);
    setRes("Время выхода газа, мин:", time_gas_min);
    setRes("Полное время цикла, мин:", full_cycle_time_min);

    statusBar()->showMessage("Расчёт выполнен успешно", 3000);
}
