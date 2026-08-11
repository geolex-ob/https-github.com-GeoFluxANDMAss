#ifndef WELLCONSTRUCTION_H
#define WELLCONSTRUCTION_H

#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>

// Обсадная колонна или голый ствол
struct Casing {
    QString name;
    double startDepth;     // м
    double endDepth;       // м
    double outerDiameter;  // мм (для голого ствола - диаметр долота)
    double innerDiameter;  // мм (для голого ствола игнорируется)
    bool isOpenHole = false;  // голый ствол
    double cavernosity = 1.0; // коэффициент кавернозности (только для голого ствола)

    QJsonObject toJson() const;
    static Casing fromJson(const QJsonObject &json);
};

class WellConstruction
{
public:
    WellConstruction();

    void addCasing(const Casing &casing);
    void removeCasing(int index);
    void clear();

    QVector<Casing> casings() const { return m_casings; }
    Casing casingAt(int index) const;

    // Расчет полного внутреннего объема скважины (с учетом всех колонн)
    double totalInnerVolume() const;

    // Расчет полного объема затрубного пространства
    double totalAnnulusVolume(double toolOuterDiameter) const;

    // Сериализация
    QJsonObject toJson() const;
    void fromJson(const QJsonObject &json);

    void saveToFile(const QString &filename);
    void loadFromFile(const QString &filename);

private:
    QVector<Casing> m_casings;
};

#endif // WELLCONSTRUCTION_H
