/*
 * serial.h
 *
 *  Created on: 12 янв. 2025 г.
 *      Author: Solderingiron
 */

#ifndef INC_SERIAL_H_
#define INC_SERIAL_H_

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <stdint.h>

struct SerialPort{
	uint8_t tx_data[256];	//Буфер под исходящие байты
	DWORD bytes_written;	//Сколько байт записано
	uint8_t rx_data[256]; 	//Буфер под входящие байты
	DWORD bytes_recieve; 	//Сколько байт прочитано
};

int serial_init(char *com_port_name, DWORD BaudRate, BYTE ByteSize, BYTE Parity, BYTE StopBits);
void serial_info_status(DCB dcb);
int create_thread_serial_read_data(void);
void serial_read_file(void);
void serial_rx_data_processing(void);
int serial_write_data(HANDLE *hSerial, uint8_t* Data, uint16_t size);
int32_t serial_read_data(HANDLE hComm, uint8_t* buf, uint16_t count, int32_t byte_timeout_ms);

#endif /* INC_SERIAL_H_ */
