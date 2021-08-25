// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include "dsi_ctrl_hw.h"
#include "dsi_ctrl_reg.h"
#include "dsi_hw.h"
#include "dsi_catalog.h"

#define DISP_CC_MISC_CMD_REG_OFF 0x00

/* register to configure DMA scheduling */
#define DSI_DMA_SCHEDULE_CTRL 0x100

/* MDP INTF registers to be mapped*/
#define MDP_INTF1_TEAR_LINE_COUNT 0xAE6BAB0
#define MDP_INTF1_LINE_COUNT 0xAE6B8B0

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

/**
 * dsi_ctrl_hw_22_map_mdp_regs() - maps MDP interface line count registers.
 * @pdev:		Pointer to platform device.
 * @ctrl:		Pointer to the controller host hardware.
 *
 * Return: 0 on success and error on failure.
 */
int dsi_ctrl_hw_22_map_mdp_regs(struct platform_device *pdev,
			struct dsi_ctrl_hw *ctrl)
{
	int rc = 0;
	void __iomem *ptr = NULL, *ptr1 = NULL;

	if (ctrl->index == 0) {
		ptr = devm_ioremap(&pdev->dev, MDP_INTF1_TEAR_LINE_COUNT, 1);
		if (IS_ERR_OR_NULL(ptr)) {
			DSI_CTRL_HW_ERR(ctrl,
				"MDP TE LINE COUNT address not found\n");
			rc = PTR_ERR(ptr);
			return rc;
		}

		ptr1 = devm_ioremap(&pdev->dev, MDP_INTF1_LINE_COUNT, 1);
		if (IS_ERR_OR_NULL(ptr1)) {
			DSI_CTRL_HW_ERR(ctrl,
				"MDP TE LINE COUNT address not found\n");
			rc = PTR_ERR(ptr1);
			return rc;
		}
	}

	ctrl->te_rd_ptr_reg = ptr;
	ctrl->line_count_reg = ptr1;

	return rc;
}

/**
 * dsi_ctrl_hw_22_log_line_count() - reads the MDP interface line count
 *					registers.
 * @ctrl:	Pointer to the controller host hardware.
 * @cmd_mode:	Boolean to indicate command mode operation.
 *
 * Return: INTF register value.
 */
u32 dsi_ctrl_hw_22_log_line_count(struct dsi_ctrl_hw *ctrl, bool cmd_mode)
{

	u32 reg = 0;

	if (cmd_mode && ctrl->te_rd_ptr_reg)
		reg = readl_relaxed(ctrl->te_rd_ptr_reg);
	else if (ctrl->line_count_reg)
		reg = readl_relaxed(ctrl->line_count_reg);

	return reg;
}
