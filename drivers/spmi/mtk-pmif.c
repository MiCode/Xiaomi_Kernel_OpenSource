// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Author: Argus Lin <argus.lin@mediatek.com>
 */
/* #define DEBUG */

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
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/pm_wakeup.h>
#include <linux/spmi.h>
#include <linux/pmif.h>
#include <spmi_sw.h>
#include <dt-bindings/spmi/spmi.h>
#include <mt-plat/aee.h>

/*
 * marco
 */
#define PMIF_TIMEOUT    0
#define PMIF_BRINGUP    0

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
#define PMIF_PMIFID_SPI			0
#define PMIF_PMIFID_SPMI0		0
#define PMIF_PMIFID_SPMI1		1
#define PMIF_PMIFID_SPMI2		2
#define PMIF_PMIFID_SPMI3		3

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
	[PMIF_CMDISSUE_EN] =			0x03B4,
	[PMIF_TIMER_CTRL] =			0x03E0,
	[PMIF_SPI_MODE_CTRL] =			0x0400,
	[PMIF_IRQ_EVENT_EN_0] =                 0x0418,
	[PMIF_IRQ_FLAG_0] =                     0x0420,
	[PMIF_IRQ_CLR_0] =                      0x0424,
	[PMIF_IRQ_EVENT_EN_1] =                 0x0428,
	[PMIF_IRQ_FLAG_1] =                     0x0430,
	[PMIF_IRQ_CLR_1] =                      0x0434,
	[PMIF_IRQ_EVENT_EN_2] =                 0x0438,
	[PMIF_IRQ_FLAG_2] =                     0x0440,
	[PMIF_IRQ_CLR_2] =                      0x0444,
	[PMIF_IRQ_EVENT_EN_3] =                 0x0448,
	[PMIF_IRQ_FLAG_3] =                     0x0450,
	[PMIF_IRQ_CLR_3] =                      0x0454,
	[PMIF_IRQ_EVENT_EN_4] =                 0x0458,
	[PMIF_IRQ_FLAG_4] =                     0x0460,
	[PMIF_IRQ_CLR_4] =                      0x0464,
	[PMIF_WDT_EVENT_EN_0] =			0x046C,
	[PMIF_WDT_FLAG_0] =			0x0470,
	[PMIF_WDT_EVENT_EN_1] =			0x0474,
	[PMIF_WDT_FLAG_1] =			0x0478,
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

static const u32 mt6xxx_spmi_regs[] = {
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

static const u32 mt6853_spmi_regs[] = {
	[SPMI_OP_ST_CTRL] =			0x0000,
	[SPMI_GRP_ID_EN] =			0x0004,
	[SPMI_OP_ST_STA] =			0x0008,
	[SPMI_MST_SAMPL] =			0x000C,
	[SPMI_MST_REQ_EN] =			0x0010,
	[SPMI_MST_RCS_CTRL] =			0x0014,
	[SPMI_SLV_3_0_EINT] =			0x0020,
	[SPMI_SLV_7_4_EINT] =			0x0024,
	[SPMI_SLV_B_8_EINT] =			0x0028,
	[SPMI_SLV_F_C_EINT] =			0x002C,
	[SPMI_REC_CTRL] =			0x0040,
	[SPMI_REC0] =				0x0044,
	[SPMI_REC1] =				0x0048,
	[SPMI_REC2] =				0x004C,
	[SPMI_REC3] =				0x0050,
	[SPMI_REC4] =				0x0054,
	[SPMI_REC_CMD_DEC] =			0x005C,
	[SPMI_DEC_DBG] =			0x00F8,
	[SPMI_MST_DBG] =			0x00FC,
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

enum infra_regs {
	MODULE_SW_CG_0_SET,
	MODULE_SW_CG_0_CLR,
	MODULE_SW_CG_2_SET,
	MODULE_SW_CG_2_CLR,
	PMICW_CLOCK_CTRL,
	INFRA_GLOBALCON_RST2_SET,
	INFRA_GLOBALCON_RST2_CLR,
	INFRA_GLOBALCON_RST2,
};

static const u32 mt6xxx_infra_regs[] = {
	[MODULE_SW_CG_0_SET] =			0x0080,
	[MODULE_SW_CG_0_CLR] =			0x0084,
	[MODULE_SW_CG_2_SET] =			0x00a4,
	[MODULE_SW_CG_2_CLR] =			0x00a8,
	[PMICW_CLOCK_CTRL] =			0x0108,
	[INFRA_GLOBALCON_RST2_SET] =		0x0140,
	[INFRA_GLOBALCON_RST2_CLR] =		0x0144,
	[INFRA_GLOBALCON_RST2] =		0x0148,
};

enum topckgen_regs {
	CLK_CFG_UPDATE1,
	CLK_CFG_UPDATE2,
	CLK_CFG_8_CLR,
	CLK_CFG_8,
	CLK_CFG_15,
	CLK_CFG_16,
	CLK_CFG_15_CLR,
	CLK_CFG_16_CLR,
};

static const u32 mt6xxx_topckgen_regs[] = {
	[CLK_CFG_UPDATE1] =			0x0008,
	[CLK_CFG_UPDATE2] =			0x000c,
	[CLK_CFG_8] =				0x0090,
	[CLK_CFG_8_CLR] =			0x0098,
	[CLK_CFG_15] =				0x0100,
	[CLK_CFG_15_CLR] =			0x0108,
	[CLK_CFG_16] =				0x0110,
	[CLK_CFG_16_CLR] =			0x0118,
};

enum toprgu_regs {
	WDT_SWSYSRST2,
};

static const u32 mt6xxx_toprgu_regs[] = {
	[WDT_SWSYSRST2] =			0x0090,
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
};
struct pmif_irq_desc {
	const char *name;
	irq_handler_t irq_handler;
	int irq;
};

#define PMIF_IRQDESC(name) { #name, pmif_##name##_irq_handler, -1}

/*
 * pmif internal API declaration
 */
int __attribute__((weak)) spmi_pmif_create_attr(struct device_driver *driver);
int __attribute__((weak)) spmi_pmif_dbg_init(struct spmi_controller *ctrl);
void __attribute__((weak)) spmi_dump_spmimst_record_reg(struct pmif *arb);
static int pmif_timeout_ns(struct spmi_controller *ctrl,
	unsigned long long start_time_ns, unsigned long long timeout_time_ns);
static void pmif_enable_soft_reset(struct pmif *arb);
static void pmif_enable_soft_reset_p_v1(struct pmif *arb);
static void pmif_spmi_enable_clk_set(struct pmif *arb);
static void pmif_spmi_force_normal_mode(struct pmif *arb);
static void pmif_spmi_enable_swinf(struct pmif *arb,
	unsigned int chan_no, unsigned int swinf_no);
static void pmif_spmi_enable_cmdIssue(struct pmif *arb, bool en);
static void pmif_spmi_enable(struct pmif *arb);
static void pmif_spmi_enable_m_v1(struct pmif *arb);
static void pmif_spmi_enable_p_v1(struct pmif *arb);
static int is_pmif_spmi_init_done(struct pmif *arb);
static int pmif_spmi_read_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid,
		u16 addr, u8 *buf, size_t len);
static int pmif_spmi_write_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid,
		u16 addr, const u8 *buf, size_t len);
static int mtk_spmi_config_master(struct pmif *arb,
		unsigned int mstid, bool en);
static int mtk_spmi_config_master_m_v1(struct pmif *arb,
		unsigned int mstid, bool en);
static int mtk_spmi_config_master_p_v1(struct pmif *arb,
		unsigned int mstid, bool en);
static int mtk_spmi_cali_rd_clock_polarity(struct pmif *arb);
static int mtk_spmi_cali_rd_clock_polarity_m_p_v1(struct pmif *arb);
static int mtk_spmi_ctrl_op_st(struct spmi_controller *ctrl,
			u8 opc, u8 sid);
static int mtk_spmi_enable_group_id(struct pmif *arb, u8 grpid);

static struct pmif mt6xxx_pmif_arb[] = {
	{
		.base = 0x0,
		.regs = mt6xxx_regs,
		.spmimst_base = 0x0,
		.spmimst_regs = mt6xxx_spmi_regs,
		.infra_base = 0x0,
		.infra_regs = mt6xxx_infra_regs,
		.topckgen_base = 0x0,
		.topckgen_regs = mt6xxx_topckgen_regs,
		.toprgu_base = 0x0,
		.toprgu_regs = mt6xxx_toprgu_regs,
		.swinf_ch_start = 0,
		.ap_swinf_no = 0,
		.write = 0x0,
		.mstid = SPMI_MASTER_0,
		.pmifid = PMIF_PMIFID_SPMI0,
		.spmic = 0x0,
		.read_cmd = pmif_spmi_read_cmd,
		.write_cmd = pmif_spmi_write_cmd,
		.pmif_enable_clk_set = pmif_spmi_enable_clk_set,
		.pmif_force_normal_mode = pmif_spmi_force_normal_mode,
		.pmif_enable_swinf = pmif_spmi_enable_swinf,
		.pmif_enable_cmdIssue = pmif_spmi_enable_cmdIssue,
		.pmif_enable = pmif_spmi_enable,
		.is_pmif_init_done = is_pmif_spmi_init_done,
		.pmif_enable_reset = pmif_enable_soft_reset,
		.pmif_cali_clock = mtk_spmi_cali_rd_clock_polarity,
		.spmi_config_master = mtk_spmi_config_master,
	},
};

static struct pmif mt6xxx_pmif_m_arb[] = {
	{
		.base = 0x0,
		.regs = mt6853_regs,
		.spmimst_base = 0x0,
		.spmimst_regs = mt6853_spmi_regs,
		.infra_base = 0x0,
		.infra_regs = mt6xxx_infra_regs,
		.topckgen_base = 0x0,
		.topckgen_regs = mt6xxx_topckgen_regs,
		.toprgu_base = 0x0,
		.toprgu_regs = mt6xxx_toprgu_regs,
		.swinf_ch_start = 0,
		.ap_swinf_no = 0,
		.write = 0x0,
		.mstid = SPMI_MASTER_1,
		.pmifid = PMIF_PMIFID_SPMI1,
		.spmic = 0x0,
		.read_cmd = pmif_spmi_read_cmd,
		.write_cmd = pmif_spmi_write_cmd,
		.pmif_enable_clk_set = pmif_spmi_enable_clk_set,
		.pmif_force_normal_mode = pmif_spmi_force_normal_mode,
		.pmif_enable_swinf = pmif_spmi_enable_swinf,
		.pmif_enable_cmdIssue = pmif_spmi_enable_cmdIssue,
		.pmif_enable = pmif_spmi_enable_m_v1,
		.is_pmif_init_done = is_pmif_spmi_init_done,
		.pmif_enable_reset = pmif_enable_soft_reset,
		.pmif_cali_clock = mtk_spmi_cali_rd_clock_polarity_m_p_v1,
		.spmi_config_master = mtk_spmi_config_master_m_v1,
	},
};

static struct pmif mt6xxx_pmif_m_arb_v2[] = {
	{
		.base = 0x0,
		.regs = mt6833_regs,
		.spmimst_base = 0x0,
		.spmimst_regs = mt6853_spmi_regs,
		.infra_base = 0x0,
		.infra_regs = mt6xxx_infra_regs,
		.topckgen_base = 0x0,
		.topckgen_regs = mt6xxx_topckgen_regs,
		.toprgu_base = 0x0,
		.toprgu_regs = mt6xxx_toprgu_regs,
		.swinf_ch_start = 0,
		.ap_swinf_no = 0,
		.write = 0x0,
		.mstid = SPMI_MASTER_1,
		.pmifid = PMIF_PMIFID_SPMI1,
		.spmic = 0x0,
		.read_cmd = pmif_spmi_read_cmd,
		.write_cmd = pmif_spmi_write_cmd,
		.pmif_enable_clk_set = pmif_spmi_enable_clk_set,
		.pmif_force_normal_mode = pmif_spmi_force_normal_mode,
		.pmif_enable_swinf = pmif_spmi_enable_swinf,
		.pmif_enable_cmdIssue = pmif_spmi_enable_cmdIssue,
		.pmif_enable = pmif_spmi_enable_m_v1,
		.is_pmif_init_done = is_pmif_spmi_init_done,
		.pmif_enable_reset = pmif_enable_soft_reset,
		.pmif_cali_clock = mtk_spmi_cali_rd_clock_polarity_m_p_v1,
		.spmi_config_master = mtk_spmi_config_master_m_v1,
	},
};

static struct pmif mt6xxx_pmif_p_arb[] = {
	{
		.base = 0x0,
		.regs = mt6853_regs,
		.spmimst_base = 0x0,
		.spmimst_regs = mt6853_spmi_regs,
		.infra_base = 0x0,
		.infra_regs = mt6xxx_infra_regs,
		.topckgen_base = 0x0,
		.topckgen_regs = mt6xxx_topckgen_regs,
		.toprgu_base = 0x0,
		.toprgu_regs = mt6xxx_toprgu_regs,
		.swinf_ch_start = 0,
		.ap_swinf_no = 0,
		.write = 0x0,
		.mstid = SPMI_MASTER_1,
		.pmifid = PMIF_PMIFID_SPMI2,
		.spmic = 0x0,
		.read_cmd = pmif_spmi_read_cmd,
		.write_cmd = pmif_spmi_write_cmd,
		.pmif_enable_clk_set = pmif_spmi_enable_clk_set,
		.pmif_force_normal_mode = pmif_spmi_force_normal_mode,
		.pmif_enable_swinf = pmif_spmi_enable_swinf,
		.pmif_enable_cmdIssue = pmif_spmi_enable_cmdIssue,
		.pmif_enable = pmif_spmi_enable_p_v1,
		.is_pmif_init_done = is_pmif_spmi_init_done,
		.pmif_enable_reset = pmif_enable_soft_reset_p_v1,
		.pmif_cali_clock = mtk_spmi_cali_rd_clock_polarity_m_p_v1,
		.spmi_config_master = mtk_spmi_config_master_p_v1,
	},
};

static const struct of_device_id pmif_match_table[] = {
	{
		.compatible = "mediatek,mt6833-pmif-m",
		.data = &mt6xxx_pmif_m_arb_v2,
	}, {
		.compatible = "mediatek,mt6853-pmif-m",
		.data = &mt6xxx_pmif_m_arb,
	}, {
		.compatible = "mediatek,mt6853-pmif-p",
		.data = &mt6xxx_pmif_p_arb,
	}, {
		.compatible = "mediatek,mt6877-pmif-m",
		.data = &mt6xxx_pmif_m_arb_v2,
	}, {
		.compatible = "mediatek,mt6885-pmif",
		.data = &mt6xxx_pmif_arb,
	}, {
		.compatible = "mediatek,pmif",
		.data = &mt6xxx_pmif_arb,
	}, {
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, pmif_match_table);
static struct platform_driver pmif_driver;

/*
 * pmif timeout
 */
#if PMIF_TIMEOUT
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
static int pmif_timeout_ns(struct spmi_controller *ctrl,
	unsigned long long start_time_ns, unsigned long long timeout_time_ns)
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
	u32 offset = 0, offset1 = 0, offset2 = 0;

	start_time_ns = sched_clock();
	timeout_ns = 10000 * 1000;  /* 10000us */

	offset = arb->toprgu_regs[WDT_SWSYSRST2];
	offset1 = arb->infra_regs[INFRA_GLOBALCON_RST2];
	offset2 = arb->topckgen_regs[CLK_CFG_8];
	do {
		if (pmif_timeout_ns(ctrl, start_time_ns, timeout_ns)) {
			dev_notice(&ctrl->dev,
				"[PMIF] WDT_RST2:0x%x INF_RST2:0x%x CLK:0x%x\n",
				readl(arb->toprgu_base + offset),
				readl(arb->infra_base + offset1),
				readl(arb->topckgen_base + offset2));
			if (fp(arb)) {
				return 0;
			} else if (fp(arb) == 0) {
				dev_notice(&ctrl->dev, "[PMIF] FSM Timeout\n");
				spmi_dump_pmif_swinf_reg();
				spmi_dump_pmif_all_reg();
				spmi_dump_spmimst_all_reg();
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
u32 mtk_spmi_readl(struct pmif *arb, enum spmi_regs reg)
{
	return readl(arb->spmimst_base + arb->spmimst_regs[reg]);
}

/*
 * Function : mtk_spmi_writel()
 * Description : mtk spmi controller write api
 * Parameter :
 * Return :
 */
void mtk_spmi_writel(struct pmif *arb, u32 val,
		enum spmi_regs reg)
{
	writel(val, arb->spmimst_base + arb->spmimst_regs[reg]);
}

static void pmif_enable_soft_reset(struct pmif *arb)
{
	writel(0x1 << 14,
		arb->infra_base + arb->infra_regs[INFRA_GLOBALCON_RST2_SET]);
	writel(0x1 << 14,
		arb->infra_base + arb->infra_regs[INFRA_GLOBALCON_RST2_CLR]);
}

static void pmif_enable_soft_reset_p_v1(struct pmif *arb)
{
	writel(0x1 << 15,
		arb->infra_base + arb->infra_regs[INFRA_GLOBALCON_RST2_SET]);
	writel(0x1 << 15,
		arb->infra_base + arb->infra_regs[INFRA_GLOBALCON_RST2_CLR]);
}

static void pmif_spmi_enable_clk_set(struct pmif *arb)
{
	/* sys_ck cg enable, turn off clock */
	writel(0x0000000f,
		arb->infra_base + arb->infra_regs[MODULE_SW_CG_0_SET]);

	writel((0x1 << 15) | (0x1 << 12) | (0x7 << 8),
		arb->topckgen_base + arb->topckgen_regs[CLK_CFG_8_CLR]);
	writel(0x1 << 2,
		arb->topckgen_base + arb->topckgen_regs[CLK_CFG_UPDATE1]);

	/* toggle SPMI sw reset */
	arb->pmif_enable_reset(arb);

	/* sys_ck cg enable, turn on clock */
	writel(0x0000000f,
		arb->infra_base + arb->infra_regs[MODULE_SW_CG_0_CLR]);
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
	pmif_writel(arb, 0x2F7, PMIF_INF_EN);
	pmif_writel(arb, 0x2F7, PMIF_ARB_EN);
	pmif_writel(arb, 0x3, PMIF_TIMER_CTRL);
	pmif_writel(arb, 0x1, PMIF_INIT_DONE);
}

static void pmif_spmi_enable_m_v1(struct pmif *arb)
{
	pmif_writel(arb, 0x2F6, PMIF_INF_EN);
	pmif_writel(arb, 0x2F6, PMIF_ARB_EN);
	pmif_writel(arb, 0x3, PMIF_TIMER_CTRL);
	pmif_writel(arb, 0x1, PMIF_INIT_DONE);
}

static void pmif_spmi_enable_p_v1(struct pmif *arb)
{
	pmif_writel(arb, 0xF1, PMIF_INF_EN);
	pmif_writel(arb, 0xF1, PMIF_ARB_EN);
	pmif_writel(arb, 0x3, PMIF_TIMER_CTRL);
	pmif_writel(arb, 0x1, PMIF_INIT_DONE);
}

static int mtk_spmi_ctrl_op_st(struct spmi_controller *ctrl,
			u8 opc, u8 sid)
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

	mtk_spmi_writel(arb, (cmd << 0x4) | sid, SPMI_OP_ST_CTRL);

	rdata = mtk_spmi_readl(arb, SPMI_OP_ST_CTRL);
	pr_notice("[SPMIMST]:pmif_ctrl_op_st 0x%x\r\n", rdata);

	do {
		rdata = mtk_spmi_readl(arb, SPMI_OP_ST_STA);
		pr_notice("[SPMIMST]:pmif_ctrl_op_st 0x%x\r\n", rdata);

		if (((rdata >> 0x1) & SPMI_OP_ST_NACK) == SPMI_OP_ST_NACK) {
			spmi_dump_spmimst_record_reg(arb);
			break;
		}
	} while ((rdata & SPMI_OP_ST_BUSY) == SPMI_OP_ST_BUSY);

	return 0;
}

/* Non-data command */
static int pmif_arb_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid)
{
	return mtk_spmi_ctrl_op_st(ctrl, opc, sid);
}

static int mtk_spmi_enable_group_id(struct pmif *arb, u8 grpid)
{
	mtk_spmi_writel(arb, 0x1 << grpid, SPMI_GRP_ID_EN);

	return 0;
}

static int pmif_spmi_read_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid,
		u16 addr, u8 *buf, size_t len)
{
	struct pmif *arb = spmi_controller_get_drvdata(ctrl);
	int ret = 0, write = 0x0;
	u32 offset = 0, data = 0;
	u8 bc = len - 1;
	unsigned long flags;

	/* Check for argument validation. */
	if ((arb->ap_swinf_no & ~(0x3)) != 0x0)
		return -EINVAL;

	if ((arb->pmifid & ~(0x3)) != 0x0)
		return -EINVAL;

	if ((sid & ~(0xf)) != 0x0)
		return -EINVAL;

	/* Check the opcode */
	if (opc >= 0x60 && opc <= 0x7F)
		opc = PMIF_CMD_REG;
	else if (opc >= 0x20 && opc <= 0x2F)
		opc = PMIF_CMD_EXT_REG_LONG; /* wk1 opc = PMIF_CMD_EXT_REG; */
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

		data = readl(arb->base + offset);
		memcpy(buf, &data, (bc & 3) + 1);
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
	u32 offset = 0, data = 0;
	u8 bc = len - 1;
	unsigned long flags = 0;

	/* Check for argument validation. */
	if ((arb->ap_swinf_no & ~(0x3)) != 0x0)
		return -EINVAL;

	if ((arb->pmifid & ~(0x3)) != 0x0)
		return -EINVAL;

	if ((sid & ~(0xf)) != 0x0)
		return -EINVAL;

	/* Check the opcode */
	if (opc >= 0x40 && opc <= 0x5F)
		opc = PMIF_CMD_REG;
	else if (opc <= 0x0F)
		opc = PMIF_CMD_EXT_REG_LONG; /* wk1 opc = PMIF_CMD_EXT_REG; */
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
	if (write == 1) {
		memcpy(&data, buf, (bc & 3) + 1);
		writel(data, arb->base + offset);
	}

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


static int mtk_spmi_config_master(struct pmif *arb,
		unsigned int mstid, bool en)
{
	/* Software reset */
	writel(0x85 << 24 | 0x1 << 4,
		arb->toprgu_base + arb->toprgu_regs[WDT_SWSYSRST2]);

	writel(0x7 | (0x1 << 4) | (0x1 << 7),
		arb->topckgen_base + arb->topckgen_regs[CLK_CFG_16_CLR]);
	writel(0x1 << 2,
		arb->topckgen_base + arb->topckgen_regs[CLK_CFG_UPDATE2]);

	/* Software reset */
	writel(0x85 << 24,
		arb->toprgu_base + arb->toprgu_regs[WDT_SWSYSRST2]);

	/* Enable SPMI */
	mtk_spmi_writel(arb, en, SPMI_MST_REQ_EN);

	return 0;
}

static int mtk_spmi_config_master_m_v1(struct pmif *arb,
		unsigned int mstid, bool en)
{
	/* Software reset */
	writel(0x85 << 24 | 0x3 << 3,
		arb->toprgu_base + arb->toprgu_regs[WDT_SWSYSRST2]);

	writel((0x7 << 8) | (0x1 << 12) | (0x1 << 15),
		arb->topckgen_base + arb->topckgen_regs[CLK_CFG_15_CLR]);
	writel(0x1 << 30,
		arb->topckgen_base + arb->topckgen_regs[CLK_CFG_UPDATE2]);

	/* Software reset */
	writel(0x85 << 24,
		arb->toprgu_base + arb->toprgu_regs[WDT_SWSYSRST2]);

	/* Enable master rcs */
	mtk_spmi_writel(arb, 0x14 | arb->mstid, SPMI_MST_RCS_CTRL);
	/* Enable SPMI */
	mtk_spmi_writel(arb, en, SPMI_MST_REQ_EN);

	return 0;
}

static int mtk_spmi_config_master_p_v1(struct pmif *arb,
		unsigned int mstid, bool en)
{
	/* Enable master rcs */
	mtk_spmi_writel(arb, 0x14 | arb->mstid, SPMI_MST_RCS_CTRL);
	/* Enable SPMI */
	mtk_spmi_writel(arb, en, SPMI_MST_REQ_EN);

	return 0;
}

static int mtk_spmi_cali_rd_clock_polarity(struct pmif *arb)
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

static int mtk_spmi_cali_rd_clock_polarity_m_p_v1(struct pmif *arb)
{
#if defined(CONFIG_MTK_FPGA) || defined(CONFIG_FPGA_EARLY_PORTING)
	unsigned int dly = 0, pol = 0;
#else
	unsigned int dly = 0, pol = 1;
#endif

	/* Indicate sampling clock polarity, 1: Positive 0: Negative  */
	mtk_spmi_writel(arb,
		((dly+1) << 0x4) | (dly << 0x1) | pol, SPMI_MST_SAMPL);

	return 0;
}

/*
 * PMIF Exception IRQ Handler
 */
static void pmif_cmd_err_parity_err_irq_handler(int irq, void *data)
{
	spmi_dump_spmimst_all_reg();
	spmi_dump_pmif_record_reg();
	aee_kernel_warning("PMIF:parity error", "PMIF");
}

static void pmif_pmif_acc_vio_irq_handler(int irq, void *data)
{
	spmi_dump_pmif_acc_vio_reg();
	aee_kernel_warning("PMIF:pmif_acc_vio", "PMIF");
}

static void pmif_pmic_acc_vio_irq_handler(int irq, void *data)
{
	spmi_dump_pmic_acc_vio_reg();
	aee_kernel_warning("PMIF:pmic_acc_vio", "PMIF");
}

static void pmif_lat_limit_reached_irq_handler(int irq, void *data)
{
	spmi_dump_pmif_busy_reg();
	spmi_dump_pmif_record_reg();
}

static void pmif_hw_monitor_irq_handler(int irq, void *data)
{
	spmi_dump_pmif_record_reg();
	aee_kernel_warning("PMIF:pmif_hw_monitor_match", "PMIF");
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

	for (idx = 0; idx < 31; idx++) {
		if ((irq_f & (0x1 << idx)) != 0) {
			switch (idx) {
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

	for (idx = 0; idx < 31; idx++) {
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

	for (idx = 0; idx < 31; idx++) {
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

	for (idx = 0; idx < 31; idx++) {
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

	for (idx = 0; idx < 31; idx++) {
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

static int mtk_spmimst_init(struct platform_device *pdev, struct pmif *arb)
{
	struct resource *res = NULL;
	int err = 0;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "spmimst");
	arb->spmimst_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(arb->spmimst_base)) {
		err = PTR_ERR(arb->spmimst_base);
		return err;
	}

	err = of_property_read_u32(pdev->dev.of_node, "grpid", &arb->grpid);
	if (err) {
		dev_notice(&pdev->dev, "[SPMIMST]:grpid unspecified.\n");
		return -EINVAL;
	}
	/* set group id */
	mtk_spmi_enable_group_id(arb, arb->grpid);

	/* if spmimst not enabled, enable it */
	if ((mtk_spmi_readl(arb, SPMI_MST_REQ_EN) & 0x1) != 0x1) {
		dev_info(&pdev->dev, "[SPMIMST]:enable spmimst.\n");
		arb->spmi_config_master(arb, arb->mstid, true);
		arb->pmif_cali_clock(arb);

	}
	pr_notice("[SPMIMST]:%s done\n", __func__);

	return 0;
}


static int pmif_probe(struct platform_device *pdev)
{
	struct device_node *node = NULL;
	const struct of_device_id *of_id =
		of_match_device(pmif_match_table, &pdev->dev);
	struct spmi_controller *ctrl = NULL;
	struct pmif *arb = NULL;
	struct resource *res = NULL;
	u32 swinf_ch_start = 0, ap_swinf_no = 0;
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	int pmif_clk26m = 0, spmimst_clk26m = 0;
#endif
	int err = 0;

#if PMIF_BRINGUP
	dev_notice(&pdev->dev, "[PMIF]bringup do nothing\n");
	return 0;
#endif
	if (!of_id) {
		dev_notice(&pdev->dev, "[PMIF]:Error: No device match found\n");
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
	/* pmif is not initialized, just init once */
	node = of_find_compatible_node(NULL, NULL,
			"mediatek,infracfg_ao");
	arb->infra_base = of_iomap(node, 0);
	dev_info(&pdev->dev, "[PMIF]:mtk-pmif arb infra ao base:0x%x\n",
			arb->infra_base);
	if (IS_ERR(arb->infra_base)) {
		err = PTR_ERR(arb->infra_base);
		goto err_put_ctrl;
	}

	node = of_find_compatible_node(NULL, NULL,
			"mediatek,topckgen");
	arb->topckgen_base = of_iomap(node, 0);
	dev_info(&pdev->dev, "[PMIF]:mtk-pmif arb topckgen base:0x%x\n",
			arb->topckgen_base);
	if (IS_ERR(arb->topckgen_base)) {
		err = PTR_ERR(arb->topckgen_base);
		goto err_put_ctrl;
	}

	node = of_find_compatible_node(NULL, NULL,
			"mediatek,toprgu");
	arb->toprgu_base = of_iomap(node, 0);
	dev_info(&pdev->dev, "[PMIF]:mtk-pmif arb toprgu base:0x%x\n",
			arb->toprgu_base);
	if (IS_ERR(arb->toprgu_base)) {
		err = PTR_ERR(arb->toprgu_base);
		goto err_put_ctrl;
	}
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	/* get pmif infracfg_ao clock */
	arb->pmif_sys_ck = devm_clk_get(&pdev->dev, "pmif_sys_ck");
	if (IS_ERR(arb->pmif_sys_ck)) {
		dev_notice(&pdev->dev, "[PMIF]:failed to get ap clock: %ld\n",
			PTR_ERR(arb->pmif_sys_ck));
		return PTR_ERR(arb->pmif_sys_ck);
	}

	arb->pmif_tmr_ck = devm_clk_get(&pdev->dev, "pmif_tmr_ck");
	if (IS_ERR(arb->pmif_tmr_ck)) {
		dev_notice(&pdev->dev, "[PMIF]:failed to get tmr clock: %ld\n",
			PTR_ERR(arb->pmif_tmr_ck));
		return PTR_ERR(arb->pmif_tmr_ck);
	}

	/* get pmif topckgen clock */
	arb->pmif_clk_mux = devm_clk_get(&pdev->dev, "pmif_clk_mux");
	if (IS_ERR(arb->pmif_clk_mux)) {
		dev_notice(&pdev->dev, "[PMIF]:failed to get clock: %ld\n",
			PTR_ERR(arb->pmif_clk_mux));
		return PTR_ERR(arb->pmif_clk_mux);
	}

	arb->pmif_clk_osc_d10 = devm_clk_get(&pdev->dev, "pmif_clk_osc_d10");
	if (IS_ERR(arb->pmif_clk_osc_d10)) {
		dev_notice(&pdev->dev, "[PMIF]:failed to get clock: %ld\n",
			PTR_ERR(arb->pmif_clk_osc_d10));
		return PTR_ERR(arb->pmif_clk_osc_d10);
	}

	arb->pmif_clk26m = devm_clk_get(&pdev->dev, "pmif_clk26m");
	if (IS_ERR(arb->pmif_clk26m)) {
		dev_notice(&pdev->dev, "[PMIF]:failed to get clock: %ld\n",
			PTR_ERR(arb->pmif_clk26m));
		return PTR_ERR(arb->pmif_clk26m);
	}

	/* now enable pmif/spmimst clock */
	pmif_clk26m =
		readl(arb->infra_base + arb->infra_regs[PMICW_CLOCK_CTRL]);

	if ((pmif_clk26m & 0x1) == 0x1) {
		dev_info(&pdev->dev, "[PMIF]:enable clk26m.\n");
		err = clk_prepare_enable(arb->pmif_clk26m);
		if (err)
			return err;
	} else {
		dev_info(&pdev->dev, "[PMIF]:enable ulposc1 osc d10.\n");
		err = clk_prepare_enable(arb->pmif_clk_mux);
		if (err)
			return err;
		err = clk_set_parent(arb->pmif_clk_mux, arb->pmif_clk_osc_d10);
		if (err)
			return err;
	}
	err = clk_prepare_enable(arb->pmif_sys_ck);
	if (err)
		return err;
	err = clk_prepare_enable(arb->pmif_tmr_ck);
	if (err)
		return err;

	/* get spmimst topckgen clock */
	arb->spmimst_clk_mux = devm_clk_get(&pdev->dev, "spmimst_clk_mux");
	if (IS_ERR(arb->spmimst_clk_mux)) {
		dev_notice(&pdev->dev, "[SPMIMST]:failed to get clock: %ld\n",
			PTR_ERR(arb->spmimst_clk_mux));
		return PTR_ERR(arb->spmimst_clk_mux);
	}
	arb->spmimst_clk26m = devm_clk_get(&pdev->dev, "spmimst_clk26m");
	if (IS_ERR(arb->spmimst_clk26m)) {
		dev_notice(&pdev->dev, "[SPMIMST]:failed to get clock: %ld\n",
			PTR_ERR(arb->spmimst_clk26m));
		return PTR_ERR(arb->spmimst_clk26m);
	}
	arb->spmimst_clk_osc_d10 = devm_clk_get(&pdev->dev,
			"spmimst_clk_osc_d10");
	if (IS_ERR(arb->spmimst_clk_osc_d10)) {
		dev_notice(&pdev->dev, "[SPMIMST]:failed to get clock: %ld\n",
			PTR_ERR(arb->spmimst_clk_osc_d10));
		return PTR_ERR(arb->spmimst_clk_osc_d10);
	}
	err = clk_prepare_enable(arb->spmimst_clk_mux);
	if (err)
		return err;

	if (of_device_is_compatible(ctrl->dev.parent->of_node,
				    "mediatek,mt6885-pmif")) {
		spmimst_clk26m =
		readl(arb->topckgen_base + arb->topckgen_regs[CLK_CFG_16]);
	} else {
		spmimst_clk26m =
		readl(arb->topckgen_base + arb->topckgen_regs[CLK_CFG_15]);
		spmimst_clk26m = (spmimst_clk26m >> 0x8) & 0x7;
	}

	if ((spmimst_clk26m & 0x7) == 0) {
		dev_info(&pdev->dev, "[SPMIMST]:enable clk26m.\n");
		err = clk_set_parent(arb->spmimst_clk_mux,
				arb->spmimst_clk26m);
		if (err)
			return err;
	} else if ((spmimst_clk26m & 0x7) == 0x3) {
		dev_info(&pdev->dev, "[SPMIMST]:enable ulposc1 osc d10.\n");
		err = clk_set_parent(arb->spmimst_clk_mux,
				arb->spmimst_clk_osc_d10);
		if (err)
			return err;
	}
#else
	dev_notice(&pdev->dev, "[PMIF]:no need to get clock at fpga\n");
#endif /* #if defined(CONFIG_MTK_FPGA) || defined(CONFIG_FPGA_EARLY_PORTING) */
	err = of_property_read_u32(pdev->dev.of_node,
			"swinf_ch_start", &swinf_ch_start);
	if (err) {
		dev_notice(&pdev->dev, "[PMIF]:swinf_ch_start unspecified.\n");
		goto err_put_ctrl;
	}
	arb->swinf_ch_start = swinf_ch_start;

	err = of_property_read_u32(pdev->dev.of_node,
			"ap_swinf_no", &ap_swinf_no);
	if (err) {
		dev_notice(&pdev->dev, "[PMIF]:ap_swinf_no unspecified.\n");
		goto err_put_ctrl;
	}
	arb->ap_swinf_no = ap_swinf_no;

	if (arb->is_pmif_init_done(arb) != 0) {
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
	arb->pmifThread_lock =
		wakeup_source_register(NULL, "pmif wakelock");
	mutex_init(&arb->pmif_mutex);

	ctrl->cmd = pmif_arb_cmd;
	ctrl->read_cmd = pmif_spmi_read_cmd;
	ctrl->write_cmd = pmif_spmi_write_cmd;

	if (arb->is_pmif_init_done(arb) == 0) {
		/* pmif already done, call spmi master driver init */
		err = mtk_spmimst_init(pdev, arb);
		if (err)
			goto err_put_ctrl;
	}
	/* enable debugger */
	spmi_pmif_dbg_init(ctrl);
	spmi_pmif_create_attr(&pmif_driver.driver);

	arb->irq = platform_get_irq_byname(pdev, "pmif_irq");
	if (arb->irq < 0) {
		err = arb->irq;
		goto err_put_ctrl;
	}

	pmif_irq_register(pdev, arb, arb->irq);

	platform_set_drvdata(pdev, ctrl);

	err = spmi_controller_add(ctrl);
	if (err)
		goto err_domain_remove;

	return 0;

err_domain_remove:
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	clk_disable_unprepare(arb->spmimst_clk_mux);
	if ((pmif_clk26m & 0x1) == 0x1)
		clk_disable_unprepare(arb->pmif_clk26m);
	else
		clk_disable_unprepare(arb->pmif_clk_mux);
#endif
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
