/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_CAM_DEBUG__
#define __MTK_CAM_DEBUG__

#include <linux/debugfs.h>
#include "mtk_cam.h"

#define MTK_CAM_DEBUG_DUMP_MAX_BUF	(33 * 5)
#define MTK_CAM_DEBUG_DUMP_DESC_SIZE	64

struct mtk_cam_dump_header {
	/* Common Debug Information*/
	__u8	desc[MTK_CAM_DEBUG_DUMP_DESC_SIZE];
	__u32	request_fd;
	__u32   buffer_state;
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
	__u32	cq_iova;
	__u32	cq_desc_size;

	/* meta in */
	__u32	meta_in_dump_buf_offset;
	__u32	meta_in_dump_buf_size;
	__u32	meta_in_iova;

	/* meta out 0 */
	__u32	meta_out_0_dump_buf_offset;
	__u32	meta_out_0_dump_buf_size;
	__u32	meta_out_0_iova;

	/* meta out 1 */
	__u32	meta_out_1_dump_buf_offset;
	__u32	meta_out_1_dump_buf_size;
	__u32	meta_out_1_iova;

	/* meta out 2 */
	__u32	meta_out_2_dump_buf_offset;
	__u32	meta_out_2_dump_buf_size;
	__u32	meta_out_2_iova;

	/* status dump */
	__u32	status_dump_offset;
	__u32	status_dump_size;

};

struct mtk_cam_debug_fs;
struct mtk_cam_dump_param;

struct mtk_cam_debug_ops {
	int (*init)(struct mtk_cam_debug_fs *debug_fs,
		    struct mtk_cam_device *cam_dev, int content_size,
		    int buf_num);
	int (*dump)(struct mtk_cam_debug_fs *debug_fs,
		    struct mtk_cam_dump_param *param);
	int (*exp_dump)(struct mtk_cam_debug_fs *debug_fs,
			struct mtk_cam_dump_param *param);
	int (*realloc)(struct mtk_cam_debug_fs *debug_fs,
		       int num_of_bufs);
	void (*deinit)(struct mtk_cam_debug_fs *debug_fs);
};

struct mtk_cam_debug_fs {
	struct mtk_cam_device *cam_dev;
	void *exp_dump_buf; /* resreved for kernel exception */
	void *dump_buf[MTK_CAM_DEBUG_DUMP_MAX_BUF];
	int dump_buf_head_idx;
	int dump_buf_tail_idx;
	int dump_buf_entry_size;
	int dump_buf_entry_num;
	int dump_count;
	atomic_t dump_state;
	atomic_t exp_dump_state;
	int reader_count;
	int exp_reader_count;
	struct mutex dump_buf_lock;
	struct mutex exp_dump_buf_lock;

	struct mtk_cam_debug_ops *ops;
	wait_queue_head_t exp_dump_waitq;
	struct dentry *dump_entry;
	struct dentry *exp_dump_entry;

};

#ifndef CONFIG_DEBUG_FS
static inline struct mtk_cam_debug_fs *mtk_cam_get_debugfs(void)
{
	return NULL;
}
#else
struct mtk_cam_debug_fs *mtk_cam_get_debugfs(void);
#endif /* CONFIG_DEBUG_FS */

#endif /* __MTK_CAM_DEBUG__ */
