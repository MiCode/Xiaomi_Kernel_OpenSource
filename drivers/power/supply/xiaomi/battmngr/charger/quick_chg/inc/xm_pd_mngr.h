
#ifndef XM_PD_MNGR_H_
#define XM_PD_MNGR_H_

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/usb/usbpd.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/poll.h>

#include <linux/usb/tcpc/tcpm.h>
#include <linux/battmngr/xm_battmngr_iio.h>
#include <linux/battmngr/battmngr_voter.h>
#include <linux/battmngr/battmngr_notifier.h>

enum pm_state {
    PD_PM_STATE_ENTRY,
    PD_PM_STATE_FC2_ENTRY,
    PD_PM_STATE_FC2_ENTRY_1,
    PD_PM_STATE_FC2_ENTRY_2,
    PD_PM_STATE_FC2_ENTRY_3,
    PD_PM_STATE_FC2_TUNE,
    PD_PM_STATE_FC2_EXIT,
};

#define PROBE_CNT_MAX	50

#define PCA_PPS_CMD_RETRY_COUNT	2

#define PD_SRC_PDO_TYPE_FIXED		0
#define PD_SRC_PDO_TYPE_BATTERY		1
#define PD_SRC_PDO_TYPE_VARIABLE	2
#define PD_SRC_PDO_TYPE_AUGMENTED	3

#define BATT_MAX_CHG_VOLT		4480
#define BATT_FAST_CHG_CURR		6000
#define	BUS_OVP_THRESHOLD		12000
#define	BUS_OVP_ALARM_THRESHOLD		9500

#define BUS_VOLT_INIT_UP		800
#define MIN_ADATPER_VOLTAGE_11V 11000
#define CAPACITY_HIGH_THR	90

#define BAT_VOLT_LOOP_LMT		BATT_MAX_CHG_VOLT
#define BAT_CURR_LOOP_LMT		BATT_FAST_CHG_CURR
#define BUS_VOLT_LOOP_LMT		BUS_OVP_THRESHOLD

#define PM_WORK_RUN_NORMAL_INTERVAL		500
#define PM_WORK_RUN_QUICK_INTERVAL		200
#define PM_WORK_RUN_CRITICAL_INTERVAL		100

enum {
	PM_ALGO_RET_OK,
	PM_ALGO_RET_THERM_FAULT,
	PM_ALGO_RET_OTHER_FAULT,
	PM_ALGO_RET_CHG_DISABLED,
	PM_ALGO_RET_TAPER_DONE,
	PM_ALGO_RET_UNSUPPORT_PPSTA,
	PM_ALGO_RET_NIGHT_CHARGING,
};

enum adapter_cap_type {
	XM_PD_FIXED,
	XM_PD_APDO,
	XM_PD_APDO_START,
	XM_PD_APDO_END,
};

#define	BAT_OVP_FAULT_SHIFT			0
#define	BAT_OCP_FAULT_SHIFT			1
#define	BUS_OVP_FAULT_SHIFT			2
#define	BUS_OCP_FAULT_SHIFT			3
#define	BAT_THERM_FAULT_SHIFT			4
#define	BUS_THERM_FAULT_SHIFT			5
#define	DIE_THERM_FAULT_SHIFT			6

#define	BAT_OVP_FAULT_MASK		(1 << BAT_OVP_FAULT_SHIFT)
#define	BAT_OCP_FAULT_MASK		(1 << BAT_OCP_FAULT_SHIFT)
#define	BUS_OVP_FAULT_MASK		(1 << BUS_OVP_FAULT_SHIFT)
#define	BUS_OCP_FAULT_MASK		(1 << BUS_OCP_FAULT_SHIFT)
#define	BAT_THERM_FAULT_MASK		(1 << BAT_THERM_FAULT_SHIFT)
#define	BUS_THERM_FAULT_MASK		(1 << BUS_THERM_FAULT_SHIFT)
#define	DIE_THERM_FAULT_MASK		(1 << DIE_THERM_FAULT_SHIFT)

#define	BAT_OVP_ALARM_SHIFT			0
#define	BAT_OCP_ALARM_SHIFT			1
#define	BUS_OVP_ALARM_SHIFT			2
#define	BUS_OCP_ALARM_SHIFT			3
#define	BAT_THERM_ALARM_SHIFT			4
#define	BUS_THERM_ALARM_SHIFT			5
#define	DIE_THERM_ALARM_SHIFT			6
#define BAT_UCP_ALARM_SHIFT			7

#define	BAT_OVP_ALARM_MASK		(1 << BAT_OVP_ALARM_SHIFT)
#define	BAT_OCP_ALARM_MASK		(1 << BAT_OCP_ALARM_SHIFT)
#define	BUS_OVP_ALARM_MASK		(1 << BUS_OVP_ALARM_SHIFT)
#define	BUS_OCP_ALARM_MASK		(1 << BUS_OCP_ALARM_SHIFT)
#define	BAT_THERM_ALARM_MASK		(1 << BAT_THERM_ALARM_SHIFT)
#define	BUS_THERM_ALARM_MASK		(1 << BUS_THERM_ALARM_SHIFT)
#define	DIE_THERM_ALARM_MASK		(1 << DIE_THERM_ALARM_SHIFT)
#define	BAT_UCP_ALARM_MASK		(1 << BAT_UCP_ALARM_SHIFT)

#define VBAT_REG_STATUS_SHIFT			0
#define IBAT_REG_STATUS_SHIFT			1

#define VBAT_REG_STATUS_MASK		(1 << VBAT_REG_STATUS_SHIFT)
#define IBAT_REG_STATUS_MASK		(1 << VBAT_REG_STATUS_SHIFT)

/* voters for usbpd */
#define BQ_TAPER_FCC_VOTER	"BQ_TAPER_FCC_VOTER"
#define BQ_TAPER_CELL_HGIH_FCC_VOTER	"BQ_TAPER_CELL_HGIH_FCC_VOTER"
#define NON_PPS_PD_FCC_VOTER	"NON_PPS_PD_FCC_VOTER"
#define MAIN_CHG_ICL_VOTER	"MAIN_CHG_ICL_VOTER"
#define FFC_DISABLE_CP_FV_VOTER	"FFC_DISABLE_CP_FV_VOTER"
#define USER_VOTER	"USER_VOTER"
#define SMART_BATTERY_FV	"SMART_BATTERY_FV"

#define MAIN_CHG_ICL		(3000 * 1000)
#define FFC_DISABLE_CP_FV	(4560 * 1000)
#define FFC_DISABLE_CP_FV_D	(4576 * 1000)

/* defined min fcc threshold for start bq direct charging */
#define START_DRIECT_CHARGE_FCC_MIN_THR			2000

/* product related */
#define PPS_VOL_MAX			11000
#define PPS_VOL_HYS			1000

#define STEP_MV			20
#define TAPER_VOL_HYS			80
#define TAPER_WITH_IBUS_HYS			60
#define TAPER_IBUS_THR			450
#define MAX_THERMAL_LEVEL			13
#define MAX_THERMAL_LEVEL_FOR_DUAL_BQ			9
#define THERMAL_LEVEL_11			11
#define THERMAL_LEVEL_12			12
#define THERMAL_11_VBUS_UP			300

#define FCC_MAX_MA_FOR_MASTER_BQ			6000
#define IBUS_THRESHOLD_MA_FOR_DUAL_BQ			2100
#define IBUS_THRESHOLD_MA_FOR_DUAL_BQ_LN8000		2500
#define IBUS_THR_MA_HYS_FOR_DUAL_BQ			200
#define IBUS_THR_TO_CLOSE_SLAVE_COUNT_MAX			40

/* jeita related */
#define JEITA_WARM_THR			480
#define JEITA_COOL_NOT_ALLOW_CP_THR			100
/*
 * add hysteresis for warm threshold to avoid flash
 * charge and normal charge switch frequently at
 * the warm threshold
 */
#define JEITA_HYSTERESIS			20

#define BQ_TAPER_HYS_MV			10
#define NON_FFC_BQ_TAPER_HYS_MV			50

#define BQ_TAPER_DECREASE_STEP_MA			200

#define CELL_VOLTAGE_HIGH_COUNT_MAX			2
#define CELL_VOLTAGE_MAX_COUNT_MAX			1

#define HIGH_VOL_THR_MV			4380
#define CRITICAL_HIGH_VOL_THR_MV			4480

#define TAPER_DONE_FFC_MA_LN8000		2500
#define TAPER_DONE_NORMAL_MA			2200

#define VBAT_HIGH_FOR_FC_HYS_MV			100
#define CAPACITY_TOO_HIGH_THR			95

#define CRITICAL_LOW_IBUS_THR			300

#define MAX_UNSUPPORT_PPS_CURRENT_MA			5500
#define NON_PPS_PD_FCC_LIMIT			(3000 * 1000)

#define POWER_SUPPLY_PD_ACTIVE		QTI_POWER_SUPPLY_PD_ACTIVE
#define POWER_SUPPLY_PD_PPS_ACTIVE	QTI_POWER_SUPPLY_PD_PPS_ACTIVE

/* APDO */
#define APDO_MAX_WATT			68000000
#define APDO_MAX_MV			20000


enum {
	POWER_SUPPLY_PPS_INACTIVE = 0,
	POWER_SUPPLY_PPS_NON_VERIFIED,
	POWER_SUPPLY_PPS_VERIFIED,
};

struct sw_device {
	bool charge_enabled;
	bool night_charging;
};

struct cp_device {
	bool charge_enabled;
	bool batt_connecter_present;

	bool batt_pres;
	bool vbus_pres;

	/* alarm/fault status */
	bool bat_ovp_fault;
	bool bat_ocp_fault;
	bool bus_ovp_fault;
	bool bus_ocp_fault;

	bool bat_ovp_alarm;
	bool bat_ocp_alarm;
	bool bus_ovp_alarm;
	bool bus_ocp_alarm;

	bool bat_ucp_alarm;

	bool bat_therm_alarm;
	bool bus_therm_alarm;
	bool die_therm_alarm;

	bool bat_therm_fault;
	bool bus_therm_fault;
	bool die_therm_fault;

	bool vbat_reg;
	bool ibat_reg;

	int  vbat_volt;
	int  fg_vbat_mv;
	int  vbus_volt;
	int  ibat_curr;
	int  ibus_curr;
	int  bat_temp;
	int  bus_temp;
	int  die_temp;
};

#define PM_STATE_LOG_MAX    32
struct usbpd_pm {
	struct device *dev;
	struct tcpc_device *tcpc;

	enum pm_state state;

	struct cp_device cp;
	struct cp_device cp_sec;

	struct sw_device sw;

	int	pd_active;
	bool pps_supported;
	bool fc2_exit_flag;

	int	request_voltage;
	int	request_current;

	int	apdo_max_volt;
	int	apdo_max_curr;
	int	apdo_maxwatt;

	struct delayed_work pm_work;
	struct delayed_work fc2_exit_work;

	struct notifier_block nb;
	struct notifier_block battmngr_nb;
	struct notifier_block tcp_nb;

	struct work_struct usb_psy_change_work;
	spinlock_t psy_change_lock;

	struct votable *fcc_votable;
	struct votable *fv_votable;
	struct votable *usb_icl_votable;
	struct votable *input_suspend_votable;
	struct votable *smart_batt_votable;
	struct power_supply *batt_psy;
	struct power_supply *usb_psy;

	/* dtsi properties */
	int	bat_volt_max;
	int	bat_curr_max;
	int	bus_volt_max;
	int	bus_curr_max;
	int	non_ffc_bat_volt_max;
	int	bus_curr_compensate;
	int	therm_level_threshold;
	int	pd_power_max;
	bool cp_sec_enable;
	/* jeita or thermal related */
	bool jeita_triggered;
	bool is_temp_out_fc2_range;
	int	battery_warm_th;

	/* bq taper related */
	int	over_cell_vol_high_count;
	int	over_cell_vol_max_count;
	int	step_charge_high_vol_curr_max;
	int	cell_vol_high_threshold_mv;
	int	cell_vol_max_threshold_mv;

	/* dual bq contrl related */
	bool no_need_en_slave_bq;
	int	slave_bq_disabled_check_count;
	int	master_ibus_below_critical_low_count;
	int	chip_ok_count;
	int	pd_auth_val;

	/*unsupport pps ta check count*/
	int	unsupport_pps_ta_check_count;

	/*pca_pps_tcp_notifier_call*/
	bool is_pps_en_unlock;
	int hrst_cnt;
};

struct pdpm_config {
	int	bat_volt_lp_lmt; /*bat volt loop limit*/
	int	bat_curr_lp_lmt;
	int	bus_volt_lp_lmt;
	int	bus_curr_lp_lmt;
	int	bus_curr_compensate;

	int	fc2_taper_current;
	int	fc2_steps;

	int	min_adapter_volt_required;
	int min_adapter_curr_required;
	int	min_vbat_for_cp;

	bool cp_sec_enable;
	bool fc2_disable_sw;		/* disable switching charger during flash charge*/
};

#endif /* XM_PD_MNGR_H_ */

