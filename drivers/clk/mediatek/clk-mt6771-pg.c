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
#include "clk-mt6771-pg.h"
/*#include "../../misc/mediatek/base/power/mt6771/mtk_gpufreq.h"*/

#include <dt-bindings/clock/mt6771-clk.h>

/*#define TOPAXI_PROTECT_LOCK*/

#if !defined(MT_CCF_DEBUG) || !defined(MT_CCF_BRINGUP)
#define MT_CCF_DEBUG	0
#define MT_CCF_BRINGUP  0
#define CONTROL_LIMIT 1
#endif

#define	CHECK_PWR_ST	1

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
void __iomem *clk_vdec_gcon_base;
void __iomem *clk_venc_gcon_base;
void __iomem *clk_camsys_base;
void __iomem *clk_mfg_base;
#endif

#if 0
#define MM_CG_CLR0 (clk_mmsys_config_base + 0x108)
#define IMG_CG_CLR	(clk_imgsys_base + 0x0008)
#define VDEC_CKEN_SET	(clk_vdec_gcon_base + 0x0000)
#define VDEC_GALS_CFG (clk_vdec_gcon_base + 0x0168)
#define VENC_CG_SET	(clk_venc_gcon_base + 0x0004)
#endif
#define MM_CG_CON0 (clk_mmsys_config_base + 0x100)
#define MM_CG_CLR0 (clk_mmsys_config_base + 0x108)
#define IMG_CG_CLR	(clk_imgsys_base + 0x0008)
#define CAM_CG_CLR	(clk_camsys_base + 0x0008)
#define IMG_CG_CON	(clk_imgsys_base + 0x0000)
#define CAM_CG_CON	(clk_camsys_base + 0x0000)


#define MFG_QCHANNEL_CON (clk_mfg_base + 0x0B4)
#define MFG_DEBUG_17C (clk_mfg_base + 0x17C)
#define MFG_DEBUG_SEL (clk_mfg_base + 0x180)
#define MFG_DEBUG_LATCH (clk_mfg_base + 0x184)
#define MFG_DEBUG_TOP (clk_mfg_base + 0x188)
#define MFG_DEBUG_ASYNC (clk_mfg_base + 0x18C)
/*
 * MTCMOS
 */

#define STA_POWER_DOWN	0
#define STA_POWER_ON	1
#define SUBSYS_PWR_DOWN		0
#define SUBSYS_PWR_ON		1

	struct subsys;

struct subsys_ops {
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
static struct subsys_ops DIS_sys_ops;
static struct subsys_ops MFG_sys_ops;
static struct subsys_ops ISP_sys_ops;
static struct subsys_ops VEN_sys_ops;
static struct subsys_ops MFG_ASYNC_sys_ops;
static struct subsys_ops AUDIO_sys_ops;
static struct subsys_ops CAM_sys_ops;
static struct subsys_ops MFG_CORE1_sys_ops;
static struct subsys_ops MFG_CORE0_sys_ops;
static struct subsys_ops MFG_2D_sys_ops;
static struct subsys_ops VDE_sys_ops;
static struct subsys_ops VPU_TOP_sys_ops;
static struct subsys_ops VPU_CORE0_DORMANT_sys_ops;
static struct subsys_ops VPU_CORE0_SHUTDOWN_sys_ops;
static struct subsys_ops VPU_CORE1_DORMANT_sys_ops;
static struct subsys_ops VPU_CORE1_SHUTDOWN_sys_ops;
static struct subsys_ops VPU_CORE2_DORMANT_sys_ops;
static struct subsys_ops VPU_CORE2_SHUTDOWN_sys_ops;


static void __iomem *infracfg_base;/*infracfg_ao*/
static void __iomem *spm_base;
static void __iomem *infra_base;/*infracfg*/
static void __iomem *ckgen_base;/*ckgen*/
static void __iomem *smi_larb6_base;
static void __iomem *smi_common_base;

#define INFRACFG_REG(offset)		(infracfg_base + offset)
#define SPM_REG(offset)			(spm_base + offset)
#define INFRA_REG(offset)	(infra_base + offset)
#define CKGEN_REG(offset)	(ckgen_base + offset)
#define SMI_LARB6_REG(offset)	(smi_larb6_base + offset)
#define SMI_COMMON_REG(offset)	(smi_common_base + offset)
/**************************************
 * for non-CPU MTCMOS
 **************************************/
#if 1
#define POWERON_CONFIG_EN			SPM_REG(0x0000)
#define SUBSYS_IDLE_STA	SPM_REG(0x0170)

#define PWR_STATUS       SPM_REG(0x0180)
#define PWR_STATUS_2ND       SPM_REG(0x0184)

#define MD_SRAM_ISO_CON     SPM_REG(0x0394)
#define MD_EXTRA_PWR_CON       SPM_REG(0x0398)


#define VDE_PWR_CON       SPM_REG(0x0300)
#define VEN_PWR_CON       SPM_REG(0x0304)
#define ISP_PWR_CON       SPM_REG(0x0308)
#define DIS_PWR_CON       SPM_REG(0x030C)
#define ISP_PWR_CON       SPM_REG(0x0308)
#define MFG_CORE1_PWR_CON       SPM_REG(0x0310)
#define AUDIO_PWR_CON     SPM_REG(0x0314)
#define MD1_PWR_CON      SPM_REG(0x320)
#define VPU_TOP_PWR_CON       SPM_REG(0x324)
#define CONN_PWR_CON       SPM_REG(0x32C)
#define VPU_CORE2_PWR_CON       SPM_REG(0x330)
#define MFG_ASYNC_PWR_CON       SPM_REG(0x334)
#define MFG_PWR_CON       SPM_REG(0x338)
#define VPU_CORE0_PWR_CON       SPM_REG(0x33C)
#define VPU_CORE1_PWR_CON       SPM_REG(0x340)
#define CAM_PWR_CON       SPM_REG(0x344)
#define MFG_2D_PWR_CON       SPM_REG(0x348)
#define MFG_CORE0_PWR_CON       SPM_REG(0x34C)


#define MBIST_EFUSE_REPAIR_ACK_STA       SPM_REG(0x3D0)

#define INFRA_TOPAXI_SI0_CTL	INFRACFG_REG(0x0200)
#define INFRA_TOPAXI_PROTECTEN		INFRACFG_REG(0x0220)
#define INFRA_TOPAXI_PROTECTSTA0	INFRACFG_REG(0x0224)
#define INFRA_TOPAXI_PROTECTEN_STA1	INFRACFG_REG(0x0228)
#define INFRA_TOPAXI_PROTECTEN_1   INFRACFG_REG(0x0250)
#define INFRA_TOPAXI_PROTECTSTA0_1 INFRACFG_REG(0x0254)
#define INFRA_TOPAXI_PROTECTEN_STA1_1 INFRACFG_REG(0x0258)

#define INFRA_TOPAXI_PROTECTEN_SET	INFRACFG_REG(0x02A0)
#define INFRA_TOPAXI_PROTECTEN_CLR	INFRACFG_REG(0x02A4)
#define INFRA_TOPAXI_PROTECTEN_1_SET	INFRACFG_REG(0x02A8)
#define INFRA_TOPAXI_PROTECTEN_1_CLR	INFRACFG_REG(0x02AC)

#define INFRA_TOPAXI_PROTECTEN_MM	INFRACFG_REG(0x02D0)
#define INFRA_TOPAXI_PROTECTEN_MM_SET	INFRACFG_REG(0x02D4)
#define INFRA_TOPAXI_PROTECTEN_MM_CLR	INFRACFG_REG(0x02D8)
#define INFRA_TOPAXI_PROTECTEN_MM_STA0	INFRACFG_REG(0x02E8)
#define INFRA_TOPAXI_PROTECTEN_MM_STA1	INFRACFG_REG(0x02EC)

#define INFRA_TOPAXI_PROTECTEN_MCU_SET	INFRACFG_REG(0x02C4)
#define INFRA_TOPAXI_PROTECTEN_MCU_CLR	INFRACFG_REG(0x02C8)
#define INFRA_TOPAXI_PROTECTEN_MCU_STA0	INFRACFG_REG(0x02E0)
#define INFRA_TOPAXI_PROTECTEN_MCU_STA1	INFRACFG_REG(0x02E4)

#define INFRABUS_DBG1	INFRACFG_REG(0x0D04)
#define INFRABUS_DBG15	INFRACFG_REG(0x0D3C)
#define INFRABUS_DBG17	INFRACFG_REG(0x0D44)
#define INFRABUS_DBG21	INFRACFG_REG(0x0D54)
/* INFRACFG */
#define INFRA_TOPAXI_SI0_STA		INFRA_REG(0x0000)
#define INFRA_BUS_IDLE_STA5	INFRA_REG(0x0190)
/* SMI LARB */
#define SMI_LARB_STAT	SMI_LARB6_REG(0x0000)
#define SMI_LARB6_SLP_CON    SMI_LARB6_REG(0x000C)

/* SMI COMMON */
#define SMI_COMMON_SMI_CLAMP	SMI_COMMON_REG(0x03C0)
#define SMI_COMMON_SMI_CLAMP_SET    SMI_COMMON_REG(0x03C4)
#define SMI_COMMON_SMI_CLAMP_CLR    SMI_COMMON_REG(0x03C8)


#define CLK_MODE	CKGEN_REG(0x0000)
#endif

#define  SPM_PROJECT_CODE    0xB16

/* Define MBIST EFUSE control */

#define INFRA_AO_EFUSE_S2P_RX_DONE_BIT   (0x1 << 3)
#define INFRA_PWR_EFUSE_S2P_RX_DONE_BIT  (0x1 << 2)
#define CAM_EFUSE_S2P_RX_DONE_BIT        (0x1 << 1)
#define MFG_EFUSE_S2P_RX_DONE_BIT        (0x1 << 0)

/* Define MTCMOS power control */
#define PWR_RST_B                        (0x1 << 0)
#define PWR_ISO                          (0x1 << 1)
#define PWR_ON                           (0x1 << 2)
#define PWR_ON_2ND                       (0x1 << 3)
#define PWR_CLK_DIS                      (0x1 << 4)
#define SRAM_CKISO                       (0x1 << 5)
#define SRAM_ISOINT_B                    (0x1 << 6)
#define SLPB_CLAMP                       (0x1 << 7)

/* Define MTCMOS Bus Protect Mask */
#define MD1_PROT_STEP1_0_MASK            ((0x1 << 7))
#define MD1_PROT_STEP1_0_ACK_MASK        ((0x1 << 7))
#define MD1_PROT_STEP2_0_MASK            ((0x1 << 3) \
					  |(0x1 << 4))
#define MD1_PROT_STEP2_0_ACK_MASK        ((0x1 << 3) \
					  |(0x1 << 4))
#define MD1_PROT_STEP2_1_MASK            ((0x1 << 6))
#define MD1_PROT_STEP2_1_ACK_MASK        ((0x1 << 6))
#define CONN_PROT_STEP1_0_MASK           ((0x1 << 13) \
					  |(0x1 << 14))
#define CONN_PROT_STEP1_0_ACK_MASK       ((0x1 << 13) \
					  |(0x1 << 14))
#define DIS_PROT_STEP1_0_MASK            ((0x1 << 16) \
					  |(0x1 << 17))
#define DIS_PROT_STEP1_0_ACK_MASK        ((0x1 << 16) \
					  |(0x1 << 17))
#define DIS_PROT_STEP2_0_MASK            ((0x1 << 10) \
					  |(0x1 << 11))
#define DIS_PROT_STEP2_0_ACK_MASK        ((0x1 << 10) \
					  |(0x1 << 11))
#define DIS_PROT_STEP2_1_MASK            ((0x1 << 0) \
					  |(0x1 << 1) \
					  |(0x1 << 2) \
					  |(0x1 << 3) \
					  |(0x1 << 4) \
					  |(0x1 << 5) \
					  |(0x1 << 6) \
					  |(0x1 << 7))
#define DIS_PROT_STEP2_1_ACK_MASK        ((0x1 << 0) \
					  |(0x1 << 1) \
					  |(0x1 << 2) \
					  |(0x1 << 3) \
					  |(0x1 << 4) \
					  |(0x1 << 5) \
					  |(0x1 << 6) \
					  |(0x1 << 7))
#define MFG_PROT_STEP1_0_MASK            ((0x1 << 19) \
					  |(0x1 << 20) \
					  |(0x1 << 21))
#define MFG_PROT_STEP1_0_ACK_MASK        ((0x1 << 19) \
					  |(0x1 << 20) \
					  |(0x1 << 21))
#define MFG_PROT_STEP2_0_MASK            ((0x1 << 21) \
					  |(0x1 << 22))
#define MFG_PROT_STEP2_0_ACK_MASK        ((0x1 << 21) \
					  |(0x1 << 22))
#define ISP_PROT_STEP1_0_MASK            ((0x1 << 3) \
					  |(0x1 << 8))
#define ISP_PROT_STEP1_0_ACK_MASK        ((0x1 << 3) \
					  |(0x1 << 8))
#define ISP_PROT_STEP2_0_MASK            ((0x1 << 10))
#define ISP_PROT_STEP2_0_ACK_MASK        ((0x1 << 10))
#define ISP_PROT_STEP2_1_MASK            ((0x1 << 2))
#define ISP_PROT_STEP2_1_ACK_MASK        ((0x1 << 2))
#define VEN_PROT_STEP2_0_MASK            ((0x1 << 1))
#define VEN_PROT_STEP2_0_ACK_MASK        ((0x1 << 1))
#define CAM_PROT_STEP1_0_MASK            ((0x1 << 4) \
					  |(0x1 << 5) \
					  |(0x1 << 9) \
					  |(0x1 << 13))
#define CAM_PROT_STEP1_0_ACK_MASK        ((0x1 << 4) \
					  |(0x1 << 5) \
					  |(0x1 << 9) \
					  |(0x1 << 13))
#define CAM_PROT_STEP2_0_MASK            ((0x1 << 28))
#define CAM_PROT_STEP2_0_ACK_MASK        ((0x1 << 28))
#define CAM_PROT_STEP2_1_MASK            ((0x1 << 11))
#define CAM_PROT_STEP2_1_ACK_MASK        ((0x1 << 11))
#define CAM_PROT_STEP2_2_MASK            ((0x1 << 3) \
					  |(0x1 << 4))
#define CAM_PROT_STEP2_2_ACK_MASK        ((0x1 << 3) \
					  |(0x1 << 4))
#define VPU_TOP_PROT_STEP1_0_MASK        ((0x1 << 6) \
					  |(0x1 << 7) \
					  |(0x1 << 8) \
					  |(0x1 << 9) \
					  |(0x1 << 12))
#define VPU_TOP_PROT_STEP1_0_ACK_MASK    ((0x1 << 6) \
					  |(0x1 << 7) \
					  |(0x1 << 8) \
					  |(0x1 << 9) \
					  |(0x1 << 12))
#define VPU_TOP_PROT_STEP2_0_MASK        ((0x1 << 27))
#define VPU_TOP_PROT_STEP2_0_ACK_MASK    ((0x1 << 27))
#define VPU_TOP_PROT_STEP2_1_MASK        ((0x1 << 10) \
					  |(0x1 << 11))
#define VPU_TOP_PROT_STEP2_1_ACK_MASK    ((0x1 << 10) \
					  |(0x1 << 11))
#define VPU_TOP_PROT_STEP2_2_MASK        ((0x1 << 5) \
					  |(0x1 << 6))
#define VPU_TOP_PROT_STEP2_2_ACK_MASK    ((0x1 << 5) \
					  |(0x1 << 6))
#define VPU_CORE0_PROT_STEP1_0_MASK      ((0x1 << 6))
#define VPU_CORE0_PROT_STEP1_0_ACK_MASK   ((0x1 << 6))
#define VPU_CORE0_PROT_STEP2_0_MASK      ((0x1 << 0) \
					  |(0x1 << 2) \
					  |(0x1 << 4))
#define VPU_CORE0_PROT_STEP2_0_ACK_MASK   ((0x1 << 0) \
					  |(0x1 << 2) \
					  |(0x1 << 4))
#define VPU_CORE1_PROT_STEP1_0_MASK      ((0x1 << 7))
#define VPU_CORE1_PROT_STEP1_0_ACK_MASK   ((0x1 << 7))
#define VPU_CORE1_PROT_STEP2_0_MASK      ((0x1 << 1) \
					  |(0x1 << 3) \
					  |(0x1 << 5))
#define VPU_CORE1_PROT_STEP2_0_ACK_MASK   ((0x1 << 1) \
					  |(0x1 << 3) \
					  |(0x1 << 5))
#define VDE_PROT_STEP2_0_MASK            ((0x1 << 7))
#define VDE_PROT_STEP2_0_ACK_MASK        ((0x1 << 7))

/* Define MTCMOS Power Status Mask */
#define MD1_PWR_STA_MASK                 (0x1 << 0)
#define CONN_PWR_STA_MASK                (0x1 << 1)
#define DPY_PWR_STA_MASK                 (0x1 << 2)
#define DIS_PWR_STA_MASK                 (0x1 << 3)
#define MFG_PWR_STA_MASK                 (0x1 << 4)
#define ISP_PWR_STA_MASK                 (0x1 << 5)
#define IFR_PWR_STA_MASK                 (0x1 << 6)
#define MFG_CORE0_PWR_STA_MASK           (0x1 << 7)
#define MP0_CPUTOP_PWR_STA_MASK          (0x1 << 8)
#define MP0_CPU0_PWR_STA_MASK            (0x1 << 9)
#define MP0_CPU1_PWR_STA_MASK            (0x1 << 10)
#define MP0_CPU2_PWR_STA_MASK            (0x1 << 11)
#define MP0_CPU3_PWR_STA_MASK            (0x1 << 12)
#define MFG_CORE1_PWR_STA_MASK           (0x1 << 20)
#define VEN_PWR_STA_MASK                 (0x1 << 21)
#define MFG_2D_PWR_STA_MASK              (0x1 << 22)
#define MFG_ASYNC_PWR_STA_MASK           (0x1 << 23)
#define AUDIO_PWR_STA_MASK               (0x1 << 24)
#define CAM_PWR_STA_MASK                 (0x1 << 25)
#define VPU_TOP_PWR_STA_MASK             (0x1 << 26)
#define VPU_CORE0_PWR_STA_MASK           (0x1 << 27)
#define VPU_CORE1_PWR_STA_MASK           (0x1 << 28)
#define VPU_CORE2_PWR_STA_MASK           (0x1 << 29)
#define VDE_PWR_STA_MASK                 (0x1 << 31)

/* Define CPU SRAM Mask */
#define VPU_CORE0_SRAM_SLEEP_B           (0x3 << 16)
#define VPU_CORE0_SRAM_SLEEP_B_ACK       (0x0 << 28)
#define VPU_CORE0_SRAM_SLEEP_B_ACK_BIT0   (0x1 << 28)
#define VPU_CORE0_SRAM_SLEEP_B_ACK_BIT1   (0x1 << 29)
#define VPU_CORE1_SRAM_SLEEP_B           (0x3 << 16)
#define VPU_CORE1_SRAM_SLEEP_B_ACK       (0x0 << 28)
#define VPU_CORE1_SRAM_SLEEP_B_ACK_BIT0   (0x1 << 28)
#define VPU_CORE1_SRAM_SLEEP_B_ACK_BIT1   (0x1 << 29)
#define VPU_CORE2_SRAM_SLEEP_B           (0x3 << 16)
#define VPU_CORE2_SRAM_SLEEP_B_ACK       (0x3 << 28)
#define VPU_CORE2_SRAM_SLEEP_B_ACK_BIT0   (0x1 << 28)
#define VPU_CORE2_SRAM_SLEEP_B_ACK_BIT1   (0x1 << 29)

/* Define Non-CPU SRAM Mask */
#define MD1_SRAM_PDN                     (0x1 << 8)
#define MD1_SRAM_PDN_ACK                 (0x0 << 12)
#define MD1_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define DPY_SRAM_PDN                     (0xF << 8)
#define DPY_SRAM_PDN_ACK                 (0xF << 12)
#define DPY_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define DPY_SRAM_PDN_ACK_BIT1            (0x1 << 13)
#define DPY_SRAM_PDN_ACK_BIT2            (0x1 << 14)
#define DPY_SRAM_PDN_ACK_BIT3            (0x1 << 15)
#define DIS_SRAM_PDN                     (0x1 << 8)
#define DIS_SRAM_PDN_ACK                 (0x1 << 12)
#define DIS_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define MFG_SRAM_PDN                     (0x1 << 8)
#define MFG_SRAM_PDN_ACK                 (0x1 << 12)
#define MFG_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define ISP_SRAM_PDN                     (0x3 << 8)
#define ISP_SRAM_PDN_ACK                 (0x3 << 12)
#define ISP_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define ISP_SRAM_PDN_ACK_BIT1            (0x1 << 13)
#define IFR_SRAM_PDN                     (0xF << 8)
#define IFR_SRAM_PDN_ACK                 (0xF << 12)
#define IFR_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define IFR_SRAM_PDN_ACK_BIT1            (0x1 << 13)
#define IFR_SRAM_PDN_ACK_BIT2            (0x1 << 14)
#define IFR_SRAM_PDN_ACK_BIT3            (0x1 << 15)
#define MFG_CORE0_SRAM_PDN               (0x1 << 8)
#define MFG_CORE0_SRAM_PDN_ACK           (0x1 << 12)
#define MFG_CORE0_SRAM_PDN_ACK_BIT0      (0x1 << 12)
#define MFG_CORE1_SRAM_PDN               (0x1 << 8)
#define MFG_CORE1_SRAM_PDN_ACK           (0x1 << 12)
#define MFG_CORE1_SRAM_PDN_ACK_BIT0      (0x1 << 12)
#define VEN_SRAM_PDN                     (0xF << 8)
#define VEN_SRAM_PDN_ACK                 (0xF << 12)
#define VEN_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define VEN_SRAM_PDN_ACK_BIT1            (0x1 << 13)
#define VEN_SRAM_PDN_ACK_BIT2            (0x1 << 14)
#define VEN_SRAM_PDN_ACK_BIT3            (0x1 << 15)
#define MFG_2D_SRAM_PDN                  (0x1 << 8)
#define MFG_2D_SRAM_PDN_ACK              (0x1 << 12)
#define MFG_2D_SRAM_PDN_ACK_BIT0         (0x1 << 12)
#define AUDIO_SRAM_PDN                   (0xF << 8)
#define AUDIO_SRAM_PDN_ACK               (0xF << 12)
#define AUDIO_SRAM_PDN_ACK_BIT0          (0x1 << 12)
#define AUDIO_SRAM_PDN_ACK_BIT1          (0x1 << 13)
#define AUDIO_SRAM_PDN_ACK_BIT2          (0x1 << 14)
#define AUDIO_SRAM_PDN_ACK_BIT3          (0x1 << 15)
#define CAM_SRAM_PDN                     (0x3 << 8)
#define CAM_SRAM_PDN_ACK                 (0x3 << 12)
#define CAM_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define CAM_SRAM_PDN_ACK_BIT1            (0x1 << 13)
#define VPU_TOP_SRAM_PDN                 (0x1 << 8)
#define VPU_TOP_SRAM_PDN_ACK             (0x1 << 12)
#define VPU_TOP_SRAM_PDN_ACK_BIT0        (0x1 << 12)
#define VPU_CORE0_SRAM_PDN               (0xF << 8)
#define VPU_CORE0_SRAM_PDN_ACK           (0x3 << 12)
#define VPU_CORE0_SRAM_PDN_ACK_BIT0      (0x1 << 12)
#define VPU_CORE0_SRAM_PDN_ACK_BIT1      (0x1 << 13)
#define VPU_CORE0_SRAM_PDN_ACK_BIT2      (0x1 << 14)
#define VPU_CORE0_SRAM_PDN_ACK_BIT3      (0x1 << 15)
#define VPU_CORE1_SRAM_PDN               (0xF << 8)
#define VPU_CORE1_SRAM_PDN_ACK           (0x3 << 12)
#define VPU_CORE1_SRAM_PDN_ACK_BIT0      (0x1 << 12)
#define VPU_CORE1_SRAM_PDN_ACK_BIT1      (0x1 << 13)
#define VPU_CORE1_SRAM_PDN_ACK_BIT2      (0x1 << 14)
#define VPU_CORE1_SRAM_PDN_ACK_BIT3      (0x1 << 15)
#define VPU_CORE2_SRAM_PDN               (0x3 << 8)
#define VPU_CORE2_SRAM_PDN_ACK           (0x3 << 12)
#define VPU_CORE2_SRAM_PDN_ACK_BIT0      (0x1 << 12)
#define VPU_CORE2_SRAM_PDN_ACK_BIT1      (0x1 << 13)
#define VDE_SRAM_PDN                     (0x1 << 8)
#define VDE_SRAM_PDN_ACK                 (0x1 << 12)
#define VDE_SRAM_PDN_ACK_BIT0            (0x1 << 12)



static struct subsys syss[] =	/* NR_SYSS *//* FIXME: set correct value */
{
	[SYS_MD1] = {
		     .name = __stringify(SYS_MD1),
		     .sta_mask = MD1_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
		     .sram_pdn_bits = MD1_SRAM_PDN,
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
	[SYS_VEN] = {
		     .name = __stringify(SYS_VEN),
		     .sta_mask = VEN_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
		     .sram_pdn_bits = 0,
		     .sram_pdn_ack_bits = 0,
		     .bus_prot_mask = 0,
		     .ops = &VEN_sys_ops,
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
	[SYS_AUDIO] = {
		       .name = __stringify(SYS_AUDIO),
		       .sta_mask = AUDIO_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
		       .sram_pdn_bits = 0,
		       .sram_pdn_ack_bits = 0,
		       .bus_prot_mask = 0,
		       .ops = &AUDIO_sys_ops,
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
	[SYS_MFG_CORE1] = {
			   .name = __stringify(SYS_MFG_CORE1),
			   .sta_mask = MFG_CORE1_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
			   .sram_pdn_bits = 0,
			   .sram_pdn_ack_bits = 0,
			   .bus_prot_mask = 0,
			   .ops = &MFG_CORE1_sys_ops,
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
	[SYS_VDE] = {
		     .name = __stringify(SYS_VDE),
		     .sta_mask = VDE_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
		     .sram_pdn_bits = 0,
		     .sram_pdn_ack_bits = 0,
		     .bus_prot_mask = 0,
		     .ops = &VDE_sys_ops,
		     },
	[SYS_VPU_TOP] = {
		     .name = __stringify(SYS_VPU_TOP),
		     .sta_mask = VPU_TOP_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
		     .sram_pdn_bits = 0,
		     .sram_pdn_ack_bits = 0,
		     .bus_prot_mask = 0,
		     .ops = &VPU_TOP_sys_ops,
		     },
	[SYS_VPU_CORE0_DORMANT] = {
		     .name = __stringify(SYS_VPU_CORE0_DORMANT),
		     .sta_mask = VPU_CORE0_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
		     .sram_pdn_bits = 0,
		     .sram_pdn_ack_bits = 0,
		     .bus_prot_mask = 0,
		     .ops = &VPU_CORE0_DORMANT_sys_ops,
		     },
	[SYS_VPU_CORE0_SHUTDOWN] = {
		     .name = __stringify(SYS_VPU_CORE0_SHUTDOWN),
		     .sta_mask = VPU_CORE0_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
		     .sram_pdn_bits = 0,
		     .sram_pdn_ack_bits = 0,
		     .bus_prot_mask = 0,
		     .ops = &VPU_CORE0_SHUTDOWN_sys_ops,
		     },
	[SYS_VPU_CORE1_DORMANT] = {
		     .name = __stringify(SYS_VPU_CORE1_DORMANT),
		     .sta_mask = VPU_CORE1_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
		     .sram_pdn_bits = 0,
		     .sram_pdn_ack_bits = 0,
		     .bus_prot_mask = 0,
		     .ops = &VPU_CORE1_DORMANT_sys_ops,
		     },
	[SYS_VPU_CORE1_SHUTDOWN] = {
		     .name = __stringify(SYS_VPU_CORE1_SHUTDOWN),
		     .sta_mask = VPU_CORE1_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
		     .sram_pdn_bits = 0,
		     .sram_pdn_ack_bits = 0,
		     .bus_prot_mask = 0,
		     .ops = &VPU_CORE1_SHUTDOWN_sys_ops,
		     },
	[SYS_VPU_CORE2_DORMANT] = {
		     .name = __stringify(SYS_VPU_CORE2_DORMANT),
		     .sta_mask = VPU_CORE2_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
		     .sram_pdn_bits = 0,
		     .sram_pdn_ack_bits = 0,
		     .bus_prot_mask = 0,
		     .ops = &VPU_CORE2_DORMANT_sys_ops,
		     },
	[SYS_VPU_CORE2_SHUTDOWN] = {
		     .name = __stringify(SYS_VPU_CORE2_SHUTDOWN),
		     .sta_mask = VPU_CORE2_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
		     .sram_pdn_bits = 0,
		     .sram_pdn_ack_bits = 0,
		     .bus_prot_mask = 0,
		     .ops = &VPU_CORE2_SHUTDOWN_sys_ops,
		     },
	[SYS_MFG_2D] = {
		     .name = __stringify(SYS_MFG_2D),
		     .sta_mask = MFG_2D_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
		     .sram_pdn_bits = 0,
		     .sram_pdn_ack_bits = 0,
		     .bus_prot_mask = 0,
		     .ops = &MFG_2D_sys_ops,
		     },
};

static struct pg_callbacks *g_pgcb;

struct pg_callbacks *register_pg_callback(struct pg_callbacks *pgcb)
{
	struct pg_callbacks *old_pgcb = g_pgcb;

	g_pgcb = pgcb;
	return old_pgcb;
}



static struct subsys *id_to_sys(unsigned int id)
{
	return id < NR_SYSS ? &syss[id] : NULL;
}

/* sync from mtcmos_ctrl.c  */
#ifdef CONFIG_MTK_RAM_CONSOLE
#if 0 /*add after early porting*/
static void aee_clk_data_rest(void)
{
	aee_rr_rec_clk(0, 0);
	aee_rr_rec_clk(1, 0);
	aee_rr_rec_clk(2, 0);
	aee_rr_rec_clk(3, 0);
}
#endif
#endif

/* auto-gen begin*/

int spm_mtcmos_ctrl_md1(int state)
{
	int err = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MD1" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, MD1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) &
			MD1_PROT_STEP1_0_ACK_MASK) !=
			MD1_PROT_STEP1_0_ACK_MASK) {
		}
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, MD1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) &
			MD1_PROT_STEP2_0_ACK_MASK) !=
			MD1_PROT_STEP2_0_ACK_MASK) {
		}
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET, MD1_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1) &
			MD1_PROT_STEP2_1_ACK_MASK) !=
			MD1_PROT_STEP2_1_ACK_MASK) {
		}
#endif
		/* TINFO="MD_EXTRA_PWR_CON[0]=1"*/
		spm_write(MD_EXTRA_PWR_CON,
			spm_read(MD_EXTRA_PWR_CON) | (0x1 << 0));
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) | PWR_ISO);
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
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
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
			!= MD1_PWR_STA_MASK) || ((spm_read(PWR_STATUS_2ND)
			& MD1_PWR_STA_MASK) != MD1_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
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
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, MD1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR, MD1_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, MD1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Finish to turn on MD1" */
	}
	return err;
}

int spm_mtcmos_ctrl_conn(int state)
{
	int err = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off CONN" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, CONN_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) &
			CONN_PROT_STEP1_0_ACK_MASK) !=
			CONN_PROT_STEP1_0_ACK_MASK) {
		}
#endif
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & CONN_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & CONN_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Finish to turn off CONN" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on CONN" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & CONN_PWR_STA_MASK)
			!= CONN_PWR_STA_MASK) || ((spm_read(PWR_STATUS_2ND)
			& CONN_PWR_STA_MASK) != CONN_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) | PWR_RST_B);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, CONN_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Finish to turn on CONN" */
	}
	return err;
}

void enable_mm_clk(void)
{
	clk_writel(MM_CG_CLR0, 0x03ff);
}

int spm_mtcmos_ctrl_dis(int state)
{
	int err = 0;
	int retry = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off DIS" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET, DIS_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1) &
			DIS_PROT_STEP1_0_ACK_MASK) !=
			DIS_PROT_STEP1_0_ACK_MASK) {
			retry++;
			if (retry == 5000) {
				pr_notice("INFRA_TOPAXI_PROTECTEN_1 = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN_1));
				pr_notice("INFRA_TOPAXI_PROTECTEN_STA1_1 = %08x\n",
				spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1));
				pr_notice("PWR_STATUS = %08x, %08x\n",
					spm_read(PWR_STATUS),
					spm_read(PWR_STATUS_2ND));
				pr_notice("MM_CG_CON0 = %08x\n",
					spm_read(MM_CG_CON0));
				pr_notice("INFRA_TOPAXI_PROTECTEN = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN));
				pr_notice("INFRA_TOPAXI_PROTECTEN_STA1 = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN_STA1));
				pr_notice("INFRA_TOPAXI_PROTECTSTA0_1 = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTSTA0_1));
				/*BUG_ON(1);*/
				break;
			}
		}
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, DIS_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		retry = 0;
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) &
			DIS_PROT_STEP2_0_ACK_MASK) !=
			DIS_PROT_STEP2_0_ACK_MASK) {
			retry++;
			if (retry == 5000) {
				pr_notice("INFRA_TOPAXI_PROTECTEN_1 = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN_1));
				pr_notice("INFRA_TOPAXI_PROTECTEN_STA1_1 = %08x\n",
				spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1));
				pr_notice("PWR_STATUS = %08x, %08x\n",
					spm_read(PWR_STATUS),
					spm_read(PWR_STATUS_2ND));
				pr_notice("MM_CG_CON0 = %08x\n",
					spm_read(MM_CG_CON0));
				pr_notice("INFRA_TOPAXI_PROTECTEN = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN));
				pr_notice("INFRA_TOPAXI_PROTECTEN_STA1 = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN_STA1));
				pr_notice("INFRA_TOPAXI_PROTECTSTA0_1 = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTSTA0_1));
				/*BUG_ON(1);*/
				break;
			}
		}
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(SMI_COMMON_SMI_CLAMP_SET, DIS_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		retry = 0;
		while ((spm_read(SMI_COMMON_SMI_CLAMP) &
			DIS_PROT_STEP2_1_ACK_MASK) !=
			DIS_PROT_STEP2_1_ACK_MASK) {
			retry++;
			if (retry == 5000) {
				pr_notice("INFRA_TOPAXI_PROTECTEN_1 = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN_1));
				pr_notice("INFRA_TOPAXI_PROTECTEN_STA1_1 = %08x\n",
				spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1));
				pr_notice("PWR_STATUS = %08x, %08x\n",
					spm_read(PWR_STATUS),
					spm_read(PWR_STATUS_2ND));
				pr_notice("MM_CG_CON0 = %08x\n",
					spm_read(MM_CG_CON0));
				pr_notice("INFRA_TOPAXI_PROTECTEN = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN));
				pr_notice("INFRA_TOPAXI_PROTECTEN_STA1 = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN_STA1));
				pr_notice("INFRA_TOPAXI_PROTECTSTA0_1 = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTSTA0_1));
				break;
			}
		}
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) | DIS_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DIS_SRAM_PDN_ACK = 1" */
		while ((spm_read(DIS_PWR_CON) & DIS_SRAM_PDN_ACK) !=
			DIS_SRAM_PDN_ACK) {
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
#endif
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
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
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
			!= DIS_PWR_STA_MASK) || ((spm_read(PWR_STATUS_2ND)
			& DIS_PWR_STA_MASK) != DIS_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DIS_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(DIS_PWR_CON) & DIS_SRAM_PDN_ACK_BIT0) {
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, DIS_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(SMI_COMMON_SMI_CLAMP_CLR, DIS_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR, DIS_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Finish to turn on DIS" */
		enable_mm_clk();
	}
	return err;
}

int spm_mtcmos_ctrl_mfg(int state)
{
	int err = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG" */
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) | MFG_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG_SRAM_PDN_ACK = 1" */
		while ((spm_read(MFG_PWR_CON) & MFG_SRAM_PDN_ACK)
			!= MFG_SRAM_PDN_ACK) {
			/* Need f_fmfg_core_ck for SRAM PDN delay IP. */
			/* Need f_fmfg_core_ck for SRAM PDN delay IP. */
		}
#endif
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
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
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
			!= MFG_PWR_STA_MASK) || ((spm_read(PWR_STATUS_2ND)
			& MFG_PWR_STA_MASK) != MFG_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MFG_PWR_CON, spm_read(MFG_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(MFG_PWR_CON) & MFG_SRAM_PDN_ACK_BIT0) {
			/* Need f_fmfg_core_ck for SRAM PDN delay IP. */
			/* Need f_fmfg_core_ck for SRAM PDN delay IP. */
		}
#endif
		/* TINFO="Finish to turn on MFG" */
	}
	return err;
}

void enable_img_clk(void)
{
	clk_writel(IMG_CG_CLR, 0x03);
}

int spm_mtcmos_ctrl_isp(int state)
{
	int err = 0;
	int retry = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off ISP" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET, ISP_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
			ISP_PROT_STEP1_0_ACK_MASK) !=
			ISP_PROT_STEP1_0_ACK_MASK) {
			retry++;
			if (retry == 5000) {
				pr_notice("INFRA_TOPAXI_PROTECTEN_MM = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN_MM));
				pr_notice("INFRA_TOPAXI_PROTECTEN_MM_STA1 = %08x\n",
				spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1));
				pr_notice("PWR_STATUS = %08x, %08x\n",
					spm_read(PWR_STATUS),
					spm_read(PWR_STATUS_2ND));
				pr_notice("IMG_CG_CON = %08x, MM_CG_CON0 = %08x\n",
					spm_read(IMG_CG_CON),
					spm_read(MM_CG_CON0));
				pr_notice("INFRA_TOPAXI_PROTECTEN = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN));
				pr_notice("INFRA_TOPAXI_PROTECTEN_STA1 = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN_STA1));
				/*BUG_ON(1);*/
				break;
			}
		}
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET, ISP_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		retry = 0;
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
			ISP_PROT_STEP2_0_ACK_MASK) !=
			ISP_PROT_STEP2_0_ACK_MASK) {
			retry++;
			if (retry == 5000) {
				pr_notice("INFRA_TOPAXI_PROTECTEN_MM = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN_MM));
				pr_notice("INFRA_TOPAXI_PROTECTEN_MM_STA1 = %08x\n",
				spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1));
				pr_notice("PWR_STATUS = %08x, %08x\n",
					spm_read(PWR_STATUS),
					spm_read(PWR_STATUS_2ND));
				pr_notice("IMG_CG_CON = %08x, MM_CG_CON0 = %08x\n",
					spm_read(IMG_CG_CON),
					spm_read(MM_CG_CON0));
				pr_notice("INFRA_TOPAXI_PROTECTEN = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN));
				pr_notice("INFRA_TOPAXI_PROTECTEN_STA1 = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN_STA1));
				/*BUG_ON(1);*/
				break;
			}
		}
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(SMI_COMMON_SMI_CLAMP_SET, ISP_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		retry = 0;
		while ((spm_read(SMI_COMMON_SMI_CLAMP) &
			ISP_PROT_STEP2_1_ACK_MASK) !=
			ISP_PROT_STEP2_1_ACK_MASK) {
			retry++;
			if (retry == 5000) {
				pr_notice("INFRA_TOPAXI_PROTECTEN_MM = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN_MM));
				pr_notice("INFRA_TOPAXI_PROTECTEN_MM_STA1 = %08x\n",
				spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1));
				pr_notice("PWR_STATUS = %08x, %08x\n",
					spm_read(PWR_STATUS),
					spm_read(PWR_STATUS_2ND));
				pr_notice("IMG_CG_CON = %08x, MM_CG_CON0 = %08x\n",
					spm_read(IMG_CG_CON),
					spm_read(MM_CG_CON0));
				pr_notice("INFRA_TOPAXI_PROTECTEN = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN));
				pr_notice("INFRA_TOPAXI_PROTECTEN_STA1 = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN_STA1));
				pr_notice("SMI_COMMON_SMI_CLAMP = %08x\n",
					spm_read(SMI_COMMON_SMI_CLAMP));
				break;
			}
		}
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) | ISP_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ISP_SRAM_PDN_ACK = 1" */
		while ((spm_read(ISP_PWR_CON) &
			ISP_SRAM_PDN_ACK) !=
			ISP_SRAM_PDN_ACK) {
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
#endif
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
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
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
			!= ISP_PWR_STA_MASK) || ((spm_read(PWR_STATUS_2ND)
			& ISP_PWR_STA_MASK) != ISP_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ISP_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(ISP_PWR_CON) & ISP_SRAM_PDN_ACK_BIT0) {
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
#endif
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) & ~(0x1 << 9));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ISP_SRAM_PDN_ACK_BIT1 = 0" */
		while (spm_read(ISP_PWR_CON) & ISP_SRAM_PDN_ACK_BIT1) {
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR, ISP_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(SMI_COMMON_SMI_CLAMP_CLR, ISP_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR, ISP_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Finish to turn on ISP" */
		enable_img_clk();
	}
	return err;
}

int spm_mtcmos_ctrl_mfg_core0(int state)
{
	int err = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG_CORE0" */
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MFG_CORE0_PWR_CON,
			spm_read(MFG_CORE0_PWR_CON) | MFG_CORE0_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG_CORE0_SRAM_PDN_ACK = 1" */
		while ((spm_read(MFG_CORE0_PWR_CON) & MFG_CORE0_SRAM_PDN_ACK)
			!= MFG_CORE0_SRAM_PDN_ACK) {
			/*  */
			/*  */
		}
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
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
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
			!= MFG_CORE0_PWR_STA_MASK) || ((spm_read(PWR_STATUS_2ND)
			& MFG_CORE0_PWR_STA_MASK) != MFG_CORE0_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
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
		while (spm_read(MFG_CORE0_PWR_CON) &
			MFG_CORE0_SRAM_PDN_ACK_BIT0) {
			/*  */
			/*  */
		}
#endif
		/* TINFO="Finish to turn on MFG_CORE0" */
	}
	return err;
}

int spm_mtcmos_ctrl_mfg_core1(int state)
{
	int err = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG_CORE1" */
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MFG_CORE1_PWR_CON,
			spm_read(MFG_CORE1_PWR_CON) | MFG_CORE1_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG_CORE1_SRAM_PDN_ACK = 1" */
		while ((spm_read(MFG_CORE1_PWR_CON) & MFG_CORE1_SRAM_PDN_ACK)
			!= MFG_CORE1_SRAM_PDN_ACK) {
			/*  */
			/*  */
		}
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MFG_CORE1_PWR_CON,
			spm_read(MFG_CORE1_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MFG_CORE1_PWR_CON,
			spm_read(MFG_CORE1_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MFG_CORE1_PWR_CON,
			spm_read(MFG_CORE1_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MFG_CORE1_PWR_CON,
			spm_read(MFG_CORE1_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MFG_CORE1_PWR_CON,
			spm_read(MFG_CORE1_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MFG_CORE1_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MFG_CORE1_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Finish to turn off MFG_CORE1" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MFG_CORE1" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MFG_CORE1_PWR_CON,
			spm_read(MFG_CORE1_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MFG_CORE1_PWR_CON,
			spm_read(MFG_CORE1_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & MFG_CORE1_PWR_STA_MASK)
			!= MFG_CORE1_PWR_STA_MASK) || ((spm_read(PWR_STATUS_2ND)
			& MFG_CORE1_PWR_STA_MASK) != MFG_CORE1_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG_CORE1_PWR_CON,
			spm_read(MFG_CORE1_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG_CORE1_PWR_CON,
			spm_read(MFG_CORE1_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG_CORE1_PWR_CON,
			spm_read(MFG_CORE1_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MFG_CORE1_PWR_CON,
			spm_read(MFG_CORE1_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG_CORE1_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(MFG_CORE1_PWR_CON) &
			MFG_CORE1_SRAM_PDN_ACK_BIT0) {
			/*  */
			/*  */
		}
#endif
		/* TINFO="Finish to turn on MFG_CORE1" */
	}
	return err;
}

int spm_mtcmos_ctrl_ven(int state)
{
	int err = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VEN" */
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(SMI_COMMON_SMI_CLAMP_SET, VEN_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(SMI_COMMON_SMI_CLAMP) &
			VEN_PROT_STEP2_0_ACK_MASK) !=
			VEN_PROT_STEP2_0_ACK_MASK) {
		}
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) | VEN_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VEN_SRAM_PDN_ACK = 1" */
		while ((spm_read(VEN_PWR_CON) & VEN_SRAM_PDN_ACK) !=
			VEN_SRAM_PDN_ACK) {
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & VEN_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & VEN_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Finish to turn off VEN" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on VEN" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & VEN_PWR_STA_MASK)
			!= VEN_PWR_STA_MASK) || ((spm_read(PWR_STATUS_2ND)
			& VEN_PWR_STA_MASK) != VEN_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VEN_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(VEN_PWR_CON) & VEN_SRAM_PDN_ACK_BIT0) {
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
#endif
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) & ~(0x1 << 9));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VEN_SRAM_PDN_ACK_BIT1 = 0" */
		while (spm_read(VEN_PWR_CON) & VEN_SRAM_PDN_ACK_BIT1) {
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
#endif
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) & ~(0x1 << 10));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VEN_SRAM_PDN_ACK_BIT2 = 0" */
		while (spm_read(VEN_PWR_CON) & VEN_SRAM_PDN_ACK_BIT2) {
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
#endif
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) & ~(0x1 << 11));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VEN_SRAM_PDN_ACK_BIT3 = 0" */
		while (spm_read(VEN_PWR_CON) & VEN_SRAM_PDN_ACK_BIT3) {
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(SMI_COMMON_SMI_CLAMP_CLR, VEN_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Finish to turn on VEN" */
	}
	return err;
}

void dump_mfg_sts(void)
{
	pr_notice("MFG_DEBUG_17C = %08x\n",
		spm_read(MFG_DEBUG_17C));
	pr_notice("MFG_DEBUG_SEL = %08x\n",
		spm_read(MFG_DEBUG_SEL));
	pr_notice("MFG_DEBUG_LATCH = %08x\n",
		spm_read(MFG_DEBUG_LATCH));
	pr_notice("MFG_DEBUG_TOP = %08x\n",
		spm_read(MFG_DEBUG_TOP));
	pr_notice("MFG_DEBUG_ASYNC = %08x\n",
		spm_read(MFG_DEBUG_ASYNC));
}

void gpu_dump(void)
{
	spm_write(MFG_DEBUG_SEL, 0x10);
	spm_write(MFG_DEBUG_LATCH, 0xff);
	dump_mfg_sts();
	spm_write(MFG_DEBUG_SEL, 0x11);
	spm_write(MFG_DEBUG_LATCH, 0xff);
	dump_mfg_sts();
	spm_write(MFG_DEBUG_SEL, 0x12);
	spm_write(MFG_DEBUG_LATCH, 0xff);
	dump_mfg_sts();
	spm_write(MFG_DEBUG_SEL, 0x13);
	spm_write(MFG_DEBUG_LATCH, 0xff);
	dump_mfg_sts();
	spm_write(MFG_DEBUG_SEL, 0x100000);
	spm_write(MFG_DEBUG_LATCH, 0xff00);
	dump_mfg_sts();
	spm_write(MFG_DEBUG_SEL, 0x110000);
	spm_write(MFG_DEBUG_LATCH, 0xff00);
	dump_mfg_sts();
	spm_write(MFG_DEBUG_SEL, 0x80000);
	spm_write(MFG_DEBUG_LATCH, 0x0);
	dump_mfg_sts();
	spm_write(MFG_DEBUG_SEL, 0x90000);
	spm_write(MFG_DEBUG_LATCH, 0x0);
	dump_mfg_sts();
	pr_notice("SUBSYS_IDLE_STA = %08x\n",
		spm_read(SUBSYS_IDLE_STA));
	pr_notice("INFRA_BUS_IDLE_STA5 = %08x\n",
		spm_read(INFRA_BUS_IDLE_STA5));
	pr_notice("INFRABUS_DBG1 = %08x\n",
		spm_read(INFRABUS_DBG1));
	pr_notice("INFRABUS_DBG15 = %08x\n",
		spm_read(INFRABUS_DBG15));
	pr_notice("INFRABUS_DBG17 = %08x\n",
		spm_read(INFRABUS_DBG17));
	pr_notice("INFRABUS_DBG21 = %08x\n",
		spm_read(INFRABUS_DBG21));
	pr_notice("INFRA_TOPAXI_PROTECTEN = %08x\n",
		spm_read(INFRA_TOPAXI_PROTECTEN));
	pr_notice("INFRA_TOPAXI_PROTECTEN_STA1 = %08x\n",
		spm_read(INFRA_TOPAXI_PROTECTEN_STA1));
	pr_notice("INFRA_TOPAXI_PROTECTEN_1 = %08x\n",
		spm_read(INFRA_TOPAXI_PROTECTEN_1));
	pr_notice("INFRA_TOPAXI_PROTECTEN_STA1_1 = %08x\n",
		spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1));
	pr_notice("INFRA_TOPAXI_PROTECTSTA0_1 = %08x\n",
		spm_read(INFRA_TOPAXI_PROTECTSTA0_1));
}


int spm_mtcmos_ctrl_mfg_2d(int state)
{
	int err = 0;
	int retry = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET, MFG_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1) &
			MFG_PROT_STEP1_0_ACK_MASK) !=
			MFG_PROT_STEP1_0_ACK_MASK) {
			retry++;
			if (retry == 5000) {
				/*mt_gpufreq_dump_reg();*/
				gpu_dump();
				/*mt_gpufreq_dump_DVFS_status();*/
				BUG_ON(1);
			}
		}
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, MFG_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) &
			MFG_PROT_STEP2_0_ACK_MASK) !=
			MFG_PROT_STEP2_0_ACK_MASK) {
		}
#endif
		/* TINFO="Start to turn off MFG_2D" */
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MFG_2D_PWR_CON,
			spm_read(MFG_2D_PWR_CON) | MFG_2D_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG_2D_SRAM_PDN_ACK = 1" */
		while ((spm_read(MFG_2D_PWR_CON) & MFG_2D_SRAM_PDN_ACK)
			!= MFG_2D_SRAM_PDN_ACK) {
			/*  */
			/*  */
		}
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MFG_2D_PWR_CON,
			spm_read(MFG_2D_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MFG_2D_PWR_CON,
			spm_read(MFG_2D_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MFG_2D_PWR_CON,
			spm_read(MFG_2D_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MFG_2D_PWR_CON,
			spm_read(MFG_2D_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MFG_2D_PWR_CON,
			spm_read(MFG_2D_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MFG_2D_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MFG_2D_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Finish to turn off MFG_2D" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MFG_2D" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MFG_2D_PWR_CON,
			spm_read(MFG_2D_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MFG_2D_PWR_CON,
			spm_read(MFG_2D_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & MFG_2D_PWR_STA_MASK)
			!= MFG_2D_PWR_STA_MASK) || ((spm_read(PWR_STATUS_2ND)
			& MFG_2D_PWR_STA_MASK) != MFG_2D_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG_2D_PWR_CON,
			spm_read(MFG_2D_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG_2D_PWR_CON,
			spm_read(MFG_2D_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG_2D_PWR_CON,
			spm_read(MFG_2D_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MFG_2D_PWR_CON,
			spm_read(MFG_2D_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG_2D_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(MFG_2D_PWR_CON) & MFG_2D_SRAM_PDN_ACK_BIT0) {
				/*  */
				/*  */
		}
#endif
		/* TINFO="Finish to turn on MFG_2D" */
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, MFG_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR, MFG_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
	}
	return err;
}

int spm_mtcmos_ctrl_mfg_async(int state)
{
	int err = 0;

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
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
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
			!= MFG_ASYNC_PWR_STA_MASK) || ((spm_read(PWR_STATUS_2ND)
			& MFG_ASYNC_PWR_STA_MASK) != MFG_ASYNC_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
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
	return err;
}

int spm_mtcmos_ctrl_audio(int state)
{
	int err = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off AUDIO" */
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(AUDIO_PWR_CON,
			spm_read(AUDIO_PWR_CON) | AUDIO_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until AUDIO_SRAM_PDN_ACK = 1" */
		while ((spm_read(AUDIO_PWR_CON) & AUDIO_SRAM_PDN_ACK)
			!= AUDIO_SRAM_PDN_ACK) {
			/* Need f_f26M_aud_ck for SRAM PDN delay IP. */
			/* Need f_f26M_aud_ck for SRAM PDN delay IP. */
		}
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & AUDIO_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & AUDIO_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Finish to turn off AUDIO" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on AUDIO" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & AUDIO_PWR_STA_MASK)
			!= AUDIO_PWR_STA_MASK) || ((spm_read(PWR_STATUS_2ND)
			& AUDIO_PWR_STA_MASK) != AUDIO_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(AUDIO_PWR_CON,
			spm_read(AUDIO_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until AUDIO_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(AUDIO_PWR_CON) & AUDIO_SRAM_PDN_ACK_BIT0) {
			/* Need f_f26M_aud_ck for SRAM PDN delay IP. */
			/* Need f_f26M_aud_ck for SRAM PDN delay IP. */
		}
#endif
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~(0x1 << 9));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until AUDIO_SRAM_PDN_ACK_BIT1 = 0" */
		while (spm_read(AUDIO_PWR_CON) & AUDIO_SRAM_PDN_ACK_BIT1) {
			/* Need f_f26M_aud_ck for SRAM PDN delay IP. */
			/* Need f_f26M_aud_ck for SRAM PDN delay IP. */
		}
#endif
		spm_write(AUDIO_PWR_CON,
			spm_read(AUDIO_PWR_CON) & ~(0x1 << 10));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until AUDIO_SRAM_PDN_ACK_BIT2 = 0" */
		while (spm_read(AUDIO_PWR_CON) & AUDIO_SRAM_PDN_ACK_BIT2) {
			/* Need f_f26M_aud_ck for SRAM PDN delay IP. */
			/* Need f_f26M_aud_ck for SRAM PDN delay IP. */
		}
#endif
		spm_write(AUDIO_PWR_CON,
			spm_read(AUDIO_PWR_CON) & ~(0x1 << 11));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until AUDIO_SRAM_PDN_ACK_BIT3 = 0" */
		while (spm_read(AUDIO_PWR_CON) & AUDIO_SRAM_PDN_ACK_BIT3) {
			/* Need f_f26M_aud_ck for SRAM PDN delay IP. */
			/* Need f_f26M_aud_ck for SRAM PDN delay IP. */
		}
#endif
		/* TINFO="Finish to turn on AUDIO" */
	}
	return err;
}

void enable_cam_clk(void)
{
	#if 0
	clk_writel(CAM_CG_CLR, 0x1fff);
	clk_writel(CAM_CG_CLR, 0x00ff);
	clk_writel(CAM_CG_CLR, 0x0105);
	clk_writel(CAM_CG_CLR, 0x01ff);
	#endif
	clk_writel(CAM_CG_CLR, 0x1f05);
}
int spm_mtcmos_ctrl_cam(int state)
{
	int err = 0;
	int retry = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off CAM" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
			CAM_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
			CAM_PROT_STEP1_0_ACK_MASK) !=
			CAM_PROT_STEP1_0_ACK_MASK) {
			retry++;
			if (retry == 5000) {
				pr_notice("INFRA_TOPAXI_PROTECTEN_MM = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN_MM));
				pr_notice("INFRA_TOPAXI_PROTECTEN_MM_STA1 = %08x\n",
				spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1));
				pr_notice("PWR_STATUS = %08x, %08x\n",
					spm_read(PWR_STATUS),
					spm_read(PWR_STATUS_2ND));
				pr_notice("CAM_CG_CON = %08x, MM_CG_CON0 = %08x\n",
					spm_read(CAM_CG_CON),
					spm_read(MM_CG_CON0));
				pr_notice("INFRA_TOPAXI_PROTECTEN = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN));
				pr_notice("INFRA_TOPAXI_PROTECTEN_STA1 = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN_STA1));
				/*BUG_ON(1);*/
				break;
			}
		}
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, CAM_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		retry = 0;
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) &
			CAM_PROT_STEP2_0_ACK_MASK) !=
			CAM_PROT_STEP2_0_ACK_MASK) {
			retry++;
			if (retry == 5000) {
				pr_notice("INFRA_TOPAXI_PROTECTEN_MM = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN_MM));
				pr_notice("INFRA_TOPAXI_PROTECTEN_MM_STA1 = %08x\n",
				spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1));
				pr_notice("PWR_STATUS = %08x, %08x\n",
					spm_read(PWR_STATUS),
					spm_read(PWR_STATUS_2ND));
				pr_notice("CAM_CG_CON = %08x, MM_CG_CON0 = %08x\n",
					spm_read(CAM_CG_CON),
					spm_read(MM_CG_CON0));
				pr_notice("INFRA_TOPAXI_PROTECTEN = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN));
				pr_notice("INFRA_TOPAXI_PROTECTEN_STA1 = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN_STA1));
				/*BUG_ON(1);*/
				break;
			}
		}
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET, CAM_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		retry = 0;
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
			CAM_PROT_STEP2_1_ACK_MASK) !=
			CAM_PROT_STEP2_1_ACK_MASK) {
			retry++;
			if (retry == 5000) {
				pr_notice("INFRA_TOPAXI_PROTECTEN_MM = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN_MM));
				pr_notice("INFRA_TOPAXI_PROTECTEN_MM_STA1 = %08x\n",
				spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1));
				pr_notice("PWR_STATUS = %08x, %08x\n",
					spm_read(PWR_STATUS),
					spm_read(PWR_STATUS_2ND));
				pr_notice("CAM_CG_CON = %08x, MM_CG_CON0 = %08x\n",
					spm_read(CAM_CG_CON),
					spm_read(MM_CG_CON0));
				pr_notice("INFRA_TOPAXI_PROTECTEN = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN));
				pr_notice("INFRA_TOPAXI_PROTECTEN_STA1 = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN_STA1));
				/*BUG_ON(1);*/
				break;
			}
		}
#endif
		/* TINFO="Set bus protect - step2 : 2" */
		spm_write(SMI_COMMON_SMI_CLAMP_SET, CAM_PROT_STEP2_2_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		retry = 0;
		while ((spm_read(SMI_COMMON_SMI_CLAMP) &
			CAM_PROT_STEP2_2_ACK_MASK) !=
			CAM_PROT_STEP2_2_ACK_MASK) {
			retry++;
			if (retry == 5000) {
				pr_notice("INFRA_TOPAXI_PROTECTEN_MM = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN_MM));
				pr_notice("INFRA_TOPAXI_PROTECTEN_MM_STA1 = %08x\n",
				spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1));
				pr_notice("PWR_STATUS = %08x, %08x\n",
					spm_read(PWR_STATUS),
					spm_read(PWR_STATUS_2ND));
				pr_notice("CAM_CG_CON = %08x, MM_CG_CON0 = %08x\n",
					spm_read(CAM_CG_CON),
					spm_read(MM_CG_CON0));
				pr_notice("INFRA_TOPAXI_PROTECTEN = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN));
				pr_notice("INFRA_TOPAXI_PROTECTEN_STA1 = %08x\n",
					spm_read(INFRA_TOPAXI_PROTECTEN_STA1));
				pr_notice("SMI_COMMON_SMI_CLAMP = %08x\n",
					spm_read(SMI_COMMON_SMI_CLAMP));
				break;
			}
		}
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(CAM_PWR_CON, spm_read(CAM_PWR_CON) | CAM_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until CAM_SRAM_PDN_ACK = 1" */
		while ((spm_read(CAM_PWR_CON) & CAM_SRAM_PDN_ACK) !=
			CAM_SRAM_PDN_ACK) {
			/*  */
			/*  */
		}
#endif
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
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
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
			/* No logic between pwr_on and pwr_ack. */
			/*Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(CAM_PWR_CON, spm_read(CAM_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(CAM_PWR_CON, spm_read(CAM_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(CAM_PWR_CON, spm_read(CAM_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(CAM_PWR_CON, spm_read(CAM_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until CAM_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(CAM_PWR_CON) & CAM_SRAM_PDN_ACK_BIT0) {
				/*  */
				/*  */
		}
#endif
		spm_write(CAM_PWR_CON, spm_read(CAM_PWR_CON) & ~(0x1 << 9));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until CAM_SRAM_PDN_ACK_BIT1 = 0" */
		while (spm_read(CAM_PWR_CON) & CAM_SRAM_PDN_ACK_BIT1) {
				/*  */
				/*  */
		}
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, CAM_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR, CAM_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Release bus protect - step2 : 2" */
		spm_write(SMI_COMMON_SMI_CLAMP_CLR, CAM_PROT_STEP2_2_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR, CAM_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Finish to turn on CAM" */
		enable_cam_clk();
	}
	return err;
}

int spm_mtcmos_ctrl_vpu_top(int state)
{
	int err = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VPU_TOP" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
			VPU_TOP_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
			VPU_TOP_PROT_STEP1_0_ACK_MASK) !=
			VPU_TOP_PROT_STEP1_0_ACK_MASK) {
		}
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET,
			VPU_TOP_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) &
			VPU_TOP_PROT_STEP2_0_ACK_MASK) !=
			VPU_TOP_PROT_STEP2_0_ACK_MASK) {
		}
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
			VPU_TOP_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
			VPU_TOP_PROT_STEP2_1_ACK_MASK) !=
			VPU_TOP_PROT_STEP2_1_ACK_MASK) {
		}
#endif
		/* TINFO="Set bus protect - step2 : 2" */
		spm_write(SMI_COMMON_SMI_CLAMP_SET, VPU_TOP_PROT_STEP2_2_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(SMI_COMMON_SMI_CLAMP) &
			VPU_TOP_PROT_STEP2_2_ACK_MASK) !=
			VPU_TOP_PROT_STEP2_2_ACK_MASK) {
		}
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(VPU_TOP_PWR_CON,
			spm_read(VPU_TOP_PWR_CON) | VPU_TOP_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_TOP_SRAM_PDN_ACK = 1" */
		while ((spm_read(VPU_TOP_PWR_CON) & VPU_TOP_SRAM_PDN_ACK)
			!= VPU_TOP_SRAM_PDN_ACK) {
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
		}
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(VPU_TOP_PWR_CON,
			spm_read(VPU_TOP_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(VPU_TOP_PWR_CON,
			spm_read(VPU_TOP_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(VPU_TOP_PWR_CON,
			spm_read(VPU_TOP_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(VPU_TOP_PWR_CON,
			spm_read(VPU_TOP_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(VPU_TOP_PWR_CON,
			spm_read(VPU_TOP_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & VPU_TOP_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & VPU_TOP_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Finish to turn off VPU_TOP" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on VPU_TOP" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(VPU_TOP_PWR_CON,
			spm_read(VPU_TOP_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(VPU_TOP_PWR_CON,
			spm_read(VPU_TOP_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & VPU_TOP_PWR_STA_MASK)
			!= VPU_TOP_PWR_STA_MASK) || ((spm_read(PWR_STATUS_2ND)
			& VPU_TOP_PWR_STA_MASK) != VPU_TOP_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(VPU_TOP_PWR_CON,
			spm_read(VPU_TOP_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(VPU_TOP_PWR_CON,
			spm_read(VPU_TOP_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(VPU_TOP_PWR_CON,
			spm_read(VPU_TOP_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(VPU_TOP_PWR_CON,
			spm_read(VPU_TOP_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_TOP_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(VPU_TOP_PWR_CON) & VPU_TOP_SRAM_PDN_ACK_BIT0) {
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
		}
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR,
			VPU_TOP_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
			VPU_TOP_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Release bus protect - step2 : 2" */
		spm_write(SMI_COMMON_SMI_CLAMP_CLR,
			VPU_TOP_PROT_STEP2_2_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
			VPU_TOP_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Finish to turn on VPU_TOP" */
	}
	return err;
}

int spm_mtcmos_ctrl_vpu_core0_dormant(int state)
{
	int err = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VPU_CORE0" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_SET,
			VPU_CORE0_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MCU_STA1) &
			VPU_CORE0_PROT_STEP1_0_ACK_MASK) !=
			VPU_CORE0_PROT_STEP1_0_ACK_MASK) {
		}
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_SET,
			VPU_CORE0_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MCU_STA1) &
			VPU_CORE0_PROT_STEP2_0_ACK_MASK) !=
			VPU_CORE0_PROT_STEP2_0_ACK_MASK) {
		}
#endif
		/* TINFO="Set SRAM_CKISO = 1" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) | SRAM_CKISO);
		/* TINFO="Set SRAM_ISOINT_B = 0" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) & ~SRAM_ISOINT_B);
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) | VPU_CORE0_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE0_SRAM_PDN_ACK = 1" */
		while ((spm_read(VPU_CORE0_PWR_CON) & VPU_CORE0_SRAM_PDN_ACK)
			!= VPU_CORE0_SRAM_PDN_ACK) {
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
		}
#endif
		/* TINFO="Set SRAM_SLEEP_B = 0" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) & ~VPU_CORE0_SRAM_SLEEP_B);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & VPU_CORE0_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND)
			& VPU_CORE0_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Finish to turn off VPU_CORE0" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on VPU_CORE0" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & VPU_CORE0_PWR_STA_MASK)
			!= VPU_CORE0_PWR_STA_MASK) || ((spm_read(PWR_STATUS_2ND)
			& VPU_CORE0_PWR_STA_MASK) != VPU_CORE0_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE0_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(VPU_CORE0_PWR_CON) &
			VPU_CORE0_SRAM_PDN_ACK_BIT0) {
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
		}
#endif
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) & ~(0x1 << 9));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE0_SRAM_PDN_ACK_BIT1 = 0" */
		while (spm_read(VPU_CORE0_PWR_CON) &
			VPU_CORE0_SRAM_PDN_ACK_BIT1) {
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
		}
#endif
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) & ~(0x1 << 10));
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) & ~(0x1 << 11));
		/* TINFO="Set SRAM_SLEEP_B = 1" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) | VPU_CORE0_SRAM_SLEEP_B);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_ISOINT_B = 1" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) | SRAM_ISOINT_B);
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_CKISO = 0" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) & ~SRAM_CKISO);
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_CLR,
			VPU_CORE0_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_CLR,
			VPU_CORE0_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Finish to turn on VPU_CORE0" */
	}
	return err;
}

int spm_mtcmos_ctrl_vpu_core0_shut_down(int state)
{
	int err = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VPU_CORE0" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_SET,
			VPU_CORE0_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MCU_STA1) &
			VPU_CORE0_PROT_STEP1_0_ACK_MASK) !=
			VPU_CORE0_PROT_STEP1_0_ACK_MASK) {
		}
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_SET,
			VPU_CORE0_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MCU_STA1) &
			VPU_CORE0_PROT_STEP2_0_ACK_MASK) !=
			VPU_CORE0_PROT_STEP2_0_ACK_MASK) {
		}
#endif
		/* TINFO="Set SRAM_CKISO = 1" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) | SRAM_CKISO);
		/* TINFO="Set SRAM_ISOINT_B = 0" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) & ~SRAM_ISOINT_B);
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) | VPU_CORE0_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE0_SRAM_PDN_ACK = 1" */
		while ((spm_read(VPU_CORE0_PWR_CON) & VPU_CORE0_SRAM_PDN_ACK)
			!= VPU_CORE0_SRAM_PDN_ACK) {
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
		}
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & VPU_CORE0_PWR_STA_MASK)
			|| (spm_read(PWR_STATUS_2ND)
			& VPU_CORE0_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Finish to turn off VPU_CORE0" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on VPU_CORE0" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & VPU_CORE0_PWR_STA_MASK)
			!= VPU_CORE0_PWR_STA_MASK) || ((spm_read(PWR_STATUS_2ND)
			& VPU_CORE0_PWR_STA_MASK) != VPU_CORE0_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE0_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(VPU_CORE0_PWR_CON) &
			VPU_CORE0_SRAM_PDN_ACK_BIT0) {
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
		}
#endif
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) & ~(0x1 << 9));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE0_SRAM_PDN_ACK_BIT1 = 0" */
		while (spm_read(VPU_CORE0_PWR_CON) &
			VPU_CORE0_SRAM_PDN_ACK_BIT1) {
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
		}
#endif
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) & ~(0x1 << 10));
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) & ~(0x1 << 11));
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_ISOINT_B = 1" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) | SRAM_ISOINT_B);
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_CKISO = 0" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) & ~SRAM_CKISO);
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_CLR,
			VPU_CORE0_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check */
		/* after releasing protect has been ignored */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_CLR,
			VPU_CORE0_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check */
		/* after releasing protect has been ignored */
#endif
		/* TINFO="Finish to turn on VPU_CORE0" */
	}
	return err;
}

int spm_mtcmos_ctrl_vpu_core1_dormant(int state)
{
	int err = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VPU_CORE1" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_SET,
			VPU_CORE1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MCU_STA1) &
			VPU_CORE1_PROT_STEP1_0_ACK_MASK) !=
			VPU_CORE1_PROT_STEP1_0_ACK_MASK) {
		}
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_SET,
			VPU_CORE1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MCU_STA1) &
			VPU_CORE1_PROT_STEP2_0_ACK_MASK) !=
			VPU_CORE1_PROT_STEP2_0_ACK_MASK) {
		}
#endif
		/* TINFO="Set SRAM_CKISO = 1" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) | SRAM_CKISO);
		/* TINFO="Set SRAM_ISOINT_B = 0" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) & ~SRAM_ISOINT_B);
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) | VPU_CORE1_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE1_SRAM_PDN_ACK = 1" */
		while ((spm_read(VPU_CORE1_PWR_CON) & VPU_CORE1_SRAM_PDN_ACK)
			!= VPU_CORE1_SRAM_PDN_ACK) {
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
		}
#endif
		/* TINFO="Set SRAM_SLEEP_B = 0" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) & ~VPU_CORE1_SRAM_SLEEP_B);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & VPU_CORE1_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & VPU_CORE1_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Finish to turn off VPU_CORE1" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on VPU_CORE1" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & VPU_CORE1_PWR_STA_MASK) !=
			VPU_CORE1_PWR_STA_MASK) || ((spm_read(PWR_STATUS_2ND) &
			VPU_CORE1_PWR_STA_MASK) != VPU_CORE1_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE1_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(VPU_CORE1_PWR_CON) &
			VPU_CORE1_SRAM_PDN_ACK_BIT0) {
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
		}
#endif
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) & ~(0x1 << 9));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE1_SRAM_PDN_ACK_BIT1 = 0" */
		while (spm_read(VPU_CORE1_PWR_CON) &
			VPU_CORE1_SRAM_PDN_ACK_BIT1) {
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
		}
#endif
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) & ~(0x1 << 10));
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) & ~(0x1 << 11));
		/* TINFO="Set SRAM_SLEEP_B = 1" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) | VPU_CORE1_SRAM_SLEEP_B);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_ISOINT_B = 1" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) | SRAM_ISOINT_B);
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_CKISO = 0" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) & ~SRAM_CKISO);
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_CLR,
			VPU_CORE1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check */
		/* after releasing protect has been ignored */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_CLR,
			VPU_CORE1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check */
		/* after releasing protect has been ignored */
#endif
		/* TINFO="Finish to turn on VPU_CORE1" */
	}
	return err;
}

int spm_mtcmos_ctrl_vpu_core1_shut_down(int state)
{
	int err = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VPU_CORE1" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_SET,
			VPU_CORE1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MCU_STA1) &
			VPU_CORE1_PROT_STEP1_0_ACK_MASK) !=
			VPU_CORE1_PROT_STEP1_0_ACK_MASK) {
		}
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_SET,
			VPU_CORE1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MCU_STA1) &
			VPU_CORE1_PROT_STEP2_0_ACK_MASK) !=
			VPU_CORE1_PROT_STEP2_0_ACK_MASK) {
		}
#endif
		/* TINFO="Set SRAM_CKISO = 1" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) | SRAM_CKISO);
		/* TINFO="Set SRAM_ISOINT_B = 0" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) & ~SRAM_ISOINT_B);
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) | VPU_CORE1_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE1_SRAM_PDN_ACK = 1" */
		while ((spm_read(VPU_CORE1_PWR_CON) & VPU_CORE1_SRAM_PDN_ACK)
			!= VPU_CORE1_SRAM_PDN_ACK) {
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
		}
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & VPU_CORE1_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & VPU_CORE1_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/*Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Finish to turn off VPU_CORE1" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on VPU_CORE1" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & VPU_CORE1_PWR_STA_MASK)
			!= VPU_CORE1_PWR_STA_MASK) || ((spm_read(PWR_STATUS_2ND)
			& VPU_CORE1_PWR_STA_MASK) != VPU_CORE1_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE1_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(VPU_CORE1_PWR_CON) &
			VPU_CORE1_SRAM_PDN_ACK_BIT0) {
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
		}
#endif
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) & ~(0x1 << 9));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE1_SRAM_PDN_ACK_BIT1 = 0" */
		while (spm_read(VPU_CORE1_PWR_CON) &
			VPU_CORE1_SRAM_PDN_ACK_BIT1) {
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
		}
#endif
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) & ~(0x1 << 10));
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) & ~(0x1 << 11));
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_ISOINT_B = 1" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) | SRAM_ISOINT_B);
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_CKISO = 0" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) & ~SRAM_CKISO);
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_CLR,
			VPU_CORE1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check */
		/* after releasing protect has been ignored */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_CLR,
			VPU_CORE1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check */
		/* after releasing protect has been ignored */
#endif
		/* TINFO="Finish to turn on VPU_CORE1" */
	}
	return err;
}

int spm_mtcmos_ctrl_vpu_core2_dormant(int state)
{
	int err = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VPU_CORE2" */
		/* TINFO="Set SRAM_CKISO = 1" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) | SRAM_CKISO);
		/* TINFO="Set SRAM_ISOINT_B = 0" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) & ~SRAM_ISOINT_B);
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) | VPU_CORE2_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE2_SRAM_PDN_ACK = 1" */
		while ((spm_read(VPU_CORE2_PWR_CON) & VPU_CORE2_SRAM_PDN_ACK) !=
			VPU_CORE2_SRAM_PDN_ACK) {
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
		}
#endif
		/* TINFO="Set SRAM_SLEEP_B = 0" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) & ~VPU_CORE2_SRAM_SLEEP_B);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE2_SRAM_SLEEP_B_ACK = 0" */
		while (spm_read(VPU_CORE2_PWR_CON) &
			VPU_CORE2_SRAM_SLEEP_B_ACK) {
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
		}
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & VPU_CORE2_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & VPU_CORE2_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Finish to turn off VPU_CORE2" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on VPU_CORE2" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & VPU_CORE2_PWR_STA_MASK)
			!= VPU_CORE2_PWR_STA_MASK) || ((spm_read(PWR_STATUS_2ND)
			& VPU_CORE2_PWR_STA_MASK) != VPU_CORE2_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE2_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(VPU_CORE2_PWR_CON) &
			VPU_CORE2_SRAM_PDN_ACK_BIT0) {
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
		}
#endif
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) & ~(0x1 << 9));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE2_SRAM_PDN_ACK_BIT1 = 0" */
		while (spm_read(VPU_CORE2_PWR_CON) &
			VPU_CORE2_SRAM_PDN_ACK_BIT1) {
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
		}
#endif
		/* TINFO="Set SRAM_SLEEP_B = 1" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) | VPU_CORE2_SRAM_SLEEP_B);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE2_SRAM_SLEEP_B_ACK = 1" */
		while ((spm_read(VPU_CORE2_PWR_CON) &
			VPU_CORE2_SRAM_SLEEP_B_ACK) !=
			VPU_CORE2_SRAM_SLEEP_B_ACK) {
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
		}
#endif
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_ISOINT_B = 1" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) | SRAM_ISOINT_B);
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_CKISO = 0" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) & ~SRAM_CKISO);
		/* TINFO="Finish to turn on VPU_CORE2" */
	}
	return err;
}

int spm_mtcmos_ctrl_vpu_core2_shut_down(int state)
{
	int err = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VPU_CORE2" */
		/* TINFO="Set SRAM_CKISO = 1" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) | SRAM_CKISO);
		/* TINFO="Set SRAM_ISOINT_B = 0" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) & ~SRAM_ISOINT_B);
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) | VPU_CORE2_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE2_SRAM_PDN_ACK = 1" */
		while ((spm_read(VPU_CORE2_PWR_CON) & VPU_CORE2_SRAM_PDN_ACK)
			!= VPU_CORE2_SRAM_PDN_ACK) {
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
		}
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & VPU_CORE2_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & VPU_CORE2_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Finish to turn off VPU_CORE2" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on VPU_CORE2" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & VPU_CORE2_PWR_STA_MASK)
			!= VPU_CORE2_PWR_STA_MASK) || ((spm_read(PWR_STATUS_2ND)
			& VPU_CORE2_PWR_STA_MASK) != VPU_CORE2_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.*/
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE2_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(VPU_CORE2_PWR_CON)
			& VPU_CORE2_SRAM_PDN_ACK_BIT0) {
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
			/* Need f_fsmi_ck for SRAM PDN delay IP. */
		}
#endif
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) & ~(0x1 << 9));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE2_SRAM_PDN_ACK_BIT1 = 0" */
		while (spm_read(VPU_CORE2_PWR_CON)
			& VPU_CORE2_SRAM_PDN_ACK_BIT1) {
				/* Need f_fsmi_ck for SRAM PDN delay IP. */
				/* Need f_fsmi_ck for SRAM PDN delay IP. */
		}
#endif
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_ISOINT_B = 1" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) | SRAM_ISOINT_B);
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_CKISO = 0" */
		spm_write(VPU_CORE2_PWR_CON,
			spm_read(VPU_CORE2_PWR_CON) & ~SRAM_CKISO);
		/* TINFO="Finish to turn on VPU_CORE2" */
	}
	return err;
}

int spm_mtcmos_ctrl_vde(int state)
{
	int err = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VDE" */
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(SMI_COMMON_SMI_CLAMP_SET, VDE_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(SMI_COMMON_SMI_CLAMP) &
			VDE_PROT_STEP2_0_ACK_MASK) !=
			VDE_PROT_STEP2_0_ACK_MASK) {
		}
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) | VDE_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VDE_SRAM_PDN_ACK = 1" */
		while ((spm_read(VDE_PWR_CON) & VDE_SRAM_PDN_ACK)
			!= VDE_SRAM_PDN_ACK) {
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & VDE_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & VDE_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Finish to turn off VDE" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on VDE" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & VDE_PWR_STA_MASK)
			!= VDE_PWR_STA_MASK) || ((spm_read(PWR_STATUS_2ND)
			& VDE_PWR_STA_MASK) != VDE_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack. */
			/* Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VDE_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(VDE_PWR_CON) & VDE_SRAM_PDN_ACK_BIT0) {
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(SMI_COMMON_SMI_CLAMP_CLR, VDE_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Finish to turn on VDE" */
	}
	return err;
}




/* auto-gen end*/

/* enable op*/
/*
 *static int general_sys_enable_op(struct subsys *sys)
 *{
 *	return spm_mtcmos_power_on_general_locked(sys, 1, 0);
 *}
 */
static int MD1_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_md1(STA_POWER_ON);
}

static int CONN_sys_enable_op(struct subsys *sys)
{
	/*printk("CONN_sys_enable_op\r\n"); */
	return spm_mtcmos_ctrl_conn(STA_POWER_ON);
}

static int DIS_sys_enable_op(struct subsys *sys)
{
	/*printk("DIS_sys_enable_op\r\n"); */
	return spm_mtcmos_ctrl_dis(STA_POWER_ON);
}

static int MFG_sys_enable_op(struct subsys *sys)
{
	/*printk("MFG_sys_enable_op\r\n"); */
	return spm_mtcmos_ctrl_mfg(STA_POWER_ON);
}

static int ISP_sys_enable_op(struct subsys *sys)
{
	/*printk("ISP_sys_enable_op\r\n"); */
	return spm_mtcmos_ctrl_isp(STA_POWER_ON);
}

static int VEN_sys_enable_op(struct subsys *sys)
{
	/*printk("VEN_sys_enable_op\r\n"); */
	return spm_mtcmos_ctrl_ven(STA_POWER_ON);
}

static int MFG_ASYNC_sys_enable_op(struct subsys *sys)
{
	/*printk("MFG_ASYNC_sys_enable_op\r\n"); */
	return spm_mtcmos_ctrl_mfg_async(STA_POWER_ON);
}

static int AUDIO_sys_enable_op(struct subsys *sys)
{
	/*printk("AUDIO_sys_enable_op\r\n"); */
	return spm_mtcmos_ctrl_audio(STA_POWER_ON);
}

static int CAM_sys_enable_op(struct subsys *sys)
{
	/*printk("CAM_sys_enable_op\r\n"); */
	return spm_mtcmos_ctrl_cam(STA_POWER_ON);
}

static int MFG_CORE1_sys_enable_op(struct subsys *sys)
{
	/*printk("MFG_CORE1_sys_enable_op\r\n"); */
	return spm_mtcmos_ctrl_mfg_core1(STA_POWER_ON);
}

static int MFG_CORE0_sys_enable_op(struct subsys *sys)
{
	/*printk("MFG_CORE0_sys_enable_op\r\n"); */
	return spm_mtcmos_ctrl_mfg_core0(STA_POWER_ON);
}

static int MFG_2D_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_mfg_2d(STA_POWER_ON);
}

static int VDE_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vde(STA_POWER_ON);
}

static int VPU_TOP_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vpu_top(STA_POWER_ON);
}

static int VPU_CORE0_DORMANT_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vpu_core0_dormant(STA_POWER_ON);
}

static int VPU_CORE0_SHUTDOWN_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vpu_core0_shut_down(STA_POWER_ON);
}

static int VPU_CORE1_DORMANT_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vpu_core1_dormant(STA_POWER_ON);
}

static int VPU_CORE1_SHUTDOWN_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vpu_core1_shut_down(STA_POWER_ON);
}

static int VPU_CORE2_DORMANT_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vpu_core2_dormant(STA_POWER_ON);
}

static int VPU_CORE2_SHUTDOWN_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vpu_core2_shut_down(STA_POWER_ON);
}
/* disable op */
/*
 *static int general_sys_disable_op(struct subsys *sys)
 *{
 *	return spm_mtcmos_power_off_general_locked(sys, 1, 0);
 *}
 */
static int MD1_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_md1(STA_POWER_DOWN);
}

static int CONN_sys_disable_op(struct subsys *sys)
{
	/*printk("CONN_sys_disable_op\r\n"); */
	return spm_mtcmos_ctrl_conn(STA_POWER_DOWN);
}

static int DIS_sys_disable_op(struct subsys *sys)
{
	/*printk("DIS_sys_disable_op\r\n"); */
	return spm_mtcmos_ctrl_dis(STA_POWER_DOWN);
}

static int MFG_sys_disable_op(struct subsys *sys)
{
	/*printk("MFG_sys_disable_op\r\n"); */
	return spm_mtcmos_ctrl_mfg(STA_POWER_DOWN);
}

static int ISP_sys_disable_op(struct subsys *sys)
{
	/*printk("ISP_sys_disable_op\r\n"); */
	return spm_mtcmos_ctrl_isp(STA_POWER_DOWN);
}

static int VEN_sys_disable_op(struct subsys *sys)
{
	/*printk("VEN_sys_disable_op\r\n"); */
	return spm_mtcmos_ctrl_ven(STA_POWER_DOWN);
}

static int MFG_ASYNC_sys_disable_op(struct subsys *sys)
{
	/*printk("MFG_ASYNC_sys_disable_op\r\n"); */
	return spm_mtcmos_ctrl_mfg_async(STA_POWER_DOWN);
}

static int AUDIO_sys_disable_op(struct subsys *sys)
{
	/*printk("AUDIO_sys_disable_op\r\n"); */
	return spm_mtcmos_ctrl_audio(STA_POWER_DOWN);
}

static int CAM_sys_disable_op(struct subsys *sys)
{
	/*printk("CAM_sys_disable_op\r\n"); */
	return spm_mtcmos_ctrl_cam(STA_POWER_DOWN);
}

static int MFG_CORE1_sys_disable_op(struct subsys *sys)
{
	/*printk("MFG_CORE1_sys_disable_op\r\n"); */
	return spm_mtcmos_ctrl_mfg_core1(STA_POWER_DOWN);
}

static int MFG_CORE0_sys_disable_op(struct subsys *sys)
{
	/*printk("MFG_CORE0_sys_disable_op\r\n"); */
	return spm_mtcmos_ctrl_mfg_core0(STA_POWER_DOWN);
}

static int MFG_2D_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_mfg_2d(STA_POWER_DOWN);
}

static int VDE_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vde(STA_POWER_DOWN);
}

static int VPU_TOP_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vpu_top(STA_POWER_DOWN);
}

static int VPU_CORE0_DORMANT_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vpu_core0_dormant(STA_POWER_DOWN);
}

static int VPU_CORE0_SHUTDOWN_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vpu_core0_shut_down(STA_POWER_DOWN);
}

static int VPU_CORE1_DORMANT_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vpu_core1_dormant(STA_POWER_DOWN);
}

static int VPU_CORE1_SHUTDOWN_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vpu_core1_shut_down(STA_POWER_DOWN);
}

static int VPU_CORE2_DORMANT_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vpu_core2_dormant(STA_POWER_DOWN);
}

static int VPU_CORE2_SHUTDOWN_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vpu_core2_shut_down(STA_POWER_DOWN);
}

static int sys_get_state_op(struct subsys *sys)
{
	unsigned int sta = clk_readl(PWR_STATUS);
	unsigned int sta_s = clk_readl(PWR_STATUS_2ND);

	return (sta & sys->sta_mask) && (sta_s & sys->sta_mask);
}

/* ops */
/*
 *static struct subsys_ops general_sys_ops = {
 *	.enable = general_sys_enable_op,
 *	.disable = general_sys_disable_op,
 *	.get_state = sys_get_state_op,
 *};
 */

static struct subsys_ops MD1_sys_ops = {
	.enable = MD1_sys_enable_op,
	.disable = MD1_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops CONN_sys_ops = {
	.enable = CONN_sys_enable_op,
	.disable = CONN_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops DIS_sys_ops = {
	.enable = DIS_sys_enable_op,
	.disable = DIS_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops MFG_sys_ops = {
	.enable = MFG_sys_enable_op,
	.disable = MFG_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops ISP_sys_ops = {
	.enable = ISP_sys_enable_op,
	.disable = ISP_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops VEN_sys_ops = {
	.enable = VEN_sys_enable_op,
	.disable = VEN_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops MFG_ASYNC_sys_ops = {
	.enable = MFG_ASYNC_sys_enable_op,
	.disable = MFG_ASYNC_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops AUDIO_sys_ops = {
	.enable = AUDIO_sys_enable_op,
	.disable = AUDIO_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops CAM_sys_ops = {
	.enable = CAM_sys_enable_op,
	.disable = CAM_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops MFG_CORE1_sys_ops = {
	.enable = MFG_CORE1_sys_enable_op,
	.disable = MFG_CORE1_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops MFG_CORE0_sys_ops = {
	.enable = MFG_CORE0_sys_enable_op,
	.disable = MFG_CORE0_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops VDE_sys_ops = {
	.enable = VDE_sys_enable_op,
	.disable = VDE_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops MFG_2D_sys_ops = {
	.enable = MFG_2D_sys_enable_op,
	.disable = MFG_2D_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops VPU_TOP_sys_ops = {
	.enable = VPU_TOP_sys_enable_op,
	.disable = VPU_TOP_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops VPU_CORE0_DORMANT_sys_ops = {
	.enable = VPU_CORE0_DORMANT_sys_enable_op,
	.disable = VPU_CORE0_DORMANT_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops VPU_CORE0_SHUTDOWN_sys_ops = {
	.enable = VPU_CORE0_SHUTDOWN_sys_enable_op,
	.disable = VPU_CORE0_SHUTDOWN_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops VPU_CORE1_DORMANT_sys_ops = {
	.enable = VPU_CORE1_DORMANT_sys_enable_op,
	.disable = VPU_CORE1_DORMANT_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops VPU_CORE1_SHUTDOWN_sys_ops = {
	.enable = VPU_CORE1_SHUTDOWN_sys_enable_op,
	.disable = VPU_CORE1_SHUTDOWN_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops VPU_CORE2_DORMANT_sys_ops = {
	.enable = VPU_CORE2_DORMANT_sys_enable_op,
	.disable = VPU_CORE2_DORMANT_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops VPU_CORE2_SHUTDOWN_sys_ops = {
	.enable = VPU_CORE2_SHUTDOWN_sys_enable_op,
	.disable = VPU_CORE2_SHUTDOWN_sys_disable_op,
	.get_state = sys_get_state_op,
};
static int subsys_is_on(enum subsys_id id)
{
	int r;
	struct subsys *sys = id_to_sys(id);

	if (!sys) {
		WARN_ON(!sys);
		return -EINVAL;
	}

	r = sys->ops->get_state(sys);

#if MT_CCF_DEBUG
	pr_debug("[CCF] %s:%d, sys=%s, id=%d\n", __func__, r, sys->name, id);
#endif				/* MT_CCF_DEBUG */

	return r;
}

#if CONTROL_LIMIT
int allow[NR_SYSS] = {
1,	/*SYS_MD1 = 0,*/
1,	/*SYS_CONN = 1,*/
1,	/*SYS_DIS = 2,*//*can HS, but resume fail*/
1,	/*SYS_MFG = 3,*/
1,	/*SYS_ISP = 4,*/
1,	/*SYS_VEN = 5,*/
1,	/*SYS_MFG_ASYNC = 6,*/
1,	/*SYS_AUDIO = 7,*/
1,	/*SYS_CAM = 8,*//*There is no process hold rtnl lock*/
1,	/*SYS_MFG_CORE1 = 9,*/
1,	/*SYS_MFG_CORE0 = 10,*/
1,	/*SYS_VDE = 11,*/
1,	/*SYS_VPU_TOP = 12,*/
1,	/*SYS_VPU_CORE0_DORMANT = 13,*/
1,	/*SYS_VPU_CORE0_SHUTDOWN = 14,*/
1,	/*SYS_VPU_CORE1_DORMANT = 15,*/
1,	/*SYS_VPU_CORE1_SHUTDOWN = 16,*/
0,	/*SYS_VPU_CORE2_DORMANT = 17,*/
0,	/*SYS_VPU_CORE2_SHUTDOWN = 18,*/
1,	/*SYS_MFG_2D = 19*/
};
#endif
static int enable_subsys(enum subsys_id id)
{
	int r;
	unsigned long flags;
	struct subsys *sys = id_to_sys(id);

	if (!sys) {
		WARN_ON(!sys);
		return -EINVAL;
	}

#if MT_CCF_BRINGUP
	/*pr_debug("[CCF] %s: sys=%s, id=%d\n", __func__, sys->name, id);*/
	switch (id) {
	case SYS_MD1:
		spm_mtcmos_ctrl_md1(STA_POWER_ON);
		break;
	case SYS_CONN:
		spm_mtcmos_ctrl_conn(STA_POWER_ON);
		break;
	default:
		break;
	}
	return 0;
#endif				/* MT_CCF_BRINGUP */

#if CONTROL_LIMIT
	#if MT_CCF_DEBUG
	pr_debug("[CCF] %s: sys=%s, id=%d\n", __func__, sys->name, id);
	#endif
	if (allow[id] == 0) {
		#if MT_CCF_DEBUG
		pr_debug("[CCF] %s: do nothing return\n", __func__);
		#endif
		return 0;
	}
#endif


	mtk_clk_lock(flags);

#if CHECK_PWR_ST
	if (sys->ops->get_state(sys) == SUBSYS_PWR_ON) {
		mtk_clk_unlock(flags);
		return 0;
	}
#endif				/* CHECK_PWR_ST */

	r = sys->ops->enable(sys);
	WARN_ON(r);

	mtk_clk_unlock(flags);

	if (g_pgcb && g_pgcb->after_on)
		g_pgcb->after_on(id);

	return r;
}

static int disable_subsys(enum subsys_id id)
{
	int r;
	unsigned long flags;
	struct subsys *sys = id_to_sys(id);

	if (!sys) {
		WARN_ON(!sys);
		return -EINVAL;
	}

#if MT_CCF_BRINGUP
	/*pr_debug("[CCF] %s: sys=%s, id=%d\n", __func__, sys->name, id);*/
	switch (id) {
	case SYS_MD1:
		spm_mtcmos_ctrl_md1(STA_POWER_DOWN);
		break;
	case SYS_CONN:
		spm_mtcmos_ctrl_conn(STA_POWER_DOWN);
		break;
	default:
		break;
	}
	return 0;
#endif				/* MT_CCF_BRINGUP */
#if CONTROL_LIMIT
	#if MT_CCF_DEBUG
	pr_debug("[CCF] %s: sys=%s, id=%d\n", __func__, sys->name, id);
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

	if (g_pgcb && g_pgcb->before_off)
		g_pgcb->before_off(id);

	mtk_clk_lock(flags);

#if CHECK_PWR_ST
	if (sys->ops->get_state(sys) == SUBSYS_PWR_DOWN) {
		mtk_clk_unlock(flags);
		return 0;
	}
#endif				/* CHECK_PWR_ST */

	r = sys->ops->disable(sys);
	WARN_ON(r);

	mtk_clk_unlock(flags);

	return r;
}

/*
 * power_gate
 */

struct mt_power_gate {
	struct clk_hw hw;
	struct clk *pre_clk;
	enum subsys_id pd_id;
};

#define to_power_gate(_hw) container_of(_hw, struct mt_power_gate, hw)

static int pg_enable(struct clk_hw *hw)
{
	struct mt_power_gate *pg = to_power_gate(hw);

#if MT_CCF_DEBUG
	pr_debug("[CCF] %s: sys=%s, pd_id=%u\n", __func__,
		 __clk_get_name(hw->clk), pg->pd_id);
#endif				/* MT_CCF_DEBUG */

	return enable_subsys(pg->pd_id);
}

static void pg_disable(struct clk_hw *hw)
{
	struct mt_power_gate *pg = to_power_gate(hw);

#if MT_CCF_DEBUG
	pr_debug("[CCF] %s: sys=%s, pd_id=%u\n", __func__,
		 __clk_get_name(hw->clk), pg->pd_id);
#endif				/* MT_CCF_DEBUG */

	disable_subsys(pg->pd_id);
}

static int pg_is_enabled(struct clk_hw *hw)
{
	struct mt_power_gate *pg = to_power_gate(hw);

	return subsys_is_on(pg->pd_id);
}

int pg_prepare(struct clk_hw *hw)
{
	int r;
	struct mt_power_gate *pg = to_power_gate(hw);

#if MT_CCF_DEBUG
	pr_debug("[CCF] %s: sys=%s, pre_sys=%s\n", __func__,
		 __clk_get_name(hw->clk),
		 pg->pre_clk ? __clk_get_name(pg->pre_clk) : "");
#endif				/* MT_CCF_DEBUG */

	if (pg->pre_clk) {
		r = clk_prepare_enable(pg->pre_clk);
		if (r)
			return r;
	}

	return pg_enable(hw);

}

void pg_unprepare(struct clk_hw *hw)
{
	struct mt_power_gate *pg = to_power_gate(hw);

#if MT_CCF_DEBUG
	pr_debug("[CCF] %s: clk=%s, pre_clk=%s\n", __func__,
		 __clk_get_name(hw->clk),
		 pg->pre_clk ? __clk_get_name(pg->pre_clk) : "");
#endif				/* MT_CCF_DEBUG */

	pg_disable(hw);

	if (pg->pre_clk)
		clk_disable_unprepare(pg->pre_clk);
}

static const struct clk_ops mt_power_gate_ops = {
	.prepare = pg_prepare,
	.unprepare = pg_unprepare,
	.is_enabled = pg_is_enabled,
};

struct clk *mt_clk_register_power_gate(const char *name,
				       const char *parent_name,
				       struct clk *pre_clk,
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

	pg->pre_clk = pre_clk;
	pg->pd_id = pd_id;
	pg->hw.init = &init;

	clk = clk_register(NULL, &pg->hw);
	if (IS_ERR(clk))
		kfree(pg);

	return clk;
}

#define pg_md1         "pg_md1"
#define pg_conn        "pg_conn"
#define pg_dis         "pg_dis"
#define pg_mfg         "pg_mfg"
#define pg_isp         "pg_isp"
#define pg_ven         "pg_ven"
#define pg_mfg_async   "pg_mfg_async"
#define pg_audio       "pg_audio"
#define pg_cam   "pg_cam"
#define pg_mfg_core1   "pg_mfg_core1"
#define pg_mfg_core0   "pg_mfg_core0"

#define img_sel	"img_sel"
#define cam_sel	"cam_sel"
#define mm_sel			"mm_sel"
#define f26m_sel	"f26m_sel"
#define mfg_sel	"mfg_sel"
#define infracfg_ao_audio_26m_bclk_ck "infracfg_ao_audio_26m_bclk_ck"
/* FIXME: set correct value: E */

struct mtk_power_gate {
	int id;
	const char *name;
	const char *parent_name;
	const char *pre_clk_name;
	enum subsys_id pd_id;
};

#define PGATE(_id, _name, _parent, _pre_clk, _pd_id) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.pre_clk_name = _pre_clk,		\
		.pd_id = _pd_id,			\
	}

/* FIXME: all values needed to be verified */
struct mtk_power_gate scp_clks[] __initdata = {
	PGATE(SCP_SYS_MD1, pg_md1,
		NULL, NULL, SYS_MD1),
	PGATE(SCP_SYS_CONN, pg_conn,
		NULL, NULL, SYS_CONN),
	PGATE(SCP_SYS_DIS, pg_dis,
		NULL, mm_sel, SYS_DIS),
	PGATE(SCP_SYS_MFG_ASYNC, pg_mfg_async,
		NULL, mfg_sel, SYS_MFG_ASYNC),
	PGATE(SCP_SYS_MFG, pg_mfg,
		pg_mfg_async, NULL, SYS_MFG),
	PGATE(SCP_SYS_ISP, pg_isp,
		pg_dis, img_sel, SYS_ISP),
	PGATE(SCP_SYS_VEN, pg_ven,
		pg_dis, mm_sel, SYS_VEN),
	PGATE(SCP_SYS_AUDIO, pg_audio,
		NULL, infracfg_ao_audio_26m_bclk_ck, SYS_AUDIO),
	PGATE(SCP_SYS_CAM, pg_cam,
		pg_dis, cam_sel, SYS_CAM),
	PGATE(SCP_SYS_MFG_CORE1, pg_mfg_core1,
		pg_mfg, NULL, SYS_MFG_CORE1),
	PGATE(SCP_SYS_MFG_CORE0, pg_mfg_core0,
		pg_mfg, NULL, SYS_MFG_CORE0),

	PGATE(SCP_SYS_MFG_2D, "pg_mfg_2d",
		pg_mfg, NULL, SYS_MFG_2D),
	PGATE(SCP_SYS_VDE, "pg_vde",
		pg_dis, mm_sel, SYS_VDE),
	PGATE(SCP_SYS_VPU_TOP, "pg_vpu_top",
		pg_dis, "ipu_if_sel", SYS_VPU_TOP),
	PGATE(SCP_SYS_VPU_CORE0_DORMANT, "pg_vpu_core0_d",
		"pg_vpu_top", NULL, SYS_VPU_CORE0_DORMANT),
	PGATE(SCP_SYS_VPU_CORE0_SHUTDOWN, "pg_vpu_core0_s",
		"pg_vpu_top", NULL, SYS_VPU_CORE0_SHUTDOWN),
	PGATE(SCP_SYS_VPU_CORE1_DORMANT, "pg_vpu_core1_d",
		"pg_vpu_top", NULL, SYS_VPU_CORE1_DORMANT),
	PGATE(SCP_SYS_VPU_CORE1_SHUTDOWN, "pg_vpu_core1_s",
		"pg_vpu_top", NULL, SYS_VPU_CORE1_SHUTDOWN),
	PGATE(SCP_SYS_VPU_CORE2_DORMANT, "pg_vpu_core2_d",
		NULL, NULL, SYS_VPU_CORE2_DORMANT),
	PGATE(SCP_SYS_VPU_CORE2_SHUTDOWN, "pg_vpu_core2_s",
		NULL, NULL, SYS_VPU_CORE2_SHUTDOWN),
};

static void __init init_clk_scpsys(void __iomem *infracfg_reg,
				   void __iomem *spm_reg,
				   void __iomem *infra_reg,
				   void __iomem *smi_larb6_reg,
				   void __iomem *smi_common_reg,
				   struct clk_onecell_data *clk_data)
{
	int i;
	struct clk *clk;
	struct clk *pre_clk;

	infracfg_base = infracfg_reg;
	spm_base = spm_reg;
	infra_base = infra_reg;
	smi_larb6_base = smi_larb6_reg;
	smi_common_base = smi_common_reg;

	syss[SYS_MD1].ctl_addr = MD1_PWR_CON;
	syss[SYS_CONN].ctl_addr = CONN_PWR_CON;
	syss[SYS_DIS].ctl_addr = DIS_PWR_CON;
	syss[SYS_MFG].ctl_addr = MFG_PWR_CON;
	syss[SYS_ISP].ctl_addr = ISP_PWR_CON;
	syss[SYS_VEN].ctl_addr = VEN_PWR_CON;
	syss[SYS_MFG_ASYNC].ctl_addr = MFG_ASYNC_PWR_CON;
	syss[SYS_AUDIO].ctl_addr = AUDIO_PWR_CON;
	syss[SYS_CAM].ctl_addr = CAM_PWR_CON;
	syss[SYS_MFG_CORE1].ctl_addr = MFG_CORE1_PWR_CON;
	syss[SYS_MFG_CORE0].ctl_addr = MFG_CORE0_PWR_CON;

	for (i = 0; i < ARRAY_SIZE(scp_clks); i++) {
		struct mtk_power_gate *pg = &scp_clks[i];

		pre_clk = pg->pre_clk_name ?
			__clk_lookup(pg->pre_clk_name) : NULL;

		clk = mt_clk_register_power_gate(pg->name,
			pg->parent_name, pre_clk, pg->pd_id);

		if (IS_ERR(clk)) {
			pr_err("[CCF] %s: Failed to register clk %s: %ld\n",
			       __func__, pg->name, PTR_ERR(clk));
			continue;
		}

		if (clk_data)
			clk_data->clks[pg->id] = clk;

#if MT_CCF_DEBUG
		pr_debug("[CCF] %s: pgate %3d: %s\n", __func__, i, pg->name);
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

#ifdef CONFIG_OF
void iomap_mm(void)
{
	struct device_node *node;

/*mmsys_config*/
	node = of_find_compatible_node(NULL, NULL, "mediatek,mmsys_config");
	if (!node)
		pr_debug("[CLK_MMSYS] find node failed\n");
	clk_mmsys_config_base = of_iomap(node, 0);
	if (!clk_mmsys_config_base)
		pr_debug("[CLK_MMSYS] base failed\n");
/*imgsys*/
	node = of_find_compatible_node(NULL, NULL, "mediatek,imgsys");
	if (!node)
		pr_debug("[CLK_IMGSYS_CONFIG] find node failed\n");
	clk_imgsys_base = of_iomap(node, 0);
	if (!clk_imgsys_base)
		pr_debug("[CLK_IMGSYS_CONFIG] base failed\n");
/*vdec_gcon*/
	node = of_find_compatible_node(NULL, NULL, "mediatek,vdec_gcon");
	if (!node)
		pr_debug("[CLK_VDEC_GCON] find node failed\n");
	clk_vdec_gcon_base = of_iomap(node, 0);
	if (!clk_vdec_gcon_base)
		pr_debug("[CLK_VDEC_GCON] base failed\n");
/*venc_gcon*/
	node = of_find_compatible_node(NULL, NULL, "mediatek,venc_gcon");
	if (!node)
		pr_debug("[CLK_VENC_GCON] find node failed\n");
	clk_venc_gcon_base = of_iomap(node, 0);
	if (!clk_venc_gcon_base)
		pr_debug("[CLK_VENC_GCON] base failed\n");

/*cam*/
	node = of_find_compatible_node(NULL, NULL, "mediatek,camsys");
	if (!node)
		pr_debug("[CLK_CAM] find node failed\n");
	clk_camsys_base = of_iomap(node, 0);
	if (!clk_camsys_base)
		pr_debug("[CLK_CAM] base failed\n");
/*mfg*/
	node = of_find_compatible_node(NULL, NULL, "mediatek,mfgcfg");
	if (!node)
		pr_debug("[CLK_MFG] find node failed\n");
	clk_mfg_base = of_iomap(node, 0);
	if (!clk_mfg_base)
		pr_debug("[CLK_MFG] base failed\n");
}
#endif

static void __init mt_scpsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *infracfg_reg;
	void __iomem *spm_reg;
	void __iomem *infra_reg;
	void __iomem *ckgen_reg;
	void __iomem *smi_larb6_reg;
	void __iomem *smi_common_reg;
	int r;

	infracfg_reg = get_reg(node, 0);
	spm_reg = get_reg(node, 1);
	infra_reg = get_reg(node, 2);
	ckgen_reg = get_reg(node, 3);
	smi_larb6_reg = get_reg(node, 4);
	smi_common_reg = get_reg(node, 5);



	if (!infracfg_reg || !spm_reg || !infra_reg  ||
		!ckgen_reg || !smi_larb6_reg || !smi_common_reg) {
		pr_err("clk-pg-mt6771: missing reg\n");
		return;
	}

	clk_data = alloc_clk_data(SCP_NR_SYSS);

	init_clk_scpsys(infracfg_reg, spm_reg, infra_reg,
		smi_larb6_reg, smi_common_reg, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}

	ckgen_base = ckgen_reg;
	/*MM Bus*/
	iomap_mm();
#if 0/*!MT_CCF_BRINGUP*/
	/* subsys init: per modem owner request, disable modem power first */
	disable_subsys(SYS_MD1);
#else				/*power on all subsys for bring up */
#ifndef CONFIG_FPGA_EARLY_PORTING
	spm_mtcmos_ctrl_mfg_async(STA_POWER_ON);
	spm_mtcmos_ctrl_mfg(STA_POWER_ON);
	spm_mtcmos_ctrl_mfg_core0(STA_POWER_ON);
	spm_mtcmos_ctrl_mfg_core1(STA_POWER_ON);
	spm_mtcmos_ctrl_mfg_2d(STA_POWER_ON);
#if 0
	spm_mtcmos_ctrl_dis(STA_POWER_ON);
	spm_mtcmos_ctrl_cam(STA_POWER_ON);
	spm_mtcmos_ctrl_ven(STA_POWER_ON);
	spm_mtcmos_ctrl_vde(STA_POWER_ON);
	spm_mtcmos_ctrl_isp(STA_POWER_ON);
#endif
#if 0 /*avoid hang in bring up*/
	spm_mtcmos_ctrl_vpu_top(STA_POWER_ON);
	spm_mtcmos_ctrl_vpu_core0_shut_down(STA_POWER_ON);
	spm_mtcmos_ctrl_vpu_core1_shut_down(STA_POWER_ON);
	/*spm_mtcmos_ctrl_vpu_core2_shut_down(STA_POWER_ON);*/
#endif
	/*spm_mtcmos_ctrl_md1(STA_POWER_ON);*/
	spm_mtcmos_ctrl_md1(STA_POWER_DOWN);
	/*spm_mtcmos_ctrl_audio(STA_POWER_ON);*/
#endif
#endif				/* !MT_CCF_BRINGUP */
}

CLK_OF_DECLARE_DRIVER(mtk_pg_regs, "mediatek,scpsys", mt_scpsys_init);

#if 0
static const char * const *get_all_clk_names(size_t *num)
{
	static const char * const clks[] = {

		/* CAM */
		"camsys_larb6",
		"camsys_dfp_vad",
		"camsys_larb3",
		"camsys_cam",
		"camsys_camtg",
		"camsys_seninf",
		"camsys_camsv0",
		"camsys_camsv1",
		"camsys_camsv2",
		"camsys_ccu",
		/* IMG */
		"imgsys_larb5",
		"imgsys_larb2",
		"imgsys_dip",
		"imgsys_fdvt",
		"imgsys_dpe",
		"imgsys_rsc",
		"imgsys_mfb",
		"imgsys_wpe_a",
		"imgsys_wpe_b",
		"imgsys_owe",
		/* MM */
		"mm_smi_common",
		"mm_smi_larb0",
		"mm_smi_larb1",
		"mm_gals_comm0",
		"mm_gals_comm1",
		"mm_gals_ccu2mm",
		"mm_gals_ipu12mm",
		"mm_gals_img2mm",
		"mm_gals_cam2mm",
		"mm_gals_ipu2mm",
		"mm_mdp_dl_txck",
		"mm_ipu_dl_txck",
		"mm_mdp_rdma0",
		"mm_mdp_rdma1",
		"mm_mdp_rsz0",
		"mm_mdp_rsz1",
		"mm_mdp_tdshp",
		"mm_mdp_wrot0",
		"mm_mdp_wdma0",
		"mm_fake_eng",
		"mm_disp_ovl0",
		"mm_disp_ovl0_2l",
		"mm_disp_ovl1_2l",
		"mm_disp_rdma0",
		"mm_disp_rdma1",
		"mm_disp_wdma0",
		"mm_disp_color0",
		"mm_disp_ccorr0",
		"mm_disp_aal0",
		"mm_disp_gamma0",
		"mm_disp_dither0",
		"mm_disp_split",
		"mm_dsi0_mmck",
		"mm_dsi0_ifck",
		"mm_dpi_mmck",
		"mm_dpi_ifck",
		"mm_fake_eng2",
		"mm_mdp_dl_rxck",
		"mm_ipu_dl_rxck",
		"mm_26m",
		"mm_mmsys_r2y",
		"mm_disp_rsz",
		"mm_mdp_aal",
		"mm_mdp_ccorr",
		"mm_dbi_mmck",
		"mm_dbi_ifck",
		/* VENC */
		"venc_larb",
		"venc_venc",
		"venc_jpgenc",
		/* VDE */
		"vdec_cken",
		"vdec_larb1_cken",
	};
	*num = ARRAY_SIZE(clks);
	return clks;
}
#endif

static const char * const *get_cam_clk_names(size_t *num)
{
	static const char * const clks[] = {

		/* CAM */
		"camsys_larb6",
		"camsys_dfp_vad",
		"camsys_larb3",
		"camsys_cam",
		"camsys_camtg",
		"camsys_seninf",
		"camsys_camsv0",
		"camsys_camsv1",
		"camsys_camsv2",
		"camsys_ccu",
	};
	*num = ARRAY_SIZE(clks);
	return clks;
}

static const char * const *get_img_clk_names(size_t *num)
{
	static const char * const clks[] = {

		/* IMG */
		"imgsys_larb5",
		"imgsys_larb2",
		"imgsys_dip",
		"imgsys_fdvt",
		"imgsys_dpe",
		"imgsys_rsc",
		"imgsys_mfb",
		"imgsys_wpe_a",
		"imgsys_wpe_b",
		"imgsys_owe",
	};
	*num = ARRAY_SIZE(clks);
	return clks;
}

static const char * const *get_mm_clk_names(size_t *num)
{
	static const char * const clks[] = {

		/* MM */
		"mm_smi_common",
		"mm_smi_larb0",
		"mm_smi_larb1",
		"mm_gals_comm0",
		"mm_gals_comm1",
		"mm_gals_ccu2mm",
		"mm_gals_ipu12mm",
		"mm_gals_img2mm",
		"mm_gals_cam2mm",
		"mm_gals_ipu2mm",
		"mm_mdp_dl_txck",
		"mm_ipu_dl_txck",
		"mm_mdp_rdma0",
		"mm_mdp_rdma1",
		"mm_mdp_rsz0",
		"mm_mdp_rsz1",
		"mm_mdp_tdshp",
		"mm_mdp_wrot0",
		"mm_mdp_wdma0",
		"mm_fake_eng",
		"mm_disp_ovl0",
		"mm_disp_ovl0_2l",
		"mm_disp_ovl1_2l",
		"mm_disp_rdma0",
		"mm_disp_rdma1",
		"mm_disp_wdma0",
		"mm_disp_color0",
		"mm_disp_ccorr0",
		"mm_disp_aal0",
		"mm_disp_gamma0",
		"mm_disp_dither0",
		"mm_disp_split",
		"mm_dsi0_mmck",
		"mm_dsi0_ifck",
		"mm_dpi_mmck",
		"mm_dpi_ifck",
		"mm_fake_eng2",
		"mm_mdp_dl_rxck",
		"mm_ipu_dl_rxck",
		"mm_26m",
		"mm_mmsys_r2y",
		"mm_disp_rsz",
		"mm_mdp_aal",
		"mm_mdp_ccorr",
		"mm_dbi_mmck",
		"mm_dbi_ifck",
	};
	*num = ARRAY_SIZE(clks);
	return clks;
}

static const char * const *get_mm_ccu_clk_names(size_t *num)
{
	static const char * const clks[] = {

		/* MM */
		"mm_smi_common",
		"mm_smi_larb0",
		"mm_smi_larb1",
		"mm_gals_comm0",
		"mm_gals_comm1",
		"mm_gals_ccu2mm",
		"mm_gals_ipu12mm",
		"mm_gals_img2mm",
		"mm_gals_cam2mm",
		"mm_gals_ipu2mm",
	};
	*num = ARRAY_SIZE(clks);
	return clks;
}

static const char * const *get_venc_clk_names(size_t *num)
{
	static const char * const clks[] = {

		/* VENC */
		"venc_larb",
		"venc_venc",
		"venc_jpgenc",
	};
	*num = ARRAY_SIZE(clks);
	return clks;
}

static const char * const *get_vdec_clk_names(size_t *num)
{
	static const char * const clks[] = {

		/* VDE */
		"vdec_cken",
		"vdec_larb1_cken",
	};
	*num = ARRAY_SIZE(clks);
	return clks;
}

static const char * const *get_cam_pwr_names(size_t *num)
{
	static const char * const clks[] = {

		/* PG */
		"pg_dis",
		"pg_cam",
	};
	*num = ARRAY_SIZE(clks);
	return clks;
}

static void dump_cg_state(const char *clkname)
{
	struct clk *c = __clk_lookup(clkname);

	if (IS_ERR_OR_NULL(c)) {
		pr_notice("[%17s: NULL]\n", clkname);
		return;
	}

	pr_notice("[%-17s: %3d]\n",
		__clk_get_name(c),
		__clk_get_enable_count(c));
}

void subsys_if_on(void)
{
	unsigned int sta = spm_read(PWR_STATUS);
	unsigned int sta_s = spm_read(PWR_STATUS_2ND);
	int ret = 0;
	int i = 0;
	size_t cam_num, img_num, mm_num, venc_num, vdec_num = 0;
	/*size_t num, cam_num, img_num, mm_num, venc_num, vdec_num = 0;*/

	/*const char * const *clks = get_all_clk_names(&num);*/
	const char * const *cam_clks = get_cam_clk_names(&cam_num);
	const char * const *img_clks = get_img_clk_names(&img_num);
	const char * const *mm_clks = get_mm_clk_names(&mm_num);
	const char * const *venc_clks = get_venc_clk_names(&venc_num);
	const char * const *vdec_clks = get_vdec_clk_names(&vdec_num);

	if ((sta & (1U << 0)) && (sta_s & (1U << 0)))
		pr_notice("suspend warning: SYS_MD1 is on!!!\n");
	if ((sta & (1U << 1)) && (sta_s & (1U << 1)))
		pr_notice("suspend warning: SYS_CONN is on!!!\n");
	if ((sta & (1U << 3)) && (sta_s & (1U << 3))) {
		pr_notice("suspend warning: SYS_DIS is on!!!\n");
		check_mm0_clk_sts();
		for (i = 0; i < mm_num; i++)
			dump_cg_state(mm_clks[i]);
		ret++;
	}
	if ((sta & (1U << 4)) && (sta_s & (1U << 4))) {
		pr_notice("suspend warning: SYS_MFG is on!!!\n");
		ret++;
	}
	if ((sta & (1U << 5)) && (sta_s & (1U << 5))) {
		pr_notice("suspend warning: SYS_ISP is on!!!\n");
		check_img_clk_sts();
		for (i = 0; i < img_num; i++)
			dump_cg_state(img_clks[i]);
		ret++;
	}
	if ((sta & (1U << 7)) && (sta_s & (1U << 7))) {
		pr_notice("suspend warning: SYS_MFG_CORE0 is on!!!\n");
		ret++;
	}
	if ((sta & (1U << 20)) && (sta_s & (1U << 20))) {
		pr_notice("suspend warning: SYS_MFG_CORE1 is on!!!\n");
		ret++;
	}
	if ((sta & (1U << 21)) && (sta_s & (1U << 21))) {
		pr_notice("suspend warning: SYS_VEN is on!!!\n");
		check_ven_clk_sts();
		for (i = 0; i < venc_num; i++)
			dump_cg_state(venc_clks[i]);
		ret++;
	}
	if ((sta & (1U << 22)) && (sta_s & (1U << 22))) {
		pr_notice("suspend warning: SYS_MFG_2D is on!!!\n");
		ret++;
	}
	if ((sta & (1U << 23)) && (sta_s & (1U << 23))) {
		pr_notice("suspend warning: SYS_MFG_ASYNC is on!!!\n");
		ret++;
	}
	if ((sta & (1U << 24)) && (sta_s & (1U << 24)))
		pr_notice("suspend warning: SYS_AUDIO is on!!!\n");
	if ((sta & (1U << 25)) && (sta_s & (1U << 25))) {
		pr_notice("suspend warning: SYS_CAM is on!!!\n");
		check_cam_clk_sts();
		for (i = 0; i < cam_num; i++)
			dump_cg_state(cam_clks[i]);
		ret++;
	}
	if ((sta & (1U << 26)) && (sta_s & (1U << 26))) {
		pr_notice("suspend warning: SYS_VPU_TOP is on!!!\n");
		ret++;
	}
	if ((sta & (1U << 27)) && (sta_s & (1U << 27))) {
		pr_notice("suspend warning: SYS_VPU_CORE0 is on!!!\n");
		ret++;
	}
	if ((sta & (1U << 28)) && (sta_s & (1U << 28))) {
		pr_notice("suspend warning: SYS_VPU_CORE1 is on!!!\n");
		ret++;
	}
	if ((sta & (1U << 31)) && (sta_s & (1U << 31))) {
		pr_notice("suspend warning: SYS_VDE is on!!!\n");
		for (i = 0; i < vdec_num; i++)
			dump_cg_state(vdec_clks[i]);
		ret++;
	}
	if (ret > 0)
		BUG_ON(1);
#if 0
	for (i = 0; i < num; i++)
		dump_cg_state(clks[i]);
#endif
}

void cam_mtcmos_check(void)
{
	unsigned int sta = spm_read(PWR_STATUS);
	unsigned int sta_s = spm_read(PWR_STATUS_2ND);
	int i = 0;
	size_t cam_num, mm_num, pwr_num = 0;

	/*const char * const *clks = get_all_clk_names(&num);*/
	const char * const *cam_clks = get_cam_clk_names(&cam_num);
	const char * const *mm_clks = get_mm_ccu_clk_names(&mm_num);
	const char * const *cam_pwrs = get_cam_pwr_names(&pwr_num);

	pr_notice("PWR_STATUS = %08x, %08x\n", spm_read(PWR_STATUS),
		spm_read(PWR_STATUS_2ND));
	pr_notice("PWR_CON = %08x, %08x\n", spm_read(DIS_PWR_CON),
		spm_read(CAM_PWR_CON));

	if ((sta & (1U << 3)) && (sta_s & (1U << 3))) {
		pr_notice("%s: SYS_DIS is on!!!\n", __func__);
		check_mm0_clk_sts();
	} else {
		pr_notice("%s: SYS_DIS is off!!!\n", __func__);
	}
	if ((sta & (1U << 25)) && (sta_s & (1U << 25))) {
		pr_notice("%s: SYS_CAM is on!!!\n", __func__);
		check_cam_clk_sts();
	} else {
		pr_notice("%s: SYS_CAM is off!!!\n", __func__);
	}
	for (i = 0; i < pwr_num; i++)
		dump_cg_state(cam_pwrs[i]);
	for (i = 0; i < mm_num; i++)
		dump_cg_state(mm_clks[i]);
	for (i = 0; i < cam_num; i++)
		dump_cg_state(cam_clks[i]);
}

#if 1 /*only use for suspend test*/
void mtcmos_force_off(void)
{
	spm_mtcmos_ctrl_mfg_2d(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg_core1(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg_core0(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg_async(STA_POWER_DOWN);

	spm_mtcmos_ctrl_vpu_core1_shut_down(STA_POWER_DOWN);
	spm_mtcmos_ctrl_vpu_core0_shut_down(STA_POWER_DOWN);
	spm_mtcmos_ctrl_vpu_top(STA_POWER_DOWN);

	spm_mtcmos_ctrl_cam(STA_POWER_DOWN);
	spm_mtcmos_ctrl_ven(STA_POWER_DOWN);
	spm_mtcmos_ctrl_vde(STA_POWER_DOWN);
	spm_mtcmos_ctrl_isp(STA_POWER_DOWN);
	spm_mtcmos_ctrl_dis(STA_POWER_DOWN);

	spm_mtcmos_ctrl_conn(STA_POWER_DOWN);

	spm_mtcmos_ctrl_md1(STA_POWER_DOWN);

	spm_mtcmos_ctrl_audio(STA_POWER_DOWN);
}
#endif

