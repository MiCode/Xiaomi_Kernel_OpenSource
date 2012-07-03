/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/debugfs.h>
#include <linux/list.h>
#include <linux/ktime.h>

#ifndef _WFD_UTIL_H_
#define _WFD_UTIL_H_

/*#define DEBUG_WFD*/

#define WFD_TAG "wfd: "
#ifdef DEBUG_WFD
	#define WFD_MSG_INFO(fmt...) pr_info(WFD_TAG fmt)
	#define WFD_MSG_WARN(fmt...) pr_warning(WFD_TAG fmt)
#else
	#define WFD_MSG_INFO(fmt...)
	#define WFD_MSG_WARN(fmt...)
#endif
	#define WFD_MSG_ERR(fmt...) pr_err(KERN_ERR WFD_TAG fmt)
	#define WFD_MSG_CRIT(fmt...) pr_crit(KERN_CRIT WFD_TAG fmt)
	#define WFD_MSG_DBG(fmt...) pr_debug(WFD_TAG fmt)


struct wfd_stats_encode_sample {
	ktime_t encode_start_ts;
	struct list_head list;
};

struct wfd_stats {
	/* Output Buffers */
	uint32_t v4l2_buf_count;

	/* Input Buffers */
	uint32_t mdp_buf_count;
	uint32_t vsg_buf_count;
	uint32_t enc_buf_count;

	/* Other */
	uint32_t frames_encoded;
	uint32_t mdp_updates;

	uint32_t enc_avg_latency;
	uint32_t enc_cumulative_latency;
	uint32_t enc_latency_samples;
	struct list_head enc_queue;

	/* Debugfs entries */
	struct dentry *d_parent;
	struct dentry *d_v4l2_buf_count;
	struct dentry *d_mdp_buf_count;
	struct dentry *d_vsg_buf_count;
	struct dentry *d_enc_buf_count;
	struct dentry *d_frames_encoded;
	struct dentry *d_mdp_updates;
	struct dentry *d_enc_avg_latency;
};

enum wfd_stats_event {
	WFD_STAT_EVENT_CLIENT_QUEUE,
	WFD_STAT_EVENT_CLIENT_DEQUEUE,

	WFD_STAT_EVENT_MDP_QUEUE,
	WFD_STAT_EVENT_MDP_DEQUEUE,

	WFD_STAT_EVENT_VSG_QUEUE,
	WFD_STAT_EVENT_VSG_DEQUEUE,

	WFD_STAT_EVENT_ENC_QUEUE,
	WFD_STAT_EVENT_ENC_DEQUEUE,
};

int wfd_stats_setup(void);
int wfd_stats_init(struct wfd_stats *, int device);
int wfd_stats_update(struct wfd_stats *, enum wfd_stats_event);
int wfd_stats_deinit(struct wfd_stats *);
void wfd_stats_teardown(void);
#endif
