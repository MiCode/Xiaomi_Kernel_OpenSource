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

#include "mtk_drm_mmp.h"
#include "mtk_log.h"

static struct DRM_MMP_Events g_DRM_MMP_Events;
static struct CRTC_MMP_Events g_CRTC_MMP_Events[MMP_CRTC_NUM];

/* need to update if add new mmp_event in DRM_MMP_Events */
void init_drm_mmp_event(void)
{
	int i;

	if (g_DRM_MMP_Events.drm)
		return;

	g_DRM_MMP_Events.drm = mmprofile_register_event(MMP_ROOT_EVENT, "DRM");

	/* init DRM mmp events */
	g_DRM_MMP_Events.IRQ =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "IRQ");
	g_DRM_MMP_Events.ovl =
		mmprofile_register_event(g_DRM_MMP_Events.IRQ, "OVL");
	g_DRM_MMP_Events.ovl0 =
		mmprofile_register_event(g_DRM_MMP_Events.ovl, "OVL0");
	g_DRM_MMP_Events.ovl1 =
		mmprofile_register_event(g_DRM_MMP_Events.ovl, "OVL1");
	g_DRM_MMP_Events.ovl0_2l =
		mmprofile_register_event(g_DRM_MMP_Events.ovl, "OVL0_2L");
	g_DRM_MMP_Events.ovl1_2l =
		mmprofile_register_event(g_DRM_MMP_Events.ovl, "OVL1_2L");
	g_DRM_MMP_Events.ovl2_2l =
		mmprofile_register_event(g_DRM_MMP_Events.ovl, "OVL2_2L");
	g_DRM_MMP_Events.ovl3_2l =
		mmprofile_register_event(g_DRM_MMP_Events.ovl, "OVL3_2L");
	g_DRM_MMP_Events.rdma =
		mmprofile_register_event(g_DRM_MMP_Events.IRQ, "RDMA");
	g_DRM_MMP_Events.rdma0 =
		mmprofile_register_event(g_DRM_MMP_Events.rdma, "RDMA0");
	g_DRM_MMP_Events.rdma1 =
		mmprofile_register_event(g_DRM_MMP_Events.rdma, "RDMA1");
	g_DRM_MMP_Events.wdma =
		mmprofile_register_event(g_DRM_MMP_Events.IRQ, "WDMA");
	g_DRM_MMP_Events.wdma0 =
		mmprofile_register_event(g_DRM_MMP_Events.wdma, "WDMA0");
	g_DRM_MMP_Events.dsi =
		mmprofile_register_event(g_DRM_MMP_Events.IRQ, "DSI");
	g_DRM_MMP_Events.dsi0 =
		mmprofile_register_event(g_DRM_MMP_Events.dsi, "DSI0");
	g_DRM_MMP_Events.dsi1 =
		mmprofile_register_event(g_DRM_MMP_Events.dsi, "DSI1");
	g_DRM_MMP_Events.pmqos =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "PMQOS");
	g_DRM_MMP_Events.hrt_bw =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "HRT_BW");
	g_DRM_MMP_Events.mutex_lock =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "LOCK");
	g_DRM_MMP_Events.layering =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "HRT");
	g_DRM_MMP_Events.dma_alloc =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "D_ALLOC");
	g_DRM_MMP_Events.dma_free =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "D_FREE");
	g_DRM_MMP_Events.ion_import_dma =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "I_DMA");
	g_DRM_MMP_Events.ion_import_fd =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "I_FD");
	g_DRM_MMP_Events.ion_import_free =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "I_FREE");
	g_DRM_MMP_Events.set_mode =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "SET_MODE");

	g_DRM_MMP_Events.ddp =
		mmprofile_register_event(g_DRM_MMP_Events.IRQ, "MUTEX");
	for (i = 0; i < DISP_MUTEX_DDP_COUNT; i++) {
		char name[32];

		snprintf(name, sizeof(name), "MUTEX%d", i);
		g_DRM_MMP_Events.mutex[i] =
			mmprofile_register_event(g_DRM_MMP_Events.ddp, name);
	}

	g_DRM_MMP_Events.postmask =
		mmprofile_register_event(g_DRM_MMP_Events.IRQ, "POSTMASK");
	g_DRM_MMP_Events.postmask0 = mmprofile_register_event(
		g_DRM_MMP_Events.postmask, "POSTMASK0");
	g_DRM_MMP_Events.abnormal_irq =
		mmprofile_register_event(g_DRM_MMP_Events.IRQ, "ABNORMAL_IRQ");
}

/* need to update if add new mmp_event in CRTC_MMP_Events */
void init_crtc_mmp_event(void)
{
	int i = 0;

	for (i = 0; i < MMP_CRTC_NUM; i++) {
		char name[32];
		mmp_event crtc_mmp_root;

		/* create i th root of CRTC mmp events */
		snprintf(name, sizeof(name), "crtc%d", i);
		crtc_mmp_root =
			mmprofile_register_event(g_DRM_MMP_Events.drm, name);
		g_DRM_MMP_Events.crtc[i] = crtc_mmp_root;

		/* init CRTC mmp events */
		g_CRTC_MMP_Events[i].trig_loop_done = mmprofile_register_event(
			crtc_mmp_root, "trig_loop_done");
		g_CRTC_MMP_Events[i].enable =
			mmprofile_register_event(crtc_mmp_root, "enable");
		g_CRTC_MMP_Events[i].disable =
			mmprofile_register_event(crtc_mmp_root, "disable");
		g_CRTC_MMP_Events[i].release_fence = mmprofile_register_event(
			crtc_mmp_root, "release_fence");
		g_CRTC_MMP_Events[i].update_present_fence =
			mmprofile_register_event(crtc_mmp_root,
				"update_present_fence");
		g_CRTC_MMP_Events[i].release_present_fence =
			mmprofile_register_event(crtc_mmp_root,
				"release_present_fence");
		g_CRTC_MMP_Events[i].atomic_begin = mmprofile_register_event(
			crtc_mmp_root, "atomic_begin");
		g_CRTC_MMP_Events[i].atomic_flush = mmprofile_register_event(
			crtc_mmp_root, "atomic_flush");
		g_CRTC_MMP_Events[i].enable_vblank = mmprofile_register_event(
			crtc_mmp_root, "enable_vblank");
		g_CRTC_MMP_Events[i].disable_vblank = mmprofile_register_event(
			crtc_mmp_root, "disable_vblank");
		g_CRTC_MMP_Events[i].esd_check =
			mmprofile_register_event(crtc_mmp_root, "ESD check");
		g_CRTC_MMP_Events[i].esd_recovery =
			mmprofile_register_event(crtc_mmp_root, "ESD recovery");
		g_CRTC_MMP_Events[i].leave_idle = mmprofile_register_event(
			crtc_mmp_root, "leave_idle");
		g_CRTC_MMP_Events[i].enter_idle = mmprofile_register_event(
			crtc_mmp_root, "enter_idle");
		g_CRTC_MMP_Events[i].frame_cfg =
			mmprofile_register_event(crtc_mmp_root, "frame cfg");
		g_CRTC_MMP_Events[i].suspend = mmprofile_register_event(
			crtc_mmp_root, "suspend");
		g_CRTC_MMP_Events[i].resume = mmprofile_register_event(
			crtc_mmp_root, "resume");
		g_CRTC_MMP_Events[i].dsi_suspend = mmprofile_register_event(
			crtc_mmp_root, "dsi_suspend");
		g_CRTC_MMP_Events[i].dsi_resume = mmprofile_register_event(
			crtc_mmp_root, "dsi_resume");
		g_CRTC_MMP_Events[i].backlight = mmprofile_register_event(
			crtc_mmp_root, "backlight");
		g_CRTC_MMP_Events[i].backlight_grp = mmprofile_register_event(
			crtc_mmp_root, "backlight_grp");
		g_CRTC_MMP_Events[i].ddic_send_cmd = mmprofile_register_event(
			crtc_mmp_root, "ddic_send_cmd");
		g_CRTC_MMP_Events[i].ddic_read_cmd = mmprofile_register_event(
			crtc_mmp_root, "ddic_read_cmd");
		g_CRTC_MMP_Events[i].path_switch = mmprofile_register_event(
			crtc_mmp_root, "path_switch");
		g_CRTC_MMP_Events[i].user_cmd = mmprofile_register_event(
			crtc_mmp_root, "user_cmd");
		g_CRTC_MMP_Events[i].check_trigger = mmprofile_register_event(
			crtc_mmp_root, "check_trigger");
		g_CRTC_MMP_Events[i].kick_trigger = mmprofile_register_event(
			crtc_mmp_root, "kick_trigger");
		g_CRTC_MMP_Events[i].atomic_commit = mmprofile_register_event(
			crtc_mmp_root, "atomic_commit");
		g_CRTC_MMP_Events[i].user_cmd_cb =
			mmprofile_register_event(crtc_mmp_root, "user_cmd_cb");
		g_CRTC_MMP_Events[i].bl_cb =
			mmprofile_register_event(crtc_mmp_root, "bl_cb");
		g_CRTC_MMP_Events[i].clk_change = mmprofile_register_event(
			crtc_mmp_root, "clk_change");
	}
}

void drm_mmp_init(void)
{
	DDPMSG("%s\n", __func__);

	mmprofile_enable(1);

	/* init mmp events */
	init_drm_mmp_event();
	init_crtc_mmp_event();

	/* enable all mmp events */
	mmprofile_enable_event_recursive(g_DRM_MMP_Events.drm, 1);

	mmprofile_start(1);
}

struct DRM_MMP_Events *get_drm_mmp_events(void)
{
	return &g_DRM_MMP_Events;
}

struct CRTC_MMP_Events *get_crtc_mmp_events(unsigned long id)
{
	return &g_CRTC_MMP_Events[id];
}
