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


/* ------------------------*/
/* Basic Type Definitions */
/* -----------------------*/



#define kal_int8 signed char
#define kal_int16 signed short
#define kal_int32 signed int
#define kal_uint8 unsigned char
#define kal_uint16 unsigned short
#define kal_uint32 unsigned int




#define UINT8 unsigned char
#define UINT16 unsigned short
#define UINT32 unsigned int
#define INT8 signed char
#define INT32 signed int
#define BYTE unsigned char



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
#define BOOL unsigned char
#endif
#define kal_bool bool
#define	KAL_FALSE 0
#define KAL_TRUE 1


#endif				/* _CAMERA_TYPEDEFS_H */
