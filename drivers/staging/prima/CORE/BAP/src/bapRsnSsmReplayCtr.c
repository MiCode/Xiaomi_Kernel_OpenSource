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
 * File: $Header: //depot/software/projects/feature_branches/gen5_phase1/os/linux/classic/ap/apps/ssm/lib/aniSsmReplayCtr.c#2 $ 
 *
 * Contains definitions of various utilities for EAPoL frame
 * parsing and creation.
 *
 * Author:      Mayank D. Upadhyay
 * Date:        19-June-2002
 * History:-
 * Date         Modified by     Modification Information
 * ------------------------------------------------------
 *
 */
#include "vos_types.h"
#include "vos_trace.h"
#include <bapRsnErrors.h>

#include <bapRsnSsmReplayCtr.h>
#include "vos_status.h"
#include "vos_memory.h"
#include "vos_utils.h"
#include "vos_packet.h"

//#include "aniSsmUtils.h"

/*
 * Opaque replay counter type. Does TX and RX side replay counter
 * tracking. On the TX side, it returns monotonically increasing values
 * of the counter and checks that the peer returned a value matching
 * the one we sent. On the RX side, it makes sure that the peer sent a
 * replay counter greater than the last one seen (excepting for the
 * first time a check is made which the application has to special case.)
 */
struct sAniSsmReplayCtr {
    v_U8_t size;
    v_U8_t *buf;
    v_U32_t currentValue;
    v_U8_t init;
};

static int
updateCtrBuf(tAniSsmReplayCtr *ctr);

/**
 * aniSsmReplayCtrCreate
 *
 * Creates a replay counter and initializes it for first time
 * use. The initialization can be done randomly or with a passed in
 * value like 0. In case this is going to be used on the RX side, it
 * doesn't matter what the initialization is and can be optimized to
 * a fixed value so as to avoid the overhead of obtaining a random
 * value.
 *
 * @param ctrPtr a pointer that will be set to the newly allocated
 * counter if the operation succeeds
 * @param size the number of bytes that are desired in the counter
 * @param initValue if this is negative and size is greater than 4,
 * the initialization is done randomly. Otherwise, these bytes are
 * copied into the least significant four or less octets of the
 * counter, depending on the size of the counter. i.e., if the counter
 * is only 2B, then the least significant 2B of initValue will be
 * copied over.
 *
 * @return ANI_OK if the operation succeeds
 */
int
aniSsmReplayCtrCreate(v_U32_t cryptHandle, tAniSsmReplayCtr **ctrPtr, 
                      v_U8_t size,
                      int initValue)
{
    tAniSsmReplayCtr *ctr;

    ctr = vos_mem_malloc( sizeof(tAniSsmReplayCtr) );
    if( NULL == ctr )
    {
        return ANI_E_MALLOC_FAILED;
    }

    ctr->buf = vos_mem_malloc( size );
    if (ctr->buf == NULL) 
    {
        VOS_ASSERT( 0 );
        vos_mem_free(ctr);
        return ANI_E_MALLOC_FAILED;
    }

    ctr->size = size;

    // We cannot randomly generate the most significant bytes if the
    // total number of bytes is not greater than 4 (sizeof ANI_U32).
    if (initValue < 0 && ctr->size <= 4)
        initValue = 0;

    // If initValue is negative, initialize the ctr randomly, else
    // initialize it to what the user specified.
    if (initValue < 0) 
    {
        if( !VOS_IS_STATUS_SUCCESS( vos_rand_get_bytes(cryptHandle, ctr->buf, ctr->size) ) )
        {
            return ANI_ERROR;
        }
    } 
    else {
        ctr->currentValue = initValue - 1;
    }

    *ctrPtr = ctr;

    return ANI_OK;
}

static int
updateCtrBuf(tAniSsmReplayCtr *ctr)
{

    v_U32_t numBytes;
    v_U32_t offset;
    v_U32_t tmp;

    tmp = vos_cpu_to_be32( ctr->currentValue );

    numBytes = (4 <= ctr->size) ? 4 : ctr->size;
    offset = 4 - numBytes;
    vos_mem_copy(ctr->buf + ctr->size - numBytes, 
           ((v_U8_t *) &tmp) + offset, numBytes);

    return ANI_OK;
}

/**
 * aniSsmReplayCtrCmp
 *
 * Used to check if the passed in value is greater
 * than, less than, or the same as the previous value. 
 *
 * Can be used on the TX side to determine if the response to a
 * request contains the same counter as the one in the request.
 *
 * Can be used on the RX side to determine if the request has a
 * counter greater than the previous request, or if this is a
 * retransmission of the previous request. The application should
 * special-case the first time this is called on the RX side.
 *
 * @param ctr the current replay counter
 * @param value the value to check against
 *
 * @return a negative value if current ctr is less than the
 * given value, zero if they are the same, and a positive value if the
 * current counter is greater than that of the given value.
 */
int
aniSsmReplayCtrCmp(tAniSsmReplayCtr *ctr, v_U8_t *value)
{
    return vos_mem_compare2(ctr->buf, value, ctr->size);
}

/**
 * aniSsmReplayCtrUpdate
 *
 * Used on the RX side to update the value of the current replay
 * counter to that received in the next request. Typically this is
 * called after it is determined that this is not a retransmission,
 * and some sort of integrity checking is done on it.
 *
 * @param ctr the current replay counter
 * @param value the value that it should be set to
 *
 * @return ANI_OK if the operation succeeds
 */
int
aniSsmReplayCtrUpdate(tAniSsmReplayCtr *ctr,
                      v_U8_t *value)
{
    vos_mem_copy(ctr->buf, value, ctr->size);

    return ANI_OK;
}

/**
 * aniSsmReplayCtrNext
 *
 * Used on the RX side to obtain the next value that should be sent
 * with a request. After this call, the current value is incremented
 * by one.
 *
 * @param ctr the current replay counter
 * @param value where the next counter value should be copied
 * into. The caller must allocated enough storage for this.
 *
 * @return ANI_OK if the operation succeeds
 */
int
aniSsmReplayCtrNext(tAniSsmReplayCtr *ctr,
                    v_U8_t *value)
{
    ctr->currentValue++;
    updateCtrBuf(ctr);
    vos_mem_copy(value, ctr->buf, ctr->size);

    return ANI_OK;
}

/**
 * aniSsmReplayCtrFree
 *
 * Frees the replay counter context.
 *
 * @param ctr the replay counter to free.
 *
 * @return ANI_OK if the operation succeeds
 */
int
aniSsmReplayCtrFree(tAniSsmReplayCtr *ctr)
{

    if (ctr->buf != NULL)
        vos_mem_free(ctr->buf);

    vos_mem_free(ctr);

    return ANI_OK;
}
