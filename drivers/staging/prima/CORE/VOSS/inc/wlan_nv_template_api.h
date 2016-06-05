/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
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

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

#if !defined _WLAN_NV_TEMPLATE_API_H
#define  _WLAN_NV_TEMPLATE_API_H

#include "wlan_nv_types.h"

/*
* API Prototypes
* These are meant to be exposed to outside applications
*/
//extern void writeNvData(void);
//extern VOS_STATUS nvParser(tANI_U8 *pnvEncodedBuf, tANI_U32 nvReadBufSize, sHalNv *);

/*
* Parsing control bitmap
*/
#define _ABORT_WHEN_MISMATCH_MASK  0x00000001     /*set: abort when mismatch, clear: continue taking matched entries*/
#define _IGNORE_THE_APPENDED_MASK  0x00000002     /*set: ignore, clear: take*/

#define _FLAG_AND_ABORT(b)    (((b) & _ABORT_WHEN_MISMATCH_MASK) ? 1 : 0)

#endif /*#if !defined(_WLAN_NV_TEMPLATE_API_H) */
