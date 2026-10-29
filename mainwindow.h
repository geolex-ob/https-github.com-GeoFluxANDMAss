#pragma once

#include <QMainWindow>
#include <QTableWidget>
#include <QLineEdit>
#include <QMap>
#include <QComboBox>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void saveConstruction();
    void addKNBKRow();
    void removeKNBKRow();
    void addConstructionRow();
    void removeConstructionRow();
    void calculate();
    void onKNBKComboChanged(int index);
    void openKNBKEditor();
    void onKNBKModeChanged(int index); // Новый слот для изменения режима

private:
    void setupUI();
    void loadCSV(const QString &filePath, QTableWidget *table);
    void saveCSV(const QString &filePath, QTableWidget *table);
    QString getDataPath(const QString &fileName);
    void setupKNBKCombo(int row);
    void setupKNBKModeCombo(int row); // Метод для создания списка режимов

    QMap<QString, QLineEdit*> m_infoInputs;
    QMap<QString, QLineEdit*> m_resultOutputs;
    
    QTableWidget *m_knbkTable = nullptr;
    QTableWidget *m_constructionTable = nullptr;

    struct KNBKData {
        QString length;
        QString outerD;
        QString innerD;
        QString mass1m;
        QString vol1m; // Объем 1 м из справочника
    };
    QMap<QString, KNBKData> m_knbkReference;
    
    void loadKNBKReference();
    void refreshKNBKCombos();
};