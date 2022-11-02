/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_CAM_DEBUG__
#define __MTK_CAM_DEBUG__

#include <linux/debugfs.h>
struct mtk_cam_debug_fs;
struct mtk_cam_device;
struct mtk_cam_dump_param;
struct mtk_raw_device;
struct mtk_cam_request;
struct mtk_cam_request_stream_data;
struct mtk_cam_debug_fs;

#define MTK_CAM_DEBUG_DUMP_MAX_BUF		(33 * 5)
#define MTK_CAM_DEBUG_DUMP_DESC_SIZE		64
#define MTK_CAM_DEBUG_DUMP_HEADER_MAX_SIZE	0x1000

/* Force dump by user */
#define MTK_CAM_REQ_DUMP_FORCE			BIT(0)
/* Triggered when dequeue failed */
#define MTK_CAM_REQ_DUMP_DEQUEUE_FAILED		BIT(1)
/**
 * Triggered when SOF may not come aganin. In this
 * case, we will check if the request's state is
 * still the same as the original one and start the
 * dump if the state does not change.
 */
#define MTK_CAM_REQ_DUMP_CHK_DEQUEUE_FAILED	BIT(2)


#define MTK_CAM_REQ_DBGWORK_S_INIT		0
#define MTK_CAM_REQ_DBGWORK_S_PREPARED		1
#define MTK_CAM_REQ_DBGWORK_S_FINISHED		2
#define MTK_CAM_REQ_DBGWORK_S_CANCEL		3

struct mtk_cam_dump_param {
	/* Common Debug Information*/
	char *desc;
	__u32 request_fd;
	__u32 stream_id;
	__u64 timestamp;
	__u32 sequence;

	/* CQ dump */
	void *cq_cpu_addr;
	__u32 cq_size;
	__u64 cq_iova;
	__u32 cq_desc_offset;
	__u32 cq_desc_size;
	__u32 sub_cq_desc_offset;
	__u32 sub_cq_desc_size;

	/* meta in */
	void *meta_in_cpu_addr;
	__u32 meta_in_dump_buf_size;
	__u64 meta_in_iova;

	/* meta out 0 */
	void *meta_out_0_cpu_addr;
	__u32 meta_out_0_dump_buf_size;
	__u64 meta_out_0_iova;

	/* meta out 1 */
	void *meta_out_1_cpu_addr;
	__u32 meta_out_1_dump_buf_size;
	__u64 meta_out_1_iova;

	/* meta out 2 */
	void *meta_out_2_cpu_addr;
	__u32 meta_out_2_dump_buf_size;
	__u64 meta_out_2_iova;

	/* ipi frame param */
	struct mtkcam_ipi_frame_param *frame_params;
	__u32 frame_param_size;

	/* ipi config param */
	struct mtkcam_ipi_config_param *config_params;
	__u32 config_param_size;
};

struct mtk_cam_seninf_dump_work {
	struct work_struct work;
	struct v4l2_subdev *seninf;
	unsigned int frame_seq_no;
};

struct mtk_cam_req_dbg_work {
	struct work_struct work;
	struct mtk_cam_request_stream_data *s_data;
	atomic_t state;
	unsigned int dump_flags;
	int buffer_state;
	char desc[MTK_CAM_DEBUG_DUMP_DESC_SIZE];
	bool smi_dump;
};

struct mtk_cam_dump_header {
	/* Common Debug Information*/
	__u8	desc[MTK_CAM_DEBUG_DUMP_DESC_SIZE];
	__u32	request_fd;
	__u32   stream_id;
	__u64	timestamp;
	__u32	sequence;
	__u32	header_size;
	__u32	payload_offset;
	__u32	payload_size;

	/* meta file information */
	__u32	meta_version_major;
	__u32	meta_version_minor;

	/* CQ dump */
	__u32	cq_dump_buf_offset;
	__u32	cq_size;
	__u64	cq_iova;
	__u32	cq_desc_offset;
	__u32	cq_desc_size;
	__u32	sub_cq_desc_offset;
	__u32	sub_cq_desc_size;

	/* meta in */
	__u32	meta_in_dump_buf_offset;
	__u32	meta_in_dump_buf_size;
	__u64	meta_in_iova;

	/* meta out 0 */
	__u32	meta_out_0_dump_buf_offset;
	__u32	meta_out_0_dump_buf_size;
	__u64	meta_out_0_iova;

	/* meta out 1 */
	__u32	meta_out_1_dump_buf_offset;
	__u32	meta_out_1_dump_buf_size;
	__u64	meta_out_1_iova;

	/* meta out 2 */
	__u32	meta_out_2_dump_buf_offset;
	__u32	meta_out_2_dump_buf_size;
	__u64	meta_out_2_iova;

	/* status dump */
	__u32	status_dump_offset;
	__u32	status_dump_size;

	/* ipi frame param */
	__u32	frame_dump_offset;
	__u32	frame_dump_size;

	/* ipi config param */
	__u32	config_dump_offset;
	__u32	config_dump_size;
	__u32	used_stream_num;
};

struct mtk_cam_dump_ctrl_block {
	atomic_t state;
	void *buf;
};

struct mtk_cam_dump_buf_ctrl {
	int pipe_id;
	struct mtk_cam_debug_fs *debug_fs;
	struct mtk_cam_dump_ctrl_block blocks[MTK_CAM_DEBUG_DUMP_MAX_BUF];
	struct dentry *dir_entry;
	struct dentry *ctrl_entry;
	struct dentry *data_entry;
	struct mutex ctrl_lock;
	int head;
	int tail;
	int num;
	int count;
	int cur_read;
	atomic_t dump_state;
	struct dentry *dump_entry;
};

struct mtk_cam_debug_ops {
	int (*init)(struct mtk_cam_debug_fs *debug_fs,
		    struct mtk_cam_device *cam, int content_size);
	int (*dump)(struct mtk_cam_debug_fs *debug_fs,
		    struct mtk_cam_dump_param *param);
	int (*exp_dump)(struct mtk_cam_debug_fs *debug_fs,
			struct mtk_cam_dump_param *param);
	int (*exp_reinit)(struct mtk_cam_debug_fs *debug_fs);
	int (*reinit)(struct mtk_cam_debug_fs *debug_fs,
		      int stream_id);
	void (*deinit)(struct mtk_cam_debug_fs *debug_fs);
};

struct mtk_cam_debug_fs {
	struct mtk_cam_device *cam;
	uint force_dump;
	void *exp_dump_buf; /* kernel exception dump */
	atomic_t exp_dump_state;
	struct mutex exp_dump_buf_lock;
	struct dentry *exp_dump_entry;
	int buf_size;
	struct dentry *dbg_entry;
	struct mtk_cam_dump_buf_ctrl ctrl[MTKCAM_SUBDEV_MAX];
	struct mtk_cam_debug_ops *ops;
};

#ifndef CONFIG_DEBUG_FS
static inline struct mtk_cam_debug_fs *mtk_cam_get_debugfs(void)
{
	return NULL;
}

static inline int mtk_cam_req_dump(struct mtk_cam_request_stream_data *s_data,
				   unsigned int dump_flag, char *desc, bool smi_dump)
{
	return 0;
}

static inline void
mtk_cam_debug_detect_dequeue_failed(struct mtk_cam_request_stream_data *s_data,
				    const unsigned int frame_no_update_limit,
				    struct mtk_camsys_irq_info *irq_info,
				    struct mtk_raw_device *raw_dev)
{
}

static inline void mtk_cam_debug_wakeup(struct wait_queue_head *wq_head)
{
}

static inline void
mtk_cam_req_dump_work_init(struct mtk_cam_request_stream_data *s_data)
{
}

static inline void
mtk_cam_req_dbg_works_clean(struct mtk_cam_request_stream_data *s_data)
{
}

static inline void
mtk_cam_debug_seninf_dump(struct mtk_cam_request_stream_data *s_data)
{
}

#else
struct mtk_cam_debug_fs *mtk_cam_get_debugfs(void);

int mtk_cam_req_dump(struct mtk_cam_request_stream_data *s_data,
		     unsigned int dump_flag, char *desc, bool smi_dump);
void
mtk_cam_debug_detect_dequeue_failed(struct mtk_cam_request_stream_data *s_data,
				    const unsigned int frame_no_update_limit,
				    struct mtk_camsys_irq_info *irq_info,
				    struct mtk_raw_device *raw_dev);
void mtk_cam_debug_wakeup(struct wait_queue_head *wq_head);

void mtk_cam_req_dump_work_init(struct mtk_cam_request_stream_data *s_data);

void mtk_cam_req_dbg_works_clean(struct mtk_cam_request_stream_data *s_data);

void
mtk_cam_debug_seninf_dump(struct mtk_cam_request_stream_data *s_data);

#endif /* CONFIG_DEBUG_FS */

static inline struct mtk_cam_req_dbg_work *
to_mtk_cam_req_dbg_work(struct work_struct *__work)
{
	return container_of(__work, struct mtk_cam_req_dbg_work, work);
}

#endif /* __MTK_CAM_DEBUG__ */
