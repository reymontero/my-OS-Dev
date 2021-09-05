// Project name:  Master Boot Loader (mbldr)
// File name:     common.h
// See also:      common.c, mbldrcli.c, mbldrgui.cpp, mbldr.asm, mbldr.h
// Author:        Arnold Shade
// Creation date: 14 February 2007
// License type:  BSD
// URL:           http://mbldr.sourceforge.net/
// Description:   Contains common functions, definitions and
// data structures for CLI and GUI versions of mbldr program

#if !defined ( _COMMON_H )
#define _COMMON_H

// ------------------------------------------------------------
// Definitions
// ------------------------------------------------------------

// Major+minor version of the mbldr software
#define MBLDR_VERSION "1.46"
// String for appending to the boot menu text if progress-bar is used
#define PROGRESS_BAR_STRING "[          ]\r["

// ------------------------------------------------------------
// Types
// ------------------------------------------------------------

// Entry describing found partition
struct FoundPartitionEntry
{
    unsigned char partition_identifier;
    unsigned char is_primary;
    unsigned long relative_sectors_offset;
    unsigned long size_in_sectors;
};

// Parameters for mbldr configuration (all variables are initialized with improper
// values to indicate they are not yet configured by a user)
struct BootablePartitionEntry
{
    unsigned long relative_sectors_offset;
    char label[ 256 ];
};

// ------------------------------------------------------------
// Declaration of shared arrays and structures
// ------------------------------------------------------------

// Array of partitions descriptions. This list was mainly copied
// from http://www.win.tue.nl/~aeb/partitions/partition_types-1.html
// I changed it a little bit to shorten textual string
// (allowing them to be displayed correctly in 80chars-width
// terminals like DOS in its usual 80x25 text video mode)
// This array does not contain partition identifiers because
// the identifier is an index of particular entry
extern const char* PartitionTypes[];
// Windows/Unix: Device name of a hard disk to be configured
// DOS: String representation of BIOS device number of a hard disk
// (in hexadecimal mode with "0x" prefix)
extern char Device[ 1024 ];
// Number of partitions in the table
extern unsigned short NumberOfFoundPartitions;
// Array of found partitions (not more than 256 entries is allowed)
extern struct FoundPartitionEntry FoundPartitions[ 256 ];
// A number between 1 and 9
extern unsigned char NumberOfBootablePartitions;
// Boolean flag indicating the presence of cusom boot menu text
// (the boot menu text should not be generated automatically)
extern unsigned char CustomBootMenuText;
// Text describing boot menu (512 is unreachable maximum)
extern char BootMenuText[ 512 ];
// Array of offsets in sectors representing bootable partitions
extern struct BootablePartitionEntry BootablePartitions[ 9 ];
// A number between 0 and NumberBootablePartitions-1
extern unsigned char NumberOfDefaultPartition;
// Either 0x1B (Esc) or 0x20 (Space)
extern unsigned char TimerInterruptKey;
// Either 0 (not allowed) or 1 (allowed)
extern unsigned char ProgressBarAllowed;
// ASCII code of a symbol being used for progress-bar output
// 0 means that progress-bar should look like "[9876543210]"
// Any other code refers to a real symbol in ASCII table like "[**********]"
extern unsigned char ProgressBarSymbol;
// Either 0 (not allowed) or 1 (allowed, user timer) or 2 (allowed, system timer)
extern unsigned char TimedBootAllowed;
// Timeout for default boot. Here it is measured in ticks (1/18th of a
// second). Maximum value is 64800*18*10 (10 hours), minimum is 0 which
// means immediate boot. Default value for timeout is 1 minute
extern unsigned long Timeout;
// 0 means ASCII code of a key should be analyzed, 1 means scan-code
extern unsigned char UseAsciiOrScanCode;
// Base of the key code that should be used to boot any of the operating systems
extern unsigned char BaseAsciiOrScanCode;
// Either 0 (don't hide other primary partitions) or 1 (hide them)
extern unsigned char HideOtherPrimaryPartitions;
// Either 0 (don't mark primary partitions active/inactive) or 1 (mark them)
extern unsigned char MarkActivePartition;

// ------------------------------------------------------------
// Common declarations of functions which have different
// implementations in mbldrcli and mbldrgui (not implemented
// in common module)
// ------------------------------------------------------------

// Displays information message on a screen
void MbldrShowInfoMessage
    (
    const char *message_format,
    ...
    );

// Asks a user with yes/no possible answers. If user disagrees (answer is no)
// then return value is 0, otherwise (if user says yes) the return value is 1
unsigned char MbldrYesNo
    (
    char *message_string
    );

// Warns a user and suggests him two choices of possible answer (return values
// are 0 and 1 respectively). User is also allowed to cancel the choice (-1 is
// returned)
signed char MbldrChooseOneOfTwo
    (
    char *warning_string,
    char *choice1_string,
    char *choice2_string
    );

// Displays about message on a screen
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
    );

// ------------------------------------------------------------
// Declarations of functions
// ------------------------------------------------------------

// Get a message from array of messages for logging and user interface
// with possible translation
char* CommonMessage
    (
    unsigned int uiMessageIndex
    );

// Parse command-line and process all options. Returns 0 if the program should
// be continued, 1 otherwise (in case if parameters contain request to
// command-line help). argc and argv[] parameters should be the same to what
// is passed to main() function of a program
int CommonParseCommandLine
    (
    int argc,
    char *argv[]
    );

// Exchanges two partition entries to sort out the array of found partitions
void CommonSwapPartitionEntries
    (
    struct FoundPartitionEntry* pPartitionEntry1,
    struct FoundPartitionEntry* pPartitionEntry2
    );

// Reads MBR and extended partitions and locates bootable primary partitions
// or logical disks. Every found partition will be included in this list
// even if it is a swap/data/backup or system. The decision whether to include
// such partitions is on the user during the configuration process.
// SectorNumber is a number of sector from where the analyzis begins. Should
// be always 0 pointing to MBR at the time of initial call (however during
// recursion it may change)
void CommonDetectBootablePartitions
    (
    unsigned long SectorNumber
    );

// Constructs boot menu text adding optional header and mandatory list of bootable
// partitions. Return value represents how many symbols are available while
// constructing boot menu text
// If this value is negative, user has typed too long labels
// If this value is positive or equal to 0, boot menu text was constructed
// successfully. Returned value is a number of extra characters user is
// allowed to enter. However we try to fill boot menu text as efficient as
// possible (by adding extra optional strings if there is empty space
// available), so actual amount of free bytes in the boot menu text could
// not be equal to the value returned by this function
// The parameter of this function is used to determine how many details and extra
// strings to insert into boot menu text. The function calls itself recursively
// increasing the verbosity until there would be no free place in the place
// reserved for boot menu text (0 - low verbosity, 6 - max verbosity)
int CommonConstructBootMenuText
    (
    unsigned char Verbosity
    );

// Detects whether the mbldr is installed on the target hard disk
void CommonCheckMBR
    (
    void
    );

// Prepares binary image of MBR with mbldr setting all fields
void CommonPrepareMBR
    (
    unsigned char MBRImage[]
    );

// Return maximum size of boot menu text (including trailing NULL-character)
unsigned int CommonGetMaximumSizeOfBootMenuText
    (
    void
    );

// Return string describing the name and parameters of found (available) partition
// by its index. If partition index is equal to -1 then 'skip boot' option is returned.
// If partition index is equal to -2 then 'boot from next hdd' option is returned.
void CommonGetDescriptionOfAvailablePartition
    (
    int iPartitionIndex,
    char *Buffer
    );

// Return string describing the name and parameters of bootable (configured) partition
// by its index
void CommonGetDescriptionOfBootablePartition
    (
    int iPartitionIndex,
    char *Buffer
    );

// Return a string describing the list of specific sectors on a very first track of a
// chosen disk device. "Specific" here means "completely filled by zeroes" if the
// "iType" parameter is equal to 0 or "having a 0x55AA signature at the end" if "iType"
// is equal to 1
void CommonGetListOfSpecificSectors
    (
    int iType,
    char *Buffer
    );

// Verifies and compares all critical fields of the current MBR and its backup in
// order to prepare for restoring from this backup. Allows a user to choose which
// fields need to be saved in MBR and which will be taken from backup overwriting
// current MBR (providing him the information what exactly differs and can be lost
// while backup restoring). This function modifies the contents of BackupSector
// (passed as a parameter) and returns 1 if user agrees to write MBR back to disk
// from the backup or 0 if he cancels the operation.
int CommonPrepareBackupSectorToRestore
    (
    unsigned char MBRSector[],
    unsigned char BackupSector[]
    );

#endif /* _COMMON_H */

