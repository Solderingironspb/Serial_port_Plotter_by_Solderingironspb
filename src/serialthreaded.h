#ifndef SERIALTHREADED_H
#define SERIALTHREADED_H

#include <QObject>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>

class serialthreaded : public QSerialPort{

    Q_OBJECT
public:
    serialthreaded();

public slots:
    void open_port();

signals:
    void emitData(QByteArray Data);
    void portOpenFail();
    void portOpenOK();

private slots:
    void serialRecieve();

};

#endif // SERIALTHREADED_H
