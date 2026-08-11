#include "mainwindow.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QScrollArea>
#include <QSplitter>
#include <QTextStream>
#include <QDebug>
#include <algorithm>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_visualization(nullptr)
    , m_bottomUpTimeLabel(nullptr)
    , m_surfaceToBottomTimeLabel(nullptr)
{
    m_dataPath = QDir::homePath() + "/.burenie_calculator";
    QDir().mkpath(m_dataPath);

    m_toolModel = new ToolModel(this);
    m_layoutModel = new QStandardItemModel(this);
    m_wellModel = new QStandardItemModel(this);

    setupUI();
    createMenuBar();
    applySettings();

    QString toolsFile = m_dataPath + "/tools.dat";
    if (QFile::exists(toolsFile))
        m_toolModel->loadFromFile(toolsFile);
    
    QString layoutFile = m_dataPath + "/layout.csv";
    if (QFile::exists(layoutFile))
        loadLayoutFromCSV(layoutFile);
    
    QString wellFile = m_dataPath + "/well.csv";
    if (QFile::exists(wellFile))
        loadWellFromCSV(wellFile);

    connect(m_toolModel, &QAbstractItemModel::dataChanged, this, &MainWindow::onTableCellChanged);
    
    // Обновляем таблицы после загрузки
    refreshLayoutTable();
    updateVisualization();
}

MainWindow::~MainWindow()
{
    autoSaveAll();
    saveSettings();
}

QString MainWindow::dataPath() const
{
    return m_dataPath;
}

void MainWindow::setupUI()
{
    m_tabWidget = new QTabWidget(this);
    setCentralWidget(m_tabWidget);
    createToolTab();
    createLayoutTab();
    createWellTab();
}

void MainWindow::createToolTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);

    m_toolTable = new QTableView();
    m_toolTable->setModel(m_toolModel);
    m_toolTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_toolTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_toolTable->horizontalHeader()->setStretchLastSection(true);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_addToolBtn = new QPushButton("Добавить");
    m_removeToolBtn = new QPushButton("Удалить");
    m_calcVolumeBtn = new QPushButton("Рассчитать объем по весу");

    btnLayout->addWidget(m_addToolBtn);
    btnLayout->addWidget(m_removeToolBtn);
    btnLayout->addWidget(m_calcVolumeBtn);
    btnLayout->addStretch();

    layout->addWidget(m_toolTable);
    layout->addLayout(btnLayout);

    connect(m_addToolBtn, &QPushButton::clicked, this, &MainWindow::onAddTool);
    connect(m_removeToolBtn, &QPushButton::clicked, this, &MainWindow::onRemoveTool);
    connect(m_calcVolumeBtn, &QPushButton::clicked, this, &MainWindow::onCalculateVolume);

    m_tabWidget->addTab(tab, "Справочник");
}

void MainWindow::createLayoutTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);

    m_layoutTable = new QTableView();
    m_layoutModel->setHorizontalHeaderLabels({
        "Название", "D наруж, мм", "D внутр, мм", "Вес, кг/м",
        "Длина, м", "Объем, м³", "Вес в возд, т", "Вес в жидк, т",
        "Сум. объем, м³", "Сум. вес возд, т", "Сум. вес жидк, т"
    });
    m_layoutTable->setModel(m_layoutModel);
    m_layoutTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_layoutTable->horizontalHeader()->setStretchLastSection(true);
    m_layoutTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_addToLayoutBtn = new QPushButton("Вставить из справочника");
    m_removeFromLayoutBtn = new QPushButton("Удалить");
    m_moveLayoutUpBtn = new QPushButton("▲ Вверх");
    m_moveLayoutDownBtn = new QPushButton("▼ Вниз");
    
    btnLayout->addWidget(m_addToLayoutBtn);
    btnLayout->addWidget(m_removeFromLayoutBtn);
    btnLayout->addWidget(m_moveLayoutUpBtn);
    btnLayout->addWidget(m_moveLayoutDownBtn);
    btnLayout->addStretch();

    QHBoxLayout *fluidLayout = new QHBoxLayout();
    fluidLayout->addWidget(new QLabel("Плотность жидкости, кг/м³:"));
    m_fluidDensitySpin = new QDoubleSpinBox();
    m_fluidDensitySpin->setRange(800, 3000);
    m_fluidDensitySpin->setValue(1200);
    m_fluidDensitySpin->setSuffix(" кг/м³");
    fluidLayout->addWidget(m_fluidDensitySpin);
    fluidLayout->addStretch();

    QHBoxLayout *totalLayout = new QHBoxLayout();
    m_totalWeightAirLabel = new QLabel("Вес в воздухе: 0.000 т");
    m_totalWeightFluidLabel = new QLabel("Вес в жидкости: 0.000 т");
    m_totalVolumeLabel = new QLabel("Объем: 0.000 м³");
    totalLayout->addWidget(m_totalWeightAirLabel);
    totalLayout->addWidget(m_totalWeightFluidLabel);
    totalLayout->addWidget(m_totalVolumeLabel);

    layout->addWidget(m_layoutTable);
    layout->addLayout(btnLayout);
    layout->addLayout(fluidLayout);
    layout->addLayout(totalLayout);

    connect(m_addToLayoutBtn, &QPushButton::clicked, this, &MainWindow::onAddToLayout);
    connect(m_removeFromLayoutBtn, &QPushButton::clicked, this, &MainWindow::onRemoveFromLayout);
    connect(m_moveLayoutUpBtn, &QPushButton::clicked, this, &MainWindow::onMoveLayoutUp);
    connect(m_moveLayoutDownBtn, &QPushButton::clicked, this, &MainWindow::onMoveLayoutDown);
    connect(m_fluidDensitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this]() {
        refreshLayoutTable();
        autoSaveAll();
        updateVisualization();
    });
    
    connect(m_layoutModel, &QStandardItemModel::itemChanged, this, [this](QStandardItem *item) {
        if (item->column() == 4) {
            int row = item->row();
            if (row >= 0 && row < m_layoutItems.size()) {
                bool ok;
                double newLength = item->text().toDouble(&ok);
                if (ok && newLength > 0) {
                    m_layoutItems[row].length = newLength;
                    m_layoutModel->blockSignals(true);
                    refreshLayoutTable();
                    m_layoutModel->blockSignals(false);
                    autoSaveAll();
                    updateVisualization();
                }
            }
        }
    });

    m_tabWidget->addTab(tab, "Компоновка");
}

void MainWindow::createWellTab()
{
    QWidget *tab = new QWidget();
    QHBoxLayout *mainLayout = new QHBoxLayout(tab);

    QWidget *leftWidget = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 5, 0);

    m_wellTable = new QTableView();
    m_wellModel->setHorizontalHeaderLabels({
        "Название", "Нач. глубина, м", "Кон. глубина, м",
        "D наруж, мм", "D внутр, мм", "Голый ствол", "Кавернозность"
    });
    m_wellTable->setModel(m_wellModel);
    m_wellTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_wellTable->horizontalHeader()->setStretchLastSection(true);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_addCasingBtn = new QPushButton("Добавить колонну");
    m_removeCasingBtn = new QPushButton("Удалить");
    m_moveCasingUpBtn = new QPushButton("▲ Вверх");
    m_moveCasingDownBtn = new QPushButton("▼ Вниз");
    
    btnLayout->addWidget(m_addCasingBtn);
    btnLayout->addWidget(m_removeCasingBtn);
    btnLayout->addWidget(m_moveCasingUpBtn);
    btnLayout->addWidget(m_moveCasingDownBtn);
    btnLayout->addStretch();

    QHBoxLayout *flowLayout = new QHBoxLayout();
    flowLayout->addWidget(new QLabel("Расход жидкости, л/с:"));
    m_flowRateSpin = new QDoubleSpinBox();
    m_flowRateSpin->setRange(1, 200);
    m_flowRateSpin->setValue(30);
    m_flowRateSpin->setSuffix(" л/с");
    flowLayout->addWidget(m_flowRateSpin);

    QPushButton *calcCircBtn = new QPushButton("Рассчитать циркуляцию");
    flowLayout->addWidget(calcCircBtn);
    flowLayout->addStretch();

    QHBoxLayout *resultLayout = new QHBoxLayout();
    m_circulationTimeLabel = new QLabel("Полный цикл: 0.00 мин");
    m_bottomUpTimeLabel = new QLabel("Забой→Устье: 0.00 мин");
    m_surfaceToBottomTimeLabel = new QLabel("Устье→Забой: 0.00 мин");
    m_wellVolumeLabel = new QLabel("Объем скважины: 0.000 м³");
    
    resultLayout->addWidget(m_circulationTimeLabel);
    resultLayout->addWidget(m_bottomUpTimeLabel);
    resultLayout->addWidget(m_surfaceToBottomTimeLabel);
    resultLayout->addWidget(m_wellVolumeLabel);

    leftLayout->addWidget(m_wellTable);
    leftLayout->addLayout(btnLayout);
    leftLayout->addLayout(flowLayout);
    leftLayout->addLayout(resultLayout);
    leftLayout->addStretch();

    QWidget *rightWidget = new QWidget();
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(5, 0, 0, 0);

    QHBoxLayout *zoomLayout = new QHBoxLayout();
    QPushButton *zoomInBtn = new QPushButton("+");
    zoomInBtn->setFixedWidth(30);
    QPushButton *zoomOutBtn = new QPushButton("-");
    zoomOutBtn->setFixedWidth(30);
    QPushButton *fitBtn = new QPushButton("По размеру");
    
    zoomLayout->addWidget(new QLabel("Масштаб:"));
    zoomLayout->addWidget(zoomInBtn);
    zoomLayout->addWidget(zoomOutBtn);
    zoomLayout->addWidget(fitBtn);
    zoomLayout->addStretch();

    m_visualization = new WellVisualization();
    m_visualization->setMinimumSize(400, 400);

    rightLayout->addLayout(zoomLayout);
    rightLayout->addWidget(m_visualization);

    QSplitter *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(leftWidget);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    splitter->setSizes({400, 600});

    mainLayout->addWidget(splitter);

    connect(m_addCasingBtn, &QPushButton::clicked, this, &MainWindow::onAddCasing);
    connect(m_removeCasingBtn, &QPushButton::clicked, this, &MainWindow::onRemoveCasing);
    connect(m_moveCasingUpBtn, &QPushButton::clicked, this, &MainWindow::onMoveCasingUp);
    connect(m_moveCasingDownBtn, &QPushButton::clicked, this, &MainWindow::onMoveCasingDown);
    connect(calcCircBtn, &QPushButton::clicked, this, &MainWindow::onCalculateCirculation);
    connect(m_wellModel, &QStandardItemModel::itemChanged, this, [this]() {
        onCalculateCirculation();
    });

    connect(zoomInBtn, &QPushButton::clicked, m_visualization, &WellVisualization::zoomIn);
    connect(zoomOutBtn, &QPushButton::clicked, m_visualization, &WellVisualization::zoomOut);
    connect(fitBtn, &QPushButton::clicked, m_visualization, &WellVisualization::fitToScreen);

    m_tabWidget->addTab(tab, "Конструкция скважины");
}

void MainWindow::createMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu("Файл");

    QAction *saveLayoutAct = fileMenu->addAction("Сохранить компоновку (CSV)");
    connect(saveLayoutAct, &QAction::triggered, this, &MainWindow::onSaveLayout);

    QAction *loadLayoutAct = fileMenu->addAction("Загрузить компоновку (CSV)");
    connect(loadLayoutAct, &QAction::triggered, this, &MainWindow::onLoadLayout);

    fileMenu->addSeparator();

    QAction *saveWellAct = fileMenu->addAction("Сохранить конструкцию скважины (CSV)");
    connect(saveWellAct, &QAction::triggered, this, &MainWindow::onSaveWell);

    QAction *loadWellAct = fileMenu->addAction("Загрузить конструкцию скважины (CSV)");
    connect(loadWellAct, &QAction::triggered, this, &MainWindow::onLoadWell);
}

void MainWindow::applySettings()
{
    QSettings settings;
    settings.beginGroup("MainWindow");
    resize(settings.value("size", QSize(1400, 900)).toSize());
    move(settings.value("pos", QPoint(200, 200)).toPoint());
    settings.endGroup();
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.beginGroup("MainWindow");
    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.endGroup();
}

void MainWindow::updateVisualization()
{
    if (!m_visualization) return;

    WellConstruction wc;
    for (int i = 0; i < m_wellModel->rowCount(); ++i) {
        Casing c;
        c.name = m_wellModel->item(i, 0) ? m_wellModel->item(i, 0)->text() : "";
        c.startDepth = m_wellModel->item(i, 1) ? m_wellModel->item(i, 1)->text().toDouble() : 0;
        c.endDepth = m_wellModel->item(i, 2) ? m_wellModel->item(i, 2)->text().toDouble() : 0;
        c.outerDiameter = m_wellModel->item(i, 3) ? m_wellModel->item(i, 3)->text().toDouble() : 0;
        
        QString holeStr = m_wellModel->item(i, 5) ? m_wellModel->item(i, 5)->text().toLower() : "";
        c.isOpenHole = (holeStr == "да" || holeStr == "yes" || holeStr == "1");
        
        if (c.isOpenHole) {
            c.innerDiameter = c.outerDiameter;
            c.cavernosity = m_wellModel->item(i, 6) ? m_wellModel->item(i, 6)->text().toDouble() : 1.0;
        } else {
            c.innerDiameter = m_wellModel->item(i, 4) ? m_wellModel->item(i, 4)->text().toDouble() : 0;
            c.cavernosity = 1.0;
        }
        
        wc.addCasing(c);
    }

    m_visualization->setWellConstruction(wc);
    m_visualization->setLayout(m_layoutItems);
}

void MainWindow::refreshLayoutTable()
{
    m_layoutModel->removeRows(0, m_layoutModel->rowCount());
    
    m_calculator.setLayout(m_layoutItems);
    m_calculator.setFluidDensity(m_fluidDensitySpin->value());

    QVector<double> cumVol = m_calculator.cumulativeVolume();
    QVector<double> cumWair = m_calculator.cumulativeWeightAir();
    QVector<double> cumWfluid = m_calculator.cumulativeWeightFluid();

    for (int i = 0; i < m_layoutItems.size(); ++i) {
        const auto &item = m_layoutItems[i];
        QList<QStandardItem*> row;

        auto makeItem = [](const QString &text, bool editable) {
            QStandardItem *it = new QStandardItem(text);
            it->setEditable(editable);
            it->setTextAlignment(Qt::AlignCenter);
            return it;
        };

        row.append(makeItem(item.tool.name(), false));
        row.append(makeItem(QString::number(item.tool.outerDiameter(), 'f', 2), false));
        row.append(makeItem(QString::number(item.tool.innerDiameter(), 'f', 2), false));
        row.append(makeItem(QString::number(item.tool.weightPerMeter(), 'f', 3), false));
        row.append(makeItem(QString::number(item.length, 'f', 3), true));
        row.append(makeItem(QString::number(item.volumeInner(), 'f', 4), false));
        row.append(makeItem(QString::number(item.weightInAir(), 'f', 4), false));
        row.append(makeItem(QString::number(item.weightInFluid(m_fluidDensitySpin->value()), 'f', 4), false));
        
        // Кумулятивные суммы: от данного элемента до забоя (снизу вверх)
        row.append(makeItem(i < cumVol.size() ? QString::number(cumVol[i], 'f', 4) : "", false));
        row.append(makeItem(i < cumWair.size() ? QString::number(cumWair[i], 'f', 4) : "", false));
        row.append(makeItem(i < cumWfluid.size() ? QString::number(cumWfluid[i], 'f', 4) : "", false));

        m_layoutModel->appendRow(row);
    }

    m_totalWeightAirLabel->setText(QString("Вес в воздухе: %1 т")
        .arg(m_calculator.totalWeightInAir(), 0, 'f', 3));
    m_totalWeightFluidLabel->setText(QString("Вес в жидкости: %1 т")
        .arg(m_calculator.totalWeightInFluid(), 0, 'f', 3));
    double totalVol = 0;
    for (const auto &item : m_layoutItems)
        totalVol += item.volumeInner();
    m_totalVolumeLabel->setText(QString("Объем: %1 м³").arg(totalVol, 0, 'f', 4));
}

// --- CSV сохранение/загрузка ---

void MainWindow::saveLayoutToCSV(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось сохранить файл: " + filename);
        return;
    }
    
    QTextStream stream(&file);
    stream << "Название;D_наруж_мм;D_внутр_мм;Вес_кг_м;Длина_м;Объем_м3;Вес_возд_т;Вес_жидк_т;Сум_объем_м3;Сум_вес_возд_т;Сум_вес_жидк_т;Плотность_жидк_кг_м3\n";
    
    m_calculator.setLayout(m_layoutItems);
    m_calculator.setFluidDensity(m_fluidDensitySpin->value());
    QVector<double> cumVol = m_calculator.cumulativeVolume();
    QVector<double> cumWair = m_calculator.cumulativeWeightAir();
    QVector<double> cumWfluid = m_calculator.cumulativeWeightFluid();
    
    for (int i = 0; i < m_layoutItems.size(); ++i) {
        const auto &item = m_layoutItems[i];
        stream << item.tool.name() << ";"
               << QString::number(item.tool.outerDiameter(), 'f', 2) << ";"
               << QString::number(item.tool.innerDiameter(), 'f', 2) << ";"
               << QString::number(item.tool.weightPerMeter(), 'f', 3) << ";"
               << QString::number(item.length, 'f', 3) << ";"
               << QString::number(item.volumeInner(), 'f', 4) << ";"
               << QString::number(item.weightInAir(), 'f', 4) << ";"
               << QString::number(item.weightInFluid(m_fluidDensitySpin->value()), 'f', 4) << ";"
               << (i < cumVol.size() ? QString::number(cumVol[i], 'f', 4) : "") << ";"
               << (i < cumWair.size() ? QString::number(cumWair[i], 'f', 4) : "") << ";"
               << (i < cumWfluid.size() ? QString::number(cumWfluid[i], 'f', 4) : "") << ";"
               << m_fluidDensitySpin->value() << "\n";
    }
    
    file.close();
}

void MainWindow::loadLayoutFromCSV(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось открыть файл: " + filename);
        return;
    }
    
    QTextStream stream(&file);
    
    QString header = stream.readLine();
    
    m_layoutItems.clear();
    double fluidDensity = 1200.0;
    
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty()) continue;
        
        QStringList fields = line.split(';');
        if (fields.size() < 12) continue;
        
        LayoutItem item;
        item.tool.setName(fields[0]);
        item.tool.setOuterDiameter(fields[1].toDouble());
        item.tool.setInnerDiameter(fields[2].toDouble());
        item.tool.setWeightPerMeter(fields[3].toDouble());
        item.length = fields[4].toDouble();
        
        m_layoutItems.append(item);
        
        if (fields.size() > 11) {
            fluidDensity = fields[11].toDouble();
        }
    }
    
    m_fluidDensitySpin->setValue(fluidDensity);
    file.close();
    
    refreshLayoutTable();
    autoSaveAll();
    updateVisualization();
}

void MainWindow::saveWellToCSV(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось сохранить файл: " + filename);
        return;
    }
    
    QTextStream stream(&file);
    stream << "Название;Нач_глубина_м;Кон_глубина_м;D_наруж_мм;D_внутр_мм;Голый_ствол;Кавернозность;Расход_л_с\n";
    
    for (int i = 0; i < m_wellModel->rowCount(); ++i) {
        stream << (m_wellModel->item(i, 0) ? m_wellModel->item(i, 0)->text() : "") << ";"
               << (m_wellModel->item(i, 1) ? m_wellModel->item(i, 1)->text() : "0") << ";"
               << (m_wellModel->item(i, 2) ? m_wellModel->item(i, 2)->text() : "0") << ";"
               << (m_wellModel->item(i, 3) ? m_wellModel->item(i, 3)->text() : "0") << ";"
               << (m_wellModel->item(i, 4) ? m_wellModel->item(i, 4)->text() : "0") << ";"
               << (m_wellModel->item(i, 5) ? m_wellModel->item(i, 5)->text() : "Нет") << ";"
               << (m_wellModel->item(i, 6) ? m_wellModel->item(i, 6)->text() : "1.0") << ";"
               << m_flowRateSpin->value() << "\n";
    }
    
    file.close();
}

void MainWindow::loadWellFromCSV(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось открыть файл: " + filename);
        return;
    }
    
    QTextStream stream(&file);
    
    QString header = stream.readLine();
    
    m_wellModel->removeRows(0, m_wellModel->rowCount());
    double flowRate = 30.0;
    
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty()) continue;
        
        QStringList fields = line.split(';');
        if (fields.size() < 7) continue;
        
        QList<QStandardItem*> row;
        row.append(new QStandardItem(fields[0]));
        row.append(new QStandardItem(fields[1]));
        row.append(new QStandardItem(fields[2]));
        row.append(new QStandardItem(fields[3]));
        row.append(new QStandardItem(fields[4]));
        row.append(new QStandardItem(fields[5]));
        row.append(new QStandardItem(fields[6]));
        m_wellModel->appendRow(row);
        
        if (fields.size() > 7) {
            flowRate = fields[7].toDouble();
        }
    }
    
    m_flowRateSpin->setValue(flowRate);
    file.close();
    
    onCalculateCirculation();
    autoSaveAll();
}

// --- Основные слоты ---

void MainWindow::onAddTool()
{
    Tool tool;
    tool.setName("Новый инструмент");
    tool.setOuterDiameter(127.0);
    tool.setInnerDiameter(108.62);
    tool.setWeightPerMeter(29.0);
    tool.setVolumePerMeter(0.0);
    tool.setDensity(7850.0);
    m_toolModel->addTool(tool);
    m_toolModel->saveToFile(m_dataPath + "/tools.dat");
}

void MainWindow::onRemoveTool()
{
    QModelIndex idx = m_toolTable->currentIndex();
    if (!idx.isValid()) {
        QMessageBox::information(this, "Внимание", "Выберите строку для удаления");
        return;
    }
    m_toolModel->removeTool(idx.row());
    m_toolModel->saveToFile(m_dataPath + "/tools.dat");
}

void MainWindow::onCalculateVolume()
{
    QModelIndex idx = m_toolTable->currentIndex();
    if (!idx.isValid()) {
        QMessageBox::information(this, "Внимание", "Выберите инструмент");
        return;
    }
    Tool tool = m_toolModel->toolAt(idx.row());
    double calcVol = tool.calculateVolumeFromWeight();
    m_toolModel->setData(m_toolModel->index(idx.row(), ToolModel::VolumePerMeter), calcVol);
    m_toolModel->saveToFile(m_dataPath + "/tools.dat");
}

void MainWindow::onTableCellChanged()
{
    m_toolModel->saveToFile(m_dataPath + "/tools.dat");
}

void MainWindow::onAddToLayout()
{
    QModelIndex idx = m_toolTable->currentIndex();
    if (!idx.isValid()) {
        QMessageBox::information(this, "Внимание", "Выберите инструмент в справочнике");
        return;
    }

    Tool tool = m_toolModel->toolAt(idx.row());
    LayoutItem item;
    item.tool = tool;
    item.length = 1.0;
    m_layoutItems.append(item);

    refreshLayoutTable();
    autoSaveAll();
    updateVisualization();
}

void MainWindow::onRemoveFromLayout()
{
    QModelIndex idx = m_layoutTable->currentIndex();
    if (!idx.isValid()) return;

    int row = idx.row();
    if (row < m_layoutItems.size()) {
        m_layoutItems.removeAt(row);
        refreshLayoutTable();
        autoSaveAll();
        updateVisualization();
    }
}

void MainWindow::onMoveLayoutUp()
{
    QModelIndex idx = m_layoutTable->currentIndex();
    if (!idx.isValid()) return;
    
    int row = idx.row();
    if (row <= 0 || row >= m_layoutItems.size()) return;
    
    std::swap(m_layoutItems[row], m_layoutItems[row - 1]);
    
    refreshLayoutTable();
    m_layoutTable->selectRow(row - 1);
    
    autoSaveAll();
    updateVisualization();
}

void MainWindow::onMoveLayoutDown()
{
    QModelIndex idx = m_layoutTable->currentIndex();
    if (!idx.isValid()) return;
    
    int row = idx.row();
    if (row < 0 || row >= m_layoutItems.size() - 1) return;
    
    std::swap(m_layoutItems[row], m_layoutItems[row + 1]);
    
    refreshLayoutTable();
    m_layoutTable->selectRow(row + 1);
    
    autoSaveAll();
    updateVisualization();
}

void MainWindow::onAddCasing()
{
    QList<QStandardItem*> row;
    row.append(new QStandardItem("Новая колонна"));
    row.append(new QStandardItem("0"));
    row.append(new QStandardItem("100"));
    row.append(new QStandardItem("168"));
    row.append(new QStandardItem("148"));
    row.append(new QStandardItem("Нет"));
    row.append(new QStandardItem("1.0"));
    m_wellModel->appendRow(row);
    onCalculateCirculation();
}

void MainWindow::onRemoveCasing()
{
    QModelIndex idx = m_wellTable->currentIndex();
    if (idx.isValid()) {
        m_wellModel->removeRow(idx.row());
        onCalculateCirculation();
    }
}

void MainWindow::onMoveCasingUp()
{
    QModelIndex idx = m_wellTable->currentIndex();
    if (!idx.isValid()) return;
    
    int row = idx.row();
    if (row <= 0 || row >= m_wellModel->rowCount()) return;
    
    QList<QStandardItem*> currentRow;
    QList<QStandardItem*> prevRow;
    
    for (int col = 0; col < m_wellModel->columnCount(); ++col) {
        currentRow.append(m_wellModel->item(row, col)->clone());
        prevRow.append(m_wellModel->item(row - 1, col)->clone());
    }
    
    for (int col = 0; col < m_wellModel->columnCount(); ++col) {
        m_wellModel->setItem(row - 1, col, currentRow[col]);
        m_wellModel->setItem(row, col, prevRow[col]);
    }
    
    m_wellTable->selectRow(row - 1);
    onCalculateCirculation();
}

void MainWindow::onMoveCasingDown()
{
    QModelIndex idx = m_wellTable->currentIndex();
    if (!idx.isValid()) return;
    
    int row = idx.row();
    if (row < 0 || row >= m_wellModel->rowCount() - 1) return;
    
    QList<QStandardItem*> currentRow;
    QList<QStandardItem*> nextRow;
    
    for (int col = 0; col < m_wellModel->columnCount(); ++col) {
        currentRow.append(m_wellModel->item(row, col)->clone());
        nextRow.append(m_wellModel->item(row + 1, col)->clone());
    }
    
    for (int col = 0; col < m_wellModel->columnCount(); ++col) {
        m_wellModel->setItem(row + 1, col, currentRow[col]);
        m_wellModel->setItem(row, col, nextRow[col]);
    }
    
    m_wellTable->selectRow(row + 1);
    onCalculateCirculation();
}

void MainWindow::onCalculateCirculation()
{
    m_wellConstruction.clear();
    for (int i = 0; i < m_wellModel->rowCount(); ++i) {
        Casing c;
        c.name = m_wellModel->item(i, 0) ? m_wellModel->item(i, 0)->text() : "";
        c.startDepth = m_wellModel->item(i, 1) ? m_wellModel->item(i, 1)->text().toDouble() : 0;
        c.endDepth = m_wellModel->item(i, 2) ? m_wellModel->item(i, 2)->text().toDouble() : 0;
        c.outerDiameter = m_wellModel->item(i, 3) ? m_wellModel->item(i, 3)->text().toDouble() : 0;
        
        QString holeStr = m_wellModel->item(i, 5) ? m_wellModel->item(i, 5)->text().toLower() : "";
        c.isOpenHole = (holeStr == "да" || holeStr == "yes");
        
        if (c.isOpenHole) {
            c.innerDiameter = c.outerDiameter;
            c.cavernosity = m_wellModel->item(i, 6) ? m_wellModel->item(i, 6)->text().toDouble() : 1.0;
        } else {
            c.innerDiameter = m_wellModel->item(i, 4) ? m_wellModel->item(i, 4)->text().toDouble() : 0;
            c.cavernosity = 1.0;
        }
        
        m_wellConstruction.addCasing(c);
    }

    m_calculator.setWellConstruction(m_wellConstruction);
    m_calculator.setLayout(m_layoutItems);
    m_calculator.setFlowRate(m_flowRateSpin->value());
    m_calculator.setFluidDensity(m_fluidDensitySpin->value());

    double totalTimeMin = m_calculator.calculateCirculationTime();
    double bottomUpTimeMin = m_calculator.calculateBottomUpTime();
    double surfaceToBottomTimeMin = m_calculator.calculateSurfaceToBottomTime();
    double volume = m_calculator.totalWellVolume();
    double annulusVol = m_calculator.annulusVolume();
    double toolVol = m_calculator.toolInnerVolume();

    m_circulationTimeLabel->setText(QString("Полный цикл: %1 мин").arg(totalTimeMin, 0, 'f', 2));
    m_bottomUpTimeLabel->setText(QString("Забой→Устье: %1 мин").arg(bottomUpTimeMin, 0, 'f', 2));
    m_surfaceToBottomTimeLabel->setText(QString("Устье→Забой: %1 мин").arg(surfaceToBottomTimeMin, 0, 'f', 2));
    m_wellVolumeLabel->setText(QString("Объем: %1 м³ (затр: %2, инстр: %3)")
        .arg(volume, 0, 'f', 3)
        .arg(annulusVol, 0, 'f', 3)
        .arg(toolVol, 0, 'f', 3));

    autoSaveAll();
    updateVisualization();
}

void MainWindow::autoSaveAll()
{
    saveLayoutToCSV(m_dataPath + "/layout.csv");
    saveWellToCSV(m_dataPath + "/well.csv");
}

void MainWindow::onSaveLayout()
{
    QString filename = QFileDialog::getSaveFileName(this, "Сохранить компоновку (CSV)", 
                                                     QDir::homePath() + "/layout.csv",
                                                     "CSV файлы (*.csv)");
    if (filename.isEmpty()) return;
    saveLayoutToCSV(filename);
    QMessageBox::information(this, "Сохранение", "Компоновка сохранена в " + filename);
}

void MainWindow::onLoadLayout()
{
    QString filename = QFileDialog::getOpenFileName(this, "Загрузить компоновку (CSV)",
                                                     QDir::homePath(),
                                                     "CSV файлы (*.csv)");
    if (filename.isEmpty()) return;
    loadLayoutFromCSV(filename);
    QMessageBox::information(this, "Загрузка", "Компоновка загружена из " + filename);
}

void MainWindow::onSaveWell()
{
    QString filename = QFileDialog::getSaveFileName(this, "Сохранить конструкцию скважины (CSV)",
                                                     QDir::homePath() + "/well.csv",
                                                     "CSV файлы (*.csv)");
    if (filename.isEmpty()) return;
    saveWellToCSV(filename);
    QMessageBox::information(this, "Сохранение", "Конструкция скважины сохранена в " + filename);
}

void MainWindow::onLoadWell()
{
    QString filename = QFileDialog::getOpenFileName(this, "Загрузить конструкцию скважины (CSV)",
                                                     QDir::homePath(),
                                                     "CSV файлы (*.csv)");
    if (filename.isEmpty()) return;
    loadWellFromCSV(filename);
    QMessageBox::information(this, "Загрузка", "Конструкция скважины загружена из " + filename);
}
