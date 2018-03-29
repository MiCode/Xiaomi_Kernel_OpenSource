/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <asm/cacheflush.h>

#include "mt_vcore_dvfs.h"
#if OLD_VCORE_DVFS_FORMAT
#include <mach/mtk_ccci_helper.h>
#include <mach/mt_spm_idle.h>
#include <mach/mt_spm_sleep.h>
#else
#include "mt_idle.h"
#include "mt_spm_idle.h"
void __attribute__((weak)) spm_mcdi_switch_on_off(enum spm_mcdi_lock_id id, int mcdi_en)
{
}
#include <mach/mt_dramc.h>


#endif

#include "mt_spm_internal.h"


/**************************************
 * Config and Parameter
 **************************************/
#define FW_PATCH_START_INDEX	6
#define FW_PATCH_DATA_NUM	30

#define SPARE_REG_FOR_LTE	0xf0000308	/* in TOPCKGEN */

#define SPARE_REG_MAGIC		0x24546595

#define LTE_PTIME_SOFF		1		/* ms */

#define TWO_PAUSE_MIN_INTV	650		/* ms */
#define TWO_DFS_MIN_INTV	20		/* ms */

#define DDR_DFS_TIMEOUT		1000		/* 1000 * 100us = 100ms */

#define R2_PCM_RETURN		(1U << 0)
#define R2_SKIP_DSI		(1U << 1)

#define STAT_WAIT_LTE_PAUSE	(1U << 1)
#define STAT_WAIT_DSI_BLANK	(1U << 2)
#define STAT_ENTER_BUS_PROT	(1U << 3)
#define STAT_ENTER_EMI_SRF	(1U << 4)
#define STAT_LEAVE_EMI_SRF	(1U << 7)

#define WAKE_SRC_FOR_MD32	0x00002008


/**************************************
 * Define and Declare
 **************************************/
static u32 ddrdfs_binary[] = {
	0x1840001f, 0x00000001, 0xd0000c40, 0x11807c1f, 0xe8208000, 0x1000f614,
	0xd3040300, 0xe8208000, 0x1000f620, 0xd3040300, 0xe8208000, 0x1000f62c,
	0xd3040300, 0xe8208000, 0x10012614, 0xd3040300, 0xe8208000, 0x10012620,
	0xd3040300, 0xe8208000, 0x1001262c, 0xd3040300, 0xe8208000, 0x1020928c,
	0x00000003, 0xe8208000, 0x1020928c, 0x00000001, 0xe8208000, 0x10209284,
	0x800fab52, 0xe8208000, 0x10209284, 0x000fab52, 0xe8208000, 0x10209280,
	0x00010100, 0xe8208000, 0x10209280, 0x00010101, 0xe8208000, 0x1000f618,
	0x4e00c000, 0xe8208000, 0x1000f624, 0x4e00c000, 0xe8208000, 0x1000f630,
	0x4e00c000, 0xe8208000, 0x10012618, 0x4e00c000, 0xe8208000, 0x10012624,
	0x4e00c000, 0xe8208000, 0x10012630, 0x4e00c000, 0xe8208000, 0x1000f61c,
	0x00021401, 0xe8208000, 0x1000f628, 0x00021401, 0xe8208000, 0x1000f634,
	0x00001401, 0xe8208000, 0x1001261c, 0x00021401, 0xe8208000, 0x10012628,
	0x00021401, 0xe8208000, 0x10012634, 0x00001401, 0xe8208000, 0x1000f614,
	0xd3040301, 0xe8208000, 0x1000f620, 0xd3040301, 0xe8208000, 0x1000f62c,
	0xd3040301, 0xe8208000, 0x10012614, 0xd3040301, 0xe8208000, 0x10012620,
	0xd3040301, 0xe8208000, 0x1001262c, 0xd3040301, 0x18d0001f, 0x1001262c,
	0xf0000000, 0x17c07c1f, 0x19d0001f, 0x10006014, 0xe8208000, 0x10006358,
	0x00000081, 0x1a10001f, 0x10006b04, 0x82462001, 0xd8200f49, 0x18d0001f,
	0x10006314, 0x1900001f, 0x10006314, 0xa8c00003, 0x0b160020, 0xe1000003,
	0xa1d50407, 0x81f50407, 0xa1d48407, 0x1900001f, 0x10006b60, 0x18d0001f,
	0x10006360, 0xe1000003, 0x8245a001, 0xd80018e9, 0x17c07c1f, 0x82402001,
	0xd80010e9, 0x17c07c1f, 0x81000402, 0xd80018e4, 0x1190841f, 0x18d0001f,
	0x10000308, 0xd8001003, 0x17c07c1f, 0x8240a001, 0xd8001449, 0x17c07c1f,
	0x81108402, 0xd8001544, 0x1191041f, 0x81000402, 0xd80018e4, 0x17c07c1f,
	0x18d0001f, 0x1401b160, 0x1910001f, 0x1401c160, 0xa0c01003, 0x8ac00003,
	0x00000018, 0x18d0001f, 0x1401b164, 0x1910001f, 0x1401c164, 0xa0c01003,
	0xb8c00163, 0x0000e000, 0xd80011a3, 0x17c07c1f, 0x1b80001f, 0x20000256,
	0x1900001f, 0x10006b64, 0x18d0001f, 0x10006360, 0xe1000003, 0x82412001,
	0xd8001649, 0x17c07c1f, 0xe8208000, 0x10050090, 0x00004000, 0x18d0001f,
	0x10050090, 0x81170403, 0xd8201544, 0x17c07c1f, 0x8241a001, 0xd8001789,
	0x17c07c1f, 0x1900001f, 0x10006b68, 0x18d0001f, 0x10006360, 0xe1000003,
	0xc0c01900, 0x17c07c1f, 0xe8208000, 0x10006358, 0x00000000, 0xe8208000,
	0x10050090, 0x00004000, 0x1900001f, 0x10006b6c, 0x18d0001f, 0x10006360,
	0xe1000003, 0xf0000000, 0x82442001, 0xd80019c9, 0x17c07c1f, 0xe8208000,
	0x10006834, 0x00000000, 0x8244a001, 0xd8001aa9, 0x17c07c1f, 0x1950001f,
	0x100041c0, 0x1a90001f, 0x100111c0, 0xe8208000, 0x10006354, 0x00000210,
	0x82452001, 0xd8001bc9, 0x17c07c1f, 0xa1d10407, 0x1b80001f, 0x20000014,
	0x1191841f, 0x82422001, 0xd8001d49, 0x17c07c1f, 0xc0c02b40, 0x1192041f,
	0xd8202a43, 0x17c07c1f, 0xa1da0407, 0xa0110400, 0xa0140400, 0xa0180400,
	0x82442001, 0xd8001e69, 0x17c07c1f, 0xe8208000, 0x10006830, 0x65930009,
	0xe8208000, 0x10006834, 0x00000001, 0x82432001, 0xd8001f89, 0x17c07c1f,
	0x18c0001f, 0x10209f00, 0x1910001f, 0x10209f00, 0x81318404, 0xe0c00004,
	0x8242a001, 0xd8002029, 0x17c07c1f, 0xc0c00080, 0x17c07c1f, 0x82422001,
	0xd8002189, 0x17c07c1f, 0x80380400, 0xa01d8400, 0x1b80001f, 0x20000034,
	0x803d8400, 0x1b80001f, 0x20000152, 0x80340400, 0x8243a001, 0xd80022a9,
	0x17c07c1f, 0x18d0001f, 0x10006830, 0x68e00003, 0x0000beef, 0xd82021e3,
	0x17c07c1f, 0x80310400, 0x8244a001, 0xd80023e9, 0x17c07c1f, 0x18c0001f,
	0x100041c0, 0xe0c00005, 0x18c0001f, 0x100111c0, 0xe0c0000a, 0x81f18407,
	0x81f08407, 0x1193841f, 0x18c0001f, 0x1000f0f0, 0x1910001f, 0x1000f0f0,
	0xe0c00004, 0x17c07c1f, 0x18c0001f, 0x100040f4, 0x1910001f, 0x100040f4,
	0xe0c00004, 0x17c07c1f, 0x18c0001f, 0x1000f0f0, 0x1910001f, 0x1000f0f0,
	0xe0c00004, 0x17c07c1f, 0x18c0001f, 0x100040f4, 0x1910001f, 0x100040f4,
	0xe0c00004, 0x17c07c1f, 0x18c0001f, 0x100120f0, 0x1910001f, 0x100120f0,
	0xe0c00004, 0x17c07c1f, 0x18c0001f, 0x100110f4, 0x1910001f, 0x100110f4,
	0xe0c00004, 0x17c07c1f, 0x18c0001f, 0x100120f0, 0x1910001f, 0x100120f0,
	0xe0c00004, 0x17c07c1f, 0x18c0001f, 0x100110f4, 0x1910001f, 0x100110f4,
	0xe0c00004, 0x17c07c1f, 0x81f10407, 0x1a50001f, 0x10008028, 0xe8208000,
	0x10006834, 0x00000010, 0xf0000000, 0x17c07c1f, 0xa1d08407, 0xa1d18407,
	0x1b80001f, 0x20000020, 0x80cab401, 0x80cbb403, 0xd8002d63, 0x17c07c1f,
	0x81f18407, 0x81f08407, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x80e10801, 0xd8002b43, 0xa0910402, 0xf0000000, 0x17c07c1f
};
static struct pcm_desc ddrdfs_pcm = {
	.version	= "mt8173_pcm_ddrdfs_v1.3_2015_0922",
	.base		= ddrdfs_binary,
	.size		= 365,
	.sess		= 3, /* force IM to fetch code */
	.replace	= 1,
};

static struct pwr_ctrl ddrdfs_ctrl = {
	/* r0 and r7 control will be enabled by PCM itself */
	.infra_dcm_lock		= 1,
	.param1			= 1,	/* pause_lte */
	.wake_src_md32		= WAKE_SRC_FOR_MD32,
};

struct spm_lp_scen __spm_ddrdfs = {
	.pcmdesc	= &ddrdfs_pcm,
	.pwrctrl	= &ddrdfs_ctrl,
};

#define pause_lte		(__spm_ddrdfs.pwrctrl->param1)

static unsigned long long pause_t1;


/**************************************
 * Function and API
 **************************************/
void patch_fw_for_mempll_setting(struct pwr_ctrl *pwrctrl)
{
	int i;

	unsigned int	size = 0;
	u32 *table = 0;
	unsigned long high_base = 0;
	unsigned long low_base = 0;

	/* store DRAM 1333 / 1600 MHz calibration data */
	get_mempll_table_info(&high_base, &low_base, &size);

	table = (u32 *)(pwrctrl->pcm_flags & SPM_DDR_HIGH_SPEED ? high_base : low_base);

	BUG_ON(size != FW_PATCH_DATA_NUM);	/* table is out of sync */

	for (i = 0; i < FW_PATCH_DATA_NUM; i++)
		ddrdfs_binary[FW_PATCH_START_INDEX + i * 3] = table[i];

#if defined(CONFIG_ARM64)
	__flush_dcache_area(ddrdfs_binary + FW_PATCH_START_INDEX,
				 FW_PATCH_DATA_NUM * 3 * sizeof(ddrdfs_binary[0]));
#else
	__cpuc_flush_dcache_area(ddrdfs_binary + FW_PATCH_START_INDEX,
				 FW_PATCH_DATA_NUM * 3 * sizeof(ddrdfs_binary[0]));
#endif
}


void kick_pcm_to_run(struct pwr_ctrl *pwrctrl)
{
	/* make PCM skip DSI check if screen is off */
	if (pwrctrl->pcm_flags & SPM_SCREEN_OFF) {
		spm_write(SPM_PCM_REG_DATA_INI, R2_SKIP_DSI);
		spm_write(SPM_PCM_PWR_IO_EN, PCM_RF_SYNC_R2);
		spm_write(SPM_PCM_PWR_IO_EN, 0);
	}

	/* mt8173 not LTE VCORE-DVFS-TBD */
	/* make PCM wait for SPARE_REG_FOR_LTE = 0 */
	/* mt8173 NO SPARE_REG_FOR_LTE TBD */
	/* spm_write(SPARE_REG_FOR_LTE, SPARE_REG_MAGIC); */

	__spm_kick_pcm_to_run(pwrctrl);
}


void spm_ensure_lte_pause_interval(void)
{
	u32 remd;
	unsigned long long intv;

	if (pause_t1 == 0)
		return;

	/* the Spec from LTE L1 */
	intv = sched_clock() - pause_t1;
	remd = do_div(intv, TWO_PAUSE_MIN_INTV * 1000000);
	if (intv == 0)
		msleep(TWO_PAUSE_MIN_INTV - remd / 1000000);
	spm_debug("intv - %llu, remd - %u\n", intv, remd);
}


/*
 * return value:
 *   > 0: wait timeout
 *   = 0: PCM return
 *   < 0: CCCI error
 */
/* mt8173 not LTE VCORE-DVFS-TBD BARRY - Remove all ? */
int pause_lte_wait_pcm_return(struct pwr_ctrl *pwrctrl, bool do_pause, int *cnt)
{
	*cnt = 0;
	while (!(spm_read(SPM_SLEEP_ISR_STATUS) & ISRS_PCM_RETURN)) {
		if (*cnt >= DDR_DFS_TIMEOUT)
			return *cnt;
		udelay(100);
		(*cnt)++;
	}

	return 0;
}

int wait_pcm_complete_ddrdfs(struct pwr_ctrl *pwrctrl, unsigned long *flags)
{
	int r, cnt = -1;
	u32	r2, srf_us;
	/* mt8173 not LTE VCORE-DVFS-TBD */
	u32 remd;
	unsigned long long intv;
	static unsigned long long t1;

	static u32 r6;

	/* mt8173 not LTE VCORE-DVFS-TBD */
	spin_unlock_irqrestore(&__spm_lock, *flags);

	if (r6 == STAT_LEAVE_EMI_SRF) {		/* the Spec from 3G L1/DE */
		intv = sched_clock() - t1;
		remd = do_div(intv, TWO_DFS_MIN_INTV * 1000000);
		if (intv == 0)
			mdelay(TWO_DFS_MIN_INTV - remd / 1000000);
		spm_debug("intv = %llu, remd = %u\n", intv, remd);
	}

	r = pause_lte_wait_pcm_return(pwrctrl, true, &cnt);

	if (r >= 0)
		pause_t1 = sched_clock();

	if (r > 0 && spm_read(SPM_PCM_REG6_DATA) == STAT_WAIT_LTE_PAUSE) {
		spm_crit("SKIP LTE PAUSE DUE TO TIMEOUT\n");
		r = pause_lte_wait_pcm_return(pwrctrl, false, &cnt);
	}

	if (r == 0 && spm_read(SPM_PCM_REG6_DATA) == STAT_LEAVE_EMI_SRF)
		t1 = sched_clock();

	spin_lock_irqsave(&__spm_lock, *flags);
	if (r) {
		/* make PCM stop polling and then return */
		spm_write(SPM_PCM_REG_DATA_INI, spm_read(SPM_PCM_REG2_DATA) | R2_PCM_RETURN);
		spm_write(SPM_PCM_PWR_IO_EN, PCM_RF_SYNC_R2);
		spm_write(SPM_PCM_PWR_IO_EN, 0);
		udelay(30);
	}

	r6 = spm_read(SPM_PCM_REG6_DATA);
	r2 = spm_read(SPM_PCM_REG2_DATA);
	srf_us = (spm_read(SPM_PCM_REG9_DATA) - spm_read(SPM_PCM_REG8_DATA)) / 13;
	spm_crit("r6 = 0x%x, r2 = 0x%x, srf_us = %u, cnt = %d\n",
		 r6, r2, srf_us, cnt);

	BUG_ON(!(spm_read(SPM_SLEEP_ISR_STATUS) & ISRS_PCM_RETURN));	/* PCM hang */

	if (r6 == STAT_WAIT_LTE_PAUSE || r6 == STAT_WAIT_DSI_BLANK ||
	    r6 == STAT_ENTER_BUS_PROT || r6 == STAT_ENTER_EMI_SRF) {
		r = -EBUSY;
	} else {
		BUG_ON(r6 != STAT_LEAVE_EMI_SRF);	/* impossible? */
		r = 0;

		/* re-config MPLL hopping since it is disabled by PCM */
		/* Don't use FHCTL for bug recover */
		/* freqhopping_config(FH_M_PLLID, 0, 1); */
	}

	return r;
}

void clean_after_wakeup(void)
{
	__spm_clean_after_wakeup();
}

#define	PDEF_PCM_RESERVE2	(SPM_BASE + 0xb04)

#define	PDEF_DISABLE_WAIT_LTE			(1U << 0)
#define	PDEF_DISABLE_DSI_CHECK			(1U << 1)
#define	PDEF_DISABLE_WAIT_SEMAPHORE		(1U << 2)
#define	PDEF_DISABLE_MEMPLL_DFS			(1U << 3)
#define	PDEF_DISABLE_EMI_MEMPLL_OFF		(1U << 4)	/* disable emi & mempll */
#define	PDEF_DISABLE_PLLGP_SETTING		(1U << 5)	/* disable mempll */
/* bit#6 don't set mpll_hp_en */
#define	PDEF_SKIP_POLLING_MD32_FINISH	(1U << 7)	/* spm don't send mail box */
#define	PDEF_DNT_FIRE_MD32				(1U << 8)	/* spm don't send mail box */
#define	PDEF_DISABLE_SAVE_RESTORE_HW_GATING	(1U << 9)	/* HW gatting enable */
#define	PDEF_DISABLE_BUS_PROTECT	(1U << 10)	/* Set R7 bit 2 */
#define	PDEF_DISABLE_ALL			(1U << 11)	/* skip all */
#define PDEF_ENABLE_TIME_STAMP		(1U << 12)	/* enable timestamp */

#define PDEF_PCM_PASR_DPD_0  (SPM_BASE + 0xB60)
#define PDEF_PCM_PASR_DPD_1  (SPM_BASE + 0xB64)
#define PDEF_PCM_PASR_DPD_2  (SPM_BASE + 0xB68)
#define PDEF_PCM_PASR_DPD_3  (SPM_BASE + 0xB6C)
u32 pcm_pasr_dpd_0 = 0;
u32 pcm_pasr_dpd_1 = 0;
u32 pcm_pasr_dpd_2 = 0;
u32 pcm_pasr_dpd_3 = 0;


int spm_go_to_ddrdfs(u32 spm_flags, u32 spm_data)
{
	int r = 0;
	unsigned long flags;
	struct pcm_desc *pcmdesc = __spm_ddrdfs.pcmdesc;
	struct pwr_ctrl *pwrctrl = __spm_ddrdfs.pwrctrl;
	struct irq_desc *desc;

	/* reduce config time: fddrphycfg_ck = SYSPLL1_D8, fscp_ck = SYSPLL1_D2 */
	vcore_dvfs_config_speed(1);

	idle_lock_spm(IDLE_SPM_LOCK_VCORE_DVFS);
	spm_mcdi_switch_on_off(SPM_MCDI_VCORE_DVFS, 0);

	spin_lock_irqsave(&__spm_lock, flags);
	desc = irq_to_desc(SPM_IRQ0_ID);
	if (desc)
		mask_irq(desc);

	set_pwrctrl_pcm_flags(pwrctrl, spm_flags);

	patch_fw_for_mempll_setting(pwrctrl);

	__spm_reset_and_init_pcm(pcmdesc);

	__spm_kick_im_to_fetch(pcmdesc);

	__spm_init_pcm_register();

	__spm_init_event_vector(pcmdesc);

	__spm_set_power_control(pwrctrl);

	__spm_set_wakeup_event(pwrctrl);

	/* Barry debug bit to disable not used feature */
	/* spm_write(PDEF_PCM_RESERVE2, setting_value); */

	kick_pcm_to_run(pwrctrl);

	r = wait_pcm_complete_ddrdfs(pwrctrl, &flags);

	clean_after_wakeup();

	if (desc)
		unmask_irq(desc);
	spin_unlock_irqrestore(&__spm_lock, flags);

	spm_mcdi_switch_on_off(SPM_MCDI_VCORE_DVFS, 1);
	idle_unlock_spm(IDLE_SPM_LOCK_VCORE_DVFS);

	/* restore clock mux: fddrphycfg_ck = CLK26M */
	vcore_dvfs_config_speed(0);

	return r;
}

MODULE_DESCRIPTION("SPM-DDRDFS Driver v0.3");
