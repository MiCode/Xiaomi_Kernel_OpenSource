/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _GSP_SYNC_H
#define _GSP_SYNC_H

#include <linux/types.h>
#include <linux/kconfig.h>
#include <linux/sync_file.h>

#include <drm/gsp_cfg.h>

#define GSP_WAIT_FENCE_TIMEOUT 3000/* 3000ms */
#define GSP_WAIT_FENCE_MAX 8

struct gsp_sync_timeline {
	unsigned int fence_context;
	spinlock_t fence_lock;
	unsigned long fence_seqno;
	char timeline_name[32];
	char driver_name[32];
};

struct gsp_fence_data {

	/* manage wait&sig sync fence */
	struct dma_fence *sig_fen;
	struct dma_fence *wait_fen_arr[GSP_WAIT_FENCE_MAX];
	int wait_cnt;

	/* judge handling fence or not */
	int32_t __user *ufd;
	struct gsp_sync_timeline *tl;
};

int gsp_sync_fence_process(struct gsp_layer *layer,
			struct gsp_fence_data *data, bool last);

void gsp_sync_fence_data_setup(struct gsp_fence_data *data,
				struct gsp_sync_timeline *tl, int __user *ufd);

void gsp_sync_fence_signal(struct gsp_fence_data *data);

void gsp_sync_fence_free(struct gsp_fence_data *data);

int gsp_sync_fence_wait(struct gsp_fence_data *data);

struct gsp_sync_timeline *gsp_sync_timeline_create(const char *name);
void gsp_sync_timeline_destroy(struct gsp_sync_timeline *obj);
#endif
