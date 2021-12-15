/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include "vdec_fmt_pm.h"
#include "vdec_fmt_ion.h"
#include "vdec_fmt_utils.h"
#include <linux/clk.h>

#include <linux/soc/mediatek/mtk-pm-qos.h>
#include <mmdvfs_pmqos.h>
#include "smi_public.h"

static struct mtk_pm_qos_request fmt_qos_req_f;
static u32 fmt_freq_step_size;
static u64 fmt_freq_steps[MAX_FREQ_STEP];

static struct plist_head fmt_rlist[FMT_CORE_NUM];
static struct mm_qos_request fmt_rdma_request[FMT_CORE_NUM];
static struct mm_qos_request fmt_wdma_request[FMT_CORE_NUM];

#ifdef CONFIG_MTK_PSEUDO_M4U
#include <mach/mt_iommu.h>
#include "mach/pseudo_m4u.h"
#include "smi_port.h"
#endif

void fmt_get_module_clock_by_name(struct mtk_vdec_fmt *fmt,
	const char *clkName, struct clk **clk_module)
{
	*clk_module = of_clk_get_by_name(fmt->dev->of_node, clkName);
	if (IS_ERR(*clk_module))
		fmt_err("cannot get module clock:%s", clkName);
	else
		fmt_debug(0, "get module clock:%s", clkName);
}

/* Common Clock Framework */
void fmt_init_pm(struct mtk_vdec_fmt *fmt)
{
	fmt_debug(0, "+");
	fmt_get_module_clock_by_name(fmt, "MT_CG_VDEC",
		&fmt->clk_VDEC);
}

int32_t fmt_clock_on(struct mtk_vdec_fmt *fmt)
{
	int ret = 0;

#ifdef CONFIG_MTK_PSEUDO_M4U
	int i, larb_id;
	struct M4U_PORT_STRUCT port;
#endif

	smi_bus_prepare_enable(SMI_LARB4, "FMT");
	ret = clk_prepare_enable(fmt->clk_VDEC);
	if (ret)
		fmt_debug(0, "clk_prepare_enable VDEC_SOC failed %d", ret);
#ifdef CONFIG_MTK_PSEUDO_M4U
	larb_id = 4;

	//enable 34bits port configs
	for (i = 12; i < 14; i++) {
		port.ePortID = MTK_M4U_ID(larb_id, i);
		port.Direction = 0;
		port.Distance = 1;
		port.domain = 0;
		port.Security = 0;
		port.Virtuality = 1;
		m4u_config_port(&port);
		fmt_debug(1, "port.ePortID %d m4u_config_port", port.ePortID);
	}
#endif

	return ret;
}

int32_t fmt_clock_off(struct mtk_vdec_fmt *fmt)
{
	clk_disable_unprepare(fmt->clk_VDEC);
	smi_bus_disable_unprepare(SMI_LARB4, "FMT");
	return 0;
}

void fmt_prepare_dvfs_emi_bw(void)
{
	int ret, i;

	mtk_pm_qos_add_request(&fmt_qos_req_f, PM_QOS_VDEC_FREQ,
				PM_QOS_DEFAULT_VALUE);
	fmt_freq_step_size = 1;
	ret = mmdvfs_qos_get_freq_steps(PM_QOS_VDEC_FREQ, &fmt_freq_steps[0],
					&fmt_freq_step_size);
	if (ret < 0)
		fmt_err("Failed to get fmt freq steps (%d)", ret);
	for (i = 0; i < fmt_freq_step_size; i++)
		fmt_debug(1, "freq_steps[%d] %ld", i, fmt_freq_steps[i]);

	plist_head_init(&fmt_rlist[0]);
	mm_qos_add_request(&fmt_rlist[0],
		&fmt_rdma_request[0], M4U_PORT_L4_MINI_MDP_R0_EXT);
	mm_qos_add_request(&fmt_rlist[0],
		&fmt_wdma_request[0], M4U_PORT_L4_MINI_MDP_W0_EXT);
}

void fmt_unprepare_dvfs_emi_bw(void)
{
	int freq_idx = 0;

	freq_idx = (fmt_freq_step_size == 0) ? 0 : (fmt_freq_step_size - 1);
	mtk_pm_qos_update_request(&fmt_qos_req_f, fmt_freq_steps[freq_idx]);
	mtk_pm_qos_remove_request(&fmt_qos_req_f);

	mm_qos_remove_all_request(&fmt_rlist[0]);
}

void fmt_start_dvfs_emi_bw(struct fmt_pmqos pmqos_param)
{
	u64 request_freq;
	struct timeval curr_time;
	s32 duration;
	u32 bandwidth;

	fmt_debug(1, "tv_sec %d tv_usec %d pixel_size %d rdma_datasize %d wdma_datasize %d",
				pmqos_param.tv_sec,
				pmqos_param.tv_usec,
				pmqos_param.pixel_size,
				pmqos_param.rdma_datasize,
				pmqos_param.wdma_datasize);

	do_gettimeofday(&curr_time);
	fmt_debug(1, "curr time tv_sec %d tv_usec %d", curr_time.tv_sec, curr_time.tv_usec);

	FMT_TIMER_GET_DURATION_IN_US(curr_time, pmqos_param, duration);
	request_freq = pmqos_param.pixel_size / duration;

	if (request_freq > fmt_freq_steps[0])
		request_freq = fmt_freq_steps[0];

	fmt_debug(1, "request_freq %d", request_freq);

	mtk_pm_qos_update_request(&fmt_qos_req_f, request_freq);
	FMT_BANDWIDTH(pmqos_param.rdma_datasize, pmqos_param.pixel_size, request_freq, bandwidth);
	mm_qos_set_request(&fmt_rdma_request[0], bandwidth, 0, BW_COMP_NONE);
	fmt_debug(1, "rdma bandwidth %d", bandwidth);
	FMT_BANDWIDTH(pmqos_param.wdma_datasize, pmqos_param.pixel_size, request_freq, bandwidth);
	mm_qos_set_request(&fmt_wdma_request[0], bandwidth, 0, BW_COMP_NONE);
	fmt_debug(1, "wdma bandwidth %d", bandwidth);
	mm_qos_update_all_request(&fmt_rlist[0]);
}

void fmt_end_dvfs_emi_bw(void)
{
	mtk_pm_qos_update_request(&fmt_qos_req_f, 0);

	mm_qos_set_request(&fmt_rdma_request[0], 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&fmt_wdma_request[0], 0, 0, BW_COMP_NONE);
	mm_qos_update_all_request(&fmt_rlist[0]);
}

void fmt_dump_addr_reg(struct mtk_vdec_fmt *fmt, int port)
{
	int i;

	switch (port) {
	case M4U_PORT_L4_MINI_MDP_R0_EXT:
		for (i = 0; i < 313; i++)
			fmt_debug(0, "FMT RDMA0(0x%x) 0x%x",
				fmt->map_base[0].base + i*4,
				FMT_GET32(fmt->map_base[0].va + i*4));
		for (i = 960; i < 982; i++)
			fmt_debug(0, "FMT RDMA0(0x%x) 0x%x",
				fmt->map_base[0].base + i*4,
				FMT_GET32(fmt->map_base[0].va + i*4));
		break;
	case M4U_PORT_L4_MINI_MDP_W0_EXT:
		for (i = 0; i < 60; i++)
			fmt_debug(0, "FMT WROT0(0x%x) 0x%x",
				fmt->map_base[1].base + i*4,
				FMT_GET32(fmt->map_base[1].va + i*4));
		for (i = 960; i < 973; i++)
			fmt_debug(0, "FMT WROT0(0x%x) 0x%x",
				fmt->map_base[1].base + i*4,
				FMT_GET32(fmt->map_base[1].va + i*4));
		break;
	default:
		break;
	}
}

#ifdef CONFIG_MTK_IOMMU_V2
enum mtk_iommu_callback_ret_t fmt_translation_fault_callback(
	int port, unsigned long mva, void *data)
{
	struct mtk_vdec_fmt *fmt = (struct mtk_vdec_fmt *) data;

	fmt_debug(0, "TF callback, port:%d, mva:0x%x", port, mva);
	fmt_dump_addr_reg(fmt, port);

	return MTK_IOMMU_CALLBACK_HANDLED;
}
#endif

void fmt_translation_fault_callback_setting(struct mtk_vdec_fmt *fmt)
{
#ifdef CONFIG_MTK_IOMMU_V2
	mtk_iommu_register_fault_callback(M4U_PORT_L4_MINI_MDP_R0_EXT,
		(mtk_iommu_fault_callback_t)fmt_translation_fault_callback,
		fmt);
	mtk_iommu_register_fault_callback(M4U_PORT_L4_MINI_MDP_W0_EXT,
		(mtk_iommu_fault_callback_t)fmt_translation_fault_callback,
		fmt);
#endif
}
