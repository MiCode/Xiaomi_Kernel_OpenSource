/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/* porting layer */
/* Android */

#ifndef _MTK_PORTING_H_
#define _MTK_PORTING_H_

#include <linux/kernel.h>	/* include stddef.h for NULL */

/* typedef void VOID, *PVOID; */

#ifndef MTK_WCN_BOOL_TRUE
#define MTK_WCN_BOOL_FALSE               ((int) 0)
#define MTK_WCN_BOOL_TRUE                ((int) 1)
#endif



/* system APIs */
/* mutex */
typedef int(*MUTEX_CREATE) (const char *const name);
typedef int32_t(*MUTEX_DESTROY) (int mtx);
typedef int32_t(*MUTEX_LOCK) (int mtx);
typedef int32_t(*MUTEX_UNLOCK) (int mtx, unsigned long flags);
/* debug */
typedef int32_t(*DBG_PRINT) (const char *str, ...);
typedef int32_t(*DBG_ASSERT) (int32_t expr, const char *file, int32_t line);
/* timer */
typedef void (*MTK_WCN_TIMER_CB) (void);
typedef int(*TIMER_CREATE) (const char *const name);
typedef int32_t(*TIMER_DESTROY) (int tmr);
typedef int32_t(*TIMER_START) (int tmr, uint32_t timeout, MTK_WCN_TIMER_CB tmr_cb, void *param);
typedef int32_t(*TIMER_STOP) (int tmr);
/* kernel lib */
typedef void *(*SYS_MEMCPY) (void *dest, const void *src, uint32_t n);
typedef void *(*SYS_MEMSET) (void *s, int32_t c, uint32_t n);
typedef int32_t(*SYS_SPRINTF) (char *str, const char *format, ...);

#endif /* _MTK_PORTING_H_ */
