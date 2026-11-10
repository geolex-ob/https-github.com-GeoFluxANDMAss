#include "ipttab.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QHeaderView>
#include <cmath>
#include <algorithm>

static const double STEEL_DENSITY = 7850.0;
static const double AIR_DENSITY   = 1.2;   // кг/м3

IPTTab::IPTTab(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);

    // ---------- Блок: внешняя среда ----------
    auto *outsideGroup = new QGroupBox("Внешняя среда (затрубное пространство)", this);
    auto *outsideForm = new QFormLayout(outsideGroup);
    m_densityOutsideEdit = new QLineEdit("1.0", this);
    outsideForm->addRow("Плотность внешней жидкости, г/см³:", m_densityOutsideEdit);

    // ---------- Блок: слои внутри ----------
    auto *layersGroup = new QGroupBox(
        "Внутренняя полость (герметизирована, не сообщается с затрубным)", this);
    auto *layersLayout = new QVBoxLayout(layersGroup);

    auto *layersTable = new QTableWidget(3, 3, layersGroup);
    layersTable->setHorizontalHeaderLabels({
        "Верхняя граница слоя от забоя, м",
        "Плотность, г/см³",
        "Объём слоя, м³"
    });
    layersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layersTable->verticalHeader()->setVisible(true);
    layersTable->setVerticalHeaderLabels({"Слой 1 (низ)", "Слой 2", "Слой 3 (верх)"});

    m_layers.resize(3);
    for (int i = 0; i < 3; ++i) {
        m_layers[i].heightEdit = new QLineEdit(layersTable);
        m_layers[i].densityEdit = new QLineEdit(layersTable);
        m_layers[i].volumeLabel = new QLabel("—", layersTable);
        m_layers[i].volumeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        m_layers[i].heightEdit->setPlaceholderText(i == 2 ? "0 = вся колонна" : "");
        m_layers[i].densityEdit->setPlaceholderText("0 = пусто");

        layersTable->setCellWidget(i, 0, m_layers[i].heightEdit);
        layersTable->setCellWidget(i, 1, m_layers[i].densityEdit);
        layersTable->setCellWidget(i, 2, m_layers[i].volumeLabel);

        connect(m_layers[i].heightEdit, &QLineEdit::textChanged, this, &IPTTab::recalculate);
        connect(m_layers[i].densityEdit, &QLineEdit::textChanged, this, &IPTTab::recalculate);
    }

    layersLayout->addWidget(new QLabel(
        "Высоты отсчитываются от забоя (низа колонны) вверх. "
        "Слой 1 — самый нижний. Если верхняя граница слоя 3 = 0, "
        "считается, что слой 3 занимает всю оставшуюся высоту до устья. "
        "Пустая часть внутренней полости заполнена воздухом.", layersGroup));
    layersLayout->addWidget(layersTable);

    // ---------- Блок: результаты ----------
    auto *resGroup = new QGroupBox("Результаты", this);
    auto *resGrid = new QGridLayout(resGroup);

    m_weightAirLabel          = new QLabel("—", this);
    m_buoyancyLabel           = new QLabel("—", this);
    m_innerLiquidWeightLabel  = new QLabel("—", this);
    m_airInToolWeightLabel    = new QLabel("—", this);
    m_weightInMudLabel        = new QLabel("—", this);
    m_totalInnerVolumeLabel   = new QLabel("—", this);
    m_filledInnerVolumeLabel  = new QLabel("—", this);
    m_emptyInnerVolumeLabel   = new QLabel("—", this);
    m_filledLengthLabel       = new QLabel("—", this);
    m_emptyLengthLabel        = new QLabel("—", this);
    m_totalOuterVolumeLabel   = new QLabel("—", this);
    m_metalVolumeLabel        = new QLabel("—", this);
    m_avgMass1mLabel          = new QLabel("—", this);
    m_rhoOutUsedLabel         = new QLabel("—", this);
    m_toolLengthLabel         = new QLabel("—", this);

    int r = 0;
    auto addRow = [&](const QString &label, QLabel *w) {
        resGrid->addWidget(new QLabel(label, this), r, 0);
        resGrid->addWidget(w, r, 1);
        ++r;
    };
    addRow("Вес инструмента в воздухе, т:", m_weightAirLabel);
    addRow("Полный объём по наружному d, м³:", m_totalOuterVolumeLabel);
    addRow("Объём металла, м³:", m_metalVolumeLabel);
    addRow("Выталкивающая сила (по наружному d), т:", m_buoyancyLabel);
    addRow("Вес внутренней жидкости, т:", m_innerLiquidWeightLabel);
    addRow("Вес воздуха внутри пустой части, т:", m_airInToolWeightLabel);
    addRow("Вес инструмента в жидкости (с учётом ИПТ), т:", m_weightInMudLabel);
    addRow("Полный внутренний объём колонны, м³:", m_totalInnerVolumeLabel);
    addRow("Заполненный внутренний объём, м³:", m_filledInnerVolumeLabel);
    addRow("Пустой внутренний объём, м³:", m_emptyInnerVolumeLabel);
    addRow("Длина заполненной части, м:", m_filledLengthLabel);
    addRow("Длина пустой части, м:", m_emptyLengthLabel);
    addRow("Длина инструмента, м:", m_toolLengthLabel);
    addRow("Средняя масса 1 м, кг/м:", m_avgMass1mLabel);
    addRow("Плотность внешней жидкости (использована), кг/м³:", m_rhoOutUsedLabel);

    auto *calcBtn = new QPushButton("Рассчитать", this);
    calcBtn->setFixedWidth(150);
    connect(calcBtn, &QPushButton::clicked, this, &IPTTab::recalculate);

    root->addWidget(outsideGroup);
    root->addWidget(layersGroup);
    root->addWidget(resGroup);
    root->addWidget(calcBtn, 0, Qt::AlignRight);
    root->addStretch();

    connect(m_densityOutsideEdit, &QLineEdit::textChanged, this, &IPTTab::recalculate);

    recalculate();
}

void IPTTab::syncFromMain(const QMap<QString, QString> &infoValues, QTableWidget *knbkTable)
{
    m_infoValues = infoValues;
    m_knbkTable = knbkTable;

    if (m_infoValues.contains("Плотность, г/см3:")) {
        QString d = m_infoValues["Плотность, г/см3:"].trimmed();
        if (!d.isEmpty()) {
            bool ok = false;
            d.replace(",", ".");
            d.toDouble(&ok);
            if (ok) {
                const QString cur = m_densityOutsideEdit->text().trimmed();
                if (cur.isEmpty() || cur == "1.0" || cur == "1,0") {
                    m_densityOutsideEdit->blockSignals(true);
                    m_densityOutsideEdit->setText(d);
                    m_densityOutsideEdit->blockSignals(false);
                }
            }
        }
    }

    recalculate();
}

double IPTTab::getInfoDouble(const QString &key, double def) const
{
    if (!m_infoValues.contains(key)) return def;
    bool ok = false;
    double v = m_infoValues[key].replace(",", ".").toDouble(&ok);
    return ok ? v : def;
}

QVector<IPTTab::Segment> IPTTab::buildSegments(double depth) const
{
    QVector<Segment> segs;
    if (!m_knbkTable) return segs;

    QVector<double> lengths;
    QVector<double> outerDs, innerDs, mass1ms, vol1ms;
    double totalLen = 0.0;

    for (int row = 0; row < m_knbkTable->rowCount(); ++row) {
        auto getVal = [&](int col) -> double {
            QTableWidgetItem *item = m_knbkTable->item(row, col);
            if (!item) return 0.0;
            bool ok = false;
            return item->text().replace(",", ".").toDouble(&ok);
        };
        double length = getVal(1);
        if (length <= 0) continue;

        lengths.append(length);
        outerDs.append(getVal(2) / 1000.0);
        innerDs.append(getVal(3) / 1000.0);
        mass1ms.append(getVal(4));
        vol1ms.append(getVal(5));
        totalLen += length;
    }

    if (lengths.isEmpty()) return segs;

    const double topAbs = std::max(0.0, depth - totalLen);

    double cursor = topAbs;
    for (int i = 0; i < lengths.size(); ++i) {
        Segment s;
        s.top = cursor;
        s.bottom = cursor + lengths[i];
        s.outerD = outerDs[i];
        s.innerD = innerDs[i];
        s.mass1m = mass1ms[i];

        // --- Металл: строго из геометрии ---
        const double dispGeom = M_PI * (s.outerD * s.outerD - s.innerD * s.innerD) / 4.0;
        s.disp1m = (dispGeom > 0) ? dispGeom : 0.0;

        // Fallback: если геометрия неполная, пробуем vol1m_l, но только
        // если он согласуется с массой (ρ металла в разумных пределах)
        if (s.disp1m <= 0 && vol1ms[i] > 0) {
            const double rho = s.mass1m > 0 ? (s.mass1m / vol1ms[i] * 1000.0) : STEEL_DENSITY;
            if (rho > 500.0 && rho < 20000.0) s.disp1m = vol1ms[i] / 1000.0;
        }

        // --- Внутренняя ёмкость: строго из геометрии ---
        const double capGeom = M_PI * s.innerD * s.innerD / 4.0;
        s.innerCap1m = (capGeom > 0) ? capGeom : 0.0;

        // --- Полный объём по наружному d: строго из outerD ---
        const double outerDispG = M_PI * s.outerD * s.outerD / 4.0;
        s.outerDisp1m = (outerDispG > 0) ? outerDispG : 0.0;

        // Масса: если не задана — из объёма металла
        if (s.mass1m <= 0 && s.disp1m > 0) s.mass1m = s.disp1m * STEEL_DENSITY;

        segs.append(s);
        cursor = s.bottom;
    }

    for (auto &s : segs) {
        s.top = std::max(0.0, s.top);
        s.bottom = std::min(depth, s.bottom);
    }
    segs.erase(std::remove_if(segs.begin(), segs.end(),
        [](const Segment &s){ return s.bottom <= s.top; }), segs.end());

    return segs;
}

void IPTTab::recalculate()
{
    auto toDouble = [](QLineEdit *le, bool *ok = nullptr) -> double {
        if (!le) { if (ok) *ok = false; return 0.0; }
        QString t = le->text(); t.replace(",", ".");
        return t.toDouble(ok);
    };

    const double depth = getInfoDouble("Глубина спуска, м:", 0.0);
    bool okRhoOut = false;
    const double rhoOut = toDouble(m_densityOutsideEdit, &okRhoOut);
    const double rhoOutKgM3 = (okRhoOut && rhoOut > 0) ? rhoOut * 1000.0 : 1000.0;

    if (depth <= 0 || !m_knbkTable) {
        m_weightAirLabel->setText("—");
        m_buoyancyLabel->setText("—");
        m_innerLiquidWeightLabel->setText("—");
        m_airInToolWeightLabel->setText("—");
        m_weightInMudLabel->setText("—");
        m_totalInnerVolumeLabel->setText("—");
        m_filledInnerVolumeLabel->setText("—");
        m_emptyInnerVolumeLabel->setText("—");
        m_filledLengthLabel->setText("—");
        m_emptyLengthLabel->setText("—");
        m_totalOuterVolumeLabel->setText("—");
        m_metalVolumeLabel->setText("—");
        m_avgMass1mLabel->setText("—");
        m_rhoOutUsedLabel->setText(QString::number(rhoOutKgM3, 'f', 1));
        m_toolLengthLabel->setText("—");
        for (auto &L : m_layers)
            if (L.volumeLabel) L.volumeLabel->setText("—");
        return;
    }

    const QVector<Segment> segs = buildSegments(depth);
    if (segs.isEmpty()) {
        m_weightAirLabel->setText("—");
        return;
    }

    // ---------- 1. Вес в воздухе, Архимед (по d_нар), объёмы ----------
    double weightAir_t = 0.0;
    double buoyancy_t = 0.0;
    double totalInnerVol = 0.0;
    double totalOuterVol = 0.0;
    double metalVol = 0.0;
    double toolLength = 0.0;
    double weightedMassSum = 0.0;   // Σ mass1m × len — для средней массы

    for (const auto &s : segs) {
        const double len = s.bottom - s.top;
        if (len <= 0) continue;
        weightAir_t   += len * s.mass1m / 1000.0;
        buoyancy_t    += rhoOutKgM3 * s.outerDisp1m * len / 1000.0;   // по наружному d
        totalInnerVol += s.innerCap1m * len;
        totalOuterVol += s.outerDisp1m * len;
        metalVol      += s.disp1m * len;
        toolLength    += len;
        weightedMassSum += s.mass1m * len;
    }

    // ---------- 2. Слои внутри ----------
    struct Layer {
        double topDepth = 0.0;
        double bottomDepth = 0.0;
        double rhoKgM3 = 0.0;
        bool active = false;
    };
    QVector<Layer> layers(3);

    const double toolBottomAbs = depth;
    const double toolTopAbs = depth - toolLength;

    double prevTopFromBottom = 0.0;
    for (int i = 0; i < 3; ++i) {
        bool okH = false, okR = false;
        double hFromBottom = toDouble(m_layers[i].heightEdit, &okH);
        double rho = toDouble(m_layers[i].densityEdit, &okR);
        if (!okH) hFromBottom = 0.0;
        if (!okR) rho = 0.0;

        if (rho <= 0.0) {
            m_layers[i].volumeLabel->setText("—");
            prevTopFromBottom = std::max(prevTopFromBottom, hFromBottom);
            continue;
        }

        double topFromBottom = hFromBottom;
        if (i == 2 && hFromBottom <= 0.0) topFromBottom = toolLength;
        if (topFromBottom <= prevTopFromBottom) {
            m_layers[i].volumeLabel->setText("—");
            continue;
        }

        Layer L;
        L.bottomDepth = toolBottomAbs - prevTopFromBottom;
        L.topDepth = toolBottomAbs - topFromBottom;
        L.topDepth = std::max(toolTopAbs, std::min(toolBottomAbs, L.topDepth));
        L.bottomDepth = std::max(toolTopAbs, std::min(toolBottomAbs, L.bottomDepth));
        L.rhoKgM3 = rho * 1000.0;
        L.active = true;
        layers[i] = L;

        prevTopFromBottom = topFromBottom;
    }

    // ---------- 3. Объём/вес по слоям + длины ----------
    double innerLiquidWeight_t = 0.0;
    double filledInnerVol = 0.0;
    double filledLength = 0.0;

    for (int li = 0; li < 3; ++li) {
        const Layer &L = layers[li];
        if (!L.active) continue;
        if (L.topDepth >= L.bottomDepth) {
            m_layers[li].volumeLabel->setText("0.000");
            continue;
        }

        double vLayer = 0.0;
        double lenLayer = 0.0;
        for (const auto &s : segs) {
            const double lo = std::max(L.topDepth, s.top);
            const double hi = std::min(L.bottomDepth, s.bottom);
            if (hi > lo) {
                vLayer += s.innerCap1m * (hi - lo);
                lenLayer += (hi - lo);
            }
        }
        m_layers[li].volumeLabel->setText(QString::number(vLayer, 'f', 4));
        innerLiquidWeight_t += L.rhoKgM3 * vLayer / 1000.0;
        filledInnerVol += vLayer;
        filledLength += lenLayer;
    }

    const double emptyInnerVol = std::max(0.0, totalInnerVol - filledInnerVol);
    const double emptyLength = std::max(0.0, toolLength - filledLength);

    // ---------- 4. Вес воздуха внутри пустой части ----------
    const double airWeight_t = AIR_DENSITY * emptyInnerVol / 1000.0;

    // ---------- 5. Итог ----------
    // W = W_air − Архимед(по d_нар) + W_жидкости_внутри + W_воздуха_внутри
    const double weightInMud_t =
        weightAir_t - buoyancy_t + innerLiquidWeight_t + airWeight_t;

    const double avgMass1m = (toolLength > 0) ? (weightedMassSum / toolLength) : 0.0;

    m_weightAirLabel->setText(QString::number(weightAir_t, 'f', 3));
    m_totalOuterVolumeLabel->setText(QString::number(totalOuterVol, 'f', 4));
    m_metalVolumeLabel->setText(QString::number(metalVol, 'f', 4));
    m_buoyancyLabel->setText(QString::number(buoyancy_t, 'f', 3));
    m_innerLiquidWeightLabel->setText(QString::number(innerLiquidWeight_t, 'f', 3));
    m_airInToolWeightLabel->setText(QString::number(airWeight_t, 'f', 6));
    m_weightInMudLabel->setText(QString::number(weightInMud_t, 'f', 3));
    m_totalInnerVolumeLabel->setText(QString::number(totalInnerVol, 'f', 4));
    m_filledInnerVolumeLabel->setText(QString::number(filledInnerVol, 'f', 4));
    m_emptyInnerVolumeLabel->setText(QString::number(emptyInnerVol, 'f', 4));
    m_filledLengthLabel->setText(QString::number(filledLength, 'f', 2));
    m_emptyLengthLabel->setText(QString::number(emptyLength, 'f', 2));
    m_toolLengthLabel->setText(QString::number(toolLength, 'f', 2));
    m_avgMass1mLabel->setText(QString::number(avgMass1m, 'f', 3));
    m_rhoOutUsedLabel->setText(QString::number(rhoOutKgM3, 'f', 1));
}