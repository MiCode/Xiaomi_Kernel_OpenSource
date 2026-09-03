#ifndef __XM_SMART_CHG_H__
#define __XM_SMART_CHG_H__

#include <linux/err.h>
#include <linux/power_supply.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/kstrtox.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/printk.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/errno.h>
#include <linux/notifier.h>
#include <linux/time64.h>
#include <linux/iio/iio.h>
#include <linux/iio/consumer.h>
#include <linux/power/charger-manager.h>
#include "xm_dfx_chg.h"

#define XM_SMART_LOG_LEVER		2

#define lc_err(fmt, ...)     \
do {                                    \
    if (XM_SMART_LOG_LEVER > 0)                  \
        printk(KERN_ERR "[SMART_CHG]" fmt, ##__VA_ARGS__);   \
}while (0)
#define lc_info(fmt, ...)    \
do {                                    \
    if (XM_SMART_LOG_LEVER > 1)          \
        printk(KERN_ERR "[SMART_CHG]" fmt, ##__VA_ARGS__);    \
}while (0)
#define lc_dbg(fmt, ...)    \
do {                                        \
    if (XM_SMART_LOG_LEVER >=2 )          \
        printk(KERN_ERR "[SMART_CHG]" fmt, ##__VA_ARGS__);    \
}while (0)

#define XM_CHARGE_WORK_MS           5000
#define MAX_THERMAL_LEVELS	        15

#define CYCLE_LOW_TEMP					150
#define CYCLE_HIGH_TEMP					450

enum smart_chg_functype{
	SMART_CHG_STATUS_FLAG = 0,
	SMART_CHG_FEATURE_MIN_NUM = 1,
	SMART_CHG_NAVIGATION = 1,
	SMART_CHG_OUTDOOR_CHARGE = 2,
	SMART_CHG_LOW_FAST = 3,
	SMART_CHG_ENDURANCE_PRO = 4,
	/* add new func here */
	SMART_CHG_FEATURE_MAX_NUM = 15,
};

struct smart_chg {
	bool en_ret;
	int active_status;
	int func_val;
};

struct charger_screen_monitor {
	struct notifier_block charger_panel_notifier;
	int screen_state;
};

enum blank_flag{
	SNORMAL = 0,
	BLACK_TO_BRIGHT = 1,
	BRIGHT = 2,
	BLACK = 3,
};

struct xm_smart_chg_info {
	struct device dev;
	const char *label;
	int fv_ageing;
	bool night_charging;
	bool night_charging_flag;
	bool endurance_changed_flag;
	bool navigation_changed_flag;
	int *pps_thermal_mitigation;
	int *pps_thermal_mitigation_fast;
	int screen_state;
	int thermal_level;
	int thermal_levels;
	struct charger_screen_monitor sm;
	struct smart_chg smart_charge[SMART_CHG_FEATURE_MAX_NUM + 1];
	int smart_chg_cmd;
	int soc;
	int raw_soc;
	struct power_supply *batt_psy;
	struct power_supply *bms_psy;
	bool smart_ctrl_en;
	bool ffc_en;
	int thermal_board_temp;
	int pd_active;
	bool low_fast_plugin_flag;
	bool pps_fast_mode;
	enum blank_flag b_flag;
	struct notifier_block charger_thermal_nb;
	/*charger_plugin_event 0:none 1:plugin 2:plugout*/
	int charger_plugin_event;
	struct notifier_block smart_chg_nb;
	struct delayed_work xm_charge_work;
	struct delayed_work xm_smart_chg_post_init_work;
	struct charger_manager *chg_info;
};

static int cycle_count_conf[] = {
	100, 300, 800, 65535
};
static int dropfv_conf[] = {
	0, 10, 20, 45
};

void set_error(struct xm_smart_chg_info *info);
void set_success(struct xm_smart_chg_info *info);
int smart_chg_is_error(struct xm_smart_chg_info *info);
void handle_smart_chg_functype(struct xm_smart_chg_info *info,
	const int func_type, const int en_ret, const int func_val);
int handle_smart_chg_functype_status(struct xm_smart_chg_info *info);
void monitor_smart_chg(struct xm_smart_chg_info *info);
void monitor_smart_batt(struct xm_smart_chg_info *info);
void monitor_cycle_count(struct xm_smart_chg_info *info);
void monitor_night_charging(struct xm_smart_chg_info *info);
void monitor_low_fast_strategy(struct xm_smart_chg_info *info);
void xm_charge_work(struct work_struct *work);
#endif
