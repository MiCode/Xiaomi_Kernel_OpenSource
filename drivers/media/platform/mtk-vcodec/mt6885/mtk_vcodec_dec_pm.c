/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <soc/mediatek/smi.h>
#include <linux/slab.h>
#include "smi_public.h"
#include "mtk_vcodec_dec_pm.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcu.h"

#ifdef CONFIG_MTK_PSEUDO_M4U
#include <mach/mt_iommu.h>
#include "mach/pseudo_m4u.h"
#include "smi_port.h"
#endif

#if DEC_DVFS
#include <linux/pm_qos.h>
#include <linux/soc/mediatek/mtk-pm-qos.h>
#include <mmdvfs_pmqos.h>
#include "vcodec_dvfs.h"
#define STD_VDEC_FREQ 249
static struct mtk_pm_qos_request vdec_qos_req_f;
static u64 vdec_freq;
static u32 vdec_freq_step_size;
static u64 vdec_freq_steps[MAX_FREQ_STEP];
static struct codec_history *vdec_hists;
/* static struct codec_job *vdec_jobs; */
/* TODO: apply new DVFS */
static u64 vdec_req_freq[2]; /* 0 - LAT, 1 - Core */
#endif

#define VDEC_DRV_UFO_AUO_ON (1 << 1)
#if DEC_EMI_BW
#include <mtk_smi.h>
#include <dt-bindings/memory/mt6885-larb-port.h>
static unsigned int h264_frm_scale[4] = {12, 24, 40, 12};
static unsigned int h265_frm_scale[4] = {12, 24, 40, 12};
static unsigned int vp9_frm_scale[4] = {12, 24, 40, 12};
static unsigned int vp8_frm_scale[4] = {12, 24, 40, 12};
static unsigned int mp24_frm_scale[5] = {16, 20, 32, 50, 16};

static struct plist_head vdec_rlist_core;
static struct plist_head vdec_rlist_lat;
/* LARB4, mostly core */
static struct mm_qos_request vdec_mc;
static struct mm_qos_request vdec_ufo;
static struct mm_qos_request vdec_pp;
static struct mm_qos_request vdec_pred_rd;
static struct mm_qos_request vdec_pred_wr;
static struct mm_qos_request vdec_ppwrap;
static struct mm_qos_request vdec_tile;
static struct mm_qos_request vdec_vld;
static struct mm_qos_request vdec_vld2;
static struct mm_qos_request vdec_avc_mv;
static struct mm_qos_request vdec_rg_ctrl_dma;
/* LARB5, mostly lat */
static struct mm_qos_request vdec_lat0_vld;
static struct mm_qos_request vdec_lat0_vld2;
static struct mm_qos_request vdec_lat0_avc_mv;
static struct mm_qos_request vdec_lat0_pred_rd;
static struct mm_qos_request vdec_lat0_tile;
static struct mm_qos_request vdec_lat0_wdma;
static struct mm_qos_request vdec_lat0_rg_ctrl_dma;
static struct mm_qos_request vdec_ufo_enc;
#endif
static struct ion_client *ion_vdec_client;

void mtk_dec_init_ctx_pm(struct mtk_vcodec_ctx *ctx)
{
	ctx->input_driven = 0;
}

int mtk_vcodec_init_dec_pm(struct mtk_vcodec_dev *mtkdev)
{
	int ret = 0;
#ifndef FPGA_PWRCLK_API_DISABLE
	struct device_node *node;
	struct platform_device *pdev;
	struct mtk_vcodec_pm *pm;

	pdev = mtkdev->plat_dev;
	pm = &mtkdev->pm;
	pm->mtkdev = mtkdev;
	pm->chip_node = of_find_compatible_node(NULL,
		NULL, "mediatek,mt6885-vcodec-dec");
	node = of_parse_phandle(pdev->dev.of_node, "mediatek,larb", 0);
	if (!node) {
		mtk_v4l2_err("of_parse_phandle mediatek,larb fail!");
		return -1;
	}

	pdev = of_find_device_by_node(node);
	if (WARN_ON(!pdev)) {
		of_node_put(node);
		return -1;
	}
	pm->larbvdec = &pdev->dev;
	pdev = mtkdev->plat_dev;
	pm->dev = &pdev->dev;

	if (pm->chip_node) {
		pm->clk_MT_CG_SOC = devm_clk_get(&pdev->dev, "MT_CG_SOC");
		if (IS_ERR(pm->clk_MT_CG_SOC)) {
			mtk_v4l2_err("[VCODEC][ERROR] Unable to devm_clk_get MT_CG_SOC\n");
			return PTR_ERR(pm->clk_MT_CG_SOC);
		}
		pm->clk_MT_CG_VDEC0 = devm_clk_get(&pdev->dev, "MT_CG_VDEC0");
		if (IS_ERR(pm->clk_MT_CG_VDEC0)) {
			mtk_v4l2_err("[VCODEC][ERROR] Unable to devm_clk_get MT_CG_VDEC0\n");
			return PTR_ERR(pm->clk_MT_CG_VDEC0);
		}
		pm->clk_MT_CG_VDEC1 = devm_clk_get(&pdev->dev, "MT_CG_VDEC1");
		if (IS_ERR(pm->clk_MT_CG_VDEC1)) {
			mtk_v4l2_err("[VCODEC][ERROR] Unable to devm_clk_get MT_CG_VDEC1\n");
			return PTR_ERR(pm->clk_MT_CG_VDEC1);
		}

	} else
		mtk_v4l2_err("[VCODEC][ERROR] DTS went wrong...");

	atomic_set(&pm->dec_active_cnt, 0);
	memset(pm->vdec_racing_info, 0, sizeof(pm->vdec_racing_info));
	mutex_init(&pm->dec_racing_info_mutex);
#endif
	ion_vdec_client = NULL;

	return ret;
}

void mtk_vcodec_release_dec_pm(struct mtk_vcodec_dev *dev)
{
#if DEC_DVFS
	mutex_lock(&dev->dec_dvfs_mutex);
	free_hist(&vdec_hists, 0);
	mutex_unlock(&dev->dec_dvfs_mutex);
#endif
}

void mtk_vcodec_dec_pw_on(struct mtk_vcodec_pm *pm, int hw_id)
{
}

void mtk_vcodec_dec_pw_off(struct mtk_vcodec_pm *pm, int hw_id)
{
}

void mtk_vcodec_dec_clock_on(struct mtk_vcodec_pm *pm, int hw_id)
{
#ifdef CONFIG_MTK_PSEUDO_M4U
	int i, larb_port_num, larb_id;
	struct M4U_PORT_STRUCT port;
#endif

#ifndef FPGA_PWRCLK_API_DISABLE
	int j, ret;
	struct mtk_vcodec_dev *dev;
	void __iomem *vdec_racing_addr;
	unsigned long flags;

	time_check_start(MTK_FMT_DEC, hw_id);
	if (hw_id == MTK_VDEC_CORE) {
		smi_bus_prepare_enable(SMI_LARB4, "VDEC_CORE");
		ret = clk_prepare_enable(pm->clk_MT_CG_SOC);
		if (ret)
			mtk_v4l2_err("clk_prepare_enable VDEC_SOC fail %d",
				ret);
		ret = clk_prepare_enable(pm->clk_MT_CG_VDEC0);
		if (ret)
			mtk_v4l2_err("clk_prepare_enable VDEC_CORE fail %d",
				ret);
	} else if (hw_id == MTK_VDEC_LAT) {
		smi_bus_prepare_enable(SMI_LARB5, "VDEC_LAT");
		ret = clk_prepare_enable(pm->clk_MT_CG_VDEC1);
		if (ret)
			mtk_v4l2_err("clk_prepare_enable VDEC_LAT fail %d",
				ret);
	} else {
		mtk_v4l2_err("invalid hw_id %d", hw_id);
		time_check_end(MTK_FMT_DEC, hw_id, 50);
		return;
	}

	dev = container_of(pm, struct mtk_vcodec_dev, pm);

	if (!ret) {
		spin_lock_irqsave(&dev->dec_power_lock[hw_id], flags);
		dev->dec_is_power_on[hw_id] = true;
		spin_unlock_irqrestore(&dev->dec_power_lock[hw_id], flags);
	}

	mutex_lock(&pm->dec_racing_info_mutex);
	if (atomic_inc_return(&pm->dec_active_cnt) == 1) {
		/* restore racing info read/write ptr */
		vdec_racing_addr =
			dev->dec_reg_base[VDEC_RACING_CTRL] +
				MTK_VDEC_RACING_INFO_OFFSET;
		for (j = 0; j < MTK_VDEC_RACING_INFO_SIZE; j++)
			writel(pm->vdec_racing_info[j],
				vdec_racing_addr + j * 4);
	}
	mutex_unlock(&pm->dec_racing_info_mutex);
	time_check_end(MTK_FMT_DEC, hw_id, 50);
#endif

#ifdef CONFIG_MTK_PSEUDO_M4U
	time_check_start(MTK_FMT_DEC, hw_id);
	if (hw_id == MTK_VDEC_CORE) {
		larb_port_num = SMI_LARB4_PORT_NUM;
		larb_id = 4;

		//enable UFO port
		port.ePortID = M4U_PORT_L5_VDEC_UFO_ENC_EXT_DISP;
		port.Direction = 0;
		port.Distance = 1;
		port.domain = 0;
		port.Security = 0;
		port.Virtuality = 1;
		m4u_config_port(&port);
	} else if (hw_id == MTK_VDEC_LAT) {
		larb_port_num = SMI_LARB5_PORT_NUM;
		larb_id = 5;
	}

	//enable 34bits port configs & sram settings
	for (i = 0; i < larb_port_num; i++) {
		port.ePortID = MTK_M4U_ID(larb_id, i);
		port.Direction = 0;
		port.Distance = 1;
		port.domain = 0;
		port.Security = 0;
		port.Virtuality = 1;
		m4u_config_port(&port);
	}
	time_check_end(MTK_FMT_DEC, hw_id, 50);
#endif
}

void mtk_vcodec_dec_clock_off(struct mtk_vcodec_pm *pm, int hw_id)
{
#ifndef FPGA_PWRCLK_API_DISABLE
	struct mtk_vcodec_dev *dev;
	void __iomem *vdec_racing_addr;
	unsigned long flags;
	int i;

	mutex_lock(&pm->dec_racing_info_mutex);
	if (atomic_dec_and_test(&pm->dec_active_cnt)) {
		/* backup racing info read/write ptr */
		dev = container_of(pm, struct mtk_vcodec_dev, pm);
		vdec_racing_addr =
			dev->dec_reg_base[VDEC_RACING_CTRL] +
				MTK_VDEC_RACING_INFO_OFFSET;
		for (i = 0; i < MTK_VDEC_RACING_INFO_SIZE; i++)
			pm->vdec_racing_info[i] =
				readl(vdec_racing_addr + i * 4);
	}
	mutex_unlock(&pm->dec_racing_info_mutex);

	dev = container_of(pm, struct mtk_vcodec_dev, pm);
	mtk_vdec_hw_break(dev, hw_id);

	/* avoid translation fault callback dump reg not done */
	spin_lock_irqsave(&dev->dec_power_lock[hw_id], flags);
	dev->dec_is_power_on[hw_id] = false;
	spin_unlock_irqrestore(&dev->dec_power_lock[hw_id], flags);

	if (hw_id == MTK_VDEC_CORE) {
		clk_disable_unprepare(pm->clk_MT_CG_VDEC0);
		clk_disable_unprepare(pm->clk_MT_CG_SOC);
		smi_bus_disable_unprepare(SMI_LARB4, "VDEC_CORE");
	} else if (hw_id == MTK_VDEC_LAT) {
		clk_disable_unprepare(pm->clk_MT_CG_VDEC1);
		smi_bus_disable_unprepare(SMI_LARB5, "VDEC_LAT");
	} else
		mtk_v4l2_err("invalid hw_id %d", hw_id);

#endif
}

void mtk_vdec_hw_break(struct mtk_vcodec_dev *dev, int hw_id)
{
	u32 cg_status = 0, ufo_cg_status = 0;
	void __iomem *vdec_misc_addr = dev->dec_reg_base[VDEC_MISC];
	void __iomem *vdec_vld_addr = dev->dec_reg_base[VDEC_VLD];
	void __iomem *vdec_gcon_addr = dev->dec_reg_base[VDEC_SYS];
	void __iomem *vdec_ufo_addr = dev->dec_reg_base[VDEC_BASE] + 0x800;
	void __iomem *vdec_lat_misc_addr = dev->dec_reg_base[VDEC_LAT_MISC];
	void __iomem *vdec_lat_vld_addr = dev->dec_reg_base[VDEC_LAT_VLD];
	void __iomem *vdec_soc_gcon_addr = dev->dec_reg_base[VDEC_SOC_GCON];
	struct mtk_vcodec_ctx *ctx = NULL;
	int misc_offset[4] = {64, 66, 67, 65};

	struct timeval tv_start;
	struct timeval tv_end;
	s32 usec, timeout = 20000;
	int offset, idx;
	unsigned long value;
	u32 fourcc;
	u32 is_ufo = 0;

	if (hw_id == MTK_VDEC_CORE) {
		ctx = dev->curr_dec_ctx[hw_id];
		if (ctx)
			fourcc = ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;
		else
			fourcc = 0;
		if (readl(vdec_gcon_addr) == 0) {
			mtk_v4l2_debug(0, "VDEC CORE not HW break since clk off. codec:0x%08x(%c%c%c%c)",
			    fourcc, fourcc & 0xFF, (fourcc >> 8) & 0xFF,
			    (fourcc >> 16) & 0xFF, (fourcc >> 24) & 0xFF);
			return;
		}

		if (fourcc != V4L2_PIX_FMT_AV1)
			is_ufo = readl(vdec_ufo_addr + 0x08C) & 0x1;

		/* hw break */
		writel((readl(vdec_misc_addr + 0x0100) | 0x1),
			vdec_misc_addr + 0x0100);
		if (is_ufo)
			writel((readl(vdec_ufo_addr + 0x01C) & 0xFFFFFFFD),
				vdec_ufo_addr + 0x01C);

		do_gettimeofday(&tv_start);
		cg_status = readl(vdec_misc_addr + 0x0104);
		if (is_ufo)
			ufo_cg_status = readl(vdec_ufo_addr + 0x08C);
		while (((cg_status & 0x11) != 0x11) ||
		      (is_ufo && ((ufo_cg_status & 0x11000) != 0x11000))) {
			do_gettimeofday(&tv_end);
			usec = (tv_end.tv_sec - tv_start.tv_sec) * 1000000 +
				   (tv_end.tv_usec - tv_start.tv_usec);
			if (usec > timeout) {
				mtk_v4l2_err("VDEC HW break timeout. codec:0x%08x(%c%c%c%c) ufo %d",
				    fourcc, fourcc & 0xFF, (fourcc >> 8) & 0xFF,
				    (fourcc >> 16) & 0xFF, (fourcc >> 24) & 0xFF, is_ufo);
				value = readl(vdec_gcon_addr + (0 << 2));
				mtk_v4l2_err("[DEBUG][GCON] 0x%x(%d) = 0x%lx",
					0 << 2, 0, value);
				value = readl(vdec_gcon_addr + (6 << 2));
				mtk_v4l2_err("[DEBUG][GCON] 0x%x(%d) = 0x%lx",
					6 << 2, 6, value);
				for (offset = 64; offset <= 79; offset++) {
					value = readl(
					    vdec_misc_addr + (offset << 2));
					mtk_v4l2_err("[DEBUG][MISC] 0x%x(%d) = 0x%lx",
						offset << 2, offset, value);
				}
				for (idx = 0; idx < 4; idx++) {
					offset = misc_offset[idx];
					value = readl(
					    vdec_misc_addr + (offset << 2));
					mtk_v4l2_err("[DEBUG][MISC] 0x%x(%d) = 0x%lx",
						offset << 2, offset, value);
				}
				if (is_ufo)
					mtk_v4l2_err("[DEBUG][UFO] 0x%x(%d) = 0x%lx",
						0x08C, 0x08C >> 2, ufo_cg_status);

				if (timeout == 20000)
					timeout = 1000000;
				else if (timeout == 1000000) {
					/* v4l2_aee_print(
					 *    "%s %p codec:0x%08x(%c%c%c%c) hw break timeout\n",
					 *    __func__, ctx, fourcc,
					 *    fourcc & 0xFF, (fourcc >> 8) & 0xFF,
					 *    (fourcc >> 16) & 0xFF, (fourcc >> 24) & 0xFF);
					 */
					break;
				}
				do_gettimeofday(&tv_start);
				//smi_debug_bus_hang_detect(0, "VCODEC");
			}
			cg_status = readl(vdec_misc_addr + 0x0104);
			if (is_ufo)
				ufo_cg_status = readl(vdec_ufo_addr + 0x08C);
		}

		/* sw reset */
		if (is_ufo)
			writel((readl(vdec_ufo_addr + 0x01C) | 0x2),
				vdec_ufo_addr + 0x01C);
		writel(0x1, vdec_vld_addr + 0x0108);
		writel(0x0, vdec_vld_addr + 0x0108);
	} else if (hw_id == MTK_VDEC_LAT) {
		ctx = dev->curr_dec_ctx[hw_id];
		if (ctx)
			fourcc = ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;
		else
			fourcc = 0;
		if (readl(vdec_soc_gcon_addr+0x200) == 0) {
			mtk_v4l2_debug(0, "VDEC LAT not HW break since clk off. codec:0x%08x(%c%c%c%c)",
			    fourcc, fourcc & 0xFF, (fourcc >> 8) & 0xFF,
			    (fourcc >> 16) & 0xFF, (fourcc >> 24) & 0xFF);
			return;
		}
		/* hw break */
		writel((readl(vdec_lat_misc_addr + 0x0100) | 0x1),
			vdec_lat_misc_addr + 0x0100);

		do_gettimeofday(&tv_start);
		cg_status = readl(vdec_lat_misc_addr + 0x0104);
		while (!((cg_status & 0x1) && (cg_status & 0x10))) {
			do_gettimeofday(&tv_end);
			usec = (tv_end.tv_sec - tv_start.tv_sec) * 1000000 +
				   (tv_end.tv_usec - tv_start.tv_usec);
			if (usec > timeout) {
				mtk_v4l2_err("VDEC HW %d break timeout. codec:0x%08x(%c%c%c%c)",
				    hw_id, fourcc, fourcc & 0xFF, (fourcc >> 8) & 0xFF,
				    (fourcc >> 16) & 0xFF, (fourcc >> 24) & 0xFF);
				for (offset = 64; offset <= 79; offset++) {
					value = readl(
					    vdec_lat_misc_addr + (offset << 2));
					mtk_v4l2_err("[DEBUG][LAT_MISC] 0x%x(%d) = 0x%lx",
						offset << 2, offset, value);
				}
				for (idx = 0; idx < 4; idx++) {
					offset = misc_offset[idx];
					value = readl(
					    vdec_lat_misc_addr + (offset << 2));
					mtk_v4l2_err("[DEBUG][LAT_MISC] 0x%x(%d) = 0x%lx",
						offset << 2, offset, value);
				}

				if (timeout == 20000)
					timeout = 1000000;
				else if (timeout == 1000000) {
					/* v4l2_aee_print(
					 *    "%s %p codec:0x%08x(%c%c%c%c) hw %d break timeout\n",
					 *    __func__, ctx, fourcc,
					 *    fourcc & 0xFF, (fourcc >> 8) & 0xFF,
					 *    (fourcc >> 16) & 0xFF, (fourcc >> 24) & 0xFF, hw_id);
					 */
					break;
				}
				do_gettimeofday(&tv_start);
				//smi_debug_bus_hang_detect(0, "VCODEC");
			}
			cg_status = readl(vdec_lat_misc_addr + 0x0104);
		}

		/* sw reset */
		writel(0x1, vdec_lat_vld_addr + 0x0108);
		writel(0x0, vdec_lat_vld_addr + 0x0108);
	} else {
		mtk_v4l2_err("hw_id (%d) is unknown\n", hw_id);
	}
}

void mtk_vdec_dump_addr_reg(
	struct mtk_vcodec_dev *dev, int hw_id, enum mtk_dec_dump_addr_type type)
{
	struct mtk_vcodec_ctx *ctx;
	u32 fourcc;
	void __iomem *vld_addr = dev->dec_reg_base[VDEC_VLD];
	void __iomem *mc_addr = dev->dec_reg_base[VDEC_MC];
	void __iomem *mv_addr = dev->dec_reg_base[VDEC_MV];
	void __iomem *ufo_addr = dev->dec_reg_base[VDEC_BASE] + 0x800;
	void __iomem *lat_vld_addr = dev->dec_reg_base[VDEC_LAT_VLD];
	void __iomem *lat_wdma_addr = dev->dec_reg_base[VDEC_LAT_MISC] + 0x800;
	void __iomem *rctrl_addr = dev->dec_reg_base[VDEC_RACING_CTRL];
	unsigned long value, values[6];
	bool is_ufo = false;
	int i, j, start, end;
	unsigned long flags;

	#define INPUT_LAT_VLD_NUM 7
	const unsigned int input_lat_vld_reg[INPUT_LAT_VLD_NUM] = {
		0xB0, 0xB4, 0xB8, 0x110, 0xEC, 0xF8, 0xFC};
	// RPTR, VSTART, VEND, WPTR, VBAR, VWPTR, VRPTR
	#define OUTPUT_MC_NUM 2
	const unsigned int output_mc_reg[OUTPUT_MC_NUM] = {
		0x224, 0x228}; // PY_ADD, PC_ADD
	#define OUTPUT_UFO_MC_NUM 5
	const unsigned int output_ufo_mc_reg[OUTPUT_UFO_MC_NUM] = {
		0xB5C, 0xAE8, 0xAEC, 0xCE4, 0xCE8};
	// YC_SEP, LEN_Y, LEN_C, LEN_Y_OFFSET, LEN_C_OFFSET
	#define OUTPUT_UFO_NUM 4
	const unsigned int output_ufo_reg[OUTPUT_UFO_NUM] = {
		0x7C, 0x80, 0x84, 0x88}; // LEN_Y, LEN_C, BS_Y, BS_C
	#define REF_MC_NUM 7
	const unsigned int ref_mc_base[REF_MC_NUM] = {
		0x3DC, 0xB60, 0x45C, 0xBE0, 0x4DC, 0xC60, 0xD28};
	// P_L0_Y, P_L0_C, B_L0_Y, B_L0_C, B_L1_Y, B_L1_C, REF
	#define UBE_CORE_VLD_NUM 3
	const unsigned int ube_core_vld_reg[UBE_CORE_VLD_NUM] = {
		0xB0, 0xB4, 0xB8};

	if (hw_id != MTK_VDEC_CORE && hw_id != MTK_VDEC_LAT) {
		mtk_v4l2_err("hw_id %d not support !!", hw_id);
		return;
	}
	ctx = dev->curr_dec_ctx[hw_id];
	if (ctx)
		fourcc = ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;
	else
		fourcc = 0;

	spin_lock_irqsave(&dev->dec_power_lock[hw_id], flags);
	if (dev->dec_is_power_on[hw_id] == false) {
		mtk_v4l2_err("hw %d power is off !!", hw_id);
		spin_unlock_irqrestore(&dev->dec_power_lock[hw_id], flags);
		return;
	}

	if (hw_id == MTK_VDEC_CORE && fourcc != V4L2_PIX_FMT_AV1)
		is_ufo = (readl(ufo_addr + 0x08C) & 0x1) == 0x1;

	switch (type) {
	case DUMP_VDEC_IN_BUF:
		for (i = 0; i < INPUT_LAT_VLD_NUM; i++) {
			value = readl(lat_vld_addr + input_lat_vld_reg[i]);
			mtk_v4l2_err("[LAT][VLD] 0x%x(%d) = 0x%lx",
			  input_lat_vld_reg[i], input_lat_vld_reg[i]/4, value);
		}
		break;
	case DUMP_VDEC_OUT_BUF:
		for (i = 0; i < OUTPUT_MC_NUM; i++) {
			value = readl(mc_addr + output_mc_reg[i]);
			mtk_v4l2_err("[CORE][MC] 0x%x(%d) = 0x%lx",
				output_mc_reg[i], output_mc_reg[i]/4, value);
		}
		if (is_ufo) {
			for (i = 0; i < OUTPUT_UFO_MC_NUM; i++) {
				value = readl(mc_addr + output_ufo_mc_reg[i]);
				mtk_v4l2_err("[CORE][MC] 0x%x(%d) = 0x%lx",
				    output_ufo_mc_reg[i],
				    output_ufo_mc_reg[i]/4, value);
			}
			for (i = 0; i < OUTPUT_UFO_NUM; i++) {
				value = readl(ufo_addr + output_ufo_reg[i]);
				mtk_v4l2_err("[CORE][UFO] 0x%x(%d) = 0x%lx",
				    output_ufo_reg[i],
				    output_ufo_reg[i]/4, value);
			}
		}
		break;
	case DUMP_VDEC_REF_BUF:
		for (i = 0; i < 32; i++) {
			for (j = 0; j < 6; j++) {
				values[j] = readl(mc_addr +
					(ref_mc_base[j] + i * 4));
			}
			mtk_v4l2_err("[CORE][MC] 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx",
				ref_mc_base[0] + i * 4,
				ref_mc_base[0]/4 + i, values[0],
				ref_mc_base[1] + i * 4,
				ref_mc_base[1]/4 + i, values[1],
				ref_mc_base[2] + i * 4,
				ref_mc_base[2]/4 + i, values[2],
				ref_mc_base[3] + i * 4,
				ref_mc_base[3]/4 + i, values[3],
				ref_mc_base[4] + i * 4,
				ref_mc_base[4]/4 + i, values[4],
				ref_mc_base[5] + i * 4,
				ref_mc_base[5]/4 + i, values[5]);
		}
		for (i = 0; i < 4; i++)
			values[i] = readl(mc_addr + i * 4);
		mtk_v4l2_err("[CORE][MC] 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx",
			i * 4, i, values[0],
			i * 4, i, values[1],
			i * 4, i, values[2],
			i * 4, i, values[3]);
		for (i = 0; i < 6; i++) {
			values[i] = readl(mc_addr +
				(ref_mc_base[6] + i * 4));
		}
		mtk_v4l2_err("[CORE][MC] 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx, 0x%x(%d) = 0x%lx",
			ref_mc_base[6],
			ref_mc_base[6]/4 + 0, values[0],
			ref_mc_base[6] + 1 * 4,
			ref_mc_base[6]/4 + 1, values[1],
			ref_mc_base[6] + 2 * 4,
			ref_mc_base[6]/4 + 2, values[2],
			ref_mc_base[6] + 3 * 4,
			ref_mc_base[6]/4 + 3, values[3],
			ref_mc_base[6] + 4 * 4,
			ref_mc_base[6]/4 + 4, values[4],
			ref_mc_base[6] + 5 * 4,
			ref_mc_base[6]/4 + 5, values[5]);
		break;
	case DUMP_VDEC_MV_BUF:
		if (hw_id != MTK_VDEC_CORE) {
			mtk_v4l2_err("not support dump MV at hw_id %d",
				hw_id);
			break;
		}
		value = readl(mv_addr + 0x20C);
		mtk_v4l2_err("[CORE][MV] 0x%x(%d) = 0x%lx",
			0x20C, 0x20C/4, value);
		switch (fourcc) {
		case V4L2_PIX_FMT_H265:
			start = 0;
			end = 32;
			break;
		case V4L2_PIX_FMT_H264:
			start = 96;
			end = 128;
			break;
		case V4L2_PIX_FMT_VP9:
			start = 240;
			end = 241;
			break;
		case V4L2_PIX_FMT_AV1:
			start = 353;
			end = 356;
			break;
		default:
			start = 195;
			end = 198;
		}
		for (i = start; i < end; i++) {
			value = readl(mv_addr + i * 4);
			mtk_v4l2_err("[CORE][MV] 0x%x(%d) = 0x%lx",
				i * 4, i, value);
		}
		break;
	case DUMP_VDEC_UBE_BUF:
		if (hw_id == MTK_VDEC_LAT) {
			value = readl(lat_wdma_addr + 0x50);
			mtk_v4l2_err("[LAT][WDMA] 0x%x(%d) = 0x%lx",
				0x50, 0x50/4, value);
			value = readl(lat_wdma_addr + 0x44);
			mtk_v4l2_err("[LAT][WDMA] 0x%x(%d) = 0x%lx",
				0x44, 0x44/4, value);
			value = readl(rctrl_addr + 0x78);
			mtk_v4l2_err("[RACING_CTRL] 0x%x(%d) = 0x%lx",
				0x78, 0x78/4, value);
		} else {
			value = readl(rctrl_addr + 0x7C);
			mtk_v4l2_err("[RACING_CTRL] 0x%x(%d) = 0x%lx",
				0x7C, 0x7C/4, value);
			for (i = 0; i < UBE_CORE_VLD_NUM; i++) {
				value = readl(vld_addr + ube_core_vld_reg[i]);
				mtk_v4l2_err("[CORE][VLD] 0x%x(%d) = 0x%lx",
				    ube_core_vld_reg[i],
				    ube_core_vld_reg[i]/4, value);
			}
		}
		break;
	default:
		mtk_v4l2_err("unknown addr type");
	}

	spin_unlock_irqrestore(&dev->dec_power_lock[hw_id], flags);
}

#ifdef CONFIG_MTK_IOMMU_V2
enum mtk_iommu_callback_ret_t mtk_vdec_translation_fault_callback(
	int port, unsigned long mva, void *data)
{
	struct mtk_vcodec_dev *dev = (struct mtk_vcodec_dev *)data;
	int hw_id;
	struct mtk_vcodec_ctx *ctx;
	u32 fourcc;

	if (port == M4U_PORT_L5_VDEC_UFO_ENC_EXT_DISP)
		hw_id = MTK_VDEC_CORE;
	else if ((port >> 5) == 5) // larb5 LAT
		hw_id = MTK_VDEC_LAT;
	else
		hw_id = MTK_VDEC_CORE;

	ctx = dev->curr_dec_ctx[hw_id];
	if (ctx) {
		fourcc = ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;
		mtk_v4l2_err("codec:0x%08x(%c%c%c%c) %s TF larb %d port %x mva 0x%lx",
			fourcc, fourcc & 0xFF, (fourcc >> 8) & 0xFF,
			(fourcc >> 16) & 0xFF, (fourcc >> 24) & 0xFF,
			(hw_id == MTK_VDEC_LAT) ? "LAT" : "CORE",
			port >> 5, port, mva);
	} else {
		mtk_v4l2_err("ctx NULL codec unknown, %s TF larb %d port %x mva 0x%lx",
			(hw_id == MTK_VDEC_LAT) ? "LAT" : "CORE",
			port >> 5, port, mva);
	}

	switch (port) {
	case M4U_PORT_L5_VDEC_LAT0_VLD_EXT_DISP:
	case M4U_PORT_L5_VDEC_LAT0_VLD2_EXT_DISP:
		mtk_vdec_dump_addr_reg(dev, hw_id, DUMP_VDEC_IN_BUF);
		break;
	case M4U_PORT_L4_VDEC_PP_EXT_MDP:
	case M4U_PORT_L4_VDEC_UFO_EXT_MDP:
	case M4U_PORT_L5_VDEC_UFO_ENC_EXT_DISP:
		mtk_vdec_dump_addr_reg(dev, MTK_VDEC_CORE, DUMP_VDEC_OUT_BUF);
		break;
	case M4U_PORT_L4_VDEC_MC_EXT_MDP:
		mtk_vdec_dump_addr_reg(dev, hw_id, DUMP_VDEC_REF_BUF);
		break;
	case M4U_PORT_L4_VDEC_AVC_MV_EXT_MDP:
		mtk_vdec_dump_addr_reg(dev, hw_id, DUMP_VDEC_MV_BUF);
		break;
	case M4U_PORT_L4_VDEC_VLD_EXT_MDP:
	case M4U_PORT_L4_VDEC_VLD2_EXT_MDP:
	case M4U_PORT_L4_VDEC_RG_CTRL_DMA_EXT_MDP:
	case M4U_PORT_L5_VDEC_LAT0_RG_CTRL_DMA_EXT_DISP:
	case M4U_PORT_L5_VDEC_LAT0_WDMA_EXT_DISP:
		mtk_vdec_dump_addr_reg(dev, hw_id, DUMP_VDEC_UBE_BUF);
		break;
	default:
		if (hw_id == MTK_VDEC_LAT)
			mtk_vdec_dump_addr_reg(dev, hw_id, DUMP_VDEC_IN_BUF);
		else
			mtk_vdec_dump_addr_reg(dev, hw_id, DUMP_VDEC_OUT_BUF);
	}

	return MTK_IOMMU_CALLBACK_HANDLED;
}

void mtk_vdec_translation_fault_callback_setting(
	struct mtk_vcodec_dev *dev)
{
	int i;

	for (i = M4U_PORT_L4_VDEC_MC_EXT_MDP; i <= M4U_PORT_L4_VDEC_RG_CTRL_DMA_EXT_MDP; i++) {
		mtk_iommu_register_fault_callback(i,
			mtk_vdec_translation_fault_callback, (void *)dev);
	}
	for (i = M4U_PORT_L5_VDEC_LAT0_VLD_EXT_DISP; i <= M4U_PORT_L5_VDEC_UFO_ENC_EXT_DISP; i++) {
		mtk_iommu_register_fault_callback(i,
			mtk_vdec_translation_fault_callback, (void *)dev);
	}
}
#endif

void mtk_prepare_vdec_dvfs(void)
{
#if DEC_DVFS
	int ret;

	mtk_pm_qos_add_request(&vdec_qos_req_f, PM_QOS_VDEC_FREQ,
				PM_QOS_DEFAULT_VALUE);
	vdec_freq_step_size = 1;
	ret = mmdvfs_qos_get_freq_steps(PM_QOS_VDEC_FREQ, &vdec_freq_steps[0],
					&vdec_freq_step_size);
	if (ret < 0)
		pr_debug("Failed to get vdec freq steps (%d)\n", ret);
#endif
}

void mtk_unprepare_vdec_dvfs(void)
{
#if DEC_DVFS
	int freq_idx = 0;

	freq_idx = (vdec_freq_step_size == 0) ? 0 : (vdec_freq_step_size - 1);
	mtk_pm_qos_update_request(&vdec_qos_req_f, vdec_freq_steps[freq_idx]);
	mtk_pm_qos_remove_request(&vdec_qos_req_f);
	free_hist(&vdec_hists, 0);
	/* TODO: jobs error handle */
#endif
}

void mtk_prepare_vdec_emi_bw(void)
{
#if DEC_EMI_BW
	plist_head_init(&vdec_rlist_core);
	mm_qos_add_request(&vdec_rlist_core, &vdec_mc,
				M4U_PORT_L4_VDEC_MC_EXT_MDP);
	mm_qos_add_request(&vdec_rlist_core, &vdec_ufo,
				M4U_PORT_L4_VDEC_UFO_EXT_MDP);
	mm_qos_add_request(&vdec_rlist_core, &vdec_pp,
				M4U_PORT_L4_VDEC_PP_EXT_MDP);
	mm_qos_add_request(&vdec_rlist_core, &vdec_pred_rd,
				M4U_PORT_L4_VDEC_PRED_RD_EXT_MDP);
	mm_qos_add_request(&vdec_rlist_core, &vdec_pred_wr,
				M4U_PORT_L4_VDEC_PRED_WR_EXT_MDP);
	mm_qos_add_request(&vdec_rlist_core, &vdec_ppwrap,
				M4U_PORT_L4_VDEC_PPWRAP_EXT_MDP);
	mm_qos_add_request(&vdec_rlist_core, &vdec_tile,
				M4U_PORT_L4_VDEC_TILE_EXT_MDP);
	mm_qos_add_request(&vdec_rlist_core, &vdec_vld,
				M4U_PORT_L4_VDEC_VLD_EXT_MDP);
	mm_qos_add_request(&vdec_rlist_core, &vdec_vld2,
				M4U_PORT_L4_VDEC_VLD2_EXT_MDP);
	mm_qos_add_request(&vdec_rlist_core, &vdec_avc_mv,
				M4U_PORT_L4_VDEC_AVC_MV_EXT_MDP);
	mm_qos_add_request(&vdec_rlist_core, &vdec_rg_ctrl_dma,
				M4U_PORT_L4_VDEC_RG_CTRL_DMA_EXT_MDP);
	mm_qos_add_request(&vdec_rlist_core, &vdec_ufo_enc,
				M4U_PORT_L5_VDEC_UFO_ENC_EXT_DISP);

	plist_head_init(&vdec_rlist_lat);
	mm_qos_add_request(&vdec_rlist_lat, &vdec_lat0_vld,
				M4U_PORT_L5_VDEC_LAT0_VLD_EXT_DISP);
	mm_qos_add_request(&vdec_rlist_lat, &vdec_lat0_vld2,
				M4U_PORT_L5_VDEC_LAT0_VLD2_EXT_DISP);
	mm_qos_add_request(&vdec_rlist_lat, &vdec_lat0_avc_mv,
				M4U_PORT_L5_VDEC_LAT0_AVC_MV_EXT_DISP);
	mm_qos_add_request(&vdec_rlist_lat, &vdec_lat0_pred_rd,
				M4U_PORT_L5_VDEC_LAT0_PRED_RD_EXT_DISP);
	mm_qos_add_request(&vdec_rlist_lat, &vdec_lat0_tile,
				M4U_PORT_L5_VDEC_LAT0_TILE_EXT_DISP);
	mm_qos_add_request(&vdec_rlist_lat, &vdec_lat0_wdma,
				M4U_PORT_L5_VDEC_LAT0_WDMA_EXT_DISP);
	mm_qos_add_request(&vdec_rlist_lat, &vdec_lat0_rg_ctrl_dma,
				M4U_PORT_L5_VDEC_LAT0_RG_CTRL_DMA_EXT_DISP);
#endif
}

void mtk_unprepare_vdec_emi_bw(void)
{
#if DEC_EMI_BW
	mm_qos_remove_all_request(&vdec_rlist_core);
	mm_qos_remove_all_request(&vdec_rlist_lat);
#endif
}

void mtk_vdec_dvfs_begin(struct mtk_vcodec_ctx *ctx, int hw_id)
{
#if 0
	int target_freq = 0;
	u64 target_freq_64 = 0;
	struct codec_job *vdec_cur_job = 0;

	mutex_lock(&ctx->dev->dec_dvfs_mutex);
	vdec_cur_job = move_job_to_head(&ctx->id, &vdec_jobs);
	if (vdec_cur_job != 0) {
		vdec_cur_job->start = get_time_us();
		target_freq = est_freq(vdec_cur_job->handle, &vdec_jobs,
					vdec_hists);
		target_freq_64 = match_freq(target_freq, &vdec_freq_steps[0],
					vdec_freq_step_size);
		if (target_freq > 0) {
			vdec_freq = target_freq;
			if (vdec_freq > target_freq_64)
				vdec_freq = target_freq_64;
			vdec_cur_job->mhz = (int)target_freq_64;
			mtk_pm_qos_update_request(&vdec_qos_req_f, target_freq_64);
		}
	} else {
		target_freq_64 = match_freq(DEFAULT_MHZ, &vdec_freq_steps[0],
						vdec_freq_step_size);
		mtk_pm_qos_update_request(&vdec_qos_req_f, target_freq_64);
	}
	mutex_unlock(&ctx->dev->dec_dvfs_mutex);
#endif
#if DEC_DVFS
	long long op_rate_to_freq = 0;
	u64 target_freq_64 = 0;
	struct mtk_vcodec_dev *dev = ctx->dev;

	mutex_lock(&ctx->dev->dec_dvfs_mutex);
	if ((ctx->q_data[MTK_Q_DATA_DST].coded_width *
		ctx->q_data[MTK_Q_DATA_DST].coded_height) >=
		3840*2160) {
		vdec_req_freq[hw_id] = 416;
	} else {
		vdec_req_freq[hw_id] = STD_VDEC_FREQ;
	}

	if (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_MPEG1 ||
	ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_MPEG2 ||
	ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_MPEG4 ||
	ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_H263)
		vdec_req_freq[hw_id] = 312;

	if (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_VP8)
		vdec_req_freq[hw_id] = 416;

	if (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_HEIF)
		vdec_req_freq[hw_id] = 546;

	if ((ctx->dec_params.operating_rate > 121 || ctx->dec_params.operating_rate <= 0) &&
	(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_H264 ||
	ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_H265 ||
	ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_VP9 ||
	ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_AV1)) {
		vdec_req_freq[hw_id] = 546;
	} else if (ctx->dec_params.operating_rate > 0) {
		op_rate_to_freq = 416LL *
				ctx->q_data[MTK_Q_DATA_DST].coded_width *
				ctx->q_data[MTK_Q_DATA_DST].coded_height *
				ctx->dec_params.operating_rate /
				3840LL / 2160LL / 60LL;
		target_freq_64 = match_freq((int)op_rate_to_freq,
					&vdec_freq_steps[0],
					vdec_freq_step_size);

		vdec_req_freq[hw_id] = target_freq_64;
	}

	if (dev->dec_cnt > 2)
		vdec_req_freq[hw_id] = 546;

	vdec_freq = vdec_req_freq[0] > vdec_req_freq[1] ?
			vdec_req_freq[0] : vdec_req_freq[1];

	mtk_pm_qos_update_request(&vdec_qos_req_f, vdec_freq);
	mutex_unlock(&ctx->dev->dec_dvfs_mutex);
#endif
}

void mtk_vdec_dvfs_end(struct mtk_vcodec_ctx *ctx, int hw_id)
{
#if 0
	int freq_idx = 0;
	struct codec_job *vdec_cur_job = 0;

	/* vdec dvfs */
	mutex_lock(&ctx->dev->dec_dvfs_mutex);
	vdec_cur_job = vdec_jobs;
	if (vdec_cur_job->handle == &ctx->id) {
		vdec_cur_job->end = get_time_us();
		update_hist(vdec_cur_job, &vdec_hists, 0);
		vdec_jobs = vdec_jobs->next;
		kfree(vdec_cur_job);
	} else {
		/* print error log */
	}

	freq_idx = (vdec_freq_step_size == 0) ? 0 : (vdec_freq_step_size - 1);
	mtk_pm_qos_update_request(&vdec_qos_req_f, vdec_freq_steps[freq_idx]);
	mutex_unlock(&ctx->dev->dec_dvfs_mutex);
#endif
#if DEC_DVFS
	struct mtk_vcodec_dev *dev = ctx->dev;
	mutex_lock(&ctx->dev->dec_dvfs_mutex);

	vdec_req_freq[hw_id] = 0;
	vdec_freq = vdec_req_freq[0] > vdec_req_freq[1] ?
			vdec_req_freq[0] : vdec_req_freq[1];

	if ((ctx->dec_params.operating_rate > 121 || ctx->dec_params.operating_rate <= 0) &&
	(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_AV1)) {
		vdec_req_freq[hw_id] = 546;
	}

	if (dev->dec_cnt > 2)
		vdec_req_freq[hw_id] = 546;

	mtk_pm_qos_update_request(&vdec_qos_req_f, vdec_freq);
	mutex_unlock(&ctx->dev->dec_dvfs_mutex);
#endif
}

void mtk_vdec_emi_bw_begin(struct mtk_vcodec_ctx *ctx, int hw_id)
{
#if 0
	int b_freq_idx = 0;
	int f_type = 1; /* TODO */
	long emi_bw = 0;
	long emi_bw_input = 0;
	long emi_bw_output = 0;

	if (vdec_freq_step_size > 1)
		b_freq_idx = vdec_freq_step_size - 1;

	emi_bw = 8L * 1920 * 1080 * 3 * 10 * vdec_freq;
	emi_bw_input = 8 * vdec_freq / STD_VDEC_FREQ;
	emi_bw_output = 1920 * 1088 * 3 * 30 * 10 * vdec_freq /
			2 / 3 / STD_VDEC_FREQ / 1024 / 1024;

	switch (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc) {
	case V4L2_PIX_FMT_H264:
		emi_bw = emi_bw * h264_frm_scale[f_type] / (2 * STD_VDEC_FREQ);
		break;
	case V4L2_PIX_FMT_H265:
		emi_bw = emi_bw * h265_frm_scale[f_type] / (2 * STD_VDEC_FREQ);
		break;
	case V4L2_PIX_FMT_VP8:
		emi_bw = emi_bw * vp8_frm_scale[f_type] / (2 * STD_VDEC_FREQ);
		break;
	case V4L2_PIX_FMT_VP9:
		emi_bw = emi_bw * vp9_frm_scale[f_type] / (2 * STD_VDEC_FREQ);
		break;
	case V4L2_PIX_FMT_MPEG4:
	case V4L2_PIX_FMT_H263:
	case V4L2_PIX_FMT_MPEG1:
	case V4L2_PIX_FMT_MPEG2:
		emi_bw = emi_bw * mp24_frm_scale[f_type] / (2 * STD_VDEC_FREQ);
		break;
	}

	/* transaction bytes to occupied BW */
	emi_bw = emi_bw * 4 / 3;

	/* bits/s to MBytes/s */
	emi_bw = emi_bw / (1024 * 1024) / 8;

	mtk_pm_qos_update_request(&vdec_qos_req_bw, (int)emi_bw);
#endif
#if DEC_EMI_BW
	int f_type = 1;
	long emi_bw = 0;
	long emi_bw_input = 0;
	long emi_bw_output = 0;

	if (hw_id == MTK_VDEC_LAT) {
		switch (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc) {
		case V4L2_PIX_FMT_H264:
		case V4L2_PIX_FMT_H265:
		case V4L2_PIX_FMT_HEIF:
			emi_bw_input = 35 * vdec_freq / STD_VDEC_FREQ;
			break;
		case V4L2_PIX_FMT_VP9:
		case V4L2_PIX_FMT_AV1:
			emi_bw_input = 15 * vdec_freq / STD_VDEC_FREQ;
			break;
		default:
			emi_bw_input = 35 * vdec_freq / STD_VDEC_FREQ;
		}
		mm_qos_set_request(&vdec_lat0_vld, emi_bw_input, 0,
					BW_COMP_NONE);
		mm_qos_set_request(&vdec_lat0_vld2, 0, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_lat0_avc_mv, emi_bw_input * 2, 0,
					BW_COMP_NONE);
		mm_qos_set_request(&vdec_lat0_pred_rd, 10, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_lat0_tile, 0, 0, BW_COMP_NONE);
		if (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc ==
			V4L2_PIX_FMT_HEIF) {
			mm_qos_set_request(&vdec_lat0_wdma, 0, 0, BW_COMP_NONE);
		} else {
			mm_qos_set_request(&vdec_lat0_wdma, 0, emi_bw_input * 2,
					BW_COMP_NONE);
		}
		mm_qos_set_request(&vdec_lat0_rg_ctrl_dma, 0, 0, BW_COMP_NONE);
		mm_qos_update_all_request(&vdec_rlist_lat);

	} else if (hw_id == MTK_VDEC_CORE) {
		emi_bw = 8L * 1920 * 1080 * 9 * 10 * 5 * vdec_freq / 2 / 3;
		emi_bw_output = 1920L * 1088 * 9 * 30 * 10 * 5 * vdec_freq /
				4 / 3 / 3 / STD_VDEC_FREQ / 1024 / 1024;

		switch (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc) {
		case V4L2_PIX_FMT_H264:
			emi_bw_input = 70 * vdec_freq / STD_VDEC_FREQ;
			emi_bw = emi_bw * h264_frm_scale[f_type] /
					(2 * STD_VDEC_FREQ);
			break;
		case V4L2_PIX_FMT_H265:
			emi_bw_input = 70 * vdec_freq / STD_VDEC_FREQ;
			emi_bw = emi_bw * h265_frm_scale[f_type] /
					(2 * STD_VDEC_FREQ);
			break;
		case V4L2_PIX_FMT_HEIF:
			emi_bw_input = 0;
			emi_bw = emi_bw * h265_frm_scale[f_type] /
					(2 * STD_VDEC_FREQ);
			break;
		case V4L2_PIX_FMT_VP8:
			emi_bw_input = 15 * vdec_freq / STD_VDEC_FREQ;
			emi_bw = emi_bw * vp8_frm_scale[f_type] /
					(2 * STD_VDEC_FREQ);
			break;
		case V4L2_PIX_FMT_VP9:
			emi_bw_input = 30 * vdec_freq / STD_VDEC_FREQ;
			emi_bw = emi_bw * vp9_frm_scale[f_type] /
					(2 * STD_VDEC_FREQ);
			break;
		case V4L2_PIX_FMT_AV1:
			emi_bw_input = 30 * vdec_freq / STD_VDEC_FREQ;
			emi_bw = emi_bw * vp9_frm_scale[f_type] /
					(2 * STD_VDEC_FREQ);
			break;
		case V4L2_PIX_FMT_MPEG4:
		case V4L2_PIX_FMT_H263:
		case V4L2_PIX_FMT_MPEG1:
		case V4L2_PIX_FMT_MPEG2:
			emi_bw_input = 15 * vdec_freq / STD_VDEC_FREQ;
			emi_bw = emi_bw * mp24_frm_scale[f_type] /
					(2 * STD_VDEC_FREQ);
			break;
		}
		emi_bw = emi_bw / (1024 * 1024) / 8;
		emi_bw = emi_bw - emi_bw_output - emi_bw_input;
		if (emi_bw < 0)
			emi_bw = 0;

		if (ctx->picinfo.layout_mode == VDEC_DRV_UFO_AUO_ON) {
			mm_qos_set_request(&vdec_ufo, emi_bw, 0, BW_COMP_NONE);
			mm_qos_set_request(&vdec_ufo_enc, emi_bw_output, 0,
						BW_COMP_NONE);
		} else {
			mm_qos_set_request(&vdec_mc, emi_bw, 0, BW_COMP_NONE);
			mm_qos_set_request(&vdec_pp, emi_bw_output, 0,
						BW_COMP_NONE);
		}
		mm_qos_set_request(&vdec_pred_rd, 1, 0, BW_COMP_NONE);
		if ((ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc ==
			V4L2_PIX_FMT_AV1) ||
			(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc ==
			V4L2_PIX_FMT_VP9))
			mm_qos_set_request(&vdec_pred_wr, emi_bw, 0,
				BW_COMP_NONE);
		else
			mm_qos_set_request(&vdec_pred_wr, 1, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_ppwrap, 0, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_tile, 0, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_vld, emi_bw_input, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_vld2, 0, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_avc_mv, emi_bw_input, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_rg_ctrl_dma, 0, 0, BW_COMP_NONE);
		mm_qos_update_all_request(&vdec_rlist_core);
	} else {
		pr_debug("%s unknown hw_id %d\n", __func__, hw_id);
	}

#endif
}

static void mtk_vdec_emi_bw_end(int hw_id)
{
#if DEC_EMI_BW
	if (hw_id == MTK_VDEC_LAT) {
		mm_qos_set_request(&vdec_lat0_vld, 0, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_lat0_vld2, 0, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_lat0_avc_mv, 0, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_lat0_pred_rd, 0, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_lat0_tile, 0, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_lat0_wdma, 0, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_lat0_rg_ctrl_dma, 0, 0, BW_COMP_NONE);
		mm_qos_update_all_request(&vdec_rlist_lat);
	} else if (hw_id == MTK_VDEC_CORE) {
		mm_qos_set_request(&vdec_mc, 0, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_ufo, 0, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_pp, 0, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_pred_rd, 0, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_pred_wr, 0, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_ppwrap, 0, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_tile, 0, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_vld, 0, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_vld2, 0, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_avc_mv, 0, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_rg_ctrl_dma, 0, 0, BW_COMP_NONE);
		mm_qos_set_request(&vdec_ufo_enc, 0, 0, BW_COMP_NONE);
		mm_qos_update_all_request(&vdec_rlist_core);
	} else {
		pr_debug("%s unknown hw_id %d\n", __func__, hw_id);
	}
#endif
}

void mtk_vdec_pmqos_prelock(struct mtk_vcodec_ctx *ctx, int hw_id)
{
#if DEC_DVFS
	mutex_lock(&ctx->dev->dec_dvfs_mutex);
	/* add_job(&ctx->id, &vdec_jobs); */
	mutex_unlock(&ctx->dev->dec_dvfs_mutex);
#endif
}

void mtk_vdec_pmqos_begin_frame(struct mtk_vcodec_ctx *ctx, int hw_id)
{
	mtk_vdec_dvfs_begin(ctx, hw_id);
	mtk_vdec_emi_bw_begin(ctx, hw_id);
}

void mtk_vdec_pmqos_end_frame(struct mtk_vcodec_ctx *ctx, int hw_id)
{
	mtk_vdec_dvfs_end(ctx, hw_id);
	mtk_vdec_emi_bw_end(hw_id);
}

int mtk_vdec_ion_config_buff(struct dma_buf *dmabuf)
{
/* for dma-buf using ion buffer, ion will check portid in dts
 * So, don't need to config buffer at user side, but remember
 * set iommus attribute in dts file.
 */
#if 0
	struct ion_handle *handle = NULL;
	struct ion_mm_data mm_data;

	mtk_v4l2_debug(4, "%p", dmabuf);

	if (!ion_vdec_client)
		ion_vdec_client = ion_client_create(g_ion_device, "vdec");

	handle = ion_import_dma_buf(ion_vdec_client, dmabuf);
	if (IS_ERR(handle)) {
		mtk_v4l2_err("import ion handle failed!\n");
		return -1;
	}
	mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
	mm_data.config_buffer_param.kernel_handle = handle;
	mm_data.config_buffer_param.module_id = M4U_PORT_L4_VDEC_MC_EXT_MDP;
	mm_data.config_buffer_param.security = 0;
	mm_data.config_buffer_param.coherent = 0;

	if (ion_kernel_ioctl(ion_vdec_client, ION_CMD_MULTIMEDIA,
		(unsigned long)&mm_data)) {
		mtk_v4l2_err("configure ion buffer failed!\n");
		/* dma hold ref, ion directly free */
		ion_free(ion_vdec_client, handle);

		return -1;
	}

	/* dma hold ref, ion directly free */
	ion_free(ion_vdec_client, handle);
#endif
	return 0;
}

