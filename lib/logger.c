#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#define RESET   "\033[1m\033[0m"
#define BLACK   "\033[1m\033[30m"
#define RED     "\033[1m\033[31m"     
#define GREEN   "\033[1m\033[32m"
#define YELLOW  "\033[1m\033[33m"
#define BLUE    "\033[1m\033[34m"
#define MAGENTA "\033[1m\033[35m"
#define CYAN    "\033[1m\033[36m"
#define WHITE   "\033[1m\033[37m"

char *log_source[] = {
    "SERVER",
    "CLIENT",
    "SEEDER",
    "LEECHER",
    "PROTOCOL"
};

char *log_color[] = {
    RED,
    GREEN,
    YELLOW,
    BLUE,
    BLACK
};

void timestamp() {
    time_t now;
    time(&now);

    struct tm *now_tm = localtime(&now);
    // int year = now_tm->tm_year + 1900;
    // int month = now_tm->tm_mon + 1;
    // int day = now_tm->tm_mday;
    int hour = now_tm->tm_hour;
    int min = now_tm->tm_min;
    int sec = now_tm->tm_sec;

    printf("[%02d:%02d:%02d] ", hour, min, sec);
}

void logger(int source, const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    timestamp();
    printf("%s%s%s: ", log_color[source], log_source[source], RESET);
    vprintf(format, args);
    printf("\n");

    va_end(args);
}