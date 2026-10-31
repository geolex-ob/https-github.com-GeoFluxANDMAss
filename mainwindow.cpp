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
#include <QRegularExpression>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QCoreApplication>
#include <QMessageBox>
#include <QMenuBar>
#include <QStatusBar>
#include <QStringConverter>
#include <QSettings>
#include <QCloseEvent>
#include <QLineEdit>
#include <cmath>
#include <algorithm>

// Единый формат файла knbk.csv. Колонки 4 и 5 идут в порядке "Масса 1 м, кг; Объем 1 м, л"
// (так же, как в таблице КНБК главного окна). Редактор справочника заполняет только
// колонки 0..5, поэтому структура файла не зависит от того, кто последним его сохранил.
// Объявлено в mainwindow.h, чтобы редактор справочника использовал тот же порядок колонок.
const QStringList KNBK_CSV_HEADER = {
    "Шифр", "Длина, м", "Наружный d, мм", "Внутренний d, мм",
    "Масса 1 м, кг", "Объем 1 м, л", "Режим", "Масса всего, т", "Объем всего, м3"
};

// Плотность стали, кг/м3 (для авторасчёта массы/объёма погонного метра)
static const double STEEL_DENSITY_KG_M3_REF = 7850.0;

// Признак ручного ввода значений в справочнике: если масса 1 м отличается от
// геометрического расчёта pi/4*(OD^2-ID^2)*7850 более чем на 2%, считаем её
// введённой вручную (паспортные данные: утяжеленные буры, канавки, наплавка,
// другая плотность материала) и НЕ перетираем авто-расчётом. То же для объёма.
static bool massMatchesGeometry(double mass_kg_m, double od_mm, double id_mm)
{
    if (!(od_mm > 0 && id_mm > 0 && id_mm < od_mm)) return false; // нет геометрии — не спорим
    const double expected = M_PI / 4.0 * (std::pow(od_mm / 1000.0, 2) - std::pow(id_mm / 1000.0, 2))
                            * STEEL_DENSITY_KG_M3_REF;
    if (expected <= 0) return false;
    return std::fabs(mass_kg_m - expected) <= 0.02 * expected;
}

static bool volMatchesGeometry(double vol_l_m, double od_mm, double id_mm)
{
    if (!(od_mm > 0 && id_mm > 0 && id_mm < od_mm)) return false;
    const double expected = M_PI / 4.0 * (std::pow(od_mm / 1000.0, 2) - std::pow(id_mm / 1000.0, 2)) * 1000.0;
    if (expected <= 0) return false;
    return std::fabs(vol_l_m - expected) <= 0.005 * expected;
}

// Собственная плотность металла по паспортным данным справочника:
// rho_собств = масса 1 м [кг/м] / объём металла 1 м [л/м] * 1000 [кг/м3].
// Используется при расчете архимедовой силы для инструментов с ручным весом
// (УБТ, переводники с выточками, несталь), чтобы потеря веса в растворе
// соответствовала именно паспортному весу, а не гипотетической стали 7850.
// Результат ограничивается разумным диапазоном 500..20000 кг/м3.
static double effectiveSteelDensity(double mass_kg_m, double vol_l_m)
{
    if (mass_kg_m <= 0 || vol_l_m <= 0) return STEEL_DENSITY_KG_M3_REF;
    const double rho = mass_kg_m / vol_l_m * 1000.0;
    if (rho < 500.0 || rho > 20000.0) return STEEL_DENSITY_KG_M3_REF;
    return rho;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("Расчеты ГТИ");
    resize(1200, 800);

    auto *fileMenu = menuBar()->addMenu("Файл");
    fileMenu->addAction("Сохранить конструкцию", this, &MainWindow::saveConstruction);
    fileMenu->addAction("Сохранить компоновку КНБК", this, &MainWindow::saveKNBKLayout);
    fileMenu->addAction("Загрузить компоновку КНБК", this, &MainWindow::loadKNBKLayoutFromSettingsSlot);
    fileMenu->addSeparator();
    fileMenu->addAction("Выход", this, &QWidget::close);

    auto *refMenu = menuBar()->addMenu("Справочники");
    refMenu->addAction("Редактор КНБК", this, &MainWindow::openKNBKEditor);
    
    auto *helpMenu = menuBar()->addMenu("Справка");
    helpMenu->addAction("О программе", [this](){
        QMessageBox::about(this, "О программе", "Расчеты ГТИ. GeoFluxANDMAss.\nhttps://github.com/geolex-ob/https-github.com-GeoFluxANDMAss\nВерсия 0.1.1");
    });

    statusBar()->showMessage("Готово");

    setupUI();

    loadKNBKReference();
    // Справочник КНБК (knbk.csv) — только справочные данные.
    // Рабочая компоновка КНБК на главном экране хранится ОТДЕЛЬНО
    // в настройках приложения (QSettings) и восстанавливается при запуске,
    // включая набор элементов, их последовательность и режимы.
    loadCSV(getDataPath("well_construction.csv"), m_constructionTable);
    if (!loadKNBKLayout()) {
        // Первый запуск (настроек ещё нет): миграция из knbk_layout.csv,
        // затем из knbk.csv (прежнее поведение).
        if (!loadKNBKLayoutFromCsv(getDataPath("knbk_layout.csv")))
            loadKNBKLayoutFromCsv(getDataPath("knbk.csv"));
    }
}

MainWindow::~MainWindow() {}

// ================= АВТОСОХРАНЕНИЕ КОМПОНОВКИ КНБК =================

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveKNBKLayout();   // запоминаем последнюю компоновку при закрытии окна
    QMainWindow::closeEvent(event);
}

QString MainWindow::knbkLayoutSettingsKey() const
{
    return QStringLiteral("KNBKLayout/rows");
}

void MainWindow::saveKNBKLayout()
{
    if (m_loadingLayout) return;  // не сохраняем во время загрузки/миграции
    QSettings settings;   // организация/домен берутся из QCoreApplication (main.cpp)
    settings.beginWriteArray(knbkLayoutSettingsKey());
    for (int row = 0; row < m_knbkTable->rowCount(); ++row) {
        settings.setArrayIndex(row);

        QString code;
        QComboBox *codeCombo = qobject_cast<QComboBox*>(m_knbkTable->cellWidget(row, 0));
        if (codeCombo) {
            code = codeCombo->currentText();
            //Editable-комбобокс: если шифр удалён из справочника, currentText
            //всё равно возвращает введённый текст — сохраняем его как есть.
        }
        settings.setValue("code", code);

        QComboBox *modeCombo = qobject_cast<QComboBox*>(m_knbkTable->cellWidget(row, 6));
        settings.setValue("mode", modeCombo ? modeCombo->currentText() : QString("Справочник"));

        for (int col = 1; col <= 5; ++col) {
            QTableWidgetItem *item = m_knbkTable->item(row, col);
            settings.setValue(QString("c%1").arg(col), item ? item->text() : QString());
        }
        // Дополнительные колонки (Масса всего, Объем всего), если они есть
        for (int col = 7; col < m_knbkTable->columnCount(); ++col) {
            QTableWidgetItem *item = m_knbkTable->item(row, col);
            settings.setValue(QString("c%1").arg(col), item ? item->text() : QString());
        }
    }
    settings.endArray();
    settings.sync();
}

bool MainWindow::loadKNBKLayout()
{
    m_loadingLayout = true;
    struct Guard { MainWindow *w; ~Guard(){ w->m_loadingLayout = false; } } guard{this};

    QSettings settings;
    const int count = settings.beginReadArray(knbkLayoutSettingsKey());
    if (count <= 0) {
        settings.endArray();
        return false;   // сохранённой компоновки нет
    }

    m_knbkTable->setRowCount(0);
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        const QString code = settings.value("code").toString();
        const QString mode = settings.value("mode", "Справочник").toString();

        QStringList fields(9);
        fields[0] = code;
        for (int col = 1; col <= 5; ++col) fields[col] = settings.value(QString("c%1").arg(col)).toString();
        fields[6] = mode;
        for (int col = 7; col < 9; ++col) fields[col] = settings.value(QString("c%1").arg(col)).toString();

        addKNBKRow();
        fillKNBKRow(m_knbkTable->rowCount() - 1, fields);
    }
    settings.endArray();
    return true;
}

void MainWindow::fillKNBKRow(int row, const QStringList &fields)
{
    if (row < 0 || row >= m_knbkTable->rowCount() || fields.size() < 7) return;

    // Восстанавливаем выбор шифра в комбобоксе без автоподстановки из справочника:
    // в таблице должна вернуться ровно та компоновка, что была при закрытии
    // (включая вручную отредактированные длина/диаметры/масса/объем).
    QComboBox *combo = qobject_cast<QComboBox*>(m_knbkTable->cellWidget(row, 0));
    if (combo) {
        combo->blockSignals(true);
        int idx = combo->findText(fields[0]);
        if (idx >= 0) {
            combo->setCurrentIndex(idx);
        } else if (!fields[0].isEmpty()) {
            // Шифр удалён из справочника — оставляем его текстом в поле
            combo->setEditText(fields[0]);
        }
        combo->blockSignals(false);
    }

    QComboBox *modeCombo = qobject_cast<QComboBox*>(m_knbkTable->cellWidget(row, 6));
    const bool isManual = (modeCombo && fields.value(6) == "Вручную");
    if (modeCombo) {
        modeCombo->blockSignals(true);
        int idx = modeCombo->findText(fields[6]);
        modeCombo->setCurrentIndex(idx >= 0 ? idx : 0);
        modeCombo->blockSignals(false);
    }

    m_knbkTable->blockSignals(true);
    for (int col = 1; col <= 5; ++col) {
        if (col < fields.size() && m_knbkTable->item(row, col))
            m_knbkTable->item(row, col)->setText(fields[col]);
    }
    for (int col = 7; col < m_knbkTable->columnCount() && col < fields.size(); ++col) {
        if (m_knbkTable->item(row, col)) m_knbkTable->item(row, col)->setText(fields[col]);
    }
    m_knbkTable->blockSignals(false);

    // Восстанавливаем состояние редактируемости колонок "Масса 1 м"/"Объем 1 м"
    applyKNBKRowEditability(row, isManual);
}

bool MainWindow::loadKNBKLayoutFromCsv(const QString &filePath)
{
    m_loadingLayout = true;
    struct Guard { MainWindow *w; ~Guard(){ w->m_loadingLayout = false; } } guard{this};

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    QStringList allLines;
    while (!in.atEnd()) allLines << in.readLine();
    file.close();

    int startIdx = 0;
    if (!allLines.isEmpty()) {
        static const QRegularExpression lettersRe("[^0-9.;,\\s-]");
        if (allLines.first().contains(lettersRe)) startIdx = 1; // строка заголовка
    }

    bool any = false;
    m_knbkTable->setRowCount(0);
    for (int li = startIdx; li < allLines.size(); ++li) {
        const QString line = allLines[li];
        if (line.trimmed().isEmpty()) continue;
        QStringList fields = line.split(";");
        if (fields.size() < 7) continue;

        // Защита от перепутанных колонок "Масса/Объем" в старых файлах
        double vMass = fields[4].replace(",", ".").toDouble();
        double vVol  = fields[5].replace(",", ".").toDouble();
        if (vMass < 10.0 && vVol > 500.0) std::swap(fields[4], fields[5]);

        addKNBKRow();
        fillKNBKRow(m_knbkTable->rowCount() - 1, fields);
        any = true;
    }
    return any;
}

// Слот меню "Файл → Загрузить компоновку КНБК" (явная загрузка из QSettings)
void MainWindow::loadKNBKLayoutFromSettingsSlot()
{
    if (!loadKNBKLayout())
        statusBar()->showMessage("Сохранённая компоновка не найдена", 3000);
    else
        statusBar()->showMessage("Компоновка КНБК загружена из настроек", 3000);
}

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

    addInfoField("Производительность, л/сек:", "1.0");
    addInfoField("Вес тальблока, т:", "0.0");
    addInfoField("Вес квадрата, т:", "0.0");
    addInfoField("Коэфф. кавернозности:", "1.0");
    addInfoField("Глубина спуска, м:", "0.0");
    addInfoField("Диаметр долота, мм:", "215.9");
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
    m_knbkTable->setHorizontalHeaderLabels(KNBK_CSV_HEADER);
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
        "Вес инструмента в растворе с таль блока, т:",
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
    combo->setEditable(true);             // нужно для setEditText (шифр, удалённый из справочника)
    combo->lineEdit()->setReadOnly(true); // ввод с клавиатуры запрещён — только выбор из списка
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

// Управление редактируемостью колонок "Масса 1 м, кг" и "Объем 1 м, л" в строке
// таблицы компоновок: в режиме "Вручную" обе колонки доступны для ввода
// (можно задать паспортный вес погонного метра), в режиме "Справочник" — только
// чтение (серые), значения подтягиваются из справочника.
void MainWindow::applyKNBKRowEditability(int row, bool isManual)
{
    for (int col : {4, 5}) {
        QTableWidgetItem *it = m_knbkTable->item(row, col);
        if (!it) continue;
        if (isManual) {
            it->setFlags(it->flags() | Qt::ItemIsEditable);
            it->setBackground(QBrush(Qt::white));
        } else {
            it->setFlags(it->flags() & ~Qt::ItemIsEditable);
            it->setBackground(QBrush(Qt::lightGray));
        }
    }
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

        // Масса и объем погонного метра. Режим "Справочник" — значения берутся
        // из справочника как есть (включая ручной вес 1 м, введённый в Редакторе
        // справочника КНБК: он распознаётся при загрузке и не перетирается).
        // Если значений в справочнике нет — считаем по геометрии полового
        // инструмента V_мет = pi/4*(OD^2 - ID^2), W = V*7850.
        // В режиме "Вручную" масса/объем редактируются прямо в таблице компоновок.
        QComboBox *modeCombo = qobject_cast<QComboBox*>(m_knbkTable->cellWidget(row, 6));
        const bool isManual = (modeCombo && modeCombo->currentText() == "Вручную");
        QString odStr = data.outerD;
        QString idStr = data.innerD;
        odStr.replace(",", ".");
        idStr.replace(",", ".");
        double od_mm = odStr.toDouble();
        double id_mm = idStr.toDouble();

        QString massText = data.mass1m;
        QString volText  = data.vol1m;

        if (!isManual && (massText.trimmed().isEmpty() || volText.trimmed().isEmpty())
            && od_mm > 0 && id_mm > 0 && id_mm < od_mm) {
            const double d_out = od_mm / 1000.0;
            const double d_in = id_mm / 1000.0;
            const double v_metal_m3 = M_PI * (d_out * d_out - d_in * d_in) / 4.0;
            massText = QString::number(v_metal_m3 * STEEL_DENSITY_KG_M3_REF, 'f', 2);
            volText  = QString::number(v_metal_m3 * 1000.0, 'f', 3);
        }

        if(m_knbkTable->item(row, 4)) m_knbkTable->item(row, 4)->setText(massText);
        if(m_knbkTable->item(row, 5)) m_knbkTable->item(row, 5)->setText(volText);
        m_knbkTable->blockSignals(false);

        applyKNBKRowEditability(row, isManual);
        
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

    // При переключении в "Справочник" возвращаем значения из справочника,
    // при "Вручную" — оставляем то, что введено в таблице, и открываем
    // редактирование колонок "Масса 1 м" и "Объем 1 м".
    if (!isManual) {
        QComboBox *codeCombo = qobject_cast<QComboBox*>(m_knbkTable->cellWidget(row, 0));
        if (codeCombo && m_knbkReference.contains(codeCombo->currentText())) {
            const KNBKData &data = m_knbkReference[codeCombo->currentText()];
            m_knbkTable->blockSignals(true);
            if (m_knbkTable->item(row, 4)) m_knbkTable->item(row, 4)->setText(data.mass1m);
            if (m_knbkTable->item(row, 5)) m_knbkTable->item(row, 5)->setText(data.vol1m);
            m_knbkTable->blockSignals(false);
        }
    }
    applyKNBKRowEditability(row, isManual);
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
    saveKNBKLayout();   // сразу фиксируем последнюю компоновку
}

void MainWindow::removeKNBKRow()
{
    int row = m_knbkTable->currentRow();
    if (row >= 0) {
        m_knbkTable->removeRow(row);
        saveKNBKLayout();
    }
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

    // Файл читаем целиком: заголовком считаем только строку с буквами.
    // Старый код безусловно пропускал первую строку и терял первую запись,
    // если CSV был сохранен без заголовка.
    QStringList allLines;
    while (!in.atEnd()) allLines << in.readLine();
    file.close();

    table->setRowCount(0);

    int startIdx = 0;
    if (!allLines.isEmpty()) {
        // Буквы (в т.ч. кириллица) — признак строки заголовка, а не данных.
        static const QRegularExpression lettersRe("[^0-9.;,\\s-]");
        if (allLines.first().contains(lettersRe)) startIdx = 1;
    }

    for (int li = startIdx; li < allLines.size(); ++li) {
        const QString line = allLines[li];
        if (line.trimmed().isEmpty()) continue;
        QStringList fields = line.split(";");

        // Для таблицы КНБК контролируем порядок колонок "Масса 1 м, кг"(4) и
        // "Объем 1 м, л"(5): старые файлы могли быть сохранены с перепутанными
        // колонками. Масса 1 м не может быть меньше 10 кг/м, а объем внутренней
        // полости физически не превышает ~500 л/м — по этому признаку определяем
        // перестановку (в т.ч. для УБТ, где масса и объём сопоставимы).
        if (table == m_knbkTable && fields.size() >= 6) {
            double vMass = fields[4].replace(",", ".").toDouble();
            double vVol  = fields[5].replace(",", ".").toDouble();
            if (vMass < 10.0 && vVol > 500.0) std::swap(fields[4], fields[5]);
        }

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
}

void MainWindow::loadKNBKReference()
{
    m_knbkReference.clear();
    QFile file(getDataPath("knbk.csv"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    // Определяем номера колонок по заголовку CSV.
    // Это защищает от перестановки колонок "Масса 1 м" и "Объем 1 м":
    // старые файлы, сохраненные редактором справочника в формате
    // "...;Объем 1 м, л;Вес 1 м, кг", теперь читаются корректно.
    int colCode = 0, colLength = 1, colOuterD = 2, colInnerD = 3, colMass = 4, colVol = 5;
    bool headerParsed = false;

    if (!in.atEnd()) {
        const QString firstLine = in.readLine();
        const QStringList headers = firstLine.split(";");
        // Строку считаем заголовком только если в ней есть буквы.
        // Иначе это первая запись справочника и она не должна теряться.
        // Буквы (в т.ч. кириллица) — признак строки заголовка, а не данных.
        static const QRegularExpression lettersRe("[^0-9.;,\\s-]");
        if (firstLine.contains(lettersRe)) {
            headerParsed = true;
            for (int i = 0; i < headers.size(); ++i) {
                const QString raw = headers[i].trimmed().toLower();
                // Порядок проверок важен: сначала "масса/вес", затем "объем".
                // Колонку длины ищем только по слову "длина" — иначе заголовок
                // "Масса всего, т"/"Объем всего, м3" мог перекрывать нужные колонки.
                if (raw.contains("шифр") || raw.contains("наименование")) colCode = i;
                else if (raw.contains("длина")) colLength = i;
                else if (raw.contains("наруж")) colOuterD = i;
                else if (raw.contains("внутр")) colInnerD = i;
                else if (raw.contains("масса") || raw.contains("вес")) {
                    if (!raw.contains("всего")) colMass = i;
                }
                else if (raw.contains("объем") || raw.contains("объём")) {
                    if (!raw.contains("всего")) colVol = i;
                }
            }
        }
    }

    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.trimmed().isEmpty()) continue;
        QStringList fields = line.split(";");

        auto fieldAt = [&](int col) -> QString {
            if (col >= 0 && col < fields.size()) return fields[col].trimmed();
            return QString();
        };

        const QString code = fieldAt(colCode);
        if (code.isEmpty()) continue;

        KNBKData data;
        data.length = fieldAt(colLength);
        data.outerD = fieldAt(colOuterD);
        data.innerD = fieldAt(colInnerD);

        double mass = fieldAt(colMass).replace(",", ".").toDouble();
        double vol  = fieldAt(colVol).replace(",", ".").toDouble();

        // Эвристика для файлов без заголовка, сохраненных СТАРЫМ редактором
        // (порядок колонок "...;Объем 1 м, л;Вес 1 м, кг"): если значение в колонке
        // объема физически невозможно для массы 1 м (< 10 кг/м), а в колонке массы
        // стоит число, слишком большое для объема полости (> 500 л/м) — колонки перепутаны.
        if (!headerParsed && fields.size() >= 6) {
            if (mass < 10.0 && vol > 500.0) std::swap(mass, vol);
        }

        double od_mm = fieldAt(colOuterD).replace(",", ".").toDouble();
        double id_mm = fieldAt(colInnerD).replace(",", ".").toDouble();
        const bool geomOk = (od_mm > 0 && id_mm > 0 && id_mm < od_mm);
        double expected_vol_l = geomOk
                ? M_PI / 4.0 * (std::pow(od_mm / 1000.0, 2) - std::pow(id_mm / 1000.0, 2)) * 1000.0
                : 0.0;
        double expected_mass = expected_vol_l * STEEL_DENSITY_KG_M3_REF / 1000.0;

        // "Объем 1 м, л" должен быть ОБЪЕМОМ МЕТАЛЛА погонного метра полового
        // инструмента: V = pi/4*(OD^2 - ID^2). Старые файлы могли содержать здесь
        // объем внутренней полости (pi/4*ID^2) или мусор из-за перестановки колонок.
        // ВАЖНО: масса 1 м теперь можно ввести вручную в справочнике (паспортное
        // значение). Ручной ввод распознаётся по отклонению массы от геометрического
        // расчёта более чем на 2% — такие значения НЕ перетираются.
        const QString mode = fieldAt(6);
        const bool manualMode = mode.contains(QString::fromUtf8("ручн"), Qt::CaseInsensitive);
        // Ручной ввод распознаётся НЕЗАВИСИМО для массы и объёма: пользователь мог
        // ввести вручную только вес погонного метра или только его объём.
        const bool massIsManual = (mass > 0) && (!geomOk || !massMatchesGeometry(mass, od_mm, id_mm));
        // Объем считается «своим» (не перетирать), если он задан и согласован либо
        // с геометрией, либо с ручной массой (масса/объем = плотность в разумных
        // пределах). Иначе — это мусор из старой версии файла: пересчитываем.
        const bool volConsistentWithMass =
            (mass > 0 && vol > 0 &&
             std::fabs(vol * effectiveSteelDensity(mass, vol) / 1000.0 - mass) <= 0.02 * mass);
        const bool volIsManual = (vol > 0) && geomOk && !volMatchesGeometry(vol, od_mm, id_mm)
                                 && !(massIsManual && volConsistentWithMass);

        if (massIsManual) {
            // Масса 1 м введена вручную в справочнике (паспортное значение).
            // Она УЧАСТВУЕТ в расчете итогового веса инструмента как есть.
            // Объём металла 1 м: если он тоже введён вручную (и не согласован с
            // массой) или геометрия недоступна — оставляем как есть; иначе выводим
            // из паспортной массы V = m/rho_ст, чтобы архимедова сила
            // соответствовала паспортному весу.
            if (!geomOk) {
                if (vol <= 0) vol = mass / STEEL_DENSITY_KG_M3_REF * 1000.0;
            } else if (vol <= 0 || (!volIsManual && !volConsistentWithMass)) {
                vol = mass / STEEL_DENSITY_KG_M3_REF * 1000.0; // л на 1 м
            }
        } else if (volIsManual && geomOk) {
            // Вручную задан только объём металла погонного метра — масса
            // восстанавливается из него (сталь 7850), сам объём не трогаем.
            if (mass <= 0) mass = vol * STEEL_DENSITY_KG_M3_REF / 1000.0;
        } else if (geomOk && !manualMode) {
            // Обычный случай: масса и объём соответствуют геометрии (сталь 7850).
            // Лечим старые файлы с перепутанными/неверными колонками.
            if (expected_vol_l > 0 && (vol <= 0 || std::fabs(vol - expected_vol_l) > 0.005 * expected_vol_l))
                vol = expected_vol_l;
            if (mass <= 0 || std::fabs(mass - expected_mass) > 0.02 * expected_mass)
                mass = expected_mass;
        }

        data.mass1m = QString::number(mass, 'f', 2);
        data.vol1m  = QString::number(vol, 'f', 3);
        m_knbkReference[code] = data;
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
    double density = getInfoVal("Плотность, г/см3:"); // г/см3 == т/м3 == кг/л

    if (density <= 0) density = 1.0;
    if (cavern_coeff <= 0) cavern_coeff = 1.0;

    if (q_pump <= 0) {
        QMessageBox::warning(this, "Ошибка", "Производительность насоса должна быть больше 0");
        return;
    }
    if (depth <= 0) {
        QMessageBox::warning(this, "Ошибка", "Глубина спуска должна быть больше 0");
        return;
    }

    double total_weight_air_t = 0.0;   // вес КНБК в воздухе, т
    double total_volume_tool_m3 = 0.0; // вытесненный объем инструмента (объем металла), м3 — архимедова сила
    double total_capacity_m3 = 0.0;    // внутренний (емкостной) объем КНБК, м3 — время прокачки вниз
    double knbk_length_m = 0.0;        // суммарная длина КНБК, м

    struct KnbkSegment { double top; double bottom; double vMetalPerM; };
    QList<KnbkSegment> segments;
    double buoy_loss_t = 0.0; // сумма rho_р * V_металла по строкам, т (с поправкой на собств. плотность)

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
        double outer_d = getVal(2) / 1000.0; // м
        double inner_d = getVal(3) / 1000.0; // м

        // Вес погонного метра берем из таблицы (туда он попадает из справочника при выборе;
        // если в справочнике вес 1 м введен вручную — используется паспортное значение)
        double mass_1m = getVal(4);

        // Объем металла 1 м из справочника/таблицы (колонка 5). Если задан —
        // участвует в расчете вытесненного объема и архимедовой силы как есть.
        double vol_1m_l = getVal(5); // л на 1 м

        // Защита от физически невозможных данных (ID >= OD)
        if (length > 0 && outer_d > 0 && inner_d >= outer_d) {
            QMessageBox::warning(this, "Ошибка",
                QString("Строка %1 КНБК: внутренний d не может быть больше или равен наружному d").arg(row + 1));
            return;
        }

        // Объем внутренней полости погонного метра (через внутренний диаметр):
        // раствор входит в верхнее отверстие и выходит из нижнего, полость заполнена,
        // поэтому на архимедову силу она не влияет, но нужна для времени прокачки.
        double capacity_1m_l = M_PI * inner_d * inner_d / 4.0 * 1000.0; // л/м

        // Объем вытесненной жидкости погонным метром = наружный объем минус полость,
        // т.е. геометрический объем металла стенки: pi/4 * (OD^2 - ID^2)
        double disp_1m_m3 = M_PI * (outer_d * outer_d - inner_d * inner_d) / 4.0;

        // Приоритет значения колонки 5 «Объем 1 м, л» зависит от режима строки:
        QComboBox *modeCombo = qobject_cast<QComboBox*>(m_knbkTable->cellWidget(row, 6));
        const bool isManualRow = (modeCombo && modeCombo->currentText() == "Вручную");
        if (isManualRow) {
            // Режим "Вручную": пользователь сам задал массу и объем — используем их как есть.
            if (vol_1m_l > 0)
                disp_1m_m3 = vol_1m_l / 1000.0; // л -> м3
        } else {
            // Режим "Справочник": если справочник содержит ОБЪЕМ ВНУТРЕННЕЙ ПОЛОСТИ
            // (pi/4*ID^2 — старые файлы), а не объем металла стенок (pi/4*(OD^2-ID^2)),
            // такое значение физически невозможно как объем металла (его собственная
            // плотность оказалась бы больше плотности стали) — пересчитываем из геометрии.
            const double rho_check = effectiveSteelDensity(mass_1m, vol_1m_l);
            if (vol_1m_l > 0 && !(rho_check > STEEL_DENSITY_KG_M3_REF * 1.02))
                disp_1m_m3 = vol_1m_l / 1000.0; // л -> м3
        }

        // Если массы 1 м нет — восстанавливаем из объема металла (сталь 7850)
        if (mass_1m <= 0 && disp_1m_m3 > 0)
            mass_1m = disp_1m_m3 * STEEL_DENSITY_KG_M3_REF;

        // Собственная плотность металла строки (паспорт масса/объем) — для
        // корректной архимедовой силы при ручном весе (УБТ и т.п.)
        const double rho_eff = effectiveSteelDensity(mass_1m, disp_1m_m3 * 1000.0);

        // Вес в воздухе = Длина * Масса 1 м / 1000 (кг -> т)
        double weight_air = length * mass_1m / 1000.0;
        total_weight_air_t += weight_air;

        // Вытесненный объем = Длина * Объем металла 1 м (из справочника/таблицы,
        // включая ручной ввод; если не задан — из геометрии pi/4*(OD^2-ID^2))
        double v_disp = disp_1m_m3 * length;
        total_volume_tool_m3 += v_disp;

        // Потеря веса в растворе по этой строке: rho_р * V_металла. Для строк с
        // паспортным (ручным) весом используем собственную плотность металла —
        // см. weight_in_mud_t ниже.
        buoy_loss_t += density * v_disp * (STEEL_DENSITY_KG_M3_REF / rho_eff);

        // Емкостной объем = Длина * объем полости 1 м
        double v_inner = (capacity_1m_l / 1000.0) * length;
        total_capacity_m3 += v_inner;

        knbk_length_m += length;

        m_knbkTable->blockSignals(true);
        if (m_knbkTable->item(row, 7)) m_knbkTable->item(row, 7)->setText(QString::number(weight_air, 'f', 3));
        if (m_knbkTable->item(row, 8)) m_knbkTable->item(row, 8)->setText(QString::number(v_disp, 'f', 4));
        m_knbkTable->blockSignals(false);

        if (length > 0) {
            KnbkSegment seg;
            seg.top = 0;
            seg.bottom = length;
            seg.vMetalPerM = disp_1m_m3;
            segments.append(seg);
        }
    }

    // Распределяем металл КНБК по глубине: считаем, что КНБК занимает НИЖНЮЮ часть
    // сборки, т.е. интервал [depth - L_кнбк, depth] конструкции скважины.
    std::sort(segments.begin(), segments.end(), [](const KnbkSegment &a, const KnbkSegment &b) {
        return a.top < b.top;
    });
    double offset = depth - knbk_length_m;
    for (auto &s : segments) {
        s.top += offset;
        s.bottom += offset;
    }

    auto metalVolumeInInterval = [&](double t, double b) -> double {
        double v = 0.0;
        for (const auto &s : segments) {
            double lo = std::max(t, s.top);
            double hi = std::min(b, s.bottom);
            if (hi > lo) v += s.vMetalPerM * (hi - lo);
        }
        return v;
    };

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

        if (outerD_mm > 0 && thickness_mm > 0 && bottom > top) {
            Casing c;
            c.top = top;
            c.bottom = bottom;
            c.innerD = (outerD_mm - 2.0 * thickness_mm) / 1000.0;
            if (c.innerD > 0) casings.append(c);
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
            // Металл КНБК, находящийся именно в этом интервале глубин
            double v_metal_interval = metalVolumeInInterval(top, bottom);
            v_annulus += (v_interval_well - v_metal_interval);
            current_depth = bottom;
        }
    }

    if (current_depth < depth && d_bit_m > 0) {
        double length = depth - current_depth;
        double v_open_well = M_PI * std::pow(d_bit_m, 2) / 4.0 * length * cavern_coeff;
        v_well += v_open_well;
        double v_metal_open = metalVolumeInInterval(current_depth, depth);
        v_annulus += (v_open_well - v_metal_open);
    }

    if (v_annulus < 0) v_annulus = 0;

    // Архимедова сила для ПОЛОГО инструмента с входным и выходным отверстиями.
    // Раствор свободно проходит через внутреннюю полость (вход сверху, выход снизу),
    // поэтому столб жидкости внутри полости не создает дополнительной осевой силы
    // и в вытеснении участвует ТОЛЬКО объем металла стенок: V = pi/4*(OD^2-ID^2)*L
    // (или объем металла 1 м из справочника, если он задан/введен вручную).
    // Плотность раствора: г/см3 == т/м3 == кг/л, поэтому пересчет множителя не нужен.
    // buoy_loss_t — сумма по строкам rho_р * V_металла с поправкой на собственную
    // плотность металла паспортных (ручных) позиций: W_раствор = W_воздух - buoy_loss_t
    double weight_in_mud_t = total_weight_air_t - buoy_loss_t;

    // Контроль корректности исходных данных
    if (density > 2.6) {
        QMessageBox::warning(this, "Проверка данных",
            QString("Плотность раствора %1 г/см3 выглядит ошибочной (проверьте единицы: г/см3, а не кг/м3)").arg(density));
    }
    if (total_weight_air_t > 0 && weight_in_mud_t < 0) {
        QMessageBox::warning(this, "Проверка данных",
            "Вес в растворе получился отрицательным — проверьте массу 1 м и диаметры КНБК.");
    }

    double q_pump_m3_min = q_pump * 60.0 / 1000.0; // л/с -> м3/мин
    double time_gas_min = v_annulus / q_pump_m3_min;
    double time_down_min = total_capacity_m3 / q_pump_m3_min;
    double full_cycle_time_min = time_down_min + time_gas_min;

    auto setRes = [&](const QString &key, double val) {
        if (m_resultOutputs.contains(key)) {
            m_resultOutputs[key]->setText(QString::number(val, 'f', 2));
        }
    };

    setRes("Вес инструмента в растворе с таль блока, т:", weight_in_mud_t + weight_travel_block);
    setRes("Вес инструмента в р-ре с квадратом, т:", weight_in_mud_t + weight_square);
    setRes("Вес инструмента в воздухе, т:", total_weight_air_t);
    setRes("Объем инструмента, куб.м:", total_volume_tool_m3);
    setRes("Объем скважины до гл. спуска, куб.м:", v_well);
    setRes("Объем трубного пространства, куб.м:", total_capacity_m3);
    setRes("Объем затрубного пространства, куб.м:", v_annulus);
    setRes("Время выхода газа, мин:", time_gas_min);
    setRes("Полное время цикла, мин:", full_cycle_time_min);

    statusBar()->showMessage("Расчёт выполнен успешно", 3000);
}
