#include "knbkeditordialog.h"
#include "mainwindow.h" // KNBK_CSV_HEADER
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QRegularExpression>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QCoreApplication>
#include <QMessageBox>
#include <QStringConverter>
#include <cmath>

// Плотность стали, кг/м3 (для расчета веса 1 м инструмента по разности OD^2 - ID^2)
static const double STEEL_DENSITY_KG_M3 = 7850.0;

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

    // Колонки справочника соответствуют первым 6 колонкам единого формата knbk.csv
    // (см. KNBK_CSV_HEADER в mainwindow.h). Раньше колонки 4 и 5 шли в обратном порядке
    // ("Объем 1 м" перед "Вес 1 м"), из-за чего при сохранении/загрузке вес и объем
    // менялись местами и расчет веса инструмента давал неверный результат.
    m_table = new QTableWidget(0, 6, this);
    m_table->setHorizontalHeaderLabels(KNBK_CSV_HEADER.mid(0, 6));
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

bool KNBKEditorDialog::isManual(QTableWidgetItem *item, int role) const
{
    return item && item->data(role).toBool();
}

void KNBKEditorDialog::setManual(QTableWidgetItem *item, int role, bool manual)
{
    if (item) item->setData(role, manual);
}

void KNBKEditorDialog::addRow()
{
    int row = m_table->rowCount();
    m_table->insertRow(row);
    for (int i = 0; i < m_table->columnCount(); ++i) {
        m_table->setItem(row, i, new QTableWidgetItem(""));
    }

    // col 4 = "Масса 1 м, кг" и col 5 = "Объем 1 м, л" — РЕДАКТИРУЕМЫЕ.
    // По умолчанию заполняются авто-расчётом из OD/ID (сталь 7850 кг/м3),
    // но пользователь может ввести своё паспортное значение веса погонного
    // метра (учёт выточек, канавок, приваренных деталей, другая плотность
    // материала). Введённое вручную значение помечается ролью ManualMassRole/
    // ManualVolRole и не перетирается авто-расчётом до следующей ручной правки.
    for (int col : {4, 5}) {
        if (m_table->item(row, col)) {
            m_table->item(row, col)->setFlags(m_table->item(row, col)->flags() | Qt::ItemIsEditable);
            m_table->item(row, col)->setBackground(QBrush(Qt::white));
        }
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
    if (!item) return;
    const int col = item->column();
    if (col == 2 || col == 3) {
        // Изменился диаметр — авто-пересчёт массы/объёма (ручные значения сохраняем)
        recalculateRow(item->row());
    } else if (col == 4 || col == 5) {
        // Ручной ввод веса или объёма погонного метра: запоминаем факт ручного
        // ввода и синхронизируем парное значение (кг <-> л через плотность стали).
        const int otherCol = (col == 4) ? 5 : 4;
        const int thisRole = (col == 4) ? ManualMassRole : ManualVolRole;
        const int otherRole = (col == 4) ? ManualVolRole : ManualMassRole;
        setManual(item, thisRole, true);
        if (m_table->item(item->row(), otherCol))
            setManual(m_table->item(item->row(), otherCol), otherRole, true);

        bool ok = false;
        double v = item->text().replace(",", ".").toDouble(&ok);
        m_table->blockSignals(true);
        if (ok && v > 0) {
            QTableWidgetItem *other = m_table->item(item->row(), otherCol);
            if (other) {
                double ov = (col == 4) ? v / STEEL_DENSITY_KG_M3 * 1000.0   // кг/м -> л/м металла
                                       : v * STEEL_DENSITY_KG_M3 / 1000.0;  // л/м -> кг/м
                other->setText(QString::number(ov, 'f', (col == 4) ? 3 : 2));
            }
        } else {
            if (QTableWidgetItem *other = m_table->item(item->row(), otherCol))
                other->setText("");
        }
        m_table->blockSignals(false);
    }
}

void KNBKEditorDialog::recalculateRow(int row)
{
    if (!m_table->item(row, 2) || !m_table->item(row, 3)) return;

    bool ok1, ok2;
    QString odStr = m_table->item(row, 2)->text();
    QString idStr = m_table->item(row, 3)->text();
    odStr.replace(",", ".");
    idStr.replace(",", ".");
    double outerD_mm = odStr.toDouble(&ok1);
    double innerD_mm = idStr.toDouble(&ok2);

    QTableWidgetItem *massItem = m_table->item(row, 4);
    QTableWidgetItem *volItem  = m_table->item(row, 5);

    if (ok1 && ok2 && outerD_mm > 0 && innerD_mm > 0) {
        double d_out = outerD_mm / 1000.0;
        double d_in = innerD_mm / 1000.0;

        // Объем металла стенки: pi/4 * (OD^2 - ID^2). При ID >= OD значение было бы
        // отрицательным (физически невозможно) — в этом случае поля не заполняем.
        double v_metal_m3 = M_PI * (std::pow(d_out, 2) - std::pow(d_in, 2)) / 4.0;
        if (v_metal_m3 <= 0) {
            m_table->blockSignals(true);
            if (massItem) massItem->setText("");
            if (volItem)  volItem->setText("");
            m_table->blockSignals(false);
            return;
        }
        double weight_kg = v_metal_m3 * STEEL_DENSITY_KG_M3;

        m_table->blockSignals(true);

        // Колонка 4 = "Масса 1 м, кг", колонка 5 = "Объем 1 м, л" — ОБЪЕМ МЕТАЛЛА
        // погонного метра полового инструмента (стенки): pi/4*(OD^2 - ID^2).
        // Значения, введённые пользователем вручную (роль Manual*Role), НЕ
        // перетираются — иначе ручной вес погонного метра терялся при любой
        // правке диаметров.
        if (massItem && !isManual(massItem, ManualMassRole))
            massItem->setText(QString::number(weight_kg, 'f', 2));
        if (volItem && !isManual(volItem, ManualVolRole))
            volItem->setText(QString::number(v_metal_m3 * 1000.0, 'f', 3)); // л на 1 м

        m_table->blockSignals(false);
    } else {
        m_table->blockSignals(true);
        if (massItem && !isManual(massItem, ManualMassRole)) massItem->setText("");
        if (volItem && !isManual(volItem, ManualVolRole))    volItem->setText("");
        m_table->blockSignals(false);
    }
}

void KNBKEditorDialog::loadCSV()
{
    QFile file(getDataPath("knbk.csv"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    QStringList allLines;
    while (!in.atEnd()) allLines << in.readLine();
    file.close();

    m_table->blockSignals(true);
    m_table->setRowCount(0);

    // Порядок колонок "масса/объем" определяется по заголовку файла.
    // Файл читаем целиком: заголовком считаем только строку с буквами, иначе
    // первая строка — данные и терять её нельзя (старый код терял первую запись).
    // Старые файлы могли быть сохранены в формате "...;Объем 1 м, л;Вес 1 м, кг".
    int colMass = 4, colVol = 5;
    bool headerSeen = false;
    int startIdx = 0;
    if (!allLines.isEmpty()) {
        const QString firstLine = allLines.first();
        const QStringList headers = firstLine.split(";");
        // Заголовком считаем строку, содержащую буквы (а не числа/пунктуацию).
        static const QRegularExpression lettersRe("[^0-9.;,\\s-]");
        if (firstLine.contains(lettersRe)) {
            headerSeen = true;
            startIdx = 1;
            for (int i = 0; i < headers.size(); ++i) {
                const QString raw = headers[i].trimmed().toLower();
                // Колонки "Масса всего"/"Объем всего" игнорируем — это производные поля.
                if ((raw.contains("масса") || raw.contains("вес")) && !raw.contains("всего")) colMass = i;
                else if ((raw.contains("объем") || raw.contains("объём")) && !raw.contains("всего")) colVol = i;
            }
        }
    }

    for (int li = startIdx; li < allLines.size(); ++li) {
        const QString line = allLines[li];
        if (line.trimmed().isEmpty()) continue;
        QStringList fields = line.split(";");

        // Если заголовок отсутствовал, а данные физически противоречат новому формату
        // (масса 1 м < 10 кг/м невозможна, а в колонке массы стоит число больше 500,
        // что невозможно для объема внутренней полости) — считаем колонки перепутанными.
        if (!headerSeen && fields.size() >= 6) {
            double v4 = fields[4].replace(",", ".").toDouble();
            double v5 = fields[5].replace(",", ".").toDouble();
            if (v4 < 10.0 && v5 > 500.0) { colMass = 5; colVol = 4; }
        }

        auto fieldAt = [&](int col) -> QString {
            return (col >= 0 && col < fields.size()) ? fields[col] : QString();
        };

        int row = m_table->rowCount();
        m_table->insertRow(row);
        
        for (int col = 0; col < m_table->columnCount(); ++col) {
            QString val;
            if (col == 4) val = fieldAt(colMass);
            else if (col == 5) val = fieldAt(colVol);
            else val = fieldAt(col);
            QTableWidgetItem *item = new QTableWidgetItem(val);

            // Колонки "Масса 1 м, кг" и "Объем 1 м, л" редактируемые: вес
            // погонного метра можно ввести вручную (паспортные данные).
            item->setFlags(item->flags() | Qt::ItemIsEditable);
            item->setBackground(QBrush(Qt::white));
            m_table->setItem(row, col, item);
        }

        bool recalcNeeded = false;
        if (fieldAt(colMass).isEmpty() || fieldAt(colVol).isEmpty()) {
            recalcNeeded = true;
        } else if (colMass == 4 && colVol == 5) {
            // Проверка сохраненных значений: ручные значения (масса и/или объем,
            // введенные в справочнике) НЕ перетираем. Авто-пересчет включаем
            // только если оба значения соответствуют геометрии стали с точностью
            // до 2% — иначе это паспортные/ручные данные или мусор старого формата.
            bool okO = false, okI = false;
            QString odStr = fields.value(2);
            QString idStr = fields.value(3);
            odStr.replace(",", ".");
            idStr.replace(",", ".");
            double od = odStr.toDouble(&okO);
            double id = idStr.toDouble(&okI);
            if (okO && okI && od > 0 && id > 0 && id < od) {
                double expected_l = M_PI * ((od / 1000.0) * (od / 1000.0) - (id / 1000.0) * (id / 1000.0)) / 4.0 * 1000.0;
                double stored_v = fieldAt(colVol).replace(",", ".").toDouble();
                double stored_m = fieldAt(colMass).replace(",", ".").toDouble();
                const bool volGeomOk = expected_l > 0 && std::fabs(stored_v - expected_l) <= 0.02 * expected_l;
                const bool massGeomOk = expected_l > 0 && std::fabs(stored_m - expected_l * STEEL_DENSITY_KG_M3 / 1000.0) <= 0.02 * (expected_l * STEEL_DENSITY_KG_M3 / 1000.0);
                // Мусор старого формата: объем не соответствует геометрии, но масса
                // тоже не образует с ним физически возможную пару (паспортный вариант)
                const bool plausiblePair = stored_v > 0 && stored_m > 0 &&
                        stored_m / stored_v * 1000.0 >= 500.0 && stored_m / stored_v * 1000.0 <= 20000.0;
                if (!volGeomOk && !massGeomOk && !plausiblePair) {
                    recalcNeeded = true;
                }
            }
        }
        if (recalcNeeded) {
            recalculateRow(row);
        } else {
            // Значения из файла сохраняем как ручные: при последующей правке
            // диаметров авто-расчёт их не перетирает.
            setManual(m_table->item(row, 4), ManualMassRole, true);
            setManual(m_table->item(row, 5), ManualVolRole, true);
        }
    }
    m_table->blockSignals(false);
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