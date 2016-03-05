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
/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This file pmmAP.cc contains AP PM functions
 *
 * Author:      Sandesh Goel
 * Date:        02/25/02
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "sirCommon.h"

#include "aniGlobal.h"

#include "schApi.h"
#include "limApi.h"
#include "cfgApi.h"
#include "wniCfgSta.h"

#include "pmmApi.h"
#include "pmmDebug.h"

/**
 * pmmGenerateTIM
 *
 * FUNCTION:
 * Generate TIM
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param pMac pointer to global mac structure
 * @param **pPtr pointer to the buffer, where the TIM bit is to be written.
 * @param *timLength pointer to limLength, which needs to be returned.
 * @return None
 */
void pmmGenerateTIM(tpAniSirGlobal pMac, tANI_U8 **pPtr, tANI_U16 *timLength, tANI_U8 dtimPeriod)
{
    tANI_U8 *ptr = *pPtr;
    tANI_U32 val = 0;
    tANI_U32 minAid = 1; // Always start with AID 1 as minimum
    tANI_U32 maxAid = HAL_NUM_STA;


    // Generate partial virtual bitmap
    tANI_U8 N1 = minAid / 8;
    tANI_U8 N2 = maxAid / 8;
    if (N1 & 1) N1--;

    *timLength = N2 - N1 + 4;
    val = dtimPeriod;

    /* 
     * 09/23/2011 - ASW team decision; 
     * Write 0xFF to firmware's field to detect firmware's mal-function early.
     * DTIM count and bitmap control usually cannot be 0xFF, so it is easy to know that 
     * firmware never updated DTIM count/bitmap control field after host driver downloaded
     * beacon template if end-user complaints that DTIM count and bitmapControl is 0xFF.
     */
    *ptr++ = SIR_MAC_TIM_EID;
    *ptr++ = (tANI_U8)(*timLength);
    *ptr++ = 0xFF; // location for dtimCount. will be filled in by FW.
    *ptr++ = (tANI_U8)val;

    *ptr++ = 0xFF; // location for bitmap control. will be filled in by FW.
    ptr += (N2 - N1 + 1);

    PELOG2(sirDumpBuf(pMac, SIR_PMM_MODULE_ID, LOG2, *pPtr, (*timLength)+2);)
    *pPtr = ptr;
}
