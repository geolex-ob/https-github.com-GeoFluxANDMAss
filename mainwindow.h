#pragma once

#include <QMainWindow>
#include <QTableWidget>
#include <QLineEdit>
#include <QMap>
#include <QComboBox>

// Единый формат файла knbk.csv (колонки 0..5 одинаковы для справочника и таблицы КНБК):
// Шифр;Длина, м;Наружный d, мм;Внутренний d, мм;Масса 1 м, кг;Объем 1 м, л;Режим;...
extern const QStringList KNBK_CSV_HEADER;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    // При закрытии окна автоматически сохраняем последнюю компоновку КНБК
    void closeEvent(QCloseEvent *event) override;

private slots:
    void saveConstruction();
    // Явное сохранение/загрузка компоновки КНБК (Файл → Сохранить/Загрузить компоновку КНБК)
    void loadKNBKLayoutFromSettingsSlot(); // слот меню "Загрузить компоновку КНБК"
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
    // Компоновка КНБК (рабочая таблица) хранится отдельно от справочника:
    // QSettings + автоматическое сохранение при закрытии окна
    void saveKNBKLayout();
    bool loadKNBKLayout();               // из QSettings; false — если настроек нет
    bool loadKNBKLayoutFromCsv(const QString &filePath); // миграция из CSV
    void fillKNBKRow(int row, const QStringList &fields);
    QString knbkLayoutSettingsKey() const;
    void setupKNBKCombo(int row);
    void setupKNBKModeCombo(int row); // Метод для создания списка режимов
    void applyKNBKRowEditability(int row, bool isManual); // "Масса/Объем 1 м": ввод или только чтение

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

    bool m_loadingLayout = false; // защита от автосохранения во время загрузки компоновки
    
    void loadKNBKReference();
    void refreshKNBKCombos();
};