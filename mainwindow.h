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

    QTableView *m_toolTable;
    ToolModel *m_toolModel;
    QPushButton *m_addToolBtn;
    QPushButton *m_removeToolBtn;
    QPushButton *m_calcVolumeBtn;

    QTableView *m_layoutTable;
    QStandardItemModel *m_layoutModel;
    QPushButton *m_addToLayoutBtn;
    QPushButton *m_removeFromLayoutBtn;
    QPushButton *m_moveLayoutUpBtn;
    QPushButton *m_moveLayoutDownBtn;
    QDoubleSpinBox *m_fluidDensitySpin;
    QLabel *m_totalWeightAirLabel;
    QLabel *m_totalWeightFluidLabel;
    QLabel *m_totalVolumeLabel;

    QTableView *m_wellTable;
    QStandardItemModel *m_wellModel;
    QPushButton *m_addCasingBtn;
    QPushButton *m_removeCasingBtn;
    QPushButton *m_moveCasingUpBtn;
    QPushButton *m_moveCasingDownBtn;
    QDoubleSpinBox *m_flowRateSpin;
    QLabel *m_circulationTimeLabel;
    QLabel *m_bottomUpTimeLabel;
    QLabel *m_surfaceToBottomTimeLabel;
    QLabel *m_wellVolumeLabel;
    WellVisualization *m_visualization;

    QTabWidget *m_tabWidget;

    CirculationCalculator m_calculator;
    WellConstruction m_wellConstruction;
    QVector<LayoutItem> m_layoutItems;

    QString m_dataPath;
};

#endif // MAINWINDOW_H
