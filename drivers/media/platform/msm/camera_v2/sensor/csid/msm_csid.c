/* Copyright (c) 2011-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
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
#include <linux/module.h>
#include <linux/of.h>
#include <linux/irqreturn.h>
#include <soc/qcom/scm.h>
#include "msm_csid.h"
#include "msm_sd.h"
#include "msm_camera_io_util.h"
#include "msm_camera_dt_util.h"
#include "msm_camera_tz_util.h"
#include "include/msm_csid_2_0_hwreg.h"
#include "include/msm_csid_2_2_hwreg.h"
#include "include/msm_csid_3_0_hwreg.h"
#include "include/msm_csid_3_1_hwreg.h"
#include "include/msm_csid_3_2_hwreg.h"
#include "include/msm_csid_3_5_hwreg.h"
#include "include/msm_csid_3_4_1_hwreg.h"
#include "include/msm_csid_3_4_2_hwreg.h"
#include "include/msm_csid_3_4_3_hwreg.h"
#include "include/msm_csid_3_6_0_hwreg.h"
#include "include/msm_csid_3_5_1_hwreg.h"
#include "cam_hw_ops.h"

#define V4L2_IDENT_CSID                            50002
#define CSID_VERSION_V20                      0x02000011
#define CSID_VERSION_V22                      0x02001000
#define CSID_VERSION_V30                      0x30000000
#define CSID_VERSION_V31                      0x30010000
#define CSID_VERSION_V31_1                    0x30010001
#define CSID_VERSION_V31_3                    0x30010003
#define CSID_VERSION_V32                      0x30020000
#define CSID_VERSION_V33                      0x30030000
#define CSID_VERSION_V34                      0x30040000
#define CSID_VERSION_V34_1                    0x30040001
#define CSID_VERSION_V34_2                    0x30040002
#define CSID_VERSION_V34_3                    0x30040003
#define CSID_VERSION_V36                      0x30060000
#define CSID_VERSION_V37                      0x30070000
#define CSID_VERSION_V35                      0x30050000
#define CSID_VERSION_V35_1                    0x30050001
#define CSID_VERSION_V40                      0x40000000
#define CSID_VERSION_V50                      0x50000000
#define MSM_CSID_DRV_NAME                    "msm_csid"

#define DBG_CSID                             0
#define SHORT_PKT_CAPTURE                    0
#define SHORT_PKT_OFFSET                     0x200
#define ENABLE_3P_BIT                        1
#define SOF_DEBUG_ENABLE                     1
#define SOF_DEBUG_DISABLE                    0

#define SCM_SVC_CAMERASS                     0x18
#define SECURE_SYSCALL_ID                    0x7
#define TOPOLOGY_SYSCALL_ID                  0x8
#define STREAM_NOTIF_SYSCALL_ID              0x9

#define CSIPHY_0_LANES_MASK                  0x000f
#define CSIPHY_1_LANES_MASK                  0x00f0
#define CSIPHY_2_LANES_MASK                  0x0f00
#define CSIPHY_3_LANES_MASK                  0xf000

#define TRUE   1
#define FALSE  0

#define MAX_LANE_COUNT 4
#define CSID_TIMEOUT msecs_to_jiffies(800)

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

static const uint32_t CSIPHY_LANES_MASKS[] = {
	CSIPHY_0_LANES_MASK,
	CSIPHY_1_LANES_MASK,
	CSIPHY_2_LANES_MASK,
	CSIPHY_3_LANES_MASK,
};

static struct camera_vreg_t csid_vreg_info[] = {
	{"qcom,mipi-csi-vdd", 0, 0, 12000},
};

#ifdef CONFIG_COMPAT
static struct v4l2_file_operations msm_csid_v4l2_subdev_fops;
#endif

static inline uint32_t msm_camera_vio_r(
	void __iomem *base_addr, uint32_t offset, uint32_t dev_id) {
	return msm_camera_tz_r(base_addr, offset,
		MSM_CAMERA_TZ_IO_REGION_CSIDCORE0 + dev_id);
}

static inline void msm_camera_vio_w(uint32_t data,
	void __iomem *base_addr, uint32_t offset, uint32_t dev_id) {
	msm_camera_tz_w(data, base_addr, offset,
		MSM_CAMERA_TZ_IO_REGION_CSIDCORE0 + dev_id);
}

static inline void msm_camera_vio_w_def(uint32_t data,
	void __iomem *base_addr, uint32_t offset, uint32_t dev_id) {
	msm_camera_tz_w_deferred(data, base_addr, offset,
		MSM_CAMERA_TZ_IO_REGION_CSIDCORE0 + dev_id);
}

static int msm_csid_cid_lut(
	struct msm_camera_csid_lut_params *csid_lut_params,
	struct csid_device *csid_dev)
{
	int rc = 0, i = 0;
	uint32_t val = 0;

	if (!csid_lut_params) {
		pr_err("%s:%d csid_lut_params NULL\n", __func__, __LINE__);
		return -EINVAL;
	}
	if (csid_lut_params->num_cid > MAX_CID) {
		pr_err("%s:%d num_cid exceeded limit num_cid = %d max = %d\n",
			__func__, __LINE__, csid_lut_params->num_cid, MAX_CID);
		return -EINVAL;
	}
	for (i = 0; i < csid_lut_params->num_cid; i++) {
		if (csid_lut_params->vc_cfg[i]->cid >= MAX_CID) {
			pr_err("%s: cid outside range %d\n",
				 __func__, csid_lut_params->vc_cfg[i]->cid);
			return -EINVAL;
		}
		CDBG("%s lut params num_cid = %d, cid = %d\n",
			__func__,
			csid_lut_params->num_cid,
			csid_lut_params->vc_cfg[i]->cid);
		CDBG("%s lut params dt = 0x%x, df = %d\n", __func__,
			csid_lut_params->vc_cfg[i]->dt,
			csid_lut_params->vc_cfg[i]->decode_format);
		if (csid_lut_params->vc_cfg[i]->dt < 0x12 ||
			csid_lut_params->vc_cfg[i]->dt > 0x37) {
			pr_err("%s: unsupported data type 0x%x\n",
				 __func__, csid_lut_params->vc_cfg[i]->dt);
			return rc;
		}
		val = msm_camera_vio_r(csid_dev->base,
			csid_dev->ctrl_reg->csid_reg.csid_cid_lut_vc_0_addr +
			(csid_lut_params->vc_cfg[i]->cid >> 2) * 4,
			csid_dev->pdev->id)
			& ~(0xFF << ((csid_lut_params->vc_cfg[i]->cid % 4) *
			8));
		val |= (csid_lut_params->vc_cfg[i]->dt <<
			((csid_lut_params->vc_cfg[i]->cid % 4) * 8));
		msm_camera_vio_w(val, csid_dev->base,
			csid_dev->ctrl_reg->csid_reg.csid_cid_lut_vc_0_addr +
			(csid_lut_params->vc_cfg[i]->cid >> 2) * 4,
			csid_dev->pdev->id);

		val = (csid_lut_params->vc_cfg[i]->decode_format << 4) | 0x3;
		msm_camera_vio_w(val, csid_dev->base,
			csid_dev->ctrl_reg->csid_reg.csid_cid_n_cfg_addr +
			(csid_lut_params->vc_cfg[i]->cid * 4),
			csid_dev->pdev->id);
	}
	return rc;
}

#if (DBG_CSID)
static void msm_csid_set_debug_reg(struct csid_device *csid_dev,
	struct msm_camera_csid_params *csid_params)
{
	uint32_t val = 0;

	if ((csid_dev->hw_dts_version == CSID_VERSION_V34_1) ||
		(csid_dev->hw_dts_version == CSID_VERSION_V36)) {
		val = ((1 << csid_params->lane_cnt) - 1) << 20;
		msm_camera_vio_w(0x7f010800 | val, csid_dev->base,
			csid_dev->ctrl_reg->csid_reg.csid_irq_mask_addr,
			csid_dev->pdev->id);
		msm_camera_vio_w(0x7f010800 | val, csid_dev->base,
			csid_dev->ctrl_reg->csid_reg.csid_irq_clear_cmd_addr,
			csid_dev->pdev->id);
	} else {
		if (csid_dev->csid_3p_enabled == 1) {
			val = ((1 << csid_params->lane_cnt) - 1) <<
			csid_dev->ctrl_reg->csid_reg
				.csid_err_lane_overflow_offset_3p;
		} else {
			val = ((1 << csid_params->lane_cnt) - 1) <<
			csid_dev->ctrl_reg->csid_reg
				.csid_err_lane_overflow_offset_2p;
		}
		val |= csid_dev->ctrl_reg->csid_reg.csid_irq_mask_val;
		msm_camera_vio_w(val, csid_dev->base,
			csid_dev->ctrl_reg->csid_reg.csid_irq_mask_addr,
			csid_dev->pdev->id);
		msm_camera_vio_w(val, csid_dev->base,
			csid_dev->ctrl_reg->csid_reg.csid_irq_clear_cmd_addr,
			csid_dev->pdev->id);
	}
}
#elif(SHORT_PKT_CAPTURE)
static void msm_csid_set_debug_reg(struct csid_device *csid_dev,
	struct msm_camera_csid_params *csid_params)
{
	uint32_t val = 0;

	if ((csid_dev->hw_dts_version == CSID_VERSION_V34_1) ||
		(csid_dev->hw_dts_version == CSID_VERSION_V36)) {
		val = ((1 << csid_params->lane_cnt) - 1) << 20;
		msm_camera_vio_w(0x7f010a00 | val, csid_dev->base,
			csid_dev->ctrl_reg->csid_reg.csid_irq_mask_addr,
			csid_dev->pdev->id);
		msm_camera_vio_w(0x7f010a00 | val, csid_dev->base,
			csid_dev->ctrl_reg->csid_reg.csid_irq_clear_cmd_addr,
			csid_dev->pdev->id);
	} else {
		if (csid_dev->csid_3p_enabled == 1) {
			val = ((1 << csid_params->lane_cnt) - 1) <<
			csid_dev->ctrl_reg->csid_reg
				.csid_err_lane_overflow_offset_3p;
		} else {
			val = ((1 << csid_params->lane_cnt) - 1) <<
			csid_dev->ctrl_reg->csid_reg
				.csid_err_lane_overflow_offset_2p;
		}
		val |= csid_dev->ctrl_reg->csid_reg.csid_irq_mask_val;
		val |= SHORT_PKT_OFFSET;
		msm_camera_vio_w(val, csid_dev->base,
			csid_dev->ctrl_reg->csid_reg.csid_irq_mask_addr,
			csid_dev->pdev->id);
		msm_camera_vio_w(val, csid_dev->base,
			csid_dev->ctrl_reg->csid_reg.csid_irq_clear_cmd_addr,
			csid_dev->pdev->id);
	}
}
#else
static void msm_csid_set_debug_reg(struct csid_device *csid_dev,
	struct msm_camera_csid_params *csid_params)
{
}
#endif

static void msm_csid_set_sof_freeze_debug_reg(
	struct csid_device *csid_dev, uint8_t irq_enable)
{
	uint32_t val = 0;

	if (csid_dev && msm_camera_tz_is_secured(
		MSM_CAMERA_TZ_IO_REGION_CSIDCORE0 +
			csid_dev->pdev->id)) {
		CDBG("%s, Skip set_sof_freeze_debug_reg in secure mode\n",
			__func__);
		return;
	}

	if (!irq_enable) {
		val = msm_camera_io_r(csid_dev->base +
			csid_dev->ctrl_reg->csid_reg.csid_irq_status_addr);
		msm_camera_io_w(val, csid_dev->base +
			csid_dev->ctrl_reg->csid_reg.csid_irq_clear_cmd_addr);
		msm_camera_io_w(0, csid_dev->base +
			csid_dev->ctrl_reg->csid_reg.csid_irq_mask_addr);
		return;
	}

	if (csid_dev->csid_3p_enabled == 1) {
		val = ((1 << csid_dev->current_csid_params.lane_cnt) - 1) <<
			csid_dev->ctrl_reg->csid_reg
				.csid_err_lane_overflow_offset_3p;
	} else {
		val = ((1 << csid_dev->current_csid_params.lane_cnt) - 1) <<
			csid_dev->ctrl_reg->csid_reg
				.csid_err_lane_overflow_offset_2p;
	}
	val |= csid_dev->ctrl_reg->csid_reg.csid_irq_mask_val;
	val |= SHORT_PKT_OFFSET;
	msm_camera_io_w(val, csid_dev->base +
	csid_dev->ctrl_reg->csid_reg.csid_irq_mask_addr);
	msm_camera_io_w(val, csid_dev->base +
	csid_dev->ctrl_reg->csid_reg.csid_irq_clear_cmd_addr);
}

static int msm_csid_reset(struct csid_device *csid_dev)
{
	int32_t rc = 0;
	uint32_t irq = 0, irq_bitshift;

	CDBG("%s: id %d\n", __func__, csid_dev->pdev->id);

	if (msm_camera_tz_is_secured(
		MSM_CAMERA_TZ_IO_REGION_CSIDCORE0 + csid_dev->pdev->id)) {
		msm_camera_enable_irq(csid_dev->irq, false);
		rc = msm_camera_tz_reset_hw_block(
			csid_dev->ctrl_reg->csid_reg.csid_rst_stb_all,
			MSM_CAMERA_TZ_IO_REGION_CSIDCORE0 + csid_dev->pdev->id);
	} else {
		irq_bitshift =
		csid_dev->ctrl_reg->csid_reg.csid_rst_done_irq_bitshift;
		msm_camera_vio_w(csid_dev->ctrl_reg->csid_reg.csid_rst_stb_all,
			csid_dev->base,
			csid_dev->ctrl_reg->csid_reg.csid_rst_cmd_addr,
			csid_dev->pdev->id);
		rc = wait_for_completion_timeout(&csid_dev->reset_complete,
			CSID_TIMEOUT);
	}
	if (rc < 0) {
		pr_err("wait_for_completion in %s fail rc = %d\n",
			__func__, rc);
	} else if (rc == 0) {
		irq = msm_camera_vio_r(csid_dev->base,
			csid_dev->ctrl_reg->csid_reg.csid_irq_status_addr,
			csid_dev->pdev->id);
		pr_err_ratelimited("%s CSID%d_IRQ_STATUS_ADDR = 0x%x\n",
			__func__, csid_dev->pdev->id, irq);
		if (irq & (0x1 << irq_bitshift)) {
			rc = 1;
			CDBG("%s succeeded", __func__);
		} else {
			rc = 0;
			pr_err("%s reset csid_irq_status failed = 0x%x\n",
				__func__, irq);
		}
		if (rc == 0)
			rc = -ETIMEDOUT;
	} else {
		CDBG("%s succeeded", __func__);
	}
	return rc;
}

static bool msm_csid_find_max_clk_rate(struct csid_device *csid_dev)
{
	int i;
	bool ret = FALSE;

	for (i = 0; i < csid_dev->num_clk; i++) {
		if (!strcmp(csid_dev->csid_clk_info[i].clk_name,
			 "csi_src_clk")) {
			CDBG("%s:%d, copy csi_src_clk, clk_rate[%d] = %ld",
				__func__, __LINE__, i,
				csid_dev->csid_clk_info[i].clk_rate);
			csid_dev->csid_max_clk =
				 csid_dev->csid_clk_info[i].clk_rate;
			csid_dev->csid_clk_index = i;
			ret = TRUE;
			break;
		}
	}
	return ret;
}

static int msm_csid_seccam_send_topology(struct csid_device *csid_dev,
	struct msm_camera_csid_params *csid_params)
{
	void __iomem *csidbase;
	struct scm_desc desc = {0};

	csidbase = csid_dev->base;
	if (!csidbase || !csid_params) {
		pr_err("%s:%d csidbase %pK, csid params %pK\n", __func__,
			__LINE__, csidbase, csid_params);
		return -EINVAL;
	}

	desc.arginfo = SCM_ARGS(2, SCM_VAL, SCM_VAL);
	desc.args[0] = csid_params->phy_sel;
	desc.args[1] = csid_params->topology;

	CDBG("phy_sel %d, topology %d\n",
		csid_params->phy_sel, csid_params->topology);
	if (scm_call2(SCM_SIP_FNID(SCM_SVC_CAMERASS,
		TOPOLOGY_SYSCALL_ID), &desc)) {
		pr_err("%s:%d scm call to hypervisor failed\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	return 0;
}

static int msm_csid_seccam_reset_pipeline(struct csid_device *csid_dev,
	struct msm_camera_csid_params *csid_params)
{
	void __iomem *csidbase;
	struct scm_desc desc = {0};

	csidbase = csid_dev->base;
	if (!csidbase || !csid_params) {
		pr_err("%s:%d csidbase %pK, csid params %pK\n", __func__,
			__LINE__, csidbase, csid_params);
		return -EINVAL;
	}

	desc.arginfo = SCM_ARGS(2, SCM_VAL, SCM_VAL);
	desc.args[0] = csid_params->phy_sel;
	desc.args[1] = csid_params->is_streamon;

	CDBG("phy_sel %d, is_streamon %d\n",
		csid_params->phy_sel, csid_params->is_streamon);
	if (scm_call2(SCM_SIP_FNID(SCM_SVC_CAMERASS,
		STREAM_NOTIF_SYSCALL_ID), &desc)) {
		pr_err("%s:%d scm call to hypervisor failed\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	return 0;
}

static int msm_csid_config(struct csid_device *csid_dev,
	struct msm_camera_csid_params *csid_params)
{
	int rc = 0;
	uint32_t val = 0;
	long clk_rate = 0;
	uint32_t input_sel;
	uint32_t lane_assign = 0;
	uint8_t  lane_num = 0;
	uint8_t  i, j;
	void __iomem *csidbase;

	csidbase = csid_dev->base;
	if (!csidbase || !csid_params) {
		pr_err("%s:%d csidbase %pK, csid params %pK\n", __func__,
			__LINE__, csidbase, csid_params);
		return -EINVAL;
	}

	CDBG("%s csid_params, lane_cnt = %d, lane_assign = 0x%x\n",
		__func__,
		csid_params->lane_cnt,
		csid_params->lane_assign);
	CDBG("%s csid_params phy_sel = %d\n", __func__,
		csid_params->phy_sel);
	if ((csid_params->lane_cnt == 0) ||
		(csid_params->lane_cnt > MAX_LANE_COUNT)) {
		pr_err("%s:%d invalid lane count = %d\n",
			__func__, __LINE__, csid_params->lane_cnt);
		return -EINVAL;
	}

	if (csid_params->is_secure == 1) {
		struct scm_desc desc = {0};

		desc.arginfo = SCM_ARGS(2, SCM_VAL, SCM_VAL);
		desc.args[0] = csid_params->is_secure;
		desc.args[1] = CSIPHY_LANES_MASKS[csid_params->phy_sel];

		CDBG("phy_sel : %d, secure : %d\n",
			csid_params->phy_sel, csid_params->is_secure);

		msm_camera_tz_clear_tzbsp_status();

		if (scm_call2(SCM_SIP_FNID(SCM_SVC_CAMERASS,
			SECURE_SYSCALL_ID), &desc)) {
			pr_err("%s:%d scm call to hypervisor failed\n",
				__func__, __LINE__);
			return -EINVAL;
		}
	}

	csid_dev->csid_lane_cnt = csid_params->lane_cnt;
	rc = msm_csid_reset(csid_dev);
	if (rc < 0) {
		pr_err("%s:%d msm_csid_reset failed\n", __func__, __LINE__);
		return rc;
	}

	if (!msm_csid_find_max_clk_rate(csid_dev))
		pr_err("msm_csid_find_max_clk_rate failed\n");

	clk_rate = csid_dev->csid_max_clk;

	clk_rate = msm_camera_clk_set_rate(&csid_dev->pdev->dev,
		csid_dev->csid_clk[csid_dev->csid_clk_index], clk_rate);
	if (clk_rate < 0) {
		pr_err("csi_src_clk set failed\n");
		return -EINVAL;
	}

	if (csid_dev->is_testmode == 1) {
		struct msm_camera_csid_testmode_parms *tm;

		tm = &csid_dev->testmode_params;

		/* 31:24 V blank, 23:13 H blank, 3:2 num of active DT, 1:0 VC */
		val = ((tm->v_blanking_count & 0xFF) << 24) |
			((tm->h_blanking_count & 0x7FF) << 13);
		msm_camera_vio_w_def(val, csidbase,
			csid_dev->ctrl_reg->csid_reg.csid_tg_vc_cfg_addr,
			csid_dev->pdev->id);
		CDBG("[TG] CSID_TG_VC_CFG_ADDR 0x%08x\n", val);

		/* 28:16 bytes per lines, 12:0 num of lines */
		val = ((tm->num_bytes_per_line & 0x1FFF) << 16) |
			(tm->num_lines & 0x1FFF);
		msm_camera_vio_w_def(val, csidbase,
			csid_dev->ctrl_reg->csid_reg.csid_tg_dt_n_cfg_0_addr,
			csid_dev->pdev->id);
		CDBG("[TG] CSID_TG_DT_n_CFG_0_ADDR 0x%08x\n", val);

		/* 5:0 data type */
		val = csid_params->lut_params.vc_cfg[0]->dt;
		msm_camera_vio_w_def(val, csidbase,
			csid_dev->ctrl_reg->csid_reg.csid_tg_dt_n_cfg_1_addr,
			csid_dev->pdev->id);
		CDBG("[TG] CSID_TG_DT_n_CFG_1_ADDR 0x%08x\n", val);

		/* 2:0 output random */
		msm_camera_vio_w(csid_dev->testmode_params.payload_mode,
			csidbase,
			csid_dev->ctrl_reg->csid_reg.csid_tg_dt_n_cfg_2_addr,
			csid_dev->pdev->id);
	} else {
		val = csid_params->lane_cnt - 1;

		for (i = 0, j = 0; i < PHY_LANE_MAX; i++) {
			if (i == PHY_LANE_CLK)
				continue;
			lane_num = (csid_params->lane_assign >> j) & 0xF;
			if (lane_num >= PHY_LANE_MAX) {
				pr_err("%s:%d invalid lane number %d\n",
					__func__, __LINE__, lane_num);
				return -EINVAL;
			}
			if (csid_dev->ctrl_reg->csid_lane_assign[lane_num] >=
				PHY_LANE_MAX){
				pr_err("%s:%d invalid lane map %d\n",
					__func__, __LINE__,
					csid_dev->ctrl_reg->csid_lane_assign[
						lane_num]);
				return -EINVAL;
			}
			lane_assign |=
				csid_dev->ctrl_reg->csid_lane_assign[lane_num]
				<< j;
			j += 4;
		}

		CDBG("%s csid_params calculated lane_assign = 0x%X\n",
			__func__, lane_assign);

		val |= lane_assign <<
			csid_dev->ctrl_reg->csid_reg.csid_dl_input_sel_shift;
		if (csid_dev->hw_version < CSID_VERSION_V30) {
			val |= (0xF << 10);
			msm_camera_vio_w(val, csidbase,
			    csid_dev->ctrl_reg->csid_reg.csid_core_ctrl_0_addr,
			    csid_dev->pdev->id);
		} else {
			msm_camera_vio_w(val, csidbase,
			    csid_dev->ctrl_reg->csid_reg.csid_core_ctrl_0_addr,
				csid_dev->pdev->id);
			val = csid_params->phy_sel <<
			    csid_dev->ctrl_reg->csid_reg.csid_phy_sel_shift;
			val |= 0xF;
			msm_camera_vio_w(val, csidbase,
			    csid_dev->ctrl_reg->csid_reg.csid_core_ctrl_1_addr,
				csid_dev->pdev->id);
		}
		if (csid_dev->hw_version >= CSID_VERSION_V35 &&
			csid_params->csi_3p_sel == 1) {
			csid_dev->csid_3p_enabled = 1;
			val = (csid_params->lane_cnt - 1) << ENABLE_3P_BIT;

			for (i = 0; i < csid_params->lane_cnt; i++) {
				input_sel =
					(csid_params->lane_assign >> (4*i))
					& 0xF;
				val |= input_sel << (4*(i+1));
			}
			val |= csid_params->phy_sel <<
			    csid_dev->ctrl_reg->csid_reg.csid_phy_sel_shift_3p;
			val |= ENABLE_3P_BIT;
			msm_camera_vio_w(val, csidbase,
			    csid_dev->ctrl_reg->csid_reg.csid_3p_ctrl_0_addr,
				csid_dev->pdev->id);
		}
	}

	rc = msm_csid_cid_lut(&csid_params->lut_params, csid_dev);
	if (rc < 0) {
		pr_err("%s:%d config cid lut failed\n", __func__, __LINE__);
		return rc;
	}
	msm_csid_set_debug_reg(csid_dev, csid_params);

	if (csid_dev->is_testmode == 1)
		msm_camera_vio_w(0x00A06437, csidbase,
			csid_dev->ctrl_reg->csid_reg.csid_tg_ctrl_addr,
			csid_dev->pdev->id);

	return rc;
}

#if SHORT_PKT_CAPTURE
static irqreturn_t msm_csid_irq(int irq_num, void *data)
{
	uint32_t irq;
	uint32_t short_dt = 0;
	uint32_t count = 0, dt = 0;
	struct csid_device *csid_dev = data;

	if (!csid_dev) {
		pr_err("%s:%d csid_dev NULL\n", __func__, __LINE__);
		return IRQ_HANDLED;
	}
	irq = msm_camera_io_r(csid_dev->base +
		csid_dev->ctrl_reg->csid_reg.csid_irq_status_addr);
	CDBG("%s CSID%d_IRQ_STATUS_ADDR = 0x%x\n",
		 __func__, csid_dev->pdev->id, irq);
	if (irq & (0x1 <<
		csid_dev->ctrl_reg->csid_reg.csid_rst_done_irq_bitshift))
		complete(&csid_dev->reset_complete);
	if (irq & SHORT_PKT_OFFSET) {
		short_dt = msm_camera_io_r(csid_dev->base +
			csid_dev->ctrl_reg->csid_reg
				.csid_captured_short_pkt_addr);
		count = (short_dt >> 8) & 0xffff;
		dt =  short_dt >> 24;
		CDBG("CSID:: %s:%d core %d dt: 0x%x, count: %d\n",
			__func__, __LINE__, csid_dev->pdev->id, dt, count);
		msm_camera_io_w(0x101, csid_dev->base +
		csid_dev->ctrl_reg->csid_reg.csid_rst_cmd_addr);
	}
	msm_camera_io_w(irq, csid_dev->base +
		csid_dev->ctrl_reg->csid_reg.csid_irq_clear_cmd_addr);
	return IRQ_HANDLED;
}
#else
static irqreturn_t msm_csid_irq(int irq_num, void *data)
{
	uint32_t irq;
	struct csid_device *csid_dev = data;

	if (!csid_dev) {
		pr_err("%s:%d csid_dev NULL\n", __func__, __LINE__);
		return IRQ_HANDLED;
	}

	/* In secure mode interrupt is handeled by the TZ */
	if (csid_dev && msm_camera_tz_is_secured(
		MSM_CAMERA_TZ_IO_REGION_CSIDCORE0 + csid_dev->pdev->id)) {
		CDBG("%s, skip\n", __func__);
		return IRQ_HANDLED;
	}

	if (csid_dev->csid_sof_debug == SOF_DEBUG_ENABLE) {
		if (csid_dev->csid_sof_debug_count < CSID_SOF_DEBUG_COUNT)
			csid_dev->csid_sof_debug_count++;
		else {
			msm_csid_set_sof_freeze_debug_reg(csid_dev, false);
			return IRQ_HANDLED;
		}
	}

	irq = msm_camera_io_r(csid_dev->base +
		csid_dev->ctrl_reg->csid_reg.csid_irq_status_addr);
	pr_err_ratelimited("%s CSID%d_IRQ_STATUS_ADDR = 0x%x\n",
		 __func__, csid_dev->pdev->id, irq);
	if (irq & (0x1 <<
		csid_dev->ctrl_reg->csid_reg.csid_rst_done_irq_bitshift))
		complete(&csid_dev->reset_complete);
	msm_camera_io_w(irq, csid_dev->base +
		csid_dev->ctrl_reg->csid_reg.csid_irq_clear_cmd_addr);
	return IRQ_HANDLED;
}
#endif

static int msm_csid_irq_routine(struct v4l2_subdev *sd, u32 status,
	bool *handled)
{
	struct csid_device *csid_dev = v4l2_get_subdevdata(sd);
	irqreturn_t ret;

	CDBG("%s E\n", __func__);
	ret = msm_csid_irq(csid_dev->irq->start, csid_dev);
	*handled = TRUE;
	return 0;
}

static int msm_csid_init(struct csid_device *csid_dev, uint32_t *csid_version)
{
	int rc = 0;

	if (!csid_version) {
		pr_err("%s:%d csid_version NULL\n", __func__, __LINE__);
		rc = -EINVAL;
		return rc;
	}

	csid_dev->csid_sof_debug_count = 0;
	csid_dev->reg_ptr = NULL;

	if (csid_dev->csid_state == CSID_POWER_UP) {
		pr_err("%s: csid invalid state %d\n", __func__,
			csid_dev->csid_state);
		return -EINVAL;
	}

	rc = cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_CSID,
			CAM_AHB_SVS_VOTE);
	if (rc < 0) {
		pr_err("%s: failed to vote for AHB\n", __func__);
		return rc;
	}

	pr_info("%s: CSID_VERSION = 0x%x\n", __func__,
		csid_dev->ctrl_reg->csid_reg.csid_version);
	/* power up */
	rc = msm_camera_config_vreg(&csid_dev->pdev->dev, csid_dev->csid_vreg,
		csid_dev->regulator_count, NULL, 0,
		&csid_dev->csid_reg_ptr[0], 1);
	if (rc < 0) {
		pr_err("%s:%d csid config_vreg failed\n", __func__, __LINE__);
		goto top_vreg_config_failed;
	}

	rc = msm_camera_config_vreg(&csid_dev->pdev->dev,
		csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
		NULL, 0, &csid_dev->csi_vdd, 1);
	if (rc < 0) {
		pr_err("%s: regulator on failed\n", __func__);
		goto csid_vreg_config_failed;
	}

	rc = msm_camera_enable_vreg(&csid_dev->pdev->dev, csid_dev->csid_vreg,
		csid_dev->regulator_count, NULL, 0,
		&csid_dev->csid_reg_ptr[0], 1);
	if (rc < 0) {
		pr_err("%s:%d csid enable_vreg failed\n", __func__, __LINE__);
		goto top_vreg_enable_failed;
	}

	rc = msm_camera_enable_vreg(&csid_dev->pdev->dev,
		csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
		NULL, 0, &csid_dev->csi_vdd, 1);
	if (rc < 0) {
		pr_err("%s: regulator enable failed\n", __func__);
		goto csid_vreg_enable_failed;
	}
	rc = msm_camera_clk_enable(&csid_dev->pdev->dev,
		csid_dev->csid_clk_info, csid_dev->csid_clk,
		csid_dev->num_clk, true);
	if (rc < 0) {
		pr_err("%s:%d clock enable failed\n",
			 __func__, __LINE__);
		goto clk_enable_failed;
	}
	CDBG("%s:%d called\n", __func__, __LINE__);
	csid_dev->hw_version =
		msm_camera_vio_r(csid_dev->base,
			csid_dev->ctrl_reg->csid_reg.csid_hw_version_addr,
			csid_dev->pdev->id);
	CDBG("%s:%d called csid_dev->hw_version %x\n", __func__, __LINE__,
		csid_dev->hw_version);
	*csid_version = csid_dev->hw_version;
	csid_dev->csid_sof_debug = SOF_DEBUG_DISABLE;

	csid_dev->is_testmode = 0;

	init_completion(&csid_dev->reset_complete);
	if (msm_camera_tz_is_secured(
		MSM_CAMERA_TZ_IO_REGION_CSIDCORE0 + csid_dev->pdev->id)) {
		/* TEE is not accesible from IRQ handler context.
		 * Switch IRQ off and rely on TEE to handle interrupt in
		 * polling mode
		 */
		rc = msm_camera_enable_irq(csid_dev->irq, false);
	} else {
		rc = msm_camera_enable_irq(csid_dev->irq, true);
		if (rc < 0)
			pr_err("%s: irq enable failed\n", __func__);
	}
	rc = msm_csid_reset(csid_dev);
	if (rc < 0) {
		pr_err("%s:%d msm_csid_reset failed\n", __func__, __LINE__);
		goto msm_csid_reset_fail;
	}

	csid_dev->csid_state = CSID_POWER_UP;
	return rc;

msm_csid_reset_fail:
	msm_camera_enable_irq(csid_dev->irq, false);
	msm_camera_clk_enable(&csid_dev->pdev->dev, csid_dev->csid_clk_info,
		csid_dev->csid_clk, csid_dev->num_clk, false);
clk_enable_failed:
	msm_camera_enable_vreg(&csid_dev->pdev->dev,
		csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
		NULL, 0, &csid_dev->csi_vdd, 0);
csid_vreg_enable_failed:
	msm_camera_enable_vreg(&csid_dev->pdev->dev, csid_dev->csid_vreg,
		csid_dev->regulator_count, NULL, 0,
		&csid_dev->csid_reg_ptr[0], 0);
top_vreg_enable_failed:
	msm_camera_config_vreg(&csid_dev->pdev->dev,
		csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
		NULL, 0, &csid_dev->csi_vdd, 0);
csid_vreg_config_failed:
	msm_camera_config_vreg(&csid_dev->pdev->dev, csid_dev->csid_vreg,
		csid_dev->regulator_count, NULL, 0,
		&csid_dev->csid_reg_ptr[0], 0);
top_vreg_config_failed:
	if (cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_CSID,
		CAM_AHB_SUSPEND_VOTE) < 0)
		pr_err("%s: failed to remove vote from AHB\n", __func__);
	return rc;
}

static int msm_csid_release(struct csid_device *csid_dev)
{
	uint32_t irq;

	if (csid_dev->csid_state != CSID_POWER_UP) {
		pr_err("%s: csid invalid state %d\n", __func__,
			csid_dev->csid_state);
		return -EINVAL;
	}

	CDBG("%s:%d, hw_version = 0x%x\n", __func__, __LINE__,
		csid_dev->hw_version);

	if (csid_dev->current_csid_params.is_secure == 1) {
		struct scm_desc desc = {0};

		desc.arginfo = SCM_ARGS(2, SCM_VAL, SCM_VAL);
		desc.args[0] = 0;
		desc.args[1] = CSIPHY_LANES_MASKS[
				csid_dev->current_csid_params.phy_sel];

		if (scm_call2(SCM_SIP_FNID(SCM_SVC_CAMERASS,
			SECURE_SYSCALL_ID), &desc)) {
			pr_err("%s:%d scm call to hyp with protect 0 failed\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		msm_camera_tz_clear_tzbsp_status();
	}

	irq = msm_camera_vio_r(csid_dev->base,
		csid_dev->ctrl_reg->csid_reg.csid_irq_status_addr,
		csid_dev->pdev->id);
	msm_camera_vio_w(irq, csid_dev->base,
		csid_dev->ctrl_reg->csid_reg.csid_irq_clear_cmd_addr,
		csid_dev->pdev->id);
	msm_camera_vio_w(0, csid_dev->base,
		csid_dev->ctrl_reg->csid_reg.csid_irq_mask_addr,
		csid_dev->pdev->id);

	msm_camera_enable_irq(csid_dev->irq, false);

	msm_camera_clk_enable(&csid_dev->pdev->dev,
		csid_dev->csid_clk_info,
		csid_dev->csid_clk,
		csid_dev->num_clk, false);

	msm_camera_enable_vreg(&csid_dev->pdev->dev,
		csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
		NULL, 0, &csid_dev->csi_vdd, 0);

	msm_camera_enable_vreg(&csid_dev->pdev->dev,
		csid_dev->csid_vreg, csid_dev->regulator_count, NULL,
		0, &csid_dev->csid_reg_ptr[0], 0);

	msm_camera_config_vreg(&csid_dev->pdev->dev,
		csid_vreg_info, ARRAY_SIZE(csid_vreg_info),
		NULL, 0, &csid_dev->csi_vdd, 0);

	msm_camera_config_vreg(&csid_dev->pdev->dev,
		csid_dev->csid_vreg, csid_dev->regulator_count, NULL,
		0, &csid_dev->csid_reg_ptr[0], 0);

	if (!IS_ERR_OR_NULL(csid_dev->reg_ptr)) {
		regulator_disable(csid_dev->reg_ptr);
		regulator_put(csid_dev->reg_ptr);
	}

	csid_dev->csid_state = CSID_POWER_DOWN;

	if (cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_CSID,
		CAM_AHB_SUSPEND_VOTE) < 0)
		pr_err("%s: failed to remove vote from AHB\n", __func__);
	return 0;
}

static int32_t msm_csid_cmd(struct csid_device *csid_dev, void *arg)
{
	int rc = 0;
	struct csid_cfg_data *cdata = (struct csid_cfg_data *)arg;

	if (!csid_dev || !cdata) {
		pr_err("%s:%d csid_dev %pK, cdata %pK\n", __func__, __LINE__,
			csid_dev, cdata);
		return -EINVAL;
	}
	CDBG("%s cfgtype = %d\n", __func__, cdata->cfgtype);
	switch (cdata->cfgtype) {
	case CSID_INIT:
		rc = msm_csid_init(csid_dev, &cdata->cfg.csid_version);
		CDBG("%s csid version 0x%x\n", __func__,
			cdata->cfg.csid_version);
		break;
	case CSID_TESTMODE_CFG: {
		csid_dev->is_testmode = 1;
		if (copy_from_user(&csid_dev->testmode_params,
			(void __user *)cdata->cfg.csid_testmode_params,
			sizeof(struct msm_camera_csid_testmode_parms))) {
			pr_err("%s: %d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		break;
	}
	case CSID_CFG: {
		struct msm_camera_csid_params csid_params;
		struct msm_camera_csid_vc_cfg *vc_cfg = NULL;
		int i = 0;

		if (copy_from_user(&csid_params,
			(void __user *)cdata->cfg.csid_params,
			sizeof(struct msm_camera_csid_params))) {
			pr_err("%s: %d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		if (csid_params.lut_params.num_cid < 1 ||
			csid_params.lut_params.num_cid > MAX_CID) {
			pr_err("%s: %d num_cid outside range\n",
				 __func__, __LINE__);
			rc = -EINVAL;
			break;
		}
		for (i = 0; i < csid_params.lut_params.num_cid; i++) {
			vc_cfg = kzalloc(sizeof(struct msm_camera_csid_vc_cfg),
				GFP_KERNEL);
			if (!vc_cfg) {
				rc = -ENOMEM;
				goto MEM_CLEAN;
			}
			if (copy_from_user(vc_cfg,
				(void __user *)csid_params.lut_params.vc_cfg[i],
				sizeof(struct msm_camera_csid_vc_cfg))) {
				pr_err("%s: %d failed\n", __func__, __LINE__);
				kfree(vc_cfg);
				rc = -EFAULT;
				goto MEM_CLEAN;
			}
			csid_params.lut_params.vc_cfg[i] = vc_cfg;
		}
		csid_dev->current_csid_params = csid_params;
		csid_dev->csid_sof_debug = SOF_DEBUG_DISABLE;
		rc = msm_csid_config(csid_dev, &csid_params);
MEM_CLEAN:
		for (i--; i >= 0; i--)
			kfree(csid_params.lut_params.vc_cfg[i]);
		break;
	}
	case CSID_SECCAM_TOPOLOGY: {
		struct msm_camera_csid_params csid_params;

		if (copy_from_user(&csid_params,
			(void __user *)cdata->cfg.csid_params,
			sizeof(struct msm_camera_csid_params))) {
			pr_err("%s: %d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		rc = msm_csid_seccam_send_topology(csid_dev, &csid_params);
		break;
	}
	case CSID_SECCAM_RESET: {
		struct msm_camera_csid_params csid_params;

		if (copy_from_user(&csid_params,
			(void __user *)cdata->cfg.csid_params,
			sizeof(struct msm_camera_csid_params))) {
			pr_err("%s: %d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		rc = msm_csid_seccam_reset_pipeline(csid_dev, &csid_params);
		break;
	}
	case CSID_RELEASE:
		rc = msm_csid_release(csid_dev);
		break;
	default:
		pr_err("%s: %d failed\n", __func__, __LINE__);
		rc = -ENOIOCTLCMD;
		break;
	}
	return rc;
}

static int32_t msm_csid_get_subdev_id(struct csid_device *csid_dev, void *arg)
{
	uint32_t *subdev_id = (uint32_t *)arg;

	if (!subdev_id) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		return -EINVAL;
	}
	*subdev_id = csid_dev->pdev->id;
	pr_debug("%s:%d subdev_id %d\n", __func__, __LINE__, *subdev_id);
	return 0;
}

static long msm_csid_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	int rc = -ENOIOCTLCMD;
	struct csid_device *csid_dev = v4l2_get_subdevdata(sd);

	mutex_lock(&csid_dev->mutex);
	CDBG("%s:%d id %d\n", __func__, __LINE__, csid_dev->pdev->id);
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
		rc = msm_csid_get_subdev_id(csid_dev, arg);
		break;
	case VIDIOC_MSM_CSID_IO_CFG:
		rc = msm_csid_cmd(csid_dev, arg);
		break;
	case MSM_SD_NOTIFY_FREEZE:
		if (csid_dev->csid_state != CSID_POWER_UP)
			break;
		if (csid_dev->csid_sof_debug == SOF_DEBUG_DISABLE) {
			csid_dev->csid_sof_debug = SOF_DEBUG_ENABLE;
			msm_csid_set_sof_freeze_debug_reg(csid_dev, true);
		}
		break;
	case MSM_SD_UNNOTIFY_FREEZE:
		if (csid_dev->csid_state != CSID_POWER_UP)
			break;
		csid_dev->csid_sof_debug = SOF_DEBUG_DISABLE;
		msm_csid_set_sof_freeze_debug_reg(csid_dev, false);
		break;
	case VIDIOC_MSM_CSID_RELEASE:
	case MSM_SD_SHUTDOWN:
		rc = msm_csid_release(csid_dev);
		break;
	default:
		pr_err_ratelimited("%s: command not found\n", __func__);
	}
	CDBG("%s:%d\n", __func__, __LINE__);
	mutex_unlock(&csid_dev->mutex);
	return rc;
}


#ifdef CONFIG_COMPAT
static int32_t msm_csid_cmd32(struct csid_device *csid_dev, void *arg)
{
	int rc = 0;
	struct csid_cfg_data *cdata;
	struct csid_cfg_data32 *arg32 =  (struct csid_cfg_data32 *) (arg);
	struct csid_cfg_data local_arg;

	local_arg.cfgtype = arg32->cfgtype;
	cdata = &local_arg;

	if (!csid_dev || !cdata) {
		pr_err("%s:%d csid_dev %pK, cdata %pK\n", __func__, __LINE__,
			csid_dev, cdata);
		return -EINVAL;
	}

	CDBG("%s cfgtype = %d\n", __func__, cdata->cfgtype);
	switch (cdata->cfgtype) {
	case CSID_INIT:
		rc = msm_csid_init(csid_dev, &cdata->cfg.csid_version);
		arg32->cfg.csid_version = local_arg.cfg.csid_version;
		CDBG("%s csid version 0x%x\n", __func__,
			cdata->cfg.csid_version);
		break;
	case CSID_TESTMODE_CFG: {
		csid_dev->is_testmode = 1;
		if (copy_from_user(&csid_dev->testmode_params,
			(void __user *)
			compat_ptr(arg32->cfg.csid_testmode_params),
			sizeof(struct msm_camera_csid_testmode_parms))) {
			pr_err("%s: %d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		break;
	}
	case CSID_CFG: {

		struct msm_camera_csid_params csid_params;
		struct msm_camera_csid_vc_cfg *vc_cfg = NULL;
		int i = 0;
		struct msm_camera_csid_lut_params32 lut_par32;
		struct msm_camera_csid_params32 csid_params32;
		struct msm_camera_csid_vc_cfg vc_cfg32;

		if (copy_from_user(&csid_params32,
			(void __user *)compat_ptr(arg32->cfg.csid_params),
			sizeof(struct msm_camera_csid_params32))) {
			pr_err("%s: %d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		csid_params.lane_cnt = csid_params32.lane_cnt;
		csid_params.lane_assign = csid_params32.lane_assign;
		csid_params.phy_sel = csid_params32.phy_sel;
		csid_params.csi_clk = csid_params32.csi_clk;
		csid_params.csi_3p_sel = csid_params32.csi_3p_sel;
		csid_params.is_secure = csid_params32.is_secure;

		lut_par32 = csid_params32.lut_params;
		csid_params.lut_params.num_cid = lut_par32.num_cid;

		if (csid_params.lut_params.num_cid < 1 ||
			csid_params.lut_params.num_cid > MAX_CID) {
			pr_err("%s: %d num_cid outside range %d\n", __func__,
				__LINE__, csid_params.lut_params.num_cid);
			rc = -EINVAL;
			break;
		}

		for (i = 0; i < lut_par32.num_cid; i++) {
			vc_cfg = kzalloc(sizeof(struct msm_camera_csid_vc_cfg),
				GFP_KERNEL);
			if (!vc_cfg) {
				rc = -ENOMEM;
				goto MEM_CLEAN32;
			}
			/* msm_camera_csid_vc_cfg size
			 * does not change in COMPAT MODE
			 */
			if (copy_from_user(&vc_cfg32,
				(void __user *)compat_ptr(lut_par32.vc_cfg[i]),
				sizeof(vc_cfg32))) {
				pr_err("%s: %d failed\n", __func__, __LINE__);
				kfree(vc_cfg);
				vc_cfg = NULL;
				rc = -EFAULT;
				goto MEM_CLEAN32;
			}
			vc_cfg->cid = vc_cfg32.cid;
			vc_cfg->dt = vc_cfg32.dt;
			vc_cfg->decode_format = vc_cfg32.decode_format;
			csid_params.lut_params.vc_cfg[i] = vc_cfg;
		}
		rc = msm_csid_config(csid_dev, &csid_params);
		csid_dev->current_csid_params = csid_params;

MEM_CLEAN32:
		for (i--; i >= 0; i--) {
			kfree(csid_params.lut_params.vc_cfg[i]);
			csid_params.lut_params.vc_cfg[i] = NULL;
		}
		break;
	}
	case CSID_SECCAM_TOPOLOGY: {
		struct msm_camera_csid_params csid_params;
		struct msm_camera_csid_params32 csid_params32;

		if (copy_from_user(&csid_params32,
			(void __user *)compat_ptr(arg32->cfg.csid_params),
			sizeof(struct msm_camera_csid_params32))) {
			pr_err("%s: %d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		csid_params.topology = csid_params32.topology;
		csid_params.phy_sel = csid_params32.phy_sel;

		rc = msm_csid_seccam_send_topology(csid_dev, &csid_params);
		break;
	}
	case CSID_SECCAM_RESET: {
		struct msm_camera_csid_params csid_params;
		struct msm_camera_csid_params32 csid_params32;

		if (copy_from_user(&csid_params32,
			(void __user *)compat_ptr(arg32->cfg.csid_params),
			sizeof(struct msm_camera_csid_params32))) {
			pr_err("%s: %d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		csid_params.is_streamon = csid_params32.is_streamon;
		csid_params.phy_sel = csid_params32.phy_sel;
		rc = msm_csid_seccam_reset_pipeline(csid_dev, &csid_params);
		break;
	}
	case CSID_RELEASE:
		rc = msm_csid_release(csid_dev);
		break;
	default:
		pr_err("%s: %d failed\n", __func__, __LINE__);
		rc = -ENOIOCTLCMD;
		break;
	}
	return rc;
}

static long msm_csid_subdev_ioctl32(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	int rc = -ENOIOCTLCMD;
	struct csid_device *csid_dev = v4l2_get_subdevdata(sd);

	mutex_lock(&csid_dev->mutex);
	CDBG("%s:%d id %d\n", __func__, __LINE__, csid_dev->pdev->id);
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
		rc = msm_csid_get_subdev_id(csid_dev, arg);
		break;
	case VIDIOC_MSM_CSID_IO_CFG32:
		rc = msm_csid_cmd32(csid_dev, arg);
		break;
	case MSM_SD_NOTIFY_FREEZE:
		if (csid_dev->csid_state != CSID_POWER_UP)
			break;
		if (csid_dev->csid_sof_debug == SOF_DEBUG_DISABLE) {
			csid_dev->csid_sof_debug = SOF_DEBUG_ENABLE;
			msm_csid_set_sof_freeze_debug_reg(csid_dev, true);
		}
		break;
	case MSM_SD_UNNOTIFY_FREEZE:
		if (csid_dev->csid_state != CSID_POWER_UP)
			break;
		csid_dev->csid_sof_debug = SOF_DEBUG_DISABLE;
		msm_csid_set_sof_freeze_debug_reg(csid_dev, false);
		break;
	case VIDIOC_MSM_CSID_RELEASE:
	case MSM_SD_SHUTDOWN:
		rc = msm_csid_release(csid_dev);
		break;
	default:
		pr_err_ratelimited("%s: command not found\n", __func__);
	}
	CDBG("%s:%d\n", __func__, __LINE__);
	mutex_unlock(&csid_dev->mutex);
	return rc;
}

static long msm_csid_subdev_do_ioctl32(
	struct file *file, unsigned int cmd, void *arg)
{
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);

	return msm_csid_subdev_ioctl32(sd, cmd, arg);
}

static long msm_csid_subdev_fops_ioctl32(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_csid_subdev_do_ioctl32);
}
#endif
static const struct v4l2_subdev_internal_ops msm_csid_internal_ops;

static struct v4l2_subdev_core_ops msm_csid_subdev_core_ops = {
	.ioctl = &msm_csid_subdev_ioctl,
	.interrupt_service_routine = msm_csid_irq_routine,
};

static const struct v4l2_subdev_ops msm_csid_subdev_ops = {
	.core = &msm_csid_subdev_core_ops,
};

static int csid_probe(struct platform_device *pdev)
{
	struct csid_device *new_csid_dev;
	uint32_t csi_vdd_voltage = 0;
	int rc = 0;

	new_csid_dev = kzalloc(sizeof(struct csid_device), GFP_KERNEL);
	if (!new_csid_dev)
		return -ENOMEM;

	CDBG("%s: entry\n", __func__);

	new_csid_dev->csid_3p_enabled = 0;
	new_csid_dev->ctrl_reg = NULL;
	new_csid_dev->ctrl_reg = kzalloc(sizeof(struct csid_ctrl_t),
		GFP_KERNEL);
	if (!new_csid_dev->ctrl_reg) {
		kfree(new_csid_dev);
		return -ENOMEM;
	}

	v4l2_subdev_init(&new_csid_dev->msm_sd.sd, &msm_csid_subdev_ops);
	v4l2_set_subdevdata(&new_csid_dev->msm_sd.sd, new_csid_dev);
	platform_set_drvdata(pdev, &new_csid_dev->msm_sd.sd);
	mutex_init(&new_csid_dev->mutex);

	if (pdev->dev.of_node) {
		rc = of_property_read_u32((&pdev->dev)->of_node,
			"cell-index", &pdev->id);
		if (rc < 0) {
			pr_err("%s:%d failed to read cell-index\n", __func__,
				__LINE__);
			goto csid_no_resource;
		}
		CDBG("%s device id %d\n", __func__, pdev->id);

		rc = of_property_read_u32((&pdev->dev)->of_node,
			"qcom,csi-vdd-voltage", &csi_vdd_voltage);
		if (rc < 0) {
			pr_err("%s:%d failed to read qcom,csi-vdd-voltage\n",
				__func__, __LINE__);
			goto csid_no_resource;
		}
		CDBG("%s:%d reading mipi_csi_vdd is %d\n", __func__, __LINE__,
			csi_vdd_voltage);

		csid_vreg_info[0].min_voltage = csi_vdd_voltage;
		csid_vreg_info[0].max_voltage = csi_vdd_voltage;
	}

	rc = msm_camera_get_clk_info(pdev, &new_csid_dev->csid_clk_info,
		&new_csid_dev->csid_clk, &new_csid_dev->num_clk);
	if (rc < 0) {
		pr_err("%s: msm_camera_get_clk_info failed", __func__);
		rc = -EFAULT;
		goto csid_no_resource;
	}

	rc = msm_camera_get_dt_vreg_data(pdev->dev.of_node,
		&(new_csid_dev->csid_vreg), &(new_csid_dev->regulator_count));
	if (rc < 0) {
		pr_err("%s: get vreg data from dtsi fail\n", __func__);
		rc = -EFAULT;
		goto csid_no_resource;
	}

	if ((new_csid_dev->regulator_count < 0) ||
		(new_csid_dev->regulator_count > MAX_REGULATOR)) {
		pr_err("%s: invalid reg count = %d, max is %d\n", __func__,
			new_csid_dev->regulator_count, MAX_REGULATOR);
		rc = -EFAULT;
		goto csid_no_resource;
	}

	new_csid_dev->base = msm_camera_get_reg_base(pdev, "csid", true);
	if (!new_csid_dev->base) {
		pr_err("%s: no mem resource?\n", __func__);
		rc = -ENODEV;
		goto csid_invalid_vreg_data;
	}
	new_csid_dev->irq = msm_camera_get_irq(pdev, "csid");
	if (!new_csid_dev->irq) {
		pr_err("%s: no irq resource?\n", __func__);
		rc = -ENODEV;
		goto csid_invalid_irq;
	}
	new_csid_dev->pdev = pdev;
	new_csid_dev->msm_sd.sd.internal_ops = &msm_csid_internal_ops;
	new_csid_dev->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(new_csid_dev->msm_sd.sd.name,
			ARRAY_SIZE(new_csid_dev->msm_sd.sd.name), "msm_csid");
	media_entity_pads_init(&new_csid_dev->msm_sd.sd.entity, 0, NULL);
	new_csid_dev->msm_sd.sd.entity.function = MSM_CAMERA_SUBDEV_CSID;
	new_csid_dev->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x5;
	msm_sd_register(&new_csid_dev->msm_sd);

#ifdef CONFIG_COMPAT
	msm_cam_copy_v4l2_subdev_fops(&msm_csid_v4l2_subdev_fops);
	msm_csid_v4l2_subdev_fops.compat_ioctl32 = msm_csid_subdev_fops_ioctl32;
	new_csid_dev->msm_sd.sd.devnode->fops = &msm_csid_v4l2_subdev_fops;
#endif

	rc = msm_camera_register_irq(pdev, new_csid_dev->irq,
		msm_csid_irq, IRQF_TRIGGER_RISING, "csid", new_csid_dev);
	if (rc < 0) {
		pr_err("%s: irq request fail\n", __func__);
		rc = -EBUSY;
		goto csid_invalid_irq;
	}
	rc = msm_camera_enable_irq(new_csid_dev->irq, false);
	if (rc < 0) {
		pr_err("%s Error registering irq ", __func__);
		rc = -EBUSY;
		goto csid_invalid_irq;
	}

	if (of_device_is_compatible(new_csid_dev->pdev->dev.of_node,
		"qcom,csid-v2.0")) {
		new_csid_dev->ctrl_reg->csid_reg = csid_v2_0;
		new_csid_dev->ctrl_reg->csid_lane_assign =
			csid_lane_assign_v2_0;
		new_csid_dev->hw_dts_version = CSID_VERSION_V20;
	} else if (of_device_is_compatible(new_csid_dev->pdev->dev.of_node,
		"qcom,csid-v2.2")) {
		new_csid_dev->ctrl_reg->csid_reg = csid_v2_2;
		new_csid_dev->ctrl_reg->csid_lane_assign =
			csid_lane_assign_v2_2;
		new_csid_dev->hw_dts_version = CSID_VERSION_V22;
	} else if (of_device_is_compatible(new_csid_dev->pdev->dev.of_node,
		"qcom,csid-v3.0")) {
		new_csid_dev->ctrl_reg->csid_reg = csid_v3_0;
		new_csid_dev->ctrl_reg->csid_lane_assign =
			csid_lane_assign_v3_0;
		new_csid_dev->hw_dts_version = CSID_VERSION_V30;
	} else if (of_device_is_compatible(new_csid_dev->pdev->dev.of_node,
		"qcom,csid-v4.0")) {
		new_csid_dev->ctrl_reg->csid_reg = csid_v3_0;
		new_csid_dev->ctrl_reg->csid_lane_assign =
			csid_lane_assign_v3_0;
		new_csid_dev->hw_dts_version = CSID_VERSION_V40;
	} else if (of_device_is_compatible(new_csid_dev->pdev->dev.of_node,
		"qcom,csid-v3.1")) {
		new_csid_dev->ctrl_reg->csid_reg = csid_v3_1;
		new_csid_dev->ctrl_reg->csid_lane_assign =
			csid_lane_assign_v3_1;
		new_csid_dev->hw_dts_version = CSID_VERSION_V31;
	} else if (of_device_is_compatible(new_csid_dev->pdev->dev.of_node,
		"qcom,csid-v3.2")) {
		new_csid_dev->ctrl_reg->csid_reg = csid_v3_2;
		new_csid_dev->ctrl_reg->csid_lane_assign =
			csid_lane_assign_v3_2;
		new_csid_dev->hw_dts_version = CSID_VERSION_V32;
	} else if (of_device_is_compatible(new_csid_dev->pdev->dev.of_node,
		"qcom,csid-v3.4.1")) {
		new_csid_dev->ctrl_reg->csid_reg = csid_v3_4_1;
		new_csid_dev->hw_dts_version = CSID_VERSION_V34_1;
		new_csid_dev->ctrl_reg->csid_lane_assign =
			csid_lane_assign_v3_4_1;
	} else if (of_device_is_compatible(new_csid_dev->pdev->dev.of_node,
		"qcom,csid-v3.4.2")) {
		new_csid_dev->ctrl_reg->csid_reg = csid_v3_4_2;
		new_csid_dev->hw_dts_version = CSID_VERSION_V34_2;
		new_csid_dev->ctrl_reg->csid_lane_assign =
			csid_lane_assign_v3_4_2;
	} else if (of_device_is_compatible(new_csid_dev->pdev->dev.of_node,
		"qcom,csid-v3.4.3")) {
		new_csid_dev->ctrl_reg->csid_reg = csid_v3_4_3;
		new_csid_dev->hw_dts_version = CSID_VERSION_V34_3;
		new_csid_dev->ctrl_reg->csid_lane_assign =
			csid_lane_assign_v3_4_3;
	} else if (of_device_is_compatible(new_csid_dev->pdev->dev.of_node,
		"qcom,csid-v3.6.0")) {
		new_csid_dev->ctrl_reg->csid_reg = csid_v3_6_0;
		new_csid_dev->hw_dts_version = CSID_VERSION_V36;
		new_csid_dev->ctrl_reg->csid_lane_assign =
			csid_lane_assign_v3_6_0;
	} else if (of_device_is_compatible(new_csid_dev->pdev->dev.of_node,
		"qcom,csid-v3.5")) {
		new_csid_dev->ctrl_reg->csid_reg = csid_v3_5;
		new_csid_dev->ctrl_reg->csid_lane_assign =
			csid_lane_assign_v3_5;
		new_csid_dev->hw_dts_version = CSID_VERSION_V35;
	} else if (of_device_is_compatible(new_csid_dev->pdev->dev.of_node,
		"qcom,csid-v5.0")) {
		new_csid_dev->ctrl_reg->csid_reg = csid_v3_5;
		new_csid_dev->ctrl_reg->csid_lane_assign =
			csid_lane_assign_v3_5;
		new_csid_dev->hw_dts_version = CSID_VERSION_V50;
	} else if (of_device_is_compatible(new_csid_dev->pdev->dev.of_node,
		"qcom,csid-v3.5.1")) {
		new_csid_dev->ctrl_reg->csid_reg = csid_v3_5_1;
		new_csid_dev->ctrl_reg->csid_lane_assign =
			csid_lane_assign_v3_5_1;
		new_csid_dev->hw_dts_version = CSID_VERSION_V35_1;
	} else {
		pr_err("%s:%d, invalid hw version : 0x%x", __func__, __LINE__,
			new_csid_dev->hw_dts_version);
		rc = -EINVAL;
		goto csid_invalid_irq;
	}

	new_csid_dev->csid_state = CSID_POWER_DOWN;
	return 0;

csid_invalid_irq:
	msm_camera_put_reg_base(pdev, new_csid_dev->base, "csid", true);
csid_invalid_vreg_data:
	kfree(new_csid_dev->csid_vreg);
csid_no_resource:
	mutex_destroy(&new_csid_dev->mutex);
	kfree(new_csid_dev->ctrl_reg);
	kfree(new_csid_dev);
	return rc;
}

static int msm_csid_exit(struct platform_device *pdev)
{
	struct v4l2_subdev *subdev = platform_get_drvdata(pdev);
	struct csid_device *csid_dev =
		v4l2_get_subdevdata(subdev);

	msm_camera_put_clk_info(pdev, &csid_dev->csid_clk_info,
		&csid_dev->csid_clk, csid_dev->num_clk);
	msm_camera_put_reg_base(pdev, csid_dev->base, "csid", true);
	kfree(csid_dev);
	return 0;
}

static const struct of_device_id msm_csid_dt_match[] = {
	{.compatible = "qcom,csid"},
	{}
};

MODULE_DEVICE_TABLE(of, msm_csid_dt_match);

static struct platform_driver csid_driver = {
	.probe = csid_probe,
	.remove = msm_csid_exit,
	.driver = {
		.name = MSM_CSID_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msm_csid_dt_match,
	},
};

static int __init msm_csid_init_module(void)
{
	return platform_driver_register(&csid_driver);
}

static void __exit msm_csid_exit_module(void)
{
	platform_driver_unregister(&csid_driver);
}

module_init(msm_csid_init_module);
module_exit(msm_csid_exit_module);
MODULE_DESCRIPTION("MSM CSID driver");
MODULE_LICENSE("GPL v2");
