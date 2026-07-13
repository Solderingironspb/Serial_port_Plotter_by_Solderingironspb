/*
 * ModbusRTU_Master.c
 *
 *  Created on: Jan 31, 2024
 *      Author: Solderingiron
 */

#include "main.h"
#include "ModbusRTU.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "serial.h"
#include <time.h>
#include <stdio.h>

extern struct SerialPort Serial;

struct ModbusRTU_Data RTU_Data;

extern uint16_t exp_num_bytes; //Сколько байт мы ожидаем при ответе?
extern HANDLE hSerial;

extern volatile uint8_t ModbusRTU_Counter_in_queue; //Счетчик запросов в отчереди на отправку слейв устройствам. Номер счетчика соответствует отправленному запросу.
extern bool flag_ModbusRTU_request_on; //флаг, разрешающий отправку пакета
extern bool flag_ModbusRTU_block; //флаг, блокирующий работу мастера, пока не обработается входящее сообщение
extern uint8_t Data_count;  // По умолчанию 8 каналов

/**
 ***************************************************************************************
 *  @breif Очередь запросов от мастера к слев устройствам
 *  @param ModbusRTU_Counter_in_queue - номер запроса в очереди
 ***************************************************************************************
 */
void ModbusRTU_request_queue(uint8_t ModbusRTU_Counter_in_queue) {
	switch (ModbusRTU_Counter_in_queue) {
	case 0:
		ModbusRTU_Read_Holding_Registers_0x03(1, 0, (Data_count * 2), CRC_BYTE_ORDER_CDAB);

		break;
	case 1:
		//ModbusRTU_Read_Holding_Registers_0x03(1, 3, 5, CRC_BYTE_ORDER_CDAB);
		break;
	case 2:

		break;
	case 3:

		break;
	case 4:

		break;
	case 5:

		break;
	case 6:

		break;
	}

}

/**
 ***************************************************************************************
 *  @breif Очередь обработки ответов от слейв устройств
 *  @param ModbusRTU_Counter_in_queue - номер запроса в очереди, на который пришел ответ
 ***************************************************************************************
 */
void ModbusRTU_response_handler(uint8_t ModbusRTU_Counter_in_queue) {

	switch (ModbusRTU_Counter_in_queue) {
	case 0:
		/*uint8_t ADDR1 = 3;
		uint8_t ADDR2 = 7;
		uint8_t ADDR3 = 11;
		uint8_t ADDR4 = 15;
		uint8_t ADDR5 = 19;
		uint8_t ADDR6 = 23;
		uint8_t ADDR7 = 27;
		uint8_t ADDR8 = 31;


		float Channel_1;
		float Channel_2;
		float Channel_3;
		float Channel_4;
		float Channel_5;
		float Channel_6;
		float Channel_7;
		float Channel_8;*/
		float Channel_data[8] = {0,};


		uint8_t ADDR[8] = {3, 7, 11, 15, 19, 23, 27, 31};
		for(int i = 0; i < Data_count; i++){

			Channel_data[i] =  ModbusRTU_GetData_Float(Serial.rx_data, (uint8_t*) &ADDR[i], BYTE32_ORDER_CD_AB);
		}

		/*Channel_1 = ModbusRTU_GetData_Float(Serial.rx_data, (uint8_t*) &ADDR1, BYTE32_ORDER_CD_AB);
		Channel_2 = ModbusRTU_GetData_Float(Serial.rx_data, (uint8_t*) &ADDR2, BYTE32_ORDER_CD_AB);
		Channel_3 = ModbusRTU_GetData_Float(Serial.rx_data, (uint8_t*) &ADDR3, BYTE32_ORDER_CD_AB);
		Channel_4 = ModbusRTU_GetData_Float(Serial.rx_data, (uint8_t*) &ADDR4, BYTE32_ORDER_CD_AB);
		Channel_5 = ModbusRTU_GetData_Float(Serial.rx_data, (uint8_t*) &ADDR5, BYTE32_ORDER_CD_AB);
		Channel_6 = ModbusRTU_GetData_Float(Serial.rx_data, (uint8_t*) &ADDR6, BYTE32_ORDER_CD_AB);
		Channel_7 = ModbusRTU_GetData_Float(Serial.rx_data, (uint8_t*) &ADDR7, BYTE32_ORDER_CD_AB);
		Channel_8 = ModbusRTU_GetData_Float(Serial.rx_data, (uint8_t*) &ADDR8, BYTE32_ORDER_CD_AB);*/


		printf("1 = %f\r\n", Channel_data[0]);
		printf("2 = %f\r\n", Channel_data[1]);
		printf("3 = %f\r\n", Channel_data[2]);
		printf("4 = %f\r\n", Channel_data[3]);
		printf("5 = %f\r\n", Channel_data[4]);
		printf("6 = %f\r\n", Channel_data[5]);
		printf("7 = %f\r\n", Channel_data[6]);
		printf("8 = %f\r\n", Channel_data[7]);
		printf("9 = %lld\r\n", (long long)get_unix_timestamp_ms());

		break;
	case 1:

		break;
	case 2:

		break;
	case 3:

		break;
	case 4:

		break;
	case 5:

		break;
	case 6:

		break;
	}
}

/**
 ***************************************************************************************
 *  @breif Функция-распределитель очереди запросов по времени и номеру в очереди
 *  @attention Добавляется в systick или таймер, который работает с периодичностью в 1 мс
 ***************************************************************************************
 */
void ModbusRTU_Master_pool(void) {
	if (!flag_ModbusRTU_block) {
		if (!flag_ModbusRTU_request_on) {
			ModbusRTU_Counter_in_queue++; //Переходим к следующему запросу в очереди
			if (ModbusRTU_Counter_in_queue >= MODBUSRTU_NUMBER_OF_REQUESTS) {
				ModbusRTU_Counter_in_queue = 0;
			}
			flag_ModbusRTU_request_on = true;
		}
		ModbusRTU_Master_pool_run();
	}
}

/**
 ***************************************************************************************
 *  @breif Функция отправки запросов и обработки ответов
 *  @attention Добавляется в main.c в while(1){}
 ***************************************************************************************
 */
void ModbusRTU_Master_pool_run(void) {
	/*===============Отправка запросов мастером по ModbusRTU================*/
	if (flag_ModbusRTU_request_on) { //Если запрос разрешен от мастера
		ModbusRTU_request_queue(ModbusRTU_Counter_in_queue); //формируем пакет из очереди и отправляем слейвам
		flag_ModbusRTU_request_on = false; //сбросим флаг на разрешение запроса ModbusRTU от мастера
	}
	/*===============Отправка запросов мастером по ModbusRTU================*/

	/*===============Обработка ответов от слейв устройств==================*/
	//Serial.bytes_recieve = serial_read_data(hSerial, Serial.rx_data, exp_num_bytes, MODBUSRTU_TIMEOUT);
	ReadFile(hSerial, Serial.rx_data, exp_num_bytes, &Serial.bytes_recieve, 0);
	flag_ModbusRTU_block = true; //поставим блок на дальнейшую работу мастера, пока не обработаем входящие данные
	if (flag_ModbusRTU_block) {
		if (Serial.bytes_recieve > 0) {
			/*Проверка CRC16*/
			uint16_t CRC_check = ModbusRTU_CRC16_Calculate(Serial.rx_data, Serial.bytes_recieve - 2, CRC_BYTE_ORDER_CDAB); //Считаем CRC входящих данных
			uint16_t CRC_rx_buffer = Serial.rx_data[Serial.bytes_recieve - 2] << 8u | Serial.rx_data[Serial.bytes_recieve - 1]; //Смотрим CRC, которая была в пакете
			/*Если CRC16 бьется, то работаем с данными*/
			if (CRC_check == CRC_rx_buffer) {
				ModbusRTU_response_handler(ModbusRTU_Counter_in_queue); //обработка ответов
			}
		}
		flag_ModbusRTU_block = false;
	}
	/*===============Обработка ответов от слейв устройств==================*/

}
