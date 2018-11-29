/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 The Linux Foundation. All rights reserved.
 */

#ifndef __QG_CORE_H__
#define __QG_CORE_H__

#include <linux/kernel.h>
#include "fg-alg.h"
#include "qg-defs.h"

struct qg_batt_props {
	const char		*batt_type_str;
	int			float_volt_uv;
	int			vbatt_full_mv;
	int			fastchg_curr_ma;
	int			qg_profile_version;
};

struct qg_irq_info {
	const char		*name;
	const irq_handler_t	handler;
	const bool		wake;
	int			irq;
};

struct qg_dt {
	int			vbatt_empty_mv;
	int			vbatt_empty_cold_mv;
	int			vbatt_low_mv;
	int			vbatt_low_cold_mv;
	int			vbatt_cutoff_mv;
	int			iterm_ma;
	int			s2_fifo_length;
	int			s2_vbat_low_fifo_length;
	int			s2_acc_length;
	int			s2_acc_intvl_ms;
	int			ocv_timer_expiry_min;
	int			ocv_tol_threshold_uv;
	int			s3_entry_fifo_length;
	int			s3_entry_ibat_ua;
	int			s3_exit_ibat_ua;
	int			delta_soc;
	int			rbat_conn_mohm;
	int			ignore_shutdown_soc_secs;
	int			shutdown_temp_diff;
	int			cold_temp_threshold;
	int			esr_qual_i_ua;
	int			esr_qual_v_uv;
	int			esr_disable_soc;
	int			esr_min_ibat_ua;
	int			shutdown_soc_threshold;
	bool			hold_soc_while_full;
	bool			linearize_soc;
	bool			cl_disable;
	bool			cl_feedback_on;
	bool			esr_disable;
	bool			esr_discharge_enable;
	bool			qg_ext_sense;
};

struct qg_esr_data {
	u32			pre_esr_v;
	u32			pre_esr_i;
	u32			post_esr_v;
	u32			post_esr_i;
	u32			esr;
	bool			valid;
};

struct qpnp_qg {
	struct device		*dev;
	struct pmic_revid_data	*pmic_rev_id;
	struct regmap		*regmap;
	struct qpnp_vadc_chip	*vadc_dev;
	struct power_supply	*qg_psy;
	struct class		*qg_class;
	struct device		*qg_device;
	struct cdev		qg_cdev;
	struct dentry		*dfs_root;
	dev_t			dev_no;
	struct work_struct	udata_work;
	struct work_struct	scale_soc_work;
	struct work_struct	qg_status_change_work;
	struct notifier_block	nb;
	struct mutex		bus_lock;
	struct mutex		data_lock;
	struct mutex		soc_lock;
	wait_queue_head_t	qg_wait_q;
	struct votable		*awake_votable;
	struct votable		*vbatt_irq_disable_votable;
	struct votable		*fifo_irq_disable_votable;
	struct votable		*good_ocv_irq_disable_votable;
	u32			qg_base;
	u8			qg_subtype;

	/* local data variables */
	u32			batt_id_ohm;
	struct qg_kernel_data	kdata;
	struct qg_user_data	udata;
	struct power_supply	*batt_psy;
	struct power_supply	*usb_psy;
	struct power_supply	*parallel_psy;
	struct qg_esr_data	esr_data[QG_MAX_ESR_COUNT];

	/* status variable */
	u32			*debug_mask;
	bool			qg_device_open;
	bool			profile_loaded;
	bool			battery_missing;
	bool			data_ready;
	bool			suspend_data;
	bool			vbat_low;
	bool			charge_done;
	bool			parallel_enabled;
	bool			usb_present;
	bool			charge_full;
	bool			force_soc;
	int			charge_status;
	int			charge_type;
	int			chg_iterm_ma;
	int			next_wakeup_ms;
	int			esr_actual;
	int			esr_nominal;
	int			soh;
	int			soc_reporting_ready;
	u32			fifo_done_count;
	u32			wa_flags;
	u32			seq_no;
	u32			charge_counter_uah;
	u32			esr_avg;
	u32			esr_last;
	ktime_t			last_user_update_time;
	ktime_t			last_fifo_update_time;
	unsigned long		last_maint_soc_update_time;
	struct iio_channel	*batt_therm_chan;
	struct iio_channel	*batt_id_chan;

	/* soc params */
	int			catch_up_soc;
	int			maint_soc;
	int			msoc;
	int			pon_soc;
	int			batt_soc;
	int			cc_soc;
	int			full_soc;
	int			sys_soc;
	int			last_adj_ssoc;
	int			recharge_soc;
	struct alarm		alarm_timer;
	u32			sdam_data[SDAM_MAX];

	/* DT */
	struct qg_dt		dt;
	struct qg_batt_props	bp;
	/* capacity learning */
	struct cap_learning	*cl;
	/* charge counter */
	struct cycle_counter	*counter;
	/* ttf */
	struct ttf		*ttf;
};

struct ocv_all {
	u32 ocv_uv;
	u32 ocv_raw;
	char ocv_type[20];
};

enum ocv_type {
	S7_PON_OCV,
	S3_GOOD_OCV,
	S3_LAST_OCV,
	SDAM_PON_OCV,
	PON_OCV_MAX,
};

enum debug_mask {
	QG_DEBUG_PON		= BIT(0),
	QG_DEBUG_PROFILE	= BIT(1),
	QG_DEBUG_DEVICE		= BIT(2),
	QG_DEBUG_STATUS		= BIT(3),
	QG_DEBUG_FIFO		= BIT(4),
	QG_DEBUG_IRQ		= BIT(5),
	QG_DEBUG_SOC		= BIT(6),
	QG_DEBUG_PM		= BIT(7),
	QG_DEBUG_BUS_READ	= BIT(8),
	QG_DEBUG_BUS_WRITE	= BIT(9),
	QG_DEBUG_ALG_CL		= BIT(10),
	QG_DEBUG_ESR		= BIT(11),
};

enum qg_irq {
	QG_BATT_MISSING_IRQ,
	QG_VBATT_LOW_IRQ,
	QG_VBATT_EMPTY_IRQ,
	QG_FIFO_UPDATE_DONE_IRQ,
	QG_GOOD_OCV_IRQ,
	QG_FSM_STAT_CHG_IRQ,
	QG_EVENT_IRQ,
	QG_MAX_IRQ,
};

enum qg_wa_flags {
	QG_VBAT_LOW_WA = BIT(0),
	QG_RECHARGE_SOC_WA = BIT(1),
	QG_CLK_ADJUST_WA = BIT(2),
	QG_PON_OCV_WA = BIT(3),
};


#endif /* __QG_CORE_H__ */
