/*
 * Copyright (C) 2015 MediaTek Inc.
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
#include "mtk_freqhopping.h"
#include "mtk_fhreg.h"
#include "sync_write.h"
#include "mtk_freqhopping_drv.h"
#include <linux/seq_file.h>
#include <linux/of_address.h>
/***********************************/
/* Other global variable           */
/***********************************/
static unsigned int g_initialize;	/* [True]: Init done */
static DEFINE_SPINLOCK(g_fh_lock);


/*********************************/
/* FHCTL related IP base address */
/*********************************/

static void __iomem *g_fhctl_base;
static void __iomem *g_apmixed_base;
static void __iomem *g_ddrphy_base;
static void __iomem *g_spm_base;

/*********************************/
/* Utility Macro */
/*********************************/
#define MASK21b (0x1FFFFF)
#define MASK22b (0x3FFFFF)
#define BIT32   (1U<<31)

#define VALIDATE_PLLID(id) WARN_ON(id >= FH_PLL_NUM)
#define VALIDATE_DDS(dds)  WARN_ON(dds > 0x1FFFFF)
#define PERCENT_TO_DDSLMT(dDS, pERCENT_M10) (((dDS * pERCENT_M10) >> 5) / 100)

/*********************************/
/* FHCTL PLL Setting ID */
/*********************************/
#define PLL_SETTING_IDX__USER	(0x9)	/* Magic number, no any special indication */
#define PLL_SETTING_IDX__DEF    (0x1)	/* Default Setting, Magic number, indicate table position 1. */


/*********************************/
/* Track the status of all FHCTL PLL */
/*********************************/
static struct fh_pll_t g_fh_pll[FH_PLL_NUM] = { };	/* init during run time. */


/*********************************/
/* FHCTL PLL name                */
/*********************************/
static const char *g_pll_name[FH_PLL_NUM] = {
	"ARMPLL",
	"MAINPLL",
	"MSDCPLL",
	"MFGPLL",
	"MEMPLL",
	"MMPLL"
};

/*********************************/
/* FHCTL PLL SSC Setting Table   */
/*********************************/
#define UNINIT_DDS   0x0
#define MAX_DDS      0x1fffff	/* 21 bit */

/* Should be setting according to HQA de-sense result.  */
static const int g_pll_ssc_init_tbl[FH_PLL_NUM] = {
	/*
	 *  [FH_SSC_DEF_DISABLE]: Default SSC disable,
	 *  [FH_SSC_DEF_ENABLE_SSC]: Default enable SSC.
	 */
	FH_SSC_DEF_DISABLE,	/* FHCTL PLL0 */
	FH_SSC_DEF_DISABLE,	/* FHCTL PLL1 */
	FH_SSC_DEF_DISABLE,	/* FHCTL PLL2 */
	FH_SSC_DEF_DISABLE,	/* FHCTL PLL3 */
	FH_SSC_DEF_DYNAMIC_SSC,	/* FHCTL PLL4 */
	FH_SSC_DEF_DISABLE	/* FHCTL PLL5 */
};

static const struct freqhopping_ssc g_pll_ssc_setting_tbl[FH_PLL_NUM][4] = {
	/* FH PLL0 */
	{
	 {0, 0, 0, 0, 0, 0},
	 {PLL_SETTING_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* Default 0%(upbnd) ~ -0%(lowbnd) */
	 },

	/* FH PLL1 */
	{
	 {0, 0, 0, 0, 0, 0},
	 /* SSC Slope [dys]:0.015625 [dts]:1.808000 [slope]:0.096619 Mhz/us */
	 /* double slope = ((DYS[dy]*26)/DTS[df])*0.43; Test by from Yulia */
	 {PLL_SETTING_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* Default 0%(upbnd) ~ -2%(lowbnd) */
	 },

	/* FH PLL2 */
	{
	 {0, 0, 0, 0, 0, 0},
	 {PLL_SETTING_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* Default 0%(upbnd) ~ -0%(lowbnd) */
	 },

	/* FH PLL3 */
	{
	 {0, 0, 0, 0, 0, 0},
	 {PLL_SETTING_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* Default 0%(upbnd) ~ -0%(lowbnd) */
	 },

	/* FH PLL4 */
	{
	 {0, 0, 0, 0, 0, 0},
	 {PLL_SETTING_IDX__DEF, 0, 9, 0, 8, UNINIT_DDS},	/* Default 0%(upbnd) ~ -0%(lowbnd) */
	 },

	/* FH PLL5 */
	{
	 {0, 0, 0, 0, 0, 0},
	 {PLL_SETTING_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* Default 0%(upbnd) ~ -0%(lowbnd) */
	 }
};


/***********************************/
/*FHCTL HP CON Register */
/***********************************/
/* [MT6763] not used */
/*static const int pllid_to_hp_con[] = { 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 6, 7, 8 };*/

static struct freqhopping_ssc mt_ssc_fhpll_userdefined[FH_PLL_NUM];	/* freq, dt, df, upbnd, lowbnd, dds */

/*************************************/
/* FHCTL Register table              */
/* - Dynamic assign address based on */
/*   Device tree IP address          */
/*************************************/
static unsigned long g_reg_dds[FH_PLL_NUM];
static unsigned long g_reg_cfg[FH_PLL_NUM];
static unsigned long g_reg_updnlmt[FH_PLL_NUM];
static unsigned long g_reg_mon[FH_PLL_NUM];
static unsigned long g_reg_dvfs[FH_PLL_NUM];
static unsigned long g_reg_pll_con0[FH_PLL_NUM];
static unsigned long g_reg_pll_con1[FH_PLL_NUM];

/* 0~8% of 0x1026E8 */
static const int mempll_ssc_pmap1066[9] = {0, 0x14A, 0x295, 0x3E0, 0x52B, 0x675, 0x7C0, 0x90B, 0xA56};
/* 0~8% of 0x1435E7*/
static const int mempll_ssc_pmap1344[9] = {0, 0x19D, 0x33B, 0x4D9, 0x677, 0x815, 0x9B3, 0xB51, 0xCEF};
/* 0~8% of 0xDD89D*/
static const int mmpll_ssc_pmap180[9] = {0, 0x11B, 0x237, 0x352, 0x46E, 0x589, 0x6A5, 0x7C0, 0x8DC};
/* 0~8% of 0x1713B1*/
static const int mmpll_ssc_pmap300[9] = {0, 0x1D8, 0x3B1, 0x589, 0x762, 0x93B, 0xB13, 0xCEC, 0xEC4};

/*****************************************************************************/
/* Function */
/*****************************************************************************/

static void mt_fh_hal_default_conf(void)
{
	int id;

	FH_MSG_DEBUG("%s", __func__);


	/* According to setting to enable PLL SSC during init FHCTL. */
	for (id = 0; id < FH_PLL_NUM; id++) {

		g_fh_pll[id].pll_status = FH_PLL_ENABLE;

		if (g_pll_ssc_init_tbl[id] == FH_SSC_DEF_ENABLE_SSC) {
			FH_MSG("[Default ENABLE SSC] PLL_ID:%d", id);
			g_fh_pll[id].fh_status = FH_FH_ENABLE_SSC;
			freqhopping_config(id, PLL_SETTING_IDX__DEF, true);	/* MAINPLL */
		} else if (g_pll_ssc_init_tbl[id] == FH_SSC_DEF_DYNAMIC_SSC) {
			g_fh_pll[id].fh_status = FH_FH_ENABLE_SSC;
		} else {
			g_fh_pll[id].fh_status = FH_FH_DISABLE;
		}
	}
}

#ifdef HP_EN_REG_SEMAPHORE_PROTECT
static int get_hw_semaphore(void)
{
	const unsigned long ap_sema_reg = (unsigned long) g_spm_base + 0x428;
	unsigned int i;

	for (i = 0; i < 200; i++) {
		fh_write32(ap_sema_reg, 0x1);
		if (fh_read32(ap_sema_reg) & 0x1)
			return 0;

		udelay(10);
	}
	return -1;
}

static void release_hw_semaphore(void)
{
	const unsigned long ap_sema_reg = (unsigned long) g_spm_base + 0x428;

	if (fh_read32(ap_sema_reg) & 0x1)
		fh_write32(ap_sema_reg, 0x1);
}

#if 0
static u32 test_hw_semaphore(void)
{
	const unsigned long ap_sema_reg = (unsigned long)g_spm_base + 0x428;

	return fh_read32(ap_sema_reg);
}
#endif
#endif

static void fh_switch2fhctl(enum FH_PLL_ID pll_id, int i_control)
{
	unsigned int mask = 0;

	VALIDATE_PLLID(pll_id);

/*	mask = 0x1U << pllid_to_hp_con[pll_id];*/
	mask = 0x1U << pll_id;

	/* Release software reset */
	/* fh_set_field(REG_FHCTL_RST_CON, mask, 0); */

#ifdef HP_EN_REG_SEMAPHORE_PROTECT
	/* Switch to FHCTL_CORE controller */
	/* Use HW semaphore to share REG_FHCTL_HP_EN with SPM */
	if (isFHCTL(pll_id)) {
		if (get_hw_semaphore() == 0) {
			fh_set_field(REG_FHCTL_HP_EN, mask, i_control);
			release_hw_semaphore();
		} else {
			FH_MSG("sema time out 2ms\n");
			if (get_hw_semaphore() == 0) {
				fh_set_field(REG_FHCTL_HP_EN, mask, i_control);
				release_hw_semaphore();
			} else {
				FH_MSG("sema time out 4ms\n");
				WARN_ON(1);
			}
		}
	} else {
		FH_MSG("Invalid pll id!\n");
		WARN_ON(1);
	}
#else
	/* Switch to FHCTL_CORE controller - Original design */
	if (isFHCTL(pll_id)) {
		fh_set_field(REG_FHCTL_HP_EN, mask, i_control);
	} else {
		FH_MSG("Invalid pll id!\n");
		WARN_ON(1);
	}
#endif
}

static void fh_sync_ncpo_to_fhctl_dds(enum FH_PLL_ID pll_id)
{
	unsigned long reg_src = 0;
	unsigned long reg_dst = 0;

	VALIDATE_PLLID(pll_id);

	if (pll_id >= FH_PLL_NUM)
		return;

	reg_src = g_reg_pll_con1[pll_id];
	reg_dst = g_reg_dds[pll_id];

	if (pll_id == FH_MEM_PLLID)
		fh_write32(reg_dst, (((fh_read32(reg_src) & 0xFFFFFFFE) >> 11) & MASK21b) | BIT32);
	else
		fh_write32(reg_dst, (fh_read32(reg_src) & MASK21b) | BIT32);
}

static void __enable_ssc(unsigned int pll_id, const struct freqhopping_ssc *setting)
{
	unsigned long flags = 0;
	const unsigned long reg_cfg = g_reg_cfg[pll_id];
	const unsigned long reg_updnlmt = g_reg_updnlmt[pll_id];
	const unsigned long reg_dds = g_reg_dds[pll_id];

	FH_MSG_DEBUG("%s: %x~%x df:%d dt:%d dds:%x",
		     __func__, setting->lowbnd, setting->upbnd, setting->df, setting->dt,
		     setting->dds);

	mb();/* prevent reg setting value not sync */

	g_fh_pll[pll_id].fh_status = FH_FH_ENABLE_SSC;

	local_irq_save(flags);
	/* spin_lock(&g_fh_lock); */

	/* Set the relative parameter registers (dt/df/upbnd/downbnd) */
	fh_set_field(reg_cfg, MASK_FRDDSX_DYS, setting->df);
	fh_set_field(reg_cfg, MASK_FRDDSX_DTS, setting->dt);

	fh_sync_ncpo_to_fhctl_dds(pll_id);

	/* TODO: Not setting upper due to they are all 0? */
	fh_write32(reg_updnlmt,
		   (PERCENT_TO_DDSLMT((fh_read32(reg_dds) & MASK21b), setting->lowbnd) << 16));

	if (pll_id == FH_MEM_PLLID) {
		/* Since SPM cannot do multiplicatio */
		/* we pass DDS lower bound for SSC for 1066MHz and 1344 MHz */
		fh_write32((unsigned long) g_spm_base+0x4D0, (mempll_ssc_pmap1066[setting->lowbnd]));
		fh_write32((unsigned long) g_spm_base+0x4D4, (mempll_ssc_pmap1344[setting->lowbnd]));
	} else if (pll_id == FH_MM_PLLID) {
		fh_write32((unsigned long) g_spm_base+0x4D8, (mmpll_ssc_pmap180[setting->lowbnd]));
		fh_write32((unsigned long) g_spm_base+0x4DC, (mmpll_ssc_pmap300[setting->lowbnd]));
	}

	/* Switch to FHCTL */
	fh_switch2fhctl(pll_id, 1);
	mb();/* prevent reg setting value not sync */


	/* Enable SSC */
	fh_set_field(reg_cfg, FH_FRDDSX_EN, 1);
	/* Enable Hopping control */
	fh_set_field(reg_cfg, FH_FHCTLX_EN, 1);

	local_irq_restore(flags);
	/* spin_unlock(&g_fh_lock); */
}

static void __disable_ssc(unsigned int pll_id, const struct freqhopping_ssc *ssc_setting)
{
	unsigned long flags = 0;
	unsigned long reg_cfg = g_reg_cfg[pll_id];

	FH_MSG_DEBUG("Calling %s", __func__);

	local_irq_save(flags);
	/* spin_lock(&g_fh_lock); */


	/* Set the relative registers */
	fh_set_field(reg_cfg, FH_FRDDSX_EN, 0);
	fh_set_field(reg_cfg, FH_FHCTLX_EN, 0);
	mb();/* prevent reg setting value not sync */
	fh_switch2fhctl(pll_id, 0);
	g_fh_pll[pll_id].fh_status = FH_FH_DISABLE;

	local_irq_restore(flags);
	/* spin_unlock(&g_fh_lock); */
	mb();

}

/* Just to use special index pattern to find right setting. */
static noinline int __freq_to_index(enum FH_PLL_ID pll_id, int setting_idx_pattern)
{
	unsigned int retVal = 0;
	unsigned int i = PLL_SETTING_IDX__DEF;	/* start from 1 */
	const unsigned int size = ARRAY_SIZE(g_pll_ssc_setting_tbl[pll_id]);

	while (i < size) {
		if (setting_idx_pattern == g_pll_ssc_setting_tbl[pll_id][i].idx_pattern) {
			retVal = i;
			break;
		}
		++i;
	}

	return retVal;
}

/* Hook to g_fh_hal_drv.mt_fh_hal_ctrl function point.
 * Common drv freqhopping_config() will call the HAL API.
 */
static int __freqhopping_ctrl(struct freqhopping_ioctl *fh_ctl, bool enable)
{
	const struct freqhopping_ssc *pSSC_setting = NULL;
	unsigned int ssc_setting_id = 0;
	int retVal = 1;
	struct fh_pll_t *pfh_pll = NULL;

	FH_MSG("%s for pll %d", __func__, fh_ctl->pll_id);

	/* Check the out of range of frequency hopping PLL ID */
	VALIDATE_PLLID(fh_ctl->pll_id);

	pfh_pll = &g_fh_pll[fh_ctl->pll_id];

	pfh_pll->setting_idx_pattern = PLL_SETTING_IDX__DEF;

	if ((enable == true) && (pfh_pll->fh_status == FH_FH_ENABLE_SSC)) {
		__disable_ssc(fh_ctl->pll_id, pSSC_setting);
	} else if ((enable == false) && (pfh_pll->fh_status == FH_FH_DISABLE)) {
		retVal = 0;
		goto Exit;
	}
	/* enable freq. hopping @ fh_ctl->pll_id */
	if (enable == true) {
		if (pfh_pll->pll_status == FH_PLL_DISABLE) {
			pfh_pll->fh_status = FH_FH_ENABLE_SSC;
			retVal = 0;
			goto Exit;
		} else {
			if (pfh_pll->user_defined == true) {
				FH_MSG("Apply user defined setting");

				pSSC_setting = &mt_ssc_fhpll_userdefined[fh_ctl->pll_id];
				pfh_pll->setting_id = PLL_SETTING_IDX__USER;
			} else {
				if (pfh_pll->setting_idx_pattern != 0) {
					ssc_setting_id = pfh_pll->setting_id =
					    __freq_to_index(fh_ctl->pll_id,
							    pfh_pll->setting_idx_pattern);
				} else {
					ssc_setting_id = 0;
				}

				if (ssc_setting_id == 0) {
					FH_MSG("!!! No corresponding setting found !!!");

					/* just disable FH & exit */
					__disable_ssc(fh_ctl->pll_id, pSSC_setting);
					goto Exit;
				}

				pSSC_setting =
				    &g_pll_ssc_setting_tbl[fh_ctl->pll_id][ssc_setting_id];

			}	/* user defined */

			if (pSSC_setting == NULL) {
				FH_MSG("SSC_setting is NULL!");

				/* disable FH & exit */
				__disable_ssc(fh_ctl->pll_id, pSSC_setting);
				goto Exit;
			}

			__enable_ssc(fh_ctl->pll_id, pSSC_setting);
			retVal = 0;
		}
	} else {		/* disable req. hopping @ fh_ctl->pll_id */
		__disable_ssc(fh_ctl->pll_id, pSSC_setting);
		retVal = 0;
	}

Exit:
	return retVal;
}

static void wait_dds_stable(unsigned int target_dds, unsigned long reg_mon, unsigned int wait_count)
{
	unsigned int fh_dds = 0;
	unsigned int i = 0;

	fh_dds = fh_read32(reg_mon) & MASK21b;
	while ((target_dds != fh_dds) && (i < wait_count)) {
		udelay(10);
#if 0
		if (unlikely(i > 100)) {
			WARN_ON(1);
			break;
		}
#endif
		fh_dds = (fh_read32(reg_mon)) & MASK21b;
		++i;
	}
	if (i >= wait_count) {
		/* Has something wrong during hopping */
		FH_MSG("[Warning]wait_dds_stable()  target_dds = 0x%x, fh_dds = 0x%x, i = %d",
		       target_dds, fh_dds, i);
	}
}

/* Please add lock between the API for protecting FHCLT register atomic operation.
 *     spin_lock(&g_fh_lock);
 *     mt_fh_hal_hopping();
 *     spin_unlock(&g_fh_lock);
 */
static int mt_fh_hal_hopping(enum FH_PLL_ID pll_id, unsigned int dds_value)
{
	unsigned long flags = 0;

	FH_MSG_DEBUG("%s for pll %d:", __func__, pll_id);

	VALIDATE_PLLID(pll_id);

	local_irq_save(flags);

	/* 1. sync ncpo to DDS of FHCTL */
	fh_sync_ncpo_to_fhctl_dds(pll_id);

	/* FH_MSG("1. sync ncpo to DDS of FHCTL"); */
	FH_MSG_DEBUG("[Before DVFS] FHCTL%d_DDS: 0x%08x", pll_id,
		     (fh_read32(g_reg_dds[pll_id]) & MASK21b));

	/* 2. enable DVFS and Hopping control */
	{
		unsigned long reg_cfg = g_reg_cfg[pll_id];

		fh_set_field(reg_cfg, FH_SFSTRX_EN, 1);	/* enable dvfs mode */
		fh_set_field(reg_cfg, FH_FHCTLX_EN, 1);	/* enable hopping control */
	}

	/* FH_MSG("2. enable DVFS and Hopping control"); */

	/* for slope setting. */
	fh_write32(REG_FHCTL_SLOPE0, 0x6000F4B); /* use default value */
	if (pll_id == FH_MEM_PLLID)
		fh_write32(REG_FHCTL_SLOPE1, 0xFF000368); /* 0.1401MHz/us from DE */


	/* 3. switch to hopping control */
	fh_switch2fhctl(pll_id, 1);
	mb(); /* prevent reg setting value not sync */

	/* FH_MSG("3. switch to hopping control"); */

	/* 4. set DFS DDS */
	{
		unsigned long dvfs_req = g_reg_dvfs[pll_id];

		fh_write32(dvfs_req, (dds_value) | (BIT32));	/* set dds */

		/* FH_MSG("4. set DFS DDS"); */
		FH_MSG_DEBUG("[After DVFS] FHCTL%d_DDS: 0x%08x", pll_id,
			     (fh_read32(dvfs_req) & MASK21b));
		FH_MSG_DEBUG("FHCTL%d_DVFS: 0x%08x", pll_id, (fh_read32(dvfs_req) & MASK21b));
	}

	/* 4.1 ensure jump to target DDS */
	wait_dds_stable(dds_value, g_reg_mon[pll_id], 100);
	/* FH_MSG("4.1 ensure jump to target DDS"); */

	/* 5. write back to ncpo */
	/* FH_MSG("5. write back to ncpo"); */
	{
		unsigned long reg_dvfs = 0;
		unsigned long reg_pll_con1 = 0;


		reg_pll_con1 = g_reg_pll_con1[pll_id];
		reg_dvfs = g_reg_dvfs[pll_id];
		FH_MSG_DEBUG("PLL_CON1: 0x%08x", (fh_read32(reg_pll_con1)&MASK21b));

		if (pll_id == FH_MEM_PLLID) {
			if (fh_read32(reg_pll_con1) & 0x1)
				fh_write32(reg_pll_con1,
					(((fh_read32(g_reg_dds[pll_id]) & MASK21b) << 11) & 0xFFFFF800));
			else
				fh_write32(reg_pll_con1,
					(((fh_read32(g_reg_dds[pll_id]) & MASK21b) << 11) & 0xFFFFF800) | 0x1);
			FH_MSG_DEBUG("New MEMPLL_CON1:0x%08x  MEMPLL_CON1>>11_DDS: 0x%08x", fh_read32(reg_pll_con1),
				((fh_read32(reg_pll_con1) & 0xFFFFFFFE) >> 11) & MASK21b);
		} else {
			fh_write32(reg_pll_con1,
				(fh_read32(g_reg_mon[pll_id])&MASK21b)
				|(fh_read32(reg_pll_con1)&0xFFE00000)|(BIT32));
			FH_MSG_DEBUG("PLL_CON1: 0x%08x", (fh_read32(reg_pll_con1)&MASK21b));
		}
	}

	/* 6. switch to register control */
	fh_switch2fhctl(pll_id, 0);
	mb(); /* prevent reg setting value not sync */

	/* FH_MSG("6. switch to register control"); */

	local_irq_restore(flags);

	return 0;
}

/* General purpose PLL hopping and SSC enable API. */
static int mt_fh_hal_general_pll_dfs(enum FH_PLL_ID pll_id, unsigned int target_dds)
{
	const unsigned long reg_cfg = g_reg_cfg[pll_id];
	unsigned long flags = 0;

	VALIDATE_PLLID(pll_id);

	switch (pll_id) {
	case FH_ARM_PLLID:
	case FH_MAIN_PLLID:
	case FH_MSDC_PLLID:
	case FH_MFG_PLLID:
	case FH_MEM_PLLID:
	case FH_MM_PLLID:
		break;
	default:
		FH_MSG("ERROR! The [PLL_ID]:%d was forbidden hopping by MT6739 FHCTL.", pll_id);
		WARN_ON(1);
		return 0;
	}

	if (g_initialize == 0) {
		FH_MSG("(Warning) %s FHCTL isn't ready. ", __func__);
		return -1;
	}

	if (target_dds > MAX_DDS) {
		/* Check dds overflow (21 bit) */
		FH_MSG("[ERROR] Overflow! [%s] [pll_id]:%d [dds]:0x%x", __func__, pll_id, target_dds);
		WARN_ON(1);
	}

	FH_MSG("%s, [Pll_ID]:%d [current dds(CON1)]:0x%x, [target dds]:%d",
	       __func__, pll_id, (fh_read32(g_reg_pll_con1[pll_id]) & MASK21b), target_dds);

	spin_lock_irqsave(&g_fh_lock, flags);

	if (g_fh_pll[pll_id].fh_status == FH_FH_ENABLE_SSC) {
		unsigned int pll_dds = 0;
		unsigned int fh_dds = 0;

		/* only when SSC is enable, turn off PLL hopping */
		fh_set_field(reg_cfg, FH_FRDDSX_EN, 0);	/* disable SSC mode */
		fh_set_field(reg_cfg, FH_SFSTRX_EN, 0);	/* disable dvfs mode */
		fh_set_field(reg_cfg, FH_FHCTLX_EN, 0);	/* disable hopping control */

		pll_dds = (fh_read32(g_reg_dds[pll_id])) & MASK21b;
		fh_dds = (fh_read32(g_reg_mon[pll_id])) & MASK21b;

		wait_dds_stable(pll_dds, g_reg_mon[pll_id], 100);
	}

	mt_fh_hal_hopping(pll_id, target_dds);

	if (g_fh_pll[pll_id].fh_status == FH_FH_ENABLE_SSC) {
		const struct freqhopping_ssc *p_setting =
		    &g_pll_ssc_setting_tbl[pll_id][PLL_SETTING_IDX__DEF];

		fh_set_field(reg_cfg, FH_FRDDSX_EN, 0);	/* disable SSC mode */
		fh_set_field(reg_cfg, FH_SFSTRX_EN, 0);	/* disable dvfs mode */
		fh_set_field(reg_cfg, FH_FHCTLX_EN, 0);	/* disable hopping control */

		fh_sync_ncpo_to_fhctl_dds(pll_id);

		/* FH_MSG("Enable PLL SSC mode"); */
		/* FH_MSG("DDS: 0x%08x", (fh_read32(g_reg_dds[pll_id]) & MASK21b)); */

		fh_set_field(reg_cfg, MASK_FRDDSX_DYS, p_setting->df);
		fh_set_field(reg_cfg, MASK_FRDDSX_DTS, p_setting->dt);

		fh_write32(g_reg_updnlmt[pll_id],
			   (PERCENT_TO_DDSLMT
			    ((fh_read32(g_reg_dds[pll_id]) & MASK21b), p_setting->lowbnd) << 16));
		/* FH_MSG("UPDNLMT: 0x%08x", fh_read32(g_reg_updnlmt[pll_id])); */

		fh_switch2fhctl(pll_id, 1);

		fh_set_field(reg_cfg, FH_FRDDSX_EN, 1);	/* enable SSC mode */
		fh_set_field(reg_cfg, FH_FHCTLX_EN, 1);	/* enable hopping control */

		/* FH_MSG("CFG: 0x%08x", fh_read32(reg_cfg)); */

	}
	spin_unlock_irqrestore(&g_fh_lock, flags);
	return 0;
}

static int mt_fh_hal_dfs_armpll(unsigned int coreid, unsigned int target_dds)
{
	return mt_fh_hal_general_pll_dfs(FH_ARM_PLLID, target_dds);
}


/* #define UINT_MAX (unsigned int)(-1) */
static int fh_dumpregs_proc_read(struct seq_file *m, void *v)
{
	int i = 0;
	static unsigned int dds_max[FH_PLL_NUM] = { 0 };
	static unsigned int dds_min[FH_PLL_NUM] = { 0 };

	if (g_initialize != 1) {
		FH_MSG("[ERROR] %s fhctl didn't init. Please check!!!", __func__);
		return -1;
	}

	FH_MSG("EN: %s", __func__);

	for (i = 0; i < FH_PLL_NUM; ++i) {
		FH_MSG_DEBUG("REG ADDR (%d) : 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx",
			i, g_reg_mon[i], g_reg_cfg[i], g_reg_updnlmt[i],
			g_reg_dvfs[i], g_reg_dds[i], g_reg_pll_con0[i], g_reg_pll_con1[i]);
	}

	for (i = 0; i < FH_PLL_NUM; ++i) {
		unsigned int mon;
		unsigned int dds;

		FH_MSG_DEBUG("Dumping PLL %d", i);

		mon = fh_read32(g_reg_mon[i]);
		dds = mon & MASK21b;

		seq_printf(m, "FHCTL%d CFG, UPDNLMT, DVFS, DDS, MON\r\n", i);
		seq_printf(m, "0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\r\n",
			   fh_read32(g_reg_cfg[i]), fh_read32(g_reg_updnlmt[i]),
			   fh_read32(g_reg_dvfs[i]), fh_read32(g_reg_dds[i]), mon);

		if (dds > dds_max[i])
			dds_max[i] = dds;
		if ((dds < dds_min[i]) || (dds_min[i] == 0))
			dds_min[i] = dds;
	}

	FH_MSG_DEBUG("Dumping flags");
	seq_printf(m, "\r\nFHCTL_HP_EN:\r\n0x%08x\r\n", fh_read32(REG_FHCTL_HP_EN));
	seq_printf(m, "\r\nFHCTL_CLK_CON:\r\n0x%08x\r\n", fh_read32(REG_FHCTL_CLK_CON));

	FH_MSG_DEBUG("Dumping CON0");
	seq_puts(m, "\r\nPLL_CON0 :\r\n");
	for (i = 0; i < FH_PLL_NUM; ++i) {
		FH_MSG_DEBUG("Dumping PLL %d", i);
		if (g_reg_pll_con0[i] == REG_PLL_NOT_SUPPORT)
			seq_printf(m, "PLL%d;not support", i);
		else
			seq_printf(m, "PLL%d;0x%08x ", i, fh_read32(g_reg_pll_con0[i]));
	}

	FH_MSG_DEBUG("Dumping CON1");
	seq_puts(m, "\r\nPLL_CON1 :\r\n");
	for (i = 0; i < FH_PLL_NUM; ++i) {
		FH_MSG_DEBUG("Dumping PLL %d", i);
		if (g_reg_pll_con1[i] == REG_PLL_NOT_SUPPORT)
			seq_printf(m, "PLL%d;not support", i);
		else
			seq_printf(m, "PLL%d;0x%08x ", i, fh_read32(g_reg_pll_con1[i]));
	}

	seq_puts(m, "\r\nRecorded dds range\r\n");

	for (i = 0; i < FH_PLL_NUM; ++i)
		seq_printf(m, "Pll%d dds max 0x%06x, min 0x%06x\r\n", i, dds_max[i], dds_min[i]);

	return 0;
}

static void __reg_tbl_init(void)
{
	int id = 0;

    /****************************************/
	/* Should porting for specific platform. */
    /****************************************/

	const unsigned long reg_dds[] = {
		REG_FHCTL0_DDS, REG_FHCTL1_DDS, REG_FHCTL2_DDS, REG_FHCTL3_DDS,
		REG_FHCTL4_DDS, REG_FHCTL5_DDS

	};

	const unsigned long reg_cfg[] = {
		REG_FHCTL0_CFG, REG_FHCTL1_CFG, REG_FHCTL2_CFG, REG_FHCTL3_CFG,
		REG_FHCTL4_CFG, REG_FHCTL5_CFG
	};

	const unsigned long reg_updnlmt[] = {
		REG_FHCTL0_UPDNLMT, REG_FHCTL1_UPDNLMT, REG_FHCTL2_UPDNLMT, REG_FHCTL3_UPDNLMT,
		REG_FHCTL4_UPDNLMT, REG_FHCTL5_UPDNLMT
	};

	const unsigned long reg_mon[] = {
		REG_FHCTL0_MON, REG_FHCTL1_MON, REG_FHCTL2_MON, REG_FHCTL3_MON,
		REG_FHCTL4_MON, REG_FHCTL5_MON
	};

	const unsigned long reg_dvfs[] = {
		REG_FHCTL0_DVFS, REG_FHCTL1_DVFS, REG_FHCTL2_DVFS, REG_FHCTL3_DVFS,
		REG_FHCTL4_DVFS, REG_FHCTL5_DVFS
	};

	const unsigned long reg_pll_con0[] = {
		REG_FH_PLL0_CON0, REG_FH_PLL1_CON0, REG_FH_PLL2_CON0, REG_FH_PLL3_CON0,
		REG_FH_PLL4_CON0, REG_FH_PLL5_CON0
	};

	const unsigned long reg_pll_con1[] = {
		REG_FH_PLL0_CON1, REG_FH_PLL1_CON1, REG_FH_PLL2_CON1, REG_FH_PLL3_CON1,
		REG_FH_PLL4_CON1, REG_FH_PLL5_CON1
	};

    /****************************************/

	FH_MSG_DEBUG("EN: %s", __func__);


	for (id = 0; id < FH_PLL_NUM; ++id) {
		g_reg_dds[id] = reg_dds[id];
		g_reg_cfg[id] = reg_cfg[id];
		g_reg_updnlmt[id] = reg_updnlmt[id];
		g_reg_mon[id] = reg_mon[id];
		g_reg_dvfs[id] = reg_dvfs[id];
		g_reg_pll_con0[id] = reg_pll_con0[id];
		g_reg_pll_con1[id] = reg_pll_con1[id];
	}
}

/* Device Tree Initialize */
static int __reg_base_addr_init(void)
{
	struct device_node *fhctl_node;
	struct device_node *apmixed_node;
	struct device_node *ddrphy_node;
	struct device_node *spm_node;

	FH_MSG("(b) g_fhctl_base:0x%lx", (unsigned long)g_fhctl_base);
	FH_MSG("(b) g_apmixed_base:0x%lx", (unsigned long)g_apmixed_base);

	/* Init FHCTL base address */
	fhctl_node = of_find_compatible_node(NULL, NULL, "mediatek,fhctl");
	g_fhctl_base = of_iomap(fhctl_node, 0);
	if (!g_fhctl_base) {
		FH_MSG_DEBUG("Error, FHCTL iomap failed");
		WARN_ON(1);
	}

	/* Init APMIXED base address */
	apmixed_node = of_find_compatible_node(NULL, NULL, "mediatek,apmixed");
	g_apmixed_base = of_iomap(apmixed_node, 0);
	if (!g_apmixed_base) {
		FH_MSG_DEBUG("Error, APMIXED iomap failed");
		WARN_ON(1);
	}

	/* Init DDRPHY base address */
	ddrphy_node = of_find_compatible_node(NULL, NULL, "mediatek,ddrphy");
	g_ddrphy_base = of_iomap(ddrphy_node, 0);
	if (!g_ddrphy_base) {
		FH_MSG_DEBUG("Error, DDRPHY iomap failed");
		WARN_ON(1);
	}

	/* Init SPM base address */
	spm_node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");
	g_spm_base = of_iomap(spm_node, 0);
	if (!g_spm_base) {
		FH_MSG_DEBUG("Error, SPM iomap failed");
		WARN_ON(1);
	}

	FH_MSG("g_fhctl_base:0x%lx", (unsigned long)g_fhctl_base);
	FH_MSG("g_apmixed_base:0x%lx", (unsigned long)g_apmixed_base);
	FH_MSG("g_ddrphy_base:0x%lx", (unsigned long)g_ddrphy_base);
	FH_MSG("g_spm_base:0x%lx", (unsigned long)g_spm_base);
	__reg_tbl_init();

	return 0;
}

static void __global_var_init(void)
{

}

static int mt_fh_hal_init(void)
{
	int i = 0;
	unsigned long flags = 0;

	FH_MSG_DEBUG("EN: %s", __func__);

	if (g_initialize == 1)
		return 0;

	/* Init relevant register base address by device tree */
	__reg_base_addr_init();

	/* Global Variable Init */
	__global_var_init();

	/* FHCTL IP Init */
	for (i = 0; i < FH_PLL_NUM; ++i) {
		unsigned int mask;

		mask = 1 << i;

		spin_lock_irqsave(&g_fh_lock, flags);

		fh_set_field(REG_FHCTL_CLK_CON, mask, 1);

		/* Release software-reset to reset */
		fh_set_field(REG_FHCTL_RST_CON, mask, 0);
		fh_set_field(REG_FHCTL_RST_CON, mask, 1);

		g_fh_pll[i].setting_id = 0;
		fh_write32(g_reg_cfg[i], 0x00000000);	/* No SSC and FH enabled */
		fh_write32(g_reg_updnlmt[i], 0x00000000);	/* clear all the settings */
		fh_write32(g_reg_dds[i], 0x00000000);	/* clear all the settings */

		spin_unlock_irqrestore(&g_fh_lock, flags);
	}

	g_initialize = 1;

	FH_MSG("mt_fh_hal_init done");

	return 0;
}

static void mt_fh_hal_lock(unsigned long *flags)
{
	/*spin_lock(&g_fh_lock);*/
	spin_lock_irqsave(&g_fh_lock, *flags);
}

static void mt_fh_hal_unlock(unsigned long *flags)
{
	/*spin_unlock(&g_fh_lock);*/
	spin_unlock_irqrestore(&g_fh_lock, *flags);
}

static int mt_fh_hal_get_init(void)
{
	return g_initialize;
}

/* Engineer mode will use the proc msg to create UI!!! */
static int __fh_debug_proc_read(struct seq_file *m, void *v, struct fh_pll_t *pll)
{
	int id;

	FH_MSG("EN: %s", __func__);

	/* [WWK] Should remove PLL name to save porting time. */
	/* [WWK] Could print ENG ID and PLL mapping */

	seq_puts(m, "\r\n[freqhopping debug flag]\r\n");
	seq_puts(m, "[1st Status] FH_FH_UNINIT:0, FH_FH_DISABLE: 1, FH_FH_ENABLE_SSC:2 \r\n");
	seq_puts(m, "[2nd Setting_id] Disable:0, Default:1, PLL_SETTING_IDX__USER:9 \r\n");
	seq_puts(m, "===============================================\r\n");

    /****** String Format sensitive for EM mode ******/
	seq_puts(m, "id");
	for (id = 0; id < FH_PLL_NUM; ++id)
		seq_printf(m, "=%s", g_pll_name[id]);

	seq_puts(m, "\r\n");



	for (id = 0; id < FH_PLL_NUM; ++id) {
		/* "  =%04d==%04d==%04d==%04d=\r\n" */
		if (id == 0)
			seq_puts(m, "  =");
		else
			seq_puts(m, "==");

		seq_printf(m, "%04d", pll[id].fh_status);

		if (id == (FH_PLL_NUM - 1))
			seq_puts(m, "=");
	}
	seq_puts(m, "\r\n");


	for (id = 0; id < FH_PLL_NUM; ++id) {
		/* "  =%04d==%04d==%04d==%04d=\r\n" */
		if (id == 0)
			seq_puts(m, "  =");
		else
			seq_puts(m, "==");

		seq_printf(m, "%04d", pll[id].setting_id);

		if (id == (FH_PLL_NUM - 1))
			seq_puts(m, "=");
	}
    /*************************************************/

	seq_puts(m, "\r\n");

	return 0;
}


/* *********************************************************************** */
/* This function would support special request. */
/* [History] */
/* We implement API mt_freqhopping_devctl() to */
/* complete -2~-4% SSC. (DVFS to -2% freq and enable 0~-2% SSC) */
/*  */
/* *********************************************************************** */
static int fh_ioctl_dvfs_ssc(unsigned int ctlid, void *arg)
{
	struct freqhopping_ioctl *fh_ctl = arg;

	switch (ctlid) {
	case FH_DCTL_CMD_DVFS:	/* < PLL DVFS */
		{
			mt_fh_hal_hopping(fh_ctl->pll_id, fh_ctl->ssc_setting.dds);
		}
		break;
	case FH_DCTL_CMD_DVFS_SSC_ENABLE:	/* PLL DVFS and enable SSC */
		{
			__disable_ssc(fh_ctl->pll_id, &(fh_ctl->ssc_setting));
			mt_fh_hal_hopping(fh_ctl->pll_id, fh_ctl->ssc_setting.dds);
			__enable_ssc(fh_ctl->pll_id, &(fh_ctl->ssc_setting));
		}
		break;
	case FH_DCTL_CMD_DVFS_SSC_DISABLE:	/* PLL DVFS and disable SSC */
		{
			__disable_ssc(fh_ctl->pll_id, &(fh_ctl->ssc_setting));
			mt_fh_hal_hopping(fh_ctl->pll_id, fh_ctl->ssc_setting.dds);
		}
		break;
	case FH_DCTL_CMD_SSC_ENABLE:	/* SSC enable */
		{
			__enable_ssc(fh_ctl->pll_id, &(fh_ctl->ssc_setting));
		}
		break;
	case FH_DCTL_CMD_SSC_DISABLE:	/* SSC disable */
		{
			__disable_ssc(fh_ctl->pll_id, &(fh_ctl->ssc_setting));
		}
		break;
	case FH_DCTL_CMD_GENERAL_DFS:
		{
			mt_fh_hal_general_pll_dfs(fh_ctl->pll_id, fh_ctl->ssc_setting.dds);
		}
		break;
	default:
		break;
	};

	return 0;
}


static void __ioctl(unsigned int ctlid, void *arg)
{
	switch (ctlid) {
	case FH_IO_PROC_READ:
		{
			struct FH_IO_PROC_READ_T *tmp = (struct FH_IO_PROC_READ_T *) (arg);

			__fh_debug_proc_read(tmp->m, tmp->v, tmp->pll);
		}
		break;
	case FH_DCTL_CMD_DVFS:	/* PLL DVFS */
	case FH_DCTL_CMD_DVFS_SSC_ENABLE:	/* PLL DVFS and enable SSC */
	case FH_DCTL_CMD_DVFS_SSC_DISABLE:	/* PLL DVFS and disable SSC */
	case FH_DCTL_CMD_SSC_ENABLE:	/* SSC enable */
	case FH_DCTL_CMD_SSC_DISABLE:	/* SSC disable */
	case FH_DCTL_CMD_GENERAL_DFS:
		{
			fh_ioctl_dvfs_ssc(ctlid, arg);
		}
		break;
	default:
		FH_MSG("Unrecognized ctlid %d", ctlid);
		break;
	};
}

static struct mt_fh_hal_driver g_fh_hal_drv = {
	.fh_pll = g_fh_pll,
	//.fh_usrdef = mt_ssc_fhpll_userdefined,
	.pll_cnt = FH_PLL_NUM,
	.mt_fh_hal_dumpregs_read = fh_dumpregs_proc_read,
	//.proc.dvfs_read = fh_dvfs_proc_read,
	//.proc.dvfs_write = fh_dvfs_proc_write,
	.mt_fh_hal_init = mt_fh_hal_init,
	.mt_fh_hal_ctrl = __freqhopping_ctrl,
	.mt_fh_lock = mt_fh_hal_lock,
	.mt_fh_unlock = mt_fh_hal_unlock,
	.mt_fh_get_init = mt_fh_hal_get_init,
	//.mt_fh_popod_restore = mt_fh_hal_popod_restore,
	//.mt_fh_popod_save = mt_fh_hal_popod_save,
	//.mt_l2h_mempll = NULL,
	//.mt_h2l_mempll = NULL,
	.mt_dfs_armpll = mt_fh_hal_dfs_armpll,
	//.mt_dfs_mmpll = mt_fh_hal_dfs_mmpll,
	//.mt_dfs_mempll = mt_fh_hal_dfs_mempll,
	//.mt_is_support_DFS_mode = mt_fh_hal_is_support_DFS_mode,
	//.mt_l2h_dvfs_mempll = mt_fh_hal_l2h_dvfs_mempll,
	//.mt_h2l_dvfs_mempll = mt_fh_hal_h2l_dvfs_mempll,
	//.mt_dram_overclock = mt_fh_hal_dram_overclock,
	//.mt_get_dramc = mt_fh_hal_get_dramc,
	.mt_fh_hal_default_conf = mt_fh_hal_default_conf,
	.mt_dfs_general_pll = mt_fh_hal_general_pll_dfs,
	.ioctl = __ioctl
};

struct mt_fh_hal_driver *mt_get_fh_hal_drv(void)
{
	return &g_fh_hal_drv;
}

/* SS13 request to provide the pause ARMPLL API */
/* [Purpose]: control PLL for each cluster */
int mt_pause_armpll(unsigned int pll, unsigned int pause)
{
	/* unsigned long flags = 0; */
	unsigned long reg_cfg = 0;
	unsigned long flags = 0;

	if (g_initialize == 0) {
		FH_MSG("(Warning) %s FHCTL isn't ready.", __func__);
		return -1;
	}

	FH_MSG_DEBUG("%s for pll %d pause %d", __func__, pll, pause);

	switch (pll) {
	case FH_ARM_PLLID:
		reg_cfg = g_reg_cfg[pll];
		FH_MSG_DEBUG("(FHCTLx_CFG): 0x%x", fh_read32(g_reg_cfg[pll]));
		break;
	default:
		WARN_ON(1);
		return 1;
	};

	/* TODO: provelock issue spin_lock(&g_fh_lock); */
	spin_lock_irqsave(&g_fh_lock, flags);

	if (pause & 0x00000001)
		fh_set_field(reg_cfg, FH_FHCTLX_PAUSE, 1);	/* pause  */
	else
		fh_set_field(reg_cfg, FH_FHCTLX_PAUSE, 0);	/* no pause  */

	spin_unlock_irqrestore(&g_fh_lock, flags);

	return 0;
}

#if 0
/*TODO: init in hal. Should find a proper place*/
static int __init mt_fh_driver_init(void)
{
	mt_freqhopping_init();
	return 0;
}
arch_initcall(mt_fh_driver_init);
#endif

/* TODO: module_exit(cpufreq_exit); */
