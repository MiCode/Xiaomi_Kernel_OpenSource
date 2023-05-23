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
	int fake_conn_temp;
	int connector_temp;
	int vbus_ctrl_gpio;
	int vbus_disable;
};

#endif /* __CHG_FEATURE_H__ */

