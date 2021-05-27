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
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <soc/mediatek/smi.h>
#include <dt-bindings/memory/mtk-smi-larb-port.h>

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

#define INT_SMI_LARB_CMD_THRT_CON	(0x500 + (SMI_LARB_CMD_THRT_CON))
#define INT_SMI_LARB_DBG_CON		(0x500 + (SMI_LARB_DBG_CON))
#define INT_SMI_LARB_OSTD_MON_PORT(p)	(0x500 + SMI_LARB_OSTD_MON_PORT(p))

#define SMI_LARB_REGS_NR		(162)
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
	INT_SMI_LARB_CMD_THRT_CON, INT_SMI_LARB_DBG_CON,
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

#define SMI_MON_ENA(m)		((m) ? (0x600 + (((m) - 1) * 0x50)) : 0x1a0)
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

struct mtk_smi_dbg_node {
	struct device	*dev;
	void __iomem	*va;
	phys_addr_t	pa;
	struct device	*comm;

	u32	regs_nr;
	u32	*regs;

	u32	mon[SMI_MON_BUS_NR];
};

struct mtk_smi_dbg {
	bool			probe;
	struct dentry		*fs;
	struct mtk_smi_dbg_node	larb[MTK_LARB_NR_MAX];
	struct mtk_smi_dbg_node	comm[MTK_LARB_NR_MAX];
	u64			exec;
	u8			frame;
};
static struct mtk_smi_dbg	*gsmi;

static void mtk_smi_dbg_print(
	struct mtk_smi_dbg *smi, const bool larb, const u32 id)
{
#define PRINT_NR	(5)
	struct mtk_smi_dbg_node	node = larb ? smi->larb[id] : smi->comm[id];
	const char		*name = larb ? "larb" : "common";
	const u32		regs_nr = node.regs_nr;
	const u32		reg = larb ? SMI_LARB_STAT : SMI_DEBUG_MISC;

	char	buf[LINK_MAX + 1] = {0};
	u32	val[PRINT_NR], busy = 0;
	s32	i, j, len, ret;
	bool	diff;

	if (!node.dev || !node.va)
		return;

	ret = pm_runtime_get_if_in_use(node.dev);
	dev_info(node.dev, "===== %pa.%s:%u rpm:%d =====\n",
		&node.pa, name, id, ret);
	if (ret <= 0)
		return;

	for (i = 0, len = 0; i < regs_nr; i++) {
		for (j = 0, diff = false; j < PRINT_NR; j++) {
			val[j] = readl_relaxed(node.va + node.regs[i]);
			if (!diff && j && val[j] != val[j - 1])
				diff = true;
			if (node.regs[i] == reg)
				busy += !(larb ^ val[j]) ? 1 : 0;
		}

		if (diff) {
			dev_info(node.dev, " %#x=%#x %#x %#x %#x %#x\n",
				node.regs[i],
				val[0], val[1], val[2], val[3], val[4]);
			continue;
		}

		ret = snprintf(buf + len, LINK_MAX - len, " %#x=%#x,",
			node.regs[i], val[0]);
		if (ret < 0 || ret >= LINK_MAX - len) {
			snprintf(buf + len, LINK_MAX - len, "%c", '\0');
			dev_info(node.dev, "%s\n", buf);

			len = 0;
			memset(buf, '\0', sizeof(char) * ARRAY_SIZE(buf));
			ret = snprintf(buf + len, LINK_MAX - len, " %#x=%#x,",
				node.regs[i], val[0]);
		}
		len += ret;
	}
	dev_info(node.dev, "===== %pa.%s:%u %s:%d/%d =====\n",
		&node.pa, name, id,
		busy == PRINT_NR ? "busy" : "idle", busy, PRINT_NR);
	pm_runtime_put_sync(node.dev);
}

static void mtk_smi_dbg_hang_detect_single(
	struct mtk_smi_dbg *smi, const bool larb, const u32 id)
{
	struct mtk_smi_dbg_node	node = larb ? smi->larb[id] : smi->comm[id];
	s32			i;

	dev_info(node.dev, "%s: larb:%d id:%u\n", __func__, larb, id);
	mtk_smi_dbg_print(smi, larb, id);
	if (larb)
		for (i = 0; i < ARRAY_SIZE(smi->comm); i++) {
			if (smi->comm[i].dev == smi->larb[id].comm) {
				mtk_smi_dbg_print(smi, !larb, i);
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
	s32	i, larb_on[MTK_LARB_NR_MAX], comm_on[MTK_LARB_NR_MAX];

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
	"mediatek,smi-larb", "mediatek,smi-common"
};

static s32 mtk_smi_dbg_probe(struct mtk_smi_dbg *smi)
{
	//struct device_node	*node = NULL, *comm;
	struct device_node	*node = NULL;
	struct platform_device	*pdev;
	s32			larb_nr = 0, comm_nr = 0, id, ret;


	pr_info("%s: comp[%d]:%s\n", __func__, 0, mtk_smi_dbg_comp[0]);

	for_each_compatible_node(node, NULL, mtk_smi_dbg_comp[0]) {
		if (!node)
			return -EINVAL;

		if (of_property_read_u32(node, "mediatek,larb-id", &id))
			id = larb_nr;
		larb_nr += 1;

		pdev = of_find_device_by_node(node);
		of_node_put(node);
		if (!pdev)
			return -EINVAL;
		smi->larb[id].dev = &pdev->dev;

		ret = mtk_smi_dbg_parse(pdev, smi->larb, true, id);
		if (ret)
			return ret;
	}

	for_each_compatible_node(node, NULL, mtk_smi_dbg_comp[1]) {
		if (!node)
			return -EINVAL;
		if (of_property_read_u32(node, "mediatek,common-id", &id))
			id = comm_nr;
		comm_nr += 1;

		pdev = of_find_device_by_node(node);
		of_node_put(node);
		if (!pdev)
			return -EINVAL;
		smi->comm[id].dev = &pdev->dev;

		ret = mtk_smi_dbg_parse(pdev, smi->comm, false, id);
		if (ret)
			return ret;
	}
	return 0;
}

s32 mtk_smi_dbg_hang_detect(const char *user)
{
	struct mtk_smi_dbg	*smi = gsmi;
	s32			i, ret;

	pr_info("%s: check caller:%s\n", __func__, user);

	if (!smi->probe) {
		ret = mtk_smi_dbg_probe(smi);
		if (ret)
			return ret;

		smi->probe = true;
	}

	for (i = 0; i < ARRAY_SIZE(smi->larb); i++)
		mtk_smi_dbg_print(smi, true, i);

	for (i = 0; i < ARRAY_SIZE(smi->comm); i++)
		mtk_smi_dbg_print(smi, false, i);
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_smi_dbg_hang_detect);

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

	smi = kzalloc(sizeof(*smi), GFP_KERNEL);
	if (!smi)
		return -ENOMEM;
	gsmi = smi;

	smi->fs = debugfs_create_file(
		DRV_NAME, 0444, NULL, smi, &mtk_smi_dbg_fops);
	if (IS_ERR(smi->fs))
		return PTR_ERR(smi->fs);

	mtk_smi_dbg_probe(smi);
	mtk_smi_dbg_hang_detect(DRV_NAME);
	pr_debug("%s: smi:%p fs:%p\n", __func__, smi, smi->fs);
	return 0;
}

int smi_ut_dump_get(char *buf, const struct kernel_param *kp)
{
	struct mtk_smi_dbg	*smi = gsmi;
	s32 i;

	for (i = 0; i < ARRAY_SIZE(smi->larb); i++) {
		if (!smi->larb[i].dev)
			continue;
		pm_runtime_get_sync(smi->larb[i].dev);
	}
	for (i = 0; i < ARRAY_SIZE(smi->comm); i++) {
		if (!smi->comm[i].dev)
			continue;
		pm_runtime_get_sync(smi->comm[i].dev);
	}

	mtk_smi_dbg_hang_detect("SMI UT");

	for (i = 0; i < ARRAY_SIZE(smi->larb); i++) {
		if (!smi->larb[i].dev)
			continue;
		pm_runtime_put_sync(smi->larb[i].dev);
	}
	for (i = 0; i < ARRAY_SIZE(smi->comm); i++) {
		if (!smi->comm[i].dev)
			continue;
		pm_runtime_put_sync(smi->comm[i].dev);
	}

	return 0;
}

static struct kernel_param_ops smi_ut_dump_ops = {
	.get = smi_ut_dump_get,
};
module_param_cb(smi_ut_dump, &smi_ut_dump_ops, NULL, 0644);
MODULE_PARM_DESC(smi_ut_dump, "dump smi current setting");

module_init(mtk_smi_dbg_init);
MODULE_LICENSE("GPL v2");

