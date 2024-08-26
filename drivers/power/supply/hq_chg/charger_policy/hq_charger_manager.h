#ifndef __HQ_CHARGER_MANAGER_H__
#define __HQ_CHARGER_MANAGER_H__

#include <linux/module.h>
#include <linux/types.h>
#include <linux/power_supply.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/usb/phy.h>
#include <linux/acpi.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <generated/autoconf.h>
#include <linux/device/class.h>
#include <linux/reboot.h>

#include "../charger_class/hq_charger_class.h"
#include "../charger_class/hq_cp_class.h"
#include "../charger_class/hq_fg_class.h"
#include "../charger_class/hq_batt_class.h"
#include "../common/hq_voter.h"
#include "hq_jeita.h"
#include "hq_cp_policy.h"

#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
#include "../charger_class/xm_adapter_class.h"
#endif

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
#include <linux/notifier.h>
#include "../../../../gpu/drm/mediatek/mediatek_v2/mtk_disp_notify.h"
#endif

#define CHARGER_MANAGER_VERSION            "1.1.1"

#if IS_ENABLED(CONFIG_TCPC_CLASS)
#include "../../../misc/mediatek/typec/tcpc/inc/tcpm.h"
#include "../../../misc/mediatek/typec/tcpc/inc/tcpci_core.h"
#include "../../../misc/mediatek/typec/tcpc/inc/tcpci_typec.h"
#endif

#define CHARGER_VINDPM_USE_DYNAMIC         1
#define CHARGER_VINDPM_DYNAMIC_BY_VBAT1    3800
#define CHARGER_VINDPM_DYNAMIC_VALUE1      4000
#define CHARGER_VINDPM_DYNAMIC_BY_VBAT2    4200
#define CHARGER_VINDPM_DYNAMIC_VALUE2      4400
#define CHARGER_VINDPM_DYNAMIC_BY_VBAT3    4300
#define CHARGER_VINDPM_DYNAMIC_VALUE3      4500
#define CHARGER_VINDPM_DYNAMIC_BY_VBAT4    4400
#define CHARGER_VINDPM_DYNAMIC_VALUE4      4600
#define CHARGER_VINDPM_DYNAMIC_VALUE5      4700
#define FLOAT_DELAY_TIME                   5000
#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
#define WAIT_USB_RDY_TIME               100
#define WAIT_USB_RDY_MAX_CNT            300
#endif

#define NORMAL_CHG_VOLTAGE_MAX             4480000
#define FAST_CHG_VOLTAGE_MAX               4530000
#define VOLTAGE_MAX                        11000000
#define CURRENT_MAX                        12400000
#define INPUT_CURRENT_LIMIT                6100000
#define CP_EN_MAIN_CHG_CURR                100
#define TYPICAL_CAPACITY                   5030000

#define _TO_STR(x) #x
#define TO_STR(x) _TO_STR(x)
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
#define PROJECT_NAME                        n19a
#define TYPICAL_CAPACITY_MAH                5000
#define INPUT_POWER_LIMIT                   33
#define INPUT_POWER_OVER_33W                33
#define _MODEL_NAME(name, capacity, adpo_max) \
	name##_##capacity##mah_##adpo_max##w
#define MODEL_NAME(name, capacity, adpo_max) \
	_MODEL_NAME(name, capacity, adpo_max)
#endif

#define POWER_SUPPLY_MANUFACTURER          "HUAQIN"
#define POWER_SUPPLY_MODEL_NAME            "Main chg Driver"

#define FASTCHARGE_MIN_CURR                1800
#define CHARGER_MANAGER_LOOP_TIME          5000    // 5s
#define CHARGER_MANAGER_LOOP_TIME_OUT      20000   // 20s
#define MAX_UEVENT_LENGTH                  50
#define SHUTDOWN_DELAY_VOL_LOW             3300
#define SHUTDOWN_DELAY_VOL_HIGH            3400

#define BATTERY_WARM_TEMP                  480
#define BATTERY_HOT_TEMP                   580
#define BATTERY_COLD_TEMP                  -120

#define SUPER_CHARGE_POWER                 50

#define MIAN_CHG_ADC_LENGTH                180
#define PD20_ICHG_MULTIPLE                 1800
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
#define FG_I2C_ERR_SOC                     15
#define FG_I2C_ERR_VBUS                    6500
#endif

static bool is_mtbf_mode;
static int float_count = 0;
#if IS_ENABLED(CONFIG_BC12_RETRY_FOR_MI_PD)
static bool bc12_retry_flag = 1;
#endif

static const char *bc12_result[] = {
	"None",
	"SDP",
	"CDP",
	"DCP",
	"QC",
	"FLOAT",
	"Non-Stand",
	"QC3",
	"QC3+",
	"PD",
};

static const char *real_type_txt[] = {
	"None",
	"USB",
	"USB_CDP",
	"DCP",
	"USB_HVDCP",
	"USB_FLOAT",
	"Non-Stand",
	"USB_HVDCP_3",
	"USB_HVDCP_3P5",
	"USB_PD",
};

enum charger_vbus_type {
	CHARGER_VBUS_NONE,
	CHARGER_VBUS_USB_SDP,
	CHARGER_VBUS_USB_CDP,
	CHARGER_VBUS_USB_DCP,
	CHARGER_VBUS_HVDCP,
	CHARGER_VBUS_UNKNOWN,
	CHARGER_VBUS_NONSTAND,
	CHARGER_VBUS_OTG,
	CHARGER_VBUS_TYPE_NUM,
};

#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
enum quick_charge_type {
	QUICK_CHARGE_NORMAL = 0,    /* USB、DCP、CDP、Float */
	QUICK_CHARGE_FAST,          /* PD、QC2、QC3 */
	QUICK_CHARGE_FLASH,
	QUICK_CHARGE_TURBE,         /* verified PD(apdo_max < 50W)、QC3.5、QC3-27W */
	QUICK_CHARGE_SUPER,         /* verified PD(apdo_max >= 50W) */
};

struct quick_charge {
	enum vbus_type adap_type;
	enum quick_charge_type adap_cap;
};

static struct quick_charge quick_charge_map[10] = {
	{ VBUS_TYPE_SDP, QUICK_CHARGE_NORMAL },
	{ VBUS_TYPE_DCP, QUICK_CHARGE_NORMAL },
	{ VBUS_TYPE_CDP, QUICK_CHARGE_NORMAL },
	{ VBUS_TYPE_NON_STAND, QUICK_CHARGE_NORMAL },
	{ VBUS_TYPE_PD, QUICK_CHARGE_FAST },
	{ VBUS_TYPE_HVDCP, QUICK_CHARGE_FAST },
	{ VBUS_TYPE_HVDCP_3, QUICK_CHARGE_TURBE },
	{ VBUS_TYPE_HVDCP_3P5, QUICK_CHARGE_TURBE },
	{ 0, 0 },
};
#endif

enum battery_temp_level {
	TEMP_LEVEL_COLD,
	TEMP_LEVEL_COOL,
	TEMP_LEVEL_GOOD,
	TEMP_LEVEL_WARM,
	TEMP_LEVEL_HOT,
	TEMP_LEVEL_MAX,
};

enum {
	PD_THERM_PARSE_ERROR = 1,
	QC2_THERM_PARSE_ERROR,
};

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
enum smart_chg_functype{
	SMART_CHG_STATUS_FLAG = 0,
	SMART_CHG_FEATURE_MIN_NUM = 1,
	SMART_CHG_NAVIGATION = 1,
	SMART_CHG_OUTDOOR_CHARGE,
        SMART_CHG_LOW_FAST = 3,
        SMART_CHG_ENDURANCE_PRO = 4,
	/* add new func here */
	SMART_CHG_FEATURE_MAX_NUM = 16,
};

struct smart_chg {
	bool en_ret;
	int active_status;
	int func_val;
};

/*thermal board temp*/
enum charger_thermal_notifier_events {
	THERMAL_BOARD_TEMP = 0,
};

struct charger_screen_monitor {
       struct notifier_block charger_panel_notifier;
       int screen_state;
};

enum blank_flag{
	NORMAL = 0,
	BLACK_TO_BRIGHT = 1,
	BRIGHT = 2,
	BLACK = 3,
};

#define CYCLE_COUNT_MAX 4
#endif

struct charger_manager {
	struct device *dev;
	wait_queue_head_t wait_queue;
	struct task_struct *thread;
	bool run_thread;

	struct timer_list charger_timer;
	struct delayed_work second_detect_work;
	#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
	struct delayed_work wait_usb_ready_work;
	int get_usb_rdy_cnt;
	struct device_node *usb_node;
	#endif
	/* notifier add here */
	struct notifier_block charger_nb;
#if IS_ENABLED(CONFIG_TCPC_CLASS)
	struct tcpc_device *tcpc;
	struct notifier_block pd_nb;
#endif

	/* ext_dev add here */
	struct charger_dev *charger;
	struct chargerpump_dev *master_cp_chg;
	struct chargerpump_dev *slave_cp_chg;
#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
	struct adapter_device *pd_adapter;
#endif
	struct fuel_gauge_dev *fuel_gauge;
	struct batt_info_dev *batt_info;

	/* flag add here */
	int pd_active;
	bool is_pr_swap;
	bool pd_contract_update;
	int qc3_mode;
	int input_suspend;
	bool qc_detected;
	bool adapter_plug_in;
	bool usb_online;
	bool shutdown_delay;
	bool last_shutdown_delay;
	int soc;
	int ibat;
	int vbat;
	int tbat;
	int chg_status;
	enum vbus_type vbus_type;
	int32_t chg_adc[ADC_GET_MAX];
	int typec_mode;
	int batt_cycle;

	/* psy add here */
	struct power_supply *usb_psy;
	struct power_supply *batt_psy;
	struct power_supply *fg_psy;
	struct power_supply *cp_master_psy;
	struct power_supply *cp_slave_psy;
	struct power_supply_desc usb_psy_desc;

	/* voter add here */
	struct votable *main_fcc_votable;
	struct votable *fv_votable;
	struct votable *main_icl_votable;
	struct votable *iterm_votable;
	struct votable *main_chg_disable_votable;
	struct votable *cp_disable_votable;
	struct votable *total_fcc_votable;

	/* charge current add here*/
	int pd_curr_max;
	int pd_volt_max;
	int usb_current;
	int float_current;
	int cdp_current;
	int dcp_current;
	int hvdcp_charge_current;
	int hvdcp_input_current;
	int hvdcp3_charge_current;
	int hvdcp3_input_current;
	int pd2_charge_current;
	int pd2_input_current;
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
	int xm_outdoor_current;
	int apdo_max;
#endif

	/*thermal add here*/
	bool thermal_enable;
	int thermal_parse_flags;
	int system_temp_level;
	int *pd_thermal_mitigation;
	int *qc2_thermal_mitigation;
	int pd_thermal_levels;
	int qc2_thermal_levels;

	/********dts setting********/
	bool cp_master_use;
	bool cp_slave_use;
	int *battery_temp;
	bool shippingmode;
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	struct smart_chg smart_charge[SMART_CHG_FEATURE_MAX_NUM + 1];
	int smart_chg_cmd;
	struct delayed_work xm_charge_work;
	bool smart_ctrl_en;
        bool endurance_ctrl_en;

	int smart_batt;
	bool night_charging;
	bool night_charging_flag;
	int cyclecount[CYCLE_COUNT_MAX];
	int dropfv[CYCLE_COUNT_MAX];
	int dropfv_normal[CYCLE_COUNT_MAX];
	int fv_againg;
	struct charger_screen_monitor sm;
	int thermal_board_temp;
	struct notifier_block charger_thermal_nb;
	int *pd_thermal_mitigation_fast;
	bool low_fast_plugin_flag;
	bool pps_fast_mode;
        int low_fast_ffc;
	enum blank_flag b_flag;
	int  is_full_flag;
	struct votable	*is_full_votable;
#endif
};

const static char * adc_name[] = {
	"VBUS","VSYS","VBAT","VAC","IBUS","IBAT","TSBUS","TSBAT","TDIE",
};

/*********extern func/struct/int start***********/
int charger_manager_usb_psy_register(struct charger_manager *manager);
int charger_manager_batt_psy_register(struct charger_manager *manager);
int hq_usb_sysfs_create_group(struct charger_manager *manager);
int hq_batt_sysfs_create_group(struct charger_manager *manager);
void hq_set_prop_system_temp_level(struct charger_manager *manager, char *voter_name);
int charger_manager_get_current(struct charger_manager *manager, int *curr);
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
void xm_uevent_report(struct charger_manager *manager);
#endif
// #if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
// extern struct adapter_device *get_adapter_by_name(const char *name);
// extern int adapter_get_usbpd_verifed(struct adapter_device *adapter_dev, bool *verifed);
// #endif
extern struct chargerpump_policy *g_policy;
extern bool is_mtbf_mode_func(void);

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
/*thermal board temp*/
extern struct srcu_notifier_head charger_thermal_notifier;
extern int charger_thermal_reg_notifier(struct notifier_block *nb);
extern int charger_thermal_unreg_notifier(struct notifier_block *nb);
extern int charger_thermal_notifier_call_chain(unsigned long event,int val);
#endif
/*********extern func/struct/int end***********/
#endif
