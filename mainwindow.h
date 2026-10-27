#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QTableWidget>
#include <QLabel>
#include <QComboBox>
#include <QFileInfo>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void calculate();
    void openToolsReference();
    void openCasingsReference();
    void openFluidsReference();
    void addToolRow();
    void deleteToolRow();
    void addCasingRow();
    void deleteCasingRow();
    void showAbout();
    void exportData();
    void importData();
    void onToolCellChanged(int row, int column);  // НОВОЕ

private:
    void setupUI();
    void setupMenuBar();
    void loadToolCombo(QComboBox* combo);
    void loadCasingCombo(QComboBox* combo);
    void refreshToolTable();
    void refreshCasingTable();
    void recalculateRow(int row);  // НОВОЕ

    QLineEdit *editTripNumber, *editBatchNumber, *editWellNumber;
    QLineEdit *editField, *editTripDepth, *editFluidDensity;
    QLineEdit *editProductivity, *editBlockWeight, *editSquareWeight;
    QLineEdit *editCavernosity, *editBitDiameter;

    QTableWidget *casingTable;
    QTableWidget *toolTable;

    QLabel *labelWeightFluidBlock, *labelWeightFluidSquare, *labelWeightAir;
    QLabel *labelToolVolume, *labelWellVolume, *labelTubingVolume;
    QLabel *labelAnnularVolume, *labelGasTime, *labelCycleTime, *labelDateTime;

    bool m_blockCellChanged;  // НОВОЕ: флаг защиты от рекурсии
};

#endif // MAINWINDOW_H