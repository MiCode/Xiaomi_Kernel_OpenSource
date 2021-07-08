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

#include <soc/mediatek/smi.h>

#include "mtk_cam.h"
#include "mtk_cam-raw.h"
#include "mtk_cam-regs.h"
#include "mtk_cam-v4l2.h"
#include "mtk_cam-video.h"
#include "mtk_cam-meta.h"
#include "mtk_cam-seninf-if.h"

#include "mtk_cam-dmadbg.h"
#include "mtk_cam-raw_debug.h"

extern struct clk *__clk_lookup(const char *name);

#define MTK_RAW_STOP_HW_TIMEOUT			(33)

#define MTK_CAMSYS_RES_IDXMASK		0xF0
#define MTK_CAMSYS_RES_BIN_TAG		0x10
#define MTK_CAMSYS_RES_FRZ_TAG		0x20
#define MTK_CAMSYS_RES_HWN_TAG		0x30
#define MTK_CAMSYS_RES_CLK_TAG		0x40

#define MTK_CAMSYS_RES_PLAN_NUM		10
#define TGO_MAX_PXLMODE		8
#define FRZ_PXLMODE_THRES		71
#define MHz		1000000
#define MTK_CAMSYS_PROC_DEFAULT_PIXELMODE 2

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
	E_RES_HWN2,
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


#define CAM_RAW_PROCESS_MAX_LINE_BUFFER		(6632)
#define CAM_RAW_FRZ_MAX_LINE_BUFFER		(6632)
#define CAM_RAW_BIN_MAX_LINE_BUFFER		(12000)
#define CAM_RAW_QBND_MAX_LINE_BUFFER		(16000)
#define CAM_RAW_CBN_MAX_LINE_BUFFER		(18472)
#define CAM_TWIN_PROCESS_MAX_LINE_BUFFER	(12400)

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
			E_RES_CLK3, E_RES_FRZ1, E_RES_BIN1, E_RES_HWN2} },
	[RESOURCE_STRATEGY_PQR] = {
		.cam_resource = {
			E_RES_BASIC, E_RES_HWN1, E_RES_HWN2, E_RES_FRZ1,
			E_RES_BIN1, E_RES_CLK1, E_RES_CLK2, E_RES_CLK3} },
	[RESOURCE_STRATEGY_RPQ] = {
		.cam_resource = {
			E_RES_BASIC, E_RES_FRZ1, E_RES_BIN1, E_RES_CLK1,
			E_RES_CLK2, E_RES_CLK3, E_RES_HWN1, E_RES_HWN2} },
	[RESOURCE_STRATEGY_QRP] = {
		.cam_resource = {
			E_RES_BASIC, E_RES_CLK1, E_RES_CLK2, E_RES_CLK3,
			E_RES_HWN1, E_RES_HWN2, E_RES_FRZ1, E_RES_BIN1} },
};

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
	struct mtk_raw_pipeline *pipeline;
	struct device *dev;
	int ret = 0;

	pipeline = mtk_cam_ctrl_handler_to_raw_pipeline(ctrl->handler);
	dev = pipeline->raw->devs[pipeline->id];

	if (ctrl->id >= V4L2_CID_MTK_CAM_USED_ENGINE_TRY &&
	    ctrl->id <= V4L2_CID_MTK_CAM_FRZ_TRY) {
		/**
		 * Read the determined resource for the "try" format
		 * negotiation result
		 */
		mutex_lock(&pipeline->try_res_config.resource_lock);
		switch (ctrl->id) {
		case V4L2_CID_MTK_CAM_USED_ENGINE_TRY:
			ctrl->val = pipeline->try_res_config.raw_num_used;
			break;
		case V4L2_CID_MTK_CAM_BIN_TRY:
			ctrl->val = pipeline->try_res_config.bin_enable;
			break;
		case V4L2_CID_MTK_CAM_FRZ_TRY:
			ctrl->val = pipeline->try_res_config.frz_enable ?
				    pipeline->try_res_config.frz_ratio : 100;
			break;
		default:
			dev_info(dev,
				 "%s:pipe(%d): unknown resource CID: %d\n",
				 __func__, pipeline->id, ctrl->id);
			break;
		}
		mutex_unlock(&pipeline->try_res_config.resource_lock);
	} else if (ctrl->id == V4L2_CID_MTK_CAM_SYNC_ID) {
		mutex_lock(&pipeline->res_config.resource_lock);
		*ctrl->p_new.p_s64 = pipeline->sync_id;
		mutex_unlock(&pipeline->try_res_config.resource_lock);
	} else {
		/**
		 * Read the determined resource for the "set" format
		 * negotiations result
		 */
		mutex_lock(&pipeline->res_config.resource_lock);
		switch (ctrl->id) {
		case V4L2_CID_MTK_CAM_USED_ENGINE:
			ctrl->val = pipeline->res_config.raw_num_used;
			break;
		case V4L2_CID_MTK_CAM_BIN:
			ctrl->val = pipeline->res_config.bin_enable;
			break;
		case V4L2_CID_MTK_CAM_FRZ:
			ctrl->val = pipeline->res_config.frz_enable ?
				    pipeline->res_config.frz_ratio : 100;
			break;
		default:
			dev_info(dev,
				 "%s:pipe(%d): unknown try resource CID: %d\n",
				 __func__, pipeline->id, ctrl->id);
			break;
		}
		mutex_unlock(&pipeline->res_config.resource_lock);

	}

	dev_dbg(dev, "%s:pipe(%d):id(%s) val(%d)\n",
		__func__, pipeline->id, ctrl->name, ctrl->val);
	return ret;
}

static int mtk_raw_set_res_ctrl(struct device *dev, struct v4l2_ctrl *ctrl,
				struct mtk_cam_resource_config *res_cfg,
				int pipe_id)
{
	int ret = 0;
	struct mtk_raw_pipeline *pipeline;

	pipeline = mtk_cam_ctrl_handler_to_raw_pipeline(ctrl->handler);

	if (ctrl->type == V4L2_CTRL_TYPE_INTEGER64)
		dev_dbg(dev, "%s:pipe(%d):(name:%s, val:%ld)\n", __func__,
			pipe_id, ctrl->name, *ctrl->p_new.p_s64);
	else
		dev_dbg(dev, "%s:pipe(%d):(name:%s, val:%d)\n", __func__,
			pipe_id, ctrl->name, ctrl->val);

	mutex_lock(&res_cfg->resource_lock);
	switch (ctrl->id) {
	case V4L2_CID_MTK_CAM_USED_ENGINE_LIMIT:
		res_cfg->hwn_limit = ctrl->val;
		break;
	case V4L2_CID_MTK_CAM_BIN_LIMIT:
		res_cfg->bin_limit = ctrl->val;
		break;
	case V4L2_CID_MTK_CAM_FRZ_LIMIT:
		res_cfg->frz_limit = ctrl->val;
		break;
	case V4L2_CID_MTK_CAM_RESOURCE_PLAN_POLICY:
		res_cfg->res_plan = ctrl->val;
		break;
	case V4L2_CID_MTK_CAM_RAW_PATH_SELECT:
		res_cfg->raw_path = ctrl->val;
		break;
	case V4L2_CID_HBLANK:
		res_cfg->hblank = ctrl->val;
		break;
	case V4L2_CID_VBLANK:
		res_cfg->vblank = ctrl->val;
		break;
	case V4L2_CID_MTK_CAM_PIXEL_RATE:
		res_cfg->sensor_pixel_rate = *ctrl->p_new.p_s64;
		break;
	case V4L2_CID_MTK_CAM_FEATURE:
		res_cfg->raw_feature = ctrl->val;
		break;
	case V4L2_CID_MTK_CAM_SYNC_ID:
		pipeline->sync_id = *ctrl->p_new.p_s64;
		break;
	default:
		dev_info(dev,
			 "%s:pipe(%d):ctrl(id:0x%x,val:%d) not handled\n",
			 __func__, pipe_id, ctrl->id, ctrl->val);
		ret = -EINVAL;
		break;
	}
	mutex_unlock(&res_cfg->resource_lock);

	return ret;
}

static int mtk_raw_try_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_raw_pipeline *pipeline;

	pipeline = mtk_cam_ctrl_handler_to_raw_pipeline(ctrl->handler);

	return mtk_raw_set_res_ctrl(pipeline->raw->devs[pipeline->id], ctrl,
				    &pipeline->try_res_config, pipeline->id);
}

static int mtk_raw_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_raw_pipeline *pipeline;

	pipeline = mtk_cam_ctrl_handler_to_raw_pipeline(ctrl->handler);

	return mtk_raw_set_res_ctrl(pipeline->raw->devs[pipeline->id], ctrl,
				    &pipeline->res_config, pipeline->id);
}

static const struct v4l2_ctrl_ops cam_ctrl_ops = {
	.g_volatile_ctrl = mtk_raw_get_ctrl,
	.s_ctrl = mtk_raw_set_ctrl,
	.try_ctrl = mtk_raw_try_ctrl,
};

static const struct v4l2_ctrl_config hwn_limit = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_USED_ENGINE_LIMIT,
	.name = "Engine resource limitation",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 1,
	.max = 2,
	.step = 1,
	.def = 2,
};

static const struct v4l2_ctrl_config bin_limit = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_BIN_LIMIT,
	.name = "Binning limitation",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.min = 0,
	.max = 1,
	.step = 1,
	.def = 1,
};

static const struct v4l2_ctrl_config frz_limit = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_FRZ_LIMIT,
	.name = "Resizer limitation",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 70,
	.max = 100,
	.step = 1,
	.def = 100,
};

static const struct v4l2_ctrl_config hwn = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_USED_ENGINE,
	.name = "Engine resource",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 1,
	.max = 2,
	.step = 1,
	.def = 2,
};

static const struct v4l2_ctrl_config bin = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_BIN,
	.name = "Binning",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.min = 0,
	.max = 1,
	.step = 1,
	.def = 1,
};

static const struct v4l2_ctrl_config frz = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_FRZ,
	.name = "Resizer",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 70,
	.max = 100,
	.step = 1,
	.def = 100,
};
static const struct v4l2_ctrl_config raw_path = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_RAW_PATH_SELECT,
	.name = "Raw Path",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 1,
	.step = 1,
	.def = 1,
};

static const struct v4l2_ctrl_config hwn_try = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_USED_ENGINE_TRY,
	.name = "Engine resource",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 1,
	.max = 2,
	.step = 1,
	.def = 2,
};

static const struct v4l2_ctrl_config bin_try = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_BIN_TRY,
	.name = "Binning",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.min = 0,
	.max = 1,
	.step = 1,
	.def = 1,
};

static const struct v4l2_ctrl_config frz_try = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_FRZ_TRY,
	.name = "Resizer",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 70,
	.max = 100,
	.step = 1,
	.def = 100,
};
static const struct v4l2_ctrl_config res_plan_policy = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_RESOURCE_PLAN_POLICY,
	.name = "Resource planning policy",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 10,
	.step = 1,
	.def = 1,
};

static const struct v4l2_ctrl_config res_pixel_rate = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_PIXEL_RATE,
	.name = "Resource pixel rate",
	.type = V4L2_CTRL_TYPE_INTEGER64,
	.min = 0,
	.max = 0xFFFFFFFF,
	.step = 1,
	.def = 0,
};
static const struct v4l2_ctrl_config frame_sync_id = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_SYNC_ID,
	.name = "Frame sync id",
	.type = V4L2_CTRL_TYPE_INTEGER64,
	.min = -1,
	.max = 0x7FFFFFFF,
	.step = 1,
	.def = -1,
};

static const struct v4l2_ctrl_config mtk_feature = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_FEATURE,
	.name = "Mediatek camsys feature",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = RAW_FUNCTION_END,
	.step = 1,
	.def = 0,
};

void trigger_rawi(struct mtk_raw_device *dev)
{
#define TRIGGER_RAWI_R6 0x10

	writel_relaxed(TRIGGER_RAWI_R6, dev->base + REG_CTL_RAWI_TRIG);
	wmb(); /* TBC */
}

void apply_cq(struct mtk_raw_device *dev,
	      dma_addr_t cq_addr, unsigned int cq_size, unsigned int cq_offset,
	      int initial, unsigned int sub_cq_size, unsigned int sub_cq_offset)
{
#define DUMMY_CQ_SUB_DESC_DIZE 16

	dev_dbg(dev->dev,
		"apply raw%d cq - addr:0x%08x ,size:%d/%d,offset:%d, REG_CQ_THR0_CTL:0x%8x\n",
		dev->id, cq_addr, cq_size, sub_cq_size, sub_cq_offset,
		readl_relaxed(dev->base + REG_CQ_THR0_CTL));
	writel_relaxed(cq_addr + cq_offset, dev->base + REG_CQ_THR0_BASEADDR);

	writel_relaxed(cq_size, dev->base + REG_CQ_THR0_DESC_SIZE);

	writel_relaxed(cq_addr + sub_cq_offset,
		   dev->base + REG_CQ_SUB_THR0_BASEADDR_2);
	writel_relaxed(sub_cq_size,
			   dev->base + REG_CQ_SUB_THR0_DESC_SIZE_2);

	wmb(); /* TBC */
	if (initial) {
		writel_relaxed(CAMCTL_CQ_THR0_DONE_ST,
			       dev->base + REG_CTL_RAW_INT6_EN);
		writel_relaxed(BIT(10),
			       dev->base + REG_CTL_RAW_INT7_EN);
		writel_relaxed(CTL_CQ_THR0_START,
			       dev->base + REG_CTL_START);
		wmb(); /* TBC */
	} else {
#if USINGSCQ
		writel_relaxed(CTL_CQ_THR0_START, dev->base + REG_CTL_START);
		wmb(); /* TBC */
#endif
	}
	dev_dbg(dev->dev,
		"apply raw%d scq - addr/size = [main] 0x%x/%d [sub] 0x%x/%d\n",
		dev->id, cq_addr, cq_size, cq_addr + sub_cq_offset, sub_cq_size);

}

void reset(struct mtk_raw_device *dev)
{
	unsigned long end = jiffies + msecs_to_jiffies(100);

	dev_dbg(dev->dev, "%s\n", __func__);

	/* Disable all DMA DCM before reset */
	wmb(); /* TBC */
	writel_relaxed(0x00000fff, dev->base + REG_CTL_RAW_MOD5_DCM_DIS);
	writel_relaxed(0x0007ffff, dev->base + REG_CTL_RAW_MOD6_DCM_DIS);
	writel_relaxed(0xffffffff, dev->yuv_base + REG_CTL_RAW_MOD5_DCM_DIS);

	//writel_relaxed(0, dev->base + REG_CTL_SW_CTL);
	writel(1, dev->base + REG_CTL_SW_CTL);

	while (time_before(jiffies, end)) {
		if (readl(dev->base + REG_CTL_SW_CTL) & 0x2) {
			/* do hw rst */
			writel_relaxed(4, dev->base + REG_CTL_SW_CTL);
			wmb(); /* TBC */
			writel_relaxed(0, dev->base + REG_CTL_SW_CTL);
			wmb(); /* TBC */
			/* Enable all DMA DCM after reset */
			writel_relaxed(0x0, dev->base + REG_CTL_RAW_MOD5_DCM_DIS);
			writel_relaxed(0x0, dev->base + REG_CTL_RAW_MOD6_DCM_DIS);
			writel_relaxed(0x0, dev->yuv_base + REG_CTL_RAW_MOD5_DCM_DIS);
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

	/* Enable all DMA DCM after reset fail */
	wmb(); /* TBC */
	writel_relaxed(0x0, dev->base + REG_CTL_RAW_MOD5_DCM_DIS);
	writel_relaxed(0x0, dev->base + REG_CTL_RAW_MOD6_DCM_DIS);
	writel_relaxed(0x0, dev->yuv_base + REG_CTL_RAW_MOD5_DCM_DIS);

	dev_dbg(dev->dev, "%s: hw timeout\n", __func__);
}

#define FIFO_THRESHOLD(FIFO_SIZE, HEIGHT_RATIO, LOW_RATIO) \
	(((FIFO_SIZE * HEIGHT_RATIO) & 0xFFF) << 16 | \
	((FIFO_SIZE * LOW_RATIO) & 0xFFF))

void set_fifo_threshold(void __iomem *dma_base)
{
	int fifo_size = 0;

	fifo_size = readl_relaxed(dma_base + DMA_OFFSET_CON0) & 0xFFF;
	writel_relaxed((0x1 << 28) | FIFO_THRESHOLD(fifo_size, 1/4, 1/8),
			dma_base + DMA_OFFSET_CON1);
	writel_relaxed((0x1 << 28) | FIFO_THRESHOLD(fifo_size, 1/2, 3/8),
			dma_base + DMA_OFFSET_CON2);
	writel_relaxed((0x1 << 31) | FIFO_THRESHOLD(fifo_size, 2/3, 13/24),
			dma_base + DMA_OFFSET_CON3);
	writel_relaxed((0x1 << 31) | FIFO_THRESHOLD(fifo_size, 3/8, 1/4),
			dma_base + DMA_OFFSET_CON4);
}
void toggle_db(struct mtk_raw_device *dev)
{
	int value;
	u32 val_cfg, val_dcif_ctl, val_sen;

	value = readl_relaxed(dev->base + REG_CTL_MISC);
	val_cfg = readl_relaxed(dev->base_inner + REG_TG_PATH_CFG);
	val_dcif_ctl = readl_relaxed(dev->base_inner + REG_TG_DCIF_CTL);
	val_sen = readl_relaxed(dev->base_inner + REG_TG_SEN_MODE);
	writel_relaxed(value & ~CTL_DB_EN, dev->base + REG_CTL_MISC);
	wmb(); /* TBC */
	writel_relaxed(value | CTL_DB_EN, dev->base + REG_CTL_MISC);
	wmb(); /* TBC */
	dev_dbg(dev->dev, "%s,  read inner AsIs->ToBe TG_PATH_CFG:0x%x->0x%x, TG_DCIF:0x%x->0x%x, TG_SEN:0x%x->0x%x\n",
		__func__,
			val_cfg,
			readl_relaxed(dev->base_inner + REG_TG_PATH_CFG),
			val_dcif_ctl,
			readl_relaxed(dev->base_inner + REG_TG_DCIF_CTL),
			val_sen,
			readl_relaxed(dev->base_inner + REG_TG_SEN_MODE));
}

void enable_tg_db(struct mtk_raw_device *dev, int en)
{
	int value;
	u32 val_cfg, val_dcif_ctl, val_sen;

	value = readl_relaxed(dev->base_inner + REG_TG_PATH_CFG);
	if (en == 0) { /*disable tg db*/
		val_cfg = readl_relaxed(dev->base_inner + REG_TG_PATH_CFG);
		val_dcif_ctl = readl_relaxed(dev->base_inner + REG_TG_DCIF_CTL);
		val_sen = readl_relaxed(dev->base_inner + REG_TG_SEN_MODE);
		writel_relaxed(value | TG_DB_LOAD_DIS, dev->base + REG_TG_PATH_CFG);
		wmb(); /* TBC */
		dev_dbg(dev->dev, "%s disable,  TG_PATH_CFG:0x%x->0x%x, TG_DCIF:0x%x->0x%x, TG_SEN:0x%x->0x%x\n",
		__func__,
			val_cfg,
			readl_relaxed(dev->base_inner + REG_TG_PATH_CFG),
			val_dcif_ctl,
			readl_relaxed(dev->base_inner + REG_TG_DCIF_CTL),
			val_sen,
			readl_relaxed(dev->base_inner + REG_TG_SEN_MODE));
	} else { /*enable tg db*/
		writel_relaxed(value & ~TG_DB_LOAD_DIS, dev->base + REG_TG_PATH_CFG);
		wmb(); /* TBC */
		dev_dbg(dev->dev, "%s enable, TG_PATH_CFG:0x%x\n", __func__, value);
	}
}


static void init_dma_threshold(struct mtk_raw_device *dev)
{
	struct mtk_cam_device *cam_dev;

	cam_dev = dev->cam;

	set_fifo_threshold(dev->base + REG_IMGO_R1_BASE);
	set_fifo_threshold(dev->base + REG_FHO_R1_BASE);
	set_fifo_threshold(dev->base + REG_AAHO_R1_BASE);
	set_fifo_threshold(dev->base + REG_PDO_R1_BASE);
	set_fifo_threshold(dev->base + REG_AAO_R1_BASE);
	set_fifo_threshold(dev->base + REG_AFO_R1_BASE);

	set_fifo_threshold(dev->yuv_base + REG_YUVO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVBO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVCO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVO_R3_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVBO_R3_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVCO_R3_BASE);
	set_fifo_threshold(dev->yuv_base + REG_LTMSO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_TSFSO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_TSFSO_R2_BASE);
	set_fifo_threshold(dev->yuv_base + REG_FLKO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_UFEO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVO_R2_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVBO_R2_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVO_R4_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVBO_R4_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVO_R5_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVBO_R5_BASE);
	set_fifo_threshold(dev->yuv_base + REG_RZH1N2TO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_RZH1N2TBO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_RZH1N2TO_R2_BASE);
	set_fifo_threshold(dev->yuv_base + REG_RZH1N2TO_R3_BASE);
	set_fifo_threshold(dev->yuv_base + REG_RZH1N2TBO_R3_BASE);
	set_fifo_threshold(dev->yuv_base + REG_DRZS4NO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_DRZS4NO_R2_BASE);
	set_fifo_threshold(dev->yuv_base + REG_DRZS4NO_R3_BASE);
	set_fifo_threshold(dev->yuv_base + REG_ACTSO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_TNCSO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_TNCSBO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_TNCSHO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_TNCSYO_R1_BASE);

	set_fifo_threshold(dev->base + REG_RAWI_R2_BASE);
	set_fifo_threshold(dev->base + REG_UFDI_R2_BASE);
	set_fifo_threshold(dev->base + REG_RAWI_R3_BASE);
	set_fifo_threshold(dev->base + REG_UFDI_R3_BASE);
	set_fifo_threshold(dev->base + REG_CQI_R1_BASE);
	set_fifo_threshold(dev->base + REG_CQI_R2_BASE);
	set_fifo_threshold(dev->base + REG_CQI_R3_BASE);
	set_fifo_threshold(dev->base + REG_CQI_R4_BASE);
	set_fifo_threshold(dev->base + REG_LSCI_R1_BASE);
	set_fifo_threshold(dev->base + REG_BPCI_R1_BASE);
	set_fifo_threshold(dev->base + REG_BPCI_R2_BASE);
	set_fifo_threshold(dev->base + REG_BPCI_R3_BASE);
	set_fifo_threshold(dev->base + REG_PDI_R1_BASE);
	set_fifo_threshold(dev->base + REG_AAI_R1_BASE);
	set_fifo_threshold(dev->base + REG_CACI_R1_BASE);
	set_fifo_threshold(dev->base + REG_RAWI_R6_BASE);

	writel_relaxed(0x1, cam_dev->base + REG_HALT1_EN);
	writel_relaxed(0x1, cam_dev->base + REG_HALT2_EN);
	writel_relaxed(0x1, cam_dev->base + REG_HALT3_EN);
	writel_relaxed(0x1, cam_dev->base + REG_HALT4_EN);
	writel_relaxed(0x1, cam_dev->base + REG_HALT5_EN);
	writel_relaxed(0x1, cam_dev->base + REG_HALT6_EN);
	writel_relaxed(0x1, cam_dev->base + REG_ULTRA_HALT1_EN);
	writel_relaxed(0x1, cam_dev->base + REG_ULTRA_HALT2_EN);
	writel_relaxed(0x1, cam_dev->base + REG_ULTRA_HALT3_EN);
	writel_relaxed(0x1, cam_dev->base + REG_ULTRA_HALT4_EN);
	writel_relaxed(0x1, cam_dev->base + REG_ULTRA_HALT5_EN);
	writel_relaxed(0x1, cam_dev->base + REG_ULTRA_HALT6_EN);
	writel_relaxed(0x1, cam_dev->base + REG_PREULTRA_HALT1_EN);
	writel_relaxed(0x1, cam_dev->base + REG_PREULTRA_HALT2_EN);
	writel_relaxed(0x1, cam_dev->base + REG_PREULTRA_HALT3_EN);
	writel_relaxed(0x1, cam_dev->base + REG_PREULTRA_HALT4_EN);
	writel_relaxed(0x1, cam_dev->base + REG_PREULTRA_HALT5_EN);
	writel_relaxed(0x1, cam_dev->base + REG_PREULTRA_HALT6_EN);

}
void write_readcount(struct mtk_raw_device *dev)
{
	int val, val2;

	/* raw-domain */
	val = readl_relaxed(dev->base_inner + REG_CTL_RAW_DMA_EN);
	writel_relaxed(val, dev->base + REG_CAMCTL_FBC_RCNT_INC);

	/* yuv-domain */
	val2 = readl_relaxed(dev->base_inner + REG_CTL_YUV_DMA_EN);
	writel_relaxed(val2, dev->yuv_base + REG_CAMCTL_FBC_RCNT_INC);
	wmb(); /* assure order */

	dev_info(dev->dev, "[Write Rcnt] val:0x%x, 0x%x\n", val, val2);
}

void set_cam_clk(int freq)
{
	struct clk *clk;
	struct clk *parent;

	switch (freq) {
	case 273:
		parent = __clk_lookup("mainpll_d4_d2");
		break;
	case 312:
		parent = __clk_lookup("univpll_d4_d2");
		break;
	case 416:
		parent = __clk_lookup("univpll_d6");
		break;
	case 499:
		parent = __clk_lookup("univpll_d5");
		break;
	case 546:
		parent = __clk_lookup("mainpll_d4");
		break;
	case 624:
		parent = __clk_lookup("univpll_d4");
		break;
	case 688:
		parent = __clk_lookup("mmpll_d4");
		break;
	default:
		parent = __clk_lookup("mainpll_d4_d2");
		break;
	}

	clk = __clk_lookup("cam_sel");
	clk_set_parent(clk, parent);
}

int get_fps_ratio(struct mtk_raw_device *dev)
{
	int fps, fps_ratio;

	fps = dev->pipeline->res_config.interval.denominator /
			dev->pipeline->res_config.interval.numerator;
	if (fps <= 30)
		fps_ratio = 1;
	else if (fps <= 60)
		fps_ratio = 2;
	else
		fps_ratio = 1;

	return fps_ratio;
}

int mtk_cam_get_subsample_ratio(int raw_feature)
{
	int sub_ratio = 0;
	int sub_value = raw_feature & MTK_CAM_FEATURE_SUBSAMPLE_MASK;

	switch (sub_value) {
	case HIGHFPS_2_SUBSAMPLE:
		sub_ratio = 1;
		break;
	case HIGHFPS_4_SUBSAMPLE:
		sub_ratio = 3;
		break;
	case HIGHFPS_8_SUBSAMPLE:
		sub_ratio = 7;
		break;
	case HIGHFPS_16_SUBSAMPLE:
		sub_ratio = 15;
		break;
	case HIGHFPS_32_SUBSAMPLE:
		sub_ratio = 31;
		break;
	default:
		sub_ratio = 0;
	}

	return sub_ratio;
}

void stagger_enable(struct mtk_raw_device *dev)
{
	u32 val;

	val = readl_relaxed(dev->base + REG_CQ_EN);
	writel_relaxed(val | SCQ_STAGGER_MODE, dev->base + REG_CQ_EN);
	wmb(); /* TBC */
	dev->stagger_en = 1;
	dev_dbg(dev->dev, "%s - CQ_EN:0x%x->0x%x, TG_PATH_CFG:0x%x, TG_DCIF:0x%x, TG_SEN:0x%x\n",
		__func__, val,
			readl_relaxed(dev->base + REG_CQ_EN),
			readl_relaxed(dev->base + REG_TG_PATH_CFG),
			readl_relaxed(dev->base + REG_TG_DCIF_CTL),
			readl_relaxed(dev->base + REG_TG_SEN_MODE));

}

void stagger_disable(struct mtk_raw_device *dev)
{
	u32 val;

	val = readl_relaxed(dev->base + REG_CQ_EN);
	writel_relaxed(val & (~SCQ_STAGGER_MODE), dev->base + REG_CQ_EN);
	wmb(); /* TBC */
	dev->stagger_en = 0;
	dev_dbg(dev->dev, "%s - CQ_EN:0x%x->0x%x, TG_PATH_CFG:0x%x, TG_DCIF:0x%x, TG_SEN:0x%x\n",
		__func__,
			val,
			readl_relaxed(dev->base + REG_CQ_EN),
			readl_relaxed(dev->base + REG_TG_PATH_CFG),
			readl_relaxed(dev->base + REG_TG_DCIF_CTL),
			readl_relaxed(dev->base + REG_TG_SEN_MODE));
}

void subsample_enable(struct mtk_raw_device *dev)
{
	u32 val;
	u32 sub_ratio = mtk_cam_get_subsample_ratio(
			dev->pipeline->res_config.raw_feature);

	val = readl_relaxed(dev->base + REG_CQ_EN);
	writel_relaxed(val | SCQ_SUBSAMPLE_EN, dev->base + REG_CQ_EN);
	writel_relaxed(SW_DONE_SAMPLE_EN | sub_ratio,
		dev->base + REG_CTL_SW_PASS1_DONE);
	writel_relaxed(SW_DONE_SAMPLE_EN | sub_ratio,
		dev->base_inner + REG_CTL_SW_PASS1_DONE);
	wmb(); /* TBC */
	dev_dbg(dev->dev, "%s - REG_CQ_EN:0x%x ,REG_CTL_SW_PASS1_DONE:0x%8x\n",
			__func__,
			readl_relaxed(dev->base + REG_CQ_EN),
			readl_relaxed(dev->base + REG_CTL_SW_PASS1_DONE));
}

void initialize(struct mtk_raw_device *dev)
{
#if USINGSCQ
	u32 val;

	val = readl_relaxed(dev->base + REG_CQ_EN);
	writel_relaxed(val | SCQ_EN, dev->base + REG_CQ_EN);

	//writel_relaxed(0x100010, dev->base + REG_CQ_EN);
	writel_relaxed(0xffffffff, dev->base + REG_SCQ_START_PERIOD);
	wmb(); /* TBC */
#endif
	writel_relaxed(CQ_THR0_MODE_IMMEDIATE | CQ_THR0_EN,
		       dev->base + REG_CQ_THR0_CTL);
	writel_relaxed(CQ_THR0_MODE_IMMEDIATE | CQ_THR0_EN,
		       dev->base + REG_CQ_SUB_THR0_CTL);
	writel_relaxed(CAMCTL_CQ_THR0_DONE_ST,
		       dev->base + REG_CTL_RAW_INT6_EN);
	wmb(); /* TBC */
	dev->sof_count = 0;
	dev->setting_count = 0;
	dev->time_shared_busy = 0;
	dev->vf_en = 0;
	dev->stagger_en = 0;
	init_dma_threshold(dev);

	/* Workaround: disable FLKO error_sof: double sof error
	 *   HW will send FLKO dma error when
	 *      FLKO rcnt = 0 (not going to output this frame)
	 *      However, HW_PASS1_DONE still comes as expected
	 */
	writel_relaxed(0xFFFE0000,
		       dev->base + REG_FLKO_R1_BASE + DMA_OFFSET_ERR_STAT);

	dev_dbg(dev->dev, "%s - REG_CQ_EN:0x%x ,REG_CQ_THR0_CTL:0x%8x\n",
		__func__,
			readl_relaxed(dev->base + REG_CQ_EN),
			readl_relaxed(dev->base + REG_CQ_THR0_CTL));

}

void stream_on(struct mtk_raw_device *dev, int on)
{
	u32 val;
	u32 chk_val;
	u32 cfg_val;
	unsigned long end;
	u32 fps_ratio = 1;

	dev_dbg(dev->dev, "raw %d %s %d\n", dev->id, __func__, on);

	if (on) {
#if USINGSCQ
		val = readl_relaxed(dev->base + REG_TG_TIME_STAMP_CNT);
		fps_ratio = get_fps_ratio(dev);
		dev_info(dev->dev, "REG_TG_TIME_STAMP_CNT val:%d fps(30x):%d\n", val, fps_ratio);

		writel_relaxed(SCQ_DEADLINE_MS * 1000 * SCQ_DEFAULT_CLK_RATE /
			(val * 2) / fps_ratio, dev->base + REG_SCQ_START_PERIOD);
#else
		writel_relaxed(CQ_THR0_MODE_CONTINUOUS | CQ_THR0_EN,
			       dev->base + REG_CQ_THR0_CTL);
		writel_relaxed(CQ_DB_EN | CQ_DB_LOAD_MODE,
			       dev->base + REG_CQ_EN);
		wmb(); /* TBC */
#endif
		/*
		 * mtk_cam_set_topdebug_rdyreq(dev->dev, dev->base, dev->yuv_base,
		 *                             TG_OVERRUN);
		 */
		if (dev->pipeline->res_config.raw_feature & MTK_CAM_FEATURE_TIMESHARE_MASK ||
			dev->pipeline->res_config.raw_feature & MTK_CAM_FEATURE_STAGGER_M2M_MASK) {
			dev_info(dev->dev, "[%s] M2M view finder disable\n", __func__);
		} else {
			val = readl_relaxed(dev->base + REG_TG_VF_CON);
			val |= TG_VFDATA_EN;
			writel_relaxed(val, dev->base + REG_TG_VF_CON);
			wmb(); /* TBC */
		}
		dev->vf_en = 1;
		dev_dbg(dev->dev,
			"%s - REG_CQ_EN:0x%x, REG_CQ_THR0_CTL:0x%8x, REG_TG_VF_CON:0x%8x\n",
			__func__,
			readl_relaxed(dev->base + REG_CQ_EN),
			readl_relaxed(dev->base + REG_CQ_THR0_CTL),
			readl_relaxed(dev->base + REG_TG_VF_CON));
	} else {
		writel_relaxed(~CQ_THR0_EN, dev->base + REG_CQ_THR0_CTL);
		wmb(); /* TBC */

		cfg_val = readl_relaxed(dev->base + REG_TG_PATH_CFG);
		cfg_val |= 0x100;
		writel(cfg_val, dev->base + REG_TG_PATH_CFG);

		val = readl_relaxed(dev->base + REG_TG_VF_CON);
		val &= ~TG_VFDATA_EN;
		writel(val, dev->base + REG_TG_VF_CON);

		cfg_val = readl_relaxed(dev->base + REG_TG_PATH_CFG);
		cfg_val &= ~0x100;
		writel(cfg_val, dev->base + REG_TG_PATH_CFG);

		//writel_relaxed(val, dev->base_inner + REG_CTL_EN);
		//writel_relaxed(val, dev->base_inner + REG_CTL_EN2);
		wmb(); /* TBC */
		/* reset hw after vf off */
		end = jiffies + msecs_to_jiffies(10);
		while (time_before(jiffies, end)) {
			chk_val = readl_relaxed(dev->base + REG_TG_VF_CON);
			if (chk_val == val) {
				//writel_relaxed(0x0, dev->base + REG_TG_SEN_MODE);
				//wmb(); /* TBC */
				reset(dev);
				break;
			}
			usleep_range(10, 20);
		}
		dev->vf_en = 0;
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

static int mtk_raw_pixelmode_calc(int rawpxl, int b_twin, bool b_bin,
				  bool b_frz, int min_ratio)
{
	int pixelmode = rawpxl;

	pixelmode = (b_twin == 2) ? pixelmode << 2 : pixelmode;
	pixelmode = (b_twin == 1) ? pixelmode << 1 : pixelmode;
	pixelmode = b_bin ? pixelmode << 2 : pixelmode;
	pixelmode = (b_frz && (min_ratio < FRZ_PXLMODE_THRES))
			? pixelmode << 1 : pixelmode;
	pixelmode = (pixelmode > TGO_MAX_PXLMODE) ?
		TGO_MAX_PXLMODE : pixelmode;

	return pixelmode;
}

static bool mtk_raw_resource_calc(struct mtk_cam_device *cam,
				  struct mtk_cam_resource_config *res,
				  s64 pixel_rate, int res_plan,
				  int in_w, int in_h, int *out_w, int *out_h)
{
	struct mtk_camsys_dvfs *clk = &cam->camsys_ctrl.dvfs_info;
	u64 eq_throughput = clk->clklv[0];
	int res_step_type = 0;
	int tgo_pxl_mode = 1;
	int pixel_mode[MTK_CAMSYS_RES_STEP_NUM] = {0};
	int bin_temp = 0, frz_temp = 0, hwn_temp = 0;
	int bin_en = 0, frz_en = 0, twin_en = 0, clk_cur = 0;
	int idx = 0, clk_res = 0, idx_res = 0;
	bool res_found = false;
	int lb_chk_res = -1;
	int frz_ratio = 100;
	int p;

	res->res_plan = res_plan;
	res->pixel_rate = pixel_rate;
	/* test pattern */
	if (res->pixel_rate == 0)
		res->pixel_rate = 450 * MHz;
	if (res->raw_feature & MTK_CAM_FEATURE_TIMESHARE_MASK)
		res->hwn_limit = 1;
	dev_dbg(cam->dev,
		"[Res] PR = %lld, w/h=%d/%d HWN(%d)/BIN(%d)/FRZ(%d),Plan:%d\n",
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
		lb_chk_res = mtk_raw_linebuf_chk(twin_en, bin_en, frz_en, 0, 0,
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
		if (res->raw_feature & MTK_CAM_FEATURE_TIMESHARE_MASK) {
			tgo_pxl_mode = mtk_raw_pixelmode_calc(MTK_CAMSYS_PROC_DEFAULT_PIXELMODE,
					twin_en, bin_en, frz_en, res->frz_ratio);
			pixel_mode[idx] = tgo_pxl_mode;
			if (lb_chk_res == LB_CHECK_OK) {
				res->bin_enable = bin_en;
				res->frz_enable = frz_en;
				res->raw_num_used = twin_en + 1;
				clk_res = clk_cur;
				idx_res = idx;
				res->clk_target = clk->clklv[clk_res];
				res_found = true;
			}
		} else {
			/*try 1-pixel mode first*/
			for (p = 1; p <= MTK_CAMSYS_PROC_DEFAULT_PIXELMODE; p++) {
				tgo_pxl_mode = mtk_raw_pixelmode_calc(p, twin_en, bin_en, frz_en,
								      res->frz_ratio);
				/**
				 * isp throughput along resource strategy
				 * (compared with pixel rate)
				 */
				pixel_mode[idx] = tgo_pxl_mode;
				eq_throughput = ((u64)tgo_pxl_mode) * clk->clklv[clk_cur];
				if (eq_throughput > res->pixel_rate &&
				    lb_chk_res == LB_CHECK_OK) {
					if (!res_found) {
						res->bin_enable = bin_en;
						res->frz_enable = frz_en;
						res->raw_num_used = twin_en + 1;
						clk_res = clk_cur;
						idx_res = idx;
						res->clk_target = clk->clklv[clk_res];
						res_found = true;
						break;
					}
				}
			}
		}
		dev_dbg(cam->dev, "Res-%d B/F/H/C=%d/%d/%d/%d -> %d/%d/%d/%d (%d)(%d):%10llu\n",
			idx, bin_temp, frz_temp, hwn_temp, clk_cur, bin_en,
			frz_en, twin_en, clk_cur, lb_chk_res, pixel_mode[idx],
			eq_throughput);
	}
	tgo_pxl_mode = pixel_mode[idx_res];
	switch (tgo_pxl_mode) {
	case 1:
		res->tgo_pxl_mode = 0;
		break;
	case 2:
		res->tgo_pxl_mode = 1;
		break;
	case 4:
		res->tgo_pxl_mode = 2;
		break;
	case 8:
		res->tgo_pxl_mode = 3;
		break;
	default:
		break;
	}
	eq_throughput = ((u64)tgo_pxl_mode) * res->clk_target;
	if (res_found) {
		dev_dbg(cam->dev, "Res-end:%d BIN/FRZ/HWN/CLK/pxl=%d/%d(%d)/%d/%d/%d:%10llu, clk:%d\n",
			idx_res, res->bin_enable, res->frz_enable, res->frz_ratio,
			res->raw_num_used, clk_res, res->tgo_pxl_mode, eq_throughput,
			res->clk_target);
	} else {
		dev_dbg(cam->dev, "[%s] Error resource result; use %dMhz\n",
			__func__, clk->clklv[clk_cur]);
		res->clk_target = clk->clklv[clk_cur];
	}
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

	return res_found;
}

static bool mtk_raw_resource_calc_set(struct mtk_raw_pipeline *pipe,
				      int in_w, int in_h, int *out_w, int *out_h)
{
	struct mtk_cam_device *cam =
		container_of(pipe->raw, struct mtk_cam_device, raw);
	struct mtk_cam_resource_config *res = &pipe->res_config;
	s64 pixel_rate = 0;
	bool result;

	mutex_lock(&res->resource_lock);

	mtk_cam_seninf_get_pixelrate(res->seninf, &pixel_rate);
	result = mtk_raw_resource_calc(cam, res, pixel_rate, res->res_plan,
				       in_w, in_h, out_w, out_h);

	mutex_unlock(&res->resource_lock);

	return result;
}

bool mtk_raw_dev_is_slave(struct mtk_raw_device *raw_dev)
{
	struct device *dev_slave;
	struct mtk_raw_device *raw_dev_slave;
	struct device *dev_slave2;
	struct mtk_raw_device *raw_dev_slave2;
	unsigned int i;

	if (raw_dev->pipeline->res_config.raw_num_used == 2) {
		for (i = 0; i < raw_dev->cam->num_raw_drivers - 1; i++) {
			if (raw_dev->pipeline->enabled_raw & (1 << i)) {
				dev_slave = raw_dev->cam->raw.devs[i + 1];
				break;
			}
		}
		raw_dev_slave = dev_get_drvdata(dev_slave);
		return (raw_dev_slave == raw_dev);
	}
	if (raw_dev->pipeline->res_config.raw_num_used == 3) {
		//dev_slave = raw_dev->cam->raw.devs[RAW_B];
		//dev_slave2 = raw_dev->cam->raw.devs[RAW_C];
		raw_dev_slave = dev_get_drvdata(dev_slave);
		raw_dev_slave2 = dev_get_drvdata(dev_slave2);
		return (raw_dev_slave == raw_dev) || (raw_dev_slave2 == raw_dev);
	}

	return false;
}

static void raw_irq_handle_tg_grab_err(struct mtk_raw_device *raw_dev);
static void raw_irq_handle_dma_err(struct mtk_raw_device *raw_dev);
static void raw_irq_handle_tg_overrun_err(struct mtk_raw_device *raw_dev,
					  int dequeued_frame_seq_no);

static bool is_sub_sample_sensor_timing(struct mtk_raw_device *dev)
{
	int sub_overori_cnt, sub_overori_time, sub_frame_time;
	int frame_count;
	int fps = 30;
	bool res = false;

	sub_overori_cnt = mtk_cam_get_subsample_ratio(
				dev->pipeline->res_config.raw_feature) - 1;
	sub_overori_time = ktime_get_boot_ns() /
				1000 - dev->sof_time;
	fps = dev->pipeline->res_config.interval.denominator /
			dev->pipeline->res_config.interval.numerator;
	sub_frame_time = 1000000 / fps;
	frame_count = (sub_overori_time + (sub_frame_time - 1) / 2) / sub_frame_time;
	if (dev->vsync_ori_count == frame_count) {
		res = false;
	} else if (frame_count == sub_overori_cnt) {
		res = true;
		dev_dbg(dev->dev, "[%s] Normal frame sensor set\n", __func__);
	} else if ((dev->vsync_ori_count < sub_overori_cnt) &&
		(frame_count == sub_overori_cnt + 1)) {
		res = true;
		dev_dbg(dev->dev, "[%s] Last frame sensor set\n", __func__);
	} else {
		res = false;
	}
	dev->vsync_ori_count = frame_count;
	return res;

}
static irqreturn_t mtk_irq_raw(int irq, void *data)
{
	struct mtk_raw_device *raw_dev = (struct mtk_raw_device *)data;
	struct device *dev = raw_dev->dev;
	struct mtk_camsys_irq_info irq_info;
	unsigned int dequeued_frame_seq_no, dequeued_frame_seq_no_inner, fbc_fho_r1_ctl2;
	unsigned int irq_status, err_status, dma_done_status, dmai_done_status;
	unsigned int drop_status, dma_ofl_status, cq_done_status, cq2_done_status;
	unsigned int tg_cfg, cq_en, val_dcif_ctl, val_tg_sen;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&raw_dev->spinlock_irq, flags);
	irq_status	= readl_relaxed(raw_dev->base + REG_CTL_RAW_INT_STAT);
	dma_done_status = readl_relaxed(raw_dev->base + REG_CTL_RAW_INT2_STAT);
	dmai_done_status = readl_relaxed(raw_dev->base + REG_CTL_RAW_INT3_STAT);
	drop_status	= readl_relaxed(raw_dev->base + REG_CTL_RAW_INT4_STAT);
	dma_ofl_status  = readl_relaxed(raw_dev->base + REG_CTL_RAW_INT5_STAT);
	cq_done_status  = readl_relaxed(raw_dev->base + REG_CTL_RAW_INT6_STAT);
	cq2_done_status = readl_relaxed(raw_dev->base + REG_CTL_RAW_INT7_STAT);
	tg_cfg = readl_relaxed(raw_dev->base_inner + REG_TG_PATH_CFG);
	cq_en = readl_relaxed(raw_dev->base_inner + REG_CQ_EN);
	val_dcif_ctl = readl_relaxed(raw_dev->base_inner + REG_TG_DCIF_CTL);
	val_tg_sen = readl_relaxed(raw_dev->base_inner + REG_TG_SEN_MODE);

	/**
	 * TODO: read seq number from outer register.
	 * This value may be wrong, since
	 * @sw p1 done: legacy cq is triggered, overwriting this value
	 * Should replace with the frame-header implementation.
	 */
	dequeued_frame_seq_no =
		readl_relaxed(raw_dev->base + REG_FRAME_SEQ_NUM);
	dequeued_frame_seq_no_inner =
		readl_relaxed(raw_dev->base_inner + REG_FRAME_SEQ_NUM);
	fbc_fho_r1_ctl2 =
		readl_relaxed(REG_FBC_CTL2(raw_dev->base + FBC_R1A_BASE, 1));
	spin_unlock_irqrestore(&raw_dev->spinlock_irq, flags);

	err_status = irq_status & INT_ST_MASK_CAM_ERR;
	dev_dbg(dev,
		"INT:0x%x(err:0x%x) 2~7 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x (in:%d)\n",
		irq_status, err_status,
		dma_done_status, dmai_done_status, drop_status,
		dma_ofl_status, cq_done_status, cq2_done_status,
		dequeued_frame_seq_no_inner);

	if (!raw_dev->pipeline || !raw_dev->pipeline->enabled_raw) {
		dev_dbg(dev,
			"%s: %i: raw pipeline is disabled\n",
			__func__, raw_dev->id);
		goto ctx_not_found;
	}

	/*
	 * In normal case, the next SOF ISR should come after HW PASS1 DONE ISR.
	 * If these two ISRs come together, print warning msg to hint.
	 */
	irq_info.engine_id = CAMSYS_ENGINE_RAW_BEGIN + raw_dev->id;
	irq_info.frame_idx = dequeued_frame_seq_no;
	irq_info.frame_inner_idx = dequeued_frame_seq_no_inner;
	irq_info.irq_type = 0;
	irq_info.slave_engine = mtk_raw_dev_is_slave(raw_dev);
	/* CQ done */
	if (cq_done_status & CAMCTL_CQ_THR0_DONE_ST) {
		irq_info.irq_type |= 1 << CAMSYS_IRQ_SETTING_DONE;
		raw_dev->setting_count++;
	}
	/* Frame done */
	if (irq_status & SW_PASS1_DON_ST)
		irq_info.irq_type |= 1 << CAMSYS_IRQ_FRAME_DONE;

	/* Frame start */
#if _STAGGER_TRIGGER_CQ_BY_CAMSV_SOF
	if (irq_status & SOF_INT_ST) {
		irq_info.irq_type |= 1 << CAMSYS_IRQ_FRAME_START;
		raw_dev->sof_time = ktime_get_boot_ns() / 1000;
		raw_dev->write_cnt =
			((fbc_fho_r1_ctl2 & WCNT_BIT_MASK) >> 8) - 1;
		dev_dbg(dev, "[SOF] fho wcnt:%d\n", raw_dev->write_cnt);
		raw_dev->sof_count++;
		raw_dev->vsync_ori_count = 0;
	}
#else
	if ((!raw_dev->pipeline->res_config.raw_feature) && (irq_status & SOF_INT_ST)) {
		irq_info.irq_type |= 1 << CAMSYS_IRQ_FRAME_START;
		raw_dev->sof_count++;
	} else if ((raw_dev->pipeline->res_config.raw_feature) && (irq_status & VS_INT_ST)) {
		irq_info.irq_type |= 1 << CAMSYS_IRQ_FRAME_START;
		raw_dev->sof_count++;
	}
#endif
	if (irq_status & TG_VS_INT_ORG_ST &&
		raw_dev->pipeline->res_config.raw_feature & MTK_CAM_FEATURE_SUBSAMPLE_MASK) {
		if (is_sub_sample_sensor_timing(raw_dev))
			irq_info.irq_type |= 1 << CAMSYS_IRQ_SUBSAMPLE_SENSOR_SET;
	}
	if (err_status & DMA_ERR_ST &&
		irq_status & SOF_INT_ST) {
		irq_info.irq_type |= 1 << CAMSYS_IRQ_FRAME_DROP;
		dev_dbg(dev, "[SOF+DMA_ERR] CAMSYS_IRQ_FRAME_DROP\n");
	}

	/* inform interrupt information to camsys controller */
	ret = mtk_camsys_isr_event(raw_dev->cam, &irq_info);
	if (ret)
		goto ctx_not_found;

	/* Check ISP error status */
	if (err_status) {
		dev_info(dev, "int_err:0x%x 0x%x\n", irq_status, err_status);

		/* Show DMA errors in detail */
		if (err_status & DMA_ERR_ST) {

			/*
			 * mtk_cam_dump_topdebug_rdyreq(dev,
			 *                              raw_dev->base, raw_dev->yuv_base);
			 */

			raw_irq_handle_dma_err(raw_dev);

			/*
			 * mtk_cam_dump_req_rdy_status(raw_dev->dev, raw_dev->base,
			 *		    raw_dev->yuv_base);
			 */

			/*
			 * mtk_cam_dump_dma_debug(raw_dev->dev, raw_dev->base + CAMDMATOP_BASE,
			 *                        "IMGO_R1",
			 *                        dbg_IMGO_R1, ARRAY_SIZE(dbg_IMGO_R1));
			 */
		}
		/* Show TG register for more error detail*/
		if (err_status & TG_GBERR_ST) {
			struct mtk_cam_ctx *ctx =
			mtk_cam_find_ctx(raw_dev->cam, &raw_dev->pipeline->subdev.entity);

			mtk_cam_seninf_dump(ctx->seninf);
			raw_irq_handle_tg_grab_err(raw_dev);
		}
		if (err_status & TG_OVRUN_ST) {
			raw_irq_handle_tg_overrun_err(raw_dev,
						      dequeued_frame_seq_no_inner);
		}
	}

	/* enable to debug fbc related
	 * if (irq_status & SOF_INT_ST)
	 *     mtk_cam_raw_dump_fbc(raw_dev->dev,
	 *                          raw_dev->base, raw_dev->yuv_base);
	 */

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
	dev_dbg_ratelimited(raw_dev->dev,
		"TG PATHCFG/SENMODE FRMSIZE/R GRABPXL/LIN:%x/%x %x/%x %x/%x\n",
		readl_relaxed(raw_dev->base + REG_TG_PATH_CFG),
		readl_relaxed(raw_dev->base + REG_TG_SEN_MODE),
		readl_relaxed(raw_dev->base + REG_TG_FRMSIZE_ST),
		readl_relaxed(raw_dev->base + REG_TG_FRMSIZE_ST_R),
		readl_relaxed(raw_dev->base + REG_TG_SEN_GRAB_PXL),
		readl_relaxed(raw_dev->base + REG_TG_SEN_GRAB_LIN));
}

void raw_irq_handle_dma_err(struct mtk_raw_device *raw_dev)
{
	mtk_cam_raw_dump_dma_err_st(raw_dev->dev, raw_dev->base);
	mtk_cam_yuv_dump_dma_err_st(raw_dev->dev, raw_dev->yuv_base);

	if (raw_dev->pipeline->res_config.raw_feature)
		mtk_cam_dump_dma_debug(raw_dev->dev,
				       raw_dev->base + CAMDMATOP_BASE,
				       "RAWI_R2",
				       dbg_RAWI_R2, ARRAY_SIZE(dbg_RAWI_R2));

	/*
	 * mtk_cam_dump_dma_debug(raw_dev->dev, raw_dev->base + CAMDMATOP_BASE,
	 *                        "IMGO_R1", dbg_IMGO_R1, ARRAY_SIZE(dbg_IMGO_R1));
	 */
}

static void raw_irq_handle_tg_overrun_err(struct mtk_raw_device *raw_dev,
					  int dequeued_frame_seq_no)
{
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_request *req;
	int val, val2;

	val = readl_relaxed(raw_dev->base + REG_TG_PATH_CFG);
	val = val | TG_TG_FULL_SEL;
	writel_relaxed(val, raw_dev->base + REG_TG_PATH_CFG);
	wmb(); /* for dbg dump register */
	val2 = readl_relaxed(raw_dev->base + REG_TG_SEN_MODE);
	val2 = val2 | TG_CMOS_RDY_SEL;
	writel_relaxed(val2, raw_dev->base + REG_TG_SEN_MODE);
	wmb(); /* for dbg dump register */

	dev_info(raw_dev->dev,
			 "TG PATHCFG/SENMODE FRMSIZE/R GRABPXL/LIN:%x/%x %x/%x %x/%x\n",
			 readl_relaxed(raw_dev->base + REG_TG_PATH_CFG),
			 readl_relaxed(raw_dev->base + REG_TG_SEN_MODE),
			 readl_relaxed(raw_dev->base + REG_TG_FRMSIZE_ST),
			 readl_relaxed(raw_dev->base + REG_TG_FRMSIZE_ST_R),
			 readl_relaxed(raw_dev->base + REG_TG_SEN_GRAB_PXL),
			 readl_relaxed(raw_dev->base + REG_TG_SEN_GRAB_LIN));
	dev_info(raw_dev->dev,
			 "REQ RAW/2/3 DMA/2:%08x/%08x/%08x/%08x/%08x\n",
			 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD_REQ_STAT),
			 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD2_REQ_STAT),
			 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD3_REQ_STAT),
			 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD5_REQ_STAT),
			 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD6_REQ_STAT));
	dev_info(raw_dev->dev,
			 "RDY RAW/2/3 DMA/2:%08x/%08x/%08x/%08x/%08x\n",
			 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD_RDY_STAT),
			 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD2_RDY_STAT),
			 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD3_RDY_STAT),
			 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD5_RDY_STAT),
			 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD6_RDY_STAT));
	dev_info(raw_dev->dev,
			 "REQ YUV/2/3/4 WDMA:%08x/%08x/%08x/%08x/%08x\n",
			 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD_REQ_STAT),
			 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD2_REQ_STAT),
			 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD3_REQ_STAT),
			 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD4_REQ_STAT),
			 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD5_REQ_STAT));
	dev_info(raw_dev->dev,
			 "RDY YUV/2/3/4 WDMA:%08x/%08x/%08x/%08x/%08x\n",
			 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD_RDY_STAT),
			 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD2_RDY_STAT),
			 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD3_RDY_STAT),
			 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD4_RDY_STAT),
			 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD5_RDY_STAT));

	ctx = mtk_cam_find_ctx(raw_dev->cam, &raw_dev->pipeline->subdev.entity);
	if (!ctx) {
		dev_info(raw_dev->dev, "%s: cannot find ctx\n", __func__);
		return;
	}

	/* TODO: check if we tried recover the error before we dump */

	req = mtk_cam_dev_get_req(raw_dev->cam, ctx, dequeued_frame_seq_no);
	if (req)
		mtk_cam_req_dump(ctx, req, MTK_CAM_REQ_DUMP_CHK_DEQUEUE_FAILED,
					 "TG overrun");
	else
		dev_info(raw_dev->dev, "%s: req(%d) can't be found for dump\n",
			__func__, dequeued_frame_seq_no);
}

static int mtk_raw_of_probe(struct platform_device *pdev,
			    struct mtk_raw_device *raw)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	unsigned int i;
	int irq, ret;

	ret = of_property_read_u32(dev->of_node, "mediatek,cam-id",
				   &raw->id);
	if (ret) {
		dev_dbg(dev, "missing camid property\n");
		return ret;
	}

	/* base outer register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base");
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	raw->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(raw->base)) {
		dev_dbg(dev, "failed to map register base\n");
		return PTR_ERR(raw->base);
	}

	/* base inner register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "inner_base");
	if (!res) {
		dev_dbg(dev, "failed to get mem\n");
		return -ENODEV;
	}

	raw->base_inner = devm_ioremap_resource(dev, res);
	if (IS_ERR(raw->base_inner)) {
		dev_dbg(dev, "failed to map register inner base\n");
		return PTR_ERR(raw->base_inner);
	}

	/* will be assigned later */
	raw->yuv_base = NULL;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_dbg(dev, "failed to get irq\n");
		return -ENODEV;
	}

	ret = devm_request_irq(dev, irq, mtk_irq_raw, 0,
			       dev_name(dev), raw);
	if (ret) {
		dev_dbg(dev, "failed to request irq=%d\n", irq);
		return ret;
	}
	dev_dbg(dev, "registered irq=%d\n", irq);

	raw->num_clks = of_count_phandle_with_args(pdev->dev.of_node, "clocks",
			"#clock-cells");
	dev_info(dev, "clk_num:%d\n", raw->num_clks);

	if (!raw->num_clks) {
		dev_dbg(dev, "no clock\n");
		return -ENODEV;
	}

	raw->clks = devm_kcalloc(dev, raw->num_clks, sizeof(*raw->clks),
				 GFP_KERNEL);
	if (!raw->clks)
		return -ENOMEM;

	for (i = 0; i < raw->num_clks; i++) {
		raw->clks[i] = of_clk_get(pdev->dev.of_node, i);
		if (IS_ERR(raw->clks[i])) {
			dev_info(dev, "failed to get clk %d\n", i);
			return -ENODEV;
		}
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
	case V4L2_EVENT_EOS:
		return v4l2_event_subscribe(fh, sub, 0, NULL);
	case V4L2_EVENT_REQUEST_DUMPED:
		return v4l2_event_subscribe(fh, sub, 0, NULL);

	default:
		return -EINVAL;
	}
}

static int mtk_raw_available_resource(struct mtk_raw *raw)
{
	int res_status = 0;
	int i, j;

	for (i = 0; i < RAW_PIPELINE_NUM; i++) {
		struct mtk_raw_pipeline *pipe = raw->pipelines + i;

		for (j = 0; j < ARRAY_SIZE(raw->devs); j++) {
			if (pipe->enabled_raw & 1 << j)
				res_status |= 1 << j;
		}
	}
	dev_info(raw->cam_dev, "%s raw_status:0x%x Available Engine:A/B/C:%d/%d/%d\n",
		 __func__, res_status,
			!(res_status & (1 << MTKCAM_SUBDEV_RAW_0)),
			!(res_status & (1 << MTKCAM_SUBDEV_RAW_1)),
			!(res_status & (1 << MTKCAM_SUBDEV_RAW_2)));

	return res_status;
}

int mtk_cam_raw_select(struct mtk_raw_pipeline *pipe,
		       struct mtkcam_ipi_input_param *cfg_in_param)
{
	struct mtk_raw_pipeline *pipe_chk;
	int raw_status = 0;
	int mask = 0x0;
	bool selected = false;
	int m;

	pipe->enabled_raw = 0;
	raw_status = mtk_raw_available_resource(pipe->raw);

	if (pipe->res_config.raw_feature & MTK_CAM_FEATURE_TIMESHARE_MASK) {
		int ts_id, ts_id_chk;
		/*First, check if group ID used in every pipeline*/
		/*if yes , use same engine*/
		for (m = MTKCAM_SUBDEV_RAW_0; m < ARRAY_SIZE(pipe->raw->devs); m++) {
			pipe_chk = pipe->raw->pipelines + m;
			pr_info("[%s] checking idx:%d pipe_id:%d pipe_chk_id:%d\n", __func__,
				m, pipe->id, pipe_chk->id);
			if (pipe->id != pipe_chk->id) {
				ts_id = pipe->res_config.raw_feature &
						MTK_CAM_FEATURE_TIMESHARE_MASK;
				ts_id_chk = pipe_chk->res_config.raw_feature &
							MTK_CAM_FEATURE_TIMESHARE_MASK;
				if (ts_id == ts_id_chk &&
					pipe_chk->enabled_raw != 0) {
					mask = pipe_chk->enabled_raw & MTKCAM_SUBDEV_RAW_MASK;
					pipe->enabled_raw |= mask;
					selected = true;
					break;
				}
			}
		}
		if (selected) {
			pr_info("[%s] Timeshared (%d)- enabled_raw:0x%x as pipe:%d enabled_raw:0x%x\n",
				__func__, ts_id >> 8, pipe->enabled_raw,
				pipe_chk->id, pipe_chk->enabled_raw);
		} else {
			/*if no , use new engine from a->b->c*/
			for (m = MTKCAM_SUBDEV_RAW_0; m < ARRAY_SIZE(pipe->raw->devs); m++) {
				pipe_chk = pipe->raw->pipelines + m;
				mask = 1 << m;
				if (!(raw_status & mask)) {
					pipe->enabled_raw |= mask;
					selected = true;
					break;
				}
			}
		}
	} else if (pipe->res_config.raw_num_used == 3) {
		mask = 1 << MTKCAM_SUBDEV_RAW_0
			| 1 << MTKCAM_SUBDEV_RAW_1 | 1 << MTKCAM_SUBDEV_RAW_2;
		if (!(raw_status & mask)) {
			pipe->enabled_raw |= mask;
			selected = true;
		}
	} else if (pipe->res_config.raw_num_used == 2) {
		for (m = MTKCAM_SUBDEV_RAW_1; m >= MTKCAM_SUBDEV_RAW_0; m--) {
			mask = (1 << m) | (1 << (m + 1));
			if (!(raw_status & mask)) {
				pipe->enabled_raw |= mask;
				selected = true;
				break;
			}
		}
	} else {
		for (m = MTKCAM_SUBDEV_RAW_0; m < ARRAY_SIZE(pipe->raw->devs); m++) {
			mask = 1 << m;
			if (!(raw_status & mask)) {
				pipe->enabled_raw |= mask;
				selected = true;
				break;
			}
		}
	}
	mtk_raw_available_resource(pipe->raw);
	if (!selected)
		return -EINVAL;
	/**
	 * TODO: duplicated using raw case will implement in time sharing isp case
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
		ctx->used_raw_num++;

		for (i = 0; i < ARRAY_SIZE(pipe->vdev_nodes); i++) {
			if (!pipe->vdev_nodes[i].enabled)
				continue;
			pipe->enabled_dmas |= 1 << pipe->vdev_nodes[i].desc.dma_port;
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(raw->devs); i++) {
			if (pipe->enabled_raw & 1 << i) {
				dev_info(raw->cam_dev, "%s: power off raw (%d)\n", __func__, i);
				pm_runtime_put(raw->devs[i]);
			}
		}
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
		if (fmt->pad < MTK_RAW_META_OUT_BEGIN &&
				fmt->pad >= MTK_RAW_META_IN)
			node->raw_feature = pipe->res_config.raw_feature;
		dev_dbg(raw->cam_dev, "node:%s num_fmts:%d feature:0x%x",
			node->desc.name, node->desc.num_fmts, node->raw_feature);
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

	return MTKCAM_IPI_IMG_FMT_UNKNOWN;
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

static int mtk_raw_call_set_fmt(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *fmt)
{
	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);
	struct mtk_raw *raw = pipe->raw;
	struct v4l2_mbus_framefmt *mf;
	struct mtk_cam_resource_config *res_cfg;

	if (!sd || !fmt) {
		dev_dbg(raw->cam_dev, "%s: Required sd(%p), fmt(%p)\n",
			__func__, sd, fmt);
		return -EINVAL;
	}

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY && !cfg) {
		dev_dbg(raw->cam_dev, "%s: Required sd(%p), cfg(%p) for FORMAT_TRY\n",
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
		struct v4l2_format *img_fmt;
		bool ret;
		int w, h;

		if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
			img_fmt = &pipe->vdev_nodes[MTK_RAW_SINK].pending_fmt;
			img_fmt->fmt.pix_mp.width = mf->width;
			img_fmt->fmt.pix_mp.height = mf->height;
		}
		source_mf = get_fmt(pipe, cfg, MTK_RAW_MAIN_STREAM_OUT,
				    fmt->which);
		if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
			s64 prate = 0;
			struct mtk_cam_device *cam = container_of(pipe->raw,
				struct mtk_cam_device, raw);
			res_cfg = &pipe->try_res_config;
			prate = mtk_cam_seninf_calc_pixelrate
					(raw->cam_dev, mf->width, mf->height,
					res_cfg->hblank, res_cfg->vblank,
					res_cfg->interval.denominator,
					res_cfg->interval.numerator,
					res_cfg->sensor_pixel_rate);
			ret = mtk_raw_resource_calc(cam, &pipe->try_res_config,
						    prate, res_cfg->res_plan,
						    mf->width, mf->height,
						    &w, &h);
		} else {
			/**
			 * mtk_raw_resource_calc_set() will be phased out
			 * since it uses the link information and it must
			 * be decoupled with format negociation flow
			 * like the updated V4L2_SUBDEV_FORMAT_TRY flow.
			 */
			ret = mtk_raw_resource_calc_set(pipe, mf->width,
							mf->height, &w, &h);
		}

		if (!ret)
			return -EINVAL;

		propagate_fmt(mf, source_mf, w, h);
	}

	return 0;
}

int mtk_raw_call_pending_set_fmt(struct v4l2_subdev *sd,
				 struct v4l2_subdev_format *fmt)
{
	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);
	struct mtk_cam_device *cam = dev_get_drvdata(pipe->raw->cam_dev);

	/* We only allow V4L2_SUBDEV_FORMAT_ACTIVE for pending set fmt */
	if (fmt->which != V4L2_SUBDEV_FORMAT_ACTIVE) {
		dev_info(cam->dev,
			"%s:pipe(%d):pad(%d): only allow V4L2_SUBDEV_FORMAT_ACTIVE\n",
			__func__, pipe->id, fmt->pad);
		return -EINVAL;
	}

	return mtk_raw_call_set_fmt(sd, NULL, fmt);
}

static int mtk_raw_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *fmt)
{
	struct media_request *req;
	struct mtk_cam_request *cam_req;
	struct mtk_cam_request_stream_data *stream_data;

	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);
	struct mtk_cam_device *cam = dev_get_drvdata(pipe->raw->cam_dev);

	/* if the pipeline is streaming, pending the change */
	if (!sd->entity.stream_count || fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		return mtk_raw_call_set_fmt(sd, cfg, fmt);

	if (fmt->request_fd <= 0)
		return -EINVAL;

	req = media_request_get_by_fd(&cam->media_dev, fmt->request_fd);
	if (req) {
		cam_req = to_mtk_cam_req(req);
		dev_info(cam->dev, "sd:%s pad:%d pending success, req fd(%d)\n",
			sd->name, fmt->pad, fmt->request_fd);
	} else {
		dev_info(cam->dev, "sd:%s pad:%d pending failed, req fd(%d) invalid\n",
			sd->name, fmt->pad, fmt->request_fd);
		return -EINVAL;
	}

	stream_data = &cam_req->stream_data[pipe->id];
	stream_data->pad_fmt_update |= (1 << fmt->pad);
	stream_data->pad_fmt[fmt->pad] = *fmt;

	media_request_put(req);

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

int mtk_cam_update_sensor(struct mtk_cam_ctx *ctx,
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
			dev_dbg(ctx->cam->dev, "sensor:%s->%s\n",
				ctx->prev_sensor->entity.name, ctx->sensor->entity.name);
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
	u32 pad = local->index;

	dev_dbg(raw->cam_dev, "%s: raw %d: remote:%s:%d->local:%s:%d flags:0x%x\n",
		__func__, pipe->id, remote->entity->name, remote->index,
		local->entity->name, local->index, flags);

	if (pad < MTK_RAW_PIPELINE_PADS_NUM && pad != MTK_RAW_SINK)
		pipe->vdev_nodes[pad - MTK_RAW_SINK_NUM].enabled =
			!!(flags & MEDIA_LNK_FL_ENABLED);

	if (!(flags & MEDIA_LNK_FL_ENABLED))
		memset(pipe->cfg, 0, sizeof(pipe->cfg));
	if (pad == MTK_RAW_SINK && flags & MEDIA_LNK_FL_ENABLED)
		pipe->res_config.seninf =
			media_entity_to_v4l2_subdev(remote->entity);

	return 0;
}

static int
mtk_raw_s_frame_interval(struct v4l2_subdev *sd,
			 struct v4l2_subdev_frame_interval *interval)
{
	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);
	struct mtk_raw *raw = pipe->raw;

	if (interval->which == V4L2_SUBDEV_FORMAT_TRY) {
		dev_dbg(raw->cam_dev, "%s:pipe(%d):try res: fps = %d/%d",
			__func__, pipe->id,
			interval->interval.numerator,
			interval->interval.denominator);
		pipe->try_res_config.interval = interval->interval;
	} else {
		dev_dbg(raw->cam_dev, "%s:pipe(%d):current res: fps = %d/%d",
			__func__, pipe->id,
			interval->interval.numerator,
			interval->interval.denominator);
		pipe->res_config.interval = interval->interval;
	}

	return 0;
}

static const struct v4l2_subdev_core_ops mtk_raw_subdev_core_ops = {
	.subscribe_event = mtk_raw_sd_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_video_ops mtk_raw_subdev_video_ops = {
	.s_stream =  mtk_raw_sd_s_stream,
	.s_frame_interval = mtk_raw_s_frame_interval,
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

static const struct v4l2_ioctl_ops mtk_cam_v4l2_vout_ioctl_ops = {
	.vidioc_querycap = mtk_cam_vidioc_querycap,
	.vidioc_enum_framesizes = mtk_cam_vidioc_enum_framesizes,
	.vidioc_enum_fmt_vid_cap = mtk_cam_vidioc_enum_fmt,
	.vidioc_enum_fmt_vid_cap_mplane = mtk_cam_vidioc_enum_fmt,
	.vidioc_g_fmt_vid_out_mplane = mtk_cam_vidioc_g_fmt,
	.vidioc_s_fmt_vid_out_mplane = mtk_cam_vidioc_s_fmt,
	.vidioc_try_fmt_vid_out_mplane = mtk_cam_vidioc_try_fmt,
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

static const struct v4l2_ioctl_ops mtk_cam_v4l2_vcap_ioctl_ops = {
	.vidioc_querycap = mtk_cam_vidioc_querycap,
	.vidioc_enum_framesizes = mtk_cam_vidioc_enum_framesizes,
	.vidioc_enum_fmt_vid_cap = mtk_cam_vidioc_enum_fmt,
	.vidioc_enum_fmt_vid_cap_mplane = mtk_cam_vidioc_enum_fmt,
	.vidioc_g_fmt_vid_cap_mplane = mtk_cam_vidioc_g_fmt,
	.vidioc_s_fmt_vid_cap_mplane = mtk_cam_vidioc_s_fmt,
	.vidioc_try_fmt_vid_cap_mplane = mtk_cam_vidioc_try_fmt,
	.vidioc_s_selection = mtk_cam_vidioc_s_selection,
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
			.buffersize = RAW_STATS_CFG_SIZE,
		},
	},
	{
		.fmt.meta = {
			.dataformat = V4L2_META_FMT_MTISP_3A,
			.buffersize = RAW_STATS_0_SIZE,
		},
	},
	{
		.fmt.meta = {
			.dataformat = V4L2_META_FMT_MTISP_AF,
			.buffersize = RAW_STATS_1_SIZE,
		},
	},
	{
		.fmt.meta = {
			.dataformat = V4L2_META_FMT_MTISP_LCS,
			.buffersize = RAW_STATS_2_SIZE,
		},
	},
};

static const struct v4l2_format stream_out_fmts[] = {
	/* This is a default image format */
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
			.pixelformat = V4L2_PIX_FMT_SBGGR10,
		},
	},
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
			.pixelformat = V4L2_PIX_FMT_SBGGR12,
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
			.pixelformat = V4L2_PIX_FMT_SBGGR14,
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
			.pixelformat = V4L2_PIX_FMT_SGBRG10,
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
			.pixelformat = V4L2_PIX_FMT_SGBRG12,
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
			.pixelformat = V4L2_PIX_FMT_SGBRG14,
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
			.pixelformat = V4L2_PIX_FMT_SGRBG10,
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
			.pixelformat = V4L2_PIX_FMT_SGRBG12,
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
			.pixelformat = V4L2_PIX_FMT_SGRBG14,
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
			.pixelformat = V4L2_PIX_FMT_SRGGB10,
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
			.pixelformat = V4L2_PIX_FMT_SRGGB12,
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
			.pixelformat = V4L2_PIX_FMT_SRGGB14,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB14,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SBGGR16,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGBRG16,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGRBG16,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SRGGB16,
		},
	},
};

static const struct v4l2_format yuv_out_group1_fmts[] = {
	{
		.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12,
		},
	},
	{
		.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21,
		},
	},
	{
		.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12_10,
		},
	},
	{
		.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21_10,
		},
	},
	{
		.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV12_10P,
		},
	},
	{
		.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV21_10P,
		},
	},
	{
		.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12_12,
		},
	},
	{
		.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21_12,
		},
	},
	{
		.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV12_12P,
		},
	},
	{
		.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV21_12P,
		},
	},
	{
		.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_YUV420,
		},
	}
};

static const struct v4l2_format yuv_out_group2_fmts[] = {
	{
		.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12,
		},
	},
	{
		.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21,
		},
	},
	{
		.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12_10,
		},
	},
	{
		.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21_10,
		},
	},
	{
		.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV12_10P,
		},
	},
	{
		.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV21_10P,
		},
	},
	{
		.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12_12,
		},
	},
	{
		.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21_12,
		},
	},
	{
		.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV12_12P,
		},
	},
	{
		.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV21_12P,
		},
	}
};

static const struct v4l2_format rzh1n2to1_out_fmts[] = {
	{
		.fmt.pix_mp = {
			.width = RZH1N2TO1_MAX_WIDTH,
			.height = RZH1N2TO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12,
		},
	},
	{
		.fmt.pix_mp = {
			.width = RZH1N2TO1_MAX_WIDTH,
			.height = RZH1N2TO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21,
		},
	},
	{
		.fmt.pix_mp = {
			.width = RZH1N2TO1_MAX_WIDTH,
			.height = RZH1N2TO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV16,
		},
	},
	{
		.fmt.pix_mp = {
			.width = RZH1N2TO1_MAX_WIDTH,
			.height = RZH1N2TO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV61,
		},
	}
};

static const struct v4l2_format rzh1n2to2_out_fmts[] = {
	{
		.fmt.pix_mp = {
			.width = RZH1N2TO2_MAX_WIDTH,
			.height = RZH1N2TO2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_YUYV,
		},
	},
	{
		.fmt.pix_mp = {
			.width = RZH1N2TO2_MAX_WIDTH,
			.height = RZH1N2TO2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_YVYU,
		},
	}
};

static const struct v4l2_format rzh1n2to3_out_fmts[] = {
	{
		.fmt.pix_mp = {
			.width = RZH1N2TO3_MAX_WIDTH,
			.height = RZH1N2TO3_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12,
		},
	},
	{
		.fmt.pix_mp = {
			.width = RZH1N2TO3_MAX_WIDTH,
			.height = RZH1N2TO3_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21,
		},
	},
	{
		.fmt.pix_mp = {
			.width = RZH1N2TO3_MAX_WIDTH,
			.height = RZH1N2TO3_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV16,
		},
	},
	{
		.fmt.pix_mp = {
			.width = RZH1N2TO3_MAX_WIDTH,
			.height = RZH1N2TO3_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV61,
		},
	}
};

static const struct v4l2_format drzs4no1_out_fmts[] = {
	{
		.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_GREY,
		},
	},
	{
		.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV16,
		},
	},
	{
		.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV61,
		},
	},
	{
		.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV16_10,
		},
	},
	{
		.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV61_10,
		},
	},
	{
		.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV16_10P,
		},
	},
	{
		.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV61_10P,
		},
	},
	{
		.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12,
		},
	},
	{
		.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21,
		},
	},
	{
		.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12_10,
		},
	},
	{
		.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21_10,
		},
	},
	{
		.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV12_10P,
		},
	},
	{
		.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV21_10P,
		},
	},
};

static const struct v4l2_format drzs4no2_out_fmts[] = {
	{
		.fmt.pix_mp = {
			.width = DRZS4NO2_MAX_WIDTH,
			.height = DRZS4NO2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_GREY,
		},
	}
};

static const struct v4l2_format drzs4no3_out_fmts[] = {
	{
		.fmt.pix_mp = {
			.width = DRZS4NO3_MAX_WIDTH,
			.height = DRZS4NO3_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_GREY,
		},
	}
};

#define MTK_RAW_TOTAL_OUTPUT_QUEUES 4

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
		.dma_port = MTKCAM_IPI_RAW_META_STATS_CFG,
		.fmts = meta_fmts,
		.default_fmt_idx = 0,
		.max_buf_count = 16,
		.ioctl_ops = &mtk_cam_v4l2_meta_out_ioctl_ops,
	},
	{
		.id = MTK_RAW_RAWI_2_IN,
		.name = "rawi 2",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.link_flags = 0,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_RAWI_2,
		.fmts = stream_out_fmts,
		.num_fmts = ARRAY_SIZE(stream_out_fmts),
		.default_fmt_idx = 0,
		.ioctl_ops = &mtk_cam_v4l2_vout_ioctl_ops,
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
		.id = MTK_RAW_RAWI_3_IN,
		.name = "rawi 3",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.link_flags = 0,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_RAWI_3,
		.fmts = stream_out_fmts,
		.num_fmts = ARRAY_SIZE(stream_out_fmts),
		.default_fmt_idx = 0,
		.ioctl_ops = &mtk_cam_v4l2_vout_ioctl_ops,
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
		.id = MTK_RAW_RAWI_4_IN,
		.name = "rawi 4",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.link_flags = 0,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_RAWI_3, //todo: wait backend add MTKCAM_IPI_RAW_RAWI_4
		.fmts = stream_out_fmts,
		.num_fmts = ARRAY_SIZE(stream_out_fmts),
		.default_fmt_idx = 0,
		.ioctl_ops = &mtk_cam_v4l2_vout_ioctl_ops,
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
	}
};

static const char *output_queue_names[RAW_PIPELINE_NUM][MTK_RAW_TOTAL_OUTPUT_QUEUES] = {
	{"mtk-cam raw-0 meta-input", "mtk-cam raw-0 rawi-2",
	 "mtk-cam raw-0 rawi-3", "mtk-cam raw-0 rawi-4"},
	{"mtk-cam raw-1 meta-input", "mtk-cam raw-1 rawi-2",
	 "mtk-cam raw-1 rawi 3", "mtk-cam raw-1 rawi-4"},
};

#define MTK_RAW_TOTAL_CAPTURE_QUEUES 15 //todo :check backend node size

static const struct
mtk_cam_dev_node_desc capture_queues[] = {
	{
		.id = MTK_RAW_MAIN_STREAM_OUT,
		.name = "imgo",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = 0,
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
		.id = MTK_RAW_YUVO_1_OUT,
		.name = "yuvo 1",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = 0,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_YUVO_1,
		.fmts = yuv_out_group1_fmts,
		.num_fmts = ARRAY_SIZE(yuv_out_group1_fmts),
		.default_fmt_idx = 0,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = YUV_GROUP1_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = YUV_GROUP1_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_YUVO_2_OUT,
		.name = "yuvo 2",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = 0,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_YUVO_2,
		.fmts = yuv_out_group2_fmts,
		.num_fmts = ARRAY_SIZE(yuv_out_group2_fmts),
		.default_fmt_idx = 0,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = YUV_GROUP2_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = YUV_GROUP2_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_YUVO_3_OUT,
		.name = "yuvo 3",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = 0,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_YUVO_3,
		.fmts = yuv_out_group1_fmts,
		.num_fmts = ARRAY_SIZE(yuv_out_group1_fmts),
		.default_fmt_idx = 0,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = YUV_GROUP1_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = YUV_GROUP1_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_YUVO_4_OUT,
		.name = "yuvo 4",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = 0,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_YUVO_4,
		.fmts = yuv_out_group2_fmts,
		.num_fmts = ARRAY_SIZE(yuv_out_group2_fmts),
		.default_fmt_idx = 0,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = YUV_GROUP2_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = YUV_GROUP2_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_YUVO_5_OUT,
		.name = "yuvo 5",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = 0,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_YUVO_5,
		.fmts = yuv_out_group2_fmts,
		.num_fmts = ARRAY_SIZE(yuv_out_group2_fmts),
		.default_fmt_idx = 0,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = YUV_GROUP2_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = YUV_GROUP2_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_DRZS4NO_1_OUT,
		.name = "drzs4no 1",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = 0,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_DRZS4NO_1,
		.fmts = drzs4no1_out_fmts,
		.num_fmts = ARRAY_SIZE(drzs4no1_out_fmts),
		.default_fmt_idx = 0,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = DRZS4NO1_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = DRZS4NO1_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_DRZS4NO_2_OUT,
		.name = "drzs4no 2",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = 0,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_DRZS4NO_2,
		.fmts = drzs4no2_out_fmts,
		.num_fmts = ARRAY_SIZE(drzs4no2_out_fmts),
		.default_fmt_idx = 0,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = DRZS4NO2_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = DRZS4NO2_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_DRZS4NO_3_OUT,
		.name = "drzs4no 3",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = 0,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_DRZS4NO_3,
		.fmts = drzs4no3_out_fmts,
		.num_fmts = ARRAY_SIZE(drzs4no3_out_fmts),
		.default_fmt_idx = 0,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = DRZS4NO3_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = DRZS4NO3_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_RZH1N2TO_1_OUT,
		.name = "rzh1n2to 1",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = 0,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_RZH1N2TO_1,
		.fmts = rzh1n2to1_out_fmts,
		.num_fmts = ARRAY_SIZE(rzh1n2to1_out_fmts),
		.default_fmt_idx = 0,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = RZH1N2TO1_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = RZH1N2TO1_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_RZH1N2TO_2_OUT,
		.name = "rzh1n2to 2",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = 0,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_RZH1N2TO_2,
		.fmts = rzh1n2to2_out_fmts,
		.num_fmts = ARRAY_SIZE(rzh1n2to2_out_fmts),
		.default_fmt_idx = 0,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = RZH1N2TO2_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = RZH1N2TO2_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_RZH1N2TO_3_OUT,
		.name = "rzh1n2to 3",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = 0,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_RZH1N2TO_3,
		.fmts = rzh1n2to3_out_fmts,
		.num_fmts = ARRAY_SIZE(rzh1n2to3_out_fmts),
		.default_fmt_idx = 0,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = RZH1N2TO3_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = RZH1N2TO3_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_META_OUT_0,
		.name = "partial meta 0",
		.cap = V4L2_CAP_META_CAPTURE,
		.buf_type = V4L2_BUF_TYPE_META_CAPTURE,
		.link_flags = 0,
		.image = false,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_META_STATS_0,
		.fmts = meta_fmts,
		.default_fmt_idx = 1,
		.max_buf_count = 16,
		.ioctl_ops = &mtk_cam_v4l2_meta_cap_ioctl_ops,
	},
	{
		.id = MTK_RAW_META_OUT_1,
		.name = "partial meta 1",
		.cap = V4L2_CAP_META_CAPTURE,
		.buf_type = V4L2_BUF_TYPE_META_CAPTURE,
		.link_flags = 0,
		.image = false,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_META_STATS_1,
		.fmts = meta_fmts,
		.default_fmt_idx = 2,
		.max_buf_count = 16,
		.ioctl_ops = &mtk_cam_v4l2_meta_cap_ioctl_ops,
	},
	{
		.id = MTK_RAW_META_OUT_2,
		.name = "partial meta 2",
		.cap = V4L2_CAP_META_CAPTURE,
		.buf_type = V4L2_BUF_TYPE_META_CAPTURE,
		.link_flags = 0,
		.image = false,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_META_STATS_2,
		.fmts = meta_fmts,
		.default_fmt_idx = 3,
		.max_buf_count = 16,
		.ioctl_ops = &mtk_cam_v4l2_meta_cap_ioctl_ops,
	},
};

static const char *capture_queue_names[RAW_PIPELINE_NUM][MTK_RAW_TOTAL_CAPTURE_QUEUES] = {
	{"mtk-cam raw-0 main-stream",
	 "mtk-cam raw-0 yuvo-1", "mtk-cam raw-0 yuvo-2",
	 "mtk-cam raw-0 yuvo-3", "mtk-cam raw-0 yuvo-4",
	 "mtk-cam raw-0 yuvo-5",
	 "mtk-cam raw-0 drzs4no-1", "mtk-cam raw-0 drzs4no-2", "mtk-cam raw-0 drzs4no-3",
	 "mtk-cam raw-0 rzh1n2to-1", "mtk-cam raw-0 rzh1n2to-2", "mtk-cam raw-0 rzh1n2to-3",
	 "mtk-cam raw-0 partial-meta-0", "mtk-cam raw-0 partial-meta-1",
	 "mtk-cam raw-0 partial-meta-2"},

	{"mtk-cam raw-1 main-stream",
	 "mtk-cam raw-1 yuvo-1", "mtk-cam raw-1 yuvo-2",
	 "mtk-cam raw-1 yuvo-3", "mtk-cam raw-1 yuvo-4",
	 "mtk-cam raw-1 yuvo-5",
	 "mtk-cam raw-1 drzs4no-1", "mtk-cam raw-1 drzs4no-2", "mtk-cam raw-1 drzs4no-3",
	 "mtk-cam raw-1 rzh1n2to-1", "mtk-cam raw-1 rzh1n2to-2", "mtk-cam raw-1 rzh1n2to-3",
	 "mtk-cam raw-1 partial-meta-0", "mtk-cam raw-1 partial-meta-1",
	 "mtk-cam raw-1 partial-meta-2"},
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
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 5);
	if (ret) {
		dev_dbg(dev, "v4l2_ctrl_handler init failed\n");
		return;
	}
	mutex_init(&pipe->res_config.resource_lock);
	mutex_init(&pipe->try_res_config.resource_lock);
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &hwn_limit, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			       V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &frz_limit, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			       V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &bin_limit, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			       V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &hwn, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &frz, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &bin, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &hwn_try, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &frz_try, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &bin_try, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	v4l2_ctrl_new_custom(ctrl_hdlr, &res_plan_policy, NULL);
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &res_pixel_rate, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &frame_sync_id, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			       V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &raw_path, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			       V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;

	v4l2_ctrl_new_std(ctrl_hdlr, &cam_ctrl_ops,
				    V4L2_CID_HBLANK, 0, 65535, 1, 0);
	v4l2_ctrl_new_std(ctrl_hdlr, &cam_ctrl_ops,
				    V4L2_CID_VBLANK, 0, 65535, 1, 0);

	v4l2_ctrl_new_custom(ctrl_hdlr, &mtk_feature, NULL);
	pipe->res_config.hwn_limit = hwn_limit.def;
	pipe->res_config.frz_limit = frz_limit.def;
	pipe->res_config.bin_limit = bin_limit.def;
	pipe->res_config.res_plan = res_plan_policy.def;
	pipe->res_config.raw_feature = mtk_feature.def;
	pipe->sync_id = frame_sync_id.def;
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
			dev_dbg(dev, "invalid pipe id\n");
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

int mtk_raw_setup_dependencies(struct mtk_raw *raw)
{
	struct device *dev = raw->cam_dev;
	struct device *consumer, *supplier;
	struct device_link *link;
	struct mtk_raw_device *raw_dev;
	struct mtk_yuv_device *yuv_dev;
	int i;

	for (i = 0; i < RAW_PIPELINE_NUM; i++) {
		consumer = raw->devs[i];
		supplier = raw->yuvs[i];
		if (!consumer || !supplier) {
			dev_info(dev, "failed to get raw/yuv dev for id %d\n", i);
			continue;
		}

		raw_dev = dev_get_drvdata(consumer);
		yuv_dev = dev_get_drvdata(supplier);
		raw_dev->yuv_base = yuv_dev->base;

		link = device_link_add(consumer, supplier,
				       DL_FLAG_AUTOREMOVE_CONSUMER |
				       DL_FLAG_PM_RUNTIME);
		if (!link) {
			dev_info(dev, "Unable to create link between %s and %s\n",
				 dev_name(consumer), dev_name(supplier));
			return -ENODEV;
		}
	}

	return 0;
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

	pm_runtime_enable(dev);

	return component_add(dev, &mtk_raw_component_ops);
}

static int mtk_raw_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_raw_device *raw_dev = dev_get_drvdata(dev);
	int i;

	dev_info(dev, "%s\n", __func__);

	pm_runtime_disable(dev);
	component_del(dev, &mtk_raw_component_ops);

	for (i = 0; i < raw_dev->num_clks; i++)
		clk_put(raw_dev->clks[i]);

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
	writel(val & (~TG_VFDATA_EN), raw_dev->base + REG_TG_VF_CON);
	ret = readl_poll_timeout_atomic(raw_dev->base + REG_TG_INTER_ST, val,
					(val & TG_CAM_CS_MASK) == TG_IDLE_ST,
					USEC_PER_MSEC, MTK_RAW_STOP_HW_TIMEOUT);
	if (ret)
		dev_dbg(dev, "can't stop HW:%d:0x%x\n", ret, val);

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
	writel(val | TG_VFDATA_EN, raw_dev->base + REG_TG_VF_CON);

	return 0;
}

static int mtk_raw_runtime_suspend(struct device *dev)
{
	struct mtk_raw_device *drvdata = dev_get_drvdata(dev);
	int i;

	dev_dbg(dev, "%s:disable clock\n", __func__);

	for (i = 0; i < drvdata->num_clks; i++)
		clk_disable_unprepare(drvdata->clks[i]);

	return 0;
}

static int mtk_raw_runtime_resume(struct device *dev)
{
	struct mtk_raw_device *drvdata = dev_get_drvdata(dev);
	int i, ret;

	dev_dbg(dev, "%s:enable clock\n", __func__);

	for (i = 0; i < drvdata->num_clks; i++) {
		ret = clk_prepare_enable(drvdata->clks[i]);
		if (ret) {
			dev_info(dev, "enable failed at clk #%d, ret = %d\n",
				 i, ret);
			i--;
			while (i >= 0)
				clk_disable_unprepare(drvdata->clks[i--]);

			return ret;
		}
	}

	reset(drvdata);
	return 0;
}

static const struct dev_pm_ops mtk_raw_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_raw_pm_suspend, mtk_raw_pm_resume)
	SET_RUNTIME_PM_OPS(mtk_raw_runtime_suspend, mtk_raw_runtime_resume,
			   NULL)
};

static const struct of_device_id mtk_raw_of_ids[] = {
	{.compatible = "mediatek,mt8195-cam-raw",},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_raw_of_ids);

struct platform_driver mtk_cam_raw_driver = {
	.probe   = mtk_raw_probe,
	.remove  = mtk_raw_remove,
	.driver  = {
		.name  = "mtk-cam raw",
		.of_match_table = of_match_ptr(mtk_raw_of_ids),
		.pm     = &mtk_raw_pm_ops,
	}
};

static int mtk_yuv_component_bind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_yuv_device *drvdata = dev_get_drvdata(dev);
	struct mtk_cam_device *cam_dev = data;
	struct mtk_raw *raw = &cam_dev->raw;

	dev_info(dev, "%s: id=%d\n", __func__, drvdata->id);
	raw->yuvs[drvdata->id] = dev;

	return 0;
}

static void mtk_yuv_component_unbind(struct device *dev, struct device *master,
				     void *data)
{
	struct mtk_yuv_device *drvdata = dev_get_drvdata(dev);
	struct mtk_cam_device *cam_dev = data;
	struct mtk_raw *raw = &cam_dev->raw;

	dev_info(dev, "%s\n", __func__);
	raw->yuvs[drvdata->id] = NULL;
}

static const struct component_ops mtk_yuv_component_ops = {
	.bind = mtk_yuv_component_bind,
	.unbind = mtk_yuv_component_unbind,
};

static irqreturn_t mtk_irq_yuv(int irq, void *data)
{
	struct mtk_yuv_device *drvdata = (struct mtk_yuv_device *)data;
	struct device *dev = drvdata->dev;

	unsigned int irq_status, err_status, dma_done_status;
	unsigned int drop_status, dma_ofl_status;

	irq_status =
		readl_relaxed(drvdata->base + REG_CTL_RAW_INT_STAT);
	dma_done_status =
		readl_relaxed(drvdata->base + REG_CTL_RAW_INT2_STAT);
	drop_status =
		readl_relaxed(drvdata->base + REG_CTL_RAW_INT4_STAT);
	dma_ofl_status =
		readl_relaxed(drvdata->base + REG_CTL_RAW_INT5_STAT);

	err_status = irq_status & 0x4; // bit2: DMA_ERR

	dev_dbg(dev, "YUV-INT:0x%x(err:0x%x) INT2/4/5 0x%x/0x%x/0x%x\n",
		irq_status, err_status,
		dma_done_status, drop_status, dma_ofl_status);

	return IRQ_HANDLED;
}
static int mtk_yuv_of_probe(struct platform_device *pdev,
			    struct mtk_yuv_device *drvdata)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	unsigned int i;
	int irq, ret;

	ret = of_property_read_u32(dev->of_node, "mediatek,cam-id",
				   &drvdata->id);
	if (ret) {
		dev_dbg(dev, "missing camid property\n");
		return ret;
	}

	/* base outer register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base");
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	drvdata->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(drvdata->base)) {
		dev_dbg(dev, "failed to map register base\n");
		return PTR_ERR(drvdata->base);
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_dbg(dev, "failed to get irq\n");
		return -ENODEV;
	}

	ret = devm_request_irq(dev, irq, mtk_irq_yuv, 0,
			       dev_name(dev), drvdata);
	if (ret) {
		dev_dbg(dev, "failed to request irq=%d\n", irq);
		return ret;
	}
	dev_dbg(dev, "registered irq=%d\n", irq);

	drvdata->num_clks = of_count_phandle_with_args(pdev->dev.of_node, "clocks",
			"#clock-cells");
	dev_info(dev, "clk_num:%d\n", drvdata->num_clks);

	if (!drvdata->num_clks) {
		dev_dbg(dev, "no clock\n");
		return -ENODEV;
	}

	drvdata->clks = devm_kcalloc(dev,
				     drvdata->num_clks, sizeof(*drvdata->clks),
				     GFP_KERNEL);
	if (!drvdata->clks)
		return -ENOMEM;

	for (i = 0; i < drvdata->num_clks; i++) {
		drvdata->clks[i] = of_clk_get(pdev->dev.of_node, i);
		if (IS_ERR(drvdata->clks[i])) {
			dev_info(dev, "failed to get clk %d\n", i);
			return -ENODEV;
		}
	}

	return 0;
}

static int mtk_yuv_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_yuv_device *drvdata;
	int ret;

	dev_info(dev, "%s\n", __func__);

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->dev = dev;
	dev_set_drvdata(dev, drvdata);

	ret = mtk_yuv_of_probe(pdev, drvdata);
	if (ret) {
		dev_info(dev, "mtk_yuv_of_probe failed\n");
		return ret;
	}

	pm_runtime_enable(dev);

	return component_add(dev, &mtk_yuv_component_ops);
}

static int mtk_yuv_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_yuv_device *drvdata = dev_get_drvdata(dev);
	int i;

	dev_info(dev, "%s\n", __func__);

	pm_runtime_disable(dev);
	component_del(dev, &mtk_yuv_component_ops);

	for (i = 0; i < drvdata->num_clks; i++)
		clk_put(drvdata->clks[i]);

	return 0;
}

static int mtk_yuv_pm_suspend(struct device *dev)
{
	int ret;

	dev_dbg(dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	/* Force ISP HW to idle */
	ret = pm_runtime_force_suspend(dev);
	return ret;
}

static int mtk_yuv_pm_resume(struct device *dev)
{
	int ret;

	dev_dbg(dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	/* Force ISP HW to resume */
	ret = pm_runtime_force_resume(dev);
	if (ret)
		return ret;

	return 0;
}

/* driver for yuv part */
static int mtk_yuv_runtime_suspend(struct device *dev)
{
	struct mtk_yuv_device *drvdata = dev_get_drvdata(dev);
	int i;

	dev_dbg(dev, "%s:disable clock\n", __func__);

	for (i = 0; i < drvdata->num_clks; i++)
		clk_disable_unprepare(drvdata->clks[i]);

	return 0;
}

static int mtk_yuv_runtime_resume(struct device *dev)
{
	struct mtk_yuv_device *drvdata = dev_get_drvdata(dev);
	int i, ret;

	dev_dbg(dev, "%s:enable clock\n", __func__);

	for (i = 0; i < drvdata->num_clks; i++) {
		ret = clk_prepare_enable(drvdata->clks[i]);
		if (ret) {
			dev_info(dev, "enable failed at clk #%d, ret = %d\n",
				 i, ret);
			i--;
			while (i >= 0)
				clk_disable_unprepare(drvdata->clks[i--]);

			return ret;
		}
	}

	return 0;
}


static const struct dev_pm_ops mtk_yuv_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_yuv_pm_suspend, mtk_yuv_pm_resume)
	SET_RUNTIME_PM_OPS(mtk_yuv_runtime_suspend, mtk_yuv_runtime_resume,
			   NULL)
};

static const struct of_device_id mtk_yuv_of_ids[] = {
	{.compatible = "mediatek,mt8195-cam-yuv",},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_yuv_of_ids);

struct platform_driver mtk_cam_yuv_driver = {
	.probe   = mtk_yuv_probe,
	.remove  = mtk_yuv_remove,
	.driver  = {
		.name  = "mtk-cam yuv",
		.of_match_table = of_match_ptr(mtk_yuv_of_ids),
		.pm     = &mtk_yuv_pm_ops,
	}
};
