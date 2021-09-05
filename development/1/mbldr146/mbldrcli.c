// Project name:  Master Boot Loader (mbldr)
// File name:     mbldrcli.c
// See also:      mbldr.h, common.h
// Author:        Arnold Shade
// Creation date: 24 April 2006
// License type:  BSD
// URL:           http://mbldr.sourceforge.net/
// Description:   Main source file of a program
// that contains all function logic, menus, interation
// with user, normal and error messages, etc.
// mbldr.h file is automatically generated hexadecimal
// array of opcodes of the master boot loader which
// is written in assembly language

// Include standard system headers
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <locale.h>
#include <libintl.h>
#include <errno.h>
#if defined ( __DJGPP__ )
#include <dpmi.h>
#elif defined ( _WIN32 )
#include <windows.h>
#endif /* __DJGPP__ or _WIN32 */

// Include local headers
#include "common.h"
#include "log.h"
#include "disks.h"

// Displays error message on a screen. It is used by logging module
void MbldrShowError
    (
    char *message_string
    )
{
    printf ( "%s\n",message_string );
}

// Displays information message on a screen. It is used by common module
void MbldrShowInfoMessage
    (
    const char *message_format,
    ...
    )
{
    va_list list;
    char message_string[ 4096 ];

    // Generate message string
    va_start ( list, message_format );
    vsprintf ( message_string, message_format, list );
    va_end ( list );

    // Output message
    printf ( "\n%s\n", message_string );
    printf ( CommonMessage ( 169 ) );

    // Duplicate string output into log
    Log ( DEBUG,"%s",message_string );

    // Wait for user's input (just pressing of Enter key as a
    // confirmation of this informational message
    getchar ();
}

// Asks a user with yes/no possible answers. If user disagrees (answer is no)
// then return value is 0, otherwise (if user says yes) the return value is 1.
unsigned char MbldrYesNo
    (
    char *message_string
    )
{
    // Text buffer representing user's input
    char Buffer[ 128 ];

    // Output a menu asking for confirmation
    printf ( "\n%s\n", message_string );
    printf ( "y. " );
    printf ( CommonMessage ( 170 ) );
    printf ( "\n" );
    printf ( "n. " );
    printf ( CommonMessage ( 171 ) );
    printf ( "\n" );

    // Wait for user's input
    do
    {
        printf ( CommonMessage ( 172 ) );
        printf ( " (y,n): " );
        strcpy ( Buffer,"" );
        scanf ( "%127[^\n]",Buffer );
        getchar ();
        Log ( DEBUG,CommonMessage ( 173 ),Buffer );
    } while (
            ( strcasecmp ( Buffer,"y" ) != 0 ) &&
            ( strcasecmp ( Buffer,"n" ) != 0 )
            );
    if ( strcasecmp ( Buffer,"y" ) == 0 )
    {
        Log ( DEBUG,CommonMessage ( 170 ) );
        return ( 1 );
    }
    Log ( DEBUG,CommonMessage ( 171 ) );
    return ( 0 );
}

// Warns a user and suggests him two choices of possible answer (return values
// are 0 and 1 respectively). User is also allowed to cancel the choice (-1 is
// returned)
signed char MbldrChooseOneOfTwo
    (
    char *warning_string,
    char *choice1_string,
    char *choice2_string
    )
{
    // Text buffer representing user's input
    char Buffer[ 128 ];

    // Output a menu asking for coice
    printf ( "\n%s\n", warning_string );
    printf ( "1. " );
    printf ( choice1_string );
    printf ( "\n" );
    printf ( "2. " );
    printf ( choice2_string );
    printf ( "\n" );
    printf ( "3. " );
    printf ( CommonMessage ( 200 ) );
    printf ( "\n" );

    // Wait for user's input
    do
    {
        printf ( CommonMessage ( 172 ) );
        printf ( " (1,2,3): " );
        strcpy ( Buffer,"" );
        scanf ( "%127[^\n]",Buffer );
        getchar ();
        Log ( DEBUG,CommonMessage ( 173 ),Buffer );
    } while (
            ( strcasecmp ( Buffer,"1" ) != 0 ) &&
            ( strcasecmp ( Buffer,"2" ) != 0 ) &&
            ( strcasecmp ( Buffer,"3" ) != 0 )
            );
    if ( strcasecmp ( Buffer,"1" ) == 0 )
    {
        // First choice
        return ( 0 );
    }
    if ( strcasecmp ( Buffer,"2" ) == 0 )
    {
        // Second choice
        return ( 1 );
    }
    // Cancel the dialog
    return ( -1 );
}

// Displays about message on a screen. It is used by common module
void MbldrAbout
    (
    char* sName,
    char* sVersion,
    char* sDeveloper,
    char* sDescription,
    char* sSyntaxPrefix,
    char* sSyntaxPostfix,
    char* sOptions,
    char* sDeviceOption,
    char* sWebSite
    )
{
    // Print the caption, description and part of syntax
    printf ( "%s v%s\n", sName, sVersion );
    printf ( "%s %s\n\n%s%s", CommonMessage ( 174 ), sDeveloper, sDescription, sSyntaxPrefix );
    // Print the name of executable file
#if defined ( _WIN32 )
    printf ( "mbldrcli.exe" );
#elif defined ( __unix__ )
    printf ( "mbldrcli" );
#elif defined ( __DJGPP__ )
    printf ( "mbldr.exe" );
#endif /* _WIN32 or __unix__ or __DJGPP__ */
    // Print the rest of syntax and link to web-site
    printf ( "%s%s%s\n\n%s %s\n", sSyntaxPostfix, sOptions, sDeviceOption, CommonMessage ( 175 ), sWebSite );
}

// Performs devices autodetection and lets user choose the one
void MbldrChooseDevice
    (
    void
    )
{
    // Dynamic array representing a list of disk devices
    DisksInfo_t *pDisksArray = NULL;
    // Pointer for iterating devices list
    DisksInfo_t *pDisksArrayIterator = NULL;
    // Counter of found disk devices
    unsigned char DevicesCount;
    // String buffer representing user's input
    char Buffer[ 128 ];
    // User's choice
    int UserChoice = 0;

    // Create a list describing found disk devices
    DisksCollectInfo ( &pDisksArray );

    // Check for at least one device in a list
    if ( pDisksArray == NULL )
    {
        Log ( FATAL,CommonMessage ( 176 ) );
    }

    // Check for exactly one device in a list
    if ( pDisksArray->pNext == NULL )
    {
        printf ( CommonMessage ( 177 ) );
        if ( strcmp ( pDisksArray->sDiskDescription,"" ) == 0 )
        {
            printf ( "%s",pDisksArray->sDiskName );
        }
        else
        {
            printf ( "%s",pDisksArray->sDiskDescription );
#if defined ( __unix__ )
            printf ( " (%s)",pDisksArray->sDiskName );
#endif /* __unix__ */
        }
        printf ( "\n" );

        // Copy device name
        strcpy ( Device,pDisksArray->sDiskName );

        // Release alocated list of devices
        DisksFreeInfo ( pDisksArray );

        // There is no need to show a menu
        return;
    }

    // Iterate through the list of devices to output a menu for user
    printf ( "\n" );
    pDisksArrayIterator = pDisksArray;
    DevicesCount = 0;
    while ( pDisksArrayIterator != NULL )
    {
        // Increase the number of disks
        DevicesCount++;
        // Output menu entry
        if ( strcmp ( pDisksArrayIterator->sDiskDescription,"" ) == 0 )
        {
            printf ( "%i. %s\n",DevicesCount,pDisksArrayIterator->sDiskName );
        }
        else
        {
#if defined ( __unix__ )
            printf ( "%i. %s (%s)\n",DevicesCount,pDisksArrayIterator->sDiskDescription,pDisksArrayIterator->sDiskName );
#else
            printf ( "%i. %s\n",DevicesCount,pDisksArrayIterator->sDiskDescription );
#endif /* __unix__ */
        }
        // Switch to next element in the dynamic array
        pDisksArrayIterator = pDisksArrayIterator->pNext;
    }
    printf ( "q. " );
    printf ( CommonMessage ( 179 ) );
    printf ( "\n" );
    do
    {
        printf ( CommonMessage ( 180 ) );
        printf ( " (1-%u,q): ",DevicesCount );
        strcpy ( Buffer,"" );
        scanf ( "%127[^\n]",Buffer );
        getchar ();
        if ( strcasecmp ( Buffer,"q" ) == 0 )
        {
            Log ( DEBUG,CommonMessage ( 181 ) );
            exit ( 5 );
        }
        UserChoice = atoi ( Buffer );
        Log ( DEBUG,CommonMessage ( 182 ),Buffer,UserChoice );
    } while (( UserChoice <= 0 ) || ( UserChoice > DevicesCount ));

    // Copy device name
    pDisksArrayIterator = pDisksArray;
    DevicesCount = 0;
    while ( DevicesCount < UserChoice - 1 )
    {
        pDisksArrayIterator = pDisksArrayIterator->pNext;
        if ( pDisksArrayIterator == NULL )
        {
            Log ( FATAL,CommonMessage ( 29 ) );
        }
        DevicesCount++;
    }

    // Copy device name
    strcpy ( Device,pDisksArrayIterator->sDiskName );

    // Release alocated list of devices
    DisksFreeInfo ( pDisksArray );

    return;
}

// Add one partition of user's choice to current configuration of mbldr
// Also allows adding special boot menu items 'skip boot' and 'next hdd'
// It is also used to output a list of available partitions (found on a chosen HDD)
void MbldrAddPartitionToCurrentConfiguration
    (
    void
    )
{
    // Counter used to iterate through the list of found (available) partitions
    int Count;
    // Text buffer used to temporary store name and parameters describing
    // found (available) partitions. This buffer also represents user's input
    char Buffer[ 1024 ];
    // Number of item that user chooses from menu
    int UserChoice = 0;

    if ( NumberOfBootablePartitions == 9 )
    {
        MbldrShowInfoMessage ( CommonMessage ( 183 ) );
    }
    else
    {
        printf ( "\n" );
        printf ( CommonMessage ( 184 ) );
        printf ( "\n" );
        for ( Count=0 ; Count<NumberOfFoundPartitions ; Count++ )
        {
            CommonGetDescriptionOfAvailablePartition ( Count,Buffer );
            printf ( "%u.%s\n",Count + 1,Buffer );
        }

        // Add 'skip boot attempt' menu item
        CommonGetDescriptionOfAvailablePartition ( -1,Buffer );
        printf ( "a.%s\n",Buffer );

        // Add 'boot from next hard disk' menu item
        CommonGetDescriptionOfAvailablePartition ( -2,Buffer );
        printf ( "b.%s\n",Buffer );

        // Do not add partition
        printf ( "c. " );
        printf ( CommonMessage ( 198 ) );
        printf ( "\n" );

        // Quit is a last choice
        printf ( "q. " );
        printf ( CommonMessage ( 179 ) );
        printf ( "\n" );

        // Wait for user's input
        do
        {
            printf ( CommonMessage ( 199 ) );
            printf ( " (" );
            if ( NumberOfFoundPartitions != 0 )
            {
                printf ( "1" );
                if ( NumberOfFoundPartitions > 1 )
                {
                    printf ( "-%u",NumberOfFoundPartitions );
                }
                printf ( "," );
            }
            printf ( "a,b,c,q): " );
            strcpy ( Buffer,"" );
            scanf ( "%127[^\n]",Buffer );
            getchar ();
            if ( strcasecmp ( Buffer,"q" ) == 0 )
            {
                Log ( DEBUG,CommonMessage ( 181 ) );
                exit ( 5 );
            }
            if ( strcasecmp ( Buffer,"a" ) == 0 )
            {
                break;
            }
            if ( strcasecmp ( Buffer,"b" ) == 0 )
            {
                break;
            }
            if ( strcasecmp ( Buffer,"c" ) == 0 )
            {
                Log ( DEBUG,CommonMessage ( 200 ) );
                return;
            }
            // At this point it is safe to do a string to int conversion
            UserChoice = atoi ( Buffer );
            Log ( DEBUG,CommonMessage ( 182 ),Buffer,UserChoice );
        } while (( UserChoice <= 0 ) || ( UserChoice > NumberOfFoundPartitions ));
        if ( strcasecmp ( Buffer,"a" ) == 0 )
        {
            BootablePartitions[ NumberOfBootablePartitions ].relative_sectors_offset = 0xFFFFFFFFul;
            strcpy ( BootablePartitions[ NumberOfBootablePartitions ].label,"" );
            NumberOfBootablePartitions++;
            CommonConstructBootMenuText ( 0 );
            MbldrShowInfoMessage ( CommonMessage ( 201 ) );
        }
        else if ( strcasecmp ( Buffer,"b" ) == 0 )
        {
            printf ( "\n" );
            // Since 'Next HDD' is quite risky, ask for confirmation
            if ( MbldrYesNo ( CommonMessage ( 202 ) ) != 0 )
            {
                BootablePartitions[ NumberOfBootablePartitions ].relative_sectors_offset = 0;
                strcpy ( BootablePartitions[ NumberOfBootablePartitions ].label,"" );
                NumberOfBootablePartitions++;
                CommonConstructBootMenuText ( 0 );
                MbldrShowInfoMessage ( CommonMessage ( 204 ) );
            }
        }
        else
        {
            BootablePartitions[ NumberOfBootablePartitions ].relative_sectors_offset = FoundPartitions[ UserChoice - 1 ].relative_sectors_offset;
            printf ( CommonMessage ( 205 ) );
            strcpy ( BootablePartitions[ NumberOfBootablePartitions ].label,"" );
            scanf ( "%255[^\n]",BootablePartitions[ NumberOfBootablePartitions ].label );
            getchar ();
            Log ( DEBUG,CommonMessage ( 207 ),BootablePartitions[ NumberOfBootablePartitions ].label );
            printf ( CommonMessage ( 207 ),BootablePartitions[ NumberOfBootablePartitions ].label );
            printf ( "\n" );
            // Check how many symbols are available while constructing boot menu text
            NumberOfBootablePartitions++;
            CommonConstructBootMenuText ( 0 );
            MbldrShowInfoMessage ( CommonMessage ( 208 ) );
        }
    }
}

// Manages bootable partitions in current mbldr configuration. It allows to remove,
// set default, set label and view list of partitions
void MbldrManageBootablePartitions
    (
    void
    )
{
    // Counter used to iterate through the list of bootable (configured) partitions
    int Count;
    // Text buffer used to temporary store name and parameters describing
    // bootable (configured) partitions. This buffer also represents user's input
    char Buffer[ 1024 ];
    // Number of item that user chooses from menu
    int UserChoice = 0;

    if ( NumberOfBootablePartitions == 0 )
    {
        MbldrShowInfoMessage ( CommonMessage ( 209 ) );
    }
    else
    {
        printf ( "\n" );
        if ( NumberOfDefaultPartition == 255 )
        {
            printf ( CommonMessage ( 210 ) );
            printf ( " " );
        }
        else
        {
            printf ( CommonMessage ( 211 ) );
            printf ( " " );
        }
        if ( HideOtherPrimaryPartitions == 1 )
        {
            printf ( CommonMessage ( 376 ) );
            printf ( "\n" );
        }
        else if ( HideOtherPrimaryPartitions == 0 )
        {
            printf ( CommonMessage ( 377 ) );
            printf ( "\n" );
        }
        for ( Count=0 ; Count<NumberOfBootablePartitions ; Count++ )
        {
            CommonGetDescriptionOfBootablePartition ( Count,Buffer );
            printf ( "%u.%s\n",Count + 1,Buffer );
        }

        // Reset default partition. The operating system which was last booted
        // is marked as a default one for the next boot
        printf ( "a. " );
        printf ( CommonMessage ( 218 ) );
        printf ( "\n" );

        // Switch between "to hide other primary partitions at boot attempts" or "not to hide"
        printf ( "b. " );
        if ( HideOtherPrimaryPartitions == 1 )
        {
            printf ( CommonMessage ( 377 ) );
        }
        else if ( HideOtherPrimaryPartitions == 0 )
        {
            printf ( CommonMessage ( 376 ) );
        }
        printf ( "\n" );

        // Switch between "to mark primary partitions active/inactive at boot attempts" or "not to mark"
        printf ( "c. " );
        if ( MarkActivePartition == 1 )
        {
            printf ( CommonMessage ( 388 ) );
        }
        else if ( MarkActivePartition == 0 )
        {
            printf ( CommonMessage ( 387 ) );
        }
        printf ( "\n" );

        // Do nothing, go back to main menu
        printf ( "d. " );
        printf ( CommonMessage ( 200 ) );
        printf ( "\n" );

        // Quit is a last choice
        printf ( "q. " );
        printf ( CommonMessage ( 179 ) );
        printf ( "\n" );

        // Wait for user's input
        do
        {
            printf ( CommonMessage ( 219 ) );
            printf ( " (1" );
            if ( NumberOfBootablePartitions > 1 )
            {
                printf ( "-%u",NumberOfBootablePartitions );
            }
            printf ( ",a,b,c,d,q): " );
            strcpy ( Buffer,"" );
            scanf ( "%127[^\n]",Buffer );
            getchar ();
            if ( strcasecmp ( Buffer,"q" ) == 0 )
            {
                Log ( DEBUG,CommonMessage ( 181 ) );
                exit ( 5 );
            }
            if ( strcasecmp ( Buffer,"a" ) == 0 )
            {
                NumberOfDefaultPartition = 255;
                CommonConstructBootMenuText ( 0 );
                MbldrShowInfoMessage ( CommonMessage ( 220 ) );
                return;
            }
            if ( strcasecmp ( Buffer,"b" ) == 0 )
            {
                if ( HideOtherPrimaryPartitions == 1 )
                {
                    HideOtherPrimaryPartitions = 0;
                    MbldrShowInfoMessage ( CommonMessage ( 379 ) );
                }
                else if ( HideOtherPrimaryPartitions == 0 )
                {
                    HideOtherPrimaryPartitions = 1;
                    MbldrShowInfoMessage ( CommonMessage ( 378 ) );
                }
                return;
            }
            if ( strcasecmp ( Buffer,"c" ) == 0 )
            {
                if ( MarkActivePartition == 1 )
                {
                    MarkActivePartition = 0;
                    MbldrShowInfoMessage ( CommonMessage ( 383 ) );
                }
                else if ( MarkActivePartition == 0 )
                {
                    MarkActivePartition = 1;
                    MbldrShowInfoMessage ( CommonMessage ( 384 ) );
                }
                return;
            }
            if ( strcasecmp ( Buffer,"d" ) == 0 )
            {
                Log ( DEBUG,CommonMessage ( 200 ) );
                return;
            }
            // At this point it is safe to do a string to int conversion
            UserChoice = atoi ( Buffer );
            Log ( DEBUG,CommonMessage ( 182 ),Buffer,UserChoice );
        } while (( UserChoice <= 0 ) || ( UserChoice > NumberOfBootablePartitions ));

        // Ask user what does he want to do with the chosen partition
        printf ( "\n" );
        printf ( "a. " );
        printf ( CommonMessage ( 221 ) );
        printf ( "\n" );
        printf ( "b. " );
        printf ( CommonMessage ( 222 ) );
        printf ( "\n" );
        printf ( "c. " );
        printf ( CommonMessage ( 223 ) );
        printf ( "\n" );
        printf ( "d. " );
        printf ( CommonMessage ( 393 ) );
        printf ( "\n" );
        printf ( "e. " );
        printf ( CommonMessage ( 200 ) );
        printf ( "\n" );
        printf ( "q. " );
        printf ( CommonMessage ( 179 ) );
        printf ( "\n" );

        // Wait for user's input
        do
        {
            printf ( CommonMessage ( 224 ) );
            printf ( " (a,b,c,d,e,q): " );
            strcpy ( Buffer,"" );
            scanf ( "%127[^\n]",Buffer );
            getchar ();
            Log ( DEBUG,CommonMessage ( 173 ),Buffer );
        } while (
                ( strcasecmp ( Buffer,"a" ) != 0 ) &&
                ( strcasecmp ( Buffer,"b" ) != 0 ) &&
                ( strcasecmp ( Buffer,"c" ) != 0 ) &&
                ( strcasecmp ( Buffer,"d" ) != 0 ) &&
                ( strcasecmp ( Buffer,"e" ) != 0 ) &&
                ( strcasecmp ( Buffer,"q" ) != 0 )
                );
        if ( strcasecmp ( Buffer,"q" ) == 0 )
        {
            Log ( DEBUG,CommonMessage ( 181 ) );
            exit ( 5 );
        }
        else if ( strcasecmp ( Buffer,"a" ) == 0 )
        {
            Log ( DEBUG,CommonMessage ( 226 ) );
            // Do a renumbering/compression of a list
            for ( Count=UserChoice-1 ; Count<NumberOfBootablePartitions-1 ; Count++ )
            {
                BootablePartitions[ Count ].relative_sectors_offset = BootablePartitions[ Count + 1 ].relative_sectors_offset;
                strcpy ( BootablePartitions[ Count ].label,BootablePartitions[ Count + 1 ].label );
            }

            // Decrease number of bootable partitions
            NumberOfBootablePartitions--;
            if (
                ( NumberOfDefaultPartition >= NumberOfBootablePartitions ) &&
                ( NumberOfDefaultPartition != 0 ) &&
                ( NumberOfDefaultPartition != 255 )
               )
            {
                // Number of default partition may become out of bounds
                NumberOfDefaultPartition--;
                Log ( DEBUG,CommonMessage ( 227 ),NumberOfDefaultPartition );
            }
            CommonConstructBootMenuText ( 0 );
            if ( NumberOfBootablePartitions == 0 )
            {
                MbldrShowInfoMessage ( "%s %s", CommonMessage ( 228 ), CommonMessage ( 229 ) );
            }
            else
            {
                MbldrShowInfoMessage ( CommonMessage ( 228 ) );
            }
        }
        else if ( strcasecmp ( Buffer,"b" ) == 0 )
        {
            Log ( DEBUG,CommonMessage ( 230 ) );
            printf ( CommonMessage ( 205 ) );
            strcpy ( BootablePartitions[ UserChoice - 1 ].label,"" );
            scanf ( "%255[^\n]",BootablePartitions[ UserChoice - 1 ].label );
            getchar ();
            // Check how many symbols are available while constructing boot menu text
            CommonConstructBootMenuText ( 0 );
            MbldrShowInfoMessage ( CommonMessage ( 207 ),BootablePartitions[ UserChoice - 1 ].label );
        }
        else if ( strcasecmp ( Buffer,"c" ) == 0 )
        {
            Log ( DEBUG,CommonMessage ( 233 ) );
            NumberOfDefaultPartition = UserChoice - 1;
            CommonConstructBootMenuText ( 0 );
            MbldrShowInfoMessage ( CommonMessage ( 234 ),NumberOfDefaultPartition );
        }
        else if ( strcasecmp ( Buffer,"d" ) == 0 )
        {
            // Offset of a bootable partition (in sectors)
            unsigned long ulOffset;
            // Pointer to the end of decoded number in the string entered by user
            char *pEndPointer;

            Log ( DEBUG,CommonMessage ( 394 ) );
            printf ( CommonMessage ( 389 ) );
            printf ( " (%lu) ",BootablePartitions[ UserChoice - 1 ].relative_sectors_offset );
            strcpy ( Buffer,"" );
            scanf ( "%127[^\n]",Buffer );
            getchar ();
            Log ( DEBUG,CommonMessage ( 173 ),Buffer );
            errno = 0;
            ulOffset = strtoul ( Buffer,&pEndPointer,10 );
            // Check that no error has occured during conversion, string is not empty and
            // whole string has been converted to a number
            if (( errno == 0 ) && ( strcmp ( Buffer,"" ) != 0 ) && ( *pEndPointer == 0 ))
            {
                // Correct input
                BootablePartitions[ UserChoice - 1 ].relative_sectors_offset = ulOffset;
                MbldrShowInfoMessage ( CommonMessage ( 390 ) );
            }
        }
        else if ( strcasecmp ( Buffer,"e" ) == 0 )
        {
            Log ( DEBUG,CommonMessage ( 200 ) );
            return;
        }
    }
}

// Configuration of timer interrupt key: either Esc or Space
void MbldrManageTimerInterruptKey
    (
    void
    )
{
    // Text buffer representing user's input
    char Buffer[ 128 ];

    // Output a menu asking for timer interrupt key
    printf ( "\n" );
    printf ( "a. " );
    printf ( CommonMessage ( 235 ) );
    printf ( "\n" );
    printf ( "b. " );
    printf ( CommonMessage ( 236 ) );
    printf ( "\n" );
    printf ( "c. " );
    printf ( CommonMessage ( 200 ) );
    printf ( "\n" );
    printf ( "q. " );
    printf ( CommonMessage ( 179 ) );
    printf ( "\n" );

    // Wait for user's input
    do
    {
        printf ( CommonMessage ( 237 ) );
        printf ( " (a,b,c,q): " );
        strcpy ( Buffer,"" );
        scanf ( "%127[^\n]",Buffer );
        getchar ();
        Log ( DEBUG,CommonMessage ( 173 ),Buffer );
        if ( strcasecmp ( Buffer,"q" ) == 0 )
        {
            Log ( DEBUG,CommonMessage ( 181 ) );
            exit ( 5 );
        }
    } while (
             ( strcasecmp ( Buffer,"a" ) != 0 ) &&
             ( strcasecmp ( Buffer,"b" ) != 0 ) &&
             ( strcasecmp ( Buffer,"c" ) != 0 )
            );

    if ( strcasecmp ( Buffer,"a" ) == 0 )
    {
        Log ( DEBUG,CommonMessage ( 235 ) );
        TimerInterruptKey = 0x1B;
    }
    else if ( strcasecmp ( Buffer,"b" ) == 0 )
    {
        Log ( DEBUG,CommonMessage ( 236 ) );
        TimerInterruptKey = 0x20;
    }
    else if ( strcasecmp ( Buffer,"c" ) == 0 )
    {
        Log ( DEBUG,CommonMessage ( 200 ) );
        return;
    }

    CommonConstructBootMenuText ( 0 );
}

// Progress-bar configuration: presence and type
void MbldrConfigureProgressBarParameters
    (
    void
    )
{
    // Cycle iterator
    int Count;
    // Text buffer representing user's input
    char Buffer[ 128 ];

    // Output current configuration of progress-bar
    printf ( "\n" );
    if ( ProgressBarAllowed == 0 )
    {
        printf ( CommonMessage ( 238 ) );
        printf ( "\n" );
    }
    else
    {
        printf ( CommonMessage ( 239 ) );
        if ( ProgressBarSymbol == 0 )
        {
            printf ( "[9876543210]\n" );
        }
        else
        {
            printf ( "[" );
            for ( Count=0 ; Count<10 ; Count++ )
            {
                printf ( "%c",ProgressBarSymbol );
            }
            printf ( "]\n" );
        }
    }

    // Output a menu asking for timer interrupt key
    printf ( "1. [9876543210] " );
    printf ( CommonMessage ( 240 ) );
    printf ( "\n" );
    printf ( "2. [**********] " );
    printf ( CommonMessage ( 241 ) );
    printf ( "\n" );
    printf ( "3. [==========] " );
    printf ( CommonMessage ( 242 ) );
    printf ( "\n" );
    printf ( "4. [>>>>>>>>>>] " );
    printf ( CommonMessage ( 243 ) );
    printf ( "\n" );
    printf ( "5. [..........] " );
    printf ( CommonMessage ( 244 ) );
    printf ( "\n" );
    printf ( "6. [##########] " );
    printf ( CommonMessage ( 245 ) );
    printf ( "\n" );
    printf ( "7. [++++++++++] " );
    printf ( CommonMessage ( 246 ) );
    printf ( "\n" );
    printf ( "8. [----------] " );
    printf ( CommonMessage ( 247 ) );
    printf ( "\n" );
    printf ( "a. " );
    printf ( CommonMessage ( 248 ) );
    printf ( "\n" );
    printf ( "b. " );
    printf ( CommonMessage ( 249 ) );
    printf ( "\n" );
    printf ( "c. " );
    printf ( CommonMessage ( 200 ) );
    printf ( "\n" );
    printf ( "q. " );
    printf ( CommonMessage ( 179 ) );
    printf ( "\n" );

    // Wait for user's input
    do
    {
        printf ( CommonMessage ( 250 ) );
        printf ( " (1-8,a,b,c,q): " );
        strcpy ( Buffer,"" );
        scanf ( "%127[^\n]",Buffer );
        getchar ();
        Log ( DEBUG,CommonMessage ( 173 ),Buffer );
        if ( strcasecmp ( Buffer,"q" ) == 0 )
        {
            Log ( DEBUG,CommonMessage ( 181 ) );
            exit ( 5 );
        }
    } while (
             ( strcasecmp ( Buffer,"1" ) != 0 ) &&
             ( strcasecmp ( Buffer,"2" ) != 0 ) &&
             ( strcasecmp ( Buffer,"3" ) != 0 ) &&
             ( strcasecmp ( Buffer,"4" ) != 0 ) &&
             ( strcasecmp ( Buffer,"5" ) != 0 ) &&
             ( strcasecmp ( Buffer,"6" ) != 0 ) &&
             ( strcasecmp ( Buffer,"7" ) != 0 ) &&
             ( strcasecmp ( Buffer,"8" ) != 0 ) &&
             ( strcasecmp ( Buffer,"a" ) != 0 ) &&
             ( strcasecmp ( Buffer,"b" ) != 0 ) &&
             ( strcasecmp ( Buffer,"c" ) != 0 )
            );

    if ( strcasecmp ( Buffer,"1" ) == 0 )
    {
        Log ( DEBUG,"%s%s",CommonMessage ( 239 ),CommonMessage ( 240 ) );
        ProgressBarAllowed = 1;
        ProgressBarSymbol = 0;
    }
    else if ( strcasecmp ( Buffer,"2" ) == 0 )
    {
        Log ( DEBUG,"%s%s",CommonMessage ( 239 ),CommonMessage ( 241 ) );
        ProgressBarAllowed = 1;
        ProgressBarSymbol = '*';
    }
    else if ( strcasecmp ( Buffer,"3" ) == 0 )
    {
        Log ( DEBUG,"%s%s",CommonMessage ( 239 ),CommonMessage ( 242 ) );
        ProgressBarAllowed = 1;
        ProgressBarSymbol = '=';
    }
    else if ( strcasecmp ( Buffer,"4" ) == 0 )
    {
        Log ( DEBUG,"%s%s",CommonMessage ( 239 ),CommonMessage ( 243 ) );
        ProgressBarAllowed = 1;
        ProgressBarSymbol = '>';
    }
    else if ( strcasecmp ( Buffer,"5" ) == 0 )
    {
        Log ( DEBUG,"%s%s",CommonMessage ( 239 ),CommonMessage ( 244 ) );
        ProgressBarAllowed = 1;
        ProgressBarSymbol = '.';
    }
    else if ( strcasecmp ( Buffer,"6" ) == 0 )
    {
        Log ( DEBUG,"%s%s",CommonMessage ( 239 ),CommonMessage ( 245 ) );
        ProgressBarAllowed = 1;
        ProgressBarSymbol = '#';
    }
    else if ( strcasecmp ( Buffer,"7" ) == 0 )
    {
        Log ( DEBUG,"%s%s",CommonMessage ( 239 ),CommonMessage ( 246 ) );
        ProgressBarAllowed = 1;
        ProgressBarSymbol = '+';
    }
    else if ( strcasecmp ( Buffer,"8" ) == 0 )
    {
        Log ( DEBUG,"%s%s",CommonMessage ( 239 ),CommonMessage ( 247 ) );
        ProgressBarAllowed = 1;
        ProgressBarSymbol = '-';
    }
    else if ( strcasecmp ( Buffer,"a" ) == 0 )
    {
        Log ( DEBUG,CommonMessage ( 248 ) );
        ProgressBarAllowed = 1;
        printf ( "%s: ",CommonMessage ( 251 ) );
        scanf ( "%c",&ProgressBarSymbol );
        getchar ();
    }
    else if ( strcasecmp ( Buffer,"b" ) == 0 )
    {
        Log ( DEBUG,CommonMessage ( 249 ) );
        ProgressBarAllowed = 0;
    }
    else if ( strcasecmp ( Buffer,"c" ) == 0 )
    {
        Log ( DEBUG,CommonMessage ( 200 ) );
        return;
    }

    CommonConstructBootMenuText ( 0 );
}

// Timed boot configuration (timeout value, timer interrupt key, progress-bar)
void MbldrConfigureTimedBoot
    (
    void
    )
{
    // Boolean value indicating whether user's input is correct
    unsigned char IsInputCorrect = 0;
    // Text buffer representing user's input
    char Buffer[ 128 ];
    // Number of item that user chooses from menu
    long UserChoice = 0;

    // Output current configuration of timed boot
    printf ( "\n" );
    if (( TimedBootAllowed == 0 ) && ( Timeout != 0 ))
    {
        printf ( CommonMessage ( 252 ) );
        printf ( "\n" );
    }
    else
    {
        printf ( CommonMessage ( 253 ) );
        if ( TimedBootAllowed == 1 )
        {
            // User timer
            printf ( CommonMessage ( 333 ) );
        }
        else if ( TimedBootAllowed == 2 )
        {
            // System timer
            printf ( CommonMessage ( 369 ) );
        }
        printf ( " " );
        printf ( CommonMessage ( 254 ) );
        printf ( " " );
        if ( TimerInterruptKey == 0x1B )
        {
            printf ( CommonMessage ( 255 ) );
        }
        else if ( TimerInterruptKey == 0x20 )
        {
            printf ( CommonMessage ( 256 ) );
        }
        printf ( ". " );
        printf ( CommonMessage ( 257 ),Timeout/18 );
        printf ( "\n" );
        if (( NumberOfBootablePartitions != 0 ) && ( NumberOfDefaultPartition < NumberOfBootablePartitions ))
        {
            printf ( CommonMessage ( 258 ),NumberOfDefaultPartition + 1,BootablePartitions[ NumberOfDefaultPartition ].label );
            printf ( "\n" );
        }
    }

    // Output a menu asking for timeout
    printf ( "#. " );
    printf ( CommonMessage ( 259 ) );
    printf ( "\n" );
    printf ( "a. " );
    printf ( CommonMessage ( 370 ) );
    printf ( "\n" );
    printf ( "b. " );
    printf ( CommonMessage ( 260 ) );
    printf ( "\n" );
    printf ( "c. " );
    printf ( CommonMessage ( 261 ) );
    printf ( "\n" );
    printf ( "d. " );
    printf ( CommonMessage ( 262 ) );
    printf ( "\n" );
    printf ( "e. " );
    printf ( CommonMessage ( 263 ) );
    printf ( "\n" );
    printf ( "f. " );
    printf ( CommonMessage ( 200 ) );
    printf ( "\n" );
    printf ( "q. " );
    printf ( CommonMessage ( 179 ) );
    printf ( "\n" );

    // Wait for user's input
    do
    {
        printf ( CommonMessage ( 264 ) );
        printf ( " (1-36000,a,b,c,d,e,f,q): " );
        strcpy ( Buffer,"" );
        scanf ( "%127[^\n]",Buffer );
        getchar ();
        if ( strcasecmp ( Buffer,"q" ) == 0 )
        {
            Log ( DEBUG,CommonMessage ( 181 ) );
            exit ( 5 );
        }
        else if ( strcasecmp ( Buffer,"a" ) == 0 )
        {
            if ( TimedBootAllowed == 1 )
            {
                // If user timer is set, then switch to system timer
                TimedBootAllowed = 2;
                Log ( DEBUG,CommonMessage ( 371 ) );
            }
            else if ( TimedBootAllowed == 2 )
            {
                // If system timer is set, then switch to user timer
                TimedBootAllowed = 1;
                Log ( DEBUG,CommonMessage ( 372 ) );
            }
            IsInputCorrect = 1;
        }
        else if ( strcasecmp ( Buffer,"b" ) == 0 )
        {
            Log ( DEBUG,CommonMessage ( 260 ) );
            Timeout = 0;
            TimedBootAllowed = 1;
            ProgressBarAllowed = 0;
            IsInputCorrect = 1;
        }
        else if ( strcasecmp ( Buffer,"c" ) == 0 )
        {
            Log ( DEBUG,CommonMessage ( 261 ) );
            // We can not set timeout to 0 if timed boot is not allowed since
            // if Timeout=0 it always means immediate boot
            Timeout = 18*60;
            TimedBootAllowed = 0;
            ProgressBarAllowed = 0;
            IsInputCorrect = 1;
        }
        else if ( strcasecmp ( Buffer,"d" ) == 0 )
        {
            Log ( DEBUG,CommonMessage ( 262 ) );
            MbldrManageTimerInterruptKey ();
            return;
        }
        else if ( strcasecmp ( Buffer,"e" ) == 0 )
        {
            Log ( DEBUG,CommonMessage ( 263 ) );
            MbldrConfigureProgressBarParameters ();
            return;
        }
        else if ( strcasecmp ( Buffer,"f" ) == 0 )
        {
            Log ( DEBUG,CommonMessage ( 200 ) );
            return;
        }
        else
        {
            // At this point it is safe to do a string to int conversion
            // Since maximum value is 36000 we don't care about signed/unsigned
            UserChoice = atol ( Buffer );
            Log ( DEBUG,CommonMessage ( 182 ),Buffer,UserChoice );
            if (( UserChoice > 0 ) && ( UserChoice <= 36000 ))
            {
                ProgressBarAllowed = 1;
                Timeout = UserChoice;
                Timeout *= 18;
                if ( TimedBootAllowed == 0 )
                {
                    TimedBootAllowed = 1;
                }
                IsInputCorrect = 1;
            }
        }
    } while ( IsInputCorrect == 0 );

    CommonConstructBootMenuText ( 0 );
    MbldrShowInfoMessage ( CommonMessage ( 265 ) );
}

// Manages boot menu text allowing review of current contents, loading and writing
// to file, regenerating text automatically and resetting it to empty string
// This function produces a menu on a screen asking a user what to do
void MbldrManageBootMenuText
    (
    void
    )
{
    // Text buffer representing user's input
    char Buffer[ 128 ];
    // Text buffer representing file name to save/load current boot menu text
    char FileName[ 1024 ];
    // Handle of opened file
    FILE* FileHandle;

    // The contents of boot menu text we are dealing with depend on current state -
    // whether user added partitions or edit existing labels. It could be either old
    // (what was read from existing mbr) or new automatically generated. "Custom boot
    // menu text" flag also affect the contents of boot menu

    // Output the information whether we are in cusom mode or not
    printf ( "\n" );
    if ( CustomBootMenuText != 0 )
    {
        printf ( CommonMessage ( 266 ) );
        printf ( "\n" );
    }
    // Output a menu asking for operation on boot menu text
    printf ( "a. " );
    printf ( CommonMessage ( 267 ) );
    printf ( "\n" );
    printf ( "b. " );
    printf ( CommonMessage ( 268 ) );
    printf ( "\n" );
    printf ( "c. " );
    printf ( CommonMessage ( 269 ) );
    printf ( "\n" );
    printf ( "d. " );
    printf ( CommonMessage ( 270 ) );
    printf ( "\n" );
    printf ( "e. " );
    printf ( CommonMessage ( 271 ) );
    printf ( "\n" );
    printf ( "f. " );
    printf ( CommonMessage ( 200 ) );
    printf ( "\n" );
    printf ( "q. " );
    printf ( CommonMessage ( 179 ) );
    printf ( "\n" );

    // Wait for user's input
    do
    {
        printf ( CommonMessage ( 224 ) );
        printf ( " (a,b,c,d,e,f,q): " );
        strcpy ( Buffer,"" );
        scanf ( "%127[^\n]",Buffer );
        getchar ();
        Log ( DEBUG,CommonMessage ( 173 ),Buffer );
    } while (
            ( strcasecmp ( Buffer,"a" ) != 0 ) &&
            ( strcasecmp ( Buffer,"b" ) != 0 ) &&
            ( strcasecmp ( Buffer,"c" ) != 0 ) &&
            ( strcasecmp ( Buffer,"d" ) != 0 ) &&
            ( strcasecmp ( Buffer,"e" ) != 0 ) &&
            ( strcasecmp ( Buffer,"f" ) != 0 ) &&
            ( strcasecmp ( Buffer,"q" ) != 0 )
            );
    if ( strcasecmp ( Buffer,"q" ) == 0 )
    {
        Log ( DEBUG,CommonMessage ( 181 ) );
        exit ( 5 );
    }
    else if ( strcasecmp ( Buffer,"a" ) == 0 )
    {
        Log ( DEBUG,CommonMessage ( 267 ) );
        printf ( "%s: %u\n",CommonMessage ( 272 ),CommonGetMaximumSizeOfBootMenuText () );
        printf ( "%s: %u\n",CommonMessage ( 273 ),( unsigned int )strlen ( BootMenuText ) + 1 );
        printf ( "%s: %i\n",CommonMessage ( 274 ),CommonGetMaximumSizeOfBootMenuText () - ( ( int )strlen ( BootMenuText ) + 1 ) );
        printf ( CommonMessage ( 275 ) );
        printf ( "\n" );
        printf ( "---------------------------------------------------------------------" );
        if ( BootMenuText[ 0 ] != 0x0A )
        {
            printf ( "\n" );
        }
        if ( strcmp ( BootMenuText,"" ) != 0 )
        {
            printf ( "%s",BootMenuText );
            if ( BootMenuText[ strlen ( BootMenuText ) - 1 ] != 0x0A )
            {
                printf ( "\n" );
            }
        }
        printf ( "---------------------------------------------------------------------\n" );
        MbldrShowInfoMessage ( "" );
    }
    else if ( strcasecmp ( Buffer,"b" ) == 0 )
    {
        Log ( DEBUG,CommonMessage ( 268 ) );
        printf ( CommonMessage ( 276 ) );
        strcpy ( FileName,"" );
        scanf ( "%1023[^\n]",FileName );
        getchar ();
        FileHandle = fopen ( FileName,"wb" );
        if ( FileHandle != NULL )
        {
            if ( strlen ( BootMenuText ) != 0 )
            {
                if ( fwrite ( BootMenuText,strlen ( BootMenuText ),1,FileHandle ) != 1 )
                {
                    MbldrShowInfoMessage ( CommonMessage ( 277 ) );
                }
                else if ( fflush ( FileHandle ) != 0 )
                {
                    MbldrShowInfoMessage ( CommonMessage ( 278 ) );
                }
                else
                {
                    MbldrShowInfoMessage ( CommonMessage ( 279 ) );
                }
            }
            fclose ( FileHandle );
        }
        else
        {
            MbldrShowInfoMessage ( CommonMessage ( 280 ) );
        }
    }
    else if ( strcasecmp ( Buffer,"c" ) == 0 )
    {
        Log ( DEBUG,CommonMessage ( 269 ) );
        // Set custom mode
        CustomBootMenuText = 1;
        printf ( CommonMessage ( 266 ) );
        printf ( "\n" );
        printf ( CommonMessage ( 281 ) );
        strcpy ( FileName,"" );
        scanf ( "%1023[^\n]",FileName );
        getchar ();
        FileHandle = fopen ( FileName,"rb" );
        if ( FileHandle != NULL )
        {
            memset ( BootMenuText,0,sizeof ( BootMenuText ) );
            // Read maximum number of characters minus one because trailing NULL-character should not be overwritten
            if ( fread ( BootMenuText,1,CommonGetMaximumSizeOfBootMenuText () - 1,FileHandle ) == 0 )
            {
                MbldrShowInfoMessage ( CommonMessage ( 282 ) );
            }
            else if ( fgetc ( FileHandle ) != EOF )
            {
                MbldrShowInfoMessage ( CommonMessage ( 283 ) );
            }
            else
            {
                MbldrShowInfoMessage ( CommonMessage ( 284 ) );
            }
            fclose ( FileHandle );
        }
        else
        {
            MbldrShowInfoMessage ( CommonMessage ( 280 ) );
        }
    }
    else if ( strcasecmp ( Buffer,"d" ) == 0 )
    {
        Log ( DEBUG,CommonMessage ( 270 ) );
        // Set custom mode
        CustomBootMenuText = 1;
        // Clear boot menu text
        strcpy ( BootMenuText,"" );
        MbldrShowInfoMessage ( "%s %s",CommonMessage ( 266 ),CommonMessage ( 285 ) );
    }
    else if ( strcasecmp ( Buffer,"e" ) == 0 )
    {
        Log ( DEBUG,CommonMessage ( 271 ) );
        // Reset custom mode
        CustomBootMenuText = 0;
        // Regenerate boot menu text
        CommonConstructBootMenuText ( 0 );
        MbldrShowInfoMessage ( "%s %s",CommonMessage ( 287 ),CommonMessage ( 286 ) );
    }
    else if ( strcasecmp ( Buffer,"f" ) == 0 )
    {
        Log ( DEBUG,CommonMessage ( 200 ) );
        return;
    }
}

// Allows a user to choose which keys will be used to boot operating systems
// from a list. There is an ability to analyze either an ASCII code of the
// particular key being pressed or the scan-code. All operating systems in a
// list are associated with incrementing list of ASCII/scan-codes so there is
// no ability to define particular key for each operating system. User is
// limited to choose an offset for the first OS in a list. All other OSes will
// be identified with an appropriately increased ASCII/scan code (initial code
// + the offset of OS in a list)
void MbldrDefineBootKeys
    (
    void
    )
{
    // Text buffer representing user's input
    char Buffer[ 128 ];

    // Output current key-mode (ASCII or scan-codes and a code of base key)
    printf ( "\n" );
    if ( ( UseAsciiOrScanCode == 0 ) && ( BaseAsciiOrScanCode == '1' ) )
    {
        // ASCII-codes mode ('1', '2', etc.)
        printf ( "%s %s",CommonMessage ( 288 ),CommonMessage ( 289 ) );
        printf ( "\n" );
    }
    else if ( ( UseAsciiOrScanCode == 1 ) && ( BaseAsciiOrScanCode == 0x3B ) )
    {
        // Scan-codes mode ('F1', 'F2', etc.)
        printf ( "%s %s",CommonMessage ( 290 ),CommonMessage ( 291 ) );
        printf ( "\n" );
    }
    else
    {
        if ( UseAsciiOrScanCode == 0 )
        {
            printf ( CommonMessage ( 288 ) );
        }
        else
        {
            printf ( CommonMessage ( 290 ) );
        }
        printf ( " " );
        printf ( CommonMessage ( 292 ),BaseAsciiOrScanCode );
        printf ( "\n" );
    }
    // Output a menu asking for operation on boot menu text
    printf ( "a. " );
    printf ( CommonMessage ( 293 ) );
    printf ( "\n" );
    printf ( "b. " );
    printf ( CommonMessage ( 294 ) );
    printf ( "\n" );
    printf ( "c. " );
    printf ( CommonMessage ( 295 ) );
    printf ( "\n" );
    printf ( "d. " );
    printf ( CommonMessage ( 296 ) );
    printf ( "\n" );
    printf ( "e. " );
    printf ( CommonMessage ( 200 ) );
    printf ( "\n" );
    printf ( "q. " );
    printf ( CommonMessage ( 179 ) );
    printf ( "\n" );

    // Wait for user's input
    do
    {
        printf ( CommonMessage ( 224 ) );
        printf ( " (a,b,c,d,e,q): " );
        strcpy ( Buffer,"" );
        scanf ( "%127[^\n]",Buffer );
        getchar ();
        Log ( DEBUG,CommonMessage ( 173 ),Buffer );
    } while (
            ( strcasecmp ( Buffer,"a" ) != 0 ) &&
            ( strcasecmp ( Buffer,"b" ) != 0 ) &&
            ( strcasecmp ( Buffer,"c" ) != 0 ) &&
            ( strcasecmp ( Buffer,"d" ) != 0 ) &&
            ( strcasecmp ( Buffer,"e" ) != 0 ) &&
            ( strcasecmp ( Buffer,"q" ) != 0 )
            );
    if ( strcasecmp ( Buffer,"q" ) == 0 )
    {
        Log ( DEBUG,CommonMessage ( 181 ) );
        exit ( 5 );
    }
    else if ( strcasecmp ( Buffer,"a" ) == 0 )
    {
        UseAsciiOrScanCode = 0;
        BaseAsciiOrScanCode = '1';
        CommonConstructBootMenuText ( 0 );
        MbldrShowInfoMessage ( CommonMessage ( 297 ) );
    }
    else if ( strcasecmp ( Buffer,"b" ) == 0 )
    {
        UseAsciiOrScanCode = 1;
        BaseAsciiOrScanCode = 0x3B;
        CommonConstructBootMenuText ( 0 );
        MbldrShowInfoMessage ( CommonMessage ( 298 ) );
    }
    else if (( strcasecmp ( Buffer,"c" ) == 0 ) || ( strcasecmp ( Buffer,"d" ) == 0 ))
    {
        printf ( "\n" );
        printf ( CommonMessage ( 299 ) );
        printf ( "\n" );
        if ( strcasecmp ( Buffer,"c" ) == 0 )
        {
            Log ( DEBUG,CommonMessage ( 300 ) );
            printf ( CommonMessage ( 300 ) );
            printf ( "\n" );
            UseAsciiOrScanCode = 0;
        }
        else if ( strcasecmp ( Buffer,"d" ) == 0 )
        {
            Log ( DEBUG,CommonMessage ( 301 ) );
            printf ( CommonMessage ( 301 ) );
            printf ( "\n" );
            UseAsciiOrScanCode = 1;
        }
        do
        {
            printf ( CommonMessage ( 302 ) );
            printf ( ": " );
            scanf ( "%127[^\n]",Buffer );
            getchar ();
        } while (
                 ( strcasecmp ( Buffer,"0" ) != 0 ) &&
                 (
                  ( atoi ( Buffer ) <= 0 ) ||
                  ( atoi ( Buffer ) > 255 )
                 )
                );
        BaseAsciiOrScanCode = atoi ( Buffer );
        CommonConstructBootMenuText ( 0 );
        MbldrShowInfoMessage ( CommonMessage ( 303 ) );
    }
    else if ( strcasecmp ( Buffer,"e" ) == 0 )
    {
        Log ( DEBUG,CommonMessage ( 200 ) );
        return;
    }
}

// Writes current configuration of mbldr either to MBR or to a file. Also
// allows doing backup/restore operations with MBR.
void MbldrSaveOrBackupCurrentConfiguration
    (
    void
    )
{
    // Text buffer representing user's input
    char Buffer[ 128 ];
    // First sector of chosen hard disk (including MBR and partition table)
    unsigned char FirstSector[ 512 ];

    // Request MBR from the chosen disk
    DisksReadSector ( 0,FirstSector );

    // Output a menu asking for backup/restore/save operation
    printf ( "\n" );
    printf ( "a. " );
    printf ( CommonMessage ( 304 ) );
    printf ( " (" );
    printf ( CommonMessage ( 306 ) );
    printf ( ")" );
    printf ( "\n" );
    printf ( "b. " );
    printf ( CommonMessage ( 304 ) );
    printf ( " (" );
    printf ( CommonMessage ( 307 ) );
    printf ( ")" );
    printf ( "\n" );
    printf ( "c. " );
    printf ( CommonMessage ( 305 ) );
    printf ( " (" );
    printf ( CommonMessage ( 306 ) );
    printf ( ")" );
    printf ( "\n" );
    printf ( "d. " );
    printf ( CommonMessage ( 305 ) );
    printf ( " (" );
    printf ( CommonMessage ( 307 ) );
    printf ( ")" );
    printf ( "\n" );
    printf ( "e. " );
    printf ( CommonMessage ( 355 ) );
    printf ( " (" );
    printf ( CommonMessage ( 306 ) );
    printf ( ")" );
    printf ( "\n" );
    printf ( "f. " );
    printf ( CommonMessage ( 355 ) );
    printf ( " (" );
    printf ( CommonMessage ( 307 ) );
    printf ( ")" );
    printf ( "\n" );
    printf ( "g. " );
    printf ( CommonMessage ( 200 ) );
    printf ( "\n" );
    printf ( "q. " );
    printf ( CommonMessage ( 179 ) );
    printf ( "\n" );

    // Wait for user's input
    do
    {
        printf ( CommonMessage ( 224 ) );
        printf ( " (a,b,c,d,e,f,g,q): " );
        strcpy ( Buffer,"" );
        scanf ( "%127[^\n]",Buffer );
        getchar ();
        Log ( DEBUG,CommonMessage ( 173 ),Buffer );
    } while (
            ( strcasecmp ( Buffer,"a" ) != 0 ) &&
            ( strcasecmp ( Buffer,"b" ) != 0 ) &&
            ( strcasecmp ( Buffer,"c" ) != 0 ) &&
            ( strcasecmp ( Buffer,"d" ) != 0 ) &&
            ( strcasecmp ( Buffer,"e" ) != 0 ) &&
            ( strcasecmp ( Buffer,"f" ) != 0 ) &&
            ( strcasecmp ( Buffer,"g" ) != 0 ) &&
            ( strcasecmp ( Buffer,"q" ) != 0 )
            );
    if ( strcasecmp ( Buffer,"q" ) == 0 )
    {
        Log ( DEBUG,CommonMessage ( 181 ) );
        exit ( 5 );
    }
    else if ( strcasecmp ( Buffer,"a" ) == 0 )
    {
        // Backup existing MBR to file

        // Text buffer representing file name to locate file for MBR contents backup
        char FileName[ 1024 ];
        // Handle of opened file
        FILE* FileHandle;

        Log ( DEBUG,CommonMessage ( 304 ) );
        Log ( DEBUG,CommonMessage ( 306 ) );

        printf ( CommonMessage ( 276 ) );
        strcpy ( FileName,"" );
        scanf ( "%1023[^\n]",FileName );
        getchar ();
        FileHandle = fopen ( FileName,"wb" );
        if ( FileHandle != NULL )
        {
            if ( fwrite ( FirstSector,512,1,FileHandle ) != 1 )
            {
                MbldrShowInfoMessage ( CommonMessage ( 277 ) );
            }
            else if ( fflush ( FileHandle ) != 0 )
            {
                MbldrShowInfoMessage ( CommonMessage ( 278 ) );
            }
            else
            {
                MbldrShowInfoMessage ( CommonMessage ( 279 ) );
            }
            fclose ( FileHandle );
        }
        else
        {
            MbldrShowInfoMessage ( CommonMessage ( 280 ) );
        }
    }
    else if ( strcasecmp ( Buffer,"b" ) == 0 )
    {
        // Backup existing MBR to sector

        // Number of sector to write the data to
        unsigned long ulSectorNumber;
        // Pointer to the end of decoded number in the string entered by user
        char *pEndPointer;

        Log ( DEBUG,CommonMessage ( 304 ) );
        Log ( DEBUG,CommonMessage ( 307 ) );

        // Get a list of free sectors on a very first track
        CommonGetListOfSpecificSectors ( 0,Buffer );

        printf ( CommonMessage ( 398 ) );
        printf ( Buffer );
        printf ( "\n" );

        // Request the sector number
        strcpy ( Buffer,"" );
        scanf ( "%127[^\n]",Buffer );
        getchar ();
        Log ( DEBUG,CommonMessage ( 173 ),Buffer );
        errno = 0;
        ulSectorNumber = strtoul ( Buffer,&pEndPointer,10 );
        // Check that no error has occured during conversion, string is not empty and
        // whole string has been converted to a number
        if (( errno == 0 ) && ( strcmp ( Buffer,"" ) != 0 ) && ( *pEndPointer == 0 ))
        {
            if ( ulSectorNumber != 0 )
            {
                // Message for displaying. Used to warn a user and ask for action confirmation
                char *pMessage;

                if ( ulSectorNumber >= 63 )
                {
                    pMessage = CommonMessage ( 401 );
                }
                else
                {
                    pMessage = CommonMessage ( 203 );
                }

                // Before actual write to sector on HDD ask once again
                if ( MbldrYesNo ( pMessage ) != 0 )
                {
                    DisksWriteSector ( ulSectorNumber,FirstSector );
                    MbldrShowInfoMessage ( CommonMessage ( 399 ) );
                    // Since the sector has been written, suggest a used to add a chainload entry
                    if ( MbldrYesNo ( CommonMessage ( 402 ) ) != 0 )
                    {
                        if ( NumberOfBootablePartitions == 9 )
                        {
                            MbldrShowInfoMessage ( CommonMessage ( 183 ) );
                        }
                        else
                        {
                            BootablePartitions[ NumberOfBootablePartitions ].relative_sectors_offset = ulSectorNumber;
                            strcpy ( BootablePartitions[ NumberOfBootablePartitions ].label,"" );
                            NumberOfBootablePartitions++;
                            Log ( DEBUG,CommonMessage ( 208 ) );
                            MbldrShowInfoMessage ( CommonMessage ( 208 ) );

                            // Update boot menu text
                            CommonConstructBootMenuText ( 0 );
                        }
                    }
                }
                else
                {
                    MbldrShowInfoMessage ( CommonMessage ( 400 ) );
                }
            }
        }
    }
    else if ( strcasecmp ( Buffer,"c" ) == 0 )
    {
        // Restore MBR from a file backup

        // Backup of first sector (including MBR and partition table)
        unsigned char BackupSector[ 512 ];
        // Text buffer representing file name to locate file for MBR contents backup
        char FileName[ 1024 ];
        // Handle of opened file
        FILE* FileHandle;

        Log ( DEBUG,CommonMessage ( 305 ) );
        Log ( DEBUG,CommonMessage ( 306 ) );

        printf ( CommonMessage ( 281 ) );
        strcpy ( FileName,"" );
        scanf ( "%1023[^\n]",FileName );
        getchar ();
        FileHandle = fopen ( FileName,"rb" );
        if ( FileHandle != NULL )
        {
            if ( fread ( BackupSector,512,1,FileHandle ) != 1 )
            {
                MbldrShowInfoMessage ( CommonMessage ( 282 ) );
            }
            else
            {
                if ( fgetc ( FileHandle ) != EOF )
                {
                    MbldrShowInfoMessage ( CommonMessage ( 308 ) );
                }
                else
                {
                    if ( CommonPrepareBackupSectorToRestore ( FirstSector,BackupSector ) == 1 )
                    {
                        DisksWriteSector ( 0,BackupSector );
                        MbldrShowInfoMessage ( CommonMessage ( 315 ) );
                    }
                    else
                    {
                        MbldrShowInfoMessage ( CommonMessage ( 316 ) );
                    }
                }
            }
            fclose ( FileHandle );
        }
        else
        {
            MbldrShowInfoMessage ( CommonMessage ( 280 ) );
        }
    }
    else if ( strcasecmp ( Buffer,"d" ) == 0 )
    {
        // Restore MBR from a sector backup

        // Backup of first sector (including MBR and partition table)
        unsigned char BackupSector[ 512 ];
        // Number of sector to write the data to
        unsigned long ulSectorNumber;
        // Pointer to the end of decoded number in the string entered by user
        char *pEndPointer;

        Log ( DEBUG,CommonMessage ( 305 ) );
        Log ( DEBUG,CommonMessage ( 307 ) );

        // Get a list of free sectors on a very first track
        CommonGetListOfSpecificSectors ( 1,Buffer );

        printf ( CommonMessage ( 403 ) );
        printf ( Buffer );
        printf ( "\n" );

        // Request the sector number
        strcpy ( Buffer,"" );
        scanf ( "%127[^\n]",Buffer );
        getchar ();
        Log ( DEBUG,CommonMessage ( 173 ),Buffer );
        errno = 0;
        ulSectorNumber = strtoul ( Buffer,&pEndPointer,10 );
        // Check that no error has occured during conversion, string is not empty and
        // whole string has been converted to a number
        if (( errno == 0 ) && ( strcmp ( Buffer,"" ) != 0 ) && ( *pEndPointer == 0 ))
        {
            if ( ulSectorNumber != 0 )
            {
                if (
                    ( ulSectorNumber < 63 ) ||
                    (
                     ( ulSectorNumber >= 63 ) &&
                     ( MbldrYesNo ( CommonMessage ( 404 ) ) != 0 )
                    )
                   )
                {
                    DisksReadSector ( ulSectorNumber,BackupSector );
                    if ( CommonPrepareBackupSectorToRestore ( FirstSector,BackupSector ) == 1 )
                    {
                        DisksWriteSector ( 0,BackupSector );
                        MbldrShowInfoMessage ( CommonMessage ( 315 ) );
                    }
                    else
                    {
                        MbldrShowInfoMessage ( CommonMessage ( 316 ) );
                    }
                }
                else
                {
                    MbldrShowInfoMessage ( CommonMessage ( 316 ) );
                }
            }
        }
    }
    else if ( strcasecmp ( Buffer,"e" ) == 0 )
    {
        // Save current mbldr configuration to file

        // Text buffer representing file name to locate file for MBR contents backup
        char FileName[ 1024 ];
        // Handle of opened file
        FILE* FileHandle;

        Log ( DEBUG,CommonMessage ( 355 ) );
        Log ( DEBUG,CommonMessage ( 306 ) );

        // Check whether mbldr configuration contains at least one partition
        if ( NumberOfBootablePartitions == 0 )
        {
            Log ( DEBUG,CommonMessage ( 209 ) );
            MbldrShowInfoMessage ( CommonMessage ( 209 ) );
        }
        // Check how many symbols are available while constructing boot menu text
        else if ( CommonConstructBootMenuText ( 0 ) >= 0 )
        {
            // Prepare binary image of MBR with mbldr
            CommonPrepareMBR ( FirstSector );

            printf ( CommonMessage ( 276 ) );
            strcpy ( FileName,"" );
            scanf ( "%1023[^\n]",FileName );
            getchar ();
            FileHandle = fopen ( FileName,"wb" );
            if ( FileHandle != NULL )
            {
                if ( fwrite ( FirstSector,512,1,FileHandle ) != 1 )
                {
                    MbldrShowInfoMessage ( CommonMessage ( 277 ) );
                }
                else if ( fflush ( FileHandle ) != 0 )
                {
                    MbldrShowInfoMessage ( CommonMessage ( 278 ) );
                }
                else
                {
                    MbldrShowInfoMessage ( CommonMessage ( 279 ) );
                }
                fclose ( FileHandle );
            }
            else
            {
                MbldrShowInfoMessage ( CommonMessage ( 280 ) );
            }
        }
    }
    else if ( strcasecmp ( Buffer,"f" ) == 0 )
    {
        // Save current mbldr configuration to sector

        // Number of sector to write the data to
        unsigned long ulSectorNumber;
        // Pointer to the end of decoded number in the string entered by user
        char *pEndPointer;

        Log ( DEBUG,CommonMessage ( 355 ) );
        Log ( DEBUG,CommonMessage ( 307 ) );

        // Check whether mbldr configuration contains at least one partition
        if ( NumberOfBootablePartitions == 0 )
        {
            Log ( DEBUG,CommonMessage ( 209 ) );
            MbldrShowInfoMessage ( CommonMessage ( 209 ) );
        }
        // Check how many symbols are available while constructing boot menu text
        else if ( CommonConstructBootMenuText ( 0 ) >= 0 )
        {
            // Prepare binary image of MBR with mbldr
            CommonPrepareMBR ( FirstSector );

            // Get a list of free sectors on a very first track
            CommonGetListOfSpecificSectors ( 0,Buffer );

            printf ( CommonMessage ( 398 ) );
            printf ( Buffer );
            printf ( "\n" );

            // Request the sector number
            strcpy ( Buffer,"" );
            scanf ( "%127[^\n]",Buffer );
            getchar ();
            Log ( DEBUG,CommonMessage ( 173 ),Buffer );
            errno = 0;
            ulSectorNumber = strtoul ( Buffer,&pEndPointer,10 );
            // Check that no error has occured during conversion, string is not empty and
            // whole string has been converted to a number
            if (( errno == 0 ) && ( strcmp ( Buffer,"" ) != 0 ) && ( *pEndPointer == 0 ))
            {
                // Message for displaying. Used to warn a user and ask for action confirmation
                char *pMessage;

                if ( ulSectorNumber >= 63 )
                {
                    pMessage = CommonMessage ( 401 );
                }
                else
                {
                    pMessage = CommonMessage ( 203 );
                }

                // Before actual write to sector on HDD ask once again
                if ( MbldrYesNo ( pMessage ) != 0 )
                {
                    DisksWriteSector ( ulSectorNumber,FirstSector );
                    if ( ulSectorNumber == 0 )
                    {
                        MbldrShowInfoMessage ( CommonMessage ( 315 ) );
                    }
                    else
                    {
                        MbldrShowInfoMessage ( CommonMessage ( 399 ) );
                    }
                }
                else
                {
                    if ( ulSectorNumber == 0 )
                    {
                        MbldrShowInfoMessage ( CommonMessage ( 316 ) );
                    }
                    else
                    {
                        MbldrShowInfoMessage ( CommonMessage ( 400 ) );
                    }
                }
            }
        }
    }
    else if ( strcasecmp ( Buffer,"g" ) == 0 )
    {
        Log ( DEBUG,CommonMessage ( 200 ) );
        return;
    }
}

// Main function of a program
int main
    (
    int argc,
    char* argv[]
    )
{
    // Text buffer representing user's input initialized with fake unused data
    // (however it is not empty to allow showing of a main menu)
    char Buffer[ 128 ] = "not empty";
    // Boolean flag indicating whether user wants to exit main menu
    // and all conditions allowing that have been satisfied
    unsigned char ExitFromMainMenu = 0;

    // Define text domain to get the proper translation
#if defined ( __unix__ )
    setlocale ( LC_ALL, "" );
#else
    bindtextdomain ( "mbldr", "./mo/" );
    {
        // Pointer to proper environment variable
        char* Locale = NULL;

        // Check LC_ALL enviroment variable
        if (
            ( getenv ( "LC_ALL" ) != NULL ) &&
            ( strcmp ( getenv ( "LC_ALL" ), "" ) != 0 )
           )
        {
            // Use LC_ALL, try to locate part with codepage
            Locale = strstr ( getenv ( "LC_ALL" ), "." );
        }
        else if (
                 ( getenv ( "LC_MESSAGES" ) != NULL ) &&
                 ( strcmp ( getenv ( "LC_MESSAGES" ), "") != 0 )
                )
        {
            // Use LC_MESSAGES, try to locate part with codepage
            Locale = strstr ( getenv ( "LC_MESSAGES" ), "." );
        }
        else if (
                 ( getenv ( "LANG" ) != NULL ) &&
                 ( strcmp ( getenv ( "LANG" ), "" ) != 0 )
                )
        {
            // Use LANG, try to locate part with codepage
            Locale = strstr ( getenv ( "LANG" ), "." );
        }

        // Check whether we can find codepage part
        if ( Locale != NULL )
        {
            // Move to next symbol (after ".")
            Locale++;
            if ( strcmp ( Locale,"" ) == 0 )
            {
                // This seems to be something strange
                // "." is present without codepage itself
                Locale = NULL;
            }
        }

        // If there is no proper environment variables,
        // try to locate codepage using DOS interrupts.
        // This requires proper
        if ( Locale == NULL )
        {
            // Codeset/codepage string
            char Codeset[ 32 ];
#if defined ( __DJGPP__ )
            // CPU registers set for interrupt call
            __dpmi_regs r;

            // Invoke "Get codepage" from DOS
            r.h.ah = 0x66; // Function code
            r.h.al = 1;    // Do get (not set)
            __dpmi_int ( 0x21,&r );
            // Check Carry flag
            if ( ( r.x.flags & 0x0001 ) == 0x0001 )
            {
                Log ( DEBUG,CommonMessage ( 6 ),r.x.ax );
                Log ( FATAL,CommonMessage ( 206 ) );
            }
            // Convert codepage from number to a string
            // Here we hope that appropriate codeset exists
            // (we can't produce strings like "IBM866" or
            // "KOI8-R" here)
            sprintf ( Codeset,"%i",r.x.bx );
#elif ( _WIN32 )
            sprintf ( Codeset,"%i",GetOEMCP () );
#else
    #error "Unsupported platform"
#endif /* __DJGPP__ */
            // Set codeset using information from DOS interrupt
            bind_textdomain_codeset ( "mbldr", Codeset );
        }
        else
        {
            // Set codeset using information from locale-related
            // environment variables
            bind_textdomain_codeset ( "mbldr", Locale );
        }
    }
#endif /* __unix__ */
    textdomain ( "mbldr" );

    // Perform parsing of command line
    if ( CommonParseCommandLine ( argc, argv ) != 0 )
    {
        // Command-line contains parameters that prohibit execution of a program
        // but no error has happened
        return ( 0 );
    }

    // Let user choose the device from a list if it is not defined in a
    // command line
    if ( strcmp ( Device,"" ) == 0 )
    {
        MbldrChooseDevice ();
    }
    Log ( DEBUG,CommonMessage ( 225 ),Device );

    // Detect all partitions starting from MBR
    CommonDetectBootablePartitions( 0 );
    if ( NumberOfFoundPartitions == 0 )
    {
        Log ( DEBUG,CommonMessage ( 178 ) );
    }

    // Check whether mbldr is already installed and read existing settings
    CommonCheckMBR ();

    // Main cycle for menu and user's choice
    do
    {
        if ( strcmp ( Buffer,"" ) != 0 )
        {
            printf ( "\n" );
            printf ( "a. " );
            printf ( CommonMessage ( 317 ) );
            printf ( "\n" );
            printf ( "b. " );
            printf ( CommonMessage ( 318 ) );
            printf ( "\n" );
            printf ( "c. " );
            printf ( CommonMessage ( 319 ) );
            printf ( "\n" );
            printf ( "d. " );
            printf ( CommonMessage ( 320 ) );
            printf ( "\n" );
            printf ( "e. " );
            printf ( CommonMessage ( 321 ) );
            printf ( "\n" );
            printf ( "f. " );
            printf ( CommonMessage ( 322 ) );
            printf ( "\n" );
            printf ( "q. " );
            printf ( CommonMessage ( 179 ) );
            printf ( "\n" );
        }
        printf ( CommonMessage ( 224 ) );
        printf ( " (a,b,c,d,e,f,q): " );
        strcpy ( Buffer,"" );
        scanf ( "%127[^\n]",Buffer );
        getchar ();
        if ( strcasecmp ( Buffer,"q" ) == 0 )
        {
            Log ( DEBUG,CommonMessage ( 181 ) );
            ExitFromMainMenu = 1;
        }
        else if ( strcasecmp ( Buffer,"a" ) == 0 )
        {
            Log ( DEBUG,CommonMessage ( 317 ) );
            MbldrAddPartitionToCurrentConfiguration ();
        }
        else if ( strcasecmp ( Buffer,"b" ) == 0 )
        {
            Log ( DEBUG,CommonMessage ( 318 ) );
            MbldrManageBootablePartitions ();
        }
        else if ( strcasecmp ( Buffer,"c" ) == 0 )
        {
            Log ( DEBUG,CommonMessage ( 319 ) );
            MbldrConfigureTimedBoot ();
        }
        else if ( strcasecmp ( Buffer,"d" ) == 0 )
        {
            Log ( DEBUG,CommonMessage ( 320 ) );
            MbldrManageBootMenuText ();
        }
        else if ( strcasecmp ( Buffer,"e" ) == 0 )
        {
            Log ( DEBUG,CommonMessage ( 321 ) );
            MbldrDefineBootKeys ();
        }
        else if ( strcasecmp ( Buffer,"f" ) == 0 )
        {
            Log ( DEBUG,CommonMessage ( 322 ) );
            MbldrSaveOrBackupCurrentConfiguration ();
        }
        else
        {
            // User pressed something wrong
            strcpy ( Buffer,"" );
        }
    } while ( ExitFromMainMenu == 0 );

    // Return success
    Log ( DEBUG,CommonMessage ( 323 ) );
    return ( 0 );
}

