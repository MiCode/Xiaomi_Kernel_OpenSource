/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/clk.h>

/*
 * BIT Operation
 */
#undef  BIT
#define BIT(_bit_) (unsigned int)(1 << (_bit_))
#define BITS(_bits_, _val_) ((((unsigned int) -1 >> (31 - ((1) ? _bits_))) \
& ~((1U << ((0) ? _bits_)) - 1)) & ((_val_)<<((0) ? _bits_)))
#define BITMASK(_bits_) (((unsigned int) -1 >> (31 - ((1) ? _bits_))) \
& ~((1U << ((0) ? _bits_)) - 1))
#define GET_BITS_VAL(_bits_, _val_) (((_val_) & \
(BITMASK(_bits_))) >> ((0) ? _bits_))

/*
 * Clock Operation
 */
#define PREPARE_CLK(clk) \
	{ \
		clk = devm_clk_get(&pdev->dev, #clk); \
		if (IS_ERR(clk)) { \
			ret = -ENOENT; \
			pr_notice("can not find clk: %s\n", #clk); \
		} \
		ret_clk |= ret; \
	}

#define UNPREPARE_CLK(clk) \
	{ \
		if (clk != NULL) \
			clk = NULL; \
	}

#define ENABLE_CLK(clk) \
	{ \
		ret = clk_prepare_enable(clk); \
		if (ret) { \
			pr_notice("fail to prepare & enable clk:%s\n", #clk); \
			clk_disable_unprepare(clk); \
		} \
		ret_clk |= ret; \
}

#define DISABLE_CLK(clk) \
	{ \
		clk_disable_unprepare(clk); \
}


void *g_APUSYS_RPCTOP_BASE;
void *g_APUSYS_VCORE_BASE;
void *g_APUSYS_CONN_BASE;
void *g_APUSYS_SPM_BASE;

/**************************************************
 * APUSYS_RPC related register
 *************************************************/
#define APUSYS_RPCTOP_BASE		(g_APUSYS_RPCTOP_BASE)
#define APUSYS_RPC_TOP_CON		(void *)(APUSYS_RPCTOP_BASE + 0x000)
#define APUSYS_RPC_TOP_SEL		(void *)(APUSYS_RPCTOP_BASE + 0x004)
#define APUSYS_RPC_SW_FIFO_WE	(void *)(APUSYS_RPCTOP_BASE + 0x008)
#define APUSYS_RPC_INTF_PWR_RDY	(void *)(APUSYS_RPCTOP_BASE + 0x044)

#define APUSYS_RPC_SW_TYPE0	(void *)(APUSYS_RPCTOP_BASE + 0x200)
#define APUSYS_RPC_SW_TYPE1	(void *)(APUSYS_RPCTOP_BASE + 0x210)
#define APUSYS_RPC_SW_TYPE2	(void *)(APUSYS_RPCTOP_BASE + 0x220)
#define APUSYS_RPC_SW_TYPE3	(void *)(APUSYS_RPCTOP_BASE + 0x230)
#define APUSYS_RPC_SW_TYPE4	(void *)(APUSYS_RPCTOP_BASE + 0x240)
#define APUSYS_RPC_SW_TYPE6	(void *)(APUSYS_RPCTOP_BASE + 0x260)
#define APUSYS_RPC_SW_TYPE7	(void *)(APUSYS_RPCTOP_BASE + 0x270)

/**************************************************
 * APUSYS_VCORE related register
 *************************************************/
#define APUSYS_VCORE_BASE		(g_APUSYS_VCORE_BASE)
#define APUSYS_VCORE_CG_CON	(void *)(APUSYS_VCORE_BASE + 0x000)
#define APUSYS_VCORE_CG_SET	(void *)(APUSYS_VCORE_BASE + 0x004)
#define APUSYS_VCORE_CG_CLR	(void *)(APUSYS_VCORE_BASE + 0x008)

/**************************************************
 * SPM and related register
 *************************************************/
#define APUSYS_SPM_BASE		(g_APUSYS_SPM_BASE)
#define APUSYS_OTHER_PWR_STATUS	(void *)(APUSYS_SPM_BASE + 0x178)
#define APUSYS_BUCK_ISOLATION		(void *)(APUSYS_SPM_BASE + 0x39C)
#define APUSYS_SPM_CROSS_WAKE_M01_REQ	(void *)(APUSYS_SPM_BASE + 0x670)

#define APMCU_WAKEUP_APU	(0x1 << 0)

/**************************************************
 * APUSYS_CONN related register
 *************************************************/
#define APUSYS_CONN_BASE		(g_APUSYS_CONN_BASE)
#define APUSYS_CONN_CG_CON	(void *)(APUSYS_CONN_BASE+0x000)
#define APUSYS_CONN_CG_CLR	(void *)(APUSYS_CONN_BASE + 0x0008)

