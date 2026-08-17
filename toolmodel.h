#ifndef TOOLMODEL_H
#define TOOLMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include "tool.h"

class ToolModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        Name = 0,
        OuterDiameter,
        InnerDiameter,
        WeightPerMeter,
        VolumePerMeter,
        Density,
        GeometricFactor,
        CalcMethod,
        ColumnCount
    };

    explicit ToolModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    bool setData(const QModelIndex &index, const QVariant &value,
                 int role = Qt::EditRole) override;

    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void addTool(const Tool &tool);
    void removeTool(int row);
    Tool toolAt(int row) const;

    QVector<Tool> allTools() const { return m_tools; }
    void setTools(const QVector<Tool> &tools);

    void saveToFile(const QString &filename);
    void loadFromFile(const QString &filename);

private:
    QVector<Tool> m_tools;
};

#endif // TOOLMODEL_H
