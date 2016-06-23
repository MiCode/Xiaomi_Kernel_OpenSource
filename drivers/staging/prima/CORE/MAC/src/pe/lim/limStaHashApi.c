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
 * limStaHashApi.c: Provides access functions to get/set values of station hash entry fields.
 * Author:    Sunit Bhatia
 * Date:       09/19/2006
 * History:-
 * Date        Modified by            Modification Information
 *
 * --------------------------------------------------------------------------
 *
 */

#include "limStaHashApi.h"


/**
 * limGetStaHashBssidx()
 *
 *FUNCTION:
 * This function is called to Get the Bss Index of the currently associated Station.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param pMac  pointer to Global Mac structure.
 * @param assocId AssocID of the Station.
 * @param bssidx pointer to the bss index, which will be returned by the function.
 *
 * @return success if GET operation is ok, else Failure.
 */

tSirRetStatus limGetStaHashBssidx(tpAniSirGlobal pMac, tANI_U16 assocId, tANI_U8 *bssidx, tpPESession psessionEntry)
{
    tpDphHashNode pSta = dphGetHashEntry(pMac, assocId, &psessionEntry->dph.dphHashTable);

    if (pSta == NULL)
    {
        PELOGE(limLog(pMac, LOGE, FL("invalid STA %d"),  assocId);)
        return eSIR_LIM_INVALID_STA;
    }

    *bssidx = (tANI_U8)pSta->bssId;
    return eSIR_SUCCESS;
}



