/*
 * Copyright (C) 2011 MediaTek, Inc.
 *
 * Author: Holmes Chiou <holmes.chiou@mediatek.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/sched_clock.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/seq_file.h>
#include <linux/bitops.h>
#include <mt_freqhopping_drv.h>

#include "mt_freqhopping.h"
#include "sync_write.h"

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

static void __iomem *freqhopping_base;	/* 0x10209E00 */
static void __iomem *apmixed_base;	/* 0x10209000 */
static void __iomem *ddrphy_base;	/* 0x10100000 */

#define FREQHOPPING_BASE    (freqhopping_base)
#define REG_FHCTL_HP_EN     (FREQHOPPING_BASE+0x0000)
#define REG_FHCTL_CLK_CON   (FREQHOPPING_BASE+0x0004)
#define REG_FHCTL_RST_CON   (FREQHOPPING_BASE+0x0008)
#define REG_FHCTL_SLOPE0    (FREQHOPPING_BASE+0x000C)
#define REG_FHCTL_SLOPE1    (FREQHOPPING_BASE+0x0010)
#define REG_FHCTL_DSSC_CFG  (FREQHOPPING_BASE+0x0014)

#define REG_FHCTL_DSSC0_CON (FREQHOPPING_BASE+0x0018)
#define REG_FHCTL_DSSC1_CON (FREQHOPPING_BASE+0x001C)
#define REG_FHCTL_DSSC2_CON (FREQHOPPING_BASE+0x0020)
#define REG_FHCTL_DSSC3_CON (FREQHOPPING_BASE+0x0024)
#define REG_FHCTL_DSSC4_CON (FREQHOPPING_BASE+0x0028)
#define REG_FHCTL_DSSC5_CON (FREQHOPPING_BASE+0x002C)
#define REG_FHCTL_DSSC6_CON (FREQHOPPING_BASE+0x0030)
#define REG_FHCTL_DSSC7_CON (FREQHOPPING_BASE+0x0034)

#define REG_FHCTL0_CFG      (FREQHOPPING_BASE+0x0038)
#define REG_FHCTL0_UPDNLMT  (FREQHOPPING_BASE+0x003C)
#define REG_FHCTL0_DDS      (FREQHOPPING_BASE+0x0040)
#define REG_FHCTL0_DVFS     (FREQHOPPING_BASE+0x0044)
#define REG_FHCTL0_MON      (FREQHOPPING_BASE+0x0048)

#define REG_FHCTL1_CFG      (FREQHOPPING_BASE+0x004C)
#define REG_FHCTL1_UPDNLMT  (FREQHOPPING_BASE+0x0050)
#define REG_FHCTL1_DDS      (FREQHOPPING_BASE+0x0054)
#define REG_FHCTL1_DVFS     (FREQHOPPING_BASE+0x0058)
#define REG_FHCTL1_MON      (FREQHOPPING_BASE+0x005C)

#define REG_FHCTL2_CFG      (FREQHOPPING_BASE+0x0060)
#define REG_FHCTL2_UPDNLMT  (FREQHOPPING_BASE+0x0064)
#define REG_FHCTL2_DDS      (FREQHOPPING_BASE+0x0068)
#define REG_FHCTL2_DVFS     (FREQHOPPING_BASE+0x006C)
#define REG_FHCTL2_MON      (FREQHOPPING_BASE+0x0070)

#define REG_FHCTL3_CFG      (FREQHOPPING_BASE+0x0074)
#define REG_FHCTL3_UPDNLMT  (FREQHOPPING_BASE+0x0078)
#define REG_FHCTL3_DDS      (FREQHOPPING_BASE+0x007C)
#define REG_FHCTL3_DVFS     (FREQHOPPING_BASE+0x0080)
#define REG_FHCTL3_MON      (FREQHOPPING_BASE+0x0084)

#define REG_FHCTL4_CFG      (FREQHOPPING_BASE+0x0088)
#define REG_FHCTL4_UPDNLMT  (FREQHOPPING_BASE+0x008C)
#define REG_FHCTL4_DDS      (FREQHOPPING_BASE+0x0090)
#define REG_FHCTL4_DVFS     (FREQHOPPING_BASE+0x0094)
#define REG_FHCTL4_MON      (FREQHOPPING_BASE+0x0098)

#define REG_FHCTL5_CFG      (FREQHOPPING_BASE+0x009C)
#define REG_FHCTL5_UPDNLMT  (FREQHOPPING_BASE+0x00A0)
#define REG_FHCTL5_DDS      (FREQHOPPING_BASE+0x00A4)
#define REG_FHCTL5_DVFS     (FREQHOPPING_BASE+0x00A8)
#define REG_FHCTL5_MON      (FREQHOPPING_BASE+0x00AC)

#define REG_FHCTL6_CFG      (FREQHOPPING_BASE+0x00B0)
#define REG_FHCTL6_UPDNLMT  (FREQHOPPING_BASE+0x00B4)
#define REG_FHCTL6_DDS      (FREQHOPPING_BASE+0x00B8)
#define REG_FHCTL6_DVFS     (FREQHOPPING_BASE+0x00BC)
#define REG_FHCTL6_MON      (FREQHOPPING_BASE+0x00C0)

#define REG_FHCTL7_CFG      (FREQHOPPING_BASE+0x00C4)
#define REG_FHCTL7_UPDNLMT  (FREQHOPPING_BASE+0x00C8)
#define REG_FHCTL7_DDS      (FREQHOPPING_BASE+0x00CC)
#define REG_FHCTL7_DVFS     (FREQHOPPING_BASE+0x00D0)
#define REG_FHCTL7_MON      (FREQHOPPING_BASE+0x00D4)

#define REG_FHCTL8_CFG      (FREQHOPPING_BASE+0x00D8)
#define REG_FHCTL8_UPDNLMT  (FREQHOPPING_BASE+0x00DC)
#define REG_FHCTL8_DDS      (FREQHOPPING_BASE+0x00E0)
#define REG_FHCTL8_DVFS     (FREQHOPPING_BASE+0x00E4)
#define REG_FHCTL8_MON      (FREQHOPPING_BASE+0x00E8)

#define REG_FHCTL9_CFG      (FREQHOPPING_BASE+0x00EC)
#define REG_FHCTL9_UPDNLMT  (FREQHOPPING_BASE+0x00F0)
#define REG_FHCTL9_DDS      (FREQHOPPING_BASE+0x00F4)
#define REG_FHCTL9_DVFS     (FREQHOPPING_BASE+0x00F8)
#define REG_FHCTL9_MON      (FREQHOPPING_BASE+0x00FC)

#define REG_FHCTL10_CFG      (FREQHOPPING_BASE+0x0100)
#define REG_FHCTL10_UPDNLMT  (FREQHOPPING_BASE+0x0104)
#define REG_FHCTL10_DDS      (FREQHOPPING_BASE+0x0108)
#define REG_FHCTL10_DVFS     (FREQHOPPING_BASE+0x010C)
#define REG_FHCTL10_MON      (FREQHOPPING_BASE+0x0110)

#define REG_FHCTL11_CFG      (FREQHOPPING_BASE+0x0114)
#define REG_FHCTL11_UPDNLMT  (FREQHOPPING_BASE+0x0118)
#define REG_FHCTL11_DDS      (FREQHOPPING_BASE+0x011C)
#define REG_FHCTL11_DVFS     (FREQHOPPING_BASE+0x0120)
#define REG_FHCTL11_MON      (FREQHOPPING_BASE+0x0124)

/* mt2712 */
#define APMIXED_BASE		(apmixed_base)
#define ARMCA7PLL_CON0		(APMIXED_BASE+0x100)
#define ARMCA7PLL_CON1		(APMIXED_BASE+0x104)
#define ARMCA7PLL_PWR_CON0	(APMIXED_BASE+0x110)

#define ARMCA15PLL_CON0		(APMIXED_BASE+0x210)
#define ARMCA15PLL_CON1		(APMIXED_BASE+0x214)
#define ARMCA15PLL_PWR_CON0	(APMIXED_BASE+0x220)

#define MAINPLL_CON0		(APMIXED_BASE+0x230)
#define MAINPLL_CON1		(APMIXED_BASE+0x234)
#define MAINPLL_PWR_CON0	(APMIXED_BASE+0x23C)

#define MMPLL_CON0		(APMIXED_BASE+0x250)
#define MMPLL_CON1		(APMIXED_BASE+0x254)
#define MMPLL_PWR_CON0		(APMIXED_BASE+0x260)

#define MSDCPLL_CON0		(APMIXED_BASE+0x270)
#define MSDCPLL_CON1		(APMIXED_BASE+0x274)
#define MSDCPLL_PWR_CON0	(APMIXED_BASE+0x27C)

#define VENCPLL_CON0		(APMIXED_BASE+0x280)
#define VENCPLL_CON1		(APMIXED_BASE+0x284)
#define VENCPLL_PWR_CON0	(APMIXED_BASE+0x28C)

#define TVDPLL_CON0		(APMIXED_BASE+0x290)
#define TVDPLL_CON1		(APMIXED_BASE+0x294)
#define TVDPLL_PWR_CON0		(APMIXED_BASE+0x29C)

#define VCODECPLL_CON0		(APMIXED_BASE+0x320)
#define VCODECPLL_CON1		(APMIXED_BASE+0x324)
#define VCODECPLL_PWR_CON0	(APMIXED_BASE+0x32C)

#define LVDSPLL_CON0		(APMIXED_BASE+0x370)
#define LVDSPLL_CON1		(APMIXED_BASE+0x374)
#define LVDSPLL_PWR_CON0	(APMIXED_BASE+0x37C)

#define LVDS2PLL_CON0		(APMIXED_BASE+0x390)
#define LVDS2PLL_CON1		(APMIXED_BASE+0x394)
#define LVDS2PLL_PWR_CON0	(APMIXED_BASE+0x39C)

#define MSDC2PLL_CON0		(APMIXED_BASE+0x410)
#define MSDC2PLL_CON1		(APMIXED_BASE+0x414)
#define MSDC2PLL_PWR_CON0	(APMIXED_BASE+0x41C)

/* DDRPHY_PLL = MEMPLL*/
#define DDRPHY_BASE		(ddrphy_base)
#define MEMPLL			(DDRPHY_BASE+0xD9C) /* SHU1_PLL7 */

/* masks */
#define MASK_FRDDSX_DYS         (0xFU<<20)
#define MASK_FRDDSX_DTS         (0xFU<<16)
#define FH_FHCTLX_SRHMODE       (0x1U<<5)
#define FH_SFSTRX_BP            (0x1U<<4)
#define FH_SFSTRX_EN            (0x1U<<2)
#define FH_FRDDSX_EN            (0x1U<<1)
#define FH_FHCTLX_EN            (0x1U<<0)
#define FH_FRDDSX_DNLMT         (0xFFU<<16)
#define FH_FRDDSX_UPLMT         (0xFFU)
#define FH_FHCTLX_PLL_TGL_ORG   (0x1U<<31)
#define FH_FHCTLX_PLL_ORG       (0xFFFFFU)
#define FH_FHCTLX_PAUSE         (0x1U<<31)
#define FH_FHCTLX_PRD           (0x1U<<30)
#define FH_SFSTRX_PRD           (0x1U<<29)
#define FH_FRDDSX_PRD           (0x1U<<28)
#define FH_FHCTLX_STATE         (0xFU<<24)
#define FH_FHCTLX_PLL_CHG       (0x1U<<21)
#define FH_FHCTLX_PLL_DDS       (0xFFFFFU)

#define MASK20b (0x1FFFFFu)
#define BIT31   (1u<<31u)

#define fh_read8(reg)		readb((reg))
#define fh_read16(reg)		readw((reg))
#define fh_read32(reg)		readl((reg))
#define fh_write8(reg, val)	mt_reg_sync_writeb((val), (reg))
#define fh_write16(reg, val)	mt_reg_sync_writew((val), (reg))
#define fh_write32(reg, val)	mt_reg_sync_writel((val), (reg))

static unsigned int uffs(unsigned int mask)
{
	unsigned int shift = 0u;

	if (mask == 0u)
		return 0;

	if ((mask & 0xffffu) == 0u) {
		mask >>= 16u;
		shift += 16u;
	}

	if ((mask & 0xffu) == 0u) {
		mask >>= 8u;
		shift += 8u;
	}

	if ((mask & 0xfu) == 0u) {
		mask >>= 4u;
		shift += 4u;
	}

	if ((mask & 0x3U) == 0U) {
		mask >>= 2u;
		shift += 2u;
	}

	if ((mask & 0x1u) == 0u) {
		mask >>= 1u;
		shift += 1u;
	}

	return shift;
}

static void fh_set_field(void __iomem *fh_reg, unsigned int mask, unsigned int mask_val)
{
	unsigned int tv = fh_read32(fh_reg);

	tv &= ~mask;
	tv |= mask_val << uffs(mask);
	fh_write32(fh_reg, tv);
}

#if 0
#define fh_get_field(reg, field, val) \
	do {	\
		unsigned int tv = fh_read32(reg);	\
		val = ((tv & (field)) >> (uffs((unsigned int)field) - 1)); \
	} while (0)
#endif

/* #define fh_set_bits(reg,bs)	((*(volatile u32*)(reg)) |= (u32)(bs)) */
/* #define fh_clr_bits(reg,bs)	((*(volatile u32*)(reg)) &= ~((u32)(bs))) */

static unsigned int percent_to_ddslmt(unsigned int dDs, unsigned int pERCENT_M10)
{
	return ((dDs * pERCENT_M10) >> 5u) / 100u;
}

static DEFINE_SPINLOCK(g_fh_lock);

static unsigned int g_initialize;

static void __iomem *g_reg_pll_con1[FH_PLL_NUM];
static void __iomem *g_reg_dds[FH_PLL_NUM];
static void __iomem *g_reg_cfg[FH_PLL_NUM];
static void __iomem *g_reg_updnlmt[FH_PLL_NUM];
static void __iomem *g_reg_mon[FH_PLL_NUM];
static void __iomem *g_reg_dvfs[FH_PLL_NUM];
static unsigned int g_slt_fmax[FH_PLL_NUM];
static unsigned int g_slt_fmin[FH_PLL_NUM];
static char g_fh_name[FH_PLL_NUM][20] =	{
	"ARMCA7PLL", "ARMCA15PLL", "MAINPLL", "MEMPLL",
	"MSDCPLL", "MMPLL", "VENCPLL", "TVDPLL",
	"VCODECPLL", "LVDSPLL", "MSDC2PLL", "LVDS2PLL"
};

/* caller: clk mgr */
static void mt_fh_default_conf(void)
{
	FH_MSG_INFO("%s", __func__);
#if 0
	mtk_fhctl_hopping_by_id(FH_MAIN_PLLID, 0x1CC000);
	mtk_fhctl_hopping_by_id(FH_MAIN_PLLID, 0x134000);
	mtk_fhctl_enable_ssc_by_id(FH_MAIN_PLLID);

	mtk_fhctl_hopping_by_id(FH_MM_PLLID, 0x172000);
	mtk_fhctl_hopping_by_id(FH_MM_PLLID, 0x9A000);
	mtk_fhctl_enable_ssc_by_id(FH_MM_PLLID);
	msleep(5000);
	mtk_fhctl_disable_ssc_by_id(FH_MM_PLLID);
	mtk_fhctl_disable_ssc_by_id(100); /* give wrong pll_id on purpose */
#endif
}

static void fh_switch2fhctl(unsigned int pll_id, unsigned int i_control)
{
	unsigned int mask;

	mask = 0x1u << pll_id;

	/* FIXME: clock should be turned on/off at entry functions */
	/* Turn on clock */
	/* if (i_control == 1) */
	/* fh_set_field(REG_FHCTL_CLK_CON, mask, i_control); */

	/* Release software reset */
	/* fh_set_field(REG_FHCTL_RST_CON, mask, 0); */

	/* Switch to FHCTL_CORE controller */
	fh_set_field(REG_FHCTL_HP_EN, mask, i_control);

	/* Turn off clock */
	/* if (i_control == 0) */
	/* fh_set_field(REG_FHCTL_CLK_CON, mask, i_control); */
}

static void fh_sync_ncpo_to_fhctl_dds(unsigned int pll_id)
{
	unsigned int pll_con1;

	pll_con1 = fh_read32(g_reg_pll_con1[pll_id]);

	if (pll_id != FH_MEM_PLLID) {
		/* MT2712: To apmixed: we only extract significant bits(30:10) from apmixed's pll to
		 * fhctl dds register.
		 */
		pll_con1 = pll_con1 >> 10;
	} else {
		/* MT2712: To mempll: MEMPLL uses high 16 bits(31:16). Therefore, we extract its 31:16 bits.
		 * However, we still need to shift left 5 times for matching
		 * FHCTL_DDS format (FHCTL_DDS uses 20:0 bits)
		 */
		pll_con1 = (pll_con1 >> 16) << 5;
	}

	fh_write32(g_reg_dds[pll_id], (pll_con1 & MASK20b) | BIT31);
}

static void __enable_ssc(unsigned int pll_id)
{
	unsigned long flags = 0;
	void __iomem *reg_cfg = g_reg_cfg[pll_id];
	void __iomem *reg_updnlmt = g_reg_updnlmt[pll_id];
	void __iomem *reg_dds = g_reg_dds[pll_id];

	FH_MSG_INFO("Calling %s", __func__);

	spin_lock_irqsave(&g_fh_lock, flags);

	/* Set the relative parameter registers (dt/df/upbnd/downbnd) */
	fh_set_field(reg_cfg, MASK_FRDDSX_DYS, 0x0);
	fh_set_field(reg_cfg, MASK_FRDDSX_DTS, 0x9);

	fh_sync_ncpo_to_fhctl_dds(pll_id);

	/* TODO: Not setting upper due to they are all 0? */
	/* Set downlimit 100% ~ 92% */
	fh_write32(reg_updnlmt,
		   (percent_to_ddslmt((fh_read32(reg_dds) & MASK20b), 8u) << 16u));

	/* Switch to FHCTL */
	fh_switch2fhctl(pll_id, 1);

	/* Enable SSC */
	fh_set_field(reg_cfg, FH_FRDDSX_EN, 1);
	/* Enable Hopping control */
	fh_set_field(reg_cfg, FH_FHCTLX_EN, 1);

	spin_unlock_irqrestore(&g_fh_lock, flags);
}

static void __disable_ssc(unsigned int pll_id)
{
	unsigned long flags = 0;
	void __iomem *reg_cfg = g_reg_cfg[pll_id];

	FH_MSG_INFO("Calling %s", __func__);

	spin_lock_irqsave(&g_fh_lock, flags);

	/* Set the relative registers */
	fh_set_field(reg_cfg, FH_FRDDSX_EN, 0);
	fh_set_field(reg_cfg, FH_FHCTLX_EN, 0);
	fh_switch2fhctl(pll_id, 0);

	spin_unlock_irqrestore(&g_fh_lock, flags);

}

/* reg_mon reflects the value of reg_dds */
static int wait_dds_stable(unsigned int target_dds, void __iomem *reg_mon, unsigned int wait_count)
{
	unsigned int fh_dds = 0;
	unsigned int count = 0;

	fh_dds = fh_read32(reg_mon) & MASK20b;
	while ((target_dds != fh_dds) && (count < wait_count)) {
		udelay(10);

		fh_dds = (fh_read32(reg_mon)) & MASK20b;
		++count;
		FH_MSG_INFO("target_dds = 0x%08x, fh_dds = 0x%08x, i = %d\n", target_dds, fh_dds, count);
	}

	if (count >= wait_count) {
		FH_MSG_ERROR("[ERROR]fh_dds cannot reach target_dds\n");
		FH_MSG_ERROR("[ERROR]target_dds = 0x%08x, fh_dds = 0x%08x, i = %d\n", target_dds, fh_dds, count);
		return -1;
	}

	FH_MSG_INFO("target_dds = 0x%08x, fh_dds = 0x%08x, i = %d\n", target_dds, fh_dds, count);

	return 0;
}

/* #define UINT_MAX (unsigned int)(-1) */
static int mt_fh_dumpregs_proc_read(struct seq_file *m, void *data)
{
	unsigned int i;

	for (i = 0; i < FH_PLL_NUM; ++i) {
		/* mt2712  doesn't need to support MEMPLL. Therefore, ignore MEMPLL setting. */
		if (i == FH_MEM_PLLID)
			continue;

		seq_puts(m, "\t CFG\t    UPDNLMT    DDS\t  DVFS       MON\r\n");
		seq_printf(m, "FHCTL%2d: 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\r\n", i,
			fh_read32(g_reg_cfg[i]), fh_read32(g_reg_updnlmt[i]),
			fh_read32(g_reg_dds[i]), fh_read32(g_reg_dvfs[i]),
			fh_read32(g_reg_mon[i]));
	}

	seq_printf(m, "\r\nFHCTL_HP_EN:\r\n0x%08x\r\n", fh_read32(REG_FHCTL_HP_EN));

	seq_puts(m, "\r\nPLL_CON0 :\r\n");
	seq_printf(m, "ARMCA7:0x%08x\nARMCA15:0x%08x\nMAIN:0x%08x\n",
		   fh_read32(ARMCA7PLL_CON0), fh_read32(ARMCA15PLL_CON0),
		   fh_read32(MAINPLL_CON0));
	seq_printf(m, "MSDC:0x%08x\nMM:0x%08x\nVENC:0x%08x\nTVD:0x%08x\nVCODEC:0x%08x\n",
		   fh_read32(MSDCPLL_CON0), fh_read32(MMPLL_CON0),
		   fh_read32(VENCPLL_CON0), fh_read32(TVDPLL_CON0), fh_read32(VCODECPLL_CON0));
	seq_printf(m, "LVDS:0x%08x\nMSDC2:0x%08x\nLVDS2:0x%08x\r\n",
		   fh_read32(LVDSPLL_CON0), fh_read32(MSDC2PLL_CON0), fh_read32(LVDS2PLL_CON0));

	seq_puts(m, "\r\nPLL_CON1 :\r\n");
	seq_printf(m, "ARMCA7:0x%08x\nARMCA15:0x%08x\nMAIN:0x%08x\n",
		   fh_read32(ARMCA7PLL_CON1), fh_read32(ARMCA15PLL_CON1),
		   fh_read32(MAINPLL_CON1));
	seq_printf(m, "MSDC:0x%08x\nMM:0x%08x\nVENC:0x%08x\nTVD:0x%08x\nVCODEC:0x%08x\n",
		   fh_read32(MSDCPLL_CON1), fh_read32(MMPLL_CON1),
		   fh_read32(VENCPLL_CON1), fh_read32(TVDPLL_CON1), fh_read32(VCODECPLL_CON1));
	seq_printf(m, "LVDS:0x%08x\nMSDC2:0x%08x\nLVDS2:0x%08x\r\n",
		   fh_read32(LVDSPLL_CON1), fh_read32(MSDC2PLL_CON1), fh_read32(LVDS2PLL_CON1));

	return 0;
}

static void mt_fh_init_register(void)
{
	g_reg_pll_con1[FH_ARMCA7_PLLID] = ARMCA7PLL_CON1;
	g_reg_dds[FH_ARMCA7_PLLID] = REG_FHCTL0_DDS;
	g_reg_cfg[FH_ARMCA7_PLLID] = REG_FHCTL0_CFG;
	g_reg_updnlmt[FH_ARMCA7_PLLID] = REG_FHCTL0_UPDNLMT;
	g_reg_mon[FH_ARMCA7_PLLID] = REG_FHCTL0_MON;
	g_reg_dvfs[FH_ARMCA7_PLLID] = REG_FHCTL0_DVFS;
	g_slt_fmax[FH_ARMCA7_PLLID] = 0xF8000;
	g_slt_fmin[FH_ARMCA7_PLLID] = 0x9A000;

	g_reg_pll_con1[FH_ARMCA15_PLLID] = ARMCA15PLL_CON1;
	g_reg_dds[FH_ARMCA15_PLLID] = REG_FHCTL1_DDS;
	g_reg_cfg[FH_ARMCA15_PLLID] = REG_FHCTL1_CFG;
	g_reg_updnlmt[FH_ARMCA15_PLLID] = REG_FHCTL1_UPDNLMT;
	g_reg_mon[FH_ARMCA15_PLLID] = REG_FHCTL1_MON;
	g_reg_dvfs[FH_ARMCA15_PLLID] = REG_FHCTL1_DVFS;
	g_slt_fmax[FH_ARMCA15_PLLID] = 0x172000;
	g_slt_fmin[FH_ARMCA15_PLLID] = 0xCC000;

	g_reg_pll_con1[FH_MAIN_PLLID] = MAINPLL_CON1;
	g_reg_dds[FH_MAIN_PLLID] = REG_FHCTL2_DDS;
	g_reg_cfg[FH_MAIN_PLLID] = REG_FHCTL2_CFG;
	g_reg_updnlmt[FH_MAIN_PLLID] = REG_FHCTL2_UPDNLMT;
	g_reg_mon[FH_MAIN_PLLID] = REG_FHCTL2_MON;
	g_reg_dvfs[FH_MAIN_PLLID] = REG_FHCTL2_DVFS;
	g_slt_fmax[FH_MAIN_PLLID] = 0x1CC000;
	g_slt_fmin[FH_MAIN_PLLID] = 0x134000;

	g_reg_pll_con1[FH_MEM_PLLID] = MEMPLL; /* FHCTL doesn't need to support MEMPLL */
	g_reg_dds[FH_MEM_PLLID] = REG_FHCTL3_DDS;
	g_reg_cfg[FH_MEM_PLLID] = REG_FHCTL3_CFG;
	g_reg_updnlmt[FH_MEM_PLLID] = REG_FHCTL3_UPDNLMT;
	g_reg_mon[FH_MEM_PLLID] = REG_FHCTL3_MON;
	g_reg_dvfs[FH_MEM_PLLID] = REG_FHCTL3_DVFS;
	/* MEMPLL doesn't have its DVFS. No need to test hopping */
	g_slt_fmax[FH_MEM_PLLID] = 0x0;
	g_slt_fmin[FH_MEM_PLLID] = 0x0;

	g_reg_pll_con1[FH_MSDC_PLLID] = MSDCPLL_CON1;
	g_reg_dds[FH_MSDC_PLLID] = REG_FHCTL4_DDS;
	g_reg_cfg[FH_MSDC_PLLID] = REG_FHCTL4_CFG;
	g_reg_updnlmt[FH_MSDC_PLLID] = REG_FHCTL4_UPDNLMT;
	g_reg_mon[FH_MSDC_PLLID] = REG_FHCTL4_MON;
	g_reg_dvfs[FH_MSDC_PLLID] = REG_FHCTL4_DVFS;
	g_slt_fmax[FH_MSDC_PLLID] = 0x134000;
	g_slt_fmin[FH_MSDC_PLLID] = 0x9C000;

	g_reg_pll_con1[FH_MM_PLLID] = MMPLL_CON1;
	g_reg_dds[FH_MM_PLLID] = REG_FHCTL5_DDS;
	g_reg_cfg[FH_MM_PLLID] = REG_FHCTL5_CFG;
	g_reg_updnlmt[FH_MM_PLLID] = REG_FHCTL5_UPDNLMT;
	g_reg_mon[FH_MM_PLLID] = REG_FHCTL5_MON;
	g_reg_dvfs[FH_MM_PLLID] = REG_FHCTL5_DVFS;
	g_slt_fmax[FH_MM_PLLID] = 0x172000;
	g_slt_fmin[FH_MM_PLLID] = 0x9A000;

	g_reg_pll_con1[FH_VENC_PLLID] = VENCPLL_CON1;
	g_reg_dds[FH_VENC_PLLID] = REG_FHCTL6_DDS;
	g_reg_cfg[FH_VENC_PLLID] = REG_FHCTL6_CFG;
	g_reg_updnlmt[FH_VENC_PLLID] = REG_FHCTL6_UPDNLMT;
	g_reg_mon[FH_VENC_PLLID] = REG_FHCTL6_MON;
	g_reg_dvfs[FH_VENC_PLLID] = REG_FHCTL6_DVFS;
	g_slt_fmax[FH_VENC_PLLID] = 0x132000;
	g_slt_fmin[FH_VENC_PLLID] = 0x9A000;

	g_reg_pll_con1[FH_TVD_PLLID] = TVDPLL_CON1;
	g_reg_dds[FH_TVD_PLLID] = REG_FHCTL7_DDS;
	g_reg_cfg[FH_TVD_PLLID] = REG_FHCTL7_CFG;
	g_reg_updnlmt[FH_TVD_PLLID] = REG_FHCTL7_UPDNLMT;
	g_reg_mon[FH_TVD_PLLID] = REG_FHCTL7_MON;
	g_reg_dvfs[FH_TVD_PLLID] = REG_FHCTL7_DVFS;
	g_slt_fmax[FH_TVD_PLLID] = 0x144000;
	g_slt_fmin[FH_TVD_PLLID] = 0xB8000;

	g_reg_pll_con1[FH_VCODEC_PLLID] = VCODECPLL_CON1;
	g_reg_dds[FH_VCODEC_PLLID] = REG_FHCTL8_DDS;
	g_reg_cfg[FH_VCODEC_PLLID] = REG_FHCTL8_CFG;
	g_reg_updnlmt[FH_VCODEC_PLLID] = REG_FHCTL8_UPDNLMT;
	g_reg_mon[FH_VCODEC_PLLID] = REG_FHCTL8_MON;
	g_reg_dvfs[FH_VCODEC_PLLID] = REG_FHCTL8_DVFS;
	g_slt_fmax[FH_VCODEC_PLLID] = 0x14C000;
	g_slt_fmin[FH_VCODEC_PLLID] = 0x9C000;

	g_reg_pll_con1[FH_LVDS_PLLID] = LVDSPLL_CON1;
	g_reg_dds[FH_LVDS_PLLID] = REG_FHCTL9_DDS;
	g_reg_cfg[FH_LVDS_PLLID] = REG_FHCTL9_CFG;
	g_reg_updnlmt[FH_LVDS_PLLID] = REG_FHCTL9_UPDNLMT;
	g_reg_mon[FH_LVDS_PLLID] = REG_FHCTL9_MON;
	g_reg_dvfs[FH_LVDS_PLLID] = REG_FHCTL9_DVFS;
	g_slt_fmax[FH_LVDS_PLLID] = 0x132000;
	g_slt_fmin[FH_LVDS_PLLID] = 0x9A000;

	g_reg_pll_con1[FH_MSDC2_PLLID] = MSDC2PLL_CON1;
	g_reg_dds[FH_MSDC2_PLLID] = REG_FHCTL10_DDS;
	g_reg_cfg[FH_MSDC2_PLLID] = REG_FHCTL10_CFG;
	g_reg_updnlmt[FH_MSDC2_PLLID] = REG_FHCTL10_UPDNLMT;
	g_reg_mon[FH_MSDC2_PLLID] = REG_FHCTL10_MON;
	g_reg_dvfs[FH_MSDC2_PLLID] = REG_FHCTL10_DVFS;
	g_slt_fmax[FH_MSDC2_PLLID] = 0x134000;
	g_slt_fmin[FH_MSDC2_PLLID] = 0x9C000;

	g_reg_pll_con1[FH_LVDS2_PLLID] = LVDS2PLL_CON1;
	g_reg_dds[FH_LVDS2_PLLID] = REG_FHCTL11_DDS;
	g_reg_cfg[FH_LVDS2_PLLID] = REG_FHCTL11_CFG;
	g_reg_updnlmt[FH_LVDS2_PLLID] = REG_FHCTL11_UPDNLMT;
	g_reg_mon[FH_LVDS2_PLLID] = REG_FHCTL11_MON;
	g_reg_dvfs[FH_LVDS2_PLLID] = REG_FHCTL11_DVFS;
	g_slt_fmax[FH_LVDS2_PLLID] = 0x132000;
	g_slt_fmin[FH_LVDS2_PLLID] = 0x9A000;
}

/* TODO: __init void mt_freqhopping_init(void) */
static int mt_fh_init(void)
{
	unsigned int i, mask;
	unsigned long flags = 0;
	struct device_node *freqhopping_node = NULL;
	struct device_node *apmix_node = NULL;

	FH_MSG_INFO("%s", __func__);

	if (g_initialize == 1u) {
		FH_MSG_ERROR("fh driver has ever been initialized successfully\n");
		return -EPERM;
	}

	freqhopping_node = of_find_compatible_node(NULL, NULL, "mediatek,mt2712-fhctl");
	if (freqhopping_node == NULL) {
		FH_MSG_ERROR("node \"mediatek,mt2712-fhctl\" not found!\n");
		return -ENODEV;
	}

	/* Setup IO addresses */
	freqhopping_base = of_iomap(freqhopping_node, 0);
	if (freqhopping_base == NULL) {
		FH_MSG_ERROR("Cannot get freqhopping_base from freqhopping_node\n");
		return -EFAULT;
	}

	apmix_node = of_find_compatible_node(NULL, NULL, "mediatek,mt2712-apmixedsys");
	if (apmix_node == NULL) {
		FH_MSG_ERROR("node \"mediatek,mt2712-apmixedsys\" not found!\n");
		return -ENODEV;
	}

	/* Setup IO addresses */
	apmixed_base = of_iomap(apmix_node, 0);
	if (apmixed_base == NULL) {
		FH_MSG_ERROR("Cannot get apmixed_base from apmix_node");
		return -EFAULT;
	}

	mt_fh_init_register();

	for (i = 0; i < FH_PLL_NUM; ++i) {
		mask = 0x1u << i;

		spin_lock_irqsave(&g_fh_lock, flags);

		/* TODO: clock should be turned on only when FH is needed */
		/* Turn on all clock */
		fh_set_field(REG_FHCTL_CLK_CON, mask, 1);

		/* Release software-reset to reset */
		fh_set_field(REG_FHCTL_RST_CON, mask, 0);
		fh_set_field(REG_FHCTL_RST_CON, mask, 1);

		fh_write32(g_reg_cfg[i], 0x00000000);		/* No SSC and FH enabled */
		fh_write32(g_reg_updnlmt[i], 0x00000000);	/* clear all the settings */
		fh_write32(g_reg_dds[i], 0x00000000);		/* clear all the settings */

		spin_unlock_irqrestore(&g_fh_lock, flags);
	}

	g_initialize = 1u;

	return 0;
}

static void print_fhctl_reg(const char *type, const char *regname, void __iomem *addr)
{
	FH_MSG_NOTICE("[%7s] %-23s: [0x%p]: 0x%08x\n", type, regname, addr, fh_read32(addr));
}

#define DUMP(type, regname)	print_fhctl_reg(type, #regname, regname)

static void print_fhctl_register(unsigned int fh_pll_id, char *fh_pll_name)
{
	DUMP(fh_pll_name, g_reg_pll_con1[fh_pll_id]);
	DUMP(fh_pll_name, g_reg_dds[fh_pll_id]);
	DUMP(fh_pll_name, g_reg_cfg[fh_pll_id]);
	DUMP(fh_pll_name, g_reg_updnlmt[fh_pll_id]);
	DUMP(fh_pll_name, g_reg_mon[fh_pll_id]);
	DUMP(fh_pll_name, g_reg_dvfs[fh_pll_id]);
	FH_MSG_NOTICE("[%7s] g_slt_fmin = 0x%08x\n", fh_pll_name, g_slt_fmin[fh_pll_id]);
	FH_MSG_NOTICE("[%7s] g_slt_fmax = 0x%08x\n", fh_pll_name, g_slt_fmax[fh_pll_id]);
}

/* User needs to set its pll frequency first and use this API to turn on
 * corresponding frequency hopping SSC.
 */
int mtk_fhctl_enable_ssc_by_id(unsigned int fh_pll_id)
{
	if (g_initialize != 1u) {
		FH_MSG_ERROR("fh driver isn't initialized successfully\n");
		return -EPERM;
	}

	if (fh_pll_id > FH_MAX_PLLID) {
		FH_MSG_ERROR("unknown pll_id = %d\n", fh_pll_id);
		return -EINVAL;
	} else if (fh_pll_id == FH_MEM_PLLID) {
		FH_MSG_ERROR("MEMPLL is not supported\n");
		return -EINVAL;
	} else {
		; /* do nothing */
	}

	__enable_ssc(fh_pll_id);

	return 0;
}
EXPORT_SYMBOL(mtk_fhctl_enable_ssc_by_id);

int mtk_fhctl_disable_ssc_by_id(unsigned int fh_pll_id)
{
	if (g_initialize != 1u) {
		FH_MSG_ERROR("fh driver isn't initialized successfully\n");
		return -EPERM;
	}

	if (fh_pll_id > FH_MAX_PLLID) {
		FH_MSG_ERROR("unknown pll_id = %d\n", fh_pll_id);
		return -EINVAL;
	} else if (fh_pll_id == FH_MEM_PLLID) {
		FH_MSG_ERROR("MEMPLL is not supported\n");
		return -EINVAL;
	} else {
		; /* do nothing */
	}

	__disable_ssc(fh_pll_id);

	return 0;
}
EXPORT_SYMBOL(mtk_fhctl_disable_ssc_by_id);

/* This API helps hopping PLL to your target vco frequency via fhctl*/
int mtk_fhctl_hopping_by_id(unsigned int fh_pll_id, unsigned int target_vco_frequency)
{
	int ret;
	unsigned int fh_vco_freq;
	unsigned long flags;

	if (g_initialize != 1u) {
		FH_MSG_ERROR("fh driver isn't initialized successfully\n");
		return -EPERM;
	}

	if (fh_pll_id > FH_MAX_PLLID) {
		FH_MSG_ERROR("unknown pll_id = %d\n", fh_pll_id);
		return -EINVAL;
	} else if (fh_pll_id == FH_MEM_PLLID) {
		FH_MSG_ERROR("MEMPLL is not supported\n");
		return -EINVAL;
	} else {
		; /* do nothing */
	}

	/* Disable corresponding FHCTL SSC first. */
	__disable_ssc(fh_pll_id);

	spin_lock_irqsave(&g_fh_lock, flags);

	fh_sync_ncpo_to_fhctl_dds(fh_pll_id);			/* sync ncpo to DDS of FHCTL */

	fh_set_field(g_reg_cfg[fh_pll_id], FH_SFSTRX_EN, 1);	/* enable dvfs mode */
	fh_set_field(g_reg_cfg[fh_pll_id], FH_FHCTLX_EN, 1);	/* enable hopping control */

	/* for slope setting. (hardcode) */
	fh_write32(REG_FHCTL_SLOPE0, 0x6003c97);

	fh_switch2fhctl(fh_pll_id, 1); /* Switch control back to FHCLT */

	/* FH_MSG_NOTICE("[%s]DDS: 0x%08x\n", g_fh_name[fh_pll_id], (fh_read32(g_reg_dds[fh_pll_id]) & MASK20b)); */
	fh_write32(g_reg_dvfs[fh_pll_id], (target_vco_frequency) | (BIT31));	/* set hopping dvfs */
	/* FH_MSG_NOTICE("[%s]DVFS: 0x%08x\n", g_fh_name[fh_pll_id], (fh_read32(g_reg_dvfs[fh_pll_id]) & MASK20b)); */

	/* FH_MSG_NOTICE("ensure jump to target DDS\n"); */
	ret = wait_dds_stable(target_vco_frequency, g_reg_mon[fh_pll_id], 1000);
	if (ret == -1)
		goto freqhopping_done;

	/* MT2712: APMIXED PLL uses bit 30:0 but FHCTL_MON uses 20:0.
	 * So we need to shift left FHCTL_MON 10 bits to match APMIXEDPLL
	 * register format (bits 30:0 are PLL bits)
	 */
	fh_vco_freq = ((fh_read32(g_reg_mon[fh_pll_id]) & MASK20b) << 10) | (BIT31);
	/* FH_MSG_NOTICE("fh_vco_freq: 0x%08x\n", fh_vco_freq); */
	fh_write32(g_reg_pll_con1[fh_pll_id], fh_vco_freq); /* write back to ncpo */

freqhopping_done:
	fh_set_field(g_reg_cfg[fh_pll_id], FH_SFSTRX_EN, 0);	/* disable dvfs mode */
	fh_set_field(g_reg_cfg[fh_pll_id], FH_FHCTLX_EN, 0);	/* disable hopping control */
	fh_switch2fhctl(fh_pll_id, 0); /* Switch control back to PLL */

	spin_unlock_irqrestore(&g_fh_lock, flags);

	return ret;
}
EXPORT_SYMBOL(mtk_fhctl_hopping_by_id);

static int test_freqhopping(unsigned int fh_pll_id, char *fh_pll_name)
{
	int ret = 0; /* slt pass flag */
	unsigned long flags = 0;

	FH_MSG_NOTICE("%s() start.\n", __func__);

	if (g_slt_fmax[fh_pll_id] == 0u || g_slt_fmin[fh_pll_id] == 0u) {
		FH_MSG_ERROR("[Notice][%s]un-define g_slt_fmax = 0x%x, g_slt_fmin = 0x%x\n",
						fh_pll_name, g_slt_fmax[fh_pll_id], g_slt_fmin[fh_pll_id]);
		return 0;
	}

	/* Disable corresponding FHCTL SSC first. */
	__disable_ssc(fh_pll_id);

	spin_lock_irqsave(&g_fh_lock, flags);
	print_fhctl_register(fh_pll_id, fh_pll_name);

	FH_MSG_NOTICE("1. sync ncpo to DDS of FHCTL\n");
	fh_sync_ncpo_to_fhctl_dds(fh_pll_id);			/* set dds */

	FH_MSG_NOTICE("2. enable DVFS and Hopping control\n");
	fh_set_field(g_reg_cfg[fh_pll_id], FH_SFSTRX_EN, 1);	/* enable dvfs mode */
	fh_set_field(g_reg_cfg[fh_pll_id], FH_FHCTLX_EN, 1);	/* enable hopping control */

	/* for slope setting. (hardcode) */
	fh_write32(REG_FHCTL_SLOPE0, 0x6003c97);

	FH_MSG_NOTICE("3. switch to hopping control(do nothing here)\n");
	/* fh_switch2fhctl(pll_id, 1); */

	FH_MSG_NOTICE("4. set hopping Fmin to DVFS and check DDS\n");
	FH_MSG_NOTICE("[%s]DDS: 0x%08x\n", fh_pll_name, (fh_read32(g_reg_dds[fh_pll_id]) & MASK20b));
	fh_write32(g_reg_dvfs[fh_pll_id], (g_slt_fmin[fh_pll_id]) | (BIT31));	/* set hopping dvfs */
	FH_MSG_NOTICE("[%s]DVFS: 0x%08x\n", fh_pll_name, (fh_read32(g_reg_dvfs[fh_pll_id]) & MASK20b));

	FH_MSG_NOTICE("4.1 ensure jump to target DDS");
	ret = wait_dds_stable(g_slt_fmin[fh_pll_id], g_reg_mon[fh_pll_id], 1000);
	if (ret == -1)
		goto freqhopping_test_done;

	FH_MSG_NOTICE("4.2 set hopping Fmax to DVFS and check DDS\n");
	FH_MSG_NOTICE("[%s]DDS: 0x%08x\n", fh_pll_name, (fh_read32(g_reg_dds[fh_pll_id]) & MASK20b));
	fh_write32(g_reg_dvfs[fh_pll_id], (g_slt_fmax[fh_pll_id]) | (BIT31));	/* set hopping dvfs */
	FH_MSG_NOTICE("[%s]DVFS: 0x%08x\n", fh_pll_name, (fh_read32(g_reg_dvfs[fh_pll_id]) & MASK20b));

	FH_MSG_NOTICE("4.3 ensure jump to target DDS");
	ret = wait_dds_stable(g_slt_fmax[fh_pll_id], g_reg_mon[fh_pll_id], 1000);
	if (ret == -1)
		goto freqhopping_test_done;

	FH_MSG_NOTICE("4.4 set hopping Fmin to DVFS and check DDS\n");
	FH_MSG_NOTICE("[%s]DDS: 0x%08x\n", fh_pll_name, (fh_read32(g_reg_dds[fh_pll_id]) & MASK20b));
	fh_write32(g_reg_dvfs[fh_pll_id], (g_slt_fmin[fh_pll_id]) | (BIT31));	/* set hopping dvfs */
	FH_MSG_NOTICE("[%s]DVFS: 0x%08x\n", fh_pll_name, (fh_read32(g_reg_dvfs[fh_pll_id]) & MASK20b));

	FH_MSG_NOTICE("4.5 ensure jump to target DDS");
	ret = wait_dds_stable(g_slt_fmin[fh_pll_id], g_reg_mon[fh_pll_id], 1000);
	if (ret == -1)
		goto freqhopping_test_done;

freqhopping_test_done:
	FH_MSG_NOTICE("5. disable DVFS and Hopping control\n");
	fh_set_field(g_reg_cfg[fh_pll_id], FH_SFSTRX_EN, 0);	/* disable dvfs mode */
	fh_set_field(g_reg_cfg[fh_pll_id], FH_FHCTLX_EN, 0);	/* disable hopping control */

	print_fhctl_register(fh_pll_id, fh_pll_name);
	spin_unlock_irqrestore(&g_fh_lock, flags);

	FH_MSG_NOTICE("%s() done.\n\n", __func__);
	return ret;
}

static int test_ssc(unsigned int fh_pll_id, char *fh_pll_name)
{
	int ret = 0; /* slt pass flag */
	unsigned long flags = 0;
	unsigned int last_mon = 0, dds_value, dds_downlimit, mon_value;
	int count, same_with_last_mon = 0;

	FH_MSG_NOTICE("%s() start.\n", __func__);

	spin_lock_irqsave(&g_fh_lock, flags);
	print_fhctl_register(fh_pll_id, fh_pll_name);

	/* Set the DTS/DYS (hardcode) */
	fh_set_field(g_reg_cfg[fh_pll_id], MASK_FRDDSX_DTS, 0x0);
	fh_set_field(g_reg_cfg[fh_pll_id], MASK_FRDDSX_DYS, 0x9);

	fh_sync_ncpo_to_fhctl_dds(fh_pll_id);

	dds_value = fh_read32(g_reg_dds[fh_pll_id]) & MASK20b;
	dds_downlimit = (unsigned int)((dds_value * 92u) / 100u); /* SSC between 92% and 100%  */

	FH_MSG_NOTICE("dds_value = 0x%x, dds_downlimit = 0x%x\n", dds_value, dds_downlimit);

	/* Setting ssc downlimit */
	fh_write32(g_reg_updnlmt[fh_pll_id],
			(percent_to_ddslmt((fh_read32(g_reg_dds[fh_pll_id]) & MASK20b), 8u) << 16u));

	FH_MSG_NOTICE("Enable DVFS and Hopping control\n");
	fh_set_field(g_reg_cfg[fh_pll_id], FH_FRDDSX_EN, 1); /* Enable SSC */
	fh_set_field(g_reg_cfg[fh_pll_id], FH_FHCTLX_EN, 1); /* Enable Hopping control */

	/* Test SSC */
	for (count = 0; count <= 3000; count++) {
		mon_value = fh_read32(g_reg_mon[fh_pll_id]) & MASK20b;

		if (mon_value > dds_value || mon_value < dds_downlimit) {
			FH_MSG_ERROR("[ERROR][%s] ssc out of range\n", fh_pll_name);
			FH_MSG_ERROR("[ERROR]dds_value = 0x%x, mon_value = 0x%x, dds_downlimit = 0x%x\n",
									dds_value, mon_value, dds_downlimit);
			ret = -1;
			goto ssc_test_done;
		}

		if (last_mon == mon_value)
			same_with_last_mon++;
		else
			same_with_last_mon = 0;

		last_mon = mon_value;
	}

	FH_MSG_NOTICE("same_with_last_mon = %d\n", same_with_last_mon);
	if (same_with_last_mon >= 20) {
		FH_MSG_ERROR("[ERROR]reg_mon doesn't change often. Should consult this phenomenon with DE..\n");
		ret = -1;
		goto ssc_test_done;
	}

ssc_test_done:
	FH_MSG_NOTICE("Disable DVFS and Hopping control\n");
	fh_set_field(g_reg_cfg[fh_pll_id], FH_FRDDSX_EN, 0); /* Disable SSC */
	fh_set_field(g_reg_cfg[fh_pll_id], FH_FHCTLX_EN, 0); /* Disable Hopping control */

	print_fhctl_register(fh_pll_id, fh_pll_name);
	spin_unlock_irqrestore(&g_fh_lock, flags);

	FH_MSG_NOTICE("%s() done.\n\n", __func__);

	return ret;
}

static int mt_fh_slt_start(void)
{
	int ret = 0; /* slt pass flag */
	unsigned int fh_pll_id;

	FH_MSG_NOTICE("%s() start\n", __func__);

	for (fh_pll_id = FH_ARMCA7_PLLID; fh_pll_id < FH_PLL_NUM; fh_pll_id++) {
		/* FHCLT doesn't need to control MEMPLL's SSC & hopping.
		 * Dram driver can do it on its own.
		 */
		if (fh_pll_id == FH_MEM_PLLID)
			continue;

		ret = test_freqhopping(fh_pll_id, g_fh_name[fh_pll_id]);
		if (ret == -1) {
			FH_MSG_ERROR("test_freqhopping failed\n");
			goto slt_test_result;
		}

		ret = test_ssc(fh_pll_id, g_fh_name[fh_pll_id]);
		if (ret == -1) {
			FH_MSG_ERROR("test_ssc failed\n");
			goto slt_test_result;
		}
	}

slt_test_result:
	FH_MSG_NOTICE("%s() done\n", __func__);

	return ret;
}

static struct mt_fh_hal_driver g_fh_hal_drv = {
	.mt_fh_hal_dumpregs_read = mt_fh_dumpregs_proc_read,
	.mt_fh_hal_init = mt_fh_init,
	.mt_fh_hal_default_conf = mt_fh_default_conf,
	.mt_fh_hal_slt_start = mt_fh_slt_start,
};

struct mt_fh_hal_driver *mt_get_fh_hal_drv(void)
{
	return &g_fh_hal_drv;
}

