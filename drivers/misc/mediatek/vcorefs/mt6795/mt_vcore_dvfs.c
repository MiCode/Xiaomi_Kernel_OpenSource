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
#include <mach/mt_pmic_wrap.h>
#include <mach/mt_cpufreq.h>
#include <mach/mt_dramc.h>
#include <mach/mt_spm.h>
#include <mach/mt_clkmgr.h>
#include <mach/board.h>
#include <mach/mtk_wcn_cmb_stub.h>
#include <mt_sd_func.h>

extern u32 get_devinfo_with_index(u32 index);

/**************************************
 * Config and Parameter
 **************************************/
#define VCORE_SET_CHECK		0
#define FDDR_SET_CHECK		0

#define FDDR_S0_KHZ		1792000
#define FDDR_S1_KHZ		1600000
#define FDDR_S2_KHZ		1333000

#define FAXI_S1_KHZ		273000		/* MUX_1 */
#define FAXI_S2_KHZ		218400		/* MUX_2 */

#define FMM_S1_KHZ		400000		/* FH */
#define FMM_S2_KHZ		317000		/* FH */

#define FVENC_S1_KHZ		494000		/* FH */
#define FVENC_S2_KHZ		384000		/* FH */

#define FVDEC_S1_KHZ		494000		/* FH */
#define FVDEC_S2_KHZ		384000		/* FH */

#define DVFS_CMD_SETTLE_US	5	/* (PWRAP->1.5us->PMIC->10mv/1us) * 2 */
#define DRAM_WINDOW_SHIFT_MAX	128

#define OPPI_SCREEN_OFF_LP	1
#define OPPI_LATE_INIT_LP	1


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


/**************************************
 * Define and Declare
 **************************************/
struct dvfs_opp {
	u32 vcore_ao;
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
	u8 feature_en;
	u8 sonoff_dvfs_only;
	u8 stay_lv_en;
	u8 vcore_dvs;
	u8 ddr_dfs;
	u8 axi_dfs;
	u8 screen_off;
	u8 mm_off;
	u8 sdio_lock;
	u8 sdio_lv_check;
	u8 lv_autok_trig;		/* META-FT check this to enable LV AutoK */
	u8 lv_autok_abort;
	u8 test_sim_ignore;
	u32 test_sim_prot;		/* RILD pass info to avoid pausing LTE */
	u32 son_opp_index;
	u32 son_dvfs_try;

	/* for OPP table */
	struct dvfs_opp *opp_table;
	u32 num_opps;

	/* for Vcore control */
	u32 curr_vcore_ao;
	u32 curr_vcore_pdn;
	u8 sdio_trans_pause;
	u8 dma_dummy_read;

	/* for Freq control */
	u32 curr_ddr_khz;
	u32 curr_axi_khz;
	u32 curr_mm_khz;
	u32 curr_venc_khz;
	u32 curr_vdec_khz;
};

/* NOTE: __nosavedata will not be restored after IPO-H boot */

static struct dvfs_opp opp_table1[] __nosavedata = {
	{	/* OPP 0: performance mode */
		.vcore_ao	= VCORE_1_P_125,
		.vcore_pdn	= VCORE_1_P_125,
		.vcore_nml	= VCORE_1_P_125,
		.ddr_khz	= FDDR_S1_KHZ,
		.axi_khz	= FAXI_S1_KHZ,
		.mm_khz		= FMM_S1_KHZ,
		.venc_khz	= FVENC_S1_KHZ,
		.vdec_khz	= FVDEC_S1_KHZ,
	},
	{	/* OPP 1: low power mode */
		.vcore_ao	= VCORE_1_P_0,
		.vcore_pdn	= VCORE_1_P_0,
		.vcore_nml	= VCORE_1_P_0,
		.ddr_khz	= FDDR_S2_KHZ,
		.axi_khz	= FAXI_S2_KHZ,
		.mm_khz		= FMM_S2_KHZ,
		.venc_khz	= FVENC_S2_KHZ,
		.vdec_khz	= FVDEC_S2_KHZ,
	}
};

static u32 curr_opp_index __nosavedata;
static u32 prev_opp_index __nosavedata;
static u32 curr_vcore_nml __nosavedata = VCORE_1_P_125;

static struct pwr_ctrl vcorefs_ctrl = {
	.feature_en		= 1,
	.sonoff_dvfs_only	= 1,
	.vcore_dvs		= 1,
	.ddr_dfs		= 1,
	.axi_dfs		= 1,
	.sdio_lv_check		= 1,
	.lv_autok_trig		= 1,
	.lv_autok_abort		= 1,
	.son_opp_index		= 0,
	.son_dvfs_try		= 3,
	.opp_table		= opp_table1,
	.num_opps		= ARRAY_SIZE(opp_table1),
	.curr_vcore_ao		= VCORE_1_P_125,
	.curr_vcore_pdn		= VCORE_1_P_125,
	.sdio_trans_pause	= 1,
	.dma_dummy_read		= 1,
	.curr_ddr_khz		= FDDR_S1_KHZ,
	.curr_axi_khz		= FAXI_S1_KHZ,
	.curr_mm_khz		= FMM_S1_KHZ,
	.curr_venc_khz		= FVENC_S1_KHZ,
	.curr_vdec_khz		= FVDEC_S1_KHZ,
};

static struct wake_lock vcorefs_wakelock;
static struct task_struct *vcorefs_ktask;
static atomic_t kthread_nreq = ATOMIC_INIT(0);
static DEFINE_MUTEX(vcorefs_mutex);


/**************************************
 * Vcore Control Function
 **************************************/
static u32 get_vcore_ao(void)
{
	u32 vcore_ao = VCORE_INVALID;

	pwrap_read(PMIC_VCORE_AO_VOSEL_ON, &vcore_ao);
	if (vcore_ao >= VCORE_INVALID)		/* try again */
		pwrap_read(PMIC_VCORE_AO_VOSEL_ON, &vcore_ao);

	return vcore_ao;
}

static u32 get_vcore_pdn(void)
{
	u32 vcore_pdn = VCORE_INVALID;

	pwrap_read(PMIC_VCORE_PDN_VOSEL_ON, &vcore_pdn);
	if (vcore_pdn >= VCORE_INVALID)		/* try again */
		pwrap_read(PMIC_VCORE_PDN_VOSEL_ON, &vcore_pdn);

	return vcore_pdn;
}

static void __update_vcore_ao_pdn(struct pwr_ctrl *pwrctrl, int steps)
{
	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VCORE_AO, pwrctrl->curr_vcore_ao);
	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VCORE_PDN, pwrctrl->curr_vcore_pdn);

	/* also need to update deep idle table for Vcore restore */
	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_DEEPIDLE, IDX_DI_VCORE_AO_NORMAL, pwrctrl->curr_vcore_ao);
	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_DEEPIDLE, IDX_DI_VCORE_PDN_NORMAL, pwrctrl->curr_vcore_pdn);

	mt_cpufreq_apply_pmic_cmd(IDX_NM_VCORE_PDN);
	mt_cpufreq_apply_pmic_cmd(IDX_NM_VCORE_AO);

	if (pwrctrl->dma_dummy_read) {
		int loops = (DRAM_WINDOW_SHIFT_MAX + (steps - 1)) / steps;
		dma_dummy_read_for_vcorefs(loops);	/* for DQS gating window tracking */
	} else {
		udelay(DVFS_CMD_SETTLE_US);
	}
}

static void set_vcore_ao_pdn(struct pwr_ctrl *pwrctrl, u32 vcore_ao, u32 vcore_pdn)
{
	int ao_step, pdn_step, steps, i;
	int ao_step_uv, pdn_step_uv;
	int ao_curr_uv, pdn_curr_uv, ao_uv, pdn_uv;

	ao_step = abs(vcore_ao - pwrctrl->curr_vcore_ao);
	pdn_step = abs(vcore_pdn - pwrctrl->curr_vcore_pdn);
	steps = (ao_step >= pdn_step ? ao_step : pdn_step);

	ao_step_uv = ao_step * VCORE_STEP_UV / steps;
	pdn_step_uv = pdn_step * VCORE_STEP_UV / steps;

	ao_step = (vcore_ao >= pwrctrl->curr_vcore_ao ? 1 : -1);
	pdn_step = (vcore_pdn >= pwrctrl->curr_vcore_pdn ? 1 : -1);

	ao_curr_uv = vcore_pmic_to_uv(pwrctrl->curr_vcore_ao) * ao_step;
	pdn_curr_uv = vcore_pmic_to_uv(pwrctrl->curr_vcore_pdn) * pdn_step;

	ao_uv = ao_curr_uv;
	pdn_uv = pdn_curr_uv;

	for (i = 0; i < steps; i++) {
		ao_uv += ao_step_uv;
		if (ao_uv > ao_curr_uv) {
			pwrctrl->curr_vcore_ao += ao_step;
			ao_curr_uv += VCORE_STEP_UV;
		}

		pdn_uv += pdn_step_uv;
		if (pdn_uv > pdn_curr_uv) {
			pwrctrl->curr_vcore_pdn += pdn_step;
			pdn_curr_uv += VCORE_STEP_UV;
		}

		__update_vcore_ao_pdn(pwrctrl, steps);
	}

	vcorefs_crit("curr_ao = 0x%x, curr_pdn = 0x%x\n",
		     pwrctrl->curr_vcore_ao, pwrctrl->curr_vcore_pdn);

	BUG_ON(pwrctrl->curr_vcore_ao != vcore_ao ||
	       pwrctrl->curr_vcore_pdn != vcore_pdn);

#if VCORE_SET_CHECK
	vcore_ao = get_vcore_ao();
	vcore_pdn = get_vcore_pdn();
	vcorefs_debug("hw_ao = 0x%x, hw_pdn = 0x%x\n", vcore_ao, vcore_pdn);
	BUG_ON(vcore_ao != pwrctrl->curr_vcore_ao ||
	       vcore_pdn != pwrctrl->curr_vcore_pdn);
#endif
}

static int set_vcore_with_opp(struct pwr_ctrl *pwrctrl, struct dvfs_opp *opp)
{
	int sdio_ret = -1;

	pwrctrl->curr_vcore_ao = get_vcore_ao();
	pwrctrl->curr_vcore_pdn = get_vcore_pdn();

	vcorefs_crit("ao = 0x%x, curr_ao = 0x%x, pdn = 0x%x, curr_pdn = 0x%x %s\n",
		     opp->vcore_ao, pwrctrl->curr_vcore_ao,
		     opp->vcore_pdn, pwrctrl->curr_vcore_pdn, pwrctrl->vcore_dvs ? "" : "[X]");

	if (pwrctrl->curr_vcore_ao >= VCORE_INVALID ||
	    pwrctrl->curr_vcore_pdn >= VCORE_INVALID)
		return -EBUSY;

	if (opp->vcore_ao == pwrctrl->curr_vcore_ao &&
	    opp->vcore_pdn == pwrctrl->curr_vcore_pdn) {
		curr_vcore_nml = opp->vcore_nml;
		return 0;
	}

	if (!pwrctrl->vcore_dvs)
		return 0;

	if (pwrctrl->sdio_trans_pause)
		sdio_ret = sdio_stop_transfer();

	set_vcore_ao_pdn(pwrctrl, opp->vcore_ao, opp->vcore_pdn);

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
	return get_dram_data_rate() * 1000;
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

	spm_flags = (pwrctrl->screen_off ? SPM_SCREEN_OFF : 0);
	spm_flags |= (opp->ddr_khz > FDDR_S2_KHZ ? SPM_DDR_HIGH_SPEED : 0);
	r = spm_go_to_ddrdfs(spm_flags, 0);
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

	/* change Faxi, Fmm, Fvenc, Fvdec between high/low speed */
	clkmux_sel_for_vcorefs(opp->axi_khz > FAXI_S2_KHZ ? true : false);

	pwrctrl->curr_axi_khz = opp->axi_khz;
	pwrctrl->curr_mm_khz = opp->mm_khz;
	pwrctrl->curr_venc_khz = opp->venc_khz;
	pwrctrl->curr_vdec_khz = opp->vdec_khz;

	return 0;
}


/**************************************
 * Framework Function/API
 **************************************/
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

static void trigger_sdio_lv_autok(struct pwr_ctrl *pwrctrl)
{
	if (is_sdio_lv_ready(pwrctrl) ||
	    (!pwrctrl->test_sim_ignore && pwrctrl->test_sim_prot))
		return;

#ifdef CONFIG_MTK_WCN_CMB_SDIO_SLOT
{
	int r = mtk_wcn_cmb_stub_1vautok_for_dvfs();
	if (r)
		vcorefs_err("FAILED TO TRIGGER LV AUTOK (%d)\n", r);
}
#endif
}

static void abort_sdio_lv_autok(struct pwr_ctrl *pwrctrl)
{
	if (is_sdio_lv_ready(pwrctrl))
		return;

	autok_abort_action();
}

static bool can_dvfs_to_lowpwr_opp(struct pwr_ctrl *pwrctrl, bool in_autok, u32 index)
{
	if (index == 0)		/* skip performance OPP */
		return true;

	if (pwrctrl->sonoff_dvfs_only) {
		if (!pwrctrl->screen_off || !pwrctrl->mm_off) {
			vcorefs_err("ONLY SCREEN-ON/OFF DVFS\n");
			return false;
		}

		if (!pwrctrl->test_sim_ignore && pwrctrl->test_sim_prot) {
			vcorefs_err("TEST SIM PROTECT ENABLED\n");
			return false;
		}
	}

	if (!in_autok && !is_sdio_lv_ready(pwrctrl)) {
		vcorefs_err("SDIO LV IS NOT READY\n");
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

/*
 * return value:
 *   > 0: partial success
 *   = 0: full success
 *   < 0: failure
 */
static int __kick_dvfs_by_index(struct pwr_ctrl *pwrctrl, u32 index)
{
	int err;

	if (index == curr_opp_index) {
		if (index == prev_opp_index)
			return 0;
		else	/* try again since previous change is partial success */
			curr_opp_index = prev_opp_index;
	}

	/* 0 (performance) <= index <= X (low power) */
	if (index < curr_opp_index)
		err = do_dvfs_for_performance(pwrctrl, &pwrctrl->opp_table[index]);
	else
		err = do_dvfs_for_low_power(pwrctrl, &pwrctrl->opp_table[index]);

	if (err == 0) {
		prev_opp_index = index;
		curr_opp_index = index;
	} else if (err > 0) {
		prev_opp_index = curr_opp_index;
		curr_opp_index = index;
	}

	if (err >= 0 && !pwrctrl->screen_off)
		pwrctrl->son_opp_index = curr_opp_index;

	vcorefs_crit("done: curr_index = %u (%u), err = %d\n",
		     curr_opp_index, prev_opp_index, err);

	return err;
}

static int kick_dvfs_by_index(struct pwr_ctrl *pwrctrl, enum dvfs_kicker kicker, u32 index)
{
	int err;
	bool in_autok = false;

	vcorefs_crit("%u kick: lock = %u, index = %u, curr_index = %u (%u)\n",
		     kicker, pwrctrl->sdio_lock, index,
		     curr_opp_index, prev_opp_index);

	if (pwrctrl->sdio_lock) {
		if (index <= curr_opp_index &&
		    kicker == KR_SCREEN_ON) {		/* need performance */
			if (pwrctrl->lv_autok_abort)
				abort_sdio_lv_autok(pwrctrl);
			pwrctrl->sdio_lock = 0;
			goto KICK;
		}

		if (kicker == KR_SDIO_AUTOK) {
			in_autok = true;
			goto KICK;
		}

		if (kicker == KR_SCREEN_ON || kicker == KR_SYSFS)
			return 0;	/* fake success due to BUG check */
		else
			return -EAGAIN;
	}

KICK:
	if (!can_dvfs_to_lowpwr_opp(pwrctrl, in_autok, index))
		return -EPERM;

	err = __kick_dvfs_by_index(pwrctrl, index);
	return err;
}


/**************************************
 * Stay-LV DVFS Function/API
 **************************************/
bool vcorefs_is_95m_segment(void)
{
#if defined(CONFIG_MTK_WQHD) || defined(MTK_DISABLE_EFUSE)
	return false;
#else
	u32 func;
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	if (!pwrctrl->feature_en)
		return false;		/* make SDIO only do 1.125V AutoK */

	func = get_devinfo_with_index(24);
	func = (func >> 24) & 0xf;

	if (func >= 6 && func <= 10)	/* 95 segment */
		return false;

	return true;
#endif
}

bool vcorefs_is_stay_lv_enabled(void)
{
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	if (!pwrctrl->stay_lv_en)
		return false;

	return vcorefs_is_95m_segment();
}

static void __late_init_to_lowpwr_opp(struct pwr_ctrl *pwrctrl)
{
	vcorefs_crit("stay_lv = %u, 95m_seg = %u\n",
		     pwrctrl->stay_lv_en, vcorefs_is_95m_segment());

	if (!pwrctrl->feature_en || !vcorefs_is_stay_lv_enabled())
		return;

	kick_dvfs_by_index(pwrctrl, KR_LATE_INIT, OPPI_LATE_INIT_LP);
}

static int late_init_to_lowpwr_opp(void)
{
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	mutex_lock(&vcorefs_mutex);
	__late_init_to_lowpwr_opp(pwrctrl);
	mutex_unlock(&vcorefs_mutex);

	return 0;
}


/**************************************
 * SDIO AutoK related API
 **************************************/
/*
 * return value:
 *   = 1: lock at screen-off
 *   = 0: lock at screen-on
 */
int vcorefs_sdio_lock_dvfs(bool in_ot)
{
	int soff;
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	if (in_ot) {	/* avoid OT thread sleeping in vcorefs_mutex */
		vcorefs_crit("sdio lock: in online-tuning\n");
		return 0;
	}

	mutex_lock(&vcorefs_mutex);
	vcorefs_crit("sdio lock: screen_off = %u, mm_off = %u\n",
		     pwrctrl->screen_off, pwrctrl->mm_off);

	pwrctrl->sdio_lock = 1;
	soff = (pwrctrl->screen_off && pwrctrl->mm_off ? 1 : 0);
	mutex_unlock(&vcorefs_mutex);

	return soff;
}

u32 vcorefs_sdio_get_vcore_nml(void)
{
	return vcore_pmic_to_uv(curr_vcore_nml);
}

int vcorefs_sdio_set_vcore_nml(u32 vcore_uv)
{
	int i, r;
	u32 vcore_nml = vcore_uv_to_pmic(vcore_uv);
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	mutex_lock(&vcorefs_mutex);
	vcorefs_crit("sdio set: lock = %u, nml = 0x%x, curr_nml = 0x%x\n",
		     pwrctrl->sdio_lock, vcore_nml, curr_vcore_nml);

	if (vcore_nml == curr_vcore_nml) {
		mutex_unlock(&vcorefs_mutex);
		return 0;
	}

	if (!pwrctrl->feature_en || !pwrctrl->sdio_lock) {
		mutex_unlock(&vcorefs_mutex);
		return -EPERM;
	}

	for (i = 0; i < pwrctrl->num_opps; i++) {
		if (vcore_nml == pwrctrl->opp_table[i].vcore_nml)
			break;
	}
	if (i >= pwrctrl->num_opps) {
		mutex_unlock(&vcorefs_mutex);
		return -EINVAL;
	}

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

	if (!pwrctrl->sdio_lock) {
		mutex_unlock(&vcorefs_mutex);
		return -EPERM;
	}

	pwrctrl->sdio_lock = 0;
	__late_init_to_lowpwr_opp(pwrctrl);	/* try to save power */
	mutex_unlock(&vcorefs_mutex);

	return 0;
}

bool vcorefs_sdio_need_multi_autok(void)
{
	return vcorefs_is_95m_segment();
}


/**************************************
 * Screen-on/off DVFS Function/API
 **************************************/
static void kick_dvfs_after_screen_off(struct pwr_ctrl *pwrctrl)
{
	vcorefs_crit("screen_off = %u, mm_off = %u\n",
		     pwrctrl->screen_off, pwrctrl->mm_off);

	if (!pwrctrl->feature_en || !pwrctrl->screen_off || !pwrctrl->mm_off)
		return;

	if (pwrctrl->lv_autok_trig)	/* for Vcore 1.0 */
		trigger_sdio_lv_autok(pwrctrl);

	kick_dvfs_by_index(pwrctrl, KR_SCREEN_OFF, OPPI_SCREEN_OFF_LP);
}

static void kick_dvfs_before_screen_on(struct pwr_ctrl *pwrctrl)
{
	int i, r;

	vcorefs_crit("screen_off - %u, mm_off - %u\n",
		     pwrctrl->screen_off, pwrctrl->mm_off);

	if (!pwrctrl->feature_en || !pwrctrl->screen_off || !pwrctrl->mm_off)
		return;

	for (i = 1; i <= pwrctrl->son_dvfs_try; i++) {
		r = kick_dvfs_by_index(pwrctrl, KR_SCREEN_ON, pwrctrl->son_opp_index);
		if (!r)
			return;
	}

	BUG();	/* screen-on DVFS failed */
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

		wake_lock(&vcorefs_wakelock);

		if (pwrctrl->screen_off && pwrctrl->mm_off)
			spm_ensure_lte_pause_interval();	/* may sleep */

		mutex_lock(&vcorefs_mutex);
		kick_dvfs_after_screen_off(pwrctrl);
		atomic_dec(&kthread_nreq);
		mutex_unlock(&vcorefs_mutex);

		wake_unlock(&vcorefs_wakelock);
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
	smp_mb();
	wake_up_process(vcorefs_ktask);
}

/* this will be called at CLKMGR late resume */
void vcorefs_clkmgr_notify_mm_on(void)
{
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	pwrctrl->mm_off = 0;
	smp_mb();
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
	char *p = buf;
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	p += sprintf(p, "feature_en = %u\n"      , pwrctrl->feature_en);
	p += sprintf(p, "sonoff_dvfs_only = %u\n", pwrctrl->sonoff_dvfs_only);
	p += sprintf(p, "stay_lv_en = %u\n"      , pwrctrl->stay_lv_en);
	p += sprintf(p, "vcore_dvs = %u\n"       , pwrctrl->vcore_dvs);
	p += sprintf(p, "ddr_dfs = %u\n"         , pwrctrl->ddr_dfs);
	p += sprintf(p, "axi_dfs = %u\n"         , pwrctrl->axi_dfs);
	p += sprintf(p, "screen_off = %u\n"      , pwrctrl->screen_off);
	p += sprintf(p, "mm_off = %u\n"          , pwrctrl->mm_off);
	p += sprintf(p, "sdio_lock = %u\n"       , pwrctrl->sdio_lock);
	p += sprintf(p, "sdio_lv_check = %u\n"   , pwrctrl->sdio_lv_check);
	p += sprintf(p, "lv_autok_trig = %u\n"   , pwrctrl->lv_autok_trig);
	p += sprintf(p, "lv_autok_abort = %u\n"  , pwrctrl->lv_autok_abort);
	p += sprintf(p, "test_sim_ignore = %u\n" , pwrctrl->test_sim_ignore);
	p += sprintf(p, "test_sim_prot = 0x%x\n" , pwrctrl->test_sim_prot);
	p += sprintf(p, "son_opp_index = %u\n"   , pwrctrl->son_opp_index);
	p += sprintf(p, "son_dvfs_try = %u\n"    , pwrctrl->son_dvfs_try);

	p += sprintf(p, "opp_table = 0x%p\n"     , pwrctrl->opp_table);
	p += sprintf(p, "num_opps = %u\n"        , pwrctrl->num_opps);
	p += sprintf(p, "curr_opp_index = %u\n"  , curr_opp_index);
	p += sprintf(p, "prev_opp_index = %u\n"  , prev_opp_index);

	p += sprintf(p, "curr_vcore_ao = 0x%x\n" , pwrctrl->curr_vcore_ao);
	p += sprintf(p, "curr_vcore_pdn = 0x%x\n", pwrctrl->curr_vcore_pdn);
	p += sprintf(p, "curr_vcore_nml = 0x%x\n", curr_vcore_nml);
	p += sprintf(p, "sdio_trans_pause = %u\n", pwrctrl->sdio_trans_pause);
	p += sprintf(p, "dma_dummy_read = %u\n"  , pwrctrl->dma_dummy_read);

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
	u32 val;
	char cmd[32];
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	if (sscanf(buf, "%31s %x", cmd, &val) != 2)
		return -EPERM;

	vcorefs_crit("pwr_ctrl: cmd = %s, val = 0x%x\n", cmd, val);

	if (!strcmp(cmd, "feature_en")) {
		pwrctrl->feature_en = val;
		pwrctrl->lv_autok_trig = (val && pwrctrl->sdio_lv_check);
	} else if (!strcmp(cmd, "vcore_dvs")) {
		pwrctrl->vcore_dvs = val;
	} else if (!strcmp(cmd, "ddr_dfs")) {
		pwrctrl->ddr_dfs = val;
	} else if (!strcmp(cmd, "axi_dfs")) {
		pwrctrl->axi_dfs = val;
	} else if (!strcmp(cmd, "sdio_lv_check")) {
		pwrctrl->sdio_lv_check = val;
		pwrctrl->lv_autok_trig = (pwrctrl->feature_en && val);
	} else if (!strcmp(cmd, "lv_autok_trig")) {
		pwrctrl->lv_autok_trig = !!val;
	} else if (!strcmp(cmd, "lv_autok_abort")) {
		pwrctrl->lv_autok_abort = val;
	} else if (!strcmp(cmd, "test_sim_ignore")) {
		pwrctrl->test_sim_ignore = val;
	} else if (!strcmp(cmd, "test_sim_prot")) {
		pwrctrl->test_sim_prot = val;
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
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;
	struct dvfs_opp *opp;

	for (i = 0; i < pwrctrl->num_opps; i++) {
		opp = &pwrctrl->opp_table[i];

		p += sprintf(p, "[OPP %d]\n", i);
		p += sprintf(p, "vcore_ao = 0x%x\n" , opp->vcore_ao);
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
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;
	struct dvfs_opp *opp;

	if (sscanf(buf, "%31s %x %x", cmd, &val1, &val2) != 3)
		return -EPERM;

	vcorefs_debug("opp_table: cmd = %s, val1 = 0x%x, val2 = 0x%x\n", cmd, val1, val2);

	if (val1 >= pwrctrl->num_opps)
		return -EINVAL;

	opp = &pwrctrl->opp_table[val1];

	if (!strcmp(cmd, "vcore_ao") && val2 < VCORE_INVALID)
		opp->vcore_ao = val2;
	else if (!strcmp(cmd, "vcore_pdn") && val2 < VCORE_INVALID)
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
	u32 vcore_ao, vcore_pdn;
	char *p = buf;

	vcore_ao = get_vcore_ao();
	vcore_pdn = get_vcore_pdn();

	p += sprintf(p, "VCORE_AO = %u (0x%x)\n" , vcore_pmic_to_uv(vcore_ao), vcore_ao);
	p += sprintf(p, "VCORE_PDN = %u (0x%x)\n", vcore_pmic_to_uv(vcore_pdn), vcore_pdn);
	p += sprintf(p, "FDDR = %u\n", get_ddr_khz());
	p += sprintf(p, "FAXI = %u\n", get_axi_khz());

	BUG_ON(p - buf >= PAGE_SIZE);
	return p - buf;
}

static ssize_t vcore_debug_store(struct kobject *kobj, struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	int r;
	u32 val;
	char cmd[32];
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	if (sscanf(buf, "%31s %x", cmd, &val) != 2)
		return -EPERM;

	vcorefs_debug("vcore_debug: cmd = %s, val = 0x%x\n", cmd, val);

	if (!strcmp(cmd, "opp_vcore_set") && val < pwrctrl->num_opps) {
		mutex_lock(&vcorefs_mutex);
		if (pwrctrl->feature_en && pwrctrl->screen_off && pwrctrl->mm_off) {
			r = kick_dvfs_by_index(pwrctrl, KR_SYSFS, val);
			BUG_ON(r);
		}
		mutex_unlock(&vcorefs_mutex);
	} else if (!strcmp(cmd, "opp_vcore_setX") && val < pwrctrl->num_opps) {
		mutex_lock(&vcorefs_mutex);
		if (pwrctrl->feature_en) {
			u8 orig = pwrctrl->sonoff_dvfs_only;
			pwrctrl->sonoff_dvfs_only = 0;
			r = kick_dvfs_by_index(pwrctrl, KR_SYSFS, val);
			BUG_ON(r);
			pwrctrl->sonoff_dvfs_only = orig;
		}
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
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;
	struct dvfs_opp *opp;

	mutex_lock(&vcorefs_mutex);
	pwrctrl->curr_vcore_ao = get_vcore_ao();
	pwrctrl->curr_vcore_pdn = get_vcore_pdn();
	BUG_ON(pwrctrl->curr_vcore_ao >= VCORE_INVALID ||
	       pwrctrl->curr_vcore_pdn >= VCORE_INVALID);

	pwrctrl->curr_ddr_khz = get_ddr_khz();
	pwrctrl->curr_axi_khz = get_axi_khz();

	opp = &pwrctrl->opp_table[0];
	opp->vcore_ao = pwrctrl->curr_vcore_ao;		/* by PTPOD */
	opp->vcore_pdn = pwrctrl->curr_vcore_pdn;	/* by PTPOD */
	opp->ddr_khz = pwrctrl->curr_ddr_khz;		/* E1: 1600M, E2: 1792M, M: 1333M */
	opp->axi_khz = pwrctrl->curr_axi_khz;
	vcorefs_crit("OPP 0: vcore_ao = 0x%x, vcore_pdn = 0x%x, ddr_khz = %u, axi_khz = %u\n",
		     opp->vcore_ao, opp->vcore_pdn, opp->ddr_khz, opp->axi_khz);

	if (vcorefs_is_95m_segment()) {
		BUG_ON(opp->ddr_khz != FDDR_S2_KHZ);	/* violate spec */
		pwrctrl->sonoff_dvfs_only = 0;
		pwrctrl->lv_autok_trig = 0;
		/*pwrctrl->stay_lv_en = 1;*/
	} else {
		/*pwrctrl->feature_en = 0;*/
		pwrctrl->lv_autok_trig = (pwrctrl->feature_en && pwrctrl->sdio_lv_check);
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

	wake_lock_init(&vcorefs_wakelock, WAKE_LOCK_SUSPEND, "vcorefs");

	r = create_vcorefs_kthread();	/* for screen-off DVFS */
	if (r) {
		vcorefs_err("FAILED TO CREATE KTHREAD (%d)\n", r);
		return r;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&vcorefs_earlysuspend_desc);	/* for screen-on DVFS */
#endif

	r = sysfs_create_group(power_kobj, &vcorefs_attr_group);
	if (r)
		vcorefs_err("FAILED TO CREATE /sys/power/vcorefs (%d)\n", r);

	return r;
}

module_init(vcorefs_module_init);
late_initcall_sync(late_init_to_lowpwr_opp);

MODULE_DESCRIPTION("Vcore DVFS Driver v1.1");
