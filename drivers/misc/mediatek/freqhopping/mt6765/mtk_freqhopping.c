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
#ifdef HP_EN_REG_SEMAPHORE_PROTECT
#include "mt_cpufreq_hybrid.h"
#endif
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

/*********************************/
/* Utility Macro */
/*********************************/

#define VALIDATE_PLLID(id) WARN_ON((id >= FH_PLL_NUM)  \
	|| (g_reg_pll_con0[id] == REG_PLL_NOT_SUPPORT))
#define VALIDATE_DDS(dds)  WARN_ON(dds >  FH_DDS_MASK)
#define PERCENT_TO_DDSLMT(dDS, pERCENT_M10) \
			(((dDS * pERCENT_M10) >> 5) / 100)

/*********************************/
/* FHCTL PLL Setting ID */
/*********************************/
#define PLL_IDX__USER	(0x9)	/* Magic number */
#define PLL_IDX__DEF    (0x1)	/* Default Setting, Magic number */


/*********************************/
/* Track the status of all FHCTL PLL */
/*********************************/
/* init during run time. */
static struct fh_pll_t g_fh_pll[FH_PLL_NUM] = { };


/*********************************/
/* FHCTL PLL name                */
/*********************************/
static const char *g_pll_name[FH_PLL_NUM] = {
	"ARMPLL",
	"MAINPLL",
	"MSDCPLL",
	"MFGPLL",
	"NOTSUPPORT", /*tell EM MEMPLL cannot do hopping*/
	"MPLL",
	"MMPLL",
#if defined(CONFIG_ARCH_MT6765)
	"ARMPLL_L",
	"CCIPLL",
#endif
};

/*********************************/
/* FHCTL PLL SSC Setting Table   */
/*********************************/

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
	FH_SSC_DEF_DISABLE,	/* FHCTL PLL4 */
	FH_SSC_DEF_DISABLE,	/* FHCTL PLL5 */
	FH_SSC_DEF_DISABLE,	/* FHCTL PLL6 */
#if defined(CONFIG_ARCH_MT6765)
	FH_SSC_DEF_DISABLE,	/* FHCTL PLL7 */
	FH_SSC_DEF_DISABLE,	/* FHCTL PLL8 */
#endif
};


static const struct freqhopping_ssc g_pll_ssc_tbl[FH_PLL_NUM][4] = {
	/* FH PLL0 */
	{
	 {0, 0, 0, 0, 0, 0},
	 {PLL_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* 0% ~ -0% */
	 },

	/* FH PLL1 */
	{
	 {0, 0, 0, 0, 0, 0},
	 /* SSC Slope [dys]:0.015625 [dts]:1.808000 [slope]:0.096619 Mhz/us */
	 /* double slope = ((DYS[dy]*26)/DTS[df])*0.43; Test by from Yulia */
	 {PLL_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* 0% ~ -0% */
	 },

	/* FH PLL2 */
	{
	 {0, 0, 0, 0, 0, 0},
	 {PLL_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* 0% ~ -0% */
	 },

	/* FH PLL3 */
	{
	 {0, 0, 0, 0, 0, 0},
	 {PLL_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* 0% ~ -0% */
	 },

	/* FH PLL4 */
	{
	 {0, 0, 0, 0, 0, 0},
	 {PLL_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* 0% ~ -0% */
	 },

	/* FH PLL5 */
	{
	 {0, 0, 0, 0, 0, 0},
	 {PLL_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* 0% ~ -0% */
	 },

	/* FH PLL6 */
	{
	 {0, 0, 0, 0, 0, 0},
	 {PLL_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* 0% ~ -0% */
	 },

#if defined(CONFIG_ARCH_MT6765)
	/* FH PLL7 */
	{
	 {0, 0, 0, 0, 0, 0},
	 {PLL_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* 0% ~ -0% */
	 },

	/* FH PLL8 */
	{
	 {0, 0, 0, 0, 0, 0},
	 {PLL_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* 0% ~ -0%) */
	 },
#endif
};


/***********************************/
/*FHCTL HP CON Register */
/***********************************/

/* freq, dt, df, upbnd, lowbnd, dds */
static struct freqhopping_ssc mt_ssc_fhpll_userdefined[FH_PLL_NUM];


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


/********************************************************************/
/* Function */
/********************************************************************/

static void mt_fh_hal_default_conf(void)
{
	int id;

	FH_MSG_DEBUG("%s", __func__);


	/* According to setting to enable PLL SSC during init FHCTL. */
	for (id = 0; id < FH_PLL_NUM; id++) {
		if (g_reg_pll_con0[id] == REG_PLL_NOT_SUPPORT)
			g_fh_pll[id].pll_status = FH_PLL_DISABLE;
		else
			g_fh_pll[id].pll_status = FH_PLL_ENABLE;

		if (g_pll_ssc_init_tbl[id] == FH_SSC_DEF_ENABLE_SSC) {
			FH_MSG("[Default ENABLE SSC] PLL_ID:%d", id);
			g_fh_pll[id].fh_status = FH_FH_ENABLE_SSC;
			freqhopping_config(id, PLL_IDX__DEF, true);
		} else {
			g_fh_pll[id].fh_status = FH_FH_DISABLE;
		}
	}
}

static void fh_switch2fhctl(enum FH_PLL_ID pll_id, int i_control)
{
	unsigned int mask = 0;

	VALIDATE_PLLID(pll_id);

	/*mask = 0x1U << pllid_to_hp_con[pll_id];*/
	mask = 0x1U << pll_id;

	/* Release software reset */
	/* fh_set_field(REG_FHCTL_RST_CON, mask, 0); */


#ifdef HP_EN_REG_SEMAPHORE_PROTECT
	/* Switch to FHCTL_CORE controller */
	/* Use HW semaphore to share REG_FHCTL_HP_EN with secure CPU DVFS */
	if (cpuhvfs_get_dvfsp_semaphore(SEMA_FHCTL_DRV) == 0) {
		fh_set_field(REG_FHCTL_HP_EN, mask, i_control);
		cpuhvfs_release_dvfsp_semaphore(SEMA_FHCTL_DRV);
	} else {
		FH_MSG("sema time out 2ms\n");
		if (cpuhvfs_get_dvfsp_semaphore(SEMA_FHCTL_DRV) == 0) {
			fh_set_field(REG_MCU_FHCTL_HP_EN, mask, i_control);
			cpuhvfs_release_dvfsp_semaphore(SEMA_FHCTL_DRV);
		} else {
			FH_MSG("sema time out 4ms\n");
			WARN_ON(1);
		} /* if-else */
	} /* if-else */
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

	reg_src = g_reg_pll_con1[pll_id];
	reg_dst = g_reg_dds[pll_id];
	if (pll_id == FH_MEM_PLLID) {
		/* Confirmed with DE that we do not need to support MEMPLL */
		FH_MSG_DEBUG("Do not need to support MEMPLL");
	} else {
		fh_write32(reg_dst,
			(fh_read32(reg_src) & FH_DDS_MASK) |
			FH_FHCTLX_PLL_TGL_ORG);
	}

}

static void __enable_ssc(unsigned int pll_id,
			const struct freqhopping_ssc *setting)
{
	unsigned long flags = 0;
	const unsigned long reg_cfg = g_reg_cfg[pll_id];
	const unsigned long reg_updnlmt = g_reg_updnlmt[pll_id];
	const unsigned long reg_dds = g_reg_dds[pll_id];

	FH_MSG_DEBUG("%s: %x~%x df:%d dt:%d dds:%x",
		__func__, setting->lowbnd, setting->upbnd,
		setting->df, setting->dt, setting->dds);

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
		(PERCENT_TO_DDSLMT((fh_read32(reg_dds) & FH_DDS_MASK),
		setting->lowbnd) << 16));

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

static void __disable_ssc(unsigned int pll_id,
			const struct freqhopping_ssc *ssc_setting)
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
static noinline int __freq_to_index(enum FH_PLL_ID pll_id, int pattern)
{
	unsigned int retVal = 0;
	unsigned int i = PLL_IDX__DEF;	/* start from 1 */
	const unsigned int size = ARRAY_SIZE(g_pll_ssc_tbl[pll_id]);

	while (i < size) {
		if (pattern == g_pll_ssc_tbl[pll_id][i].idx_pattern) {
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
	unsigned int ssc_id = 0;
	int retVal = 1;
	struct fh_pll_t *pfh_pll = NULL;

	FH_MSG("%s for pll %d", __func__, fh_ctl->pll_id);

	/* Check the out of range of frequency hopping PLL ID */
	VALIDATE_PLLID(fh_ctl->pll_id);

	pfh_pll = &g_fh_pll[fh_ctl->pll_id];

	pfh_pll->setting_idx_pattern = PLL_IDX__DEF;

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

				pSSC_setting =
				  &mt_ssc_fhpll_userdefined[fh_ctl->pll_id];

				pfh_pll->setting_id = PLL_IDX__USER;
			} else {
				if (pfh_pll->setting_idx_pattern != 0) {
					ssc_id = pfh_pll->setting_id =
					    __freq_to_index(fh_ctl->pll_id,
						pfh_pll->setting_idx_pattern);
				} else {
					ssc_id = 0;
				}

				if (ssc_id == 0) {
					FH_MSG("No FH setting found !");

					/* just disable FH & exit */
					__disable_ssc(fh_ctl->pll_id,
							pSSC_setting);
					goto Exit;
				}

				pSSC_setting =
				    &g_pll_ssc_tbl[fh_ctl->pll_id][ssc_id];

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

#define WAIT_DDS_STABLE_WARNING_MSG \
	"[Warning]wait_dds_stable: target_dds = 0x%x, fh_dds = 0x%x, i = %d"

static void wait_dds_stable(unsigned int target_dds,
		unsigned long reg_mon, unsigned int wait_count)
{
	unsigned int fh_dds = 0;
	unsigned int i = 0;

	fh_dds = fh_read32(reg_mon) & FH_DDS_MASK;
	while ((target_dds != fh_dds) && (i < wait_count)) {
		udelay(10);
#if 0
		if (unlikely(i > 100)) {
			WARN_ON(1);
			break;
		}
#endif
		fh_dds = (fh_read32(reg_mon)) & FH_DDS_MASK;
		++i;
	}
	if (i >= wait_count) {
		/* Has something wrong during hopping */

		FH_MSG(WAIT_DDS_STABLE_WARNING_MSG,
			target_dds, fh_dds, i);
	}
}
#undef WAIT_DDS_STABLE_WARNING_MSG

/* Please add lock between the API for protecting FHCTL register
 * atomic operation.
 *     spin_lock(&g_fh_lock);
 *     mt_fh_hal_hopping();
 *     spin_unlock(&g_fh_lock);
 */
static int mt_fh_hal_hopping(enum FH_PLL_ID pll_id, unsigned int dds_value)
{
	unsigned long flags = 0;
	unsigned long reg_dvfs = 0;
	unsigned long reg_pll_con1 = 0;

	FH_MSG_DEBUG("%s for pll %d:", __func__, pll_id);

	if (g_reg_pll_con0[pll_id] == REG_PLL_NOT_SUPPORT) {
		FH_MSG("(ERROR) %s [pll_id]: %d freqhop isn't supported ",
			__func__, pll_id);
		return -1;
	}

	VALIDATE_PLLID(pll_id);

	if (dds_value > FH_DVFS_MASK) {
		FH_MSG("[ERROR] Overflow! [%s] [pll_id]:%d [dds]:0x%x",
			__func__, pll_id, dds_value);
		/* Check dds overflow (22 bit) */
		WARN_ON(1);
		return -1;
	}

	local_irq_save(flags);

	/* 1. sync ncpo to DDS of FHCTL */
	fh_sync_ncpo_to_fhctl_dds(pll_id);

	/* FH_MSG("1. sync ncpo to DDS of FHCTL"); */
	FH_MSG_DEBUG("[Before DVFS] FHCTL%d_DDS: 0x%08x", pll_id,
		(fh_read32(g_reg_dds[pll_id]) &  FH_DDS_MASK));

	/* 2. enable DVFS and Hopping control */
	{
		unsigned long reg_cfg = g_reg_cfg[pll_id];

		fh_set_field(reg_cfg, FH_SFSTRX_EN, 1);	/* enable dvfs mode */
		fh_set_field(reg_cfg, FH_FHCTLX_EN, 1);	/* enable hopping */
	}

	/* for slope setting. */

	fh_write32(REG_FHCTL_SLOPE0, 0x6003c97);
	/*ToDo: SLOPE1 is for MEMPLL. Do we need to set this register?*/
	/*fh_write32(REG_FHCTL_SLOPE1, 0x6003c97);*/


	/* FH_MSG("2. enable DVFS and Hopping control"); */

	/* 3. switch to hopping control */
	fh_switch2fhctl(pll_id, 1);
	mb(); /* prevent reg setting value not sync */

	/* FH_MSG("3. switch to hopping control"); */

	/* 4. set DFS DDS */
	{
		unsigned long dvfs_req = g_reg_dvfs[pll_id];

		/* set dds */
		fh_write32(dvfs_req, (dds_value) | (FH_FHCTLX_PLL_DVFS_TRI));

		/* FH_MSG("4. set DFS DDS"); */
		FH_MSG_DEBUG("[After DVFS] FHCTL%d_DDS: 0x%08x", pll_id,
			(fh_read32(dvfs_req) &  FH_DDS_MASK));
		FH_MSG_DEBUG("FHCTL%d_DVFS: 0x%08x", pll_id,
			(fh_read32(dvfs_req) &  FH_DDS_MASK));
	}

	/* 4.1 ensure jump to target DDS */
	wait_dds_stable(dds_value, g_reg_mon[pll_id], 100);
	/* FH_MSG("4.1 ensure jump to target DDS"); */

	/* 5. write back to ncpo */
	/* FH_MSG("5. write back to ncpo"); */
	if (pll_id == FH_MEM_PLLID) {
		/* Confirmed with DE */
		/* We dont need to support MEMPLL*/
		FH_MSG_DEBUG("Only support MPLL SSC on MT6759");
	} else {
		reg_pll_con1 = g_reg_pll_con1[pll_id];
		reg_dvfs = g_reg_dvfs[pll_id];
		FH_MSG_DEBUG("PLL_CON1: 0x%08x",
			(fh_read32(reg_pll_con1) & FH_XXPLL_CON1_PCW));

		fh_write32(reg_pll_con1,
			(fh_read32(g_reg_mon[pll_id]) & FH_DDS_MASK)|
			(fh_read32(reg_pll_con1) & (~FH_XXPLL_CON1_PCW))|
			(FH_XXPLL_CON1_PCWCHG)
		);
		FH_MSG_DEBUG("PLL_CON1: 0x%08x",
			(fh_read32(reg_pll_con1) & FH_XXPLL_CON1_PCW));
	}

	/* 6. switch to register control */
	fh_switch2fhctl(pll_id, 0);
	mb(); /* prevent reg setting value not sync */

	/* FH_MSG("6. switch to register control"); */

	local_irq_restore(flags);

	return 0;
}

/*
 * armpll dfs mdoe
 */
static int mt_fh_hal_dfs_armpll(unsigned int coreid, unsigned int dds)
{
	/* unsigned long flags = 0; */
	unsigned long reg_cfg = 0;
	unsigned int pll = coreid;
	unsigned long flags = 0;

	if (g_initialize == 0) {
		FH_MSG("(Warning) %s FHCTL isn't ready.", __func__);
		return -1;
	}

	if (dds > FH_DDS_MASK) {
		FH_MSG("[ERROR] Overflow! [%s] [pll_id]:%d [dds]:0x%x",
			__func__, pll, dds);
		/* Check dds overflow (21 bit) */
		WARN_ON(1);
	}

	switch (pll) {
	case FH_PLL0:
#if defined(CONFIG_ARCH_MT6765)
	case FH_PLL7:
#endif
		FH_MSG_DEBUG("[Before DVFS] (PLL_CON1): 0x%x",
			(fh_read32(g_reg_pll_con1[pll]) &  FH_DDS_MASK));
		break;
	default:
		FH_MSG("[ERROR] %s [pll_id]:%d isn't ARMPLLX. ", __func__, pll);
		WARN_ON(1);
		return 1;
	};

	reg_cfg = g_reg_cfg[pll];

	/* TODO: provelock issue spin_lock(&g_fh_lock); */
	/*spin_lock(&g_fh_lock);*/
	spin_lock_irqsave(&g_fh_lock, flags);

#if 0
	fh_set_field(reg_cfg, FH_FRDDSX_EN, 0); /* disable SSC mode */
	fh_set_field(reg_cfg, FH_SFSTRX_EN, 0); /* disable dvfs mode */
	fh_set_field(reg_cfg, FH_FHCTLX_EN, 0); /* disable hopping ctl*/
#else
	if (g_fh_pll[pll].fh_status == FH_FH_ENABLE_SSC) {
		unsigned int pll_dds = 0;
		unsigned int fh_dds = 0;

		/* only when SSC is enable, turn off ARMPLL hopping */
		fh_set_field(reg_cfg, FH_FRDDSX_EN, 0); /* disable SSC mode */
		fh_set_field(reg_cfg, FH_SFSTRX_EN, 0); /* disable dvfs mode */
		fh_set_field(reg_cfg, FH_FHCTLX_EN, 0); /* disable hp ctl */

		pll_dds = (fh_read32(g_reg_dds[pll])) &  FH_DDS_MASK;
		fh_dds = (fh_read32(g_reg_mon[pll])) &  FH_DDS_MASK;

		FH_MSG_DEBUG(">p:f< %x:%x", pll_dds, fh_dds);

		wait_dds_stable(pll_dds, g_reg_mon[pll], 100);
	}
#endif

	mt_fh_hal_hopping(pll, dds);

#if 0
	fh_set_field(reg_cfg, FH_FRDDSX_EN, 0); /* disable SSC mode */
	fh_set_field(reg_cfg, FH_SFSTRX_EN, 0); /* disable dvfs mode */
	fh_set_field(reg_cfg, FH_FHCTLX_EN, 0); /* disable hopping control */
#else
	if (g_fh_pll[pll].fh_status == FH_FH_ENABLE_SSC) {

		const struct freqhopping_ssc *p_setting = NULL;

		if (g_fh_pll[pll].user_defined == true)
			p_setting = &mt_ssc_fhpll_userdefined[pll];
		else
			p_setting = &g_pll_ssc_tbl[pll][PLL_IDX__DEF];

		fh_set_field(reg_cfg, FH_FRDDSX_EN, 0); /* disable SSC mode */
		fh_set_field(reg_cfg, FH_SFSTRX_EN, 0); /* disable dvfs mode */
		fh_set_field(reg_cfg, FH_FHCTLX_EN, 0); /* disable hp ctl */

		fh_sync_ncpo_to_fhctl_dds(pll);

		FH_MSG_DEBUG("Enable armpll SSC mode");
		FH_MSG_DEBUG("DDS: 0x%08x",
			(fh_read32(g_reg_dds[pll]) & FH_DDS_MASK));

		fh_set_field(reg_cfg, MASK_FRDDSX_DYS, p_setting->df);
		fh_set_field(reg_cfg, MASK_FRDDSX_DTS, p_setting->dt);

		fh_write32(g_reg_updnlmt[pll],
			(PERCENT_TO_DDSLMT(
				(fh_read32(g_reg_dds[pll]) & FH_DDS_MASK),
				p_setting->lowbnd
			) << 16)
		);
		FH_MSG_DEBUG("UPDNLMT: 0x%08x", fh_read32(g_reg_updnlmt[pll]));

		fh_switch2fhctl(pll, 1);

		fh_set_field(reg_cfg, FH_FRDDSX_EN, 1); /* enable SSC mode */
		fh_set_field(reg_cfg, FH_FHCTLX_EN, 1); /* enable hopping ctl */

		FH_MSG_DEBUG("CFG: 0x%08x", fh_read32(reg_cfg));
	}
#endif

	/*spin_unlock(&g_fh_lock);*/
	spin_unlock_irqrestore(&g_fh_lock, flags);

	return 0;
}

/* General purpose PLL hopping and SSC enable API. */
static int mt_fh_hal_general_pll_dfs(enum FH_PLL_ID pll_id,
				     unsigned int target_dds)
{
	const unsigned long reg_cfg = g_reg_cfg[pll_id];
	unsigned long flags = 0;

	VALIDATE_PLLID(pll_id);
	if (g_initialize == 0) {
		FH_MSG("(Warning) %s FHCTL isn't ready. ", __func__);
		return -1;
	}

	if (g_reg_pll_con0[pll_id] == REG_PLL_NOT_SUPPORT) {
		FH_MSG("(ERROR) %s [pll_id]: %d freqhop isn't supported ",
			__func__, pll_id);
		return -1;
	}

	if (target_dds > FH_DDS_MASK) {
		FH_MSG("[ERROR] Overflow! [%s] [pll_id]:%d [dds]:0x%x",
			__func__, pll_id,
		       target_dds);
		/* Check dds overflow (21 bit) */
		WARN_ON(1);
	}

	/* All new platform should confirm again!!! */
	switch (pll_id) {
	case FH_MEM_PLLID:
		/* MEMPLL Confirmed with DRAM SA that MEMPLL hopping
		 * will only control by clk DIV
		 */
		FH_MSG("ERROR! The [PLL_ID]:%d was forbidden hopping.", pll_id);
		WARN_ON(1);
		break;
	default:
		break;
	}

	FH_MSG("%s, [Pll_ID]:%d [current dds(CON1)]:0x%x, [target dds]:%d",
	       __func__, pll_id,
	       (fh_read32(g_reg_pll_con1[pll_id]) & FH_DDS_MASK),
	       target_dds);

	spin_lock_irqsave(&g_fh_lock, flags);

	if (g_fh_pll[pll_id].fh_status == FH_FH_ENABLE_SSC) {
		unsigned int pll_dds = 0;
		unsigned int fh_dds = 0;

		/* only when SSC is enable, turn off MEMPLL hopping */
		fh_set_field(reg_cfg, FH_FRDDSX_EN, 0);	/* disable SSC mode */
		fh_set_field(reg_cfg, FH_SFSTRX_EN, 0);	/* disable dvfs mode */
		fh_set_field(reg_cfg, FH_FHCTLX_EN, 0);	/* disable hp ctl */

		pll_dds = (fh_read32(g_reg_dds[pll_id])) & FH_DDS_MASK;
		fh_dds = (fh_read32(g_reg_mon[pll_id])) & FH_DDS_MASK;

		FH_MSG_DEBUG(">p:f< %x:%x", pll_dds, fh_dds);

		wait_dds_stable(pll_dds, g_reg_mon[pll_id], 100);
	}

	mt_fh_hal_hopping(pll_id, target_dds);

	if (g_fh_pll[pll_id].fh_status == FH_FH_ENABLE_SSC) {
		const struct freqhopping_ssc *p_setting = NULL;

		if (g_fh_pll[pll_id].user_defined == true)
			p_setting = &mt_ssc_fhpll_userdefined[pll_id];
		else
			p_setting = &g_pll_ssc_tbl[pll_id][PLL_IDX__DEF];


		fh_set_field(reg_cfg, FH_FRDDSX_EN, 0);	/* disable SSC mode */
		fh_set_field(reg_cfg, FH_SFSTRX_EN, 0);	/* disable dvfs mode */
		fh_set_field(reg_cfg, FH_FHCTLX_EN, 0);	/* disable hp ctl */

		fh_sync_ncpo_to_fhctl_dds(pll_id);

		fh_set_field(reg_cfg, MASK_FRDDSX_DYS, p_setting->df);
		fh_set_field(reg_cfg, MASK_FRDDSX_DTS, p_setting->dt);

		fh_write32(g_reg_updnlmt[pll_id],
			(PERCENT_TO_DDSLMT(
				(fh_read32(g_reg_dds[pll_id]) &  FH_DDS_MASK),
				p_setting->lowbnd
			) << 16)
		);

		fh_switch2fhctl(pll_id, 1);

		fh_set_field(reg_cfg, FH_FRDDSX_EN, 1);	/* enable SSC mode */
		fh_set_field(reg_cfg, FH_FHCTLX_EN, 1);	/* enable hp ctl */

		FH_MSG_DEBUG("CFG: 0x%08x", fh_read32(reg_cfg));

	}
	spin_unlock_irqrestore(&g_fh_lock, flags);

	return 0;

}

/* #define UINT_MAX (unsigned int)(-1) */
static int fh_dumpregs_proc_read(struct seq_file *m, void *v)
{
	int i = 0;
	static unsigned int dds_max[FH_PLL_NUM] = { 0 };
	static unsigned int dds_min[FH_PLL_NUM] = { 0 };

	if (g_initialize != 1) {
		FH_MSG("[ERROR] %s fhctl didn't init. Please check!", __func__);
		return -1;
	}

	FH_MSG("EN: %s", __func__);

	for (i = 0; i < FH_PLL_NUM; ++i) {
		const unsigned int mon = fh_read32(g_reg_mon[i]);
		const unsigned int dds = mon & FH_DDS_MASK;

		seq_printf(m, "FHCTL%d CFG, UPDNLMT, DVFS, DDS, MON\r\n", i);
		seq_printf(m, "0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\r\n",
			fh_read32(g_reg_cfg[i]), fh_read32(g_reg_updnlmt[i]),
			fh_read32(g_reg_dvfs[i]), fh_read32(g_reg_dds[i]), mon);

		if (dds > dds_max[i])
			dds_max[i] = dds;
		if ((dds < dds_min[i]) || (dds_min[i] == 0))
			dds_min[i] = dds;
	}

	seq_printf(m, "\r\nFHCTL_HP_EN:\r\n0x%08x\r\n",
			fh_read32(REG_FHCTL_HP_EN));
	seq_printf(m, "\r\nFHCTL_CLK_CON:\r\n0x%08x\r\n",
			fh_read32(REG_FHCTL_CLK_CON));

	seq_puts(m, "\r\nPLL_CON0 :\r\n");

	for (i = 0; i < FH_PLL_NUM; ++i) {
		if (g_reg_pll_con0[i] == REG_PLL_NOT_SUPPORT)
			seq_printf(m, "PLL%d;Not Support ", i);
		else
			seq_printf(m, "PLL%d;0x%08x ",
				i, fh_read32(g_reg_pll_con0[i]));
	}

	seq_puts(m, "\r\nPLL_CON1 :\r\n");
	for (i = 0; i < FH_PLL_NUM; ++i) {
		if (g_reg_pll_con1[i] == REG_PLL_NOT_SUPPORT)
			seq_printf(m, "PLL%d;Not Support ", i);
		else
			seq_printf(m, "PLL%d;0x%08x ",
				i, fh_read32(g_reg_pll_con1[i]));
	}

	seq_puts(m, "\r\nRecorded dds range\r\n");

	for (i = 0; i < FH_PLL_NUM; ++i)
		seq_printf(m, "Pll%d dds max 0x%06x, min 0x%06x\r\n",
				i, dds_max[i], dds_min[i]);

	return 0;
}

static void __reg_tbl_init(void)
{
	int id = 0;

	/****************************************/
	/* Should porting for specific platform. */
	/****************************************/

	const unsigned long reg_dds[] = {
		REG_FHCTL0_DDS, REG_FHCTL1_DDS, REG_FHCTL2_DDS,
		REG_FHCTL3_DDS, REG_FHCTL4_DDS, REG_FHCTL5_DDS,
		REG_FHCTL6_DDS,
#if defined(CONFIG_ARCH_MT6765)
		REG_FHCTL7_DDS, REG_FHCTL8_DDS
#endif
	};

	const unsigned long reg_cfg[] = {
		REG_FHCTL0_CFG, REG_FHCTL1_CFG, REG_FHCTL2_CFG,
		REG_FHCTL3_CFG, REG_FHCTL4_CFG, REG_FHCTL5_CFG,
		REG_FHCTL6_CFG,
#if defined(CONFIG_ARCH_MT6765)
		REG_FHCTL7_CFG, REG_FHCTL8_CFG
#endif
	};

	const unsigned long reg_updnlmt[] = {
		REG_FHCTL0_UPDNLMT, REG_FHCTL1_UPDNLMT, REG_FHCTL2_UPDNLMT,
		REG_FHCTL3_UPDNLMT, REG_FHCTL4_UPDNLMT, REG_FHCTL5_UPDNLMT,
		REG_FHCTL6_UPDNLMT,
#if defined(CONFIG_ARCH_MT6765)
		REG_FHCTL7_UPDNLMT, REG_FHCTL8_UPDNLMT
#endif
	};

	const unsigned long reg_mon[] = {
		REG_FHCTL0_MON, REG_FHCTL1_MON, REG_FHCTL2_MON,
		REG_FHCTL3_MON, REG_FHCTL4_MON, REG_FHCTL5_MON,
		REG_FHCTL6_MON,
#if defined(CONFIG_ARCH_MT6765)
		REG_FHCTL7_MON, REG_FHCTL8_MON
#endif
	};

	const unsigned long reg_dvfs[] = {
		REG_FHCTL0_DVFS, REG_FHCTL1_DVFS, REG_FHCTL2_DVFS,
		REG_FHCTL3_DVFS, REG_FHCTL4_DVFS, REG_FHCTL5_DVFS,
		REG_FHCTL6_DVFS,
#if defined(CONFIG_ARCH_MT6765)
		REG_FHCTL7_DVFS, REG_FHCTL8_DVFS
#endif
	};

	const unsigned long reg_pll_con0[] = {
		REG_FH_PLL0_CON0, REG_FH_PLL1_CON0, REG_FH_PLL2_CON0,
		REG_FH_PLL3_CON0, REG_FH_PLL4_CON0, REG_FH_PLL5_CON0,
		REG_FH_PLL6_CON0,
#if defined(CONFIG_ARCH_MT6765)
		REG_FH_PLL7_CON0, REG_FH_PLL8_CON0
#endif
	};

	const unsigned long reg_pll_con1[] = {
		REG_FH_PLL0_CON1, REG_FH_PLL1_CON1, REG_FH_PLL2_CON1,
		REG_FH_PLL3_CON1, REG_FH_PLL4_CON1, REG_FH_PLL5_CON1,
		REG_FH_PLL6_CON1,
#if defined(CONFIG_ARCH_MT6765)
		REG_FH_PLL7_CON1, REG_FH_PLL8_CON1
#endif
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

	FH_MSG("(b) g_fhctl_base:0x%lx", (unsigned long)g_fhctl_base);
	FH_MSG("(b) g_apmixed_base:0x%lx", (unsigned long)g_apmixed_base);

	/* Init FHCTL base address */
	fhctl_node = of_find_compatible_node(NULL, NULL, "mediatek,fhctl");
	g_fhctl_base = of_iomap(fhctl_node, 0);
	if (!g_fhctl_base) {
		FH_MSG_DEBUG("Error, FHCTL iomap failed");
		WARN_ON(1);
		return -ENODEV;
	}

	/* Init APMIXED base address */
#if defined(CONFIG_ARCH_MT6765)
	apmixed_node =
		of_find_compatible_node(NULL, NULL, "mediatek,apmixedsys");
#else
	apmixed_node =
		of_find_compatible_node(NULL, NULL, "mediatek,apmixed");
#endif
	g_apmixed_base = of_iomap(apmixed_node, 0);
	if (!g_apmixed_base) {
		FH_MSG_DEBUG("Error, APMIXED iomap failed");
		WARN_ON(1);
		return -ENODEV;
	}

	FH_MSG("g_fhctl_base:0x%lx", (unsigned long)g_fhctl_base);
	FH_MSG("g_apmixed_base:0x%lx", (unsigned long)g_apmixed_base);
	__reg_tbl_init();

	return 0;
}

static void __global_var_init(void)
{

}

/* TODO: __init void mt_freqhopping_init(void) */
static int mt_fh_hal_init(void)
{
	int i = 0;
	unsigned long flags = 0;
	int of_init_result = 0;

	FH_MSG_DEBUG("EN: %s", __func__);

	if (g_initialize == 1)
		return 0;

	/* Init relevant register base address by device tree */
	of_init_result = __reg_base_addr_init();
	if (of_init_result != 0)
		return of_init_result;

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
		fh_write32(g_reg_cfg[i], 0x00000000);	/* No SSC/FH enabled */
		fh_write32(g_reg_updnlmt[i], 0x00000000); /* clr all setting */
		fh_write32(g_reg_dds[i], 0x00000000);	/* clr all settings */

		spin_unlock_irqrestore(&g_fh_lock, flags);
	}

	g_initialize = 1;

	FH_MSG("%s done", __func__);
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


/* TODO: module_init(mt_freqhopping_init); */
/* TODO: module_exit(cpufreq_exit); */

/* Engineer mode will use the proc msg to create UI!!! */
#define FH_DEBUG_PROC_READ_MSG_STATUS \
	"[1st Status] FH_FH_UNINIT:" \
	" 0, FH_FH_DISABLE: 1, FH_FH_ENABLE_SSC:2 \r\n"

#define FH_DEBUG_PROC_READ_MSG_SETTING_IDX \
	"[2nd Setting_id] Disable:" \
	"0, Default:1, PLL_SETTING_IDX__USER:9 \r\n"

static int __fh_debug_proc_read(struct seq_file *m, void *v,
				struct fh_pll_t *pll)
{
	int id;

	FH_MSG("EN: %s", __func__);

	/* [WWK] Should remove PLL name to save porting time. */
	/* [WWK] Could print ENG ID and PLL mapping */

	seq_puts(m, "\r\n[freqhopping debug flag]\r\n");
	seq_puts(m, FH_DEBUG_PROC_READ_MSG_STATUS);
	seq_puts(m, FH_DEBUG_PROC_READ_MSG_SETTING_IDX);
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
#undef FH_DEBUG_PROC_READ_MSG_STATUS
#undef FH_DEBUG_PROC_READ_MSG_SETTING_IDX

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

	if (g_reg_pll_con0[fh_ctl->pll_id] == REG_PLL_NOT_SUPPORT) {
		FH_MSG("(ERROR) %s [pll_id]: %d freqhop is not supported!",
			__func__, fh_ctl->pll_id);
		return -1;
	}

	switch (ctlid) {
	case FH_DCTL_CMD_DVFS:	/* < PLL DVFS */
		{
			mt_fh_hal_hopping(fh_ctl->pll_id,
				fh_ctl->ssc_setting.dds);
		}
		break;
	case FH_DCTL_CMD_DVFS_SSC_ENABLE:	/* PLL DVFS and enable SSC */
		{
			__disable_ssc(fh_ctl->pll_id, &(fh_ctl->ssc_setting));
			mt_fh_hal_hopping(fh_ctl->pll_id,
				fh_ctl->ssc_setting.dds);
			__enable_ssc(fh_ctl->pll_id, &(fh_ctl->ssc_setting));
		}
		break;
	case FH_DCTL_CMD_DVFS_SSC_DISABLE:	/* PLL DVFS and disable SSC */
		{
			__disable_ssc(fh_ctl->pll_id, &(fh_ctl->ssc_setting));
			mt_fh_hal_hopping(fh_ctl->pll_id,
				fh_ctl->ssc_setting.dds);
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
			mt_fh_hal_general_pll_dfs(fh_ctl->pll_id,
				fh_ctl->ssc_setting.dds);
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
			struct FH_IO_PROC_READ_T *tmp =
				(struct FH_IO_PROC_READ_T *) (arg);

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
	//.mt_dfs_vencpll = mt_fh_hal_dfs_vencpll,
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

	if (g_reg_pll_con0[pll] == REG_PLL_NOT_SUPPORT) {
		FH_MSG("(ERROR) %s [pll_id]: %d freqhop is not supported",
				__func__, pll);
		return -1;
	}

	FH_MSG_DEBUG("%s for pll %d pause %d", __func__, pll, pause);

	switch (pll) {
	case FH_PLL0:
#if defined(CONFIG_ARCH_MT6765)
	case FH_PLL7:
#endif
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
		fh_set_field(reg_cfg, FH_FHCTLX_CFG_PAUSE, 1);	/* pause  */
	else
		fh_set_field(reg_cfg, FH_FHCTLX_CFG_PAUSE, 0);	/* no pause  */

	spin_unlock_irqrestore(&g_fh_lock, flags);

	return 0;
}
