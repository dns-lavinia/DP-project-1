#ifndef __LOGGER_H
#define __LOGGER_H

#define LOG_SERVER 0
#define LOG_CLIENT 1
#define LOG_SEEDER 2
#define LOG_LEECHER 3
#define LOG_PROTOCOL 4

void logger(int source, const char *format, ...);

#endif