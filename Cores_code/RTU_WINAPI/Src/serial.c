/*
 * serial.c
 *
 *  Created on: 12 янв. 2025 г.
 *      Author: Solderingiron
 */
#include "main.h"
#include "serial.h"
#include <stdbool.h>

DCB dcb;
HANDLE hSerial;
BOOL fSuccess;
HANDLE hThread;
struct SerialPort Serial;

// Функция инициализации последовательного порта
int serial_init(char *com_port_name, DWORD BaudRate, BYTE ByteSize, BYTE Parity, BYTE StopBits) {
	hSerial = CreateFile(com_port_name,
	GENERIC_READ | GENERIC_WRITE, 0,
	NULL,
	OPEN_EXISTING, 0,
	NULL);

	if (hSerial == INVALID_HANDLE_VALUE) {
		fprintf(stderr,"CreateFile failed with error %ld.\n", GetLastError());
		return (1);
	}

	SecureZeroMemory(&dcb, sizeof(DCB));
	dcb.DCBlength = sizeof(DCB);

	fSuccess = GetCommState(hSerial, &dcb);
	if (!fSuccess) {
		fprintf(stderr,"GetCommState failed with error %ld.\n", GetLastError());
		return (2);
	}

	dcb.BaudRate = BaudRate;
	dcb.ByteSize = ByteSize;
	dcb.Parity = Parity;
	dcb.StopBits = StopBits;

	fSuccess = SetCommState(hSerial, &dcb);
	if (!fSuccess) {
		fprintf(stderr,"SetCommState failed with error %ld.\n", GetLastError());
		return (3);
	}

	fSuccess = GetCommState(hSerial, &dcb);
	if (!fSuccess) {
		fprintf(stderr,"GetCommState failed with error %ld.\n", GetLastError());
		return (2);
	}

	serial_info_status(dcb);
	fprintf(stderr,"%s successfully configured\r\n", com_port_name);

	// Настройка таймаутов
	COMMTIMEOUTS timeouts;
	timeouts.ReadIntervalTimeout = 1000;
	timeouts.ReadTotalTimeoutConstant = 50;
	timeouts.ReadTotalTimeoutMultiplier = 10;
	timeouts.WriteTotalTimeoutConstant = 1000;
	timeouts.WriteTotalTimeoutMultiplier = 0;
	SetCommTimeouts(hSerial, &timeouts);

	return 0; // Успешная инициализация
}

// Функция для вывода параметров COM-порта
void serial_info_status(DCB dcb) {
	fprintf(stderr,"Serial port parameters:\nBaudRate = %ld\nByteSize = %d\n", dcb.BaudRate, dcb.ByteSize);
	fprintf(stderr,"Parity = ");
	switch (dcb.Parity) {
	case 0:
		fprintf(stderr,"NOPARTITY\r\n");
		break;
	case 1:
		fprintf(stderr,"ODDPARTITY\r\n");
		break;
	case 2:
		fprintf(stderr,"EVENPARTITY\r\n");
		break;
	case 3:
		fprintf(stderr,"MARKPARTITY\r\n");
		break;
	case 4:
		fprintf(stderr,"SPACEPARTITY\r\n");
		break;
	}
	fprintf(stderr,"StopBits = ");
	switch (dcb.StopBits) {
	case 0:
		fprintf(stderr,"ONESTOPBIT\r\n");
		break;
	case 1:
		fprintf(stderr,"ONE5STOPBITS\r\n");
		break;
	case 2:
		fprintf(stderr,"TWOSTOPBITS\r\n");
		break;
	}

}

// Функция потока для приема данных из последовательного порта
DWORD WINAPI thread_serial_read_data(LPVOID lpParam) {
	while (1) {
		if (ReadFile(hSerial, Serial.rx_data, sizeof(Serial.rx_data), &Serial.bytes_recieve, NULL)) {
				/*=================Здесь работаем с принятыми данными=================*/

				/*=================Здесь работаем с принятыми данными=================*/
		} else {
			// Обработка ошибок чтения
			DWORD dwError = GetLastError();
			if (dwError != ERROR_IO_PENDING) {
				fprintf(stderr,"Error reading from serial port: %ld\n", dwError);
			}
		}
	}
	return 0;
}

__attribute__((weak)) void serial_rx_data_processing(void) {

}

int create_thread_serial_read_data(void) {
	// Создание потока для чтения данных
	hThread = CreateThread(NULL, 0, thread_serial_read_data, NULL, 0, NULL);
	if (hThread == NULL) {
		fprintf(stderr,"Failed to create thread: %ld\n", GetLastError());
		CloseHandle(hSerial);
		return 1;
	}
	return 0;
}

int serial_write_data(HANDLE *hSerial, uint8_t* Data, uint16_t size) {
	if (!WriteFile(hSerial, Data, size, &Serial.bytes_written, NULL)) {
		fprintf(stderr,"Error writing to serial COM port\n");
		Sleep(1000);
		CloseHandle(hSerial);
		return 1;
	}
	return 0;
}

int32_t serial_read_data(HANDLE hComm, uint8_t* buf, uint16_t count, int32_t byte_timeout_ms) {
    int TotalBytesRead = 0;
    bool Status = false;
    ULONGLONG StartTime = 0;
    uint8_t b;
    DWORD tmpByteCount;

    StartTime = GetTickCount64();

    do {
        // read one byte
        Status = ReadFile(hComm, &b, 1, &tmpByteCount, NULL);

        // can't read from port at all??
        if (!Status)
            return false;

        // put one byte into our buffer
        if (tmpByteCount == 1) {
            buf[TotalBytesRead++] = b;
        }

        // did we time out yet??
        if (GetTickCount64() - StartTime > byte_timeout_ms) {
            break;
        }

    } while (TotalBytesRead < count);

    return TotalBytesRead;
}
