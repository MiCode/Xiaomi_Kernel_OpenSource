/* Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
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
#include "msm_sd.h"

/* Buffer type could be userspace / HAL.
 * Userspase could provide native or scratch buffer. */
#define BUF_SRC(id) ( \
		(id & ISP_SCRATCH_BUF_BIT) ? MSM_ISP_BUFFER_SRC_SCRATCH : \
		(id & ISP_NATIVE_BUF_BIT) ? MSM_ISP_BUFFER_SRC_NATIVE : \
				MSM_ISP_BUFFER_SRC_HAL)

/*
 * This mask can be set dynamically if there are more than 2 VFE
 *.and 2 of those are used
 */
#define ISP_SHARE_BUF_MASK 0x3
#define ISP_NUM_BUF_MASK 2
#define BUF_MGR_NUM_BUF_Q 28
#define MAX_IOMMU_CTX 2

#define MSM_ISP_INVALID_BUF_INDEX 0xFFFFFFFF

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

enum msm_isp_buffer_put_state {
	MSM_ISP_BUFFER_STATE_PUT_PREPARED,  /* on init */
	MSM_ISP_BUFFER_STATE_PUT_BUF,       /* on rotation */
	MSM_ISP_BUFFER_STATE_FLUSH,         /* on recovery */
	MSM_ISP_BUFFER_STATE_DROP_REG,      /* on drop frame for reg_update */
	MSM_ISP_BUFFER_STATE_DROP_SKIP,      /* on drop frame for sw skip */
	MSM_ISP_BUFFER_STATE_RETURN_EMPTY,  /* for return empty */
};

enum msm_isp_buffer_flush_t {
	MSM_ISP_BUFFER_FLUSH_DIVERTED,
	MSM_ISP_BUFFER_FLUSH_ALL,
};

enum msm_isp_buf_mgr_state {
	MSM_ISP_BUF_MGR_ATTACH,
	MSM_ISP_BUF_MGR_DETACH,
};

struct msm_isp_buffer_mapped_info {
	size_t len;
	dma_addr_t paddr;
	int buf_fd;
};

struct buffer_cmd {
	struct list_head list;
	struct msm_isp_buffer_mapped_info *mapped_info;
};

struct msm_isp_buffer_debug_t {
	enum msm_isp_buffer_put_state put_state[2];
	uint8_t put_state_last;
};

struct msm_isp_buffer {
	/*Common Data structure*/
	int num_planes;
	struct msm_isp_buffer_mapped_info mapped_info[VIDEO_MAX_PLANES];
	int buf_idx;
	uint32_t bufq_handle;
	uint32_t frame_id;
	struct timeval *tv;
	/* Indicates whether buffer is used as ping ot pong buffer */
	uint32_t pingpong_bit;
	/* Indicates buffer is reconfig due to drop frame */
	uint32_t is_drop_reconfig;

	/*Native buffer*/
	struct list_head list;
	enum msm_isp_buffer_state state;

	struct msm_isp_buffer_debug_t buf_debug;

	/*Vb2 buffer data*/
	struct vb2_v4l2_buffer *vb2_v4l2_buf;
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
	enum smmu_attach_mode security_mode;
};

struct msm_isp_buf_ops {
	int (*request_buf)(struct msm_isp_buf_mgr *buf_mgr,
		struct msm_isp_buf_request_ver2 *buf_request);

	int (*enqueue_buf)(struct msm_isp_buf_mgr *buf_mgr,
		struct msm_isp_qbuf_info *info);

	int (*dequeue_buf)(struct msm_isp_buf_mgr *buf_mgr,
		struct msm_isp_qbuf_info *info);

	int (*release_buf)(struct msm_isp_buf_mgr *buf_mgr,
		uint32_t bufq_handle);

	int (*get_bufq_handle)(struct msm_isp_buf_mgr *buf_mgr,
		uint32_t session_id, uint32_t stream_id);

	int (*get_buf_src)(struct msm_isp_buf_mgr *buf_mgr,
		uint32_t bufq_handle, uint32_t *buf_src);

	int (*get_buf)(struct msm_isp_buf_mgr *buf_mgr, uint32_t id,
		uint32_t bufq_handle, uint32_t buf_index,
		struct msm_isp_buffer **buf_info);

	int (*get_buf_by_index)(struct msm_isp_buf_mgr *buf_mgr,
		uint32_t bufq_handle, uint32_t buf_index,
		struct msm_isp_buffer **buf_info);

	int (*map_buf)(struct msm_isp_buf_mgr *buf_mgr,
		struct msm_isp_buffer_mapped_info *mapped_info, uint32_t fd);

	int (*unmap_buf)(struct msm_isp_buf_mgr *buf_mgr, uint32_t fd);

	int (*put_buf)(struct msm_isp_buf_mgr *buf_mgr,
		uint32_t bufq_handle, uint32_t buf_index);

	int (*flush_buf)(struct msm_isp_buf_mgr *buf_mgr,
	uint32_t bufq_handle, enum msm_isp_buffer_flush_t flush_type,
	struct timeval *tv, uint32_t frame_id);

	int (*buf_done)(struct msm_isp_buf_mgr *buf_mgr,
		uint32_t bufq_handle, uint32_t buf_index,
		struct timeval *tv, uint32_t frame_id, uint32_t output_format);
	void (*register_ctx)(struct msm_isp_buf_mgr *buf_mgr,
		struct device **iommu_ctx1, struct device **iommu_ctx2,
		int num_iommu_ctx1, int num_iommu_ctx2);
	int (*buf_mgr_init)(struct msm_isp_buf_mgr *buf_mgr,
		const char *ctx_name);
	int (*buf_mgr_deinit)(struct msm_isp_buf_mgr *buf_mgr);
	int (*buf_mgr_debug)(struct msm_isp_buf_mgr *buf_mgr,
		unsigned long fault_addr);
	struct msm_isp_bufq * (*get_bufq)(struct msm_isp_buf_mgr *buf_mgr,
		uint32_t bufq_handle);
	int (*buf_divert)(struct msm_isp_buf_mgr *buf_mgr,
			uint32_t bufq_handle, uint32_t buf_index,
			struct timeval *tv, uint32_t frame_id);
};

struct msm_isp_buf_mgr {
	int init_done;
	uint32_t open_count;
	uint32_t pagefault_debug_disable;
	uint32_t frameId_mismatch_recovery;
	uint16_t num_buf_q;
	struct msm_isp_bufq bufq[BUF_MGR_NUM_BUF_Q];

	struct ion_client *client;
	struct msm_isp_buf_ops *ops;

	struct msm_sd_req_vb2_q *vb2_ops;


	/*Add secure mode*/
	int secure_enable;

	int attach_ref_cnt;
	enum msm_isp_buf_mgr_state attach_state;
	struct device *isp_dev;
	struct mutex lock;
	/* Scratch buffer */
	dma_addr_t scratch_buf_addr;
	dma_addr_t scratch_buf_stats_addr;
	uint32_t scratch_buf_range;
	int iommu_hdl;
	struct ion_handle *sc_handle;
};

int msm_isp_create_isp_buf_mgr(struct msm_isp_buf_mgr *buf_mgr,
	struct msm_sd_req_vb2_q *vb2_ops, struct device *dev,
	uint32_t scratch_addr_range);

int msm_isp_proc_buf_cmd(struct msm_isp_buf_mgr *buf_mgr,
	unsigned int cmd, void *arg);

int msm_isp_smmu_attach(struct msm_isp_buf_mgr *buf_mgr,
	void *arg);

#endif /* _MSM_ISP_BUF_H_ */
