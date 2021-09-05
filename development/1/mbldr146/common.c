// Project name:  Master Boot Loader (mbldr)
// file name:     common.c
// See also:      common.h, mbldrcli.c, mbldrgui.cpp, mbldr.asm, mbldr.h
// Author:        Arnold Shade
// Creation date: 14 February 2007
// License type:  BSD
// URL:           http://mbldr.sourceforge.net/
// Description:   Contains common functions, definitions and
// data structures for CLI and GUI versions of mbldr program

// Include standard system headers
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <libintl.h>

// Include local headers
#include "common.h"
#include "disks.h"
#include "log.h"
#include "mbldr.h"

// Offsets of configurable fields (opcodes and data) in first
// sector containing binary mbldr and partition table

// 1 byte (number of timer interrupt - user or system)
#define MBLDR_TIMER_INTERRUPT1 0x30

// 1 byte (ASCII code of interrupt key)
#define MBLDR_TIMER_INTERRUPT_KEY 0x44

// 2 bytes (opcode, which chooses between ASCII and scan code)
#define MBLDR_USE_ASCII_OR_SCAN_OPCODE 0x4C

// 1 byte (base ASCII/scan code for the first OS in the list)
#define MBLDR_BASE_ASCII_OR_SCAN_CODE 0x4F

// 1 byte (number of OS entries in the boot menu)
#define MBLDR_NUMBER_OF_BOOTABLE_PARTITIONS 0x51

// 2 bytes (1/10 of number of ticks to wait for timed boot)
// (ThisValue*10*55ms=DefaultBootTimeout)
#define MBLDR_TIMEOUT 0x55

// 2 bytes (opcode for progress-bar symbol)
// Either [0x88, 0xF0] or [0xB0, 0xNN] (where NN is a progress-bar symbol)
#define MBLDR_PROGRESS_BAR_SYMBOL_OPCODE 0x5F

// 2 bytes (opcode for progress-bar switch on/off)
#define MBLDR_PROGRESS_BAR_SWITCH_OPCODE 0x61

// 1 byte (number of default entry in the boot menu)
#define MBLDR_NUMBER_OF_DEFAULT_PARTITION 0x6B

// 3 bytes (opcode, which saves new default entry in the boot menu)
#define MBLDR_MODIFY_DEFAULT_PARTITION_OPCODE 0x6C

// 1 byte (number of timer interrupt - user or system)
#define MBLDR_TIMER_INTERRUPT2 0x73

// 2 bytes (whether to mark primary partitions active or inactive depending
// on what to boot or leave "active" flag as-is)
#define MBLDR_MARK_ACTIVE_PARTITION 0x97

// 1 byte (jump offset to enable/disable hiding of other primary partitions)
#define MBLDR_HIDING_JUMP_OFFSET 0xBF

// 1 byte (opcode, which controls timer ticks)
#define MBLDR_TIMED_BOOT_ALLOWED 0xF6

// 1 byte (opcode, which controls whether to do an IRET or proceed with old vector)
#define MBLDR_IRET 0xF7

// Offset of the boot menu text, which occupies the rest of MBR
#define MBLDR_BOOT_MENU_TEXT 0x10A

// DiskSignature (or so-called NT Drive Serial Number) for Microsoft
// Windows. These bytes should not be overwritten since it breaks boot
// process under Windows Vista
// 4 bytes (the signature itself) + 2 bytes (seems to be always 0000h)
#define MBLDR_WINDOWS_DISK_SIGNATURE 0x1B8

// Number of partitions in the table
unsigned short NumberOfFoundPartitions = 0;
// Array of found partitions (not more than 256 entries is allowed)
struct FoundPartitionEntry FoundPartitions[ 256 ];
// First sector number of the extended partition
unsigned long ExtendedPartitionBegin = 0;

// Array of offsets in sectors representing bootable partitions
struct BootablePartitionEntry BootablePartitions[ 9 ];
// A number between 1 and 9
unsigned char NumberOfBootablePartitions = 0;
// A number between 0 and NumberBootablePartitions-1
unsigned char NumberOfDefaultPartition = 0;
// 0 means ASCII code of a key should be analyzed, 1 means scan-code
unsigned char UseAsciiOrScanCode = 0;
// Base of the key code that should be used to boot any of the operating systems
unsigned char BaseAsciiOrScanCode = '1';
// Either 0 (don't hide other primary partitions) or 1 (hide them)
unsigned char HideOtherPrimaryPartitions = 1;
// Either 0 (don't mark primary partitions active/inactive) or 1 (mark them)
unsigned char MarkActivePartition = 1;
// Either 0 (not allowed) or 1 (allowed)
unsigned char ProgressBarAllowed = 1;
// ASCII code of a symbol being used for progress-bar output
// 0 means that progress-bar should look like "[9876543210]"
// Any other code refers to a real symbol in ASCII table like "[**********]"
unsigned char ProgressBarSymbol = 0;
// Either 0x1B (Esc) or 0x20 (Space)
unsigned char TimerInterruptKey = 0x1B;
// Either 0 (not allowed) or 1 (allowed, user timer) or 2 (allowed, system timer)
unsigned char TimedBootAllowed = 1;
// Timeout for default boot. Here it is measured in ticks (1/18th of a
// second). Maximum value is 64800*18*10 (10 hours), minimum is 0 which
// means immediate boot. Default value for timeout is 1 minute
unsigned long Timeout = 18*60;

// Boolean flag indicating the presence of cusom boot menu text
// (the boot menu text should not be generated automatically)
unsigned char CustomBootMenuText = 0;
// Text describing boot menu (512 is unreachable maximum)
char BootMenuText[ 512 ] = "";

// Table with mask that should be used to determine whether
// the MBR contains mbldr. 0x00 means the byte is valuable,
// 0xff means it should be ignored. The length of this table
// is always 512 bytes. Location of bytes that should be
// ignored is determined manually with listing after mbldr.asm
// translation. This table has similar contents to mbldr.h
// which is generated automatically from assembly source
unsigned char mask_bin[ 512 ] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff,
  0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
  0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00,
  0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00
};

// Array of partitions descriptions. This list was mainly copied
// from http://www.win.tue.nl/~aeb/partitions/partition_types-1.html
// I changed it a little bit to shorten textual string
// (allowing them to be displayed correctly in 80chars-width
// terminals like DOS in its usual 80x25 text video mode)
// This array does not contain partition identifiers because
// the identifier is an index of particular entry
const char* PartitionTypes[] = {
    "Empty", // 0x00
    "DOS 12-bit FAT", // 0x01
    "XENIX root", // 0x02
    "XENIX /usr", // 0x03
    "DOS 3.0+ 16-bit FAT (up to 32M)", // 0x04
    "Extended partition (DOS 3.3+)", // 0x05
    "DOS 3.31+ 16-bit FAT (over 32M)", // 0x06
    "Windows NT NTFS or OS/2 HPFS (IFS)", // 0x07
    "AIX boot (PS/2 port) or OS/2 (v1.0-1.3) or SplitDrive", // 0x08
    "AIX data or Coherent filesystem or QNX 1.x-2.x", // 0x09
    "OS/2 Boot Manager or Coherent swap partition or OPUS", // 0x0a
    "WIN95_OSR2/Win98 FAT32", // 0x0b
    "WIN95_OSR2/Win98 FAT32, LBA-mapped", // 0x0c
    "Unknown or not recognized", // 0x0d
    "WIN95/98: DOS 16-bit FAT, LBA-mapped", // 0x0e
    "Extended partition, LBA-mapped (WIN95/98)", // 0x0f
    "OPUS", // 0x10
    "Hidden DOS 12-bit FAT or Leading Edge DOS 3.x", // 0x11
    "Compaq configuration/diagnostics partition", // 0x12
    "Unknown or not recognized", // 0x13
    "Hidden DOS 16-bit FAT <32M or AST DOS", // 0x14
    "Unknown or not recognized", // 0x15
    "Hidden DOS 16-bit FAT >=32M", // 0x16
    "Hidden NTFS/HPFS(IFS)", // 0x17
    "AST SmartSleep Partition", // 0x18
    "Unknown or not recognized", // 0x19
    "Unknown or not recognized", // 0x1a
    "Hidden WIN95_OSR2/98 FAT32", // 0x1b
    "Hidden WIN95_OSR2/98 FAT32, LBA-mapped", // 0x1c
    "Unknown or not recognized", // 0x1d
    "Hidden WIN95 16-bit FAT, LBA-mapped", // 0x1e
    "Unknown or not recognized", // 0x1f
    "Unknown or not recognized", // 0x20
    "Unknown or not recognized", // 0x21
    "Unknown or not recognized", // 0x22
    "Unknown or not recognized", // 0x23
    "NEC DOS 3.x", // 0x24
    "Unknown or not recognized", // 0x25
    "Unknown or not recognized", // 0x26
    "Unknown or not recognized", // 0x27
    "Unknown or not recognized", // 0x28
    "Unknown or not recognized", // 0x29
    "AtheOS File System (AFS)", // 0x2a
    "SyllableSecure (SylStor)", // 0x2b
    "Unknown or not recognized", // 0x2c
    "Unknown or not recognized", // 0x2d
    "Unknown or not recognized", // 0x2e
    "Unknown or not recognized", // 0x2f
    "Unknown or not recognized", // 0x30
    "Unknown or not recognized", // 0x31
    "NOS", // 0x32
    "Unknown or not recognized", // 0x33
    "Unknown or not recognized", // 0x34
    "JFS on OS/2 or eCS", // 0x35
    "Unknown or not recognized", // 0x36
    "Unknown or not recognized", // 0x37
    "THEOS ver 3.2 2gb partition", // 0x38
    "Plan 9 partition or THEOS ver 4 spanned partition", // 0x39
    "THEOS ver 4 4gb partition", // 0x3a
    "THEOS ver 4 extended partition", // 0x3b
    "PartitionMagic recovery partition", // 0x3c
    "Hidden NetWare", // 0x3d
    "Unknown or not recognized", // 0x3e
    "Unknown or not recognized", // 0x3f
    "Venix 80286", // 0x40
    "Linux/MINIX or PPC PReP Boot", // 0x41
    "Linux swap or SFS or Windows 2000 dynamic ext. partition marker", // 0x42
    "Linux native", // 0x43
    "GoBack partition", // 0x44
    "Boot-US boot manager or Priam or EUMEL/Elan", // 0x45
    "EUMEL/Elan", // 0x46
    "EUMEL/Elan", // 0x47
    "EUMEL/Elan", // 0x48
    "Unknown or not recognized", // 0x49
    "AdaOS Aquila (Withdrawn)", // 0x4a
    "Unknown or not recognized", // 0x4b
    "Oberon partition", // 0x4c
    "QNX4.x", // 0x4d
    "QNX4.x 2nd part", // 0x4e
    "QNX4.x 3rd part or Oberon partition", // 0x4f
    "OnTrack Disk Manager (older versions) RO or Lynx RTOS", // 0x50
    "OnTrack Disk Manager RW (DM6 Aux1) or Novell", // 0x51
    "CP/M or Microport SysV/AT", // 0x52
    "OnTrack Disk Manager 6.0 Aux3", // 0x53
    "OnTrack Disk Manager 6.0 Dynamic Drive Overlay (DDO)", // 0x54
    "EZ-Drive", // 0x55
    "Golden Bow VFeature Partitioned Volume", // 0x56
    "DrivePro or VNDI Partition", // 0x57
    "Unknown or not recognized", // 0x58
    "Unknown or not recognized", // 0x59
    "Unknown or not recognized", // 0x5a
    "Unknown or not recognized", // 0x5b
    "Priam EDisk", // 0x5c
    "Unknown or not recognized", // 0x5d
    "Unknown or not recognized", // 0x5e
    "Unknown or not recognized", // 0x5f
    "Unknown or not recognized", // 0x60
    "SpeedStor", // 0x61
    "Unknown or not recognized", // 0x62
    "Unix System V/386 (SCO, ISC Unix, UnixWare) or Mach or GNU Hurd", // 0x63
    "PC-ARMOUR protected partition or Novell Netware 286, 2.xx", // 0x64
    "Novell Netware 386, 3.xx or 4.xx", // 0x65
    "Novell Netware SMS Partition", // 0x66
    "Novell", // 0x67
    "Novell", // 0x68
    "Novell Netware 5+, Novell Netware NSS Partition", // 0x69
    "Unknown or not recognized", // 0x6a
    "Unknown or not recognized", // 0x6b
    "Unknown or not recognized", // 0x6c
    "Unknown or not recognized", // 0x6d
    "Unknown or not recognized", // 0x6e
    "Unknown or not recognized", // 0x6f
    "DiskSecure Multi-Boot", // 0x70
    "Unknown or not recognized", // 0x71
    "Unknown or not recognized", // 0x72
    "Unknown or not recognized", // 0x73
    "Scramdisk partition", // 0x74
    "IBM PC/IX", // 0x75
    "Unknown or not recognized", // 0x76
    "M2FS/M2CS partition or VNDI Partition", // 0x77
    "XOSL FS", // 0x78
    "Unknown or not recognized", // 0x79
    "Unknown or not recognized", // 0x7a
    "Unknown or not recognized", // 0x7b
    "Unknown or not recognized", // 0x7c
    "Unknown or not recognized", // 0x7d
    "Unknown or not recognized", // 0x7e
    "Unknown or not recognized", // 0x7f
    "Old MINIX (until 1.4a)", // 0x80
    "MINIX since 1.4b, early Linux or Mitac disk manager", // 0x81
    "Linux swap or Prime or Solaris x86", // 0x82
    "Linux native partition", // 0x83
    "OS/2 hidden C: drive or Hibernation partition", // 0x84
    "Linux extended partition", // 0x85
    "Old Linux RAID partition superblock or FAT16 volume set", // 0x86
    "NTFS volume set", // 0x87
    "Linux plaintext partition table", // 0x88
    "Unknown or not recognized", // 0x89
    "Linux Kernel Partition (used by AiR-BOOT)", // 0x8a
    "Legacy Fault Tolerant FAT32 volume", // 0x8b
    "Legacy Fault Tolerant FAT32 volume using BIOS extended INT 13h", // 0x8c
    "Free FDISK hidden Primary DOS FAT12 partitition", // 0x8d
    "Linux Logical Volume Manager (LVM) partition", // 0x8e
    "Unknown or not recognized", // 0x8f
    "Free FDISK hidden Primary DOS FAT16 partitition", // 0x90
    "Free FDISK hidden DOS extended partitition", // 0x91
    "Free FDISK hidden Primary DOS large FAT16 partitition", // 0x92
    "Hidden Linux native partition or Amoeba", // 0x93
    "Amoeba bad block table (BBT)", // 0x94
    "MIT EXOPC native partitions", // 0x95
    "Unknown or not recognized", // 0x96
    "Free FDISK hidden Primary DOS FAT32 partitition", // 0x97
    "Free FDISK hidden Primary DOS FAT32 partitition", // 0x98
    "DCE376 logical drive", // 0x99
    "Free FDISK hidden Primary DOS FAT16 partitition (LBA)", // 0x9a
    "Free FDISK hidden DOS extended partitition (LBA)", // 0x9b
    "Unknown or not recognized", // 0x9c
    "Unknown or not recognized", // 0x9d
    "Unknown or not recognized", // 0x9e
    "BSD/OS (BSDI)", // 0x9f
    "IBM Thinkpad laptop hibernation partition", // 0xa0
    "Laptop hibernation partition or HP Volume Expansion", // 0xa1
    "Unknown or not recognized", // 0xa2
    "HP Volume Expansion (SpeedStor variant)", // 0xa3
    "HP Volume Expansion (SpeedStor variant)", // 0xa4
    "FreeBSD, NetBSD, BSD/386, 386BSD", // 0xa5
    "OpenBSD or HP Volume Expansion (SpeedStor variant)", // 0xa6
    "NeXTStep", // 0xa7
    "Mac OS-X (Darwin UFS)", // 0xa8
    "NetBSD", // 0xa9
    "Olivetti Fat 12 1.44MB Service Partition", // 0xaa
    "Mac OS-X (Darwin) Boot partition or GO! partition", // 0xab
    "Unknown or not recognized", // 0xac
    "Unknown or not recognized", // 0xad
    "ShagOS filesystem", // 0xae
    "ShagOS swap partition", // 0xaf
    "BootStar Dummy", // 0xb0
    "HP Volume Expansion (SpeedStor variant)", // 0xb1
    "Unknown or not recognized", // 0xb2
    "HP Volume Expansion (SpeedStor variant)", // 0xb3
    "HP Volume Expansion (SpeedStor variant)", // 0xb4
    "Unknown or not recognized", // 0xb5
    "Corrupted Windows NT mirror set (master), FAT16 file system", // 0xb6
    "BSDI BSD/386 filesystem or Corrupted Win NT mirror set, NTFS", // 0xb7
    "BSDI BSD/386 swap partition", // 0xb8
    "Unknown or not recognized", // 0xb9
    "Unknown or not recognized", // 0xba
    "Boot Wizard hidden", // 0xbb
    "Unknown or not recognized", // 0xbc
    "Unknown or not recognized", // 0xbd
    "Solaris 8 boot partition", // 0xbe
    "New Solaris x86 partition", // 0xbf
    "CTOS or REAL/32 secure small partition or NTFT Partition", // 0xc0
    "DRDOS/secured (FAT-12)", // 0xc1
    "Hidden Linux", // 0xc2
    "Hidden Linux swap", // 0xc3
    "DRDOS/secured (FAT-16, < 32M)", // 0xc4
    "DRDOS/secured (extended)", // 0xc5
    "DRDOS/secured (FAT-16, >= 32M) or WinNT corrupted FAT16 volume/stripe set", // 0xc6
    "Windows NT corrupted NTFS volume/stripe set or Syrinx boot", // 0xc7
    "Reserved for DR-DOS 8.0+", // 0xc8
    "Reserved for DR-DOS 8.0+", // 0xc9
    "Reserved for DR-DOS 8.0+", // 0xca
    "DR-DOS 7.04+ secured FAT32 (CHS)", // 0xcb
    "DR-DOS 7.04+ secured FAT32 (LBA)", // 0xcc
    "CTOS Memdump?", // 0xcd
    "DR-DOS 7.04+ FAT16X (LBA)", // 0xce
    "DR-DOS 7.04+ secured EXT DOS (LBA)", // 0xcf
    "REAL/32 secure big partition or Multiuser DOS secured partition", // 0xd0
    "Old Multiuser DOS secured FAT12", // 0xd1
    "Unknown or not recognized", // 0xd2
    "Unknown or not recognized", // 0xd3
    "Old Multiuser DOS secured FAT16 <32M", // 0xd4
    "Old Multiuser DOS secured extended partition", // 0xd5
    "Old Multiuser DOS secured FAT16 >=32M", // 0xd6
    "Unknown or not recognized", // 0xd7
    "CP/M-86", // 0xd8
    "Unknown or not recognized", // 0xd9
    "Non-FS Data", // 0xda
    "Digital Research CP/M", // 0xdb
    "Unknown or not recognized", // 0xdc
    "Hidden CTOS Memdump?", // 0xdd
    "Dell PowerEdge Server utilities (FAT fs)", // 0xde
    "DG/UX virtual disk manager partition or BootIt EMBRM", // 0xdf
    "Reserved by STMicroelectronics for a filesystem called ST AVFS", // 0xe0
    "DOS access or SpeedStor 12-bit FAT extended partition", // 0xe1
    "Unknown or not recognized", // 0xe2
    "DOS R/O or SpeedStor", // 0xe3
    "SpeedStor 16-bit FAT extended partition < 1024 cyl", // 0xe4
    "Tandy MSDOS with logically sectored FAT", // 0xe5
    "Storage Dimensions SpeedStor", // 0xe6
    "Unknown or not recognized", // 0xe7
    "Unknown or not recognized", // 0xe8
    "Unknown or not recognized", // 0xe9
    "Unknown or not recognized", // 0xea
    "BeOS BFS", // 0xeb
    "SkyOS SkyFS", // 0xec
    "Unknown or not recognized", // 0xed
    "Intel EFI GUID Partition Table (followed by an EFI header)", // 0xee
    "Intel EFI (FAT-12/16/32) file system partition", // 0xef
    "Linux/PA-RISC boot loader", // 0xf0
    "Storage Dimensions SpeedStor", // 0xf1
    "DOS 3.3+ secondary partition", // 0xf2
    "Unknown or not recognized", // 0xf3
    "SpeedStor large partition or Prologue single-volume partition", // 0xf4
    "Prologue multi-volume partition", // 0xf5
    "Storage Dimensions SpeedStor", // 0xf6
    "Unknown or not recognized", // 0xf7
    "Unknown or not recognized", // 0xf8
    "pCache", // 0xf9
    "Bochs", // 0xfa
    "VMware File System partition", // 0xfb
    "VMware Swap partition", // 0xfc
    "Linux raid partition with autodetect", // 0xfd
    "Windows NT Disk Administrator hidden partition or Linux LVM (old)", // 0xfe
    "Xenix Bad Block Table (BBT)" // 0xff
};

// Windows/Unix: Device name of a hard disk or file being used
// DOS: String representation of BIOS device number of a hard disk
// (in hexadecimal mode with "0x" prefix like "0x80", "0x81", etc.)
char Device[ 1024 ] = "";

// Array of messages used in the system. This includes all user
// interface and logging messages. Each message is referenced by
// array index across the program code. It is not allowed to delete
// entries from the middle of the list, obsolete entries should be
// set to empty strings (if they are not used in the code anymore).
// Similarly adding new items is allowed only at the and of the list
const char* Messages[] = {
    /* Strings for logging module */
    "FATAL: ", // 0
    "DEBUG: ", // 1
    "UNKNOWN: ", // 2
    "Error writing log, it seems there is no space left on a device.", // 3

    /* Strings for disks management module */
    "Before interrupt INT 13h, AH=%02Xh", // 4
    "After interrupt", // 5
    "Error code: %u", // 6
    "Unable to get maximum number of available devices.", // 7
    "Found devices: %u", // 8
    "Device: 0x%02X.", // 9
    "Extensions are not present, device will not be used.", // 10
    "Magic number: %u", // 11
    "Magic number is not equal to 0xAA55.", // 12
    "Version of extensions: %u (is not equal to 0x30)", // 13
    "Interface support bitmap: %u", // 14
    "Fixed disk access subset is not supported by extensions.", // 15
    "Size of DeviceParameters structure: %u", // 16
    "Could not get device parameters.", // 17
    "Since structure size is more than 30 and interface support bitmap has EDD support extension, save important information from DPTE", // 18
    "IOPortBaseAddress=0x%04X", // 19
    "ATADeviceBit=0x%02X", // 20
    "Size structure returned by BIOS interrupt: %u", // 21
    "Structure returned by 'get device parameters' is less than minimum.", // 22
    "Sector size: %u", // 23
    "Size of a sector is not equal to 512.", // 24
    "Calculating approximate size", // 25
    "SizeByte[%u]=%02X", // 26
    "Too small hard drive, it seems some error has occured.", // 27
    "Memory allocation error", // 28
    "Internal error with dynamic list allocation logic, this should never happen", // 29
    "Size of a structure is greater or equal than 30", // 30
    "Size of a structure is equal either to 74 or to 66", // 31
    "Should be 0xBEDD, found: 0x%X", // 32
    "Key, indicating presence of Device Path Information was not found.", // 33
    "Buffer size is %u, Device Path Information length is %u", // 34
    "Buffer size does not suit the length of Device Path Information.", // 35
    "Host bus type was: '%s'", // 36
    "Reducing trailing space", // 37
    "Interface type was: '%s'", // 38
    "Either ATA or SATA interface type is detected", // 39
    "DevicePath[0]=0x%02X, so modifying ATADeviceBit", // 40
    "InterfaceSupportBitmap=%04X", // 41
    "'Enhanced Disk Drive (EDD) Support' extensions subset is present", // 42
    "Primary interface", // 43
    "/Primary", // 44
    "Secondary interface", // 45
    "/Secondary", // 46
    "Master device", // 47
    "/Master", // 48
    "Slave device", // 49
    "/Slave", // 50
    "ATADeviceBit=0x%02X", // 51
    "All 256 words of data have been received", // 52
    "Model number was: '%s'", // 53
    "Trying disk device %s", // 54
    "Unable to close disk device %s", // 55
    "glob() system call succeeded on %s", // 56
    "Unable to open file with model description, errno=%i, (%s)", // 57
    "Unable to read file with model description, errno=%i, (%s)", // 58
    "Running out of memory, errno=%i, (%s)", // 59
    "Read error, errno=%i, (%s)", // 60
    "No found matches, errno=%i, (%s)", // 61
    "Unknown error, errno=%i, (%s)", // 62
    "Unable to open file with vendor description, errno=%i, (%s)", // 63
    "Unable to read file with vendor description, errno=%i, (%s)", // 64
    "Unable to create temporary file, errno=%i, (%s)", // 65
    "Unable to close temporary file, errno=%i, (%s)", // 66
    "Command to run: '%s', error code=%i", // 67
    "Unable to execute sysctl command", // 68
    "Unable to open temporary file, errno=%i, (%s)", // 69
    "Trying to get description for device %s", // 70
    "Trying to read device model/vendor for device %s", // 71
    "Unable to open file with device description, errno=%i, (%s)", // 72
    "Unable to read file with device description, errno=%i, (%s)", // 73
    "Unable to delete temporary file, errno=%i, (%s)", // 74
    "DisksReadSector() started", // 75
    "Device name could not be NULL", // 76
    "Length of string of DeviceName should be equal to 4, but it is %i (Device='%s')", // 77
    "Invalid prefix in Device name (Device='%s')", // 78
    "Invalid Device name (Device='%s')", // 79
    "Size of DeviceAddressPacket structure: %u", // 80
    "Unable to read sector.", // 81
    "Unable to open device %s for reading, %s", // 82
    "Unable to seek to desired position, %s", // 83
    "Unable to read data from device %s, %s", // 84
    "Read operation returned unexpected number of bytes being read: %i", // 85
    "Unable to close device %s, %s", // 86
    "DisksWriteSector() started", // 87
    "Unable to write sector.", // 88
    "Unable to open device %s for writing, %s", // 89
    "Unable to write data to device %s, %s", // 90
    "Write operation returned unexpected number of bytes being written: %i", // 91

    /* Strings for common module */
    "MBLDR", // 92
    "Arnold Shade <arnold_shade@users.sourceforge.net>", // 93
    "\
Master Boot LoaDeR is a boot loader for master boot record intended to support\n\
booting from different partitions. It has simple text boot menu and occupies\n\
only the first sector of hard disk. It is intended to replace MBR contents\n\
coming with MS-DOS/Windows. It supports partition hiding, booting OSes above\n\
1024 cylinder, active partition switching and may load default OS by timeout.\n\
It is also capable to boot Linux/BSD partitions. This program is intended to\n\
install and configure mbldr on the HDD you choose using interactive menus.\n\n", // 94
    "Syntax: ", // 95
    " [option] [option]...\n", // 96
    "\
Options are:\n\
 -h Outputs help message and exits\n\
 -v Verbose mode (debug only, uses log file)\n", // 97
#if defined ( __DJGPP__ )
    "\
 -d <device_number> Defines BIOS device number if hard disk autodetect does not\n\
    work. Parameter should be in the following form: '0x80', '0x81', etc.\n\
    (without apostrophes)", // 98
#elif defined ( __unix__ )
    "\
 -d <device> Defines a name of device (usually hard disk) to manage.\n\
    This is typically /dev/hda or /dev/sda under Linux and /dev/ad0 or\n\
    /dev/da0 under BSD. It also can be a regular file representing binary\n\
    image of some device.", // 98
#elif defined ( _WIN32 )
    "\
 -d <device_number> Defines a number of hard disk (should be a sequential\n\
    non-negative integer value: 0, 1, 2, etc.) This could be useful if disk\n\
    autodetect does not work for some reasons.\n\
    or\n\
 -d <file_name> Representing binary image of a device. This syntax could also\n\
    be used if you want to specify devices which are not covered with\n\
    \\\\.\\PHYSICALDRIVE naming scheme.", // 98
#else
    #error "Unsupported platform"
#endif /* __DJGPP__ or __unix__ or _WIN32 */
    "http://mbldr.sourceforge.net/", // 99
    "User asked for command-line help.", // 100
    "'%c' option is detected", // 101
    "Log level set to DEBUG", // 102
    "Invalid -d parameter: %s", // 103
    "Device name has been set to %s", // 104
    "Unknown option %c", // 105
    "Reading sector %lu.", // 106
    "Magic number 0xAA55 was not found.", // 107
    "Extended partition 0x%02X has been found.", // 108
    "More than one extended partition has been found.", // 109
    "Storing initial extended partition offset 0x%08lX", // 110
    "Calculating extended partition offset: initial 0x%08lX + current 0x%08lX", // 111
    "Partition 0x%02X has been found.", // 112
    "Relative offset in sectors: 0x%08lX", // 113
    "Size in sectors: 0x%08lX", // 114
    "This partition is primary", // 115
    "Swap partition entries: %s", // 116
    "This partition belongs to an extended partition (is a logical disk)", // 117
    "Too many partitions have been found.", // 118
    "Empty partition slot has been found.", // 119
    "Recursively call this function once again pointing it to an extended partition.", // 120
    "Verbosity is set to %u", // 121
    "Maximum verbosity reached", // 122
    "Number of available characters for custom boot menu text: %i", // 123
    "\
Too many characters in custom boot menu text.\n\
Regenerate boot menu text automatically or upload smaller file.\n\
Installation of mbldr is not allowed until you free at least %i bytes.", // 124
    "Boot menu text will be generated after adding bootable partitions.", // 125
    "Program name and version", // 126
    "Boot menu body with the list of partitions", // 127
    "Unknown partition has been found, empty label", // 128
    "Timeout value and instruction how to interrupt timer", // 129
    "TimerInterruptKey has unexpected value %u", // 130
    "Instruction what Enter does", // 131
    "Instruction what to click", // 132
    "Progress bar frame", // 133
    "Number of characters left=%i", // 134
    "Result of nested call=%i", // 135
    "Populate boot menu text", // 136
    "\
Too many characters have been entered for labels of bootable partitions.\n\
Installation of mbldr is not allowed until you free at least %i bytes.", // 137
    "Old MBR contents:", // 138
    "mbldr_bin_len=%u, (not equal to 512)", // 139
    "First difference found at offset %u, found 0x%02X, should be 0x%02X.", // 140
    "Mask array contains wrong values (offset %u, value 0x%02X).", // 141
    "Mbldr was found.", // 142
    "\
Existing mbldr was found on the chosen disk device. Would you\n\
like to configure it? (otherwise a fresh install will be performed)", // 143
    "User wants to configure existing mbldr.", // 144
    "User wants to do fresh install.", // 145
    "ASCII code 0x%02X for timer interruption is neither Esc nor Space in existing mbldr.", // 146
    "TimerInterruptKey: 0x%02X", // 147
    "Number of bootable partitions is incorrect (%u) in existing mbldr.", // 148
    "NumberOfBootablePartitions: %u", // 149
    "Number of default partition is incorrect (%u) in existing mbldr.", // 150
    "NumberOfDefaultPartition: %u", // 151
    "ASCII codes are used", // 152
    "Scan-codes are used", // 153
    "Base keyboard code: %u", // 154
    "Progress-bar looks like 9876543210", // 155
    "Progress-bar symbol is '%c'", // 156
    "Progress-bar is disabled", // 157
    "Progress-bar is enabled", // 158
    "Timed boot is disabled (opcode: 0x%02X)", // 159
    "Timed boot is set to %i, opcode: 0x%02X, timer_byte1=0x%02X, timer_byte2=0x%02X", // 160
    "Timeout field is incorrect (%u) in existing mbldr.", // 161
    "Timeout: %u", // 162
    "Boot menu text is corrupted.", // 163
    "BootMenuText: %s", // 164
    "\
Existing mbldr is configured to use custom %skey codes.\n\
It is unable to properly parse boot menu text so all labels\n\
will be lost. You need to type them again or use custom boot\n\
menu.", // 165
    "Partition %u: offset %u", // 166
    "Partition's description: '%s'", // 167
    "New MBR contents:", // 168

    /* Strings for mbldrcli/mbldrgui modules */
    "Press Enter.", // 169
    "Yes, agree, go ahead", // 170
    "No, please cancel the operation", // 171
    "Choose the answer", // 172
    "User's choice: '%s'", // 173
    "Author:", // 174
    "More info can be found at:", // 175
    "No devices has been found, try to explicitly specify device with '-d' option", // 176
    "Only one suitable device was found and will be used: ", // 177
    "No partitions have been found", // 178
    "Quit immediately", // 179
    "Choose a device", // 180
    "User chooses immediate quit.", // 181
    "User's choice: '%s' converted to integer %li.", // 182
    "The number of partitions reached its maximum (9). You can't add another one.", // 183
    "Partitions that are already in the bootable list are marked with asterisks.", // 184
    "Partition %u is found in current configuration at slot %u", // 185
    "PRI", // 186
    "EXT", // 187
    "SectorOffset: 0x%08lX", // 188
    "SectorSize: 0x%08lX", // 189
    "Tb", // 190
    "Gb", // 191
    "Mb", // 192
    "Kb", // 193
    "'Skip boot attempt' is found in slot %u", // 194
    "SKIP BOOT", // 195
    "'Boot from next HDD' is found in slot %u", // 196
    "NEXT HDD", // 197
    "Do not add anything at this time", // 198
    "Choose what do you want to add", // 199
    "Do nothing, just go back from this menu.", // 200
    "'Skip boot' menu item has been added.", // 201
    "\
'Next HDD' menu item adds the following capability to the\n\
boot loader: after choosing this menu item mbldr tries to\n\
load MBR from another hard drive (number of drive is increased\n\
sequentially: 0x80, 0x81, etc) and execute it. New master boot\n\
loader gets a number of drive being used as a parameter.\n\
Theoretically this scheme should work fine assuming that all\n\
boot loaders respect this input parameter. Unfortunately\n\
several boot loaders ignore it assuming that the drive number\n\
used to boot is always 0x80 (what is incorrect according to\n\
EDD3 specification). It may lead to impossibility to boot for\n\
so called 'passive' master boot loaders (that just try to boot\n\
not updating MBR every time you boot). But even worse for so\n\
called 'active' master boot loaders (that modify MBR every time\n\
you boot) you may overwrite MBR on the wrong disk. Please\n\
note that 'Next HDD' feature is not the same as the one\n\
provided by BIOS (boot from either of installed hard disks).\n\
BIOS does the reenumeration of hard drives making bootable\n\
disk always 0x80, while mbldr does not. Use this feature at\n\
your own risk! It is dangerous and may lead to data loss if\n\
you have master boot loaders others than mbldr with version\n\
1.37 or above!\n\
Are you sure?", // 202
    "Are you sure?", // 203
    "'Next HDD' menu item has been added.", // 204
    "Type the label for this partition: ", // 205
    "Unable to get current active codepage", // 206
    "Label of partition is set to '%s'", // 207
    "Partition has been added.", // 208
    "\
No partitions have been found in current configuration.\n\
You should add at least one.", // 209
    "Default partition is what was booted last.", // 210
    "Default partition is marked with asterisk.", // 211
    "Partition in the slot %u is the default one", // 212
    "Skip boot attempt and try another device", // 213
    "Try to boot from next hard disk (dangerous!)", // 214
    "Broken partition offset 0x%08lX is found in slot %u", // 215
    "!!!BROKEN!!!", // 216
    "Partition offset points to a wrong sector, remove it", // 217
    "No default partition (load what was used on previous boot)", // 218
    "Choose partition or menu item", // 219
    "The default partition has been reset. It will point to what was booted last.", // 220
    "Remove this partition or boot menu item", // 221
    "Set/edit text label for partition or boot menu item", // 222
    "Set this partition as a default", // 223
    "What do you want to do?", // 224
    "Using device %s", // 225
    "User wants to remove this partition or menu item.", // 226
    "Decreasing number of default partition, now it's %u.", // 227
    "Partition has been removed from configuration.", // 228
    "List is now empty.", // 229
    "User wants to set or edit text label for boot menu.", // 230
    "custom", // 231
    "automatic", // 232
    "User wants to set this partition as a default.", // 233
    "Partition %i is set as a default one.", // 234
    "Use <Esc> key to interrupt timer", // 235
    "Use <Space> key to interrupt timer", // 236
    "Choose timer interrupt key", // 237
    "Currently progress-bar is not allowed.", // 238
    "Current progress-bar type: ", // 239
    "Decreasing digits", // 240
    "Asterisks", // 241
    "Equal signs", // 242
    "Greater signs", // 243
    "Dots", // 244
    "Number signs", // 245
    "Pluses", // 246
    "Minuses", // 247
    "Custom symbols", // 248
    "Disabled", // 249
    "Choose progress-bar type", // 250
    "Type one custom symbol for progress-bar", // 251
    "Timed boot is not allowed.", // 252
    "Timed boot is: ", // 253
    "Timer interrupt key:", // 254
    "Esc", // 255
    "Space", // 256
    "Timeout: %lusec", // 257
    "Default partition number: %u, label: '%s'", // 258
    "Timeout value (in seconds) should be between 1 and 36000", // 259
    "Immediate boot not waiting for user's input", // 260
    "Do not use timed boot. Wait for user's choice infinitely", // 261
    "Manage timer interrupt key", // 262
    "Configure progress-bar parameters", // 263
    "Choose timeout", // 264
    "Timed boot configuration is done.", // 265
    "\
Custom mode of boot menu text is active.\n\
Regenerate boot menu if you want to return to automatic mode.", // 266
    "Review boot menu text", // 267
    "Save boot menu text to a file for custom editing", // 268
    "Load custom boot menu text from a file", // 269
    "Clear boot menu text to simulate hidden mode", // 270
    "Regenerate menu automatically based on internal structures", // 271
    "Maximum number of characters for boot menu text", // 272
    "Length of current boot menu text (including trailing 0)", // 273
    "Number of available characters", // 274
    "Current boot menu text:", // 275
    "Enter the name of a file (existing file will be overwritten): ", // 276
    "Error saving file.", // 277
    "Error saving file, it seems there is no space left on a device.", // 278
    "File has been successfully written.", // 279
    "Error opening file.", // 280
    "Enter the name of existing file: ", // 281
    "Error reading file.", // 282
    "Boot menu text has been truncated because file is too long.", // 283
    "File has been successfully read.", // 284
    "Boot menu is now empty.", // 285
    "Boot menu has been regenerated.", // 286
    "\
Custom mode of boot menu text is inactive.\n\
Text will be updated automatically.", // 287
    "ASCII codes detection mode is active.", // 288
    "Keys: '1', '2', etc.", // 289
    "Scan-codes detection mode is active.", // 290
    "Keys: 'F1', 'F2', etc.", // 291
    "Code of base key=0x%02X.", // 292
    "Use '1', '2', etc. to boot from desired partition", // 293
    "Use 'F1', 'F2', etc. to boot from desired partition", // 294
    "Use arbitrary ASCII code as a base key", // 295
    "Use arbitrary scan-code as a base key", // 296
    "Boot will be activated by pressing '1', '2', etc.", // 297
    "Boot will be activated by pressing 'F1', 'F2', etc. ", // 298
    "\
This allows you to choose which keys will be used to boot\n\
operating systems from a list. In the mbldr assembly code\n\
that resides in MBR there is an ability to analyze either\n\
an ASCII code of the particular key being pressed or a\n\
scan-code. All operating systems (partitions) in a list\n\
are associated with incrementing list of ASCII/scan-codes\n\
so there is no ability to define particular key for each\n\
operating system. You are limited to choose a base code\n\
for the first partition in a list. All others will be\n\
identified with an appropriately increased ASCII/scan\n\
code (initial code + the offset of partition in a list).\n\
Please note that some keys on a keyboard generate neither\n\
ASCII code nor scan-code (best example is multimedia keys),\n\
so you could not use them. Also after specifying custom\n\
key code your boot menu text lacks key labels for\n\
partitions in the list. In this case you (most probably)\n\
should use custom boot menu text.", // 299
    "You are about to define custom ASCII code for the base key.", // 300
    "You are about to define custom scan-code for the base key.", // 301
    "Enter the decimal value of the code (0-255)", // 302
    "Custom code is set.", // 303
    "Backup existing MBR", // 304
    "Restore MBR from a backup", // 305
    "file", // 306
    "sector", // 307
    "File is too long.", // 308
    "Magic number 0xAA55 is not found, hard disk may not be bootable.", // 309
    "\
Several partitions have changed visibility (hidden became visible and/or\n\
vice versa). It is safe to restore old values from backup.", // 310
    "Keep visibility as in current MBR", // 311
    "Update visibility as in the backup", // 312
    "Overwrite current partition table, restore whole MBR from backup", // 313
    "Keep current partition table, restore only boot loader from backup", // 314
    "Master boot record has been successfully written.", // 315
    "Cancelled. Master boot record is left untouched.", // 316
    "View list of available partitions and add to current configuration", // 317
    "Manage bootable partitions (view list, remove, set default, set label)", // 318
    "Set timed boot parameters (timeout value, timer interrupt key, progress-bar)", // 319
    "Manage current boot menu text", // 320
    "Define keys choosing what to boot", // 321
    "Backup/restore MBR and save current configuration", // 322
    "Logging finished", // 323
    "mbldr: Error", // 324
    "mbldr: Information", // 325
    "mbldr: Question", // 326
    "mbldr: Main configurator", // 327
    "Available partitions:", // 328
    "A&dd >", // 329
    "< R&emove", // 330
    "Configured partitions:", // 331
    "Timed boot configuration:", // 332
    "Allowed (user timer)", // 333
    "Never", // 334
    "Immediate boot", // 335
    "seconds", // 336
    "Progress-bar:", // 337
    "Label:", // 338
    "Default:", // 339
    "Mar&k current", // 340
    "Last b&ooted", // 341
    "Keys choosing what to boot:", // 342
    "1, 2, 3...", // 343
    "F1, F2, F3...", // 344
    "Custom ASCII code", // 345
    "Custom scan-code", // 346
    "decimal key code", // 347
    "Boot menu text:", // 348
    "&Save...", // 349
    "&Load...", // 350
    "&Clear", // 351
    "&Generate", // 352
    "&Backup MBR...", // 353
    "&Restore MBR...", // 354
    "Save new created mbldr configuration", // 355
    "Save &mbldr...", // 356
    "&About", // 357
    "&Quit", // 358
    "mbldr: Choose device", // 359
    "Please specify what do you want to configure:", // 360
    "Devices representing hard disk drives:", // 361
    "Files representing images of hard disks:", // 362
    "Open &Device", // 363
    "&Browse...", // 364
    "Open &File", // 365
    "\
Windows 95/98/ME is not supported by GUI verion of mbldr installation program.\n\
Please use command-line version of mbldr (mbldrcli) for DOS/Windows 95/98/ME", // 366
    "Show frame of DeviceChoice.", // 367
    "Show frame of MainConfigurator.", // 368
    "Allowed (system timer)", // 369
    "Switch between user and system timers", // 370
    "Switching from user timer to system timer.", // 371
    "Switching from system timer to user timer.", // 372
    "Jump offset=0x%02X, other primary partitions should be hidden at boot attempt", // 373
    "Jump offset=0x%02X, other primary partitions should be left in original state at boot attempt", // 374
    "Unrecognized jump offset=0x%02X, so let other partitions be hidden at boot attempt", // 375
    "Hide other primary partitions at boot time", // 376
    "Don't hide other primary partitions at boot time", // 377
    "Other primary partitions will be hidden at the time of boot attempt.", // 378
    "Other primary partitions will be left in their original state at boot attempt.", // 379
    "drive(%i): ", // 380
    "No active (bootable) partitions have been found, the hard disk may not be bootable.", // 381
    "Mark primary partitions with active flag", // 382
    "Active flag on all primary partitions will be left intact at boot time", // 383
    "Primary partition being booted will be marked as active, all others will be marked inactive at boot time", // 384
    "Active flag of primary partitions will be left intact (NOP opcodes found)", // 385
    "Primary partitions will be marked active/inactive, opcodes: 0x%02X and 0x%02X", // 386
    "Mark primary partitions active/inactive at boot time", // 387
    "Leave active flag of primary partitions intact", // 388
    "\
The value of bootable entry usually corresponds to an offset of partition to\n\
boot. But you may modify it manually (for example to chain-load a copy of\n\
another MBR located somewhere). This is an expert feature, don't change this\n\
value if you don't know what you are doing!\n\n\
Enter a positive decimal value representing the offset in sectors:", // 389
    "The offset of bootable partition has been modified", // 390
    "CHAINLOAD", // 391
    "Offset points to a sector on a first track", // 392
    "Manually modify the offset", // 393
    "User wants to manually modify the offset", // 394
    "\
Backup, restore and save operations will deal with files. So, 'backup MBR'\n\
will save it to a file, and 'save current configuration of mbldr' will also\n\
write it to a file (not to MBR on a chosen hard drive).", // 395
    "\
Backup, restore and save operations will deal with sectors on a disk. So,\n\
'backup MBR' will save the MBR to a sector of your choice (can be used to\n\
prepare data for chainloading). 'Save current configuration of mbldr' will\n\
also write it to a sector (MBR by default as it is most useful operation).", // 396
    "Source or target for backup, restore and save operations", // 397
    "\
Enter a number of target sector to write the data to (0 means MBR, values in\n\
range of [1-62] usually correspond to sectors on a very first track, so they\n\
can be used for chainloading if not occupied). For your information, the list\n\
of free (filled by zeroes) sectors on a first track:\n", // 398
    "Target sector has been successfully written.", // 399
    "Cancelled. Target sector is left untouched.", // 400
    "\
The sector you have chosen is outside of the first track. This can\n\
be dangerous to write the data there and may lead to corruption of\n\
data integrity. Are you sure?", // 401
    "Do you want to add a chainload entry to the list of bootable partitions?", // 402
    "\
Enter a number of source sector to read the data from (0 means MBR, values in\n\
range of [1-62] usually correspond to sectors on a very first track, so they\n\
can be used for chainloading if not occupied). For your information, the list\n\
of occupied (bootable - having a 0x55AA signature at the end) sectors on a\n\
first track:\n", // 403
    "\
The sector you have chosen is outside of the first track. This space is\n\
usually occupied by partitions, so MBR backup should not be stored there.\n\
Are you sure?", // 404
    "Add 0xAA55 signature to the end of the backup sector", // 405
    "Leave backup sector with no signature", // 406
    "\
New partition(s) appeared in current MBR comparing to backup.\n\
It is highly recommended to keep current partition table from your MBR\n\
since restoring from backup may corrupt hard disk partitioning.", // 407
    "\
Old partition(s) disappeared from current MBR comparing to backup.\n\
It is highly recommended to keep current partition table from your MBR\n\
since restoring from backup may corrupt hard disk partitioning.", // 408
    "\
Partitions in current MBR have different geometry comparing to backup.\n\
It is highly recommended to keep current partition table from your MBR\n\
since restoring from backup may corrupt hard disk partitioning.", // 409
    "\
Windows hard-disk signature is different in current MBR and backup.\n\
It is better to keep what you have in MBR if you use Windows operating system.", // 410
    "Keep hard-disk signature from current MBR", // 411
    "Overwrite hard-disk signature with the version from backup", // 412
    "\
Different partitions are marked as actve in your current MBR and the backup.\n\
It is safe to restore old values from backup.", // 413
    "Keep active partition as in current MBR", // 414
    "Update active partition as in the backup", // 415
    "\
Partitions in current MBR have different identifiers comparing to backup.\n\
It is highly recommended to keep current partition table from your MBR\n\
since restoring from backup may corrupt hard disk partitioning." // 416
};

// Get a message from array of messages for logging and user interface
// with possible translation
char* CommonMessage
    (
    unsigned int uiMessageIndex
    )
{
    if ( strcmp ( ( char* )( Messages[ uiMessageIndex ] ),"" ) == 0 )
    {
        // Return empty string if it was requested
        return ( ( char* )( Messages[ uiMessageIndex ] ) );
    }
    else
    {
        // Perform string translation
        return ( gettext ( ( char* )( Messages[ uiMessageIndex ] ) ) );
    }
}

// Parse command-line and process all options. Returns 0 if the program should
// be continued, 1 otherwise (in case if parameters contain request to
// command-line help). argc and argv[] parameters should be the same to what
// is passed to main() function of a program
int CommonParseCommandLine
    (
    int argc,
    char *argv[]
    )
{
    // Command line option for getopt() parser
    // Note: here we do not use getopt_long since it is incompatible with
    // current version of DJGPP and old versions of FreeBSD (at least some 4.x)
    int option;

    // Before trying getopt() try more common command line switches
    if ( argc == 2 )
    {
        if (
#if defined ( __unix__ )
            ( strcmp ( argv[ 1 ],"--help" ) == 0 ) || // Standard Unix help switch
#else
            ( strcmp ( argv[ 1 ],"/?" ) == 0 ) ||     // Standard DOS/Windows help switch
            ( strcmp ( argv[ 1 ],"/h" ) == 0 ) ||     // Alternative DOS/Windows help switch
#endif /* __unix__ */
#if defined ( _WIN32 )
            ( strcmp ( argv[ 1 ],"/$" ) == 0 ) ||     // '/?' switch is sometimes magically
                                                      // automatically converted to '/$'
                                                      // under Windows for some reasons
#endif /* _WIN32 */
            ( strcmp ( argv[ 1 ],"-?" ) == 0 )        // getopt does not handle this
           )
        {
            Log ( DEBUG,CommonMessage ( 100 ) );
            // Call mbldrcli/mbldrgui-specific about function
            MbldrAbout ( CommonMessage ( 92 ),
                         MBLDR_VERSION,
                         CommonMessage ( 93 ),
                         CommonMessage ( 94 ),
                         CommonMessage ( 95 ),
                         CommonMessage ( 96 ),
                         CommonMessage ( 97 ),
                         CommonMessage ( 98 ),
                         CommonMessage ( 99 ) );
            // Request termination of a program
            return ( 1 );
        }
    }

    // Set opterr to 0 preventing getopt function from printing error messages
    // to stderr in case of unknown option
    // The special option `--' indicates that no more options follow on the
    // command line, and cause `getopt' to stop looking.
    opterr = 0;

    // Loop for command line parsing
    while ( ( option=getopt ( argc,argv,"hvd:" ) ) != -1 )
    {
        Log ( DEBUG,CommonMessage ( 101 ),option );
        switch ( option )
        {
            // Command line help
            case 'h':
            {
                Log ( DEBUG,CommonMessage ( 100 ) );
                // Call mbldrcli/mbldrgui-specific about function
                MbldrAbout ( CommonMessage ( 92 ),
                             MBLDR_VERSION,
                             CommonMessage ( 93 ),
                             CommonMessage ( 94 ),
                             CommonMessage ( 95 ),
                             CommonMessage ( 96 ),
                             CommonMessage ( 97 ),
                             CommonMessage ( 98 ),
                             CommonMessage ( 99 ) );
                // Request termination of a program
                return ( 1 );
            }
            // Debug verbosity level
            case 'v':
            {
                LogSetLevel ( DEBUG );
                Log ( DEBUG,CommonMessage ( 102 ) );
                break;
            }
            // Device name
            case 'd':
            {
                if (( strlen ( optarg ) == 0 ) || ( strlen ( optarg ) >= 256 ))
                {
                    Log ( FATAL,CommonMessage ( 103 ),optarg );
                }
#if defined ( _WIN32 )
                strcpy ( Device,"\\\\.\\PhysicalDrive" );
                if ( strcasecmp ( optarg,"0" ) == 0 )
                {
                    // This is a first hard disk
                    strcat ( Device,"0" );
                }
                else if (
                         ( strlen ( optarg ) <= 3 ) &&
                         ( atoi ( optarg ) >= 1 ) &&
                         ( atoi ( optarg ) <= 127 )
                        )
                {
                    // This is a hard disk
                    strcat ( Device,optarg );
                }
                else
                {
                    // This seems to be a file name
                    strcpy ( Device,optarg );
                }
#else
                strcpy ( Device,optarg );
#endif /* _WIN32 */
                Log ( DEBUG,CommonMessage ( 104 ),Device );
                break;
            }
            case '?':
            default:
            {
                Log ( FATAL,CommonMessage ( 105 ),optopt );
            }
       }
    }
    // Allow program to be continued
    return ( 0 );
}

// Exchanges two partition entries to sort out the array of found partitions
void CommonSwapPartitionEntries
    (
    struct FoundPartitionEntry* pPartitionEntry1,
    struct FoundPartitionEntry* pPartitionEntry2
    )
{
    struct FoundPartitionEntry TempPartitionEntry;

    TempPartitionEntry.partition_identifier = pPartitionEntry1->partition_identifier;
    TempPartitionEntry.is_primary = pPartitionEntry1->is_primary;
    TempPartitionEntry.relative_sectors_offset = pPartitionEntry1->relative_sectors_offset;
    TempPartitionEntry.size_in_sectors = pPartitionEntry1->size_in_sectors;

    pPartitionEntry1->partition_identifier = pPartitionEntry2->partition_identifier;
    pPartitionEntry1->is_primary = pPartitionEntry2->is_primary;
    pPartitionEntry1->relative_sectors_offset = pPartitionEntry2->relative_sectors_offset;
    pPartitionEntry1->size_in_sectors = pPartitionEntry2->size_in_sectors;

    pPartitionEntry2->partition_identifier = TempPartitionEntry.partition_identifier;
    pPartitionEntry2->is_primary = TempPartitionEntry.is_primary;
    pPartitionEntry2->relative_sectors_offset = TempPartitionEntry.relative_sectors_offset;
    pPartitionEntry2->size_in_sectors = TempPartitionEntry.size_in_sectors;
}

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
    )
{
    // Analysed sector of chosen hard disk (including MBR and partition table)
    unsigned char Sector[ 512 ];
    // Number of partition entries that could be stored (4 for MBR, 2 for extended)
    unsigned char MaximumEntries;
    // Counter for the cycle enumerating partition entries
    unsigned char EntryNumber;
    // Number of sector pointing to the extended partition (zero means no
    // extended partition has been found (yet))
    unsigned long SectorOfExtendedPartition = 0;

    if ( SectorNumber == 0 )
    {
        // In the master boot record 4 partitions could be listed at maximum
        MaximumEntries = 4;
    }
    else
    {
        // In externded boot records only 2 partitions could be listed
        MaximumEntries = 2;
    }

    Log ( DEBUG,CommonMessage ( 106 ),SectorNumber );
    DisksReadSector ( SectorNumber,Sector );
    // Check magic word
    if (( Sector[ 510 ] != 0x55 ) || ( Sector[ 511 ] != 0xAA ))
    {
        Log ( FATAL,CommonMessage ( 107 ) );
    }

    // Check that at least one partition is marked as active (bootable)
    if ( SectorNumber == 0 )
    {
        // We check only highest bit on every partition (actually they could either be
        // equal to 0x00 or 0x80, but in theory they also can be 0x81, 0x82, etc.)
        if (
            ( ( ( Sector[ 446 + 16*0 ] & 0x80 ) != 0x80 ) || ( Sector[ 446 + 16*0 + 4 ] == 0 ) ) &&
            ( ( ( Sector[ 446 + 16*1 ] & 0x80 ) != 0x80 ) || ( Sector[ 446 + 16*1 + 4 ] == 0 ) ) &&
            ( ( ( Sector[ 446 + 16*2 ] & 0x80 ) != 0x80 ) || ( Sector[ 446 + 16*2 + 4 ] == 0 ) ) &&
            ( ( ( Sector[ 446 + 16*3 ] & 0x80 ) != 0x80 ) || ( Sector[ 446 + 16*3 + 4 ] == 0 ) )
           )
        {
            MbldrShowInfoMessage ( CommonMessage ( 381 ) );
        }
    }

    // Partitions detection cycle
    for ( EntryNumber=0 ; EntryNumber<MaximumEntries ; EntryNumber++ )
    {
        // Check partition identifier
        if (( Sector[ 446 + 16*EntryNumber + 4 ] == 0x05 ) || ( Sector[ 446 + 16*EntryNumber + 4 ] == 0x0F ))
        {
            Log ( DEBUG,CommonMessage ( 108 ),Sector[ 446 + 16*EntryNumber + 4 ] );
            if ( SectorOfExtendedPartition != 0 )
            {
                // There could be only one extended partition
                Log ( FATAL,CommonMessage ( 109 ) );
            }
            // Save sector number
            SectorOfExtendedPartition = *( ( unsigned long* )&( Sector[ 446 + 16*EntryNumber + 8 ] ));
            if ( SectorNumber == 0 )
            {
                Log ( DEBUG,CommonMessage ( 110 ),SectorOfExtendedPartition );
                ExtendedPartitionBegin = SectorOfExtendedPartition;
            }
            else
            {
                Log ( DEBUG,CommonMessage ( 111 ),ExtendedPartitionBegin,SectorOfExtendedPartition );
                SectorOfExtendedPartition += ExtendedPartitionBegin;
            }
        }
        // If partition identifier is 0, it is unused, skip this and do nothing
        else if ( Sector[ 446 + 16*EntryNumber + 4 ] != 0x00 )
        {
            // Treat any partition as bootable
            FoundPartitions[ NumberOfFoundPartitions ].partition_identifier = Sector[ 446 + 16*EntryNumber + 4 ];
            Log ( DEBUG,CommonMessage ( 112 ),FoundPartitions[ NumberOfFoundPartitions ].partition_identifier );
            FoundPartitions[ NumberOfFoundPartitions ].relative_sectors_offset = SectorNumber + *( ( unsigned long* )&( Sector[ 446 + 16*EntryNumber + 8 ] ));
            Log ( DEBUG,CommonMessage ( 113 ),FoundPartitions[ NumberOfFoundPartitions ].relative_sectors_offset );
            FoundPartitions[ NumberOfFoundPartitions ].size_in_sectors = *( ( unsigned long* )&( Sector[ 446 + 16*EntryNumber + 12 ] ));
            Log ( DEBUG,CommonMessage ( 114 ),FoundPartitions[ NumberOfFoundPartitions ].size_in_sectors );
            if ( SectorNumber == 0 )
            {
                Log ( DEBUG,CommonMessage ( 115 ) );
                FoundPartitions[ NumberOfFoundPartitions ].is_primary = 1;
                // Here we need to sort found primary partitions by relative offset
                // because otherwise we may confuse a user since he expects to see
                // partitions located at the beginning of hard disk at the beginning
                // of the list. Without this sorting we will generate a list basing
                // on the location of partition record in a table, but not the
                // RelSec offset of partition itself. Sorting will only be activated
                // if there is more than one primary partition
                if ( NumberOfFoundPartitions == 1 )
                {
                    // 2 primary partitions
                    if ( FoundPartitions[ 1 ].relative_sectors_offset < FoundPartitions[ 0 ].relative_sectors_offset )
                    {
                        Log ( DEBUG,CommonMessage ( 116 ),"[AB]->[BA]" );
                        CommonSwapPartitionEntries ( &FoundPartitions[ 0 ],&FoundPartitions[ 1 ] );
                    }
                }
                else if ( NumberOfFoundPartitions == 2 )
                {
                    // 3 primary partitions
                    if ( FoundPartitions[ 2 ].relative_sectors_offset < FoundPartitions[ 0 ].relative_sectors_offset )
                    {
                        Log ( DEBUG,CommonMessage ( 116 ),"[ABC]->[CAB]" );
                        CommonSwapPartitionEntries ( &FoundPartitions[ 0 ],&FoundPartitions[ 2 ] );
                        CommonSwapPartitionEntries ( &FoundPartitions[ 1 ],&FoundPartitions[ 2 ] );
                    }
                    else if ( FoundPartitions[ 2 ].relative_sectors_offset < FoundPartitions[ 1 ].relative_sectors_offset )
                    {
                        Log ( DEBUG,CommonMessage ( 116 ),"[ABC]->[ACB]" );
                        CommonSwapPartitionEntries ( &FoundPartitions[ 1 ],&FoundPartitions[ 2 ] );
                    }
                }
                else if ( NumberOfFoundPartitions == 3 )
                {
                    // 4 primary partitions
                    if ( FoundPartitions[ 3 ].relative_sectors_offset < FoundPartitions[ 0 ].relative_sectors_offset )
                    {
                        Log ( DEBUG,CommonMessage ( 116 ),"[ABCD]->[DABC]" );
                        CommonSwapPartitionEntries ( &FoundPartitions[ 0 ],&FoundPartitions[ 3 ] );
                        CommonSwapPartitionEntries ( &FoundPartitions[ 1 ],&FoundPartitions[ 3 ] );
                        CommonSwapPartitionEntries ( &FoundPartitions[ 2 ],&FoundPartitions[ 3 ] );
                    }
                    else if ( FoundPartitions[ 3 ].relative_sectors_offset < FoundPartitions[ 1 ].relative_sectors_offset )
                    {
                        Log ( DEBUG,CommonMessage ( 116 ),"[ABCD]->[ADBC]" );
                        CommonSwapPartitionEntries ( &FoundPartitions[ 1 ],&FoundPartitions[ 3 ] );
                        CommonSwapPartitionEntries ( &FoundPartitions[ 2 ],&FoundPartitions[ 3 ] );
                    }
                    else if ( FoundPartitions[ 3 ].relative_sectors_offset < FoundPartitions[ 2 ].relative_sectors_offset )
                    {
                        Log ( DEBUG,CommonMessage ( 116 ),"[ABCD]->[ABDC]" );
                        CommonSwapPartitionEntries ( &FoundPartitions[ 2 ],&FoundPartitions[ 3 ] );
                    }
                }
            }
            else
            {
                Log ( DEBUG,CommonMessage ( 117 ) );
                FoundPartitions[ NumberOfFoundPartitions ].is_primary = 0;
            }
            NumberOfFoundPartitions++;
            // Check maximum amount of partitions
            if ( NumberOfFoundPartitions == 256 )
            {
                Log ( FATAL,CommonMessage ( 118 ) );
            }
        }
        else
        {
            Log ( DEBUG,CommonMessage ( 119 ) );
        }
    }
    if ( SectorOfExtendedPartition != 0 )
    {
        Log ( DEBUG,CommonMessage ( 120 ) );
        CommonDetectBootablePartitions ( SectorOfExtendedPartition );
    }
}

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
    )
{
    char TempBootMenuText[ 512 ] = "\n";
    char Buffer[ 128 ];
    int Count;
    // Result of recursive call of this function used to determine whether the
    // current menu text should be used
    int ResultOfNestedCall;

    Log ( DEBUG,CommonMessage ( 121 ),Verbosity );

    // Prohibit calling of this function with verbosity > 6 since it does not
    // provide more verbosity and may lead to recursion infinite loop
    if ( Verbosity > 6 )
    {
        Log ( DEBUG,CommonMessage ( 122 ) );
        return ( -1 );
    }

    // Check string length of boot menu text if custom menu is used
    if ( CustomBootMenuText != 0 )
    {
        Count = CommonGetMaximumSizeOfBootMenuText () - ( ( int )strlen ( BootMenuText ) + 1 );
        Log ( DEBUG,CommonMessage ( 123 ),Count );
        if ( Count < 0 )
        {
            MbldrShowInfoMessage ( CommonMessage ( 124 ), Count*( -1 ) );
        }
        return ( Count );
    }

    // Prohibit construction of boot menu text if not partitions have been added
    if ( NumberOfBootablePartitions == 0 )
    {
        strcpy ( BootMenuText,CommonMessage ( 125 ) );
        return ( 0 );
    }

    // Boot menu header
    if ( Verbosity == 6 )
    {
        Log ( DEBUG,CommonMessage ( 126 ) );
        sprintf ( Buffer,"mbldr v%s\r\n\n",MBLDR_VERSION );
        strcat ( TempBootMenuText,Buffer );
    }

    // Boot menu body
    Log ( DEBUG,CommonMessage ( 127 ) );
    for ( Count=0 ; Count<NumberOfBootablePartitions ; Count++ )
    {
        // Boot menu item number (what key to press for booting)
        if (
            ( ( UseAsciiOrScanCode == 0 ) && ( BaseAsciiOrScanCode == '1' ) ) ||
            ( ( UseAsciiOrScanCode == 1 ) && ( BaseAsciiOrScanCode == 0x3B ) )
           )
        {
            if ( ( UseAsciiOrScanCode == 1 ) && ( BaseAsciiOrScanCode == 0x3B ) )
            {
                strcat ( TempBootMenuText,"F" );
            }
            sprintf ( Buffer,"%u",Count + 1 );
            strcat ( TempBootMenuText,Buffer );
            if ( Verbosity > 4 )
            {
                // Dots after number increase appearance of boot menu
                strcat ( TempBootMenuText,"." );
            }
        }
        // Mark default partition
        if ( Count == NumberOfDefaultPartition )
        {
            strcat ( TempBootMenuText,"*" );
        }
        else
        {
            strcat ( TempBootMenuText," " );
        }
        // Append label
        if ( strcmp ( BootablePartitions[ Count ].label,"" ) != 0 )
        {
            strcat ( TempBootMenuText,BootablePartitions[ Count ].label );
        }
        else
        {
            if ( BootablePartitions[ Count ].relative_sectors_offset == 0 )
            {
                // The strings here must not be localized
                if ( Verbosity <= 1 )
                {
                    strcat ( TempBootMenuText,"Next HDD" );
                }
                else if ( Verbosity == 2 )
                {
                    strcat ( TempBootMenuText,"Try next HDD" );
                }
                else if ( Verbosity == 3 )
                {
                    strcat ( TempBootMenuText,"Next hard disk" );
                }
                else if ( Verbosity >= 4 )
                {
                    strcat ( TempBootMenuText,"Try next hard disk" );
                }
            }
            else if ( BootablePartitions[ Count ].relative_sectors_offset == 0xFFFFFFFFul )
            {
                // The strings here must not be localized
                if ( Verbosity <= 1 )
                {
                    strcat ( TempBootMenuText,"Skip" );
                }
                else if ( Verbosity == 2 )
                {
                    strcat ( TempBootMenuText,"Skip boot" );
                }
                else if ( Verbosity == 3 )
                {
                    strcat ( TempBootMenuText,"Skip HDDs boot" );
                }
                else if ( Verbosity >= 4 )
                {
                    strcat ( TempBootMenuText,"Skip hard disks boot" );
                }
            }
            else
            {
                Log ( DEBUG,CommonMessage ( 128 ) );
                // This string must not be localized
                strcat ( TempBootMenuText,"Unknown" );
            }
        }
        strcat ( TempBootMenuText,"\r\n" );
    }

    // Boot menu footer
    if ( Verbosity >= 1 )
    {
        // Output information about a key to interrupt timer only if timeout > 0
        // (no immediate boot) and timed boot is allowed
        if (( Timeout > 0 ) && ( TimedBootAllowed > 0 ))
        {
            Log ( DEBUG,CommonMessage ( 129 ) );
            // Determine the key
            strcat ( TempBootMenuText,"\n" );
            if ( TimerInterruptKey == 0x1B )
            {
                // Esc is a key to interrupt timer
                strcat ( TempBootMenuText,"ESC" );
            }
            else if ( TimerInterruptKey == 0x20 )
            {
                // Space is a key to interrupt timer
                strcat ( TempBootMenuText,"SPACE" );
            }
            else
            {
                Log ( FATAL,CommonMessage ( 130 ),TimerInterruptKey );
            }

            // Determine timeout measurement units
            if ( Timeout < 60*18 )
            {
                // If timeout is less than 1 minute, measure in seconds
                if ( Verbosity >= 5 )
                {
                    // This string must not be localized
                    sprintf ( Buffer," stops %lusec timer\r\n",Timeout/18 );
                }
                else
                {
                    // This string must not be localized
                    sprintf ( Buffer," stops %lus timer\r\n",Timeout/18 );
                }
            }
            else
            {
                // If timeout is greater or equal than 1 minute, measure in minutes
                if ( Verbosity >= 5 )
                {
                    // This string must not be localized
                    sprintf ( Buffer," stops %lumin timer\r\n",Timeout/18/60 );
                }
                else
                {
                    // This string must not be localized
                    sprintf ( Buffer," stops %lum timer\r\n",Timeout/18/60 );
                }
            }
            strcat ( TempBootMenuText,Buffer );
        }
        else if ( Verbosity >= 2 )
        {
            // This LF is used to separate boot menu footer from the body
            strcat ( TempBootMenuText,"\n" );
        }
    }
    if ( Verbosity >= 2 )
    {
        Log ( DEBUG,CommonMessage ( 131 ) );
        // This string must not be localized
        strcat ( TempBootMenuText,"ENTER boots default\r\n" );
    }
    if ( Verbosity >= 3 )
    {
        Log ( DEBUG,CommonMessage ( 132 ) );

        // These strings should not be localized
        if ( ( UseAsciiOrScanCode == 0 ) && ( BaseAsciiOrScanCode == '1' ) )
        {
            // ASCII-codes mode ('1', '2', etc.)
            strcat ( TempBootMenuText,"Digits boot OS\r\n" );
        }
        else if ( ( UseAsciiOrScanCode == 1 ) && ( BaseAsciiOrScanCode == 0x3B ) )
        {
            // Scan-codes mode ('F1', 'F2', etc.)
            strcat ( TempBootMenuText,"F-keys boot OS\r\n" );
        }
        else
        {
            strcat ( TempBootMenuText,"Press a key to boot OS\r\n" );
        }

    }

    // Frame for progress-bar
    if ( ProgressBarAllowed == 1 )
    {
        if ( Verbosity >= 4 )
        {
            Log ( DEBUG,CommonMessage ( 133 ) );
            strcat ( TempBootMenuText,PROGRESS_BAR_STRING );
        }
    }

    // Check size of boot menu text
    Count = MBLDR_WINDOWS_DISK_SIGNATURE - 4*NumberOfBootablePartitions - MBLDR_BOOT_MENU_TEXT - ( strlen ( TempBootMenuText ) + 1 );
    Log ( DEBUG,CommonMessage ( 134 ),Count );
    if ( Count >= 0 )
    {
        ResultOfNestedCall = CommonConstructBootMenuText ( Verbosity + 1 );
        Log ( DEBUG,CommonMessage ( 135 ),ResultOfNestedCall );
        if ( ResultOfNestedCall >= 0 )
        {
            return ( ResultOfNestedCall );
        }
        Log ( DEBUG,CommonMessage ( 136 ) );
        strcpy ( BootMenuText,TempBootMenuText );
    }
    if (( Verbosity == 0 ) && ( Count < 0 ))
    {
        MbldrShowInfoMessage ( CommonMessage ( 137 ),Count*( -1 ) );
        sprintf ( BootMenuText,CommonMessage ( 137 ),Count*( -1 ) );
    }

    return ( Count );
}

// Detects whether the mbldr is installed on the target hard disk
void CommonCheckMBR
    (
    void
    )
{
    // First sector of chosen hard disk (including MBR and partition table)
    unsigned char MBRImage[ 512 ];
    unsigned short Count;
    unsigned char mbldr_found = 1;
    // Text buffer representing user's input
    char Buffer[ 128 ];
    // Pointer to string inside boot menu text (used to locate partition description)
    char* pLabel;

    // Request MBR from the chosen disk
    DisksReadSector ( 0,MBRImage );
    Log ( DEBUG,CommonMessage ( 138 ) );
    for ( Count=0 ; Count<64 ; Count++ )
    {
        Log ( DEBUG,"0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
            MBRImage[ Count*8 + 0 ],MBRImage[ Count*8 + 1 ],
            MBRImage[ Count*8 + 2 ],MBRImage[ Count*8 + 3 ],
            MBRImage[ Count*8 + 4 ],MBRImage[ Count*8 + 5 ],
            MBRImage[ Count*8 + 6 ],MBRImage[ Count*8 + 7 ] );
    }
    if ( mbldr_bin_len != 512 )
    {
        Log ( FATAL,CommonMessage ( 139 ),mbldr_bin_len );
    }
    for ( Count=0 ; Count<512 ; Count++ )
    {
        if ( mask_bin[ Count ] == 0x00 )
        {
            if ( mbldr_bin[ Count ] != MBRImage[ Count ] )
            {
                Log ( DEBUG,CommonMessage ( 140 ),Count,MBRImage[ Count ],mbldr_bin[ Count ] );
                mbldr_found = 0;
                break;
            }
        }
        else
        {
            if ( mask_bin[ Count ] != 0xFF )
            {
                Log ( FATAL,CommonMessage ( 141 ),Count,mask_bin[ Count ] );
            }
        }
    }
    if ( mbldr_found == 1 )
    {
        Log ( DEBUG,CommonMessage ( 142 ) );
        if ( MbldrYesNo ( CommonMessage ( 143 ) ) != 0 )
        {
            Log ( DEBUG,CommonMessage ( 144 ) );
            mbldr_found = 1;
        }
        else
        {
            Log ( DEBUG,CommonMessage ( 145 ) );
            mbldr_found = 0;
        }
    }
    if ( mbldr_found == 1 )
    {
        // If mbldr was found and user wants to configure it
        // Save old parameters of the existing mbldr

        // Either Esc or Space ASCII code to interrupt timer
        // Should be ignored if timed boot is disabled
        TimerInterruptKey = MBRImage[ MBLDR_TIMER_INTERRUPT_KEY ];
        if (( TimerInterruptKey != 0x1B ) && ( TimerInterruptKey != 0x20 ))
        {
            Log ( FATAL,CommonMessage ( 146 ),TimerInterruptKey );
        }
        Log ( DEBUG,CommonMessage ( 147 ),TimerInterruptKey );

        // Number of bootable partitions (operating systems)
        NumberOfBootablePartitions = MBRImage[ MBLDR_NUMBER_OF_BOOTABLE_PARTITIONS ];
        // Should be between 1 and 9
        if (( NumberOfBootablePartitions == 0 ) || ( NumberOfBootablePartitions > 9 ))
        {
            Log ( FATAL,CommonMessage ( 148 ),NumberOfBootablePartitions );
        }
        Log ( DEBUG,CommonMessage ( 149 ),NumberOfBootablePartitions );

        // Number of default partition to boot
        if (
            ( MBRImage[ MBLDR_MODIFY_DEFAULT_PARTITION_OPCODE + 0 ] == 0x90 ) &&
            ( MBRImage[ MBLDR_MODIFY_DEFAULT_PARTITION_OPCODE + 1 ] == 0x90 ) &&
            ( MBRImage[ MBLDR_MODIFY_DEFAULT_PARTITION_OPCODE + 2 ] == 0x90 )
           )
        {
            // There is a default partition
            NumberOfDefaultPartition = MBRImage[ MBLDR_NUMBER_OF_DEFAULT_PARTITION ];
            // A number between 1 and NumberBootablePartitions
            if ( NumberOfDefaultPartition >= NumberOfBootablePartitions )
            {
                Log ( FATAL,CommonMessage ( 150 ),NumberOfDefaultPartition );
            }
        }
        else
        {
            // Default partition is a last one being booted
            NumberOfDefaultPartition = 255;
        }
        Log ( DEBUG,CommonMessage ( 151 ),NumberOfDefaultPartition );

        // ASCII or scan code is analyzed
        if (
            ( MBRImage[ MBLDR_USE_ASCII_OR_SCAN_OPCODE + 0 ] == 0x90 ) &&
            ( MBRImage[ MBLDR_USE_ASCII_OR_SCAN_OPCODE + 1 ] == 0x90 )
           )
        {
            // ASCII code
            UseAsciiOrScanCode = 0;
            Log ( DEBUG,CommonMessage ( 152 ) );
        }
        else
        {
            // Scan-code
            UseAsciiOrScanCode = 1;
            Log ( DEBUG,CommonMessage ( 153 ) );
        }
        // And the base code itself (for the first OS in the list)
        BaseAsciiOrScanCode = MBRImage[ MBLDR_BASE_ASCII_OR_SCAN_CODE ];
        Log ( DEBUG,CommonMessage ( 154 ),BaseAsciiOrScanCode );

        // Check the type of progress-bar and it's symbol
        if (
            ( MBRImage[ MBLDR_PROGRESS_BAR_SYMBOL_OPCODE + 0 ] == 0x88 ) &&
            ( MBRImage[ MBLDR_PROGRESS_BAR_SYMBOL_OPCODE + 1 ] == 0xF0 )
           )
        {
            // Progress-bar looks like "[9876543210]"
            ProgressBarSymbol = 0;
            Log ( DEBUG,CommonMessage ( 155 ) );
        }
        else
        {
            // Progress-bar looks like "[**********]"
            ProgressBarSymbol = MBRImage[ MBLDR_PROGRESS_BAR_SYMBOL_OPCODE + 1 ];
            Log ( DEBUG,CommonMessage ( 156 ),MBRImage[ MBLDR_PROGRESS_BAR_SYMBOL_OPCODE + 1 ] );
        }

        // Check whether the progress-bar is allowed
        if (
            ( MBRImage[ MBLDR_PROGRESS_BAR_SWITCH_OPCODE + 0 ] == 0x90 ) &&
            ( MBRImage[ MBLDR_PROGRESS_BAR_SWITCH_OPCODE + 1 ] == 0x90 )
           )
        {
            // Progress-bar is disabled
            ProgressBarAllowed = 0;
            Log ( DEBUG,CommonMessage ( 157 ) );
        }
        else
        {
            // Progress-bar is enabled
            ProgressBarAllowed = 1;
            Log ( DEBUG,CommonMessage ( 158 ) );
        }

        // Check whether mbldr should hide other primary partitions at boot attempt
        if ( MBRImage[ MBLDR_HIDING_JUMP_OFFSET ] == 0x02 )
        {
            // Hide other primary partitions
            HideOtherPrimaryPartitions = 1;
            Log ( DEBUG,CommonMessage ( 373 ),MBRImage[ MBLDR_HIDING_JUMP_OFFSET ] );
        }
        else if ( MBRImage[ MBLDR_HIDING_JUMP_OFFSET ] == 0x05 )
        {
            // Don't hide other primary partitions
            HideOtherPrimaryPartitions = 0;
            Log ( DEBUG,CommonMessage ( 374 ),MBRImage[ MBLDR_HIDING_JUMP_OFFSET ] );
        }
        else
        {
            // Unexpected value, however it is more safe to hide other primary partitions
            HideOtherPrimaryPartitions = 1;
            Log ( DEBUG,CommonMessage ( 375 ),MBRImage[ MBLDR_HIDING_JUMP_OFFSET ] );
        }

        // Check whether mbldr should mark primary partitions active/inactive at boot attempt
        if (( MBRImage[ MBLDR_MARK_ACTIVE_PARTITION + 0 ] == 0x90 ) && ( MBRImage[ MBLDR_MARK_ACTIVE_PARTITION + 1 ] == 0x90 ))
        {
            // Don't mark partitions, leave active flag intact
            MarkActivePartition = 0;
            Log ( DEBUG,CommonMessage ( 385 ) );
        }
        else
        {
            // Mark primary partition (being booted) with active flag, all others
            // will be marked inactive
            MarkActivePartition = 1;
            Log ( DEBUG,CommonMessage ( 386 ),MBRImage[ MBLDR_MARK_ACTIVE_PARTITION + 0 ],MBRImage[ MBLDR_MARK_ACTIVE_PARTITION + 1 ] );
        }

        // Check whether timed boot is allowed (0x90 is an opcode of NOP command)
        if ( MBRImage[ MBLDR_TIMED_BOOT_ALLOWED ] == 0x90 )
        {
            // Opcode of NOP is found at the beginning of timer interrupt
            // Timed boot is disabled, we may ignore timeout field
            TimedBootAllowed = 0;
            Log ( DEBUG,CommonMessage ( 159 ),MBRImage[ MBLDR_TIMED_BOOT_ALLOWED ] );
        }
        else
        {
            // No NOP opcode was found
            // Timed boot is enabled
            if (( MBRImage[ MBLDR_TIMER_INTERRUPT1 ] == 0x70 ) && ( MBRImage[ MBLDR_TIMER_INTERRUPT2 ] == 0x70 ))

            {
                // User timer (int 1Ch - value 70h)
                TimedBootAllowed = 1;
                Log ( DEBUG,CommonMessage ( 160 ),TimedBootAllowed,MBRImage[ MBLDR_TIMED_BOOT_ALLOWED ],MBRImage[ MBLDR_TIMER_INTERRUPT1 ],MBRImage[ MBLDR_TIMER_INTERRUPT2 ] );
            }
            else if (( MBRImage[ MBLDR_TIMER_INTERRUPT1 ] == 0x20 ) && ( MBRImage[ MBLDR_TIMER_INTERRUPT2 ] == 0x20 ))
            {
                // System timer (int 08h - value 20h)
                TimedBootAllowed = 2;
                Log ( DEBUG,CommonMessage ( 160 ),TimedBootAllowed,MBRImage[ MBLDR_TIMED_BOOT_ALLOWED ],MBRImage[ MBLDR_TIMER_INTERRUPT1 ],MBRImage[ MBLDR_TIMER_INTERRUPT2 ] );
            }
            else
            {
                // Unknown value, it is better to mark timer as disabled
                TimedBootAllowed = 0;
                Log ( DEBUG,CommonMessage ( 160 ),TimedBootAllowed,MBRImage[ MBLDR_TIMED_BOOT_ALLOWED ],MBRImage[ MBLDR_TIMER_INTERRUPT1 ],MBRImage[ MBLDR_TIMER_INTERRUPT2 ] );
            }
        }

        // Get timeout field
        if ( TimedBootAllowed > 0 )
        {
            Timeout = *( ( unsigned short* )( &( MBRImage[ MBLDR_TIMEOUT ] ) ) );
            // Any number between 0 and 64800, not more
            if ( Timeout > 64800 )
            {
                Log ( FATAL,CommonMessage ( 161 ),Timeout );
            }
            Timeout *= 10;
            if ( Timeout%18 != 0 )
            {
                Timeout /= 18;
                Timeout++;
                Timeout *= 18;
            }
            Log ( DEBUG,CommonMessage ( 162 ),Timeout );
        }

        // Check the size of boot menu text
        if ( strlen ( ( char* )&( MBRImage[ MBLDR_BOOT_MENU_TEXT ] ) ) >= MBLDR_WINDOWS_DISK_SIGNATURE - 4*NumberOfBootablePartitions - MBLDR_BOOT_MENU_TEXT )
        {
            Log ( FATAL,CommonMessage ( 163 ) );
        }
        strncpy ( BootMenuText,( char* )&( MBRImage[ MBLDR_BOOT_MENU_TEXT ] ),sizeof ( BootMenuText ) - 1 );
        Log ( DEBUG,CommonMessage ( 164 ),BootMenuText );

        // Immediately after boot menu text there is an array of sectors offset
        // (unsigned long[]) representing bootable partitions
        if (
            ( ( UseAsciiOrScanCode == 0 ) && ( BaseAsciiOrScanCode != '1' ) ) ||
            ( ( UseAsciiOrScanCode == 1 ) && ( BaseAsciiOrScanCode != 0x3B ) )
           )
        {
            // Warn a user about labels autodetect
            if ( UseAsciiOrScanCode == 0 )
            {
                MbldrShowInfoMessage ( CommonMessage ( 165 ), "ASCII " );
            }
            else if ( UseAsciiOrScanCode == 1 )
            {
                MbldrShowInfoMessage ( CommonMessage ( 165 ), "scan-" );
            }
            // Switch on custom mode of boot menu text
            CustomBootMenuText = 1;
        }
        for ( Count=0 ; Count<NumberOfBootablePartitions ; Count++ )
        {
            BootablePartitions[ Count ].relative_sectors_offset = *( ( unsigned long* )( &( MBRImage[ MBLDR_BOOT_MENU_TEXT + strlen ( BootMenuText ) + 1 + Count*4 ] ) ) );
            Log ( DEBUG,CommonMessage ( 166 ),Count,BootablePartitions[ Count ].relative_sectors_offset );

            // Here we try to detect the label for chosen partition searching
            // strings in boot menu text
            strcpy ( BootablePartitions[ Count ].label,"" );
            if (
                ( BootablePartitions[ Count ].relative_sectors_offset == 0 ) ||
                ( BootablePartitions[ Count ].relative_sectors_offset == 0xFFFFFFFFul )
               )
            {
                // Do not read labels for 'Skip boot' and 'Next HDD' menu entries, they always
                // should be generated at run-time (however user is able to set these label once)
                continue;
            }
            strcpy ( Buffer,"" );
            if ( ( UseAsciiOrScanCode == 0 ) && ( BaseAsciiOrScanCode == '1' ) )
            {
                sprintf ( Buffer,"\n%i",Count + 1 );
            }
            else if ( ( UseAsciiOrScanCode == 1 ) && ( BaseAsciiOrScanCode == 0x3B ) )
            {
                sprintf ( Buffer,"\nF%i",Count + 1 );
            }
            if ( strcmp ( Buffer,"" ) != 0 )
            {
                pLabel = strstr ( BootMenuText,Buffer );
                if ( pLabel != NULL )
                {
                    pLabel += strlen ( Buffer );
                    if ( *pLabel == '.' )
                    {
                        pLabel++;
                    }
                    if ( *pLabel != 0 )
                    {
                        // Skip space of special symbol representing default boot menu item
                        pLabel++;
                        if ( strstr ( pLabel,"\r" ) != NULL )
                        {
                            strncpy ( BootablePartitions[ Count ].label,pLabel,strstr ( pLabel,"\r" ) - pLabel );
                        }
                        else
                        {
                            strcpy ( BootablePartitions[ Count ].label,pLabel );
                        }
                        if ( strcmp ( BootablePartitions[ Count ].label,"Unknown" ) == 0 )
                        {
                            strcpy ( BootablePartitions[ Count ].label,"" );
                        }
                        Log ( DEBUG,CommonMessage ( 167 ),BootablePartitions[ Count ].label );
                    }
                }
            }
        }
    }
    else
    {
        // Set default menu text (no partitions have been added)
        CommonConstructBootMenuText ( 0 );
    }
}

// Prepares binary image of MBR with mbldr setting all fields
void CommonPrepareMBR
    (
    unsigned char MBRImage[]
    )
{
    unsigned short Count;

    // Overwrite first sector with mbldr static code
    memcpy ( ( void* )MBRImage,( void* )mbldr_bin,MBLDR_WINDOWS_DISK_SIGNATURE );

    // Set dynamic data based on current configuration

    MBRImage[ MBLDR_TIMER_INTERRUPT_KEY ] = TimerInterruptKey;

    MBRImage[ MBLDR_NUMBER_OF_BOOTABLE_PARTITIONS ] = NumberOfBootablePartitions;

    if ( NumberOfDefaultPartition != 255 )
    {
        MBRImage[ MBLDR_NUMBER_OF_DEFAULT_PARTITION ] = NumberOfDefaultPartition;
        // Fill the code which updates default partition with NOP opcodes
        MBRImage[ MBLDR_MODIFY_DEFAULT_PARTITION_OPCODE + 0 ] = 0x90;
        MBRImage[ MBLDR_MODIFY_DEFAULT_PARTITION_OPCODE + 1 ] = 0x90;
        MBRImage[ MBLDR_MODIFY_DEFAULT_PARTITION_OPCODE + 2 ] = 0x90;
    }
    else
    {
        MBRImage[ MBLDR_NUMBER_OF_DEFAULT_PARTITION ] = 0;
    }

    if ( UseAsciiOrScanCode == 0 )
    {
        // ASCII code of the pressed key should be analyzed, thus fill the code
        // that switches to scan-code with NOP opcodes
        MBRImage[ MBLDR_USE_ASCII_OR_SCAN_OPCODE + 0 ] = 0x90;
        MBRImage[ MBLDR_USE_ASCII_OR_SCAN_OPCODE + 1 ] = 0x90;
    }

    MBRImage[ MBLDR_BASE_ASCII_OR_SCAN_CODE ] = BaseAsciiOrScanCode;

    if ( ProgressBarAllowed == 0 )
    {
        // Progress-bar is disabled
        MBRImage[ MBLDR_PROGRESS_BAR_SWITCH_OPCODE + 0 ] = 0x90;
        MBRImage[ MBLDR_PROGRESS_BAR_SWITCH_OPCODE + 1 ] = 0x90;
    }

    if ( ProgressBarSymbol == 0 )
    {
        // Progress-bar looks like "[9876543210]"
        MBRImage[ MBLDR_PROGRESS_BAR_SYMBOL_OPCODE + 0 ] = 0x88;
        MBRImage[ MBLDR_PROGRESS_BAR_SYMBOL_OPCODE + 1 ] = 0xF0;
    }
    else
    {
        // Progress-bar looks like "[**********]"
        MBRImage[ MBLDR_PROGRESS_BAR_SYMBOL_OPCODE + 0 ] = 0xB0;
        MBRImage[ MBLDR_PROGRESS_BAR_SYMBOL_OPCODE + 1 ] = ProgressBarSymbol;
    }

    // Either hide other primary partitions at boot attempt or leave them in their original state
    if ( HideOtherPrimaryPartitions == 0 )
    {
        // Don't hide other primary partitions
        MBRImage[ MBLDR_HIDING_JUMP_OFFSET ] = 0x05;
    }
    else if ( HideOtherPrimaryPartitions == 1 )
    {
        // Hide other primary partitions
        MBRImage[ MBLDR_HIDING_JUMP_OFFSET ] = 0x02;
    }

    // Either set primary partitions active/inactive at boot attempt or leave them intact
    if ( MarkActivePartition == 0 )
    {
        // Don't mark primary partitions active/inactive
        MBRImage[ MBLDR_MARK_ACTIVE_PARTITION + 0 ] = 0x90;
        MBRImage[ MBLDR_MARK_ACTIVE_PARTITION + 1 ] = 0x90;
    }

    if ( TimedBootAllowed == 0 )
    {
        MBRImage[ MBLDR_TIMED_BOOT_ALLOWED ] = 0x90;
        // User timer (int 1Ch) - it is not used actually, but since we perform
        // interrupt interception in any case, it is more safe not to use system
        // interrupt
        MBRImage[ MBLDR_TIMER_INTERRUPT1 ] = 0x70;
        MBRImage[ MBLDR_TIMER_INTERRUPT2 ] = 0x70;
        // Opcode of IRET - we should not call old interrupt of user timer as it causes
        // hanging on certain configurations
        MBRImage[ MBLDR_IRET ] = 0xCF;
    }
    else if ( TimedBootAllowed == 1 )
    {
        MBRImage[ MBLDR_TIMED_BOOT_ALLOWED ] = mbldr_bin[ MBLDR_TIMED_BOOT_ALLOWED ];
        // User timer (int 1Ch)
        MBRImage[ MBLDR_TIMER_INTERRUPT1 ] = 0x70;
        MBRImage[ MBLDR_TIMER_INTERRUPT2 ] = 0x70;
        // Opcode of IRET - we should not call old interrupt of user timer as it causes
        // hanging on certain configurations
        MBRImage[ MBLDR_IRET ] = 0xCF;
    }
    else if ( TimedBootAllowed == 2 )
    {
        MBRImage[ MBLDR_TIMED_BOOT_ALLOWED ] = mbldr_bin[ MBLDR_TIMED_BOOT_ALLOWED ];
        // System timer (int 08h)
        MBRImage[ MBLDR_TIMER_INTERRUPT1 ] = 0x20;
        MBRImage[ MBLDR_TIMER_INTERRUPT2 ] = 0x20;
    }

    *( ( unsigned short* )( &( MBRImage[ MBLDR_TIMEOUT ] ) ) ) = Timeout/10;

    memcpy ( &( MBRImage[ MBLDR_BOOT_MENU_TEXT ] ),BootMenuText,strlen ( BootMenuText ) + 1 );

    for ( Count=0 ; Count<NumberOfBootablePartitions ; Count++ )
    {
        *( ( unsigned long* )( &( MBRImage[ MBLDR_BOOT_MENU_TEXT + strlen ( BootMenuText ) + 1 + Count*4 ] ) ) ) = BootablePartitions[ Count ].relative_sectors_offset;
    }

    // Store new MBR into log file
    Log ( DEBUG,CommonMessage ( 168 ) );
    for ( Count=0 ; Count<64 ; Count++ )
    {
        Log ( DEBUG,"0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
            MBRImage[ Count*8 + 0 ],MBRImage[ Count*8 + 1 ],
            MBRImage[ Count*8 + 2 ],MBRImage[ Count*8 + 3 ],
            MBRImage[ Count*8 + 4 ],MBRImage[ Count*8 + 5 ],
            MBRImage[ Count*8 + 6 ],MBRImage[ Count*8 + 7 ] );
    }
}

// Return maximum size of boot menu text (including trailing NULL-character)
unsigned int CommonGetMaximumSizeOfBootMenuText
    (
    void
    )
{
    return ( MBLDR_WINDOWS_DISK_SIGNATURE - 4*NumberOfBootablePartitions - MBLDR_BOOT_MENU_TEXT );
}

// Return string describing the name and parameters of found (available) partition
// by its index. If partition index is equal to -1 then 'skip boot' option is returned.
// If partition index is equal to -2 then 'boot from next hdd' option is returned.
void CommonGetDescriptionOfAvailablePartition
    (
    int iPartitionIndex,
    char *Buffer
    )
{
    // Boolean flag indicating whether the partition was found in the bootable list
    unsigned char PartitionFound = 0;
    // Count used to iterate through bootable (configured) partitions
    int Count;

    if ( iPartitionIndex == -1 )
    {
        // 'skip boot' entry
        for ( Count=0 ; Count<NumberOfBootablePartitions ; Count++ )
        {
            if ( BootablePartitions[ Count ].relative_sectors_offset == 0xFFFFFFFFul )
            {
                Log ( DEBUG,CommonMessage ( 194 ),Count );
                PartitionFound = 1;
                break;
            }
        }
        if ( PartitionFound == 0 )
        {
            strcpy ( Buffer," " );
        }
        else
        {
            strcpy ( Buffer,"*" );
        }
        strcat ( Buffer,"[" );
        strcat ( Buffer,CommonMessage ( 195 ) );
        strcat ( Buffer,"] " );
        strcat ( Buffer,CommonMessage ( 213 ) );
    }
    else if ( iPartitionIndex == -2 )
    {
        // 'boot from next hdd' entry
        for ( Count=0 ; Count<NumberOfBootablePartitions ; Count++ )
        {
            if ( BootablePartitions[ Count ].relative_sectors_offset == 0 )
            {
                Log ( DEBUG,CommonMessage ( 196 ),Count );
                PartitionFound = 1;
                break;
            }
        }
        if ( PartitionFound == 0 )
        {
            strcpy ( Buffer," " );
        }
        else
        {
            strcpy ( Buffer,"*" );
        }
        strcat ( Buffer,"[" );
        strcat ( Buffer,CommonMessage ( 197 ) );
        strcat ( Buffer,"] " );
        strcat ( Buffer,CommonMessage ( 214 ) );
    }
    else
    {
        // Ordinary partition
        for ( Count=0 ; Count<NumberOfBootablePartitions ; Count++ )
        {
            if ( BootablePartitions[ Count ].relative_sectors_offset == FoundPartitions[ iPartitionIndex ].relative_sectors_offset )
            {
                Log ( DEBUG,CommonMessage ( 185 ),iPartitionIndex,Count );
                PartitionFound = 1;
                break;
            }
        }
        if ( PartitionFound == 0 )
        {
            strcpy ( Buffer," " );
        }
        else
        {
            strcpy ( Buffer,"*" );
        }
        if ( FoundPartitions[ iPartitionIndex ].is_primary == 1 )
        {
            strcat ( Buffer,"[" );
            strcat ( Buffer,CommonMessage ( 186 ) );
            strcat ( Buffer,":" );
        }
        else
        {
            strcat ( Buffer,"[" );
            strcat ( Buffer,CommonMessage ( 187 ) );
            strcat ( Buffer,":" );
        }
        Log ( DEBUG,CommonMessage ( 188 ),FoundPartitions[ iPartitionIndex ].relative_sectors_offset );
        Log ( DEBUG,CommonMessage ( 189 ),FoundPartitions[ iPartitionIndex ].size_in_sectors );
        if ( ( FoundPartitions[ iPartitionIndex ].size_in_sectors & 0xC0000000 ) != 0 )
        {
            // Measuring in terabytes
            sprintf ( &( Buffer[ strlen ( Buffer ) ] ),"%.1f",( FoundPartitions[ iPartitionIndex ].size_in_sectors >> 21 )/1024.0 );
            strcat ( Buffer,CommonMessage ( 190 ) );
        }
        else if ( ( FoundPartitions[ iPartitionIndex ].size_in_sectors & 0xFFF00000 ) != 0 )
        {
            // Measuring in gigabytes
            sprintf ( &( Buffer[ strlen ( Buffer ) ] ),"%.1f",( FoundPartitions[ iPartitionIndex ].size_in_sectors >> 11 )/1024.0 );
            strcat ( Buffer,CommonMessage ( 191 ) );
        }
        else if ( ( FoundPartitions[ iPartitionIndex ].size_in_sectors & 0xFFFFFC00 ) != 0 )
        {
            // Measuring in megabytes
            sprintf ( &( Buffer[ strlen ( Buffer ) ] ),"%.1f",( FoundPartitions[ iPartitionIndex ].size_in_sectors >> 1 )/1024.0 );
            strcat ( Buffer,CommonMessage ( 192 ) );
        }
        else
        {
            // Measuring in kilobytes
            sprintf ( &( Buffer[ strlen ( Buffer ) ] ),"%lu",FoundPartitions[ iPartitionIndex ].size_in_sectors >> 1 );
            strcat ( Buffer,CommonMessage ( 193 ) );
        }
        strcat ( Buffer,"] " );
        strcat ( Buffer,PartitionTypes[ FoundPartitions[ iPartitionIndex ].partition_identifier ] );
    }
}

// Return string describing the name and parameters of bootable (configured) partition
// by its index
void CommonGetDescriptionOfBootablePartition
    (
    int iPartitionIndex,
    char *Buffer
    )
{
    // This boolean flag means "whether the offset of particular partition is valid
    // (could it be found in a list of existing partitions)"
    unsigned char IsPartitionFound = 0;
    // Count used to iterate through found (available) partitions
    int Count;

    if ( iPartitionIndex == NumberOfDefaultPartition )
    {
        Log ( DEBUG,CommonMessage ( 212 ),iPartitionIndex );
        strcpy ( Buffer,"*" );
    }
    else
    {
        strcpy ( Buffer," " );
    }
    for ( Count=0 ; Count<NumberOfFoundPartitions ; Count++ )
    {
        if ( BootablePartitions[ iPartitionIndex ].relative_sectors_offset == FoundPartitions[ Count ].relative_sectors_offset )
        {
            IsPartitionFound = 1;
            // Here we assume that list of found partitions never contain
            // entries with same offset (it would be illegal partition table)
            break;
        }
    }
    if ( IsPartitionFound == 1 )
    {
        // Partition offset in current configuration is valid (points to
        // existing partition)
        if ( FoundPartitions[ Count ].is_primary == 1 )
        {
            strcat ( Buffer,"[" );
            strcat ( Buffer,CommonMessage ( 186 ) );
            strcat ( Buffer,":" );
        }
        else
        {
            strcat ( Buffer,"[" );
            strcat ( Buffer,CommonMessage ( 187 ) );
            strcat ( Buffer,":" );
        }
        Log ( DEBUG,CommonMessage ( 188 ),FoundPartitions[ Count ].relative_sectors_offset );
        Log ( DEBUG,CommonMessage ( 189 ),FoundPartitions[ Count ].size_in_sectors );
        if ( ( FoundPartitions[ Count ].size_in_sectors & 0xC0000000 ) != 0 )
        {
            // Measuring in terabytes
            sprintf ( &( Buffer[ strlen ( Buffer ) ] ),"%.1f",( FoundPartitions[ Count ].size_in_sectors >> 21 )/1024.0 );
            strcat ( Buffer,CommonMessage ( 190 ) );
        }
        else if ( ( FoundPartitions[ Count ].size_in_sectors & 0xFFF00000 ) != 0 )
        {
            // Measuring in gigabytes
            sprintf ( &( Buffer[ strlen ( Buffer ) ] ),"%.1f",( FoundPartitions[ Count ].size_in_sectors >> 11 )/1024.0 );
            strcat ( Buffer,CommonMessage ( 191 ) );
        }
        else if ( ( FoundPartitions[ Count ].size_in_sectors & 0xFFFFFC00 ) != 0 )
        {
            // Measuring in megabytes
            sprintf ( &( Buffer[ strlen ( Buffer ) ] ),"%.1f",( FoundPartitions[ Count ].size_in_sectors >> 1 )/1024.0 );
            strcat ( Buffer,CommonMessage ( 192 ) );
        }
        else
        {
            // Measuring in kilobytes
            sprintf ( &( Buffer[ strlen ( Buffer ) ] ),"%lu",FoundPartitions[ Count ].size_in_sectors >> 1 );
            strcat ( Buffer,CommonMessage ( 193 ) );
        }
        strcat ( Buffer,"] " );
        if ( strcmp ( BootablePartitions[ iPartitionIndex ].label,"" ) != 0 )
        {
            strcat ( Buffer,BootablePartitions[ iPartitionIndex ].label );
        }
        else
        {
            strcat ( Buffer,PartitionTypes[ FoundPartitions[ Count ].partition_identifier ] );
        }
    }
    else
    {
        // At this point we need to remember that offsets to partitions taken
        // from existing mbldr configuration may point to incorrect places
        if ( BootablePartitions[ iPartitionIndex ].relative_sectors_offset == 0 )
        {
            Log ( DEBUG,CommonMessage ( 196 ),iPartitionIndex );
            strcat ( Buffer,"[" );
            strcat ( Buffer,CommonMessage ( 197 ) );
            strcat ( Buffer,"] " );
            if ( strcmp ( BootablePartitions[ iPartitionIndex ].label,"" ) != 0 )
            {
                strcat ( Buffer,BootablePartitions[ iPartitionIndex ].label );
            }
            else
            {
                strcat ( Buffer,CommonMessage ( 214 ) );
            }
        }
        else if ( BootablePartitions[ iPartitionIndex ].relative_sectors_offset == 0xFFFFFFFFul )
        {
            Log ( DEBUG,CommonMessage ( 194 ),iPartitionIndex );
            strcat ( Buffer,"[" );
            strcat ( Buffer,CommonMessage ( 195 ) );
            strcat ( Buffer,"] " );
            if ( strcmp ( BootablePartitions[ iPartitionIndex ].label,"" ) != 0 )
            {
                strcat ( Buffer,BootablePartitions[ iPartitionIndex ].label );
            }
            else
            {
                strcat ( Buffer,CommonMessage ( 213 ) );
            }
        }
        else
        {
            // Existing mbldr configuration may contain offsets of partitions
            // that are no longer valid (for instance, they have moved or even
            // deleted after previous configuration of mbldr)
            Log ( DEBUG,CommonMessage ( 215 ),BootablePartitions[ iPartitionIndex ].relative_sectors_offset,iPartitionIndex );
            strcat ( Buffer,"[" );
            if ( BootablePartitions[ iPartitionIndex ].relative_sectors_offset <= 62 )
            {
                // Offset is on the first track
                strcat ( Buffer,CommonMessage ( 391 ) );
            }
            else
            {
                // Offset if outside of the first track
                strcat ( Buffer,CommonMessage ( 216 ) );
            }
            strcat ( Buffer,"] " );
            if ( strcmp ( BootablePartitions[ iPartitionIndex ].label,"" ) != 0 )
            {
                strcat ( Buffer,BootablePartitions[ iPartitionIndex ].label );
            }
            else
            {
                if ( BootablePartitions[ iPartitionIndex ].relative_sectors_offset <= 62 )
                {
                    // Offset is on the first track
                    strcat ( Buffer,CommonMessage ( 392 ) );
                }
                else
                {
                    // Offset if outside of the first track
                    strcat ( Buffer,CommonMessage ( 217 ) );
                }
            }
        }
    }
}

// Return a string describing the list of specific sectors on a very first track of a
// chosen disk device. "Specific" here means "completely filled by zeroes" if the
// "iType" parameter is equal to 0 or "having a 0x55AA signature at the end" if "iType"
// is equal to 1
void CommonGetListOfSpecificSectors
    (
    int iType,
    char *Buffer
    )
{
    // Sector on a chosen disk device
    unsigned char Sector[ 512 ];
    // Blank sector
    unsigned char BlankSector[ 512 ];
    // Count of sector number
    int iSectorNumber;
    // Number of previously found empty sector
    int iPreviousSectorNumber;

    // Erase blank sector
    memset ( BlankSector,0,512 );

    // Initialize target string
    strcpy ( Buffer,"" );

    // Previous sector has not been found yet
    iPreviousSectorNumber = -1;

    // Go through all sectors on a very first track
    for ( iSectorNumber=0 ; iSectorNumber<63 ; iSectorNumber++ )
    {
        // Read next sector
        DisksReadSector ( iSectorNumber,Sector );
        if (
            (
             ( iType == 0 ) &&
             ( memcmp ( Sector,BlankSector,512 ) == 0 )
            ) ||
            (
             ( iType == 1 ) &&
             ( Sector[ 510 ] == 0x55 ) &&
             ( Sector[ 511 ] == 0xAA )
            )
           )
        {
            // Empty sector
            if ( iPreviousSectorNumber == -1 )
            {
                // Remember the number of sector to detect long sequences of empty sectors
                iPreviousSectorNumber = iSectorNumber;
            }
        }
        else
        {
            // Non-empty sector
            if ( iPreviousSectorNumber != -1 )
            {
                if ( strlen ( Buffer ) != 0 )
                {
                    // This is not a first sector number which should be recorded
                    strcat ( Buffer,", " );
                }
                if ( iPreviousSectorNumber + 1 == iSectorNumber )
                {
                    // Only one sector in a sequence
                    sprintf ( &( Buffer[ strlen ( Buffer ) ] ),"%u",iPreviousSectorNumber );
                }
                else if ( iPreviousSectorNumber + 2 == iSectorNumber )
                {
                    // Two sectors in a sequence
                    sprintf ( &( Buffer[ strlen ( Buffer ) ] ),"%u, %u",iPreviousSectorNumber,iPreviousSectorNumber + 1 );
                }
                else
                {
                    // More than 2 sectors in a sequence
                    sprintf ( &( Buffer[ strlen ( Buffer ) ] ),"%u-%u",iPreviousSectorNumber,iSectorNumber - 1 );
                }
                iPreviousSectorNumber = -1;
            }
        }
    }

    // When all sectors have been verified, we may still have an unterminated sequence
    if ( iPreviousSectorNumber != -1 )
    {
        if ( strlen ( Buffer ) != 0 )
        {
            // This is not a first sector number which should be recorded
            strcat ( Buffer,", " );
        }
        if ( iPreviousSectorNumber + 1 == iSectorNumber )
        {
            // Only one sector in a sequence
            sprintf ( &( Buffer[ strlen ( Buffer ) ] ),"%u",iPreviousSectorNumber );
        }
        else if ( iPreviousSectorNumber + 2 == iSectorNumber )
        {
            // Two sectors in a sequence
            sprintf ( &( Buffer[ strlen ( Buffer ) ] ),"%u, %u",iPreviousSectorNumber,iPreviousSectorNumber + 1 );
        }
        else
        {
            // More than 2 sectors in a sequence
            sprintf ( &( Buffer[ strlen ( Buffer ) ] ),"%u-%u",iPreviousSectorNumber,iSectorNumber - 1 );
        }
    }
}

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
    )
{
    // Check signature at the end of sector
    if (( BackupSector[ 510 ] != 0x55 ) || ( BackupSector[ 511 ] != 0xAA ))
    {
        // Allow a user to overwrite the signature
        switch ( MbldrChooseOneOfTwo ( CommonMessage ( 309 ),CommonMessage ( 405 ),CommonMessage ( 406 ) ) )
        {
            // First choice
            case 0:
            {
                // Initialize the signature
                BackupSector[ 510 ] = 0x55;
                BackupSector[ 511 ] = 0xAA;
            }
            break;

            // Second choice
            case 1:
            {
                // Do nothing
            }
            break;

            // User cancelled the dialog
            case -1:
            default:
            {
                return ( 0 );
            }
            break;
        }
    }

    // Check Windows hard-disk signature
    if (
        ( BackupSector[ 440 ] != MBRSector[ 440 ] ) ||
        ( BackupSector[ 441 ] != MBRSector[ 441 ] ) ||
        ( BackupSector[ 442 ] != MBRSector[ 442 ] ) ||
        ( BackupSector[ 443 ] != MBRSector[ 443 ] )
       )
    {
        // Allow a user to overwrite Windows hard-disk signature
        switch ( MbldrChooseOneOfTwo ( CommonMessage ( 410 ),CommonMessage ( 411 ),CommonMessage ( 412 ) ) )
        {
            // First choice
            case 0:
            {
                // Overwrite Windows hard-disk signature with the contents of current MBR
                BackupSector[ 440 ] = MBRSector[ 440 ];
                BackupSector[ 441 ] = MBRSector[ 441 ];
                BackupSector[ 442 ] = MBRSector[ 442 ];
                BackupSector[ 443 ] = MBRSector[ 443 ];
            }
            break;

            // Second choice
            case 1:
            {
                // Do nothing
            }
            break;

            // User cancelled the dialog
            case -1:
            default:
            {
                return ( 0 );
            }
            break;
        }
    }

    // Check if backup has at least one partition
    if (
        ( BackupSector[ 446 + 16*0 + 4 ] == 0 ) &&
        ( BackupSector[ 446 + 16*1 + 4 ] == 0 ) &&
        ( BackupSector[ 446 + 16*2 + 4 ] == 0 ) &&
        ( BackupSector[ 446 + 16*3 + 4 ] == 0 )
       )
    {
        // No partitions at all
        MbldrShowInfoMessage ( CommonMessage ( 178 ) );
    }
    else
    {
        // At least one partition exists

        // Check if there is no active partition in the backup
        if (
            ( ( ( BackupSector[ 446 + 16*0 ] & 0x80 ) != 0x80 ) || ( BackupSector[ 446 + 16*0 + 4 ] == 0 ) ) &&
            ( ( ( BackupSector[ 446 + 16*1 ] & 0x80 ) != 0x80 ) || ( BackupSector[ 446 + 16*1 + 4 ] == 0 ) ) &&
            ( ( ( BackupSector[ 446 + 16*2 ] & 0x80 ) != 0x80 ) || ( BackupSector[ 446 + 16*2 + 4 ] == 0 ) ) &&
            ( ( ( BackupSector[ 446 + 16*3 ] & 0x80 ) != 0x80 ) || ( BackupSector[ 446 + 16*3 + 4 ] == 0 ) )
           )
        {
            // No active partitions
            MbldrShowInfoMessage ( CommonMessage ( 381 ) );
        }
    }

    // Check if partition tables are completely identical
    if ( memcmp ( &( MBRSector[ 446 ] ),&( BackupSector[ 446 ] ),16*4 ) != 0 )
    {
        // Partition tables are different

        // Check if at least one new partition has appeared in current MBR comparing to backup
        if (
            (
             ( BackupSector[ 446 + 16*0 + 4 ] == 0 ) &&
             ( MBRSector[ 446 + 16*0 + 4 ] != 0 )
            ) ||
            (
             ( BackupSector[ 446 + 16*1 + 4 ] == 0 ) &&
             ( MBRSector[ 446 + 16*1 + 4 ] != 0 )
            ) ||
            (
             ( BackupSector[ 446 + 16*2 + 4 ] == 0 ) &&
             ( MBRSector[ 446 + 16*2 + 4 ] != 0 )
            ) ||
            (
             ( BackupSector[ 446 + 16*3 + 4 ] == 0 ) &&
             ( MBRSector[ 446 + 16*3 + 4 ] != 0 )
            )
           )
        {
            // Warn user that new partitions have appeared: suggest user to keep whole PT from current MBR
            switch ( MbldrChooseOneOfTwo ( CommonMessage ( 407 ),CommonMessage ( 314 ),CommonMessage ( 313 ) ) )
            {
                // First choice
                case 0:
                {
                    // Overwrite partition table in backup from current MBR
                    memcpy ( &( BackupSector[ 446 ] ),&( MBRSector[ 446 ] ),16*4 );
                }
                break;

                // Second choice
                case 1:
                {
                    // Do nothing
                }
                break;

                // User cancelled the dialog
                case -1:
                default:
                {
                    return ( 0 );
                }
                break;
            }
        }

        // Check if at least one old partition has disappeared in current MBR comparing to backup
        if (
            (
             ( MBRSector[ 446 + 16*0 + 4 ] == 0 ) &&
             ( BackupSector[ 446 + 16*0 + 4 ] != 0 )
            ) ||
            (
             ( MBRSector[ 446 + 16*1 + 4 ] == 0 ) &&
             ( BackupSector[ 446 + 16*1 + 4 ] != 0 )
            ) ||
            (
             ( MBRSector[ 446 + 16*2 + 4 ] == 0 ) &&
             ( BackupSector[ 446 + 16*2 + 4 ] != 0 )
            ) ||
            (
             ( MBRSector[ 446 + 16*3 + 4 ] == 0 ) &&
             ( BackupSector[ 446 + 16*3 + 4 ] != 0 )
            )
           )
        {
            // Warn user that old partitions have disappeared: suggest user to keep whole PT from current MBR
            switch ( MbldrChooseOneOfTwo ( CommonMessage ( 408 ),CommonMessage ( 314 ),CommonMessage ( 313 ) ) )
            {
                // First choice
                case 0:
                {
                    // Overwrite partition table in backup from current MBR
                    memcpy ( &( BackupSector[ 446 ] ),&( MBRSector[ 446 ] ),16*4 );
                }
                break;

                // Second choice
                case 1:
                {
                    // Do nothing
                }
                break;

                // User cancelled the dialog
                case -1:
                default:
                {
                    return ( 0 );
                }
                break;
            }
        }

        // Compare partition locations and size for non-empty slots in partition table
        // (ignoring partitions' id and bootable flag)
        if (
            (
             ( BackupSector[ 446 + 16*0 + 4 ] != 0 ) &&
             ( MBRSector[ 446 + 16*0 + 4 ] != 0 ) &&
             (
              ( memcmp ( &( BackupSector[ 446 + 16*0 + 1 ] ),&( MBRSector[ 446 + 16*0 + 1 ] ),3 ) != 0 ) ||
              ( memcmp ( &( BackupSector[ 446 + 16*0 + 5 ] ),&( MBRSector[ 446 + 16*0 + 5 ] ),11 ) != 0 )
             )
            ) ||
            (
             ( BackupSector[ 446 + 16*1 + 4 ] != 0 ) &&
             ( MBRSector[ 446 + 16*1 + 4 ] != 0 ) &&
             (
              ( memcmp ( &( BackupSector[ 446 + 16*1 + 1 ] ),&( MBRSector[ 446 + 16*1 + 1 ] ),3 ) != 0 ) ||
              ( memcmp ( &( BackupSector[ 446 + 16*1 + 5 ] ),&( MBRSector[ 446 + 16*1 + 5 ] ),11 ) != 0 )
             )
            ) ||
            (
             ( BackupSector[ 446 + 16*2 + 4 ] != 0 ) &&
             ( MBRSector[ 446 + 16*2 + 4 ] != 0 ) &&
             (
              ( memcmp ( &( BackupSector[ 446 + 16*2 + 1 ] ),&( MBRSector[ 446 + 16*2 + 1 ] ),3 ) != 0 ) ||
              ( memcmp ( &( BackupSector[ 446 + 16*2 + 5 ] ),&( MBRSector[ 446 + 16*2 + 5 ] ),11 ) != 0 )
             )
            ) ||
            (
             ( BackupSector[ 446 + 16*3 + 4 ] != 0 ) &&
             ( MBRSector[ 446 + 16*3 + 4 ] != 0 ) &&
             (
              ( memcmp ( &( BackupSector[ 446 + 16*3 + 1 ] ),&( MBRSector[ 446 + 16*3 + 1 ] ),3 ) != 0 ) ||
              ( memcmp ( &( BackupSector[ 446 + 16*3 + 5 ] ),&( MBRSector[ 446 + 16*3 + 5 ] ),11 ) != 0 )
             )
            )
           )
        {
            // Existing partitions have different geometry (size/location and probably the id too, but we have not checked
            // yet - anyway it is a dangerous change): suggest user to keep whole PT from current MBR
            switch ( MbldrChooseOneOfTwo ( CommonMessage ( 409 ),CommonMessage ( 314 ),CommonMessage ( 313 ) ) )
            {
                // First choice
                case 0:
                {
                    // Overwrite partition table in backup from current MBR
                    memcpy ( &( BackupSector[ 446 ] ),&( MBRSector[ 446 ] ),16*4 );
                }
                break;

                // Second choice
                case 1:
                {
                    // Do nothing
                }
                break;

                // User cancelled the dialog
                case -1:
                default:
                {
                    return ( 0 );
                }
                break;
            }
        }
        else
        {
            // Either partitions' bootable flag differs or they have different
            // identification codes (usually defining file-system on a partition)

            // Check if bootable flag is different
            if (
                ( ( BackupSector[ 446 + 16*0 ] & 0x80 ) != ( MBRSector[ 446 + 16*0 ] & 0x80 ) ) ||
                ( ( BackupSector[ 446 + 16*1 ] & 0x80 ) != ( MBRSector[ 446 + 16*1 ] & 0x80 ) ) ||
                ( ( BackupSector[ 446 + 16*2 ] & 0x80 ) != ( MBRSector[ 446 + 16*2 ] & 0x80 ) ) ||
                ( ( BackupSector[ 446 + 16*3 ] & 0x80 ) != ( MBRSector[ 446 + 16*3 ] & 0x80 ) )
               )
            {
                // Different partitions are marked as bootable: suggest to keep bootable flag as in MBR,
                // but it is also quite safe to copy it from backup
                switch ( MbldrChooseOneOfTwo ( CommonMessage ( 413 ),CommonMessage ( 414 ),CommonMessage ( 415 ) ) )
                {
                    // First choice
                    case 0:
                    {
                        // Overwrite bootable flags for all partitions in backup from current MBR
                        BackupSector[ 446 + 16*0 ] = MBRSector[ 446 + 16*0 ];
                        BackupSector[ 446 + 16*1 ] = MBRSector[ 446 + 16*1 ];
                        BackupSector[ 446 + 16*2 ] = MBRSector[ 446 + 16*2 ];
                        BackupSector[ 446 + 16*3 ] = MBRSector[ 446 + 16*3 ];
                    }
                    break;

                    // Second choice
                    case 1:
                    {
                        // Do nothing
                    }
                    break;

                    // User cancelled the dialog
                    case -1:
                    default:
                    {
                        return ( 0 );
                    }
                    break;
                }
            }

            // Check if partition identifiers are different
            if (
                ( BackupSector[ 446 + 16*0 + 4 ] != MBRSector[ 446 + 16*0 + 4 ] ) ||
                ( BackupSector[ 446 + 16*1 + 4 ] != MBRSector[ 446 + 16*1 + 4 ] ) ||
                ( BackupSector[ 446 + 16*2 + 4 ] != MBRSector[ 446 + 16*2 + 4 ] ) ||
                ( BackupSector[ 446 + 16*3 + 4 ] != MBRSector[ 446 + 16*3 + 4 ] )
               )
            {
                // Check if visibility of partitions has been changed (10h - hidden bit: 1-hidden, 0-visible)
                // 0x01 - Visible/hidden DOS 12-bit FAT
                // 0x04 - Visible/hidden DOS 3.0+ 16-bit FAT (up to 32M)
                // 0x06 - Visible/hidden DOS 3.31+ 16-bit FAT (over 32M)
                // 0x07 - Visible/hidden Windows NT NTFS or OS/2 HPFS (IFS)
                // 0x0B - Visible/hidden WIN95_OSR2/Win98 FAT32
                // 0x0C - Visible/hidden WIN95_OSR2/Win98 FAT32, LBA-mapped
                // 0x0E - Visible/hidden WIN95/98: DOS 16-bit FAT, LBA-mapped
                if (
                    // First partition
                    (
                     ( BackupSector[ 446 + 16*0 + 4 ] != MBRSector[ 446 + 16*0 + 4 ] ) &&
                     (
                      (
                       ( ( BackupSector[ 446 + 16*0 + 4 ] & 0xEF ) == 0x01 ) &&
                       ( ( MBRSector[ 446 + 16*0 + 4 ] & 0xEF ) == 0x01 )
                      ) ||
                      (
                       ( ( BackupSector[ 446 + 16*0 + 4 ] & 0xEF ) == 0x04 ) &&
                       ( ( MBRSector[ 446 + 16*0 + 4 ] & 0xEF ) == 0x04 )
                      ) ||
                      (
                       ( ( BackupSector[ 446 + 16*0 + 4 ] & 0xEF ) == 0x06 ) &&
                       ( ( MBRSector[ 446 + 16*0 + 4 ] & 0xEF ) == 0x06 )
                      ) ||
                      (
                       ( ( BackupSector[ 446 + 16*0 + 4 ] & 0xEF ) == 0x07 ) &&
                       ( ( MBRSector[ 446 + 16*0 + 4 ] & 0xEF ) == 0x07 )
                      ) ||
                      (
                       ( ( BackupSector[ 446 + 16*0 + 4 ] & 0xEF ) == 0x0B ) &&
                       ( ( MBRSector[ 446 + 16*0 + 4 ] & 0xEF ) == 0x0B )
                      ) ||
                      (
                       ( ( BackupSector[ 446 + 16*0 + 4 ] & 0xEF ) == 0x0C ) &&
                       ( ( MBRSector[ 446 + 16*0 + 4 ] & 0xEF ) == 0x0C )
                      ) ||
                      (
                       ( ( BackupSector[ 446 + 16*0 + 4 ] & 0xEF ) == 0x0E ) &&
                       ( ( MBRSector[ 446 + 16*0 + 4 ] & 0xEF ) == 0x0E )
                      )
                     )
                    ) ||
                    // Second partition
                    (
                     ( BackupSector[ 446 + 16*1 + 4 ] != MBRSector[ 446 + 16*1 + 4 ] ) &&
                     (
                      (
                       ( ( BackupSector[ 446 + 16*1 + 4 ] & 0xEF ) == 0x01 ) &&
                       ( ( MBRSector[ 446 + 16*1 + 4 ] & 0xEF ) == 0x01 )
                      ) ||
                      (
                       ( ( BackupSector[ 446 + 16*1 + 4 ] & 0xEF ) == 0x04 ) &&
                       ( ( MBRSector[ 446 + 16*1 + 4 ] & 0xEF ) == 0x04 )
                      ) ||
                      (
                       ( ( BackupSector[ 446 + 16*1 + 4 ] & 0xEF ) == 0x06 ) &&
                       ( ( MBRSector[ 446 + 16*1 + 4 ] & 0xEF ) == 0x06 )
                      ) ||
                      (
                       ( ( BackupSector[ 446 + 16*1 + 4 ] & 0xEF ) == 0x07 ) &&
                       ( ( MBRSector[ 446 + 16*1 + 4 ] & 0xEF ) == 0x07 )
                      ) ||
                      (
                       ( ( BackupSector[ 446 + 16*1 + 4 ] & 0xEF ) == 0x0B ) &&
                       ( ( MBRSector[ 446 + 16*1 + 4 ] & 0xEF ) == 0x0B )
                      ) ||
                      (
                       ( ( BackupSector[ 446 + 16*1 + 4 ] & 0xEF ) == 0x0C ) &&
                       ( ( MBRSector[ 446 + 16*1 + 4 ] & 0xEF ) == 0x0C )
                      ) ||
                      (
                       ( ( BackupSector[ 446 + 16*1 + 4 ] & 0xEF ) == 0x0E ) &&
                       ( ( MBRSector[ 446 + 16*1 + 4 ] & 0xEF ) == 0x0E )
                      )
                     )
                    ) ||
                    // Third partition
                    (
                     ( BackupSector[ 446 + 16*2 + 4 ] != MBRSector[ 446 + 16*2 + 4 ] ) &&
                     (
                      (
                       ( ( BackupSector[ 446 + 16*2 + 4 ] & 0xEF ) == 0x01 ) &&
                       ( ( MBRSector[ 446 + 16*2 + 4 ] & 0xEF ) == 0x01 )
                      ) ||
                      (
                       ( ( BackupSector[ 446 + 16*2 + 4 ] & 0xEF ) == 0x04 ) &&
                       ( ( MBRSector[ 446 + 16*2 + 4 ] & 0xEF ) == 0x04 )
                      ) ||
                      (
                       ( ( BackupSector[ 446 + 16*2 + 4 ] & 0xEF ) == 0x06 ) &&
                       ( ( MBRSector[ 446 + 16*2 + 4 ] & 0xEF ) == 0x06 )
                      ) ||
                      (
                       ( ( BackupSector[ 446 + 16*2 + 4 ] & 0xEF ) == 0x07 ) &&
                       ( ( MBRSector[ 446 + 16*2 + 4 ] & 0xEF ) == 0x07 )
                      ) ||
                      (
                       ( ( BackupSector[ 446 + 16*2 + 4 ] & 0xEF ) == 0x0B ) &&
                       ( ( MBRSector[ 446 + 16*2 + 4 ] & 0xEF ) == 0x0B )
                      ) ||
                      (
                       ( ( BackupSector[ 446 + 16*2 + 4 ] & 0xEF ) == 0x0C ) &&
                       ( ( MBRSector[ 446 + 16*2 + 4 ] & 0xEF ) == 0x0C )
                      ) ||
                      (
                       ( ( BackupSector[ 446 + 16*2 + 4 ] & 0xEF ) == 0x0E ) &&
                       ( ( MBRSector[ 446 + 16*2 + 4 ] & 0xEF ) == 0x0E )
                      )
                     )
                    ) ||
                    // Fourth partition
                    (
                     ( BackupSector[ 446 + 16*3 + 4 ] != MBRSector[ 446 + 16*3 + 4 ] ) &&
                     (
                      (
                       ( ( BackupSector[ 446 + 16*3 + 4 ] & 0xEF ) == 0x01 ) &&
                       ( ( MBRSector[ 446 + 16*3 + 4 ] & 0xEF ) == 0x01 )
                      ) ||
                      (
                       ( ( BackupSector[ 446 + 16*3 + 4 ] & 0xEF ) == 0x04 ) &&
                       ( ( MBRSector[ 446 + 16*3 + 4 ] & 0xEF ) == 0x04 )
                      ) ||
                      (
                       ( ( BackupSector[ 446 + 16*3 + 4 ] & 0xEF ) == 0x06 ) &&
                       ( ( MBRSector[ 446 + 16*3 + 4 ] & 0xEF ) == 0x06 )
                      ) ||
                      (
                       ( ( BackupSector[ 446 + 16*3 + 4 ] & 0xEF ) == 0x07 ) &&
                       ( ( MBRSector[ 446 + 16*3 + 4 ] & 0xEF ) == 0x07 )
                      ) ||
                      (
                       ( ( BackupSector[ 446 + 16*3 + 4 ] & 0xEF ) == 0x0B ) &&
                       ( ( MBRSector[ 446 + 16*3 + 4 ] & 0xEF ) == 0x0B )
                      ) ||
                      (
                       ( ( BackupSector[ 446 + 16*3 + 4 ] & 0xEF ) == 0x0C ) &&
                       ( ( MBRSector[ 446 + 16*3 + 4 ] & 0xEF ) == 0x0C )
                      ) ||
                      (
                       ( ( BackupSector[ 446 + 16*3 + 4 ] & 0xEF ) == 0x0E ) &&
                       ( ( MBRSector[ 446 + 16*3 + 4 ] & 0xEF ) == 0x0E )
                      )
                     )
                    )
                   )
                {
                    // At least one partition has changed its visibility: suggest to keep visibility as in MBR,
                    // but it is also quite safe to copy it from backup
                    switch ( MbldrChooseOneOfTwo ( CommonMessage ( 310 ),CommonMessage ( 311 ),CommonMessage ( 312 ) ) )
                    {
                        // First choice
                        case 0:
                        {
                            // Overwrite identifiers for all partitions in backup from current MBR
                            BackupSector[ 446 + 16*0 + 4 ] = MBRSector[ 446 + 16*0 + 4 ];
                            BackupSector[ 446 + 16*1 + 4 ] = MBRSector[ 446 + 16*1 + 4 ];
                            BackupSector[ 446 + 16*2 + 4 ] = MBRSector[ 446 + 16*2 + 4 ];
                            BackupSector[ 446 + 16*3 + 4 ] = MBRSector[ 446 + 16*3 + 4 ];
                        }
                        break;

                        // Second choice
                        case 1:
                        {
                            // Do nothing
                        }
                        break;

                        // User cancelled the dialog
                        case -1:
                        default:
                        {
                            return ( 0 );
                        }
                        break;
                    }
                }
                else
                {
                    // The difference between partition identifiers if critical: suggest user to keep whole PT from MBR
                    switch ( MbldrChooseOneOfTwo ( CommonMessage ( 416 ),CommonMessage ( 314 ),CommonMessage ( 313 ) ) )
                    {
                        // First choice
                        case 0:
                        {
                            // Overwrite partition table in backup from current MBR
                            memcpy ( &( BackupSector[ 446 ] ),&( MBRSector[ 446 ] ),16*4 );
                        }
                        break;

                        // Second choice
                        case 1:
                        {
                            // Do nothing
                        }
                        break;

                        // User cancelled the dialog
                        case -1:
                        default:
                        {
                            return ( 0 );
                        }
                        break;
                    }
                }
            }
        }
    }

    // Final confirmation from a user
    if ( MbldrYesNo ( CommonMessage ( 203 ) ) != 0 )
    {
        // User confirmed all questions and is ready to restore MBR from backup
        return ( 1 );
    }
    else
    {
        // User cancels the operation
        return ( 0 );
    }
}

