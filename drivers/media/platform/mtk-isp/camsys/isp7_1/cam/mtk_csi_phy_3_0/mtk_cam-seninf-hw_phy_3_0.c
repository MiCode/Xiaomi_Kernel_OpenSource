// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <linux/module.h>
#include <linux/delay.h>

#include "mtk_cam-seninf.h"
#include "mtk_cam-seninf-hw.h"
#include "mtk_cam-seninf-regs.h"
#include "mtk_csi_phy_3_0/mtk_cam-seninf-top-ctrl.h"
#include "mtk_csi_phy_3_0/mtk_cam-seninf-seninf1-mux.h"
#include "mtk_csi_phy_3_0/mtk_cam-seninf-seninf1.h"
#include "mtk_csi_phy_3_0/mtk_cam-seninf-seninf1-csi2.h"
#include "mtk_csi_phy_3_0/mtk_cam-seninf-tg1.h"
#include "mtk_csi_phy_3_0/mtk_cam-seninf-cammux-gcsr.h"
#include "mtk_csi_phy_3_0/mtk_cam-seninf-cammux-pcsr.h"
#include "mtk_csi_phy_3_0/mtk_cam-seninf-mipi-rx-ana-cdphy-csi0a.h"
#include "mtk_csi_phy_3_0/mtk_cam-seninf-csi0-cphy.h"
#include "mtk_csi_phy_3_0/mtk_cam-seninf-csi0-dphy.h"

#include "mtk_cam-seninf-route.h"
#include "imgsensor-user.h"
#define __SMT 0
#define SENINF_CK 273000000
#define CYCLE_MARGIN 1
#define RESYNC_DMY_CNT 4
#define FIX_DPHY_SETTLE 1
#define DPHY_SETTLE 0x1C
#define CPHY_SETTLE 0x16 //60~80ns

#define DEBUG_CAM_MUX_SWITCH 0
//#define SCAN_SETTLE
#define LOG_MORE 0

static struct mtk_cam_seninf_ops *_seninf_ops = &mtk_csi_phy_3_0;





#define SET_DI_CTRL(ptr, s, vc) do { \
	SENINF_BITS(ptr, SENINF_CSI2_S##s##_DI_CTRL, \
			RG_CSI2_S##s##_DT_SEL, vc->dt); \
	SENINF_BITS(ptr, SENINF_CSI2_S##s##_DI_CTRL, \
			RG_CSI2_S##s##_VC_SEL, vc->vc); \
	SENINF_BITS(ptr, SENINF_CSI2_S##s##_DI_CTRL, \
			RG_CSI2_S##s##_DT_INTERLEAVE_MODE, 1); \
	SENINF_BITS(ptr, SENINF_CSI2_S##s##_DI_CTRL, \
			RG_CSI2_S##s##_VC_INTERLEAVE_EN, 1); \
} while (0)

#define SET_CH_CTRL(ptr, ch, s) \
	SENINF_BITS(ptr, SENINF_CSI2_CH##ch##_CTRL, \
		RG_CSI2_CH##ch##_S##s##_GRP_EN, 1)

#define SET_DI_CH_CTRL(ptr, s, vc) do { \
	SET_DI_CTRL(ptr, s, vc); \
	if (vc->group == 0) \
		SET_CH_CTRL(ptr, 0, s); \
	else if (vc->group == 1) \
		SET_CH_CTRL(ptr, 1, s); \
	else if (vc->group == 2) \
		SET_CH_CTRL(ptr, 2, s); \
	else if (vc->group == 3) \
		SET_CH_CTRL(ptr, 3, s); \
} while (0)

#define SHOW(buf, len, fmt, ...) { \
	len += snprintf(buf + len, PAGE_SIZE - len, fmt, ##__VA_ARGS__); \
}

static int mtk_cam_seninf_init_iomem(struct seninf_ctx *ctx,
			      void __iomem *if_base, void __iomem *ana_base)
{
	int i;

	ctx->reg_ana_csi_rx[CSI_PORT_0] =
	ctx->reg_ana_csi_rx[CSI_PORT_0A] = ana_base + 0;
	ctx->reg_ana_csi_rx[CSI_PORT_0B] = ana_base + 0x1000;

	ctx->reg_ana_csi_rx[CSI_PORT_1] =
	ctx->reg_ana_csi_rx[CSI_PORT_1A] = ana_base + 0x10000;
	ctx->reg_ana_csi_rx[CSI_PORT_1B] = ana_base + 0x11000;

	ctx->reg_ana_csi_rx[CSI_PORT_3A] =
	ctx->reg_ana_csi_rx[CSI_PORT_3] = ana_base + 0x14000;
	ctx->reg_ana_csi_rx[CSI_PORT_3B] = ana_base + 0x15000;

	ctx->reg_ana_csi_rx[CSI_PORT_2A] =
	ctx->reg_ana_csi_rx[CSI_PORT_2] = ana_base + 0x4000;
	ctx->reg_ana_csi_rx[CSI_PORT_2B] = ana_base + 0x5000;

	ctx->reg_ana_csi_rx[CSI_PORT_4A] =
	ctx->reg_ana_csi_rx[CSI_PORT_4] = ana_base + 0x8000;
	ctx->reg_ana_csi_rx[CSI_PORT_4B] = ana_base + 0x9000;

	ctx->reg_ana_csi_rx[CSI_PORT_5A] =
	ctx->reg_ana_csi_rx[CSI_PORT_5] = ana_base + 0xc000;
	ctx->reg_ana_csi_rx[CSI_PORT_5B] = ana_base + 0xd000;


	ctx->reg_ana_dphy_top[CSI_PORT_0A] =
	ctx->reg_ana_dphy_top[CSI_PORT_0B] =
	ctx->reg_ana_dphy_top[CSI_PORT_0] = ana_base + 0x2000;

	ctx->reg_ana_dphy_top[CSI_PORT_1A] =
	ctx->reg_ana_dphy_top[CSI_PORT_1B] =
	ctx->reg_ana_dphy_top[CSI_PORT_1] = ana_base + 0x12000;

	ctx->reg_ana_dphy_top[CSI_PORT_2A] =
	ctx->reg_ana_dphy_top[CSI_PORT_2B] =
	ctx->reg_ana_dphy_top[CSI_PORT_2] = ana_base + 0x6000;

	ctx->reg_ana_dphy_top[CSI_PORT_3A] =
	ctx->reg_ana_dphy_top[CSI_PORT_3B] =
	ctx->reg_ana_dphy_top[CSI_PORT_3] = ana_base + 0x16000;

	ctx->reg_ana_dphy_top[CSI_PORT_4A] =
	ctx->reg_ana_dphy_top[CSI_PORT_4B] =
	ctx->reg_ana_dphy_top[CSI_PORT_4] = ana_base + 0xa000;

	ctx->reg_ana_dphy_top[CSI_PORT_5A] =
	ctx->reg_ana_dphy_top[CSI_PORT_5B] =
	ctx->reg_ana_dphy_top[CSI_PORT_5] = ana_base + 0xe000;

	ctx->reg_ana_cphy_top[CSI_PORT_0A] =
	ctx->reg_ana_cphy_top[CSI_PORT_0B] =
	ctx->reg_ana_cphy_top[CSI_PORT_0] = ana_base + 0x3000;

	ctx->reg_ana_cphy_top[CSI_PORT_1A] =
	ctx->reg_ana_cphy_top[CSI_PORT_1B] =
	ctx->reg_ana_cphy_top[CSI_PORT_1] = ana_base + 0x13000;


	ctx->reg_ana_cphy_top[CSI_PORT_2A] =
	ctx->reg_ana_cphy_top[CSI_PORT_2B] =
	ctx->reg_ana_cphy_top[CSI_PORT_2] = ana_base + 0x7000;

	ctx->reg_ana_cphy_top[CSI_PORT_3A] =
	ctx->reg_ana_cphy_top[CSI_PORT_3B] =
	ctx->reg_ana_cphy_top[CSI_PORT_3] = ana_base + 0x17000;

	ctx->reg_ana_cphy_top[CSI_PORT_4A] =
	ctx->reg_ana_cphy_top[CSI_PORT_4B] =
	ctx->reg_ana_cphy_top[CSI_PORT_4] = ana_base + 0xb000;

	ctx->reg_ana_cphy_top[CSI_PORT_5A] =
	ctx->reg_ana_cphy_top[CSI_PORT_5B] =
	ctx->reg_ana_cphy_top[CSI_PORT_5] = ana_base + 0xf000;

	ctx->reg_if_top = if_base;

	for (i = SENINF_1; i < _seninf_ops->seninf_num; i++) {
		ctx->reg_if_ctrl[i] = if_base + 0x0200 + (0x1000 * i);
		ctx->reg_if_tg[i] = if_base + 0x0f00 + (0x1000 * i);
		ctx->reg_if_csi2[i] = if_base + 0x0a00 + (0x1000 * i);
	}


	for (i = SENINF_MUX1; i < _seninf_ops->mux_num; i++)
		ctx->reg_if_mux[i] = if_base + 0x0d00 + (0x1000 * i);

	for (i = SENINF_CAM_MUX0; i < _seninf_ops->cam_mux_num; i++)
		ctx->reg_if_cam_mux_pcsr[i] = if_base + 0x0400 + (0x0020 * i);

	ctx->reg_if_cam_mux_gcsr = if_base + 0x0300;


	return 0;
}

static int mtk_cam_seninf_init_port(struct seninf_ctx *ctx, int port)
{
	int portNum;

	if (port >= CSI_PORT_0A)
		portNum = (port - CSI_PORT_0) >> 1;
	else
		portNum = port;

	ctx->port = port;
	ctx->portNum = portNum;
	ctx->portA = CSI_PORT_0A + (portNum << 1);
	ctx->portB = ctx->portA + 1;
	ctx->is_4d1c = (port == portNum);

	switch (port) {
	case CSI_PORT_0:
	case CSI_PORT_0A:
		ctx->seninfIdx = SENINF_1;
		break;
	case CSI_PORT_0B:
		ctx->seninfIdx = SENINF_2;
		break;
	case CSI_PORT_1:
	case CSI_PORT_1A:
		ctx->seninfIdx = SENINF_3;
		break;
	case CSI_PORT_1B:
		ctx->seninfIdx = SENINF_4;
		break;
	case CSI_PORT_2:
	case CSI_PORT_2A:
		ctx->seninfIdx = SENINF_5;
		break;
	case CSI_PORT_2B:
		ctx->seninfIdx = SENINF_6;
		break;
	case CSI_PORT_3:
	case CSI_PORT_3A:
		ctx->seninfIdx = SENINF_7;
		break;
	case CSI_PORT_3B:
		ctx->seninfIdx = SENINF_8;
		break;
	case CSI_PORT_4:
	case CSI_PORT_4A:
		ctx->seninfIdx = SENINF_9;
		break;
	case CSI_PORT_4B:
		ctx->seninfIdx = SENINF_10;
		break;
	case CSI_PORT_5:
	case CSI_PORT_5A:
		ctx->seninfIdx = SENINF_11;
		break;
	case CSI_PORT_5B:
		ctx->seninfIdx = SENINF_12;
		break;

	default:
		dev_info(ctx->dev, "invalid port %d\n", port);
		return -EINVAL;
	}

	return 0;
}

static int mtk_cam_seninf_is_cammux_used(struct seninf_ctx *ctx, int cam_mux)
{
	void *pSeninf_cam_mux_pcsr = NULL;

	if (cam_mux >= _seninf_ops->cam_mux_num) {
		dev_info(ctx->dev, "%s err cam_mux %d >= SENINF_CAM_MUX_NUM %d\n",
		__func__,
		cam_mux,
		_seninf_ops->cam_mux_num);
		return 0;
	}
	pSeninf_cam_mux_pcsr = ctx->reg_if_cam_mux_pcsr[cam_mux];

	return SENINF_READ_BITS(pSeninf_cam_mux_pcsr,
		SENINF_CAM_MUX_PCSR_CTRL, RG_SENINF_CAM_MUX_PCSR_EN);
}

static int mtk_cam_seninf_cammux(struct seninf_ctx *ctx, int cam_mux)
{
	void *pSeninf_cam_mux_pcsr = NULL;

	if (cam_mux >= _seninf_ops->cam_mux_num) {
		dev_info(ctx->dev, "%s err cam_mux %d >= SENINF_CAM_MUX_NUM %d\n",
		__func__,
		cam_mux,
		_seninf_ops->cam_mux_num);
		return 0;
	}
	pSeninf_cam_mux_pcsr = ctx->reg_if_cam_mux_pcsr[cam_mux];

	SENINF_BITS(pSeninf_cam_mux_pcsr,
			SENINF_CAM_MUX_PCSR_CTRL, RG_SENINF_CAM_MUX_PCSR_EN, 1);


	SENINF_WRITE_REG(pSeninf_cam_mux_pcsr, SENINF_CAM_MUX_PCSR_IRQ_STATUS,
			 (1 << RO_SENINF_CAM_MUX_PCSR_HSIZE_ERR_IRQ_SHIFT) |
			 (1 << RO_SENINF_CAM_MUX_PCSR_VSIZE_ERR_IRQ_SHIFT) |
			 (1 << RO_SENINF_CAM_MUX_PCSR_VSYNC_IRQ_SHIFT));//clr irq
#if LOG_MORE
	dev_info(ctx->dev, " %s cam_mux %d EN 0x%x IRQ_EN 0x%x IRQ_STATUS 0x%x\n",
		__func__,
		cam_mux,
		SENINF_READ_REG(pSeninf_cam_mux_pcsr, SENINF_CAM_MUX_PCSR_CTRL),
		SENINF_READ_REG(pSeninf_cam_mux_pcsr, SENINF_CAM_MUX_PCSR_IRQ_EN),
		SENINF_READ_REG(pSeninf_cam_mux_pcsr, SENINF_CAM_MUX_PCSR_IRQ_STATUS));
#endif
	return 0;
}

static int mtk_cam_seninf_disable_cammux(struct seninf_ctx *ctx, int cam_mux)
{
	void *pSeninf_cam_mux_pcsr = NULL;

	if (cam_mux >= _seninf_ops->cam_mux_num) {
		dev_info(ctx->dev, "%s err cam_mux %d >= SENINF_CAM_MUX_NUM %d\n",
		__func__,
		cam_mux,
		_seninf_ops->cam_mux_num);
		return 0;
	}
	pSeninf_cam_mux_pcsr = ctx->reg_if_cam_mux_pcsr[cam_mux];

	SENINF_BITS(pSeninf_cam_mux_pcsr,
			SENINF_CAM_MUX_PCSR_CTRL, CAM_MUX_PCSR_NEXT_SRC_SEL, 0x1f);

	SENINF_BITS(pSeninf_cam_mux_pcsr,
			SENINF_CAM_MUX_PCSR_CTRL, RG_SENINF_CAM_MUX_PCSR_SRC_SEL, 0x1f);

	SENINF_BITS(pSeninf_cam_mux_pcsr,
			SENINF_CAM_MUX_PCSR_CTRL, RG_SENINF_CAM_MUX_PCSR_EN, 0);

	SENINF_WRITE_REG(pSeninf_cam_mux_pcsr, SENINF_CAM_MUX_PCSR_IRQ_STATUS,
			 (1 << RO_SENINF_CAM_MUX_PCSR_HSIZE_ERR_IRQ_SHIFT) |
			 (1 << RO_SENINF_CAM_MUX_PCSR_VSIZE_ERR_IRQ_SHIFT) |
			 (1 << RO_SENINF_CAM_MUX_PCSR_VSYNC_IRQ_SHIFT));//clr irq

#if LOG_MORE
	dev_info(ctx->dev, "%s cam_mux %d EN 0x%x IRQ_EN 0x%x IRQ_STATUS 0x%x\n",
	__func__,
	cam_mux,
	SENINF_READ_REG(pSeninf_cam_mux_pcsr, SENINF_CAM_MUX_PCSR_CTRL),
	SENINF_READ_REG(pSeninf_cam_mux_pcsr, SENINF_CAM_MUX_PCSR_IRQ_EN),
	SENINF_READ_REG(pSeninf_cam_mux_pcsr, SENINF_CAM_MUX_PCSR_IRQ_STATUS));
#endif
	return 0;
}

static int mtk_cam_seninf_disable_all_cammux(struct seninf_ctx *ctx)
{
	int i = 0;
	void *pSeninf_cam_mux_gcsr = ctx->reg_if_cam_mux_gcsr;

	for (i = SENINF_CAM_MUX0; i < _seninf_ops->cam_mux_num; i++)
		mtk_cam_seninf_disable_cammux(ctx, i);

	dev_info(ctx->dev, "%s all SENINF_CAM_MUX_GCSR_MUX_EN 0x%x\n",
	__func__,
	SENINF_READ_REG(pSeninf_cam_mux_gcsr, SENINF_CAM_MUX_GCSR_MUX_EN));

	return 0;
}

static int mtk_cam_seninf_set_top_mux_ctrl(struct seninf_ctx *ctx,
				    int mux_idx, int seninf_src)
{
	void *pSeninf = ctx->reg_if_top;

	switch (mux_idx) {
	case SENINF_MUX1:
		SENINF_BITS(pSeninf, SENINF_TOP_MUX_CTRL_0,
			    RG_SENINF_MUX1_SRC_SEL, seninf_src);
		break;
	case SENINF_MUX2:
		SENINF_BITS(pSeninf, SENINF_TOP_MUX_CTRL_0,
			    RG_SENINF_MUX2_SRC_SEL, seninf_src);
		break;
	case SENINF_MUX3:
		SENINF_BITS(pSeninf, SENINF_TOP_MUX_CTRL_0,
			    RG_SENINF_MUX3_SRC_SEL, seninf_src);
		break;
	case SENINF_MUX4:
		SENINF_BITS(pSeninf, SENINF_TOP_MUX_CTRL_0,
			    RG_SENINF_MUX4_SRC_SEL, seninf_src);
		break;
	case SENINF_MUX5:
		SENINF_BITS(pSeninf, SENINF_TOP_MUX_CTRL_1,
			    RG_SENINF_MUX5_SRC_SEL, seninf_src);
		break;
	case SENINF_MUX6:
		SENINF_BITS(pSeninf, SENINF_TOP_MUX_CTRL_1,
			    RG_SENINF_MUX6_SRC_SEL, seninf_src);
		break;
	case SENINF_MUX7:
		SENINF_BITS(pSeninf, SENINF_TOP_MUX_CTRL_1,
			    RG_SENINF_MUX7_SRC_SEL, seninf_src);
		break;
	case SENINF_MUX8:
		SENINF_BITS(pSeninf, SENINF_TOP_MUX_CTRL_1,
			    RG_SENINF_MUX8_SRC_SEL, seninf_src);
		break;
	case SENINF_MUX9:
		SENINF_BITS(pSeninf, SENINF_TOP_MUX_CTRL_2,
			    RG_SENINF_MUX9_SRC_SEL, seninf_src);
		break;
	case SENINF_MUX10:
		SENINF_BITS(pSeninf, SENINF_TOP_MUX_CTRL_2,
			    RG_SENINF_MUX10_SRC_SEL, seninf_src);
		break;
	case SENINF_MUX11:
		SENINF_BITS(pSeninf, SENINF_TOP_MUX_CTRL_2,
			    RG_SENINF_MUX11_SRC_SEL, seninf_src);
		break;
	case SENINF_MUX12:
		SENINF_BITS(pSeninf, SENINF_TOP_MUX_CTRL_2,
			    RG_SENINF_MUX12_SRC_SEL, seninf_src);
		break;
	case SENINF_MUX13:
		SENINF_BITS(pSeninf, SENINF_TOP_MUX_CTRL_3,
				RG_SENINF_MUX13_SRC_SEL, seninf_src);
		break;
	case SENINF_MUX14:
		SENINF_BITS(pSeninf, SENINF_TOP_MUX_CTRL_3,
				RG_SENINF_MUX14_SRC_SEL, seninf_src);
		break;
	case SENINF_MUX15:
		SENINF_BITS(pSeninf, SENINF_TOP_MUX_CTRL_3,
				RG_SENINF_MUX15_SRC_SEL, seninf_src);
		break;
	case SENINF_MUX16:
		SENINF_BITS(pSeninf, SENINF_TOP_MUX_CTRL_3,
				RG_SENINF_MUX16_SRC_SEL, seninf_src);
		break;
	case SENINF_MUX17:
		SENINF_BITS(pSeninf, SENINF_TOP_MUX_CTRL_4,
				RG_SENINF_MUX17_SRC_SEL, seninf_src);
		break;
	case SENINF_MUX18:
		SENINF_BITS(pSeninf, SENINF_TOP_MUX_CTRL_4,
				RG_SENINF_MUX18_SRC_SEL, seninf_src);
		break;
	case SENINF_MUX19:
		SENINF_BITS(pSeninf, SENINF_TOP_MUX_CTRL_4,
				RG_SENINF_MUX19_SRC_SEL, seninf_src);
		break;
	case SENINF_MUX20:
		SENINF_BITS(pSeninf, SENINF_TOP_MUX_CTRL_4,
				RG_SENINF_MUX20_SRC_SEL, seninf_src);
		break;
	case SENINF_MUX21:
		SENINF_BITS(pSeninf, SENINF_TOP_MUX_CTRL_5,
				RG_SENINF_MUX21_SRC_SEL, seninf_src);
		break;
	case SENINF_MUX22:
		SENINF_BITS(pSeninf, SENINF_TOP_MUX_CTRL_5,
				RG_SENINF_MUX22_SRC_SEL, seninf_src);
			break;

	default:
		dev_info(ctx->dev, "invalid mux_idx %d\n", mux_idx);
		return -EINVAL;
	}
#if LOG_MORE
	dev_info(ctx->dev,
		"TOP_MUX_CTRL_0(0x%x) TOP_MUX_CTRL_1(0x%x) TOP_MUX_CTRL_2(0x%x) TOP_MUX_CTRL_3(0x%x) TOP_MUX_CTRL_4(0x%x) TOP_MUX_CTRL_5(0x%x)\n",
		SENINF_READ_REG(pSeninf, SENINF_TOP_MUX_CTRL_0),
		SENINF_READ_REG(pSeninf, SENINF_TOP_MUX_CTRL_1),
		SENINF_READ_REG(pSeninf, SENINF_TOP_MUX_CTRL_2),
		SENINF_READ_REG(pSeninf, SENINF_TOP_MUX_CTRL_3),
		SENINF_READ_REG(pSeninf, SENINF_TOP_MUX_CTRL_4),
		SENINF_READ_REG(pSeninf, SENINF_TOP_MUX_CTRL_5));
#endif
	return 0;
}

static int mtk_cam_seninf_get_top_mux_ctrl(struct seninf_ctx *ctx, int mux_idx)
{
	void *pSeninf = ctx->reg_if_top;
	unsigned int seninf_src = 0;


	switch (mux_idx) {
	case SENINF_MUX1:
		seninf_src = SENINF_READ_BITS(pSeninf, SENINF_TOP_MUX_CTRL_0,
				RG_SENINF_MUX1_SRC_SEL);
		break;
	case SENINF_MUX2:
		seninf_src = SENINF_READ_BITS(pSeninf, SENINF_TOP_MUX_CTRL_0,
				RG_SENINF_MUX2_SRC_SEL);
		break;
	case SENINF_MUX3:
		seninf_src = SENINF_READ_BITS(pSeninf, SENINF_TOP_MUX_CTRL_0,
			RG_SENINF_MUX3_SRC_SEL);
			break;
	case SENINF_MUX4:
		seninf_src = SENINF_READ_BITS(pSeninf, SENINF_TOP_MUX_CTRL_0,
			RG_SENINF_MUX4_SRC_SEL);
		break;
	case SENINF_MUX5:
		seninf_src = SENINF_READ_BITS(pSeninf, SENINF_TOP_MUX_CTRL_1,
			RG_SENINF_MUX5_SRC_SEL);
		break;
	case SENINF_MUX6:
		seninf_src = SENINF_READ_BITS(pSeninf, SENINF_TOP_MUX_CTRL_1,
			RG_SENINF_MUX6_SRC_SEL);
		break;
	case SENINF_MUX7:
		seninf_src = SENINF_READ_BITS(pSeninf, SENINF_TOP_MUX_CTRL_1,
			RG_SENINF_MUX7_SRC_SEL);
		break;
	case SENINF_MUX8:
		seninf_src = SENINF_READ_BITS(pSeninf, SENINF_TOP_MUX_CTRL_1,
			RG_SENINF_MUX8_SRC_SEL);
		break;
	case SENINF_MUX9:
		seninf_src = SENINF_READ_BITS(pSeninf, SENINF_TOP_MUX_CTRL_2,
			RG_SENINF_MUX9_SRC_SEL);
		break;
	case SENINF_MUX10:
		seninf_src = SENINF_READ_BITS(pSeninf, SENINF_TOP_MUX_CTRL_2,
			RG_SENINF_MUX10_SRC_SEL);
		break;
	case SENINF_MUX11:
		seninf_src = SENINF_READ_BITS(pSeninf, SENINF_TOP_MUX_CTRL_2,
			RG_SENINF_MUX11_SRC_SEL);
		break;
	case SENINF_MUX12:
		seninf_src = SENINF_READ_BITS(pSeninf, SENINF_TOP_MUX_CTRL_2,
			RG_SENINF_MUX12_SRC_SEL);
		break;
	case SENINF_MUX13:
		seninf_src = SENINF_READ_BITS(pSeninf, SENINF_TOP_MUX_CTRL_3,
			RG_SENINF_MUX13_SRC_SEL);
		break;
	case SENINF_MUX14:
		seninf_src = SENINF_READ_BITS(pSeninf, SENINF_TOP_MUX_CTRL_3,
			RG_SENINF_MUX14_SRC_SEL);
		break;
	case SENINF_MUX15:
		seninf_src = SENINF_READ_BITS(pSeninf, SENINF_TOP_MUX_CTRL_3,
			RG_SENINF_MUX15_SRC_SEL);
		break;
	case SENINF_MUX16:
		seninf_src = SENINF_READ_BITS(pSeninf, SENINF_TOP_MUX_CTRL_3,
			RG_SENINF_MUX16_SRC_SEL);
		break;
	case SENINF_MUX17:
		seninf_src = SENINF_READ_BITS(pSeninf, SENINF_TOP_MUX_CTRL_4,
			RG_SENINF_MUX17_SRC_SEL);
		break;
	case SENINF_MUX18:
		seninf_src = SENINF_READ_BITS(pSeninf, SENINF_TOP_MUX_CTRL_4,
			RG_SENINF_MUX18_SRC_SEL);
		break;
	case SENINF_MUX19:
		seninf_src = SENINF_READ_BITS(pSeninf, SENINF_TOP_MUX_CTRL_4,
			RG_SENINF_MUX19_SRC_SEL);
		break;
	case SENINF_MUX20:
		seninf_src = SENINF_READ_BITS(pSeninf, SENINF_TOP_MUX_CTRL_4,
			RG_SENINF_MUX20_SRC_SEL);
		break;
	case SENINF_MUX21:
		seninf_src = SENINF_READ_BITS(pSeninf, SENINF_TOP_MUX_CTRL_5,
			RG_SENINF_MUX21_SRC_SEL);
		break;
	case SENINF_MUX22:
		seninf_src = SENINF_READ_BITS(pSeninf, SENINF_TOP_MUX_CTRL_5,
			RG_SENINF_MUX22_SRC_SEL);
		break;

	default:
		dev_info(ctx->dev, "invalid mux_idx %d", mux_idx);
		return -EINVAL;
	}

	return seninf_src;
}

static int mtk_cam_seninf_get_cammux_ctrl(struct seninf_ctx *ctx, int cam_mux)
{
	int ret = 0;
	void *pSeninf_cam_mux_pcsr = NULL;

	if (cam_mux >= _seninf_ops->cam_mux_num) {
		dev_info(ctx->dev, "%s err cam_mux %d >= SENINF_CAM_MUX_NUM %d\n",
		__func__,
		cam_mux,
		_seninf_ops->cam_mux_num);
		return 0;
	}
	pSeninf_cam_mux_pcsr = ctx->reg_if_cam_mux_pcsr[cam_mux];

	ret = SENINF_READ_BITS(pSeninf_cam_mux_pcsr,
		SENINF_CAM_MUX_PCSR_CTRL, RG_SENINF_CAM_MUX_PCSR_SRC_SEL);

	return ret;
}

static u32 mtk_cam_seninf_get_cammux_res(struct seninf_ctx *ctx, int cam_mux)
{
	u32 ret = 0;
	void *pSeninf_cam_mux_pcsr = NULL;

	if (cam_mux >= _seninf_ops->cam_mux_num) {
		dev_info(ctx->dev, "%s err cam_mux %d >= SENINF_CAM_MUX_NUM %d\n",
		__func__,
		cam_mux,
		_seninf_ops->cam_mux_num);
		return 0;
	}
	pSeninf_cam_mux_pcsr = ctx->reg_if_cam_mux_pcsr[cam_mux];

	ret = SENINF_READ_REG(pSeninf_cam_mux_pcsr, SENINF_CAM_MUX_PCSR_CHK_RES);
	return ret;
}

static u32 mtk_cam_seninf_get_cammux_exp(struct seninf_ctx *ctx, int cam_mux)
{
	void *pSeninf_cam_mux_pcsr = NULL;
	u32 ret = 0;

	if (cam_mux >= _seninf_ops->cam_mux_num) {
		dev_info(ctx->dev, "%s err cam_mux %d >= SENINF_CAM_MUX_NUM %d\n",
		__func__,
		cam_mux,
		_seninf_ops->cam_mux_num);
		return 0;
	}
	pSeninf_cam_mux_pcsr = ctx->reg_if_cam_mux_pcsr[cam_mux];


	ret = SENINF_READ_REG(pSeninf_cam_mux_pcsr, SENINF_CAM_MUX_PCSR_CHK_CTL);
	return ret;
}

static u32 mtk_cam_seninf_get_cammux_err(struct seninf_ctx *ctx, int cam_mux)
{
	void *pSeninf_cam_mux_pcsr = NULL;
	u32 ret = 0;

	if (cam_mux >= _seninf_ops->cam_mux_num) {
		dev_info(ctx->dev, "%s err cam_mux %d >= SENINF_CAM_MUX_NUM %d\n",
		__func__,
		cam_mux,
		_seninf_ops->cam_mux_num);
		return 0;
	}
	pSeninf_cam_mux_pcsr = ctx->reg_if_cam_mux_pcsr[cam_mux];


	ret = SENINF_READ_REG(pSeninf_cam_mux_pcsr, SENINF_CAM_MUX_PCSR_CHK_ERR_RES);
	return ret;
}


static int mtk_cam_seninf_set_cammux_vc(struct seninf_ctx *ctx, int cam_mux,
				 int vc_sel, int dt_sel, int vc_en, int dt_en)
{
	void *pSeninf_cam_mux_pcsr = NULL;

	if (cam_mux >= _seninf_ops->cam_mux_num) {
		dev_info(ctx->dev, "%s err cam_mux %d >= SENINF_CAM_MUX_NUM %d\n",
		__func__,
		cam_mux,
		_seninf_ops->cam_mux_num);
		return 0;
	}
	pSeninf_cam_mux_pcsr = ctx->reg_if_cam_mux_pcsr[cam_mux];

#if	DEBUG_CAM_MUX_SWITCH
	dev_info(ctx->dev, "%s cam_mux %d vc 0x%x dt 0x%x, vc_en %d dt_en %d\n",
		__func__,
		cam_mux, vc_sel, dt_sel, vc_en, dt_en);
#endif

	SENINF_BITS(pSeninf_cam_mux_pcsr, SENINF_CAM_MUX_PCSR_OPT,
			RG_SENINF_CAM_MUX_PCSR_VC_SEL, vc_sel);
	SENINF_BITS(pSeninf_cam_mux_pcsr, SENINF_CAM_MUX_PCSR_OPT,
			RG_SENINF_CAM_MUX_PCSR_DT_SEL, dt_sel);
	SENINF_BITS(pSeninf_cam_mux_pcsr, SENINF_CAM_MUX_PCSR_OPT,
			RG_SENINF_CAM_MUX_PCSR_VC_SEL_EN, vc_en);
	SENINF_BITS(pSeninf_cam_mux_pcsr, SENINF_CAM_MUX_PCSR_OPT,
			RG_SENINF_CAM_MUX_PCSR_DT_SEL_EN, dt_en);
	return 0;
}
static int mtk_cam_seninf_switch_to_cammux_inner_page(struct seninf_ctx *ctx, bool inner)
{
	void *pSeninf_cam_mux_gcsr = ctx->reg_if_cam_mux_gcsr;

#if	DEBUG_CAM_MUX_SWITCH
	dev_info(ctx->dev, "%s inner %d\n", __func__, inner);
#endif
	SENINF_BITS(pSeninf_cam_mux_gcsr,
		SENINF_CAM_MUX_GCSR_DYN_CTRL, RG_SENINF_CAM_MUX_GCSR_DYN_PAGE_SEL, inner ? 0 : 1);

	return 0;
}

static int mtk_cam_seninf_set_cammux_next_ctrl(struct seninf_ctx *ctx, int src, int target)
{
	void *pSeninf_cam_mux_pcsr = NULL;

	if (target >= _seninf_ops->cam_mux_num) {
		dev_info(ctx->dev, "%s err cam_mux %d >= SENINF_CAM_MUX_NUM %d\n",
		__func__,
		target,
		_seninf_ops->cam_mux_num);
		return 0;
	}
	pSeninf_cam_mux_pcsr = ctx->reg_if_cam_mux_pcsr[target];
#if	DEBUG_CAM_MUX_SWITCH
	dev_info(ctx->dev, "%s cam_mux %d src %d\n", __func__, target, src);
#endif

	SENINF_BITS(pSeninf_cam_mux_pcsr, SENINF_CAM_MUX_PCSR_CTRL,
					CAM_MUX_PCSR_NEXT_SRC_SEL, src);

	if (src != 0x1f) {
		u32 in_ctrl, in_opt, out_ctrl, out_opt;

		mtk_cam_seninf_switch_to_cammux_inner_page(ctx, true);
		in_ctrl = SENINF_READ_REG(pSeninf_cam_mux_pcsr, SENINF_CAM_MUX_PCSR_CTRL);
		in_opt = SENINF_READ_REG(pSeninf_cam_mux_pcsr, SENINF_CAM_MUX_PCSR_OPT);
		mtk_cam_seninf_switch_to_cammux_inner_page(ctx, false);
		out_ctrl = SENINF_READ_REG(pSeninf_cam_mux_pcsr, SENINF_CAM_MUX_PCSR_CTRL);
		out_opt = SENINF_READ_REG(pSeninf_cam_mux_pcsr, SENINF_CAM_MUX_PCSR_OPT);
		mtk_cam_seninf_switch_to_cammux_inner_page(ctx, true);
		dev_info(ctx->dev,
			" %s cam_mux %d in|out SENINF_CAM_MUX_PCSR_CTRL 0x%x|0x%x SENINF_CAM_MUX_PCSR_OPT 0x%x|0x%x\n",
			__func__,
			target,
			in_ctrl,
			out_ctrl,
			in_opt,
			out_opt);
	}

	return 0;
}


static int mtk_cam_seninf_set_cammux_src(struct seninf_ctx *ctx, int src, int target,
				  int exp_hsize, int exp_vsize)
{

	void *pSeninf_cam_mux_pcsr = NULL;

	if (target >= _seninf_ops->cam_mux_num) {
		dev_info(ctx->dev, "%s err cam_mux %d >= SENINF_CAM_MUX_NUM %d\n",
		__func__,
		target,
		_seninf_ops->cam_mux_num);
		return 0;
	}
	pSeninf_cam_mux_pcsr = ctx->reg_if_cam_mux_pcsr[target];

#if	DEBUG_CAM_MUX_SWITCH

	dev_info(ctx->dev, "%s cam_mux %d src %d exp_hsize %d, exp_hsize %d\n",
		__func__, target, src, exp_hsize, exp_vsize);
#endif
	SENINF_BITS(pSeninf_cam_mux_pcsr, SENINF_CAM_MUX_PCSR_CTRL,
					RG_SENINF_CAM_MUX_PCSR_SRC_SEL, src);
	SENINF_BITS(pSeninf_cam_mux_pcsr, SENINF_CAM_MUX_PCSR_CHK_CTL,
					RG_SENINF_CAM_MUX_PCSR_EXP_HSIZE, exp_hsize);
	SENINF_BITS(pSeninf_cam_mux_pcsr, SENINF_CAM_MUX_PCSR_CHK_CTL,
					RG_SENINF_CAM_MUX_PCSR_EXP_VSIZE, exp_vsize);
	return 0;
}

static int mtk_cam_seninf_set_vc(struct seninf_ctx *ctx, int intf,
			  struct seninf_vcinfo *vcinfo)
{
	void *pSeninf_csi2 = ctx->reg_if_csi2[intf];
	int i;
	struct seninf_vc *vc;

	if (!vcinfo || !vcinfo->cnt)
		return 0;

	SENINF_WRITE_REG(pSeninf_csi2, SENINF_CSI2_S0_DI_CTRL, 0);
	SENINF_WRITE_REG(pSeninf_csi2, SENINF_CSI2_S1_DI_CTRL, 0);
	SENINF_WRITE_REG(pSeninf_csi2, SENINF_CSI2_S2_DI_CTRL, 0);
	SENINF_WRITE_REG(pSeninf_csi2, SENINF_CSI2_S3_DI_CTRL, 0);
	SENINF_WRITE_REG(pSeninf_csi2, SENINF_CSI2_S4_DI_CTRL, 0);
	SENINF_WRITE_REG(pSeninf_csi2, SENINF_CSI2_S5_DI_CTRL, 0);
	SENINF_WRITE_REG(pSeninf_csi2, SENINF_CSI2_S6_DI_CTRL, 0);
	SENINF_WRITE_REG(pSeninf_csi2, SENINF_CSI2_S7_DI_CTRL, 0);

	SENINF_WRITE_REG(pSeninf_csi2, SENINF_CSI2_CH0_CTRL, 0);
	SENINF_WRITE_REG(pSeninf_csi2, SENINF_CSI2_CH1_CTRL, 0);
	SENINF_WRITE_REG(pSeninf_csi2, SENINF_CSI2_CH2_CTRL, 0);
	SENINF_WRITE_REG(pSeninf_csi2, SENINF_CSI2_CH3_CTRL, 0);

	for (i = 0; i < vcinfo->cnt; i++) {
		vc = &vcinfo->vc[i];

		/* General Long Packet Data Types: 0x10-0x17 */
		if (vc->dt >= 0x10 && vc->dt <= 0x17) {
			SENINF_BITS(pSeninf_csi2, SENINF_CSI2_OPT,
				    RG_CSI2_GENERIC_LONG_PACKET_EN, 1);
		}

		switch (i) {
		case 0:
			SET_DI_CH_CTRL(pSeninf_csi2, 0, vc);
			break;
		case 1:
			SET_DI_CH_CTRL(pSeninf_csi2, 1, vc);
			break;
		case 2:
			SET_DI_CH_CTRL(pSeninf_csi2, 2, vc);
			break;
		case 3:
			SET_DI_CH_CTRL(pSeninf_csi2, 3, vc);
			break;
		case 4:
			SET_DI_CH_CTRL(pSeninf_csi2, 4, vc);
			break;
		case 5:
			SET_DI_CH_CTRL(pSeninf_csi2, 5, vc);
			break;
		case 6:
			SET_DI_CH_CTRL(pSeninf_csi2, 6, vc);
			break;
		case 7:
			SET_DI_CH_CTRL(pSeninf_csi2, 7, vc);
			break;
		}
	}
#if LOG_MORE
	dev_info(ctx->dev, "DI_CTRL 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
		 SENINF_READ_REG(pSeninf_csi2, SENINF_CSI2_S0_DI_CTRL),
		SENINF_READ_REG(pSeninf_csi2, SENINF_CSI2_S1_DI_CTRL),
		SENINF_READ_REG(pSeninf_csi2, SENINF_CSI2_S2_DI_CTRL),
		SENINF_READ_REG(pSeninf_csi2, SENINF_CSI2_S3_DI_CTRL),
		SENINF_READ_REG(pSeninf_csi2, SENINF_CSI2_S4_DI_CTRL),
		SENINF_READ_REG(pSeninf_csi2, SENINF_CSI2_S5_DI_CTRL),
		SENINF_READ_REG(pSeninf_csi2, SENINF_CSI2_S6_DI_CTRL),
		SENINF_READ_REG(pSeninf_csi2, SENINF_CSI2_S7_DI_CTRL));

	dev_info(ctx->dev, "CH_CTRL 0x%x 0x%x 0x%x 0x%x\n",
		 SENINF_READ_REG(pSeninf_csi2, SENINF_CSI2_CH0_CTRL),
		SENINF_READ_REG(pSeninf_csi2, SENINF_CSI2_CH1_CTRL),
		SENINF_READ_REG(pSeninf_csi2, SENINF_CSI2_CH2_CTRL),
		SENINF_READ_REG(pSeninf_csi2, SENINF_CSI2_CH3_CTRL));
#endif
	return 0;
}

static int mtk_cam_seninf_set_mux_ctrl(struct seninf_ctx *ctx, int mux,
				int hsPol, int vsPol, int src_sel,
				int pixel_mode)
{
	unsigned int temp = 0;
	void *pSeninf_mux;

	pSeninf_mux = ctx->reg_if_mux[mux];

	//1A00 4D04[3:0] select source group
	SENINF_BITS(pSeninf_mux, SENINF_MUX_CTRL_1,
		    RG_SENINF_MUX_SRC_SEL,
			src_sel);

	SENINF_BITS(pSeninf_mux, SENINF_MUX_CTRL_1,
		    RG_SENINF_MUX_PIX_MODE_SEL,
			pixel_mode);

//	set_camux_checker_pixel_mode(0, pixel_mode);

	SENINF_BITS(pSeninf_mux, SENINF_MUX_OPT,
		    RG_SENINF_MUX_HSYNC_POL, hsPol);
	SENINF_BITS(pSeninf_mux, SENINF_MUX_OPT,
		    RG_SENINF_MUX_VSYNC_POL, vsPol);

	temp = SENINF_READ_REG(pSeninf_mux, SENINF_MUX_CTRL_0);
	SENINF_WRITE_REG(pSeninf_mux, SENINF_MUX_CTRL_0, temp | 0x6);//reset
	SENINF_WRITE_REG(pSeninf_mux, SENINF_MUX_CTRL_0, temp & 0xFFFFFFF9);
#if LOG_MORE
	dev_info(ctx->dev, "SENINF_MUX_CTRL_1(0x%x), SENINF_MUX_OPT(0x%x)",
		 SENINF_READ_REG(pSeninf_mux, SENINF_MUX_CTRL_1),
		SENINF_READ_REG(pSeninf_mux, SENINF_MUX_OPT));
#endif
	return 0;
}

static int mtk_cam_seninf_update_mux_pixel_mode(struct seninf_ctx *ctx, int mux,
				int pixel_mode)
{
	unsigned int temp = 0;
	void *pSeninf_mux;

	pSeninf_mux = ctx->reg_if_mux[mux];


	SENINF_BITS(pSeninf_mux, SENINF_MUX_CTRL_1,
		    RG_SENINF_MUX_PIX_MODE_SEL,
			pixel_mode);

	temp = SENINF_READ_REG(pSeninf_mux, SENINF_MUX_CTRL_0);
	SENINF_WRITE_REG(pSeninf_mux, SENINF_MUX_CTRL_0, temp | 0x6);//reset
	SENINF_WRITE_REG(pSeninf_mux, SENINF_MUX_CTRL_0, temp & 0xFFFFFFF9);

	dev_info(ctx->dev, "%s mux %d SENINF_MUX_CTRL_1(0x%x), SENINF_MUX_OPT(0x%x)",
		__func__,
		mux,
		SENINF_READ_REG(pSeninf_mux, SENINF_MUX_CTRL_1),
		SENINF_READ_REG(pSeninf_mux, SENINF_MUX_OPT));

	return 0;
}


static int mtk_cam_seninf_set_mux_crop(struct seninf_ctx *ctx, int mux,
				int start_x, int end_x, int enable)
{
	void *pSeninf_mux = ctx->reg_if_mux[mux];

	SENINF_BITS(pSeninf_mux, SENINF_MUX_CROP_PIX_CTRL,
		    RG_SENINF_MUX_CROP_START_NPIX_CNT, start_x / 8);
	SENINF_BITS(pSeninf_mux, SENINF_MUX_CROP_PIX_CTRL,
		    RG_SENINF_MUX_CROP_END_NPIX_CNT,
		start_x / 8 + (end_x - start_x + 1) / 8 - 1 + (((end_x -
		start_x + 1) % 8) > 0 ? 1 : 0));
	SENINF_BITS(pSeninf_mux, SENINF_MUX_CTRL_1,
		    RG_SENINF_MUX_CROP_EN, enable);

	dev_info(ctx->dev, "MUX_CROP_PIX_CTRL 0x%x MUX_CTRL_1 0x%x\n",
		 SENINF_READ_REG(pSeninf_mux, SENINF_MUX_CROP_PIX_CTRL),
		SENINF_READ_REG(pSeninf_mux, SENINF_MUX_CTRL_1));

	dev_info(ctx->dev, "mux %d, start %d, end %d, enable %d\n",
		 mux, start_x, end_x, enable);

	return 0;
}

static int mtk_cam_seninf_is_mux_used(struct seninf_ctx *ctx, int mux)
{
	void *pSeninf_mux = ctx->reg_if_mux[mux];

	return SENINF_READ_BITS(pSeninf_mux, SENINF_MUX_CTRL_0, SENINF_MUX_EN);
}

static int mtk_cam_seninf_mux(struct seninf_ctx *ctx, int mux)
{
	void *pSeninf_mux = ctx->reg_if_mux[mux];

	SENINF_BITS(pSeninf_mux, SENINF_MUX_CTRL_0, SENINF_MUX_EN, 1);
	return 0;
}

static int mtk_cam_seninf_disable_mux(struct seninf_ctx *ctx, int mux)
{
	int i;
	void *pSeninf_mux = ctx->reg_if_mux[mux];

	SENINF_BITS(pSeninf_mux, SENINF_MUX_CTRL_0, SENINF_MUX_EN, 0);

	//also disable CAM_MUX with input from mux
	for (i = SENINF_CAM_MUX0; i < _seninf_ops->cam_mux_num; i++) {
		if (mux == mtk_cam_seninf_get_cammux_ctrl(ctx, i))
			mtk_cam_seninf_disable_cammux(ctx, i);
	}

	return 0;
}

static int mtk_cam_seninf_disable_all_mux(struct seninf_ctx *ctx)
{
	int i;
	void *pSeninf_mux;

	for (i = 0; i < _seninf_ops->mux_num; i++) {
		pSeninf_mux = ctx->reg_if_mux[i];
		SENINF_BITS(pSeninf_mux, SENINF_MUX_CTRL_0, SENINF_MUX_EN, 0);
	}

	return 0;
}

static int mtk_cam_seninf_set_cammux_chk_pixel_mode(struct seninf_ctx *ctx,
					     int cam_mux, int pixel_mode)
{
	void *pSeninf_cam_mux_pcsr = NULL;

	if (cam_mux >= _seninf_ops->cam_mux_num) {
		dev_info(ctx->dev, "%s err cam_mux %d >= SENINF_CAM_MUX_NUM %d\n",
		__func__,
		cam_mux,
		_seninf_ops->cam_mux_num);
		return 0;
	}
	pSeninf_cam_mux_pcsr = ctx->reg_if_cam_mux_pcsr[cam_mux];

	SENINF_BITS(pSeninf_cam_mux_pcsr, SENINF_CAM_MUX_PCSR_CTRL,
					RG_SENINF_CAM_MUX_PCSR_CHK_PIX_MODE, pixel_mode);
	return 0;
}

static int mtk_cam_seninf_set_test_model(struct seninf_ctx *ctx,
				  int mux, int cam_mux, int pixel_mode)
{
	int intf;
	void *pSeninf;
	void *pSeninf_tg;
	void *pSeninf_mux;

	intf = mux % 5; /* XXX: only a subset of seninf has a testmdl, by platform */

	pSeninf = ctx->reg_if_ctrl[intf];
	pSeninf_tg = ctx->reg_if_tg[intf];
	pSeninf_mux = ctx->reg_if_mux[mux];

	_seninf_ops->_reset(ctx, intf);
	mtk_cam_seninf_mux(ctx, mux);
	mtk_cam_seninf_set_mux_ctrl(ctx, mux,
				    0, 0, TEST_MODEL, pixel_mode);
	mtk_cam_seninf_set_top_mux_ctrl(ctx, mux, intf);

	mtk_cam_seninf_set_cammux_vc(ctx, cam_mux, 0, 0, 0, 0);
	mtk_cam_seninf_set_cammux_src(ctx, mux, cam_mux, 0, 0);
	mtk_cam_seninf_set_cammux_chk_pixel_mode(ctx, cam_mux, pixel_mode);
	mtk_cam_seninf_cammux(ctx, cam_mux);

	SENINF_BITS(pSeninf, SENINF_TESTMDL_CTRL, RG_SENINF_TESTMDL_EN, 1);
	SENINF_BITS(pSeninf, SENINF_CTRL, SENINF_EN, 1);

	SENINF_BITS(pSeninf_tg, TM_SIZE, TM_LINE, 4224);
	SENINF_BITS(pSeninf_tg, TM_SIZE, TM_PXL, 5632);
	SENINF_BITS(pSeninf_tg, TM_CLK, TM_CLK_CNT, 7);

	SENINF_BITS(pSeninf_tg, TM_DUM, TM_VSYNC, 100);
	SENINF_BITS(pSeninf_tg, TM_DUM, TM_DUMMYPXL, 100);

	SENINF_BITS(pSeninf_tg, TM_CTL, TM_PAT, 0xC);
	SENINF_BITS(pSeninf_tg, TM_CTL, TM_EN, 1);

	return 0;
}

static int csirx_phyA_power_on(struct seninf_ctx *ctx, int portIdx, int en)
{
	void *base = ctx->reg_ana_csi_rx[portIdx];

	SENINF_BITS(base, CDPHY_RX_ANA_8, RG_CSI0_L0_T0AB_EQ_OS_CAL_EN, 0);
	SENINF_BITS(base, CDPHY_RX_ANA_8, RG_CSI0_L1_T1AB_EQ_OS_CAL_EN, 0);
	SENINF_BITS(base, CDPHY_RX_ANA_8, RG_CSI0_L2_T1BC_EQ_OS_CAL_EN, 0);
	SENINF_BITS(base, CDPHY_RX_ANA_8, RG_CSI0_XX_T0BC_EQ_OS_CAL_EN, 0);
	SENINF_BITS(base, CDPHY_RX_ANA_8, RG_CSI0_XX_T0CA_EQ_OS_CAL_EN, 0);
	SENINF_BITS(base, CDPHY_RX_ANA_8, RG_CSI0_XX_T1CA_EQ_OS_CAL_EN, 0);
	SENINF_BITS(base, CDPHY_RX_ANA_0, RG_CSI0_BG_LPF_EN, 0);
	SENINF_BITS(base, CDPHY_RX_ANA_0, RG_CSI0_BG_CORE_EN, 0);
	udelay(200);

	if (en) {
		SENINF_BITS(base, CDPHY_RX_ANA_0,
			    RG_CSI0_BG_CORE_EN, 1);
		udelay(30);
		SENINF_BITS(base, CDPHY_RX_ANA_0,
			    RG_CSI0_BG_LPF_EN, 1);
		udelay(1);
		SENINF_BITS(base, CDPHY_RX_ANA_8,
			    RG_CSI0_L0_T0AB_EQ_OS_CAL_EN, 1);
		SENINF_BITS(base, CDPHY_RX_ANA_8,
			    RG_CSI0_L1_T1AB_EQ_OS_CAL_EN, 1);
		SENINF_BITS(base, CDPHY_RX_ANA_8,
			    RG_CSI0_L2_T1BC_EQ_OS_CAL_EN, 1);
		SENINF_BITS(base, CDPHY_RX_ANA_8,
			    RG_CSI0_XX_T0BC_EQ_OS_CAL_EN, 1);
		SENINF_BITS(base, CDPHY_RX_ANA_8,
			    RG_CSI0_XX_T0CA_EQ_OS_CAL_EN, 1);
		SENINF_BITS(base, CDPHY_RX_ANA_8,
			    RG_CSI0_XX_T1CA_EQ_OS_CAL_EN, 1);
		udelay(1);
#if LOG_MORE
		dev_info(ctx->dev, "portIdx %d en %d CDPHY_RX_ANA_0 0x%x ANA_8 0x%x\n",
			 portIdx, en,
			SENINF_READ_REG(base, CDPHY_RX_ANA_0),
			SENINF_READ_REG(base, CDPHY_RX_ANA_8));
#endif
	}


	return 0;
}
#ifdef CSI_EFUSE_SET
static int apply_efuse_data(struct seninf_ctx *ctx)
{
	int ret = 0;
	int port;
	void *base;
	unsigned int m_csi_efuse = ctx->m_csi_efuse;

	if (m_csi_efuse == 0) {
#if LOG_MORE
		dev_info(ctx->dev, "No efuse data. Returned.\n");
#endif
		return -1;
	}

	port = ctx->port;
	base = ctx->reg_ana_csi_rx[port];
	SENINF_BITS(base, CDPHY_RX_ANA_2,
		RG_CSI0_L0P_T0A_HSRT_CODE, (m_csi_efuse>>27) & 0x1f);
	SENINF_BITS(base, CDPHY_RX_ANA_2,
		RG_CSI0_L0N_T0B_HSRT_CODE, (m_csi_efuse>>27) & 0x1f);
	SENINF_BITS(base, CDPHY_RX_ANA_3,
		RG_CSI0_L1P_T0C_HSRT_CODE, (m_csi_efuse>>22) & 0x1f);
	SENINF_BITS(base, CDPHY_RX_ANA_3,
		RG_CSI0_L1N_T1A_HSRT_CODE, (m_csi_efuse>>22) & 0x1f);
	SENINF_BITS(base, CDPHY_RX_ANA_4,
		RG_CSI0_L2P_T1B_HSRT_CODE, (m_csi_efuse>>17) & 0x1f);
	SENINF_BITS(base, CDPHY_RX_ANA_4,
		RG_CSI0_L2N_T1C_HSRT_CODE, (m_csi_efuse>>17) & 0x1f);
	dev_info(ctx->dev,
		"CSI%dA CDPHY_RX_ANA_2(0x%x) CDPHY_RX_ANA_3(0x%x) CDPHY_RX_ANA_4(0x%x)",
		ctx->port,
		SENINF_READ_REG(base, CDPHY_RX_ANA_2),
		SENINF_READ_REG(base, CDPHY_RX_ANA_3),
		SENINF_READ_REG(base, CDPHY_RX_ANA_4));

	if (ctx->is_4d1c == 0)
		return ret;

	port = ctx->portB;
	base = ctx->reg_ana_csi_rx[port];
	SENINF_BITS(base, CDPHY_RX_ANA_2,
		RG_CSI0_L0P_T0A_HSRT_CODE, (m_csi_efuse>>12) & 0x1f);
	SENINF_BITS(base, CDPHY_RX_ANA_2,
		RG_CSI0_L0N_T0B_HSRT_CODE, (m_csi_efuse>>12) & 0x1f);
	SENINF_BITS(base, CDPHY_RX_ANA_3,
		RG_CSI0_L1P_T0C_HSRT_CODE, (m_csi_efuse>>7) & 0x1f);
	SENINF_BITS(base, CDPHY_RX_ANA_3,
		RG_CSI0_L1N_T1A_HSRT_CODE, (m_csi_efuse>>7) & 0x1f);
	if (port < CSI_PORT_2A) {
		SENINF_BITS(base, CDPHY_RX_ANA_4,
			RG_CSI0_L2P_T1B_HSRT_CODE, (m_csi_efuse>>2) & 0x1f);
		SENINF_BITS(base, CDPHY_RX_ANA_4,
			RG_CSI0_L2N_T1C_HSRT_CODE, (m_csi_efuse>>2) & 0x1f);
		dev_info(ctx->dev,
			"CSI%dB CDPHY_RX_ANA_2(0x%x) CDPHY_RX_ANA_3(0x%x) CDPHY_RX_ANA_4(0x%x)",
			ctx->port,
			SENINF_READ_REG(base, CDPHY_RX_ANA_2),
			SENINF_READ_REG(base, CDPHY_RX_ANA_3),
			SENINF_READ_REG(base, CDPHY_RX_ANA_4));
	} else
		dev_info(ctx->dev,
			"CSI%dB CDPHY_RX_ANA_2(0x%x) CDPHY_RX_ANA_3(0x%x)",
			ctx->port,
			SENINF_READ_REG(base, CDPHY_RX_ANA_2),
			SENINF_READ_REG(base, CDPHY_RX_ANA_3));

	return ret;
}
#endif
static int csirx_phyA_init(struct seninf_ctx *ctx)
{
	int i, port;
	void *base;

	port = ctx->port;
	for (i = 0; i <= ctx->is_4d1c; i++) {
		port = i ? ctx->portB : ctx->port;
		base = ctx->reg_ana_csi_rx[port];
		SENINF_BITS(base, CDPHY_RX_ANA_1,
			    RG_CSI0_BG_LPRX_VTL_SEL, 0x4);
		SENINF_BITS(base, CDPHY_RX_ANA_1,
			    RG_CSI0_BG_LPRX_VTH_SEL, 0x4);
		SENINF_BITS(base, CDPHY_RX_ANA_2,
			    RG_CSI0_BG_ALP_RX_VTL_SEL, 0x4);
		SENINF_BITS(base, CDPHY_RX_ANA_2,
			    RG_CSI0_BG_ALP_RX_VTH_SEL, 0x4);
		SENINF_BITS(base, CDPHY_RX_ANA_1,
			    RG_CSI0_BG_VREF_SEL, 0x8);
		SENINF_BITS(base, CDPHY_RX_ANA_1,
			    RG_CSI0_CDPHY_EQ_DES_VREF_SEL, 0x2);
		SENINF_BITS(base, CDPHY_RX_ANA_5,
			    RG_CSI0_CDPHY_EQ_BW, 0x3);
		SENINF_BITS(base, CDPHY_RX_ANA_5,
			    RG_CSI0_CDPHY_EQ_IS, 0x1);
		SENINF_BITS(base, CDPHY_RX_ANA_5,
			    RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
		SENINF_BITS(base, CDPHY_RX_ANA_5,
			    RG_CSI0_CDPHY_EQ_DG0_EN, 0x1);
		SENINF_BITS(base, CDPHY_RX_ANA_5,
			    RG_CSI0_CDPHY_EQ_DG1_EN, 0x1);
		SENINF_BITS(base, CDPHY_RX_ANA_5,
			    RG_CSI0_CDPHY_EQ_SR0, 0x0);
		SENINF_BITS(base, CDPHY_RX_ANA_5,
			    RG_CSI0_CDPHY_EQ_SR1, 0x0);
/* Todo
 *		SENINF_BITS(base, CDPHY_RX_ANA_9,
 *			    RG_CSI0_RESERVE, 0x3003);
 *		SENINF_BITS(base, CDPHY_RX_ANA_SETTING_0,
 *			    CSR_CSI_RST_MODE, 0x2);
 */
		//r50 termination
		SENINF_BITS(base, CDPHY_RX_ANA_2,
			    RG_CSI0_L0P_T0A_HSRT_CODE, 0x10);
		SENINF_BITS(base, CDPHY_RX_ANA_2,
			    RG_CSI0_L0N_T0B_HSRT_CODE, 0x10);
		SENINF_BITS(base, CDPHY_RX_ANA_3,
			    RG_CSI0_L1P_T0C_HSRT_CODE, 0x10);
		SENINF_BITS(base, CDPHY_RX_ANA_3,
			    RG_CSI0_L1N_T1A_HSRT_CODE, 0x10);
		SENINF_BITS(base, CDPHY_RX_ANA_4,
			    RG_CSI0_L2P_T1B_HSRT_CODE, 0x10);
		SENINF_BITS(base, CDPHY_RX_ANA_4,
			    RG_CSI0_L2N_T1C_HSRT_CODE, 0x10);
		SENINF_BITS(base, CDPHY_RX_ANA_0,
			    RG_CSI0_CPHY_T0_CDR_FIRST_EDGE_EN, 0x0);
		SENINF_BITS(base, CDPHY_RX_ANA_0,
			    RG_CSI0_CPHY_T1_CDR_FIRST_EDGE_EN, 0x0);
		SENINF_BITS(base, CDPHY_RX_ANA_2,
			    RG_CSI0_CPHY_T0_CDR_SELF_CAL_EN, 0x0);
		SENINF_BITS(base, CDPHY_RX_ANA_2,
			    RG_CSI0_CPHY_T1_CDR_SELF_CAL_EN, 0x0);

		SENINF_BITS(base, CDPHY_RX_ANA_6,
			    RG_CSI0_CPHY_T0_CDR_CK_DELAY, 0x4);//TODO
		SENINF_BITS(base, CDPHY_RX_ANA_7,
			    RG_CSI0_CPHY_T1_CDR_CK_DELAY, 0x4);//TODO
		SENINF_BITS(base, CDPHY_RX_ANA_6,
			    RG_CSI0_CPHY_T0_CDR_AB_WIDTH, 0x9);
		SENINF_BITS(base, CDPHY_RX_ANA_6,
			    RG_CSI0_CPHY_T0_CDR_BC_WIDTH, 0x9);
		SENINF_BITS(base, CDPHY_RX_ANA_6,
			    RG_CSI0_CPHY_T0_CDR_CA_WIDTH, 0x9);
		SENINF_BITS(base, CDPHY_RX_ANA_7,
			    RG_CSI0_CPHY_T1_CDR_AB_WIDTH, 0x9);
		SENINF_BITS(base, CDPHY_RX_ANA_7,
			    RG_CSI0_CPHY_T1_CDR_BC_WIDTH, 0x9);
		SENINF_BITS(base, CDPHY_RX_ANA_7,
			    RG_CSI0_CPHY_T1_CDR_CA_WIDTH, 0x9);

		// SENINF_BITS(base, CDPHY_RX_ANA_SETTING_1,
			// RG_CSI0_ASYNC_OPTION, 0xC);

	}

#ifdef CSI_EFUSE_SET
	apply_efuse_data(ctx);
#endif

	return 0;
}

#ifdef SCAN_SETTLE
static int set_trail(struct seninf_ctx *ctx, u16 hs_trail)
{
	void *base = ctx->reg_ana_dphy_top[ctx->port];
	void *pSeninf_cam_mux_gcsr = ctx->reg_if_cam_mux_gcsr;


	SENINF_BITS(base, DPHY_RX_DATA_LANE0_HS_PARAMETER,
		    RG_DPHY_RX_LD0_HS_TRAIL_PARAMETER, hs_trail);
	SENINF_BITS(base, DPHY_RX_DATA_LANE1_HS_PARAMETER,
		    RG_DPHY_RX_LD1_HS_TRAIL_PARAMETER, hs_trail);
	SENINF_BITS(base, DPHY_RX_DATA_LANE2_HS_PARAMETER,
		    RG_DPHY_RX_LD2_HS_TRAIL_PARAMETER, hs_trail);
	SENINF_BITS(base, DPHY_RX_DATA_LANE3_HS_PARAMETER,
		    RG_DPHY_RX_LD3_HS_TRAIL_PARAMETER, hs_trail);
	SENINF_BITS(base, DPHY_RX_DATA_LANE0_HS_PARAMETER,

			    RG_DPHY_RX_LD0_HS_TRAIL_EN, 1);
	SENINF_BITS(base, DPHY_RX_DATA_LANE1_HS_PARAMETER,
			    RG_DPHY_RX_LD1_HS_TRAIL_EN, 1);
	SENINF_BITS(base, DPHY_RX_DATA_LANE2_HS_PARAMETER,
			    RG_DPHY_RX_LD2_HS_TRAIL_EN, 1);
	SENINF_BITS(base, DPHY_RX_DATA_LANE3_HS_PARAMETER,
			    RG_DPHY_RX_LD3_HS_TRAIL_EN, 1);


	SENINF_BITS(pSeninf_cam_mux_gcsr, SENINF_CAM_MUX_GCSR_CTRL,
			RG_SENINF_CAM_MUX_GCSR_SW_RST, 1);
	udelay(1);
	SENINF_BITS(pSeninf_cam_mux_gcsr, SENINF_CAM_MUX_GCSR_CTRL,
			RG_SENINF_CAM_MUX_GCSR_SW_RST, 0);

	return 0;

}



static int set_settle(struct seninf_ctx *ctx, u16 settle, bool hs_trail_en)
{

	void *base = ctx->reg_ana_dphy_top[ctx->port];
	void *pSeninf_cam_mux_gcsr = ctx->reg_if_cam_mux_gcsr;


	int settle_delay_dt, settle_delay_ck;

	settle_delay_dt = settle;


	SENINF_BITS(base, DPHY_RX_DATA_LANE0_HS_PARAMETER,
		    RG_CDPHY_RX_LD0_TRIO0_HS_SETTLE_PARAMETER,
		    settle_delay_dt);
	SENINF_BITS(base, DPHY_RX_DATA_LANE1_HS_PARAMETER,
		    RG_CDPHY_RX_LD1_TRIO1_HS_SETTLE_PARAMETER,
		    settle_delay_dt);
	SENINF_BITS(base, DPHY_RX_DATA_LANE2_HS_PARAMETER,
		    RG_CDPHY_RX_LD2_TRIO2_HS_SETTLE_PARAMETER,
		    settle_delay_dt);
	SENINF_BITS(base, DPHY_RX_DATA_LANE3_HS_PARAMETER,
		    RG_CDPHY_RX_LD3_TRIO3_HS_SETTLE_PARAMETER,
		    settle_delay_dt);

	settle_delay_ck = settle;


	SENINF_BITS(base, DPHY_RX_CLOCK_LANE0_HS_PARAMETER,
		    RG_DPHY_RX_LC0_HS_SETTLE_PARAMETER,
		    settle_delay_ck);
	SENINF_BITS(base, DPHY_RX_CLOCK_LANE1_HS_PARAMETER,
		    RG_DPHY_RX_LC1_HS_SETTLE_PARAMETER,
		    settle_delay_ck);
	SENINF_BITS(base, DPHY_RX_DATA_LANE0_HS_PARAMETER,
			    RG_DPHY_RX_LD0_HS_TRAIL_EN, hs_trail_en);
	SENINF_BITS(base, DPHY_RX_DATA_LANE1_HS_PARAMETER,
			    RG_DPHY_RX_LD1_HS_TRAIL_EN, hs_trail_en);
	SENINF_BITS(base, DPHY_RX_DATA_LANE2_HS_PARAMETER,
			    RG_DPHY_RX_LD2_HS_TRAIL_EN, hs_trail_en);
	SENINF_BITS(base, DPHY_RX_DATA_LANE3_HS_PARAMETER,
			    RG_DPHY_RX_LD3_HS_TRAIL_EN, hs_trail_en);


	SENINF_BITS(pSeninf_cam_mux_gcsr, SENINF_CAM_MUX_GCSR_CTRL,
			RG_SENINF_CAM_MUX_GCSR_SW_RST, 1);
	udelay(1);
	SENINF_BITS(pSeninf_cam_mux_gcsr, SENINF_CAM_MUX_GCSR_CTRL,
			RG_SENINF_CAM_MUX_GCSR_SW_RST, 0);


	return 0;
}
#endif

static int csirx_dphy_init(struct seninf_ctx *ctx)
{
	void *base = ctx->reg_ana_dphy_top[ctx->port];
	int settle_delay_dt, settle_delay_ck, hs_trail, hs_trail_en;
	int bit_per_pixel = 10;
	struct seninf_vc *vc = mtk_cam_seninf_get_vc_by_pad(ctx, PAD_SRC_RAW0);
	struct seninf_vc *vc1 = mtk_cam_seninf_get_vc_by_pad(ctx, PAD_SRC_RAW_EXT0);

	if (vc)
		bit_per_pixel = vc->bit_depth;
	else if (vc1)
		bit_per_pixel = vc1->bit_depth;

	if (ctx->is_cphy) {
		if (ctx->csi_param.not_fixed_trail_settle) {
			settle_delay_dt = ctx->csi_param.cphy_settle
					? ctx->csi_param.cphy_settle
					: ctx->cphy_settle_delay_dt;
		} else {
			settle_delay_dt = ctx->csi_param.cphy_settle;
			if (settle_delay_dt == 0)
				settle_delay_dt = CPHY_SETTLE;
		}
		settle_delay_ck = settle_delay_dt;
	} else {

#if FIX_DPHY_SETTLE
		settle_delay_dt = settle_delay_ck = DPHY_SETTLE;
			// 0x1c = 100ns/(1/csi_ck)
#else

		settle_delay_dt = ctx->csi_param.dphy_data_settle
			? ctx->csi_param.dphy_data_settle
			: ctx->dphy_settle_delay_dt;
		settle_delay_ck = ctx->csi_param.dphy_clk_settle
				? ctx->csi_param.dphy_clk_settle
				: ctx->settle_delay_ck;
#endif
	}

	SENINF_BITS(base, DPHY_RX_DATA_LANE0_HS_PARAMETER,
		    RG_CDPHY_RX_LD0_TRIO0_HS_SETTLE_PARAMETER,
		    settle_delay_dt);
	SENINF_BITS(base, DPHY_RX_DATA_LANE1_HS_PARAMETER,
		    RG_CDPHY_RX_LD1_TRIO1_HS_SETTLE_PARAMETER,
		    settle_delay_dt);
	SENINF_BITS(base, DPHY_RX_DATA_LANE2_HS_PARAMETER,
		    RG_CDPHY_RX_LD2_TRIO2_HS_SETTLE_PARAMETER,
		    settle_delay_dt);
	SENINF_BITS(base, DPHY_RX_DATA_LANE3_HS_PARAMETER,
		    RG_CDPHY_RX_LD3_TRIO3_HS_SETTLE_PARAMETER,
		    settle_delay_dt);


	SENINF_BITS(base, DPHY_RX_CLOCK_LANE0_HS_PARAMETER,
		    RG_DPHY_RX_LC0_HS_SETTLE_PARAMETER,
		    settle_delay_ck);
	SENINF_BITS(base, DPHY_RX_CLOCK_LANE1_HS_PARAMETER,
		    RG_DPHY_RX_LC1_HS_SETTLE_PARAMETER,
		    settle_delay_ck);
#if __SMT
	/*Settle delay by lane*/
	SENINF_BITS(base, DPHY_RX_DATA_LANE0_HS_PARAMETER,
		    RG_CDPHY_RX_LD0_TRIO0_HS_PREPARE_PARAMETER, 2);
	SENINF_BITS(base, DPHY_RX_DATA_LANE1_HS_PARAMETER,
		    RG_CDPHY_RX_LD1_TRIO1_HS_PREPARE_PARAMETER, 2);
	SENINF_BITS(base, DPHY_RX_DATA_LANE2_HS_PARAMETER,
		    RG_CDPHY_RX_LD2_TRIO2_HS_PREPARE_PARAMETER, 2);
	SENINF_BITS(base, DPHY_RX_DATA_LANE3_HS_PARAMETER,
		    RG_CDPHY_RX_LD3_TRIO3_HS_PREPARE_PARAMETER, 2);
#else
	/*Settle delay by lane*/
	SENINF_BITS(base, DPHY_RX_DATA_LANE0_HS_PARAMETER,
		    RG_CDPHY_RX_LD0_TRIO0_HS_PREPARE_PARAMETER, 0);
	SENINF_BITS(base, DPHY_RX_DATA_LANE1_HS_PARAMETER,
		    RG_CDPHY_RX_LD1_TRIO1_HS_PREPARE_PARAMETER, 0);
	SENINF_BITS(base, DPHY_RX_DATA_LANE2_HS_PARAMETER,
		    RG_CDPHY_RX_LD2_TRIO2_HS_PREPARE_PARAMETER, 0);
	SENINF_BITS(base, DPHY_RX_DATA_LANE3_HS_PARAMETER,
		    RG_CDPHY_RX_LD3_TRIO3_HS_PREPARE_PARAMETER, 0);
#endif
	if (ctx->csi_param.not_fixed_trail_settle) {
		hs_trail = ctx->hs_trail_parameter;
		hs_trail =	ctx->csi_param.dphy_trail
				? ctx->csi_param.dphy_trail
				: ctx->hs_trail_parameter;
	} else
		hs_trail =  ctx->csi_param.dphy_trail;

	SENINF_BITS(base, DPHY_RX_DATA_LANE0_HS_PARAMETER,
		    RG_DPHY_RX_LD0_HS_TRAIL_PARAMETER, hs_trail);
	SENINF_BITS(base, DPHY_RX_DATA_LANE1_HS_PARAMETER,
		    RG_DPHY_RX_LD1_HS_TRAIL_PARAMETER, hs_trail);
	SENINF_BITS(base, DPHY_RX_DATA_LANE2_HS_PARAMETER,
		    RG_DPHY_RX_LD2_HS_TRAIL_PARAMETER, hs_trail);
	SENINF_BITS(base, DPHY_RX_DATA_LANE3_HS_PARAMETER,
		    RG_DPHY_RX_LD3_HS_TRAIL_PARAMETER, hs_trail);

	if (!ctx->is_cphy) {

		if (ctx->csi_param.not_fixed_trail_settle) {
			u64 data_rate = 0;

			data_rate = ctx->mipi_pixel_rate * bit_per_pixel;
			do_div(data_rate, ctx->num_data_lanes);
			hs_trail_en = data_rate < SENINF_HS_TRAIL_EN_CONDITION;
			if (ctx->csi_param.dphy_trail != 0) {
				hs_trail_en = 1;
				dev_info(ctx->dev, "hs_trail = %d\n", hs_trail);
			}
		} else {
			if (ctx->csi_param.dphy_trail != 0) {
				hs_trail_en = 1;
				dev_info(ctx->dev, "hs_trail = %d\n", hs_trail);
			} else
				hs_trail_en = 0;
		}
		SENINF_BITS(base, DPHY_RX_DATA_LANE0_HS_PARAMETER,
			    RG_DPHY_RX_LD0_HS_TRAIL_EN, hs_trail_en);
		SENINF_BITS(base, DPHY_RX_DATA_LANE1_HS_PARAMETER,
			    RG_DPHY_RX_LD1_HS_TRAIL_EN, hs_trail_en);
		SENINF_BITS(base, DPHY_RX_DATA_LANE2_HS_PARAMETER,
			    RG_DPHY_RX_LD2_HS_TRAIL_EN, hs_trail_en);
		SENINF_BITS(base, DPHY_RX_DATA_LANE3_HS_PARAMETER,
			    RG_DPHY_RX_LD3_HS_TRAIL_EN, hs_trail_en);
	}

	return 0;
}

static int csirx_cphy_init(struct seninf_ctx *ctx)
{
	void *base = ctx->reg_ana_cphy_top[ctx->port];

	SENINF_BITS(base, CPHY_RX_DETECT_CTRL_POST,
		    RG_CPHY_RX_DATA_VALID_POST_EN, 1);

	return 0;
}

static int csirx_phy_init(struct seninf_ctx *ctx)
{
	/* phyA init */
	csirx_phyA_init(ctx);

	/* phyD init */
	csirx_dphy_init(ctx);
	csirx_cphy_init(ctx);

	return 0;
}

static int csirx_seninf_csi2_setting(struct seninf_ctx *ctx)
{
	void *pSeninf_csi2 = ctx->reg_if_csi2[ctx->seninfIdx];
	int csi_en;
	int bit_per_pixel = 10;
	struct seninf_vc *vc = mtk_cam_seninf_get_vc_by_pad(ctx, PAD_SRC_RAW0);
	struct seninf_vc *vc1 = mtk_cam_seninf_get_vc_by_pad(ctx, PAD_SRC_RAW_EXT0);

	if (vc)
		bit_per_pixel = vc->bit_depth;
	else if (vc1)
		bit_per_pixel = vc1->bit_depth;


	SENINF_BITS(pSeninf_csi2, SENINF_CSI2_DBG_CTRL,
		    RG_CSI2_DBG_PACKET_CNT_EN, 1);

	// lane/trio count
	SENINF_BITS(pSeninf_csi2, SENINF_CSI2_RESYNC_MERGE_CTRL,
		    RG_CSI2_RESYNC_CYCLE_CNT_OPT, 1);

	csi_en = (1 << ctx->num_data_lanes) - 1;

	if (!ctx->is_cphy) { //Dphy

		u64 data_rate = ctx->mipi_pixel_rate * bit_per_pixel;
		u64 cycles = 64;

		cycles *= SENINF_CK;
		do_div(data_rate, ctx->num_data_lanes);
		do_div(cycles, data_rate);
		cycles += CYCLE_MARGIN;


		dev_info(ctx->dev,
		"%s data_rate %lld bps cycles %lld\n",
		__func__, data_rate, cycles);
		SENINF_BITS(pSeninf_csi2, SENINF_CSI2_OPT,
			    RG_CSI2_CPHY_SEL, 0);
		SENINF_WRITE_REG(pSeninf_csi2, SENINF_CSI2_EN, csi_en);
		SENINF_BITS(pSeninf_csi2, SENINF_CSI2_HDR_MODE_0,
			    RG_CSI2_HEADER_MODE, 0);
		SENINF_BITS(pSeninf_csi2, SENINF_CSI2_HDR_MODE_0,
			    RG_CSI2_HEADER_LEN, 0);
		SENINF_WRITE_REG(pSeninf_csi2,
			SENINF_CSI2_RESYNC_MERGE_CTRL, 0x2020f106);
#if __SMT == 0
		SENINF_BITS(pSeninf_csi2, SENINF_CSI2_RESYNC_MERGE_CTRL,
				RG_CSI2_RESYNC_DMY_CYCLE, cycles);

		SENINF_BITS(pSeninf_csi2, SENINF_CSI2_RESYNC_MERGE_CTRL,
				RG_CSI2_RESYNC_DMY_CNT,
				RESYNC_DMY_CNT);
		if (!ctx->csi_param.legacy_phy) {
			SENINF_BITS(pSeninf_csi2, SENINF_CSI2_RESYNC_MERGE_CTRL,
				RG_CSI2_RESYNC_DMY_CNT,
				3);
			if (ctx->num_data_lanes == 2) {
				SENINF_BITS(pSeninf_csi2,
					SENINF_CSI2_RESYNC_MERGE_CTRL,
					RG_CSI2_RESYNC_DMY_EN, 0x3);
			} else {
				SENINF_BITS(pSeninf_csi2,
					SENINF_CSI2_RESYNC_MERGE_CTRL,
					RG_CSI2_RESYNC_DMY_EN, 0xf);
			}
		} else {
			SENINF_BITS(pSeninf_csi2, SENINF_CSI2_RESYNC_MERGE_CTRL,
				RG_CSI2_RESYNC_DMY_CNT,
				4);
			SENINF_BITS(pSeninf_csi2, SENINF_CSI2_RESYNC_MERGE_CTRL,
				RG_CSI2_RESYNC_DMY_EN,
				0xF);
		}

#endif
	} else { //Cphy
		u8 map_hdr_len[] = {0, 1, 2, 4, 5};
		u64 cycles = 64;
		u64 data_rate = ctx->mipi_pixel_rate * bit_per_pixel;

		cycles *= SENINF_CK;
		data_rate *= 7;
		do_div(data_rate, ctx->num_data_lanes*16);
		do_div(cycles, data_rate);
		cycles += CYCLE_MARGIN;
		dev_info(ctx->dev,
		"%s data_rate %lld pps cycles %lld\n",
		__func__, data_rate, cycles);

		SENINF_WRITE_REG(pSeninf_csi2, SENINF_CSI2_EN, csi_en);
		SENINF_BITS(pSeninf_csi2, SENINF_CSI2_OPT,
			    RG_CSI2_CPHY_SEL, 1);
		SENINF_BITS(pSeninf_csi2, SENINF_CSI2_HDR_MODE_0,
			    RG_CSI2_HEADER_MODE, 2); //cphy
		SENINF_BITS(pSeninf_csi2, SENINF_CSI2_HDR_MODE_0,
			    RG_CSI2_HEADER_LEN,
			    map_hdr_len[ctx->num_data_lanes]);
		SENINF_WRITE_REG(pSeninf_csi2,
			SENINF_CSI2_RESYNC_MERGE_CTRL, 0x20207106);
#if __SMT == 0
		SENINF_BITS(pSeninf_csi2, SENINF_CSI2_RESYNC_MERGE_CTRL,
				RG_CSI2_RESYNC_DMY_CYCLE,
				cycles);
		SENINF_BITS(pSeninf_csi2, SENINF_CSI2_RESYNC_MERGE_CTRL,
				RG_CSI2_RESYNC_DMY_CNT,
				RESYNC_DMY_CNT);
		if (!ctx->csi_param.legacy_phy) {
			SENINF_BITS(pSeninf_csi2, SENINF_CSI2_RESYNC_MERGE_CTRL,
				RG_CSI2_RESYNC_DMY_CNT,
				3);
			if (ctx->num_data_lanes == 3) {
				SENINF_BITS(pSeninf_csi2,
					SENINF_CSI2_RESYNC_MERGE_CTRL,
					RG_CSI2_RESYNC_DMY_EN, 0x7);
			} else {
				SENINF_BITS(pSeninf_csi2,
					SENINF_CSI2_RESYNC_MERGE_CTRL,
					RG_CSI2_RESYNC_DMY_EN, 0x3);
			}
		} else {
			SENINF_BITS(pSeninf_csi2, SENINF_CSI2_RESYNC_MERGE_CTRL,
				RG_CSI2_RESYNC_DMY_CNT,
				4);
			SENINF_BITS(pSeninf_csi2, SENINF_CSI2_RESYNC_MERGE_CTRL,
				RG_CSI2_RESYNC_DMY_EN,
				0x7);
		}


#endif
	}

	return 0;
}

static int csirx_seninf_setting(struct seninf_ctx *ctx)
{
	void *pSeninf = ctx->reg_if_ctrl[ctx->seninfIdx];

	// enable/disable seninf csi2
	SENINF_BITS(pSeninf, SENINF_CSI2_CTRL, RG_SENINF_CSI2_EN, 1);

	// enable/disable seninf, enable after csi2, testmdl is done.
	SENINF_BITS(pSeninf, SENINF_CTRL, SENINF_EN, 1);

	return 0;
}

static int csirx_seninf_top_setting(struct seninf_ctx *ctx)
{
	void *pSeninf_top = ctx->reg_if_top;

	switch (ctx->port) {
	case CSI_PORT_0:
		SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI0,
			    RG_PHY_SENINF_MUX0_CPHY_MODE, 0); //4T
		break;
	case CSI_PORT_0A:
	case CSI_PORT_0B:
		SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI0,
			    RG_PHY_SENINF_MUX0_CPHY_MODE, 2); //2T+2T
		break;
	case CSI_PORT_1:
		SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI1,
			    RG_PHY_SENINF_MUX1_CPHY_MODE, 0); //4T
		break;
	case CSI_PORT_1A:
	case CSI_PORT_1B:
		SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI1,
			    RG_PHY_SENINF_MUX1_CPHY_MODE, 2); //2T+2T
		break;
	case CSI_PORT_2:
		SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI2,
			    RG_PHY_SENINF_MUX2_CPHY_MODE, 0); //4T
		break;
	case CSI_PORT_2A:
	case CSI_PORT_2B:
		SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI2,
				RG_PHY_SENINF_MUX2_CPHY_MODE, 2); //2T+2T
		break;

	case CSI_PORT_3:
		SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI3,
			    RG_PHY_SENINF_MUX3_CPHY_MODE, 0); //4T
		break;
	case CSI_PORT_3A:
	case CSI_PORT_3B:
		SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI3,
				RG_PHY_SENINF_MUX3_CPHY_MODE, 2); //2T+2T
		break;
	case CSI_PORT_4:
		SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI4,
			RG_PHY_SENINF_MUX4_CPHY_MODE, 0); //4T
	break;
	case CSI_PORT_4A:
	case CSI_PORT_4B:
		SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI4,
				RG_PHY_SENINF_MUX4_CPHY_MODE, 2); //2T+2T
		break;

	case CSI_PORT_5:
		SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI5,
			RG_PHY_SENINF_MUX5_CPHY_MODE, 0); //4T

		break;
	case CSI_PORT_5A:
	case CSI_PORT_5B:
		SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI5,
				RG_PHY_SENINF_MUX5_CPHY_MODE, 2); //2T+2T
		break;
	default:
		break;
	}

	// port operation mode
	switch (ctx->port) {
	case CSI_PORT_0:
	case CSI_PORT_0A:
	case CSI_PORT_0B:
		if (!ctx->is_cphy) { //Dphy
			SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI0,
				    PHY_SENINF_MUX0_CPHY_EN, 0);
			SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI0,
				    PHY_SENINF_MUX0_DPHY_EN, 1);
		} else {
			SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI0,
				    PHY_SENINF_MUX0_DPHY_EN, 0);
			SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI0,
				    PHY_SENINF_MUX0_CPHY_EN, 1);
		}
		break;
	case CSI_PORT_1:
	case CSI_PORT_1A:
	case CSI_PORT_1B:
		if (!ctx->is_cphy) { //Dphy
			SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI1,
				    PHY_SENINF_MUX1_CPHY_EN, 0);
			SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI1,
				    PHY_SENINF_MUX1_DPHY_EN, 1);
		} else {
			SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI1,
				    PHY_SENINF_MUX1_DPHY_EN, 0);
			SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI1,
				    PHY_SENINF_MUX1_CPHY_EN, 1);
		}
		break;
	case CSI_PORT_2:
	case CSI_PORT_2A:
	case CSI_PORT_2B:

		if (!ctx->is_cphy) { //Dphy
			SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI2,
				    PHY_SENINF_MUX2_CPHY_EN, 0);
			SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI2,
				    PHY_SENINF_MUX2_DPHY_EN, 1);
		} else {
			SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI2,
				    PHY_SENINF_MUX2_DPHY_EN, 0);
			SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI2,
				    PHY_SENINF_MUX2_CPHY_EN, 1);
		}
		break;
	case CSI_PORT_3:
	case CSI_PORT_3A:
	case CSI_PORT_3B:

		if (!ctx->is_cphy) { //Dphy
			SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI3,
				    PHY_SENINF_MUX3_CPHY_EN, 0);
			SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI3,
				    PHY_SENINF_MUX3_DPHY_EN, 1);
		} else {
			SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI3,
				    PHY_SENINF_MUX3_DPHY_EN, 0);
			SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI3,
				    PHY_SENINF_MUX3_CPHY_EN, 1);
		}
		break;
	case CSI_PORT_4:
	case CSI_PORT_4A:
	case CSI_PORT_4B:

		if (!ctx->is_cphy) { //Dphy
			SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI4,
					PHY_SENINF_MUX4_CPHY_EN, 0);
			SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI4,
					PHY_SENINF_MUX4_DPHY_EN, 1);
		} else {
			SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI4,
					PHY_SENINF_MUX4_DPHY_EN, 0);
			SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI4,
					PHY_SENINF_MUX4_CPHY_EN, 1);
		}
		break;
	case CSI_PORT_5:
	case CSI_PORT_5A:
	case CSI_PORT_5B:

		if (!ctx->is_cphy) { //Dphy
			SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI5,
					PHY_SENINF_MUX5_CPHY_EN, 0);
			SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI5,
					PHY_SENINF_MUX5_DPHY_EN, 1);
		} else {
			SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI5,
					PHY_SENINF_MUX5_DPHY_EN, 0);
			SENINF_BITS(pSeninf_top, SENINF_TOP_PHY_CTRL_CSI5,
					PHY_SENINF_MUX5_CPHY_EN, 1);
		}
		break;

	default:
		break;
	}

	return 0;
}
#if __SMT
static int csirx_phyA_setting(struct seninf_ctx *ctx)
{
	void *base, *baseA, *baseB;

	base = ctx->reg_ana_csi_rx[ctx->port];
	baseA = ctx->reg_ana_csi_rx[ctx->portA];
	baseB = ctx->reg_ana_csi_rx[ctx->portB];

	if (!ctx->is_cphy) { //Dphy
		if (ctx->is_4d1c) {
			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_EN, 0);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_EN, 0);
			// clear clk sel first
			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L0_CKMODE_EN, 0);
			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L1_CKMODE_EN, 0);
			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L2_CKMODE_EN, 0);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L0_CKMODE_EN, 0);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L1_CKMODE_EN, 0);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L2_CKMODE_EN, 0);

			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L0_CKSEL, 1);
			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L1_CKSEL, 1);
			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L2_CKSEL, 1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L0_CKSEL, 1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L1_CKSEL, 1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L2_CKSEL, 1);

			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L0_CKMODE_EN, 0);
			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L1_CKMODE_EN, 0);
			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L2_CKMODE_EN, 1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L0_CKMODE_EN, 0);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L1_CKMODE_EN, 0);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L2_CKMODE_EN, 0);

			SENINF_BITS(baseA, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_BW, 0x3);
			SENINF_BITS(baseA, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_IS, 0x1);
			SENINF_BITS(baseA, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
			SENINF_BITS(baseA, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_DG0_EN, 0x1);
			SENINF_BITS(baseA, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_DG1_EN, 0x1);
			SENINF_BITS(baseA, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_SR0, 0x1);
			SENINF_BITS(baseA, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_SR1, 0x0);

			SENINF_BITS(baseB, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_BW, 0x3);
			SENINF_BITS(baseB, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_IS, 0x1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_DG0_EN, 0x1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_DG1_EN, 0x1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_SR0, 0x1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_SR1, 0x0);
		} else {
			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_EN, 0);
			// clear clk sel first
			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L0_CKMODE_EN, 0);
			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L1_CKMODE_EN, 0);
			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L2_CKMODE_EN, 0);

			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L0_CKSEL, 0);
			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L1_CKSEL, 0);
			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L2_CKSEL, 0);

			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L0_CKMODE_EN, 0);
			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L1_CKMODE_EN, 1);
			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L2_CKMODE_EN, 0);

			SENINF_BITS(base, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_BW, 0x3);
			SENINF_BITS(base, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_IS, 0x1);
			SENINF_BITS(base, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
			SENINF_BITS(base, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_DG0_EN, 0x1);
			SENINF_BITS(base, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_DG1_EN, 0x1);
			SENINF_BITS(base, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_SR0, 0x1);
			SENINF_BITS(base, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_SR1, 0x0);
		}
	} else { //Cphy
		if (ctx->is_4d1c) {
			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_EN, 1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_EN, 1);

			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_T0_HSMODE_EN, 1);
			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_T1_HSMODE_EN, 1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_T0_HSMODE_EN, 1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_T1_HSMODE_EN, 1);
#if __SMT
			SENINF_BITS(baseA, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_BW, 0x3);
			SENINF_BITS(baseA, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_IS, 0x1);
			SENINF_BITS(baseA, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
			SENINF_BITS(baseA, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_DG0_EN, 0x1);
			SENINF_BITS(baseA, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_DG1_EN, 0x0);
			SENINF_BITS(baseA, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_SR0, 0x3);
			SENINF_BITS(baseA, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_SR1, 0x0);

			SENINF_BITS(baseB, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_BW, 0x3);
			SENINF_BITS(baseB, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_IS, 0x1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_DG0_EN, 0x1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_DG1_EN, 0x0);
			SENINF_BITS(baseB, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_SR0, 0x3);
			SENINF_BITS(baseB, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_SR1, 0x0);
#else
			SENINF_WRITE_REG(baseA, CDPHY_RX_ANA_5, 0x77);
			SENINF_WRITE_REG(baseB, CDPHY_RX_ANA_5, 0x77);
			SENINF_WRITE_REG(baseA, CDPHY_RX_ANA_SETTING_0, 0x322);
			SENINF_WRITE_REG(baseB, CDPHY_RX_ANA_SETTING_0, 0x322);
#endif

			SENINF_BITS(baseA, CDPHY_RX_ANA_SETTING_1,
			    RG_CSI0_ASYNC_OPTION, 0xC);

			SENINF_BITS(baseA, CDPHY_RX_ANA_3,
				    RG_CSI0_EQ_DES_VREF_SEL, 0x2E);

			SENINF_BITS(baseA, CDPHY_RX_ANA_4,
					RG_CSI0_CPHY_T0_CDR_RSTB_CODE, 0x0);
			SENINF_BITS(baseA, CDPHY_RX_ANA_4,
					RG_CSI0_CPHY_T0_CDR_SEC_EDGE_CODE, 0x4);
			SENINF_BITS(baseA, CDPHY_RX_ANA_4,
					RG_CSI0_CPHY_T1_CDR_RSTB_CODE, 0x0);
			SENINF_BITS(baseA, CDPHY_RX_ANA_4,
					RG_CSI0_CPHY_T1_CDR_SEC_EDGE_CODE, 0x4);
			SENINF_BITS(baseB, CDPHY_RX_ANA_3,
				    RG_CSI0_EQ_DES_VREF_SEL, 0x2E);

			SENINF_BITS(baseB, CDPHY_RX_ANA_4,
					RG_CSI0_CPHY_T0_CDR_RSTB_CODE, 0x0);
			SENINF_BITS(baseB, CDPHY_RX_ANA_4,
					RG_CSI0_CPHY_T0_CDR_SEC_EDGE_CODE, 0x4);
			SENINF_BITS(baseB, CDPHY_RX_ANA_4,
					RG_CSI0_CPHY_T1_CDR_RSTB_CODE, 0x0);
			SENINF_BITS(baseB, CDPHY_RX_ANA_4,
					RG_CSI0_CPHY_T1_CDR_SEC_EDGE_CODE, 0x4);

		} else {
			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_EN, 1);
			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_T0_HSMODE_EN, 1);
			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_T1_HSMODE_EN, 1);

#if __SMT
			SENINF_BITS(base, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_BW, 0x3);
			SENINF_BITS(base, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_IS, 0x1);
			SENINF_BITS(base, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
			SENINF_BITS(base, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_DG0_EN, 0x1);
			SENINF_BITS(base, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_DG1_EN, 0x0);
			SENINF_BITS(base, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_SR0, 0x3);
			SENINF_BITS(base, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_SR1, 0x0);
#else
			SENINF_WRITE_REG(base, CDPHY_RX_ANA_5, 0x77);
			SENINF_WRITE_REG(base, CDPHY_RX_ANA_SETTING_0, 0x322);
#endif
			SENINF_BITS(base, CDPHY_RX_ANA_SETTING_1,
				RG_CSI0_ASYNC_OPTION, 0xC);

			SENINF_BITS(base, CDPHY_RX_ANA_3,
				RG_CSI0_EQ_DES_VREF_SEL, 0x2E);
			SENINF_BITS(base, CDPHY_RX_ANA_4,
				RG_CSI0_CPHY_T0_CDR_RSTB_CODE, 0x0);
			SENINF_BITS(base, CDPHY_RX_ANA_4,
				RG_CSI0_CPHY_T0_CDR_SEC_EDGE_CODE, 0x4);
			SENINF_BITS(base, CDPHY_RX_ANA_4,
				RG_CSI0_CPHY_T1_CDR_RSTB_CODE, 0x0);
			SENINF_BITS(base, CDPHY_RX_ANA_4,
				RG_CSI0_CPHY_T1_CDR_SEC_EDGE_CODE, 0x4);
		}
	}

	/* phyA power on */

	if (ctx->is_4d1c) {
		csirx_phyA_power_on(ctx, ctx->portA, 1);
		csirx_phyA_power_on(ctx, ctx->portB, 1);
	} else {
		csirx_phyA_power_on(ctx, ctx->port, 1);
	}

	return 0;
}
#else
static int csirx_phyA_setting(struct seninf_ctx *ctx)
{
	void *base, *baseA, *baseB;
	struct seninf_vc *vc = mtk_cam_seninf_get_vc_by_pad(ctx, PAD_SRC_RAW0);
	struct seninf_vc *vc1 = mtk_cam_seninf_get_vc_by_pad(ctx, PAD_SRC_RAW_EXT0);
	int bit_per_pixel = 10;

	if (vc)
		bit_per_pixel = vc->bit_depth;
	else if (vc1)
		bit_per_pixel = vc1->bit_depth;

	base = ctx->reg_ana_csi_rx[ctx->port];
	baseA = ctx->reg_ana_csi_rx[ctx->portA];
	baseB = ctx->reg_ana_csi_rx[ctx->portB];


	//dev_info(ctx->dev, "port %d A %d B %d\n", ctx->port, ctx->portA, ctx->portB);

	if (!ctx->is_cphy) { //Dphy
		u64 data_rate = ctx->mipi_pixel_rate * bit_per_pixel;

		do_div(data_rate, ctx->num_data_lanes);
		//dev_info(ctx->dev, "data_rate %llu bps\n", data_rate);

		if (!ctx->csi_param.legacy_phy) {
			SENINF_BITS(baseA, CDPHY_RX_ANA_SETTING_1,
			    RG_AFIFO_DUMMY_VALID_EN, 0x1);
			SENINF_BITS(baseA, CDPHY_RX_ANA_SETTING_1,
			    RG_CSI0_ASYNC_OPTION, 0x5);
			SENINF_BITS(baseA, CDPHY_RX_ANA_SETTING_1,
			    RG_AFIFO_DUMMY_VALID_PREPARE_NUM, 0x4);
			SENINF_BITS(baseA, CDPHY_RX_ANA_SETTING_1,
				RG_AFIFO_DUMMY_VALID_NUM, 0x1);
		} else {
			SENINF_BITS(baseA, CDPHY_RX_ANA_SETTING_1,
			    RG_AFIFO_DUMMY_VALID_EN, 0x0);
			SENINF_BITS(baseA, CDPHY_RX_ANA_SETTING_1,
			    RG_CSI0_ASYNC_OPTION, 0x5);
			SENINF_BITS(baseA, CDPHY_RX_ANA_SETTING_1,
			    RG_AFIFO_DUMMY_VALID_PREPARE_NUM, 0x3);
			SENINF_BITS(baseA, CDPHY_RX_ANA_SETTING_1,
				RG_AFIFO_DUMMY_VALID_NUM, 0x5);
		}
		if (ctx->is_4d1c) {
			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_EN, 0);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_EN, 0);
			// clear clk sel first
			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L0_CKMODE_EN, 0);
			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L1_CKMODE_EN, 0);
			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L2_CKMODE_EN, 0);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L0_CKMODE_EN, 0);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L1_CKMODE_EN, 0);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L2_CKMODE_EN, 0);

			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L0_CKSEL, 1);
			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L1_CKSEL, 1);
			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L2_CKSEL, 1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L0_CKSEL, 1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L1_CKSEL, 1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L2_CKSEL, 1);

			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L0_CKMODE_EN, 0);
			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L1_CKMODE_EN, 0);
			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L2_CKMODE_EN, 1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L0_CKMODE_EN, 0);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L1_CKMODE_EN, 0);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L2_CKMODE_EN, 0);

			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_T0_HSMODE_EN, 1);
			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_T1_HSMODE_EN, 1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_T0_HSMODE_EN, 1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_T1_HSMODE_EN, 1);



			if (data_rate < 2500000000) {
				SENINF_BITS(baseA, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_SR1, 0x0);
				SENINF_BITS(baseA, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_SR0, 0x0);
				SENINF_BITS(baseA, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
				SENINF_BITS(baseA, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_DG1_EN, 0x0);
				SENINF_BITS(baseA, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_DG0_EN, 0x0);
				SENINF_BITS(baseA, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_IS, 0x1);
				SENINF_BITS(baseA, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_BW, 0x0);

				SENINF_BITS(baseB, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_SR1, 0x0);
				SENINF_BITS(baseB, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_SR0, 0x0);
				SENINF_BITS(baseB, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
				SENINF_BITS(baseB, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_DG1_EN, 0x0);
				SENINF_BITS(baseB, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_DG0_EN, 0x0);
				SENINF_BITS(baseB, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_IS, 0x1);
				SENINF_BITS(baseB, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_BW, 0x0);
			} else if (data_rate < 4500000000)  {
				SENINF_BITS(baseA, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_SR1, 0x0);
				SENINF_BITS(baseA, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_SR0, 0x0);
				SENINF_BITS(baseA, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
				SENINF_BITS(baseA, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_DG1_EN, 0x0);
				SENINF_BITS(baseA, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_DG0_EN, 0x1);
				SENINF_BITS(baseA, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_IS, 0x1);
				SENINF_BITS(baseA, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_BW, 0x1);

				SENINF_BITS(baseB, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_SR1, 0x0);
				SENINF_BITS(baseB, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_SR0, 0x0);
				SENINF_BITS(baseB, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
				SENINF_BITS(baseB, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_DG1_EN, 0x0);
				SENINF_BITS(baseB, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_DG0_EN, 0x1);
				SENINF_BITS(baseB, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_IS, 0x1);
				SENINF_BITS(baseB, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_BW, 0x1);
			} else {
				SENINF_BITS(baseA, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_SR1, 0x0);
				SENINF_BITS(baseA, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_SR0, 0x1);
				SENINF_BITS(baseA, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
				SENINF_BITS(baseA, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_DG1_EN, 0x0);
				SENINF_BITS(baseA, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_DG0_EN, 0x1);
				SENINF_BITS(baseA, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_IS, 0x1);
				SENINF_BITS(baseA, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_BW, 0x3);

				SENINF_BITS(baseB, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_SR1, 0x0);
				SENINF_BITS(baseB, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_SR0, 0x1);
				SENINF_BITS(baseB, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
				SENINF_BITS(baseB, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_DG1_EN, 0x0);
				SENINF_BITS(baseB, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_DG0_EN, 0x1);
				SENINF_BITS(baseB, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_IS, 0x1);
				SENINF_BITS(baseB, CDPHY_RX_ANA_5,
					    RG_CSI0_CDPHY_EQ_BW, 0x3);
			}

		} else {
			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_EN, 0);
			// clear clk sel first
			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L0_CKMODE_EN, 0);
			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L1_CKMODE_EN, 0);
			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L2_CKMODE_EN, 0);

			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L0_CKSEL, 0);
			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L1_CKSEL, 0);
			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L2_CKSEL, 0);

			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L0_CKMODE_EN, 0);
			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L1_CKMODE_EN, 1);
			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_DPHY_L2_CKMODE_EN, 0);
			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_T0_HSMODE_EN, 1);
			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_T1_HSMODE_EN, 1);



			if (data_rate < 2500000000) {
				SENINF_BITS(base, CDPHY_RX_ANA_5,
						RG_CSI0_CDPHY_EQ_SR1, 0x0);
				SENINF_BITS(base, CDPHY_RX_ANA_5,
						RG_CSI0_CDPHY_EQ_SR0, 0x0);
				SENINF_BITS(base, CDPHY_RX_ANA_5,
						RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
				SENINF_BITS(base, CDPHY_RX_ANA_5,
						RG_CSI0_CDPHY_EQ_DG1_EN, 0x0);
				SENINF_BITS(base, CDPHY_RX_ANA_5,
						RG_CSI0_CDPHY_EQ_DG0_EN, 0x0);
				SENINF_BITS(base, CDPHY_RX_ANA_5,
						RG_CSI0_CDPHY_EQ_IS, 0x1);
				SENINF_BITS(base, CDPHY_RX_ANA_5,
					RG_CSI0_CDPHY_EQ_BW, 0x0);
			} else if (data_rate < 4500000000)	{

				SENINF_BITS(base, CDPHY_RX_ANA_5,
						RG_CSI0_CDPHY_EQ_SR1, 0x0);
				SENINF_BITS(base, CDPHY_RX_ANA_5,
						RG_CSI0_CDPHY_EQ_SR0, 0x0);
				SENINF_BITS(base, CDPHY_RX_ANA_5,
						RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
				SENINF_BITS(base, CDPHY_RX_ANA_5,
						RG_CSI0_CDPHY_EQ_DG1_EN, 0x0);
				SENINF_BITS(base, CDPHY_RX_ANA_5,
						RG_CSI0_CDPHY_EQ_DG0_EN, 0x1);
				SENINF_BITS(base, CDPHY_RX_ANA_5,
						RG_CSI0_CDPHY_EQ_IS, 0x1);
				SENINF_BITS(base, CDPHY_RX_ANA_5,
						RG_CSI0_CDPHY_EQ_BW, 0x1);
			} else {

				SENINF_BITS(base, CDPHY_RX_ANA_5,
						RG_CSI0_CDPHY_EQ_SR1, 0x0);
				SENINF_BITS(base, CDPHY_RX_ANA_5,
						RG_CSI0_CDPHY_EQ_SR0, 0x1);
				SENINF_BITS(base, CDPHY_RX_ANA_5,
						RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
				SENINF_BITS(base, CDPHY_RX_ANA_5,
						RG_CSI0_CDPHY_EQ_DG1_EN, 0x0);
				SENINF_BITS(base, CDPHY_RX_ANA_5,
						RG_CSI0_CDPHY_EQ_DG0_EN, 0x1);
				SENINF_BITS(base, CDPHY_RX_ANA_5,
						RG_CSI0_CDPHY_EQ_IS, 0x1);
				SENINF_BITS(base, CDPHY_RX_ANA_5,
						RG_CSI0_CDPHY_EQ_BW, 0x3);
			}


		}
	} else { //Cphy
		u64 data_rate = ctx->mipi_pixel_rate * bit_per_pixel;

		data_rate *= 7;
		do_div(data_rate, ctx->num_data_lanes*16);
		/* dev_info(ctx->dev,
		 *	"%s data_rate %llu bps\n",
		 *	__func__, data_rate);
		 */

		if (ctx->is_4d1c) {
			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_EN, 1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_EN, 1);

			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_T0_HSMODE_EN, 1);
			SENINF_BITS(baseA, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_T1_HSMODE_EN, 1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_T0_HSMODE_EN, 1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_T1_HSMODE_EN, 1);
#if __SMT
			SENINF_BITS(baseA, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_BW, 0x3);
			SENINF_BITS(baseA, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_IS, 0x1);
			SENINF_BITS(baseA, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
			SENINF_BITS(baseA, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_DG0_EN, 0x1);
			SENINF_BITS(baseA, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_DG1_EN, 0x0);
			SENINF_BITS(baseA, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_SR0, 0x3);
			SENINF_BITS(baseA, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_SR1, 0x0);

			SENINF_BITS(baseB, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_BW, 0x3);
			SENINF_BITS(baseB, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_IS, 0x1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_DG0_EN, 0x1);
			SENINF_BITS(baseB, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_DG1_EN, 0x0);
			SENINF_BITS(baseB, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_SR0, 0x3);
			SENINF_BITS(baseB, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_SR1, 0x0);
#else
			SENINF_WRITE_REG(baseA, CDPHY_RX_ANA_5, 0x157);
			SENINF_WRITE_REG(baseB, CDPHY_RX_ANA_5, 0x157);
			SENINF_WRITE_REG(baseA, CDPHY_RX_ANA_SETTING_0, 0x322);
			SENINF_WRITE_REG(baseB, CDPHY_RX_ANA_SETTING_0, 0x322);
#endif

			if (!ctx->csi_param.legacy_phy) {
				SENINF_BITS(baseA, CDPHY_RX_ANA_SETTING_1,
				    RG_AFIFO_DUMMY_VALID_EN, 0x1);
				SENINF_BITS(baseA, CDPHY_RX_ANA_SETTING_1,
				    RG_CSI0_ASYNC_OPTION, 0xC);
				SENINF_BITS(baseA, CDPHY_RX_ANA_SETTING_1,
				    RG_AFIFO_DUMMY_VALID_PREPARE_NUM, 0x2);
				SENINF_BITS(baseA, CDPHY_RX_ANA_SETTING_1,
					RG_AFIFO_DUMMY_VALID_NUM, 0x1);
			} else {
				SENINF_BITS(baseA, CDPHY_RX_ANA_SETTING_1,
				    RG_AFIFO_DUMMY_VALID_EN, 0x0);
				SENINF_BITS(baseA, CDPHY_RX_ANA_SETTING_1,
				    RG_CSI0_ASYNC_OPTION, 0xC);
				SENINF_BITS(baseA, CDPHY_RX_ANA_SETTING_1,
				    RG_AFIFO_DUMMY_VALID_PREPARE_NUM, 0x3);
				SENINF_BITS(baseA, CDPHY_RX_ANA_SETTING_1,
					RG_AFIFO_DUMMY_VALID_NUM, 0x5);
			}


			SENINF_BITS(baseA, CDPHY_RX_ANA_3,
				    RG_CSI0_EQ_DES_VREF_SEL, 0x2E);

			SENINF_BITS(baseB, CDPHY_RX_ANA_3,
				    RG_CSI0_EQ_DES_VREF_SEL, 0x2E);

			if (data_rate < 2500000000) {
				SENINF_BITS(baseA, CDPHY_RX_ANA_4,
						RG_CSI0_CPHY_T0_CDR_RSTB_CODE, 0x1);
				SENINF_BITS(baseA, CDPHY_RX_ANA_4,
						RG_CSI0_CPHY_T0_CDR_SEC_EDGE_CODE, 0x2);
				SENINF_BITS(baseA, CDPHY_RX_ANA_4,
						RG_CSI0_CPHY_T1_CDR_RSTB_CODE, 0x1);
				SENINF_BITS(baseA, CDPHY_RX_ANA_4,
						RG_CSI0_CPHY_T1_CDR_SEC_EDGE_CODE, 0x2);
				SENINF_BITS(baseA, CDPHY_RX_ANA_6,
					    RG_CSI0_CPHY_T0_CDR_CK_DELAY, 0xA);//TODO
				SENINF_BITS(baseA, CDPHY_RX_ANA_7,
					    RG_CSI0_CPHY_T1_CDR_CK_DELAY, 0xA);//TODO


				SENINF_BITS(baseB, CDPHY_RX_ANA_4,
						RG_CSI0_CPHY_T0_CDR_RSTB_CODE, 0x1);
				SENINF_BITS(baseB, CDPHY_RX_ANA_4,
						RG_CSI0_CPHY_T0_CDR_SEC_EDGE_CODE, 0x2);
				SENINF_BITS(baseB, CDPHY_RX_ANA_4,
						RG_CSI0_CPHY_T1_CDR_RSTB_CODE, 0x1);
				SENINF_BITS(baseB, CDPHY_RX_ANA_4,
						RG_CSI0_CPHY_T1_CDR_SEC_EDGE_CODE, 0x2);
				SENINF_BITS(baseB, CDPHY_RX_ANA_6,
					    RG_CSI0_CPHY_T0_CDR_CK_DELAY, 0xA);//TODO
				SENINF_BITS(baseB, CDPHY_RX_ANA_7,
					    RG_CSI0_CPHY_T1_CDR_CK_DELAY, 0xA);//TODO
			} else {
				SENINF_BITS(baseA, CDPHY_RX_ANA_4,
						RG_CSI0_CPHY_T0_CDR_RSTB_CODE, 0x0);
				SENINF_BITS(baseA, CDPHY_RX_ANA_4,
						RG_CSI0_CPHY_T0_CDR_SEC_EDGE_CODE, 0x4);
				SENINF_BITS(baseA, CDPHY_RX_ANA_4,
						RG_CSI0_CPHY_T1_CDR_RSTB_CODE, 0x0);
				SENINF_BITS(baseA, CDPHY_RX_ANA_4,
						RG_CSI0_CPHY_T1_CDR_SEC_EDGE_CODE, 0x4);

				SENINF_BITS(baseB, CDPHY_RX_ANA_4,
						RG_CSI0_CPHY_T0_CDR_RSTB_CODE, 0x0);
				SENINF_BITS(baseB, CDPHY_RX_ANA_4,
						RG_CSI0_CPHY_T0_CDR_SEC_EDGE_CODE, 0x4);
				SENINF_BITS(baseB, CDPHY_RX_ANA_4,
						RG_CSI0_CPHY_T1_CDR_RSTB_CODE, 0x0);
				SENINF_BITS(baseB, CDPHY_RX_ANA_4,
						RG_CSI0_CPHY_T1_CDR_SEC_EDGE_CODE, 0x4);
			}

		} else {
			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_EN, 1);
			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_T0_HSMODE_EN, 1);
			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_T1_HSMODE_EN, 1);

#if __SMT
			SENINF_BITS(base, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_BW, 0x3);
			SENINF_BITS(base, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_IS, 0x1);
			SENINF_BITS(base, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_LATCH_EN, 0x1);
			SENINF_BITS(base, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_DG0_EN, 0x1);
			SENINF_BITS(base, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_DG1_EN, 0x0);
			SENINF_BITS(base, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_SR0, 0x3);
			SENINF_BITS(base, CDPHY_RX_ANA_5,
				    RG_CSI0_CDPHY_EQ_SR1, 0x0);
#else
			SENINF_WRITE_REG(base, CDPHY_RX_ANA_5, 0x157);
			SENINF_WRITE_REG(base, CDPHY_RX_ANA_SETTING_0, 0x322);
#endif


			if (!ctx->csi_param.legacy_phy) {
				SENINF_BITS(baseA, CDPHY_RX_ANA_SETTING_1,
				    RG_AFIFO_DUMMY_VALID_EN, 0x1);
				SENINF_BITS(baseA, CDPHY_RX_ANA_SETTING_1,
				    RG_CSI0_ASYNC_OPTION, 0xC);
				SENINF_BITS(baseA, CDPHY_RX_ANA_SETTING_1,
				    RG_AFIFO_DUMMY_VALID_PREPARE_NUM, 0x2);
				SENINF_BITS(baseA, CDPHY_RX_ANA_SETTING_1,
					RG_AFIFO_DUMMY_VALID_NUM, 0x1);
			} else {
				SENINF_BITS(baseA, CDPHY_RX_ANA_SETTING_1,
				    RG_AFIFO_DUMMY_VALID_EN, 0x0);
				SENINF_BITS(baseA, CDPHY_RX_ANA_SETTING_1,
				    RG_CSI0_ASYNC_OPTION, 0xC);
				SENINF_BITS(baseA, CDPHY_RX_ANA_SETTING_1,
				    RG_AFIFO_DUMMY_VALID_PREPARE_NUM, 0x3);
				SENINF_BITS(baseA, CDPHY_RX_ANA_SETTING_1,
					RG_AFIFO_DUMMY_VALID_NUM, 0x5);
			}

			/*baseA works for both A & B*/

			SENINF_BITS(base, CDPHY_RX_ANA_3,
				RG_CSI0_EQ_DES_VREF_SEL, 0x2E);
			if (data_rate < 2500000000) {
				SENINF_BITS(base, CDPHY_RX_ANA_4,
					RG_CSI0_CPHY_T0_CDR_RSTB_CODE, 0x1);
				SENINF_BITS(base, CDPHY_RX_ANA_4,
					RG_CSI0_CPHY_T0_CDR_SEC_EDGE_CODE, 0x2);
				SENINF_BITS(base, CDPHY_RX_ANA_4,
					RG_CSI0_CPHY_T1_CDR_RSTB_CODE, 0x1);
				SENINF_BITS(base, CDPHY_RX_ANA_4,
					RG_CSI0_CPHY_T1_CDR_SEC_EDGE_CODE, 0x2);
						SENINF_BITS(base, CDPHY_RX_ANA_6,
				    RG_CSI0_CPHY_T0_CDR_CK_DELAY, 0xA);//TODO
				SENINF_BITS(base, CDPHY_RX_ANA_7,
				    RG_CSI0_CPHY_T1_CDR_CK_DELAY, 0xA);//TODO
			} else {
				SENINF_BITS(base, CDPHY_RX_ANA_4,
					RG_CSI0_CPHY_T0_CDR_RSTB_CODE, 0x0);
				SENINF_BITS(base, CDPHY_RX_ANA_4,
					RG_CSI0_CPHY_T0_CDR_SEC_EDGE_CODE, 0x4);
				SENINF_BITS(base, CDPHY_RX_ANA_4,
					RG_CSI0_CPHY_T1_CDR_RSTB_CODE, 0x0);
				SENINF_BITS(base, CDPHY_RX_ANA_4,
					RG_CSI0_CPHY_T1_CDR_SEC_EDGE_CODE, 0x4);
			}
		}
	}

	/* phyA power on */

	if (ctx->is_4d1c) {
		csirx_phyA_power_on(ctx, ctx->portA, 1);
		csirx_phyA_power_on(ctx, ctx->portB, 1);
	} else {
		csirx_phyA_power_on(ctx, ctx->port, 1);
	}

	return 0;
}
#endif

static int csirx_dphy_setting(struct seninf_ctx *ctx)
{
	void *base = ctx->reg_ana_dphy_top[ctx->port];

	if (ctx->is_4d1c) {
		SENINF_BITS(base, DPHY_RX_LANE_SELECT, RG_DPHY_RX_LD3_SEL, 4);
		SENINF_BITS(base, DPHY_RX_LANE_SELECT, RG_DPHY_RX_LD2_SEL, 0);
		SENINF_BITS(base, DPHY_RX_LANE_SELECT, RG_DPHY_RX_LD1_SEL, 3);
		SENINF_BITS(base, DPHY_RX_LANE_SELECT, RG_DPHY_RX_LD0_SEL, 1);
		SENINF_BITS(base, DPHY_RX_LANE_SELECT, RG_DPHY_RX_LC0_SEL, 2);

		SENINF_BITS(base, DPHY_RX_LANE_EN, DPHY_RX_LD0_EN, 1);
		SENINF_BITS(base, DPHY_RX_LANE_EN, DPHY_RX_LD1_EN, 1);
		SENINF_BITS(base, DPHY_RX_LANE_EN, DPHY_RX_LD2_EN, 1);
		SENINF_BITS(base, DPHY_RX_LANE_EN, DPHY_RX_LD3_EN, 1);
		SENINF_BITS(base, DPHY_RX_LANE_EN, DPHY_RX_LC0_EN, 1);
		SENINF_BITS(base, DPHY_RX_LANE_EN, DPHY_RX_LC1_EN, 0);
	} else {
		SENINF_BITS(base, DPHY_RX_LANE_SELECT, RG_DPHY_RX_LD3_SEL, 5);
		SENINF_BITS(base, DPHY_RX_LANE_SELECT, RG_DPHY_RX_LD2_SEL, 3);
		SENINF_BITS(base, DPHY_RX_LANE_SELECT, RG_DPHY_RX_LD1_SEL, 2);
		SENINF_BITS(base, DPHY_RX_LANE_SELECT, RG_DPHY_RX_LD0_SEL, 0);
		SENINF_BITS(base, DPHY_RX_LANE_SELECT, RG_DPHY_RX_LC1_SEL, 4);
		SENINF_BITS(base, DPHY_RX_LANE_SELECT, RG_DPHY_RX_LC0_SEL, 1);

		SENINF_BITS(base, DPHY_RX_LANE_EN, DPHY_RX_LD0_EN, 1);
		SENINF_BITS(base, DPHY_RX_LANE_EN, DPHY_RX_LD1_EN, 1);
		SENINF_BITS(base, DPHY_RX_LANE_EN, DPHY_RX_LD2_EN, 1);
		SENINF_BITS(base, DPHY_RX_LANE_EN, DPHY_RX_LD3_EN, 1);
		SENINF_BITS(base, DPHY_RX_LANE_EN, DPHY_RX_LC0_EN, 1);
		SENINF_BITS(base, DPHY_RX_LANE_EN, DPHY_RX_LC1_EN, 1);
	}

	SENINF_BITS(base, DPHY_RX_LANE_SELECT, DPHY_RX_CK_DATA_MUX_EN, 1);
	if (!ctx->csi_param.legacy_phy)
		SENINF_WRITE_REG(base, DPHY_RX_SPARE0, 0xf1);
	else
		SENINF_WRITE_REG(base, DPHY_RX_SPARE0, 0xf0);

	return 0;
}

static int csirx_cphy_setting(struct seninf_ctx *ctx)
{
	void *base = ctx->reg_ana_cphy_top[ctx->port];
	void *dphy_base = ctx->reg_ana_dphy_top[ctx->port];

	switch (ctx->port) {
	case CSI_PORT_0:
	case CSI_PORT_1:
	case CSI_PORT_2:
	case CSI_PORT_3:
	case CSI_PORT_4:
	case CSI_PORT_5:
	case CSI_PORT_0A:
	case CSI_PORT_1A:
	case CSI_PORT_2A:
	case CSI_PORT_3A:
	case CSI_PORT_4A:
	case CSI_PORT_5A:

		if (ctx->num_data_lanes == 3) {
			SENINF_BITS(base, CPHY_RX_CTRL, CPHY_RX_TR0_LPRX_EN, 1);
			SENINF_BITS(base, CPHY_RX_CTRL, CPHY_RX_TR1_LPRX_EN, 1);
			SENINF_BITS(base, CPHY_RX_CTRL, CPHY_RX_TR2_LPRX_EN, 1);
			SENINF_BITS(base, CPHY_RX_CTRL, CPHY_RX_TR3_LPRX_EN, 0);
		} else if (ctx->num_data_lanes == 2) {
			SENINF_BITS(base, CPHY_RX_CTRL, CPHY_RX_TR0_LPRX_EN, 1);
			SENINF_BITS(base, CPHY_RX_CTRL, CPHY_RX_TR1_LPRX_EN, 1);
		} else {
			SENINF_BITS(base, CPHY_RX_CTRL, CPHY_RX_TR0_LPRX_EN, 1);
		}
		break;
	case CSI_PORT_0B:
	case CSI_PORT_1B:
	case CSI_PORT_2B:
	case CSI_PORT_3B:
	case CSI_PORT_4B:
	case CSI_PORT_5B:

		if (ctx->num_data_lanes == 2) {
			SENINF_BITS(base, CPHY_RX_CTRL, CPHY_RX_TR2_LPRX_EN, 1);
			SENINF_BITS(base, CPHY_RX_CTRL, CPHY_RX_TR3_LPRX_EN, 1);
		} else
			SENINF_BITS(base, CPHY_RX_CTRL, CPHY_RX_TR2_LPRX_EN, 1);
		break;
	default:
		break;
	}
	if (!ctx->csi_param.legacy_phy)
		SENINF_WRITE_REG(dphy_base, DPHY_RX_SPARE0, 0xf1);
	else
		SENINF_WRITE_REG(dphy_base, DPHY_RX_SPARE0, 0xf0);

	return 0;
}

static int csirx_phy_setting(struct seninf_ctx *ctx)
{
	/* phyA */
	csirx_phyA_setting(ctx);

	if (!ctx->is_cphy)
		csirx_dphy_setting(ctx);
	else
		csirx_cphy_setting(ctx);

	return 0;
}

static int mtk_cam_seninf_set_csi_mipi(struct seninf_ctx *ctx)
{
	csirx_phy_init(ctx);

	/* seninf */
	csirx_seninf_setting(ctx);

	/* seninf csi2 */
	csirx_seninf_csi2_setting(ctx);

	/* seninf top */
	csirx_seninf_top_setting(ctx);

	/* phy */
	csirx_phy_setting(ctx);

	return 0;
}

static int mtk_cam_seninf_poweroff(struct seninf_ctx *ctx)
{
	void *pSeninf_csi2;

	pSeninf_csi2 = ctx->reg_if_csi2[ctx->seninfIdx];

	SENINF_WRITE_REG(pSeninf_csi2, SENINF_CSI2_EN, 0x0);

	if (ctx->is_4d1c) {
		csirx_phyA_power_on(ctx, ctx->portA, 0);
		csirx_phyA_power_on(ctx, ctx->portB, 0);
	} else {
		csirx_phyA_power_on(ctx, ctx->port, 0);
	}

	return 0;
}

static int mtk_cam_seninf_reset(struct seninf_ctx *ctx, int seninfIdx)
{
	int i;
	void *pSeninf_mux;
	void *pSeninf = ctx->reg_if_ctrl[seninfIdx];

	SENINF_BITS(pSeninf, SENINF_CSI2_CTRL, SENINF_CSI2_SW_RST, 1);
	udelay(1);
	SENINF_BITS(pSeninf, SENINF_CSI2_CTRL, SENINF_CSI2_SW_RST, 0);

	//dev_info(ctx->dev, "reset seninf %d\n", seninfIdx);

	for (i = SENINF_MUX1; i < _seninf_ops->mux_num; i++)
		if (mtk_cam_seninf_get_top_mux_ctrl(ctx, i) == seninfIdx &&
		    mtk_cam_seninf_is_mux_used(ctx, i)) {
			pSeninf_mux = ctx->reg_if_mux[i];
			SENINF_BITS(pSeninf_mux, SENINF_MUX_CTRL_0,
				    SENINF_MUX_SW_RST, 1);
			udelay(1);
			SENINF_BITS(pSeninf_mux, SENINF_MUX_CTRL_0,
				    SENINF_MUX_SW_RST, 0);
			dev_info(ctx->dev, "reset mux %d\n", i);
		}

	return 0;
}

static int mtk_cam_seninf_set_idle(struct seninf_ctx *ctx)
{
	int i;
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;

	for (i = 0; i < vcinfo->cnt; i++) {
		vc = &vcinfo->vc[i];
		if (vc->enable) {
			mtk_cam_seninf_disable_mux(ctx, vc->mux);
			mtk_cam_seninf_disable_cammux(ctx, vc->cam);
		}
	}
	for (i = 0; i < PAD_MAXCNT; i++)
		ctx->pad2cam[i] = 0xff;
#if LOG_MORE != 1
	dev_info(ctx->dev, "%s	rlease all mux & cam mux set all pd2cam to 0xff\n", __func__);
#endif

	return 0;
}

static int mtk_cam_seninf_get_mux_meter(struct seninf_ctx *ctx, int mux,
				 struct mtk_cam_seninf_mux_meter *meter)
{
	void *pSeninf_mux;
	s64 hv, hb, vv, vb, w, h, mipi_pixel_rate;
	s64 vb_in_us, hb_in_us, line_time_in_us;
	u32 res;

	pSeninf_mux = ctx->reg_if_mux[mux];

	SENINF_BITS(pSeninf_mux, SENINF_MUX_FRAME_SIZE_MON_CTRL,
		    RG_SENINF_MUX_FRAME_SIZE_MON_EN, 1);

	hv = SENINF_READ_REG(pSeninf_mux,
			     SENINF_MUX_FRAME_SIZE_MON_H_VALID);
	hb = SENINF_READ_REG(pSeninf_mux,
			     SENINF_MUX_FRAME_SIZE_MON_H_BLANK);
	vv = SENINF_READ_REG(pSeninf_mux,
			     SENINF_MUX_FRAME_SIZE_MON_V_VALID);
	vb = SENINF_READ_REG(pSeninf_mux,
			     SENINF_MUX_FRAME_SIZE_MON_V_BLANK);
	res = SENINF_READ_REG(pSeninf_mux,
			      SENINF_MUX_SIZE);

	w = res & 0xffff;
	h = res >> 16;

	if (ctx->fps_n && ctx->fps_d) {
		mipi_pixel_rate = w * ctx->fps_n * (vv + vb);
		do_div(mipi_pixel_rate, ctx->fps_d);
		do_div(mipi_pixel_rate, hv);

		vb_in_us = vb * ctx->fps_d * 1000000;
		do_div(vb_in_us, vv + vb);
		do_div(vb_in_us, ctx->fps_n);

		hb_in_us = hb * ctx->fps_d * 1000000;
		do_div(hb_in_us, vv + vb);
		do_div(hb_in_us, ctx->fps_n);

		line_time_in_us = (hv + hb) * ctx->fps_d * 1000000;
		do_div(line_time_in_us, vv + vb);
		do_div(line_time_in_us, ctx->fps_n);
	} else {
		mipi_pixel_rate = -1;
		vb_in_us = -1;
		hb_in_us = -1;
		line_time_in_us = -1;
	}

	meter->width = w;
	meter->height = h;

	meter->h_valid = hv;
	meter->h_blank = hb;
	meter->v_valid = vv;
	meter->v_blank = vb;

	meter->mipi_pixel_rate = mipi_pixel_rate;
	meter->vb_in_us = vb_in_us;
	meter->hb_in_us = hb_in_us;
	meter->line_time_in_us = line_time_in_us;

	return 0;
}

static ssize_t mtk_cam_seninf_show_status(struct device *dev,
				   struct device_attribute *attr,
		char *buf)
{
	int i, len;
	struct seninf_core *core;
	struct seninf_ctx *ctx;
	struct seninf_vc *vc;
	struct media_link *link;
	struct media_pad *pad;
	struct mtk_cam_seninf_mux_meter meter;
	void *csi2, *rx, *pmux, *pcammux, *base_ana;

	core = dev_get_drvdata(dev);
	len = 0;

	mutex_lock(&core->mutex);

	list_for_each_entry(ctx, &core->list, list) {
		SHOW(buf, len,
		     "\n[%s] port %d intf %d test %d cphy %d lanes %d\n",
			ctx->subdev.name,
			ctx->port,
			ctx->seninfIdx,
			ctx->is_test_model,
			ctx->is_cphy,
			ctx->num_data_lanes);

		pad = &ctx->pads[PAD_SINK];
		list_for_each_entry(link, &pad->entity->links, list) {
			if (link->sink == pad) {
				SHOW(buf, len, "source %s flags 0x%x\n",
				     link->source->entity->name,
					link->flags);
			}
		}

		if (!ctx->streaming)
			continue;

		csi2 = ctx->reg_if_csi2[ctx->seninfIdx];
		rx = ctx->reg_ana_dphy_top[ctx->port];
		base_ana = ctx->reg_ana_csi_rx[ctx->port];
		SHOW(buf, len, "csi2 irq_stat 0x%08x\n",
		     SENINF_READ_REG(csi2, SENINF_CSI2_IRQ_STATUS));
		SHOW(buf, len, "csi2 line_frame_num 0x%08x\n",
		     SENINF_READ_REG(csi2, SENINF_CSI2_LINE_FRAME_NUM));
		SHOW(buf, len, "csi2 packet_status 0x%08x\n",
		     SENINF_READ_REG(csi2, SENINF_CSI2_PACKET_STATUS));
		SHOW(buf, len, "csi2 packet_cnt_status 0x%08x\n",
		     SENINF_READ_REG(csi2, SENINF_CSI2_PACKET_CNT_STATUS));

		SHOW(buf, len, "rx-ana settle ck 0x%02x dt 0x%02x\n",
		     SENINF_READ_BITS(rx, DPHY_RX_CLOCK_LANE0_HS_PARAMETER,
				      RG_DPHY_RX_LC0_HS_SETTLE_PARAMETER),
		     SENINF_READ_BITS(rx, DPHY_RX_DATA_LANE0_HS_PARAMETER,
				      RG_CDPHY_RX_LD0_TRIO0_HS_SETTLE_PARAMETER));
		SHOW(buf, len, "rx-ana trail en %u param 0x%02x\n",
		     SENINF_READ_BITS(rx, DPHY_RX_DATA_LANE0_HS_PARAMETER,
				      RG_DPHY_RX_LD0_HS_TRAIL_EN),
		     SENINF_READ_BITS(rx, DPHY_RX_DATA_LANE0_HS_PARAMETER,
				      RG_DPHY_RX_LD0_HS_TRAIL_PARAMETER));
		SHOW(buf, len,
			"SENINF_CSI2_RESYNC_MERGE_CTRL 0x%08x CDPHY_RX_ANA_SETTING_1 0x%08x DPHY_RX_SPARE0 0x%08x\n",
			SENINF_READ_REG(csi2, SENINF_CSI2_RESYNC_MERGE_CTRL),
			SENINF_READ_REG(base_ana, CDPHY_RX_ANA_SETTING_1),
			SENINF_READ_REG(rx, DPHY_RX_SPARE0));

		SHOW(buf, len, "data_not_enough_cnt : <%d>",
			ctx->data_not_enough_cnt);
		SHOW(buf, len, "\terr_lane_resync_cnt : <%d>",
			ctx->err_lane_resync_cnt);
		SHOW(buf, len, "\tcrc_err_cnt : <%d>",
			ctx->crc_err_flag);
		SHOW(buf, len, "\tecc_err_double_cnt : <%d>",
			ctx->ecc_err_double_cnt);
		SHOW(buf, len, "\tecc_err_corrected_cnt : <%d>\n",
			ctx->ecc_err_corrected_cnt);

		if (SENINF_READ_REG(csi2, SENINF_CSI2_IRQ_STATUS) & ~(0x324)) {
			SENINF_WRITE_REG(csi2, SENINF_CSI2_IRQ_STATUS, 0xffffffff);
			SHOW(buf, len,
			     "after write clear csi irq 0x%x\n",
			     SENINF_READ_REG(csi2, SENINF_CSI2_IRQ_STATUS));
		}

		for (i = 0; i < ctx->vcinfo.cnt; i++) {
			vc = &ctx->vcinfo.vc[i];
			pmux = ctx->reg_if_mux[vc->mux];
			pcammux = ctx->reg_if_cam_mux_pcsr[vc->cam];
			SHOW(buf, len,
			     "[%d] vc 0x%x dt 0x%x mux %d cam %d\n",
				i, vc->vc, vc->dt, vc->mux, vc->cam);
			SHOW(buf, len,
			     "\tmux[%d] en %d src %d irq_stat 0x%x\n",
				vc->mux,
				mtk_cam_seninf_is_mux_used(ctx, vc->mux),
				mtk_cam_seninf_get_top_mux_ctrl(ctx, vc->mux),
				SENINF_READ_REG(pmux, SENINF_MUX_IRQ_STATUS));
			SHOW(buf, len, "\t\tfifo_overrun_cnt : <%d>\n",
				ctx->fifo_overrun_cnt);
			SHOW(buf, len,
			     "\tcam[%d] en %d src %d exp 0x%x res 0x%x err 0x%x irq_stat 0x%x\n",
				vc->cam,
				_seninf_ops->_is_cammux_used(ctx, vc->cam),
				_seninf_ops->_get_cammux_ctrl(ctx, vc->cam),
				mtk_cam_seninf_get_cammux_exp(ctx, vc->cam),
				mtk_cam_seninf_get_cammux_res(ctx, vc->cam),
				mtk_cam_seninf_get_cammux_err(ctx, vc->cam),
				SENINF_READ_REG(pcammux, SENINF_CAM_MUX_PCSR_IRQ_STATUS));
			SHOW(buf, len, "\t\tsize_err_cnt : <%d>\n",
				ctx->size_err_cnt);

			if (vc->feature == VC_RAW_DATA ||
				vc->feature == VC_STAGGER_NE ||
				vc->feature == VC_STAGGER_ME ||
				vc->feature == VC_STAGGER_SE) {
				mtk_cam_seninf_get_mux_meter(ctx,
							     vc->mux, &meter);
				SHOW(buf, len, "\t--- mux meter ---\n");
				SHOW(buf, len, "\twidth %d height %d\n",
				     meter.width, meter.height);
				SHOW(buf, len, "\th_valid %d, h_blank %d\n",
				     meter.h_valid, meter.h_blank);
				SHOW(buf, len, "\tv_valid %d, v_blank %d\n",
				     meter.v_valid, meter.v_blank);
				SHOW(buf, len, "\tmipi_pixel_rate %lld\n",
				     meter.mipi_pixel_rate);
				SHOW(buf, len, "\tv_blank %lld us\n",
				     meter.vb_in_us);
				SHOW(buf, len, "\th_blank %lld us\n",
				     meter.hb_in_us);
				SHOW(buf, len, "\tline_time %lld us\n",
				     meter.line_time_in_us);
			}
		}
	}

	mutex_unlock(&core->mutex);

	return len;
}

#ifdef SCAN_SETTLE
#define SCAN_TIME 3
#define SENINF_DRV_DEBUG_MAX_DELAY 1200
#else
#define SCAN_TIME 1
#define SENINF_DRV_DEBUG_MAX_DELAY 400
#endif


static int mtk_cam_seninf_debug(struct seninf_ctx *ctx)
{
	void *base_ana, *base_cphy, *base_dphy, *base_csi, *base_mux;
	unsigned int temp = 0;
	int pkg_cnt_changed = 0;
	unsigned int mipi_packet_cnt = 0;
	unsigned int tmp_mipi_packet_cnt = 0;
	unsigned long total_delay = 0;
	int irq_status_h = 0;
	int irq_status_v = 0;
	int enabled = 0;
	int ret = 0;
	int j, i;
	unsigned long debug_ft = 33 * SCAN_TIME ;//FIXME
	unsigned long debug_vb = 1 * SCAN_TIME;//FIXME

	for (j = CSI_PORT_0A; j <= CSI_PORT_5B; j++) {
		base_ana = ctx->reg_ana_csi_rx[j];
		dev_info(ctx->dev,
			"MipiRx_ANA%d: CDPHY_RX_ANA_SETTING_1(0x%08x) CDPHY_RX_ANA_0(0x%08x) ANA_1(0x%08x) ANA_2(0x%08x) ANA_3(0x%08x) ANA_4(0x%08x) ANA_5(0x%08x) ANA_6(0x%08x) ANA_7(0x%08x) ANA_8(0x%08x)\n",
			j - CSI_PORT_0A,
			SENINF_READ_REG(base_ana, CDPHY_RX_ANA_SETTING_1),
			SENINF_READ_REG(base_ana, CDPHY_RX_ANA_0),
			SENINF_READ_REG(base_ana, CDPHY_RX_ANA_1),
			SENINF_READ_REG(base_ana, CDPHY_RX_ANA_2),
			SENINF_READ_REG(base_ana, CDPHY_RX_ANA_3),
			SENINF_READ_REG(base_ana, CDPHY_RX_ANA_4),
			SENINF_READ_REG(base_ana, CDPHY_RX_ANA_5),
			SENINF_READ_REG(base_ana, CDPHY_RX_ANA_6),
			SENINF_READ_REG(base_ana, CDPHY_RX_ANA_7),
			SENINF_READ_REG(base_ana, CDPHY_RX_ANA_8));
		dev_info(ctx->dev,
			"MipiRx_ANA%d: CDPHY_RX_ANA_AD_0(0x%x) AD_HS_0(0x%x) AD_HS_1(0x%x)\n",
			j - CSI_PORT_0A,
			SENINF_READ_REG(base_ana, CDPHY_RX_ANA_AD_0),
			SENINF_READ_REG(base_ana, CDPHY_RX_ANA_AD_HS_0),
			SENINF_READ_REG(base_ana, CDPHY_RX_ANA_AD_HS_1));
	}

	for (j = CSI_PORT_0; j <= CSI_PORT_5; j++) {
		base_cphy = ctx->reg_ana_cphy_top[j];
		base_dphy = ctx->reg_ana_dphy_top[j];

		dev_info(ctx->dev,
			"Csi%d_Dphy_Top: LANE_EN(0x%x) LANE_SELECT(0x%x) CLK_LANE0_HS(0x%x) CLK_LANE1_HS(0x%x) DATA_LANE0_HS(0x%x) DATA_LANE1_HS(0x%x) DATA_LANE2_HS(0x%x) DATA_LANE3_HS(0x%x) DPHY_RX_SPARE0(0x%x)\n",
			j,
			SENINF_READ_REG(base_dphy, DPHY_RX_LANE_EN),
			SENINF_READ_REG(base_dphy, DPHY_RX_LANE_SELECT),
			SENINF_READ_REG(base_dphy,
					DPHY_RX_CLOCK_LANE0_HS_PARAMETER),
			SENINF_READ_REG(base_dphy,
					DPHY_RX_CLOCK_LANE1_HS_PARAMETER),
			SENINF_READ_REG(base_dphy,
					DPHY_RX_DATA_LANE0_HS_PARAMETER),
			SENINF_READ_REG(base_dphy,
					DPHY_RX_DATA_LANE1_HS_PARAMETER),
			SENINF_READ_REG(base_dphy,
					DPHY_RX_DATA_LANE2_HS_PARAMETER),
			SENINF_READ_REG(base_dphy,
					DPHY_RX_DATA_LANE3_HS_PARAMETER),
			SENINF_READ_REG(base_dphy,
					DPHY_RX_SPARE0));
			dev_info(ctx->dev,
			"Csi%d_Dphy_Top: DPHY_RX_DESKEW_CTRL(0x%08x) DPHY_RX_DESKEW_TIMING_CTRL(0x%08x) DPHY_RX_DESKEW_LANE0_CTRL(0x%08x) DPHY_RX_DESKEW_LANE1_CTRL(0x%08x) DPHY_RX_DESKEW_LANE2_CTRL(0x%08x) DPHY_RX_DESKEW_LANE3_CTRL(0x%08x)\n",
			j,
			SENINF_READ_REG(base_dphy, DPHY_RX_DESKEW_CTRL),
			SENINF_READ_REG(base_dphy, DPHY_RX_DESKEW_TIMING_CTRL),
				SENINF_READ_REG(base_dphy,
					DPHY_RX_DESKEW_LANE0_CTRL),
			SENINF_READ_REG(base_dphy,
					DPHY_RX_DESKEW_LANE1_CTRL),
			SENINF_READ_REG(base_dphy,
					DPHY_RX_DESKEW_LANE2_CTRL),
			SENINF_READ_REG(base_dphy,
					DPHY_RX_DESKEW_LANE3_CTRL));

		dev_info(ctx->dev,
			"Csi%d_Cphy_Top: CPHY_RX_CTRL(0x%x) CPHY_RX_DETECT_CTRL_POST(0x%x)\n",
			j,
			SENINF_READ_REG(base_cphy, CPHY_RX_CTRL),
			SENINF_READ_REG(base_cphy, CPHY_RX_DETECT_CTRL_POST));
	}


	dev_info(ctx->dev,
		"TOP_MUX_CTRL_0(0x%x) TOP_MUX_CTRL_1(0x%x) TOP_MUX_CTRL_2(0x%x) TOP_MUX_CTRL_3(0x%x) TOP_MUX_CTRL_4(0x%x) TOP_MUX_CTRL_5(0x%x)\n",
		SENINF_READ_REG(ctx->reg_if_top, SENINF_TOP_MUX_CTRL_0),
		SENINF_READ_REG(ctx->reg_if_top, SENINF_TOP_MUX_CTRL_1),
		SENINF_READ_REG(ctx->reg_if_top, SENINF_TOP_MUX_CTRL_2),
		SENINF_READ_REG(ctx->reg_if_top, SENINF_TOP_MUX_CTRL_3),
		SENINF_READ_REG(ctx->reg_if_top, SENINF_TOP_MUX_CTRL_4),
		SENINF_READ_REG(ctx->reg_if_top, SENINF_TOP_MUX_CTRL_5));


	enabled = SENINF_READ_REG(ctx->reg_if_cam_mux_gcsr, SENINF_CAM_MUX_GCSR_MUX_EN);
	/* clear cam mux irq */
	for (j = 0; j < ctx->vcinfo.cnt; j++) {
		if (ctx->vcinfo.vc[j].enable) {
			unsigned int used_cammux = ctx->vcinfo.vc[j].cam;

			for (i = 0; i < _seninf_ops->cam_mux_num; i++) {
				if ((used_cammux == i) && (enabled & (1<<i))) {
					u32 res, irq_st;

					res = SENINF_READ_REG(ctx->reg_if_cam_mux_pcsr[i],
							SENINF_CAM_MUX_PCSR_CHK_RES);
					irq_st = SENINF_READ_REG(ctx->reg_if_cam_mux_pcsr[i],
							SENINF_CAM_MUX_PCSR_IRQ_STATUS);
					SENINF_WRITE_REG(ctx->reg_if_cam_mux_pcsr[i],
							SENINF_CAM_MUX_PCSR_IRQ_STATUS, 0x103);
					dev_info(ctx->dev,
						"before clear cam mux%u recSize = 0x%x, irq = 0x%x|0x%x",
						i, res, irq_st,
						SENINF_READ_REG(ctx->reg_if_cam_mux_pcsr[i],
							SENINF_CAM_MUX_PCSR_IRQ_STATUS));
				}
			}
		}
	}

	/* Seninf_csi status IRQ */
	for (j = SENINF_1; j < _seninf_ops->seninf_num; j++) {
		base_csi = ctx->reg_if_csi2[j];
		temp = SENINF_READ_REG(base_csi, SENINF_CSI2_IRQ_STATUS);
		if (temp & ~(0x324)) {
			SENINF_WRITE_REG(base_csi, SENINF_CSI2_IRQ_STATUS,
					 0xffffffff);
		}
		dev_info(ctx->dev,
			"SENINF%d_CSI2_EN(0x%x) SENINF_CSI2_OPT(0x%x) SENINF_CSI2_IRQ_STATUS(0x%x), SENINF_CSI2_RESYNC_MERGE_CTRL(0x%x)\n",
			j,
			SENINF_READ_REG(base_csi, SENINF_CSI2_EN),
			SENINF_READ_REG(base_csi, SENINF_CSI2_OPT),
			temp,
			SENINF_READ_REG(base_csi, SENINF_CSI2_RESYNC_MERGE_CTRL));
	}

	/* Seninf_csi packet count */
	pkg_cnt_changed = 0;
	base_csi = ctx->reg_if_csi2[(uint32_t)ctx->seninfIdx];
	if (SENINF_READ_REG(base_csi, SENINF_CSI2_EN) & 0x1) {
		SENINF_BITS(base_csi, SENINF_CSI2_DBG_CTRL,
			    RG_CSI2_DBG_PACKET_CNT_EN, 1);
		mipi_packet_cnt = SENINF_READ_REG(base_csi,
					SENINF_CSI2_PACKET_CNT_STATUS);
		dev_info(ctx->dev,
			"total_delay %d SENINF%d_PkCnt(0x%x)\n",
			total_delay, ctx->seninfIdx, mipi_packet_cnt);

		while (total_delay <= debug_ft) {
			tmp_mipi_packet_cnt = mipi_packet_cnt & 0xFFFF;
			mdelay(debug_vb);
			total_delay += debug_vb;
			mipi_packet_cnt = SENINF_READ_REG(base_csi,
						SENINF_CSI2_PACKET_CNT_STATUS);
			dev_info(ctx->dev,
				"total_delay %d SENINF%d_PkCnt(0x%x)\n",
				total_delay, ctx->seninfIdx, mipi_packet_cnt);
			if (tmp_mipi_packet_cnt != (mipi_packet_cnt & 0xFFFF)) {
				pkg_cnt_changed = 1;
				break;
			}
		}
	}
	if (!pkg_cnt_changed)
		ret = -1;

	/* Check csi status again */
	if (debug_ft > total_delay) {
		mdelay(debug_ft - total_delay);
		total_delay = debug_ft;
	}
	temp = SENINF_READ_REG(base_csi, SENINF_CSI2_IRQ_STATUS);
	dev_info(ctx->dev,
		"SENINF%d_CSI2_IRQ_STATUS(0x%x)\n", ctx->seninfIdx, temp);
	if (total_delay < SENINF_DRV_DEBUG_MAX_DELAY) {
		if (total_delay + debug_ft < SENINF_DRV_DEBUG_MAX_DELAY)
			mdelay(debug_ft);
		else
			mdelay(SENINF_DRV_DEBUG_MAX_DELAY - total_delay);
	}
	temp = SENINF_READ_REG(base_csi, SENINF_CSI2_IRQ_STATUS);
	dev_info(ctx->dev,
		"SENINF%d_CSI2_IRQ_STATUS(0x%x)\n", ctx->seninfIdx, temp);
	if ((temp & 0xD0) != 0)
		ret = -2; //multi lanes sync error, crc error, ecc error

	/* SENINF_MUX */
	for (j = SENINF_MUX1; j < _seninf_ops->mux_num; j++) {
		base_mux = ctx->reg_if_mux[j];
		dev_info(ctx->dev,
			"SENINF%d_MUX_CTRL0(0x%x) SENINF%d_MUX_CTRL1(0x%x) SENINF_MUX_IRQ_STATUS(0x%x) SENINF%d_MUX_SIZE(0x%x) SENINF_MUX_ERR_SIZE(0x%x) SENINF_MUX_EXP_SIZE(0x%x)\n",
			j,
			SENINF_READ_REG(base_mux, SENINF_MUX_CTRL_0),
			j,
			SENINF_READ_REG(base_mux, SENINF_MUX_CTRL_1),
			SENINF_READ_REG(base_mux, SENINF_MUX_IRQ_STATUS),
			j,
			SENINF_READ_REG(base_mux, SENINF_MUX_SIZE),
			SENINF_READ_REG(base_mux, SENINF_MUX_ERR_SIZE),
			SENINF_READ_REG(base_mux, SENINF_MUX_IMG_SIZE));

		if ((SENINF_READ_REG(base_mux, SENINF_MUX_CTRL_0) & 0x1) &&
			(SENINF_READ_REG(base_mux, SENINF_MUX_IRQ_STATUS) & 0x1)) {
			SENINF_WRITE_REG(base_mux, SENINF_MUX_IRQ_STATUS,
					 0xffffffff);
			mdelay(debug_ft);
			dev_info(ctx->dev,
				"after reset overrun, SENINF_MUX_IRQ_STATUS(0x%x) SENINF%d_MUX_SIZE(0x%x)\n",
				SENINF_READ_REG(base_mux,
						SENINF_MUX_IRQ_STATUS),
				j,
				SENINF_READ_REG(base_mux, SENINF_MUX_SIZE));
		}
	}

	irq_status_h = SENINF_READ_REG(ctx->reg_if_cam_mux_gcsr,
				     SENINF_CAM_MUX_GCSR_HSIZE_ERR_IRQ_STS);
	irq_status_v = SENINF_READ_REG(ctx->reg_if_cam_mux_gcsr,
				     SENINF_CAM_MUX_GCSR_VSIZE_ERR_IRQ_STS);

	dev_info(ctx->dev,
		"SENINF_CAM_MUX_GCSR_MUX_EN 0x%x SENINF_CAM_MUX_GCSR_HSIZE_ERR_IRQ_STS 0x%x SENINF_CAM_MUX_GCSR_VSIZE_ERR_IRQ_STS 0x%x\n",
		enabled,
		irq_status_h,
		irq_status_v);
	dev_info(ctx->dev,
		"SENINF_CAM_MUX_GCSR_HSIZE_ERR_IRQ_EN 0x%x SENINF_CAM_MUX_GCSR_VSIZE_ERR_IRQ_EN 0x%x\n",
		SENINF_READ_REG(ctx->reg_if_cam_mux_gcsr, SENINF_CAM_MUX_GCSR_HSIZE_ERR_IRQ_EN),
		SENINF_READ_REG(ctx->reg_if_cam_mux_gcsr, SENINF_CAM_MUX_GCSR_VSIZE_ERR_IRQ_EN));



	/* check SENINF_CAM_MUX size */
	for (j = 0; j < ctx->vcinfo.cnt; j++) {
		if (ctx->vcinfo.vc[j].enable) {
			for (i = 0; i < _seninf_ops->cam_mux_num; i++) {
				unsigned int used_cammux = ctx->vcinfo.vc[j].cam;

				if ((used_cammux == i) && (enabled & (1<<i))) {
					int recSize = SENINF_READ_REG(ctx->reg_if_cam_mux_pcsr[i],
								SENINF_CAM_MUX_PCSR_CHK_RES);
					int expSize = SENINF_READ_REG(ctx->reg_if_cam_mux_pcsr[i],
								SENINF_CAM_MUX_PCSR_CHK_CTL);
					if (recSize != expSize) {
						dev_info(ctx->dev,
							"cam mux%u size mismatch!, recSize = 0x%x, expSize = 0x%x",
							i, recSize, expSize);
						//ret = -3;
					}
					if ((irq_status_h & (1 << i)) ||
						(irq_status_v & (1 << i))) {
						dev_info(ctx->dev,
							"cam mux%u size mismatch!, irq_status_h = 0x%x irq_status_v = 0x%x",
							i, irq_status_h, irq_status_v);
						//ret = -3;
					}
				}
			}
		}
	}

	for (i = 0; i < _seninf_ops->cam_mux_num; i++) {
		dev_info(ctx->dev,
		"cam_mux_%d CTRL(0x%x) RES(0x%x) EXP(0x%x) ERR(0x%x) OPT(0x%x) IRQ(0x%x)\n",
		i,
		SENINF_READ_REG(ctx->reg_if_cam_mux_pcsr[i], SENINF_CAM_MUX_PCSR_CTRL),
		SENINF_READ_REG(ctx->reg_if_cam_mux_pcsr[i], SENINF_CAM_MUX_PCSR_CHK_RES),
		SENINF_READ_REG(ctx->reg_if_cam_mux_pcsr[i], SENINF_CAM_MUX_PCSR_CHK_CTL),
		SENINF_READ_REG(ctx->reg_if_cam_mux_pcsr[i], SENINF_CAM_MUX_PCSR_CHK_ERR_RES),
		SENINF_READ_REG(ctx->reg_if_cam_mux_pcsr[i], SENINF_CAM_MUX_PCSR_OPT),
		SENINF_READ_REG(ctx->reg_if_cam_mux_pcsr[i], SENINF_CAM_MUX_PCSR_IRQ_STATUS));
	}

	dev_info(ctx->dev, "ret = %d", ret);

	return ret;
}

static ssize_t mtk_cam_seninf_show_err_status(struct device *dev,
				   struct device_attribute *attr,
		char *buf)
{
	int i, len;
	struct seninf_core *core;
	struct seninf_ctx *ctx;
	struct seninf_vc *vc;
	void *pmux;

	core = dev_get_drvdata(dev);
	len = 0;

	mutex_lock(&core->mutex);

	list_for_each_entry(ctx, &core->list, list) {
		SHOW(buf, len,
		     "\n[%s] port %d intf %d test %d cphy %d lanes %d\n",
			ctx->subdev.name,
			ctx->port,
			ctx->seninfIdx,
			ctx->is_test_model,
			ctx->is_cphy,
			ctx->num_data_lanes);

		SHOW(buf, len, "---flag = errs exceed theshhold ? 1 : 0---\n");
		SHOW(buf, len, "\tdata_not_enough error flag : <%d>",
			ctx->data_not_enough_flag);
		SHOW(buf, len, "\terr_lane_resync error flag : <%d>",
			ctx->err_lane_resync_flag);
		SHOW(buf, len, "\tcrc_err error flag : <%d>",
			ctx->crc_err_flag);
		SHOW(buf, len, "\tecc_err_double error flag : <%d>",
			ctx->ecc_err_double_flag);
		SHOW(buf, len, "\tecc_err_corrected error flag : <%d>\n",
			ctx->ecc_err_corrected_flag);

		for (i = 0; i < ctx->vcinfo.cnt; i++) {
			vc = &ctx->vcinfo.vc[i];
			pmux = ctx->reg_if_mux[vc->mux];
			SHOW(buf, len,
			     "[%d] vc 0x%x dt 0x%x mux %d cam %d\n",
				i, vc->vc, vc->dt, vc->mux, vc->cam);
			SHOW(buf, len, "\tfifo_overrun error flag : <%d>",
				ctx->fifo_overrun_flag);
			SHOW(buf, len, "\tsize_err error flag : <%d>\n",
				ctx->size_err_flag);
		}
	}

	mutex_unlock(&core->mutex);

	return len;
}

static int mtk_cam_seninf_irq_handler(int irq, void *data)
{
	struct seninf_core *core = (struct seninf_core *)data;
	struct seninf_ctx *ctx;
	unsigned int cam_vsyc_irq_tmp;
	void *pcammux_gcsr;
	unsigned long flags;
	/* for mipi err detection */
	struct seninf_ctx *ctx_;
	struct seninf_vc *vc;
	void *csi2, *pmux, *pSeninf_cam_mux_pcsr;
	int i;
	unsigned int csiIrq_tmp;
	unsigned int muxIrq_tmp;
	unsigned int camIrq_tmp;
	char seninf_name[256];
	/* for aee log */
	int cnt1, cnt2, cnt3, cnt4, cnt5, cnt6, cnt7;
	int cnt_tmp = 0;
	u64 time_boot = ktime_get_boottime_ns();
	u64 time_mono = ktime_get_ns();


	spin_lock_irqsave(&core->spinlock_irq, flags);

	if (core->vsync_irq_en_flag) {
		ctx = list_first_entry_or_null(&core->list,
						   struct seninf_ctx, list);

		if (ctx != NULL) {
			pcammux_gcsr = ctx->reg_if_cam_mux_gcsr;
			cam_vsyc_irq_tmp =
				SENINF_READ_REG(pcammux_gcsr, SENINF_CAM_MUX_GCSR_VSYNC_IRQ_STS);
			if (cam_vsyc_irq_tmp) {
				SENINF_WRITE_REG(
					pcammux_gcsr,
					SENINF_CAM_MUX_GCSR_VSYNC_IRQ_STS,
					0xffffffff);
				dev_info(ctx->dev, "%s SENINF_CAM_MUX_GCSR_VSYNC_IRQ_STS 0x%x cleared  0x%x tBoot %llu tMono %llu\n",
					__func__,
					cam_vsyc_irq_tmp,
					SENINF_READ_REG(
						pcammux_gcsr,
						SENINF_CAM_MUX_GCSR_VSYNC_IRQ_STS),
					time_boot/1000000, time_mono/1000000);
			}

		} else {
			dev_info(ctx->dev, "%s, ctx == NULL", __func__);
		}
	}

	if (core->csi_irq_en_flag) {
		list_for_each_entry(ctx_, &core->list, list) {
			csi2 = ctx_->reg_if_csi2[ctx_->seninfIdx];
			csiIrq_tmp =
				SENINF_READ_REG(csi2, SENINF_CSI2_IRQ_STATUS);

			if (csiIrq_tmp & ~(0x324)) {
				SENINF_WRITE_REG(
				csi2,
				SENINF_CSI2_IRQ_STATUS,
				0xffffffff);
				csiIrq_tmp =
					SENINF_READ_REG(csi2, SENINF_CSI2_IRQ_STATUS);
			}

			if (csiIrq_tmp & (0x1 << 3))
				ctx_->ecc_err_corrected_cnt++;
			if (csiIrq_tmp & (0x1 << 4))
				ctx_->ecc_err_double_cnt++;
			if (csiIrq_tmp & (0x1 << 6))
				ctx_->crc_err_cnt++;
			if (csiIrq_tmp & (0x1 << 13))
				ctx_->err_lane_resync_cnt++;
			if (csiIrq_tmp & (0x1 << 29))
				ctx_->data_not_enough_cnt++;

			for (i = 0; i < ctx_->vcinfo.cnt; i++) {
				vc = &ctx_->vcinfo.vc[i];
				pmux = ctx_->reg_if_mux[vc->mux];
				pSeninf_cam_mux_pcsr =
					ctx_->reg_if_cam_mux_pcsr[vc->cam];
				muxIrq_tmp = SENINF_READ_REG(
					     pmux,
					     SENINF_MUX_IRQ_STATUS);
				camIrq_tmp = SENINF_READ_REG(
					     pSeninf_cam_mux_pcsr,
					     SENINF_CAM_MUX_PCSR_CHK_ERR_RES);

				if (muxIrq_tmp) {
					SENINF_WRITE_REG(
					pmux,
					SENINF_MUX_IRQ_STATUS,
					0xffffffff);
					muxIrq_tmp = SENINF_READ_REG(
						     pmux,
						     SENINF_MUX_IRQ_STATUS);
				}
				if (camIrq_tmp) {
					SENINF_WRITE_REG(
					pSeninf_cam_mux_pcsr,
					SENINF_CAM_MUX_PCSR_CHK_ERR_RES,
					0xffffffff);
					camIrq_tmp =
					SENINF_READ_REG(
					pSeninf_cam_mux_pcsr,
					SENINF_CAM_MUX_PCSR_CHK_ERR_RES);
				}
				if (muxIrq_tmp & (0x1 << 0))
					ctx_->fifo_overrun_cnt++;
				if (camIrq_tmp)
					ctx_->size_err_cnt++;
			}

			if ((ctx_->data_not_enough_cnt) >= (core->detection_cnt) ||
			    (ctx_->err_lane_resync_cnt) >= (core->detection_cnt) ||
			    (ctx_->crc_err_cnt) >= (core->detection_cnt) ||
			    (ctx_->ecc_err_double_cnt) >= (core->detection_cnt) ||
			    (ctx_->ecc_err_corrected_cnt) >= (core->detection_cnt) ||
			    (ctx_->fifo_overrun_cnt) >= (core->detection_cnt) ||
			    (ctx_->size_err_cnt) >= (core->detection_cnt)) {
				SENINF_WRITE_REG(csi2, SENINF_CSI2_IRQ_EN, 0x80000000);

				if ((ctx_->data_not_enough_cnt) >= (core->detection_cnt))
					ctx_->data_not_enough_flag = 1;
				if ((ctx_->err_lane_resync_cnt) >= (core->detection_cnt))
					ctx_->err_lane_resync_flag = 1;
				if ((ctx_->crc_err_cnt) >= (core->detection_cnt))
					ctx_->crc_err_flag = 1;
				if ((ctx_->ecc_err_double_cnt) >= (core->detection_cnt))
					ctx_->ecc_err_double_flag = 1;
				if ((ctx_->ecc_err_corrected_cnt) >= (core->detection_cnt))
					ctx_->ecc_err_corrected_flag = 1;
				if ((ctx_->fifo_overrun_cnt) >= (core->detection_cnt))
					ctx_->fifo_overrun_flag = 1;
				if ((ctx_->size_err_cnt) >= (core->detection_cnt))
					ctx_->size_err_flag = 1;

				cnt1 = snprintf(seninf_name, 256, "%s", __func__);
				cnt_tmp += cnt1;
				cnt2 = snprintf(seninf_name+cnt_tmp, 256-cnt_tmp,
					"   data_not_enough_count: %d",
					ctx_->data_not_enough_cnt);
				cnt_tmp += cnt2;
				cnt3 = snprintf(seninf_name+cnt_tmp, 256-cnt_tmp,
					"   err_lane_resync_count: %d",
					ctx_->err_lane_resync_cnt);
				cnt_tmp += cnt3;
				cnt4 = snprintf(seninf_name+cnt_tmp, 256-cnt_tmp,
					"   crc_err_count: %d",
					ctx_->crc_err_cnt);
				cnt_tmp += cnt4;
				cnt5 = snprintf(seninf_name+cnt_tmp, 256-cnt_tmp,
					"   ecc_err_double_count: %d",
					ctx_->ecc_err_double_cnt);
				cnt_tmp += cnt5;
				cnt6 = snprintf(seninf_name+cnt_tmp, 256-cnt_tmp,
					"   ecc_err_corrected_count: %d",
					ctx_->ecc_err_corrected_cnt);
				cnt_tmp += cnt6;
				cnt7 = snprintf(seninf_name+cnt_tmp, 256-cnt_tmp,
					"   fifo_overrun_count: %d",
					ctx_->fifo_overrun_cnt);
				cnt_tmp += cnt7;
				snprintf(seninf_name+cnt_tmp, 256-cnt_tmp,
					"   size_err_count: %d",
					ctx_->size_err_cnt);
				seninf_aee_print("[AEE] %s", seninf_name);

				// kill_pid(core->pid, SIGKILL, 1);
			}
		}
	}

	spin_unlock_irqrestore(&core->spinlock_irq, flags);

	return 0;
}

static int mtk_cam_seninf_set_sw_cfg_busy(struct seninf_ctx *ctx, bool enable, int index)
{
	void *pSeninf_cam_mux_gcsr = ctx->reg_if_cam_mux_gcsr;

	if (index == 0)
		SENINF_BITS(pSeninf_cam_mux_gcsr, SENINF_CAM_MUX_GCSR_DYN_CTRL,
			RG_SENINF_CAM_MUX_GCSR_DYN_SWITCH_BSY0, enable);
	else
		SENINF_BITS(pSeninf_cam_mux_gcsr, SENINF_CAM_MUX_GCSR_DYN_CTRL,
			 RG_SENINF_CAM_MUX_GCSR_DYN_SWITCH_BSY0, enable);
	return 0;
}

static int mtk_cam_seninf_set_cam_mux_dyn_en(
	struct seninf_ctx *ctx, bool enable, int cam_mux, int index)
{
	void *pSeninf_cam_mux_gcsr = ctx->reg_if_cam_mux_gcsr;
	int tmp = 0;

	if (index == 0) {
		tmp = SENINF_READ_BITS(pSeninf_cam_mux_gcsr,
			SENINF_CAM_MUX_GCSR_DYN_EN0, RG_SENINF_CAM_MUX_GCSR_DYN_SWITCH_EN0);
		if (enable)
			tmp = tmp | (1 << cam_mux);
		else
			tmp = tmp & ~(1 << cam_mux);
		SENINF_BITS(pSeninf_cam_mux_gcsr,
			SENINF_CAM_MUX_GCSR_DYN_EN0, RG_SENINF_CAM_MUX_GCSR_DYN_SWITCH_EN0, tmp);
	} else {
		tmp = SENINF_READ_BITS(pSeninf_cam_mux_gcsr,
			SENINF_CAM_MUX_GCSR_DYN_EN1, RG_SENINF_CAM_MUX_GCSR_DYN_SWITCH_EN1);
		if (enable)
			tmp = tmp | (1 << cam_mux);
		else
			tmp = tmp & ~(1 << cam_mux);
		SENINF_BITS(pSeninf_cam_mux_gcsr,
			SENINF_CAM_MUX_GCSR_DYN_EN1, RG_SENINF_CAM_MUX_GCSR_DYN_SWITCH_EN1, tmp);
	}
	return 0;

}

static int mtk_cam_seninf_reset_cam_mux_dyn_en(struct seninf_ctx *ctx, int index)
{
	void *pSeninf_cam_mux_gcsr = ctx->reg_if_cam_mux_gcsr;

	if (index == 0)
		SENINF_BITS(pSeninf_cam_mux_gcsr,
			SENINF_CAM_MUX_GCSR_DYN_EN0, RG_SENINF_CAM_MUX_GCSR_DYN_SWITCH_EN0, 0);
	else
		SENINF_BITS(pSeninf_cam_mux_gcsr,
			SENINF_CAM_MUX_GCSR_DYN_EN1, RG_SENINF_CAM_MUX_GCSR_DYN_SWITCH_EN1, 0);
	return 0;
}


static int mtk_cam_seninf_enable_global_drop_irq(struct seninf_ctx *ctx, bool enable, int index)
{
	void *pSeninf_cam_mux_gcsr = ctx->reg_if_cam_mux_gcsr;
	int tmp = 0;

	tmp = SENINF_READ_BITS(pSeninf_cam_mux_gcsr,
			SENINF_CAM_MUX_GCSR_IRQ_EN, RG_SENINF_CAM_MUX_GCSR_SKIP_NEXT_FRAME_IRQ_EN);
	if (enable)
		tmp |= 1 << index;
	else
		tmp &= ~(1 << index);

	SENINF_BITS(pSeninf_cam_mux_gcsr,
		SENINF_CAM_MUX_GCSR_IRQ_EN, RG_SENINF_CAM_MUX_GCSR_SKIP_NEXT_FRAME_IRQ_EN, tmp);

	return 0;

}

static int mtk_cam_seninf_enable_cam_mux_vsync_irq(struct seninf_ctx *ctx, bool enable, int cam_mux)
{
	void *pSeninf_cam_mux_gcsr = ctx->reg_if_cam_mux_gcsr;
	int tmp = 0;

	tmp = SENINF_READ_BITS(pSeninf_cam_mux_gcsr,
			SENINF_CAM_MUX_GCSR_VSYNC_IRQ_EN, RG_SENINF_CAM_MUX_GCSR_VSYNC_IRQ_EN);
	if (enable)
		tmp |= (1 << cam_mux);
	else
		tmp &= ~(1 << cam_mux);
	SENINF_BITS(pSeninf_cam_mux_gcsr,
			SENINF_CAM_MUX_GCSR_VSYNC_IRQ_EN, RG_SENINF_CAM_MUX_GCSR_VSYNC_IRQ_EN, tmp);
	return 0;
}


static int mtk_cam_seninf_disable_all_cam_mux_vsync_irq(struct seninf_ctx *ctx)
{
	void *pSeninf_cam_mux_gcsr = ctx->reg_if_cam_mux_gcsr;

	SENINF_BITS(pSeninf_cam_mux_gcsr,
		SENINF_CAM_MUX_GCSR_VSYNC_IRQ_EN, RG_SENINF_CAM_MUX_GCSR_VSYNC_IRQ_EN, 0);
	return 0;

}
#ifdef SCAN_SETTLE
static int mtk_cam_scan_settle(struct seninf_ctx *ctx)
{
	u16 settle = 0, trail = 0;
	int ret = 0, ret_old = -1;

	if (!ctx->is_cphy) {
		for (trail = 0; trail <= 0xff; trail++) {
			set_trail(ctx, trail);
			msleep(30);
			ret = mtk_cam_seninf_debug(ctx);
			if (ret == 0) {
				if (ret != ret_old)
					dev_info(ctx->dev,
							"%s valid trail = 0x%x ret = %d ret_old = %d trail enbled\n",
							__func__, trail, ret, ret_old);

				dev_info(ctx->dev,
					"%s valid trail = 0x%x ret_detail = %dtrail enbled\n",
					__func__, trail, ret);

			} else {
				if (ret != ret_old)
					dev_info(ctx->dev,
						"%s invalid trail = 0x%x ret = %d ret_old = %d trail enbled\n",
						__func__, trail, ret, ret_old);
				dev_info(ctx->dev,
					"%s invalid trail = 0x%x ret_detail = %d trail enbled\n",
					__func__, trail, ret);
			}
			ret_old = ret;
		}
	} else {
		ret = 0;
		ret_old = -1;
		for (settle = 0; settle <= 0xff; settle++) {
			set_settle(ctx, settle, false);
			msleep(30);
			ret = mtk_cam_seninf_debug(ctx);
			if (ret == 0) {
				if (ret != ret_old)
					dev_info(ctx->dev,
							"%s valid settle = 0x%x ret = %d ret_old = %d\n",
							__func__, settle, ret, ret_old);
					dev_info(ctx->dev,
						"%s valid settle = 0x%x ret_detail = %d\n",
						__func__, settle, ret);
			} else {
				if (ret != ret_old)
					dev_info(ctx->dev,
							"%s invalid settle = 0x%x ret = %d ret_old = %d\n",
							__func__, settle, ret, ret_old);
				dev_info(ctx->dev,
						"%s invalid settle = 0x%x ret_detail = %d\n",
						__func__, settle, ret);
			}
			ret_old = ret;
		}
	}

	return 0;
}
#endif

static int mtk_cam_seninf_set_reg(struct seninf_ctx *ctx, u32 key, u32 val)
{
	int i;
	void *base = ctx->reg_ana_dphy_top[ctx->port];
	void *csi2 = ctx->reg_if_csi2[ctx->seninfIdx];
	void *pmux, *pcammux, *p_gcammux;
	struct seninf_vc *vc;
	struct seninf_core *core;
	struct seninf_ctx *ctx_;
	void *csi2_;

	core = dev_get_drvdata(ctx->dev->parent);

	if (!ctx->streaming)
		return 0;

	dev_info(ctx->dev, "%s key = 0x%x, val = 0x%x\n",
		 __func__, key, val);

	switch (key) {
	case REG_KEY_SETTLE_CK:
		SENINF_BITS(base, DPHY_RX_CLOCK_LANE0_HS_PARAMETER,
			    RG_DPHY_RX_LC0_HS_SETTLE_PARAMETER,
			    val);
		SENINF_BITS(base, DPHY_RX_CLOCK_LANE1_HS_PARAMETER,
			    RG_DPHY_RX_LC1_HS_SETTLE_PARAMETER,
			    val);
		break;
	case REG_KEY_SETTLE_DT:
		SENINF_BITS(base, DPHY_RX_DATA_LANE0_HS_PARAMETER,
			    RG_CDPHY_RX_LD0_TRIO0_HS_SETTLE_PARAMETER,
			    val);
		SENINF_BITS(base, DPHY_RX_DATA_LANE1_HS_PARAMETER,
			    RG_CDPHY_RX_LD1_TRIO1_HS_SETTLE_PARAMETER,
			    val);
		SENINF_BITS(base, DPHY_RX_DATA_LANE2_HS_PARAMETER,
			    RG_CDPHY_RX_LD2_TRIO2_HS_SETTLE_PARAMETER,
			    val);
		SENINF_BITS(base, DPHY_RX_DATA_LANE3_HS_PARAMETER,
			    RG_CDPHY_RX_LD3_TRIO3_HS_SETTLE_PARAMETER,
			    val);
		break;
	case REG_KEY_HS_TRAIL_EN:
		SENINF_BITS(base, DPHY_RX_DATA_LANE0_HS_PARAMETER,
			    RG_DPHY_RX_LD0_HS_TRAIL_EN, val);
		SENINF_BITS(base, DPHY_RX_DATA_LANE1_HS_PARAMETER,
			    RG_DPHY_RX_LD1_HS_TRAIL_EN, val);
		SENINF_BITS(base, DPHY_RX_DATA_LANE2_HS_PARAMETER,
			    RG_DPHY_RX_LD2_HS_TRAIL_EN, val);
		SENINF_BITS(base, DPHY_RX_DATA_LANE3_HS_PARAMETER,
			    RG_DPHY_RX_LD3_HS_TRAIL_EN, val);
		break;
	case REG_KEY_HS_TRAIL_PARAM:
		SENINF_BITS(base, DPHY_RX_DATA_LANE0_HS_PARAMETER,
			    RG_DPHY_RX_LD0_HS_TRAIL_PARAMETER, val);
		SENINF_BITS(base, DPHY_RX_DATA_LANE1_HS_PARAMETER,
			    RG_DPHY_RX_LD1_HS_TRAIL_PARAMETER, val);
		SENINF_BITS(base, DPHY_RX_DATA_LANE2_HS_PARAMETER,
			    RG_DPHY_RX_LD2_HS_TRAIL_PARAMETER, val);
		SENINF_BITS(base, DPHY_RX_DATA_LANE3_HS_PARAMETER,
			    RG_DPHY_RX_LD3_HS_TRAIL_PARAMETER, val);
		break;
	case REG_KEY_CSI_IRQ_STAT:
		SENINF_WRITE_REG(csi2, SENINF_CSI2_IRQ_STATUS,
				 val & 0xFFFFFFFF);
		break;
	case REG_KEY_CSI_RESYNC_CYCLE:
		SENINF_BITS(csi2, SENINF_CSI2_RESYNC_MERGE_CTRL,
			   RG_CSI2_RESYNC_DMY_CYCLE, val);
		break;
	case REG_KEY_MUX_IRQ_STAT:
		for (i = 0; i < ctx->vcinfo.cnt; i++) {
			vc = &ctx->vcinfo.vc[i];
			pmux = ctx->reg_if_mux[vc->mux];
			SENINF_WRITE_REG(pmux, SENINF_MUX_IRQ_STATUS,
					val & 0xFFFFFFFF);
		}
		break;
	case REG_KEY_CAMMUX_IRQ_STAT:
		for (i = 0; i < ctx->vcinfo.cnt; i++) {
			vc = &ctx->vcinfo.vc[i];
			pcammux = ctx->reg_if_cam_mux_pcsr[vc->cam];
			SENINF_WRITE_REG(pcammux,
					SENINF_CAM_MUX_PCSR_IRQ_STATUS,
					val & 0xFFFFFFFF);
		}
		break;
	case REG_KEY_CAMMUX_VSYNC_IRQ_EN:
			p_gcammux = ctx->reg_if_cam_mux_gcsr;
			core->vsync_irq_en_flag = 1;
			SENINF_WRITE_REG(p_gcammux,
					SENINF_CAM_MUX_GCSR_VSYNC_IRQ_EN,
					val & 0xFFFFFFFF);
		break;
	case REG_KEY_CSI_IRQ_EN:
		{
			if (val < 0)
				return 0;
			if (!val)
				core->detection_cnt = 50;

			core->csi_irq_en_flag = 1;
			core->detection_cnt = val;
			list_for_each_entry(ctx_, &core->list, list) {
				csi2_ = ctx_->reg_if_csi2[ctx_->seninfIdx];
				SENINF_WRITE_REG(csi2_, SENINF_CSI2_IRQ_EN,
						0xA0002058);
			}
		}
	}

	return 0;
}

struct mtk_cam_seninf_ops mtk_csi_phy_3_0 = {
	._init_iomem = mtk_cam_seninf_init_iomem,
	._init_port = mtk_cam_seninf_init_port,
	._is_cammux_used = mtk_cam_seninf_is_cammux_used,
	._cammux = mtk_cam_seninf_cammux,
	._cammux = mtk_cam_seninf_cammux,
	._disable_cammux = mtk_cam_seninf_disable_cammux,
	._disable_all_cammux = mtk_cam_seninf_disable_all_cammux,
	._set_top_mux_ctrl = mtk_cam_seninf_set_top_mux_ctrl,
	._get_top_mux_ctrl = mtk_cam_seninf_get_top_mux_ctrl,
	._get_cammux_ctrl = mtk_cam_seninf_get_cammux_ctrl,
	._get_cammux_res = mtk_cam_seninf_get_cammux_res,
	._set_cammux_vc = mtk_cam_seninf_set_cammux_vc,
	._set_cammux_src = mtk_cam_seninf_set_cammux_src,
	._set_vc = mtk_cam_seninf_set_vc,
	._set_mux_ctrl = mtk_cam_seninf_set_mux_ctrl,
	._set_mux_crop = mtk_cam_seninf_set_mux_crop,
	._is_mux_used = mtk_cam_seninf_is_mux_used,
	._mux = mtk_cam_seninf_mux,
	._disable_mux = mtk_cam_seninf_disable_mux,
	._disable_all_mux = mtk_cam_seninf_disable_all_mux,
	._set_cammux_chk_pixel_mode = mtk_cam_seninf_set_cammux_chk_pixel_mode,
	._set_test_model = mtk_cam_seninf_set_test_model,
	._set_csi_mipi = mtk_cam_seninf_set_csi_mipi,
	._poweroff = mtk_cam_seninf_poweroff,
	._reset = mtk_cam_seninf_reset,
	._set_idle = mtk_cam_seninf_set_idle,
	._get_mux_meter = mtk_cam_seninf_get_mux_meter,
	._show_status = mtk_cam_seninf_show_status,
	._switch_to_cammux_inner_page = mtk_cam_seninf_switch_to_cammux_inner_page,
	._set_cammux_next_ctrl = mtk_cam_seninf_set_cammux_next_ctrl,
	._update_mux_pixel_mode = mtk_cam_seninf_update_mux_pixel_mode,
	._irq_handler = mtk_cam_seninf_irq_handler,
	._set_sw_cfg_busy = mtk_cam_seninf_set_sw_cfg_busy,
	._set_cam_mux_dyn_en = mtk_cam_seninf_set_cam_mux_dyn_en,
	._reset_cam_mux_dyn_en = mtk_cam_seninf_reset_cam_mux_dyn_en,
	._enable_global_drop_irq = mtk_cam_seninf_enable_global_drop_irq,
	._enable_cam_mux_vsync_irq = mtk_cam_seninf_enable_cam_mux_vsync_irq,
	._disable_all_cam_mux_vsync_irq = mtk_cam_seninf_disable_all_cam_mux_vsync_irq,
#ifdef SCAN_SETTLE
	._debug = mtk_cam_scan_settle,

#else
	._debug = mtk_cam_seninf_debug,
#endif
	._set_reg = mtk_cam_seninf_set_reg,
	.seninf_num = 12,
	.mux_num = 22,
	.cam_mux_num = 23,
	.pref_mux_num = 17,
	._show_err_status = mtk_cam_seninf_show_err_status,
};
