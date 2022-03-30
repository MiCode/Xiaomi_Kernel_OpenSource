/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_PE5_H
#define __MTK_PE5_H

#include "mtk_charger_algorithm_class.h"

#define PE50_ERR_LEVEL	1
#define PE50_INFO_LEVEL	2
#define PE50_DBG_LEVEL	3
#define PE50_ITA_GAP_WINDOW_SIZE	50
#define PRECISION_ENHANCE	5

#define DISABLE_VBAT_THRESHOLD -1

extern int pe50_get_log_level(void);
#define PE50_DBG(fmt, ...) \
	do { \
		if (pe50_get_log_level() >= PE50_DBG_LEVEL) \
			pr_info("[PE50]%s " fmt, __func__, ##__VA_ARGS__); \
	} while (0)

#define PE50_INFO(fmt, ...) \
	do { \
		if (pe50_get_log_level() >= PE50_INFO_LEVEL) \
			pr_info("[PE50]%s " fmt, __func__, ##__VA_ARGS__); \
	} while (0)

#define PE50_ERR(fmt, ...) \
	do { \
		if (pe50_get_log_level() >= PE50_ERR_LEVEL) \
			pr_info("[PE50]%s " fmt, __func__, ##__VA_ARGS__); \
	} while (0)

enum pe50_adc_channel {
	PE50_ADCCHAN_VBUS = 0,
	PE50_ADCCHAN_IBUS,
	PE50_ADCCHAN_VBAT,
	PE50_ADCCHAN_IBAT,
	PE50_ADCCHAN_TBAT,
	PE50_ADCCHAN_TCHG,
	PE50_ADCCHAN_VOUT,
	PE50_ADCCHAN_VSYS,
	PE50_ADCCHAN_MAX,
};

enum pe50_algo_state {
	PE50_ALGO_INIT = 0,
	PE50_ALGO_MEASURE_R,
	PE50_ALGO_SS_SWCHG,
	PE50_ALGO_SS_DVCHG,
	PE50_ALGO_CC_CV,
	PE50_ALGO_STOP,
	PE50_ALGO_STATE_MAX,
};

enum pe50_thermal_level {
	PE50_THERMAL_VERY_COLD = 0,
	PE50_THERMAL_COLD,
	PE50_THERMAL_VERY_COOL,
	PE50_THERMAL_COOL,
	PE50_THERMAL_NORMAL,
	PE50_THERMAL_WARM,
	PE50_THERMAL_VERY_WARM,
	PE50_THERMAL_HOT,
	PE50_THERMAL_VERY_HOT,
	PE50_THERMAL_MAX,
};

enum pe50_rcable_level {
	PE50_RCABLE_NORMAL = 0,
	PE50_RCABLE_BAD1,
	PE50_RCABLE_BAD2,
	PE50_RCABLE_BAD3,
	PE50_RCABLE_MAX,
};

enum pe50_dvchg_role {
	PE50_DVCHG_MASTER = 0,
	PE50_DVCHG_SLAVE,
	PE50_DVCHG_MAX,
};

struct pe50_ta_status {
	int temperature;
	bool ocp;
	bool otp;
	bool ovp;
};

struct pe50_ta_auth_data {
	int vcap_min;
	int vcap_max;
	int icap_min;
	int vta_min;
	int vta_max;
	int ita_max;
	int ita_min;
	bool pwr_lmt;
	u8 pdp;
	bool support_meas_cap;
	bool support_status;
	bool support_cc;
	u32 vta_step;
	u32 ita_step;
	u32 ita_gap_per_vstep;
};

struct pe50_algo_data {
	bool is_dvchg_exist[PE50_DVCHG_MAX];

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
	bool is_dvchg_en[PE50_DVCHG_MAX];
	bool ignore_ibusucpf;
	bool force_ta_cv;
	bool tried_dual_dvchg;
	bool suspect_ta_cc;
	struct pe50_ta_auth_data ta_auth_data;
	u32 vta_setting;
	u32 ita_setting;
	u32 vta_measure;
	u32 ita_measure;
	u32 ita_gap_per_vstep;
	u32 ita_gap_window_idx;
	u32 ita_gaps[PE50_ITA_GAP_WINDOW_SIZE];
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
	u32 r_cable_by_swchg;
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
	int vbat_threshold; /* For checking Ready */
	int ref_vbat; /* Vbat with cable in */
	bool is_vbat_over_cv;
	ktime_t stime;
	enum pe50_algo_state state;
	enum pe50_thermal_level tbat_level;
	enum pe50_thermal_level tta_level;
	enum pe50_thermal_level tdvchg_level;
	enum pe50_thermal_level tswchg_level;
	int input_current_limit;
	int cv_limit;
};

/* Setting from dtsi */
struct pe50_algo_desc {
	u32 polling_interval;		/* polling interval */
	u32 ta_cv_ss_repeat_tmin;	/* min repeat time of ss for TA CV */
	u32 vbat_cv;			/* vbat constant voltage */
	u32 start_soc_min;		/* algo start bat low bound */
	u32 start_soc_max;		/* algo start bat upper bound */
	u32 start_vbat_max;		/* algo start bat upper bound */
	u32 idvchg_term;		/* terminated current */
	u32 idvchg_step;		/* input current step */
	u32 ita_level[PE50_RCABLE_MAX];	/* input current */
	u32 rcable_level[PE50_RCABLE_MAX];	/* cable impedance level */
	u32 ita_level_dual[PE50_RCABLE_MAX];	/* input current */
	u32 rcable_level_dual[PE50_RCABLE_MAX];	/* cable impedance level */
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
	int tta_level_def[PE50_THERMAL_MAX];	/* TA temp level */
	int tta_curlmt[PE50_THERMAL_MAX];	/* TA temp current limit */
	int tbat_level_def[PE50_THERMAL_MAX];	/* BAT temp level */
	int tbat_curlmt[PE50_THERMAL_MAX];	/* BAT temp current limit */
	int tdvchg_level_def[PE50_THERMAL_MAX];	/* DVCHG temp level */
	int tdvchg_curlmt[PE50_THERMAL_MAX];	/* DVCHG temp current limit */
	int tswchg_level_def[PE50_THERMAL_MAX];	/* SWCHG temp level */
	int tswchg_curlmt[PE50_THERMAL_MAX];	/* SWCHG temp current limit */
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

struct pe50_algo_info {
	struct device *dev;
	struct chg_alg_device *alg;
	struct pe50_algo_desc *desc;
	struct pe50_algo_data *data;
};

static inline u32 precise_div(u64 dividend, u64 divisor)
{
	u64 _val = div64_u64(dividend << PRECISION_ENHANCE, divisor);

	return (u32)((_val + (1 << (PRECISION_ENHANCE - 1))) >>
		PRECISION_ENHANCE);
}

static inline u32 percent(u32 val, u32 percent)
{
	return precise_div((u64)val * percent, 100);
}

static inline u32 div1000(u32 val)
{
	return precise_div(val, 1000);
}

static inline u32 milli_to_micro(u32 val)
{
	return val * 1000;
}

static inline int micro_to_milli(int val)
{
	return (val < 0) ? -1 : div1000(val);
}

extern int pe50_hal_get_ta_output(struct chg_alg_device *alg, int *mV, int *mA);
extern int pe50_hal_get_ta_status(struct chg_alg_device *alg,
				  struct pe50_ta_status *status);
extern int pe50_hal_set_ta_cap(struct chg_alg_device *alg, int mV, int mA);
extern int pe50_hal_is_ta_cc(struct chg_alg_device *alg, bool *is_cc);
extern int pe50_hal_set_ta_wdt(struct chg_alg_device *alg, u32 ms);
extern int pe50_hal_enable_ta_wdt(struct chg_alg_device *alg, bool en);
extern int pe50_hal_enable_ta_charging(struct chg_alg_device *alg, bool en,
				       int mV, int mA);
extern int pe50_hal_sync_ta_volt(struct chg_alg_device *alg, u32 mV);
extern int pe50_hal_authenticate_ta(struct chg_alg_device *alg,
				    struct pe50_ta_auth_data *data);
extern int pe50_hal_send_ta_hardreset(struct chg_alg_device *alg);
extern int pe50_hal_init_hardware(struct chg_alg_device *alg,
				  const char **support_ta, int support_ta_cnt);
extern int pe50_hal_enable_sw_vbusovp(struct chg_alg_device *alg, bool en);
extern int pe50_hal_enable_charging(struct chg_alg_device *alg,
				    enum chg_idx chgidx, bool en);
extern int pe50_hal_enable_chip(struct chg_alg_device *alg, enum chg_idx chgidx,
				bool en);
extern int pe50_hal_enable_hz(struct chg_alg_device *alg, enum chg_idx chgidx,
			      bool en);
extern int pe50_hal_set_vbusovp(struct chg_alg_device *alg, enum chg_idx chgidx,
				u32 mV);
extern int pe50_hal_set_ibusocp(struct chg_alg_device *alg, enum chg_idx chgidx,
				u32 mA);
extern int pe50_hal_set_vbatovp(struct chg_alg_device *alg, enum chg_idx chgidx,
				u32 mV);
extern int pe50_hal_set_ibatocp(struct chg_alg_device *alg, enum chg_idx chgidx,
				u32 mA);
extern int pe50_hal_set_vbatovp_alarm(struct chg_alg_device *alg,
				      enum chg_idx chgidx, u32 mV);
extern int pe50_hal_reset_vbatovp_alarm(struct chg_alg_device *alg,
					enum chg_idx chgidx);
extern int pe50_hal_set_vbusovp_alarm(struct chg_alg_device *alg,
				      enum chg_idx chgidx, u32 mV);
extern int pe50_hal_reset_vbusovp_alarm(struct chg_alg_device *alg,
					enum chg_idx chgidx);
extern int pe50_hal_get_adc(struct chg_alg_device *alg, enum chg_idx chgidx,
			    enum pe50_adc_channel chan, int *val);
extern int pe50_hal_get_soc(struct chg_alg_device *alg, u32 *soc);
extern int pe50_hal_is_pd_adapter_ready(struct chg_alg_device *alg);
extern int pe50_hal_set_ichg(struct chg_alg_device *alg, enum chg_idx chgidx,
			     u32 mA);
extern int pe50_hal_set_aicr(struct chg_alg_device *alg, enum chg_idx chgidx,
			     u32 mA);
extern int pe50_hal_get_ichg(struct chg_alg_device *alg, enum chg_idx chgidx,
			     u32 *mA);
extern int pe50_hal_get_aicr(struct chg_alg_device *alg, enum chg_idx chgidx,
			     u32 *mA);
extern int pe50_hal_is_vbuslowerr(struct chg_alg_device *alg,
				  enum chg_idx chgidx, bool *err);
extern int pe50_hal_get_adc_accuracy(struct chg_alg_device *alg,
				     enum chg_idx chgidx,
				     enum pe50_adc_channel chan, int *val);
extern int pe50_hal_init_chip(struct chg_alg_device *alg, enum chg_idx chgidx);
#endif /* __MTK_PE5_H */
