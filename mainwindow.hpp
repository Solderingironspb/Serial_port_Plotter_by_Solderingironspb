#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <QtSerialPort/QtSerialPort>
#include <QSerialPortInfo>
#include "about.h"
#include "qcustomplot/qcustomplot.h"
#include <QSettings>

#define START_MSG       '$'
#define END_MSG         ';'
#define WAIT_START      1
#define IN_MESSAGE      2
#define UNDEFINED       3
#define CUSTOM_LINE_COLORS   14
#define GCP_CUSTOM_LINE_COLORS 4

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void write_settings();
    void read_settings();

private slots:
    void apply_setttings_OpenGL(bool arg1);
    void on_comboPort_currentIndexChanged(const QString &arg1);
    void portOpenedSuccess();
    void portOpenedFail();
    void onPortClosed();
    void replot();
    void onNewDataArrived(QStringList newData);
    void saveStream(QStringList newData);
    void readData();
    void on_spinYStep_valueChanged(int arg1);
    void on_savePNGButton_clicked();
    void onMouseMoveInPlot (QMouseEvent *event);
    void channel_selection (void);
    void legend_double_click (QCPLegend *legend, QCPAbstractLegendItem *item, QMouseEvent *event);
    void on_actionConnect_triggered();
    void on_actionDisconnect_triggered();
    void on_actionHow_to_use_triggered();
    void on_actionPause_Plot_triggered();
    void on_actionClear_triggered();
    void on_actionRecord_stream_triggered();
    void on_action_png_triggered();
    void on_pushButton_TextEditHide_clicked();
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
    void Window_init(bool COM, bool input_data, bool graph_settings, bool debug_info);
    void on_action_triggered();
    void on_action_hide_debug_info_triggered();
    void on_action_visible_legend_triggered(bool checked);
    void on_action_windows_stays_on_top_triggered(bool checked);
    void on_action_Frameless_window_hint_triggered(bool checked);
    void on_action_run_triggered(bool checked);
    void on_action_use_OpenGL_triggered(bool checked);

signals:
    void portOpenFail();
    void portOpenOK();
    void portClosed();
    void newData(QStringList data);

private:
    Ui::MainWindow *ui;
    QSettings *settings;

    QColor line_colors[CUSTOM_LINE_COLORS] = {
        QColor (205,34,46,255), //красный
        QColor (19,84,208,255), //Синий
        QColor (0,144,144,255), //зеленый
        QColor (213,193,90,255), //золотой
        QColor (218,166,168,255), //розовый
        QColor (176,0,13,255), //красный насыщенней
        QColor (159,109,25), //коричневый
        QColor (173,173,173), //серый
        QColor (73,146,184,255), //голубой
        QColor (35,195,193,255), //голубо-зеленый
        QColor (245,181,52,255), //оранжевый
        QColor (223,99,68,255), //недокрасный
        QColor (255,141,59,255), //морковный
        QColor (0,77,119,255), //темносиний;
    };
    QColor gui_colors[GCP_CUSTOM_LINE_COLORS] = {
        QColor (249, 249,  249,  255), //Задний фон графика
        QColor (170,  170,  170,  255), //Grid color
        QColor (30, 30, 30, 255), //Цвет текста графика
        QColor (245,  245,  245,  240)  //Цвет задника у легенды
    };

    bool connected;
    bool plotting;
    int dataPointNumber;
    int channels;
    bool filterDisplayedData = true;

    QStringListModel *channelListModel;
    QStringList     channelStrList;

    QFile* m_csvFile = nullptr;
    void openCsvFile(void);
    void closeCsvFile(void);

    QTimer updateTimer;
    QSerialPort *serialPort;
    QString receivedData;
    int STATE;
    About *about;

    void createUI();
    void enable_com_controls (bool enable);
    void setupPlot();
    void openPort(QSerialPortInfo portInfo, int baudRate, QSerialPort::DataBits dataBits, QSerialPort::Parity parity, QSerialPort::StopBits stopBits);
};


#endif                                                                                    // MAINWINDOW_HPP
