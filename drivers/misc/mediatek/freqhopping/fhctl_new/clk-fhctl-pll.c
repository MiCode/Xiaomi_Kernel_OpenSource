// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Yu-Chang Wang <Yu-Chang.Wang@mediatek.com>
 */
#include <linux/device.h>
#include <linux/module.h>
#include "clk-fhctl-pll.h"
#include "clk-fhctl-util.h"

#define REG_ADDR(base, x) (void __iomem *)((unsigned long)base + (x))

struct match {
	char *compatible;
	struct fh_pll_domain **domain_list;
};

static int init_v1(struct fh_pll_domain *d,
		void __iomem *fhctl_base,
		void __iomem *apmixed_base)
{
	struct fh_pll_data *data;
	struct fh_pll_offset *offset;
	struct fh_pll_regs *regs;
	char *name;

	name = d->name;
	data = d->data;
	offset = d->offset;
	regs = d->regs;

	if (regs->reg_hp_en) {
		FHDBG("domain<%s> inited\n", name);
		return 0;
	}

	FHDBG("init domain<%s>\n", name);
	while (data->dds_mask != 0) {
		int regs_offset;

		/* fhctl common part */
		regs->reg_hp_en = REG_ADDR(fhctl_base,
				offset->offset_hp_en);
		regs->reg_clk_con = REG_ADDR(fhctl_base,
				offset->offset_clk_con);
		regs->reg_rst_con = REG_ADDR(fhctl_base,
				offset->offset_rst_con);
		regs->reg_slope0 = REG_ADDR(fhctl_base,
				offset->offset_slope0);
		regs->reg_slope1 = REG_ADDR(fhctl_base,
				offset->offset_slope1);

		/* fhctl pll part */
		regs_offset = offset->offset_fhctl + offset->offset_cfg;
		regs->reg_cfg = REG_ADDR(fhctl_base, regs_offset);
		regs->reg_updnlmt = REG_ADDR(regs->reg_cfg,
				offset->offset_updnlmt);
		regs->reg_dds = REG_ADDR(regs->reg_cfg,
				offset->offset_dds);
		regs->reg_dvfs = REG_ADDR(regs->reg_cfg,
				offset->offset_dvfs);
		regs->reg_mon = REG_ADDR(regs->reg_cfg,
				offset->offset_mon);

		/* apmixed part */
		regs->reg_con_pcw = REG_ADDR(apmixed_base,
				offset->offset_con_pcw);

		FHDBG("pll<%s>, dds_mask<%d>, data<%x> offset<%x> regs<%x>\n",
				data->name, data->dds_mask, data, offset, regs);
		data++;
		offset++;
		regs++;
	}

	return 0;
}

/* platform data begin */
/* 6853 begin */
#define SIZE_6853_TOP (sizeof(mt6853_top_data)\
	/sizeof(struct fh_pll_data))
#define DATA_6853_TOP(_name) {				\
		.name = _name,						\
		.dds_mask = GENMASK(21, 0),			\
		.slope0_value = 0x6003c97,			\
		.slope1_value = 0x6003c97,			\
		.sfstrx_en = BIT(2),				\
		.frddsx_en = BIT(1),				\
		.fhctlx_en = BIT(0),				\
		.tgl_org = BIT(31),					\
		.dvfs_tri = BIT(31),				\
		.pcwchg = BIT(31),					\
		.dt_val = 0x0,						\
		.df_val = 0x9,						\
		.updnlmt_shft = 16,					\
		.msk_frddsx_dys = GENMASK(23, 20),	\
		.msk_frddsx_dts = GENMASK(19, 16),	\
	}
#define OFFSET_6853_TOP(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_hp_en = 0x0,				\
		.offset_clk_con = 0x8,				\
		.offset_rst_con = 0xc,				\
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x14,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}
static struct fh_pll_data mt6853_top_data[] = {
	DATA_6853_TOP("armpll_ll"),
	DATA_6853_TOP("armpll_bl0"),
	DATA_6853_TOP("armpll_bl1"),
	DATA_6853_TOP("armpll_bl2"),
	DATA_6853_TOP("npupll"),
	DATA_6853_TOP("ccipll"),
	DATA_6853_TOP("mfgpll"),
	DATA_6853_TOP("mempll"),
	DATA_6853_TOP("mpll"),
	DATA_6853_TOP("mmpll"),
	DATA_6853_TOP("mainpll"),
	DATA_6853_TOP("msdcpll"),
	DATA_6853_TOP("adsppll"),
	DATA_6853_TOP("apupll"),
	DATA_6853_TOP("tvdpll"),
	{}
};
static struct fh_pll_offset mt6853_top_offset[SIZE_6853_TOP] = {
	OFFSET_6853_TOP(0x003C, 0x020C),
	OFFSET_6853_TOP(0x0050, 0x021C),
	OFFSET_6853_TOP(0x0064, 0x022C),
	OFFSET_6853_TOP(0x0078, 0x023C),
	OFFSET_6853_TOP(0x008C, 0x03B8),
	OFFSET_6853_TOP(0x00A0, 0x025C),
	OFFSET_6853_TOP(0x00B4, 0x026C),
	OFFSET_6853_TOP(0x00C8, 0xffff),
	OFFSET_6853_TOP(0x00DC, 0x0394),
	OFFSET_6853_TOP(0x00F0, 0x0364),
	OFFSET_6853_TOP(0x0104, 0x0344),
	OFFSET_6853_TOP(0x0118, 0x0354),
	OFFSET_6853_TOP(0x012c, 0x0374),
	OFFSET_6853_TOP(0x0140, 0xffff),
	OFFSET_6853_TOP(0x0154, 0x0384),
	{}
};
static struct fh_pll_regs mt6853_top_regs[SIZE_6853_TOP];
static struct fh_pll_domain mt6853_top = {
	.name = "top",
	.data = (struct fh_pll_data *)&mt6853_top_data,
	.offset = (struct fh_pll_offset *)&mt6853_top_offset,
	.regs = (struct fh_pll_regs *)&mt6853_top_regs,
	.init = &init_v1,
};
static struct fh_pll_domain *mt6853_domain[] = {
	&mt6853_top,
	NULL
};
static struct match mt6853_match = {
	.compatible = "mediatek,mt6853-fhctl",
	.domain_list = (struct fh_pll_domain **)mt6853_domain,
};
/* 6853 end */

/* 6877 begin */
#define SIZE_6877_TOP (sizeof(mt6877_top_data)\
	/sizeof(struct fh_pll_data))
#define DATA_6877_TOP(_name) {				\
		.name = _name,						\
		.dds_mask = GENMASK(21, 0),			\
		.slope0_value = 0x6003c97,			\
		.slope1_value = 0x6003c97,			\
		.sfstrx_en = BIT(2),				\
		.frddsx_en = BIT(1),				\
		.fhctlx_en = BIT(0),				\
		.tgl_org = BIT(31),					\
		.dvfs_tri = BIT(31),				\
		.pcwchg = BIT(31),					\
		.dt_val = 0x0,						\
		.df_val = 0x9,						\
		.updnlmt_shft = 16,					\
		.msk_frddsx_dys = GENMASK(23, 20),	\
		.msk_frddsx_dts = GENMASK(19, 16),	\
	}
#define OFFSET_6877_TOP(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_hp_en = 0x0,				\
		.offset_clk_con = 0x8,				\
		.offset_rst_con = 0xc,				\
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x14,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}
static struct fh_pll_data mt6877_top_data[] = {
	DATA_6877_TOP("armpll_ll"),
	DATA_6877_TOP("armpll_bl0"),
	DATA_6877_TOP("armpll_b"),
	DATA_6877_TOP("ccipll"),
	DATA_6877_TOP("mempll"),
	DATA_6877_TOP("emipll"),
	DATA_6877_TOP("mpll"),
	DATA_6877_TOP("mmpll"),
	DATA_6877_TOP("mainpll"),
	DATA_6877_TOP("msdcpll"),
	DATA_6877_TOP("adsppll"),
	DATA_6877_TOP("imgpll"),
	DATA_6877_TOP("tvdpll"),
	{}
};
static struct fh_pll_offset mt6877_top_offset[] = {
	OFFSET_6877_TOP(0x003C, 0x020C),  // FHCTL0_CFG, ARMPLL_LL_CON1
	OFFSET_6877_TOP(0x0050, 0x021C),  // FHCTL1_CFG, ARMPLL_BL_CON1
	OFFSET_6877_TOP(0x0064, 0x022C),  // FHCTL2_CFG, ARMPLL_B_CON1
	OFFSET_6877_TOP(0x0078, 0x023C),  // FHCTL3_CFG, CCIPLL_CON1
	OFFSET_6877_TOP(0x008C, 0xffff),  // FHCTL4_CFG,
	OFFSET_6877_TOP(0x00A0, 0x03B4),  // FHCTL5_CFG, EMIPLL_CON1
	OFFSET_6877_TOP(0x00B4, 0x0394),  // FHCTL6_CFG, MPLL_CON1
	OFFSET_6877_TOP(0x00C8, 0x03A4),  // FHCTL7_CFG, MMPLL_CON1
	OFFSET_6877_TOP(0x00DC, 0x0354),  // FHCTL8_CFG, MAINPLL_CON1
	OFFSET_6877_TOP(0x00F0, 0x0364),  // FHCTL9_CFG, MSDCPLL_CON1
	OFFSET_6877_TOP(0x0104, 0x0384),  // FHCTL10_CFG, ADSPPLL_CON1
	OFFSET_6877_TOP(0x0118, 0x0374),  // FHCTL11_CFG, IMGPLL_CON1
	OFFSET_6877_TOP(0x012c, 0x024c),  // FHCTL12_CFG, TVDPLL_CON1
	{}
};

#define SIZE_6877_GPU (sizeof(mt6877_gpu_data)\
	/sizeof(struct fh_pll_data))

static struct fh_pll_data mt6877_gpu_data[] = {
	DATA_6877_TOP("mfgpll1"),
	DATA_6877_TOP("mfgpll2"),
	DATA_6877_TOP("mfgpll3"),
	DATA_6877_TOP("mfgpll4"),
	{}
};
static struct fh_pll_offset mt6877_gpu_offset[] = {
	OFFSET_6877_TOP(0x003C, 0x000C),  // PLL4H_FHCTL0_CFG, PLL4H_PLL1_CON1
	OFFSET_6877_TOP(0x0050, 0x001C),  // PLL4HPLL_FHCTL1_CFG, PLL4H_PLL2_CON1
	OFFSET_6877_TOP(0x0064, 0x002C),  // PLL4HPLL_FHCTL2_CFG, PLL4H_PLL3_CON1
	OFFSET_6877_TOP(0x0078, 0x003C),  // PLL4HPLL_FHCTL3_CFG, PLL4H_PLL4_CON1
	{}
};

#define SIZE_6877_APU (sizeof(mt6877_apu_data)\
	/sizeof(struct fh_pll_data))

static struct fh_pll_data mt6877_apu_data[] = {
	DATA_6877_TOP("apupll"),
	DATA_6877_TOP("npupll"),
	DATA_6877_TOP("apupll1"),
	DATA_6877_TOP("apupll2"),
	{}
};
static struct fh_pll_offset mt6877_apu_offset[] = {
	OFFSET_6877_TOP(0x003C, 0x000C),  // PLL4HPLL_FHCTL0_CFG, PLL4H_PLL1_CON1
	OFFSET_6877_TOP(0x0050, 0x001C),  // PLL4HPLL_FHCTL1_CFG, PLL4H_PLL2_CON1
	OFFSET_6877_TOP(0x0064, 0x002C),  // PLL4HPLL_FHCTL2_CFG, PLL4H_PLL3_CON1
	OFFSET_6877_TOP(0x0078, 0x003C),  // PLL4HPLL_FHCTL3_CFG, PLL4H_PLL4_CON1
	{}
};
static struct fh_pll_regs mt6877_top_regs[SIZE_6877_TOP];
static struct fh_pll_regs mt6877_gpu_regs[SIZE_6877_GPU];
static struct fh_pll_regs mt6877_apu_regs[SIZE_6877_APU];
static struct fh_pll_domain mt6877_top = {
	.name = "top",
	.data = (struct fh_pll_data *)&mt6877_top_data,
	.offset = (struct fh_pll_offset *)&mt6877_top_offset,
	.regs = (struct fh_pll_regs *)&mt6877_top_regs,
	.init = &init_v1,
};
static struct fh_pll_domain mt6877_gpu = {
	.name = "gpu",
	.data = (struct fh_pll_data *)&mt6877_gpu_data,
	.offset = (struct fh_pll_offset *)&mt6877_gpu_offset,
	.regs = (struct fh_pll_regs *)&mt6877_gpu_regs,
	.init = &init_v1,
};
static struct fh_pll_domain mt6877_apu = {
	.name = "apu",
	.data = (struct fh_pll_data *)&mt6877_apu_data,
	.offset = (struct fh_pll_offset *)&mt6877_apu_offset,
	.regs = (struct fh_pll_regs *)&mt6877_apu_regs,
	.init = &init_v1,
};
static struct fh_pll_domain *mt6877_domain[] = {
	&mt6877_top,
	&mt6877_gpu,
	&mt6877_apu,
	NULL,
};
static struct match mt6877_match = {
	.compatible = "mediatek,mt6877-fhctl",
	.domain_list = (struct fh_pll_domain **)mt6877_domain,
};
/* 6877 end */

/* platform data begin */
/* 6873 begin */
#define SIZE_6873_TOP (sizeof(mt6873_top_data)\
	/sizeof(struct fh_pll_data))
#define DATA_6873_TOP(_name) {				\
		.name = _name,						\
		.dds_mask = GENMASK(21, 0),			\
		.slope0_value = 0x6003c97,			\
		.slope1_value = 0x6003c97,			\
		.sfstrx_en = BIT(2),				\
		.frddsx_en = BIT(1),				\
		.fhctlx_en = BIT(0),				\
		.tgl_org = BIT(31),					\
		.dvfs_tri = BIT(31),				\
		.pcwchg = BIT(31),					\
		.dt_val = 0x0,						\
		.df_val = 0x9,						\
		.updnlmt_shft = 16,					\
		.msk_frddsx_dys = GENMASK(23, 20),	\
		.msk_frddsx_dts = GENMASK(19, 16),	\
	}
#define OFFSET_6873_TOP(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_hp_en = 0x0,				\
		.offset_clk_con = 0x8,				\
		.offset_rst_con = 0xc,				\
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x14,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}
static struct fh_pll_data mt6873_top_data[] = {
	DATA_6873_TOP("armpll_ll"),
	DATA_6873_TOP("armpll_bl0"),
	DATA_6873_TOP("armpll_bl1"),
	DATA_6873_TOP("armpll_bl2"),
	DATA_6873_TOP("npupll"),
	DATA_6873_TOP("ccipll"),
	DATA_6873_TOP("mfgpll"),
	DATA_6873_TOP("mempll"),
	DATA_6873_TOP("mpll"),
	DATA_6873_TOP("mmpll"),
	DATA_6873_TOP("mainpll"),
	DATA_6873_TOP("msdcpll"),
	DATA_6873_TOP("adsppll"),
	DATA_6873_TOP("apupll"),
	DATA_6873_TOP("tvdpll"),
	{}
};
static struct fh_pll_offset mt6873_top_offset[SIZE_6873_TOP] = {
	OFFSET_6873_TOP(0x003C, 0x020C),
	OFFSET_6873_TOP(0x0050, 0x021C),
	OFFSET_6873_TOP(0x0064, 0x022C),
	OFFSET_6873_TOP(0x0078, 0x023C),
	OFFSET_6873_TOP(0x008C, 0x03B8),
	OFFSET_6873_TOP(0x00A0, 0x025C),
	OFFSET_6873_TOP(0x00B4, 0x026C),
	OFFSET_6873_TOP(0x00C8, 0xffff),
	OFFSET_6873_TOP(0x00DC, 0x0394),
	OFFSET_6873_TOP(0x00F0, 0x0364),
	OFFSET_6873_TOP(0x0104, 0x0344),
	OFFSET_6873_TOP(0x0118, 0x0354),
	OFFSET_6873_TOP(0x012c, 0x0374),
	OFFSET_6873_TOP(0x0140, 0x03A4),
	OFFSET_6873_TOP(0x0154, 0x0384),
	{}
};
static struct fh_pll_regs mt6873_top_regs[SIZE_6873_TOP];
static struct fh_pll_domain mt6873_top = {
	.name = "top",
	.data = (struct fh_pll_data *)&mt6873_top_data,
	.offset = (struct fh_pll_offset *)&mt6873_top_offset,
	.regs = (struct fh_pll_regs *)&mt6873_top_regs,
	.init = &init_v1,
};
static struct fh_pll_domain *mt6873_domain[] = {
	&mt6873_top,
	NULL
};
static struct match mt6873_match = {
	.compatible = "mediatek,mt6873-fhctl",
	.domain_list = (struct fh_pll_domain **)mt6873_domain,
};
/* 6873 end */

/* platform data begin */
/* 6885 begin */
#define SIZE_6885_TOP (sizeof(mt6885_top_data)\
	/sizeof(struct fh_pll_data))
#define DATA_6885_TOP(_name) {				\
		.name = _name,						\
		.dds_mask = GENMASK(21, 0),			\
		.slope0_value = 0x6003c97,			\
		.slope1_value = 0x6003c97,			\
		.sfstrx_en = BIT(2),				\
		.frddsx_en = BIT(1),				\
		.fhctlx_en = BIT(0),				\
		.tgl_org = BIT(31),					\
		.dvfs_tri = BIT(31),				\
		.pcwchg = BIT(31),					\
		.dt_val = 0x0,						\
		.df_val = 0x9,						\
		.updnlmt_shft = 16,					\
		.msk_frddsx_dys = GENMASK(23, 20),	\
		.msk_frddsx_dts = GENMASK(19, 16),	\
	}
#define OFFSET_6885_TOP(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_hp_en = 0x0,				\
		.offset_clk_con = 0x8,				\
		.offset_rst_con = 0xc,				\
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x14,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}
static struct fh_pll_data mt6885_top_data[] = {
	DATA_6885_TOP("armpll_ll"),
	DATA_6885_TOP("armpll_bl0"),
	DATA_6885_TOP("armpll_bl1"),
	DATA_6885_TOP("armpll_bl2"),
	DATA_6885_TOP("armpll_bl3"),
	DATA_6885_TOP("ccipll"),
	DATA_6885_TOP("mfgpll"),
	DATA_6885_TOP("mempll"),
	DATA_6885_TOP("mpll"),
	DATA_6885_TOP("mmpll"),
	DATA_6885_TOP("mainpll"),
	DATA_6885_TOP("msdcpll"),
	DATA_6885_TOP("adsppll"),
	DATA_6885_TOP("apupll"),
	DATA_6885_TOP("tvdpll"),
	{}
};
static struct fh_pll_offset mt6885_top_offset[SIZE_6885_TOP] = {
	OFFSET_6885_TOP(0x003C, 0x020C),
	OFFSET_6885_TOP(0x0050, 0x021C),
	OFFSET_6885_TOP(0x0064, 0x022C),
	OFFSET_6885_TOP(0x0078, 0x023C),
	OFFSET_6885_TOP(0x008C, 0x024C),
	OFFSET_6885_TOP(0x00A0, 0x025C),
	OFFSET_6885_TOP(0x00B4, 0x026C),
	OFFSET_6885_TOP(0x00C8, 0xffff),
	OFFSET_6885_TOP(0x00DC, 0x0394),
	OFFSET_6885_TOP(0x00F0, 0x0364),
	OFFSET_6885_TOP(0x0104, 0x0344),
	OFFSET_6885_TOP(0x0118, 0x0354),
	OFFSET_6885_TOP(0x012c, 0x0374),
	OFFSET_6885_TOP(0x0140, 0x03A4),
	OFFSET_6885_TOP(0x0154, 0x0384),
	{}
};
static struct fh_pll_regs mt6885_top_regs[SIZE_6885_TOP];
static struct fh_pll_domain mt6885_top = {
	.name = "top",
	.data = (struct fh_pll_data *)&mt6885_top_data,
	.offset = (struct fh_pll_offset *)&mt6885_top_offset,
	.regs = (struct fh_pll_regs *)&mt6885_top_regs,
	.init = &init_v1,
};
static struct fh_pll_domain *mt6885_domain[] = {
	&mt6885_top,
	NULL
};
static struct match mt6885_match = {
	.compatible = "mediatek,mt6885-fhctl",
	.domain_list = (struct fh_pll_domain **)mt6885_domain,
};
/* 6885 end */

static const struct match *matchs[] = {
	&mt6853_match,
	&mt6877_match,
	&mt6873_match,
	&mt6885_match,
	NULL
};

static struct fh_pll_domain **get_list(char *comp)
{
	struct match **match;
	static struct fh_pll_domain **list;

	match = (struct match **)matchs;

	/* name used only if !list */
	if (!list) {
		while (*matchs != NULL) {
			if (strcmp(comp,
						(*match)->compatible) == 0) {
				list = (*match)->domain_list;
				FHDBG("target<%s>\n", comp);
				break;
			}
			match++;
		}
	}
	return list;
}
void init_fh_domain(const char *domain,
		char *comp,
		void __iomem *fhctl_base,
		void __iomem *apmixed_base)
{
	struct fh_pll_domain **list;

	list = get_list(comp);

	while (*list != NULL) {
		if (strcmp(domain,
					(*list)->name) == 0) {
			(*list)->init(*list,
					fhctl_base,
					apmixed_base);
			return;
		}
		list++;
	}
}
struct fh_pll_domain *get_fh_domain(const char *domain)
{
	struct fh_pll_domain **list;

	list = get_list(NULL);

	/* find instance */
	while (*list != NULL) {
		if (strcmp(domain,
					(*list)->name) == 0)
			return *list;
		list++;
	}
	return NULL;
}
