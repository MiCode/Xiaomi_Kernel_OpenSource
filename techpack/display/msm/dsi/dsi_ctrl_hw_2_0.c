// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/iopoll.h>

#include "dsi_ctrl_hw.h"
#include "dsi_ctrl_reg.h"
#include "dsi_hw.h"

void dsi_ctrl_hw_20_setup_lane_map(struct dsi_ctrl_hw *ctrl,
		       struct dsi_lane_map *lane_map)
{
	u32 reg_value = lane_map->lane_map_v2[DSI_LOGICAL_LANE_0] |
			(lane_map->lane_map_v2[DSI_LOGICAL_LANE_1] << 4) |
			(lane_map->lane_map_v2[DSI_LOGICAL_LANE_2] << 8) |
			(lane_map->lane_map_v2[DSI_LOGICAL_LANE_3] << 12);

	DSI_W32(ctrl, DSI_LANE_SWAP_CTRL, reg_value);

	DSI_CTRL_HW_DBG(ctrl, "Lane swap setup complete\n");
}

int dsi_ctrl_hw_20_wait_for_lane_idle(struct dsi_ctrl_hw *ctrl,
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

	DSI_CTRL_HW_DBG(ctrl, "polling for fifo empty, mask=0x%08x\n",
		fifo_empty_mask);
	rc = readl_poll_timeout(ctrl->base + DSI_FIFO_STATUS, val,
			(val & fifo_empty_mask), sleep_us, timeout_us);
	if (rc) {
		DSI_CTRL_HW_ERR(ctrl, "fifo not empty, FIFO_STATUS=0x%08x\n",
				val);
		goto error;
	}

error:
	return rc;
}

#define DUMP_REG_VALUE(off) "\t%-30s: 0x%08x\n", #off, DSI_R32(ctrl, off)
ssize_t dsi_ctrl_hw_20_reg_dump_to_buffer(struct dsi_ctrl_hw *ctrl,
					  char *buf,
					  u32 size)
{
	u32 len = 0;

	len += snprintf((buf + len), (size - len), "CONFIGURATION REGS:\n");

	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_HW_VERSION));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_STATUS));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_FIFO_STATUS));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_VIDEO_MODE_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_VIDEO_MODE_SYNC_DATATYPE));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_VIDEO_MODE_PIXEL_DATATYPE));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_VIDEO_MODE_BLANKING_DATATYPE));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_VIDEO_MODE_DATA_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_VIDEO_MODE_ACTIVE_H));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_VIDEO_MODE_ACTIVE_V));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_VIDEO_MODE_TOTAL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_VIDEO_MODE_HSYNC));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_VIDEO_MODE_VSYNC));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_VIDEO_MODE_VSYNC_VPOS));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_COMMAND_MODE_DMA_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_COMMAND_MODE_MDP_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_COMMAND_MODE_MDP_DCS_CMD_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_DMA_CMD_OFFSET));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_DMA_CMD_LENGTH));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_DMA_FIFO_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_DMA_NULL_PACKET_DATA));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_COMMAND_MODE_MDP_STREAM0_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_COMMAND_MODE_MDP_STREAM0_TOTAL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_COMMAND_MODE_MDP_STREAM1_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_COMMAND_MODE_MDP_STREAM1_TOTAL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_ACK_ERR_STATUS));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_RDBK_DATA0));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_RDBK_DATA1));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_RDBK_DATA2));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_RDBK_DATA3));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_RDBK_DATATYPE0));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_RDBK_DATATYPE1));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_TRIG_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_EXT_MUX));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_EXT_MUX_TE_PULSE_DETECT_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_CMD_MODE_DMA_SW_TRIGGER));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_CMD_MODE_MDP_SW_TRIGGER));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_CMD_MODE_BTA_SW_TRIGGER));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_RESET_SW_TRIGGER));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_MISR_CMD_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_MISR_VIDEO_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_LANE_STATUS));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_LANE_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_LANE_SWAP_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_DLN0_PHY_ERR));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_LP_TIMER_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_HS_TIMER_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_TIMEOUT_STATUS));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_CLKOUT_TIMING_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_EOT_PACKET));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_EOT_PACKET_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_GENERIC_ESC_TX_TRIGGER));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_ERR_INT_MASK0));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_INT_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_SOFT_RESET));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_CLK_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_CLK_STATUS));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_PHY_SW_RESET));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_AXI2AHB_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_MISR_CMD_MDP0_32BIT));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_MISR_CMD_MDP1_32BIT));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_MISR_VIDEO_32BIT));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_COMMAND_MODE_MDP_CTRL2));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_COMMAND_MODE_MDP_STREAM2_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_COMMAND_MODE_MDP_STREAM2_TOTAL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_VBIF_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_AES_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_RDBK_DATA_CTRL));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_TEST_PATTERN_GEN_CMD_DMA_INIT_VAL2));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_TPG_DMA_FIFO_STATUS));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_TPG_DMA_FIFO_WRITE_TRIGGER));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_DSI_TIMING_FLUSH));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_DSI_TIMING_DB_MODE));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_TPG_DMA_FIFO_RESET));
	len += snprintf((buf + len), (size - len),
			DUMP_REG_VALUE(DSI_VERSION));

	DSI_CTRL_HW_ERR(ctrl, "LLENGTH = %d\n", len);
	return len;
}
