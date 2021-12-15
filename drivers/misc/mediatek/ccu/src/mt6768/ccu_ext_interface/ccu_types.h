/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef CCU_TYPES_H
#define CCU_TYPES_H

/*********************************************************************/
/* typedef unsigned char   BOOLEAN;    // uint8_t*/
#define MUINT8 unsigned char
#define MUINT16 unsigned short
#define MUINT32 unsigned int
/*typedef unsigned long  MUINT64;*/
/**/
#define MINT8 signed char
#define MINT16 signed short
#define MINT32 signed int
/*typedef signed long    MINT64;*/
/**/
#define MFLOAT float
#define MDOUBLE double
/**/
#define MVOID void
#define MBOOL int

#ifndef MTRUE
#define MTRUE 1
#endif

#ifndef MFALSE
#define MFALSE 0
#endif

#ifndef MNULL
#define MNULL 0
#endif

/********************************************************************
 *Sensor Types
 ********************************************************************/
/* #define CCU_CODE_SLIM*/
/* typedef unsigned char   BOOLEAN;    // uint8_t*/
#define U8 unsigned char	/* uint8_t*/
#define U16 unsigned short	/* uint16_t*/
#define U32 unsigned int	/* uint32_t*/
/*typedef unsigned long long  U64;    // uint64_t*/
#define I8 char		/* int8_t*/
#define I16 short		/* int16_t*/
#define I32 int		/* int32_t*/
/*typedef long long           I64;    // int64_t*/

#ifndef NULL
#define NULL                0
#endif				/* NULL*/

/********************************************************************
 * Error code
 ********************************************************************/
#define ERR_NONE                    (0)
#define ERR_INVALID                 (-1)
#define ERR_TIMEOUT                 (-2)


#endif				/* CCU_TYPES_H*/

