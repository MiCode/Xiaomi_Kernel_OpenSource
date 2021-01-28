/*
 * Copyright (c) 2019 MediaTek Inc.
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
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <dt-bindings/clock/mt6885-clk.h>

#include "clk-mtk-v1.h"
#include "clk-mt6885-pg.h"
#include "clkdbg-mt6885.h"
#include <mt-plat/aee.h>

#define MT_CCF_DEBUG	0
#define MT_CCF_BRINGUP  0
#define CONTROL_LIMIT	1

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

#define clk_writel(addr, val)		mt_reg_sync_writel(val, addr)
#define clk_readl(addr)			__raw_readl(IOMEM(addr))

/*MM Bus*/
#ifdef CONFIG_OF
void __iomem *clk_mdp_base;
void __iomem *clk_disp_base;
void __iomem *clk_img1_base;
void __iomem *clk_img2_base;
void __iomem *clk_ipe_base;
void __iomem *clk_vdec_soc_gcon_base;
void __iomem *clk_vdec_gcon_base;
void __iomem *clk_venc_gcon_base;
void __iomem *clk_venc_c1_gcon_base;
void __iomem *clk_cam_base;
void __iomem *clk_cam_rawa_base;
void __iomem *clk_cam_rawb_base;
void __iomem *clk_cam_rawc_base;
void __iomem *clk_apu_vcore_base;
void __iomem *clk_apu_conn_base;


#define MDP_CG_CLR1		(clk_mdp_base + 0x118)
#define DISP_CG_CLR1		(clk_disp_base + 0x118)
#define IMG1_CG_CLR		(clk_img1_base + 0x0008)
#define IMG2_CG_CLR		(clk_img2_base + 0x0008)
#define IPE_CG_CLR		(clk_ipe_base + 0x0008)
#define VDEC_CKEN_SET		(clk_vdec_gcon_base + 0x0000)
#define VDEC_LARB1_CKEN_SET	(clk_vdec_gcon_base + 0x0008)
#define VDEC_LAT_CKEN_SET	(clk_vdec_gcon_base + 0x0200)
#define VDEC_SOC_CKEN_SET	(clk_vdec_soc_gcon_base + 0x0000)
#define VDEC_SOC_LARB1_CKEN_SET	(clk_vdec_soc_gcon_base + 0x0008)
#define VDEC_SOC_LAT_CKEN_SET	(clk_vdec_soc_gcon_base + 0x0200)
#define VENC_CG_SET		(clk_venc_gcon_base + 0x0004)
#define VENC_C1_CG_SET		(clk_venc_c1_gcon_base + 0x0004)
#define CAMSYS_CG_CLR		(clk_cam_base + 0x0008)
#define CAMSYS_RAWA_CG_CLR	(clk_cam_rawa_base + 0x0008)
#define CAMSYS_RAWB_CG_CLR	(clk_cam_rawb_base + 0x0008)
#define CAMSYS_RAWC_CG_CLR	(clk_cam_rawc_base + 0x0008)
#define APU_VCORE_CG_CLR	(clk_apu_vcore_base + 0x0008)
#define APU_CONN_CG_CLR		(clk_apu_conn_base + 0x0008)
#define MFG_MISC_CON		INFRACFG_REG(0x0600)
#define MFG_DFD_TRIGGER (1<<19)
#endif

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

/* MT6885: 25+1 subsys */
static struct subsys_ops MD1_sys_ops;
static struct subsys_ops CONN_sys_ops;
static struct subsys_ops MFG0_sys_ops;
static struct subsys_ops MFG1_sys_ops;
static struct subsys_ops MFG2_sys_ops;
static struct subsys_ops MFG3_sys_ops;
static struct subsys_ops MFG4_sys_ops;
static struct subsys_ops MFG5_sys_ops;
static struct subsys_ops MFG6_sys_ops;
static struct subsys_ops ISP_sys_ops;
static struct subsys_ops ISP2_sys_ops;
static struct subsys_ops IPE_sys_ops;
static struct subsys_ops VDE_sys_ops;
static struct subsys_ops VDE2_sys_ops;
static struct subsys_ops VEN_sys_ops;
static struct subsys_ops VEN_CORE1_sys_ops;
static struct subsys_ops MDP_sys_ops;
static struct subsys_ops DIS_sys_ops;
static struct subsys_ops AUDIO_sys_ops;
static struct subsys_ops ADSP_sys_ops;
static struct subsys_ops CAM_sys_ops;
static struct subsys_ops CAM_RAWA_sys_ops;
static struct subsys_ops CAM_RAWB_sys_ops;
static struct subsys_ops CAM_RAWC_sys_ops;
static struct subsys_ops DP_TX_sys_ops;
static struct subsys_ops VPU_sys_ops;

static void __iomem *infracfg_base;	/*infracfg_ao*/
static void __iomem *spm_base;
static void __iomem *ckgen_base;	/*ckgen*/

void __iomem *spm_base_debug;

#define INFRACFG_REG(offset)		(infracfg_base + offset)
#define SPM_REG(offset)			(spm_base + offset)
#define CKGEN_REG(offset)		(ckgen_base + offset)


#define POWERON_CONFIG_EN	SPM_REG(0x0000)
#define PWR_STATUS		SPM_REG(0x016C)
#define PWR_STATUS_2ND		SPM_REG(0x0170)
#define OTHER_PWR_STATUS	SPM_REG(0x0178)	/* for MT6885 VPU only */

#define MD1_PWR_CON		SPM_REG(0x300)
#define CONN_PWR_CON		SPM_REG(0x304)
#define MFG0_PWR_CON		SPM_REG(0x308)
#define MFG1_PWR_CON		SPM_REG(0x30C)
#define MFG2_PWR_CON		SPM_REG(0x310)
#define MFG3_PWR_CON		SPM_REG(0x314)
#define MFG4_PWR_CON		SPM_REG(0x318)
#define MFG5_PWR_CON		SPM_REG(0x31C)
#define MFG6_PWR_CON		SPM_REG(0x320)
#define IFR_PWR_CON		SPM_REG(0x324)
#define IFR_SUB_PWR_CON		SPM_REG(0x328)
#define DPY_PWR_CON		SPM_REG(0x32C)
#define ISP_PWR_CON		SPM_REG(0x330)
#define ISP2_PWR_CON		SPM_REG(0x334)
#define IPE_PWR_CON		SPM_REG(0x338)
#define VDE_PWR_CON		SPM_REG(0x33C)
#define VDE2_PWR_CON		SPM_REG(0x340)
#define VEN_PWR_CON		SPM_REG(0x344)
#define VEN_CORE1_PWR_CON	SPM_REG(0x348)
#define MDP_PWR_CON		SPM_REG(0x34C)
#define DIS_PWR_CON		SPM_REG(0x350)
#define AUDIO_PWR_CON		SPM_REG(0x354)
#define ADSP_PWR_CON		SPM_REG(0x358)
#define CAM_PWR_CON		SPM_REG(0x35C)
#define CAM_RAWA_PWR_CON	SPM_REG(0x360)
#define CAM_RAWB_PWR_CON	SPM_REG(0x364)
#define CAM_RAWC_PWR_CON	SPM_REG(0x368)
#define DP_TX_PWR_CON		SPM_REG(0x3AC)
#define DPY2_PWR_CON		SPM_REG(0x3C4)
#define MD_EXT_BUCK_ISO_CON	SPM_REG(0x398)
#define EXT_BUCK_ISO		SPM_REG(0x39C)

#define SPM_CROSS_WAKE_M01_REQ	SPM_REG(0x670)	/* for MT6885 VPU wakeup src */
#define APMCU_WAKEUP_APU	(0x1 << 0)

/* for MT6885 DISP/MDP MTCMOS on/off APIs  */
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
#define INFRA_TOPAXI_PROTECTEN_MCU_STA0			INFRACFG_REG(0x02E0)
#define INFRA_TOPAXI_PROTECTEN_MCU_STA1			INFRACFG_REG(0x02E4)
#define INFRA_TOPAXI_PROTECTEN_MCU_SET			INFRACFG_REG(0x02C4)
#define INFRA_TOPAXI_PROTECTEN_MCU_CLR			INFRACFG_REG(0x02C8)

#define INFRA_TOPAXI_PROTECTEN_MM			INFRACFG_REG(0x02D0)
#define INFRA_TOPAXI_PROTECTEN_MM_SET			INFRACFG_REG(0x02D4)
#define INFRA_TOPAXI_PROTECTEN_MM_CLR			INFRACFG_REG(0x02D8)
#define INFRA_TOPAXI_PROTECTEN_MM_STA0			INFRACFG_REG(0x02E8)
#define INFRA_TOPAXI_PROTECTEN_MM_STA1			INFRACFG_REG(0x02EC)

#define INFRA_TOPAXI_PROTECTEN_2			INFRACFG_REG(0x0710)
#define INFRA_TOPAXI_PROTECTEN_2_SET			INFRACFG_REG(0x0714)
#define INFRA_TOPAXI_PROTECTEN_2_CLR			INFRACFG_REG(0x0718)
#define INFRA_TOPAXI_PROTECTEN_STA0_2			INFRACFG_REG(0x0720)
#define INFRA_TOPAXI_PROTECTEN_STA1_2			INFRACFG_REG(0x0724)

#define INFRA_TOPAXI_PROTECTEN_MM_2			INFRACFG_REG(0x0DC8)
#define INFRA_TOPAXI_PROTECTEN_MM_2_SET			INFRACFG_REG(0x0DCC)
#define INFRA_TOPAXI_PROTECTEN_MM_2_CLR			INFRACFG_REG(0x0DD0)
#define INFRA_TOPAXI_PROTECTEN_MM_2_STA0		INFRACFG_REG(0x0DD4)
#define INFRA_TOPAXI_PROTECTEN_MM_2_STA1		INFRACFG_REG(0x0DD8)

#define INFRA_TOPAXI_PROTECTEN_INFRA_VDNR		INFRACFG_REG(0x0B80)
#define INFRA_TOPAXI_PROTECTEN_INFRA_VDNR_SET		INFRACFG_REG(0x0B84)
#define INFRA_TOPAXI_PROTECTEN_INFRA_VDNR_CLR		INFRACFG_REG(0x0B88)
#define INFRA_TOPAXI_PROTECTEN_INFRA_VDNR_STA0		INFRACFG_REG(0x0B8c)
#define INFRA_TOPAXI_PROTECTEN_INFRA_VDNR_STA1		INFRACFG_REG(0x0B90)

#define INFRA_TOPAXI_PROTECTEN_INFRA_VDNR_1		INFRACFG_REG(0x0BA0)
#define INFRA_TOPAXI_PROTECTEN_INFRA_VDNR_1_SET		INFRACFG_REG(0x0BA4)
#define INFRA_TOPAXI_PROTECTEN_INFRA_VDNR_1_CLR		INFRACFG_REG(0x0BA8)
#define INFRA_TOPAXI_PROTECTEN_INFRA_VDNR_1_STA0	INFRACFG_REG(0x0BAc)
#define INFRA_TOPAXI_PROTECTEN_INFRA_VDNR_1_STA1	INFRACFG_REG(0x0BB0)

#define INFRA_TOPAXI_PROTECTEN_SUB_INFRA_VDNR		INFRACFG_REG(0x0BB4)
#define INFRA_TOPAXI_PROTECTEN_SUB_INFRA_VDNR_SET	INFRACFG_REG(0x0BB8)
#define INFRA_TOPAXI_PROTECTEN_SUB_INFRA_VDNR_CLR	INFRACFG_REG(0x0BBC)
#define INFRA_TOPAXI_PROTECTEN_SUB_INFRA_VDNR_STA0	INFRACFG_REG(0x0BC0)
#define INFRA_TOPAXI_PROTECTEN_SUB_INFRA_VDNR_STA1	INFRACFG_REG(0x0BC4)


/* Autogen Begin, 0724 version  */
#define  SPM_PROJECT_CODE    0xB16
//#define VDEC_ACTIVE	((0x1 << 4))

/* Define MTCMOS power control */
#define PWR_RST_B                        (0x1 << 0)
#define PWR_ISO                          (0x1 << 1)
#define PWR_ON                           (0x1 << 2)
#define PWR_ON_2ND                       (0x1 << 3)
#define PWR_CLK_DIS                      (0x1 << 4)
#define SRAM_CKISO                       (0x1 << 5)
#define SRAM_ISOINT_B                    (0x1 << 6)
#define DORMANT_ENABLE                   (0x1 << 6)
#define SLPB_CLAMP                       (0x1 << 7)
#define VPROC_EXT_OFF                    (0x1 << 7)

/* Define MTCMOS Bus Protect Mask */
#define MD1_PROT_STEP1_0_MASK            ((0x1 << 7))
#define MD1_PROT_STEP1_0_ACK_MASK        ((0x1 << 7))
#define MD1_PROT_STEP2_0_MASK            ((0x1 << 10) \
					  |(0x1 << 21) \
					  |(0x1 << 29))
#define MD1_PROT_STEP2_0_ACK_MASK        ((0x1 << 10) \
					  |(0x1 << 21) \
					  |(0x1 << 29))
#define MD1_PROT_STEP2_1_MASK            ((0x1 << 6) \
					  |(0x1 << 7))
#define MD1_PROT_STEP2_1_ACK_MASK        ((0x1 << 6) \
					  |(0x1 << 7))
#define CONN_PROT_STEP1_0_MASK           ((0x1 << 13))
#define CONN_PROT_STEP1_0_ACK_MASK       ((0x1 << 13))
#define CONN_PROT_STEP2_0_MASK           ((0x1 << 14))
#define CONN_PROT_STEP2_0_ACK_MASK       ((0x1 << 14))
#define CONN_PROT_STEP2_1_MASK           ((0x1 << 2))
#define CONN_PROT_STEP2_1_ACK_MASK       ((0x1 << 2))
#define MFG1_PROT_STEP1_0_MASK           ((0x1 << 19) \
					  |(0x1 << 20) \
					  |(0x1 << 21))
#define MFG1_PROT_STEP1_0_ACK_MASK       ((0x1 << 19) \
					  |(0x1 << 20) \
					  |(0x1 << 21))
#define MFG1_PROT_STEP1_1_MASK           ((0x1 << 5) \
					  |(0x1 << 6))
#define MFG1_PROT_STEP1_1_ACK_MASK       ((0x1 << 5) \
					  |(0x1 << 6))
#define MFG1_PROT_STEP2_0_MASK           ((0x1 << 21) \
					  |(0x1 << 22))
#define MFG1_PROT_STEP2_0_ACK_MASK       ((0x1 << 21) \
					  |(0x1 << 22))
#define MFG1_PROT_STEP2_1_MASK           ((0x1 << 7))
#define MFG1_PROT_STEP2_1_ACK_MASK       ((0x1 << 7))
#define MFG1_PROT_STEP2_2_MASK           ((0x1 << 17) \
					  |(0x1 << 19))
#define MFG1_PROT_STEP2_2_ACK_MASK       ((0x1 << 17) \
					  |(0x1 << 19))
#define IFR_PROT_STEP1_0_MASK            ((0x1 << 3) \
					  |(0x1 << 4) \
					  |(0x1 << 7) \
					  |(0x1 << 9) \
					  |(0x1 << 10) \
					  |(0x1 << 12) \
					  |(0x1 << 13) \
					  |(0x1 << 16) \
					  |(0x1 << 17) \
					  |(0x1 << 20) \
					  |(0x1 << 24) \
					  |(0x1 << 27) \
					  |(0x1 << 28))
#define IFR_PROT_STEP1_0_ACK_MASK        ((0x1 << 3) \
					  |(0x1 << 4) \
					  |(0x1 << 7) \
					  |(0x1 << 9) \
					  |(0x1 << 10) \
					  |(0x1 << 12) \
					  |(0x1 << 13) \
					  |(0x1 << 16) \
					  |(0x1 << 17) \
					  |(0x1 << 20) \
					  |(0x1 << 24) \
					  |(0x1 << 27) \
					  |(0x1 << 28))
#define IFR_PROT_STEP1_1_MASK            ((0x1 << 21))
#define IFR_PROT_STEP1_1_ACK_MASK        ((0x1 << 21))
#define IFR_PROT_STEP1_2_MASK            ((0x1 << 1) \
					  |(0x1 << 2))
#define IFR_PROT_STEP1_2_ACK_MASK        ((0x1 << 1) \
					  |(0x1 << 2))
#define IFR_PROT_STEP1_3_MASK            ((0x1 << 4))
#define IFR_PROT_STEP1_3_ACK_MASK        ((0x1 << 4))
#define IFR_PROT_STEP1_4_MASK            ((0x1 << 13) \
					  |(0x1 << 16) \
					  |(0x1 << 30) \
					  |(0x1 << 31))
#define IFR_PROT_STEP1_4_ACK_MASK        ((0x1 << 13) \
					  |(0x1 << 16) \
					  |(0x1 << 30) \
					  |(0x1 << 31))
#define IFR_PROT_STEP1_5_MASK            ((0x1 << 0) \
					  |(0x1 << 31))
#define IFR_PROT_STEP1_5_ACK_MASK        ((0x1 << 0) \
					  |(0x1 << 31))
#define IFR_PROT_STEP2_0_MASK            ((0x1 << 14))
#define IFR_PROT_STEP2_0_ACK_MASK        ((0x1 << 14))
#define IFR_PROT_STEP2_1_MASK            ((0x1 << 22))
#define IFR_PROT_STEP2_1_ACK_MASK        ((0x1 << 22))
#define IFR_PROT_STEP2_2_MASK            ((0x1 << 7))
#define IFR_PROT_STEP2_2_ACK_MASK        ((0x1 << 7))
#define IFR_PROT_STEP2_3_MASK            ((0x1 << 28) \
					  |(0x1 << 29))
#define IFR_PROT_STEP2_3_ACK_MASK        ((0x1 << 28) \
					  |(0x1 << 29))
#define IFR_PROT_STEP2_4_MASK            ((0x1 << 30))
#define IFR_PROT_STEP2_4_ACK_MASK        ((0x1 << 30))
#define ISP_PROT_STEP1_0_MASK            ((0x1 << 8))
#define ISP_PROT_STEP1_0_ACK_MASK        ((0x1 << 8))
#define ISP_PROT_STEP2_0_MASK            ((0x1 << 9))
#define ISP_PROT_STEP2_0_ACK_MASK        ((0x1 << 9))
#define ISP2_PROT_STEP1_0_MASK           ((0x1 << 14))
#define ISP2_PROT_STEP1_0_ACK_MASK       ((0x1 << 14))
#define ISP2_PROT_STEP2_0_MASK           ((0x1 << 15))
#define ISP2_PROT_STEP2_0_ACK_MASK       ((0x1 << 15))
#define IPE_PROT_STEP1_0_MASK            ((0x1 << 16))
#define IPE_PROT_STEP1_0_ACK_MASK        ((0x1 << 16))
#define IPE_PROT_STEP2_0_MASK            ((0x1 << 17))
#define IPE_PROT_STEP2_0_ACK_MASK        ((0x1 << 17))
#define VDE_PROT_STEP1_0_MASK            ((0x1 << 24))
#define VDE_PROT_STEP1_0_ACK_MASK        ((0x1 << 24))
#define VDE_PROT_STEP2_0_MASK            ((0x1 << 25))
#define VDE_PROT_STEP2_0_ACK_MASK        ((0x1 << 25))
#define VDE2_PROT_STEP1_0_MASK           ((0x1 << 6))
#define VDE2_PROT_STEP1_0_ACK_MASK       ((0x1 << 6))
#define VDE2_PROT_STEP2_0_MASK           ((0x1 << 7))
#define VDE2_PROT_STEP2_0_ACK_MASK       ((0x1 << 7))
#define VEN_PROT_STEP1_0_MASK            ((0x1 << 26))
#define VEN_PROT_STEP1_0_ACK_MASK        ((0x1 << 26))
#define VEN_PROT_STEP1_1_MASK            ((0x1 << 0))
#define VEN_PROT_STEP1_1_ACK_MASK        ((0x1 << 0))
#define VEN_PROT_STEP2_0_MASK            ((0x1 << 27))
#define VEN_PROT_STEP2_0_ACK_MASK        ((0x1 << 27))
#define VEN_PROT_STEP2_1_MASK            ((0x1 << 1))
#define VEN_PROT_STEP2_1_ACK_MASK        ((0x1 << 1))
#define VEN_CORE1_PROT_STEP1_0_MASK      ((0x1 << 28) \
					  |(0x1 << 30))
#define VEN_CORE1_PROT_STEP1_0_ACK_MASK   ((0x1 << 28) \
					  |(0x1 << 30))
#define VEN_CORE1_PROT_STEP2_0_MASK      ((0x1 << 29) \
					  |(0x1 << 31))
#define VEN_CORE1_PROT_STEP2_0_ACK_MASK   ((0x1 << 29) \
					  |(0x1 << 31))
#define MDP_PROT_STEP1_0_MASK            ((0x1 << 10))
#define MDP_PROT_STEP1_0_ACK_MASK        ((0x1 << 10))
#define MDP_PROT_STEP1_1_MASK            ((0x1 << 2) \
					  |(0x1 << 4) \
					  |(0x1 << 6) \
					  |(0x1 << 8) \
					  |(0x1 << 18) \
					  |(0x1 << 22) \
					  |(0x1 << 28) \
					  |(0x1 << 30))
#define MDP_PROT_STEP1_1_ACK_MASK        ((0x1 << 2) \
					  |(0x1 << 4) \
					  |(0x1 << 6) \
					  |(0x1 << 8) \
					  |(0x1 << 18) \
					  |(0x1 << 22) \
					  |(0x1 << 28) \
					  |(0x1 << 30))
#define MDP_PROT_STEP1_2_MASK            ((0x1 << 0) \
					  |(0x1 << 2) \
					  |(0x1 << 4) \
					  |(0x1 << 6) \
					  |(0x1 << 8))
#define MDP_PROT_STEP1_2_ACK_MASK        ((0x1 << 0) \
					  |(0x1 << 2) \
					  |(0x1 << 4) \
					  |(0x1 << 6) \
					  |(0x1 << 8))
#define MDP_PROT_STEP2_0_MASK            ((0x1 << 23))
#define MDP_PROT_STEP2_0_ACK_MASK        ((0x1 << 23))
#define MDP_PROT_STEP2_1_MASK            ((0x1 << 3) \
					  |(0x1 << 5) \
					  |(0x1 << 7) \
					  |(0x1 << 9) \
					  |(0x1 << 19) \
					  |(0x1 << 23) \
					  |(0x1 << 29) \
					  |(0x1 << 31))
#define MDP_PROT_STEP2_1_ACK_MASK        ((0x1 << 3) \
					  |(0x1 << 5) \
					  |(0x1 << 7) \
					  |(0x1 << 9) \
					  |(0x1 << 19) \
					  |(0x1 << 23) \
					  |(0x1 << 29) \
					  |(0x1 << 31))
#define MDP_PROT_STEP2_2_MASK            ((0x1 << 1) \
					  |(0x1 << 7) \
					  |(0x1 << 9) \
					  |(0x1 << 11))
#define MDP_PROT_STEP2_2_ACK_MASK        ((0x1 << 1) \
					  |(0x1 << 7) \
					  |(0x1 << 9) \
					  |(0x1 << 11))
#define MDP_PROT_STEP2_3_MASK            ((0x1 << 20))
#define MDP_PROT_STEP2_3_ACK_MASK        ((0x1 << 20))
#define DIS_PROT_STEP1_0_MASK            ((0x1 << 0) \
					  |(0x1 << 6) \
					  |(0x1 << 8) \
					  |(0x1 << 10) \
					  |(0x1 << 12) \
					  |(0x1 << 14) \
					  |(0x1 << 16) \
					  |(0x1 << 20) \
					  |(0x1 << 24) \
					  |(0x1 << 26))
#define DIS_PROT_STEP1_0_ACK_MASK        ((0x1 << 0) \
					  |(0x1 << 6) \
					  |(0x1 << 8) \
					  |(0x1 << 10) \
					  |(0x1 << 12) \
					  |(0x1 << 14) \
					  |(0x1 << 16) \
					  |(0x1 << 20) \
					  |(0x1 << 24) \
					  |(0x1 << 26))
#define DIS_PROT_STEP2_0_MASK            ((0x1 << 6))
#define DIS_PROT_STEP2_0_ACK_MASK        ((0x1 << 6))
#define DIS_PROT_STEP2_1_MASK            ((0x1 << 1) \
					  |(0x1 << 7) \
					  |(0x1 << 9) \
					  |(0x1 << 15) \
					  |(0x1 << 17) \
					  |(0x1 << 21) \
					  |(0x1 << 25) \
					  |(0x1 << 27))
#define DIS_PROT_STEP2_1_ACK_MASK        ((0x1 << 1) \
					  |(0x1 << 7) \
					  |(0x1 << 9) \
					  |(0x1 << 15) \
					  |(0x1 << 17) \
					  |(0x1 << 21) \
					  |(0x1 << 25) \
					  |(0x1 << 27))
#define DIS_PROT_STEP2_2_MASK            ((0x1 << 21))
#define DIS_PROT_STEP2_2_ACK_MASK        ((0x1 << 21))
#define AUDIO_PROT_STEP1_0_MASK          ((0x1 << 4))
#define AUDIO_PROT_STEP1_0_ACK_MASK      ((0x1 << 4))
#define ADSP_PROT_STEP1_0_MASK           ((0x1 << 3))
#define ADSP_PROT_STEP1_0_ACK_MASK       ((0x1 << 3))
#define CAM_PROT_STEP1_0_MASK            ((0x1 << 1))
#define CAM_PROT_STEP1_0_ACK_MASK        ((0x1 << 1))
#define CAM_PROT_STEP1_1_MASK            ((0x1 << 0) \
					  |(0x1 << 2) \
					  |(0x1 << 4))
#define CAM_PROT_STEP1_1_ACK_MASK        ((0x1 << 0) \
					  |(0x1 << 2) \
					  |(0x1 << 4))
#define CAM_PROT_STEP2_0_MASK            ((0x1 << 22))
#define CAM_PROT_STEP2_0_ACK_MASK        ((0x1 << 22))
#define CAM_PROT_STEP2_1_MASK            ((0x1 << 1) \
					  |(0x1 << 3) \
					  |(0x1 << 5))
#define CAM_PROT_STEP2_1_ACK_MASK        ((0x1 << 1) \
					  |(0x1 << 3) \
					  |(0x1 << 5))
#define CAM_RAWA_PROT_STEP1_0_MASK       ((0x1 << 18))
#define CAM_RAWA_PROT_STEP1_0_ACK_MASK   ((0x1 << 18))
#define CAM_RAWA_PROT_STEP2_0_MASK       ((0x1 << 19))
#define CAM_RAWA_PROT_STEP2_0_ACK_MASK   ((0x1 << 19))
#define CAM_RAWB_PROT_STEP1_0_MASK       ((0x1 << 20))
#define CAM_RAWB_PROT_STEP1_0_ACK_MASK   ((0x1 << 20))
#define CAM_RAWB_PROT_STEP2_0_MASK       ((0x1 << 21))
#define CAM_RAWB_PROT_STEP2_0_ACK_MASK   ((0x1 << 21))
#define CAM_RAWC_PROT_STEP1_0_MASK       ((0x1 << 22))
#define CAM_RAWC_PROT_STEP1_0_ACK_MASK   ((0x1 << 22))
#define CAM_RAWC_PROT_STEP2_0_MASK       ((0x1 << 23))
#define CAM_RAWC_PROT_STEP2_0_ACK_MASK   ((0x1 << 23))

/* Define MTCMOS Power Status Mask */

#define MD1_PWR_STA_MASK                 (0x1 << 0)
#define CONN_PWR_STA_MASK                (0x1 << 1)
#define MFG0_PWR_STA_MASK                (0x1 << 2)
#define MFG1_PWR_STA_MASK                (0x1 << 3)
#define MFG2_PWR_STA_MASK                (0x1 << 4)
#define MFG3_PWR_STA_MASK                (0x1 << 5)
#define MFG4_PWR_STA_MASK                (0x1 << 6)
#define MFG5_PWR_STA_MASK                (0x1 << 7)
#define MFG6_PWR_STA_MASK                (0x1 << 8)
#define IFR_PWR_STA_MASK                 (0x1 << 9)
#define IFR_SUB_PWR_STA_MASK             (0x1 << 10)
#define DPY_PWR_STA_MASK                 (0x1 << 11)
#define ISP_PWR_STA_MASK                 (0x1 << 12)
#define ISP2_PWR_STA_MASK                (0x1 << 13)
#define IPE_PWR_STA_MASK                 (0x1 << 14)
#define VDE_PWR_STA_MASK                 (0x1 << 15)
#define VDE2_PWR_STA_MASK                (0x1 << 16)
#define VEN_PWR_STA_MASK                 (0x1 << 17)
#define VEN_CORE1_PWR_STA_MASK           (0x1 << 18)
#define MDP_PWR_STA_MASK                 (0x1 << 19)
#define DIS_PWR_STA_MASK                 (0x1 << 20)
#define AUDIO_PWR_STA_MASK               (0x1 << 21)
#define ADSP_PWR_STA_MASK                (0x1 << 22)
#define CAM_PWR_STA_MASK                 (0x1 << 23)
#define CAM_RAWA_PWR_STA_MASK            (0x1 << 24)
#define CAM_RAWB_PWR_STA_MASK            (0x1 << 25)
#define CAM_RAWC_PWR_STA_MASK            (0x1 << 26)
#define DP_TX_PWR_STA_MASK               (0x1 << 27)
#define DPY2_PWR_STA_MASK                (0x1 << 28)

/* Define CPU SRAM Mask */
#define ADSP_SRAM_SLEEP_B                (0x1 << 9)
#define ADSP_SRAM_SLEEP_B_ACK            (0x1 << 13)
#define ADSP_SRAM_SLEEP_B_ACK_BIT0       (0x1 << 13)

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
#define MFG4_SRAM_PDN                    (0x1 << 8)
#define MFG4_SRAM_PDN_ACK                (0x1 << 12)
#define MFG4_SRAM_PDN_ACK_BIT0           (0x1 << 12)
#define MFG5_SRAM_PDN                    (0x1 << 8)
#define MFG5_SRAM_PDN_ACK                (0x1 << 12)
#define MFG5_SRAM_PDN_ACK_BIT0           (0x1 << 12)
#define MFG6_SRAM_PDN                    (0x1 << 8)
#define MFG6_SRAM_PDN_ACK                (0x1 << 12)
#define MFG6_SRAM_PDN_ACK_BIT0           (0x1 << 12)
#define IFR_SRAM_PDN                     (0x1 << 8)
#define IFR_SRAM_PDN_ACK                 (0x1 << 12)
#define IFR_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define IFR_SUB_SRAM_PDN                 (0x1 << 8)
#define IFR_SUB_SRAM_PDN_ACK             (0x1 << 12)
#define IFR_SUB_SRAM_PDN_ACK_BIT0        (0x1 << 12)
#define DPY_SRAM_PDN                     (0x1 << 8)
#define DPY_SRAM_PDN_ACK                 (0x1 << 12)
#define DPY_SRAM_PDN_ACK_BIT0            (0x1 << 12)
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
#define VDE2_SRAM_PDN                    (0x1 << 8)
#define VDE2_SRAM_PDN_ACK                (0x1 << 12)
#define VDE2_SRAM_PDN_ACK_BIT0           (0x1 << 12)
#define VEN_SRAM_PDN                     (0x1 << 8)
#define VEN_SRAM_PDN_ACK                 (0x1 << 12)
#define VEN_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define VEN_CORE1_SRAM_PDN               (0x1 << 8)
#define VEN_CORE1_SRAM_PDN_ACK           (0x1 << 12)
#define VEN_CORE1_SRAM_PDN_ACK_BIT0      (0x1 << 12)
#define MDP_SRAM_PDN                     (0x1 << 8)
#define MDP_SRAM_PDN_ACK                 (0x1 << 12)
#define MDP_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define DIS_SRAM_PDN                     (0x1 << 8)
#define DIS_SRAM_PDN_ACK                 (0x1 << 12)
#define DIS_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define AUDIO_SRAM_PDN                   (0x1 << 8)
#define AUDIO_SRAM_PDN_ACK               (0x1 << 12)
#define AUDIO_SRAM_PDN_ACK_BIT0          (0x1 << 12)
#define ADSP_SRAM_PDN                    (0x1 << 8)
#define ADSP_SRAM_PDN_ACK                (0x1 << 12)
#define ADSP_SRAM_PDN_ACK_BIT0           (0x1 << 12)
#define CAM_SRAM_PDN                     (0x1 << 8)
#define CAM_SRAM_PDN_ACK                 (0x1 << 12)
#define CAM_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define CAM_RAWA_SRAM_PDN                (0x1 << 8)
#define CAM_RAWA_SRAM_PDN_ACK            (0x1 << 12)
#define CAM_RAWA_SRAM_PDN_ACK_BIT0       (0x1 << 12)
#define CAM_RAWB_SRAM_PDN                (0x1 << 8)
#define CAM_RAWB_SRAM_PDN_ACK            (0x1 << 12)
#define CAM_RAWB_SRAM_PDN_ACK_BIT0       (0x1 << 12)
#define CAM_RAWC_SRAM_PDN                (0x1 << 8)
#define CAM_RAWC_SRAM_PDN_ACK            (0x1 << 12)
#define CAM_RAWC_SRAM_PDN_ACK_BIT0       (0x1 << 12)
#define DP_TX_SRAM_PDN                   (0x1 << 8)
#define DP_TX_SRAM_PDN_ACK               (0x1 << 12)
#define DP_TX_SRAM_PDN_ACK_BIT0          (0x1 << 12)
#define DPY2_SRAM_PDN                    (0x1 << 8)
#define DPY2_SRAM_PDN_ACK                (0x1 << 12)
#define DPY2_SRAM_PDN_ACK_BIT0           (0x1 << 12)
/* Autogen End */

static struct subsys syss[] =	/* NR_SYSS */
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
	[SYS_MFG5] = {
			.name = __stringify(SYS_MFG5),
			.sta_mask = MFG5_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &MFG5_sys_ops,
			},
	[SYS_MFG6] = {
			.name = __stringify(SYS_MFG6),
			.sta_mask = MFG6_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &MFG6_sys_ops,
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
	[SYS_ISP2] = {
			.name = __stringify(SYS_ISP2),
			.sta_mask = ISP2_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &ISP2_sys_ops,
			},
	[SYS_IPE] = {
			.name = __stringify(SYS_IPE),
			.sta_mask = IPE_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &IPE_sys_ops,
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
	[SYS_VDE2] = {
			.name = __stringify(SYS_VDE2),
			.sta_mask = VDE2_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &VDE2_sys_ops,
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
	[SYS_VEN_CORE1] = {
			.name = __stringify(SYS_VEN_CORE1),
			.sta_mask = VEN_CORE1_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &VEN_CORE1_sys_ops,
			},
	[SYS_MDP] = {
			.name = __stringify(SYS_MDP),
			.sta_mask = MDP_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &MDP_sys_ops,
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
	[SYS_AUDIO] = {
			.name = __stringify(SYS_AUDIO),
			.sta_mask = AUDIO_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &AUDIO_sys_ops,
			},
	[SYS_ADSP] = {
			.name = __stringify(SYS_ADSP),
			.sta_mask = ADSP_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &ADSP_sys_ops,
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
	[SYS_CAM_RAWA] = {
			.name = __stringify(SYS_CAM_RAWA),
			.sta_mask = CAM_RAWA_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &CAM_RAWA_sys_ops,
			},
	[SYS_CAM_RAWB] = {
			.name = __stringify(SYS_CAM_RAWB),
			.sta_mask = CAM_RAWB_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &CAM_RAWB_sys_ops,
			},
	[SYS_CAM_RAWC] = {
			.name = __stringify(SYS_CAM_RAWC),
			.sta_mask = CAM_RAWC_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &CAM_RAWC_sys_ops,
			},
	[SYS_DP_TX] = {
			.name = __stringify(SYS_DP_TX),
			.sta_mask = DP_TX_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &DP_TX_sys_ops,
			},
	[SYS_VPU] = {
			.name = __stringify(SYS_VPU),
			 /* MT6885: fixme: resident in OTHER_PWR_STATUS */
			.sta_mask = (0x1 << 5),
			/* .ctl_addr = NULL,  */
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


#define DBG_ID_MD1		0
#define DBG_ID_CONN		1
#define DBG_ID_MFG0		2
#define DBG_ID_MFG1		3
#define DBG_ID_MFG2		4
#define DBG_ID_MFG3		5
#define DBG_ID_MFG4		6
#define DBG_ID_MFG5		7
#define DBG_ID_MFG6		8
#define DBG_ID_ISP		9
#define DBG_ID_ISP2		10
#define DBG_ID_IPE		11
#define DBG_ID_VDE		12
#define DBG_ID_VDE2		13
#define DBG_ID_VEN		14
#define DBG_ID_VEN_CORE1	15
#define DBG_ID_MDP		16
#define DBG_ID_DIS		17
#define DBG_ID_AUDIO		18
#define DBG_ID_ADSP		19
#define DBG_ID_CAM		20
#define DBG_ID_CAM_RAWA		21
#define DBG_ID_CAM_RAWB		22
#define DBG_ID_CAM_RAWC		23
#define DBG_ID_DP_TX		24
#define DBG_ID_VPU		25


#define ID_MADK   0xFF000000
#define STA_MASK  0x00F00000
#define STEP_MASK 0x000000FF

#define INCREASE_STEPS \
	do { DBG_STEP++; hang_release = true; } while (0)

static int DBG_ID;
static int DBG_STA;
static int DBG_STEP;

static unsigned long long block_time;
static unsigned long long upd_block_time;
static bool hang_release;
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
	static u32 pre_data;
	static s32 loop_cnt = -1;
	static bool log_over_cnt;
	static bool log_timeout;
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

	if (pre_data == data[0]) {
		upd_block_time = sched_clock();
		if (loop_cnt >= 0)
			loop_cnt++;
	}

	if (pre_data != data[0] || hang_release) {
		hang_release = false;
		pre_data = data[0];
		block_time = sched_clock();
		loop_cnt = 0;
		log_over_cnt = false;
	}

	if (loop_cnt > 5000) {
		log_over_cnt = true;
		loop_cnt = -1;
	}

	if ((upd_block_time > 0  && block_time > 0)
			&& (upd_block_time > block_time)
			&& (upd_block_time - block_time > 5000000000))
		log_timeout = true;

	if (log_over_cnt || log_timeout) {
		pr_notice("%s: upd(%llu ns), ori(%llu ns)\n", __func__,
				upd_block_time, block_time);

		log_over_cnt = false;

		print_enabled_clks_once();

		for (i = 0; i < ARRAY_SIZE(data); i++)
			pr_notice("%s: data[%i]=%08x\n", __func__, i, data[i]);

		/* The code based on  clkdbg/clkdbg-mt6885. */
		/* When power on/off fails, dump the related registers. */
		print_subsys_reg(topckgen);
		print_subsys_reg(infracfg);
		print_subsys_reg(infracfg_dbg);
		print_subsys_reg(scpsys);
		print_subsys_reg(apmixed);

		if (DBG_STA == STA_POWER_DOWN) {
			/* dump only when power off failes */
			if (DBG_ID == DBG_ID_MFG0 || DBG_ID == DBG_ID_MFG1
			|| DBG_ID == DBG_ID_MFG2 || DBG_ID == DBG_ID_MFG3
			|| DBG_ID == DBG_ID_MFG4 || DBG_ID == DBG_ID_MFG5
			|| DBG_ID == DBG_ID_MFG6)
				print_subsys_reg(mfgsys);

			if (DBG_ID == DBG_ID_AUDIO)
				print_subsys_reg(audio);

			if (DBG_ID == DBG_ID_DIS)
				print_subsys_reg(mmsys);

			if (DBG_ID == DBG_ID_MDP)
				print_subsys_reg(mdpsys);

			/* isp/img */
			if (DBG_ID == DBG_ID_ISP) {
				print_subsys_reg(mmsys);
				print_subsys_reg(img1sys);
				}
			if (DBG_ID == DBG_ID_ISP2) {
				print_subsys_reg(mmsys);
				print_subsys_reg(mdpsys);
				print_subsys_reg(img2sys);
			}

			/* ipe */
			if (DBG_ID == DBG_ID_IPE) {
				print_subsys_reg(mmsys);
				print_subsys_reg(mdpsys);
				print_subsys_reg(ipesys);
			}

			/* venc */
			if (DBG_ID == DBG_ID_VEN) {
				print_subsys_reg(mmsys);
				print_subsys_reg(vencsys);
			}
			if (DBG_ID == DBG_ID_VEN_CORE1) {
				print_subsys_reg(mmsys);
				print_subsys_reg(mdpsys);
				print_subsys_reg(venc_c1_sys);
			}

			/* vdec */
			if (DBG_ID == DBG_ID_VDE) {
				print_subsys_reg(mmsys);
				print_subsys_reg(vdec_soc_sys);
			}
			if (DBG_ID == DBG_ID_VDE2) {
				print_subsys_reg(mmsys);
				print_subsys_reg(vdec_soc_sys);
				print_subsys_reg(vdecsys);
			}

			/* cam */
			if (DBG_ID == DBG_ID_CAM) {
				print_subsys_reg(mmsys);
				print_subsys_reg(mdpsys);
				print_subsys_reg(camsys);
			}
			if (DBG_ID == DBG_ID_CAM_RAWA) {
				print_subsys_reg(mmsys);
				print_subsys_reg(mdpsys);
				print_subsys_reg(camsys);
				print_subsys_reg(cam_rawa_sys);
			}
			if (DBG_ID == DBG_ID_CAM_RAWB) {
				print_subsys_reg(mmsys);
				print_subsys_reg(mdpsys);
				print_subsys_reg(camsys);
				print_subsys_reg(cam_rawb_sys);
			}
			if (DBG_ID == DBG_ID_CAM_RAWC) {
				print_subsys_reg(mmsys);
				print_subsys_reg(mdpsys);
				print_subsys_reg(camsys);
				print_subsys_reg(cam_rawc_sys);
			}
		}

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

	if (log_timeout)
		BUG_ON(1);
}

/* auto-gen begin, 0724 */
/* 0724 version */
int spm_mtcmos_ctrl_md1(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MD1;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MD1" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, MD1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) &
					MD1_PROT_STEP1_0_ACK_MASK) !=
					MD1_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_INFRA_VDNR_SET,
							MD1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_INFRA_VDNR_STA1) &
					MD1_PROT_STEP2_0_ACK_MASK) !=
					MD1_PROT_STEP2_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SUB_INFRA_VDNR_SET,
							MD1_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_SUB_INFRA_VDNR_STA1) &
					MD1_PROT_STEP2_1_ACK_MASK) !=
					MD1_PROT_STEP2_1_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) & ~PWR_ON);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MD1_PWR_STA_MASK = 0" */
		while (spm_read(PWR_STATUS) & MD1_PWR_STA_MASK) {
			ram_console_update();
			/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="MD_EXT_BUCK_ISO_CON[0]=1"*/
		spm_write(MD_EXT_BUCK_ISO_CON, spm_read(MD_EXT_BUCK_ISO_CON)
								| (0x1 << 0));
		/* TINFO="MD_EXT_BUCK_ISO_CON[1]=1"*/
		spm_write(MD_EXT_BUCK_ISO_CON, spm_read(MD_EXT_BUCK_ISO_CON)
								| (0x1 << 1));
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Finish to turn off MD1" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MD1" */
		/* TINFO="MD_EXT_BUCK_ISO_CON[0]=0"*/
		spm_write(MD_EXT_BUCK_ISO_CON, spm_read(MD_EXT_BUCK_ISO_CON)
								& ~(0x1 << 0));
		/* TINFO="MD_EXT_BUCK_ISO_CON[1]=0"*/
		spm_write(MD_EXT_BUCK_ISO_CON, spm_read(MD_EXT_BUCK_ISO_CON)
								& ~(0x1 << 1));
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) | PWR_RST_B);

		/* TINFO="Set PWR_ON = 1" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) | PWR_ON);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MD1_PWR_STA_MASK = 1" */
		while ((spm_read(PWR_STATUS) & MD1_PWR_STA_MASK) !=
							MD1_PWR_STA_MASK) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_INFRA_VDNR_CLR,
							MD1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SUB_INFRA_VDNR_CLR,
							MD1_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, MD1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
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
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off CONN" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, CONN_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) &
					CONN_PROT_STEP1_0_ACK_MASK) !=
					CONN_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, CONN_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) &
					CONN_PROT_STEP2_0_ACK_MASK) !=
					CONN_PROT_STEP2_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif

		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_SET, CONN_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2) &
					CONN_PROT_STEP2_1_ACK_MASK) !=
					CONN_PROT_STEP2_1_ACK_MASK) {
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
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
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
		while (((spm_read(PWR_STATUS) & CONN_PWR_STA_MASK) !=
							CONN_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & CONN_PWR_STA_MASK) !=
							CONN_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
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
		/* prot ack check after release protect is ignored */
#endif

		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_CLR, CONN_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, CONN_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Finish to turn on CONN" */
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
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG0" */
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MFG0_PWR_CON, spm_read(MFG0_PWR_CON) | MFG0_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG0_SRAM_PDN_ACK = 1" */
		while ((spm_read(MFG0_PWR_CON) & MFG0_SRAM_PDN_ACK) !=
							MFG0_SRAM_PDN_ACK) {
			ram_console_update();
				/* Need f_fmfg_core_ck for SRAM PDN delay IP. */
		}
		INCREASE_STEPS;
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
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
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
		while (((spm_read(PWR_STATUS) & MFG0_PWR_STA_MASK) !=
							MFG0_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & MFG0_PWR_STA_MASK) !=
							MFG0_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG0_PWR_CON, spm_read(MFG0_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG0_PWR_CON, spm_read(MFG0_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG0_PWR_CON, spm_read(MFG0_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MFG0_PWR_CON, spm_read(MFG0_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG0_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(MFG0_PWR_CON) & MFG0_SRAM_PDN_ACK_BIT0) {
			ram_console_update();
				/* Need f_fmfg_core_ck for SRAM PDN delay IP. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn on MFG0" */
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
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG1" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET, MFG1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1) &
					MFG1_PROT_STEP1_0_ACK_MASK) !=
					MFG1_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_SET, MFG1_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2) &
					MFG1_PROT_STEP1_1_ACK_MASK) !=
					MFG1_PROT_STEP1_1_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, MFG1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) &
					MFG1_PROT_STEP2_0_ACK_MASK) !=
					MFG1_PROT_STEP2_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_SET, MFG1_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2) &
					MFG1_PROT_STEP2_1_ACK_MASK) !=
					MFG1_PROT_STEP2_1_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 2" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SUB_INFRA_VDNR_SET,
							MFG1_PROT_STEP2_2_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_SUB_INFRA_VDNR_STA1) &
					MFG1_PROT_STEP2_2_ACK_MASK) !=
					MFG1_PROT_STEP2_2_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MFG1_PWR_CON, spm_read(MFG1_PWR_CON) | MFG1_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG1_SRAM_PDN_ACK = 1" */
		while ((spm_read(MFG1_PWR_CON) & MFG1_SRAM_PDN_ACK)
						!= MFG1_SRAM_PDN_ACK) {
			ram_console_update();
				/*  */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MFG1_PWR_CON, spm_read(MFG1_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MFG1_PWR_CON, spm_read(MFG1_PWR_CON) | PWR_CLK_DIS);
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
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off MFG1" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MFG1" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MFG1_PWR_CON, spm_read(MFG1_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MFG1_PWR_CON, spm_read(MFG1_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & MFG1_PWR_STA_MASK) !=
							MFG1_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & MFG1_PWR_STA_MASK) !=
							MFG1_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG1_PWR_CON, spm_read(MFG1_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG1_PWR_CON, spm_read(MFG1_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG1_PWR_CON, spm_read(MFG1_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MFG1_PWR_CON, spm_read(MFG1_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG1_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(MFG1_PWR_CON) & MFG1_SRAM_PDN_ACK_BIT0) {
			ram_console_update();
				/*  */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, MFG1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_CLR, MFG1_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step2 : 2" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SUB_INFRA_VDNR_CLR,
							MFG1_PROT_STEP2_2_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR, MFG1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_CLR, MFG1_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Finish to turn on MFG1" */
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
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG2" */
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MFG2_PWR_CON, spm_read(MFG2_PWR_CON) | MFG2_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG2_SRAM_PDN_ACK = 1" */
		while ((spm_read(MFG2_PWR_CON) & MFG2_SRAM_PDN_ACK)
						!= MFG2_SRAM_PDN_ACK) {
			ram_console_update();
				/*  */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MFG2_PWR_CON, spm_read(MFG2_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MFG2_PWR_CON, spm_read(MFG2_PWR_CON) | PWR_CLK_DIS);
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
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off MFG2" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MFG2" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MFG2_PWR_CON, spm_read(MFG2_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MFG2_PWR_CON, spm_read(MFG2_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & MFG2_PWR_STA_MASK) !=
							MFG2_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & MFG2_PWR_STA_MASK) !=
							MFG2_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG2_PWR_CON, spm_read(MFG2_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG2_PWR_CON, spm_read(MFG2_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG2_PWR_CON, spm_read(MFG2_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MFG2_PWR_CON, spm_read(MFG2_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG2_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(MFG2_PWR_CON) & MFG2_SRAM_PDN_ACK_BIT0) {
			ram_console_update();
				/*  */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn on MFG2" */
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
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG3" */
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MFG3_PWR_CON, spm_read(MFG3_PWR_CON) | MFG3_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG3_SRAM_PDN_ACK = 1" */
		while ((spm_read(MFG3_PWR_CON) & MFG3_SRAM_PDN_ACK) !=
							MFG3_SRAM_PDN_ACK) {
			ram_console_update();
				/*  */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MFG3_PWR_CON, spm_read(MFG3_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MFG3_PWR_CON, spm_read(MFG3_PWR_CON) | PWR_CLK_DIS);
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
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off MFG3" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MFG3" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MFG3_PWR_CON, spm_read(MFG3_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MFG3_PWR_CON, spm_read(MFG3_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & MFG3_PWR_STA_MASK) !=
							MFG3_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & MFG3_PWR_STA_MASK) !=
							MFG3_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG3_PWR_CON, spm_read(MFG3_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG3_PWR_CON, spm_read(MFG3_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG3_PWR_CON, spm_read(MFG3_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MFG3_PWR_CON, spm_read(MFG3_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG3_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(MFG3_PWR_CON) & MFG3_SRAM_PDN_ACK_BIT0) {
			ram_console_update();
				/*  */
		}
		INCREASE_STEPS;
#endif
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
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG4" */
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MFG4_PWR_CON, spm_read(MFG4_PWR_CON) | MFG4_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG4_SRAM_PDN_ACK = 1" */
		while ((spm_read(MFG4_PWR_CON) & MFG4_SRAM_PDN_ACK) !=
							MFG4_SRAM_PDN_ACK) {
			ram_console_update();
				/*  */
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
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
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
		while (((spm_read(PWR_STATUS) & MFG4_PWR_STA_MASK) !=
							MFG4_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & MFG4_PWR_STA_MASK) !=
							MFG4_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
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
			ram_console_update();
				/*  */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn on MFG4" */
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
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG5" */
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MFG5_PWR_CON, spm_read(MFG5_PWR_CON) | MFG5_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG5_SRAM_PDN_ACK = 1" */
		while ((spm_read(MFG5_PWR_CON) & MFG5_SRAM_PDN_ACK) !=
							MFG5_SRAM_PDN_ACK) {
			ram_console_update();
				/*  */
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
			ram_console_update();
			/* No logic between pwr_on and pwr_ack. */
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
		while (((spm_read(PWR_STATUS) & MFG5_PWR_STA_MASK) !=
							MFG5_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & MFG5_PWR_STA_MASK) !=
							MFG5_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
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
			ram_console_update();
				/*  */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn on MFG5" */
	}
	return err;
}

int spm_mtcmos_ctrl_mfg6(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG6;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG6" */
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MFG6_PWR_CON, spm_read(MFG6_PWR_CON) | MFG6_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG6_SRAM_PDN_ACK = 1" */
		while ((spm_read(MFG6_PWR_CON) & MFG6_SRAM_PDN_ACK) !=
							MFG6_SRAM_PDN_ACK) {
			ram_console_update();
				/* n/a */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MFG6_PWR_CON, spm_read(MFG6_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MFG6_PWR_CON, spm_read(MFG6_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MFG6_PWR_CON, spm_read(MFG6_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MFG6_PWR_CON, spm_read(MFG6_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MFG6_PWR_CON, spm_read(MFG6_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MFG6_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MFG6_PWR_STA_MASK)) {
			ram_console_update();
			/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off MFG6" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MFG6" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MFG6_PWR_CON, spm_read(MFG6_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MFG6_PWR_CON, spm_read(MFG6_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & MFG6_PWR_STA_MASK) !=
							MFG6_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & MFG6_PWR_STA_MASK) !=
							MFG6_PWR_STA_MASK)) {
			ram_console_update();
			/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG6_PWR_CON, spm_read(MFG6_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG6_PWR_CON, spm_read(MFG6_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG6_PWR_CON, spm_read(MFG6_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MFG6_PWR_CON, spm_read(MFG6_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG6_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(MFG6_PWR_CON) & MFG6_SRAM_PDN_ACK_BIT0) {
			ram_console_update();
				/* n/a */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn on MFG6" */
	}
	return err;
}


int spm_mtcmos_ctrl_isp(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_ISP;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off ISP" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_2_SET,
							ISP_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_2_STA1) &
					ISP_PROT_STEP1_0_ACK_MASK) !=
					ISP_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_2_SET,
							ISP_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_2_STA1) &
					ISP_PROT_STEP2_0_ACK_MASK) !=
					ISP_PROT_STEP2_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(ISP_PWR_CON, spm_read(ISP_PWR_CON) | ISP_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ISP_SRAM_PDN_ACK = 1" */
		while ((spm_read(ISP_PWR_CON) & ISP_SRAM_PDN_ACK) !=
							ISP_SRAM_PDN_ACK) {
			ram_console_update();
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
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
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
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
		while (((spm_read(PWR_STATUS) & ISP_PWR_STA_MASK) !=
							ISP_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & ISP_PWR_STA_MASK) !=
							ISP_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
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
			ram_console_update();
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_2_CLR,
							ISP_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_2_CLR,
							ISP_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Finish to turn on ISP" */
	}
	return err;
}

int spm_mtcmos_ctrl_isp2(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_ISP2;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off ISP2" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
							ISP2_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
					ISP2_PROT_STEP1_0_ACK_MASK) !=
					ISP2_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
							ISP2_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
					ISP2_PROT_STEP2_0_ACK_MASK) !=
					ISP2_PROT_STEP2_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(ISP2_PWR_CON, spm_read(ISP2_PWR_CON) | ISP2_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ISP2_SRAM_PDN_ACK = 1" */
		while ((spm_read(ISP2_PWR_CON) & ISP2_SRAM_PDN_ACK) !=
							ISP2_SRAM_PDN_ACK) {
			ram_console_update();
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(ISP2_PWR_CON, spm_read(ISP2_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(ISP2_PWR_CON, spm_read(ISP2_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(ISP2_PWR_CON, spm_read(ISP2_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(ISP2_PWR_CON, spm_read(ISP2_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(ISP2_PWR_CON, spm_read(ISP2_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & ISP2_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & ISP2_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off ISP2" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on ISP2" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(ISP2_PWR_CON, spm_read(ISP2_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(ISP2_PWR_CON, spm_read(ISP2_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & ISP2_PWR_STA_MASK) !=
							ISP2_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & ISP2_PWR_STA_MASK) !=
							ISP2_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(ISP2_PWR_CON, spm_read(ISP2_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(ISP2_PWR_CON, spm_read(ISP2_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(ISP2_PWR_CON, spm_read(ISP2_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(ISP2_PWR_CON, spm_read(ISP2_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ISP2_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(ISP2_PWR_CON) & ISP2_SRAM_PDN_ACK_BIT0) {
			ram_console_update();
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
							ISP2_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
							ISP2_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Finish to turn on ISP2" */
	}
	return err;
}

int spm_mtcmos_ctrl_ipe(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_IPE;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off IPE" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET, IPE_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
					IPE_PROT_STEP1_0_ACK_MASK) !=
					IPE_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET, IPE_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
					IPE_PROT_STEP2_0_ACK_MASK) !=
					IPE_PROT_STEP2_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(IPE_PWR_CON, spm_read(IPE_PWR_CON) | IPE_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until IPE_SRAM_PDN_ACK = 1" */
		while ((spm_read(IPE_PWR_CON) & IPE_SRAM_PDN_ACK) !=
							IPE_SRAM_PDN_ACK) {
			ram_console_update();
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(IPE_PWR_CON, spm_read(IPE_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(IPE_PWR_CON, spm_read(IPE_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(IPE_PWR_CON, spm_read(IPE_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(IPE_PWR_CON, spm_read(IPE_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(IPE_PWR_CON, spm_read(IPE_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & IPE_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & IPE_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off IPE" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on IPE" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(IPE_PWR_CON, spm_read(IPE_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(IPE_PWR_CON, spm_read(IPE_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & IPE_PWR_STA_MASK) !=
							IPE_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & IPE_PWR_STA_MASK) !=
							IPE_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(IPE_PWR_CON, spm_read(IPE_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(IPE_PWR_CON, spm_read(IPE_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(IPE_PWR_CON, spm_read(IPE_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(IPE_PWR_CON, spm_read(IPE_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until IPE_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(IPE_PWR_CON) & IPE_SRAM_PDN_ACK_BIT0) {
			ram_console_update();
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR, IPE_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR, IPE_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Finish to turn on IPE" */
	}
	return err;
}

int spm_mtcmos_ctrl_vde(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_VDE;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VDE" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET, VDE_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
					VDE_PROT_STEP1_0_ACK_MASK) !=
					VDE_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET, VDE_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
					VDE_PROT_STEP2_0_ACK_MASK) !=
					VDE_PROT_STEP2_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(VDE_PWR_CON, spm_read(VDE_PWR_CON) | VDE_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VDE_SRAM_PDN_ACK = 1" */
		while ((spm_read(VDE_PWR_CON) & VDE_SRAM_PDN_ACK) !=
							VDE_SRAM_PDN_ACK) {
			ram_console_update();
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
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
				/* No logic between pwr_on and pwr_ack. */
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
		while (((spm_read(PWR_STATUS) & VDE_PWR_STA_MASK) !=
							VDE_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & VDE_PWR_STA_MASK) !=
							VDE_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
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
			ram_console_update();
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR, VDE_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR, VDE_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Finish to turn on VDE" */
	}
	return err;
}

int spm_mtcmos_ctrl_vde2(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_VDE2;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VDE2" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_2_SET,
							VDE2_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_2_STA1) &
					VDE2_PROT_STEP1_0_ACK_MASK) !=
					VDE2_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_2_SET,
							VDE2_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_2_STA1) &
					VDE2_PROT_STEP2_0_ACK_MASK) !=
					VDE2_PROT_STEP2_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(VDE2_PWR_CON, spm_read(VDE2_PWR_CON) | VDE2_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VDE2_SRAM_PDN_ACK = 1" */
		while ((spm_read(VDE2_PWR_CON) & VDE2_SRAM_PDN_ACK) !=
							VDE2_SRAM_PDN_ACK) {
			ram_console_update();
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(VDE2_PWR_CON, spm_read(VDE2_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(VDE2_PWR_CON, spm_read(VDE2_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(VDE2_PWR_CON, spm_read(VDE2_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(VDE2_PWR_CON, spm_read(VDE2_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(VDE2_PWR_CON, spm_read(VDE2_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & VDE2_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & VDE2_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off VDE2" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on VDE2" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(VDE2_PWR_CON, spm_read(VDE2_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(VDE2_PWR_CON, spm_read(VDE2_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & VDE2_PWR_STA_MASK) !=
							VDE2_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & VDE2_PWR_STA_MASK) !=
							VDE2_PWR_STA_MASK)) {
			ram_console_update();
			/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(VDE2_PWR_CON, spm_read(VDE2_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(VDE2_PWR_CON, spm_read(VDE2_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(VDE2_PWR_CON, spm_read(VDE2_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(VDE2_PWR_CON, spm_read(VDE2_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VDE2_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(VDE2_PWR_CON) & VDE2_SRAM_PDN_ACK_BIT0) {
			ram_console_update();
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_2_CLR,
							VDE2_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_2_CLR,
							VDE2_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Finish to turn on VDE2" */
	}
	return err;
}

int spm_mtcmos_ctrl_ven(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_VEN;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VEN" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET, VEN_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
					VEN_PROT_STEP1_0_ACK_MASK) !=
					VEN_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_2_SET,
							VEN_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_2_STA1) &
					VEN_PROT_STEP1_1_ACK_MASK) !=
					VEN_PROT_STEP1_1_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET, VEN_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
					VEN_PROT_STEP2_0_ACK_MASK) !=
					VEN_PROT_STEP2_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_2_SET,
							VEN_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_2_STA1) &
					VEN_PROT_STEP2_1_ACK_MASK) !=
					VEN_PROT_STEP2_1_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(VEN_PWR_CON, spm_read(VEN_PWR_CON) | VEN_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VEN_SRAM_PDN_ACK = 1" */
		while ((spm_read(VEN_PWR_CON) & VEN_SRAM_PDN_ACK) !=
							VEN_SRAM_PDN_ACK) {
			ram_console_update();
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
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
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
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
		while (((spm_read(PWR_STATUS) & VEN_PWR_STA_MASK) !=
							VEN_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & VEN_PWR_STA_MASK) !=
							VEN_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
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
			ram_console_update();
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR, VEN_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_2_CLR,
							VEN_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR, VEN_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_2_CLR,
							VEN_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Finish to turn on VEN" */
	}
	return err;
}

int spm_mtcmos_ctrl_ven_core1(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_VEN_CORE1;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VEN_CORE1" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
						VEN_CORE1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
					VEN_CORE1_PROT_STEP1_0_ACK_MASK) !=
					VEN_CORE1_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
						VEN_CORE1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
					VEN_CORE1_PROT_STEP2_0_ACK_MASK) !=
					VEN_CORE1_PROT_STEP2_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(VEN_CORE1_PWR_CON, spm_read(VEN_CORE1_PWR_CON) |
							VEN_CORE1_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VEN_CORE1_SRAM_PDN_ACK = 1" */
		while ((spm_read(VEN_CORE1_PWR_CON) & VEN_CORE1_SRAM_PDN_ACK)
						!= VEN_CORE1_SRAM_PDN_ACK) {
			ram_console_update();
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(VEN_CORE1_PWR_CON, spm_read(VEN_CORE1_PWR_CON) |
								PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(VEN_CORE1_PWR_CON, spm_read(VEN_CORE1_PWR_CON) |
								PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(VEN_CORE1_PWR_CON, spm_read(VEN_CORE1_PWR_CON) &
								~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(VEN_CORE1_PWR_CON, spm_read(VEN_CORE1_PWR_CON) &
								~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(VEN_CORE1_PWR_CON, spm_read(VEN_CORE1_PWR_CON) &
								~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & VEN_CORE1_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & VEN_CORE1_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off VEN_CORE1" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on VEN_CORE1" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(VEN_CORE1_PWR_CON, spm_read(VEN_CORE1_PWR_CON) |
									PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(VEN_CORE1_PWR_CON, spm_read(VEN_CORE1_PWR_CON) |
								PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & VEN_CORE1_PWR_STA_MASK) !=
						VEN_CORE1_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & VEN_CORE1_PWR_STA_MASK)
						!= VEN_CORE1_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(VEN_CORE1_PWR_CON, spm_read(VEN_CORE1_PWR_CON) &
								~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(VEN_CORE1_PWR_CON, spm_read(VEN_CORE1_PWR_CON) &
								~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(VEN_CORE1_PWR_CON, spm_read(VEN_CORE1_PWR_CON) |
								PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(VEN_CORE1_PWR_CON, spm_read(VEN_CORE1_PWR_CON) &
								~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VEN_CORE1_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(VEN_CORE1_PWR_CON) &
						VEN_CORE1_SRAM_PDN_ACK_BIT0) {
			ram_console_update();
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
						VEN_CORE1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
						VEN_CORE1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Finish to turn on VEN_CORE1" */
	}
	return err;
}

int spm_mtcmos_ctrl_mdp(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MDP;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MDP" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, MDP_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) &
					MDP_PROT_STEP1_0_ACK_MASK) !=
					MDP_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET, MDP_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
					MDP_PROT_STEP1_1_ACK_MASK) !=
					MDP_PROT_STEP1_1_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step1 : 2" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_2_SET,
							MDP_PROT_STEP1_2_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_2_STA1) &
					MDP_PROT_STEP1_2_ACK_MASK) !=
					MDP_PROT_STEP1_2_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, MDP_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) &
					MDP_PROT_STEP2_0_ACK_MASK) !=
					MDP_PROT_STEP2_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET, MDP_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
					MDP_PROT_STEP2_1_ACK_MASK) !=
					MDP_PROT_STEP2_1_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 2" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_2_SET,
							MDP_PROT_STEP2_2_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_2_STA1) &
					MDP_PROT_STEP2_2_ACK_MASK) !=
					MDP_PROT_STEP2_2_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 3" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SUB_INFRA_VDNR_SET,
							MDP_PROT_STEP2_3_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_SUB_INFRA_VDNR_STA1) &
					MDP_PROT_STEP2_3_ACK_MASK) !=
					MDP_PROT_STEP2_3_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MDP_PWR_CON, spm_read(MDP_PWR_CON) | MDP_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MDP_SRAM_PDN_ACK = 1" */
		while ((spm_read(MDP_PWR_CON) & MDP_SRAM_PDN_ACK) !=
							MDP_SRAM_PDN_ACK) {
			ram_console_update();
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MDP_PWR_CON, spm_read(MDP_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MDP_PWR_CON, spm_read(MDP_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MDP_PWR_CON, spm_read(MDP_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MDP_PWR_CON, spm_read(MDP_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MDP_PWR_CON, spm_read(MDP_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MDP_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MDP_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off MDP" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MDP" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MDP_PWR_CON, spm_read(MDP_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MDP_PWR_CON, spm_read(MDP_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & MDP_PWR_STA_MASK) !=
							MDP_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & MDP_PWR_STA_MASK) !=
							MDP_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MDP_PWR_CON, spm_read(MDP_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MDP_PWR_CON, spm_read(MDP_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MDP_PWR_CON, spm_read(MDP_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MDP_PWR_CON, spm_read(MDP_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MDP_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(MDP_PWR_CON) & MDP_SRAM_PDN_ACK_BIT0) {
			ram_console_update();
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, MDP_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR, MDP_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step2 : 2" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_2_CLR,
							MDP_PROT_STEP2_2_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step2 : 3" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SUB_INFRA_VDNR_CLR,
							MDP_PROT_STEP2_3_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, MDP_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR, MDP_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step1 : 2" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_2_CLR,
							MDP_PROT_STEP1_2_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Finish to turn on MDP" */
	}
	return err;
}

int spm_mtcmos_ctrl_dis(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_DIS;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off DIS" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET, DIS_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
					DIS_PROT_STEP1_0_ACK_MASK) !=
					DIS_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, DIS_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) &
					DIS_PROT_STEP2_0_ACK_MASK) !=
					DIS_PROT_STEP2_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET, DIS_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
					DIS_PROT_STEP2_1_ACK_MASK) !=
					DIS_PROT_STEP2_1_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 2" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SUB_INFRA_VDNR_SET,
							DIS_PROT_STEP2_2_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_SUB_INFRA_VDNR_STA1) &
					DIS_PROT_STEP2_2_ACK_MASK) !=
					DIS_PROT_STEP2_2_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(DIS_PWR_CON, spm_read(DIS_PWR_CON) | DIS_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DIS_SRAM_PDN_ACK = 1" */
		while ((spm_read(DIS_PWR_CON) & DIS_SRAM_PDN_ACK) !=
							DIS_SRAM_PDN_ACK) {
			ram_console_update();
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
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
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
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
		while (((spm_read(PWR_STATUS) & DIS_PWR_STA_MASK) !=
							DIS_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & DIS_PWR_STA_MASK) !=
							DIS_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
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
			ram_console_update();
				/* Need hf_fmm_ck for SRAM PDN delay IP. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, DIS_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR, DIS_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step2 : 2" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SUB_INFRA_VDNR_CLR,
							DIS_PROT_STEP2_2_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR, DIS_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Finish to turn on DIS" */
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
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off AUDIO" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_SET,
						AUDIO_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2) &
					AUDIO_PROT_STEP1_0_ACK_MASK) !=
					AUDIO_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) |
								AUDIO_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until AUDIO_SRAM_PDN_ACK = 1" */
		while ((spm_read(AUDIO_PWR_CON) & AUDIO_SRAM_PDN_ACK) !=
							AUDIO_SRAM_PDN_ACK) {
			ram_console_update();
				/* Need f_f26M_aud_ck for SRAM PDN delay IP. */
		}
		INCREASE_STEPS;
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
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
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
		while (((spm_read(PWR_STATUS) & AUDIO_PWR_STA_MASK) !=
							AUDIO_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & AUDIO_PWR_STA_MASK) !=
							AUDIO_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) &
								~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until AUDIO_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(AUDIO_PWR_CON) & AUDIO_SRAM_PDN_ACK_BIT0) {
			ram_console_update();
				/* Need f_f26M_aud_ck for SRAM PDN delay IP. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_CLR,
						AUDIO_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Finish to turn on AUDIO" */
	}
	return err;
}

int spm_mtcmos_ctrl_adsp_dormant(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_ADSP;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off ADSP" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_SET, ADSP_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2) &
						ADSP_PROT_STEP1_0_ACK_MASK) !=
						ADSP_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_CKISO = 1" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) | SRAM_CKISO);
		/* TINFO="Set SRAM_ISOINT_B = 0" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) &
								~SRAM_ISOINT_B);
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_SLEEP_B = 0" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) &
							~ADSP_SRAM_SLEEP_B);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ADSP_SRAM_SLEEP_B_ACK = 0" */
		while (spm_read(ADSP_PWR_CON) & ADSP_SRAM_SLEEP_B_ACK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) & ~PWR_ON_2ND);
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
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & ADSP_PWR_STA_MASK) !=
							ADSP_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & ADSP_PWR_STA_MASK) !=
							ADSP_PWR_STA_MASK)) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_SLEEP_B = 1" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) |
							ADSP_SRAM_SLEEP_B);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ADSP_SRAM_SLEEP_B_ACK = 1" */
		while ((spm_read(ADSP_PWR_CON) & ADSP_SRAM_SLEEP_B_ACK) !=
							ADSP_SRAM_SLEEP_B_ACK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_ISOINT_B = 1" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) | SRAM_ISOINT_B);
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_CKISO = 0" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) & ~SRAM_CKISO);
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_CLR, ADSP_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK

#endif
		/* TINFO="Finish to turn on ADSP" */
	}
	return err;
}


int spm_mtcmos_ctrl_adsp_shut_down(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_ADSP;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off ADSP" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_SET, ADSP_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2) &
					ADSP_PROT_STEP1_0_ACK_MASK) !=
					ADSP_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_CKISO = 1" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) | SRAM_CKISO);
		/* TINFO="Set SRAM_ISOINT_B = 0" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) &
								~SRAM_ISOINT_B);
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) | ADSP_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ADSP_SRAM_PDN_ACK = 1" */
		while ((spm_read(ADSP_PWR_CON) & ADSP_SRAM_PDN_ACK) !=
							ADSP_SRAM_PDN_ACK) {
			ram_console_update();
				/* Need f_f26M_aud_ck for SRAM PDN delay IP. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & ADSP_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & ADSP_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off ADSP" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on ADSP" */
		/* TINFO="Set SRAM_ISOINT_B = 1" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) | SRAM_ISOINT_B);
		/* TINFO="Set PWR_ON = 1" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & ADSP_PWR_STA_MASK) !=
							ADSP_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & ADSP_PWR_STA_MASK) !=
							ADSP_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ADSP_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(ADSP_PWR_CON) & ADSP_SRAM_PDN_ACK_BIT0) {
			ram_console_update();
				/* Need f_f26M_aud_ck for SRAM PDN delay IP. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Delay 1us" */
		udelay(1);
		/* TINFO="Set SRAM_CKISO = 0" */
		spm_write(ADSP_PWR_CON, spm_read(ADSP_PWR_CON) & ~SRAM_CKISO);
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_CLR, ADSP_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Finish to turn on ADSP" */
	}
	return err;
}

int spm_mtcmos_ctrl_cam(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_CAM;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off CAM" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_SET, CAM_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2) &
					CAM_PROT_STEP1_0_ACK_MASK) !=
					CAM_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET, CAM_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
					CAM_PROT_STEP1_1_ACK_MASK) !=
					CAM_PROT_STEP1_1_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET, CAM_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1) &
					CAM_PROT_STEP2_0_ACK_MASK) !=
					CAM_PROT_STEP2_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET, CAM_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
					CAM_PROT_STEP2_1_ACK_MASK) !=
					CAM_PROT_STEP2_1_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(CAM_PWR_CON, spm_read(CAM_PWR_CON) | CAM_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until CAM_SRAM_PDN_ACK = 1" */
		while ((spm_read(CAM_PWR_CON) & CAM_SRAM_PDN_ACK) !=
							CAM_SRAM_PDN_ACK) {
			ram_console_update();
				/*  */
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
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
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
		while (((spm_read(PWR_STATUS) & CAM_PWR_STA_MASK) !=
							CAM_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & CAM_PWR_STA_MASK) !=
							CAM_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
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
			ram_console_update();
				/*  */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR, CAM_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR, CAM_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_CLR, CAM_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR, CAM_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Finish to turn on CAM" */
	}
	return err;
}

int spm_mtcmos_ctrl_cam_rawa(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_CAM_RAWA;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off CAM_RAWA" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
						CAM_RAWA_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
					CAM_RAWA_PROT_STEP1_0_ACK_MASK) !=
					CAM_RAWA_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
						CAM_RAWA_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
					CAM_RAWA_PROT_STEP2_0_ACK_MASK) !=
					CAM_RAWA_PROT_STEP2_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(CAM_RAWA_PWR_CON, spm_read(CAM_RAWA_PWR_CON) |
							CAM_RAWA_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until CAM_RAWA_SRAM_PDN_ACK = 1" */
		while ((spm_read(CAM_RAWA_PWR_CON) & CAM_RAWA_SRAM_PDN_ACK) !=
						CAM_RAWA_SRAM_PDN_ACK) {
			ram_console_update();
				/*  */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(CAM_RAWA_PWR_CON, spm_read(CAM_RAWA_PWR_CON) |
								PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(CAM_RAWA_PWR_CON, spm_read(CAM_RAWA_PWR_CON) |
								PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(CAM_RAWA_PWR_CON, spm_read(CAM_RAWA_PWR_CON) &
								~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(CAM_RAWA_PWR_CON, spm_read(CAM_RAWA_PWR_CON) &
								~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(CAM_RAWA_PWR_CON, spm_read(CAM_RAWA_PWR_CON) &
								~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & CAM_RAWA_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & CAM_RAWA_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off CAM_RAWA" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on CAM_RAWA" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(CAM_RAWA_PWR_CON, spm_read(CAM_RAWA_PWR_CON) |
								PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(CAM_RAWA_PWR_CON, spm_read(CAM_RAWA_PWR_CON) |
								PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & CAM_RAWA_PWR_STA_MASK) !=
						CAM_RAWA_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & CAM_RAWA_PWR_STA_MASK) !=
						CAM_RAWA_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(CAM_RAWA_PWR_CON, spm_read(CAM_RAWA_PWR_CON) &
								~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(CAM_RAWA_PWR_CON, spm_read(CAM_RAWA_PWR_CON) &
								~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(CAM_RAWA_PWR_CON, spm_read(CAM_RAWA_PWR_CON) |
								PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(CAM_RAWA_PWR_CON, spm_read(CAM_RAWA_PWR_CON) &
								~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until CAM_RAWA_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(CAM_RAWA_PWR_CON) &
						CAM_RAWA_SRAM_PDN_ACK_BIT0) {
			ram_console_update();
				/*  */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
						CAM_RAWA_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
						CAM_RAWA_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Finish to turn on CAM_RAWA" */
	}
	return err;
}

int spm_mtcmos_ctrl_cam_rawb(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_CAM_RAWB;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off CAM_RAWB" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
						CAM_RAWB_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
					CAM_RAWB_PROT_STEP1_0_ACK_MASK) !=
					CAM_RAWB_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
						CAM_RAWB_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
					CAM_RAWB_PROT_STEP2_0_ACK_MASK) !=
					CAM_RAWB_PROT_STEP2_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(CAM_RAWB_PWR_CON, spm_read(CAM_RAWB_PWR_CON) |
							CAM_RAWB_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until CAM_RAWB_SRAM_PDN_ACK = 1" */
		while ((spm_read(CAM_RAWB_PWR_CON) & CAM_RAWB_SRAM_PDN_ACK) !=
							CAM_RAWB_SRAM_PDN_ACK) {
			ram_console_update();
				/*  */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(CAM_RAWB_PWR_CON, spm_read(CAM_RAWB_PWR_CON) |
								PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(CAM_RAWB_PWR_CON, spm_read(CAM_RAWB_PWR_CON) |
								PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(CAM_RAWB_PWR_CON, spm_read(CAM_RAWB_PWR_CON) &
								~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(CAM_RAWB_PWR_CON, spm_read(CAM_RAWB_PWR_CON) &
								~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(CAM_RAWB_PWR_CON, spm_read(CAM_RAWB_PWR_CON) &
								~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & CAM_RAWB_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & CAM_RAWB_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off CAM_RAWB" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on CAM_RAWB" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(CAM_RAWB_PWR_CON, spm_read(CAM_RAWB_PWR_CON) |
								PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(CAM_RAWB_PWR_CON, spm_read(CAM_RAWB_PWR_CON) |
								PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & CAM_RAWB_PWR_STA_MASK) !=
							CAM_RAWB_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & CAM_RAWB_PWR_STA_MASK) !=
						CAM_RAWB_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(CAM_RAWB_PWR_CON, spm_read(CAM_RAWB_PWR_CON) &
								~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(CAM_RAWB_PWR_CON, spm_read(CAM_RAWB_PWR_CON) &
								~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(CAM_RAWB_PWR_CON, spm_read(CAM_RAWB_PWR_CON) |
								PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(CAM_RAWB_PWR_CON, spm_read(CAM_RAWB_PWR_CON) &
								~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until CAM_RAWB_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(CAM_RAWB_PWR_CON) &
						CAM_RAWB_SRAM_PDN_ACK_BIT0) {
			ram_console_update();
				/*  */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
						CAM_RAWB_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
						CAM_RAWB_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Finish to turn on CAM_RAWB" */
	}
	return err;
}

int spm_mtcmos_ctrl_cam_rawc(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_CAM_RAWC;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off CAM_RAWC" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
						CAM_RAWC_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
					CAM_RAWC_PROT_STEP1_0_ACK_MASK) !=
					CAM_RAWC_PROT_STEP1_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_SET,
						CAM_RAWC_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_MM_STA1) &
					CAM_RAWC_PROT_STEP2_0_ACK_MASK) !=
					CAM_RAWC_PROT_STEP2_0_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(CAM_RAWC_PWR_CON, spm_read(CAM_RAWC_PWR_CON) |
							CAM_RAWC_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until CAM_RAWC_SRAM_PDN_ACK = 1" */
		while ((spm_read(CAM_RAWC_PWR_CON) & CAM_RAWC_SRAM_PDN_ACK) !=
							CAM_RAWC_SRAM_PDN_ACK) {
			ram_console_update();
				/*  */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(CAM_RAWC_PWR_CON, spm_read(CAM_RAWC_PWR_CON) |
								PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(CAM_RAWC_PWR_CON, spm_read(CAM_RAWC_PWR_CON) |
								PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(CAM_RAWC_PWR_CON, spm_read(CAM_RAWC_PWR_CON) &
								~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(CAM_RAWC_PWR_CON, spm_read(CAM_RAWC_PWR_CON) &
								~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(CAM_RAWC_PWR_CON, spm_read(CAM_RAWC_PWR_CON) &
								~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & CAM_RAWC_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & CAM_RAWC_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off CAM_RAWC" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on CAM_RAWC" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(CAM_RAWC_PWR_CON, spm_read(CAM_RAWC_PWR_CON) |
								PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(CAM_RAWC_PWR_CON, spm_read(CAM_RAWC_PWR_CON) |
								PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & CAM_RAWC_PWR_STA_MASK) !=
							CAM_RAWC_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & CAM_RAWC_PWR_STA_MASK) !=
						CAM_RAWC_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(CAM_RAWC_PWR_CON, spm_read(CAM_RAWC_PWR_CON) &
								~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(CAM_RAWC_PWR_CON, spm_read(CAM_RAWC_PWR_CON) &
								~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(CAM_RAWC_PWR_CON, spm_read(CAM_RAWC_PWR_CON) |
								PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(CAM_RAWC_PWR_CON, spm_read(CAM_RAWC_PWR_CON) &
								~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until CAM_RAWC_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(CAM_RAWC_PWR_CON) &
						CAM_RAWC_SRAM_PDN_ACK_BIT0) {
			ram_console_update();
				/*  */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
						CAM_RAWC_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_MM_CLR,
						CAM_RAWC_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* prot ack check after release protect is ignored */
#endif
		/* TINFO="Finish to turn on CAM_RAWC" */
	}
	return err;
}

int spm_mtcmos_ctrl_dp_tx(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_DP_TX;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off DP_TX" */
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(DP_TX_PWR_CON, spm_read(DP_TX_PWR_CON) |
								DP_TX_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DP_TX_SRAM_PDN_ACK = 1" */
		while ((spm_read(DP_TX_PWR_CON) & DP_TX_SRAM_PDN_ACK) !=
							DP_TX_SRAM_PDN_ACK) {
			ram_console_update();
				/*  */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(DP_TX_PWR_CON, spm_read(DP_TX_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(DP_TX_PWR_CON, spm_read(DP_TX_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(DP_TX_PWR_CON, spm_read(DP_TX_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(DP_TX_PWR_CON, spm_read(DP_TX_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(DP_TX_PWR_CON, spm_read(DP_TX_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & DP_TX_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & DP_TX_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off DP_TX" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on DP_TX" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(DP_TX_PWR_CON, spm_read(DP_TX_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(DP_TX_PWR_CON, spm_read(DP_TX_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & DP_TX_PWR_STA_MASK) !=
							DP_TX_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & DP_TX_PWR_STA_MASK) !=
							DP_TX_PWR_STA_MASK)) {
			ram_console_update();
				/* No logic between pwr_on and pwr_ack. */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(DP_TX_PWR_CON, spm_read(DP_TX_PWR_CON) &
								~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(DP_TX_PWR_CON, spm_read(DP_TX_PWR_CON) &
								~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(DP_TX_PWR_CON, spm_read(DP_TX_PWR_CON) |
								PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(DP_TX_PWR_CON, spm_read(DP_TX_PWR_CON) &
								~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DP_TX_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(DP_TX_PWR_CON) & DP_TX_SRAM_PDN_ACK_BIT0) {
			ram_console_update();
				/*  */
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn on DP_TX" */
	}
	return err;
}

int spm_mtcmos_vpu(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_VPU;
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
							(0x1UL << 5)) {
			ram_console_update();
		}
		INCREASE_STEPS;
	}

	return err;
}

/* auto-gen end*/

/* enable op*/
static int MD1_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_md1(STA_POWER_ON);
}
static int CONN_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_conn(STA_POWER_ON);
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
static int MFG5_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_mfg5(STA_POWER_ON);
}
static int MFG6_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_mfg6(STA_POWER_ON);
}
static int ISP_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_isp(STA_POWER_ON);
}
static int ISP2_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_isp2(STA_POWER_ON);
}
static int IPE_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_ipe(STA_POWER_ON);
}
static int VDE_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vde(STA_POWER_ON);
}
static int VDE2_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vde2(STA_POWER_ON);
}
static int VEN_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_ven(STA_POWER_ON);
}
static int VEN_CORE1_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_ven_core1(STA_POWER_ON);
}
static int MDP_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_mdp(STA_POWER_ON);
}
static int DIS_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_dis(STA_POWER_ON);
}
static int AUDIO_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_audio(STA_POWER_ON);
}
static int ADSP_sys_enable_op(struct subsys *sys)
{
	/* return spm_mtcmos_ctrl_adsp_shut_down(STA_POWER_ON); */
	/* MT6885: For ADSP, only enter dormant flow */
	return spm_mtcmos_ctrl_adsp_dormant(STA_POWER_ON);
}
static int CAM_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_cam(STA_POWER_ON);
}
static int CAM_RAWA_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_cam_rawa(STA_POWER_ON);
}
static int CAM_RAWB_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_cam_rawb(STA_POWER_ON);
}
static int CAM_RAWC_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_cam_rawc(STA_POWER_ON);
}
static int DP_TX_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_dp_tx(STA_POWER_ON);
}
static int VPU_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_vpu(STA_POWER_ON);
}

/* disable op */
static int MD1_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_md1(STA_POWER_DOWN);
}
static int CONN_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_conn(STA_POWER_DOWN);
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
static int MFG5_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_mfg5(STA_POWER_DOWN);
}
static int MFG6_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_mfg6(STA_POWER_DOWN);
}
static int ISP_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_isp(STA_POWER_DOWN);
}
static int ISP2_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_isp2(STA_POWER_DOWN);
}
static int IPE_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_ipe(STA_POWER_DOWN);
}
static int VDE_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vde(STA_POWER_DOWN);
}
static int VDE2_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_vde2(STA_POWER_DOWN);
}
static int VEN_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_ven(STA_POWER_DOWN);
}
static int VEN_CORE1_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_ven_core1(STA_POWER_DOWN);
}
static int MDP_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_mdp(STA_POWER_DOWN);
}
static int DIS_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_dis(STA_POWER_DOWN);
}
static int AUDIO_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_audio(STA_POWER_DOWN);
}
static int ADSP_sys_disable_op(struct subsys *sys)
{
	/* return spm_mtcmos_ctrl_adsp_shut_down(STA_POWER_DOWN); */
	/* MT6885: For ADSP, only enter dormant flow */
	return spm_mtcmos_ctrl_adsp_dormant(STA_POWER_DOWN);
}
static int CAM_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_cam(STA_POWER_DOWN);
}
static int CAM_RAWA_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_cam_rawa(STA_POWER_DOWN);
}
static int CAM_RAWB_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_cam_rawb(STA_POWER_DOWN);
}
static int CAM_RAWC_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_cam_rawc(STA_POWER_DOWN);
}
static int DP_TX_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_dp_tx(STA_POWER_DOWN);
}
static int VPU_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_vpu(STA_POWER_DOWN);
}

static int sys_get_state_op(struct subsys *sys)
{
	unsigned int sta = clk_readl(PWR_STATUS);
	unsigned int sta_s = clk_readl(PWR_STATUS_2ND);

	return (sta & sys->sta_mask) && (sta_s & sys->sta_mask);
}

static int vpu_get_state_op(struct subsys *sys)
{
	unsigned int sta = clk_readl(OTHER_PWR_STATUS);

	return (sta & sys->sta_mask);
}

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
static struct subsys_ops MFG5_sys_ops = {
	.enable = MFG5_sys_enable_op,
	.disable = MFG5_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops MFG6_sys_ops = {
	.enable = MFG6_sys_enable_op,
	.disable = MFG6_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops ISP_sys_ops = {
	.enable = ISP_sys_enable_op,
	.disable = ISP_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops ISP2_sys_ops = {
	.enable = ISP2_sys_enable_op,
	.disable = ISP2_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops IPE_sys_ops = {
	.enable = IPE_sys_enable_op,
	.disable = IPE_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops VDE_sys_ops = {
	.enable = VDE_sys_enable_op,
	.disable = VDE_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops VDE2_sys_ops = {
	.enable = VDE2_sys_enable_op,
	.disable = VDE2_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops VEN_sys_ops = {
	.enable = VEN_sys_enable_op,
	.disable = VEN_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops VEN_CORE1_sys_ops = {
	.enable = VEN_CORE1_sys_enable_op,
	.disable = VEN_CORE1_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops MDP_sys_ops = {
	.enable = MDP_sys_enable_op,
	.disable = MDP_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops DIS_sys_ops = {
	.enable = DIS_sys_enable_op,
	.disable = DIS_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops AUDIO_sys_ops = {
	.enable = AUDIO_sys_enable_op,
	.disable = AUDIO_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops ADSP_sys_ops = {
	.enable = ADSP_sys_enable_op,
	.disable = ADSP_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops CAM_sys_ops = {
	.enable = CAM_sys_enable_op,
	.disable = CAM_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops CAM_RAWA_sys_ops = {
	.enable = CAM_RAWA_sys_enable_op,
	.disable = CAM_RAWA_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops CAM_RAWB_sys_ops = {
	.enable = CAM_RAWB_sys_enable_op,
	.disable = CAM_RAWB_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops CAM_RAWC_sys_ops = {
	.enable = CAM_RAWC_sys_enable_op,
	.disable = CAM_RAWC_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops DP_TX_sys_ops = {
	.enable = DP_TX_sys_enable_op,
	.disable = DP_TX_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops VPU_sys_ops = {
	.enable = VPU_sys_enable_op,
	.disable = VPU_sys_disable_op,
	.get_state = vpu_get_state_op,
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
1,	/* SYS_MD1 = 0 */
1,	/* SYS_CONN = 1 */
1,	/* SYS_MFG0 = 2 */
1,	/* SYS_MFG1 = 3 */
1,	/* SYS_MFG2 = 4 */
1,	/* SYS_MFG3 = 5 */
1,	/* SYS_MFG4 = 6 */
1,	/* SYS_MFG5 = 7 */
1,	/* SYS_MFG6 = 8 */
1,	/* SYS_ISP = 9 */
1,	/* SYS_ISP2 = 10 */
1,	/* SYS_IPE = 11 */
1,	/* SYS_VDE = 12 */
1,	/* SYS_VDE2 = 13 */
1,	/* SYS_VEN = 14 */
1,	/* SYS_VEN_CORE1 = 15 */
1,	/* SYS_MDP = 16 */
1,	/* SYS_DIS = 17 */
1,	/* SYS_AUDIO = 18 */
1,	/* SYS_ADSP = 19 */
1,	/* SYS_CAM = 20 */
1,	/* SYS_CAM_RAWA = 21 */
1,	/* SYS_CAM_RAWB = 22 */
1,	/* SYS_CAM_RAWC = 23 */
1,	/* SYS_DP_TX = 24 */
1,	/* SYS_VPU = 25 */
};
#endif

static int isNeedMfgFakePowerOn(enum subsys_id id)
{
	int isGpuDfdTriggered = 0;
	unsigned int gpu_dfd_status;

	if (id == SYS_MFG0 || id == SYS_MFG1 || id == SYS_MFG2 ||
	    id == SYS_MFG3 || id == SYS_MFG4 || id == SYS_MFG5 ||
	    id == SYS_MFG6) {
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

static int enable_subsys(enum subsys_id id)
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

#if MT_CCF_BRINGUP
	pr_debug("[CCF] %s: sys=%s, id=%d\n", __func__, sys->name, id);
	if (sys->ops->get_state(sys) == SUBSYS_PWR_DOWN) {
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
	if (sys->ops->get_state(sys) == SUBSYS_PWR_ON &&
		!isNeedMfgFakePowerOn(id)) {
		mtk_clk_unlock(flags);
		return 0;
	}
#endif				/* CHECK_PWR_ST */

	r = sys->ops->enable(sys);
	WARN_ON(r);

	/* for MT6885 preclks CGs. */
	enable_subsys_hwcg(id);

	mtk_clk_unlock(flags);

	spin_lock_irqsave(&pgcb_lock, spinlock_save_flags);
	list_for_each_entry(pgcb, &pgcb_list, list) {
		if (pgcb->after_on)
			pgcb->after_on(id);
	}
	spin_unlock_irqrestore(&pgcb_lock, spinlock_save_flags);

	return r;
}

static int disable_subsys(enum subsys_id id)
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

#if MT_CCF_BRINGUP
	pr_debug("[CCF] %s: sys=%s, id=%d\n", __func__, sys->name, id);
	if (sys->ops->get_state(sys) == SUBSYS_PWR_ON) {
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
	spin_lock_irqsave(&pgcb_lock, spinlock_save_flags);
	list_for_each_entry_reverse(pgcb, &pgcb_list, list) {
		if (pgcb->before_off)
			pgcb->before_off(id);
	}
	spin_unlock_irqrestore(&pgcb_lock, spinlock_save_flags);

	mtk_clk_lock(flags);

#if CHECK_PWR_ST
	if (sys->ops->get_state(sys) == SUBSYS_PWR_DOWN) {
		mtk_clk_unlock(flags);
		return 0;
	}
#endif				/* CHECK_PWR_ST */

	/*
	 * Check if subsys CGs are still on before the mtcmos  is going
	 * to be off. (Could do nothing here for early porting)
	 */
	mtk_check_subsys_swcg(id);

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
	struct clk *pre_clk2;
	struct clk *pre_clk3;
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
	if (pg->pre_clk2) {
		r = clk_prepare_enable(pg->pre_clk2);
		if (r) {
			clk_disable_unprepare(pg->pre_clk);
			return r;
		}
	}
	if (pg->pre_clk3) {
		r = clk_prepare_enable(pg->pre_clk3);
		if (r) {
			clk_disable_unprepare(pg->pre_clk2);
			clk_disable_unprepare(pg->pre_clk);
			return r;
		}
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

	if (pg->pre_clk3)
		clk_disable_unprepare(pg->pre_clk3);
	if (pg->pre_clk2)
		clk_disable_unprepare(pg->pre_clk2);
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
					struct clk *pre_clk2,
					struct clk *pre_clk3,
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
	pg->pre_clk2 = pre_clk2;
	pg->pre_clk3 = pre_clk3;
	pg->pd_id = pd_id;
	pg->hw.init = &init;

	clk = clk_register(NULL, &pg->hw);
	if (IS_ERR(clk))
		kfree(pg);

	return clk;
}

struct mtk_power_gate {
	int id;
	const char *name;
	const char *parent_name;
	const char *pre_clk_name;
	const char *pre_clk2_name;
	const char *pre_clk3_name;
	enum subsys_id pd_id;
};

#define PGATE(_id, _name, _parent, _pre_clk, _pd_id) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.pre_clk_name = _pre_clk,		\
		.pd_id = _pd_id,			\
	}

#define PGATE3(_id, _name, _parent, _pre_clk, _pre2_clk, _pre3_clk, _pd_id) {\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.pre_clk_name = _pre_clk,		\
		.pre_clk2_name = _pre2_clk,		\
		.pre_clk3_name = _pre3_clk,		\
		.pd_id = _pd_id,			\
	}

/* MT6885: TODO:FIXME: all values needed to be verified */
/* MT6885: preclks */
struct mtk_power_gate scp_clks[] __initdata = {
	PGATE(SCP_SYS_MD1, "PG_MD1", NULL, NULL, SYS_MD1),
	PGATE(SCP_SYS_CONN, "PG_CONN", NULL, NULL, SYS_CONN),
	PGATE(SCP_SYS_MDP, "PG_MDP", NULL, "mdp_sel", SYS_MDP),
	PGATE(SCP_SYS_DIS, "PG_DIS", NULL, "disp_sel", SYS_DIS),
	PGATE(SCP_SYS_MFG0, "PG_MFG0", NULL, "mfg_sel", SYS_MFG0),
	PGATE(SCP_SYS_MFG1, "PG_MFG1", "PG_MFG0", NULL, SYS_MFG1),
	PGATE(SCP_SYS_MFG2, "PG_MFG2", "PG_MFG1", NULL, SYS_MFG2),
	PGATE(SCP_SYS_MFG3, "PG_MFG3", "PG_MFG1", NULL, SYS_MFG3),
	PGATE(SCP_SYS_MFG4, "PG_MFG4", "PG_MFG1", NULL, SYS_MFG4),
	PGATE(SCP_SYS_MFG5, "PG_MFG5", "PG_MFG1", NULL, SYS_MFG5),
	PGATE(SCP_SYS_MFG6, "PG_MFG6", "PG_MFG1", NULL, SYS_MFG6),
	PGATE(SCP_SYS_ISP, "PG_ISP", "PG_MDP", "img1_sel", SYS_ISP),
	PGATE(SCP_SYS_ISP2, "PG_ISP2", "PG_DIS", "img2_sel", SYS_ISP2), /* MDP*/
	PGATE(SCP_SYS_IPE, "PG_IPE", "PG_DIS", "ipe_sel", SYS_IPE), /* MDP */
	PGATE(SCP_SYS_VDEC, "PG_VDEC", "PG_DIS", "vdec_sel", SYS_VDE),
	PGATE(SCP_SYS_VDEC2, "PG_VDEC2", "PG_DIS", "vdec_sel", SYS_VDE2),
	PGATE(SCP_SYS_VENC, "PG_VENC", "PG_DIS", "venc_sel", SYS_VEN),
	PGATE(SCP_SYS_VENC_CORE1, "PG_VENC_C1", "PG_DIS", "venc_sel",
								SYS_VEN_CORE1),

	PGATE3(SCP_SYS_AUDIO, "PG_AUDIO", NULL, "aud_intbus_sel",
			"infracfg_ao_audio_26m_bclk_ck",
			"infracfg_ao_audio_cg", SYS_AUDIO),
	PGATE(SCP_SYS_ADSP, "PG_ADSP", NULL, "adsp_sel", SYS_ADSP),
	PGATE(SCP_SYS_CAM, "PG_CAM", "PG_DIS", "cam_sel", SYS_CAM),
	PGATE(SCP_SYS_CAM_RAWA, "PG_CAM_RAWA", "PG_CAM", NULL, SYS_CAM_RAWA),
	PGATE(SCP_SYS_CAM_RAWB, "PG_CAM_RAWB", "PG_CAM", NULL, SYS_CAM_RAWB),
	PGATE(SCP_SYS_CAM_RAWC, "PG_CAM_RAWC", "PG_CAM", NULL, SYS_CAM_RAWC),
	PGATE(SCP_SYS_DP_TX, "PG_DP_TX", "PG_DIS", NULL, SYS_DP_TX),
	/* Gary Wang: no need to turn of disp mtcmos*/
	PGATE3(SCP_SYS_VPU, "PG_VPU", NULL, "ipu_if_sel", "dsp_sel",
							"dsp7_sel", SYS_VPU),
};

static void __init init_clk_scpsys(void __iomem *infracfg_reg,
				   void __iomem *spm_reg,
				   struct clk_onecell_data *clk_data)
{
	int i;
	struct clk *clk;
	struct clk *pre_clk, *pre_clk2, *pre_clk3;


	infracfg_base = infracfg_reg;
	spm_base = spm_reg;
	spm_base_debug = spm_reg;

	for (i = 0; i < ARRAY_SIZE(scp_clks); i++) {
		struct mtk_power_gate *pg = &scp_clks[i];

		pre_clk = pg->pre_clk_name ?
			__clk_lookup(pg->pre_clk_name) : NULL;

		pre_clk2 = pg->pre_clk2_name ?
			__clk_lookup(pg->pre_clk2_name) : NULL;

		pre_clk3 = pg->pre_clk3_name ?
			__clk_lookup(pg->pre_clk3_name) : NULL;

		clk = mt_clk_register_power_gate(pg->name, pg->parent_name,
			pre_clk, pre_clk2, pre_clk3, pg->pd_id);

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

static void iomap_mm(void)
{
	clk_mdp_base = find_and_iomap("mediatek,mdpsys_config");
	if (!clk_mdp_base)
		return;
	clk_disp_base = find_and_iomap("mediatek,dispsys_config");
	if (!clk_disp_base)
		return;
	clk_img1_base = find_and_iomap("mediatek,imgsys");
	if (!clk_img1_base)
		return;
	clk_img2_base = find_and_iomap("mediatek,imgsys2");
	if (!clk_img2_base)
		return;
	clk_ipe_base = find_and_iomap("mediatek,ipesys_config");
	if (!clk_ipe_base)
		return;
	clk_vdec_soc_gcon_base = find_and_iomap("mediatek,vdec_soc_gcon");
	if (!clk_vdec_soc_gcon_base)
		return;
	clk_vdec_gcon_base = find_and_iomap("mediatek,vdec_gcon");
	if (!clk_vdec_gcon_base)
		return;
	clk_venc_gcon_base = find_and_iomap("mediatek,venc_gcon");
	if (!clk_venc_gcon_base)
		return;
	clk_venc_c1_gcon_base = find_and_iomap("mediatek,venc_c1_gcon");
	if (!clk_venc_c1_gcon_base)
		return;
	clk_cam_base = find_and_iomap("mediatek,camsys");
	if (!clk_cam_base)
		return;
	clk_cam_rawa_base = find_and_iomap("mediatek,camsys_rawa");
	if (!clk_cam_rawa_base)
		return;
	clk_cam_rawb_base = find_and_iomap("mediatek,camsys_rawb");
	if (!clk_cam_rawb_base)
		return;
	clk_cam_rawc_base = find_and_iomap("mediatek,camsys_rawc");
	if (!clk_cam_rawc_base)
		return;
	clk_apu_vcore_base = find_and_iomap("mediatek,apu_vcore");
	if (!clk_apu_vcore_base)
		return;
	clk_apu_conn_base = find_and_iomap("mediatek,apu_conn");
	if (!clk_apu_conn_base)
		return;
}
#endif

void enable_subsys_hwcg(enum subsys_id id)
{
	if (id == SYS_MDP) {
		/* SMI0, SMI1, SMI2, APMCU_GALS */
		clk_writel(MDP_CG_CLR1, 0x1112);
	} else if (id == SYS_DIS) {
		/* SMI_COMMON, SMI_GALS, SMI_INFRA, SMI_IOMMU */
		clk_writel(DISP_CG_CLR1, 0x88880000);
	} else if (id == SYS_ISP) {
		/* LARB9_CGPDN */
		clk_writel(IMG1_CG_CLR, 0x1);
	} else if (id == SYS_ISP2) {
		/* LARB11_CGPDN */
		clk_writel(IMG2_CG_CLR, 0x1);
	} else if (id == SYS_IPE) {
		/* LARB19_CGPDN, LARB20_CGPDN, IPE_SMI_SUBCOM_CGPDN */
		clk_writel(IPE_CG_CLR, 0x7);
	} else if (id == SYS_VDE) {
		/* VDEC_CKEN, LAT_CKEN */
		/* LARB1_CKEN */
		clk_writel(VDEC_SOC_CKEN_SET, 0x1);
		clk_writel(VDEC_SOC_LAT_CKEN_SET, 0x1);
		clk_writel(VDEC_SOC_LARB1_CKEN_SET, 0x1);
		//print_subsys_reg(vdec_soc_sys);
	} else if (id == SYS_VDE2) {
		/* VDEC_CKEN, LAT_CKEN */
		clk_writel(VDEC_CKEN_SET, 0x1);
		clk_writel(VDEC_LAT_CKEN_SET, 0x1);
		clk_writel(VDEC_LARB1_CKEN_SET, 0x1);
		//print_subsys_reg(vdec_soc_sys);
		//print_subsys_reg(vdecsys);
	} else if (id == SYS_VEN) {
		/* SET1_VENC */
		clk_writel(VENC_CG_SET, 0x4);
	} else if (id == SYS_VEN_CORE1) {
		/* SET1_VENC */
		clk_writel(VENC_C1_CG_SET, 0x4);
	} else if (id == SYS_CAM) {
		/* LARB13_CGPDN, LARB14_CGPDN, LARB15_CGPDN */
		clk_writel(CAMSYS_CG_CLR, 0xD);
	} else if (id == SYS_CAM_RAWA) {
		/* LARBX_CGPDN */
		clk_writel(CAMSYS_RAWA_CG_CLR, 0x1);
	} else if (id == SYS_CAM_RAWB) {
		/* LARBX_CGPDN */
		clk_writel(CAMSYS_RAWB_CG_CLR, 0x1);
	} else if (id == SYS_CAM_RAWC) {
		/* LARBX_CGPDN */
		clk_writel(CAMSYS_RAWC_CG_CLR, 0x1);
	} else if (id == SYS_VPU) {
		clk_writel(APU_VCORE_CG_CLR, 0xFFFFFFFF);
		clk_writel(APU_CONN_CG_CLR, 0xFFFFFFFF);
	}
}

static void __init mt_scpsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *infracfg_reg;
	void __iomem *spm_reg;
	void __iomem *ckgen_reg;

	int r;

	infracfg_reg = get_reg(node, 0);
	spm_reg = get_reg(node, 1);
	ckgen_reg = get_reg(node, 2);

	pr_notice("mt_scpsys_init begin\n");

	if (!infracfg_reg || !spm_reg   || !ckgen_reg) {
		pr_notice("clk-pg-mt6885: missing reg\n");
		return;
	}

	clk_data = alloc_clk_data(SCP_NR_SYSS);

	init_clk_scpsys(infracfg_reg, spm_reg, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_notice("[CCF] %s:could not register clock provide\n",
			__func__);

	ckgen_base = ckgen_reg;

	/*MM Bus*/
	iomap_mm();

	spin_lock_init(&pgcb_lock);

#if !MT_CCF_BRINGUP
	/* subsys init: per modem owner request, remain modem power */
	/* disable_subsys(SYS_MD1); */
#else				/*power on all subsys for bring up */
#ifndef CONFIG_FPGA_EARLY_PORTING
	pr_notice("MTCMOS AO begin\n");

	spm_mtcmos_ctrl_md1(STA_POWER_DOWN);
	spm_mtcmos_ctrl_conn(STA_POWER_DOWN);

	pr_notice("MTCMOS MM AO begin\n");
	spm_mtcmos_ctrl_mdp(STA_POWER_ON);
	spm_mtcmos_ctrl_dis(STA_POWER_ON);

	pr_notice("MTCMOS GPU begin\n");
	spm_mtcmos_ctrl_mfg0(STA_POWER_ON);
	spm_mtcmos_ctrl_mfg1(STA_POWER_ON);
	spm_mtcmos_ctrl_mfg2(STA_POWER_ON);
	spm_mtcmos_ctrl_mfg3(STA_POWER_ON);
	spm_mtcmos_ctrl_mfg4(STA_POWER_ON);
	spm_mtcmos_ctrl_mfg5(STA_POWER_ON);
	spm_mtcmos_ctrl_mfg6(STA_POWER_ON);

	pr_notice("MTCMOS ISP begin\n");
	spm_mtcmos_ctrl_isp(STA_POWER_ON);
	spm_mtcmos_ctrl_isp2(STA_POWER_ON);

	pr_notice("MTCMOS IPE begin\n");
	spm_mtcmos_ctrl_ipe(STA_POWER_ON);

	pr_notice("MTCMOS VDE/VEN begin\n");
	spm_mtcmos_ctrl_vde(STA_POWER_ON);
	spm_mtcmos_ctrl_vde2(STA_POWER_ON);
	spm_mtcmos_ctrl_ven(STA_POWER_ON);
	spm_mtcmos_ctrl_ven_core1(STA_POWER_ON);

	pr_notice("MTCMOS AUDIO begin\n");
	spm_mtcmos_ctrl_audio(STA_POWER_ON);
	spm_mtcmos_ctrl_adsp_shut_down(STA_POWER_ON);
	spm_mtcmos_ctrl_adsp_dormant(STA_POWER_ON);

	pr_notice("MTCMOS CAM begin\n");
	spm_mtcmos_ctrl_cam(STA_POWER_ON);
	spm_mtcmos_ctrl_cam_rawa(STA_POWER_ON);
	spm_mtcmos_ctrl_cam_rawb(STA_POWER_ON);
	spm_mtcmos_ctrl_cam_rawc(STA_POWER_ON);
	spm_mtcmos_ctrl_dp_tx(STA_POWER_ON);

	/* pr_notice("MTCMOS VPU begin\n"); */
	/* spm_mtcmos_vpu(STA_POWER_ON); */

	pr_notice("MTCMOS AO end\n");
#endif /* CONFIG_FPGA_EARLY_PORTING */
#endif /* !MT_CCF_BRINGUP */
}
CLK_OF_DECLARE_DRIVER(mtk_pg_regs, "mediatek,scpsys", mt_scpsys_init);

#if 0 /* MT6885 todo: add print CG status for suspend checking */
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
		"mm_mdp_hdr",
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

#endif

#if 1 /*only use for suspend test*/
void mtcmos_force_off(void)
{
	pr_notice("suspend test: dp_tx\n");
	spm_mtcmos_ctrl_dp_tx(STA_POWER_DOWN);

	pr_notice("suspend test: cam\n");
	spm_mtcmos_ctrl_cam_rawa(STA_POWER_DOWN);
	spm_mtcmos_ctrl_cam_rawb(STA_POWER_DOWN);
	spm_mtcmos_ctrl_cam_rawc(STA_POWER_DOWN);
	spm_mtcmos_ctrl_cam(STA_POWER_DOWN);

	pr_notice("suspend test: ven\n");
	spm_mtcmos_ctrl_ven_core1(STA_POWER_DOWN);
	spm_mtcmos_ctrl_ven(STA_POWER_DOWN);

	pr_notice("suspend test: vde\n");
	spm_mtcmos_ctrl_vde2(STA_POWER_DOWN);
	spm_mtcmos_ctrl_vde(STA_POWER_DOWN);

	pr_notice("suspend test: ipe\n");
	spm_mtcmos_ctrl_ipe(STA_POWER_DOWN);

	pr_notice("suspend test: isp\n");
	spm_mtcmos_ctrl_isp2(STA_POWER_DOWN);
	spm_mtcmos_ctrl_isp(STA_POWER_DOWN);

	pr_notice("suspend test: mfg\n");
	spm_mtcmos_ctrl_mfg6(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg5(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg4(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg3(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg2(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg1(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg0(STA_POWER_DOWN);

	pr_notice("suspend test: dis\n");
	spm_mtcmos_ctrl_dis(STA_POWER_DOWN);

	pr_notice("suspend test: mdp\n");
	spm_mtcmos_ctrl_mdp(STA_POWER_DOWN);

	pr_notice("suspend test: md1\n");
	spm_mtcmos_ctrl_md1(STA_POWER_DOWN);

	pr_notice("suspend test: conn\n");
	spm_mtcmos_ctrl_conn(STA_POWER_DOWN);

	pr_notice("suspend test: audio\n");
	spm_mtcmos_ctrl_audio(STA_POWER_DOWN);

	pr_notice("suspend test: adsp\n");
	/* spm_mtcmos_ctrl_adsp_shut_down(STA_POWER_DOWN); */
	spm_mtcmos_ctrl_adsp_dormant(STA_POWER_DOWN);
}
#endif
