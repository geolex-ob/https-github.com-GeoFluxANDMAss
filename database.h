#ifndef DATABASE_H
#define DATABASE_H

#include <QString>
#include <QVector>

struct ToolRecord {
    int id = 0;
    QString name;
    double outerDiameter = 0;
    double innerDiameter = 0;
    double massPerMeter = 0;
    double volumePerMeter = 0;
    double length = 0;
};

struct CasingRecord {
    int id = 0;
    QString name;
    double outerDiameter = 0;
    double wallThickness = 0;
    double massPerMeter = 0;
    QString note;
    double topDepth = 0;
    double bottomDepth = 0;
};

struct FluidRecord {
    int id = 0;
    QString name;
    double density = 0;
};

class Database {
public:
    static Database& instance();

    void loadData(const QString& dataDir);
    void saveAll();

    QVector<ToolRecord>& getTools();
    QVector<CasingRecord>& getCasings();
    QVector<FluidRecord>& getFluids();

    QString dataDirectory() const { return m_dataDir; }

    bool exportCsv(const QString& filePath, const QString& table);
    bool importCsv(const QString& filePath, const QString& table);

    void saveTools();
    void saveCasings();
    void saveFluids();

private:
    Database() = default;

    QVector<ToolRecord> m_tools;
    QVector<CasingRecord> m_casings;
    QVector<FluidRecord> m_fluids;
    QString m_dataDir;

    void loadTools();
    void loadCasings();
    void loadFluids();

    void createDefaults();

    static QString escapeCsvField(const QString& field);
    static QString unescapeCsvField(const QString& field);
};

#endif // DATABASE_H