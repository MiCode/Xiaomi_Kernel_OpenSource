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
 * Airgo Networks, Inc proprietary. All rights reserved
 * aniParam.h: MAC parameter interface.
 * Author:  Kevin Nguyen
 * Date:    09/09/2002
 *
 * History:-
 * Date        Modified by              Modification Information
 * --------------------------------------------------------------------------
 *
 */

#ifndef _ANIPARAM_H
#define _ANIPARAM_H

#include "halTypes.h"

/**
 * -------------------------------------------------------------------------*
 *  MAC parameter structure                                                *
 *  This structure is the only interface passed between the MAC FW and the *
 *  host driver.                                                           *
 *                                                                         *
 *  Host-to-MAC parameters:                                                *
 *  =======================                                                *
 *  radioId:         radio ID (1 or 2)                                     *
 *  pPacketBufAlloc: function pointer for SKBuffer allocation              *
 *  pPacketBufFree : function pointer for SKBuffer free                    *
 *                                                                         *
 *-------------------------------------------------------------------------
 */
typedef struct 
{
    // HDD to MAC parameters
    int             radioId;

    void          (*pPacketBufAlloc)(unsigned short size, void **ppBuf, 
                                     void **ppSkb, void *dev);
    void          (*pPacketBufFree)(void* pBuf, void *pSkb);

    int rx_tasklet;

    // block table allocated by HDD
    void * block_table;

    tHalHandle hHalHandle;

} tAniMacParam;


#ifdef ANI_AP_SDK
#define NUM_RADIO 1
#else
#define NUM_RADIO 2
#endif


#endif /* _ANIPARAM_H */


