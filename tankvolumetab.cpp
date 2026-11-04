#include "tankvolumetab.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QFont>
#include <QSizePolicy>
#include <cmath>
#include <algorithm>

// Внутренний класс-«холст» для схемы торца. Рисование делегируется
// TankVolumeTab::drawEndView(), чтобы вся геометрия была в одном месте.
namespace {
    class EndView : public QWidget
    {
    public:
        explicit EndView(TankVolumeTab *owner) : QWidget(owner), m_owner(owner) {
            setMinimumSize(320, 320);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        }
    protected:
        void paintEvent(QPaintEvent *) override {
            if (m_owner) m_owner->drawEndView(this);
        }
    private:
        TankVolumeTab *m_owner = nullptr;
    };
} // namespace

TankVolumeTab::TankVolumeTab(QWidget *parent) : QWidget(parent)
{
    auto *root = new QHBoxLayout(this);

    // ---------------- Левая колонка: ввод и результаты ----------------
    auto *left = new QVBoxLayout();

    auto *inputGroup = new QGroupBox("Исходные данные (все размеры в метрах)", this);
    auto *form = new QFormLayout(inputGroup);

    m_diameterEdit = new QLineEdit("1.0", this); // м
    m_lengthEdit   = new QLineEdit("5.0", this); // м
    m_levelEdit    = new QLineEdit("0.3", this); // м

    form->addRow("Диаметр цилиндра, м:", m_diameterEdit);
    form->addRow("Длина цилиндра, м:",    m_lengthEdit);
    form->addRow("Высота жидкости, м:",  m_levelEdit);

    auto *resGroup = new QGroupBox("Результаты", this);
    auto *resForm = new QFormLayout(resGroup);

    m_volumeLabel      = new QLabel("—", this);
    m_fullVolumeLabel  = new QLabel("—", this);
    m_fillPercentLabel = new QLabel("—", this);

    resForm->addRow("Объём жидкости, м³:", m_volumeLabel);
    resForm->addRow("Полный объём, м³:",   m_fullVolumeLabel);
    resForm->addRow("Заполнение, %:",      m_fillPercentLabel);

    left->addWidget(inputGroup);
    left->addWidget(resGroup);
    left->addStretch();

    // ---------------- Правая колонка: схема торца ----------------
    auto *canvas = new EndView(this);

    root->addLayout(left, 1);
    root->addWidget(canvas, 1);

    // Сигналы: любое изменение поля → пересчёт и перерисовка
    connect(m_diameterEdit, &QLineEdit::textChanged, this, &TankVolumeTab::recalculate);
    connect(m_lengthEdit,   &QLineEdit::textChanged, this, &TankVolumeTab::recalculate);
    connect(m_levelEdit,    &QLineEdit::textChanged, this, &TankVolumeTab::recalculate);

    recalculate();
}

double TankVolumeTab::segmentArea(double R, double h)
{
    if (R <= 0) return 0.0;
    h = std::max(0.0, std::min(h, 2.0 * R));
    if (h <= 0.0) return 0.0;
    if (h >= 2.0 * R) return M_PI * R * R;
    return R * R * std::acos((R - h) / R)
    - (R - h) * std::sqrt(std::max(0.0, 2.0 * R * h - h * h));
}

void TankVolumeTab::recalculate()
{
    auto toDouble = [](const QString &s, bool *ok = nullptr) {
        QString t = s; t.replace(",", ".");
        return t.toDouble(ok);
    };

    bool okD=false, okL=false, okH=false;
    // Все три значения — уже в метрах, никаких пересчётов из мм.
    const double D = toDouble(m_diameterEdit->text(), &okD); // м
    const double L = toDouble(m_lengthEdit->text(),   &okL); // м
    const double H = toDouble(m_levelEdit->text(),    &okH); // м

    if (!okD || !okL || !okH || D <= 0 || L <= 0) {
        m_volumeLabel->setText("—");
        m_fullVolumeLabel->setText("—");
        m_fillPercentLabel->setText("—");
        m_diameter_m = 1.0;
        m_level_m = 0.0;
        update();
        return;
    }

    const double R = D / 2.0;                                  // м
    const double Hclamped = std::max(0.0, std::min(H, D));     // м, 0..D
    const double A = segmentArea(R, Hclamped);                 // м2
    const double V_liquid = A * L;                             // м3
    const double V_full   = M_PI * R * R * L;                  // м3
    const double fillPct  = (V_full > 0) ? (V_liquid / V_full * 100.0) : 0.0;

    m_volumeLabel->setText(QString::number(V_liquid, 'f', 3));
    m_fullVolumeLabel->setText(QString::number(V_full, 'f', 3));
    m_fillPercentLabel->setText(QString::number(fillPct, 'f', 1));

    m_diameter_m = D;
    m_level_m = Hclamped;
    update(); // перерисовать схему
}

// Отрисовка схемы торца: круг + сегмент жидкости (вид с торца горизонтального
// цилиндра). Уровень жидкости отсчитывается от нижней точки круга.
// Все входные значения — в метрах; в пикселях — только геометрия отрисовки.
void TankVolumeTab::drawEndView(QWidget *canvas)
{
    if (!canvas) return;

    QPainter p(canvas);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int w = canvas->width();
    const int h = canvas->height();
    if (w < 20 || h < 20) return;

    const double margin = 20.0;
    const double availW = w - 2 * margin;
    const double availH = h - 2 * margin;
    const double radiusPx = std::min(availW, availH) / 2.0;
    if (radiusPx <= 0) return;

    const QPointF center(w / 2.0, h / 2.0);

    // Масштаб: круг всегда вписан в доступную область.
    const double D = std::max(1e-9, m_diameter_m);            // м
    const double levelFrac = std::max(0.0, std::min(m_level_m / D, 1.0)); // 0..1

    // ---- Фон ёмкости (круг) ----
    QRectF circleRect(center.x() - radiusPx, center.y() - radiusPx,
                      2 * radiusPx, 2 * radiusPx);

    p.setPen(QPen(QColor(40, 40, 40), 2));
    p.setBrush(QColor(240, 245, 250));
    p.drawEllipse(circleRect);

    // ---- Сегмент жидкости ----
    const double levelPx = levelFrac * 2.0 * radiusPx;
    const double yLine = circleRect.bottom() - levelPx;

    QPainterPath circlePath;
    circlePath.addEllipse(circleRect);

    QPainterPath clipRect;
    clipRect.addRect(QRectF(0, yLine, w, h));

    QPainterPath liquid = circlePath.intersected(clipRect);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(60, 140, 220, 180)); // полупрозрачный синий
    p.drawPath(liquid);

    // Обводка круга заново (чтобы линия не перекрывалась заливкой)
    p.setPen(QPen(QColor(40, 40, 40), 2));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(circleRect);

    // Линия уровня
    if (levelFrac > 0.0 && levelFrac < 1.0) {
        p.setPen(QPen(QColor(30, 90, 160), 2, Qt::SolidLine));
        p.drawLine(QPointF(circleRect.left(), yLine), QPointF(circleRect.right(), yLine));
    }

    // Подпись уровня — в метрах
    p.setPen(QColor(20, 20, 20));
    QFont f = p.font();
    f.setPointSizeF(9.0);
    p.setFont(f);

    const QString label = QString("D = %1 м   h = %2 м   %3%")
    .arg(m_diameter_m, 0, 'f', 3)
    .arg(m_level_m, 0, 'f', 3)
    .arg(levelFrac * 100.0, 0, 'f', 1);
    p.drawText(QRectF(10, 6, w - 20, 20), Qt::AlignLeft | Qt::AlignVCenter, label);

    // Центр (для наглядности)
    p.setBrush(QColor(40, 40, 40));
    p.setPen(Qt::NoPen);
    p.drawEllipse(center, 2.5, 2.5);
}
