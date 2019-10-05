// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Author: Argus Lin <argus.lin@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/mfd/mt6315/registers.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/spmi.h>
#include <linux/pmif.h>
#include <spmi_sw.h>
#include <dt-bindings/spmi/spmi.h>

/*
 * marco
 */
#if defined(CONFIG_MTK_FPGA) || defined(CONFIG_FPGA_EARLY_PORTING)
	#define PMIF_NO_PMIC
	#define PMIF_TIMEOUT
#else
	#define PMIF_TIMEOUT
#endif

#define PMIF_DEBUG
#define PMIF_MONITOR_SUPPORT
/* #define PMIF_MATCH_SUPPORT */

/* macro for PMIF SWINF FSM */
#define PMIF_SWINF_FSM_IDLE		0x00
#define PMIF_SWINF_FSM_REQ		0x02
#define PMIF_SWINF_FSM_WFDLE		0x04
#define PMIF_SWINF_FSM_WFVLDCLR		0x06
#define PMIF_SWINF_INIT_DONE		0x01
#define PMIF_SWINF_SYS_IDLE		0x00
#define PMIF_SWINF_SYS_BUSY		0x01

#define PMIF_GET_SWINF_FSM(x)           ((x>>1)  & 0x00000007)

#define PMIF_CMD_REG_0			0
#define PMIF_CMD_REG			1
#define PMIF_CMD_EXT_REG		2
#define PMIF_CMD_EXT_REG_LONG		3
/* 0: SPI, 1: SPMI */
#define PMIF_PMIFID			1

#define PMIF_CHAN_NUM			17
/* indicate which number SW channel start, by project */
#define PMIF_SWINF_0_CHAN_NO		4
/* MD: 0, security domain: 1, AP: 2 */
#define PMIF_AP_SWINF_NO		2

enum pmif_regs {
	PMIF_INIT_DONE,
	PMIF_INF_EN,
	PMIF_ARB_EN,
	PMIF_CMDISSUE_EN,
	PMIF_TIMER_CTRL,
	PMIF_SPI_MODE_CTRL,
	PMIF_IRQ_EVENT_EN_0,
	PMIF_IRQ_FLAG_0,
	PMIF_IRQ_CLR_0,
	PMIF_SWINF_0_STA,
	PMIF_SWINF_0_WDATA_31_0,
	PMIF_SWINF_0_RDATA_31_0,
	PMIF_SWINF_0_ACC,
	PMIF_SWINF_0_VLD_CLR,
	PMIF_SWINF_1_STA,
	PMIF_SWINF_1_WDATA_31_0,
	PMIF_SWINF_1_RDATA_31_0,
	PMIF_SWINF_1_ACC,
	PMIF_SWINF_1_VLD_CLR,
	PMIF_SWINF_2_STA,
	PMIF_SWINF_2_WDATA_31_0,
	PMIF_SWINF_2_RDATA_31_0,
	PMIF_SWINF_2_ACC,
	PMIF_SWINF_2_VLD_CLR,
	PMIF_SWINF_3_STA,
	PMIF_SWINF_3_WDATA_31_0,
	PMIF_SWINF_3_RDATA_31_0,
	PMIF_SWINF_3_ACC,
	PMIF_SWINF_3_VLD_CLR,
};

static const u32 mt6885_regs[] = {
	[PMIF_INIT_DONE] =			0x0000,
	[PMIF_INF_EN] =				0x0024,
	[PMIF_ARB_EN] =				0x0150,
	[PMIF_CMDISSUE_EN] =			0x03B4,
	[PMIF_TIMER_CTRL] =			0x03E0,
	[PMIF_SPI_MODE_CTRL] =			0x0400,
	[PMIF_IRQ_EVENT_EN_0] =                 0x0418,
	[PMIF_IRQ_FLAG_0] =                     0x0420,
	[PMIF_IRQ_CLR_0] =                      0x0424,
	[PMIF_SWINF_0_ACC] =			0x0C00,
	[PMIF_SWINF_0_WDATA_31_0] =		0x0C04,
	[PMIF_SWINF_0_RDATA_31_0] =		0x0C14,
	[PMIF_SWINF_0_VLD_CLR] =		0x0C24,
	[PMIF_SWINF_0_STA] =			0x0C28,
	[PMIF_SWINF_1_ACC] =			0x0C40,
	[PMIF_SWINF_1_WDATA_31_0] =		0x0C44,
	[PMIF_SWINF_1_RDATA_31_0] =		0x0C54,
	[PMIF_SWINF_1_VLD_CLR] =		0x0C64,
	[PMIF_SWINF_1_STA] =			0x0C68,
	[PMIF_SWINF_2_ACC] =			0x0C80,
	[PMIF_SWINF_2_WDATA_31_0] =		0x0C84,
	[PMIF_SWINF_2_RDATA_31_0] =		0x0C94,
	[PMIF_SWINF_2_VLD_CLR] =		0x0CA4,
	[PMIF_SWINF_2_STA] =			0x0CA8,
	[PMIF_SWINF_3_ACC] =			0x0CC0,
	[PMIF_SWINF_3_WDATA_31_0] =		0x0CC4,
	[PMIF_SWINF_3_RDATA_31_0] =		0x0CD4,
	[PMIF_SWINF_3_VLD_CLR] =		0x0CE4,
	[PMIF_SWINF_3_STA] =			0x0CE8,
};

enum spmi_regs {
	SPMI_OP_ST_CTRL,
	SPMI_GRP_ID_EN,
	SPMI_OP_ST_STA,
	SPMI_MST_SAMPL,
	SPMI_MST_REQ_EN,
	SPMI_REC_CTRL,
	SPMI_REC0,
	SPMI_REC1,
	SPMI_REC2,
	SPMI_REC3,
	SPMI_REC4,
	SPMI_MST_DBG,
	/* RCS support */
	SPMI_MST_RCS_CTRL,
	SPMI_MST_IRQ,
	SPMI_SLV_3_0_EINT,
	SPMI_SLV_7_4_EINT,
	SPMI_SLV_B_8_EINT,
	SPMI_SLV_F_C_EINT,
	SPMI_TIA,
	SPMI_DEC_DBG,
};

static const u32 mt6885_spmi_regs[] = {
	[SPMI_OP_ST_CTRL] =			0x0000,
	[SPMI_GRP_ID_EN] =			0x0004,
	[SPMI_OP_ST_STA] =			0x0008,
	[SPMI_MST_SAMPL] =			0x000c,
	[SPMI_MST_REQ_EN] =			0x0010,
	[SPMI_REC_CTRL] =			0x0040,
	[SPMI_REC0] =				0x0044,
	[SPMI_REC1] =				0x0048,
	[SPMI_REC2] =				0x004c,
	[SPMI_REC3] =				0x0050,
	[SPMI_REC4] =				0x0054,
	[SPMI_MST_DBG] =			0x00fc,
};

enum {
	SPMI_OP_ST_BUSY = 1,
	SPMI_OP_ST_ACK = 0,
	SPMI_OP_ST_NACK = 1
};

enum {
	SPMI_RCS_SR_BIT,
	SPMI_RCS_A_BIT
};

enum {
	SPMI_RCS_MST_W = 1,
	SPMI_RCS_SLV_W = 3
};
#if defined(CONFIG_MTK_FPGA) || defined(CONFIG_FPGA_EARLY_PORTING)
enum infra_regs {
	MODULE_SW_CG_0_SET,
	MODULE_SW_CG_0_CLR,
	MODULE_SW_CG_2_SET,
	MODULE_SW_CG_2_CLR,
	INFRA_GLOBALCON_RST2_SET,
	INFRA_GLOBALCON_RST2_CLR,
};

static const u32 mt6885_infra_regs[] = {
	[MODULE_SW_CG_0_SET] =			0x0080,
	[MODULE_SW_CG_0_CLR] =			0x0084,
	[MODULE_SW_CG_2_SET] =			0x00a4,
	[MODULE_SW_CG_2_CLR] =			0x00a8,
	[INFRA_GLOBALCON_RST2_SET] =		0x0140,
	[INFRA_GLOBALCON_RST2_CLR] =		0x0144,
};

enum topckgen_regs {
	CLK_CFG_UPDATE1,
	CLK_CFG_UPDATE2,
	WDT_SWSYSRST2,
	CLK_CFG_8_CLR,
	CLK_CFG_16_CLR,
};

static const u32 mt6885_topckgen_regs[] = {
	[CLK_CFG_UPDATE1] =			0x0008,
	[CLK_CFG_UPDATE2] =			0x000c,
	[WDT_SWSYSRST2] =			0x0090,
	[CLK_CFG_8_CLR] =			0x0098,
	[CLK_CFG_16_CLR] =			0x0118,
};
#endif

#if !defined PMIF_NO_PMIC
/*
 * pmif internal API declaration
 */
int __attribute__((weak)) pmif_probe_rw_test(struct spmi_controller *ctrl);
int __attribute__((weak)) spmi_dump_dbg_register(struct spmi_controller *ctrl);
static int pmif_timeout_ns(struct spmi_controller *ctrl,
	unsigned long long start_time_ns, unsigned long long timeout_time_ns);
static void pmif_enable_soft_reset(struct pmif *arb);
static void pmif_spmi_enable_clk_set(struct pmif *arb);
static void pmif_spmi_force_normal_mode(struct pmif *arb);
static void pmif_spmi_enable_swinf(struct pmif *arb,
	unsigned int chan_no, unsigned int swinf_no);
static void pmif_spmi_enable_cmdIssue(struct pmif *arb, bool en);
static void pmif_spmi_enable(struct pmif *arb);
static int is_pmif_spmi_init_done(struct pmif *arb);
static int pmif_spmi_read_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid,
		u16 addr, u8 *buf, size_t len);
static int pmif_spmi_write_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid,
		u16 addr, const u8 *buf, size_t len);
static int mtk_spmi_config_master(struct pmif *arb,
		unsigned int mstid, bool en);
static int mtk_spmi_cali_rd_clock_polarity(struct pmif *arb,
			unsigned int mstid);
#ifdef SPMI_RCS_SUPPORT
static int mtk_spmi_enable_rcs(struct spmi_controller *ctrl,
			unsigned int mstid);
static int mtk_spmi_read_eint_sta(u8 *slv_eint_sta);
#endif
static int mtk_spmi_ctrl_op_st(struct spmi_controller *ctrl,
			unsigned int grpiden, u8 sid, unsigned int opc);

static struct pmif mt6885_pmif_arb[] = {
	{
		.base = 0x0,
		.regs = mt6885_regs,
		.spmimst_base = 0x0,
		.spmimst_regs = mt6885_spmi_regs,
		.infra_base = 0x0,
		.infra_regs = mt6885_infra_regs,
		.topckgen_base = 0x0,
		.topckgen_regs = mt6885_topckgen_regs,
		.swinf_ch_start = PMIF_SWINF_0_CHAN_NO,
		.ap_swinf_no = PMIF_AP_SWINF_NO,
		.write = 0x0,
		.pmifid = PMIF_PMIFID,
		.spmic = 0x0,
		.read_cmd = pmif_spmi_read_cmd,
		.write_cmd = pmif_spmi_write_cmd,
		.pmif_enable_clk_set = pmif_spmi_enable_clk_set,
		.pmif_force_normal_mode = pmif_spmi_force_normal_mode,
		.pmif_enable_swinf = pmif_spmi_enable_swinf,
		.pmif_enable_cmdIssue = pmif_spmi_enable_cmdIssue,
		.pmif_enable = pmif_spmi_enable,
		.is_pmif_init_done = is_pmif_spmi_init_done,
	},
};

static const struct of_device_id pmif_match_table[] = {
	{
		.compatible = "mediatek,mt6885-pmif",
		.data = &mt6885_pmif_arb,
	}, {
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, pmif_match_table);

/*
 * pmif timeout
 */
#ifdef PMIF_TIMEOUT
static int pmif_timeout_ns(struct spmi_controller *ctrl,
	unsigned long long start_time_ns, unsigned long long timeout_time_ns)
{
	unsigned long long cur_time = 0;
	unsigned long long elapse_time = 0;

	/* get current tick */
	cur_time = sched_clock();  /* ns */

	/* avoid timer over flow exiting in FPGA env */
	if (cur_time < start_time_ns)
		start_time_ns = cur_time;

	elapse_time = cur_time - start_time_ns;

	/* check if timeout */
	if (timeout_time_ns <= elapse_time) {
		dev_notice(&ctrl->dev,
			"[PMIF] Timeout start time: %lld\n", start_time_ns);
		dev_notice(&ctrl->dev,
			"[PMIF] Timeout cur time: %lld\n", cur_time);
		dev_notice(&ctrl->dev,
			"[PMIF] Timeout elapse time: %lld\n", elapse_time);
		dev_notice(&ctrl->dev,
			"[PMIF] Timeout set timeout: %lld\n", timeout_time_ns);
		return 1;
	}
	return 0;
}
#else
static int pmif_timeout_ns(unsigned long long start_time_ns,
		unsigned long long timeout_time_ns)
{
	return 0;
}

#endif
/*
 * pmif define for FSM
 */
static inline bool pmif_is_fsm_idle(struct pmif *arb)
{
	u32 offset = 0, reg_rdata = 0;

	offset = arb->regs[PMIF_SWINF_0_STA] + (0x40 * arb->ap_swinf_no);
	reg_rdata = readl(arb->base + offset);
	return PMIF_GET_SWINF_FSM(reg_rdata) == PMIF_SWINF_FSM_IDLE;
}

static inline bool pmif_is_fsm_vldclr(struct pmif *arb)
{
	u32 offset = 0, reg_rdata = 0;

	offset = arb->regs[PMIF_SWINF_0_STA] + (0x40 * arb->ap_swinf_no);
	reg_rdata = readl(arb->base + offset);
	return PMIF_GET_SWINF_FSM(reg_rdata) == PMIF_SWINF_FSM_WFVLDCLR;
}
static inline void pmif_leave_fsm_vldclr(struct pmif *arb)
{
	u32 offset = 0;

	offset = arb->regs[PMIF_SWINF_0_VLD_CLR] + (0x40 * arb->ap_swinf_no);
	if (pmif_is_fsm_vldclr(arb))
		writel(0x1, arb->base + offset);
}

static int pmif_wait_for_state(struct spmi_controller *ctrl,
		bool (*fp)(struct pmif *))
{
	struct pmif *arb = spmi_controller_get_drvdata(ctrl);
	unsigned long long start_time_ns = 0, timeout_ns = 0;

	start_time_ns = sched_clock();
	timeout_ns = 10000 * 1000;  /* 10000us */

	do {
		if (pmif_timeout_ns(ctrl, start_time_ns, timeout_ns)) {
			if (fp(arb)) {
				return 0;
			} else if (fp(arb) == 0) {
				dev_notice(&ctrl->dev, "[PMIF] FSM Timeout\n");
				return -ETIMEDOUT;
			}
		}
		if (fp(arb))
			return 0;
	} while (1);
}

/*
 * Function : pmif_readl()
 * Description : mtk pmif controller read api
 * Parameter :
 * Return :
 */
static u32 pmif_readl(struct pmif *arb, enum pmif_regs reg)
{
	return readl(arb->base + arb->regs[reg]);
}

/*
 * Function : pmif_writel()
 * Description : mtk pmif controller write api
 * Parameter :
 * Return :
 */
static void pmif_writel(struct pmif *arb, u32 val, enum pmif_regs reg)
{
	writel(val, arb->base + arb->regs[reg]);
}
/*
 * Function : mtk_spmi_readl()
 * Description : mtk spmi controller read api
 * Parameter :
 * Return :
 */
static u32 mtk_spmi_readl(struct pmif *arb, enum spmi_regs reg)
{
	return readl(arb->spmimst_base + arb->spmimst_regs[reg]);
}

/*
 * Function : mtk_spmi_writel()
 * Description : mtk spmi controller write api
 * Parameter :
 * Return :
 */
static void mtk_spmi_writel(struct pmif *arb, u32 val,
		enum spmi_regs reg)
{
	writel(val, arb->spmimst_base + arb->spmimst_regs[reg]);
}

static void pmif_enable_soft_reset(struct pmif *arb)
{
#if defined(CONFIG_FPGA_EARLY_PORTING)
	writel(0x1 << 14,
		arb->infra_base + arb->infra_regs[INFRA_GLOBALCON_RST2_SET]);
	writel(0x1 << 14,
		arb->infra_base + arb->infra_regs[INFRA_GLOBALCON_RST2_CLR]);
#endif
}

static void pmif_spmi_enable_clk_set(struct pmif *arb)
{
#if defined(CONFIG_FPGA_EARLY_PORTING)
	writel((0x1 << 15) | (0x1 << 12) | (0x7 << 8),
		arb->topckgen_base + arb->topckgen_regs[CLK_CFG_8_CLR]);
	writel(0x1 << 2,
		arb->topckgen_base + arb->topckgen_regs[CLK_CFG_UPDATE1]);

	/* sys_ck cg enable, turn off clock */
	writel(0x0000000f,
		arb->infra_base + arb->infra_regs[MODULE_SW_CG_0_SET]);
	/* turn off clock */
	writel(0x00000100,
		arb->infra_base + arb->infra_regs[MODULE_SW_CG_2_SET]);

	/* toggle SPMI sw reset */
	pmif_enable_soft_reset(arb);

	/* sys_ck cg enable, turn on clock */
	writel(0x0000000f,
		arb->infra_base + arb->infra_regs[MODULE_SW_CG_0_CLR]);
	/* turn on clock */
	writel(0x00000100,
		arb->infra_base + arb->infra_regs[MODULE_SW_CG_2_CLR]);
#endif
}

static void pmif_spmi_force_normal_mode(struct pmif *arb)
{
	/* Force SPMI in normal mode. */
	pmif_writel(arb, pmif_readl(arb, PMIF_SPI_MODE_CTRL) & (~(0x3 << 9)),
			PMIF_SPI_MODE_CTRL);
	pmif_writel(arb, pmif_readl(arb, PMIF_SPI_MODE_CTRL) | (0x1 << 9),
			PMIF_SPI_MODE_CTRL);

}

static void pmif_spmi_enable_swinf(struct pmif *arb, unsigned int chan_no,
	unsigned int swinf_no)
{
	/* Enable swinf */
	pmif_writel(arb, 0x1 << (chan_no + swinf_no), PMIF_INF_EN);

	/* Enable arbitration */
	pmif_writel(arb, 0x1 << (chan_no + swinf_no), PMIF_ARB_EN);

}

static void pmif_spmi_enable_cmdIssue(struct pmif *arb, bool en)
{
	/* Enable cmdIssue */
	pmif_writel(arb, en, PMIF_CMDISSUE_EN);

}

static void pmif_spmi_enable(struct pmif *arb)
{
	pmif_writel(arb, 0x2F5, PMIF_INF_EN);
	pmif_writel(arb, 0x2F5, PMIF_ARB_EN);
	pmif_writel(arb, 0x3, PMIF_TIMER_CTRL);
	pmif_writel(arb, 0x1, PMIF_INIT_DONE);

}

static int mtk_spmi_ctrl_op_st(struct spmi_controller *ctrl,
		unsigned int grpiden, u8 sid, unsigned int opc)
{
	struct pmif *arb = spmi_controller_get_drvdata(ctrl);
	u32 rdata = 0x0;
	u8 cmd = 0;

	/* Check the opcode */
	if (opc == SPMI_CMD_RESET)
		cmd = 0;
	else if (opc == SPMI_CMD_SLEEP)
		cmd = 1;
	else if (opc == SPMI_CMD_SHUTDOWN)
		cmd = 2;
	else if (opc == SPMI_CMD_WAKEUP)
		cmd = 3;

	/* gid is 0x800 */
	mtk_spmi_writel(arb, grpiden, SPMI_GRP_ID_EN);
#if MT6315_EVB
	if (grpiden == 0x800)
		mtk_spmi_writel(arb, (cmd << 0x4) | 0xB, SPMI_OP_ST_CTRL);
#else
	if (grpiden == 0x100)
		mtk_spmi_writel(arb, (cmd << 0x4) | 0x8, SPMI_OP_ST_CTRL);
#endif
	else
		mtk_spmi_writel(arb, (cmd << 0x4) | sid, SPMI_OP_ST_CTRL);

	rdata = mtk_spmi_readl(arb, SPMI_OP_ST_CTRL);
	pr_notice("pmif_ctrl_op_st 0x%x\r\n", rdata);

	do {
		rdata = mtk_spmi_readl(arb, SPMI_OP_ST_STA);
		pr_notice("pmif_ctrl_op_st 0x%x\r\n", rdata);

		if (((rdata >> 0x1) & SPMI_OP_ST_NACK) == SPMI_OP_ST_NACK) {
			spmi_dump_dbg_register(ctrl);
			break;
		}
	} while ((rdata & SPMI_OP_ST_BUSY) == SPMI_OP_ST_BUSY);

	return 0;
}

/* Non-data command */
static int pmif_arb_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid)
{
	u8 usid = 0;
	struct pmif *arb = spmi_controller_get_drvdata(ctrl);

#if MT6315_EVB
	if (arb->grpiden != 0x800)
		usid = arb->grpiden;
#else
	if (arb->grpiden != 0x100)
		usid = arb->grpiden;
#endif
	return mtk_spmi_ctrl_op_st(ctrl, arb->grpiden, usid, opc);
}

int mtk_spmi_enable_group_id(struct spmi_controller *ctrl,
			unsigned int grpiden)
{
	struct pmif *arb = spmi_controller_get_drvdata(ctrl);

	mtk_spmi_writel(arb, arb->grpiden, SPMI_GRP_ID_EN);

	return 0;
}

static int pmif_spmi_read_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid,
		u16 addr, u8 *buf, size_t len)
{
	struct pmif *arb = spmi_controller_get_drvdata(ctrl);
	int ret = 0, write = 0x0;
	u32 offset = 0;
	u8 bc = len - 1;
	unsigned long flags;

	/* Check for argument validation. */
	if ((arb->ap_swinf_no & ~(0x3)) != 0x0)
		return -EINVAL;

	if ((arb->pmifid & ~(0x1)) != 0x0)
		return -EINVAL;

	if ((sid & ~(0xf)) != 0x0)
		return -EINVAL;

	if ((addr & ~(0xffff)) != 0x0)
		return -EINVAL;

	if ((*buf & ~(0xffff)) != 0x0)
		return -EINVAL;

	if ((bc & ~(0x1)) != 0x0)
		return -EINVAL;

	/* Check the opcode */
	if (opc >= 0x60 && opc <= 0x7F)
		opc = PMIF_CMD_REG;
	else if (opc >= 0x20 && opc <= 0x2F)
		opc = PMIF_CMD_EXT_REG;
	else if (opc >= 0x38 && opc <= 0x3F)
		opc = PMIF_CMD_EXT_REG_LONG;
	else
		return -EINVAL;

	raw_spin_lock_irqsave(&arb->lock, flags);
	/* Wait for Software Interface FSM state to be IDLE. */
	ret = pmif_wait_for_state(ctrl, pmif_is_fsm_idle);
	if (ret) {
		pmif_leave_fsm_vldclr(arb);
		raw_spin_unlock_irqrestore(&arb->lock, flags);
		return ret;
	}
	/* Send the command. */
	offset = arb->regs[PMIF_SWINF_0_ACC] + (0x40 * arb->ap_swinf_no);
	writel((opc << 30) | (write << 29) | (sid << 24) | (bc << 16) | addr,
		arb->base + offset);

	/* Wait for Software Interface FSM state to be WFVLDCLR,
	 *
	 * read the data and clear the valid flag.
	 */
	if (write == 0) {
		ret = pmif_wait_for_state(ctrl, pmif_is_fsm_vldclr);
		if (ret) {
			raw_spin_unlock_irqrestore(&arb->lock, flags);
			return ret;
		}
		offset =
		arb->regs[PMIF_SWINF_0_RDATA_31_0] + (0x40 * arb->ap_swinf_no);

		*buf = readl(arb->base + offset);

		offset =
		arb->regs[PMIF_SWINF_0_VLD_CLR] + (0x40 * arb->ap_swinf_no);

		writel(0x1, arb->base + offset);
	}

	raw_spin_unlock_irqrestore(&arb->lock, flags);

	return 0x0;
}

static int pmif_spmi_write_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid,
		u16 addr, const u8 *buf, size_t len)
{
	struct pmif *arb = spmi_controller_get_drvdata(ctrl);
	int ret = 0, write = 0x1;
	u32 offset = 0;
	u8 bc = len - 1;
	unsigned long flags;

	/* Check for argument validation. */
	if ((arb->ap_swinf_no & ~(0x3)) != 0x0)
		return -EINVAL;

	if ((arb->pmifid & ~(0x1)) != 0x0)
		return -EINVAL;

	if ((sid & ~(0xf)) != 0x0)
		return -EINVAL;

	if ((addr & ~(0xffff)) != 0x0)
		return -EINVAL;

	if ((*buf & ~(0xffff)) != 0x0)
		return -EINVAL;

	if ((bc & ~(0x1)) != 0x0)
		return -EINVAL;

	/* Check the opcode */
	if (opc >= 0x40 && opc <= 0x5F)
		opc = PMIF_CMD_REG;
	else if (opc <= 0x0F)
		opc = PMIF_CMD_EXT_REG;
	else if (opc >= 0x30 && opc <= 0x37)
		opc = PMIF_CMD_EXT_REG_LONG;
	else if (opc >= 0x80)
		opc = PMIF_CMD_REG_0;
	else
		return -EINVAL;

	raw_spin_lock_irqsave(&arb->lock, flags);
	/* Wait for Software Interface FSM state to be IDLE. */
	ret = pmif_wait_for_state(ctrl, pmif_is_fsm_idle);
	if (ret) {
		pmif_leave_fsm_vldclr(arb);
		raw_spin_unlock_irqrestore(&arb->lock, flags);
		return ret;
	}
	/* Set the write data. */
	offset = arb->regs[PMIF_SWINF_0_WDATA_31_0] + (0x40 * arb->ap_swinf_no);
	if (write == 1)
		writel(*buf, arb->base + offset);

	/* Send the command. */
	offset = arb->regs[PMIF_SWINF_0_ACC] + (0x40 * arb->ap_swinf_no);
	writel((opc << 30) | (write << 29) | (sid << 24) | (bc << 16) | addr,
			arb->base + offset);
	raw_spin_unlock_irqrestore(&arb->lock, flags);

	return 0x0;
}

int is_pmif_spmi_init_done(struct pmif *arb)
{
	int ret = 0;

	ret = pmif_readl(arb, PMIF_INIT_DONE);
	if ((ret & 0x1) == 1)
		return 0;

	return -1;
}

#ifdef SPMI_RCS_SUPPORT
static int mtk_spmi_enable_rcs(struct spmi_controller *ctrl, unsigned int mstid)
{
	u8 wdata = 0, rdata = 0, i = 0;

	/* config match mode */
	wdata = 0x2;
	spmi_ext_register_writel(dev, MT6315_PMIC_RG_INT_RCS1_ADDR, &wdata);
	spmi_ext_register_readl(dev, MT6315_PMIC_RG_INT_RCS1_ADDR, &rdata);
	pr_notice("usid:%x After set RG_NT_RCS1[0x%x]=0x%x\r\n",
			dev->usid, MT6315_PMIC_RG_INT_RCS1_ADDR, rdata);

	spmi_ext_register_readl(dev, MT6315_PMIC_RG_RCS_ID_ADDR, &rdata);
	wdata = rdata & 0xf;
	spmi_ext_register_writel(dev, MT6315_PMIC_RG_RCS_ID_ADDR, &wdata);
	spmi_ext_register_readl(dev, MT6315_PMIC_RG_RCS_ID_ADDR, &rdata);
	pr_notice("usid:%x After set ID[0x%x]=0x%x\r\n",
			dev->usid, MT6315_PMIC_RG_RCS_ID_ADDR, rdata);
	wdata = rdata | (SPMI_RCS_A_BIT << 0x1);
	spmi_ext_register_writel(dev, MT6315_PMIC_RG_RCS_ABIT_ADDR, &wdata);
	spmi_ext_register_readl(dev, MT6315_PMIC_RG_RCS_ABIT_ADDR, &rdata);
	pr_notice("usid:%x After set ABIT[0x%x]=0x%x\r\n",
			dev->usid, MT6315_PMIC_RG_RCS_ABIT_ADDR, rdata);
	spmi_ext_register_readl(dev, MT6315_PMIC_RG_RCS_CMD_ADDR, &rdata);
	wdata = rdata | (SPMI_RCS_MST_W << MT6315_PMIC_RG_RCS_CMD_SHIFT);
	spmi_ext_register_writel(dev, MT6315_PMIC_RG_RCS_CMD_ADDR, &wdata);
	spmi_ext_register_readl(dev, MT6315_PMIC_RG_RCS_CMD_ADDR, &rdata);
	pr_notice("usid:%x After set CMD[0x%x]=0x%x\r\n",
			dev->usid, MT6315_PMIC_RG_RCS_CMD_ADDR, rdata);

	wdata = 0xF8;
	spmi_ext_register_writel(dev, MT6315_PMIC_RG_RCS_ADDR_ADDR, &wdata);
	spmi_ext_register_readl(dev, MT6315_PMIC_RG_RCS_ADDR_ADDR, &rdata);
	pr_notice("usid:%x After set RCS_ADDR[0x%x]=0x%x\r\n",
			dev->usid, MT6315_PMIC_RG_RCS_ADDR_ADDR, rdata);

	spmi_ext_register_readl(dev, MT6315_PMIC_RG_RCS_ENABLE_ADDR, &rdata);
	wdata = rdata | 0x1;
	spmi_ext_register_writel(dev, MT6315_PMIC_RG_RCS_ENABLE_ADDR, &wdata);
	spmi_ext_register_readl(dev, MT6315_PMIC_RG_RCS_ENABLE_ADDR, &rdata);
	pr_notice("usid:%x After set Enable[0x%x]=0x%x\r\n",
			dev->usid, MT6315_PMIC_RG_RCS_ENABLE_ADDR, rdata);

	wdata = (0x1 << MT6315_PMIC_RG_INT_EN_RCS0_SHIFT) |
		(0x1 << MT6315_PMIC_RG_INT_EN_RCS1_SHIFT);
	spmi_ext_register_writel(dev, MT6315_PMIC_RG_INT_EN_RCS0_ADDR, &wdata);
	spmi_ext_register_readl(dev, MT6315_PMIC_RG_INT_EN_RCS0_ADDR, &rdata);
	pr_notice("usid:%x After set RG_INT_EN[0x%x]=0x%x\r\n",
			dev->usid, MT6315_PMIC_RG_INT_EN_RCS0_ADDR, rdata);

	wdata = 0x0;
	spmi_ext_register_writel(dev,
			MT6315_PMIC_RG_INT_MASK_RCS0_ADDR, &wdata);
	spmi_ext_register_readl(dev,
			MT6315_PMIC_RG_INT_MASK_RCS0_ADDR, &rdata);
	pr_notice("usid:%x After set RG_INT_MASK[0x%x]=0x%x\r\n",
			dev->usid, MT6315_PMIC_RG_INT_MASK_RCS0_ADDR, rdata);
}

static int mtk_spmi_read_eint_sta(struct pmif *arb, u8 *slv_eint_sta)
{
	u8 offset = 0, j = 0;
	u32 rdata = 0, regs = 0;

	for (offset = 0; offset < 4; offset++) {
		regs = arb->spmimst_regs[SPMI_SLV_3_0_EINT] + (offset*4);
		rdata = readl(arb->spmimst_base + regs);
		*(slv_eint_sta + j) = rdata & 0xff;
		*(slv_eint_sta + j+1) = (rdata >> 8) & 0xff;
		*(slv_eint_sta + j+2) = (rdata >> 16) & 0xff;
		*(slv_eint_sta + j+3) = (rdata >> 24) & 0xff;
		j += 4;
	}

	for (offset = 0; offset < 16; offset++) {
		pr_notice("mtk-spmi%d, slv_eint_sta[0x%x]\r\n",
				offset, *(slv_eint_sta + offset));
	}
	mtk_spmi_writel(arb, 0xffffffff, SPMI_SLV_3_0_EINT);
	mtk_spmi_writel(arb, 0xffffffff, SPMI_SLV_7_4_EINT);
	mtk_spmi_writel(arb, 0xffffffff, SPMI_SLV_B_8_EINT);
	mtk_spmi_writel(arb, 0xffffffff, SPMI_SLV_F_C_EINT);

	rdata = mtk_spmi_readl(arb, SPMI_DEC_DBG);
	pr_notice("%s, [0x%x]=0x%x\r\n", __func__,
			arb->spmimst_regs[SPMI_DEC_DBG], rdata);
}
#endif

static int mtk_spmi_config_master(struct pmif *arb,
		unsigned int mstid, bool en)
{
#if defined(CONFIG_FPGA_EARLY_PORTING)
	/* Software reset */
	writel(0x85 << 24 | 0x1 << 4,
		arb->topckgen_base + arb->topckgen_regs[WDT_SWSYSRST2]);

	writel(0x7 | (0x1 << 4) | (0x1 << 7),
		arb->topckgen_base + arb->topckgen_regs[CLK_CFG_16_CLR]);
	writel(0x1 << 2,
		arb->topckgen_base + arb->topckgen_regs[CLK_CFG_UPDATE2]);

	/* Software reset */
	writel(0x85 << 24,
		arb->topckgen_base + arb->topckgen_regs[WDT_SWSYSRST2]);
#endif
	/* Enable SPMI */
	mtk_spmi_writel(arb, en, SPMI_MST_REQ_EN);

	return 0;
}

static int mtk_spmi_cali_rd_clock_polarity(struct pmif *arb,
					unsigned int mstid)
{
#if defined(CONFIG_MTK_FPGA) || defined(CONFIG_FPGA_EARLY_PORTING)
	unsigned int dly = 0, pol = 0;
#else
	unsigned int dly = 1, pol = 1;
#endif

	/* Indicate sampling clock polarity, 1: Positive 0: Negative  */
	mtk_spmi_writel(arb, (dly << 0x1) | pol, SPMI_MST_SAMPL);

	return 0;
}

static int mtk_spmi_init(struct platform_device *pdev, struct pmif *arb)
{
	struct resource *res;
	int err, i;
	u32 grpid = 0;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "spmimst");
	arb->spmimst_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(arb->spmimst_base)) {
		err = PTR_ERR(arb->spmimst_base);
		return err;
	}

	err = of_property_read_u32(pdev->dev.of_node, "grpid", &grpid);
	if (err) {
		dev_dbg(&pdev->dev, "grpid unspecified.\n");
		return -EINVAL;
	}
	arb->grpiden = 1<<grpid;

	mtk_spmi_config_master(arb, SPMI_MASTER_0, true);
	for (i = 0; i < 3; i++) {
		mtk_spmi_cali_rd_clock_polarity(arb, SPMI_MASTER_0);
#ifdef SPMI_RCS_SUPPORT
		/* enable master rcs support */
		mtk_spmi_writel(arb, 0x4, SPMI_MST_RCS_CTRL);
		mtk_spmi_enable_rcs(&spmi_dev[i], SPMI_MASTER_0);
#endif

	}
	pr_notice("%s done\n", __func__);

	return 0;
}


static int pmif_probe(struct platform_device *pdev)
{
#if defined(CONFIG_FPGA_EARLY_PORTING)
	struct device_node *node;
#endif
	const struct of_device_id *of_id =
		of_match_device(pmif_match_table, &pdev->dev);
	struct spmi_controller *ctrl;
	struct pmif *arb;
	struct resource *res;
	u32 swinf_ch_start = 0, ap_swinf_no = 0;
	int err;

	if (!of_id) {
		dev_dbg(&pdev->dev, "Error: No device match found\n");
		return -ENODEV;
	}

	ctrl = spmi_controller_alloc(&pdev->dev, sizeof(*arb));
	if (!ctrl)
		return -ENOMEM;

	/* re-assign of_id->data */
	spmi_controller_set_drvdata(ctrl, (void *)of_id->data);
	arb = spmi_controller_get_drvdata(ctrl);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pmif");
	arb->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(arb->base)) {
		err = PTR_ERR(arb->base);
		goto err_put_ctrl;
	}
	err = of_property_read_u32(pdev->dev.of_node,
			"swinf_ch_start", &swinf_ch_start);
	if (err) {
		dev_dbg(&pdev->dev, "swinf_ch_start unspecified.\n");
		goto err_put_ctrl;
	}
	arb->swinf_ch_start = swinf_ch_start;

	err = of_property_read_u32(pdev->dev.of_node,
			"ap_swinf_no", &ap_swinf_no);
	if (err) {
		dev_dbg(&pdev->dev, "ap_swinf_no unspecified.\n");
		goto err_put_ctrl;
	}
	arb->ap_swinf_no = ap_swinf_no;

	if (arb->is_pmif_init_done(arb) != 0) {
		/* pmif is not initialized, just init once */
#if defined(CONFIG_FPGA_EARLY_PORTING)
		node = of_find_compatible_node(NULL, NULL,
				"mediatek,infracfg_ao");
		arb->infra_base = of_iomap(node, 0);
		dev_dbg(&pdev->dev, "mtk-pmif arb infra ao base:0x%x\n",
				arb->infra_base);

		node = of_find_compatible_node(NULL, NULL,
				"mediatek,topckgen");
		arb->topckgen_base = of_iomap(node, 0);
		dev_dbg(&pdev->dev, "mtk-pmif arb topckgen base:0x%x\n",
				arb->topckgen_base);
#endif
		/* pmif initialize start */
		arb->pmif_enable_clk_set(arb);
		arb->pmif_force_normal_mode(arb);
		/* Enable SWINF and arbitration for AP. */
		arb->pmif_enable_swinf(arb,
				arb->swinf_ch_start, arb->ap_swinf_no);
		arb->pmif_enable_cmdIssue(arb, true);

		arb->pmif_enable(arb);
		arb->is_pmif_init_done(arb);
		/* pmif initialize end */
	}

	raw_spin_lock_init(&arb->lock);

	ctrl->cmd = pmif_arb_cmd;
	ctrl->read_cmd = pmif_spmi_read_cmd;
	ctrl->write_cmd = pmif_spmi_write_cmd;

	if (arb->is_pmif_init_done(arb) == 0) {
		/* pmif already done, call spmi master driver init */
		err = mtk_spmi_init(pdev, arb);
		if (err)
			goto err_put_ctrl;
	}

	pmif_probe_rw_test(ctrl);

	arb->irq = platform_get_irq_byname(pdev, "pmif_irq");
	if (arb->irq < 0) {
		err = arb->irq;
		goto err_put_ctrl;
	}
	platform_set_drvdata(pdev, ctrl);

	err = spmi_controller_add(ctrl);
	if (err)
		goto err_domain_remove;

	return 0;

err_domain_remove:
err_put_ctrl:
	spmi_controller_put(ctrl);
	return err;
}

static int pmif_remove(struct platform_device *pdev)
{
	struct spmi_controller *ctrl = platform_get_drvdata(pdev);

	spmi_controller_remove(ctrl);
	spmi_controller_put(ctrl);
	return 0;
}

static struct platform_driver pmif_driver = {
	.probe		= pmif_probe,
	.remove		= pmif_remove,
	.driver		= {
		.name	= "pmif",
		.of_match_table = of_match_ptr(pmif_match_table),
	},
};

static int __init mtk_pmif_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&pmif_driver);
	if (ret)
		return -ENODEV;
	return 0;
}
postcore_initcall(mtk_pmif_init);

/* Module information */
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIF module");
MODULE_AUTHOR("Argus Lin <argus.lin@mediatek.com>");
MODULE_ALIAS("platform:pmif");
#endif /* endif SPMI_NO_PMIC */
