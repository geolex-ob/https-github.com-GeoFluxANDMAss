#pragma once

#include <QMainWindow>
#include <QTableWidget>
#include <QLineEdit>
#include <QMap>
#include <QComboBox>
#include <QCheckBox>
#include <QPoint>

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
    void closeEvent(QCloseEvent *event) override;

private slots:
    void saveConstruction();
    void loadKNBKLayoutFromSettingsSlot();
    void addKNBKRow();
    void removeKNBKRow();
    void addConstructionRow();
    void removeConstructionRow();
    void calculate();
    void onKNBKComboChanged(int index);
    void openKNBKEditor();
    void onKNBKModeChanged(int index);
    void onKNBKToBottomToggled(bool checked);   // чекбокс «До забоя» в строке

private:
    void setupUI();
    void loadCSV(const QString &filePath, QTableWidget *table);
    void saveCSV(const QString &filePath, QTableWidget *table);
    QString getDataPath(const QString &fileName);
    void saveKNBKLayout();
    bool loadKNBKLayout();
    bool loadKNBKLayoutFromCsv(const QString &filePath);
    void fillKNBKRow(int row, const QStringList &fields);
    QString knbkLayoutSettingsKey() const;
    void setupKNBKCombo(int row);       // создаёт контейнер: чекбокс + комбобокс
    void setupKNBKModeCombo(int row);
    void applyKNBKRowEditability(int row, bool isManual);

    // Галочка/чекбокс "До забоя" (обычный режим): длина = глубина - сумма выше.
    // Хранится в QTableWidgetItem(row,0) в ToBottomRole. В QSettings/CSV
    // маркируется символом "✔" в конце шифра (формат файлов не меняется).
    void toggleToBottom(QTableWidgetItem *cell);
    void updateToBottomLengths();

    // Одиночный (ручной) выбор одного элемента: Qt::UserRole+3, маркер "◉".
    void showKNBKContextMenu(const QPoint &pos);
    void setSingleToBottom(int row);

    // Синхронизация чекбокса в ячейке (row,0) с ролью ToBottomRole.
    void updateToBottomCheckBox(int row);
    void updateAllToBottomCheckBoxes();

    // Обратная промывка: раствор идёт вниз по затрубному, вверх по трубам.
    // Влияет на «Время выхода газа» (газ выходит по трубам, а не по затрубному).
    // Сохраняется в QSettings (Info/reverseCirculation) и в data/info.csv.
    QCheckBox *m_reverseCirculationCheck = nullptr;
    bool isReverseCirculation() const;
    void saveReverseCirculation();
    void loadReverseCirculation();

    // Файл data/info.csv — последние введённые вручную значения раздела
    // «Информация» (производительность, веса, кавернозность, глубина, долото,
    // плотность, режим промывки). Формат: "ключ;значение" по строке, UTF-8, ";".
    // Не пересекается с knbk.csv / knbk_layout.csv / QSettings компоновки КНБК.
    void saveInfoCsv();
    void loadInfoCsv();
    QString infoCsvPath() const;

    QMap<QString, QLineEdit*> m_infoInputs;
    QMap<QString, QLineEdit*> m_resultOutputs;

    QTableWidget *m_knbkTable = nullptr;
    QTableWidget *m_constructionTable = nullptr;

    struct KNBKData {
        QString length;
        QString outerD;
        QString innerD;
        QString mass1m;
        QString vol1m;
    };
    QMap<QString, KNBKData> m_knbkReference;

    bool m_loadingLayout = false;

    void loadKNBKReference();
    void refreshKNBKCombos();
};