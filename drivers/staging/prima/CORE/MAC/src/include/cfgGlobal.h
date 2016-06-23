/*
 * Copyright (c) 2011-2014 The Linux Foundation. All rights reserved.
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

/*
 *
 * Author:      Sandesh Goel
 * Date:        02/09/03
 * History:-
 * 04/09/02        Created.
 * --------------------------------------------------------------------
 *
 */

#ifndef __CFGGLOBAL_H
#define __CFGGLOBAL_H

#include "sirCommon.h"
#include "sirTypes.h"
#include "wniCfgSta.h"
#include "wniCfgAp.h"
#define CFG_MAX_NUM_STA      SIR_MAX_NUM_STA_IN_IBSS

#define CFG_MAX_STATIC_STRING   70
#define CFG_MAX_STR_LEN       256    // as the number of channels grows, 128 is not big enough

/*--------------------------------------------------------------------*/
/* Configuration Control Structure                                    */
/*--------------------------------------------------------------------*/
typedef struct
{
    tANI_U32   control;
} tCfgCtl;

typedef struct sAniSirCfgStaticString
{
    tANI_U16 cfgId;
    tANI_U8  maxLen;
    tANI_U8  length;
    tANI_U8  data[255];
}tAniSirCfgStaticString;

typedef struct sAniSirCfgStatic
{
    tANI_U16 cfgId;
    tANI_U32 control;
    tANI_U32 cfgIMin;
    tANI_U32 cfgIMax;
    tANI_U32 cfgIVal;
    void     *pStrData;
}tAniSirCgStatic;

typedef struct sAniSirCfg
{
    // CFG module status
    tANI_U8    gCfgStatus;

    tCfgCtl    *gCfgEntry;
    tANI_U32   *gCfgIBufMin;
    tANI_U32   *gCfgIBufMax;
    tANI_U32   *gCfgIBuf;
    tANI_U8    *gCfgSBuf;

    tANI_U16    gCfgMaxIBufSize;
    tANI_U16    gCfgMaxSBufSize;

    // Static buffer for string parameter (must be word-aligned)
    tANI_U8    *gSBuffer;

    // Message parameter list buffer (enough for largest possible response)
    tANI_U32   *gParamList;
} tAniSirCfg,  *tpAniSirCfg;

#endif
