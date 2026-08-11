#include "tool.h"
#include <cmath>

Tool::Tool()
    : m_outerDiameter(0.0)
    , m_innerDiameter(0.0)
    , m_weightPerMeter(0.0)
    , m_volumePerMeter(0.0)
    , m_density(7850.0)
{
}

Tool::Tool(const QString &name, double outerDiameter, double innerDiameter,
           double weightPerMeter, double volumePerMeter, double density)
    : m_name(name)
    , m_outerDiameter(outerDiameter)
    , m_innerDiameter(innerDiameter)
    , m_weightPerMeter(weightPerMeter)
    , m_volumePerMeter(volumePerMeter)
    , m_density(density)
{
}

double Tool::calculateVolumeFromWeight() const
{
    // V = m / ρ  (объем на метр = вес погонного метра / плотность)
    // Переводим кг в м³: (кг/м) / (кг/м³) = м³/м
    if (m_density <= 0.0) return 0.0;
    return m_weightPerMeter / m_density;
}

double Tool::calculateWeightFromDimensions() const
{
    // Площадь кольца: π/4 * (D² - d²) в мм², переводим в м²
    double outerM = m_outerDiameter / 1000.0;
    double innerM = m_innerDiameter / 1000.0;
    double area = M_PI / 4.0 * (outerM * outerM - innerM * innerM);
    return area * m_density; // кг/м
}

QJsonObject Tool::toJson() const
{
    QJsonObject obj;
    obj["name"] = m_name;
    obj["outerDiameter"] = m_outerDiameter;
    obj["innerDiameter"] = m_innerDiameter;
    obj["weightPerMeter"] = m_weightPerMeter;
    obj["volumePerMeter"] = m_volumePerMeter;
    obj["density"] = m_density;
    return obj;
}

Tool Tool::fromJson(const QJsonObject &json)
{
    Tool tool;
    tool.m_name = json["name"].toString();
    tool.m_outerDiameter = json["outerDiameter"].toDouble();
    tool.m_innerDiameter = json["innerDiameter"].toDouble();
    tool.m_weightPerMeter = json["weightPerMeter"].toDouble();
    tool.m_volumePerMeter = json["volumePerMeter"].toDouble();
    tool.m_density = json["density"].toDouble(7850.0);
    return tool;
}
