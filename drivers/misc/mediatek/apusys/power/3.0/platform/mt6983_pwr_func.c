// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "apu_top.h"
#include "mt6983_apupwr.h"
#include "mt6983_apupwr_prot.h"

#define LOCAL_DBG	(1)

static void __iomem *g_reg_base;

#define _OPP_LMT_TBL(_opp_lmt_reg) {    \
	.opp_lmt_reg = _opp_lmt_reg,    \
}
static struct cluster_dev_opp_info opp_limit_tbl[CLUSTER_NUM] = {
	_OPP_LMT_TBL(ACX0_LIMIT_OPP_REG),
	_OPP_LMT_TBL(ACX1_LIMIT_OPP_REG),
};

static void _opp_limiter(int vpu_max, int vpu_min, int dla_max, int dla_min,
		enum apu_opp_limit_type type)
{
	int i;
	unsigned int reg_data;
	unsigned int reg_offset;

#if LOCAL_DBG
	pr_info("%s type:%d, %d/%d/%d/%d\n", __func__, type,
			vpu_max, vpu_min, dla_max, dla_min);
#endif
	for (i = 0 ; i < CLUSTER_NUM ; i++) {
		opp_limit_tbl[i].dev_opp_lmt.vpu_max = vpu_max & 0xf;
		opp_limit_tbl[i].dev_opp_lmt.vpu_min = vpu_min & 0xf;
		opp_limit_tbl[i].dev_opp_lmt.dla_max = dla_max & 0xf;
		opp_limit_tbl[i].dev_opp_lmt.dla_min = dla_min & 0xf;
		opp_limit_tbl[i].dev_opp_lmt.lmt_type = type & 0xff;

		reg_data = 0x0;
		reg_data = ((vpu_max & 0xf) |		// [3:0]
			((vpu_min & 0xf) << 4) |	// [7:4]
			((dla_max & 0xf) << 8) |	// [b:8]
			((dla_min & 0xf) << 12) |	// [f:c]
			((type & 0xff) << 16));		// dedicate 1 byte

		reg_offset = opp_limit_tbl[i].opp_lmt_reg;

		apu_writel(reg_data, g_reg_base + reg_offset);
#if LOCAL_DBG
		pr_info("%s cluster%d write:0x%08x, readback:0x%08x\n",
				__func__, i, reg_data,
				apu_readl(g_reg_base + reg_offset));
#endif
	}

}

void aputop_opp_limit(struct aputop_func_param *aputop,
		enum apu_opp_limit_type type)
{
	int vpu_max, vpu_min, dla_max, dla_min;

	vpu_max = aputop->param1;
	vpu_min = aputop->param2;
	dla_max = aputop->param3;
	dla_min = aputop->param4;

	_opp_limiter(vpu_max, vpu_min, dla_max, dla_min, type);
}

void aputop_curr_status(struct aputop_func_param *aputop)
{

}

void aputop_pwr_cfg(struct aputop_func_param *aputop)
{

}

void aputop_pwr_ut(struct aputop_func_param *aputop)
{

}

#if IS_ENABLED(CONFIG_DEBUG_FS)
static int apu_top_dbg_show(struct seq_file *s, void *unused)
{
	pr_info("%s\n", __func__);
	return 0;
}

int mt6983_apu_top_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, apu_top_dbg_show, inode->i_private);
}

ssize_t mt6983_apu_top_dbg_write(
		struct file *flip, const char __user *buffer,
		size_t count, loff_t *f_pos)
{
	pr_info("%s\n", __func__);
	return 0;
}
#endif

int aputop_opp_limiter_init(void __iomem *reg_base)
{
	int i;
	uint32_t reg_offset = 0x0;

	g_reg_base = reg_base;

	for (i = 0 ; i < CLUSTER_NUM ; i++) {
		// 0xffff_ffff means no limit
		memset(&opp_limit_tbl[i].dev_opp_lmt, -1,
				sizeof(struct device_opp_limit));
		reg_offset = opp_limit_tbl[i].opp_lmt_reg;
#if LOCAL_DBG
		pr_info("%s g_reg_base:0x%08x, offset:0x%08x\n",
				__func__, g_reg_base, reg_offset);
#endif
		apu_writel(0xffffffff, g_reg_base + reg_offset);
	}

	return 0;
}
