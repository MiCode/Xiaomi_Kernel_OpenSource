/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 */

#ifndef __SMBLITE_LIB_H
#define __SMBLITE_LIB_H
#include <linux/alarmtimer.h>
#include <linux/ktime.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/consumer.h>
#include <linux/extcon-provider.h>
#include <linux/usb/typec.h>
#include "storm-watch.h"
#include "battery.h"

enum print_reason {
	PR_INTERRUPT	= BIT(0),
	PR_REGISTER	= BIT(1),
	PR_MISC		= BIT(2),
	PR_PARALLEL	= BIT(3),
	PR_OTG		= BIT(4),
};

#define DEFAULT_VOTER			"DEFAULT_VOTER"
#define USER_VOTER			"USER_VOTER"
#define USB_PSY_VOTER			"USB_PSY_VOTER"
#define USBIN_V_VOTER			"USBIN_V_VOTER"
#define THERMAL_DAEMON_VOTER		"THERMAL_DAEMON_VOTER"
#define BOOST_BACK_VOTER		"BOOST_BACK_VOTER"
#define DEBUG_BOARD_VOTER		"DEBUG_BOARD_VOTER"
#define PL_DELAY_VOTER			"PL_DELAY_VOTER"
#define SW_ICL_MAX_VOTER		"SW_ICL_MAX_VOTER"
#define BATT_PROFILE_VOTER		"BATT_PROFILE_VOTER"
#define USBIN_I_VOTER			"USBIN_I_VOTER"
#define WEAK_CHARGER_VOTER		"WEAK_CHARGER_VOTER"
#define HW_LIMIT_VOTER			"HW_LIMIT_VOTER"
#define FORCE_RECHARGE_VOTER		"FORCE_RECHARGE_VOTER"
#define FCC_STEPPER_VOTER		"FCC_STEPPER_VOTER"
#define SW_THERM_REGULATION_VOTER	"SW_THERM_REGULATION_VOTER"
#define JEITA_ARB_VOTER			"JEITA_ARB_VOTER"
#define AICL_THRESHOLD_VOTER		"AICL_THRESHOLD_VOTER"
#define USB_SUSPEND_VOTER		"USB_SUSPEND_VOTER"
#define DETACH_DETECT_VOTER		"DETACH_DETECT_VOTER"
#define ICL_CHANGE_VOTER		"ICL_CHANGE_VOTER"
#define TYPEC_SWAP_VOTER		"TYPEC_SWAP_VOTER"
#define FLASH_ACTIVE_VOTER		"FLASH_ACTIVE_VOTER"

#define BOOST_BACK_STORM_COUNT	3
#define WEAK_CHG_STORM_COUNT	8

#define VBAT_TO_VRAW_ADC(v)		div_u64((u64)v * 1000000UL, 194637UL)

#define ITERM_LIMITS_MA			10000
#define ADC_CHG_ITERM_MASK		32767

#define USBIN_25UA	25000
#define USBIN_100UA     100000
#define USBIN_150UA     150000
#define USBIN_300UA     300000
#define USBIN_400UA     400000
#define USBIN_500UA     500000
#define USBIN_900UA     900000
#define CDP_CURRENT_UA			1500000
#define DCP_CURRENT_UA			1500000
#define TYPEC_DEFAULT_CURRENT_UA	900000
#define TYPEC_MEDIUM_CURRENT_UA		1500000
#define TYPEC_HIGH_CURRENT_UA		3000000
#define ROLE_REVERSAL_DELAY_MS		500

enum smb_mode {
	PARALLEL_MASTER = 0,
	PARALLEL_SLAVE,
	NUM_MODES,
};

enum sink_src_mode {
	SINK_MODE,
	SRC_MODE,
	AUDIO_ACCESS_MODE,
	UNATTACHED_MODE,
};

enum {
	BOOST_BACK_WA			= BIT(0),
	WEAK_ADAPTER_WA			= BIT(1),
	FLASH_DIE_TEMP_DERATE_WA	= BIT(2),
};

enum jeita_cfg_stat {
	JEITA_CFG_NONE = 0,
	JEITA_CFG_FAILURE,
	JEITA_CFG_COMPLETE,
};

enum {
	RERUN_AICL = 0,
	RESTART_AICL,
};

enum smb_irq_index {
	/* CHGR */
	CHG_STATE_CHANGE_IRQ = 0,
	CHGR_ERROR_IRQ,
	BUCK_OC_IRQ,
	VPH_OV_IRQ,
	/* DCDC */
	OTG_FAIL_IRQ,
	OTG_FAULT_IRQ,
	SKIP_MODE_IRQ,
	INPUT_CURRENT_LIMITING_IRQ,
	SWITCHER_POWER_OK_IRQ,
	/* BATIF */
	BAT_TEMP_IRQ,
	BAT_THERM_OR_ID_MISSING_IRQ,
	BAT_LOW_IRQ,
	BAT_OV_IRQ,
	BSM_ACTIVE_IRQ,
	/* USB */
	USBIN_PLUGIN_IRQ,
	USBIN_COLLAPSE_IRQ,
	USBIN_UV_IRQ,
	USBIN_OV_IRQ,
	USBIN_GT_VT_IRQ,
	USBIN_ICL_CHANGE_IRQ,
	/* TYPEC */
	TYPEC_OR_RID_DETECTION_CHANGE_IRQ,
	TYPEC_VPD_DETECT_IRQ,
	TYPEC_CC_STATE_CHANGE_IRQ,
	TYPEC_VBUS_CHANGE_IRQ,
	TYPEC_ATTACH_DETACH_IRQ,
	TYPEC_LEGACY_CABLE_DETECT_IRQ,
	TYPEC_TRY_SNK_SRC_DETECT_IRQ,
	/* MISC */
	WDOG_SNARL_IRQ,
	WDOG_BARK_IRQ,
	AICL_FAIL_IRQ,
	AICL_DONE_IRQ,
	IMP_TRIGGER_IRQ,
	ALL_CHNL_CONV_DONE_IRQ,
	TEMP_CHANGE_IRQ,
	/* FLASH */
	VREG_OK_IRQ,
	ILIM_S2_IRQ,
	ILIM_S1_IRQ,
	FLASH_STATE_CHANGE_IRQ,
	TORCH_REQ_IRQ,
	FLASH_EN_IRQ,
	/* END */
	SMB_IRQ_MAX,
};

enum chg_term_config_src {
	ITERM_SRC_UNSPECIFIED,
	ITERM_SRC_ADC,
	ITERM_SRC_ANALOG
};

struct smb_irq_info {
	const char			*name;
	const irq_handler_t		handler;
	const bool			wake;
	const struct storm_watch	storm_data;
	struct smb_irq_data		*irq_data;
	int				irq;
	bool				enabled;
};

static const unsigned int smblite_lib_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

enum icl_override_mode {
	/* APSD/Type-C/QC auto */
	HW_AUTO_MODE,
	/* 100/150/500/900mA */
	SW_OVERRIDE_USB51_MODE,
	/* ICL other than USB51 */
	SW_OVERRIDE_HC_MODE,
};

/* EXTCON_USB and EXTCON_USB_HOST are mutually exclusive */
static const u32 smblite_lib_extcon_exclusive[] = {0x3, 0};

struct smb_irq_data {
	void                    *parent_data;
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

struct smb_params {
	struct smb_chg_param	fcc;
	struct smb_chg_param	fv;
	struct smb_chg_param	usb_icl;
	struct smb_chg_param	icl_max_stat;
	struct smb_chg_param	icl_stat;
	struct smb_chg_param	aicl_5v_threshold;
};

struct parallel_params {
	struct power_supply	*psy;
};

struct smb_iio {
	struct iio_channel	*temp_chan;
	struct iio_channel	*usbin_v_chan;
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
	int			weak_chg_icl_ua;

	/* locks */
	struct mutex		typec_lock;

	/* power supplies */
	struct power_supply		*batt_psy;
	struct power_supply		*usb_psy;
	struct power_supply		*bms_psy;
	struct power_supply		*usb_main_psy;
	enum power_supply_type		real_charger_type;

	/* notifiers */
	struct notifier_block	nb;

	/* parallel charging */
	struct parallel_params	pl;

	/* typec */
	struct typec_port	*typec_port;
	struct typec_capability	typec_caps;
	struct typec_partner	*typec_partner;
	struct typec_partner_desc typec_partner_desc;

	/* votables */
	struct votable		*fcc_votable;
	struct votable		*fcc_main_votable;
	struct votable		*fv_votable;
	struct votable		*usb_icl_votable;
	struct votable		*awake_votable;
	struct votable		*pl_disable_votable;
	struct votable		*chg_disable_votable;
	struct votable		*pl_enable_votable_indirect;
	struct votable		*icl_irq_disable_votable;
	struct votable		*temp_change_irq_disable_votable;

	/* work */
	struct work_struct	bms_update_work;
	struct work_struct	pl_update_work;
	struct work_struct	jeita_update_work;
	struct delayed_work	icl_change_work;
	struct delayed_work	pl_enable_work;
	struct delayed_work	bb_removal_work;
	struct delayed_work	thermal_regulation_work;
	struct delayed_work	role_reversal_check;
	struct delayed_work	pr_swap_detach_work;

	struct charger_param	chg_param;

	/* cached status */
	int			system_temp_level;
	int			thermal_levels;
	int			*thermal_mitigation;
	int			fake_capacity;
	int			fake_batt_status;
	bool			step_chg_enabled;
	bool			typec_legacy_use_rp_icl;
	int			connector_type;
	bool			suspend_input_on_debug_batt;
	bool			fake_chg_status_on_debug_batt;
	int			typec_mode;
	int			dr_mode;
	int			term_vbat_uv;
	u32			jeita_status;
	bool			jeita_arb_flag;
	bool			typec_legacy;
	bool			otg_present;
	int			auto_recharge_soc;
	enum sink_src_mode	sink_src_mode;
	enum power_supply_typec_power_role power_role;
	enum jeita_cfg_stat	jeita_configured;
	bool			fcc_stepper_enable;
	u32			jeita_soft_thlds[2];
	u32			jeita_soft_hys_thlds[2];
	int			jeita_soft_fcc[2];
	int			jeita_soft_fv[2];
	int			aicl_5v_threshold_mv;
	int			default_aicl_5v_threshold_mv;
	int			cutoff_count;
	bool			aicl_max_reached;
	bool			pr_swap_in_progress;
	bool			ldo_mode;
	int			usb_id_gpio;
	int			usb_id_irq;
	bool			typec_role_swap_failed;

	/* workaround flag */
	u32			wa_flags;
	int			boost_current_ua;

	/* extcon for VBUS / ID notification to USB for uUSB */
	struct extcon_dev	*extcon;

	/* battery profile */
	int			batt_profile_fcc_ua;
	int			batt_profile_fv_uv;

	/* flash */
	u32			flash_derating_soc;
	u32			flash_disable_soc;
	u32			headroom_mode;
	bool			flash_init_done;
	bool			flash_active;
	u32			irq_status;
};

int smblite_lib_read(struct smb_charger *chg, u16 addr, u8 *val);
int smblite_lib_masked_write(struct smb_charger *chg, u16 addr, u8 mask,
				u8 val);
int smblite_lib_write(struct smb_charger *chg, u16 addr, u8 val);
int smblite_lib_batch_write(struct smb_charger *chg, u16 addr, u8 *val,
				int count);
int smblite_lib_batch_read(struct smb_charger *chg, u16 addr, u8 *val,
				int count);
int smblite_lib_get_charge_param(struct smb_charger *chg,
				struct smb_chg_param *param, int *val_u);
int smblite_lib_get_usb_suspend(struct smb_charger *chg, int *suspend);
int smblite_lib_enable_charging(struct smb_charger *chg, bool enable);
int smblite_lib_set_charge_param(struct smb_charger *chg,
				struct smb_chg_param *param, int val_u);
int smblite_lib_set_usb_suspend(struct smb_charger *chg, bool suspend);

irqreturn_t smblite_default_irq_handler(int irq, void *data);
irqreturn_t smblite_chg_state_change_irq_handler(int irq, void *data);
irqreturn_t smblite_batt_temp_changed_irq_handler(int irq, void *data);
irqreturn_t smblite_batt_psy_changed_irq_handler(int irq, void *data);
irqreturn_t smblite_usbin_uv_irq_handler(int irq, void *data);
irqreturn_t smblite_usb_plugin_irq_handler(int irq, void *data);
irqreturn_t smblite_icl_change_irq_handler(int irq, void *data);
irqreturn_t smblite_typec_state_change_irq_handler(int irq, void *data);
irqreturn_t smblite_typec_attach_detach_irq_handler(int irq, void *data);
irqreturn_t smblite_switcher_power_ok_irq_handler(int irq, void *data);
irqreturn_t smblite_wdog_bark_irq_handler(int irq, void *data);
irqreturn_t smblite_typec_or_rid_detection_change_irq_handler(int irq,
				void *data);
irqreturn_t smblite_temp_change_irq_handler(int irq, void *data);
irqreturn_t smblite_usbin_ov_irq_handler(int irq, void *data);
irqreturn_t smblite_usb_id_irq_handler(int irq, void *data);

int smblite_lib_get_prop_input_suspend(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_get_prop_batt_present(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_get_prop_batt_capacity(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_get_prop_batt_status(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_get_prop_batt_charge_type(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_get_prop_batt_charge_done(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_get_batt_current_now(struct smb_charger *chg,
					union power_supply_propval *val);
int smblite_lib_get_prop_batt_health(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_get_prop_system_temp_level(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_get_prop_system_temp_level_max(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_get_prop_input_current_limited(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_get_prop_batt_iterm(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_set_prop_input_suspend(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblite_lib_set_prop_batt_capacity(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblite_lib_set_prop_batt_status(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblite_lib_set_prop_system_temp_level(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblite_lib_get_prop_usb_present(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_get_prop_usb_online(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_get_usb_online(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_get_prop_usb_suspend(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_get_prop_usb_voltage_now(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_get_usb_prop_typec_mode(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_get_prop_typec_cc_orientation(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_get_prop_scope(struct smb_charger *chg,
			union power_supply_propval *val);
int smblite_lib_get_prop_typec_power_role(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_get_prop_input_current_settled(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_get_prop_input_voltage_settled(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_get_prop_charger_temp(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_get_prop_die_health(struct smb_charger *chg);
int smblite_lib_get_die_health(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_set_prop_current_max(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblite_lib_set_prop_typec_power_role(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblite_lib_set_prop_ship_mode(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblite_lib_set_prop_rechg_soc_thresh(struct smb_charger *chg,
				const union power_supply_propval *val);
void smblite_lib_suspend_on_debug_battery(struct smb_charger *chg);
int smblite_lib_get_prop_fcc_delta(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_get_thermal_threshold(struct smb_charger *chg, u16 addr,
				int *val);
int smblite_lib_run_aicl(struct smb_charger *chg, int type);
int smblite_lib_set_icl_current(struct smb_charger *chg, int icl_ua);
int smblite_lib_get_icl_current(struct smb_charger *chg, int *icl_ua);
int smblite_lib_get_charge_current(struct smb_charger *chg,
				int *total_current_ua);
int smblite_lib_get_hw_current_max(struct smb_charger *chg,
				int *total_current_ua);
int smblite_lib_get_prop_pr_swap_in_progress(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_set_prop_pr_swap_in_progress(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblite_lib_typec_port_type_set(const struct typec_capability *cap,
				enum typec_port_type type);
int smblite_lib_get_prop_from_bms(struct smb_charger *chg,
				enum power_supply_property psp,
				union power_supply_propval *val);
int smblite_lib_get_iio_channel(struct smb_charger *chg, const char *propname,
					struct iio_channel **chan);
int smblite_lib_read_iio_channel(struct smb_charger *chg,
				struct iio_channel *chan, int div, int *data);
int smblite_lib_icl_override(struct smb_charger *chg,
				enum icl_override_mode mode);
int smblite_lib_get_irq_status(struct smb_charger *chg,
				union power_supply_propval *val);
int smblite_lib_set_prop_usb_type(struct smb_charger *chg,
				const union power_supply_propval *val);
void smblite_update_usb_desc(struct smb_charger *chg);
int smblite_lib_init(struct smb_charger *chg);
int smblite_lib_deinit(struct smb_charger *chg);
#endif /* __SMBLITE_LIB_H */
