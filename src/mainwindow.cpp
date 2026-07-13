#include "mainwindow.hpp"
#include "ui_mainwindow.h"
#include <stdbool.h>
#include <QDebug>
#include <QTableWidgetItem>
#include <QDateTime>
#include <cmath>
#include <limits>

bool flag_Autoscale = false;
bool flag_Graph_moove = false;
bool flag_automatic_cnt_channels = false;
uint32_t x_scale_value = 500;
uint32_t Data_count = 0;
uint64_t Horizontal_scroll_value = 0;
bool flag_only_graph = false;
uint32_t Width_pen_graph = 1;
QString ComPortName;
bool Debug_visible = false;
uint16_t Scan_rate = 1000;
bool Process_RTU_WINAPI_state = false;
uint8_t RTU_WINAPI_STATE_PROCESS = RTU_WINAPI_STOPPED;
bool Dynamic_range_for_x = false;
bool flag_for_record_first_time = false; // Костыль для записи времени первого замера
QString filePath; //Файл, в который будем писать бинарник с данными
QFile m_dataFile;
QDataStream m_dataStream;
bool m_isDataFileOpen = false;
QString WindowTitle = "Serial port Plotter 2.0 by Solderingironspb";

/*Точность данных(до какого знака ограничиваем)*/
uint8_t precisionValues[8] = { 5, 5, 5, 5, 5, 5, 5, 5 }; // Вместо отдельных переменных
/*Точность данных(до какого знака ограничиваем)*/

//Структура шапки файла
#pragma pack(push, 1)
struct FileHeader {
    uint8_t channelCount;           // 1 байт
    struct ChannelInfo {
        char name[32];              // Название канала (31 символ + \0)
        char unit[16];              // Единица измерения (15 символов + \0)
    } channels[8];                  // Информация о 8 каналах
    // Итого: 1 + 8*(32+16) = 1 + 384 = 385 байт
    // Остается 639 байт для расширения
    char reserved[639];             // Оставшийся резерв
};
#pragma pack(pop)

bool isReadMode = false; // false - режим записи, true - режим чтения

enum{
    Theme_White,
    Theme_Dark
};

uint8_t Theme = Theme_White;

/*===========================Worker - работа с процессом RTU_WINAPI=================================*/
Worker::Worker(QObject *parent) :
    QObject(parent), m_process(nullptr), m_counter(0) {
    for (int i = 0; i < 8; ++i) {
        m_channel[i] = 0;
    }
}

Worker::~Worker() {
    if (m_process) {
        if (m_process->state() == QProcess::Running) {
            m_process->kill();
            m_process->waitForFinished(1000);
        }
        delete m_process;
    }
}

void Worker::startProcess(const QString &program, const QStringList &arguments) {
    if (m_process) {
        if (m_process->state() == QProcess::Running) {
            m_process->kill();
            m_process->waitForFinished(1000);
        }
        delete m_process;
        m_process = nullptr;
    }

    m_process = new QProcess();

    connect(m_process, &QProcess::started, this, &Worker::onProcessStarted);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Worker::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &Worker::onProcessError);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &Worker::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &Worker::onReadyReadStandardError);

    m_process->start(program, arguments);

    if (!m_process->waitForStarted(3000)) {
        qDebug() << "Ошибка запуска:" << m_process->errorString();
        errorReceived("Ошибка запуска:" + m_process->errorString());
        emit processStateChanged(false);
        Process_RTU_WINAPI_state = false;
    }
}

void Worker::stopProcess() {
    if (m_process && m_process->state() == QProcess::Running) {
        errorReceived("Остановка процесса...");
        m_process->kill();
    }
}

void Worker::stopProcessSafely() {
    if (!m_process)
        return;

    m_process->disconnect();

    if (m_process->state() == QProcess::Running) {
        qDebug() << "Остановка процесса (мягкое завершение)...";
        m_process->terminate();
        if (!m_process->waitForFinished(2000)) {
            qDebug() << "Процесс не завершился мягко, принудительное завершение...";
            m_process->kill();
            m_process->waitForFinished(1000);
        }
    }

    delete m_process;
    m_process = nullptr;
    qDebug() << "Процесс полностью остановлен";
}

void Worker::clearStats() {
    m_counter = 0;
    m_buffer.clear();
    for (int i = 0; i < 8; ++i) {
        m_channel[i] = 0;
    }
}

void Worker::onProcessStarted() {
    if (m_process) {
        qDebug() << "Процесс успешно запущен! PID:" << m_process->processId();
        errorReceived("Процесс успешно запущен! PID:" + QString::number(m_process->processId()));
        emit processStateChanged(true);
        Process_RTU_WINAPI_state = true;
        RTU_WINAPI_STATE_PROCESS = RTU_WINAPI_RUN;
        flag_for_record_first_time = false;
    }
}

void Worker::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    qDebug() << "========================";
    qDebug() << "Процесс завершен";
    qDebug() << "Код выхода:" << exitCode;
    qDebug() << "Статус:" << (exitStatus == QProcess::NormalExit ? "Нормальный" : "Аварийный");
    qDebug() << "Всего пакетов принято:" << m_counter;
    qDebug() << "========================";

    errorReceived(
                QString("\r\n========================\r\n") + "Процесс завершен\r\n" + "Код выхода:" + QString::number(exitCode) + "\r\n" + "Всего пакетов принято:" + QString::number(m_counter) + "\r\n"
                    + "========================");
    emit processStateChanged(false);
    Process_RTU_WINAPI_state = false;
}

void Worker::onProcessError(QProcess::ProcessError error) {
    qDebug() << "Ошибка процесса:" << error;
}

void Worker::onReadyReadStandardOutput() {
    if (!m_process)
        return;

    QByteArray data = m_process->readAllStandardOutput();
    m_buffer += data;
    parseData();
}

void Worker::onReadyReadStandardError() {
    if (!m_process)
        return;

    QByteArray data = m_process->readAllStandardError();
    QString error = QString::fromLocal8Bit(data);
    emit errorReceived(error);
}

void Worker::parseData() {
    while (true) {
        int startPos = m_buffer.indexOf("1 = ");
        if (startPos == -1) {
            startPos = m_buffer.indexOf("1=");
            if (startPos == -1) {
                break;
            }
        }

        int endPos = m_buffer.indexOf("1 = ", startPos + 2);
        if (endPos == -1) {
            endPos = m_buffer.indexOf("1=", startPos + 2);
        }

        if (endPos == -1) {
            QString packet = m_buffer.mid(startPos);

            int channelCount = 0;
            for (int i = 1; i <= 8; ++i) {
                if (packet.contains(QString::number(i) + " = ") || packet.contains(QString::number(i) + "=")) {
                    channelCount++;
                }
            }

            bool hasTime = packet.contains("9 = ") || packet.contains("9=") || packet.contains("Time = ") || packet.contains("Time=");

            if (channelCount >= 8 && hasTime) {
                bool hasValue = false;
                int lastPos = packet.lastIndexOf("8");
                if (lastPos != -1) {
                    QString after8 = packet.mid(lastPos + 1).trimmed();
                    if (after8.startsWith("=") || after8.startsWith(" = ")) {
                        QString value = after8.mid(after8.indexOf("=") + 1).trimmed();
                        if (!value.isEmpty()) {
                            for (QChar c : value) {
                                if (c.isDigit()) {
                                    hasValue = true;
                                    break;
                                }
                            }
                        }
                    }
                }

                bool hasTimeValue = false;
                int timePos = packet.indexOf("9 = ");
                if (timePos == -1)
                    timePos = packet.indexOf("9=");
                if (timePos == -1)
                    timePos = packet.indexOf("Time = ");
                if (timePos == -1)
                    timePos = packet.indexOf("Time=");

                if (timePos != -1) {
                    QString afterTime = packet.mid(timePos + 2).trimmed();
                    int eqPos = afterTime.indexOf("=");
                    if (eqPos != -1) {
                        QString timeValue = afterTime.mid(eqPos + 1).trimmed();
                        if (!timeValue.isEmpty()) {
                            hasTimeValue = true;
                        }
                    }
                }

                if (hasValue && hasTimeValue) {
                    processPacket(packet);
                    m_buffer.clear();
                } else {
                    break;
                }
            } else {
                break;
            }
        } else {
            QString packet = m_buffer.mid(startPos, endPos - startPos);
            m_buffer.remove(0, endPos);
            processPacket(packet);
        }
    }
}

void Worker::processPacket(const QString &packet) {
    QStringList lines = packet.split('\n', Qt::SkipEmptyParts);

    if (lines.size() != 9) {
        qDebug() << "НЕВАЛИДНЫЙ ПАКЕТ: не 9 строк, а" << lines.size();
        return;
    }

    bool packetValid = true;
    float ch[8] = { 0 };
    bool channelSeen[8] = { false };
    qint64 timestamp = 0;
    bool timestampFound = false;

    for (const QString &line : lines) {
        QString cleanLine = line.trimmed();
        if (cleanLine.isEmpty()) {
            packetValid = false;
            break;
        }

        if (cleanLine.startsWith("9") || cleanLine.startsWith("Time")) {
            int pos = cleanLine.indexOf("=");
            if (pos == -1) {
                qDebug() << "Нет '=' в строке времени:" << cleanLine;
                packetValid = false;
                break;
            }
            QString timeStr = cleanLine.mid(pos + 1).trimmed();
            timestamp = timeStr.toLongLong();
            timestampFound = true;
            continue;
        }

        int pos = cleanLine.indexOf(" = ");
        if (pos == -1) {
            pos = cleanLine.indexOf("=");
            if (pos == -1) {
                qDebug() << "Нет '=' в строке:" << cleanLine;
                packetValid = false;
                break;
            }
        }

        QString channelStr = cleanLine.left(pos).trimmed();
        QString valueStr = cleanLine.mid(pos + (cleanLine.indexOf("=") == pos ? 1 : 3)).trimmed();

        valueStr.replace(',', '.');

        bool channelOk, valueOk;
        int channel = channelStr.toInt(&channelOk);
        float value = valueStr.toFloat(&valueOk);

        if (!channelOk || !valueOk || channel < 1 || channel > 8) {
            qDebug() << "Ошибка парсинга: канал=" << channelStr << "(ok=" << channelOk << "), значение=" << valueStr << "(ok=" << valueOk << ")";
            packetValid = false;
            break;
        }

        if (channelSeen[channel - 1]) {
            qDebug() << "Повтор канала:" << channel;
            packetValid = false;
            break;
        }

        channelSeen[channel - 1] = true;
        ch[channel - 1] = value;
    }

    if (packetValid && timestampFound) {
        for (int i = 0; i < 8; ++i) {
            if (!channelSeen[i]) {
                qDebug() << "Отсутствует канал" << (i + 1);
                packetValid = false;
                break;
            }
        }
    } else {
        if (!timestampFound) {
            qDebug() << "Отсутствует временная метка";
        }
        packetValid = false;
    }

    if (packetValid) {
        for (int i = 0; i < 8; ++i) {
            m_channel[i] = ch[i];
        }
        m_counter++;

        emit dataReceived(m_channel[0], m_channel[1], m_channel[2], m_channel[3], m_channel[4], m_channel[5], m_channel[6], m_channel[7], m_counter, timestamp);

    } else {
        qDebug() << "НЕВАЛИДНЫЙ ПАКЕТ!";
        qDebug() << "Содержимое пакета:" << packet;
    }
}
/*===========================Worker - работа с процессом RTU_WINAPI=================================*/

/*===========================DataProcessor - работа с данными в отдельном потоке=================================*/
DataProcessor::DataProcessor(QObject *parent) :
    QObject(parent) {
}

void DataProcessor::processData(float ch1, float ch2, float ch3, float ch4, float ch5, float ch6, float ch7, float ch8, int counter, qint64 timestamp) {
    emit dataReceived(ch1, ch2, ch3, ch4, ch5, ch6, ch7, ch8, counter, timestamp);
}
/*===========================DataProcessor - работа с данными в отдельном потоке=================================*/

// ==================== MainWindow ====================
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent), startTime(0), lastTime(0), pSerial(nullptr), ui(new Ui::MainWindow),  connected(false), plotting(false), dataPointNumber(0), channels(0), hasNewData(false), needAxisUpdate(false),  channelListModel(nullptr),  m_workerThread(nullptr), m_worker(nullptr)  {

    ui->setupUi(this);

    /*==================Worker=================*/
    m_workerThread = new QThread(this);
    m_worker = new Worker();
    m_worker->moveToThread(m_workerThread);

    connect(this, &MainWindow::startProcess, m_worker, &Worker::startProcess);
    connect(this, &MainWindow::stopProcess, m_worker, &Worker::stopProcess);
    connect(this, &MainWindow::clearStats, m_worker, &Worker::clearStats);

    /*==================DataProcessor=================*/
    m_processorThread = new QThread(this);
    m_dataProcessor = new DataProcessor();
    m_dataProcessor->moveToThread(m_processorThread);

    connect(m_worker, &Worker::dataReceived, m_dataProcessor, &DataProcessor::processData);
    connect(m_dataProcessor, &DataProcessor::dataReceived, this, &MainWindow::updateData);

    connect(m_worker, &Worker::errorReceived, this, &MainWindow::updateError);
    connect(m_worker, &Worker::processStateChanged, this, &MainWindow::updateProcessState);

    m_workerThread->start();
    m_processorThread->start();
    /*==================DataProcessor=================*/

    settings = new QSettings("settings.ini", QSettings::IniFormat, this);
    createUI();
    setupPlot();

    setupTable();

    tracerLinesX.clear();
    tracerLinesY.clear();
    tracerLabels.clear();

    connect(ui->plot, SIGNAL(mouseMove(QMouseEvent*)), this, SLOT(onMouseMoveInPlot(QMouseEvent*)));
    connect(ui->plot, SIGNAL(selectionChangedByUser()), this, SLOT(channel_selection()));
    connect(&updateTimer, SIGNAL(timeout()), this, SLOT(replot()));

    // Подключаем сигнал изменения ячейки таблицы
    connect(ui->tableWidget, &QTableWidget::itemChanged, this, &MainWindow::onTableItemChanged);

    ui->plot->installEventFilter(this);


}

MainWindow::~MainWindow() {
    if (pSerial != nullptr) {
        pMyThread->exit();
        delete pSerial;
        delete pMyThread;
    }
    write_settings();

    /*==================Worker=================*/
    if (m_workerThread && m_workerThread->isRunning()) {
        disconnect(m_worker, nullptr, this, nullptr);
        disconnect(this, nullptr, m_worker, nullptr);

        if (m_worker) {
            m_worker->stopProcessSafely();
        }

        m_workerThread->quit();
        if (!m_workerThread->wait(3000)) {
            m_workerThread->terminate();
            m_workerThread->wait();
        }

        delete m_worker;
        delete m_workerThread;
        m_worker = nullptr;
        m_workerThread = nullptr;
    }
    /*==================Worker=================*/

    /*==================DataProcessor=================*/
    if (m_processorThread && m_processorThread->isRunning()) {
        m_processorThread->quit();
        if (!m_processorThread->wait(3000)) {
            m_processorThread->terminate();
            m_processorThread->wait();
        }
        delete m_dataProcessor;
        delete m_processorThread;
        m_dataProcessor = nullptr;
        m_processorThread = nullptr;
    }
    /*==================DataProcessor=================*/

    delete ui;
}

void MainWindow::setupTable() {
    if (Data_count == 0) {
        Data_count = 8;
        qDebug() << "Data_count был 0, установлено значение по умолчанию:" << Data_count;
    }

    if (Data_count > 8) {
        Data_count = 8;
        qDebug() << "Data_count > 8, установлено максимальное значение: 8";
    }

    qDebug() << "SetupTable: Data_count =" << Data_count;

    // Создаем временные массивы
    QLabel *labels[] = { ui->label_precision_ch1, ui->label_precision_ch2, ui->label_precision_ch3, ui->label_precision_ch4, ui->label_precision_ch5, ui->label_precision_ch6, ui->label_precision_ch7,
                         ui->label_precision_ch8 };

    QComboBox *combos[] = { ui->comboBox_precision_ch1, ui->comboBox_precision_ch2, ui->comboBox_precision_ch3, ui->comboBox_precision_ch4, ui->comboBox_precision_ch5, ui->comboBox_precision_ch6,
                            ui->comboBox_precision_ch7, ui->comboBox_precision_ch8 };

    for (int i = 0; i < 8; ++i) {
        bool visible = (i < (int)Data_count);

        if (labels[i] != nullptr) {
            labels[i]->setVisible(visible);
        }

        if (combos[i] != nullptr) {
            combos[i]->setVisible(visible);
        }
    }

    // Блокируем сигналы таблицы во время инициализации
    ui->tableWidget->blockSignals(true);

    ui->tableWidget->setColumnCount(9); //Количество столбцов
    ui->tableWidget->setRowCount(Data_count);

    QStringList headers;
    //headers << "        Имя        " << "Значение" << "Ед. измерения" << "   Пакет   " << "Дата начала замера" << "Время начала замера" << "Текущая дата" << "Текущее время" << "Описание";
    updateTableHeaders(isReadMode);

    ui->tableWidget->setHorizontalHeaderLabels(headers);

    ui->tableWidget->setColumnWidth(0, 80);
    ui->tableWidget->setColumnWidth(1, 150);

    ui->tableWidget->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

    for (int i = 0; i < (int)Data_count; ++i) {
        QString channelName = QString("Канал %1").arg(i + 1);
        QTableWidgetItem *item = new QTableWidgetItem(QString("Канал %1").arg(i + 1));
        ui->tableWidget->setItem(i, 0, item);

        if (i < 8 && labels[i] != nullptr) {
            labels[i]->setText(channelName);
            qDebug() << "Label" << i << "set to:" << channelName << "(during setupTable)";
        }

        int precision = precisionValues[i];
            QString formattedZero = QString::number(0.0, 'f', precision);

        QTableWidgetItem *valueItem = new QTableWidgetItem(formattedZero);
        //QTableWidgetItem *valueItem = new QTableWidgetItem("0");
        valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
        ui->tableWidget->setItem(i, 1, valueItem);
        valueItem->setTextAlignment(Qt::AlignCenter);

        QTableWidgetItem *unit_of_measurement = new QTableWidgetItem("---");
        ui->tableWidget->setItem(i, 2, unit_of_measurement);
        unit_of_measurement->setTextAlignment(Qt::AlignCenter);

        QTableWidgetItem *packet_counter = new QTableWidgetItem("---");
        packet_counter->setFlags(packet_counter->flags() & ~Qt::ItemIsEditable);
        ui->tableWidget->setItem(i, 3, packet_counter);
        packet_counter->setTextAlignment(Qt::AlignCenter);

        QTableWidgetItem *time_date_marker_start = new QTableWidgetItem("---");
        time_date_marker_start->setFlags(time_date_marker_start->flags() & ~Qt::ItemIsEditable);
        ui->tableWidget->setItem(i, 4, time_date_marker_start);
        time_date_marker_start->setTextAlignment(Qt::AlignCenter);

        QTableWidgetItem *time_marker_start = new QTableWidgetItem("---");
        time_marker_start->setFlags(time_marker_start->flags() & ~Qt::ItemIsEditable);
        ui->tableWidget->setItem(i, 5, time_marker_start);
        time_marker_start->setTextAlignment(Qt::AlignCenter);

        QTableWidgetItem *time_date_marker = new QTableWidgetItem("---");
        time_date_marker->setFlags(time_date_marker->flags() & ~Qt::ItemIsEditable);
        ui->tableWidget->setItem(i, 6, time_date_marker);
        time_date_marker->setTextAlignment(Qt::AlignCenter);

        QTableWidgetItem *time_marker = new QTableWidgetItem("---");
        time_marker->setFlags(time_marker->flags() & ~Qt::ItemIsEditable);
        ui->tableWidget->setItem(i, 7, time_marker);
        time_marker->setTextAlignment(Qt::AlignCenter);

        QTableWidgetItem *description = new QTableWidgetItem("");
        ui->tableWidget->setItem(i, 8, description);
    }

    ui->tableWidget->resizeColumnsToContents(); //Расширим ширину столбцов под содержимое
    ui->tableWidget->horizontalHeader()->setStretchLastSection(true); //Растянем последний столбец


    ui->tableWidget->resizeRowsToContents();

    // Устанавливаем высоту строк
    int rowHeight = 25;
    for (int i = 0; i < (int)Data_count; ++i) {
        ui->tableWidget->setRowHeight(i, rowHeight);
    }

    // Вычисляем общую высоту таблицы
    int totalHeight = 0;
    for (int i = 0; i < (int)Data_count; ++i) {
        totalHeight += ui->tableWidget->rowHeight(i);
    }
    totalHeight += ui->tableWidget->horizontalHeader()->height();
    totalHeight += ui->tableWidget->frameWidth() * 2;

    // Фиксируем высоту
    ui->tableWidget->setFixedHeight(totalHeight);

    // Разблокируем сигналы
    ui->tableWidget->blockSignals(false);

    qDebug() << "SetupTable завершена. Создано строк:" << Data_count;
}

void MainWindow::updateSpinBoxMax() {
    if (dataPointNumber > 0 && startTime > 0) {
        int maxValue = static_cast<int>(lastTime);
        if (maxValue < 1)
            maxValue = 1;

        ui->spinBox->setMaximum(maxValue);
        if (Dynamic_range_for_x) {
            ui->spinBox->setValue(maxValue);
        }

        if (ui->spinBox->value() > maxValue) {
            ui->spinBox->setValue(maxValue);
        }
    } else {
        ui->spinBox->setMaximum(1);
    }
}

void MainWindow::updateData(float ch1, float ch2, float ch3, float ch4, float ch5, float ch6, float ch7, float ch8, int counter, qint64 timestamp) {
    float channelValues[8] = { ch1, ch2, ch3, ch4, ch5, ch6, ch7, ch8 };

    QDateTime dt = QDateTime::fromMSecsSinceEpoch(timestamp);

    // Используем полный формат с датой и временем
    QString TimeMarker = dt.toString("dd.MM.yyyy hh:mm:ss.zzz");
    QStringList partsTimeMarker = TimeMarker.split(' ');
    QString date = partsTimeMarker[0];  // "dd.MM.yyyy"
    QString time = partsTimeMarker[1];  // "hh:mm:ss.zzz"
    //QString StartTime;

    if (flag_for_record_first_time == false) {
        // Сохраняем время первого замера в полном формате
        //StartTime = TimeMarker;
        flag_for_record_first_time = true;
        for (int i = 0; i < (int)Data_count && i < 8; ++i) {
            if (ui->tableWidget->item(i, 4)) {
                ui->tableWidget->item(i, 4)->setText(date);
            }

            if (ui->tableWidget->item(i, 5)) {
                ui->tableWidget->item(i, 5)->setText(time);
            }
        }
    }

    int rows = ui->tableWidget->rowCount();
    qDebug() << "UpdateData: rows in table =" << rows << ", Data_count =" << Data_count;

    for (int i = 0; i < (int)Data_count && i < 8; ++i) {
        if (ui->tableWidget->item(i, 1)) {
            int precision = precisionValues[i];
            ui->tableWidget->item(i, 1)->setText(QString::number(channelValues[i], 'f', precision));
        }

        if (ui->tableWidget->item(i, 3)) {
            ui->tableWidget->item(i, 3)->setText(QString::number(counter));
        }

        if (ui->tableWidget->item(i, 6)) {
            ui->tableWidget->item(i, 6)->setText(date);
        }
        if (ui->tableWidget->item(i, 7)) {
            ui->tableWidget->item(i, 7)->setText(time);
        }
    }

    plotting = true;

    if (startTime == 0) {
        startTime = timestamp;
    }

    if (plotting) {
        if (channels == 0) {
            for (int i = 0; i < (int)Data_count && i < 8; ++i) {
                ui->plot->addGraph();
                QPen pen;
                pen.setWidth(Width_pen_graph);
                pen.setColor(line_colors[i % CUSTOM_LINE_COLORS]);
                ui->plot->graph(i)->setPen(pen);

                QString graphName;
                if (i < ui->tableWidget->rowCount() && ui->tableWidget->item(i, 0)) {
                    graphName = ui->tableWidget->item(i, 0)->text();
                } else {
                    graphName = QString("Канал %1").arg(i + 1);
                }
                ui->plot->graph(i)->setName(graphName);

                // ===== НОВЫЙ КОД: Обновляем лейблы при создании графиков =====
                QLabel *labels[] = { ui->label_precision_ch1, ui->label_precision_ch2, ui->label_precision_ch3, ui->label_precision_ch4, ui->label_precision_ch5, ui->label_precision_ch6,
                                     ui->label_precision_ch7, ui->label_precision_ch8 };

                if (i < 8 && labels[i] != nullptr) {
                    labels[i]->setText(graphName);
                    qDebug() << "Label" << i << "set to:" << graphName << "(during graph creation)";
                }
                // ============================================================

                if (ui->plot->legend->item(i)) {
                    ui->plot->legend->item(i)->setTextColor(line_colors[i % CUSTOM_LINE_COLORS]);
                }

                ui->listWidget_Channels->addItem(ui->plot->graph(i)->name());
                ui->listWidget_Channels->item(i)->setForeground(QBrush(line_colors[i % CUSTOM_LINE_COLORS]));

                QCPItemLine *lineX = new QCPItemLine(ui->plot);
                lineX->setLayer("overlay");
                lineX->setPen(QPen(line_colors[i % CUSTOM_LINE_COLORS], 1, Qt::DashLine));
                lineX->setVisible(false);
                tracerLinesX.append(lineX);

                QCPItemLine *lineY = new QCPItemLine(ui->plot);
                lineY->setLayer("overlay");
                lineY->setPen(QPen(line_colors[i % CUSTOM_LINE_COLORS], 1, Qt::DashLine));
                lineY->setVisible(false);
                tracerLinesY.append(lineY);

                QCPItemText *label = new QCPItemText(ui->plot);
                label->setLayer("overlay");
                label->setFont(QFont("Arial", 8));
                label->setBrush(QBrush(QColor(255, 255, 255, 220)));
                label->setPadding(QMargins(4, 2, 4, 2));
                label->setVisible(false);
                tracerLabels.append(label);
            }
            channels = Data_count;
        }

        // Используем абсолютное время в секундах для оси X
        double timeInSeconds = timestamp / 1000.0;
        double relativeTime = (timestamp - startTime) / 1000.0;
        lastTime = relativeTime;

        for (int i = 0; i < (int)Data_count && i < 8; ++i) {
            if (i < ui->plot->graphCount()) {
                buffer_x[i].append(timeInSeconds);
                buffer_y[i].append(channelValues[i]);
            }
        }

        dataPointNumber++;

        bool bufferFull = (buffer_x[0].size() >= BUFFER_SIZE);

        if (bufferFull) {
            if (m_isDataFileOpen) {
                int pointsInBuffer = buffer_x[0].size();
                for (int p = 0; p < pointsInBuffer; ++p) {
                    for (int ch = 0; ch < (int)Data_count && ch < 8; ++ch) {
                        float value = static_cast<float>(buffer_y[ch][p]);
                        m_dataStream << value;
                    }
                    double time = buffer_x[0][p];
                    m_dataStream << time;
                }
            }

            for (int i = 0; i < (int)Data_count && i < 8; ++i) {
                if (i < ui->plot->graphCount() && !buffer_x[i].isEmpty()) {
                    ui->plot->graph(i)->addData(buffer_x[i], buffer_y[i]);
                    buffer_x[i].clear();
                    buffer_y[i].clear();
                }
            }
        }

        if (bufferFull) {
            hasNewData = true;
            needAxisUpdate = true;

            if (dataPointNumber > 0 && startTime > 0) {
                int maxScroll = static_cast<int>(lastTime - x_scale_value);
                if (maxScroll < 0)
                    maxScroll = 0;

                ui->horizontalScrollBar->setMaximum(maxScroll);
                ui->horizontalScrollBar->setPageStep(static_cast<int>(x_scale_value));
                ui->horizontalScrollBar->setSingleStep(10);

                if (flag_Graph_moove) {
                    ui->horizontalScrollBar->setValue(maxScroll);
                    Horizontal_scroll_value = maxScroll;
                }
            }

            updateSpinBoxMax();

            if (!updateTimer.isActive()) {
                ui->plot->replot();
            }
        }
    }
}

void MainWindow::updateXAxisRange() {
    if (dataPointNumber == 0 || startTime == 0) {
        ui->plot->xAxis->setRange(0, x_scale_value);
        return;
    }

    double startTimeSeconds = startTime / 1000.0;
    double currentRelativeTime = lastTime;

    int maxScroll = static_cast<int>(currentRelativeTime - x_scale_value);
    if (maxScroll < 0)
        maxScroll = 0;

    ui->horizontalScrollBar->setMaximum(maxScroll);
    ui->horizontalScrollBar->setPageStep(static_cast<int>(x_scale_value));
    ui->horizontalScrollBar->setSingleStep(10);

    if (Horizontal_scroll_value > (uint64_t)maxScroll) {
        Horizontal_scroll_value = maxScroll;
        ui->horizontalScrollBar->setValue(static_cast<int>(Horizontal_scroll_value));
    }

    if (flag_Graph_moove) {
        if (currentRelativeTime > x_scale_value) {
            ui->plot->xAxis->setRange(startTimeSeconds + currentRelativeTime - x_scale_value, startTimeSeconds + currentRelativeTime);
        } else {
            ui->plot->xAxis->setRange(startTimeSeconds, startTimeSeconds + currentRelativeTime + 1);
        }
    } else {
        double left = startTimeSeconds + Horizontal_scroll_value;
        double right = startTimeSeconds + x_scale_value + Horizontal_scroll_value;

        if (currentRelativeTime < x_scale_value) {
            right = startTimeSeconds + currentRelativeTime + 1;
            left = startTimeSeconds;
        }

        ui->plot->xAxis->setRange(left, right);
    }
}

void MainWindow::updateProcessState(bool state) {
    if (state) {
        qDebug() << "State = true";
        portOpenedSuccess();
    } else {
        qDebug() << "State = false";
        onPortClosed();
    }
}

void MainWindow::updateError(const QString &error) {
    QString timestamp = QDateTime::currentDateTime().toString("dd-MM-yyyy hh:mm:ss");
    QString formattedMessage = QString("[%1] Info: %2").arg(timestamp, error);

    qDebug() << formattedMessage;
    ui->textEdit_UartWindow->append(formattedMessage);
}

void MainWindow::write_settings() {
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
    settings->setValue("Count_channels", ui->channel_count->value());
    settings->setValue("Legend", ui->action_visible_legend->isChecked());
    settings->setValue("Dynamic_axis_X", ui->checkBox_3->isChecked());
    settings->setValue("Precision", ui->action_precision->isChecked());

    settings->setValue("Precision_ch1", ui->comboBox_precision_ch1->currentIndex());
    settings->setValue("Precision_ch2", ui->comboBox_precision_ch2->currentIndex());
    settings->setValue("Precision_ch3", ui->comboBox_precision_ch3->currentIndex());
    settings->setValue("Precision_ch4", ui->comboBox_precision_ch4->currentIndex());
    settings->setValue("Precision_ch5", ui->comboBox_precision_ch5->currentIndex());
    settings->setValue("Precision_ch6", ui->comboBox_precision_ch6->currentIndex());
    settings->setValue("Precision_ch7", ui->comboBox_precision_ch7->currentIndex());
    settings->setValue("Precision_ch8", ui->comboBox_precision_ch8->currentIndex());

    settings->setValue("Core_Protocol", ui->comboBox_Core_Protocol->currentIndex());
    settings->setValue("Scan_Rate", Scan_rate);
}

void MainWindow::read_settings() {
    Width_pen_graph = settings->value("WidthPen", "1").toUInt();
    apply_setttings_OpenGL(settings->value("OpenGL", "false").toBool());
    ui->comboBaud->setCurrentIndex(settings->value("Baudrate", "3").toInt());
    ui->comboData->setCurrentIndex(settings->value("Databits", "0").toInt());
    ui->comboParity->setCurrentIndex(settings->value("Partity", "0").toInt());
    ComPortName = settings->value("ComPort", "").toString();
    Window_init(settings->value("WinComPort", "true").toBool(), settings->value("WinInputData", "false").toBool(), settings->value("WinGraphSettings", "true").toBool(),
                settings->value("WinDebugInfo", "true").toBool(), settings->value("Precision", "true").toBool());
    ui->action_windows_stays_on_top->setChecked(settings->value("WinOnTop", "false").toBool());
    on_action_windows_stays_on_top_triggered(settings->value("WinOnTop", "false").toBool());
    setGeometry(settings->value("Geometry", QRect(200, 200, 300, 300)).toRect());
    ui->checkBox->setChecked(settings->value("Autoscale", "true").toBool());
    ui->Autoscale->setChecked(settings->value("Autoscale", "true").toBool());
    on_Autoscale_triggered();
    ui->action_run->setChecked(settings->value("RunGraph", "true").toBool());
    ui->checkBox_2->setChecked(settings->value("RunGraph", "true").toBool());
    on_checkBox_2_clicked(settings->value("RunGraph", "true").toBool());
    ui->spinYStep->setValue(settings->value("Y_set", "5").toUInt());
    on_spinYStep_valueChanged(settings->value("Y_set", "5").toUInt());
    ui->spinBox->setValue(settings->value("X_set", "500").toUInt());
    on_spinBox_valueChanged(settings->value("X_set", "500").toUInt());
    ui->checkBox_3->setChecked(settings->value("Dynamic_axis_X", "true").toBool());

    ui->comboBox_precision_ch1->setCurrentIndex(settings->value("Precision_ch1", "3").toInt());
    ui->comboBox_precision_ch2->setCurrentIndex(settings->value("Precision_ch2", "3").toInt());
    ui->comboBox_precision_ch3->setCurrentIndex(settings->value("Precision_ch3", "3").toInt());
    ui->comboBox_precision_ch4->setCurrentIndex(settings->value("Precision_ch4", "3").toInt());
    ui->comboBox_precision_ch5->setCurrentIndex(settings->value("Precision_ch5", "3").toInt());
    ui->comboBox_precision_ch6->setCurrentIndex(settings->value("Precision_ch6", "3").toInt());
    ui->comboBox_precision_ch7->setCurrentIndex(settings->value("Precision_ch7", "3").toInt());
    ui->comboBox_precision_ch8->setCurrentIndex(settings->value("Precision_ch8", "3").toInt());

    int count = settings->value("Count_channels", "1").toUInt();
    if (count < 1)
        count = 1;
    if (count > 8)
        count = 8;
    Data_count = count;
    ui->channel_count->setValue(Data_count);

    ui->actionDisconnect->setEnabled(false);
    ui->actionPause_Plot->setEnabled(false);

    ui->comboBox_Core_Protocol->setCurrentIndex(settings->value("Core_Protocol", "0").toInt());
    ui->spinBox_2->setValue(settings->value("Scan_rate", "1000").toInt());
}

void MainWindow::apply_setttings_OpenGL(bool arg1) {
    ui->action_use_OpenGL->setChecked(arg1);
    on_action_use_OpenGL_triggered(arg1);
}

void MainWindow::createUI() {
    if (QSerialPortInfo::availablePorts().size() == 0) {
        enable_com_controls(false);
        ui->statusBar->showMessage("COM портов не обнаружено");
        ui->savePNGButton->setEnabled(false);
    }

    Q_FOREACH(QSerialPortInfo port, QSerialPortInfo::availablePorts()) {
        ui->comboPort->addItem(port.portName());
    }

    ui->comboBaud->addItem("1200");
    ui->comboBaud->addItem("2400");
    ui->comboBaud->addItem("4800");
    ui->comboBaud->addItem("9600");
    ui->comboBaud->addItem("19200");
    ui->comboBaud->addItem("38400");
    ui->comboBaud->addItem("57600");
    ui->comboBaud->addItem("115200");
    ui->comboBaud->addItem("128000");
    ui->comboBaud->addItem("153600");
    ui->comboBaud->addItem("230400");
    ui->comboBaud->addItem("256000");
    ui->comboBaud->addItem("460800");
    ui->comboBaud->addItem("921600");
    ui->comboData->addItem("8 bits");
    ui->comboData->addItem("7 bits");
    ui->comboParity->addItem("none");
    ui->comboParity->addItem("odd");
    ui->comboParity->addItem("even");
    ui->comboStop->addItem("1 bit");
    ui->comboStop->addItem("2 bits");
    ui->listWidget_Channels->clear();

    read_settings();

    uint32_t Index = ui->comboPort->count();
    for (uint32_t i = 0; i < Index; i++) {
        if (ui->comboPort->itemText(i) == ComPortName) {
            ui->comboPort->setCurrentIndex(i);
        }
    }
}

void MainWindow::setupPlot() {
    ui->plot->clearItems();
    ui->plot->setBackground(gui_colors[0]);
    ui->plot->setNotAntialiasedElements(QCP::aeAll);
    QFont font;
    font.setStyleStrategy(QFont::NoAntialias);
    ui->plot->legend->setFont(font);

    ui->plot->xAxis->grid()->setZeroLinePen(QPen(gui_colors[1], 1, Qt::DotLine));
    ui->plot->xAxis->grid()->setPen(QPen(gui_colors[1], 1, Qt::DotLine));
    ui->plot->xAxis->grid()->setSubGridPen(QPen(gui_colors[1], 1, Qt::DotLine));
    ui->plot->xAxis->setBasePen(QPen(gui_colors[2]));
    ui->plot->xAxis->setTickPen(QPen(gui_colors[2]));
    ui->plot->xAxis->setSubTickPen(QPen(gui_colors[2]));
    ui->plot->xAxis->setUpperEnding(QCPLineEnding::esSpikeArrow);
    ui->plot->xAxis->setTickLabelColor(gui_colors[2]);
    ui->plot->xAxis->setTickLabelFont(font);

    ui->plot->xAxis->setLabel("Время");
    ui->plot->xAxis->setNumberFormat("f");
    ui->plot->xAxis->setNumberPrecision(0);

    QSharedPointer < QCPAxisTickerText > timeTicker(new QCPAxisTickerText);
    ui->plot->xAxis->setTicker(timeTicker);

    updateXAxisRange();

    ui->plot->yAxis->grid()->setZeroLinePen(QPen(gui_colors[1], 1, Qt::DotLine));
    ui->plot->yAxis->grid()->setPen(QPen(gui_colors[1], 1, Qt::DotLine));
    ui->plot->yAxis->grid()->setSubGridPen(QPen(gui_colors[1], 1, Qt::DotLine));
    ui->plot->yAxis->setBasePen(QPen(gui_colors[2]));
    ui->plot->yAxis->setTickPen(QPen(gui_colors[2]));
    ui->plot->yAxis->setSubTickPen(QPen(gui_colors[2]));
    ui->plot->yAxis->setUpperEnding(QCPLineEnding::esSpikeArrow);
    ui->plot->yAxis->setTickLabelColor(gui_colors[2]);
    ui->plot->yAxis->setTickLabelFont(font);

    QFont legendFont;
    legendFont.setPointSize(8);
    ui->plot->legend->setFont(legendFont);
    ui->plot->legend->setBrush(gui_colors[3]);
    ui->plot->legend->setBorderPen(gui_colors[1]);
    ui->plot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignRight);

    ui->action_visible_legend->setChecked(true);
    on_action_visible_legend_triggered(true);
    ui->action_visible_legend->setChecked(settings->value("Legend", "true").toBool());
    on_action_visible_legend_triggered(settings->value("Legend", "true").toBool());

    ui->horizontalScrollBar->setValue(0);
    ui->horizontalScrollBar->setMaximum(0);
    ui->horizontalScrollBar->setPageStep(1);
    ui->horizontalScrollBar->setSingleStep(10);
    Horizontal_scroll_value = 0;

    ui->spinBox->setMaximum(1);
}

void MainWindow::enable_com_controls(bool enable) {
    ui->comboBaud->setEnabled(enable);
    ui->comboData->setEnabled(enable);
    ui->comboParity->setEnabled(enable);
    ui->comboPort->setEnabled(enable);
    ui->comboStop->setEnabled(enable);
    ui->actionConnect->setEnabled(enable);
    ui->actionPause_Plot->setEnabled(!enable);
    ui->actionDisconnect->setEnabled(!enable);
    ui->channel_count->setEnabled(enable);
    ui->spinBox_2->setEnabled(enable);
}

void MainWindow::on_spinBox_2_valueChanged(int arg1) {
    Scan_rate = arg1;
}

void MainWindow::openPort(int baudRate, QSerialPort::DataBits dataBits, QSerialPort::Parity parity, QSerialPort::StopBits stopBits) {
    qDebug() << ui->comboPort->currentText();
    ui->textEdit_UartWindow->append("COM Port:" + ui->comboPort->currentText());
    QString Serialport = ui->comboPort->currentText();
    Serialport.remove("COM");

    qDebug() << "Запуск ядра опроса...";
    ui->textEdit_UartWindow->append("Запуск ядра опроса...");

    //Назначим накопительный буфер в зависимости от скорости опроса, чтоб сильно не грузить ПК
    if (ui->spinBox_2->text().toInt() < 10) {
        BUFFER_SIZE = 100;
    } else if (ui->spinBox_2->text().toInt() >= 10 && ui->spinBox_2->text().toInt() < 50) {
        BUFFER_SIZE = 25;
    } else if (ui->spinBox_2->text().toInt() >= 50 && ui->spinBox_2->text().toInt() < 100) {
        BUFFER_SIZE = 10;
    } else if (ui->spinBox_2->text().toInt() >= 100 && ui->spinBox_2->text().toInt() < 500) {
        BUFFER_SIZE = 5;
    } else {
        BUFFER_SIZE = 1;
    }

    qDebug() << dataBits;

    QString Data_bits = "";

    if (dataBits == QSerialPort::Data8){
        Data_bits = "8";
    }else if (dataBits == QSerialPort::Data7){
        Data_bits = "7";
    }

    QString String_Parity = "";

    if (parity == QSerialPort::NoParity){
        String_Parity = "NOPARITY";
    }else if (parity == QSerialPort::EvenParity){
        String_Parity = "EVENPARITY";
    }else if (parity == QSerialPort::OddParity){
        String_Parity = "ODDPARITY";
    }

    QString String_StopBits = "";

    if (stopBits == QSerialPort::OneStop){
        String_StopBits = "ONESTOPBIT";
    }else if (stopBits == QSerialPort::TwoStop){
        String_StopBits = "TWOSTOPBITS";
    }


    emit clearStats();

    QString program = "";

    if (ui->comboBox_Core_Protocol->currentText() == "Embedded") {
        program = "Cores/Embedded_Debug_ModbusRTU/Core_Embedded_ModbusRTU.exe";
    } else if (ui->comboBox_Core_Protocol->currentText() == "Овен TPM101") {
        program = "Cores/TPM101/Core_TPM101.exe";
    }

    // СИНХРОНИЗАЦИЯ ТОЧНОСТИ ПЕРЕД ПОДКЛЮЧЕНИЕМ
    QComboBox* precisionCombos[] = {
        ui->comboBox_precision_ch1,
        ui->comboBox_precision_ch2,
        ui->comboBox_precision_ch3,
        ui->comboBox_precision_ch4,
        ui->comboBox_precision_ch5,
        ui->comboBox_precision_ch6,
        ui->comboBox_precision_ch7,
        ui->comboBox_precision_ch8
    };

    for (int i = 0; i < 8; ++i) {
        if (precisionCombos[i]) {
            precisionValues[i] = precisionCombos[i]->currentIndex();
        }
    }



    QStringList arguments = { Serialport, QString::number(baudRate), Data_bits, String_Parity, String_StopBits, QString::number(Scan_rate), QString::number(Data_count) };
    emit startProcess(program, arguments);
}

void MainWindow::onPortClosed() {
    connected = false;
    plotting = false;
    hasNewData = false;
    needAxisUpdate = false;
    ui->pushButton->setEnabled(true);
}

void MainWindow::on_comboPort_currentIndexChanged(const QString &arg1) {
    QSerialPortInfo selectedPort(arg1);
    ui->statusBar->showMessage(selectedPort.description());
}

void MainWindow::portOpenedSuccess() {
    ui->action_COM_port->setChecked(false);
    on_action_COM_port_triggered(false);
    ui->statusBar->showMessage("Подключено!");
    enable_com_controls(false);

   /* if (ui->actionRecord_stream->isChecked()) {
        openCsvFile();
    }*/
   // ui->actionRecord_stream->setEnabled(false);
    updateTimer.start(20);
    connected = true;
    plotting = true;
    ui->pushButton->setEnabled(false);
}

void MainWindow::portOpenedFail() {
    onPortClosed();
    ui->PortControlsBox->setVisible(true);
    ui->statusBar->showMessage("Невозможно подключиться к COM порту!");
    ui->pushButton->setEnabled(true);
    on_pushButton_clicked();
}

void MainWindow::replot() {
    if (flag_Autoscale) {
        ui->plot->yAxis->rescale(true);
        double lower = ui->plot->yAxis->range().lower;
        double upper = ui->plot->yAxis->range().upper;
        double margin = (upper - lower) * 0.1;
        ui->plot->yAxis->setRange(lower - margin, upper + margin);
    }

    if (needAxisUpdate || !flag_Graph_moove) {
        updateXAxisRange();
        updateTimeTicks();
        if (needAxisUpdate) {
            needAxisUpdate = false;
        }
    }

    if (dataPointNumber > 0 && startTime > 0) {
        updateSpinBoxMax();
    }

    ui->plot->replot();
}

void MainWindow::updateTimeTicks() {
    if (ui->plot->graphCount() > 0 && ui->plot->graph(0)->data()->size() > 0) {
        QSharedPointer < QCPAxisTickerText > timeTicker = ui->plot->xAxis->ticker().staticCast<QCPAxisTickerText>();
        timeTicker->clear();

        double rangeMin = ui->plot->xAxis->range().lower;
        double rangeMax = ui->plot->xAxis->range().upper;
        double diff = rangeMax - rangeMin;

        int plotWidth = ui->plot->axisRect()->width();
        if (plotWidth <= 0) {
            plotWidth = 800;
        }

        int maxLabels = plotWidth / 80;
        if (maxLabels < 2)
            maxLabels = 2;
        if (maxLabels > 15)
            maxLabels = 15;

        double step = 1;
        if (diff > 0) {
            QVector<double> possibleSteps = { 1, 2, 5, 10, 15, 20, 30, 60, 120, 300, 600, 900, 1800, 3600, 7200, 14400, 28800, 86400 };
            for (double s : possibleSteps) {
                if (diff / s <= maxLabels) {
                    step = s;
                    break;
                }
            }
            if (step == 1 && diff / 1 > maxLabels) {
                step = possibleSteps.last();
            }
        }

        double start = ceil(rangeMin / step) * step;

        QDate lastDate;
        bool firstTick = true;

        for (double t = start; t <= rangeMax; t += step) {
            QDateTime dateTime = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(t));
            QDate currentDate = dateTime.date();

            QString label;
            QString dayInfo;

            // Определяем основной формат
            if (step >= 86400) {
                label = dateTime.toString("dd.MM.yyyy");
            } else if (step >= 3600) {
                label = dateTime.toString("HH:mm");
            } else if (step >= 60) {
                label = dateTime.toString("HH:mm");
            } else {
                label = dateTime.toString("HH:mm:ss");
            }

            // Добавляем информацию о дне, если он изменился или если диапазон > 12 часов
            if (!firstTick && currentDate != lastDate) {
                // Смена дня - показываем дату в скобках под меткой или рядом
                dayInfo = QString("(%1)").arg(currentDate.toString("dd.MM.yyyy"));
                label = label + "\n" + dayInfo;  // Перенос строки
            } else if (diff >= 43200 && (t == start || t == start + step)) {
                // Для длительных записей показываем дату на первой метке
                dayInfo = QString("(%1)").arg(currentDate.toString("dd.MM.yyyy"));
                label = label + "\n" + dayInfo;
            }

            timeTicker->addTick(t, label);
            lastDate = currentDate;
            firstTick = false;
        }
    }
}

void MainWindow::on_spinYStep_valueChanged(int arg1) {
    ui->plot->yAxis->ticker()->setTickCount(arg1);
    ui->spinYStep->setValue(ui->plot->yAxis->ticker()->tickCount());
}

void MainWindow::on_savePNGButton_clicked() {
    // Сохраняем текущее состояние видимости трекеров
    QVector<bool> tracerStatesX, tracerStatesY, tracerStatesLabels;
    for (int i = 0; i < tracerLinesX.size(); ++i) {
        tracerStatesX.append(tracerLinesX[i]->visible());
        tracerStatesY.append(tracerLinesY[i]->visible());
        tracerStatesLabels.append(tracerLabels[i]->visible());
    }

    // Скрываем все трекеры (маркеры)
    for (int i = 0; i < tracerLinesX.size(); ++i) {
        tracerLinesX[i]->setVisible(false);
        tracerLinesY[i]->setVisible(false);
        tracerLabels[i]->setVisible(false);
    }

    // Перерисовываем график без маркеров
    ui->plot->replot();

    // Сохраняем PNG
    QString Text = "Graph_snapshot_" + QDateTime::currentDateTime().toString("dd.MM.yyyy-HH.mm.ss") + ".png";
    ui->plot->savePng(Text, 1920, 1080, 2, 50);
    ui->statusBar->showMessage("Запись в файл: " + Text);

    // Восстанавливаем состояние трекеров
    for (int i = 0; i < tracerLinesX.size(); ++i) {
        tracerLinesX[i]->setVisible(tracerStatesX[i]);
        tracerLinesY[i]->setVisible(tracerStatesY[i]);
        tracerLabels[i]->setVisible(tracerStatesLabels[i]);
    }

    // Перерисовываем график с восстановленными маркерами
    ui->plot->replot();
}

void MainWindow::onMouseMoveInPlot(QMouseEvent *event) {
    if (tracerLinesX.isEmpty() || tracerLinesY.isEmpty() || tracerLabels.isEmpty() || tracerLinesX.size() != ui->plot->graphCount()) {
        return;
    }

    double xCoord = ui->plot->xAxis->pixelToCoord(event->pos().x());

    if (ui->plot->graphCount() == 0 || ui->plot->graph(0)->data()->size() == 0) {
        hideAllTracers();
        ui->statusBar->showMessage("");
        return;
    }

    double targetKey = findNearestKey(xCoord);
    if (targetKey < 0) {
        hideAllTracers();
        ui->statusBar->showMessage("");
        return;
    }

    updateStatusBar(targetKey, xCoord);
    updateTracers(targetKey);
    ui->plot->replot(QCustomPlot::rpQueuedReplot);
}

double MainWindow::findNearestKey(double xCoord) {
    double targetKey = -1;
    double minDist = std::numeric_limits<double>::max();

    for (int g = 0; g < ui->plot->graphCount(); ++g) {
        QCPGraph *graph = ui->plot->graph(g);
        if (!graph->visible() || graph->data()->size() == 0)
            continue;

        QCPGraphDataContainer::const_iterator it = graph->data()->findBegin(xCoord);
        if (it == graph->data()->constEnd())
            continue;

        checkPoint(it, xCoord, targetKey, minDist);

        auto itNext = it;
        ++itNext;
        if (itNext != graph->data()->constEnd())
            checkPoint(itNext, xCoord, targetKey, minDist);

        if (it != graph->data()->constBegin()) {
            auto itPrev = it;
            --itPrev;
            checkPoint(itPrev, xCoord, targetKey, minDist);
        }
    }

    return targetKey;
}

void MainWindow::checkPoint(QCPGraphDataContainer::const_iterator it, double xCoord, double &targetKey, double &minDist) {
    double dist = qAbs(xCoord - it->key);
    if (dist < minDist) {
        minDist = dist;
        targetKey = it->key;
    }
}

void MainWindow::hideAllTracers() {
    for (int g = 0; g < tracerLinesX.size(); ++g) {
        tracerLinesX[g]->setVisible(false);
        tracerLinesY[g]->setVisible(false);
        tracerLabels[g]->setVisible(false);
    }
}

void MainWindow::updateTracers(double targetKey) {
    hideAllTracers();

    for (int g = 0; g < ui->plot->graphCount(); ++g) {
        QCPGraph *graph = ui->plot->graph(g);
        if (!graph->visible() || graph->data()->size() == 0)
            continue;

        double value = getValueAtKey(graph, targetKey);
        if (std::isnan(value))
            continue;

        tracerLinesX[g]->start->setCoords(targetKey, ui->plot->yAxis->range().lower);
        tracerLinesX[g]->end->setCoords(targetKey, ui->plot->yAxis->range().upper);
        tracerLinesX[g]->setPen(QPen(line_colors[g % CUSTOM_LINE_COLORS], 1, Qt::DashLine));
        tracerLinesX[g]->setVisible(true);

        tracerLinesY[g]->start->setCoords(ui->plot->xAxis->range().lower, value);
        tracerLinesY[g]->end->setCoords(ui->plot->xAxis->range().upper, value);
        tracerLinesY[g]->setPen(QPen(line_colors[g % CUSTOM_LINE_COLORS], 1, Qt::DashLine));
        tracerLinesY[g]->setVisible(true);

        // ===== ИСПОЛЬЗУЕМ ТОЧНОСТЬ ДЛЯ МАРКЕРА =====
        int precision = getPrecisionForChannel(g);
        tracerLabels[g]->setText(QString("%1: %2").arg(graph->name()).arg(value, 0, 'f', precision));
        // ============================================

        tracerLabels[g]->setColor(line_colors[g % CUSTOM_LINE_COLORS]);
        tracerLabels[g]->setPen(QPen(line_colors[g % CUSTOM_LINE_COLORS], 1));
        tracerLabels[g]->setBrush(QBrush(QColor(255, 255, 255, 200)));

        positionLabel(g, targetKey, value);
        tracerLabels[g]->setVisible(true);
    }
}

double MainWindow::getValueAtKey(QCPGraph *graph, double key) {
    QCPGraphDataContainer::const_iterator it = graph->data()->findBegin(key);
    if (it != graph->data()->constEnd()) {
        if (qAbs(it->key - key) < 0.001)
            return it->value;

        auto itNext = it;
        ++itNext;
        if (itNext != graph->data()->constEnd() && qAbs(itNext->key - key) < 0.001)
            return itNext->value;

        if (it != graph->data()->constBegin()) {
            auto itPrev = it;
            --itPrev;
            if (qAbs(itPrev->key - key) < 0.001)
                return itPrev->value;
        }
        return it->value;
    }
    return std::numeric_limits<double>::quiet_NaN();
}

void MainWindow::positionLabel(int index, double key, double value) {
    double offsetX = (ui->plot->xAxis->range().upper - ui->plot->xAxis->range().lower) * 0.01;
    double offsetY = (ui->plot->yAxis->range().upper - ui->plot->yAxis->range().lower) * 0.02 * (index + 1);

    if (key + offsetX > ui->plot->xAxis->range().upper) {
        tracerLabels[index]->setPositionAlignment(Qt::AlignRight | Qt::AlignVCenter);
        tracerLabels[index]->position->setCoords(key - offsetX, value + offsetY);
    } else {
        tracerLabels[index]->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        tracerLabels[index]->position->setCoords(key + offsetX, value + offsetY);
    }
}

void MainWindow::updateStatusBar(double targetKey, double xCoord) {
    // Преобразуем секунды в миллисекунды
    qint64 msecs = static_cast<qint64>(targetKey * 1000);

    QString statusText = QString("Время: %1").arg(QDateTime::fromMSecsSinceEpoch(msecs).toString("dd.MM.yyyy hh:mm:ss.zzz"));

    for (int g = 0; g < ui->plot->graphCount(); ++g) {
        QCPGraph *graph = ui->plot->graph(g);
        if (!graph->visible() || graph->data()->size() == 0)
            continue;

        double value = getValueAtKey(graph, xCoord);
        if (!std::isnan(value)) {
            // ===== ИСПОЛЬЗУЕМ ТОЧНОСТЬ ДЛЯ СТАТУС-БАРА =====
            int precision = getPrecisionForChannel(g);
            statusText += QString("  %1: %2;").arg(graph->name()).arg(value, 0, 'f', precision);
            // ================================================
        }
    }

    ui->statusBar->showMessage(statusText);
}

void MainWindow::channel_selection(void) {
    for (int i = 0; i < ui->plot->graphCount(); i++) {
        QCPGraph *graph = ui->plot->graph(i);
        QCPPlottableLegendItem *item = ui->plot->legend->itemWithPlottable(graph);
        if (item->selected()) {
            item->setSelected(true);
        } else {
            item->setSelected(false);
        }
    }
}


void MainWindow::scale_setting() {
    updateXAxisRange();
}

void MainWindow::on_actionHow_to_use_triggered() {
    about = new About(this);
    about->setWindowTitle("О программе");
    about->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowSystemMenuHint);
    about->show();
}

/*============================Кнопка подключения=============================*/
void MainWindow::on_actionConnect_triggered() {
    if (RTU_WINAPI_STATE_PROCESS == RTU_WINAPI_STOPPED) {
        on_actionClear_triggered();

        // Если пользователь отменил выбор файла - выходим
        if (!on_saveSnapshotAction_triggered()) {
            return;
        }
    }

    if (connected) {
        if (!plotting) {
            updateTimer.start();
            plotting = true;
            ui->actionConnect->setEnabled(false);
            ui->actionPause_Plot->setEnabled(true);
            ui->statusBar->showMessage("Продолжим отображение графика!");
        }
    } else {
        QSerialPortInfo portInfo(ui->comboPort->currentText());
        int baudRate = ui->comboBaud->currentText().toInt();
        int dataBitsIndex = ui->comboData->currentIndex();
        int parityIndex = ui->comboParity->currentIndex();
        int stopBitsIndex = ui->comboStop->currentIndex();
        QSerialPort::DataBits dataBits;
        QSerialPort::Parity parity;
        QSerialPort::StopBits stopBits;

        switch (dataBitsIndex) {
        case 0:
            dataBits = QSerialPort::Data8;
            break;
        case 1:
            dataBits = QSerialPort::Data7;
            break;
        default:
            dataBits = QSerialPort::Data8;
        }

        switch (parityIndex) {
        case 0:
            parity = QSerialPort::NoParity;
            break;
        case 1:
            parity = QSerialPort::OddParity;
            break;
        case 2:
            parity = QSerialPort::EvenParity;
            break;
        default:
            parity = QSerialPort::NoParity;
        }

        switch (stopBitsIndex) {
        case 0:
            stopBits = QSerialPort::OneStop;
            break;
        case 1:
            stopBits = QSerialPort::TwoStop;
            break;
        default:
            stopBits = QSerialPort::OneStop;
        }

        openPort(baudRate, dataBits, parity, stopBits);
       ui->openSnapshotAction->setEnabled(false);
       ui->actionRecord_stream->setEnabled(false);
    }
}

void MainWindow::on_actionPause_Plot_triggered() {
    if (connected) {
        emit stopProcess();
        updateTimer.stop();
        enable_com_controls(true);
        ui->action_COM_port->setChecked(true);
        ui->actionConnect->setText("Продолжить запись");
        on_action_COM_port_triggered(true);
    }
}

void MainWindow::on_actionRecord_stream_triggered() {
    // Открываем диалог выбора .dat файла
    QString datFilePath = QFileDialog::getOpenFileName(this,
        "Выберите .dat файл для конвертации в CSV",
        QDir::homePath(),
        "Snapshot Files (*.dat)");

    // Если пользователь нажал "Отмена" - выходим
    if (datFilePath.isEmpty()) {
        return;
    }

    // Открываем .dat файл для чтения
    QFile datFile(datFilePath);
    if (!datFile.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось открыть .dat файл!");
        return;
    }

    // Читаем заголовок
    FileHeader header;
    if (datFile.read(reinterpret_cast<char*>(&header), sizeof(header)) != sizeof(header)) {
        QMessageBox::warning(this, "Ошибка", "Неверный формат .dat файла!");
        datFile.close();
        return;
    }

    if (header.channelCount < 1 || header.channelCount > 8) {
        QMessageBox::warning(this, "Ошибка", QString("Некорректное количество каналов: %1").arg(header.channelCount));
        datFile.close();
        return;
    }

    uint8_t channelCount = header.channelCount;

    // Создаем имя для .csv файла
    QString csvFilePath = datFilePath;
    csvFilePath.replace(".dat", ".csv");

    // Если файл существует, предлагаем перезаписать
    if (QFile::exists(csvFilePath)) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Файл существует",
            QString("Файл %1 уже существует. Перезаписать?").arg(QFileInfo(csvFilePath).fileName()),
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) {
            datFile.close();
            return;
        }
    }

    // Открываем .csv файл для записи
    QFile csvFile(csvFilePath);
    if (!csvFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось создать .csv файл!");
        datFile.close();
        return;
    }

    // Настраиваем поток для чтения данных
    QDataStream in(&datFile);
    in.setVersion(QDataStream::Qt_5_15);
    in.setFloatingPointPrecision(QDataStream::DoublePrecision);

    // Позиционируемся после заголовка
    datFile.seek(sizeof(header));

    // Собираем имена каналов для заголовка CSV
    QStringList channelNames;
    for (int i = 0; i < channelCount; ++i) {
        QString name = QString::fromUtf8(header.channels[i].name);
        if (name.trimmed().isEmpty()) {
            name = QString("Канал %1").arg(i + 1);
        }
        // Добавляем единицу измерения, если она есть
        QString unit = QString::fromUtf8(header.channels[i].unit);
        if (!unit.trimmed().isEmpty() && unit != "---") {
            name += QString(" (%1)").arg(unit);
        }
        channelNames.append(name);
    }

    // Пишем заголовок CSV
    QTextStream out(&csvFile);
    out << "Time";
    for (const QString &name : channelNames) {
        out << "," << name;
    }
    out << "\n";

    // Читаем данные и пишем в CSV
    QVector<double> values(channelCount);
    double time;
    qint64 pointCount = 0;
    qint64 errorCount = 0;

    // Показываем прогресс (опционально)
    ui->statusBar->showMessage("Конвертация в CSV...");
    QApplication::processEvents();

    while (!in.atEnd()) {
        // Читаем значения всех каналов
        bool readOk = true;
        for (int ch = 0; ch < channelCount; ++ch) {
            float value;
            in >> value;
            if (in.status() != QDataStream::Ok) {
                readOk = false;
                errorCount++;
                break;
            }
            values[ch] = static_cast<double>(value);
        }

        if (!readOk) {
            // Пропускаем оставшиеся данные до конца файла
            break;
        }

        // Читаем время
        in >> time;
        if (in.status() != QDataStream::Ok) {
            errorCount++;
            break;
        }

        // Преобразуем время из секунд в читаемый формат
        qint64 msecs = static_cast<qint64>(time * 1000);
        QString timeStr = QDateTime::fromMSecsSinceEpoch(msecs).toString("dd.MM.yyyy hh:mm:ss.zzz");

        // Пишем строку в CSV
        out << timeStr;
        for (int ch = 0; ch < channelCount; ++ch) {
            out << "," << QString::number(values[ch], 'f', 6);
        }
        out << "\n";

        pointCount++;

        // Обновляем статус каждые 1000 точек
        if (pointCount % 1000 == 0) {
            ui->statusBar->showMessage(QString("Конвертация... %1 точек").arg(pointCount));
            QApplication::processEvents();
        }
    }

    datFile.close();
    csvFile.close();

    // Проверяем результат
    if (errorCount > 0) {
        QMessageBox::warning(this, "Предупреждение",
            QString("Конвертация завершена с ошибками!\n"
                   "Обработано точек: %1\n"
                   "Ошибок чтения: %2\n"
                   "Файл сохранен как: %3")
                   .arg(pointCount).arg(errorCount).arg(QFileInfo(csvFilePath).fileName()));
    } else {
        QMessageBox::information(this, "Успешно",
            QString("Конвертация завершена успешно!\n"
                   "Обработано точек: %1\n"
                   "Каналов: %2\n"
                   "Файл сохранен как: %3")
                   .arg(pointCount).arg(channelCount).arg(QFileInfo(csvFilePath).fileName()));
    }

    ui->statusBar->showMessage(QString("CSV файл сохранен: %1").arg(csvFilePath));
}

void MainWindow::on_actionDisconnect_triggered() {
    if (connected) {
        emit stopProcess();
        updateTimer.stop();
        enable_com_controls(true);
        ui->action_COM_port->setChecked(true);
        ui->actionConnect->setText("Начать запись");
        on_action_COM_port_triggered(true);

        ui->openSnapshotAction->setEnabled(true);
        ui->actionRecord_stream->setEnabled(true);

        RTU_WINAPI_STATE_PROCESS = RTU_WINAPI_STOPPED;
        qDebug() << "Disconnect" + QString::number(RTU_WINAPI_STATE_PROCESS);
        closeDataFile(); //Закрыть бинарный файл
        setWindowTitle (WindowTitle);
    }
}

void MainWindow::on_actionClear_triggered() {
    clearPlot();
}

void MainWindow::on_pushButton_AutoScale_clicked() {
    ui->plot->yAxis->rescale(true);
    double lower = ui->plot->yAxis->range().lower;
    double upper = ui->plot->yAxis->range().upper;
    double range = upper - lower;

    if (range < 0.1) {
        ui->plot->yAxis->setRange(lower - 0.5, upper + 0.5);
    } else {
        double margin = range * 0.08;
        ui->plot->yAxis->setRange(lower - margin, upper + margin);
    }

    if (!updateTimer.isActive()) {
        replot();
    }
}

void MainWindow::on_pushButton_ResetVisible_clicked() {
    for (int i = 0; i < ui->plot->graphCount(); i++) {
        ui->plot->graph(i)->setVisible(true);
        ui->listWidget_Channels->item(i)->setBackground(Qt::NoBrush);
    }
}

void MainWindow::on_listWidget_Channels_itemDoubleClicked(QListWidgetItem *item) {
    int graphIdx = ui->listWidget_Channels->currentRow();
    if (ui->plot->graph(graphIdx)->visible()) {
        ui->plot->graph(graphIdx)->setVisible(false);
        item->setBackground(QColor(205, 34, 46, 100));
    } else {
        ui->plot->graph(graphIdx)->setVisible(true);
        item->setBackground(Qt::NoBrush);
    }
    ui->plot->replot();
}

void MainWindow::on_pushButton_clicked() {
    ui->comboPort->clear();
    if (QSerialPortInfo::availablePorts().size() == 0) {
        enable_com_controls(false);
        ui->statusBar->showMessage("COM портов не обнаружено");
        ui->savePNGButton->setEnabled(false);
    } else {
        enable_com_controls(true);
        ui->savePNGButton->setEnabled(true);
    }
    Q_FOREACH(QSerialPortInfo port, QSerialPortInfo::availablePorts()) {
        ui->comboPort->addItem(port.portName());
    }
}

void MainWindow::on_Autoscale_triggered() {
    if (ui->Autoscale->isChecked()) {
        flag_Autoscale = true;
        ui->checkBox->setChecked(1);
        ui->plot->setInteraction(QCP::iRangeZoom, false);
    } else {
        flag_Autoscale = false;
        ui->checkBox->setChecked(0);
        ui->plot->setInteraction(QCP::iRangeZoom, true);
    }

    if (!updateTimer.isActive()) {
        replot();
    }
}

void MainWindow::on_horizontalScrollBar_valueChanged(int value) {
    if (flag_Graph_moove) {
        return;
    }

    Horizontal_scroll_value = value;

    if (dataPointNumber > 0 && startTime > 0) {
        double startTimeSeconds = startTime / 1000.0;

        ui->plot->xAxis->setRange(startTimeSeconds + value, startTimeSeconds + x_scale_value + value);

        if (!updateTimer.isActive()) {
            replot();
        }
    }
}

void MainWindow::on_checkBox_clicked(bool checked) {
    if (checked) {
        ui->Autoscale->setChecked(1);
        ui->Autoscale->setChecked(1);
        on_Autoscale_triggered();
    } else {
        ui->Autoscale->setChecked(0);
        ui->Autoscale->setChecked(0);
        on_Autoscale_triggered();
    }
}

void MainWindow::on_checkBox_2_clicked(bool checked) {
    ui->action_run->setChecked(checked);
    flag_Graph_moove = checked;

    if (flag_Graph_moove) {
        ui->plot->setInteraction(QCP::iRangeDrag, false);
        if (dataPointNumber > 0 && startTime > 0) {
            int maxScroll = static_cast<int>(lastTime - x_scale_value);
            if (maxScroll < 0)
                maxScroll = 0;

            ui->horizontalScrollBar->setMaximum(maxScroll);
            ui->horizontalScrollBar->setPageStep(static_cast<int>(x_scale_value));
            ui->horizontalScrollBar->setValue(maxScroll);
            Horizontal_scroll_value = maxScroll;
        } else {
            ui->horizontalScrollBar->setMaximum(0);
            ui->horizontalScrollBar->setPageStep(static_cast<int>(x_scale_value));
            ui->horizontalScrollBar->setValue(0);
            Horizontal_scroll_value = 0;
        }
    } else {
        ui->plot->setInteraction(QCP::iRangeDrag, false);
        if (dataPointNumber > 0 && startTime > 0) {
            int maxScroll = static_cast<int>(lastTime - x_scale_value);
            if (maxScroll < 0)
                maxScroll = 0;

            ui->horizontalScrollBar->setMaximum(maxScroll);
            ui->horizontalScrollBar->setPageStep(static_cast<int>(x_scale_value));
            ui->horizontalScrollBar->setSingleStep(10);

            if (lastTime > x_scale_value) {
                ui->horizontalScrollBar->setValue(maxScroll);
                Horizontal_scroll_value = maxScroll;
            } else {
                ui->horizontalScrollBar->setValue(0);
                Horizontal_scroll_value = 0;
            }
        } else {
            ui->horizontalScrollBar->setMaximum(0);
            ui->horizontalScrollBar->setPageStep(static_cast<int>(x_scale_value));
            ui->horizontalScrollBar->setValue(0);
            Horizontal_scroll_value = 0;
        }
    }

    if (!updateTimer.isActive()) {
        replot();
    }
}

void MainWindow::on_action_run_triggered(bool checked) {
    ui->checkBox_2->setChecked(checked);
    on_checkBox_2_clicked(checked);
}

void MainWindow::on_Reset_data_clicked() {
    on_actionClear_triggered();
}

void MainWindow::on_spinBox_valueChanged(int arg1) {
    if (dataPointNumber > 0 && startTime > 0) {
        if (arg1 > lastTime) {
            ui->spinBox->setValue(static_cast<int>(lastTime));
            return;
        }
    }

    x_scale_value = arg1;
    ui->plot->xAxis->setLabel(QString("Время (показано %1)").arg(formatTime(arg1)));

    ui->horizontalScrollBar->setPageStep(static_cast<int>(x_scale_value));

    if (dataPointNumber > 0 && startTime > 0) {
        int maxScroll = static_cast<int>(lastTime - x_scale_value);
        if (maxScroll < 0)
            maxScroll = 0;
        ui->horizontalScrollBar->setMaximum(maxScroll);

        if (Horizontal_scroll_value > (uint64_t)maxScroll) {
            Horizontal_scroll_value = maxScroll;
            ui->horizontalScrollBar->setValue(static_cast<int>(Horizontal_scroll_value));
        }
    }

    needAxisUpdate = true;
    if (!updateTimer.isActive()) {
        replot();
    }
}

void MainWindow::on_automatic_cnt_channel_clicked(bool checked) {
    flag_automatic_cnt_channels = checked;
}

void MainWindow::on_action_png_triggered() {
    on_savePNGButton_clicked();
}

void MainWindow::on_action_input_data_triggered(bool checked) {
    ui->groupBox_2->setVisible(checked);
}

void MainWindow::on_action_graph_settings_triggered(bool checked) {
    ui->PlotControlsBox->setVisible(checked);
}

void MainWindow::on_action_debug_info_triggered(bool checked) {
    if (!checked) {
        ui->textEdit_UartWindow->setVisible(false);
        ui->label_system_message->setVisible(false);
        Debug_visible = false;
    } else {
        ui->textEdit_UartWindow->setVisible(true);
        ui->label_system_message->setVisible(true);
        Debug_visible = true;
    }

}

void MainWindow::on_action_COM_port_triggered(bool checked) {
    ui->PortControlsBox->setVisible(checked);
}

void MainWindow::Window_init(bool COM, bool input_data, bool graph_settings, bool debug_info, bool precision) {
    ui->action_COM_port->setChecked(COM);
    on_action_COM_port_triggered(COM);

    ui->action_input_data->setChecked(input_data);
    on_action_input_data_triggered(input_data);

    ui->action_graph_settings->setChecked(graph_settings);
    on_action_graph_settings_triggered(graph_settings);

    ui->action_debug_info->setChecked(debug_info);
    on_action_debug_info_triggered(debug_info);

    ui->action_precision->setChecked(precision);
    on_action_precision_triggered(precision);
}

void MainWindow::on_action_precision_triggered(bool checked) {
    ui->groupBox_3->setVisible(checked);
}

void MainWindow::on_action_triggered() {
    flag_only_graph = !flag_only_graph;
    if (flag_only_graph) {
        Window_init(0, 0, 0, 0, 0);
    } else {
        Window_init(1, 1, 1, 1, 1);
    }
}

void MainWindow::on_action_visible_legend_triggered(bool checked) {
    ui->plot->legend->setVisible(checked);
    if (!updateTimer.isActive()) {
        replot();
    }
}

void MainWindow::on_action_windows_stays_on_top_triggered(bool checked) {
    if (checked) {
        this->setWindowFlag(Qt::WindowStaysOnTopHint, true);
        this->show();
    } else {
        this->setWindowFlag(Qt::WindowStaysOnTopHint, false);
        this->show();
    }
}

void MainWindow::on_action_Frameless_window_hint_triggered(bool checked) {
    if (checked) {
        this->setWindowFlag(Qt::FramelessWindowHint, true);
        this->show();
    } else {
        this->setWindowFlag(Qt::FramelessWindowHint, false);
        this->show();
    }
}

void MainWindow::on_action_use_OpenGL_triggered(bool checked) {
    ui->plot->setOpenGl(checked, 3);
    if (ui->plot->openGl()) {
        ui->statusBar->showMessage("OpenGL включен");
    } else {
        ui->statusBar->showMessage("OpenGL выключен");
    }
}

bool MainWindow::on_saveSnapshotAction_triggered() {
    filePath = QFileDialog::getSaveFileName(this, "Сохранить слепок данных",
        QDir::homePath() + "/snapshot_" + QDateTime::currentDateTime().toString("HH.mm.ss_d.MM.yyyy") + ".dat",
        "Snapshot Files (*.dat)");

    // Если пользователь нажал "Отмена" - возвращаем false
    if (filePath.isEmpty()) {
        return false;
    }

    if (QFile::exists(filePath)) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Файл существует",
            "Файл уже существует. Перезаписать?",
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            saveSnapshot(filePath);
            setWindowTitle("Запись файла: " + filePath + " - " + WindowTitle);
            return true;
        }
        // Если выбрано "Нет" - возвращаем false (пользователь отказался)
        return false;
    } else {
        saveSnapshot(filePath);
        setWindowTitle("Запись файла: " + filePath + " - " + WindowTitle);
        return true;
    }
}

void MainWindow::on_openSnapshotAction_triggered() {
    QString filePath = QFileDialog::getOpenFileName(this, "Загрузить файл", QDir::homePath(), "Snapshot Files (*.dat)");

    // Если пользователь нажал "Отмена" - выходим, ничего не делаем
    if (filePath.isEmpty()) {
        return;
    }

    loadSnapshot(filePath);
}

void MainWindow::saveSnapshot(const QString &filePath) {
    // Открываем файл для записи
    m_dataFile.setFileName(filePath);
    if (!m_dataFile.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось создать файл!");
        return;
    }

    isReadMode = false; // Устанавливаем режим записи
    updateTableHeaders(isReadMode);

    // Работа со структурой. Заполняем шапку
    FileHeader header;
    memset(&header, 0, sizeof(header));

    // Количество каналов
    header.channelCount = static_cast<uint8_t>(ui->channel_count->text().toInt());

    // Заполняем информацию о каналах
    for (int i = 0; i < header.channelCount && i < 8; ++i) {
        // Название канала из таблицы
        QTableWidgetItem *nameItem = ui->tableWidget->item(i, 0);
        if (nameItem) {
            strncpy(header.channels[i].name,
                    nameItem->text().toUtf8().constData(),
                    sizeof(header.channels[i].name) - 1);
            header.channels[i].name[sizeof(header.channels[i].name) - 1] = '\0';
        } else {
            strcpy(header.channels[i].name, QString("Канал %1").arg(i+1).toUtf8().constData());
        }

        // Единица измерения из таблицы
        QTableWidgetItem *unitItem = ui->tableWidget->item(i, 2);
        if (unitItem) {
            strncpy(header.channels[i].unit,
                    unitItem->text().toUtf8().constData(),
                    sizeof(header.channels[i].unit) - 1);
            header.channels[i].unit[sizeof(header.channels[i].unit) - 1] = '\0';
        } else {
            strcpy(header.channels[i].unit, "---");
        }
    }

    // Записываем заголовок в файл
    m_dataFile.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Настраиваем поток для записи данных
    m_dataStream.setDevice(&m_dataFile);
    m_dataStream.setVersion(QDataStream::Qt_5_15);
    m_dataStream.setFloatingPointPrecision(QDataStream::DoublePrecision);

    m_isDataFileOpen = true;

    // Обновляем заголовок окна
    setWindowTitle("Запись файла: " + filePath + " - " + WindowTitle);
}

void MainWindow::loadSnapshot(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось открыть файл!");
        return;
    }

    // Читаем заголовок
    FileHeader header;
    if (file.read(reinterpret_cast<char*>(&header), sizeof(header)) != sizeof(header)) {
        QMessageBox::warning(this, "Ошибка", "Неверный формат файла!");
        file.close();
        return;
    }

    if (header.channelCount < 1 || header.channelCount > 8) {
        QMessageBox::warning(this, "Ошибка", QString("Некорректное количество каналов: %1").arg(header.channelCount));
        file.close();
        return;
    }

    // Сохраняем количество каналов из заголовка
    uint8_t channelCount = header.channelCount;

    // Обновляем таблицу с именами и единицами измерения
    ui->tableWidget->blockSignals(true);

    // Обновляем количество каналов
    Data_count = channelCount;
    ui->channel_count->setValue(Data_count);
    isReadMode = true; // Устанавливаем режим чтения
    updateTableHeaders(isReadMode);
    setupTable(); // Пересоздаем таблицу с новым количеством каналов

    for (int i = 0; i < channelCount && i < 8; ++i) {
        // Восстанавливаем имя канала
        if (ui->tableWidget->item(i, 0)) {
            QString name = QString::fromUtf8(header.channels[i].name);
            if (!name.trimmed().isEmpty()) {
                ui->tableWidget->item(i, 0)->setText(name);
            }
        }

        // Восстанавливаем единицу измерения
        if (ui->tableWidget->item(i, 2)) {
            QString unit = QString::fromUtf8(header.channels[i].unit);
            if (!unit.trimmed().isEmpty()) {
                ui->tableWidget->item(i, 2)->setText(unit);
            }
        }
    }
    ui->tableWidget->blockSignals(false);

    // Очищаем текущие данные
    clearPlot();

    // Читаем данные
    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_5_15);
    in.setFloatingPointPrecision(QDataStream::DoublePrecision);

    // Позиционируемся после заголовка
    file.seek(sizeof(header));

    // Векторы для данных
    QVector<QVector<double>> channelData(channelCount);
    QVector<double> timeData;

    // Читаем данные
    while (!in.atEnd()) {
        // Читаем значения всех каналов
        for (int ch = 0; ch < channelCount; ++ch) {
            float value;
            in >> value;
            if (in.status() != QDataStream::Ok) {
                QMessageBox::warning(this, "Ошибка", "Ошибка чтения данных!");
                file.close();
                return;
            }
            channelData[ch].append(static_cast<double>(value));
        }

        // Читаем время
        double time;
        in >> time;
        if (in.status() != QDataStream::Ok) {
            QMessageBox::warning(this, "Ошибка", "Ошибка чтения времени!");
            file.close();
            return;
        }
        timeData.append(time);
    }

    file.close();

    if (timeData.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Файл не содержит данных!");
        return;
    }

    // Обновляем параметры
    startTime = static_cast<qint64>(timeData.first() * 1000);
    lastTime = timeData.last() - timeData.first();
    dataPointNumber = timeData.size();
    channels = channelCount;

    /*=====Заполняем метаданные в таблице=====*/
    QDateTime firstDT = QDateTime::fromMSecsSinceEpoch(startTime);
    QDateTime lastDT = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(timeData.last() * 1000));

    QString startDate = firstDT.toString("dd.MM.yyyy");
    QString startTimeStr = firstDT.toString("hh:mm:ss.zzz");
    QString lastDate = lastDT.toString("dd.MM.yyyy");
    QString lastTimeStr = lastDT.toString("hh:mm:ss.zzz");

    ui->tableWidget->blockSignals(true);
    for (int i = 0; i < channelCount && i < 8; ++i) {
        // Количество пакетов (колонка 3)
        if (ui->tableWidget->item(i, 3)) {
            ui->tableWidget->item(i, 3)->setText(QString::number(dataPointNumber));
        }
        // Дата начала замера (колонка 4)
        if (ui->tableWidget->item(i, 4)) {
            ui->tableWidget->item(i, 4)->setText(startDate);
        }
        // Время начала замера (колонка 5)
        if (ui->tableWidget->item(i, 5)) {
            ui->tableWidget->item(i, 5)->setText(startTimeStr);
        }
        // Текущая дата (колонка 6) - последняя дата
        if (ui->tableWidget->item(i, 6)) {
            ui->tableWidget->item(i, 6)->setText(lastDate);
        }
        // Текущее время (колонка 7) - последнее время
        if (ui->tableWidget->item(i, 7)) {
            ui->tableWidget->item(i, 7)->setText(lastTimeStr);
        }
    }
    ui->tableWidget->blockSignals(false);
    /*=====Заполняем метаданные в таблице=====*/

    // Создаем графики
    for (int i = 0; i < channelCount; ++i) {
        ui->plot->addGraph();
        QPen pen;
        pen.setWidth(Width_pen_graph);
        pen.setColor(line_colors[i % CUSTOM_LINE_COLORS]);
        ui->plot->graph(i)->setPen(pen);

        QString name = QString("Канал %1").arg(i + 1);
        if (i < ui->tableWidget->rowCount() && ui->tableWidget->item(i, 0)) {
            name = ui->tableWidget->item(i, 0)->text();
        }
        ui->plot->graph(i)->setName(name);
        ui->plot->graph(i)->setData(timeData, channelData[i]);

        if (ui->plot->legend->item(i)) {
            ui->plot->legend->item(i)->setTextColor(line_colors[i % CUSTOM_LINE_COLORS]);
        }

        ui->listWidget_Channels->addItem(name);
        ui->listWidget_Channels->item(i)->setForeground(QBrush(line_colors[i % CUSTOM_LINE_COLORS]));

        // Трекеры
        QCPItemLine *lineX = new QCPItemLine(ui->plot);
        lineX->setLayer("overlay");
        lineX->setPen(QPen(line_colors[i % CUSTOM_LINE_COLORS], 1, Qt::DashLine));
        lineX->setVisible(false);
        tracerLinesX.append(lineX);

        QCPItemLine *lineY = new QCPItemLine(ui->plot);
        lineY->setLayer("overlay");
        lineY->setPen(QPen(line_colors[i % CUSTOM_LINE_COLORS], 1, Qt::DashLine));
        lineY->setVisible(false);
        tracerLinesY.append(lineY);

        QCPItemText *label = new QCPItemText(ui->plot);
        label->setLayer("overlay");
        label->setFont(QFont("Arial", 8));
        label->setBrush(QBrush(QColor(255, 255, 255, 220)));
        label->setPadding(QMargins(4, 2, 4, 2));
        label->setVisible(false);
        tracerLabels.append(label);
    }

    // Настраиваем оси
    ui->plot->xAxis->setLabel("Время");
    ui->plot->xAxis->setNumberFormat("f");
    ui->plot->xAxis->setNumberPrecision(0);

    QSharedPointer<QCPAxisTickerText> timeTicker(new QCPAxisTickerText);
    ui->plot->xAxis->setTicker(timeTicker);

    // Обновляем скролл
    if (dataPointNumber > 0) {
        int maxScroll = static_cast<int>(lastTime - x_scale_value);
        if (maxScroll < 0)
            maxScroll = 0;
        ui->horizontalScrollBar->setMaximum(maxScroll);
        ui->horizontalScrollBar->setPageStep(static_cast<int>(x_scale_value));
        ui->horizontalScrollBar->setSingleStep(10);

        if (!flag_Graph_moove && lastTime > x_scale_value) {
            ui->horizontalScrollBar->setValue(maxScroll);
            Horizontal_scroll_value = maxScroll;
        }
    }

    // Масштабируем Y
    ui->plot->yAxis->rescale(true);
    double lower = ui->plot->yAxis->range().lower;
    double upper = ui->plot->yAxis->range().upper;
    double margin = (upper - lower) * 0.1;
    ui->plot->yAxis->setRange(lower - margin, upper + margin);

    updateTimeTicks();
    updateSpinBoxMax();
    ui->plot->replot();

    ui->statusBar->showMessage("Файл загружен: " + filePath + " (" + QString::number(channelCount) + " каналов, " + QString::number(timeData.size()) + " точек)");
    setWindowTitle("Файл: " + filePath + " - " + WindowTitle);
}
void MainWindow::clearPlot() {
    ui->plot->clearItems();
    ui->plot->clearPlottables();
    ui->listWidget_Channels->clear();

    tracerLinesX.clear();
    tracerLinesY.clear();
    tracerLabels.clear();

    channels = 0;
    dataPointNumber = 0;
    startTime = 0;
    lastTime = 0;
    hasNewData = false;
    needAxisUpdate = false;

    ui->horizontalScrollBar->setValue(0);
    ui->horizontalScrollBar->setMaximum(0);
    ui->horizontalScrollBar->setPageStep(1);
    ui->horizontalScrollBar->setSingleStep(10);
    Horizontal_scroll_value = 0;

    //Сбросим диапазон
    ui->plot->yAxis->setRange(-10, 10);
    ui->plot->xAxis->setRange(0, x_scale_value);

    ui->plot->replot();

    ui->spinBox->setMaximum(1);
    ui->spinBox->setValue(1);
}

void MainWindow::on_channel_count_valueChanged(int arg1) {
    Data_count = arg1;

    if (Data_count > 8) {
        Data_count = 8;
        ui->channel_count->setValue(8);
    }
    if (Data_count < 1) {
        Data_count = 1;
        ui->channel_count->setValue(1);
    }

    setupTable();

    if (channels > 0) {
        clearPlot();
        channels = 0;

        ui->plot->xAxis->setRange(0, x_scale_value);
        ui->plot->yAxis->setRange(-10, 10);
        updateTimeTicks();

        qDebug() << "Data_count изменён на:" << Data_count;

    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (obj == ui->plot && event->type() == QEvent::Wheel) {
        QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);

        if (wheelEvent->modifiers() & Qt::ControlModifier) {
            int currentValue = ui->spinBox->value();
            int delta = wheelEvent->angleDelta().y();

            if (currentValue < 100) {
                if (delta > 0) {
                    currentValue -= 1;
                } else if (delta < 0) {
                    currentValue += 1;
                }
            } else if (currentValue >= 100 && currentValue < 1000) {
                if (delta > 0) {
                    currentValue -= 10;
                } else if (delta < 0) {
                    currentValue += 10;
                }
            } else if (currentValue >= 1000) {
                if (delta > 0) {
                    currentValue -= 100;
                } else if (delta < 0) {
                    currentValue += 100;
                }
            }

            currentValue = qBound(1, currentValue, 65535);
            ui->spinBox->setValue(currentValue);

            return true;
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::on_checkBox_3_stateChanged(int arg1) {
    Dynamic_range_for_x = arg1;
}

// ============================================================
// ОСНОВНОЙ МЕТОД - ОБРАБОТЧИК ИЗМЕНЕНИЯ ИМЕНИ В ТАБЛИЦЕ
// ============================================================
void MainWindow::onTableItemChanged(QTableWidgetItem *item) {

    // Проверяем, что изменяемая ячейка находится в колонке "Имя" (колонка 0)
    if (item->column() != 0)
        return;

    int row = item->row();

    // Проверяем, что индекс в пределах таблицы
    if (row < 0 || row >= ui->tableWidget->rowCount())
        return;

    QString newName = item->text();

    if (newName.trimmed().isEmpty()) {
        newName = QString("Канал %1").arg(row + 1);
        ui->tableWidget->blockSignals(true);
        item->setText(newName);
        ui->tableWidget->blockSignals(false);
    }

    qDebug() << "=== TABLE ITEM CHANGED ===";
    qDebug() << "Row:" << row << "New name:" << newName;
    qDebug() << "Graph count:" << ui->plot->graphCount();

    // Обновляем лейблы ВСЕГДА (даже если графиков нет)
    QLabel *labels[] = { ui->label_precision_ch1, ui->label_precision_ch2, ui->label_precision_ch3, ui->label_precision_ch4, ui->label_precision_ch5, ui->label_precision_ch6, ui->label_precision_ch7,
                         ui->label_precision_ch8 };

    if (row < 8 && labels[row] != nullptr) {
        labels[row]->setText(newName);
        qDebug() << "Label" << row << "updated to:" << newName;
    }

    // Обновляем график, если он существует
    if (row < ui->plot->graphCount()) {
        ui->plot->graph(row)->setName(newName);
        qDebug() << "Graph" << row << "updated";
    }

    // Обновляем список каналов
    if (row < ui->listWidget_Channels->count()) {
        QListWidgetItem *listItem = ui->listWidget_Channels->item(row);
        if (listItem) {
            listItem->setText(newName);
            qDebug() << "ListWidget item" << row << "updated";
        }
    }

    // Обновляем легенду, если графики есть
    if (ui->plot->graphCount() > 0) {
        ui->plot->replot();
    }
}

void MainWindow::closeDataFile() {
    // Сначала обновляем заголовок, если файл открыт
    if (m_isDataFileOpen && m_dataFile.isOpen()) {
        updateFileHeader();  // Перезаписываем заголовок с актуальными данными
        m_dataFile.close();
        m_isDataFileOpen = false;
        qDebug() << "Файл закрыт с обновленным заголовком";
    }
}

// Функция для форматирования секунд в красивый вид
QString MainWindow::formatTime(int seconds) {
    if (seconds < 60) {
        return QString("%1 сек.").arg(seconds);
    } else if (seconds < 3600) {
        int minutes = seconds / 60;
        int secs = seconds % 60;
        return QString("%1 мин. %2 сек.").arg(minutes).arg(secs);
    } else {
        int hours = seconds / 3600;
        int minutes = (seconds % 3600) / 60;
        int secs = seconds % 60;
        return QString("%1 ч. %2 мин. %3 сек.").arg(hours).arg(minutes).arg(secs);
    }
}

//Задаем настройки прецизионности на канал
void MainWindow::on_comboBox_precision_ch1_currentIndexChanged(int index) {
    precisionValues[0] = index;
    updateTablePrecision(0);
}

void MainWindow::on_comboBox_precision_ch2_currentIndexChanged(int index) {
    precisionValues[1] = index;
    updateTablePrecision(1);
}

void MainWindow::on_comboBox_precision_ch3_currentIndexChanged(int index) {
    precisionValues[2] = index;
    updateTablePrecision(2);
}

void MainWindow::on_comboBox_precision_ch4_currentIndexChanged(int index) {
    precisionValues[3] = index;
    updateTablePrecision(3);
}

void MainWindow::on_comboBox_precision_ch5_currentIndexChanged(int index) {
    precisionValues[4] = index;
    updateTablePrecision(4);
}

void MainWindow::on_comboBox_precision_ch6_currentIndexChanged(int index) {
    precisionValues[5] = index;
    updateTablePrecision(5);
}

void MainWindow::on_comboBox_precision_ch7_currentIndexChanged(int index) {
    precisionValues[6] = index;
    updateTablePrecision(6);
}

void MainWindow::on_comboBox_precision_ch8_currentIndexChanged(int index) {
    precisionValues[7] = index;
    updateTablePrecision(7);
}

void MainWindow::updateTablePrecision(int channelIndex) {
    if (channelIndex < 0 || channelIndex >= (int)Data_count)
        return;

    QTableWidgetItem *item = ui->tableWidget->item(channelIndex, 1);
    if (!item)
        return;

    // Получаем текущее значение из ячейки
    QString text = item->text();
    bool ok;
    double value = text.toDouble(&ok);

    if (ok) {
        // Переформатируем с новой точностью
        int precision = precisionValues[channelIndex];
        item->setText(QString::number(value, 'f', precision));
    }
}

int MainWindow::getPrecisionForChannel(int channelIndex) const {
    if (channelIndex < 0 || channelIndex >= 8)
        return 4; // Значение по умолчанию

    return precisionValues[channelIndex];
}

void MainWindow::updateTableHeaders(bool isReadMode) {
    ui->tableWidget->blockSignals(true);

    QStringList headers;
    if (isReadMode) {
        // Заголовки для режима ЧТЕНИЯ
        headers << "        Имя        " << "Значение" << "Ед. измерения" << "   Пакет   "
                << "Дата начала замера" << "Время начала замера"
                << "Дата окончания замера" << "Время окончания замера" << "Описание";
    } else {
        // Заголовки для режима ЗАПИСИ
        headers << "        Имя        " << "Значение" << "Ед. измерения" << "   Пакет   "
                << "Дата начала замера" << "Время начала замера"
                << "Текущая дата" << "Текущее время" << "Описание";
    }

    ui->tableWidget->setHorizontalHeaderLabels(headers);
    ui->tableWidget->blockSignals(false);
}


//Функция обновления шапки файла
void MainWindow::updateFileHeader() {
    if (!m_isDataFileOpen || !m_dataFile.isOpen()) {
        return;
    }

    // Буфер для 1024 байт
    const int HEADER_SIZE = 1024;
    QByteArray headerBuffer;
    headerBuffer.resize(HEADER_SIZE);
    memset(headerBuffer.data(), 0, HEADER_SIZE);

    // Заполняем структуру в буфере
    FileHeader* header = reinterpret_cast<FileHeader*>(headerBuffer.data());
    header->channelCount = static_cast<uint8_t>(Data_count);

    for (int i = 0; i < static_cast<int>(Data_count) && i < 8; ++i) {
        QTableWidgetItem *nameItem = ui->tableWidget->item(i, 0);
        if (nameItem) {
            strncpy(header->channels[i].name,
                    nameItem->text().toUtf8().constData(),
                    sizeof(header->channels[i].name) - 1);
            header->channels[i].name[sizeof(header->channels[i].name) - 1] = '\0';
        }

        QTableWidgetItem *unitItem = ui->tableWidget->item(i, 2);
        if (unitItem) {
            strncpy(header->channels[i].unit,
                    unitItem->text().toUtf8().constData(),
                    sizeof(header->channels[i].unit) - 1);
            header->channels[i].unit[sizeof(header->channels[i].unit) - 1] = '\0';
        }
    }

    // Перемещаемся в начало и записываем 1024 байта
    if (!m_dataFile.seek(0)) {
        qDebug() << "Не удалось переместиться в начало файла";
        return;
    }

    qint64 bytesWritten = m_dataFile.write(headerBuffer);
    if (bytesWritten != HEADER_SIZE) {
        qDebug() << "Ошибка перезаписи заголовка! Записано:" << bytesWritten << "байт";
        return;
    }

    m_dataFile.flush();
    qDebug() << "Заголовок (1024 байт) успешно обновлен";
}

//Светлая тема
void MainWindow::on_Theme_White_triggered(){
    ui->Theme_White->setChecked(true);
    ui->Theme_Dark->setChecked(false);
    Theme = Theme_White;

    //График
    gui_colors[0] = QColor(249, 249, 249, 255);   // Задний фон графика
    gui_colors[1] = QColor(170, 170, 170, 255),   // Grid color
    gui_colors[2] = QColor(30, 30, 30, 255);      // Цвет текста графика
    gui_colors[3] = QColor(245, 245, 245, 240);    // Цвет задника у легенды

    //Линии
    line_colors[0] = QColor(205, 34, 46, 255);     // красный
    line_colors[1] = QColor(19, 84, 208, 255);     // Синий
    line_colors[2] = QColor(0, 144, 144, 255);     // зеленый
    line_colors[3] = QColor(213, 193, 90, 255);    // золотой
    line_colors[4] = QColor(218, 166, 168, 255);   // розовый
    line_colors[5] = QColor(176, 0, 13, 255);      // красный насыщенней
    line_colors[6] = QColor(159, 109, 25);         // коричневый
    line_colors[7] = QColor(173, 173, 173);        // серый

    setupPlot();


}

//Темная тема
void MainWindow::on_Theme_Dark_triggered(){
    ui->Theme_Dark->setChecked(true);
    ui->Theme_White->setChecked(false);
    Theme = Theme_Dark;


    //График
    gui_colors[0] = QColor(20, 20, 20, 255);   // Задний фон графика
    gui_colors[1] = QColor(116, 116, 116, 255),   // Grid color
    gui_colors[2] = QColor(206, 207, 206, 255);      // Цвет текста графика
    gui_colors[3] = QColor(20, 20, 20, 240);    // Цвет задника у легенды

    //Линии (А ля 8 канальный RIGOL)
    line_colors[0] = QColor(250, 251, 10, 255);     // Yellow
    line_colors[1] = QColor(27, 255, 255, 255);     // Cyan
    line_colors[2] = QColor(255, 0, 255, 255);     // Magenta
    line_colors[3] = QColor(10, 133, 255, 255);    // Blue
    line_colors[4] = QColor(255, 128, 0, 255);   // Orange
    line_colors[5] = QColor(0, 249, 0, 255);      // Green
    line_colors[6] = QColor(244, 2, 61);         // Red
    line_colors[7] = QColor(68, 168, 119);        // Green темнее

    setupPlot();
}


