/*
 * Copyright (C) 2019 MediaTek Inc.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/of.h>
#include <linux/notifier.h>
#include <linux/alarmtimer.h>
#include <linux/wait.h>
#include <mt-plat/prop_chgalgo_class.h>
#include <mt-plat/mtk_battery.h>

#define PCA_DV2_ALGO_VERSION	"1.0.14_G"
#define MS_TO_NS(msec) ((msec) * 1000 * 1000)
#define MIN(A, B) (((A) < (B)) ? (A) : (B))
#define MAX(A, B) (((A) > (B)) ? (A) : (B))

/* Parameters */
#define DV2_VTA_INIT		(5000)	/* mV */
#define DV2_ITA_INIT		(3000)	/* mA */
#define DV2_TA_WDT_MIN		(10000)	/* ms */
#define DV2_TA_GAP_VMIN		(200)	/* mV */
#define DV2_DVCHG_VBUSALM_GAP	(100)	/* mV */
#define DV2_DVCHG_CONVERT_RATIO	(210)
#define DV2_VBUSOVP_RATIO	(110)
#define DV2_IBUSOCP_RATIO	(110)
#define DV2_VBATOVP_RATIO	(110)
#define DV2_IBATOCP_RATIO	(110)
#define DV2_ITAOCP_RATIO	(110)
#define DV2_IBUSUCPF_RECHECK	(250)	/* mA */
#define DV2_VBUS_CALI_THRESHOLD	(150)	/* mV */
#define DV2_CV_LOWER_BOUND_GAP	(20)	/* mV */
#define DV2_ALGO_INIT_POLLING_INTERVAL	(500)	/* ms */
#define DV2_ALGO_INIT_RETRY_MAX		(0)
#define DV2_ALGO_MEASURE_R_RETRY_MAX	(3)
#define DV2_ALGO_MEASURE_R_AVG_TIMES	(10)

#define DV2_HWERR_NOTIFY \
	(BIT(PCA_NOTIEVT_VBUSOVP) | BIT(PCA_NOTIEVT_IBUSOCP) | \
	BIT(PCA_NOTIEVT_VBATOVP) | BIT(PCA_NOTIEVT_IBATOCP) | \
	BIT(PCA_NOTIEVT_VOUTOVP) | BIT(PCA_NOTIEVT_VDROVP) | \
	BIT(PCA_NOTIEVT_IBUSUCP_FALL))

#define DV2_RESET_NOTIFY \
	(BIT(PCA_NOTIEVT_DETACH) | BIT(PCA_NOTIEVT_HARDRESET))

enum dv2_algo_state {
	DV2_ALGO_INIT = 0,
	DV2_ALGO_MEASURE_R,
	DV2_ALGO_SS_SWCHG,
	DV2_ALGO_SS_DVCHG,
	DV2_ALGO_CC_CV,
	DV2_ALGO_STOP,
	DV2_ALGO_STATE_MAX,
};

enum dv2_thermal_level {
	DV2_THERMAL_VERY_COLD = 0,
	DV2_THERMAL_COLD,
	DV2_THERMAL_VERY_COOL,
	DV2_THERMAL_COOL,
	DV2_THERMAL_NORMAL,
	DV2_THERMAL_WARM,
	DV2_THERMAL_VERY_WARM,
	DV2_THERMAL_HOT,
	DV2_THERMAL_VERY_HOT,
	DV2_THERMAL_MAX,
};

enum dv2_rcable_level {
	DV2_RCABLE_NORMAL = 0,
	DV2_RCABLE_BAD1,
	DV2_RCABLE_BAD2,
	DV2_RCABLE_BAD3,
	DV2_RCABLE_MAX,
};

enum dv2_dvchg_role {
	DV2_DVCHG_MASTER = 0,
	DV2_DVCHG_SLAVE,
	DV2_DVCHG_MAX,
};

static const char *const __dv2_dvchg_role_name[DV2_DVCHG_MAX] = {
	"master", "slave",
};

static const char *const __dv2_algo_state_name[DV2_ALGO_STATE_MAX] = {
	"INIT", "MEASURE_R", "SS_SWCHG", "SS_DVCHG", "CC_CV", "STOP",
};

/* Setting from dtsi */
struct dv2_algo_desc {
	u32 polling_interval;		/* polling interval */
	u32 ta_cv_ss_repeat_tmin;	/* min repeat time of ss for TA CV */
	u32 vbat_cv;			/* vbat constant voltage */
	u32 start_soc_min;		/* algo start bat low bound */
	u32 start_soc_max;		/* algo start bat upper bound */
	u32 start_vbat_max;		/* algo end bat upper bound */
	u32 idvchg_term;		/* terminated current */
	u32 idvchg_step;		/* input current step */
	u32 ita_level[DV2_RCABLE_MAX];	/* input current */
	u32 rcable_level[DV2_RCABLE_MAX];	/* cable impedance level */
	u32 ita_level_dual[DV2_RCABLE_MAX];	/* input current */
	u32 rcable_level_dual[DV2_RCABLE_MAX];	/* cable impedance level */
	u32 idvchg_ss_init;		/* SS state init input current */
	u32 idvchg_ss_step;		/* SS state input current step */
	u32 idvchg_ss_step1;		/* SS state input current step2 */
	u32 idvchg_ss_step2;		/* SS state input current step3 */
	u32 idvchg_ss_step1_vbat;	/* vbat threshold for ic_ss_step2 */
	u32 idvchg_ss_step2_vbat;	/* vbat threshold for ic_ss_step3 */
	u32 ta_blanking;		/* wait TA stable */
	u32 swchg_aicr;			/* CC state swchg input current */
	u32 swchg_ichg;			/* CC state swchg charging current */
	u32 swchg_aicr_ss_init;		/* SWCHG_SS state init input current */
	u32 swchg_aicr_ss_step;		/* SWCHG_SS state input current step */
	u32 swchg_off_vbat;		/* VBAT to turn off SWCHG */
	u32 force_ta_cv_vbat;		/* Force TA using CV mode */
	u32 chg_time_max;		/* max charging time */
	int tta_level_def[DV2_THERMAL_MAX];	/* TA temp level */
	int tta_curlmt[DV2_THERMAL_MAX];	/* TA temp current limit */
	int tbat_level_def[DV2_THERMAL_MAX];	/* BAT temp level */
	int tbat_curlmt[DV2_THERMAL_MAX];	/* BAT temp current limit */
	int tdvchg_level_def[DV2_THERMAL_MAX];	/* DVCHG temp level */
	int tdvchg_curlmt[DV2_THERMAL_MAX];	/* DVCHG temp current limit */
	int tswchg_level_def[DV2_THERMAL_MAX];	/* SWCHG temp level */
	int tswchg_curlmt[DV2_THERMAL_MAX];	/* SWCHG temp current limit */
	u32 tta_recovery_area;
	u32 tbat_recovery_area;
	u32 tdvchg_recovery_area;
	u32 tswchg_recovery_area;
	u32 ifod_threshold;		/* FOD current threshold */
	u32 rsw_min;			/* min rsw */
	u32 ircmp_rbat;			/* IR compensation's rbat */
	u32 ircmp_vclamp;		/* IR compensation's vclamp */
	u32 vta_cap_min;		/* min ta voltage capability */
	u32 vta_cap_max;		/* max ta voltage capability */
	u32 ita_cap_min;		/* min ta current capability */
	const char **support_ta;	/* supported ta name */
	u32 support_ta_cnt;		/* supported ta count */
	bool allow_not_check_ta_status;	/* allow not to check ta status */
};

/* Algorithm related information */
struct dv2_algo_data {
	/* PCA devices */
	struct prop_chgalgo_device **pca_ta_pool;
	struct prop_chgalgo_device *pca_ta;
	struct prop_chgalgo_device *pca_swchg;
	struct prop_chgalgo_device *pca_dvchg[DV2_DVCHG_MAX];

	/* Thread & Timer */
	struct alarm timer;
	struct task_struct *task;
	struct mutex lock;
	struct mutex ext_lock;
	wait_queue_head_t wq;
	atomic_t wakeup_thread;
	atomic_t stop_thread;
	atomic_t stop_algo;

	/* Notify */
	struct mutex notify_lock;
	u32 notify;

	/* Algorithm */
	bool inited;
	bool ta_ready;
	bool run_once;
	bool is_swchg_en;
	bool is_dvchg_en[DV2_DVCHG_MAX];
	bool ignore_ibusucpf;
	bool force_ta_cv;
	bool tried_dual_dvchg;
	bool suspect_ta_cc;
	struct prop_chgalgo_ta_auth_data ta_auth_data;
	u32 vta_setting;
	u32 ita_setting;
	u32 vta_measure;
	u32 ita_measure;
	u32 ita_gap_per_vstep;
	u32 ita_gap_avg_cnt;
	u32 ichg_setting;
	u32 aicr_setting;
	u32 aicr_lmt;
	u32 aicr_init_lmt;
	u32 idvchg_cc;
	u32 idvchg_ss_init;
	u32 idvchg_term;
	int vbus_cali;
	u32 r_sw;
	u32 r_cable;
	u32 r_bat;
	u32 r_total;
	u32 ita_lmt;
	u32 ita_pwr_lmt;
	u32 cv_lower_bound;
	u32 err_retry_cnt;
	u32 vbusovp;
	u32 zcv;
	u32 vbat_cv_no_ircmp;
	u32 vbat_cv;
	u32 vbat_ircmp;
	int vta_comp;
	bool is_vbat_over_cv;
	struct timespec stime;
	enum dv2_algo_state state;
	enum dv2_thermal_level tbat_level;
	enum dv2_thermal_level tta_level;
	enum dv2_thermal_level tdvchg_level;
	enum dv2_thermal_level tswchg_level;
	int thermal_throttling;
	int jeita_vbat_cv;
};

/*
 * @reset_ta: set output voltage/current of TA to 5V/3A and disable
 *            direct charge
 * @hardreset: send hardreset to port partner
 * Note: hardreset's priority is higher than reset_ta
 */
struct dv2_stop_info {
	bool hardreset_ta;
	bool reset_ta;
};

struct dv2_algo_info {
	struct device *dev;
	struct prop_chgalgo_device *pca;
	struct dv2_algo_desc *desc;
	struct dv2_algo_data *data;
};

/* if there's no property in dts, these values will by applied */
static struct dv2_algo_desc algo_desc_defval = {
	.polling_interval = 500,
	.ta_cv_ss_repeat_tmin = 25,
	.vbat_cv = 4350,
	.start_soc_min = 5,
	.start_soc_max = 80,
	.start_vbat_max = 4300,
	.idvchg_term = 500,
	.idvchg_step = 50,
	.ita_level = {3000, 2700, 2400, 2000},
	.rcable_level = {250, 278, 313, 375},
	.ita_level_dual = {4000, 3700, 3400, 3000},
	.rcable_level_dual = {188, 203, 221, 250},
	.idvchg_ss_init = 500,
	.idvchg_ss_step = 250,
	.idvchg_ss_step1 = 100,
	.idvchg_ss_step2 = 50,
	.idvchg_ss_step1_vbat = 4000,
	.idvchg_ss_step2_vbat = 4200,
	.ta_blanking = 500,
	.swchg_aicr = 0,
	.swchg_ichg = 0,
	.swchg_aicr_ss_init = 400,
	.swchg_aicr_ss_step = 200,
	.swchg_off_vbat = 4250,
	.force_ta_cv_vbat = 4250,
	.chg_time_max = 5400,
	.tta_level_def = {0, 0, 0, 0, 25, 40, 50, 60, 70},
	.tta_curlmt = {0, 0, 0, 0, 0, 300, 600, 900, -1},
	.tta_recovery_area = 3,
	.tbat_level_def = {0, 0, 0, 5, 25, 40, 50, 55, 60},
	.tbat_curlmt = {-1, -1, -1, 300, 0, 300, 600, 900, -1},
	.tbat_recovery_area = 3,
	.tdvchg_level_def = {0, 0, 0, 5, 25, 55, 60, 65, 70},
	.tdvchg_curlmt = {-1, -1, -1, 300, 0, 300, 600, 900, -1},
	.tdvchg_recovery_area = 3,
	.tswchg_level_def = {0, 0, 0, 5, 25, 55, 60, 65, 70},
	.tswchg_curlmt = {-1, -1, -1, 200, 0, 200, 300, 400, -1},
	.tswchg_recovery_area = 3,
	.ifod_threshold = 200,
	.rsw_min = 20,
	.ircmp_rbat = 40,
	.ircmp_vclamp = 0,
	.vta_cap_min = 6800,
	.vta_cap_max = 11000,
	.ita_cap_min = 1000,
	.allow_not_check_ta_status = true,
};

/*
 * Send notification to all subscribers
 * This function is called by dv2 thread, "DO NOT" call dv2's API in the
 * callback function that you registered
 */
static int __dv2_send_notification(struct dv2_algo_info *info,
				   unsigned long val,
				   struct prop_chgalgo_notify *notify)
{
	return srcu_notifier_call_chain(&info->pca->desc->nh, val, notify);
}

/* Check if there is error notification coming from H/W */
static bool __dv2_is_hwerr_notified(struct dv2_algo_info *info)
{
	struct dv2_algo_data *data = info->data;
	bool err = false;
	u32 hwerr = DV2_HWERR_NOTIFY;

	mutex_lock(&data->notify_lock);
	if (data->ignore_ibusucpf)
		hwerr &= ~BIT(PCA_NOTIEVT_IBUSUCP_FALL);
	err = !!(data->notify & hwerr);
	if (err)
		PCA_ERR("H/W error(0x%08X)", hwerr);
	mutex_unlock(&data->notify_lock);
	return err;
}

/*
 * Get ADC value from divider charger
 * Note: ibus will sum up value from all enabled chargers
 * (master dvchg, slave dvchg and swchg)
 */
static int __dv2_stop(struct dv2_algo_info *info, struct dv2_stop_info *sinfo);
static int __dv2_get_adc(struct dv2_algo_info *info,
			 enum prop_chgalgo_adc_channel chan, int *min, int *max)
{
	struct dv2_algo_data *data = info->data;
	int ret, i, ibus;
	struct dv2_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	if (!data->pca_dvchg[DV2_DVCHG_MASTER])
		return -EINVAL;
	if (atomic_read(&data->stop_algo)) {
		PCA_INFO("stop algo\n");
		goto stop;
	}
	*max = *min = 0;
	if (chan == PCA_ADCCHAN_IBUS) {
		for (i = DV2_DVCHG_MASTER; i < DV2_DVCHG_MAX; i++) {
			if (!data->is_dvchg_en[i])
				continue;
			ret = prop_chgalgo_get_adc(data->pca_dvchg[i],
						   PCA_ADCCHAN_IBUS, &ibus,
						   &ibus);
			if (ret < 0) {
				PCA_ERR("get dvchg ibus fail(%d)\n", ret);
				return ret;
			}
			*min += ibus;
		}
		if (data->is_swchg_en) {
			ret = prop_chgalgo_get_adc(data->pca_swchg,
						   PCA_ADCCHAN_IBUS, &ibus,
						   &ibus);
			if (ret < 0) {
				PCA_ERR("get swchg ibus fail(%d)\n", ret);
				return ret;
			}
			*min += ibus;
		}
		*max = *min;
		return 0;
	}
	return prop_chgalgo_get_adc(data->pca_dvchg[DV2_DVCHG_MASTER], chan,
				    min, max);
stop:
	__dv2_stop(info, &sinfo);
	return -EIO;
}

/*
 * Calculate VBUS for divider charger
 * If divider charger is operating in CC mode
 * the VBUS only needs to be 2 times of VOUT. So we reduce ratio by 0.5
 */
static inline u32 __dv2_vout2vbus(struct dv2_algo_info *info, u32 vout)
{
	struct dv2_algo_data *data = info->data;

	if (!data->is_dvchg_en[DV2_DVCHG_MASTER])
		return vout * DV2_DVCHG_CONVERT_RATIO / 100;
	return vout * (DV2_DVCHG_CONVERT_RATIO - 5) / 100;
}

/*
 * Maximum 3% variation (from PD's sepcification)
 * Keep vta_setting 4% higher than vta_measure
 */
static inline u32 __dv2_vta_add_gap(struct dv2_algo_info *info, u32 vta)
{
	return MAX(vta * 104 / 100, DV2_TA_GAP_VMIN);
}

/*
 * Get output current and voltage measured by TA
 * and updates measured data
 */
static inline int __dv2_get_ta_cap(struct dv2_algo_info *info)
{
	struct dv2_algo_data *data = info->data;

	return prop_chgalgo_get_ta_measure_cap(data->pca_ta, &data->vta_measure,
					       &data->ita_measure);
}

static inline int __dv2_get_ta_cap_by_supportive(struct dv2_algo_info *info,
						 int *vta, int *ita)
{
	int ret;
	struct dv2_algo_data *data = info->data;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;

	if (auth_data->support_meas_cap) {
		ret = __dv2_get_ta_cap(info);
		if (ret < 0) {
			PCA_ERR("get ta cap fail(%d)\n", ret);
			return ret;
		}
		*vta = data->vta_measure;
		*ita = data->ita_measure;
		return 0;
	}
	ret = __dv2_get_adc(info, PCA_ADCCHAN_VBUS, vta, vta);
	if (ret < 0) {
		PCA_ERR("get vbus fail(%d)\n", ret);
		return ret;
	}
	return __dv2_get_adc(info, PCA_ADCCHAN_IBUS, ita, ita);
}

/*
 * Calculate calibrated output voltage of TA by measured resistence
 * Firstly, calculate voltage outputing from divider charger
 * Secondly, calculate voltage outputing from TA
 *
 * @ita: expected output current of TA
 * @vta: calibrated output voltage of TA
 */
static int __dv2_get_cali_vta(struct dv2_algo_info *info, u32 ita, u32 *vta)
{
	int ret, vbat;
	struct dv2_algo_data *data = info->data;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;
	u32 ibat, vbus, vta_add_gap;

	ret = __dv2_get_adc(info, PCA_ADCCHAN_VBAT, &vbat, &vbat);
	if (ret < 0) {
		PCA_ERR("get vbat fail(%d)\n", ret);
		return ret;
	}
	ibat = 2 * (data->is_swchg_en ? (ita - data->aicr_setting) : ita);
	vbus = __dv2_vout2vbus(info, vbat + (ibat * data->r_sw) / 1000);
	*vta = vbus + (data->vbus_cali + data->vta_comp +
		       (ita * data->r_cable / 1000));
	if (data->is_dvchg_en[DV2_DVCHG_MASTER]) {
		ret = __dv2_get_ta_cap(info);
		if (ret < 0) {
			PCA_ERR("get ta cap fail(%d)\n", ret);
			return ret;
		}
		vta_add_gap = __dv2_vta_add_gap(info, data->vta_measure);
		*vta = MAX(*vta, vta_add_gap);
	}
	if (*vta >= auth_data->vcap_max)
		*vta = auth_data->vcap_max;
	return 0;
}

/*
 * Tracking vbus of divider charger using vbusovp alarm
 * If vbusovp alarm is triggered, algorithm needs to come up with a new vbus
 */
static int __dv2_set_vbus_tracking(struct dv2_algo_info *info)
{
	int ret, vbus;
	struct dv2_algo_data *data = info->data;

	ret = __dv2_get_adc(info, PCA_ADCCHAN_VBUS, &vbus, &vbus);
	if (ret < 0) {
		PCA_ERR("get vbus fail(%d)\n", ret);
		return ret;
	}
	return prop_chgalgo_set_vbusovp_alarm(data->pca_dvchg[DV2_DVCHG_MASTER],
					      vbus + DV2_DVCHG_VBUSALM_GAP);
}

/* Calculate power limited ita according to TA's power limitation */
static u32 __dv2_get_ita_pwr_lmt_by_vta(struct dv2_algo_info *info, u32 vta)
{
	struct dv2_algo_data *data = info->data;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;
	u32 ita_pwr_lmt;

	if (!auth_data->pwr_lmt)
		return data->ita_lmt;

	ita_pwr_lmt = auth_data->pdp * 1000000 / vta;
	/* Round to nearest level */
	if (auth_data->support_cc) {
		ita_pwr_lmt /= auth_data->ita_step;
		ita_pwr_lmt *= auth_data->ita_step;
	}
	return (ita_pwr_lmt < data->ita_lmt) ? ita_pwr_lmt : data->ita_lmt;
}

/*
 * Set output capability of TA in CC mode and update setting in data
 *
 * @vta: output voltage of TA, mV
 * @ita: output current of TA, mA
 */
static inline int __dv2_set_ta_cap_cc(struct dv2_algo_info *info, u32 vta,
				      u32 ita)
{
	int ret, vbat, bat_current, ita_meas;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;
	bool set_opt_vta = true;
	bool is_ta_cc = false;
	u32 opt_vta;
	struct dv2_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	if (data->vta_setting == vta && data->ita_setting == ita &&
	    data->state != DV2_ALGO_INIT)
		return 0;
	while (true) {
		if (atomic_read(&data->stop_algo)) {
			PCA_INFO("stop algo\n");
			goto stop;
		}
		/* Check TA's PDP */
		data->ita_pwr_lmt = __dv2_get_ita_pwr_lmt_by_vta(info, vta);
		if (data->ita_pwr_lmt < ita) {
			PCA_INFO("ita(%d) > ita_pwr_lmt(%d)\n", ita,
				 data->ita_pwr_lmt);
			ita = data->ita_pwr_lmt;
		}
		ret = prop_chgalgo_set_ta_cap(data->pca_ta, vta, ita);
		if (ret < 0) {
			PCA_ERR("set ta cap fail(%d)\n", ret);
			return ret;
		}
		msleep(desc->ta_blanking);
		if (!data->is_dvchg_en[DV2_DVCHG_MASTER])
			break;
		ret = prop_chgalgo_is_ta_cc(data->pca_ta, &is_ta_cc);
		if (ret < 0) {
			PCA_ERR("get ta cc mode fail(%d)\n", ret);
			return ret;
		}
		ret = __dv2_get_ta_cap(info);
		if (ret < 0) {
			PCA_ERR("get ta cap fail(%d)\n", ret);
			return ret;
		}
		PCA_DBG("vta(set,meas,comp),ita(set,meas)=(%d,%d,%d),(%d,%d)\n",
			vta, data->vta_measure, data->vta_comp, ita,
			data->ita_measure);
		if (is_ta_cc) {
			opt_vta = __dv2_vta_add_gap(info, data->vta_measure);
			if (vta > opt_vta && set_opt_vta) {
				data->vta_comp -= (vta - opt_vta);
				vta = opt_vta;
				set_opt_vta = false;
				continue;
			}
			break;
		}
		if (vta >= auth_data->vcap_max) {
			PCA_ERR("vta(%d) over capability(%d)\n", vta,
				auth_data->vcap_max);
			goto stop;
		}
		if (__dv2_is_hwerr_notified(info)) {
			PCA_ERR("H/W error notified\n");
			goto stop;
		}
		ret = __dv2_get_adc(info, PCA_ADCCHAN_VBAT, &vbat, &vbat);
		if (ret < 0) {
			PCA_ERR("get vbat fail(%d)\n", ret);
			return ret;
		}
		if (vbat >= data->vbat_cv) {
			PCA_INFO("vbat(%d), decrease ita immediately\n", vbat);
			ita -= auth_data->ita_step;
			continue;
		}
		bat_current = battery_get_bat_current() / 10;

		ret = __dv2_get_adc(info, PCA_ADCCHAN_IBUS, &ita_meas,
				    &ita_meas);
		PCA_ERR("Not in cc mode, ibat_gauge = %dmA, ibus = %dmA\n",
			bat_current, ita_meas);
		set_opt_vta = false;
		data->vta_comp += auth_data->vta_step;
		vta += auth_data->vta_step;
		vta = MIN(vta, auth_data->vcap_max);
	}
	data->vta_setting = vta;
	data->ita_setting = ita;
	PCA_INFO("vta,ita = (%d,%d)\n", vta, ita);
	__dv2_set_vbus_tracking(info);
	return 0;
stop:
	__dv2_stop(info, &sinfo);
	return -EIO;
}

/*
 * Set TA's output voltage & current by a given current and
 * calculated voltage
 */
static inline int __dv2_set_ta_cap_cc_by_cali_vta(struct dv2_algo_info *info,
						  u32 ita)
{
	int ret;
	u32 vta;

	ret = __dv2_get_cali_vta(info, ita, &vta);
	if (ret < 0) {
		PCA_ERR("get cali vta fail(%d)\n", ret);
		return ret;
	}
	return __dv2_set_ta_cap_cc(info, vta, ita);
}


static inline int __dv2_set_ta_cap_cv(struct dv2_algo_info *info, u32 vta,
				      u32 ita)
{
	int ret, ita_meas_pre, ita_meas_post, vta_meas;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;
	u32 vstep_cnt, ita_gap, vta_gap;
	struct dv2_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	if (__dv2_is_hwerr_notified(info)) {
		PCA_ERR("H/W error notified\n");
		goto stop;
	}
	if (atomic_read(&data->stop_algo)) {
		PCA_INFO("stop algo\n");
		goto stop;
	}
	if (vta > auth_data->vcap_max) {
		PCA_ERR("vta(%d) over capability(%d)\n", vta,
			auth_data->vcap_max);
		goto stop;
	}
	if (ita < auth_data->ita_min) {
		PCA_INFO("ita(%d) under ita_min(%d)\n", ita,
			 auth_data->ita_min);
		ita = auth_data->ita_min;
	}
	if (data->vta_setting == vta && data->ita_setting == ita)
		return 0;
	vta_gap = abs(data->vta_setting - vta);

	/* Get ta cap before setting */
	ret = __dv2_get_ta_cap_by_supportive(info, &vta_meas, &ita_meas_pre);
	if (ret < 0) {
		PCA_ERR("get ta cap by supportive fail(%d)\n", ret);
		return ret;
	}

	/* Not to increase vta if it exceeds pwr_lmt */
	data->ita_pwr_lmt = __dv2_get_ita_pwr_lmt_by_vta(info, vta);
	if (vta > data->vta_setting &&
	    (data->ita_pwr_lmt < ita_meas_pre + data->ita_gap_per_vstep)) {
		PCA_INFO("ita_meas(%d) + ita_gap(%d) > pwr_lmt(%d)\n",
			 ita_meas_pre, data->ita_gap_per_vstep,
			 data->ita_pwr_lmt);
		return 0;
	}

	/* Set ta cap */
	ret = prop_chgalgo_set_ta_cap(data->pca_ta, vta, ita);
	if (ret < 0) {
		PCA_ERR("set ta cap fail(%d)\n", ret);
		return ret;
	}
	if (vta_gap > auth_data->vta_step || data->state != DV2_ALGO_SS_DVCHG)
		msleep(desc->ta_blanking);

	/* Get ta cap after setting */
	ret = __dv2_get_ta_cap_by_supportive(info, &vta_meas, &ita_meas_post);
	if (ret < 0) {
		PCA_ERR("get ta cap by supportive fail(%d)\n", ret);
		return ret;
	}

	if (data->is_dvchg_en[DV2_DVCHG_MASTER] &&
	    (ita_meas_post > ita_meas_pre) && (vta > data->vta_setting)) {
		vstep_cnt = (MAX(vta, vta_meas) - data->vta_setting) /
			    auth_data->vta_step;
		ita_gap = (ita_meas_post - ita_meas_pre) / vstep_cnt;
		if (ita_gap > data->ita_gap_per_vstep) {
			data->ita_gap_per_vstep *= data->ita_gap_avg_cnt;
			data->ita_gap_avg_cnt++;
			data->ita_gap_per_vstep =
				(data->ita_gap_per_vstep + ita_gap) /
				data->ita_gap_avg_cnt;
		}
		PCA_INFO("ita gap(now,updated)=(%d,%d)\n",
			 ita_gap, data->ita_gap_per_vstep);
	}

	data->vta_setting = vta;
	data->ita_setting = ita;
	data->vta_measure = vta_meas;
	data->ita_measure = ita_meas_post;
	PCA_INFO("vta(set,meas):(%d,%d),ita(set,meas):(%d,%d)\n",
		 data->vta_setting, data->vta_measure, data->ita_setting,
		 data->ita_measure);
	return 0;
stop:
	__dv2_stop(info, &sinfo);
	return -EIO;
}

static inline void __dv2_calculate_vbat_ircmp(struct dv2_algo_info *info)
{
	int ret, ibat;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	u32 ircmp;

	if (!data->is_dvchg_en[DV2_DVCHG_MASTER]) {
		data->vbat_ircmp = 0;
		return;
	}

	ret = __dv2_get_adc(info, PCA_ADCCHAN_IBAT, &ibat, &ibat);
	if (ret < 0) {
		PCA_ERR("get ibat fail(%d)\n", ret);
		return;
	}
	ircmp = ibat * data->r_bat / 1000;
	/* If state is CC_CV, ircmp can only be smaller than previous one */
	if (data->state == DV2_ALGO_CC_CV)
		ircmp = MIN(data->vbat_ircmp, ircmp);
	data->vbat_ircmp = MIN(desc->ircmp_vclamp, ircmp);
	PCA_INFO("vbat_ircmp(vclamp,ibat,rbat)=%d(%d,%d,%d)\n",
		 data->vbat_ircmp, desc->ircmp_vclamp, ibat, data->r_bat);
}

static inline void __dv2_select_vbat_cv(struct dv2_algo_info *info)
{
	int ret;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	u32 cv = data->vbat_cv;
	u32 cv_no_ircmp = desc->vbat_cv;

	mutex_lock(&data->ext_lock);
	if (data->jeita_vbat_cv > 0)
		cv_no_ircmp = MIN(cv_no_ircmp, data->jeita_vbat_cv);

	if (cv_no_ircmp != data->vbat_cv_no_ircmp)
		data->vbat_cv_no_ircmp = cv_no_ircmp;

	cv = data->vbat_cv_no_ircmp + data->vbat_ircmp;
	if (cv == data->vbat_cv)
		goto out;

	/* VBATOVP ALARM */
	ret = prop_chgalgo_set_vbatovp_alarm(data->pca_dvchg[DV2_DVCHG_MASTER],
					     cv);
	if (ret < 0) {
		PCA_ERR("set vbatovp alarm fail(%d)\n", ret);
		goto out;
	}
	data->vbat_cv = cv;
	data->cv_lower_bound = data->vbat_cv - DV2_CV_LOWER_BOUND_GAP;
out:
	PCA_INFO("vbat_cv(org,jeita,no_ircmp,low_bound)=%d(%d,%d,%d,%d)\n",
		 data->vbat_cv, desc->vbat_cv, data->jeita_vbat_cv,
		 data->vbat_cv_no_ircmp, data->cv_lower_bound);
	mutex_unlock(&data->ext_lock);
}

/*
 * Select current limit according to severial status
 * If switching charger is charging, add AICR setting to ita
 * For now, the following features are taken into consider
 * 1. Resistence
 * 2. Phone's thermal throttling
 * 3. TA's power limit
 * 4. TA's temperature
 * 5. Battery's temperature
 * 6. Divider charger's temperature
 */
static inline int __dv2_get_ita_lmt(struct dv2_algo_info *info)
{
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	int ita = data->ita_lmt;

	mutex_lock(&data->ext_lock);
	if (data->thermal_throttling >= 0)
		ita = MIN(ita, data->thermal_throttling);
	if (data->ita_pwr_lmt > 0)
		ita = MIN(ita, data->ita_pwr_lmt);
	ita = MIN(ita, data->ita_lmt - desc->tta_curlmt[data->tta_level]);
	ita = MIN(ita, data->ita_lmt - desc->tbat_curlmt[data->tbat_level]);
	ita = MIN(ita, data->ita_lmt - desc->tdvchg_curlmt[data->tdvchg_level]);
	PCA_INFO("ita(org,tta,tbat,tdvchg,prlmt,throt)=%d(%d,%d,%d,%d,%d,%d)\n",
		 ita, data->ita_lmt, desc->tta_curlmt[data->tta_level],
		 desc->tbat_curlmt[data->tbat_level],
		 desc->tdvchg_curlmt[data->tdvchg_level], data->ita_pwr_lmt,
		 data->thermal_throttling);
	mutex_unlock(&data->ext_lock);
	return ita;
}

static inline int __dv2_get_idvchg_lmt(struct dv2_algo_info *info)
{
	u32 ita_lmt, idvchg_lmt;
	struct dv2_algo_data *data = info->data;

	ita_lmt = __dv2_get_ita_lmt(info);
	idvchg_lmt = MIN(data->idvchg_cc, ita_lmt);
	PCA_INFO("idvchg_lmt(ita_lmt,idvchg_cc)=%d(%d,%d)\n", idvchg_lmt,
		 ita_lmt, data->idvchg_cc);
	return idvchg_lmt;
}

/* Calculate VBUSOV S/W level */
static u32 __dv2_get_dvchg_vbusovp(struct dv2_algo_info *info, u32 ita)
{
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	u32 vout, ibat;

	ibat = 2 * (data->is_swchg_en ? (ita - data->aicr_setting) : ita);
	vout = desc->vbat_cv + ibat * data->r_sw / 1000;
	return MIN(DV2_VBUSOVP_RATIO * __dv2_vout2vbus(info, vout) / 100,
		   data->vbusovp);
}

/* Calculate IBUSOC S/W level */
static u32 __dv2_get_dvchg_ibusocp(struct dv2_algo_info *info, u32 ita)
{
	struct dv2_algo_data *data = info->data;
	u32 ibus;

	ibus = data->is_swchg_en ? (ita - data->aicr_setting) : ita;
	if (data->is_dvchg_en[DV2_DVCHG_SLAVE])
		return (DV2_IBUSOCP_RATIO + 10) * ibus / 100;
	return DV2_IBUSOCP_RATIO * ibus / 100;
}

/* Calculate VBATOV S/W level */
static u32 __dv2_get_vbatovp(struct dv2_algo_info *info)
{
	struct dv2_algo_desc *desc = info->desc;

	return DV2_VBATOVP_RATIO * (desc->vbat_cv + desc->ircmp_vclamp) / 100;
}

/* Calculate IBATOC S/W level */
static u32 __dv2_get_ibatocp(struct dv2_algo_info *info, u32 ita)
{
	struct dv2_algo_data *data = info->data;
	u32 ibat;

	ibat = 2 * (data->is_swchg_en ? ita - data->aicr_setting : ita);
	if (data->is_swchg_en)
		ibat += data->ichg_setting;
	return DV2_IBATOCP_RATIO * ibat / 100;
}

/* Calculate ITAOC S/W level */
static u32 __dv2_get_itaocp(struct dv2_algo_info *info)
{
	struct dv2_algo_data *data = info->data;

	return DV2_ITAOCP_RATIO * data->ita_setting / 100;
}

static int __dv2_set_dvchg_protection(struct dv2_algo_info *info, bool dual)
{
	int ret;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;
	u32 vout, idvchg_lmt;
	u32 vbusovp, ibusocp, vbatovp, ibatocp;

	/* VBATOVP ALARM */
	ret = prop_chgalgo_set_vbatovp_alarm(data->pca_dvchg[DV2_DVCHG_MASTER],
					     desc->vbat_cv);
	if (ret < 0) {
		PCA_ERR("set vbatovp alarm fail(%d)\n", ret);
		return ret;
	}

	/* VBUSOVP */
	vout = desc->vbat_cv + 2 * data->idvchg_cc * data->r_sw / 1000;
	vbusovp = DV2_VBUSOVP_RATIO * __dv2_vout2vbus(info, vout) / 100;
	vbusovp = MIN(vbusovp, auth_data->vcap_max);
	ret = prop_chgalgo_set_vbusovp(data->pca_dvchg[DV2_DVCHG_MASTER],
				       vbusovp);
	if (ret < 0) {
		PCA_ERR("set vbusovp fail(%d)\n", ret);
		return ret;
	}
	data->vbusovp = vbusovp;
	/* For TA CV mode, vbusovp alarm is not required */
	if (!auth_data->support_cc) {
		ret = prop_chgalgo_set_vbusovp_alarm(
			data->pca_dvchg[DV2_DVCHG_MASTER], vbusovp);
		if (ret < 0) {
			PCA_ERR("set vbusovp alarm fail(%d)\n", ret);
			return ret;
		}
	}

	/* IBUSOCP */
	idvchg_lmt = MIN(data->idvchg_cc, auth_data->ita_max);
	ibusocp = DV2_IBUSOCP_RATIO * idvchg_lmt / 100;
	if (data->pca_dvchg[DV2_DVCHG_SLAVE] && dual) {
		ibusocp = (DV2_IBUSOCP_RATIO + 10) * idvchg_lmt / 100;
		ibusocp /= 2;
		ret = prop_chgalgo_set_ibusocp(data->pca_dvchg[DV2_DVCHG_SLAVE],
					       ibusocp);
		if (ret < 0) {
			PCA_ERR("set slave ibusocp fail(%d)\n", ret);
			return ret;
		}
	}
	ret = prop_chgalgo_set_ibusocp(data->pca_dvchg[DV2_DVCHG_MASTER],
				       ibusocp);
	if (ret < 0) {
		PCA_ERR("set ibusocp fail(%d)\n", ret);
		return ret;
	}

	/* VBATOVP */
	vbatovp = DV2_VBATOVP_RATIO *
		  (desc->vbat_cv + desc->ircmp_vclamp) / 100;
	ret = prop_chgalgo_set_vbatovp(data->pca_dvchg[DV2_DVCHG_MASTER],
				       vbatovp);
	if (ret < 0) {
		PCA_ERR("set vbatovp fail(%d)\n", ret);
		return ret;
	}

	/* IBATOCP */
	ibatocp = 2 * data->idvchg_cc + desc->swchg_ichg;
	ibatocp = DV2_IBATOCP_RATIO * ibatocp / 100;
	ret = prop_chgalgo_set_ibatocp(data->pca_dvchg[DV2_DVCHG_MASTER],
				       ibatocp);
	if (ret < 0) {
		PCA_ERR("set ibatocp fail(%d)\n", ret);
		return ret;
	}
	PCA_INFO("vbusovp,ibusocp,vbatovp,ibatocp = (%d,%d,%d,%d)\n",
		 vbusovp, ibusocp, vbatovp, ibatocp);
	return 0;
}

/*
 * Enable/Disable divider charger
 *
 * @en: enable/disable
 */
static int __dv2_enable_dvchg_charging(struct dv2_algo_info *info,
				       enum dv2_dvchg_role role, bool en)
{
	int ret;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;

	if (data->is_dvchg_en[role] == en)
		return 0;
	ret = prop_chgalgo_enable_charging(data->pca_dvchg[role], en);
	if (ret < 0) {
		PCA_ERR("en chg fail(%d)\n", ret);
		return ret;
	}
	data->is_dvchg_en[role] = en;
	msleep(desc->ta_blanking);
	return 0;
}

/*
 * Set protection parameters, disable swchg and  enable divider charger
 *
 * @en: enable/disable
 */
static int __dv2_set_dvchg_charging(struct dv2_algo_info *info,
				    enum dv2_dvchg_role role, bool en)
{
	int ret;
	struct dv2_algo_data *data = info->data;

	if (!data->pca_dvchg[role])
		return -EINVAL;

	PCA_INFO("en[%s] = %d\n", __dv2_dvchg_role_name[role], en);

	if (en && role == DV2_DVCHG_MASTER) {
		ret = prop_chgalgo_enable_hz(data->pca_swchg, true);
		if (ret < 0) {
			PCA_ERR("set swchg hz fail(%d)\n", ret);
			return ret;
		}

		ret = __dv2_set_dvchg_protection(info, false);
		if (ret < 0) {
			PCA_ERR("set protection fail(%d)\n", ret);
			return ret;
		}
	}
	ret = __dv2_enable_dvchg_charging(info, role, en);
	if (ret < 0)
		return ret;
	if (!en && role == DV2_DVCHG_MASTER) {
		ret = prop_chgalgo_enable_hz(data->pca_swchg, false);
		if (ret < 0) {
			PCA_ERR("disable swchg hz fail(%d)\n", ret);
			return ret;
		}
	}
	return 0;
}

/*
 * Enable charging of switching charger
 * For divide by two algorithm, according to swchg_ichg to decide enable or not
 *
 * @en: enable/disable
 */
static int __dv2_enable_swchg_charging(struct dv2_algo_info *info, bool en)
{
	int ret;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;

	PCA_INFO("en = %d\n", en);
	if (en) {
		ret = prop_chgalgo_enable_charging(data->pca_swchg, true);
		if (ret < 0) {
			PCA_ERR("en swchg fail(%d)\n", ret);
			return ret;
		}
		ret = prop_chgalgo_enable_hz(data->pca_swchg, false);
		if (ret < 0) {
			PCA_ERR("disable hz fail(%d)\n", ret);
			return ret;
		}
	} else {
		ret = prop_chgalgo_enable_hz(data->pca_swchg, true);
		if (ret < 0) {
			PCA_ERR("disable hz fail(%d)\n", ret);
			return ret;
		}
		ret = prop_chgalgo_enable_charging(data->pca_swchg, false);
		if (ret < 0) {
			PCA_ERR("en swchg fail(%d)\n", ret);
			return ret;
		}
	}
	data->is_swchg_en = en;
	ret = prop_chgalgo_set_vbatovp_alarm(data->pca_dvchg[DV2_DVCHG_MASTER],
		en ? desc->swchg_off_vbat : desc->vbat_cv);
	if (ret < 0) {
		PCA_ERR("set vbatovp alarm fail(%d)\n", ret);
		return ret;
	}
	return 0;
}

/*
 * Set AICR & ICHG of switching charger
 *
 * @aicr: setting of AICR
 * @ichg: setting of ICHG
 */
static int __dv2_set_swchg_cap(struct dv2_algo_info *info, u32 aicr)
{
	int ret;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	u32 ichg, vbat, vbus;

	if (aicr == data->aicr_setting)
		goto set_ichg;
	ret = prop_chgalgo_set_aicr(data->pca_swchg, aicr);
	if (ret < 0) {
		PCA_ERR("set aicr fail(%d)\n", ret);
		return ret;
	}
	data->aicr_setting = aicr;
set_ichg:
	ret = __dv2_get_adc(info, PCA_ADCCHAN_VBAT, &vbat, &vbat);
	if (ret < 0) {
		PCA_ERR("get vbat fail(%d)\n", ret);
		return ret;
	}
	ret = __dv2_get_adc(info, PCA_ADCCHAN_VBUS, &vbus, &vbus);
	if (ret < 0) {
		PCA_ERR("get vbus fail(%d)\n", ret);
		return ret;
	}
	/* 90% charging efficiency */
	ichg = (90 * vbus * aicr / 100) / vbat;
	ichg = MIN(ichg, desc->swchg_ichg);
	if (ichg == data->ichg_setting)
		return 0;
	ret = prop_chgalgo_set_ichg(data->pca_swchg, ichg);
	if (ret < 0) {
		PCA_ERR("set_ichg fail(%d)\n", ret);
		return ret;
	}
	data->ichg_setting = ichg;
	PCA_INFO("AICR = %d, ICHG = %d\n", aicr, ichg);
	return 0;
}

/*
 * Enable TA by algo
 *
 * @en: enable/disable
 * @mV: requested output voltage
 * @mA: requested output current
 */
static int __dv2_enable_ta_charging(struct dv2_algo_info *info, bool en, int mV,
				    int mA)
{
	int ret;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	u32 wdt = MAX(desc->polling_interval * 2, DV2_TA_WDT_MIN);

	PCA_INFO("en = %d\n", en);
	if (en) {
		ret = prop_chgalgo_set_ta_wdt(data->pca_ta, wdt);
		if (ret < 0) {
			PCA_ERR("set ta wdt fail(%d)\n", ret);
			return ret;
		}
		ret = prop_chgalgo_enable_ta_wdt(data->pca_ta, true);
		if (ret < 0) {
			PCA_ERR("en ta wdt fail(%d)\n", ret);
			return ret;
		}
	}
	ret = prop_chgalgo_enable_ta_charging(data->pca_ta, en, mV, mA);
	if (ret < 0) {
		PCA_ERR("en ta charging fail(%d)\n", ret);
		return ret;
	}
	if (!en) {
		ret = prop_chgalgo_enable_ta_wdt(data->pca_ta, false);
		if (ret < 0)
			PCA_ERR("disable ta wdt fail(%d)\n", ret);
	}
	data->vta_setting = mV;
	data->ita_setting = mA;
	return ret;
}

/* Stop dv2 charging and reset parameter */
static int __dv2_stop(struct dv2_algo_info *info, struct dv2_stop_info *sinfo)
{
	struct dv2_algo_data *data = info->data;
	struct prop_chgalgo_notify notify = {
		.src = PCA_NOTISRC_ALGO,
		.evt = PCA_NOTIEVT_ALGO_STOP,
	};

	if (data->state == DV2_ALGO_STOP) {
		PCA_DBG("already stop\n");
		return 0;
	}

	PCA_INFO("reset ta(%d), hardreset ta(%d)\n", sinfo->reset_ta,
		 sinfo->hardreset_ta);
	data->state = DV2_ALGO_STOP;
	atomic_set(&data->stop_algo, 0);
	alarm_cancel(&data->timer);

	if (data->is_swchg_en)
		__dv2_enable_swchg_charging(info, false);
	__dv2_set_dvchg_charging(info, DV2_DVCHG_SLAVE, false);
	__dv2_set_dvchg_charging(info, DV2_DVCHG_MASTER, false);
	if (!(data->notify & DV2_RESET_NOTIFY)) {
		if (sinfo->hardreset_ta)
			prop_chgalgo_send_ta_hardreset(data->pca_ta);
		else if (sinfo->reset_ta) {
			prop_chgalgo_set_ta_cap(data->pca_ta, DV2_VTA_INIT,
						DV2_ITA_INIT);
			__dv2_enable_ta_charging(info, false, DV2_VTA_INIT,
						 DV2_ITA_INIT);
		}
	}
	__dv2_send_notification(info, PCA_NOTIEVT_ALGO_STOP, &notify);
	return 0;
}

static inline void __dv2_init_algo_data(struct dv2_algo_info *info)
{
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;
	u32 *rcable_level = desc->rcable_level;
	u32 *ita_level = desc->ita_level;

	data->ita_lmt = MIN(ita_level[DV2_RCABLE_NORMAL], auth_data->ita_max);
	data->idvchg_ss_init = MAX(data->idvchg_ss_init, auth_data->ita_min);
	data->idvchg_ss_init = MIN(data->idvchg_ss_init, data->ita_lmt);
	data->ita_pwr_lmt = 0;
	data->idvchg_cc = ita_level[DV2_RCABLE_NORMAL] - desc->swchg_aicr;
	data->idvchg_term = desc->idvchg_term;
	data->err_retry_cnt = 0;
	data->is_swchg_en = false;
	data->is_dvchg_en[DV2_DVCHG_MASTER] = false;
	data->is_dvchg_en[DV2_DVCHG_SLAVE] = false;
	data->suspect_ta_cc = false;
	data->aicr_setting = 0;
	data->ichg_setting = 0;
	data->vta_setting = DV2_VTA_INIT;
	data->ita_setting = DV2_ITA_INIT;
	data->ita_gap_per_vstep = 0;
	data->ita_gap_avg_cnt = 0;
	data->is_vbat_over_cv = false;
	data->ignore_ibusucpf = false;
	data->force_ta_cv = false;
	data->vbat_cv = desc->vbat_cv;
	data->vbat_cv_no_ircmp = desc->vbat_cv;
	data->cv_lower_bound = desc->vbat_cv - DV2_CV_LOWER_BOUND_GAP;
	data->vta_comp = 0;
	data->zcv = 0;
	data->r_bat = desc->ircmp_rbat;
	data->r_sw = desc->rsw_min;
	data->r_cable = rcable_level[DV2_RCABLE_NORMAL];
	data->tbat_level = DV2_THERMAL_NORMAL;
	data->tta_level = DV2_THERMAL_NORMAL;
	data->tdvchg_level = DV2_THERMAL_NORMAL;
	data->tswchg_level = DV2_THERMAL_NORMAL;
	data->run_once = true;
	data->state = DV2_ALGO_INIT;
	mutex_lock(&data->notify_lock);
	data->notify = 0;
	mutex_unlock(&data->notify_lock);
	get_monotonic_boottime(&data->stime);
}

static int __dv2_earily_restart(struct dv2_algo_info *info)
{
	int ret;
	struct dv2_algo_data *data = info->data;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;

	ret = __dv2_enable_dvchg_charging(info, DV2_DVCHG_SLAVE, false);
	if (ret < 0) {
		PCA_ERR("disable slave dvchg fail(%d)\n", ret);
		return ret;
	}
	ret = __dv2_enable_dvchg_charging(info, DV2_DVCHG_MASTER, false);
	if (ret < 0) {
		PCA_ERR("disable master dvchg fail(%d)\n", ret);
		return ret;
	}
	if (auth_data->support_cc) {
		ret = __dv2_set_ta_cap_cc(info, DV2_VTA_INIT, DV2_ITA_INIT);
		if (ret < 0) {
			PCA_ERR("set ta cap fail(%d)\n", ret);
			return ret;
		}
	}
	ret = __dv2_enable_ta_charging(info, false, DV2_VTA_INIT, DV2_ITA_INIT);
	if (ret < 0) {
		PCA_ERR("disable ta charging fail(%d)\n", ret);
		return ret;
	}
	__dv2_init_algo_data(info);
	return 0;
}

/*
 * Start dv2 algo timer and run algo
 * It cannot start algo again if algo has been started once before
 * Run once flag will be reset after plugging out TA
 */
static inline int __dv2_start(struct dv2_algo_info *info)
{
	int ret, ibus, vbat, vbus, ita, i;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;
	ktime_t ktime = ktime_set(0, MS_TO_NS(DV2_ALGO_INIT_POLLING_INTERVAL));

	PCA_DBG("++\n");

	if (data->run_once) {
		PCA_ERR("already run dv2 algo once\n");
		return -EINVAL;
	}

	data->idvchg_ss_init = desc->idvchg_ss_init;
	ret = prop_chgalgo_set_aicr(data->pca_swchg, 3000);
	if (ret < 0) {
		PCA_ERR("set aicr fail(%d)\n", ret);
		goto start;
	}
	ret = prop_chgalgo_set_ichg(data->pca_swchg, 3000);
	if (ret < 0) {
		PCA_ERR("set ichg fail(%d)\n", ret);
		goto start;
	}
	ret = prop_chgalgo_enable_charging(data->pca_swchg, true);
	if (ret < 0) {
		PCA_ERR("en swchg fail(%d)\n", ret);
		return ret;
	}
	msleep(1000);
	ret = prop_chgalgo_get_adc(data->pca_swchg, PCA_ADCCHAN_VBUS, &vbus,
				   &vbus);
	if (ret < 0) {
		PCA_ERR("get swchg vbus fail(%d)\n", ret);
		goto start;
	}
	ret = prop_chgalgo_get_adc(data->pca_swchg, PCA_ADCCHAN_IBUS, &ibus,
				   &ibus);
	if (ret < 0) {
		PCA_ERR("get swchg ibus fail(%d)\n", ret);
		goto start;
	}
	ret = prop_chgalgo_get_adc(data->pca_swchg, PCA_ADCCHAN_VBAT, &vbat,
				   &vbat);
	if (ret < 0) {
		PCA_ERR("get swchg vbat fail(%d)\n", ret);
		goto start;
	}
	ita = ((90 * vbus * ibus / 100) / vbat) / 2;
	if (ita < desc->idvchg_term) {
		PCA_ERR("estimated ita(%d) < idvchg_term(%d)\n", ita,
			desc->idvchg_term);
		return -EINVAL;
	}
	/* Update idvchg_ss_init */
	if (ita >= auth_data->ita_min) {
		PCA_INFO("set idvchg_ss_init(%d)->(%d)\n", desc->idvchg_ss_init,
			 ita);
		data->idvchg_ss_init = ita;
	}
start:
	/* disable charger */
	ret = prop_chgalgo_enable_charging(data->pca_swchg, false);
	if (ret < 0) {
		PCA_ERR("disable charger fail\n");
		return ret;
	}
	msleep(1000); /* wait for battery to recovery */

	/* Check DVCHG registers stat first */
	for (i = DV2_DVCHG_MASTER; i < DV2_DVCHG_MAX; i++) {
		if (!data->pca_dvchg[i])
			continue;
		ret = prop_chgalgo_init_chip(data->pca_dvchg[i]);
		if (ret < 0) {
			PCA_ERR("(%s) init chip fail(%d)\n",
				__dv2_dvchg_role_name[i], ret);
			return ret;
		}
	}

	/* Parameters that only reset by restarting from outside */
	mutex_lock(&data->ext_lock);
	data->thermal_throttling = -1;
	data->jeita_vbat_cv = -1;
	mutex_unlock(&data->ext_lock);
	data->tried_dual_dvchg = false;
	__dv2_init_algo_data(info);
	alarm_start_relative(&data->timer, ktime);
	return 0;
}

/* =================================================================== */
/* DV2 Algo State Machine                                              */
/* =================================================================== */

static int __dv2_algo_init_with_ta_cc(struct dv2_algo_info *info)
{
	int ret, i, vbus, vbat;
	int ita_avg = 0, vta_avg = 0, vbus_avg = 0, vbat_avg = 0;
	const int avg_times = 10;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	struct dv2_stop_info sinfo = {
		.hardreset_ta = false,
		.reset_ta = true,
	};

	PCA_DBG("++\n");

	/* Change charging policy first */
	ret = __dv2_enable_ta_charging(info, true, DV2_VTA_INIT, DV2_ITA_INIT);
	if (ret < 0) {
		PCA_ERR("enable ta direct charge fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto err;
	}

	ret = prop_chgalgo_enable_hz(data->pca_swchg, true);
	if (ret < 0) {
		PCA_ERR("set swchg hz fail(%d)\n", ret);
		goto err;
	}
	msleep(500); /* Wait current stable */

	/* Initial setting, no need to check ita_lmt */
	ret = __dv2_set_ta_cap_cc_by_cali_vta(info, data->idvchg_ss_init);
	if (ret < 0) {
		PCA_ERR("set ta cap by algo fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto err;
	}

	for (i = 0; i < avg_times; i++) {
		ret = __dv2_get_ta_cap(info);
		if (ret < 0) {
			PCA_ERR("get ta cap fail(%d)\n", ret);
			sinfo.hardreset_ta = true;
			goto err;
		}
		ret = __dv2_get_adc(info, PCA_ADCCHAN_VBUS, &vbus, &vbus);
		if (ret < 0) {
			PCA_ERR("get vbus fail(%d)\n", ret);
			goto err;
		}
		ret = __dv2_get_adc(info, PCA_ADCCHAN_VBAT, &vbat, &vbat);
		if (ret < 0) {
			PCA_ERR("get vbus fail(%d)\n", ret);
			goto err;
		}
		ita_avg += data->ita_measure;
		vta_avg += data->vta_measure;
		vbat_avg += vbat;
		vbus_avg += vbus;
	}
	ita_avg /= avg_times;
	vta_avg /= avg_times;
	vbus_avg /= avg_times;
	vbat_avg /= avg_times;
	data->zcv = vbat_avg;

	if (vbat_avg > desc->start_vbat_max) {
		PCA_INFO("finish dv2, vbat(%d) > %d\n", vbat_avg,
			 desc->start_vbat_max);
		goto out;
	}

	/* vbus calibration: voltage difference between TA & device */
	data->vbus_cali = vta_avg - vbus_avg;
	PCA_INFO("avg(ita,vta,vbus):(%d, %d, %d), vbus cali:%d\n", ita_avg,
		 vta_avg, vbus_avg, data->vbus_cali);
	if (abs(data->vbus_cali) > DV2_VBUS_CALI_THRESHOLD) {
		PCA_ERR("vbus cali (%d) > (%d)\n", data->vbus_cali,
			DV2_VBUS_CALI_THRESHOLD);
		goto err;
	}
	if (ita_avg > desc->ifod_threshold) {
		PCA_ERR("foreign object detected, ita(%d) > (%d)\n",
			ita_avg, desc->ifod_threshold);
		goto err;
	}

	ret = __dv2_set_dvchg_charging(info, DV2_DVCHG_MASTER, true);
	if (ret < 0) {
		PCA_ERR("en dvchg fail\n");
		goto err;
	}

	ret = __dv2_set_ta_cap_cc_by_cali_vta(info, data->idvchg_ss_init);
	if (ret < 0) {
		PCA_ERR("set ta cap by algo fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto err;
	}
	data->err_retry_cnt = 0;
	data->state = DV2_ALGO_MEASURE_R;
	return 0;

err:
	if (data->err_retry_cnt < DV2_ALGO_INIT_RETRY_MAX) {
		data->err_retry_cnt++;
		return 0;
	}
out:
	return __dv2_stop(info, &sinfo);
}

static int __dv2_algo_init_with_ta_cv(struct dv2_algo_info *info)
{
	int ret, i, vbus, vbat, vout;
	int ita_avg = 0, vta_avg = 0, vbus_avg = 0, vbat_avg = 0;
	bool err;
	u32 vta;
	const int avg_times = 10;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;
	struct dv2_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	PCA_DBG("++\n");

	/* Change charging policy first */
	ret = __dv2_enable_ta_charging(info, true, DV2_VTA_INIT, DV2_ITA_INIT);
	if (ret < 0) {
		PCA_ERR("enable ta direct charge fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto err;
	}

	ret = prop_chgalgo_enable_hz(data->pca_swchg, true);
	if (ret < 0) {
		PCA_ERR("set swchg hz fail(%d)\n", ret);
		goto err;
	}
	msleep(500); /* Wait current stable */

	ret = __dv2_get_adc(info, PCA_ADCCHAN_VBUS, &vbus, &vbus);
	if (ret < 0) {
		PCA_ERR("get vbus fail\n");
		goto err;
	}
	ret = prop_chgalgo_sync_ta_volt(data->pca_ta, vbus);
	if (ret < 0 && ret != -ENOTSUPP) {
		PCA_ERR("sync ta setting fail\n");
		goto err;
	}
	ret = __dv2_get_adc(info, PCA_ADCCHAN_VOUT, &vout, &vout);
	if (ret < 0) {
		PCA_ERR("get vout fail\n");
		goto err;
	}

	/* Adjust VBUS to make sure DVCHG can be turned on */
	vta = __dv2_vout2vbus(info, vout);
	ret = __dv2_set_ta_cap_cv(info, vta, data->idvchg_ss_init);
	if (ret < 0) {
		PCA_ERR("set ta cap fail(%d)\n", ret);
		goto err;
	}
	while (true) {
		ret = prop_chgalgo_is_vbuslowerr(
			data->pca_dvchg[DV2_DVCHG_MASTER], &err);
		if (ret < 0) {
			PCA_ERR("get vbuslowerr fail(%d)\n", ret);
			goto err;
		}
		if (!err)
			break;
		vta = data->vta_setting + auth_data->vta_step;
		ret = __dv2_set_ta_cap_cv(info, vta, data->idvchg_ss_init);
		if (ret < 0) {
			PCA_ERR("set ta cap fail(%d)\n", ret);
			goto err;
		}
	}

	for (i = 0; i < avg_times; i++) {
		if (auth_data->support_meas_cap) {
			ret = __dv2_get_ta_cap(info);
			if (ret < 0) {
				PCA_ERR("get ta cap fail(%d)\n", ret);
				sinfo.hardreset_ta = true;
				goto err;
			}
			ita_avg += data->ita_measure;
			vta_avg += data->vta_measure;
			ret = __dv2_get_adc(info, PCA_ADCCHAN_VBUS, &vbus,
					    &vbus);
			if (ret < 0) {
				PCA_ERR("get vbus fail(%d)\n", ret);
				goto err;
			}
			vbus_avg += vbus;
		}
		ret = __dv2_get_adc(info, PCA_ADCCHAN_VBAT, &vbat, &vbat);
		if (ret < 0) {
			PCA_ERR("get vbus fail(%d)\n", ret);
			goto err;
		}
		vbat_avg += vbat;
	}
	if (auth_data->support_meas_cap) {
		ita_avg /= avg_times;
		vta_avg /= avg_times;
		vbus_avg /= avg_times;
	}
	vbat_avg /= avg_times;
	data->zcv = vbat_avg;

	if (vbat_avg >= desc->start_vbat_max) {
		PCA_INFO("finish dv2, vbat(%d) > %d\n", vbat_avg,
			 desc->start_vbat_max);
		goto out;
	}

	if (auth_data->support_meas_cap) {
		/* vbus calibration: voltage difference between TA & device */
		data->vbus_cali = vta_avg - vbus_avg;
		PCA_INFO("avg(ita,vta,vbus):(%d,%d,%d), vbus_cali:%d\n",
			 ita_avg, vta_avg, vbus_avg, data->vbus_cali);
		if (abs(data->vbus_cali) > DV2_VBUS_CALI_THRESHOLD) {
			PCA_ERR("vbus cali (%d) > (%d)\n", data->vbus_cali,
				DV2_VBUS_CALI_THRESHOLD);
			goto err;
		}
		if (ita_avg > desc->ifod_threshold) {
			PCA_ERR("foreign object detected, ita(%d) > (%d)\n",
				ita_avg, desc->ifod_threshold);
			goto err;
		}
	}
	PCA_INFO("avg(vbat):(%d)\n", vbat_avg);

	ret = __dv2_set_dvchg_charging(info, DV2_DVCHG_MASTER, true);
	if (ret < 0) {
		PCA_ERR("en dvchg fail\n");
		goto err;
	}

	/* Get ita measure after enable dvchg */
	ret = __dv2_get_ta_cap_by_supportive(info, &data->vta_measure,
					     &data->ita_measure);
	if (ret < 0) {
		PCA_ERR("get ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = auth_data->support_meas_cap;
		goto out;
	}
	data->err_retry_cnt = 0;
	data->state = DV2_ALGO_MEASURE_R;
	return 0;
err:
	if (data->err_retry_cnt < DV2_ALGO_INIT_RETRY_MAX) {
		data->err_retry_cnt++;
		return 0;
	}
out:
	return __dv2_stop(info, &sinfo);
}

/*
 * DV2 algorithm initial state
 * It does Foreign Object Detection(FOD)
 */
static int __dv2_algo_init(struct dv2_algo_info *info)
{
	struct dv2_algo_data *data = info->data;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;

	return auth_data->support_cc ? __dv2_algo_init_with_ta_cc(info) :
				       __dv2_algo_init_with_ta_cv(info);
}

struct meas_r_info {
	u32 vbus;
	u32 ibus;
	u32 vbat;
	u32 ibat;
	u32 vout;
	u32 vta;
	u32 ita;
	u32 r_cable;
	u32 r_bat;
	u32 r_sw;
};

static int __dv2_algo_get_r_info(struct dv2_algo_info *info,
				 struct meas_r_info *r_info,
				 struct dv2_stop_info *sinfo)
{
	int ret;
	struct dv2_algo_data *data = info->data;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;

	memset(r_info, 0, sizeof(struct meas_r_info));
	if (auth_data->support_meas_cap) {
		ret = __dv2_get_ta_cap(info);
		if (ret < 0) {
			PCA_ERR("get ta cap fail(%d)\n", ret);
			sinfo->hardreset_ta = true;
			return ret;
		}
		r_info->ita = data->ita_measure;
		r_info->vta = data->vta_measure;
	}
	ret = __dv2_get_adc(info, PCA_ADCCHAN_VBUS, &r_info->vbus,
			    &r_info->vbus);
	if (ret < 0) {
		PCA_ERR("get vbus fail(%d)\n", ret);
		return ret;
	}
	ret = __dv2_get_adc(info, PCA_ADCCHAN_IBUS, &r_info->ibus,
			    &r_info->ibus);
	if (ret < 0) {
		PCA_ERR("get ibus fail(%d)\n", ret);
		return ret;
	}
	ret = __dv2_get_adc(info, PCA_ADCCHAN_VOUT, &r_info->vout,
			    &r_info->vout);
	if (ret < 0) {
		PCA_ERR("get vout fail(%d)\n", ret);
		return ret;
	}
	ret = __dv2_get_adc(info, PCA_ADCCHAN_VBAT, &r_info->vbat,
			    &r_info->vbat);
	if (ret < 0) {
		PCA_ERR("get vbat fail(%d)\n", ret);
		return ret;
	}
	ret = __dv2_get_adc(info, PCA_ADCCHAN_IBAT, &r_info->ibat,
			    &r_info->ibat);
	if (ret < 0) {
		PCA_ERR("get ibat fail(%d)\n", ret);
		return ret;
	}
	PCA_DBG("vta:%d,ita:%d,vbus:%d,ibus:%d,vout:%d,vbat:%d,ibat:%d\n",
		r_info->vta, r_info->ita, r_info->vbus, r_info->ibus,
		r_info->vout, r_info->vbat, r_info->ibat);
	return 0;
}

static int __dv2_algo_cal_r_info_with_ta_cap(struct dv2_algo_info *info,
					     struct dv2_stop_info *sinfo)
{
	int ret, i;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	struct meas_r_info r_info, max_r_info, min_r_info;
	struct dv2_stop_info _sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	memset(&max_r_info, 0, sizeof(struct meas_r_info));
	memset(&min_r_info, 0, sizeof(struct meas_r_info));
	data->r_bat = data->r_sw = data->r_cable = 0;
	for (i = 0; i < DV2_ALGO_MEASURE_R_AVG_TIMES + 2; i++) {
		if (atomic_read(&data->stop_algo)) {
			PCA_INFO("stop algo\n");
			goto stop;
		}
		ret = __dv2_algo_get_r_info(info, &r_info, sinfo);
		if (ret < 0) {
			PCA_ERR("get r info fail(%d)\n", ret);
			return ret;
		}
		if (r_info.ibat == 0) {
			PCA_ERR("ibat == 0 fail\n");
			return -EINVAL;
		}
		if (r_info.ita == 0) {
			PCA_ERR("ita == 0 fail\n");
			sinfo->hardreset_ta = true;
			return -EINVAL;
		}
		if (r_info.ita < data->idvchg_term &&
		    r_info.vbat >= data->vbat_cv) {
			PCA_INFO("finish dv2 charging\n");
			return -EINVAL;
		}

		/* Use absolute instead of relative calculation */
		r_info.r_bat = abs(r_info.vbat - data->zcv) * 1000 /
			       abs(r_info.ibat);
		if (r_info.r_bat > desc->ircmp_rbat)
			r_info.r_bat = desc->ircmp_rbat;

		r_info.r_sw = abs(r_info.vout - r_info.vbat) * 1000 /
			      abs(r_info.ibat);
		if (r_info.r_sw < desc->rsw_min)
			r_info.r_sw = desc->rsw_min;

		r_info.r_cable = abs(r_info.vta - data->vbus_cali -
				 r_info.vbus) * 1000 / abs(r_info.ita);

		PCA_INFO("r_sw:%d, r_bat:%d, r_cable:%d\n", r_info.r_sw,
			 r_info.r_bat, r_info.r_cable);

		if (i == 0) {
			memcpy(&max_r_info, &r_info,
			       sizeof(struct meas_r_info));
			memcpy(&min_r_info, &r_info,
			       sizeof(struct meas_r_info));
		} else {
			max_r_info.r_bat = MAX(max_r_info.r_bat, r_info.r_bat);
			max_r_info.r_sw = MAX(max_r_info.r_sw, r_info.r_sw);
			max_r_info.r_cable = MAX(max_r_info.r_cable,
						 r_info.r_cable);
			min_r_info.r_bat = MIN(min_r_info.r_bat, r_info.r_bat);
			min_r_info.r_sw = MIN(min_r_info.r_sw, r_info.r_sw);
			min_r_info.r_cable = MIN(min_r_info.r_cable,
						 r_info.r_cable);
		}
		data->r_bat += r_info.r_bat;
		data->r_sw += r_info.r_sw;
		data->r_cable += r_info.r_cable;
	}
	data->r_bat -= (max_r_info.r_bat + min_r_info.r_bat);
	data->r_sw -= (max_r_info.r_sw + min_r_info.r_sw);
	data->r_cable -= (max_r_info.r_cable + min_r_info.r_cable);
	data->r_bat /= DV2_ALGO_MEASURE_R_AVG_TIMES;
	data->r_sw /= DV2_ALGO_MEASURE_R_AVG_TIMES;
	data->r_cable /= DV2_ALGO_MEASURE_R_AVG_TIMES;
	data->r_total = data->r_bat + data->r_sw + data->r_cable;
	return 0;
stop:
	__dv2_stop(info, &_sinfo);
	return -EIO;
}

static int __dv2_select_ita_lmt_by_r(struct dv2_algo_info *info, bool dual)
{
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;
	u32 ita_lmt_by_r, ita_lmt;
	u32 *rcable_level = dual ? desc->rcable_level_dual : desc->rcable_level;
	u32 *ita_level = dual ? desc->ita_level_dual : desc->ita_level;

	if (!auth_data->support_meas_cap) {
		ita_lmt_by_r = ita_level[DV2_RCABLE_NORMAL];
		goto out;
	}
	if (data->r_cable <= rcable_level[DV2_RCABLE_NORMAL])
		ita_lmt_by_r = ita_level[DV2_RCABLE_NORMAL];
	else if (data->r_cable <= rcable_level[DV2_RCABLE_BAD1])
		ita_lmt_by_r = ita_level[DV2_RCABLE_BAD1];
	else if (data->r_cable <= rcable_level[DV2_RCABLE_BAD2])
		ita_lmt_by_r = ita_level[DV2_RCABLE_BAD2];
	else if (data->r_cable <= rcable_level[DV2_RCABLE_BAD3])
		ita_lmt_by_r = ita_level[DV2_RCABLE_BAD3];
	else {
		PCA_ERR("r_cable(%d) too worse\n", data->r_cable);
		return -EINVAL;
	}
out:
	PCA_INFO("ita limited by r = %d\n", ita_lmt_by_r);
	data->ita_lmt = MIN(ita_lmt_by_r, auth_data->ita_max);
	data->ita_pwr_lmt = __dv2_get_ita_pwr_lmt_by_vta(info,
							 data->vta_setting);
	ita_lmt = __dv2_get_ita_lmt(info);
	if (ita_lmt < data->idvchg_term) {
		PCA_ERR("ita_lmt(%d) < dvchg_term(%d)\n", ita_lmt,
			data->idvchg_term);
		return -EINVAL;
	}
	return 0;
}

static int __dv2_algo_measure_r_with_ta_cc(struct dv2_algo_info *info)
{
	int ret;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	u32 ita, idvchg_lmt;
	u32 rcable_retry_level = (data->pca_dvchg[DV2_DVCHG_SLAVE] &&
				  !data->tried_dual_dvchg) ?
				 desc->rcable_level_dual[DV2_RCABLE_NORMAL] :
				 desc->rcable_level[DV2_RCABLE_NORMAL];
	struct dv2_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	PCA_DBG("++\n");

	ret = __dv2_algo_cal_r_info_with_ta_cap(info, &sinfo);
	if (ret < 0)
		goto err;
	if (data->r_cable > rcable_retry_level &&
	    data->err_retry_cnt < DV2_ALGO_MEASURE_R_RETRY_MAX) {
		PCA_INFO("rcable(%d) is worse than normal\n", data->r_cable);
		goto err;
	}
	PCA_ERR("avg_r(sw,bat,cable):(%d,%d,%d), r_total:%d\n",
		 data->r_sw, data->r_bat, data->r_cable, data->r_total);

	/* If haven't tried dual dvchg, try it once */
	if (data->pca_dvchg[DV2_DVCHG_SLAVE] && !data->tried_dual_dvchg &&
	    !data->is_dvchg_en[DV2_DVCHG_SLAVE]) {
		PCA_INFO("try dual dvchg\n");
		data->tried_dual_dvchg = true;
		data->idvchg_term = 2 * desc->idvchg_term;
		data->idvchg_cc = desc->ita_level_dual[DV2_RCABLE_NORMAL] -
				  desc->swchg_aicr;
		ret = __dv2_select_ita_lmt_by_r(info, true);
		if (ret < 0) {
			PCA_ERR("select dual dvchg ita lmt fail(%d)\n", ret);
			goto single_dvchg_select_ita;
		}
		/* Turn on slave dvchg if idvchg_lmt >= 2 * idvchg_term */
		idvchg_lmt = __dv2_get_idvchg_lmt(info);
		if (idvchg_lmt < data->idvchg_term) {
			PCA_ERR("idvchg_lmt(%d) < 2 * idvchg_term(%d)\n",
				idvchg_lmt, data->idvchg_term);
			goto single_dvchg_select_ita;
		}
		ret = __dv2_enable_dvchg_charging(info, DV2_DVCHG_MASTER,
						  false);
		if (ret < 0) {
			PCA_ERR("disable master dvchg fail(%d)\n", ret);
			goto single_dvchg_restart;
		}
		data->ignore_ibusucpf = true;
		ret = __dv2_set_dvchg_protection(info, true);
		if (ret < 0) {
			PCA_ERR("set dual dvchg protection fail(%d)\n", ret);
			goto single_dvchg_restart;
		}
		ret = __dv2_enable_dvchg_charging(info, DV2_DVCHG_SLAVE, true);
		if (ret < 0) {
			PCA_ERR("en slave dvchg fail(%d)\n", ret);
			goto single_dvchg_restart;
		}
		ita = MAX(data->idvchg_term, data->ita_setting);
		ita = MIN(ita, idvchg_lmt);
		ret = __dv2_set_ta_cap_cc_by_cali_vta(info, ita);
		if (ret < 0) {
			PCA_ERR("set ta cap fail(%d)\n", ret);
			sinfo.hardreset_ta = true;
			goto out;
		}
		ret = __dv2_enable_dvchg_charging(info, DV2_DVCHG_MASTER, true);
		if (ret < 0) {
			PCA_ERR("en master dvchg fail(%d)\n", ret);
			goto single_dvchg_restart;
		}
		goto ss_dvchg;
single_dvchg_restart:
		ret = __dv2_earily_restart(info);
		if (ret < 0) {
			PCA_ERR("earily restart fail(%d)\n", ret);
			goto out;
		}
		return 0;
	}
single_dvchg_select_ita:
	data->idvchg_term = desc->idvchg_term;
	data->idvchg_cc = desc->ita_level[DV2_RCABLE_NORMAL] - desc->swchg_aicr;
	ret = __dv2_select_ita_lmt_by_r(info, false);
	if (ret < 0) {
		PCA_ERR("select dvchg ita lmt fail(%d)\n", ret);
		goto out;
	}
ss_dvchg:
	data->err_retry_cnt = 0;
	data->state = DV2_ALGO_SS_DVCHG;
	return 0;
err:
	if (data->err_retry_cnt < DV2_ALGO_MEASURE_R_RETRY_MAX) {
		data->err_retry_cnt++;
		return 0;
	}
out:
	return __dv2_stop(info, &sinfo);
}

static int __dv2_algo_measure_r_with_ta_cv(struct dv2_algo_info *info)
{
	int ret;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;
	u32 rcable_retry_level = (data->pca_dvchg[DV2_DVCHG_SLAVE] &&
				  !data->tried_dual_dvchg) ?
				 desc->rcable_level_dual[DV2_RCABLE_NORMAL] :
				 desc->rcable_level[DV2_RCABLE_NORMAL];
	struct dv2_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	PCA_DBG("++\n");

	/*
	 * Ignore measuring r,
	 * treat as normal cable if meas_cap is not supported
	 */
	if (!auth_data->support_meas_cap) {
		PCA_INFO("ignore measuring resistance\n");
		goto select_ita;
	}
	ret = __dv2_algo_cal_r_info_with_ta_cap(info, &sinfo);
	if (ret < 0) {
		PCA_ERR("get r info fail(%d)\n", ret);
		goto err;
	}
	if (data->r_cable > rcable_retry_level &&
	    data->err_retry_cnt < DV2_ALGO_MEASURE_R_RETRY_MAX) {
		PCA_INFO("rcable(%d) is worse than normal\n", data->r_cable);
		goto err;
	}
	PCA_ERR("avg_r(sw,bat,cable):(%d,%d,%d), r_total:%d\n",
		 data->r_sw, data->r_bat, data->r_cable, data->r_total);
select_ita:
	ret = __dv2_select_ita_lmt_by_r(info, false);
	if (ret < 0) {
		PCA_ERR("select dvchg ita lmt fail(%d)\n", ret);
		goto out;
	}
	data->err_retry_cnt = 0;
	data->state = DV2_ALGO_SS_DVCHG;
	return 0;
err:
	if (data->err_retry_cnt < DV2_ALGO_MEASURE_R_RETRY_MAX) {
		data->err_retry_cnt++;
		return 0;
	}
out:
	return __dv2_stop(info, &sinfo);
}

/* Measure resistance of cable/battery/sw and get corressponding ita limit */
static int __dv2_algo_measure_r(struct dv2_algo_info *info)
{
	struct dv2_algo_data *data = info->data;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;

	return auth_data->support_cc ? __dv2_algo_measure_r_with_ta_cc(info) :
				       __dv2_algo_measure_r_with_ta_cv(info);
}

static int __dv2_check_slave_dvchg_off(struct dv2_algo_info *info)
{
	int ret;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;

	data->idvchg_cc = desc->ita_level[DV2_RCABLE_NORMAL] - desc->swchg_aicr;
	data->idvchg_term = desc->idvchg_term;
	ret = __dv2_enable_dvchg_charging(info, DV2_DVCHG_SLAVE, false);
	if (ret < 0) {
		PCA_ERR("disable slave dvchg fail(%d)\n", ret);
		return ret;
	}
	ret = __dv2_select_ita_lmt_by_r(info, false);
	if (ret < 0) {
		PCA_ERR("select dvchg ita lmt fail(%d)\n", ret);
		return ret;
	}
	ret = __dv2_set_dvchg_protection(info, false);
	if (ret < 0) {
		PCA_ERR("dvchg protection fail(%d)\n", ret);
		return ret;
	}
	return 0;
}

static int __dv2_check_force_ta_cv(struct dv2_algo_info *info,
				   struct dv2_stop_info *sinfo)
{
	int ret;
	u32 vbat, vta, ita;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;

	ret = __dv2_get_adc(info, PCA_ADCCHAN_VBAT, &vbat, &vbat);
	if (ret < 0) {
		PCA_ERR("get vbat fail(%d)\n", ret);
		return ret;
	}

	if (desc->force_ta_cv_vbat != 0 && vbat >= desc->force_ta_cv_vbat &&
	    !data->is_swchg_en) {
		ret = prop_chgalgo_set_vbusovp_alarm(
			data->pca_dvchg[DV2_DVCHG_MASTER], data->vbusovp);
		if (ret < 0) {
			PCA_ERR("set vbusovp alarm fail(%d)\n", ret);
			return ret;
		}
		ret = __dv2_get_ta_cap(info);
		if (ret < 0) {
			PCA_ERR("get ta cap fail\n");
			sinfo->hardreset_ta = true;
			return ret;
		}
		ita = MIN(data->ita_measure, data->ita_setting);
		vta = MIN(data->vta_measure, auth_data->vcap_max);
		ret = __dv2_set_ta_cap_cv(info, vta, ita);
		if (ret < 0) {
			PCA_ERR("set ta cap fail\n");
			return ret;
		}
		data->force_ta_cv = true;
	}
	return 0;
}

static int __dv2_algo_ss_dvchg_with_ta_cc(struct dv2_algo_info *info)
{
	int ret, vbat;
	u32 ita, ita_lmt, idvchg_lmt;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	struct dv2_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	PCA_DBG("++\n");

	ret = __dv2_check_force_ta_cv(info, &sinfo);
	if (ret < 0) {
		PCA_ERR("check force ta cv fail(%d)\n", ret);
		goto err;
	}
	if (data->force_ta_cv) {
		PCA_INFO("force switching to ta cv mode\n");
		return 0;
	}

	ret = __dv2_get_ta_cap(info);
	if (ret < 0) {
		PCA_ERR("get ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto err;
	}
	ret = __dv2_get_adc(info, PCA_ADCCHAN_VBAT, &vbat, &vbat);
	if (ret < 0) {
		PCA_ERR("get vbat fail(%d)\n", ret);
		goto err;
	}

	idvchg_lmt = __dv2_get_idvchg_lmt(info);
	if (idvchg_lmt < data->idvchg_term) {
		PCA_INFO("idvchg_lmt(%d) < idvchg_term(%d)\n", idvchg_lmt,
			 data->idvchg_term);
		goto err;
	}

	/* VBAT reaches CV level */
	if (vbat >= data->vbat_cv) {
		if (data->ita_measure < data->idvchg_term) {
			if (data->is_dvchg_en[DV2_DVCHG_SLAVE]) {
				ret = __dv2_check_slave_dvchg_off(info);
				if (ret < 0) {
					PCA_INFO("slave off fail(%d)\n", ret);
					goto err;
				}
				idvchg_lmt = __dv2_get_idvchg_lmt(info);
				goto cc_cv;
			}
			PCA_INFO("finish dv2 charging, vbat(%d), ita(%d)\n",
				 vbat, data->ita_measure);
			goto err;
		}
cc_cv:
		ita = MIN(data->ita_setting - desc->idvchg_ss_step, idvchg_lmt);
		data->state = DV2_ALGO_CC_CV;
		goto out_set_cap;
	}

	/* ITA reaches CC level */
	if (data->ita_measure >= idvchg_lmt ||
	    data->ita_setting >= idvchg_lmt) {
		/*
		 * Turn on switching charger only if divider charger
		 * is charging by it's maximum setting current
		 */
		ita_lmt = __dv2_get_ita_lmt(info);
		if (vbat < desc->swchg_off_vbat && desc->swchg_aicr > 0 &&
		    desc->swchg_ichg > 0 &&
		    (ita_lmt > data->idvchg_cc + desc->swchg_aicr_ss_init)) {
			ret = __dv2_set_swchg_cap(info,
						  desc->swchg_aicr_ss_init);
			if (ret < 0) {
				PCA_ERR("set swchg cap fail(%d)\n", ret);
				goto err;
			}
			ret = __dv2_enable_swchg_charging(info, true);
			if (ret < 0) {
				PCA_ERR("en swchg fail(%d)\n", ret);
				goto err;
			}
			ita = idvchg_lmt + desc->swchg_aicr_ss_init;
			data->aicr_init_lmt = ita_lmt - data->idvchg_cc;
			data->aicr_lmt = data->aicr_init_lmt;
			data->state = DV2_ALGO_SS_SWCHG;
			goto out_set_cap;
		}
		data->state = DV2_ALGO_CC_CV;
		ita = idvchg_lmt;
		goto out_set_cap;
	}

	/* Increase ita according to vbat level */
	if (vbat < desc->idvchg_ss_step1_vbat)
		ita = data->ita_setting + desc->idvchg_ss_step;
	else if (vbat < desc->idvchg_ss_step2_vbat)
		ita = data->ita_setting + desc->idvchg_ss_step1;
	else
		ita = data->ita_setting + desc->idvchg_ss_step2;
	ita = MIN(ita, idvchg_lmt);

out_set_cap:
	ret = __dv2_set_ta_cap_cc_by_cali_vta(info, ita);
	if (ret < 0) {
		PCA_ERR("set ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto err;
	}
	return 0;
err:
	return __dv2_stop(info, &sinfo);
}

static int __dv2_algo_ss_dvchg_with_ta_cv(struct dv2_algo_info *info)
{
	int ret, vbat;
	ktime_t start_time, end_time;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;
	u32 idvchg_lmt, vta = data->vta_setting, ita, delta_time;
	u32 ita_gap_per_vstep = data->ita_gap_per_vstep > 0 ?
				data->ita_gap_per_vstep :
				auth_data->ita_gap_per_vstep;
	struct dv2_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

repeat:
	PCA_DBG("++\n");
	start_time = ktime_get();
	ret = __dv2_get_adc(info, PCA_ADCCHAN_VBAT, &vbat, &vbat);
	if (ret < 0) {
		PCA_ERR("get vbat fail(%d)\n", ret);
		goto out;
	}
	ret = __dv2_get_ta_cap_by_supportive(info, &data->vta_measure,
					     &data->ita_measure);
	if (ret < 0) {
		PCA_ERR("get ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = auth_data->support_meas_cap;
		goto out;
	}

	/* Turn on slave dvchg if idvchg_lmt >= 2 * idvchg_term */
	ita = data->idvchg_term * 2;
	if (data->pca_dvchg[DV2_DVCHG_SLAVE] && !data->tried_dual_dvchg &&
	    !data->is_dvchg_en[DV2_DVCHG_SLAVE] && (data->ita_measure >= ita)) {
		PCA_INFO("try dual dvchg\n");
		data->tried_dual_dvchg = true;
		data->idvchg_term = 2 * desc->idvchg_term;
		data->idvchg_cc = desc->ita_level_dual[DV2_RCABLE_NORMAL] -
				  desc->swchg_aicr;
		ret = __dv2_select_ita_lmt_by_r(info, true);
		if (ret < 0) {
			PCA_ERR("select dual dvchg ita lmt fail(%d)\n", ret);
			goto single_dvchg_select_ita;
		}
		ret = __dv2_enable_dvchg_charging(info, DV2_DVCHG_MASTER,
						  false);
		if (ret < 0) {
			PCA_ERR("disable master dvchg fail(%d)\n", ret);
			goto single_dvchg_restart;
		}
		data->ignore_ibusucpf = true;
		ret = __dv2_set_dvchg_protection(info, true);
		if (ret < 0) {
			PCA_ERR("set dual dvchg protection fail(%d)\n", ret);
			goto single_dvchg_restart;
		}
		ret = __dv2_enable_dvchg_charging(info, DV2_DVCHG_SLAVE, true);
		if (ret < 0) {
			PCA_ERR("en slave dvchg fail(%d)\n", ret);
			goto single_dvchg_restart;
		}
		ret = __dv2_enable_dvchg_charging(info, DV2_DVCHG_MASTER, true);
		if (ret < 0) {
			PCA_ERR("en master dvchg fail(%d)\n", ret);
			goto single_dvchg_restart;
		}
		goto ss_dvchg;
single_dvchg_restart:
		ret = __dv2_earily_restart(info);
		if (ret < 0) {
			PCA_ERR("earily restart fail(%d)\n", ret);
			goto out;
		}
		return 0;
single_dvchg_select_ita:
		data->idvchg_term = desc->idvchg_term;
		data->idvchg_cc = desc->ita_level[DV2_RCABLE_NORMAL] -
				  desc->swchg_aicr;
		ret = __dv2_select_ita_lmt_by_r(info, false);
		if (ret < 0) {
			PCA_ERR("select dvchg ita lmt fail(%d)\n", ret);
			goto out;
		}
	}

ss_dvchg:
	ita = data->ita_setting;
	idvchg_lmt = __dv2_get_idvchg_lmt(info);
	if (idvchg_lmt < data->idvchg_term) {
		PCA_INFO("idvchg_lmt(%d) < idvchg_term(%d)\n", idvchg_lmt,
			 data->idvchg_term);
		goto out;
	}

	/* VBAT reaches CV level */
	if (vbat >= data->vbat_cv) {
		if (data->ita_measure < data->idvchg_term) {
			if (data->is_dvchg_en[DV2_DVCHG_SLAVE]) {
				ret = __dv2_check_slave_dvchg_off(info);
				if (ret < 0) {
					PCA_INFO("slave off fail(%d)\n", ret);
					goto out;
				}
				idvchg_lmt = __dv2_get_idvchg_lmt(info);
				goto cc_cv;
			}
			PCA_INFO("finish dv2 charging, vbat(%d), ita(%d)\n",
				 vbat, data->ita_measure);
			goto out;
		}
cc_cv:
		vta -= auth_data->vta_step;
		ita -= ita_gap_per_vstep;
		data->state = DV2_ALGO_CC_CV;
		goto out_set_cap;
	}

	/* IBUS reaches CC level */
	if (data->ita_measure + ita_gap_per_vstep > idvchg_lmt ||
	    vta == auth_data->vcap_max)
		data->state = DV2_ALGO_CC_CV;
	else {
		vta += auth_data->vta_step;
		vta = MIN(vta, auth_data->vcap_max);
		ita += ita_gap_per_vstep;
		ita = MIN(ita, idvchg_lmt);
	}

out_set_cap:
	ret = __dv2_set_ta_cap_cv(info, vta, ita);
	if (ret < 0) {
		PCA_ERR("set ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto out;
	}
	if (data->state == DV2_ALGO_SS_DVCHG) {
		end_time = ktime_get();
		delta_time = ktime_ms_delta(end_time, start_time);
		PCA_DBG("delta time %dms\n", delta_time);
		if (delta_time < desc->ta_cv_ss_repeat_tmin)
			msleep(desc->ta_cv_ss_repeat_tmin - delta_time);
		goto repeat;
	}
	return 0;
out:
	return __dv2_stop(info, &sinfo);
}

/* Soft start of divider charger */
static int __dv2_algo_ss_dvchg(struct dv2_algo_info *info)
{
	struct dv2_algo_data *data = info->data;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;

	return (auth_data->support_cc && !data->force_ta_cv) ?
		__dv2_algo_ss_dvchg_with_ta_cc(info) :
		__dv2_algo_ss_dvchg_with_ta_cv(info);
}

static int __dv2_check_swchg_off(struct dv2_algo_info *info,
				 struct dv2_stop_info *sinfo)
{
	int ret;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	u32 aicr = data->aicr_setting, ita, vbat;

	ret = __dv2_get_adc(info, PCA_ADCCHAN_VBAT, &vbat, &vbat);
	if (ret < 0) {
		PCA_ERR("get vbat fail(%d)\n", ret);
		return ret;
	}
	if (!data->is_swchg_en || (vbat < desc->swchg_off_vbat &&
	    desc->tswchg_curlmt[data->tswchg_level] <= 0 &&
	    aicr <= data->aicr_lmt))
		return 0;
	PCA_INFO("vbat(%d),swchg_off_vbat(%d),aicr_lmt(%d)\n", vbat,
		 desc->swchg_off_vbat, data->aicr_lmt);
	/* Calculate AICR */
	if (vbat >= desc->swchg_off_vbat)
		aicr -= desc->swchg_aicr_ss_step;
	aicr = MIN(aicr, data->aicr_lmt);
	/* Calculate ITA */
	if (aicr >= desc->swchg_aicr_ss_init)
		ita = data->ita_setting - (data->aicr_setting - aicr);
	else
		ita = data->ita_setting - data->aicr_setting;
	ret = __dv2_set_ta_cap_cc_by_cali_vta(info, ita);
	if (ret < 0) {
		PCA_ERR("set ta cap fail(%d)\n", ret);
		sinfo->hardreset_ta = true;
		return ret;
	}
	return (aicr >= desc->swchg_aicr_ss_init) ?
		__dv2_set_swchg_cap(info, aicr) :
		__dv2_enable_swchg_charging(info, false);
}

static int __dv2_update_aicr_lmt(struct dv2_algo_info *info)
{
	struct dv2_algo_desc *desc = info->desc;
	struct dv2_algo_data *data = info->data;
	u32 ita_lmt;

	if (!data->is_swchg_en)
		return 0;
	ita_lmt = __dv2_get_ita_lmt(info);
	/* Turn off swchg and use dvchg only */
	if (ita_lmt < data->idvchg_cc + desc->swchg_aicr_ss_init) {
		data->aicr_lmt = 0;
		return -EINVAL;
	}
	data->aicr_lmt = MIN(data->aicr_lmt, ita_lmt - data->idvchg_cc);
	if (data->aicr_lmt < desc->swchg_aicr_ss_init) {
		data->aicr_lmt = 0;
		return -EINVAL;
	}
	return 0;
}

static int __dv2_algo_ss_swchg(struct dv2_algo_info *info)
{
	int ret;
	struct dv2_algo_desc *desc = info->desc;
	struct dv2_algo_data *data = info->data;
	u32 ita = data->ita_setting, aicr = 0, vbat;
	struct dv2_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	PCA_DBG("++\n");

	if (__dv2_update_aicr_lmt(info) < 0)
		goto out;
	/* Set new AICR & TA cap */
	aicr = MIN(data->aicr_setting + desc->swchg_aicr_ss_step,
		   data->aicr_lmt);
	ita += (aicr - data->aicr_setting);
	ret = __dv2_set_swchg_cap(info, aicr);
	if (ret < 0) {
		PCA_ERR("set swchg cap fail(%d)\n", ret);
		goto err;
	}
	ret = __dv2_set_ta_cap_cc_by_cali_vta(info, ita);
	if (ret < 0) {
		PCA_ERR("set ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto err;
	}
out:
	ret = __dv2_get_adc(info, PCA_ADCCHAN_VBAT, &vbat, &vbat);
	if (ret < 0) {
		PCA_ERR("get vbat fail(%d)\n", ret);
		goto err;
	}
	ret = __dv2_check_swchg_off(info, &sinfo);
	if (ret < 0) {
		PCA_ERR("check swchg off fail(%d)\n", ret);
		goto err;
	}
	if (!data->is_swchg_en || vbat >= desc->swchg_off_vbat ||
	    aicr >= data->aicr_lmt)
		data->state = DV2_ALGO_CC_CV;
	return 0;
err:
	return __dv2_stop(info, &sinfo);
}

static int __dv2_algo_cc_cv_with_ta_cc(struct dv2_algo_info *info)
{
	int ret, vbat = 0;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	u32 ita = data->ita_setting, ita_lmt;
	struct dv2_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	PCA_DBG("++\n");

	ret = __dv2_check_force_ta_cv(info, &sinfo);
	if (ret < 0) {
		PCA_ERR("check force ta cv fail(%d)\n", ret);
		goto err;
	}
	if (data->force_ta_cv) {
		PCA_INFO("force switching to ta cv mode\n");
		return 0;
	}
	/* Check swchg */
	__dv2_update_aicr_lmt(info);
	ret = __dv2_check_swchg_off(info, &sinfo);
	if (ret < 0) {
		PCA_ERR("check swchg off fail(%d)\n", ret);
		goto err;
	}

	ret = __dv2_get_adc(info, PCA_ADCCHAN_VBAT, &vbat, &vbat);
	if (ret < 0)
		PCA_ERR("get vbat fail(%d)\n", ret);

	ret = __dv2_get_ta_cap(info);
	if (ret < 0) {
		PCA_ERR("get ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto err;
	}

	if (data->ita_measure < data->idvchg_term) {
		if (data->is_dvchg_en[DV2_DVCHG_SLAVE]) {
			ret = __dv2_check_slave_dvchg_off(info);
			if (ret < 0) {
				PCA_INFO("slave off fail(%d)\n", ret);
				goto err;
			}
			goto cc_cv;
		}
		PCA_INFO("finish dv2 charging\n");
		goto err;
	}
cc_cv:
	ita_lmt = __dv2_get_ita_lmt(info);
	/* Consider AICR is decreased */
	ita_lmt = MIN(ita_lmt, data->is_swchg_en ?
		      (data->idvchg_cc + data->aicr_setting) : data->idvchg_cc);
	if (ita_lmt < data->idvchg_term) {
		PCA_INFO("ita_lmt(%d) < idvchg_term(%d)\n", ita_lmt,
			 data->idvchg_term);
		goto err;
	}

	if (vbat >= data->vbat_cv) {
		ita = data->ita_setting - desc->idvchg_step;
		data->is_vbat_over_cv = true;
	} else if (vbat < desc->idvchg_ss_step1_vbat && ita < ita_lmt) {
		PCA_INFO("++ita(set,lmt,add)=(%d,%d,%d)\n", ita, ita_lmt,
			 desc->idvchg_ss_step);
		ita = data->ita_setting + desc->idvchg_ss_step;
	} else if (vbat < desc->idvchg_ss_step2_vbat && ita < ita_lmt) {
		PCA_INFO("++ita(set,lmt,add)=(%d,%d,%d)\n", ita, ita_lmt,
			 desc->idvchg_ss_step1);
		ita = data->ita_setting + desc->idvchg_ss_step1;
	} else if (!data->is_vbat_over_cv &&
		   vbat <= data->cv_lower_bound && ita < ita_lmt) {
		PCA_INFO("++ita(set,lmt,add)=(%d,%d,%d)\n", ita, ita_lmt,
			 desc->idvchg_step);
		ita = data->ita_setting + desc->idvchg_step;
	} else if (data->is_vbat_over_cv)
		data->is_vbat_over_cv = false;

	ita = MIN(ita, ita_lmt);
	ret = __dv2_set_ta_cap_cc_by_cali_vta(info, ita);
	if (ret < 0) {
		PCA_ERR("set_ta_cap fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto err;
	}
	return 0;
err:
	return __dv2_stop(info, &sinfo);
}

static int __dv2_algo_cc_cv_with_ta_cv(struct dv2_algo_info *info)
{
	int ret, vbat;
	struct dv2_algo_data *data = info->data;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;
	u32 idvchg_lmt, vta = data->vta_setting, ita = data->ita_setting;
	u32 ita_gap_per_vstep = data->ita_gap_per_vstep > 0 ?
				data->ita_gap_per_vstep :
				auth_data->ita_gap_per_vstep;
	u32 vta_measure, ita_measure, suspect_ta_cc = false;
	struct dv2_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	PCA_DBG("++\n");

	ret = __dv2_get_adc(info, PCA_ADCCHAN_VBAT, &vbat, &vbat);
	if (ret < 0) {
		PCA_ERR("get vbat fail(%d)\n", ret);
		goto out;
	}
	ret = __dv2_get_ta_cap_by_supportive(info, &data->vta_measure,
					     &data->ita_measure);
	if (ret < 0) {
		PCA_ERR("get ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = auth_data->support_meas_cap;
		goto out;
	}
	if (data->ita_measure <= data->idvchg_term) {
		if (data->is_dvchg_en[DV2_DVCHG_SLAVE]) {
			ret = __dv2_check_slave_dvchg_off(info);
			if (ret < 0) {
				PCA_INFO("slave off fail(%d)\n", ret);
				goto out;
			}
			goto cc_cv;
		}
		PCA_INFO("finish dv2 charging\n");
		goto out;
	}
cc_cv:
	idvchg_lmt = __dv2_get_idvchg_lmt(info);
	if (idvchg_lmt < data->idvchg_term) {
		PCA_INFO("idvchg_lmt(%d) < idvchg_term(%d)\n", idvchg_lmt,
			 data->idvchg_term);
		goto out;
	}

	if (vbat >= data->vbat_cv) {
		vta -= auth_data->vta_step;
		ita -= ita_gap_per_vstep;
		data->is_vbat_over_cv = true;
	} else if (data->ita_measure > idvchg_lmt) {
		vta -= auth_data->vta_step;
		ita -= ita_gap_per_vstep;
		ita = MAX(ita, idvchg_lmt);
		PCA_INFO("--vta, ita(meas,lmt)=(%d,%d)\n", data->ita_measure,
			 idvchg_lmt);
	} else if (!data->is_vbat_over_cv && vbat <= data->cv_lower_bound &&
		   data->ita_measure <= (idvchg_lmt - ita_gap_per_vstep) &&
		   vta < auth_data->vcap_max && !data->suspect_ta_cc) {
		vta += auth_data->vta_step;
		vta = MIN(vta, auth_data->vcap_max);
		ita += ita_gap_per_vstep;
		ita = MIN(ita, idvchg_lmt);
		if (ita == data->ita_setting)
			suspect_ta_cc = true;
		PCA_INFO("++vta, ita(meas,lmt)=(%d,%d)\n", data->ita_measure,
			 idvchg_lmt);
	} else if (data->is_vbat_over_cv)
		data->is_vbat_over_cv = false;

	ret = __dv2_set_ta_cap_cv(info, vta, ita);
	if (ret < 0) {
		PCA_ERR("set_ta_cap fail(%d)\n", ret);
		sinfo.hardreset_ta = true;
		goto out;
	}
	ret = __dv2_get_ta_cap_by_supportive(info, &vta_measure, &ita_measure);
	if (ret < 0) {
		PCA_ERR("get ta cap fail(%d)\n", ret);
		sinfo.hardreset_ta = auth_data->support_meas_cap;
		goto out;
	}
	data->suspect_ta_cc = (suspect_ta_cc &&
			       data->ita_measure == ita_measure);
	return 0;
out:
	return __dv2_stop(info, &sinfo);
}

static int __dv2_algo_cc_cv(struct dv2_algo_info *info)
{
	struct dv2_algo_data *data = info->data;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;

	return (auth_data->support_cc && !data->force_ta_cv) ?
		__dv2_algo_cc_cv_with_ta_cc(info) :
		__dv2_algo_cc_cv_with_ta_cv(info);
}

/*
 * Check TA's status
 * Get status from TA and check temperature, OCP, OTP, and OVP, etc...
 *
 * return true if TA is normal and false if it is abnormal
 */
static bool __dv2_check_ta_status(struct dv2_algo_info *info,
				  struct dv2_stop_info *sinfo)
{
	int ret;
	struct prop_chgalgo_ta_status status;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;

	if (!auth_data->support_status)
		return desc->allow_not_check_ta_status;
	ret = prop_chgalgo_get_ta_status(data->pca_ta, &status);
	if (ret < 0) {
		PCA_ERR("get ta status fail(%d)\n", ret);
		goto err;
	}

	PCA_INFO("temp = (%d,%d), (OVP,OCP,OTP) = (%d,%d,%d)\n",
		 status.temp1, status.temp_level, status.ovp, status.ocp,
		 status.otp);
	if (status.ocp || status.otp || status.ovp)
		goto err;
	return true;
err:
	sinfo->hardreset_ta = true;
	return false;
}

static bool __dv2_check_dvchg_ibusocp(struct dv2_algo_info *info,
				      struct dv2_stop_info *sinfo)
{
	int ret, i, ibus, acc = 0;
	struct dv2_algo_data *data = info->data;
	u32 ibusocp;

	if (!data->is_dvchg_en[DV2_DVCHG_MASTER])
		return true;
	ibusocp =  __dv2_get_dvchg_ibusocp(info, data->ita_setting);
	if (data->is_dvchg_en[DV2_DVCHG_SLAVE])
		ibusocp /= 2;
	for (i = DV2_DVCHG_MASTER; i < DV2_DVCHG_MAX; i++) {
		if (!data->is_dvchg_en[i])
			continue;
		prop_chgalgo_get_adc_accuracy(data->pca_dvchg[i],
					      PCA_ADCCHAN_IBUS, &acc, &acc);
		ret = prop_chgalgo_get_adc(data->pca_dvchg[i], PCA_ADCCHAN_IBUS,
					   &ibus, &ibus);
		if (ret < 0) {
			PCA_ERR("get ibus fail(%d)\n", ret);
			return false;
		}
		PCA_INFO("(%s)ibus(%d+-%dmA), ibusocp(%dmA)\n",
			 __dv2_dvchg_role_name[i], ibus, acc, ibusocp);
		if (ibus > acc)
			ibus -= acc;
		if (ibus > ibusocp) {
			PCA_ERR("(%s)ibus(%dmA) > ibusocp(%dmA)\n",
				__dv2_dvchg_role_name[i], ibus, ibusocp);
			return false;
		}
	}
	return true;
}

static bool __dv2_check_ta_ibusocp(struct dv2_algo_info *info,
				   struct dv2_stop_info *sinfo)
{
	int ret;
	struct dv2_algo_data *data = info->data;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;
	u32 itaocp;

	if (!auth_data->support_meas_cap)
		return true;
	ret = __dv2_get_ta_cap(info);
	if (ret < 0) {
		PCA_ERR("get ta cap fail(%d)\n", ret);
		goto err;
	}
	itaocp = __dv2_get_itaocp(info);
	PCA_INFO("ita(%dmA), itaocp(%dmA)\n", data->ita_measure, itaocp);
	if (data->ita_measure > itaocp) {
		PCA_ERR("ita(%dmA) > itaocp(%dmA)\n", data->ita_measure,
			itaocp);
		/* double confirm using dvchg */
		if (!__dv2_check_dvchg_ibusocp(info, sinfo))
			return true;
		goto err;
	}
	return true;

err:
	sinfo->hardreset_ta = true;
	return false;
}

/*
 * Check VBUS voltage of divider charger
 * return false if VBUS is over voltage otherwise return true
 */
static bool __dv2_check_dvchg_vbusovp(struct dv2_algo_info *info,
				      struct dv2_stop_info *sinfo)
{
	int ret, vbus, i;
	struct dv2_algo_data *data = info->data;
	u32 vbusovp;

	vbusovp = __dv2_get_dvchg_vbusovp(info, data->ita_setting);
	for (i = DV2_DVCHG_MASTER; i < DV2_DVCHG_MAX; i++) {
		if (!data->is_dvchg_en[i])
			continue;
		ret = prop_chgalgo_get_adc(data->pca_dvchg[i], PCA_ADCCHAN_VBUS,
					   &vbus, &vbus);
		if (ret < 0) {
			PCA_ERR("get vbus fail(%d)\n", ret);
			return false;
		}
		PCA_INFO("(%s)vbus(%dmV), vbusovp(%dmV)\n",
			 __dv2_dvchg_role_name[i], vbus, vbusovp);
		if (vbus > vbusovp) {
			PCA_ERR("(%s)vbus(%dmV) > vbusovp(%dmV)\n",
				__dv2_dvchg_role_name[i], vbus, vbusovp);
			return false;
		}
	}
	return true;
}

static bool __dv2_check_vbatovp(struct dv2_algo_info *info,
				struct dv2_stop_info *sinfo)
{
	int ret, vbat;
	u32 vbatovp;

	vbatovp =  __dv2_get_vbatovp(info);
	ret = __dv2_get_adc(info, PCA_ADCCHAN_VBAT, &vbat, &vbat);
	if (ret < 0) {
		PCA_ERR("get vbat fail(%d)\n", ret);
		return false;
	}
	PCA_INFO("vbat(%dmV), vbatovp(%dmV)\n", vbat, vbatovp);
	if (vbat > vbatovp) {
		PCA_ERR("vbat(%dmV) > vbatovp(%dmV)\n", vbat, vbatovp);
		return false;
	}
	return true;
}

static bool __dv2_check_ibatocp(struct dv2_algo_info *info,
				struct dv2_stop_info *sinfo)
{
	int ret, ibat, bat_current;
	struct dv2_algo_data *data = info->data;
	u32 ibatocp;

	if (!data->is_dvchg_en[DV2_DVCHG_MASTER])
		return true;
	ibatocp =  __dv2_get_ibatocp(info, data->ita_setting);
	ret = __dv2_get_adc(info, PCA_ADCCHAN_IBAT, &ibat, &ibat);
	if (ret < 0) {
		PCA_ERR("get ibat fail(%d)\n", ret);
		return false;
	}
	bat_current = battery_get_bat_current() / 10;
	PCA_INFO("ibat(%dmA), ibatocp(%dmA), ibat_gauge(%dmA)\n", ibat,
		 ibatocp, bat_current);
	if (ibat > ibatocp) {
		PCA_ERR("ibat(%dmA) > ibatocp(%dmA)\n", ibat, ibatocp);
		return false;
	}
	return true;
}

struct dv2_thermal_data {
	const char *name;
	int temp;
	enum dv2_thermal_level *temp_level;
	int *temp_level_def;
	int *curlmt;
	int recovery_area;
};

static bool __dv2_check_thermal_level(struct dv2_algo_info *info,
				      struct dv2_thermal_data *tdata)
{
	if (tdata->temp >= tdata->temp_level_def[DV2_THERMAL_VERY_HOT]) {
		if (tdata->curlmt[DV2_THERMAL_VERY_HOT] == 0)
			return true;
		PCA_ERR("%s(%d) is over max(%d)\n", tdata->name, tdata->temp,
			tdata->temp_level_def[DV2_THERMAL_VERY_HOT]);
		return false;
	}
	if (tdata->temp <= tdata->temp_level_def[DV2_THERMAL_VERY_COLD]) {
		if (tdata->curlmt[DV2_THERMAL_VERY_COLD] == 0)
			return true;
		PCA_ERR("%s(%d) is under min(%d)\n", tdata->name, tdata->temp,
			tdata->temp_level_def[DV2_THERMAL_VERY_COLD]);
		return false;
	}
	switch (*tdata->temp_level) {
	case DV2_THERMAL_COLD:
		if (tdata->temp >= (tdata->temp_level_def[DV2_THERMAL_COLD] +
				    tdata->recovery_area))
			*tdata->temp_level = DV2_THERMAL_VERY_COOL;
		break;
	case DV2_THERMAL_VERY_COOL:
		if (tdata->temp >=
		    (tdata->temp_level_def[DV2_THERMAL_VERY_COOL] +
		     tdata->recovery_area))
			*tdata->temp_level = DV2_THERMAL_COOL;
		else if (tdata->temp <=
			 tdata->temp_level_def[DV2_THERMAL_COLD] &&
			 tdata->curlmt[DV2_THERMAL_COLD] > 0)
			*tdata->temp_level = DV2_THERMAL_COLD;
		break;
	case DV2_THERMAL_COOL:
		if (tdata->temp >= (tdata->temp_level_def[DV2_THERMAL_COOL] +
				    tdata->recovery_area))
			*tdata->temp_level = DV2_THERMAL_NORMAL;
		else if (tdata->temp <=
			 tdata->temp_level_def[DV2_THERMAL_VERY_COOL] &&
			 tdata->curlmt[DV2_THERMAL_VERY_COOL] > 0)
			*tdata->temp_level = DV2_THERMAL_VERY_COOL;
		break;
	case DV2_THERMAL_NORMAL:
		if (tdata->temp >= tdata->temp_level_def[DV2_THERMAL_WARM] &&
		    tdata->curlmt[DV2_THERMAL_WARM] > 0)
			*tdata->temp_level = DV2_THERMAL_WARM;
		else if (tdata->temp <=
			 tdata->temp_level_def[DV2_THERMAL_COOL] &&
			 tdata->curlmt[DV2_THERMAL_COOL] > 0)
			*tdata->temp_level = DV2_THERMAL_COOL;
		break;
	case DV2_THERMAL_WARM:
		if (tdata->temp <= (tdata->temp_level_def[DV2_THERMAL_WARM] -
				    tdata->recovery_area))
			*tdata->temp_level = DV2_THERMAL_NORMAL;
		else if (tdata->temp >=
			 tdata->temp_level_def[DV2_THERMAL_VERY_WARM] &&
			 tdata->curlmt[DV2_THERMAL_VERY_WARM] > 0)
			*tdata->temp_level = DV2_THERMAL_VERY_WARM;
		break;
	case DV2_THERMAL_VERY_WARM:
		if (tdata->temp <=
		    (tdata->temp_level_def[DV2_THERMAL_VERY_WARM] -
		     tdata->recovery_area))
			*tdata->temp_level = DV2_THERMAL_WARM;
		else if (tdata->temp >=
			 tdata->temp_level_def[DV2_THERMAL_HOT] &&
			 tdata->curlmt[DV2_THERMAL_HOT] > 0)
			*tdata->temp_level = DV2_THERMAL_HOT;
		break;
	case DV2_THERMAL_HOT:
		if (tdata->temp <= (tdata->temp_level_def[DV2_THERMAL_HOT] -
				    tdata->recovery_area))
			*tdata->temp_level = DV2_THERMAL_VERY_WARM;
		break;
	default:
		PCA_ERR("NO SUCH STATE\n");
		return false;
	}
	PCA_INFO("%s(%d,%d)\n", tdata->name, tdata->temp, *tdata->temp_level);
	return true;
}

/*
 * Check and adjust battery's temperature level
 * return false if battery's temperature is over maximum or under minimum
 * otherwise return true
 */
static bool __dv2_check_tbat_level(struct dv2_algo_info *info,
				   struct dv2_stop_info *sinfo)
{
	int ret, tbat;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	struct dv2_thermal_data tdata = {
		.name = "tbat",
		.temp_level_def = desc->tbat_level_def,
		.curlmt = desc->tbat_curlmt,
		.temp_level = &data->tbat_level,
		.recovery_area = desc->tbat_recovery_area,
	};

	ret = __dv2_get_adc(info, PCA_ADCCHAN_TBAT, &tbat, &tbat);
	if (ret < 0) {
		PCA_ERR("get tbat fail(%d)\n", ret);
		return false;
	}
	tdata.temp = tbat;
	return __dv2_check_thermal_level(info, &tdata);
}

/*
 * Check and adjust TA's temperature level
 * return false if TA's temperature is over maximum
 * otherwise return true
 */
static bool __dv2_check_tta_level(struct dv2_algo_info *info,
				  struct dv2_stop_info *sinfo)
{
	int ret, tta;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;
	struct dv2_thermal_data tdata = {
		.name = "tta",
		.temp_level_def = desc->tta_level_def,
		.curlmt = desc->tta_curlmt,
		.temp_level = &data->tta_level,
		.recovery_area = desc->tta_recovery_area,
	};

	if (!auth_data->support_status)
		return desc->allow_not_check_ta_status;

	ret = prop_chgalgo_get_ta_temperature(data->pca_ta, &tta);
	if (ret < 0) {
		PCA_ERR("get tta fail(%d)\n", ret);
		sinfo->hardreset_ta = true;
		return false;
	}

	tdata.temp = tta;
	return __dv2_check_thermal_level(info, &tdata);
}

/*
 * Check and adjust divider charger's temperature level
 * return false if divider charger's temperature is over maximum
 * otherwise return true
 */
static bool __dv2_check_tdvchg_level(struct dv2_algo_info *info,
				     struct dv2_stop_info *sinfo)
{
	int ret, i, tdvchg;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	char buf[14];
	struct dv2_thermal_data tdata = {
		.temp_level_def = desc->tdvchg_level_def,
		.curlmt = desc->tdvchg_curlmt,
		.temp_level = &data->tdvchg_level,
		.recovery_area = desc->tdvchg_recovery_area,
	};

	for (i = DV2_DVCHG_MASTER; i < DV2_DVCHG_MAX; i++) {
		if (!data->is_dvchg_en[i])
			continue;
		ret = prop_chgalgo_get_adc(data->pca_dvchg[i], PCA_ADCCHAN_TCHG,
					   &tdvchg, &tdvchg);
		if (ret < 0) {
			PCA_ERR("get tdvchg fail(%d)\n", ret);
			return false;
		}
		snprintf(buf, 8 + strlen(__dv2_dvchg_role_name[i]), "tdvchg_%s",
			 __dv2_dvchg_role_name[i]);
		tdata.name = buf;
		tdata.temp = tdvchg;
		if (!__dv2_check_thermal_level(info, &tdata))
			return false;
	}
	return true;
}

/*
 * Check and adjust switching charger's temperature level
 * return false if switching charger's temperature is over maximum
 * otherwise return true
 */
static bool __dv2_check_tswchg_level(struct dv2_algo_info *info,
				     struct dv2_stop_info *sinfo)
{
	int ret, tswchg;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	struct dv2_thermal_data tdata = {
		.name = "tswchg",
		.temp_level_def = desc->tswchg_level_def,
		.curlmt = desc->tswchg_curlmt,
		.temp_level = &data->tswchg_level,
		.recovery_area = desc->tswchg_recovery_area,
	};

	if (!data->is_swchg_en)
		return true;

	ret = prop_chgalgo_get_adc(data->pca_swchg, PCA_ADCCHAN_TCHG,
				   &tswchg, &tswchg);
	if (ret < 0) {
		PCA_ERR("get tswchg fail(%d)\n", ret);
		return false;
	}
	tdata.temp = tswchg;
	if (!__dv2_check_thermal_level(info, &tdata))
		return false;
	data->aicr_lmt = MIN(data->aicr_lmt, data->aicr_init_lmt -
			     desc->tswchg_curlmt[data->tswchg_level]);
	return true;
}

static bool
(*__dv2_safety_check_fn[])(struct dv2_algo_info *info,
			   struct dv2_stop_info *sinfo) = {
	__dv2_check_ta_status,
	__dv2_check_ta_ibusocp,
	__dv2_check_dvchg_vbusovp,
	__dv2_check_dvchg_ibusocp,
	__dv2_check_vbatovp,
	__dv2_check_ibatocp,
	__dv2_check_tbat_level,
	__dv2_check_tta_level,
	__dv2_check_tdvchg_level,
	__dv2_check_tswchg_level,
};

static bool __dv2_algo_safety_check(struct dv2_algo_info *info)
{
	int i;
	struct dv2_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	PCA_DBG("++\n");
	for (i = 0; i < ARRAY_SIZE(__dv2_safety_check_fn); i++) {
		if (!__dv2_safety_check_fn[i](info, &sinfo))
			goto err;
	}
	return true;

err:
	__dv2_stop(info, &sinfo);
	return false;
}

static bool __dv2_is_ta_rdy(struct dv2_algo_info *info)
{
	int ret, i;
	struct dv2_algo_desc *desc = info->desc;
	struct dv2_algo_data *data = info->data;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;

	if (data->ta_ready)
		return true;

	auth_data->vcap_min = desc->vta_cap_min;
	auth_data->vcap_max = desc->vta_cap_max;
	auth_data->icap_min = desc->ita_cap_min;
	auth_data->pwr_lmt = false;
	auth_data->pdp = 0;
	for (i = 0; i < desc->support_ta_cnt; i++) {
		if (!data->pca_ta_pool[i])
			continue;
		ret = prop_chgalgo_authenticate_ta(data->pca_ta_pool[i],
						   auth_data);
		if (ret < 0) {
			PCA_DBG("%s authenticate fail(%d)\n",
				desc->support_ta[i], ret);
			continue;
		}

		data->pca_ta = data->pca_ta_pool[i];
		PCA_INFO("%s,lmt(%d,%dW),step(%d,%d),cc=%d,cap=%d,status=%d\n",
			 desc->support_ta[i], auth_data->pwr_lmt,
			 auth_data->pdp, auth_data->vta_step,
			 auth_data->ita_step, auth_data->support_cc,
			 auth_data->support_meas_cap,
			 auth_data->support_status);
		data->ta_ready = true;
		return true;
	}
	return false;
}

static inline void __dv2_wakeup_algo_thread(struct dv2_algo_data *data)
{
	PCA_DBG("++\n");
	atomic_set(&data->wakeup_thread, 1);
	wake_up_interruptible(&data->wq);
}

static enum alarmtimer_restart
__dv2_algo_timer_cb(struct alarm *alarm, ktime_t now)
{
	struct dv2_algo_data *data =
		container_of(alarm, struct dv2_algo_data, timer);

	PCA_DBG("++\n");
	__dv2_wakeup_algo_thread(data);
	return ALARMTIMER_NORESTART;
}

/*
 * Check charging time of dv2 algorithm
 * return false if timeout otherwise return true
 */
static bool __dv2_algo_check_charging_time(struct dv2_algo_info *info)
{
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	struct timespec etime, dtime;
	struct dv2_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	get_monotonic_boottime(&etime);
	dtime = timespec_sub(etime, data->stime);
	if (dtime.tv_sec >= desc->chg_time_max) {
		PCA_ERR("dv2 algo timeout(%d, %d)\n", (int)dtime.tv_sec,
			desc->chg_time_max);
		__dv2_stop(info, &sinfo);
		return false;
	}
	return true;
}

static inline int __dv2_plugout_reset(struct dv2_algo_info *info,
				      struct dv2_stop_info *sinfo)
{
	struct dv2_algo_data *data = info->data;

	PCA_DBG("++\n");
	data->ta_ready = false;
	data->run_once = false;
	return __dv2_stop(info, sinfo);
}

static int __dv2_notify_hardreset_hdlr(struct dv2_algo_info *info)
{
	struct dv2_stop_info sinfo = {
		.reset_ta = false,
		.hardreset_ta = false,
	};

	PCA_INFO("++\n");
	return __dv2_plugout_reset(info, &sinfo);
}

static int __dv2_notify_detach_hdlr(struct dv2_algo_info *info)
{
	struct dv2_stop_info sinfo = {
		.reset_ta = false,
		.hardreset_ta = false,
	};

	PCA_INFO("++\n");
	return __dv2_plugout_reset(info, &sinfo);
}

static int __dv2_notify_hwerr_hdlr(struct dv2_algo_info *info)
{
	struct dv2_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	PCA_INFO("++\n");
	return __dv2_stop(info, &sinfo);
}

static int __dv2_notify_ibusucpf_hdlr(struct dv2_algo_info *info)
{
	int ret, ibus;
	struct dv2_algo_data *data = info->data;

	if (data->ignore_ibusucpf) {
		PCA_INFO("ignore ibusucpf\n");
		data->ignore_ibusucpf = false;
		return 0;
	}
	if (!data->is_dvchg_en[DV2_DVCHG_MASTER]) {
		PCA_INFO("master dvchg is off\n");
		return 0;
	}
	/* Last chance */
	ret = prop_chgalgo_get_adc(data->pca_dvchg[DV2_DVCHG_MASTER],
				   PCA_ADCCHAN_IBUS, &ibus, &ibus);
	if (ret < 0) {
		PCA_ERR("get dvchg ibus fail(%d)\n", ret);
		goto out;
	}
	if (ibus < DV2_IBUSUCPF_RECHECK) {
		PCA_ERR("ibus(%d) < recheck(%d)\n", ibus, DV2_IBUSUCPF_RECHECK);
		goto out;
	}
	PCA_INFO("recheck ibus and it is not ucp\n");
	return 0;
out:
	return __dv2_notify_hwerr_hdlr(info);
}

static int __dv2_notify_vbatovp_alarm_hdlr(struct dv2_algo_info *info)
{
	int ret;
	struct dv2_algo_data *data = info->data;
	struct dv2_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	if (data->state == DV2_ALGO_STOP)
		return 0;
	PCA_INFO("++\n");
	ret = prop_chgalgo_reset_vbatovp_alarm(
		data->pca_dvchg[DV2_DVCHG_MASTER]);
	if (ret < 0) {
		PCA_ERR("reset vbatovp alarm fail(%d)\n", ret);
		return __dv2_stop(info, &sinfo);
	}
	return 0;
}

static int __dv2_notify_vbusovp_alarm_hdlr(struct dv2_algo_info *info)
{
	int ret;
	struct dv2_algo_data *data = info->data;
	struct dv2_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	if (data->state == DV2_ALGO_STOP)
		return 0;
	PCA_INFO("++\n");
	ret = prop_chgalgo_reset_vbusovp_alarm(
		data->pca_dvchg[DV2_DVCHG_MASTER]);
	if (ret < 0) {
		PCA_ERR("reset vbusovp alarm fail(%d)\n", ret);
		return __dv2_stop(info, &sinfo);
	}
	return 0;
}

static int
(*__dv2_notify_pre_hdlr[PCA_NOTIEVT_MAX])(struct dv2_algo_info *info) = {
	[PCA_NOTIEVT_DETACH] = __dv2_notify_detach_hdlr,
	[PCA_NOTIEVT_HARDRESET] = __dv2_notify_hardreset_hdlr,
	[PCA_NOTIEVT_VBUSOVP] = __dv2_notify_hwerr_hdlr,
	[PCA_NOTIEVT_IBUSOCP] = __dv2_notify_hwerr_hdlr,
	[PCA_NOTIEVT_IBUSUCP_FALL] = __dv2_notify_ibusucpf_hdlr,
	[PCA_NOTIEVT_VBATOVP] = __dv2_notify_hwerr_hdlr,
	[PCA_NOTIEVT_IBATOCP] = __dv2_notify_hwerr_hdlr,
	[PCA_NOTIEVT_VOUTOVP] = __dv2_notify_hwerr_hdlr,
	[PCA_NOTIEVT_VDROVP] = __dv2_notify_hwerr_hdlr,
	[PCA_NOTIEVT_VBATOVP_ALARM] = __dv2_notify_vbatovp_alarm_hdlr,
};

static int
(*__dv2_notify_post_hdlr[PCA_NOTIEVT_MAX])(struct dv2_algo_info *info) = {
	[PCA_NOTIEVT_DETACH] = __dv2_notify_detach_hdlr,
	[PCA_NOTIEVT_HARDRESET] = __dv2_notify_hardreset_hdlr,
	[PCA_NOTIEVT_VBUSOVP] = __dv2_notify_hwerr_hdlr,
	[PCA_NOTIEVT_IBUSOCP] = __dv2_notify_hwerr_hdlr,
	[PCA_NOTIEVT_IBUSUCP_FALL] = __dv2_notify_ibusucpf_hdlr,
	[PCA_NOTIEVT_VBATOVP] = __dv2_notify_hwerr_hdlr,
	[PCA_NOTIEVT_IBATOCP] = __dv2_notify_hwerr_hdlr,
	[PCA_NOTIEVT_VOUTOVP] = __dv2_notify_hwerr_hdlr,
	[PCA_NOTIEVT_VDROVP] = __dv2_notify_hwerr_hdlr,
	[PCA_NOTIEVT_VBATOVP_ALARM] = __dv2_notify_vbatovp_alarm_hdlr,
	[PCA_NOTIEVT_VBUSOVP_ALARM] = __dv2_notify_vbusovp_alarm_hdlr,
};

static int __dv2_pre_handle_notify_evt(struct dv2_algo_info *info)
{
	int i;
	struct dv2_algo_data *data = info->data;

	mutex_lock(&data->notify_lock);
	PCA_DBG("0x%08X\n", data->notify);
	for (i = 0; i < PCA_NOTIEVT_MAX; i++) {
		if ((data->notify & BIT(i)) && __dv2_notify_pre_hdlr[i]) {
			data->notify &= ~BIT(i);
			mutex_unlock(&data->notify_lock);
			__dv2_notify_pre_hdlr[i](info);
			mutex_lock(&data->notify_lock);
		}
	}
	mutex_unlock(&data->notify_lock);
	return 0;
}

static int __dv2_post_handle_notify_evt(struct dv2_algo_info *info)
{
	int i;
	struct dv2_algo_data *data = info->data;

	mutex_lock(&data->notify_lock);
	PCA_DBG("0x%08X\n", data->notify);
	for (i = 0; i < PCA_NOTIEVT_MAX; i++) {
		if ((data->notify & BIT(i)) && __dv2_notify_post_hdlr[i]) {
			data->notify &= ~BIT(i);
			mutex_unlock(&data->notify_lock);
			__dv2_notify_post_hdlr[i](info);
			mutex_lock(&data->notify_lock);
		}
	}
	mutex_unlock(&data->notify_lock);
	return 0;
}

static int __dv2_dump_charging_info(struct dv2_algo_info *info)
{
	int ret, i;
	int vbus, ibus[DV2_DVCHG_MAX] = {0}, ibus_swchg = 0, vbat, ibat;
	struct dv2_algo_data *data = info->data;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;

	/* vbus */
	ret = __dv2_get_adc(info, PCA_ADCCHAN_VBUS, &vbus, &vbus);
	if (ret < 0)
		PCA_ERR("get vbus fail\n");
	/* ibus */
	for (i = 0; i < DV2_DVCHG_MAX; i++) {
		if (!data->is_dvchg_en[i])
			continue;
		ret = prop_chgalgo_get_adc(data->pca_dvchg[i], PCA_ADCCHAN_IBUS,
					   &ibus[i], &ibus[i]);
		if (ret < 0) {
			PCA_ERR("get %s ibus fail\n", __dv2_dvchg_role_name[i]);
			continue;
		}
	}
	if (data->is_swchg_en) {
		ret = prop_chgalgo_get_adc(data->pca_swchg, PCA_ADCCHAN_IBUS,
					   &ibus_swchg, &ibus_swchg);
		if (ret < 0)
			PCA_ERR("get swchg ibus fail\n");
	}
	/* vbat */
	ret = __dv2_get_adc(info, PCA_ADCCHAN_VBAT, &vbat, &vbat);
	if (ret < 0)
		PCA_ERR("get vbat fail(%d)\n", ret);
	/* ibat */
	ret = __dv2_get_adc(info, PCA_ADCCHAN_IBAT, &ibat, &ibat);
	if (ret < 0)
		PCA_ERR("get ibat fail(%d)\n", ret);

	if (auth_data->support_meas_cap) {
		ret = __dv2_get_ta_cap(info);
		if (ret < 0)
			PCA_ERR("get ta measure cap fail(%d)\n", ret);
	} else {
		data->vta_measure = vbus;
		data->ita_measure = ibus[DV2_DVCHG_MASTER] +
				    ibus[DV2_DVCHG_SLAVE];
	}

	PCA_INFO("vbus,ibus(master,slave,sw),vbat,ibat=%d,(%d,%d,%d),%d,%d\n",
		 vbus, ibus[DV2_DVCHG_MASTER], ibus[DV2_DVCHG_SLAVE],
		 ibus_swchg, vbat, ibat);
	PCA_INFO("vta,ita(set,meas)=(%d,%d),(%d,%d),force_cv=%d\n",
		 data->vta_setting, data->vta_measure, data->ita_setting,
		 data->ita_measure, data->force_ta_cv);
	return 0;
}

static int __dv2_algo_threadfn(void *param)
{
	struct dv2_algo_info *info = param;
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	struct prop_chgalgo_ta_auth_data *auth_data = &data->ta_auth_data;
	u32 sec, ms, polling_interval;
	ktime_t ktime;
	struct dv2_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	while (!kthread_should_stop()) {
		wait_event_interruptible(data->wq,
					 atomic_read(&data->wakeup_thread));
		pm_stay_awake(info->dev);
		if (atomic_read(&data->stop_thread)) {
			pm_relax(info->dev);
			break;
		}
		atomic_set(&data->wakeup_thread, 0);
		mutex_lock(&data->lock);
		PCA_INFO("state = %s\n", __dv2_algo_state_name[data->state]);
		if (atomic_read(&data->stop_algo))
			__dv2_stop(info, &sinfo);
		__dv2_pre_handle_notify_evt(info);
		if (data->state != DV2_ALGO_STOP) {
			__dv2_algo_check_charging_time(info);
			__dv2_calculate_vbat_ircmp(info);
			__dv2_select_vbat_cv(info);
			__dv2_dump_charging_info(info);
		}
		switch (data->state) {
		case DV2_ALGO_INIT:
			__dv2_algo_init(info);
			break;
		case DV2_ALGO_MEASURE_R:
			__dv2_algo_measure_r(info);
			break;
		case DV2_ALGO_SS_SWCHG:
			__dv2_algo_ss_swchg(info);
			break;
		case DV2_ALGO_SS_DVCHG:
			__dv2_algo_ss_dvchg(info);
			break;
		case DV2_ALGO_CC_CV:
			__dv2_algo_cc_cv(info);
			break;
		case DV2_ALGO_STOP:
			PCA_INFO("DV2 ALGO STOP\n");
			break;
		default:
			PCA_ERR("NO SUCH STATE\n");
			break;
		}
		__dv2_post_handle_notify_evt(info);
		if (data->state != DV2_ALGO_STOP) {
			if (!__dv2_algo_safety_check(info))
				goto cont;
			__dv2_dump_charging_info(info);
			if (data->state == DV2_ALGO_CC_CV &&
			    auth_data->support_cc && !data->force_ta_cv)
				polling_interval = desc->polling_interval;
			else
				polling_interval =
					DV2_ALGO_INIT_POLLING_INTERVAL;
			sec = polling_interval / 1000;
			ms = polling_interval % 1000;
			ktime = ktime_set(sec, MS_TO_NS(ms));
			alarm_start_relative(&data->timer, ktime);
		}
cont:
		mutex_unlock(&data->lock);
		pm_relax(info->dev);
	}
	return 0;
}

/* =================================================================== */
/* DV2 Algo OPS                                                        */
/* =================================================================== */

static int dv2_init_algo(struct prop_chgalgo_device *pca)
{
	int ret = 0, i;
	struct dv2_algo_info *info = prop_chgalgo_get_drvdata(pca);
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	bool has_ta = false;

	mutex_lock(&data->lock);
	PCA_DBG("++\n");

	if (data->inited) {
		PCA_INFO("already inited\n");
		goto out;
	}

	/* get ta & chg pca device */
	data->pca_ta_pool = devm_kzalloc(info->dev,
					 sizeof(struct prop_chgalgo_device *) *
					 desc->support_ta_cnt, GFP_KERNEL);
	if (!data->pca_ta_pool) {
		ret = -ENOMEM;
		goto out;
	}
	for (i = 0; i < desc->support_ta_cnt; i++) {
		data->pca_ta_pool[i] =
			prop_chgalgo_dev_get_by_name(desc->support_ta[i]);
		if (data->pca_ta_pool[i]) {
			has_ta = true;
			continue;
		}
		PCA_ERR("no %s\n", desc->support_ta[i]);
	}
	if (!has_ta) {
		ret = -ENODEV;
		goto out;
	}

	data->pca_swchg = prop_chgalgo_dev_get_by_name("pca_chg_swchg");
	if (!data->pca_swchg) {
		PCA_ERR("get pca_swchg fail\n");
		ret = -ENODEV;
		goto out;
	}
	data->pca_dvchg[DV2_DVCHG_MASTER] =
		prop_chgalgo_dev_get_by_name("pca_chg_dvchg");
	if (!data->pca_dvchg[DV2_DVCHG_MASTER]) {
		PCA_ERR("get pca_dvchg fail\n");
		ret = -ENODEV;
		goto out;
	}
	data->pca_dvchg[DV2_DVCHG_SLAVE] =
		prop_chgalgo_dev_get_by_name("pca_chg_dvchg_slave");
	if (!data->pca_dvchg[DV2_DVCHG_SLAVE])
		PCA_ERR("get pca_dvchg_slave fail\n");

	data->inited = true;
	PCA_INFO("successfully\n");
out:
	mutex_unlock(&data->lock);
	return ret;
}

static bool dv2_is_algo_ready(struct prop_chgalgo_device *pca)
{
	int ret;
	bool rdy = true;
	struct dv2_algo_info *info = prop_chgalgo_get_drvdata(pca);
	struct dv2_algo_data *data = info->data;
	struct dv2_algo_desc *desc = info->desc;
	u32 soc;

	mutex_lock(&data->lock);
	PCA_DBG("++\n");

	if (!data->inited) {
		rdy = false;
		goto out;
	}

	PCA_DBG("run once(%d)\n", data->run_once);
	if (data->run_once) {
		if (!(data->notify & DV2_RESET_NOTIFY)) {
			rdy = false;
			goto out;
		}
		mutex_lock(&data->notify_lock);
		PCA_INFO("run once but detach/hardreset happened\n");
		data->notify &= ~DV2_RESET_NOTIFY;
		data->run_once = false;
		data->ta_ready = false;
		mutex_unlock(&data->notify_lock);
	}

	ret = prop_chgalgo_get_soc(data->pca_swchg, &soc);
	if (ret < 0) {
		rdy = false;
		goto out;
	}
	if (soc < desc->start_soc_min || soc > desc->start_soc_max) {
		PCA_INFO("soc(%d) not in range(%d~%d)\n", soc,
			 desc->start_soc_min, desc->start_soc_max);
		rdy = false;
		goto out;
	}

	if (!__dv2_is_ta_rdy(info))
		rdy = false;
out:
	mutex_unlock(&data->lock);
	return rdy;
}

static int dv2_start_algo(struct prop_chgalgo_device *pca)
{
	int ret = 0;
	struct dv2_algo_info *info = prop_chgalgo_get_drvdata(pca);
	struct dv2_algo_data *data = info->data;

	mutex_lock(&data->lock);
	PCA_DBG("++\n");
	if (!data->inited || !data->ta_ready)
		goto out;
	ret = __dv2_start(info);
	if (ret < 0)
		PCA_ERR("start dv2 algo fail\n");
out:
	mutex_unlock(&data->lock);
	return ret;
}

static bool dv2_is_algo_running(struct prop_chgalgo_device *pca)
{
	struct dv2_algo_info *info = prop_chgalgo_get_drvdata(pca);
	struct dv2_algo_data *data = info->data;
	bool running = true;

	if (!mutex_trylock(&data->lock))
		goto out;
	if (!data->inited)
		goto out_unlock;

	running = !(data->state == DV2_ALGO_STOP);
	PCA_DBG("running = %d\n", running);
out_unlock:
	mutex_unlock(&data->lock);
out:
	return running;
}

static int dv2_plugout_reset(struct prop_chgalgo_device *pca)
{
	int ret = 0;
	struct dv2_algo_info *info = prop_chgalgo_get_drvdata(pca);
	struct dv2_algo_data *data = info->data;
	struct dv2_stop_info sinfo = {
		.reset_ta = false,
		.hardreset_ta = false,
	};

	mutex_lock(&data->lock);
	PCA_DBG("++\n");
	if (!data->inited)
		goto out;
	ret = __dv2_plugout_reset(info, &sinfo);
out:
	mutex_unlock(&data->lock);
	return ret;
}

static int dv2_stop_algo(struct prop_chgalgo_device *pca, bool rerun)
{
	int ret = 0;
	struct dv2_algo_info *info = prop_chgalgo_get_drvdata(pca);
	struct dv2_algo_data *data = info->data;
	struct dv2_stop_info sinfo = {
		.reset_ta = true,
		.hardreset_ta = false,
	};

	atomic_set(&data->stop_algo, 1);
	mutex_lock(&data->lock);
	PCA_DBG("rerun %d\n", rerun);
	if (!data->inited)
		goto out;
	ret = __dv2_stop(info, &sinfo);
	if (rerun)
		data->run_once = false;
out:
	mutex_unlock(&data->lock);
	return ret;
}

static int dv2_notifier_call(struct prop_chgalgo_device *pca,
			     struct prop_chgalgo_notify *notify)
{
	int ret = 0;
	struct dv2_algo_info *info = prop_chgalgo_get_drvdata(pca);
	struct dv2_algo_data *data = info->data;

	mutex_lock(&data->notify_lock);
	if (data->state == DV2_ALGO_STOP) {
		if ((notify->evt == PCA_NOTIEVT_DETACH ||
		     notify->evt == PCA_NOTIEVT_HARDRESET) && data->run_once) {
			PCA_INFO("detach/hardreset && run once after stop\n");
			data->notify |= BIT(notify->evt);
		}
		goto out;
	}
	PCA_INFO("%d %s\n", notify->src,
		 prop_chgalgo_notify_evt_tostring(notify->evt));
	switch (notify->evt) {
	case PCA_NOTIEVT_DETACH:
	case PCA_NOTIEVT_HARDRESET:
	case PCA_NOTIEVT_VBUSOVP:
	case PCA_NOTIEVT_IBUSOCP:
	case PCA_NOTIEVT_IBUSUCP_FALL:
	case PCA_NOTIEVT_VBATOVP:
	case PCA_NOTIEVT_IBATOCP:
	case PCA_NOTIEVT_VOUTOVP:
	case PCA_NOTIEVT_VDROVP:
	case PCA_NOTIEVT_VBATOVP_ALARM:
	case PCA_NOTIEVT_VBUSOVP_ALARM:
		data->notify |= BIT(notify->evt);
		break;
	default:
		ret = -EINVAL;
		goto out;
	}
	__dv2_wakeup_algo_thread(data);
out:
	mutex_unlock(&data->notify_lock);
	return ret;
}

static int dv2_thermal_throttling(struct prop_chgalgo_device *pca, int mA)
{
	struct dv2_algo_info *info = prop_chgalgo_get_drvdata(pca);
	struct dv2_algo_data *data = info->data;

	PCA_INFO("%d\n", mA);
	mutex_lock(&data->ext_lock);
	data->thermal_throttling = mA;
	__dv2_wakeup_algo_thread(data);
	mutex_unlock(&data->ext_lock);
	return 0;
}

static int dv2_set_jeita_vbat_cv(struct prop_chgalgo_device *pca, int mV)
{
	struct dv2_algo_info *info = prop_chgalgo_get_drvdata(pca);
	struct dv2_algo_data *data = info->data;

	PCA_INFO("%d\n", mV);
	mutex_lock(&data->ext_lock);
	data->jeita_vbat_cv = mV;
	__dv2_wakeup_algo_thread(data);
	mutex_unlock(&data->ext_lock);
	return 0;
}

static struct prop_chgalgo_algo_ops pca_dv2_ops = {
	.init_algo = dv2_init_algo,
	.is_algo_ready = dv2_is_algo_ready,
	.start_algo = dv2_start_algo,
	.is_algo_running = dv2_is_algo_running,
	.plugout_reset = dv2_plugout_reset,
	.stop_algo = dv2_stop_algo,
	.notifier_call = dv2_notifier_call,
	.thermal_throttling = dv2_thermal_throttling,
	.set_jeita_vbat_cv = dv2_set_jeita_vbat_cv,
};

static struct prop_chgalgo_desc pca_dv2_desc = {
	.name = "pca_algo_dv2",
	.type = PCA_DEVTYPE_ALGO,
};

#define DV2_DT_VALPROP_ARR(name, sz) \
	{#name, offsetof(struct dv2_algo_desc, name), sz}

#define DV2_DT_VALPROP(name) \
	DV2_DT_VALPROP_ARR(name, 1)

struct dv2_dtprop {
	const char *name;
	size_t offset;
	size_t sz;
};

static inline void dv2_parse_dt_u32(struct device_node *np, void *desc,
				    const struct dv2_dtprop *props,
				    int prop_cnt)
{
	int i;

	for (i = 0; i < prop_cnt; i++) {
		if (unlikely(!props[i].name))
			continue;
		of_property_read_u32(np, props[i].name, desc + props[i].offset);
	}
}

static inline void dv2_parse_dt_u32_arr(struct device_node *np, void *desc,
					const struct dv2_dtprop *props,
					int prop_cnt)
{
	int i;

	for (i = 0; i < prop_cnt; i++) {
		if (unlikely(!props[i].name))
			continue;
		of_property_read_u32_array(np, props[i].name,
					   desc + props[i].offset, props[i].sz);
	}
}

static inline int __of_property_read_s32_array(const struct device_node *np,
					       const char *propname,
					       s32 *out_values, size_t sz)
{
	return of_property_read_u32_array(np, propname, (u32 *)out_values, sz);
}

static inline void dv2_parse_dt_s32_arr(struct device_node *np, void *desc,
					   const struct dv2_dtprop *props,
					   int prop_cnt)
{
	int i;

	for (i = 0; i < prop_cnt; i++) {
		if (unlikely(!props[i].name))
			continue;
		__of_property_read_s32_array(np, props[i].name,
					     desc + props[i].offset,
					     props[i].sz);
	}
}

static const struct dv2_dtprop dv2_dtprops_u32[] = {
	DV2_DT_VALPROP(polling_interval),
	DV2_DT_VALPROP(ta_cv_ss_repeat_tmin),
	DV2_DT_VALPROP(vbat_cv),
	DV2_DT_VALPROP(start_soc_min),
	DV2_DT_VALPROP(start_soc_max),
	DV2_DT_VALPROP(start_vbat_max),
	DV2_DT_VALPROP(idvchg_term),
	DV2_DT_VALPROP(idvchg_step),
	DV2_DT_VALPROP(idvchg_ss_init),
	DV2_DT_VALPROP(idvchg_ss_step),
	DV2_DT_VALPROP(idvchg_ss_step1),
	DV2_DT_VALPROP(idvchg_ss_step2),
	DV2_DT_VALPROP(idvchg_ss_step1_vbat),
	DV2_DT_VALPROP(idvchg_ss_step2_vbat),
	DV2_DT_VALPROP(ta_blanking),
	DV2_DT_VALPROP(swchg_aicr),
	DV2_DT_VALPROP(swchg_ichg),
	DV2_DT_VALPROP(swchg_aicr_ss_init),
	DV2_DT_VALPROP(swchg_aicr_ss_step),
	DV2_DT_VALPROP(swchg_off_vbat),
	DV2_DT_VALPROP(force_ta_cv_vbat),
	DV2_DT_VALPROP(chg_time_max),
	DV2_DT_VALPROP(tta_recovery_area),
	DV2_DT_VALPROP(tbat_recovery_area),
	DV2_DT_VALPROP(tdvchg_recovery_area),
	DV2_DT_VALPROP(tswchg_recovery_area),
	DV2_DT_VALPROP(ifod_threshold),
	DV2_DT_VALPROP(rsw_min),
	DV2_DT_VALPROP(ircmp_rbat),
	DV2_DT_VALPROP(ircmp_vclamp),
	DV2_DT_VALPROP(vta_cap_min),
	DV2_DT_VALPROP(vta_cap_max),
	DV2_DT_VALPROP(ita_cap_min),
};

static const struct dv2_dtprop dv2_dtprops_u32_array[] = {
	DV2_DT_VALPROP_ARR(ita_level, DV2_RCABLE_MAX),
	DV2_DT_VALPROP_ARR(rcable_level, DV2_RCABLE_MAX),
	DV2_DT_VALPROP_ARR(ita_level_dual, DV2_RCABLE_MAX),
	DV2_DT_VALPROP_ARR(rcable_level_dual, DV2_RCABLE_MAX),
};

static const struct dv2_dtprop dv2_dtprops_s32_array[] = {
	DV2_DT_VALPROP_ARR(tta_level_def, DV2_THERMAL_MAX),
	DV2_DT_VALPROP_ARR(tta_curlmt, DV2_THERMAL_MAX),
	DV2_DT_VALPROP_ARR(tbat_level_def, DV2_THERMAL_MAX),
	DV2_DT_VALPROP_ARR(tbat_curlmt, DV2_THERMAL_MAX),
	DV2_DT_VALPROP_ARR(tdvchg_level_def, DV2_THERMAL_MAX),
	DV2_DT_VALPROP_ARR(tdvchg_curlmt, DV2_THERMAL_MAX),
	DV2_DT_VALPROP_ARR(tswchg_level_def, DV2_THERMAL_MAX),
	DV2_DT_VALPROP_ARR(tswchg_curlmt, DV2_THERMAL_MAX),
};

static const char * const __dv2_dev_node_name[] = {
	"mtk_pe50",
};

static int dv2_parse_dt(struct dv2_algo_info *info)
{
	int i, ret;
	struct dv2_algo_desc *desc;
	struct device_node *np;

	desc = devm_kzalloc(info->dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	info->desc = desc;
	memcpy(desc, &algo_desc_defval, sizeof(*desc));

	for (i = 0; i < ARRAY_SIZE(__dv2_dev_node_name); i++) {
		np = of_find_node_by_name(NULL, __dv2_dev_node_name[i]);
		if (np) {
			PCA_ERR("find node %s\n", __dv2_dev_node_name[i]);
			break;
		}
	}
	if (i == ARRAY_SIZE(__dv2_dev_node_name)) {
		PCA_ERR("no device node found\n");
		return -EINVAL;
	}

	ret = of_property_count_strings(np, "support_ta");
	if (ret < 0)
		return ret;
	desc->support_ta_cnt = ret;
	desc->support_ta = devm_kzalloc(info->dev, ret * sizeof(char *),
					GFP_KERNEL);
	if (!desc->support_ta)
		return -ENOMEM;
	for (i = 0; i < desc->support_ta_cnt; i++) {
		ret = of_property_read_string_index(np, "support_ta", i,
						    &desc->support_ta[i]);
		if (ret < 0)
			return ret;
		PCA_INFO("support ta(%s)\n", desc->support_ta[i]);
	}

	desc->allow_not_check_ta_status =
		of_property_read_bool(np, "allow_not_check_ta_status");
	dv2_parse_dt_u32(np, (void *)desc, dv2_dtprops_u32,
			 ARRAY_SIZE(dv2_dtprops_u32));
	dv2_parse_dt_u32_arr(np, (void *)desc, dv2_dtprops_u32_array,
			     ARRAY_SIZE(dv2_dtprops_u32_array));
	dv2_parse_dt_s32_arr(np, (void *)desc, dv2_dtprops_s32_array,
			     ARRAY_SIZE(dv2_dtprops_s32_array));
	if (desc->swchg_aicr == 0 || desc->swchg_ichg == 0) {
		desc->swchg_aicr = 0;
		desc->swchg_ichg = 0;
	}
	return 0;
}

static int dv2_algo_probe(struct platform_device *pdev)
{
	int ret;
	struct dv2_algo_info *info;
	struct dv2_algo_data *data;

	dev_info(&pdev->dev, "%s(%s)\n", __func__, PCA_DV2_ALGO_VERSION);

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	info->data = data;
	info->dev = &pdev->dev;
	platform_set_drvdata(pdev, info);

	ret = dv2_parse_dt(info);
	if (ret < 0) {
		dev_notice(info->dev, "%s parse dt fail(%d)\n", __func__, ret);
		return ret;
	}

	info->pca = prop_chgalgo_device_register(info->dev, &pca_dv2_desc,
						 NULL, NULL, &pca_dv2_ops,
						 info);
	if (IS_ERR_OR_NULL(info->pca)) {
		dev_notice(info->dev, "%s reg dv2 algo fail(%d)\n", __func__,
			   ret);
		return PTR_ERR(info->pca);
	}

	/* init algo thread & timer */
	mutex_init(&data->notify_lock);
	mutex_init(&data->lock);
	mutex_init(&data->ext_lock);
	init_waitqueue_head(&data->wq);
	atomic_set(&data->wakeup_thread, 0);
	atomic_set(&data->stop_thread, 0);
	data->state = DV2_ALGO_STOP;
	alarm_init(&data->timer, ALARM_REALTIME, __dv2_algo_timer_cb);
	data->task = kthread_run(__dv2_algo_threadfn, info, "dv2_algo_task");
	if (IS_ERR(data->task)) {
		ret = PTR_ERR(data->task);
		dev_notice(info->dev, "%s run task fail(%d)\n", __func__, ret);
		goto err;
	}
	device_init_wakeup(info->dev, true);
	dev_info(info->dev, "%s successfully\n", __func__);
	return 0;

err:
	mutex_destroy(&data->ext_lock);
	mutex_destroy(&data->lock);
	mutex_destroy(&data->notify_lock);
	prop_chgalgo_device_unregister(info->pca);
	return ret;
}

static int dv2_algo_remove(struct platform_device *pdev)
{
	struct dv2_algo_info *info = platform_get_drvdata(pdev);
	struct dv2_algo_data *data;

	if (info) {
		data = info->data;
		atomic_set(&data->stop_thread, 1);
		__dv2_wakeup_algo_thread(data);
		kthread_stop(data->task);
		mutex_destroy(&data->ext_lock);
		mutex_destroy(&data->lock);
		mutex_destroy(&data->notify_lock);
		prop_chgalgo_device_unregister(info->pca);
	}

	return 0;
}

static int __maybe_unused dv2_algo_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct dv2_algo_info *info = platform_get_drvdata(pdev);
	struct dv2_algo_data *data = info->data;

	dev_info(dev, "%s\n", __func__);
	mutex_lock(&data->lock);
	return 0;
}

static int __maybe_unused dv2_algo_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct dv2_algo_info *info = platform_get_drvdata(pdev);
	struct dv2_algo_data *data = info->data;

	dev_info(dev, "%s\n", __func__);
	mutex_unlock(&data->lock);
	return 0;
}

static SIMPLE_DEV_PM_OPS(dv2_algo_pm_ops, dv2_algo_suspend, dv2_algo_resume);

static struct platform_device dv2_algo_platdev = {
	.name = "pca_dv2_algo",
	.id = -1,
};

static struct platform_driver dv2_algo_platdrv = {
	.probe = dv2_algo_probe,
	.remove = dv2_algo_remove,
	.driver = {
		.name = "pca_dv2_algo",
		.owner = THIS_MODULE,
		.pm = &dv2_algo_pm_ops,
	},
};

static int __init dv2_algo_init(void)
{
	platform_device_register(&dv2_algo_platdev);
	return platform_driver_register(&dv2_algo_platdrv);
}
device_initcall_sync(dv2_algo_init);

static void __exit dv2_algo_exit(void)
{
	platform_driver_unregister(&dv2_algo_platdrv);
	platform_device_unregister(&dv2_algo_platdev);
}
module_exit(dv2_algo_exit);

MODULE_DESCRIPTION("Divide By Two Algorithm For PCA");
MODULE_AUTHOR("ShuFanLee <shufan_lee@richtek.com>");
MODULE_VERSION(PCA_DV2_ALGO_VERSION);
MODULE_LICENSE("GPL");

/*
 * 1.0.14
 * (1) For TA CV mode, not to increase vta if ita reaches ita_lmt and
 *     there is no difference in ita's measurement
 *
 * 1.0.13
 * (1) Init DVCHG chip in __dv2_start
 *
 * 1.0.12
 * (1) For TA CV mode, compatible with TA which only accepts request current
 *     greater than a certain value
 *
 * 1.0.11
 * (1) Optimize speed of soft start of TA CV mode
 * (2) Average ita_gap only if new one is larger
 *
 * 1.0.10
 * (1) Add stop_algo flag to speed up the process
 * (2) For estimating power, enable charger after setting ichg/aicr
 * (3) Limit vta to auth_data->vcap_max for set_ta_cap_cv
 * (4) Stop algo without hardreset if hwerr received during set_ta_cap
 *
 * 1.0.9
 * (1) Add S/W IR compensation
 * (2) Skip one run's chance to increase ita after vbat over cv
 *     Always keep cv_lower_bound be 20mV lower than cv
 *
 * 1.0.8
 * (1) Remove DV2_TA_ACCURACY_IMAX, if ita is over spec, use ibus of charger to
 *     confirm the status
 * (2) Add allow_not_check_ta_status property in dtsi
 * (3) Add a rerun paramenter in stop_algo
 *     If it is true, run_once will be cleared
 * (4) In charging flow with ta_cv, always use 500ms polling interval
 * (5) Add algo api for thermal throttling
 * (6) Change auth_data->support_status to auth_data->support_meas_cap
 *     and auth_data->support_status now means status of ta but not cap
 * (7) Ignore measured resistance if measure cap is not supported by TA
 * (8) Try dual dvchg for both cc/cv mode
 *     If error occurs, restart and charge with single dvchg
 * (9) Remove BIF support
 * (10) Move ita_gap_per_step to TA's authentication data,
 *      and average ita_gap when there's a new one
 * (11) If detach/hardreset hannened after ALGO_STOP,
 *      reset run once/ta_ready flag
 * (12) Update vbat_upper_bound to vbat_cv, remove vbus_upper_bound
 * (13) For TA CV mode, not to increase ITA to maximum limited value but
 *      tracking it as close as possible
 * (14) Make sure ita is limited by idvchg_cc if aicr/swchg are both > 0
 * (15) Use MIN(idvchg_cc, ita_max) to calculate dvchg's ibusocp
 * (16) Add algo api for jeita vbat cv
 * (17) Remove cv_readd_cnt used in cc_cv_with_ta_cc
 *
 * 1.0.7
 * (1) Remove enable_power(true) in dv2_algo_init_with_ta_xx
 * (2) If curlmt == 0, ignore temperature checking
 *
 * 1.0.6
 * (1) For ss_dvchg_with_ta_cv, fix idvchg_lmt not initialized,
 *     if vbat > desc->bat_upper_bound
 * (2) Take DVCHG IBUS ADC's accuracy as reference while checking IBUSOCP
 * (3) For set_ta_cap_cc, if (ita_measure > ita_setting + DV2_TA_ACCURACY_IMAX),
 *     this TA is classified as not supporting CC mode
 * (4) Make sure ITAOCP is DV2_TA_ACCURACY_IMAX greater than ita_setting
 *
 * 1.0.5
 * (1) For measure_r, check ibat/ibus/ita are zero or not before calculating
 * (2) For TA CV mode, check if (ita + ita_gap_per_vstep > ita_lmt) before
 *     adding vta
 * (3) Not to check ibusocp/ibatocp before enabling master dvchg
 * (4) For TA CC mode, directly set opt_vta to vta_measure + DV2_VTA_GAP_VMIN
 * (5) For dual DVCHG, set DV2_IBUSOCP_RATIO += 10
 * (6) Show ibus of swchg in dump_charging_info
 *
 * 1.0.4
 * (1) Modify flow of INIT, measure vbus_cali after rising vbus
 * (2) Modify flow of MEASURE_R, calculate R for DV2_ALGO_MEASURE_R_AVG_TIMES
 *     and average after discarding max & min
 * (3) Handle IBUSUCP_FALL from divider charger
 * (4) Use HZ instead of power path in INIT
 * (5) Add flow for TA with CV mode only(with/without status support)
 *
 * 1.0.3
 * (1) If power limited bit is specified by TA(x watts), the output current
 *     limit is calculated as RoundDown(x/requested output voltage) to the
 *     nearest 50 mA
 * (2) Modify set TA cap flow to minimize the gap between set and measure so
 *     that TA is able to provide as much power as possible if it has power
 *     limited
 * (3) Move TA's req istep & vstep to its authentication data
 *
 * 1.0.2
 * (1) Add retry mechanism if rcable is worse than expected
 * (2) Use VBUSOVP_ALM to wakeup thread to adjust VBUS and not to set VBUS using
 *     VBAT = CV directly
 *
 * 1.0.1
 * (1) Add notifier call interface, move out TCPC notifier
 * (2) Set VTA using vbat_upper_bound after entering DV2_ALGO_CC_CV stage
 * (3) Use 500ms as initial polling interval and use polling_interval in desc
 *     after entering DV2_ALGO_CC_CV stage
 * (4) Start to use divider charger's ADC
 * (5) Add enum of DV2_THERMAL_LEVEL and remove TTA/TBAT/TDVCHG_TEMP_LEVEL
 * (6) Move safety check functions to function pointer array
 * (7) Parse support ta count and name from dtsi
 * (8) Add Master/Slave dvchg charging mode
 *
 * 1.0.0
 * Initial Release
 */
