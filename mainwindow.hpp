/***************************************************************************
**  This file is part of Serial Port Plotter                              **
**                                                                        **
**                                                                        **
**  Serial Port Plotter is a program for plotting integer data from       **
**  serial port using Qt and QCustomPlot                                  **
**                                                                        **
**  This program is free software: you can redistribute it and/or modify  **
**  it under the terms of the GNU General Public License as published by  **
**  the Free Software Foundation, either version 3 of the License, or     **
**  (at your option) any later version.                                   **
**                                                                        **
**  This program is distributed in the hope that it will be useful,       **
**  but WITHOUT ANY WARRANTY; without even the implied warranty of        **
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         **
**  GNU General Public License for more details.                          **
**                                                                        **
**  You should have received a copy of the GNU General Public License     **
**  along with this program.  If not, see http://www.gnu.org/licenses/.   **
**                                                                        **
****************************************************************************
**           Author: Borislav                                             **
**           Contact: b.kereziev@gmail.com                                **
**           Date: 29.12.14                                               **
****************************************************************************/

#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <QtSerialPort/QtSerialPort>
#include <QSerialPortInfo>
#include "helpwindow.hpp"
#include "qcustomplot/qcustomplot.h"

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

private slots:
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

signals:
    void portOpenFail();
    void portOpenOK();
    void portClosed();
    void newData(QStringList data);

private:
    Ui::MainWindow *ui;

    QColor line_colors[CUSTOM_LINE_COLORS];
    QColor gui_colors[GCP_CUSTOM_LINE_COLORS];

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
    HelpWindow *helpWindow;

    void createUI();
    void enable_com_controls (bool enable);
    void setupPlot();
    void openPort(QSerialPortInfo portInfo, int baudRate, QSerialPort::DataBits dataBits, QSerialPort::Parity parity, QSerialPort::StopBits stopBits);
};


#endif                                                                                    // MAINWINDOW_HPP
