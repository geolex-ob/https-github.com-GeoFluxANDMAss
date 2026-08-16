#include "circulation.h"
#include <cmath>
#include <algorithm>

double LayoutItem::volumeInner() const
{
    double innerDiamM = tool.innerDiameter() / 1000.0;
    double area = M_PI / 4.0 * innerDiamM * innerDiamM;
    return area * length * quantity;
}

double LayoutItem::volumeOuter() const
{
    double outerDiamM = tool.outerDiameter() / 1000.0;
    double area = M_PI / 4.0 * outerDiamM * outerDiamM;
    return area * length * quantity;
}

double LayoutItem::weightInAir() const
{
    return tool.weightPerMeter() * length * quantity / 1000.0; // тонн
}

double LayoutItem::weightInFluid(double fluidDensity) const
{
    double weightAir = weightInAir(); // вес в воздухе, тонн

    double steelDensity = tool.density();

    if (steelDensity <= 0.0)
        steelDensity = 7850.0;

    // Коэффициент плавучести
    double buoyancyFactor = 1.0 - (fluidDensity / steelDensity);

    return weightAir * buoyancyFactor;
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
    double totalVolume = 0.0;
    
    // Определяем максимальный наружный диаметр инструмента
    double maxToolOD = 0.0;
    for (const auto &item : m_layout) {
        if (item.tool.outerDiameter() > maxToolOD)
            maxToolOD = item.tool.outerDiameter();
    }
    
    // Вычисляем общую длину инструмента
    double totalToolLength = 0.0;
    for (const auto &item : m_layout) {
        totalToolLength += item.length;
    }
    
    double remainingToolLength = totalToolLength;
    double toolOD_M = maxToolOD / 1000.0;
    double toolArea = M_PI / 4.0 * toolOD_M * toolOD_M;
    
    for (const auto &c : m_wellConstruction.casings()) {
        double sectionLength = c.endDepth - c.startDepth;
        if (sectionLength <= 0) continue;
        
        double holeDiamM;
        
        if (c.isOpenHole) {
            // Голый ствол: диаметр = диаметр долота * кавернозность
            holeDiamM = (c.outerDiameter * c.cavernosity) / 1000.0;
        } else {
            // Обсадная колонна: используем внутренний диаметр
            holeDiamM = c.innerDiameter / 1000.0;
        }
        
        double holeArea = M_PI / 4.0 * holeDiamM * holeDiamM;
        double annulusArea = holeArea - toolArea;
        if (annulusArea < 0) annulusArea = 0;
        
        // Определяем, сколько инструмента находится в этой секции
        double toolInSection = std::min(remainingToolLength, sectionLength);
        
        if (toolInSection > 0) {
            // Часть с инструментом
            totalVolume += annulusArea * toolInSection;
            remainingToolLength -= toolInSection;
            
            // Часть без инструмента
            double sectionWithoutTool = sectionLength - toolInSection;
            if (sectionWithoutTool > 0) {
                totalVolume += holeArea * sectionWithoutTool;
            }
        } else {
            // Инструмента нет в этой секции
            totalVolume += holeArea * sectionLength;
        }
    }
    
    return totalVolume;
}

double CirculationCalculator::toolInnerVolume() const
{
    double totalVolume = 0.0;
    for (const auto &item : m_layout) {
        totalVolume += item.volumeInner();
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
    double cum = 0.0;
    // РАСЧЁТ ВЕСА: идем от конца к началу (от забоя к устью), суммируем снизу вверх
    // Элемент с макс. индексом (последний в таблице) = забой, суммируется первым
    for (int i = m_layout.size() - 1; i >= 0; --i) {
        cum += m_layout[i].volumeInner();
        result.prepend(cum);
    }
    return result;
}

QVector<double> CirculationCalculator::cumulativeWeightAir() const
{
    QVector<double> result;
    double cum = 0.0;
    for (int i = m_layout.size() - 1; i >= 0; --i) {
        cum += m_layout[i].weightInAir();
        result.prepend(cum);
    }
    return result;
}

QVector<double> CirculationCalculator::cumulativeWeightFluid() const
{
    QVector<double> result;
    double cum = 0.0;
    for (int i = m_layout.size() - 1; i >= 0; --i) {
        cum += m_layout[i].weightInFluid(m_fluidDensity);
        result.prepend(cum);
    }
    return result;
}
