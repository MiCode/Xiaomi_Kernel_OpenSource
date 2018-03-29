/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _CAMERA_TYPEDEFS_H
#define _CAMERA_TYPEDEFS_H

#include <linux/bug.h>

/* ------------------------*/
/* Basic Type Definitions */
/* -----------------------*/

typedef long LONG;
typedef unsigned char UBYTE;
typedef short SHORT;

typedef signed char kal_int8;
typedef signed short kal_int16;
typedef signed int kal_int32;
typedef long long kal_int64;
typedef unsigned char kal_uint8;
typedef unsigned short kal_uint16;
typedef unsigned int kal_uint32;
typedef unsigned long long kal_uint64;
typedef char kal_char;

typedef unsigned int *UINT32P;
typedef volatile unsigned short *UINT16P;
typedef volatile unsigned char *UINT8P;
typedef unsigned char *U8P;


typedef unsigned char U8;
typedef signed char S8;
typedef unsigned short U16;
typedef signed short S16;
typedef unsigned int U32;
typedef signed int S32;
typedef unsigned long long U64;
typedef signed long long S64;
/* typedef unsigned char       bool; */

typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned int UINT32;
typedef unsigned short USHORT;
typedef signed char INT8;
typedef signed short INT16;
typedef signed int INT32;
typedef unsigned int DWORD;
typedef void VOID;
typedef unsigned char BYTE;
typedef float FLOAT;

typedef char *LPCSTR;
typedef short *LPWSTR;


/* -----------*/
/* Constants */
/* ----------*/
#ifndef FALSE
  #define FALSE (0)
#endif

#ifndef TRUE
#define TRUE  (1)
#endif

#ifndef NULL
#define NULL  (0)
#endif

/* enum boolean {false, true}; */
enum { RX, TX, NONE };

#ifndef BOOL
typedef unsigned char BOOL;
#endif

typedef enum {
	KAL_FALSE = 0,
	KAL_TRUE = 1,
} kal_bool;

#endif				/* _CAMERA_TYPEDEFS_H */
