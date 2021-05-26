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
#include <linux/platform_device.h>
#include <linux/time64.h>
#include <linux/timekeeping.h>
#include <linux/sched/clock.h>

#include "clk-mtk-v1.h"
#include "clk-mt6853-pg.h"
#include "clkdbg-mt6853.h"

#include <dt-bindings/clock/mt6853-clk.h>

#define MT_CCF_DEBUG	0
#define MT_CCF_BRINGUP	0
#define CONTROL_LIMIT	1
#define	CHECK_PWR_ST	1

#define CONN_TIMEOUT_RECOVERY	5
#define CONN_TIMEOUT_STEP1	4

#define LP_CLK			1
#define NORMAL_CLK		0
#define CLK_ENABLE		1
#define CLK_DISABLE		0

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

#define clk_writel(addr, val)		mt_reg_sync_writel(val, addr)
#define clk_readl(addr)			__raw_readl(IOMEM(addr))

#define MFG_MISC_CON		INFRACFG_REG(0x0600)
#define MFG_DFD_TRIGGER (1<<19)

/*
 * MTCMOS
 */
#define STA_POWER_DOWN		0
#define STA_POWER_ON		1
#define SUBSYS_PWR_DOWN		0
#define SUBSYS_PWR_ON		1

static spinlock_t pgcb_lock;
LIST_HEAD(pgcb_list);

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
	void __iomem *sta_addr;
	void __iomem *sta_s_addr;
	uint32_t sram_pdn_bits;
	uint32_t sram_pdn_ack_bits;
	uint32_t bus_prot_mask;
	struct subsys_ops *ops;
};

/*static struct subsys_ops general_sys_ops;*/

static struct subsys_ops MD1_sys_ops;
static struct subsys_ops CONN_sys_ops;
static struct subsys_ops MFG0_sys_ops;
static struct subsys_ops MFG1_sys_ops;
static struct subsys_ops MFG2_sys_ops;
static struct subsys_ops MFG3_sys_ops;
static struct subsys_ops MFG5_sys_ops;
static struct subsys_ops ISP_sys_ops;
static struct subsys_ops ISP2_sys_ops;
static struct subsys_ops IPE_sys_ops;
static struct subsys_ops VDE_sys_ops;
static struct subsys_ops VEN_sys_ops;
static struct subsys_ops DIS_sys_ops;
static struct subsys_ops AUDIO_sys_ops;
static struct subsys_ops ADSP_sys_ops;
static struct subsys_ops CAM_sys_ops;
static struct subsys_ops CAM_RAWA_sys_ops;
static struct subsys_ops CAM_RAWB_sys_ops;
static struct subsys_ops VPU_sys_ops;

static void __iomem *infracfg_base;/* infracfg_ao */
static void __iomem *spm_base;/* spm */
static void __iomem *infra_base;/* infra */
static void __iomem *infra_pdn_base;/* infra_pdn */
static void __iomem *apu_vcore_base;
static void __iomem *apu_conn_base;

#define INFRACFG_REG(offset)		(infracfg_base + offset)
#define SPM_REG(offset)			(spm_base + offset)
#define INFRA_REG(offset)		(infra_base + offset)
#define INFRA_PDN_REG(offset)		(infra_pdn_base + offset)

/**************************************
 * for non-CPU MTCMOS
 **************************************/
#define POWERON_CONFIG_EN	SPM_REG(0x0000)
#define PWR_STATUS		SPM_REG(0x016C)
#define PWR_STATUS_2ND		SPM_REG(0x0170)
#define OTHER_PWR_STATUS	SPM_REG(0x0178)	/* for MT6873 VPU only */

#define MD1_PWR_CON		SPM_REG(0x300)
#define CONN_PWR_CON		SPM_REG(0x304)
#define MFG0_PWR_CON		SPM_REG(0x308)
#define MFG1_PWR_CON		SPM_REG(0x30C)
#define MFG2_PWR_CON		SPM_REG(0x310)
#define MFG3_PWR_CON		SPM_REG(0x314)
#define MFG5_PWR_CON		SPM_REG(0x31C)
#define IFR_PWR_CON		SPM_REG(0x324)
#define IFR_SUB_PWR_CON		SPM_REG(0x328)
#define DPY_PWR_CON		SPM_REG(0x32C)
#define ISP_PWR_CON		SPM_REG(0x330)
#define ISP2_PWR_CON		SPM_REG(0x334)
#define IPE_PWR_CON		SPM_REG(0x338)
#define VDE_PWR_CON		SPM_REG(0x33C)
#define VEN_PWR_CON		SPM_REG(0x344)
#define DIS_PWR_CON		SPM_REG(0x350)
#define AUDIO_PWR_CON		SPM_REG(0x354)
#define ADSP_PWR_CON		SPM_REG(0x358)
#define CAM_PWR_CON		SPM_REG(0x35C)
#define CAM_RAWA_PWR_CON	SPM_REG(0x360)
#define CAM_RAWB_PWR_CON	SPM_REG(0x364)
#define MD_EXT_BUCK_ISO_CON	SPM_REG(0x398)
#define EXT_BUCK_ISO		SPM_REG(0x39C)

#define SPM_CROSS_WAKE_M01_REQ	SPM_REG(0x670)	/* for MT6873 VPU wakeup src */
#define APMCU_WAKEUP_APU	(0x1 << 0)

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

/* for DISP/MDP MTCMOS on/off APIs  */
#define INFRA_TOPAXI_PROTECTEN				INFRACFG_REG(0x0220)
#define INFRA_TOPAXI_PROTECTEN_SET			INFRACFG_REG(0x02A0)
#define INFRA_TOPAXI_PROTECTEN_CLR			INFRACFG_REG(0x02A4)
#define INFRA_TOPAXI_PROTECTEN_STA0			INFRACFG_REG(0x0224)
#define INFRA_TOPAXI_PROTECTEN_STA1			INFRACFG_REG(0x0228)

#define INFRA_TOPAXI_PROTECTEN_1			INFRACFG_REG(0x0250)
#define INFRA_TOPAXI_PROTECTEN_1_SET			INFRACFG_REG(0x02A8)
#define INFRA_TOPAXI_PROTECTEN_1_CLR			INFRACFG_REG(0x02AC)
#define INFRA_TOPAXI_PROTECTEN_STA0_1			INFRACFG_REG(0x0254)
#define INFRA_TOPAXI_PROTECTEN_STA1_1			INFRACFG_REG(0x0258)

#define INFRA_TOPAXI_PROTECTEN_MCU			INFRACFG_REG(0x02C0)
#define INFRA_TOPAXI_PROTECTEN_MCU_STA0		INFRACFG_REG(0x02E0)
#define INFRA_TOPAXI_PROTECTEN_MCU_STA1		INFRACFG_REG(0x02E4)
#define INFRA_TOPAXI_PROTECTEN_MCU_SET			INFRACFG_REG(0x02C4)
#define INFRA_TOPAXI_PROTECTEN_MCU_CLR			INFRACFG_REG(0x02C8)

#define INFRA_TOPAXI_PROTECTEN_MM			INFRACFG_REG(0x02D0)
#define INFRA_TOPAXI_PROTECTEN_MM_SET			INFRACFG_REG(0x02D4)
#define INFRA_TOPAXI_PROTECTEN_MM_CLR			INFRACFG_REG(0x02D8)
#define INFRA_TOPAXI_PROTECTEN_MM_STA0		INFRACFG_REG(0x02E8)
#define INFRA_TOPAXI_PROTECTEN_MM_STA1		INFRACFG_REG(0x02EC)

#define INFRA_TOPAXI_PROTECTEN_2			INFRACFG_REG(0x0710)
#define INFRA_TOPAXI_PROTECTEN_2_SET			INFRACFG_REG(0x0714)
#define INFRA_TOPAXI_PROTECTEN_2_CLR			INFRACFG_REG(0x0718)
#define INFRA_TOPAXI_PROTECTEN_STA0_2			INFRACFG_REG(0x0720)
#define INFRA_TOPAXI_PROTECTEN_STA1_2			INFRACFG_REG(0x0724)

#define INFRA_TOPAXI_PROTECTEN_MM_2			INFRACFG_REG(0x0DC8)
#define INFRA_TOPAXI_PROTECTEN_MM_2_SET		INFRACFG_REG(0x0DCC)
#define INFRA_TOPAXI_PROTECTEN_MM_2_CLR		INFRACFG_REG(0x0DD0)
#define INFRA_TOPAXI_PROTECTEN_MM_2_STA0		INFRACFG_REG(0x0DD4)
#define INFRA_TOPAXI_PROTECTEN_MM_2_STA1		INFRACFG_REG(0x0DD8)

#define INFRA_TOPAXI_PROTECTEN_VDNR			INFRACFG_REG(0x0B80)
#define INFRA_TOPAXI_PROTECTEN_VDNR_SET		INFRACFG_REG(0x0B84)
#define INFRA_TOPAXI_PROTECTEN_VDNR_CLR		INFRACFG_REG(0x0B88)
#define INFRA_TOPAXI_PROTECTEN_VDNR_STA0		INFRACFG_REG(0x0B8C)
#define INFRA_TOPAXI_PROTECTEN_VDNR_STA1		INFRACFG_REG(0x0B90)

#define INFRA_PDN_MFG1_WAY_EN				INFRA_PDN_REG(0x004C)

/* Autogen Begin, 20200210 version  */
#define  SPM_PROJECT_CODE    0xB16

/* Define MTCMOS power control */
#define PWR_RST_B		(0x1 << 0)
#define PWR_ISO			(0x1 << 1)
#define PWR_ON			(0x1 << 2)
#define PWR_ON_2ND		(0x1 << 3)
#define PWR_CLK_DIS		(0x1 << 4)
#define SRAM_CKISO		(0x1 << 5)
#define SRAM_ISOINT_B		(0x1 << 6)
#define DORMANT_ENABLE		(0x1 << 6)
#define SLPB_CLAMP		(0x1 << 7)
#define VPROC_EXT_OFF		(0x1 << 7)

/* Define MTCMOS Bus Protect Mask */
#define MD1_PROT_STEP1_0_MASK			((0x1 << 7))
#define MD1_PROT_STEP1_0_ACK_MASK		((0x1 << 7))
#define MD1_PROT_STEP2_0_MASK			((0x1 << 2) \
						|(0x1 << 12) \
						|(0x1 << 20))
#define MD1_PROT_STEP2_0_ACK_MASK		((0x1 << 2) \
						|(0x1 << 12) \
						|(0x1 << 20))
#define CONN_PROT_STEP1_0_MASK			((0x1 << 13) \
						|(0x1 << 18))
#define CONN_PROT_STEP1_0_ACK_MASK		((0x1 << 13) \
						|(0x1 << 18))
#define CONN_PROT_STEP2_0_MASK			((0x1 << 14))
#define CONN_PROT_STEP2_0_ACK_MASK		((0x1 << 14))
#define CONN_PROT_STEP2_1_MASK			((0x1 << 10))
#define CONN_PROT_STEP2_1_ACK_MASK		((0x1 << 10))
#define MFG1_PROT_STEP0_WAY_EN_ON_MASK		((0x1 << 11) \
						|(0x1 << 12))
#define MFG1_PROT_STEP0_WAY_EN_OFF_MASK		((0x1 << 12))
#define MFG1_PROT_STEP0_WAY_EN_ON_MASK_ACK	((0x1 << 11) \
						|(0x1 << 12))
#define MFG1_PROT_STEP0_WAY_EN_OFF_MASK_ACK	((0x1 << 12))
#define MFG1_PROT_STEP1_0_MASK			((0x1 << 21))
#define MFG1_PROT_STEP1_0_ACK_MASK		((0x1 << 21))
#define MFG1_PROT_STEP1_1_MASK			((0x1 << 5) \
						|(0x1 << 6))
#define MFG1_PROT_STEP1_1_ACK_MASK		((0x1 << 5) \
						|(0x1 << 6))
#define MFG1_PROT_STEP2_0_MASK			((0x1 << 21) \
						|(0x1 << 22))
#define MFG1_PROT_STEP2_0_ACK_MASK		((0x1 << 21) \
						|(0x1 << 22))
#define MFG1_PROT_STEP2_1_MASK			((0x1 << 7))
#define MFG1_PROT_STEP2_1_ACK_MASK		((0x1 << 7))
#define IFR_PROT_STEP1_0_MASK			((0x1 << 3) \
						|(0x1 << 4) \
						|(0x1 << 7) \
						|(0x1 << 9) \
						|(0x1 << 12) \
						|(0x1 << 13) \
						|(0x1 << 16) \
						|(0x1 << 17) \
						|(0x1 << 18) \
						|(0x1 << 20) \
						|(0x1 << 24) \
						|(0x1 << 28))
#define IFR_PROT_STEP1_0_ACK_MASK		((0x1 << 3) \
						|(0x1 << 4) \
						|(0x1 << 7) \
						|(0x1 << 9) \
						|(0x1 << 12) \
						|(0x1 << 13) \
						|(0x1 << 16) \
						|(0x1 << 17) \
						|(0x1 << 18) \
						|(0x1 << 20) \
						|(0x1 << 24) \
						|(0x1 << 28))
#define IFR_PROT_STEP1_1_MASK			((0x1 << 21))
#define IFR_PROT_STEP1_1_ACK_MASK		((0x1 << 21))
#define IFR_PROT_STEP1_2_MASK			((0x1 << 0))
#define IFR_PROT_STEP1_2_ACK_MASK		((0x1 << 0))
#define IFR_PROT_STEP1_3_MASK			((0x1 << 4) \
						|(0x1 << 14))
#define IFR_PROT_STEP1_3_ACK_MASK		((0x1 << 4) \
						|(0x1 << 14))
#define IFR_PROT_STEP1_4_MASK			((0x1 << 31))
#define IFR_PROT_STEP1_4_ACK_MASK		((0x1 << 31))
#define IFR_PROT_STEP2_0_MASK			((0x1 << 14))
#define IFR_PROT_STEP2_0_ACK_MASK		((0x1 << 14))
#define IFR_PROT_STEP2_1_MASK			((0x1 << 22))
#define IFR_PROT_STEP2_1_ACK_MASK		((0x1 << 22))
#define IFR_PROT_STEP2_2_MASK			((0x1 << 7))
#define IFR_PROT_STEP2_2_ACK_MASK		((0x1 << 7))
#define IFR_PROT_STEP2_3_MASK			((0x1 << 5))
#define IFR_PROT_STEP2_3_ACK_MASK		((0x1 << 5))
#define IFR_PROT_STEP2_4_MASK			((0x1 << 8) \
						|(0x1 << 20) \
						|(0x1 << 21) \
						|(0x1 << 22))
#define IFR_PROT_STEP2_4_ACK_MASK		((0x1 << 8) \
						|(0x1 << 20) \
						|(0x1 << 21) \
						|(0x1 << 22))
#define ISP_PROT_STEP1_0_MASK			((0x1 << 8))
#define ISP_PROT_STEP1_0_ACK_MASK		((0x1 << 8))
#define ISP_PROT_STEP2_0_MASK			((0x1 << 9))
#define ISP_PROT_STEP2_0_ACK_MASK		((0x1 << 9))
#define IPE_PROT_STEP1_0_MASK			((0x1 << 16))
#define IPE_PROT_STEP1_0_ACK_MASK		((0x1 << 16))
#define IPE_PROT_STEP2_0_MASK			((0x1 << 17))
#define IPE_PROT_STEP2_0_ACK_MASK		((0x1 << 17))
#define VDE_PROT_STEP1_0_MASK			((0x1 << 24))
#define VDE_PROT_STEP1_0_ACK_MASK		((0x1 << 24))
#define VDE_PROT_STEP2_0_MASK			((0x1 << 25))
#define VDE_PROT_STEP2_0_ACK_MASK		((0x1 << 25))
#define VEN_PROT_STEP1_0_MASK			((0x1 << 26))
#define VEN_PROT_STEP1_0_ACK_MASK		((0x1 << 26))
#define VEN_PROT_STEP2_0_MASK			((0x1 << 27))
#define VEN_PROT_STEP2_0_ACK_MASK		((0x1 << 27))
#define DIS_PROT_STEP1_0_MASK			((0x1 << 0) \
						|(0x1 << 2) \
						|(0x1 << 10) \
						|(0x1 << 12) \
						|(0x1 << 16) \
						|(0x1 << 24) \
						|(0x1 << 26))
#define DIS_PROT_STEP1_0_ACK_MASK		((0x1 << 0) \
						|(0x1 << 2) \
						|(0x1 << 10) \
						|(0x1 << 12) \
						|(0x1 << 16) \
						|(0x1 << 24) \
						|(0x1 << 26))
#define DIS_PROT_STEP1_1_MASK			((0x1 << 8))
#define DIS_PROT_STEP1_1_ACK_MASK		((0x1 << 8))
#define DIS_PROT_STEP2_0_MASK			((0x1 << 6) \
						|(0x1 << 23))
#define DIS_PROT_STEP2_0_ACK_MASK		((0x1 << 6) \
						|(0x1 << 23))
#define DIS_PROT_STEP2_1_MASK			((0x1 << 1) \
						|(0x1 << 3) \
						|(0x1 << 17) \
						|(0x1 << 25) \
						|(0x1 << 27))
#define DIS_PROT_STEP2_1_ACK_MASK		((0x1 << 1) \
						|(0x1 << 3) \
						|(0x1 << 17) \
						|(0x1 << 25) \
						|(0x1 << 27))
#define DIS_PROT_STEP2_2_MASK			((0x1 << 9))
#define DIS_PROT_STEP2_2_ACK_MASK		((0x1 << 9))
#define AUDIO_PROT_STEP1_0_MASK			((0x1 << 4))
#define AUDIO_PROT_STEP1_0_ACK_MASK		((0x1 << 4))
#define ADSP_PROT_STEP1_0_MASK			((0x1 << 3))
#define ADSP_PROT_STEP1_0_ACK_MASK		((0x1 << 3))
#define CAM_PROT_STEP1_0_MASK			((0x1 << 0))
#define CAM_PROT_STEP1_0_ACK_MASK		((0x1 << 0))
#define CAM_PROT_STEP1_1_MASK			((0x1 << 0) \
						|(0x1 << 2))
#define CAM_PROT_STEP1_1_ACK_MASK		((0x1 << 0) \
						|(0x1 << 2))
#define CAM_PROT_STEP2_0_MASK			((0x1 << 22))
#define CAM_PROT_STEP2_0_ACK_MASK		((0x1 << 22))
#define CAM_PROT_STEP2_1_MASK			((0x1 << 1) \
						|(0x1 << 3))
#define CAM_PROT_STEP2_1_ACK_MASK		((0x1 << 1) \
						|(0x1 << 3))
#define CAM_PROT_STEP2_2_MASK			((0x1 << 19))
#define CAM_PROT_STEP2_2_ACK_MASK		((0x1 << 19))

/* Define MTCMOS Power Status Mask */

#define MD1_PWR_STA_MASK                 (0x1 << 0)
#define CONN_PWR_STA_MASK                (0x1 << 1)
#define MFG0_PWR_STA_MASK                (0x1 << 2)
#define MFG1_PWR_STA_MASK                (0x1 << 3)
#define MFG2_PWR_STA_MASK                (0x1 << 4)
#define MFG3_PWR_STA_MASK                (0x1 << 5)
#define MFG5_PWR_STA_MASK                (0x1 << 7)
#define DPY_PWR_STA_MASK                 (0x1 << 11)
#define ISP_PWR_STA_MASK                 (0x1 << 12)
#define ISP2_PWR_STA_MASK                (0x1 << 13)
#define IPE_PWR_STA_MASK                 (0x1 << 14)
#define VDE_PWR_STA_MASK                 (0x1 << 15)
#define VEN_PWR_STA_MASK                 (0x1 << 17)
#define MDP_PWR_STA_MASK                 (0x1 << 19)
#define DIS_PWR_STA_MASK                 (0x1 << 20)
#define AUDIO_PWR_STA_MASK               (0x1 << 21)
#define ADSP_PWR_STA_MASK                (0x1 << 22)
#define CAM_PWR_STA_MASK                 (0x1 << 23)
#define CAM_RAWA_PWR_STA_MASK            (0x1 << 24)
#define CAM_RAWB_PWR_STA_MASK            (0x1 << 25)

/* Define CPU SRAM Mask */

/* Define Non-CPU SRAM Mask */
#define MD1_SRAM_PDN                     (0x1 << 8)
#define MD1_SRAM_PDN_ACK                 (0x0 << 12)
#define MD1_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define MFG0_SRAM_PDN                    (0x1 << 8)
#define MFG0_SRAM_PDN_ACK                (0x1 << 12)
#define MFG0_SRAM_PDN_ACK_BIT0           (0x1 << 12)
#define MFG1_SRAM_PDN                    (0x1 << 8)
#define MFG1_SRAM_PDN_ACK                (0x1 << 12)
#define MFG1_SRAM_PDN_ACK_BIT0           (0x1 << 12)
#define MFG2_SRAM_PDN                    (0x1 << 8)
#define MFG2_SRAM_PDN_ACK                (0x1 << 12)
#define MFG2_SRAM_PDN_ACK_BIT0           (0x1 << 12)
#define MFG3_SRAM_PDN                    (0x1 << 8)
#define MFG3_SRAM_PDN_ACK                (0x1 << 12)
#define MFG3_SRAM_PDN_ACK_BIT0           (0x1 << 12)
#define MFG5_SRAM_PDN                    (0x1 << 8)
#define MFG5_SRAM_PDN_ACK                (0x1 << 12)
#define MFG5_SRAM_PDN_ACK_BIT0           (0x1 << 12)
#define ISP_SRAM_PDN                     (0x1 << 8)
#define ISP_SRAM_PDN_ACK                 (0x1 << 12)
#define ISP_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define ISP2_SRAM_PDN                    (0x1 << 8)
#define ISP2_SRAM_PDN_ACK                (0x1 << 12)
#define ISP2_SRAM_PDN_ACK_BIT0           (0x1 << 12)
#define IPE_SRAM_PDN                     (0x1 << 8)
#define IPE_SRAM_PDN_ACK                 (0x1 << 12)
#define IPE_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define VDE_SRAM_PDN                     (0x1 << 8)
#define VDE_SRAM_PDN_ACK                 (0x1 << 12)
#define VDE_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define VEN_SRAM_PDN                     (0x1 << 8)
#define VEN_SRAM_PDN_ACK                 (0x1 << 12)
#define VEN_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define DIS_SRAM_PDN                     (0x1 << 8)
#define DIS_SRAM_PDN_ACK                 (0x1 << 12)
#define DIS_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define AUDIO_SRAM_PDN                   (0x1 << 8)
#define AUDIO_SRAM_PDN_ACK               (0x1 << 12)
#define AUDIO_SRAM_PDN_ACK_BIT0          (0x1 << 12)
#define ADSP_SRAM_PDN                    (0x1 << 8)
#define ADSP_SRAM_PDN_ACK                (0x1 << 12)
#define ADSP_SRAM_PDN_ACK_BIT0           (0x1 << 12)
#define ADSP_SRAM_SLEEP_B                (0x1 << 9)
#define ADSP_SRAM_SLEEP_B_ACK            (0x1 << 13)
#define ADSP_SRAM_SLEEP_B_ACK_BIT0       (0x1 << 13)
#define CAM_SRAM_PDN                     (0x1 << 8)
#define CAM_SRAM_PDN_ACK                 (0x1 << 12)
#define CAM_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define CAM_RAWA_SRAM_PDN                (0x1 << 8)
#define CAM_RAWA_SRAM_PDN_ACK            (0x1 << 12)
#define CAM_RAWA_SRAM_PDN_ACK_BIT0       (0x1 << 12)
#define CAM_RAWB_SRAM_PDN                (0x1 << 8)
#define CAM_RAWB_SRAM_PDN_ACK            (0x1 << 12)
#define CAM_RAWB_SRAM_PDN_ACK_BIT0       (0x1 << 12)

#define APU_VCORE_CG_CON		(apu_vcore_base + 0x0000)
#define APU_CONN_CG_CON		(apu_conn_base + 0x0000)
#define APU_VCORE_CG_CLR		(apu_vcore_base + 0x0008)
#define APU_CONN_CG_CLR			(apu_conn_base + 0x0008)

static struct subsys syss[] =	/* NR_SYSS */
{
	[SYS_MD1] = {
			.name = __stringify(SYS_MD1),
			.sta_mask = MD1_PWR_STA_MASK,
			.sram_pdn_bits = 8,
			.sram_pdn_ack_bits = 12,
			.bus_prot_mask = 0,
			.ops = &MD1_sys_ops,
			},
	[SYS_CONN] = {
			.name = __stringify(SYS_CONN),
			.sta_mask = CONN_PWR_STA_MASK,
			.sram_pdn_bits = 8,
			.sram_pdn_ack_bits = 12,
			.bus_prot_mask = 0,
			.ops = &CONN_sys_ops,
			},
	[SYS_MFG0] = {
			.name = __stringify(SYS_MFG0),
			.sta_mask = MFG0_PWR_STA_MASK,
			.sram_pdn_bits = 8,
			.sram_pdn_ack_bits = 12,
			.bus_prot_mask = 0,
			.ops = &MFG0_sys_ops,
			},
	[SYS_MFG1] = {
			.name = __stringify(SYS_MFG1),
			.sta_mask = MFG1_PWR_STA_MASK,
			.sram_pdn_bits = 8,
			.sram_pdn_ack_bits = 12,
			.bus_prot_mask = 0,
			.ops = &MFG1_sys_ops,
			},
	[SYS_MFG2] = {
			.name = __stringify(SYS_MFG2),
			.sta_mask = MFG2_PWR_STA_MASK,
			.sram_pdn_bits = 8,
			.sram_pdn_ack_bits = 12,
			.bus_prot_mask = 0,
			.ops = &MFG2_sys_ops,
			},
	[SYS_MFG3] = {
			.name = __stringify(SYS_MFG3),
			.sta_mask = MFG3_PWR_STA_MASK,
			.sram_pdn_bits = 8,
			.sram_pdn_ack_bits = 12,
			.bus_prot_mask = 0,
			.ops = &MFG3_sys_ops,
			},
	[SYS_MFG5] = {
			.name = __stringify(SYS_MFG5),
			.sta_mask = MFG5_PWR_STA_MASK,
			.sram_pdn_bits = 8,
			.sram_pdn_ack_bits = 12,
			.bus_prot_mask = 0,
			.ops = &MFG5_sys_ops,
			},
	[SYS_ISP] = {
			.name = __stringify(SYS_ISP),
			.sta_mask = ISP_PWR_STA_MASK,
			.sram_pdn_bits = 8,
			.sram_pdn_ack_bits = 12,
			.bus_prot_mask = 0,
			.ops = &ISP_sys_ops,
			},
	[SYS_ISP2] = {
			.name = __stringify(SYS_ISP2),
			.sta_mask = ISP2_PWR_STA_MASK,
			.sram_pdn_bits = 8,
			.sram_pdn_ack_bits = 12,
			.bus_prot_mask = 0,
			.ops = &ISP2_sys_ops,
			},
	[SYS_IPE] = {
			.name = __stringify(SYS_IPE),
			.sta_mask = IPE_PWR_STA_MASK,
			.sram_pdn_bits = 8,
			.sram_pdn_ack_bits = 12,
			.bus_prot_mask = 0,
			.ops = &IPE_sys_ops,
			},
	[SYS_VDE] = {
			.name = __stringify(SYS_VDE),
			.sta_mask = VDE_PWR_STA_MASK,
			.sram_pdn_bits = 8,
			.sram_pdn_ack_bits = 12,
			.bus_prot_mask = 0,
			.ops = &VDE_sys_ops,
			},
	[SYS_VEN] = {
			.name = __stringify(SYS_VEN),
			.sta_mask = VEN_PWR_STA_MASK,
			.sram_pdn_bits = 8,
			.sram_pdn_ack_bits = 12,
			.bus_prot_mask = 0,
			.ops = &VEN_sys_ops,
			},
	[SYS_DIS] = {
			.name = __stringify(SYS_DIS),
			.sta_mask = DIS_PWR_STA_MASK,
			.sram_pdn_bits = 8,
			.sram_pdn_ack_bits = 12,
			.bus_prot_mask = 0,
			.ops = &DIS_sys_ops,
			},
	[SYS_AUDIO] = {
			.name = __stringify(SYS_AUDIO),
			.sta_mask = AUDIO_PWR_STA_MASK,
			.sram_pdn_bits = 8,
			.sram_pdn_ack_bits = 12,
			.bus_prot_mask = 0,
			.ops = &AUDIO_sys_ops,
			},
	[SYS_ADSP] = {
			.name = __stringify(SYS_ADSP),
			.sta_mask = ADSP_PWR_STA_MASK,
			.sram_pdn_bits = 8,
			.sram_pdn_ack_bits = 12,
			.bus_prot_mask = 0,
			.ops = &ADSP_sys_ops,
			},
	[SYS_CAM] = {
			.name = __stringify(SYS_CAM),
			.sta_mask = CAM_PWR_STA_MASK,
			.sram_pdn_bits = 8,
			.sram_pdn_ack_bits = 12,
			.bus_prot_mask = 0,
			.ops = &CAM_sys_ops,
			},
	[SYS_CAM_RAWA] = {
			.name = __stringify(SYS_CAM_RAWA),
			.sta_mask = CAM_RAWA_PWR_STA_MASK,
			.sram_pdn_bits = 8,
			.sram_pdn_ack_bits = 12,
			.bus_prot_mask = 0,
			.ops = &CAM_RAWA_sys_ops,
			},
	[SYS_CAM_RAWB] = {
			.name = __stringify(SYS_CAM_RAWB),
			.sta_mask = CAM_RAWB_PWR_STA_MASK,
			.sram_pdn_bits = 8,
			.sram_pdn_ack_bits = 12,
			.bus_prot_mask = 0,
			.ops = &CAM_RAWB_sys_ops,
			},
	[SYS_VPU] = {
			.name = __stringify(SYS_VPU),
			.sta_mask = (0x1 << 5),
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &VPU_sys_ops,
			},
};

struct pg_callbacks *register_pg_callback(struct pg_callbacks *pgcb)
{
	unsigned long spinlock_save_flags;

	spin_lock_irqsave(&pgcb_lock, spinlock_save_flags);

	INIT_LIST_HEAD(&pgcb->list);

	list_add(&pgcb->list, &pgcb_list);

	spin_unlock_irqrestore(&pgcb_lock, spinlock_save_flags);

	return pgcb;
}

static struct subsys *id_to_sys(unsigned int id)
{
	return id < NR_SYSS ? &syss[id] : NULL;
}

enum dbg_id {
	DBG_ID_MD1_BUS =		0,
	DBG_ID_CONN_BUS =		1,
	DBG_ID_MFG0_BUS =		2,
	DBG_ID_MFG1_BUS =		3,
	DBG_ID_MFG2_BUS =		4,
	DBG_ID_MFG3_BUS =		5,
	DBG_ID_MFG5_BUS =		6,
	DBG_ID_ISP_BUS =			7,
	DBG_ID_ISP2_BUS =		8,
	DBG_ID_IPE_BUS =			9,
	DBG_ID_VDE_BUS =		10,
	DBG_ID_VEN_BUS =		11,
	DBG_ID_DIS_BUS =		12,
	DBG_ID_AUDIO_BUS =		13,
	DBG_ID_ADSP_BUS =		14,
	DBG_ID_CAM_BUS =		15,
	DBG_ID_CAM_RAWA_BUS =		16,
	DBG_ID_CAM_RAWB_BUS =		17,
	DBG_ID_VPU_BUS =		18,
	DBG_ID_BUS_NUM =		19,

	DBG_ID_MD1_PWR =	DBG_ID_BUS_NUM + 0,
	DBG_ID_CONN_PWR =	DBG_ID_BUS_NUM + 1,
	DBG_ID_MFG0_PWR =	DBG_ID_BUS_NUM + 2,
	DBG_ID_MFG1_PWR =	DBG_ID_BUS_NUM + 3,
	DBG_ID_MFG2_PWR =	DBG_ID_BUS_NUM + 4,
	DBG_ID_MFG3_PWR =	DBG_ID_BUS_NUM + 5,
	DBG_ID_MFG5_PWR =	DBG_ID_BUS_NUM + 6,
	DBG_ID_ISP_PWR =	DBG_ID_BUS_NUM + 7,
	DBG_ID_ISP2_PWR =	DBG_ID_BUS_NUM + 8,
	DBG_ID_IPE_PWR =	DBG_ID_BUS_NUM + 9,
	DBG_ID_VDE_PWR =	DBG_ID_BUS_NUM + 10,
	DBG_ID_VEN_PWR =	DBG_ID_BUS_NUM + 11,
	DBG_ID_DIS_PWR =	DBG_ID_BUS_NUM + 12,
	DBG_ID_AUDIO_PWR =	DBG_ID_BUS_NUM + 13,
	DBG_ID_ADSP_PWR =	DBG_ID_BUS_NUM + 14,
	DBG_ID_CAM_PWR =	DBG_ID_BUS_NUM + 15,
	DBG_ID_CAM_RAWA_PWR =	DBG_ID_BUS_NUM + 16,
	DBG_ID_CAM_RAWB_PWR =	DBG_ID_BUS_NUM + 17,
	DBG_ID_VPU_PWR =	DBG_ID_BUS_NUM + 18,
	DG_ID_PWR_NUM,
	DBG_ID_NUM = DG_ID_PWR_NUM,
};

#define ID_MADK   0xFF000000
#define STA_MASK  0x00F00000
#define STEP_MASK 0x000000FF

#define INCREASE_STEPS \
	do { \
		DBG_STEP++; \
		first_enter = true; \
		loop_cnt = 0; \
		log_over_cnt = false; \
		log_timeout = false; \
		log_dump = false; \
	} while (0)

static int DBG_ID;
static int DBG_STA;
static int DBG_STEP;

static unsigned long long block_time;
static unsigned long long upd_block_time;
static u32 loop_cnt;
static bool log_over_cnt;
static bool log_timeout;
static bool log_dump;
static bool first_enter = true;

/*
 * ram console data0 define
 * [31:24] : DBG_ID
 * [23:20] : DBG_STA
 * [7:0] : DBG_STEP
 */
static void ram_console_update(void)
{
	unsigned long spinlock_save_flags;
	struct pg_callbacks *pgcb;
	u32 data[8] = {0x0};
	u32 i = 0;

	data[i] = ((DBG_ID << 24) & ID_MADK)
		| ((DBG_STA << 20) & STA_MASK)
		| (DBG_STEP & STEP_MASK);

	data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN);
	data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN_STA1);
	data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN_1);
	data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN_STA1_1);
	data[++i] = clk_readl(PWR_STATUS);
	data[++i] = clk_readl(PWR_STATUS_2ND);
	data[++i] = clk_readl(CAM_PWR_CON);

	if (first_enter) {
		first_enter = false;
		block_time = sched_clock();
	}
	upd_block_time = sched_clock();
	loop_cnt++;

	if (loop_cnt > 5000)
		log_over_cnt = true;

	if ((upd_block_time > 0  && block_time > 0)
			&& (upd_block_time > block_time)
			&& (upd_block_time - block_time > 5000000000))
		log_timeout = true;

	if ((log_over_cnt && !log_dump) || (log_over_cnt && log_timeout)) {
		pr_notice("%s: upd(%llu ns), ori(%llu ns)\n", __func__,
				upd_block_time, block_time);
		pr_notice("%s: over_cnt: %d, time_out: %d, log_dump: %d\n",
				__func__, log_over_cnt, log_timeout, log_dump);

		log_dump = true;

		print_enabled_clks_once();

		for (i = 0; i < ARRAY_SIZE(data); i++)
			pr_notice("%s: data[%i]=%08x\n", __func__, i, data[i]);

		/* The code based on  clkdbg/clkdbg-mt6873. */
		/* When power on/off fails, dump the related registers. */
		print_subsys_reg(topckgen);
		print_subsys_reg(infracfg_ao);
		print_subsys_reg(infracfg);
		print_subsys_reg(infracfg_dbg);
		print_subsys_reg(infrapdn_dbg);
		print_subsys_reg(scpsys);
		print_subsys_reg(apmixed);

		if (DBG_STA == STA_POWER_DOWN) {
			u32 id = DBG_ID;

			if (DBG_ID >= (DBG_ID_NUM / 2))
				id = DBG_ID - (DBG_ID_NUM / 2);
			/* dump only when power off failes */
			if (id == SYS_MFG0 || id == SYS_MFG1
			|| id == SYS_MFG2 || id == SYS_MFG3
			|| id == SYS_MFG5)
				print_subsys_reg(mfgsys);

			if (id == SYS_AUDIO) {
				print_subsys_reg(audio);
				print_subsys_reg(scpsys);
			}

			if (id == SYS_DIS)
				print_subsys_reg(mmsys);

			/* isp/img */
			if (id == SYS_ISP) {
				print_subsys_reg(mmsys);
				print_subsys_reg(img1sys);
			}

			if (id == SYS_ISP2) {
				print_subsys_reg(mmsys);
				print_subsys_reg(img2sys);
			}

			/* ipe */
			if (id == SYS_IPE) {
				print_subsys_reg(mmsys);
				print_subsys_reg(ipesys);
			}

			/* venc */
			if (id == SYS_VEN) {
				print_subsys_reg(mmsys);
				print_subsys_reg(vencsys);
			}

			/* vdec */
			if (id == SYS_VDE) {
				print_subsys_reg(mmsys);
				print_subsys_reg(vdecsys);
			}

			/* cam */
			if (id == SYS_CAM) {
				print_subsys_reg(mmsys);
				print_subsys_reg(camsys);
			}

			if (id == SYS_CAM_RAWA) {
				print_subsys_reg(mmsys);
				print_subsys_reg(camsys);
				print_subsys_reg(cam_rawa_sys);
			}

			if (id == SYS_CAM_RAWB) {
				print_subsys_reg(mmsys);
				print_subsys_reg(camsys);
				print_subsys_reg(cam_rawb_sys);
			}

			if (id == SYS_VPU) {
				print_subsys_reg(apu0);
				print_subsys_reg(apu1);
				print_subsys_reg(apuvc);
				print_subsys_reg(apuc);
			}
		}

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

		spin_lock_irqsave(&pgcb_lock, spinlock_save_flags);
		list_for_each_entry_reverse(pgcb, &pgcb_list, list) {
			if (pgcb->debug_dump)
				pgcb->debug_dump(DBG_ID);
		}
		spin_unlock_irqrestore(&pgcb_lock, spinlock_save_flags);
	}
#ifdef CONFIG_MTK_RAM_CONSOLE
	for (i = 0; i < ARRAY_SIZE(data); i++)
		aee_rr_rec_clk(i, data[i]);
	/*todo: add each domain's debug register to ram console*/
#endif

	if (log_over_cnt && log_timeout)
		BUG_ON(1);
}

#ifdef CONFIG_OF
static void __iomem *find_and_iomap(char *comp_str)
{
	void __iomem *ret;
	struct device_node *node = of_find_compatible_node(NULL, NULL,
								comp_str);

	if (!node) {
		pr_debug("[CCF] PG: find node %s failed\n", comp_str);
		return NULL;
	}
	ret = of_iomap(node, 0);
	if (!ret) {
		pr_debug("[CCF] iomap base %s failed\n", comp_str);
		return NULL;
	}
	return ret;
}

static void iomap_apu(void)
{
	apu_vcore_base = find_and_iomap("mediatek,apu_vcore");
	if (!apu_vcore_base) {
		pr_notice("cannot get apu vcore base\n");

		return;
	}

	apu_conn_base = find_and_iomap("mediatek,apu_conn");
	if (!apu_conn_base) {
		pr_notice("cannot get apu conn base\n");

		return;
	}
}
#endif

static void enable_subsys_hwcg(enum subsys_id id)
{
	if (id == SYS_VPU) {
		clk_writel(APU_VCORE_CG_CLR, 0xFFFFFFFF);
		clk_writel(APU_CONN_CG_CLR, 0xFFFFFFFF);
	}
}

/* auto-gen begin*/
int spm_mtcmos_ctrl_md1_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MD1_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET,
				MD1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
				& MD1_PROT_STEP1_0_ACK_MASK)
				!= MD1_PROT_STEP1_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_VDNR_SET,
				MD1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_VDNR_STA1)
				& MD1_PROT_STEP2_0_ACK_MASK)
				!= MD1_PROT_STEP2_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set MD1 bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_VDNR_CLR,
				MD1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR,
				MD1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Finish to release MD1 bus protect" */
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

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MD1" */
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MD1_PWR_CON,
			spm_read(MD1_PWR_CON) & ~PWR_ON);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MD1_PWR_STA_MASK = 0" */
		while (spm_read(PWR_STATUS) & MD1_PWR_STA_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="MD_EXT_BUCK_ISO_CON[0]=1"*/
		spm_write(MD_EXT_BUCK_ISO_CON,
			spm_read(MD_EXT_BUCK_ISO_CON) | (0x1 << 0));
		/* TINFO="MD_EXT_BUCK_ISO_CON[1]=1"*/
		spm_write(MD_EXT_BUCK_ISO_CON,
			spm_read(MD_EXT_BUCK_ISO_CON) | (0x1 << 1));
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MD1_PWR_CON,
			spm_read(MD1_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Finish to turn off MD1" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MD1" */
		/* TINFO="MD_EXT_BUCK_ISO_CON[0]=0"*/
		spm_write(MD_EXT_BUCK_ISO_CON,
			spm_read(MD_EXT_BUCK_ISO_CON) & ~(0x1 << 0));
		/* TINFO="MD_EXT_BUCK_ISO_CON[1]=0"*/
		spm_write(MD_EXT_BUCK_ISO_CON,
			spm_read(MD_EXT_BUCK_ISO_CON) & ~(0x1 << 1));
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MD1_PWR_CON,
			spm_read(MD1_PWR_CON) | PWR_RST_B);
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MD1_PWR_CON,
			spm_read(MD1_PWR_CON) | PWR_ON);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MD1_PWR_STA_MASK = 1" */
		while ((spm_read(PWR_STATUS) & MD1_PWR_STA_MASK)
				!= MD1_PWR_STA_MASK) {
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn on MD1" */
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

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET,
				CONN_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
				& CONN_PROT_STEP1_0_ACK_MASK)
				!= CONN_PROT_STEP1_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET,
				CONN_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
				& CONN_PROT_STEP2_0_ACK_MASK)
				!= CONN_PROT_STEP2_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET,
				CONN_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1)
				& CONN_PROT_STEP2_1_ACK_MASK)
				!= CONN_PROT_STEP2_1_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
#ifndef IGNORE_MTCMOS_CHECK
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set CONN bus protect" */
	} else {    /* STA_RELEASE_BUS */
#ifndef IGNORE_MTCMOS_CHECK
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR,
				CONN_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR,
				CONN_PROT_STEP2_1_MASK);
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
		/* TINFO="Finish to release CONN bus protect" */
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

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off CONN" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(CONN_PWR_CON,
			spm_read(CONN_PWR_CON) | PWR_ISO);
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
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off CONN" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on CONN" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(CONN_PWR_CON,
			spm_read(CONN_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(CONN_PWR_CON,
			spm_read(CONN_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & CONN_PWR_STA_MASK)
				!= CONN_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & CONN_PWR_STA_MASK)
				!= CONN_PWR_STA_MASK))
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(CONN_PWR_CON,
			spm_read(CONN_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(CONN_PWR_CON,
			spm_read(CONN_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(CONN_PWR_CON,
			spm_read(CONN_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on CONN" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_mfg0_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG0_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MFG0_PWR_CON,
			spm_read(MFG0_PWR_CON) | MFG0_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG0_SRAM_PDN_ACK = 1" */
		while ((spm_read(MFG0_PWR_CON) & MFG0_SRAM_PDN_ACK)
				!= MFG0_SRAM_PDN_ACK) {
			/* Need f_fmfg_core_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set MFG0 bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MFG0_PWR_CON,
			spm_read(MFG0_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG0_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(MFG0_PWR_CON) & MFG0_SRAM_PDN_ACK_BIT0) {
			/* Need f_fmfg_core_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to release MFG0 bus protect" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_mfg0_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG0_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG0" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MFG0_PWR_CON,
			spm_read(MFG0_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MFG0_PWR_CON,
			spm_read(MFG0_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MFG0_PWR_CON,
			spm_read(MFG0_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MFG0_PWR_CON,
			spm_read(MFG0_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MFG0_PWR_CON,
			spm_read(MFG0_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MFG0_PWR_STA_MASK)
			|| (spm_read(PWR_STATUS_2ND) & MFG0_PWR_STA_MASK)) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off MFG0" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MFG0" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MFG0_PWR_CON,
			spm_read(MFG0_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MFG0_PWR_CON,
			spm_read(MFG0_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & MFG0_PWR_STA_MASK)
				!= MFG0_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & MFG0_PWR_STA_MASK)
				!= MFG0_PWR_STA_MASK))
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG0_PWR_CON,
			spm_read(MFG0_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG0_PWR_CON,
			spm_read(MFG0_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG0_PWR_CON,
			spm_read(MFG0_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on MFG0" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_mfg1_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG1_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set way_en=0" */
		spm_write(INFRA_PDN_MFG1_WAY_EN,
				MFG1_PROT_STEP0_WAY_EN_OFF_MASK);
		while ((spm_read(INFRA_PDN_MFG1_WAY_EN)
				& MFG1_PROT_STEP0_WAY_EN_OFF_MASK)
				!= MFG1_PROT_STEP0_WAY_EN_OFF_MASK_ACK)
			ram_console_update();

		INCREASE_STEPS;
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET,
				MFG1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1)
				& MFG1_PROT_STEP1_0_ACK_MASK)
				!= MFG1_PROT_STEP1_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_SET,
				MFG1_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2)
				& MFG1_PROT_STEP1_1_ACK_MASK)
				!= MFG1_PROT_STEP1_1_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET,
				MFG1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
				& MFG1_PROT_STEP2_0_ACK_MASK)
				!= MFG1_PROT_STEP2_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_SET,
				MFG1_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2)
				& MFG1_PROT_STEP2_1_ACK_MASK)
				!= MFG1_PROT_STEP2_1_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MFG1_PWR_CON,
			spm_read(MFG1_PWR_CON) | MFG1_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG1_SRAM_PDN_ACK = 1" */
		while ((spm_read(MFG1_PWR_CON) & MFG1_SRAM_PDN_ACK)
				!= MFG1_SRAM_PDN_ACK) {
			/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set MFG1 bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MFG1_PWR_CON,
			spm_read(MFG1_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG1_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(MFG1_PWR_CON) & MFG1_SRAM_PDN_ACK_BIT0) {
			/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to release MFG1 bus protect" */
		/* TINFO="Set way_en=1" */
		spm_write(INFRA_PDN_MFG1_WAY_EN,
				MFG1_PROT_STEP0_WAY_EN_ON_MASK);
		while ((spm_read(INFRA_PDN_MFG1_WAY_EN)
				& MFG1_PROT_STEP0_WAY_EN_ON_MASK)
				!= MFG1_PROT_STEP0_WAY_EN_ON_MASK_ACK)
			ram_console_update();

		INCREASE_STEPS;
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR,
				MFG1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_CLR,
				MFG1_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR,
				MFG1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_CLR,
				MFG1_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_mfg1_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG1_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG1" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MFG1_PWR_CON,
			spm_read(MFG1_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MFG1_PWR_CON,
			spm_read(MFG1_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MFG1_PWR_CON,
			spm_read(MFG1_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MFG1_PWR_CON,
			spm_read(MFG1_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MFG1_PWR_CON,
			spm_read(MFG1_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MFG1_PWR_STA_MASK)
			|| (spm_read(PWR_STATUS_2ND) & MFG1_PWR_STA_MASK)) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off MFG1" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MFG1" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MFG1_PWR_CON,
			spm_read(MFG1_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MFG1_PWR_CON,
			spm_read(MFG1_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & MFG1_PWR_STA_MASK)
				!= MFG1_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & MFG1_PWR_STA_MASK)
				!= MFG1_PWR_STA_MASK))
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG1_PWR_CON,
			spm_read(MFG1_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG1_PWR_CON,
			spm_read(MFG1_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG1_PWR_CON,
			spm_read(MFG1_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on MFG1" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_mfg2_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG2_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MFG2_PWR_CON,
			spm_read(MFG2_PWR_CON) | MFG2_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG2_SRAM_PDN_ACK = 1" */
		while ((spm_read(MFG2_PWR_CON) & MFG2_SRAM_PDN_ACK)
				!= MFG2_SRAM_PDN_ACK) {
			/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set MFG2 bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MFG2_PWR_CON,
			spm_read(MFG2_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG2_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(MFG2_PWR_CON) & MFG2_SRAM_PDN_ACK_BIT0) {
			/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to release MFG2 bus protect" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_mfg2_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG2_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG2" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MFG2_PWR_CON,
			spm_read(MFG2_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MFG2_PWR_CON,
			spm_read(MFG2_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MFG2_PWR_CON,
			spm_read(MFG2_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MFG2_PWR_CON,
			spm_read(MFG2_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MFG2_PWR_CON,
			spm_read(MFG2_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MFG2_PWR_STA_MASK)
			|| (spm_read(PWR_STATUS_2ND) & MFG2_PWR_STA_MASK)) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off MFG2" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MFG2" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MFG2_PWR_CON,
			spm_read(MFG2_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MFG2_PWR_CON,
			spm_read(MFG2_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & MFG2_PWR_STA_MASK)
				!= MFG2_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & MFG2_PWR_STA_MASK)
				!= MFG2_PWR_STA_MASK))
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG2_PWR_CON,
			spm_read(MFG2_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG2_PWR_CON,
			spm_read(MFG2_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG2_PWR_CON,
			spm_read(MFG2_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on MFG2" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_mfg3_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG3_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MFG3_PWR_CON,
			spm_read(MFG3_PWR_CON) | MFG3_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG3_SRAM_PDN_ACK = 1" */
		while ((spm_read(MFG3_PWR_CON) & MFG3_SRAM_PDN_ACK)
				!= MFG3_SRAM_PDN_ACK) {
			/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set MFG3 bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MFG3_PWR_CON,
			spm_read(MFG3_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG3_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(MFG3_PWR_CON) & MFG3_SRAM_PDN_ACK_BIT0) {
			/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to release MFG3 bus protect" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_mfg3_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG3_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG3" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MFG3_PWR_CON,
			spm_read(MFG3_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MFG3_PWR_CON,
			spm_read(MFG3_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MFG3_PWR_CON,
			spm_read(MFG3_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MFG3_PWR_CON,
			spm_read(MFG3_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MFG3_PWR_CON,
			spm_read(MFG3_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MFG3_PWR_STA_MASK)
			|| (spm_read(PWR_STATUS_2ND) & MFG3_PWR_STA_MASK)) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off MFG3" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MFG3" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MFG3_PWR_CON,
			spm_read(MFG3_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MFG3_PWR_CON,
			spm_read(MFG3_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & MFG3_PWR_STA_MASK)
				!= MFG3_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & MFG3_PWR_STA_MASK)
				!= MFG3_PWR_STA_MASK))
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG3_PWR_CON,
			spm_read(MFG3_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG3_PWR_CON,
			spm_read(MFG3_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG3_PWR_CON,
			spm_read(MFG3_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on MFG3" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_mfg5_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG5_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MFG5_PWR_CON,
			spm_read(MFG5_PWR_CON) | MFG5_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG5_SRAM_PDN_ACK = 1" */
		while ((spm_read(MFG5_PWR_CON) & MFG5_SRAM_PDN_ACK)
				!= MFG5_SRAM_PDN_ACK) {
			/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set MFG5 bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MFG5_PWR_CON,
			spm_read(MFG5_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG5_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(MFG5_PWR_CON) & MFG5_SRAM_PDN_ACK_BIT0) {
			/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to release MFG5 bus protect" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_mfg5_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG5_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG5" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MFG5_PWR_CON,
			spm_read(MFG5_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MFG5_PWR_CON,
			spm_read(MFG5_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MFG5_PWR_CON,
			spm_read(MFG5_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MFG5_PWR_CON,
			spm_read(MFG5_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MFG5_PWR_CON,
			spm_read(MFG5_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MFG5_PWR_STA_MASK)
			|| (spm_read(PWR_STATUS_2ND) & MFG5_PWR_STA_MASK)) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off MFG5" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MFG5" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MFG5_PWR_CON,
			spm_read(MFG5_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MFG5_PWR_CON,
			spm_read(MFG5_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & MFG5_PWR_STA_MASK)
				!= MFG5_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & MFG5_PWR_STA_MASK)
				!= MFG5_PWR_STA_MASK))
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG5_PWR_CON,
			spm_read(MFG5_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG5_PWR_CON,
			spm_read(MFG5_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG5_PWR_CON,
			spm_read(MFG5_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on MFG5" */
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

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_2_SET,
				ISP_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_2_STA1)
				& ISP_PROT_STEP1_0_ACK_MASK)
				!= ISP_PROT_STEP1_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_2_SET,
				ISP_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_2_STA1)
				& ISP_PROT_STEP2_0_ACK_MASK)
				!= ISP_PROT_STEP2_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(ISP_PWR_CON,
			spm_read(ISP_PWR_CON) | ISP_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ISP_SRAM_PDN_ACK = 1" */
		while ((spm_read(ISP_PWR_CON) & ISP_SRAM_PDN_ACK)
				!= ISP_SRAM_PDN_ACK) {
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set ISP bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(ISP_PWR_CON,
			spm_read(ISP_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ISP_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(ISP_PWR_CON) & ISP_SRAM_PDN_ACK_BIT0) {
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_2_CLR,
				ISP_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_2_CLR,
				ISP_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Finish to release ISP bus protect" */
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

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off ISP" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(ISP_PWR_CON,
			spm_read(ISP_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(ISP_PWR_CON,
			spm_read(ISP_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(ISP_PWR_CON,
			spm_read(ISP_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(ISP_PWR_CON,
			spm_read(ISP_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(ISP_PWR_CON,
			spm_read(ISP_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & ISP_PWR_STA_MASK)
			|| (spm_read(PWR_STATUS_2ND) & ISP_PWR_STA_MASK)) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off ISP" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on ISP" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(ISP_PWR_CON,
			spm_read(ISP_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(ISP_PWR_CON,
			spm_read(ISP_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & ISP_PWR_STA_MASK)
				!= ISP_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & ISP_PWR_STA_MASK)
				!= ISP_PWR_STA_MASK))
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(ISP_PWR_CON,
			spm_read(ISP_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(ISP_PWR_CON,
			spm_read(ISP_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(ISP_PWR_CON,
			spm_read(ISP_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on ISP" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_isp2_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_ISP2_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(ISP2_PWR_CON,
			spm_read(ISP2_PWR_CON) | ISP2_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ISP2_SRAM_PDN_ACK = 1" */
		while ((spm_read(ISP2_PWR_CON) & ISP2_SRAM_PDN_ACK)
				!= ISP2_SRAM_PDN_ACK) {
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set ISP2 bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(ISP2_PWR_CON,
			spm_read(ISP2_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ISP2_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(ISP2_PWR_CON) & ISP2_SRAM_PDN_ACK_BIT0) {
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to release ISP2 bus protect" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_isp2_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_ISP2_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off ISP2" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(ISP2_PWR_CON,
			spm_read(ISP2_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(ISP2_PWR_CON,
			spm_read(ISP2_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(ISP2_PWR_CON,
			spm_read(ISP2_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(ISP2_PWR_CON,
			spm_read(ISP2_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(ISP2_PWR_CON,
			spm_read(ISP2_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & ISP2_PWR_STA_MASK)
			|| (spm_read(PWR_STATUS_2ND) & ISP2_PWR_STA_MASK)) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off ISP2" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on ISP2" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(ISP2_PWR_CON,
			spm_read(ISP2_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(ISP2_PWR_CON,
			spm_read(ISP2_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & ISP2_PWR_STA_MASK)
				!= ISP2_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & ISP2_PWR_STA_MASK)
				!= ISP2_PWR_STA_MASK))
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(ISP2_PWR_CON,
			spm_read(ISP2_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(ISP2_PWR_CON,
			spm_read(ISP2_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(ISP2_PWR_CON,
			spm_read(ISP2_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on ISP2" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_ipe_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_IPE_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
				IPE_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1)
				& IPE_PROT_STEP1_0_ACK_MASK)
				!= IPE_PROT_STEP1_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
				IPE_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1)
				& IPE_PROT_STEP2_0_ACK_MASK)
				!= IPE_PROT_STEP2_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(IPE_PWR_CON,
			spm_read(IPE_PWR_CON) | IPE_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until IPE_SRAM_PDN_ACK = 1" */
		while ((spm_read(IPE_PWR_CON) & IPE_SRAM_PDN_ACK)
				!= IPE_SRAM_PDN_ACK) {
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set IPE bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(IPE_PWR_CON,
			spm_read(IPE_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until IPE_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(IPE_PWR_CON) & IPE_SRAM_PDN_ACK_BIT0) {
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
				IPE_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
				IPE_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Finish to release IPE bus protect" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_ipe_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_IPE_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off IPE" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(IPE_PWR_CON,
			spm_read(IPE_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(IPE_PWR_CON,
			spm_read(IPE_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(IPE_PWR_CON,
			spm_read(IPE_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(IPE_PWR_CON,
			spm_read(IPE_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(IPE_PWR_CON,
			spm_read(IPE_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & IPE_PWR_STA_MASK)
			|| (spm_read(PWR_STATUS_2ND) & IPE_PWR_STA_MASK)) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off IPE" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on IPE" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(IPE_PWR_CON,
			spm_read(IPE_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(IPE_PWR_CON,
			spm_read(IPE_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & IPE_PWR_STA_MASK)
				!= IPE_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & IPE_PWR_STA_MASK)
				!= IPE_PWR_STA_MASK))
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(IPE_PWR_CON,
			spm_read(IPE_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(IPE_PWR_CON,
			spm_read(IPE_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(IPE_PWR_CON,
			spm_read(IPE_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on IPE" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_vde_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_VDE_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
				VDE_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1)
				& VDE_PROT_STEP1_0_ACK_MASK)
				!= VDE_PROT_STEP1_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
				VDE_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1)
				& VDE_PROT_STEP2_0_ACK_MASK)
				!= VDE_PROT_STEP2_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(VDE_PWR_CON,
			spm_read(VDE_PWR_CON) | VDE_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VDE_SRAM_PDN_ACK = 1" */
		while ((spm_read(VDE_PWR_CON) & VDE_SRAM_PDN_ACK)
				!= VDE_SRAM_PDN_ACK) {
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set VDE bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(VDE_PWR_CON,
			spm_read(VDE_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VDE_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(VDE_PWR_CON) & VDE_SRAM_PDN_ACK_BIT0) {
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
				VDE_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
				VDE_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Finish to release VDE bus protect" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_vde_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_VDE_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VDE" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(VDE_PWR_CON,
			spm_read(VDE_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(VDE_PWR_CON,
			spm_read(VDE_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(VDE_PWR_CON,
			spm_read(VDE_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(VDE_PWR_CON,
			spm_read(VDE_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(VDE_PWR_CON,
			spm_read(VDE_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & VDE_PWR_STA_MASK)
			|| (spm_read(PWR_STATUS_2ND) & VDE_PWR_STA_MASK)) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off VDE" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on VDE" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(VDE_PWR_CON,
			spm_read(VDE_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(VDE_PWR_CON,
			spm_read(VDE_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & VDE_PWR_STA_MASK)
				!= VDE_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & VDE_PWR_STA_MASK)
				!= VDE_PWR_STA_MASK))
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(VDE_PWR_CON,
			spm_read(VDE_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(VDE_PWR_CON,
			spm_read(VDE_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(VDE_PWR_CON,
			spm_read(VDE_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on VDE" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_ven_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_VEN_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
				VEN_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1)
				& VEN_PROT_STEP1_0_ACK_MASK)
				!= VEN_PROT_STEP1_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
				VEN_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1)
				& VEN_PROT_STEP2_0_ACK_MASK)
				!= VEN_PROT_STEP2_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(VEN_PWR_CON,
			spm_read(VEN_PWR_CON) | VEN_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VEN_SRAM_PDN_ACK = 1" */
		while ((spm_read(VEN_PWR_CON) & VEN_SRAM_PDN_ACK)
				!= VEN_SRAM_PDN_ACK) {
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set VEN bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(VEN_PWR_CON,
			spm_read(VEN_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VEN_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(VEN_PWR_CON) & VEN_SRAM_PDN_ACK_BIT0) {
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
				VEN_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
				VEN_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Finish to release VEN bus protect" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_ven_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_VEN_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VEN" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(VEN_PWR_CON,
			spm_read(VEN_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(VEN_PWR_CON,
			spm_read(VEN_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(VEN_PWR_CON,
			spm_read(VEN_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(VEN_PWR_CON,
			spm_read(VEN_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(VEN_PWR_CON,
			spm_read(VEN_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & VEN_PWR_STA_MASK)
			|| (spm_read(PWR_STATUS_2ND) & VEN_PWR_STA_MASK)) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off VEN" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on VEN" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(VEN_PWR_CON,
			spm_read(VEN_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(VEN_PWR_CON,
			spm_read(VEN_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & VEN_PWR_STA_MASK)
				!= VEN_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & VEN_PWR_STA_MASK)
				!= VEN_PWR_STA_MASK))
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(VEN_PWR_CON,
			spm_read(VEN_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(VEN_PWR_CON,
			spm_read(VEN_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(VEN_PWR_CON,
			spm_read(VEN_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on VEN" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_dis_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_DIS_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
				DIS_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1)
				& DIS_PROT_STEP1_0_ACK_MASK)
				!= DIS_PROT_STEP1_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_2_SET,
				DIS_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_2_STA1)
				& DIS_PROT_STEP1_1_ACK_MASK)
				!= DIS_PROT_STEP1_1_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET,
				DIS_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
				& DIS_PROT_STEP2_0_ACK_MASK)
				!= DIS_PROT_STEP2_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
				DIS_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1)
				& DIS_PROT_STEP2_1_ACK_MASK)
				!= DIS_PROT_STEP2_1_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 2" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_2_SET,
				DIS_PROT_STEP2_2_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_2_STA1)
				& DIS_PROT_STEP2_2_ACK_MASK)
				!= DIS_PROT_STEP2_2_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(DIS_PWR_CON,
			spm_read(DIS_PWR_CON) | DIS_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DIS_SRAM_PDN_ACK = 1" */
		while ((spm_read(DIS_PWR_CON) & DIS_SRAM_PDN_ACK)
				!= DIS_SRAM_PDN_ACK) {
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set DIS bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(DIS_PWR_CON,
			spm_read(DIS_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DIS_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(DIS_PWR_CON) & DIS_SRAM_PDN_ACK_BIT0) {
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR,
				DIS_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
				DIS_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step2 : 2" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_2_CLR,
				DIS_PROT_STEP2_2_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
				DIS_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_2_CLR,
				DIS_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Finish to release DIS bus protect" */
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

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off DIS" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(DIS_PWR_CON,
			spm_read(DIS_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(DIS_PWR_CON,
			spm_read(DIS_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(DIS_PWR_CON,
			spm_read(DIS_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(DIS_PWR_CON,
			spm_read(DIS_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(DIS_PWR_CON,
			spm_read(DIS_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & DIS_PWR_STA_MASK)
			|| (spm_read(PWR_STATUS_2ND) & DIS_PWR_STA_MASK)) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off DIS" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on DIS" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(DIS_PWR_CON,
			spm_read(DIS_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(DIS_PWR_CON,
			spm_read(DIS_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & DIS_PWR_STA_MASK)
				!= DIS_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & DIS_PWR_STA_MASK)
				!= DIS_PWR_STA_MASK))
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(DIS_PWR_CON,
			spm_read(DIS_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(DIS_PWR_CON,
			spm_read(DIS_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(DIS_PWR_CON,
			spm_read(DIS_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on DIS" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_audio_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_AUDIO_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_SET,
				AUDIO_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2)
				& AUDIO_PROT_STEP1_0_ACK_MASK)
				!= AUDIO_PROT_STEP1_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(AUDIO_PWR_CON,
			spm_read(AUDIO_PWR_CON) | AUDIO_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until AUDIO_SRAM_PDN_ACK = 1" */
		while ((spm_read(AUDIO_PWR_CON) & AUDIO_SRAM_PDN_ACK)
				!= AUDIO_SRAM_PDN_ACK) {
			/* Need f_f26M_aud_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set AUDIO bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(AUDIO_PWR_CON,
			spm_read(AUDIO_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until AUDIO_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(AUDIO_PWR_CON) & AUDIO_SRAM_PDN_ACK_BIT0) {
			/* Need f_f26M_aud_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_CLR,
				AUDIO_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Finish to release AUDIO bus protect" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_audio_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_AUDIO_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off AUDIO" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(AUDIO_PWR_CON,
			spm_read(AUDIO_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(AUDIO_PWR_CON,
			spm_read(AUDIO_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(AUDIO_PWR_CON,
			spm_read(AUDIO_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(AUDIO_PWR_CON,
			spm_read(AUDIO_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(AUDIO_PWR_CON,
			spm_read(AUDIO_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & AUDIO_PWR_STA_MASK)
			|| (spm_read(PWR_STATUS_2ND) & AUDIO_PWR_STA_MASK)) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off AUDIO" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on AUDIO" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(AUDIO_PWR_CON,
			spm_read(AUDIO_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(AUDIO_PWR_CON,
			spm_read(AUDIO_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & AUDIO_PWR_STA_MASK)
				!= AUDIO_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & AUDIO_PWR_STA_MASK)
				!= AUDIO_PWR_STA_MASK))
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(AUDIO_PWR_CON,
			spm_read(AUDIO_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(AUDIO_PWR_CON,
			spm_read(AUDIO_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(AUDIO_PWR_CON,
			spm_read(AUDIO_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on AUDIO" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_adsp_dormant_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_ADSP_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_SET,
				ADSP_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2)
				& ADSP_PROT_STEP1_0_ACK_MASK)
				!= ADSP_PROT_STEP1_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_SLEEP_B = 0" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) & ~ADSP_SRAM_SLEEP_B);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ADSP_SRAM_SLEEP_B_ACK = 0" */
		while (spm_read(ADSP_PWR_CON) & ADSP_SRAM_SLEEP_B_ACK) {
			/* n/a */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set ADSP bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Set SRAM_SLEEP_B = 1" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) | (0x1 << 9));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ADSP_SRAM_SLEEP_B_ACK_BIT0 = 1" */
		while ((spm_read(ADSP_PWR_CON) & ADSP_SRAM_SLEEP_B_ACK_BIT0)
				!= ADSP_SRAM_SLEEP_B_ACK_BIT0) {
			/* n/a */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_CLR,
				ADSP_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Finish to release ADSP bus protect" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_adsp_dormant_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_ADSP_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off ADSP" */
		/* TINFO="Set SRAM_CKISO = 1" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) | SRAM_CKISO);
		/* TINFO="Set SRAM_ISOINT_B = 0" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) & ~SRAM_ISOINT_B);
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & ADSP_PWR_STA_MASK)
			|| (spm_read(PWR_STATUS_2ND) & ADSP_PWR_STA_MASK)) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off ADSP" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on ADSP" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & ADSP_PWR_STA_MASK)
				!= ADSP_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & ADSP_PWR_STA_MASK)
				!= ADSP_PWR_STA_MASK))
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) | PWR_RST_B);
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_ISOINT_B = 1" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) | SRAM_ISOINT_B);
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_CKISO = 0" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) & ~SRAM_CKISO);
		/* TINFO="Finish to turn on ADSP" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_adsp_shut_down_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_ADSP_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_SET,
				ADSP_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2)
				& ADSP_PROT_STEP1_0_ACK_MASK)
				!= ADSP_PROT_STEP1_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_SLEEP_B = 0" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) & ~ADSP_SRAM_SLEEP_B);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ADSP_SRAM_SLEEP_B_ACK = 0" */
		while (spm_read(ADSP_PWR_CON) & ADSP_SRAM_SLEEP_B_ACK) {
			/* n/a */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_SET,
				ADSP_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2)
				& ADSP_PROT_STEP1_0_ACK_MASK)
				!= ADSP_PROT_STEP1_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) | ADSP_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ADSP_SRAM_PDN_ACK = 1" */
		while ((spm_read(ADSP_PWR_CON) & ADSP_SRAM_PDN_ACK)
				!= ADSP_SRAM_PDN_ACK) {
			/* Need f_f26M_aud_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set ADSP bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Set SRAM_SLEEP_B = 1" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) | (0x1 << 9));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ADSP_SRAM_SLEEP_B_ACK_BIT0 = 1" */
		while ((spm_read(ADSP_PWR_CON) & ADSP_SRAM_SLEEP_B_ACK_BIT0)
				!= ADSP_SRAM_SLEEP_B_ACK_BIT0) {
			/* n/a */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_CLR,
				ADSP_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ADSP_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(ADSP_PWR_CON) & ADSP_SRAM_PDN_ACK_BIT0) {
			/* Need f_f26M_aud_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_CLR,
				ADSP_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Finish to release ADSP bus protect" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_adsp_shut_down_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_ADSP_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off ADSP" */
		/* TINFO="Set SRAM_CKISO = 1" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) | SRAM_CKISO);
		/* TINFO="Set SRAM_ISOINT_B = 0" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) & ~SRAM_ISOINT_B);
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & ADSP_PWR_STA_MASK)
			|| (spm_read(PWR_STATUS_2ND) & ADSP_PWR_STA_MASK)) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off ADSP" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on ADSP" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & ADSP_PWR_STA_MASK)
				!= ADSP_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & ADSP_PWR_STA_MASK)
				!= ADSP_PWR_STA_MASK))
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) | PWR_RST_B);
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_ISOINT_B = 1" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) | SRAM_ISOINT_B);
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_CKISO = 0" */
		spm_write(ADSP_PWR_CON,
			spm_read(ADSP_PWR_CON) & ~SRAM_CKISO);
		/* TINFO="Finish to turn on ADSP" */
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

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_SET,
				CAM_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2)
				& CAM_PROT_STEP1_0_ACK_MASK)
				!= CAM_PROT_STEP1_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
				CAM_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1)
				& CAM_PROT_STEP1_1_ACK_MASK)
				!= CAM_PROT_STEP1_1_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET,
				CAM_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1)
				& CAM_PROT_STEP2_0_ACK_MASK)
				!= CAM_PROT_STEP2_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
				CAM_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1)
				& CAM_PROT_STEP2_1_ACK_MASK)
				!= CAM_PROT_STEP2_1_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 2" */
		spm_write(INFRA_TOPAXI_PROTECTEN_VDNR_SET,
				CAM_PROT_STEP2_2_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_VDNR_STA1)
				& CAM_PROT_STEP2_2_ACK_MASK)
				!= CAM_PROT_STEP2_2_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(CAM_PWR_CON,
			spm_read(CAM_PWR_CON) | CAM_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until CAM_SRAM_PDN_ACK = 1" */
		while ((spm_read(CAM_PWR_CON) & CAM_SRAM_PDN_ACK)
				!= CAM_SRAM_PDN_ACK) {
			/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set CAM bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(CAM_PWR_CON,
			spm_read(CAM_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until CAM_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(CAM_PWR_CON) & CAM_SRAM_PDN_ACK_BIT0) {
			/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR,
				CAM_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
				CAM_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step2 : 2" */
		spm_write(INFRA_TOPAXI_PROTECTEN_VDNR_CLR,
				CAM_PROT_STEP2_2_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_CLR,
				CAM_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
				CAM_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Finish to release CAM bus protect" */
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

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off CAM" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(CAM_PWR_CON,
			spm_read(CAM_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(CAM_PWR_CON,
			spm_read(CAM_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(CAM_PWR_CON,
			spm_read(CAM_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(CAM_PWR_CON,
			spm_read(CAM_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(CAM_PWR_CON,
			spm_read(CAM_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & CAM_PWR_STA_MASK)
			|| (spm_read(PWR_STATUS_2ND) & CAM_PWR_STA_MASK)) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off CAM" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on CAM" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(CAM_PWR_CON,
			spm_read(CAM_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(CAM_PWR_CON,
			spm_read(CAM_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & CAM_PWR_STA_MASK)
				!= CAM_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & CAM_PWR_STA_MASK)
				!= CAM_PWR_STA_MASK))
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(CAM_PWR_CON,
			spm_read(CAM_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(CAM_PWR_CON,
			spm_read(CAM_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(CAM_PWR_CON,
			spm_read(CAM_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on CAM" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_cam_rawa_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_CAM_RAWA_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(CAM_RAWA_PWR_CON,
			spm_read(CAM_RAWA_PWR_CON) | CAM_RAWA_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until CAM_RAWA_SRAM_PDN_ACK = 1" */
		while ((spm_read(CAM_RAWA_PWR_CON) & CAM_RAWA_SRAM_PDN_ACK)
				!= CAM_RAWA_SRAM_PDN_ACK) {
			/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set CAM_RAWA bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(CAM_RAWA_PWR_CON,
			spm_read(CAM_RAWA_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until CAM_RAWA_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(CAM_RAWA_PWR_CON) & CAM_RAWA_SRAM_PDN_ACK_BIT0)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Finish to release CAM_RAWA bus protect" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_cam_rawa_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_CAM_RAWA_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off CAM_RAWA" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(CAM_RAWA_PWR_CON,
			spm_read(CAM_RAWA_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(CAM_RAWA_PWR_CON,
			spm_read(CAM_RAWA_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(CAM_RAWA_PWR_CON,
			spm_read(CAM_RAWA_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(CAM_RAWA_PWR_CON,
			spm_read(CAM_RAWA_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(CAM_RAWA_PWR_CON,
			spm_read(CAM_RAWA_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & CAM_RAWA_PWR_STA_MASK)
			|| (spm_read(PWR_STATUS_2ND) & CAM_RAWA_PWR_STA_MASK)) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off CAM_RAWA" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on CAM_RAWA" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(CAM_RAWA_PWR_CON,
			spm_read(CAM_RAWA_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(CAM_RAWA_PWR_CON,
			spm_read(CAM_RAWA_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & CAM_RAWA_PWR_STA_MASK)
				!= CAM_RAWA_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & CAM_RAWA_PWR_STA_MASK)
				!= CAM_RAWA_PWR_STA_MASK))
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(CAM_RAWA_PWR_CON,
			spm_read(CAM_RAWA_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(CAM_RAWA_PWR_CON,
			spm_read(CAM_RAWA_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(CAM_RAWA_PWR_CON,
			spm_read(CAM_RAWA_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on CAM_RAWA" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_cam_rawb_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_CAM_RAWB_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(CAM_RAWB_PWR_CON,
			spm_read(CAM_RAWB_PWR_CON) | CAM_RAWB_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until CAM_RAWB_SRAM_PDN_ACK = 1" */
		while ((spm_read(CAM_RAWB_PWR_CON) & CAM_RAWB_SRAM_PDN_ACK)
				!= CAM_RAWB_SRAM_PDN_ACK) {
			/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set CAM_RAWB bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(CAM_RAWB_PWR_CON,
			spm_read(CAM_RAWB_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until CAM_RAWB_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(CAM_RAWB_PWR_CON) & CAM_RAWB_SRAM_PDN_ACK_BIT0)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Finish to release CAM_RAWB bus protect" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_cam_rawb_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_CAM_RAWB_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off CAM_RAWB" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(CAM_RAWB_PWR_CON,
			spm_read(CAM_RAWB_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(CAM_RAWB_PWR_CON,
			spm_read(CAM_RAWB_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(CAM_RAWB_PWR_CON,
			spm_read(CAM_RAWB_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(CAM_RAWB_PWR_CON,
			spm_read(CAM_RAWB_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(CAM_RAWB_PWR_CON,
			spm_read(CAM_RAWB_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & CAM_RAWB_PWR_STA_MASK)
			|| (spm_read(PWR_STATUS_2ND) & CAM_RAWB_PWR_STA_MASK)) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off CAM_RAWB" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on CAM_RAWB" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(CAM_RAWB_PWR_CON,
			spm_read(CAM_RAWB_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(CAM_RAWB_PWR_CON,
			spm_read(CAM_RAWB_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & CAM_RAWB_PWR_STA_MASK)
				!= CAM_RAWB_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & CAM_RAWB_PWR_STA_MASK)
				!= CAM_RAWB_PWR_STA_MASK))
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(CAM_RAWB_PWR_CON,
			spm_read(CAM_RAWB_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(CAM_RAWB_PWR_CON,
			spm_read(CAM_RAWB_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(CAM_RAWB_PWR_CON,
			spm_read(CAM_RAWB_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on CAM_RAWB" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

int spm_mtcmos_ctrl_vpu_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_VPU_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		spm_write(SPM_CROSS_WAKE_M01_REQ,
			spm_read(SPM_CROSS_WAKE_M01_REQ) &
						~APMCU_WAKEUP_APU);
		/* mt6885: no need to wait for power down.*/
		INCREASE_STEPS;
	} else {
		spm_write(EXT_BUCK_ISO, spm_read(EXT_BUCK_ISO) &
							~(0x00000021));

		spm_write(SPM_CROSS_WAKE_M01_REQ,
			spm_read(SPM_CROSS_WAKE_M01_REQ) |
						APMCU_WAKEUP_APU);

		while ((spm_read(OTHER_PWR_STATUS) & (0x1UL << 5)) !=
				(0x1UL << 5))
			ram_console_update();
		INCREASE_STEPS;

		enable_subsys_hwcg(SYS_VPU);
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

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

static int MFG0_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg0_bus_prot(STA_POWER_ON);
}

static int MFG0_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg0_pwr(STA_POWER_ON);
}

static int MFG0_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg0_bus_prot(STA_POWER_DOWN);
}

static int MFG0_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg0_pwr(STA_POWER_DOWN);
}

static int MFG1_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg1_bus_prot(STA_POWER_ON);
}

static int MFG1_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg1_pwr(STA_POWER_ON);
}

static int MFG1_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg1_bus_prot(STA_POWER_DOWN);
}

static int MFG1_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg1_pwr(STA_POWER_DOWN);
}

static int MFG2_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg2_bus_prot(STA_POWER_ON);
}

static int MFG2_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg2_pwr(STA_POWER_ON);
}

static int MFG2_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg2_bus_prot(STA_POWER_DOWN);
}

static int MFG2_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg2_pwr(STA_POWER_DOWN);
}

static int MFG3_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg3_bus_prot(STA_POWER_ON);
}

static int MFG3_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg3_pwr(STA_POWER_ON);
}

static int MFG3_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg3_bus_prot(STA_POWER_DOWN);
}

static int MFG3_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg3_pwr(STA_POWER_DOWN);
}

static int MFG5_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg5_bus_prot(STA_POWER_ON);
}

static int MFG5_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg5_pwr(STA_POWER_ON);
}

static int MFG5_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg5_bus_prot(STA_POWER_DOWN);
}

static int MFG5_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg5_pwr(STA_POWER_DOWN);
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

static int ISP2_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_isp2_bus_prot(STA_POWER_ON);
}

static int ISP2_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_isp2_pwr(STA_POWER_ON);
}

static int ISP2_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_isp2_bus_prot(STA_POWER_DOWN);
}

static int ISP2_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_isp2_pwr(STA_POWER_DOWN);
}

static int IPE_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_ipe_bus_prot(STA_POWER_ON);
}

static int IPE_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_ipe_pwr(STA_POWER_ON);
}

static int IPE_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_ipe_bus_prot(STA_POWER_DOWN);
}

static int IPE_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_ipe_pwr(STA_POWER_DOWN);
}

static int VDE_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_vde_bus_prot(STA_POWER_ON);
}

static int VDE_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_vde_pwr(STA_POWER_ON);
}

static int VDE_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_vde_bus_prot(STA_POWER_DOWN);
}

static int VDE_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_vde_pwr(STA_POWER_DOWN);
}

static int VEN_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_ven_bus_prot(STA_POWER_ON);
}

static int VEN_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_ven_pwr(STA_POWER_ON);
}

static int VEN_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_ven_bus_prot(STA_POWER_DOWN);
}

static int VEN_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_ven_pwr(STA_POWER_DOWN);
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

static int AUDIO_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_audio_bus_prot(STA_POWER_ON);
}

static int AUDIO_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_audio_pwr(STA_POWER_ON);
}

static int AUDIO_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_audio_bus_prot(STA_POWER_DOWN);
}

static int AUDIO_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_audio_pwr(STA_POWER_DOWN);
}

static int ADSP_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_adsp_dormant_bus_prot(STA_POWER_ON);
}

static int ADSP_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_adsp_dormant_pwr(STA_POWER_ON);
}

static int ADSP_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_adsp_dormant_bus_prot(STA_POWER_DOWN);
}

static int ADSP_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_adsp_dormant_pwr(STA_POWER_DOWN);
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

static int CAM_RAWA_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_cam_rawa_bus_prot(STA_POWER_ON);
}

static int CAM_RAWA_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_cam_rawa_pwr(STA_POWER_ON);
}

static int CAM_RAWA_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_cam_rawa_bus_prot(STA_POWER_DOWN);
}

static int CAM_RAWA_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_cam_rawa_pwr(STA_POWER_DOWN);
}

static int CAM_RAWB_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_cam_rawb_bus_prot(STA_POWER_ON);
}

static int CAM_RAWB_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_cam_rawb_pwr(STA_POWER_ON);
}

static int CAM_RAWB_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_cam_rawb_bus_prot(STA_POWER_DOWN);
}

static int CAM_RAWB_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_cam_rawb_pwr(STA_POWER_DOWN);
}

static int VPU_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return 0;
}

static int VPU_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return 0;
}

static int VPU_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_vpu_pwr(STA_POWER_ON);
}

static int VPU_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_vpu_pwr(STA_POWER_DOWN);
}

static int sys_get_state_op(struct subsys *sys)
{
	unsigned int sta;
	unsigned int sta_s;

	if (sys->sta_addr != NULL && sys->sta_s_addr != NULL) {
		sta = clk_readl(sys->sta_addr);
		sta_s = clk_readl(sys->sta_s_addr);

		return (sta & sys->sta_mask) && (sta_s & sys->sta_mask);
	} else if (sys->sta_addr != NULL) {
		sta = clk_readl(sys->sta_addr);

		return (sta & sys->sta_mask);
	}

	sta = clk_readl(PWR_STATUS);
	sta_s = clk_readl(PWR_STATUS_2ND);

	return (sta & sys->sta_mask) && (sta_s & sys->sta_mask);

}

static struct subsys_ops MD1_sys_ops = {
	.prepare =  MD1_sys_prepare_op,
	.unprepare =  MD1_sys_unprepare_op,
	.enable =  MD1_sys_enable_op,
	.disable =  MD1_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops CONN_sys_ops = {
	.prepare =  CONN_sys_prepare_op,
	.unprepare =  CONN_sys_unprepare_op,
	.enable =  CONN_sys_enable_op,
	.disable =  CONN_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops MFG0_sys_ops = {
	.prepare =  MFG0_sys_prepare_op,
	.unprepare =  MFG0_sys_unprepare_op,
	.enable =  MFG0_sys_enable_op,
	.disable =  MFG0_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops MFG1_sys_ops = {
	.prepare =  MFG1_sys_prepare_op,
	.unprepare =  MFG1_sys_unprepare_op,
	.enable =  MFG1_sys_enable_op,
	.disable =  MFG1_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops MFG2_sys_ops = {
	.prepare =  MFG2_sys_prepare_op,
	.unprepare =  MFG2_sys_unprepare_op,
	.enable =  MFG2_sys_enable_op,
	.disable =  MFG2_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops MFG3_sys_ops = {
	.prepare =  MFG3_sys_prepare_op,
	.unprepare =  MFG3_sys_unprepare_op,
	.enable =  MFG3_sys_enable_op,
	.disable =  MFG3_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops MFG5_sys_ops = {
	.prepare =  MFG5_sys_prepare_op,
	.unprepare =  MFG5_sys_unprepare_op,
	.enable =  MFG5_sys_enable_op,
	.disable =  MFG5_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops ISP_sys_ops = {
	.prepare =  ISP_sys_prepare_op,
	.unprepare =  ISP_sys_unprepare_op,
	.enable =  ISP_sys_enable_op,
	.disable =  ISP_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops ISP2_sys_ops = {
	.prepare =  ISP2_sys_prepare_op,
	.unprepare =  ISP2_sys_unprepare_op,
	.enable =  ISP2_sys_enable_op,
	.disable =  ISP2_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops IPE_sys_ops = {
	.prepare =  IPE_sys_prepare_op,
	.unprepare =  IPE_sys_unprepare_op,
	.enable =  IPE_sys_enable_op,
	.disable =  IPE_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops VDE_sys_ops = {
	.prepare =  VDE_sys_prepare_op,
	.unprepare =  VDE_sys_unprepare_op,
	.enable =  VDE_sys_enable_op,
	.disable =  VDE_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops VEN_sys_ops = {
	.prepare =  VEN_sys_prepare_op,
	.unprepare =  VEN_sys_unprepare_op,
	.enable =  VEN_sys_enable_op,
	.disable =  VEN_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops DIS_sys_ops = {
	.prepare =  DIS_sys_prepare_op,
	.unprepare =  DIS_sys_unprepare_op,
	.enable =  DIS_sys_enable_op,
	.disable =  DIS_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops AUDIO_sys_ops = {
	.prepare =  AUDIO_sys_prepare_op,
	.unprepare =  AUDIO_sys_unprepare_op,
	.enable =  AUDIO_sys_enable_op,
	.disable =  AUDIO_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops ADSP_sys_ops = {
	.prepare =  ADSP_sys_prepare_op,
	.unprepare =  ADSP_sys_unprepare_op,
	.enable =  ADSP_sys_enable_op,
	.disable =  ADSP_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops CAM_sys_ops = {
	.prepare =  CAM_sys_prepare_op,
	.unprepare =  CAM_sys_unprepare_op,
	.enable =  CAM_sys_enable_op,
	.disable =  CAM_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops CAM_RAWA_sys_ops = {
	.prepare =  CAM_RAWA_sys_prepare_op,
	.unprepare =  CAM_RAWA_sys_unprepare_op,
	.enable =  CAM_RAWA_sys_enable_op,
	.disable =  CAM_RAWA_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops CAM_RAWB_sys_ops = {
	.prepare =  CAM_RAWB_sys_prepare_op,
	.unprepare =  CAM_RAWB_sys_unprepare_op,
	.enable =  CAM_RAWB_sys_enable_op,
	.disable =  CAM_RAWB_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops VPU_sys_ops = {
	.prepare =  VPU_sys_prepare_op,
	.unprepare =  VPU_sys_unprepare_op,
	.enable = VPU_sys_enable_op,
	.disable = VPU_sys_disable_op,
	.get_state = sys_get_state_op,
};

/* auto-gen end*/

static int subsys_is_on(enum subsys_id id)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	int r;
	struct subsys *sys = id_to_sys(id);

	WARN_ON(!sys);

	r = sys->ops->get_state(sys);

#if MT_CCF_DEBUG
	pr_debug("[CCF] %s:%d, sys=%s, id=%d\n", __func__, r, sys->name, id);
#endif				/* MT_CCF_DEBUG */

	return r;
#else
	return 1;
#endif
}

#if CONTROL_LIMIT
int allow[NR_SYSS] = {
1,/*SYS_MD1*/
1,/*SYS_CONN*/
1,/*SYS_MFG0*/
1,/*SYS_MFG1*/
1,/*SYS_MFG2*/
1,/*SYS_MFG3*/
1,/*SYS_MFG5*/
1,/*SYS_ISP*/
1,/*SYS_ISP2*/
1,/*SYS_IPE*/
1,/*SYS_VDE*/
1,/*SYS_VEN*/
1,/*SYS_DIS*/
1,/*SYS_AUDIO*/
1,/*SYS_ADSP*/
1,/*SYS_CAM*/
1,/*SYS_CAM_RAWA*/
1,/*SYS_CAM_RAWB*/
1,/*SYS_VPU*/
};
#endif

static int isNeedMfgFakePowerOn(enum subsys_id id)
{
	int isGpuDfdTriggered = 0;
	unsigned int gpu_dfd_status;

	if (id == SYS_MFG0 || id == SYS_MFG1 || id == SYS_MFG2 ||
	    id == SYS_MFG3 || id == SYS_MFG5) {
		gpu_dfd_status = spm_read(MFG_MISC_CON);

		// if gpu dfd is triggered, the power control will be locked
		// so we need to do fake power on
		if (gpu_dfd_status & MFG_DFD_TRIGGER) {
			pr_info("%s:power on, MFG_MISC_CON(0x%x)\n",
				__func__, gpu_dfd_status);
			isGpuDfdTriggered = 1;
		}
	}

	return isGpuDfdTriggered;
}

static int enable_subsys(enum subsys_id id, enum mtcmos_op action)
{
	int r = 0;
	unsigned long flags;
	unsigned long spinlock_save_flags;
	struct subsys *sys = id_to_sys(id);
	struct pg_callbacks *pgcb;

	if (!sys) {
		WARN_ON(!sys);
		return -EINVAL;
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

	if (action == MTCMOS_BUS_PROT && sys->ops->prepare)
		r = sys->ops->prepare(sys);
	else if (action == MTCMOS_PWR && sys->ops->enable)
		r = sys->ops->enable(sys);

	WARN_ON(r);

	mtk_clk_unlock(flags);

	if (action == MTCMOS_BUS_PROT) {
		spin_lock_irqsave(&pgcb_lock, spinlock_save_flags);
		list_for_each_entry(pgcb, &pgcb_list, list) {
			if (pgcb->after_on)
				pgcb->after_on(id);
		}
		spin_unlock_irqrestore(&pgcb_lock, spinlock_save_flags);
	}

	return r;
}

static int disable_subsys(enum subsys_id id, enum mtcmos_op action)
{
	int r = 0;
	unsigned long flags;
	unsigned long spinlock_save_flags;
	struct subsys *sys = id_to_sys(id);
	struct pg_callbacks *pgcb;

	if (!sys) {
		WARN_ON(!sys);
		return -EINVAL;
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
		spin_lock_irqsave(&pgcb_lock, spinlock_save_flags);
		list_for_each_entry_reverse(pgcb, &pgcb_list, list) {
			if (pgcb->before_off)
				pgcb->before_off(id);
		}
		spin_unlock_irqrestore(&pgcb_lock, spinlock_save_flags);
	}

	mtk_clk_lock(flags);

	if (action == MTCMOS_BUS_PROT && sys->ops->unprepare)
		r = sys->ops->unprepare(sys);
	else if (action == MTCMOS_PWR && sys->ops->disable) {
		/*
		 * Check if subsys CGs are still on before the mtcmos  is going
		 * to be off. (Could do nothing here for early porting)
		 */
		mtk_check_subsys_swcg(id);

		r = sys->ops->disable(sys);
	}

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
	const char *lp_cg[CLK_NUM];
};

static int pg_pre_clk_ctrl(struct cg_list *list,
		const char *name, unsigned int enable, unsigned int lp)
{
	struct clk *clk;
	int ret = 0;
	int i = 0;

	do {
		if (list == NULL)
			break;

		if (!lp)
			clk = list->cg[i] ? __clk_lookup(list->cg[i]) : NULL;
		else
			clk = list->lp_cg[i] ?
					__clk_lookup(list->lp_cg[i]) : NULL;

		if (!clk) {
			if (list->cg[i] && !lp)
				pr_notice("[CCF] cannot find pre_clk(%s)\n",
						list->cg[i]);
			break;
		}

		if (enable == CLK_ENABLE) {
			ret = clk_prepare_enable(clk);
			if (ret)
				break;
		} else if (enable == CLK_DISABLE)
			clk_disable_unprepare(clk);

#if MT_CCF_DEBUG
		pr_notice("[CCF] %s: sys=%s, pre_clk=%s\n",
				__func__,
				name ? name : NULL,
				lp ? (list->lp_cg[i] ? list->lp_cg[i] : NULL) :
				(list->cg[i] ? list->cg[i] : NULL));
#endif				/* MT_CCF_DEBUG */
		i++;
	} while (i < CLK_NUM);

	return ret;
}

static int pg_is_enabled(struct clk_hw *hw)
{
	struct mt_power_gate *pg = to_power_gate(hw);

	return subsys_is_on(pg->pd_id);
}

int pg_prepare(struct clk_hw *hw)
{
	struct mt_power_gate *pg = to_power_gate(hw);
	const char *pg_name = __clk_get_name(hw->clk);
	unsigned long flags;
	int skip_pg = 0;
	int ret = 0;

	mtk_mtcmos_lock(flags);
#if CHECK_PWR_ST
	if (pg_is_enabled(hw) == SUBSYS_PWR_ON &&
		!isNeedMfgFakePowerOn(pg->pd_id))
		skip_pg = 1;
#endif				/* CHECK_PWR_ST */

	ret = pg_pre_clk_ctrl(pg->pre_clk1_list, pg_name,
			CLK_ENABLE, NORMAL_CLK);
	if (ret)
		goto fail;

	ret = pg_pre_clk_ctrl(pg->pre_clk1_list, pg_name,
			CLK_ENABLE, LP_CLK);
	if (ret)
		goto fail;

	if (!skip_pg) {
		ret = enable_subsys(pg->pd_id, MTCMOS_PWR);
		if (ret)
			goto fail;
	}

	ret = pg_pre_clk_ctrl(pg->pre_clk2_list, pg_name,
			CLK_ENABLE, NORMAL_CLK);
	if (ret)
		goto fail;

	ret = pg_pre_clk_ctrl(pg->pre_clk2_list, pg_name, CLK_ENABLE, LP_CLK);
	if (ret)
		goto fail;

	if (!skip_pg) {
		ret = enable_subsys(pg->pd_id, MTCMOS_BUS_PROT);
		if (ret)
			goto fail;
	}

	pg_pre_clk_ctrl(pg->pre_clk2_list, pg_name, CLK_DISABLE, LP_CLK);
	pg_pre_clk_ctrl(pg->pre_clk1_list, pg_name, CLK_DISABLE, LP_CLK);
fail:
	mtk_mtcmos_unlock(flags);

	return ret;
}

void pg_unprepare(struct clk_hw *hw)
{
	struct mt_power_gate *pg = to_power_gate(hw);
	const char *pg_name = __clk_get_name(hw->clk);
	unsigned long flags;
	int skip_pg = 0;
	int ret = 0;

	mtk_mtcmos_lock(flags);
#if CHECK_PWR_ST
	if (pg_is_enabled(hw) == SUBSYS_PWR_DOWN)
		skip_pg = 1;
#endif				/* CHECK_PWR_ST */

	ret = pg_pre_clk_ctrl(pg->pre_clk1_list, pg_name, CLK_ENABLE, LP_CLK);
	if (ret)
		goto fail;

	ret = pg_pre_clk_ctrl(pg->pre_clk2_list, pg_name, CLK_ENABLE, LP_CLK);
	if (ret)
		goto fail;

	if (!skip_pg)
		disable_subsys(pg->pd_id, MTCMOS_BUS_PROT);

	pg_pre_clk_ctrl(pg->pre_clk2_list, pg_name, CLK_DISABLE, NORMAL_CLK);
	pg_pre_clk_ctrl(pg->pre_clk2_list, pg_name, CLK_DISABLE, LP_CLK);

	if (!skip_pg)
		disable_subsys(pg->pd_id, MTCMOS_PWR);

	pg_pre_clk_ctrl(pg->pre_clk1_list, pg_name, CLK_DISABLE, NORMAL_CLK);
	pg_pre_clk_ctrl(pg->pre_clk1_list, pg_name, CLK_DISABLE, LP_CLK);

fail:
	mtk_mtcmos_unlock(flags);
}

static const struct clk_ops mt_power_gate_ops = {
	.prepare = pg_prepare,
	.unprepare = pg_unprepare,
	.is_prepared = pg_is_enabled,
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
	init.parent_names = parent_name ? &parent_name : NULL;
	init.num_parents = parent_name ? 1 : 0;
	init.ops = &mt_power_gate_ops;
	init.flags = 0;

	pg->pre_clk1_list = pre_clk1_list;
	pg->pre_clk2_list = pre_clk2_list;
	pg->pd_id = pd_id;
	pg->hw.init = &init;

	clk = clk_register(NULL, &pg->hw);
	if (IS_ERR(clk))
		kfree(pg);

	return clk;
}

struct cg_list audio_cg1 = {.cg = {"aud_intbus_sel"},};
struct cg_list audio_cg2 = {
	.cg = {
		"ifrao_audio26m",
		"ifrao_audio"
	},
};

struct cg_list adsp_cg = {.cg = {"adsp_sel"},};

struct cg_list mfg_cg = {.cg = {"mfg_pll_sel", "mfg_ref_sel"},};

struct cg_list mm_cg1 = {
	.cg = {"disp_sel"},
	.lp_cg = {"mdp_sel"},
};

struct cg_list mm_cg2 = {
	.cg = {
		"mm_smi_common",
		"mm_smi_infra",
		"mm_smi_iommu",
		"mm_smi_gals"
	},
	.lp_cg = {"mdp_smi0"},
};

struct cg_list isp_cg1 = {.cg = {"img1_sel"},};

struct cg_list isp_cg2 = {.cg = {"imgsys1_larb9", "imgsys1_gals"},};

struct cg_list isp2_cg1 = {.cg = {"img2_sel"},};

struct cg_list isp2_cg2 = {
	.cg = {
		"imgsys2_larb9",
		"imgsys2_larb10",
		"imgsys2_gals"
	},
};

struct cg_list ipe_cg1 = {.cg = {"ipe_sel"},};

struct cg_list ipe_cg2 = {
	.cg = {
		"ipe_larb19",
		"ipe_larb20",
		"ipe_smi_subcom",
		"ipe_gals",
	},
};

struct cg_list ven_cg1 = {.cg = {"venc_sel"},};

struct cg_list ven_cg2 = {.cg = {"venc_set1_venc"},};

struct cg_list vde_cg1 = {.cg = {"vdec_sel"},};

struct cg_list vde_cg2 = {.cg = {"vdec_cken", "vdec_larb1_cken"},};

struct cg_list cam_cg1 = {.cg = {"cam_sel"},};

struct cg_list cam_cg2 = {
	.cg = {
		"cam_m_larb13",
		"cam_m_larb14",
		"cam_m_ccu_gals",
		"cam_m_cam2mm_gals"
	},
};

struct cg_list cam_ra_cg = {.cg = {"cam_ra_larbx"},};

struct cg_list cam_rb_cg = {.cg = {"cam_rb_larbx"},};

struct cg_list vpu_cg1 = {
	.cg = {
		"ipu_if_sel",
		"dsp_sel",
	},
};

struct cg_list vpu_cg2 = {.cg = {"apuc_iommu_0"},};

struct mtk_power_gate {
	int id;
	const char *name;
	const char *parent_name;
	enum subsys_id pd_id;
	struct cg_list *pre_clk1_names;
	struct cg_list *pre_clk2_names;
};

#define PGATE(_id, _name, _parent, _pre_clks1, _pre_clks2, _pd_id) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.pre_clk1_names = _pre_clks1,		\
		.pre_clk2_names = _pre_clks2,		\
		.pd_id = _pd_id,				\
	}

/* FIXME: all values needed to be verified */
struct mtk_power_gate scp_clks[] = {
	PGATE(SCP_SYS_MD1, "PG_MD1", NULL, NULL, NULL, SYS_MD1),
	PGATE(SCP_SYS_CONN, "PG_CONN", NULL, NULL, NULL, SYS_CONN),
	PGATE(SCP_SYS_DIS, "PG_DIS", NULL, &mm_cg1, &mm_cg2, SYS_DIS),
	PGATE(SCP_SYS_MFG0, "PG_MFG0", NULL, &mfg_cg, NULL, SYS_MFG0),
	PGATE(SCP_SYS_MFG1, "PG_MFG1", "PG_MFG0", NULL, NULL, SYS_MFG1),
	PGATE(SCP_SYS_MFG2, "PG_MFG2", "PG_MFG1", NULL, NULL, SYS_MFG2),
	PGATE(SCP_SYS_MFG3, "PG_MFG3", "PG_MFG1", NULL, NULL, SYS_MFG3),
	PGATE(SCP_SYS_MFG5, "PG_MFG5", "PG_MFG1", NULL, NULL, SYS_MFG5),
	PGATE(SCP_SYS_ISP, "PG_ISP", "PG_DIS", &isp_cg1, &isp_cg2, SYS_ISP),
	PGATE(SCP_SYS_ISP2, "PG_ISP2", "PG_DIS", &isp2_cg1,
			&isp2_cg2, SYS_ISP2),
	PGATE(SCP_SYS_IPE, "PG_IPE", "PG_DIS", &ipe_cg1, &ipe_cg2, SYS_IPE),
	PGATE(SCP_SYS_VDEC, "PG_VDEC", "PG_DIS", &vde_cg1, &vde_cg2, SYS_VDE),
	PGATE(SCP_SYS_VENC, "PG_VENC", "PG_DIS", &ven_cg1, &ven_cg2, SYS_VEN),
	PGATE(SCP_SYS_AUDIO, "PG_AUDIO", NULL, &audio_cg1,
			&audio_cg2, SYS_AUDIO),
	PGATE(SCP_SYS_ADSP, "PG_ADSP", NULL, &adsp_cg, NULL, SYS_ADSP),
	PGATE(SCP_SYS_CAM, "PG_CAM", "PG_DIS", &cam_cg1, &cam_cg2, SYS_CAM),
	PGATE(SCP_SYS_CAM_RAWA, "PG_CAM_RAWA", "PG_CAM", NULL,
			&cam_ra_cg, SYS_CAM_RAWA),
	PGATE(SCP_SYS_CAM_RAWB, "PG_CAM_RAWB", "PG_CAM", NULL,
			&cam_rb_cg, SYS_CAM_RAWB),
	/* Gary Wang: no need to turn on disp mtcmos*/
	PGATE(SCP_SYS_VPU, "PG_VPU", NULL, &vpu_cg1, &vpu_cg2, SYS_VPU),
};

static void init_clk_scpsys(struct clk_onecell_data *clk_data)
{
	int i;
	struct clk *clk;

	for (i = 0; i < ARRAY_SIZE(scp_clks); i++) {
		struct mtk_power_gate *pg = &scp_clks[i];

#if MT_CCF_BRINGUP
		pr_notice("[CCF] %s: pgate %3d: %s begin\n", __func__,
				i, pg->name);
#endif

#if !MT_CCF_BRINGUP
		clk = mt_clk_register_power_gate(pg->name,
			pg->parent_name, pg->pre_clk1_names,
			pg->pre_clk2_names, pg->pd_id);
#else
		clk = mt_clk_register_power_gate(pg->name,
			pg->parent_name, NULL,
			NULL, pg->pd_id);
#endif
		if (IS_ERR(clk)) {
			pr_err("[CCF] %s: Failed to register clk %s: %ld\n",
				__func__, pg->name, PTR_ERR(clk));
			continue;
		}

		if (clk_data)
			clk_data->clks[pg->id] = clk;

#if MT_CCF_BRINGUP
		pr_notice("[CCF] %s: pgate %3d: %s end\n", __func__,
				i, pg->name);
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

static int clk_mt6853_scpsys_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif
	infracfg_base = get_reg(node, 0);
	spm_base = get_reg(node, 1);
	infra_base = get_reg(node, 2);
	infra_pdn_base = get_reg(node, 3);

	if (!infracfg_base || !spm_base || !infra_base || !infra_pdn_base) {
		pr_err("clk-pg-mt6853: missing reg\n");
		return -ENODEV;
	}

	syss[SYS_VPU].sta_addr = OTHER_PWR_STATUS;
	clk_data = alloc_clk_data(SCP_NR_SYSS);
	init_clk_scpsys(clk_data);
	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("[CCF] %s:could not register clock provide\n",
			__func__);

	iomap_apu();

	spin_lock_init(&pgcb_lock);
#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif
	return r;
}

static const struct of_device_id of_match_clk_mt6853_scpsys[] = {
	{ .compatible = "mediatek,mt6853-scpsys", },
	{}
};

static struct platform_driver clk_mt6853_scpsys_drv = {
	.probe = clk_mt6853_scpsys_probe,
	.driver = {
		.name = "clk-mt6853-scpsys",
		.of_match_table = of_match_clk_mt6853_scpsys,
	},
};
static int __init clk_mt6853_scpsys_init(void)
{
	return platform_driver_register(&clk_mt6853_scpsys_drv);
}
arch_initcall_sync(clk_mt6853_scpsys_init);

/* for suspend LDVT only */
void mtcmos_force_off(void)
{
	pr_notice("suspend test: vpu\n");
	spm_mtcmos_ctrl_vpu_pwr(STA_POWER_DOWN);

	pr_notice("suspend test: cam\n");
	spm_mtcmos_ctrl_cam_rawa_bus_prot(STA_POWER_DOWN);
	spm_mtcmos_ctrl_cam_rawa_pwr(STA_POWER_DOWN);
	spm_mtcmos_ctrl_cam_rawb_bus_prot(STA_POWER_DOWN);
	spm_mtcmos_ctrl_cam_rawb_pwr(STA_POWER_DOWN);
	spm_mtcmos_ctrl_cam_bus_prot(STA_POWER_DOWN);
	spm_mtcmos_ctrl_cam_pwr(STA_POWER_DOWN);

	pr_notice("suspend test: ven\n");
	spm_mtcmos_ctrl_ven_bus_prot(STA_POWER_DOWN);
	spm_mtcmos_ctrl_ven_pwr(STA_POWER_DOWN);

	pr_notice("suspend test: vde\n");
	spm_mtcmos_ctrl_vde_bus_prot(STA_POWER_DOWN);
	spm_mtcmos_ctrl_vde_pwr(STA_POWER_DOWN);

	pr_notice("suspend test: ipe\n");
	spm_mtcmos_ctrl_ipe_bus_prot(STA_POWER_DOWN);
	spm_mtcmos_ctrl_ipe_pwr(STA_POWER_DOWN);

	pr_notice("suspend test: isp\n");
	spm_mtcmos_ctrl_isp2_bus_prot(STA_POWER_DOWN);
	spm_mtcmos_ctrl_isp2_pwr(STA_POWER_DOWN);
	spm_mtcmos_ctrl_isp_bus_prot(STA_POWER_DOWN);
	spm_mtcmos_ctrl_isp_pwr(STA_POWER_DOWN);

	pr_notice("suspend test: mfg\n");
	spm_mtcmos_ctrl_mfg5_bus_prot(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg5_pwr(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg3_bus_prot(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg3_pwr(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg2_bus_prot(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg2_pwr(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg1_bus_prot(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg1_pwr(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg0_bus_prot(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg0_pwr(STA_POWER_DOWN);

	pr_notice("suspend test: audio\n");
	spm_mtcmos_ctrl_audio_bus_prot(STA_POWER_DOWN);
	spm_mtcmos_ctrl_audio_pwr(STA_POWER_DOWN);

	pr_notice("suspend test: adsp\n");
	/* spm_mtcmos_ctrl_adsp_shut_down(STA_POWER_DOWN); */
	spm_mtcmos_ctrl_adsp_dormant_bus_prot(STA_POWER_DOWN);
	spm_mtcmos_ctrl_adsp_dormant_pwr(STA_POWER_DOWN);

	pr_notice("suspend test: dis\n");
	spm_mtcmos_ctrl_dis_bus_prot(STA_POWER_DOWN);
	spm_mtcmos_ctrl_dis_pwr(STA_POWER_DOWN);

	pr_notice("suspend test: md1\n");
	spm_mtcmos_ctrl_md1_bus_prot(STA_POWER_DOWN);
	spm_mtcmos_ctrl_md1_bus_prot(STA_POWER_DOWN);

	pr_notice("suspend test: conn\n");
	spm_mtcmos_ctrl_conn_bus_prot(STA_POWER_DOWN);
	spm_mtcmos_ctrl_conn_pwr(STA_POWER_DOWN);
}
