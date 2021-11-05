#include "logger.h"
#include "assert.h"
#include "colors.h"
#include <time.h>

// Logger struct definition
typedef struct Logger {
    FILE * io_stream;
    FILE * error_io_stream;

} Logger;

// Logger instance definition
static Logger * _logger = NULL;

Logger * _new_logger(FILE * io_stream, FILE * error_io_stream)
{
    Logger * logger = malloc(sizeof(Logger));
    logger -> error_io_stream = error_io_stream;
    logger -> io_stream = io_stream;
    return logger;
}

// Get logget instance
Logger * logger_get()
{
    // initialized if not yet initialized
    if ( _logger == NULL)
        _logger = _new_logger(stdout, stderr);

    return _logger;
}

// Destroy logger instance
void logger_destroy()
{
    free(_logger);
    _logger = NULL;
}

// Forward declaration for utility functions
char * _logger_create_color_str(const char *msg, const char * ansi_color);
void _logger_log_err(const char * msg);
void _logger_log_msg(const char * msg);

// log a warn message
void logger_warn(const char * msg) 
{   
    char * new_msg = _logger_create_color_str(msg, YELLLOW);    
    _logger_log_err(new_msg);

    free(new_msg);
}

// Log an info message
void logger_info(const char * msg)
{
    char * new_msg = _logger_create_color_str(msg, GREEN);    
    _logger_log_msg(new_msg);

    free(new_msg);
}

// Log an error message
void logger_error(const char * msg) 
{ 
    char * new_msg = _logger_create_color_str(msg, RED);    
    
    _logger_log_err(new_msg);

    free(new_msg);
}

// Log a regular message message
void logger_trace(const char * msg)
{ 
    char * new_msg = _logger_create_color_str(msg, WHITE);    
    _logger_log_msg(new_msg);
    

    free(new_msg);
}

// -- < Utility functions > ------------------------------------------------------

// Create a new color string
char * _logger_create_color_str(const char *msg, const char * ansi_color)
{
    int new_size = strlen(msg) + 20;
    char * new_msg = malloc(sizeof(char) * new_size);

    // Get time to print in format
    time_t rawtime;
    struct tm info;
    char  time_str[100];

    time(&rawtime);

    localtime_r(&rawtime, &info);

    strftime(time_str, 100, "[%d-%m-%Y %H:%M:%S]", &info); 

    sprintf(new_msg, "%s%s %s%s", ansi_color, time_str, msg ,RESET);
    return new_msg;
}

// Log msg to error stream
void _logger_log_err(const char * msg)
{
    assert(_logger && "Logging not yet initialized");
    fprintf(_logger->io_stream,"%s", msg);
}

// Log msg to io stream
void _logger_log_msg(const char * msg)
{
    assert(_logger && "Logging not yet initialized");
    fprintf(_logger->io_stream,"%s", msg);
}

