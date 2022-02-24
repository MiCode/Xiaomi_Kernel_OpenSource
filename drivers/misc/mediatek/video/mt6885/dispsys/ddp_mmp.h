/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __H_DDP_MMP__
#define __H_DDP_MMP__

#include "mmprofile.h"
/* #include "mmprofile_function.h" */
#include "ddp_info.h"
#include "disp_session.h"
struct DDP_MMP_Events {
	mmp_event DDP;
	mmp_event layerParent;
	mmp_event layer[4];
	mmp_event ovl1_layer[4];
	mmp_event layer_dump_parent;
	mmp_event layer_dump[4];
	mmp_event ovl1layer_dump[4];
	mmp_event wdma_dump[2];
	mmp_event rdma_dump[2];
	mmp_event DDP_IRQ;
	mmp_event DDP_event;
	mmp_event event_wait;
	mmp_event event_signal;
	mmp_event event_error;
	mmp_event OVL_IRQ_Parent;
	mmp_event OVL_IRQ[OVL_NUM];
	mmp_event WDMA_IRQ_Parent;
	mmp_event WDMA_IRQ[2];
	mmp_event RDMA_IRQ_Parent;
	mmp_event RDMA_IRQ[3];
	mmp_event SCREEN_UPDATE[3];
	mmp_event DSI_IRQ_Parent;
	mmp_event DSI_IRQ[2];
	mmp_event MutexParent;
	mmp_event MUTEX_IRQ[5];
	mmp_event POSTMASK_IRQ;
	mmp_event primary_Parent;
	mmp_event primary_display_switch_dst_mode;
	mmp_event primary_trigger;
	mmp_event primary_suspend;
	mmp_event primary_resume;
	mmp_event primary_config;
	mmp_event primary_query_valid;
	mmp_event primary_rdma_config;
	mmp_event primary_wdma_config;
	mmp_event primary_set_dirty;
	mmp_event primary_cmdq_flush;
	mmp_event primary_cmdq_done;
	mmp_event primary_display_cmd;
	mmp_event primary_cache_sync;
	mmp_event primary_display_aalod_trigger;
	mmp_event primary_wakeup;
	mmp_event primary_switch_mode;
	mmp_event primary_mode[DISP_SESSION_MODE_NUM];
	mmp_event primary_seq_info;
	mmp_event primary_switch_fps;
	mmp_event primary_seq_insert;
	mmp_event primary_seq_config;
	mmp_event primary_seq_trigger;
	mmp_event primary_seq_rdma_irq;
	mmp_event primary_seq_release;
	mmp_event primary_ovl_fence_release;
	mmp_event primary_wdma_fence_release;
	mmp_event present_fence_release;
	mmp_event present_fence_get;
	mmp_event present_fence_set;
	mmp_event esd_recovery;
	mmp_event esd_cmdq;
	mmp_event idlemgr;
	mmp_event idle_monitor;
	mmp_event share_sram;
	mmp_event sbch_set;
	mmp_event sbch_set_error;
	mmp_event sec;
	mmp_event svp_module[DISP_MODULE_NUM];
	mmp_event tui;
	mmp_event self_refresh;
	mmp_event fps_set;
	mmp_event fps_get;
	mmp_event fps_ext_set;
	mmp_event fps_ext_get;
	mmp_event primary_error;
	mmp_event ovl_trigger;
	mmp_event interface_trigger;
	mmp_event hrt;
	mmp_event dvfs;
	mmp_event Extd_Parent;
	mmp_event Extd_layerParent;
	mmp_event Extd_layer[4];
	mmp_event Extd_layer_dump_parent;
	mmp_event Extd_State;
	mmp_event Extd_DevInfo;
	mmp_event Extd_ErrorInfo;
	mmp_event Extd_Mutex;
	mmp_event Extd_ImgDump;
	mmp_event Extd_IrqStatus;
	mmp_event Extd_UsedBuff;
	mmp_event Extd_trigger;
	mmp_event Extd_config;
	mmp_event Extd_set_dirty;
	mmp_event Extd_cmdq_flush;
	mmp_event Extd_cmdq_done;
	mmp_event dprec_cpu_write_reg;
	mmp_event primary_sw_mutex;
	mmp_event primary_set_bl;
	mmp_event ESD_Parent;
	mmp_event esd_check_t;
	mmp_event esd_recovery_t;
	mmp_event esd_extte;
	mmp_event esd_rdlcm;
	mmp_event esd_vdo_eint;
	mmp_event session_Parent;
	mmp_event session_prepare;
	mmp_event session_set_input;
	mmp_event session_trigger;
	mmp_event session_find_idx;
	mmp_event session_release;
	mmp_event session_wait_vsync;
	mmp_event MonitorParent;
	mmp_event rdma_underflow;
	mmp_event trigger_delay;
	mmp_event release_delay;
	mmp_event vsync_count;
	mmp_event dal_printf;
	mmp_event dal_clean;
	mmp_event tmp_debug;
	mmp_event cg_mode;
	mmp_event power_down_mode;
	mmp_event sodi_disable;
	mmp_event sodi_enable;
	mmp_event ovl_enable;
	mmp_event ovl_disable;
	mmp_event cascade_enable;
	mmp_event cascade_disable;
	mmp_event ddp_abnormal_irq;
	mmp_event ovl1_status;
	mmp_event dpmgr_wait_event_timeout;
	mmp_event cmdq_rebuild;
	mmp_event LFR_NUM;
	mmp_event dsi_te;
	mmp_event dsi_frame_done;
	mmp_event dsi_lfr_switch;
	mmp_event Dsi_Update;
	mmp_event primary_set_cmd;
	mmp_event primary_pm_qos;
};

struct DDP_MMP_Events *ddp_mmp_get_events(void);
void init_ddp_mmp_events(void);
void ddp_mmp_init(void);
void ddp_mmp_ovl_layer(struct OVL_CONFIG_STRUCT *pLayer,
	unsigned int down_sample_x, unsigned int down_sample_y,
	unsigned int session);
void ddp_mmp_wdma_layer(struct WDMA_CONFIG_STRUCT *wdma_layer,
	unsigned int wdma_num, unsigned int down_sample_x,
	unsigned int down_sample_y);
void ddp_mmp_rdma_layer(struct RDMA_CONFIG_STRUCT *rdma_layer,
	unsigned int rdma_num, unsigned int down_sample_x,
	unsigned int down_sample_y);

/*defined in mmp driver, should remove it */

#endif
