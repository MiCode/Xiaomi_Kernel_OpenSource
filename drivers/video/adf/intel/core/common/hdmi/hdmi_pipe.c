/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
 * Copyright (c) 2008, Tungsten Graphics, Inc. Cedar Park, TX., USA.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 **************************************************************************/
#include <linux/kernel.h>
#include <video/adf_client.h>
#include "hdmi_pipe.h"
#include "hdmi_edid.h"
#include "hdmi_hotplug.h"
#include "hdmi_hdcp.h"

static const struct drm_mode_modeinfo fake_mode = {
	/* 4 - 1280x720@60Hz */
	 DEFINE_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 1390,
		   1430, 1650, 0, 720, 725, 730, 750, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC |
		   DRM_MODE_FLAG_PAR4_3),
	  .vrefresh = 60, };

/*
	return true if connection status changed
	return false if else
*/

static u32 vsync_counter;
#define VSYNC_COUNT_MAX_MASK 0xffffff


static bool hdmi_check_connection_status(struct hdmi_pipe *pipe)
{
	bool is_current_connected;
	bool last_status;

	if (pipe == NULL)
		return -EINVAL;

	last_status = atomic_read(&pipe->hpd_ctx.is_connected);
	is_current_connected = mofd_hdmi_get_cable_status(&pipe->config);

	if (is_current_connected != last_status) {
		if (is_current_connected)
			atomic_set(&pipe->hpd_ctx.is_connected, 1);
		else
			atomic_set(&pipe->hpd_ctx.is_connected, 0);
		return true;
	}
	return false;
}

static void populate_modes(struct hdmi_pipe *pipe)
{
	struct hdmi_monitor *monitor;
	struct hdmi_mode_info *mode_info, *t, *saved_mode = NULL;
	int active_region = 0;
	int saved_region = 0;
	int vrefresh = 0;
	int saved_vrefresh = 0;
	int modes_num = 0;
	monitor = &pipe->monitor;

	/*step 1: destroy all the modes that exceeds the limit*/
	list_for_each_entry_safe(mode_info, t,
					&monitor->probedModes,
					head) {
		modes_num++;
		if (mode_info->drm_mode.hdisplay > HDMI_MAX_DISPLAY_H  ||
			mode_info->drm_mode.vdisplay > HDMI_MAX_DISPLAY_V ||
			mode_info->drm_mode.clock > pipe->config.max_clock ||
			mode_info->drm_mode.clock < pipe->config.min_clock ||
			mode_info->drm_mode.flags & DRM_MODE_FLAG_DBLSCAN ||
			mode_info->drm_mode.flags & DRM_MODE_FLAG_INTERLACE) {

			list_del(&mode_info->head);
			kfree(mode_info);
			modes_num--;
		}
	}

	/*step 2: find the preferred mode with max display size */
	list_for_each_entry_safe(mode_info, t,
				&monitor->probedModes, head) {

		active_region =
			mode_info->drm_mode.hdisplay *
						mode_info->drm_mode.vdisplay;

			if (active_region > saved_region) {
				saved_region = active_region;
				vrefresh = mode_vrefresh(&mode_info->drm_mode);

				pr_debug("%s:vrefresh=%d,saved_vrefresh = %d\n",
					__func__, vrefresh, saved_vrefresh);

				saved_vrefresh = vrefresh;
				saved_mode = mode_info;
			} else if (active_region == saved_region) {
					vrefresh = mode_vrefresh(
							&mode_info->drm_mode);
					if (vrefresh > saved_vrefresh) {
						saved_vrefresh = vrefresh;
						saved_mode = mode_info;
					}
			}
	}

	if (saved_mode)
		saved_mode->drm_mode.type |= DRM_MODE_TYPE_PREFERRED;


	pr_debug("Left %d modes after validate\n", modes_num);
	list_for_each_entry(mode_info,
					&monitor->probedModes,
					head) {
		pr_debug("vdisplay=%d, hdisplay=%d, vtotal=%d, htotal=%d, vfresh=%d,"
			"flags=%x, type=%x, name=%s, clock=%d\n",
		mode_info->drm_mode.vdisplay,
		mode_info->drm_mode.hdisplay, mode_info->drm_mode.vtotal,
		mode_info->drm_mode.htotal, mode_info->drm_mode.vrefresh,
		mode_info->drm_mode.flags, mode_info->drm_mode.type,
		mode_info->drm_mode.name, mode_info->drm_mode.clock);

		pipe->monitor.preferred_mode = &saved_mode->drm_mode;
	}

}

static irqreturn_t hotplug_thread_fn(int irq, void *data)
{
	struct hdmi_pipe *pipe = (struct hdmi_pipe *)data;
	struct hdmi_hotplug_context *hpd_ctx;
	pid_t current_pid;

	current_pid = task_pid_nr(current);

	pr_debug("In %s and pipe=%p\n", __func__, pipe);

	if (pipe == NULL) {
		pr_err("%s: Hdmi hotplug context is empty\n", __func__);
		return IRQ_HANDLED;
	}

	hpd_ctx = &pipe->hpd_ctx;

	if (hdmi_check_connection_status(pipe)) {
		atomic_set(&hpd_ctx->is_asserted, 1);
		return intel_adf_context_on_event();
	}
	return IRQ_HANDLED;
}

static int hdmi_get_screen_size(struct intel_pipe *pipe,
		u16 *width_mm, u16 *height_mm)
{
	struct hdmi_pipe *hdmi_pipe = to_hdmi_pipe(pipe);
	*width_mm = hdmi_pipe->monitor.screen_width_mm;
	*height_mm = hdmi_pipe->monitor.screen_height_mm;

	pr_debug("%s: width=%d height=%d\n", __func__, *width_mm, *height_mm);
	return 0;
}

static void gunit_iosf_write32(u32 ep_id, u32 reg, u32 val)
{
	u32 ret;
	int retry = 0;
	u32 sb_pkt = (1 << 16) | (ep_id << 8) | 0xf0;

	/* Write value to side band register */
	REG_WRITE(0x2108, reg);
	REG_WRITE(0x2104, val);
	REG_WRITE(0x2100, sb_pkt);

	/* Check if transaction is complete */
	ret = REG_READ(0x210C);
	while ((retry++ < 0x1000) && (ret != 0x2)) {
		usleep_range(500, 1000);
		ret = REG_READ(0x210C);
	}

	if (ret != 2)
		pr_err("%s: failed to program DPLL\n", __func__);
}

static u32 gunit_iosf_read32(u32 ep_id, u32 reg)
{
	u32 ret;
	int retry = 0;
	u32 sb_pkt = (0 << 16) | (ep_id << 8) | 0xf0;

	/* Read side band register */
	REG_WRITE(0x2108, reg);
	REG_WRITE(0x2100, sb_pkt);

	/* Check if transaction is complete */
	ret = REG_READ(0x210C);
	while ((retry++ < 0x1000) && (ret != 2)) {
		usleep_range(500, 1000);
		ret = REG_READ(0x210C);
	}

	if (ret != 2)
		pr_err("%s: Failed to read\n", __func__);
	else
		ret = REG_READ(0x2104);

	return ret;
}

static int  hdmi_setup_clock(u32 div)
{
	u32 tmp = 0;
	u32 ret;
	int retry = 0;

	pr_debug("In %s div=%x\n", __func__, div);

	/* Common reset */
	REG_WRITE(DISP_PLL_CTRL, 0x70006800);  /* DPLLB */

	/* Program DPLL registers via IOSF (TNG display HAS) */

	/* Process monitor to 19.2MHz */
	gunit_iosf_write32(DPLL_IOSF_EP, REF_DWORD22, 0x19080000);

	/* LRC clock to 19.2MHz */
	gunit_iosf_write32(DPLL_IOSF_EP, DPLL_LRC_CLK, 0x00000F10);

	/* Disable periodic GRC IREF update for DPLL */
	tmp = gunit_iosf_read32(DPLL_IOSF_EP, PLLB_DWORD8);
	gunit_iosf_write32(DPLL_IOSF_EP, PLLB_DWORD8, tmp & 0x00FFFFFF);

	/* Enable Tx for periodic GRC update*/
	gunit_iosf_write32(DPLL_IOSF_EP, DPLL_Tx_GRC, 0x00004000);

	/* GRC cal clock set to 19.2MHZ */
	gunit_iosf_write32(DPLL_IOSF_EP, REF_DWORD18, 0x30002400);

	/* Set lock time to 53us.
	 * Disable fast lock.
	 */
	gunit_iosf_write32(DPLL_IOSF_EP, CMN_DWORD8, 0x0);

	/* Stagger Programming */
	gunit_iosf_write32(DPLL_IOSF_EP, TX_GROUP_1, 0x00001500);
	gunit_iosf_write32(DPLL_IOSF_EP, TX_GROUP_2, 0x40400000);
	gunit_iosf_write32(DPLL_IOSF_EP, PCS_DWORD12_1, 0x00220F00);
	gunit_iosf_write32(DPLL_IOSF_EP, PCS_DWORD12_2, 0x00750F00);

	/* Set divisors*/
	gunit_iosf_write32(DPLL_IOSF_EP, PLLA_DWORD3_1, div);
	gunit_iosf_write32(DPLL_IOSF_EP, PLLA_DWORD3_2, div);

	/* Set up LCPLL in digital mode */
	gunit_iosf_write32(DPLL_IOSF_EP, PLLA_DWORD5_1, 0x0DF44300);
	gunit_iosf_write32(DPLL_IOSF_EP, PLLA_DWORD5_2, 0x0DF44300);

	/* LPF co-efficients for LCPLL in digital mode */
	gunit_iosf_write32(DPLL_IOSF_EP, PLLB_DWORD10_1, 0x005F0021);
	gunit_iosf_write32(DPLL_IOSF_EP, PLLB_DWORD10_2, 0x005F0021);

	/* Disable unused TLine clocks on right side */
	gunit_iosf_write32(DPLL_IOSF_EP, CMN_DWORD3, 0x14540000);

	/* Enable DPLL */
	tmp = REG_READ(DISP_PLL_CTRL);
	REG_WRITE(DISP_PLL_CTRL, tmp | DPLL_EN);

	/* Enable DCLP to core */
	tmp = gunit_iosf_read32(DPLL_IOSF_EP, PLLA_DWORD7_1);
	gunit_iosf_write32(DPLL_IOSF_EP, PLLA_DWORD7_1, tmp | (1 << 24));
	tmp = gunit_iosf_read32(DPLL_IOSF_EP, PLLA_DWORD7_2);
	gunit_iosf_write32(DPLL_IOSF_EP, PLLA_DWORD7_2, tmp | (1 << 24));

	/* Set HDMI lane CML clock */
	gunit_iosf_write32(DPLL_IOSF_EP, DPLL_CML_CLK1, 0x07760018);
	gunit_iosf_write32(DPLL_IOSF_EP, DPLL_CML_CLK2, 0x00400888);

	/* Swing settings */
	gunit_iosf_write32(DPLL_IOSF_EP, TX_SWINGS_1, 0x00000000);
	gunit_iosf_write32(DPLL_IOSF_EP, TX_SWINGS_2, 0x2B245555);
	gunit_iosf_write32(DPLL_IOSF_EP, TX_SWINGS_3, 0x5578B83A);
	gunit_iosf_write32(DPLL_IOSF_EP, TX_SWINGS_4, 0x0C782040);
	/*gunit_iosf_write32(DPLL_IOSF_EP, TX_SWINGS_5, 0x2B247878);*/
	gunit_iosf_write32(DPLL_IOSF_EP, TX_SWINGS_6, 0x00030000);
	gunit_iosf_write32(DPLL_IOSF_EP, TX_SWINGS_7, 0x00004000);
	gunit_iosf_write32(DPLL_IOSF_EP, TX_SWINGS_1, 0x80000000);

	/* Wait until DPLL is locked */
	ret = REG_READ(DISP_PLL_CTRL);
	ret &= 0x8000;
	while ((retry++ < 1000) && (ret != 0x8000)) {
		usleep_range(500, 1000);
		ret = REG_READ(DISP_PLL_CTRL);
		ret &= 0x8000;
	}

	if (ret != 0x8000)
		return -EIO;
	return 0;
}

static int __compute_check_sum(struct avi_info_packet *packet)
{
	uint8_t i = 0;
	uint8_t sum = 0;

	for (i = 0; i < 3; i++)
		sum += packet->header[i];
	for (i = 1; i < 28; i++)
		sum += packet->data[i];

	packet->data[0] = (uint8_t)(0xFF - sum + 1);

	return (int)packet->data[0];
}

static int hdmi_build_avi_packet(struct hdmi_monitor *monitor,
					struct drm_mode_modeinfo *mode)
{
	struct avi_info_packet *avi_pkt;
	u8 par = 0;
	unsigned int pf;
	bool p;
	int i = 0;

	if (!mode)
		return -EINVAL;

	avi_pkt = &monitor->avi_packet;

	/* Set header to AVI */
	avi_pkt->header[0] = 0x82;
	avi_pkt->header[1] = 0x02;
	avi_pkt->header[2] = 0x0D;
	/* Clear payload section */
	memset(avi_pkt->data, 0, sizeof(avi_pkt->data));

	/* RGB, Active Format Info valid, no bars */
	/* use underscan as HDMI video is composed with all
	 * active pixels and lines with or without border
	 */
	avi_pkt->data[1] = 0x12;
	/* Set color component sample format */
	pf = 0; /* 0: RGB444 1:YUV422 2:YUV444 */
	avi_pkt->data[1] |= pf << 5;
	/* Colorimetry */
	avi_pkt->data[2] = 0;

	/* Fill PAR for all supported modes
	 * This is required for passing compliance tests
	 */

	if (mode->flags & DRM_MODE_FLAG_PAR16_9)
		par = 2;
	else if (mode->flags & DRM_MODE_FLAG_PAR4_3)
		par = 1;

	avi_pkt->data[2] |= par << 4;
	/* Fill FAR */
	avi_pkt->data[2] |= 8; /* same as par */

	/* Fill extended colorimetry */
	avi_pkt->data[3] = 0;

	/* Fill quantization range */
	if (monitor->quant_range_selectable)
		avi_pkt->data[3] |= 0x01<<2;

	/* Only support RGB output, 640x480: full range Q0=1, Q1=0
	* other timing: limited range Q0=0, Q1=1 */
	avi_pkt->data[3] &= ~0x0c;
	if (mode->hdisplay == 640 && mode->vdisplay == 480)
		avi_pkt->data[3] |= 0x02 << 2;
	else
		avi_pkt->data[3] |= 0x01 << 2;

	/* Fill Video Identification Code [adjust VIC according to PAR] */
	hdmi_avi_infoframe_from_mode(monitor, mode);
	avi_pkt->data[4] = monitor->video_code;

	/* Fill pixel repetition value: 2x for 480i and 546i */
	p = ((mode->flags & DRM_MODE_FLAG_INTERLACE) == 0);
	avi_pkt->data[5] = ((mode->hdisplay == 720) && !p) ? 0x01 : 0x00;

	/* Compute and fill checksum */
	avi_pkt->data[0] = __compute_check_sum(avi_pkt);
	pr_debug("Dump avi_pkt");
	pr_debug("%x,%x,%x\n", avi_pkt->header[0], avi_pkt->header[1],
		avi_pkt->header[2]);
	for (i = 0; i < 28; i++)
		pr_debug("%x ", avi_pkt->data[i]);
	pr_debug("\n");

	return 0;
}

void hdmi_disable_avi_infoframe(void)
{
	uint32_t vid_dip_ctl = 0;
	uint32_t dip_type = 0;
	uint32_t index = 0;

	dip_type = DIP_TYPE_AVI;
	index = DIP_BUFFER_INDEX_AVI;

	/* Disable DIP type & set the buffer index & reset access address */
	vid_dip_ctl = REG_READ(VIDEO_DIP_CTRL);
	vid_dip_ctl &= ~(dip_type |
			DIP_BUFFER_INDEX_MASK |
			DIP_ADDR_MASK |
			DIP_TX_FREQ_MASK);
	vid_dip_ctl |= (index |
			PORT_B_SELECT |
			EN_DIP);
	REG_WRITE(VIDEO_DIP_CTRL, vid_dip_ctl);
}

int  hdmi_enable_avi_infoframe(struct avi_info_packet *pkt)
{
	int res = 0;
	uint32_t vid_dip_ctl = 0;
	uint32_t index = 0;
	uint32_t dip_type = 0;
	uint32_t dip_data = 0;

	if (!pkt)
		return -EINVAL;

	hdmi_disable_avi_infoframe();

	/* Add delay for any Pending transmissions ~ 2 VSync + 3 HSync */
	msleep_interruptible(32 + 8);

	dip_type = DIP_TYPE_AVI;
	index = DIP_BUFFER_INDEX_AVI;


	/* Disable DIP type & set the buffer index & reset access address */
	vid_dip_ctl = REG_READ(VIDEO_DIP_CTRL);
	vid_dip_ctl &= ~(dip_type |
			DIP_BUFFER_INDEX_MASK |
			DIP_ADDR_MASK |
			DIP_TX_FREQ_MASK);
	vid_dip_ctl |= (index |
			PORT_B_SELECT |
			EN_DIP);
	REG_WRITE(VIDEO_DIP_CTRL, vid_dip_ctl);

	/* Write Packet Data */
	dip_data = 0;
	dip_data = (pkt->header[0] << 0) |
		   (pkt->header[1] << 8) |
		   (pkt->header[2] << 16);
	REG_WRITE(VIDEO_DIP_DATA, dip_data);

	for (index = 0; index < (HDMI_DIP_PACKET_DATA_LEN / 4); index++) {
		dip_data = pkt->data32[index];
		REG_WRITE(VIDEO_DIP_DATA, dip_data);
	}

	/* Enable Packet Type & Transmission Frequency */
	vid_dip_ctl = REG_READ(VIDEO_DIP_CTRL);
	vid_dip_ctl |= (PORT_B_SELECT | EN_DIP);
	vid_dip_ctl |= (dip_type | DIP_TX_FREQ_EVERY);
	pr_debug("vid_dip_ctl %x\n", vid_dip_ctl);
	REG_WRITE(VIDEO_DIP_CTRL, vid_dip_ctl);

	return res;
}

static int hdmi_modeset(struct intel_pipe *pipe,
		struct drm_mode_modeinfo *mode)
{
	__u16 vblank_start;
	__u16 vblank_end;
	__u16 hblank_start;
	__u16 hblank_end;
	u32 m1 = 0, m2 = 0, n = 0, p1 = 0, p2 = 0;
	int err = 0;

	struct hdmi_pipe *hdmi_pipe = to_hdmi_pipe(pipe);
	struct hdmi_hw_context *ctx = &hdmi_pipe->ctx;

	if (!mode) {
		pr_err("%s: invalid mode\n", __func__);
		err = -EINVAL;
		goto out_err;
	}

	pr_debug("%s\n", __func__);

	/* TODO: calculate divisor here!! */
	if (mode->clock == 148500) {
		m1 = 2;
		m2 = 116;
		n = 1;
		p1 = 3;
		p2 = 2;
	} else if (mode->clock == 74250) {
		m1 = 2;
		m2 = 145;
		n = 1;
		p1 = 3;
		p2 = 5;
	}

	ctx->div = (0x11 << 24) | (p1 << 21) | (p2 << 16) | (n << 12) |
		  (0x1 << 11)  | (m1 << 8)  | (m2);


	/* TODO: setup audio clock */
	hdmi_pipe->audio.tmds_clock = mode->clock;

	vblank_start = min(mode->vsync_start, mode->vdisplay);
	vblank_end = max(mode->vsync_end, mode->vtotal);
	hblank_start = min(mode->hsync_start, mode->hdisplay);
	hblank_end = max(mode->hsync_end, mode->htotal);

	ctx->htotal =
		(mode->hdisplay - 1) | ((mode->htotal - 1) << HORZ_TOTAL_SHIFT);
	ctx->hblank =
		(hblank_start - 1) | ((hblank_end - 1) << HORZ_BLANK_END_SHIFT);
	ctx->hsync =
		(mode->hsync_start - 1) |
		((mode->hsync_end - 1) << HORZ_SYNC_END_SHIFT);
	ctx->vtotal =
		(mode->vdisplay - 1) | ((mode->vtotal - 1) << VERT_TOTAL_SHIFT);
	ctx->vblank =
		(vblank_start - 1) | ((vblank_end - 1) << VERT_BLANK_END_SHIFT);
	ctx->vsync =
		(mode->vsync_start - 1) |
		((mode->vsync_end - 1) << VERT_SYNC_END_SHIFT);

	ctx->pipesrc =
		((mode->hdisplay - 1) << HORZ_SRC_SIZE_SHIFT) |
							(mode->vdisplay - 1);
	ctx->pipeconf = PIPEACONF_ENABLE;

	ctx->hdmib = NULL_PACKET_EN | AUDIO_ENABLE | HDMI_PORT_EN;

	/* FIXME: To make the aligned value DC independant. */
	ctx->dspstride = ALIGN(mode->hdisplay * 4, 32);
	ctx->dspsurf = 0;
	ctx->dsplinoff = 0;
	ctx->dspcntr = SRC_PIX_FMT_BGRX8888;
	ctx->dsppos = 0;
	ctx->dspsize = (mode->vdisplay-1) << HEIGHT_SHIFT | (mode->hdisplay-1);

	ctx->pipestat = VERTICAL_SYNC_STATUS |
		HDMI_AUDIO_UNDERRUN_STATUS |
		HDMI_AUDIO_BUF_DONE_STATUS |
		VERTICAL_SYNC_ENABLE |
		HDMI_AUDIO_UNDERRUN_ENABLE |
		HDMI_AUDIO_BUF_DONE_ENABLE;

	hdmi_build_avi_packet(&hdmi_pipe->monitor, mode);
	return 0;
out_err:
	return err;
}

static int hdmi_power_on(struct hdmi_pipe *pipe)
{
	int err = 0;
	u32 power_island = 0;
	struct hdmi_hw_context *ctx;
	if (!pipe) {
		pr_err("%s: invalid mode\n", __func__);
		err = -EINVAL;
		goto out_err;
	}
	ctx = &pipe->ctx;
	power_island = pipe_to_island(pipe->base.base.idx);
	if (power_island & OSPM_DISPLAY_B)
		power_island |= OSPM_DISPLAY_HDMI;

	if (!power_island_get(power_island)) {
		err = -EIO;
		goto out_err;
	}

	err = hdmi_setup_clock(pipe->ctx.div);
	if (err)
		goto out_err;

	REG_WRITE(HTOTAL_B, ctx->htotal);
	REG_WRITE(HBLANK_B, ctx->hblank);
	REG_WRITE(HSYNC_B, ctx->hsync);
	REG_WRITE(VTOTAL_B, ctx->vtotal);
	REG_WRITE(VBLANK_B, ctx->vblank);
	REG_WRITE(VSYNC_B, ctx->vsync);

	REG_WRITE(SRCSZ_B, ctx->pipesrc);
	REG_WRITE(PIPEBCONF, ctx->pipeconf);

	REG_WRITE(HDMIB, ctx->hdmib);

	REG_WRITE(DSPBSTRIDE, ctx->dspstride);
	REG_WRITE(DSPBLINOFF, ctx->dsplinoff);
	REG_WRITE(DSPBCNTR, ctx->dspcntr);
	REG_WRITE(DSPBPOS, ctx->dsppos);
	REG_WRITE(DSPBSIZE, ctx->dspsize);
	REG_WRITE(DSPBSURF, ctx->dspsurf);

	REG_WRITE(PIPEBSTAT, ctx->pipestat);
	REG_WRITE(IMR, REG_READ(IMR) & ~IIR_PIPEB_EVENT);
	REG_WRITE(IER, REG_READ(IER) | IIR_PIPEB_EVENT);

	if (pipe->monitor.is_hdmi)
		hdmi_enable_avi_infoframe(&pipe->monitor.avi_packet);
out_err:
	return err;
}

static int hdmi_dpms(struct intel_pipe *pipe, u8 state)
{
	struct hdmi_pipe *hdmi_pipe = to_hdmi_pipe(pipe);
	int res = 0;

	switch (state) {
	case DRM_MODE_DPMS_ON:
		res = hdmi_power_on(hdmi_pipe);
		hdmi_hdcp_enable(hdmi_pipe);
		break;
	case DRM_MODE_DPMS_OFF:
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	default:
		pr_err("%s: unsupported dpms mode\n", __func__);
		return -EOPNOTSUPP;
	}
	return res;
}

static bool hdmi_is_screen_connected(struct intel_pipe *pipe)
{
	struct hdmi_pipe *hdmi_pipe = to_hdmi_pipe(pipe);
	if (atomic_read(&hdmi_pipe->hpd_ctx.is_connected))
		return true;
	else
		return false;
}

static void hdmi_get_modelist(struct intel_pipe *pipe,
		struct drm_mode_modeinfo **modelist, size_t *n_modes)
{
	/*TODO:  populate all the modes*/
	struct hdmi_pipe *hdmi_pipe = to_hdmi_pipe(pipe);

	if (atomic_read(&hdmi_pipe->hpd_ctx.is_connected)) {
		hdmi_pipe->monitor.raw_edid = get_edid(hdmi_pipe->adapter);
		if (hdmi_pipe->monitor.raw_edid) {
			hdmi_pipe->monitor.is_hdmi =
				detect_hdmi_monitor(
						hdmi_pipe->monitor.raw_edid);
				parse_edid(&hdmi_pipe->monitor,
					  hdmi_pipe->monitor.raw_edid);
				populate_modes(hdmi_pipe);
		} else {
			pr_debug("TV is connected but no edid!!\n");
			hdmi_pipe->monitor.preferred_mode = &fake_mode;
			hdmi_pipe->monitor.screen_height_mm = 300;
			hdmi_pipe->monitor.screen_width_mm = 500;
		}
	}
	*modelist = hdmi_pipe->monitor.preferred_mode;
	*n_modes = 1;
}

static void hdmi_get_preferred_mode(struct intel_pipe *pipe,
		struct drm_mode_modeinfo **mode)
{
	struct hdmi_pipe *hdmi_pipe = to_hdmi_pipe(pipe);
	*mode = hdmi_pipe->monitor.preferred_mode;
}

static void hdmi_pipe_hw_deinit(struct intel_pipe *pipe)
{
	struct hdmi_pipe *hdmi_pipe = to_hdmi_pipe(pipe);
	struct hdmi_monitor *monitor;
	struct hdmi_mode_info *mode_info, *t;
	monitor = &hdmi_pipe->monitor;
	/* delete all modes from list*/
	list_for_each_entry_safe(mode_info, t,
				&monitor->probedModes,
				head) {
		list_del(&mode_info->head);
		kfree(mode_info);
	}
	monitor->preferred_mode = NULL;
	monitor->raw_edid = NULL;
	return;
}

static int hdmi_pipe_hw_init(struct intel_pipe *pipe)
{
	struct hdmi_pipe *hdmi_pipe = to_hdmi_pipe(pipe);

	hdmi_check_connection_status(hdmi_pipe);

	if (atomic_read(&hdmi_pipe->hpd_ctx.is_connected)) {
		hdmi_pipe->monitor.raw_edid = get_edid(hdmi_pipe->adapter);
		if (hdmi_pipe->monitor.raw_edid) {
			hdmi_pipe->monitor.is_hdmi =
				detect_hdmi_monitor(
						hdmi_pipe->monitor.raw_edid);
			parse_edid(&hdmi_pipe->monitor,
				  hdmi_pipe->monitor.raw_edid);
			populate_modes(hdmi_pipe);
		} else {
			pr_debug("TV is connected but no edid!!\n");
			hdmi_pipe->monitor.is_hdmi = true;
			hdmi_pipe->monitor.preferred_mode = &fake_mode;
			hdmi_pipe->monitor.screen_height_mm = 300;
			hdmi_pipe->monitor.screen_width_mm = 500;
		}
	}
	return 0;
}

static u32 hdmi_get_supported_events(struct intel_pipe *pipe)
{
	return INTEL_PIPE_EVENT_VSYNC |
			INTEL_PIPE_EVENT_HOTPLUG_CONNECTED |
			INTEL_PIPE_EVENT_HOTPLUG_DISCONNECTED |
			INTEL_PIPE_EVENT_AUDIO_BUFFERDONE |
			INTEL_PIPE_EVENT_AUDIO_UNDERRUN;
}

static int hdmi_set_event(struct intel_pipe *pipe, u8 event, bool enabled)
{
	return -EOPNOTSUPP;
}

static void hdmi_handle_events(struct intel_pipe *pipe, u32 events)
{
	struct hdmi_pipe *hdmi_pipe = to_hdmi_pipe(pipe);
	pr_debug("%s: events=%d\n", __func__, events);
	if (events & INTEL_PIPE_EVENT_AUDIO_BUFFERDONE)
		schedule_work(&hdmi_pipe->audio.hdmi_bufferdone_work);

	if (events & INTEL_PIPE_EVENT_AUDIO_UNDERRUN)
		schedule_work(&hdmi_pipe->audio.hdmi_underrun_work);
}
static void hdmi_get_events(struct intel_pipe *pipe, u32 *events)
{
	struct hdmi_pipe *hdmi_pipe = to_hdmi_pipe(pipe);
	u8 idx = pipe->base.idx;
	u32 dc_events = REG_READ(IIR);
	u32 event_bit;

	*events = 0;

	switch (idx) {
	case 1:
		event_bit = IIR_PIPEB_EVENT;
		break;
	default:
		pr_err("%s: invalid pipe index %d\n", __func__, idx);
		return;
	}

	/* first check if it's hotplug interrupt */
	if (atomic_read(&hdmi_pipe->hpd_ctx.is_asserted)) {
		if (atomic_read(&hdmi_pipe->hpd_ctx.is_connected)) {
			*events |= INTEL_PIPE_EVENT_HOTPLUG_CONNECTED;
			pr_debug("%s: HOTPLUG_CONNECTED\n", __func__);
		} else {
			*events |= INTEL_PIPE_EVENT_HOTPLUG_DISCONNECTED;
			pr_debug("In %s disconnected!!\n", __func__);
		}
		/* clear hpd assertion */
		atomic_set(&hdmi_pipe->hpd_ctx.is_asserted, 0);
	}

	if (!(dc_events & event_bit))
		return;

	/* Clear the 1st level interrupt. */
	REG_WRITE(IIR, dc_events & IIR_PIPEB_EVENT);

	if (VBLANK_STATUS & REG_READ(PIPEBSTAT))
		*events = INTEL_PIPE_EVENT_VSYNC;

	if (HDMI_AUDIO_UNDERRUN_STATUS & REG_READ(PIPEBSTAT)) {
		*events |= INTEL_PIPE_EVENT_AUDIO_UNDERRUN;
		pr_debug("%s: Underrun\n", __func__);
	}

	if (HDMI_AUDIO_BUF_DONE_STATUS & REG_READ(PIPEBSTAT)) {
		*events |= INTEL_PIPE_EVENT_AUDIO_BUFFERDONE;
		pr_debug("%s: Bufferdone\n", __func__);
	}

	REG_WRITE(PIPEBSTAT, REG_READ(PIPEBSTAT));

	/**
	 * FIXME: should use hardware vsync counter.
	 */
	if (*events & INTEL_PIPE_EVENT_VSYNC) {
		if (++vsync_counter > VSYNC_COUNT_MAX_MASK)
			vsync_counter = 0;
	}
}

u32 hdmi_get_vsync_counter(struct intel_pipe *pipe, u32 interval)
{

	u32 count;
	u32 max_count_mask = VSYNC_COUNT_MAX_MASK;

	/* NOTE: PIPEFRAMEHIGH & PIPEFRAMEPIXEL regs are RESERVED in ANN DC. */
#if 0
	u32 count, high1, high2, low;
	u32 max_count_mask = (PIPE_FRAME_HIGH_MASK | PIPE_FRAME_LOW_MASK);

	if (!(PIPEACONF_ENABLE & REG_READ(PIPEBCONF))) {
		pr_err("%s: pipe was disabled\n", __func__);
		return 0;
	}

	/*
	 * High & low register fields aren't synchronized, so make sure
	 * we get a low value that's stable across two reads of the high
	 * register.
	 */
	do {
		high1 = ((REG_READ(PIPEBFRAMEHIGH) &
			PIPE_FRAME_HIGH_MASK) >> PIPE_FRAME_HIGH_SHIFT);
		low =  ((REG_READ(PIPEBFRAMEPIXEL) &
			PIPE_FRAME_LOW_MASK) >> PIPE_FRAME_LOW_SHIFT);
		high2 = ((REG_READ(PIPEBFRAMEHIGH) &
			PIPE_FRAME_HIGH_MASK) >>  PIPE_FRAME_HIGH_SHIFT);
	} while (high1 != high2);

	count = (high1 << 8) | low;
	count |= (~max_count_mask);
	count += interval;
	count &= max_count_mask;
#endif

	count = vsync_counter;
	count |= (~max_count_mask);
	count += interval;
	count &= max_count_mask;

	pr_debug("%s: count = %#x\n", __func__, count);

	return count;
}


static struct intel_pipe_ops hdmi_base_ops = {
	.hw_init = hdmi_pipe_hw_init,
	.hw_deinit = hdmi_pipe_hw_deinit,
	.get_preferred_mode = hdmi_get_preferred_mode,
	.get_modelist = hdmi_get_modelist,
	.dpms = hdmi_dpms,
	.modeset = hdmi_modeset,
	.get_screen_size = hdmi_get_screen_size,
	.is_screen_connected = hdmi_is_screen_connected,
	.get_supported_events = hdmi_get_supported_events,
	.set_event = hdmi_set_event,
	.get_events = hdmi_get_events,
	.get_vsync_counter = hdmi_get_vsync_counter,
	.handle_events = hdmi_handle_events,
};

void hdmi_pipe_destroy(struct hdmi_pipe *pipe)
{
	return;
}

int hdmi_pipe_init(struct hdmi_pipe *pipe, struct device *dev,
	struct intel_plane *primary_plane, u8 idx)
{
	int err;

	if (!pipe) {
		pr_err("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	/* get platform dependent configs */
	err = mofd_get_platform_configs(&pipe->config);
	if (err) {
		pr_err("%s: failed to init HDMI hw\n",
			__func__);
		goto out_err0;
	}

	pipe->adapter = i2c_get_adapter(HDMI_I2C_ADAPTER_NUM);
	if (!pipe->adapter) {
		pr_err("Unable to get i2c adapter for HDMI");
		err = -EIO;
		goto out_err0;
	}

	/* necessary init work */
	INIT_LIST_HEAD(&pipe->monitor.probedModes);

	/*
		TODO put this thing to a more elegant place
	*/
	mofd_hdmi_enable_hpd(true);

	/* init hotplug ctx */
	atomic_set(&pipe->hpd_ctx.is_connected, 0);
	atomic_set(&pipe->hpd_ctx.is_asserted, 0);

	/* init hpd driver*/
	pipe->hdmi_hpd_driver.name = HDMI_HPD_DRIVER_NAME;
	pipe->hdmi_hpd_driver.probe = hdmi_hpd_probe;
	pipe->id_table[0].vendor = 0x8086;
	pipe->id_table[0].device = pipe->config.pci_device_id;
	pipe->id_table[0].driver_data = (unsigned long)pipe;
	pipe->hdmi_hpd_driver.id_table = pipe->id_table;

	pipe->hotplug_data = pipe;
	pipe->hotplug_irq_cb = hotplug_thread_fn;

	/*
		create workqueue for register hotplug device
		1, Because we need it to schedue ASAP, so put a WQ_HIGHPRI
		2, Max_active assigned as 1 because there will be only one
		    work
		3, We don't want concurrent execute so put a WQ_NON_REENTRANT
	*/
	pipe->hotplug_register_wq = alloc_workqueue("hotplug_device_register",
				WQ_UNBOUND | WQ_NON_REENTRANT | WQ_HIGHPRI, 1);
	if (!pipe->hotplug_register_wq) {
		pr_err("failed to create hotplug device register workqueue\n");
		err = -ENOMEM;
		goto out_err0;
	}

	INIT_WORK(&pipe->hotplug_register_work, adf_hdmi_hpd_init_work);
	schedule_work(&pipe->hotplug_register_work);

	/*HDCP initialized, just allocate resources, no function working*/
	hdmi_hdcp_init(pipe);

	/* TODO: put it to a more elegant place */
	adf_hdmi_audio_init(pipe);

	return intel_pipe_init(&pipe->base, dev, idx, false, INTEL_PIPE_HDMI,
		primary_plane, &hdmi_base_ops, "hdmi_pipe");

out_err0:
	return err;
}
