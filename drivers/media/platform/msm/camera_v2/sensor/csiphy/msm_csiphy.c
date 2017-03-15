/* Copyright (c) 2011-2017, The Linux Foundation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/irqreturn.h>
#include "msm_csiphy.h"
#include "msm_sd.h"
#include "include/msm_csiphy_2_0_hwreg.h"
#include "include/msm_csiphy_2_2_hwreg.h"
#include "include/msm_csiphy_3_0_hwreg.h"
#include "include/msm_csiphy_3_1_hwreg.h"
#include "include/msm_csiphy_3_2_hwreg.h"
#include "include/msm_csiphy_3_4_2_hwreg.h"
#include "include/msm_csiphy_3_5_hwreg.h"
#include "include/msm_csiphy_5_0_hwreg.h"
#include "include/msm_csiphy_5_0_1_hwreg.h"
#include "cam_hw_ops.h"

#define DBG_CSIPHY 0
#define SOF_DEBUG_ENABLE 1
#define SOF_DEBUG_DISABLE 0

#define V4L2_IDENT_CSIPHY                        50003
#define CSIPHY_VERSION_V22                        0x01
#define CSIPHY_VERSION_V20                        0x00
#define CSIPHY_VERSION_V30                        0x10
#define CSIPHY_VERSION_V31                        0x31
#define CSIPHY_VERSION_V32                        0x32
#define CSIPHY_VERSION_V342                       0x342
#define CSIPHY_VERSION_V35                        0x35
#define CSIPHY_VERSION_V50                        0x500
#define CSIPHY_VERSION_V501                       0x501
#define MSM_CSIPHY_DRV_NAME                      "msm_csiphy"
#define CLK_LANE_OFFSET                             1
#define NUM_LANES_OFFSET                            4
#define CLOCK_LANE                                  0x02

#define CSI_3PHASE_HW                               1
#define MAX_DPHY_DATA_LN                            4
#define CLOCK_OFFSET                              0x700
#define CSIPHY_SOF_DEBUG_COUNT                      2

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

static struct v4l2_file_operations msm_csiphy_v4l2_subdev_fops;

static void msm_csiphy_write_settings(
	struct csiphy_device *csiphy_dev,
	struct csiphy_settings_t csiphy_settings)
{
	int i = 0;

	for (i = 0; i < MAX_CSIPHY_SETTINGS; i++) {
		if (csiphy_settings.settings[i].addr == 0 &&
			csiphy_settings.settings[i].data == 0)
			break;

		msm_camera_io_w(csiphy_settings.settings[i].data,
			csiphy_dev->base + csiphy_settings.settings[i].addr);
	}
}

static void msm_csiphy_cphy_irq_config(
	struct csiphy_device *csiphy_dev,
	struct msm_camera_csiphy_params *csiphy_params)
{
	void __iomem *csiphybase;

	csiphybase = csiphy_dev->base;
	msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl11.data,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl11.addr);
	msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl12.data,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl12.addr);
	msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl13.data,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl13.addr);
	msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl14.data,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl14.addr);
	msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl15.data,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl15.addr);
	msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl16.data,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl16.addr);
	msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl17.data,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl17.addr);
	msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl18.data,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl18.addr);
	msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl19.data,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl19.addr);
	msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl20.data,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl20.addr);
	msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl21.data,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl21.addr);
}

static int msm_csiphy_3phase_lane_config(
	struct csiphy_device *csiphy_dev,
	struct msm_camera_csiphy_params *csiphy_params)
{
	uint8_t i = 0;
	uint16_t lane_mask = 0, lane_enable = 0, temp;
	void __iomem *csiphybase;

	csiphybase = csiphy_dev->base;
	lane_mask = csiphy_params->lane_mask & 0x7;
	while (lane_mask != 0) {
		temp = (i << 1)+1;
		lane_enable |= ((lane_mask & 0x1) << temp);
		lane_mask >>= 1;
		i++;
	}
	msm_camera_io_w(lane_enable,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl5.addr);
	msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl6.data,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl6.addr);
	lane_mask = csiphy_params->lane_mask & 0x7;
	i = 0;
	while (lane_mask & 0x7) {
		if (!(lane_mask & 0x1)) {
			i++;
			lane_mask >>= 1;
			continue;
		}

		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl21.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl21.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl23.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl23.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl26.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl26.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl27.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl27.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl1.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl1.addr + 0x200*i);
		msm_camera_io_w(((csiphy_params->settle_cnt >> 8) & 0xff),
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl2.addr + 0x200*i);
		msm_camera_io_w((csiphy_params->settle_cnt & 0xff),
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl3.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl5.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl5.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl6.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl6.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl7.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl7.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl8.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl8.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl9.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl9.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl10.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl10.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl11.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl11.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl12.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl12.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl15.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl15.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl16.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl16.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl17.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl17.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl18.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl18.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl19.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl19.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl23.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl23.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl24.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl24.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl28.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl28.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl29.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl29.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl30.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl30.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl33.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl33.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl34.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl34.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl35.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl35.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl36.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl36.addr + 0x200*i);

		if (ULPM_WAKE_UP_TIMER_MODE == 0x22) {
			msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
				mipi_csiphy_3ph_lnn_ctrl51.data,
				csiphybase + csiphy_dev->ctrl_reg->
				csiphy_3ph_reg.mipi_csiphy_3ph_lnn_ctrl51.addr +
				0x200*i);
		}
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl25.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl25.addr + 0x200*i);

		lane_mask >>= 1;
		i++;
	}
	if (csiphy_params->combo_mode == 1) {
		msm_camera_io_w(0x2,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_cmn_ctrl7.addr);
	} else {
		msm_camera_io_w(0x6,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_cmn_ctrl7.addr);
	}
	/* Delay for stabilizing the regulator*/
	usleep_range(10, 15);
	msm_csiphy_cphy_irq_config(csiphy_dev, csiphy_params);
	return 0;
}

static int msm_csiphy_3phase_lane_config_v50(
	struct csiphy_device *csiphy_dev,
	struct msm_camera_csiphy_params *csiphy_params)
{
	uint8_t i = 0;
	uint16_t lane_mask = 0, lane_enable = 0, temp;
	void __iomem *csiphybase;

	csiphybase = csiphy_dev->base;
	lane_mask = csiphy_params->lane_mask & 0x7;
	while (lane_mask != 0) {
		temp = (i << 1)+1;
		lane_enable |= ((lane_mask & 0x1) << temp);
		lane_mask >>= 1;
		i++;
	}
	msm_camera_io_w(lane_enable,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl5.addr);
	msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl6.data,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl6.addr);
	msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl7_cphy.data,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl7_cphy.addr);

	lane_mask = csiphy_params->lane_mask & 0x7;
	i = 0;
	while (lane_mask & 0x7) {
		if (!(lane_mask & 0x1)) {
			i++;
			lane_mask >>= 1;
			continue;
		}

		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl23.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl23.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl26.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl26.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl27.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl27.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl1.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl1.addr + 0x200*i);
		msm_camera_io_w((csiphy_params->settle_cnt & 0xff),
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl3.addr + 0x200*i);
		msm_camera_io_w(((csiphy_params->settle_cnt >> 8) & 0xff),
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl2.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl5.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl5.addr + 0x200*i);
		msm_camera_io_w(((csiphy_params->settle_cnt >> 8) & 0xff),
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl20.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl6.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl6.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl7.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl7.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl8.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl8.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl9.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl9.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl10.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl10.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl11.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl11.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl17.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl17.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl24.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl24.addr + 0x200*i);
		if (ULPM_WAKE_UP_TIMER_MODE == 0x22) {
			msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
				mipi_csiphy_3ph_lnn_ctrl51.data,
				csiphybase + csiphy_dev->ctrl_reg->
				csiphy_3ph_reg.mipi_csiphy_3ph_lnn_ctrl51.addr +
				0x200*i);
		}
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl25.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl25.addr + 0x200*i);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl55.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_lnn_ctrl55.addr + 0x200*i);

		lane_mask >>= 1;
		i++;
	}
	/* Delay for stabilizing the regulator*/
	usleep_range(10, 15);
	msm_csiphy_cphy_irq_config(csiphy_dev, csiphy_params);
	return 0;
}

static int msm_csiphy_2phase_lane_config(
	struct csiphy_device *csiphy_dev,
	struct msm_camera_csiphy_params *csiphy_params)
{
	uint32_t val = 0, lane_enable = 0, clk_lane, mask = 1;
	uint16_t lane_mask = 0, i = 0, offset;
	void __iomem *csiphybase;

	csiphybase = csiphy_dev->base;
	lane_mask = csiphy_params->lane_mask & 0x1f;
	for (i = 0; i < MAX_DPHY_DATA_LN; i++) {
		if (mask == 0x2) {
			if (lane_mask & mask)
				lane_enable |= 0x80;
			i--;
		} else if (lane_mask & mask)
			lane_enable |= 0x1 << (i<<1);
		mask <<= 1;
	}
	CDBG("%s:%d lane_enable: %d\n", __func__, __LINE__, lane_enable);

	msm_camera_io_w(lane_enable,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl5.addr);
	msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl6.data,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl6.addr);

	for (i = 0, mask = 0x1; i < MAX_DPHY_DATA_LN; i++) {
		if (!(lane_mask & mask)) {
			if (mask == 0x2)
				i--;
			mask <<= 0x1;
			continue;
		}
		if (mask == 0x2) {
			val = 4;
			offset = CLOCK_OFFSET;
			clk_lane = 1;
			i--;
		} else {
			offset = 0x200*i;
			val = 0;
			clk_lane = 0;
		}

		if (csiphy_params->combo_mode == 1) {
			val |= 0xA;
			if (mask == csiphy_dev->ctrl_reg->
				csiphy_reg.combo_clk_mask) {
				val |= 0x4;
				clk_lane = 1;
			}
		}
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_2ph_lnn_cfg7.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_2ph_lnn_cfg7.addr + offset);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_2ph_lnn_cfg6.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_2ph_lnn_cfg6.addr + offset);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_2ph_lnn_cfg8.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_2ph_lnn_cfg8.addr + offset);
		msm_camera_io_w(val, csiphybase +
			csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_2ph_lnn_misc1.addr + offset);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_2ph_lnn_ctrl15.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_2ph_lnn_ctrl15.addr + offset);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_2ph_lnn_cfg2.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_2ph_lnn_cfg2.addr + offset);

		msm_camera_io_w((csiphy_params->settle_cnt & 0xFF),
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_2ph_lnn_cfg3.addr + offset);

		if (clk_lane == 1) {
			msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
				mipi_csiphy_3ph_lnck_cfg1.data, csiphybase +
				csiphy_dev->ctrl_reg->csiphy_3ph_reg.
				mipi_csiphy_3ph_lnck_cfg1.addr);

			msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_cfg4.data, csiphybase +
				csiphy_dev->ctrl_reg->csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_cfg4.addr + offset);
		} else {
			msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_cfg1.data,
				csiphybase +
				csiphy_dev->ctrl_reg->csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_cfg1.addr + offset);
		}
		if (csiphy_dev->hw_version == CSIPHY_VERSION_V342 &&
			csiphy_params->combo_mode == 1) {
			msm_camera_io_w(0x52,
				csiphybase +
				csiphy_dev->ctrl_reg->csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_cfg5.addr + offset);
		} else {
			msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_cfg5.data,
				csiphybase +
				csiphy_dev->ctrl_reg->csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_cfg5.addr + offset);
		}
		if (clk_lane == 1 &&
			csiphy_dev->hw_version == CSIPHY_VERSION_V342) {
			msm_camera_io_w(0x1f,
				csiphybase +
				csiphy_dev->ctrl_reg->csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_cfg9.addr + offset);
		} else {
			msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_cfg9.data,
				csiphybase +
				csiphy_dev->ctrl_reg->csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_cfg9.addr + offset);
		}
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_2ph_lnn_test_imp.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_2ph_lnn_test_imp.addr + offset);
		if (csiphy_dev->hw_version == CSIPHY_VERSION_V342) {
			msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_ctrl5.data,
				csiphybase +
				csiphy_dev->ctrl_reg->csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_ctrl5.addr + offset);
		}
		mask <<= 1;
	}
	if (csiphy_dev->hw_version == CSIPHY_VERSION_V342 &&
		csiphy_params->combo_mode != 1) {
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_cmn_ctrl0.data,
			csiphy_dev->base + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_cmn_ctrl0.addr);
	}
	msm_csiphy_cphy_irq_config(csiphy_dev, csiphy_params);
	return 0;
}

static int msm_csiphy_2phase_lane_config_v50(
	struct csiphy_device *csiphy_dev,
	struct msm_camera_csiphy_params *csiphy_params)
{
	uint32_t lane_enable = 0, mask = 1;
	uint16_t lane_mask = 0, i = 0, offset;
	void __iomem *csiphybase;

	csiphybase = csiphy_dev->base;
	lane_mask = csiphy_params->lane_mask & 0x1f;

	lane_enable = msm_camera_io_r(csiphybase +
		csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl5.addr);

    /* write settle count and lane_enable */
	for (i = 0; i < MAX_DPHY_DATA_LN; i++) {
		if (mask == 0x2) {
			if (lane_mask & mask)
				lane_enable |= 0x80;
			i--;
			offset = CLOCK_OFFSET;
		} else if (lane_mask & mask) {
			lane_enable |= 0x1 << (i<<1);
			offset = 0x200*i;
		}

		if (lane_mask & mask)
			msm_camera_io_w((csiphy_params->settle_cnt & 0xFF),
				csiphybase + csiphy_dev->ctrl_reg->
				csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_cfg2.addr + offset);
		mask <<= 1;
	}
	CDBG("%s:%d lane_enable: 0x%x\n", __func__, __LINE__, lane_enable);

	msm_camera_io_w(lane_enable,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl5.addr);

    /* write mode specific settings */
	if (csiphy_params->combo_mode == 1)
		msm_csiphy_write_settings(csiphy_dev,
			csiphy_dev->ctrl_reg->csiphy_combo_mode_settings);
	else {
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_cmn_ctrl6.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_cmn_ctrl6.addr);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_cmn_ctrl7.data,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_cmn_ctrl7.addr);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_2ph_lnck_ctrl10.data,
			csiphybase +
			csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_2ph_lnck_ctrl10.addr);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_2ph_lnck_ctrl3.data, csiphybase +
			csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_2ph_lnck_ctrl3.addr);

		for (i = 0, mask = 0x1; i < MAX_DPHY_DATA_LN; i++) {
			if (!(lane_mask & mask)) {
				if (mask == 0x2)
					i--;
				mask <<= 0x1;
				continue;
			}
			if (mask == 0x2) {
				offset = CLOCK_OFFSET;
				i--;
			} else {
				offset = 0x200*i;
			}

			msm_camera_io_w(csiphy_dev->ctrl_reg->
				csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_ctrl11.data,
				csiphybase + csiphy_dev->ctrl_reg->
				csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_ctrl11.addr + offset);
			msm_camera_io_w(csiphy_dev->ctrl_reg->
				csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_ctrl13.data,
				csiphybase + csiphy_dev->ctrl_reg->
				csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_ctrl13.addr + offset);
			msm_camera_io_w(csiphy_dev->ctrl_reg->
				csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_cfg7.data,
				csiphybase + csiphy_dev->ctrl_reg->
				csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_cfg7.addr + offset);
			msm_camera_io_w(csiphy_dev->ctrl_reg->
				csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_cfg5.data,
				csiphybase + csiphy_dev->ctrl_reg->
				csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_cfg5.addr + offset);
			msm_camera_io_w(csiphy_dev->ctrl_reg->
				csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_ctrl15.data,
				csiphybase + csiphy_dev->ctrl_reg->
				csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_ctrl15.addr + offset);
			if (mask == CLOCK_LANE)
				msm_camera_io_w(csiphy_dev->ctrl_reg->
					csiphy_3ph_reg.
					mipi_csiphy_2ph_lnck_ctrl0.data,
					csiphybase + csiphy_dev->ctrl_reg->
					csiphy_3ph_reg.
					mipi_csiphy_2ph_lnck_ctrl0.addr);
			else
				msm_camera_io_w(csiphy_dev->ctrl_reg->
					csiphy_3ph_reg.
					mipi_csiphy_2ph_lnn_ctrl0.data,
					csiphybase + csiphy_dev->ctrl_reg->
					csiphy_3ph_reg.
					mipi_csiphy_2ph_lnn_ctrl0.addr +
					offset);
			msm_camera_io_w(csiphy_dev->ctrl_reg->
				csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_cfg1.data,
				csiphybase + csiphy_dev->ctrl_reg->
				csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_cfg1.addr + offset);
			msm_camera_io_w(csiphy_dev->ctrl_reg->
				csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_cfg4.data, csiphybase +
				csiphy_dev->ctrl_reg->csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_cfg4.addr + offset);
			msm_camera_io_w(csiphy_dev->ctrl_reg->
				csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_ctrl14.data,
				csiphybase + csiphy_dev->ctrl_reg->
				csiphy_3ph_reg.
				mipi_csiphy_2ph_lnn_ctrl14.addr + offset);
			mask <<= 1;
		}
	}
	msm_csiphy_cphy_irq_config(csiphy_dev, csiphy_params);
	return 0;
}

static int msm_csiphy_lane_config(struct csiphy_device *csiphy_dev,
	struct msm_camera_csiphy_params *csiphy_params)
{
	int rc = 0;
	int j = 0, curr_lane = 0;
	uint32_t val = 0;
	long clk_rate = 0;
	uint8_t lane_cnt = 0;
	uint16_t lane_mask = 0;
	void __iomem *csiphybase;
	uint8_t csiphy_id = csiphy_dev->pdev->id;
	int32_t lane_val = 0, lane_right = 0, num_lanes = 0;
	int ratio = 1;

	csiphybase = csiphy_dev->base;
	if (!csiphybase) {
		pr_err("%s: csiphybase NULL\n", __func__);
		return -EINVAL;
	}

	csiphy_dev->lane_mask[csiphy_id] |= csiphy_params->lane_mask;
	lane_mask = csiphy_dev->lane_mask[csiphy_id];
	lane_cnt = csiphy_params->lane_cnt;
	if (csiphy_params->lane_cnt < 1 || csiphy_params->lane_cnt > 4) {
		pr_err("%s: unsupported lane cnt %d\n",
			__func__, csiphy_params->lane_cnt);
		return rc;
	}

	clk_rate = (csiphy_params->csiphy_clk > 0)
			? csiphy_params->csiphy_clk :
			csiphy_dev->csiphy_max_clk;
	clk_rate = msm_camera_clk_set_rate(&csiphy_dev->pdev->dev,
		csiphy_dev->csiphy_clk[csiphy_dev->csiphy_clk_index],
		clk_rate);
	if (clk_rate < 0) {
		pr_err("csiphy_clk_set_rate failed\n");
		return -EINVAL;
	}

	if (clk_rate < csiphy_dev->csiphy_max_clk &&
		clk_rate > 0) {
		ratio = csiphy_dev->csiphy_max_clk/clk_rate;
		csiphy_params->settle_cnt = csiphy_params->settle_cnt/ratio;
	}
	CDBG("%s csiphy_params, mask = 0x%x cnt = %d\n",
		__func__,
		csiphy_params->lane_mask,
		csiphy_params->lane_cnt);
	CDBG("%s csiphy_params, settle cnt = 0x%x csid %d\n",
		__func__, csiphy_params->settle_cnt,
		csiphy_params->csid_core);

	if (csiphy_dev->hw_version >= CSIPHY_VERSION_V30 &&
		csiphy_dev->clk_mux_base != NULL &&
		csiphy_dev->hw_version < CSIPHY_VERSION_V50) {
		val = msm_camera_io_r(csiphy_dev->clk_mux_base);
		if (csiphy_params->combo_mode &&
			(csiphy_params->lane_mask & 0x18) == 0x18) {
			val &= ~0xf0;
			val |= csiphy_params->csid_core << 4;
		} else {
			val &= ~0xf;
			val |= csiphy_params->csid_core;
		}
		msm_camera_io_w(val, csiphy_dev->clk_mux_base);
		CDBG("%s clk mux addr %pK val 0x%x\n", __func__,
			csiphy_dev->clk_mux_base, val);
		/* ensure write is done */
		mb();
	}

	csiphy_dev->csi_3phase = csiphy_params->csi_3phase;
	if (csiphy_dev->csiphy_3phase == CSI_3PHASE_HW) {
		if (csiphy_dev->csi_3phase == 1) {
			rc = msm_camera_clk_enable(&csiphy_dev->pdev->dev,
				csiphy_dev->csiphy_3p_clk_info,
				csiphy_dev->csiphy_3p_clk, 2, true);
			if (csiphy_dev->hw_dts_version >= CSIPHY_VERSION_V50)
				rc = msm_csiphy_3phase_lane_config_v50(
					csiphy_dev, csiphy_params);
			else
				rc = msm_csiphy_3phase_lane_config(csiphy_dev,
					csiphy_params);
			csiphy_dev->num_irq_registers = 20;
		} else {
			if (csiphy_dev->hw_dts_version >= CSIPHY_VERSION_V50)
				rc = msm_csiphy_2phase_lane_config_v50(
					csiphy_dev, csiphy_params);
			else
				rc = msm_csiphy_2phase_lane_config(csiphy_dev,
					csiphy_params);
			csiphy_dev->num_irq_registers = 11;
		}
		if (rc < 0) {
			pr_err("%s:%d: Error in setting lane configuration\n",
				__func__, __LINE__);
		}
		return rc;
	}

	msm_camera_io_w(0x1, csiphybase + csiphy_dev->ctrl_reg->
		csiphy_reg.mipi_csiphy_glbl_t_init_cfg0_addr);
	msm_camera_io_w(0x1, csiphybase + csiphy_dev->ctrl_reg->
		csiphy_reg.mipi_csiphy_t_wakeup_cfg0_addr);

	if (csiphy_dev->hw_version < CSIPHY_VERSION_V30) {
		val = 0x3;
		msm_camera_io_w((lane_mask << 2) | val,
				csiphybase +
				csiphy_dev->ctrl_reg->
				csiphy_reg.mipi_csiphy_glbl_pwr_cfg_addr);
		msm_camera_io_w(0x10, csiphybase +
			csiphy_dev->ctrl_reg->csiphy_reg.
			mipi_csiphy_lnck_cfg2_addr);
		msm_camera_io_w(csiphy_params->settle_cnt,
			csiphybase +
			csiphy_dev->ctrl_reg->csiphy_reg.
			mipi_csiphy_lnck_cfg3_addr);
		msm_camera_io_w(0x24,
			csiphybase + csiphy_dev->ctrl_reg->
			csiphy_reg.mipi_csiphy_interrupt_mask0_addr);
		msm_camera_io_w(0x24,
			csiphybase + csiphy_dev->ctrl_reg->
			csiphy_reg.mipi_csiphy_interrupt_clear0_addr);
	} else {
		val = 0x1;
		msm_camera_io_w((lane_mask << 1) | val,
				csiphybase +
				csiphy_dev->ctrl_reg->
				csiphy_reg.mipi_csiphy_glbl_pwr_cfg_addr);
		msm_camera_io_w(csiphy_params->combo_mode <<
			csiphy_dev->ctrl_reg->csiphy_reg.
			mipi_csiphy_mode_config_shift,
			csiphybase +
			csiphy_dev->ctrl_reg->csiphy_reg.
			mipi_csiphy_glbl_reset_addr);
	}

	lane_mask &= 0x1f;
	while (lane_mask & 0x1f) {
		if (!(lane_mask & 0x1)) {
			j++;
			lane_mask >>= 1;
			continue;
		}
		msm_camera_io_w(0x10,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_reg.
			mipi_csiphy_lnn_cfg2_addr + 0x40*j);
		msm_camera_io_w(csiphy_params->settle_cnt,
			csiphybase + csiphy_dev->ctrl_reg->csiphy_reg.
			mipi_csiphy_lnn_cfg3_addr + 0x40*j);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_reg.
			mipi_csiphy_interrupt_mask_val, csiphybase +
			csiphy_dev->ctrl_reg->csiphy_reg.
			mipi_csiphy_interrupt_mask_addr + 0x4*j);
		msm_camera_io_w(csiphy_dev->ctrl_reg->csiphy_reg.
			mipi_csiphy_interrupt_mask_val, csiphybase +
			csiphy_dev->ctrl_reg->csiphy_reg.
			mipi_csiphy_interrupt_clear_addr + 0x4*j);
		if (csiphy_dev->is_3_1_20nm_hw == 1) {
			if (j > CLK_LANE_OFFSET) {
				lane_right = 0x8;
				num_lanes = (lane_cnt - curr_lane)
					<< NUM_LANES_OFFSET;
				if (lane_cnt < curr_lane) {
					pr_err("%s: Lane_cnt is less than curr_lane number\n",
						__func__);
					return -EINVAL;
				}
				lane_val = lane_right|num_lanes;
			} else if (j == 1) {
				lane_val = 0x4;
			}
			if (csiphy_params->combo_mode == 1) {
				/*
				* In the case of combo mode, the clock is always
				* 4th lane for the second sensor.
				* So check whether the sensor is of one lane
				* sensor and curr_lane for 0.
				*/
				if (curr_lane == 0 &&
					((csiphy_params->lane_mask &
						0x18) == 0x18))
					lane_val = 0x4;
			}
			msm_camera_io_w(lane_val, csiphybase +
				csiphy_dev->ctrl_reg->csiphy_reg.
				mipi_csiphy_lnn_misc1_addr + 0x40*j);
			msm_camera_io_w(0x17, csiphybase +
				csiphy_dev->ctrl_reg->csiphy_reg.
				mipi_csiphy_lnn_test_imp + 0x40*j);
			curr_lane++;
		}
		j++;
		lane_mask >>= 1;
	}
	return rc;
}

void msm_csiphy_disable_irq(
	struct csiphy_device *csiphy_dev)
{
	void __iomem *csiphybase;

	csiphybase = csiphy_dev->base;
	msm_camera_io_w(0,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl11.addr);
	msm_camera_io_w(0,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl12.addr);
	msm_camera_io_w(0,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl13.addr);
	msm_camera_io_w(0,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl14.addr);
	msm_camera_io_w(0,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl15.addr);
	msm_camera_io_w(0,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl16.addr);
	msm_camera_io_w(0,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl17.addr);
	msm_camera_io_w(0,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl18.addr);
	msm_camera_io_w(0,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl19.addr);
	msm_camera_io_w(0,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl20.addr);
	msm_camera_io_w(0,
		csiphybase + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl21.addr);
}

static irqreturn_t msm_csiphy_irq(int irq_num, void *data)
{
	uint32_t irq;
	int i;
	struct csiphy_device *csiphy_dev = data;

	if (csiphy_dev->csiphy_sof_debug == SOF_DEBUG_ENABLE) {
		if (csiphy_dev->csiphy_sof_debug_count < CSIPHY_SOF_DEBUG_COUNT)
			csiphy_dev->csiphy_sof_debug_count++;
		else {
			msm_csiphy_disable_irq(csiphy_dev);
			return IRQ_HANDLED;
		}
	}

	for (i = 0; i < csiphy_dev->num_irq_registers; i++) {
		irq = msm_camera_io_r(
			csiphy_dev->base +
			csiphy_dev->ctrl_reg->csiphy_reg.
			mipi_csiphy_interrupt_status0_addr + 0x4*i);
		msm_camera_io_w(irq,
			csiphy_dev->base +
			csiphy_dev->ctrl_reg->csiphy_reg.
			mipi_csiphy_interrupt_clear0_addr + 0x4*i);
		pr_err_ratelimited(
			"%s CSIPHY%d_IRQ_STATUS_ADDR%d = 0x%x\n",
			__func__, csiphy_dev->pdev->id, i, irq);
		msm_camera_io_w(0x0,
			csiphy_dev->base +
			csiphy_dev->ctrl_reg->csiphy_reg.
			mipi_csiphy_interrupt_clear0_addr + 0x4*i);
	}
	msm_camera_io_w(0x1, csiphy_dev->base +
		csiphy_dev->ctrl_reg->
		csiphy_reg.mipi_csiphy_glbl_irq_cmd_addr);
	msm_camera_io_w(0x0, csiphy_dev->base +
		csiphy_dev->ctrl_reg->
		csiphy_reg.mipi_csiphy_glbl_irq_cmd_addr);
	return IRQ_HANDLED;
}

static void msm_csiphy_reset(struct csiphy_device *csiphy_dev)
{
	msm_camera_io_w(0x1, csiphy_dev->base +
		csiphy_dev->ctrl_reg->csiphy_reg.mipi_csiphy_glbl_reset_addr);
	usleep_range(5000, 8000);
	msm_camera_io_w(0x0, csiphy_dev->base +
		csiphy_dev->ctrl_reg->csiphy_reg.mipi_csiphy_glbl_reset_addr);
}

static void msm_csiphy_3ph_reset(struct csiphy_device *csiphy_dev)
{
	msm_camera_io_w(0x1, csiphy_dev->base +
		csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl0.addr);
	usleep_range(5000, 8000);
	msm_camera_io_w(0x0, csiphy_dev->base +
		csiphy_dev->ctrl_reg->csiphy_3ph_reg.
		mipi_csiphy_3ph_cmn_ctrl0.addr);
}

#if DBG_CSIPHY
static int msm_csiphy_init(struct csiphy_device *csiphy_dev)
{
	int rc = 0;
	if (csiphy_dev == NULL) {
		pr_err("%s: csiphy_dev NULL\n", __func__);
		rc = -ENOMEM;
		return rc;
	}

	CDBG("%s:%d called\n", __func__, __LINE__);
	if (csiphy_dev->ref_count++) {
		CDBG("%s csiphy refcount = %d\n", __func__,
			csiphy_dev->ref_count);
		return rc;
	}

	CDBG("%s:%d called\n", __func__, __LINE__);
	if (csiphy_dev->csiphy_state == CSIPHY_POWER_UP) {
		pr_err("%s: csiphy invalid state %d\n", __func__,
			csiphy_dev->csiphy_state);
		rc = -EINVAL;
		return rc;
	}

	CDBG("%s:%d called\n", __func__, __LINE__);

	rc = cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_CSIPHY,
			CAM_AHB_SVS_VOTE);
	if (rc < 0) {
		csiphy_dev->ref_count--;
		pr_err("%s: failed to vote for AHB\n", __func__);
		return rc;
	}

	CDBG("%s:%d called\n", __func__, __LINE__);

	rc = msm_camera_config_vreg(&csiphy_dev->pdev->dev,
		csiphy_dev->csiphy_vreg,
		csiphy_dev->regulator_count, NULL, 0,
		&csiphy_dev->csiphy_reg_ptr[0], 1);
	if (rc < 0) {
		pr_err("%s:%d csiphy config_vreg failed\n",
			__func__, __LINE__);
		goto csiphy_resource_fail;
	}
	rc = msm_camera_enable_vreg(&csiphy_dev->pdev->dev,
		csiphy_dev->csiphy_vreg,
		csiphy_dev->regulator_count, NULL, 0,
		&csiphy_dev->csiphy_reg_ptr[0], 1);
	if (rc < 0) {
		pr_err("%s:%d csiphy enable_vreg failed\n",
			__func__, __LINE__);
		goto top_vreg_enable_failed;
	}

	rc = msm_camera_clk_enable(&csiphy_dev->pdev->dev,
		csiphy_dev->csiphy_clk_info, csiphy_dev->csiphy_clk,
		csiphy_dev->num_clk, true);

	CDBG("%s:%d called\n", __func__, __LINE__);
	if (rc < 0) {
		pr_err("%s: csiphy clk enable failed\n", __func__);
		csiphy_dev->ref_count--;
		goto csiphy_resource_fail;
	}
	CDBG("%s:%d called\n", __func__, __LINE__);

	rc = msm_camera_enable_irq(csiphy_dev->irq, true);
	if (rc < 0)
		pr_err("%s: irq enable failed\n", __func__);

	if (csiphy_dev->csiphy_3phase == CSI_3PHASE_HW)
		msm_csiphy_3ph_reset(csiphy_dev);
	else
		msm_csiphy_reset(csiphy_dev);

	CDBG("%s:%d called\n", __func__, __LINE__);

	if (csiphy_dev->hw_dts_version == CSIPHY_VERSION_V30)
		csiphy_dev->hw_version =
			msm_camera_io_r(csiphy_dev->base +
				 csiphy_dev->ctrl_reg->
				 csiphy_reg.mipi_csiphy_hw_version_addr);
	else
		csiphy_dev->hw_version = csiphy_dev->hw_dts_version;

	CDBG("%s:%d called csiphy_dev->hw_version 0x%x\n", __func__, __LINE__,
		csiphy_dev->hw_version);
	csiphy_dev->csiphy_state = CSIPHY_POWER_UP;
	return 0;

top_vreg_enable_failed:
	msm_camera_config_vreg(&csiphy_dev->pdev->dev,
		csiphy_dev->csiphy_vreg,
		csiphy_dev->regulator_count, NULL, 0,
		&csiphy_dev->csiphy_reg_ptr[0], 0);
csiphy_resource_fail:
	if (cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_CSIPHY,
		CAM_AHB_SUSPEND_VOTE) < 0)
		pr_err("%s: failed to vote for AHB\n", __func__);
	return rc;
}
#else
static int msm_csiphy_init(struct csiphy_device *csiphy_dev)
{
	int rc = 0;
	if (csiphy_dev == NULL) {
		pr_err("%s: csiphy_dev NULL\n", __func__);
		rc = -ENOMEM;
		return rc;
	}
	csiphy_dev->csiphy_sof_debug_count = 0;

	CDBG("%s:%d called\n", __func__, __LINE__);
	if (csiphy_dev->ref_count++) {
		CDBG("%s csiphy refcount = %d\n", __func__,
			csiphy_dev->ref_count);
		return rc;
	}

	CDBG("%s:%d called\n", __func__, __LINE__);
	if (csiphy_dev->csiphy_state == CSIPHY_POWER_UP) {
		pr_err("%s: csiphy invalid state %d\n", __func__,
			csiphy_dev->csiphy_state);
		rc = -EINVAL;
		return rc;
	}

	CDBG("%s:%d called\n", __func__, __LINE__);
	rc = cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_CSIPHY,
			CAM_AHB_SVS_VOTE);
	if (rc < 0) {
		csiphy_dev->ref_count--;
		pr_err("%s: failed to vote for AHB\n", __func__);
		return rc;
	}
	rc = msm_camera_config_vreg(&csiphy_dev->pdev->dev,
		csiphy_dev->csiphy_vreg,
		csiphy_dev->regulator_count, NULL, 0,
		&csiphy_dev->csiphy_reg_ptr[0], 1);
	if (rc < 0) {
		pr_err("%s:%d csiphy config_vreg failed\n",
			__func__, __LINE__);
		goto csiphy_resource_fail;
	}
	rc = msm_camera_enable_vreg(&csiphy_dev->pdev->dev,
		csiphy_dev->csiphy_vreg,
		csiphy_dev->regulator_count, NULL, 0,
		&csiphy_dev->csiphy_reg_ptr[0], 1);
	if (rc < 0) {
		pr_err("%s:%d csiphy enable_vreg failed\n",
			__func__, __LINE__);
		goto top_vreg_enable_failed;
	}

	rc = msm_camera_clk_enable(&csiphy_dev->pdev->dev,
		csiphy_dev->csiphy_clk_info, csiphy_dev->csiphy_clk,
		csiphy_dev->num_clk, true);

	CDBG("%s:%d called\n", __func__, __LINE__);
	if (rc < 0) {
		pr_err("%s: csiphy clk enable failed\n", __func__);
		csiphy_dev->ref_count--;
		goto csiphy_resource_fail;
	}
	CDBG("%s:%d clk enable success\n", __func__, __LINE__);

	if (csiphy_dev->csiphy_3phase == CSI_3PHASE_HW)
		msm_csiphy_3ph_reset(csiphy_dev);
	else
		msm_csiphy_reset(csiphy_dev);

	CDBG("%s:%d called\n", __func__, __LINE__);

	if (csiphy_dev->hw_dts_version == CSIPHY_VERSION_V30)
		csiphy_dev->hw_version =
			msm_camera_io_r(csiphy_dev->base +
				 csiphy_dev->ctrl_reg->
				 csiphy_reg.mipi_csiphy_hw_version_addr);
	else
		csiphy_dev->hw_version = csiphy_dev->hw_dts_version;

	csiphy_dev->csiphy_sof_debug = SOF_DEBUG_DISABLE;
	CDBG("%s:%d called csiphy_dev->hw_version 0x%x\n", __func__, __LINE__,
		csiphy_dev->hw_version);
	csiphy_dev->csiphy_state = CSIPHY_POWER_UP;
	return 0;
top_vreg_enable_failed:
	msm_camera_config_vreg(&csiphy_dev->pdev->dev,
		csiphy_dev->csiphy_vreg,
		csiphy_dev->regulator_count, NULL, 0,
		&csiphy_dev->csiphy_reg_ptr[0], 0);
csiphy_resource_fail:
	if (cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_CSIPHY,
		CAM_AHB_SUSPEND_VOTE) < 0)
		pr_err("%s: failed to vote for AHB\n", __func__);
	return rc;
}
#endif

#if DBG_CSIPHY
static int msm_csiphy_release(struct csiphy_device *csiphy_dev, void *arg)
{
	int i = 0;
	int rc = 0;
	struct msm_camera_csi_lane_params *csi_lane_params;
	uint16_t csi_lane_mask;
	csi_lane_params = (struct msm_camera_csi_lane_params *)arg;

	if (!csiphy_dev || !csiphy_dev->ref_count) {
		pr_err("%s csiphy dev NULL / ref_count ZERO\n", __func__);
		return 0;
	}

	if (csiphy_dev->csiphy_state != CSIPHY_POWER_UP) {
		pr_err("%s: csiphy invalid state %d\n", __func__,
			csiphy_dev->csiphy_state);
		return -EINVAL;
	}

	if (--csiphy_dev->ref_count) {
		CDBG("%s csiphy refcount = %d\n", __func__,
			csiphy_dev->ref_count);
		return 0;
	}

	if (csiphy_dev->csiphy_3phase == CSI_3PHASE_HW) {
		msm_camera_io_w(0x0,
			csiphy_dev->base + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_cmn_ctrl5.addr);
		msm_camera_io_w(0x0,
			csiphy_dev->base + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_cmn_ctrl6.addr);
		if (csiphy_dev->hw_dts_version >= CSIPHY_VERSION_V50)
			msm_camera_io_w(0x0,
				csiphy_dev->base +
				csiphy_dev->ctrl_reg->csiphy_3ph_reg.
				mipi_csiphy_3ph_cmn_ctrl7.addr);
	} else if (csiphy_dev->hw_version < CSIPHY_VERSION_V30) {
		csiphy_dev->lane_mask[csiphy_dev->pdev->id] = 0;
		for (i = 0; i < 4; i++)
			msm_camera_io_w(0x0, csiphy_dev->base +
				csiphy_dev->ctrl_reg->csiphy_reg.
				mipi_csiphy_lnn_cfg2_addr + 0x40*i);
		msm_camera_io_w(0x0, csiphy_dev->base +
			csiphy_dev->ctrl_reg->csiphy_reg.
			mipi_csiphy_lnck_cfg2_addr);
		msm_camera_io_w(0x0, csiphy_dev->base +
			csiphy_dev->ctrl_reg->csiphy_reg.
			mipi_csiphy_glbl_pwr_cfg_addr);
	} else {
		if (!csi_lane_params) {
			pr_err("%s:%d failed: csi_lane_params %pK\n", __func__,
				__LINE__, csi_lane_params);
			return -EINVAL;
		}
		csi_lane_mask = (csi_lane_params->csi_lane_mask & 0x1F);

		CDBG("%s csiphy_params, lane assign 0x%x mask = 0x%x\n",
			__func__,
			csi_lane_params->csi_lane_assign,
			csi_lane_params->csi_lane_mask);

		if (!csi_lane_mask)
			csi_lane_mask = 0x1f;

		csiphy_dev->lane_mask[csiphy_dev->pdev->id] &=
			~(csi_lane_mask);
		i = 0;
		while (csi_lane_mask) {
			if (csi_lane_mask & 0x1) {
				msm_camera_io_w(0x0, csiphy_dev->base +
					csiphy_dev->ctrl_reg->csiphy_reg.
					mipi_csiphy_lnn_cfg2_addr + 0x40*i);
				msm_camera_io_w(0x0, csiphy_dev->base +
					csiphy_dev->ctrl_reg->csiphy_reg.
					mipi_csiphy_lnn_misc1_addr + 0x40*i);
				msm_camera_io_w(0x0, csiphy_dev->base +
					csiphy_dev->ctrl_reg->csiphy_reg.
					mipi_csiphy_lnn_test_imp + 0x40*i);
			}
			csi_lane_mask >>= 1;
			i++;
		}
		msm_camera_io_w(0x0, csiphy_dev->base +
			csiphy_dev->ctrl_reg->csiphy_reg.
			mipi_csiphy_lnck_cfg2_addr);
		msm_camera_io_w(0x0, csiphy_dev->base +
			csiphy_dev->ctrl_reg->csiphy_reg.
			mipi_csiphy_glbl_pwr_cfg_addr);
	}

	rc = msm_camera_enable_irq(csiphy_dev->irq, false);

	msm_camera_clk_enable(&csiphy_dev->pdev->dev,
		csiphy_dev->csiphy_clk_info, csiphy_dev->csiphy_clk,
		csiphy_dev->num_clk, false);

	if (csiphy_dev->csiphy_3phase == CSI_3PHASE_HW &&
		csiphy_dev->csi_3phase == 1) {
		msm_camera_clk_enable(&csiphy_dev->pdev->dev,
			csiphy_dev->csiphy_3p_clk_info,
			csiphy_dev->csiphy_3p_clk, 2, false);
	}

	msm_camera_enable_vreg(&csiphy_dev->pdev->dev,
		csiphy_dev->csiphy_vreg,
		csiphy_dev->regulator_count, NULL, 0,
		&csiphy_dev->csiphy_reg_ptr[0], 0);
	msm_camera_config_vreg(&csiphy_dev->pdev->dev,
		csiphy_dev->csiphy_vreg, csiphy_dev->regulator_count,
		NULL, 0, &csiphy_dev->csiphy_reg_ptr[0], 0);

	csiphy_dev->csiphy_state = CSIPHY_POWER_DOWN;

	if (cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_CSIPHY,
		 CAM_AHB_SUSPEND_VOTE) < 0)
		pr_err("%s: failed to remove vote for AHB\n", __func__);
	return 0;
}
#else
static int msm_csiphy_release(struct csiphy_device *csiphy_dev, void *arg)
{
	int i = 0, rc = 0;
	struct msm_camera_csi_lane_params *csi_lane_params;
	uint16_t csi_lane_mask;
	csi_lane_params = (struct msm_camera_csi_lane_params *)arg;

	if (!csiphy_dev || !csiphy_dev->ref_count) {
		pr_err("%s csiphy dev NULL / ref_count ZERO\n", __func__);
		return 0;
	}

	if (csiphy_dev->csiphy_state != CSIPHY_POWER_UP) {
		pr_err("%s: csiphy invalid state %d\n", __func__,
			csiphy_dev->csiphy_state);
		return -EINVAL;
	}

	if (--csiphy_dev->ref_count) {
		CDBG("%s csiphy refcount = %d\n", __func__,
			csiphy_dev->ref_count);
		return 0;
	}

	if (csiphy_dev->csiphy_3phase == CSI_3PHASE_HW) {
		msm_camera_io_w(0x0,
			csiphy_dev->base + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_cmn_ctrl5.addr);
		msm_camera_io_w(0x0,
			csiphy_dev->base + csiphy_dev->ctrl_reg->csiphy_3ph_reg.
			mipi_csiphy_3ph_cmn_ctrl6.addr);
		if (csiphy_dev->hw_dts_version >= CSIPHY_VERSION_V50)
			msm_camera_io_w(0x0,
				csiphy_dev->base +
				csiphy_dev->ctrl_reg->csiphy_3ph_reg.
				mipi_csiphy_3ph_cmn_ctrl7.addr);
	} else	if (csiphy_dev->hw_version < CSIPHY_VERSION_V30) {
		csiphy_dev->lane_mask[csiphy_dev->pdev->id] = 0;
		for (i = 0; i < 4; i++)
			msm_camera_io_w(0x0, csiphy_dev->base +
				csiphy_dev->ctrl_reg->csiphy_reg.
				mipi_csiphy_lnn_cfg2_addr + 0x40*i);
		msm_camera_io_w(0x0, csiphy_dev->base +
			csiphy_dev->ctrl_reg->csiphy_reg.
			mipi_csiphy_lnck_cfg2_addr);
		msm_camera_io_w(0x0, csiphy_dev->base +
			csiphy_dev->ctrl_reg->csiphy_reg.
			mipi_csiphy_glbl_pwr_cfg_addr);
	} else {
		if (!csi_lane_params) {
			pr_err("%s:%d failed: csi_lane_params %pK\n", __func__,
				__LINE__, csi_lane_params);
			return -EINVAL;
		}
		csi_lane_mask = (csi_lane_params->csi_lane_mask & 0x1F);

		CDBG("%s csiphy_params, lane assign 0x%x mask = 0x%x\n",
			__func__,
			csi_lane_params->csi_lane_assign,
			csi_lane_params->csi_lane_mask);

		if (!csi_lane_mask)
			csi_lane_mask = 0x1f;

		csiphy_dev->lane_mask[csiphy_dev->pdev->id] &=
			~(csi_lane_mask);
		i = 0;
		while (csi_lane_mask) {
			if (csi_lane_mask & 0x1) {
				msm_camera_io_w(0x0, csiphy_dev->base +
					csiphy_dev->ctrl_reg->csiphy_reg.
					mipi_csiphy_lnn_cfg2_addr + 0x40*i);
				msm_camera_io_w(0x0, csiphy_dev->base +
					csiphy_dev->ctrl_reg->csiphy_reg.
					mipi_csiphy_lnn_misc1_addr + 0x40*i);
				msm_camera_io_w(0x0, csiphy_dev->base +
					csiphy_dev->ctrl_reg->csiphy_reg.
					mipi_csiphy_lnn_test_imp + 0x40*i);
			}
			csi_lane_mask >>= 1;
			i++;
		}
		msm_camera_io_w(0x0, csiphy_dev->base +
			csiphy_dev->ctrl_reg->csiphy_reg.
			mipi_csiphy_lnck_cfg2_addr);
		msm_camera_io_w(0x0, csiphy_dev->base +
			csiphy_dev->ctrl_reg->csiphy_reg.
			mipi_csiphy_glbl_pwr_cfg_addr);
	}
	if (csiphy_dev->csiphy_sof_debug == SOF_DEBUG_ENABLE) {
		rc = msm_camera_enable_irq(csiphy_dev->irq, false);
	}

	msm_camera_clk_enable(&csiphy_dev->pdev->dev,
		csiphy_dev->csiphy_clk_info, csiphy_dev->csiphy_clk,
		csiphy_dev->num_clk, false);
	if (csiphy_dev->csiphy_3phase == CSI_3PHASE_HW &&
		csiphy_dev->csi_3phase == 1) {
		msm_camera_clk_enable(&csiphy_dev->pdev->dev,
			csiphy_dev->csiphy_3p_clk_info,
			csiphy_dev->csiphy_3p_clk, 2, false);
	}

	msm_camera_enable_vreg(&csiphy_dev->pdev->dev,
		csiphy_dev->csiphy_vreg, csiphy_dev->regulator_count,
		NULL, 0, &csiphy_dev->csiphy_reg_ptr[0], 0);
	msm_camera_config_vreg(&csiphy_dev->pdev->dev,
		csiphy_dev->csiphy_vreg, csiphy_dev->regulator_count,
		NULL, 0, &csiphy_dev->csiphy_reg_ptr[0], 0);

	csiphy_dev->csiphy_state = CSIPHY_POWER_DOWN;

	if (cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_CSIPHY,
		 CAM_AHB_SUSPEND_VOTE) < 0)
		pr_err("%s: failed to remove vote for AHB\n", __func__);
	return 0;
}

#endif
static int32_t msm_csiphy_cmd(struct csiphy_device *csiphy_dev, void *arg)
{
	int rc = 0;
	struct csiphy_cfg_data *cdata = (struct csiphy_cfg_data *)arg;
	struct msm_camera_csiphy_params csiphy_params;
	struct msm_camera_csi_lane_params csi_lane_params;
	if (!csiphy_dev || !cdata) {
		pr_err("%s: csiphy_dev NULL\n", __func__);
		return -EINVAL;
	}

	switch (cdata->cfgtype) {
	case CSIPHY_INIT:
		rc = msm_csiphy_init(csiphy_dev);
		break;
	case CSIPHY_CFG:
		if (copy_from_user(&csiphy_params,
			(void *)cdata->cfg.csiphy_params,
			sizeof(struct msm_camera_csiphy_params))) {
			pr_err("%s: %d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		csiphy_dev->csiphy_sof_debug = SOF_DEBUG_DISABLE;
		rc = msm_csiphy_lane_config(csiphy_dev, &csiphy_params);
		break;
	case CSIPHY_RELEASE:
		if (copy_from_user(&csi_lane_params,
			(void *)cdata->cfg.csi_lane_params,
			sizeof(struct msm_camera_csi_lane_params))) {
			pr_err("%s: %d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		rc = msm_csiphy_release(csiphy_dev, &csi_lane_params);
		break;
	default:
		pr_err("%s: %d failed\n", __func__, __LINE__);
		rc = -ENOIOCTLCMD;
		break;
	}
	return rc;
}

static int32_t msm_csiphy_get_subdev_id(struct csiphy_device *csiphy_dev,
	void *arg)
{
	uint32_t *subdev_id = (uint32_t *)arg;
	if (!subdev_id) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		return -EINVAL;
	}
	*subdev_id = csiphy_dev->pdev->id;
	pr_debug("%s:%d subdev_id %d\n", __func__, __LINE__, *subdev_id);
	return 0;
}

static long msm_csiphy_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	int rc = -ENOIOCTLCMD;
	struct csiphy_device *csiphy_dev = v4l2_get_subdevdata(sd);
	if (!csiphy_dev) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		return  -EINVAL;
	}
	mutex_lock(&csiphy_dev->mutex);
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
		rc = msm_csiphy_get_subdev_id(csiphy_dev, arg);
		break;
	case VIDIOC_MSM_CSIPHY_IO_CFG:
		rc = msm_csiphy_cmd(csiphy_dev, arg);
		break;
	case VIDIOC_MSM_CSIPHY_RELEASE:
	case MSM_SD_SHUTDOWN:
		rc = msm_csiphy_release(csiphy_dev, arg);
		break;
	case MSM_SD_NOTIFY_FREEZE:
		if (!csiphy_dev || !csiphy_dev->ctrl_reg ||
				!csiphy_dev->ref_count)
			break;
		if (csiphy_dev->csiphy_sof_debug == SOF_DEBUG_DISABLE) {
			csiphy_dev->csiphy_sof_debug = SOF_DEBUG_ENABLE;
			rc = msm_camera_enable_irq(csiphy_dev->irq, true);
		}
		break;
	case MSM_SD_UNNOTIFY_FREEZE:
		if (!csiphy_dev || !csiphy_dev->ctrl_reg ||
				!csiphy_dev->ref_count)
			break;
		csiphy_dev->csiphy_sof_debug = SOF_DEBUG_DISABLE;
		rc = msm_camera_enable_irq(csiphy_dev->irq, false);
		break;
	default:
		pr_err_ratelimited("%s: command not found\n", __func__);
	}
	mutex_unlock(&csiphy_dev->mutex);
	CDBG("%s:%d\n", __func__, __LINE__);
	return rc;
}

#ifdef CONFIG_COMPAT
static long msm_csiphy_subdev_do_ioctl(
	struct file *file, unsigned int cmd, void *arg)
{
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);
	struct csiphy_cfg_data32 *u32 =
		(struct csiphy_cfg_data32 *)arg;
	struct csiphy_cfg_data csiphy_data;

	switch (cmd) {
	case VIDIOC_MSM_CSIPHY_IO_CFG32:
		cmd = VIDIOC_MSM_CSIPHY_IO_CFG;
		csiphy_data.cfgtype = u32->cfgtype;
		csiphy_data.cfg.csiphy_params =
			compat_ptr(u32->cfg.csiphy_params);
		return msm_csiphy_subdev_ioctl(sd, cmd, &csiphy_data);
	default:
		return msm_csiphy_subdev_ioctl(sd, cmd, arg);
	}
}

static long msm_csiphy_subdev_fops_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_csiphy_subdev_do_ioctl);
}
#endif

static const struct v4l2_subdev_internal_ops msm_csiphy_internal_ops;

static struct v4l2_subdev_core_ops msm_csiphy_subdev_core_ops = {
	.ioctl = &msm_csiphy_subdev_ioctl,
};

static const struct v4l2_subdev_ops msm_csiphy_subdev_ops = {
	.core = &msm_csiphy_subdev_core_ops,
};

static int msm_csiphy_get_clk_info(struct csiphy_device *csiphy_dev,
	struct platform_device *pdev)
{
	int i, rc = 0;
	char *csi_3p_clk_name = "csi_phy_3p_clk";
	char *csi_3p_clk_src_name = "csiphy_3p_clk_src";
	uint32_t clk_cnt = 0;

	rc = msm_camera_get_clk_info(csiphy_dev->pdev,
		&csiphy_dev->csiphy_all_clk_info,
		&csiphy_dev->csiphy_all_clk,
		&csiphy_dev->num_all_clk);
	if (rc < 0) {
		pr_err("%s:%d, failed\n", __func__, __LINE__);
		return rc;
	}
	if (csiphy_dev->num_all_clk > CSIPHY_NUM_CLK_MAX) {
		pr_err("%s: invalid count=%zu, max is %d\n", __func__,
			csiphy_dev->num_all_clk, CSIPHY_NUM_CLK_MAX);
		rc = -EINVAL;
		goto MAX_CLK_ERROR;
	}

	for (i = 0; i < csiphy_dev->num_all_clk; i++) {
		if (!strcmp(csiphy_dev->csiphy_all_clk_info[i].clk_name,
			csi_3p_clk_src_name)) {
			csiphy_dev->csiphy_3p_clk_info[0].clk_name =
				csiphy_dev->csiphy_all_clk_info[i].clk_name;
			csiphy_dev->csiphy_3p_clk_info[0].clk_rate =
				csiphy_dev->csiphy_all_clk_info[i].clk_rate;
			csiphy_dev->csiphy_3p_clk[0] =
				csiphy_dev->csiphy_all_clk[i];
			continue;
		} else if (!strcmp(csiphy_dev->csiphy_all_clk_info[i].clk_name,
					csi_3p_clk_name)) {
			csiphy_dev->csiphy_3p_clk_info[1].clk_name =
				csiphy_dev->csiphy_all_clk_info[i].clk_name;
			csiphy_dev->csiphy_3p_clk_info[1].clk_rate =
				csiphy_dev->csiphy_all_clk_info[i].clk_rate;
			csiphy_dev->csiphy_3p_clk[1] =
				csiphy_dev->csiphy_all_clk[i];
			continue;
		}
		csiphy_dev->csiphy_clk_info[clk_cnt].clk_name =
			csiphy_dev->csiphy_all_clk_info[i].clk_name;
		csiphy_dev->csiphy_clk_info[clk_cnt].clk_rate =
			csiphy_dev->csiphy_all_clk_info[i].clk_rate;
		csiphy_dev->csiphy_clk[clk_cnt] =
			csiphy_dev->csiphy_all_clk[clk_cnt];
		if (!strcmp(csiphy_dev->csiphy_clk_info[clk_cnt].clk_name,
				"csiphy_timer_src_clk")) {
			CDBG("%s:%d, copy csiphy_timer_src_clk",
				__func__, __LINE__);
			csiphy_dev->csiphy_max_clk =
				csiphy_dev->csiphy_clk_info[clk_cnt].clk_rate;
			csiphy_dev->csiphy_clk_index = clk_cnt;
		}
		CDBG("%s: clk_rate[%d] = %ld\n", __func__, clk_cnt,
			csiphy_dev->csiphy_clk_info[clk_cnt].clk_rate);
		clk_cnt++;
	}

	csiphy_dev->num_clk = clk_cnt;
	return rc;
MAX_CLK_ERROR:
	msm_camera_put_clk_info(csiphy_dev->pdev,
		&csiphy_dev->csiphy_all_clk_info,
		&csiphy_dev->csiphy_all_clk,
		csiphy_dev->num_all_clk);

	return rc;
}

static int csiphy_probe(struct platform_device *pdev)
{
	struct csiphy_device *new_csiphy_dev;
	int rc = 0;

	new_csiphy_dev = kzalloc(sizeof(struct csiphy_device), GFP_KERNEL);
	if (!new_csiphy_dev) {
		pr_err("%s: no enough memory\n", __func__);
		return -ENOMEM;
	}
	new_csiphy_dev->is_3_1_20nm_hw = 0;
	new_csiphy_dev->ctrl_reg = NULL;
	new_csiphy_dev->ctrl_reg = kzalloc(sizeof(struct csiphy_ctrl_t),
		GFP_KERNEL);
	if (!new_csiphy_dev->ctrl_reg) {
		pr_err("%s:%d kzalloc failed\n", __func__, __LINE__);
		return -ENOMEM;
	}
	v4l2_subdev_init(&new_csiphy_dev->msm_sd.sd, &msm_csiphy_subdev_ops);
	v4l2_set_subdevdata(&new_csiphy_dev->msm_sd.sd, new_csiphy_dev);
	platform_set_drvdata(pdev, &new_csiphy_dev->msm_sd.sd);

	mutex_init(&new_csiphy_dev->mutex);

	if (pdev->dev.of_node) {
		of_property_read_u32((&pdev->dev)->of_node,
			"cell-index", &pdev->id);
		CDBG("%s: device id = %d\n", __func__, pdev->id);
	}

	new_csiphy_dev->pdev = pdev;
	new_csiphy_dev->msm_sd.sd.internal_ops = &msm_csiphy_internal_ops;
	new_csiphy_dev->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(new_csiphy_dev->msm_sd.sd.name,
		ARRAY_SIZE(new_csiphy_dev->msm_sd.sd.name), "msm_csiphy");
	media_entity_init(&new_csiphy_dev->msm_sd.sd.entity, 0, NULL, 0);
	new_csiphy_dev->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	new_csiphy_dev->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_CSIPHY;
	new_csiphy_dev->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x4;
	msm_sd_register(&new_csiphy_dev->msm_sd);

	new_csiphy_dev->csiphy_3phase = 0;
	new_csiphy_dev->num_irq_registers = 0x8;

	if (of_device_is_compatible(new_csiphy_dev->pdev->dev.of_node,
		"qcom,csiphy-v2.0")) {
		new_csiphy_dev->ctrl_reg->csiphy_reg = csiphy_v2_0;
		new_csiphy_dev->hw_dts_version = CSIPHY_VERSION_V20;
	} else if (of_device_is_compatible(new_csiphy_dev->pdev->dev.of_node,
		"qcom,csiphy-v2.2")) {
		new_csiphy_dev->ctrl_reg->csiphy_reg = csiphy_v2_2;
		new_csiphy_dev->hw_dts_version = CSIPHY_VERSION_V22;
	} else if (of_device_is_compatible(new_csiphy_dev->pdev->dev.of_node,
		"qcom,csiphy-v3.0")) {
		new_csiphy_dev->ctrl_reg->csiphy_reg = csiphy_v3_0;
		new_csiphy_dev->hw_dts_version = CSIPHY_VERSION_V30;
	} else if (of_device_is_compatible(new_csiphy_dev->pdev->dev.of_node,
		"qcom,csiphy-v3.1")) {
		new_csiphy_dev->ctrl_reg->csiphy_reg = csiphy_v3_1;
		new_csiphy_dev->hw_dts_version = CSIPHY_VERSION_V31;
	} else if (of_device_is_compatible(new_csiphy_dev->pdev->dev.of_node,
		"qcom,csiphy-v3.1.1")) {
		new_csiphy_dev->ctrl_reg->csiphy_reg = csiphy_v3_1;
		new_csiphy_dev->hw_dts_version = CSIPHY_VERSION_V31;
		new_csiphy_dev->is_3_1_20nm_hw = 1;
	} else if (of_device_is_compatible(new_csiphy_dev->pdev->dev.of_node,
		"qcom,csiphy-v3.2")) {
		new_csiphy_dev->ctrl_reg->csiphy_reg = csiphy_v3_2;
		new_csiphy_dev->hw_dts_version = CSIPHY_VERSION_V32;
	} else if (of_device_is_compatible(new_csiphy_dev->pdev->dev.of_node,
		"qcom,csiphy-v3.4.2")) {
		new_csiphy_dev->ctrl_reg->csiphy_3ph_reg = csiphy_v3_4_2_3ph;
		new_csiphy_dev->ctrl_reg->csiphy_reg = csiphy_v3_4_2;
		new_csiphy_dev->hw_dts_version = CSIPHY_VERSION_V342;
		new_csiphy_dev->csiphy_3phase = CSI_3PHASE_HW;
	} else if (of_device_is_compatible(new_csiphy_dev->pdev->dev.of_node,
		"qcom,csiphy-v3.5")) {
		new_csiphy_dev->ctrl_reg->csiphy_3ph_reg = csiphy_v3_5_3ph;
		new_csiphy_dev->ctrl_reg->csiphy_reg = csiphy_v3_5;
		new_csiphy_dev->hw_dts_version = CSIPHY_VERSION_V35;
		new_csiphy_dev->csiphy_3phase = CSI_3PHASE_HW;
	} else if (of_device_is_compatible(new_csiphy_dev->pdev->dev.of_node,
		"qcom,csiphy-v5.0")) {
		new_csiphy_dev->ctrl_reg->csiphy_3ph_reg = csiphy_v5_0_3ph;
		new_csiphy_dev->ctrl_reg->csiphy_reg = csiphy_v5_0;
		new_csiphy_dev->hw_dts_version = CSIPHY_VERSION_V50;
		new_csiphy_dev->csiphy_3phase = CSI_3PHASE_HW;
		new_csiphy_dev->ctrl_reg->csiphy_combo_mode_settings =
			csiphy_combo_mode_v5_0;
	} else if (of_device_is_compatible(new_csiphy_dev->pdev->dev.of_node,
		"qcom,csiphy-v5.01")) {
		new_csiphy_dev->ctrl_reg->csiphy_3ph_reg = csiphy_v5_0_1_3ph;
		new_csiphy_dev->ctrl_reg->csiphy_reg = csiphy_v5_0_1;
		new_csiphy_dev->hw_dts_version = CSIPHY_VERSION_V501;
		new_csiphy_dev->csiphy_3phase = CSI_3PHASE_HW;
		new_csiphy_dev->ctrl_reg->csiphy_combo_mode_settings =
			csiphy_combo_mode_v5_0_1;
	} else {
		pr_err("%s:%d, invalid hw version : 0x%x\n", __func__, __LINE__,
		new_csiphy_dev->hw_dts_version);
		rc =  -EINVAL;
		goto csiphy_no_resource;
	}

	rc = msm_camera_get_dt_vreg_data(pdev->dev.of_node,
		&(new_csiphy_dev->csiphy_vreg),
		&(new_csiphy_dev->regulator_count));
	if (rc < 0) {
		pr_err("%s: get vreg data from dtsi fail\n", __func__);
		rc = -EFAULT;
		goto csiphy_no_resource;
	}
	/* ToDo: Enable 3phase clock for dynamic clock enable/disable */
	rc = msm_csiphy_get_clk_info(new_csiphy_dev, pdev);
	if (rc < 0) {
		pr_err("%s: msm_csiphy_get_clk_info() failed", __func__);
		rc =  -EFAULT;
		goto csiphy_no_resource;
	}

	new_csiphy_dev->base = msm_camera_get_reg_base(pdev, "csiphy", true);
	if (!new_csiphy_dev->base) {
		pr_err("%s: no mem resource?\n", __func__);
		rc = -ENODEV;
		goto csiphy_no_resource;
	}

	if (new_csiphy_dev->hw_dts_version >= CSIPHY_VERSION_V30) {
		new_csiphy_dev->clk_mux_base = msm_camera_get_reg_base(pdev,
					"csiphy_clk_mux", true);
		if (!new_csiphy_dev->clk_mux_base)
			pr_err("%s: no mem resource?\n", __func__);
	}
	new_csiphy_dev->irq = msm_camera_get_irq(pdev, "csiphy");
	if (!new_csiphy_dev->irq) {
		pr_err("%s: no irq resource?\n", __func__);
		rc = -ENODEV;
		goto csiphy_no_irq_resource;
	}
	rc = msm_camera_register_irq(pdev, new_csiphy_dev->irq,
		msm_csiphy_irq, IRQF_TRIGGER_RISING, "csiphy", new_csiphy_dev);
	if (rc < 0) {
		pr_err("%s: irq request fail\n", __func__);
		rc = -EBUSY;
		goto csiphy_no_irq_resource;
	}
	msm_camera_enable_irq(new_csiphy_dev->irq, false);

	msm_cam_copy_v4l2_subdev_fops(&msm_csiphy_v4l2_subdev_fops);
#ifdef CONFIG_COMPAT
	msm_csiphy_v4l2_subdev_fops.compat_ioctl32 =
		msm_csiphy_subdev_fops_ioctl;
#endif
	new_csiphy_dev->msm_sd.sd.devnode->fops =
		&msm_csiphy_v4l2_subdev_fops;
	new_csiphy_dev->csiphy_state = CSIPHY_POWER_DOWN;
	return 0;

csiphy_no_irq_resource:
	if (new_csiphy_dev->hw_dts_version >= CSIPHY_VERSION_V30) {
		msm_camera_put_reg_base(pdev, new_csiphy_dev->clk_mux_base,
			"csiphy_clk_mux", true);
	}
	msm_camera_put_reg_base(pdev, new_csiphy_dev->base, "csiphy", true);
csiphy_no_resource:
	mutex_destroy(&new_csiphy_dev->mutex);
	kfree(new_csiphy_dev->ctrl_reg);
	kfree(new_csiphy_dev);
	return rc;
}

static int msm_csiphy_exit(struct platform_device *pdev)
{
	struct v4l2_subdev *subdev = platform_get_drvdata(pdev);
	struct csiphy_device *csiphy_dev =
		v4l2_get_subdevdata(subdev);

	msm_camera_put_clk_info(pdev,
		&csiphy_dev->csiphy_all_clk_info,
		&csiphy_dev->csiphy_all_clk,
		csiphy_dev->num_all_clk);

	msm_camera_put_reg_base(pdev, csiphy_dev->base, "csiphy", true);
	if (csiphy_dev->hw_dts_version >= CSIPHY_VERSION_V30) {
		msm_camera_put_reg_base(pdev, csiphy_dev->clk_mux_base,
			"csiphy_clk_mux", true);
	}
	kfree(csiphy_dev);
	return 0;
}

static const struct of_device_id msm_csiphy_dt_match[] = {
	{.compatible = "qcom,csiphy"},
	{}
};

MODULE_DEVICE_TABLE(of, msm_csiphy_dt_match);

static struct platform_driver csiphy_driver = {
	.probe = csiphy_probe,
	.remove = msm_csiphy_exit,
	.driver = {
		.name = MSM_CSIPHY_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msm_csiphy_dt_match,
	},
};

static int __init msm_csiphy_init_module(void)
{
	return platform_driver_register(&csiphy_driver);
}

static void __exit msm_csiphy_exit_module(void)
{
	platform_driver_unregister(&csiphy_driver);
}

module_init(msm_csiphy_init_module);
module_exit(msm_csiphy_exit_module);
MODULE_DESCRIPTION("MSM CSIPHY driver");
MODULE_LICENSE("GPL v2");
