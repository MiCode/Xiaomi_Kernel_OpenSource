/*  SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __CONAP_SCP_RING_BUFFER_H__
#define __CONAP_SCP_RING_BUFFER_H__

#include <linux/spinlock.h>

#define SCP_CORE_OP_SZ 8

struct conap_rb_data {
	uint32_t param0;
	uint32_t param1;
	atomic_t ref_count;
	struct completion comp;
};

struct conap_core_rb_q {
	uint32_t write;
	uint32_t read;
	uint32_t size;
	spinlock_t lock;
	struct conap_rb_data* queue[SCP_CORE_OP_SZ];
};

struct conap_core_rb {
	spinlock_t lock;
	struct conap_rb_data queue[SCP_CORE_OP_SZ];
	struct conap_core_rb_q freeQ;
	struct conap_core_rb_q activeQ;
};

int conap_core_rb_init(struct conap_core_rb *rb);
int conap_core_rb_deinit(struct conap_core_rb *rb);

struct conap_rb_data* conap_core_rb_pop_free(struct conap_core_rb *rb);
void conap_core_rb_push_free(struct conap_core_rb *rb, struct conap_rb_data* data);
void conap_core_rb_push_active(struct conap_core_rb *rb, struct conap_rb_data* data);
struct conap_rb_data* conap_core_rb_pop_active(struct conap_core_rb *rb);

int conap_core_rb_has_pending_data(struct conap_core_rb *rb);


#endif
