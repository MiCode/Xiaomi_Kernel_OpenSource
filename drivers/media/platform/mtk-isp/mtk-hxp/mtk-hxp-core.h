/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MTK_HXP_CORE_H
#define MTK_HXP_CORE_H

#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/dma-heap.h>
#include <linux/dma-buf.h>

#include "mtk-hxp-aov.h"
#include "mtk-hxp-queue.h"

#include "./alloc/tlsf/tlsf_alloc.h"

#define HXP_TIMEOUT_MS  4000U

// Forward declaration
struct mtk_hxp;

struct buffer {
	struct list_head entry;
	struct aov_event data;
};

struct hxp_core {
	struct packet packet;

	atomic_t disp_mode;

	wait_queue_head_t scp_queue;
	atomic_t scp_session;
	atomic_t scp_ready;
	atomic_t aov_session;
	atomic_t aov_ready;

	phys_addr_t buf_pa;
	uint8_t *buf_va;
	size_t buf_size;
	struct tlsf_info alloc;
	spinlock_t buf_lock;
	struct aov_init *aov_init;

	struct dma_buf *dma_buf;
	struct dma_buf_map dma_map;
	struct buffer *event_data;
	struct list_head event_list;
	spinlock_t event_lock;

	wait_queue_head_t ack_wq[HXP_AOV_CMD_MAX];
	atomic_t ack_cmd[HXP_AOV_CMD_MAX];

	wait_queue_head_t poll_wq;
	struct queue queue;
};

int hxp_core_init(struct mtk_hxp *device);

struct mtk_hxp *hxp_core_get_device(void);

void chans_pool_dump(struct mtk_hxp *hxp_dev);

bool chan_pool_available(struct mtk_hxp *hxp_dev,
	int module_id);

int hxp_core_send_cmd(struct mtk_hxp *hxp_dev,
	uint32_t cmd, void *data, int len, bool ack);

int hxp_core_copy(struct mtk_hxp *hxp_dev,
	struct aov_dqevent *dequeue);

int hxp_core_poll(struct mtk_hxp *hxp_dev,
	struct file *file, poll_table *wait);

int hxp_core_uninit(struct mtk_hxp *hxp_dev);

#endif  // MTK_HXP_CORE_H
