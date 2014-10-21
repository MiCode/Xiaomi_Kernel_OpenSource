/**
 *
 * INTEL CONFIDENTIAL
 * Copyright 2011 Intel Corporation All Rights Reserved. 
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material contains trade secrets and proprietary
 * and confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and treaty
 * provisions. No part of the Material may be used, copied, reproduced, modified,
 * published, uploaded, posted, transmitted, distributed, or disclosed in any
 * way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 *
 */


/*
 *	This file is to define some types that is platform independent.
*/
#pragma once

#ifndef _SE_TYPE_H_
#define _SE_TYPE_H_
//#include "se_cdefs.h"
#ifdef __x86_64__
#define SE_64   1
#endif

#ifdef SE_DRIVER

typedef	INT8	int8_t;
typedef	UINT8	uint8_t;
typedef	INT16	int16_t;
typedef	UINT16	uint16_t;
typedef	INT32	int32_t;
typedef	UINT32	uint32_t;
typedef	INT64	int64_t;
typedef	UINT64	uint64_t;

#else

#if defined(_MSC_VER)

#if _MSC_VER<=1400
#include <windows.h>
typedef	INT8	int8_t;
typedef	UINT8	uint8_t;
typedef	INT16	int16_t;
typedef	UINT16	uint16_t;
typedef	INT32	int32_t;
typedef	UINT32	uint32_t;
typedef	INT64	int64_t;
typedef	UINT64	uint64_t;
#else
#include <stdint.h>
#endif


#ifndef TRUE
#define	TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#elif defined(__GNUC__)
//#include <stdint.h>
//#include <unistd.h>

#ifndef TRUE
#define	TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#endif
#endif

#if defined(SE_64)

#define	PADDED_POINTER(t, p)			t* p
#define	PADDED_DWORD(d)					uint64_t d
#define	PADDED_LONG(l)					int64_t l
#define REG(name)						r##name
#ifdef SE_SIM_EXCEPTION
#define REG_ALIAS(name)					R##name
#endif
#define REGISTER(name)                  uint64_t REG(name)

#else // !defined(SE_64)

#define	PADDED_POINTER(t, p) t* p;       void*    ___##p##_pad_to64_bit
#define	PADDED_DWORD(d)      uint32_t d; uint32_t ___##d##_pad_to64_bit
#define	PADDED_LONG(l)       int32_t l;  int32_t  ___##l##_pad_to64_bit

#define REG(name)						e##name

#ifdef SE_SIM_EXCEPTION
#define REG_ALIAS(name)					E##name
#endif

#define REGISTER(name) uint32_t REG(name); uint32_t ___##e##name##_pad_to64_bit

#endif // !defined(SE_64)

#endif
