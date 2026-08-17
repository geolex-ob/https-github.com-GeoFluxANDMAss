#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QTableView>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QSplitter>
#include <QSettings>
#include <QStandardItemModel>
#include <QScrollArea>
#include <QDir>
#include <QVector>

#include "toolmodel.h"
#include "circulation.h"
#include "wellconstruction.h"
#include "wellvisualization.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onAddTool();
    void onRemoveTool();
    void onCalculateVolume();
    void onTableCellChanged();
    void onAddToLayout();
    void onRemoveFromLayout();
    void onMoveLayoutUp();
    void onMoveLayoutDown();
    void onAddCasing();
    void onRemoveCasing();
    void onMoveCasingUp();
    void onMoveCasingDown();
    void onCalculateCirculation();
    void onSaveLayout();
    void onLoadLayout();
    void onSaveWell();
    void onLoadWell();
    void onAbout();
    void autoSaveAll();

private:
    void setupUI();
    void createToolTab();
    void createLayoutTab();
    void createWellTab();
    void createMenuBar();
    void applySettings();
    void saveSettings();
    void updateVisualization();
    void refreshLayoutTable();
    QVector<double> cumulativeLayoutLength() const;
    void saveLayoutToCSV(const QString &filename);
    void loadLayoutFromCSV(const QString &filename, bool autoSave = true);
    void saveWellToCSV(const QString &filename);
    void loadWellFromCSV(const QString &filename, bool autoSave = true);
    void recalculateCirculation(bool autoSave);
    QString dataPath() const;

    bool m_updatingLayout = false;
    bool m_updatingWell = false;
    bool m_visualFitScheduled = false;

    QTableView *m_toolTable = nullptr;
    ToolModel *m_toolModel = nullptr;
    QPushButton *m_addToolBtn = nullptr;
    QPushButton *m_removeToolBtn = nullptr;
    QPushButton *m_calcVolumeBtn = nullptr;

    QTableView *m_layoutTable = nullptr;
    QStandardItemModel *m_layoutModel = nullptr;
    QPushButton *m_addToLayoutBtn = nullptr;
    QPushButton *m_removeFromLayoutBtn = nullptr;
    QPushButton *m_moveLayoutUpBtn = nullptr;
    QPushButton *m_moveLayoutDownBtn = nullptr;
    QDoubleSpinBox *m_fluidDensitySpin = nullptr;
    QLabel *m_totalWeightAirLabel = nullptr;
    QLabel *m_totalWeightFluidLabel = nullptr;
    QLabel *m_totalVolumeLabel = nullptr;

    QTableView *m_wellTable = nullptr;
    QStandardItemModel *m_wellModel = nullptr;
    QPushButton *m_addCasingBtn = nullptr;
    QPushButton *m_removeCasingBtn = nullptr;
    QPushButton *m_moveCasingUpBtn = nullptr;
    QPushButton *m_moveCasingDownBtn = nullptr;
    QDoubleSpinBox *m_flowRateSpin = nullptr;
    QLabel *m_circulationTimeLabel = nullptr;
    QLabel *m_bottomUpTimeLabel = nullptr;
    QLabel *m_surfaceToBottomTimeLabel = nullptr;
    QLabel *m_wellVolumeLabel = nullptr;

    WellVisualization *m_visualization = nullptr;
    QTabWidget *m_tabWidget = nullptr;
    CirculationCalculator m_calculator;
    WellConstruction m_wellConstruction;
    QVector<LayoutItem> m_layoutItems;
    QString m_dataPath;
};

#endif // MAINWINDOW_H
