// Project name:  Master Boot Loader (mbldr)
// File name:     disks.c
// See also:      disks.h
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

// Include standard system headers
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libintl.h>
#if defined ( __DJGPP__ )
#include <dpmi.h>
#include <go32.h>
#include <pc.h>
#include <sys/farptr.h>
#elif defined ( __unix__ )
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#if defined ( __linux__ )
#include <glob.h>
#endif /* __linux__ */
#elif defined ( _WIN32 )
#include <windows.h>
#include <ddk/ntdddisk.h>
#include <ddk/ntddscsi.h>
#include <ddk/scsi.h>
#else
    #error "Unsupported platform"
#endif /* __DJGPP__ or __unix__ or _WIN32 */

// Include local header
#include "disks.h"
#include "log.h"
#include "common.h"

#if defined ( __DJGPP__ )
// Default device parameters table, we use reduced table which should
// be 26 bytes in size
struct DeviceParameters {
    unsigned short structure_size;
    unsigned short information_flags;
    unsigned long cylinders;
    unsigned long heads;
    unsigned long sectors_per_track;
    unsigned char total_sectors[ 8 ];
    unsigned short bytes_in_sector;
    unsigned short dpte_offset;
    unsigned short dpte_segment;
    unsigned short device_path_key;
    unsigned char device_path_length; // May contain either 36 or 44 depending
                                      // on the length of device_path field
    unsigned char reserved1;
    unsigned short reserved2;
    unsigned char host_bus_type[ 4 ];
    unsigned char interface_type[ 8 ];
    unsigned char interface_path[ 8 ];
    unsigned char device_path[ 16 ];  // The length of this field could be 8
    unsigned char reserved3;
    unsigned char checksum;
} __attribute__((packed));

// Device address packet representing read/write access to a disk device
// Must be 16 bytes in size (there could be extra fields in extended
// version of the int 13h API causing the structure to be more than 16 bytes
// long, but we do not use them in sake of compatibility with older BIOSes)
struct DeviceAddressPacket {
    unsigned char packet_size;
    unsigned char reserved1;
    unsigned char number_of_blocks_to_transfer;
    unsigned char reserved2;
    unsigned short offset_of_host_transfer_buffer;
    unsigned short segment_of_host_transfer_buffer;
    unsigned char starting_logical_block_address[ 8 ]; // LBA sector number
} __attribute__((packed));
#endif /* __DJGPP__ */

#if defined ( _WIN32 )
// The virtual sector returned by IOCTL_SCSI_MINIPORT_IDENTIFY
typedef struct _IDSECTOR
{
    USHORT wGenConfig;
    USHORT wNumCyls;
    USHORT wReserved;
    USHORT wNumHeads;
    USHORT wBytesPerTrack;
    USHORT wBytesPerSector;
    USHORT wSectorsPerTrack;
    USHORT wVendorUnique[ 3 ];
    CHAR   sSerialNumber[ 20 ];
    USHORT wBufferType;
    USHORT wBufferSize;
    USHORT wECCSize;
    CHAR   sFirmwareRev[ 8 ];
    CHAR   sModel[ 40 ];
    USHORT wMoreVendorUnique;
    USHORT wDoubleWordIO;
    USHORT wCapabilities;
    USHORT wReserved1;
    USHORT wPIOTiming;
    USHORT wDMATiming;
    USHORT wBS;
    USHORT wNumCurrentCyls;
    USHORT wNumCurrentHeads;
    USHORT wNumCurrentSectorsPerTrack;
    ULONG  ulCurrentSectorCapacity;
    USHORT wMultSectorStuff;
    ULONG  ulTotalAddressableSectors;
    USHORT wSingleWordDMA;
    USHORT wMultiWordDMA;
    BYTE   bReserved[ 128 ];
} IDSECTOR, *PIDSECTOR;
#endif /* _WIN32 */

// Creates a dynamic array with information about disk drives
// present in a system. The result pointer could be NULL if no
// drives has been detected. Please note that you need to pass
// pointer to pointer to DisksInfo_t structure (the function
// updates the pointer's value)
void DisksCollectInfo
    (
    DisksInfo_t **ppDisksArray
    )
{
    // Pointer to last allocated element of the dynamic list
    DisksInfo_t *pDisksArrayCurrent = NULL;

    // Initially no drives have been found
    *ppDisksArray = NULL;

#if defined ( __DJGPP__ )

    // Set of registers to perform interrupt calls
    __dpmi_regs r;
    // Structure with generic parameters of disk device returned by IDE controller
    struct DeviceParameters buf;
    // Interface support bitmap
    unsigned short InterfaceSupportBitmap;
    // Generic counter used to process disk size
    signed short Count;
    // I/O port address: primary IDE by default
    unsigned short IOPortBaseAddress = 0x01F0;
    // This value is used to determine whether the device is a master or slave
    unsigned short ATADeviceBit = 0xFF;
    // Size of the disk
    unsigned long long DiskSize;
    // Number of disk device being analyzed
    unsigned int Device;
    // Total number of found devices
    unsigned int TotalDevices;

    // Invoke "Get Drive Parameters" from BIOS to determine total number of disk devices
    // (function 08h of INT 13h)
    r.h.ah = 0x08;
    r.h.dl = 0x80;
    r.x.ss = 0x0000;
    r.x.sp = 0x0000;
    r.x.flags = 0x0000;
    Log ( DEBUG,CommonMessage ( 4 ),0x08 );
    __dpmi_int ( 0x13,&r );
    Log ( DEBUG,CommonMessage ( 5 ) );
    // Check Carry flag
    if ( ( r.x.flags & 0x0001 ) == 0x0001 )
    {
        Log ( DEBUG,CommonMessage ( 6 ),r.h.ah );
        Log ( FATAL,CommonMessage ( 7 ) );
    }
    TotalDevices = r.h.dl;
    Log ( DEBUG,CommonMessage ( 8 ),TotalDevices );

    // Iterate through all found hard disks
    for ( Device=0 ; Device<TotalDevices ; Device++ )
    {
        // Get disk device parameters using functions 41h/48h of INT 13h
        Log ( DEBUG,CommonMessage ( 9 ),Device + 0x80 );

        // Invoke "Check Extensions Present" from BIOS
        r.h.ah = 0x41;
        r.x.bx = 0x55AA;
        r.h.dl = Device + 0x80;
        r.x.ss = 0x0000;
        r.x.sp = 0x0000;
        r.x.flags = 0x0000;
        Log ( DEBUG,CommonMessage ( 4 ),0x41 );
        __dpmi_int ( 0x13,&r );
        Log ( DEBUG,CommonMessage ( 5 ) );
        // Check Carry flag
        if ( ( r.x.flags & 0x0001 ) == 0x0001 )
        {
            Log ( DEBUG,CommonMessage ( 6 ),r.h.ah );
            Log ( DEBUG,CommonMessage ( 10 ) );
            // This device should not be used
            continue;
        }
        // Check magic number
        if ( r.x.bx != 0xAA55 )
        {
            Log ( DEBUG,CommonMessage ( 11 ),r.x.bx );
            Log ( DEBUG,CommonMessage ( 12 ) );
            // This device should not be used
            continue;
        }
        // Check version of extensions
        if ( r.h.ah != 0x30 )
        {
            Log ( DEBUG,CommonMessage ( 13 ),r.h.ah );
        }
        // Check interface support bitmap
        if ( ( r.x.cx & 0x0001 ) != 0x0001 )
        {
            Log ( DEBUG,CommonMessage ( 14 ),r.x.cx );
            Log ( DEBUG,CommonMessage ( 15 ) );
            // This device should not be used
            continue;
        }
        InterfaceSupportBitmap = r.x.cx;

        // Invoke "Get device parameters" from BIOS
        buf.structure_size = sizeof ( struct DeviceParameters );
        Log ( DEBUG,CommonMessage ( 16 ),buf.structure_size );
        dosmemput ( &buf,2,__tb );
        r.h.ah = 0x48;
        r.h.dl = Device + 0x80;
        r.x.ds = __tb >> 4;
        r.x.si = __tb & 0x0F;
        r.x.ss = 0x0000;
        r.x.sp = 0x0000;
        r.x.flags = 0x0000;
        Log ( DEBUG,CommonMessage ( 4 ),0x48 );
        __dpmi_int ( 0x13,&r );
        // Here we should not call Log() because it activates
        // writing to disk what corrupts buffer in DOS memory
        if ( ( r.x.flags & 0x0001 ) == 0x0001 )
        {
            // Calling Log() here is safe because the buffer contents is
            // invalidate because of failed interrupt call
            Log ( DEBUG,CommonMessage ( 6 ),r.h.ah );
            Log ( DEBUG,CommonMessage ( 17 ) );
            // This device should not be used
            continue;
        }
        // We saved requested data buffer to our structure, however we still could not
        // call Log() facility because DPTE is not yet saved
        dosmemget ( __tb,sizeof ( buf ),&buf );
        if ( ( buf.structure_size >= 30 ) && ( ( InterfaceSupportBitmap & 0x0004 ) == 0x0004 ) )
        {
            IOPortBaseAddress = _farpeekw ( _dos_ds,( ( ( unsigned long )buf.dpte_segment )<<4 ) + ( ( unsigned long )buf.dpte_offset ) );
            ATADeviceBit = _farpeekb ( _dos_ds,( ( ( unsigned long )buf.dpte_segment )<<4 ) + ( ( unsigned long )buf.dpte_offset + 4 ) )&0x10;
            // We have successfully saved all important data from DOS conventional memory
            // so calling Log() is safe
            Log ( DEBUG,CommonMessage ( 5 ) );
            Log ( DEBUG,CommonMessage ( 18 ) );
            Log ( DEBUG,CommonMessage ( 19 ),IOPortBaseAddress );
            Log ( DEBUG,CommonMessage ( 20 ),ATADeviceBit );
        }
        else
        {
            Log ( DEBUG,CommonMessage ( 5 ) );
        }
        Log ( DEBUG,CommonMessage ( 21 ),buf.structure_size );
        if ( buf.structure_size < 26 )
        {
            Log ( DEBUG,CommonMessage ( 22 ) );
            // This device should not be used
            continue;
        }
        if ( buf.bytes_in_sector != 512 )
        {
            Log ( DEBUG,CommonMessage ( 23 ),buf.bytes_in_sector );
            Log ( DEBUG,CommonMessage ( 24 ) );
            // This device should not be used
            continue;
        }
        Log ( DEBUG,CommonMessage ( 25 ) );
        for ( Count=0 ; Count<8 ; Count++ )
        {
            Log ( DEBUG,CommonMessage ( 26 ),Count,buf.total_sectors[ Count ] );
        }
        DiskSize = *( unsigned long long* )( buf.total_sectors );
        // Convert DiskSize from number of sectors to megabytes
        DiskSize = DiskSize>>11;
        if ( DiskSize == 0 )
        {
            Log ( DEBUG,CommonMessage ( 27 ) );
            // This device should not be used
            continue;
        }

        // It's time to create new entry in the dynamic list of disk devices
        if ( pDisksArrayCurrent == NULL )
        {
            // This is a first element in a list, nothing was created before
            pDisksArrayCurrent = malloc ( sizeof ( DisksInfo_t ) );
            if ( pDisksArrayCurrent == NULL )
            {
                Log ( FATAL,CommonMessage ( 28 ) );
            }
            // We have allocated the last element in the list
            pDisksArrayCurrent->pNext = NULL;
            // Since it the the only element in the list, update the pointer (passed as a parameter)
            *ppDisksArray = pDisksArrayCurrent;
        }
        else
        {
            // Add another element to a list
            if ( pDisksArrayCurrent->pNext != NULL )
            {
                Log ( FATAL,CommonMessage ( 29 ) );
            }
            pDisksArrayCurrent->pNext = malloc ( sizeof ( DisksInfo_t ) );
            if ( pDisksArrayCurrent->pNext == NULL )
            {
                Log ( FATAL,CommonMessage ( 28 ) );
            }
            // Move to the newly created element, so pDisksArrayCurrent always point to last element
            pDisksArrayCurrent = pDisksArrayCurrent->pNext;
            // No more elements in the list after this one, this is the last one
            pDisksArrayCurrent->pNext = NULL;
        }
        // Set disk device name
        sprintf ( pDisksArrayCurrent->sDiskName, "0x%x", Device + 0x80 );

        if ( DiskSize/1024.0 >= 1 )
        {
            if ( ( ( DiskSize>>10 )/1024.0 ) >= 1 )
            {
                DiskSize = DiskSize>>10;
                if ( ( ( DiskSize>>10 )/1024.0 ) >= 1 )
                {
                    DiskSize = DiskSize>>10;
                    if ( ( ( DiskSize>>10 )/1024.0 ) >= 1 )
                    {
                        DiskSize = DiskSize>>10;
                        // Measuring in exabytes
                        sprintf ( pDisksArrayCurrent->sDiskDescription,"%.1fEb",DiskSize/1024.0 );
                    }
                    else
                    {
                        // Measuring in petabytes
                        sprintf ( pDisksArrayCurrent->sDiskDescription,"%.1fPb",DiskSize/1024.0 );
                    }
                }
                else
                {
                    // Measuring in terabytes
                    sprintf ( pDisksArrayCurrent->sDiskDescription,"%.1fTb",DiskSize/1024.0 );
                }
            }
            else
            {
                // Measuring in gigabytes
                sprintf ( pDisksArrayCurrent->sDiskDescription,"%.1fGb",DiskSize/1024.0 );
            }
        }
        else
        {
            // Measuring in megabytes
            sprintf ( pDisksArrayCurrent->sDiskDescription,"%luMb",( unsigned long )DiskSize );
        }

        if ( buf.structure_size >= 30 )
        {
            Log ( DEBUG,CommonMessage ( 30 ) );
            if (
                ( buf.structure_size == sizeof ( struct DeviceParameters ) ) ||
                ( buf.structure_size == sizeof ( struct DeviceParameters ) - 8 )
               )
            {
                char HostBusType[ 4+1 ];
                char InterfaceType[ 8+1 ];

                Log ( DEBUG,CommonMessage ( 31 ) );
                if ( buf.device_path_key != 0xBEDD )
                {
                    Log ( DEBUG,CommonMessage ( 32 ),buf.device_path_key );
                    Log ( DEBUG,CommonMessage ( 33 ) );
                    // This device could be used
                    continue;
                }
                if (
                    !(
                     (( buf.structure_size = sizeof ( buf ) ) && ( buf.device_path_length == 44 )) ||
                     (( buf.structure_size = sizeof ( buf ) - 8 ) && ( buf.device_path_length == 36 ))
                    )
                   )
                {
                    Log ( DEBUG,CommonMessage ( 34 ),buf.structure_size,buf.device_path_length );
                    Log ( DEBUG,CommonMessage ( 35 ) );
                    // This device could be used
                    continue;
                }
                memcpy ( HostBusType,buf.host_bus_type,4 );
                HostBusType[ 4 ] = 0;
                Log ( DEBUG,CommonMessage ( 36 ),HostBusType );
                for ( Count=(4-1) ; Count>=0 ; Count-- )
                {
                    if ( HostBusType[ Count ] == 0x20 )
                    {
                        HostBusType[ Count ] = 0;
                        Log ( DEBUG,CommonMessage ( 37 ) );
                    }
                    else
                    {
                        break;
                    }
                }
                if ( strcmp ( HostBusType,"" ) != 0 )
                {
                    strcat ( pDisksArrayCurrent->sDiskDescription,"/" );
                    strcat ( pDisksArrayCurrent->sDiskDescription,HostBusType );
                }
                memcpy ( InterfaceType,buf.interface_type,8 );
                InterfaceType[ 8 ] = 0;
                Log ( DEBUG,CommonMessage ( 38 ),InterfaceType );
                for ( Count=(8-1) ; Count>=0 ; Count-- )
                {
                    if ( InterfaceType[ Count ] == 0x20 )
                    {
                        InterfaceType[ Count ] = 0;
                        Log ( DEBUG,CommonMessage ( 37 ) );
                    }
                    else
                    {
                        break;
                    }
                }
                if ( strcmp ( InterfaceType,"" ) != 0 )
                {
                    strcat ( pDisksArrayCurrent->sDiskDescription,"/" );
                    strcat ( pDisksArrayCurrent->sDiskDescription,InterfaceType );
                }
                if (( strcmp ( InterfaceType,"ATA" ) == 0 ) || ( strcmp ( InterfaceType,"SATA" ) == 0 ))
                {
                    Log ( DEBUG,CommonMessage ( 39 ) );
                    Log ( DEBUG,CommonMessage ( 40 ),buf.device_path[ 0 ] );
                    ATADeviceBit = buf.device_path[ 0 ]<<4;
                }
            }
            Log ( DEBUG,CommonMessage ( 41 ),InterfaceSupportBitmap );
            if ( ( InterfaceSupportBitmap & 0x0004 ) == 0x0004 )
            {
                unsigned char ByteFromPort;
                unsigned short IdentifyDevice[ 256 ];
                char ModelNumber[ 40+1 ];

                Log ( DEBUG,CommonMessage ( 42 ) );
                if ( IOPortBaseAddress == 0x01F0 )
                {
                    Log ( DEBUG,CommonMessage ( 43 ) );
                    strcat ( pDisksArrayCurrent->sDiskDescription,CommonMessage ( 44 ) );
                }
                else
                if ( IOPortBaseAddress == 0x0170 )
                {
                    Log ( DEBUG,CommonMessage ( 45 ) );
                    strcat ( pDisksArrayCurrent->sDiskDescription,CommonMessage ( 46 ) );
                }

                if ( ATADeviceBit == 0x00 )
                {
                    Log ( DEBUG,CommonMessage ( 47 ) );
                    strcat ( pDisksArrayCurrent->sDiskDescription,CommonMessage ( 48 ) );
                }
                else
                {
                    Log ( DEBUG,CommonMessage ( 49 ) );
                    strcat ( pDisksArrayCurrent->sDiskDescription,CommonMessage ( 50 ) );
                }
                Log ( DEBUG,CommonMessage ( 51 ),ATADeviceBit );

                // Here we should not call Log() because it activates
                // writing to disk what may corrupt controller state

                // Wait until BUSY bit in status register is reset
                do
                {
                    ByteFromPort = inportb ( IOPortBaseAddress + 0x0007 );
                } while ( ( ByteFromPort & 0x80 ) == 0x80 );

                /*
                // I don't want to use the hell of AT&T assembly syntax which is
                // BTW completely different that Intel syntax. As soon as the MBR
                // is compiled with NASM which supports Intel syntax, let's switch
                // GAS to this mode just to simplify coding process
                __asm__ __volatile__
                (
                    ".intel_syntax\n\t"
                    "cli\n\t"
                    ".att_syntax"
                );
                */

                // Wait until DRDY bit in status register is set
                do
                {
                    ByteFromPort = inportb ( IOPortBaseAddress + 0x0007 );
                } while ( ( ByteFromPort & 0x40 ) != 0x40 );

                // Choose device (master/slave)
                outportb ( IOPortBaseAddress + 0x0006,ATADeviceBit );
                // Send "IDENTIFY DRIVE" command
                outportb ( IOPortBaseAddress + 0x0007,0x0EC );

                // Wait until DRQ bit in status register is set
                do
                {
                    ByteFromPort = inportb ( IOPortBaseAddress + 0x0007 );
                } while ( ( ByteFromPort & 0x08 ) != 0x08 );

                // Get data
                for ( Count=0 ; Count<256 ; Count++ )
                {
                    IdentifyDevice[ Count ] = inportw ( IOPortBaseAddress );
                    IdentifyDevice[ Count ] = ( IdentifyDevice[ Count ] << 8 ) + ( IdentifyDevice[ Count ] >> 8 );
                }
                // Ok, finally we may use Log() again since all
                // controller-related operations have been completed
                Log ( DEBUG,CommonMessage ( 52 ) );

                /*
                __asm__ __volatile__
                (
                    ".intel_syntax\n\t"
                    "sti\n\t"
                    ".att_syntax"
                );
                */

                memcpy ( ModelNumber,&IdentifyDevice[ 27 ],40 );
                ModelNumber[ 40 ] = 0;
                Log ( DEBUG,CommonMessage ( 53 ),ModelNumber );
                for ( Count=(40-1) ; Count>=0 ; Count-- )
                {
                    if ( ModelNumber[ Count ] == 0x20 )
                    {
                        ModelNumber[ Count ] = 0;
                        Log ( DEBUG,CommonMessage ( 37 ) );
                    }
                    else
                    {
                        break;
                    }
                }

                if ( strcmp ( ModelNumber,"" ) != 0 )
                {
                    strcat ( pDisksArrayCurrent->sDiskDescription,"/" );
                    strcat ( pDisksArrayCurrent->sDiskDescription,ModelNumber );
                }
            }
        }
    }

#elif defined ( _WIN32 )

    // Handle of disk device
    HANDLE DriveHandle;
    // Message buffer for error output
    LPVOID lpMsgBuf;
    // Name of disk device
    char DeviceName[ 128 ];
    // Number of device being analyzed
    unsigned int DeviceNumber = 0;
    // Size of a disk device in sectors
    unsigned __int64 Size;
    // Model of disk device retrieved by either of the available  mechanisms (in theory should be 40 symbols or less)
    char Model[ 100 ];
    // This is a dummy variable for DeviceIoControl() function
    DWORD NotUsed;
    // Count used to fill model string and cut trailing spaces
    int Count;
    // Type of the bus used to connect the desired disk device (like ATA or SCSI)
    STORAGE_BUS_TYPE BusType;

    // Generate name of disk device
    sprintf ( DeviceName, "\\\\.\\PhysicalDrive%i", DeviceNumber );
    Log ( DEBUG, CommonMessage ( 54 ), DeviceName );
    // Try to open disk device
    DriveHandle = CreateFile ( DeviceName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
    // Iterate through all disk devices
    while ( DriveHandle != INVALID_HANDLE_VALUE )
    {
        // Stage 1: Add disk device to the list

        // Create new element in the dynamic list
        if ( pDisksArrayCurrent == NULL )
        {
            // This is a first element in a list, nothing was created before
            pDisksArrayCurrent = malloc ( sizeof ( DisksInfo_t ) );
            if ( pDisksArrayCurrent == NULL )
            {
                Log ( FATAL,CommonMessage ( 28 ) );
            }
            // We have allocated the last element in the list
            pDisksArrayCurrent->pNext = NULL;
            // Since it the the only element in the list, update the pointer (passed as a parameter)
            *ppDisksArray = pDisksArrayCurrent;
        }
        else
        {
            // Add another element to a list
            if ( pDisksArrayCurrent->pNext != NULL )
            {
                Log ( FATAL,CommonMessage ( 29 ) );
            }
            pDisksArrayCurrent->pNext = malloc ( sizeof ( DisksInfo_t ) );
            if ( pDisksArrayCurrent->pNext == NULL )
            {
                Log ( FATAL,CommonMessage ( 28 ) );
            }
            // Move to the newly created element, so pDisksArrayCurrent always point to last element
            pDisksArrayCurrent = pDisksArrayCurrent->pNext;
            // No more elements in the list after this one, this is the last one
            pDisksArrayCurrent->pNext = NULL;
        }
        // Set disk device name
        strcpy ( pDisksArrayCurrent->sDiskName, DeviceName );
        // No description yet (will be filled later)
        sprintf ( pDisksArrayCurrent->sDiskDescription, CommonMessage ( 380 ), DeviceNumber );

        // Stage 2: retrieve disk description

        // Initially we know nothing about the disk
        BusType = BusTypeUnknown;
        Size = 0;
        memset ( Model,0,sizeof ( Model ) );

        // Try to get geometry info using basic mechanism
        {
            DISK_GEOMETRY geometry;

            if ( DeviceIoControl ( DriveHandle,
                                   IOCTL_DISK_GET_DRIVE_GEOMETRY,
                                   NULL,
                                   0,
                                   &geometry,
                                   sizeof ( geometry ),
                                   &NotUsed,
                                   NULL ) )
            {
                Size  = geometry.Cylinders.QuadPart;
                Size *= geometry.TracksPerCylinder;
                Size *= geometry.SectorsPerTrack;
                // Size *= geometry.BytesPerSector;
            }
        }

        // Try to get model number using generic mechanism
        // (1st attempt of 3) - this is the only way to get model of USB flash drives
        {
            STORAGE_PROPERTY_QUERY Query;
            // The size of destination structure is unknown (device dependant), hope 16K is enough
            char TempBuffer[ 16*1024 ];

            memset ( ( void* )&Query,0,sizeof ( Query ) );
            Query.PropertyId = StorageDeviceProperty;
            Query.QueryType = PropertyStandardQuery;
            memset ( TempBuffer,0,sizeof ( TempBuffer ) );
            if ( DeviceIoControl ( DriveHandle,
                                   IOCTL_STORAGE_QUERY_PROPERTY,
                                   &Query,
                                   sizeof ( Query ),
                                   &TempBuffer,
                                   sizeof ( TempBuffer ),
                                   &NotUsed,
                                   NULL ) )
            {
                // Save bus type for future analysis
                BusType = ( ( STORAGE_DEVICE_DESCRIPTOR* )&TempBuffer )->BusType;
                // If vendor field is present, copy it into the model string
                if ( ( ( STORAGE_DEVICE_DESCRIPTOR* )&TempBuffer )->VendorIdOffset != 0 )
                {
                    strncpy ( Model,&TempBuffer [ ( ( STORAGE_DEVICE_DESCRIPTOR* )&TempBuffer )->VendorIdOffset ],sizeof ( Model ) - 1 );
                    // Strip trailing spaces from vendor id
                    for ( Count=sizeof ( Model )-1 ; Count>=0 ; Count-- )
                    {
                        if (( Model[ Count ] != 0x20 ) && ( Model[ Count ] != 0x00 ))
                        {
                            break;
                        }
                        Model[ Count ] = 0;
                    }
                }
                // If product field is present, add it into the model string
                if ( ( ( STORAGE_DEVICE_DESCRIPTOR* )&TempBuffer )->ProductIdOffset != 0 )
                {
                    // Add space symbol to separate vendor and product strings
                    if ( strcmp ( Model,"" ) != 0 )
                    {
                        strncat ( Model," ",sizeof ( Model ) - 1 - strlen ( Model ) );
                    }
                    strncat ( Model,&TempBuffer [ ( ( STORAGE_DEVICE_DESCRIPTOR* )&TempBuffer )->ProductIdOffset ],sizeof ( Model ) - 1 - strlen ( Model ) );
                    // Strip trailing spaces from product id
                    for ( Count=sizeof ( Model )-1 ; Count>=0 ; Count-- )
                    {
                        if (( Model[ Count ] != 0x20 ) && ( Model[ Count ] != 0x00 ))
                        {
                            break;
                        }
                        Model[ Count ] = 0;
                    }
                }
            }
        }

        // Try to get model number using SMART mechanism
        // (2nd attempt of 3)
        if ( ( strcmp ( Model,"" ) == 0 ) || ( Size == 0 ) )
        {
            SENDCMDINPARAMS InParameters;
            IDEREGS Registers;
            unsigned char TempBuffer[ sizeof ( SENDCMDOUTPARAMS ) - 1 + 512 ];

            memset ( &Registers,0,sizeof ( Registers ) );
            // 0xEC - IDE_ATA_IDENTIFY_DEVICE - Returns ID sector for ATA
            Registers.bCommandReg = 0xEC;
            Registers.bCylLowReg = 0;
            Registers.bCylHighReg = 0;
            Registers.bSectorCountReg = 1;
            memset ( &InParameters,0,sizeof ( InParameters ) );
            InParameters.irDriveRegs = Registers;
            InParameters.irDriveRegs.bDriveHeadReg = 0xA0 | ( ( DeviceNumber & 1 ) << 4 );
            InParameters.bDriveNumber = DeviceNumber;
            InParameters.cBufferSize = 512;
            memset ( &TempBuffer,0,sizeof ( TempBuffer ) );
            if ( DeviceIoControl ( DriveHandle,
                                   SMART_RCV_DRIVE_DATA,
                                   &InParameters,
                                   sizeof ( SENDCMDINPARAMS ) - 1,
                                   TempBuffer,
                                   sizeof ( SENDCMDOUTPARAMS ) - 1 + 512,
                                   &NotUsed,
                                   NULL ) )
            {
                if ( !( ( ( SENDCMDOUTPARAMS* )TempBuffer )->DriverStatus.bDriverError ) )
                {
                    // Copy model string if it is not yet done by previous attempt
                    if ( strcmp ( Model,"" ) == 0 )
                    {
                        memset ( Model,0,sizeof ( Model ) );
                        // Swap odd and even bytes
                        for ( Count=0 ; Count<40 ; Count++ )
                        {
                            Model[ Count ] = *( ( ( SENDCMDOUTPARAMS* )TempBuffer )->bBuffer+27*2 + ( Count/2 )*2 + ( !( Count%2 ) ) );
                        }
                        // Strip trailing spaces from model number
                        for ( Count=sizeof ( Model )-1 ; Count>=0 ; Count-- )
                        {
                            if (( Model[ Count ] != 0x20 ) && ( Model[ Count ] != 0x00 ))
                            {
                                break;
                            }
                            Model[ Count ] = 0;
                        }
                    }
                    // Get disk size if it is not yet done by previous attempt
                    if ( Size == 0 )
                    {
                        // Check if drive supports LBA addressing and 48-bit addressing
                        if (
                            ( ( ( unsigned short* )&( ( ( SENDCMDOUTPARAMS* )TempBuffer )->bBuffer ) )[ 49 ] & 0x0200 ) &&
                            ( ( ( ( unsigned short* )&( ( ( SENDCMDOUTPARAMS* )TempBuffer )->bBuffer ) )[ 83 ] & 0xc000 ) == 0x4000 ) &&
                            ( ( ( unsigned short* )&( ( ( SENDCMDOUTPARAMS* )TempBuffer )->bBuffer ) )[ 83 ] & 0x0400 )
                           )
                        {
                            Size  = ( unsigned __int64 )( ( ( unsigned short* )&( ( ( SENDCMDOUTPARAMS* )TempBuffer )->bBuffer ) )[ 103 ] ) << 48;
                            Size |= ( unsigned __int64 )( ( ( unsigned short* )&( ( ( SENDCMDOUTPARAMS* )TempBuffer )->bBuffer ) )[ 102 ] ) << 32;
                            Size |= ( unsigned __int64 )( ( ( unsigned short* )&( ( ( SENDCMDOUTPARAMS* )TempBuffer )->bBuffer ) )[ 101 ] ) << 16;
                            Size |= ( unsigned __int64 )( ( ( unsigned short* )&( ( ( SENDCMDOUTPARAMS* )TempBuffer )->bBuffer ) )[ 100 ] ) << 0;
                        }
                    }
                }
            }
        }

        // Try to get model number using SCSI backdoor
        // (3rd attempt of 3) - this is the only way to get model of SCSI disks
        if ( strcmp ( Model,"" ) == 0 )
        {
            char TempBuffer[ sizeof ( SRB_IO_CONTROL ) + sizeof ( SENDCMDOUTPARAMS ) + IDENTIFY_BUFFER_SIZE ];
            IDSECTOR *pIdentify;

            memset ( TempBuffer,0,sizeof ( TempBuffer ) );
            ( ( SRB_IO_CONTROL* )TempBuffer )->HeaderLength = sizeof ( SRB_IO_CONTROL );
            ( ( SRB_IO_CONTROL* )TempBuffer )->Timeout = 10000;
            ( ( SRB_IO_CONTROL* )TempBuffer )->Length = sizeof ( SENDCMDOUTPARAMS ) + IDENTIFY_BUFFER_SIZE;
            ( ( SRB_IO_CONTROL* )TempBuffer )->ControlCode = IOCTL_SCSI_MINIPORT_IDENTIFY;
            strncpy ( ( char* )( ( SRB_IO_CONTROL* )TempBuffer )->Signature,"SCSIDISK",8 );
            // 0xEC - IDE_ATA_IDENTIFY_DEVICE - Returns ID sector for ATA
            ( ( SENDCMDINPARAMS* )( TempBuffer + sizeof ( SRB_IO_CONTROL ) ) )->irDriveRegs.bCommandReg = 0xEC;
            ( ( SENDCMDINPARAMS* )( TempBuffer + sizeof ( SRB_IO_CONTROL ) ) )->bDriveNumber = DeviceNumber & 1;
            if ( DeviceIoControl ( DriveHandle,
                                   IOCTL_SCSI_MINIPORT,
                                   TempBuffer,
                                   sizeof ( SRB_IO_CONTROL ) + sizeof ( SENDCMDINPARAMS ) - 1,
                                   TempBuffer,
                                   sizeof ( SRB_IO_CONTROL ) + sizeof ( SENDCMDOUTPARAMS ) + IDENTIFY_BUFFER_SIZE,
                                   &NotUsed,
                                   NULL ) )
            {
                pIdentify = ( IDSECTOR* )( ( ( SENDCMDOUTPARAMS* )( TempBuffer + sizeof ( SRB_IO_CONTROL ) ) )->bBuffer );
                memset ( Model,0,sizeof ( Model ) );
                // Swap odd and even bytes
                for ( Count=0 ; Count<sizeof ( pIdentify->sModel ) ; Count++ )
                {
                    Model[ Count ] = pIdentify->sModel[ ( Count/2 )*2 + ( !( Count%2 ) ) ];
                }
                // Strip trailing spaces from model number
                for ( Count=sizeof ( Model )-1 ; Count>=0 ; Count-- )
                {
                    if (( Model[ Count ] != 0x20 ) && ( Model[ Count ] != 0x00 ))
                    {
                        break;
                    }
                    Model[ Count ] = 0;
                }
            }
        }

        // Add size of the drive to the result buffer in human-readable form
        if ( Size != 0 )
        {
            // Convert size from number of sectors to kilobytes
            Size = Size>>1;
            if ( ( Size>>10 ) == 0 )
            {
                sprintf ( pDisksArrayCurrent->sDiskDescription + strlen ( pDisksArrayCurrent->sDiskDescription ),"%1#I64ub",Size*512 );
            }
            else
            {
                // Convert size from kilobytes to megabytes
                Size = Size>>10;
                if ( Size/1024.0 >= 1 )
                {
                    if ( ( ( Size>>10 )/1024.0 ) >= 1 )
                    {
                        Size = Size>>10;
                        if ( ( ( Size>>10 )/1024.0 ) >= 1 )
                        {
                            Size = Size>>10;
                            if ( ( ( Size>>10 )/1024.0 ) >= 1 )
                            {
                                Size = Size>>10;
                                // Measuring in exabytes
                                sprintf ( pDisksArrayCurrent->sDiskDescription + strlen ( pDisksArrayCurrent->sDiskDescription ),"%.1fEb",Size/1024.0 );
                            }
                            else
                            {
                                // Measuring in petabytes
                                sprintf ( pDisksArrayCurrent->sDiskDescription + strlen ( pDisksArrayCurrent->sDiskDescription ),"%.1fPb",Size/1024.0 );
                            }
                        }
                        else
                        {
                            // Measuring in terabytes
                            sprintf ( pDisksArrayCurrent->sDiskDescription + strlen ( pDisksArrayCurrent->sDiskDescription ),"%.1fTb",Size/1024.0 );
                        }
                    }
                    else
                    {
                        // Measuring in gigabytes
                        sprintf ( pDisksArrayCurrent->sDiskDescription + strlen ( pDisksArrayCurrent->sDiskDescription ),"%.1fGb",Size/1024.0 );
                    }
                }
                else
                {
                    // Measuring in megabytes
                    sprintf ( pDisksArrayCurrent->sDiskDescription + strlen ( pDisksArrayCurrent->sDiskDescription ),"%luMb",( unsigned long )Size );
                }
            }
        }
        // Add bus type to the result buffer
        switch ( BusType )
        {
            case BusTypeUnknown: // 0x00
            break;

            case BusTypeScsi: // 0x01
            {
                if ( strcmp ( pDisksArrayCurrent->sDiskDescription,"" ) != 0 )
                {
                    strcat ( pDisksArrayCurrent->sDiskDescription,"/" );
                }
                strcat ( pDisksArrayCurrent->sDiskDescription,"SCSI" );
            }
            break;

            case BusTypeAtapi: // 0x02
            {
                if ( strcmp ( pDisksArrayCurrent->sDiskDescription,"" ) != 0 )
                {
                    strcat ( pDisksArrayCurrent->sDiskDescription,"/" );
                }
                strcat ( pDisksArrayCurrent->sDiskDescription,"ATAPI" );
            }
            break;

            case BusTypeAta: // 0x03 ATA/IDE
            {
                if ( strcmp ( pDisksArrayCurrent->sDiskDescription,"" ) != 0 )
                {
                    strcat ( pDisksArrayCurrent->sDiskDescription,"/" );
                }
                strcat ( pDisksArrayCurrent->sDiskDescription,"ATA" );
            }
            break;

            case BusType1394: // 0x04
            {
                if ( strcmp ( pDisksArrayCurrent->sDiskDescription,"" ) != 0 )
                {
                    strcat ( pDisksArrayCurrent->sDiskDescription,"/" );
                }
                strcat ( pDisksArrayCurrent->sDiskDescription,"1394" );
            }
            break;

            case BusTypeSsa: // 0x05
            {
                if ( strcmp ( pDisksArrayCurrent->sDiskDescription,"" ) != 0 )
                {
                    strcat ( pDisksArrayCurrent->sDiskDescription,"/" );
                }
                strcat ( pDisksArrayCurrent->sDiskDescription,"SSA" );
            }
            break;

            case BusTypeFibre: // 0x06
            {
                if ( strcmp ( pDisksArrayCurrent->sDiskDescription,"" ) != 0 )
                {
                    strcat ( pDisksArrayCurrent->sDiskDescription,"/" );
                }
                strcat ( pDisksArrayCurrent->sDiskDescription,"FIBRE" );
            }
            break;

            case BusTypeUsb: // 0x07
            {
                if ( strcmp ( pDisksArrayCurrent->sDiskDescription,"" ) != 0 )
                {
                    strcat ( pDisksArrayCurrent->sDiskDescription,"/" );
                }
                strcat ( pDisksArrayCurrent->sDiskDescription,"USB" );
            }
            break;

            case BusTypeRAID: // 0x08
            {
                if ( strcmp ( pDisksArrayCurrent->sDiskDescription,"" ) != 0 )
                {
                    strcat ( pDisksArrayCurrent->sDiskDescription,"/" );
                }
                strcat ( pDisksArrayCurrent->sDiskDescription,"RAID" );
            }
            break;

            case 0x09: // BusTypeiSCSI
            {
                if ( strcmp ( pDisksArrayCurrent->sDiskDescription,"" ) != 0 )
                {
                    strcat ( pDisksArrayCurrent->sDiskDescription,"/" );
                }
                strcat ( pDisksArrayCurrent->sDiskDescription,"iSCSI" );
            }
            break;

            case 0x0A: // BusTypeSATA
            {
                if ( strcmp ( pDisksArrayCurrent->sDiskDescription,"" ) != 0 )
                {
                    strcat ( pDisksArrayCurrent->sDiskDescription,"/" );
                }
                strcat ( pDisksArrayCurrent->sDiskDescription,"SATA" );
            }
            break;

            case 0x0B: // BusTypeSAS
            {
                if ( strcmp ( pDisksArrayCurrent->sDiskDescription,"" ) != 0 )
                {
                    strcat ( pDisksArrayCurrent->sDiskDescription,"/" );
                }
                strcat ( pDisksArrayCurrent->sDiskDescription,"SAS" );
            }
            break;

            case BusTypeMaxReserved:
            break;

            default:
            break;
        }
        // Add model string to the result buffer
        if ( strcmp ( Model,"" ) != 0 )
        {
            if ( strcmp ( pDisksArrayCurrent->sDiskDescription,"" ) != 0 )
            {
                strcat ( pDisksArrayCurrent->sDiskDescription,"/" );
            }
            strcat ( pDisksArrayCurrent->sDiskDescription,Model );
        }

        // Close disk device
        if ( CloseHandle ( DriveHandle ) == 0 )
        {
            // Prepare a string explaining what has happened
            FormatMessage ( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                            NULL,
                            GetLastError (),
                            MAKELANGID ( LANG_NEUTRAL,SUBLANG_DEFAULT ),
                            ( LPTSTR )&lpMsgBuf,
                            0,
                            NULL );
            Log ( DEBUG, ( LPCTSTR )lpMsgBuf );
            LocalFree ( lpMsgBuf );
            // Unable to close disk device, this is not a fatal error
            Log ( DEBUG,CommonMessage ( 55 ),DeviceName );
        }
        // Try next disk device
        DeviceNumber++;
        // Generate name of disk device
        sprintf ( DeviceName, "\\\\.\\PhysicalDrive%i", DeviceNumber );
        Log ( DEBUG, CommonMessage ( 54 ), DeviceName );
        // Try to open disk device
        DriveHandle = CreateFile ( DeviceName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
    }

#elif defined ( __unix__ )

#if defined ( __linux__ )

    // Buffer to store data used in matching files by pattern
    glob_t GlobBuffer;
    // Counter used to iterate found entries returned by glob()
    int iCount1;
    // Counter used to strip leading spaces, CR and LF symbols from the model/vendor description
    int iCount2;
    // Name of the found file + path/filename of model or vendor
    char sFileName[ 128 ];
    // Vendor of the disk device
    char sVendor[ 128 ];
    // Model of the disk device
    char sModel[ 128 ];
    // Handle of the file in virtual (/proc or /sys) filesystem describing vendor/model
    int FileHandle;

    // First stage: Check ATA/IDE devices (usually hard disks and CD-ROM drives)
    // Seems to be always present in a system if /proc is mounted (at least from 2.2.16 kernel in RedHat 7)
    memset ( &GlobBuffer, 0, sizeof ( glob_t ) );
    switch ( glob ( "/proc/ide/hd?", GLOB_MARK, NULL, &GlobBuffer ) )
    {
        case 0:
        {
            // In case of success
            Log ( DEBUG, CommonMessage ( 56 ), "/proc" );
            // This cycle goes through all found filenames
            for ( iCount1=0 ; iCount1<GlobBuffer.gl_pathc ; iCount1++ )
            {
                // Get a name of the file
                strcpy ( sFileName, GlobBuffer.gl_pathv[ iCount1 ] );
                Log ( DEBUG, CommonMessage ( 54 ), sFileName );
                // Slash is already appended because of GLOB_MARK, just add a filename with model description
                strcat ( sFileName, "model" );
                // Initialize model string
                memset ( sModel, 0, sizeof ( sModel ) );
                // Open the file with model
                FileHandle = open ( sFileName, O_RDONLY );
                if ( FileHandle == -1 )
                {
                    // Unable to open model description, not a critical error
                    Log ( DEBUG, CommonMessage ( 57 ), errno, strerror ( errno ) );
                }
                else
                {
                    // Try to read model string
                    if ( read ( FileHandle, &sModel, sizeof ( sModel ) - 1 ) == -1 )
                    {
                        // Unable to read model description, not a critical error
                        Log ( DEBUG, CommonMessage ( 58 ), errno, strerror ( errno ) );
                    }
                    else
                    {
                        // Create new element in the dynamic list
                        if ( pDisksArrayCurrent == NULL )
                        {
                            // This is a first element in a list, nothing was created before
                            pDisksArrayCurrent = malloc ( sizeof ( DisksInfo_t ) );
                            if ( pDisksArrayCurrent == NULL )
                            {
                                Log ( FATAL,CommonMessage ( 28 ) );
                            }
                            // We have allocated the last element in the list
                            pDisksArrayCurrent->pNext = NULL;
                            // Since it the the only element in the list, update the pointer (passed as a parameter)
                            *ppDisksArray = pDisksArrayCurrent;
                        }
                        else
                        {
                            // Add another element to a list
                            if ( pDisksArrayCurrent->pNext != NULL )
                            {
                                Log ( FATAL,CommonMessage ( 29 ) );
                            }
                            pDisksArrayCurrent->pNext = malloc ( sizeof ( DisksInfo_t ) );
                            if ( pDisksArrayCurrent->pNext == NULL )
                            {
                                Log ( FATAL,CommonMessage ( 28 ) );
                            }
                            // Move to the newly created element, so pDisksArrayCurrent always point to last element
                            pDisksArrayCurrent = pDisksArrayCurrent->pNext;
                            // No more elements in the list after this one, this is the last one
                            pDisksArrayCurrent->pNext = NULL;
                        }
                        // 12 is an offset of symbol in a found filename which defines a found disk device
                        // (a suffix after standard prefix "hd" for ATA disks
                        sprintf ( pDisksArrayCurrent->sDiskName,"/dev/hd%c", GlobBuffer.gl_pathv [ iCount1 ][ 12 ] );
                        // Trim trailing spaces and CR/LF symbols
                        for ( iCount2=strlen ( sModel ) - 1 ; iCount2>=0 ; iCount2-- )
                        {
                            // If either CR, LF or space has been found
                            if (( sModel[ iCount2 ] == ' ' ) || ( sModel[ iCount2 ] == 13 ) || ( sModel[ iCount2 ] == 10 ))
                            {
                                // Trim it
                                sModel[ iCount2 ] = 0;
                            }
                            else
                            {
                                break;
                            }
                        }
                        // Copy model name even if it is an empty string
                        strcpy ( pDisksArrayCurrent->sDiskDescription, sModel );
                    }
                    // File handle will no longer be used for this disk device
                    close ( FileHandle );
                }
            }
        }
        break;

        case GLOB_NOSPACE:
        {
            // Memory allocation error, critical error
            Log ( FATAL, CommonMessage ( 59 ), errno, strerror ( errno ) );
        }
        break;

        case GLOB_ABORTED:
        {
            // Most probably no permissions, not a critical error
            Log ( DEBUG, CommonMessage ( 60 ), errno, strerror ( errno ) );
        }
        break;

        case GLOB_NOMATCH:
        {
            // No devices, not a critical error
            Log ( DEBUG, CommonMessage ( 61 ), errno, strerror ( errno ) );
        }
        break;

        default:
        {
            // Unknown error, this should never happen, but skip it
            Log ( DEBUG, CommonMessage ( 62 ), errno, strerror ( errno ) );
        }
        break;
    }
    globfree ( &GlobBuffer );

    // Second stage: Check SCSI device (including flash drives and SATA hard disks)
    // Usually for 2.6 kernels with sysfs mounted into /sys
    memset ( &GlobBuffer, 0, sizeof ( glob_t ) );
    switch ( glob ( "/sys/block/sd?", GLOB_MARK, NULL, &GlobBuffer ) )
    {
        case 0:
        {
            // In case of success
            Log ( DEBUG, CommonMessage ( 56 ), "/sys" );
            // This cycle goes through all found filenames
            for ( iCount1=0 ; iCount1<GlobBuffer.gl_pathc ; iCount1++ )
            {
                // Get a name of the file
                strcpy ( sFileName, GlobBuffer.gl_pathv[ iCount1 ] );
                Log ( DEBUG, CommonMessage ( 54 ), sFileName );
                // Slash is already appended because of GLOB_MARK, just add a filename with vendor description
                strcat ( sFileName, "device/vendor" );
                // Initialize vendor string
                memset ( sVendor, 0, sizeof ( sVendor ) );
                // Open the file with vendor
                FileHandle = open ( sFileName, O_RDONLY );
                if ( FileHandle == -1 )
                {
                    // Unable to open vendor description, not a critical error
                    Log ( DEBUG, CommonMessage ( 63 ), errno, strerror ( errno ) );
                }
                else
                {
                    // Try to read vendor string
                    if ( read ( FileHandle, &sVendor, sizeof ( sVendor ) - 1 ) == -1 )
                    {
                        // Unable to read vendor description, not a critical error
                        Log ( DEBUG, CommonMessage ( 64 ), errno, strerror ( errno ) );
                        strcpy ( sVendor, "" );
                    }
                    else
                    {
                        // Trim trailing spaces and CR/LF symbols
                        for ( iCount2=strlen ( sVendor ) - 1 ; iCount2>=0 ; iCount2-- )
                        {
                            // If either CR, LF or space has been found
                            if (( sVendor[ iCount2 ] == ' ' ) || ( sVendor[ iCount2 ] == 13 ) || ( sVendor[ iCount2 ] == 10 ))
                            {
                                // Trim it
                                sVendor[ iCount2 ] = 0;
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                    // File handle will no longer be used for this disk device
                    close ( FileHandle );
                }

                // Get a name of the file
                strcpy ( sFileName, GlobBuffer.gl_pathv[ iCount1 ] );
                // Slash is already appended because of GLOB_MARK, just add a filename with model description
                strcat ( sFileName, "device/model" );
                // Initialize model string
                memset ( sModel, 0, sizeof ( sModel ) );
                // Open the file with model
                FileHandle = open ( sFileName, O_RDONLY );
                if ( FileHandle == -1 )
                {
                    // Unable to open with model description, not a critical error
                    Log ( DEBUG, CommonMessage ( 57 ), errno, strerror ( errno ) );
                }
                else
                {
                    // Try to read model string
                    if ( read ( FileHandle, &sModel, sizeof ( sModel ) - 1 ) == -1 )
                    {
                        // Unable to read model description, not a critical error
                        Log ( DEBUG, CommonMessage ( 58 ), errno, strerror ( errno ) );
                        strcpy ( sModel, "" );
                    }
                    else
                    {
                        // Trim trailing spaces and CR/LF symbols
                        for ( iCount2=strlen ( sModel ) - 1 ; iCount2>=0 ; iCount2-- )
                        {
                            // If either CR, LF or space has been found
                            if (( sModel[ iCount2 ] == ' ' ) || ( sModel[ iCount2 ] == 13 ) || ( sModel[ iCount2 ] == 10 ))
                            {
                                // Trim it
                                sModel[ iCount2 ] = 0;
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                    // File handle will no longer be used for this disk device
                    close ( FileHandle );
                }

                // Create new element in the dynamic list
                if ( pDisksArrayCurrent == NULL )
                {
                    // This is a first element in a list, nothing was created before
                    pDisksArrayCurrent = malloc ( sizeof ( DisksInfo_t ) );
                    if ( pDisksArrayCurrent == NULL )
                    {
                        Log ( FATAL,CommonMessage ( 28 ) );
                    }
                    // We have allocated the last element in the list
                    pDisksArrayCurrent->pNext = NULL;
                    // Since it the the only element in the list, update the pointer (passed as a parameter)
                    *ppDisksArray = pDisksArrayCurrent;
                }
                else
                {
                    // Add another element to a list
                    if ( pDisksArrayCurrent->pNext != NULL )
                    {
                        Log ( FATAL,CommonMessage ( 29 ) );
                    }
                    pDisksArrayCurrent->pNext = malloc ( sizeof ( DisksInfo_t ) );
                    if ( pDisksArrayCurrent->pNext == NULL )
                    {
                        Log ( FATAL,CommonMessage ( 28 ) );
                    }
                    // Move to the newly created element, so pDisksArrayCurrent always point to last element
                    pDisksArrayCurrent = pDisksArrayCurrent->pNext;
                    // No more elements in the list after this one, this is the last one
                    pDisksArrayCurrent->pNext = NULL;
                }
                // 13 is an offset of symbol in a found filename which defines a found disk device
                // (a suffix after standard prefix "sd" for SCSI/SATA disks
                sprintf ( pDisksArrayCurrent->sDiskName,"/dev/sd%c", GlobBuffer.gl_pathv [ iCount1 ][ 13 ] );
                // Combine vendor and model in description
                strcpy ( pDisksArrayCurrent->sDiskDescription, "" );
                if ( strcmp ( sVendor, "" ) != 0 )
                {
                    strcpy ( pDisksArrayCurrent->sDiskDescription, sVendor );
                    strcat ( pDisksArrayCurrent->sDiskDescription, " " );
                }
                strcat ( pDisksArrayCurrent->sDiskDescription, sModel );
            }
        }
        break;

        case GLOB_NOSPACE:
        {
            // Memory allocation error, critical error
            Log ( FATAL, CommonMessage ( 59 ), errno, strerror ( errno ) );
        }
        break;

        case GLOB_ABORTED:
        {
            // Most probably no permissions, not a critical error
            Log ( DEBUG, CommonMessage ( 60 ), errno, strerror ( errno ) );
        }
        break;

        case GLOB_NOMATCH:
        {
            // No devices, not a critical error
            Log ( DEBUG, CommonMessage ( 61 ), errno, strerror ( errno ) );
        }
        break;

        default:
        {
            // Unknown error, this should never happen, but skip it
            Log ( DEBUG, CommonMessage ( 62 ), errno, strerror ( errno ) );
        }
        break;
    }
    globfree ( &GlobBuffer );

#elif defined ( __BSD__ ) || defined ( __386BSD__ ) || defined ( __FreeBSD__ ) || defined ( __NetBSD__ ) || defined ( __OpenBSD__ ) || defined ( __DragonFly__ )

    // Handle used to create temporary file
    int FileHandle1;
    // Handle used to read temporary file
    FILE *FileHandle2;
    // Template of the temporary file name + path
    char sTemplate[ 256 ];
    // Directory used to store temporary files
    char *sTempDirectory;
    // Command for extracting a list of disk devices
    // Pipeline contais sed to cut prefix from sysctl output and to replace commas with spaces
#if defined ( __NetBSD__ ) || defined ( __OpenBSD__ )
    char sSysCtlCommand[ 256 ] = "sysctl hw.disknames 2>/dev/null | sed \"s/^.*[=|:] *//;s/,/ /\" > ";
#else
    char sSysCtlCommand[ 256 ] = "sysctl kern.disks   2>/dev/null | sed \"s/^.*[=|:] *//;s/,/ /\" > ";
#endif /* __NetBSD__ or __OpenBSD__ */
    // Command for getting vendor and model for ATA disk devices
    char sAtaControlCommand[ 256 ] = "atacontrol    list 2>/dev/null | grep %s | sed \"s/^.*<//;s/>.*//;\" > ";
    // Command for getting vendor and model for SCSI disk devices (also includes flash cards and possibly SATA disks)
    char sCamControlCommand[ 256 ] = "camcontrol devlist 2>/dev/null | grep %s | sed \"s/^.*<//;s/>.*//;\" >> ";
    // Buffer for file reading operations
    char Buffer[ 1024 ];
    // Error code of the invoked command
    int ErrorCode;
    // Counter to strip trailing whitespaces and CR/LF symbold from disk description
    int iCount;

    // Get temporary directory if it is defined in user's environment
    sTempDirectory = getenv ( "TMPDIR" );
    if ( sTempDirectory == NULL )
    {
        // No user-specific temporary directory, use the default
        strcpy ( sTemplate, "/tmp/" );
    }
    else
    {
        // TMPDIR environment variable is defined
        if ( strlen ( sTempDirectory ) == 0 )
        {
            // Fallback to default if it is empty
            strcpy ( sTemplate, "/tmp/" );
        }
        else
        {
            // Use defined environment variable
            strcpy ( sTemplate, sTempDirectory );
            // And append a trailing slash if it does not exist
            if ( sTemplate[ strlen ( sTemplate ) - 1 ] != '/' )
            {
                strcat ( sTemplate, "/" );
            }
        }
    }
    // Append a filename-template to the path pointing to temporary directory
    strcat ( sTemplate, "mbldrXXXXXX" );

    // Create temporary file (sTemplate will be updated to reflect actual filename)
    FileHandle1 = mkstemp ( sTemplate );
    if ( FileHandle1 == -1 )
    {
        // Unable to create temporary filename, this is not a critical error, filesystem may be
        // mounted in read-only mode (for recovery reasons or if LiveCD is used)
        Log ( DEBUG, CommonMessage ( 65 ), errno, strerror ( errno ) );
        // There is no reason to continue disk autodetection
        return;
    }
    // Close the temporary file before invoking "sysctl" external program, since it will write there
    if ( close ( FileHandle1 ) == -1 )
    {
        // Unable to close temporary filename, this is not a critical error
        Log ( DEBUG, CommonMessage ( 66 ), errno, strerror ( errno ) );
        // There is no reason to continue disk autodetection
        return;
    }

    // Stage 1: get a list of disk devices

    // Append a temporary filename to redirect stdout of sysctl command
    strcat ( sSysCtlCommand,sTemplate );
    // Execute sysctl command
    ErrorCode = system ( sSysCtlCommand );
    Log ( DEBUG, CommonMessage ( 67 ), sSysCtlCommand, ErrorCode );
    if ( ErrorCode == -1 )
    {
        // We can not get the list of disks, it is not a critical error
        Log ( DEBUG, CommonMessage ( 68 ) );
        // There is no reason to continue disk autodetection
        return;
    }
    // Read the contents of temporary file in a text mode
    FileHandle2 = fopen ( sTemplate,"rt" );
    if ( FileHandle2 == NULL )
    {
        // Unable to open temporary file, this is not a critical error
        Log ( DEBUG, CommonMessage ( 69 ), errno, strerror ( errno ) );
        // There is no reason to continue disk autodetection
        return;
    }
    // Read device names
    while ( fscanf ( FileHandle2, "%s", Buffer ) == 1 )
    {
        // Create new element in the dynamic list
        if ( pDisksArrayCurrent == NULL )
        {
            // This is a first element in a list, nothing was created before
            pDisksArrayCurrent = malloc ( sizeof ( DisksInfo_t ) );
            if ( pDisksArrayCurrent == NULL )
            {
                Log ( FATAL,CommonMessage ( 28 ) );
            }
            // We have allocated the last element in the list
            pDisksArrayCurrent->pNext = NULL;
            // Since it the the only element in the list, update the pointer (passed as a parameter)
            *ppDisksArray = pDisksArrayCurrent;
        }
        else
        {
            // Add another element to a list
            if ( pDisksArrayCurrent->pNext != NULL )
            {
                Log ( FATAL,CommonMessage ( 29 ) );
            }
            pDisksArrayCurrent->pNext = malloc ( sizeof ( DisksInfo_t ) );
            if ( pDisksArrayCurrent->pNext == NULL )
            {
                Log ( FATAL,CommonMessage ( 28 ) );
            }
            // Move to the newly created element, so pDisksArrayCurrent always point to last element
            pDisksArrayCurrent = pDisksArrayCurrent->pNext;
            // No more elements in the list after this one, this is the last one
            pDisksArrayCurrent->pNext = NULL;
        }
        // Set disk device name (currently without "/dev/" prefix)
        strcpy ( pDisksArrayCurrent->sDiskName, Buffer );
        // No description yet (will be filled later)
        strcpy ( pDisksArrayCurrent->sDiskDescription, "" );
    }
    // Close file
    if ( fclose ( FileHandle2 ) != 0 )
    {
        // Error closing file, just skip that
        Log ( DEBUG, CommonMessage ( 66 ), errno, strerror ( errno ) );
    }

    // Stage 2: retrieve model/vendor for each disk device

    // Go through the list of found disk devices
    pDisksArrayCurrent = *ppDisksArray;
    while ( pDisksArrayCurrent != NULL )
    {
        Log ( DEBUG, CommonMessage ( 70 ), pDisksArrayCurrent->sDiskName );
        // Form the atacontrol command
        sprintf ( Buffer, sAtaControlCommand, pDisksArrayCurrent->sDiskName );
        // Append a temporary filename to allow stdout redirection (> - redirect overwriting old contents)
        strcat ( Buffer, sTemplate );
        // Execute atacontrol command ignoring error code
        // Even if atacontrol could not be executed the contents of the temporary file will be cleaned up
        system ( Buffer );
        // Form the camcontrol command
        sprintf ( Buffer, sCamControlCommand, pDisksArrayCurrent->sDiskName );
        // Append a temporary filename to allow stdout redirection (>> - redirect appending to existing contents)
        strcat ( Buffer, sTemplate );
        // Execute atacontrol command ignoring error code
        system ( Buffer );
        // Only one of two system() calls should update the temporary file, it seems to be quite odd if both
        // of them will return some text

        // Try to copy description (model/vendor)
        Log ( DEBUG, CommonMessage ( 71 ), pDisksArrayCurrent->sDiskName );
        // Initialize disk description
        memset ( pDisksArrayCurrent->sDiskDescription, 0, sizeof ( pDisksArrayCurrent->sDiskDescription ) );
        // Open the file with description
        FileHandle1 = open ( sTemplate, O_RDONLY );
        if ( FileHandle1 == -1 )
        {
            // Unable to open device description, not a critical error
            Log ( DEBUG, CommonMessage ( 72 ), errno, strerror ( errno ) );
        }
        else
        {
            // Try to read description string
            if ( read ( FileHandle1, pDisksArrayCurrent->sDiskDescription, sizeof ( pDisksArrayCurrent->sDiskDescription ) - 1 ) == -1 )
            {
                // Unable to read device description, not a critical error
                Log ( DEBUG, CommonMessage ( 73 ), errno, strerror ( errno ) );
                strcpy ( pDisksArrayCurrent->sDiskDescription, "" );
            }
            // Trim trailing spaces and CR/LF symbols
            for ( iCount=strlen ( pDisksArrayCurrent->sDiskDescription ) - 1 ; iCount>=0 ; iCount-- )
            {
                // If either CR, LF or space has been found
                if (
                    ( pDisksArrayCurrent->sDiskDescription[ iCount ] == ' ' ) ||
                    ( pDisksArrayCurrent->sDiskDescription[ iCount ] == 13 ) ||
                    ( pDisksArrayCurrent->sDiskDescription[ iCount ] == 10 )
                   )
                {
                    // Trim it
                    pDisksArrayCurrent->sDiskDescription[ iCount ] = 0;
                }
                else
                {
                    break;
                }
            }
            // File handle will no longer be used for this disk device, ignoring result value
            close ( FileHandle1 );
        }

        // Append "/dev/" prefix to the device name
        strcpy ( Buffer, "/dev/" );
        strcat ( Buffer, pDisksArrayCurrent->sDiskName );
        strcpy ( pDisksArrayCurrent->sDiskName, Buffer );

        // Jump to next element in the list
        pDisksArrayCurrent = pDisksArrayCurrent->pNext;
    }

    // Delete temporary file since we don't need it anymore
    if ( unlink ( sTemplate ) == -1 )
    {
        // Error deleting temporary file, just skip that
        Log ( DEBUG, CommonMessage ( 74 ), errno, strerror ( errno ) );
    }

#endif /* __linux__ or __BSD__ */

#endif /* __DJGPP__ or _WIN32 or __unix__ */
}

// Releases memory allocated by DisksCollectInfo() function
// Invoke this function when the list of disk drives info has
// been completely processed
void DisksFreeInfo
    (
    DisksInfo_t *pDisksArray
    )
{
    if ( pDisksArray != NULL )
    {
        // This is a recursive call to delete all array elements
        // from the end to beginning
        DisksFreeInfo ( pDisksArray->pNext );
        free ( pDisksArray );
    }
}

// Reads one sector from disk device
// Under DOS: using function 42h of INT 13h
// Under Windows/Unix: using regular file operations
// Length of the buffer should be 512 bytes
void DisksReadSector
    (
    unsigned long SectorNumber,
    unsigned char* Buffer
    )
{
#if defined ( __DJGPP__ )
    struct DeviceAddressPacket buf;
    __dpmi_regs r;
    unsigned int DeviceNumber = 0;

    Log ( DEBUG,CommonMessage ( 75 ) );

    // Convert string representation of hexadecimal number of disk device
    // into integer form
    if ( Device == NULL )
    {
        Log ( FATAL,CommonMessage ( 76 ) );
    }
    if ( strlen ( Device ) != 4 )
    {
        Log ( FATAL,CommonMessage ( 77 ),strlen ( Device ),Device );
    }
    if (( Device[ 0 ] != '0' ) || ( Device[ 1 ] != 'x' ))
    {
        Log ( FATAL,CommonMessage ( 78 ),Device );
    }
    if ( sscanf ( &( Device[ 2 ] ),"%x",&DeviceNumber ) != 1 )
    {
        Log ( FATAL,CommonMessage ( 79 ),Device );
    }
    if (( DeviceNumber < 0x80 ) || ( DeviceNumber > 0xFF ))
    {
        Log ( FATAL,CommonMessage ( 79 ),Device );
    }

    memset ( &buf,0,sizeof ( struct DeviceAddressPacket ) );
    buf.packet_size = sizeof ( struct DeviceAddressPacket );
    Log ( DEBUG,CommonMessage ( 80 ),buf.packet_size );
    buf.number_of_blocks_to_transfer = 1; // we always need only 1 sector
    buf.offset_of_host_transfer_buffer = ( __tb & 0x0F ) + sizeof ( struct DeviceAddressPacket );
    buf.segment_of_host_transfer_buffer = __tb >> 4;
    // Since partition table uses 32-bit relative offset (measured in sectors)
    // we always use only first 4 bytes from 64-bit value of LBA sector number
    *( ( unsigned long* )buf.starting_logical_block_address ) = SectorNumber;

    // Transfer device address packet to conventional memory
    dosmemput ( &buf,sizeof ( buf ),__tb );

    // Invoke "Extended read" from BIOS
    r.h.ah = 0x42;
    r.h.dl = DeviceNumber;
    r.x.ds = __tb >> 4;
    r.x.si = __tb & 0x0F;
    r.x.ss = 0x0000;
    r.x.sp = 0x0000;
    r.x.flags = 0x0000;
    // Here we should not call Log() because it may corrupt
    // transfer buffer in conventional DOS memory
    __dpmi_int ( 0x13,&r );
    // Check Carry flag
    if ( ( r.x.flags & 0x0001 ) == 0x0001 )
    {
        Log ( DEBUG,CommonMessage ( 6 ),r.h.ah );
        Log ( FATAL,CommonMessage ( 81 ) );
    }

    // Transfer 512 bytes of read data from conventional memory
    dosmemget ( __tb + sizeof ( struct DeviceAddressPacket ),512,Buffer );
#elif defined ( __unix__ )
    int DeviceDescriptor;
    ssize_t NumberOfBytesRead;
#if defined ( O_LARGEFILE )
    DeviceDescriptor = open ( Device,O_RDONLY | O_LARGEFILE );
#else
    DeviceDescriptor = open ( Device,O_RDONLY );
#endif /* O_LARGEFILE */
    if ( DeviceDescriptor == -1 )
    {
        Log ( FATAL,CommonMessage ( 82 ),Device,gettext ( strerror ( errno ) ) );
    }
    if ( lseek ( DeviceDescriptor,( ( off_t )SectorNumber )*512,SEEK_SET ) == ( off_t )-1 )
    {
        Log ( FATAL,CommonMessage ( 83 ),strerror ( errno ) );
    }
    NumberOfBytesRead = read ( DeviceDescriptor,( void* )Buffer,512 );
    if ( NumberOfBytesRead == -1 )
    {
        Log ( FATAL,CommonMessage ( 84 ),Device,strerror ( errno ) );
    }
    if ( NumberOfBytesRead != 512 )
    {
        Log ( FATAL,CommonMessage ( 85 ),NumberOfBytesRead );
    }
    if ( close ( DeviceDescriptor ) == -1 )
    {
        Log ( FATAL,CommonMessage ( 86 ),Device,strerror ( errno ) );
    }
#elif defined ( _WIN32 )
    HANDLE hDisk;
    LPVOID lpMsgBuf;
    LONG lDistanceToMove;
    LONG lDistanceToMoveHigh;
    DWORD dwError;
    DWORD dwPtrLow;
    DWORD dwNumberOfBytesRead;

    hDisk = CreateFile ( Device,GENERIC_READ,FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL );
    if ( hDisk == INVALID_HANDLE_VALUE )
    {
        FormatMessage ( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,NULL,GetLastError (),MAKELANGID ( LANG_NEUTRAL,SUBLANG_DEFAULT ),( LPTSTR )&lpMsgBuf,0,NULL );
        Log ( FATAL,CommonMessage ( 82 ),Device,( LPCTSTR )lpMsgBuf );
        // Since previous Log() call leads to immediate exit, this code will never be executed
        LocalFree ( lpMsgBuf );
    }
    lDistanceToMove = ( DWORD )( ( ( ( unsigned __int64 )SectorNumber ) * 512 )&( ( unsigned __int64 )0x00000000FFFFFFFF ) );
    lDistanceToMoveHigh = ( DWORD )( ( ( ( unsigned __int64 )SectorNumber ) * 512 )>>32 );
    dwPtrLow = SetFilePointer ( hDisk,lDistanceToMove,&lDistanceToMoveHigh,FILE_BEGIN );
    dwError = GetLastError ();
    if (( dwPtrLow == 0xFFFFFFFF ) && ( dwError != NO_ERROR ))
    {
        FormatMessage ( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,NULL,dwError,MAKELANGID ( LANG_NEUTRAL,SUBLANG_DEFAULT ),( LPTSTR )&lpMsgBuf,0,NULL );
        Log ( FATAL,CommonMessage ( 83 ),( LPCTSTR )lpMsgBuf );
        // Since previous Log() call leads to immediate exit, this code will never be executed
        LocalFree ( lpMsgBuf );
    }
    if ( ReadFile ( hDisk,Buffer,512,&dwNumberOfBytesRead,NULL ) == 0 )
    {
        FormatMessage ( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,NULL,GetLastError (),MAKELANGID ( LANG_NEUTRAL,SUBLANG_DEFAULT ),( LPTSTR )&lpMsgBuf,0,NULL );
        Log ( FATAL,CommonMessage ( 84 ),Device,( LPCTSTR )lpMsgBuf );
        // Since previous Log() call leads to immediate exit, this code will never be executed
        LocalFree ( lpMsgBuf );
    }
    if ( dwNumberOfBytesRead != 512 )
    {
        Log ( FATAL,CommonMessage ( 85 ),dwNumberOfBytesRead );
    }
    if ( CloseHandle ( hDisk ) == 0 )
    {
        FormatMessage ( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,NULL,GetLastError (),MAKELANGID ( LANG_NEUTRAL,SUBLANG_DEFAULT ),( LPTSTR )&lpMsgBuf,0,NULL );
        Log ( FATAL,CommonMessage ( 86 ),Device,( LPCTSTR )lpMsgBuf );
        // Since previous Log() call leads to immediate exit, this code will never be executed
        LocalFree ( lpMsgBuf );
    }
#else
    #error "Unsupported platform"
#endif /* __DJGPP__ or __unix__ or _WIN32 */
}

// Writes one sector to disk device
// Under DOS: using function 43h of INT 13h
// Under Windows/Unix: using regular file operations
// Length of the buffer should be 512 bytes
void DisksWriteSector
    (
    unsigned long SectorNumber,
    unsigned char* Buffer
    )
{
#if defined ( __DJGPP__ )
    struct DeviceAddressPacket buf;
    __dpmi_regs r;
    unsigned int DeviceNumber = 0;

    Log ( DEBUG,CommonMessage ( 87 ) );

    // Convert string representation of hexadecimal number of disk device
    // into integer form
    if ( Device == NULL )
    {
        Log ( FATAL,CommonMessage ( 76 ) );
    }
    if ( strlen ( Device ) != 4 )
    {
        Log ( FATAL,CommonMessage ( 77 ),strlen ( Device ),Device );
    }
    if (( Device[ 0 ] != '0' ) || ( Device[ 1 ] != 'x' ))
    {
        Log ( FATAL,CommonMessage ( 78 ),Device );
    }
    if ( sscanf ( &( Device[ 2 ] ),"%x",&DeviceNumber ) != 1 )
    {
        Log ( FATAL,CommonMessage ( 79 ),Device );
    }
    if (( DeviceNumber < 0x80 ) || ( DeviceNumber > 0xFF ))
    {
        Log ( FATAL,CommonMessage ( 79 ),Device );
    }

    memset ( &buf,0,sizeof ( struct DeviceAddressPacket ) );
    buf.packet_size = sizeof ( struct DeviceAddressPacket );
    Log ( DEBUG,CommonMessage ( 80 ),buf.packet_size );
    buf.number_of_blocks_to_transfer = 1; // we always need only 1 sector
    buf.offset_of_host_transfer_buffer = ( __tb & 0x0F ) + sizeof ( struct DeviceAddressPacket );
    buf.segment_of_host_transfer_buffer = __tb >> 4;
    // Since partition table uses 32-bit relative offset (measured in sectors)
    // we always use only first 4 bytes from 64-bit value of LBA sector number
    *( ( unsigned long* )buf.starting_logical_block_address ) = SectorNumber;

    // Transfer device address packet to conventional memory
    dosmemput ( &buf,sizeof ( buf ),__tb );
    // Transfer 512 bytes of data to conventional memory for writing
    dosmemput ( Buffer,512,__tb + sizeof ( struct DeviceAddressPacket ) );

    // Invoke "Extended write" from BIOS
    r.h.ah = 0x43;
    r.h.al = 0;   // no write verify
    r.h.dl = DeviceNumber;
    r.x.ds = __tb >> 4;
    r.x.si = __tb & 0x0F;
    r.x.ss = 0x0000;
    r.x.sp = 0x0000;
    r.x.flags = 0x0000;
    // Here we should not call Log() because it may corrupt
    // transfer buffer in conventional DOS memory
    __dpmi_int ( 0x13,&r );
    // Check Carry flag
    if ( ( r.x.flags & 0x0001 ) == 0x0001 )
    {
        Log ( DEBUG,CommonMessage ( 6 ),r.h.ah );
        Log ( FATAL,CommonMessage ( 88 ) );
    }
#elif defined ( __unix__ )
    int DeviceDescriptor;
    ssize_t NumberOfBytesWritten;
#if defined ( O_LARGEFILE )
    DeviceDescriptor = open ( Device,O_WRONLY | O_LARGEFILE );
#else
    DeviceDescriptor = open ( Device,O_WRONLY );
#endif /* O_LARGEFILE */
    if ( DeviceDescriptor == -1 )
    {
        Log ( FATAL,CommonMessage ( 89 ),Device,strerror ( errno ) );
    }
    if ( lseek ( DeviceDescriptor,( ( off_t )SectorNumber )*512,SEEK_SET ) == ( off_t )-1 )
    {
        Log ( FATAL,CommonMessage ( 83 ),strerror ( errno ) );
    }
    NumberOfBytesWritten = write ( DeviceDescriptor,( void* )Buffer,512 );
    if ( NumberOfBytesWritten == -1 )
    {
        Log ( FATAL,CommonMessage ( 90 ),Device,strerror ( errno ) );
    }
    if ( NumberOfBytesWritten != 512 )
    {
        Log ( FATAL,CommonMessage ( 91 ),NumberOfBytesWritten );
    }
    if ( close ( DeviceDescriptor ) == -1 )
    {
        Log ( FATAL,CommonMessage ( 86 ),Device,strerror ( errno ) );
    }
#elif defined ( _WIN32 )
    HANDLE hDisk;
    LPVOID lpMsgBuf;
    LONG lDistanceToMove;
    LONG lDistanceToMoveHigh;
    DWORD dwError;
    DWORD dwPtrLow;
    DWORD dwNumberOfBytesWritten;

    hDisk = CreateFile ( Device,GENERIC_WRITE,FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL );
    if ( hDisk == INVALID_HANDLE_VALUE )
    {
        FormatMessage ( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,NULL,GetLastError (),MAKELANGID ( LANG_NEUTRAL,SUBLANG_DEFAULT ),( LPTSTR )&lpMsgBuf,0,NULL );
        Log ( FATAL,CommonMessage ( 89 ),Device,( LPCTSTR )lpMsgBuf );
        // Since previous Log() call leads to immediate exit, this code will never be executed
        LocalFree ( lpMsgBuf );
    }
    lDistanceToMove = ( DWORD )( ( ( ( unsigned __int64 )SectorNumber ) * 512 )&( ( unsigned __int64 )0x00000000FFFFFFFF ) );
    lDistanceToMoveHigh = ( DWORD )( ( ( ( unsigned __int64 )SectorNumber ) * 512 )>>32 );
    dwPtrLow = SetFilePointer ( hDisk,lDistanceToMove,&lDistanceToMoveHigh,FILE_BEGIN );
    dwError = GetLastError ();
    if (( dwPtrLow == 0xFFFFFFFF ) && ( dwError != NO_ERROR ))
    {
        FormatMessage ( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,NULL,dwError,MAKELANGID ( LANG_NEUTRAL,SUBLANG_DEFAULT ),( LPTSTR )&lpMsgBuf,0,NULL );
        Log ( FATAL,CommonMessage ( 83 ),( LPCTSTR )lpMsgBuf );
        // Since previous Log() call leads to immediate exit, this code will never be executed
        LocalFree ( lpMsgBuf );
    }
    if ( WriteFile ( hDisk,Buffer,512,&dwNumberOfBytesWritten,NULL ) == 0 )
    {
        FormatMessage ( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,NULL,GetLastError (),MAKELANGID ( LANG_NEUTRAL,SUBLANG_DEFAULT ),( LPTSTR )&lpMsgBuf,0,NULL );
        Log ( FATAL,CommonMessage ( 90 ),Device,( LPCTSTR )lpMsgBuf );
        // Since previous Log() call leads to immediate exit, this code will never be executed
        LocalFree ( lpMsgBuf );
    }
    if ( dwNumberOfBytesWritten != 512 )
    {
        Log ( FATAL,CommonMessage ( 91 ),dwNumberOfBytesWritten );
    }
    if ( CloseHandle ( hDisk ) == 0 )
    {
        FormatMessage ( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,NULL,GetLastError (),MAKELANGID ( LANG_NEUTRAL,SUBLANG_DEFAULT ),( LPTSTR )&lpMsgBuf,0,NULL );
        Log ( FATAL,CommonMessage ( 86 ),Device,( LPCTSTR )lpMsgBuf );
        // Since previous Log() call leads to immediate exit, this code will never be executed
        LocalFree ( lpMsgBuf );
    }
#else
    #error "Unsupported platform"
#endif /* __DJGPP__ or __unix__ or _WIN32 */
}

