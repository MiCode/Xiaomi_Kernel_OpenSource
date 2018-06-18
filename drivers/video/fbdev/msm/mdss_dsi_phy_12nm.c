/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#include <linux/iopoll.h>
#include "mdss_dsi_phy.h"

#define T_TA_GO_TIM_COUNT                    0x014
#define T_TA_SURE_TIM_COUNT                  0x018
#define HSTX_DRIV_INDATA_CTRL_CLKLANE        0x0c0
#define HSTX_DATAREV_CTRL_CLKLANE            0x0d4
#define HSTX_DRIV_INDATA_CTRL_LANE0          0x100
#define HSTX_READY_DLY_DATA_REV_CTRL_LANE0   0x114
#define HSTX_DRIV_INDATA_CTRL_LANE1          0x140
#define HSTX_READY_DLY_DATA_REV_CTRL_LANE1   0x154
#define HSTX_CLKLANE_REQSTATE_TIM_CTRL       0x180
#define HSTX_CLKLANE_HS0STATE_TIM_CTRL       0x188
#define HSTX_CLKLANE_TRALSTATE_TIM_CTRL      0x18c
#define HSTX_CLKLANE_EXITSTATE_TIM_CTRL      0x190
#define HSTX_CLKLANE_CLKPOSTSTATE_TIM_CTRL   0x194
#define HSTX_DATALANE_REQSTATE_TIM_CTRL      0x1c0
#define HSTX_DATALANE_HS0STATE_TIM_CTRL      0x1c8
#define HSTX_DATALANE_TRAILSTATE_TIM_CTRL    0x1cc
#define HSTX_DATALANE_EXITSTATE_TIM_CTRL     0x1d0
#define HSTX_DRIV_INDATA_CTRL_LANE2          0x200
#define HSTX_READY_DLY_DATA_REV_CTRL_LANE2   0x214
#define HSTX_READY_DLY_DATA_REV_CTRL_LANE3   0x254
#define HSTX_DRIV_INDATA_CTRL_LANE3          0x240
#define CTRL0                                0x3e8
#define SYS_CTRL                             0x3f0
#define REQ_DLY                              0x3fc

#define DSI_PHY_W32(b, off, val) MIPI_OUTP((b) + (off), (val))
#define DSI_PHY_R32(b, off) MIPI_INP((b) + (off))

int mdss_dsi_12nm_phy_regulator_enable(struct mdss_dsi_ctrl_pdata *ctrl)
{
	/* Nothing to be done for 12nm PHY */
	return 0;
}

int mdss_dsi_12nm_phy_regulator_disable(struct mdss_dsi_ctrl_pdata *ctrl)
{
	/* Nothing to be done for 12nm PHY */
	return 0;
}

int mdss_dsi_12nm_phy_config(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_dsi_phy_ctrl *pd =
		&(((ctrl->panel_data).panel_info.mipi).dsi_phy_db);

	/* CTRL0: CFG_CLK_EN */
	DSI_PHY_W32(ctrl->phy_io.base, CTRL0, BIT(0));

	/* DSI PHY clock lane timings */
	DSI_PHY_W32(ctrl->phy_io.base, HSTX_CLKLANE_HS0STATE_TIM_CTRL,
		(pd->timing_12nm[0] | BIT(7)));
	DSI_PHY_W32(ctrl->phy_io.base, HSTX_CLKLANE_TRALSTATE_TIM_CTRL,
		(pd->timing_12nm[1] | BIT(6)));
	DSI_PHY_W32(ctrl->phy_io.base, HSTX_CLKLANE_CLKPOSTSTATE_TIM_CTRL,
		(pd->timing_12nm[2] | BIT(6)));
	DSI_PHY_W32(ctrl->phy_io.base, HSTX_CLKLANE_REQSTATE_TIM_CTRL,
		pd->timing_12nm[3]);
	DSI_PHY_W32(ctrl->phy_io.base, HSTX_CLKLANE_EXITSTATE_TIM_CTRL,
		(pd->timing_12nm[7] | BIT(6) | BIT(7)));

	/* DSI PHY data lane timings */
	DSI_PHY_W32(ctrl->phy_io.base, HSTX_DATALANE_HS0STATE_TIM_CTRL,
		(pd->timing_12nm[4] | BIT(7)));
	DSI_PHY_W32(ctrl->phy_io.base, HSTX_DATALANE_TRAILSTATE_TIM_CTRL,
		(pd->timing_12nm[5] | BIT(6)));
	DSI_PHY_W32(ctrl->phy_io.base, HSTX_DATALANE_REQSTATE_TIM_CTRL,
		pd->timing_12nm[6]);
	DSI_PHY_W32(ctrl->phy_io.base, HSTX_DATALANE_EXITSTATE_TIM_CTRL,
		(pd->timing_12nm[7] | BIT(6) | BIT(7)));

	DSI_PHY_W32(ctrl->phy_io.base, T_TA_GO_TIM_COUNT, 0x03);
	DSI_PHY_W32(ctrl->phy_io.base, T_TA_SURE_TIM_COUNT, 0x01);
	DSI_PHY_W32(ctrl->phy_io.base, REQ_DLY, 0x85);

	/* DSI lane control registers */
	DSI_PHY_W32(ctrl->phy_io.base,
	HSTX_READY_DLY_DATA_REV_CTRL_LANE0, 0x00);
	DSI_PHY_W32(ctrl->phy_io.base,
		HSTX_READY_DLY_DATA_REV_CTRL_LANE1, 0x00);
	DSI_PHY_W32(ctrl->phy_io.base,
		HSTX_READY_DLY_DATA_REV_CTRL_LANE2, 0x00);
	DSI_PHY_W32(ctrl->phy_io.base,
		HSTX_READY_DLY_DATA_REV_CTRL_LANE3, 0x00);
	DSI_PHY_W32(ctrl->phy_io.base, HSTX_DATAREV_CTRL_CLKLANE, 0x00);
	wmb(); /* make sure DSI PHY registers are programmed */

	return 0;
}

int mdss_dsi_12nm_phy_shutdown(struct mdss_dsi_ctrl_pdata *ctrl)
{
	DSI_PHY_W32(ctrl->phy_io.base, SYS_CTRL, BIT(0) | BIT(3));
	wmb(); /* make sure DSI PHY is disabled */
	mdss_dsi_ctrl_phy_reset(ctrl);
	return 0;
}

void mdss_dsi_12nm_phy_hstx_drv_ctrl(
	struct mdss_dsi_ctrl_pdata *ctrl, bool enable)
{
	u32 data = 0;

	if (enable)
		data = BIT(2) | BIT(3);

	DSI_PHY_W32(ctrl->phy_io.base, HSTX_DRIV_INDATA_CTRL_CLKLANE, data);
	DSI_PHY_W32(ctrl->phy_io.base, HSTX_DRIV_INDATA_CTRL_LANE0, data);
	DSI_PHY_W32(ctrl->phy_io.base, HSTX_DRIV_INDATA_CTRL_LANE1, data);
	DSI_PHY_W32(ctrl->phy_io.base, HSTX_DRIV_INDATA_CTRL_LANE2, data);
	DSI_PHY_W32(ctrl->phy_io.base, HSTX_DRIV_INDATA_CTRL_LANE3, data);
	wmb(); /* make sure DSI PHY registers are programmed */
}
