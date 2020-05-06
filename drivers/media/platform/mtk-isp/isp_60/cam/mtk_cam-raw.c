// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/vmalloc.h>
#include <linux/jiffies.h>

#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>

#include "mtk_cam.h"
#include "mtk_cam-raw.h"
#include "mtk_cam-regs.h"
#include "mtk_cam-video.h"

#ifdef CONFIG_MTK_SMI_EXT
#include "smi_public.h"
#endif

#ifdef CONFIG_MTK_IOMMU_V2
#include "mtk_iommu_ext.h"
#include "mach/pseudo_m4u.h"
#endif

#define MTK_RAW_STOP_HW_TIMEOUT			(33 * USEC_PER_MSEC)

static const struct of_device_id mtk_raw_of_ids[] = {
	{.compatible = "mediatek,isp6s-cam",},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_raw_of_ids);

#define MAX_NUM_CLOCKS 6

#define MTK_CAMSYS_RES_IDXMASK		0xF0
#define MTK_CAMSYS_RES_BIN_TAG		0x10
#define MTK_CAMSYS_RES_FRZ_TAG		0x20
#define MTK_CAMSYS_RES_HWN_TAG		0x30
#define MTK_CAMSYS_RES_CLK_TAG		0x40

#define MTK_CAMSYS_RES_PLAN_NUM		10
#define TGO_MAX_PXLMODE		8
#define FRZ_PXLMODE_THRES		71
#define MHz		1000000

enum MTK_CAMSYS_RES_STEP {
	E_RES_BASIC,
	E_RES_BIN_S = MTK_CAMSYS_RES_BIN_TAG,
	E_RES_BIN0 = E_RES_BIN_S,
	E_RES_BIN1,
	E_RES_BIN_E,
	E_RES_FRZ_S = MTK_CAMSYS_RES_FRZ_TAG,
	E_RES_FRZ0 = E_RES_FRZ_S,
	E_RES_FRZ1,
	E_RES_FRZ_E,
	E_RES_HWN_S = MTK_CAMSYS_RES_HWN_TAG,
	E_RES_HWN0 = E_RES_HWN_S,
	E_RES_HWN1,
	E_RES_HWN_E,
	E_RES_CLK_S = MTK_CAMSYS_RES_CLK_TAG,
	E_RES_CLK0 = E_RES_CLK_S,
	E_RES_CLK1,
	E_RES_CLK2,
	E_RES_CLK3,
	E_RES_CLK_E,
};

enum MTK_CAMSYS_MAXLB_CHECK_RESULT {
	LB_CHECK_OK = 0,
	LB_CHECK_CBN,
	LB_CHECK_QBN,
	LB_CHECK_BIN,
	LB_CHECK_FRZ,
	LB_CHECK_TWIN,
	LB_CHECK_RAW,
};

#ifdef ISP7
	#define CAM_RAW_PROCESS_MAX_LINE_BUFFER		(6632)
	#define CAM_RAW_FRZ_MAX_LINE_BUFFER		(6632)
	#define CAM_RAW_BIN_MAX_LINE_BUFFER		(12000)
	#define CAM_RAW_QBND_MAX_LINE_BUFFER		(16000)
	#define CAM_RAW_CBN_MAX_LINE_BUFFER		(18472)
	#define CAM_TWIN_PROCESS_MAX_LINE_BUFFER	(12400)
#else
	#define CAM_RAW_PROCESS_MAX_LINE_BUFFER		(5504)
	#define CAM_RAW_FRZ_MAX_LINE_BUFFER		(5888)
	#define CAM_RAW_BIN_MAX_LINE_BUFFER		(9312)
	/* fbnd in 6s */
	#define CAM_RAW_QBND_MAX_LINE_BUFFER		(9312)
	/* no in 6s */
	#define CAM_RAW_CBN_MAX_LINE_BUFFER		(9312)
	#define CAM_TWIN_PROCESS_MAX_LINE_BUFFER	(9312)
#endif

struct cam_resource_plan {
	int cam_resource[MTK_CAMSYS_RES_STEP_NUM];
};

enum resource_strategy_id {
	RESOURCE_STRATEGY_QPR = 0,
	RESOURCE_STRATEGY_PQR,
	RESOURCE_STRATEGY_RPQ,
	RESOURCE_STRATEGY_QRP,
	RESOURCE_STRATEGY_NUMBER
};

static const struct cam_resource_plan raw_resource_strategy_plan[] = {
	[RESOURCE_STRATEGY_QPR] = {
		.cam_resource = {
			E_RES_BASIC, E_RES_HWN1, E_RES_CLK1, E_RES_CLK2,
			E_RES_CLK3, E_RES_FRZ1, E_RES_BIN1} },
	[RESOURCE_STRATEGY_PQR] = {
		.cam_resource = {
			E_RES_BASIC, E_RES_HWN1, E_RES_FRZ1, E_RES_BIN1,
			E_RES_CLK1, E_RES_CLK2, E_RES_CLK3} },
	[RESOURCE_STRATEGY_RPQ] = {
		.cam_resource = {
			E_RES_BASIC, E_RES_FRZ1, E_RES_BIN1, E_RES_CLK1,
			E_RES_CLK2, E_RES_CLK3, E_RES_HWN1} },
	[RESOURCE_STRATEGY_QRP] = {
		.cam_resource = {
			E_RES_BASIC, E_RES_CLK1, E_RES_CLK2, E_RES_CLK3,
			E_RES_HWN1, E_RES_FRZ1, E_RES_BIN1} },
};

struct raw_resource {
	char *clock[MAX_NUM_CLOCKS];
};

static const struct raw_resource raw_resources[] = {
	[RAW_A] = {
		.clock = {
			"camsys_cam_cgpdn", "camsys_camtg_cgpdn",
			"camsys-cama-larb-cgpdn", "camsys-cama-cgpdn",
			"camsys-camatg-cgpdn", "topckgen-top-muxcamtm"},
	},
	[RAW_B] = {
		.clock = {
			"camsys_cam_cgpdn", "camsys_camtg_cgpdn",
			"camsys-camb-larb-cgpdn", "camsys-camb-cgpdn",
			"camsys-cambtg-cgpdn", "topckgen-top-muxcamtm"},
	},
	[RAW_C] = {
		.clock = {
			"camsys_cam_cgpdn", "camsys_camtg_cgpdn",
			"camsys-camc-larb-cgpdn", "camsys-camc-cgpdn",
			"camsys-camctg-cgpdn", "topckgen-top-muxcamtm"},
	},
};

#ifdef CONFIG_MTK_SMI_EXT

#define PORT_SIZE 17

struct raw_port {
	int smi_id;
	char *port[PORT_SIZE];
};

static const struct raw_port raw_ports[] = {
	[RAW_A] = {
		.smi_id = SMI_LARB16,
		.port = {"imgo_r1_a", "rrzo_r1_a", "cqi_r1_a", "bpci_r1_a",
			"yuvo_r1_a", "ufdi_r2_a", "rawi_r2_a", "rawi_r3_a",
			"aao_r1_a", "afo_r1_a", "flko_r1_a", "lceso_r1_a",
			"crzo_r1_a", "ltmso_r1_a", "rsso_r1_a", "aaho_r1_a",
			"lsci_r1_a"},
	},
	[RAW_B] = {
		.smi_id = SMI_LARB17,
		.port = {"imgo_r1_b", "rrzo_r1_b", "cqi_r1_b", "bpci_r1_b",
			"yuvo_r1_b", "ufdi_r2_b", "rawi_r2_b", "rawi_r3_b",
			"aao_r1_b", "afo_r1_b", "flko_r1_b", "lceso_r1_b",
			"crzo_r1_b", "ltmso_r1_b", "rsso_r1_b", "aaho_r1_b",
			"lsci_r1_b"},
	},
	[RAW_C] = {
		.smi_id = SMI_LARB18,
		.port = {"imgo_r1_c", "rrzo_r1_c", "cqi_r1_c", "bpci_r1_c",
			"yuvo_r1_c", "ufdi_r2_c", "rawi_r2_c", "rawi_r3_c",
			"aao_r1_c", "afo_r1_c", "flko_r1_c", "lceso_r1_c",
			"crzo_r1_c", "ltmso_r1_c", "rsso_r1_c", "aaho_r1_c",
			"lsci_r1_c"},
	},
};

#endif /* CONFIG_MTK_SMI_EXT */

static const struct v4l2_mbus_framefmt mfmt_default = {
	.code = MEDIA_BUS_FMT_SBGGR10_1X10,
	.width = DEFAULT_WIDTH,
	.height = DEFAULT_HEIGHT,
	.field = V4L2_FIELD_NONE,
	.colorspace = V4L2_COLORSPACE_SRGB,
	.xfer_func = V4L2_XFER_FUNC_DEFAULT,
	.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT,
	.quantization = V4L2_QUANTIZATION_DEFAULT,
};

static int mtk_raw_get_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_raw_pipeline *pipeline =
		container_of(ctrl->handler, struct mtk_raw_pipeline,
			     ctrl_handler);
	struct device *dev = pipeline->raw->devs[pipeline->id];
	int ret = 0;

	mutex_lock(&pipeline->res_config.resource_lock);
	switch (ctrl->id) {
	case V4L2_CID_ENGINE_RESOURCE_USAGE_LIMITATION:
		ctrl->val = pipeline->res_config.raw_num_used;
		dev_dbg(dev, "get_ctrl (id:%s, val:%d)\n", ctrl->name,
			ctrl->val);
		break;
	default:
		break;
	}
	mutex_unlock(&pipeline->res_config.resource_lock);
	return ret;
}

static int mtk_raw_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_raw_pipeline *pipeline =
		container_of(ctrl->handler, struct mtk_raw_pipeline,
			     ctrl_handler);
	struct device *dev = pipeline->raw->devs[pipeline->id];
	int ret = 0;

	mutex_lock(&pipeline->res_config.resource_lock);
	switch (ctrl->id) {
	case V4L2_CID_ENGINE_RESOURCE_USAGE_LIMITATION:
		pipeline->res_config.hwn_limit = ctrl->val;
		break;
	case V4L2_CID_BIN_LIMITATION:
		pipeline->res_config.bin_limit = ctrl->val;
		break;
	case V4L2_CID_FRZ_LIMITATION:
		pipeline->res_config.frz_limit = ctrl->val;
		break;
	case V4L2_CID_RESOURCE_PLAN_POLICY:
		pipeline->res_config.res_plan = ctrl->val;
		break;
	default:
		dev_info(dev, "ctrl(id:0x%x,val:%d) is not handled\n",
			 ctrl->id, ctrl->val);
		ret = -EINVAL;
		break;
	}
	mutex_unlock(&pipeline->res_config.resource_lock);
	dev_dbg(dev, "%s (name:%s, val:%d)\n", __func__, ctrl->name,
		ctrl->val);
	return ret;
}

static const struct v4l2_ctrl_ops cam_ctrl_ops = {
	.g_volatile_ctrl = mtk_raw_get_ctrl,
	.s_ctrl = mtk_raw_set_ctrl,
};

static const struct v4l2_ctrl_config hwn_limit = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_ENGINE_RESOURCE_USAGE_LIMITATION,
	.name = "Engine resource limitation",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 1,
	.max = 2,
	.step = 1,
	.def = 2,
};

static const struct v4l2_ctrl_config bin_limit = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_BIN_LIMITATION,
	.name = "Binning limitation",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.min = 0,
	.max = 1,
	.step = 1,
	.def = 1,
};

static const struct v4l2_ctrl_config frz_limit = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_FRZ_LIMITATION,
	.name = "Resizer limitation",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 70,
	.max = 100,
	.step = 1,
	.def = 100,
};

static const struct v4l2_ctrl_config res_plan_policy = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_RESOURCE_PLAN_POLICY,
	.name = "Resource planning policy",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 10,
	.step = 1,
	.def = 1,
};

void apply_cq(struct mtk_raw_device *dev,
	      dma_addr_t cq_addr, unsigned int cq_size, int initial)
{
	dev_dbg(dev->dev,
		"apply raw%d cq - addr:0x%08x ,size:%d, REG_CQ_THR0_CTL:0x%8x\n",
		dev->id, cq_addr, cq_size,
		readl_relaxed(dev->base + REG_CQ_THR0_CTL));
	writel_relaxed(cq_addr, dev->base + REG_CQ_THR0_BASEADDR);
	writel_relaxed(cq_size, dev->base + REG_CQ_THR0_DESC_SIZE);
	wmb(); /* TBC */
	if (initial) {
		writel_relaxed(CAMCTL_CQ_THR0_DONE_ST,
			       dev->base + REG_CTL_RAW_INT6_EN);
		writel_relaxed(CTL_CQ_THR0_START,
			       dev->base + REG_CTL_START);
		wmb(); /* TBC */
		return;
	}
#if USINGSCQ
	writel_relaxed(CTL_CQ_THR0_START, dev->base + REG_CTL_START);
	wmb(); /* TBC */
#endif
}

void reset(struct mtk_raw_device *dev)
{
	unsigned long end = jiffies + msecs_to_jiffies(100);

	dev_dbg(dev->dev, "%s\n", __func__);

	writel_relaxed(0, dev->base + REG_CTL_SW_CTL);
	writel_relaxed(1, dev->base + REG_CTL_SW_CTL);
	wmb(); /* TBC */

	while (time_before(jiffies, end)) {
		if (readl(dev->base + REG_CTL_SW_CTL) & 0x2) {
			/* do hw rst */
			writel_relaxed(4, dev->base + REG_CTL_SW_CTL);
			writel_relaxed(0, dev->base + REG_CTL_SW_CTL);
			wmb(); /* TBC */
			return;
		}

		dev_info(dev->dev,
			 "tg_sen_mode: 0x%x, ctl_en: 0x%x, ctl_sw_ctl:0x%x,frame_no:0x%x\n",
			 readl(dev->base + REG_TG_SEN_MODE),
			 readl(dev->base + REG_CTL_EN),
			 readl(dev->base + REG_CTL_SW_CTL),
			 readl(dev->base + REG_FRAME_SEQ_NUM));
		usleep_range(10, 20);
	}

	dev_warn(dev->dev, "s%: hw timeout\n", __func__);
}

void initialize(struct mtk_raw_device *dev)
{
#if USINGSCQ
	u32 val;

	val = readl_relaxed(dev->base + REG_CQ_EN);
	writel_relaxed(val | SCQ_EN, dev->base + REG_CQ_EN);
	writel_relaxed(0xffffffff, dev->base + REG_SCQ_START_PERIOD);
	wmb(); /* TBC */
#endif
	writel_relaxed(CQ_THR0_MODE_IMMEDIATE | CQ_THR0_EN,
		       dev->base + REG_CQ_THR0_CTL);
	wmb(); /* TBC */
	dev->sof_count = 0;
	dev_dbg(dev->dev, "%s - REG_CQ_EN:0x%x ,REG_CQ_THR0_CTL:0x%8x\n",
		__func__,
			readl_relaxed(dev->base + REG_CQ_EN),
			readl_relaxed(dev->base + REG_CQ_THR0_CTL));
}

void stream_on(struct mtk_raw_device *dev, int on)
{
	u32 val;
	u32 chk_val;
	unsigned long end;

	dev_dbg(dev->dev, "raw %d %s %d\n", dev->id, __func__, on);

	if (on) {
#if USINGSCQ
		val = readl_relaxed(dev->base + REG_TG_TIME_STAMP_CNT);
		writel_relaxed(SCQ_DEADLINE_MS * 1000 * SCQ_DEFAULT_CLK_RATE /
			       (val * 2),
			       dev->base + REG_SCQ_START_PERIOD);
#else
		writel_relaxed(CQ_THR0_MODE_CONTINUOUS | CQ_THR0_EN,
			       dev->base + REG_CQ_THR0_CTL);
		writel_relaxed(CQ_DB_EN | CQ_DB_LOAD_MODE,
			       dev->base + REG_CQ_EN);
		wmb(); /* TBC */
#endif
		val = readl_relaxed(dev->base + REG_TG_VF_CON);
		val |= TG_VF_CON_VFDATA_EN;
		writel_relaxed(val, dev->base + REG_TG_VF_CON);
		wmb(); /* TBC */
		dev_dbg(dev->dev,
			"%s - REG_CQ_EN:0x%x, REG_CQ_THR0_CTL:0x%8x, REG_TG_VF_CON:0x%8x\n",
			__func__,
			readl_relaxed(dev->base + REG_CQ_EN),
			readl_relaxed(dev->base + REG_CQ_THR0_CTL),
			readl_relaxed(dev->base + REG_TG_VF_CON));
	} else {
		writel_relaxed(~CQ_THR0_EN, dev->base + REG_CQ_THR0_CTL);
		wmb(); /* TBC */

		val = readl_relaxed(dev->base + REG_TG_VF_CON);
		val &= ~TG_VF_CON_VFDATA_EN;
		writel_relaxed(val, dev->base + REG_TG_VF_CON);
		wmb(); /* TBC */

		/* reset hw after vf off */
		end = jiffies + msecs_to_jiffies(10);
		while (time_before(jiffies, end)) {
			chk_val = readl_relaxed(dev->base + REG_TG_VF_CON);
			if (chk_val == val) {
				reset(dev);
				break;
			}
			usleep_range(10, 20);
		}
	}
}

static int mtk_raw_linebuf_chk(bool b_twin, bool b_bin, bool b_frz, bool b_qbn,
			       bool b_cbn, int tg_x, int *frz_ratio)
{
	int input_x = tg_x;
	/* max line buffer check for frontal binning and resizer */
	if (b_twin) {
		if (input_x > CAM_TWIN_PROCESS_MAX_LINE_BUFFER)
			return LB_CHECK_TWIN;
		input_x = input_x >> 1;
	}
	if (b_cbn) {
		if (input_x > CAM_RAW_CBN_MAX_LINE_BUFFER)
			return LB_CHECK_CBN;
		input_x = input_x >> 1;
	}
	if (b_qbn) {
		if (input_x > CAM_RAW_QBND_MAX_LINE_BUFFER)
			return LB_CHECK_QBN;
		input_x = input_x >> 1;
	}
	if (b_bin) {
		if (input_x > CAM_RAW_BIN_MAX_LINE_BUFFER)
			return LB_CHECK_BIN;
		input_x = input_x >> 1;
	}
	if (input_x <= CAM_RAW_PROCESS_MAX_LINE_BUFFER) {
		return LB_CHECK_OK;
	} else if (b_frz) {
		if (input_x > CAM_RAW_FRZ_MAX_LINE_BUFFER)
			return LB_CHECK_FRZ;

		*frz_ratio = input_x * 100 /
			CAM_RAW_PROCESS_MAX_LINE_BUFFER;
		return LB_CHECK_OK;
	} else {
		return LB_CHECK_RAW;
	}
}

static int mtk_raw_pixelmode_calc(bool b_twin, bool b_bin,
				  bool b_frz, int min_ratio)
{
	int pixelmode = 1;

	pixelmode = b_twin ? pixelmode << 1 : pixelmode;
	pixelmode = b_bin ? pixelmode << 2 : pixelmode;
	pixelmode = (b_frz && (min_ratio < FRZ_PXLMODE_THRES))
			? pixelmode << 1 : pixelmode;
	pixelmode = (pixelmode > TGO_MAX_PXLMODE) ?
		TGO_MAX_PXLMODE : pixelmode;

	return pixelmode;
}

static bool mtk_raw_resource_calc(struct mtk_raw_pipeline *pipe,
				  int in_w, int in_h, int *out_w, int *out_h)
{
	struct mtk_cam_device *cam =
		container_of(pipe->raw, struct mtk_cam_device, raw);
	struct mtk_camsys_clkinfo *clk = &cam->camsys_ctrl.clk_info;
	struct mtk_cam_resource_config *res = &pipe->res_config;
	u64 eq_throughput = clk->clklv[clk->clklv_num - 1] * MHz;
	int res_step_type = 0;
	int tgo_pxl_mode = 1;
	int pixel_mode[MTK_CAMSYS_RES_STEP_NUM] = {0};
	int bin_temp = 0, frz_temp = 0, hwn_temp = 0;
	int bin_en = 0, frz_en = 0, twin_en = 0, clk_cur = 0;
	int idx = 0, clk_res = 0, idx_res = 0;
	bool res_found = false;
	int lb_chk_res = -1;
	int frz_ratio = 100;

	mutex_lock(&pipe->res_config.resource_lock);
	mtk_cam_seninf_get_pixelrate(pipe->res_config.seninf,
				     &pipe->res_config.pixel_rate);
	dev_dbg(cam->dev,
		"[Res-PARAM] PR = %lld, in_w/in_h = %d/%d Limit[HWN(%d)/BIN(%d)/FRZ(%d)],Plan:%d\n",
		res->pixel_rate, in_w, in_h,
		res->hwn_limit, res->bin_limit, res->frz_limit, res->res_plan);
	memcpy(res->res_strategy, raw_resource_strategy_plan + res->res_plan,
	       MTK_CAMSYS_RES_STEP_NUM * sizeof(int));
	res->bin_enable = 0;
	res->raw_num_used = 1;
	res->frz_enable = 0;
	res->frz_ratio = frz_ratio;
	for (idx = 0; idx < MTK_CAMSYS_RES_STEP_NUM ; idx++) {
		res_step_type = res->res_strategy[idx] & MTK_CAMSYS_RES_IDXMASK;
		switch (res_step_type) {
		case MTK_CAMSYS_RES_BIN_TAG:
			bin_temp = res->res_strategy[idx] - E_RES_BIN_S;
			if (bin_temp <= res->bin_limit)
				bin_en = bin_temp;
			if (bin_en && frz_en)
				frz_en = 0;
			break;
		case MTK_CAMSYS_RES_FRZ_TAG:
			frz_temp = res->res_strategy[idx] - E_RES_FRZ_S;
			if (res->frz_limit < 100)
				frz_en = frz_temp;
			break;
		case MTK_CAMSYS_RES_HWN_TAG:
			hwn_temp = res->res_strategy[idx] - E_RES_HWN_S;
			if (hwn_temp + 1 <= res->hwn_limit)
				twin_en = hwn_temp;
			break;
		case MTK_CAMSYS_RES_CLK_TAG:
			clk_cur = res->res_strategy[idx] - E_RES_CLK_S;
			break;
		default:
			break;
		}
		/* max line buffer check*/
		lb_chk_res = mtk_raw_linebuf_chk(bin_en, frz_en, twin_en, 0, 0,
						 in_w, &frz_ratio);
		/* frz ratio*/
		if (res_step_type == MTK_CAMSYS_RES_FRZ_TAG) {
			if (eq_throughput > res->pixel_rate &&
			    lb_chk_res == LB_CHECK_OK)
				res->frz_ratio = frz_ratio;
			else
				res->frz_ratio =
					res->frz_limit < FRZ_PXLMODE_THRES
					? res->frz_limit : FRZ_PXLMODE_THRES;
		}
		tgo_pxl_mode = mtk_raw_pixelmode_calc(twin_en, bin_en, frz_en,
						      res->frz_ratio);
		/**
		 * isp throughput along resource strategy
		 * (compared with pixel rate)
		 */
		pixel_mode[idx] = tgo_pxl_mode;
		eq_throughput = ((u64)tgo_pxl_mode) *
			clk->clklv[clk->clklv_num - clk_cur - 1] * MHz;
		if (eq_throughput > res->pixel_rate &&
		    lb_chk_res == LB_CHECK_OK) {
			if (!res_found) {
				res->bin_enable = bin_en;
				res->frz_enable = frz_en;
				res->raw_num_used = twin_en ? 2 : 1;
				clk_res = clk_cur;
				idx_res = idx;
				clk->clklv_target =
					clk->clklv[clk->clklv_num -
						   clk_res - 1];
				res_found = true;
			}
		}
		dev_dbg(cam->dev, "[Res-STEP_%d] BIN/FRZ/HWN/CLK = %d/%d/%d/%d -> %d/%d/%d/%d,throughput=%10llu, LB=%d\n",
			idx, bin_temp, frz_temp, hwn_temp, clk_cur, bin_en,
			frz_en, twin_en, clk_cur, eq_throughput, lb_chk_res);
	}
	tgo_pxl_mode = pixel_mode[idx_res];
	eq_throughput = ((u64)tgo_pxl_mode) * clk->clklv_target * MHz;
	if (res_found)
		dev_dbg(cam->dev, "[Res-Final:%d] BIN/FRZ/HWN/CLK/tgo_pxl = %d/%d(%d)/%d/%d/%d ,EQ_PR=%lld (throughput=%10llu) clk_target:%d\n",
			idx_res,
			res->bin_enable, res->frz_enable, res->frz_ratio,
			res->raw_num_used,
			clk_res, tgo_pxl_mode, res->pixel_rate, eq_throughput,
			clk->clklv_target);
	else
		dev_dbg(cam->dev, "[%s] Error resource result\n", __func__);
	if (res->bin_enable) {
		*out_w = in_w >> 1;
		*out_h = in_h >> 1;
	} else if (res->frz_enable) {
		*out_w = in_w * res->frz_ratio / 100;
		*out_h = in_h * res->frz_ratio / 100;
	} else {
		*out_w = in_w;
		*out_h = in_h;
	}
	mutex_unlock(&pipe->res_config.resource_lock);

	return res_found;
}

static void raw_irq_handle_tg_grab_err(struct mtk_raw_device *raw_dev);

static void raw_irq_handle_dma_err(struct mtk_raw_device *raw_dev);

static irqreturn_t mtk_irq_raw(int irq, void *data)
{
	struct mtk_raw_device *raw_dev = (struct mtk_raw_device *)data;
	struct device *dev = raw_dev->dev;
	struct mtk_camsys_irq_info irq_info;
	unsigned int dequeued_frame_seq_no, dequeued_frame_seq_no_inner;
	unsigned int scq_deadline, trig_time, tg_timestamp, cq_ctl;
	unsigned int irq_status, err_status, dma_done_status, dmai_done_status;
	unsigned int drop_status, dma_err_status, cq_done_status;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&raw_dev->spinlock_irq, flags);
	irq_status	= readl_relaxed(raw_dev->base + REG_CTL_RAW_INT_STAT);
	dma_done_status = readl_relaxed(raw_dev->base + REG_CTL_RAW_INT2_STAT);
	dmai_done_status = readl_relaxed(raw_dev->base + REG_CTL_RAW_INT3_STAT);
	drop_status	= readl_relaxed(raw_dev->base + REG_CTL_RAW_INT4_STAT);
	dma_err_status  = readl_relaxed(raw_dev->base + REG_CTL_RAW_INT5_STAT);
	cq_done_status  = readl_relaxed(raw_dev->base + REG_CTL_RAW_INT6_STAT);
	scq_deadline = readl_relaxed(raw_dev->base + REG_SCQ_START_PERIOD);
	trig_time = readl_relaxed(raw_dev->base + REG_SCQ_CQ_TRIG_TIME);
	cq_ctl = readl_relaxed(raw_dev->base + REG_CQ_EN);
	tg_timestamp = readl_relaxed(raw_dev->base + REG_TG_TIME_STAMP);

	/**
	 * TODO: read seq number from outer register.
	 * This value may be wrong, since
	 * @sw p1 done: legacy cq is triggered, overwriting this value
	 * Should replace with the the frame-header implementation.
	 */
	dequeued_frame_seq_no =
		readl_relaxed(raw_dev->base + REG_FRAME_SEQ_NUM);
	dequeued_frame_seq_no_inner =
		readl_relaxed(raw_dev->base_inner + REG_FRAME_SEQ_NUM);
	spin_unlock_irqrestore(&raw_dev->spinlock_irq, flags);

	err_status = irq_status & INT_ST_MASK_CAM_ERR;

	dev_dbg(dev,
		"%i status 0x%x(err 0x%x) dmao/dmai_done:0x%x/0x%x drop:0x%x dma_err:0x%x cq_done:0x%x scq_deadline/trig_time/cq_ctrl:0x%x/0x%x/0x%x\n",
		raw_dev->id,
		irq_status, err_status,
		dma_done_status, dmai_done_status, drop_status, dma_err_status,
		cq_done_status, scq_deadline, trig_time, cq_ctl);
	/*
	 * In normal case, the next SOF ISR should come after HW PASS1 DONE ISR.
	 * If these two ISRs come together, print warning msg to hint.
	 */
	irq_info.engine_id = CAMSYS_ENGINE_RAW_BEGIN + raw_dev->id;
	irq_info.frame_idx = dequeued_frame_seq_no;
	irq_info.frame_inner_idx = dequeued_frame_seq_no_inner;
	irq_info.irq_type = 0;
	if ((irq_status & SOF_INT_ST) && (irq_status & HW_PASS1_DON_ST))
		dev_warn(dev, "sof_done block cnt:%d\n", raw_dev->sof_count);
	/* CQ done */
	if (cq_done_status & CAMCTL_CQ_THR0_DONE_ST)
		irq_info.irq_type |= 1 << CAMSYS_IRQ_SETTING_DONE;
	/* Frame done */
	if (irq_status & SW_PASS1_DON_ST)
		irq_info.irq_type |= 1 << CAMSYS_IRQ_FRAME_DONE;
	/* Frame start */
	if (irq_status & SOF_INT_ST) {
		irq_info.irq_type |= 1 << CAMSYS_IRQ_FRAME_START;
		raw_dev->sof_count++;
	}
	/* inform interrupt information to camsys controller */
	ret = mtk_camsys_isr_event(raw_dev->cam, &irq_info);
	if (ret)
		goto ctx_not_found;
	/* Check ISP error status */
	if (err_status) {
		dev_err(dev, "int_err:0x%x 0x%x\n", irq_status, err_status);
		/* Show DMA errors in detail */
		if (err_status & DMA_ERR_ST)
			raw_irq_handle_dma_err(raw_dev);
		/* Show TG register for more error detail*/
		if (err_status & TG_GBERR_ST)
			raw_irq_handle_tg_grab_err(raw_dev);
	}
ctx_not_found:

	return IRQ_HANDLED;
}

void raw_irq_handle_tg_grab_err(struct mtk_raw_device *raw_dev)
{
	int val, val2;

	val = readl_relaxed(raw_dev->base + REG_TG_PATH_CFG);
	val = val | TG_TG_FULL_SEL;
	writel_relaxed(val, raw_dev->base + REG_TG_PATH_CFG);
	wmb(); /* TBC */
	val2 = readl_relaxed(raw_dev->base + REG_TG_SEN_MODE);
	val2 = val2 | TG_CMOS_RDY_SEL;
	writel_relaxed(val2, raw_dev->base + REG_TG_SEN_MODE);
	wmb(); /* TBC */
	dev_err_ratelimited(raw_dev->dev,
			    "TG_PATH_CFG:0x%x, TG_SEN_MODE:0x%xTG_FRMSIZE_ST:0x%8x, TG_FRMSIZE_ST_R:0x%8xTG_SEN_GRAB_PXL:0x%8x, TG_SEN_GRAB_LIN:0x%8x\n",
		readl_relaxed(raw_dev->base + REG_TG_PATH_CFG),
		readl_relaxed(raw_dev->base + REG_TG_SEN_MODE),
		readl_relaxed(raw_dev->base + REG_TG_FRMSIZE_ST),
		readl_relaxed(raw_dev->base + REG_TG_FRMSIZE_ST_R),
		readl_relaxed(raw_dev->base + REG_TG_SEN_GRAB_PXL),
		readl_relaxed(raw_dev->base + REG_TG_SEN_GRAB_LIN));
}

void raw_irq_handle_dma_err(struct mtk_raw_device *raw_dev)
{
	dev_err_ratelimited(raw_dev->dev,
			    "IMGO:0x%x, RRZO:0x%x, AAO=0x%x, AFO=0x%x, LMVO=0x%x\n",
		readl_relaxed(raw_dev->base + REG_IMGO_ERR_STAT),
		readl_relaxed(raw_dev->base + REG_RRZO_ERR_STAT),
		readl_relaxed(raw_dev->base + REG_AAO_ERR_STAT),
		readl_relaxed(raw_dev->base + REG_AFO_ERR_STAT),
		readl_relaxed(raw_dev->base + REG_LMVO_ERR_STAT));
	dev_err_ratelimited(raw_dev->dev,
			    "LCSO=0x%x, PSO=0x%x, FLKO=0x%x, BPCI:0x%x, LSCI=0x%x\n",
		readl_relaxed(raw_dev->base + REG_LCSO_ERR_STAT),
		readl_relaxed(raw_dev->base + REG_FLKO_ERR_STAT),
		readl_relaxed(raw_dev->base + REG_BPCI_ERR_STAT),
		readl_relaxed(raw_dev->base + REG_LSCI_ERR_STAT));
}

#ifdef CONFIG_MTK_IOMMU_V2
static inline int m4u_control_iommu_port(unsigned int raw_id)
{
	struct M4U_PORT_STRUCT s_port;
	int port_begin;
	int use_m4u = 1;
	int ret = 0;
	int i = 0;

	switch (raw_id) {
	default:
	case RAW_A:
		port_begin = M4U_PORT_L16_CAM_IMGO_R1_A_MDP;
		break;
	case RAW_B:
		port_begin = M4U_PORT_L17_CAM_IMGO_R1_B_DISP;
		break;
	case RAW_C:
		port_begin = M4U_PORT_L18_CAM_IMGO_R1_C_MDP;
		break;
	}

	for (i = 0; i < PORT_SIZE; i++) {
		s_port.ePortID = port_begin + i;
		s_port.Virtuality = use_m4u;
		s_port.Security = 0;
		s_port.domain = 2;
		s_port.Distance = 1;
		s_port.Direction = 0;
		ret = m4u_config_port(&s_port);
		if (ret) {
			pr_warn("config M4U Port %s to %s FAIL(ret=%d)\n",
				iommu_get_port_name(port_begin + i),
				use_m4u ? "virtual" : "physical", ret);
			ret = -1;
		}
	}

	return ret;
}
#endif

static int mtk_raw_of_probe(struct platform_device *pdev,
			    struct mtk_raw_device *raw)
{
	struct device *dev = &pdev->dev;
	const struct raw_resource *raw_res;
	struct resource *res, *res_inner;
	unsigned int i;
	int irq, ret;

	ret = of_property_read_u32(dev->of_node, "mediatek,cam-id",
				   &raw->id);
	if (ret) {
		dev_err(dev, "missing camid property\n");
		return ret;
	}
	raw_res = raw_resources + raw->id;
	/* base outer register */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "failed to get mem\n");
		return -ENODEV;
	}

	raw->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(raw->base)) {
		dev_err(dev, "failed to map register base\n");
		return PTR_ERR(raw->base);
	}
	dev_dbg(dev, "raw, map_addr=0x%pK\n", raw->base);
	/* base inner register */
	res_inner = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res_inner) {
		dev_err(dev, "failed to get mem\n");
		return -ENODEV;
	}

	raw->base_inner = devm_ioremap_resource(dev, res_inner);
	if (IS_ERR(raw->base_inner)) {
		dev_err(dev, "failed to map register inner base\n");
		return PTR_ERR(raw->base_inner);
	}
	dev_dbg(dev, "raw, map_addr(inner)=0x%pK\n", raw->base_inner);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "failed to get irq\n");
		return -ENODEV;
	}

	ret = devm_request_irq(dev, irq, mtk_irq_raw, 0,
			       dev_name(dev), raw);
	if (ret) {
		dev_err(dev, "failed to request irq=%d\n", irq);
		return ret;
	}
	dev_dbg(dev, "registered irq=%d\n", irq);

	raw->num_clks = 0;
	while (raw_res->clock[raw->num_clks] && raw->num_clks < MAX_NUM_CLOCKS)
		raw->num_clks++;
	if (!raw->num_clks) {
		dev_err(dev, "no clock\n");
		return -ENODEV;
	}

	raw->clks = devm_kcalloc(dev, raw->num_clks, sizeof(*raw->clks),
				 GFP_KERNEL);
	if (!raw->clks)
		return -ENOMEM;

	for (i = 0; i < raw->num_clks; i++)
		raw->clks[i].id = raw_res->clock[i];

	ret = devm_clk_bulk_get(dev, raw->num_clks, raw->clks);
	if (ret) {
		dev_err(dev, "failed to get raw clock:%d\n", ret);
		return ret;
	}

	return 0;
}

static int mtk_raw_sd_subscribe_event(struct v4l2_subdev *subdev,
				      struct v4l2_fh *fh,
				      struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_FRAME_SYNC:
		return v4l2_event_subscribe(fh, sub, 0, NULL);
	case V4L2_EVENT_REQUEST_DRAINED:
		return v4l2_event_subscribe(fh, sub, 0, NULL);
	default:
		return -EINVAL;
	}
}

int mtk_cam_raw_select(struct mtk_raw_pipeline *pipe,
		       struct mtkcam_ipi_input_param *cfg_in_param)
{
	/**
	 * FIXME: calculate througput to assign raw dev and update hw map
	 * by config_param bin sepo
	 *
	 * check size, pixel bit, data pattern, tg crop, pixel mode
	 * master/slave cam selection
	 */
	pipe->enabled_raw = 1 << pipe->id;

	/**
	 * TODO: check duplicate use of same raw in different pipe
	 * for loop check all pipe with enabled_raw and logic check
	 */

	return 0;
}

static int mtk_raw_sd_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);
	struct mtk_raw *raw = pipe->raw;
	struct mtk_cam_device *cam = dev_get_drvdata(raw->cam_dev);
	struct mtk_cam_ctx *ctx = mtk_cam_find_ctx(cam, &sd->entity);
	unsigned int i;

	if (WARN_ON(!ctx))
		return -EINVAL;

	if (enable) {
		pipe->enabled_dmas = 0;
		ctx->pipe = pipe;

		for (i = 0; i < ARRAY_SIZE(pipe->vdev_nodes); i++) {
			if (!pipe->vdev_nodes[i].enabled)
				continue;
			pipe->enabled_dmas |= pipe->vdev_nodes[i].desc.dma_port;
		}

		if (ctx->used_raw_dmas != pipe->enabled_dmas) {
			dev_info(raw->cam_dev,
				 "ctx used raw DMA port != raw pipeline enabled DMA port\n");
			return -EINVAL;
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(raw->devs); i++) {
			if (pipe->enabled_raw & 1 << i)
				pm_runtime_put(raw->devs[i]);
		}

		pipe->enabled_raw = 0;
		pipe->enabled_dmas = 0;
	}

	dev_info(raw->cam_dev, "%s:raw-%d: en %d, dev 0x%x dmas 0x%x\n",
		 __func__, pipe->id, enable, pipe->enabled_raw,
		 pipe->enabled_dmas);

	return 0;
}

static int mtk_raw_init_cfg(struct v4l2_subdev *sd,
			    struct v4l2_subdev_pad_config *cfg)
{
	struct v4l2_mbus_framefmt *mf;
	unsigned int i;
	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);
	struct mtk_raw *raw = pipe->raw;

	for (i = 0; i < sd->entity.num_pads; i++) {
		mf = v4l2_subdev_get_try_format(sd, cfg, i);
		*mf = mfmt_default;
		pipe->cfg[i].mbus_fmt = mfmt_default;

		dev_dbg(raw->cam_dev, "%s init pad:%d format:0x%x\n",
			sd->name, i, mf->code);
	}

	return 0;
}

static int mtk_raw_try_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_format *fmt)
{
	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);
	struct mtk_raw *raw = pipe->raw;
	unsigned int sensor_fmt = mtk_cam_get_sensor_fmt(fmt->format.code);

	dev_dbg(raw->cam_dev, "%s try format 0x%x, w:%d, h:%d field:%d\n",
		sd->name, fmt->format.code, fmt->format.width,
		fmt->format.height, fmt->format.field);

	/* check sensor format */
	if (!sensor_fmt || fmt->pad == MTK_RAW_SINK) {
		return sensor_fmt;
	} else if (fmt->pad < MTK_RAW_PIPELINE_PADS_NUM) {
		/* check vdev node format */
		unsigned int img_fmt, i;
		struct mtk_cam_video_device *node =
			&pipe->vdev_nodes[fmt->pad - MTK_RAW_SINK_NUM];

		dev_dbg(raw->cam_dev, "node:%s num_fmts:%d",
			node->desc.name, node->desc.num_fmts);
		for (i = 0; i < node->desc.num_fmts; i++) {
			img_fmt = mtk_cam_get_img_fmt
				(node->desc.fmts[i].fmt.pix_mp.pixelformat);
			dev_dbg(raw->cam_dev,
				"try format sensor_fmt 0x%x img_fmt 0x%x",
				sensor_fmt, img_fmt);
			if (sensor_fmt == img_fmt)
				return img_fmt;
		}
	}

	return MTK_CAM_IMG_FMT_UNKNOWN;
}

static struct v4l2_mbus_framefmt *get_fmt(struct mtk_raw_pipeline *pipe,
					  struct v4l2_subdev_pad_config *cfg,
					   int padid, int which)
{
	/* format invalid and return default format */
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(&pipe->subdev, cfg, padid);

	if (WARN_ON(padid >= pipe->subdev.entity.num_pads))
		return &pipe->cfg[0].mbus_fmt;

	return &pipe->cfg[padid].mbus_fmt;
}

static void propagate_fmt(struct v4l2_mbus_framefmt *sink_mf,
			  struct v4l2_mbus_framefmt *source_mf, int w, int h)
{
	source_mf->code = sink_mf->code;
	source_mf->colorspace = sink_mf->colorspace;
	source_mf->field = sink_mf->field;
	source_mf->width = w;
	source_mf->height = h;
}

static int mtk_raw_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);
	struct mtk_raw *raw = pipe->raw;
	struct v4l2_mbus_framefmt *mf;

	if (!sd || !cfg) {
		dev_dbg(raw->cam_dev, "%s: Required sd(%p), cfg(%p)\n",
			__func__, sd, cfg);
		return -EINVAL;
	}

	if (!mtk_raw_try_fmt(sd, fmt)) {
		mf = get_fmt(pipe, cfg, fmt->pad, fmt->which);
		fmt->format = *mf;

	} else {
		mf = get_fmt(pipe, cfg, fmt->pad, fmt->which);
		*mf = fmt->format;
		dev_dbg(raw->cam_dev,
			"sd:%s pad:%d set format w/h/code %d/%d/0x%x\n",
			sd->name, fmt->pad, mf->width, mf->height, mf->code);
	}
	/*sink pad format propagate to source pad*/
	if (fmt->pad == MTK_RAW_SINK) {
		struct v4l2_mbus_framefmt *source_mf;
		bool res = false;
		int w, h;

		source_mf = get_fmt(pipe, cfg, MTK_RAW_MAIN_STREAM_OUT,
				    fmt->which);
		res = mtk_raw_resource_calc(pipe, mf->width, mf->height, &w,
					    &h);
		if (!res)
			return -EINVAL;

		propagate_fmt(mf, source_mf, w, h);
	}

	return 0;
}

static int mtk_raw_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);
	struct mtk_raw *raw = pipe->raw;
	struct v4l2_mbus_framefmt *mf;

	mf = get_fmt(pipe, cfg, fmt->pad, fmt->which);
	fmt->format = *mf;
	dev_dbg(raw->cam_dev, "sd:%s pad:%d get format w/h/code %d/%d/0x%x\n",
		sd->name, fmt->pad, fmt->format.width, fmt->format.height,
		fmt->format.code);

	return 0;
}

static int mtk_cam_update_sensor(struct mtk_cam_ctx *ctx,
				 struct media_entity *entity)
{
	struct media_graph *graph;
	struct v4l2_subdev **target_sd;

	graph = &ctx->pipeline.graph;
	media_graph_walk_start(graph, entity);

	while ((entity = media_graph_walk_next(graph))) {
		dev_dbg(ctx->cam->dev, "linked entity: %s\n", entity->name);

		target_sd = NULL;

		switch (entity->function) {
		case MEDIA_ENT_F_CAM_SENSOR:
			ctx->prev_sensor = ctx->sensor;
			target_sd = &ctx->sensor;
			*target_sd = media_entity_to_v4l2_subdev(entity);
			ctx->sensor->entity.stream_count++;
			ctx->sensor->entity.pipe = &ctx->pipeline;
			break;
		default:
			break;
		}

		if (!target_sd)
			continue;
	}

	return 0;
}

static int mtk_cam_media_link_setup(struct media_entity *entity,
				    const struct media_pad *local,
				    const struct media_pad *remote, u32 flags)
{
	struct mtk_raw_pipeline *pipe =
		container_of(entity, struct mtk_raw_pipeline, subdev.entity);
	struct mtk_raw *raw = pipe->raw;
	struct mtk_cam_device *cam = dev_get_drvdata(raw->cam_dev);
	struct mtk_cam_ctx *ctx = mtk_cam_find_ctx(cam, entity);
	u32 pad = local->index;

	dev_dbg(raw->cam_dev, "%s: raw %d: %d->%d flags:0x%x\n",
		__func__, pipe->id, remote->index, local->index, flags);

	if (pad < MTK_RAW_PIPELINE_PADS_NUM && pad != MTK_RAW_SINK)
		pipe->vdev_nodes[pad - MTK_RAW_SINK_NUM].enabled =
			!!(flags & MEDIA_LNK_FL_ENABLED);

	if (!(flags & MEDIA_LNK_FL_ENABLED))
		memset(pipe->cfg, 0, sizeof(pipe->cfg));
	if (pad == MTK_RAW_SINK && flags & MEDIA_LNK_FL_ENABLED)
		pipe->res_config.seninf =
			media_entity_to_v4l2_subdev(remote->entity);
	if (cam->streaming_ctx && ctx && pad == MTK_RAW_SINK) {
		if (flags & MEDIA_LNK_FL_ENABLED) {
			struct mtk_camsys_ctrl *camsys_ctrl =
				&ctx->cam->camsys_ctrl;
			u8 *state = &camsys_ctrl->link_change_state;
			unsigned long flags;

			spin_lock_irqsave(&camsys_ctrl->link_change_lock,
					  flags);
			if (*state == LINK_CHANGE_IDLE ||
			    *state == LINK_CHANGE_PREPARING) {
				struct mtk_camsys_link_ctrl *link_ctrl;
				struct media_entity *entity;

				link_ctrl =
					&camsys_ctrl->link_ctrl[ctx->stream_id];
				if (*state == LINK_CHANGE_IDLE)
					*state = LINK_CHANGE_PREPARING;

				link_ctrl->pipe = pipe;
				link_ctrl->remote = *remote;
				link_ctrl->active = 1;
				entity = remote->entity;

				if (entity->function ==
						MEDIA_ENT_F_VID_IF_BRIDGE) {
					ctx->seninf = (struct v4l2_subdev *)
						media_entity_to_v4l2_subdev
						(entity);
					ctx->seninf->entity.stream_count++;
					ctx->seninf->entity.pipe =
							&ctx->pipeline;
					mtk_cam_update_sensor(ctx, entity);
				}

				dev_dbg(raw->cam_dev, "link preparing:%d\n",
					ctx->stream_id);
			} else if (camsys_ctrl->link_change_state ==
							LINK_CHANGE_QUEUED) {
				dev_dbg(raw->cam_dev, "link queued");
				spin_unlock_irqrestore
					(&camsys_ctrl->link_change_lock, flags);
				return -EINVAL;
			}
			spin_unlock_irqrestore(&camsys_ctrl->link_change_lock,
					       flags);
		}
	}

	return 0;
}

static const struct v4l2_subdev_core_ops mtk_raw_subdev_core_ops = {
	.subscribe_event = mtk_raw_sd_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_video_ops mtk_raw_subdev_video_ops = {
	.s_stream =  mtk_raw_sd_s_stream,
};

static const struct v4l2_subdev_pad_ops mtk_raw_subdev_pad_ops = {
	.link_validate = v4l2_subdev_link_validate_default,
	.init_cfg = mtk_raw_init_cfg,
	.set_fmt = mtk_raw_set_fmt,
	.get_fmt = mtk_raw_get_fmt,
};

static const struct v4l2_subdev_ops mtk_raw_subdev_ops = {
	.core = &mtk_raw_subdev_core_ops,
	.video = &mtk_raw_subdev_video_ops,
	.pad = &mtk_raw_subdev_pad_ops,
};

static const struct media_entity_operations mtk_cam_media_entity_ops = {
	.link_setup = mtk_cam_media_link_setup,
	.link_validate = v4l2_subdev_link_validate,
};

static const struct v4l2_ioctl_ops mtk_cam_v4l2_vcap_ioctl_ops = {
	.vidioc_querycap = mtk_cam_vidioc_querycap,
	.vidioc_enum_framesizes = mtk_cam_vidioc_enum_framesizes,
	.vidioc_enum_fmt_vid_cap = mtk_cam_vidioc_enum_fmt,
	.vidioc_g_fmt_vid_cap_mplane = mtk_cam_vidioc_g_fmt,
	.vidioc_s_fmt_vid_cap_mplane = mtk_cam_vidioc_s_fmt,
	.vidioc_try_fmt_vid_cap_mplane = mtk_cam_vidioc_try_fmt,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_subscribe_event = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

static const struct v4l2_ioctl_ops mtk_cam_v4l2_meta_cap_ioctl_ops = {
	.vidioc_querycap = mtk_cam_vidioc_querycap,
	.vidioc_enum_fmt_meta_cap = mtk_cam_vidioc_meta_enum_fmt,
	.vidioc_g_fmt_meta_cap = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_s_fmt_meta_cap = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_try_fmt_meta_cap = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,
};

static const struct v4l2_ioctl_ops mtk_cam_v4l2_meta_out_ioctl_ops = {
	.vidioc_querycap = mtk_cam_vidioc_querycap,
	.vidioc_enum_fmt_meta_out = mtk_cam_vidioc_meta_enum_fmt,
	.vidioc_g_fmt_meta_out = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_s_fmt_meta_out = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_try_fmt_meta_out = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,
};

static const struct v4l2_format meta_fmts[] = { /* FIXME for ISP6 meta format */
	{
		.fmt.meta = {
			.dataformat = V4L2_META_FMT_MTISP_PARAMS,
			.buffersize = 512 * SZ_1K,
		},
	},
	{
		.fmt.meta = {
			.dataformat = V4L2_META_FMT_MTISP_3A,
			.buffersize = 1200 * SZ_1K,
		},
	},
	{
		.fmt.meta = {
			.dataformat = V4L2_META_FMT_MTISP_AF,
			.buffersize = 640 * SZ_1K,
		},
	},
	{
		.fmt.meta = {
			.dataformat = V4L2_META_FMT_MTISP_LCS,
			.buffersize = 288 * SZ_1K,
		},
	},
	{
		.fmt.meta = {
			.dataformat = V4L2_META_FMT_MTISP_LMV,
			.buffersize = 256,
		},
	},
};

static const struct v4l2_format stream_out_fmts[] = {
	/* This is a default image format */
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR10,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SBGGR8,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR12,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR14,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGBRG8,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG10,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG12,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG14,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGRBG8,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG10,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG12,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG14,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SRGGB8,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB10,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB12,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB14,
		},
	},
};

static const struct v4l2_format bin_out_fmts[] = {
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR8F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR10F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR12F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR14F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG8F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG10F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG12F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG14F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG8F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG10F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG12F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG14F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB8F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB10F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB12F,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB14F,
		},
	},
};

#define MTK_RAW_TOTAL_OUTPUT_QUEUES 1

static const struct
mtk_cam_dev_node_desc output_queues[] = {
	{
		.id = MTK_RAW_META_IN,
		.name = "meta input",
		.cap = V4L2_CAP_META_OUTPUT,
		.buf_type = V4L2_BUF_TYPE_META_OUTPUT,
		.link_flags = 0,
		.image = false,
#ifdef CONFIG_MTK_SCP
		.smem_alloc = true,
#else
		.smem_alloc = false,
#endif
		.fmts = meta_fmts,
		.default_fmt_idx = 0,
		.max_buf_count = 10,
		.ioctl_ops = &mtk_cam_v4l2_meta_out_ioctl_ops,
	},
};

static const char *
	output_queue_names[RAW_PIPELINE_NUM][MTK_RAW_TOTAL_OUTPUT_QUEUES] = {
	{"mtk-cam raw-0 meta-input"},
	{"mtk-cam raw-1 meta-input"},
	{"mtk-cam raw-2 meta-input"},
};

#define MTK_RAW_TOTAL_CAPTURE_QUEUES 2

static const struct
mtk_cam_dev_node_desc capture_queues[] = {
	{
		.id = MTK_RAW_MAIN_STREAM_OUT,
		.name = "main stream",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_IMMUTABLE | MEDIA_LNK_FL_ENABLED,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_IMGO,
		.fmts = stream_out_fmts,
		.num_fmts = ARRAY_SIZE(stream_out_fmts),
		.default_fmt_idx = 0,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = IMG_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = IMG_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_META_OUT,
		.name = "partial meta 0",
		.cap = V4L2_CAP_META_CAPTURE,
		.buf_type = V4L2_BUF_TYPE_META_CAPTURE,
		.link_flags = 0,
		.image = false,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_META_STATS_0,
		.fmts = meta_fmts,
		.default_fmt_idx = 1,
		.max_buf_count = 5,
		.ioctl_ops = &mtk_cam_v4l2_meta_cap_ioctl_ops,
	},
};

static const char *
	capture_queue_names[RAW_PIPELINE_NUM][MTK_RAW_TOTAL_CAPTURE_QUEUES] = {
	{"mtk-cam raw-0 main-stream", "mtk-cam raw-0 partial-meta-0"},
	{"mtk-cam raw-1 main-stream", "mtk-cam raw-1 partial-meta-0"},
	{"mtk-cam raw-2 main-stream", "mtk-cam raw-2 partial-meta-0"},
};

/* The helper to configure the device context */
static void mtk_raw_pipeline_queue_setup(struct mtk_raw_pipeline *pipe)
{
	unsigned int node_idx, i;

	if (WARN_ON(MTK_RAW_TOTAL_OUTPUT_QUEUES + MTK_RAW_TOTAL_CAPTURE_QUEUES
	    != MTK_RAW_TOTAL_NODES))
		return;

	node_idx = 0;
	/* Setup the output queue */
	for (i = 0; i < MTK_RAW_TOTAL_OUTPUT_QUEUES; i++) {
		pipe->vdev_nodes[node_idx].desc = output_queues[i];
		pipe->vdev_nodes[node_idx++].desc.name =
			output_queue_names[pipe->id][i];
	}

	/* Setup the capture queue */
	for (i = 0; i < MTK_RAW_TOTAL_CAPTURE_QUEUES; i++) {
		pipe->vdev_nodes[node_idx].desc = capture_queues[i];
		pipe->vdev_nodes[node_idx++].desc.name =
			capture_queue_names[pipe->id][i];
	}
}

static void mtk_raw_pipeline_ctrl_setup(struct mtk_raw_pipeline *pipe)
{
	struct v4l2_ctrl_handler *ctrl_hdlr;
	struct v4l2_ctrl *ctrl;
	struct device *dev = pipe->raw->devs[pipe->id];
	int ret = 0;

	ctrl_hdlr = &pipe->ctrl_handler;
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 4);
	if (ret) {
		dev_err(dev, "v4l2_ctrl_handler init failed\n");
		return;
	}
	mutex_init(&pipe->res_config.resource_lock);
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &hwn_limit, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			       V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;
	v4l2_ctrl_new_custom(ctrl_hdlr, &frz_limit, NULL);
	v4l2_ctrl_new_custom(ctrl_hdlr, &bin_limit, NULL);
	v4l2_ctrl_new_custom(ctrl_hdlr, &res_plan_policy, NULL);
	pipe->res_config.hwn_limit = hwn_limit.def;
	pipe->res_config.frz_limit = frz_limit.def;
	pipe->res_config.bin_limit = bin_limit.def;
	pipe->res_config.res_plan = res_plan_policy.def;
	pipe->subdev.ctrl_handler = ctrl_hdlr;
}

static int mtk_raw_pipeline_register(unsigned int id, struct device *dev,
				     struct mtk_raw_pipeline *pipe,
			       struct v4l2_device *v4l2_dev)
{
	struct mtk_cam_device *cam = dev_get_drvdata(pipe->raw->cam_dev);
	struct v4l2_subdev *sd = &pipe->subdev;
	struct mtk_cam_video_device *video;
	unsigned int i;
	int ret;

	pipe->id = id;

	/* Initialize subdev */
	v4l2_subdev_init(sd, &mtk_raw_subdev_ops);
	sd->entity.function = MEDIA_ENT_F_PROC_VIDEO_PIXEL_FORMATTER;
	sd->entity.ops = &mtk_cam_media_entity_ops;
	sd->flags = V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	snprintf(sd->name, sizeof(sd->name),
		 "%s-%d", dev_driver_string(dev), pipe->id);
	v4l2_set_subdevdata(sd, pipe);
	mtk_raw_pipeline_ctrl_setup(pipe);
	dev_info(dev, "%s: %s\n", __func__, sd->name);

	ret = v4l2_device_register_subdev(v4l2_dev, sd);
	if (ret < 0) {
		dev_info(dev, "Failed to register subdev: %d\n", ret);
		return ret;
	}

	mtk_raw_pipeline_queue_setup(pipe);
	/* setup pads of raw pipeline */
	for (i = 0; i < ARRAY_SIZE(pipe->pads); i++) {
		pipe->pads[i].flags = i < MTK_RAW_SOURCE_BEGIN ?
			MEDIA_PAD_FL_SINK : MEDIA_PAD_FL_SOURCE;
	}

	media_entity_pads_init(&sd->entity, ARRAY_SIZE(pipe->pads), pipe->pads);

	/* setup video node */
	for (i = 0; i < ARRAY_SIZE(pipe->vdev_nodes); i++) {
		video = pipe->vdev_nodes + i;

		switch (pipe->id) {
		case MTKCAM_SUBDEV_RAW_0:
		case MTKCAM_SUBDEV_RAW_1:
		case MTKCAM_SUBDEV_RAW_2:
			video->uid.pipe_id = pipe->id;
			break;
		default:
			dev_err(dev, "invalid pipe id\n");
			return -EINVAL;
		}

		video->uid.id = video->desc.dma_port;
		video->ctx = &cam->ctxs[id];
		ret = mtk_cam_video_register(video, v4l2_dev);
		if (ret)
			goto fail_unregister_video;

		if (V4L2_TYPE_IS_OUTPUT(video->desc.buf_type))
			ret = media_create_pad_link(&video->vdev.entity, 0,
						    &sd->entity,
						    video->desc.id,
						    video->desc.link_flags);
		else
			ret = media_create_pad_link(&sd->entity,
						    video->desc.id,
						    &video->vdev.entity, 0,
						    video->desc.link_flags);

		if (ret)
			goto fail_unregister_video;
	}

	return 0;

fail_unregister_video:
	for (i = i - 1; i >= 0; i--)
		mtk_cam_video_unregister(pipe->vdev_nodes + i);

	return ret;
}

static void mtk_raw_pipeline_unregister(struct mtk_raw_pipeline *pipe)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(pipe->vdev_nodes); i++)
		mtk_cam_video_unregister(pipe->vdev_nodes + i);
	v4l2_ctrl_handler_free(&pipe->ctrl_handler);
	mutex_destroy(&pipe->res_config.resource_lock);
	v4l2_device_unregister_subdev(&pipe->subdev);
	media_entity_cleanup(&pipe->subdev.entity);
}

int mtk_raw_register_entities(struct mtk_raw *raw, struct v4l2_device *v4l2_dev)
{
	unsigned int i;
	int ret;

	for (i = 0; i < RAW_PIPELINE_NUM; i++) {
		struct mtk_raw_pipeline *pipe = raw->pipelines + i;

		pipe->raw = raw;
		memset(pipe->cfg, 0, sizeof(*pipe->cfg));
		ret = mtk_raw_pipeline_register(MTKCAM_SUBDEV_RAW_0 + i,
						raw->devs[i],
						raw->pipelines + i, v4l2_dev);
		if (ret)
			return ret;
	}
	return 0;
}

void mtk_raw_unregister_entities(struct mtk_raw *raw)
{
	unsigned int i;

	for (i = 0; i < RAW_PIPELINE_NUM; i++)
		mtk_raw_pipeline_unregister(raw->pipelines + i);
}

static int mtk_raw_component_bind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_raw_device *raw_dev = dev_get_drvdata(dev);
	struct mtk_cam_device *cam_dev = data;
	struct mtk_raw *raw = &cam_dev->raw;

	dev_info(dev, "%s: id=%d\n", __func__, raw_dev->id);

	raw_dev->cam = cam_dev;
	raw->devs[raw_dev->id] = dev;
	raw->cam_dev = cam_dev->dev;

	return 0;
}

static void mtk_raw_component_unbind(struct device *dev, struct device *master,
				     void *data)
{
	struct mtk_raw_device *raw_dev = dev_get_drvdata(dev);
	struct mtk_cam_device *cam_dev = data;
	struct mtk_raw *raw = &cam_dev->raw;

	dev_info(dev, "%s\n", __func__);

	raw_dev->cam = NULL;
	raw_dev->pipeline = NULL;
	raw->devs[raw_dev->id] = NULL;
}

static const struct component_ops mtk_raw_component_ops = {
	.bind = mtk_raw_component_bind,
	.unbind = mtk_raw_component_unbind,
};

static int mtk_raw_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_raw_device *raw_dev;
	int ret;

	dev_info(dev, "%s\n", __func__);

	raw_dev = devm_kzalloc(dev, sizeof(*raw_dev), GFP_KERNEL);
	if (!raw_dev)
		return -ENOMEM;

	raw_dev->dev = dev;
	dev_set_drvdata(dev, raw_dev);

	spin_lock_init(&raw_dev->spinlock_irq);

	ret = mtk_raw_of_probe(pdev, raw_dev);
	if (ret)
		return ret;

	pm_runtime_set_autosuspend_delay(dev, 2 * MTK_RAW_STOP_HW_TIMEOUT);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_enable(dev);

	return component_add(dev, &mtk_raw_component_ops);
}

static int mtk_raw_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_info(dev, "%s\n", __func__);

	pm_runtime_disable(dev);

	component_del(dev, &mtk_raw_component_ops);
	return 0;
}

static int mtk_raw_pm_suspend(struct device *dev)
{
	struct mtk_raw_device *raw_dev = dev_get_drvdata(dev);
	u32 val;
	int ret;

	dev_dbg(dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	/* Disable ISP's view finder and wait for TG idle */
	dev_dbg(dev, "cam suspend, disable VF\n");
	val = readl(raw_dev->base + REG_TG_VF_CON);
	writel(val & (~TG_VF_CON_VFDATA_EN), raw_dev->base + REG_TG_VF_CON);
	ret = readl_poll_timeout_atomic(raw_dev->base + REG_TG_INTER_ST, val,
					(val & TG_CS_MASK) == TG_IDLE_ST,
					USEC_PER_MSEC, MTK_RAW_STOP_HW_TIMEOUT);
	if (ret)
		dev_warn(dev, "can't stop HW:%d:0x%x\n", ret, val);

	/* Disable CMOS */
	val = readl(raw_dev->base + REG_TG_SEN_MODE);
	writel(val & (~TG_SEN_MODE_CMOS_EN), raw_dev->base + REG_TG_SEN_MODE);

	/* Force ISP HW to idle */
	ret = pm_runtime_force_suspend(dev);
	return ret;
}

static int mtk_raw_pm_resume(struct device *dev)
{
	struct mtk_raw_device *raw_dev = dev_get_drvdata(dev);
	u32 val;
	int ret;

	dev_dbg(dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	/* Force ISP HW to resume */
	ret = pm_runtime_force_resume(dev);
	if (ret)
		return ret;

	/* Enable CMOS */
	dev_dbg(dev, "cam resume, enable CMOS/VF\n");
	val = readl(raw_dev->base + REG_TG_SEN_MODE);
	writel(val | TG_SEN_MODE_CMOS_EN, raw_dev->base + REG_TG_SEN_MODE);

	/* Enable VF */
	val = readl(raw_dev->base + REG_TG_VF_CON);
	writel(val | TG_VF_CON_VFDATA_EN, raw_dev->base + REG_TG_VF_CON);

	return 0;
}

static int mtk_raw_runtime_suspend(struct device *dev)
{
	struct mtk_raw_device *raw_dev = dev_get_drvdata(dev);
	int ret = 0;

	dev_dbg(dev, "%s:disable clock\n", __func__);
	clk_bulk_disable_unprepare(raw_dev->num_clks, raw_dev->clks);
	pm_runtime_put(raw_dev->cam->dev);

#ifdef CONFIG_MTK_SMI_EXT
	{
		const struct raw_port *raw_dev_port;
		int i;

		raw_dev_port = raw_ports + raw_dev->id;
		for (i = 0; i < PORT_SIZE; i++) {
			ret = smi_bus_disable_unprepare(raw_dev_port->smi_id,
							raw_dev_port->port[i]);
			if (ret != 0)
				dev_err(dev,
					"smi_bus_disable_unprepare:%d, %s\n",
					raw_dev_port->smi_id,
					raw_dev_port->port[i]);
		}
	}
#endif /* CONFIG_MTK_SMI_EXT */

	return ret;
}

static int mtk_raw_runtime_resume(struct device *dev)
{
	struct mtk_raw_device *raw_dev = dev_get_drvdata(dev);
	int ret;

	dev_dbg(dev, "%s:enable clock\n", __func__);
	pm_runtime_get_sync(raw_dev->cam->dev);
	ret = clk_bulk_prepare_enable(raw_dev->num_clks, raw_dev->clks);
	if (ret) {
		dev_err(dev, "failed to enable clock:%d\n", ret);
		return ret;
	}

#ifdef CONFIG_MTK_SMI_EXT
	{
		const struct raw_port *raw_dev_port;
		int i;

		raw_dev_port = raw_ports + raw_dev->id;
		for (i = 0; i < PORT_SIZE; i++) {
			ret = smi_bus_prepare_enable(raw_dev_port->smi_id,
						     raw_dev_port->port[i]);
			if (ret != 0)
				dev_err(dev, "smi_bus_prepare_enable:%d, %s\n",
					raw_dev_port->smi_id,
					raw_dev_port->port[i]);
		}
	}
#endif /* CONFIG_MTK_SMI_EXT */

#ifdef CONFIG_MTK_IOMMU_V2
	m4u_control_iommu_port(raw_dev->id);
#endif
	reset(raw_dev);

	return 0;
}

static const struct dev_pm_ops mtk_raw_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_raw_pm_suspend, mtk_raw_pm_resume)
	SET_RUNTIME_PM_OPS(mtk_raw_runtime_suspend, mtk_raw_runtime_resume,
			   NULL)
};

struct platform_driver mtk_cam_raw_driver = {
	.probe   = mtk_raw_probe,
	.remove  = mtk_raw_remove,
	.driver  = {
		.name  = "mtk-cam raw",
		.of_match_table = of_match_ptr(mtk_raw_of_ids),
		.pm     = &mtk_raw_pm_ops,
	}
};
