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

void mdss_dp_ctrl_lane_mapping(struct dss_io_data *ctrl_io,
				struct lane_mapping l_map)
{
	u8 bits_per_lane = 2;
	u32 lane_map = ((l_map.lane0 << (bits_per_lane * 0))
			    || (l_map.lane1 << (bits_per_lane * 1))
			    || (l_map.lane2 << (bits_per_lane * 2))
			    || (l_map.lane3 << (bits_per_lane * 3)));
	writel_relaxed(lane_map,
		ctrl_io->base + DP_LOGICAL2PHYSCIAL_LANE_MAPPING);
}

void mdss_dp_phy_aux_setup(struct dss_io_data *phy_io)
{
	writel_relaxed(0x3d, phy_io->base + DP_PHY_PD_CTL);
	writel_relaxed(0x03, phy_io->base + DP_PHY_AUX_CFG1);
	writel_relaxed(0x00, phy_io->base + DP_PHY_AUX_CFG3);
	writel_relaxed(0x0a, phy_io->base + DP_PHY_AUX_CFG4);
	writel_relaxed(0x26, phy_io->base + DP_PHY_AUX_CFG5);
	writel_relaxed(0x0a, phy_io->base + DP_PHY_AUX_CFG6);
	writel_relaxed(0x03, phy_io->base + DP_PHY_AUX_CFG7);
	writel_relaxed(0xbb, phy_io->base + DP_PHY_AUX_CFG8);
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
