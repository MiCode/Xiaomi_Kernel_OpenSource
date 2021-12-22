// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.

#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/spmi.h>
#include <linux/irq.h>

#include "spmi-mtk.h"
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <aee.h>
#endif

#define SWINF_IDLE	0x00
#define SWINF_WFVLDCLR	0x06

#define GET_SWINF(x)	(((x) >> 1) & 0x7)

#define PMIF_CMD_REG_0		0
#define PMIF_CMD_REG		1
#define PMIF_CMD_EXT_REG	2
#define PMIF_CMD_EXT_REG_LONG	3

/* 0: SPMI-M, 1: SPMI-P */
#define PMIF_PMIFID_SPMI0		0
#define PMIF_PMIFID_SPMI1		1

#define PMIF_DELAY_US   10
//#define PMIF_TIMEOUT    (10 * 1000)
#define PMIF_TIMEOUT    (100 * 1000)

#define PMIF_CHAN_OFFSET 0x5

#define SPMI_OP_ST_BUSY 1

#define PMIF_IRQDESC(name) { #name, pmif_##name##_irq_handler, -1}
enum {
	SPMI_MASTER_0 = 0,
	SPMI_MASTER_1,
	SPMI_MASTER_2,
};

struct pmif_irq_desc {
	const char *name;
	irq_handler_t irq_handler;
	int irq;
};


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
	PMIF_IRQ_EVENT_EN_1,
	PMIF_IRQ_FLAG_1,
	PMIF_IRQ_CLR_1,
	PMIF_IRQ_EVENT_EN_2,
	PMIF_IRQ_FLAG_2,
	PMIF_IRQ_CLR_2,
	PMIF_IRQ_EVENT_EN_3,
	PMIF_IRQ_FLAG_3,
	PMIF_IRQ_CLR_3,
	PMIF_IRQ_EVENT_EN_4,
	PMIF_IRQ_FLAG_4,
	PMIF_IRQ_CLR_4,
	PMIF_WDT_EVENT_EN_0,
	PMIF_WDT_FLAG_0,
	PMIF_WDT_EVENT_EN_1,
	PMIF_WDT_FLAG_1,
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
static const u32 mt6xxx_regs[] = {
	[PMIF_INIT_DONE] =			0x0000,
	[PMIF_INF_EN] =				0x0024,
	[PMIF_ARB_EN] =				0x0150,
	[PMIF_CMDISSUE_EN] =			0x03B8,
	[PMIF_TIMER_CTRL] =			0x03E4,
	[PMIF_SPI_MODE_CTRL] =			0x0408,
	[PMIF_IRQ_EVENT_EN_0] =			0x0420,
	[PMIF_IRQ_FLAG_0] =			0x0428,
	[PMIF_IRQ_CLR_0] =			0x042C,
	[PMIF_IRQ_EVENT_EN_1] =			0x0430,
	[PMIF_IRQ_FLAG_1] =			0x0438,
	[PMIF_IRQ_CLR_1] =			0x043C,
	[PMIF_IRQ_EVENT_EN_2] =			0x0440,
	[PMIF_IRQ_FLAG_2] =			0x0448,
	[PMIF_IRQ_CLR_2] =			0x044C,
	[PMIF_IRQ_EVENT_EN_3] =			0x0450,
	[PMIF_IRQ_FLAG_3] =			0x0458,
	[PMIF_IRQ_CLR_3] =			0x045C,
	[PMIF_IRQ_EVENT_EN_4] =			0x0460,
	[PMIF_IRQ_FLAG_4] =			0x0468,
	[PMIF_IRQ_CLR_4] =			0x046C,
	[PMIF_WDT_EVENT_EN_0] =			0x0474,
	[PMIF_WDT_FLAG_0] =			0x0478,
	[PMIF_WDT_EVENT_EN_1] =			0x047C,
	[PMIF_WDT_FLAG_1] =			0x0480,
	[PMIF_SWINF_0_ACC] =			0x0800,
	[PMIF_SWINF_0_WDATA_31_0] =		0x0804,
	[PMIF_SWINF_0_RDATA_31_0] =		0x0814,
	[PMIF_SWINF_0_VLD_CLR] =		0x0824,
	[PMIF_SWINF_0_STA] =			0x0828,
	[PMIF_SWINF_1_ACC] =			0x0840,
	[PMIF_SWINF_1_WDATA_31_0] =		0x0844,
	[PMIF_SWINF_1_RDATA_31_0] =		0x0854,
	[PMIF_SWINF_1_VLD_CLR] =		0x0864,
	[PMIF_SWINF_1_STA] =			0x0868,
	[PMIF_SWINF_2_ACC] =			0x0880,
	[PMIF_SWINF_2_WDATA_31_0] =		0x0884,
	[PMIF_SWINF_2_RDATA_31_0] =		0x0894,
	[PMIF_SWINF_2_VLD_CLR] =		0x08A4,
	[PMIF_SWINF_2_STA] =			0x08A8,
	[PMIF_SWINF_3_ACC] =			0x08C0,
	[PMIF_SWINF_3_WDATA_31_0] =		0x08C4,
	[PMIF_SWINF_3_RDATA_31_0] =		0x08D4,
	[PMIF_SWINF_3_VLD_CLR] =		0x08E4,
	[PMIF_SWINF_3_STA] =			0x08E8,
};

static const u32 mt6833_regs[] = {
	[PMIF_INIT_DONE] =			0x0000,
	[PMIF_INF_EN] =				0x0024,
	[PMIF_ARB_EN] =				0x0150,
	[PMIF_CMDISSUE_EN] =			0x03B8,
	[PMIF_TIMER_CTRL] =			0x03E4,
	[PMIF_SPI_MODE_CTRL] =			0x0408,
	[PMIF_IRQ_EVENT_EN_0] =                 0x0420,
	[PMIF_IRQ_FLAG_0] =                     0x0428,
	[PMIF_IRQ_CLR_0] =                      0x042C,
	[PMIF_IRQ_EVENT_EN_1] =                 0x0430,
	[PMIF_IRQ_FLAG_1] =                     0x0438,
	[PMIF_IRQ_CLR_1] =                      0x043C,
	[PMIF_IRQ_EVENT_EN_2] =                 0x0440,
	[PMIF_IRQ_FLAG_2] =                     0x0448,
	[PMIF_IRQ_CLR_2] =                      0x044C,
	[PMIF_IRQ_EVENT_EN_3] =                 0x0450,
	[PMIF_IRQ_FLAG_3] =                     0x0458,
	[PMIF_IRQ_CLR_3] =                      0x045C,
	[PMIF_IRQ_EVENT_EN_4] =                 0x0460,
	[PMIF_IRQ_FLAG_4] =                     0x0468,
	[PMIF_IRQ_CLR_4] =                      0x046C,
	[PMIF_WDT_EVENT_EN_0] =			0x0474,
	[PMIF_WDT_FLAG_0] =			0x0478,
	[PMIF_WDT_EVENT_EN_1] =			0x047C,
	[PMIF_WDT_FLAG_1] =			0x0480,
	[PMIF_SWINF_0_ACC] =			0x0800,
	[PMIF_SWINF_0_WDATA_31_0] =		0x0804,
	[PMIF_SWINF_0_RDATA_31_0] =		0x0814,
	[PMIF_SWINF_0_VLD_CLR] =		0x0824,
	[PMIF_SWINF_0_STA] =			0x0828,
	[PMIF_SWINF_1_ACC] =			0x0840,
	[PMIF_SWINF_1_WDATA_31_0] =		0x0844,
	[PMIF_SWINF_1_RDATA_31_0] =		0x0854,
	[PMIF_SWINF_1_VLD_CLR] =		0x0864,
	[PMIF_SWINF_1_STA] =			0x0868,
	[PMIF_SWINF_2_ACC] =			0x0880,
	[PMIF_SWINF_2_WDATA_31_0] =		0x0884,
	[PMIF_SWINF_2_RDATA_31_0] =		0x0894,
	[PMIF_SWINF_2_VLD_CLR] =		0x08A4,
	[PMIF_SWINF_2_STA] =			0x08A8,
	[PMIF_SWINF_3_ACC] =			0x08C0,
	[PMIF_SWINF_3_WDATA_31_0] =		0x08C4,
	[PMIF_SWINF_3_RDATA_31_0] =		0x08D4,
	[PMIF_SWINF_3_VLD_CLR] =		0x08E4,
	[PMIF_SWINF_3_STA] =			0x08E8,
};

static const u32 mt6853_regs[] = {
	[PMIF_INIT_DONE] =			0x0000,
	[PMIF_INF_EN] =				0x0024,
	[PMIF_ARB_EN] =				0x0150,
	[PMIF_CMDISSUE_EN] =			0x03B8,
	[PMIF_TIMER_CTRL] =			0x03E4,
	[PMIF_SPI_MODE_CTRL] =			0x0408,
	[PMIF_IRQ_EVENT_EN_0] =			0x0420,
	[PMIF_IRQ_FLAG_0] =			0x0428,
	[PMIF_IRQ_CLR_0] =			0x042C,
	[PMIF_IRQ_EVENT_EN_1] =			0x0430,
	[PMIF_IRQ_FLAG_1] =			0x0438,
	[PMIF_IRQ_CLR_1] =			0x043C,
	[PMIF_IRQ_EVENT_EN_2] =			0x0440,
	[PMIF_IRQ_FLAG_2] =			0x0448,
	[PMIF_IRQ_CLR_2] =			0x044C,
	[PMIF_IRQ_EVENT_EN_3] =			0x0450,
	[PMIF_IRQ_FLAG_3] =			0x0458,
	[PMIF_IRQ_CLR_3] =			0x045C,
	[PMIF_IRQ_EVENT_EN_4] =			0x0460,
	[PMIF_IRQ_FLAG_4] =			0x0468,
	[PMIF_IRQ_CLR_4] =			0x046C,
	[PMIF_WDT_EVENT_EN_0] =			0x0474,
	[PMIF_WDT_FLAG_0] =			0x0478,
	[PMIF_WDT_EVENT_EN_1] =			0x047C,
	[PMIF_WDT_FLAG_1] =			0x0480,
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

static const u32 mt6873_regs[] = {
	[PMIF_INIT_DONE] =	0x0000,
	[PMIF_INF_EN] =		0x0024,
	[PMIF_ARB_EN] =		0x0150,
	[PMIF_CMDISSUE_EN] =	0x03B4,
	[PMIF_TIMER_CTRL] =	0x03E0,
	[PMIF_SPI_MODE_CTRL] =	0x0400,
	[PMIF_IRQ_EVENT_EN_0] =	0x0418,
	[PMIF_IRQ_FLAG_0] =	0x0420,
	[PMIF_IRQ_CLR_0] =	0x0424,
	[PMIF_IRQ_EVENT_EN_1] =	0x0428,
	[PMIF_IRQ_FLAG_1] =	0x0430,
	[PMIF_IRQ_CLR_1] =	0x0434,
	[PMIF_IRQ_EVENT_EN_2] =	0x0438,
	[PMIF_IRQ_FLAG_2] =	0x0440,
	[PMIF_IRQ_CLR_2] =	0x0444,
	[PMIF_IRQ_EVENT_EN_3] =	0x0448,
	[PMIF_IRQ_FLAG_3] =	0x0450,
	[PMIF_IRQ_CLR_3] =	0x0454,
	[PMIF_IRQ_EVENT_EN_4] =	0x0458,
	[PMIF_IRQ_FLAG_4] =	0x0460,
	[PMIF_IRQ_CLR_4] =	0x0464,
	[PMIF_WDT_EVENT_EN_0] =	0x046C,
	[PMIF_WDT_FLAG_0] =	0x0470,
	[PMIF_WDT_EVENT_EN_1] =	0x0474,
	[PMIF_WDT_FLAG_1] =	0x0478,
	[PMIF_SWINF_0_ACC] =	0x0C00,
	[PMIF_SWINF_0_WDATA_31_0] =	0x0C04,
	[PMIF_SWINF_0_RDATA_31_0] =	0x0C14,
	[PMIF_SWINF_0_VLD_CLR] =	0x0C24,
	[PMIF_SWINF_0_STA] =	0x0C28,
	[PMIF_SWINF_1_ACC] =	0x0C40,
	[PMIF_SWINF_1_WDATA_31_0] =	0x0C44,
	[PMIF_SWINF_1_RDATA_31_0] =	0x0C54,
	[PMIF_SWINF_1_VLD_CLR] =	0x0C64,
	[PMIF_SWINF_1_STA] =	0x0C68,
	[PMIF_SWINF_2_ACC] =	0x0C80,
	[PMIF_SWINF_2_WDATA_31_0] =	0x0C84,
	[PMIF_SWINF_2_RDATA_31_0] =	0x0C94,
	[PMIF_SWINF_2_VLD_CLR] =	0x0CA4,
	[PMIF_SWINF_2_STA] =	0x0CA8,
	[PMIF_SWINF_3_ACC] =	0x0CC0,
	[PMIF_SWINF_3_WDATA_31_0] =	0x0CC4,
	[PMIF_SWINF_3_RDATA_31_0] =	0x0CD4,
	[PMIF_SWINF_3_VLD_CLR] =	0x0CE4,
	[PMIF_SWINF_3_STA] =	0x0CE8,
};

static const u32 mt6853_spmi_regs[] = {
	[SPMI_OP_ST_CTRL] =	0x0000,
	[SPMI_GRP_ID_EN] =	0x0004,
	[SPMI_OP_ST_STA] =	0x0008,
	[SPMI_MST_SAMPL] =	0x000C,
	[SPMI_MST_REQ_EN] =	0x0010,
	[SPMI_MST_RCS_CTRL] =	0x0014,
	[SPMI_SLV_3_0_EINT] =	0x0020,
	[SPMI_SLV_7_4_EINT] =	0x0024,
	[SPMI_SLV_B_8_EINT] =	0x0028,
	[SPMI_SLV_F_C_EINT] =	0x002C,
	[SPMI_REC_CTRL] =	0x0040,
	[SPMI_REC0] =		0x0044,
	[SPMI_REC1] =		0x0048,
	[SPMI_REC2] =		0x004C,
	[SPMI_REC3] =		0x0050,
	[SPMI_REC4] =		0x0054,
	[SPMI_REC_CMD_DEC] =	0x005C,
	[SPMI_DEC_DBG] =	0x00F8,
	[SPMI_MST_DBG] =	0x00FC,
};

static const u32 mt6873_spmi_regs[] = {
	[SPMI_OP_ST_CTRL] =	0x0000,
	[SPMI_GRP_ID_EN] =	0x0004,
	[SPMI_OP_ST_STA] =	0x0008,
	[SPMI_MST_SAMPL] =	0x000c,
	[SPMI_MST_REQ_EN] =	0x0010,
	[SPMI_REC_CTRL] =	0x0040,
	[SPMI_REC0] =		0x0044,
	[SPMI_REC1] =		0x0048,
	[SPMI_REC2] =		0x004c,
	[SPMI_REC3] =		0x0050,
	[SPMI_REC4] =		0x0054,
	[SPMI_MST_DBG] =	0x00fc,
};

enum {
	SPMI_RCS_SR_BIT,
	SPMI_RCS_A_BIT
};

enum {
	SPMI_RCS_MST_W = 1,
	SPMI_RCS_SLV_W = 3
};

enum {
	/* MT6885/MT6873 series */
	IRQ_PMIC_CMD_ERR_PARITY_ERR = 17,
	IRQ_PMIF_ACC_VIO = 20,
	IRQ_PMIC_ACC_VIO = 21,
	IRQ_LAT_LIMIT_REACHED = 6,
	IRQ_HW_MONITOR = 7,
	IRQ_WDT = 8,
	/* MT6853 series */
	IRQ_PMIF_ACC_VIO_V2 = 31,
	IRQ_PMIC_ACC_VIO_V2 = 0,
	IRQ_HW_MONITOR_V2 = 18,
	IRQ_WDT_V2 = 19,
	IRQ_ALL_PMIC_MPU_VIO_V2 = 20,
	/* MT6833/MT6877 series */
	IRQ_HW_MONITOR_V3 = 12,
	IRQ_WDT_V3 = 13,
	IRQ_ALL_PMIC_MPU_VIO_V3 = 14,
	/* MT6983/MT6879 */
	IRQ_HW_MONITOR_V4 = 29,
	IRQ_WDT_V4 = 30,
	IRQ_ALL_PMIC_MPU_VIO_V4 = 31,
};

static u32 pmif_readl(struct pmif *arb, enum pmif_regs reg)
{
	return readl(arb->base + arb->regs[reg]);
}

static void pmif_writel(struct pmif *arb, u32 val, enum pmif_regs reg)
{
	writel(val, arb->base + arb->regs[reg]);
}

static void mtk_spmi_writel(struct pmif *arb, u32 val, enum spmi_regs reg)
{
	writel(val, arb->spmimst_base + arb->spmimst_regs[reg]);
}


static u32 mtk_spmi_readl(struct pmif *arb, enum spmi_regs reg)
{
	return readl(arb->spmimst_base + arb->spmimst_regs[reg]);
}

static bool pmif_is_fsm_vldclr(struct pmif *arb)
{
	u32 reg_rdata;

	reg_rdata = pmif_readl(arb, arb->chan.ch_sta);
	return GET_SWINF(reg_rdata) == SWINF_WFVLDCLR;
}

static int pmif_arb_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid)
{
	struct pmif *arb = spmi_controller_get_drvdata(ctrl);
	u32 rdata;
	u8 cmd;
	int ret;

	/* Check the opcode */
	if (opc == SPMI_CMD_RESET)
		cmd = 0;
	else if (opc == SPMI_CMD_SLEEP)
		cmd = 1;
	else if (opc == SPMI_CMD_SHUTDOWN)
		cmd = 2;
	else if (opc == SPMI_CMD_WAKEUP)
		cmd = 3;
	else
		return -EINVAL;

	mtk_spmi_writel(arb, (cmd << 0x4) | sid, SPMI_OP_ST_CTRL);
	ret = readl_poll_timeout_atomic(arb->spmimst_base + arb->spmimst_regs[SPMI_OP_ST_STA],
					rdata, (rdata & SPMI_OP_ST_BUSY) == SPMI_OP_ST_BUSY,
					PMIF_DELAY_US, PMIF_TIMEOUT);
	if (ret < 0)
		dev_err(&ctrl->dev, "timeout, err = %d\r\n", ret);

	return ret;
}

static int pmif_spmi_read_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid,
			      u16 addr, u8 *buf, size_t len)
{
	struct pmif *arb = spmi_controller_get_drvdata(ctrl);
	struct ch_reg *inf_reg = NULL;
	int ret;
	u32 data = 0;
	u8 bc = len - 1;
	unsigned long flags;

	/* Check for argument validation. */
	if (sid & ~(0xf))
		return -EINVAL;

	if (!arb)
		return -EINVAL;

	inf_reg = &arb->chan;
	/* Check the opcode */
	if (opc >= 0x60 && opc <= 0x7f)
		opc = PMIF_CMD_REG;
	else if (opc >= 0x20 && opc <= 0x2f)
		opc = PMIF_CMD_EXT_REG_LONG;
	else if (opc >= 0x38 && opc <= 0x3f)
		opc = PMIF_CMD_EXT_REG_LONG;
	else
		return -EINVAL;

	raw_spin_lock_irqsave(&arb->lock, flags);
	/* Wait for Software Interface FSM state to be IDLE. */
	ret = readl_poll_timeout_atomic(arb->base + arb->regs[inf_reg->ch_sta],
					data, GET_SWINF(data) == SWINF_IDLE,
					PMIF_DELAY_US, PMIF_TIMEOUT);
	if (ret < 0) {
		dev_err(&ctrl->dev, "check IDLE timeout, read 0x%x, sta=0x%x, SPMI_DBG=0x%x\n",
			addr, pmif_readl(arb, inf_reg->ch_sta),
			readl(arb->spmimst_base + arb->spmimst_regs[SPMI_MST_DBG]));
		/* set channel ready if the data has transferred */
		if (pmif_is_fsm_vldclr(arb))
			pmif_writel(arb, 1, inf_reg->ch_rdy);
		raw_spin_unlock_irqrestore(&arb->lock, flags);
		return ret;
	}

	/* Send the command. */
	pmif_writel(arb,
		    (opc << 30) | (sid << 24) | (bc << 16) | addr,
		    inf_reg->ch_send);

	/* Wait for Software Interface FSM state to be WFVLDCLR,
	 *
	 * read the data and clear the valid flag.
	 */
	ret = readl_poll_timeout_atomic(arb->base + arb->regs[inf_reg->ch_sta],
					data, GET_SWINF(data) == SWINF_WFVLDCLR,
					PMIF_DELAY_US, PMIF_TIMEOUT);
	if (ret < 0) {
		dev_err(&ctrl->dev, "check WFVLDCLR timeout, read 0x%x, sta=0x%x, SPMI_DBG=0x%x\n",
			addr, pmif_readl(arb, inf_reg->ch_sta),
			readl(arb->spmimst_base + arb->spmimst_regs[SPMI_MST_DBG]));
		raw_spin_unlock_irqrestore(&arb->lock, flags);
		return ret;
	}

	data = pmif_readl(arb, inf_reg->rdata);
	memcpy(buf, &data, (bc & 3) + 1);
	pmif_writel(arb, 1, inf_reg->ch_rdy);
	raw_spin_unlock_irqrestore(&arb->lock, flags);

	return 0;
}

static int pmif_spmi_write_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid,
			       u16 addr, const u8 *buf, size_t len)
{
	struct pmif *arb = spmi_controller_get_drvdata(ctrl);
	struct ch_reg *inf_reg = NULL;
	int ret;
	u32 data = 0;
	u8 bc = len - 1;
	unsigned long flags = 0;

	/* Check for argument validation. */
	if (sid & ~(0xf))
		return -EINVAL;

	if (!arb)
		return -EINVAL;

	inf_reg = &arb->chan;

	/* Check the opcode */
	if (opc >= 0x40 && opc <= 0x5F)
		opc = PMIF_CMD_REG;
	else if (opc <= 0x0F)
		opc = PMIF_CMD_EXT_REG_LONG;
	else if (opc >= 0x30 && opc <= 0x37)
		opc = PMIF_CMD_EXT_REG_LONG;
	else if (opc >= 0x80)
		opc = PMIF_CMD_REG_0;
	else
		return -EINVAL;

	raw_spin_lock_irqsave(&arb->lock, flags);
	/* Wait for Software Interface FSM state to be IDLE. */
	ret = readl_poll_timeout_atomic(arb->base + arb->regs[inf_reg->ch_sta],
					data, GET_SWINF(data) == SWINF_IDLE,
					PMIF_DELAY_US, PMIF_TIMEOUT);
	if (ret < 0) {
		dev_err(&ctrl->dev, "check IDLE timeout, read 0x%x, sta=0x%x, SPMI_DBG=0x%x\n",
			addr, pmif_readl(arb, inf_reg->ch_sta),
			readl(arb->spmimst_base + arb->spmimst_regs[SPMI_MST_DBG]));
		/* set channel ready if the data has transferred */
		if (pmif_is_fsm_vldclr(arb))
			pmif_writel(arb, 1, inf_reg->ch_rdy);
		raw_spin_unlock_irqrestore(&arb->lock, flags);
		return ret;
	}

	/* Set the write data. */
	memcpy(&data, buf, (bc & 3) + 1);
	pmif_writel(arb, data, inf_reg->wdata);

	/* Send the command. */
	pmif_writel(arb,
		    (opc << 30) | BIT(29) | (sid << 24) | (bc << 16) | addr,
		    inf_reg->ch_send);
	raw_spin_unlock_irqrestore(&arb->lock, flags);

	return 0;
}

static struct platform_driver mtk_spmi_driver;

static struct pmif mt6833_pmif_arb[] = {
	{
		.regs = mt6833_regs,
		.spmimst_regs = mt6853_spmi_regs,
		.soc_chan = 2,
		.caps = 1,
	},
};

static struct pmif mt6853_pmif_arb[] = {
	{
		.regs = mt6853_regs,
		.spmimst_regs = mt6853_spmi_regs,
		.soc_chan = 2,
		.caps = 1,
	},
};

static struct pmif mt6873_pmif_arb[] = {
	{
		.regs = mt6873_regs,
		.spmimst_regs = mt6873_spmi_regs,
		.soc_chan = 2,
		.caps = 1,
	},
};

static struct pmif mt6893_pmif_arb[] = {
	{
		.regs = mt6873_regs,
		.spmimst_regs = mt6873_spmi_regs,
		.soc_chan = 2,
		.caps = 1,
	},
};

static struct pmif mt6xxx_pmif_arb[] = {
	{
		.base = 0x0,
		.regs = mt6xxx_regs,
		.spmimst_base = 0x0,
		.spmimst_regs = mt6853_spmi_regs,
		.soc_chan = 2,
		.mstid = SPMI_MASTER_1,
		.pmifid = PMIF_PMIFID_SPMI0,
		.read_cmd = pmif_spmi_read_cmd,
		.write_cmd = pmif_spmi_write_cmd,
		.caps = 2,
	},
};

/* PMIF Exception IRQ Handler */
static void pmif_cmd_err_parity_err_irq_handler(int irq, void *data)
{
	spmi_dump_spmimst_all_reg();
	spmi_dump_pmif_record_reg();
	if (IS_ENABLED(CONFIG_MTK_AEE_FEATURE))
		aee_kernel_warning("PMIF", "PMIF:parity error");
}

static void pmif_pmif_acc_vio_irq_handler(int irq, void *data)
{
	spmi_dump_pmif_record_reg();
	spmi_dump_pmif_swinf_reg();
	spmi_dump_pmif_acc_vio_reg();
	if (IS_ENABLED(CONFIG_MTK_AEE_FEATURE))
		aee_kernel_warning("PMIF", "PMIF:pmif_acc_vio");
}

static void pmif_pmic_acc_vio_irq_handler(int irq, void *data)
{
	spmi_dump_pmic_acc_vio_reg();
	if (IS_ENABLED(CONFIG_MTK_AEE_FEATURE))
		aee_kernel_warning("PMIF", "PMIF:pmic_acc_vio");
}

static void pmif_lat_limit_reached_irq_handler(int irq, void *data)
{
	spmi_dump_pmif_busy_reg();
	spmi_dump_pmif_record_reg();
}

static void pmif_hw_monitor_irq_handler(int irq, void *data)
{
	spmi_dump_pmif_record_reg();
	if (IS_ENABLED(CONFIG_MTK_AEE_FEATURE))
		aee_kernel_warning("PMIF", "PMIF:pmif_hw_monitor_match");
}

static void pmif_wdt_irq_handler(int irq, void *data)
{
	spmi_dump_pmif_busy_reg();
	spmi_dump_pmif_record_reg();
	spmi_dump_wdt_reg();
}

static irqreturn_t pmif_event_0_irq_handler(int irq, void *data)
{
	struct pmif *arb = data;
	int irq_f = 0, idx = 0;

	__pm_stay_awake(arb->pmifThread_lock);
	mutex_lock(&arb->pmif_mutex);
	irq_f = pmif_readl(arb, PMIF_IRQ_FLAG_0);
	if (irq_f == 0) {
		mutex_unlock(&arb->pmif_mutex);
		__pm_relax(arb->pmifThread_lock);
		return IRQ_NONE;
	}

	for (idx = 0; idx < 32; idx++) {
		if ((irq_f & (0x1 << idx)) != 0) {
			switch (idx) {
			case IRQ_WDT_V4:
				pmif_wdt_irq_handler(irq, data);
			break;
			case IRQ_ALL_PMIC_MPU_VIO_V4:
				pmif_pmif_acc_vio_irq_handler(irq, data);
			break;
			default:
				pr_notice("%s IRQ[%d] triggered\n",
					__func__, idx);
				spmi_dump_pmif_record_reg();
			break;
			}
			pmif_writel(arb, irq_f, PMIF_IRQ_CLR_0);
			break;
		}
	}
	mutex_unlock(&arb->pmif_mutex);
	__pm_relax(arb->pmifThread_lock);

	return IRQ_HANDLED;
}

static irqreturn_t pmif_event_1_irq_handler(int irq, void *data)
{
	struct pmif *arb = data;
	int irq_f = 0, idx = 0;

	__pm_stay_awake(arb->pmifThread_lock);
	mutex_lock(&arb->pmif_mutex);
	irq_f = pmif_readl(arb, PMIF_IRQ_FLAG_1);
	if (irq_f == 0) {
		mutex_unlock(&arb->pmif_mutex);
		__pm_relax(arb->pmifThread_lock);
		return IRQ_NONE;
	}

	for (idx = 0; idx < 32; idx++) {
		if ((irq_f & (0x1 << idx)) != 0) {
			switch (idx) {
			default:
				pr_notice("%s IRQ[%d] triggered\n",
					__func__, idx);
				spmi_dump_pmif_record_reg();
			break;
			}
			pmif_writel(arb, irq_f, PMIF_IRQ_CLR_1);
			break;
		}
	}
	mutex_unlock(&arb->pmif_mutex);
	__pm_relax(arb->pmifThread_lock);

	return IRQ_HANDLED;
}

static irqreturn_t pmif_event_2_irq_handler(int irq, void *data)
{
	struct pmif *arb = data;
	int irq_f = 0, idx = 0;

	__pm_stay_awake(arb->pmifThread_lock);
	mutex_lock(&arb->pmif_mutex);
	irq_f = pmif_readl(arb, PMIF_IRQ_FLAG_2);
	if (irq_f == 0) {
		mutex_unlock(&arb->pmif_mutex);
		__pm_relax(arb->pmifThread_lock);
		return IRQ_NONE;
	}

	for (idx = 0; idx < 32; idx++) {
		if ((irq_f & (0x1 << idx)) != 0) {
			switch (idx) {
			case IRQ_PMIC_CMD_ERR_PARITY_ERR:
				pmif_cmd_err_parity_err_irq_handler(irq, data);
			break;
			case IRQ_PMIF_ACC_VIO:
			case IRQ_PMIF_ACC_VIO_V2:
				pmif_pmif_acc_vio_irq_handler(irq, data);
			break;
			case IRQ_PMIC_ACC_VIO:
				pmif_pmic_acc_vio_irq_handler(irq, data);
			break;
			default:
				pr_notice("%s IRQ[%d] triggered\n",
					__func__, idx);
				spmi_dump_pmif_record_reg();
			break;
			}
			pmif_writel(arb, irq_f, PMIF_IRQ_CLR_2);
			break;
		}
	}
	mutex_unlock(&arb->pmif_mutex);
	__pm_relax(arb->pmifThread_lock);

	return IRQ_HANDLED;
}

static irqreturn_t pmif_event_3_irq_handler(int irq, void *data)
{
	struct pmif *arb = data;
	int irq_f = 0, idx = 0;

	__pm_stay_awake(arb->pmifThread_lock);
	mutex_lock(&arb->pmif_mutex);
	irq_f = pmif_readl(arb, PMIF_IRQ_FLAG_3);
	if (irq_f == 0) {
		mutex_unlock(&arb->pmif_mutex);
		__pm_relax(arb->pmifThread_lock);
		return IRQ_NONE;
	}

	for (idx = 0; idx < 32; idx++) {
		if ((irq_f & (0x1 << idx)) != 0) {
			switch (idx) {
			case IRQ_LAT_LIMIT_REACHED:
				pmif_lat_limit_reached_irq_handler(irq, data);
			break;
			case IRQ_HW_MONITOR:
			case IRQ_HW_MONITOR_V2:
			case IRQ_HW_MONITOR_V3:
				pmif_hw_monitor_irq_handler(irq, data);
			break;
			case IRQ_WDT:
			case IRQ_WDT_V2:
			case IRQ_WDT_V3:
				pmif_wdt_irq_handler(irq, data);
			break;
			case IRQ_PMIC_ACC_VIO_V2:
				pmif_pmic_acc_vio_irq_handler(irq, data);
			break;
			case IRQ_ALL_PMIC_MPU_VIO_V2:
			case IRQ_ALL_PMIC_MPU_VIO_V3:
				pmif_pmif_acc_vio_irq_handler(irq, data);
			break;
			default:
				pr_notice("%s IRQ[%d] triggered\n",
					__func__, idx);
				spmi_dump_pmif_record_reg();
			break;
			}
			pmif_writel(arb, irq_f, PMIF_IRQ_CLR_3);
			break;
		}
	}
	mutex_unlock(&arb->pmif_mutex);
	__pm_relax(arb->pmifThread_lock);

	return IRQ_HANDLED;
}

static irqreturn_t pmif_event_4_irq_handler(int irq, void *data)
{
	struct pmif *arb = data;
	int irq_f = 0, idx = 0;

	__pm_stay_awake(arb->pmifThread_lock);
	mutex_lock(&arb->pmif_mutex);
	irq_f = pmif_readl(arb, PMIF_IRQ_FLAG_4);
	if (irq_f == 0) {
		mutex_unlock(&arb->pmif_mutex);
		__pm_relax(arb->pmifThread_lock);
		return IRQ_NONE;
	}

	for (idx = 0; idx < 32; idx++) {
		if ((irq_f & (0x1 << idx)) != 0) {
			switch (idx) {
			default:
				pr_notice("%s IRQ[%d] triggered\n",
					__func__, idx);
				spmi_dump_pmif_record_reg();
			break;
			}
			pmif_writel(arb, irq_f, PMIF_IRQ_CLR_4);
			break;
		}
	}
	mutex_unlock(&arb->pmif_mutex);
	__pm_relax(arb->pmifThread_lock);

	return IRQ_HANDLED;
}

static struct pmif_irq_desc pmif_event_irq[] = {
	PMIF_IRQDESC(event_0),
	PMIF_IRQDESC(event_1),
	PMIF_IRQDESC(event_2),
	PMIF_IRQDESC(event_3),
	PMIF_IRQDESC(event_4),
};

static void pmif_irq_register(struct platform_device *pdev,
		struct pmif *arb, int irq)
{
	int i = 0, ret = 0;
	u32 irq_event_en[5] = {0};

	for (i = 0; i < ARRAY_SIZE(pmif_event_irq); i++) {
		if (!pmif_event_irq[i].name)
			continue;
		ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
				pmif_event_irq[i].irq_handler,
				IRQF_TRIGGER_HIGH | IRQF_ONESHOT | IRQF_SHARED,
				pmif_event_irq[i].name, arb);
		if (ret < 0) {
			dev_notice(&pdev->dev, "request %s irq fail\n",
				pmif_event_irq[i].name);
			continue;
		}
		pmif_event_irq[i].irq = irq;
	}

	ret = of_property_read_u32_array(pdev->dev.of_node, "irq_event_en",
		irq_event_en, ARRAY_SIZE(irq_event_en));

	pmif_writel(arb, irq_event_en[0] | pmif_readl(arb, PMIF_IRQ_EVENT_EN_0),
			PMIF_IRQ_EVENT_EN_0);
	pmif_writel(arb, irq_event_en[1] | pmif_readl(arb, PMIF_IRQ_EVENT_EN_1),
			PMIF_IRQ_EVENT_EN_1);
	pmif_writel(arb, irq_event_en[2] | pmif_readl(arb, PMIF_IRQ_EVENT_EN_2),
			PMIF_IRQ_EVENT_EN_2);
	pmif_writel(arb, irq_event_en[3] | pmif_readl(arb, PMIF_IRQ_EVENT_EN_3),
			PMIF_IRQ_EVENT_EN_3);
	pmif_writel(arb, irq_event_en[4] | pmif_readl(arb, PMIF_IRQ_EVENT_EN_4),
			PMIF_IRQ_EVENT_EN_4);
}

static void rcs_irq_lock(struct irq_data *data)
{
	struct pmif *arb = irq_data_get_irq_chip_data(data);

	mutex_lock(&arb->rcs_irqlock);
}

static void rcs_irq_sync_unlock(struct irq_data *data)
{
	struct pmif *arb = irq_data_get_irq_chip_data(data);

	mutex_unlock(&arb->rcs_irqlock);
}

static void rcs_irq_enable(struct irq_data *data)
{
	unsigned int hwirq = irqd_to_hwirq(data);
	struct pmif *arb = irq_data_get_irq_chip_data(data);

	arb->rcs_enable_hwirq[hwirq] = true;
}

static void rcs_irq_disable(struct irq_data *data)
{
	unsigned int hwirq = irqd_to_hwirq(data);
	struct pmif *arb = irq_data_get_irq_chip_data(data);

	arb->rcs_enable_hwirq[hwirq] = false;
}

static const struct irq_chip rcs_irq_chip = {
	.name			= "rcs_irq",
	.irq_bus_lock		= rcs_irq_lock,
	.irq_bus_sync_unlock	= rcs_irq_sync_unlock,
	.irq_enable		= rcs_irq_enable,
	.irq_disable		= rcs_irq_disable,
};

static int rcs_irq_map(struct irq_domain *d, unsigned int virq,
			irq_hw_number_t hw)
{
	struct pmif *arb = d->host_data;

	irq_set_chip_data(virq, arb);
	irq_set_chip(virq, &arb->irq_chip);
	irq_set_nested_thread(virq, 1);
	irq_set_parent(virq, arb->rcs_irq);
	irq_set_noprobe(virq);

	return 0;
}

static const struct irq_domain_ops rcs_irq_domain_ops = {
	.map	= rcs_irq_map,
	.xlate	= irq_domain_xlate_onetwocell,
};

static irqreturn_t rcs_irq_handler(int irq, void *data)
{
	struct pmif *arb = data;
	unsigned int slv_irq_sta;
	int i;

	for (i = 0; i < SPMI_MAX_SLAVE_ID; i++) {
		slv_irq_sta = mtk_spmi_readl(arb, SPMI_SLV_3_0_EINT + (i / 4));
		slv_irq_sta = (slv_irq_sta >> ((i % 4) * 8)) & 0xFF;

		/* need to clear using 0xFF to avoid new irq happen
		 * after read SPMI_SLV_3_0_EINT + (i/4) value then use
		 * this value to clean
		 */
		mtk_spmi_writel(arb, (0xFF << ((i % 4) * 8)),
				SPMI_SLV_3_0_EINT + (i / 4));
		if (arb->rcs_enable_hwirq[i] && slv_irq_sta) {
			dev_info(&arb->spmic->dev,
				 "hwirq=%d, sta=0x%x\n", i, slv_irq_sta);
			handle_nested_irq(irq_find_mapping(arb->domain, i));
		}
	}
	return IRQ_HANDLED;
}

static int rcs_irq_register(struct platform_device *pdev,
			    struct pmif *arb, int irq)
{
	int i, ret = 0;

	mutex_init(&arb->rcs_irqlock);
	arb->rcs_enable_hwirq = devm_kcalloc(&pdev->dev, SPMI_MAX_SLAVE_ID,
					     sizeof(*arb->rcs_enable_hwirq),
					     GFP_KERNEL);
	if (!arb->rcs_enable_hwirq)
		return -ENOMEM;

	arb->irq_chip = rcs_irq_chip;
	arb->domain = irq_domain_add_linear(pdev->dev.of_node,
					    SPMI_MAX_SLAVE_ID,
					    &rcs_irq_domain_ops, arb);
	if (!arb->domain) {
		dev_notice(&pdev->dev, "Failed to create IRQ domain\n");
		return -ENODEV;
	}
	/* clear all slave irq status */
	for (i = 0; i < SPMI_MAX_SLAVE_ID; i++) {
		mtk_spmi_writel(arb, (0xFF << ((i % 4) * 8)),
				SPMI_SLV_3_0_EINT + (i / 4));
	}
	ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
					rcs_irq_handler, IRQF_ONESHOT,
					rcs_irq_chip.name, arb);
	if (ret < 0) {
		dev_notice(&pdev->dev, "Failed to request IRQ=%d, ret = %d\n",
			   irq, ret);
		return ret;
	}
	enable_irq_wake(irq);

	return ret;
}

static int mtk_spmi_probe(struct platform_device *pdev)
{
	struct pmif *arb;
	struct resource *res;
	struct spmi_controller *ctrl;
	int err = 0;
#if defined(CONFIG_FPGA_EARLY_PORTING)
	u8 id_l = 0, id_h = 0, val = 0, test_id = 0x5;
	u16 hwcid_l_addr = 0x8, hwcid_h_addr = 0x9, test_w_addr = 0x3a7;
#endif
	ctrl = spmi_controller_alloc(&pdev->dev, sizeof(*arb));
	if (!ctrl)
		return -ENOMEM;

	ctrl->cmd = pmif_arb_cmd;
	ctrl->read_cmd = pmif_spmi_read_cmd;
	ctrl->write_cmd = pmif_spmi_write_cmd;

	/* re-assign of_id->data */
	spmi_controller_set_drvdata(ctrl, (void *)of_device_get_match_data(&pdev->dev));
	arb = spmi_controller_get_drvdata(ctrl);
	arb->spmic = ctrl;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pmif");
	arb->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(arb->base)) {
		err = PTR_ERR(arb->base);
		goto err_put_ctrl;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "spmimst");
	arb->spmimst_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(arb->spmimst_base)) {
		err = PTR_ERR(arb->spmimst_base);
		goto err_put_ctrl;
	}
#if !defined(CONFIG_FPGA_EARLY_PORTING)

	if (arb->caps == 1) {
		arb->pmif_sys_ck = devm_clk_get(&pdev->dev, "pmif_sys_ck");
		if (IS_ERR(arb->pmif_sys_ck)) {
			dev_notice(&pdev->dev, "[PMIF]:failed to get ap clock: %ld\n",
			PTR_ERR(arb->pmif_sys_ck));
			goto err_put_ctrl;
		}

		arb->pmif_tmr_ck = devm_clk_get(&pdev->dev, "pmif_tmr_ck");
		if (IS_ERR(arb->pmif_tmr_ck)) {
			dev_notice(&pdev->dev, "[PMIF]:failed to get tmr clock: %ld\n",
			PTR_ERR(arb->pmif_tmr_ck));
			goto err_put_ctrl;
		}

		arb->spmimst_clk_mux = devm_clk_get(&pdev->dev, "spmimst_clk_mux");
		if (IS_ERR(arb->spmimst_clk_mux)) {
			dev_notice(&pdev->dev, "[SPMIMST]:failed to get clock: %ld\n",
			PTR_ERR(arb->spmimst_clk_mux));
			goto err_put_ctrl;
		} else
			err = clk_prepare_enable(arb->spmimst_clk_mux);
		if (err) {
			dev_notice(&pdev->dev, "[SPMIMST]:failed to enable spmi master clk\n");
			goto err_put_ctrl;
		}
	} else {
		arb->pmif_clk_mux = devm_clk_get(&pdev->dev, "pmif_clk_mux");
		if (IS_ERR(arb->pmif_clk_mux))
			dev_notice(&pdev->dev, "[PMIF]:failed to get clock: %ld\n",
				PTR_ERR(arb->pmif_clk_mux));
		else
			err = clk_prepare_enable(arb->pmif_clk_mux);
		if (err)
			dev_notice(&pdev->dev, "[PMIF]:failed to enable pmif_clk_mux\n");

		arb->spmimst_m_clk_mux = devm_clk_get(&pdev->dev, "spmimst_m_clk_mux");
		if (IS_ERR(arb->spmimst_m_clk_mux))
			dev_notice(&pdev->dev, "[SPMIMST]:failed to get clock: %ld\n",
				PTR_ERR(arb->spmimst_m_clk_mux));
		else
			err = clk_prepare_enable(arb->spmimst_m_clk_mux);
		if (err)
			dev_notice(&pdev->dev, "[PMIF]:failed to enable spmimst_m_clk\n");

		arb->spmimst_p_clk_mux = devm_clk_get(&pdev->dev, "spmimst_p_clk_mux");
		if (IS_ERR(arb->spmimst_p_clk_mux))
			dev_notice(&pdev->dev, "[SPMIMST]:failed to get clock: %ld\n",
				PTR_ERR(arb->spmimst_p_clk_mux));
		else
			err = clk_prepare_enable(arb->spmimst_p_clk_mux);
		if (err)
			dev_notice(&pdev->dev, "[SPMIMST]:failed to enable spmimst_p_clk\n");

		arb->vlp_pmif_clk_mux = devm_clk_get(&pdev->dev, "vlp_pmif_clk_mux");
		if (IS_ERR(arb->vlp_pmif_clk_mux))
			dev_notice(&pdev->dev, "[PMIF]:failed to get clock: %ld\n",
				PTR_ERR(arb->vlp_pmif_clk_mux));
		else
			err = clk_prepare_enable(arb->vlp_pmif_clk_mux);
		if (err)
			dev_notice(&pdev->dev, "[PMIF]:failed to enable vlp_pmif_clk\n");

		arb->vlp_spmimst_m_clk_mux = devm_clk_get(&pdev->dev, "vlp_spmimst_m_clk_mux");
		if (IS_ERR(arb->vlp_spmimst_m_clk_mux))
			dev_notice(&pdev->dev, "[SPMIMST]:failed to get clock: %ld\n",
				PTR_ERR(arb->vlp_spmimst_m_clk_mux));
		else
			err = clk_prepare_enable(arb->vlp_spmimst_m_clk_mux);
		if (err)
			dev_notice(&pdev->dev, "[SPMIMST]:failed to enable vlp_spmimst_m_clk\n");

		arb->vlp_spmimst_p_clk_mux = devm_clk_get(&pdev->dev, "vlp_spmimst_p_clk_mux");
		if (IS_ERR(arb->vlp_spmimst_p_clk_mux))
			dev_notice(&pdev->dev, "[SPMIMST]:failed to get clock: %ld\n",
				PTR_ERR(arb->vlp_spmimst_p_clk_mux));
		else
			err = clk_prepare_enable(arb->vlp_spmimst_p_clk_mux);
		if (err)
			dev_notice(&pdev->dev, "[SPMIMST]:failed to enable vlp_spmimst_p_clk\n");
	}
#else
	dev_notice(&pdev->dev, "[PMIF]:no need to get clock at fpga\n");
#endif /* #if defined(CONFIG_MTK_FPGA) || defined(CONFIG_FPGA_EARLY_PORTING) */
	arb->chan.ch_sta = PMIF_SWINF_0_STA + (PMIF_CHAN_OFFSET * arb->soc_chan);
	arb->chan.wdata = PMIF_SWINF_0_WDATA_31_0 + (PMIF_CHAN_OFFSET * arb->soc_chan);
	arb->chan.rdata = PMIF_SWINF_0_RDATA_31_0 + (PMIF_CHAN_OFFSET * arb->soc_chan);
	arb->chan.ch_send = PMIF_SWINF_0_ACC + (PMIF_CHAN_OFFSET * arb->soc_chan);
	arb->chan.ch_rdy = PMIF_SWINF_0_VLD_CLR + (PMIF_CHAN_OFFSET * arb->soc_chan);

	raw_spin_lock_init(&arb->lock);
	arb->pmifThread_lock =
		wakeup_source_register(NULL, "pmif wakelock");
	mutex_init(&arb->pmif_mutex);

	/* enable debugger */
	spmi_pmif_dbg_init(ctrl);
	spmi_pmif_create_attr(&mtk_spmi_driver.driver);

	if (arb->caps == 2) {
		arb->irq = platform_get_irq_byname(pdev, "pmif_irq");
		if (arb->irq < 0) {
			err = arb->irq;
			dev_notice(&pdev->dev,
				   "Failed to get pmif_irq, ret = %d\n", arb->irq);
		}
		pmif_irq_register(pdev, arb, arb->irq);

		arb->rcs_irq = platform_get_irq_byname(pdev, "rcs_irq");
		if (arb->rcs_irq < 0) {
			dev_notice(&pdev->dev,
				   "Failed to get rcs_irq, ret = %d\n", arb->rcs_irq);
		} else {
			err = rcs_irq_register(pdev, arb, arb->rcs_irq);
			if (err)
				dev_notice(&pdev->dev,
				   "Failed to register rcs_irq, ret = %d\n", arb->rcs_irq);
		}
	}
#if defined(CONFIG_FPGA_EARLY_PORTING)
	/* pmif/spmi initial setting */
	pmif_writel(arb, 0xffffffff, PMIF_INF_EN);
	pmif_writel(arb, 0xffffffff, PMIF_ARB_EN);
	pmif_writel(arb, 0x1, PMIF_CMDISSUE_EN);
	pmif_writel(arb, 0x1, PMIF_INIT_DONE);

	mtk_spmi_writel(arb, 0x1, SPMI_MST_REQ_EN);
	/* r/w verification */
	ctrl->read_cmd(ctrl, 0x38, test_id, hwcid_l_addr, &id_l, 1);
	ctrl->read_cmd(ctrl, 0x38, test_id, hwcid_h_addr, &id_h, 1);
	dev_notice(&pdev->dev, "%s PMIC=[0x%x%x]\n", __func__, id_h, id_l);
	val = 0x5a;
	ctrl->write_cmd(ctrl, 0x30, test_id, test_w_addr, &val, 1);
	val = 0x0;
	ctrl->read_cmd(ctrl, 0x38, test_id, test_w_addr, &val, 1);
	dev_notice(&pdev->dev, "%s check [0x%x] = 0x%x\n", __func__, test_w_addr, val);

#endif
	platform_set_drvdata(pdev, ctrl);

	err = spmi_controller_add(ctrl);
	if (err)
		goto err_domain_remove;

	return 0;

err_domain_remove:
	if (arb->domain)
		irq_domain_remove(arb->domain);
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	if (arb->caps == 1)
		clk_disable_unprepare(arb->spmimst_clk_mux);
	else {
		clk_disable_unprepare(arb->pmif_clk_mux);
		clk_disable_unprepare(arb->spmimst_m_clk_mux);
		clk_disable_unprepare(arb->spmimst_p_clk_mux);
		clk_disable_unprepare(arb->vlp_pmif_clk_mux);
		clk_disable_unprepare(arb->vlp_spmimst_m_clk_mux);
		clk_disable_unprepare(arb->vlp_spmimst_p_clk_mux);
	}
#endif
err_put_ctrl:
	spmi_controller_put(ctrl);
	return err;
}

static int mtk_spmi_remove(struct platform_device *pdev)
{
	struct spmi_controller *ctrl = platform_get_drvdata(pdev);
	struct pmif *arb = spmi_controller_get_drvdata(ctrl);

	if (arb->domain)
		irq_domain_remove(arb->domain);
	spmi_controller_remove(ctrl);
	spmi_controller_put(ctrl);
	return 0;
}

static const struct of_device_id mtk_spmi_match_table[] = {
	{
		.compatible = "mediatek,mt6833-spmi",
		.data = &mt6833_pmif_arb,
	}, {
		.compatible = "mediatek,mt6853-spmi",
		.data = &mt6853_pmif_arb,
	}, {
		.compatible = "mediatek,mt6855-spmi",
		.data = &mt6xxx_pmif_arb,
	}, {
		.compatible = "mediatek,mt6873-spmi",
		.data = &mt6873_pmif_arb,
	}, {
		.compatible = "mediatek,mt6879-spmi",
		.data = &mt6xxx_pmif_arb,
	}, {
		.compatible = "mediatek,mt6893-spmi",
		.data = &mt6893_pmif_arb,
	}, {
		.compatible = "mediatek,mt6895-spmi",
		.data = &mt6xxx_pmif_arb,
	}, {
		.compatible = "mediatek,mt6983-spmi",
		.data = &mt6xxx_pmif_arb,
	}, {
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, mtk_spmi_match_table);

static struct platform_driver mtk_spmi_driver = {
	.driver		= {
		.name	= "spmi-mtk",
		.of_match_table = of_match_ptr(mtk_spmi_match_table),
	},
	.probe		= mtk_spmi_probe,
	.remove		= mtk_spmi_remove,
};
module_platform_driver(mtk_spmi_driver);

MODULE_AUTHOR("Hsin-Hsiung Wang <hsin-hsiung.wang@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SPMI Driver");
MODULE_LICENSE("GPL");
