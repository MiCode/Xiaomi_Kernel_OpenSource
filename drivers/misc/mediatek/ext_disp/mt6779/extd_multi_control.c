// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kthread.h>
/* #include <linux/rtpm_prio.h> */
#include <uapi/linux/sched/types.h>

#include "extd_multi_control.h"
#include "disp_drv_platform.h"
#include "external_display.h"
#include "extd_platform.h"
#include "extd_log.h"
#include "mtk_ovl.h"
#include "display_recorder.h"
#include "mtkfb_fence.h"
#include "disp_drv_log.h"
#ifdef EXTD_DUAL_PIPE_SWITCH_SUPPORT
#  include "layering_rule.h"
#endif

static const struct EXTD_DRIVER *extd_driver[DEV_MAX_NUM];
static struct SWITCH_MODE_INFO_STRUCT path_info;

struct task_struct *disp_switch_mode_task;
wait_queue_head_t switch_mode_wq;
atomic_t switch_mode_event = ATOMIC_INIT(0);

static int _get_dev_index(unsigned int session)
{
	int dev_id = session & 0x0FF;

	if (dev_id >= DEV_MAX_NUM) {
		MULTI_COTRL_LOG("%s device id error:%d\n", __func__, dev_id);
		return -1;
	}

	if (dev_id < DEV_LCM)
		dev_id -= 1;

	return dev_id;
}

static int _extd_create_path(enum EXT_DISP_PATH_MODE mode, unsigned int session)
{
	int ret = 0;

	MULTI_COTRL_LOG("%s session:0x%08x, mode:%d\n",
			__func__, session, mode);

	ext_disp_path_set_mode(mode, session);
	ret = ext_disp_init(NULL, session);

	return ret;
}

static int _extd_get_device_type(unsigned int session)
{
	int ret = -1;
	int dev_idx;

	dev_idx = _get_dev_index(session);
	if (dev_idx >= 0 && extd_driver[dev_idx] && extd_driver[dev_idx]->ioctl)
		ret = extd_driver[dev_idx]->ioctl(GET_DEV_TYPE_CMD, 0, 0, NULL);

	MULTI_COTRL_LOG("device type is:%d\n", ret);
	return ret;
}

static void _extd_set_layer_num(int layer_num, unsigned int session)
{
	int dev_idx;

	dev_idx = _get_dev_index(session);
	if (dev_idx >= 0 && extd_driver[dev_idx] && extd_driver[dev_idx]->ioctl)
		extd_driver[dev_idx]->ioctl(SET_LAYER_NUM_CMD, layer_num,
					    0, NULL);
}

/**
 * __setup_mem_session - create memory session or switch mode
 */
int __setup_mem_session(unsigned int session, int to_mode)
{
	int ret = 0;

	if ((path_info.old_session[DEV_WFD] != DISP_SESSION_MEMORY ||
	     path_info.old_mode[DEV_WFD] >=
	     DISP_SESSION_DIRECT_LINK_MIRROR_MODE) &&
	    to_mode < DISP_SESSION_DIRECT_LINK_MIRROR_MODE) {
		/* switch to memory session, extension mode, extension path */
		int has_physical_disp = 0;

		if (path_info.old_session[DEV_LCM] == DISP_SESSION_EXTERNAL)
			has_physical_disp = 1;

		if (has_physical_disp == 0 &&
		    ext_disp_wait_ovl_available(0) > 0) {
			ovl2mem_init(session);
			ovl2mem_setlayernum(MEMORY_SESSION_INPUT_LAYER_COUNT);
		} else {
			MULTI_COTRL_ERR("mhl path:OVL1 cannot be split out!\n");
			ret = -1;
		}
	} else if (path_info.old_session[DEV_WFD] == DISP_SESSION_MEMORY &&
		   path_info.old_mode[DEV_WFD] <
		   DISP_SESSION_DIRECT_LINK_MIRROR_MODE &&
		   to_mode >= DISP_SESSION_DIRECT_LINK_MIRROR_MODE) {
		/*
		 * stay in the same memory session,
		 * but switch to mirror mode, mirror path
		 */
		ovl2mem_deinit();
		ovl2mem_setlayernum(0);
		ext_disp_path_change(EXTD_OVL_IDLE_REQ, session);
#ifdef EXTD_DUAL_PIPE_SWITCH_SUPPORT
		/* Notify primary display can switch to dual pipe */
		set_hrt_state(DISP_HRT_FORCE_DUAL_OFF, 0);
#endif
	}

	return ret;
}

int __setup_ext_session(unsigned int session, int mode)
{
	int ret = 0;
	int if_type = DISP_IF_MHL;
	int dev = 0;
	int has_virtual_disp = 0;

	dev = DISP_SESSION_DEV(session);
	if (dev != DEV_LCM) {
		/* mhl/eink device */
		dev -= 1;
	} else if (path_info.old_session[DEV_WFD] == DISP_SESSION_MEMORY) {
		/* has virtual display, 3 displays at the same time */
		has_virtual_disp = 1;
	}

	if (path_info.old_session[dev] == DISP_SESSION_EXTERNAL) {
		if (EXTD_OVERLAY_CNT < 1) {
			/* external display has no OVL to use */
			return ret;
		}
	}

	if_type = _extd_get_device_type(session);
	if (if_type != DISP_IF_EPD &&
	    (mode < DISP_SESSION_DIRECT_LINK_MIRROR_MODE ||
	     if_type == DISP_IF_HDMI_SMARTBOOK)) {
		if (ext_disp_wait_ovl_available(0) > 0 &&
		    has_virtual_disp == 0) {
			if (path_info.old_session[dev] ==
			    DISP_SESSION_EXTERNAL &&
			    if_type != DISP_IF_HDMI_SMARTBOOK) {
				/* insert OVL to external display path */
				ext_disp_path_change(EXTD_OVL_INSERT_REQ,
						     session);
			} else {
				_extd_create_path(EXTD_DIRECT_LINK_MODE,
						  session);
			}

			_extd_set_layer_num(EXTERNAL_SESSION_INPUT_LAYER_COUNT,
					    session);
		} else {
			MULTI_COTRL_ERR("mhl path:OVL1 cannot be split out!\n");
			if (path_info.old_session[dev] !=
			    DISP_SESSION_EXTERNAL) {
				/* insert OVL to external display path */
				_extd_create_path(EXTD_RDMA_DPI_MODE, session);
				_extd_set_layer_num(1, session);
			}
		}
	} else { /* DISP_IF_EPD */
		if (path_info.old_session[dev] == DISP_SESSION_EXTERNAL) {
			ext_disp_path_change(EXTD_OVL_REMOVE_REQ, session);
			_extd_set_layer_num(1, session);
		} else {
			_extd_create_path(EXTD_RDMA_DPI_MODE, session);
			_extd_set_layer_num(1, session);
		}
	}

	return ret;
}

/**
 * return: 0 on success
 */
static int __create_external_display_path(unsigned int session, int mode)
{
	int ret = 0;

	MULTI_COTRL_LOG("%s session:0x%08x, mode:%d\n",
			__func__, session, mode);

	if (DISP_SESSION_TYPE(session) == DISP_SESSION_MEMORY &&
	    EXTD_OVERLAY_CNT > 0) {
		ret = __setup_mem_session(session, mode);
	} else if (DISP_SESSION_TYPE(session) == DISP_SESSION_EXTERNAL) {
		ret = __setup_ext_session(session, mode);
	} else if (DISP_SESSION_TYPE(session) == DISP_SESSION_MEMORY &&
		   EXTD_OVERLAY_CNT == 0) {
		MULTI_COTRL_ERR("memory session and ovl time sharing!\n");
		ovl2mem_setlayernum(MEMORY_SESSION_INPUT_LAYER_COUNT);
	}

	return ret;
}

static void __destroy_external_display_path(unsigned int session, int mode)
{
	int dev = 0;

	MULTI_COTRL_LOG("%s session:0x%08x\n", __func__, session);

	dev = DISP_SESSION_DEV(session);
	if ((DISP_SESSION_TYPE(session) == DISP_SESSION_EXTERNAL) &&
	    (dev != DEV_LCM)) {
		/* mhl/eink device id */
		dev -= 1;
	}

	if ((path_info.old_session[dev] == DISP_SESSION_PRIMARY) ||
	    (path_info.old_session[dev] == DISP_SESSION_MEMORY &&
	     path_info.old_mode[dev] >= DISP_SESSION_DIRECT_LINK_MIRROR_MODE)) {
		/* discard for memory session in mirror mode */
		MULTI_COTRL_LOG("no need destroy path: sess:0x%08x, mode:%d\n",
				path_info.old_session[dev],
				path_info.old_mode[dev]);
		return;
	}

	if (path_info.old_session[dev] == DISP_SESSION_EXTERNAL) {
		ext_disp_deinit(session);
		_extd_set_layer_num(0, session);
		ext_disp_path_change(EXTD_OVL_IDLE_REQ, session);
#ifdef EXTD_DUAL_PIPE_SWITCH_SUPPORT
		/* Notify primary display can switch to dual pipe */
		set_hrt_state(DISP_HRT_FORCE_DUAL_OFF, 0);
#endif
	} else if (path_info.old_session[dev] == DISP_SESSION_MEMORY &&
		   EXTD_OVERLAY_CNT > 0) {
		ovl2mem_deinit();
		ovl2mem_setlayernum(0);
		ext_disp_path_change(EXTD_OVL_IDLE_REQ, session);
#ifdef EXTD_DUAL_PIPE_SWITCH_SUPPORT
		/* Notify primary display can switch to dual pipe */
		set_hrt_state(DISP_HRT_FORCE_DUAL_OFF, 0);
#endif
	}
}

static int disp_switch_mode_kthread(void *data)
{
	int ret = 0;
	struct sched_param param = {.sched_priority = 94 };
	const int len = 100;
	char msg[len];
	int n = 0;

	sched_setscheduler(current, SCHED_RR, &param);

	MULTI_COTRL_LOG("%s in!\n", __func__);

	for (;;) {
		wait_event_interruptible(switch_mode_wq,
					 atomic_read(&switch_mode_event));
		atomic_set(&switch_mode_event, 0);

		n = snprintf(msg, len, "switch mode, create or change path, ");
		n += snprintf(msg + n, len - n, "mode:%d, sess:0x%08x\n",
			      path_info.cur_mode, path_info.ext_sid);
		MULTI_COTRL_LOG("%s", msg);

		ret = __create_external_display_path(path_info.ext_sid,
						     path_info.cur_mode);
		if ((ret == 0) && (path_info.switching < DEV_MAX_NUM)) {
			path_info.old_session[path_info.switching] =
					DISP_SESSION_TYPE(path_info.ext_sid);
			path_info.old_mode[path_info.switching] =
							path_info.cur_mode;
		}

		path_info.switching = DEV_MAX_NUM;
		path_info.ext_sid = 0;

		if (kthread_should_stop()) {
			/* thread exit */
			break;
		}
	}

	return 0;
}

/**
 * __get_session_by_dev - query session by device id
 *
 * return: session id
 */
static unsigned int __get_session_by_dev(unsigned int dev)
{
	unsigned int session = 0;

	switch (dev) {
	case DEV_WFD:
		session = MAKE_DISP_SESSION(DISP_SESSION_MEMORY, dev);
		break;
	case DEV_LCM:
		session = MAKE_DISP_SESSION(DISP_SESSION_EXTERNAL, dev);
		break;
	default:
		session = MAKE_DISP_SESSION(DISP_SESSION_EXTERNAL, dev + 1);
		break;
	}

	return session;
}

#ifndef OVL_CASCADE_SUPPORT
/**
 * @session_id: if 0, the @dev is about to be destroyed
 */
static int path_change_without_cascade(enum DISP_MODE mode,
				       unsigned int to_session,
				       unsigned int dev)
{
	int ret = -1;

	/* MULTI_COTRL_FUNC(); */

	/* destroy external display path */
	if (path_info.old_session[dev] != DISP_SESSION_PRIMARY &&
	    to_session == 0) {
		unsigned int session = __get_session_by_dev(dev);

		__destroy_external_display_path(session,
						DISP_SESSION_DIRECT_LINK_MODE);

		/* reset */
		path_info.old_session[dev] = DISP_SESSION_PRIMARY;
		path_info.old_mode[dev] = DISP_SESSION_DIRECT_LINK_MODE;
		path_info.switching = DEV_MAX_NUM;

		return 1;
	}

	/* create path or change path */
	if ((path_info.old_session[dev] == DISP_SESSION_PRIMARY &&
	     to_session > 0)/* create path */ ||
	    (path_info.old_session[dev] != DISP_SESSION_PRIMARY &&
	     mode != path_info.old_mode[dev])/* switch mode */) {
		ret = __create_external_display_path(to_session, mode);
		if (!ret) {
			path_info.old_session[dev] = DISP_SESSION_TYPE(
								to_session);
			path_info.old_mode[dev] = mode;
		}

		path_info.switching = DEV_MAX_NUM;

		return 1;
	}

	return 0;
}
#else
static int path_change_with_cascade(enum DISP_MODE mode,
				    unsigned int session_id,
				    unsigned int device_id)
{
	int disp_type = 0;
	unsigned int session = 0;

	/* MULTI_COTRL_FUNC(); */

	/* destroy external display path */
	if (session_id == 0
	    && path_info.old_session[device_id] != DISP_SESSION_PRIMARY) {
		session = __get_session_by_dev(dev);

		__destroy_external_display_path(session,
						DISP_SESSION_DIRECT_LINK_MODE);
		path_info.old_session[device_id] = DISP_SESSION_PRIMARY;
		path_info.old_mode[device_id] = DISP_SESSION_DIRECT_LINK_MODE;
		path_info.switching = DEV_MAX_NUM;
		path_info.ext_sid = 0;

		return 1;
	}

	/* create path or change path */
	if ((session_id > 0 &&
	     path_info.old_session[device_id] == DISP_SESSION_PRIMARY) ||
	    (mode != path_info.old_mode[device_id] &&
	     path_info.old_session[device_id] != DISP_SESSION_PRIMARY)) {
		/* the case will use OVL */
		disp_type = _extd_get_device_type(session_id);
		if (disp_type != DISP_IF_EPD &&
		    (mode < DISP_SESSION_DIRECT_LINK_MIRROR_MODE ||
		     disp_type == DISP_IF_HDMI_SMARTBOOK)) {
			/* request OVL */
			ext_disp_path_change(EXTD_OVL_REQUSTING_REQ,
					     session_id);
		}

		if (path_info.old_session[device_id] == DISP_SESSION_EXTERNAL &&
		    mode >= DISP_SESSION_DIRECT_LINK_MIRROR_MODE) {
			/* it is to say that the path is RDMA-DPI */
			_extd_set_layer_num(1, session_id);
		}

		MULTI_COTRL_LOG("%s, wake up\n", __func__);
		path_info.cur_mode = mode;
		path_info.ext_sid = session_id;
		path_info.switching = device_id;
		atomic_set(&switch_mode_event, 1);
		wake_up_interruptible(&switch_mode_wq);

		return 1;
	}

	return 0;
}
#endif

void external_display_control_init(void)
{
	int i = 0;

	MULTI_COTRL_FUNC();
	memset(&path_info, 0, sizeof(path_info));
	path_info.switching = DEV_MAX_NUM;

	for (i = 0; i < DEV_MAX_NUM; i++) {
		path_info.old_mode[i] = DISP_SESSION_DIRECT_LINK_MODE;
		path_info.old_session[i] = DISP_SESSION_PRIMARY;
	}

	extd_driver[DEV_MHL] = EXTD_HDMI_Driver();
	extd_driver[DEV_EINK] = EXTD_EPD_Driver();
	extd_driver[DEV_WFD] = NULL;
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
		(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
	extd_driver[DEV_LCM] = EXTD_LCM_Driver();
#endif

	init_waitqueue_head(&switch_mode_wq);
	disp_switch_mode_task = kthread_create(disp_switch_mode_kthread, NULL,
					       "disp_switch_mode_kthread");
	wake_up_process(disp_switch_mode_task);

	ext_disp_probe();
}

int external_display_trigger(enum EXTD_TRIGGER_MODE trigger,
			     unsigned int session)
{
	int ret = 0;
	enum EXTD_OVL_REQ_STATUS ovl_status = EXTD_OVL_NO_REQ;

	if (trigger == TRIGGER_RESUME) {
		ext_disp_resume(session);
		if (session == __get_session_by_dev(DEV_EINK)) {
			if (extd_driver[DEV_EINK] &&
			    extd_driver[DEV_EINK]->power_enable)
				extd_driver[DEV_EINK]->power_enable(1);
		}
	}

	ret = ext_disp_trigger(0, ext_fence_release_callback, 0, session);

	if (trigger == TRIGGER_SUSPEND) {
		ext_disp_suspend_trigger(NULL, 0, session);
		if (session == __get_session_by_dev(DEV_EINK)) {
			if (extd_driver[DEV_EINK] &&
			    extd_driver[DEV_EINK]->power_enable)
				extd_driver[DEV_EINK]->power_enable(0);
		}
	}

	ovl_status = ext_disp_get_ovl_req_status(session);
	if (ovl_status == EXTD_OVL_REMOVING)
		ext_disp_path_change(EXTD_OVL_REMOVED, session);
	else if (ovl_status == EXTD_OVL_INSERTING)
		ext_disp_path_change(EXTD_OVL_INSERTED, session);

	return ret;
}

int external_display_suspend(unsigned int session)
{
	int i = 0;
	int ret = 0;
	unsigned int session_id = 0;

	if (session) {
		ret = ext_disp_suspend(session);
		return ret;
	}

	for (i = DEV_MHL; i < DEV_MAX_NUM; i++) {
		session_id = __get_session_by_dev(i);
		ext_disp_suspend(session_id);
	}

	return ret;
}

int external_display_resume(unsigned int session)
{
	int i = 0;
	int ret = 0;
	unsigned int session_id = 0;

	if (session) {
		ret = ext_disp_resume(session);
		return ret;
	}

	for (i = DEV_MHL; i < DEV_MAX_NUM; i++) {
		session_id = __get_session_by_dev(i);
		ext_disp_resume(session_id);
	}

	return ret;
}

int external_display_wait_for_vsync(void *config, unsigned int session)
{
	int ret = 0;

	ret = ext_disp_wait_for_vsync(config, session);

	return ret;
}

int external_display_get_info(void *info, unsigned int session)
{
	int ret = -1;
	int dev_index;

	dev_index = _get_dev_index(session);
	if (dev_index >= 0 && extd_driver[dev_index] &&
	    extd_driver[dev_index]->get_dev_info)
		ret = extd_driver[dev_index]->get_dev_info(SF_GET_INFO, info);

	return ret;
}

int external_display_switch_mode(enum DISP_MODE mode,
				 unsigned int *session_created,
				 unsigned int session)
{
	int i = 0;
	int ret = -1;
	int switching = 0;
	int session_id[DEV_MAX_NUM] = { 0 };

	if (session_created == NULL)
		return ret;
	if (path_info.switching < DEV_MAX_NUM)
		return ret; /* mode is switching, return directly */

	path_info.switching = DEV_MAX_NUM - 1;

	for (i = 0; i < MAX_SESSION_COUNT; i++) {
		unsigned int dev = 0;

		/* sort session_created by device_id */
		for (dev = 0; dev < DEV_MAX_NUM; dev++) {
			if (session_created[i] == __get_session_by_dev(dev))
				session_id[dev] = session_created[i];
		}
	}

	for (i = 0; i < DEV_MAX_NUM; i++) {
#ifndef OVL_CASCADE_SUPPORT
		switching = path_change_without_cascade(mode, session_id[i], i);
#else
		switching = path_change_with_cascade(mode, session_id[i], i);
#endif

		if (switching == 1)
			break; /* session switching one by one */
	}

	path_info.switching = !switching ? DEV_MAX_NUM : path_info.switching;

	return 0;
}

int external_display_frame_cfg(struct disp_frame_cfg_t *cfg)
{
	int ret = 0;
	unsigned int i;
	unsigned int session_id = 0;
	struct dprec_logger_event *input_event, *trigger_event;
	struct disp_session_sync_info *s_info = NULL;

	s_info = disp_get_session_sync_info_for_debug(cfg->session_id);
	session_id = cfg->session_id;

	if (s_info) {
		input_event = &s_info->event_setinput;
		trigger_event = &s_info->event_trigger;
	} else {
		input_event = trigger_event = NULL;
	}

	/* set input */
	dprec_start(input_event, cfg->overlap_layer_num, cfg->input_layer_num);
	ret = ext_disp_frame_cfg_input(cfg);
	if (ret == -2) {
		for (i = 0; i < cfg->input_layer_num; i++)
			mtkfb_release_layer_fence(cfg->session_id, i);

		return ret;
	}
	dprec_done(input_event, 0, 0);

	if (trigger_event) {
		/* debug UI thread or MM thread */
		unsigned int proc_name = (current->comm[0] << 24) |
			(current->comm[1] << 16) | (current->comm[2] << 8) |
			current->comm[3];

		dprec_start(trigger_event, proc_name, 0);
	}
	DISPFENCE("T+/E%d\n", DISP_SESSION_DEV(session_id));
	ret = external_display_trigger(cfg->tigger_mode, session_id);
	dprec_done(trigger_event, 0, 0);

	return ret;
}
