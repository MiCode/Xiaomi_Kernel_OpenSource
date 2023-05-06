
#ifndef __XM_CHARGER_CORE_H
#define __XM_CHARGER_CORE_H

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/regulator/driver.h>

#include <linux/battmngr/xm_battmngr_iio.h>
#include <linux/battmngr/battmngr_voter.h>
#include <linux/battmngr/battmngr_notifier.h>
#include <linux/battmngr/step-chg-jeita.h>
#include <linux/battmngr/xm_charger_feature.h>

static int chg_log_level = 2;
#define charger_err(fmt, ...)							\
do {										\
	if (chg_log_level >= 0)							\
		printk(KERN_ERR "[xm_charger_core] " fmt, ##__VA_ARGS__);	\
} while (0)

#define charger_info(fmt, ...)							\
do {										\
	if (chg_log_level >= 1)							\
		printk(KERN_ERR "[xm_charger_core] " fmt, ##__VA_ARGS__);	\
} while (0)

#define charger_dbg(fmt, ...)							\
do {										\
	if (chg_log_level >= 2)							\
		printk(KERN_ERR "[xm_charger_core] " fmt, ##__VA_ARGS__);	\
} while (0)

#define FFC_MODE_VOTER		"FFC_MODE_VOTER"
#define THERMAL_DAEMON_VOTER		"THERMAL_DAEMON_VOTER"
#define USER_VOTER		"USER_VOTER"
#define CHG_INIT_VOTER		"CHG_INIT_VOTER"
#define CHG_AWAKE_VOTER	"CHG_AWAKE_VOTER"
#define SMART_BATTERY_FV		"SMART_BATTERY_FV"

#define MAX_TEMP_LEVEL		16

#define POWER_SUPPLY_TYPE_USB_HVDCP		QTI_POWER_SUPPLY_TYPE_USB_HVDCP
#define POWER_SUPPLY_TYPE_USB_HVDCP_3		QTI_POWER_SUPPLY_TYPE_USB_HVDCP_3
#define POWER_SUPPLY_TYPE_USB_HVDCP_3P5		QTI_POWER_SUPPLY_TYPE_USB_HVDCP_3P5
#define POWER_SUPPLY_TYPE_USB_FLOAT		QTI_POWER_SUPPLY_TYPE_USB_FLOAT
static struct power_supply_desc usb_psy_desc;

#define LIGHTING_ICON_CHANGE		50
#define BATT_COOL_THRESHOLD		150
#define BATT_NORMAL_H_THRESHOLD		350
#define BATT_WARM_THRESHOLD		480
#define BATT_HYS_THRESHOLD		20
#define SOC_HIGH_THRESHOLD		95
#define BATT_WARM_VBAT_THRESHOLD	4100000

#define MAIN_MIN_FCC			(100 * 1000)

struct charger_dt_props {
	int	chg_voltage_max;
	int	batt_current_max;
	int	chg_design_voltage_max;
	int	input_batt_current_max;
	int step_chg_enable;
	int sw_jeita_enable;
	int ffc_ieoc_l;
	int ffc_ieoc_h;
	int non_ffc_ieoc;
	int non_ffc_cv;
	int non_ffc_cc;


	int *thermal_mitigation;
	int *thermal_mitigation_dcp;
	int *thermal_mitigation_qc2;
	int *thermal_mitigation_icl;
	int *thermal_mitigation_pd;
	int *thermal_mitigation_pd_cp;

};

struct xm_charger {
	struct device *dev;
	struct power_supply *usb_psy;
	struct power_supply *batt_psy;
	struct charger_dt_props	dt;
	struct step_chg_info *step_chg;
	struct chg_feature_info *chg_feature;
	struct regulator *dpdm_reg;
	struct mutex dpdm_lock;

	int thermal_levels;
	int system_temp_level;
	int bc12_type;
	int bc12_active;
	int pd_active;
	int pd_verified;
	int dpdm_enabled;
	int real_type;
	int input_suspend;
	int smartBatVal;
	int otg_enable;
	int smart_batt;

	struct votable *fcc_votable;
	struct votable *fv_votable;
	struct votable *usb_icl_votable;
	struct votable *awake_votable;
	struct votable *input_suspend_votable;
	struct votable *smart_batt_votable;
};

extern struct xm_charger *g_xm_charger;

extern int xm_charger_init(struct xm_charger *charger);
extern void xm_charger_deinit(void);
int xm_charger_parse_dt_therm(struct xm_charger *charger);
extern int xm_charger_thermal(struct xm_charger *charger);
int xm_charger_get_fastcharge_mode(struct xm_charger *charger, int *mode);
int xm_charger_set_fastcharge_mode(struct xm_charger *charger, int mode);
int xm_charger_create_votable(struct xm_charger *charger);
int stepchg_jeita_start_stop(struct xm_charger *charger, struct step_chg_info *chip);
int night_charging_start_stop(struct xm_charger *charger, struct chg_feature_info *chip);
int xm_stepchg_jeita_init(struct xm_charger *charger, bool step_chg_enable, bool sw_jeita_enable);
void xm_step_chg_deinit(void);
int typec_conn_therm_start_stop(struct xm_charger *charger, struct chg_feature_info *info);
int xm_chg_feature_init(struct xm_charger *charger);
void xm_chg_feature_deinit(void);
extern int charger_process_event_cp(struct battmngr_notify *noti_data);
extern int charger_process_event_mainchg(struct battmngr_notify *noti_data);
extern int charger_process_event_pd(struct battmngr_notify *noti_data);

#endif /* __XM_CHARGER_CORE_H */

