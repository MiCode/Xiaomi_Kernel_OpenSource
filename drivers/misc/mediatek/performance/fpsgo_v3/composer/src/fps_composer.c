// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/seq_file.h>
#include <linux/security.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/sched/task.h>
#include <linux/sched.h>

#include <mt-plat/fpsgo_common.h>

#include "fpsgo_base.h"
#include "fpsgo_sysfs.h"
#include "fpsgo_usedext.h"
#include "fps_composer.h"
#include "fbt_cpu.h"
#include "fstb.h"
#include "xgf.h"
#include "mini_top.h"
#include "gbe2.h"

/*#define FPSGO_COM_DEBUG*/

#ifdef FPSGO_COM_DEBUG
#define FPSGO_COM_TRACE(...)	xgf_trace("fpsgo_com:" __VA_ARGS__)
#else
#define FPSGO_COM_TRACE(...)
#endif

#define COMP_TAG "FPSGO_COMP"
#define TIME_1MS  1000000
#define MAX_CONNECT_API_SIZE 20

static struct kobject *comp_kobj;
static struct rb_root ui_pid_tree;
static struct rb_root connect_api_tree;
static int bypass_non_SF = 1;
static int fpsgo_control;
static int control_hwui;
/* EGL, CPU, CAMREA */
static int control_api_mask = 22;

#define mtk_composer_dprintk_always(fmt, args...) \
	pr_debug("[FPSGO_COMP]" fmt, ##args)

static inline int fpsgo_com_check_is_surfaceflinger(int pid)
{
	struct task_struct *tsk;
	int is_surfaceflinger = FPSGO_COM_IS_RENDER;

	rcu_read_lock();
	tsk = find_task_by_vpid(pid);

	if (tsk)
		get_task_struct(tsk);
	rcu_read_unlock();

	if (!tsk)
		return FPSGO_COM_TASK_NOT_EXIST;

	if (strstr(tsk->comm, "surfaceflinger"))
		is_surfaceflinger = FPSGO_COM_IS_SF;
	put_task_struct(tsk);

	return is_surfaceflinger;
}

struct connect_api_info *fpsgo_com_search_and_add_connect_api_info(int pid,
	unsigned long long buffer_id, int force)
{
	struct rb_node **p = &connect_api_tree.rb_node;
	struct rb_node *parent = NULL;
	struct connect_api_info *tmp = NULL;
	unsigned long long buffer_key;
	int tgid;

	fpsgo_lockprove(__func__);

	tgid = fpsgo_get_tgid(pid);
	buffer_key = (buffer_id & 0xFFFFFFFFFFFF)
		| (((unsigned long long)tgid) << 48);
	FPSGO_COM_TRACE("%s key:%llu tgid:%d buffer_id:%llu",
		__func__, buffer_key, tgid, buffer_id);
	while (*p) {
		parent = *p;
		tmp = rb_entry(parent, struct connect_api_info, rb_node);

		if (buffer_key < tmp->buffer_key)
			p = &(*p)->rb_left;
		else if (buffer_key > tmp->buffer_key)
			p = &(*p)->rb_right;
		else
			return tmp;
	}

	if (!force)
		return NULL;

	tmp = kzalloc(sizeof(*tmp), GFP_KERNEL);
	if (!tmp)
		return NULL;

	INIT_LIST_HEAD(&(tmp->render_list));

	tmp->pid = pid;
	tmp->tgid = tgid;
	tmp->buffer_id = buffer_id;
	tmp->buffer_key = buffer_key;

	mtk_composer_dprintk_always("Connect API! pid=%d, tgid=%d, buffer_id=%llu",
		pid, tgid, buffer_id);

	rb_link_node(&tmp->rb_node, parent, p);
	rb_insert_color(&tmp->rb_node, &connect_api_tree);

	return tmp;
}

/*
 * UX: sbe_ctrl
 * VP: control_hwui
 * Camera: fpsgo_control
 * Game: fpsgo_control_pid
 */
int fpsgo_com_check_frame_type(int pid, int queue_SF, int api,
	int hwui, int sbe_ctrl, int fpsgo_control_pid)
{

	if (bypass_non_SF && !queue_SF)
		return BY_PASS_TYPE;

	if ((control_api_mask & (1 << api)) == 0)
		return BY_PASS_TYPE;

	if (hwui == RENDER_INFO_HWUI_TYPE) {
		if (control_hwui || sbe_ctrl)
			return NON_VSYNC_ALIGNED_TYPE;
		else
			return BY_PASS_TYPE;
	}

	if (!fpsgo_control && !fpsgo_control_pid)
		return BY_PASS_TYPE;

	return NON_VSYNC_ALIGNED_TYPE;
}

int fpsgo_com_update_render_api_info(struct render_info *f_render)
{
	struct connect_api_info *connect_api;

	fpsgo_lockprove(__func__);
	fpsgo_thread_lockprove(__func__, &(f_render->thr_mlock));

	connect_api =
		fpsgo_com_search_and_add_connect_api_info(f_render->pid,
				f_render->buffer_id, 0);

	if (!connect_api) {
		FPSGO_COM_TRACE("no pair connect api pid[%d] buffer_id[%llu]",
			f_render->pid, f_render->buffer_id);
		return 0;
	}

	f_render->api = connect_api->api;
	list_add(&(f_render->bufferid_list), &(connect_api->render_list));
	FPSGO_COM_TRACE("add connect api pid[%d] key[%llu] buffer_id[%llu]",
		connect_api->pid, connect_api->buffer_key,
		connect_api->buffer_id);
	return 1;
}

static int fpsgo_com_refetch_buffer(struct render_info *f_render, int pid,
		unsigned long long identifier, int enqueue)
{
	int ret;
	unsigned long long buffer_id = 0;
	int queue_SF = 0;

	if (!f_render)
		return 0;

	fpsgo_lockprove(__func__);
	fpsgo_thread_lockprove(__func__, &(f_render->thr_mlock));

	ret = fpsgo_get_BQid_pair(pid, f_render->tgid,
		identifier, &buffer_id, &queue_SF, enqueue);
	if (!ret || !buffer_id) {
		FPSGO_LOGI("refetch %d: %llu, %d, %llu\n",
			pid, buffer_id, queue_SF, identifier);
		fpsgo_main_trace("COMP: refetch %d: %llu, %d, %llu\n",
			pid, buffer_id, queue_SF, identifier);
		return 0;
	}

	f_render->buffer_id = buffer_id;
	f_render->queue_SF = queue_SF;

	if (!f_render->p_blc)
		fpsgo_base2fbt_node_init(f_render);

	FPSGO_COM_TRACE("%s: refetch %d: %llu, %llu, %d\n", __func__,
				pid, identifier, buffer_id, queue_SF);

	return 1;
}

void fpsgo_ctrl2comp_enqueue_start(int pid,
	unsigned long long enqueue_start_time,
	unsigned long long identifier)
{
	struct render_info *f_render;
	int xgf_ret = 0;
	int check_render;
	int ret;

	FPSGO_COM_TRACE("%s pid[%d] id %llu", __func__, pid, identifier);

	check_render = fpsgo_com_check_is_surfaceflinger(pid);

	if (check_render != FPSGO_COM_IS_RENDER)
		return;

	fpsgo_render_tree_lock(__func__);

	f_render = fpsgo_search_and_add_render_info(pid, identifier, 1);

	if (!f_render) {
		fpsgo_render_tree_unlock(__func__);
		FPSGO_COM_TRACE("%s: store frame info fail : %d !!!!\n",
			__func__, pid);
		return;
	}

	fpsgo_thread_lock(&f_render->thr_mlock);

	/* @buffer_id and @queue_SF MUST be initialized
	 * with @api at the same time
	 */
	if (!f_render->api && identifier) {
		ret = fpsgo_com_refetch_buffer(f_render, pid, identifier, 1);
		if (!ret) {
			goto exit;
		}

		ret = fpsgo_com_update_render_api_info(f_render);
		if (!ret) {
			goto exit;
		}
	} else if (identifier) {
		ret = fpsgo_com_refetch_buffer(f_render, pid, identifier, 1);
		if (!ret) {
			goto exit;
		}
	}

	f_render->frame_type = fpsgo_com_check_frame_type(pid,
			f_render->queue_SF, f_render->api,
			f_render->hwui, f_render->sbe_control_flag,
			f_render->control_pid_flag);

	switch (f_render->frame_type) {
	case NON_VSYNC_ALIGNED_TYPE:
		f_render->t_enqueue_start = enqueue_start_time;
		FPSGO_COM_TRACE(
			"pid[%d] type[%d] enqueue_s:%llu",
			pid, f_render->frame_type,
			enqueue_start_time);
		FPSGO_COM_TRACE("update pid[%d] tgid[%d] buffer_id:%llu api:%d",
			f_render->pid, f_render->tgid,
			f_render->buffer_id, f_render->api);
		xgf_ret =
			fpsgo_comp2xgf_qudeq_notify(pid, f_render->buffer_id,
					XGF_QUEUE_START, NULL,
					enqueue_start_time, 0);

		break;
	case BY_PASS_TYPE:
		f_render->t_enqueue_start = enqueue_start_time;
		fpsgo_systrace_c_fbt(pid, f_render->buffer_id,
			f_render->queue_SF, "bypass_sf");
		fpsgo_systrace_c_fbt(pid, f_render->buffer_id,
			f_render->api, "bypass_api");
		fpsgo_systrace_c_fbt(pid, f_render->buffer_id,
			f_render->hwui, "bypass_hwui");
		xgf_ret =
			fpsgo_comp2xgf_qudeq_notify(pid, f_render->buffer_id,
					XGF_QUEUE_START, NULL,
					enqueue_start_time, 1);
		break;
	default:
		FPSGO_COM_TRACE("type not found pid[%d] type[%d]",
			pid, f_render->frame_type);
		break;
	}
exit:
	fpsgo_thread_unlock(&f_render->thr_mlock);
	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_ctrl2comp_enqueue_end(int pid,
	unsigned long long enqueue_end_time,
	unsigned long long identifier)
{
	struct render_info *f_render;
	struct hwui_info *h_info;
	struct sbe_info *s_info;
	struct fps_control_pid_info *f_info;
	int xgf_ret = 0;
	int check_render;
	unsigned long long running_time = 0;
	int ret;

	FPSGO_COM_TRACE("%s pid[%d] id %llu", __func__, pid, identifier);

	check_render = fpsgo_com_check_is_surfaceflinger(pid);

	if (check_render != FPSGO_COM_IS_RENDER)
		return;


	fpsgo_render_tree_lock(__func__);

	f_render = fpsgo_search_and_add_render_info(pid, identifier, 0);

	if (!f_render) {
		fpsgo_render_tree_unlock(__func__);
		FPSGO_COM_TRACE("%s: NON pair frame info : %d !!!!\n",
			__func__, pid);
		return;
	}

	fpsgo_thread_lock(&f_render->thr_mlock);

	ret = fpsgo_com_refetch_buffer(f_render, pid, identifier, 0);
	if (!ret) {
		goto exit;
	}

	/* hwui */
	h_info = fpsgo_search_and_add_hwui_info(f_render->pid, 0);
	if (h_info)
		f_render->hwui = RENDER_INFO_HWUI_TYPE;
	else
		f_render->hwui = RENDER_INFO_HWUI_NONE;

	/* sbe */
	s_info = fpsgo_search_and_add_sbe_info(f_render->pid, 0);
	if (s_info)
		f_render->sbe_control_flag = 1;
	else
		f_render->sbe_control_flag = 0;

	f_info = fpsgo_search_and_add_fps_control_pid(f_render->tgid, 0);
	if (f_info)
		f_render->control_pid_flag = 1;
	else
		f_render->control_pid_flag = 0;

	f_render->frame_type = fpsgo_com_check_frame_type(pid,
			f_render->queue_SF, f_render->api,
			f_render->hwui, f_render->sbe_control_flag,
			f_render->control_pid_flag);

	switch (f_render->frame_type) {
	case NON_VSYNC_ALIGNED_TYPE:
		if (f_render->t_enqueue_end)
			f_render->Q2Q_time =
				enqueue_end_time - f_render->t_enqueue_end;
		f_render->t_enqueue_end = enqueue_end_time;
		f_render->enqueue_length =
			enqueue_end_time - f_render->t_enqueue_start;
		FPSGO_COM_TRACE(
			"pid[%d] type[%d] enqueue_e:%llu enqueue_l:%llu",
			pid, f_render->frame_type,
			enqueue_end_time, f_render->enqueue_length);

		fpsgo_comp2fstb_prepare_calculate_target_fps(pid, f_render->buffer_id,
			enqueue_end_time);

		xgf_ret =
			fpsgo_comp2xgf_qudeq_notify(pid, f_render->buffer_id,
					XGF_QUEUE_END, &running_time,
					enqueue_end_time, 0);
		f_render->enqueue_length_real = f_render->enqueue_length;
		fpsgo_systrace_c_fbt_debug(pid, f_render->buffer_id,
			f_render->enqueue_length_real, "enq_length_real");
		if (running_time != 0)
			f_render->running_time = running_time;

		fpsgo_comp2fbt_frame_start(f_render,
				enqueue_end_time);
		fpsgo_comp2fstb_queue_time_update(pid,
			f_render->buffer_id,
			f_render->frame_type,
			enqueue_end_time,
			f_render->api,
			f_render->hwui);
		fpsgo_comp2minitop_queue_update(enqueue_end_time);
		fpsgo_comp2gbe_frame_update(f_render->pid, f_render->buffer_id);

		fpsgo_systrace_c_fbt_debug(-300, 0, f_render->enqueue_length,
			"%d_%d-enqueue_length", pid, f_render->frame_type);
		break;
	case BY_PASS_TYPE:
		if (f_render->t_enqueue_end)
			f_render->Q2Q_time =
				enqueue_end_time - f_render->t_enqueue_end;
		f_render->t_enqueue_end = enqueue_end_time;
		f_render->enqueue_length =
			enqueue_end_time - f_render->t_enqueue_start;
		fbt_set_render_last_cb(f_render, enqueue_end_time);
		FPSGO_COM_TRACE(
			"pid[%d] type[%d] enqueue_e:%llu enqueue_l:%llu",
			pid, f_render->frame_type,
			enqueue_end_time, f_render->enqueue_length);
		xgf_ret =
			fpsgo_comp2xgf_qudeq_notify(pid, f_render->buffer_id,
					XGF_QUEUE_END, &running_time, enqueue_end_time, 1);
		f_render->enqueue_length_real = f_render->enqueue_length;
		fpsgo_systrace_c_fbt_debug(pid, f_render->buffer_id,
			f_render->enqueue_length_real, "enq_length_real");

		/*
		 * For timer to recycle
		 * TODO: add timer at fps_composer.c
		 */
		fpsgo_comp2fstb_queue_time_update(pid,
			f_render->buffer_id,
			f_render->frame_type,
			enqueue_end_time,
			f_render->api,
			f_render->hwui);
		fpsgo_stop_boost_by_render(f_render);
		break;
	default:
		FPSGO_COM_TRACE("type not found pid[%d] type[%d]",
			pid, f_render->frame_type);
		break;
	}
exit:
	fpsgo_thread_unlock(&f_render->thr_mlock);
	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_ctrl2comp_dequeue_start(int pid,
	unsigned long long dequeue_start_time,
	unsigned long long identifier)
{
	struct render_info *f_render;
	int xgf_ret = 0;
	int check_render;
	int ret;

	FPSGO_COM_TRACE("%s pid[%d] id %llu", __func__, pid, identifier);

	check_render = fpsgo_com_check_is_surfaceflinger(pid);

	if (check_render != FPSGO_COM_IS_RENDER)
		return;

	fpsgo_render_tree_lock(__func__);

	f_render = fpsgo_search_and_add_render_info(pid, identifier, 0);

	if (!f_render) {
		struct BQ_id *pair;

		pair = fpsgo_find_BQ_id(pid, 0, identifier, ACTION_FIND);
		if (pair) {
			FPSGO_COM_TRACE("%s: find pair enqueuer: %d, %d\n",
				__func__, pid, pair->queue_pid);
			pid = pair->queue_pid;
			f_render = fpsgo_search_and_add_render_info(pid,
				identifier, 0);
			if (!f_render) {
				fpsgo_render_tree_unlock(__func__);
				FPSGO_COM_TRACE("%s: NO pair enqueuer: %d\n",
					__func__, pid);
				return;
			}
		} else {
			fpsgo_render_tree_unlock(__func__);
			FPSGO_COM_TRACE("%s: NO pair enqueuer: %d\n",
				__func__, pid);
			return;
		}
	}

	fpsgo_thread_lock(&f_render->thr_mlock);

	ret = fpsgo_com_refetch_buffer(f_render, pid, identifier, 0);
	if (!ret) {
		goto exit;
	}

	f_render->frame_type = fpsgo_com_check_frame_type(pid,
			f_render->queue_SF, f_render->api,
			f_render->hwui, f_render->sbe_control_flag,
			f_render->control_pid_flag);

	switch (f_render->frame_type) {
	case NON_VSYNC_ALIGNED_TYPE:
		f_render->t_dequeue_start = dequeue_start_time;
		FPSGO_COM_TRACE("pid[%d] type[%d] dequeue_s:%llu",
			pid, f_render->frame_type, dequeue_start_time);
		xgf_ret =
			fpsgo_comp2xgf_qudeq_notify(pid, f_render->buffer_id,
					XGF_DEQUEUE_START, NULL,
					dequeue_start_time, 0);
		break;
	case BY_PASS_TYPE:
		f_render->t_dequeue_start = dequeue_start_time;
		xgf_ret =
			fpsgo_comp2xgf_qudeq_notify(pid, f_render->buffer_id,
					XGF_DEQUEUE_START, NULL,
					dequeue_start_time, 1);
		break;
	default:
		FPSGO_COM_TRACE("type not found pid[%d] type[%d]",
			pid, f_render->frame_type);
		break;
	}
exit:
	fpsgo_thread_unlock(&f_render->thr_mlock);
	fpsgo_render_tree_unlock(__func__);

}

void fpsgo_ctrl2comp_dequeue_end(int pid,
	unsigned long long dequeue_end_time,
	unsigned long long identifier)
{
	struct render_info *f_render;
	int xgf_ret = 0;
	int check_render;
	int ret;

	FPSGO_COM_TRACE("%s pid[%d] id %llu", __func__, pid, identifier);

	check_render = fpsgo_com_check_is_surfaceflinger(pid);

	if (check_render != FPSGO_COM_IS_RENDER)
		return;

	fpsgo_render_tree_lock(__func__);

	f_render = fpsgo_search_and_add_render_info(pid, identifier, 0);

	if (!f_render) {
		struct BQ_id *pair;

		pair = fpsgo_find_BQ_id(pid, 0, identifier, ACTION_FIND);
		if (pair) {
			pid = pair->queue_pid;
			f_render = fpsgo_search_and_add_render_info(pid,
				identifier, 0);
		}

		if (!f_render) {
			fpsgo_render_tree_unlock(__func__);
			FPSGO_COM_TRACE("%s: NO pair enqueuer: %d\n",
				__func__, pid);
			return;
		}
	}

	fpsgo_thread_lock(&f_render->thr_mlock);

	ret = fpsgo_com_refetch_buffer(f_render, pid, identifier, 0);
	if (!ret) {
		goto exit;
	}

	f_render->frame_type = fpsgo_com_check_frame_type(pid,
			f_render->queue_SF, f_render->api,
			f_render->hwui, f_render->sbe_control_flag,
			f_render->control_pid_flag);

	switch (f_render->frame_type) {
	case NON_VSYNC_ALIGNED_TYPE:
		f_render->t_dequeue_end = dequeue_end_time;
		f_render->dequeue_length =
			dequeue_end_time - f_render->t_dequeue_start;
		FPSGO_COM_TRACE(
			"pid[%d] type[%d] dequeue_e:%llu dequeue_l:%llu",
			pid, f_render->frame_type,
			dequeue_end_time, f_render->dequeue_length);
		xgf_ret =
			fpsgo_comp2xgf_qudeq_notify(pid, f_render->buffer_id,
				XGF_DEQUEUE_END, NULL, dequeue_end_time, 0);
		fpsgo_comp2fbt_deq_end(f_render, dequeue_end_time);
		fpsgo_systrace_c_fbt_debug(-300, 0, f_render->dequeue_length,
			"%d_%d-dequeue_length", pid, f_render->frame_type);
		break;
	case BY_PASS_TYPE:
		f_render->t_dequeue_end = dequeue_end_time;
		f_render->dequeue_length =
			dequeue_end_time - f_render->t_dequeue_start;
		xgf_ret =
			fpsgo_comp2xgf_qudeq_notify(pid, f_render->buffer_id,
				XGF_DEQUEUE_END, NULL, dequeue_end_time, 1);
		break;
	default:
		FPSGO_COM_TRACE("type not found pid[%d] type[%d]",
			pid, f_render->frame_type);
		break;
	}
exit:
	fpsgo_thread_unlock(&f_render->thr_mlock);
	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_ctrl2comp_connect_api(int pid, int api,
		unsigned long long identifier)
{
	struct connect_api_info *connect_api;
	int check_render;
	unsigned long long buffer_id = 0;
	int queue_SF = 0;
	int ret;

	check_render = fpsgo_com_check_is_surfaceflinger(pid);

	if (check_render != FPSGO_COM_IS_RENDER)
		return;

	FPSGO_COM_TRACE("%s pid[%d], identifier:%llu",
		__func__, pid, identifier);

	fpsgo_render_tree_lock(__func__);

	ret = fpsgo_get_BQid_pair(pid, 0, identifier, &buffer_id, &queue_SF, 0);
	if (!ret || !buffer_id) {
		FPSGO_LOGI("connect %d: %llu, %llu\n",
				pid, buffer_id, identifier);
		fpsgo_main_trace("COMP: connect %d: %llu, %llu\n",
				pid, buffer_id, identifier);
		fpsgo_render_tree_unlock(__func__);
		return;
	}

	connect_api =
		fpsgo_com_search_and_add_connect_api_info(pid, buffer_id, 1);
	if (!connect_api) {
		fpsgo_render_tree_unlock(__func__);
		FPSGO_COM_TRACE("%s: store frame info fail : %d !!!!\n",
			__func__, pid);
		return;
	}

	connect_api->api = api;
	fpsgo_render_tree_unlock(__func__);

}

void fpsgo_ctrl2comp_bqid(int pid, unsigned long long buffer_id,
	int queue_SF, unsigned long long identifier, int create)
{
	struct BQ_id *pair;

	if (!identifier || !pid)
		return;

	if (!buffer_id && create)
		return;

	fpsgo_render_tree_lock(__func__);

	FPSGO_LOGI("pid %d: bufid %llu, id %llu, queue_SF %d, create %d\n",
		pid, buffer_id, identifier, queue_SF, create);

	if (create) {
		pair = fpsgo_find_BQ_id(pid, 0,
				identifier, ACTION_FIND_ADD);

		if (!pair) {
			fpsgo_render_tree_unlock(__func__);
			FPSGO_COM_TRACE("%s: add fail : %d !!!!\n",
				__func__, pid);
			return;
		}

		if (pair->pid != pid)
			FPSGO_LOGI("%d: diff render same key %d\n",
				pid, pair->pid);

		pair->buffer_id = buffer_id;
		pair->queue_SF = queue_SF;
	} else
		fpsgo_find_BQ_id(pid, 0,
			identifier, ACTION_FIND_DEL);

	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_com_clear_connect_api_render_list(
	struct connect_api_info *connect_api)
{
	struct render_info *pos, *next;

	fpsgo_lockprove(__func__);

	list_for_each_entry_safe(pos, next,
		&connect_api->render_list, bufferid_list) {
		fpsgo_delete_render_info(pos->pid,
			pos->buffer_id, pos->identifier);
	}

}

void fpsgo_ctrl2comp_disconnect_api(
	int pid, int api, unsigned long long identifier)
{
	struct connect_api_info *connect_api;
	int check_render;
	unsigned long long buffer_id = 0;
	int queue_SF = 0;
	int ret;

	check_render = fpsgo_com_check_is_surfaceflinger(pid);

	if (check_render != FPSGO_COM_IS_RENDER)
		return;


	FPSGO_COM_TRACE("%s pid[%d], identifier:%llu",
		__func__, pid, identifier);

	fpsgo_render_tree_lock(__func__);

	ret = fpsgo_get_BQid_pair(pid, 0, identifier, &buffer_id, &queue_SF, 0);
	if (!ret || !buffer_id) {
		mtk_composer_dprintk_always("[Disconnect] NoBQid %d: %llu, %llu\n",
				pid, buffer_id, identifier);
		fpsgo_main_trace("COMP: disconnect %d: %llu, %llu\n",
				pid, buffer_id, identifier);
		fpsgo_render_tree_unlock(__func__);
		return;
	}

	connect_api =
		fpsgo_com_search_and_add_connect_api_info(pid, buffer_id, 0);
	if (!connect_api) {
		mtk_composer_dprintk_always("[Disconnect] NoConnectApi %d: %llu, %llu\n",
			pid, buffer_id, identifier);
		FPSGO_COM_TRACE(
			"%s: FPSGo composer distory connect api fail : %d !!!!\n",
			__func__, pid);
		fpsgo_render_tree_unlock(__func__);
		return;
	}
	mtk_composer_dprintk_always("[Disconnect] Success %d: %llu, %llu\n",
		pid, buffer_id, identifier);
	fpsgo_com_clear_connect_api_render_list(connect_api);
	rb_erase(&connect_api->rb_node, &connect_api_tree);
	kfree(connect_api);


	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_fstb2comp_check_connect_api(void)
{
	struct rb_node *n;
	struct connect_api_info *iter;
	struct task_struct *tsk;
	int size = 0;

	FPSGO_COM_TRACE("%s ", __func__);

	fpsgo_render_tree_lock(__func__);

	n = rb_first(&connect_api_tree);

	while (n) {
		iter = rb_entry(n, struct connect_api_info, rb_node);
		rcu_read_lock();
		tsk = find_task_by_vpid(iter->tgid);
		rcu_read_unlock();
		if (!tsk) {
			fpsgo_com_clear_connect_api_render_list(iter);
			rb_erase(&iter->rb_node, &connect_api_tree);
			n = rb_first(&connect_api_tree);
			kfree(iter);
			size = 0;
		} else {
			n = rb_next(n);
			size++;
		}
	}

	if (size >= MAX_CONNECT_API_SIZE)
		mtk_composer_dprintk_always("connect api tree size: %d", size);

	fpsgo_render_tree_unlock(__func__);

}

static int switch_ui_ctrl(int pid, int set_ctrl)
{
	fpsgo_render_tree_lock(__func__);
	if (set_ctrl)
		fpsgo_search_and_add_sbe_info(pid, 1);
	else {
		fpsgo_delete_sbe_info(pid);

		fpsgo_stop_boost_by_pid(pid);
	}
	fpsgo_render_tree_unlock(__func__);

	fpsgo_systrace_c_fbt(pid, 0,
			set_ctrl, "sbe_set_ctrl");
	fpsgo_systrace_c_fbt(pid, 0,
			0, "sbe_state");

	return 0;
}

static int switch_fpsgo_control_pid(int pid, int set_ctrl)
{
	fpsgo_render_tree_lock(__func__);
	if (set_ctrl)
		fpsgo_search_and_add_fps_control_pid(pid, 1);
	else
		fpsgo_delete_fpsgo_control_pid(pid);

	fpsgo_render_tree_unlock(__func__);

	fpsgo_systrace_c_fbt(pid, 0,
			set_ctrl, "fpsgo_control_pid");

	return 0;
}

static ssize_t connect_api_info_show
	(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct rb_node *n;
	struct connect_api_info *iter;
	struct task_struct *tsk;
	struct render_info *pos, *next;
	char *temp = NULL;
	int posi = 0;
	int length = 0;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	length = scnprintf(temp + posi, FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
			"=================================\n");
	posi += length;

	fpsgo_render_tree_lock(__func__);
	rcu_read_lock();

	for (n = rb_first(&connect_api_tree); n != NULL; n = rb_next(n)) {
		iter = rb_entry(n, struct connect_api_info, rb_node);
		tsk = find_task_by_vpid(iter->tgid);
		if (tsk) {
			get_task_struct(tsk);
			length = scnprintf(temp + posi,
				FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
				"PID  TGID  NAME    BufferID    API    Key\n");
			posi += length;
			length = scnprintf(temp + posi,
				FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
				"%5d %5d %5s %4llu %5d %4llu\n",
				iter->pid, iter->tgid, tsk->comm,
				iter->buffer_id, iter->api, iter->buffer_key);
			posi += length;
			put_task_struct(tsk);
		}

		length = scnprintf(temp + posi,
			FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
			"******render list******\n");
		posi += length;

		list_for_each_entry_safe(pos, next,
				&iter->render_list, bufferid_list) {
			fpsgo_thread_lock(&pos->thr_mlock);

			length = scnprintf(temp + posi,
					FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
					"  PID  TGID	 BufferID	API    TYPE\n");
			posi += length;
			length = scnprintf(temp + posi,
					FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
					"%5d %5d %4llu %5d %5d\n",
					pos->pid, pos->tgid, pos->buffer_id,
					pos->api, pos->frame_type);
			posi += length;

			fpsgo_thread_unlock(&pos->thr_mlock);

		}

		length = scnprintf(temp + posi,
				FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
				"***********************\n");
		posi += length;
		length = scnprintf(temp + posi,
				FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
				"=================================\n");
		posi += length;
	}

	rcu_read_unlock();
	fpsgo_render_tree_unlock(__func__);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(connect_api_info);

static ssize_t control_hwui_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", control_hwui);
}

static ssize_t control_hwui_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				control_hwui = !!arg;
		}
	}

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(control_hwui);

static ssize_t control_api_mask_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", control_api_mask);
}

static ssize_t control_api_mask_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				control_api_mask = arg;
		}
	}

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(control_api_mask);

static ssize_t bypass_non_SF_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", bypass_non_SF);
}

static ssize_t bypass_non_SF_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				bypass_non_SF = !!arg;
		}
	}

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(bypass_non_SF);

static ssize_t set_ui_ctrl_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return 0;
}

static ssize_t set_ui_ctrl_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int arg;
	int ret = 0;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) != 0)
				goto out;
			fpsgo_systrace_c_fbt(arg > 0 ? arg : -arg,
				0, arg > 0, "force_ctrl");
			if (arg > 0)
				ret = switch_ui_ctrl(arg, 1);
			else
				ret = switch_ui_ctrl(-arg, 0);
		}
	}

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(set_ui_ctrl);

static ssize_t fpsgo_control_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", fpsgo_control);
}

static ssize_t  fpsgo_control_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				fpsgo_control = !!arg;
		}
	}

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(fpsgo_control);

static ssize_t fpsgo_control_pid_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return 0;
}

static ssize_t  fpsgo_control_pid_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int pid = 0, value = 0;
	int ret = 0;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (sscanf(acBuffer, "%d %d", &pid, &value) == 2)
				ret = switch_fpsgo_control_pid(pid, !!value);
		}
	}

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(fpsgo_control_pid);

void __exit fpsgo_composer_exit(void)
{
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_connect_api_info);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_control_hwui);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_control_api_mask);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_bypass_non_SF);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_set_ui_ctrl);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_fpsgo_control);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_fpsgo_control_pid);

	fpsgo_sysfs_remove_dir(&comp_kobj);
}

int __init fpsgo_composer_init(void)
{
	ui_pid_tree = RB_ROOT;
	connect_api_tree = RB_ROOT;

	if (!fpsgo_sysfs_create_dir(NULL, "composer", &comp_kobj)) {
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_connect_api_info);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_control_hwui);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_control_api_mask);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_bypass_non_SF);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_set_ui_ctrl);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_fpsgo_control);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_fpsgo_control_pid);
	}

	return 0;
}

