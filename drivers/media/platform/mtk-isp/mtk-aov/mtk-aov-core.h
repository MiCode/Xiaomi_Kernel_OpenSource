/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 *
 * Author: ChenHung Yang <chenhung.yang@mediatek.com>
 */

#ifndef MTK_AOV_CORE_H
#define MTK_AOV_CORE_H

#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/dma-heap.h>
#include <linux/dma-buf.h>

#include "mtk-aov-data.h"
#include "mtk-aov-queue.h"

#include "./alloc/tlsf/tlsf_alloc.h"

#define AOV_TIMEOUT_MS  4000U

// Forward declaration
struct mtk_aov;

struct buffer {
	struct list_head entry;
	struct aov_event data;
};

struct aov_core {
	struct packet packet;

	atomic_t debug_mode;
	atomic_t disp_mode;
	atomic_t aie_avail;

	wait_queue_head_t scp_queue;
	atomic_t scp_session;
	atomic_t scp_ready;
	atomic_t aov_session;
	atomic_t aov_ready;
	atomic_t cmd_seq;

	uint32_t sensor_id;

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

	wait_queue_head_t ack_wq[AOV_SCP_CMD_MAX];
	atomic_t ack_cmd[AOV_SCP_CMD_MAX];

	wait_queue_head_t poll_wq;
	struct queue event;
	struct queue queue;
};

int aov_core_init(struct mtk_aov *device);

struct mtk_aov *aov_core_get_device(void);

int aov_core_send_cmd(struct mtk_aov *aov_dev,
	uint32_t cmd, void *data, int len, bool ack);

int aov_core_notify(struct mtk_aov *aov_dev,
	void *data, bool enable);

int aov_core_copy(struct mtk_aov *aov_dev,
	struct aov_dqevent *dequeue);

int aov_core_poll(struct mtk_aov *aov_dev,
	struct file *file, poll_table *wait);

int aov_core_reset(struct mtk_aov *device);

int aov_core_uninit(struct mtk_aov *aov_dev);

#endif  // MTK_AOV_CORE_H
