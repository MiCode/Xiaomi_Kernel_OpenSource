/*
** =========================================================================
** Copyright (c) 2007-2010  Immersion Corporation.  All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
**                          Immersion Corporation Confidential and Proprietary
** =========================================================================
*/
/**
\file   ImmVibeOS.h
\brief  Defines OS dependent constants, macros, and types for the \api.
*/

#ifndef _IMMVIBEOS_H
#define _IMMVIBEOS_H

#include <sys/types.h>
#include <limits.h>

#define VIBE_INT8_MIN					SCHAR_MIN
#define VIBE_INT8_MAX					SCHAR_MAX
#define VIBE_UINT8_MAX					UCHAR_MAX
#define VIBE_INT16_MIN					SHRT_MIN
#define VIBE_INT16_MAX					SHRT_MAX
#define VIBE_UINT16_MAX					USHRT_MAX
#define VIBE_INT32_MIN					LONG_MIN
#define VIBE_INT32_MAX					LONG_MAX
#define VIBE_UINT32_MAX					ULONG_MAX
#define VIBE_TRUE						1
#define VIBE_FALSE						0

/** \brief 8-bit integer. */
typedef int8_t						    VibeInt8;
/** \brief 8-bit unsigned integer. */
typedef u_int8_t					    VibeUInt8;
/** \brief 16-bit integer. */
typedef int16_t					        VibeInt16;
/** \brief 16-bit unsigned integer. */
typedef u_int16_t					    VibeUInt16;
/** \brief 32-bit integer. */
typedef int32_t						    VibeInt32;
/** \brief 32-bit unsigned integer. */
typedef u_int32_t					    VibeUInt32;
/** \brief Boolean. */
typedef u_int8_t					    VibeBool;
/** \brief UCS-2 character. */
typedef unsigned short                  VibeWChar;

#endif /* _IMMVIBEOS_H */
