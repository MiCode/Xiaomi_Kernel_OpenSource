/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __WDA_DEBUG_H__
#define __WDA_DEBUG_H__
#if  !defined (ANI_OS_TYPE_ANDROID)
#include <stdio.h>
#endif
#include <stdarg.h>

#include "utilsApi.h"
#include "sirDebug.h"
#include "sirParams.h"
#define WDA_DEBUG_LOGIDX  ( LOG_INDEX_FOR_MODULE(SIR_WDA_MODULE_ID) )



#ifdef WLAN_DEBUG

#define WDALOGP(x0)  x0
#define WDALOGE(x0)  x0
#define WDALOGW(x0)  x0
#define WDALOG1(x0)  x0

#ifdef HAL_DEBUG_LOG2
#define WDALOG2(x0)  x0
#else
 #define WDALOG2(x0)
#endif

#ifdef HAL_DEBUG_LOG3
#define WDALOG3(x0)  x0
#else
 #define WDALOG3(x0)
#endif

#ifdef HAL_DEBUG_LOG4
#define WDALOG4(x0)  x0
#else
 #define WDALOG4(x0)
#endif

#define STR(x)  x

#else

#define WDALOGP(x)  x
#define WDALOGE(x)  {}
#define WDALOGW(x)  {}
#define WDALOG1(x)  {}
#define WDALOG2(x)  {}
#define WDALOG3(x)  {}
#define WDALOG4(x)  {}
#define STR(x)      ""
#endif

void wdaLog(tpAniSirGlobal pMac, tANI_U32 loglevel, const char *pString,...);

#endif // __WDA_DEBUG_H__

