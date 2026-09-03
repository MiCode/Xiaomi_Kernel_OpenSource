/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MCHN_H__
#define __MCHN_H__

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <misc/wcn_bus.h>

#define MCHN_MAX_NUM 32

struct buffer_pool {
	int size;
	int free;
	int payload;
	void *head;
	char *mem;
	spinlock_t lock;
	unsigned long irq_flags;
};

struct mchn_info_t {
	struct mchn_ops_t *ops[MCHN_MAX_NUM];
	struct {
		struct buffer_pool pool;
	} chn_public[MCHN_MAX_NUM];
};

/* configuration channel */
int mchn_init(struct mchn_ops_t *ops);
/* cancellation channel */
int mchn_deinit(struct mchn_ops_t *ops);
/* push link list */
int mchn_push_link(int channel, struct mbuf_t *head,
		   struct mbuf_t *tail, int num);
/* push link list, Using a blocking mode, Timeout wait for tx_complete */
int mchn_push_link_wait_complete(int chn, struct mbuf_t *head,
				 struct mbuf_t *tail, int num, int timeout);
int mchn_hw_pop_link(int chn, void *head, void *tail, int num);
int mchn_hw_tx_complete(int chn, int timeout);
int mchn_hw_req_push_link(int chn, int need);
int mbuf_link_alloc(int chn, struct mbuf_t **head, struct mbuf_t **tail,
		     int *num);
int mbuf_link_free(int chn, struct mbuf_t *head,
		   struct mbuf_t *tail, int num);
int mchn_hw_max_pending(int chn);
int mchn_wcn_mem_write(unsigned int addr, void *buf, unsigned int len);
int mchn_wcn_mem_read(unsigned int addr, void *buf, unsigned int len);
int mchn_wcn_update_bits(unsigned int reg, unsigned int mask, unsigned int val);
struct mchn_info_t *mchn_info(void);
struct mchn_ops_t *mchn_ops(int channel);
#endif
