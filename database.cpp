#include "database.h"
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDebug>

Database& Database::instance() {
    static Database inst;
    return inst;
}

void Database::loadData(const QString& dataDir) {
    m_dataDir = dataDir;
    QDir().mkpath(m_dataDir);
    loadTools();
    loadCasings();
    loadFluids();
}

void Database::saveAll() {
    saveTools();
    saveCasings();
    saveFluids();
}

QVector<ToolRecord>& Database::getTools() { return m_tools; }
QVector<CasingRecord>& Database::getCasings() { return m_casings; }
QVector<FluidRecord>& Database::getFluids() { return m_fluids; }

QString Database::escapeCsvField(const QString& field) {
    if (field.contains(',') || field.contains('"') || field.contains('\n')) {
        QString escaped = field;
        escaped.replace("\"", "\"\"");
        return "\"" + escaped + "\"";
    }
    return field;
}

QString Database::unescapeCsvField(const QString& field) {
    QString f = field.trimmed();
    if (f.startsWith('"') && f.endsWith('"') && f.size() >= 2) {
        f = f.mid(1, f.size() - 2);
        f.replace("\"\"", "\"");
    }
    return f;
}

// ===== Инструменты =====
void Database::loadTools() {
    m_tools.clear();
    QFile file(m_dataDir + "/ref_tools.csv");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        createDefaults();
        saveTools();
        loadTools();
        return;
    }
    QTextStream in(&file);
    in.setCodec("UTF-8");
    in.readLine();
    while (!in.atEnd()) {
        QStringList fields = in.readLine().split(",");
        if (fields.size() >= 6) {
            ToolRecord r;
            r.id = fields[0].toInt();
            r.name = unescapeCsvField(fields[1]);
            r.outerDiameter = fields[2].toDouble();
            r.innerDiameter = fields[3].toDouble();
            r.massPerMeter = fields[4].toDouble();
            r.volumePerMeter = fields[5].toDouble();
            m_tools.append(r);
        }
    }
}

void Database::saveTools() {
    QFile file(m_dataDir + "/ref_tools.csv");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << "id,name,outer_diameter,inner_diameter,mass_per_meter,volume_per_meter\n";
    for (const auto& t : m_tools) {
        out << t.id << ","
            << escapeCsvField(t.name) << ","
            << t.outerDiameter << ","
            << t.innerDiameter << ","
            << t.massPerMeter << ","
            << t.volumePerMeter << "\n";
    }
}

// ===== Обсадные трубы =====
void Database::loadCasings() {
    m_casings.clear();
    QFile file(m_dataDir + "/ref_casings.csv");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        saveCasings();
        loadCasings();
        return;
    }
    QTextStream in(&file);
    in.setCodec("UTF-8");
    in.readLine();
    while (!in.atEnd()) {
        QStringList fields = in.readLine().split(",");
        if (fields.size() >= 6) {
            CasingRecord r;
            r.id = fields[0].toInt();
            r.name = unescapeCsvField(fields[1]);
            r.outerDiameter = fields[2].toDouble();
            r.wallThickness = fields[3].toDouble();
            r.massPerMeter = fields[4].toDouble();
            r.note = unescapeCsvField(fields[5]);
            m_casings.append(r);
        }
    }
}

void Database::saveCasings() {
    QFile file(m_dataDir + "/ref_casings.csv");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << "id,name,outer_diameter,wall_thickness,mass_per_meter,note\n";
    for (const auto& c : m_casings) {
        out << c.id << ","
            << escapeCsvField(c.name) << ","
            << c.outerDiameter << ","
            << c.wallThickness << ","
            << c.massPerMeter << ","
            << escapeCsvField(c.note) << "\n";
    }
}

// ===== Жидкости =====
void Database::loadFluids() {
    m_fluids.clear();
    QFile file(m_dataDir + "/ref_fluids.csv");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        saveFluids();
        loadFluids();
        return;
    }
    QTextStream in(&file);
    in.setCodec("UTF-8");
    in.readLine();
    while (!in.atEnd()) {
        QStringList fields = in.readLine().split(",");
        if (fields.size() >= 3) {
            FluidRecord r;
            r.id = fields[0].toInt();
            r.name = unescapeCsvField(fields[1]);
            r.density = fields[2].toDouble();
            m_fluids.append(r);
        }
    }
}

void Database::saveFluids() {
    QFile file(m_dataDir + "/ref_fluids.csv");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << "id,name,density\n";
    for (const auto& f : m_fluids) {
        out << f.id << ","
            << escapeCsvField(f.name) << ","
            << f.density << "\n";
    }
}

// ===== Defaults =====
void Database::createDefaults() {
    m_tools.clear();
    m_tools.append({1, "ОБТ-5135(9.35)", 88.9, 70.2, 19.2, 2.8459});
    m_tools.append({2, "НКТ89х6.45", 89, 76.1, 13.4, 1.707});

    m_casings.clear();
    m_casings.append({1, "323.9*11", 323.9, 11, 85.6, "ТМС-FMC/ОТТМ"});
    m_casings.append({2, "245*10", 245, 10, 59.5, "ТМС-FMC"});
    m_casings.append({3, "426*11", 426, 11, 104.4, "Батресс"});
    m_casings.append({4, "245*8.94", 245, 8.94, 53.31, "ОТТГ"});
    m_casings.append({5, "168*8.9", 168, 8.9, 41.25, "VAGT"});

    m_fluids.clear();
    m_fluids.append({1, "Вода", 1.0});
    m_fluids.append({2, "Буровой раствор", 1.05});
    m_fluids.append({3, "Тяжелый раствор", 1.2});
}

// ===== Экспорт =====
bool Database::exportCsv(const QString& filePath, const QString& table) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&file);
    out.setCodec("UTF-8");

    if (table == "tools") {
        out << "id,name,outer_diameter,inner_diameter,mass_per_meter,volume_per_meter\n";
        for (const auto& t : m_tools)
            out << t.id << "," << escapeCsvField(t.name) << ","
                << t.outerDiameter << "," << t.innerDiameter << ","
                << t.massPerMeter << "," << t.volumePerMeter << "\n";
    } else if (table == "casings") {
        out << "id,name,outer_diameter,wall_thickness,mass_per_meter,note\n";
        for (const auto& c : m_casings)
            out << c.id << "," << escapeCsvField(c.name) << ","
                << c.outerDiameter << "," << c.wallThickness << ","
                << c.massPerMeter << "," << escapeCsvField(c.note) << "\n";
    } else if (table == "fluids") {
        out << "id,name,density\n";
        for (const auto& f : m_fluids)
            out << f.id << "," << escapeCsvField(f.name) << "," << f.density << "\n";
    }
    return true;
}

// ===== Импорт =====
bool Database::importCsv(const QString& filePath, const QString& table) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QTextStream in(&file);
    in.setCodec("UTF-8");
    in.readLine();

    if (table == "tools") {
        m_tools.clear();
        int id = 1;
        while (!in.atEnd()) {
            QStringList f = in.readLine().split(",");
            if (f.size() >= 6) {
                ToolRecord r;
                r.id = id++;
                r.name = unescapeCsvField(f[1]);
                r.outerDiameter = f[2].toDouble();
                r.innerDiameter = f[3].toDouble();
                r.massPerMeter = f[4].toDouble();
                r.volumePerMeter = f[5].toDouble();
                m_tools.append(r);
            }
        }
    } else if (table == "casings") {
        m_casings.clear();
        int id = 1;
        while (!in.atEnd()) {
            QStringList f = in.readLine().split(",");
            if (f.size() >= 6) {
                CasingRecord r;
                r.id = id++;
                r.name = unescapeCsvField(f[1]);
                r.outerDiameter = f[2].toDouble();
                r.wallThickness = f[3].toDouble();
                r.massPerMeter = f[4].toDouble();
                r.note = unescapeCsvField(f[5]);
                m_casings.append(r);
            }
        }
    } else if (table == "fluids") {
        m_fluids.clear();
        int id = 1;
        while (!in.atEnd()) {
            QStringList f = in.readLine().split(",");
            if (f.size() >= 3) {
                FluidRecord r;
                r.id = id++;
                r.name = unescapeCsvField(f[1]);
                r.density = f[2].toDouble();
                m_fluids.append(r);
            }
        }
    }
    saveAll();
    return true;
}