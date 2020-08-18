// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <linux/module.h>
#include <linux/delay.h>

#include "mtk_cam-seninf.h"
#include "mtk_cam-seninf-hw.h"
#include "mtk_cam-seninf-regs.h"

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

int mtk_cam_seninf_init_iomem(struct seninf_ctx *ctx,
			      void __iomem *if_base, void __iomem *ana_base)
{
	int i;

	ctx->reg_ana_csi_rx[CSI_PORT_0] =
	ctx->reg_ana_csi_rx[CSI_PORT_0A] = ana_base + 0;
	ctx->reg_ana_csi_rx[CSI_PORT_0B] = ana_base + 0x1000;

	ctx->reg_ana_csi_rx[CSI_PORT_1] =
	ctx->reg_ana_csi_rx[CSI_PORT_1A] = ana_base + 0x4000;
	ctx->reg_ana_csi_rx[CSI_PORT_1B] = ana_base + 0x5000;

	ctx->reg_ana_csi_rx[CSI_PORT_2] =
	ctx->reg_ana_csi_rx[CSI_PORT_2A] = ana_base + 0x8000;
	ctx->reg_ana_csi_rx[CSI_PORT_2B] = ana_base + 0x9000;

	ctx->reg_ana_csi_rx[CSI_PORT_3] =
	ctx->reg_ana_csi_rx[CSI_PORT_3A] = ana_base + 0xc000;
	ctx->reg_ana_csi_rx[CSI_PORT_3B] = ana_base + 0xd000;

	ctx->reg_ana_dphy_top[CSI_PORT_0A] =
	ctx->reg_ana_dphy_top[CSI_PORT_0B] =
	ctx->reg_ana_dphy_top[CSI_PORT_0] = ana_base + 0x2000;

	ctx->reg_ana_dphy_top[CSI_PORT_1A] =
	ctx->reg_ana_dphy_top[CSI_PORT_1B] =
	ctx->reg_ana_dphy_top[CSI_PORT_1] = ana_base + 0x6000;

	ctx->reg_ana_dphy_top[CSI_PORT_2A] =
	ctx->reg_ana_dphy_top[CSI_PORT_2B] =
	ctx->reg_ana_dphy_top[CSI_PORT_2] = ana_base + 0xa000;

	ctx->reg_ana_dphy_top[CSI_PORT_3A] =
	ctx->reg_ana_dphy_top[CSI_PORT_3B] =
	ctx->reg_ana_dphy_top[CSI_PORT_3] = ana_base + 0xe000;

	ctx->reg_ana_cphy_top[CSI_PORT_0A] =
	ctx->reg_ana_cphy_top[CSI_PORT_0B] =
	ctx->reg_ana_cphy_top[CSI_PORT_0] = ana_base + 0x3000;

	ctx->reg_ana_cphy_top[CSI_PORT_1A] =
	ctx->reg_ana_cphy_top[CSI_PORT_1B] =
	ctx->reg_ana_cphy_top[CSI_PORT_1] = ana_base + 0x7000;

	ctx->reg_ana_cphy_top[CSI_PORT_2A] =
	ctx->reg_ana_cphy_top[CSI_PORT_2B] =
	ctx->reg_ana_cphy_top[CSI_PORT_2] = ana_base + 0xb000;

	ctx->reg_ana_cphy_top[CSI_PORT_3A] =
	ctx->reg_ana_cphy_top[CSI_PORT_3B] =
	ctx->reg_ana_cphy_top[CSI_PORT_3] = ana_base + 0xf000;

	ctx->reg_if_top = if_base;

	for (i = SENINF_1; i < SENINF_NUM; i++) {
		ctx->reg_if_ctrl[i] = if_base + 0x0200 + (0x1000 * i);
		ctx->reg_if_tg[i] = if_base + 0x0600 + (0x1000 * i);
		ctx->reg_if_csi2[i] = if_base + 0x0a00 + (0x1000 * i);
	}

	for (i = SENINF_MUX1; i < SENINF_MUX_NUM; i++)
		ctx->reg_if_mux[i] = if_base + 0x0d00 + (0x1000 * i);

	ctx->reg_if_cam_mux = if_base + 0x0400;

	return 0;
}

int mtk_cam_seninf_init_port(struct seninf_ctx *ctx, int port)
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
		ctx->seninfIdx = SENINF_1;
		break;
	case CSI_PORT_0A:
		ctx->seninfIdx = SENINF_1;
		break;
	case CSI_PORT_0B:
		ctx->seninfIdx = SENINF_2;
		break;
	case CSI_PORT_1:
		ctx->seninfIdx = SENINF_3;
		break;
	case CSI_PORT_1A:
		ctx->seninfIdx = SENINF_3;
		break;
	case CSI_PORT_1B:
		ctx->seninfIdx = SENINF_4;
		break;
	case CSI_PORT_2:
		ctx->seninfIdx = SENINF_5;
		break;
	case CSI_PORT_2A:
		ctx->seninfIdx = SENINF_5;
		break;
	case CSI_PORT_2B:
		ctx->seninfIdx = SENINF_6;
		break;
	case CSI_PORT_3:
		ctx->seninfIdx = SENINF_7;
		break;
	case CSI_PORT_3A:
		ctx->seninfIdx = SENINF_7;
		break;
	case CSI_PORT_3B:
		ctx->seninfIdx = SENINF_8;
		break;
	default:
		dev_err(ctx->dev, "invalid port %d\n", port);
		return -EINVAL;
	}

	return 0;
}

int mtk_cam_seninf_is_cammux_used(struct seninf_ctx *ctx, int cam_mux)
{
	void *pSeninf_cam_mux = ctx->reg_if_cam_mux;
	unsigned int temp = SENINF_READ_REG(pSeninf_cam_mux,
			SENINF_CAM_MUX_EN);

	return !!(temp & (1 << cam_mux));
}

int mtk_cam_seninf_cammux(struct seninf_ctx *ctx, int cam_mux)
{
	void *pSeninf_cam_mux = ctx->reg_if_cam_mux;
	u32 temp, irq_en;

	temp = SENINF_READ_REG(pSeninf_cam_mux, SENINF_CAM_MUX_EN);
	SENINF_WRITE_REG(pSeninf_cam_mux, SENINF_CAM_MUX_EN, temp |
			 (1 << cam_mux));

	irq_en = SENINF_READ_REG(pSeninf_cam_mux, SENINF_CAM_MUX_IRQ_EN);
	SENINF_WRITE_REG(pSeninf_cam_mux, SENINF_CAM_MUX_IRQ_EN,
			 irq_en | (3 << (cam_mux * 2)));
	SENINF_WRITE_REG(pSeninf_cam_mux, SENINF_CAM_MUX_IRQ_STATUS,
			 3 << (cam_mux * 2));//clr irq

	dev_info(ctx->dev, "cam_mux %d EN 0x%x IRQ_EN 0x%x IRQ_STATUS 0x%x\n",
		 cam_mux,
		SENINF_READ_REG(pSeninf_cam_mux, SENINF_CAM_MUX_EN),
		SENINF_READ_REG(pSeninf_cam_mux, SENINF_CAM_MUX_IRQ_EN),
		SENINF_READ_REG(pSeninf_cam_mux, SENINF_CAM_MUX_IRQ_STATUS));

	return 0;
}

int mtk_cam_seninf_disable_cammux(struct seninf_ctx *ctx, int cam_mux)
{
	void *base = ctx->reg_if_cam_mux;
	u32 temp, irq_en;

	temp = SENINF_READ_REG(base, SENINF_CAM_MUX_EN);
	irq_en = SENINF_READ_REG(base, SENINF_CAM_MUX_IRQ_EN);

	if ((1 << cam_mux) & temp) {
		SENINF_WRITE_REG(base, SENINF_CAM_MUX_IRQ_EN,
				 irq_en & (~(3 << (cam_mux * 2))));
		SENINF_WRITE_REG(base, SENINF_CAM_MUX_EN,
				 temp & (~(1 << cam_mux)));
		dev_info(ctx->dev, "cammux %d EN %x IRQ_EN %x IRQ_STATUS %x",
			 cam_mux,
			SENINF_READ_REG(base, SENINF_CAM_MUX_EN),
			SENINF_READ_REG(base, SENINF_CAM_MUX_IRQ_EN),
			SENINF_READ_REG(base, SENINF_CAM_MUX_IRQ_STATUS));
	}

	return 0;
}

int mtk_cam_seninf_disable_all_cammux(struct seninf_ctx *ctx)
{
	void *pSeninf_cam_mux = ctx->reg_if_cam_mux;

	SENINF_WRITE_REG(pSeninf_cam_mux, SENINF_CAM_MUX_EN, 0);

	return 0;
}

int mtk_cam_seninf_set_top_mux_ctrl(struct seninf_ctx *ctx,
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
	default:
		dev_err(ctx->dev, "invalid mux_idx %d\n", mux_idx);
		return -EINVAL;
	}

	dev_info(ctx->dev, "TOP_MUX_CTRL_0(0x%x) TOP_MUX_CTRL_1(0x%x)\n",
		 SENINF_READ_REG(pSeninf, SENINF_TOP_MUX_CTRL_0),
		SENINF_READ_REG(pSeninf, SENINF_TOP_MUX_CTRL_1));

	return 0;
}

int mtk_cam_seninf_get_top_mux_ctrl(struct seninf_ctx *ctx, int mux_idx)
{
	void *pSeninf = ctx->reg_if_top;
	unsigned int seninf_src = 0;
	unsigned int temp0 = 0;
	unsigned int temp1 = 0;

	temp0 = SENINF_READ_REG(pSeninf, SENINF_TOP_MUX_CTRL_0);
	temp1 = SENINF_READ_REG(pSeninf, SENINF_TOP_MUX_CTRL_1);

	switch (mux_idx) {
	case SENINF_MUX1:
		seninf_src = (temp0 & 0xF);
		break;
	case SENINF_MUX2:
		seninf_src = (temp0 & 0xF00) >> 8;
		break;
	case SENINF_MUX3:
		seninf_src = (temp0 & 0xF0000) >> 16;
		break;
	case SENINF_MUX4:
		seninf_src = (temp0 & 0xF000000) >> 24;
		break;
	case SENINF_MUX5:
		seninf_src = (temp1 & 0xF);
		break;
	case SENINF_MUX6:
		seninf_src = (temp1 & 0xF00) >> 8;
		break;
	case SENINF_MUX7:
		seninf_src = (temp1 & 0xF0000) >> 16;
		break;
	case SENINF_MUX8:
		seninf_src = (temp1 & 0xF000000) >> 24;
		break;
	default:
		dev_err(ctx->dev, "invalid mux_idx %d", mux_idx);
		return -EINVAL;
	}

	return seninf_src;
}

int mtk_cam_seninf_get_cammux_ctrl(struct seninf_ctx *ctx, int cam_mux)
{
	void *pSeninf_cam_mux = ctx->reg_if_cam_mux;
	unsigned int temp0, temp1, temp2, temp3;
	unsigned int seninfMuxSrc = 0;

	temp0 = SENINF_READ_REG(pSeninf_cam_mux, SENINF_CAM_MUX_CTRL_0);
	temp1 = SENINF_READ_REG(pSeninf_cam_mux, SENINF_CAM_MUX_CTRL_1);
	temp2 = SENINF_READ_REG(pSeninf_cam_mux, SENINF_CAM_MUX_CTRL_2);
	temp3 = SENINF_READ_REG(pSeninf_cam_mux, SENINF_CAM_MUX_CTRL_3);

	switch (cam_mux) {
	case SENINF_CAM_MUX0:
		seninfMuxSrc = (temp0 & 0xF);
		break;
	case SENINF_CAM_MUX1:
		seninfMuxSrc = (temp0 & 0xF00) >> 8;
		break;
	case SENINF_CAM_MUX2:
		seninfMuxSrc = (temp0 & 0xF0000) >> 16;
		break;
	case SENINF_CAM_MUX3:
		seninfMuxSrc = (temp0 & 0xF000000) >> 24;
		break;
	case SENINF_CAM_MUX4:
		seninfMuxSrc = (temp1 & 0xF);
		break;
	case SENINF_CAM_MUX5:
		seninfMuxSrc = (temp1 & 0xF00) >> 8;
		break;
	case SENINF_CAM_MUX6:
		seninfMuxSrc = (temp1 & 0xF0000) >> 16;
		break;
	case SENINF_CAM_MUX7:
		seninfMuxSrc = (temp1 & 0xF000000) >> 24;
		break;
	case SENINF_CAM_MUX8:
		seninfMuxSrc = (temp2 & 0xF);
		break;
	case SENINF_CAM_MUX9:
		seninfMuxSrc = (temp2 & 0xF00) >> 8;
		break;
	case SENINF_CAM_MUX10:
		seninfMuxSrc = (temp2 & 0xF0000) >> 16;
		break;
	case SENINF_CAM_MUX11:
		seninfMuxSrc = (temp2 & 0xF000000) >> 24;
		break;
	case SENINF_CAM_MUX12:
		seninfMuxSrc = (temp3 & 0xF);
		break;
	default:
		dev_err(ctx->dev, "invalid cam_mux %d", cam_mux);
		return -EINVAL;
	}

	return seninfMuxSrc;
}

u32 mtk_cam_seninf_get_cammux_res(struct seninf_ctx *ctx, int cam_mux)
{
	return SENINF_READ_REG(ctx->reg_if_cam_mux,
			SENINF_CAM_MUX0_CHK_RES + (0x10 * cam_mux));
}

u32 mtk_cam_seninf_get_cammux_exp(struct seninf_ctx *ctx, int cam_mux)
{
	return SENINF_READ_REG(ctx->reg_if_cam_mux,
			SENINF_CAM_MUX0_CHK_CTL_1 + (0x10 * cam_mux));
}

int mtk_cam_seninf_set_cammux_vc(struct seninf_ctx *ctx, int cam_mux,
				 int vc_sel, int dt_sel, int vc_en, int dt_en)
{
	void *mpSeninfCamMuxVCAddr = ctx->reg_if_cam_mux +
			(4 * cam_mux);

	dev_info(ctx->dev, "cam_mux %d vc 0x%x dt 0x%x, vc_en %d dt_en %d\n",
		 cam_mux, vc_sel, dt_sel, vc_en, dt_en);

	SENINF_BITS(mpSeninfCamMuxVCAddr, SENINF_CAM_MUX0_OPT,
		    RG_SENINF_CAM_MUX0_VC_SEL, vc_sel);
	SENINF_BITS(mpSeninfCamMuxVCAddr, SENINF_CAM_MUX0_OPT,
		    RG_SENINF_CAM_MUX0_DT_SEL, dt_sel);
	SENINF_BITS(mpSeninfCamMuxVCAddr, SENINF_CAM_MUX0_OPT,
		    RG_SENINF_CAM_MUX0_VC_EN, vc_en);
	SENINF_BITS(mpSeninfCamMuxVCAddr, SENINF_CAM_MUX0_OPT,
		    RG_SENINF_CAM_MUX0_DT_EN, dt_en);

	return 0;
}

int mtk_cam_seninf_set_cammux_src(struct seninf_ctx *ctx, int src, int target,
				  int exp_hsize, int exp_vsize)
{
	void *mpSeninfCamMux_base = ctx->reg_if_cam_mux;

	dev_info(ctx->dev, "cam_mux %d src %d\n", target, src);

	switch (target) {
	case SENINF_CAM_MUX0:
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX_CTRL_0,
			    RG_SENINF_CAM_MUX0_SRC_SEL, src);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX0_CHK_CTL_1,
			    RG_SENINF_CAM_MUX0_EXP_HSIZE, exp_hsize);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX0_CHK_CTL_1,
			    RG_SENINF_CAM_MUX0_EXP_VSIZE, exp_vsize);
		break;
	case SENINF_CAM_MUX1:
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX_CTRL_0,
			    RG_SENINF_CAM_MUX1_SRC_SEL, src);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX1_CHK_CTL_1,
			    RG_SENINF_CAM_MUX1_EXP_HSIZE, exp_hsize);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX1_CHK_CTL_1,
			    RG_SENINF_CAM_MUX1_EXP_VSIZE, exp_vsize);
		break;
	case SENINF_CAM_MUX2:
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX_CTRL_0,
			    RG_SENINF_CAM_MUX2_SRC_SEL, src);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX2_CHK_CTL_1,
			    RG_SENINF_CAM_MUX2_EXP_HSIZE, exp_hsize);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX2_CHK_CTL_1,
			    RG_SENINF_CAM_MUX2_EXP_VSIZE, exp_vsize);
		break;
	case SENINF_CAM_MUX3:
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX_CTRL_0,
			    RG_SENINF_CAM_MUX3_SRC_SEL, src);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX3_CHK_CTL_1,
			    RG_SENINF_CAM_MUX3_EXP_HSIZE, exp_hsize);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX3_CHK_CTL_1,
			    RG_SENINF_CAM_MUX3_EXP_VSIZE, exp_vsize);
		break;
	case SENINF_CAM_MUX4:
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX_CTRL_1,
			    RG_SENINF_CAM_MUX4_SRC_SEL, src);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX4_CHK_CTL_1,
			    RG_SENINF_CAM_MUX4_EXP_HSIZE, exp_hsize);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX4_CHK_CTL_1,
			    RG_SENINF_CAM_MUX4_EXP_VSIZE, exp_vsize);
		break;
	case SENINF_CAM_MUX5:
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX_CTRL_1,
			    RG_SENINF_CAM_MUX5_SRC_SEL, src);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX5_CHK_CTL_1,
			    RG_SENINF_CAM_MUX5_EXP_HSIZE, exp_hsize);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX5_CHK_CTL_1,
			    RG_SENINF_CAM_MUX5_EXP_VSIZE, exp_vsize);
		break;
	case SENINF_CAM_MUX6:
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX_CTRL_1,
			    RG_SENINF_CAM_MUX6_SRC_SEL, src);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX6_CHK_CTL_1,
			    RG_SENINF_CAM_MUX6_EXP_HSIZE, exp_hsize);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX6_CHK_CTL_1,
			    RG_SENINF_CAM_MUX6_EXP_VSIZE, exp_vsize);
		break;
	case SENINF_CAM_MUX7:
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX_CTRL_1,
			    RG_SENINF_CAM_MUX7_SRC_SEL, src);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX7_CHK_CTL_1,
			    RG_SENINF_CAM_MUX7_EXP_HSIZE, exp_hsize);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX7_CHK_CTL_1,
			    RG_SENINF_CAM_MUX7_EXP_VSIZE, exp_vsize);
		break;
	case SENINF_CAM_MUX8:
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX_CTRL_2,
			    RG_SENINF_CAM_MUX8_SRC_SEL, src);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX8_CHK_CTL_1,
			    RG_SENINF_CAM_MUX8_EXP_HSIZE, exp_hsize);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX8_CHK_CTL_1,
			    RG_SENINF_CAM_MUX8_EXP_VSIZE, exp_vsize);
		break;
	case SENINF_CAM_MUX9:
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX_CTRL_2,
			    RG_SENINF_CAM_MUX9_SRC_SEL, src);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX9_CHK_CTL_1,
			    RG_SENINF_CAM_MUX9_EXP_HSIZE, exp_hsize);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX9_CHK_CTL_1,
			    RG_SENINF_CAM_MUX9_EXP_VSIZE, exp_vsize);
		break;
	case SENINF_CAM_MUX10:
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX_CTRL_2,
			    RG_SENINF_CAM_MUX10_SRC_SEL, src);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX10_CHK_CTL_1,
			    RG_SENINF_CAM_MUX10_EXP_HSIZE, exp_hsize);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX10_CHK_CTL_1,
			    RG_SENINF_CAM_MUX10_EXP_VSIZE, exp_vsize);
		break;
	case SENINF_CAM_MUX11:
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX_CTRL_2,
			    RG_SENINF_CAM_MUX11_SRC_SEL, src);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX11_CHK_CTL_1,
			    RG_SENINF_CAM_MUX11_EXP_HSIZE, exp_hsize);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX11_CHK_CTL_1,
			    RG_SENINF_CAM_MUX11_EXP_VSIZE, exp_vsize);
		break;
	case SENINF_CAM_MUX12:
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX_CTRL_3,
			    RG_SENINF_CAM_MUX12_SRC_SEL, src);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX12_CHK_CTL_1,
			    RG_SENINF_CAM_MUX12_EXP_HSIZE, exp_hsize);
		SENINF_BITS(mpSeninfCamMux_base, SENINF_CAM_MUX12_CHK_CTL_1,
			    RG_SENINF_CAM_MUX12_EXP_VSIZE, exp_vsize);
		break;
	default:
		dev_err(ctx->dev, "invalid src %d target %d", src, target);
		return -EINVAL;
	}

	return 0;
}

int mtk_cam_seninf_set_vc(struct seninf_ctx *ctx, int intf,
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

	return 0;
}

int mtk_cam_seninf_set_mux_ctrl(struct seninf_ctx *ctx, int mux,
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

	dev_info(ctx->dev, "SENINF_MUX_CTRL_1(0x%x), SENINF_MUX_OPT(0x%x)",
		 SENINF_READ_REG(pSeninf_mux, SENINF_MUX_CTRL_1),
		SENINF_READ_REG(pSeninf_mux, SENINF_MUX_OPT));

	return 0;
}

int mtk_cam_seninf_set_mux_crop(struct seninf_ctx *ctx, int mux,
				int start_x, int end_x, int enable)
{
	void *pSeninf_mux = ctx->reg_if_mux[mux];

	SENINF_BITS(pSeninf_mux, SENINF_MUX_CROP_PIX_CTRL,
		    RG_SENINF_MUX_CROP_START_8PIX_CNT, start_x / 8);
	SENINF_BITS(pSeninf_mux, SENINF_MUX_CROP_PIX_CTRL,
		    RG_SENINF_MUX_CROP_END_8PIX_CNT,
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

int mtk_cam_seninf_is_mux_used(struct seninf_ctx *ctx, int mux)
{
	void *pSeninf_mux = ctx->reg_if_mux[mux];

	return SENINF_READ_BITS(pSeninf_mux, SENINF_MUX_CTRL_0, SENINF_MUX_EN);
}

int mtk_cam_seninf_mux(struct seninf_ctx *ctx, int mux)
{
	void *pSeninf_mux = ctx->reg_if_mux[mux];

	SENINF_BITS(pSeninf_mux, SENINF_MUX_CTRL_0, SENINF_MUX_EN, 1);
	return 0;
}

int mtk_cam_seninf_disable_mux(struct seninf_ctx *ctx, int mux)
{
	int i;
	void *pSeninf_mux = ctx->reg_if_mux[mux];

	SENINF_BITS(pSeninf_mux, SENINF_MUX_CTRL_0, SENINF_MUX_EN, 0);

	//also disable CAM_MUX with input from mux
	for (i = SENINF_CAM_MUX0; i < SENINF_CAM_MUX_NUM; i++) {
		if (mux == mtk_cam_seninf_get_cammux_ctrl(ctx, i))
			mtk_cam_seninf_disable_cammux(ctx, i);
	}

	return 0;
}

int mtk_cam_seninf_disable_all_mux(struct seninf_ctx *ctx)
{
	int i;
	void *pSeninf_mux;

	for (i = 0; i < SENINF_MUX_NUM; i++) {
		pSeninf_mux = ctx->reg_if_mux[i];
		SENINF_BITS(pSeninf_mux, SENINF_MUX_CTRL_0, SENINF_MUX_EN, 0);
	}

	return 0;
}

int mtk_cam_seninf_set_cammux_chk_pixel_mode(struct seninf_ctx *ctx,
					     int cam_mux, int pixel_mode)
{
	void *mpSeninfCamMux_base = ctx->reg_if_cam_mux;

	dev_info(ctx->dev, "cam_mux %d chk pixel_mode %d\n",
		 cam_mux, pixel_mode);

	switch (cam_mux) {
	case SENINF_CAM_MUX0:
		SENINF_BITS(mpSeninfCamMux_base,
			    SENINF_CAM_MUX0_CHK_CTL_0,
			RG_SENINF_CAM_MUX0_PIX_MODE_SEL, pixel_mode);
		break;
	case SENINF_CAM_MUX1:
		SENINF_BITS(mpSeninfCamMux_base,
			    SENINF_CAM_MUX1_CHK_CTL_0,
			RG_SENINF_CAM_MUX1_PIX_MODE_SEL, pixel_mode);
		break;
	case SENINF_CAM_MUX2:
		SENINF_BITS(mpSeninfCamMux_base,
			    SENINF_CAM_MUX2_CHK_CTL_0,
			RG_SENINF_CAM_MUX2_PIX_MODE_SEL, pixel_mode);
		break;
	case SENINF_CAM_MUX3:
		SENINF_BITS(mpSeninfCamMux_base,
			    SENINF_CAM_MUX3_CHK_CTL_0,
			RG_SENINF_CAM_MUX3_PIX_MODE_SEL, pixel_mode);
		break;
	case SENINF_CAM_MUX4:
		SENINF_BITS(mpSeninfCamMux_base,
			    SENINF_CAM_MUX4_CHK_CTL_0,
			RG_SENINF_CAM_MUX4_PIX_MODE_SEL, pixel_mode);
		break;
	case SENINF_CAM_MUX5:
		SENINF_BITS(mpSeninfCamMux_base,
			    SENINF_CAM_MUX5_CHK_CTL_0,
			RG_SENINF_CAM_MUX5_PIX_MODE_SEL, pixel_mode);
		break;
	case SENINF_CAM_MUX6:
		SENINF_BITS(mpSeninfCamMux_base,
			    SENINF_CAM_MUX6_CHK_CTL_0,
			RG_SENINF_CAM_MUX6_PIX_MODE_SEL, pixel_mode);
		break;
	case SENINF_CAM_MUX7:
		SENINF_BITS(mpSeninfCamMux_base,
			    SENINF_CAM_MUX7_CHK_CTL_0,
			RG_SENINF_CAM_MUX7_PIX_MODE_SEL, pixel_mode);
		break;
	case SENINF_CAM_MUX8:
		SENINF_BITS(mpSeninfCamMux_base,
			    SENINF_CAM_MUX8_CHK_CTL_0,
			RG_SENINF_CAM_MUX8_PIX_MODE_SEL, pixel_mode);
		break;
	case SENINF_CAM_MUX9:
		SENINF_BITS(mpSeninfCamMux_base,
			    SENINF_CAM_MUX9_CHK_CTL_0,
			RG_SENINF_CAM_MUX9_PIX_MODE_SEL, pixel_mode);
		break;
	case SENINF_CAM_MUX10:
		SENINF_BITS(mpSeninfCamMux_base,
			    SENINF_CAM_MUX10_CHK_CTL_0,
			RG_SENINF_CAM_MUX10_PIX_MODE_SEL, pixel_mode);
		break;
	case SENINF_CAM_MUX11:
		SENINF_BITS(mpSeninfCamMux_base,
			    SENINF_CAM_MUX11_CHK_CTL_0,
			RG_SENINF_CAM_MUX11_PIX_MODE_SEL, pixel_mode);
		break;
	case SENINF_CAM_MUX12:
		SENINF_BITS(mpSeninfCamMux_base,
			    SENINF_CAM_MUX12_CHK_CTL_0,
			RG_SENINF_CAM_MUX12_PIX_MODE_SEL, pixel_mode);
		break;
	default:
		dev_err(ctx->dev, "invalid cam_mux %d pixel_mode %d\n",
			cam_mux, pixel_mode);
		return -EINVAL;
	}

	return 0;
}

int mtk_cam_seninf_set_test_model(struct seninf_ctx *ctx,
				  int mux, int cam_mux, int pixel_mode)
{
	int intf;
	void *pSeninf;
	void *pSeninf_tg;
	void *pSeninf_mux;
	void *pSeninf_cam_mux;

	intf = 0; /* XXX: only seninf1 testmdl is valid */

	pSeninf = ctx->reg_if_ctrl[intf];
	pSeninf_tg = ctx->reg_if_tg[intf];
	pSeninf_mux = ctx->reg_if_mux[mux];
	pSeninf_cam_mux = ctx->reg_if_cam_mux;

	mtk_cam_seninf_reset(ctx, intf);
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

int csirx_phyA_power_on(struct seninf_ctx *ctx, int portIdx, int en)
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
	}

	dev_info(ctx->dev, "portIdx %d en %d CDPHY_RX_ANA_0 0x%x ANA_8 0x%x\n",
		 portIdx, en,
		SENINF_READ_REG(base, CDPHY_RX_ANA_0),
		SENINF_READ_REG(base, CDPHY_RX_ANA_8));

	return 0;
}

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
		SENINF_BITS(base, CDPHY_RX_ANA_9,
			    RG_CSI0_RESERVE, 0x3003);
		SENINF_BITS(base, CDPHY_RX_ANA_SETTING_0,
			    CSR_CSI_RST_MODE, 0x2);

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
			    RG_CSI0_CPHY_T0_CDR_CK_DELAY, 0x0);
		SENINF_BITS(base, CDPHY_RX_ANA_7,
			    RG_CSI0_CPHY_T1_CDR_CK_DELAY, 0x0);
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

		dev_info(ctx->dev, "port:%d CDPHY_RX_ANA_0(0x%x)\n",
			 port, SENINF_READ_REG(base, CDPHY_RX_ANA_0));
	}

	return 0;
}

static int csirx_dphy_init(struct seninf_ctx *ctx)
{
	void *base = ctx->reg_ana_dphy_top[ctx->port];
	int settle_delay_data, hs_trail, hs_trail_en, bit_per_pixel;
	u64 data_rate;

	/* TODO */
	settle_delay_data = SENINF_SETTLE_DELAY_DT;

	SENINF_BITS(base, DPHY_RX_DATA_LANE0_HS_PARAMETER,
		    RG_CDPHY_RX_LD0_TRIO0_HS_SETTLE_PARAMETER,
		    settle_delay_data);
	SENINF_BITS(base, DPHY_RX_DATA_LANE1_HS_PARAMETER,
		    RG_CDPHY_RX_LD1_TRIO1_HS_SETTLE_PARAMETER,
		    settle_delay_data);
	SENINF_BITS(base, DPHY_RX_DATA_LANE2_HS_PARAMETER,
		    RG_CDPHY_RX_LD2_TRIO2_HS_SETTLE_PARAMETER,
		    settle_delay_data);
	SENINF_BITS(base, DPHY_RX_DATA_LANE3_HS_PARAMETER,
		    RG_CDPHY_RX_LD3_TRIO3_HS_SETTLE_PARAMETER,
		    settle_delay_data);

	SENINF_BITS(base, DPHY_RX_CLOCK_LANE0_HS_PARAMETER,
		    RG_DPHY_RX_LC0_HS_SETTLE_PARAMETER,
		    SENINF_SETTLE_DELAY_CK);
	SENINF_BITS(base, DPHY_RX_CLOCK_LANE1_HS_PARAMETER,
		    RG_DPHY_RX_LC1_HS_SETTLE_PARAMETER,
		    SENINF_SETTLE_DELAY_CK);

	/*Settle delay by lane*/
	SENINF_BITS(base, DPHY_RX_DATA_LANE0_HS_PARAMETER,
		    RG_CDPHY_RX_LD0_TRIO0_HS_PREPARE_PARAMETER, 2);
	SENINF_BITS(base, DPHY_RX_DATA_LANE1_HS_PARAMETER,
		    RG_CDPHY_RX_LD1_TRIO1_HS_PREPARE_PARAMETER, 2);
	SENINF_BITS(base, DPHY_RX_DATA_LANE2_HS_PARAMETER,
		    RG_CDPHY_RX_LD2_TRIO2_HS_PREPARE_PARAMETER, 2);
	SENINF_BITS(base, DPHY_RX_DATA_LANE3_HS_PARAMETER,
		    RG_CDPHY_RX_LD3_TRIO3_HS_PREPARE_PARAMETER, 2);

	/* TODO */
	hs_trail = SENINF_HS_TRAIL_PARAMETER;

	SENINF_BITS(base, DPHY_RX_DATA_LANE0_HS_PARAMETER,
		    RG_DPHY_RX_LD0_HS_TRAIL_PARAMETER, hs_trail);
	SENINF_BITS(base, DPHY_RX_DATA_LANE1_HS_PARAMETER,
		    RG_DPHY_RX_LD1_HS_TRAIL_PARAMETER, hs_trail);
	SENINF_BITS(base, DPHY_RX_DATA_LANE2_HS_PARAMETER,
		    RG_DPHY_RX_LD2_HS_TRAIL_PARAMETER, hs_trail);
	SENINF_BITS(base, DPHY_RX_DATA_LANE3_HS_PARAMETER,
		    RG_DPHY_RX_LD3_HS_TRAIL_PARAMETER, hs_trail);

	if (!ctx->is_cphy) {
		/* TODO */
		bit_per_pixel = 10;
		data_rate = ctx->mipi_pixel_rate * bit_per_pixel;
		do_div(data_rate, ctx->num_data_lanes);
		hs_trail_en = data_rate < SENINF_HS_TRAIL_EN_CONDITION;

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

	SENINF_BITS(pSeninf_csi2, SENINF_CSI2_DBG_CTRL,
		    RG_CSI2_DBG_PACKET_CNT_EN, 1);

	// lane/trio count
	SENINF_BITS(pSeninf_csi2, SENINF_CSI2_RESYNC_MERGE_CTRL,
		    RG_CSI2_RESYNC_CYCLE_CNT_OPT, 1);

	csi_en = (1 << ctx->num_data_lanes) - 1;

	if (!ctx->is_cphy) { //Dphy
		SENINF_BITS(pSeninf_csi2, SENINF_CSI2_OPT,
			    RG_CSI2_CPHY_SEL, 0);
		SENINF_WRITE_REG(pSeninf_csi2, SENINF_CSI2_EN, csi_en);
		SENINF_BITS(pSeninf_csi2, SENINF_CSI2_HDR_MODE_0,
			    RG_CSI2_HEADER_MODE, 0);
		SENINF_BITS(pSeninf_csi2, SENINF_CSI2_HDR_MODE_0,
			    RG_CSI2_HEADER_LEN, 0);
	} else { //Cphy
		u8 map_hdr_len[] = {0, 1, 2, 4, 5};

		SENINF_WRITE_REG(pSeninf_csi2, SENINF_CSI2_EN, csi_en);
		SENINF_BITS(pSeninf_csi2, SENINF_CSI2_OPT,
			    RG_CSI2_CPHY_SEL, 1);
		SENINF_BITS(pSeninf_csi2, SENINF_CSI2_HDR_MODE_0,
			    RG_CSI2_HEADER_MODE, 2); //cphy
		SENINF_BITS(pSeninf_csi2, SENINF_CSI2_HDR_MODE_0,
			    RG_CSI2_HEADER_LEN,
			    map_hdr_len[ctx->num_data_lanes]);
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
	default:
		break;
	}

	return 0;
}

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
		} else {
			SENINF_BITS(base, CDPHY_RX_ANA_0,
				    RG_CSI0_CPHY_EN, 1);
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

	return 0;
}

static int csirx_cphy_setting(struct seninf_ctx *ctx)
{
	void *base = ctx->reg_ana_cphy_top[ctx->port];

	switch (ctx->port) {
	case CSI_PORT_0:
	case CSI_PORT_1:
	case CSI_PORT_2:
	case CSI_PORT_3:
	case CSI_PORT_0A:
	case CSI_PORT_1A:
	case CSI_PORT_2A:
	case CSI_PORT_3A:
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
		if (ctx->num_data_lanes == 2) {
			SENINF_BITS(base, CPHY_RX_CTRL, CPHY_RX_TR2_LPRX_EN, 1);
			SENINF_BITS(base, CPHY_RX_CTRL, CPHY_RX_TR3_LPRX_EN, 1);
		} else
			SENINF_BITS(base, CPHY_RX_CTRL, CPHY_RX_TR2_LPRX_EN, 1);
		break;
	default:
		break;
	}

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

int mtk_cam_seninf_set_csi_mipi(struct seninf_ctx *ctx)
{
	csirx_phy_init(ctx);

	/* seninf csi2 */
	csirx_seninf_csi2_setting(ctx);

	/* seninf */
	csirx_seninf_setting(ctx);

	/* seninf top */
	csirx_seninf_top_setting(ctx);

	/* phy */
	csirx_phy_setting(ctx);

	return 0;
}

int mtk_cam_seninf_poweroff(struct seninf_ctx *ctx)
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

int mtk_cam_seninf_reset(struct seninf_ctx *ctx, int seninfIdx)
{
	int i;
	void *pSeninf_mux;
	void *pSeninf = ctx->reg_if_ctrl[seninfIdx];

	SENINF_BITS(pSeninf, SENINF_CSI2_CTRL, SENINF_CSI2_SW_RST, 1);
	udelay(1);
	SENINF_BITS(pSeninf, SENINF_CSI2_CTRL, SENINF_CSI2_SW_RST, 0);

	dev_info(ctx->dev, "reset seninf %d\n", seninfIdx);

	for (i = SENINF_MUX1; i < SENINF_MUX_NUM; i++)
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

int mtk_cam_seninf_set_idle(struct seninf_ctx *ctx)
{
	int i;
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;

	for (i = 0; i < vcinfo->cnt; i++) {
		vc = &vcinfo->vc[i];
		mtk_cam_seninf_disable_mux(ctx, vc->mux);
		mtk_cam_seninf_disable_cammux(ctx, vc->cam);
	}

	return 0;
}

int mtk_cam_seninf_get_mux_meter(struct seninf_ctx *ctx, int mux,
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

ssize_t mtk_cam_seninf_show_status(struct device *dev,
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
	void *csi2, *pmux;

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
		SHOW(buf, len, "csi2 irq_stat 0x%08x\n",
		     SENINF_READ_REG(csi2, SENINF_CSI2_IRQ_STATUS));
		SHOW(buf, len, "csi2 line_frame_num 0x%08x\n",
		     SENINF_READ_REG(csi2, SENINF_CSI2_LINE_FRAME_NUM));
		SHOW(buf, len, "csi2 packete_status 0x%08x\n",
		     SENINF_READ_REG(csi2, SENINF_CSI2_PACKET_STATUS));
		SHOW(buf, len, "csi2 packete_cnt_status 0x%08x\n",
		     SENINF_READ_REG(csi2, SENINF_CSI2_PACKET_CNT_STATUS));

		for (i = 0; i < ctx->vcinfo.cnt; i++) {
			vc = &ctx->vcinfo.vc[i];
			pmux = ctx->reg_if_mux[vc->mux];
			SHOW(buf, len,
			     "[%d] vc 0x%x dt 0x%x mux %d cam %d\n",
				i, vc->vc, vc->dt, vc->mux, vc->cam);
			SHOW(buf, len,
			     "\tmux[%d] en %d src %d irq_stat 0x%x\n",
				vc->mux,
				mtk_cam_seninf_is_mux_used(ctx, vc->mux),
				mtk_cam_seninf_get_top_mux_ctrl(ctx, vc->mux),
				SENINF_READ_REG(pmux, SENINF_MUX_IRQ_STATUS));
			SHOW(buf, len,
			     "\tcam[%d] en %d src %d exp 0x%x res 0x%x\n",
				vc->cam,
				mtk_cam_seninf_is_cammux_used(ctx, vc->cam),
				mtk_cam_seninf_get_cammux_ctrl(ctx, vc->cam),
				mtk_cam_seninf_get_cammux_exp(ctx, vc->cam),
				mtk_cam_seninf_get_cammux_res(ctx, vc->cam));

			if (vc->feature == VC_RAW_DATA) {
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

