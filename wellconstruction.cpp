#include "wellconstruction.h"
#include <QFile>
#include <QJsonDocument>
#include <cmath>

QJsonObject Casing::toJson() const
{
    QJsonObject obj;
    obj["name"] = name;
    obj["startDepth"] = startDepth;
    obj["endDepth"] = endDepth;
    obj["outerDiameter"] = outerDiameter;
    obj["innerDiameter"] = innerDiameter;
    obj["isOpenHole"] = isOpenHole;
    obj["cavernosity"] = cavernosity;
    return obj;
}

Casing Casing::fromJson(const QJsonObject &json)
{
    Casing c;
    c.name = json["name"].toString();
    c.startDepth = json["startDepth"].toDouble();
    c.endDepth = json["endDepth"].toDouble();
    c.outerDiameter = json["outerDiameter"].toDouble();
    c.innerDiameter = json["innerDiameter"].toDouble();
    c.isOpenHole = json["isOpenHole"].toBool();
    c.cavernosity = json["cavernosity"].toDouble(1.0);
    return c;
}

WellConstruction::WellConstruction()
{
}

void WellConstruction::addCasing(const Casing &casing)
{
    m_casings.append(casing);
}

void WellConstruction::removeCasing(int index)
{
    if (index >= 0 && index < m_casings.size())
        m_casings.removeAt(index);
}

void WellConstruction::clear()
{
    m_casings.clear();
}

Casing WellConstruction::casingAt(int index) const
{
    if (index >= 0 && index < m_casings.size())
        return m_casings.at(index);
    return Casing();
}

double WellConstruction::totalInnerVolume() const
{
    double totalVolume = 0.0;
    for (const auto &c : m_casings) {
        double length = c.endDepth - c.startDepth;
        if (length <= 0) continue;

        double innerDiamM;
        
        if (c.isOpenHole) {
            // Для голого ствола внутренний диаметр = наружный * кавернозность
            innerDiamM = (c.outerDiameter * c.cavernosity) / 1000.0;
        } else {
            // Для обсадной колонны используем внутренний диаметр
            innerDiamM = c.innerDiameter / 1000.0;
        }
        
        double volume = M_PI / 4.0 * innerDiamM * innerDiamM * length;
        totalVolume += volume;
    }
    return totalVolume;
}

double WellConstruction::totalAnnulusVolume(double toolOuterDiameter) const
{
    double totalVolume = 0.0;
    
    for (const auto &c : m_casings) {
        double length = c.endDepth - c.startDepth;
        if (length <= 0) continue;

        double holeDiamM;
        
        if (c.isOpenHole) {
            // Для голого ствола диаметр = диаметр долота * кавернозность
            holeDiamM = (c.outerDiameter * c.cavernosity) / 1000.0;
        } else {
            // Для обсадной колонны диаметр = внутренний диаметр колонны
            holeDiamM = c.innerDiameter / 1000.0;
        }
        
        double toolDiamM = toolOuterDiameter / 1000.0;
        
        if (holeDiamM <= toolDiamM) continue;
        
        double holeArea = M_PI / 4.0 * holeDiamM * holeDiamM;
        double toolArea = M_PI / 4.0 * toolDiamM * toolDiamM;
        double annulusArea = holeArea - toolArea;
        
        totalVolume += annulusArea * length;
    }
    return totalVolume;
}

QJsonObject WellConstruction::toJson() const
{
    QJsonArray arr;
    for (const auto &c : m_casings)
        arr.append(c.toJson());

    QJsonObject obj;
    obj["casings"] = arr;
    return obj;
}

void WellConstruction::fromJson(const QJsonObject &json)
{
    m_casings.clear();
    QJsonArray arr = json["casings"].toArray();
    for (const auto &val : arr)
        m_casings.append(Casing::fromJson(val.toObject()));
}

void WellConstruction::saveToFile(const QString &filename)
{
    QFile file(filename);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(toJson()).toJson());
        file.close();
    }
}

void WellConstruction::loadFromFile(const QString &filename)
{
    QFile file(filename);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();
        fromJson(QJsonDocument::fromJson(data).object());
    }
}
