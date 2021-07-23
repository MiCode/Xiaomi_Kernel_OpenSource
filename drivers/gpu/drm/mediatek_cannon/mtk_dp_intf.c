/*
 * Copyright (c) 2015 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <drm/drmP.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/soc/mediatek/mtk-cmdq.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_fb.h"
#include "mt-plat/sync_write.h"
#include "mtk_dp.h"

#define DP_EN                             0x0000
	#define DP_CONTROLLER_EN BIT(0)
	#define CON_FLD_DP_EN		REG_FLD_MSB_LSB(0, 0)
#define DP_RST                            0x0004
	#define CON_FLD_DP_RST			REG_FLD_MSB_LSB(0, 0)
	#define CON_FLD_DP_RST_SEL		REG_FLD_MSB_LSB(16, 16)
#define DP_INTEN                          0x0008
	#define INT_TARGET_LINE_EN BIT(3)
	#define INT_UNDERFLOW_EN BIT(2)
	#define INT_VDE_EN BIT(1)
	#define INT_VSYNC_EN BIT(0)
#define DP_INTSTA                         0x000C
	#define INTSTA_TARGET_LINE BIT(3)
	#define INTSTA_UNDERFLOW BIT(2)
	#define INTSTA_VDE BIT(1)
	#define INTSTA_VSYNC BIT(0)
#define DP_CON                            0x0010
	#define CON_FLD_DP_BG_EN		REG_FLD_MSB_LSB(0, 0)
	#define CON_FLD_DP_INTL_EN		REG_FLD_MSB_LSB(2, 2)
#define DP_OUTPUT_SETTING                 0x0014
	#define RB_SWAP BIT(0)
#define DP_SIZE                           0x0018
#define DP_TGEN_HWIDTH                    0x0020
#define DP_TGEN_HPORCH                    0x0024
#define DP_TGEN_VWIDTH                    0x0028
#define DP_TGEN_VPORCH                    0x002C
#define DP_BG_HCNTL                       0x0030
#define DP_BG_VCNTL                       0x0034
#define DP_BG_COLOR                       0x0038
#define DP_FIFO_CTL                       0x003C
#define DP_STATUS                         0x0040
	#define DP_BUSY BIT(24)
#define DP_DCM                            0x004C
#define DP_DUMMY                          0x0050
#define DP_TGEN_VWIDTH_LEVEN              0x0068
#define DP_TGEN_VPORCH_LEVEN              0x006C
#define DP_TGEN_VWIDTH_RODD               0x0070
#define DP_TGEN_VPORCH_RODD               0x0074
#define DP_TGEN_VWIDTH_REVEN              0x0078
#define DP_TGEN_VPORCH_REVEN              0x007C
#define DP_MUTEX_VSYNC_SETTING            0x00E0
#define DP_SHEUDO_REG_UPDATE              0x00E4
#define DP_INTERNAL_DCM_DIS               0x00E8
#define DP_TARGET_LINE                    0x00F0
#define DP_CHKSUM_EN                      0x0100
#define DP_CHKSUM0                        0x0104
#define DP_CHKSUM1                        0x0108
#define DP_CHKSUM2                        0x010C
#define DP_CHKSUM3                        0x0110
#define DP_CHKSUM4                        0x0114
#define DP_CHKSUM5                        0x0118
#define DP_CHKSUM6                        0x011C
#define DP_CHKSUM7                        0x0120
#define DP_PATTERN_CTRL0                  0x0F00
#define DP_PATTERN_CTRL1                  0x0F04

static const struct of_device_id mtk_dp_intf_driver_dt_match[];
/**
 * struct mtk_dp_intf - DP_INTF driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 */
struct mtk_dp_intf {
	struct mtk_ddp_comp	 ddp_comp;
	struct device *dev;
	struct mtk_dp_intf_driver_data *driver_data;
	struct drm_encoder encoder;
	struct drm_connector conn;
	struct drm_bridge *bridge;
	void __iomem *regs;
	struct clk *hf_fmm_ck;
	struct clk *hf_fdp_ck;
	struct clk *pclk;
	struct clk *pclk_src[5];
	int irq;
	struct drm_display_mode mode;
	int enable;
	int res;
};

struct mtk_dp_intf_driver_data {
	const u32 reg_cmdq_ofs;
	s32 (*poll_for_idle)(struct mtk_dp_intf *dp_intf,
		struct cmdq_pkt *handle);
	irqreturn_t (*irq_handler)(int irq, void *dev_id);
};

#define DISP_REG_SET(handle, reg32, val) \
	do { \
		if (handle == NULL) { \
			mt_reg_sync_writel(val, (unsigned long *)(reg32));\
		} \
	} while (0)

#if 0
#define DISP_REG_SET_FIELD(handle, field, reg32, val)  \
	do {  \
		if (handle == NULL) { \
			unsigned int regval; \
			regval = __raw_readl((unsigned long *)(reg32)); \
			regval  = (regval & ~REG_FLD_MASK(field)) | \
				(REG_FLD_VAL((field), (val))); \
			mt_reg_sync_writel(regval, (reg32));  \
		} \
	} while (0)
#else
#define DISP_REG_SET_FIELD(handle, field, reg32, val)  \
	do {  \
		if (handle == NULL) { \
			unsigned int regval; \
			regval = readl((unsigned long *)(reg32)); \
			regval  = (regval & ~REG_FLD_MASK(field)) | \
				(REG_FLD_VAL((field), (val))); \
			writel(regval, (reg32));  \
		} \
	} while (0)

#endif

static void __iomem	*clk_apmixed_base;
static int irq_intsa;
static int irq_vdesa;
static int irq_underflowsa;
static int irq_tl;
static struct mtk_dp_intf *g_dp_intf;


static inline struct mtk_dp_intf *comp_to_dp_intf(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_dp_intf, ddp_comp);
}

static inline struct mtk_dp_intf *encoder_to_dp_intf(struct drm_encoder *e)
{
	return container_of(e, struct mtk_dp_intf, encoder);
}

static inline struct mtk_dp_intf *connector_to_dp_intf(struct drm_connector *c)
{
	return container_of(c, struct mtk_dp_intf, conn);
}

static void mtk_dp_intf_mask(struct mtk_dp_intf *dp_intf, u32 offset,
	u32 mask, u32 data)
{
	u32 temp = readl(dp_intf->regs + offset);

	writel((temp & ~mask) | (data & mask), dp_intf->regs + offset);
}

static void mtk_dp_intf_destroy_conn_enc(struct mtk_dp_intf *dp_intf)
{
	drm_encoder_cleanup(&dp_intf->encoder);
	/* Skip connector cleanup if creation was delegated to the bridge */
	if (dp_intf->conn.dev)
		drm_connector_cleanup(&dp_intf->conn);
}

static void mtk_dp_intf_start(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle)
{
	void __iomem *baddr = comp->regs;
	struct mtk_dp_intf *dp_intf = comp_to_dp_intf(comp);

	irq_intsa = 0;
	irq_vdesa = 0;
	irq_underflowsa = 0;
	irq_tl = 0;

	mtk_dp_intf_mask(dp_intf, DP_INTSTA, 0xf, 0);

	mtk_ddp_write_mask(comp, 1,
		DP_RST, CON_FLD_DP_RST, handle);
	mtk_ddp_write_mask(comp, 0,
		DP_RST, CON_FLD_DP_RST, handle);

#if 0
	mtk_ddp_write_mask(comp,
			(INT_UNDERFLOW_EN |
			 INT_VDE_EN | INT_VSYNC_EN),
			DP_INTEN,
			(INT_UNDERFLOW_EN |
			 INT_VDE_EN | INT_VSYNC_EN), handle);
#else
	mtk_ddp_write_mask(comp,
			INT_VSYNC_EN,
			DP_INTEN,
			(INT_UNDERFLOW_EN |
			 INT_VDE_EN | INT_VSYNC_EN), handle);

#endif
	mtk_ddp_write_mask(comp, DP_CONTROLLER_EN,
		DP_EN, DP_CONTROLLER_EN, handle);
	dp_intf->enable = 1;

	DDPMSG("%s, dp_intf_start:0x%x!\n",
		mtk_dump_comp_str(comp), readl(baddr + DP_EN));
}

static void mtk_dp_intf_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	mtk_ddp_write_mask(comp, 0x0, DP_EN, DP_CONTROLLER_EN, handle);

	//mtk_dp_video_trigger(video_mute<<16 | 0);
	irq_intsa = 0;
	irq_vdesa = 0;
	irq_underflowsa = 0;
	irq_tl = 0;

	DDPMSG("%s, stop\n", mtk_dump_comp_str(comp));
}

static void mtk_dp_intf_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_dp_intf *dp_intf = NULL;
	int ret;

	DDPFUNC();
	mtk_dp_poweron();

	dp_intf = comp_to_dp_intf(comp);

	/* Enable dp intf clk */
	if (dp_intf != NULL) {
		ret = clk_prepare_enable(dp_intf->hf_fmm_ck);
		if (ret < 0)
			DDPPR_ERR("%s Failed to enable hf_fmm_ck clock: %d\n",
				__func__, ret);
		ret = clk_prepare_enable(dp_intf->hf_fdp_ck);
		if (ret < 0)
			DDPPR_ERR("%s Failed to enable hf_fdp_ck clock: %d\n",
				__func__, ret);
		//ret = clk_prepare_enable(dp_intf->pclk);
		if (ret < 0)
			DDPPR_ERR("%s Failed to enable pclk clock: %d\n",
				__func__, ret);
		DDPMSG("%s:succesed eanble dp_intf clock\n", __func__);
	} else
		DDPPR_ERR("Failed to enable dp_intf clock\n");
}

static void mtk_dp_intf_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_dp_intf *dp_intf = NULL;

	DDPFUNC();
	mtk_dp_poweroff();

	dp_intf = comp_to_dp_intf(comp);

	/* disable dp intf clk */
	if (dp_intf != NULL) {
		clk_disable_unprepare(dp_intf->hf_fmm_ck);
		clk_disable_unprepare(dp_intf->hf_fdp_ck);
		clk_disable_unprepare(dp_intf->pclk);
		DDPMSG("%s:succesed disable dp_intf clock\n", __func__);
	} else
		DDPPR_ERR("Failed to disable dp_intf clock\n");
}

enum TVDPLL_CLK {
	TCK_26M = 0,
	TVDPLL_D2 = 1,
	TVDPLL_D4 = 2,
	TVDPLL_D8 = 3,
	TVDPLL_D16 = 4,
};

void mtk_dp_inf_video_clock(struct mtk_dp_intf *dp_intf)
{
	unsigned int clksrc = TVDPLL_D2;
	unsigned int con1 = 0;
	int ret = 0;
	struct device_node *node;

	switch (dp_intf->res) {
	case SINK_640_480: /* pix clk: 6.3M, dpi clk: 6.3*4M */
		clksrc = TVDPLL_D16;
		con1 = 0x840F81F8;
		break;
	case SINK_1280_720: /* pix clk: 18.5625M, dpi clk: 18.5625*4M */
		clksrc = TVDPLL_D8; // 18.585*4M
		con1 = 0x8416DFB4;
		break;
	case SINK_1920_1080: /* pix clk: 37.125M, dpi clk: 37.125*4M */
		clksrc = TVDPLL_D16;
		con1 = 0x8216D89D;
		break;
	case SINK_1080_2460: /* pix clk: 43.5275M, dpi clk: 43.5275*4M */
		clksrc = TVDPLL_D16;
		con1 = 0x821AC941;
		break;
	case SINK_1920_1200: /* pix clk: 43.5275M, dpi clk: 43.5275*4M */
		clksrc = TVDPLL_D16;
		con1 = 0x8217B645;
		break;
	case SINK_3840_2160_30: /* pix clk: 74.25M, dpi clk: 74.25*4M */
		clksrc = TVDPLL_D8;
		con1 = 0x8216D89D;
		break;
	case SINK_3840_2160:  /* pix clk: 74.25M, dpi clk: 74.25*4M */
		//clksrc = TVDPLL_D8;
		//con1 = 0x8216D89D;
		clksrc = TVDPLL_D4;
		con1 = 0x83109D89; //htotal = 1600
		con1 = 0x830F93B1; //htotal = 1500
		break;
	default:
		pr_info("%s error res %d!\n", __func__, dp_intf->res);

	}

	if (clk_apmixed_base == NULL) {
		node = of_find_compatible_node(NULL, NULL, "mediatek,apmixed");
		if (!node)
			pr_info("dp_intf [CLK_APMIXED] find node failed\n");
			clk_apmixed_base = of_iomap(node, 0);
		if (clk_apmixed_base == NULL) {
			pr_info("dp_intf [CLK_APMIXED] io map failed\n");
			return;
		}
	}

	pr_info("clk_apmixed_base clk_apmixed_base 0x%lx!!!,res %d\n",
		clk_apmixed_base, dp_intf->res);
	ret = clk_prepare_enable(dp_intf->pclk);
	ret = clk_set_parent(dp_intf->pclk, dp_intf->pclk_src[clksrc]);

	DISP_REG_SET(NULL, clk_apmixed_base + 0x384, con1);

	/*enable TVDPLL */
	DISP_REG_SET_FIELD(NULL, REG_FLD_MSB_LSB(0, 0),
			clk_apmixed_base + 0x380, 1);

	pr_info("%s set pclk2 and src %d\n", __func__, clksrc);

}

void mhal_DPTx_VideoClock(bool enable, int resolution)
{
	if (enable) {
		g_dp_intf->res = resolution;
		mtk_dp_inf_video_clock(g_dp_intf);
	} else
		clk_disable_unprepare(g_dp_intf->pclk);
}

static void mtk_dp_intf_config(struct mtk_ddp_comp *comp,
				 struct mtk_ddp_config *cfg,
				 struct cmdq_pkt *handle)
{
	/*u32 reg_val;*/
	struct mtk_dp_intf *dp_intf = comp_to_dp_intf(comp);
	unsigned int hsize, vsize;
	unsigned int hpw;
	unsigned int hfp, hbp;
	unsigned int vpw;
	unsigned int vfp, vbp;
	unsigned int bg_left, bg_right;
	unsigned int bg_top, bg_bot;

	pr_info("%s w %d, h, %d, fps %d!\n",
			__func__, cfg->w, cfg->h, cfg->vrefresh);

	hsize = cfg->w;
	vsize = cfg->h;
	if ((cfg->w == 640) && (cfg->h == 480)) {
		dp_intf->res = SINK_640_480;
		hpw = 24;
		hfp = 4;
		hbp = 12;
		vpw = 2;
		vfp = 10;
		vbp = 33;
	} else if ((cfg->w == 1280) && (cfg->h == 720)
	    && (cfg->vrefresh == 60)) {
		dp_intf->res = SINK_1280_720;
		hpw = 10;
		hfp = 28;
		hbp = 55;
		vpw = 5;
		vfp = 5;
		vbp = 20;
	} else if ((cfg->w == 1920) && (cfg->h == 1080)
		   && (cfg->vrefresh == 60)) {
		dp_intf->res = SINK_1920_1080;
		hpw = 11;
		hfp = 22;
		hbp = 37;
		vpw = 5;
		vfp = 4;
		vbp = 36;
	} else if ((cfg->w == 1080) && (cfg->h == 2460)
			  && (cfg->vrefresh == 60)) {
		dp_intf->res = SINK_1080_2460;
		hpw = 8;
		hfp = 8; //30/4
		hbp = 7; //30/4
		vpw = 2;
		vfp = 9;
		vbp = 5;
	} else if ((cfg->w == 1920) && (cfg->h == 1200)
			  && (cfg->vrefresh == 60)) {
		dp_intf->res = SINK_1920_1200;
		hpw = 8;
		hfp = 12;
		hbp = 20;
		vpw = 6;
		vfp = 3;
		vbp = 26;
	} else if ((cfg->w == 3840) && (cfg->h == 2160)
		   && (cfg->vrefresh == 30)) {
		dp_intf->res = SINK_3840_2160_30;
		hpw = 22;
		hfp = 44;
		hbp = 74;
		vpw = 10;
		vfp = 8;
		vbp = 72;
	} else if ((cfg->w == 3840) && (cfg->h == 2160)
		   && (cfg->vrefresh == 60)) {
		dp_intf->res = SINK_3840_2160;
		hpw = 10; //22;
		hfp = 25; //134;
		hbp = 20; //74;
		vpw = 10;
		vfp = 8;
		vbp = 72;
		hsize /= 3;/* with dsc on*/
		mtk_ddp_write_mask(comp, RB_SWAP,
			DP_OUTPUT_SETTING, RB_SWAP, handle);
	} else
		pr_info("%s error, w %d, h, %d, fps %d!\n",
			__func__, cfg->w, cfg->h, cfg->vrefresh);


	mtk_dp_inf_video_clock(dp_intf);

	mtk_ddp_write_relaxed(comp, vsize << 16 | hsize,
			DP_SIZE, handle);

	mtk_ddp_write_relaxed(comp, hpw,
			DP_TGEN_HWIDTH, handle);
	mtk_ddp_write_relaxed(comp, hfp << 16 | hbp,
			DP_TGEN_HPORCH, handle);

	mtk_ddp_write_relaxed(comp, vpw,
			DP_TGEN_VWIDTH, handle);
	mtk_ddp_write_relaxed(comp, vfp << 16 | vbp,
			DP_TGEN_VPORCH, handle);

	bg_left  = 0x0;
	bg_right = 0x0;
	mtk_ddp_write_relaxed(comp, bg_left << 16 | bg_right,
			DP_BG_HCNTL, handle);

	bg_top  = 0x0;
	bg_bot  = 0x0;
	mtk_ddp_write_relaxed(comp, bg_top << 16 | bg_bot,
			DP_BG_VCNTL, handle);
#if 0
	mtk_ddp_write_mask(comp, DSC_UFOE_SEL,
			DISP_REG_DSC_CON, DSC_UFOE_SEL, handle);
	mtk_ddp_write_relaxed(comp,
			(slice_group_width - 1) << 16 | slice_width,
			DISP_REG_DSC_SLICE_W, handle);
	mtk_ddp_write(comp, 0x20000c03, DISP_REG_DSC_PPS6, handle);
#endif
	DDPMSG("%s config done\n",
			mtk_dump_comp_str(comp));

	dp_intf->enable = true;
}

int mtk_dp_intf_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	DDPDUMP("== %s REGS ==\n", mtk_dump_comp_str(comp));
	DDPDUMP("(0x0000) DP_EN                 =0x%x\n",
			readl(baddr + DP_EN));
	DDPDUMP("(0x0004) DP_RST                =0x%x\n",
			readl(baddr + DP_RST));
	DDPDUMP("(0x0008) DP_INTEN              =0x%x\n",
			readl(baddr + DP_INTEN));
	DDPDUMP("(0x000C) DP_INTSTA             =0x%x\n",
			readl(baddr + DP_INTSTA));
	DDPDUMP("(0x0010) DP_CON                =0x%x\n",
			readl(baddr + DP_CON));
	DDPDUMP("(0x0014) DP_OUTPUT_SETTING     =0x%x\n",
			readl(baddr + DP_OUTPUT_SETTING));
	DDPDUMP("(0x0018) DP_SIZE               =0x%x\n",
			readl(baddr + DP_SIZE));
	DDPDUMP("(0x0020) DP_TGEN_HWIDTH        =0x%x\n",
			readl(baddr + DP_TGEN_HWIDTH));
	DDPDUMP("(0x0024) DP_TGEN_HPORCH        =0x%x\n",
			readl(baddr + DP_TGEN_HPORCH));
	DDPDUMP("(0x0028) DP_TGEN_VWIDTH        =0x%x\n",
			readl(baddr + DP_TGEN_VWIDTH));
	DDPDUMP("(0x002C) DP_TGEN_VPORCH        =0x%x\n",
			readl(baddr + DP_TGEN_VPORCH));
	DDPDUMP("(0x0030) DP_BG_HCNTL           =0x%x\n",
			readl(baddr + DP_BG_HCNTL));
	DDPDUMP("(0x0034) DP_BG_VCNTL           =0x%x\n",
			readl(baddr + DP_BG_VCNTL));
	DDPDUMP("(0x0038) DP_BG_COLOR           =0x%x\n",
			readl(baddr + DP_BG_COLOR));
	DDPDUMP("(0x003C) DP_FIFO_CTL           =0x%x\n",
			readl(baddr + DP_FIFO_CTL));
	DDPDUMP("(0x0040) DP_STATUS             =0x%x\n",
			readl(baddr + DP_STATUS));
	DDPDUMP("(0x004C) DP_DCM                =0x%x\n",
			readl(baddr + DP_DCM));
	DDPDUMP("(0x0050) DP_DUMMY              =0x%x\n",
			readl(baddr + DP_DUMMY));
	DDPDUMP("(0x0068) DP_TGEN_VWIDTH_LEVEN  =0x%x\n",
			readl(baddr + DP_TGEN_VWIDTH_LEVEN));
	DDPDUMP("(0x006C) DP_TGEN_VPORCH_LEVEN  =0x%x\n",
			readl(baddr + DP_TGEN_VPORCH_LEVEN));
	DDPDUMP("(0x0070) DP_TGEN_VWIDTH_RODD   =0x%x\n",
			readl(baddr + DP_TGEN_VWIDTH_RODD));
	DDPDUMP("(0x0074) DP_TGEN_VPORCH_RODD   =0x%x\n",
			readl(baddr + DP_TGEN_VPORCH_RODD));
	DDPDUMP("(0x0078) DP_TGEN_VWIDTH_REVEN  =0x%x\n",
			readl(baddr + DP_TGEN_VWIDTH_REVEN));
	DDPDUMP("(0x007C) DP_TGEN_VPORCH_REVEN  =0x%x\n",
			readl(baddr + DP_TGEN_VPORCH_REVEN));
	DDPDUMP("(0x00E0) DP_MUTEX_VSYNC_SETTING=0x%x\n",
			readl(baddr + DP_MUTEX_VSYNC_SETTING));
	DDPDUMP("(0x00E4) DP_SHEUDO_REG_UPDATE  =0x%x\n",
			readl(baddr + DP_SHEUDO_REG_UPDATE));
	DDPDUMP("(0x00E8) DP_INTERNAL_DCM_DIS   =0x%x\n",
			readl(baddr + DP_INTERNAL_DCM_DIS));
	DDPDUMP("(0x00F0) DP_TARGET_LINE        =0x%x\n",
			readl(baddr + DP_TARGET_LINE));
	DDPDUMP("(0x0100) DP_CHKSUM_EN          =0x%x\n",
			readl(baddr + DP_CHKSUM_EN));
	DDPDUMP("(0x0104) DP_CHKSUM0            =0x%x\n",
			readl(baddr + DP_CHKSUM0));
	DDPDUMP("(0x0108) DP_CHKSUM1            =0x%x\n",
			readl(baddr + DP_CHKSUM1));
	DDPDUMP("(0x010C) DP_CHKSUM2            =0x%x\n",
			readl(baddr + DP_CHKSUM2));
	DDPDUMP("(0x0110) DP_CHKSUM3            =0x%x\n",
			readl(baddr + DP_CHKSUM3));
	DDPDUMP("(0x0114) DP_CHKSUM4            =0x%x\n",
			readl(baddr + DP_CHKSUM4));
	DDPDUMP("(0x0118) DP_CHKSUM5            =0x%x\n",
			readl(baddr + DP_CHKSUM5));
	DDPDUMP("(0x011C) DP_CHKSUM6            =0x%x\n",
			readl(baddr + DP_CHKSUM6));
	DDPDUMP("(0x0120) DP_CHKSUM7            =0x%x\n",
			readl(baddr + DP_CHKSUM7));
	DDPDUMP("(0x0F00) DP_PATTERN_CTRL0      =0x%x\n",
			readl(baddr + DP_PATTERN_CTRL0));
	DDPDUMP("(0x0F04) DP_PATTERN_CTRL1      =0x%x\n",
			readl(baddr + DP_PATTERN_CTRL1));

	return 0;
}

int mtk_dp_intf_analysis(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	DDPDUMP("== %s ANALYSIS ==\n", mtk_dump_comp_str(comp));
	DDPDUMP("en=%d, rst_sel=%d, rst=%d, bg_en=%d, intl_en=%d\n",
		 DISP_REG_GET_FIELD(CON_FLD_DP_EN,
				baddr + DP_EN),
		 DISP_REG_GET_FIELD(CON_FLD_DP_RST_SEL,
				baddr + DP_RST),
		 DISP_REG_GET_FIELD(CON_FLD_DP_RST,
				baddr + DP_RST),
		 DISP_REG_GET_FIELD(CON_FLD_DP_BG_EN,
				baddr + DP_CON),
		 DISP_REG_GET_FIELD(CON_FLD_DP_INTL_EN,
				baddr + DP_CON));
	DDPDUMP("== End %s ANALYSIS ==\n", mtk_dump_comp_str(comp));

	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_dp_intf_funcs = {
	.config = mtk_dp_intf_config,
	.start = mtk_dp_intf_start,
	.stop = mtk_dp_intf_stop,
	.prepare = mtk_dp_intf_prepare,
	.unprepare = mtk_dp_intf_unprepare,
};

static int mtk_dp_intf_bind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_dp_intf *dp_intf = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	DDPINFO("%s\n", __func__);
	ret = mtk_ddp_comp_register(drm_dev, &dp_intf->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	DDPINFO("%s-\n", __func__);
	return 0;
}

static void mtk_dp_intf_unbind(struct device *dev, struct device *master,
				 void *data)
{
	struct mtk_dp_intf *dp_intf = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_dp_intf_destroy_conn_enc(dp_intf);
	mtk_ddp_comp_unregister(drm_dev, &dp_intf->ddp_comp);
}

static const struct component_ops mtk_dp_intf_component_ops = {
	.bind = mtk_dp_intf_bind,
	.unbind = mtk_dp_intf_unbind,
};

static int mtk_dp_intf_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_dp_intf *dp_intf;
	enum mtk_ddp_comp_id comp_id;
	const struct of_device_id *of_id;
	struct resource *mem;
	int ret;
	struct device_node *node;

	DDPMSG("%s+\n", __func__);
	dp_intf = devm_kzalloc(dev, sizeof(*dp_intf), GFP_KERNEL);
	if (!dp_intf)
		return -ENOMEM;
	dp_intf->dev = dev;

	of_id = of_match_device(mtk_dp_intf_driver_dt_match, &pdev->dev);
	dp_intf->driver_data = (struct mtk_dp_intf_driver_data *)of_id->data;
	DDPMSG("%s:%d\n", __func__, __LINE__);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dp_intf->regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(dp_intf->regs)) {
		ret = PTR_ERR(dp_intf->regs);
		dev_err(dev, "Failed to ioremap mem resource: %d\n", ret);
		return ret;
	}

	/* Get dp intf clk
	 * Input pixel clock(hf_fmm_ck) frequency needs to be > hf_fdp_ck * 4
	 * Otherwise FIFO will underflow
	 */
	dp_intf->hf_fmm_ck = devm_clk_get(dev, "hf_fmm_ck");
	if (IS_ERR(dp_intf->hf_fmm_ck)) {
		ret = PTR_ERR(dp_intf->hf_fmm_ck);
		dev_err(dev, "Failed to get hf_fmm_ck clock: %d\n", ret);
		return ret;
	}
	dp_intf->hf_fdp_ck = devm_clk_get(dev, "hf_fdp_ck");
	if (IS_ERR(dp_intf->hf_fdp_ck)) {
		ret = PTR_ERR(dp_intf->hf_fdp_ck);
		dev_err(dev, "Failed to get hf_fdp_ck clock: %d\n", ret);
		return ret;
	}

	dp_intf->pclk = devm_clk_get(dev, "MUX_DP");
	///dp_intf->pclk_src[0] = devm_clk_get(dev, "TVDPLL_D2");
	dp_intf->pclk_src[1] = devm_clk_get(dev, "TVDPLL_D2");
	dp_intf->pclk_src[2] = devm_clk_get(dev, "TVDPLL_D4");
	dp_intf->pclk_src[3] = devm_clk_get(dev, "TVDPLL_D8");
	dp_intf->pclk_src[4] = devm_clk_get(dev, "TVDPLL_D16");

	if (clk_apmixed_base == NULL) {
		node = of_find_compatible_node(NULL, NULL, "mediatek,apmixed");
		if (!node)
			pr_info("[CLK_APMIXED] find node failed\n");
		clk_apmixed_base = of_iomap(node, 0);
		if (clk_apmixed_base == NULL)
			pr_info("[CLK_APMIXED] io map failed\n");

		pr_info("clk_apmixed_base clk_apmixed_base 0x%lx!!!\n",
			clk_apmixed_base);
	}

	if (IS_ERR(dp_intf->pclk)
		|| IS_ERR(dp_intf->pclk_src[0])
		|| IS_ERR(dp_intf->pclk_src[1])
		|| IS_ERR(dp_intf->pclk_src[2])
		|| IS_ERR(dp_intf->pclk_src[3]))
		dev_err(dev, "Failed to get pclk andr src clock !!!\n");

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DP_INTF);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	DDPMSG("%s:%d\n", __func__, __LINE__);
	ret = mtk_ddp_comp_init(dev, dev->of_node, &dp_intf->ddp_comp, comp_id,
				&mtk_dp_intf_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	DDPMSG("%s:%d\n", __func__, __LINE__);
	/* Get dp intf irq num and request irq */
	dp_intf->irq = platform_get_irq(pdev, 0);
	dp_intf->res = SINK_MAX;
	if (dp_intf->irq <= 0) {
		dev_err(dev, "Failed to get irq: %d\n", dp_intf->irq);
		return -EINVAL;
	}

	irq_set_status_flags(dp_intf->irq, IRQ_TYPE_LEVEL_HIGH);
	ret = devm_request_irq(
		&pdev->dev, dp_intf->irq, dp_intf->driver_data->irq_handler,
		IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(&pdev->dev), dp_intf);
	if (ret) {
		dev_err(&pdev->dev, "failed to request mediatek dp intf irq\n");
		ret = -EPROBE_DEFER;
		return ret;
	}

	DDPMSG("%s:%d\n", __func__, __LINE__);

	platform_set_drvdata(pdev, dp_intf);
	pm_runtime_enable(dev);

	ret = component_add(dev, &mtk_dp_intf_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		pm_runtime_disable(dev);
	}

	g_dp_intf = dp_intf;
	DDPMSG("%s-\n", __func__);
	return ret;
}

static int mtk_dp_intf_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mtk_dp_intf_component_ops);

	pm_runtime_disable(&pdev->dev);
	return 0;
}

static s32 mtk_dp_intf_poll_for_idle(struct mtk_dp_intf *dp_intf,
	struct cmdq_pkt *handle)
{
	return 0;
}

static irqreturn_t mtk_dp_intf_irq_status(int irq, void *dev_id)
{
	struct mtk_dp_intf *dp_intf = dev_id;
	u32 status = 0;
	struct mtk_drm_crtc *mtk_crtc;

	status = readl(dp_intf->regs + DP_INTSTA);

	DRM_MMP_MARK(dp_intf0, status, 0);

	status &= 0xf;
	if (status) {
		mtk_dp_intf_mask(dp_intf, DP_INTSTA, status, 0);
		if (status & INTSTA_VSYNC) {
			mtk_crtc = dp_intf->ddp_comp.mtk_crtc;
			mtk_crtc_vblank_irq(&mtk_crtc->base);
			irq_intsa++;

		}

		if (status & INTSTA_VDE)
			irq_vdesa++;

		if (status & INTSTA_UNDERFLOW)
			irq_underflowsa++;

		if (status & INTSTA_TARGET_LINE)
			irq_tl++;
	}

	if (irq_intsa == 3)
		mtk_dp_video_trigger(video_unmute<<16 | dp_intf->res);

	if (((irq_intsa+1)%200 == 0)
		|| ((irq_vdesa+1)%200 == 0)
		|| ((irq_underflowsa+1)%200 == 0)
		|| ((irq_tl+1)%200 == 0))
		pr_info("dp_intf irq %d - %d - %d - %d! 0x%x\n", irq_intsa,
				irq_vdesa, irq_underflowsa, irq_tl, status);

	return IRQ_HANDLED;
}


static const struct mtk_dp_intf_driver_data mt6885_dp_intf_driver_data = {
	.reg_cmdq_ofs = 0x200,
	.poll_for_idle = mtk_dp_intf_poll_for_idle,
	.irq_handler = mtk_dp_intf_irq_status,
};

static const struct of_device_id mtk_dp_intf_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6885-dp-intf",
	.data = &mt6885_dp_intf_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_dp_intf_driver_dt_match);

struct platform_driver mtk_dp_intf_driver = {
	.probe = mtk_dp_intf_probe,
	.remove = mtk_dp_intf_remove,
	.driver = {
		.name = "mediatek-dp-intf",
		.owner = THIS_MODULE,
		.of_match_table = mtk_dp_intf_driver_dt_match,
	},
};
