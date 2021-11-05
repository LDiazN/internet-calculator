/*
    This file contains the definition of a logging system,
    in order to get more meaningfull messages during the application
*/

#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Logger Logger;

/// Summary:
///     Use this function to get the current active function of the Logger. A Logger
///     is lazily initialized.
/// Return:
///     Pointer to the Logger object
Logger * logger_get();

/// Summary:
///     Destroy de logger object
void logger_destroy();

/// Summary:
///     Show a warning in yellow color
/// Parameters:
///     msg : message to be printed
void logger_warn(const char * msg);

/// Summary:
///     Show an info message in green color
/// Parameters:
///     msg : message to be printed
void logger_info(const char * msg);

/// Summary:
///     Show a critical error in red color
/// Parameters:
///     msg : message to be printed
void logger_error(const char * msg);

/// Summary:
///     Show a regular message with no color
/// Parameters:
///     msg : message to be printed
void logger_trace(const char * msg);

// -- < Logging Macros > ---------------------

#define LOGGING
#ifdef LOGGING

#define _LOG_MSG(f, ...) {\
        int size = snprintf(NULL, 0, __VA_ARGS__) + 1;\
        char * buff = malloc(sizeof(char)*size); \
        sprintf(buff,__VA_ARGS__); \
        f(buff);\
        if (1) \
        free(buff);\
    }

#define LOG_WARN(...)  _LOG_MSG(logger_warn, __VA_ARGS__)
#define LOG_INFO(...)  _LOG_MSG(logger_info, __VA_ARGS__)
#define LOG_ERROR(...) _LOG_MSG(logger_error, __VA_ARGS__)
#define LOG_TRACE(...) _LOG_MSG(logger_trace, __VA_ARGS__)

#else

#define LOG_WARN(...) 
#define LOG_INFO(...) 
#define LOG_ERROR(...)
#define LOG_TRACE(...)

#endif // Logging

#endif // LOGER_H

