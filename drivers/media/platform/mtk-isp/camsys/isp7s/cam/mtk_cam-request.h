/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_REQUEST_H
#define __MTK_CAM_REQUEST_H

#include <linux/list.h>
#include <media/media-request.h>

//#include <mtk_cam-raw_pipeline.h>

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
	WARN(idx < 0 || idx >= num, "idx %d num %d", idx, num);
	*m |= (1 << (idx + ofst));
}

static inline void _used_mask_unset(int *m, int idx, int ofst, int num)
{
	WARN(idx < 0 || idx >= num, "idx %d num %d", idx, num);
	*m &= ~(1 << (idx + ofst));
}

static inline bool _used_mask_has(int *m, int idx, int ofst, int num)
{
	WARN(idx < 0 || idx >= num, "idx %d num %d", idx, num);
	return *m & (1 << (idx + ofst));
}

static inline int _used_mask_get_submask(int *m, int ofst, int num)
{
	return (*m) >> ofst & ((1 << num) - 1);
}

static inline int _submask_has(int *m, int idx, int num)
{
	WARN(idx < 0 || idx >= num, "idx %d num %d", idx, num);
	return *m & (1 << idx);
}

#define _RANGE_POS_raw			0
#define _RANGE_NUM_raw			8
#define _RANGE_POS_camsv		8
#define _RANGE_NUM_camsv		16
#define _RANGE_POS_mraw			24
#define _RANGE_NUM_mraw			8
#define _RANGE_POS_stream		0
#define _RANGE_NUM_stream		8
#define USED_MASK_HAS(m, type, idx)	\
	_used_mask_has((m), idx, _RANGE_POS_ ## type, _RANGE_NUM_ ## type)
#define USED_MASK_SET(m, type, idx)	\
	_used_mask_set((m), idx, _RANGE_POS_ ## type, _RANGE_NUM_ ## type)
#define USED_MASK_UNSET(m, type, idx)	\
	_used_mask_unset((m), idx, _RANGE_POS_ ## type, _RANGE_NUM_ ## type)
#define USED_MASK_GET_SUBMASK(m, type)	\
	_used_mask_get_submask((m), _RANGE_POS_ ## type, _RANGE_NUM_ ## type)
#define SUBMASK_HAS(m, type, idx)		\
	_submask_has((m), idx, _RANGE_NUM_ ## type)

#define USED_MASK_CONTAINS(ma, mb)	\
({					\
	typeof(*(ma)) _a = *(ma);	\
	typeof(*(ma)) _b = *(mb);	\
	(_a & _b) == _b;		\
})

struct v4l2_ctrl_handler;
/*
 * struct mtk_cam_request - MTK camera request.
 *
 * @req: Embedded struct media request.
 */
struct mtk_cam_request {
	struct media_request req;
	struct list_head list; /* entry in pending_job_list */

	int used_ctx;
	int used_pipe;

	spinlock_t buf_lock;
	struct list_head buf_list;

	struct mtk_cam_frame_sync fs;

	struct media_request_object *ctrl_objs[8]; /* TODO: count */
	int ctrl_objs_nr;

	struct mtk_raw_request_data raw_data[3]; /* TODO: count */
};

static inline struct mtk_cam_request *
to_mtk_cam_req(struct media_request *__req)
{
	return container_of(__req, struct mtk_cam_request, req);
}

static inline int mtk_cam_req_used_ctx(struct media_request *req)
{
	struct mtk_cam_request *cam_req = to_mtk_cam_req(req);

	return cam_req->used_ctx;
}

#endif //__MTK_CAM_REQUEST_H
