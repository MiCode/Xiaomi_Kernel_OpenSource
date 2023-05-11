#if !defined(__NOPMI_CHG_H__)
#define __NOPMI_CHG_H__

#include "nopmi_chg_jeita.h"

#define STEP_TABLE_MAX 2
#define STEP_DOWN_CURR_MA 150
#define CV_BATT_VOLT_HYSTERESIS 10

#define CC_CV_STEP_VOTER		"CC_CV_STEP_VOTER"

struct step_config {
	int volt_lim;
	int curr_lim;
};

struct nopmi_dt_props {
	int	usb_icl_ua;
	int	chg_inhibit_thr_mv;
	bool	no_battery;
	bool	hvdcp_disable;
	bool	hvdcp_autonomous;
	bool	adc_based_aicl;
	int	sec_charger_config;
	int	auto_recharge_soc;
	int	auto_recharge_vbat_mv;
	int	wd_bark_time;
	int	wd_snarl_time_cfg;
	int	batt_profile_fcc_ua;
	int	batt_profile_fv_uv;
};


struct nopmi_chg {
	struct platform_device *pdev;
	struct device *dev;
	struct charger_device *master_dev;
	struct charger_device *slave_dev;
	struct charger_device *bbc_dev;

	struct tcpc_device *tcpc_dev;
	struct notifier_block pd_nb;
	//int typec_mode;
	enum power_supply_typec_mode typec_mode;
	int  cc_orientation;

	struct power_supply *main_psy;
	struct power_supply *master_psy;
	struct power_supply *slave_psy;
	struct power_supply *batt_psy;
	struct power_supply *usb_psy;
	struct power_supply *bms_psy;
	struct power_supply *bbc_psy;

	//2021.09.21 wsy edit reomve vote to jeita
#if 1
	struct votable		*fcc_votable;
	struct votable		*fv_votable;
	struct votable		*usb_icl_votable;
#endif
	struct nopmi_dt_props	dt;
	struct delayed_work nopmi_chg_work;
	struct delayed_work 	cvstep_monitor_work;
	int pd_active;
	int real_type;
	int pd_min_vol;
	int pd_max_vol;
	int pd_cur_max;
	int pd_usb_suspend;
	int pd_in_hard_reset;
	int usb_online;
	int batt_health;
	int input_suspend;
	int mtbf_cur;
	/*jeita config*/
	struct nopmi_chg_jeita_st jeita_ctl;

	/* thermal */
	int *thermal_mitigation;
	int thermal_levels;
	int system_temp_level;
};

#endif
