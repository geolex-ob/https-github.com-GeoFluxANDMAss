#ifndef CALCULATIONS_H
#define CALCULATIONS_H

#include <QVector>
#include "database.h"

struct CalculationResults {
    double weightInFluidWithBlock = 0;
    double weightInFluidWithSquare = 0;
    double weightInAir = 0;
    double toolVolume = 0;
    double wellVolumeToDepth = 0;
    double tubingVolume = 0;
    double annularVolume = 0;
    double gasExitTime = 0;
    double fullCycleTime = 0;
};

class Calculations {
public:
    static CalculationResults calculate(
        const QVector<ToolRecord>& tools,
        const QVector<CasingRecord>& casings,
        double tripDepth,
        double fluidDensity,
        double productivity,
        double blockWeight,
        double squareWeight,
        double bitDiameter,
        double cavernosityCoeff);  // НОВЫЙ ПАРАМЕТР
};

#endif // CALCULATIONS_H