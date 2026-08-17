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
    // ✅ ИСПРАВЛЕНО: время полного цикла = объём жидкости в системе / расход
    // НЕ включаем объём стали инструмента
    double totalVolume = toolInnerVolume() + annulusVolume();
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
    // ✅ ИСПРАВЛЕНО: Улучшенный расчёт с учётом разных диаметров инструмента
    double totalVolume = 0.0;
    
    // Вычисляем общую длину инструмента
    double totalToolLength = 0.0;
    for (const auto &item : m_layout) {
        totalToolLength += item.length * item.quantity;
    }
    
    // Создаём список элементов компоновки с их реальными диаметрами
    struct ToolSection {
        double outerDiameter;
        double length;
    };
    
    QVector<ToolSection> toolSections;
    for (const auto &item : m_layout) {
        ToolSection ts;
        ts.outerDiameter = item.tool.outerDiameter();
        ts.length = item.length * item.quantity;
        toolSections.append(ts);
    }
    
    double remainingToolLength = totalToolLength;
    int currentToolIdx = 0;
    double currentToolRemaining = (currentToolIdx < toolSections.size()) 
        ? toolSections[currentToolIdx].length : 0.0;
    
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
        
        double sectionRemaining = sectionLength;
        
        while (sectionRemaining > 0 && remainingToolLength > 0) {
            // Берём текущий элемент компоновки
            if (currentToolRemaining <= 0) {
                currentToolIdx++;
                if (currentToolIdx >= toolSections.size()) {
                    break;
                }
                currentToolRemaining = toolSections[currentToolIdx].length;
            }
            
            double toolOD_M = toolSections[currentToolIdx].outerDiameter / 1000.0;
            double toolArea = M_PI / 4.0 * toolOD_M * toolOD_M;
            double annulusArea = holeArea - toolArea;
            if (annulusArea < 0) annulusArea = 0;
            
            // Определяем, сколько инструмента помещается в этой секции
            double toolInSection = std::min(currentToolRemaining, sectionRemaining);
            
            if (toolInSection > 0) {
                totalVolume += annulusArea * toolInSection;
                currentToolRemaining -= toolInSection;
                remainingToolLength -= toolInSection;
                sectionRemaining -= toolInSection;
            }
        }
        
        // Если секция длиннее оставшегося инструмента
        if (sectionRemaining > 0) {
            totalVolume += holeArea * sectionRemaining;
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
