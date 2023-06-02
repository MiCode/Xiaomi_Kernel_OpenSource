#ifndef __BATT_FEATURE_H__
#define __BATT_FEATURE_H__

#define CONN_THERM_DELAY_2S 2000 /* 2sec */

struct batt_feature_info {
	struct device *dev;

	struct delayed_work	xm_prop_change_work;

	int update_cont;
};

#endif /* __BATT_FEATURE_H__ */

