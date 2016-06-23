/*
 * Copyright (c) 2014 The Linux Foundation. All rights reserved.
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

/************************************************************************
   wlan_qct_tl_trace.c

  \brief implementation for trace related APIs

  ========================================================================*/

#include "vos_trace.h"
#include "vos_types.h"
#include "wlan_qct_tl_trace.h"
#include "tlDebug.h"

static v_U8_t* tlTraceGetEventString(v_U32_t code)
{
    switch(code)
    {
         CASE_RETURN_STRING(TRACE_CODE_TL_STA_STATE);
         CASE_RETURN_STRING(TRACE_CODE_TL_EAPOL_PKT_PENDING);
         CASE_RETURN_STRING(TRACE_CODE_TL_GET_FRAMES_EAPOL);
         CASE_RETURN_STRING(TRACE_CODE_TL_RX_CONN_EAPOL);
         CASE_RETURN_STRING(TRACE_CODE_TL_REGISTER_STA_CLIENT);
         CASE_RETURN_STRING(TRACE_CODE_TL_SUSPEND_DATA_TX);
         CASE_RETURN_STRING(TRACE_CODE_TL_RESUME_DATA_TX);
         CASE_RETURN_STRING(TRACE_CODE_TL_STA_PKT_PENDING);
         CASE_RETURN_STRING(TRACE_CODE_TL_QUEUE_CURRENT);
         CASE_RETURN_STRING(TRACE_CODE_TL_REORDER_TIMER_EXP_CB);
         CASE_RETURN_STRING(TRACE_CODE_TL_BA_SESSION_DEL);
         default:
               return ("UNKNOWN");
               break;
    }
}

void tlTraceDump(void *pMac, tpvosTraceRecord pRecord, v_U16_t recIndex)
{
   TLLOGE( VOS_TRACE (VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                       "%04d    %012u  S%-3d    %-14s  %-30s(0x%x)",
                       recIndex, pRecord->time, pRecord->session, "  TL Event:  ",
                       tlTraceGetEventString (pRecord->code), pRecord->data));
}

void tlTraceInit()
{
   vosTraceRegister(VOS_MODULE_ID_TL, (tpvosTraceCb)&tlTraceDump);
}
