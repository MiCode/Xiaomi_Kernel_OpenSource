/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <linux/io.h>
#include <linux/slab.h>p
#include <linux/delay.h>
#include <linux/clkdev.h>

#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include "clk-mtk-v1.h"
#include "clk-mt6785-pg.h"

#include <dt-bindings/clock/mt6785-clk.h>


#define MT_CCF_DEBUG	0
#define MT_CCF_BRINGUP  0
#define CONTROL_LIMIT 0

#define	CHECK_PWR_ST	1

#define CONN_TIMEOUT_RECOVERY	6
#define CONN_TIMEOUT_STEP1	5

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

void __attribute__((weak)) mtk_wcn_cmb_stub_clock_fail_dump(void) {}

/*MM Bus*/
#if 1
#ifdef CONFIG_OF
void __iomem *clk_mmsys_config_base;
void __iomem *clk_imgsys_base;
void __iomem *clk_vdec_gcon_base;
void __iomem *clk_venc_gcon_base;
void __iomem *clk_camsys_base;
void __iomem *clk_apu_vcore_base;
void __iomem *clk_apu_conn_base;
#endif


#define MM_CG_CLR0 (clk_mmsys_config_base + 0x108)
#define IMG_CG_CLR	(clk_imgsys_base + 0x0008)
#define VDEC_CKEN_SET	(clk_vdec_gcon_base + 0x0000)
#define VDEC_GALS_CFG (clk_vdec_gcon_base + 0x0168)
#define VENC_CG_SET	(clk_venc_gcon_base + 0x0004)

#define MM_CG_CLR0 (clk_mmsys_config_base + 0x108)
#define MM_CG_SET0 (clk_mmsys_config_base + 0x104)
#define IMG_CG_CLR	(clk_imgsys_base + 0x0008)
#define CAM_CG_CLR	(clk_camsys_base + 0x0008)
#define CAM_CG_CON	(clk_camsys_base + 0x0000)
#define APU_VCORE_CG_CON	(clk_apu_vcore_base)
#define APU_CONN_CG_CON	(clk_apu_conn_base)
#define APU_VCORE_CG_CLR	(clk_apu_vcore_base + 0x0008)
#define APU_CONN_CG_CLR	(clk_apu_conn_base + 0x0008)
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
static struct subsys_ops CAM_sys_ops;
static struct subsys_ops ISP_sys_ops;
static struct subsys_ops VEN_sys_ops;
static struct subsys_ops VDE_sys_ops;

static struct subsys_ops AUDIO_sys_ops;

static struct subsys_ops MFG0_sys_ops;
static struct subsys_ops MFG1_sys_ops;
static struct subsys_ops MFG2_sys_ops;
static struct subsys_ops MFG3_sys_ops;
static struct subsys_ops MFG4_sys_ops;
static struct subsys_ops MFG4_sys_ops;
static struct subsys_ops MFG5_sys_ops;

static struct subsys_ops VPU_VCORE_DORMANT_sys_ops;
static struct subsys_ops VPU_VCORE_SHUTDOWN_sys_ops;
static struct subsys_ops VPU_CORE0_DORMANT_sys_ops;
static struct subsys_ops VPU_CORE0_SHUTDOWN_sys_ops;
static struct subsys_ops VPU_CORE1_DORMANT_sys_ops;
static struct subsys_ops VPU_CORE1_SHUTDOWN_sys_ops;
static struct subsys_ops VPU_CORE2_DORMANT_sys_ops;
static struct subsys_ops VPU_CORE2_SHUTDOWN_sys_ops;
static struct subsys_ops VPU_CONN_DORMANT_sys_ops;
static struct subsys_ops VPU_CONN_SHUTDOWN_sys_ops;

static void __iomem *infracfg_base;/*infracfg_ao*/
static void __iomem *spm_base;
static void __iomem *infra_base;/*infracfg*/
static void __iomem *ckgen_base;/*ckgen*/
static void __iomem *smi_common_base;

#define INFRACFG_REG(offset)	(infracfg_base + offset)
#define SPM_REG(offset)		(spm_base + offset)
#define INFRA_REG(offset)	(infra_base + offset)
#define CKGEN_REG(offset)	(ckgen_base + offset)
#define SMI_COMMON_REG(offset)	(smi_common_base + offset)

#define POWERON_CONFIG_EN	SPM_REG(0x0000)
#define PWR_STATUS		SPM_REG(0x0160)
#define PWR_STATUS_2ND		SPM_REG(0x0164)

#define MD1_PWR_CON	SPM_REG(0x318)
#define CONN_PWR_CON	SPM_REG(0x320)

#define DIS_PWR_CON	SPM_REG(0x030C)
#define VEN_PWR_CON	SPM_REG(0x0304)
#define VDE_PWR_CON	SPM_REG(0x0300)
#define CAM_PWR_CON	SPM_REG(0x0324)
#define ISP_PWR_CON	SPM_REG(0x0308)

#define AUDIO_PWR_CON	SPM_REG(0x31C)
#define MFG0_PWR_CON	SPM_REG(0x328)
#define MFG1_PWR_CON	SPM_REG(0x32C)
#define MFG2_PWR_CON	SPM_REG(0x330)
#define MFG3_PWR_CON	SPM_REG(0x334)
#define MFG4_PWR_CON	SPM_REG(0x338)
#define MFG5_PWR_CON	SPM_REG(0x350)//new, reuse ipe

#define VPU_VCORE_PWR_CON	SPM_REG(0x33C)
#define VPU_CONN_PWR_CON	SPM_REG(0x340)
#define VPU_CORE0_PWR_CON	SPM_REG(0x344)
#define VPU_CORE1_PWR_CON	SPM_REG(0x348)
#define VPU_CORE2_PWR_CON	SPM_REG(0x34C)

#define VPU_VCORE_SRAM_CON	SPM_REG(0x384)
#define VPU_CONN_SRAM_CON	SPM_REG(0x388)
#define VPU_CORE0_SRAM_CON	SPM_REG(0x38C)
#define VPU_CORE1_SRAM_CON	SPM_REG(0x390)
#define VPU_CORE2_SRAM_CON	SPM_REG(0x394)

#define VPU_CORE0_SRAM_STA	SPM_REG(0x398)
#define VPU_CORE1_SRAM_STA	SPM_REG(0x39C)
#define VPU_CORE2_SRAM_STA	SPM_REG(0x3A0)

#define MD_EXT_BUCK_ISO_CON	SPM_REG(0x3B0)
#define EXT_BUCK_ISO		SPM_REG(0x3B4)

#define INFRA_TOPAXI_SI0_CTL		INFRACFG_REG(0x0200)
#define INFRA_TOPAXI_SI0_CTL_SET	INFRACFG_REG(0x03B8)
#define INFRA_TOPAXI_SI0_CTL_CLR	INFRACFG_REG(0x03BC)

#define INFRA_TOPAXI_SI2_CTL		INFRACFG_REG(0x0234)

#define INFRA_TOPAXI_PROTECTEN		INFRACFG_REG(0x0220)
#define INFRA_TOPAXI_PROTECTEN_SET	INFRACFG_REG(0x02A0)
#define INFRA_TOPAXI_PROTECTEN_CLR	INFRACFG_REG(0x02A4)
#define INFRA_TOPAXI_PROTECTEN_STA0	INFRACFG_REG(0x0224)
#define INFRA_TOPAXI_PROTECTEN_STA1	INFRACFG_REG(0x0228)

#define INFRA_TOPAXI_PROTECTEN_1	INFRACFG_REG(0x0250)
#define INFRA_TOPAXI_PROTECTEN_1_SET	INFRACFG_REG(0x02A8)
#define INFRA_TOPAXI_PROTECTEN_1_CLR	INFRACFG_REG(0x02AC)
#define INFRA_TOPAXI_PROTECTEN_STA0_1	INFRACFG_REG(0x0254)
#define INFRA_TOPAXI_PROTECTEN_STA1_1	INFRACFG_REG(0x0258)

#define INFRA_TOPAXI_PROTECTEN_2	INFRACFG_REG(0x0710)
#define INFRA_TOPAXI_PROTECTEN_2_SET	INFRACFG_REG(0x0714)
#define INFRA_TOPAXI_PROTECTEN_2_CLR	INFRACFG_REG(0x0718)
#define INFRA_TOPAXI_PROTECTEN_STA0_2	INFRACFG_REG(0x0720)
#define INFRA_TOPAXI_PROTECTEN_STA1_2	INFRACFG_REG(0x0724)

#define INFRA_TOPAXI_PROTECTEN_MM	INFRACFG_REG(0x02D0)
#define INFRA_TOPAXI_PROTECTEN_MM_SET	INFRACFG_REG(0x02D4)
#define INFRA_TOPAXI_PROTECTEN_MM_CLR	INFRACFG_REG(0x02D8)
#define INFRA_TOPAXI_PROTECTEN_MM_STA0	INFRACFG_REG(0x02E8)
#define INFRA_TOPAXI_PROTECTEN_MM_STA1	INFRACFG_REG(0x02EC)

#define INFRA_TOPAXI_PROTECTEN_MCU	INFRACFG_REG(0x02C0)
#define INFRA_TOPAXI_PROTECTEN_MCU_SET	INFRACFG_REG(0x02C4)
#define INFRA_TOPAXI_PROTECTEN_MCU_CLR	INFRACFG_REG(0x02C8)
#define INFRA_TOPAXI_PROTECTEN_MCU_STA0	INFRACFG_REG(0x02E0)
#define INFRA_TOPAXI_PROTECTEN_MCU_STA1	INFRACFG_REG(0x02E4)
/* INFRACFG */
#define INFRA_TOPAXI_SI0_STA		INFRA_REG(0x0000)
#define INFRA_TOPAXI_SI2_STA		INFRA_REG(0x0028)
/* SMI LARB */

/* SMI COMMON */
#define SMI_COMMON_SMI_CLAMP		SMI_COMMON_REG(0x03C0)
#define SMI_COMMON_SMI_CLAMP_SET	SMI_COMMON_REG(0x03C4)
#define SMI_COMMON_SMI_CLAMP_CLR	SMI_COMMON_REG(0x03C8)

#define  SPM_PROJECT_CODE    0xB16

/* Define MTCMOS power control */
#define PWR_RST_B                        (0x1 << 0)
#define PWR_ISO                          (0x1 << 1)
#define PWR_ON                           (0x1 << 2)
#define PWR_ON_2ND                       (0x1 << 3)
#define PWR_CLK_DIS                      (0x1 << 4)
#define SRAM_CKISO                       (0x1 << 5)
#define SRAM_ISOINT_B                    (0x1 << 6)
#define DORMANT_ENABLE                   (0x1 << 6)
#define VPROC_EXT_OFF                    (0x1 << 7)
#define SLPB_CLAMP                       (0x1 << 7)
#define VPU_CORE2_SRAM_PDN_ACK_BIT0      (0x1 << 16)
#define VPU_CORE0_SRAM_PDN_ACK_BIT0      (0x1 << 16)
#define VPU_CORE1_SRAM_PDN_ACK_BIT0      (0x1 << 16)
#define VPU_CORE2_SRAM_PDN_ACK_BIT1      (0x1 << 17)
#define VPU_CORE0_SRAM_PDN_ACK_BIT1      (0x1 << 17)
#define VPU_CORE1_SRAM_PDN_ACK_BIT1      (0x1 << 17)
#define VPU_CONN_SRAM_PDN_ACK_BIT0       (0x1 << 28)

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
					  |(0x1 << 18))
#define CONN_PROT_STEP1_0_ACK_MASK       ((0x1 << 13) \
					  |(0x1 << 18))
#define CONN_PROT_STEP2_0_MASK           ((0x1 << 14))
#define CONN_PROT_STEP2_0_ACK_MASK       ((0x1 << 14))
#define CONN_PROT_STEP2_1_MASK           ((0x1 << 10))
#define CONN_PROT_STEP2_1_ACK_MASK       ((0x1 << 10))
#define DIS_PROT_STEP1_0_MASK            ((0x1 << 0) \
					  |(0x1 << 1) \
					  |(0x1 << 3) \
					  |(0x1 << 4) \
					  |(0x1 << 5))

#define DIS_PROT_STEP1_0_ACK_MASK        ((0x1 << 0) \
					  |(0x1 << 1) \
					  |(0x1 << 3) \
					  |(0x1 << 4) \
					  |(0x1 << 5))

#define DIS_PROT_STEP2_0_MASK            ((0x1 << 0) \
					  |(0x1 << 1) \
					  |(0x1 << 2) \
					  |(0x1 << 3) \
					  |(0x1 << 4) \
					  |(0x1 << 6) \
					  |(0x1 << 7))
#define DIS_PROT_STEP2_0_ACK_MASK        ((0x1 << 0) \
					  |(0x1 << 1) \
					  |(0x1 << 2) \
					  |(0x1 << 3) \
					  |(0x1 << 4) \
					  |(0x1 << 6) \
					  |(0x1 << 7))
#define DIS_PROT_STEP3_0_MASK            ((0x1 << 16) \
					  |(0x1 << 17))
#define DIS_PROT_STEP3_0_ACK_MASK        ((0x1 << 16) \
					  |(0x1 << 17))
#define DIS_PROT_STEP4_0_MASK            ((0x1 << 10) \
					  |(0x1 << 11))
#define DIS_PROT_STEP4_0_ACK_MASK        ((0x1 << 10) \
					  |(0x1 << 11))
#define DIS_PROT_STEP5_0_MASK            ((0x1 << 6))
#define DIS_PROT_STEP5_0_ACK_MASK        ((0x1 << 6))
#define ISP_PROT_STEP1_0_MASK            ((0x1 << 3) \
					  |(0x1 << 8))
#define ISP_PROT_STEP1_0_ACK_MASK        ((0x1 << 3) \
					  |(0x1 << 8))
#define ISP_PROT_STEP2_0_MASK            ((0x1 << 10))
#define ISP_PROT_STEP2_0_ACK_MASK        ((0x1 << 10))
#define ISP_PROT_STEP2_1_MASK            ((0x1 << 4))
#define ISP_PROT_STEP2_1_ACK_MASK        ((0x1 << 4))
#define IFR_PROT_STEP1_0_MASK            ((0x1 << 0) \
					  |(0x1 << 3) \
					  |(0x1 << 4) \
					  |(0x1 << 5) \
					  |(0x1 << 7) \
					  |(0x1 << 8) \
					  |(0x1 << 10) \
					  |(0x1 << 11) \
					  |(0x1 << 29) \
					  |(0x1 << 30) \
					  |(0x1 << 31))
#define IFR_PROT_STEP1_0_ACK_MASK        ((0x1 << 0) \
					  |(0x1 << 3) \
					  |(0x1 << 4) \
					  |(0x1 << 5) \
					  |(0x1 << 7) \
					  |(0x1 << 8) \
					  |(0x1 << 10) \
					  |(0x1 << 11) \
					  |(0x1 << 29) \
					  |(0x1 << 30) \
					  |(0x1 << 31))
#define IFR_PROT_STEP1_1_MASK            ((0x1 << 6) \
					  |(0x1 << 16) \
					  |(0x1 << 17) \
					  |(0x1 << 19) \
					  |(0x1 << 20) \
					  |(0x1 << 21) \
					  |(0x1 << 29))
#define IFR_PROT_STEP1_1_ACK_MASK        ((0x1 << 6) \
					  |(0x1 << 16) \
					  |(0x1 << 17) \
					  |(0x1 << 19) \
					  |(0x1 << 20) \
					  |(0x1 << 21) \
					  |(0x1 << 29))
#define IFR_PROT_STEP1_2_MASK            ((0x1 << 12) \
					  |(0x1 << 13))
#define IFR_PROT_STEP1_2_ACK_MASK        ((0x1 << 12) \
					  |(0x1 << 13))
#define IFR_PROT_STEP2_0_MASK            ((0x1 << 6) \
					  |(0x1 << 17) \
					  |(0x1 << 21) \
					  |(0x1 << 22) \
					  |(0x1 << 23) \
					  |(0x1 << 27) \
					  |(0x1 << 28))
#define IFR_PROT_STEP2_0_ACK_MASK        ((0x1 << 6) \
					  |(0x1 << 17) \
					  |(0x1 << 21) \
					  |(0x1 << 22) \
					  |(0x1 << 23) \
					  |(0x1 << 27) \
					  |(0x1 << 28))
#define IFR_PROT_STEP2_1_MASK            ((0x1 << 0) \
					  |(0x1 << 1))
#define IFR_PROT_STEP2_1_ACK_MASK        ((0x1 << 0) \
					  |(0x1 << 1))
#define MFG1_PROT_STEP1_0_MASK           ((0x1 << 19) \
					  |(0x1 << 20) \
					  |(0x1 << 21))
#define MFG1_PROT_STEP1_0_ACK_MASK       ((0x1 << 19) \
					  |(0x1 << 20) \
					  |(0x1 << 21))
#define MFG1_PROT_STEP2_0_MASK           ((0x1 << 21) \
					  |(0x1 << 22))
#define MFG1_PROT_STEP2_0_ACK_MASK       ((0x1 << 21) \
					  |(0x1 << 22))
#define MP0_SPMC_PROT_STEP1_0_MASK       ((0x1 << 12))
#define MP0_SPMC_PROT_STEP1_0_ACK_MASK   ((0x1 << 12))
#define MP0_SPMC_PROT_STEP1_1_MASK       ((0x1 << 12) \
					  |(0x1 << 26))
#define MP0_SPMC_PROT_STEP1_1_ACK_MASK   ((0x1 << 12) \
					  |(0x1 << 26))
#define MCUSYS_SPMC_PROT_STEP1_0_MASK    ((0x1 << 0) \
					  |(0x1 << 5) \
					  |(0x1 << 9))
#define MCUSYS_SPMC_PROT_STEP1_0_ACK_MASK   ((0x1 << 0) \
					  |(0x1 << 5) \
					  |(0x1 << 9))
#define MCUSYS_SPMC_PROT_STEP1_1_MASK    ((0x1 << 8) \
					  |(0x1 << 9) \
					  |(0x1 << 25))
#define MCUSYS_SPMC_PROT_STEP1_1_ACK_MASK   ((0x1 << 8) \
					  |(0x1 << 9) \
					  |(0x1 << 25))
#define VEN_PROT_STEP1_0_MASK            ((0x1 << 0))
#define VEN_PROT_STEP1_0_ACK_MASK        ((0x1 << 0))
#define VEN_PROT_STEP2_0_MASK            ((0x1 << 3))
#define VEN_PROT_STEP2_0_ACK_MASK        ((0x1 << 3))
#define AUDIO_PROT_STEP1_0_MASK          ((0x1 << 31))
#define AUDIO_PROT_STEP1_0_ACK_MASK      ((0x1 << 31))
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
#define CAM_PROT_STEP2_2_MASK            ((0x1 << 6) \
					  |(0x1 << 7))
#define CAM_PROT_STEP2_2_ACK_MASK        ((0x1 << 6) \
					  |(0x1 << 7))
#define VPU_CONN_PROT_STEP1_0_MASK       ((0x1 << 8) \
					  |(0x1 << 9) \
					  |(0x1 << 12))
#define VPU_CONN_PROT_STEP1_0_ACK_MASK   ((0x1 << 8) \
					  |(0x1 << 9) \
					  |(0x1 << 12))
#define VPU_CONN_PROT_STEP1_1_MASK       ((0x1 << 12) \
					  |(0x1 << 13))
#define VPU_CONN_PROT_STEP1_1_ACK_MASK   ((0x1 << 12) \
					  |(0x1 << 13))
#define VPU_CONN_PROT_STEP2_0_MASK       ((0x1 << 0) \
					  |(0x1 << 1) \
					  |(0x1 << 6))
#define VPU_CONN_PROT_STEP2_0_ACK_MASK   ((0x1 << 0) \
					  |(0x1 << 1) \
					  |(0x1 << 6))
#define VPU_CONN_PROT_STEP2_1_MASK       ((0x1 << 10) \
					  |(0x1 << 11))
#define VPU_CONN_PROT_STEP2_1_ACK_MASK   ((0x1 << 10) \
					  |(0x1 << 11))
#define VPU_CORE0_PROT_STEP1_0_MASK      ((0x1 << 6))
#define VPU_CORE0_PROT_STEP1_0_ACK_MASK   ((0x1 << 6))
#define VPU_CORE0_PROT_STEP2_0_MASK      ((0x1 << 0) \
					  |(0x1 << 2) \
					  |(0x1 << 4) \
					  |(0x1 << 14))
#define VPU_CORE0_PROT_STEP2_0_ACK_MASK   ((0x1 << 0) \
					  |(0x1 << 2) \
					  |(0x1 << 4) \
					  |(0x1 << 14))
#define VPU_CORE1_PROT_STEP1_0_MASK      ((0x1 << 7))
#define VPU_CORE1_PROT_STEP1_0_ACK_MASK   ((0x1 << 7))
#define VPU_CORE1_PROT_STEP2_0_MASK      ((0x1 << 1) \
					  |(0x1 << 3) \
					  |(0x1 << 5) \
					  |(0x1 << 15))
#define VPU_CORE1_PROT_STEP2_0_ACK_MASK   ((0x1 << 1) \
					  |(0x1 << 3) \
					  |(0x1 << 5) \
					  |(0x1 << 15))
#define VPU_CORE2_PROT_STEP1_0_MASK      ((0x1 << 8) \
					  |(0x1 << 9) \
					  |(0x1 << 10))
#define VPU_CORE2_PROT_STEP1_0_ACK_MASK   ((0x1 << 8) \
					  |(0x1 << 9) \
					  |(0x1 << 10))
#define VDE_PROT_STEP1_0_MASK            ((0x1 << 1))
#define VDE_PROT_STEP1_0_ACK_MASK        ((0x1 << 1))
#define VDE_PROT_STEP2_0_MASK            ((0x1 << 2))
#define VDE_PROT_STEP2_0_ACK_MASK        ((0x1 << 2))

/* Define MTCMOS Power Status Mask */

#define MD1_PWR_STA_MASK                 (0x1 << 0)
#define CONN_PWR_STA_MASK                (0x1 << 1)
#define DPY_PWR_STA_MASK                 (0x1 << 2)
#define DIS_PWR_STA_MASK                 (0x1 << 3)
#define MFG0_PWR_STA_MASK                (0x1 << 4)
#define ISP_PWR_STA_MASK                 (0x1 << 5)
#define IFR_PWR_STA_MASK                 (0x1 << 6)
#define MFG1_PWR_STA_MASK                (0x1 << 7)
#define MP0_SPMC_PWR_STA_MASK            (0x1 << 8)
#define MP0_CPU0_SPMC_PWR_STA_MASK       (0x1 << 9)
#define MP0_CPU1_SPMC_PWR_STA_MASK       (0x1 << 10)
#define MP0_CPU2_SPMC_PWR_STA_MASK       (0x1 << 11)
#define MP0_CPU3_SPMC_PWR_STA_MASK       (0x1 << 12)
#define MFG5_PWR_STA_MASK                (0x1 << 13)
#define MCUSYS_SPMC_PWR_STA_MASK         (0x1 << 14)
#define MP0_CPU4_SPMC_PWR_STA_MASK       (0x1 << 15)
#define MP0_CPU5_SPMC_PWR_STA_MASK       (0x1 << 16)
#define MP0_CPU6_SPMC_PWR_STA_MASK       (0x1 << 17)
#define MP0_CPU7_SPMC_PWR_STA_MASK       (0x1 << 18)
#define MFG2_PWR_STA_MASK                (0x1 << 20)
#define VEN_PWR_STA_MASK                 (0x1 << 21)
#define MFG3_PWR_STA_MASK                (0x1 << 22)
#define MFG4_PWR_STA_MASK                (0x1 << 23)
#define AUDIO_PWR_STA_MASK               (0x1 << 24)
#define CAM_PWR_STA_MASK                 (0x1 << 25)
#define VPU_VCORE_PWR_STA_MASK           (0x1 << 26)
#define VPU_CONN_PWR_STA_MASK            (0x1 << 27)
#define VPU_CORE0_PWR_STA_MASK           (0x1 << 28)
#define VPU_CORE1_PWR_STA_MASK           (0x1 << 29)
#define VPU_CORE2_PWR_STA_MASK           (0x1 << 30)
#define VDE_PWR_STA_MASK                 (0x1 << 31)

/* Define CPU SRAM Mask */

/* Define Non-CPU SRAM Mask */
#define DPY_SRAM_PDN                     (0xF << 8)
#define DPY_SRAM_PDN_ACK                 (0xF << 12)
#define DPY_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define DPY_SRAM_PDN_ACK_BIT1            (0x1 << 13)
#define DPY_SRAM_PDN_ACK_BIT2            (0x1 << 14)
#define DPY_SRAM_PDN_ACK_BIT3            (0x1 << 15)
#define DIS_SRAM_PDN                     (0x1 << 8)
#define DIS_SRAM_PDN_ACK                 (0x1 << 12)
#define DIS_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define ISP_SRAM_PDN                     (0x1 << 8)
#define ISP_SRAM_PDN_ACK                 (0x1 << 12)
#define ISP_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define IFR_SRAM_PDN                     (0xF << 8)
#define IFR_SRAM_PDN_ACK                 (0xF << 12)
#define IFR_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define IFR_SRAM_PDN_ACK_BIT1            (0x1 << 13)
#define IFR_SRAM_PDN_ACK_BIT2            (0x1 << 14)
#define IFR_SRAM_PDN_ACK_BIT3            (0x1 << 15)
#define MFG1_SRAM_PDN                    (0x3 << 8)
#define MFG1_SRAM_PDN_ACK                (0x3 << 12)
#define MFG1_SRAM_PDN_ACK_BIT0           (0x1 << 12)
#define MFG1_SRAM_PDN_ACK_BIT1           (0x1 << 13)
#define MFG5_SRAM_PDN                    (0x1 << 8)
#define MFG5_SRAM_PDN_ACK                (0x1 << 12)
#define MFG5_SRAM_PDN_ACK_BIT0           (0x1 << 12)
#define MFG2_SRAM_PDN                    (0x1 << 8)
#define MFG2_SRAM_PDN_ACK                (0x1 << 12)
#define MFG2_SRAM_PDN_ACK_BIT0           (0x1 << 12)
#define VEN_SRAM_PDN                     (0xF << 8)
#define VEN_SRAM_PDN_ACK                 (0xF << 12)
#define VEN_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define VEN_SRAM_PDN_ACK_BIT1            (0x1 << 13)
#define VEN_SRAM_PDN_ACK_BIT2            (0x1 << 14)
#define VEN_SRAM_PDN_ACK_BIT3            (0x1 << 15)
#define MFG3_SRAM_PDN                    (0x1 << 8)
#define MFG3_SRAM_PDN_ACK                (0x1 << 12)
#define MFG3_SRAM_PDN_ACK_BIT0           (0x1 << 12)
#define MFG4_SRAM_PDN                    (0x1 << 8)
#define MFG4_SRAM_PDN_ACK                (0x1 << 12)
#define MFG4_SRAM_PDN_ACK_BIT0           (0x1 << 12)
#define AUDIO_SRAM_PDN                   (0x1 << 8)
#define AUDIO_SRAM_PDN_ACK               (0x1 << 12)
#define AUDIO_SRAM_PDN_ACK_BIT0          (0x1 << 12)
#define CAM_SRAM_PDN                     (0x3 << 8)
#define CAM_SRAM_PDN_ACK                 (0x3 << 12)
#define CAM_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define CAM_SRAM_PDN_ACK_BIT1            (0x1 << 13)
#define VDE_SRAM_PDN                     (0x1 << 8)
#define VDE_SRAM_PDN_ACK                 (0x1 << 12)
#define VDE_SRAM_PDN_ACK_BIT0            (0x1 << 12)


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
	[SYS_DIS] = {
		     .name = __stringify(SYS_DIS),
		     .sta_mask = DIS_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
		     .sram_pdn_bits = 0,
		     .sram_pdn_ack_bits = 0,
		     .bus_prot_mask = 0,
		     .ops = &DIS_sys_ops,
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
	[SYS_VDE] = {
		     .name = __stringify(SYS_VDE),
		     .sta_mask = VDE_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
		     .sram_pdn_bits = 0,
		     .sram_pdn_ack_bits = 0,
		     .bus_prot_mask = 0,
		     .ops = &VDE_sys_ops,
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
	[SYS_ISP] = {
		     .name = __stringify(SYS_ISP),
		     .sta_mask = ISP_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
		     .sram_pdn_bits = 0,
		     .sram_pdn_ack_bits = 0,
		     .bus_prot_mask = 0,
		     .ops = &ISP_sys_ops,
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
	[SYS_MFG0] = {
			   .name = __stringify(SYS_MFG0),
			   .sta_mask = MFG0_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
			   .sram_pdn_bits = 0,
			   .sram_pdn_ack_bits = 0,
			   .bus_prot_mask = 0,
			   .ops = &MFG0_sys_ops,
			   },
	[SYS_MFG1] = {
		     .name = __stringify(SYS_MFG1),
		     .sta_mask = MFG1_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
		     .sram_pdn_bits = 0,
		     .sram_pdn_ack_bits = 0,
		     .bus_prot_mask = 0,
		     .ops = &MFG1_sys_ops,
		     },
	[SYS_MFG2] = {
			   .name = __stringify(SYS_MFG2),
			   .sta_mask = MFG2_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
			   .sram_pdn_bits = 0,
			   .sram_pdn_ack_bits = 0,
			   .bus_prot_mask = 0,
			   .ops = &MFG2_sys_ops,
			   },
	[SYS_MFG3] = {
			   .name = __stringify(SYS_MFG3),
			   .sta_mask = MFG3_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
			   .sram_pdn_bits = 0,
			   .sram_pdn_ack_bits = 0,
			   .bus_prot_mask = 0,
			   .ops = &MFG3_sys_ops,
			   },
	[SYS_MFG4] = {
		     .name = __stringify(SYS_MFG4),
		     .sta_mask = MFG4_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
		     .sram_pdn_bits = 0,
		     .sram_pdn_ack_bits = 0,
		     .bus_prot_mask = 0,
		     .ops = &MFG4_sys_ops,
		     },
	[SYS_VPU_VCORE_DORMANT] = {
		     .name = __stringify(SYS_VPU_VCORE_DORMANT),
		     .sta_mask = VPU_VCORE_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
		     .sram_pdn_bits = 0,
		     .sram_pdn_ack_bits = 0,
		     .bus_prot_mask = 0,
		     .ops = &VPU_VCORE_DORMANT_sys_ops,
		     },
	[SYS_VPU_VCORE_SHUTDOWN] = {
		     .name = __stringify(SYS_VPU_VCORE_SHUTDOWN),
		     .sta_mask = VPU_VCORE_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
		     .sram_pdn_bits = 0,
		     .sram_pdn_ack_bits = 0,
		     .bus_prot_mask = 0,
		     .ops = &VPU_VCORE_SHUTDOWN_sys_ops,
		     },
	[SYS_VPU_CONN_DORMANT] = {
		     .name = __stringify(SYS_VPU_CONN_DORMANT),
		     .sta_mask = VPU_CONN_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
		     .sram_pdn_bits = 0,
		     .sram_pdn_ack_bits = 0,
		     .bus_prot_mask = 0,
		     .ops = &VPU_CONN_DORMANT_sys_ops,
		     },
	[SYS_VPU_CONN_SHUTDOWN] = {
		     .name = __stringify(SYS_VPU_CONN_SHUTDOWN),
		     .sta_mask = VPU_CONN_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
		     .sram_pdn_bits = 0,
		     .sram_pdn_ack_bits = 0,
		     .bus_prot_mask = 0,
		     .ops = &VPU_CONN_SHUTDOWN_sys_ops,
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
	[SYS_MFG5] = {
		     .name = __stringify(SYS_MFG5),
		     .sta_mask = MFG5_PWR_STA_MASK,
		     /* .ctl_addr = NULL,  */
		     .sram_pdn_bits = 0,
		     .sram_pdn_ack_bits = 0,
		     .bus_prot_mask = 0,
		     .ops = &MFG5_sys_ops,
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

#define DBG_ID_MD1 0
#define DBG_ID_CONN 1
#define DBG_ID_DIS 2
#define DBG_ID_VEN 3
#define DBG_ID_VDE 4
#define DBG_ID_CAM 5
#define DBG_ID_ISP 6
#define DBG_ID_AUDIO 7
#define DBG_ID_MFG0 8
#define DBG_ID_MFG1 9
#define DBG_ID_MFG2 10
#define DBG_ID_MFG3 11
#define DBG_ID_MFG4 12
#define DBG_ID_VPU_VCORE_DORMANT 13
#define DBG_ID_VPU_VCORE_SHUTDOWN 14
#define DBG_ID_VPU_CONN_DORMANT 15
#define DBG_ID_VPU_CONN_SHUTDOWN 16
#define DBG_ID_VPU_CORE0_DORMANT 17
#define DBG_ID_VPU_CORE0_SHUTDOWN 18
#define DBG_ID_VPU_CORE1_DORMANT 19
#define DBG_ID_VPU_CORE1_SHUTDOWN 20
#define DBG_ID_VPU_CORE2_DORMANT 21
#define DBG_ID_VPU_CORE2_SHUTDOWN 22
#define DBG_ID_MFG5 23

#define ID_MADK   0xFF000000
#define STA_MASK  0x00F00000
#define STEP_MASK 0x000000FF

#define INCREASE_STEPS \
	do { DBG_STEP++; } while (0)

/*static int first_conn = 1;*/
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
	struct pg_callbacks *pgcb;
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
	data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN_STA1);
	/*data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN_STA1_1);*/
	/*data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN_MM);*/
	data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN_MM_STA1);
	/*data[++i] = clk_readl(SMI_COMMON_SMI_CLAMP);*/
	data[++i] = clk_readl(PWR_STATUS);
	data[++i] = clk_readl(PWR_STATUS_2ND);
	data[++i] = clk_readl(CAM_PWR_CON);


	if (pre_data == data[0])
		k++;
	else if (pre_data != data[0]) {
		k = 0;
		pre_data = data[0];
		print_once = true;
	}

	if (k > 10000 && print_once) {
		print_once = false;
		k = 0;

#if 0
		if (DBG_ID == DBG_ID_CONN) {
			if (DBG_STEP == 0 && DBG_STA == STA_POWER_DOWN) {
				/* TINFO="Release bus protect - step1 : 0" */
				spm_write(INFRA_TOPAXI_PROTECTEN_CLR,
					CONN_PROT_STEP1_0_13_MASK);
			}
		}
#endif

		print_enabled_clks_once();

		pr_notice("%s: clk = 0x%08x\n", __func__, data[0]);
		pr_notice("%s: INFRA_TOPAXI_PROTECTEN = 0x%08x\n",
			__func__, clk_readl(INFRA_TOPAXI_PROTECTEN));
		pr_notice("%s: INFRA_TOPAXI_PROTECTEN_STA0 = 0x%08x\n",
			__func__, clk_readl(INFRA_TOPAXI_PROTECTEN_STA0));
		pr_notice("%s: INFRA_TOPAXI_PROTECTEN_STA1 = 0x%08x\n",
			__func__, clk_readl(INFRA_TOPAXI_PROTECTEN_STA1));

		pr_notice("%s: INFRA_TOPAXI_PROTECTEN_1 = 0x%08x\n",
			__func__, clk_readl(INFRA_TOPAXI_PROTECTEN_1));
		pr_notice("%s: INFRA_TOPAXI_PROTECTEN_STA0_1 = 0x%08x\n",
			__func__, clk_readl(INFRA_TOPAXI_PROTECTEN_STA0_1));
		pr_notice("%s: INFRA_TOPAXI_PROTECTEN_STA1_1 = 0x%08x\n",
			__func__, clk_readl(INFRA_TOPAXI_PROTECTEN_STA1_1));

		pr_notice("%s: INFRA_TOPAXI_PROTECTEN_MM = 0x%08x\n",
			__func__, clk_readl(INFRA_TOPAXI_PROTECTEN_MM));
		pr_notice("%s: INFRA_TOPAXI_PROTECTEN_MM_STA1 = 0x%08x\n",
			__func__, clk_readl(INFRA_TOPAXI_PROTECTEN_MM_STA1));
		pr_notice("%s: SMI_COMMON_SMI_CLAMP = 0x%08x\n",
			__func__, clk_readl(SMI_COMMON_SMI_CLAMP));

		pr_notice("%s: INFRA_TOPAXI_PROTECTEN_MCU = 0x%08x\n",
			__func__, clk_readl(INFRA_TOPAXI_PROTECTEN_MCU));
		pr_notice("%s: INFRA_TOPAXI_PROTECTEN_MCU_STA1 = 0x%08x\n",
			__func__, clk_readl(INFRA_TOPAXI_PROTECTEN_MCU_STA1));

#if 0
		if (DBG_ID == DBG_ID_CAM)
			check_cam_clk_sts();

		if (DBG_ID == DBG_ID_VPU_CONN_SHUTDOWN) {
			pr_notice("%s: APU_VCORE_CG_CON = 0x%08x\n",
			__func__, clk_readl(APU_VCORE_CG_CON));
			pr_notice("%s: APU_CONN_CG_CON = 0x%08x\n",
			__func__, clk_readl(APU_CONN_CG_CON));
		}
#endif

		list_for_each_entry_reverse(pgcb, &pgcb_list, list) {
			if (pgcb->debug_dump)
				pgcb->debug_dump(DBG_ID);
		}

#if 0
		if (DBG_ID == DBG_ID_CONN) {
			if (DBG_STEP == 0 && DBG_STA == STA_POWER_DOWN) {
				/* TINFO="Set bus protect - step1 : 0" */
				spm_write(INFRA_TOPAXI_PROTECTEN_SET,
					CONN_PROT_STEP1_0_13_MASK);
				j = 0;
				while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
					& CONN_PROT_STEP1_0_13_ACK_MASK)
					!= CONN_PROT_STEP1_0_13_ACK_MASK) {
					udelay(1);
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
				pre_data = data[0];
			}
		}
#endif
	}
	for (j = 0; j <= i; j++)
		aee_rr_rec_clk(j, data[j]);
	/*todo: add each domain's debug register to ram console*/
#endif
}

/* auto-gen begin*/
int spm_mtcmos_ctrl_md1(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MD1;
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
			!= MD1_PROT_STEP1_0_ACK_MASK) {
			/*  */
			/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, MD1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& MD1_PROT_STEP2_0_ACK_MASK)
			!= MD1_PROT_STEP2_0_ACK_MASK) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET, MD1_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1)
			& MD1_PROT_STEP2_1_ACK_MASK)
			!= MD1_PROT_STEP2_1_ACK_MASK) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) & ~PWR_ON);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MD1_PWR_STA_MASK = 0" */
		while (spm_read(PWR_STATUS) & MD1_PWR_STA_MASK) {
				/*  */
				/*  */
			ram_console_update();
		}
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
		while ((spm_read(PWR_STATUS)
			& MD1_PWR_STA_MASK)
			!= MD1_PWR_STA_MASK) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
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

	DBG_ID = DBG_ID_CONN;
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
			!= CONN_PROT_STEP1_0_ACK_MASK) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, CONN_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& CONN_PROT_STEP2_0_ACK_MASK)
			!= CONN_PROT_STEP2_0_ACK_MASK) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET,
			CONN_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1)
			& CONN_PROT_STEP2_1_ACK_MASK)
			!= CONN_PROT_STEP2_1_ACK_MASK) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
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
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
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
		while (((spm_read(PWR_STATUS)
			& CONN_PWR_STA_MASK) != CONN_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & CONN_PWR_STA_MASK)
		       != CONN_PWR_STA_MASK)) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) | PWR_RST_B);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, CONN_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR,
			CONN_PROT_STEP2_1_MASK);
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
	clk_writel(MM_CG_CLR0, 0x000003ff);
}

int spm_mtcmos_ctrl_dis(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_DIS;
	DBG_STA = state;
	DBG_STEP = 0;
	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off DIS" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
			DIS_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1)
			& DIS_PROT_STEP1_0_ACK_MASK)
			!= DIS_PROT_STEP1_0_ACK_MASK) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(SMI_COMMON_SMI_CLAMP_SET, DIS_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(SMI_COMMON_SMI_CLAMP)
			& DIS_PROT_STEP2_0_ACK_MASK)
			!= DIS_PROT_STEP2_0_ACK_MASK) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step3 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET, DIS_PROT_STEP3_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1)
			& DIS_PROT_STEP3_0_ACK_MASK)
			!= DIS_PROT_STEP3_0_ACK_MASK) {
				/*  */
				/*  */
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
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - way_en : 0" */
		spm_write(INFRA_TOPAXI_SI0_CTL_CLR, 0x1 << 6);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_SI0_STA)
			& (0x1 << 24)) != (0x1 << 24)) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - way_en : 0" */
		//INFRA_TOPAXI_SI2_CTL = *INFRA_TOPAXI_SI2_CTL & (~(0x1 << 5));
		spm_write(INFRA_TOPAXI_SI2_CTL,
			spm_read(INFRA_TOPAXI_SI2_CTL) & (~(0x1 << 5)));
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_SI2_STA)
			& (0x1 << 14)) != (0x1 << 14)) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step5 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, DIS_PROT_STEP5_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& DIS_PROT_STEP5_0_ACK_MASK)
			!= DIS_PROT_STEP5_0_ACK_MASK) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) | DIS_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DIS_SRAM_PDN_ACK = 1" */
		while ((spm_read(DIS_PWR_CON)
			& DIS_SRAM_PDN_ACK) != DIS_SRAM_PDN_ACK) {
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
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
				/*  */
				/*  */
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
		while (((spm_read(PWR_STATUS)
			& DIS_PWR_STA_MASK) != DIS_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & DIS_PWR_STA_MASK)
		       != DIS_PWR_STA_MASK)) {
				/*  */
				/*  */
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
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DIS_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(DIS_PWR_CON) & DIS_SRAM_PDN_ACK_BIT0) {
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step5 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, DIS_PROT_STEP5_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Release bus protect - way_en : 0" */
		//INFRA_TOPAXI_SI2_CTL = *INFRA_TOPAXI_SI2_CTL | (0x1 << 5);
		spm_write(INFRA_TOPAXI_SI2_CTL,
			spm_read(INFRA_TOPAXI_SI2_CTL) | (0x1 << 5));
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_SI2_STA)
			& (0x1 << 14)) != (0x1 << 14)) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - way_en : 0" */
		spm_write(INFRA_TOPAXI_SI0_CTL_SET, 0x1 << 6);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_SI0_STA)
			& (0x1 << 24)) != (0x1 << 24)) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step4 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, DIS_PROT_STEP4_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Release bus protect - step3 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR, DIS_PROT_STEP3_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(SMI_COMMON_SMI_CLAMP_CLR, DIS_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
			DIS_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Finish to turn on DIS" */
		enable_mm_clk();
	}
	return err;
}

int spm_mtcmos_ctrl_mfg0(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG0;
	DBG_STA = state;
	DBG_STEP = 0;
	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG0" */
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MFG0_PWR_CON, spm_read(MFG0_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MFG0_PWR_CON, spm_read(MFG0_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MFG0_PWR_CON, spm_read(MFG0_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MFG0_PWR_CON, spm_read(MFG0_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MFG0_PWR_CON, spm_read(MFG0_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MFG0_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MFG0_PWR_STA_MASK)) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off MFG0" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MFG0" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MFG0_PWR_CON, spm_read(MFG0_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MFG0_PWR_CON, spm_read(MFG0_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & MFG0_PWR_STA_MASK)
			!= MFG0_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & MFG0_PWR_STA_MASK)
			!= MFG0_PWR_STA_MASK)) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG0_PWR_CON, spm_read(MFG0_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG0_PWR_CON, spm_read(MFG0_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG0_PWR_CON, spm_read(MFG0_PWR_CON) | PWR_RST_B);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Finish to turn on MFG0" */
	}
	return err;
}

void enable_isp_clk(void)
{
	clk_writel(IMG_CG_CLR, 0x00000003);
}

int spm_mtcmos_ctrl_isp(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_ISP;
	DBG_STA = state;
	DBG_STEP = 0;
	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off ISP" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
			ISP_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1)
			& ISP_PROT_STEP1_0_ACK_MASK)
			!= ISP_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
			ISP_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
//For VPU_CONN power off bypass ack check
		if (spm_read(PWR_STATUS) & VPU_CONN_PWR_STA_MASK) {
			while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1)
			& ISP_PROT_STEP2_0_ACK_MASK) !=
			ISP_PROT_STEP2_0_ACK_MASK) {
				ram_console_update();
			}
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(SMI_COMMON_SMI_CLAMP_SET, ISP_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(SMI_COMMON_SMI_CLAMP)
			& ISP_PROT_STEP2_1_ACK_MASK)
			!= ISP_PROT_STEP2_1_ACK_MASK) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) | ISP_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ISP_SRAM_PDN_ACK = 1" */
		while ((spm_read(ISP_PWR_CON) & ISP_SRAM_PDN_ACK)
			!= ISP_SRAM_PDN_ACK) {
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
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
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
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
				/*  */
				/*  */
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
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ISP_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(ISP_PWR_CON) & ISP_SRAM_PDN_ACK_BIT0) {
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
			ISP_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(SMI_COMMON_SMI_CLAMP_CLR, ISP_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
			ISP_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Finish to turn on ISP" */
		enable_isp_clk();
	}
	return err;
}

int spm_mtcmos_ctrl_mfg1(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG1;
	DBG_STA = state;
	DBG_STEP = 0;
	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG1" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET,
			MFG1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1)
			& MFG1_PROT_STEP1_0_ACK_MASK)
			!= MFG1_PROT_STEP1_0_ACK_MASK) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, MFG1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& MFG1_PROT_STEP2_0_ACK_MASK)
			!= MFG1_PROT_STEP2_0_ACK_MASK) {
				/*  */
				/*  */
			ram_console_update();
		}
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
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MFG1_PWR_CON, spm_read(MFG1_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MFG1_PWR_CON, spm_read(MFG1_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="SIM_LOG_IGNORE: sram_ps_sva_pd_a4"*/
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MFG1_PWR_CON, spm_read(MFG1_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MFG1_PWR_CON, spm_read(MFG1_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MFG1_PWR_CON, spm_read(MFG1_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MFG1_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MFG1_PWR_STA_MASK)) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="SIM_LOG_RESUME: sram_ps_sva_pd_a4"*/
		/* TINFO="Finish to turn off MFG1" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MFG1" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MFG1_PWR_CON, spm_read(MFG1_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MFG1_PWR_CON, spm_read(MFG1_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & MFG1_PWR_STA_MASK)
			!= MFG1_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & MFG1_PWR_STA_MASK)
			!= MFG1_PWR_STA_MASK)) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG1_PWR_CON, spm_read(MFG1_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG1_PWR_CON, spm_read(MFG1_PWR_CON) & ~PWR_ISO);
		/* TINFO="SIM_LOG_IGNORE: sram_ps_sva_pd_a4"*/
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG1_PWR_CON, spm_read(MFG1_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MFG1_PWR_CON, spm_read(MFG1_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG1_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(MFG1_PWR_CON) & MFG1_SRAM_PDN_ACK_BIT0) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		spm_write(MFG1_PWR_CON, spm_read(MFG1_PWR_CON) & ~(0x1 << 9));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG1_SRAM_PDN_ACK_BIT1 = 0" */
		while (spm_read(MFG1_PWR_CON) & MFG1_SRAM_PDN_ACK_BIT1) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, MFG1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR,
			MFG1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="SIM_LOG_RESUME: sram_ps_sva_pd_a4"*/
		/* TINFO="Finish to turn on MFG1" */
	}
	return err;
}

int spm_mtcmos_ctrl_mfg5(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG5;
	DBG_STA = state;
	DBG_STEP = 0;
	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG5" */
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MFG5_PWR_CON,
			spm_read(MFG5_PWR_CON) | MFG5_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG5_SRAM_PDN_ACK = 1" */
		while ((spm_read(MFG5_PWR_CON) & MFG5_SRAM_PDN_ACK)
			!= MFG5_SRAM_PDN_ACK) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MFG5_PWR_CON, spm_read(MFG5_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MFG5_PWR_CON, spm_read(MFG5_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MFG5_PWR_CON, spm_read(MFG5_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MFG5_PWR_CON, spm_read(MFG5_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MFG5_PWR_CON, spm_read(MFG5_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MFG5_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MFG5_PWR_STA_MASK)) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off MFG5" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MFG5" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MFG5_PWR_CON, spm_read(MFG5_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MFG5_PWR_CON, spm_read(MFG5_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & MFG5_PWR_STA_MASK)
			!= MFG5_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & MFG5_PWR_STA_MASK)
			!= MFG5_PWR_STA_MASK)) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG5_PWR_CON, spm_read(MFG5_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG5_PWR_CON, spm_read(MFG5_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG5_PWR_CON, spm_read(MFG5_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MFG5_PWR_CON, spm_read(MFG5_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG5_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(MFG5_PWR_CON) & MFG5_SRAM_PDN_ACK_BIT0) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn on MFG5" */
	}
	return err;
}

int spm_mtcmos_ctrl_mfg2(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG2;
	DBG_STA = state;
	DBG_STEP = 0;
	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG2" */
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MFG2_PWR_CON,
			spm_read(MFG2_PWR_CON) | MFG2_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG2_SRAM_PDN_ACK = 1" */
		while ((spm_read(MFG2_PWR_CON) & MFG2_SRAM_PDN_ACK)
			!= MFG2_SRAM_PDN_ACK) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MFG2_PWR_CON, spm_read(MFG2_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MFG2_PWR_CON, spm_read(MFG2_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="SIM_LOG_IGNORE: sram_ps_sva_pd_a4"*/
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MFG2_PWR_CON, spm_read(MFG2_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MFG2_PWR_CON, spm_read(MFG2_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MFG2_PWR_CON, spm_read(MFG2_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MFG2_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MFG2_PWR_STA_MASK)) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="SIM_LOG_RESUME: sram_ps_sva_pd_a4"*/
		/* TINFO="Finish to turn off MFG2" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MFG2" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MFG2_PWR_CON, spm_read(MFG2_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MFG2_PWR_CON, spm_read(MFG2_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & MFG2_PWR_STA_MASK)
			!= MFG2_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & MFG2_PWR_STA_MASK)
			!= MFG2_PWR_STA_MASK)) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG2_PWR_CON, spm_read(MFG2_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG2_PWR_CON, spm_read(MFG2_PWR_CON) & ~PWR_ISO);
		/* TINFO="SIM_LOG_IGNORE: sram_ps_sva_pd_a4"*/
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG2_PWR_CON, spm_read(MFG2_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MFG2_PWR_CON, spm_read(MFG2_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG2_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(MFG2_PWR_CON) & MFG2_SRAM_PDN_ACK_BIT0) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="SIM_LOG_RESUME: sram_ps_sva_pd_a4"*/
		/* TINFO="Finish to turn on MFG2" */
	}
	return err;
}

void enable_ven_clk(void)
{
	clk_writel(VENC_CG_SET, 0x10000111);
}

int spm_mtcmos_ctrl_ven(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_VEN;
	DBG_STA = state;
	DBG_STEP = 0;
	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VEN" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
			VEN_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1)
			& VEN_PROT_STEP1_0_ACK_MASK)
			!= VEN_PROT_STEP1_0_ACK_MASK) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(SMI_COMMON_SMI_CLAMP_SET, VEN_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(SMI_COMMON_SMI_CLAMP)
			& VEN_PROT_STEP2_0_ACK_MASK)
			!= VEN_PROT_STEP2_0_ACK_MASK) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) | VEN_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VEN_SRAM_PDN_ACK = 1" */
		while ((spm_read(VEN_PWR_CON) & VEN_SRAM_PDN_ACK)
			!= VEN_SRAM_PDN_ACK) {
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
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
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
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
			!= VEN_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & VEN_PWR_STA_MASK)
			!= VEN_PWR_STA_MASK)) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
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
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) & ~(0x1 << 9));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VEN_SRAM_PDN_ACK_BIT1 = 0" */
		while (spm_read(VEN_PWR_CON) & VEN_SRAM_PDN_ACK_BIT1) {
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) & ~(0x1 << 10));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VEN_SRAM_PDN_ACK_BIT2 = 0" */
		while (spm_read(VEN_PWR_CON) & VEN_SRAM_PDN_ACK_BIT2) {
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) & ~(0x1 << 11));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VEN_SRAM_PDN_ACK_BIT3 = 0" */
		while (spm_read(VEN_PWR_CON) & VEN_SRAM_PDN_ACK_BIT3) {
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(SMI_COMMON_SMI_CLAMP_CLR, VEN_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
			VEN_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Finish to turn on VEN" */
		enable_ven_clk();
	}
	return err;
}

int spm_mtcmos_ctrl_mfg3(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG3;
	DBG_STA = state;
	DBG_STEP = 0;
	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG3" */
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MFG3_PWR_CON,
			spm_read(MFG3_PWR_CON) | MFG3_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG3_SRAM_PDN_ACK = 1" */
		while ((spm_read(MFG3_PWR_CON) & MFG3_SRAM_PDN_ACK)
			!= MFG3_SRAM_PDN_ACK) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MFG3_PWR_CON, spm_read(MFG3_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MFG3_PWR_CON, spm_read(MFG3_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="SIM_LOG_IGNORE: sram_ps_sva_pd_a4"*/
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MFG3_PWR_CON, spm_read(MFG3_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MFG3_PWR_CON, spm_read(MFG3_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MFG3_PWR_CON, spm_read(MFG3_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MFG3_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MFG3_PWR_STA_MASK)) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="SIM_LOG_RESUME: sram_ps_sva_pd_a4"*/
		/* TINFO="Finish to turn off MFG3" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MFG3" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MFG3_PWR_CON, spm_read(MFG3_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MFG3_PWR_CON, spm_read(MFG3_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & MFG3_PWR_STA_MASK)
			!= MFG3_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & MFG3_PWR_STA_MASK)
			!= MFG3_PWR_STA_MASK)) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG3_PWR_CON, spm_read(MFG3_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG3_PWR_CON, spm_read(MFG3_PWR_CON) & ~PWR_ISO);
		/* TINFO="SIM_LOG_IGNORE: sram_ps_sva_pd_a4"*/
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG3_PWR_CON, spm_read(MFG3_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MFG3_PWR_CON, spm_read(MFG3_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG3_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(MFG3_PWR_CON) & MFG3_SRAM_PDN_ACK_BIT0) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="SIM_LOG_RESUME: sram_ps_sva_pd_a4"*/
		/* TINFO="Finish to turn on MFG3" */
	}
	return err;
}

int spm_mtcmos_ctrl_mfg4(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG4;
	DBG_STA = state;
	DBG_STEP = 0;
	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG4" */
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MFG4_PWR_CON,
			spm_read(MFG4_PWR_CON) | MFG4_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG4_SRAM_PDN_ACK = 1" */
		while ((spm_read(MFG4_PWR_CON) & MFG4_SRAM_PDN_ACK)
			!= MFG4_SRAM_PDN_ACK) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MFG4_PWR_CON, spm_read(MFG4_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MFG4_PWR_CON, spm_read(MFG4_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MFG4_PWR_CON, spm_read(MFG4_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MFG4_PWR_CON, spm_read(MFG4_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MFG4_PWR_CON, spm_read(MFG4_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MFG4_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MFG4_PWR_STA_MASK)) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off MFG4" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MFG4" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MFG4_PWR_CON, spm_read(MFG4_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MFG4_PWR_CON, spm_read(MFG4_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & MFG4_PWR_STA_MASK)
			!= MFG4_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & MFG4_PWR_STA_MASK)
			!= MFG4_PWR_STA_MASK)) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG4_PWR_CON, spm_read(MFG4_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG4_PWR_CON, spm_read(MFG4_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG4_PWR_CON, spm_read(MFG4_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MFG4_PWR_CON, spm_read(MFG4_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG4_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(MFG4_PWR_CON) & MFG4_SRAM_PDN_ACK_BIT0) {
				/* n/a */
				/* n/a */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn on MFG4" */
	}
	return err;
}

int spm_mtcmos_ctrl_audio(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_AUDIO;
	DBG_STA = state;
	DBG_STEP = 0;
	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off AUDIO" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, AUDIO_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& AUDIO_PROT_STEP1_0_ACK_MASK)
			!= AUDIO_PROT_STEP1_0_ACK_MASK) {
				/*  */
				/*  */
			ram_console_update();
		}
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
				/* Need f_f26M_aud_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(AUDIO_PWR_CON,
			spm_read(AUDIO_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(AUDIO_PWR_CON,
			spm_read(AUDIO_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & AUDIO_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & AUDIO_PWR_STA_MASK)) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
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
			!= AUDIO_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & AUDIO_PWR_STA_MASK)
			!= AUDIO_PWR_STA_MASK)) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(AUDIO_PWR_CON,
			spm_read(AUDIO_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(AUDIO_PWR_CON,
			spm_read(AUDIO_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until AUDIO_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(AUDIO_PWR_CON) & AUDIO_SRAM_PDN_ACK_BIT0) {
			/* Need f_f26M_aud_ck for SRAM PDN delay IP. */
			/* Need f_f26M_aud_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, AUDIO_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Finish to turn on AUDIO" */
	}
	return err;
}

void enable_cam_clk(void)
{
	/*clk_writel(CAM_CG_CLR, 0x7fcf);*/
	clk_writel(CAM_CG_CLR, 0x15);
}

int spm_mtcmos_ctrl_cam(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_CAM;
	DBG_STA = state;
	DBG_STEP = 0;
	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off CAM" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
			CAM_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1)
			& CAM_PROT_STEP1_0_ACK_MASK)
			!= CAM_PROT_STEP1_0_ACK_MASK) {
				/*  */
				/*  */
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
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
			CAM_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		if (spm_read(PWR_STATUS) & VPU_CONN_PWR_STA_MASK) {
			while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1)
				& CAM_PROT_STEP2_1_ACK_MASK)
				!= CAM_PROT_STEP2_1_ACK_MASK) {
					/*  */
					/*  */
				ram_console_update();
			}
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 2" */
		spm_write(SMI_COMMON_SMI_CLAMP_SET, CAM_PROT_STEP2_2_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(SMI_COMMON_SMI_CLAMP)
			& CAM_PROT_STEP2_2_ACK_MASK)
			!= CAM_PROT_STEP2_2_ACK_MASK) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(CAM_PWR_CON, spm_read(CAM_PWR_CON) | CAM_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until CAM_SRAM_PDN_ACK = 1" */
		while ((spm_read(CAM_PWR_CON) & CAM_SRAM_PDN_ACK)
			!= CAM_SRAM_PDN_ACK) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
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
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
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
				/*  */
				/*  */
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
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(CAM_PWR_CON, spm_read(CAM_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until CAM_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(CAM_PWR_CON) & CAM_SRAM_PDN_ACK_BIT0) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		spm_write(CAM_PWR_CON, spm_read(CAM_PWR_CON) & ~(0x1 << 9));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until CAM_SRAM_PDN_ACK_BIT1 = 0" */
		while (spm_read(CAM_PWR_CON) & CAM_SRAM_PDN_ACK_BIT1) {
				/*  */
				/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, CAM_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
			CAM_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Release bus protect - step2 : 2" */
		spm_write(SMI_COMMON_SMI_CLAMP_CLR, CAM_PROT_STEP2_2_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
			CAM_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Finish to turn on CAM" */
		enable_cam_clk();
	}
	return err;
}

int spm_mtcmos_ctrl_vpu_vcore_dormant(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_VPU_VCORE_DORMANT;
	DBG_STA = state;
	DBG_STEP = 0;
	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VPU_VCORE" */
		/* TINFO="VPU_VCORE_SRAM_CON[0]=1"*/
		spm_write(VPU_VCORE_SRAM_CON,
			spm_read(VPU_VCORE_SRAM_CON) | (0x1 << 0));
		/* TINFO="VPU_VCORE_SRAM_CON[1]=0"*/
		spm_write(VPU_VCORE_SRAM_CON,
			spm_read(VPU_VCORE_SRAM_CON) & ~(0x1 << 1));
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="VPU_VCORE_SRAM_CON[4]=0"*/
		spm_write(VPU_VCORE_SRAM_CON,
			spm_read(VPU_VCORE_SRAM_CON) & ~(0x1 << 4));
		/* TINFO="VPU_VCORE_SRAM_CON[5]=0"*/
		spm_write(VPU_VCORE_SRAM_CON,
			spm_read(VPU_VCORE_SRAM_CON) & ~(0x1 << 5));
		/* TINFO="VPU_VCORE_SRAM_CON[6]=0"*/
		spm_write(VPU_VCORE_SRAM_CON,
			spm_read(VPU_VCORE_SRAM_CON) & ~(0x1 << 6));
		/* TINFO="VPU_VCORE_SRAM_CON[7]=0"*/
		spm_write(VPU_VCORE_SRAM_CON,
			spm_read(VPU_VCORE_SRAM_CON) & ~(0x1 << 7));
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(VPU_VCORE_PWR_CON,
			spm_read(VPU_VCORE_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(VPU_VCORE_PWR_CON,
			spm_read(VPU_VCORE_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(VPU_VCORE_PWR_CON,
			spm_read(VPU_VCORE_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(VPU_VCORE_PWR_CON,
			spm_read(VPU_VCORE_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(VPU_VCORE_PWR_CON,
			spm_read(VPU_VCORE_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & VPU_VCORE_PWR_STA_MASK)
			|| (spm_read(PWR_STATUS_2ND)
			& VPU_VCORE_PWR_STA_MASK)) {

		}
#endif
		/* TINFO="Finish to turn off VPU_VCORE" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on VPU_VCORE" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(VPU_VCORE_PWR_CON,
			spm_read(VPU_VCORE_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(VPU_VCORE_PWR_CON,
			spm_read(VPU_VCORE_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & VPU_VCORE_PWR_STA_MASK)
			!= VPU_VCORE_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & VPU_VCORE_PWR_STA_MASK)
			!= VPU_VCORE_PWR_STA_MASK)) {

		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(VPU_VCORE_PWR_CON,
			spm_read(VPU_VCORE_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(VPU_VCORE_PWR_CON,
			spm_read(VPU_VCORE_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(VPU_VCORE_PWR_CON,
			spm_read(VPU_VCORE_PWR_CON) | PWR_RST_B);
		/* TINFO="VPU_VCORE_SRAM_CON[4]=1"*/
		spm_write(VPU_VCORE_SRAM_CON,
			spm_read(VPU_VCORE_SRAM_CON) | (0x1 << 4));
		/* TINFO="VPU_VCORE_SRAM_CON[5]=1"*/
		spm_write(VPU_VCORE_SRAM_CON,
			spm_read(VPU_VCORE_SRAM_CON) | (0x1 << 5));
		/* TINFO="VPU_VCORE_SRAM_CON[6]=1"*/
		spm_write(VPU_VCORE_SRAM_CON,
			spm_read(VPU_VCORE_SRAM_CON) | (0x1 << 6));
		/* TINFO="VPU_VCORE_SRAM_CON[7]=1"*/
		spm_write(VPU_VCORE_SRAM_CON,
			spm_read(VPU_VCORE_SRAM_CON) | (0x1 << 7));
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="VPU_VCORE_SRAM_CON[1]=1"*/
		spm_write(VPU_VCORE_SRAM_CON,
			spm_read(VPU_VCORE_SRAM_CON) | (0x1 << 1));
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="VPU_VCORE_SRAM_CON[0]=0"*/
		spm_write(VPU_VCORE_SRAM_CON,
			spm_read(VPU_VCORE_SRAM_CON) & ~(0x1 << 0));
		/* TINFO="Finish to turn on VPU_VCORE" */
	}
	return err;
}

int spm_mtcmos_ctrl_vpu_vcore_shut_down(int state)
{
	int err = 0;

	/* TINFO="enable SPM register control" */

	DBG_ID = DBG_ID_VPU_VCORE_SHUTDOWN;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VPU_VCORE" */
		/* TINFO="VPU_VCORE_SRAM_CON[16]=1"*/
		spm_write(VPU_VCORE_SRAM_CON,
			spm_read(VPU_VCORE_SRAM_CON) | (0x1 << 16));
		/* TINFO="VPU_VCORE_SRAM_CON[17]=1"*/
		spm_write(VPU_VCORE_SRAM_CON,
			spm_read(VPU_VCORE_SRAM_CON) | (0x1 << 17));
		/* TINFO="VPU_VCORE_SRAM_CON[18]=1"*/
		spm_write(VPU_VCORE_SRAM_CON,
			spm_read(VPU_VCORE_SRAM_CON) | (0x1 << 18));
		/* TINFO="VPU_VCORE_SRAM_CON[19]=1"*/
		spm_write(VPU_VCORE_SRAM_CON,
			spm_read(VPU_VCORE_SRAM_CON) | (0x1 << 19));
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(VPU_VCORE_PWR_CON,
			spm_read(VPU_VCORE_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(VPU_VCORE_PWR_CON,
			spm_read(VPU_VCORE_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(VPU_VCORE_PWR_CON,
			spm_read(VPU_VCORE_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(VPU_VCORE_PWR_CON,
			spm_read(VPU_VCORE_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(VPU_VCORE_PWR_CON,
			spm_read(VPU_VCORE_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & VPU_VCORE_PWR_STA_MASK)
			|| (spm_read(PWR_STATUS_2ND)
			& VPU_VCORE_PWR_STA_MASK)) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off VPU_VCORE" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on VPU_VCORE" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(VPU_VCORE_PWR_CON,
			spm_read(VPU_VCORE_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(VPU_VCORE_PWR_CON,
			spm_read(VPU_VCORE_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & VPU_VCORE_PWR_STA_MASK)
			!= VPU_VCORE_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & VPU_VCORE_PWR_STA_MASK)
			!= VPU_VCORE_PWR_STA_MASK)) {

			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(VPU_VCORE_PWR_CON,
			spm_read(VPU_VCORE_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(VPU_VCORE_PWR_CON,
			spm_read(VPU_VCORE_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(VPU_VCORE_PWR_CON,
			spm_read(VPU_VCORE_PWR_CON) | PWR_RST_B);
		/* TINFO="VPU_VCORE_SRAM_CON[16]=0"*/
		spm_write(VPU_VCORE_SRAM_CON,
			spm_read(VPU_VCORE_SRAM_CON) & ~(0x1 << 16));
		/* TINFO="VPU_VCORE_SRAM_CON[17]=0"*/
		spm_write(VPU_VCORE_SRAM_CON,
			spm_read(VPU_VCORE_SRAM_CON) & ~(0x1 << 17));
		/* TINFO="VPU_VCORE_SRAM_CON[18]=0"*/
		spm_write(VPU_VCORE_SRAM_CON,
			spm_read(VPU_VCORE_SRAM_CON) & ~(0x1 << 18));
		/* TINFO="VPU_VCORE_SRAM_CON[19]=0"*/
		spm_write(VPU_VCORE_SRAM_CON,
			spm_read(VPU_VCORE_SRAM_CON) & ~(0x1 << 19));
		/* TINFO="Finish to turn on VPU_VCORE" */
	}
	return err;
}

int spm_mtcmos_ctrl_vpu_conn_dormant(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_VPU_CONN_DORMANT;
	DBG_STA = state;
	DBG_STEP = 0;
	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VPU_CONN" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
			VPU_CONN_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1)
			& VPU_CONN_PROT_STEP1_0_ACK_MASK)
			!= VPU_CONN_PROT_STEP1_0_ACK_MASK) {
		}
#endif
		/* TINFO="Set bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_SET,
			VPU_CONN_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MCU_STA1)
			& VPU_CONN_PROT_STEP1_1_ACK_MASK)
			!= VPU_CONN_PROT_STEP1_1_ACK_MASK) {
		}
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_SET,
			VPU_CONN_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2)
			& VPU_CONN_PROT_STEP2_0_ACK_MASK)
			!= VPU_CONN_PROT_STEP2_0_ACK_MASK) {
		}
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
			VPU_CONN_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1)
			& VPU_CONN_PROT_STEP2_1_ACK_MASK)
			!= VPU_CONN_PROT_STEP2_1_ACK_MASK) {
		}
#endif
		/* TINFO="VPU_CONN_SRAM_CON[0]=1"*/
		spm_write(VPU_CONN_SRAM_CON,
			spm_read(VPU_CONN_SRAM_CON) | (0x1 << 0));
		/* TINFO="VPU_CONN_SRAM_CON[1]=0"*/
		spm_write(VPU_CONN_SRAM_CON,
			spm_read(VPU_CONN_SRAM_CON) & ~(0x1 << 1));
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="VPU_CONN_SRAM_CON[4]=0"*/
		spm_write(VPU_CONN_SRAM_CON,
			spm_read(VPU_CONN_SRAM_CON) & ~(0x1 << 4));
		/* TINFO="VPU_CONN_SRAM_CON[5]=0"*/
		spm_write(VPU_CONN_SRAM_CON,
			spm_read(VPU_CONN_SRAM_CON) & ~(0x1 << 5));
		/* TINFO="VPU_CONN_SRAM_CON[6]=0"*/
		spm_write(VPU_CONN_SRAM_CON,
			spm_read(VPU_CONN_SRAM_CON) & ~(0x1 << 6));
		/* TINFO="VPU_CONN_SRAM_CON[7]=0"*/
		spm_write(VPU_CONN_SRAM_CON,
			spm_read(VPU_CONN_SRAM_CON) & ~(0x1 << 7));
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(VPU_CONN_PWR_CON,
			spm_read(VPU_CONN_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(VPU_CONN_PWR_CON,
			spm_read(VPU_CONN_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(VPU_CONN_PWR_CON,
			spm_read(VPU_CONN_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(VPU_CONN_PWR_CON,
			spm_read(VPU_CONN_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(VPU_CONN_PWR_CON,
			spm_read(VPU_CONN_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & VPU_CONN_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & VPU_CONN_PWR_STA_MASK)) {

		}
#endif
		/* TINFO="Finish to turn off VPU_CONN" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on VPU_CONN" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(VPU_CONN_PWR_CON,
			spm_read(VPU_CONN_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(VPU_CONN_PWR_CON,
			spm_read(VPU_CONN_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & VPU_CONN_PWR_STA_MASK)
			!= VPU_CONN_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & VPU_CONN_PWR_STA_MASK)
			!= VPU_CONN_PWR_STA_MASK)) {

		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(VPU_CONN_PWR_CON,
			spm_read(VPU_CONN_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(VPU_CONN_PWR_CON,
			spm_read(VPU_CONN_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(VPU_CONN_PWR_CON,
			spm_read(VPU_CONN_PWR_CON) | PWR_RST_B);
		/* TINFO="VPU_CONN_SRAM_CON[4]=1"*/
		spm_write(VPU_CONN_SRAM_CON,
			spm_read(VPU_CONN_SRAM_CON) | (0x1 << 4));
		/* TINFO="VPU_CONN_SRAM_CON[5]=1"*/
		spm_write(VPU_CONN_SRAM_CON,
			spm_read(VPU_CONN_SRAM_CON) | (0x1 << 5));
		/* TINFO="VPU_CONN_SRAM_CON[6]=1"*/
		spm_write(VPU_CONN_SRAM_CON,
			spm_read(VPU_CONN_SRAM_CON) | (0x1 << 6));
		/* TINFO="VPU_CONN_SRAM_CON[7]=1"*/
		spm_write(VPU_CONN_SRAM_CON,
			spm_read(VPU_CONN_SRAM_CON) | (0x1 << 7));
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="VPU_CONN_SRAM_CON[1]=1"*/
		spm_write(VPU_CONN_SRAM_CON,
			spm_read(VPU_CONN_SRAM_CON) | (0x1 << 1));
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="VPU_CONN_SRAM_CON[0]=0"*/
		spm_write(VPU_CONN_SRAM_CON,
			spm_read(VPU_CONN_SRAM_CON) & ~(0x1 << 0));
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_CLR,
			VPU_CONN_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
			VPU_CONN_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
			VPU_CONN_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Release bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_CLR,
			VPU_CONN_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Finish to turn on VPU_CONN" */
	}
	return err;
}

void enable_apu_vcore_clk(void)
{
	clk_writel(APU_VCORE_CG_CLR, 0x0000000F);
}

void enable_apu_conn_clk(void)
{
	clk_writel(APU_CONN_CG_CLR, 0x0000FF);
}

int spm_mtcmos_ctrl_vpu_conn_shut_down(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_VPU_CONN_SHUTDOWN;
	DBG_STA = state;
	DBG_STEP = 0;
	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VPU_CONN" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
			VPU_CONN_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1)
			& VPU_CONN_PROT_STEP1_0_ACK_MASK)
			!= VPU_CONN_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_SET,
			VPU_CONN_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MCU_STA1)
			& VPU_CONN_PROT_STEP1_1_ACK_MASK)
			!= VPU_CONN_PROT_STEP1_1_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_SET,
			VPU_CONN_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2)
			& VPU_CONN_PROT_STEP2_0_ACK_MASK)
			!= VPU_CONN_PROT_STEP2_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
			VPU_CONN_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1)
			& VPU_CONN_PROT_STEP2_1_ACK_MASK)
			!= VPU_CONN_PROT_STEP2_1_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="VPU_CONN_SRAM_CON[16]=1"*/
		spm_write(VPU_CONN_SRAM_CON,
			spm_read(VPU_CONN_SRAM_CON) | (0x1 << 16));
		/* TINFO="VPU_CONN_SRAM_CON[17]=1"*/
		spm_write(VPU_CONN_SRAM_CON,
			spm_read(VPU_CONN_SRAM_CON) | (0x1 << 17));
		/* TINFO="VPU_CONN_SRAM_CON[18]=1"*/
		spm_write(VPU_CONN_SRAM_CON,
			spm_read(VPU_CONN_SRAM_CON) | (0x1 << 18));
		/* TINFO="VPU_CONN_SRAM_CON[19]=1"*/
		spm_write(VPU_CONN_SRAM_CON,
			spm_read(VPU_CONN_SRAM_CON) | (0x1 << 19));
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(VPU_CONN_PWR_CON,
			spm_read(VPU_CONN_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(VPU_CONN_PWR_CON,
			spm_read(VPU_CONN_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(VPU_CONN_PWR_CON,
			spm_read(VPU_CONN_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(VPU_CONN_PWR_CON,
			spm_read(VPU_CONN_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(VPU_CONN_PWR_CON,
			spm_read(VPU_CONN_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & VPU_CONN_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & VPU_CONN_PWR_STA_MASK)) {

			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="EXT_BUCK_ISO[2]=1"*/
		spm_write(EXT_BUCK_ISO, spm_read(EXT_BUCK_ISO) | (0x1 << 2));
		/* TINFO="Finish to turn off VPU_CONN" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on VPU_CONN" */
		/* TINFO="EXT_BUCK_ISO[2]=0"*/
		spm_write(EXT_BUCK_ISO, spm_read(EXT_BUCK_ISO) & ~(0x1 << 2));
		/* TINFO="Set PWR_ON = 1" */
		spm_write(VPU_CONN_PWR_CON,
			spm_read(VPU_CONN_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(VPU_CONN_PWR_CON,
			spm_read(VPU_CONN_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & VPU_CONN_PWR_STA_MASK)
			!= VPU_CONN_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & VPU_CONN_PWR_STA_MASK)
			!= VPU_CONN_PWR_STA_MASK)) {

			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(VPU_CONN_PWR_CON,
			spm_read(VPU_CONN_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(VPU_CONN_PWR_CON,
			spm_read(VPU_CONN_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(VPU_CONN_PWR_CON,
			spm_read(VPU_CONN_PWR_CON) | PWR_RST_B);
		/* TINFO="VPU_CONN_SRAM_CON[16]=0"*/
		spm_write(VPU_CONN_SRAM_CON,
			spm_read(VPU_CONN_SRAM_CON) & ~(0x1 << 16));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CONN_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(VPU_CONN_SRAM_CON)
			& VPU_CONN_SRAM_PDN_ACK_BIT0) {
		/* Need f_fsmi_ck for SRAM PDN delay IP */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="VPU_CONN_SRAM_CON[17]=0"*/
		spm_write(VPU_CONN_SRAM_CON,
			spm_read(VPU_CONN_SRAM_CON) & ~(0x1 << 17));
		/* TINFO="VPU_CONN_SRAM_CON[18]=0"*/
		spm_write(VPU_CONN_SRAM_CON,
			spm_read(VPU_CONN_SRAM_CON) & ~(0x1 << 18));
		/* TINFO="VPU_CONN_SRAM_CON[19]=0"*/
		spm_write(VPU_CONN_SRAM_CON,
			spm_read(VPU_CONN_SRAM_CON) & ~(0x1 << 19));
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_CLR,
			VPU_CONN_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
			VPU_CONN_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
			VPU_CONN_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Release bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_CLR,
			VPU_CONN_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Finish to turn on VPU_CONN" */
		//enable_apu_vcore_clk();
		enable_apu_conn_clk();
	}
	return err;
}

int spm_mtcmos_ctrl_vpu_core0_dormant(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_VPU_CORE0_DORMANT;
	DBG_STA = state;
	DBG_STEP = 0;
	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VPU_CORE0" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_SET,
			VPU_CORE0_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MCU_STA1)
			& VPU_CORE0_PROT_STEP1_0_ACK_MASK)
			!= VPU_CORE0_PROT_STEP1_0_ACK_MASK) {
		}
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_SET,
			VPU_CORE0_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MCU_STA1)
			& VPU_CORE0_PROT_STEP2_0_ACK_MASK)
			!= VPU_CORE0_PROT_STEP2_0_ACK_MASK) {
		}
#endif
		/* TINFO="VPU_CORE0_SRAM_CON[16]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 16));
		/* TINFO="VPU_CORE0_SRAM_CON[17]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 17));
		/* TINFO="VPU_CORE0_SRAM_CON[0]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 0));
		/* TINFO="VPU_CORE0_SRAM_CON[1]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 1));
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="VPU_CORE0_SRAM_CON[4]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 4));
		/* TINFO="VPU_CORE0_SRAM_CON[5]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 5));
		/* TINFO="VPU_CORE0_SRAM_CON[6]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 6));
		/* TINFO="VPU_CORE0_SRAM_CON[7]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 7));
		/* TINFO="VPU_CORE0_SRAM_CON[8]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 8));
		/* TINFO="VPU_CORE0_SRAM_CON[9]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 9));
		/* TINFO="VPU_CORE0_SRAM_CON[10]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 10));
		/* TINFO="VPU_CORE0_SRAM_CON[11]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 11));
		/* TINFO="VPU_CORE0_SRAM_CON[12]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 12));
		/* TINFO="VPU_CORE0_SRAM_CON[13]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 13));
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
			!= VPU_CORE0_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & VPU_CORE0_PWR_STA_MASK)
			!= VPU_CORE0_PWR_STA_MASK)) {

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
		/* TINFO="VPU_CORE0_SRAM_CON[4]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 4));
		/* TINFO="VPU_CORE0_SRAM_CON[5]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 5));
		/* TINFO="VPU_CORE0_SRAM_CON[6]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 6));
		/* TINFO="VPU_CORE0_SRAM_CON[7]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 7));
		/* TINFO="VPU_CORE0_SRAM_CON[8]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 8));
		/* TINFO="VPU_CORE0_SRAM_CON[9]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 9));
		/* TINFO="VPU_CORE0_SRAM_CON[10]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 10));
		/* TINFO="VPU_CORE0_SRAM_CON[11]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 11));
		/* TINFO="VPU_CORE0_SRAM_CON[12]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 12));
		/* TINFO="VPU_CORE0_SRAM_CON[13]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 13));
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="VPU_CORE0_SRAM_CON[1]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 1));
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="VPU_CORE0_SRAM_CON[0]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 0));
		/* TINFO="VPU_CORE0_SRAM_CON[16]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 16));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE0_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(VPU_CORE0_SRAM_STA)
			& VPU_CORE0_SRAM_PDN_ACK_BIT0) {
		/* Need f_fsmi_ck for SRAM PDN delay IP */
		}
#endif
		/* TINFO="VPU_CORE0_SRAM_CON[17]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 17));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE0_SRAM_PDN_ACK_BIT1 = 0" */
		while (spm_read(VPU_CORE0_SRAM_STA)
			& VPU_CORE0_SRAM_PDN_ACK_BIT1) {
		/* Need f_fsmi_ck for SRAM PDN delay IP */
		}
#endif
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

	DBG_ID = DBG_ID_VPU_CORE0_SHUTDOWN;
	DBG_STA = state;
	DBG_STEP = 0;
	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VPU_CORE0" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_SET,
			VPU_CORE0_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MCU_STA1)
			& VPU_CORE0_PROT_STEP1_0_ACK_MASK)
			!= VPU_CORE0_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_SET,
			VPU_CORE0_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MCU_STA1)
			& VPU_CORE0_PROT_STEP2_0_ACK_MASK)
			!= VPU_CORE0_PROT_STEP2_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="VPU_CORE0_SRAM_CON[16]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 16));
		/* TINFO="VPU_CORE0_SRAM_CON[17]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 17));
		/* TINFO="VPU_CORE0_SRAM_CON[18]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 18));
		/* TINFO="VPU_CORE0_SRAM_CON[19]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 19));
		/* TINFO="VPU_CORE0_SRAM_CON[20]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 20));
		/* TINFO="VPU_CORE0_SRAM_CON[21]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 21));
		/* TINFO="VPU_CORE0_SRAM_CON[22]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 22));
		/* TINFO="VPU_CORE0_SRAM_CON[23]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 23));
		/* TINFO="VPU_CORE0_SRAM_CON[24]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 24));
		/* TINFO="VPU_CORE0_SRAM_CON[25]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 25));
		/* TINFO="VPU_CORE0_SRAM_CON[26]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 26));
		/* TINFO="VPU_CORE0_SRAM_CON[27]=1"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) | (0x1 << 27));
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

			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="EXT_BUCK_ISO[5]=1"*/
		spm_write(EXT_BUCK_ISO, spm_read(EXT_BUCK_ISO) | (0x1 << 5));
		/* TINFO="Finish to turn off VPU_CORE0" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on VPU_CORE0" */
		/* TINFO="EXT_BUCK_ISO[5]=0"*/
		spm_write(EXT_BUCK_ISO, spm_read(EXT_BUCK_ISO) & ~(0x1 << 5));
		/* TINFO="Set PWR_ON = 1" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(VPU_CORE0_PWR_CON,
			spm_read(VPU_CORE0_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & VPU_CORE0_PWR_STA_MASK)
			!= VPU_CORE0_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & VPU_CORE0_PWR_STA_MASK)
			!= VPU_CORE0_PWR_STA_MASK)) {

			ram_console_update();
		}
		INCREASE_STEPS;
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
		/* TINFO="VPU_CORE0_SRAM_CON[16]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 16));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE0_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(VPU_CORE0_SRAM_STA)
			& VPU_CORE0_SRAM_PDN_ACK_BIT0) {
		/* Need f_fsmi_ck for SRAM PDN delay IP */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="VPU_CORE0_SRAM_CON[17]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 17));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE0_SRAM_PDN_ACK_BIT1 = 0" */
		while (spm_read(VPU_CORE0_SRAM_STA)
			& VPU_CORE0_SRAM_PDN_ACK_BIT1) {
		/* Need f_fsmi_ck for SRAM PDN delay IP */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="VPU_CORE0_SRAM_CON[18]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 18));
		/* TINFO="VPU_CORE0_SRAM_CON[19]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 19));
		/* TINFO="VPU_CORE0_SRAM_CON[20]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 20));
		/* TINFO="VPU_CORE0_SRAM_CON[21]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 21));
		/* TINFO="VPU_CORE0_SRAM_CON[22]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 22));
		/* TINFO="VPU_CORE0_SRAM_CON[23]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 23));
		/* TINFO="VPU_CORE0_SRAM_CON[24]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 24));
		/* TINFO="VPU_CORE0_SRAM_CON[25]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 25));
		/* TINFO="VPU_CORE0_SRAM_CON[26]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 26));
		/* TINFO="VPU_CORE0_SRAM_CON[27]=0"*/
		spm_write(VPU_CORE0_SRAM_CON,
			spm_read(VPU_CORE0_SRAM_CON) & ~(0x1 << 27));
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

int spm_mtcmos_ctrl_vpu_core1_dormant(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_VPU_CORE1_DORMANT;
	DBG_STA = state;
	DBG_STEP = 0;
	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VPU_CORE1" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_SET,
			VPU_CORE1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MCU_STA1)
			& VPU_CORE1_PROT_STEP1_0_ACK_MASK)
			!= VPU_CORE1_PROT_STEP1_0_ACK_MASK) {
		}
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_SET,
			VPU_CORE1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MCU_STA1)
			& VPU_CORE1_PROT_STEP2_0_ACK_MASK)
			!= VPU_CORE1_PROT_STEP2_0_ACK_MASK) {
		}
#endif
		/* TINFO="VPU_CORE1_SRAM_CON[16]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 16));
		/* TINFO="VPU_CORE1_SRAM_CON[17]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 17));
		/* TINFO="VPU_CORE1_SRAM_CON[0]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 0));
		/* TINFO="VPU_CORE1_SRAM_CON[1]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 1));
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="VPU_CORE1_SRAM_CON[4]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 4));
		/* TINFO="VPU_CORE1_SRAM_CON[5]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 5));
		/* TINFO="VPU_CORE1_SRAM_CON[6]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 6));
		/* TINFO="VPU_CORE1_SRAM_CON[7]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 7));
		/* TINFO="VPU_CORE1_SRAM_CON[8]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 8));
		/* TINFO="VPU_CORE1_SRAM_CON[9]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 9));
		/* TINFO="VPU_CORE1_SRAM_CON[10]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 10));
		/* TINFO="VPU_CORE1_SRAM_CON[11]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 11));
		/* TINFO="VPU_CORE1_SRAM_CON[12]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 12));
		/* TINFO="VPU_CORE1_SRAM_CON[13]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 13));
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
			|| (spm_read(PWR_STATUS_2ND) &
			VPU_CORE1_PWR_STA_MASK)) {

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
			!= VPU_CORE1_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & VPU_CORE1_PWR_STA_MASK)
			!= VPU_CORE1_PWR_STA_MASK)) {

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
		/* TINFO="VPU_CORE1_SRAM_CON[4]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 4));
		/* TINFO="VPU_CORE1_SRAM_CON[5]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 5));
		/* TINFO="VPU_CORE1_SRAM_CON[6]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 6));
		/* TINFO="VPU_CORE1_SRAM_CON[7]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 7));
		/* TINFO="VPU_CORE1_SRAM_CON[8]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 8));
		/* TINFO="VPU_CORE1_SRAM_CON[9]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 9));
		/* TINFO="VPU_CORE1_SRAM_CON[10]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 10));
		/* TINFO="VPU_CORE1_SRAM_CON[11]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 11));
		/* TINFO="VPU_CORE1_SRAM_CON[12]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 12));
		/* TINFO="VPU_CORE1_SRAM_CON[13]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 13));
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="VPU_CORE1_SRAM_CON[1]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 1));
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="VPU_CORE1_SRAM_CON[0]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 0));
		/* TINFO="VPU_CORE1_SRAM_CON[16]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 16));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE1_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(VPU_CORE1_SRAM_STA)
			& VPU_CORE1_SRAM_PDN_ACK_BIT0) {
		/* Need f_fsmi_ck for SRAM PDN delay IP */
		}
#endif
		/* TINFO="VPU_CORE1_SRAM_CON[17]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 17));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE1_SRAM_PDN_ACK_BIT1 = 0" */
		while (spm_read(VPU_CORE1_SRAM_STA)
			& VPU_CORE1_SRAM_PDN_ACK_BIT1) {
		/* Need f_fsmi_ck for SRAM PDN delay IP */
		}
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_CLR,
			VPU_CORE1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_CLR,
			VPU_CORE1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Finish to turn on VPU_CORE1" */
	}
	return err;
}

int spm_mtcmos_ctrl_vpu_core1_shut_down(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_VPU_CORE1_SHUTDOWN;
	DBG_STA = state;
	DBG_STEP = 0;
	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VPU_CORE1" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_SET,
			VPU_CORE1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MCU_STA1)
			& VPU_CORE1_PROT_STEP1_0_ACK_MASK)
			!= VPU_CORE1_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_SET,
			VPU_CORE1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MCU_STA1)
			& VPU_CORE1_PROT_STEP2_0_ACK_MASK)
			!= VPU_CORE1_PROT_STEP2_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="VPU_CORE1_SRAM_CON[16]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 16));
		/* TINFO="VPU_CORE1_SRAM_CON[17]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 17));
		/* TINFO="VPU_CORE1_SRAM_CON[18]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 18));
		/* TINFO="VPU_CORE1_SRAM_CON[19]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 19));
		/* TINFO="VPU_CORE1_SRAM_CON[20]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 20));
		/* TINFO="VPU_CORE1_SRAM_CON[21]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 21));
		/* TINFO="VPU_CORE1_SRAM_CON[22]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 22));
		/* TINFO="VPU_CORE1_SRAM_CON[23]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 23));
		/* TINFO="VPU_CORE1_SRAM_CON[24]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 24));
		/* TINFO="VPU_CORE1_SRAM_CON[25]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 25));
		/* TINFO="VPU_CORE1_SRAM_CON[26]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 26));
		/* TINFO="VPU_CORE1_SRAM_CON[27]=1"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) | (0x1 << 27));
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
			|| (spm_read(PWR_STATUS_2ND)
			& VPU_CORE1_PWR_STA_MASK)) {

			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="EXT_BUCK_ISO[6]=1"*/
		spm_write(EXT_BUCK_ISO, spm_read(EXT_BUCK_ISO) | (0x1 << 6));
		/* TINFO="Finish to turn off VPU_CORE1" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on VPU_CORE1" */
		/* TINFO="EXT_BUCK_ISO[6]=0"*/
		spm_write(EXT_BUCK_ISO, spm_read(EXT_BUCK_ISO) & ~(0x1 << 6));
		/* TINFO="Set PWR_ON = 1" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(VPU_CORE1_PWR_CON,
			spm_read(VPU_CORE1_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & VPU_CORE1_PWR_STA_MASK)
			!= VPU_CORE1_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & VPU_CORE1_PWR_STA_MASK)
			!= VPU_CORE1_PWR_STA_MASK)) {

			ram_console_update();
		}
		INCREASE_STEPS;
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
		/* TINFO="VPU_CORE1_SRAM_CON[16]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 16));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE1_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(VPU_CORE1_SRAM_STA)
			& VPU_CORE1_SRAM_PDN_ACK_BIT0) {
		/* Need f_fsmi_ck for SRAM PDN delay IP */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="VPU_CORE1_SRAM_CON[17]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 17));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VPU_CORE1_SRAM_PDN_ACK_BIT1 = 0" */
		while (spm_read(VPU_CORE1_SRAM_STA)
			& VPU_CORE1_SRAM_PDN_ACK_BIT1) {
		/* Need f_fsmi_ck for SRAM PDN delay IP */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="VPU_CORE1_SRAM_CON[18]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 18));
		/* TINFO="VPU_CORE1_SRAM_CON[19]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 19));
		/* TINFO="VPU_CORE1_SRAM_CON[20]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 20));
		/* TINFO="VPU_CORE1_SRAM_CON[21]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 21));
		/* TINFO="VPU_CORE1_SRAM_CON[22]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 22));
		/* TINFO="VPU_CORE1_SRAM_CON[23]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 23));
		/* TINFO="VPU_CORE1_SRAM_CON[24]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 24));
		/* TINFO="VPU_CORE1_SRAM_CON[25]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 25));
		/* TINFO="VPU_CORE1_SRAM_CON[26]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 26));
		/* TINFO="VPU_CORE1_SRAM_CON[27]=0"*/
		spm_write(VPU_CORE1_SRAM_CON,
			spm_read(VPU_CORE1_SRAM_CON) & ~(0x1 << 27));
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_CLR,
			VPU_CORE1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MCU_CLR,
			VPU_CORE1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Finish to turn on VPU_CORE1" */
	}
	return err;
}

int spm_mtcmos_ctrl_vpu_core2_dormant(int state)
{
	int err = 0;

	return err;
}

int spm_mtcmos_ctrl_vpu_core2_shut_down(int state)
{
	int err = 0;

	return err;
}

int spm_mtcmos_ctrl_vde(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_VDE;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */


	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VDE" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
			VDE_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1)
			& VDE_PROT_STEP1_0_ACK_MASK)
			!= VDE_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(SMI_COMMON_SMI_CLAMP_SET,
			VDE_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(SMI_COMMON_SMI_CLAMP)
			& VDE_PROT_STEP2_0_ACK_MASK)
			!= VDE_PROT_STEP2_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) | VDE_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VDE_SRAM_PDN_ACK = 1" */
		while ((spm_read(VDE_PWR_CON) & VDE_SRAM_PDN_ACK)
			!= VDE_SRAM_PDN_ACK) {
				/* Need f_fvdec_ck for SRAM PDN delay IP */
			ram_console_update();
		}
		INCREASE_STEPS;
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

			ram_console_update();
		}
		INCREASE_STEPS;
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
			!= VDE_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & VDE_PWR_STA_MASK)
			!= VDE_PWR_STA_MASK)) {

			ram_console_update();
		}
		INCREASE_STEPS;
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
			/* Need f_fvdec_ck for SRAM PDN delay IP */
			/* Need f_fvdec_ck for SRAM PDN delay IP */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(SMI_COMMON_SMI_CLAMP_CLR, VDE_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
			VDE_PROT_STEP1_0_MASK);
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
	return spm_mtcmos_ctrl_conn(STA_POWER_ON);
}

static int DIS_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_dis(STA_POWER_ON);
}

static int CAM_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_cam(STA_POWER_ON);
}

static int ISP_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_isp(STA_POWER_ON);
}

static int MFG5_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_mfg5(STA_POWER_ON);
}

static int VEN_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_ven(STA_POWER_ON);
}

static int VDE_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vde(STA_POWER_ON);
}

static int AUDIO_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_audio(STA_POWER_ON);
}

static int MFG0_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_mfg0(STA_POWER_ON);
}

static int MFG1_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_mfg1(STA_POWER_ON);
}

static int MFG2_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_mfg2(STA_POWER_ON);
}

static int MFG3_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_mfg3(STA_POWER_ON);
}

static int MFG4_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_mfg4(STA_POWER_ON);
}

static int VPU_VCORE_DORMANT_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vpu_vcore_dormant(STA_POWER_ON);
}

static int VPU_VCORE_SHUTDOWN_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vpu_vcore_shut_down(STA_POWER_ON);
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

static int VPU_CONN_DORMANT_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vpu_conn_dormant(STA_POWER_ON);
}

static int VPU_CONN_SHUTDOWN_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vpu_conn_shut_down(STA_POWER_ON);
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
	return spm_mtcmos_ctrl_conn(STA_POWER_DOWN);
}

static int DIS_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_dis(STA_POWER_DOWN);
}

static int CAM_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_cam(STA_POWER_DOWN);
}

static int ISP_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_isp(STA_POWER_DOWN);
}

static int MFG5_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_mfg5(STA_POWER_DOWN);
}

static int VEN_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_ven(STA_POWER_DOWN);
}

static int VDE_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vde(STA_POWER_DOWN);
}

static int AUDIO_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_audio(STA_POWER_DOWN);
}

static int MFG0_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_mfg0(STA_POWER_DOWN);
}

static int MFG1_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_mfg1(STA_POWER_DOWN);
}

static int MFG2_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_mfg2(STA_POWER_DOWN);
}

static int MFG3_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_mfg3(STA_POWER_DOWN);
}

static int MFG4_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_mfg4(STA_POWER_DOWN);
}

static int VPU_VCORE_DORMANT_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vpu_vcore_dormant(STA_POWER_DOWN);
}

static int VPU_VCORE_SHUTDOWN_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vpu_vcore_shut_down(STA_POWER_DOWN);
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

static int VPU_CONN_DORMANT_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vpu_conn_dormant(STA_POWER_DOWN);
}

static int VPU_CONN_SHUTDOWN_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vpu_conn_shut_down(STA_POWER_DOWN);
}

static int sys_get_state_op(struct subsys *sys)
{
	unsigned int sta = clk_readl(PWR_STATUS);
	unsigned int sta_s = clk_readl(PWR_STATUS_2ND);

	return (sta & sys->sta_mask) && (sta_s & sys->sta_mask);
}

#if 0
static int sys_get_md1_state_op(struct subsys *sys)//check if need
{
	unsigned int sta = clk_readl(MD1_PWR_CON);

	return (sta & sys->sta_mask);
}


static int sys_get_conn_state_op(struct subsys *sys)
{
	unsigned int sta = clk_readl(INFRA_TOPAXI_PROTECTEN);

	if (first_conn)
		return 0;
	else
		return !(sta & sys->sta_mask);
}
#endif
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

static struct subsys_ops CAM_sys_ops = {
	.enable = CAM_sys_enable_op,
	.disable = CAM_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops ISP_sys_ops = {
	.enable = ISP_sys_enable_op,
	.disable = ISP_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops MFG5_sys_ops = {
	.enable = MFG5_sys_enable_op,
	.disable = MFG5_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops VEN_sys_ops = {
	.enable = VEN_sys_enable_op,
	.disable = VEN_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops VDE_sys_ops = {
	.enable = VDE_sys_enable_op,
	.disable = VDE_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops AUDIO_sys_ops = {
	.enable = AUDIO_sys_enable_op,
	.disable = AUDIO_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops MFG0_sys_ops = {
	.enable = MFG0_sys_enable_op,
	.disable = MFG0_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops MFG1_sys_ops = {
	.enable = MFG1_sys_enable_op,
	.disable = MFG1_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops MFG2_sys_ops = {
	.enable = MFG2_sys_enable_op,
	.disable = MFG2_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops MFG3_sys_ops = {
	.enable = MFG3_sys_enable_op,
	.disable = MFG3_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops MFG4_sys_ops = {
	.enable = MFG4_sys_enable_op,
	.disable = MFG4_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops VPU_VCORE_DORMANT_sys_ops = {
	.enable = VPU_VCORE_DORMANT_sys_enable_op,
	.disable = VPU_VCORE_DORMANT_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops VPU_VCORE_SHUTDOWN_sys_ops = {
	.enable = VPU_VCORE_SHUTDOWN_sys_enable_op,
	.disable = VPU_VCORE_SHUTDOWN_sys_disable_op,
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

static struct subsys_ops VPU_CONN_DORMANT_sys_ops = {
	.enable = VPU_CONN_DORMANT_sys_enable_op,
	.disable = VPU_CONN_DORMANT_sys_disable_op,
	.get_state = sys_get_state_op,
};

static struct subsys_ops VPU_CONN_SHUTDOWN_sys_ops = {
	.enable = VPU_CONN_SHUTDOWN_sys_enable_op,
	.disable = VPU_CONN_SHUTDOWN_sys_disable_op,
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
1,	/*SYS_DIS = 2,*/
1,	/*SYS_VEN = 3,*/
1,	/*SYS_VDE = 4,*/
1,	/*SYS_CAM = 5,*/
1,	/*SYS_ISP = 6,*/
1,	/*SYS_AUDIO = 7,*/
1,	/*SYS_MFG0 = 8,*/
1,	/*SYS_MFG1 = 9,*/
1,	/*SYS_MFG2 = 10,*/
1,	/*SYS_MFG3 = 11,*/
1,	/*SYS_MFG4 = 12,*/
0,	/*SYS_VPU_VCORE_DORMANT = 13,*/
1,	/*SYS_VPU_VCORE_SHUTDOWNT = 14,*/
0,	/*SYS_VPU_CONN_DORMANT = 15,*/
1,	/*SYS_VPU_CONN_SHUTDOWNT = 16,*/
0,	/*SYS_VPU_CORE0_DORMANT = 17,*/
1,	/*SYS_VPU_CORE0_SHUTDOWN = 18,*/
0,	/*SYS_VPU_CORE1_DORMANT = 19,*/
1,	/*SYS_VPU_CORE1_SHUTDOWN = 20,*/
0,	/*SYS_VPU_CORE2_DORMANT = 21,*/
0,	/*SYS_VPU_CORE2_SHUTDOWN = 22,*/
1,	/*SYS_MFG5 = 23,*/
};
#endif
static int enable_subsys(enum subsys_id id)
{
	int r;
	unsigned long flags;
	struct subsys *sys = id_to_sys(id);
	struct pg_callbacks *pgcb;

	if (!sys) {
		WARN_ON(!sys);
		return -EINVAL;
	}

#if MT_CCF_BRINGUP
	/*pr_debug("[CCF] %s: sys=%s, id=%d\n", __func__, sys->name, id);*/
	/*if (sys->ops->get_state(sys) == SUBSYS_PWR_DOWN)*/
	{
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

	list_for_each_entry(pgcb, &pgcb_list, list) {
		if (pgcb->after_on)
			pgcb->after_on(id);
	}
	return r;
}

static int disable_subsys(enum subsys_id id)
{
	int r;
	unsigned long flags;
	struct subsys *sys = id_to_sys(id);
	struct pg_callbacks *pgcb;

	if (!sys) {
		WARN_ON(!sys);
		return -EINVAL;
	}

#if MT_CCF_BRINGUP
	/*pr_debug("[CCF] %s: sys=%s, id=%d\n", __func__, sys->name, id);*/
	/*if (sys->ops->get_state(sys) == SUBSYS_PWR_ON)*/
	{
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
	list_for_each_entry_reverse(pgcb, &pgcb_list, list) {
		if (pgcb->before_off)
			pgcb->before_off(id);
	}

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
	struct clk_init_data init = {};

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

#define pg_md1	"pg_md1"
#define pg_conn	"pg_conn"
#define pg_dis	"pg_dis"
#define pg_cam	"pg_cam"
#define pg_isp	"pg_isp"
#define pg_ven	"pg_ven"
#define pg_vde	"pg_vde"
#define pg_aud	"pg_aud"
#define pg_mfg0	"pg_mfg0"
#define pg_mfg1	"pg_mfg1"
#define pg_mfg2	"pg_mfg2"
#define pg_mfg3	"pg_mfg3"
#define pg_mfg4	"pg_mfg4"
#define pg_mfg5	"pg_mfg5"
#define pg_vpu_vcore_d	"pg_vpu_vcore_d"
#define pg_vpu_vcore_s	"pg_vpu_vcore_s"
#define pg_vpu_conn_d	"pg_vpu_conn_d"
#define pg_vpu_conn_s	"pg_vpu_conn_s"
#define pg_vpu_core0_d	"pg_vpu_core0_d"
#define pg_vpu_core0_s	"pg_vpu_core0_s"
#define pg_vpu_core1_d	"pg_vpu_core1_d"
#define pg_vpu_core1_s	"pg_vpu_core1_s"
#define pg_vpu_core2_d	"pg_vpu_core2_d"
#define pg_vpu_core2_s	"pg_vpu_core2_s"

#define dsp_sel		"dsp_sel"
#define dsp1_sel	"dsp1_sel"
#define dsp2_sel	"dsp2_sel"
#define ipu_if_sel	"ipu_if_sel"
#define img_sel		"img_sel"
#define cam_sel		"cam_sel"
#define mm_sel		"mm_sel"
#define venc_sel	"venc_sel"
#define vdec_sel	"vdec_sel"
#define mfg_sel		"mfg_sel"
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
	PGATE(SCP_SYS_MD1, pg_md1, NULL, NULL, SYS_MD1),
	PGATE(SCP_SYS_CONN, pg_conn, NULL, NULL, SYS_CONN),
	PGATE(SCP_SYS_DIS, pg_dis, NULL, mm_sel, SYS_DIS),
	PGATE(SCP_SYS_CAM, pg_cam, pg_dis, cam_sel, SYS_CAM),
	PGATE(SCP_SYS_ISP, pg_isp, pg_dis, img_sel, SYS_ISP),
	PGATE(SCP_SYS_VEN, pg_ven, pg_dis, venc_sel, SYS_VEN),
	PGATE(SCP_SYS_VDE, pg_vde, pg_dis, vdec_sel, SYS_VDE),

	PGATE(SCP_SYS_AUDIO, pg_aud, NULL,
		infracfg_ao_audio_26m_bclk_ck, SYS_AUDIO),

	PGATE(SCP_SYS_MFG0, pg_mfg0, NULL, mfg_sel, SYS_MFG0),
	PGATE(SCP_SYS_MFG1, pg_mfg1, pg_mfg0, NULL, SYS_MFG1),
	PGATE(SCP_SYS_MFG2, pg_mfg2, pg_mfg1, NULL, SYS_MFG2),
	PGATE(SCP_SYS_MFG3, pg_mfg3, pg_mfg2, NULL, SYS_MFG3),
	PGATE(SCP_SYS_MFG4, pg_mfg4, pg_mfg2, NULL, SYS_MFG4),
	PGATE(SCP_SYS_MFG5, pg_mfg5, pg_mfg2, NULL, SYS_MFG5),

	PGATE(SCP_SYS_VPU_VCORE_DORMANT, pg_vpu_vcore_d, pg_dis,
		ipu_if_sel, SYS_VPU_VCORE_DORMANT),
	PGATE(SCP_SYS_VPU_VCORE_SHUTDOWN, pg_vpu_vcore_s, pg_dis,
		ipu_if_sel, SYS_VPU_VCORE_SHUTDOWN),
	PGATE(SCP_SYS_VPU_CONN_DORMANT, pg_vpu_conn_d, pg_vpu_vcore_d,
		dsp_sel, SYS_VPU_CONN_DORMANT),
	PGATE(SCP_SYS_VPU_CONN_SHUTDOWN, pg_vpu_conn_s, pg_vpu_vcore_s,
		dsp_sel, SYS_VPU_CONN_SHUTDOWN),
	PGATE(SCP_SYS_VPU_CORE0_DORMANT, pg_vpu_core0_d, pg_vpu_conn_d,
		dsp1_sel, SYS_VPU_CORE0_DORMANT),
	PGATE(SCP_SYS_VPU_CORE0_SHUTDOWN, pg_vpu_core0_s, pg_vpu_conn_s,
		dsp1_sel, SYS_VPU_CORE0_SHUTDOWN),
	PGATE(SCP_SYS_VPU_CORE1_DORMANT, pg_vpu_core1_d, pg_vpu_conn_d,
		dsp2_sel, SYS_VPU_CORE1_DORMANT),
	PGATE(SCP_SYS_VPU_CORE1_SHUTDOWN, pg_vpu_core1_s, pg_vpu_conn_s,
		dsp2_sel, SYS_VPU_CORE1_SHUTDOWN),

	PGATE(SCP_SYS_VPU_CORE2_DORMANT, pg_vpu_core2_d, pg_vpu_conn_d,
		dsp2_sel, SYS_VPU_CORE2_DORMANT),//no dsp3, change dummy
	PGATE(SCP_SYS_VPU_CORE2_SHUTDOWN, pg_vpu_core2_s, pg_vpu_conn_s,
		dsp2_sel, SYS_VPU_CORE2_SHUTDOWN),//no dsp3
};

static int init_clk_scpsys(void __iomem *infracfg_reg,
				   void __iomem *spm_reg,
				   void __iomem *infra_reg,
				   void __iomem *smi_common_reg,
				   struct clk_onecell_data *clk_data)
{
	int i;
	struct clk *clk;
	struct clk *pre_clk;

	infracfg_base = infracfg_reg;
	spm_base = spm_reg;
	infra_base = infra_reg;
	smi_common_base = smi_common_reg;

	for (i = 0; i < ARRAY_SIZE(scp_clks); i++) {
		struct mtk_power_gate *pg = &scp_clks[i];

		pre_clk = pg->pre_clk_name ?
			__clk_lookup(pg->pre_clk_name) : NULL;

		clk = mt_clk_register_power_gate(pg->name, pg->parent_name,
			pre_clk, pg->pd_id);

		if (IS_ERR(clk)) {
			pr_notice("[CCF] %s: Failed to register clk %s: %ld\n",
			       __func__, pg->name, PTR_ERR(clk));
			continue;
		}

		if (clk_data)
			clk_data->clks[pg->id] = clk;

#if MT_CCF_DEBUG
		pr_debug("[CCF] %s: pgate %3d: %s\n", __func__, i, pg->name);
#endif				/* MT_CCF_DEBUG */
	}
	return 0;
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
	return of_iomap(np, index);
}

#if 1
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
/*apu vcore*/
	node = of_find_compatible_node(NULL, NULL, "mediatek,apu_vcore");
	if (!node)
		pr_debug("[CLK_APU_VCORE] find node failed\n");
	clk_apu_vcore_base = of_iomap(node, 0);
	if (!clk_apu_vcore_base)
		pr_debug("[CLK_APU_VCORE] base failed\n");
/*apu conn*/
	node = of_find_compatible_node(NULL, NULL, "mediatek,apu_conn");
	if (!node)
		pr_debug("[CLK_APU_CONN] find node failed\n");
	clk_apu_conn_base = of_iomap(node, 0);
	if (!clk_apu_conn_base)
		pr_debug("[CLK_APU_VCORE] base failed\n");
}
#endif
#endif

static int clk_mt6785_scpsys_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	void __iomem *infracfg_reg;
	void __iomem *spm_reg;
	void __iomem *infra_reg;
	void __iomem *ckgen_reg;
	void __iomem *smi_common_reg;
	int r;
	struct device_node *node = pdev->dev.of_node;

	if (!node) {
		pr_err("%s node is null\n", __func__);
		return -EINVAL;
	}

	infracfg_reg = get_reg(node, 0);
	spm_reg = get_reg(node, 1);
	infra_reg = get_reg(node, 2);
	ckgen_reg = get_reg(node, 3);
	smi_common_reg = get_reg(node, 4);



	if (!infracfg_reg || !spm_reg || !infra_reg  ||
		!ckgen_reg || !smi_common_reg) {
		pr_notice("clk-pg-mt3967: missing reg\n");
		return -EINVAL;
	}

/*
 *   pr_debug("[CCF] %s: sys: %s, reg: 0x%p, 0x%p\n",
 *		__func__, node->name, infracfg_reg, spm_reg);
 */

	clk_data = alloc_clk_data(SCP_NR_SYSS);
	if (!clk_data) {
		pr_err("%s clk_data is null\n", __func__);
		return -ENOMEM;
	}

	init_clk_scpsys(infracfg_reg, spm_reg, infra_reg,
		smi_common_reg, clk_data);

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
#if 1/*MT_CCF_BRINGUP*/
	spm_mtcmos_ctrl_mfg0(STA_POWER_ON);
	spm_mtcmos_ctrl_mfg1(STA_POWER_ON);
	spm_mtcmos_ctrl_mfg2(STA_POWER_ON);
	spm_mtcmos_ctrl_mfg3(STA_POWER_ON);
	spm_mtcmos_ctrl_mfg4(STA_POWER_ON);
	spm_mtcmos_ctrl_mfg5(STA_POWER_ON);
#if 0
	spm_mtcmos_ctrl_dis(STA_POWER_ON);
	spm_mtcmos_ctrl_cam(STA_POWER_ON);
	spm_mtcmos_ctrl_ven(STA_POWER_ON);
	spm_mtcmos_ctrl_vde(STA_POWER_ON);
	spm_mtcmos_ctrl_isp(STA_POWER_ON);

/*avoid hang in bring up*/
	spm_mtcmos_ctrl_vpu_vcore_shut_down(STA_POWER_ON);
	spm_mtcmos_ctrl_vpu_conn_shut_down(STA_POWER_ON);
	spm_mtcmos_ctrl_vpu_core0_shut_down(STA_POWER_ON);
	spm_mtcmos_ctrl_vpu_core1_shut_down(STA_POWER_ON);
#endif
	/*spm_mtcmos_ctrl_md1(STA_POWER_ON);*/
	/*spm_mtcmos_ctrl_md1(STA_POWER_DOWN);*/
	spm_mtcmos_ctrl_audio(STA_POWER_ON);
#endif/* !MT_CCF_BRINGUP */
#endif
#endif
	return r;
}

static const char * const *get_cam_clk_names(size_t *num)
{
	static const char * const clks[] = {

		/* CAM */
		"camsys_larb6",
		"camsys_larb7",
		"camsys_gals",
		"camsys_cam",
		"camsys_camtg",
		"camsys_seninf",
		"camsys_camsv0",
		"camsys_camsv1",
		"camsys_ccu",
		"camsys_fake_eng",
	};
	*num = ARRAY_SIZE(clks);
	return clks;
}

static const char * const *get_img_clk_names(size_t *num)
{
	static const char * const clks[] = {

		/* IMG */
		"imgsys_larb5",
		"imgsys_larb4",
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
		"mm_mdp_wrot1",
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
		/* MM1 */
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
		"mm_mdp_hdr",
		"mm_dbi_mmck",
		"mm_dbi_ifck",
		"mm_disp_pm0",
		"mm_disp_hrt_bw",
		"mm_disp_ovl_fbdc",
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
		"venc_gals",
	};
	*num = ARRAY_SIZE(clks);
	return clks;
}

static const char * const *get_vdec_clk_names(size_t *num)
{
	static const char * const clks[] = {

		/* VDEC */
		"vdec_cken",
		/* VDEC1 */
		"vdec_larb1_cken",
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

unsigned int cam_if_on(void)
{
	unsigned int sta = spm_read(PWR_STATUS);
	unsigned int sta_s = spm_read(PWR_STATUS_2ND);

	if ((sta & CAM_PWR_STA_MASK) && (sta_s & CAM_PWR_STA_MASK))
		return 1;
	else
		return 0;
}

void subsys_if_on(void)
{
	unsigned int sta = spm_read(PWR_STATUS);
	unsigned int sta_s = spm_read(PWR_STATUS_2ND);
	unsigned int sta_md1 = spm_read(MD1_PWR_CON);
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

	if (sta_md1 & PWR_ON)
		pr_notice("suspend warning: SYS_MD1 is on!!!\n");
	if ((sta & CONN_PWR_STA_MASK) && (sta_s & CONN_PWR_STA_MASK))
		pr_notice("suspend warning: SYS_CONN is on!!!\n");
	if ((sta & DIS_PWR_STA_MASK) && (sta_s & DIS_PWR_STA_MASK)) {
		pr_notice("suspend warning: SYS_DIS is on!!!\n");
		check_mm0_clk_sts();
		for (i = 0; i < mm_num; i++)
			dump_cg_state(mm_clks[i]);
		ret++;
	}
	if ((sta & MFG0_PWR_STA_MASK) && (sta_s & MFG0_PWR_STA_MASK)) {
		pr_notice("suspend warning: SYS_MFG0 is on!!!\n");
		ret++;
	}
	if ((sta & ISP_PWR_STA_MASK) && (sta_s & ISP_PWR_STA_MASK)) {
		pr_notice("suspend warning: SYS_ISP is on!!!\n");
		check_img_clk_sts();
		for (i = 0; i < img_num; i++)
			dump_cg_state(img_clks[i]);
		ret++;
	}

	if ((sta & MFG1_PWR_STA_MASK) && (sta_s & MFG1_PWR_STA_MASK)) {
		pr_notice("suspend warning: SYS_MFG1 is on!!!\n");
		ret++;
	}
	if ((sta & MFG2_PWR_STA_MASK) && (sta_s & MFG2_PWR_STA_MASK)) {
		pr_notice("suspend warning: SYS_MFG2 is on!!!\n");
		ret++;
	}
	if ((sta & VEN_PWR_STA_MASK) && (sta_s & VEN_PWR_STA_MASK)) {
		pr_notice("suspend warning: SYS_VEN is on!!!\n");
		check_ven_clk_sts();
		for (i = 0; i < venc_num; i++)
			dump_cg_state(venc_clks[i]);
		ret++;
	}
	if ((sta & MFG3_PWR_STA_MASK) && (sta_s & MFG3_PWR_STA_MASK)) {
		pr_notice("suspend warning: SYS_MFG3 is on!!!\n");
		ret++;
	}

	if ((sta & MFG4_PWR_STA_MASK) && (sta_s & MFG4_PWR_STA_MASK)) {
		pr_notice("suspend warning: SYS_MFG4 is on!!!\n");
		ret++;
	}

	if ((sta & MFG5_PWR_STA_MASK) && (sta_s & MFG5_PWR_STA_MASK)) {
		pr_notice("suspend warning: SYS_MFG5 is on!!!\n");
		ret++;
	}

	if ((sta & AUDIO_PWR_STA_MASK) && (sta_s & AUDIO_PWR_STA_MASK))
		pr_notice("suspend warning: SYS_AUDIO is on!!!\n");

	if ((sta & CAM_PWR_STA_MASK) && (sta_s & CAM_PWR_STA_MASK)) {
		pr_notice("suspend warning: SYS_CAM is on!!!\n");
		check_cam_clk_sts();
		for (i = 0; i < cam_num; i++)
			dump_cg_state(cam_clks[i]);
		ret++;
	}
	if ((sta & VPU_VCORE_PWR_STA_MASK) &&
		(sta_s & VPU_VCORE_PWR_STA_MASK)) {
		pr_notice("suspend warning: SYS_VPU_VCORE is on!!!\n");
		ret++;
	}
	if ((sta & VPU_CONN_PWR_STA_MASK) &&
		(sta_s & VPU_CONN_PWR_STA_MASK)) {
		pr_notice("suspend warning: SYS_VPU_CONN is on!!!\n");
		ret++;
	}
	if ((sta & VPU_CORE0_PWR_STA_MASK) &&
		(sta_s & VPU_CORE0_PWR_STA_MASK)) {
		pr_notice("suspend warning: SYS_VPU_CORE0 is on!!!\n");
		ret++;
	}
	if ((sta & VPU_CORE1_PWR_STA_MASK) &&
		(sta_s & VPU_CORE1_PWR_STA_MASK)) {
		pr_notice("suspend warning: SYS_VPU_CORE1 is on!!!\n");
		ret++;
	}

	if ((sta & VDE_PWR_STA_MASK) && (sta_s & VDE_PWR_STA_MASK)) {
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

void mtcmos_force_off(void)
{
	spm_mtcmos_ctrl_mfg5(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg4(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg3(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg2(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg1(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg0(STA_POWER_DOWN);

	spm_mtcmos_ctrl_vpu_core1_shut_down(STA_POWER_DOWN);//check vpu buck?
	spm_mtcmos_ctrl_vpu_core0_shut_down(STA_POWER_DOWN);
	spm_mtcmos_ctrl_vpu_conn_shut_down(STA_POWER_DOWN);
	spm_mtcmos_ctrl_vpu_vcore_shut_down(STA_POWER_DOWN);

	spm_mtcmos_ctrl_cam(STA_POWER_DOWN);
	spm_mtcmos_ctrl_ven(STA_POWER_DOWN);
	spm_mtcmos_ctrl_vde(STA_POWER_DOWN);
	spm_mtcmos_ctrl_isp(STA_POWER_DOWN);
	spm_mtcmos_ctrl_dis(STA_POWER_DOWN);
	spm_mtcmos_ctrl_audio(STA_POWER_DOWN);

	spm_mtcmos_ctrl_conn(STA_POWER_DOWN);
	spm_mtcmos_ctrl_md1(STA_POWER_DOWN);
	pr_notice("%s: %08x, %08x\n", __func__,
		clk_readl(PWR_STATUS), clk_readl(PWR_STATUS_2ND));
}

static const struct of_device_id of_match_clk_mt6785_scpsys[] = {
	{ .compatible = "mediatek,scpsys", },
	{}
};

static struct platform_driver clk_mt6785_scpsys_drv = {
	.probe = clk_mt6785_scpsys_probe,
	.driver = {
		.name = "clk-mt6785-scpsys",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt6785_scpsys,
	},
};

static int __init clk_mt6785_scpsys_init(void)
{

	return platform_driver_register(&clk_mt6785_scpsys_drv);
}

static void __exit clk_mt6785_scpsys_exit(void)
{
	pr_notice("%s: clk_mt6785_scpsys exit!\n", __func__);
}


arch_initcall(clk_mt6785_scpsys_init);
module_exit(clk_mt6785_scpsys_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("MTK");
MODULE_DESCRIPTION("MTK CCF  Driver");

