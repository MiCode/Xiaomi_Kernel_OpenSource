/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_HVBP_H
#define __MTK_HVBP_H

#include "mtk_charger_algorithm_class.h"

#define HVBP_ERR_LEVEL	1
#define HVBP_INFO_LEVEL	2
#define HVBP_DBG_LEVEL	3
#define HVBP_ITA_GAP_WINDOW_SIZE	50
#define PRECISION_ENHANCE	5

#define DISABLE_VBAT_THRESHOLD -1

extern int hvbp_get_log_level(void);
#define HVBP_DBG(fmt, ...) \
	do { \
		if (hvbp_get_log_level() >= HVBP_DBG_LEVEL) \
			pr_info("[MTK_HVBP]%s " fmt, __func__, ##__VA_ARGS__); \
	} while (0)

#define HVBP_INFO(fmt, ...) \
	do { \
		if (hvbp_get_log_level() >= HVBP_INFO_LEVEL) \
			pr_info("[MTK_HVBP]%s " fmt, __func__, ##__VA_ARGS__); \
	} while (0)

#define HVBP_ERR(fmt, ...) \
	do { \
		if (hvbp_get_log_level() >= HVBP_ERR_LEVEL) \
			pr_info("[MTK_HVBP]%s " fmt, __func__, ##__VA_ARGS__); \
	} while (0)

enum hvbp_adc_channel {
	HVBP_ADCCHAN_VBUS = 0,
	HVBP_ADCCHAN_IBUS,
	HVBP_ADCCHAN_VBAT,
	HVBP_ADCCHAN_IBAT,
	HVBP_ADCCHAN_TBAT,
	HVBP_ADCCHAN_TCHG,
	HVBP_ADCCHAN_VOUT,
	HVBP_ADCCHAN_VSYS,
	HVBP_ADCCHAN_MAX,
};

enum hvbp_algo_state {
	HVBP_ALGO_INIT = 0,
	HVBP_ALGO_MEASURE_R,
	HVBP_ALGO_SS_SWCHG,
	HVBP_ALGO_SS_DVCHG,
	HVBP_ALGO_CC_CV,
	HVBP_ALGO_STOP,
	HVBP_ALGO_STATE_MAX,
};

enum hvbp_thermal_level {
	HVBP_THERMAL_VERY_COLD = 0,
	HVBP_THERMAL_COLD,
	HVBP_THERMAL_VERY_COOL,
	HVBP_THERMAL_COOL,
	HVBP_THERMAL_NORMAL,
	HVBP_THERMAL_WARM,
	HVBP_THERMAL_VERY_WARM,
	HVBP_THERMAL_HOT,
	HVBP_THERMAL_VERY_HOT,
	HVBP_THERMAL_MAX,
};

enum hvbp_rcable_level {
	HVBP_RCABLE_NORMAL = 0,
	HVBP_RCABLE_BAD1,
	HVBP_RCABLE_BAD2,
	HVBP_RCABLE_BAD3,
	HVBP_RCABLE_MAX,
};

enum hvbp_dvchg_role {
	HVBP_DVCHG_MASTER = 0,
	HVBP_DVCHG_SLAVE,
	HVBP_HVDVCHG_MASTER,
	HVBP_HVDVCHG_SLAVE,
	HVBP_BUCK_BSTCHG,
	HVBP_DVCHG_MAX,
};

struct hvbp_ta_status {
	int temperature;
	bool ocp;
	bool otp;
	bool ovp;
};

struct hvbp_ta_auth_data {
	int vcap_min;
	int vcap_max;
	int icap_min;
	int vta_min;
	int vta_max;
	int ita_max;
	bool pwr_lmt;
	u8 pdp;
	bool support_meas_cap;
	bool support_status;
	bool support_cc;
	u32 vta_step;
	u32 ita_step;
	u32 ita_gap_per_vstep;
};

struct hvbp_algo_data {
	bool is_dvchg_exist[HVBP_DVCHG_MAX];

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
	bool is_dvchg_en[HVBP_DVCHG_MAX];
	bool ignore_ibusucpf;
	bool force_ta_cv;
	bool tried_dual_dvchg;
	bool suspect_ta_cc;
	struct hvbp_ta_auth_data ta_auth_data;
	u32 vta_setting;
	u32 ita_setting;
	u32 vta_measure;
	u32 ita_measure;
	u32 ita_gap_per_vstep;
	u32 ita_gap_window_idx;
	u32 ita_gaps[HVBP_ITA_GAP_WINDOW_SIZE];
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
	enum hvbp_algo_state state;
	enum hvbp_thermal_level tbat_level;
	enum hvbp_thermal_level tta_level;
	enum hvbp_thermal_level tdvchg_level;
	int input_current_limit;
	int cv_limit;
};

/* Setting from dtsi */
struct hvbp_algo_desc {
	u32 polling_interval;		/* polling interval */
	u32 ta_cv_ss_repeat_tmin;	/* min repeat time of ss for TA CV */
	u32 vbat_cv;			/* vbat constant voltage */
	u32 start_soc_min;		/* algo start bat low bound */
	u32 start_soc_max;		/* algo start bat upper bound */
	u32 start_vbat_min;		/* algo start bat low bound */
	u32 start_vbat_max;		/* algo start bat upper bound */
	u32 idvchg_term;		/* terminated current */
	u32 idvchg_step;		/* input current step */
	u32 ita_level[HVBP_RCABLE_MAX];	/* input current */
	u32 rcable_level[HVBP_RCABLE_MAX];	/* cable impedance level */
	u32 ita_level_dual[HVBP_RCABLE_MAX];	/* input current */
	u32 rcable_level_dual[HVBP_RCABLE_MAX];	/* cable impedance level */
	u32 idvchg_ss_init;		/* SS state init input current */
	u32 idvchg_ss_step;		/* SS state input current step */
	u32 idvchg_ss_step1;		/* SS state input current step2 */
	u32 idvchg_ss_step2;		/* SS state input current step3 */
	u32 idvchg_ss_step1_vbat;	/* vbat threshold for ic_ss_step2 */
	u32 idvchg_ss_step2_vbat;	/* vbat threshold for ic_ss_step3 */
	u32 ta_blanking;		/* wait TA stable */
	u32 force_ta_cv_vbat;		/* Force TA using CV mode */
	u32 chg_time_max;		/* max charging time */
	int tta_level_def[HVBP_THERMAL_MAX];	/* TA temp level */
	int tta_curlmt[HVBP_THERMAL_MAX];	/* TA temp current limit */
	int tbat_level_def[HVBP_THERMAL_MAX];	/* BAT temp level */
	int tbat_curlmt[HVBP_THERMAL_MAX];	/* BAT temp current limit */
	int tdvchg_level_def[HVBP_THERMAL_MAX];	/* DVCHG temp level */
	int tdvchg_curlmt[HVBP_THERMAL_MAX];	/* DVCHG temp current limit */
	u32 tta_recovery_area;
	u32 tbat_recovery_area;
	u32 tdvchg_recovery_area;
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

struct hvbp_algo_info {
	struct device *dev;
	struct chg_alg_device *alg;
	struct hvbp_algo_desc *desc;
	struct hvbp_algo_data *data;
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

extern int hvbp_hal_get_ta_output(struct chg_alg_device *alg, int *mV, int *mA);
extern int hvbp_hal_get_ta_status(struct chg_alg_device *alg,
				  struct hvbp_ta_status *status);
extern int hvbp_hal_set_ta_cap(struct chg_alg_device *alg, int mV, int mA);
extern int hvbp_hal_is_ta_cc(struct chg_alg_device *alg, bool *is_cc);
extern int hvbp_hal_set_ta_wdt(struct chg_alg_device *alg, u32 ms);
extern int hvbp_hal_enable_ta_wdt(struct chg_alg_device *alg, bool en);
extern int hvbp_hal_enable_ta_charging(struct chg_alg_device *alg, bool en,
				       int mV, int mA);
extern int hvbp_hal_sync_ta_volt(struct chg_alg_device *alg, u32 mV);
extern int hvbp_hal_authenticate_ta(struct chg_alg_device *alg,
				    struct hvbp_ta_auth_data *data);
extern int hvbp_hal_send_ta_hardreset(struct chg_alg_device *alg);
extern int hvbp_hal_init_hardware(struct chg_alg_device *alg,
				  const char **support_ta, int support_ta_cnt);
extern int hvbp_hal_enable_sw_vbusovp(struct chg_alg_device *alg, bool en);
extern int hvbp_set_operating_mode(struct chg_alg_device *alg,
				   enum chg_idx chgidx, bool en);
extern int hvbp_hal_enable_charging(struct chg_alg_device *alg,
				    enum chg_idx chgidx, bool en);
extern int hvbp_hal_dump_registers(struct chg_alg_device *alg,
				   enum chg_idx chgidx);
extern int hvbp_hal_enable_hz(struct chg_alg_device *alg, enum chg_idx chgidx,
			      bool en);
extern int hvbp_hal_set_vacovp(struct chg_alg_device *alg, enum chg_idx chgidx,
				u32 mV);
extern int hvbp_hal_set_vbusovp(struct chg_alg_device *alg, enum chg_idx chgidx,
				u32 mV);
extern int hvbp_hal_set_ibusocp(struct chg_alg_device *alg, enum chg_idx chgidx,
				u32 mA);
extern int hvbp_hal_set_vbatovp(struct chg_alg_device *alg, enum chg_idx chgidx,
				u32 mV);
extern int hvbp_hal_set_ibatocp(struct chg_alg_device *alg, enum chg_idx chgidx,
				u32 mA);
extern int hvbp_hal_set_vbatovp_alarm(struct chg_alg_device *alg,
				      enum chg_idx chgidx, u32 mV);
extern int hvbp_hal_reset_vbatovp_alarm(struct chg_alg_device *alg,
					enum chg_idx chgidx);
extern int hvbp_hal_set_vbusovp_alarm(struct chg_alg_device *alg,
				      enum chg_idx chgidx, u32 mV);
extern int hvbp_hal_reset_vbusovp_alarm(struct chg_alg_device *alg,
					enum chg_idx chgidx);
extern int hvbp_hal_get_adc(struct chg_alg_device *alg, enum chg_idx chgidx,
			    enum hvbp_adc_channel chan, int *val);
extern int hvbp_hal_get_soc(struct chg_alg_device *alg, u32 *soc);
extern int hvbp_hal_is_pd_adapter_ready(struct chg_alg_device *alg);
extern int hvbp_hal_set_ichg(struct chg_alg_device *alg, enum chg_idx chgidx,
			     u32 mA);
extern int hvbp_hal_set_aicr(struct chg_alg_device *alg, enum chg_idx chgidx,
			     u32 mA);
extern int hvbp_hal_get_ichg(struct chg_alg_device *alg, enum chg_idx chgidx,
			     u32 *mA);
extern int hvbp_hal_get_aicr(struct chg_alg_device *alg, enum chg_idx chgidx,
			     u32 *mA);
extern int hvbp_hal_is_vbuslowerr(struct chg_alg_device *alg,
				  enum chg_idx chgidx, bool *err);
extern int hvbp_hal_get_adc_accuracy(struct chg_alg_device *alg,
				     enum chg_idx chgidx,
				     enum hvbp_adc_channel chan, int *val);
extern int hvbp_hal_init_chip(struct chg_alg_device *alg, enum chg_idx chgidx);
#endif /* __MTK_HVBP_H */
