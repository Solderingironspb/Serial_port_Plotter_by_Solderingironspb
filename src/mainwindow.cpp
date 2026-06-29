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
    pSerial (nullptr),
    ui (new Ui::MainWindow),
    connected (false),
    plotting (false),
    dataPointNumber (0),
    channels(0),
    STATE (WAIT_START),
    startTime(0),
    lastTime(0)
{
    ui->setupUi (this);
    settings = new QSettings("settings.ini", QSettings::IniFormat, this);
    createUI();//Настройка окна
    setupPlot();//Настройка графика

    // Инициализация трекера (перекрестия)
    tracer = new QCPItemTracer(ui->plot);
    tracer->setStyle(QCPItemTracer::tsCircle);
    tracer->setPen(QPen(Qt::red, 2));
    tracer->setBrush(Qt::red);
    tracer->setSize(8);
    tracer->setVisible(false);

    // Текстовая метка для трекера - рядом с маркером
    tracerLabel = new QCPItemText(ui->plot);
    tracerLabel->setLayer("overlay");
    tracerLabel->setPen(QPen(Qt::black, 1));
    tracerLabel->setColor(Qt::black);
    tracerLabel->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);
    tracerLabel->setFont(QFont("Arial", 9));
    tracerLabel->setBrush(QBrush(QColor(255, 255, 255, 200)));
    tracerLabel->setPadding(QMargins(6, 3, 6, 3));
    tracerLabel->setVisible(false);

    // Вертикальная линия перекрестия
    tracerLineX = new QCPItemLine(ui->plot);
    tracerLineX->setLayer("overlay");
    tracerLineX->setPen(QPen(Qt::gray, 1, Qt::DashLine));
    tracerLineX->setVisible(false);

    // Горизонтальная линия перекрестия
    tracerLineY = new QCPItemLine(ui->plot);
    tracerLineY->setLayer("overlay");
    tracerLineY->setPen(QPen(Qt::gray, 1, Qt::DashLine));
    tracerLineY->setVisible(false);

    connect (ui->plot, SIGNAL (mouseMove (QMouseEvent*)), this, SLOT (onMouseMoveInPlot (QMouseEvent*)));
    connect (ui->plot, SIGNAL(selectionChangedByUser()), this, SLOT(channel_selection()));
    connect (ui->plot, SIGNAL(legendDoubleClick (QCPLegend*, QCPAbstractLegendItem*, QMouseEvent*)), this, SLOT(legend_double_click (QCPLegend*, QCPAbstractLegendItem*, QMouseEvent*)));
    connect (&updateTimer, SIGNAL (timeout()), this, SLOT (replot()));
    m_csvFile = nullptr;
}

MainWindow::~MainWindow(){
    closeCsvFile();

    if (pSerial != nullptr){
        pMyThread->exit();
        delete pSerial;
        delete pMyThread;
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
    ui->actionDisconnect->setEnabled(false);
    ui->actionPause_Plot->setEnabled(false);
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

    /*Настройка оси X - ОТОБРАЖЕНИЕ ВРЕМЕНИ*/
    ui->plot->xAxis->grid()->setZeroLinePen(QPen(gui_colors[1], 1, Qt::DotLine));
    ui->plot->xAxis->grid()->setPen (QPen(gui_colors[1], 1, Qt::DotLine));
    ui->plot->xAxis->grid()->setSubGridPen (QPen(gui_colors[1], 1, Qt::DotLine));
    ui->plot->xAxis->setBasePen (QPen (gui_colors[2]));
    ui->plot->xAxis->setTickPen (QPen (gui_colors[2]));
    ui->plot->xAxis->setSubTickPen (QPen (gui_colors[2]));
    ui->plot->xAxis->setUpperEnding (QCPLineEnding::esSpikeArrow);
    ui->plot->xAxis->setTickLabelColor (gui_colors[2]);
    ui->plot->xAxis->setTickLabelFont (font);

    /*Настройка оси X - используем кастомный форматтер*/
    ui->plot->xAxis->setLabel("Время");
    ui->plot->xAxis->setNumberFormat("f");
    ui->plot->xAxis->setNumberPrecision(0);

    // Устанавливаем кастомный тикер для отображения времени
    QSharedPointer<QCPAxisTickerText> timeTicker(new QCPAxisTickerText);
    ui->plot->xAxis->setTicker(timeTicker);

    scale_setting();

    /*Настройка оси Y*/
    ui->plot->yAxis->grid()->setZeroLinePen(QPen(gui_colors[1], 1, Qt::DotLine));
    ui->plot->yAxis->grid()->setPen (QPen(gui_colors[1], 1, Qt::DotLine));
    ui->plot->yAxis->grid()->setSubGridPen (QPen(gui_colors[1], 1, Qt::DotLine));
    ui->plot->yAxis->setBasePen (QPen (gui_colors[2]));
    ui->plot->yAxis->setTickPen (QPen (gui_colors[2]));
    ui->plot->yAxis->setSubTickPen (QPen (gui_colors[2]));
    ui->plot->yAxis->setUpperEnding (QCPLineEnding::esSpikeArrow);
    ui->plot->yAxis->setTickLabelColor (gui_colors[2]);
    ui->plot->yAxis->setTickLabelFont (font);

    /* Легенда */
    QFont legendFont;
    legendFont.setPointSize (8);
    ui->plot->legend->setFont (legendFont);
    ui->plot->legend->setBrush (gui_colors[3]);
    ui->plot->legend->setBorderPen (gui_colors[1]);
    ui->plot->axisRect()->insetLayout()->setInsetAlignment (0, Qt::AlignTop|Qt::AlignRight);

    ui->action_visible_legend->setChecked(true);
    on_action_visible_legend_triggered(true);
    ui->action_visible_legend->setChecked(settings->value("Legend", "true").toBool());
    on_action_visible_legend_triggered(settings->value("Legend", "true").toBool());

    // Инициализация ползунка
    ui->horizontalScrollBar->setValue(0);
    ui->horizontalScrollBar->setMaximum(0);
    Horizontal_scroll_value = 0;
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
void MainWindow::openPort (int baudRate, QSerialPort::DataBits dataBits, QSerialPort::Parity parity, QSerialPort::StopBits stopBits){
    pSerial = new serialthreaded();
    connect (pSerial, SIGNAL(portOpenOK()), this, SLOT(portOpenedSuccess()));
    connect (pSerial, SIGNAL(portOpenFail()), this, SLOT(portOpenedFail()));

    pSerial -> setPortName(ui->comboPort->currentText());
    pSerial->setBaudRate(baudRate);
    pSerial->setDataBits(dataBits);
    pSerial->setParity(parity);
    pSerial->setStopBits(stopBits);

    pMyThread = new QThread;
    pSerial->moveToThread(pMyThread);
    connect(pSerial, SIGNAL(readyRead()), pSerial, SLOT(serialRecieve()));
    connect(pSerial, SIGNAL(emitData(QByteArray)), this, SLOT(update(QByteArray)));
    connect(pMyThread, SIGNAL(started()), pSerial, SLOT(open_port()));
    pMyThread->start(QThread::InheritPriority);

    connect (this, SIGNAL(portClosed()), this, SLOT(onPortClosed()));
    connect (this, SIGNAL(newData(QStringList)), this, SLOT(onNewDataArrived(QStringList)));
    connect (this, SIGNAL(newData(QStringList)), this, SLOT(saveStream(QStringList)));
}

/*Закрыть сессию работы с COM портом*/
void MainWindow::onPortClosed(){
    updateTimer.stop();
    connected = false;
    plotting = false;

    closeCsvFile();

    disconnect (pSerial, SIGNAL(readyRead()), pSerial, SLOT(serialRecieve()));
    disconnect (pSerial, SIGNAL(portOpenOK()), this, SLOT(portOpenedSuccess()));
    disconnect (pSerial, SIGNAL(portOpenFail()), this, SLOT(portOpenedFail()));
    disconnect (this, SIGNAL(portClosed()), this, SLOT(onPortClosed()));
    disconnect (this, SIGNAL(newData(QStringList)), this, SLOT(onNewDataArrived(QStringList)));
    disconnect (this, SIGNAL(newData(QStringList)), this, SLOT(saveStream(QStringList)));

    ui->pushButton->setEnabled(true);
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
    ui->pushButton->setEnabled(false);
}

/*Если невозможно подключиться к COM порту*/
void MainWindow::portOpenedFail(){
    onPortClosed();
    ui->PortControlsBox->setVisible(true);
    ui->statusBar->showMessage ("Невозможно подключиться к COM порту!");
    ui->pushButton->setEnabled(true);
    on_pushButton_clicked();
}

/*Функция перерисовки графика. Обновление страницы.*/
void MainWindow::replot(){
    /*Если автомасштаб включен*/
    if (flag_Autoscale){
        ui->plot->yAxis->rescale(true);

        // Добавляем запас 10% сверху и снизу
        double lower = ui->plot->yAxis->range().lower;
        double upper = ui->plot->yAxis->range().upper;
        double margin = (upper - lower) * 0.1;

        ui->plot->yAxis->setRange(lower - margin, upper + margin);
    }
    scale_setting();
    updateTimeTicks();
    ui->plot->replot();
}

/*Обновление меток времени на оси X - с защитой от наложения*/
void MainWindow::updateTimeTicks(){
    if (ui->plot->graphCount() > 0 && ui->plot->graph(0)->data()->size() > 0) {
        QSharedPointer<QCPAxisTickerText> timeTicker = ui->plot->xAxis->ticker().staticCast<QCPAxisTickerText>();

        // Очищаем старые метки
        timeTicker->clear();

        // Получаем диапазон оси X
        double rangeMin = ui->plot->xAxis->range().lower;
        double rangeMax = ui->plot->xAxis->range().upper;
        double diff = rangeMax - rangeMin;

        // Получаем ширину графика в пикселях
        int plotWidth = ui->plot->axisRect()->width();
        if (plotWidth <= 0) {
            plotWidth = 800; // Значение по умолчанию
        }

        // Рассчитываем оптимальное количество меток (не более 1 метки на 80 пикселей)
        int maxLabels = plotWidth / 80;
        if (maxLabels < 2) maxLabels = 2;
        if (maxLabels > 15) maxLabels = 15;

        // Определяем шаг в зависимости от диапазона и ширины
        double step = 1;

        if (diff > 0) {
            // Список возможных шагов (в секундах)
            QVector<double> possibleSteps = {1, 2, 5, 10, 15, 20, 30, 60, 120, 300, 600, 900, 1800, 3600, 7200, 14400, 28800, 86400};

            for (double s : possibleSteps) {
                if (diff / s <= maxLabels) {
                    step = s;
                    break;
                }
            }

            // Если ни один шаг не подошел, берем последний
            if (step == 1 && diff / 1 > maxLabels) {
                step = possibleSteps.last();
            }
        }

        // Добавляем метки времени с единым форматом hh:mm:ss
        double start = ceil(rangeMin / step) * step;
        for (double t = start; t <= rangeMax; t += step) {
            QDateTime dateTime = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(t));
            QString label = dateTime.toString("hh:mm:ss");
            timeTicker->addTick(t, label);
        }
    }
}

/*Функция по составлению графика из полученных данных*/
void MainWindow::onNewDataArrived(QStringList newData){
    static int data_members = 0;
    static int channel = 0;
    static int i = 0;

    // Устанавливаем время начала при первом получении данных
    if (startTime == 0) {
        startTime = QDateTime::currentMSecsSinceEpoch();
    }

    if (plotting){
        data_members = newData.size();

        if (flag_automatic_cnt_channels){
            ui->channel_count->setValue(data_members);
        }

        if (data_members <= ui->channel_count->value()){
            // Получаем текущий timestamp в секундах
            qint64 currentTimeMs = QDateTime::currentMSecsSinceEpoch();
            double realTimeStamp = currentTimeMs / 1000.0;
            double timeInSeconds = (currentTimeMs - startTime) / 1000.0;
            lastTime = timeInSeconds;

            for (i = 0; i < data_members; i++){
                while (ui->plot->plottableCount() <= channel){
                    ui->plot->addGraph();
                    QPen GraphY1;
                    GraphY1.setWidth(Width_pen_graph);
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

                // Используем реальный timestamp
                ui->plot->graph(channel)->addData(realTimeStamp, newData[channel].toFloat());
                channel++;
            }
            dataPointNumber++;
            channel = 0;

            // Обновляем максимальное значение ползунка - используем относительное время
            if (dataPointNumber > 0) {
               ui->horizontalScrollBar->setMaximum(static_cast<qint64>(timeInSeconds) + 10);
            }

            // Обновляем диапазон оси X в зависимости от режима
            if (flag_Graph_moove) {
                if (realTimeStamp > x_scale_value + startTime/1000.0) {
                    ui->plot->xAxis->setRange(realTimeStamp - x_scale_value, realTimeStamp);
                } else {
                    ui->plot->xAxis->setRange(startTime/1000.0, startTime/1000.0 + x_scale_value);
                }
            } else {
                // Для статичного режима - ползунок работает в относительном времени
                double startTimeStamp = startTime / 1000.0;
                if (realTimeStamp > startTimeStamp + x_scale_value + Horizontal_scroll_value) {
                    ui->plot->xAxis->setRange(startTimeStamp + Horizontal_scroll_value,
                                             startTimeStamp + x_scale_value + Horizontal_scroll_value);
                } else {
                    ui->plot->xAxis->setRange(startTimeStamp, startTimeStamp + x_scale_value);
                }
            }

            if (flag_Autoscale) {
                ui->plot->yAxis->rescale(true);

                // Добавляем запас 10% сверху и снизу
                double lower = ui->plot->yAxis->range().lower;
                double upper = ui->plot->yAxis->range().upper;
                double margin = (upper - lower) * 0.1;
                ui->plot->yAxis->setRange(lower - margin, upper + margin);
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

/*Отслеживание координат на графике при вождении мыши с перекрестием*/
void MainWindow::onMouseMoveInPlot(QMouseEvent *event){
    double xCoord = ui->plot->xAxis->pixelToCoord(event->pos().x());
    double yCoord = ui->plot->yAxis->pixelToCoord(event->pos().y());

    // Проверяем, есть ли данные на графике
    if (ui->plot->graphCount() > 0 && ui->plot->graph(0)->data()->size() > 0) {
        QCPGraph *graph = ui->plot->graph(0);

        // Ищем ближайшую точку к позиции курсора
        QCPGraphDataContainer::const_iterator it = graph->data()->findBegin(xCoord);

        if (it != graph->data()->constEnd()) {
            // Берем точку слева
            double closestKey = it->key;
            double closestValue = it->value;
            double minDist = qAbs(xCoord - closestKey);

            // Проверяем точку справа
            auto itNext = it;
            ++itNext;
            if (itNext != graph->data()->constEnd()) {
                double distNext = qAbs(xCoord - itNext->key);
                if (distNext < minDist) {
                    closestKey = itNext->key;
                    closestValue = itNext->value;
                    minDist = distNext;
                }
            }

            // Проверяем точку слева
            if (it != graph->data()->constBegin()) {
                auto itPrev = it;
                --itPrev;
                double distPrev = qAbs(xCoord - itPrev->key);
                if (distPrev < minDist) {
                    closestKey = itPrev->key;
                    closestValue = itPrev->value;
                }
            }

            // Обновляем трекер
            tracer->setGraph(graph);
            tracer->setGraphKey(closestKey);
            tracer->setVisible(true);

            // Обновляем метку с данными - рядом с маркером
            QDateTime dateTime = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(closestKey));
            QString timeStr = dateTime.toString("hh:mm:ss");
            tracerLabel->setText(QString("Время: %1\nЗначение: %2").arg(timeStr).arg(closestValue, 0, 'f', 2));

            // Вычисляем позицию для метки (смещение вправо-вверх от точки)
            double offsetX = (ui->plot->xAxis->range().upper - ui->plot->xAxis->range().lower) * 0.02;
            double offsetY = (ui->plot->yAxis->range().upper - ui->plot->yAxis->range().lower) * 0.02;

            double labelX = closestKey + offsetX;
            double labelY = closestValue + offsetY;

            // Проверяем, не выходит ли метка за правую границу
            if (labelX > ui->plot->xAxis->range().upper) {
                labelX = closestKey - offsetX - (ui->plot->xAxis->range().upper - ui->plot->xAxis->range().lower) * 0.15;
                tracerLabel->setPositionAlignment(Qt::AlignRight | Qt::AlignTop);
            } else {
                tracerLabel->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);
            }

            // Проверяем, не выходит ли метка за верхнюю границу
            if (labelY > ui->plot->yAxis->range().upper) {
                labelY = closestValue - offsetY - (ui->plot->yAxis->range().upper - ui->plot->yAxis->range().lower) * 0.05;
            }

            tracerLabel->position->setCoords(labelX, labelY);
            tracerLabel->setVisible(true);

            // Обновляем линии перекрестия
            tracerLineX->start->setCoords(closestKey, ui->plot->yAxis->range().lower);
            tracerLineX->end->setCoords(closestKey, ui->plot->yAxis->range().upper);
            tracerLineX->setVisible(true);

            tracerLineY->start->setCoords(ui->plot->xAxis->range().lower, closestValue);
            tracerLineY->end->setCoords(ui->plot->xAxis->range().upper, closestValue);
            tracerLineY->setVisible(true);

            ui->plot->replot(QCustomPlot::rpQueuedReplot);
        }
    } else {
        // Если данных нет - скрываем трекер
        tracer->setVisible(false);
        tracerLabel->setVisible(false);
        tracerLineX->setVisible(false);
        tracerLineY->setVisible(false);
        ui->plot->replot(QCustomPlot::rpQueuedReplot);
    }

    // Обновляем статус-бар
    QDateTime dateTime = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(xCoord));
    QString timeStr = dateTime.toString("hh:mm:ss");
    QString coordinates("Время: %1  Значение: %2");
    coordinates = coordinates.arg(timeStr).arg(yCoord, 0, 'f', 1);
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

/*Отображение по X. Сколько секунд показывать. График бегущий или стоячий*/
void MainWindow::scale_setting(){
    if (flag_Graph_moove){
        if (dataPointNumber > 0) {
            qint64 currentTimeMs = QDateTime::currentMSecsSinceEpoch();
            double realTimeStamp = currentTimeMs / 1000.0;
            double startTimeStamp = startTime / 1000.0;

            if (realTimeStamp > startTimeStamp + x_scale_value) {
                ui->plot->xAxis->setRange(realTimeStamp - x_scale_value, realTimeStamp);
            } else {
                ui->plot->xAxis->setRange(startTimeStamp, startTimeStamp + x_scale_value);
            }
        } else {
            ui->plot->xAxis->setRange(0, x_scale_value);
        }
    } else {
        if (dataPointNumber > 0) {
            double startTimeStamp = startTime / 1000.0;

            // Проверяем валидность Horizontal_scroll_value
            qint64 currentTimeMs = QDateTime::currentMSecsSinceEpoch();
            double realTimeStamp = currentTimeMs / 1000.0;
            int maxStart = static_cast<int>(realTimeStamp - startTimeStamp) - x_scale_value;
            if (maxStart < 0) maxStart = 0;
            if (Horizontal_scroll_value > maxStart) {
                Horizontal_scroll_value = maxStart;
                ui->horizontalScrollBar->setValue(Horizontal_scroll_value);
            }
            ui->plot->xAxis->setRange(startTimeStamp + Horizontal_scroll_value,
                                     startTimeStamp + x_scale_value + Horizontal_scroll_value);
        } else {
            ui->plot->xAxis->setRange(0, x_scale_value);
        }
    }
}

/*Окно помощь -> о программе*/
void MainWindow::on_actionHow_to_use_triggered(){
    about = new About (this);
    about->setWindowTitle ("О программе");
    about->setWindowFlags(Qt::Dialog| Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowSystemMenuHint);
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

        openPort (baudRate, dataBits, parity, stopBits);
    }
}

/*Нажатие кнопки Пауза*/
void MainWindow::on_actionPause_Plot_triggered(){
    if (plotting){
        updateTimer.stop();
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
        pMyThread->exit();
        pSerial->close();
        emit portClosed();
        delete pSerial;
        delete pMyThread;
        pSerial = nullptr;
        ui->statusBar->showMessage ("COM порт отключен!");
        connected = false;
        ui->actionConnect->setEnabled (true);
        plotting = false;
        ui->actionPause_Plot->setEnabled (false);
        ui->actionDisconnect->setEnabled (false);
        ui->actionRecord_stream->setEnabled(true);
        receivedData.clear();
        startTime = 0;
        lastTime = 0;
        enable_com_controls (true);
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
    startTime = 0;
    lastTime = 0;

    // Скрываем трекер
    tracer->setVisible(false);
    tracerLabel->setVisible(false);
    tracerLineX->setVisible(false);
    tracerLineY->setVisible(false);

    // Сброс ползунка
    ui->horizontalScrollBar->setValue(0);
    ui->horizontalScrollBar->setMaximum(0);
    Horizontal_scroll_value = 0;

    ui->plot->replot();
}

/*Создание *.csv файла*/
void MainWindow::openCsvFile(void){
    // Если файл уже открыт - закрываем его
    if (m_csvFile) {
        closeCsvFile();
    }

    QString Text = "record_input_data_" + QDateTime::currentDateTime().toString("HH.mm.ss_d.MM.yyyy") + ".csv";
    m_csvFile = new QFile(Text);
    if(!m_csvFile)
        return;
    if (!m_csvFile->open(QIODevice::ReadWrite | QIODevice::Text))
        return;

    // Записываем заголовок CSV
    QTextStream out(m_csvFile);
    out << "ID,Channel,Date/Time,Value\n";
    out.flush();

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
        // Если файл больше 10 МБ - создаем новый
        if (m_csvFile->size() > 10 * 1024 * 1024) {
            closeCsvFile();
            openCsvFile();
        }

        if (dataPointNumber > 0){
            QTextStream out(m_csvFile);

            // Получаем текущее время
            QString currentTime = QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss.zzz");

            // Для каждого канала записываем строку
            for (int i = 0; i < newData.size(); i++) {
                // Номер канала
                int channelNumber = i;

                // Имя канала из легенды
                QString channelName;
                if (i < ui->plot->graphCount()) {
                    channelName = ui->plot->graph(i)->name();
                } else {
                    channelName = QString("Канал данных %1").arg(i);
                }

                // Значение
                QString value = newData[i];

                // Записываем строку: Канал,Имя канала,Дата_и_время,Значение
                out << channelNumber << "," << channelName << "," << currentTime << "," << value << "\n";
            }

            // Сбрасываем буфер на диск (опционально, для надежности)
            out.flush();
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

    double lower = ui->plot->yAxis->range().lower;
    double upper = ui->plot->yAxis->range().upper;
    double range = upper - lower;

    // Если диапазон очень маленький, добавляем фиксированный запас
    if (range < 0.1) {
        ui->plot->yAxis->setRange(lower - 0.5, upper + 0.5);
    } else {
        // Иначе добавляем 8% запас
        double margin = range * 0.08;
        ui->plot->yAxis->setRange(lower - margin, upper + margin);
    }

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
        item->setBackground(QColor (205, 34,  46,  100));
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

    // Обновляем только если не в режиме бегущего графика
    if (!flag_Graph_moove) {
        if (dataPointNumber > 0 && startTime > 0) {
            double startTimeStamp = startTime / 1000.0;
            qint64 currentTimeMs = QDateTime::currentMSecsSinceEpoch();
            double realTimeStamp = currentTimeMs / 1000.0;

            int maxStart = static_cast<int>(realTimeStamp - startTimeStamp) - x_scale_value;
            if (maxStart < 0) maxStart = 0;

            if (value > maxStart) {
                value = maxStart;
                ui->horizontalScrollBar->setValue(value);
                Horizontal_scroll_value = value;
            }
        }

        if (startTime > 0) {
            double startTimeStamp = startTime / 1000.0;
            ui->plot->xAxis->setRange(startTimeStamp + Horizontal_scroll_value,
                                     startTimeStamp + x_scale_value + Horizontal_scroll_value);
            if (!updateTimer.isActive()){
                replot();
            }
        }
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
    flag_Graph_moove = checked;

    if (flag_Graph_moove){
        ui->plot->setInteraction (QCP::iRangeDrag, false);
        // При включении бегущего режима - ползунок в конец
        if (dataPointNumber > 0 && startTime > 0) {
            qint64 currentTimeMs = QDateTime::currentMSecsSinceEpoch();
            double timeFromStart = (currentTimeMs - startTime) / 1000.0;
            ui->horizontalScrollBar->setValue(static_cast<int>(timeFromStart));
            Horizontal_scroll_value = static_cast<int>(timeFromStart);
        }
    } else {
        ui->plot->setInteraction (QCP::iRangeDrag, false);
        // При выключении бегущего режима - ползунок на последние данные
        if (dataPointNumber > 0 && startTime > 0) {
            qint64 currentTimeMs = QDateTime::currentMSecsSinceEpoch();
            double timeFromStart = (currentTimeMs - startTime) / 1000.0;

            if (timeFromStart > x_scale_value) {
                int newPos = static_cast<int>(timeFromStart) - x_scale_value;
                ui->horizontalScrollBar->setValue(newPos);
                Horizontal_scroll_value = newPos;
            } else {
                ui->horizontalScrollBar->setValue(0);
                Horizontal_scroll_value = 0;
            }
        } else {
            ui->horizontalScrollBar->setValue(0);
            Horizontal_scroll_value = 0;
        }
    }

    if (!updateTimer.isActive()){
        replot();
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

/*Выбор, сколько секунд по оси Х будем отображать*/
void MainWindow::on_spinBox_valueChanged(int arg1){
    x_scale_value = arg1;
    ui->plot->xAxis->setLabel(QString("Время (показано %1 с)").arg(arg1));

    if (!updateTimer.isActive()){
        replot();
    }
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

/*Обработка приходящих данных*/
void MainWindow::update(QByteArray Data){
    QByteArray data = Data;
    if(!data.isEmpty()) {
        unsigned char *temp = (unsigned char*)data.data();

        for(int i = 0; temp[i] != '\0'; i++) {
            switch(STATE) {
            case WAIT_START:
                if(temp[i] == START_MSG) {
                    STATE = IN_MESSAGE;
                    receivedData.clear();
                    break;
                }
                break;
            case IN_MESSAGE:
                if(temp[i] == END_MSG) {
                    STATE = WAIT_START;
                    QStringList incomingData = receivedData.split(' ');
                    if(filterDisplayedData){
                        ui->textEdit_UartWindow->clear();
                        for(int i = 0; i<incomingData.size(); i++){
                            // Получаем имя канала из легенды графика
                            QString channelName;
                            if (i < ui->plot->graphCount()) {
                                channelName = ui->plot->graph(i)->name();
                            } else {
                                channelName = QString("Канал данных %1").arg(i);
                            }
                            ui->textEdit_UartWindow->append(channelName + ": " + incomingData[i]);
                        }
                    }
                    emit newData(incomingData);
                    Data_count = incomingData.count();
                    break;
                }else if (isdigit (temp[i]) || temp[i] == ' ' || temp[i] =='-' || temp[i] =='.'){
                    receivedData.append(temp[i]);
                }
                break;
            default: break;
            }
        }
    }
}

/*Сохранить слепок данных*/
void MainWindow::on_saveSnapshotAction_triggered(){
    if (ui->plot->graphCount() == 0) {
        QMessageBox::warning(this, "Предупреждение", "Нет данных для сохранения!");
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(this,
        "Сохранить слепок данных",
        QDir::homePath() + "/snapshot_" + QDateTime::currentDateTime().toString("HH.mm.ss_d.MM.yyyy") + ".dat",
        "Snapshot Files (*.dat)");

    if (!filePath.isEmpty()) {
        saveSnapshot(filePath);
    }
}

/*Загрузить слепок данных*/
void MainWindow::on_openSnapshotAction_triggered(){
    QString filePath = QFileDialog::getOpenFileName(this,
        "Загрузить слепок данных",
        QDir::homePath(),
        "Snapshot Files (*.dat)");

    if (!filePath.isEmpty()) {
        loadSnapshot(filePath);
    }
}

/*Сохранить слепок в бинарный файл*/
void MainWindow::saveSnapshot(const QString &filePath){
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось создать файл!");
        return;
    }

    QDataStream out(&file);
    out.setVersion(QDataStream::Qt_5_15);

    // Магическое число для идентификации формата
    quint32 magic = 0x534E4150; // "SNAP" в hex
    out << magic;

    // Версия формата
    quint32 version = 1;
    out << version;

    // Количество каналов
    int channelCount = ui->plot->graphCount();
    out << channelCount;

    // Сохраняем имена каналов и данные
    for (int i = 0; i < channelCount; i++) {
        QString name = ui->plot->graph(i)->name();
        out << name;

        // Получаем данные графика
        QSharedPointer<QCPGraphDataContainer> data = ui->plot->graph(i)->data();
        int pointCount = data->size();
        out << pointCount;

        // Сохраняем точки (time, value)
        for (const QCPGraphData &point : *data) {
            out << point.key;   // время
            out << point.value; // значение
        }
    }

    // Сохраняем метаданные
    out << startTime;
    out << lastTime;
    out << dataPointNumber;
    out << channels;

    // Сохраняем диапазоны осей для восстановления
    out << ui->plot->xAxis->range().lower;
    out << ui->plot->xAxis->range().upper;
    out << ui->plot->yAxis->range().lower;
    out << ui->plot->yAxis->range().upper;

    file.close();

    ui->statusBar->showMessage("Слепок сохранен: " + filePath + " (" +
                               QString::number(channelCount) + " каналов, " +
                               QString::number(dataPointNumber) + " точек)");
}

/*Загрузить слепок из бинарного файла*/
void MainWindow::loadSnapshot(const QString &filePath){
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось открыть файл!");
        return;
    }

    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_5_15);

    // Проверяем магическое число
    quint32 magic;
    in >> magic;
    if (magic != 0x534E4150) {
        QMessageBox::warning(this, "Ошибка", "Неверный формат файла!");
        file.close();
        return;
    }

    // Проверяем версию
    quint32 version;
    in >> version;
    if (version != 1) {
        QMessageBox::warning(this, "Ошибка", "Неподдерживаемая версия файла!");
        file.close();
        return;
    }

    // Очищаем текущие данные
    clearPlot();

    // Читаем количество каналов
    int channelCount;
    in >> channelCount;

    QVector<QString> channelNames;
    QVector<QVector<double>> allValues;
    QVector<double> allTime;

    // Читаем данные каждого канала
    for (int i = 0; i < channelCount; i++) {
        QString name;
        in >> name;
        channelNames.append(name);

        int pointCount;
        in >> pointCount;

        QVector<double> values;
        QVector<double> times;

        for (int j = 0; j < pointCount; j++) {
            double key, value;
            in >> key >> value;
            times.append(key);
            values.append(value);
        }

        allValues.append(values);
        allTime = times;
    }

    // Читаем метаданные
    qint64 loadedStartTime;
    double loadedLastTime;
    int loadedDataPointNumber;
    int loadedChannels;
    in >> loadedStartTime;
    in >> loadedLastTime;
    in >> loadedDataPointNumber;
    in >> loadedChannels;

    // Читаем диапазоны осей
    double xRangeLower, xRangeUpper;
    double yRangeLower, yRangeUpper;
    in >> xRangeLower;
    in >> xRangeUpper;
    in >> yRangeLower;
    in >> yRangeUpper;

    file.close();

    if (allValues.isEmpty() || allValues[0].isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Файл не содержит данных!");
        return;
    }

    // Восстанавливаем состояние
    startTime = loadedStartTime;
    lastTime = loadedLastTime;
    dataPointNumber = loadedDataPointNumber;
    channels = loadedChannels;

    // Настраиваем ось X для отображения времени (как в setupPlot)
    ui->plot->xAxis->setLabel("Время");
    ui->plot->xAxis->setNumberFormat("f");
    ui->plot->xAxis->setNumberPrecision(0);

    // Устанавливаем кастомный тикер для отображения времени
    QSharedPointer<QCPAxisTickerText> timeTicker(new QCPAxisTickerText);
    ui->plot->xAxis->setTicker(timeTicker);

    // Строим графики
    for (int i = 0; i < channelCount; i++) {
        ui->plot->addGraph();
        QPen pen;
        pen.setWidth(Width_pen_graph);
        pen.setColor(line_colors[i % CUSTOM_LINE_COLORS]);
        ui->plot->graph(i)->setPen(pen);

        QString name = channelNames.value(i, QString("Канал %1").arg(i));
        ui->plot->graph(i)->setName(name);

        // Добавляем данные
        ui->plot->graph(i)->setData(allTime, allValues[i]);

        // Обновляем список каналов
        if (ui->plot->legend->item(i)) {
            ui->plot->legend->item(i)->setTextColor(line_colors[i % CUSTOM_LINE_COLORS]);
        }
        ui->listWidget_Channels->addItem(name);
        ui->listWidget_Channels->item(i)->setForeground(QBrush(line_colors[i % CUSTOM_LINE_COLORS]));
    }

    // Восстанавливаем диапазоны осей
    ui->plot->xAxis->setRange(xRangeLower, xRangeUpper);
    ui->plot->yAxis->setRange(yRangeLower, yRangeUpper);

    // Обновляем метки времени на оси X
    updateTimeTicks();

    // Перерисовываем
    ui->plot->replot();

    // Обновляем ползунок
    if (dataPointNumber > 0) {
        ui->horizontalScrollBar->setMaximum(dataPointNumber);
    }

    ui->statusBar->showMessage("Слепок загружен: " + filePath + " (" +
                               QString::number(channelCount) + " каналов, " +
                               QString::number(dataPointNumber) + " точек)");
}

/*Очистка графика*/
void MainWindow::clearPlot(){
    ui->plot->clearPlottables();
    ui->listWidget_Channels->clear();
    channels = 0;
    dataPointNumber = 0;
    startTime = 0;
    lastTime = 0;

    // Скрываем трекер
    tracer->setVisible(false);
    tracerLabel->setVisible(false);
    tracerLineX->setVisible(false);
    tracerLineY->setVisible(false);

    // Сброс ползунка
    ui->horizontalScrollBar->setValue(0);
    ui->horizontalScrollBar->setMaximum(0);
    Horizontal_scroll_value = 0;
}
