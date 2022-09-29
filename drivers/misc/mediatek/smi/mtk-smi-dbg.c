// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Ming-Fan Chen <ming-fan.chen@mediatek.com>
 */
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <soc/mediatek/smi.h>
#include "mtk-smi-dbg.h"
#if IS_ENABLED(CONFIG_MTK_EMI)
#include <soc/mediatek/emi.h>
#endif
#include "mtk_iommu.h"
#include "clk-mtk.h"
//#include <dt-bindings/memory/mtk-smi-larb-port.h>

#define DRV_NAME	"mtk-smi-dbg"

/* LARB */
#define SMI_LARB_STAT			(0x0)
#define SMI_LARB_IRQ_EN			(0x4)
#define SMI_LARB_IRQ_STATUS		(0x8)
#define SMI_LARB_SLP_CON		(0xc)
#define SMI_LARB_CON			(0x10)
#define SMI_LARB_CON_SET		(0x14)
#define SMI_LARB_CON_CLR		(0x18)
#define SMI_LARB_VC_PRI_MODE		(0x20)
#define SMI_LARB_CMD_THRT_CON		(0x24)
#define SMI_LARB_SW_FLAG		(0x40)
#define SMI_LARB_BWL_EN			(0x50)
#define SMI_LARB_BWL_CON		(0x58)
#define SMI_LARB_OSTDL_EN		(0x60)
#define SMI_LARB_ULTRA_DIS		(0x70)
#define SMI_LARB_PREULTRA_DIS		(0x74)
#define SMI_LARB_FORCE_ULTRA		(0x78)
#define SMI_LARB_FORCE_PREULTRA		(0x7c)
#define SMI_LARB_SPM_ULTRA_MASK		(0x80)
#define SMI_LARB_SPM_STA		(0x84)
#define SMI_LARB_EXT_GREQ_VIO		(0xa0)
#define SMI_LARB_INT_GREQ_VIO		(0xa4)
#define SMI_LARB_OSTD_UDF_VIO		(0xa8)
#define SMI_LARB_OSTD_CRS_VIO		(0xac)
#define SMI_LARB_FIFO_STAT		(0xb0)
#define SMI_LARB_BUS_STAT		(0xb4)
#define SMI_LARB_CMD_THRT_STAT		(0xb8)
#define SMI_LARB_MON_REQ		(0xbc)
#define SMI_LARB_REQ_MASK		(0xc0)
#define SMI_LARB_REQ_DET		(0xc4)
#define SMI_LARB_EXT_ONGOING		(0xc8)
#define SMI_LARB_INT_ONGOING		(0xcc)
#define SMI_LARB_DBG_CON		(0xf0)
#define SMI_LARB_WRR_PORT(p)		(0x100 + ((p) << 2))
#define SMI_LARB_OSTDL_PORT(p)		(0x200 + ((p) << 2))
#define SMI_LARB_OSTD_MON_PORT(p)	(0x280 + ((p) << 2))
#define SMI_LARB_NON_SEC_CON(p)		(0x380 + ((p) << 2))

#define SMI_LARB_MON_EN			(0x400)
#define SMI_LARB_MON_CLR		(0x404)
#define SMI_LARB_MON_PORT		(0x408)
#define SMI_LARB_MON_CON		(0x40c)
#define SMI_LARB_MON_ACT_CNT		(0x410)
#define SMI_LARB_MON_REQ_CNT		(0x414)
#define SMI_LARB_MON_BEAT_CNT		(0x418)
#define SMI_LARB_MON_BYTE_CNT		(0x41c)
#define SMI_LARB_MON_CP_CNT		(0x420)
#define SMI_LARB_MON_DP_CNT		(0x424)
#define SMI_LARB_MON_OSTD_CNT		(0x428)
#define SMI_LARB_MON_CP_MAX		(0x430)
#define SMI_LARB_MON_COS_MAX		(0x434)
#define INT_SMI_LARB_STAT		(0x500)
#define INT_SMI_LARB_CMD_THRT_CON	(0x500 + (SMI_LARB_CMD_THRT_CON))
#define INT_SMI_LARB_OSTDL_EN		(0x560)
#define INT_SMI_LARB_FIFO_STAT		(0x5b0)
#define INT_SMI_LARB_BUS_STAT		(0x5b4)
#define INT_SMI_LARB_CMD_THRT_STAT	(0x5b8)
#define INT_SMI_LARB_DBG_CON		(0x500 + (SMI_LARB_DBG_CON))
#define INT_SMI_LARB_OSTD_MON_PORT(p)	(0x500 + SMI_LARB_OSTD_MON_PORT(p))

#define SMI_LARB_REGS_NR		(199)
static u32	smi_larb_regs[SMI_LARB_REGS_NR] = {
	SMI_LARB_STAT, SMI_LARB_IRQ_EN, SMI_LARB_IRQ_STATUS, SMI_LARB_SLP_CON,
	SMI_LARB_CON, SMI_LARB_CON_SET, SMI_LARB_CON_CLR, SMI_LARB_VC_PRI_MODE,
	SMI_LARB_CMD_THRT_CON,
	SMI_LARB_SW_FLAG, SMI_LARB_BWL_EN, SMI_LARB_BWL_CON, SMI_LARB_OSTDL_EN,
	SMI_LARB_ULTRA_DIS, SMI_LARB_PREULTRA_DIS,
	SMI_LARB_FORCE_ULTRA, SMI_LARB_FORCE_PREULTRA,
	SMI_LARB_SPM_ULTRA_MASK, SMI_LARB_SPM_STA,
	SMI_LARB_EXT_GREQ_VIO, SMI_LARB_INT_GREQ_VIO,
	SMI_LARB_OSTD_UDF_VIO, SMI_LARB_OSTD_CRS_VIO,
	SMI_LARB_FIFO_STAT, SMI_LARB_BUS_STAT, SMI_LARB_CMD_THRT_STAT,
	SMI_LARB_MON_REQ, SMI_LARB_REQ_MASK, SMI_LARB_REQ_DET,
	SMI_LARB_EXT_ONGOING, SMI_LARB_INT_ONGOING, SMI_LARB_DBG_CON,
	SMI_LARB_WRR_PORT(0), SMI_LARB_WRR_PORT(1), SMI_LARB_WRR_PORT(2),
	SMI_LARB_WRR_PORT(3), SMI_LARB_WRR_PORT(4), SMI_LARB_WRR_PORT(5),
	SMI_LARB_WRR_PORT(6), SMI_LARB_WRR_PORT(7), SMI_LARB_WRR_PORT(8),
	SMI_LARB_WRR_PORT(9), SMI_LARB_WRR_PORT(10), SMI_LARB_WRR_PORT(11),
	SMI_LARB_WRR_PORT(12), SMI_LARB_WRR_PORT(13), SMI_LARB_WRR_PORT(14),
	SMI_LARB_WRR_PORT(15), SMI_LARB_WRR_PORT(16), SMI_LARB_WRR_PORT(17),
	SMI_LARB_WRR_PORT(18), SMI_LARB_WRR_PORT(19), SMI_LARB_WRR_PORT(20),
	SMI_LARB_WRR_PORT(21), SMI_LARB_WRR_PORT(22), SMI_LARB_WRR_PORT(23),
	SMI_LARB_WRR_PORT(24), SMI_LARB_WRR_PORT(25), SMI_LARB_WRR_PORT(26),
	SMI_LARB_WRR_PORT(27), SMI_LARB_WRR_PORT(28), SMI_LARB_WRR_PORT(29),
	SMI_LARB_WRR_PORT(30), SMI_LARB_WRR_PORT(31),
	SMI_LARB_OSTDL_PORT(0), SMI_LARB_OSTDL_PORT(1),
	SMI_LARB_OSTDL_PORT(2), SMI_LARB_OSTDL_PORT(3),
	SMI_LARB_OSTDL_PORT(4), SMI_LARB_OSTDL_PORT(5),
	SMI_LARB_OSTDL_PORT(6), SMI_LARB_OSTDL_PORT(7),
	SMI_LARB_OSTDL_PORT(8), SMI_LARB_OSTDL_PORT(9),
	SMI_LARB_OSTDL_PORT(10), SMI_LARB_OSTDL_PORT(11),
	SMI_LARB_OSTDL_PORT(12), SMI_LARB_OSTDL_PORT(13),
	SMI_LARB_OSTDL_PORT(14), SMI_LARB_OSTDL_PORT(15),
	SMI_LARB_OSTDL_PORT(16), SMI_LARB_OSTDL_PORT(17),
	SMI_LARB_OSTDL_PORT(18), SMI_LARB_OSTDL_PORT(19),
	SMI_LARB_OSTDL_PORT(20), SMI_LARB_OSTDL_PORT(21),
	SMI_LARB_OSTDL_PORT(22), SMI_LARB_OSTDL_PORT(23),
	SMI_LARB_OSTDL_PORT(24), SMI_LARB_OSTDL_PORT(25),
	SMI_LARB_OSTDL_PORT(26), SMI_LARB_OSTDL_PORT(27),
	SMI_LARB_OSTDL_PORT(28), SMI_LARB_OSTDL_PORT(29),
	SMI_LARB_OSTDL_PORT(30), SMI_LARB_OSTDL_PORT(31),
	SMI_LARB_OSTD_MON_PORT(0), SMI_LARB_OSTD_MON_PORT(1),
	SMI_LARB_OSTD_MON_PORT(2), SMI_LARB_OSTD_MON_PORT(3),
	SMI_LARB_OSTD_MON_PORT(4), SMI_LARB_OSTD_MON_PORT(5),
	SMI_LARB_OSTD_MON_PORT(6), SMI_LARB_OSTD_MON_PORT(7),
	SMI_LARB_OSTD_MON_PORT(8), SMI_LARB_OSTD_MON_PORT(9),
	SMI_LARB_OSTD_MON_PORT(10), SMI_LARB_OSTD_MON_PORT(11),
	SMI_LARB_OSTD_MON_PORT(12), SMI_LARB_OSTD_MON_PORT(13),
	SMI_LARB_OSTD_MON_PORT(14), SMI_LARB_OSTD_MON_PORT(15),
	SMI_LARB_OSTD_MON_PORT(16), SMI_LARB_OSTD_MON_PORT(17),
	SMI_LARB_OSTD_MON_PORT(18), SMI_LARB_OSTD_MON_PORT(19),
	SMI_LARB_OSTD_MON_PORT(20), SMI_LARB_OSTD_MON_PORT(21),
	SMI_LARB_OSTD_MON_PORT(22), SMI_LARB_OSTD_MON_PORT(23),
	SMI_LARB_OSTD_MON_PORT(24), SMI_LARB_OSTD_MON_PORT(25),
	SMI_LARB_OSTD_MON_PORT(26), SMI_LARB_OSTD_MON_PORT(27),
	SMI_LARB_OSTD_MON_PORT(28), SMI_LARB_OSTD_MON_PORT(29),
	SMI_LARB_OSTD_MON_PORT(30), SMI_LARB_OSTD_MON_PORT(31),
	SMI_LARB_NON_SEC_CON(0), SMI_LARB_NON_SEC_CON(1),
	SMI_LARB_NON_SEC_CON(2), SMI_LARB_NON_SEC_CON(3),
	SMI_LARB_NON_SEC_CON(4), SMI_LARB_NON_SEC_CON(5),
	SMI_LARB_NON_SEC_CON(6), SMI_LARB_NON_SEC_CON(7),
	SMI_LARB_NON_SEC_CON(8), SMI_LARB_NON_SEC_CON(9),
	SMI_LARB_NON_SEC_CON(10), SMI_LARB_NON_SEC_CON(11),
	SMI_LARB_NON_SEC_CON(12), SMI_LARB_NON_SEC_CON(13),
	SMI_LARB_NON_SEC_CON(14), SMI_LARB_NON_SEC_CON(15),
	SMI_LARB_NON_SEC_CON(16), SMI_LARB_NON_SEC_CON(17),
	SMI_LARB_NON_SEC_CON(18), SMI_LARB_NON_SEC_CON(19),
	SMI_LARB_NON_SEC_CON(20), SMI_LARB_NON_SEC_CON(21),
	SMI_LARB_NON_SEC_CON(22), SMI_LARB_NON_SEC_CON(23),
	SMI_LARB_NON_SEC_CON(24), SMI_LARB_NON_SEC_CON(25),
	SMI_LARB_NON_SEC_CON(26), SMI_LARB_NON_SEC_CON(27),
	SMI_LARB_NON_SEC_CON(28), SMI_LARB_NON_SEC_CON(29),
	SMI_LARB_NON_SEC_CON(30), SMI_LARB_NON_SEC_CON(31),
	INT_SMI_LARB_STAT, INT_SMI_LARB_CMD_THRT_CON,
	INT_SMI_LARB_OSTDL_EN, INT_SMI_LARB_FIFO_STAT,
	INT_SMI_LARB_BUS_STAT, INT_SMI_LARB_CMD_THRT_STAT, INT_SMI_LARB_DBG_CON,
	INT_SMI_LARB_OSTD_MON_PORT(0), INT_SMI_LARB_OSTD_MON_PORT(1),
	INT_SMI_LARB_OSTD_MON_PORT(2), INT_SMI_LARB_OSTD_MON_PORT(3),
	INT_SMI_LARB_OSTD_MON_PORT(4), INT_SMI_LARB_OSTD_MON_PORT(5),
	INT_SMI_LARB_OSTD_MON_PORT(6), INT_SMI_LARB_OSTD_MON_PORT(7),
	INT_SMI_LARB_OSTD_MON_PORT(8), INT_SMI_LARB_OSTD_MON_PORT(9),
	INT_SMI_LARB_OSTD_MON_PORT(10), INT_SMI_LARB_OSTD_MON_PORT(11),
	INT_SMI_LARB_OSTD_MON_PORT(12), INT_SMI_LARB_OSTD_MON_PORT(13),
	INT_SMI_LARB_OSTD_MON_PORT(14), INT_SMI_LARB_OSTD_MON_PORT(15),
	INT_SMI_LARB_OSTD_MON_PORT(16), INT_SMI_LARB_OSTD_MON_PORT(17),
	INT_SMI_LARB_OSTD_MON_PORT(18), INT_SMI_LARB_OSTD_MON_PORT(19),
	INT_SMI_LARB_OSTD_MON_PORT(20), INT_SMI_LARB_OSTD_MON_PORT(21),
	INT_SMI_LARB_OSTD_MON_PORT(22), INT_SMI_LARB_OSTD_MON_PORT(23),
	INT_SMI_LARB_OSTD_MON_PORT(24), INT_SMI_LARB_OSTD_MON_PORT(25),
	INT_SMI_LARB_OSTD_MON_PORT(26), INT_SMI_LARB_OSTD_MON_PORT(27),
	INT_SMI_LARB_OSTD_MON_PORT(28), INT_SMI_LARB_OSTD_MON_PORT(29),
	INT_SMI_LARB_OSTD_MON_PORT(30), INT_SMI_LARB_OSTD_MON_PORT(31)
};

/* COMM */
#define SMI_L1LEN		(0x100)
#define SMI_L1ARB(m)		(0x104 + ((m) << 2))
#define SMI_MON_AXI_ENA		(0x1a0)
#define SMI_MON_AXI_CLR		(0x1a4)
#define SMI_MON_AXI_ACT_CNT	(0x1c0)
#define SMI_BUS_SEL		(0x220)
#define SMI_WRR_REG0		(0x228)
#define SMI_WRR_REG1		(0x22c)
#define SMI_READ_FIFO_TH	(0x230)
#define SMI_M4U_TH		(0x234)
#define SMI_FIFO_TH1		(0x238)
#define SMI_FIFO_TH2		(0x23c)
#define SMI_PREULTRA_MASK0	(0x240)
#define SMI_PREULTRA_MASK1	(0x244)

#define SMI_DCM			(0x300)
#define SMI_ELA			(0x304)
#define SMI_M1_RULTRA_WRR0	(0x308)
#define SMI_M1_RULTRA_WRR1	(0x30c)
#define SMI_M1_WULTRA_WRR0	(0x310)
#define SMI_M1_WULTRA_WRR1	(0x314)
#define SMI_M2_RULTRA_WRR0	(0x318)
#define SMI_M2_RULTRA_WRR1	(0x31c)
#define SMI_M2_WULTRA_WRR0	(0x320)
#define SMI_M2_WULTRA_WRR1	(0x324)
#define SMI_COMMON_CLAMP_EN	(0x3c0)
#define SMI_COMMON_CLAMP_EN_SET	(0x3c4)
#define SMI_COMMON_CLAMP_EN_CLR	(0x3c8)

#define SMI_DEBUG_S(s)		(0x400 + ((s) << 2))
#define SMI_DEBUG_EXT(slave)	(0x420 + ((slave) << 2))
#define SMI_DEBUG_M0		(0x430)
#define SMI_DEBUG_M1		(0x434)
#define SMI_DEBUG_EXT4		(0x438)
#define SMI_DEBUG_EXT5		(0x43c)
#define SMI_DEBUG_MISC		(0x440)
#define SMI_DUMMY		(0x444)
#define SMI_DEBUG_EXT6		(0x448)
#define SMI_DEBUG_EXT7		(0x44c)

#define SMI_AST_EN	(0x700)
#define SMI_AST_CLR	(0x704)
#define SMI_SW_TRIG	(0x708)
#define SMI_AST_COND	(0x70c)
#define SMI_TIMEOUT	(0x710)
#define SMI_TIMEOUT_CNT	(0x714)
#define SMI_AST_STA	(0x718)
#define SMI_AST_STA_CLR	(0x71c)

#define SMI_MON_ENA(m)		((m != 0) ? (0x600 + (((m) - 1) * 0x50)) : 0x1a0)
#define SMI_MON_CLR(m)		(SMI_MON_ENA(m) + 0x4)
#define SMI_MON_TYPE(m)		(SMI_MON_ENA(m) + 0xc)
#define SMI_MON_CON(m)		(SMI_MON_ENA(m) + 0x10)
#define SMI_MON_ACT_CNT(m)	(SMI_MON_ENA(m) + 0x20)
#define SMI_MON_REQ_CNT(m)	(SMI_MON_ENA(m) + 0x24)
#define SMI_MON_OSTD_CNT(m)	(SMI_MON_ENA(m) + 0x28)
#define SMI_MON_BEA_CNT(m)	(SMI_MON_ENA(m) + 0x2c)
#define SMI_MON_BYT_CNT(m)	(SMI_MON_ENA(m) + 0x30)
#define SMI_MON_CP_CNT(m)	(SMI_MON_ENA(m) + 0x34)
#define SMI_MON_DP_CNT(m)	(SMI_MON_ENA(m) + 0x38)
#define SMI_MON_CP_MAX(m)	(SMI_MON_ENA(m) + 0x3c)
#define SMI_MON_COS_MAX(m)	(SMI_MON_ENA(m) + 0x40)

#define SMI_COMM_REGS_NR	(66)
static u32	smi_comm_regs[SMI_COMM_REGS_NR] = {
	SMI_L1LEN,
	SMI_L1ARB(0), SMI_L1ARB(1), SMI_L1ARB(2), SMI_L1ARB(3),
	SMI_L1ARB(4), SMI_L1ARB(5), SMI_L1ARB(6), SMI_L1ARB(7),
	SMI_MON_AXI_ENA, SMI_MON_AXI_CLR, SMI_MON_AXI_ACT_CNT,
	SMI_BUS_SEL, SMI_WRR_REG0, SMI_WRR_REG1,
	SMI_READ_FIFO_TH, SMI_M4U_TH, SMI_FIFO_TH1, SMI_FIFO_TH2,
	SMI_PREULTRA_MASK0, SMI_PREULTRA_MASK1,
	SMI_DCM, SMI_ELA,
	SMI_M1_RULTRA_WRR0, SMI_M1_RULTRA_WRR1, SMI_M1_WULTRA_WRR0,
	SMI_M1_WULTRA_WRR1, SMI_M2_RULTRA_WRR0, SMI_M2_RULTRA_WRR1,
	SMI_M2_WULTRA_WRR0, SMI_M2_WULTRA_WRR1,
	SMI_COMMON_CLAMP_EN, SMI_COMMON_CLAMP_EN_SET, SMI_COMMON_CLAMP_EN_CLR,
	SMI_DEBUG_S(0), SMI_DEBUG_S(1), SMI_DEBUG_S(2), SMI_DEBUG_S(3),
	SMI_DEBUG_S(4), SMI_DEBUG_S(5), SMI_DEBUG_S(6), SMI_DEBUG_S(7),
	SMI_DEBUG_EXT(0), SMI_DEBUG_EXT(1), SMI_DEBUG_EXT(2), SMI_DEBUG_EXT(3),
	SMI_DEBUG_EXT(4), SMI_DEBUG_EXT(5), SMI_DEBUG_EXT(6), SMI_DEBUG_EXT(7),
	SMI_DEBUG_M0, SMI_DEBUG_M1, SMI_DEBUG_EXT4, SMI_DEBUG_EXT5,
	SMI_DEBUG_MISC, SMI_DUMMY, SMI_DEBUG_EXT6, SMI_DEBUG_EXT7,
	SMI_AST_EN, SMI_AST_CLR, SMI_SW_TRIG, SMI_AST_COND,
	SMI_TIMEOUT, SMI_TIMEOUT_CNT, SMI_AST_STA, SMI_AST_STA_CLR
};

/* RSI */
#define RSI_INTLV_CON			(0x0)
#define RSI_DCM_CON			(0x4)
#define RSI_DS_PM_CON			(0x8)
#define RSI_MISC_CON			(0xc)
#define RSI_STA				(0x10)
#define RSI_TEST_CON			(0x60)
#define RSI_AWOSTD_M0			(0x80)
#define RSI_AWOSTD_M1			(0x84)

#define RSI_AWOSTD_S			(0x90)
#define RSI_WOSTD_M0			(0xa0)
#define RSI_WOSTD_M1			(0xa4)
#define RSI_WOSTD_S			(0xb0)
#define RSI_AROSTD_M0			(0xc0)
#define RSI_AROSTD_M1			(0xc4)
#define RSI_AROSTD_S			(0xd0)

#define RSI_WLAST_OWE_CNT_M0		(0xe0)
#define RSI_WLAST_OWE_CNT_M1		(0xe4)
#define RSI_WLAST_OWE_CNT_S		(0xf0)

#define RSI_WDAT_CNT_M0			(0x100)
#define RSI_WDAT_CNT_M1			(0x104)
#define RSI_WDAT_CNT_S			(0x110)
#define RSI_RDAT_CNT_M0			(0x120)
#define RSI_RDAT_CNT_M1			(0x124)
#define RSI_RDAT_CNT_S			(0x130)
#define RSI_AXI_DBG_M0			(0x140)
#define RSI_AXI_DBG_M1			(0x144)
#define RSI_AXI_DBG_S			(0x150)
#define RSI_AWOSTD_PSEUDO		(0x180)
#define RSI_AROSTD_PSEUDO		(0x184)

#define SMI_RSI_REGS_NR	(29)
static u32	smi_rsi_regs[SMI_RSI_REGS_NR] = {
	RSI_INTLV_CON, RSI_DCM_CON, RSI_DS_PM_CON, RSI_MISC_CON,
	RSI_STA, RSI_TEST_CON, RSI_AWOSTD_M0, RSI_AWOSTD_M1,
	RSI_AWOSTD_S, RSI_WOSTD_M0, RSI_WOSTD_M1, RSI_WOSTD_S,
	RSI_AROSTD_M0, RSI_AROSTD_M1, RSI_AROSTD_S, RSI_WLAST_OWE_CNT_M0,
	RSI_WLAST_OWE_CNT_M1, RSI_WLAST_OWE_CNT_S, RSI_WDAT_CNT_M0,
	RSI_WDAT_CNT_M1, RSI_WDAT_CNT_S, RSI_RDAT_CNT_M0,
	RSI_RDAT_CNT_M1, RSI_RDAT_CNT_S, RSI_AXI_DBG_M0,
	RSI_AXI_DBG_M1, RSI_AXI_DBG_S, RSI_AWOSTD_PSEUDO, RSI_AROSTD_PSEUDO
};

#define SMI_MON_BUS_NR		(4)
#define SMI_MON_CNT_NR		(9)
#define SMI_MON_DEC(val, bit)	(((val) >> mon_bit[bit]) & \
	((1 << (mon_bit[(bit) + 1] - mon_bit[bit])) - 1))
#define SMI_MON_SET(base, val, mask, bit) \
	(((base) & ~((mask) << (bit))) | (((val) & (mask)) << (bit)))

enum { SET_OPS_DUMP, SET_OPS_MON_RUN, SET_OPS_MON_SET, SET_OPS_MON_FRAME, };
enum {
	MON_BIT_OPTN, MON_BIT_PORT, MON_BIT_RDWR, MON_BIT_MSTR,
	MON_BIT_RQST, MON_BIT_DSTN, MON_BIT_PRLL, MON_BIT_LARB, MON_BIT_NR,
};

/*
 * |        LARB |    PARALLEL |28
 * | DESTINATION |     REQUEST |24
 * |                           |20
 * |                    MASTER |16
 * |          RW |             |12
 * |                      PORT | 8
 */
static const u32 mon_bit[MON_BIT_NR + 1] = {0, 8, 14, 16, 24, 26, 28, 30, 32};

#define SMI_LARB_OSTD_MON_PORT_NR	(32)
#define SMI_COMM_OSTD_MON_PORT_NR	(2)
#define BUS_BUSY_TIME (5)
#define MAX_MON_REQ	(4)

struct mtk_smi_dbg_node {
	struct device	*dev;
	void __iomem	*va;
	phys_addr_t	pa;
	struct device	*comm;
	u32	id;
	u8	smi_type;

	u32	regs_nr;
	u32	*regs;

	u32	mon[SMI_MON_BUS_NR];
	u32	bus_ostd_val[BUS_BUSY_TIME][SMI_LARB_OSTD_MON_PORT_NR];
	u32	bw[MAX_MON_REQ];

	u8	port_stat[SMI_LARB_OSTD_MON_PORT_NR];
};

enum smi_bus_type {
	SMI_LARB = 0,
	SMI_COMMON,
	SMI_SUB_COMMON,
	SMI_RSI,
};

struct mtk_smi_dbg_init_setting {
	u32 ostd_cnt_offset;
	u32 mon_port_nr;
	u32 smi_nr;
	u32 mask_r;
	u32 mask_w;
};

#define MTK_SMI_TYPE_NR (3)
#define MTK_SMI_NR_MAX (64)
struct mtk_smi_dbg {
	bool			probe;
	struct dentry		*fs;
	struct dentry		*sta_fs;
	struct mtk_smi_dbg_node	larb[MTK_SMI_NR_MAX];
	struct mtk_smi_dbg_node	comm[MTK_SMI_NR_MAX];
	struct mtk_smi_dbg_node	rsi[MTK_SMI_NR_MAX];

	struct mtk_smi_dbg_init_setting init_setting[MTK_SMI_TYPE_NR];

	u64			exec;
	u8			frame;
	struct notifier_block suspend_nb;
};
static struct mtk_smi_dbg	*gsmi;
static u32 smi_force_on;

static void mtk_smi_dbg_print(struct mtk_smi_dbg *smi, const bool larb,
				const bool rsi, const u32 id, bool skip_pm_runtime)
{
	struct mtk_smi_dbg_node	node = rsi ? smi->rsi[id] : (larb ? smi->larb[id] : smi->comm[id]);
	const char		*name = rsi ? "rsi" : (larb ? "LARB" : "COMM");
	const u32		regs_nr = node.regs_nr;

	char	buf[LINK_MAX + 1] = {0};
	u32	val, comm_id = 0;
	s32	i, len, ret = 0;
	bool	dump_with = false;

	if (!node.dev || !node.va)
		return;

	if (!skip_pm_runtime) {
		if (!rsi) {
			ret = pm_runtime_get_if_in_use(node.dev);
			if (ret)
				dev_info(node.dev, "===== %s%u rpm:%d =====\n"
					, name, id, ret);
		} else
			dev_info(node.dev, "===== %s%u =====\n", name, id);
		if (ret <= 0 || rsi) {
			if (of_property_read_u32(node.dev->of_node,
				"mediatek,dump-with-comm", &comm_id))
				return;
			if (pm_runtime_get_if_in_use(smi->comm[comm_id].dev) <= 0)
				return;
			dump_with = true;
		}
	} else {
		dev_info(node.dev, "===== %s%u rpm:skip =====\n"
			, name, id);
	}

	for (i = 0, len = 0; i < regs_nr; i++) {

		val = readl_relaxed(node.va + node.regs[i]);
		if (!val)
			continue;

		ret = snprintf(buf + len, LINK_MAX - len, " %#x=%#x,",
			node.regs[i], val);
		if (ret < 0 || ret >= LINK_MAX - len) {
			ret = snprintf(buf + len, LINK_MAX - len, "%c", '\0');
			if (ret < 0)
				dev_notice(node.dev, "smi dbg print error:%d\n", ret);

			dev_info(node.dev, "%s\n", buf);

			len = 0;
			memset(buf, '\0', sizeof(char) * ARRAY_SIZE(buf));
			ret = snprintf(buf + len, LINK_MAX - len, " %#x=%#x,",
				node.regs[i], val);
			if (ret < 0)
				dev_notice(node.dev, "smi dbg print error:%d\n", ret);
		}
		len += ret;
	}
	ret = snprintf(buf + len, LINK_MAX - len, "%c", '\0');
	if (ret < 0)
		dev_notice(node.dev, "smi dbg print error:%d\n", ret);

	dev_info(node.dev, "%s\n", buf);

	if (!skip_pm_runtime) {
		if (dump_with) {
			pm_runtime_put(smi->comm[comm_id].dev);
			return;
		}
		pm_runtime_put(node.dev);
	}
}

static void mtk_smi_dbg_print_debugfs(struct seq_file *seq, u8 bus_type, const u32 id)
{
	struct mtk_smi_dbg	*smi = gsmi;
	const char		*name;
	struct mtk_smi_dbg_node	node;
	char	buf[LINK_MAX + 1] = {0};
	u32	val, regs_nr, comm_id = 0;
	s32	i, len, ret = 0;
	bool	dump_with = false;

	node = (bus_type == SMI_LARB) ? smi->larb[id] :
		((bus_type == SMI_RSI) ? smi->rsi[id] : smi->comm[id]);
	name = (bus_type == SMI_LARB) ? "LARB" : ((bus_type == SMI_RSI) ? "RSI" : "COMM");
	regs_nr = node.regs_nr;

	if (!node.dev || !node.va)
		return;

	if (bus_type != SMI_RSI) {
		ret = pm_runtime_get_if_in_use(node.dev);
		if (ret)
			seq_printf(seq, "%s:===== %s%u rpm:%d =====\n"
				, dev_name(node.dev), name, id, ret);
	} else
		seq_printf(seq, "%s:===== %s%u =====\n", dev_name(node.dev), name, id);
	if (ret <= 0 || (bus_type == SMI_RSI)) {
		if (of_property_read_u32(node.dev->of_node,
			"mediatek,dump-with-comm", &comm_id))
			return;
		if (pm_runtime_get_if_in_use(smi->comm[comm_id].dev) <= 0)
			return;
		dump_with = true;
	}

	for (i = 0, len = 0; i < regs_nr; i++) {
		val = readl_relaxed(node.va + node.regs[i]);
		if (!val)
			continue;

		ret = snprintf(buf + len, LINK_MAX - len, " %#x=%#x,",
			node.regs[i], val);
		if (ret < 0 || ret >= LINK_MAX - len) {
			ret = snprintf(buf + len, LINK_MAX - len, "%c", '\0');
			if (ret < 0)
				dev_notice(node.dev, "smi dbg print error:%d\n", ret);

			seq_printf(seq, "%s:%s\n", dev_name(node.dev), buf);

			len = 0;
			memset(buf, '\0', sizeof(char) * ARRAY_SIZE(buf));
			ret = snprintf(buf + len, LINK_MAX - len, " %#x=%#x,",
				node.regs[i], val);
			if (ret < 0)
				dev_notice(node.dev, "smi dbg print error:%d\n", ret);
		}
		len += ret;
	}
	ret = snprintf(buf + len, LINK_MAX - len, "%c", '\0');
	if (ret < 0)
		dev_notice(node.dev, "smi dbg print error:%d\n", ret);

	seq_printf(seq, "%s:%s\n", dev_name(node.dev), buf);

	if (dump_with) {
		pm_runtime_put(smi->comm[comm_id].dev);
		return;
	}
	pm_runtime_put(node.dev);
}

static int smi_bus_status_print(struct seq_file *seq, void *data)
{
	int i, j, PRINT_NR = 5;
	struct mtk_smi_dbg	*smi = gsmi;

	seq_puts(seq, "dump SMI bus status\n");

	spin_lock_irqsave(&smi_lock.lock, smi_lock.flags);
	for (j = 0; j < PRINT_NR; j++) {
		for (i = 0; i < ARRAY_SIZE(smi->larb); i++)
			mtk_smi_dbg_print_debugfs(seq, SMI_LARB, i);

		for (i = 0; i < ARRAY_SIZE(smi->comm); i++)
			mtk_smi_dbg_print_debugfs(seq, SMI_COMMON, i);

		for (i = 0; i < ARRAY_SIZE(smi->rsi); i++)
			mtk_smi_dbg_print_debugfs(seq, SMI_RSI, i);
	}
	spin_unlock_irqrestore(&smi_lock.lock, smi_lock.flags);

	return 0;
}

static int smi_bus_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, smi_bus_status_print, inode->i_private);
}

static const struct file_operations smi_bus_status_fops = {
	.owner = THIS_MODULE,
	.open = smi_bus_status_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void mtk_smi_dbg_hang_detect_single(
	struct mtk_smi_dbg *smi, const bool larb, const u32 id)
{
	struct mtk_smi_dbg_node	node = larb ? smi->larb[id] : smi->comm[id];
	s32			i;

	dev_info(node.dev, "%s: larb:%d id:%u\n", __func__, larb, id);
	mtk_smi_dbg_print(smi, larb, false, id, false);
	if (larb)
		for (i = 0; i < ARRAY_SIZE(smi->comm); i++) {
			if (smi->comm[i].dev == smi->larb[id].comm) {
				mtk_smi_dbg_print(smi, !larb, false, i, false);
				break;
			}
		}
}

static void mtk_smi_dbg_conf_set_run(
	struct mtk_smi_dbg *smi, const bool larb, const u32 id)
{
	struct mtk_smi_dbg_node	node = larb ? smi->larb[id] : smi->comm[id];
	const char		*name = larb ? "larb" : "common";
	u32	prll, dstn, rqst, rdwr[SMI_MON_BUS_NR], port[SMI_MON_BUS_NR];
	u32	i, *mon = larb ? smi->larb[id].mon : smi->comm[id].mon;
	u32	mon_port, val_port, mon_con, val_con, mon_ena, val_ena;

	if (!node.dev || !node.va || !mon[0] || mon[0] == ~0)
		return;

	prll = SMI_MON_DEC(mon[0], MON_BIT_PRLL);
	dstn = SMI_MON_DEC(mon[0], MON_BIT_DSTN);
	rqst = SMI_MON_DEC(mon[0], MON_BIT_RQST);
	for (i = 0; i < SMI_MON_BUS_NR; i++) {
		rdwr[i] = SMI_MON_DEC(mon[i], MON_BIT_RDWR);
		port[i] = SMI_MON_DEC(mon[i], MON_BIT_PORT);
	}
	dev_info(node.dev,
		"%pa.%s:%u prll:%u dstn:%u rqst:%u port:rdwr %u:%u %u:%u %u:%u %u:%u\n",
		&node.pa, name, id, prll, dstn, rqst, port[0], rdwr[0],
		port[1], rdwr[1], port[2], rdwr[2], port[3], rdwr[3]);

	mon_port = larb ? SMI_LARB_MON_PORT : SMI_MON_TYPE(0);
	val_port = readl_relaxed(node.va + mon_port);
	if (!larb)
		val_port = SMI_MON_SET(val_port, rqst, 0x3, 4);
	for (i = 0; i < SMI_MON_BUS_NR; i++)
		val_port = larb ?
			SMI_MON_SET(val_port, port[i], 0x3f, (i << 3)) :
			SMI_MON_SET(val_port, rdwr[i], 0x1, i);
	writel_relaxed(val_port, node.va + mon_port);

	mon_con = larb ? SMI_LARB_MON_CON : SMI_MON_CON(0);
	val_con = readl_relaxed(node.va + mon_con);
	if (larb) {
		val_con = SMI_MON_SET(val_con, prll, 0x1, 10);
		val_con = SMI_MON_SET(val_con, rqst, 0x3, 4);
		val_con = SMI_MON_SET(val_con, rdwr[0], 0x3, 2);
	} else
		for (i = 0; i < SMI_MON_BUS_NR; i++)
			val_con = SMI_MON_SET(
				val_con, port[i], 0xf, (i + 1) << 2);
	writel_relaxed(val_con, node.va + mon_con);

	mon_ena = larb ? SMI_LARB_MON_EN : SMI_MON_ENA(0);
	val_ena = (larb ? readl_relaxed(node.va + mon_ena) : SMI_MON_SET(
		readl_relaxed(node.va + mon_ena), prll, 0x1, 1)) | 0x1;

	dev_info(node.dev,
		"%pa.%s:%u MON_PORT:%#x=%#x MON_CON:%#x=%#x MON_ENA:%#x=%#x\n",
		&node.pa, name, id,
		mon_port, val_port, mon_con, val_con, mon_ena, val_ena);

	smi->exec = sched_clock();
	writel_relaxed(val_ena, node.va + mon_ena);
}

static void mtk_smi_dbg_conf_stop_clr(
	struct mtk_smi_dbg *smi, const bool larb, const u32 id)
{
	struct mtk_smi_dbg_node	node = larb ? smi->larb[id] : smi->comm[id];
	void __iomem		*va = node.va;
	u32	mon = larb ? smi->larb[id].mon[0] : smi->comm[id].mon[0];
	u32	reg, val[SMI_MON_CNT_NR];

	if (!node.dev || !va || mon == ~0)
		return;

	reg = larb ? SMI_LARB_MON_EN : SMI_MON_ENA(0);
	writel_relaxed(readl_relaxed(node.va + reg) & ~1, node.va + reg);
	smi->exec = sched_clock() - smi->exec;

	/* ACT, REQ, BEA, BYT, CP, DP, OSTD, CP_MAX, COS_MAX */
	val[0] = readl_relaxed(
		va + (larb ? SMI_LARB_MON_ACT_CNT : SMI_MON_ACT_CNT(0)));
	val[1] = readl_relaxed(
		va + (larb ? SMI_LARB_MON_REQ_CNT : SMI_MON_REQ_CNT(0)));
	val[2] = readl_relaxed(
		va + (larb ? SMI_LARB_MON_BEAT_CNT : SMI_MON_BEA_CNT(0)));
	val[3] = readl_relaxed(
		va + (larb ? SMI_LARB_MON_BYTE_CNT : SMI_MON_BYT_CNT(0)));
	val[4] = readl_relaxed(
		va + (larb ? SMI_LARB_MON_CP_CNT : SMI_MON_CP_CNT(0)));
	val[5] = readl_relaxed(
		va + (larb ? SMI_LARB_MON_DP_CNT : SMI_MON_DP_CNT(0)));
	val[6] = readl_relaxed(
		va + (larb ? SMI_LARB_MON_OSTD_CNT : SMI_MON_OSTD_CNT(0)));
	val[7] = readl_relaxed(
		va + (larb ? SMI_LARB_MON_CP_MAX : SMI_MON_CP_MAX(0)));
	val[8] = readl_relaxed(
		va + (larb ? SMI_LARB_MON_COS_MAX : SMI_MON_COS_MAX(0)));

	dev_info(node.dev,
		"%pa.%s:%u exec:%llu prll:%u ACT:%u REQ:%u BEA:%u BYT:%u CP:%u DP:%u OSTD:%u CP_MAX:%u COS_MAX:%u\n",
		&node.pa, larb ? "larb" : "common", id, smi->exec,
		SMI_MON_DEC(mon, MON_BIT_PRLL), val[0], val[1], val[2], val[3],
		val[4], val[5], val[6], val[7], val[8]);

	reg = larb ? SMI_LARB_MON_CLR : SMI_MON_CLR(0);
	writel_relaxed(readl_relaxed(va + reg) | 1, node.va + reg);
	writel_relaxed(readl_relaxed(va + reg) & ~1, node.va + reg);
}

static void mtk_smi_dbg_monitor_run(struct mtk_smi_dbg *smi)
{
	s32	i, larb_on[MTK_LARB_NR_MAX] = {0};
	s32	comm_on[MTK_LARB_NR_MAX] = {0};

	smi->exec = sched_clock();
	smi->frame = smi->frame ? smi->frame : 10;
	pr_info("%s: exec:%llu frame:%u\n", __func__, smi->exec, smi->frame);

	for (i = 0; i < ARRAY_SIZE(smi->larb); i++) {
		if (!smi->larb[i].dev)
			continue;
		larb_on[i] = pm_runtime_get_sync(smi->larb[i].dev);
		if (larb_on[i] > 0)
			mtk_smi_dbg_conf_set_run(smi, true, i);
	}
	for (i = 0; i < ARRAY_SIZE(smi->comm); i++) {
		if (!smi->comm[i].dev)
			continue;
		comm_on[i] = pm_runtime_get_sync(smi->comm[i].dev);
		if (comm_on[i] > 0)
			mtk_smi_dbg_conf_set_run(smi, false, i);
	}

	while (sched_clock() - smi->exec < 16700000ULL * smi->frame) // 16.7 ms
		usleep_range(1000, 3000);

	for (i = 0; i < ARRAY_SIZE(smi->larb); i++) {
		if (larb_on[i] > 0 && smi->larb[i].dev) {
			mtk_smi_dbg_conf_stop_clr(smi, true, i);
			pm_runtime_put_sync(smi->larb[i].dev);
		}
		memset(smi->larb[i].mon, ~0, sizeof(u32) * SMI_MON_BUS_NR);
	}
	for (i = 0; i < ARRAY_SIZE(smi->comm); i++) {
		if (comm_on[i] > 0 && smi->comm[i].dev) {
			mtk_smi_dbg_conf_stop_clr(smi, false, i);
			pm_runtime_put_sync(smi->comm[i].dev);
		}
		memset(smi->comm[i].mon, ~0, sizeof(u32) * SMI_MON_BUS_NR);
	}
}

static void mtk_smi_dbg_monitor_set(struct mtk_smi_dbg *smi, const u64 val)
{
	struct mtk_smi_dbg_node	*nodes =
		SMI_MON_DEC(val, MON_BIT_LARB) ? smi->larb : smi->comm;
	const char		*name =
		SMI_MON_DEC(val, MON_BIT_LARB) ? "larb" : "common";
	u32	i, *mon, mstr = SMI_MON_DEC(val, MON_BIT_MSTR);

	if (mstr >= MTK_LARB_NR_MAX || !nodes[mstr].dev || !nodes[mstr].va) {
		pr_info("%s: invalid %s:%d\n", __func__, name, mstr);
		return;
	}
	mon = nodes[mstr].mon;

	for (i = 0; i < SMI_MON_BUS_NR; i++)
		if (mon[i] == ~0) {
			mon[i] = val;
			break;
		}

	if (i == SMI_MON_BUS_NR)
		pr_info("%s: over monitor: %pa.%s:%u mon:%#x %#x %#x %#x\n",
			__func__, &nodes[mstr].pa, name, mstr,
			mon[0], mon[1], mon[2], mon[3]);
	else
		pr_info("%s: %pa.%s:%u mon:%#x %#x %#x %#x\n",
			__func__, &nodes[mstr].pa, name, mstr,
			mon[0], mon[1], mon[2], mon[3]);
}

static s32 mtk_smi_dbg_parse(struct platform_device *pdev,
	struct mtk_smi_dbg_node node[], const bool larb, const u32 id)
{
	struct resource	*res;
	void __iomem	*va;

	if (!pdev || !node || id >= MTK_LARB_NR_MAX)
		return -EINVAL;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EINVAL;
	node[id].pa = res->start;

	va = devm_ioremap(&pdev->dev, res->start, 0x1000);
	if (IS_ERR(va))
		return PTR_ERR(va);
	node[id].va = va;

	node[id].regs_nr = larb ? SMI_LARB_REGS_NR : SMI_COMM_REGS_NR;
	node[id].regs = larb ? smi_larb_regs : smi_comm_regs;
	memset(node[id].mon, ~0, sizeof(u32) * SMI_MON_BUS_NR);

	dev_info(node[id].dev, "larb:%d id:%u pa:%pa va:%p regs_nr:%u\n",
		larb, id, &node[id].pa, node[id].va, node[id].regs_nr);
	return 0;
}

static char	*mtk_smi_dbg_comp[] = {
	"mediatek,smi-larb", "mediatek,smi-common", "mediatek,smi-rsi"
};

static int smi_dbg_suspend_cb(struct notifier_block *nb,
		unsigned long value, void *v)
{
	bool is_larb = (v != NULL);

	pr_notice("[SMI] %s: %d - %ld\n", __func__, is_larb, value);
	if (value == TRIGGER_SMI_HANG_DETECT) {
		mtk_smi_dbg_hang_detect("SMI DRIVER");
		return 0;
	}

	mtk_smi_dbg_print(gsmi, is_larb, false, value, true);
	return 0;
}

#define SC_OSTD_BITS (7)
#define SSC_OSTD_BITS (6)

static void init_smi_dbg_setting(struct mtk_smi_dbg	*smi)
{

	/*smi larb*/
	smi->init_setting[SMI_LARB].ostd_cnt_offset = SMI_LARB_OSTD_MON_PORT(0);
	smi->init_setting[SMI_LARB].mon_port_nr = SMI_LARB_OSTD_MON_PORT_NR;
	smi->init_setting[SMI_LARB].smi_nr = ARRAY_SIZE(smi->larb);
	smi->init_setting[SMI_LARB].mask_r = ~(u32)0;
	smi->init_setting[SMI_LARB].mask_w = ~(u32)0;
	/*smi common*/
	smi->init_setting[SMI_COMMON].ostd_cnt_offset = SMI_DEBUG_M0;
	smi->init_setting[SMI_COMMON].mon_port_nr = SMI_COMM_OSTD_MON_PORT_NR;
	smi->init_setting[SMI_COMMON].mask_r = GENMASK(SC_OSTD_BITS + 11, 12);
	smi->init_setting[SMI_COMMON].mask_w = GENMASK(2*SC_OSTD_BITS + 11, SC_OSTD_BITS + 12);
	smi->init_setting[SMI_COMMON].smi_nr = ARRAY_SIZE(smi->comm);
	/*smi sub common*/
	smi->init_setting[SMI_SUB_COMMON].ostd_cnt_offset = SMI_DEBUG_M0;
	smi->init_setting[SMI_SUB_COMMON].mon_port_nr = SMI_COMM_OSTD_MON_PORT_NR;
	smi->init_setting[SMI_SUB_COMMON].mask_r = GENMASK(SSC_OSTD_BITS + 10, 11);
	smi->init_setting[SMI_SUB_COMMON].mask_w =
			GENMASK(2*SSC_OSTD_BITS + 10, SSC_OSTD_BITS + 11);
	smi->init_setting[SMI_SUB_COMMON].smi_nr = ARRAY_SIZE(smi->comm);

}

static s32 mtk_smi_dbg_probe(struct mtk_smi_dbg *smi)
{
	//struct device_node	*node = NULL, *comm;
	struct device_node	*node = NULL;
	struct platform_device	*pdev;
	struct resource	*res;
	void __iomem	*va;
	s32			larb_nr = 0, comm_nr = 0, rsi_nr = 0, id, ret;

	pr_info("%s: comp[%d]:%s\n", __func__, 0, mtk_smi_dbg_comp[0]);

	for_each_compatible_node(node, NULL, mtk_smi_dbg_comp[0]) {

		if (of_property_read_u32(node, "mediatek,larb-id", &id))
			id = larb_nr;
		larb_nr += 1;

		pdev = of_find_device_by_node(node);
		of_node_put(node);
		if (!pdev)
			return -EINVAL;
		smi->larb[id].dev = &pdev->dev;
		smi->larb[id].id = id;
		smi->larb[id].smi_type = SMI_LARB;

		ret = mtk_smi_dbg_parse(pdev, smi->larb, true, id);
		if (ret)
			return ret;
	}

	for_each_compatible_node(node, NULL, mtk_smi_dbg_comp[1]) {

		if (of_property_read_u32(node, "mediatek,common-id", &id))
			id = comm_nr;
		comm_nr += 1;

		pdev = of_find_device_by_node(node);
		of_node_put(node);
		if (!pdev)
			return -EINVAL;
		smi->comm[id].dev = &pdev->dev;
		smi->comm[id].id = id;

		if (of_property_read_bool(node, "smi-common"))
			smi->comm[id].smi_type = SMI_COMMON;
		else
			smi->comm[id].smi_type = SMI_SUB_COMMON;

		ret = mtk_smi_dbg_parse(pdev, smi->comm, false, id);
		if (ret)
			return ret;
	}

	for_each_compatible_node(node, NULL, mtk_smi_dbg_comp[2]) {

		if (of_property_read_u32(node, "mediatek,rsi-id", &id))
			id = rsi_nr;
		rsi_nr += 1;

		pdev = of_find_device_by_node(node);
		of_node_put(node);
		if (!pdev)
			return -EINVAL;
		smi->rsi[id].dev = &pdev->dev;
		smi->rsi[id].id = id;

		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!res)
			return -EINVAL;
		smi->rsi[id].pa = res->start;

		va = devm_ioremap(&pdev->dev, res->start, 0x1000);
		if (IS_ERR(va))
			return PTR_ERR(va);
		smi->rsi[id].va = va;

		smi->rsi[id].regs_nr = SMI_RSI_REGS_NR;
		smi->rsi[id].regs = smi_rsi_regs;

	}

	init_smi_dbg_setting(smi);

	smi->suspend_nb.notifier_call = smi_dbg_suspend_cb;
	mtk_smi_driver_register_notifier(&smi->suspend_nb);
	return 0;
}

static RAW_NOTIFIER_HEAD(smi_notifier_list);
int mtk_smi_dbg_register_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&smi_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(mtk_smi_dbg_register_notifier);

int mtk_smi_dbg_unregister_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_unregister(&smi_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(mtk_smi_dbg_unregister_notifier);

static void read_smi_ostd_status(struct mtk_smi_dbg_node node[])
{
	u32 i, j, id, type;
	struct mtk_smi_dbg	*smi = gsmi;
	struct mtk_smi_dbg_init_setting	*dbg_setting;

	for (i = 0; i < BUS_BUSY_TIME ; i++) {
		for (id = 0; id < MTK_SMI_NR_MAX; id++) {
			if (!node[id].dev || !node[id].va)
				continue;

			type = node[id].smi_type;
			dbg_setting = &smi->init_setting[type];

			if (pm_runtime_get_if_in_use(node[id].dev) <= 0) {
				memset(node[id].bus_ostd_val[i], 0,
						sizeof(u32) * SMI_LARB_OSTD_MON_PORT_NR);
				continue;
			}

			for (j = 0; j < dbg_setting->mon_port_nr; j++)
				node[id].bus_ostd_val[i][j] =
				readl_relaxed(node[id].va +
						dbg_setting->ostd_cnt_offset + (j << 2));

			pm_runtime_put(node[id].dev);
		}
		udelay(3);
	}
}

#define READ_BIT (0)
#define WRITE_BIT (1)

static u32 smi_bus_ostd_check(struct mtk_smi_dbg_node node[])
{
	struct mtk_smi_dbg	*smi = gsmi;
	struct mtk_smi_dbg_init_setting	*dbg_setting;
	u8 i, j, id, type, r_busy_cnt, w_busy_cnt, is_busy = 0;
	u32 prev_ostd_r, prev_ostd_w, ostd_r, ostd_w;
	char *name, *cmd_type;

	read_smi_ostd_status(node);

	for (id = 0; id < MTK_SMI_NR_MAX; id++) {
		if (!node[id].dev || !node[id].va)
			continue;

		type = node[id].smi_type;
		dbg_setting = &smi->init_setting[type];

		for (j = 0; j < dbg_setting->mon_port_nr; j++) {
			if (!node[id].bus_ostd_val[0][j])
				continue;

			r_busy_cnt = 0;
			w_busy_cnt = 0;
			for (i = 1; i < BUS_BUSY_TIME; i++) {
				prev_ostd_r = node[id].bus_ostd_val[i-1][j] & dbg_setting->mask_r;
				prev_ostd_w = node[id].bus_ostd_val[i-1][j] & dbg_setting->mask_w;
				ostd_r = node[id].bus_ostd_val[i][j] & dbg_setting->mask_r;
				ostd_w = node[id].bus_ostd_val[i][j] & dbg_setting->mask_w;

				if (prev_ostd_r)
					r_busy_cnt += (prev_ostd_r == ostd_r);
				if (prev_ostd_w)
					w_busy_cnt += (prev_ostd_w == ostd_w);
			}

			/*store check result for hang detect check bw*/
			node[id].port_stat[j] = 0;
			/*bit0 for read, bit1 for write*/
			node[id].port_stat[j] = ((r_busy_cnt == (BUS_BUSY_TIME - 1)) << READ_BIT);
			node[id].port_stat[j] |= ((w_busy_cnt == (BUS_BUSY_TIME - 1)) << WRITE_BIT);

			if (node[id].port_stat[j]) {
				name = type ? "COMM" : "LARB";
				cmd_type = (type == SMI_LARB) ? "" :
					((node[id].port_stat[j] >> WRITE_BIT) ? "write" : "read");
				pr_notice("[smi] %s%d %s busy, %#x = %#x\n",
					name, id, cmd_type, dbg_setting->ostd_cnt_offset + (j << 2),
					node[id].bus_ostd_val[0][j]);
				is_busy = 1;
			}
		}
	}
	return is_busy;
}

static bool smi_bus_hang_check(struct mtk_smi_dbg_node node[])
{
	struct mtk_smi_dbg	*smi = gsmi;
	struct mtk_smi_dbg_init_setting	*dbg_setting;
	u8 id, j, type;

	for (id = 0; id < MTK_SMI_NR_MAX; id++) {
		if (!node[id].dev ||
			!node[id].va ||
			!(node[id].smi_type == SMI_COMMON))
			continue;

		type = node[id].smi_type;
		dbg_setting = &smi->init_setting[type];

		for (j = 0; j < dbg_setting->mon_port_nr; j++) {
			/*check read bw*/
			if ((node[id].port_stat[j] & BIT(READ_BIT)) && !node[id].bw[j])
				return true;
			/*check write bw*/
			if ((node[id].port_stat[j] & BIT(WRITE_BIT)) && !node[id].bw[j+2])
				return true;
		}
	}

	return false;
}

s32 mtk_smi_dbg_cg_status(void)
{
	struct mtk_smi_dbg	*smi = gsmi;
	struct mtk_smi_dbg_node	node;
	s32			i, ret = 0;

	if (!smi->probe) {
		ret = mtk_smi_dbg_probe(smi);
		if (ret)
			return ret;

		smi->probe = true;
	}

	//check LARB status
	for (i = 0; i < ARRAY_SIZE(smi->larb); i++) {
		node = smi->larb[i];
		if (!node.dev || !node.va)
			continue;
		mtk_smi_check_larb_ref_cnt(node.dev);
	}

	//check COMM status
	for (i = 0; i < ARRAY_SIZE(smi->comm); i++) {
		node = smi->comm[i];
		if (!node.dev || !node.va)
			continue;
		mtk_smi_check_comm_ref_cnt(node.dev);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_smi_dbg_cg_status);


static int mtk_smi_dbg_get(void *data, u64 *val)
{
	pr_info("%s: val:%llu\n", __func__, *val);
	return 0;
}

static int mtk_smi_dbg_set(void *data, u64 val)
{
	struct mtk_smi_dbg	*smi = (struct mtk_smi_dbg *)data;
	u64			exval;
	s32			ret;

	pr_info("%s: val:%#llx\n", __func__, val);

	if (!smi->probe) {
		ret = mtk_smi_dbg_probe(smi);
		if (ret)
			return ret;

		smi->probe = true;
	}

	switch (val & 0x7) {
	case SET_OPS_DUMP:
		mtk_smi_dbg_hang_detect_single(smi,
			SMI_MON_DEC(val, MON_BIT_LARB) ? true : false,
			SMI_MON_DEC(val, MON_BIT_MSTR));
		break;
	case SET_OPS_MON_RUN:
		mtk_smi_dbg_monitor_run(smi);
		break;
	case SET_OPS_MON_SET:
		mtk_smi_dbg_monitor_set(smi, val);
		break;
	case SET_OPS_MON_FRAME:
		smi->frame = (u8)SMI_MON_DEC(val, MON_BIT_PORT);
		break;
	default:
		/* example : monitor for 5 frames */
		mtk_smi_dbg_set(data,
			(5 << mon_bit[MON_BIT_PORT]) | SET_OPS_MON_FRAME);
		/* comm0 : 0:rd */
		exval = (0 << mon_bit[MON_BIT_MSTR]) |
			(0 << mon_bit[MON_BIT_RDWR]) |
			(0 << mon_bit[MON_BIT_PORT]) | SET_OPS_MON_SET;
		mtk_smi_dbg_set(data, exval);
		/* larb1 : 2:wr 3:all */
		exval = (1 << mon_bit[MON_BIT_LARB]) |
			(1 << mon_bit[MON_BIT_PRLL]) |
			(1 << mon_bit[MON_BIT_MSTR]) |
			(2 << mon_bit[MON_BIT_RDWR]) |
			(2 << mon_bit[MON_BIT_PORT]) | SET_OPS_MON_SET;
		mtk_smi_dbg_set(data, exval);
		exval = (1 << mon_bit[MON_BIT_LARB]) |
			(1 << mon_bit[MON_BIT_PRLL]) |
			(1 << mon_bit[MON_BIT_MSTR]) |
			(0 << mon_bit[MON_BIT_RDWR]) |
			(3 << mon_bit[MON_BIT_PORT]) | SET_OPS_MON_SET;
		mtk_smi_dbg_set(data, exval);
		mtk_smi_dbg_set(data, SET_OPS_MON_RUN);
		break;
	}
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(
	mtk_smi_dbg_fops, mtk_smi_dbg_get, mtk_smi_dbg_set, "%llu");

static int __init mtk_smi_dbg_init(void)
{
	struct mtk_smi_dbg	*smi;
	struct dentry	*dir;
	bool exists = false;

	smi = kzalloc(sizeof(*smi), GFP_KERNEL);
	if (!smi)
		return -ENOMEM;
	gsmi = smi;

	dir = debugfs_lookup("smi", NULL);
	if (!dir) {
		dir = debugfs_create_dir("smi", NULL);
		if (!dir) {
			pr_notice("debugfs_create_dir smi failed");
			return -EINVAL;
		}
	} else
		exists = true;

	smi->sta_fs = debugfs_create_file(
		"smi-status", 0444, dir, smi, &smi_bus_status_fops);
	if (IS_ERR(smi->sta_fs)) {
		pr_notice("debugfs_create_file smi-status failed:%ld",
			PTR_ERR(smi->sta_fs));
		return PTR_ERR(smi->sta_fs);
	}

	smi->fs = debugfs_create_file(
		DRV_NAME, 0444, NULL, smi, &mtk_smi_dbg_fops);
	if (IS_ERR(smi->fs))
		return PTR_ERR(smi->fs);

	if (exists)
		dput(dir);

	if (!mtk_smi_dbg_probe(smi))
		smi->probe = true;
	//mtk_smi_dbg_hang_detect(DRV_NAME);
	pr_debug("%s: smi:%p fs:%p\n", __func__, smi, smi->fs);
	return 0;
}

int smi_ut_dump_get(char *buf, const struct kernel_param *kp)
{
	struct mtk_smi_dbg	*smi = gsmi;
	s32 i, ret;

	for (i = 0; i < ARRAY_SIZE(smi->larb); i++) {
		if (!smi->larb[i].dev)
			continue;
		ret = mtk_smi_larb_get(smi->larb[i].dev);
		if (ret < 0)
			dev_notice(smi->larb[i].dev, "smi_larb%d get fail:%d\n", i, ret);
	}

	mtk_smi_dbg_hang_detect("SMI UT");

	for (i = 0; i < ARRAY_SIZE(smi->larb); i++) {
		if (!smi->larb[i].dev)
			continue;
		mtk_smi_larb_put(smi->larb[i].dev);
	}

	return 0;
}

static struct kernel_param_ops smi_ut_dump_ops = {
	.get = smi_ut_dump_get,
};
module_param_cb(smi_ut_dump, &smi_ut_dump_ops, NULL, 0644);
MODULE_PARM_DESC(smi_ut_dump, "dump smi current setting");

int smi_get_larb_dump(const char *val, const struct kernel_param *kp)
{
	struct mtk_smi_dbg	*smi = gsmi;
	s32		result, larb_id, ret;

	result = kstrtoint(val, 0, &larb_id);
	if (result || larb_id < 0) {
		pr_notice("SMI get larb dump failed: %d\n", result);
		return result;
	}
	ret = mtk_smi_larb_get(smi->larb[larb_id].dev);
	if (ret < 0)
		dev_notice(smi->larb[larb_id].dev, "smi_larb%d get fail:%d\n", larb_id, ret);

	mtk_smi_dbg_hang_detect("SMI larb get and dump");

	return 0;
}

static struct kernel_param_ops smi_get_larb_dump_ops = {
	.set = smi_get_larb_dump,
};
module_param_cb(smi_larb_enable_dump, &smi_get_larb_dump_ops, NULL, 0644);
MODULE_PARM_DESC(smi_larb_enable_dump, "enable smi larb and dump current setting");

int smi_put_larb(const char *val, const struct kernel_param *kp)
{
	struct mtk_smi_dbg	*smi = gsmi;
	s32		result, larb_id;

	result = kstrtoint(val, 0, &larb_id);
	if (result || larb_id < 0) {
		pr_notice("SMI put larb failed: %d\n", result);
		return result;
	}
	mtk_smi_larb_put(smi->larb[larb_id].dev);

	return 0;
}

static struct kernel_param_ops smi_put_larb_ops = {
	.set = smi_put_larb,
};
module_param_cb(smi_larb_disable, &smi_put_larb_ops, NULL, 0644);
MODULE_PARM_DESC(smi_larb_disable, "disable smi larb");

module_init(mtk_smi_dbg_init);
MODULE_LICENSE("GPL v2");

/*
 * smi_monitor_start() - start to monitor the commonlarb read/write byte count
 * @dev: reference to the user device node
 * @common_id: the common id
 * @commonlarb_id: the common larb which would be monitored
 * @falg:  read 0x1/write 0x2
 * @mon_id: smi monitor id (0 for MET、1 for SMI、2 for IMGsys)
 *
 * Returns 0 on success, -EAGAIN means fail to get smi monitor or an
 * appropriate error code otherwise.
 */
s32 smi_monitor_start(struct device *dev, u32 common_id, u32 commonlarb_id[MAX_MON_REQ],
			u32 flag[MAX_MON_REQ], enum smi_mon_id mon_id)
{
	u32 i, ret;
	bool is_write;
	struct mtk_smi_dbg	*smi = gsmi;
	void __iomem *comm_base;

	comm_base = smi->comm[common_id].va;
	if (!comm_base) {
		pr_notice("[smi]%s: failed to monitor comm%d\n", __func__, common_id);
		return -EAGAIN;
	}

	ret = pm_runtime_get_if_in_use(smi->comm[common_id].dev);
	if (ret <= 0) {
		pr_notice("[smi]%s: comm%d power off, rpm:%d\n", __func__, common_id, ret);
		return ret;
	}

	for (i = 0; i < MAX_MON_REQ; i++) {
		if (flag[i] == 0)
			break;
		is_write = (flag[i] == 0x2);

		if (is_write)
			writel_relaxed(readl(comm_base + SMI_MON_TYPE(mon_id)) | (0x1 << i),
				comm_base + SMI_MON_TYPE(mon_id));
		writel_relaxed(readl(comm_base + SMI_MON_CON(mon_id)) |
			(commonlarb_id[i] << (4 + i * 4)), comm_base + SMI_MON_CON(mon_id));
	}
	writel_relaxed(0x3, comm_base + SMI_MON_ENA(mon_id));

	pm_runtime_put(smi->comm[common_id].dev);

	return 0;
}
EXPORT_SYMBOL_GPL(smi_monitor_start);

 /*
  * smi_monitor_stop() - start to monitor the commonlarb read/write byte count
  * @dev: reference to the user device node
  * @common_id: the common id which would be monitored
  * @bw: byte count from start to stop
  * @mon_id: smi monitor id (0 for MET、1 for SMI、2 for IMGsys)
  *
  * Returns 0 on success, or an appropriate error code otherwise.
  */
s32 smi_monitor_stop(struct device *dev, u32 common_id, u32 *bw, enum smi_mon_id mon_id)
{
	u32 i, ret;
	u32 byte_cnt[MAX_MON_REQ] = { 0 };
	struct mtk_smi_dbg	*smi = gsmi;
	void __iomem *comm_base;

	comm_base = smi->comm[common_id].va;
	if (!comm_base) {
		pr_notice("[smi]%s: failed to monitor comm%d\n", __func__, common_id);
		return -EAGAIN;
	}

	ret = pm_runtime_get_if_in_use(smi->comm[common_id].dev);
	if (ret <= 0) {
		pr_notice("[smi]%s: comm%d power off, rpm:%d\n", __func__, common_id, ret);
		return ret;
	}

	writel_relaxed(0x0, comm_base + SMI_MON_ENA(mon_id));

	byte_cnt[0] = readl(comm_base + SMI_MON_BYT_CNT(mon_id));
	byte_cnt[1] = readl(comm_base + SMI_MON_ACT_CNT(mon_id));
	byte_cnt[2] = readl(comm_base + SMI_MON_REQ_CNT(mon_id));
	byte_cnt[3] = readl(comm_base + SMI_MON_BEA_CNT(mon_id));

	for (i = 0; i < MAX_MON_REQ; i++)
		bw[i] = byte_cnt[i];

	writel_relaxed(0x0, comm_base + SMI_MON_TYPE(mon_id));
	writel_relaxed(0x0, comm_base + SMI_MON_CON(mon_id));
	writel_relaxed(0x1, comm_base + SMI_MON_CLR(mon_id));
	writel_relaxed(0x0, comm_base + SMI_MON_CLR(mon_id));

	pm_runtime_put(smi->comm[common_id].dev);

	return 0;
}
EXPORT_SYMBOL_GPL(smi_monitor_stop);

static void smi_hang_detect_bw_monitor(bool is_start)
{
	struct mtk_smi_dbg	*smi = gsmi;
	u32 commonlarb_id[MAX_MON_REQ] = {0, 1, 0, 1};
	u32 flags[MAX_MON_REQ] = {1, 1, 2, 2};
	u8 i;

	for (i = 0; i < ARRAY_SIZE(smi->comm); i++) {
		if (!smi->comm[i].dev ||
			!smi->comm[i].va ||
			!(smi->comm[i].smi_type == SMI_COMMON))
			continue;
		if (is_start)
			smi_monitor_start(NULL, i, commonlarb_id, flags, SMI_BW_BUS);
		else
			smi_monitor_stop(NULL, i, smi->comm[i].bw, SMI_BW_BUS);
	}
}

s32 mtk_smi_dbg_hang_detect(char *user)
{
	struct mtk_smi_dbg	*smi = gsmi;
	s32			i, j, ret, PRINT_NR = 1, is_busy = 0, is_hang = 0;

	pr_info("%s: check caller:%s\n", __func__, user);

	if (!smi->probe) {
		ret = mtk_smi_dbg_probe(smi);
		if (ret)
			return ret;

		smi->probe = true;
	}
#if IS_ENABLED(CONFIG_MTK_EMI)
	mtk_emidbg_dump();
#endif

	/* notify to CCF to enable trace dump */
	mtk_clk_notify(NULL, NULL, NULL, 0, 1, 0, CLK_EVT_TRIGGER_TRACE_DUMP);

	raw_notifier_call_chain(&smi_notifier_list, 0, user);

	/*start to monitor bw and check ostd*/
	spin_lock_irqsave(&smi_lock.lock, smi_lock.flags);

	smi_hang_detect_bw_monitor(true);
	is_busy |= smi_bus_ostd_check(smi->comm);
	is_busy |= smi_bus_ostd_check(smi->larb);


	if (is_busy) {
		pr_info("%s: ===== SMI MM bus busy =====:%s\n", __func__, user);
		PRINT_NR = 5;
	} else
		pr_info("%s: ===== SMI MM bus NOT hang =====:%s\n", __func__, user);

	for (j = 0; j < PRINT_NR; j++) {
		for (i = 0; i < ARRAY_SIZE(smi->larb); i++)
			mtk_smi_dbg_print(smi, true, false, i, false);

		for (i = 0; i < ARRAY_SIZE(smi->comm); i++)
			mtk_smi_dbg_print(smi, false, false, i, false);

		for (i = 0; i < ARRAY_SIZE(smi->rsi); i++)
			mtk_smi_dbg_print(smi, true, true, i, false);
	}

	mtk_smi_dump_last_pd(user);

	smi_hang_detect_bw_monitor(false);
	is_hang = smi_bus_hang_check(smi->comm);

	spin_unlock_irqrestore(&smi_lock.lock, smi_lock.flags);

	/* notify to CCF to disable trace dump */
	mtk_clk_notify(NULL, NULL, NULL, 0, 0, 0, CLK_EVT_TRIGGER_TRACE_DUMP);

	if (is_hang) {
		pr_notice("[smi] smi may hang\n");
		BUG_ON(1);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_smi_dbg_hang_detect);

int smi_larb_force_all_on(char *buf, const struct kernel_param *kp)
{
	struct mtk_smi_dbg	*smi = gsmi;
	s32 i, ret;

	for (i = 0; i < ARRAY_SIZE(smi->larb); i++) {
		if (!smi->larb[i].dev)
			continue;
		ret = mtk_smi_larb_get(smi->larb[i].dev);
		if (ret < 0)
			dev_notice(smi->larb[i].dev, "smi_larb%d get fail:%d\n", i, ret);
	}
	smi_force_on = 1;
	pr_notice("[smi] larb force all on\n");
	return 0;
}

static struct kernel_param_ops smi_larb_force_all_on_ops = {
	.get = smi_larb_force_all_on,
};
module_param_cb(smi_force_all_on, &smi_larb_force_all_on_ops, NULL, 0644);
MODULE_PARM_DESC(smi_force_all_on, "smi larb force all on");

int smi_larb_force_all_put(char *buf, const struct kernel_param *kp)
{
	struct mtk_smi_dbg	*smi = gsmi;
	s32 i;

	if (smi_force_on) {
		for (i = 0; i < ARRAY_SIZE(smi->larb); i++) {
			if (!smi->larb[i].dev)
				continue;
			mtk_smi_larb_put(smi->larb[i].dev);
		}
		smi_force_on = 0;
		pr_notice("[smi] larb force all put\n");
	}

	return 0;
}

static struct kernel_param_ops smi_larb_force_all_put_ops = {
	.get = smi_larb_force_all_put,
};
module_param_cb(smi_force_all_put, &smi_larb_force_all_put_ops, NULL, 0644);
MODULE_PARM_DESC(smi_force_all_put, "smi larb force all put");

int smi_bw_monitor_ut(const char *val, const struct kernel_param *kp)
{
	u32 commonlarb_id[MAX_MON_REQ];
	u32 flag[MAX_MON_REQ];
	u32 common_id, i, j, result;
	u32 bw[MAX_MON_REQ] = {0};

	result = sscanf(val, "%d:%d %d %d %d:%d %d %d %d", &common_id,
		&commonlarb_id[0], &commonlarb_id[1], &commonlarb_id[2], &commonlarb_id[3],
		&flag[0], &flag[1], &flag[2], &flag[3]);

	if (result != 9) {
		pr_notice("%s: failed: %d\n", __func__, result);
		return result;
	}

	for (i = 0; i < 4; i++) {
		pr_notice("[smi] bw ut flag%d = %d\n", i, flag[i]);
		pr_notice("[smi] bw ut common_id%d = %d\n", i, commonlarb_id[i]);
	}

	for (i = 0; i < 1000; i++) {
		smi_monitor_start(NULL, common_id, commonlarb_id, flag, SMI_BW_BUS);
		msleep(1);
		smi_monitor_stop(NULL, common_id, bw, SMI_BW_BUS);
		for (j = 0; j < MAX_MON_REQ; j++)
			pr_notice("%s: common_id(%d) bw[%u]=%u\n", __func__, common_id, j, bw[j]);
	}

	return 0;
}

static struct kernel_param_ops smi_bw_monitor_ut_ops = {
	.set = smi_bw_monitor_ut,
};
module_param_cb(smi_bw_monitor_ut, &smi_bw_monitor_ut_ops, NULL, 0644);
MODULE_PARM_DESC(smi_bw_monitor_ut, "smi monitor bw");
