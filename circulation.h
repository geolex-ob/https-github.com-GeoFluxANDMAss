#ifndef CIRCULATION_H
#define CIRCULATION_H

#include "wellconstruction.h"
#include "tool.h"
#include <QVector>

struct LayoutItem {
    Tool tool;
    double length;         // м
    int quantity = 1;

    double volumeInner() const;    // внутренний объем элемента, м³
    double volumeOuter() const;    // наружный объем элемента, м³
    double weightInAir() const;    // вес в воздухе, тонн
    double weightInFluid(double fluidDensity) const; // вес в жидкости, тонн
};

class CirculationCalculator
{
public:
    CirculationCalculator();

    void setLayout(const QVector<LayoutItem> &layout) { m_layout = layout; }
    void setWellConstruction(const WellConstruction &wc) { m_wellConstruction = wc; }
    void setFluidDensity(double density) { m_fluidDensity = density; }
    void setFlowRate(double lps) { m_flowRateLps = lps; }

    // Расчет полного времени циркуляции (минут) - от насоса до насоса
    double calculateCirculationTime() const;

    // Расчет времени подъема жидкости от забоя до устья (минут)
    double calculateBottomUpTime() const;
    
    // Расчет времени от устья до забоя (минут)
    double calculateSurfaceToBottomTime() const;

    // Полный объем скважины с учетом инструмента (м³)
    double totalWellVolume() const;
    
    // Объем затрубного пространства (м³)
    double annulusVolume() const;
    
    // Внутренний объем инструмента (м³)
    double toolInnerVolume() const;

    // Расчет веса всей компоновки в воздухе (тонн)
    double totalWeightInAir() const;

    // Расчет веса всей компоновки в жидкости (тонн)
    double totalWeightInFluid() const;

    QVector<double> cumulativeVolume() const;
    QVector<double> cumulativeWeightAir() const;
    QVector<double> cumulativeWeightFluid() const;

    QVector<LayoutItem> layout() const { return m_layout; }

    double fluidDensity() const { return m_fluidDensity; }
    double flowRateLps() const { return m_flowRateLps; }

private:
    QVector<LayoutItem> m_layout;
    WellConstruction m_wellConstruction;
    double m_fluidDensity = 1200.0; // кг/м³
    double m_flowRateLps = 30.0;   // л/с
};

#endif // CIRCULATION_H
