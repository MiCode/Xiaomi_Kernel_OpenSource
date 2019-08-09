/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: James Liao <jamesjj.liao@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/of.h>
#include <linux/of_address.h>

#include <linux/io.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/clk.h>


#include "clk-mtk-v1.h"
#include "clk-mt6765-pg.h"

#include <dt-bindings/clock/mt6765-clk.h>

/*#define TOPAXI_PROTECT_LOCK*/
/* need to ignore for bring up */
#ifdef CONFIG_FPGA_EARLY_PORTING
#define IGNORE_MTCMOS_CHECK
#endif
#if !defined(MT_CCF_DEBUG) \
		|| !defined(CLK_DEBUG) || !defined(DUMMY_REG_TEST)
#define MT_CCF_DEBUG	0
#define CONTROL_LIMIT	0
#define CLK_DEBUG	0
#define DUMMY_REG_TEST	0
#endif

#define	CHECK_PWR_ST	1

#define CONN_TIMEOUT_RECOVERY 5
#define CONN_TIMEOUT_STEP1	4

#ifndef GENMASK
#define GENMASK(h, l)	(((U32_C(1) << ((h) - (l) + 1)) - 1) << (l))
#endif

#ifdef CONFIG_ARM64
#define IOMEM(a)	((void __force __iomem *)((a)))
#endif

#define mt_reg_sync_writel(v, a) \
	do { \
		__raw_writel((v), IOMEM(a)); \
		/* sync up */ \
		mb(); } \
while (0)
#define spm_read(addr)			__raw_readl(IOMEM(addr))
#define spm_write(addr, val)		mt_reg_sync_writel(val, addr)

#define clk_writel(addr, val)   \
	mt_reg_sync_writel(val, addr)

#define clk_readl(addr)			__raw_readl(IOMEM(addr))

/*MM Bus*/
#ifdef CONFIG_OF
void __iomem *clk_mmsys_config_base;
void __iomem *clk_imgsys_base;
void __iomem *clk_venc_gcon_base;
void __iomem *clk_camsys_base;
#endif




/*
 * MTCMOS
 */

#define STA_POWER_DOWN	0
#define STA_POWER_ON	1
#define SUBSYS_PWR_DOWN		0
#define SUBSYS_PWR_ON		1

struct subsys;

struct subsys_ops {
	int (*prepare)(struct subsys *sys);
	int (*unprepare)(struct subsys *sys);
	int (*enable)(struct subsys *sys);
	int (*disable)(struct subsys *sys);
	int (*get_state)(struct subsys *sys);
};

struct subsys {
	const char *name;
	uint32_t sta_mask;
	void __iomem *ctl_addr;
	uint32_t sram_pdn_bits;
	uint32_t sram_pdn_ack_bits;
	uint32_t bus_prot_mask;
	struct subsys_ops *ops;
};

/*static struct subsys_ops general_sys_ops;*/

static struct subsys_ops MD1_sys_ops;
static struct subsys_ops CONN_sys_ops;
static struct subsys_ops DPY_sys_ops;
static struct subsys_ops DIS_sys_ops;
static struct subsys_ops MFG_sys_ops;
static struct subsys_ops ISP_sys_ops;
static struct subsys_ops IFR_sys_ops;
static struct subsys_ops MFG_CORE0_sys_ops;
static struct subsys_ops MFG_ASYNC_sys_ops;
static struct subsys_ops CAM_sys_ops;
static struct subsys_ops VCODEC_sys_ops;

static void __iomem *infracfg_base;/* infracfg_ao */
static void __iomem *spm_base;/* spm */
static void __iomem *infra_base;/* infra */
static void __iomem *smi_common_base;/* smi_common */
static void __iomem *conn_base; /* connsys*/
static void __iomem *conn_mcu_base; /* connsys MCU */

#define INFRACFG_REG(offset)		(infracfg_base + offset)
#define SPM_REG(offset)			(spm_base + offset)
#define INFRA_REG(offset)		(infra_base + offset)
#define SMI_COMMON_REG(offset)	(smi_common_base + offset)
#define CONN_HIF_REG(offset)	(conn_base + offset)
#define CONN_MCU_REG(offset)	(conn_mcu_base + offset)

/* Define MTCMOS power control */
#define PWR_RST_B			(0x1 << 0)
#define PWR_ISO				(0x1 << 1)
#define PWR_ON				(0x1 << 2)
#define PWR_ON_2ND			(0x1 << 3)
#define PWR_CLK_DIS			(0x1 << 4)
#define SRAM_CKISO			(0x1 << 5)
#define SRAM_ISOINT_B			(0x1 << 6)

/**************************************
 * for non-CPU MTCMOS
 **************************************/
#define POWERON_CONFIG_EN		SPM_REG(0x0000)
#define SPM_POWER_ON_VAL0		SPM_REG(0x0004)
#define SPM_POWER_ON_VAL1		SPM_REG(0x0008)
#define PWR_STATUS			SPM_REG(0x0180)
#define PWR_STATUS_2ND			SPM_REG(0x0184)
#define VCODEC_PWR_CON			SPM_REG(0x0300)
#define ISP_PWR_CON			SPM_REG(0x0308)
#define DIS_PWR_CON			SPM_REG(0x030C)
#define MFG_CORE1_PWR_CON		SPM_REG(0x0310)
#define AUDIO_PWR_CON			SPM_REG(0x0314)
#define IFR_PWR_CON			SPM_REG(0x0318)
#define DPY_PWR_CON			SPM_REG(0x031C)
#define MD1_PWR_CON			SPM_REG(0x0320)
#define VPU_TOP_PWR_CON			SPM_REG(0x0324)
#define CONN_PWR_CON			SPM_REG(0x032C)
#define VPU_CORE2_PWR_CON		SPM_REG(0x0330)
#define MFG_ASYNC_PWR_CON		SPM_REG(0x0334)
#define MFG_PWR_CON			SPM_REG(0x0338)
#define VPU_CORE0_PWR_CON		SPM_REG(0x033C)
#define VPU_CORE1_PWR_CON		SPM_REG(0x0340)
#define CAM_PWR_CON			SPM_REG(0x0344)
#define MFG_2D_PWR_CON			SPM_REG(0x0348)
#define MFG_CORE0_PWR_CON		SPM_REG(0x034C)
#define VDE_PWR_CON			SPM_REG(0x0370)
#define MD_SRAM_ISO_CON		SPM_REG(0x0394)
#define MD_EXTRA_PWR_CON		SPM_REG(0x0398)

#define  SPM_PROJECT_CODE		0xB16

#define INFRA_TOPAXI_SI0_CTL		INFRACFG_REG(0x0200)
#define INFRA_TOPAXI_PROTECTEN		INFRACFG_REG(0x0220)
#define INFRA_TOPAXI_PROTECTEN_STA0	INFRACFG_REG(0x0224)

#define INFRA_TOPAXI_PROTECTEN_1	INFRACFG_REG(0x0250)

#define INFRA_TOPAXI_PROTECTEN_SET	INFRACFG_REG(0x02A0)
#define INFRA_TOPAXI_PROTECTEN_STA1	INFRACFG_REG(0x0228)
#define INFRA_TOPAXI_PROTECTEN_CLR	INFRACFG_REG(0x02A4)

#define INFRA_TOPAXI_PROTECTEN_1_SET	INFRACFG_REG(0x02A8)
#define INFRA_TOPAXI_PROTECTEN_STA0_1	INFRACFG_REG(0x0254)
#define INFRA_TOPAXI_PROTECTEN_STA1_1	INFRACFG_REG(0x0258)
#define INFRA_TOPAXI_PROTECTEN_1_CLR	INFRACFG_REG(0x02AC)

/* fix with infra config address */
#define INFRA_TOPAXI_SI0_STA		INFRA_REG(0x0000)
#define INFRA_BUS_IDLE_STA4		INFRA_REG(0x018C)
#define INFRA_TOPAXI_SI3_STA		INFRA_REG(0x002C)

/* SMI COMMON */
#define SMI_COMMON_SMI_CLAMP		SMI_COMMON_REG(0x03C0)
#define SMI_COMMON_SMI_CLAMP_SET	SMI_COMMON_REG(0x03C4)
#define SMI_COMMON_SMI_CLAMP_CLR	SMI_COMMON_REG(0x03C8)

/* CONN HIF */
#define CONN_HIF_TOP_MISC		CONN_HIF_REG(0x0104)
#define CONN_HIF_DBG_IDX		CONN_HIF_REG(0x012C)
#define CONN_HIF_DBG_PROBE		CONN_HIF_REG(0x0130)
#define CONN_HIF_BUSY_STATUS		CONN_HIF_REG(0x0138)
#define CONN_HIF_PDMA_BUSY_STATUS	CONN_HIF_REG(0x0168)

/* CONN MCU */
#define CONN_MCU_CLOCK_CONTROL		CONN_MCU_REG(0x0100)
#define CONN_MCU_BUS_CONTROL		CONN_MCU_REG(0x0110)
#define CONN_MCU_EMI_CONTROL		CONN_MCU_REG(0x0150)
#define CONN_MCU_DEBUG_SELECT		CONN_MCU_REG(0x0400)
#define CONN_MCU_DEBUG_STATUS		CONN_MCU_REG(0x040C)
#define CONN_MCU_BUSHANGCR		CONN_MCU_REG(0x0440)
#define CONN_MCU_BUSHANGADDR		CONN_MCU_REG(0x0444)
/* Define MTCMOS Bus Protect Mask */
#define MD1_PROT_STEP1_0_MASK		((0x1 << 7))
#define MD1_PROT_STEP1_0_ACK_MASK	((0x1 << 7))
#define MD1_PROT_STEP2_0_MASK		((0x1 << 3) \
					|(0x1 << 4))
#define MD1_PROT_STEP2_0_ACK_MASK	((0x1 << 3) \
					|(0x1 << 4))
#define MD1_PROT_STEP2_1_MASK		((0x1 << 6))
#define MD1_PROT_STEP2_1_ACK_MASK	((0x1 << 6))
#define CONN_PROT_STEP1_0_MASK		((0x1 << 13))
#define CONN_PROT_STEP1_0_ACK_MASK	((0x1 << 13))
#define CONN_PROT_STEP1_1_MASK		((0x1 << 18))
#define CONN_PROT_STEP1_1_ACK_MASK	((0x1 << 18))
#define CONN_PROT_STEP2_0_MASK		((0x1 << 14) \
					|(0x1 << 16))
#define CONN_PROT_STEP2_0_ACK_MASK	((0x1 << 14) \
					|(0x1 << 16))
#define DPY_PROT_STEP1_0_MASK		((0x1 << 0) \
					|(0x1 << 5) \
					|(0x1 << 23) \
					|(0x1 << 26))
#define DPY_PROT_STEP1_0_ACK_MASK	((0x1 << 0) \
					|(0x1 << 5) \
					|(0x1 << 23) \
					|(0x1 << 26))
#define DPY_PROT_STEP1_1_MASK		((0x1 << 10) \
					|(0x1 << 11) \
					|(0x1 << 12) \
					|(0x1 << 13) \
					|(0x1 << 14) \
					|(0x1 << 15) \
					|(0x1 << 16) \
					|(0x1 << 17))
#define DPY_PROT_STEP1_1_ACK_MASK	((0x1 << 10) \
					|(0x1 << 11) \
					|(0x1 << 12) \
					|(0x1 << 13) \
					|(0x1 << 14) \
					|(0x1 << 15) \
					|(0x1 << 16) \
					|(0x1 << 17))
#define DPY_PROT_STEP2_0_MASK		((0x1 << 1) \
					|(0x1 << 2) \
					|(0x1 << 3) \
					|(0x1 << 4) \
					|(0x1 << 10) \
					|(0x1 << 11) \
					|(0x1 << 21) \
					|(0x1 << 22))
#define DPY_PROT_STEP2_0_ACK_MASK	((0x1 << 1) \
					|(0x1 << 2) \
					|(0x1 << 3) \
					|(0x1 << 4) \
					|(0x1 << 10) \
					|(0x1 << 11) \
					|(0x1 << 21) \
					|(0x1 << 22))
#define DIS_PROT_STEP1_0_MASK		((0x1 << 19) \
					|(0x1 << 20))
#define DIS_PROT_STEP1_0_ACK_MASK	((0x1 << 19) \
					|(0x1 << 20))
#define DIS_PROT_STEP2_0_MASK		((0x1 << 16) \
					|(0x1 << 17))
#define DIS_PROT_STEP2_0_ACK_MASK	((0x1 << 16) \
					|(0x1 << 17))
#define DIS_PROT_STEP3_0_MASK		((0x1 << 10) \
					|(0x1 << 11))
#define DIS_PROT_STEP3_0_ACK_MASK	((0x1 << 10) \
					|(0x1 << 11))
#define DIS_PROT_STEP4_0_MASK		((0x1 << 1) \
					|(0x1 << 2))
#define DIS_PROT_STEP4_0_ACK_MASK	((0x1 << 1) \
					|(0x1 << 2))
#define MFG_PROT_STEP1_0_MASK		((0x1 << 25))
#define MFG_PROT_STEP1_0_ACK_MASK	((0x1 << 25))
#define MFG_PROT_STEP2_0_MASK		((0x1 << 21) \
					|(0x1 << 22))
#define MFG_PROT_STEP2_0_ACK_MASK	((0x1 << 21) \
					|(0x1 << 22))
#define ISP_PROT_STEP1_0_MASK		((0x1 << 20))
#define ISP_PROT_STEP1_0_ACK_MASK	((0x1 << 20))
#define ISP_PROT_STEP2_0_MASK		((0x1 << 2))
#define ISP_PROT_STEP2_0_ACK_MASK	((0x1 << 2))
#define CAM_PROT_STEP1_0_MASK		((0x1 << 19) \
					|(0x1 << 21))
#define CAM_PROT_STEP1_0_ACK_MASK	((0x1 << 19) \
					|(0x1 << 21))
#define CAM_PROT_STEP2_0_MASK		((0x1 << 20))
#define CAM_PROT_STEP2_0_ACK_MASK	((0x1 << 20))
#define CAM_PROT_STEP2_1_MASK		((0x1 << 3))
#define CAM_PROT_STEP2_1_ACK_MASK	((0x1 << 3))

/* Define MTCMOS Power Status Mask */

#define MD1_PWR_STA_MASK		(0x1 << 0)
#define CONN_PWR_STA_MASK		(0x1 << 1)
#define DPY_PWR_STA_MASK		(0x1 << 2)
#define DIS_PWR_STA_MASK		(0x1 << 3)
#define MFG_PWR_STA_MASK		(0x1 << 4)
#define ISP_PWR_STA_MASK		(0x1 << 5)
#define IFR_PWR_STA_MASK		(0x1 << 6)
#define MFG_CORE0_PWR_STA_MASK		(0x1 << 7)
#define MFG_ASYNC_PWR_STA_MASK		(0x1 << 23)
#define CAM_PWR_STA_MASK		(0x1 << 25)
#define VCODEC_PWR_STA_MASK		(0x1 << 26)

/* Define CPU SRAM Mask */

/* Define Non-CPU SRAM Mask */
#define MD1_SRAM_PDN			(0x1 << 8)
#define MD1_SRAM_PDN_ACK		(0x0 << 12)
#define MD1_SRAM_PDN_ACK_BIT0		(0x1 << 12)
#define DPY_SRAM_PDN			(0xF << 8)
#define DPY_SRAM_PDN_ACK		(0xF << 12)
#define DPY_SRAM_PDN_ACK_BIT0		(0x1 << 12)
#define DPY_SRAM_PDN_ACK_BIT1		(0x1 << 13)
#define DPY_SRAM_PDN_ACK_BIT2		(0x1 << 14)
#define DPY_SRAM_PDN_ACK_BIT3		(0x1 << 15)
#define DIS_SRAM_PDN			(0x1 << 8)
#define DIS_SRAM_PDN_ACK		(0x1 << 12)
#define DIS_SRAM_PDN_ACK_BIT0		(0x1 << 12)
#define MFG_SRAM_PDN			(0x1 << 8)
#define MFG_SRAM_PDN_ACK		(0x1 << 12)
#define MFG_SRAM_PDN_ACK_BIT0		(0x1 << 12)
#define ISP_SRAM_PDN			(0x1 << 8)
#define ISP_SRAM_PDN_ACK		(0x1 << 12)
#define ISP_SRAM_PDN_ACK_BIT0		(0x1 << 12)
#define IFR_SRAM_PDN			(0x1 << 8)
#define IFR_SRAM_PDN_ACK		(0x1 << 12)
#define IFR_SRAM_PDN_ACK_BIT0		(0x1 << 12)
#define MFG_CORE0_SRAM_PDN		(0x1 << 8)
#define MFG_CORE0_SRAM_PDN_ACK		(0x1 << 12)
#define MFG_CORE0_SRAM_PDN_ACK_BIT0	(0x1 << 12)
#define CAM_SRAM_PDN			(0x3 << 8)
#define CAM_SRAM_PDN_ACK		(0x3 << 12)
#define CAM_SRAM_PDN_ACK_BIT0		(0x1 << 12)
#define CAM_SRAM_PDN_ACK_BIT1		(0x1 << 13)
#define VCODEC_SRAM_PDN			(0x1 << 8)
#define VCODEC_SRAM_PDN_ACK		(0x1 << 12)
#define VCODEC_SRAM_PDN_ACK_BIT0	(0x1 << 12)

static struct subsys syss[] =	/* NR_SYSS *//* FIXME: set correct value */
{
	[SYS_MD1] = {
			.name = __stringify(SYS_MD1),
			.sta_mask = MD1_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &MD1_sys_ops,
			},
	[SYS_CONN] = {
			.name = __stringify(SYS_CONN),
			.sta_mask = CONN_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &CONN_sys_ops,
			},
	[SYS_DPY] = {
			.name = __stringify(SYS_DPY),
			.sta_mask = DPY_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &DPY_sys_ops,
			},
	[SYS_DIS] = {
			.name = __stringify(SYS_DIS),
			.sta_mask = DIS_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &DIS_sys_ops,
			},
	[SYS_MFG] = {
			.name = __stringify(SYS_MFG),
			.sta_mask = MFG_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &MFG_sys_ops,
			},
	[SYS_ISP] = {
			.name = __stringify(SYS_ISP),
			.sta_mask = ISP_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &ISP_sys_ops,
			},
	[SYS_IFR] = {
			.name = __stringify(SYS_IFR),
			.sta_mask = IFR_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &IFR_sys_ops,
			},
	[SYS_MFG_CORE0] = {
			.name = __stringify(SYS_MFG_CORE0),
			.sta_mask = MFG_CORE0_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &MFG_CORE0_sys_ops,
			},
	[SYS_MFG_ASYNC] = {
			.name = __stringify(SYS_MFG_ASYNC),
			.sta_mask = MFG_ASYNC_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &MFG_ASYNC_sys_ops,
			},
	[SYS_CAM] = {
			.name = __stringify(SYS_CAM),
			.sta_mask = CAM_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &CAM_sys_ops,
			},
	[SYS_VCODEC] = {
			.name = __stringify(SYS_VCODEC),
			.sta_mask = VCODEC_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &VCODEC_sys_ops,
			},
};

LIST_HEAD(pgcb_list);

struct pg_callbacks *register_pg_callback(struct pg_callbacks *pgcb)
{
	INIT_LIST_HEAD(&pgcb->list);

	list_add(&pgcb->list, &pgcb_list);

	return pgcb;
}

static struct subsys *id_to_sys(unsigned int id)
{
	return id < NR_SYSS ? &syss[id] : NULL;
}

enum dbg_id {
	DBG_ID_MD1_BUS = 0,
	DBG_ID_CONN_BUS,
	DBG_ID_DPY_BUS,
	DBG_ID_DIS_BUS,
	DBG_ID_MFG_BUS,
	DBG_ID_ISP_BUS,
	DBG_ID_IFR_BUS,
	DBG_ID_MFG_CORE0_BUS,
	DBG_ID_MFG_ASYNC_BUS,
	DBG_ID_CAM_BUS,
	DBG_ID_VCODEC_BUS = 10,

	DBG_ID_MD1_PWR = 11,
	DBG_ID_CONN_PWR,
	DBG_ID_DPY_PWR,
	DBG_ID_DIS_PWR,
	DBG_ID_MFG_PWR,
	DBG_ID_ISP_PWR,
	DBG_ID_IFR_PWR,
	DBG_ID_MFG_CORE0_PWR,
	DBG_ID_MFG_ASYNC_PWR,
	DBG_ID_CAM_PWR,
	DBG_ID_VCODEC_PWR,
	DBG_ID_NUM = 22,
};

#define ID_MADK   0xFF000000
#define STA_MASK  0x00F00000
#define STEP_MASK 0x000000FF

#define INCREASE_STEPS \
	do { DBG_STEP++; } while (0)

static int DBG_ID;
static int DBG_STA;
static int DBG_STEP;
/*
 * ram console data0 define
 * [31:24] : DBG_ID
 * [23:20] : DBG_STA
 * [7:0] : DBG_STEP
 */
static void ram_console_update(void)
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	u32 data[8] = {0x0};
	u32 i = 0, j = 0;
	static u32 pre_data;
	static int k;
	static bool print_once = true;

	data[i] = ((DBG_ID << 24) & ID_MADK)
		| ((DBG_STA << 20) & STA_MASK)
		| (DBG_STEP & STEP_MASK);

	data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN);
	data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN_1);
	data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN_STA0);
	data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN_STA1);
	data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN_STA0_1);
	data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN_STA1_1);
	data[++i] = clk_readl(INFRA_TOPAXI_SI3_STA);

	if (pre_data == data[0])
		k++;
	else if (pre_data != data[0]) {
		k = 0;
		pre_data = data[0];
		print_once = true;
	}

	if (k > 5000 && print_once) {
		print_once = false;
		k = 0;
		if (DBG_ID == DBG_ID_CONN_BUS) {
			if (DBG_STEP == 1 && DBG_STA == STA_POWER_DOWN) {
				/* TINFO="Release bus protect - step1 : 0" */
				spm_write(INFRA_TOPAXI_PROTECTEN_CLR,
						CONN_PROT_STEP1_0_MASK);
			}
			/* Space for clk in AEE is not enough,
			 * use printk instead.
			 */
			pr_notice("CONN_HIF_TOP_MISC=0x%08x\n",
				clk_readl(CONN_HIF_TOP_MISC));
			pr_notice("CONN_HIF_BUSY_STATUS=0x%08x\n",
				clk_readl(CONN_HIF_BUSY_STATUS));
			pr_notice("CONN_HIF_PDMA_BUSY_STATUS=0x%08x\n",
				clk_readl(CONN_HIF_PDMA_BUSY_STATUS));

			clk_writel(CONN_HIF_DBG_IDX, 0x2222);
			pr_notice("CONN_HIF_DBG_PROBE=0x%08x\n",
				clk_readl(CONN_HIF_DBG_PROBE));
			clk_writel(CONN_HIF_DBG_IDX, 0x3333);
			pr_notice("CONN_HIF_DBG_PROBE=0x%08x\n",
				clk_readl(CONN_HIF_DBG_PROBE));
			clk_writel(CONN_HIF_DBG_IDX, 0x4444);
			pr_notice("CONN_HIF_DBG_PROBE=0x%08x\n",
				clk_readl(CONN_HIF_DBG_PROBE));

			pr_notice("CONN_MCU_EMI_CONTROL=0x%08x\n",
				clk_readl(CONN_MCU_EMI_CONTROL));
			pr_notice("CONN_MCU_CLOCK_CONTROL=0x%08x\n",
				clk_readl(CONN_MCU_CLOCK_CONTROL));
			pr_notice("CONN_MCU_BUS_CONTROL=0x%08x\n",
				clk_readl(CONN_MCU_BUS_CONTROL));
			pr_notice("CONN_MCU_BUSHANGCR=0x%08x\n",
				clk_readl(CONN_MCU_BUSHANGCR));
			pr_notice("CONN_MCU_BUSHANGADDR=0x%08x\n",
				clk_readl(CONN_MCU_BUSHANGADDR));

			clk_writel(CONN_MCU_DEBUG_SELECT, 0x003e3d00);
			pr_notice("CONN_MCU_DEBUG_STATUS=0x%08x\n",
				clk_readl(CONN_MCU_DEBUG_STATUS));
			clk_writel(CONN_MCU_DEBUG_SELECT, 0x00403f00);
			pr_notice("CONN_MCU_DEBUG_STATUS=0x%08x\n",
				clk_readl(CONN_MCU_DEBUG_STATUS));
			clk_writel(CONN_MCU_DEBUG_SELECT, 0x00424100);
			pr_notice("CONN_MCU_DEBUG_STATUS=0x%08x\n",
				clk_readl(CONN_MCU_DEBUG_STATUS));
			clk_writel(CONN_MCU_DEBUG_SELECT, 0x00444300);
			pr_notice("CONN_MCU_DEBUG_STATUS=0x%08x\n",
				clk_readl(CONN_MCU_DEBUG_STATUS));

			if (DBG_STEP == 1 && DBG_STA == STA_POWER_DOWN) {
				/* TINFO="Release bus protect - step1 : 0" */
				spm_write(INFRA_TOPAXI_PROTECTEN_SET,
						CONN_PROT_STEP1_0_MASK);
				j = 0;
				while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
					& CONN_PROT_STEP1_0_ACK_MASK)
					!= CONN_PROT_STEP1_0_ACK_MASK) {
					if (j > 1000)
						break;
					j++;
				}

				if (j > 1000)
					DBG_STEP = CONN_TIMEOUT_STEP1;
				else
					DBG_STEP = CONN_TIMEOUT_RECOVERY;

				data[0] = ((DBG_ID << 24) & ID_MADK)
					| ((DBG_STA << 20) & STA_MASK)
					| (DBG_STEP & STEP_MASK);
			}
		} else
			print_enabled_clks_once();

		if (DBG_ID >= (DBG_ID_NUM / 2))
			pr_notice("%s %s MTCMOS PWR hang at %s flow step %d\n",
				"[clkmgr]",
				syss[(DBG_ID - (DBG_ID_NUM / 2))].name,
				DBG_STA ? "pwron":"pdn",
				DBG_STEP);
		else
			pr_notice("%s %s MTCMOS BUS hang at %s flow step %d\n",
				"[clkmgr]",
				syss[DBG_ID].name,
				DBG_STA ? "pwron":"pdn",
				DBG_STEP);

		for (j = 1; j <= i; j++)
			pr_notice("%s: clk[%d] = 0x%x\n", __func__, j, data[j]);
	}

	for (j = 0; j <= i; j++)
		aee_rr_rec_clk(j, data[j]);
	/*todo: add each domain's debug register to ram console*/
#endif
}

/* auto-gen begin*/
int spm_mtcmos_ctrl_md1_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MD1_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MD1" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, MD1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& MD1_PROT_STEP1_0_ACK_MASK)
			!= MD1_PROT_STEP1_0_ACK_MASK)
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, MD1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& MD1_PROT_STEP2_0_ACK_MASK)
			!= MD1_PROT_STEP2_0_ACK_MASK)
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET, MD1_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1)
			& MD1_PROT_STEP2_1_ACK_MASK)
			!= MD1_PROT_STEP2_1_ACK_MASK)
			ram_console_update();
#endif
	} else {    /* STA_POWER_ON */
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, MD1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR, MD1_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 *releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, MD1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Finish to turn on MD1" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_md1_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MD1_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */

	if (state == STA_POWER_DOWN) {
		/* TINFO="MD_EXTRA_PWR_CON[0]=1"*/
		spm_write(MD_EXTRA_PWR_CON,
			spm_read(MD_EXTRA_PWR_CON) | (0x1 << 0));
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MD1_PWR_CON,
			spm_read(MD1_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MD1_PWR_CON,
			spm_read(MD1_PWR_CON) | PWR_ISO);
		/* TINFO="MD_SRAM_ISO_CON[0]=0"*/
		spm_write(MD_SRAM_ISO_CON,
			spm_read(MD_SRAM_ISO_CON) & ~(0x1 << 0));
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) | MD1_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MD1_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MD1_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Finish to turn off MD1" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MD1" */
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & MD1_PWR_STA_MASK)
			!= MD1_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & MD1_PWR_STA_MASK)
			!= MD1_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) & ~(0x1 << 8));
		/* TINFO="MD_SRAM_ISO_CON[0]=1"*/
		spm_write(MD_SRAM_ISO_CON,
			spm_read(MD_SRAM_ISO_CON) | (0x1 << 0));
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) | PWR_RST_B);
		/* TINFO="MD_EXTRA_PWR_CON[0]=0"*/
		spm_write(MD_EXTRA_PWR_CON,
			spm_read(MD_EXTRA_PWR_CON) & ~(0x1 << 0));
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_conn_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_CONN_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off CONN" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, CONN_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& CONN_PROT_STEP1_0_ACK_MASK)
			!= CONN_PROT_STEP1_0_ACK_MASK)
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET,
			CONN_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1)
			& CONN_PROT_STEP1_1_ACK_MASK)
			!= CONN_PROT_STEP1_1_ACK_MASK) {
			ram_console_update();
			if (DBG_STEP == CONN_TIMEOUT_RECOVERY)
				break;
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, CONN_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& CONN_PROT_STEP2_0_ACK_MASK)
			!= CONN_PROT_STEP2_0_ACK_MASK)
			ram_console_update();
#endif
	} else {    /* STA_POWER_ON */
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR,
			CONN_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR,
			CONN_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR,
			CONN_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Finish to turn on CONN" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_conn_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_CONN_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(CONN_PWR_CON,
			spm_read(CONN_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(CONN_PWR_CON,
			spm_read(CONN_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(CONN_PWR_CON,
			spm_read(CONN_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(CONN_PWR_CON,
			spm_read(CONN_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & CONN_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & CONN_PWR_STA_MASK)) {
		/* No logic between pwr_on and pwr_ack.
		 * Print SRAM / MTCMOS control and
		 * PWR_ACK for debug.
		 */
			ram_console_update();
		}
#endif
		/* TINFO="Finish to turn off CONN" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on CONN" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(CONN_PWR_CON,
			spm_read(CONN_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & CONN_PWR_STA_MASK)
			!= CONN_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & CONN_PWR_STA_MASK)
			!= CONN_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(CONN_PWR_CON,
			spm_read(CONN_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) | PWR_RST_B);
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_dpy_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_DPY_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off DPY" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, DPY_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& DPY_PROT_STEP1_0_ACK_MASK)
			!= DPY_PROT_STEP1_0_ACK_MASK)
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET, DPY_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1)
			& DPY_PROT_STEP1_1_ACK_MASK)
			!= DPY_PROT_STEP1_1_ACK_MASK)
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, DPY_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& DPY_PROT_STEP2_0_ACK_MASK)
			!= DPY_PROT_STEP2_0_ACK_MASK)
			ram_console_update();
		INCREASE_STEPS;
#endif
	} else {    /* STA_POWER_ON */
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, DPY_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, DPY_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR, DPY_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Finish to turn on DPY" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_dpy_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_DPY_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(DPY_PWR_CON, spm_read(DPY_PWR_CON) | DPY_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DPY_SRAM_PDN_ACK = 1" */
		while ((spm_read(DPY_PWR_CON) & DPY_SRAM_PDN_ACK)
			!= DPY_SRAM_PDN_ACK)
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(DPY_PWR_CON, spm_read(DPY_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(DPY_PWR_CON, spm_read(DPY_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(DPY_PWR_CON, spm_read(DPY_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(DPY_PWR_CON, spm_read(DPY_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(DPY_PWR_CON, spm_read(DPY_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & DPY_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & DPY_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off DPY" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on DPY" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(DPY_PWR_CON, spm_read(DPY_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(DPY_PWR_CON, spm_read(DPY_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & DPY_PWR_STA_MASK)
			!= DPY_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & DPY_PWR_STA_MASK)
			!= DPY_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(DPY_PWR_CON, spm_read(DPY_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(DPY_PWR_CON, spm_read(DPY_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(DPY_PWR_CON, spm_read(DPY_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(DPY_PWR_CON, spm_read(DPY_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DPY_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(DPY_PWR_CON) & DPY_SRAM_PDN_ACK_BIT0)
			ram_console_update();
		INCREASE_STEPS;
#endif
		spm_write(DPY_PWR_CON, spm_read(DPY_PWR_CON) & ~(0x1 << 9));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DPY_SRAM_PDN_ACK_BIT1 = 0" */
		while (spm_read(DPY_PWR_CON) & DPY_SRAM_PDN_ACK_BIT1)
			ram_console_update();
		INCREASE_STEPS;
#endif
		spm_write(DPY_PWR_CON, spm_read(DPY_PWR_CON) & ~(0x1 << 10));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DPY_SRAM_PDN_ACK_BIT2 = 0" */
		while (spm_read(DPY_PWR_CON) & DPY_SRAM_PDN_ACK_BIT2)
			ram_console_update();
		INCREASE_STEPS;
#endif
		spm_write(DPY_PWR_CON, spm_read(DPY_PWR_CON) & ~(0x1 << 11));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DPY_SRAM_PDN_ACK_BIT3 = 0" */
		while (spm_read(DPY_PWR_CON) & DPY_SRAM_PDN_ACK_BIT3)
			ram_console_update();
		INCREASE_STEPS;
#endif
	}
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_dis_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_DIS_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off DIS" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET, DIS_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1)
			& DIS_PROT_STEP1_0_ACK_MASK)
			!= DIS_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET, DIS_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1)
			& DIS_PROT_STEP2_0_ACK_MASK)
			!= DIS_PROT_STEP2_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step3 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, DIS_PROT_STEP3_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& DIS_PROT_STEP3_0_ACK_MASK)
			!= DIS_PROT_STEP3_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step4 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, DIS_PROT_STEP4_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& DIS_PROT_STEP4_0_ACK_MASK)
			!= DIS_PROT_STEP4_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) | DIS_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DIS_SRAM_PDN_ACK = 1" */
		while ((spm_read(DIS_PWR_CON) & DIS_SRAM_PDN_ACK)
			!= DIS_SRAM_PDN_ACK)
			ram_console_update();
		/* Need hf_fmm_ck for SRAM PDN delay IP. */
		INCREASE_STEPS;
#endif
	} else {    /* STA_POWER_ON */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DIS_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(DIS_PWR_CON) & DIS_SRAM_PDN_ACK_BIT0)
			ram_console_update();
		/* Need hf_fmm_ck for SRAM PDN delay IP. */
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step4 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, DIS_PROT_STEP4_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step3 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, DIS_PROT_STEP3_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR, DIS_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR, DIS_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Finish to turn on DIS" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_dis_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_DIS_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & DIS_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & DIS_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off DIS" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on DIS" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & DIS_PWR_STA_MASK)
			!= DIS_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & DIS_PWR_STA_MASK)
			!= DIS_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) | PWR_RST_B);
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_mfg_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, MFG_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& MFG_PROT_STEP1_0_ACK_MASK)
			!= MFG_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, MFG_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& MFG_PROT_STEP2_0_ACK_MASK)
			!= MFG_PROT_STEP2_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) | MFG_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG_SRAM_PDN_ACK = 1" */
		while ((spm_read(MFG_PWR_CON) & MFG_SRAM_PDN_ACK)
			!= MFG_SRAM_PDN_ACK)
			ram_console_update();
		/* Need f_fmfg_core_ck for SRAM PDN delay IP. */
#endif
	} else {    /* STA_POWER_ON */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(MFG_PWR_CON) & MFG_SRAM_PDN_ACK_BIT0)
			ram_console_update();
		/* Need f_fmfg_core_ck for SRAM PDN delay IP. */
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, MFG_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, MFG_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Finish to turn on MFG" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_mfg_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MFG_PWR_STA_MASK)
			|| (spm_read(PWR_STATUS_2ND) & MFG_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
#endif
		/* TINFO="Finish to turn off MFG" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MFG" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & MFG_PWR_STA_MASK)
			!= MFG_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & MFG_PWR_STA_MASK)
			!= MFG_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) | PWR_RST_B);
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_isp_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_ISP_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off ISP" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET, ISP_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1)
			& ISP_PROT_STEP1_0_ACK_MASK)
			!= ISP_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(SMI_COMMON_SMI_CLAMP_SET, ISP_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(SMI_COMMON_SMI_CLAMP)
			& ISP_PROT_STEP2_0_ACK_MASK)
			!= ISP_PROT_STEP2_0_ACK_MASK) {
			ram_console_update();
		}
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) | ISP_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ISP_SRAM_PDN_ACK = 1" */
		while ((spm_read(ISP_PWR_CON) & ISP_SRAM_PDN_ACK)
			!= ISP_SRAM_PDN_ACK)
			ram_console_update();
		/* Need hf_fmm_ck for SRAM PDN delay IP. */
		INCREASE_STEPS;
#endif
	} else {    /* STA_POWER_ON */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ISP_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(ISP_PWR_CON) & ISP_SRAM_PDN_ACK_BIT0)
			ram_console_update();
		/* Need hf_fmm_ck for SRAM PDN delay IP. */
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(SMI_COMMON_SMI_CLAMP_CLR, ISP_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR, ISP_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Finish to turn on ISP" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_isp_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_ISP_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & ISP_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & ISP_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
#endif
		/* TINFO="Finish to turn off ISP" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on ISP" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & ISP_PWR_STA_MASK)
			!= ISP_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & ISP_PWR_STA_MASK)
			!= ISP_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) | PWR_RST_B);
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_ifr_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_IFR_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off IFR" */
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(IFR_PWR_CON, spm_read(IFR_PWR_CON) | IFR_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until IFR_SRAM_PDN_ACK = 1" */
		while ((spm_read(IFR_PWR_CON) & IFR_SRAM_PDN_ACK)
			!= IFR_SRAM_PDN_ACK) {
			/* SRAM PDN delay IP clock is 26MHz.
			 * Print SRAM control and ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(IFR_PWR_CON, spm_read(IFR_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(IFR_PWR_CON, spm_read(IFR_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(IFR_PWR_CON, spm_read(IFR_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(IFR_PWR_CON, spm_read(IFR_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(IFR_PWR_CON, spm_read(IFR_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & IFR_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & IFR_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off IFR" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on IFR" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(IFR_PWR_CON, spm_read(IFR_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(IFR_PWR_CON, spm_read(IFR_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & IFR_PWR_STA_MASK)
			!= IFR_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & IFR_PWR_STA_MASK)
			!= IFR_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(IFR_PWR_CON, spm_read(IFR_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(IFR_PWR_CON, spm_read(IFR_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(IFR_PWR_CON, spm_read(IFR_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(IFR_PWR_CON, spm_read(IFR_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until IFR_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(IFR_PWR_CON) & IFR_SRAM_PDN_ACK_BIT0) {
			/* SRAM PDN delay IP clock is 26MHz.
			 * Print SRAM control and ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn on IFR" */
	}
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_mfg_core0_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG_CORE0_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG_CORE0" */
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MFG_CORE0_PWR_CON,
			spm_read(MFG_CORE0_PWR_CON) | MFG_CORE0_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG_CORE0_SRAM_PDN_ACK = 1" */
		while ((spm_read(MFG_CORE0_PWR_CON)
			& MFG_CORE0_SRAM_PDN_ACK)
			!= MFG_CORE0_SRAM_PDN_ACK) {
			ram_console_update();/* n/a */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MFG_CORE0_PWR_CON,
			spm_read(MFG_CORE0_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MFG_CORE0_PWR_CON,
			spm_read(MFG_CORE0_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MFG_CORE0_PWR_CON,
			spm_read(MFG_CORE0_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MFG_CORE0_PWR_CON,
			spm_read(MFG_CORE0_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MFG_CORE0_PWR_CON,
			spm_read(MFG_CORE0_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MFG_CORE0_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MFG_CORE0_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off MFG_CORE0" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MFG_CORE0" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MFG_CORE0_PWR_CON,
			spm_read(MFG_CORE0_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MFG_CORE0_PWR_CON,
			spm_read(MFG_CORE0_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & MFG_CORE0_PWR_STA_MASK)
			!= MFG_CORE0_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & MFG_CORE0_PWR_STA_MASK)
			!= MFG_CORE0_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG_CORE0_PWR_CON,
			spm_read(MFG_CORE0_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG_CORE0_PWR_CON,
			spm_read(MFG_CORE0_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG_CORE0_PWR_CON,
			spm_read(MFG_CORE0_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MFG_CORE0_PWR_CON,
			spm_read(MFG_CORE0_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG_CORE0_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(MFG_CORE0_PWR_CON)
			& MFG_CORE0_SRAM_PDN_ACK_BIT0)
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn on MFG_CORE0" */
	}
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_mfg_async_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG_ASYNC_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG_ASYNC" */
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MFG_ASYNC_PWR_CON,
			spm_read(MFG_ASYNC_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MFG_ASYNC_PWR_CON,
			spm_read(MFG_ASYNC_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MFG_ASYNC_PWR_CON,
			spm_read(MFG_ASYNC_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MFG_ASYNC_PWR_CON,
			spm_read(MFG_ASYNC_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MFG_ASYNC_PWR_CON,
			spm_read(MFG_ASYNC_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MFG_ASYNC_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MFG_ASYNC_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off MFG_ASYNC" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MFG_ASYNC" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MFG_ASYNC_PWR_CON,
			spm_read(MFG_ASYNC_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MFG_ASYNC_PWR_CON,
			spm_read(MFG_ASYNC_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & MFG_ASYNC_PWR_STA_MASK)
			!= MFG_ASYNC_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & MFG_ASYNC_PWR_STA_MASK)
			!= MFG_ASYNC_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG_ASYNC_PWR_CON,
			spm_read(MFG_ASYNC_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG_ASYNC_PWR_CON,
			spm_read(MFG_ASYNC_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG_ASYNC_PWR_CON,
			spm_read(MFG_ASYNC_PWR_CON) | PWR_RST_B);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Finish to turn on MFG_ASYNC" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_cam_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_CAM_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off CAM" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET, CAM_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1)
			& CAM_PROT_STEP1_0_ACK_MASK)
			!= CAM_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, CAM_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& CAM_PROT_STEP2_0_ACK_MASK)
			!= CAM_PROT_STEP2_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(SMI_COMMON_SMI_CLAMP_SET, CAM_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(SMI_COMMON_SMI_CLAMP)
			& CAM_PROT_STEP2_1_ACK_MASK)
			!= CAM_PROT_STEP2_1_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(CAM_PWR_CON, spm_read(CAM_PWR_CON) | CAM_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until CAM_SRAM_PDN_ACK = 1" */
		while ((spm_read(CAM_PWR_CON) & CAM_SRAM_PDN_ACK)
			!= CAM_SRAM_PDN_ACK)
			ram_console_update();
#endif
	} else {    /* STA_POWER_ON */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(CAM_PWR_CON, spm_read(CAM_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until CAM_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(CAM_PWR_CON) & CAM_SRAM_PDN_ACK_BIT0)
			ram_console_update();
		INCREASE_STEPS;
#endif
		spm_write(CAM_PWR_CON, spm_read(CAM_PWR_CON) & ~(0x1 << 9));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until CAM_SRAM_PDN_ACK_BIT1 = 0" */
		while (spm_read(CAM_PWR_CON) & CAM_SRAM_PDN_ACK_BIT1)
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, CAM_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(SMI_COMMON_SMI_CLAMP_CLR, CAM_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR, CAM_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Finish to turn on CAM" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_cam_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_CAM_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(CAM_PWR_CON, spm_read(CAM_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(CAM_PWR_CON, spm_read(CAM_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(CAM_PWR_CON, spm_read(CAM_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(CAM_PWR_CON, spm_read(CAM_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(CAM_PWR_CON, spm_read(CAM_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & CAM_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & CAM_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
#endif
		/* TINFO="Finish to turn off CAM" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on CAM" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(CAM_PWR_CON, spm_read(CAM_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(CAM_PWR_CON, spm_read(CAM_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & CAM_PWR_STA_MASK)
			!= CAM_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & CAM_PWR_STA_MASK)
			!= CAM_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(CAM_PWR_CON, spm_read(CAM_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(CAM_PWR_CON, spm_read(CAM_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(CAM_PWR_CON, spm_read(CAM_PWR_CON) | PWR_RST_B);
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_vcodec_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_VCODEC_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VCODEC" */
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(VCODEC_PWR_CON,
			spm_read(VCODEC_PWR_CON) | VCODEC_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VCODEC_SRAM_PDN_ACK = 1" */
		while ((spm_read(VCODEC_PWR_CON) & VCODEC_SRAM_PDN_ACK)
			!= VCODEC_SRAM_PDN_ACK)
			ram_console_update();
		/* Need hf_fmm_ck for SRAM PDN delay IP. */
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(VCODEC_PWR_CON, spm_read(VCODEC_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(VCODEC_PWR_CON,
			spm_read(VCODEC_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(VCODEC_PWR_CON,
			spm_read(VCODEC_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(VCODEC_PWR_CON, spm_read(VCODEC_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(VCODEC_PWR_CON,
			spm_read(VCODEC_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & VCODEC_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & VCODEC_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off VCODEC" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on VCODEC" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(VCODEC_PWR_CON, spm_read(VCODEC_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(VCODEC_PWR_CON,
			spm_read(VCODEC_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & VCODEC_PWR_STA_MASK)
			!= VCODEC_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & VCODEC_PWR_STA_MASK)
		       != VCODEC_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(VCODEC_PWR_CON,
			spm_read(VCODEC_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(VCODEC_PWR_CON,
			spm_read(VCODEC_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(VCODEC_PWR_CON,
			spm_read(VCODEC_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(VCODEC_PWR_CON,
			spm_read(VCODEC_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VCODEC_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(VCODEC_PWR_CON)
			& VCODEC_SRAM_PDN_ACK_BIT0)
			ram_console_update();
		/* Need hf_fmm_ck for SRAM PDN delay IP. */
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn on VCODEC" */
	}

	ram_console_update();
	return err;
}

/* auto-gen end*/
static int MD1_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_md1_bus_prot(STA_POWER_ON);
}

static int MD1_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_md1_pwr(STA_POWER_ON);
}

static int CONN_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_conn_bus_prot(STA_POWER_ON);
}

static int CONN_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_conn_pwr(STA_POWER_ON);
}

static int DPY_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_dpy_bus_prot(STA_POWER_ON);
}

static int DPY_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_dpy_pwr(STA_POWER_ON);
}

static int DIS_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_dis_bus_prot(STA_POWER_ON);
}

static int DIS_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_dis_pwr(STA_POWER_ON);
}

static int MFG_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg_bus_prot(STA_POWER_ON);
}

static int MFG_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg_pwr(STA_POWER_ON);
}

static int ISP_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_isp_bus_prot(STA_POWER_ON);
}

static int ISP_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_isp_pwr(STA_POWER_ON);
}

static int IFR_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_ifr_pwr(STA_POWER_ON);
}

static int MFG_CORE0_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg_core0_pwr(STA_POWER_ON);
}

static int MFG_ASYNC_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg_async_pwr(STA_POWER_ON);
}

static int CAM_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_cam_bus_prot(STA_POWER_ON);
}

static int CAM_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_cam_pwr(STA_POWER_ON);
}

static int VCODEC_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_vcodec_pwr(STA_POWER_ON);
}

static int MD1_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_md1_bus_prot(STA_POWER_DOWN);
}

static int MD1_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_md1_pwr(STA_POWER_DOWN);
}

static int CONN_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_conn_bus_prot(STA_POWER_DOWN);
}

static int CONN_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_conn_pwr(STA_POWER_DOWN);
}

static int DPY_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_dpy_bus_prot(STA_POWER_DOWN);
}

static int DPY_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_dpy_pwr(STA_POWER_DOWN);
}

static int DIS_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_dis_bus_prot(STA_POWER_DOWN);
}

static int DIS_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_dis_pwr(STA_POWER_DOWN);
}

static int MFG_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg_bus_prot(STA_POWER_DOWN);
}

static int MFG_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg_pwr(STA_POWER_DOWN);
}

static int ISP_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_isp_bus_prot(STA_POWER_DOWN);
}

static int ISP_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_isp_pwr(STA_POWER_DOWN);
}

static int IFR_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_ifr_pwr(STA_POWER_DOWN);
}

static int MFG_CORE0_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg_core0_pwr(STA_POWER_DOWN);
}

static int MFG_ASYNC_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg_async_pwr(STA_POWER_DOWN);
}

static int CAM_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_cam_bus_prot(STA_POWER_DOWN);
}

static int CAM_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_cam_pwr(STA_POWER_DOWN);
}

static int VCODEC_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_vcodec_pwr(STA_POWER_DOWN);
}

static int sys_get_state_op(struct subsys *sys)
{
	unsigned int sta = clk_readl(PWR_STATUS);
	unsigned int sta_s = clk_readl(PWR_STATUS_2ND);

	return (sta & sys->sta_mask) && (sta_s & sys->sta_mask);
}

#if 0
static int mfg_get_state_op(struct subsys *sys)
{
	if ((spm_read(MFG_PWR_CON) & PWR_ON) &&
		(spm_read(MFG_PWR_CON) & PWR_ON_2ND))
		return 1;
	else
		return 0;
}
#endif

static struct subsys_ops MD1_sys_ops = {
	.prepare =  MD1_sys_prepare_op,
	.unprepare =  MD1_sys_unprepare_op,
	.enable = MD1_sys_enable_op,
	.disable = MD1_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops CONN_sys_ops = {
	.prepare =  CONN_sys_prepare_op,
	.unprepare =  CONN_sys_unprepare_op,
	.enable = CONN_sys_enable_op,
	.disable = CONN_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops DPY_sys_ops = {
	.prepare =  DPY_sys_prepare_op,
	.unprepare =  DPY_sys_unprepare_op,
	.enable = DPY_sys_enable_op,
	.disable = DPY_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops DIS_sys_ops = {
	.prepare =  DIS_sys_prepare_op,
	.unprepare =  DIS_sys_unprepare_op,
	.enable = DIS_sys_enable_op,
	.disable = DIS_sys_disable_op,
	/*.get_state = sys_get_state_op,*/
	.get_state = sys_get_state_op,
};

static struct subsys_ops MFG_sys_ops = {
	.prepare =  MFG_sys_prepare_op,
	.unprepare =  MFG_sys_unprepare_op,
	.enable = MFG_sys_enable_op,
	.disable = MFG_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops ISP_sys_ops = {
	.prepare =  ISP_sys_prepare_op,
	.unprepare =  ISP_sys_unprepare_op,
	.enable = ISP_sys_enable_op,
	.disable = ISP_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops IFR_sys_ops = {
	.prepare =  NULL,
	.unprepare =  NULL,
	.enable = IFR_sys_enable_op,
	.disable = IFR_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops MFG_CORE0_sys_ops = {
	.prepare =  NULL,
	.unprepare =  NULL,
	.enable = MFG_CORE0_sys_enable_op,
	.disable = MFG_CORE0_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops MFG_ASYNC_sys_ops = {
	.prepare =  NULL,
	.unprepare =  NULL,
	.enable = MFG_ASYNC_sys_enable_op,
	.disable = MFG_ASYNC_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops CAM_sys_ops = {
	.prepare =  CAM_sys_prepare_op,
	.unprepare =  CAM_sys_unprepare_op,
	.enable = CAM_sys_enable_op,
	.disable = CAM_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops VCODEC_sys_ops = {
	.prepare =  NULL,
	.unprepare =  NULL,
	.enable = VCODEC_sys_enable_op,
	.disable = VCODEC_sys_disable_op,
	.get_state = sys_get_state_op,
};

static int subsys_is_on(enum subsys_id id)
{
	int r;
	struct subsys *sys = id_to_sys(id);

	WARN_ON(!sys);

	r = sys->ops->get_state(sys);

#if MT_CCF_DEBUG
	pr_debug("[CCF] %s:%d, sys=%s, id=%d\n", __func__, r, sys->name, id);
#endif				/* MT_CCF_DEBUG */

	return r;
}

#if CONTROL_LIMIT
int allow[NR_SYSS] = {
1, /*SYS_MD1*/
1, /*SYS_CONN*/
1, /*SYS_DPY*/
1, /*SYS_DIS*/
1, /*SYS_MFG*/
1, /*SYS_ISP*/
1, /*SYS_IFR*/
1, /*SYS_MFG_CORE0*/
1, /*SYS_MFG_ASYNC*/
1, /*SYS_CAM*/
1, /*SYS_VCODEC*/
};
#endif
static int enable_subsys(enum subsys_id id, enum mtcmos_op action)
{
	int r = 0;
	unsigned long flags;
	struct subsys *sys = id_to_sys(id);
	struct pg_callbacks *pgcb;

	WARN_ON(!sys);

	if (!mtk_is_mtcmos_enable()) {
#if MT_CCF_DEBUG
		pr_notice("[CCF] skip %s: sys=%s, id=%d\n",
			__func__, sys->name, id);
#endif
		switch (id) {
		case SYS_MD1:
			spm_mtcmos_ctrl_md1_pwr(STA_POWER_ON);
			spm_mtcmos_ctrl_md1_bus_prot(STA_POWER_ON);
			break;
		case SYS_CONN:
			spm_mtcmos_ctrl_conn_pwr(STA_POWER_ON);
			spm_mtcmos_ctrl_conn_bus_prot(STA_POWER_ON);
			break;
		default:
			break;
		}
		return 0;
	}

#if CONTROL_LIMIT
	#if MT_CCF_DEBUG
	pr_notice("[CCF] %s: sys=%s, id=%d, action = %s\n",
		__func__, sys->name, id, action?"PWN":"BUS_PROT");
	#endif
	if (allow[id] == 0) {
		#if MT_CCF_DEBUG
		pr_debug("[CCF] %s: do nothing return\n", __func__);
		#endif
		return 0;
	}
#endif

	mtk_clk_lock(flags);

	if (action == MTCMOS_BUS_PROT)
		r = sys->ops->prepare(sys);
	else if (action == MTCMOS_PWR)
		r = sys->ops->enable(sys);

	WARN_ON(r);

	mtk_clk_unlock(flags);

	if (action == MTCMOS_BUS_PROT) {
		list_for_each_entry(pgcb, &pgcb_list, list) {
			if (pgcb->after_on)
				pgcb->after_on(id);
		}
	}

	return r;
}

static int disable_subsys(enum subsys_id id, enum mtcmos_op action)
{
	int r = 0;
	unsigned long flags;
	struct subsys *sys = id_to_sys(id);
	struct pg_callbacks *pgcb;

	WARN_ON(!sys);

	if (!mtk_is_mtcmos_enable()) {
#if MT_CCF_DEBUG
		pr_notice("[CCF] skip %s: sys=%s, id=%d\n",
			__func__, sys->name, id);
#endif
		switch (id) {
		case SYS_MD1:
			spm_mtcmos_ctrl_md1_bus_prot(STA_POWER_DOWN);
			spm_mtcmos_ctrl_md1_pwr(STA_POWER_DOWN);
			break;
		case SYS_CONN:
			spm_mtcmos_ctrl_conn_bus_prot(STA_POWER_DOWN);
			spm_mtcmos_ctrl_conn_pwr(STA_POWER_DOWN);
			break;
		default:
			break;
		}
		return 0;
	}

#if CONTROL_LIMIT
	#if MT_CCF_DEBUG
	pr_notice("[CCF] %s: sys=%s, id=%d, action = %s\n",
		__func__, sys->name, id, action?"PWN":"BUS_PROT");
	#endif
	if (allow[id] == 0) {
		#if MT_CCF_DEBUG
		pr_debug("[CCF] %s: do nothing return\n", __func__);
		#endif
		return 0;
	}
#endif

	/* TODO: check all clocks related to this subsys are off */
	/* could be power off or not */
	if (action == MTCMOS_BUS_PROT) {
		list_for_each_entry_reverse(pgcb, &pgcb_list, list) {
			if (pgcb->before_off)
				pgcb->before_off(id);
		}
	}

	mtk_clk_lock(flags);

	if (action == MTCMOS_BUS_PROT)
		r = sys->ops->unprepare(sys);
	else if (action == MTCMOS_PWR)
		r = sys->ops->disable(sys);

	WARN_ON(r);

	mtk_clk_unlock(flags);

	return r;
}

/*
 * power_gate
 */

#define CLK_NUM	10
struct mt_power_gate {
	struct clk_hw hw;
	struct cg_list *pre_clk1_list;
	struct cg_list *pre_clk2_list;
	enum subsys_id pd_id;
};

#define to_power_gate(_hw) container_of(_hw, struct mt_power_gate, hw)

struct cg_list {
	const char *cg[CLK_NUM];
};

static int pg_is_enabled(struct clk_hw *hw)
{
	struct mt_power_gate *pg = to_power_gate(hw);

	if (!mtk_is_mtcmos_enable())
		return 1;
	else
		return subsys_is_on(pg->pd_id);
}

int pg_prepare(struct clk_hw *hw)
{
	int ret1 = 0, ret2 = 0, ret3 = 0, ret4 = 0;
	int i = 0;
	int skip_pg = 0;
	struct clk *clk;
	struct mt_power_gate *pg = to_power_gate(hw);
	struct subsys *sys =  id_to_sys(pg->pd_id);

#if CHECK_PWR_ST
	if (sys->ops->get_state(sys) == SUBSYS_PWR_ON)
		skip_pg = 1;
#endif				/* CHECK_PWR_ST */

	do {
		if (pg->pre_clk1_list == NULL)
			break;

		clk = pg->pre_clk1_list->cg[i] ?
			__clk_lookup(pg->pre_clk1_list->cg[i]) : NULL;

		if (clk)
			ret1 = clk_prepare_enable(clk);
		else
			break;
		if (ret1)
			break;

#if MT_CCF_DEBUG
		pr_notice("[CCF] %s 1: sys=%s, pre_clk=%s\n", __func__,
			__clk_get_name(hw->clk),
			pg->pre_clk1_list->cg[i] ?
			pg->pre_clk1_list->cg[i]:NULL);
#endif				/* MT_CCF_DEBUG */
		i++;
	} while (i < CLK_NUM);

	if (!skip_pg)
		ret2 = enable_subsys(pg->pd_id, MTCMOS_PWR);

	i = 0;

	do {
		if (pg->pre_clk2_list == NULL)
			break;

		clk = pg->pre_clk2_list->cg[i] ?
			__clk_lookup(pg->pre_clk2_list->cg[i]) : NULL;
		if (clk)
			ret3 = clk_prepare_enable(clk);
		else
			break;
		if (ret3)
			break;

#if MT_CCF_DEBUG
		pr_notice("[CCF] %s 2: sys=%s, pre_clk=%s\n", __func__,
			__clk_get_name(hw->clk),
			pg->pre_clk2_list->cg[i] ?
			pg->pre_clk2_list->cg[i]:NULL);
#endif				/* MT_CCF_DEBUG */
		i++;
	} while (i < CLK_NUM);

	if (!skip_pg && sys->ops->prepare)
		ret4 = enable_subsys(pg->pd_id, MTCMOS_BUS_PROT);
	if (ret2)
		return ret2;
	if (ret3)
		return ret3;
	if (ret4)
		return ret4;

	return ret1;
}

void pg_unprepare(struct clk_hw *hw)
{
	int i = 0;
	int skip_pg = 0;
	struct clk *clk;
	struct mt_power_gate *pg = to_power_gate(hw);
	struct subsys *sys =  id_to_sys(pg->pd_id);

#if CHECK_PWR_ST
	if (sys->ops->get_state(sys) == SUBSYS_PWR_DOWN)
		skip_pg = 1;
#endif				/* CHECK_PWR_ST */
	if (!skip_pg && sys->ops->unprepare)
		disable_subsys(pg->pd_id, MTCMOS_BUS_PROT);

	do {
		if (pg->pre_clk2_list == NULL)
			break;

		clk = pg->pre_clk2_list->cg[i] ?
			__clk_lookup(pg->pre_clk2_list->cg[i]) : NULL;

		if (clk)
			clk_disable_unprepare(clk);
		else
			break;
#if MT_CCF_DEBUG
		pr_notice("[CCF] %s: sys=%s, pre_clk=%s\n", __func__,
			__clk_get_name(hw->clk),
			pg->pre_clk2_list->cg[i] ?
			pg->pre_clk2_list->cg[i]:NULL);
#endif				/* MT_CCF_DEBUG */
		i++;
	} while (i < CLK_NUM);

	if (!skip_pg)
		disable_subsys(pg->pd_id, MTCMOS_PWR);

	i = 0;
	do {
		if (pg->pre_clk1_list == NULL)
			break;

		clk = pg->pre_clk1_list->cg[i] ?
			__clk_lookup(pg->pre_clk1_list->cg[i]) : NULL;

		if (clk)
			clk_disable_unprepare(clk);
		else
			break;
#if MT_CCF_DEBUG
		pr_notice("[CCF] %s: sys=%s, pre_clk=%s\n", __func__,
			__clk_get_name(hw->clk),
			pg->pre_clk1_list->cg[i] ?
			pg->pre_clk1_list->cg[i]:NULL);
#endif				/* MT_CCF_DEBUG */
		i++;
	} while (i < CLK_NUM);
}

static const struct clk_ops mt_power_gate_ops = {
	.prepare = pg_prepare,
	.unprepare = pg_unprepare,
	.is_enabled = pg_is_enabled,
};

struct clk *mt_clk_register_power_gate(const char *name,
					const char *parent_name,
					struct cg_list *pre_clk1_list,
					struct cg_list *pre_clk2_list,
					enum subsys_id pd_id)
{
	struct mt_power_gate *pg;
	struct clk *clk;
	struct clk_init_data init;

	pg = kzalloc(sizeof(*pg), GFP_KERNEL);
	if (!pg)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.flags = CLK_IGNORE_UNUSED;
	init.parent_names = parent_name ? &parent_name : NULL;
	init.num_parents = parent_name ? 1 : 0;
	init.ops = &mt_power_gate_ops;

	pg->pre_clk1_list = pre_clk1_list;
	pg->pre_clk2_list = pre_clk2_list;
	pg->pd_id = pd_id;
	pg->hw.init = &init;

	clk = clk_register(NULL, &pg->hw);
	if (IS_ERR(clk))
		kfree(pg);

	return clk;
}

#define pg_md1 "pg_md1"
#define pg_conn "pg_conn"
#define pg_dpy "pg_dpy"
#define pg_dis "pg_dis"
#define pg_mfg "pg_mfg"
#define pg_isp "pg_isp"
#define pg_ifr "pg_ifr"
#define pg_mfg_core0 "pg_mfg_core0"
#define pg_mfg_async "pg_mfg_async"
#define pg_cam "pg_cam"
#define pg_vcodec "pg_vcodec"

struct cg_list mm_cg1 = {.cg = {"mm_sel"},};

struct cg_list mm_cg2 = {
		.cg = {
			"mm_smi_common",
			"mm_smi_comm0",
			"mm_smi_comm1",
			"mm_smi_larb0"
		},
};

struct cg_list mfg_cg = {.cg = {"mfg_sel"},};

struct cg_list isp_cg = {
		.cg = {
			"img_larb2",
			"mm_smi_img_ck"
		},
};

struct cg_list cam_cg = {
		.cg = {
			"cam_larb3",
			"cam_dfp_vad",
			"cam",
			"cam_ccu",
			"mm_smi_cam_ck"
		},
};

struct mtk_power_gate {
	int id;
	const char *name;
	const char *parent_name;
	enum subsys_id pd_id;
	struct cg_list *pre_clk1_names;
	struct cg_list *pre_clk2_names;
};

#define PGATE(_id, _name, _parent, _pre_clk, _pd_id) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.pre_clk_name = _pre_clk,		\
		.pd_id = _pd_id,			\
	}

#define PGATE2(_id, _name, _parent, _pre_clks1, _pre_clks2, _pd_id) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.pd_id = _pd_id,			\
		.pre_clk1_names = _pre_clks1,		\
		.pre_clk2_names = _pre_clks2,		\
	}

/* FIXME: all values needed to be verified */
struct mtk_power_gate scp_clks[] __initdata = {
	PGATE2(SCP_SYS_MD1, pg_md1, NULL, NULL, NULL, SYS_MD1),
	PGATE2(SCP_SYS_CONN, pg_conn, NULL, NULL, NULL, SYS_CONN),
	PGATE2(SCP_SYS_DPY, pg_dpy, NULL, NULL, NULL, SYS_DPY),
	PGATE2(SCP_SYS_DIS, pg_dis, NULL, &mm_cg1, &mm_cg2, SYS_DIS),
	PGATE2(SCP_SYS_MFG, pg_mfg, pg_mfg_async, NULL, NULL, SYS_MFG),
	PGATE2(SCP_SYS_ISP, pg_isp, pg_dis, NULL, &isp_cg, SYS_ISP),
	PGATE2(SCP_SYS_IFR, pg_ifr, NULL, NULL, NULL, SYS_IFR),
	PGATE2(SCP_SYS_MFG_CORE0, pg_mfg_core0, pg_mfg,
		NULL, NULL, SYS_MFG_CORE0),
	PGATE2(SCP_SYS_MFG_ASYNC, pg_mfg_async, NULL,
		&mfg_cg, NULL, SYS_MFG_ASYNC),
	PGATE2(SCP_SYS_CAM, pg_cam, pg_dis, NULL, &cam_cg, SYS_CAM),
	PGATE2(SCP_SYS_VCODEC, pg_vcodec, pg_dis, NULL, NULL, SYS_VCODEC),
};

static void __init init_clk_scpsys(struct clk_onecell_data *clk_data)
{
	int i;
	struct clk *clk;

	syss[SYS_MD1].ctl_addr = MD1_PWR_CON;
	syss[SYS_CONN].ctl_addr = CONN_PWR_CON;
	syss[SYS_DPY].ctl_addr = DPY_PWR_CON;
	syss[SYS_DIS].ctl_addr = DIS_PWR_CON;
	syss[SYS_MFG].ctl_addr = MFG_PWR_CON;
	syss[SYS_ISP].ctl_addr = ISP_PWR_CON;
	syss[SYS_IFR].ctl_addr = IFR_PWR_CON;
	syss[SYS_MFG_CORE0].ctl_addr = MFG_CORE0_PWR_CON;
	syss[SYS_MFG_ASYNC].ctl_addr = MFG_ASYNC_PWR_CON;
	syss[SYS_CAM].ctl_addr = CAM_PWR_CON;
	syss[SYS_VCODEC].ctl_addr = VCODEC_PWR_CON;

	for (i = 0; i < ARRAY_SIZE(scp_clks); i++) {
		struct mtk_power_gate *pg = &scp_clks[i];

		if (mtk_is_mtcmos_enable())
			clk = mt_clk_register_power_gate(pg->name,
				pg->parent_name, pg->pre_clk1_names,
				pg->pre_clk2_names, pg->pd_id);
		else
			clk = mt_clk_register_power_gate(pg->name,
				pg->parent_name, NULL,
				NULL, pg->pd_id);

		if (IS_ERR(clk)) {
			pr_debug("[CCF] %s: Failed to register clk %s: %ld\n",
				__func__, pg->name, PTR_ERR(clk));
			continue;
		}

		if (clk_data)
			clk_data->clks[pg->id] = clk;

#if MT_CCF_DEBUG
		pr_notice("[CCF] %s: pgate %3d: %s\n", __func__, i, pg->name);
#endif				/* MT_CCF_DEBUG */
	}
}

/*
 * device tree support
 */

/* TODO: remove this function */
static struct clk_onecell_data *alloc_clk_data(unsigned int clk_num)
{
	int i;
	struct clk_onecell_data *clk_data;

	clk_data = kzalloc(sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data)
		return NULL;

	clk_data->clks = kcalloc(clk_num, sizeof(struct clk *), GFP_KERNEL);
	if (!clk_data->clks) {
		kfree(clk_data);
		return NULL;
	}

	clk_data->clk_num = clk_num;

	for (i = 0; i < clk_num; ++i)
		clk_data->clks[i] = ERR_PTR(-ENOENT);

	return clk_data;
}

/* TODO: remove this function */
static void __iomem *get_reg(struct device_node *np, int index)
{
#if DUMMY_REG_TEST
	return kzalloc(PAGE_SIZE, GFP_KERNEL);
#else
	return of_iomap(np, index);
#endif
}

static void __init mt_scpsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	int r;

	infracfg_base = get_reg(node, 0);
	spm_base = get_reg(node, 1);
	smi_common_base = get_reg(node, 2);
	infra_base = get_reg(node, 3);
	conn_base = get_reg(node, 4);
	conn_mcu_base = get_reg(node, 5);

	if (!infracfg_base || !spm_base || !smi_common_base || !infra_base ||
		!conn_base || !conn_mcu_base) {
		pr_debug("clk-pg-mt6758: missing reg\n");
		return;
	}

	clk_data = alloc_clk_data(SCP_NR_SYSS);

	init_clk_scpsys(clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_notice("[CCF] %s:could not register clock provide\n",
			__func__);

	if (mtk_is_mtcmos_enable()) {
		/* subsys init: per modem owner request,
		 *disable modem power first
		 */
		disable_subsys(SYS_MD1, MTCMOS_PWR);
	} else {	/*power on all subsys for bring up */
#ifndef CONFIG_FPGA_EARLY_PORTING
		spm_mtcmos_ctrl_md1_bus_prot(STA_POWER_DOWN);/*do after ccif*/
		spm_mtcmos_ctrl_md1_pwr(STA_POWER_DOWN);/*do after ccif*/
		spm_mtcmos_ctrl_conn_bus_prot(STA_POWER_DOWN);
		spm_mtcmos_ctrl_conn_pwr(STA_POWER_DOWN);
		spm_mtcmos_ctrl_dpy_pwr(STA_POWER_ON);
		spm_mtcmos_ctrl_dpy_bus_prot(STA_POWER_ON);
		spm_mtcmos_ctrl_dis_pwr(STA_POWER_ON);
		spm_mtcmos_ctrl_dis_bus_prot(STA_POWER_ON);
		spm_mtcmos_ctrl_isp_pwr(STA_POWER_ON);
		spm_mtcmos_ctrl_isp_bus_prot(STA_POWER_ON);
		spm_mtcmos_ctrl_ifr_pwr(STA_POWER_ON);
		spm_mtcmos_ctrl_mfg_async_pwr(STA_POWER_ON);
		spm_mtcmos_ctrl_mfg_pwr(STA_POWER_ON);
		spm_mtcmos_ctrl_mfg_bus_prot(STA_POWER_ON);
		spm_mtcmos_ctrl_mfg_core0_pwr(STA_POWER_ON);
		spm_mtcmos_ctrl_cam_pwr(STA_POWER_ON);
		spm_mtcmos_ctrl_cam_bus_prot(STA_POWER_ON);
		spm_mtcmos_ctrl_vcodec_pwr(STA_POWER_ON);
#endif
	}
}

CLK_OF_DECLARE_DRIVER(mtk_pg_regs, "mediatek,scpsys", mt_scpsys_init);

#if 0
int mtcmos_mfg_series_on(void)
{
	unsigned int sta = spm_read(PWR_STATUS);
	unsigned int sta_s = spm_read(PWR_STATUS_2ND);

	int ret;

	ret = 0;
	ret |= (sta & (1U << 1)) && (sta_s & (1U << 1));
	ret |= ((sta & (1U << 2)) && (sta_s & (1U << 2))) << 1;
	ret |= ((sta & (1U << 3)) && (sta_s & (1U << 3))) << 2;
	ret |= ((sta & (1U << 4)) && (sta_s & (1U << 4))) << 3;
	/*mfgsys_cg_check();*/
	return ret;
}
#endif

void subsys_if_on(void)
{
#if 0
	unsigned int sta = spm_read(PWR_STATUS);
	unsigned int sta_s = spm_read(PWR_STATUS_2ND);
	int ret = 0;

	if ((sta & (1U << 0)) && (sta_s & (1U << 0)))
		pr_debug("suspend warning: SYS_MD1 is on!!!\n");

	if ((sta & (1U << 1)) && (sta_s & (1U << 1))) {
		pr_notice("suspend warning: SYS_CONN is on!!!\n");
		ret++;
	}
	if ((sta & (1U << 2)) && (sta_s & (1U << 2)))
		pr_debug("suspend warning: SYS_DPY is on!!!\n");

	if ((sta & (1U << 3)) && (sta_s & (1U << 3))) {
		pr_notice("suspend warning: SYS_DIS is on!!!\n");
		ret++;
	}
	if ((sta & (1U << 4)) && (sta_s & (1U << 4))) {
		pr_notice("suspend warning: SYS_MFG is on!!!\n");
		ret++;
	}
	if ((sta & (1U << 5)) && (sta_s & (1U << 5))) {
		pr_notice("suspend warning: SYS_ISP is on!!!\n");
		ret++;
	}
	if ((sta & (1U << 6)) && (sta_s & (1U << 6)))
		pr_debug("suspend warning: SYS_IFR is on!!!\n");

	if ((sta & (1U << 7)) && (sta_s & (1U << 7))) {
		pr_notice("suspend warning: SYS_MFG_CORE0 is on!!!\n");
		ret++;
	}
	if ((sta & (1U << 23)) && (sta_s & (1U << 23))) {
		pr_notice("suspend warning: SYS_MFG_ASYNC is on!!!\n");
		ret++;
	}
	if ((sta & (1U << 25)) && (sta_s & (1U << 25))) {
		pr_notice("suspend warning: SYS_CAM is on!!!\n");
		ret++;
	}
	if ((sta & (1U << 26)) && (sta_s & (1U << 26))) {
		pr_notice("suspend warning: SYS_VCODEC is on!!!\n");
		ret++;
	}
	if (ret > 0)
		WARN_ON(1);
#endif
}

#if 1 /*only use for suspend test*/
void mtcmos_force_off(void)
{
	spm_mtcmos_ctrl_md1_bus_prot(STA_POWER_DOWN);
	spm_mtcmos_ctrl_md1_pwr(STA_POWER_DOWN);/*do after ccif*/
	spm_mtcmos_ctrl_conn_bus_prot(STA_POWER_DOWN);
	spm_mtcmos_ctrl_conn_pwr(STA_POWER_DOWN);
	/* spm_mtcmos_ctrl_dpy(STA_POWER_DOWN); */
	spm_mtcmos_ctrl_isp_bus_prot(STA_POWER_DOWN);
	spm_mtcmos_ctrl_isp_pwr(STA_POWER_DOWN);
	/* spm_mtcmos_ctrl_ifr(STA_POWER_DOWN); */
	spm_mtcmos_ctrl_mfg_core0_pwr(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg_bus_prot(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg_pwr(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg_async_pwr(STA_POWER_DOWN);
	spm_mtcmos_ctrl_cam_bus_prot(STA_POWER_DOWN);
	spm_mtcmos_ctrl_cam_pwr(STA_POWER_DOWN);
	spm_mtcmos_ctrl_vcodec_pwr(STA_POWER_DOWN);
	spm_mtcmos_ctrl_dis_bus_prot(STA_POWER_DOWN);
	spm_mtcmos_ctrl_dis_pwr(STA_POWER_DOWN);

}
#endif

#if CLK_DEBUG
/*
 * debug / unit test
 */

#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/module.h>

static char last_cmd[128] = "null";

static int test_pg_dump_regs(struct seq_file *s, void *v)
{
	int i;

	for (i = 0; i < NR_SYSS; i++) {
		if (!syss[i].ctl_addr)
			continue;

		seq_printf(s, "%10s: [0x%p]: 0x%08x\n", syss[i].name,
			   syss[i].ctl_addr, clk_readl(syss[i].ctl_addr));
	}

	return 0;
}

static void dump_pg_state(const char *clkname, struct seq_file *s)
{
	struct clk *c = __clk_lookup(clkname);
	struct clk *p = IS_ERR_OR_NULL(c) ? NULL : clk_get_parent(c);

	if (IS_ERR_OR_NULL(c)) {
		seq_printf(s, "[%17s: NULL]\n", clkname);
		return;
	}

	seq_printf(s, "[%17s: %3s, %3d, %10lu, %7s]\n",
		   __clk_get_name(c),
		   __clk_is_enabled(c) ? "ON" : "off",
		   __clk_get_enable_count(c), clk_get_rate(c),
			p ? __clk_get_name(p) : "");


	clk_put(c);
}

static int test_pg_dump_state_all(struct seq_file *s, void *v)
{
	static const char *const clks[] = {
		pg_md1,
		pg_conn,
		pg_dpy,
		pg_dis,
		pg_mfg,
		pg_isp,
		pg_ifr,
		pg_mfg_core0,
		pg_mfg_async,
		pg_cam,
		pg_vcodec,
	};

	int i;

/*	pr_debug("\n");*/
	for (i = 0; i < ARRAY_SIZE(clks); i++)
		dump_pg_state(clks[i], s);

	return 0;
}

static struct {
	const char *name;
	struct clk *clk;
} g_clks[] = {
	{
	.name = pg_md1}, {
	.name = pg_vcodec}, {
.name = pg_mfg},};

static int test_pg_1(struct seq_file *s, void *v)
{
	int i;

/*	pr_debug("\n");*/

	for (i = 0; i < ARRAY_SIZE(g_clks); i++) {
		g_clks[i].clk = __clk_lookup(g_clks[i].name);
		if (IS_ERR_OR_NULL(g_clks[i].clk)) {
			seq_printf(s, "clk_get(%s): NULL\n", g_clks[i].name);
			continue;
		}

		clk_prepare_enable(g_clks[i].clk);
		seq_printf(s, "clk_prepare_enable(%s)\n",
			__clk_get_name(g_clks[i].clk));
	}

	return 0;
}

static int test_pg_2(struct seq_file *s, void *v)
{
	int i;

/*	pr_debug("\n");*/

	for (i = 0; i < ARRAY_SIZE(g_clks); i++) {
		if (IS_ERR_OR_NULL(g_clks[i].clk)) {
			seq_printf(s, "(%s).clk: NULL\n", g_clks[i].name);
			continue;
		}

		seq_printf(s, "clk_disable_unprepare(%s)\n",
			__clk_get_name(g_clks[i].clk));
		clk_disable_unprepare(g_clks[i].clk);
		clk_put(g_clks[i].clk);
	}

	return 0;
}

static int test_pg_show(struct seq_file *s, void *v)
{
	static const struct {
		int (*fn)(struct seq_file *s, void *p);
		const char *cmd;
	} cmds[] = {
		{
		.cmd = "dump_regs", .fn = test_pg_dump_regs}, {
		.cmd = "dump_state", .fn = test_pg_dump_state_all}, {
		.cmd = "1", .fn = test_pg_1}, {
	.cmd = "2", .fn = test_pg_2},};

	int i;

/*	pr_debug("last_cmd: %s\n", last_cmd);*/

	for (i = 0; i < ARRAY_SIZE(cmds); i++) {
		if (strcmp(cmds[i].cmd, last_cmd) == 0)
			return cmds[i].fn(s, v);
	}

	return 0;
}

static int test_pg_open(struct inode *inode, struct file *file)
{
	return single_open(file, test_pg_show, NULL);
}

static ssize_t test_pg_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *data)
{
	char desc[sizeof(last_cmd)];
	int len = 0;

/*	pr_debug("count: %zu\n", count);*/
	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';
	strcpy(last_cmd, desc);
	if (last_cmd[len - 1] == '\n')
		last_cmd[len - 1] = 0;

	return count;
}

static const struct file_operations test_pg_fops = {
	.owner = THIS_MODULE,
	.open = test_pg_open,
	.read = seq_read,
	.write = test_pg_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init debug_init(void)
{
	static int init;
	struct proc_dir_entry *entry;

/*	pr_debug("init: %d\n", init);*/

	if (init)
		return 0;

	++init;

	entry = proc_create("test_pg", 0000, 0000, &test_pg_fops);
	if (!entry)
		return -ENOMEM;

	++init;
	return 0;
}

static void __exit debug_exit(void)
{
	remove_proc_entry("test_pg", NULL);
}

module_init(debug_init);
module_exit(debug_exit);

#endif				/* CLK_DEBUG */
