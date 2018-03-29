/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#ifndef __H_DDP_MMP__
#define __H_DDP_MMP__

#include "mmprofile.h"
#include "ddp_info.h"
typedef struct _DDP_MMP_Events {
	MMP_Event DDP;
	MMP_Event layerParent;
	MMP_Event layer[4];
	MMP_Event ovl1_layer[4];
	MMP_Event layer_dump_parent;
	MMP_Event layer_dump[4];
	MMP_Event ovl1layer_dump[4];
	MMP_Event wdma_dump[2];
	MMP_Event rdma_dump[2];
	MMP_Event DDP_IRQ;
	MMP_Event OVL_IRQ_Parent;
	MMP_Event OVL_IRQ[2];
	MMP_Event WDMA_IRQ_Parent;
	MMP_Event WDMA_IRQ[2];
	MMP_Event RDMA_IRQ_Parent;
	MMP_Event RDMA_IRQ[3];
	MMP_Event SCREEN_UPDATE[3];
	MMP_Event DSI_IRQ_Parent;
	MMP_Event DSI_IRQ[2];
	MMP_Event MutexParent;
	MMP_Event MUTEX_IRQ[5];
	MMP_Event primary_Parent;
	MMP_Event primary_trigger;
	MMP_Event primary_suspend;
	MMP_Event primary_resume;
	MMP_Event primary_config;
	MMP_Event primary_rdma_config;
	MMP_Event primary_wdma_config;
	MMP_Event primary_set_dirty;
	MMP_Event primary_cmdq_flush;
	MMP_Event primary_cmdq_done;
	MMP_Event primary_display_cmd;
	MMP_Event primary_cache_sync;
	MMP_Event primary_display_aalod_trigger;
	MMP_Event primary_wakeup;
	MMP_Event primary_switch_mode;
	MMP_Event primary_seq_insert;
	MMP_Event primary_seq_config;
	MMP_Event primary_seq_trigger;
	MMP_Event primary_seq_rdma_irq;
	MMP_Event primary_seq_release;
	MMP_Event primary_ovl_fence_release;
	MMP_Event primary_wdma_fence_release;
	MMP_Event present_fence_release;
	MMP_Event present_fence_get;
	MMP_Event present_fence_set;
	MMP_Event ovl_trigger;
	MMP_Event interface_trigger;
	MMP_Event Extd_Parent;
	MMP_Event Extd_layerParent;
	MMP_Event Extd_layer[4];
	MMP_Event Extd_layer_dump_parent;
	MMP_Event Extd_State;
	MMP_Event Extd_DevInfo;
	MMP_Event Extd_ErrorInfo;
	MMP_Event Extd_Mutex;
	MMP_Event Extd_ImgDump;
	MMP_Event Extd_IrqStatus;
	MMP_Event Extd_UsedBuff;
	MMP_Event Extd_trigger;
	MMP_Event Extd_config;
	MMP_Event Extd_set_dirty;
	MMP_Event Extd_cmdq_flush;
	MMP_Event Extd_cmdq_done;
	MMP_Event Extd_release_present_fence;
	MMP_Event Extd_prepare_present_fence;
	MMP_Event Extd_present_fence_set;
	MMP_Event Extd_fence_prepare[4];
	MMP_Event Extd_fence_release[4];
	MMP_Event dprec_cpu_write_reg;
	MMP_Event primary_sw_mutex;
	MMP_Event primary_set_bl;
	MMP_Event ESD_Parent;
	MMP_Event esd_check_t;
	MMP_Event esd_recovery_t;
	MMP_Event esd_extte;
	MMP_Event esd_rdlcm;
	MMP_Event esd_vdo_eint;
	MMP_Event session_Parent;
	MMP_Event session_prepare;
	MMP_Event session_set_input;
	MMP_Event session_trigger;
	MMP_Event session_find_idx;
	MMP_Event session_release;
	MMP_Event session_wait_vsync;
	MMP_Event MonitorParent;
	MMP_Event trigger_delay;
	MMP_Event release_delay;
	MMP_Event vsync_count;
	MMP_Event dal_printf;
	MMP_Event dal_clean;
	MMP_Event ovl_enable;
	MMP_Event ovl_disable;
	MMP_Event cascade_enable;
	MMP_Event cascade_disable;
	MMP_Event ddp_abnormal_irq;
	MMP_Event ovl1_status;
	MMP_Event dpmgr_wait_event_timeout;
	MMP_Event cmdq_rebuild;
	MMP_Event dsi_te;
	MMP_Event dsi_wrlcm;
} DDP_MMP_Events_t;

DDP_MMP_Events_t *ddp_mmp_get_events(void);
void init_ddp_mmp_events(void);
void ddp_mmp_init(void);
void ddp_mmp_ovl_layer(OVL_CONFIG_STRUCT *pLayer, unsigned int down_sample_x,
		       unsigned int down_sample_y,
		       unsigned int session /*1:primary, 2:external, 3:memory */);
void ddp_mmp_wdma_layer(WDMA_CONFIG_STRUCT *wdma_layer, unsigned int wdma_num,
			unsigned int down_sample_x, unsigned int down_sample_y);
void ddp_mmp_rdma_layer(RDMA_CONFIG_STRUCT *rdma_layer, unsigned int rdma_num,
			unsigned int down_sample_x, unsigned int down_sample_y);

#ifdef DEFAULT_MMP_ENABLE
extern void MMProfileEnable(int enable);
extern void MMProfileStart(int start);
#endif

#endif
