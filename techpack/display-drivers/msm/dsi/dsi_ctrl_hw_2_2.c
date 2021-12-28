// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */
#include <linux/iopoll.h>
#include "dsi_ctrl_hw.h"
#include "dsi_ctrl_reg.h"
#include "dsi_hw.h"
#include "dsi_ctrl.h"
#include "dsi_catalog.h"

#define DISP_CC_MISC_CMD_REG_OFF 0x00

/* register to configure DMA scheduling */
#define DSI_DMA_SCHEDULE_CTRL 0x100
#define DSI_DMA_SCHEDULE_CTRL2 0x0104

/* offset addresses of MDP INTF base register, to be mapped for debug feature */
#define MDP_INTF_TEAR_OFFSET 0x280
#define MDP_INTF_TEAR_LINE_COUNT_OFFSET 0x30
#define MDP_INTF_LINE_COUNT_OFFSET 0xB0

void dsi_ctrl_hw_22_setup_lane_map(struct dsi_ctrl_hw *ctrl,
		       struct dsi_lane_map *lane_map)
{
	u32 reg_value = lane_map->lane_map_v2[DSI_LOGICAL_LANE_0] |
			(lane_map->lane_map_v2[DSI_LOGICAL_LANE_1] << 4) |
			(lane_map->lane_map_v2[DSI_LOGICAL_LANE_2] << 8) |
			(lane_map->lane_map_v2[DSI_LOGICAL_LANE_3] << 12);

	DSI_W32(ctrl, DSI_LANE_SWAP_CTRL, reg_value);

	DSI_CTRL_HW_DBG(ctrl, "[DSI_%d] Lane swap setup complete\n",
			ctrl->index);
}

int dsi_ctrl_hw_22_wait_for_lane_idle(struct dsi_ctrl_hw *ctrl,
		u32 lanes)
{
	int rc = 0, val = 0;
	u32 fifo_empty_mask = 0;
	u32 const sleep_us = 10;
	u32 const timeout_us = 100;

	if (lanes & DSI_DATA_LANE_0)
		fifo_empty_mask |= (BIT(12) | BIT(16));

	if (lanes & DSI_DATA_LANE_1)
		fifo_empty_mask |= BIT(20);

	if (lanes & DSI_DATA_LANE_2)
		fifo_empty_mask |= BIT(24);

	if (lanes & DSI_DATA_LANE_3)
		fifo_empty_mask |= BIT(28);

	DSI_CTRL_HW_DBG(ctrl, "%s: polling for fifo empty, mask=0x%08x\n",
		__func__, fifo_empty_mask);
	rc = DSI_READ_POLL_TIMEOUT(ctrl, DSI_FIFO_STATUS, val,
			(val & fifo_empty_mask), sleep_us, timeout_us);
	if (rc) {
		DSI_CTRL_HW_ERR(ctrl,
				"%s: fifo not empty, FIFO_STATUS=0x%08x\n",
				__func__, val);
		goto error;
	}
error:
	return rc;
}

ssize_t dsi_ctrl_hw_22_reg_dump_to_buffer(struct dsi_ctrl_hw *ctrl,
					  char *buf,
					  u32 size)
{
	return size;
}

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
	ctrl->reset_trig_ctrl = true;
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
 * dsi_ctrl_hw_22_configure_cmddma_window() - configure DMA window for CMD TX
 * @ctrl:	Pointer to the controller host hardware.
 * @cmd:	Pointer to the DSI DMA command info.
 * @line_no:	Line number at which the CMD needs to be triggered.
 * @window:	Width of the DMA CMD window.
 */
void dsi_ctrl_hw_22_configure_cmddma_window(struct dsi_ctrl_hw *ctrl,
		struct dsi_ctrl_cmd_dma_info *cmd,
		u32 line_no, u32 window)
{
	u32 reg = 0;

	if (cmd->en_broadcast) {
		reg = DSI_R32(ctrl, DSI_TRIG_CTRL);
		if (cmd->is_master) {
			reg &= ~0xF;
			reg |= 0xc;
		} else {
			reg &= ~0xF;
			reg |= BIT(16);
		}

		DSI_W32(ctrl, DSI_TRIG_CTRL, reg);

		if (cmd->is_master) {
			reg = 0;
			reg |= line_no;
			reg |= window << 16;
			DSI_W32(ctrl, DSI_DMA_SCHEDULE_CTRL2, reg);
		}
	} else {
		reg = DSI_R32(ctrl, DSI_TRIG_CTRL);
		reg &= ~0xF;
		reg |= 0xc;
		DSI_W32(ctrl, DSI_TRIG_CTRL, reg);

		reg = 0;
		reg |= line_no;
		reg |= window << 16;
		DSI_W32(ctrl, DSI_DMA_SCHEDULE_CTRL2, reg);
	}
	ctrl->reset_trig_ctrl = true;
}

/**
 * dsi_ctrl_hw_22_reset_trigger_controls() - reset dsi trigger configurations
 * @ctrl:             Pointer to the controller host hardware.
 * @cfg:              DSI host configuration that is common to both video and
 *                    command modes.
 */
void dsi_ctrl_hw_22_reset_trigger_controls(struct dsi_ctrl_hw *ctrl,
				       struct dsi_host_common_cfg *cfg)
{
	u32 reg = 0;
	const u8 trigger_map[DSI_TRIGGER_MAX] = {
		0x0, 0x2, 0x1, 0x4, 0x5, 0x6 };

	reg |= (cfg->te_mode == DSI_TE_ON_EXT_PIN) ? BIT(31) : 0;
	reg |= (trigger_map[cfg->dma_cmd_trigger] & 0x7);
	reg |= (trigger_map[cfg->mdp_cmd_trigger] & 0x7) << 4;
	DSI_W32(ctrl, DSI_TRIG_CTRL, reg);
	DSI_W32(ctrl, DSI_DMA_SCHEDULE_CTRL2, 0x0);
	DSI_W32(ctrl, DSI_DMA_SCHEDULE_CTRL, 0x0);
	ctrl->reset_trig_ctrl = false;
}

/**
 * dsi_ctrl_hw_22_log_line_count() - reads the MDP interface line count
 *				     registers.
 * @ctrl:	Pointer to the controller host hardware.
 * @cmd_mode:	Boolean to indicate command mode operation.
 *
 * Return: INTF register value.
 */
u32 dsi_ctrl_hw_22_log_line_count(struct dsi_ctrl_hw *ctrl, bool cmd_mode)
{

	u32 reg = 0;

	if (IS_ERR_OR_NULL(ctrl->mdp_intf_base))
		return reg;

	if (cmd_mode)
		reg = DSI_MDP_INTF_R32(ctrl, MDP_INTF_TEAR_OFFSET
					+ MDP_INTF_TEAR_LINE_COUNT_OFFSET);
	else
		reg = DSI_MDP_INTF_R32(ctrl, MDP_INTF_LINE_COUNT_OFFSET);
	return reg;
}

void dsi_ctrl_hw_22_configure_splitlink(struct dsi_ctrl_hw *ctrl,
		struct dsi_host_common_cfg *common_cfg, u32 flags)
{
	u32 reg = 0;

	reg = DSI_R32(ctrl, DSI_SPLIT_LINK);

	/* DMA_LINK_SEL */
	reg &= ~(0x7 << 12);

	/* Send command to both sublinks unless specified */
	if (flags & DSI_CTRL_CMD_SUBLINK0)
		reg |= BIT(12);
	else if (flags & DSI_CTRL_CMD_SUBLINK1)
		reg |= BIT(13);
	else
		reg |= (BIT(12) | BIT(13));

	DSI_W32(ctrl, DSI_SPLIT_LINK, reg);

	/* Make sure the split link config is updated */
	wmb();
}
