// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "apu_top.h"
#include "mt6983_apupwr.h"
#include "mt6983_apupwr_prot.h"


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
	unsigned int reg_data = 0x0;
	unsigned int reg_offset = 0x0;

	for (i = 0 ; i < CLUSTER_NUM ; i++) {

		memset(&opp_limit_tbl[i], -1 // 0xffff_ffff means no limit
				, sizeof(struct device_opp_limit));

		opp_limit_tbl[i].dev_opp_lmt.vpu_max = vpu_max & 0xf;
		opp_limit_tbl[i].dev_opp_lmt.vpu_min = vpu_min & 0xf;
		opp_limit_tbl[i].dev_opp_lmt.dla_max = dla_max & 0xf;
		opp_limit_tbl[i].dev_opp_lmt.dla_min = dla_min & 0xf;

		if (!(vpu_max == vpu_min == dla_max == dla_min == -1))
			opp_limit_tbl[i].dev_opp_lmt.lmt_type = (int8_t)type;

		reg_data = (vpu_max | (vpu_min << 4) |
			(dla_max << 8) | (dla_min << 12) |
			(type << 16));

		reg_offset = opp_limit_tbl[i].opp_lmt_reg;
		apu_writel(reg_data, g_reg_base + reg_offset);
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

int aputop_opp_limiter_init(void __iomem *reg_base)
{
	int i;

	g_reg_base = reg_base;

	for (i = 0 ; i < CLUSTER_NUM ; i++) {
		memset(&opp_limit_tbl[i].dev_opp_lmt, -1,
				sizeof(struct device_opp_limit));
	}

	return 0;
}
