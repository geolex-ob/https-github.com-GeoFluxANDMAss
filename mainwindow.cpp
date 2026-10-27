#include "mainwindow.h"
#include "referencetoolsdialog.h"
#include "referencecasingsdialog.h"
#include "database.h"
#include "calculations.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QPushButton>
#include <QMenuBar>
#include <QMessageBox>
#include <QDateTime>
#include <QComboBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QDir>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent),
    m_blockCellChanged(false) {
    setWindowTitle("Расчеты ГТИ");
    resize(1400, 900);

    Database::instance().loadData(QDir::homePath() + "/GTICalculator_data");

    setupMenuBar();
    setupUI();
    refreshToolTable();
    refreshCasingTable();
}

void MainWindow::setupMenuBar() {
    QMenu *fileMenu = menuBar()->addMenu("Файл");
    fileMenu->addAction("Экспорт CSV...", this, &MainWindow::exportData);
    fileMenu->addAction("Импорт CSV...", this, &MainWindow::importData);
    fileMenu->addSeparator();
    fileMenu->addAction("Выход", this, &QMainWindow::close);

    QMenu *refMenu = menuBar()->addMenu("Справочники");
    refMenu->addAction("Инструменты (КНБК)...", this, &MainWindow::openToolsReference);
    refMenu->addAction("Обсадные трубы...", this, &MainWindow::openCasingsReference);
    refMenu->addAction("Жидкости...", this, &MainWindow::openFluidsReference);

    menuBar()->addMenu("О программе")->addAction("О программе", this, &MainWindow::showAbout);
}

void MainWindow::setupUI() {
    QWidget *central = new QWidget(this);
    QVBoxLayout *root = new QVBoxLayout(central);

    QGroupBox *infoGroup = new QGroupBox("Информация");
    QFormLayout *infoLayout = new QFormLayout(infoGroup);

    editTripNumber = new QLineEdit("1");
    editBatchNumber = new QLineEdit("17");
    editWellNumber = new QLineEdit("2220");
    editField = new QLineEdit("ЧНГКМ");
    editTripDepth = new QLineEdit("200");
    editFluidDensity = new QLineEdit("1.05");
    editProductivity = new QLineEdit("10");
    editBlockWeight = new QLineEdit("0");
    editSquareWeight = new QLineEdit("0");
    editCavernosity = new QLineEdit("1");
    editBitDiameter = new QLineEdit("89");

    infoLayout->addRow("Номер рейса:", editTripNumber);
    infoLayout->addRow("Номер партии:", editBatchNumber);
    infoLayout->addRow("Номер скважины:", editWellNumber);
    infoLayout->addRow("Месторождение:", editField);
    infoLayout->addRow("Глубина спуска, м:", editTripDepth);
    infoLayout->addRow("Плотность, г/см3:", editFluidDensity);
    infoLayout->addRow("Производительность, л/сек:", editProductivity);
    infoLayout->addRow("Вес таль-блока, т:", editBlockWeight);
    infoLayout->addRow("Вес квадрата, т:", editSquareWeight);
    infoLayout->addRow("Коэфф. кавернозности:", editCavernosity);
    infoLayout->addRow("Диаметр долота, мм:", editBitDiameter);

    QGroupBox *casingGroup = new QGroupBox("Конструкция скважины");
    QVBoxLayout *casingLayout = new QVBoxLayout(casingGroup);

    casingTable = new QTableWidget();
    casingTable->setColumnCount(6);
    
    QStringList casingHeaders;
    casingHeaders << "Шифр" << "Верх, м" << "Низ, м" 
                  << "Наружный диаметр, мм" << "Толщина стенки, мм" << "Масса 1 м, кг";
    casingTable->setHorizontalHeaderLabels(casingHeaders);
    
    casingTable->horizontalHeader()->setStretchLastSection(true);
    casingTable->setAlternatingRowColors(true);

    QPushButton *btnAddC = new QPushButton("Добавить");
    QPushButton *btnDelC = new QPushButton("Удалить");
    QHBoxLayout *cBtn = new QHBoxLayout();
    cBtn->addWidget(btnAddC);
    cBtn->addWidget(btnDelC);
    cBtn->addStretch();
    casingLayout->addWidget(casingTable);
    casingLayout->addLayout(cBtn);

    connect(btnAddC, &QPushButton::clicked, this, &MainWindow::addCasingRow);
    connect(btnDelC, &QPushButton::clicked, this, &MainWindow::deleteCasingRow);

    QGroupBox *toolGroup = new QGroupBox("КНБК");
    QVBoxLayout *toolLayout = new QVBoxLayout(toolGroup);

    toolTable = new QTableWidget();
    toolTable->setColumnCount(8);
    
    QStringList toolHeaders;
    toolHeaders << "Шифр" << "Длина, м" << "Наружный диаметр, мм" 
                << "Внутренний диаметр, мм" << "Масса 1 м, кг" 
                << "Объём 1 м, л" << "Масса всего, т" << "Объём всего, м3";
    toolTable->setHorizontalHeaderLabels(toolHeaders);
    
    toolTable->horizontalHeader()->setStretchLastSection(true);
    toolTable->setAlternatingRowColors(true);

    connect(toolTable, &QTableWidget::cellChanged,
            this, &MainWindow::onToolCellChanged);

    QPushButton *btnAddT = new QPushButton("Добавить");
    QPushButton *btnDelT = new QPushButton("Удалить");
    QHBoxLayout *tBtn = new QHBoxLayout();
    tBtn->addWidget(btnAddT);
    tBtn->addWidget(btnDelT);
    tBtn->addStretch();
    toolLayout->addWidget(toolTable);
    toolLayout->addLayout(tBtn);

    connect(btnAddT, &QPushButton::clicked, this, &MainWindow::addToolRow);
    connect(btnDelT, &QPushButton::clicked, this, &MainWindow::deleteToolRow);

    QGroupBox *resGroup = new QGroupBox("Результаты расчета");
    QFormLayout *resLayout = new QFormLayout(resGroup);

    labelWeightFluidBlock = new QLabel("0");
    labelWeightFluidSquare = new QLabel("0");
    labelWeightAir = new QLabel("0");
    labelToolVolume = new QLabel("0");
    labelWellVolume = new QLabel("0");
    labelTubingVolume = new QLabel("0");
    labelAnnularVolume = new QLabel("0");
    labelGasTime = new QLabel("0");
    labelCycleTime = new QLabel("0");
    labelDateTime = new QLabel(QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss"));

    resLayout->addRow("Вес инструмента в растворе с тальблоком, т:", labelWeightFluidBlock);
    resLayout->addRow("Вес инструмента в р-ре с квадратом, т:", labelWeightFluidSquare);
    resLayout->addRow("Вес инструмента в воздухе, т:", labelWeightAir);
    resLayout->addRow("Объём инструмента, куб.м:", labelToolVolume);
    resLayout->addRow("Объём скважины до гл. спуска, куб.м:", labelWellVolume);
    resLayout->addRow("Объем трубного пространства, куб.м:", labelTubingVolume);
    resLayout->addRow("Объем затрубного пространства, куб.м:", labelAnnularVolume);
    resLayout->addRow("Время выхода газа, мин:", labelGasTime);
    resLayout->addRow("Полное время цикла, мин:", labelCycleTime);
    resLayout->addRow("Дата и время:", labelDateTime);

    QPushButton *btnCalc = new QPushButton("Рассчитать");
    btnCalc->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; padding: 10px;");
    connect(btnCalc, &QPushButton::clicked, this, &MainWindow::calculate);

    QHBoxLayout *topRow = new QHBoxLayout();
    topRow->addWidget(infoGroup);
    topRow->addWidget(casingGroup);

    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->addWidget(toolGroup, 2);
    bottomRow->addWidget(resGroup, 1);

    QVBoxLayout *finalLayout = new QVBoxLayout();
    finalLayout->addLayout(topRow);
    finalLayout->addLayout(bottomRow);
    finalLayout->addWidget(btnCalc);

    root->addLayout(finalLayout);
    setCentralWidget(central);
}

void MainWindow::onToolCellChanged(int row, int column) {
    if (m_blockCellChanged) return;
    if (column == 1 || column == 4 || column == 5) {
        recalculateRow(row);
    }
}

void MainWindow::recalculateRow(int row) {
    if (row < 0 || row >= toolTable->rowCount()) return;

    auto* lenIt = toolTable->item(row, 1);
    auto* massIt = toolTable->item(row, 4);
    auto* volIt = toolTable->item(row, 5);
    if (!lenIt || !massIt || !volIt) return;

    double L = lenIt->text().toDouble();
    double m = massIt->text().toDouble();
    double v = volIt->text().toDouble();

    double totalMass = m * L / 1000.0;
    double totalVolume = v * L / 1000.0;

    m_blockCellChanged = true;
    toolTable->setItem(row, 6, new QTableWidgetItem(QString::number(totalMass, 'f', 3)));
    toolTable->setItem(row, 7, new QTableWidgetItem(QString::number(totalVolume, 'f', 4)));
    m_blockCellChanged = false;
}

void MainWindow::loadToolCombo(QComboBox* combo) {
    combo->clear();
    for (const auto& t : Database::instance().getTools())
        combo->addItem(t.name);
}

void MainWindow::loadCasingCombo(QComboBox* combo) {
    combo->clear();
    for (const auto& c : Database::instance().getCasings())
        combo->addItem(c.name);
}

void MainWindow::refreshToolTable() {
    toolTable->setRowCount(0);
    for (const auto& t : Database::instance().getTools()) {
        int row = toolTable->rowCount();
        toolTable->insertRow(row);
        QComboBox* combo = new QComboBox();
        loadToolCombo(combo);
        combo->setCurrentText(t.name);
        toolTable->setCellWidget(row, 0, combo);
        toolTable->setItem(row, 1, new QTableWidgetItem("500"));
        toolTable->setItem(row, 2, new QTableWidgetItem(QString::number(t.outerDiameter)));
        toolTable->setItem(row, 3, new QTableWidgetItem(QString::number(t.innerDiameter)));
        toolTable->setItem(row, 4, new QTableWidgetItem(QString::number(t.massPerMeter)));
        toolTable->setItem(row, 5, new QTableWidgetItem(QString::number(t.volumePerMeter)));
        toolTable->setItem(row, 6, new QTableWidgetItem("0"));
        toolTable->setItem(row, 7, new QTableWidgetItem("0"));

        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                [this, row, combo](int) {
            for (const auto& t : Database::instance().getTools()) {
                if (t.name == combo->currentText()) {
                    m_blockCellChanged = true;
                    toolTable->setItem(row, 2, new QTableWidgetItem(QString::number(t.outerDiameter)));
                    toolTable->setItem(row, 3, new QTableWidgetItem(QString::number(t.innerDiameter)));
                    toolTable->setItem(row, 4, new QTableWidgetItem(QString::number(t.massPerMeter)));
                    toolTable->setItem(row, 5, new QTableWidgetItem(QString::number(t.volumePerMeter)));
                    m_blockCellChanged = false;
                    recalculateRow(row);
                    break;
                }
            }
        });

        recalculateRow(row);
    }
}

void MainWindow::refreshCasingTable() {
    casingTable->setRowCount(0);
    for (const auto& c : Database::instance().getCasings()) {
        int row = casingTable->rowCount();
        casingTable->insertRow(row);
        QComboBox* combo = new QComboBox();
        loadCasingCombo(combo);
        combo->setCurrentText(c.name);
        casingTable->setCellWidget(row, 0, combo);
        
        // ИСПРАВЛЕНО: правильные индексы колонок
        casingTable->setItem(row, 1, new QTableWidgetItem("0"));  // Верх
        casingTable->setItem(row, 2, new QTableWidgetItem("0"));  // Низ
        casingTable->setItem(row, 3, new QTableWidgetItem(QString::number(c.outerDiameter)));  // Наружный диаметр
        casingTable->setItem(row, 4, new QTableWidgetItem(QString::number(c.wallThickness)));  // Толщина стенки
        casingTable->setItem(row, 5, new QTableWidgetItem(QString::number(c.massPerMeter)));  // Масса 1 м

        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                [this, row, combo](int) {
            for (const auto& c : Database::instance().getCasings()) {
                if (c.name == combo->currentText()) {
                    casingTable->setItem(row, 3, new QTableWidgetItem(QString::number(c.outerDiameter)));
                    casingTable->setItem(row, 4, new QTableWidgetItem(QString::number(c.wallThickness)));
                    casingTable->setItem(row, 5, new QTableWidgetItem(QString::number(c.massPerMeter)));
                    break;
                }
            }
        });
    }
}

void MainWindow::addToolRow() {
    int row = toolTable->rowCount();
    toolTable->insertRow(row);
    QComboBox* combo = new QComboBox();
    loadToolCombo(combo);
    toolTable->setCellWidget(row, 0, combo);
    for (int c = 1; c < 8; ++c)
        toolTable->setItem(row, c, new QTableWidgetItem("0"));

    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this, row, combo](int) {
        for (const auto& t : Database::instance().getTools()) {
            if (t.name == combo->currentText()) {
                m_blockCellChanged = true;
                toolTable->setItem(row, 2, new QTableWidgetItem(QString::number(t.outerDiameter)));
                toolTable->setItem(row, 3, new QTableWidgetItem(QString::number(t.innerDiameter)));
                toolTable->setItem(row, 4, new QTableWidgetItem(QString::number(t.massPerMeter)));
                toolTable->setItem(row, 5, new QTableWidgetItem(QString::number(t.volumePerMeter)));
                m_blockCellChanged = false;
                recalculateRow(row);
                break;
            }
        }
    });
}

void MainWindow::deleteToolRow() {
    int row = toolTable->currentRow();
    if (row >= 0) toolTable->removeRow(row);
}

void MainWindow::addCasingRow() {
    int row = casingTable->rowCount();
    casingTable->insertRow(row);
    QComboBox* combo = new QComboBox();
    loadCasingCombo(combo);
    casingTable->setCellWidget(row, 0, combo);
    
    // ИСПРАВЛЕНО: правильные индексы колонок
    casingTable->setItem(row, 1, new QTableWidgetItem("0"));  // Верх
    casingTable->setItem(row, 2, new QTableWidgetItem("0"));  // Низ
    casingTable->setItem(row, 3, new QTableWidgetItem("0"));  // Наружный диаметр
    casingTable->setItem(row, 4, new QTableWidgetItem("0"));  // Толщина стенки
    casingTable->setItem(row, 5, new QTableWidgetItem("0"));  // Масса 1 м

    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this, row, combo](int) {
        for (const auto& c : Database::instance().getCasings()) {
            if (c.name == combo->currentText()) {
                casingTable->setItem(row, 3, new QTableWidgetItem(QString::number(c.outerDiameter)));
                casingTable->setItem(row, 4, new QTableWidgetItem(QString::number(c.wallThickness)));
                casingTable->setItem(row, 5, new QTableWidgetItem(QString::number(c.massPerMeter)));
                break;
            }
        }
    });
}

void MainWindow::deleteCasingRow() {
    int row = casingTable->currentRow();
    if (row >= 0) casingTable->removeRow(row);
}

void MainWindow::calculate() {
    QVector<ToolRecord> tools;
    for (int row = 0; row < toolTable->rowCount(); ++row) {
        ToolRecord t;
        if (auto* combo = qobject_cast<QComboBox*>(toolTable->cellWidget(row, 0)))
            t.name = combo->currentText();

        auto get = [&](int c){
            auto* it = toolTable->item(row, c);
            return it ? it->text().toDouble() : 0.0;
        };

        t.length = get(1);
        t.outerDiameter = get(2);
        t.innerDiameter = get(3);
        t.massPerMeter = get(6);
        t.volumePerMeter = get(7);

        tools.append(t);
    }

    QVector<CasingRecord> casings;
    for (int row = 0; row < casingTable->rowCount(); ++row) {
        CasingRecord c;
        if (auto* combo = qobject_cast<QComboBox*>(casingTable->cellWidget(row, 0)))
            c.name = combo->currentText();

        auto get = [&](int col){
            auto* it = casingTable->item(row, col);
            return it ? it->text().toDouble() : 0.0;
        };

        c.topDepth = get(1);
        c.bottomDepth = get(2);
        c.outerDiameter = get(3);
        c.wallThickness = get(4);
        c.massPerMeter = get(5);

        casings.append(c);
    }

    auto r = Calculations::calculate(
        tools, casings,
        editTripDepth->text().toDouble(),
                                     editFluidDensity->text().toDouble(),
                                     editProductivity->text().toDouble(),
                                     editBlockWeight->text().toDouble(),
                                     editSquareWeight->text().toDouble(),
                                     editBitDiameter->text().toDouble(),
                                     editCavernosity->text().toDouble());  // НОВЫЙ ПАРАМЕТР

    labelWeightFluidBlock->setText(QString::number(r.weightInFluidWithBlock, 'f', 2));
    labelWeightFluidSquare->setText(QString::number(r.weightInFluidWithSquare, 'f', 2));
    labelWeightAir->setText(QString::number(r.weightInAir, 'f', 2));
    labelToolVolume->setText(QString::number(r.toolVolume, 'f', 3));
    labelWellVolume->setText(QString::number(r.wellVolumeToDepth, 'f', 3));
    labelTubingVolume->setText(QString::number(r.tubingVolume, 'f', 3));
    labelAnnularVolume->setText(QString::number(r.annularVolume, 'f', 3));
    labelGasTime->setText(QString::number(r.gasExitTime, 'f', 2));
    labelCycleTime->setText(QString::number(r.fullCycleTime, 'f', 2));
    labelDateTime->setText(QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss"));
}

void MainWindow::openToolsReference() {
    ReferenceToolsDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) refreshToolTable();
}

void MainWindow::openCasingsReference() {
    ReferenceCasingsDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) refreshCasingTable();
}

void MainWindow::openFluidsReference() {
    QMessageBox::information(this, "Жидкости",
        "Справочник жидкостей хранится в файле:\n" +
        Database::instance().dataDirectory() + "/ref_fluids.csv\n\n" +
        "Откройте его в Excel/LibreOffice для редактирования.");
}

void MainWindow::showAbout() {
    QMessageBox::about(this, "О программе",
        "GeoFluxANDMass v0.0.1\n"
        "Расчет веса инструмента в жидкости.\n"
        "Данные хранятся в CSV файлах (совместимо с LibreOffice/Excel).");
}

void MainWindow::exportData() {
    QString dir = QFileDialog::getExistingDirectory(this, "Выберите папку для экспорта");
    if (dir.isEmpty()) return;
    Database::instance().exportCsv(dir + "/ref_tools.csv", "tools");
    Database::instance().exportCsv(dir + "/ref_casings.csv", "casings");
    Database::instance().exportCsv(dir + "/ref_fluids.csv", "fluids");
    QMessageBox::information(this, "Экспорт", "Данные экспортированы в:\n" + dir);
}

void MainWindow::importData() {
    QString file = QFileDialog::getOpenFileName(this, "Импорт CSV", QString(),
                                                "CSV файлы (*.csv)");
    if (file.isEmpty()) return;
    QString name = QFileInfo(file).fileName().toLower();
    QString table;
    if (name.contains("tool")) table = "tools";
    else if (name.contains("casing")) table = "casings";
    else if (name.contains("fluid")) table = "fluids";
    else {
        QMessageBox::warning(this, "Импорт", "Не удалось определить тип файла.\n"
                             "Имя должно содержать 'tool', 'casing' или 'fluid'.");
        return;
    }
    if (Database::instance().importCsv(file, table)) {
        QMessageBox::information(this, "Импорт", "Данные импортированы.");
        refreshToolTable();
        refreshCasingTable();
    } else {
        QMessageBox::critical(this, "Импорт", "Ошибка чтения файла.");
    }
}
