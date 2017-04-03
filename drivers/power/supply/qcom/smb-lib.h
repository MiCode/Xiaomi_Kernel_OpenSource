/* Copyright (c) 2016-2017 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SMB2_CHARGER_H
#define __SMB2_CHARGER_H
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/consumer.h>
#include <linux/extcon.h>
#include "storm-watch.h"

enum print_reason {
	PR_INTERRUPT	= BIT(0),
	PR_REGISTER	= BIT(1),
	PR_MISC		= BIT(2),
	PR_PARALLEL	= BIT(3),
	PR_OTG		= BIT(4),
};

#define DEFAULT_VOTER			"DEFAULT_VOTER"
#define USER_VOTER			"USER_VOTER"
#define PD_VOTER			"PD_VOTER"
#define DCP_VOTER			"DCP_VOTER"
#define PL_USBIN_USBIN_VOTER		"PL_USBIN_USBIN_VOTER"
#define USB_PSY_VOTER			"USB_PSY_VOTER"
#define PL_TAPER_WORK_RUNNING_VOTER	"PL_TAPER_WORK_RUNNING_VOTER"
#define PL_INDIRECT_VOTER		"PL_INDIRECT_VOTER"
#define USBIN_I_VOTER			"USBIN_I_VOTER"
#define USBIN_V_VOTER			"USBIN_V_VOTER"
#define CHG_STATE_VOTER			"CHG_STATE_VOTER"
#define TYPEC_SRC_VOTER			"TYPEC_SRC_VOTER"
#define TAPER_END_VOTER			"TAPER_END_VOTER"
#define THERMAL_DAEMON_VOTER		"THERMAL_DAEMON_VOTER"
#define CC_DETACHED_VOTER		"CC_DETACHED_VOTER"
#define HVDCP_TIMEOUT_VOTER		"HVDCP_TIMEOUT_VOTER"
#define PD_DISALLOWED_INDIRECT_VOTER	"PD_DISALLOWED_INDIRECT_VOTER"
#define PD_HARD_RESET_VOTER		"PD_HARD_RESET_VOTER"
#define VBUS_CC_SHORT_VOTER		"VBUS_CC_SHORT_VOTER"
#define PD_INACTIVE_VOTER		"PD_INACTIVE_VOTER"
#define BOOST_BACK_VOTER		"BOOST_BACK_VOTER"
#define HVDCP_INDIRECT_VOTER		"HVDCP_INDIRECT_VOTER"
#define MICRO_USB_VOTER			"MICRO_USB_VOTER"
#define DEBUG_BOARD_VOTER		"DEBUG_BOARD_VOTER"
#define PD_SUSPEND_SUPPORTED_VOTER	"PD_SUSPEND_SUPPORTED_VOTER"
#define PL_DELAY_HVDCP_VOTER		"PL_DELAY_HVDCP_VOTER"
#define CTM_VOTER			"CTM_VOTER"
#define SW_QC3_VOTER			"SW_QC3_VOTER"
#define AICL_RERUN_VOTER		"AICL_RERUN_VOTER"
#define LEGACY_UNKNOWN_VOTER		"LEGACY_UNKNOWN_VOTER"

#define VCONN_MAX_ATTEMPTS	3
#define OTG_MAX_ATTEMPTS	3

enum smb_mode {
	PARALLEL_MASTER = 0,
	PARALLEL_SLAVE,
	NUM_MODES,
};

enum cc2_sink_type {
	CC2_SINK_NONE = 0,
	CC2_SINK_STD,
	CC2_SINK_MEDIUM_HIGH,
	CC2_SINK_WA_DONE,
};

enum {
	QC_CHARGER_DETECTION_WA_BIT	= BIT(0),
	BOOST_BACK_WA			= BIT(1),
	TYPEC_CC2_REMOVAL_WA_BIT	= BIT(2),
	QC_AUTH_INTERRUPT_WA_BIT	= BIT(3),
	OTG_WA				= BIT(4),
};

enum smb_irq_index {
	CHG_ERROR_IRQ = 0,
	CHG_STATE_CHANGE_IRQ,
	STEP_CHG_STATE_CHANGE_IRQ,
	STEP_CHG_SOC_UPDATE_FAIL_IRQ,
	STEP_CHG_SOC_UPDATE_REQ_IRQ,
	OTG_FAIL_IRQ,
	OTG_OVERCURRENT_IRQ,
	OTG_OC_DIS_SW_STS_IRQ,
	TESTMODE_CHANGE_DET_IRQ,
	BATT_TEMP_IRQ,
	BATT_OCP_IRQ,
	BATT_OV_IRQ,
	BATT_LOW_IRQ,
	BATT_THERM_ID_MISS_IRQ,
	BATT_TERM_MISS_IRQ,
	USBIN_COLLAPSE_IRQ,
	USBIN_LT_3P6V_IRQ,
	USBIN_UV_IRQ,
	USBIN_OV_IRQ,
	USBIN_PLUGIN_IRQ,
	USBIN_SRC_CHANGE_IRQ,
	USBIN_ICL_CHANGE_IRQ,
	TYPE_C_CHANGE_IRQ,
	DCIN_COLLAPSE_IRQ,
	DCIN_LT_3P6V_IRQ,
	DCIN_UV_IRQ,
	DCIN_OV_IRQ,
	DCIN_PLUGIN_IRQ,
	DIV2_EN_DG_IRQ,
	DCIN_ICL_CHANGE_IRQ,
	WDOG_SNARL_IRQ,
	WDOG_BARK_IRQ,
	AICL_FAIL_IRQ,
	AICL_DONE_IRQ,
	HIGH_DUTY_CYCLE_IRQ,
	INPUT_CURRENT_LIMIT_IRQ,
	TEMPERATURE_CHANGE_IRQ,
	SWITCH_POWER_OK_IRQ,
	SMB_IRQ_MAX,
};

struct smb_irq_info {
	const char			*name;
	const irq_handler_t		handler;
	const bool			wake;
	const struct storm_watch	storm_data;
	struct smb_irq_data		*irq_data;
	int				irq;
};

static const unsigned int smblib_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

struct smb_regulator {
	struct regulator_dev	*rdev;
	struct regulator_desc	rdesc;
};

struct smb_irq_data {
	void			*parent_data;
	const char		*name;
	struct storm_watch	storm_data;
};

struct smb_chg_param {
	const char	*name;
	u16		reg;
	int		min_u;
	int		max_u;
	int		step_u;
	int		(*get_proc)(struct smb_chg_param *param,
				    u8 val_raw);
	int		(*set_proc)(struct smb_chg_param *param,
				    int val_u,
				    u8 *val_raw);
};

struct smb_chg_freq {
	unsigned int		freq_5V;
	unsigned int		freq_6V_8V;
	unsigned int		freq_9V;
	unsigned int		freq_12V;
	unsigned int		freq_removal;
	unsigned int		freq_below_otg_threshold;
	unsigned int		freq_above_otg_threshold;
};

struct smb_params {
	struct smb_chg_param	fcc;
	struct smb_chg_param	fv;
	struct smb_chg_param	usb_icl;
	struct smb_chg_param	icl_stat;
	struct smb_chg_param	otg_cl;
	struct smb_chg_param	dc_icl;
	struct smb_chg_param	dc_icl_pt_lv;
	struct smb_chg_param	dc_icl_pt_hv;
	struct smb_chg_param	dc_icl_div2_lv;
	struct smb_chg_param	dc_icl_div2_mid_lv;
	struct smb_chg_param	dc_icl_div2_mid_hv;
	struct smb_chg_param	dc_icl_div2_hv;
	struct smb_chg_param	jeita_cc_comp;
	struct smb_chg_param	step_soc_threshold[4];
	struct smb_chg_param	step_soc;
	struct smb_chg_param	step_cc_delta[5];
	struct smb_chg_param	freq_buck;
	struct smb_chg_param	freq_boost;
};

struct parallel_params {
	struct power_supply	*psy;
};

struct smb_iio {
	struct iio_channel	*temp_chan;
	struct iio_channel	*temp_max_chan;
	struct iio_channel	*usbin_i_chan;
	struct iio_channel	*usbin_v_chan;
	struct iio_channel	*batt_i_chan;
	struct iio_channel	*connector_temp_chan;
	struct iio_channel	*connector_temp_thr1_chan;
	struct iio_channel	*connector_temp_thr2_chan;
	struct iio_channel	*connector_temp_thr3_chan;
};

struct reg_info {
	u16		reg;
	u8		mask;
	u8		val;
	u8		bak;
	const char	*desc;
};

struct smb_charger {
	struct device		*dev;
	char			*name;
	struct regmap		*regmap;
	struct smb_irq_info	*irq_info;
	struct smb_params	param;
	struct smb_iio		iio;
	int			*debug_mask;
	enum smb_mode		mode;
	bool			external_vconn;
	struct smb_chg_freq	chg_freq;
	int			smb_version;

	/* locks */
	struct mutex		write_lock;
	struct mutex		ps_change_lock;
	struct mutex		otg_oc_lock;

	/* power supplies */
	struct power_supply		*batt_psy;
	struct power_supply		*usb_psy;
	struct power_supply		*dc_psy;
	struct power_supply		*bms_psy;
	struct power_supply_desc	usb_psy_desc;
	struct power_supply		*usb_main_psy;

	/* notifiers */
	struct notifier_block	nb;

	/* parallel charging */
	struct parallel_params	pl;

	/* regulators */
	struct smb_regulator	*vbus_vreg;
	struct smb_regulator	*vconn_vreg;
	struct regulator	*dpdm_reg;

	/* votables */
	struct votable		*dc_suspend_votable;
	struct votable		*fcc_votable;
	struct votable		*fv_votable;
	struct votable		*usb_icl_votable;
	struct votable		*dc_icl_votable;
	struct votable		*pd_disallowed_votable_indirect;
	struct votable		*pd_allowed_votable;
	struct votable		*awake_votable;
	struct votable		*pl_disable_votable;
	struct votable		*chg_disable_votable;
	struct votable		*pl_enable_votable_indirect;
	struct votable		*hvdcp_disable_votable_indirect;
	struct votable		*hvdcp_enable_votable;
	struct votable		*apsd_disable_votable;
	struct votable		*hvdcp_hw_inov_dis_votable;

	/* work */
	struct work_struct	bms_update_work;
	struct work_struct	rdstd_cc2_detach_work;
	struct delayed_work	hvdcp_detect_work;
	struct delayed_work	ps_change_timeout_work;
	struct delayed_work	step_soc_req_work;
	struct delayed_work	clear_hdc_work;
	struct work_struct	otg_oc_work;
	struct work_struct	vconn_oc_work;
	struct delayed_work	otg_ss_done_work;
	struct delayed_work	icl_change_work;

	/* cached status */
	int			voltage_min_uv;
	int			voltage_max_uv;
	int			pd_active;
	bool			system_suspend_supported;
	int			boost_threshold_ua;
	int			system_temp_level;
	int			thermal_levels;
	int			*thermal_mitigation;
	int			dcp_icl_ua;
	int			fake_capacity;
	bool			step_chg_enabled;
	bool			is_hdc;
	bool			chg_done;
	bool			micro_usb_mode;
	bool			otg_en;
	bool			vconn_en;
	bool			suspend_input_on_debug_batt;
	int			otg_attempts;
	int			vconn_attempts;
	int			default_icl_ua;
	int			otg_cl_ua;

	/* workaround flag */
	u32			wa_flags;
	enum cc2_sink_type	cc2_sink_detach_flag;
	int			boost_current_ua;
	int			temp_speed_reading_count;

	/* extcon for VBUS / ID notification to USB for uUSB */
	struct extcon_dev	*extcon;

	int			icl_reduction_ua;

	/* qnovo */
	int			qnovo_fcc_ua;
	int			qnovo_fv_uv;
	int			usb_icl_delta_ua;
	int			pulse_cnt;
};

int smblib_read(struct smb_charger *chg, u16 addr, u8 *val);
int smblib_masked_write(struct smb_charger *chg, u16 addr, u8 mask, u8 val);
int smblib_write(struct smb_charger *chg, u16 addr, u8 val);

int smblib_get_charge_param(struct smb_charger *chg,
			    struct smb_chg_param *param, int *val_u);
int smblib_get_usb_suspend(struct smb_charger *chg, int *suspend);

int smblib_enable_charging(struct smb_charger *chg, bool enable);
int smblib_set_charge_param(struct smb_charger *chg,
			    struct smb_chg_param *param, int val_u);
int smblib_set_usb_suspend(struct smb_charger *chg, bool suspend);
int smblib_set_dc_suspend(struct smb_charger *chg, bool suspend);

int smblib_mapping_soc_from_field_value(struct smb_chg_param *param,
					     int val_u, u8 *val_raw);
int smblib_mapping_cc_delta_to_field_value(struct smb_chg_param *param,
					   u8 val_raw);
int smblib_mapping_cc_delta_from_field_value(struct smb_chg_param *param,
					     int val_u, u8 *val_raw);
int smblib_set_chg_freq(struct smb_chg_param *param,
				int val_u, u8 *val_raw);

int smblib_vbus_regulator_enable(struct regulator_dev *rdev);
int smblib_vbus_regulator_disable(struct regulator_dev *rdev);
int smblib_vbus_regulator_is_enabled(struct regulator_dev *rdev);

int smblib_vconn_regulator_enable(struct regulator_dev *rdev);
int smblib_vconn_regulator_disable(struct regulator_dev *rdev);
int smblib_vconn_regulator_is_enabled(struct regulator_dev *rdev);

irqreturn_t smblib_handle_debug(int irq, void *data);
irqreturn_t smblib_handle_otg_overcurrent(int irq, void *data);
irqreturn_t smblib_handle_chg_state_change(int irq, void *data);
irqreturn_t smblib_handle_step_chg_state_change(int irq, void *data);
irqreturn_t smblib_handle_step_chg_soc_update_fail(int irq, void *data);
irqreturn_t smblib_handle_step_chg_soc_update_request(int irq, void *data);
irqreturn_t smblib_handle_batt_temp_changed(int irq, void *data);
irqreturn_t smblib_handle_batt_psy_changed(int irq, void *data);
irqreturn_t smblib_handle_usb_psy_changed(int irq, void *data);
irqreturn_t smblib_handle_usbin_uv(int irq, void *data);
irqreturn_t smblib_handle_usb_plugin(int irq, void *data);
irqreturn_t smblib_handle_usb_source_change(int irq, void *data);
irqreturn_t smblib_handle_icl_change(int irq, void *data);
irqreturn_t smblib_handle_usb_typec_change(int irq, void *data);
irqreturn_t smblib_handle_dc_plugin(int irq, void *data);
irqreturn_t smblib_handle_high_duty_cycle(int irq, void *data);
irqreturn_t smblib_handle_switcher_power_ok(int irq, void *data);
irqreturn_t smblib_handle_wdog_bark(int irq, void *data);

int smblib_get_prop_input_suspend(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_batt_present(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_batt_capacity(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_batt_status(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_batt_charge_type(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_batt_charge_done(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_batt_health(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_system_temp_level(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_input_current_limited(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_batt_voltage_now(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_batt_current_now(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_batt_temp(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_step_chg_step(struct smb_charger *chg,
				union power_supply_propval *val);

int smblib_set_prop_input_suspend(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_batt_capacity(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_system_temp_level(struct smb_charger *chg,
				const union power_supply_propval *val);

int smblib_get_prop_dc_present(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_dc_online(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_dc_current_max(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_set_prop_dc_current_max(struct smb_charger *chg,
				const union power_supply_propval *val);

int smblib_get_prop_usb_present(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_usb_online(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_usb_suspend(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_usb_voltage_now(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_pd_current_max(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_usb_current_max(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_usb_current_now(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_typec_cc_orientation(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_typec_mode(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_typec_power_role(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_pd_allowed(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_input_current_settled(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_input_voltage_settled(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_pd_in_hard_reset(struct smb_charger *chg,
			       union power_supply_propval *val);
int smblib_get_pe_start(struct smb_charger *chg,
			       union power_supply_propval *val);
int smblib_get_prop_charger_temp(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_charger_temp_max(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_die_health(struct smb_charger *chg,
			       union power_supply_propval *val);
int smblib_get_prop_charge_qnovo_enable(struct smb_charger *chg,
			       union power_supply_propval *val);
int smblib_set_prop_pd_current_max(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_usb_current_max(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_usb_voltage_min(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_usb_voltage_max(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_boost_current(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_typec_power_role(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_pd_active(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_pd_in_hard_reset(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_get_prop_slave_current_now(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_set_prop_ship_mode(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_charge_qnovo_enable(struct smb_charger *chg,
				const union power_supply_propval *val);
void smblib_suspend_on_debug_battery(struct smb_charger *chg);
int smblib_rerun_apsd_if_required(struct smb_charger *chg);
int smblib_get_prop_fcc_delta(struct smb_charger *chg,
			       union power_supply_propval *val);
int smblib_icl_override(struct smb_charger *chg, bool override);
int smblib_set_icl_reduction(struct smb_charger *chg, int reduction_ua);
int smblib_dp_dm(struct smb_charger *chg, int val);
int smblib_rerun_aicl(struct smb_charger *chg);

int smblib_init(struct smb_charger *chg);
int smblib_deinit(struct smb_charger *chg);
#endif /* __SMB2_CHARGER_H */
