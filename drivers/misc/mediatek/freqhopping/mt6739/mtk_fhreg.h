/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MT_FHREG_H__
#define __MT_FHREG_H__

#include <linux/bitops.h>
/* #include <mach/mt_reg_base.h> */


/* **************************************************** */
/* IP base address */
/* **************************************************** */


#ifdef CONFIG_ARM64
#define REG_ADDR(x)             ((unsigned long)g_fhctl_base + (x))
#define REG_MCU_FHCTL_ADDR(x)   ((unsigned long)g_mcu_fhctl_base + (x))
#define REG_APMIX_ADDR(x)       ((unsigned long)g_apmixed_base + (x))
#define REG_MCUMIX_ADDR(x)      ((unsigned long)g_mcumixed_base + (x))
#define REG_DDRPHY_ADDR(x)      ((unsigned long)g_ddrphy_base + (x))
#else
#define REG_ADDR(x)             ((unsigned int)g_fhctl_base + (x))
#define REG_MCU_FHCTL_ADDR(x)   ((unsigned int)g_mcu_fhctl_base + (x))
#define REG_APMIX_ADDR(x)       ((unsigned int)g_apmixed_base + (x))
#define REG_MCUMIX_ADDR(x)      ((unsigned int)g_mcumixed_base + (x))
#define REG_DDRPHY_ADDR(x)      ((unsigned int)g_ddrphy_base + (x))
#endif

/* **************************************************** */
/* FHCTL register */
/* **************************************************** */

#define REG_FHCTL_HP_EN     REG_ADDR(0x0000)
#define REG_FHCTL_CLK_CON   REG_ADDR(0x0004)
#define REG_FHCTL_RST_CON   REG_ADDR(0x0008)
#define REG_FHCTL_SLOPE0    REG_ADDR(0x000C)
#define REG_FHCTL_SLOPE1    REG_ADDR(0x0010)
#define REG_FHCTL_DSSC_CFG  REG_ADDR(0x0014)

#define REG_FHCTL_DSSC0_CON REG_ADDR(0x0018)
#define REG_FHCTL_DSSC1_CON REG_ADDR(0x001C)
#define REG_FHCTL_DSSC2_CON REG_ADDR(0x0020)
#define REG_FHCTL_DSSC3_CON REG_ADDR(0x0024)
#define REG_FHCTL_DSSC4_CON REG_ADDR(0x0028)
#define REG_FHCTL_DSSC5_CON REG_ADDR(0x002C)
#define REG_FHCTL_DSSC6_CON REG_ADDR(0x0030)
#define REG_FHCTL_DSSC7_CON REG_ADDR(0x0034)

#define REG_FHCTL0_CFG      REG_ADDR(0x0038)
#define REG_FHCTL0_UPDNLMT  REG_ADDR(0x003C)
#define REG_FHCTL0_DDS      REG_ADDR(0x0040)
#define REG_FHCTL0_DVFS     REG_ADDR(0x0044)
#define REG_FHCTL0_MON      REG_ADDR(0x0048)

#define REG_FHCTL1_CFG      REG_ADDR(0x004C)
#define REG_FHCTL1_UPDNLMT  REG_ADDR(0x0050)
#define REG_FHCTL1_DDS      REG_ADDR(0x0054)
#define REG_FHCTL1_DVFS     REG_ADDR(0x0058)
#define REG_FHCTL1_MON      REG_ADDR(0x005C)

#define REG_FHCTL2_CFG      REG_ADDR(0x0060)
#define REG_FHCTL2_UPDNLMT  REG_ADDR(0x0064)
#define REG_FHCTL2_DDS      REG_ADDR(0x0068)
#define REG_FHCTL2_DVFS     REG_ADDR(0x006C)
#define REG_FHCTL2_MON      REG_ADDR(0x0070)

#define REG_FHCTL3_CFG      REG_ADDR(0x0074)
#define REG_FHCTL3_UPDNLMT  REG_ADDR(0x0078)
#define REG_FHCTL3_DDS      REG_ADDR(0x007C)
#define REG_FHCTL3_DVFS     REG_ADDR(0x0080)
#define REG_FHCTL3_MON      REG_ADDR(0x0084)

#define REG_FHCTL4_CFG      REG_ADDR(0x0088)
#define REG_FHCTL4_UPDNLMT  REG_ADDR(0x008C)
#define REG_FHCTL4_DDS      REG_ADDR(0x0090)
#define REG_FHCTL4_DVFS     REG_ADDR(0x0094)
#define REG_FHCTL4_MON      REG_ADDR(0x0098)

#define REG_FHCTL5_CFG      REG_ADDR(0x009C)
#define REG_FHCTL5_UPDNLMT  REG_ADDR(0x00A0)
#define REG_FHCTL5_DDS      REG_ADDR(0x00A4)
#define REG_FHCTL5_DVFS     REG_ADDR(0x00A8)
#define REG_FHCTL5_MON      REG_ADDR(0x00AC)

/* **************************************************** */
/* APMIXED CON0/CON1 Register */
/* **************************************************** */

#define REG_PLL_NOT_SUPPORT 0xdeadbeef
/*CON0, PLL enable status */
#define REG_ARMPLL_CON0     REG_APMIX_ADDR(0x0200)
#define REG_ARMPLL2_CON0    REG_PLL_NOT_SUPPORT /* not support in MT6739 */
#define REG_ARMPLL3_CON0    REG_PLL_NOT_SUPPORT /* not support in MT6739 */
#define REG_CCIPLL_CON0     REG_PLL_NOT_SUPPORT /* not support in MT6739 */
#define REG_GPUPLL_CON0     REG_PLL_NOT_SUPPORT /* not support in MT6739 */
#define REG_MPLL_CON0       REG_PLL_NOT_SUPPORT /* not support in MT6739 */
#define REG_MEMPLL_CON0     REG_PLL_NOT_SUPPORT /* It's in DDRPHY */
#define REG_MAINPLL_CON0    REG_APMIX_ADDR(0x0220)
#define REG_MFGPLL_CON0     REG_APMIX_ADDR(0x0240)
#define REG_MSDCPLL_CON0    REG_APMIX_ADDR(0x0250)
#define REG_MMPLL_CON0      REG_APMIX_ADDR(0x0270)
#define REG_VDECPLL_CON0    REG_PLL_NOT_SUPPORT /* not support in MT6739 */
#define REG_TVDPLL_CON0     REG_PLL_NOT_SUPPORT /* not support in MT6739 */

/*CON1, DDS value */
#define REG_ARMPLL_CON1     REG_APMIX_ADDR(0x0204)
#define REG_ARMPLL2_CON1    REG_PLL_NOT_SUPPORT /* not support in MT6739 */
#define REG_ARMPLL3_CON1    REG_PLL_NOT_SUPPORT /* not support in MT6739 */
#define REG_CCIPLL_CON1     REG_PLL_NOT_SUPPORT /* not support in MT6739 */
#define REG_GPUPLL_CON1     REG_PLL_NOT_SUPPORT /* not support in MT6739 */
#define REG_MPLL_CON1       REG_PLL_NOT_SUPPORT /* not support in MT6739 */
/* MEMPLL's dds value is in 0x1001E624[31:11] */
#define REG_MEMPLL_CON1     REG_DDRPHY_ADDR(0x0624)
#define REG_MAINPLL_CON1    REG_APMIX_ADDR(0x0224)
#define REG_MFGPLL_CON1     REG_APMIX_ADDR(0x0244)
#define REG_MSDCPLL_CON1    REG_APMIX_ADDR(0x0254)
#define REG_MMPLL_CON1      REG_APMIX_ADDR(0x0274)
#define REG_VDECPLL_CON1    REG_PLL_NOT_SUPPORT /* not support in MT6739 */
#define REG_TVDPLL_CON1     REG_PLL_NOT_SUPPORT /* not support in MT6739 */

#define REG_FH_PLL0_CON0    REG_ARMPLL_CON0
#define REG_FH_PLL1_CON0    REG_MAINPLL_CON0
#define REG_FH_PLL2_CON0    REG_MSDCPLL_CON0
#define REG_FH_PLL3_CON0    REG_MFGPLL_CON0
#define REG_FH_PLL4_CON0    REG_MEMPLL_CON0
#define REG_FH_PLL5_CON0    REG_MMPLL_CON0

#define REG_FH_PLL0_CON1    REG_ARMPLL_CON1
#define REG_FH_PLL1_CON1    REG_MAINPLL_CON1
#define REG_FH_PLL2_CON1    REG_MSDCPLL_CON1
#define REG_FH_PLL3_CON1    REG_MFGPLL_CON1
#define REG_FH_PLL4_CON1    REG_MEMPLL_CON1
#define REG_FH_PLL5_CON1    REG_MMPLL_CON1

/***************************************************** */

/* **************************************************** */
/* FHCTL Register mask */
/* **************************************************** */

#define MASK_FRDDSX_DYS         (0xFU<<20)
#define MASK_FRDDSX_DTS         (0xFU<<16)
#define FH_FHCTLX_MON_SEL       (0x3U<<5)
#define FH_FHCTLX_PAUSE         (0x1U<<4)
#define FH_DYSSCX_EN            (0x1U<<3)
#define FH_SFSTRX_EN            (0x1U<<2)
#define FH_FRDDSX_EN            (0x1U<<1)
#define FH_FHCTLX_EN            (0x1U<<0)

#define FH_FRDDSX_DNLMT         (0xFFU<<16)
#define FH_FRDDSX_UPLMT         (0xFFU)

#define FH_FHCTLX_PLL_TGL_ORG   (0x1U<<31)
#define FH_FHCTLX_PLL_ORG       (0x3FFFFU) /* Note that bit21 is invalid  */

#define FH_MON_FHCTLX_PAUSE     (0x1U<<31)
#define FH_MON_FHCTLX_PRD       (0x1U<<30)
#define FH_MON_SFSTRX_PRD       (0x1U<<29)
#define FH_MON_FRDDSX_PRD       (0x1U<<28)
#define FH_FHCTLX_STATE         (0xFU<<24)
#define FH_FHCTLX_PLL_DDS       (0x3FFFFU)

/* **************************************************** */
/* Macro */
/* **************************************************** */


static inline unsigned int uffs(unsigned int x)
{
	unsigned int r = 1;

	if (!x)
		return 0;
	if (!(x & 0xffff)) {
		x >>= 16;
		r += 16;
	}
	if (!(x & 0xff)) {
		x >>= 8;
		r += 8;
	}
	if (!(x & 0xf)) {
		x >>= 4;
		r += 4;
		}
	if (!(x & 3)) {
		x >>= 2;
		r += 2;
		}
	if (!(x & 1)) {
		x >>= 1;
		r += 1;
		}
	return r;
}

#define fh_read8(reg)           readb(reg)
#define fh_read16(reg)          readw(reg)
#define fh_read32(reg)          readl((void __iomem *)reg)
#define fh_write8(reg, val)     mt_reg_sync_writeb((val), (reg))
#define fh_write16(reg, val)    mt_reg_sync_writew((val), (reg))
#define fh_write32(reg, val)    mt_reg_sync_writel((val), (reg))

#define fh_set_field(reg, field, val) \
	do { \
		unsigned int tv = fh_read32(reg); \
		tv &= ~(field); \
		tv |= ((val) << (uffs((unsigned int)field) - 1)); \
		fh_write32(reg, tv); \
	} while (0)

#define fh_get_field(reg, field, val) \
	do { \
		unsigned int tv = fh_read32(reg); \
		val = ((tv & (field)) >> (uffs((unsigned int)field) - 1)); \
	} while (0)

#endif /* #ifndef __MT_FHREG_H__ */

