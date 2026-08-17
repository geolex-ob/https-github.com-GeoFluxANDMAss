#include "tool.h"
#include <cmath>

static long double toolPi()
{
    return std::acos(-1.0L);
}

Tool::Tool()
    : m_outerDiameter(0.0)
    , m_innerDiameter(0.0)
    , m_weightPerMeter(0.0)
    , m_volumePerMeter(0.0)
    , m_density(7850.0)
    , m_calcMethod(ByDensity)
    , m_geometricFactor(1.0)
{
}

Tool::Tool(const QString &name,
           double outerDiameter,
           double innerDiameter,
           double weightPerMeter,
           double volumePerMeter,
           double density)
    : m_name(name)
    , m_outerDiameter(outerDiameter)
    , m_innerDiameter(innerDiameter)
    , m_weightPerMeter(weightPerMeter)
    , m_volumePerMeter(volumePerMeter)
    , m_density(density)
    , m_calcMethod(ByDensity)
    , m_geometricFactor(1.0)
{
}

double Tool::calculateVolumeFromWeight() const
{
    // V = m / ρ  (объем металла на метр)
    if (m_density <= 0.0) return 0.0;
    return m_weightPerMeter / m_density;
}

double Tool::calculateInnerVolumePerMeter() const
{
    // Внутренний объем (емкость) по внутреннему диаметру
    if (m_innerDiameter <= 0.0) return 0.0;
    long double d = static_cast<long double>(m_innerDiameter);
    return static_cast<double>(toolPi() / 4.0L * d * d * 1e-6L);
}

double Tool::calculateOuterVolumePerMeter() const
{
    // Наружный объем (вытеснение) по внешнему диаметру
    if (m_outerDiameter <= 0.0) return 0.0;
    long double D = static_cast<long double>(m_outerDiameter);
    return static_cast<double>(toolPi() / 4.0L * D * D * 1e-6L);
}

double Tool::calculateVolumeFromDimensions() const
{
    // Объем металла: внешний минус внутренний, с коэффициентом геометрии
    long double D = static_cast<long double>(m_outerDiameter);
    long double d = static_cast<long double>(m_innerDiameter);

    if (D <= 0.0L) return 0.0;
    if (d < 0.0L) d = 0.0L;
    if (d >= D) return 0.0;

    long double k = static_cast<long double>(m_geometricFactor);
    if (k <= 0.0L) k = 1.0L;

    long double v = toolPi() / 4.0L * (D * D - d * d) * 1e-6L * k;
    return static_cast<double>(v);
}

double Tool::calculateWeightFromDimensions() const
{
    // Вес погонного метра по геометрическим размерам
    long double D = static_cast<long double>(m_outerDiameter);
    long double d = static_cast<long double>(m_innerDiameter);
    long double rho = static_cast<long double>(m_density);

    if (D <= 0.0L || rho <= 0.0L) return 0.0;
    if (d < 0.0L) d = 0.0L;
    if (d >= D) return 0.0;

    long double k = static_cast<long double>(m_geometricFactor);
    if (k <= 0.0L) k = 1.0L;

    long double v = toolPi() / 4.0L * (D * D - d * d) * 1e-6L * k;
    return static_cast<double>(v * rho);
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
    obj["calcMethod"] = static_cast<int>(m_calcMethod);
    obj["geometricFactor"] = m_geometricFactor;
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
    tool.m_calcMethod = static_cast<VolumeCalcMethod>(json["calcMethod"].toInt(0));
    tool.m_geometricFactor = json["geometricFactor"].toDouble(1.0);
    if (tool.m_geometricFactor <= 0.0) tool.m_geometricFactor = 1.0;
    return tool;
}
