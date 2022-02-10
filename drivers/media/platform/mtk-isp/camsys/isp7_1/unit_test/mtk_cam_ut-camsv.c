// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

#include "mtk_cam_ut.h"
#include "mtk_cam_ut-engines.h"
#include "../cam/mtk_cam-sv-regs.h"

#define CAMSV_WRITE_BITS(RegAddr, RegName, FieldName, FieldValue) do {\
	union RegName reg;\
	\
	reg.Raw = readl_relaxed(RegAddr);\
	reg.Bits.FieldName = FieldValue;\
	writel_relaxed(reg.Raw, RegAddr);\
} while (0)

#define CAMSV_WRITE_REG(RegAddr, RegValue) ({\
	writel_relaxed(RegValue, RegAddr);\
})

#define CAMSV_READ_BITS(RegAddr, RegName, FieldName) ({\
	union RegName reg;\
	\
	reg.Raw = readl_relaxed(RegAddr);\
	reg.Bits.FieldName;\
})

#define CAMSV_READ_REG(RegAddr) ({\
	unsigned int var;\
	\
	var = readl_relaxed(RegAddr);\
	var;\
})

enum camsv_db_load_src {
	SV_DB_SRC_SUB_P1_DONE = 0,
	SV_DB_SRC_SOF         = 1,
	SV_DB_SRC_SUB_SOF     = 2,
};

enum camsv_int_en {
	SV_INT_EN_VS1_INT_EN               = (1L<<0),
	SV_INT_EN_TG_INT1_EN               = (1L<<1),
	SV_INT_EN_TG_INT2_EN               = (1L<<2),
	SV_INT_EN_EXPDON1_INT_EN           = (1L<<3),
	SV_INT_EN_TG_ERR_INT_EN            = (1L<<4),
	SV_INT_EN_TG_GBERR_INT_EN          = (1L<<5),
	SV_INT_EN_TG_SOF_INT_EN            = (1L<<6),
	SV_INT_EN_TG_WAIT_INT_EN           = (1L<<7),
	SV_INT_EN_TG_DROP_INT_EN           = (1L<<8),
	SV_INT_EN_VS_INT_ORG_EN            = (1L<<9),
	SV_INT_EN_DB_LOAD_ERR_EN           = (1L<<10),
	SV_INT_EN_PASS1_DON_INT_EN         = (1L<<11),
	SV_INT_EN_SW_PASS1_DON_INT_EN      = (1L<<12),
	SV_INT_EN_SUB_PASS1_DON_INT_EN     = (1L<<13),
	SV_INT_EN_UFEO_OVERR_INT_EN        = (1L<<15),
	SV_INT_EN_DMA_ERR_INT_EN           = (1L<<16),
	SV_INT_EN_IMGO_OVERR_INT_EN        = (1L<<17),
	SV_INT_EN_UFEO_DROP_INT_EN         = (1L<<18),
	SV_INT_EN_IMGO_DROP_INT_EN         = (1L<<19),
	SV_INT_EN_IMGO_DONE_INT_EN         = (1L<<20),
	SV_INT_EN_UFEO_DONE_INT_EN         = (1L<<21),
	SV_INT_EN_TG_INT3_EN               = (1L<<22),
	SV_INT_EN_TG_INT4_EN               = (1L<<23),
	SV_INT_EN_SW_ENQUE_ERR             = (1L<<24),
	SV_INT_EN_INT_WCLR_EN              = (1L<<31),
};

enum camsv_tg_fmt {
	SV_TG_FMT_RAW8      = 0,
	SV_TG_FMT_RAW10     = 1,
	SV_TG_FMT_RAW12     = 2,
	SV_TG_FMT_YUV422    = 3,
	SV_TG_FMT_RAW14     = 4,
	SV_TG_FMT_RSV1      = 5,
	SV_TG_FMT_RSV2      = 6,
	SV_TG_FMT_JPG       = 7,
};

enum camsv_tg_swap {
	TG_SW_UYVY = 0,
	TG_SW_YUYV = 1,
	TG_SW_VYUY = 2,
	TG_SW_YVYU = 3,
};

unsigned int ut_mtk_cam_sv_xsize_cal(
	struct mtkcam_ipi_input_param *cfg_in_param)
{

	union CAMSV_FMT_SEL fmt;
	unsigned int size;
	unsigned int divisor;

	fmt.Raw = cfg_in_param->fmt;

	switch (fmt.Bits.TG1_FMT) {
	case SV_TG_FMT_RAW8:
		size = cfg_in_param->in_crop.s.w;
		break;
	case SV_TG_FMT_RAW10:
		size = (cfg_in_param->in_crop.s.w * 10) / 8;
		break;
	case SV_TG_FMT_RAW12:
		size = (cfg_in_param->in_crop.s.w * 12) / 8;
		break;
	case SV_TG_FMT_RAW14:
		size = (cfg_in_param->in_crop.s.w * 14) / 8;
		break;
	default:
		return 0;
	}

	switch (cfg_in_param->pixel_mode) {
	case 0:
		divisor = 0x1;
		break;
	case 1:
		divisor = 0x3;
		break;
	case 2:
		divisor = 0x7;
		break;
	case 3:
		divisor = 0xF;
		break;
	default:
		return 0;
	}
	size = ((size + divisor) & ~divisor);
	return size;
}

void ut_sv_reset(struct device *dev)
{
	unsigned long end = jiffies + msecs_to_jiffies(100);
	struct mtk_ut_camsv_device *sv_dev = dev_get_drvdata(dev);

	dev_dbg(dev, "%s\n", __func__);

	writel_relaxed(0, sv_dev->base + REG_CAMSV_SW_CTL);
	writel_relaxed(1, sv_dev->base + REG_CAMSV_SW_CTL);
	wmb(); /* TBC */

	while (time_before(jiffies, end)) {
		if (readl(sv_dev->base + REG_CAMSV_SW_CTL) & 0x2) {
			// do hw rst
			writel_relaxed(4, sv_dev->base + REG_CAMSV_SW_CTL);
			writel_relaxed(0, sv_dev->base + REG_CAMSV_SW_CTL);
			wmb(); /* TBC */
			return;
		}

		dev_info(dev,
			"tg_sen_mode: 0x%x, ctl_en: 0x%x, ctl_sw_ctl:0x%x, frame_no:0x%x\n",
			readl(sv_dev->base + REG_CAMSV_TG_SEN_MODE),
			readl(sv_dev->base + REG_CAMSV_MODULE_EN),
			readl(sv_dev->base + REG_CAMSV_SW_CTL),
			readl(sv_dev->base + REG_CAMSV_FRAME_SEQ_NO)
			);
		usleep_range(10, 20);
	}

	dev_dbg(dev, "reset hw timeout\n");
}

int ut_mtk_cam_sv_convert_fmt(unsigned int ipi_fmt)
{
	union CAMSV_FMT_SEL fmt;

	fmt.Raw = 0;
	fmt.Bits.TG1_SW = TG_SW_UYVY;

	switch (ipi_fmt) {
	case MTKCAM_IPI_IMG_FMT_BAYER8:
		fmt.Bits.TG1_FMT = SV_TG_FMT_RAW8;
		break;
	case MTKCAM_IPI_IMG_FMT_BAYER10:
		fmt.Bits.TG1_FMT = SV_TG_FMT_RAW10;
		break;
	case MTKCAM_IPI_IMG_FMT_BAYER12:
		fmt.Bits.TG1_FMT = SV_TG_FMT_RAW12;
		break;
	case MTKCAM_IPI_IMG_FMT_BAYER14:
		fmt.Bits.TG1_FMT = SV_TG_FMT_RAW14;
		break;
	default:
		break;
	}

	return fmt.Raw;
}

int ut_mtk_cam_sv_tg_config(
	struct device *dev, struct mtkcam_ipi_input_param *cfg_in_param)
{
	int ret = 0;
	unsigned int pxl, lin;
	struct mtk_ut_camsv_device *sv_dev = dev_get_drvdata(dev);

	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_SEN_MODE,
		CAMSV_TG_SEN_MODE, CMOS_EN, 0);

	/* subsample */
	if (cfg_in_param->subsample > 0) {
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_SEN_MODE,
			CAMSV_TG_SEN_MODE, SOF_SUB_EN, 1);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_SEN_MODE,
			CAMSV_TG_SEN_MODE, VS_SUB_EN, 1);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_SUB_PERIOD,
			CAMSV_TG_SUB_PERIOD, VS_PERIOD,
			cfg_in_param->subsample);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_SUB_PERIOD,
			CAMSV_TG_SUB_PERIOD, SOF_PERIOD,
			cfg_in_param->subsample);
	} else {
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_SEN_MODE,
			CAMSV_TG_SEN_MODE, SOF_SUB_EN, 0);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_SEN_MODE,
			CAMSV_TG_SEN_MODE, VS_SUB_EN, 0);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_SUB_PERIOD,
			CAMSV_TG_SUB_PERIOD, VS_PERIOD,
			cfg_in_param->subsample);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_SUB_PERIOD,
			CAMSV_TG_SUB_PERIOD, SOF_PERIOD,
			cfg_in_param->subsample);
	}

	if (sv_dev->exp_order == 0) {
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_SEN_MODE,
			CAMSV_TG_SEN_MODE, STAGGER_EN, 0);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_PATH_CFG,
			CAMSV_TG_PATH_CFG, SUB_SOF_SRC_SEL, 0);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_PATH_CFG,
			CAMSV_TG_PATH_CFG, DB_LOAD_DIS, 1);
	} else {
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_SEN_MODE,
			CAMSV_TG_SEN_MODE, STAGGER_EN, 1);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_PATH_CFG,
			CAMSV_TG_PATH_CFG, SUB_SOF_SRC_SEL, 0);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_PATH_CFG,
			CAMSV_TG_PATH_CFG, DB_LOAD_DIS, 1);
	}

	/* timestamp */
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_SEN_MODE,
		CAMSV_TG_SEN_MODE, TIME_STP_EN, 1);

	/* trig mode */
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_VF_CON,
		CAMSV_TG_VF_CON, SINGLE_MODE, 0);

	/* pixel mode */
	switch (cfg_in_param->pixel_mode) {
	case 0:
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_SEN_MODE,
			CAMSV_TG_SEN_MODE, DBL_DATA_BUS, 0);
		break;
	case 1:
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_SEN_MODE,
			CAMSV_TG_SEN_MODE, DBL_DATA_BUS, 1);
		break;
	case 2:
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_SEN_MODE,
			CAMSV_TG_SEN_MODE, DBL_DATA_BUS, 2);
		break;
	case 3:
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_SEN_MODE,
			CAMSV_TG_SEN_MODE, DBL_DATA_BUS, 3);
		break;
	default:
		dev_dbg(dev, "unknown pixel mode(%d)",
			cfg_in_param->pixel_mode);
		ret = -1;
		goto EXIT;
	}

	/* grab size */
	pxl = ((cfg_in_param->in_crop.s.w+cfg_in_param->in_crop.p.x)<<16)|
			cfg_in_param->in_crop.p.x;
	lin = ((cfg_in_param->in_crop.s.h+cfg_in_param->in_crop.p.y)<<16)|
			cfg_in_param->in_crop.p.y;
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_TG_SEN_GRAB_PXL, pxl);
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_TG_SEN_GRAB_LIN, lin);

	dev_info(dev, "pixel mode:%d\n", cfg_in_param->pixel_mode);
	dev_info(dev, "sub-sample:%d\n", cfg_in_param->subsample);
	dev_info(dev, "fmt:%d\n", cfg_in_param->fmt);
	dev_info(dev, "crop_x:%d\n", cfg_in_param->in_crop.p.x);
	dev_info(dev, "crop_y:%d\n", cfg_in_param->in_crop.p.y);
	dev_info(dev, "crop_w:%d\n", cfg_in_param->in_crop.s.w);
	dev_info(dev, "crop_h:%d\n", cfg_in_param->in_crop.s.h);

EXIT:
	return ret;
}

int ut_mtk_cam_sv_top_config(
	struct device *dev,
	struct mtkcam_ipi_input_param *cfg_in_param)
{
	unsigned int int_en =
		(SV_INT_EN_VS1_INT_EN |
		SV_INT_EN_TG_ERR_INT_EN |
		SV_INT_EN_TG_GBERR_INT_EN |
		SV_INT_EN_TG_SOF_INT_EN |
		SV_INT_EN_PASS1_DON_INT_EN |
		SV_INT_EN_SW_PASS1_DON_INT_EN |
		SV_INT_EN_DMA_ERR_INT_EN |
		SV_INT_EN_IMGO_OVERR_INT_EN);
	union CAMSV_FMT_SEL fmt;
	int ret = 0;
	struct mtk_ut_camsv_device *sv_dev = dev_get_drvdata(dev);

	/* reset */
	ut_sv_reset(dev);

	/* fun en */
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, TG_EN, 1);
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, DB_EN, 0);
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, DB_LOAD_SRC, SV_DB_SRC_SUB_SOF);

	/* central sub en */
	if (cfg_in_param->subsample > 0)
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_SUB_CTRL,
			CAMSV_SUB_CTRL, CENTRAL_SUB_EN, 1);
	else
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_SUB_CTRL,
			CAMSV_SUB_CTRL, CENTRAL_SUB_EN, 0);

	/* disable db load mask for non-dcif case */
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_DCIF_SET,
		CAMSV_DCIF_SET, MASK_DB_LOAD, 0);

	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_DCIF_SET,
		CAMSV_DCIF_SET, FOR_DCIF_SUBSAMPLE_EN, 1);
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_DCIF_SET,
		CAMSV_DCIF_SET, ENABLE_OUTPUT_CQ_START_SIGNAL, 1);

	/* vf en chain */
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_MISC,
		CAMSV_MISC, VF_SRC, 0);
	if (sv_dev->id == 8 || sv_dev->id == 9) {
		if (sv_dev->exp_order == 2)
			CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_MISC,
				CAMSV_MISC, VF_SRC, 1);
	}

	/* fmt sel */
	fmt.Raw = ut_mtk_cam_sv_convert_fmt(cfg_in_param->fmt);
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_FMT_SEL, fmt.Raw);

	/* int en */
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_INT_EN, int_en);

	/* sub p1 done */
	if (cfg_in_param->subsample > 0) {
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_MODULE_EN,
			CAMSV_MODULE_EN, DOWN_SAMPLE_EN, 1);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_MODULE_EN,
			CAMSV_MODULE_EN, DOWN_SAMPLE_PERIOD,
			cfg_in_param->subsample);
	} else {
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_MODULE_EN,
			CAMSV_MODULE_EN, DOWN_SAMPLE_EN, 0);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_MODULE_EN,
			CAMSV_MODULE_EN, DOWN_SAMPLE_PERIOD,
			cfg_in_param->subsample);
	}

	switch (fmt.Bits.TG1_FMT) {
	case SV_TG_FMT_RAW8:
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_MODULE_EN,
			CAMSV_MODULE_EN, PAK_EN, 1);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_MODULE_EN,
			CAMSV_MODULE_EN, PAK_SEL, 0);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_PAK,
			CAMSV_PAK, PAK_MODE, 128);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_PAK_CON,
			CAMSV_PAK_CON, PAK_IN_BIT, 14);
		break;
	case SV_TG_FMT_RAW10:
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_MODULE_EN,
			CAMSV_MODULE_EN, PAK_EN, 1);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_MODULE_EN,
			CAMSV_MODULE_EN, PAK_SEL, 0);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_PAK,
			CAMSV_PAK, PAK_MODE, 129);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_PAK_CON,
			CAMSV_PAK_CON, PAK_IN_BIT, 14);
		break;
	case SV_TG_FMT_RAW12:
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_MODULE_EN,
			CAMSV_MODULE_EN, PAK_EN, 1);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_MODULE_EN,
			CAMSV_MODULE_EN, PAK_SEL, 0);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_PAK,
			CAMSV_PAK, PAK_MODE, 130);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_PAK_CON,
			CAMSV_PAK_CON, PAK_IN_BIT, 14);
		break;
	default:
		dev_dbg(dev, "unknown tg format(%d)", fmt.Bits.TG1_FMT);
		ret = -1;
		goto EXIT;
	}

	/* ufe disable */
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, UFE_EN, 0);

	/* pixel mode */
	switch (cfg_in_param->pixel_mode) {
	case 0:
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_PAK,
			CAMSV_PAK, PAK_DBL_MODE, 0);
		break;
	case 1:
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_PAK,
			CAMSV_PAK, PAK_DBL_MODE, 1);
		break;
	case 2:
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_PAK,
			CAMSV_PAK, PAK_DBL_MODE, 2);
		break;
	case 3:
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_PAK,
			CAMSV_PAK, PAK_DBL_MODE, 3);
		break;
	default:
		dev_dbg(dev, "unknown pixel mode(%d)",
			cfg_in_param->pixel_mode);
		ret = -1;
		goto EXIT;
	}

	/* dma performance */
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_SPECIAL_FUN_EN, 0x4000000);

EXIT:
	return ret;
}

int ut_mtk_cam_sv_dmao_config(
	struct device *dev,
	struct mtkcam_ipi_input_param *cfg_in_param)
{
	int ret = 0;
	unsigned int stride;
	struct mtk_ut_camsv_device *sv_dev = dev_get_drvdata(dev);

	/* imgo dma setting */
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_IMGO_XSIZE,
		ut_mtk_cam_sv_xsize_cal(cfg_in_param) - 1);
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_IMGO_YSIZE,
		cfg_in_param->in_crop.s.h - 1);
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_IMGO_STRIDE,
		ut_mtk_cam_sv_xsize_cal(cfg_in_param));

	dev_info(dev, "xsize:%d\n",
		CAMSV_READ_REG(sv_dev->base + REG_CAMSV_IMGO_XSIZE));
	dev_info(dev, "ysize:%d\n",
		CAMSV_READ_REG(sv_dev->base + REG_CAMSV_IMGO_YSIZE));
	dev_info(dev, "stride:%d\n",
		CAMSV_READ_REG(sv_dev->base + REG_CAMSV_IMGO_STRIDE));

	/* imgo crop */
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_IMGO_CROP, 0);

	/* imgo stride */
	stride = CAMSV_READ_REG(sv_dev->base + REG_CAMSV_IMGO_STRIDE);
	switch (cfg_in_param->pixel_mode) {
	case 0:
		stride = stride | (1<<27) | (1<<16);
		CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_IMGO_STRIDE, stride);
		break;
	case 1:
		stride = stride | (1<<27) | (3<<16);
		CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_IMGO_STRIDE, stride);
		break;
	case 2:
		stride = stride | (1<<27) | (7<<16);
		CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_IMGO_STRIDE, stride);
		break;
	case 3:
		stride = stride | (1<<27) | (15<<16);
		CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_IMGO_STRIDE, stride);
		break;
	default:
		dev_dbg(dev, "unknown pixel mode(%d)",
			cfg_in_param->pixel_mode);
		ret = -1;
		goto EXIT;
	}

	/* imgo con */
	if (sv_dev->id >= 0 && sv_dev->id < 10) {
		CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_IMGO_CON0, 0x10000300);
		CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_IMGO_CON1, 0x00C00060);
		CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_IMGO_CON2, 0x01800120);
		CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_IMGO_CON3, 0x820001A0);
		CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_IMGO_CON4, 0x812000C0);
	} else {
		CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_IMGO_CON0, 0x10000080);
		CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_IMGO_CON1, 0x00200010);
		CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_IMGO_CON2, 0x00400030);
		CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_IMGO_CON3, 0x80550045);
		CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_IMGO_CON4, 0x80300020);
	}

EXIT:
	return ret;
}

int ut_mtk_cam_sv_fbc_config(
	struct device *dev,
	struct mtkcam_ipi_input_param *cfg_in_param)
{
	int ret = 0;
	struct mtk_ut_camsv_device *sv_dev = dev_get_drvdata(dev);

	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_FBC_IMGO_CTL1, 0);

	return ret;
}

int ut_mtk_cam_sv_tg_enable(
	struct device *dev,
	struct mtkcam_ipi_input_param *cfg_in_param)
{
	int ret = 0;
	struct mtk_ut_camsv_device *sv_dev = dev_get_drvdata(dev);

	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_SEN_MODE,
		CAMSV_TG_SEN_MODE, CMOS_EN, 1);

	return ret;
}

int ut_mtk_cam_sv_top_enable(struct device *dev)
{
	int ret = 0;
	struct mtk_ut_camsv_device *sv_dev = dev_get_drvdata(dev);

	if (CAMSV_READ_BITS(sv_dev->base + REG_CAMSV_TG_SEN_MODE,
		CAMSV_TG_SEN_MODE, CMOS_EN)) {
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_VF_CON,
			CAMSV_TG_VF_CON, VFDATA_EN, 1);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_PATH_CFG,
			CAMSV_TG_PATH_CFG, DB_LOAD_DIS, 0);
	}

	return ret;
}

int ut_mtk_cam_sv_dmao_enable(
	struct device *dev,
	struct mtkcam_ipi_input_param *cfg_in_param)
{
	int ret = 0;
	struct mtk_ut_camsv_device *sv_dev = dev_get_drvdata(dev);

	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, IMGO_EN, 1);

	return ret;
}

int ut_mtk_cam_sv_fbc_enable(
	struct device *dev,
	struct mtkcam_ipi_input_param *cfg_in_param)
{
	int ret = 0;
	struct mtk_ut_camsv_device *sv_dev = dev_get_drvdata(dev);

	if (CAMSV_READ_BITS(sv_dev->base + REG_CAMSV_TG_VF_CON,
			CAMSV_TG_VF_CON, VFDATA_EN) == 1) {
		ret = -1;
		dev_dbg(dev, "cannot enable fbc when streaming");
		goto EXIT;
	}
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_FBC_IMGO_CTL1,
		CAMSV_FBC_IMGO_CTL1, SUB_RATIO, cfg_in_param->subsample);
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_FBC_IMGO_CTL1,
		CAMSV_FBC_IMGO_CTL1, FBC_EN, 1);
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_FBC_IMGO_CTL1,
		CAMSV_FBC_IMGO_CTL1, FBC_DB_EN, 0);

EXIT:
	return ret;
}

int ut_mtk_cam_sv_tg_disable(
	struct device *dev)
{
	int ret = 0;
	struct mtk_ut_camsv_device *sv_dev = dev_get_drvdata(dev);

	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_SEN_MODE,
		CAMSV_TG_SEN_MODE, CMOS_EN, 0);

	return ret;
}

int ut_mtk_cam_sv_top_disable(struct device *dev)
{
	int ret = 0;
	struct mtk_ut_camsv_device *sv_dev = dev_get_drvdata(dev);

	if (CAMSV_READ_BITS(sv_dev->base + REG_CAMSV_TG_VF_CON,
			CAMSV_TG_VF_CON, VFDATA_EN)) {
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_TG_VF_CON,
			CAMSV_TG_VF_CON, VFDATA_EN, 0);
		ut_sv_reset(dev);
	}
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, DB_EN, 0);
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_MODULE_EN, 0);
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_FMT_SEL, 0);
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_INT_EN, 0);
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_FBC_IMGO_CTL1, 0);
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, DB_EN, 1);

	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_CLK_EN,
		CAMSV_CLK_EN, TG_DP_CK_EN, 0);
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_CLK_EN,
		CAMSV_CLK_EN, QBN_DP_CK_EN, 0);
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_CLK_EN,
		CAMSV_CLK_EN, PAK_DP_CK_EN, 0);
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_CLK_EN,
		CAMSV_CLK_EN, IMGO_DP_CK_EN, 0);

	return ret;
}

int ut_mtk_cam_sv_dmao_disable(
	struct device *dev)
{
	int ret = 0;
	struct mtk_ut_camsv_device *sv_dev = dev_get_drvdata(dev);

	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, IMGO_EN, 0);

	return ret;
}

int ut_mtk_cam_sv_fbc_disable(
	struct device *dev)
{
	int ret = 0;
	struct mtk_ut_camsv_device *sv_dev = dev_get_drvdata(dev);

	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSV_FBC_IMGO_CTL1, 0);

	return ret;
}

int ut_mtk_cam_sv_toggle_db(
	struct device *dev)
{
	int ret = 0;
	struct mtk_ut_camsv_device *sv_dev = dev_get_drvdata(dev);

	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_CLK_EN,
		CAMSV_CLK_EN, TG_DP_CK_EN, 1);
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_CLK_EN,
		CAMSV_CLK_EN, QBN_DP_CK_EN, 1);
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_CLK_EN,
		CAMSV_CLK_EN, PAK_DP_CK_EN, 1);
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_CLK_EN,
		CAMSV_CLK_EN, IMGO_DP_CK_EN, 1);

	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, DB_EN, 0);
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, DB_EN, 1);

	return ret;
}

static int ut_camsv_initialize(struct device *dev, void *ext_params)
{
	return 0;
}

static int ut_camsv_reset(struct device *dev)
{
	ut_sv_reset(dev);
	return 0;
}

static int ut_camsv_s_stream(struct device *dev, int on)
{
	int ret = 0;

	dev_info(dev, "%s: %s\n", __func__, on ? "on" : "off");

	if (on)
		ret = ut_mtk_cam_sv_top_enable(dev);
	else {
		ret = ut_mtk_cam_sv_top_disable(dev) ||
			ut_mtk_cam_sv_fbc_disable(dev) ||
			ut_mtk_cam_sv_dmao_disable(dev) ||
			ut_mtk_cam_sv_tg_disable(dev);
	}

	return ret;
}

static int ut_camsv_dev_config(struct device *dev,
	struct mtkcam_ipi_input_param *cfg_in_param)
{
	int ret = 0;

	ret = ut_mtk_cam_sv_tg_config(dev, cfg_in_param) ||
		ut_mtk_cam_sv_top_config(dev, cfg_in_param) ||
		ut_mtk_cam_sv_dmao_config(dev, cfg_in_param) ||
		ut_mtk_cam_sv_fbc_config(dev, cfg_in_param) ||
		ut_mtk_cam_sv_tg_enable(dev, cfg_in_param) ||
		ut_mtk_cam_sv_dmao_enable(dev, cfg_in_param) ||
		ut_mtk_cam_sv_fbc_enable(dev, cfg_in_param) ||
		ut_mtk_cam_sv_toggle_db(dev);

	return ret;
}

static void ut_camsv_set_ops(struct device *dev)
{
	struct mtk_ut_camsv_device *sv_dev = dev_get_drvdata(dev);

	sv_dev->ops.initialize = ut_camsv_initialize;
	sv_dev->ops.reset = ut_camsv_reset;
	sv_dev->ops.s_stream = ut_camsv_s_stream;
	sv_dev->ops.dev_config = ut_camsv_dev_config;
}

static int mtk_ut_camsv_component_bind(struct device *dev,
				     struct device *master, void *data)
{
	struct mtk_ut_camsv_device *sv_dev = dev_get_drvdata(dev);
	struct mtk_cam_ut *ut = data;
	struct ut_event evt;

	dev_info(dev, "%s\n", __func__);

	if (!data) {
		dev_info(dev, "no master data\n");
		return -1;
	}

	if (!ut->camsv) {
		dev_info(dev, "no camsv arr, num of camsv %d\n", ut->num_camsv);
		return -1;
	}
	ut->camsv[sv_dev->id] = dev;

	evt.mask = EVENT_SV_SOF;
	add_listener(&sv_dev->event_src, &ut->listener, evt);

	return 0;
}

static void mtk_ut_camsv_component_unbind(struct device *dev,
					struct device *master, void *data)
{
	struct mtk_ut_camsv_device *sv_dev = dev_get_drvdata(dev);
	struct mtk_cam_ut *ut = data;

	dev_dbg(dev, "%s\n", __func__);
	ut->camsv[sv_dev->id] = NULL;
	remove_listener(&sv_dev->event_src, &ut->listener);
}

static const struct component_ops mtk_ut_camsv_component_ops = {
	.bind = mtk_ut_camsv_component_bind,
	.unbind = mtk_ut_camsv_component_unbind,
};

static irqreturn_t mtk_ut_camsv_irq(int irq, void *data)
{
	struct mtk_ut_camsv_device *camsv = data;
	struct ut_event event;
	unsigned int irq_status, fbc_imgo_ctl2;

	irq_status = readl_relaxed(camsv->base + REG_CAMSV_INT_STATUS);
	fbc_imgo_ctl2 = readl_relaxed(camsv->base + REG_CAMSV_FBC_IMGO_CTL2);

	event.mask = 0;

	if (irq_status & CAMSV_INT_TG_SOF_INT_ST)
		event.mask |= EVENT_SV_SOF;

	if (event.mask && ((camsv->exp_order == 0 && camsv->exp_num > 1) ||
		(camsv->exp_num == 1))) {
		dev_dbg(camsv->dev, "send event 0x%x\n", event.mask);
		send_event(&camsv->event_src, event);
	}

	dev_info(camsv->dev, "irq_status:0x%x/fbc_imgo_ctl2:0x%x",
		irq_status, fbc_imgo_ctl2);

	return IRQ_HANDLED;
}

static int mtk_ut_camsv_of_probe(struct platform_device *pdev,
			    struct mtk_ut_camsv_device *camsv)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	int irq, clks, ret;
	int i;

	ret = of_property_read_u32(dev->of_node, "mediatek,camsv-id",
				   &camsv->id);
	dev_info(dev, "id = %d\n", camsv->id);
	if (ret) {
		dev_info(dev, "missing camsvid property\n");
		return ret;
	}

	ret = of_property_read_u32(dev->of_node, "mediatek,camsv-hwcap",
						       &camsv->hw_cap);
	if (ret) {
		dev_dbg(dev, "missing hardware capability property\n");
		return ret;
	}

	ret = of_property_read_u32(dev->of_node, "mediatek,cammux-id",
						       &camsv->cammux_id);
	if (ret) {
		dev_dbg(dev, "missing cammux id property\n");
		return ret;
	}

	/* base outer register */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	camsv->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(camsv->base)) {
		dev_info(dev, "failed to map register base\n");
		return PTR_ERR(camsv->base);
	}
	dev_dbg(dev, "camsv, map_addr=0x%pK\n", camsv->base);

	/* base inner register */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	camsv->base_inner = devm_ioremap_resource(dev, res);
	if (IS_ERR(camsv->base_inner)) {
		dev_info(dev, "failed to map register inner base\n");
		return PTR_ERR(camsv->base_inner);
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_info(dev, "failed to get irq\n");
		return -ENODEV;
	}

	ret = devm_request_irq(dev, irq, mtk_ut_camsv_irq, 0,
			       dev_name(dev), camsv);
	if (ret) {
		dev_info(dev, "failed to request irq=%d\n", irq);
		return ret;
	}
	dev_dbg(dev, "registered irq=%d\n", irq);

	clks = of_count_phandle_with_args(pdev->dev.of_node,
				"clocks", "#clock-cells");

	camsv->num_clks = (clks == -ENOENT) ? 0:clks;
	dev_info(dev, "clk_num:%d\n", camsv->num_clks);

	if (camsv->num_clks) {
		camsv->clks = devm_kcalloc(dev, camsv->num_clks,
						sizeof(*camsv->clks), GFP_KERNEL);
		if (!camsv->clks)
			return -ENOMEM;
	}

	for (i = 0; i < camsv->num_clks; i++) {
		camsv->clks[i] = of_clk_get(pdev->dev.of_node, i);
		if (IS_ERR(camsv->clks[i])) {
			dev_info(dev, "failed to get clk %d\n", i);
			return -ENODEV;
		}
	}

	return 0;
}

static int mtk_ut_camsv_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_ut_camsv_device *camsv;
	int ret;

	dev_info(dev, "%s\n", __func__);

	camsv = devm_kzalloc(dev, sizeof(*camsv), GFP_KERNEL);
	if (!camsv)
		return -ENOMEM;

	camsv->dev = dev;
	dev_set_drvdata(dev, camsv);

	ret = mtk_ut_camsv_of_probe(pdev, camsv);
	if (ret)
		return ret;

	init_event_source(&camsv->event_src);
	ut_camsv_set_ops(dev);

	pm_runtime_enable(dev);

	ret = component_add(dev, &mtk_ut_camsv_component_ops);
	if (ret)
		return ret;

	dev_info(dev, "%s: success\n", __func__);
	return 0;
}

static int mtk_ut_camsv_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_ut_camsv_device *camsv = dev_get_drvdata(dev);
	int i;

	dev_info(dev, "%s\n", __func__);

	for (i = 0; i < camsv->num_clks; i++) {
		if (camsv->clks[i])
			clk_put(camsv->clks[i]);
	}

	pm_runtime_disable(dev);

	component_del(dev, &mtk_ut_camsv_component_ops);
	return 0;
}

static int mtk_ut_camsv_pm_suspend(struct device *dev)
{
	dev_dbg(dev, "- %s\n", __func__);
	return 0;
}

static int mtk_ut_camsv_pm_resume(struct device *dev)
{
	dev_dbg(dev, "- %s\n", __func__);
	return 0;
}

static int mtk_ut_camsv_runtime_suspend(struct device *dev)
{
	struct mtk_ut_camsv_device *camsv = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < camsv->num_clks; i++)
		clk_disable_unprepare(camsv->clks[i]);

	return 0;
}

static int mtk_ut_camsv_runtime_resume(struct device *dev)
{
	struct mtk_ut_camsv_device *camsv = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < camsv->num_clks; i++)
		clk_prepare_enable(camsv->clks[i]);

	return 0;
}

static const struct dev_pm_ops mtk_ut_camsv_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_ut_camsv_pm_suspend,
							mtk_ut_camsv_pm_resume)
	SET_RUNTIME_PM_OPS(mtk_ut_camsv_runtime_suspend,
						mtk_ut_camsv_runtime_resume,
						NULL)
};

static const struct of_device_id mtk_ut_camsv_of_ids[] = {
	{.compatible = "mediatek,camsv",},
	{}
};

MODULE_DEVICE_TABLE(of, mtk_ut_camsv_of_ids);

struct platform_driver mtk_ut_camsv_driver = {
	.probe   = mtk_ut_camsv_probe,
	.remove  = mtk_ut_camsv_remove,
	.driver  = {
		.name  = "mtk-cam camsv-ut",
		.of_match_table = of_match_ptr(mtk_ut_camsv_of_ids),
		.pm     = &mtk_ut_camsv_pm_ops,
	}
};

