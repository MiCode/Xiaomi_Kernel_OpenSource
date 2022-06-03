/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __QRTR_DEBUG_H_
#define __QRTR_DEBUG_H_

#include <linux/types.h>

enum {
	RTX_REMOVE_RECORD = 0xFF,
	RTX_SKB_ALLOC_FAIL = 0xAA,
	RTX_SKB_ALLOC_SUCC = 0xBB,
	RTX_SENT_ACK = 0xCC,
	RTX_CTRL_SKB_ALLOC_FAIL = 0xDD,
	RTX_UNREG_NODE = 0xEE,
};

#if IS_ENABLED(CONFIG_QRTR_DEBUG)

void qrtr_debug_init(void);

void qrtr_debug_remove(void);

void qrtr_log_resume_tx_node_erase(unsigned int node_id);

int qrtr_log_resume_tx(unsigned int node_id,
		       unsigned int port_id, u8 state);

void qrtr_log_skb_failure(const void *data, size_t len);

#else

static inline void qrtr_debug_init(void) { }

static inline void qrtr_debug_remove(void) { }

static inline void qrtr_log_resume_tx_node_erase(unsigned int node_id) { }

static inline int qrtr_log_resume_tx(unsigned int node_id,
				     unsigned int port_id, u8 state)
{
	return 0;
}

static inline void qrtr_log_skb_failure(const void *data, size_t len) { }

#endif

#endif

