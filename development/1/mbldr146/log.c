// Project name:  Master Boot Loader (mbldr)
// File name:     log.c
// See also:      log.h
// Author:        Arnold Shade
// Creation date: 24 April 2006
// License type:  BSD
// URL:           http://mbldr.sourceforge.net/
// Description:   Source file containing functionality of
// log facility. It allows different levels of verbosity,
// file and stdout logging

// Include standard system headers
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// Include local header
#include "log.h"
#include "common.h"

// Level of logging, by default it is set to FATAL
unsigned char LogLevel = FATAL;
// If set to 1, the next call of Log() will overwrite log file,
// then setting this variable to 0 (what means append to log file)
unsigned char LogFirstCall = 1;

// Displays error message on a screen. This is just a declaration,
// the implementation varies on mbldrcli or mbldrgui
void MbldrShowError
    (
    char *message_string
    );

// Defines desired log level
void LogSetLevel
    (
    const unsigned char log_level
    )
{
    LogLevel = log_level;
}

// Generate logging message on a screen and/or in a log-file
// log_level - Log level of this message (message will not be shown if
//     it is set to the value greater that defined LogLevel filter)
// message_format - output message formatted according to printf-style
void Log
    (
    const unsigned char log_level,
    const char* message_format,
    ...
    )
{
    va_list list;
    FILE* file;
    char message_string[ 1024 ];

    // Generate message string for logging

    va_start ( list,message_format );
    // Describe log level
    switch ( log_level )
    {
        case FATAL:
        {
            sprintf ( message_string,CommonMessage ( 0 ) );
            break;
        }

        case DEBUG:
        {
            sprintf ( message_string,CommonMessage ( 1 ) );
            break;
        }

        default:
        {
            sprintf ( message_string,CommonMessage ( 2 ) );
            break;
        }
    }
    // Append log text
    vsprintf ( message_string + strlen ( message_string ),message_format,list );
    va_end ( list );

    // Write string to log file in debug mode
    if ( ( log_level == DEBUG ) && ( LogLevel == DEBUG ) )
    {
        if ( LogFirstCall == 1 )
        {
            file = fopen ( "mbldr.log","w" );
            LogFirstCall = 0;
        }
        else
        {
            file = fopen ( "mbldr.log","a" );
        }
        if ( file != NULL )
        {
            fprintf ( file,"%s\n",message_string );
            if ( fflush ( file ) != 0 )
            {
                MbldrShowError ( CommonMessage ( 3 ) );
                exit ( 4 );
            }
            fclose ( file );
        }
    }

    if ( log_level == FATAL )
    {
        // Duplicate output to screen if program will terminate
        MbldrShowError ( message_string );
        exit ( 4 );
    }
}

