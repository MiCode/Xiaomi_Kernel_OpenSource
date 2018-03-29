/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MT_IDLE_INTERNAL_H__
#define __MT_IDLE_INTERNAL_H__
#include <linux/io.h>

/*
 * Chip specific declaratinos
 */
#include "mt_idle_internal.h"

enum {
	UNIV_PLL = 0,
	MM_PLL,
	MSDC_PLL,
	VENC_PLL,
	NR_PLLS,
};

enum {
	CG_INFRA0  = 0,
	CG_INFRA1,
	CG_INFRA2,
	CG_DISP0,
	CG_IMAGE,
	CG_MFG,
	CG_AUDIO,
	CG_VDEC,
	CG_VENC,
	NR_GRPS,
};

extern bool             soidle_by_pass_pg;
extern bool             dpidle_by_pass_pg;

extern void __iomem *infrasys_base;
extern void __iomem *mmsys_base;
extern void __iomem *sleepsys_base;
extern void __iomem *topcksys_base;
extern void __iomem *mfgsys_base;
extern void __iomem *imgsys_base;
extern void __iomem *vdecsys_base;
extern void __iomem *vencsys_base;
extern void __iomem *audiosys_base_in_idle;

extern void __iomem  *apmixed_base_in_idle;

#define INFRA_REG(ofs)      (infrasys_base + ofs)
#define MM_REG(ofs)         (mmsys_base + ofs)
#define SPM_REG(ofs)        (sleepsys_base + ofs)
#define TOPCKSYS_REG(ofs)      (topcksys_base + ofs)
#define MFGSYS_REG(ofs)     (mfgsys_base + ofs)
#define IMGSYS_REG(ofs)     (imgsys_base + ofs)
#define VDECSYS_REG(ofs)    (vdecsys_base + ofs)
#define VENCSYS_REG(ofs)    (vencsys_base + ofs)
#define AUDIOSYS_REG(ofs)   (audiosys_base_in_idle + ofs)

#define APMIXEDSYS(ofs)	    (apmixed_base_in_idle + ofs)

#ifdef SPM_PWR_STATUS
#undef SPM_PWR_STATUS
#endif

#ifdef SPM_PWR_STATUS_2ND
#undef SPM_PWR_STATUS_2ND
#endif

#define	INFRA_SW_CG_0_STA   INFRA_REG(0x0094)
#define	INFRA_SW_CG_1_STA   INFRA_REG(0x0090)
#define	INFRA_SW_CG_2_STA   INFRA_REG(0x00AC)
#define DISP_CG_CON0        MM_REG(0x100)
#define DISP_CG_CON1        MM_REG(0x110)

#define AUDIO_TOP_CON0      AUDIOSYS_REG(0x0)

#define SPM_PWR_STATUS      SPM_REG(0x0180)
#define SPM_PWR_STATUS_2ND  SPM_REG(0x0184)
#define SPM_ISP_PWR_CON     SPM_REG(0x0308)
#define SPM_MFG_PWR_CON     SPM_REG(0x0338)
#define SPM_VDE_PWR_CON     SPM_REG(0x0300)
#define SPM_VEN_PWR_CON     SPM_REG(0x0304)
#define SPM_DIS_PWR_CON     SPM_REG(0x030c)
#define SPM_AUDIO_PWR_CON   SPM_REG(0x0314)
#define SPM_MD1_PWR_CON   SPM_REG(0x0320)
#define SPM_MD2_PWR_CON   SPM_REG(0x0324)
#define SPM_C2K_PWR_CON   SPM_REG(0x0328)
#define SPM_CONN_PWR_CON   SPM_REG(0x032c)
#define SPM_MDSYS_INTF_INFRA_PWR_CON SPM_REG(0x0360)

#define	CLK_CFG_UPDATE          TOPCKSYS_REG(0x004)
#define CLK_CFG_4               TOPCKSYS_REG(0x080)
#define CLK_CFG_7               TOPCKSYS_REG(0x0B0)

#define ARMCA15PLL_BASE		APMIXEDSYS(0x200)
#define ARMCA15PLL_CON0		APMIXEDSYS(0x0200)
#define ARMCA7PLL_CON0		APMIXEDSYS(0x0210)
#define MAINPLL_CON0		APMIXEDSYS(0x0220)
#define UNIVPLL_CON0		APMIXEDSYS(0x0230)
#define MMPLL_CON0			APMIXEDSYS(0x0240)
#define MSDCPLL_CON0		APMIXEDSYS(0x0250)
#define VENCPLL_CON0		APMIXEDSYS(0x0260)
#define TVDPLL_CON0			APMIXEDSYS(0x0270)
#define APLL1_CON0			APMIXEDSYS(0x02a0)
#define APLL2_CON0			APMIXEDSYS(0x02b4)

#define MFG_CG_CON          MFGSYS_REG(0x0)
#define IMG_CG_CON          IMGSYS_REG(0x0)
#define VDEC_CG_CON_0       VDECSYS_REG(0x0)
#define VDEC_CG_CON_1       VDECSYS_REG(0x8)
#define VENCSYS_CG_CON      VENCSYS_REG(0x0)

#define DIS_PWR_STA_MASK        BIT(3)
#define MFG_PWR_STA_MASK        BIT(4)
#define ISP_PWR_STA_MASK        BIT(5)
#define VDE_PWR_STA_MASK        BIT(7)
#define VEN_PWR_STA_MASK        BIT(21)
#define MFG_2D_PWR_STA_MASK     BIT(22)
#define MFG_ASYNC_PWR_STA_MASK  BIT(23)
#define AUDIO_PWR_STA_MASK      BIT(24)

/*
 * Common declarations
 */
enum mt_idle_mode {
	MT_DPIDLE = 0,
	MT_SOIDLE,
	MT_MCIDLE,
	MT_SLIDLE
};

enum {
	IDLE_TYPE_DP = 0,
	IDLE_TYPE_SO3,
	IDLE_TYPE_SO,
	IDLE_TYPE_MC,
	IDLE_TYPE_SL,
	IDLE_TYPE_RG,
	NR_TYPES,
};

enum {
	BY_CPU = 0,
	BY_CLK,
	BY_TMR,
	BY_OTH,
	BY_VTG,
	BY_FRM,
	BY_PLL,
	BY_PWM,
#ifdef CONFIG_CPU_ISOLATION
	BY_ISO,
#endif
	BY_DVFSP,
	NR_REASONS,
};

#define INVALID_GRP_ID(grp)     (grp < 0 || grp >= NR_GRPS)
#define INVALID_IDLE_ID(id)     (id < 0 || id >= NR_TYPES)
#define INVALID_REASON_ID(id)   (id < 0 || id >= NR_REASONS)

#define idle_readl(addr)	    __raw_readl(addr)

extern unsigned int dpidle_blocking_stat[NR_GRPS][32];
extern int idle_switch[NR_TYPES];

extern unsigned int idle_condition_mask[NR_TYPES][NR_GRPS];

extern unsigned int soidle3_pll_condition_mask[NR_PLLS];

/*
 * Function Declarations
 */
const char *mt_get_idle_name(int id);
const char *mt_get_reason_name(int);
const char *mt_get_cg_group_name(int id);
const char *mt_get_pll_group_name(int id);

bool mt_idle_check_cg(unsigned int block_mask[NR_TYPES][NR_GRPS+1]);
bool mt_idle_check_cg_i2c_appm(unsigned int *block_mask);
bool mt_idle_check_pll(unsigned int *condition_mask, unsigned int *block_mask);

void __init iomap_init(void);

bool mt_idle_disp_is_pwm_rosc(void);
bool mt_idle_auxadc_is_released(void);

#endif /* __MT_IDLE_INTERNAL_H__ */

