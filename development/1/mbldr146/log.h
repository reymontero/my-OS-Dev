// Project name:  Master Boot Loader (mbldr)
// File name:     log.h
// See also:      log.c
// Author:        Arnold Shade
// Creation date: 24 April 2006
// License type:  BSD
// URL:           http://mbldr.sourceforge.net/
// Description:   Header file for declaration of functions
// and constants related to log facility. It allows different
// levels of verbosity, file and stdout logging

#if !defined ( _LOG_H )
#define _LOG_H

// Available log levels (all subsequent log level includes
// logging from all previous levels)

// Show only fatal errors that lead to unexpected program termination
#define FATAL 0
// Show lots of debugging stuff
#define DEBUG 1

// Defines desired log level
void LogSetLevel
    (
    const unsigned char log_level
    );

// Generate logging message on a screen and/or in a log-file
// log_level - Log level of this message (message will not be shown if
//     it is set to the value greater that defined LogLevel filter)
// message_format - output message formatted according to printf-style
void Log
    (
    const unsigned char log_level,
    const char* message_format,
    ...
    );

#endif /* _LOG_H */

