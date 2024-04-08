#ifndef __CHG_FEATURE_H__
#define __CHG_FEATURE_H__

#define CONN_THERM_TOOHIG_70DEC 700 /* 70 Dec */
#define CONN_THERM_HYS_2DEC 20 /* 2 Dec */
#define CONN_THERM_DELAY_2S 2000 /* 2sec */
#define CONN_THERM_DELAY_5S 5000 /* 5sec */
#define CONN_THERM_DELAY_10S 10000 /* 10 sec */

struct chg_feature_info {
	struct device *dev;
	struct delayed_work	xm_prop_change_work;
	struct delayed_work	night_chargig_change_work;
	int vbus_ctrl_gpio;
	int vbus_disable;
	int update_cont;
	int night_chg_flag;
	int battery_input_suspend;
};

extern int xm_get_adapter_power_max(void);
extern int xm_get_quick_charge_type(void);
extern int xm_get_soc_decimal(void);
extern int xm_get_soc_decimal_rate(void);

#endif /* __CHG_FEATURE_H__ */

