/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef _MSM_STATS_BUF_H_
#define _MSM_STATS_BUF_H_

enum msm_stats_buffer_state {
	MSM_STATS_BUFFER_STATE_UNUSED,	  /* not used */
	MSM_STATS_BUFFER_STATE_INITIALIZED,	   /* REQBUF done */
	MSM_STATS_BUFFER_STATE_PREPARED,	/* BUF mapped */
	MSM_STATS_BUFFER_STATE_QUEUED,	  /* buf queued */
	MSM_STATS_BUFFER_STATE_DEQUEUED,	/* in use in VFE */
	MSM_STATS_BUFFER_STATE_DISPATCHED,	  /* sent to userspace */
};

struct msm_stats_meta_buf {
	struct list_head list;
	enum msm_stats_buffer_state state;
	int type;
	int fd;
	uint32_t offset;
	unsigned long paddr;
	unsigned long len;
	struct file *file;
	struct msm_stats_buf_info info;
	struct ion_handle *handle;
};

struct msm_stats_bufq {
	struct list_head head;
	int num_bufs;
	int type;
	struct msm_stats_meta_buf *bufs;
};


struct msm_stats_bufq_ctrl {
	/* not use spin lock for now. Assume vfe holds spin lock */
	spinlock_t lock;
	int init_done;
	struct msm_stats_bufq *bufq[MSM_STATS_TYPE_MAX];
};

struct msm_stats_ops {
	struct msm_stats_bufq_ctrl *stats_ctrl;
	struct ion_client *client;
	int (*enqueue_buf) (struct msm_stats_bufq_ctrl *stats_ctrl,
						struct msm_stats_buf_info *info,
						struct ion_client *client);
	int (*qbuf) (struct msm_stats_bufq_ctrl *stats_ctrl,
				 enum msm_stats_enum_type stats_type,
				 int buf_idx);
	int (*dqbuf) (struct msm_stats_bufq_ctrl *stats_ctrl,
				  enum msm_stats_enum_type stats_type,
				  struct msm_stats_meta_buf **pp_stats_buf);
	int (*bufq_flush) (struct msm_stats_bufq_ctrl *stats_ctrl,
					   enum msm_stats_enum_type stats_type,
					   struct ion_client *client);
	int (*buf_unprepare) (struct msm_stats_bufq_ctrl *stats_ctrl,
		enum msm_stats_enum_type stats_type,
		int buf_idx,
		struct ion_client *client);
	int (*buf_prepare) (struct msm_stats_bufq_ctrl *stats_ctrl,
						struct msm_stats_buf_info *info,
						struct ion_client *client);
	int (*reqbuf) (struct msm_stats_bufq_ctrl *stats_ctrl,
				   struct msm_stats_reqbuf *reqbuf,
				   struct ion_client *client);
	int (*dispatch) (struct msm_stats_bufq_ctrl *stats_ctrl,
		enum msm_stats_enum_type stats_type,
		unsigned long phy_addr, int *buf_idx, void **vaddr, int *fd,
		struct ion_client *client);
	int (*querybuf) (struct msm_stats_bufq_ctrl *stats_ctrl,
		struct msm_stats_buf_info *info,
		struct msm_stats_meta_buf **pp_stats_buf);
	int (*stats_ctrl_init) (struct msm_stats_bufq_ctrl *stats_ctrl);
	int (*stats_ctrl_deinit) (struct msm_stats_bufq_ctrl *stats_ctrl);
};

int msm_stats_buf_ops_init(struct msm_stats_bufq_ctrl *stats_ctrl,
	struct ion_client *client, struct msm_stats_ops *ops);

#endif /* _MSM_STATS_BUF_H_ */
