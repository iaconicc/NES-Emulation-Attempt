#include "logger.h"
#include "app.h"
#include <Windows.h>
#include <stdio.h>
#include <time.h>

FILE* log_file = NULL;
FILE* console = NULL;
HANDLE hConsole;

#define C_BLACK   0
#define C_RED     FOREGROUND_RED
#define C_GREEN   FOREGROUND_GREEN
#define C_BLUE    FOREGROUND_BLUE
#define C_YELLOW  (FOREGROUND_RED | FOREGROUND_GREEN)
#define C_MAGENTA (FOREGROUND_RED | FOREGROUND_BLUE)
#define C_CYAN    (FOREGROUND_GREEN | FOREGROUND_BLUE)
#define C_WHITE   (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)

static BOOL WINAPI console_ctrl_handler(DWORD ctrl_type)
{
	switch (ctrl_type)
	{
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		set_quit();
		return TRUE;
	default:
		return FALSE;
	}
}

int log_initialise()
{
	//ensure that log folder exists
	if (_mkdir("log") != 0 && errno != EEXIST) return -1;

	//create new log file
	time_t t = time(NULL);
	struct tm currentDate;
	localtime_s(&currentDate,&t);
	char fileName[256];

	snprintf(fileName, sizeof(fileName),"log/%04d-%02d-%02d-%02d-%02d-log.txt",
		currentDate.tm_year+1900, currentDate.tm_mon+1, currentDate.tm_mday,
		currentDate.tm_hour, currentDate.tm_min);

	fopen_s(&log_file,fileName, "wb+");

	if (!log_file) return -1;

	AllocConsole();
	SetConsoleCtrlHandler(console_ctrl_handler ,TRUE);

	freopen_s(&console, "CONIN$", "r", stdin);
	freopen_s(&console, "CONOUT$", "w", stdout);
	freopen_s(&console, "CONOUT$", "w", stderr);

	// Optional: nicer title
	SetConsoleTitleA("NES Emulator Log");
	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	log_info("Logger initialised");

	return 0;
}

static void set_colour(WORD colour)
{
	SetConsoleTextAttribute(hConsole, colour);
}

void log_deinialise()
{
	if (log_file){
		fclose(log_file);
		log_file = NULL;
	}
	if (console){
		fclose(console);
		console = NULL;
	}
}

void log__(Log_level level, const char* fmt, ...)
{
	const char* level_text;
	WORD colour;
	switch (level) {
	case LOG_INFO:     level_text = "[INFO]";     colour = C_WHITE;  break;
	case LOG_DEBUG:    level_text = "[DEBUG]";    colour = C_GREEN;  break;
	case LOG_WARN:     level_text = "[WARN]";     colour = C_YELLOW; break;
	case LOG_CRITICAL: level_text = "[CRITICAL]"; colour = C_RED;    break;
	default:           level_text = "[INFO]";     colour = C_WHITE;  break;
	}

	va_list args;
	va_start(args, fmt);

	char buf[2048];
	vsnprintf(buf, sizeof(buf), fmt, args);
	char buf2[2048];
	snprintf(buf2, sizeof(buf2), "%s %s\n", level_text, buf);

	set_colour(colour);
	fputs(buf2, stdout);
	set_colour(C_WHITE);

	if (log_file)fputs(buf2, log_file);
	va_end(args);
}
