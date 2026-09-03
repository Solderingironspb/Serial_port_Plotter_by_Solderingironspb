#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include "serialthreaded.h"
#include <QMainWindow>
#include <QtSerialPort/QtSerialPort>
#include <QSerialPortInfo>
#include "about.h"
#include "qcustomplot/qcustomplot.h"
#include <QSettings>
#include <QThread>
#include <QFileDialog>
#include <QMessageBox>
#include <QDataStream>
#include <QProcess>

#define CUSTOM_LINE_COLORS   14
#define GCP_CUSTOM_LINE_COLORS 4

namespace Ui {
class MainWindow;
}

// Глобальные переменные
extern bool flag_Autoscale;
extern bool flag_Graph_moove;
extern bool flag_automatic_cnt_channels;
extern uint32_t x_scale_value;
extern uint32_t Data_count;
extern uint64_t Horizontal_scroll_value;
extern bool flag_only_graph;
extern uint32_t Width_pen_graph;
extern QString ComPortName;
extern bool Debug_visible;
extern uint16_t Scan_rate;
extern bool Process_RTU_WINAPI_state;
extern uint8_t RTU_WINAPI_STATE_PROCESS;

#define RTU_WINAPI_STOPPED 0
#define RTU_WINAPI_RUN 1
#define RTU_WINAPI_PAUSED 2

/*===========================Worker - работа с процессом RTU_WINAPI=================================*/
class Worker: public QObject {
    Q_OBJECT
public:
    explicit Worker(QObject *parent = nullptr);
    ~Worker();

    void stopProcessSafely();

public slots:
    void startProcess(const QString &program, const QStringList &arguments);
    void stopProcess();
    void clearStats();

signals:
    void dataReceived(float ch1, float ch2, float ch3, float ch4, float ch5, float ch6, float ch7, float ch8, int counter, qint64 timestamp);
    void errorReceived(const QString &error);
    void processStateChanged(bool running);

private slots:
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();

private:
    void parseData();
    void processPacket(const QString &packet);

    QProcess *m_process;
    QByteArray m_buffer;
    float m_channel[8];
    int m_counter;
};

/*===========================Worker - работа с процессом RTU_WINAPI=================================*/

/*===========================DataProcessor - обработка данных в отдельном потоке=================================*/
class DataProcessor : public QObject {
    Q_OBJECT
public:
    explicit DataProcessor(QObject *parent = nullptr);

public slots:
    void processData(float ch1, float ch2, float ch3, float ch4,
                     float ch5, float ch6, float ch7, float ch8,
                     int counter, qint64 timestamp);

signals:
    void dataReceived(float ch1, float ch2, float ch3, float ch4,
                      float ch5, float ch6, float ch7, float ch8,
                      int counter, qint64 timestamp);
};
/*===========================DataProcessor - обработка данных в отдельном потоке=================================*/

class MainWindow: public QMainWindow {
    Q_OBJECT
private:
    qint64 startTime;
    double lastTime;

    // Векторы для хранения перекрестий и меток для каждого канала
    QVector<QCPItemLine*> tracerLinesX;   // Вертикальные линии
    QVector<QCPItemLine*> tracerLinesY;   // Горизонтальные линии
    QVector<QCPItemText*> tracerLabels;   // Метки со значениями

    void updateTimeTicks();
    void updateXAxisRange();  // Новая функция для обновления диапазона оси X

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void write_settings();
    void read_settings();

    QSerialPort *serial; //Подключим сериал
    QThread *pMyThread;
    serialthreaded *pSerial;
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

public slots:
    void clearPlot();
    void saveSnapshot(const QString &filePath);
    void loadSnapshot(const QString &filePath);
    void apply_setttings_OpenGL(bool arg1);
    void on_comboPort_currentIndexChanged(const QString &arg1);
    void portOpenedSuccess();
    void portOpenedFail();
    void onPortClosed();
    void replot();
    void on_spinYStep_valueChanged(int arg1);
    void on_savePNGButton_clicked();
    void onMouseMoveInPlot(QMouseEvent *event);
    void channel_selection(void);
    void on_actionConnect_triggered();
    void on_actionDisconnect_triggered();
    void on_actionHow_to_use_triggered();
    void on_actionPause_Plot_triggered();
    void on_actionClear_triggered();
    void on_actionRecord_stream_triggered();
    void on_action_png_triggered();
    void on_pushButton_AutoScale_clicked();
    void on_pushButton_ResetVisible_clicked();
    void on_listWidget_Channels_itemDoubleClicked(QListWidgetItem *item);
    void on_pushButton_clicked();
    void on_Autoscale_triggered();
    void scale_setting();
    void on_horizontalScrollBar_valueChanged(int value);
    void on_checkBox_clicked(bool checked);
    void on_checkBox_2_clicked(bool checked);
    void on_Reset_data_clicked();
    void on_spinBox_valueChanged(int arg1);
    void on_automatic_cnt_channel_clicked(bool checked);
    void on_action_input_data_triggered(bool checked);
    void on_action_graph_settings_triggered(bool checked);
    void on_action_debug_info_triggered(bool checked);
    void on_action_COM_port_triggered(bool checked);
    void Window_init(bool COM, bool input_data, bool graph_settings, bool debug_info, bool precision);
    void on_action_triggered();
    void on_action_visible_legend_triggered(bool checked);
    void on_action_windows_stays_on_top_triggered(bool checked);
    void on_action_Frameless_window_hint_triggered(bool checked);
    void on_action_run_triggered(bool checked);
    void on_action_use_OpenGL_triggered(bool checked);

    void on_openSnapshotAction_triggered();
    bool on_saveSnapshotAction_triggered();

    void on_channel_count_valueChanged(int arg1);
    void on_checkBox_3_stateChanged(int arg1);
    void onTableItemChanged(QTableWidgetItem *item);
    void closeDataFile(); // Закрыть бинарный файл
    QString formatTime(int seconds); //Преобразовать секунды в человекочитаемый формат
    void updateTablePrecision(int channelIndex); //обновляем прецизионность данных в таблице
    int getPrecisionForChannel(int channelIndex) const;
    void updateTableHeaders(bool isReadMode);
    void updateFileHeader(); //Функция обновления шапки файла


    /*============================Worker=====================*/
    void updateData(float ch1, float ch2, float ch3, float ch4,
                    float ch5, float ch6, float ch7, float ch8,
                    int counter, qint64 timestamp);
    void updateError(const QString &error);
    void updateProcessState(bool state);
    /*============================Worker=====================*/

    void on_spinBox_2_valueChanged(int arg1);

signals:
    void portClosed();
    void newData(QStringList data);

    /*============================Worker=====================*/
    void startProcess(const QString &program, const QStringList &arguments);
    void stopProcess();
    void clearStats();
    /*============================Worker=====================*/

private slots:
    void on_comboBox_precision_ch1_currentIndexChanged(int index);

    void on_comboBox_precision_ch2_currentIndexChanged(int index);

    void on_comboBox_precision_ch3_currentIndexChanged(int index);

    void on_comboBox_precision_ch4_currentIndexChanged(int index);

    void on_comboBox_precision_ch5_currentIndexChanged(int index);

    void on_comboBox_precision_ch6_currentIndexChanged(int index);

    void on_comboBox_precision_ch7_currentIndexChanged(int index);

    void on_comboBox_precision_ch8_currentIndexChanged(int index);

    void on_action_precision_triggered(bool checked);

    void on_Theme_White_triggered();

    void on_Theme_Dark_triggered();
   // void initWhiteTheme();
    //void initDarkTheme();
   // void applyThemeToUI();
   // void applyThemeToPlot();
   // void applyTheme();

private:
    Ui::MainWindow *ui;
    QSettings *settings;

    /*================Буферизация графика================*/
    QVector<double> buffer_x[8];
    QVector<double> buffer_y[8];
    int BUFFER_SIZE = 5;
    /*================Буферизация графика================*/

    QColor line_colors[CUSTOM_LINE_COLORS] = {
        QColor(205, 34, 46, 255),     // красный
        QColor(19, 84, 208, 255),     // Синий
        QColor(0, 144, 144, 255),     // зеленый
        QColor(213, 193, 90, 255),    // золотой
        QColor(218, 166, 168, 255),   // розовый
        QColor(176, 0, 13, 255),      // красный насыщенней
        QColor(159, 109, 25),         // коричневый
        QColor(173, 173, 173),        // серый
        QColor(73, 146, 184, 255),    // голубой
        QColor(35, 195, 193, 255),    // голубо-зеленый
        QColor(245, 181, 52, 255),    // оранжевый
        QColor(223, 99, 68, 255),     // недокрасный
        QColor(255, 141, 59, 255),    // морковный
        QColor(0, 77, 119, 255)       // темносиний
    };

    QColor gui_colors[GCP_CUSTOM_LINE_COLORS] = {
        QColor(249, 249, 249, 255),   // Задний фон графика
        QColor(170, 170, 170, 255),   // Grid color
        QColor(30, 30, 30, 255),      // Цвет текста графика
        QColor(245, 245, 245, 240)    // Цвет задника у легенды
    };

    QColor app_colors[10];

    bool connected;
    bool plotting;
    int dataPointNumber;
    int channels;
    bool filterDisplayedData = true;
    bool hasNewData;
    bool needAxisUpdate;

    QStringListModel *channelListModel;
    QStringList channelStrList;

    QTimer updateTimer;
    QString receivedData;
    int STATE;
    About *about;
    void createUI();
    void enable_com_controls(bool enable);
    void setupPlot();
    void setupTable();  // Добавлена функция setupTable
    void openPort(int baudRate, QSerialPort::DataBits dataBits, QSerialPort::Parity parity, QSerialPort::StopBits stopBits);

    // Функции для перекрестий
    double findNearestKey(double xCoord);
    void checkPoint(QCPGraphDataContainer::const_iterator it, double xCoord, double &targetKey, double &minDist);
    void hideAllTracers();
    void updateTracers(double targetKey);
    double getValueAtKey(QCPGraph *graph, double key);
    void positionLabel(int index, double key, double value);
    void updateStatusBar(double targetKey, double xCoord);
    void updateSpinBoxMax();

    /*============================Worker=====================*/
    QThread *m_workerThread;
    Worker *m_worker;
    /*============================Worker=====================*/

    /*============================DataProcessor=====================*/
    QThread *m_processorThread;
    DataProcessor *m_dataProcessor;
    /*============================DataProcessor=====================*/
};

#endif // MAINWINDOW_HPP
