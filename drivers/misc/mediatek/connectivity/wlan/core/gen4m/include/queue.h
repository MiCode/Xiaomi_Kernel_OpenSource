/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*
 ** Id: //Department/DaVinci/BRANCHES/
 *      MT6620_WIFI_DRIVER_V2_3/include/queue.h#1
 */

/*! \file   queue.h
 *    \brief  Definition for singly queue operations.
 *
 *    In this file we define the singly queue data structure and its
 *    queue operation MACROs.
 */

#ifndef _QUEUE_H
#define _QUEUE_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "gl_typedef.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
/* Singly Queue Structures - Entry Part */
struct QUE_ENTRY {
	struct QUE_ENTRY *prNext;
	struct QUE_ENTRY
		*prPrev;	/* For Rx buffer reordering used only */
};

/* Singly Queue Structures - Queue Part */
struct QUE {
	struct QUE_ENTRY *prHead;
	struct QUE_ENTRY *prTail;
	uint32_t u4NumElem;
};

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */
#define MAXNUM_TDLS_PEER            4

#define QUEUE_INITIALIZE(prQueue) \
	{ \
	    (prQueue)->prHead = (struct QUE_ENTRY *)NULL; \
	    (prQueue)->prTail = (struct QUE_ENTRY *)NULL; \
	    (prQueue)->u4NumElem = 0; \
	}

#define QUEUE_IS_EMPTY(prQueue)	\
	(((struct QUE *)(prQueue))->prHead == (struct QUE_ENTRY *)NULL)

#define QUEUE_IS_NOT_EMPTY(prQueue)         ((prQueue)->u4NumElem > 0)

#define QUEUE_GET_HEAD(prQueue)             ((prQueue)->prHead)

#define QUEUE_GET_TAIL(prQueue)             ((prQueue)->prTail)

#define QUEUE_GET_NEXT_ENTRY(prQueueEntry)  ((prQueueEntry)->prNext)

#define QUEUE_INSERT_HEAD(prQueue, prQueueEntry) \
	{ \
		ASSERT(prQueue); \
		ASSERT(prQueueEntry); \
		(prQueueEntry)->prNext = (prQueue)->prHead; \
		(prQueue)->prHead = (prQueueEntry); \
		if ((prQueue)->prTail == (struct QUE_ENTRY *)NULL) { \
			(prQueue)->prTail = (prQueueEntry); \
		} \
		((prQueue)->u4NumElem)++; \
	}

#define QUEUE_INSERT_TAIL(prQueue, prQueueEntry) \
	{ \
		ASSERT(prQueue); \
	  ASSERT(prQueueEntry); \
	  (prQueueEntry)->prNext = (struct QUE_ENTRY *)NULL; \
		if ((prQueue)->prTail) { \
			((prQueue)->prTail)->prNext = (prQueueEntry); \
		} else { \
			(prQueue)->prHead = (prQueueEntry); \
		} \
		(prQueue)->prTail = (prQueueEntry); \
		((prQueue)->u4NumElem)++; \
	}

/* NOTE: We assume the queue entry located at the beginning
 * of "prQueueEntry Type",
 * so that we can cast the queue entry to other data type without doubts.
 * And this macro also decrease the total entry count at the same time.
 */
#define QUEUE_REMOVE_HEAD(prQueue, prQueueEntry, _P_TYPE) \
	{ \
		ASSERT(prQueue); \
		prQueueEntry = (_P_TYPE)((prQueue)->prHead); \
		if (prQueueEntry) { \
			(prQueue)->prHead = \
				((struct QUE_ENTRY *)(prQueueEntry))->prNext; \
			if ((prQueue)->prHead == (struct QUE_ENTRY *)NULL) { \
				(prQueue)->prTail = (struct QUE_ENTRY *)NULL; \
			} \
			((struct QUE_ENTRY *)(prQueueEntry))->prNext = \
				(struct QUE_ENTRY *)NULL; \
			((prQueue)->u4NumElem)--; \
		} \
	}

#define QUEUE_MOVE_ALL(prDestQueue, prSrcQueue) \
	{ \
		ASSERT(prDestQueue); \
		ASSERT(prSrcQueue); \
	    *(struct QUE *)prDestQueue = *(struct QUE *)prSrcQueue; \
	    QUEUE_INITIALIZE(prSrcQueue); \
	}

#define QUEUE_CONCATENATE_QUEUES(prDestQueue, prSrcQueue) \
	{ \
	    ASSERT(prDestQueue); \
	    ASSERT(prSrcQueue); \
		if ((prSrcQueue)->u4NumElem > 0) { \
			if ((prDestQueue)->prTail) { \
				((prDestQueue)->prTail)->prNext = \
					(prSrcQueue)->prHead; \
			} else { \
				(prDestQueue)->prHead = (prSrcQueue)->prHead; \
			} \
			(prDestQueue)->prTail = (prSrcQueue)->prTail; \
			((prDestQueue)->u4NumElem) += \
				((prSrcQueue)->u4NumElem); \
			QUEUE_INITIALIZE(prSrcQueue); \
	    } \
	}

#define QUEUE_CONCATENATE_QUEUES_HEAD(prDestQueue, prSrcQueue) \
	{ \
		ASSERT(prDestQueue); \
		ASSERT(prSrcQueue); \
		if ((prSrcQueue)->u4NumElem > 0 && (prSrcQueue)->prTail) { \
			((prSrcQueue)->prTail)->prNext = \
				(prDestQueue)->prHead; \
			(prDestQueue)->prHead = (prSrcQueue)->prHead; \
			((prDestQueue)->u4NumElem) += \
				((prSrcQueue)->u4NumElem); \
			if ((prDestQueue)->prTail == NULL) {                 \
				(prDestQueue)->prTail = (prSrcQueue)->prTail; \
			}  \
			QUEUE_INITIALIZE(prSrcQueue); \
		} \
	}

/*******************************************************************************
 *                            E X T E R N A L  D A T A
 *******************************************************************************
 */
extern uint8_t g_arTdlsLink[MAXNUM_TDLS_PEER];

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

#endif /* _QUEUE_H */
