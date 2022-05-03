/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_REQUEST_H
#define __MTK_CAM_REQUEST_H

#include <linux/list.h>
#include <media/media-request.h>

/**
 * mtk_cam_frame_sync: the frame sync state of one request
 *
 * @target: the num of ctx(sensor) which should be synced
 * @on_cnt: the count of frame sync on called by ctx
 * @off_cnt: the count of frame sync off called by ctx
 * @op_lock: protect frame sync state variables
 */
struct mtk_cam_frame_sync {
	unsigned int target;
	unsigned int on_cnt;
	unsigned int off_cnt;
	struct mutex op_lock;
};

static inline void _used_mask_set(int *m, int idx, int ofst, int num)
{
	WARN_ON(idx < 0 || idx >= num);
	*m |= (1 << (idx + ofst));
}

static inline bool _used_mask_has(int *m, int idx, int ofst, int num)
{
	WARN_ON(idx < 0 || idx >= num);
	return *m & (1 << (idx + ofst));
}

#define _RANGE_POS_raw			0
#define _RANGE_NUM_raw			8
#define _RANGE_POS_camsv		8
#define _RANGE_NUM_camsv		24
#define _RANGE_POS_mraw			24
#define _RANGE_NUM_mraw			8
#define _RANGE_POS_stream		0
#define _RANGE_NUM_stream		8
#define USED_MASK_HAS(m, type, idx)	\
	_used_mask_has((m), idx, _RANGE_POS_ ## type, _RANGE_NUM_ ## type)
#define USED_MASK_SET(m, type, idx)	\
	_used_mask_set((m), idx, _RANGE_POS_ ## type, _RANGE_NUM_ ## type)
#define USED_MASK_CONTAINS(ma, mb)	(((ma) & (mb)) == mb)

/*
 * struct mtk_cam_request - MTK camera request.
 *
 * @req: Embedded struct media request.
 */
struct mtk_cam_request {
	struct media_request req;
	struct list_head list; /* entry in pending_job_list */
	int used_pipe;

	struct list_head buf_list;
	struct mtk_cam_frame_sync fs;
};

#endif //__MTK_CAM_REQUEST_H
