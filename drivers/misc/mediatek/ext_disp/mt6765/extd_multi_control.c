/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include <linux/kthread.h>

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
#include "layering_rule.h"
#endif

static const struct EXTD_DRIVER *extd_driver[DEV_MAX_NUM];
static struct SWITCH_MODE_INFO_STRUCT path_info;

struct task_struct *disp_switch_mode_task;
wait_queue_head_t switch_mode_wq;
atomic_t switch_mode_event = ATOMIC_INIT(0);

static int get_dev_index(unsigned int session)
{
	int dev_id = session & 0x0FF;

	if (dev_id >= DEV_MAX_NUM) {
		EXTDERR("get_dev_index device id error:%d\n", dev_id);
		return -1;
	}

	if (dev_id < DEV_LCM)
		dev_id -= 1;

	return dev_id;
}

static int extd_create_path(enum EXT_DISP_PATH_MODE mode, unsigned int session)
{
	int ret = 0;

	EXTDMSG("extd_create_path session:%08x, mode:%d\n", session,
			mode);

	ext_disp_path_set_mode(mode, session);
	ret = ext_disp_init(NULL, session);

	return ret;
}
static int extd_get_device_type(unsigned int session)
{
	int ret = -1;
	int dev_index;

	dev_index = get_dev_index(session);
	if (dev_index >= 0 && extd_driver[dev_index]
	    && extd_driver[dev_index]->ioctl)
		ret =
		    extd_driver[dev_index]->ioctl(GET_DEV_TYPE_CMD, 0, 0, NULL);

	/* EXTDINFO("device type is:%d\n", ret); */
	return ret;
}

static void extd_set_layer_num(int layer_num, unsigned int session)
{
	int dev_index;

	dev_index = get_dev_index(session);
	if (dev_index >= 0 && extd_driver[dev_index]
	    && extd_driver[dev_index]->ioctl)
		extd_driver[dev_index]->ioctl(SET_LAYER_NUM_CMD, layer_num, 0,
					      NULL);
}

static int create_external_display_path(unsigned int session, int mode)
{
	int ret = 0;
	int extd_type = DISP_IF_MHL;
	int device_id = 0;
	int has_virtual_disp = 0;
	int has_physical_disp = 0;

	EXTDMSG("create_external_display_path session:%08x, mode:%d\n",
		session, mode);

	if (DISP_SESSION_TYPE(session) == DISP_SESSION_MEMORY
	    && EXTD_OVERLAY_CNT > 0) {
		if (mode < DISP_SESSION_DIRECT_LINK_MIRROR_MODE
		    && (path_info.old_mode[DEV_WFD] >=
			DISP_SESSION_DIRECT_LINK_MIRROR_MODE
			|| path_info.old_session[DEV_WFD] !=
			DISP_SESSION_MEMORY)) {
			if (path_info.old_session[DEV_LCM] ==
			    DISP_SESSION_EXTERNAL)
				has_physical_disp = 1;

			if (has_physical_disp == 0
			    && ext_disp_wait_ovl_available(0) > 0) {
				ovl2mem_init(session);
				ovl2mem_setlayernum
				    (MEMORY_SESSION_INPUT_LAYER_COUNT);
			} else {
				EXTDERR("mhl path: OVL1 can't be split out!\n");
				ret = -1;
			}
		} else if (mode >= DISP_SESSION_DIRECT_LINK_MIRROR_MODE
			   && path_info.old_session[DEV_WFD] ==
			   DISP_SESSION_MEMORY
			   && path_info.old_mode[DEV_WFD] <
			   DISP_SESSION_DIRECT_LINK_MIRROR_MODE) {

			ovl2mem_deinit();
			ovl2mem_setlayernum(0);
			ext_disp_path_change(EXTD_OVL_IDLE_REQ, session);
#ifdef EXTD_DUAL_PIPE_SWITCH_SUPPORT
			/* Notify primary display can switch to dual pipe */
			set_hrt_state(DISP_HRT_FORCE_DUAL_OFF, 0);
#endif
		}
	} else if (DISP_SESSION_TYPE(session) == DISP_SESSION_EXTERNAL) {
		device_id = DISP_SESSION_DEV(session);
		if (device_id != DEV_LCM) {
			/*mhl/eink device */
			device_id -= 1;
		} else if (path_info.old_session[DEV_WFD] ==
			   DISP_SESSION_MEMORY) {
			/*has virtual display, 3 display at the same time */
			has_virtual_disp = 1;
		}

		extd_type = extd_get_device_type(session);

		if (path_info.old_session[device_id] == DISP_SESSION_EXTERNAL) {
			if (EXTD_OVERLAY_CNT < 1) {
				/* external display has no OVL to use */
				return ret;
			}
		}

		if (extd_type != DISP_IF_EPD
		    && (mode < DISP_SESSION_DIRECT_LINK_MIRROR_MODE
			|| extd_type == DISP_IF_HDMI_SMARTBOOK)) {

			if (ext_disp_wait_ovl_available(0) > 0
			    && has_virtual_disp == 0) {
				if (path_info.old_session[device_id] ==
				    DISP_SESSION_EXTERNAL
				    && extd_type != DISP_IF_HDMI_SMARTBOOK) {
					/*insert OVL to external dispaly path */
					ext_disp_path_change
					    (EXTD_OVL_INSERT_REQ, session);
				} else {
					extd_create_path(EXTD_DIRECT_LINK_MODE,
							 session);
				}

				extd_set_layer_num
				    (EXTERNAL_SESSION_INPUT_LAYER_COUNT,
				     session);
			} else {
				EXTDERR
				    ("mhl path: OVL1 can not be split out!\n");
				if (path_info.old_session[device_id] !=
				    DISP_SESSION_EXTERNAL) {
					/*insert OVL to external dispaly path */
					extd_create_path(EXTD_RDMA_DPI_MODE,
							 session);
					extd_set_layer_num(1, session);
				}
			}
		} else {
			if (path_info.old_session[device_id] ==
			    DISP_SESSION_EXTERNAL) {
				ext_disp_path_change(EXTD_OVL_REMOVE_REQ,
						     session);
				extd_set_layer_num(1, session);
			} else {
				extd_create_path(EXTD_RDMA_DPI_MODE, session);
				extd_set_layer_num(1, session);
			}
		}
	} else if (DISP_SESSION_TYPE(session) == DISP_SESSION_MEMORY
		   && EXTD_OVERLAY_CNT == 0) {
		EXTDERR("memory session and ovl time sharing!\n");
		ovl2mem_setlayernum(MEMORY_SESSION_INPUT_LAYER_COUNT);
	}

	return ret;
}

static void destroy_external_display_path(unsigned int session, int mode)
{
	int device_id = 0;

	EXTDMSG("destroy_external_display_path session:%08x\n",
		session);
	device_id = DISP_SESSION_DEV(session);
	if ((DISP_SESSION_TYPE(session) == DISP_SESSION_EXTERNAL)
	    && (device_id != DEV_LCM)) {
		/*mhl/eink device id */
		device_id -= 1;
	}

	if ((path_info.old_session[device_id] == DISP_SESSION_PRIMARY)
	    || (path_info.old_session[device_id] == DISP_SESSION_MEMORY
		&& path_info.old_mode[device_id] >=
		DISP_SESSION_DIRECT_LINK_MIRROR_MODE)) {
		/*discard for memory session in mirror mode */
		EXTDMSG
		    ("no need destroy path for session:0x%08x, mode:%d\n",
		     path_info.old_session[device_id],
		     path_info.old_mode[device_id]);
		return;
	}

	if (path_info.old_session[device_id] == DISP_SESSION_EXTERNAL) {
		ext_disp_deinit(session);
		extd_set_layer_num(0, session);
		ext_disp_path_change(EXTD_OVL_IDLE_REQ, session);
#ifdef EXTD_DUAL_PIPE_SWITCH_SUPPORT
		/* Notify primary display can switch to dual pipe */
		set_hrt_state(DISP_HRT_FORCE_DUAL_OFF, 0);
#endif
	} else if (path_info.old_session[device_id] == DISP_SESSION_MEMORY
		   && EXTD_OVERLAY_CNT > 0) {
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

	sched_setscheduler(current, SCHED_RR, &param);

	EXTDMSG("disp_switch_mode_kthread in!\n");

	for (;;) {
		wait_event_interruptible(switch_mode_wq,
					 atomic_read(&switch_mode_event));
		atomic_set(&switch_mode_event, 0);

		EXTDMSG
		    ("switch mode, create or change path, mode:%d, sess:0x%x\n",
		     path_info.cur_mode, path_info.ext_sid);
		ret =
		    create_external_display_path(path_info.ext_sid,
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
			/*thread exit */
			break;
		}
	}

	return 0;
}

#ifndef OVL_CASCADE_SUPPORT
static int path_change_without_cascade(enum DISP_MODE mode,
				       unsigned int session_id,
				       unsigned int device_id)
{
	int ret = -1;
	unsigned int session = 0;

	/*destroy external display path*/
	if (session_id == 0
	    && path_info.old_session[device_id] != DISP_SESSION_PRIMARY) {
		if (device_id == DEV_WFD) {
			/*make memory session for WFD */
			session =
			    MAKE_DISP_SESSION(DISP_SESSION_MEMORY, device_id);
		} else if (device_id == DEV_LCM) {
			/*make external session for LCM */
			session =
			    MAKE_DISP_SESSION(DISP_SESSION_EXTERNAL, device_id);
		} else {
			/*make external session */
			session =
			    MAKE_DISP_SESSION(DISP_SESSION_EXTERNAL,
					      device_id + 1);
		}

		destroy_external_display_path(session,
					      DISP_SESSION_DIRECT_LINK_MODE);
		path_info.old_session[device_id] = DISP_SESSION_PRIMARY;
		path_info.old_mode[device_id] = DISP_SESSION_DIRECT_LINK_MODE;
		path_info.switching = DEV_MAX_NUM;

		return 1;
	}

	/*create path or change path */
	if ((session_id > 0
	     && path_info.old_session[device_id] == DISP_SESSION_PRIMARY)
	    || (mode != path_info.old_mode[device_id]
		&& path_info.old_session[device_id] != DISP_SESSION_PRIMARY)) {
		ret = create_external_display_path(session_id, mode);
		if (ret == 0) {
			path_info.old_session[device_id] =
			    DISP_SESSION_TYPE(session_id);
			path_info.old_mode[device_id] = mode;
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

	/*destroy external display path*/
	if (session_id == 0
	    && path_info.old_session[device_id] != DISP_SESSION_PRIMARY) {
		if (device_id == DEV_WFD) {
			/*make memory session for WFD */
			session =
			    MAKE_DISP_SESSION(DISP_SESSION_MEMORY, device_id);
		} else if (device_id == DEV_LCM) {
			/*make external session for LCM */
			session =
			    MAKE_DISP_SESSION(DISP_SESSION_EXTERNAL, device_id);
		} else {
			/*make external session */
			session =
			    MAKE_DISP_SESSION(DISP_SESSION_EXTERNAL,
					      device_id + 1);
		}

		destroy_external_display_path(session,
					      DISP_SESSION_DIRECT_LINK_MODE);
		path_info.old_session[device_id] = DISP_SESSION_PRIMARY;
		path_info.old_mode[device_id] = DISP_SESSION_DIRECT_LINK_MODE;
		path_info.switching = DEV_MAX_NUM;
		path_info.ext_sid = 0;

		return 1;
	}

	/*create path or change path */
	if ((session_id > 0
	     && path_info.old_session[device_id] == DISP_SESSION_PRIMARY)
	    || (mode != path_info.old_mode[device_id]
		&& path_info.old_session[device_id] != DISP_SESSION_PRIMARY)) {
		/* the case will use OVL */
		disp_type = extd_get_device_type(session_id);
		if (disp_type != DISP_IF_EPD
		    && (mode < DISP_SESSION_DIRECT_LINK_MIRROR_MODE
			|| disp_type == DISP_IF_HDMI_SMARTBOOK)) {
			/*request OVL */
			ext_disp_path_change(EXTD_OVL_REQUSTING_REQ,
					     session_id);
		}

		if (path_info.old_session[device_id] == DISP_SESSION_EXTERNAL
		    && mode >= DISP_SESSION_DIRECT_LINK_MIRROR_MODE) {
			/* it is to say that the path is RDMA-DPI */
			extd_set_layer_num(1, session_id);
		}

		EXTDMSG("path_change_with_cascade, wake up\n");
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

	EXTDFUNC();
	memset(&path_info, 0, sizeof(struct SWITCH_MODE_INFO_STRUCT));
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
	disp_switch_mode_task =
	    kthread_create(disp_switch_mode_kthread, NULL,
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
		if (DISP_SESSION_TYPE(session) == DISP_SESSION_EXTERNAL
		    && DISP_SESSION_DEV(session) == DEV_EINK + 1) {
			if (extd_driver[DEV_EINK]->power_enable) {
				/*1 for power on, 0 for power off */
				extd_driver[DEV_EINK]->power_enable(1);
			}
		}
	}

	ret = ext_disp_trigger(0, ext_fence_release_callback, 0, session);

	if (trigger == TRIGGER_SUSPEND) {
		ext_disp_suspend_trigger(NULL, 0, session);
		if (DISP_SESSION_TYPE(session) == DISP_SESSION_EXTERNAL
		    && DISP_SESSION_DEV(session) == DEV_EINK + 1) {
			if (extd_driver[DEV_EINK]->power_enable) {
				/*1 for power on, 0 for power off */
				extd_driver[DEV_EINK]->power_enable(0);
			}
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

	EXTDFUNC();
	if (session == 0) {
		for (i = DEV_MHL; i < DEV_MAX_NUM; i++) {
			if (i <= DEV_EINK)
				session_id =
				    MAKE_DISP_SESSION(DISP_SESSION_EXTERNAL,
						      i + 1);
			else if (i == DEV_WFD)
				session_id =
				    MAKE_DISP_SESSION(DISP_SESSION_MEMORY, i);
			else
				session_id =
				    MAKE_DISP_SESSION(DISP_SESSION_EXTERNAL, i);

			ext_disp_suspend(session_id);
		}
	} else
		ret = ext_disp_suspend(session);

	return ret;
}

int external_display_resume(unsigned int session)
{
	int i = 0;
	int ret = 0;
	unsigned int session_id = 0;

	EXTDFUNC();
	if (session == 0) {
		for (i = DEV_MHL; i < DEV_MAX_NUM; i++) {
			if (i <= DEV_EINK)
				session_id =
				    MAKE_DISP_SESSION(DISP_SESSION_EXTERNAL,
						      i + 1);
			else if (i == DEV_WFD)
				session_id =
				    MAKE_DISP_SESSION(DISP_SESSION_MEMORY, i);
			else
				session_id =
				    MAKE_DISP_SESSION(DISP_SESSION_EXTERNAL, i);

			ext_disp_resume(session_id);
		}
	} else
		ret = ext_disp_resume(session);

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

	dev_index = get_dev_index(session);
	if (dev_index >= 0 && extd_driver[dev_index]
	    && extd_driver[dev_index]->get_dev_info)
		ret = extd_driver[dev_index]->get_dev_info(SF_GET_INFO, info);

	return ret;
}

int external_display_switch_mode(enum DISP_MODE mode,
				 unsigned int *session_created,
				 unsigned int session)
{
	int i = 0;
	int j = 0;
	int ret = -1;
	int switching = 0;
	int session_id[DEV_MAX_NUM] = { 0 };

	if (session_created == NULL) {
		/*error,no session can be compared */
		return ret;
	}

	if (path_info.switching < DEV_MAX_NUM) {
		/*mode is switching, return directly */
		return ret;
	}

	path_info.switching = DEV_MAX_NUM - 1;

	for (i = 0; i < MAX_SESSION_COUNT; i++) {
		if (session_created[i] ==
		    MAKE_DISP_SESSION(DISP_SESSION_EXTERNAL, DEV_MHL + 1)) {
			/*it has MHL session */
			session_id[DEV_MHL] = session_created[i];
		}

		if (session_created[i] ==
		    MAKE_DISP_SESSION(DISP_SESSION_EXTERNAL, DEV_EINK + 1)) {
			/*it has EINK session */
			session_id[DEV_EINK] = session_created[i];
		}

		if (session_created[i] ==
		    MAKE_DISP_SESSION(DISP_SESSION_MEMORY, DEV_WFD)) {
			/*it has WFD session */
			session_id[DEV_WFD] = session_created[i];
		}

		if (session_created[i] ==
		    MAKE_DISP_SESSION(DISP_SESSION_EXTERNAL, DEV_LCM)) {
			/*it has WFD session */
			session_id[DEV_LCM] = session_created[i];
		}
	}

	for (j = 0; j < DEV_MAX_NUM; j++) {
#ifndef OVL_CASCADE_SUPPORT
		switching = path_change_without_cascade(mode, session_id[j], j);
#else
		switching = path_change_with_cascade(mode, session_id[j], j);
#endif

		if (switching == 1) {
			/*session switching one by one */
			break;
		}
	}

	path_info.switching =
	    (switching == 0) ? DEV_MAX_NUM : path_info.switching;

	return 0;
}

int external_display_frame_cfg(struct disp_frame_cfg_t *cfg)
{
	int ret = 0;
	unsigned int i;
	unsigned int session_id = 0;
	struct dprec_logger_event *input_event, *trigger_event;
	struct disp_session_sync_info *session_info =
	    disp_get_session_sync_info_for_debug(cfg->session_id);

	session_id = cfg->session_id;

	if (session_info) {
		input_event = &session_info->event_setinput;
		trigger_event = &session_info->event_trigger;
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
		/* to debug UI thread or MM thread */
		unsigned int proc_name = (current->comm[0] << 24) |
		    (current->comm[1] << 16) | (current->
						comm[2] << 8) | (current->
								 comm[3] << 0);
		dprec_start(trigger_event, proc_name, 0);
	}
	DISPPR_FENCE("T+/E%d\n", DISP_SESSION_DEV(session_id));
	ret = external_display_trigger(cfg->tigger_mode, session_id);
	dprec_done(trigger_event, 0, 0);

	return ret;
}
