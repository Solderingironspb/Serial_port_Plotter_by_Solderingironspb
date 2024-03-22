#include "mainwindow.hpp"
#include "ui_mainwindow.h"
#include <stdbool.h>

bool flag_Autoscale = false;
bool flag_Graph_moove = false;
bool flag_automatic_cnt_channels = false;
uint32_t x_scale_value = 500;
uint32_t Data_count = 0; //Количество графиков
uint64_t Horizontal_scroll_value = 0;
bool flag_only_graph = false;
uint32_t Width_pen_graph = 1;//Ширина линии графика
QString ComPortName;
bool Debug_visible = false;


MainWindow::MainWindow (QWidget *parent) :
    QMainWindow (parent),
    ui (new Ui::MainWindow),
    connected (false),
    plotting (false),
    dataPointNumber (0),
    channels(0),
    serialPort (nullptr),
    STATE (WAIT_START)
{
    ui->setupUi (this);
    settings = new QSettings("settings.ini", QSettings::IniFormat, this);
    createUI();//Настройка окна
    setupPlot();//Настройка графика
    connect (ui->plot, SIGNAL (mouseMove (QMouseEvent*)), this, SLOT (onMouseMoveInPlot (QMouseEvent*)));
    connect (ui->plot, SIGNAL(selectionChangedByUser()), this, SLOT(channel_selection()));
    connect (ui->plot, SIGNAL(legendDoubleClick (QCPLegend*, QCPAbstractLegendItem*, QMouseEvent*)), this, SLOT(legend_double_click (QCPLegend*, QCPAbstractLegendItem*, QMouseEvent*)));
    connect (&updateTimer, SIGNAL (timeout()), this, SLOT (replot()));
    m_csvFile = nullptr;
}

MainWindow::~MainWindow(){
    closeCsvFile();
    if (serialPort != nullptr){
        delete serialPort;
    }
    write_settings();//Перед закрытием окна сохраним настройки
    delete ui;
}

/*Функция сохранения настроек приложения*/
void MainWindow::write_settings(){
    settings->setValue("OpenGL", ui->action_use_OpenGL->isChecked());
    settings->setValue("WidthPen", Width_pen_graph);
    settings->setValue("Baudrate", ui->comboBaud->currentIndex());
    settings->setValue("Databits", ui->comboData->currentIndex());
    settings->setValue("Partity", ui->comboStop->currentIndex());
    settings->setValue("ComPort", ui->comboPort->currentText());
    settings->setValue("WinComPort", ui->action_COM_port->isChecked());
    settings->setValue("WinInputData", ui->action_input_data->isChecked());
    settings->setValue("WinGraphSettings", ui->action_graph_settings->isChecked());
    settings->setValue("WinDebugInfo", ui->action_debug_info->isChecked());
    //qDebug() << Debug_visible;
    settings->setValue("WinDebugInfoHide", Debug_visible);
    settings->setValue("WinOnTop", ui->action_windows_stays_on_top->isChecked());
    settings->setValue("Geometry", geometry());
    settings->setValue("Autoscale", ui->Autoscale->isChecked());
    settings->setValue("RunGraph", ui->action_run->isChecked());
    settings->setValue("X_set", ui->spinBox->value());
    settings->setValue("Y_set", ui->spinYStep->value());
    settings->setValue("Autoscan_channels", ui->automatic_cnt_channel->isChecked());
    settings->setValue("Count_channels", ui->channel_count->value());
    settings->setValue("Legend", ui->action_visible_legend->isChecked());
}

/*Функция чтения настроек приложения*/
void MainWindow::read_settings(){
    Width_pen_graph = settings->value("WidthPen", "2").toUInt();
    /*Включить/выключить аппаратное ускорение*/
    apply_setttings_OpenGL(settings->value("OpenGL", "false").toBool());
    /*Выберем 9600 по-умолчанию*/
    ui->comboBaud->setCurrentIndex (settings->value("Baudrate", "3").toInt());
    ui->comboData->setCurrentIndex(settings->value("Databits", "0").toInt());
    ui->comboParity->setCurrentIndex(settings->value("Partity", "0").toInt());
    ComPortName = settings->value("ComPort", "").toString();
    /*Отображение окон по умолчанию*/
    Window_init(settings->value("WinComPort", "true").toBool(),
                settings->value("WinInputData", "false").toBool(),
                settings->value("WinGraphSettings", "true").toBool(),
                settings->value("WinDebugInfo", "true").toBool());
    /*if ((settings->value("WinDebugInfoHide", "true").toBool()) != ui->textEdit_UartWindow->isVisible()){
        on_pushButton_TextEditHide_clicked();
    }*/
    ui->action_windows_stays_on_top->setChecked(settings->value("WinOnTop", "false").toBool());
    on_action_windows_stays_on_top_triggered(settings->value("WinOnTop", "false").toBool());
    setGeometry(settings->value("Geometry", QRect(200,200,300,300)).toRect());
    /*Автомастшабирование*/
    ui->checkBox->setChecked(settings->value("Autoscale", "true").toBool());
    ui->Autoscale->setChecked(settings->value("Autoscale", "true").toBool());
    on_Autoscale_triggered();
    /*Бегущий график*/
    ui->action_run->setChecked(settings->value("RunGraph", "true").toBool());
    ui->checkBox_2->setChecked(settings->value("RunGraph", "true").toBool());
    on_checkBox_2_clicked(settings->value("RunGraph", "true").toBool());
    /*Делений по Y выставим*/
    ui->spinYStep->setValue(settings->value("Y_set", "5").toUInt());
    on_spinYStep_valueChanged(settings->value("Y_set", "5").toUInt());
    /*Делений по X выставим*/
    ui->spinBox->setValue(settings->value("X_set", "500").toUInt());
    on_spinBox_valueChanged(settings->value("X_set", "500").toUInt());
    /*Автоматически определять количество каналов*/
    ui->automatic_cnt_channel->setChecked(settings->value("Autoscan_channels", "true").toBool());
    on_automatic_cnt_channel_clicked(settings->value("Autoscan_channels", "true").toBool());
    /*Количество каналов*/
    ui->channel_count->setValue(settings->value("Count_channels", "1").toUInt());


}

/*Функция инициализации (Вкл/выкл Аппаратное ускорение)*/
void MainWindow::apply_setttings_OpenGL(bool arg1){
    ui->action_use_OpenGL->setChecked(arg1);
    on_action_use_OpenGL_triggered(arg1);
}


/*Настройка окна*/
void MainWindow::createUI(){
    /*Проверим, есть ли в списке COM портов хоть что-то*/
    if (QSerialPortInfo::availablePorts().size() == 0){
        enable_com_controls (false);
        ui->statusBar->showMessage ("COM портов не обнаружено");
        ui->savePNGButton->setEnabled (false);
    }

    //Заполним список тем, что имеем в системе
    Q_FOREACH(QSerialPortInfo port, QSerialPortInfo::availablePorts()){
        ui->comboPort->addItem (port.portName());
    }

    /*Заполним список стандартных настроек COM портов*/
    /*Baudrate*/
    ui->comboBaud->addItem ("1200");
    ui->comboBaud->addItem ("2400");
    ui->comboBaud->addItem ("4800");
    ui->comboBaud->addItem ("9600");
    ui->comboBaud->addItem ("19200");
    ui->comboBaud->addItem ("38400");
    ui->comboBaud->addItem ("57600");
    ui->comboBaud->addItem ("115200");
    ui->comboBaud->addItem ("128000");
    ui->comboBaud->addItem ("153600");
    ui->comboBaud->addItem ("230400");
    ui->comboBaud->addItem ("256000");
    ui->comboBaud->addItem ("460800");
    ui->comboBaud->addItem ("921600");
    /*Data bits*/
    ui->comboData->addItem ("8 bits");
    ui->comboData->addItem ("7 bits");
    /*Partity*/
    ui->comboParity->addItem ("none");
    ui->comboParity->addItem ("odd");
    ui->comboParity->addItem ("even");
    /*Stop bits*/
    ui->comboStop->addItem ("1 bit");
    ui->comboStop->addItem ("2 bits");
    ui->listWidget_Channels->clear();
    /*Определим настройки по-умолчанию*/

    read_settings();

    uint32_t Index = ui->comboPort->count();
    for (uint32_t i = 0; i < Index; i++){
        if (ui->comboPort->itemText(i) == ComPortName){
            ui->comboPort->setCurrentIndex(i);
            //qDebug() << "Выбран порт: " << ui->comboPort->itemText(i);
        }
    }
}


/*Настройка графика*/
void MainWindow::setupPlot(){
    /*Очистим график*/
    ui->plot->clearItems();
    /*Зададим фон графику*/
    ui->plot->setBackground (gui_colors[0]);
    ui->plot->setNotAntialiasedElements (QCP::aeAll);
    QFont font;
    font.setStyleStrategy (QFont::NoAntialias);
    ui->plot->legend->setFont (font);

    /*Настройка оси X*/
    ui->plot->xAxis->grid()->setZeroLinePen(QPen(gui_colors[1], 1, Qt::DotLine));
    ui->plot->xAxis->grid()->setPen (QPen(gui_colors[1], 1, Qt::DotLine));
    ui->plot->xAxis->grid()->setSubGridPen (QPen(gui_colors[1], 1, Qt::DotLine));
    //ui->plot->xAxis->grid()->setSubGridVisible (true);
    ui->plot->xAxis->setBasePen (QPen (gui_colors[2]));
    ui->plot->xAxis->setTickPen (QPen (gui_colors[2]));
    ui->plot->xAxis->setSubTickPen (QPen (gui_colors[2]));
    ui->plot->xAxis->setUpperEnding (QCPLineEnding::esSpikeArrow);
    ui->plot->xAxis->setTickLabelColor (gui_colors[2]);
    ui->plot->xAxis->setTickLabelFont (font);
    scale_setting();

    /*Настройка оси Y*/
    ui->plot->yAxis->grid()->setZeroLinePen(QPen(gui_colors[1], 1, Qt::DotLine));
    ui->plot->yAxis->grid()->setPen (QPen(gui_colors[1], 1, Qt::DotLine));
    ui->plot->yAxis->grid()->setSubGridPen (QPen(gui_colors[1], 1, Qt::DotLine));
    //ui->plot->yAxis->grid()->setSubGridVisible (true);
    ui->plot->yAxis->setBasePen (QPen (gui_colors[2]));
    ui->plot->yAxis->setTickPen (QPen (gui_colors[2]));
    ui->plot->yAxis->setSubTickPen (QPen (gui_colors[2]));
    ui->plot->yAxis->setUpperEnding (QCPLineEnding::esSpikeArrow);
    ui->plot->yAxis->setTickLabelColor (gui_colors[2]);
    ui->plot->yAxis->setTickLabelFont (font);

    /* Легенда */
    QFont legendFont;
    legendFont.setPointSize (8); //Размер
    //ui->plot->legend->setVisible (true);
    ui->plot->legend->setFont (legendFont);
    ui->plot->legend->setBrush (gui_colors[3]); //Цвет задника
    ui->plot->legend->setBorderPen (gui_colors[1]);//Цвет рамки
    ui->plot->axisRect()->insetLayout()->setInsetAlignment (0, Qt::AlignTop|Qt::AlignRight); //Отображать легенду сверху и справа

    ui->action_visible_legend->setChecked(true);
    on_action_visible_legend_triggered(true);
    /*Показать/скрыть легенду*/
    ui->action_visible_legend->setChecked(settings->value("Legend", "true").toBool());
    on_action_visible_legend_triggered(settings->value("Legend", "true").toBool());

}


/*Параметры отображения работы COM порта*/
void MainWindow::enable_com_controls (bool enable){
    /*Свойства COM порта*/
    ui->comboBaud->setEnabled (enable);
    ui->comboData->setEnabled (enable);
    ui->comboParity->setEnabled (enable);
    ui->comboPort->setEnabled (enable);
    ui->comboStop->setEnabled (enable);
    /*Кнопки подключиться, пауза, отключиться*/
    ui->actionConnect->setEnabled (enable);
    ui->actionPause_Plot->setEnabled (!enable);
    ui->actionDisconnect->setEnabled (!enable);
}

/*Открыть сессию работы с COM портом*/
void MainWindow::openPort (QSerialPortInfo portInfo, int baudRate, QSerialPort::DataBits dataBits, QSerialPort::Parity parity, QSerialPort::StopBits stopBits){
    serialPort = new QSerialPort(portInfo, nullptr);

    connect (this, SIGNAL(portOpenOK()), this, SLOT(portOpenedSuccess()));
    connect (this, SIGNAL(portOpenFail()), this, SLOT(portOpenedFail()));
    connect (this, SIGNAL(portClosed()), this, SLOT(onPortClosed()));
    connect (this, SIGNAL(newData(QStringList)), this, SLOT(onNewDataArrived(QStringList)));
    connect (serialPort, SIGNAL(readyRead()), this, SLOT(readData()));
    connect (this, SIGNAL(newData(QStringList)), this, SLOT(saveStream(QStringList)));

    if (serialPort->open (QIODevice::ReadWrite)){
        serialPort->setBaudRate (baudRate);
        serialPort->setParity (parity);
        serialPort->setDataBits (dataBits);
        serialPort->setStopBits (stopBits);
        emit portOpenOK();
    }else{
        emit portOpenedFail();
    }
}

/*Закрыть сессию работы с COM портом*/
void MainWindow::onPortClosed(){
    updateTimer.stop();
    connected = false;
    plotting = false;

    closeCsvFile();
    
    disconnect (serialPort, SIGNAL(readyRead()), this, SLOT(readData()));
    disconnect (this, SIGNAL(portOpenOK()), this, SLOT(portOpenedSuccess()));
    disconnect (this, SIGNAL(portOpenFail()), this, SLOT(portOpenedFail()));
    disconnect (this, SIGNAL(portClosed()), this, SLOT(onPortClosed()));
    disconnect (this, SIGNAL(newData(QStringList)), this, SLOT(onNewDataArrived(QStringList)));
    disconnect (this, SIGNAL(newData(QStringList)), this, SLOT(saveStream(QStringList)));

    ui->pushButton->setEnabled(true); //Сделаем активной кнопку обновления списка COM портов
}

/*Отображение информации о подключившемся COM порту*/
void MainWindow::on_comboPort_currentIndexChanged (const QString &arg1){
    QSerialPortInfo selectedPort (arg1);
    ui->statusBar->showMessage (selectedPort.description());
}


/*При удачном открытии COM порта*/
void MainWindow::portOpenedSuccess(){
    /*Скроем настройка COM порта*/
    ui->action_COM_port->setChecked(false);
    on_action_COM_port_triggered(false);
    //setupPlot();
    ui->statusBar->showMessage ("Подключено!");
    enable_com_controls (false);
    
    /*Если запись в *.csv файл включена*/
    if(ui->actionRecord_stream->isChecked()){
        /*Создадим новый *.csv файл*/
        openCsvFile();
    }
    /*Заблокируем кнопку выключения записи, пока не отключимся от COM порта*/
    ui->actionRecord_stream->setEnabled(false);
    updateTimer.start (20);//Запустим таймер на обновление графика. По идее должно быть 50 FPS
    connected = true;
    plotting = true;
    ui->pushButton->setEnabled(false); //При удачном подключении кнопку обновления списка доступных COM портов сделаем неактивной
}


/*Если невозможно подключиться к COM порту*/
void MainWindow::portOpenedFail(){
    ui->PortControlsBox->setVisible(true);
    ui->statusBar->showMessage ("Невозможно подключиться к COM порту!");
    ui->pushButton->setEnabled(true);
    on_pushButton_clicked();//Автообновление списка COM портов
}


/*Функция перерисовки графика. Обновление страницы.*/
void MainWindow::replot(){
    /*Если автомасштаб включен*/
    if (flag_Autoscale){
        ui->plot->yAxis->rescale(true);//отмасштабируем график
    }
    scale_setting();
    ui->plot->replot();
}


/*Функция по составлению графика из полученных данных. Основа парсинга позаимствована у Borislav: https://github.com/CieNTi/serial_port_plotter*/
/*Я нихрена не понял, чего он так замудрил, ну да ладно...C CH340 работает хорошо. С CDC от ST link v2.1 работает со сбоями*/
void MainWindow::onNewDataArrived(QStringList newData){
    static int data_members = 0;
    static int channel = 0;
    static int i = 0;
    volatile bool you_shall_NOT_PASS = false;

    /* When a fast baud rate is set (921kbps was the first to starts to bug),
       this method is called multiple times (2x in the 921k tests), so a flag
       is used to throttle
       TO-DO: Separate processes, buffer data (1) and process data (2) */
    while (you_shall_NOT_PASS) {}
    you_shall_NOT_PASS = true;

    if (plotting){
        /* Get size of received list */
        data_members = newData.size();

        if (flag_automatic_cnt_channels){
            ui->channel_count->setValue(data_members);
        }

        if (data_members <= ui->channel_count->value()){
            /* Parse data */
            for (i = 0; i < data_members; i++){
                /* Update number of axes if needed */
                while (ui->plot->plottableCount() <= channel){
                    /* Add new channel data */ //Добавлен стиль линии на более жирный
                    ui->plot->addGraph();
                    QPen GraphY1;
                    GraphY1.setWidth(Width_pen_graph); //Ширина линии
                    GraphY1.setColor(line_colors[channels % CUSTOM_LINE_COLORS]);
                    ui->plot->graph()->setPen (GraphY1);
                    ui->plot->graph()->setName (QString("Канал данных %1").arg(channels));
                    if(ui->plot->legend->item(channels)){
                        ui->plot->legend->item (channels)->setTextColor (line_colors[channels % CUSTOM_LINE_COLORS]);
                    }
                    ui->listWidget_Channels->addItem(ui->plot->graph()->name());
                    ui->listWidget_Channels->item(channel)->setForeground(QBrush(line_colors[channels % CUSTOM_LINE_COLORS]));
                    channels++;
                }

                ui->plot->graph(channel)->addData (dataPointNumber, newData[channel].toFloat());
                /* Increment data number and channel */
                channel++;

            }

            dataPointNumber++;
            channel = 0;
            ui->horizontalScrollBar->setMaximum(dataPointNumber);
        }
        you_shall_NOT_PASS = false;
    }
}


/*Обработка приходящих данных*/
/*Основа парсинга позаимствована у Borislav: https://github.com/CieNTi/serial_port_plotter*/
void MainWindow::readData(){
    if(serialPort->bytesAvailable()) {                                                    // If any bytes are available
        QByteArray data = serialPort->readAll();                                          // Read all data in QByteArray
        if(!data.isEmpty()) {                                                             // If the byte array is not empty
            unsigned char *temp = (unsigned char*)data.data();                                                     // Get a '\0'-terminated char* to the data

            for(int i = 0; temp[i] != '\0'; i++) {                                        // Iterate over the char*
                switch(STATE) {                                                           // Switch the current state of the message
                case WAIT_START:                                                          // If waiting for start [$], examine each char
                    if(temp[i] == START_MSG) {                                            // If the char is $, change STATE to IN_MESSAGE
                        STATE = IN_MESSAGE;
                        receivedData.clear();                                             // Clear temporary QString that holds the message
                        break;                                                            // Break out of the switch
                    }
                    break;
                case IN_MESSAGE:                                                          // If state is IN_MESSAGE
                    if(temp[i] == END_MSG) {                                              // If char examined is ;, switch state to END_MSG
                        STATE = WAIT_START;
                        QStringList incomingData = receivedData.split(' ');               // Split string received from port and put it into list
                        if(filterDisplayedData){
                            ui->textEdit_UartWindow->clear();
                            for(int i = 0; i<incomingData.size(); i++){
                                ui->textEdit_UartWindow->append("Канал данных "+ QString::number(i,10) + ": " + incomingData[i]);
                            }
                        }
                        emit newData(incomingData);
                        Data_count = incomingData.count();
                        break;
                    }else if (isdigit (temp[i]) || temp[i] == ' ' || temp[i] =='-' || temp[i] =='.'){
                        /* If examined char is a digit, and not '$' or ';', append it to temporary string */
                        receivedData.append(temp[i]);
                    }
                    break;
                default: break;
                }
            }
        }
    }
}


/*Ползунок Делений по Y*/
void MainWindow::on_spinYStep_valueChanged(int arg1){
    ui->plot->yAxis->ticker()->setTickCount(arg1);
    ui->spinYStep->setValue(ui->plot->yAxis->ticker()->tickCount());
}


/*Кнопка сохранить график в *.png*/
void MainWindow::on_savePNGButton_clicked(){
    QString Text = "graph_snapshot_" + QDateTime::currentDateTime().toString("HH.mm.ss-d.MM.yyyy") + ".png";
    ui->plot->savePng (Text, 1920, 1080, 2, 50);
    ui->statusBar->showMessage ("Запись в файл: " + Text);
}


/*Отслеживание координат на графике при вождении мыши*/
void MainWindow::onMouseMoveInPlot(QMouseEvent *event){
    int xx = int(ui->plot->xAxis->pixelToCoord(event->pos().x()));
    int yy = int(ui->plot->yAxis->pixelToCoord(event->pos().y()));
    QString coordinates("Координата курсора x: %1 y: %2");
    coordinates = coordinates.arg(xx).arg(yy);
    ui->statusBar->showMessage(coordinates);
}


/*Выбор графика для отображения*/
void MainWindow::channel_selection (void){
    for (int i = 0; i < ui->plot->graphCount(); i++){
        QCPGraph *graph = ui->plot->graph(i);
        QCPPlottableLegendItem *item = ui->plot->legend->itemWithPlottable (graph);
        if (item->selected()){
            item->setSelected (true);
        }else{
            item->setSelected (false);
        }
    }
}


/*Двойной клик по легенде*/
void MainWindow::legend_double_click(QCPLegend *legend, QCPAbstractLegendItem *item, QMouseEvent *event){
    Q_UNUSED (legend)
    Q_UNUSED(event)
    if (item){
        QCPPlottableLegendItem *plItem = qobject_cast<QCPPlottableLegendItem*>(item);
        bool ok;
        QString newName = QInputDialog::getText (this, "Введите имя канала", "Изменить имя канала:", QLineEdit::Normal, plItem->plottable()->name(), &ok, Qt::Popup);
        if (ok){
            plItem->plottable()->setName(newName);
            for(int i=0; i<ui->plot->graphCount(); i++){
                ui->listWidget_Channels->item(i)->setText(ui->plot->graph(i)->name());
            }
            ui->plot->replot();
        }
    }
}

/*Отображение по X. Сколько точек. График бегущий или стоячий*/
void MainWindow::scale_setting(){
    if (flag_Graph_moove){
        if (dataPointNumber < (int)x_scale_value){
            ui->plot->xAxis->setRange (0, x_scale_value);
        }else{
            ui->plot->xAxis->setRange (dataPointNumber - x_scale_value, dataPointNumber);
        }
    }else{
        ui->plot->xAxis->setRange (0 + Horizontal_scroll_value, x_scale_value + Horizontal_scroll_value);
    }
}

/*Окно помощь -> о программе*/
void MainWindow::on_actionHow_to_use_triggered(){
    about = new About (this);
    about->setWindowTitle ("О программе");
    about->setWindowFlags(Qt::Dialog| Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowSystemMenuHint); //Кнопки на окне(Свернуть, большое окно и закрыть.)
    about->show();
}



/*Возобновление работы после паузы*/
void MainWindow::on_actionConnect_triggered(){
    if (connected){
        if (!plotting){
            updateTimer.start();
            plotting = true;
            ui->actionConnect->setEnabled (false);
            ui->actionPause_Plot->setEnabled (true);
            ui->statusBar->showMessage ("Продолжим отображение графика!");
        }
    }else{
        QSerialPortInfo portInfo (ui->comboPort->currentText());
        int baudRate = ui->comboBaud->currentText().toInt();
        int dataBitsIndex = ui->comboData->currentIndex();
        int parityIndex = ui->comboParity->currentIndex();
        int stopBitsIndex = ui->comboStop->currentIndex();
        QSerialPort::DataBits dataBits;
        QSerialPort::Parity parity;
        QSerialPort::StopBits stopBits;

        switch (dataBitsIndex){
        case 0:
            dataBits = QSerialPort::Data8;
            break;
        default:
            dataBits = QSerialPort::Data7;
        }

        switch (parityIndex){
        case 0:
            parity = QSerialPort::NoParity;
            break;
        case 1:
            parity = QSerialPort::OddParity;
            break;
        default:
            parity = QSerialPort::EvenParity;
        }

        switch (stopBitsIndex)
        {
        case 0:
            stopBits = QSerialPort::OneStop;
            break;
        default:
            stopBits = QSerialPort::TwoStop;
        }

        serialPort = new QSerialPort (portInfo, nullptr);

        openPort (portInfo, baudRate, dataBits, parity, stopBits);
    }
}


/*Нажатие кнопки Пауза*/
void MainWindow::on_actionPause_Plot_triggered(){
    if (plotting){
        updateTimer.stop();                                                               // Stop updating plot timer
        plotting = false;
        ui->actionConnect->setEnabled (true);
        ui->actionPause_Plot->setEnabled (false);
        ui->statusBar->showMessage ("COM порт подключен, но поставлен на паузу. Входящие данные игнорируются");
    }
}



/*Нажатие кнопки Сохранять данные в *.csv*/
void MainWindow::on_actionRecord_stream_triggered(){
    if (ui->actionRecord_stream->isChecked()){
        ui->statusBar->showMessage ("Запись в *.csv файл включена");
    }
    else{
        ui->statusBar->showMessage ("Запись в *.csv файл выключена");
    }
}


/*Нажатие кнопки Отключиться*/
void MainWindow::on_actionDisconnect_triggered(){
    if (connected){
        serialPort->close();
        emit portClosed();
        delete serialPort;
        serialPort = nullptr;
        ui->statusBar->showMessage ("COM порт отключен!");
        connected = false;
        ui->actionConnect->setEnabled (true);
        plotting = false;
        ui->actionPause_Plot->setEnabled (false);
        ui->actionDisconnect->setEnabled (false);
        ui->actionRecord_stream->setEnabled(true);
        receivedData.clear();
        enable_com_controls (true);
        /*Отобразим окно настройки COM порта*/
        ui->action_COM_port->setChecked(true);
        on_action_COM_port_triggered(true);
    }
}


/*Нажатие на кнопку Сбросить данные*/
void MainWindow::on_actionClear_triggered(){
    ui->plot->clearPlottables();
    ui->listWidget_Channels->clear();
    channels = 0;
    dataPointNumber = 0;
    //emit setupPlot();
    ui->plot->replot();
}


/*Создание *.csv файла*/
void MainWindow::openCsvFile(void){
    QString Text = "record_input_data_" + QDateTime::currentDateTime().toString("HH.mm.ss_d.MM.yyyy") + ".csv";
    m_csvFile = new QFile(Text);
    if(!m_csvFile)
        return;
    if (!m_csvFile->open(QIODevice::ReadWrite | QIODevice::Text))
        return;
    ui->statusBar->showMessage ("Запись в файл: " + Text);
}


/*Закрытие *.csv файла*/
void MainWindow::closeCsvFile(void){
    if(!m_csvFile) return;
    m_csvFile->close();
    if(m_csvFile) delete m_csvFile;
    m_csvFile = nullptr;
    ui->statusBar->showMessage ("Файл находится в корне папки программы");
}


/*Сохранение данных в *.csv*/
void MainWindow::saveStream(QStringList newData){
    if(!m_csvFile)
        return;
    if(ui->actionRecord_stream->isChecked()){
        if (dataPointNumber>0){
            QTextStream out(m_csvFile);
            out << dataPointNumber << ",";
            foreach (const QString &str, newData) {
                out << str << ",";
            }
            out << "\n";
        }
    }
}

/*Нажать на кнопку показать/скрыть информацию*/
void MainWindow::on_pushButton_TextEditHide_clicked(){
    if(ui->pushButton_TextEditHide->isChecked()){
        ui->textEdit_UartWindow->setVisible(false);
        Debug_visible = false;
        ui->pushButton_TextEditHide->setText("Показать информацию");
    }else{
        ui->textEdit_UartWindow->setVisible(true);
        Debug_visible = true;
        ui->pushButton_TextEditHide->setText("Скрыть информацию");
    }
}

/*Нажатие кнопки Автомасштаб*/
void MainWindow::on_pushButton_AutoScale_clicked(){
    ui->plot->yAxis->rescale(true);
    if (!updateTimer.isActive()){
        replot();
    }
}

/*Нажатие кнопки Сбросить вид*/
void MainWindow::on_pushButton_ResetVisible_clicked(){
    for(int i=0; i<ui->plot->graphCount(); i++){
        ui->plot->graph(i)->setVisible(true);
        ui->listWidget_Channels->item(i)->setBackground(Qt::NoBrush);
    }
}

/*Двойное нажатие по листу с доступными каналами*/
void MainWindow::on_listWidget_Channels_itemDoubleClicked(QListWidgetItem *item){
    int graphIdx = ui->listWidget_Channels->currentRow();
    if(ui->plot->graph(graphIdx)->visible()){
        ui->plot->graph(graphIdx)->setVisible(false);
        item->setBackground(QColor (205, 34,  46,  100)); //не отображать график. Цвет красный
    }else{
        ui->plot->graph(graphIdx)->setVisible(true);
        item->setBackground(Qt::NoBrush);
    }
    ui->plot->replot();
}

/*Нажатие кнопки обновить список*/
void MainWindow::on_pushButton_clicked(){
    /*Очистим список доступных COM портов*/
    ui->comboPort->clear();
    /*Проверим, есть ли в списке COM портов хоть что-то*/
    if (QSerialPortInfo::availablePorts().size() == 0){
        enable_com_controls (false);
        ui->statusBar->showMessage ("COM портов не обнаружено");
        ui->savePNGButton->setEnabled (false);
    }else{
        enable_com_controls (true);
        ui->savePNGButton->setEnabled (true);
    }
    Q_FOREACH(QSerialPortInfo port, QSerialPortInfo::availablePorts()){
        ui->comboPort->addItem (port.portName());
    }
}

/*График->Автомасштаб*/
void MainWindow::on_Autoscale_triggered(){
    if (ui->Autoscale->isChecked()){
        flag_Autoscale = true;
        ui->checkBox->setChecked(1);
        ui->plot->setInteraction (QCP::iRangeZoom, false);
    }else{
        flag_Autoscale = false;
        ui->checkBox->setChecked(0);
        ui->plot->setInteraction (QCP::iRangeZoom, true);
    }

    if (!updateTimer.isActive()){
        replot();

    }
}

/*Работа с горизонтальным скролл баром*/
void MainWindow::on_horizontalScrollBar_valueChanged(int value){
    Horizontal_scroll_value = value;
    if (!updateTimer.isActive()){
        replot();
    }
}

/*CheckBox Автомасштаб*/
void MainWindow::on_checkBox_clicked(bool checked){
    if (checked){
        ui->Autoscale->setChecked(1);
        ui->Autoscale->setChecked(1);
        on_Autoscale_triggered();
    }else{
        ui->Autoscale->setChecked(0);
        ui->Autoscale->setChecked(0);
        on_Autoscale_triggered();
    }
}

/*CheckBox Бегущий график*/
void MainWindow::on_checkBox_2_clicked(bool checked){
    ui->action_run->setChecked(checked);
    flag_Graph_moove= checked;
    if (flag_Graph_moove){
        ui->plot->setInteraction (QCP::iRangeDrag, false);
    }else{
        ui->plot->setInteraction (QCP::iRangeDrag, false);
    }
}

/*График -> Бегущий график*/
void MainWindow::on_action_run_triggered(bool checked){
    ui->checkBox_2->setChecked(checked);
    on_checkBox_2_clicked(checked);
}

/*Нажатие кнопки Сбросить график*/
void MainWindow::on_Reset_data_clicked(){
    on_actionClear_triggered();
}

/*Выбор, сколько точек по оси Х будем отображать*/
void MainWindow::on_spinBox_valueChanged(int arg1){
    x_scale_value = arg1;
}


/*Кнопка определения автоматического количества каналов*/
void MainWindow::on_automatic_cnt_channel_clicked(bool checked){
    flag_automatic_cnt_channels = checked;
}

/*Нажатие из верхнего меню кнопки (Сохранить в *.png)*/
void MainWindow::on_action_png_triggered(){
    on_savePNGButton_clicked();
}

/*Показать/скрыть окно входящие данные*/
void MainWindow::on_action_input_data_triggered(bool checked){
    ui->groupBox_2->setVisible(checked);
}

/*Показать/скрыть окно настройки графика*/
void MainWindow::on_action_graph_settings_triggered(bool checked){
    ui->PlotControlsBox->setVisible(checked);
}

/*Показать/скрыть окно отладочная информация*/
void MainWindow::on_action_debug_info_triggered(bool checked){
    ui->gridGroupBox->setVisible(checked);
}

/*Показать/скрыть окно настройки COM порта*/
void MainWindow::on_action_COM_port_triggered(bool checked){
    ui->PortControlsBox->setVisible(checked);
}

/*Инициализация. Какие окна показывать, а какие нет*/
void MainWindow::Window_init(bool COM, bool input_data, bool graph_settings, bool debug_info){
    ui->action_COM_port->setChecked(COM);
    on_action_COM_port_triggered(COM);

    ui->action_input_data->setChecked(input_data);
    on_action_input_data_triggered(input_data);

    ui->action_graph_settings->setChecked(graph_settings);
    on_action_graph_settings_triggered(graph_settings);

    ui->action_debug_info->setChecked(debug_info);
    on_action_debug_info_triggered(debug_info);
}


/*Показывать только график. Сочетаник Ctrl+E*/
void MainWindow::on_action_triggered(){
    flag_only_graph = !flag_only_graph;
    if (flag_only_graph){
        Window_init(0,0,0,0);
    }else{
        Window_init(1,1,1,1);
    }
}

/*Скрыть отладочную информацию. Сочетание Ctrl+L*/
void MainWindow::on_action_hide_debug_info_triggered(){
    ui->pushButton_TextEditHide->click();
}

/*Показывать/скрыть легенду с графика. Сочетание Crtl+Shift+L*/
void MainWindow::on_action_visible_legend_triggered(bool checked){
    ui->plot->legend->setVisible(checked);
    if (!updateTimer.isActive()){
        replot();
    }
}

/*Вид->Закрепить поверх всех окон*/
void MainWindow::on_action_windows_stays_on_top_triggered(bool checked){
    if (checked){
        this->setWindowFlag(Qt::WindowStaysOnTopHint, true);
        this->show();
    }else{
        this->setWindowFlag(Qt::WindowStaysOnTopHint, false);
        this->show();
    }
}

/*Вид->Сделать безрамное окно*/
void MainWindow::on_action_Frameless_window_hint_triggered(bool checked){
    if (checked){
        this->setWindowFlag(Qt::FramelessWindowHint, true);
        this->show();
    }else{
        this->setWindowFlag(Qt::FramelessWindowHint, false);
        this->show();
    }
}



/*Использование аппаратного ускорения*/
void MainWindow::on_action_use_OpenGL_triggered(bool checked){
    ui->plot->setOpenGl(checked, 3);
    if (ui->plot->openGl()){
        ui->statusBar->showMessage ("OpenGL включен");
    }else{
        ui->statusBar->showMessage ("OpenGL выключен");
    }
}

