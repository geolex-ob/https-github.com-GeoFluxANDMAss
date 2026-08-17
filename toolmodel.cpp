#include "toolmodel.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QColor>

ToolModel::ToolModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int ToolModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_tools.size();
}

int ToolModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant ToolModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_tools.size())
        return QVariant();

    const Tool &tool = m_tools.at(index.row());

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case Name: return tool.name();
        case OuterDiameter: return tool.outerDiameter();
        case InnerDiameter: return tool.innerDiameter();
        case WeightPerMeter: return tool.weightPerMeter();
        case VolumePerMeter: return tool.volumePerMeter();
        case Density: return tool.density();
        case CalcMethod:
            return (tool.volumeCalcMethod() == Tool::ByDimensions) 
                   ? "По размерам" : "По плотности";
        }
    }

    if (role == Qt::UserRole && index.column() == CalcMethod) {
        return static_cast<int>(tool.volumeCalcMethod());
    }

    if (role == Qt::TextAlignmentRole)
        return Qt::AlignCenter;

    return QVariant();
}

QVariant ToolModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case Name: return "Название";
        case OuterDiameter: return "D наруж, мм";
        case InnerDiameter: return "D внутр, мм";
        case WeightPerMeter: return "Вес, кг/м";
        case VolumePerMeter: return "Объем, м³/м";
        case Density: return "Плотность, кг/м³";
        case CalcMethod: return "Расчет объема";
        }
    }
    return QVariant();
}

bool ToolModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() >= m_tools.size() || role != Qt::EditRole)
        return false;

    Tool &tool = m_tools[index.row()];
    bool ok;
    double d;

    switch (index.column()) {
    case Name:
        tool.setName(value.toString());
        break;
    case OuterDiameter:
        d = value.toDouble(&ok);
        if (!ok || d < 0) return false;
        tool.setOuterDiameter(d);
        break;
    case InnerDiameter:
        d = value.toDouble(&ok);
        if (!ok || d < 0) return false;
        tool.setInnerDiameter(d);
        break;
    case WeightPerMeter:
        d = value.toDouble(&ok);
        if (!ok || d < 0) return false;
        tool.setWeightPerMeter(d);
        break;
    case VolumePerMeter:
        d = value.toDouble(&ok);
        if (!ok || d < 0) return false;
        tool.setVolumePerMeter(d);
        break;
    case Density:
        d = value.toDouble(&ok);
        if (!ok || d < 0) return false;
        tool.setDensity(d);
        break;
    case CalcMethod:
        tool.setVolumeCalcMethod(static_cast<Tool::VolumeCalcMethod>(value.toInt()));
        break;
    default:
        return false;
    }

    emit dataChanged(index, index, {role});

    // Автоматический пересчет объема при изменении зависимых параметров или метода расчета
    if (index.column() != VolumePerMeter) {
        double newVol = 0.0;
        if (tool.volumeCalcMethod() == Tool::ByDensity) {
            if (tool.density() > 0.0) newVol = tool.calculateVolumeFromWeight();
        } else {
            newVol = tool.calculateVolumeFromDimensions();
        }

        if (tool.volumePerMeter() != newVol) {
            tool.setVolumePerMeter(newVol);
            QModelIndex volIdx = this->index(index.row(), VolumePerMeter);
            emit dataChanged(volIdx, volIdx, {Qt::DisplayRole, Qt::EditRole});
        }
    }

    return true;
}

Qt::ItemFlags ToolModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

void ToolModel::addTool(const Tool &tool)
{
    beginInsertRows(QModelIndex(), m_tools.size(), m_tools.size());
    m_tools.append(tool);
    endInsertRows();
}

void ToolModel::removeTool(int row)
{
    if (row < 0 || row >= m_tools.size()) return;
    beginRemoveRows(QModelIndex(), row, row);
    m_tools.removeAt(row);
    endRemoveRows();
}

Tool ToolModel::toolAt(int row) const
{
    if (row < 0 || row >= m_tools.size()) return Tool();
    return m_tools.at(row);
}

void ToolModel::setTools(const QVector<Tool> &tools)
{
    beginResetModel();
    m_tools = tools;
    endResetModel();
}

void ToolModel::saveToFile(const QString &filename)
{
    QJsonArray arr;
    for (const auto &t : m_tools)
        arr.append(t.toJson());

    QFile file(filename);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(arr).toJson());
        file.close();
    }
}

void ToolModel::loadFromFile(const QString &filename)
{
    QFile file(filename);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonArray arr = doc.array();

        QVector<Tool> tools;
        for (const auto &val : arr)
            tools.append(Tool::fromJson(val.toObject()));
        setTools(tools);
    }
}
