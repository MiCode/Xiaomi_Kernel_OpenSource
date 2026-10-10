#ifndef __LX_CHARGER_MANAGER_H__
#define __LX_CHARGER_MANAGER_H__

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
#include <linux/soc/qcom/panel_event_notifier.h>
#include <drm/drm_panel.h>
#include <linux/pm_wakeup.h>
#include <linux/hrtimer.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/time64.h>
#include <linux/timekeeping.h>
#include <linux/pm_qos.h>
#include <linux/cpufreq.h>

#include "../charger_class/lx_charger_class.h"
#include "../charger_class/lx_cp_class.h"
#include "../charger_class/lx_fg_class.h"
#include "../common/lx_voter.h"
#include "lx_jeita.h"
#include "lx_cp_policy.h"

#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
#include "../charger_class/xm_adapter_class.h"
#endif

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
#include <linux/notifier.h>
#if IS_ENABLED(CONFIG_DISP_MTK)
#include "../../../../gpu/drm/mediatek/mediatek_v2/mtk_disp_notify.h"
#else
/* to do add */
#endif
#endif

#define CHARGER_MANAGER_VERSION            "1.1.1"

#if IS_ENABLED(CONFIG_TCPC_CLASS)
#if IS_ENABLED(CONFIG_CONFIG_TCPC_CLASS_MTK)
#include "../../../misc/mediatek/typec/tcpc/inc/tcpm.h"
#include "../../../misc/mediatek/typec/tcpc/inc/tcpci_core.h"
#include "../../../misc/mediatek/typec/tcpc/inc/tcpci_typec.h"
#else
#include "../../../../usb/typec/tcpc/inc/tcpm.h"
#include "../../../../usb/typec/tcpc/inc/tcpci_core.h"
#include "../../../../usb/typec/tcpc/inc/tcpci_typec.h"
#endif
#endif

#define CHARGER_VINDPM_USE_DYNAMIC         1
#define CHARGER_VINDPM_DYNAMIC_BY_VBAT1    3700
#define CHARGER_VINDPM_DYNAMIC_VALUE1      4000
#define CHARGER_VINDPM_DYNAMIC_BY_VBAT2    4200
#define CHARGER_VINDPM_DYNAMIC_VALUE2      4400
#define CHARGER_VINDPM_DYNAMIC_VALUE3      4600
#define FLOAT_DELAY_TIME                   5000
#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
#define WAIT_USB_RDY_TIME                  100
#define WAIT_USB_RDY_MAX_CNT               300
#endif

#define NORMAL_CHG_VOLTAGE_MAX             4490000
#define FAST_CHG_VOLTAGE_MAX               4560000
#define VOLTAGE_MAX                        11000000
#define CURRENT_MAX                        12400000
#define INPUT_CURRENT_LIMIT                6100000
#define CP_EN_MAIN_CHG_CURR                100
#define TYPICAL_CAPACITY                   7700000

#define _TO_STR(x) #x
#define TO_STR(x) _TO_STR(x)
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
#define PROJECT_NAME                        BM7A
#define TYPICAL_CAPACITY_MAH                7700
#define INPUT_POWER_LIMIT                   45
#define _MODEL_NAME(name, capacity, adpo_max) \
	name##_##capacity##mah_##adpo_max##w
#define MODEL_NAME(name, capacity, adpo_max) \
	_MODEL_NAME(name, capacity, adpo_max)
#endif

#define POWER_SUPPLY_MANUFACTURER          "LX"
#define POWER_SUPPLY_MODEL_NAME            "Main chg Driver"

#define MAIN_CHG_ITERM_MAX                 1700
#define FASTCHARGE_MIN_CURR_OFFSET         400
#define FASTCHARGE_MIN_CURR                2100
#define CHARGER_MANAGER_LOOP_TIME_TEMP     2000    // 2s
#define CHARGER_MANAGER_LOOP_TIME          5000    // 5s
#define CHARGER_MANAGER_LOOP_TIME_OUT      20000   // 20s
#define MAX_UEVENT_LENGTH                  50
#define SHUTDOWN_DELAY_VOL_LOW             2950
#define SHUTDOWN_DELAY_VOL_HIGH            3050
#define SHUTDOWN_DELAY_IBAT                -1000000

#define BATTERY_COLD_TEMP                  60
#define BATTERY_WARM_TEMP                  470
#define BATTERY_HOT_TEMP                   520

#define SUPER_CHARGE_POWER                 50

#define FCC_TAPER_STEP_MA                  50

#define MIAN_CHG_ADC_LENGTH                180
#define PD20_ICHG_MULTIPLE                 1800
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
#define FG_I2C_ERR_SOC                     15
#define FG_I2C_ERR_VBUS                    6500
#define FG_I2C_ERR_CURRENT_NOW             -500000
#endif

/* 22w5 reverse charge */
#define REVERSE_CHARGE_MIN_VOLT         7100
#define REVERSE_CHARGE_5V_BOOOST        5000
#define REVERSE_CHARGE_9V_BOOOST        9000
#define REVERSE_CHARGE_SOC_TH           50
#define REVERSE_CHARGE_BATTEMP_TH       0
#define REVERSE_CHARGE_THERMAL_TH       400
#define APLLE_SVID                      0x05ac
#define IPHONE_15_PID                   0x7510
#define IPHONE_16_PID                   0x7517
#define IPHONE_17_PID                   0x751b
#define IPHONE_17_PRO_PID               0x7519
#define IPHONE_17_PM_PID                0x7519
#define IPHONE_16_22W5_SOC              70
#define IPHONE_16_18W_SOC               60
#define IPHONE_17_22W5_SOC              90
#define IPHONE_17_18W_SOC               85


static bool is_mtbf_mode;

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
	"Unknown",
	"USB",
	"USB_CDP",
	"USB_DCP",
	"USB_HVDCP",
	"USB_FLOAT",
	"Non-Stand",
	"USB_HVDCP_3",
	"USB_HVDCP_3P5",
#ifdef FACTORY_BUILD
	"USB_PD",
	"USB_PPS",
	"PD_PPS",
#else
	"USB_PD",
	"PD_PPS",
	"PD_PPS",
#endif
	"OTG",
	"unknow",
};

enum reverse_quick_charge_state{
    REVCHG_NORMAL = 0,
    REVCHG_QUICK_9,
    REVCHG_QUICK_18,
    REVCHG_QUICK_22W5,
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

#define SC6601A_DEVICE_ID       0x62
#define NU6601_DEVICE_ID        0x61

typedef enum {
	SUBPMIC_UNKNOWN,
	SUBPMIC_SC6601,
	SUBPMIC_NU6601,
	SUBPMIC_TYPE_COUNT
} SubpmicType;

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
enum smart_chg_functype{
	SMART_CHG_STATUS_FLAG = 0,
	SMART_CHG_FEATURE_MIN_NUM = 1,
	SMART_CHG_NAVIGATION = 1,
	SMART_CHG_OUTDOOR_CHARGE,
	SMART_CHG_LOW_FAST = 3,
	SMART_CHG_ENDURANCE_PRO,
	SMART_CHG_BYPASS = 10,
	/* add new func here */
	SMART_CHG_FEATURE_MAX_NUM = 15,
};

enum smart_chg_scene_type {
    SMART_CHG_SCENE_NORMAL              = 0,
    SMART_CHG_SCENE_PHONE               = 5,
    SMART_CHG_SCENE_CLASS0              = 7,
    SMART_CHG_SCENE_YOUTUBE             = 8,
    SMART_CHG_SCENE_VIDEO               = 11,
    SMART_CHG_SCENE_VIDEOCHAT           = 14,
    SMART_CHG_SCENE_TGAME               = 18,
    SMART_CHG_SCENE_MGAME               = 19,
    SMART_CHG_SCENE_YUANSHEN            = 20,
    SMART_CHG_SCENE_XINGTIE             = 25,
    SMART_CHG_SCENE_PER_NORMAL          = 50,
    SMART_CHG_SCENE_PER_CLASS0          = 57,
    SMART_CHG_SCENE_PER_YOUTUBE         = 58,
    SMART_CHG_SCENE_PER_VIDEO           = 61,
    SMART_CHG_SCENE_HPGAME              = 501,
    SMART_CHG_SCENE_CGAME               = 700,
    SMART_CHG_SCENE_CGAME_2             = 702,
    SMART_CHG_SCENE_INDEX_MAX,
    SMART_CHG_SCENE_FAKE_CLEAR          = 999,
    SMART_CHG_SCENE_FAKE_START          = 1000,
    SMART_CHG_SCENE_FAKE_INDEX_MAX      = SMART_CHG_SCENE_FAKE_START + SMART_CHG_SCENE_INDEX_MAX,
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
	NORMAL = 0,
	BLACK_TO_BRIGHT = 1,
	BRIGHT = 2,
	BLACK = 3,
};

struct timespec {
    long tv_sec; /* seconds */
    long tv_nsec; /* nanoseconds */
};

#define CYCLE_COUNT_MAX 4
#endif

struct charger_manager {
	int pd_boost_cnt;
	int rp_work_count;
	struct device *dev;
	struct class lx_charger_class;
	wait_queue_head_t wait_queue;
	struct lx_jeita_info *jeita_chip;
	struct task_struct *thread;
	bool run_thread;
	bool first_plug;
	int authenticate;
	bool bms_auth_done;
	int ch;
	int vindpm_vot;
	bool pr_swap_test;
	int disable_chg;

	struct timer_list charger_timer;
	struct delayed_work second_detect_work;
	struct delayed_work pd_config_check_work;
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

	/* flag add here */
	int pd_active;
	int ffc;
	bool cp_enable;
	bool index_40_to_45;
	bool fv_overvoltage_flag;
	struct delayed_work charge_iterm_monitor_work;
	bool is_pr_swap;
	bool pd_contract_update;
	int qc3_mode;
	int input_suspend;
	bool qc_detected;
	bool adapter_plug_in;
	bool charger_online;
	bool delay_iindpm;
	bool shutdown_delay;
	bool last_shutdown_delay;
	int soc;
	int rsoc;
	int ibat;
	int avg_ibat;
	int vbat;
	int tbat;
	int vbus;
	int ibus;
	int chg_status;
	int chg_state;
	enum vbus_type vbus_type;
	int v_iterm;
	bool iterm_changed;
	bool ctoc_chg;
	bool vbus_online;
	u32 full_cnt;
	u32 recharge_cnt;
	int fg_full_cnt;
	bool charge_full;
	bool bbc_charge_done;
	bool charge_en;
	int fake_batt_status;
	int fake_batt_status_hot;
	bool recharge;
	bool is_eu_mode;
	bool prp_is_enable;
	struct timespec64 ts64;
	struct timespec tv;
	struct rtc_time tm;

	int32_t chg_adc[ADC_GET_MAX];
	int typec_mode;
	bool poweroff_flag;
	struct delayed_work power_off_check_work;

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
	struct votable *main_chg_disable_real_votable;
	struct votable *cp_disable_votable;
	struct votable *total_fcc_votable;
	const char *total_effect_voter;
	const char *icharge_effect_voter;
	const char *input_effect_voter;
	const char *fv_effect_voter;
	const char *iterm_effect_voter;
	int vote_fv;
	int vote_iterm;
	int vote_icharge;
	int vote_total_fcc;
	int vote_cp_disable;
	int vote_input_limit;

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
	int apdo_max;
	int power_max;
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
	bool setshipmode;
	bool endurance_protect_flag;
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	struct smart_chg smart_charge[SMART_CHG_FEATURE_MAX_NUM + 1];
	int smart_chg_cmd;
	struct delayed_work xm_charge_work;
	struct delayed_work dfx_stand_chg_slow_check_work;
	int last_fcc;
	int none_standard_count;
	bool dfx_first_check_flag;
	bool dfx_stand_chg_check_flag;
	bool dfx_first_work_flag;
	bool smart_ctrl_en;

	int smart_batt;
	int smart_fv;
	bool night_charging;
	bool night_charging_flag;
	int outdoor_flag;
	int cyclecount[CYCLE_COUNT_MAX];
	int cycle_count;
	int dropfv[CYCLE_COUNT_MAX];
	int fv_againg;
	struct charger_screen_monitor sm;
	int thermal_board_temp;
	int thermal_scene;
	int smart_bypass_mode;
	int smart_bypass_fcc;
	struct notifier_block lx_charger_chain_nb;
	int *pd_thermal_mitigation_fast;
	bool low_fast_plugin_flag;
	bool pps_fast_mode;
        int low_fast_ffc;
	enum blank_flag b_flag;
	int  is_full_flag;
	struct votable	*is_full_votable;
#if IS_ENABLED(CONFIG_MIEV)
	int dfx_none_standard_flag;
	int dfx_batt_linker_absent_flag;
	int dfx_cp_err_flag;
	int dfx_fg_i2c_err_flag;
	int dfx_bat_temp_abnormal_flag;
	int dfx_lpd_check_flag;
	/*add new*/
	int dfx_cp_ibat_ocp_flag;
	int dfx_cp_vbat_ovp_flag;
	int dfx_cp_ibus_ocp_flag;
	int dfx_cp_vbus_ovp_flag;
	int dfx_cp_tdie_hot_flag;
	int dfx_cp_enable_flag;
	int dfx_pd_pps_authen_flag;
	int dfx_global_soc_over_80_flag;
	int dfx_global_soc_trig_flag;
	int dfx_high_temp_discharging_flag;
	int dfx_low_temp_discharging_flag;
	int dfx_soc_not_full_flag;
	int dfx_battery_auth_fail_flag;
	int dfx_none_standard_charge_slow_flag;
	
#endif
#endif
	bool temp_triggered;
	int source_boost_status;
	int rc_curr_min_cnt;
	int last_pdo_caps;
	bool revchg_bcl;
	int ibat_check_cnt;
	int cur_mode;
	int last_mode;
	int reverse_disconnect;
	int is_iphone;
	struct delayed_work reverse_quick_charge_work;
	struct delayed_work force_set_src_work;
	struct delayed_work src_switch_drp_work;
	struct delayed_work soft_cid_work;
	struct wakeup_source *reverse_charge_wakelock;
	bool otg_status;
	bool reverse_quick_charge;
	bool force_source;
	struct delayed_work replenish_work;
	bool typec_attach;
	bool ui_cc_toggle;
	bool cid_status;
	bool last_cc_toggle_en;
	bool cc_toggle_en;
	bool soft_cid_start;
	bool control_cc_toggle;
	bool audio_usb_cctoggle;
	struct mutex scid_lock;
	struct delayed_work hrtime_otg_work;
	struct alarm rust_det_work_timer;
	struct delayed_work set_cc_drp_work;
	struct delayed_work rust_detection_work;
	struct delayed_work tcpc_class_complete_init_work;
	bool swcid_bright;
	bool audio_cctog;

#if IS_ENABLED(CONFIG_RUST_DETECTION)
	/* lpd */
	struct charger_dev *fsa4480_chg_dev;
	int lpd_enable;
	int lpd_status;
	int lpd_charging;
	int lpd_control;
	int decrease_volt;
#endif
#ifdef FACTORY_BUILD
	int ato_soc_user_control;
#endif
	int float_count;
};

const static char * adc_name[] = {
	"VBUS","VSYS","VBAT","VAC","IBUS","IBAT","TSBUS","TSBAT","TDIE",
};

/*********extern func/struct/int start***********/
int lx_charger_node_init(struct charger_manager *manager);
int psy_unregister_cooler(struct charger_manager *manager);
int charger_manager_usb_psy_register(struct charger_manager *manager);
int charger_manager_batt_psy_register(struct charger_manager *manager);
int lx_usb_sysfs_create_group(struct charger_manager *manager);
int lx_batt_sysfs_create_group(struct charger_manager *manager);
extern void disable_chg_comm_ctrl(struct charger_manager *manager, const char *client_str, bool en);
void smblib_set_prop_batt_status(struct charger_manager *manager,  const union power_supply_propval *val);
void lx_set_prop_system_temp_level(struct charger_manager *manager, char *voter_name);
#ifndef FACTORY_BUILD
void manual_set_cc_toggle(bool en);
void manual_get_cc_toggle(bool *cc_toggle);
bool manual_get_cid_status(void);
bool manual_get_attach_status(void);
void manual_set_cid_status(bool cid_status);
#endif
int charger_manager_get_current(struct charger_manager *manager, int *curr);
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
void xm_uevent_report(struct charger_manager *manager);
#endif
extern struct chargerpump_policy *g_policy;
extern bool is_mtbf_mode_func(void);

/*********extern func/struct/int end***********/
#endif
