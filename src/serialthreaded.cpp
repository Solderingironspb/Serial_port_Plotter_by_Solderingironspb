#include "serialthreaded.h"
#include <QDebug>

serialthreaded::serialthreaded(){

}

void serialthreaded::serialRecieve(){
     QByteArray newData = this->readAll();
     emit emitData(newData);

}

void serialthreaded::open_port(){
    if (this->open(QIODevice::ReadWrite)){
        emit portOpenOK();
    }else{
        emit portOpenFail();
    }
}
