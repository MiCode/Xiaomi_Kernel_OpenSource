/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MT6893_APUPWR_H__
#define __MT6893_APUPWR_H__ 

#include <linux/io.h>
#include <linux/clk.h>

#define MTK_POLL_DELAY_US	(10)
#define MTK_POLL_TIMEOUT	USEC_PER_SEC
#define DEBUG_DUMP_REG		(0)
#define APU_POWER_BRING_UP	(0)

enum apupw_reg {
	sys_spm,
	apu_vcore,
	apu_conn,
	apu_rpc,
	APUPW_MAX_REGS,
};

struct apu_power {
	void __iomem *regs[APUPW_MAX_REGS];
	unsigned int phy_addr[APUPW_MAX_REGS];
};

/**************************************************
 * APUSYS_RPC related register
 *************************************************/
#define APUSYS_RPC_TOP_CON		(0x000)
#define APUSYS_RPC_TOP_SEL		(0x004)
#define APUSYS_RPC_SW_FIFO_WE		(0x008)
#define APUSYS_RPC_INTF_PWR_RDY		(0x044)

#define APUSYS_RPC_SW_TYPE0		(0x200)
#define APUSYS_RPC_SW_TYPE1		(0x210)
#define APUSYS_RPC_SW_TYPE2		(0x220)
#define APUSYS_RPC_SW_TYPE3		(0x230)
#define APUSYS_RPC_SW_TYPE4		(0x240)
#define APUSYS_RPC_SW_TYPE6		(0x260)
#define APUSYS_RPC_SW_TYPE7		(0x270)

/**************************************************
 * APUSYS_VCORE related register
 *************************************************/
#define APUSYS_VCORE_CG_CON		(0x000)
#define APUSYS_VCORE_CG_SET		(0x004)
#define APUSYS_VCORE_CG_CLR		(0x008)

/**************************************************
 * SPM and related register
 *************************************************/
#define APUSYS_OTHER_PWR_STATUS		(0x178)
#define APUSYS_BUCK_ISOLATION		(0x39C)
#define APUSYS_SPM_CROSS_WAKE_M01_REQ	(0x670)
#define APMCU_WAKEUP_APU		(0x1 << 0)

/**************************************************
 * APUSYS_CONN related register
 *************************************************/
#define APUSYS_CONN_CG_CON		(0x000)
#define APUSYS_CONN_CG_SET		(0x004)
#define APUSYS_CONN_CG_CLR		(0x008)

#endif // __MT6893_APUPWR_H__
