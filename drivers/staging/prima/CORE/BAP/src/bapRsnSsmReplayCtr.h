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
 * $File: //depot/software/projects/feature_branches/gen5_phase1/os/linux/classic/ap/apps/include/aniSsmReplayCtr.h $
 *
 * Contains declarations of various utilities for SSM replay counter
 * module.
 *
 * Author:      Mayank D. Upadhyay
 * Date:        15-June-2003
 * History:-
 * Date         Modified by     Modification Information
 * ------------------------------------------------------
 *
 */
#ifndef _ANI_SSM_REPLAY_CTR_H_
#define _ANI_SSM_REPLAY_CTR_H_

#include "vos_types.h"
#include <bapRsnAsfPacket.h>

/*
 * Opaque replay counter type. Does TX and RX side replay counter
 * tracking. On the TX side, it returns monotonicall increasing values
 * of the counter and checks that the peer returned a value matching
 * the one we sent. On the RX side, it makes sure that the peer sent a
 * replay counter greater than the last one seen (excepting for the
 * first time a check is made which the application has to special case.)
 */
typedef struct sAniSsmReplayCtr tAniSsmReplayCtr;

/**
 * aniSsmReplayCtrCreate
 *
 * Creates a replay counter and initializes it for first time
 * use. The initialization can be done randomly or with a passed in
 * value like 0. In case this is going to be used on the RX side, it
 * doesn't matter what the initiaalization is and can be optimized to
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
                      int initValue);

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
 * @return A negative error code if value is less than the
 * current value of the counter, zero if they are the same, and a
 * positive value if the current value is greater than that of the
 * counter.
 */
int
aniSsmReplayCtrCmp(tAniSsmReplayCtr *ctr,
                   v_U8_t *value);

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
                      v_U8_t *value);

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
                    v_U8_t *value);

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
aniSsmReplayCtrFree(tAniSsmReplayCtr *ctr);

#endif //_ANI_SSM_REPLAY_CTR_H_
