// Project name:  Master Boot Loader (mbldr)
// File name:     disks.h
// See also:      disks.c
// Author:        Arnold Shade
// Creation date: 24 April 2006
// License type:  BSD
// URL:           http://mbldr.sourceforge.net/
// Description:   Hard disk related operations that include
// enumeration of disks, reading/writing of sectors and
// determination of drive description/capabilities
// Under DOS: using low-level (BIOS-interrupts interface and
// ATA/SATA interface level) according to EDD3 specification.
// Under Unix: sectors reading and writing uses regular files
// I/O, disk drives enumeration and description retrieval is
// done using /proc filesystem under Linux and several system
// utilities under BSD (sysctl, atacontrol, camcontrol)
// Under Windows: using built-in functions and I/O interface

#if !defined ( _DISKS_H )
#define _DISKS_H

// This type is used to create a list (dynamic array) with
// information about disk drives present in a system. The array
// is allocated with DisksCollectInfo() function and should be
// released with DisksFreeInfo() function
typedef struct _DisksInfo_t
{
    // Pointer to next element in the dynamic array
    // or NULL in case of last element
    struct _DisksInfo_t *pNext;

    // Name of the disk device
    // DOS: string representation of a hex value like "0x80"
    // Unix (Linux/BSD): string representing device filename
    //  like "/dev/hda"
    // Windows: virtual filename like "\\.\PhysicalDrive0"
    char sDiskName[ 256 ];

    // Textual description of the appropriate disk device. This
    // could be empty as well is the description could not be
    // retrieved for some reasons
    char sDiskDescription[ 256 ];
} DisksInfo_t;

// Creates a dynamic array with information about disk drives
// present in a system. The result pointer could be NULL if no
// drives has been detected. Please note that you need to pass
// pointer to pointer to DisksInfo_t structure (the function
// updates the pointer's value)
void DisksCollectInfo
    (
    DisksInfo_t **ppDisksArray
    );

// Releases memory allocated by DisksCollectInfo() function
// Invoke this function when the list of disk drives info has
// been completely processed
void DisksFreeInfo
    (
    DisksInfo_t *pDisksArray
    );

// Reads one sector from disk device
// Under DOS: using function 42h of INT 13h
// Under Windows/Unix: using regular file operations
// Length of the buffer should be 512 bytes
void DisksReadSector
    (
    unsigned long SectorNumber,
    unsigned char* Buffer
    );

// Writes one sector to disk device
// Under DOS: using function 43h of INT 13h
// Under Windows/Unix: using regular file operations
// Length of the buffer should be 512 bytes
void DisksWriteSector
    (
    unsigned long SectorNumber,
    unsigned char* Buffer
    );

#endif /* _DISKS_H */

