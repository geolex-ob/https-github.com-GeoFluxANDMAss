#pragma once

#include <QWidget>

class QLineEdit;
class QLabel;

// Виджет-вкладка «Объём горизонтального цилиндра (лежащего на боку)».
// Ввод: диаметр, длина, высота жидкости. Вывод: объём жидкости, полный объём,
// процент заполнения. Справа — схема торца цилиндра с заливкой жидкости.
class TankVolumeTab : public QWidget
{
    Q_OBJECT

public:
    explicit TankVolumeTab(QWidget *parent = nullptr);

    // Отрисовка схемы торца. Вызывается из вложенного «холста» (EndView)
    // в его paintEvent(), чтобы не усложнять иерархию классов.
    void drawEndView(QWidget *canvas);

private slots:
    void recalculate();

private:
    QLineEdit *m_diameterEdit = nullptr;  // внутренний диаметр, мм
    QLineEdit *m_lengthEdit   = nullptr;  // длина цилиндра, м
    QLineEdit *m_levelEdit    = nullptr;  // высота жидкости от низа, мм

    QLabel *m_volumeLabel      = nullptr; // объём жидкости, м3
    QLabel *m_fullVolumeLabel  = nullptr; // полный объём, м3
    QLabel *m_fillPercentLabel = nullptr; // заполнение, %

    // Текущие значения для отрисовки (в метрах)
    double m_diameter_m = 1.0;
    double m_level_m    = 0.5;

    // Площадь сегмента круга радиуса R при высоте h (0 <= h <= 2R).
    static double segmentArea(double R, double h);
};