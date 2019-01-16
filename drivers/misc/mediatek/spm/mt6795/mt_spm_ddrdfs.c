#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <asm/cacheflush.h>

#include <mach/mt_dramc.h>
#include <mach/mt_clkmgr.h>
#include <mach/mtk_ccci_helper.h>
#include <mach/mt_freqhopping.h>
#include <mach/mt_idle.h>
#include <mach/mt_spm_idle.h>

#include "mt_spm_internal.h"

extern void mt_irq_mask_for_sleep(unsigned int irq);
extern void mt_irq_unmask_for_sleep(unsigned int irq);

/**************************************
 * Config and Parameter
 **************************************/
#define FW_PATCH_START_INDEX	6
#define FW_PATCH_DATA_NUM	20

#define SPARE_REG_FOR_LTE	MBIST_CFG_0	/* 0x10000308 in TOPCKGEN */
#define SPARE_REG_MAGIC		0x24546595

#define LTE_PTIME_SON		17		/* ms */
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


/**************************************
 * Define and Declare
 **************************************/
static u32 ddrdfs_binary[] = {
	0x1840001f, 0x00000001, 0xd0000880, 0x11807c1f, 0xe8208000, 0x10209284,
	0x800ef41a, 0xe8208000, 0x10209284, 0x000ef41a, 0xe8208000, 0x1000f618,
	0x4e00c000, 0xe8208000, 0x1000f630, 0x4e00c000, 0xe8208000, 0x10012618,
	0x4e00c000, 0xe8208000, 0x10012630, 0x4e00c000, 0xe8208000, 0x1000f668,
	0x4e00c000, 0xe8208000, 0x1000f674, 0x4e00c000, 0xe8208000, 0x1001261c,
	0x00021401, 0xe8208000, 0x10012634, 0x00001401, 0xe8208000, 0x1000f61c,
	0x00021401, 0xe8208000, 0x1000f634, 0x00001401, 0xe8208000, 0x1000f66c,
	0x00021401, 0xe8208000, 0x1000f678, 0x00001401, 0xe8208000, 0x1000f614,
	0x87050601, 0xe8208000, 0x1000f62c, 0x87050601, 0xe8208000, 0x1000f664,
	0x87050601, 0xe8208000, 0x1000f670, 0x87050601, 0xe8208000, 0x10012614,
	0x87050601, 0xe8208000, 0x1001262c, 0x87050601, 0x18d0001f, 0x1001262c,
	0xf0000000, 0x17c07c1f, 0x81000402, 0xd8000ee4, 0x1190841f, 0x18d0001f,
	0x10000308, 0xd8000883, 0x81108402, 0xd8000c44, 0x1191041f, 0x81000402,
	0xd8000ee4, 0x17c07c1f, 0x18d0001f, 0x1401b160, 0x1910001f, 0x1401c160,
	0xa0c01003, 0x8ac00003, 0x00000018, 0x18d0001f, 0x1401b164, 0x1910001f,
	0x1401c164, 0xa0c01003, 0xb8c00163, 0x0000e000, 0xd80009a3, 0x17c07c1f,
	0x1b80001f, 0x20000256, 0xe8208000, 0x10050090, 0x00004000, 0x18d0001f,
	0x10050090, 0x81170403, 0xd8200c44, 0x17c07c1f, 0x19d0001f, 0x10006014,
	0xe8208000, 0x10006358, 0x00000081, 0xc0c00f00, 0x17c07c1f, 0xe8208000,
	0x10006358, 0x00000000, 0xe8208000, 0x10050090, 0x00004000, 0xf0000000,
	0xe8208000, 0x10006834, 0x00000000, 0x1950001f, 0x100041c0, 0x1a90001f,
	0x100111c0, 0xe8208000, 0x10006354, 0x00000210, 0x1a10001f, 0x10008028,
	0xa1d10407, 0x1b80001f, 0x20000014, 0x1910001f, 0x1000691c, 0x80d20404,
	0xd8201743, 0x1191841f, 0xc0c01840, 0x1192041f, 0xd8201743, 0x17c07c1f,
	0xa1da0407, 0xa0110400, 0xa0140400, 0xa0180400, 0xe8208000, 0x10006830,
	0x65930009, 0xe8208000, 0x10006834, 0x00000001, 0x18c0001f, 0x10209f00,
	0x1910001f, 0x10209f00, 0x81318404, 0xe0c00004, 0xc0c00080, 0x17c07c1f,
	0x80380400, 0xa01d8400, 0x1b80001f, 0x20000034, 0x803d8400, 0x1b80001f,
	0x20000152, 0x80340400, 0x18d0001f, 0x10006830, 0x68e00003, 0x0000beef,
	0xd8201543, 0x17c07c1f, 0x80310400, 0x18c0001f, 0x100041c0, 0xe0c00005,
	0x18c0001f, 0x100111c0, 0xe0c0000a, 0x81f18407, 0x81f08407, 0x1193841f,
	0x81f10407, 0x1a50001f, 0x10008028, 0xe8208000, 0x10006834, 0x00000010,
	0xf0000000, 0x17c07c1f, 0xa1d08407, 0xa1d18407, 0x1b80001f, 0x20000020,
	0x80cab401, 0x80cbb403, 0xd8001a63, 0x17c07c1f, 0x81f18407, 0x81f08407,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x80e10801, 0xd8001843,
	0xa0910402, 0xf0000000, 0x17c07c1f
};
static struct pcm_desc ddrdfs_pcm = {
	.version	= "pcm_ddrdfs_v3.2_2014_0702",
	.base		= ddrdfs_binary,
	.size		= 213,
	.sess		= 3,		/* force IM to fetch code */
	.replace	= 1,
};

static struct pwr_ctrl ddrdfs_ctrl = {
	/* r0 and r7 control will be enabled by PCM itself */
	.infra_dcm_lock		= 1,
	.param1			= 1,	/* pause_lte */
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
	u32 high_base, low_base, size, *table;

	get_mempll_table_info(&high_base, &low_base, &size);

	table = (u32 *)(pwrctrl->pcm_flags & SPM_DDR_HIGH_SPEED ? high_base : low_base);
	BUG_ON(size != FW_PATCH_DATA_NUM);	/* table is out of sync */

	for (i = 0; i < FW_PATCH_DATA_NUM; i++)
		ddrdfs_binary[FW_PATCH_START_INDEX + i * 3] = table[i];

	__cpuc_flush_dcache_area(ddrdfs_binary + FW_PATCH_START_INDEX,
				 FW_PATCH_DATA_NUM * 3 * sizeof(ddrdfs_binary[0]));
}

static void kick_pcm_to_run(struct pwr_ctrl *pwrctrl)
{
	/* reduce config time: fddrphycfg_ck = SYSPLL1_D8, fscp_ck = SYSPLL1_D2 */
	clkmux_sel(MT_MUX_DDRPHY, 1, "SPM-DDRDFS");
	clkmux_sel(MT_MUX_SCP, 1, "SPM-DDRDFS");

	/* make PCM skip DSI check if screen is off */
	if (pwrctrl->pcm_flags & SPM_SCREEN_OFF) {
		spm_write(SPM_PCM_REG_DATA_INI, R2_SKIP_DSI);
		spm_write(SPM_PCM_PWR_IO_EN, PCM_RF_SYNC_R2);
		spm_write(SPM_PCM_PWR_IO_EN, 0);
	}

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
		msleep(TWO_PAUSE_MIN_INTV - remd / 1000000);
	spm_debug("intv - %llu, remd - %u\n", intv, remd);

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
	int ptime, r;

	if (pause_lte && do_pause && spm_read(SPARE_REG_FOR_LTE)) {
		ptime = (pwrctrl->pcm_flags & SPM_SCREEN_OFF ? LTE_PTIME_SOFF : LTE_PTIME_SON);
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
			return *cnt;
		udelay(100);
		(*cnt)++;
	}

	return 0;
}

static int wait_pcm_complete_ddrdfs(struct pwr_ctrl *pwrctrl, unsigned long *flags)
{
	int r, cnt = -1;
	u32 remd, r2, srf_us;
	unsigned long long intv;
	static u32 r6;
	static unsigned long long t1;

	mt_irq_mask_for_sleep(SPM_IRQ0_ID);	/* avoid getting spm_irq0 */
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
	mt_irq_unmask_for_sleep(SPM_IRQ0_ID);
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
		freqhopping_config(FH_M_PLLID, 0, 1);
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
	int r;
	unsigned long flags;
	struct pcm_desc *pcmdesc = __spm_ddrdfs.pcmdesc;
	struct pwr_ctrl *pwrctrl = __spm_ddrdfs.pwrctrl;

	idle_lock_spm(IDLE_SPM_LOCK_VCORE_DVFS);
	spm_mcdi_switch_on_off(SPM_MCDI_VCORE_DVFS, 0);

	spin_lock_irqsave(&__spm_lock, flags);
	set_pwrctrl_pcm_flags(pwrctrl, spm_flags);

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
	spin_unlock_irqrestore(&__spm_lock, flags);

	spm_mcdi_switch_on_off(SPM_MCDI_VCORE_DVFS, 1);
	idle_unlock_spm(IDLE_SPM_LOCK_VCORE_DVFS);

	return r;
}

MODULE_DESCRIPTION("SPM-DDRDFS Driver v0.3");
