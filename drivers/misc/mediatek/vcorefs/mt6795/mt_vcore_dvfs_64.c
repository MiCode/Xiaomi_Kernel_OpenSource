#define pr_fmt(fmt)		"[VcoreFS] " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/wakelock.h>
#include <linux/kthread.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/sched/rt.h>

#include <mach/mt_vcore_dvfs.h>
#include <mach/mt_mem_bw.h>
#include <mach/mt_pmic_wrap.h>
#include <mach/mt_cpufreq.h>
#include <mach/mt_dramc.h>
#include <mach/mt_spm.h>
#include <mach/mt_clkmgr.h>
#include <mach/mt_smi.h>
#include <mach/mt_ptp.h>
#include <mach/board.h>
#include <mt_sd_func.h>
#include <primary_display.h>

/**************************************
 * Config and Parameter
 **************************************/
#define VCORE_SET_CHECK		0
#define FDDR_SET_CHECK		0

#define NORM_SEG_FAVOR_PERF	DISABLE_FLIPPER_FUNC

#define FDDR_S0_KHZ		1600000
#define FDDR_S1_KHZ		1466000
#define FDDR_S2_KHZ		1333000
#define FDDR_S3_KHZ		800000		/* 1600-800, 1466-814, 1333-799 */

#define FAXI_S1_KHZ		273000		/* MUX_1 */
#define FAXI_S2_KHZ		218400		/* MUX_2 */

#define FMM_S1_KHZ		400000		/* FH */
#define FMM_S2_KHZ		317000		/* FH */

#define FVENC_S1_KHZ		494000		/* FH */
#define FVENC_S2_KHZ		324000		/* FH */

#define FVDEC_S1_KHZ		494000		/* FH */
#define FVDEC_S2_KHZ		324000		/* FH */

#define DVFS_CMD_SETTLE_US	5	/* (PWRAP->1.5us->PMIC->10mv/1us) * 2 */
#define DRAM_WINDOW_SHIFT_MAX	80

#define SCREEN_RES_FHD		(1216 * 1920)

#define DVFS_DELAY_IGNORE_MS	10


/**************************************
 * Macro and Inline
 **************************************/
#define DEFINE_ATTR_RO(_name)			\
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = #_name,			\
		.mode = 0444,			\
	},					\
	.show	= _name##_show,			\
}

#define DEFINE_ATTR_RW(_name)			\
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = #_name,			\
		.mode = 0644,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}

#define __ATTR_OF(_name)	(&_name##_attr.attr)

#define vcorefs_emerg(fmt, args...)	pr_emerg(fmt, ##args)
#define vcorefs_alert(fmt, args...)	pr_alert(fmt, ##args)
#define vcorefs_crit(fmt, args...)	pr_crit(fmt, ##args)
#define vcorefs_err(fmt, args...)	pr_err(fmt, ##args)
#define vcorefs_warn(fmt, args...)	pr_warn(fmt, ##args)
#define vcorefs_notice(fmt, args...)	pr_notice(fmt, ##args)
#define vcorefs_info(fmt, args...)	pr_info(fmt, ##args)
#define vcorefs_debug(fmt, args...)	pr_info(fmt, ##args)	/* pr_debug show nothing */

#define for_each_request_kicker(i)	for (i = KR_SYSFS; i < NUM_KICKERS; i++)


/**************************************
 * Define and Declare
 **************************************/
struct dvfs_opp {
	u32 vcore_pdn;
	u32 vcore_nml;
	u32 ddr_khz;
	u32 axi_khz;
	u32 mm_khz;
	u32 venc_khz;
	u32 vdec_khz;
};

struct pwr_ctrl {
	/* for framework */
	u8 vcore_dvs;
	u8 ddr_dfs;
	u8 axi_dfs;
	u8 lv_autok_trig;		/* META-FT check this to enable LV AutoK */
	u8 sdio_lv_check;
	u8 sdio_lock;
	u8 test_sim_ignore;
	u32 test_sim_prot;		/* RILD pass info to avoid pausing LTE */

	/* for Stay-LV */
	u8 init_opp_perf;
	u8 do_req_kick;
	u32 kr_req_mask;
	u32 kr_oppi_map[NUM_KICKERS];

	/* for Screen-Off */
	u8 screen_off;
	u8 mm_off;
	u32 soff_opp_index;
	u32 dvfs_delay_ms;
	u32 son_dvfs_try;

	/* for Vcore control */
	u8 sdio_trans_pause;
	u8 dma_dummy_read;
	u32 curr_vcore_pdn;

	/* for Freq control */
#ifdef MMDVFS_MMCLOCK_NOTIFICATION
	u8 mm_notify;
#endif
	u32 curr_ddr_khz;
	u32 curr_axi_khz;
	u32 curr_mm_khz;
	u32 curr_venc_khz;
	u32 curr_vdec_khz;
};

/* NOTE: __nosavedata will not be restored after IPO-H boot */

static struct dvfs_opp vcorefs_opptb[NUM_OPPS] __nosavedata = {
	[OPPI_0] = {	/* performance mode */
		.vcore_pdn	= VCORE_1_P_125,
		.vcore_nml	= VCORE_1_P_125,
		.ddr_khz	= FDDR_S0_KHZ,
		.axi_khz	= FAXI_S1_KHZ,
		.mm_khz		= FMM_S1_KHZ,
		.venc_khz	= FVENC_S1_KHZ,
		.vdec_khz	= FVDEC_S1_KHZ,
	},
	[OPPI_1] = {	/* performance mode 2 */
		.vcore_pdn	= VCORE_1_P_125,
		.vcore_nml	= VCORE_1_P_125,
		.ddr_khz	= FDDR_S1_KHZ,
		.axi_khz	= FAXI_S1_KHZ,
		.mm_khz		= FMM_S1_KHZ,
		.venc_khz	= FVENC_S1_KHZ,
		.vdec_khz	= FVDEC_S1_KHZ,
	},
	[OPPI_2] = {	/* low power mode */
		.vcore_pdn	= VCORE_1_P_0,
		.vcore_nml	= VCORE_1_P_0,
		.ddr_khz	= FDDR_S2_KHZ,
		.axi_khz	= FAXI_S2_KHZ,
		.mm_khz		= FMM_S2_KHZ,
		.venc_khz	= FVENC_S2_KHZ,
		.vdec_khz	= FVDEC_S2_KHZ,
	},
	[OPPI_3] = {	/* low power mode 2 */
		.vcore_pdn	= VCORE_1_P_0,
		.vcore_nml	= VCORE_1_P_0,
		.ddr_khz	= FDDR_S3_KHZ,
		.axi_khz	= FAXI_S2_KHZ,
		.mm_khz		= FMM_S2_KHZ,
		.venc_khz	= FVENC_S2_KHZ,
		.vdec_khz	= FVDEC_S2_KHZ,
	}
};

static u32 feature_en __nosavedata = 1;

static u32 curr_opp_index __nosavedata = OPPI_PERF_2;
static u32 prev_opp_index __nosavedata = OPPI_PERF_2;
static u32 curr_vcore_nml __nosavedata = VCORE_1_P_125;

static struct pwr_ctrl vcorefs_ctrl = {
	.vcore_dvs		= 1,
	.ddr_dfs		= 1,
	.axi_dfs		= 1,
	.sdio_lv_check		= 1,
	.do_req_kick		= 0,
	.kr_req_mask		= (1U << NUM_KICKERS) - 1,
	.soff_opp_index		= OPPI_LOW_PWR_2,
	.son_dvfs_try		= 3,
	.sdio_trans_pause	= 1,
	.dma_dummy_read		= 1,
	.curr_vcore_pdn		= VCORE_1_P_125,
#ifdef MMDVFS_MMCLOCK_NOTIFICATION
	.mm_notify		= 1,
#endif
	.curr_ddr_khz		= FDDR_S1_KHZ,
	.curr_axi_khz		= FAXI_S1_KHZ,
	.curr_mm_khz		= FMM_S1_KHZ,
	.curr_venc_khz		= FVENC_S1_KHZ,
	.curr_vdec_khz		= FVDEC_S1_KHZ,
};

static struct wake_lock dvfs_wakelock;
static struct wake_lock delay_wakelock;
static struct task_struct *vcorefs_ktask;
static atomic_t kthread_nreq = ATOMIC_INIT(0);
static DEFINE_MUTEX(vcorefs_mutex);


/**************************************
 * Vcore Control Function
 **************************************/
static u32 get_vcore_pdn(void)
{
	u32 vcore_pdn = VCORE_INVALID;

	pwrap_read(PMIC_VCORE_PDN_VOSEL_ON, &vcore_pdn);
	if (vcore_pdn >= VCORE_INVALID)		/* try again */
		pwrap_read(PMIC_VCORE_PDN_VOSEL_ON, &vcore_pdn);

	return vcore_pdn;
}

static void __update_vcore_pdn(struct pwr_ctrl *pwrctrl, int steps)
{
	int loops;

	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VCORE_PDN, pwrctrl->curr_vcore_pdn);

	/* also need to update deep idle table for Vcore restore */
	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_DEEPIDLE, IDX_DI_VCORE_PDN_NORMAL, pwrctrl->curr_vcore_pdn);

	mt_cpufreq_apply_pmic_cmd(IDX_NM_VCORE_PDN);

	if (pwrctrl->dma_dummy_read) {
		loops = (DRAM_WINDOW_SHIFT_MAX + (steps - 1)) / steps;
		dma_dummy_read_for_vcorefs(loops);	/* for DQS gating window tracking */
	} else {
		udelay(DVFS_CMD_SETTLE_US);
	}
}

static void set_vcore_pdn(struct pwr_ctrl *pwrctrl, u32 vcore_pdn)
{
	int steps, pdn_step, i;

	steps = abs(vcore_pdn - pwrctrl->curr_vcore_pdn);
	pdn_step = (vcore_pdn >= pwrctrl->curr_vcore_pdn ? 1 : -1);

	for (i = 0; i < steps; i++) {
		pwrctrl->curr_vcore_pdn += pdn_step;
		__update_vcore_pdn(pwrctrl, steps);
	}

	vcorefs_crit("curr_pdn = 0x%x\n", pwrctrl->curr_vcore_pdn);

#if VCORE_SET_CHECK
	vcore_pdn = get_vcore_pdn();
	vcorefs_debug("hw_pdn = 0x%x\n", vcore_pdn);
	BUG_ON(vcore_pdn != pwrctrl->curr_vcore_pdn);
#endif
}

static int set_vcore_with_opp(struct pwr_ctrl *pwrctrl, struct dvfs_opp *opp)
{
	int sdio_ret = -1;

	pwrctrl->curr_vcore_pdn = get_vcore_pdn();

	vcorefs_crit("pdn = 0x%x, curr_pdn = 0x%x %s\n",
		     opp->vcore_pdn, pwrctrl->curr_vcore_pdn, pwrctrl->vcore_dvs ? "" : "[X]");

	if (pwrctrl->curr_vcore_pdn >= VCORE_INVALID)
		return -EBUSY;

	if (opp->vcore_pdn == pwrctrl->curr_vcore_pdn) {
		curr_vcore_nml = opp->vcore_nml;
		return 0;
	}

	if (!pwrctrl->vcore_dvs)
		return 0;

	if (pwrctrl->sdio_trans_pause)
		sdio_ret = sdio_stop_transfer();

	set_vcore_pdn(pwrctrl, opp->vcore_pdn);

	curr_vcore_nml = opp->vcore_nml;

	if (!sdio_ret)
		sdio_start_ot_transfer();

	return 0;
}


/**************************************
 * Freq Control Function
 **************************************/
static u32 get_ddr_khz(void)
{
	u32 ddr_khz;

	ddr_khz = get_dram_data_rate() * 1000;

	return ddr_khz >= FDDR_S2_KHZ ? ddr_khz : FDDR_S3_KHZ;
}

static int set_fddr_with_opp(struct pwr_ctrl *pwrctrl, struct dvfs_opp *opp)
{
	int r;
	u32 spm_flags;

	pwrctrl->curr_ddr_khz = get_ddr_khz();

	vcorefs_crit("ddr = %u, curr_ddr = %u %s\n",
		     opp->ddr_khz, pwrctrl->curr_ddr_khz, pwrctrl->ddr_dfs ? "" : "[X]");

	if (opp->ddr_khz == pwrctrl->curr_ddr_khz || !pwrctrl->ddr_dfs)
		return 0;

	if (opp->ddr_khz >= FDDR_S2_KHZ && pwrctrl->curr_ddr_khz >= FDDR_S2_KHZ) {
		r = dram_do_dfs_by_fh(opp->ddr_khz);
	} else {
		spm_flags = (opp->ddr_khz >= FDDR_S2_KHZ ? SPM_DDR_HIGH_SPEED : 0);
		r = spm_go_to_ddrdfs(spm_flags, 0);
	}
	if (r)
		return r;

	pwrctrl->curr_ddr_khz = opp->ddr_khz;

#if FDDR_SET_CHECK
	spm_flags = get_ddr_khz();
	vcorefs_debug("hw_ddr = %u\n", spm_flags);
	BUG_ON(spm_flags != pwrctrl->curr_ddr_khz);
#endif

	return 0;
}

static int set_faxi_fxxx_with_opp(struct pwr_ctrl *pwrctrl, struct dvfs_opp *opp)
{
	pwrctrl->curr_axi_khz = get_axi_khz();

	vcorefs_crit("axi = %u, curr_axi = %u %s\n",
		     opp->axi_khz, pwrctrl->curr_axi_khz, pwrctrl->axi_dfs ? "" : "[X]");

	if (opp->axi_khz == pwrctrl->curr_axi_khz || !pwrctrl->axi_dfs)
		return 0;

#ifdef MMDVFS_MMCLOCK_NOTIFICATION
	if (pwrctrl->mm_notify)
		mmdvfs_mm_clock_switch_notify(1, opp->axi_khz > FAXI_S2_KHZ ? 1 : 0);
#endif

	/* change Faxi, Fmm, Fvenc, Fvdec between high/low speed */
	clkmux_sel_for_vcorefs(opp->axi_khz > FAXI_S2_KHZ ? true : false);

	pwrctrl->curr_axi_khz = opp->axi_khz;
	pwrctrl->curr_mm_khz = opp->mm_khz;
	pwrctrl->curr_venc_khz = opp->venc_khz;
	pwrctrl->curr_vdec_khz = opp->vdec_khz;

#ifdef MMDVFS_MMCLOCK_NOTIFICATION
	if (pwrctrl->mm_notify)
		mmdvfs_mm_clock_switch_notify(0, opp->axi_khz > FAXI_S2_KHZ ? 1 : 0);
#endif

	return 0;
}


/**************************************
 * Framework Function
 **************************************/
static bool can_dvfs_with_boot_fddr(void)
{
	struct dvfs_opp *opp;

	opp = &vcorefs_opptb[OPPI_PERF_2];

	return opp->ddr_khz == FDDR_S1_KHZ || opp->ddr_khz == FDDR_S2_KHZ;
}

static bool is_in_screen_off(struct pwr_ctrl *pwrctrl)
{
	return pwrctrl->screen_off && pwrctrl->mm_off;
}

static bool is_sdio_lv_ready(struct pwr_ctrl *pwrctrl)
{
	if (!pwrctrl->sdio_lv_check)
		return true;

#ifdef CONFIG_MTK_WCN_CMB_SDIO_SLOT
	return !!autok_is_vol_done(VCORE_1_P_0_UV, CONFIG_MTK_WCN_CMB_SDIO_SLOT);
#else
	return true;
#endif
}

static bool can_dvfs_to_lowpwr_opp(struct pwr_ctrl *pwrctrl, bool in_autok, u32 index)
{
	if (index < OPPI_LOW_PWR)	/* skip performance OPP */
		return true;

	if (!in_autok && !is_sdio_lv_ready(pwrctrl)) {
		vcorefs_err("SDIO LV IS NOT READY\n");
		return false;
	}

	if (index == OPPI_LOW_PWR_2 && !is_in_screen_off(pwrctrl)) {
		vcorefs_err("NOT SUPPORT OPP_LP2 IN SCREEN-ON\n");
		return false;
	}

	return true;
}

static int do_dvfs_for_performance(struct pwr_ctrl *pwrctrl, struct dvfs_opp *opp)
{
	int r;

	/* for performance: scale UP voltage -> frequency */

	r = set_vcore_with_opp(pwrctrl, opp);
	if (r)
		return -EBUSY;

	set_faxi_fxxx_with_opp(pwrctrl, opp);

	r = set_fddr_with_opp(pwrctrl, opp);
	if (r)
		return ERR_DDR_DFS;

	return 0;
}

static int do_dvfs_for_low_power(struct pwr_ctrl *pwrctrl, struct dvfs_opp *opp)
{
	int r;

	/* for low power: scale DOWN frequency -> voltage */

	r = set_fddr_with_opp(pwrctrl, opp);
	if (r)
		return -EBUSY;

	set_faxi_fxxx_with_opp(pwrctrl, opp);

	r = set_vcore_with_opp(pwrctrl, opp);
	if (r)
		return ERR_VCORE_DVS;

	return 0;
}

static int __kick_dvfs_by_index(struct pwr_ctrl *pwrctrl, u32 index)
{
	int err;

	if (index == curr_opp_index) {
		if (curr_opp_index == prev_opp_index)
			return 0;

		/* try again since previous change is partial success */
		curr_opp_index = prev_opp_index;
	}

	/* 0 (performance) <= index <= X (low power) */
	if (index < curr_opp_index)
		err = do_dvfs_for_performance(pwrctrl, &vcorefs_opptb[index]);
	else
		err = do_dvfs_for_low_power(pwrctrl, &vcorefs_opptb[index]);

	if (err == 0) {
		prev_opp_index = index;
		curr_opp_index = index;
	} else if (err > 0) {
		prev_opp_index = curr_opp_index;
		curr_opp_index = index;
	}

	vcorefs_crit("done: curr_index = %u (%u), err = %d\n",
		     curr_opp_index, prev_opp_index, err);

	return err;
}

/*
 * return value:
 *   > 0: partial success (ERR_DDR_DFS, ERR_VCORE_DVS)
 *   = 0: full success
 *   < 0: failure (-EAGAIN, -EACCES, -EPERM, -EBUSY)
 */
static int kick_dvfs_by_index(struct pwr_ctrl *pwrctrl, enum dvfs_kicker kicker, u32 index)
{
	int r;
	bool in_autok = false;

	vcorefs_crit("%u kick: lock = %u, index = %u, curr_index = %u (%u)\n",
		     kicker, pwrctrl->sdio_lock, index, curr_opp_index, prev_opp_index);

	if (curr_opp_index == index && curr_opp_index == prev_opp_index)
		return 0;

	if (pwrctrl->sdio_lock) {
		if (kicker == KR_SDIO_AUTOK ||
		    /* allow DVFS from LP2 mode between SDIO lock and set */
		    (curr_opp_index == OPPI_LOW_PWR_2 && index < curr_opp_index))
			in_autok = true;
		else
			return -EAGAIN;
	}

	if (!can_dvfs_to_lowpwr_opp(pwrctrl, in_autok, index))
		return -EACCES;

	if (!feature_en)
		return -EPERM;

	wake_lock(&dvfs_wakelock);

	/* DVFS to/from LP2 mode need back to boot-up OPP first */
	if ((index == OPPI_LOW_PWR_2 && curr_opp_index != OPPI_LOW_PWR_2) ||
	    (curr_opp_index == OPPI_LOW_PWR_2 && curr_opp_index == prev_opp_index)) {
		r = __kick_dvfs_by_index(pwrctrl, OPPI_PERF_2);
		if (r)
			goto OUT;
	}

	r = __kick_dvfs_by_index(pwrctrl, index);

OUT:
	wake_unlock(&dvfs_wakelock);
	return r;
}


/**************************************
 * Stay-LV DVFS Function/API
 **************************************/
/*
 * return value:
 *   true : init OPP should be LOW_PWR
 *   false: init OPP should be PERF
 */
static bool is_fhd_segment(void)
{
#ifdef MMDVFS_ENABLE_DEFAULT_STEP_QUERY
	return !mmdvfs_is_default_step_need_perf();
#else
	return DISP_GetScreenWidth() * DISP_GetScreenHeight() <= SCREEN_RES_FHD;
#endif
}

static void set_init_opp_index(struct pwr_ctrl *pwrctrl)
{
	if (feature_en) {
		if (!pwrctrl->init_opp_perf && is_fhd_segment())
			pwrctrl->kr_oppi_map[KR_LATE_INIT] = OPPI_LOW_PWR;
		else
			pwrctrl->kr_oppi_map[KR_LATE_INIT] = OPPI_PERF;
	} else {
		pwrctrl->kr_oppi_map[KR_LATE_INIT] = curr_opp_index;
	}
}

static int late_init_to_lowpwr_opp(void)
{
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	mutex_lock(&vcorefs_mutex);
	vcorefs_crit("feature_en = %u, is_fhd = %u\n", feature_en, is_fhd_segment());

	set_init_opp_index(pwrctrl);
	kick_dvfs_by_index(pwrctrl, KR_LATE_INIT, pwrctrl->kr_oppi_map[KR_LATE_INIT]);

	pwrctrl->do_req_kick = 1;
	pwrctrl->kr_req_mask = 0;	/* start to accept request */
	mutex_unlock(&vcorefs_mutex);

	return 0;
}

static bool need_delay_dvfs_to_lp2(struct pwr_ctrl *pwrctrl)
{
	u32 delay_ms = spm_ensure_lte_pause_interval();

	vcorefs_crit("delay_ms = %u\n", delay_ms);

	if (delay_ms <= DVFS_DELAY_IGNORE_MS)
		return false;

	pwrctrl->dvfs_delay_ms = delay_ms;
	atomic_inc(&kthread_nreq);
	smp_mb();	/* for VcoreFS kthread */
	wake_up_process(vcorefs_ktask);

	return true;
}

static int __request_dvfs_opp(struct pwr_ctrl *pwrctrl, enum dvfs_kicker kicker, int index)
{
	int i, r = 0;
	u32 old_oppi_map, oppi_map = 0;

	old_oppi_map = pwrctrl->kr_oppi_map[kicker];

	vcorefs_crit("%u request: mask = 0x%x, index = %d (0x%x)\n",
		     kicker, pwrctrl->kr_req_mask, index, old_oppi_map);

	if (index >= 0 && (pwrctrl->kr_req_mask & (1U << kicker)))
		return 0;

	pwrctrl->kr_oppi_map[kicker] = (index >= 0 ? 1U << index : 0);

	for_each_request_kicker(i)
		oppi_map |= pwrctrl->kr_oppi_map[i];

	for (i = 0; i < NUM_OPPS; i++) {	/* performance first */
		if (oppi_map & (1U << i))
			break;
	}
	if (i >= NUM_OPPS)	/* no request, use init OPP index */
		i = pwrctrl->kr_oppi_map[KR_LATE_INIT];

	if (pwrctrl->do_req_kick) {
		if (i == OPPI_LOW_PWR_2 && need_delay_dvfs_to_lp2(pwrctrl))
			return 0;	/* vcorefs_kthread will handle LP2 DVFS */

		r = kick_dvfs_by_index(pwrctrl, kicker, i);
		if (r < 0 && index >= 0)
			pwrctrl->kr_oppi_map[kicker] = old_oppi_map;
	}

	return r;
}

int vcorefs_request_dvfs_opp(enum dvfs_kicker kicker, int index)
{
	int r;
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	if (kicker < KR_MM_SCEN || kicker >= NUM_KICKERS || index >= NUM_OPPS)
		return -EINVAL;

	mutex_lock(&vcorefs_mutex);
	r = __request_dvfs_opp(pwrctrl, kicker, index);
	mutex_unlock(&vcorefs_mutex);

	return r;
}

#ifdef MMDVFS_MMCLOCK_NOTIFICATION
int vcorefs_request_opp_no_mm_notify(enum dvfs_kicker kicker, int index)
{
	int r;
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	if (kicker < KR_MM_SCEN || kicker >= NUM_KICKERS || index >= NUM_OPPS)
		return -EINVAL;

	mutex_lock(&vcorefs_mutex);
	pwrctrl->mm_notify = 0;
	r = __request_dvfs_opp(pwrctrl, kicker, index);
	pwrctrl->mm_notify = 1;
	mutex_unlock(&vcorefs_mutex);

	return r;
}
#endif


/**************************************
 * SDIO AutoK related API
 **************************************/
int vcorefs_sdio_lock_dvfs(bool in_ot)
{
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	if (in_ot) {	/* avoid OT thread sleeping in vcorefs_mutex */
		vcorefs_crit("sdio lock: in online-tuning\n");
		return 0;
	}

	mutex_lock(&vcorefs_mutex);
	vcorefs_crit("sdio lock: lock = %u, curr_index = %u\n",
		     pwrctrl->sdio_lock, curr_opp_index);

	if (!pwrctrl->sdio_lock) {
		pwrctrl->sdio_lock = 1;
		pwrctrl->kr_oppi_map[KR_SDIO_AUTOK] = curr_opp_index;
	}
	mutex_unlock(&vcorefs_mutex);

	return 0;
}

u32 vcorefs_sdio_get_vcore_nml(void)
{
	return vcore_pmic_to_uv(curr_vcore_nml);
}

int vcorefs_sdio_set_vcore_nml(u32 vcore_uv)
{
	int i, r = 0;
	u32 vcore_nml = vcore_uv_to_pmic(vcore_uv);
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	mutex_lock(&vcorefs_mutex);
	vcorefs_crit("sdio set: lock = %u, nml = 0x%x, curr_nml = 0x%x\n",
		     pwrctrl->sdio_lock, vcore_nml, curr_vcore_nml);

	if (!pwrctrl->sdio_lock) {
		mutex_unlock(&vcorefs_mutex);
		return -EPERM;
	}

	for (i = 0; i < NUM_OPPS; i++) {
		if (vcore_nml == vcorefs_opptb[i].vcore_nml)
			break;
	}
	if (i >= NUM_OPPS) {
		mutex_unlock(&vcorefs_mutex);
		return -EINVAL;
	}

	if (vcore_nml != curr_vcore_nml)
		r = kick_dvfs_by_index(pwrctrl, KR_SDIO_AUTOK, i);
	mutex_unlock(&vcorefs_mutex);

	return r;
}

int vcorefs_sdio_unlock_dvfs(bool in_ot)
{
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	if (in_ot) {	/* avoid OT thread sleeping in vcorefs_mutex */
		vcorefs_crit("sdio unlock: in online-tuning\n");
		return 0;
	}

	mutex_lock(&vcorefs_mutex);
	vcorefs_crit("sdio unlock: lock = %u\n", pwrctrl->sdio_lock);

	if (pwrctrl->sdio_lock) {
		kick_dvfs_by_index(pwrctrl, KR_SDIO_AUTOK, pwrctrl->kr_oppi_map[KR_SDIO_AUTOK]);
		pwrctrl->sdio_lock = 0;
	}
	mutex_unlock(&vcorefs_mutex);

	return 0;
}

/*
 * return value:
 *   true : do LV+HV AutoK
 *   false: do HV AutoK
 */
bool vcorefs_sdio_need_multi_autok(void)
{
	return !!feature_en;
}


/**************************************
 * Screen-Off DVFS Function/API
 **************************************/
static bool enable_test_sim_prot(struct pwr_ctrl *pwrctrl)
{
	return !pwrctrl->test_sim_ignore && pwrctrl->test_sim_prot;
}

static void kick_dvfs_after_screen_off(struct pwr_ctrl *pwrctrl)
{
	u32 index, orig_oppi_map;

	index = (pwrctrl->dvfs_delay_ms ? OPPI_LOW_PWR_2 : pwrctrl->soff_opp_index);

	vcorefs_crit("screen_off = %u, mm_off = %u, index = %u (%u)\n",
		     pwrctrl->screen_off, pwrctrl->mm_off, index, pwrctrl->dvfs_delay_ms);

	if (!is_in_screen_off(pwrctrl))
		return;

	if (index == OPPI_LOW_PWR_2 && enable_test_sim_prot(pwrctrl))
		index = OPPI_LOW_PWR;

	orig_oppi_map = pwrctrl->kr_oppi_map[KR_SCREEN_OFF];

	__request_dvfs_opp(pwrctrl, KR_SCREEN_OFF, index);

	if (pwrctrl->dvfs_delay_ms) {	/* delayed LP2 DVFS is done, restore kr_oppi_map */
		pwrctrl->kr_oppi_map[KR_SCREEN_OFF] = orig_oppi_map;
		pwrctrl->dvfs_delay_ms = 0;
	}
}

static void kick_dvfs_before_screen_on(struct pwr_ctrl *pwrctrl)
{
	int i, r;

	vcorefs_crit("screen_off - %u, mm_off - %u\n",
		     pwrctrl->screen_off, pwrctrl->mm_off);

	if (!is_in_screen_off(pwrctrl))
		return;

	for_each_request_kicker(i)	/* avoid using LP2 mode in screen-on */
		pwrctrl->kr_oppi_map[i] &= ~(1U << OPPI_LOW_PWR_2);

	for (i = 1; i <= pwrctrl->son_dvfs_try; i++) {
		r = __request_dvfs_opp(pwrctrl, KR_SCREEN_OFF, OPPI_UNREQ);
		if (r == 0 || r == -EAGAIN /* SDIO lock */ || r == -EPERM /* DVFS disable */)
			return;
	}

	BUG();
}

static int vcorefs_kthread(void *data)
{
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);

		if (kthread_should_stop())
			break;

		if (atomic_read(&kthread_nreq) <= 0) {
			schedule();
			continue;
		}

		wake_lock(&delay_wakelock);

		if (pwrctrl->dvfs_delay_ms)
			msleep(pwrctrl->dvfs_delay_ms);

		mutex_lock(&vcorefs_mutex);
		kick_dvfs_after_screen_off(pwrctrl);
		atomic_dec(&kthread_nreq);
		mutex_unlock(&vcorefs_mutex);

		wake_unlock(&delay_wakelock);
	}

	__set_current_state(TASK_RUNNING);
	return 0;
}

/* this will be called at MM off */
void vcorefs_clkmgr_notify_mm_off(void)
{
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	if (IS_ERR_OR_NULL(vcorefs_ktask))
		return;

	pwrctrl->mm_off = 1;
	atomic_inc(&kthread_nreq);
	smp_mb();	/* for VcoreFS kthread */
	wake_up_process(vcorefs_ktask);
}

/* this will be called at CLKMGR late resume */
void vcorefs_clkmgr_notify_mm_on(void)
{
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	pwrctrl->mm_off = 0;
	smp_mb();	/* for VcoreFS kthread */
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void vcorefs_early_suspend(struct early_suspend *h)
{
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	mutex_lock(&vcorefs_mutex);
	pwrctrl->screen_off = 1;
	atomic_inc(&kthread_nreq);
	mutex_unlock(&vcorefs_mutex);

	wake_up_process(vcorefs_ktask);
}

static void vcorefs_late_resume(struct early_suspend *h)
{
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	mutex_lock(&vcorefs_mutex);
	kick_dvfs_before_screen_on(pwrctrl);
	pwrctrl->screen_off = 0;
	mutex_unlock(&vcorefs_mutex);
}

static struct early_suspend vcorefs_earlysuspend_desc = {
	.level		= 9999,		/* should be last one */
	.suspend	= vcorefs_early_suspend,
	.resume		= vcorefs_late_resume,
};
#endif


/**************************************
 * pwr_ctrl_xxx Function
 **************************************/
static ssize_t pwr_ctrl_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	int i;
	char *p = buf;
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	p += sprintf(p, "feature_en = %u\n"      , feature_en);
	p += sprintf(p, "vcore_dvs = %u\n"       , pwrctrl->vcore_dvs);
	p += sprintf(p, "ddr_dfs = %u\n"         , pwrctrl->ddr_dfs);
	p += sprintf(p, "axi_dfs = %u\n"         , pwrctrl->axi_dfs);
	p += sprintf(p, "lv_autok_trig = %u\n"   , pwrctrl->lv_autok_trig);
	p += sprintf(p, "sdio_lv_check = %u\n"   , pwrctrl->sdio_lv_check);
	p += sprintf(p, "sdio_lock = %u\n"       , pwrctrl->sdio_lock);
	p += sprintf(p, "test_sim_ignore = %u\n" , pwrctrl->test_sim_ignore);
	p += sprintf(p, "test_sim_prot = 0x%x\n" , pwrctrl->test_sim_prot);

#ifdef MMDVFS_ENABLE_DEFAULT_STEP_QUERY
	p += sprintf(p, "init_opp_perf = %u [%u]\n", pwrctrl->init_opp_perf, is_fhd_segment());
#else
	p += sprintf(p, "init_opp_perf = %u (%u)\n", pwrctrl->init_opp_perf, is_fhd_segment());
#endif
	p += sprintf(p, "do_req_kick = %u\n"     , pwrctrl->do_req_kick);
	p += sprintf(p, "kr_req_mask = 0x%x\n"   , pwrctrl->kr_req_mask);
	for (i = 0; i < NUM_KICKERS; i++)
		p += sprintf(p, "kr_oppi_map[%d] = 0x%x\n", i, pwrctrl->kr_oppi_map[i]);

	p += sprintf(p, "screen_off = %u\n"      , pwrctrl->screen_off);
	p += sprintf(p, "mm_off = %u\n"          , pwrctrl->mm_off);
	p += sprintf(p, "soff_opp_index = %u\n"  , pwrctrl->soff_opp_index);
	p += sprintf(p, "dvfs_delay_ms = %u\n"   , pwrctrl->dvfs_delay_ms);
	p += sprintf(p, "son_dvfs_try = %u\n"    , pwrctrl->son_dvfs_try);

	p += sprintf(p, "curr_opp_index = %u\n"  , curr_opp_index);
	p += sprintf(p, "prev_opp_index = %u\n"  , prev_opp_index);

	p += sprintf(p, "sdio_trans_pause = %u\n", pwrctrl->sdio_trans_pause);
	p += sprintf(p, "dma_dummy_read = %u\n"  , pwrctrl->dma_dummy_read);
	p += sprintf(p, "curr_vcore_pdn = 0x%x\n", pwrctrl->curr_vcore_pdn);
	p += sprintf(p, "curr_vcore_nml = 0x%x\n", curr_vcore_nml);

#ifdef MMDVFS_MMCLOCK_NOTIFICATION
	p += sprintf(p, "mm_notify = %u\n"       , pwrctrl->mm_notify);
#endif
	p += sprintf(p, "curr_ddr_khz = %u\n"    , pwrctrl->curr_ddr_khz);
	p += sprintf(p, "curr_axi_khz = %u\n"    , pwrctrl->curr_axi_khz);
	p += sprintf(p, "curr_mm_khz = %u\n"     , pwrctrl->curr_mm_khz);
	p += sprintf(p, "curr_venc_khz = %u\n"   , pwrctrl->curr_venc_khz);
	p += sprintf(p, "curr_vdec_khz = %u\n"   , pwrctrl->curr_vdec_khz);

	BUG_ON(p - buf >= PAGE_SIZE);
	return p - buf;
}

static ssize_t pwr_ctrl_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	int r;
	u32 val;
	char cmd[32];
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	if (sscanf(buf, "%31s %x", cmd, &val) != 2)
		return -EPERM;

	vcorefs_crit("pwr_ctrl: cmd = %s, val = 0x%x\n", cmd, val);

	if (!strcmp(cmd, "feature_en")) {
		mutex_lock(&vcorefs_mutex);
		if (val && can_dvfs_with_boot_fddr() && !feature_en) {
			feature_en = 1;
			set_init_opp_index(pwrctrl);
		} else if (!val && feature_en) {
			r = kick_dvfs_by_index(pwrctrl, KR_SYSFS, OPPI_PERF);
			BUG_ON(r);
			feature_en = 0;
			set_init_opp_index(pwrctrl);
		}
		mutex_unlock(&vcorefs_mutex);
	} else if (!strcmp(cmd, "vcore_dvs")) {
		pwrctrl->vcore_dvs = val;
	} else if (!strcmp(cmd, "ddr_dfs")) {
		pwrctrl->ddr_dfs = val;
	} else if (!strcmp(cmd, "axi_dfs")) {
		pwrctrl->axi_dfs = val;
	} else if (!strcmp(cmd, "sdio_lv_check")) {
		pwrctrl->sdio_lv_check = val;
	} else if (!strcmp(cmd, "test_sim_ignore")) {
		pwrctrl->test_sim_ignore = val;
	} else if (!strcmp(cmd, "test_sim_prot")) {
		pwrctrl->test_sim_prot = val;
	} else if (!strcmp(cmd, "init_opp_perf")) {
		pwrctrl->init_opp_perf = val;
	} else if (!strcmp(cmd, "do_req_kick")) {
		pwrctrl->do_req_kick = val;
	} else if (!strcmp(cmd, "kr_req_mask")) {
		pwrctrl->kr_req_mask = val;
	} else if (!strcmp(cmd, "soff_opp_index") && val < NUM_OPPS) {
		pwrctrl->soff_opp_index = val;
	} else if (!strcmp(cmd, "son_dvfs_try")) {
		pwrctrl->son_dvfs_try = val;
	} else if (!strcmp(cmd, "sdio_trans_pause")) {
		pwrctrl->sdio_trans_pause = val;
	} else if (!strcmp(cmd, "dma_dummy_read")) {
		pwrctrl->dma_dummy_read = val;
	} else {
		return -EINVAL;
	}

	return count;
}


/**************************************
 * opp_table_xxx Function
 **************************************/
static ssize_t opp_table_show(struct kobject *kobj, struct kobj_attribute *attr,
			      char *buf)
{
	int i;
	char *p = buf;
	struct dvfs_opp *opp;

	for (i = 0; i < NUM_OPPS; i++) {
		opp = &vcorefs_opptb[i];

		p += sprintf(p, "[OPP %d]\n", i);
		p += sprintf(p, "vcore_pdn = 0x%x\n", opp->vcore_pdn);
		p += sprintf(p, "vcore_nml = 0x%x\n", opp->vcore_nml);
		p += sprintf(p, "ddr_khz = %u\n"    , opp->ddr_khz);
		p += sprintf(p, "axi_khz = %u\n"    , opp->axi_khz);
		p += sprintf(p, "mm_khz = %u\n"     , opp->mm_khz);
		p += sprintf(p, "venc_khz = %u\n"   , opp->venc_khz);
		p += sprintf(p, "vdec_khz = %u\n"   , opp->vdec_khz);
	}

	BUG_ON(p - buf >= PAGE_SIZE);
	return p - buf;
}

static ssize_t opp_table_store(struct kobject *kobj, struct kobj_attribute *attr,
			       const char *buf, size_t count)
{
	u32 val1, val2;
	char cmd[32];
	struct dvfs_opp *opp;

	if (sscanf(buf, "%31s %u %u", cmd, &val1, &val2) != 3)
		return -EPERM;

	vcorefs_debug("opp_table: cmd = %s, val1 = %u, val2 = %u\n", cmd, val1, val2);

	if (val1 >= NUM_OPPS)
		return -EINVAL;

	opp = &vcorefs_opptb[val1];

	if (!strcmp(cmd, "vcore_pdn") && val2 < VCORE_INVALID)
		opp->vcore_pdn = val2;
	else if (!strcmp(cmd, "vcore_nml") && val2 < VCORE_INVALID)
		opp->vcore_nml = val2;
	else if (!strcmp(cmd, "ddr_khz") && val2 > FREQ_INVALID)
		opp->ddr_khz = val2;
	else if (!strcmp(cmd, "axi_khz") && val2 > FREQ_INVALID)
		opp->axi_khz = val2;
	else if (!strcmp(cmd, "mm_khz") && val2 > FREQ_INVALID)
		opp->mm_khz = val2;
	else if (!strcmp(cmd, "venc_khz") && val2 > FREQ_INVALID)
		opp->venc_khz = val2;
	else if (!strcmp(cmd, "vdec_khz") && val2 > FREQ_INVALID)
		opp->vdec_khz = val2;
	else
		return -EINVAL;

	return count;
}


/**************************************
 * vcore_debug_xxx Function
 **************************************/
static ssize_t vcore_debug_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	u32 vcore_pdn;
	char *p = buf;

	vcore_pdn = get_vcore_pdn();

	p += sprintf(p, "VCORE_PDN = %u (0x%x)\n", vcore_pmic_to_uv(vcore_pdn), vcore_pdn);
	p += sprintf(p, "FDDR = %u (%u)\n", get_ddr_khz(), get_dram_data_rate());
	p += sprintf(p, "FAXI = %u\n", get_axi_khz());

	BUG_ON(p - buf >= PAGE_SIZE);
	return p - buf;
}

static ssize_t vcore_debug_store(struct kobject *kobj, struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	int val, r;
	char cmd[32];
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	if (sscanf(buf, "%31s %d", cmd, &val) != 2)
		return -EPERM;

	vcorefs_debug("vcore_debug: cmd = %s, val = %d\n", cmd, val);

	if (!strcmp(cmd, "opp_vcore_set") && val < NUM_OPPS) {
		mutex_lock(&vcorefs_mutex);
		if ((val != OPPI_LOW_PWR_2 || is_in_screen_off(pwrctrl)) && feature_en) {
			r = __request_dvfs_opp(pwrctrl, KR_SYSFS, val);
			BUG_ON(r);
		}
		mutex_unlock(&vcorefs_mutex);
	} else if (!strcmp(cmd, "opp_vcore_setX") && (u32)val < NUM_OPPS) {
		mutex_lock(&vcorefs_mutex);
		kick_dvfs_by_index(pwrctrl, KR_SYSFS, val);
		mutex_unlock(&vcorefs_mutex);
	} else {
		return -EINVAL;
	}

	return count;
}


/**************************************
 * Init Function
 **************************************/
DEFINE_ATTR_RW(pwr_ctrl);
DEFINE_ATTR_RW(opp_table);
DEFINE_ATTR_RW(vcore_debug);

static struct attribute *vcorefs_attrs[] = {
	__ATTR_OF(pwr_ctrl),
	__ATTR_OF(opp_table),
	__ATTR_OF(vcore_debug),
	NULL,
};

static struct attribute_group vcorefs_attr_group = {
	.name	= "vcorefs",
	.attrs	= vcorefs_attrs,
};

static int create_vcorefs_kthread(void)
{
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	vcorefs_ktask = kthread_create(vcorefs_kthread, NULL, "vcorefs");
	if (IS_ERR(vcorefs_ktask))
		return PTR_ERR(vcorefs_ktask);

	sched_setscheduler_nocheck(vcorefs_ktask, SCHED_FIFO, &param);
	get_task_struct(vcorefs_ktask);
	wake_up_process(vcorefs_ktask);

	return 0;
}

static int init_vcorefs_pwrctrl(void)
{
	int i;
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;
	struct dvfs_opp *opp;

	mutex_lock(&vcorefs_mutex);
	pwrctrl->curr_vcore_pdn = get_vcore_pdn();
	BUG_ON(pwrctrl->curr_vcore_pdn >= VCORE_INVALID);

	pwrctrl->curr_ddr_khz = get_ddr_khz();
	pwrctrl->curr_axi_khz = get_axi_khz();

	for (i = 0; i < NUM_OPPS; i++) {
		opp = &vcorefs_opptb[i];

		if (i < OPPI_LOW_PWR) {		/* update performance OPP */
			opp->vcore_pdn = pwrctrl->curr_vcore_pdn;
			opp->axi_khz = pwrctrl->curr_axi_khz;
			if (pwrctrl->curr_ddr_khz != FDDR_S1_KHZ)
				opp->ddr_khz = pwrctrl->curr_ddr_khz;
		} else {	/* update low power OPP */
			opp->vcore_pdn = get_vcore_ptp_volt(VCORE_1_P_0_UV);
		}

		vcorefs_crit("OPP %d: vcore_pdn = 0x%x, ddr_khz = %u, axi_khz = %u\n",
			     i, opp->vcore_pdn, opp->ddr_khz, opp->axi_khz);
	}

	if (pwrctrl->curr_ddr_khz != FDDR_S1_KHZ) {	/* performance OPP are the same */
		curr_opp_index = 0;
		prev_opp_index = 0;
	}
#if NORM_SEG_FAVOR_PERF
	else {	/* curr_ddr = 1466 */
		pwrctrl->init_opp_perf = 1;
		vcorefs_crit("NORM_SEG_FAVOR_PERF = 1\n");
	}
#endif

	if (!can_dvfs_with_boot_fddr()) {
		feature_en = 0;
		vcorefs_err("DISABLE DVFS DUE TO NOT SUPPORT FDDR\n");
	}
	mutex_unlock(&vcorefs_mutex);

	return 0;
}

static int __init vcorefs_module_init(void)
{
	int r;

	r = init_vcorefs_pwrctrl();
	if (r) {
		vcorefs_err("FAILED TO INIT PWRCTRL (%d)\n", r);
		return r;
	}

	wake_lock_init(&dvfs_wakelock, WAKE_LOCK_SUSPEND, "vcorefs_dvfs");
	wake_lock_init(&delay_wakelock, WAKE_LOCK_SUSPEND, "vcorefs_delay");

	r = create_vcorefs_kthread();	/* for Screen-Off DVFS */
	if (r) {
		vcorefs_err("FAILED TO CREATE KTHREAD (%d)\n", r);
		return r;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&vcorefs_earlysuspend_desc);	/* for Screen-Off DVFS */
#endif

	r = sysfs_create_group(power_kobj, &vcorefs_attr_group);
	if (r)
		vcorefs_err("FAILED TO CREATE /sys/power/vcorefs (%d)\n", r);

	return r;
}

module_init(vcorefs_module_init);
late_initcall_sync(late_init_to_lowpwr_opp);

MODULE_DESCRIPTION("Vcore DVFS Driver v0.9");
