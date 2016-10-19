/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/io.h>
#include <linux/delay.h>

#include "mdss_dp_util.h"

#define HEADER_BYTE_2_BIT	 0
#define PARITY_BYTE_2_BIT	 8
#define HEADER_BYTE_1_BIT	16
#define PARITY_BYTE_1_BIT	24
#define HEADER_BYTE_3_BIT	16
#define PARITY_BYTE_3_BIT	24
#define DP_LS_FREQ_162		162000000
#define DP_LS_FREQ_270		270000000
#define DP_LS_FREQ_540		540000000

enum mdss_dp_pin_assignment {
	PIN_ASSIGNMENT_A,
	PIN_ASSIGNMENT_B,
	PIN_ASSIGNMENT_C,
	PIN_ASSIGNMENT_D,
	PIN_ASSIGNMENT_E,
	PIN_ASSIGNMENT_F,
	PIN_ASSIGNMENT_MAX,
};

static const char *mdss_dp_pin_name(u8 pin)
{
	switch (pin) {
	case PIN_ASSIGNMENT_A: return "PIN_ASSIGNMENT_A";
	case PIN_ASSIGNMENT_B: return "PIN_ASSIGNMENT_B";
	case PIN_ASSIGNMENT_C: return "PIN_ASSIGNMENT_C";
	case PIN_ASSIGNMENT_D: return "PIN_ASSIGNMENT_D";
	case PIN_ASSIGNMENT_E: return "PIN_ASSIGNMENT_E";
	case PIN_ASSIGNMENT_F: return "PIN_ASSIGNMENT_F";
	default: return "UNKNOWN";
	}
}

struct mdss_hw mdss_dp_hw = {
	.hw_ndx = MDSS_HW_EDP,
	.ptr = NULL,
	.irq_handler = dp_isr,
};

/* DP retrieve ctrl HW version */
u32 mdss_dp_get_ctrl_hw_version(struct dss_io_data *ctrl_io)
{
	return readl_relaxed(ctrl_io->base + DP_HW_VERSION);
}

/* DP retrieve phy HW version */
u32 mdss_dp_get_phy_hw_version(struct dss_io_data *phy_io)
{
	return readl_relaxed(phy_io->base + DP_PHY_REVISION_ID3);
}
/* DP PHY SW reset */
void mdss_dp_phy_reset(struct dss_io_data *ctrl_io)
{
	writel_relaxed(0x04, ctrl_io->base + DP_PHY_CTRL); /* bit 2 */
	udelay(1000);
	writel_relaxed(0x00, ctrl_io->base + DP_PHY_CTRL);
}

void mdss_dp_switch_usb3_phy_to_dp_mode(struct dss_io_data *tcsr_reg_io)
{
	writel_relaxed(0x01, tcsr_reg_io->base + TCSR_USB3_DP_PHYMODE);
}

/* DP PHY assert reset for PHY and PLL */
void mdss_dp_assert_phy_reset(struct dss_io_data *ctrl_io, bool assert)
{
	if (assert) {
		/* assert reset line for PHY and PLL */
		writel_relaxed(0x5,
			ctrl_io->base + DP_PHY_CTRL); /* bit 0 & 2 */
	} else {
		/* remove assert for PLL and PHY reset line */
		writel_relaxed(0x00, ctrl_io->base + DP_PHY_CTRL);
	}
}

/* reset AUX */
void mdss_dp_aux_reset(struct dss_io_data *ctrl_io)
{
	u32 aux_ctrl = readl_relaxed(ctrl_io->base + DP_AUX_CTRL);

	aux_ctrl |= BIT(1);
	writel_relaxed(aux_ctrl, ctrl_io->base + DP_AUX_CTRL);
	udelay(1000);
	aux_ctrl &= ~BIT(1);
	writel_relaxed(aux_ctrl, ctrl_io->base + DP_AUX_CTRL);
}

/* reset DP controller */
void mdss_dp_ctrl_reset(struct dss_io_data *ctrl_io)
{
	u32 sw_reset = readl_relaxed(ctrl_io->base + DP_SW_RESET);

	sw_reset |= BIT(0);
	writel_relaxed(sw_reset, ctrl_io->base + DP_SW_RESET);
	udelay(1000);
	sw_reset &= ~BIT(0);
	writel_relaxed(sw_reset, ctrl_io->base + DP_SW_RESET);
}

/* reset DP Mainlink */
void mdss_dp_mainlink_reset(struct dss_io_data *ctrl_io)
{
	u32 mainlink_ctrl = readl_relaxed(ctrl_io->base + DP_MAINLINK_CTRL);

	mainlink_ctrl |= BIT(1);
	writel_relaxed(mainlink_ctrl, ctrl_io->base + DP_MAINLINK_CTRL);
	udelay(1000);
	mainlink_ctrl &= ~BIT(1);
	writel_relaxed(mainlink_ctrl, ctrl_io->base + DP_MAINLINK_CTRL);
}

/* Configure HPD */
void mdss_dp_hpd_configure(struct dss_io_data *ctrl_io, bool enable)
{
	if (enable) {
		u32 reftimer =
			readl_relaxed(ctrl_io->base + DP_DP_HPD_REFTIMER);

		writel_relaxed(0xf, ctrl_io->base + DP_DP_HPD_INT_ACK);
		writel_relaxed(0xf, ctrl_io->base + DP_DP_HPD_INT_MASK);

		/* Enabling REFTIMER */
		reftimer |= BIT(16);
		writel_relaxed(0xf, ctrl_io->base + DP_DP_HPD_REFTIMER);
		/* Enable HPD */
		writel_relaxed(0x1, ctrl_io->base + DP_DP_HPD_CTRL);
	} else {
		/*Disable HPD */
		writel_relaxed(0x0, ctrl_io->base + DP_DP_HPD_CTRL);
	}
}

/* Enable/Disable AUX controller */
void mdss_dp_aux_ctrl(struct dss_io_data *ctrl_io, bool enable)
{
	u32 aux_ctrl = readl_relaxed(ctrl_io->base + DP_AUX_CTRL);

	if (enable)
		aux_ctrl |= BIT(0);
	else
		aux_ctrl &= ~BIT(0);

	writel_relaxed(aux_ctrl, ctrl_io->base + DP_AUX_CTRL);
}

/* DP Mainlink controller*/
void mdss_dp_mainlink_ctrl(struct dss_io_data *ctrl_io, bool enable)
{
	u32 mainlink_ctrl = readl_relaxed(ctrl_io->base + DP_MAINLINK_CTRL);

	if (enable)
		mainlink_ctrl |= BIT(0);
	else
		mainlink_ctrl &= ~BIT(0);

	writel_relaxed(mainlink_ctrl, ctrl_io->base + DP_MAINLINK_CTRL);
}

int mdss_dp_mainlink_ready(struct mdss_dp_drv_pdata *dp, u32 which)
{
	u32 data;
	int cnt = 10;

	while (--cnt) {
		/* DP_MAINLINK_READY */
		data = readl_relaxed(dp->base + DP_MAINLINK_READY);
		if (data & which) {
			pr_debug("which=%x ready\n", which);
			return 1;
		}
		udelay(1000);
	}
	pr_err("which=%x NOT ready\n", which);

	return 0;
}

/* DP Configuration controller*/
void mdss_dp_configuration_ctrl(struct dss_io_data *ctrl_io, u32 data)
{
	writel_relaxed(data, ctrl_io->base + DP_CONFIGURATION_CTRL);
}

/* DP state controller*/
void mdss_dp_state_ctrl(struct dss_io_data *ctrl_io, u32 data)
{
	writel_relaxed(data, ctrl_io->base + DP_STATE_CTRL);
}

void mdss_dp_timing_cfg(struct dss_io_data *ctrl_io,
					struct mdss_panel_info *pinfo)
{
	u32 total_ver, total_hor;
	u32 data;

	pr_debug("width=%d hporch= %d %d %d\n",
		pinfo->xres, pinfo->lcdc.h_back_porch,
		pinfo->lcdc.h_front_porch, pinfo->lcdc.h_pulse_width);

	pr_debug("height=%d vporch= %d %d %d\n",
		pinfo->yres, pinfo->lcdc.v_back_porch,
		pinfo->lcdc.v_front_porch, pinfo->lcdc.v_pulse_width);

	total_hor = pinfo->xres + pinfo->lcdc.h_back_porch +
		pinfo->lcdc.h_front_porch + pinfo->lcdc.h_pulse_width;

	total_ver = pinfo->yres + pinfo->lcdc.v_back_porch +
			pinfo->lcdc.v_front_porch + pinfo->lcdc.v_pulse_width;

	data = total_ver;
	data <<= 16;
	data |= total_hor;
	/* DP_TOTAL_HOR_VER */
	writel_relaxed(data, ctrl_io->base + DP_TOTAL_HOR_VER);

	data = (pinfo->lcdc.v_back_porch + pinfo->lcdc.v_pulse_width);
	data <<= 16;
	data |= (pinfo->lcdc.h_back_porch + pinfo->lcdc.h_pulse_width);
	/* DP_START_HOR_VER_FROM_SYNC */
	writel_relaxed(data, ctrl_io->base + DP_START_HOR_VER_FROM_SYNC);

	data = pinfo->lcdc.v_pulse_width;
	data <<= 16;
	data |= pinfo->lcdc.h_pulse_width;
	/* DP_HSYNC_VSYNC_WIDTH_POLARITY */
	writel_relaxed(data, ctrl_io->base + DP_HSYNC_VSYNC_WIDTH_POLARITY);

	data = pinfo->yres;
	data <<= 16;
	data |= pinfo->xres;
	/* DP_ACTIVE_HOR_VER */
	writel_relaxed(data, ctrl_io->base + DP_ACTIVE_HOR_VER);
}

void mdss_dp_sw_mvid_nvid(struct dss_io_data *ctrl_io)
{
	writel_relaxed(0x37, ctrl_io->base + DP_SOFTWARE_MVID);
	writel_relaxed(0x3c, ctrl_io->base + DP_SOFTWARE_NVID);
}

void mdss_dp_setup_tr_unit(struct dss_io_data *ctrl_io, u8 link_rate,
				u8 ln_cnt, u32 res)
{
	u32 dp_tu = 0x0;
	u32 valid_boundary = 0x0;
	u32 valid_boundary2 = 0x0;
	struct dp_vc_tu_mapping_table const *tu_entry = tu_table;

	writel_relaxed(0x21, ctrl_io->base + DP_MISC1_MISC0);

	for (; tu_entry != tu_table + ARRAY_SIZE(tu_table); ++tu_entry) {
		if ((tu_entry->vic == res) &&
			(tu_entry->lanes == ln_cnt) &&
			(tu_entry->lrate == link_rate))
		break;
	}

	if (tu_entry == tu_table + ARRAY_SIZE(tu_table)) {
		pr_err("requested ln_cnt=%d, lrate=0x%x not supported\n",
				ln_cnt, link_rate);
		return;
	}

	dp_tu |= tu_entry->tu_size_minus1;
	valid_boundary |= tu_entry->valid_boundary_link;
	valid_boundary |= (tu_entry->delay_start_link << 16);

	valid_boundary2 |= (tu_entry->valid_lower_boundary_link << 1);
	valid_boundary2 |= (tu_entry->upper_boundary_count << 16);
	valid_boundary2 |= (tu_entry->lower_boundary_count << 20);

	if (tu_entry->boundary_moderation_en)
		valid_boundary2 |= BIT(0);

	writel_relaxed(valid_boundary, ctrl_io->base + DP_VALID_BOUNDARY);
	writel_relaxed(dp_tu, ctrl_io->base + DP_TU);
	writel_relaxed(valid_boundary2, ctrl_io->base + DP_VALID_BOUNDARY_2);

	pr_debug("valid_boundary=0x%x, valid_boundary2=0x%x\n",
				valid_boundary, valid_boundary2);
	pr_debug("dp_tu=0x%x\n", dp_tu);
}

void mdss_dp_ctrl_lane_mapping(struct dss_io_data *ctrl_io,
				struct lane_mapping l_map)
{
	u8 bits_per_lane = 2;
	u32 lane_map = ((l_map.lane0 << (bits_per_lane * 0))
			    | (l_map.lane1 << (bits_per_lane * 1))
			    | (l_map.lane2 << (bits_per_lane * 2))
			    | (l_map.lane3 << (bits_per_lane * 3)));
	pr_debug("%s: lane mapping reg = 0x%x\n", __func__, lane_map);
	writel_relaxed(lane_map,
		ctrl_io->base + DP_LOGICAL2PHYSCIAL_LANE_MAPPING);
}

void mdss_dp_phy_aux_setup(struct dss_io_data *phy_io)
{
	writel_relaxed(0x3d, phy_io->base + DP_PHY_PD_CTL);
	writel_relaxed(0x13, phy_io->base + DP_PHY_AUX_CFG1);
	writel_relaxed(0x10, phy_io->base + DP_PHY_AUX_CFG3);
	writel_relaxed(0x0a, phy_io->base + DP_PHY_AUX_CFG4);
	writel_relaxed(0x26, phy_io->base + DP_PHY_AUX_CFG5);
	writel_relaxed(0x0a, phy_io->base + DP_PHY_AUX_CFG6);
	writel_relaxed(0x03, phy_io->base + DP_PHY_AUX_CFG7);
	writel_relaxed(0x8b, phy_io->base + DP_PHY_AUX_CFG8);
	writel_relaxed(0x03, phy_io->base + DP_PHY_AUX_CFG9);
	writel_relaxed(0x1f, phy_io->base + DP_PHY_AUX_INTERRUPT_MASK);
}

int mdss_dp_irq_setup(struct mdss_dp_drv_pdata *dp_drv)
{
	int ret = 0;

	mdss_dp_hw.irq_info = mdss_intr_line();
	if (mdss_dp_hw.irq_info == NULL) {
		pr_err("Failed to get mdss irq information\n");
		return -ENODEV;
	}

	mdss_dp_hw.ptr = (void *)(dp_drv);

	ret = dp_drv->mdss_util->register_irq(&mdss_dp_hw);
	if (ret)
		pr_err("mdss_register_irq failed.\n");

	return ret;
}

void mdss_dp_irq_enable(struct mdss_dp_drv_pdata *dp_drv)
{
	unsigned long flags;

	spin_lock_irqsave(&dp_drv->lock, flags);
	writel_relaxed(dp_drv->mask1, dp_drv->base + DP_INTR_STATUS);
	writel_relaxed(dp_drv->mask2, dp_drv->base + DP_INTR_STATUS2);
	spin_unlock_irqrestore(&dp_drv->lock, flags);

	dp_drv->mdss_util->enable_irq(&mdss_dp_hw);
}

void mdss_dp_irq_disable(struct mdss_dp_drv_pdata *dp_drv)
{
	unsigned long flags;

	spin_lock_irqsave(&dp_drv->lock, flags);
	writel_relaxed(0x00, dp_drv->base + DP_INTR_STATUS);
	writel_relaxed(0x00, dp_drv->base + DP_INTR_STATUS2);
	spin_unlock_irqrestore(&dp_drv->lock, flags);

	dp_drv->mdss_util->disable_irq(&mdss_dp_hw);
}

static void mdss_dp_initialize_s_port(enum dp_port_cap *s_port, int port)
{
	switch (port) {
	case 0:
		*s_port = PORT_NONE;
		break;
	case 1:
		*s_port = PORT_UFP_D;
		break;
	case 2:
		*s_port = PORT_DFP_D;
		break;
	case 3:
		*s_port = PORT_D_UFP_D;
		break;
	default:
		*s_port = PORT_NONE;
	}
}

void mdss_dp_usbpd_ext_capabilities(struct usbpd_dp_capabilities *dp_cap)
{
	u32 buf = dp_cap->response;
	int port = buf & 0x3;

	dp_cap->receptacle_state =
			(buf & BIT(6)) ? true : false;

	dp_cap->dlink_pin_config =
			(buf >> 8) & 0xff;

	dp_cap->ulink_pin_config =
			(buf >> 16) & 0xff;

	mdss_dp_initialize_s_port(&dp_cap->s_port, port);
}

void mdss_dp_usbpd_ext_dp_status(struct usbpd_dp_status *dp_status)
{
	u32 buf = dp_status->response;
	int port = buf & 0x3;

	dp_status->low_pow_st =
			(buf & BIT(2)) ? true : false;

	dp_status->adaptor_dp_en =
			(buf & BIT(3)) ? true : false;

	dp_status->multi_func =
			(buf & BIT(4)) ? true : false;

	dp_status->switch_to_usb_config =
			(buf & BIT(5)) ? true : false;

	dp_status->exit_dp_mode =
			(buf & BIT(6)) ? true : false;

	dp_status->hpd_high =
			(buf & BIT(7)) ? true : false;

	dp_status->hpd_irq =
			(buf & BIT(8)) ? true : false;

	pr_debug("low_pow_st = %d, adaptor_dp_en = %d, multi_func = %d\n",
			dp_status->low_pow_st, dp_status->adaptor_dp_en,
			dp_status->multi_func);
	pr_debug("switch_to_usb_config = %d, exit_dp_mode = %d, hpd_high =%d\n",
			dp_status->switch_to_usb_config,
			dp_status->exit_dp_mode, dp_status->hpd_high);
	pr_debug("hpd_irq = %d\n", dp_status->hpd_irq);

	mdss_dp_initialize_s_port(&dp_status->c_port, port);
}

u32 mdss_dp_usbpd_gen_config_pkt(struct mdss_dp_drv_pdata *dp)
{
	u8 pin_cfg, pin;
	u32 config = 0;

	pin_cfg = dp->alt_mode.dp_cap.dlink_pin_config;

	for (pin = PIN_ASSIGNMENT_A; pin < PIN_ASSIGNMENT_MAX; pin++) {
		if (pin_cfg & BIT(pin))
			break;
	}

	if (pin == PIN_ASSIGNMENT_MAX)
		pin = PIN_ASSIGNMENT_C;

	pr_debug("pin assignment: %s\n", mdss_dp_pin_name(pin));

	config |= BIT(pin) << 8;

	config |= (0x1 << 2); /* configure for DPv1.3 */
	config |= 0x2; /* Configuring for UFP_D */

	pr_debug("DP config = 0x%x\n", config);
	return config;
}

void mdss_dp_phy_share_lane_config(struct dss_io_data *phy_io,
					u8 orientation, u8 ln_cnt)
{
	u32 info = 0x0;

	info |= (ln_cnt & 0x0F);
	info |= ((orientation & 0x0F) << 4);
	pr_debug("Shared Info = 0x%x\n", info);
	writel_relaxed(info, phy_io->base + DP_PHY_SPARE0);
}

void mdss_dp_config_audio_acr_ctrl(struct dss_io_data *ctrl_io, char link_rate)
{
	u32 acr_ctrl = 0;
	u32 select = 0;

	acr_ctrl = readl_relaxed(ctrl_io->base + MMSS_DP_AUDIO_ACR_CTRL);

	switch (link_rate) {
	case DP_LINK_RATE_162:
		select = 0;
		break;
	case DP_LINK_RATE_270:
		select = 1;
		break;
	case DP_LINK_RATE_540:
		select = 2;
		break;
	default:
		pr_debug("Unknown link rate\n");
		select = 0;
		break;
	}

	acr_ctrl |= select << 4 | BIT(31) | BIT(8) | BIT(14);

	pr_debug("select = 0x%x, acr_ctrl = 0x%x\n", select, acr_ctrl);

	writel_relaxed(acr_ctrl, ctrl_io->base + MMSS_DP_AUDIO_ACR_CTRL);
}

static u8 mdss_dp_get_g0_value(u8 data)
{
	u8 c[4];
	u8 g[4];
	u8 rData = 0;
	u8 i;

	for (i = 0; i < 4; i++)
		c[i] = (data >> i) & 0x01;

	g[0] = c[3];
	g[1] = c[0] ^ c[3];
	g[2] = c[1];
	g[3] = c[2];

	for (i = 0; i < 4; i++)
		rData = ((g[i] & 0x01) << i) | rData;

	return rData;
}

static u8 mdss_dp_get_g1_value(u8 data)
{
	u8 c[4];
	u8 g[4];
	u8 rData = 0;
	u8 i;

	for (i = 0; i < 4; i++)
		c[i] = (data >> i) & 0x01;

	g[0] = c[0] ^ c[3];
	g[1] = c[0] ^ c[1] ^ c[3];
	g[2] = c[1] ^ c[2];
	g[3] = c[2] ^ c[3];

	for (i = 0; i < 4; i++)
		rData = ((g[i] & 0x01) << i) | rData;

	return rData;
}

static u8 mdss_dp_calculate_parity_byte(u32 data)
{
	u8 x0 = 0;
	u8 x1 = 0;
	u8 ci = 0;
	u8 iData = 0;
	u8 i = 0;
	u8 parityByte;

	for (i = 0; i < 8; i++) {
		iData = (data >> i*4) & 0xF;

		ci = iData ^ x1;
		x1 = x0 ^ mdss_dp_get_g1_value(ci);
		x0 = mdss_dp_get_g0_value(ci);
	}

	parityByte = x1 | (x0 << 4);

	return parityByte;
}

static void mdss_dp_audio_setup_audio_stream_sdp(struct dss_io_data *ctrl_io)
{
	u32 value = 0;
	u32 new_value = 0;
	u8 parity_byte = 0;

	/* Config header and parity byte 1 */
	value = readl_relaxed(ctrl_io->base + MMSS_DP_AUDIO_STREAM_0);
	new_value = 0x02;
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_1_BIT)
			| (parity_byte << PARITY_BYTE_1_BIT));
	pr_debug("Header Byte 1: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base + MMSS_DP_AUDIO_STREAM_0);

	/* Config header and parity byte 2 */
	value = readl_relaxed(ctrl_io->base + MMSS_DP_AUDIO_STREAM_1);
	new_value = 0x0;
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_2_BIT)
			| (parity_byte << PARITY_BYTE_2_BIT));
	pr_debug("Header Byte 2: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base + MMSS_DP_AUDIO_STREAM_1);

	/* Config header and parity byte 3 */
	value = readl_relaxed(ctrl_io->base + MMSS_DP_AUDIO_STREAM_1);
	new_value = 0x01;
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_3_BIT)
			| (parity_byte << PARITY_BYTE_3_BIT));
	pr_debug("Header Byte 3: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base + MMSS_DP_AUDIO_STREAM_1);

}

static void mdss_dp_audio_setup_audio_timestamp_sdp(struct dss_io_data *ctrl_io)
{
	u32 value = 0;
	u32 new_value = 0;
	u8 parity_byte = 0;

	/* Config header and parity byte 1 */
	value = readl_relaxed(ctrl_io->base + MMSS_DP_AUDIO_TIMESTAMP_0);
	new_value = 0x1;
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_1_BIT)
			| (parity_byte << PARITY_BYTE_1_BIT));
	pr_debug("Header Byte 1: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base + MMSS_DP_AUDIO_TIMESTAMP_0);

	/* Config header and parity byte 2 */
	value = readl_relaxed(ctrl_io->base + MMSS_DP_AUDIO_TIMESTAMP_1);
	new_value = 0x17;
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_2_BIT)
			| (parity_byte << PARITY_BYTE_2_BIT));
	pr_debug("Header Byte 2: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base + MMSS_DP_AUDIO_TIMESTAMP_1);

	/* Config header and parity byte 3 */
	value = readl_relaxed(ctrl_io->base + MMSS_DP_AUDIO_TIMESTAMP_1);
	new_value = (0x0 | (0x12 << 2));
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_3_BIT)
			| (parity_byte << PARITY_BYTE_3_BIT));
	pr_debug("Header Byte 3: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base + MMSS_DP_AUDIO_TIMESTAMP_1);
}

static void mdss_dp_audio_setup_audio_infoframe_sdp(struct dss_io_data *ctrl_io)
{
	u32 value = 0;
	u32 new_value = 0;
	u8 parity_byte = 0;

	/* Config header and parity byte 1 */
	value = readl_relaxed(ctrl_io->base + MMSS_DP_AUDIO_INFOFRAME_0);
	new_value = 0x84;
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_1_BIT)
			| (parity_byte << PARITY_BYTE_1_BIT));
	pr_debug("Header Byte 1: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base + MMSS_DP_AUDIO_INFOFRAME_0);

	/* Config header and parity byte 2 */
	value = readl_relaxed(ctrl_io->base + MMSS_DP_AUDIO_INFOFRAME_1);
	new_value = 0x1b;
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_2_BIT)
			| (parity_byte << PARITY_BYTE_2_BIT));
	pr_debug("Header Byte 2: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base + MMSS_DP_AUDIO_INFOFRAME_1);

	/* Config header and parity byte 3 */
	value = readl_relaxed(ctrl_io->base + MMSS_DP_AUDIO_INFOFRAME_1);
	new_value = (0x0 | (0x12 << 2));
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_3_BIT)
			| (parity_byte << PARITY_BYTE_3_BIT));
	pr_debug("Header Byte 3: value = 0x%x, parity_byte = 0x%x\n",
			new_value, parity_byte);
	writel_relaxed(value, ctrl_io->base + MMSS_DP_AUDIO_INFOFRAME_1);

	/* Config Data Byte 0 - 2 as "Refer to Stream Header" */
	writel_relaxed(0x0, ctrl_io->base + MMSS_DP_AUDIO_INFOFRAME_2);
}

static void mdss_dp_audio_setup_copy_management_sdp(struct dss_io_data *ctrl_io)
{
	u32 value = 0;
	u32 new_value = 0;
	u8 parity_byte = 0;

	/* Config header and parity byte 1 */
	value = readl_relaxed(ctrl_io->base +
					MMSS_DP_AUDIO_COPYMANAGEMENT_0);
	new_value = 0x05;
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_1_BIT)
			| (parity_byte << PARITY_BYTE_1_BIT));
	pr_debug("Header Byte 1: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base +
					MMSS_DP_AUDIO_COPYMANAGEMENT_0);

	/* Config header and parity byte 2 */
	value = readl_relaxed(ctrl_io->base +
					MMSS_DP_AUDIO_COPYMANAGEMENT_1);
	new_value = 0x0F;
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_2_BIT)
			| (parity_byte << PARITY_BYTE_2_BIT));
	pr_debug("Header Byte 2: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base +
					MMSS_DP_AUDIO_COPYMANAGEMENT_1);

	/* Config header and parity byte 3 */
	value = readl_relaxed(ctrl_io->base +
					MMSS_DP_AUDIO_COPYMANAGEMENT_1);
	new_value = 0x0;
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_3_BIT)
			| (parity_byte << PARITY_BYTE_3_BIT));
	pr_debug("Header Byte 3: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base +
					MMSS_DP_AUDIO_COPYMANAGEMENT_1);

	writel_relaxed(0x0, ctrl_io->base +
					MMSS_DP_AUDIO_COPYMANAGEMENT_2);
	writel_relaxed(0x0, ctrl_io->base +
					MMSS_DP_AUDIO_COPYMANAGEMENT_3);
	writel_relaxed(0x0, ctrl_io->base +
					MMSS_DP_AUDIO_COPYMANAGEMENT_4);
}

static void mdss_dp_audio_setup_isrc_sdp(struct dss_io_data *ctrl_io)
{
	u32 value = 0;
	u32 new_value = 0;
	u8 parity_byte = 0;

	/* Config header and parity byte 1 */
	value = readl_relaxed(ctrl_io->base + MMSS_DP_AUDIO_ISRC_0);
	new_value = 0x06;
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_1_BIT)
			| (parity_byte << PARITY_BYTE_1_BIT));
	pr_debug("Header Byte 1: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base + MMSS_DP_AUDIO_ISRC_0);

	/* Config header and parity byte 2 */
	value = readl_relaxed(ctrl_io->base + MMSS_DP_AUDIO_ISRC_1);
	new_value = 0x0F;
	parity_byte = mdss_dp_calculate_parity_byte(new_value);
	value |= ((new_value << HEADER_BYTE_2_BIT)
			| (parity_byte << PARITY_BYTE_2_BIT));
	pr_debug("Header Byte 2: value = 0x%x, parity_byte = 0x%x\n",
			value, parity_byte);
	writel_relaxed(value, ctrl_io->base + MMSS_DP_AUDIO_ISRC_1);

	writel_relaxed(0x0, ctrl_io->base + MMSS_DP_AUDIO_ISRC_2);
	writel_relaxed(0x0, ctrl_io->base + MMSS_DP_AUDIO_ISRC_3);
	writel_relaxed(0x0, ctrl_io->base + MMSS_DP_AUDIO_ISRC_4);
}

void mdss_dp_audio_setup_sdps(struct dss_io_data *ctrl_io)
{
	u32 sdp_cfg = 0;
	u32 sdp_cfg2 = 0;

	/* AUDIO_TIMESTAMP_SDP_EN */
	sdp_cfg |= BIT(1);
	/* AUDIO_STREAM_SDP_EN */
	sdp_cfg |= BIT(2);
	/* AUDIO_COPY_MANAGEMENT_SDP_EN */
	sdp_cfg |= BIT(5);
	/* AUDIO_ISRC_SDP_EN  */
	sdp_cfg |= BIT(6);
	/* AUDIO_INFOFRAME_SDP_EN  */
	sdp_cfg |= BIT(20);

	writel_relaxed(sdp_cfg, ctrl_io->base + MMSS_DP_SDP_CFG);

	sdp_cfg2 = readl_relaxed(ctrl_io->base + MMSS_DP_SDP_CFG2);
	/* IFRM_REGSRC -> Do not use reg values */
	sdp_cfg2 &= ~BIT(0);
	/* AUDIO_STREAM_HB3_REGSRC-> Do not use reg values */
	sdp_cfg2 &= ~BIT(1);

	writel_relaxed(sdp_cfg2, ctrl_io->base + MMSS_DP_SDP_CFG2);

	mdss_dp_audio_setup_audio_stream_sdp(ctrl_io);
	mdss_dp_audio_setup_audio_timestamp_sdp(ctrl_io);
	mdss_dp_audio_setup_audio_infoframe_sdp(ctrl_io);
	mdss_dp_audio_setup_copy_management_sdp(ctrl_io);
	mdss_dp_audio_setup_isrc_sdp(ctrl_io);
}

void mdss_dp_set_safe_to_exit_level(struct dss_io_data *ctrl_io,
		uint32_t lane_cnt)
{
	u32 safe_to_exit_level = 0;
	u32 mainlink_levels = 0;

	switch (lane_cnt) {
	case 1:
		safe_to_exit_level = 14;
		break;
	case 2:
		safe_to_exit_level = 8;
		break;
	case 4:
		safe_to_exit_level = 5;
		break;
	default:
		pr_debug("setting the default safe_to_exit_level = %u\n",
				safe_to_exit_level);
		safe_to_exit_level = 14;
		break;
	}

	mainlink_levels = readl_relaxed(ctrl_io->base + DP_MAINLINK_LEVELS);
	mainlink_levels &= 0xFF0;
	mainlink_levels |= safe_to_exit_level;

	pr_debug("mainlink_level = 0x%x, safe_to_exit_level = 0x%x\n",
			mainlink_levels, safe_to_exit_level);

	writel_relaxed(mainlink_levels, ctrl_io->base + DP_MAINLINK_LEVELS);
}

void mdss_dp_audio_enable(struct dss_io_data *ctrl_io, bool enable)
{
	u32 audio_ctrl = readl_relaxed(ctrl_io->base + MMSS_DP_AUDIO_CFG);

	if (enable)
		audio_ctrl |= BIT(0);
	else
		audio_ctrl &= ~BIT(0);

	writel_relaxed(audio_ctrl, ctrl_io->base + MMSS_DP_AUDIO_CFG);
}
