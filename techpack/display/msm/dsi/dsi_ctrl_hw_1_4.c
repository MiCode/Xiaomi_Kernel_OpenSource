// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/iopoll.h>

#include "dsi_ctrl_hw.h"
#include "dsi_ctrl_reg.h"
#include "dsi_hw.h"

#define MMSS_MISC_CLAMP_REG_OFF           0x0014

/**
 * dsi_ctrl_hw_14_setup_lane_map() - setup mapping between
 *	logical and physical lanes
 * @ctrl:          Pointer to the controller host hardware.
 * @lane_map:      Structure defining the mapping between DSI logical
 *                 lanes and physical lanes.
 */
void dsi_ctrl_hw_14_setup_lane_map(struct dsi_ctrl_hw *ctrl,
			       struct dsi_lane_map *lane_map)
{
	DSI_W32(ctrl, DSI_LANE_SWAP_CTRL, lane_map->lane_map_v1);

	DSI_CTRL_HW_DBG(ctrl, "Lane swap setup complete\n");
}

/**
 * dsi_ctrl_hw_14_wait_for_lane_idle()
 * This function waits for all the active DSI lanes to be idle by polling all
 * the FIFO_EMPTY bits and polling he lane status to ensure that all the lanes
 * are in stop state. This function assumes that the bus clocks required to
 * access the registers are already turned on.
 *
 * @ctrl:      Pointer to the controller host hardware.
 * @lanes:     ORed list of lanes (enum dsi_data_lanes) which need
 *             to be stopped.
 *
 * return: Error code.
 */
int dsi_ctrl_hw_14_wait_for_lane_idle(struct dsi_ctrl_hw *ctrl, u32 lanes)
{
	int rc = 0, val = 0;
	u32 stop_state_mask = 0, fifo_empty_mask = 0;
	u32 const sleep_us = 10;
	u32 const timeout_us = 100;

	if (lanes & DSI_DATA_LANE_0) {
		stop_state_mask |= BIT(0);
		fifo_empty_mask |= (BIT(12) | BIT(16));
	}
	if (lanes & DSI_DATA_LANE_1) {
		stop_state_mask |= BIT(1);
			fifo_empty_mask |= BIT(20);
	}
	if (lanes & DSI_DATA_LANE_2) {
		stop_state_mask |= BIT(2);
		fifo_empty_mask |= BIT(24);
	}
	if (lanes & DSI_DATA_LANE_3) {
		stop_state_mask |= BIT(3);
		fifo_empty_mask |= BIT(28);
	}

	DSI_CTRL_HW_DBG(ctrl, "polling for fifo empty, mask=0x%08x\n",
			fifo_empty_mask);
	rc = readl_poll_timeout(ctrl->base + DSI_FIFO_STATUS, val,
			(val & fifo_empty_mask), sleep_us, timeout_us);
	if (rc) {
		DSI_CTRL_HW_ERR(ctrl, "fifo not empty, FIFO_STATUS=0x%08x\n",
				val);
		goto error;
	}

	DSI_CTRL_HW_DBG(ctrl, "polling for lanes to be in stop state, mask=0x%08x\n",
		stop_state_mask);
	rc = readl_poll_timeout(ctrl->base + DSI_LANE_STATUS, val,
			(val & stop_state_mask), sleep_us, timeout_us);
	if (rc) {
		DSI_CTRL_HW_ERR(ctrl, "lanes not in stop state, LANE_STATUS=0x%08x\n",
			val);
		goto error;
	}

error:
	return rc;

}

/**
 * ulps_request() - request ulps entry for specified lanes
 * @ctrl:          Pointer to the controller host hardware.
 * @lanes:         ORed list of lanes (enum dsi_data_lanes) which need
 *                 to enter ULPS.
 *
 * Caller should check if lanes are in ULPS mode by calling
 * get_lanes_in_ulps() operation.
 */
void dsi_ctrl_hw_cmn_ulps_request(struct dsi_ctrl_hw *ctrl, u32 lanes)
{
	u32 reg = 0;

	reg = DSI_R32(ctrl, DSI_LANE_CTRL);

	if (lanes & DSI_CLOCK_LANE)
		reg |= BIT(4);
	if (lanes & DSI_DATA_LANE_0)
		reg |= BIT(0);
	if (lanes & DSI_DATA_LANE_1)
		reg |= BIT(1);
	if (lanes & DSI_DATA_LANE_2)
		reg |= BIT(2);
	if (lanes & DSI_DATA_LANE_3)
		reg |= BIT(3);

	/*
	 * ULPS entry request. Wait for short time to make sure
	 * that the lanes enter ULPS. Recommended as per HPG.
	 */
	DSI_W32(ctrl, DSI_LANE_CTRL, reg);
	usleep_range(100, 110);

	DSI_CTRL_HW_DBG(ctrl, "ULPS requested for lanes 0x%x\n", lanes);
}

/**
 * ulps_exit() - exit ULPS on specified lanes
 * @ctrl:          Pointer to the controller host hardware.
 * @lanes:         ORed list of lanes (enum dsi_data_lanes) which need
 *                 to exit ULPS.
 *
 * Caller should check if lanes are in active mode by calling
 * get_lanes_in_ulps() operation.
 */
void dsi_ctrl_hw_cmn_ulps_exit(struct dsi_ctrl_hw *ctrl, u32 lanes)
{
	u32 reg = 0;
	u32 prev_reg = 0;

	prev_reg = DSI_R32(ctrl, DSI_LANE_CTRL);
	prev_reg &= BIT(24);

	if (lanes & DSI_CLOCK_LANE)
		reg |= BIT(12);
	if (lanes & DSI_DATA_LANE_0)
		reg |= BIT(8);
	if (lanes & DSI_DATA_LANE_1)
		reg |= BIT(9);
	if (lanes & DSI_DATA_LANE_2)
		reg |= BIT(10);
	if (lanes & DSI_DATA_LANE_3)
		reg |= BIT(11);

	/*
	 * ULPS Exit Request
	 * Hardware requirement is to wait for at least 1ms
	 */
	DSI_W32(ctrl, DSI_LANE_CTRL, reg | prev_reg);
	usleep_range(1000, 1010);
	/*
	 * Sometimes when exiting ULPS, it is possible that some DSI
	 * lanes are not in the stop state which could lead to DSI
	 * commands not going through. To avoid this, force the lanes
	 * to be in stop state.
	 */
	DSI_W32(ctrl, DSI_LANE_CTRL, (reg << 8) | prev_reg);
	wmb(); /* ensure lanes are put to stop state */
	DSI_W32(ctrl, DSI_LANE_CTRL, 0x0 | prev_reg);
	wmb(); /* ensure lanes are put to stop state */

	DSI_CTRL_HW_DBG(ctrl, "ULPS exit request for lanes=0x%x\n", lanes);
}

/**
 * get_lanes_in_ulps() - returns the list of lanes in ULPS mode
 * @ctrl:          Pointer to the controller host hardware.
 *
 * Returns an ORed list of lanes (enum dsi_data_lanes) that are in ULPS
 * state. If 0 is returned, all the lanes are active.
 *
 * Return: List of lanes in ULPS state.
 */
u32 dsi_ctrl_hw_cmn_get_lanes_in_ulps(struct dsi_ctrl_hw *ctrl)
{
	u32 reg = 0;
	u32 lanes = 0;

	reg = DSI_R32(ctrl, DSI_LANE_STATUS);
	if (!(reg & BIT(8)))
		lanes |= DSI_DATA_LANE_0;
	if (!(reg & BIT(9)))
		lanes |= DSI_DATA_LANE_1;
	if (!(reg & BIT(10)))
		lanes |= DSI_DATA_LANE_2;
	if (!(reg & BIT(11)))
		lanes |= DSI_DATA_LANE_3;
	if (!(reg & BIT(12)))
		lanes |= DSI_CLOCK_LANE;

	DSI_CTRL_HW_DBG(ctrl, "lanes in ulps = 0x%x\n", lanes);
	return lanes;
}

/**
 * clamp_enable() - enable DSI clamps to keep PHY driving a stable link
 * @ctrl:          Pointer to the controller host hardware.
 * @lanes:         ORed list of lanes which need to be clamped.
 * @enable_ulps:   Boolean to specify if ULPS is enabled in DSI controller
 */
void dsi_ctrl_hw_14_clamp_enable(struct dsi_ctrl_hw *ctrl,
				 u32 lanes,
				 bool enable_ulps)
{
	u32 clamp_reg = 0;
	u32 bit_shift = 0;
	u32 reg = 0;

	if (ctrl->index == 1)
		bit_shift = 16;

	if (lanes & DSI_CLOCK_LANE) {
		clamp_reg |= BIT(9);
		if (enable_ulps)
			clamp_reg |= BIT(8);
	}

	if (lanes & DSI_DATA_LANE_0) {
		clamp_reg |= BIT(7);
		if (enable_ulps)
			clamp_reg |= BIT(6);
	}

	if (lanes & DSI_DATA_LANE_1) {
		clamp_reg |= BIT(5);
		if (enable_ulps)
			clamp_reg |= BIT(4);
	}

	if (lanes & DSI_DATA_LANE_2) {
		clamp_reg |= BIT(3);
		if (enable_ulps)
			clamp_reg |= BIT(2);
	}

	if (lanes & DSI_DATA_LANE_3) {
		clamp_reg |= BIT(1);
		if (enable_ulps)
			clamp_reg |= BIT(0);
	}

	reg = DSI_MMSS_MISC_R32(ctrl, MMSS_MISC_CLAMP_REG_OFF);
	reg |= (clamp_reg << bit_shift);
	DSI_MMSS_MISC_W32(ctrl, MMSS_MISC_CLAMP_REG_OFF, reg);

	reg = DSI_MMSS_MISC_R32(ctrl, MMSS_MISC_CLAMP_REG_OFF);
	reg |= (BIT(15) << bit_shift);	/* Enable clamp */
	DSI_MMSS_MISC_W32(ctrl, MMSS_MISC_CLAMP_REG_OFF, reg);

	DSI_CTRL_HW_DBG(ctrl, "Clamps enabled for lanes=0x%x\n", lanes);
}

/**
 * clamp_disable() - disable DSI clamps
 * @ctrl:          Pointer to the controller host hardware.
 * @lanes:         ORed list of lanes which need to have clamps released.
 * @disable_ulps:   Boolean to specify if ULPS is enabled in DSI controller
 */
void dsi_ctrl_hw_14_clamp_disable(struct dsi_ctrl_hw *ctrl,
				  u32 lanes,
				  bool disable_ulps)
{
	u32 clamp_reg = 0;
	u32 bit_shift = 0;
	u32 reg = 0;

	if (ctrl->index == 1)
		bit_shift = 16;

	if (lanes & DSI_CLOCK_LANE) {
		clamp_reg |= BIT(9);
		if (disable_ulps)
			clamp_reg |= BIT(8);
	}

	if (lanes & DSI_DATA_LANE_0) {
		clamp_reg |= BIT(7);
		if (disable_ulps)
			clamp_reg |= BIT(6);
	}

	if (lanes & DSI_DATA_LANE_1) {
		clamp_reg |= BIT(5);
		if (disable_ulps)
			clamp_reg |= BIT(4);
	}

	if (lanes & DSI_DATA_LANE_2) {
		clamp_reg |= BIT(3);
		if (disable_ulps)
			clamp_reg |= BIT(2);
	}

	if (lanes & DSI_DATA_LANE_3) {
		clamp_reg |= BIT(1);
		if (disable_ulps)
			clamp_reg |= BIT(0);
	}

	clamp_reg |= BIT(15); /* Enable clamp */
	clamp_reg <<= bit_shift;

	reg = DSI_MMSS_MISC_R32(ctrl, MMSS_MISC_CLAMP_REG_OFF);
	reg &= ~(clamp_reg);
	DSI_MMSS_MISC_W32(ctrl, MMSS_MISC_CLAMP_REG_OFF, reg);

	DSI_CTRL_HW_DBG(ctrl, "Disable clamps for lanes=%d\n", lanes);
}

#define DUMP_REG_VALUE(off) "\t%-30s: 0x%08x\n", #off, DSI_R32(ctrl, off)
ssize_t dsi_ctrl_hw_14_reg_dump_to_buffer(struct dsi_ctrl_hw *ctrl,
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
