#if !defined(__NOPMI_CHG_H__)
#define __NOPMI_CHG_H__

#include "nopmi_chg_jeita.h"
#include <linux/qti_power_supply.h>
#include "nopmi_chg_iio.h"
#include <linux/iio/consumer.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>

#define STEP_TABLE_MAX			2
#define STEP_DOWN_CURR_MA		150
#define CV_BATT_VOLT_HYSTERESIS		10

#define CC_CV_STEP_VOTER		"CC_CV_STEP_VOTER"

//longcheer nielianjie10 2022.10.13 add battery verify to limit charge current and modify battery verify logic
#define BAT_VERIFY_VOTER		"BAT_VERIFY_VOTER"  //battery verify vote to FCC
#define UNVEIRFY_BAT			2000     //FCC limit to 2A
#define VERIFY_BAT			6000	 //FCC limit to 6A

//longcheer nielianjie10 2022.12.05 Set CV according to circle count
#define CYCLE_COUNT			100	//battery cycle count

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

    struct iio_dev	*indio_dev;
	struct iio_chan_spec	*iio_chan;
	struct iio_channel	*int_iio_chans;
    struct iio_channel	**fg_ext_iio_chans;
    struct iio_channel	**cp_ext_iio_chans;
    struct iio_channel	**main_chg_ext_iio_chans;
    struct iio_channel	**cc_ext_iio_chans;
    struct iio_channel	**ds_ext_iio_chans;

	struct charger_device *master_dev;
	struct charger_device *slave_dev;
	struct charger_device *bbc_dev;
    struct class battery_class;
    struct device batt_device;

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

	//longcheer nielianjie10 2022.12.05 Set CV according to circle count
	struct step_config *select_cc_cv_step_config;
	int cycle_count;

	//2021.09.21 wsy edit reomve vote to jeita
#if 1
	struct votable		*fcc_votable;
	struct votable		*fv_votable;
	struct votable		*usb_icl_votable;
	struct votable		*chg_dis_votable;
#endif
	struct nopmi_dt_props	dt;
	struct delayed_work nopmi_chg_work;
	struct delayed_work 	cvstep_monitor_work;
	struct delayed_work	xm_prop_change_work;
	int pd_active;
	int in_verified;
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
	int update_cont;
	/*jeita config*/
	struct nopmi_chg_jeita_st jeita_ctl;

	/* thermal */
	int *thermal_mitigation;
	int thermal_levels;
	int system_temp_level;
        /*apdo*/
	int apdo_volt;
	int apdo_curr;
	NOPMI_CHARGER_IC_TYPE charge_ic_type;
};

enum nopmi_chg_iio_type {
	NOPMI_MAIN,
	NOPMI_BMS,
	NOPMI_CP_MASTER,
	NOPMI_CC,
	NOPMI_DS,
};

extern int nopmi_chg_get_iio_channel(struct nopmi_chg *chg,
			enum nopmi_chg_iio_type type, int channel, int *val);
extern int nopmi_chg_set_iio_channel(struct nopmi_chg *chg,
			enum nopmi_chg_iio_type type, int channel, int val);
extern bool is_cp_chan_valid(struct nopmi_chg *chip,
			enum cp_ext_iio_channels chan);
extern bool is_cc_chan_valid(struct nopmi_chg *chip,
			enum cc_ext_iio_channels chan);
extern bool is_ds_chan_valid(struct nopmi_chg *chip,
			enum ds_ext_iio_channels chan);
#endif
