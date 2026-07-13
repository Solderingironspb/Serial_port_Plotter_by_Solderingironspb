#define _CRT_SECURE_NO_WARNINGS

#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif
// Подключаем библиотеку owen_io.lib
#ifdef _MSC_VER
#pragma comment(lib, "owen_io.lib")
#endif

#include <conio.h>
#include <locale.h>
#include <stdio.h>
#include <windows.h>
#include <time.h>
#include "owen_io.h"
#include <stdint.h>

uint16_t Delay_time = 0;



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


void printError()
{
	char errStr[LASTERRTOSTR_BUFFER_SIZE];

	LastErrToStr(errStr);
	fprintf(stderr, "Error: the device is unavailable\n");
	
	Sleep(5000);
}


void printFloat(const char* name, int error, float value)
{
	float null_value = 0;
	printf("%s", name);

	if (ERR_OK == error)
	{

		printf("1 = %f\n", value);
		printf("2 = %f\r\n", null_value);
		printf("3 = %f\r\n", null_value);
		printf("4 = %f\r\n", null_value);
		printf("5 = %f\r\n", null_value);
		printf("6 = %f\r\n", null_value);
		printf("7 = %f\r\n", null_value);
		printf("8 = %f\r\n", null_value);
		printf("9 = %lld\r\n", (long long)get_unix_timestamp_ms());
	}
	else
		printError();
		
}


void print_usage(const char* program_name) {
	fprintf(stderr, "USE: %s <COM Port> <Bits per second> <data bits> <parity> <stop bits> [sleep_ms] [data_count]\n", program_name);
	fprintf(stderr, "Example: %s 10 9600 8 NOPARITY ONESTOPBIT\n", program_name);
	fprintf(stderr, "Example with sleep: %s 10 9600 8 NOPARITY ONESTOPBIT 100\n", program_name);
	fprintf(stderr, "Example with all params: %s 10 9600 8 NOPARITY ONESTOPBIT 100 4\n", program_name);
	fprintf(stderr, "Parity: NOPARITY, ODDPARITY, EVENPARITY\n");
	fprintf(stderr, "Stop bits: ONESTOPBIT, TWOSTOPBITS\n");
	fprintf(stderr, "sleep_ms: optional delay between polls in milliseconds (default: 0)\n");
	fprintf(stderr, "data_count: optional number of channels (1-8, default: 8)\n");
}


int main(int argc, char* argv[]) {
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

	//char port[20];
	

	int port = atoi(argv[1]);

	int boudrate = spd_9600;

	
	if (strcmp(argv[2], "2400") == 0) {
		boudrate = spd_2400;
	}
	else if (strcmp(argv[2], "4800") == 0) {
		boudrate = spd_4800;
	}
	else if (strcmp(argv[2], "9600") == 0) {
		boudrate = spd_9600;
	}
	else if (strcmp(argv[2], "14400") == 0) {
		boudrate = spd_14400;
	}
	else if (strcmp(argv[2], "19200") == 0) {
		boudrate = spd_19200;
	}
	else if (strcmp(argv[2], "28800") == 0) {
		boudrate = spd_28800;
	}
	else if (strcmp(argv[2], "38800") == 0) {
		boudrate = spd_38800;
	}
	else if (strcmp(argv[2], "57600") == 0) {
		boudrate = spd_57600;
	}
	else if (strcmp(argv[2], "115200") == 0) {
		boudrate = spd_115200;
	}


	int data_bits = databits_8;
	if (strcmp(argv[3], "8") == 0) {
		data_bits = databits_8;
	}
	else if (strcmp(argv[3], "7") == 0) {
		data_bits = databits_7;
	}


	int partity = prty_NONE;
	if (strcmp(argv[4], "NOPARITY") == 0) {
		partity = prty_NONE;
	}else if (strcmp(argv[4], "EVENPARITY") == 0) {
		partity = prty_EVEN;
	}
	else if (strcmp(argv[4], "ODDPARITY") == 0) {
		partity = prty_ODD;
	}

	int stop_bits = stopbit_1;
	if (strcmp(argv[5], "ONESTOPBIT") == 0) {
		stop_bits = stopbit_1;
	}
	else if (strcmp(argv[5], "TWOSTOPBITS") == 0) {
		stop_bits = stopbit_2;
	}

	Delay_time = atoi(argv[6]);

	

		
	int res = OpenPort(port - 1, boudrate, partity, data_bits, stop_bits, RS485CONV_MANUAL);

	if (res != ERR_OK){
		printf("Error open COM Port\n\r");
		return 0;
	}
	else {
		fprintf(stderr, "Connected to COM%s, baud %s, data bits %s, parity %s, stop bits %s\n", argv[1], argv[2], argv[3], argv[4], argv[5]);
	}

	int address = 1;
	float value;

	while (1) {
		res = ReadFloat24(address, ADRTYPE_8BIT, (char*)"Pv", value, -1);
		printFloat("", res, value);
		Sleep(Delay_time);
	}

	ClosePort();
	
	return 0;
}