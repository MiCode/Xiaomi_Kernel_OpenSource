/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "dsi-hw:" fmt

#include "dsi_ctrl_hw.h"
#include "dsi_ctrl_reg.h"
#include "dsi_hw.h"
#include "dsi_catalog.h"

#define DISP_CC_MISC_CMD_REG_OFF 0x00

/* register to configure DMA scheduling */
#define DSI_DMA_SCHEDULE_CTRL 0x100

/**
 * dsi_ctrl_hw_22_phy_reset_config() - to configure clamp control during ulps
 * @ctrl:          Pointer to the controller host hardware.
 * @enable:      boolean to specify enable/disable.
 */
void dsi_ctrl_hw_22_phy_reset_config(struct dsi_ctrl_hw *ctrl,
		bool enable)
{
	u32 reg = 0;

	reg = DSI_DISP_CC_R32(ctrl, DISP_CC_MISC_CMD_REG_OFF);

	/* Mask/unmask disable PHY reset bit */
	if (enable)
		reg &= ~BIT(ctrl->index);
	else
		reg |= BIT(ctrl->index);
	DSI_DISP_CC_W32(ctrl, DISP_CC_MISC_CMD_REG_OFF, reg);
}

/**
 * dsi_ctrl_hw_22_schedule_dma_cmd() - to schedule DMA command transfer
 * @ctrl:         Pointer to the controller host hardware.
 * @line_no:      Line number at which command needs to be sent.
 */
void dsi_ctrl_hw_22_schedule_dma_cmd(struct dsi_ctrl_hw *ctrl, int line_no)
{
	u32 reg = 0;

	reg = DSI_R32(ctrl, DSI_DMA_SCHEDULE_CTRL);
	reg |= BIT(28);
	reg |= (line_no & 0xffff);

	DSI_W32(ctrl, DSI_DMA_SCHEDULE_CTRL, reg);
}

/*
 * dsi_ctrl_hw_22_get_cont_splash_status() - to verify whether continuous
 *                                           splash is enabled or not
 * @ctrl:          Pointer to the controller host hardware.
 *
 * Return:         Return Continuous splash status
 */
bool dsi_ctrl_hw_22_get_cont_splash_status(struct dsi_ctrl_hw *ctrl)
{
	u32 reg = 0;

	/**
	 * DSI scratch register 1 is used to notify whether continuous
	 * splash is enabled or not by bootloader
	 */
	reg = DSI_R32(ctrl, DSI_SCRATCH_REGISTER_1);
	return reg == 0x1 ? true : false;
}

/*
 * dsi_ctrl_hw_kickoff_non_embedded_mode()-Kickoff cmd  in non-embedded mode
 * @ctrl:                  - Pointer to the controller host hardware.
 * @dsi_ctrl_cmd_dma_info: - command buffer information.
 * @flags:		   - DSI CTRL Flags.
 */
void dsi_ctrl_hw_kickoff_non_embedded_mode(struct dsi_ctrl_hw *ctrl,
				    struct dsi_ctrl_cmd_dma_info *cmd,
				    u32 flags)
{
	u32 reg = 0;

	reg = DSI_R32(ctrl, DSI_COMMAND_MODE_DMA_CTRL);

	reg &= ~BIT(31);/* disable broadcast */
	reg &= ~BIT(30);

	if (cmd->use_lpm)
		reg |= BIT(26);
	else
		reg &= ~BIT(26);

	/* Select non EMBEDDED_MODE, pick the packet header from register */
	reg &= ~BIT(28);
	reg |= BIT(24);/* long packet */
	reg |= BIT(29);/* wc_sel = 1 */
	reg |= (((cmd->datatype) & 0x03f) << 16);/* data type */
	DSI_W32(ctrl, DSI_COMMAND_MODE_DMA_CTRL, reg);

	/* Enable WRITE_WATERMARK_DISABLE and READ_WATERMARK_DISABLE bits */
	reg = DSI_R32(ctrl, DSI_DMA_FIFO_CTRL);
	reg |= BIT(20);
	reg |= BIT(16);
	reg |= 0x33;/* Set READ and WRITE watermark levels to maximum */
	DSI_W32(ctrl, DSI_DMA_FIFO_CTRL, reg);

	DSI_W32(ctrl, DSI_DMA_CMD_OFFSET, cmd->offset);
	DSI_W32(ctrl, DSI_DMA_CMD_LENGTH, ((cmd->length) & 0xFFFFFF));

	/* wait for writes to complete before kick off */
	wmb();

	if (!(flags & DSI_CTRL_HW_CMD_WAIT_FOR_TRIGGER))
		DSI_W32(ctrl, DSI_CMD_MODE_DMA_SW_TRIGGER, 0x1);
}

/*
 * dsi_ctrl_hw_22_config_clk_gating() - enable/disable clk gating on DSI PHY
 * @ctrl:          Pointer to the controller host hardware.
 * @enable:        bool to notify enable/disable.
 * @clk_selection:        clock to enable/disable clock gating.
 *
 */
void dsi_ctrl_hw_22_config_clk_gating(struct dsi_ctrl_hw *ctrl, bool enable,
				enum dsi_clk_gate_type clk_selection)
{
	u32 reg = 0;
	u32 enable_select = 0;

	reg = DSI_DISP_CC_R32(ctrl, DISP_CC_MISC_CMD_REG_OFF);

	if (clk_selection & PIXEL_CLK)
		enable_select |= ctrl->index ? BIT(6) : BIT(5);

	if (clk_selection & BYTE_CLK)
		enable_select |= ctrl->index ? BIT(8) : BIT(7);

	if (clk_selection & DSI_PHY)
		enable_select |= ctrl->index ? BIT(10) : BIT(9);

	if (enable)
		reg |= enable_select;
	else
		reg &= ~enable_select;

	DSI_DISP_CC_W32(ctrl, DISP_CC_MISC_CMD_REG_OFF, reg);
}
