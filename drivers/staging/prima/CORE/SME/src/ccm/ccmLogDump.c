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

/*============================================================================
ccmLogDump.c

Implements the dump commands specific to the ccm module. 

 ============================================================================*/


#include "aniGlobal.h"
#include "logDump.h"

#if defined(ANI_LOGDUMP)

static tDumpFuncEntry ccmMenuDumpTable[] = {

   {0,     "CCM (861-870)",                               NULL},
    //{861,   "CCM: CCM testing ",                         dump_ccm}

};

void ccmDumpInit(tHalHandle hHal)
{
   logDumpRegisterTable( (tpAniSirGlobal) hHal, &ccmMenuDumpTable[0], 
                         sizeof(ccmMenuDumpTable)/sizeof(ccmMenuDumpTable[0]) );
}

#endif //#if defined(ANI_LOGDUMP)

