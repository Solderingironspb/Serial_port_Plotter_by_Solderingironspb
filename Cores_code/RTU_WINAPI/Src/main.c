#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include "serial.h"
#include <time.h>
#include <stdbool.h>
#include "ModbusRTU.h"
#include <string.h>

extern HANDLE hSerial;
uint16_t Time_sleep = 0;
uint8_t Data_count = 8;  // По умолчанию 8 каналов

// Получить Unix timestamp в миллисекундах (Windows)
long long get_unix_timestamp_ms() {
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);

	// Конвертируем FILETIME в Unix timestamp (миллисекунды)
	ULARGE_INTEGER uli;
	uli.LowPart = ft.dwLowDateTime;
	uli.HighPart = ft.dwHighDateTime;

	// FILETIME: 100-наносекундные интервалы с 01.01.1601
	// Unix: секунды с 01.01.1970
	// Разница: 116444736000000000 (100-наносекундных интервалов)
	const unsigned long long EPOCH_DIFFERENCE = 116444736000000000ULL;
	// Переводим в миллисекунды (делим на 10000)
	long long timestamp_ms = (uli.QuadPart - EPOCH_DIFFERENCE) / 10000;
	return timestamp_ms;
}

void print_usage(const char* program_name) {
	fprintf(stderr, "USE: %s <COM Port> <Bits per second> <data bits> <parity> <stop bits> [sleep_ms] [data_count]\n", program_name);
	fprintf(stderr, "Example: %s 10 9600 8 NOPARITY ONESTOPBIT\n", program_name);
	fprintf(stderr, "Example with sleep: %s 10 9600 8 NOPARITY ONESTOPBIT 100\n", program_name);
	fprintf(stderr, "Example with all params: %s 10 9600 8 NOPARITY ONESTOPBIT 100 4\n", program_name);
	fprintf(stderr, "Parity: NOPARITY, ODDPARITY, EVENPARITY, MARKPARITY, SPACEPARITY\n");
	fprintf(stderr, "Stop bits: ONESTOPBIT, ONE5STOPBITS, TWOSTOPBITS\n");
	fprintf(stderr, "sleep_ms: optional delay between polls in milliseconds (default: 0)\n");
	fprintf(stderr, "data_count: optional number of channels (1-8, default: 8)\n");
}

int main(int argc, char *argv[]) {
	setbuf(stdout, NULL);
	setvbuf(stderr, NULL, _IONBF, 0);
	SetConsoleOutputCP(CP_UTF8);

	// Проверяем минимальное количество аргументов (5 обязательных)
	if (argc < 6) {
		print_usage(argv[0]);
		return 1;
	}

	// Проверяем, что аргументов не больше 8 (5 обязательных + 2 опциональных)
	if (argc > 8) {
		fprintf(stderr, "Error: Too many arguments (max 7)\n");
		print_usage(argv[0]);
		return 1;
	}

	char port[20];
	sprintf(port, "\\\\.\\COM%s", argv[1]);

	int baudrate = atoi(argv[2]);
	int byteSize = atoi(argv[3]);

	DWORD parity;
	if (strcmp(argv[4], "NOPARITY") == 0)
		parity = NOPARITY;
	else if (strcmp(argv[4], "ODDPARITY") == 0)
		parity = ODDPARITY;
	else if (strcmp(argv[4], "EVENPARITY") == 0)
		parity = EVENPARITY;
	else if (strcmp(argv[4], "MARKPARITY") == 0)
		parity = MARKPARITY;
	else if (strcmp(argv[4], "SPACEPARITY") == 0)
		parity = SPACEPARITY;
	else {
		fprintf(stderr, "Error: Unknown parity parameter '%s'\n", argv[4]);
		print_usage(argv[0]);
		return 1;
	}

	DWORD stopBits;
	if (strcmp(argv[5], "ONESTOPBIT") == 0)
		stopBits = ONESTOPBIT;
	else if (strcmp(argv[5], "ONE5STOPBITS") == 0)
		stopBits = ONE5STOPBITS;
	else if (strcmp(argv[5], "TWOSTOPBITS") == 0)
		stopBits = TWOSTOPBITS;
	else {
		fprintf(stderr, "Error: Unknown stop bits parameter '%s'\n", argv[5]);
		print_usage(argv[0]);
		return 1;
	}

	// Обработка необязательного 6-го аргумента (задержка)
	if (argc >= 7) {
		int sleep_value = atoi(argv[6]);
		if (sleep_value < 0) {
			fprintf(stderr, "Warning: sleep_ms cannot be negative, using 0\n");
			Time_sleep = 0;
		} else if (sleep_value > 65535) {
			fprintf(stderr, "Warning: sleep_ms exceeds maximum value 65535, clamping\n");
			Time_sleep = 65535;
		} else {
			Time_sleep = (uint16_t)sleep_value;
		}
		fprintf(stderr, "Sleep between polls: %u ms\n", Time_sleep);
	} else {
		Time_sleep = 0;
		fprintf(stderr, "Sleep between polls: 0 ms (no delay)\n");
	}

	// Обработка необязательного 7-го аргумента (Data_count)
	if (argc >= 8) {
		int data_count_value = atoi(argv[7]);
		if (data_count_value < 1) {
			fprintf(stderr, "Warning: data_count cannot be less than 1, using default 8\n");
			Data_count = 8;
		} else if (data_count_value > 8) {
			fprintf(stderr, "Warning: data_count exceeds maximum 8, clamping to 8\n");
			Data_count = 8;
		} else {
			Data_count = (uint8_t)data_count_value;
		}
		fprintf(stderr, "Number of channels: %u\n", Data_count);
	} else {
		Data_count = 8;
		fprintf(stderr, "Number of channels: 8 (default)\n");
	}

	if (serial_init(port, baudrate, byteSize, parity, stopBits) != 0) {
		fprintf(stderr, "Serial port initialization error %s\n", port);
		return 1;
	}

	fprintf(stderr, "Connected to %s, baud %d, data bits %d, parity %s, stop bits %s\n",
			port, baudrate, byteSize, argv[4], argv[5]);

	while (1) {
		ModbusRTU_Master_pool();
		if (Time_sleep != 0) {
			Sleep(Time_sleep);
		}
	}

	CloseHandle(hSerial);
	return 0;
}
