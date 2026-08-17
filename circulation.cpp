#include "circulation.h"
#include <cmath>
#include <algorithm>

double LayoutItem::volumeCapacity() const
{
    // Внутренний объем (емкость) — объем раствора внутри инструмента.
    // Если выбран расчет "По плотности", вычисляем внутренний объем
    // как разность наружного цилиндра и объема металла из веса.
    if (tool.volumeCalcMethod() == Tool::ByDensity && tool.density() > 0.0) {
        double outerDiamM = tool.outerDiameter() / 1000.0;
        double outerArea = M_PI / 4.0 * outerDiamM * outerDiamM;
        double metalVolPerMeter = tool.weightPerMeter() / tool.density();
        double innerArea = outerArea - metalVolPerMeter;
        if (innerArea < 0) innerArea = 0;
        return innerArea * length * quantity;
    } 
    
    // Иначе считаем строго по внутреннему диаметру
    double innerDiamM = tool.innerDiameter() / 1000.0;
    double area = M_PI / 4.0 * innerDiamM * innerDiamM;
    return area * length * quantity;
}

double LayoutItem::volumeDisplacement() const
{
    // Наружный объем (вытеснение). Зависит только от внешнего диаметра.
    // Это объем полного цилиндра, который инструмент занимает в скважине.
    double outerDiamM = tool.outerDiameter() / 1000.0;
    double area = M_PI / 4.0 * outerDiamM * outerDiamM;
    return area * length * quantity;
}

double LayoutItem::volumeMetal() const
{
    // Объем металла (стали). Внешний минус внутренний.
    // Если выбран расчет "По плотности", берем объем металла из веса.
    if (tool.volumeCalcMethod() == Tool::ByDensity && tool.density() > 0.0) {
        double metalVolPerMeter = tool.weightPerMeter() / tool.density();
        return metalVolPerMeter * length * quantity;
    }
    
    // Иначе считаем геометрически: D² - d²
    double outerDiamM = tool.outerDiameter() / 1000.0;
    double innerDiamM = tool.innerDiameter() / 1000.0;
    double area = M_PI / 4.0 * (outerDiamM * outerDiamM - innerDiamM * innerDiamM);
    return area * length * quantity;
}

double LayoutItem::weightInAir() const
{
    return tool.weightPerMeter() * length * quantity / 1000.0; // тонн
}

double LayoutItem::weightInFluid(double fluidDensity) const
{
    // Вес в жидкости = вес в воздухе - выталкивающая сила
    double weightAir = weightInAir();
    double buoyancy = volumeMetal() * fluidDensity / 1000.0; // тонн
    return weightAir - buoyancy;
}

CirculationCalculator::CirculationCalculator()
{
}

double CirculationCalculator::calculateCirculationTime() const
{
    double totalVolume = totalWellVolume(); // м³
    double flowRateM3perSec = m_flowRateLps / 1000.0;
    if (flowRateM3perSec <= 0) return 0.0;
    return totalVolume / flowRateM3perSec / 60.0; // минуты
}

double CirculationCalculator::calculateBottomUpTime() const
{
    // Время подъема от забоя до устья = объем затрубного пространства / расход
    double vol = annulusVolume(); // м³
    double flowRateM3perSec = m_flowRateLps / 1000.0;
    if (flowRateM3perSec <= 0) return 0.0;
    return vol / flowRateM3perSec / 60.0; // минуты
}

double CirculationCalculator::calculateSurfaceToBottomTime() const
{
    // Время от устья до забоя = внутренний объем инструмента / расход
    double vol = toolInnerVolume(); // м³
    double flowRateM3perSec = m_flowRateLps / 1000.0;
    if (flowRateM3perSec <= 0) return 0.0;
    return vol / flowRateM3perSec / 60.0; // минуты
}

double CirculationCalculator::totalWellVolume() const
{
    return toolInnerVolume() + annulusVolume();
}

double CirculationCalculator::annulusVolume() const
{
    // 1. Считаем полный объем ствола скважины
    double totalHoleVolume = 0.0;
    for (const auto &c : m_wellConstruction.casings()) {
        double sectionLength = c.endDepth - c.startDepth;
        if (sectionLength <= 0) continue;
        
        double holeDiamM;
        if (c.isOpenHole) {
            holeDiamM = (c.outerDiameter * c.cavernosity) / 1000.0;
        } else {
            holeDiamM = c.innerDiameter / 1000.0;
        }
        
        double holeArea = M_PI / 4.0 * holeDiamM * holeDiamM;
        totalHoleVolume += holeArea * sectionLength;
    }
    
    // 2. Вычитаем объем вытеснения (внешний объем) всей компоновки.
    // Это физически корректный расчет затрубного кольца.
    double totalDisplacement = 0.0;
    for (const auto &item : m_layout) {
        totalDisplacement += item.volumeDisplacement(); 
    }
    
    if (totalHoleVolume > totalDisplacement) {
        return totalHoleVolume - totalDisplacement;
    }
    return 0.0;
}

double CirculationCalculator::toolInnerVolume() const
{
    double totalVolume = 0.0;
    for (const auto &item : m_layout) {
        totalVolume += item.volumeCapacity();
    }
    return totalVolume;
}

double CirculationCalculator::totalWeightInAir() const
{
    double total = 0.0;
    for (const auto &item : m_layout)
        total += item.weightInAir();
    return total;
}

double CirculationCalculator::totalWeightInFluid() const
{
    double total = 0.0;
    for (const auto &item : m_layout)
        total += item.weightInFluid(m_fluidDensity);
    return total;
}

QVector<double> CirculationCalculator::cumulativeVolume() const
{
    QVector<double> result;
    double sum = 0.0;
    for (int i = m_layout.size() - 1; i >= 0; --i) {
        sum += m_layout[i].volumeCapacity(); 
        result.prepend(sum);
    }
    return result;
}

QVector<double> CirculationCalculator::cumulativeWeightAir() const
{
    QVector<double> result;
    double sum = 0.0;
    for (int i = m_layout.size() - 1; i >= 0; --i) {
        sum += m_layout[i].weightInAir();
        result.prepend(sum);
    }
    return result;
}

QVector<double> CirculationCalculator::cumulativeWeightFluid() const
{
    QVector<double> result;
    double sum = 0.0;
    for (int i = m_layout.size() - 1; i >= 0; --i) {
        sum += m_layout[i].weightInFluid(m_fluidDensity);
        result.prepend(sum);
    }
    return result;
}
