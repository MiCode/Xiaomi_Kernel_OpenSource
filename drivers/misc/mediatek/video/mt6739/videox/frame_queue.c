/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/* #include <../drivers/staging/android/sw_sync.h> */
#include <linux/slab.h>
#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>
#include "disp_drv_platform.h"
#include "frame_queue.h"
#include "disp_drv_log.h"
#include "mtkfb_fence.h"
#include "mtk_disp_mgr.h"
#include "ged_log.h"


static struct frame_queue_head_t frame_q_head[MAX_SESSION_COUNT];
DEFINE_MUTEX(frame_q_head_lock);
static LIST_HEAD(framequeue_pool_head);
static DEFINE_MUTEX(framequeue_pool_lock);

#ifdef CONFIG_MTK_GPU_SUPPORT
static GED_LOG_BUF_HANDLE ghlog;
atomic_t ged_log_inited = ATOMIC_INIT(0);

#define GEDLOG(fmt, ...)                                                       \
	do {                                                                   \
		if (atomic_read(&ged_log_inited) == 0) {                       \
			if (ged_log_buf_get_early("FENCE", &ghlog) == 0)       \
				atomic_set(&ged_log_inited, 1);                \
			else                                                   \
				break;                                         \
		}                                                              \
		ged_log_buf_print2(ghlog, GED_LOG_ATTR_TIME_TPT, fmt,          \
				   __VA_ARGS__);                               \
	} while (0)

void disp_init_ged_log_handle(void)
{
	int ret;

	ret = ged_log_buf_get_early("FENCE", &ghlog);
	if (ret != 0)
		DISPERR("ged_log_buf_get_early Failed!\n");
}
#else
#define GEDLOG(fmt, ...)

void disp_init_ged_log_handle(void)
{
}
#endif
#ifdef DISP_SYNC_ENABLE
static int disp_dump_fence_info(struct sync_fence *fence, int is_err)
{
	int i;

	_DISP_PRINT_FENCE_OR_ERR(is_err, "fence[%p] %s: stat=%d ==>\n",
		fence, fence->name, atomic_read(&fence->status));

	if (is_err) {
		GEDLOG("fence[%p] %s: stat=%d ==>\n",
			fence, fence->name, atomic_read(&fence->status));
	}

	for (i = 0; i < fence->num_fences; ++i) {
		struct fence *pt = fence->cbs[i].sync_pt;
		char fence_val[10], timeline_val[10];
		const char *timeline_name;
		const char *drv_name;

		timeline_name = pt->ops->get_timeline_name(pt);
		drv_name = pt->ops->get_driver_name(pt);

		pt->ops->fence_value_str(pt, fence_val, sizeof(fence_val));
		pt->ops->timeline_value_str(pt, timeline_val,
					    sizeof(timeline_val));

		_DISP_PRINT_FENCE_OR_ERR(
			is_err, "pt%d:tl=%s,drv=%s,val(%s/%s),sig=%d,stat=%d\n",
			i, timeline_name, drv_name, fence_val, timeline_val,
			fence_is_signaled(pt), pt->status);
		if (is_err) {
			GEDLOG("pt%d:tl=%s,drv=%s,val(%s/%s),sig=%d,stat=%d\n",
			       i, timeline_name, drv_name, fence_val,
			       timeline_val, fence_is_signaled(pt), pt->status);
		}
	}
	return 0;
}

static int _do_wait_fence(struct sync_fence **src_fence, int session_id,
			  int timeline, int fence_fd, int buf_idx,
			  unsigned int present_idx)
{
	int ret;
	struct disp_session_sync_info *session_info;

	session_info = disp_get_session_sync_info_for_debug(session_id);

	if (session_info)
		dprec_start(&session_info->event_wait_fence, timeline,
			    fence_fd);
	DISP_SYSTRACE_BEGIN("wait_fence:fd%d,layer%d,pf%d,idx%d\n",
			    fence_fd, timeline, present_idx, buf_idx);

	ret = sync_fence_wait(*src_fence, 1000);

	DISP_SYSTRACE_END();
	if (session_info)
		dprec_done(&session_info->event_wait_fence, present_idx, ret);

	if (ret == -ETIME) {
		DISPERR("== display fence wait timeout for 1000ms. ret%d,layer%d,fd%d,idx%d ==>\n",
			ret, timeline, fence_fd, buf_idx);
		GEDLOG("== display fence wait timeout for 1000ms. ret%d,layer%d,fd%d,idx%d ==>\n",
		       ret, timeline, fence_fd, buf_idx);
	} else if (ret != 0) {
		DISPERR("== display fence wait status error. ret%d,layer%d,fd%d,idx%d ==>\n",
			ret, timeline, fence_fd, buf_idx);
		GEDLOG("== display fence wait status error. ret%d,layer%d,fd%d,idx%d ==>\n",
		       ret, timeline, fence_fd, buf_idx);
	} else {
		DISPDBG("== display fence wait done! ret%d,layer%d,fd%d,idx%d ==\n",
			ret, timeline, fence_fd, buf_idx);
	}

	if (ret)
		disp_dump_fence_info(*src_fence, 1);

	sync_fence_put(*src_fence);
	*src_fence = NULL;
	return ret;
}
#endif
static int frame_wait_all_fence(struct disp_frame_cfg_t *cfg)
{
#ifdef DISP_SYNC_ENABLE
	int i, ret = 0, tmp;
	int session_id = cfg->session_id;
	unsigned int present_fence_idx = cfg->present_fence_idx;

	/* wait present fence */
	if (cfg->prev_present_fence_struct) {
		tmp = _do_wait_fence(
			(struct sync_fence **)&cfg->prev_present_fence_struct,
			session_id, disp_sync_get_present_timeline_id(),
			cfg->prev_present_fence_fd, present_fence_idx,
			present_fence_idx);

		if (tmp) {
			DISPERR("wait present fence fail!\n");
			ret = -1;
		}
	}

	/* wait input fence */
	for (i = 0; i < cfg->input_layer_num; i++) {
		if (cfg->input_cfg[i].src_fence_struct == NULL)
			continue;

		tmp = _do_wait_fence((struct sync_fence **)&cfg->input_cfg[i]
					     .src_fence_struct,
				     session_id, cfg->input_cfg[i].layer_id,
				     cfg->input_cfg[i].src_fence_fd,
				     cfg->input_cfg[i].next_buff_idx,
				     present_fence_idx);
		if (tmp) {
			dump_input_cfg_info(&cfg->input_cfg[i],
					    cfg->session_id, 1);
			ret = -1;
		}

		disp_sync_buf_cache_sync(session_id, cfg->input_cfg[i].layer_id,
					 cfg->input_cfg[i].next_buff_idx);
	}

	/* wait output fence */
	if (cfg->output_en && cfg->output_cfg.src_fence_struct) {
		tmp = _do_wait_fence(
			(struct sync_fence **)&cfg->output_cfg.src_fence_struct,
			session_id, disp_sync_get_output_timeline_id(),
			cfg->output_cfg.src_fence_fd, cfg->output_cfg.buff_idx,
			present_fence_idx);

		if (tmp) {
			DISPERR("wait output fence fail!\n");
			ret = -1;
		}
		disp_sync_buf_cache_sync(session_id,
					 disp_sync_get_output_timeline_id(),
					 cfg->output_cfg.buff_idx);
	}

	return ret;
#else
	return 0;
#endif
}
static int fence_wait_worker_func(void *data);

static int frame_queue_head_init(struct frame_queue_head_t *head,
				 int session_id)
{
	WARN_ON(head->inited);

	INIT_LIST_HEAD(&head->queue);
	mutex_init(&head->lock);
	head->session_id = session_id;
	init_waitqueue_head(&head->wq);

	/* create fence wait worker thread */
	head->worker = kthread_run(fence_wait_worker_func,
				    head, "disp_queue_%s%d",
				    disp_session_mode_spy(session_id),
				    DISP_SESSION_DEV(session_id));

	if (IS_ERR_OR_NULL(head->worker)) {
		disp_aee_print("create fence thread fail! ret=%ld\n",
			       PTR_ERR(head->worker));
		head->worker = NULL;
		return -ENOMEM;
	}

	head->inited = 1;

	return 0;
}

struct frame_queue_head_t *get_frame_queue_head(int session_id)
{
	int i, ret;
	struct frame_queue_head_t *head = NULL;
	struct frame_queue_head_t *unused_head = NULL;

	mutex_lock(&frame_q_head_lock);

	for (i = 0; i < ARRAY_SIZE(frame_q_head); i++) {
		if (frame_q_head[i].session_id == session_id && frame_q_head[i].inited == 1) {
			head = &frame_q_head[i];
			break;
		}

		if (!frame_q_head[i].inited && !unused_head)
			unused_head = &frame_q_head[i];
	}

	/* find it! */
	if (head)
		goto out;

	/* find a free one */
	if (unused_head) {
		ret = frame_queue_head_init(unused_head, session_id);
		if (ret)
			goto out;

		head = unused_head;
		goto out;
	}

	/* NO free node ??!! */
	disp_aee_print("cannot find frame_q_head!! session_id=0x%x ===>\n",
		       session_id);

	for (i = 0; i < ARRAY_SIZE(frame_q_head); i++)
		DISPERR("0x%x,", frame_q_head[i].session_id);

	DISPERR("\n");

out:
	mutex_unlock(&frame_q_head_lock);

	return head;
}

static int frame_queue_size(struct frame_queue_head_t *head)
{
	int cnt = 0;
	struct list_head *list;

	list_for_each(list, &head->queue)
		cnt++;

	return cnt;
}

struct frame_queue_t *frame_queue_node_create(void)
{
	struct frame_queue_t *framequeue = NULL;

	mutex_lock(&framequeue_pool_lock);
	/* query a node from the pool if possible */
	if (!list_empty(&framequeue_pool_head)) {
		framequeue = list_first_entry(&framequeue_pool_head,
					      struct frame_queue_t, link);
		list_del_init(&framequeue->link);
	}
	mutex_unlock(&framequeue_pool_lock);
	/* create a new one if the pool is empty */
	if (framequeue == NULL) {
		framequeue = kzalloc(sizeof(struct frame_queue_t), GFP_KERNEL);
		if (IS_ERR_OR_NULL(framequeue)) {
			disp_aee_print("fail to kzalloc %zu of frame_queue\n",
				       sizeof(struct frame_queue_t));
			return ERR_PTR(-ENOMEM);
		}
	}
	/* (re)init this node first */
	INIT_LIST_HEAD(&framequeue->link);
	memset(&framequeue->frame_cfg, 0, sizeof(framequeue->frame_cfg));
	framequeue->do_frame_cfg = NULL;

	return framequeue;
}

void frame_queue_node_destroy(struct frame_queue_t *framequeue)
{
	mutex_lock(&framequeue_pool_lock);
	list_add_tail(&framequeue->link, &framequeue_pool_head);
	mutex_unlock(&framequeue_pool_lock);
}

static int fence_wait_worker_func(void *data)
{
	struct frame_queue_head_t *head = data;
	struct frame_queue_t *node;
	struct list_head *list;
	struct disp_frame_cfg_t *frame_cfg;
	struct sched_param param = {.sched_priority = 94};

	sched_setscheduler(current, SCHED_RR, &param);

	while (1) {
		wait_event_interruptible(head->wq, !list_empty(&head->queue));

		mutex_lock(&head->lock);
		if (list_empty(&head->queue)) {
			mutex_unlock(&head->lock);
			goto next;
		}

		list = head->queue.next;
		mutex_unlock(&head->lock);

		node = list_entry(list, struct frame_queue_t, link);
		frame_cfg = &node->frame_cfg;

		frame_wait_all_fence(frame_cfg);

		if (node->do_frame_cfg)
			node->do_frame_cfg(node);

		mutex_lock(&head->lock);
		list_del(list);
		mutex_unlock(&head->lock);
		disp_input_free_dirty_roi(&node->frame_cfg);
		frame_queue_node_destroy(node);

next:
		/* wake up HWC thread, if it's being blocked */
		wake_up(&head->wq);
		if (kthread_should_stop())
			break;
	}
	return 0;
}

int frame_queue_push(struct frame_queue_head_t *head,
		     struct frame_queue_t *node)
{
	int frame_queue_sz;

	mutex_lock(&head->lock);

	frame_queue_sz = frame_queue_size(head);
	if (frame_queue_sz >= 5) {
		/* too many job pending, just block HWC.
		 * So SF/HWC can do some error handling
		 */
		mutex_unlock(&head->lock);
		DISPERR("block HWC because jobs=%d >=5\n", frame_queue_sz);
		wait_event_killable(head->wq, list_empty(&head->queue));
		mutex_lock(&head->lock);
	}

	list_add_tail(&node->link, &head->queue);

	mutex_unlock(&head->lock);
	wake_up(&head->wq);

	return 0;
}

int frame_queue_wait_all_jobs_done(struct frame_queue_head_t *head)
{
	int ret = 0;

	mutex_lock(&head->lock);
	while (!list_empty(&head->queue)) {
		mutex_unlock(&head->lock);
		ret = wait_event_killable(head->wq, list_empty(&head->queue));
		mutex_lock(&head->lock);

		/* wake up by SIG_KILL */
		if (ret == -ERESTARTSYS)
			break;
	}
	mutex_unlock(&head->lock);

	return ret;
}
