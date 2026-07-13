/*
 * ModbusRTU.c
 *
 *  Библиотека для работы по протоколу ModbusRTU в режиме Master.
 *  Функции: формировать запрос, вычислять CRC16.
 *
 *  Created on: Nov 16, 2021
 *      Author: Oleg Volkov
 */

#ifndef SRC_MODBUSRTU_C_
#define SRC_MODBUSRTU_C_

#include "main.h"
#include "ModbusRTU.h"
#include "serial.h"
#include <stdint.h>

extern HANDLE hSerial;

/*====================================Добавить в main.c====================================*/
volatile uint8_t ModbusRTU_Counter_in_queue = 0; //Счетчик запросов в отчереди на отправку слейв устройствам. Номер счетчика соответствует отправленному запросу.
bool flag_ModbusRTU_request_on = true; //флаг, разрешающий отправку пакета
bool flag_ModbusRTU_block = false; //флаг, блокирующий работу мастера, пока не обработается входящее сообщение
uint16_t exp_num_bytes = 0; //Сколько байт мы ожидаем при ответе?
uint8_t ModbusRTU_tx_buffer[ModbusRTU_TX_BUFFER_SIZE] = { 0, };
/*====================================Добавить в main.c====================================*/



/*------------------------------CRC16(ModbusRTU)----------------------------------*/
uint16_t ModbusRTU_CRC16_Calculate(uint8_t *data, uint8_t lenght, uint8_t byte_order) {
///Функция рассчета CRC16.
/// \param *data - массив данных
///	\param lenght - длина массива данных
/// \param byte_order - порядок байт:
///0 - ABCD(младшим регистром вперед, младшим байтом вперед),
///1 - CDAB(старшим регистром вперед, младшим байтом вперед),
///2 - BADC(младшим регистром вперед, старшим байтом вперед),
///3 - DCBA(старшим регистром вперед, старшим байтом вперед).

    uint16_t crc = 0xFFFF;
    while (lenght--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x01) {
                crc = (crc >> 1u) ^ 0xA001;
            } else {
                crc = crc >> 1u;
            }
        }
    }
    switch (byte_order) {
        case (CRC_BYTE_ORDER_ADCD): //ABCD(младшим регистром вперед, младшим байтом вперед),
            break;
        case (CRC_BYTE_ORDER_CDAB): //DCAB(старшим регистром вперед, младшим байтом вперед),
            crc = (crc << 8u) | (crc >> 8u);
            break;
        case (CRC_BYTE_ORDER_BADC): //BACD(младшим регистром вперед, старшим байтом вперед),
            crc = (((crc >> 8u) & 0x0F) << 12u) | ((crc >> 12u) << 8u) | ((crc << 12u) << 4u) | ((crc >> 4u) & 0x00F);
            break;
        case (CRC_BYTE_ORDER_DCBA): //DCBA(старшим регистром вперед, старшим байтом вперед).
            crc = (((crc >> 8u) & 0x0F) << 4u) | (crc >> 12u) | ((crc << 12u) << 12u) | (((crc >> 4u) & 0x00F) << 8u);
            break;
    }
    return crc;
}
/*------------------------------CRC16(ModbusRTU)----------------------------------*/

/*------------------------------------Функция 0x01 Read Coils----------------------------------- */
void ModbusRTU_Read_Coils_0x01(uint8_t Slave_ID, uint16_t Read_adress, uint16_t Quantity, uint8_t Slave_byte_order) {
///Функция 0x01 Read Coils
/// \param Slave_ID - ID Modbus RTU Slave устройства, к которому обращаемся
/// \param Read_adress - Адрес, с которого начинаем чтение данных
/// \param Quantity - количество запрашиваемых 16ти битных данных.
    //Quantity должен быть от 1 до 2000.
/// \param Slave_byte_order - Порядок регистра и байт в ModbusRTU Slave устройстве

    ModbusRTU_tx_buffer[0] = Slave_ID;
    ModbusRTU_tx_buffer[1] = 0x01;
    ModbusRTU_tx_buffer[2] = (uint16_t) Read_adress >> 8u;
    ModbusRTU_tx_buffer[3] = (uint16_t) Read_adress & 0x00FF;
    ModbusRTU_tx_buffer[4] = (uint16_t) Quantity >> 8u;
    ModbusRTU_tx_buffer[5] = (uint16_t) Quantity & 0x00FF;
    uint16_t CRC16 = ModbusRTU_CRC16_Calculate(ModbusRTU_tx_buffer, 6, Slave_byte_order);
    ModbusRTU_tx_buffer[6] = (uint16_t) CRC16 >> 8u;
    ModbusRTU_tx_buffer[7] = (uint16_t) CRC16 & 0x00FF;
    serial_write_data(hSerial, ModbusRTU_tx_buffer, 8);
    exp_num_bytes = (Quantity*2) + 5; //Ожидаем получить пакет из (Адрес слейва, функция, кол-во байт data, + Quantity*2 + CRC16) байт
}
/*------------------------------------Функция 0x01 Read Coils----------------------------------- */

/*--------------------------------Функция 0x02 Read Discrete Inputs----------------------------- */
void ModbusRTU_Read_Discrete_Inputs_0x02(uint8_t Slave_ID, uint16_t Read_adress, uint16_t Quantity, uint8_t Slave_byte_order) {
///Функция 0x02 Read Discrete Inputs
/// \param Slave_ID - ID Modbus RTU Slave устройства, к которому обращаемся
/// \param Read_adress - Адрес, с которого начинаем чтение данных
/// \param Quantity - количество запрашиваемых 16ти битных данных.
    //Quantity должен быть от 1 до 2000.
/// \param Slave_byte_order - Порядок регистра и байт в ModbusRTU Slave устройстве

    ModbusRTU_tx_buffer[0] = Slave_ID;
    ModbusRTU_tx_buffer[1] = 0x02;
    ModbusRTU_tx_buffer[2] = (uint16_t) Read_adress >> 8u;
    ModbusRTU_tx_buffer[3] = (uint16_t) Read_adress & 0x00FF;
    ModbusRTU_tx_buffer[4] = (uint16_t) Quantity >> 8u;
    ModbusRTU_tx_buffer[5] = (uint16_t) Quantity & 0x00FF;
    uint16_t CRC16 = ModbusRTU_CRC16_Calculate(ModbusRTU_tx_buffer, 6, Slave_byte_order);
    ModbusRTU_tx_buffer[6] = (uint16_t) CRC16 >> 8u;
    ModbusRTU_tx_buffer[7] = (uint16_t) CRC16 & 0x00FF;
    serial_write_data(hSerial, ModbusRTU_tx_buffer, 8);
    exp_num_bytes = (Quantity*2) + 5; //Ожидаем получить пакет из (Адрес слейва, функция, кол-во байт data, + Quantity*2 + CRC16) байт
}
/*--------------------------------Функция 0x02 Read Discrete Inputs----------------------------- */

/*------------------------------Функция 0x03 Read Holding Registers----------------------------- */
void ModbusRTU_Read_Holding_Registers_0x03(uint8_t Slave_ID, uint16_t Read_adress, uint8_t Quantity, uint8_t Slave_byte_order) {
///Функция 0x03 Read Holding Registers
/// \param Slave_ID - ID Modbus RTU Slave устройства, к которому обращаемся
/// \param Read_adress - Адрес, с которого начинаем чтение данных
/// \param Quantity - количество запрашиваемых 16ти битных данных. Если мы запрашиваем Float, то Quantity кратно 2, т.е. 32 бита. 2 Float переменные - соответственно Quantity = 4. И т.д.
    //Quantity должен быть от 1 до 125.
/// \param Slave_byte_order - Порядок регистра и байт в ModbusRTU Slave устройстве

    ModbusRTU_tx_buffer[0] = Slave_ID;
    ModbusRTU_tx_buffer[1] = 0x03;
    ModbusRTU_tx_buffer[2] = (uint16_t) Read_adress >> 8u;
    ModbusRTU_tx_buffer[3] = (uint16_t) Read_adress & 0x00FF;
    ModbusRTU_tx_buffer[4] = 0x00;
    ModbusRTU_tx_buffer[5] = Quantity;
    uint16_t CRC16 = ModbusRTU_CRC16_Calculate(ModbusRTU_tx_buffer, 6, Slave_byte_order);
    ModbusRTU_tx_buffer[6] = (uint16_t) CRC16 >> 8u;
    ModbusRTU_tx_buffer[7] = (uint16_t) CRC16 & 0x00FF;
    serial_write_data(hSerial, ModbusRTU_tx_buffer, 8);
    exp_num_bytes = (Quantity*2) + 5; //Ожидаем получить пакет из (Адрес слейва, функция, кол-во байт data, + Quantity*2 + CRC16) байт
}
/*------------------------------Функция 0x03 Read Holding Registers----------------------------- */

/*--------------------------------Функция 0x04 Read Input Registers----------------------------- */
void ModbusRTU_Read_Input_Registers_0x04(uint8_t Slave_ID, uint16_t Read_adress, uint8_t Quantity, uint8_t Slave_byte_order) {
///Функция 0x04 Read Discrete Inputs
/// \param Slave_ID - ID Modbus RTU Slave устройства, к которому обращаемся
/// \param Read_adress - Адрес, с которого начинаем чтение данных
/// \param Quantity - количество запрашиваемых 16ти битных данных.
    //Quantity должен быть от 1 до 125.
/// \param Slave_byte_order - Порядок регистра и байт в ModbusRTU Slave устройстве

    ModbusRTU_tx_buffer[0] = Slave_ID;
    ModbusRTU_tx_buffer[1] = 0x04;
    ModbusRTU_tx_buffer[2] = (uint16_t) Read_adress >> 8u;
    ModbusRTU_tx_buffer[3] = (uint16_t) Read_adress & 0x00FF;
    ModbusRTU_tx_buffer[4] = 0x00;
    ModbusRTU_tx_buffer[5] = Quantity;
    uint16_t CRC16 = ModbusRTU_CRC16_Calculate(ModbusRTU_tx_buffer, 6, Slave_byte_order);
    ModbusRTU_tx_buffer[6] = (uint16_t) CRC16 >> 8u;
    ModbusRTU_tx_buffer[7] = (uint16_t) CRC16 & 0x00FF;
    serial_write_data(hSerial, ModbusRTU_tx_buffer, 8);
}
/*--------------------------------Функция 00x04 Read Input Registers----------------------------- */

/*--------------------------------------------Функция 0x10 Preset Multiple Registers---------------------------------------------*/
uint8_t ModbusRTU_Preset_Multiple_Registers_0x10(uint8_t Slave_ID, uint16_t Write_adress, uint16_t Quantity_registers, int16_t *value, uint8_t Slave_byte_order) {
///Функция 0x10 Preset Multiple Registers(собирает массив данных на запрос по ModbusRTU и возвращает длинну собранного буфера данных)
/// \param Slave_ID - ID Modbus RTU Slave устройства, к которому обращаемся
/// \param Write_adress - Адрес, с которого начинаем запись данных
/// \param Quantity_registers - количество запрашиваемых 16ти битных ячеек, куда будем кидать данные.
// Обратите внимание, что далеко не все контроллеры поддерживают множественную запись, а довольствуются записью по-одному параметру.
/// \param *value - указатель на данные, которые будем отправлять. Лучше всего работать с массивом данных.
/// \param Slave_byte_order - Порядок регистра и байт в ModbusRTU Slave устройстве
    uint8_t buffer_size = 0;
    ModbusRTU_tx_buffer[0] = Slave_ID;
    ModbusRTU_tx_buffer[1] = 0x10;
    ModbusRTU_tx_buffer[2] = *((uint8_t*) &Write_adress + 1);
    ModbusRTU_tx_buffer[3] = *(uint8_t*) &Write_adress;
    ModbusRTU_tx_buffer[4] = *((uint8_t*) &Quantity_registers + 1);
    ModbusRTU_tx_buffer[5] = *(uint8_t*) &Quantity_registers;
    ModbusRTU_tx_buffer[6] = Quantity_registers * 2;
    for (uint8_t i = 0; i < Quantity_registers; i++) {
        ModbusRTU_tx_buffer[7 + (i * 2)] = *((int8_t*) value + (1 + (i * 2)));
        ModbusRTU_tx_buffer[8 + (i * 2)] = *((int8_t*) value + (i * 2));
        buffer_size = 8 + (i * 2) + 1;
    }
    uint16_t CRC16 = ModbusRTU_CRC16_Calculate(ModbusRTU_tx_buffer, buffer_size, Slave_byte_order);
    ModbusRTU_tx_buffer[buffer_size] = *((uint8_t*) &CRC16 + 1);
    ModbusRTU_tx_buffer[buffer_size + 1] = *(uint8_t*) &CRC16;
    buffer_size = buffer_size + 2;
    serial_write_data(hSerial, ModbusRTU_tx_buffer, buffer_size);
    return buffer_size;
}
/*--------------------------------------------Функция 0x10 Preset Multiple Registers---------------------------------------------*/

float ModbusRTU_GetData_Float(uint8_t* data_massive, uint8_t* start_data_address, uint8_t Slave_byte_order) {
    uint32_t Data = 0;
    switch (Slave_byte_order) {
        case BYTE32_ORDER_AB_CD:
            Data = (data_massive[*start_data_address] << 24U) | (data_massive[*start_data_address + 1] << 16U) | (data_massive[*start_data_address + 2] << 8U) | (data_massive[*start_data_address + 3]);
            break;
        case BYTE32_ORDER_CD_AB:
            Data = (data_massive[*start_data_address + 2] << 24U) | (data_massive[*start_data_address + 3] << 16U) | (data_massive[*start_data_address] << 8U) | (data_massive[*start_data_address + 1]);
            break;
        case BYTE32_ORDER_BA_DC:
            Data = (data_massive[*start_data_address + 1] << 24U) | (data_massive[*start_data_address] << 16U) | (data_massive[*start_data_address + 3] << 8U) | (data_massive[*start_data_address + 2]);
            break;
        case BYTE32_ORDER_DC_BA:
            Data = (data_massive[*start_data_address + 3] << 24U) | (data_massive[*start_data_address + 2] << 16U) | (data_massive[*start_data_address + 1] << 8U) | (data_massive[*start_data_address]);
            break;
    }
    return *((float*) &Data);
}

uint32_t ModbusRTU_GetData_U32(uint8_t* data_massive, uint8_t* start_data_address, uint8_t Slave_byte_order) {
    uint32_t Data = 0;
    switch (Slave_byte_order) {
        case BYTE32_ORDER_AB_CD:
            Data = (data_massive[*start_data_address] << 24U) | (data_massive[*start_data_address + 1] << 16U) | (data_massive[*start_data_address + 2] << 8U) | (data_massive[*start_data_address + 3]);
            break;
        case BYTE32_ORDER_CD_AB:
            Data = (data_massive[*start_data_address + 2] << 24U) | (data_massive[*start_data_address + 3] << 16U) | (data_massive[*start_data_address] << 8U) | (data_massive[*start_data_address + 1]);
            break;
        case BYTE32_ORDER_BA_DC:
            Data = (data_massive[*start_data_address + 1] << 24U) | (data_massive[*start_data_address] << 16U) | (data_massive[*start_data_address + 3] << 8U) | (data_massive[*start_data_address + 2]);
            break;
        case BYTE32_ORDER_DC_BA:
            Data = (data_massive[*start_data_address + 3] << 24U) | (data_massive[*start_data_address + 2] << 16U) | (data_massive[*start_data_address + 1] << 8U) | (data_massive[*start_data_address]);
            break;
    }
    *start_data_address = *start_data_address + 4;
    return Data;
}

uint16_t ModbusRTU_GetData_U16(uint8_t* data_massive, uint8_t* start_data_address, uint8_t slave_byte_order) {
    uint16_t Data = 0;
    switch (slave_byte_order) {
        case BYTE16_ORDER_AB:
            Data = (data_massive[*start_data_address] << 8) | data_massive[*start_data_address + 1];
            break;
        case BYTE16_ORDER_BA:
            Data = (data_massive[*start_data_address + 1] << 8) | data_massive[*start_data_address];
            break;
               }
    *start_data_address = *start_data_address + 2;
    return Data;
}

uint32_t ModbusRTU_Float_to_U32_convert(float* data, uint8_t slave_byte_order) {
    uint32_t Data = 0;
    Data = *((uint32_t*) data);
    switch (slave_byte_order) {
        case BYTE32_ORDER_AB_CD:
            Data = (*((uint8_t*)&Data + 3) << 24) | (*((uint8_t*)&Data + 2) << 16) | (*((uint8_t*)&Data + 1) << 8) | (*((uint8_t*)&Data));
            break;
        case BYTE32_ORDER_CD_AB:
            Data = (*((uint8_t*)&Data + 1) << 24) | (*((uint8_t*)&Data) << 16) | (*((uint8_t*)&Data + 3) << 8) | (*((uint8_t*)&Data + 2));
            break;
        case BYTE32_ORDER_BA_DC:
            Data = (*((uint8_t*)&Data + 2) << 24) | (*((uint8_t*)&Data + 3) << 16) | (*((uint8_t*)&Data) << 8) | (*((uint8_t*)&Data + 1));
            break;
        case BYTE32_ORDER_DC_BA:
            Data = (*((uint8_t*)&Data) << 24) | (*((uint8_t*)&Data + 1) << 16) | (*((uint8_t*)&Data + 2) << 8) | (*((uint8_t*)&Data + 3));
            break;
    }
    return Data;
}

#endif /* SRC_MODBUSRTU_C_ */
