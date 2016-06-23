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

/*
 *
 * This is the private header file for CFG module.
 *
 * Author:        Kevin Nguyen
 * Date:        03/20/02
 * History:-
 * 03/20/02        Created.
 * --------------------------------------------------------------------
 *
 */

#ifndef __CFGPRIV_H
#define __CFGPRIV_H

#include <sirCommon.h>
#include <sirTypes.h>
#include <sirDebug.h>
#include <wniStat.h>
#include <utilsApi.h>
#include <limApi.h>
#include <schApi.h>
#include <cfgApi.h>
#include "cfgDef.h"

    #include <wniCfgSta.h>

/*--------------------------------------------------------------------*/
/* CFG miscellaneous definition                                       */
/*--------------------------------------------------------------------*/

// Function index bit mask
#define CFG_FUNC_INDX_MASK   0x7f
#define CFG_GET_FUNC_INDX(val) (val & CFG_FUNC_INDX_MASK)

// Macro to convert return code to debug string index
#define CFG_GET_DBG_INDX(val) (val - eCFG_SUCCESS - 1)


/*--------------------------------------------------------------------*/
/* Binary header structure                                            */
/*--------------------------------------------------------------------*/
typedef struct sCfgBinHdr
{
    tANI_U32   hdrInfo;
    tANI_U32   controlSize;
    tANI_U32   iBufSize;
    tANI_U32   sBufSize;
} tCfgBinHdr, *tpCfgBinHdr;


/*--------------------------------------------------------------------*/
/* Polaris HW counter access structure                                */
/*--------------------------------------------------------------------*/
typedef struct
{
    tANI_U32    addr;
    tANI_U32    mask;
    tANI_U32    shift;
} tCfgHwCnt;


#define CFG_STAT_CNT_LO_MASK       0x0000ffff
#define CFG_STAT_CNT_HI_MASK       0xffff0000
#define CFG_STAT_CNT_HI_INCR       0x00010000

/*--------------------------------------------------------------------*/
/* CFG function prototypes                                            */
/*--------------------------------------------------------------------*/

extern void cfgSendHostMsg(tpAniSirGlobal, tANI_U16, tANI_U32, tANI_U32, tANI_U32*, tANI_U32, tANI_U32*);






#endif /* __CFGPRIV_H */




