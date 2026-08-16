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
#include <QFile>
#include <QDir>
#include <QSettings>
#include <QTimer>
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
        loadLayoutFromCSV(layoutFile, false);

    QString wellFile = m_dataPath + "/well.csv";
    if (QFile::exists(wellFile))
        loadWellFromCSV(wellFile, false);

    connect(m_toolModel, &QAbstractItemModel::dataChanged,
            this, &MainWindow::onTableCellChanged);

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

    // false сделано для того, чтобы ширина последнего столбца
    // тоже могла сохраняться и восстанавливаться
    m_toolTable->horizontalHeader()->setStretchLastSection(false);

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

    connect(m_addToolBtn, &QPushButton::clicked,
            this, &MainWindow::onAddTool);

    connect(m_removeToolBtn, &QPushButton::clicked,
            this, &MainWindow::onRemoveTool);

    connect(m_calcVolumeBtn, &QPushButton::clicked,
            this, &MainWindow::onCalculateVolume);

    m_tabWidget->addTab(tab, "Справочник");
}

void MainWindow::createLayoutTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);

    m_layoutTable = new QTableView();
    m_layoutModel->setHorizontalHeaderLabels({
        "Название",
        "D наруж, мм",
        "D внутр, мм",
        "Вес, кг/м",
        "Длина, м",
        "Объем, м³",
        "Вес в возд, т",
        "Вес в жидк, т",
        "Сум. объем, м³",
        "Сум. вес возд, т",
        "Сум. вес жидк, т",
        "Нараст. длина, м"
    });

    m_layoutTable->setModel(m_layoutModel);
    m_layoutTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    // false сделано для того, чтобы ширина последнего столбца
    // тоже могла сохраняться и восстанавливаться
    m_layoutTable->horizontalHeader()->setStretchLastSection(false);

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

    connect(m_addToLayoutBtn, &QPushButton::clicked,
            this, &MainWindow::onAddToLayout);

    connect(m_removeFromLayoutBtn, &QPushButton::clicked,
            this, &MainWindow::onRemoveFromLayout);

    connect(m_moveLayoutUpBtn, &QPushButton::clicked,
            this, &MainWindow::onMoveLayoutUp);

    connect(m_moveLayoutDownBtn, &QPushButton::clicked,
            this, &MainWindow::onMoveLayoutDown);

    connect(m_fluidDensitySpin,
            static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this, [this](double) {
        refreshLayoutTable();
        autoSaveAll();
        updateVisualization();
    });

    connect(m_layoutModel, &QStandardItemModel::itemChanged,
            this, [this](QStandardItem *item) {
        if (!item || m_updatingLayout)
            return;

        // Редактируется только колонка "Длина, м"
        if (item->column() != 4)
            return;

        int row = item->row();
        if (row < 0 || row >= m_layoutItems.size())
            return;

        bool ok = false;
        double newLength = item->text().toDouble(&ok);

        if (!ok || newLength <= 0.0) {
            // Если ввели некорректное значение, вернуть старое отображение
            QTimer::singleShot(0, this, [this]() {
                refreshLayoutTable();
            });
            return;
        }

        m_layoutItems[row].length = newLength;

        // Отложенное обновление, чтобы не удалять элемент из модели
        // прямо во время обработки его собственного сигнала itemChanged
        QTimer::singleShot(0, this, [this]() {
            refreshLayoutTable();
            autoSaveAll();
            updateVisualization();
        });
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
        "Название",
        "Нач. глубина, м",
        "Кон. глубина, м",
        "D наруж, мм",
        "D внутр, мм",
        "Голый ствол",
        "Кавернозность"
    });

    m_wellTable->setModel(m_wellModel);
    m_wellTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    // false сделано для того, чтобы ширина последнего столбца
    // тоже могла сохраняться и восстанавливаться
    m_wellTable->horizontalHeader()->setStretchLastSection(false);

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

    connect(m_addCasingBtn, &QPushButton::clicked,
            this, &MainWindow::onAddCasing);

    connect(m_removeCasingBtn, &QPushButton::clicked,
            this, &MainWindow::onRemoveCasing);

    connect(m_moveCasingUpBtn, &QPushButton::clicked,
            this, &MainWindow::onMoveCasingUp);

    connect(m_moveCasingDownBtn, &QPushButton::clicked,
            this, &MainWindow::onMoveCasingDown);

    connect(calcCircBtn, &QPushButton::clicked,
            this, &MainWindow::onCalculateCirculation);

    connect(m_wellModel, &QStandardItemModel::itemChanged,
            this, [this](QStandardItem*) {
        if (m_updatingWell)
            return;

        onCalculateCirculation();
    });

    connect(zoomInBtn, &QPushButton::clicked,
            m_visualization, &WellVisualization::zoomIn);

    connect(zoomOutBtn, &QPushButton::clicked,
            m_visualization, &WellVisualization::zoomOut);

    connect(fitBtn, &QPushButton::clicked,
            m_visualization, &WellVisualization::fitToScreen);

    m_tabWidget->addTab(tab, "Конструкция скважины");
}

void MainWindow::createMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu("Файл");

    QAction *saveLayoutAct = fileMenu->addAction("Сохранить компоновку (CSV)");
    connect(saveLayoutAct, &QAction::triggered,
            this, &MainWindow::onSaveLayout);

    QAction *loadLayoutAct = fileMenu->addAction("Загрузить компоновку (CSV)");
    connect(loadLayoutAct, &QAction::triggered,
            this, &MainWindow::onLoadLayout);

    fileMenu->addSeparator();

    QAction *saveWellAct = fileMenu->addAction("Сохранить конструкцию скважины (CSV)");
    connect(saveWellAct, &QAction::triggered,
            this, &MainWindow::onSaveWell);

    QAction *loadWellAct = fileMenu->addAction("Загрузить конструкцию скважины (CSV)");
    connect(loadWellAct, &QAction::triggered,
            this, &MainWindow::onLoadWell);

    QMenu *helpMenu = menuBar()->addMenu("Справка");

    QAction *aboutAct = helpMenu->addAction("О программе");
    connect(aboutAct, &QAction::triggered,
            this, &MainWindow::onAbout);
}

void MainWindow::applySettings()
{
    QSettings settings;
    settings.beginGroup("MainWindow");

    resize(settings.value("size", QSize(1400, 900)).toSize());
    move(settings.value("pos", QPoint(200, 200)).toPoint());

    // Восстановление ширины столбцов таблицы "Справочник"
    if (m_toolTable) {
        QByteArray toolHeaderState = settings.value("toolTableHeaderState").toByteArray();
        if (!toolHeaderState.isEmpty()) {
            m_toolTable->horizontalHeader()->restoreState(toolHeaderState);
        }
    }

    // Восстановление ширины столбцов таблицы "Компоновка"
    if (m_layoutTable) {
        QByteArray layoutHeaderState = settings.value("layoutTableHeaderState").toByteArray();
        if (!layoutHeaderState.isEmpty()) {
            m_layoutTable->horizontalHeader()->restoreState(layoutHeaderState);
        }
    }

    // Восстановление ширины столбцов таблицы "Конструкция скважины"
    if (m_wellTable) {
        QByteArray wellHeaderState = settings.value("wellTableHeaderState").toByteArray();
        if (!wellHeaderState.isEmpty()) {
            m_wellTable->horizontalHeader()->restoreState(wellHeaderState);
        }
    }

    settings.endGroup();
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.beginGroup("MainWindow");

    settings.setValue("size", size());
    settings.setValue("pos", pos());

    // Сохранение ширины столбцов таблицы "Справочник"
    if (m_toolTable) {
        settings.setValue("toolTableHeaderState",
                          m_toolTable->horizontalHeader()->saveState());
    }

    // Сохранение ширины столбцов таблицы "Компоновка"
    if (m_layoutTable) {
        settings.setValue("layoutTableHeaderState",
                          m_layoutTable->horizontalHeader()->saveState());
    }

    // Сохранение ширины столбцов таблицы "Конструкция скважины"
    if (m_wellTable) {
        settings.setValue("wellTableHeaderState",
                          m_wellTable->horizontalHeader()->saveState());
    }

    settings.endGroup();
}

void MainWindow::updateVisualization()
{
    if (!m_visualization)
        return;

    WellConstruction wc;

    for (int i = 0; i < m_wellModel->rowCount(); ++i) {
        Casing c;

        c.name = m_wellModel->item(i, 0)
                ? m_wellModel->item(i, 0)->text()
                : "";

        c.startDepth = m_wellModel->item(i, 1)
                ? m_wellModel->item(i, 1)->text().toDouble()
                : 0.0;

        c.endDepth = m_wellModel->item(i, 2)
                ? m_wellModel->item(i, 2)->text().toDouble()
                : 0.0;

        c.outerDiameter = m_wellModel->item(i, 3)
                ? m_wellModel->item(i, 3)->text().toDouble()
                : 0.0;

        QString holeStr = m_wellModel->item(i, 5)
                ? m_wellModel->item(i, 5)->text().toLower()
                : "";

        c.isOpenHole = (holeStr == "да" || holeStr == "yes" || holeStr == "1");

        if (c.isOpenHole) {
            c.innerDiameter = c.outerDiameter;
            c.cavernosity = m_wellModel->item(i, 6)
                    ? m_wellModel->item(i, 6)->text().toDouble()
                    : 1.0;
        } else {
            c.innerDiameter = m_wellModel->item(i, 4)
                    ? m_wellModel->item(i, 4)->text().toDouble()
                    : 0.0;
            c.cavernosity = 1.0;
        }

        wc.addCasing(c);
    }

    m_visualization->setWellConstruction(wc);
    m_visualization->setLayout(m_layoutItems);
}

QVector<double> MainWindow::cumulativeLayoutLength() const
{
    QVector<double> result;
    result.resize(m_layoutItems.size());

    double total = 0.0;

    // Текущий вариант: накопление от забоя к устью,
    // то есть сумма текущего элемента и всех элементов ниже него.
    // Если нужно нарастание сверху вниз, замените цикл на прямой:
    // for (int i = 0; i < m_layoutItems.size(); ++i) { ... }
    for (int i = m_layoutItems.size() - 1; i >= 0; --i) {
        total += m_layoutItems[i].length * m_layoutItems[i].quantity;
        result[i] = total;
    }

    return result;
}

void MainWindow::refreshLayoutTable()
{
    // Устанавливаем флаг, чтобы программное обновление таблицы
    // не вызывало обработчик itemChanged и повторное автосохранение
    m_updatingLayout = true;

    m_layoutModel->removeRows(0, m_layoutModel->rowCount());

    m_calculator.setLayout(m_layoutItems);
    m_calculator.setFluidDensity(m_fluidDensitySpin->value());

    QVector<double> cumVol = m_calculator.cumulativeVolume();
    QVector<double> cumWair = m_calculator.cumulativeWeightAir();
    QVector<double> cumWfluid = m_calculator.cumulativeWeightFluid();
    QVector<double> cumLen = cumulativeLayoutLength();

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
        row.append(makeItem(QString::number(item.tool.weightPerMeter(), 'f', 6), false));
        row.append(makeItem(QString::number(item.length, 'f', 6), true));
        row.append(makeItem(QString::number(item.volumeInner(), 'f', 4), false));
        row.append(makeItem(QString::number(item.weightInAir(), 'f', 4), false));
        row.append(makeItem(QString::number(item.weightInFluid(m_fluidDensitySpin->value()), 'f', 4), false));

        // Кумулятивные суммы: от данного элемента до забоя
        row.append(makeItem(i < cumVol.size() ? QString::number(cumVol[i], 'f', 4) : "", false));
        row.append(makeItem(i < cumWair.size() ? QString::number(cumWair[i], 'f', 4) : "", false));
        row.append(makeItem(i < cumWfluid.size() ? QString::number(cumWfluid[i], 'f', 4) : "", false));

        // Нарастающая длина инструмента
        row.append(makeItem(i < cumLen.size() ? QString::number(cumLen[i], 'f', 3) : "", false));

        m_layoutModel->appendRow(row);
    }

    m_updatingLayout = false;

    m_totalWeightAirLabel->setText(QString("Вес в воздухе: %1 т")
        .arg(m_calculator.totalWeightInAir(), 0, 'f', 3));

    m_totalWeightFluidLabel->setText(QString("Вес в жидкости: %1 т")
        .arg(m_calculator.totalWeightInFluid(), 0, 'f', 3));

    double totalVol = 0.0;
    for (const auto &item : m_layoutItems)
        totalVol += item.volumeInner();

    m_totalVolumeLabel->setText(QString("Объем: %1 м³").arg(totalVol, 0, 'f', 4));

    // Принудительно обновляем отображение таблицы
    if (m_layoutTable) {
        m_layoutTable->viewport()->update();
    }
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

    stream << "Название;D_наруж_мм;D_внутр_мм;Вес_кг_м;Длина_м;"
              "Объем_м3;Вес_возд_т;Вес_жидк_т;"
              "Сум_объем_м3;Сум_вес_возд_т;Сум_вес_жидк_т;"
              "Плотность_жидк_кг_м3;Нараст_длина_м\n";

    m_calculator.setLayout(m_layoutItems);
    m_calculator.setFluidDensity(m_fluidDensitySpin->value());

    QVector<double> cumVol = m_calculator.cumulativeVolume();
    QVector<double> cumWair = m_calculator.cumulativeWeightAir();
    QVector<double> cumWfluid = m_calculator.cumulativeWeightFluid();
    QVector<double> cumLen = cumulativeLayoutLength();

    for (int i = 0; i < m_layoutItems.size(); ++i) {
        const auto &item = m_layoutItems[i];

        stream << item.tool.name() << ";"
               << QString::number(item.tool.outerDiameter(), 'f', 6) << ";"
               << QString::number(item.tool.innerDiameter(), 'f', 6) << ";"
               << QString::number(item.tool.weightPerMeter(), 'f', 6) << ";"
               << QString::number(item.length, 'f', 6) << ";"
               << QString::number(item.volumeInner(), 'f', 6) << ";"
               << QString::number(item.weightInAir(), 'f', 6) << ";"
               << QString::number(item.weightInFluid(m_fluidDensitySpin->value()), 'f', 6) << ";"
               << (i < cumVol.size() ? QString::number(cumVol[i], 'f', 6) : "") << ";"
               << (i < cumWair.size() ? QString::number(cumWair[i], 'f', 6) : "") << ";"
               << (i < cumWfluid.size() ? QString::number(cumWfluid[i], 'f', 6) : "") << ";"
               << QString::number(m_fluidDensitySpin->value(), 'f', 6) << ";"
               << (i < cumLen.size() ? QString::number(cumLen[i], 'f', 3) : "") << "\n";
    }

    file.close();
}

void MainWindow::loadLayoutFromCSV(const QString &filename, bool autoSave)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось открыть файл: " + filename);
        return;
    }

    QTextStream stream(&file);

    // Заголовок
    if (!stream.atEnd())
        stream.readLine();

    m_layoutItems.clear();
    double fluidDensity = 1200.0;

    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty())
            continue;

        QStringList fields = line.split(';');

        // Минимально необходимые поля:
        // 0 название
        // 1 наружный диаметр
        // 2 внутренний диаметр
        // 3 вес кг/м
        // 4 длина
        if (fields.size() < 5)
            continue;

        LayoutItem item;

        item.tool.setName(fields[0]);
        item.tool.setOuterDiameter(fields[1].toDouble());
        item.tool.setInnerDiameter(fields[2].toDouble());
        item.tool.setWeightPerMeter(fields[3].toDouble());
        item.length = fields[4].toDouble();

        m_layoutItems.append(item);

        if (fields.size() > 11)
            fluidDensity = fields[11].toDouble();
    }

    file.close();

    // Блокируем сигналы, чтобы setValue не вызвал автосохранение
    m_fluidDensitySpin->blockSignals(true);
    m_fluidDensitySpin->setValue(fluidDensity);
    m_fluidDensitySpin->blockSignals(false);

    refreshLayoutTable();

    if (autoSave)
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

    stream << "Название;Нач_глубина_м;Кон_глубина_м;D_наруж_мм;D_внутр_мм;"
              "Голый_ствол;Кавернозность;Расход_л_с\n";

    for (int i = 0; i < m_wellModel->rowCount(); ++i) {
        stream << (m_wellModel->item(i, 0) ? m_wellModel->item(i, 0)->text() : "") << ";"
               << (m_wellModel->item(i, 1) ? m_wellModel->item(i, 1)->text() : "0") << ";"
               << (m_wellModel->item(i, 2) ? m_wellModel->item(i, 2)->text() : "0") << ";"
               << (m_wellModel->item(i, 3) ? m_wellModel->item(i, 3)->text() : "0") << ";"
               << (m_wellModel->item(i, 4) ? m_wellModel->item(i, 4)->text() : "0") << ";"
               << (m_wellModel->item(i, 5) ? m_wellModel->item(i, 5)->text() : "Нет") << ";"
               << (m_wellModel->item(i, 6) ? m_wellModel->item(i, 6)->text() : "1.0") << ";"
               << QString::number(m_flowRateSpin->value(), 'f', 6) << "\n";
    }

    file.close();
}

void MainWindow::loadWellFromCSV(const QString &filename, bool autoSave)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось открыть файл: " + filename);
        return;
    }

    QTextStream stream(&file);

    // Заголовок
    if (!stream.atEnd())
        stream.readLine();

    m_updatingWell = true;

    m_wellModel->removeRows(0, m_wellModel->rowCount());

    double flowRate = 30.0;

    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty())
            continue;

        QStringList fields = line.split(';');

        if (fields.size() < 7)
            continue;

        QList<QStandardItem*> row;

        row.append(new QStandardItem(fields[0]));
        row.append(new QStandardItem(fields[1]));
        row.append(new QStandardItem(fields[2]));
        row.append(new QStandardItem(fields[3]));
        row.append(new QStandardItem(fields[4]));
        row.append(new QStandardItem(fields[5]));
        row.append(new QStandardItem(fields[6]));

        m_wellModel->appendRow(row);

        if (fields.size() > 7)
            flowRate = fields[7].toDouble();
    }

    m_updatingWell = false;

    file.close();

    m_flowRateSpin->blockSignals(true);
    m_flowRateSpin->setValue(flowRate);
    m_flowRateSpin->blockSignals(false);

    // Пересчитываем циркуляцию без автосохранения
    recalculateCirculation(false);

    if (autoSave)
        autoSaveAll();

    if (m_wellTable) {
        m_wellTable->viewport()->update();
    }
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
    if (!idx.isValid())
        return;

    int row = idx.row();
    if (row >= 0 && row < m_layoutItems.size()) {
        m_layoutItems.removeAt(row);

        refreshLayoutTable();
        autoSaveAll();
        updateVisualization();
    }
}

void MainWindow::onMoveLayoutUp()
{
    QModelIndex idx = m_layoutTable->currentIndex();
    if (!idx.isValid())
        return;

    int row = idx.row();
    if (row <= 0 || row >= m_layoutItems.size())
        return;

    std::swap(m_layoutItems[row], m_layoutItems[row - 1]);

    refreshLayoutTable();
    m_layoutTable->selectRow(row - 1);

    autoSaveAll();
    updateVisualization();
}

void MainWindow::onMoveLayoutDown()
{
    QModelIndex idx = m_layoutTable->currentIndex();
    if (!idx.isValid())
        return;

    int row = idx.row();
    if (row < 0 || row >= m_layoutItems.size() - 1)
        return;

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

    m_updatingWell = true;
    m_wellModel->appendRow(row);
    m_updatingWell = false;

    onCalculateCirculation();
}

void MainWindow::onRemoveCasing()
{
    QModelIndex idx = m_wellTable->currentIndex();
    if (!idx.isValid())
        return;

    m_updatingWell = true;
    m_wellModel->removeRow(idx.row());
    m_updatingWell = false;

    onCalculateCirculation();
}

void MainWindow::onMoveCasingUp()
{
    QModelIndex idx = m_wellTable->currentIndex();
    if (!idx.isValid())
        return;

    int row = idx.row();
    if (row <= 0 || row >= m_wellModel->rowCount())
        return;

    // Проверка, что обе строки существуют
    for (int col = 0; col < m_wellModel->columnCount(); ++col) {
        if (!m_wellModel->item(row, col) || !m_wellModel->item(row - 1, col))
            return;
    }

    QList<QStandardItem*> currentRow;
    QList<QStandardItem*> prevRow;

    for (int col = 0; col < m_wellModel->columnCount(); ++col) {
        currentRow.append(m_wellModel->item(row, col)->clone());
        prevRow.append(m_wellModel->item(row - 1, col)->clone());
    }

    m_updatingWell = true;

    for (int col = 0; col < m_wellModel->columnCount(); ++col) {
        m_wellModel->setItem(row - 1, col, currentRow[col]);
        m_wellModel->setItem(row, col, prevRow[col]);
    }

    m_updatingWell = false;

    m_wellTable->selectRow(row - 1);

    onCalculateCirculation();
}

void MainWindow::onMoveCasingDown()
{
    QModelIndex idx = m_wellTable->currentIndex();
    if (!idx.isValid())
        return;

    int row = idx.row();
    if (row < 0 || row >= m_wellModel->rowCount() - 1)
        return;

    // Проверка, что обе строки существуют
    for (int col = 0; col < m_wellModel->columnCount(); ++col) {
        if (!m_wellModel->item(row, col) || !m_wellModel->item(row + 1, col))
            return;
    }

    QList<QStandardItem*> currentRow;
    QList<QStandardItem*> nextRow;

    for (int col = 0; col < m_wellModel->columnCount(); ++col) {
        currentRow.append(m_wellModel->item(row, col)->clone());
        nextRow.append(m_wellModel->item(row + 1, col)->clone());
    }

    m_updatingWell = true;

    for (int col = 0; col < m_wellModel->columnCount(); ++col) {
        m_wellModel->setItem(row + 1, col, currentRow[col]);
        m_wellModel->setItem(row, col, nextRow[col]);
    }

    m_updatingWell = false;

    m_wellTable->selectRow(row + 1);

    onCalculateCirculation();
}

void MainWindow::onCalculateCirculation()
{
    recalculateCirculation(true);
}

void MainWindow::recalculateCirculation(bool autoSave)
{
    m_wellConstruction.clear();

    for (int i = 0; i < m_wellModel->rowCount(); ++i) {
        Casing c;

        c.name = m_wellModel->item(i, 0)
                ? m_wellModel->item(i, 0)->text()
                : "";

        c.startDepth = m_wellModel->item(i, 1)
                ? m_wellModel->item(i, 1)->text().toDouble()
                : 0.0;

        c.endDepth = m_wellModel->item(i, 2)
                ? m_wellModel->item(i, 2)->text().toDouble()
                : 0.0;

        c.outerDiameter = m_wellModel->item(i, 3)
                ? m_wellModel->item(i, 3)->text().toDouble()
                : 0.0;

        QString holeStr = m_wellModel->item(i, 5)
                ? m_wellModel->item(i, 5)->text().toLower()
                : "";

        c.isOpenHole = (holeStr == "да" || holeStr == "yes" || holeStr == "1");

        if (c.isOpenHole) {
            c.innerDiameter = c.outerDiameter;
            c.cavernosity = m_wellModel->item(i, 6)
                    ? m_wellModel->item(i, 6)->text().toDouble()
                    : 1.0;
        } else {
            c.innerDiameter = m_wellModel->item(i, 4)
                    ? m_wellModel->item(i, 4)->text().toDouble()
                    : 0.0;
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

    if (autoSave)
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
    if (filename.isEmpty())
        return;

    saveLayoutToCSV(filename);

    QMessageBox::information(this, "Сохранение", "Компоновка сохранена в " + filename);
}

void MainWindow::onLoadLayout()
{
    QString filename = QFileDialog::getOpenFileName(this, "Загрузить компоновку (CSV)",
                                                     QDir::homePath(),
                                                     "CSV файлы (*.csv)");
    if (filename.isEmpty())
        return;

    loadLayoutFromCSV(filename, true);

    QMessageBox::information(this, "Загрузка", "Компоновка загружена из " + filename);
}

void MainWindow::onSaveWell()
{
    QString filename = QFileDialog::getSaveFileName(this, "Сохранить конструкцию скважины (CSV)",
                                                     QDir::homePath() + "/well.csv",
                                                     "CSV файлы (*.csv)");
    if (filename.isEmpty())
        return;

    saveWellToCSV(filename);

    QMessageBox::information(this, "Сохранение", "Конструкция скважины сохранена в " + filename);
}

void MainWindow::onLoadWell()
{
    QString filename = QFileDialog::getOpenFileName(this, "Загрузить конструкцию скважины (CSV)",
                                                     QDir::homePath(),
                                                     "CSV файлы (*.csv)");
    if (filename.isEmpty())
        return;

    loadWellFromCSV(filename, true);

    QMessageBox::information(this, "Загрузка", "Конструкция скважины загружена из " + filename);
}

void MainWindow::onAbout()
{
    QMessageBox::about(this,
        "О программе",
        "<b>GeoFluxMass</b><br><br>"
        "Программа для расчета бурового инструмента в жидкости.<br><br>"
        "Возможности:<br>"
        "- справочник инструмента;<br>"
        "- расчет компоновки;<br>"
        "- расчет веса в воздухе и в жидкости;<br>"
        "- расчет циркуляции;<br>"
        "- конструкция скважины;<br>"
        "- визуализация компоновки и скважины.<br><br>"
        "Версия: 0.0.0.2"
    );
}
