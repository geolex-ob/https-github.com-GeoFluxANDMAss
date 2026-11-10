#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QLabel>
#include <QComboBox>
#include <QMap>
#include <QVector>

// Вкладка «ИПТ — вес инструмента при герметизированной внутренней полости».
// Внутренняя полость НЕ сообщается с внешней, внутри до 3 слоёв жидкости
// с заданными высотами от забоя (от низа колонны вверх).
//
// Методика расчёта (по требованию заказчика):
//   W_в_жидкости = W_воздух
//                − ρ_внеш × V_полный × g        (Архимед по наружному диаметру)
//                + W_жидкости_внутри            (вес заполненных слоёв)
//                + W_воздуха_внутри             (пренебрежимо)
// где V_полный = π/4 × d_нар² × L — объём всего цилиндра по наружному диаметру.
class IPTTab : public QWidget
{
    Q_OBJECT

public:
    explicit IPTTab(QWidget *parent = nullptr);

    void syncFromMain(
        const QMap<QString, QString> &infoValues,
        QTableWidget *knbkTable
    );

private slots:
    void recalculate();

private:
    QLineEdit *m_densityOutsideEdit = nullptr;

    struct LayerRow {
        QLineEdit *heightEdit = nullptr;
        QLineEdit *densityEdit = nullptr;
        QLabel    *volumeLabel = nullptr;
    };
    QVector<LayerRow> m_layers;

    QLabel *m_weightAirLabel = nullptr;
    QLabel *m_buoyancyLabel = nullptr;
    QLabel *m_innerLiquidWeightLabel = nullptr;
    QLabel *m_airInToolWeightLabel = nullptr;
    QLabel *m_weightInMudLabel = nullptr;
    QLabel *m_totalInnerVolumeLabel = nullptr;
    QLabel *m_filledInnerVolumeLabel = nullptr;
    QLabel *m_emptyInnerVolumeLabel = nullptr;
    QLabel *m_filledLengthLabel = nullptr;
    QLabel *m_emptyLengthLabel = nullptr;
    QLabel *m_totalOuterVolumeLabel = nullptr;   // полный объём по d_нар
    QLabel *m_metalVolumeLabel = nullptr;        // объём металла
    QLabel *m_avgMass1mLabel = nullptr;          // средняя масса 1 м
    QLabel *m_rhoOutUsedLabel = nullptr;         // плотность внешней (использована)
    QLabel *m_toolLengthLabel = nullptr;         // длина инструмента

    QTableWidget *m_knbkTable = nullptr;
    QMap<QString, QString> m_infoValues;

    struct Segment {
        double top = 0.0;
        double bottom = 0.0;
        double outerD = 0.0;
        double innerD = 0.0;
        double mass1m = 0.0;
        double disp1m = 0.0;       // объём металла, м3/м
        double outerDisp1m = 0.0;  // объём по наружному диаметру, м3/м
        double innerCap1m = 0.0;   // внутренняя ёмкость, м3/м
    };

    QVector<Segment> buildSegments(double depth) const;
    double getInfoDouble(const QString &key, double def = 0.0) const;
};