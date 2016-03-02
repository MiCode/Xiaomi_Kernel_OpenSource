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
 */

#ifndef __DP_UTIL_H__
#define __DP_UTIL_H__

#include "mdss_dp.h"

/* DP_TX Registers */
#define	DP_HW_VERSION				(0x00000000)
#define	DP_SW_RESET				(0x00000010)
#define	DP_PHY_CTRL				(0x00000014)
#define	DP_CLK_CTRL				(0x00000018)
#define	DP_CLK_ACTIVE				(0x0000001C)
#define	DP_INTR_STATUS				(0x00000020)
#define	DP_INTR_STATUS2				(0x00000024)
#define	DP_INTR_STATUS3				(0x00000028)

#define	DP_DP_HPD_CTRL				(0x00000200)
#define	DP_DP_HPD_INT_STATUS			(0x00000204)
#define	DP_DP_HPD_INT_ACK			(0x00000208)
#define	DP_DP_HPD_INT_MASK			(0x0000020C)
#define	DP_DP_HPD_REFTIMER			(0x00000218)
#define	DP_DP_HPD_EVENT_TIME_0			(0x0000021C)
#define	DP_DP_HPD_EVENT_TIME_1			(0x00000220)
#define	DP_AUX_CTRL				(0x00000230)
#define	DP_AUX_DATA				(0x00000234)
#define	DP_AUX_TRANS_CTRL			(0x00000238)
#define	DP_AUX_STATUS				(0x00000244)

#define	DP_INTERRUPT_TRANS_NUM			(0x000002A0)

#define	DP_MAINLINK_CTRL			(0x00000400)
#define	DP_STATE_CTRL				(0x00000404)
#define	DP_CONFIGURATION_CTRL			(0x00000408)
#define	DP_SOFTWARE_MVID			(0x00000410)
#define	DP_SOFTWARE_NVID			(0x00000418)
#define	DP_TOTAL_HOR_VER			(0x0000041C)
#define	DP_START_HOR_VER_FROM_SYNC		(0x00000420)
#define	DP_HSYNC_VSYNC_WIDTH_POLARITY		(0x00000424)
#define	DP_ACTIVE_HOR_VER			(0x00000428)

#define	DP_LOGICAL2PHYSCIAL_LANE_MAPPING	(0x00000438)

#define	DP_MAINLINK_READY			(0x00000440)

/*DP PHY Register offsets */
#define DP_PHY_REVISION_ID0                     (0x00000000)
#define DP_PHY_REVISION_ID1                     (0x00000004)
#define DP_PHY_REVISION_ID2                     (0x00000008)
#define DP_PHY_REVISION_ID3                     (0x0000000C)

#define DP_PHY_CFG                              (0x00000010)
#define DP_PHY_PD_CTL                           (0x00000014)
#define DP_PHY_MODE                             (0x00000018)

#define DP_PHY_AUX_CFG0                         (0x0000001C)
#define DP_PHY_AUX_CFG1                         (0x00000020)
#define DP_PHY_AUX_CFG2                         (0x00000024)
#define DP_PHY_AUX_CFG3                         (0x00000028)
#define DP_PHY_AUX_CFG4                         (0x0000002C)
#define DP_PHY_AUX_CFG5                         (0x00000030)
#define DP_PHY_AUX_CFG6                         (0x00000034)
#define DP_PHY_AUX_CFG7                         (0x00000038)
#define DP_PHY_AUX_CFG8                         (0x0000003C)
#define DP_PHY_AUX_CFG9                         (0x00000040)
#define DP_PHY_AUX_INTERRUPT_MASK               (0x00000044)
#define DP_PHY_AUX_INTERRUPT_CLEAR              (0x00000048)

#define TCSR_USB3_DP_PHYMODE			0x48

struct lane_mapping {
	char lane0;
	char lane1;
	char lane2;
	char lane3;
};

u32 mdss_dp_get_ctrl_hw_version(struct dss_io_data *ctrl_io);
u32 mdss_dp_get_phy_hw_version(struct dss_io_data *phy_io);
void mdss_dp_aux_reset(struct dss_io_data *ctrl_io);
void mdss_dp_mainlink_reset(struct dss_io_data *ctrl_io);
void mdss_dp_phy_reset(struct dss_io_data *ctrl_io);
void mdss_dp_switch_usb3_phy_to_dp_mode(struct dss_io_data *tcsr_reg_io);
void mdss_dp_assert_phy_reset(struct dss_io_data *ctrl_io, bool assert);
void mdss_dp_phy_aux_setup(struct dss_io_data *phy_io);
void mdss_dp_hpd_configure(struct dss_io_data *ctrl_io, bool enable);
void mdss_dp_aux_ctrl(struct dss_io_data *ctrl_io, bool enable);
void mdss_dp_mainlink_ctrl(struct dss_io_data *ctrl_io, bool enable);
void mdss_dp_ctrl_lane_mapping(struct dss_io_data *ctrl_io,
					struct lane_mapping l_map);
int mdss_dp_mainlink_ready(struct mdss_dp_drv_pdata *dp, u32 which);
void mdss_dp_timing_cfg(struct dss_io_data *ctrl_io,
				struct mdss_panel_info *pinfo);
void mdss_dp_configuration_ctrl(struct dss_io_data *ctrl_io, u32 data);
void mdss_dp_state_ctrl(struct dss_io_data *ctrl_io, u32 data);
int mdss_dp_irq_setup(struct mdss_dp_drv_pdata *dp_drv);
void mdss_dp_irq_enable(struct mdss_dp_drv_pdata *dp_drv);
void mdss_dp_irq_disable(struct mdss_dp_drv_pdata *dp_drv);
void mdss_dp_sw_mvid_nvid(struct dss_io_data *ctrl_io);

#endif /* __DP_UTIL_H__ */
