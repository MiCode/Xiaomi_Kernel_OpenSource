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

		FHDBG("pll<%s>, dds_mask<%d>\n",
				data->name, data->dds_mask);
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
#define OFFSET_6853_TOP(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_hp_en = 0x4,				\
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
	.domain_list = (struct fh_pll_domain **)&mt6853_domain,
};
/* 6853 end */

/* 6739 begin */
#define SIZE_6739_TOP (sizeof(mt6739_top_data)\
	/sizeof(struct fh_pll_data))
#define DATA_6739_TOP(_name) {				\
		.name = _name,						\
		.dds_mask = GENMASK(20, 0),			\
		.slope0_value = 0x6000F4B,			\
		.slope1_value = 0xFF000368,			\
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
static struct fh_pll_data mt6739_top_data[] = {
	DATA_6739_TOP("armpll"),
	DATA_6739_TOP("mainpll"),
	DATA_6739_TOP("msdcpll"),
	DATA_6739_TOP("mfgpll"),
	DATA_6739_TOP("mempll"),
	DATA_6739_TOP("mmpll"),
	{}
};
#define OFFSET_6739_TOP(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_hp_en = 0x0,				\
		.offset_clk_con = 0x4,				\
		.offset_rst_con = 0x8,				\
		.offset_slope0 = 0xc,				\
		.offset_slope1 = 0x10,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}
static struct fh_pll_offset mt6739_top_offset[SIZE_6739_TOP] = {
	OFFSET_6739_TOP(0x0038, 0x0204),
	OFFSET_6739_TOP(0x004C, 0x0224),
	OFFSET_6739_TOP(0x0060, 0x0254),
	OFFSET_6739_TOP(0x0074, 0x0244),
	OFFSET_6739_TOP(0x0088, 0xffff),
	OFFSET_6739_TOP(0x009C, 0x0274),
	{}
};
static struct fh_pll_regs mt6739_top_regs[SIZE_6739_TOP];
static struct fh_pll_domain mt6739_top = {
	.name = "top",
	.data = (struct fh_pll_data *)&mt6739_top_data,
	.offset = (struct fh_pll_offset *)&mt6739_top_offset,
	.regs = (struct fh_pll_regs *)&mt6739_top_regs,
	.init = &init_v1,
};

static struct fh_pll_domain *mt6739_domain[] = {
	&mt6739_top,
	NULL
};
static struct match mt6739_match = {
	.compatible = "mediatek,mt6739-fhctl",
	.domain_list = (struct fh_pll_domain **)&mt6739_domain,
};
/* 6739 end */
/* platform data end */

static const struct match *matchs[] = {
	&mt6853_match,
	&mt6739_match,
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
void init_fh_domain(char *domain,
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
