/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/


#define LOG_TAG "ddp_path"
#include "ddp_log.h"

#include <linux/types.h>
#include "ddp_clkmgr.h"
#include "ddp_reg.h"

#include "ddp_debug.h"
#include "ddp_path.h"
#include "primary_display.h"
#include "ddp_hal.h"
#include "disp_helper.h"
#include "ddp_path.h"
#ifdef CONFIG_MTK_IOMMU_V2
#include <soc/mediatek/smi.h>
#include "ddp_drv.h"
#else
#include "m4u.h"
#endif
#ifdef CONFIG_MTK_SMI_EXT
#include "smi_public.h"
#endif

/* #pragma GCC optimize("O0") */

#define BIT_NUM (8)

struct m_to_b {
	int m;
	int v;
};

struct mout_s {
	int id;
	struct m_to_b out_id_bit_map[BIT_NUM];
	unsigned long *reg;
	unsigned int reg_val;
};

struct sel_s {
	int id;
	int id_bit_map[BIT_NUM];
	unsigned long *reg;
	unsigned int reg_val;
};

unsigned int module_list_scenario[DDP_SCENARIO_MAX][DDP_ENING_NUM] = {
	/* DDP_SCENARIO_PRIMARY_DISP */
	{
		DISP_MODULE_OVL0_2L, DISP_MODULE_RSZ0, DISP_MODULE_OVL0,
		DISP_MODULE_RDMA0, DISP_MODULE_RDMA_VIRTUAL0,
#ifdef DISP_COLOR_ON
		DISP_MODULE_COLOR0,
#endif
		DISP_MODULE_CCORR0, DISP_MODULE_AAL0,
		DISP_MODULE_GAMMA0, DISP_MODULE_POSTMASK, DISP_MODULE_DITHER0,
		DISP_MODULE_PWM0, DISP_MODULE_DSI0,
		-1,
	},

	/* DDP_SCENARIO_PRIMARY_BYPASS_PQ_DISP */
	{
		DISP_MODULE_OVL0_2L, DISP_MODULE_RSZ0, DISP_MODULE_OVL0,
		DISP_MODULE_RDMA0, DISP_MODULE_RDMA_VIRTUAL0,
		DISP_MODULE_PWM0, DISP_MODULE_DSI0,
		-1,
	},

	/* DDP_SCENARIO_PRIMARY_RDMA0_COLOR0_DISP */
	{
		DISP_MODULE_RDMA0, DISP_MODULE_RDMA_VIRTUAL0,
#ifdef DISP_COLOR_ON
		DISP_MODULE_COLOR0,
#endif
		DISP_MODULE_CCORR0, DISP_MODULE_AAL0, DISP_MODULE_GAMMA0,
		DISP_MODULE_POSTMASK,
		DISP_MODULE_DITHER0, DISP_MODULE_PWM0, DISP_MODULE_DSI0,
		-1,
	},

	/* DDP_SCENARIO_PRIMARY_RDMA0_DISP */
	{
		DISP_MODULE_RDMA0, DISP_MODULE_RDMA_VIRTUAL0, DISP_MODULE_PWM0,
		DISP_MODULE_DSI0,
		-1,
	},

	/* DDP_SCENARIO_PRIMARY_OVL_MEMOUT */
	{
		DISP_MODULE_OVL0_2L, DISP_MODULE_RSZ0, DISP_MODULE_OVL0,
		DISP_MODULE_WDMA_VIRTUAL0,
		DISP_MODULE_WDMA_VIRTUAL1, DISP_MODULE_WDMA0,
		-1,
	},

	/* DDP_SCENARIO_PRIMARY_ALL */
	{
		DISP_MODULE_OVL0_2L, DISP_MODULE_RSZ0, DISP_MODULE_OVL0,
		DISP_MODULE_WDMA_VIRTUAL0, DISP_MODULE_WDMA_VIRTUAL1,
		DISP_MODULE_WDMA0, DISP_MODULE_RDMA0, DISP_MODULE_RDMA_VIRTUAL0,
#ifdef DISP_COLOR_ON
		DISP_MODULE_COLOR0,
#endif
		DISP_MODULE_CCORR0, DISP_MODULE_AAL0, DISP_MODULE_GAMMA0,
		DISP_MODULE_POSTMASK,
		DISP_MODULE_DITHER0, DISP_MODULE_PWM0, DISP_MODULE_DSI0,
		-1,
	},

	/* DDP_SCENARIO_SUB_DISP */
	{
		DISP_MODULE_OVL1_2L, DISP_MODULE_RDMA1, DISP_MODULE_DPI,
		-1,
	},

	/* DDP_SCENARIO_SUB_RDMA1_DISP */
	{
		DISP_MODULE_RDMA1, DISP_MODULE_DPI,
		-1,
	},

	/* DDP_SCENARIO_SUB_OVL_MEMOUT */
	{
		DISP_MODULE_OVL1_2L, DISP_MODULE_WDMA_VIRTUAL0,
		DISP_MODULE_WDMA_VIRTUAL1, DISP_MODULE_WDMA0,
		-1,
	},

	/* DDP_SCENARIO_SUB_ALL */
	{
		DISP_MODULE_OVL1_2L, DISP_MODULE_WDMA_VIRTUAL0,
		DISP_MODULE_WDMA_VIRTUAL1, DISP_MODULE_WDMA0,
		DISP_MODULE_RDMA1, DISP_MODULE_DPI,
		-1,
	}
};

/* 1st para is mout's input, 2nd para is mout's output */
static struct mout_s mout_map[] = {
	/* OVL_MOUT */
	[0] = {
		DISP_MODULE_OVL0,
		{
			{DISP_MODULE_RDMA0, 1 << 0},
			{DISP_MODULE_WDMA_VIRTUAL0, 1 << 2},
			{DISP_MODULE_RSZ0, 1 << 4},
			{-1, 0}
		},
		0, 0
	}, /* bit1: mdp_wrot, bit3: mdp_rsz */

	[1] = {
		DISP_MODULE_OVL0_2L,
		{
			{DISP_MODULE_RDMA0, 1 << 0},
			{DISP_MODULE_WDMA_VIRTUAL0, 1 << 2},
			{DISP_MODULE_RSZ0, 1 << 4},
			{-1, 0}
		},
		0, 0
	},

	[2] = {
		DISP_MODULE_OVL1_2L,
		{
			{DISP_MODULE_RDMA0, 1 << 0},
			{DISP_MODULE_WDMA_VIRTUAL0, 1 << 2},
			{DISP_MODULE_RDMA1, 1 << 4},
			{DISP_MODULE_RSZ0, 1 << 5},
			{-1, 0}
		},
		0, 0
	},

	/* DITHER0_MOUT */
	[3] = {
		DISP_MODULE_DITHER0,
		{
			{DISP_MODULE_DSI0, 1 << 0},
			{DISP_MODULE_DPI, 1 << 1},
			{DISP_MODULE_WDMA_VIRTUAL1, 1 << 2},
			{-1, 0}
		},
		0, 0
	},

	/* RSZ0_MOUT */
	[4] = {
		DISP_MODULE_RSZ0,
		{
			{DISP_MODULE_OVL0, 1 << 0},
			{DISP_MODULE_OVL0_2L, 1 << 1},
			{DISP_MODULE_OVL1_2L, 1 << 2},
			{DISP_MODULE_RDMA0, 1 << 3},
			{DISP_MODULE_WDMA_VIRTUAL0, 1 << 4},
			{DISP_MODULE_RDMA_VIRTUAL0, 1 << 5},
			{-1, 0}
		},
		0, 0
	},

	/* DISP_OVL0_WCG_MOUT_EN */
	[5] = {
		DISP_MODULE_OVL0,
		{
			{DISP_MODULE_OVL0_2L, 1 << 0},
			{DISP_MODULE_OVL1_2L, 1 << 1},
		},
		0, 0
	},

	/* DISP_OVL0_2L_WCG_MOUT_EN */
	[6] = {
		DISP_MODULE_OVL0_2L,
		{
			{DISP_MODULE_OVL0, 1 << 0},
			{DISP_MODULE_OVL1_2L, 1 << 1},
		},
		0, 0
	},

	/* DISP_OVL1_2L_WCG_MOUT_EN */
	[7] = {
		DISP_MODULE_OVL1_2L,
		{
			{DISP_MODULE_OVL0, 1 << 0},
			{DISP_MODULE_OVL0_2L, 1 << 1},
		},
		0, 0
	},
};

static struct sel_s sel_out_map[] = {
	/* RDMA0_RSZ_IN_SOUT */
	[0] = {
		DISP_MODULE_RDMA0,
		{
			DISP_MODULE_RDMA_VIRTUAL0, DISP_MODULE_RSZ0
		},
		0, 0
	},

	/* RDMA0_SOUT */
	[1] = {
		DISP_MODULE_RDMA_VIRTUAL0,
		{
			DISP_MODULE_DSI0, DISP_MODULE_COLOR0,
			DISP_MODULE_CCORR0, DISP_MODULE_DPI,
		},
		0, 0
	},

	/* RDMA1_SOUT */
	[2] = {
		DISP_MODULE_RDMA1,
		{
			DISP_MODULE_DPI, DISP_MODULE_DSI0
		},
		0, 0
	},
};

/* 1st para is sout's output, 2nd para is sout's input */
static struct sel_s sel_in_map[] = {
	/* CCORR_SEL */
	[0] = {
		DISP_MODULE_CCORR0,
		{
			DISP_MODULE_COLOR0, DISP_MODULE_RDMA_VIRTUAL0, -1
		},
		0, 0
	},

	/* RDMA_SEL / PATH0_SEL */
	[1] = {
		DISP_MODULE_RDMA0,
		{
			DISP_MODULE_OVL0, DISP_MODULE_OVL0_2L,
			DISP_MODULE_OVL1_2L, DISP_MODULE_RSZ0, -1
		},
		0, 0
	},

	/* RSZ_OUT_SEL */
	[2] = {
		DISP_MODULE_RDMA_VIRTUAL0,
		{
			DISP_MODULE_RDMA0, DISP_MODULE_RSZ0, -1
		},
		0, 0
	},

	/* RSZ_SEL */
	[3] = {
		DISP_MODULE_RSZ0,
		{
			DISP_MODULE_NUM, DISP_MODULE_NUM, DISP_MODULE_NUM,
			DISP_MODULE_RDMA0, DISP_MODULE_OVL0,
			DISP_MODULE_OVL0_2L, DISP_MODULE_OVL1_2L, -1
		},
		0, 0
	},

	/* WDMA0_PRE_SEL */
	[4] = {
		DISP_MODULE_WDMA_VIRTUAL1,
		{
			DISP_MODULE_WDMA_VIRTUAL0, DISP_MODULE_DITHER0, -1
		},
		0, 0
	},

	/* WDMA0_SEL */
	[5] = {
		DISP_MODULE_WDMA0,
		{
			DISP_MODULE_NUM, DISP_MODULE_NUM,
			DISP_MODULE_NUM, DISP_MODULE_WDMA_VIRTUAL1, -1
		},
		0, 0
	},

	/* DPI_SEL */
	[6] = {
		DISP_MODULE_DPI,
		{
			DISP_MODULE_RDMA_VIRTUAL0, DISP_MODULE_RDMA1,
			DISP_MODULE_DITHER0, -1
		},
		0, 0
	},

	/* DSI_SEL */
	[7] = {
		DISP_MODULE_DSI0,
		{
			DISP_MODULE_DITHER0, DISP_MODULE_RDMA_VIRTUAL0,
			DISP_MODULE_RDMA1, -1
		},
		0, 0
	},

	/* OVL_TO_WDMA_SEL */
	[8] = {
		DISP_MODULE_WDMA_VIRTUAL0,
		{
			DISP_MODULE_OVL0, DISP_MODULE_OVL0_2L,
			DISP_MODULE_OVL1_2L, DISP_MODULE_RSZ0, -1
		},
		0, 0
	},

	[9] = {
		DISP_MODULE_OVL0,
		{
			DISP_MODULE_OVL0_2L, DISP_MODULE_OVL1_2L,
			DISP_MODULE_RSZ0, -1
		},
		0, 0
	},

	[10] = {
		DISP_MODULE_OVL0_2L,
		{
			DISP_MODULE_OVL0, DISP_MODULE_OVL1_2L, -1
		},
		0, 0
	},

	[11] = {
		DISP_MODULE_OVL1_2L,
		{
			DISP_MODULE_OVL0, DISP_MODULE_OVL0_2L, -1
		},
		0, 0
	},
};

static const int DDP_MOUT_NUM = ARRAY_SIZE(mout_map);
static const int DDP_SEL_OUT_NUM = ARRAY_SIZE(sel_out_map);
static const int DDP_SEL_IN_NUM = ARRAY_SIZE(sel_in_map);

int ddp_path_init(void)
{
	mout_map[0].reg = (unsigned long *)DISP_REG_CONFIG_DISP_OVL0_MOUT_EN;
	mout_map[1].reg = (unsigned long *)DISP_REG_CONFIG_DISP_OVL0_2L_MOUT_EN;
	mout_map[2].reg = (unsigned long *)DISP_REG_CONFIG_DISP_OVL1_2L_MOUT_EN;
	mout_map[3].reg = (unsigned long *)DISP_REG_CONFIG_DISP_DITHER0_MOUT_EN;
	mout_map[4].reg = (unsigned long *)DISP_REG_CONFIG_DISP_RSZ_MOUT_EN;
	mout_map[5].reg = (unsigned long *)
				DISP_REG_CONFIG_DISP_OVL0_WCG_MOUT_EN;
	mout_map[6].reg = (unsigned long *)
				DISP_REG_CONFIG_DISP_OVL0_2L_WCG_MOUT_EN;
	mout_map[7].reg = (unsigned long *)
				DISP_REG_CONFIG_DISP_OVL1_2L_WCG_MOUT_EN;

	sel_out_map[0].reg = (unsigned long *)
				DISP_REG_CONFIG_DISP_RDMA0_RSZ_IN_SOUT_SEL_IN;
	sel_out_map[1].reg = (unsigned long *)
				DISP_REG_CONFIG_DISP_RDMA0_SOUT_SEL_IN;
	sel_out_map[2].reg = (unsigned long *)
				DISP_REG_CONFIG_DISP_RDMA1_SOUT_SEL_IN;

	sel_in_map[0].reg = (unsigned long *)
				DISP_REG_CONFIG_DISP_COLOR_OUT_SEL_IN;
	sel_in_map[1].reg = (unsigned long *)DISP_REG_CONFIG_DISP_PATH0_SEL_IN;
	sel_in_map[2].reg = (unsigned long *)
				DISP_REG_CONFIG_DISP_RDMA0_RSZ_OUT_SEL_IN;
	sel_in_map[3].reg = (unsigned long *)DISP_REG_CONFIG_DISP_RSZ_SEL_IN;
	sel_in_map[4].reg = (unsigned long *)
				DISP_REG_CONFIG_DISP_WDMA0_PRE_SEL_IN;
	sel_in_map[5].reg = (unsigned long *)DISP_REG_CONFIG_DISP_WDMA0_SEL_IN;
	sel_in_map[6].reg = (unsigned long *)DISP_REG_CONFIG_DPI0_SEL_IN;
	sel_in_map[7].reg = (unsigned long *)DISP_REG_CONFIG_DSI0_SEL_IN;
	sel_in_map[8].reg = (unsigned long *)DISP_REG_CONFIG_OVL_TO_WDMA_SEL_IN;
	sel_in_map[9].reg = (unsigned long *)
				DISP_REG_CONFIG_DISP_OVL0_WCG_SEL_IN;
	sel_in_map[10].reg = (unsigned long *)
				DISP_REG_CONFIG_DISP_OVL0_2L_WCG_SEL_IN;
	sel_in_map[11].reg = (unsigned long *)
				DISP_REG_CONFIG_DISP_OVL1_2L_WCG_SEL_IN;

	return 0;
}

/* for debug */
char *ddp_get_scenario_name(enum DDP_SCENARIO_ENUM scenario)
{
	switch (scenario) {
	/* primary display */
	case DDP_SCENARIO_PRIMARY_DISP:
		return "primary_disp";
	case DDP_SCENARIO_PRIMARY_BYPASS_PQ_DISP:
		return "primary_bypass_pq";
	case DDP_SCENARIO_PRIMARY_RDMA0_COLOR0_DISP:
		return "primary_rdma0_color0_disp";
	case DDP_SCENARIO_PRIMARY_RDMA0_DISP:
		return "primary_rdma0_disp";
	case DDP_SCENARIO_PRIMARY_OVL_MEMOUT:
		return "primary_ovl_memout";
	case DDP_SCENARIO_PRIMARY_ALL:
		return "primary_all";

	/* sub display */
	case DDP_SCENARIO_SUB_DISP:
		return "sub_disp";
	case DDP_SCENARIO_SUB_RDMA1_DISP:
		return "sub_rdma1_disp";
	case DDP_SCENARIO_SUB_OVL_MEMOUT:
		return "sub_ovl_memout";
	case DDP_SCENARIO_SUB_ALL:
		return "sub_all";

	/* others */
	default:
		DDPMSG("invalid scenario id=%d\n", scenario);
		return "unknown";
	}
}

char *ddp_get_mode_name(enum DDP_MODE ddp_mode)
{
	switch (ddp_mode) {
	case DDP_VIDEO_MODE:
		return "video_mode";
	case DDP_CMD_MODE:
		return "cmd_mode";
	default:
		DDPMSG("invalid ddp mode =%d\n", ddp_mode);
		return "unknown";
	}
}

int ddp_get_module_num_l(int *module_list)
{
	unsigned int num = 0;

	while (*(module_list + num) != -1) {
		num++;

		if (num == DDP_ENING_NUM)
			break;
	}
	return num;
}

/* config mout/msel to creat a compelte path */
static void ddp_connect_path_l(int *module_list, void *handle)
{
	unsigned int i, j, k;
	int step = 0;
	unsigned int mout = 0;
	unsigned int reg_mout = 0;
	unsigned int mout_idx = 0;
	unsigned int module_num = ddp_get_module_num_l(module_list);

	DDPDBG("connect_path: %s to %s\n", ddp_get_module_name(module_list[0]),
	       ddp_get_module_name(module_list[module_num - 1]));

	/* connect mout */
	for (i = 0; i < module_num - 1; i++) {
		for (j = 0; j < DDP_MOUT_NUM; j++) {
			if (module_list[i] != mout_map[j].id)
				continue;

			/* find next module which can be connected */
			step = i + 1;
			while (_can_connect(module_list[step]) == 0 &&
			       step < module_num) {
				step++;
			}
			ASSERT(step < module_num);
			mout = mout_map[j].reg_val;
			for (k = 0; k < BIT_NUM; k++) {
				if (mout_map[j].out_id_bit_map[k].m == -1)
					break;
				if (mout_map[j].out_id_bit_map[k].m !=
				    module_list[step])
					continue;

				mout |= mout_map[j].out_id_bit_map[k].v;
				reg_mout |= mout;
				mout_idx = j;
				DDPDBG("connect mout %s to %s  bits 0x%x\n",
				       ddp_get_module_name(module_list[i]),
				       ddp_get_module_name(module_list[step]),
				       reg_mout);
				break;
			}
			mout_map[j].reg_val = mout;
			mout = 0;
		}

		if (reg_mout) {
			DISP_REG_SET(handle, mout_map[mout_idx].reg, reg_mout);
			reg_mout = 0;
			mout_idx = 0;
		}
	}

	/* connect out select */
	for (i = 0; i < module_num - 1; i++) {
		for (j = 0; j < DDP_SEL_OUT_NUM; j++) {
			if (module_list[i] != sel_out_map[j].id)
				continue;

			step = i + 1;
			/* find next module which can be connected */
			while (_can_connect(module_list[step]) == 0 &&
			       step < module_num) {
				step++;
			}
			ASSERT(step < module_num);
			for (k = 0; k < BIT_NUM; k++) {
				if (sel_out_map[j].id_bit_map[k] == -1)
					break;
				if (sel_out_map[j].id_bit_map[k] !=
				    module_list[step])
					continue;

				DDPDBG("connect out_s %s to %s, value=%d\n",
				       ddp_get_module_name(module_list[i]),
				       ddp_get_module_name(module_list[step]),
				       k);
				DISP_REG_SET(handle, sel_out_map[j].reg,
					     (uint16_t)k);
				break;
			}
		}
	}

	/* connect input select */
	for (i = 1; i < module_num; i++) {
		for (j = 0; j < DDP_SEL_IN_NUM; j++) {
			int found = 0;

			if (module_list[i] != sel_in_map[j].id)
				continue;

			step = i - 1;
			/* find next module which can be connected */
			while (_can_connect(module_list[step]) == 0 && step > 0)
				step--;

			ASSERT(step >= 0);
			for (k = 0; k < BIT_NUM; k++) {
				if (sel_in_map[j].id_bit_map[k] == -1)
					break;
				if (sel_in_map[j].id_bit_map[k] !=
				    module_list[step])
					continue;

				DDPDBG("connect in_s %s to %s, value=%d\n",
				       ddp_get_module_name(module_list[step]),
				       ddp_get_module_name(module_list[i]), k);
				DISP_REG_SET(handle, sel_in_map[j].reg,
					     (uint16_t)k);
				found = 1;
				break;
			}
			if (!found)
				DDP_PR_ERR("%s error: %s sel_in not set\n",
					   __func__,
					   ddp_get_module_name(module_list[i]));
		}
	}
}

static void ddp_check_path_l(int *module_list)
{
	unsigned int i, j, k;
	int step = 0;
	int valid = 0;
	unsigned int mout;
	unsigned int path_error = 0;
	unsigned int module_num = ddp_get_module_num_l(module_list);

	DDPDUMP("check_path: %s to %s\n", ddp_get_module_name(module_list[0]),
		ddp_get_module_name(module_list[module_num - 1]));

	/* check mout */
	for (i = 0; i < module_num - 1; i++) {
		for (j = 0; j < DDP_MOUT_NUM; j++) {
			if (module_list[i] != mout_map[j].id)
				continue;

			mout = 0;
			/* find next module which can be connected */
			step = i + 1;
			while (_can_connect(module_list[step]) == 0 &&
			       step < module_num) {
				step++;
			}
			ASSERT(step < module_num);
			for (k = 0; k < BIT_NUM; k++) {
				if (mout_map[j].out_id_bit_map[k].m == -1)
					break;
				if (mout_map[j].out_id_bit_map[k].m ==
				    module_list[step]) {
					mout |= mout_map[j].out_id_bit_map[k].v;
					valid = 1;
					break;
				}
			}
			if (!valid)
				break;

			valid = 0;
			if ((DISP_REG_GET(mout_map[j].reg) & mout) == 0) {
				path_error += 1;
				DDPDUMP(
					"error:%s mout, expect=0x%x, real=0x%x\n",
					ddp_get_module_name(module_list[i]),
					mout, DISP_REG_GET(mout_map[j].reg));
			} else if (DISP_REG_GET(mout_map[j].reg) != mout) {
				DDPDUMP(
					"warning: %s mout expect=0x%x, real=0x%x\n",
					ddp_get_module_name(module_list[i]),
					mout, DISP_REG_GET(mout_map[j].reg));
			}
			break;
		}
	}

	/* check out select */
	for (i = 0; i < module_num - 1; i++) {
		for (j = 0; j < DDP_SEL_OUT_NUM; j++) {
			if (module_list[i] != sel_out_map[j].id)
				continue;
			/* find next module which can be connected */
			step = i + 1;
			while (_can_connect(module_list[step]) == 0 &&
			       step < module_num) {
				step++;
			}
			ASSERT(step < module_num);
			for (k = 0; k < BIT_NUM; k++) {
				if (sel_out_map[j].id_bit_map[k] == -1)
					break;
				if (sel_out_map[j].id_bit_map[k] !=
				    module_list[step])
					continue;
				if (DISP_REG_GET(sel_out_map[j].reg) == k)
					break;

				path_error += 1;
				DDPDUMP(
					"error:out_s %s not connect to %s, expect=0x%x, real=0x%x\n",
					ddp_get_module_name(module_list[i]),
					ddp_get_module_name(module_list[step]),
					k, DISP_REG_GET(sel_out_map[j].reg));
				break;
			}
		}
	}

	/* check input select */
	for (i = 1; i < module_num; i++) {
		for (j = 0; j < DDP_SEL_IN_NUM; j++) {
			if (module_list[i] != sel_in_map[j].id)
				continue;
			/* find next module which can be connected */
			step = i - 1;
			while (_can_connect(module_list[step]) == 0 && step > 0)
				step--;
			ASSERT(step >= 0);
			for (k = 0; k < BIT_NUM; k++) {
				if (sel_in_map[j].id_bit_map[k] == -1)
					break;
				if (sel_in_map[j].id_bit_map[k] !=
				    module_list[step])
					continue;

				if (DISP_REG_GET(sel_in_map[j].reg) == k)
					break;

				path_error += 1;
				DDPDUMP(
					"error:in_s %s not conn to %s,expect0x%x,real0x%x\n",
					ddp_get_module_name(module_list[step]),
					ddp_get_module_name(module_list[i]), k,
					DISP_REG_GET(sel_in_map[j].reg));
				break;
			}
		}
	}

	if (path_error == 0) {
		DDPDUMP("path: %s to %s is connected\n",
			ddp_get_module_name(module_list[0]),
			ddp_get_module_name(module_list[module_num - 1]));
	} else {
		DDPDUMP("path: %s to %s not connected!!!\n",
			ddp_get_module_name(module_list[0]),
			ddp_get_module_name(module_list[module_num - 1]));
	}
}

static void ddp_disconnect_path_l(int *module_list, void *handle)
{
	unsigned int i, j, k;
	int step = 0;
	unsigned int mout = 0;
	unsigned int reg_mout = 0;
	unsigned int mout_idx = 0;
	unsigned int module_num = ddp_get_module_num_l(module_list);

	DDPDBG("disconnect_path: %s to %s\n",
	       ddp_get_module_name(module_list[0]),
	       ddp_get_module_name(module_list[module_num - 1]));
	for (i = 0; i < module_num - 1; i++) {
		for (j = 0; j < DDP_MOUT_NUM; j++) {
			if (module_list[i] != mout_map[j].id)
				continue;

			/* find next module which can be connected */
			step = i + 1;
			while (_can_connect(module_list[step]) == 0 &&
			       step < module_num) {
				step++;
			}
			ASSERT(step < module_num);
			for (k = 0; k < BIT_NUM; k++) {
				if (mout_map[j].out_id_bit_map[k].m == -1)
					break;

				if (mout_map[j].out_id_bit_map[k].m !=
				    module_list[step])
					continue;

				mout |= mout_map[j].out_id_bit_map[k].v;
				reg_mout |= mout;
				mout_idx = j;
				DDPDBG("disconnect mout %s to %s\n",
				       ddp_get_module_name(module_list[i]),
				       ddp_get_module_name(module_list[step]));
				break;
			}
			/* update mout_value */
			mout_map[j].reg_val &= ~mout;
			mout = 0;
		}

		if (reg_mout) {
			DISP_REG_SET(handle, mout_map[mout_idx].reg,
				     mout_map[mout_idx].reg_val);
			reg_mout = 0;
			mout_idx = 0;
		}
	}
}

int ddp_get_module_num(enum DDP_SCENARIO_ENUM scenario)
{
	return ddp_get_module_num_l(module_list_scenario[scenario]);
}

static void ddp_print_scenario(enum DDP_SCENARIO_ENUM scenario)
{
	int i = 0;
	char path[512] = { '\0' };
	int num = ddp_get_module_num(scenario);

	for (i = 0; i < num; i++)
		strncat(path, ddp_get_module_name(
					module_list_scenario[scenario][i]),
					(sizeof(path) - strlen(path) - 1));
	DDPMSG("scenario %s have modules: %s\n",
	       ddp_get_scenario_name(scenario), path);
}

static int ddp_find_module_index(enum DDP_SCENARIO_ENUM scn,
				 enum DISP_MODULE_ENUM module)
{
	int i = 0;

	for (i = 0; i < DDP_ENING_NUM; i++) {
		if (module_list_scenario[scn][i] == module)
			return i;

	}
	DDPDBG("find module: can not find module %s on scenario %s\n",
	       ddp_get_module_name(module),
	       ddp_get_scenario_name(scn));
	return -1;
}

/* set display interface when kernel init */
int ddp_set_dst_module(enum DDP_SCENARIO_ENUM scenario,
		       enum DISP_MODULE_ENUM dst_module)
{
	int i = 0;

	DDPDBG("%s, scenario=%s, dst_module=%s\n", __func__,
	       ddp_get_scenario_name(scenario),
	       ddp_get_module_name(dst_module));

	if (ddp_find_module_index(scenario, dst_module) != -1) {
		DDPDBG("%s is already on path\n",
		       ddp_get_module_name(dst_module));
		return 0;
	}
	i = ddp_get_module_num_l(module_list_scenario[scenario]) - 1;
	ASSERT(i >= 0);

	if (dst_module == DISP_MODULE_DSIDUAL) {
		if (i < (DDP_ENING_NUM - 1)) {
			;
		} else {
			DDP_PR_ERR("set dst module over up bound\n");
			return -1;
		}
	} else {
		if (ddp_get_dst_module(scenario) == DISP_MODULE_DSIDUAL) {
			if (i >= 1) {
				module_list_scenario[scenario][i--] = -1;
			} else {
				DDP_PR_ERR("set dst module over low bound\n");
				return -1;
			}
		}
	}

	module_list_scenario[scenario][i] = dst_module;

	if (scenario == DDP_SCENARIO_PRIMARY_ALL)
		ddp_set_dst_module(DDP_SCENARIO_PRIMARY_DISP, dst_module);
	else if (scenario == DDP_SCENARIO_SUB_ALL)
		ddp_set_dst_module(DDP_SCENARIO_SUB_RDMA1_DISP, dst_module);

	ddp_print_scenario(scenario);
	return 0;
}

enum DISP_MODULE_ENUM ddp_get_dst_module(enum DDP_SCENARIO_ENUM scn)
{
	enum DISP_MODULE_ENUM module_name = DISP_MODULE_UNKNOWN;
	int module_num = ddp_get_module_num_l(module_list_scenario[scn]) - 1;

	if (module_num >= 0)
		module_name = module_list_scenario[scn][module_num];

	return module_name;
}

int *ddp_get_scenario_list(enum DDP_SCENARIO_ENUM scn)
{
	return module_list_scenario[scn];
}

int ddp_is_module_in_scenario(enum DDP_SCENARIO_ENUM scn,
			      enum DISP_MODULE_ENUM module)
{
	int i = 0;

	for (i = 0; i < DDP_ENING_NUM; i++) {
		if (module_list_scenario[scn][i] == module)
			return 1;
	}
	return 0;
}

void ddp_connect_path(enum DDP_SCENARIO_ENUM scenario, void *handle)
{
	int *m_list = NULL;

	DDPDBG("path connect on scenario %s\n",
	       ddp_get_scenario_name(scenario));

	if (scenario == DDP_SCENARIO_PRIMARY_ALL) {
		m_list = module_list_scenario[DDP_SCENARIO_PRIMARY_DISP];
		ddp_connect_path_l(m_list, handle);

		m_list = module_list_scenario[DDP_SCENARIO_PRIMARY_OVL_MEMOUT];
		ddp_connect_path_l(m_list, handle);
	} else if (scenario == DDP_SCENARIO_SUB_ALL) {
		m_list = module_list_scenario[DDP_SCENARIO_SUB_DISP];
		ddp_connect_path_l(m_list, handle);

		m_list = module_list_scenario[DDP_SCENARIO_SUB_OVL_MEMOUT];
		ddp_connect_path_l(m_list, handle);
	} else {
		m_list = module_list_scenario[scenario];
		ddp_connect_path_l(m_list, handle);
	}
}

void ddp_disconnect_path(enum DDP_SCENARIO_ENUM scenario, void *handle)
{
	int *m_list = NULL;

	DDPDBG("path disconnect on scenario %s\n",
	       ddp_get_scenario_name(scenario));

	if (scenario == DDP_SCENARIO_PRIMARY_ALL) {
		m_list = module_list_scenario[DDP_SCENARIO_PRIMARY_DISP];
		ddp_disconnect_path_l(m_list, handle);

		m_list = module_list_scenario[DDP_SCENARIO_PRIMARY_OVL_MEMOUT];
		ddp_disconnect_path_l(m_list, handle);
	} else if (scenario == DDP_SCENARIO_SUB_ALL) {
		m_list = module_list_scenario[DDP_SCENARIO_SUB_DISP];
		ddp_disconnect_path_l(m_list, handle);

		m_list = module_list_scenario[DDP_SCENARIO_SUB_OVL_MEMOUT];
		ddp_disconnect_path_l(m_list, handle);
	} else {
		m_list = module_list_scenario[scenario];
		ddp_disconnect_path_l(m_list, handle);
	}
}

void ddp_check_path(enum DDP_SCENARIO_ENUM scenario)
{
	int *m_list = NULL;

	DDPDBG("path check path on scenario %s\n",
	       ddp_get_scenario_name(scenario));

	if (scenario == DDP_SCENARIO_PRIMARY_ALL) {
		m_list = module_list_scenario[DDP_SCENARIO_PRIMARY_DISP];
		ddp_check_path_l(m_list);

		m_list = module_list_scenario[DDP_SCENARIO_PRIMARY_OVL_MEMOUT];
		ddp_check_path_l(m_list);
	} else if (scenario == DDP_SCENARIO_SUB_ALL) {
		m_list = module_list_scenario[DDP_SCENARIO_SUB_DISP];
		ddp_check_path_l(m_list);

		m_list = module_list_scenario[DDP_SCENARIO_SUB_OVL_MEMOUT];
		ddp_check_path_l(m_list);
	} else {
		m_list = module_list_scenario[scenario];
		ddp_check_path_l(m_list);
	}
}

int ddp_path_top_clock_on(void)
{
#ifdef CONFIG_MTK_IOMMU_V2
	struct disp_iommu_device *iommu_dev;
	int larb_idx = 0;
#endif

	DDPDBG("ddp path top clock on\n");

	if (disp_helper_get_option(DISP_OPT_DYNAMIC_SWITCH_MMSYSCLK))
		; /* ddp_clk_prepare_enable(MM_VENCPLL); */

	ddp_clk_top_clk_switch(true);

	DISP_REG_SET(NULL, DISP_REG_CONFIG_MMSYS_SODI_REQ_MASK, 0x0F005506);

#ifdef CONFIG_MTK_IOMMU_V2
	iommu_dev = disp_get_iommu_dev();
	if (!iommu_dev) {
		DDPMSG("%s iommu is null\n", __func__);
		return 0;
	}
	for (larb_idx = 0; larb_idx < DISP_LARB_COUNT; larb_idx++) {
		mtk_smi_larb_get(&iommu_dev->larb_pdev[larb_idx]->dev);
		dev_notice(&iommu_dev->larb_pdev[larb_idx]->dev,
			   "%s smi larb get of %d\n", __func__, larb_idx);
	}
#endif
	DDPDBG("ddp CG0:%x, CG1:%x\n",
	       DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0),
	       DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON1));

	return 0;
}

int ddp_path_top_clock_off(void)
{
#ifdef CONFIG_MTK_IOMMU_V2
	struct disp_iommu_device *iommu_dev;
	int larb_idx = 0;

	iommu_dev = disp_get_iommu_dev();

	for (larb_idx = 0; larb_idx < DISP_LARB_COUNT; larb_idx++)
		mtk_smi_larb_put(&iommu_dev->larb_pdev[larb_idx]->dev);
#endif
	DISP_REG_SET_FIELD(NULL, FLD_SODI_REQ_MASKEN,
			   DISP_REG_CONFIG_MMSYS_SODI_REQ_MASK, 0x1);
	DISP_REG_SET_FIELD(NULL, FLD_SODI_REQ_MASKVAL,
			   DISP_REG_CONFIG_MMSYS_SODI_REQ_MASK, 0x0);

	ddp_clk_top_clk_switch(false);

	if (disp_helper_get_option(DISP_OPT_DYNAMIC_SWITCH_MMSYSCLK))
		; /* ddp_clk_disable_unprepare(MM_VENCPLL); */

	return 0;
}

int ddp_insert_config_allow_rec(void *handle)
{
	int ret = 0;

	ASSERT(handle);

	if (primary_display_is_video_mode())
		ret = cmdqRecWaitNoClear(handle, CMDQ_EVENT_MUTEX0_STREAM_EOF);
	else
		ret = cmdqRecWaitNoClear(handle, CMDQ_SYNC_TOKEN_STREAM_EOF);

	return ret;
}

int ddp_insert_config_dirty_rec(void *handle)
{
	int ret = 0;

	ASSERT(handle);

	if (primary_display_is_video_mode()) /* TODO: modify this */
		; /* do nothing */
	else
		ret = cmdqRecSetEventToken(handle,
					   CMDQ_SYNC_TOKEN_CONFIG_DIRTY);

	return ret;
}

int disp_get_dst_module(enum DDP_SCENARIO_ENUM scenario)
{
	return ddp_get_dst_module(scenario);
}

int ddp_convert_ovl_input_to_rdma(struct RDMA_CONFIG_STRUCT *rdma_cfg,
				  struct OVL_CONFIG_STRUCT *ovl_cfg,
				  int dst_w, int dst_h)
{
	unsigned int Bpp = ufmt_get_Bpp(ovl_cfg->fmt);
	unsigned int offset;

	rdma_cfg->dst_y = ovl_cfg->dst_y;
	rdma_cfg->dst_x = ovl_cfg->dst_x;
	rdma_cfg->dst_h = dst_h;
	rdma_cfg->dst_w = dst_w;
	rdma_cfg->inputFormat = ovl_cfg->fmt;
	offset = ovl_cfg->src_x * Bpp + ovl_cfg->src_y * ovl_cfg->src_pitch;
	rdma_cfg->address = ovl_cfg->addr + offset;
	rdma_cfg->pitch = ovl_cfg->src_pitch;
	rdma_cfg->width = ovl_cfg->dst_w;
	rdma_cfg->height = ovl_cfg->dst_h;
	rdma_cfg->security = ovl_cfg->security;
	rdma_cfg->yuv_range = ovl_cfg->yuv_range;
	return 0;
}

unsigned int get_smi_larb_ostd(enum DISP_MODULE_ENUM module)
{
	switch (module) {
	case DISP_MODULE_OVL0:
		return DISP_REG_GET_FIELD(REG_FLD_MSB_LSB(4, 0),
			DISPSYS_SMI_LARB0_BASE + 0x28C);
	case DISP_MODULE_OVL0_2L:
		return DISP_REG_GET_FIELD(REG_FLD_MSB_LSB(4, 0),
			DISPSYS_SMI_LARB1_BASE + 0x288);
	default:
		return 0xff;
	}
}

/* check the following things
 * 1. OVL state is at h_w_rst
 * 2. OVL RDMAx SMI busy
 * 3. SMI OSTD for OVL is zero
 */
bool ovl_need_mmsys_sw_reset(enum DISP_MODULE_ENUM module)
{
	unsigned int ovl_smi_busy = 0, ovl_state;
	unsigned int smi_larb_ostd;
	int i, ovl_reg_value;
	unsigned long ovl_base = ovl_base_addr(module);

	ovl_state = DISP_REG_GET_FIELD(REG_FLD(10, 0),
		DISP_REG_OVL_FLOW_CTRL_DBG + ovl_base);

	for (i = 0 ; i < ovl_layer_num(module) ; i++)	{
		ovl_reg_value =	DISP_REG_GET(DISP_REG_OVL_RDMA0_DBG +
			ovl_base + 0x4 * i);
		ovl_smi_busy |= (ovl_reg_value >> 30) & 0x1;
	}

	smi_larb_ostd = get_smi_larb_ostd(module);

	DDPMSG("[%s]%s: ovl state:0x%x, ovl_smi_busy:%u, larb_ostd:%u\n",
		__func__, ddp_get_module_name(module), ovl_state,
		ovl_smi_busy, smi_larb_ostd);

	if (ovl_state == 0x100 && ovl_smi_busy != 0 && smi_larb_ostd == 0)
		return true;
	else
		return false;
}

void ddp_path_mmsys_sw_reset(unsigned int sw_rst_id, unsigned int bit)
{
	unsigned long addr = 0;

	if (sw_rst_id == 0)
		addr = DISP_REG_CONFIG_MMSYS_SW0_RST_B;
	else if (sw_rst_id == 1)
		addr = DISP_REG_CONFIG_MMSYS_SW1_RST_B;
	else
		return;

	DISP_REG_SET_FIELD(NULL, REG_FLD(1, bit), addr, 0x0);
	DISP_REG_SET_FIELD(NULL, REG_FLD(1, bit), addr, 0x1);
}
