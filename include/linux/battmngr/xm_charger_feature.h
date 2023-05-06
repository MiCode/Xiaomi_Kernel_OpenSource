#ifndef __CHG_FEATURE_H__
#define __CHG_FEATURE_H__

#define CONN_THERM_TOOHIG_70DEC 700 /* 70 Dec */
#define CONN_THERM_HYS_2DEC 20 /* 2 Dec */
#define CONN_THERM_DELAY_2S 2000 /* 2sec */
#define CONN_THERM_DELAY_5S 5000 /* 5sec */
#define CONN_THERM_DELAY_10S 10000 /* 10 sec */

struct chg_feature_info {
	struct device *dev;

	struct delayed_work	typec_conn_therm_work;
	struct delayed_work	xm_prop_change_work;
	struct delayed_work	night_chargig_change_work;
	int fake_conn_temp;
	int connector_temp;
	int vbus_ctrl_gpio;
	int vbus_disable;
	int update_cont;
	int night_chg_flag;
	int battery_input_suspend;
};

enum quick_charge_type {
	QUICK_CHARGE_NORMAL = 0,
	QUICK_CHARGE_FAST,
	QUICK_CHARGE_FLASH,
	QUICK_CHARGE_TURBE,
	QUICK_CHARGE_SUPER,
	QUICK_CHARGE_MAX,
};

struct quick_charge {
	int adap_type;
	enum quick_charge_type adap_cap;
};

enum apdo_max_power {
	APDO_MAX_30W = 30, // J2 G7A F4 and some old projects use 30W pps charger
	APDO_MAX_33W = 33,   // most 33W project use 33w pps charger
	APDO_MAX_40W = 40,   // only F1X use 40w pps charger
	APDO_MAX_50W = 50,   // only j1(cmi project) use 50W pps(device support maxium 50w)
	APDO_MAX_55W =55, // K2 K9B use 55w pps charger
	APDO_MAX_65W = 65, //we have 65w pps which for j1(cmi), and also used for 120w(67w works in 65w)
	APDO_MAX_67W = 67, //most useage now for dual charge pumps projects such as L3 L1 L1A L18
	APDO_MAX_100W = 100, // Zimi car quick charger have 100w pps
	APDO_MAX_120W = 120, // L2 L10 and K8 L11 use 120W
	APDO_MAX_INVALID = 67,
};
extern int xm_get_adapter_power_max(void);
extern int xm_get_quick_charge_type(void);
extern int xm_get_soc_decimal(void);
extern int xm_get_soc_decimal_rate(void);

#endif /* __CHG_FEATURE_H__ */

