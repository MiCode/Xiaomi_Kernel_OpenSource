/*==========================================================================*/
/*     (Copyright (C) 2003 Koninklijke Philips Electronics N.V.             */
/*     All rights reserved.                                                 */
/*     This source code and any compilation or derivative thereof is the    */
/*     proprietary information of Koninklijke Philips Electronics N.V.      */
/*     and is confidential in nature.                                       */
/*     Under no circumstances is this software to be exposed to or placed   */
/*     under an Open Source License of any type without the expressed       */
/*     written permission of Koninklijke Philips Electronics N.V.           */
/*==========================================================================*/
/*
 * Copyright (C) 2000,2001
 *               Koninklijke Philips Electronics N.V.
 *               All Rights Reserved.
 *
 * Copyright (C) 2000,2001 TriMedia Technologies, Inc.
 *               All Rights Reserved.
 *
 *############################################################
 *
 * Module name  : tmNxTypes.h  %version: 7 %
 *
 * Last Update  : %date_modified: Tue Jul  8 18:08:00 2003 %
 *
 * Description: TriMedia/MIPS global type definitions.
 *
 * Document Ref: DVP Software Coding Guidelines Specification
 * DVP/MoReUse Naming Conventions specification
 * DVP Software Versioning Specification
 * DVP Device Library Architecture Specification
 * DVP Board Support Library Architecture Specification
 * DVP Hardware API Architecture Specification
 *
 *
 *############################################################
 */

#ifndef TMNXTYPES_H
#define TMNXTYPES_H

/* ----------------------------------------------------------------------------- */
/* Standard include files: */
/* ----------------------------------------------------------------------------- */
/*  */

/* ----------------------------------------------------------------------------- */
/* Project include files: */
/* ----------------------------------------------------------------------------- */
/*  */
#include "tmFlags.h"		/* DVP common build control flags */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------------------- */
/* Types and defines: */
/* ----------------------------------------------------------------------------- */
/*  */

/*Under the TCS, <tmlib/tmtypes.h> may have been included by our client. In
    order to avoid errors, we take account of this possibility, but in order to
    support environments where the TCS is not available, we do not include the
    file by name.*/

#ifndef _TMtypes_h
#define _TMtypes_h

#define False           0
#define True            1

#ifdef __cplusplus
#define Null            0
#else
#define Null            ((Void *) 0)
#endif

/*  */
/* Standard Types */
/*  */
	typedef signed char Int8;	/* 8 bit   signed integer */
	typedef signed short Int16;	/* 16 bit   signed integer */
	typedef signed long Int32;	/* 32 bit   signed integer */
	typedef unsigned char UInt8;	/* 8 bit unsigned integer */
	typedef unsigned short UInt16;	/* 16 bit unsigned integer */
	typedef unsigned long UInt32;	/* 32 bit unsigned integer */
	typedef float Float;	/* 32 bit floating point */
	typedef unsigned int Bool;	/* Boolean (True/False) */
	typedef char Char;	/* character, character array ptr */
	typedef int Int;	/* machine-natural integer */
	typedef unsigned int UInt;	/* machine-natural unsigned integer */
	typedef char *String;	/* Null terminated 8 bit char str */

/* ----------------------------------------------------------------------------- */
/* Legacy TM Types/Structures (Not necessarily DVP Coding Guideline compliant) */
/* NOTE: For DVP Coding Gudeline compliant code, do not use these types. */
/*  */
	typedef char *Address;	/* Ready for address-arithmetic */
	typedef char const *ConstAddress;
	typedef unsigned char Byte;	/* Raw byte */
	typedef float Float32;	/* Single-precision float */
	typedef double Float64;	/* Double-precision float */
	typedef void *Pointer;	/* Pointer to anonymous object */
	typedef void const *ConstPointer;
	typedef char const *ConstString;

	typedef Int Endian;
#define BigEndian       0
#define LittleEndian    1

	typedef UInt32 tmErrorCode_t;
	typedef UInt32 tmProgressCode_t;


	typedef struct tmVersion {
		UInt8 majorVersion;
		UInt8 minorVersion;
		UInt16 buildVersion;
	} tmVersion_t, *ptmVersion_t;
#endif				/*ndef _TMtypes_h */

/*Define DVP types that are not TCS types.*/
/*
** ===== Updated from SDE2/2.3_Beta/sde_template/inc/tmNxTypes.h =====
**
** NOTE: IBits32/UBits32 types are defined for use with 32 bit bitfields.
**       This is done because ANSI/ISO compliant compilers require bitfields
**       to be of type "int" else a large number of compiler warnings will
**       result.  To avoid the risks associated with redefining Int32/UInt32
**       to type "int" instead of type "long" (which are the same size on 32
**       bit CPUs) separate 32bit signed/unsigned bitfield types are defined.
*/
	typedef signed int IBits32;	/* 32 bit   signed integer bitfields */
	typedef unsigned int UBits32;	/* 32 bit unsigned integer bitfields */
	typedef IBits32 * pIBits32;	/* 32 bit   signed integer bitfield ptr */
	typedef UBits32 * pUBits32;	/* 32 bit unsigned integer bitfield ptr */

	typedef Int8 * pInt8;
	typedef Int16 * pInt16;
	typedef Int32 * pInt32;
	typedef UInt8 *pUInt8;	/* 8 bit unsigned integer */
	typedef UInt16 *pUInt16;	/* 16 bit unsigned integer */
	typedef UInt32 * pUInt32;
	typedef void Void, *pVoid;	/* Void (typeless) */
	typedef Float * pFloat;
	typedef double Double, *pDouble;	/* 32/64 bit floating point */
	typedef Bool * pBool;
	typedef Char * pChar;
	typedef Int * pInt;
	typedef UInt * pUInt;
	typedef String * pString;

/*Assume that 64-bit integers are supported natively by C99 compilers and Visual
    C version 6.00 and higher. More discrimination in this area may be added
    here as necessary.*/
#if defined __STDC_VERSION__ && __STDC_VERSION__ > 199409L
/*This can be enabled only when all explicit references to the hi and lo
    structure members are eliminated from client code.*/
#define TMFL_NATIVE_INT64 1
	typedef signed long long int Int64, *pInt64;	/* 64-bit integer */
	typedef unsigned long long int UInt64, *pUInt64;	/* 64-bit bitmask */
/* #elif defined _MSC_VER && _MSC_VER >= 1200 */
/* /*This can be enabled only when all explicit references to the hi and lo */
/* structure members are eliminated from client code.*/ */
/* #define TMFL_NATIVE_INT64 1 */
/* typedef signed   __int64 Int64,  *pInt64;  // 64-bit integer */
/* typedef unsigned __int64 UInt64, *pUInt64; // 64-bit bitmask */
#else				/*!(defined __STDC_VERSION__ && __STDC_VERSION__ > 199409L) */
#define TMFL_NATIVE_INT64 0
	typedef
	    struct {
		/*Get the correct endianness (this has no impact on any other part of
		   the system, but may make memory dumps easier to understand). */
#if TMFL_ENDIAN == TMFL_ENDIAN_BIG
		Int32 hi;
		UInt32 lo;
#else
		UInt32 lo;
		Int32 hi;
#endif
	} Int64, *pInt64;	/* 64-bit integer */
	typedef
	    struct {
#if TMFL_ENDIAN == TMFL_ENDIAN_BIG
		UInt32 hi;
		UInt32 lo;
#else
		UInt32 lo;
		UInt32 hi;
#endif
	} UInt64, *pUInt64;	/* 64-bit bitmask */
#endif				/*defined __STDC_VERSION__ && __STDC_VERSION__ > 199409L */

/* Maximum length of device name in all BSP and capability structures */
#define HAL_DEVICE_NAME_LENGTH 16
/* timestamp definition */
	typedef UInt64 tmTimeStamp_t, *ptmTimeStamp_t;

/* for backwards compatibility with the older tmTimeStamp_t definition */
#define ticks   lo
#define hiTicks hi

	typedef union tmColor3	/* 3 byte color structure */
	{
		UBits32 u32;
#if (TMFL_ENDIAN == TMFL_ENDIAN_BIG)
		struct {
			UBits32:8;
			UBits32 red:8;
			UBits32 green:8;
			UBits32 blue:8;
		} rgb;
		struct {
			UBits32:8;
			UBits32 y:8;
			UBits32 u:8;
			UBits32 v:8;
		} yuv;
		struct {
			UBits32:8;
			UBits32 u:8;
			UBits32 m:8;
			UBits32 l:8;
		} uml;
#else
		struct {
			UBits32 blue:8;
			UBits32 green:8;
			UBits32 red:8;
			 UBits32:8;
		} rgb;
		struct {
			UBits32 v:8;
			UBits32 u:8;
			UBits32 y:8;
			 UBits32:8;
		} yuv;
		struct {
			UBits32 l:8;
			UBits32 m:8;
			UBits32 u:8;
			 UBits32:8;
		} uml;
#endif
	} tmColor3_t, *ptmColor3_t;

	typedef union tmColor4	/* 4 byte color structure */
	{
		UBits32 u32;
#if (TMFL_ENDIAN == TMFL_ENDIAN_BIG)
		struct {
			UBits32 alpha:8;
			UBits32 red:8;
			UBits32 green:8;
			UBits32 blue:8;
		} argb;
		struct {
			UBits32 alpha:8;
			UBits32 y:8;
			UBits32 u:8;
			UBits32 v:8;
		} ayuv;
		struct {
			UBits32 alpha:8;
			UBits32 u:8;
			UBits32 m:8;
			UBits32 l:8;
		} auml;
#else
		struct {
			UBits32 blue:8;
			UBits32 green:8;
			UBits32 red:8;
			UBits32 alpha:8;
		} argb;
		struct {
			UBits32 v:8;
			UBits32 u:8;
			UBits32 y:8;
			UBits32 alpha:8;
		} ayuv;
		struct {
			UBits32 l:8;
			UBits32 m:8;
			UBits32 u:8;
			UBits32 alpha:8;
		} auml;
#endif
	} tmColor4_t, *ptmColor4_t;

/* ----------------------------------------------------------------------------- */
/* Hardware device power states */
/*  */
	typedef enum tmPowerState {
		tmPowerOn,	/* Device powered on      (D0 state) */
		tmPowerStandby,	/* Device power standby   (D1 state) */
		tmPowerSuspend,	/* Device power suspended (D2 state) */
		tmPowerOff	/* Device powered off     (D3 state) */
	} tmPowerState_t, *ptmPowerState_t;

/* ----------------------------------------------------------------------------- */
/* Software Version Structure */
/*  */
	typedef struct tmSWVersion {
		UInt32 compatibilityNr;	/* Interface compatibility number */
		UInt32 majorVersionNr;	/* Interface major version number */
		UInt32 minorVersionNr;	/* Interface minor version number */

	} tmSWVersion_t, *ptmSWVersion_t;

/*Under the TCS, <tm1/tmBoardDef.h> may have been included by our client. In
    order to avoid errors, we take account of this possibility, but in order to
    support environments where the TCS is not available, we do not include the
    file by name.*/
#ifndef _TMBOARDDEF_H_
#define _TMBOARDDEF_H_

/* ----------------------------------------------------------------------------- */
/* HW Unit Selection */
/*  */
	typedef Int tmUnitSelect_t, *ptmUnitSelect_t;

#define tmUnitNone (-1)
#define tmUnit0    0
#define tmUnit1    1
#define tmUnit2    2
#define tmUnit3    3
#define tmUnit4    4

/*+compatibility*/
#define unitSelect_t       tmUnitSelect_t
#define unit0              tmUnit0
#define unit1              tmUnit1
#define unit2              tmUnit2
#define unit3              tmUnit3
#define unit4              tmUnit4
#define DEVICE_NAME_LENGTH HAL_DEVICE_NAME_LENGTH
/*-compatibility*/

#endif				/*ndef _TMBOARDDEF_H_ */

/* ----------------------------------------------------------------------------- */
/* Instance handle */
/*  */
	typedef Int tmInstance_t, *ptmInstance_t;

/* Callback function declaration */
	typedef Void(*ptmCallback_t) (UInt32 events, Void *pData, UInt32 userData);
#define tmCallback_t ptmCallback_t	/*compatibility */

/* Kernel debugging function declaration */
#ifdef TMFL_CFG_INTELCE4100
#define KERN_INFO void
#define printk(fmt, args...) printf(fmt, ## args)
#endif

#ifdef __cplusplus
}
#endif
#endif				/* ndef TMNXTYPES_H */
