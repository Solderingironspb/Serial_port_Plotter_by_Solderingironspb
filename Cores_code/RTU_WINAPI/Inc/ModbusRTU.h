/*
 * ModbusRTU.h
 *
 *  Библиотека для работы по протоколу ModbusRTU в режиме Master.
 *  Функции: формировать запрос, вычислять CRC16.
 *
 *  Created on: Nov 16, 2021
 *      Author: Oleg Volkov
 */

#ifndef INC_MODBUSRTU_H_
#define INC_MODBUSRTU_H_

#include <main.h>
#include <stdint.h>
#include <stdbool.h>

#define ModbusRTU_TX_BUFFER_SIZE 256
#define ModbusRTU_RX_BUFFER_SIZE 256

#define MODBUSRTU_NUMBER_OF_REQUESTS    1 //Количество запросов от мастер устройства
#define MODBUSRTU_TIMEOUT               100 //Таймаут ответа от slave устройств (мс)
#define MODBUSRTU_DELAY_BETWEEN_POOL    0 //Задержка между фреймами запроса (мс)

struct ModbusRTU_Data{
	uint16_t ALARM;
	uint16_t SENSOR_STATE;
	uint16_t MUTE_STATE;
	uint32_t MUTE_TIMEOUT;
	uint16_t SENSOR_TIMER_FILTER;
	uint16_t SENSOR1_INVERSION;
	uint16_t SENSOR2_INVERISON;
	uint16_t RTU_ADDR;
	uint16_t USART_SPEED;
	uint16_t SAVE_SETTING;
};

enum{
    BYTE16_ORDER_AB,              	//int16_t или uint16_t
    BYTE16_ORDER_BA,
    BYTE32_ORDER_AB_CD,           	//int32_t, uint32_t, float
    BYTE32_ORDER_CD_AB,
    BYTE32_ORDER_BA_DC,
    BYTE32_ORDER_DC_BA,
    BYTE64_ORDER_AB_CD_EF_GH,     	//int64_t, uint64_t, double
    BYTE64_ORDER_GH_EF_CD_AB,
    BYTE64_ORDER_BA_DC_FE_HG,
    BYTE64_ORDER_HG_FE_DC_BA
};

enum{
	CRC_BYTE_ORDER_ADCD,
	CRC_BYTE_ORDER_CDAB,			//В основном это стандарт
	CRC_BYTE_ORDER_BADC,
	CRC_BYTE_ORDER_DCBA
};



uint16_t ModbusRTU_CRC16_Calculate(uint8_t *data, uint8_t lenght, uint8_t byte_order);
void ModbusRTU_Read_Coils_0x01(uint8_t Slave_ID, uint16_t Read_adress, uint16_t Quantity, uint8_t Slave_byte_order); //Функция 0x01
void ModbusRTU_Read_Discrete_Inputs_0x02(uint8_t Slave_ID, uint16_t Read_adress, uint16_t Quantity, uint8_t Slave_byte_order); //Функция 0x02
void ModbusRTU_Read_Holding_Registers_0x03(uint8_t Slave_ID, uint16_t Read_adress, uint8_t Quantity, uint8_t Slave_byte_order); //Функция 0x03
void ModbusRTU_Read_Input_Registers_0x04(uint8_t Slave_ID, uint16_t Read_adress, uint8_t Quantity, uint8_t Slave_byte_order); //Функция 0x04
uint8_t ModbusRTU_Preset_Multiple_Registers_0x10(uint8_t Slave_ID, uint16_t Write_adress, uint16_t Quantity_registers, int16_t *value, uint8_t Slave_byte_order); //Функция 0x10
float ModbusRTU_GetData_Float(uint8_t* data_massive, uint8_t* start_data_address, uint8_t Slave_byte_order);
uint32_t ModbusRTU_GetData_U32(uint8_t* data_massive, uint8_t* start_data_address, uint8_t Slave_byte_order);
uint16_t ModbusRTU_GetData_U16(uint8_t* data_massive, uint8_t* start_data_address, uint8_t slave_byte_order);
uint32_t ModbusRTU_Float_to_U32_convert(float* data, uint8_t slave_byte_order);

void ModbusRTU_request_queue(uint8_t ModbusRTU_Counter_in_queue);
void ModbusRTU_response_handler(uint8_t ModbusRTU_Counter_in_queue);
void ModbusRTU_Master_pool(void);
void ModbusRTU_Master_pool_run(void);



#endif /* INC_MODBUSRTU_H_ */
