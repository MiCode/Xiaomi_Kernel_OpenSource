/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/windows/include/gl_typedef.h#1 $
*/

/*! \file   gl_typedef.h
    \brief  Definition of basic data type(os dependent).

    In this file we define the basic data type.
*/



/*
** $Log: gl_typedef.h $
**
** 09 17 2012 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Duplicate source from MT6620 v2.3 driver branch
** (Davinci label: MT6620_WIFI_Driver_V2_3_120913_1942_As_MT6630_Base)
 *
 * 10 03 2011 cp.wu
 * [WCXRP00001022] [MT6628 Driver][Firmware Download] Add multi section independent download functionality
 * add firmware download path in divided scatters.
 *
 * 07 08 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration - move to new repository.
 *
 * 06 06 2010 kevin.huang
 * [WPD00003832][MT6620 5931] Create driver base
 * [MT6620 5931] Create driver base
 *
 * 03 22 2010 cp.wu
 * [WPD00003824][MT6620 Wi-Fi][New Feature] Add support of large scan list
 * Implement feature needed by CR: WPD00003824: refining association command by pasting scanning result
**  \main\maintrunk.MT6620WiFiDriver_Prj\4 2009-04-21 09:21:48 GMT mtk01461
**  Add typedef - NDIS_EVENT for lint check
**  \main\maintrunk.MT6620WiFiDriver_Prj\3 2009-03-18 20:28:03 GMT mtk01461
**  Fix LINT warning introduced by NDIS_SPIN_LOCK
**  \main\maintrunk.MT6620WiFiDriver_Prj\2 2009-03-10 20:40:51 GMT mtk01426
**  Init for develop
**
*/


#ifndef _GL_TYPEDEF_H
#define _GL_TYPEDEF_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
LINT_EXT_HEADER_BEGIN
#include <ndis.h>		/* NDIS header */
#include <intsafe.h>
#include <stddef.h>
    LINT_EXT_HEADER_END
/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/* Define HZ of timer tick for function kalGetTimeTick() */
#define KAL_HZ                  1000
/* Miscellaneous Equates */
#ifndef FALSE
#define FALSE               ((BOOL) 0)
#define TRUE                ((BOOL) 1)
#endif				/* FALSE */
#ifndef NULL
#if defined(__cplusplus)
#define NULL            0
#else
#define NULL            ((void *) 0)
#endif
#endif
/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
#ifdef _lint
typedef void VOID, *PVOID;
typedef unsigned char BOOLEAN, *PBOOLEAN;
typedef BOOLEAN BOOL;
typedef signed char CHAR;
typedef signed short SHORT;
typedef signed long LONG;
typedef signed long long LONGLONG;
typedef unsigned char UCHAR, *PUCHAR;
typedef unsigned short USHORT;
typedef unsigned long ULONG;
typedef unsigned long long ULONGLONG;
#endif				/* _lint */


/* Type definition for void */
typedef VOID * *PPVOID;
#if defined(WINDOWS_DDK)
typedef BOOLEAN BOOL;
#endif

/* Type definition for signed integers */
typedef CHAR INT_8, *PINT_8, **PPINT_8;	/*  8-bit signed value & pointer */
typedef SHORT INT_16, *PINT_16, **PPINT_16;	/* 16-bit signed value & pointer */
typedef LONG INT_32, *PINT_32, **PPINT_32;	/* 32-bit signed value & pointer */
typedef LONGLONG INT_64, *PINT_64, **PPINT_64;	/* 64-bit signed value & pointer */

/* Type definition for unsigned integers */
typedef UCHAR UINT_8, *PUINT_8, **PPUINT_8, *P_UINT_8;	/*  8-bit unsigned value & pointer */
typedef USHORT UINT_16, *PUINT_16, **PPUINT_16;	/* 16-bit unsigned value & pointer */
typedef ULONG UINT_32, *PUINT_32, **PPUINT_32;	/* 32-bit unsigned value & pointer */
typedef ULONGLONG UINT_64, *PUINT_64, **PPUINT_64;	/* 64-bit unsigned value & pointer */

typedef UINT_32 OS_SYSTIME, *POS_SYSTIME;


#ifdef _lint
typedef UINT_32 NDIS_STATUS;
typedef UINT_32 NDIS_SPIN_LOCK;
typedef UINT_32 NDIS_EVENT;

typedef union _LARGE_INTEGER {
	struct {
		UINT_32 LowPart;
		INT_32 HighPart;
	} u;
	INT_64 QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef union _ULARGE_INTEGER {
	struct {
		UINT_32 LowPart;
		UINT_32 HighPart;
	} u;
	UINT_64 QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;

typedef struct _NDIS_PACKET {
	UCHAR MiniportReservedEx[16];
} NDIS_PACKET, *PNDIS_PACKET;


extern void DbgPrint(PINT_8 Format, ...);
extern void DbgBreakPoint(void);

#if 0				/* For removing Warning 413: Likely use of null pointer */
#define CONTAINING_RECORD(address, type, field) \
	((type *)((PUCHAR)(address) - (PUCHAR)(&((type *)0)->field)))
#else
#define CONTAINING_RECORD(address, type, field) \
	((type *)(address))
#endif


#endif				/* _lint */

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#define IN
#define OUT

#define __KAL_INLINE__          inline
#define __KAL_ATTRIB_PACKED__
#define __KAL_ATTRIB_ALIGN_4__


#ifndef BIT
#define BIT(n)                          ((UINT_32) 1 << (n))
#define BITS2(m, n)                      (BIT(m) | BIT(n))
#define BITS3(m, n, o)                    (BIT(m) | BIT (n) | BIT (o))
#define BITS4(m, n, o, p)                  (BIT(m) | BIT (n) | BIT (o) | BIT(p))

/* bits range: for example BITS(16,23) = 0xFF0000
 *   ==>  (BIT(m)-1)   = 0x0000FFFF     ~(BIT(m)-1)   => 0xFFFF0000
 *   ==>  (BIT(n+1)-1) = 0x00FFFFFF
 */
#define BITS(m, n)                       (~(BIT(m)-1) & ((BIT(n) - 1) | BIT(n)))
#endif				/* BIT */


/* This macro returns the byte offset of a named field in a known structure
   type.
   _type - structure name,
   _field - field name of the structure */
#ifndef OFFSET_OF
    /* To suppress lint Warning 413: Likely use of null pointer, and assign it a magic number */
#ifdef _lint
#define OFFSET_OF(_type, _field)    4
#else
#define OFFSET_OF(_type, _field)    ((UINT_32)&(((_type *)0)->_field))
#endif				/* _lint */
#endif				/* OFFSET_OF */


/* This macro returns the base address of an instance of a structure
 * given the type of the structure and the address of a field within the
 * containing structure.
 * _addrOfField - address of current field of the structure,
 * _type - structure name,
 * _field - field name of the structure
 */
#ifndef ENTRY_OF
    /* To suppress lint Warning 413: Likely use of null pointer */
#ifdef _lint
#define ENTRY_OF(_addrOfField, _type, _field) \
	((_type *)(_addrOfField))
#else
#define ENTRY_OF(_addrOfField, _type, _field) \
	((_type *)((PINT_8)(_addrOfField) - (PINT_8)OFFSET_OF(_type, _field)))
#endif				/* _lint */
#endif				/* ENTRY_OF */


/* This macro align the input value to the DW boundary.
 * _value - value need to check
 */
#ifndef ALIGN_4
#define ALIGN_4(_value)            (((_value) + 3) & ~BITS(0, 1))
#endif				/* ALIGN_4 */

/* This macro check the DW alignment of the input value.
 * _value - value of address need to check
 */
#ifndef IS_ALIGN_4
#define IS_ALIGN_4(_value)          (((_value) & 0x3) ? FALSE : TRUE)
#endif				/* IS_ALIGN_4 */

#ifndef IS_NOT_ALIGN_4
#define IS_NOT_ALIGN_4(_value)      (((_value) & 0x3) ? TRUE : FALSE)
#endif				/* IS_NOT_ALIGN_4 */


/* This macro evaluate the input length in unit of Double Word(4 Bytes).
 * _value - value in unit of Byte, output will round up to DW boundary.
 */
#ifndef BYTE_TO_DWORD
#define BYTE_TO_DWORD(_value)       ((_value + 3) >> 2)
#endif				/* BYTE_TO_DWORD */

/* This macro evaluate the input length in unit of Byte.
 * _value - value in unit of DW, output is in unit of Byte.
 */
#ifndef DWORD_TO_BYTE
#define DWORD_TO_BYTE(_value)       ((_value) << 2)
#endif				/* DWORD_TO_BYTE */


#define SWAP16(_x)   \
	((UINT_16)((((UINT_16)(_x) & 0x00FF) << 8) | \
		    (((UINT_16)(_x) & 0xFF00) >> 8)))

#define SWAP32(_x)   \
	((UINT_32)((((UINT_32)(_x) & 0x000000FF) << 24) | \
		    (((UINT_32)(_x) & 0x0000FF00) << 8) | \
		    (((UINT_32)(_x) & 0x00FF0000) >> 8) | \
		    (((UINT_32)(_x) & 0xFF000000) >> 24)))

/* TODO(Kevin): For Little-Endian Only
 * We need change following for Big-Endian.
 */
#if 1				/* Little-Endian */
#define CONST_NTOHS(_x)     SWAP16(_x)

#define CONST_HTONS(_x)     SWAP16(_x)

#define NTOHS(_x)           SWAP16(_x)

#define HTONS(_x)           SWAP16(_x)

#define NTOHL(_x)           SWAP32(_x)

#define HTONL(_x)           SWAP32(_x)

#else				/* Big-Endian */

#define CONST_NTOHS(_x)

#define CONST_HTONS(_x)

#define NTOHS(_x)

#define HTONS(_x)

#endif

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif				/* _GL_TYPEDEF_H */
