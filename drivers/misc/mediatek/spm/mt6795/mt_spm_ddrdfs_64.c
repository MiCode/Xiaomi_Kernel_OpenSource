#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <asm/cacheflush.h>

#include <mach/mt_dramc.h>
#include <mach/mt_clkmgr.h>
#include <mach/mtk_ccci_helper.h>
#include <mach/mt_idle.h>
#include <mach/mt_spm_idle.h>
#include <mach/mt_spm_sleep.h>

#include "mt_spm_internal.h"

extern void mt_irq_mask_for_sleep(unsigned int irq);
extern void mt_irq_unmask_for_sleep(unsigned int irq);

/**************************************
 * Config and Parameter
 **************************************/
#define FW_PATCH_START_INDEX	6
#define FW_PATCH_DATA_NUM	42

#define SPARE_REG_FOR_LTE	MBIST_CFG_0	/* 0x10000308 in TOPCKGEN */
#define SPARE_REG_MAGIC		0x24546595

#define LTE_PTIME_SOFF		1		/* ms */

#define TWO_PAUSE_MIN_INTV	650		/* ms */
#define TWO_DFS_MIN_INTV	20		/* ms */

#define DDR_DFS_TIMEOUT		1000		/* 1000 * 100us = 100ms */

#define R2_PCM_RETURN		(1U << 0)

#define STAT_WAIT_LTE_PAUSE	(1U << 1)
#define STAT_ENTER_BUS_PROT	(1U << 3)
#define STAT_ENTER_EMI_SRF	(1U << 4)
#define STAT_LEAVE_EMI_SRF	(1U << 7)

#define WAKE_SRC_FOR_MD32	0x00002008


/**************************************
 * Define and Declare
 **************************************/
static u32 ddrdfs_binary[] = {
	0x1840001f, 0x00000001, 0xd00010c0, 0x11807c1f, 0xe8208000, 0x100040e0,
	0x55aa55aa, 0xe8208000, 0x10004404, 0x55aa55aa, 0xe8208000, 0x10004410,
	0x55aa55aa, 0xe8208000, 0x10004094, 0x55aa55aa, 0xe8208000, 0x10004118,
	0x55aa55aa, 0xe8208000, 0x10004418, 0x55aa55aa, 0xe8208000, 0x10004098,
	0x55aa55aa, 0xe8208000, 0x1000407c, 0x55aa55aa, 0xe8208000, 0x100040e4,
	0x55aa55aa, 0xe8208000, 0x100040f0, 0x55aa55aa, 0xe8208000, 0x10004080,
	0x55aa55aa, 0xe8208000, 0x10004138, 0x55aa55aa, 0xe8208000, 0x100041c4,
	0x55aa55aa, 0xe8208000, 0x1000441c, 0x55aa55aa, 0xe8208000, 0x10004420,
	0x55aa55aa, 0xe8208000, 0x10004424, 0x55aa55aa, 0xe8208000, 0x10004428,
	0x55aa55aa, 0xe8208000, 0x1000442c, 0x55aa55aa, 0xe8208000, 0x1000f430,
	0x55aa55aa, 0xe8208000, 0x1000f434, 0x55aa55aa, 0xe8208000, 0x1000f438,
	0x55aa55aa, 0xe8208000, 0x100110e0, 0x55aa55aa, 0xe8208000, 0x10011404,
	0x55aa55aa, 0xe8208000, 0x10011410, 0x55aa55aa, 0xe8208000, 0x10011094,
	0x55aa55aa, 0xe8208000, 0x10011118, 0x55aa55aa, 0xe8208000, 0x10011418,
	0x55aa55aa, 0xe8208000, 0x10011098, 0x55aa55aa, 0xe8208000, 0x1001107c,
	0x55aa55aa, 0xe8208000, 0x100110e4, 0x55aa55aa, 0xe8208000, 0x100110f0,
	0x55aa55aa, 0xe8208000, 0x10011080, 0x55aa55aa, 0xe8208000, 0x10011138,
	0x55aa55aa, 0xe8208000, 0x100111c4, 0x55aa55aa, 0xe8208000, 0x1001141c,
	0x55aa55aa, 0xe8208000, 0x10011420, 0x55aa55aa, 0xe8208000, 0x10011424,
	0x55aa55aa, 0xe8208000, 0x10011428, 0x55aa55aa, 0xe8208000, 0x1001142c,
	0x55aa55aa, 0xe8208000, 0x10012430, 0x55aa55aa, 0xe8208000, 0x10012434,
	0x55aa55aa, 0xe8208000, 0x10012438, 0x55aa55aa, 0x18d0001f, 0x100111c4,
	0xf0000000, 0x17c07c1f, 0x81000402, 0xd8002804, 0x1190841f, 0x18d0001f,
	0x10000308, 0xd80010c3, 0x17c07c1f, 0xe8208000, 0x10050090, 0x00004000,
	0x18d0001f, 0x10050090, 0x81170403, 0xd82011a4, 0x17c07c1f, 0x1810001f,
	0x10006010, 0x19d0001f, 0x10006014, 0xe8208000, 0x10006358, 0x00000081,
	0x1950001f, 0x10006b08, 0x81081401, 0xd8201dc4, 0x17c07c1f, 0x803d0400,
	0x18c0001f, 0x10006204, 0xe0e00005, 0x1b80001f, 0x20000034, 0xe0e00000,
	0x1b80001f, 0x20000152, 0xe8208000, 0x10006354, 0x00000210, 0x1a10001f,
	0x10008028, 0x1940001f, 0x0000000a, 0xa1d10407, 0x1b80001f, 0x20000014,
	0x1910001f, 0x1000691c, 0x80d20404, 0xd80017c3, 0x1191841f, 0x61200405,
	0xd8201644, 0x01600405, 0xd0002660, 0x1191841f, 0xc0c02820, 0x1192041f,
	0xd8202663, 0x17c07c1f, 0xa1da0407, 0xa0110400, 0xa0140400, 0xa1df0407,
	0x18c0001f, 0x10006b00, 0x1910001f, 0x1020e374, 0xe0c00004, 0x18c0001f,
	0x10006b04, 0x1910001f, 0x1020e378, 0xe0c00004, 0x18c0001f, 0x10006b60,
	0x1910001f, 0x10213374, 0xe0c00004, 0x18c0001f, 0x10006b64, 0x1910001f,
	0x10213378, 0xe0c00004, 0xa01d8400, 0xa1de8407, 0xc0c00080, 0x17c07c1f,
	0x80340400, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x80310400,
	0x81fa0407, 0x81ff0407, 0x81f08407, 0x81f18407, 0x81f10407, 0x1a50001f,
	0x10008028, 0x1193841f, 0xd0002660, 0x17c07c1f, 0xe8208000, 0x10006354,
	0x00000210, 0x1a10001f, 0x10008028, 0x1940001f, 0x0000000a, 0xa1d10407,
	0x1b80001f, 0x20000014, 0x1910001f, 0x1000691c, 0x80d20404, 0xd8002043,
	0x1191841f, 0x61200405, 0xd8201ec4, 0x01600405, 0xd0002660, 0x1191841f,
	0xc0c02820, 0x1192041f, 0xd8202663, 0x17c07c1f, 0xa1da0407, 0xa0110400,
	0xa0140400, 0xa1df0407, 0x18c0001f, 0x10006b00, 0x1910001f, 0x1020e374,
	0xe0c00004, 0x18c0001f, 0x10006b04, 0x1910001f, 0x1020e378, 0xe0c00004,
	0x18c0001f, 0x10006b60, 0x1910001f, 0x10213374, 0xe0c00004, 0x18c0001f,
	0x10006b64, 0x1910001f, 0x10213378, 0xe0c00004, 0x81fe8407, 0xc0c00080,
	0x17c07c1f, 0x803d8400, 0x1b80001f, 0x20000034, 0x80340400, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x80310400, 0x81fa0407, 0x81ff0407,
	0x81f08407, 0x81f18407, 0x81f10407, 0x1a50001f, 0x10008028, 0x1193841f,
	0xa01d0400, 0x81f10407, 0x18c0001f, 0x10006010, 0xe0c00000, 0x18c0001f,
	0x10006014, 0xe0c00007, 0xe8208000, 0x10006358, 0x00000000, 0xe8208000,
	0x10050090, 0x00004000, 0xf0000000, 0xa1d08407, 0xa1d18407, 0x1b80001f,
	0x20000020, 0x80cab401, 0x80cbb403, 0xd8002a43, 0x17c07c1f, 0x81f18407,
	0x81f08407, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x80e10801,
	0xd8002823, 0xa0910402, 0xf0000000, 0x17c07c1f
};
static struct pcm_desc ddrdfs_pcm = {
	.version	= "pcm_ddrdfs_v1.6_20141114",
	.base		= ddrdfs_binary,
	.size		= 340,
	.sess		= 3,		/* force IM to fetch code */
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
static void patch_fw_for_mempll_setting(struct pwr_ctrl *pwrctrl)
{
	int i;
	unsigned int num, *cha, *chb;
	unsigned long high_cha, high_chb, low_cha, low_chb;

	get_freq_table_info(&high_cha, &high_chb, &low_cha, &low_chb, &num);
	BUG_ON(num * 2 != FW_PATCH_DATA_NUM);	/* table is out of sync */

	if (is_ddr_high_speed(pwrctrl->pcm_flags)) {
		cha = (unsigned int *)high_cha;
		chb = (unsigned int *)high_chb;
		pwrctrl->pcm_flags &= ~SPM_MEMPLL_1PLL_EN;
	} else {
		cha = (unsigned int *)low_cha;
		chb = (unsigned int *)low_chb;
		pwrctrl->pcm_flags |= SPM_MEMPLL_1PLL_EN;
	}

	for (i = 0; i < num * 2; i++)
		ddrdfs_binary[FW_PATCH_START_INDEX + i * 3] = (i < num ? cha[i] : chb[i - num]);

	__flush_dcache_area(ddrdfs_binary + FW_PATCH_START_INDEX,
			    num * 2 * 3 * sizeof(ddrdfs_binary[0]));
}

static void kick_pcm_to_run(struct pwr_ctrl *pwrctrl)
{
	/* reduce config time: fddrphycfg_ck = SYSPLL1_D8 */
	clkmux_sel(MT_MUX_DDRPHY, 1, "SPM-DDRDFS");

	/* make PCM wait for SPARE_REG_FOR_LTE = 0 */
	spm_write(SPARE_REG_FOR_LTE, SPARE_REG_MAGIC);

	__spm_kick_pcm_to_run(pwrctrl);
}

u32 spm_ensure_lte_pause_interval(void)
{
	u32 remd;
	unsigned long long intv;

	if (pause_t1 == 0)
		return 0;

	/* the Spec from LTE L1 */
	intv = sched_clock() - pause_t1;
	remd = do_div(intv, TWO_PAUSE_MIN_INTV * 1000000);
	if (intv == 0)
		return TWO_PAUSE_MIN_INTV - remd / 1000000;

	return 0;
}

/*
 * return value:
 *   > 0: wait timeout
 *   = 0: PCM return
 *   < 0: CCCI error
 */
static int pause_lte_wait_pcm_return(struct pwr_ctrl *pwrctrl, bool do_pause, int *cnt)
{
	int ptime, r = -1;

	if (pause_lte && do_pause && spm_read(SPARE_REG_FOR_LTE)) {
		ptime = LTE_PTIME_SOFF;
		r = exec_ccci_kern_func_by_md_id(0, ID_PAUSE_LTE, (char *)&ptime, sizeof(ptime));
		spm_crit("ptime = %d, ccci_r = %d\n", ptime, r);

		if (r > 0)	/* MD is not ready */
			spm_write(SPARE_REG_FOR_LTE, 0);
		else if (r < 0)
			return r;
	} else {
		spm_write(SPARE_REG_FOR_LTE, 0);
	}

	*cnt = 0;
	while (!(spm_read(SPM_SLEEP_ISR_STATUS) & ISRS_PCM_RETURN)) {
		if (*cnt >= DDR_DFS_TIMEOUT)
			break;
		udelay(100);
		(*cnt)++;
	}

	if (r == 0 && is_ddr_high_speed(pwrctrl->pcm_flags))
		pause_t1 = sched_clock();

	return *cnt < DDR_DFS_TIMEOUT ? 0 : DDR_DFS_TIMEOUT;
}

static int wait_pcm_complete_ddrdfs(struct pwr_ctrl *pwrctrl, unsigned long *flags)
{
	int r, cnt = -1;
	u32 remd, r2, srf_us;
	unsigned long long intv;
	static u32 r6;
	static unsigned long long t1;

	spin_unlock_irqrestore(&__spm_lock, *flags);

	spm_ap_mdsrc_req(1);	/* WORKAROUND MDHW->EMI protect long response */

	if (r6 == STAT_LEAVE_EMI_SRF) {		/* the Spec from 3G L1/DE */
		intv = sched_clock() - t1;
		remd = do_div(intv, TWO_DFS_MIN_INTV * 1000000);
		if (intv == 0)
			mdelay(TWO_DFS_MIN_INTV - remd / 1000000);
		spm_debug("intv = %llu, remd = %u\n", intv, remd);
	}

	r = pause_lte_wait_pcm_return(pwrctrl, true, &cnt);

	if (r > 0 && spm_read(SPM_PCM_REG6_DATA) == STAT_WAIT_LTE_PAUSE) {
		spm_crit("SKIP LTE PAUSE DUE TO TIMEOUT\n");
		r = pause_lte_wait_pcm_return(pwrctrl, false, &cnt);
	}

	if (r == 0 && spm_read(SPM_PCM_REG6_DATA) == STAT_LEAVE_EMI_SRF)
		t1 = sched_clock();

	spm_ap_mdsrc_req(0);	/* WORKAROUND MDHW->EMI protect long response */

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

	if (r6 == STAT_WAIT_LTE_PAUSE || r6 == STAT_ENTER_BUS_PROT ||
	    r6 == STAT_ENTER_EMI_SRF) {
		r = -EBUSY;
	} else {
		BUG_ON(r6 != STAT_LEAVE_EMI_SRF);
		r = 0;

		/* save DQS HW gating value to the table of previous mode */
		updae_gating_value(pwrctrl->pcm_flags & SPM_MEMPLL_1PLL_EN ? 1 : 0,
				   spm_read(SPM_PCM_RESERVE),
				   spm_read(SPM_PCM_RESERVE2),
				   spm_read(SPM_PCM_PASR_DPD_0),
				   spm_read(SPM_PCM_PASR_DPD_1));
	}

	return r;
}

static void clean_after_wakeup(void)
{
	__spm_clean_after_wakeup();

	/* restore clock mux: fddrphycfg_ck = CLK26M */
	clkmux_sel(MT_MUX_DDRPHY, 0, "SPM-DDRDFS");
}

int spm_go_to_ddrdfs(u32 spm_flags, u32 spm_data)
{
	int r = 0;
	unsigned long flags;
	struct pcm_desc *pcmdesc = __spm_ddrdfs.pcmdesc;
	struct pwr_ctrl *pwrctrl = __spm_ddrdfs.pwrctrl;

	idle_lock_spm(IDLE_SPM_LOCK_VCORE_DVFS);
	spm_mcdi_switch_on_off(SPM_MCDI_VCORE_DVFS, 0);

	spin_lock_irqsave(&__spm_lock, flags);
	mt_irq_mask_for_sleep(SPM_IRQ0_ID);	/* avoid getting spm_irq0 */

	set_pwrctrl_pcm_flags(pwrctrl, spm_flags);

	if (is_ddr_high_speed(pwrctrl->pcm_flags) ^ is_in_mem1pll_mode())
		goto OUT;	/* no need to change */

	patch_fw_for_mempll_setting(pwrctrl);

	__spm_reset_and_init_pcm(pcmdesc);

	__spm_kick_im_to_fetch(pcmdesc);

	__spm_init_pcm_register();

	__spm_init_event_vector(pcmdesc);

	__spm_set_power_control(pwrctrl);

	__spm_set_wakeup_event(pwrctrl);

	kick_pcm_to_run(pwrctrl);

	r = wait_pcm_complete_ddrdfs(pwrctrl, &flags);

	clean_after_wakeup();

OUT:
	mt_irq_unmask_for_sleep(SPM_IRQ0_ID);
	spin_unlock_irqrestore(&__spm_lock, flags);

	spm_mcdi_switch_on_off(SPM_MCDI_VCORE_DVFS, 1);
	idle_unlock_spm(IDLE_SPM_LOCK_VCORE_DVFS);

	return r;
}

MODULE_DESCRIPTION("SPM-DDRDFS Driver v0.3");
