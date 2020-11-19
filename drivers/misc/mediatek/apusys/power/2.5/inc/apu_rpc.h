/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _APU_RPC_H
#define _APU_RPC_H

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/io.h>

/**************************************************
 * RPC register
 *************************************************/
#define RPC_TOP_CON		0x000
#define RPC_FIFO_ST		(1 << 23)
#define RPC_FIFO_RDP		(1 << 24)
#define RPC_FIFO_RDP_MASK	(7)
#define RPC_FIFO_EMPTY		(1 << 27)
#define RPC_FIFO_WRP		(28)
#define RPC_FIFO_WRP_MASK	(7)
#define RPC_FIFO_FULL		(1 << 31)

#define RPC_TOP_SEL		0x004
#define RPC_TOP_SEL_ALIVE	26
#define RPC_TOP_SEL_ALIVE_MASK	0x3F

#define RPC_SW_FIFO_WE		0x008
#define RPC_INTF_PWR_RDY	0x044
#define RPC_SW_TYPE0		0x200
#define RPC_SW_TYPE1		0x210
#define RPC_SW_TYPE2		0x220
#define RPC_SW_TYPE3		0x230
#define RPC_SW_TYPE4		0x240
#define RPC_SW_TYPE6		0x260
#define RPC_SW_TYPE7		0x270


/**************************************************
 * SPM register
 *************************************************/
#define SPM_OTHER_PWR_STATUS	0x178
#define SPM_BUCK_ISOLATION	0x39C
#define SPM_CROSS_WAKE_M01_REQ	0x670
#define WAKEUP_APU		0x1

/**************************************************
 * RPC hw engine wake up bit
 *************************************************/
#define RPC_TOP_WAKE_ID		(1)
#define RPC_VPU0_WAKE_ID	(2)
#define RPC_VPU1_WAKE_ID	(3)
#define RPC_VPU2_WAKE_ID	(4)
#define RPC_MDLA0_WAKE_ID	(6)
#define RPC_MDLA1_WAKE_ID	(7)


/**************************************************
 * TOP CLK registers
 *************************************************/
#define CLK_CFG_3	0x40
#define CLK_CFG_4	0x50

int apu_mtcmos_off(struct apu_dev *ad);
int apu_mtcmos_on(struct apu_dev *ad);
int apu_rpc_init_done(struct apu_dev *ad);
void apu_buckiso(struct apu_dev *ad, bool enable);
ulong apu_rpc_rdy_value(void);
ulong apu_spm_wakeup_value(void);

#endif
