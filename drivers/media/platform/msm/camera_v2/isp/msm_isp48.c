/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/ratelimit.h>
#include <linux/clk/msm-clk.h>

#include "msm_isp_util.h"
#include "msm_isp_axi_util.h"
#include "msm_isp_stats_util.h"
#include "msm_isp.h"
#include "msm.h"
#include "msm_camera_io_util.h"
#include "cam_hw_ops.h"
#include "msm_isp47.h"
#include "msm_isp48.h"
#include "cam_soc_api.h"

#define MSM_VFE48_BUS_CLIENT_INIT 0xABAB
#define VFE48_STATS_BURST_LEN 3
#define VFE48_UB_SIZE_VFE 2048 /* 2048 * 256 bits = 64KB */
#define VFE48_UB_STATS_SIZE 144
#define MSM_ISP48_TOTAL_IMAGE_UB_VFE (VFE48_UB_SIZE_VFE - VFE48_UB_STATS_SIZE)


static uint32_t stats_base_addr[] = {
	0x1D4, /* HDR_BE */
	0x254, /* BG(AWB_BG) */
	0x214, /* BF */
	0x1F4, /* HDR_BHIST */
	0x294, /* RS */
	0x2B4, /* CS */
	0x2D4, /* IHIST */
	0x274, /* BHIST (SKIN_BHIST) */
	0x234, /* AEC_BG */
};

#define VFE48_STATS_BASE(idx) (stats_base_addr[idx])

static struct msm_vfe_axi_hardware_info msm_vfe48_axi_hw_info = {
	.num_wm = 7,
	.num_comp_mask = 3,
	.num_rdi = 3,
	.num_rdi_master = 3,
	.min_wm_ub = 96,
	.scratch_buf_range = SZ_32M,
};

static uint8_t stats_pingpong_offset_map[] = {
	 8, /* HDR_BE */
	12, /* BG(AWB_BG) */
	10, /* BF */
	 9, /* HDR_BHIST */
	14, /* RS */
	15, /* CS */
	16, /* IHIST */
	13, /* BHIST (SKIN_BHIST) */
	11, /* AEC_BG */
};

static uint8_t stats_wm_index[] = {
	 7, /* HDR_BE */
	11, /* BG(AWB_BG) */
	 9, /* BF */
	 8, /* HDR_BHIST */
	13, /* RS */
	14, /* CS */
	15, /* IHIST */
	12, /* BHIST (SKIN_BHIST) */
	10, /* AEC_BG */
};

#define VFE48_SRC_CLK_DTSI_IDX 3

static struct msm_vfe_stats_hardware_info msm_vfe48_stats_hw_info = {
	.stats_capability_mask =
		1 << MSM_ISP_STATS_HDR_BE    | 1 << MSM_ISP_STATS_BF    |
		1 << MSM_ISP_STATS_BG	| 1 << MSM_ISP_STATS_BHIST |
		1 << MSM_ISP_STATS_HDR_BHIST | 1 << MSM_ISP_STATS_IHIST |
		1 << MSM_ISP_STATS_RS	| 1 << MSM_ISP_STATS_CS    |
		1 << MSM_ISP_STATS_AEC_BG,
	.stats_ping_pong_offset = stats_pingpong_offset_map,
	.stats_wm_index = stats_wm_index,
	.num_stats_type = VFE47_NUM_STATS_TYPE,
	.num_stats_comp_mask = VFE47_NUM_STATS_COMP,
};

static void msm_vfe48_axi_enable_wm(void __iomem *vfe_base,
	uint8_t wm_idx, uint8_t enable)
{
	uint32_t val;

	if (enable)
		val = (0x2 << (2 * wm_idx));
	else
		val = (0x1 << (2 * wm_idx));

	msm_camera_io_w_mb(val, vfe_base + 0xCEC);
}

static void msm_vfe48_enable_stats_wm(struct vfe_device *vfe_dev,
		uint32_t stats_mask, uint8_t enable)
{
	int i;

	for (i = 0; i < VFE47_NUM_STATS_TYPE; i++) {
		if (!(stats_mask & 0x1)) {
			stats_mask >>= 1;
			continue;
		}
		stats_mask >>= 1;
		msm_vfe48_axi_enable_wm(vfe_dev->vfe_base,
			vfe_dev->hw_info->stats_hw_info->stats_wm_index[i],
			enable);
	}
}

static void msm_vfe48_deinit_bandwidth_mgr(
		struct msm_isp_bandwidth_mgr *isp_bandwidth_mgr)
{
	msm_camera_unregister_bus_client(CAM_BUS_CLIENT_VFE);
	isp_bandwidth_mgr->bus_client = 0;
}

static int msm_vfe48_init_bandwidth_mgr(struct vfe_device *vfe_dev,
	struct msm_isp_bandwidth_mgr *isp_bandwidth_mgr)
{
	int rc = 0;

	rc = msm_camera_register_bus_client(vfe_dev->pdev, CAM_BUS_CLIENT_VFE);
	if (rc) {
		/*
		 * Fallback to the older method of registration. While testing
		 * this the bus apis that the soc api uses were not
		 * enabled and hence we need to use the old api for now
		 */
		vfe_dev->hw_info->vfe_ops.platform_ops.init_bw_mgr =
					msm_vfe47_init_bandwidth_mgr;
		vfe_dev->hw_info->vfe_ops.platform_ops.deinit_bw_mgr =
					msm_vfe47_deinit_bandwidth_mgr;
		vfe_dev->hw_info->vfe_ops.platform_ops.update_bw =
					msm_vfe47_update_bandwidth;
		return vfe_dev->hw_info->vfe_ops.platform_ops.init_bw_mgr(
						vfe_dev, isp_bandwidth_mgr);
	}
	isp_bandwidth_mgr->bus_client = MSM_VFE48_BUS_CLIENT_INIT;
	return rc;
}

static int msm_vfe48_update_bandwidth(
		struct msm_isp_bandwidth_mgr *isp_bandwidth_mgr)
{
	int i;
	uint64_t ab = 0;
	uint64_t ib = 0;
	int rc = 0;

	for (i = 0; i < MAX_ISP_CLIENT; i++) {
		if (isp_bandwidth_mgr->client_info[i].active) {
			ab += isp_bandwidth_mgr->client_info[i].ab;
			ib += isp_bandwidth_mgr->client_info[i].ib;
		}
	}
	rc = msm_camera_update_bus_bw(CAM_BUS_CLIENT_VFE, ab, ib);
	/* Insert into circular buffer */
	if (!rc)
		msm_isp_update_req_history(isp_bandwidth_mgr->bus_client,
			ab, ib,
			isp_bandwidth_mgr->client_info,
			sched_clock());
	return rc;
}

static void msm_vfe48_put_clks(struct vfe_device *vfe_dev)
{
	msm_camera_put_clk_info_and_rates(vfe_dev->pdev, &vfe_dev->vfe_clk_info,
			&vfe_dev->vfe_clk, &vfe_dev->vfe_clk_rates,
			vfe_dev->num_rates,
			vfe_dev->num_clk);

	vfe_dev->num_clk = 0;
	vfe_dev->num_rates = 0;
	vfe_dev->hvx_clk = NULL;
	vfe_dev->hvx_clk_info = NULL;
	vfe_dev->num_hvx_clk = 0;
	vfe_dev->num_norm_clk = 0;
}

static int msm_vfe48_get_clks(struct vfe_device *vfe_dev)
{
	int rc;
	int i, j;
	struct clk *stream_clk;
	struct msm_cam_clk_info clk_info;

	rc = msm_camera_get_clk_info_and_rates(vfe_dev->pdev,
			&vfe_dev->vfe_clk_info, &vfe_dev->vfe_clk,
			&vfe_dev->vfe_clk_rates,
			&vfe_dev->num_rates,
			&vfe_dev->num_clk);

	if (rc)
		return rc;
	vfe_dev->num_norm_clk = vfe_dev->num_clk;
	for (i = 0; i < vfe_dev->num_clk; i++) {
		if (strcmp(vfe_dev->vfe_clk_info[i].clk_name,
				"camss_vfe_stream_clk") == 0) {
			stream_clk = vfe_dev->vfe_clk[i];
			clk_info = vfe_dev->vfe_clk_info[i];
			vfe_dev->num_hvx_clk = 1;
			vfe_dev->num_norm_clk = vfe_dev->num_clk - 1;
			break;
		}
	}
	if (i >= vfe_dev->num_clk)
		pr_err("%s: cannot find camss_vfe_stream_clk\n", __func__);
	else {
		/* Switch stream_clk to the last element*/
		for (; i < vfe_dev->num_clk - 1; i++) {
			vfe_dev->vfe_clk[i] = vfe_dev->vfe_clk[i+1];
			vfe_dev->vfe_clk_info[i] = vfe_dev->vfe_clk_info[i+1];
			for (j = 0; j < MSM_VFE_MAX_CLK_RATES; j++)
				vfe_dev->vfe_clk_rates[j][i] =
					vfe_dev->vfe_clk_rates[j][i+1];
		}
		vfe_dev->vfe_clk_info[vfe_dev->num_clk-1] = clk_info;
		vfe_dev->vfe_clk[vfe_dev->num_clk-1] = stream_clk;
		vfe_dev->hvx_clk_info =
			&vfe_dev->vfe_clk_info[vfe_dev->num_clk-1];
		vfe_dev->hvx_clk = &vfe_dev->vfe_clk[vfe_dev->num_clk-1];
	}

	for (i = 0; i < vfe_dev->num_clk; i++) {
		if (strcmp(vfe_dev->vfe_clk_info[i].clk_name,
					"vfe_clk_src") == 0) {
			vfe_dev->hw_info->vfe_clk_idx = i;
			/* set initial clk rate to svs */
			msm_camera_clk_set_rate(&vfe_dev->pdev->dev,
				vfe_dev->vfe_clk[i],
				vfe_dev->vfe_clk_rates
					[MSM_VFE_CLK_RATE_SVS][i]);
		}
		if (strcmp(vfe_dev->vfe_clk_info[i].clk_name,
					"mnoc_maxi_clk") == 0)
			vfe_dev->vfe_clk_info[i].clk_rate = INIT_RATE;
		/* set no memory retention */
		if (strcmp(vfe_dev->vfe_clk_info[i].clk_name,
				"camss_vfe_clk") == 0 ||
			strcmp(vfe_dev->vfe_clk_info[i].clk_name,
				"camss_csi_vfe_clk") == 0 ||
			strcmp(vfe_dev->vfe_clk_info[i].clk_name,
				"camss_vfe_vbif_axi_clk") == 0) {
			msm_camera_set_clk_flags(vfe_dev->vfe_clk[i],
				 CLKFLAG_NORETAIN_MEM);
			msm_camera_set_clk_flags(vfe_dev->vfe_clk[i],
				 CLKFLAG_NORETAIN_PERIPH);
		}
	}
	return 0;
}

static int msm_vfe48_get_clk_rates(struct vfe_device *vfe_dev,
			struct msm_isp_clk_rates *rates)
{
	rates->svs_rate = vfe_dev->vfe_clk_rates[MSM_VFE_CLK_RATE_SVS]
					[vfe_dev->hw_info->vfe_clk_idx];
	rates->nominal_rate = vfe_dev->vfe_clk_rates[MSM_VFE_CLK_RATE_NOMINAL]
					[vfe_dev->hw_info->vfe_clk_idx];
	rates->high_rate = vfe_dev->vfe_clk_rates[MSM_VFE_CLK_RATE_TURBO]
					[vfe_dev->hw_info->vfe_clk_idx];

	return 0;
}

static int msm_vfe48_get_regulators(struct vfe_device *vfe_dev)
{
	return msm_camera_get_regulator_info(vfe_dev->pdev,
			&vfe_dev->regulator_info, &vfe_dev->vfe_num_regulators);
}

static void msm_vfe48_put_regulators(struct vfe_device *vfe_dev)
{
	msm_camera_put_regulators(vfe_dev->pdev,
			&vfe_dev->regulator_info, vfe_dev->vfe_num_regulators);
	vfe_dev->vfe_num_regulators = 0;
}

static void msm_vfe48_get_bus_err_mask(struct vfe_device *vfe_dev,
		uint32_t *bus_err, uint32_t *irq_status1)
{
	*bus_err = msm_camera_io_r(vfe_dev->vfe_base + 0xC94);

	*bus_err &= ~vfe_dev->bus_err_ign_mask;
	if (*bus_err == 0)
		*irq_status1 &= ~(1 << 4);
}

static void msm_vfe48_set_bus_err_ign_mask(struct vfe_device *vfe_dev,
				int wm, int enable)
{
	if (enable)
		vfe_dev->bus_err_ign_mask |= (1 << wm);
	else
		vfe_dev->bus_err_ign_mask &= ~(1 << wm);
}

void msm_vfe48_stats_cfg_ub(struct vfe_device *vfe_dev)
{
	int i;
	uint32_t ub_offset = 0, stats_burst_len;
	uint32_t ub_size[VFE47_NUM_STATS_TYPE] = {
		16, /* MSM_ISP_STATS_HDR_BE */
		16, /* MSM_ISP_STATS_BG */
		16, /* MSM_ISP_STATS_BF */
		16, /* MSM_ISP_STATS_HDR_BHIST */
		16, /* MSM_ISP_STATS_RS */
		16, /* MSM_ISP_STATS_CS */
		16, /* MSM_ISP_STATS_IHIST */
		16, /* MSM_ISP_STATS_BHIST */
		16, /* MSM_ISP_STATS_AEC_BG */
	};

	stats_burst_len = VFE48_STATS_BURST_LEN;
	ub_offset = VFE48_UB_SIZE_VFE;

	for (i = 0; i < VFE47_NUM_STATS_TYPE; i++) {
		ub_offset -= ub_size[i];
		msm_camera_io_w(stats_burst_len << 30 |
			ub_offset << 16 | (ub_size[i] - 1),
			vfe_dev->vfe_base + VFE48_STATS_BASE(i) + 0x14);
	}
}

uint32_t msm_vfe48_get_ub_size(struct vfe_device *vfe_dev)
{
	return MSM_ISP48_TOTAL_IMAGE_UB_VFE;
}



struct msm_vfe_hardware_info vfe48_hw_info = {
	.num_iommu_ctx = 1,
	.num_iommu_secure_ctx = 0,
	.vfe_clk_idx = VFE48_SRC_CLK_DTSI_IDX,
	.runtime_axi_update = 1,
	.min_ib = 100000000,
	.min_ab = 100000000,
	.vfe_ops = {
		.irq_ops = {
			.read_and_clear_irq_status =
				msm_vfe47_read_and_clear_irq_status,
			.read_irq_status = msm_vfe47_read_irq_status,
			.process_camif_irq = msm_vfe47_process_input_irq,
			.process_reset_irq = msm_vfe47_process_reset_irq,
			.process_halt_irq = msm_vfe47_process_halt_irq,
			.process_reset_irq = msm_vfe47_process_reset_irq,
			.process_reg_update = msm_vfe47_process_reg_update,
			.process_axi_irq = msm_isp_process_axi_irq,
			.process_stats_irq = msm_isp_process_stats_irq,
			.process_epoch_irq = msm_vfe47_process_epoch_irq,
			.config_irq = msm_vfe47_config_irq,
			.preprocess_camif_irq = msm_isp47_preprocess_camif_irq,
		},
		.axi_ops = {
			.reload_wm = msm_vfe47_axi_reload_wm,
			.enable_wm = msm_vfe48_axi_enable_wm,
			.cfg_io_format = msm_vfe47_cfg_io_format,
			.cfg_comp_mask = msm_vfe47_axi_cfg_comp_mask,
			.clear_comp_mask = msm_vfe47_axi_clear_comp_mask,
			.cfg_wm_irq_mask = msm_vfe47_axi_cfg_wm_irq_mask,
			.clear_wm_irq_mask = msm_vfe47_axi_clear_wm_irq_mask,
			.cfg_framedrop = msm_vfe47_cfg_framedrop,
			.clear_framedrop = msm_vfe47_clear_framedrop,
			.cfg_wm_reg = msm_vfe47_axi_cfg_wm_reg,
			.clear_wm_reg = msm_vfe47_axi_clear_wm_reg,
			.cfg_wm_xbar_reg = msm_vfe47_axi_cfg_wm_xbar_reg,
			.clear_wm_xbar_reg = msm_vfe47_axi_clear_wm_xbar_reg,
			.cfg_ub = msm_vfe47_cfg_axi_ub,
			.read_wm_ping_pong_addr =
				msm_vfe47_read_wm_ping_pong_addr,
			.update_ping_pong_addr =
				msm_vfe47_update_ping_pong_addr,
			.get_comp_mask = msm_vfe47_get_comp_mask,
			.get_wm_mask = msm_vfe47_get_wm_mask,
			.get_pingpong_status = msm_vfe47_get_pingpong_status,
			.halt = msm_vfe47_axi_halt,
			.restart = msm_vfe47_axi_restart,
			.update_cgc_override =
				msm_vfe47_axi_update_cgc_override,
			.ub_reg_offset = msm_vfe47_ub_reg_offset,
			.get_ub_size = msm_vfe48_get_ub_size,
		},
		.core_ops = {
			.reg_update = msm_vfe47_reg_update,
			.cfg_input_mux = msm_vfe47_cfg_input_mux,
			.update_camif_state = msm_vfe47_update_camif_state,
			.start_fetch_eng = msm_vfe47_start_fetch_engine,
			.cfg_rdi_reg = msm_vfe47_cfg_rdi_reg,
			.reset_hw = msm_vfe47_reset_hardware,
			.init_hw = msm_vfe47_init_hardware,
			.init_hw_reg = msm_vfe47_init_hardware_reg,
			.clear_status_reg = msm_vfe47_clear_status_reg,
			.release_hw = msm_vfe47_release_hardware,
			.get_error_mask = msm_vfe47_get_error_mask,
			.get_overflow_mask = msm_vfe47_get_overflow_mask,
			.get_rdi_wm_mask = msm_vfe47_get_rdi_wm_mask,
			.get_irq_mask = msm_vfe47_get_irq_mask,
			.get_halt_restart_mask =
				msm_vfe47_get_halt_restart_mask,
			.process_error_status = msm_vfe47_process_error_status,
			.is_module_cfg_lock_needed =
				msm_vfe47_is_module_cfg_lock_needed,
			.ahb_clk_cfg = msm_isp47_ahb_clk_cfg,
			.start_fetch_eng_multi_pass =
				msm_vfe47_start_fetch_engine_multi_pass,
			.set_halt_restart_mask =
				msm_vfe47_set_halt_restart_mask,
			.set_bus_err_ign_mask = msm_vfe48_set_bus_err_ign_mask,
			.get_bus_err_mask = msm_vfe48_get_bus_err_mask,
		},
		.stats_ops = {
			.get_stats_idx = msm_vfe47_get_stats_idx,
			.check_streams = msm_vfe47_stats_check_streams,
			.cfg_comp_mask = msm_vfe47_stats_cfg_comp_mask,
			.cfg_wm_irq_mask = msm_vfe47_stats_cfg_wm_irq_mask,
			.clear_wm_irq_mask = msm_vfe47_stats_clear_wm_irq_mask,
			.cfg_wm_reg = msm_vfe47_stats_cfg_wm_reg,
			.clear_wm_reg = msm_vfe47_stats_clear_wm_reg,
			.cfg_ub = msm_vfe48_stats_cfg_ub,
			.enable_module = msm_vfe47_stats_enable_module,
			.update_ping_pong_addr =
				msm_vfe47_stats_update_ping_pong_addr,
			.get_comp_mask = msm_vfe47_stats_get_comp_mask,
			.get_wm_mask = msm_vfe47_stats_get_wm_mask,
			.get_frame_id = msm_vfe47_stats_get_frame_id,
			.get_pingpong_status = msm_vfe47_get_pingpong_status,
			.update_cgc_override =
				msm_vfe47_stats_update_cgc_override,
			.enable_stats_wm = msm_vfe48_enable_stats_wm,
		},
		.platform_ops = {
			.get_platform_data = msm_vfe47_get_platform_data,
			.enable_regulators = msm_vfe47_enable_regulators,
			.get_regulators = msm_vfe48_get_regulators,
			.put_regulators = msm_vfe48_put_regulators,
			.enable_clks = msm_vfe47_enable_clks,
			.update_bw = msm_vfe48_update_bandwidth,
			.init_bw_mgr = msm_vfe48_init_bandwidth_mgr,
			.deinit_bw_mgr = msm_vfe48_deinit_bandwidth_mgr,
			.get_clks = msm_vfe48_get_clks,
			.put_clks = msm_vfe48_put_clks,
			.set_clk_rate = msm_vfe47_set_clk_rate,
			.get_max_clk_rate = msm_vfe47_get_max_clk_rate,
			.get_clk_rates = msm_vfe48_get_clk_rates,
		},
	},
	.dmi_reg_offset = 0xC2C,
	.axi_hw_info = &msm_vfe48_axi_hw_info,
	.stats_hw_info = &msm_vfe48_stats_hw_info,
};
EXPORT_SYMBOL(vfe48_hw_info);

static const struct of_device_id msm_vfe48_dt_match[] = {
	{
		.compatible = "qcom,vfe48",
		.data = &vfe48_hw_info,
	},
	{}
};

MODULE_DEVICE_TABLE(of, msm_vfe48_dt_match);

static struct platform_driver vfe48_driver = {
	.probe = vfe_hw_probe,
	.driver = {
		.name = "msm_vfe48",
		.owner = THIS_MODULE,
		.of_match_table = msm_vfe48_dt_match,
	},
};

static int __init msm_vfe47_init_module(void)
{
	return platform_driver_register(&vfe48_driver);
}

static void __exit msm_vfe47_exit_module(void)
{
	platform_driver_unregister(&vfe48_driver);
}

module_init(msm_vfe47_init_module);
module_exit(msm_vfe47_exit_module);
MODULE_DESCRIPTION("MSM VFE48 driver");
MODULE_LICENSE("GPL v2");

