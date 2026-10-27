#include "calculations.h"
#include <cmath>

static const double PI = 3.14159265358979323846;

static double cylinderVolume(double diameterMm, double lengthM) {
    return PI * diameterMm * diameterMm / 4.0 * lengthM / 1e6;
}

CalculationResults Calculations::calculate(
    const QVector<ToolRecord>& tools,
    const QVector<CasingRecord>& casings,
    double tripDepth,
    double fluidDensity,
    double productivity,
    double blockWeight,
    double squareWeight,
    double bitDiameter,
    double cavernosityCoeff)  // НОВЫЙ ПАРАМЕТР
{
    CalculationResults r;

    // ===== 1. Параметры бурильных труб (КНБК) =====
    double totalMass = 0;
    double totalMetalVolume = 0;
    double totalOuterVolume = 0;
    double tubingVolume = 0;

    for (const auto& t : tools) {
        totalMass += t.massPerMeter;
        totalMetalVolume += t.volumePerMeter;
        totalOuterVolume += cylinderVolume(t.outerDiameter, t.length);
        tubingVolume     += cylinderVolume(t.innerDiameter, t.length);
    }

    r.weightInAir   = totalMass;
    r.toolVolume    = totalMetalVolume;
    r.tubingVolume  = tubingVolume;

    // ===== 2. Вес в жидкости (закон Архимеда) =====
    double buoyancy = totalMetalVolume * fluidDensity;
    r.weightInFluidWithBlock  = totalMass - buoyancy + blockWeight;
    r.weightInFluidWithSquare = totalMass - buoyancy + squareWeight;

    // ===== 3. Объём скважины =====
    double wellVolume = 0;

    if (!casings.isEmpty()) {
        double clippedVolume = 0;
        double maxDepthReached = 0;

        for (const auto& c : casings) {
            if (c.bottomDepth <= 0) continue;
            double top = c.topDepth;
            double bottom = c.bottomDepth;

            if (top >= tripDepth) continue;
            if (bottom > tripDepth) bottom = tripDepth;

            double len = bottom - top;
            if (len <= 0) continue;

            clippedVolume += cylinderVolume(c.outerDiameter, len);
            if (bottom > maxDepthReached) maxDepthReached = bottom;
        }

        // Открытый ствол с учётом кавернозности
        if (maxDepthReached < tripDepth) {
            double openHoleLength = tripDepth - maxDepthReached;
            // Реальный диаметр ствола = диаметр долота × коэффициент кавернозности
            double realOpenHoleDiameter = bitDiameter * cavernosityCoeff;
            clippedVolume += cylinderVolume(realOpenHoleDiameter, openHoleLength);
        }

        wellVolume = clippedVolume;
    }

    if (wellVolume <= 0) {
        // Fallback: конструкция не задана
        double realDiameter = bitDiameter * cavernosityCoeff;
        wellVolume = cylinderVolume(realDiameter, tripDepth);
    }

    r.wellVolumeToDepth = wellVolume;

    // ===== 4. Затрубное пространство =====
    r.annularVolume = wellVolume - totalOuterVolume;
    if (r.annularVolume < 0) r.annularVolume = 0;

    // ===== 5. Время выхода газа =====
    if (productivity > 0 && r.annularVolume > 0) {
        double timeSec = r.annularVolume * 1000.0 / productivity;
        r.gasExitTime = timeSec / 60.0;
    } else {
        r.gasExitTime = 0;
    }

    r.fullCycleTime = r.gasExitTime * 1.5;

    return r;
}