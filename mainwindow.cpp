#include "mainwindow.h"
#include "knbkeditordialog.h"
#include "tankvolumetab.h"
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
#include <QFileInfo>
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
#include <QTextCharFormat>
#include <QMenu>
#include <QCheckBox>
#include <cmath>
#include <algorithm>

static const int ToBottomRole = Qt::UserRole + 2;
static const int SingleToBottomRole = Qt::UserRole + 3;

const QStringList KNBK_CSV_HEADER = {
    "Шифр", "Длина, м", "Наружный d, мм", "Внутренний d, мм",
    "Масса 1 м, кг", "Объем 1 м, л", "Режим", "Масса всего, т", "Объем всего, м3"
};

static const double STEEL_DENSITY_KG_M3_REF = 7850.0;

static bool massMatchesGeometry(double mass_kg_m, double od_mm, double id_mm)
{
    if (!(od_mm > 0 && id_mm > 0 && id_mm < od_mm)) return false;
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
        QMessageBox::about(this, "О программе", "Расчеты ГТИ.\nhttps://github.com/geolex-ob/https-github.com-GeoFluxANDMAss\nВерсия 0.1.5");
    });

    statusBar()->showMessage("Готово");

    setupUI();
    loadReverseCirculation();   // восстановить режим промывки (QSettings)
    loadInfoCsv();              // затем — перекрыть значениями из data/info.csv (если есть)

    loadKNBKReference();
    loadCSV(getDataPath("well_construction.csv"), m_constructionTable);
    if (!loadKNBKLayout()) {
        if (!loadKNBKLayoutFromCsv(getDataPath("knbk_layout.csv")))
            loadKNBKLayoutFromCsv(getDataPath("knbk.csv"));
    }
}

MainWindow::~MainWindow() {}

// ================= DATA/INFO.CSV — РУЧНЫЕ ДАННЫЕ РАЗДЕЛА «ИНФОРМАЦИЯ» =================

QString MainWindow::infoCsvPath() const
{
    // Тот же каталог data, что и для knbk.csv / well_construction.csv
    return const_cast<MainWindow*>(this)->getDataPath("info.csv");
}

void MainWindow::saveInfoCsv()
{
    QFile file(infoCsvPath());
    QDir().mkpath(QFileInfo(file).absolutePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    const QStringList keys = {
        "Производительность, л/сек:",
        "Вес тальблока, т:",
        "Вес квадрата, т:",
        "Коэфф. кавернозности:",
        "Глубина спуска, м:",
        "Диаметр долота, мм:",
        "Плотность, г/см3:"
    };
    for (const QString &k : keys) {
        if (m_infoInputs.contains(k)) {
            QString v = m_infoInputs[k]->text();
            v.replace("\n", " ").replace(";", ",");
            out << k << ";" << v << "\n";
        }
    }
    out << "Обратная промывка;" << (isReverseCirculation() ? "1" : "0") << "\n";
    file.close();
}

void MainWindow::loadInfoCsv()
{
    QFile file(infoCsvPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.trimmed().isEmpty()) continue;
        const int sep = line.indexOf(';');
        if (sep < 0) continue;
        const QString key = line.left(sep).trimmed();
        const QString val = line.mid(sep + 1);

        if (key == QStringLiteral("Обратная промывка")) {
            if (m_reverseCirculationCheck) {
                m_reverseCirculationCheck->blockSignals(true);
                m_reverseCirculationCheck->setChecked(val.trimmed() == "1");
                m_reverseCirculationCheck->blockSignals(false);
            }
        } else if (m_infoInputs.contains(key)) {
            m_infoInputs[key]->setText(val);
        }
    }
    file.close();
}

// ================= ОБРАТНАЯ ПРОМЫВКА =================

bool MainWindow::isReverseCirculation() const
{
    return m_reverseCirculationCheck && m_reverseCirculationCheck->isChecked();
}

void MainWindow::saveReverseCirculation()
{
    QSettings settings;
    settings.setValue("Info/reverseCirculation", isReverseCirculation());
}

void MainWindow::loadReverseCirculation()
{
    QSettings settings;
    const bool rev = settings.value("Info/reverseCirculation", false).toBool();
    if (m_reverseCirculationCheck) {
        m_reverseCirculationCheck->blockSignals(true);
        m_reverseCirculationCheck->setChecked(rev);
        m_reverseCirculationCheck->blockSignals(false);
    }
}

// ================= АВТОСОХРАНЕНИЕ КОМПОНОВКИ КНБК =================

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveInfoCsv();       // последние введённые данные раздела «Информация»
    saveKNBKLayout();    // компоновка КНБК (QSettings, как было)
    QMainWindow::closeEvent(event);
}

QString MainWindow::knbkLayoutSettingsKey() const
{
    return QStringLiteral("KNBKLayout/rows");
}

void MainWindow::saveKNBKLayout()
{
    if (m_loadingLayout) return;
    QSettings settings;
    settings.beginWriteArray(knbkLayoutSettingsKey());
    for (int row = 0; row < m_knbkTable->rowCount(); ++row) {
        settings.setArrayIndex(row);

        QString code;
        QComboBox *codeCombo = nullptr;
        if (QWidget *cellW = m_knbkTable->cellWidget(row, 0))
            codeCombo = cellW->findChild<QComboBox*>();
        if (codeCombo) {
            code = codeCombo->currentText();
        } else {
            QTableWidgetItem *cell0 = m_knbkTable->item(row, 0);
            if (cell0) {
                code = cell0->text();
                if (code.startsWith(QStringLiteral("\u2714"))) code = code.mid(1);
                else if (code.startsWith(QStringLiteral("\u2610"))) code = code.mid(1);
                else if (code.startsWith(QStringLiteral("\u25C9"))) code = code.mid(1);
                code = code.trimmed();
            }
        }
        {
            QTableWidgetItem *codeCell = m_knbkTable->item(row, 0);
            const bool tb = codeCell && codeCell->data(ToBottomRole).toBool();
            const bool sb = codeCell && codeCell->data(SingleToBottomRole).toBool();
            if (sb) code += QStringLiteral(" \u25C9");
            else if (tb) code += QStringLiteral(" \u2714");
        }
        settings.setValue("code", code);

        QComboBox *modeCombo = qobject_cast<QComboBox*>(m_knbkTable->cellWidget(row, 6));
        settings.setValue("mode", modeCombo ? modeCombo->currentText() : QString("Справочник"));

        for (int col = 1; col <= 5; ++col) {
            QTableWidgetItem *item = m_knbkTable->item(row, col);
            settings.setValue(QString("c%1").arg(col), item ? item->text() : QString());
        }
        for (int col = 7; col < m_knbkTable->columnCount(); ++col) {
            QTableWidgetItem *item = m_knbkTable->item(row, col);
            settings.setValue(QString("c%1").arg(col), item ? item->text() : QString());
        }
    }
    settings.endArray();

    settings.beginGroup("KNBKTableGeometry");
    for (int col = 0; col < m_knbkTable->columnCount(); ++col)
        settings.setValue(QString("w%1").arg(col), m_knbkTable->columnWidth(col));
    settings.endGroup();

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
        return false;
    }

    m_knbkTable->setRowCount(0);
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        const QString mode = settings.value("mode", "Справочник").toString();

        QString code = settings.value("code").toString();
        bool toBottom = false;
        bool singleToBottom = false;
        while (code.endsWith(QChar(u'✔')) || code.endsWith(QChar(u'◉')) || code.endsWith(QLatin1Char(' '))) {
            if (code.endsWith(QChar(u'◉'))) { singleToBottom = true; toBottom = true; }
            else if (code.endsWith(QChar(u'✔'))) { toBottom = true; }
            code.chop(1);
        }

        QStringList fields(9);
        fields[0] = code;
        for (int col = 1; col <= 5; ++col) fields[col] = settings.value(QString("c%1").arg(col)).toString();
        fields[6] = mode;
        for (int col = 7; col < 9; ++col) fields[col] = settings.value(QString("c%1").arg(col)).toString();

        addKNBKRow();
        int row = m_knbkTable->rowCount() - 1;
        fillKNBKRow(row, fields);
        if (singleToBottom && m_knbkTable->item(row, 0)) {
            setSingleToBottom(row);
        } else if (toBottom && m_knbkTable->item(row, 0)) {
            m_knbkTable->item(row, 0)->setData(ToBottomRole, true);
            updateToBottomCheckBox(row);
        }
    }
    settings.endArray();
    updateToBottomLengths();
    updateAllToBottomCheckBoxes();
    return true;
}

void MainWindow::fillKNBKRow(int row, const QStringList &fields)
{
    if (row < 0 || row >= m_knbkTable->rowCount() || fields.size() < 7) return;

    QComboBox *combo = nullptr;
    if (QWidget *cellW = m_knbkTable->cellWidget(row, 0))
        combo = cellW->findChild<QComboBox*>();
    if (combo) {
        combo->blockSignals(true);
        int idx = combo->findText(fields[0]);
        if (idx >= 0) combo->setCurrentIndex(idx);
        else if (!fields[0].isEmpty()) combo->setEditText(fields[0]);
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
    if (!combo && !fields[0].isEmpty()) {
        QTableWidgetItem *it0 = m_knbkTable->item(row, 0);
        if (!it0) { it0 = new QTableWidgetItem(); m_knbkTable->setItem(row, 0, it0); }
        it0->setText(fields[0]);
    }
    m_knbkTable->blockSignals(false);

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
        if (allLines.first().contains(lettersRe)) startIdx = 1;
    }

    bool any = false;
    m_knbkTable->setRowCount(0);
    for (int li = startIdx; li < allLines.size(); ++li) {
        const QString line = allLines[li];
        if (line.trimmed().isEmpty()) continue;
        QStringList fields = line.split(";");
        if (fields.size() < 7) continue;

        double vMass = fields[4].replace(",", ".").toDouble();
        double vVol  = fields[5].replace(",", ".").toDouble();
        if (vMass < 10.0 && vVol > 500.0) std::swap(fields[4], fields[5]);

        QString code = fields[0];
        bool toBottom = false;
        bool singleToBottom = false;
        while (code.endsWith(QChar(u'✔')) || code.endsWith(QChar(u'◉')) || code.endsWith(QLatin1Char(' '))) {
            if (code.endsWith(QChar(u'◉'))) { singleToBottom = true; toBottom = true; }
            else if (code.endsWith(QChar(u'✔'))) { toBottom = true; }
            code.chop(1);
        }
        fields[0] = code;

        addKNBKRow();
        int row = m_knbkTable->rowCount() - 1;
        fillKNBKRow(row, fields);
        if (singleToBottom && m_knbkTable->item(row, 0)) {
            setSingleToBottom(row);
        } else if (toBottom && m_knbkTable->item(row, 0)) {
            m_knbkTable->item(row, 0)->setData(ToBottomRole, true);
            updateToBottomCheckBox(row);
        }
        any = true;
    }
    updateToBottomLengths();
    updateAllToBottomCheckBoxes();
    return any;
}

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
    // Вкладки: [0] — КНБК/веса (существующая логика), [1] — Объём ёмкости
    m_tabs = new QTabWidget(this);
    setCentralWidget(m_tabs);

    auto *central = new QWidget();
    m_tabs->addTab(central, "КНБК / веса");

    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    auto *topLayout = new QHBoxLayout();

    auto *infoGroup = new QGroupBox("Информация", central);
    auto *infoLayout = new QFormLayout(infoGroup);
    infoLayout->setSpacing(10);

    auto addInfoField = [&](const QString &label, const QString &defaultValue = "") {
        auto *le = new QLineEdit(defaultValue);
        m_infoInputs[label] = le;
        infoLayout->addRow(label, le);
        return le;
    };

    addInfoField("Производительность, л/сек:", "1.0");

    // Галочка «Обратная промывка» — под полем производительности насоса.
    m_reverseCirculationCheck = new QCheckBox("Обратная промывка", infoGroup);
    m_reverseCirculationCheck->setToolTip(
        "При обратной промывке раствор идёт вниз по затрубному пространству,\n"
        "а вверх — по трубам. Время выхода газа считается по объёму труб.");
    infoLayout->addRow(QString(), m_reverseCirculationCheck);
    connect(m_reverseCirculationCheck, &QCheckBox::toggled,
            this, [this](bool){ saveReverseCirculation(); saveInfoCsv(); });

    addInfoField("Вес тальблока, т:", "0.0");
    addInfoField("Вес квадрата, т:", "0.0");
    addInfoField("Коэфф. кавернозности:", "1.0");
    connect(addInfoField("Глубина спуска, м:", "0.0"), &QLineEdit::textEdited,
            this, [this]() { updateToBottomLengths(); });
    addInfoField("Диаметр долота, мм:", "215.9");
    addInfoField("Плотность, г/см3:", "1.0");

    for (auto it = m_infoInputs.begin(); it != m_infoInputs.end(); ++it) {
        connect(it.value(), &QLineEdit::editingFinished, this, &MainWindow::saveInfoCsv);
    }

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

    auto *bottomLayout = new QHBoxLayout();

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
    m_knbkTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    {
        QSettings colWidths;
        colWidths.beginGroup("KNBKTableGeometry");
        for (int col = 0; col < m_knbkTable->columnCount(); ++col) {
            if (colWidths.contains(QString("w%1").arg(col)))
                m_knbkTable->setColumnWidth(col, colWidths.value(QString("w%1").arg(col)).toInt());
        }
        colWidths.endGroup();
    }

    connect(m_knbkTable, &QTableWidget::cellChanged, this, [this](int row, int col) {
        if (m_loadingLayout || col != 1) return;
        updateToBottomLengths();
    });

    m_knbkTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_knbkTable, &QTableWidget::customContextMenuRequested,
            this, &MainWindow::showKNBKContextMenu);

    knbkLayout->addLayout(knbkBtnLayout);
    knbkLayout->addWidget(m_knbkTable);

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

    // ---- Вторая вкладка: объём горизонтального цилиндра ----
    m_tankTab = new TankVolumeTab(this);
    m_tabs->addTab(m_tankTab, "Объём ёмкости");
}

void MainWindow::setupKNBKCombo(int row)
{
    QWidget *cellW = new QWidget(m_knbkTable);
    auto *h = new QHBoxLayout(cellW);
    h->setContentsMargins(2, 0, 2, 0);
    h->setSpacing(3);

    auto *check = new QCheckBox(cellW);
    check->setToolTip("До забоя: длина = глубина спуска − сумма длин выше");
    check->setProperty("row", row);
    connect(check, &QCheckBox::toggled, this, &MainWindow::onKNBKToBottomToggled);

    QComboBox *combo = new QComboBox(cellW);
    combo->setEditable(true);
    combo->lineEdit()->setReadOnly(true);
    combo->addItem("");
    combo->addItems(m_knbkReference.keys());
    combo->setProperty("row", row);
    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onKNBKComboChanged);

    h->addWidget(check);
    h->addWidget(combo, 1);

    m_knbkTable->setCellWidget(row, 0, cellW);

    if (!m_knbkTable->item(row, 0))
        m_knbkTable->setItem(row, 0, new QTableWidgetItem(""));

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

void MainWindow::onKNBKToBottomToggled(bool checked)
{
    QCheckBox *check = qobject_cast<QCheckBox*>(sender());
    if (!check || m_loadingLayout) return;
    const int row = check->property("row").toInt();
    QTableWidgetItem *cell = m_knbkTable->item(row, 0);
    if (!cell) return;

    if (cell->data(SingleToBottomRole).toBool()) {
        cell->setData(SingleToBottomRole, false);
    }
    cell->setData(ToBottomRole, checked);

    updateToBottomLengths();
    statusBar()->showMessage(checked
        ? "Включен расчет длины до глубины спуска (до забоя)"
        : "Отключен расчет длины до глубины спуска", 3000);
}

void MainWindow::updateToBottomCheckBox(int row)
{
    if (row < 0 || row >= m_knbkTable->rowCount()) return;
    QWidget *cellW = m_knbkTable->cellWidget(row, 0);
    if (!cellW) return;
    QCheckBox *check = cellW->findChild<QCheckBox*>();
    if (!check) return;

    QTableWidgetItem *cell = m_knbkTable->item(row, 0);
    const bool tb = cell && cell->data(ToBottomRole).toBool();
    const bool sb = cell && cell->data(SingleToBottomRole).toBool();

    check->blockSignals(true);
    check->setChecked(tb || sb);
    check->blockSignals(false);

    if (sb) {
        check->setTristate(true);
        check->setCheckState(Qt::PartiallyChecked);
        check->setToolTip("Одиночный выбор: только этот элемент рассчитывается до забоя");
    } else {
        check->setTristate(false);
        check->setCheckState(tb ? Qt::Checked : Qt::Unchecked);
        check->setToolTip("До забоя: длина = глубина спуска − сумма длин выше");
    }
}

void MainWindow::updateAllToBottomCheckBoxes()
{
    for (int r = 0; r < m_knbkTable->rowCount(); ++r)
        updateToBottomCheckBox(r);
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

    updateToBottomLengths();
}

void MainWindow::onKNBKModeChanged(int index)
{
    Q_UNUSED(index);
    QComboBox *modeCombo = qobject_cast<QComboBox*>(sender());
    if (!modeCombo) return;

    int row = modeCombo->property("row").toInt();
    bool isManual = (modeCombo->currentText() == "Вручную");

    if (!isManual) {
        QComboBox *codeCombo = nullptr;
        if (QWidget *cellW = m_knbkTable->cellWidget(row, 0))
            codeCombo = cellW->findChild<QComboBox*>();
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
    for (int i = 0; i < m_constructionTable->columnCount(); ++i)
        m_constructionTable->setItem(row, i, new QTableWidgetItem(""));
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
    updateToBottomCheckBox(row);
    updateToBottomLengths();
}

void MainWindow::removeKNBKRow()
{
    int row = m_knbkTable->currentRow();
    if (row >= 0) {
        m_knbkTable->removeRow(row);
        updateToBottomLengths();
    }
}

// ================= ГАЛОЧКА "ДО ЗАБОЯ" =================

void MainWindow::showKNBKContextMenu(const QPoint &pos)
{
    QTableWidgetItem *item = m_knbkTable->itemAt(pos);
    if (!item || item->column() != 0) return;
    const int row = item->row();

    QMenu menu(this);
    QAction *singleAct = menu.addAction("Рассчитать длину до забоя (только этот элемент)");
    QAction *clearAct  = menu.addAction("Снять расчёт до забоя");
    menu.addSeparator();
    QAction *toggleAct = menu.addAction(
        item->data(ToBottomRole).toBool()
            ? "Снять обычную галочку «До забоя»"
            : "Включить обычную галочку «До забоя»");

    QAction *chosen = menu.exec(m_knbkTable->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == singleAct) {
        setSingleToBottom(row);
    } else if (chosen == clearAct) {
        for (int r = 0; r < m_knbkTable->rowCount(); ++r) {
            if (QTableWidgetItem *it = m_knbkTable->item(r, 0))
                it->setData(SingleToBottomRole, false);
        }
        updateAllToBottomCheckBoxes();
        updateToBottomLengths();
        statusBar()->showMessage("Расчёт до забоя снят", 3000);
    } else if (chosen == toggleAct) {
        toggleToBottom(item);
    }
}

void MainWindow::setSingleToBottom(int row)
{
    if (m_loadingLayout) return;

    for (int r = 0; r < m_knbkTable->rowCount(); ++r) {
        if (QTableWidgetItem *it = m_knbkTable->item(r, 0))
            it->setData(SingleToBottomRole, false);
    }
    QTableWidgetItem *cell = m_knbkTable->item(row, 0);
    if (!cell) return;
    cell->setData(SingleToBottomRole, true);
    cell->setData(ToBottomRole, true);

    updateAllToBottomCheckBoxes();
    updateToBottomLengths();
    statusBar()->showMessage(
        QString("Элемент «%1» выбран для расчёта длины до забоя").arg(cell->text()), 3000);
}

void MainWindow::toggleToBottom(QTableWidgetItem *cell)
{
    if (!cell || cell->column() != 0 || m_loadingLayout) return;

    if (cell->data(SingleToBottomRole).toBool()) {
        cell->setData(SingleToBottomRole, false);
        updateToBottomCheckBox(cell->row());
        updateToBottomLengths();
        statusBar()->showMessage("Одиночный расчёт до забоя снят", 3000);
        return;
    }

    const bool newState = !cell->data(ToBottomRole).toBool();
    cell->setData(ToBottomRole, newState);
    updateToBottomCheckBox(cell->row());

    updateToBottomLengths();
    statusBar()->showMessage(newState
        ? "Включен расчет длины до глубины спуска (до забоя)"
        : "Отключен расчет длины до глубины спуска", 3000);
}

void MainWindow::updateToBottomLengths()
{
    if (m_loadingLayout || !m_knbkTable) return;

    double depth = 0.0;
    if (m_infoInputs.contains("Глубина спуска, м:"))
        depth = m_infoInputs["Глубина спуска, м:"]->text().replace(",", ".").toDouble();

    m_knbkTable->blockSignals(true);
    double cumLen = 0.0;
    for (int row = 0; row < m_knbkTable->rowCount(); ++row) {
        QTableWidgetItem *codeCell = m_knbkTable->item(row, 0);
        QTableWidgetItem *lenItem  = m_knbkTable->item(row, 1);
        const bool toBottom = codeCell && codeCell->data(ToBottomRole).toBool();
        const bool single   = codeCell && codeCell->data(SingleToBottomRole).toBool();
        const bool calc = single || toBottom;
        if (calc && lenItem) {
            const double newLen = depth - cumLen;
            lenItem->setText(QString::number(newLen > 0 ? newLen : 0.0, 'f', 2));
            lenItem->setFlags((lenItem->flags() & ~Qt::ItemIsEditable)
                              | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            lenItem->setBackground(QBrush(Qt::lightGray));
        } else if (lenItem) {
            lenItem->setFlags(lenItem->flags() | Qt::ItemIsEditable);
            lenItem->setBackground(QBrush(Qt::white));
        }
        double l = 0.0;
        if (lenItem) {
            QString s = lenItem->text();
            s.replace(",", ".");
            l = s.toDouble();
        }
        cumLen += l;
    }
    m_knbkTable->blockSignals(false);

    saveKNBKLayout();
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
    for (int col = 0; col < table->columnCount(); ++col)
        headers << table->horizontalHeaderItem(col)->text();
    out << headers.join(";") << "\n";

    for (int row = 0; row < table->rowCount(); ++row) {
        QStringList rowData;
        for (int col = 0; col < table->columnCount(); ++col) {
            QString text;
            if (col == 0 && table == m_knbkTable) {
                QComboBox *combo = nullptr;
                if (QWidget *w = table->cellWidget(row, col))
                    combo = w->findChild<QComboBox*>();
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

    QStringList allLines;
    while (!in.atEnd()) allLines << in.readLine();
    file.close();

    table->setRowCount(0);

    int startIdx = 0;
    if (!allLines.isEmpty()) {
        static const QRegularExpression lettersRe("[^0-9.;,\\s-]");
        if (allLines.first().contains(lettersRe)) startIdx = 1;
    }

    for (int li = startIdx; li < allLines.size(); ++li) {
        const QString line = allLines[li];
        if (line.trimmed().isEmpty()) continue;
        QStringList fields = line.split(";");

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
                QComboBox *combo = nullptr;
                if (QWidget *w = table->cellWidget(row, 0))
                    combo = w->findChild<QComboBox*>();
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
                if (col != 6 && table->item(row, col))
                    table->item(row, col)->setText(fields[col]);
            }
        } else {
            for (int col = 0; col < table->columnCount() && col < fields.size(); ++col)
                table->setItem(row, col, new QTableWidgetItem(fields[col]));
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

    int colCode = 0, colLength = 1, colOuterD = 2, colInnerD = 3, colMass = 4, colVol = 5;
    bool headerParsed = false;

    if (!in.atEnd()) {
        const QString firstLine = in.readLine();
        const QStringList headers = firstLine.split(";");
        static const QRegularExpression lettersRe("[^0-9.;,\\s-]");
        if (firstLine.contains(lettersRe)) {
            headerParsed = true;
            for (int i = 0; i < headers.size(); ++i) {
                const QString raw = headers[i].trimmed().toLower();
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

        const QString mode = fieldAt(6);
        const bool manualMode = mode.contains(QString::fromUtf8("ручн"), Qt::CaseInsensitive);
        const bool massIsManual = (mass > 0) && (!geomOk || !massMatchesGeometry(mass, od_mm, id_mm));
        const bool volConsistentWithMass =
            (mass > 0 && vol > 0 &&
             std::fabs(vol * effectiveSteelDensity(mass, vol) / 1000.0 - mass) <= 0.02 * mass);
        const bool volIsManual = (vol > 0) && geomOk && !volMatchesGeometry(vol, od_mm, id_mm)
                                 && !(massIsManual && volConsistentWithMass);

        if (massIsManual) {
            if (!geomOk) {
                if (vol <= 0) vol = mass / STEEL_DENSITY_KG_M3_REF * 1000.0;
            } else if (vol <= 0 || (!volIsManual && !volConsistentWithMass)) {
                vol = mass / STEEL_DENSITY_KG_M3_REF * 1000.0;
            }
        } else if (volIsManual && geomOk) {
            if (mass <= 0) mass = vol * STEEL_DENSITY_KG_M3_REF / 1000.0;
        } else if (geomOk && !manualMode) {
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
        QComboBox *combo = nullptr;
        if (QWidget *w = m_knbkTable->cellWidget(row, 0))
            combo = w->findChild<QComboBox*>();
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
    updateToBottomLengths();

    auto getInfoVal = [&](const QString &key) -> double {
        if (!m_infoInputs.contains(key)) return 0.0;
        bool ok;
        double val = m_infoInputs[key]->text().replace(",", ".").toDouble(&ok);
        return ok ? val : 0.0;
    };

    double q_pump = getInfoVal("Производительность, л/сек:");
    double weight_travel_block = getInfoVal("Вес тальблока, т:");
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
    if (depth <= 0) {
        QMessageBox::warning(this, "Ошибка", "Глубина спуска должна быть больше 0");
        return;
    }

    double total_weight_air_t = 0.0;
    double total_volume_tool_m3 = 0.0;
    double total_capacity_m3 = 0.0;
    double knbk_length_m = 0.0;

    struct KnbkSegment { double top; double bottom; double vMetalPerM; };
    QList<KnbkSegment> segments;
    double buoy_loss_t = 0.0;

    for (int row = 0; row < m_knbkTable->rowCount(); ++row) {
        auto getVal = [&](int col) -> double {
            QTableWidgetItem *item = m_knbkTable->item(row, col);
            if (!item) return 0.0;
            bool ok;
            return item->text().replace(",", ".").toDouble(&ok);
        };

        double length = getVal(1);
        double outer_d = getVal(2) / 1000.0;
        double inner_d = getVal(3) / 1000.0;
        double mass_1m = getVal(4);
        double vol_1m_l = getVal(5);

        if (length > 0 && outer_d > 0 && inner_d >= outer_d) {
            QMessageBox::warning(this, "Ошибка",
                QString("Строка %1 КНБК: внутренний d не может быть больше или равен наружному d").arg(row + 1));
            return;
        }

        double capacity_1m_l = M_PI * inner_d * inner_d / 4.0 * 1000.0;
        double disp_1m_m3 = M_PI * (outer_d * outer_d - inner_d * inner_d) / 4.0;

        QComboBox *modeCombo = qobject_cast<QComboBox*>(m_knbkTable->cellWidget(row, 6));
        const bool isManualRow = (modeCombo && modeCombo->currentText() == "Вручную");
        if (isManualRow) {
            if (vol_1m_l > 0) disp_1m_m3 = vol_1m_l / 1000.0;
        } else {
            const double rho_check = effectiveSteelDensity(mass_1m, vol_1m_l);
            if (vol_1m_l > 0 && !(rho_check > STEEL_DENSITY_KG_M3_REF * 1.02))
                disp_1m_m3 = vol_1m_l / 1000.0;
        }

        if (mass_1m <= 0 && disp_1m_m3 > 0)
            mass_1m = disp_1m_m3 * STEEL_DENSITY_KG_M3_REF;

        const double rho_eff = effectiveSteelDensity(mass_1m, disp_1m_m3 * 1000.0);

        double weight_air = length * mass_1m / 1000.0;
        total_weight_air_t += weight_air;

        double v_disp = disp_1m_m3 * length;
        total_volume_tool_m3 += v_disp;

        buoy_loss_t += density * v_disp * (STEEL_DENSITY_KG_M3_REF / rho_eff);

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

    double weight_in_mud_t = total_weight_air_t - buoy_loss_t;

    if (density > 2.6) {
        QMessageBox::warning(this, "Проверка данных",
            QString("Плотность раствора %1 г/см3 выглядит ошибочной (проверьте единицы: г/см3, а не кг/м3)").arg(density));
    }
    if (total_weight_air_t > 0 && weight_in_mud_t < 0) {
        QMessageBox::warning(this, "Проверка данных",
            "Вес в растворе получился отрицательным — проверьте массу 1 м и диаметры КНБК.");
    }

    const bool reverse = isReverseCirculation();
    double q_pump_m3_min = q_pump * 60.0 / 1000.0;
    double time_gas_min = (reverse ? total_capacity_m3 : v_annulus) / q_pump_m3_min;
    double full_cycle_time_min = (total_capacity_m3 + v_annulus) / q_pump_m3_min;

    auto setRes = [&](const QString &key, double val) {
        if (m_resultOutputs.contains(key))
            m_resultOutputs[key]->setText(QString::number(val, 'f', 2));
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

    statusBar()->showMessage(reverse
        ? "Расчёт выполнен (обратная промывка)"
        : "Расчёт выполнен (прямая промывка)", 4000);
}
