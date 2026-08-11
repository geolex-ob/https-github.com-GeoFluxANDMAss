#ifndef TOOL_H
#define TOOL_H

#include <QString>
#include <QJsonObject>

class Tool
{
public:
    Tool();
    Tool(const QString &name, double outerDiameter, double innerDiameter,
         double weightPerMeter, double volumePerMeter, double density = 7850.0);

    // Геттеры
    QString name() const { return m_name; }
    double outerDiameter() const { return m_outerDiameter; }
    double innerDiameter() const { return m_innerDiameter; }
    double weightPerMeter() const { return m_weightPerMeter; }
    double volumePerMeter() const { return m_volumePerMeter; }
    double density() const { return m_density; }

    // Сеттеры
    void setName(const QString &name) { m_name = name; }
    void setOuterDiameter(double d) { m_outerDiameter = d; }
    void setInnerDiameter(double d) { m_innerDiameter = d; }
    void setWeightPerMeter(double w) { m_weightPerMeter = w; }
    void setVolumePerMeter(double v) { m_volumePerMeter = v; }
    void setDensity(double d) { m_density = d; }

    // Расчет объема по весу погонного метра и плотности (в м³/м)
    double calculateVolumeFromWeight() const;

    // Расчет веса погонного метра по размерам и плотности
    double calculateWeightFromDimensions() const;

    // Сериализация
    QJsonObject toJson() const;
    static Tool fromJson(const QJsonObject &json);

private:
    QString m_name;
    double m_outerDiameter;   // мм
    double m_innerDiameter;   // мм
    double m_weightPerMeter;  // кг/м
    double m_volumePerMeter;  // м³/м
    double m_density;         // кг/м³
};

#endif // TOOL_H
