/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MSM_ISP_BUF_H_
#define _MSM_ISP_BUF_H_

#include <media/msmb_isp.h>
#include <linux/msm_iommu_domains.h>
#include "msm_sd.h"

/* Buffer type could be userspace / HAL.
 * Userspase could provide native or scratch buffer. */
#define BUF_SRC(id) ( \
		(id & ISP_SCRATCH_BUF_BIT) ? MSM_ISP_BUFFER_SRC_SCRATCH : \
		(id & ISP_NATIVE_BUF_BIT) ? MSM_ISP_BUFFER_SRC_NATIVE : \
				MSM_ISP_BUFFER_SRC_HAL)

#define ISP_SHARE_BUF_CLIENT 2

struct msm_isp_buf_mgr;

enum msm_isp_buffer_src_t {
	MSM_ISP_BUFFER_SRC_HAL,
	MSM_ISP_BUFFER_SRC_NATIVE,
	MSM_ISP_BUFFER_SRC_SCRATCH,
	MSM_ISP_BUFFER_SRC_MAX,
};

enum msm_isp_buffer_state {
	MSM_ISP_BUFFER_STATE_UNUSED,         /* not used */
	MSM_ISP_BUFFER_STATE_INITIALIZED,    /* REQBUF done */
	MSM_ISP_BUFFER_STATE_PREPARED,       /* BUF mapped */
	MSM_ISP_BUFFER_STATE_QUEUED,         /* buf queued */
	MSM_ISP_BUFFER_STATE_DEQUEUED,       /* in use in VFE */
	MSM_ISP_BUFFER_STATE_DIVERTED,       /* Sent to other hardware*/
	MSM_ISP_BUFFER_STATE_DISPATCHED,     /* Sent to HAL*/
};

enum msm_isp_buffer_flush_t {
	MSM_ISP_BUFFER_FLUSH_DIVERTED,
	MSM_ISP_BUFFER_FLUSH_ALL,
};

struct msm_isp_buffer_mapped_info {
	unsigned long len;
	dma_addr_t paddr;
	struct ion_handle *handle;
};

struct buffer_cmd {
	struct list_head list;
	struct msm_isp_buffer_mapped_info *mapped_info;
};

struct msm_isp_buffer {
	/*Common Data structure*/
	int num_planes;
	struct msm_isp_buffer_mapped_info mapped_info[VIDEO_MAX_PLANES];
	int buf_idx;
	uint32_t bufq_handle;
	uint32_t frame_id;
	struct timeval *tv;

	/*Native buffer*/
	struct list_head list;
	enum msm_isp_buffer_state state;

	/*Vb2 buffer data*/
	struct vb2_buffer *vb2_buf;

	/*Share buffer cache state*/
	struct list_head share_list;
	uint8_t buf_used[ISP_SHARE_BUF_CLIENT];
	uint8_t buf_get_count;
	uint8_t buf_put_count;
	uint8_t buf_reuse_flag;
};

struct msm_isp_bufq {
	uint32_t session_id;
	uint32_t stream_id;
	uint32_t num_bufs;
	uint32_t bufq_handle;
	enum msm_isp_buf_type buf_type;
	struct msm_isp_buffer *bufs;
	spinlock_t bufq_lock;

	/*Native buffer queue*/
	struct list_head head;
	/*Share buffer cache queue*/
	struct list_head share_head;
	uint8_t buf_client_count;
};

struct msm_isp_buf_ops {
	int (*request_buf) (struct msm_isp_buf_mgr *buf_mgr,
		struct msm_isp_buf_request *buf_request);

	int (*enqueue_buf) (struct msm_isp_buf_mgr *buf_mgr,
		struct msm_isp_qbuf_info *info);

	int (*release_buf) (struct msm_isp_buf_mgr *buf_mgr,
		uint32_t bufq_handle);

	int (*get_bufq_handle) (struct msm_isp_buf_mgr *buf_mgr,
		uint32_t session_id, uint32_t stream_id);

	int (*get_buf_src) (struct msm_isp_buf_mgr *buf_mgr,
		uint32_t bufq_handle, uint32_t *buf_src);

	int (*get_buf) (struct msm_isp_buf_mgr *buf_mgr, uint32_t id,
		uint32_t bufq_handle, struct msm_isp_buffer **buf_info);

	int (*put_buf) (struct msm_isp_buf_mgr *buf_mgr,
		uint32_t bufq_handle, uint32_t buf_index);

	int (*flush_buf) (struct msm_isp_buf_mgr *buf_mgr,
		uint32_t bufq_handle, enum msm_isp_buffer_flush_t flush_type);

	int (*buf_done) (struct msm_isp_buf_mgr *buf_mgr,
		uint32_t bufq_handle, uint32_t buf_index,
		struct timeval *tv, uint32_t frame_id, uint32_t output_format);
	int (*buf_divert) (struct msm_isp_buf_mgr *buf_mgr,
		uint32_t bufq_handle, uint32_t buf_index,
		struct timeval *tv, uint32_t frame_id);
	void (*register_ctx) (struct msm_isp_buf_mgr *buf_mgr,
		struct device **iommu_ctx, int num_iommu_ctx);
	int (*buf_mgr_init) (struct msm_isp_buf_mgr *buf_mgr,
		const char *ctx_name, uint16_t num_buf_q);
	int (*buf_mgr_deinit) (struct msm_isp_buf_mgr *buf_mgr);
};

struct msm_isp_buf_mgr {
	int init_done;
	uint32_t open_count;
	spinlock_t lock;
	uint16_t num_buf_q;
	struct msm_isp_bufq *bufq;

	struct ion_client *client;
	struct msm_isp_buf_ops *ops;
	uint32_t buf_handle_cnt;

	struct msm_sd_req_vb2_q *vb2_ops;

	/*IOMMU specific*/
	int iommu_domain_num;
	struct iommu_domain *iommu_domain;

	int num_iommu_ctx;
	struct device *iommu_ctx[2];
	struct list_head buffer_q;
};

int msm_isp_create_isp_buf_mgr(struct msm_isp_buf_mgr *buf_mgr,
	struct msm_sd_req_vb2_q *vb2_ops, struct msm_iova_layout *iova_layout);

int msm_isp_proc_buf_cmd(struct msm_isp_buf_mgr *buf_mgr,
	unsigned int cmd, void *arg);

#endif /* _MSM_ISP_BUF_H_ */
