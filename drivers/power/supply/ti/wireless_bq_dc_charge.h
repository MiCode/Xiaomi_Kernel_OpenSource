
#ifndef WIRELESS_BQ_DC_CHARGE_POLICY_MANAGER_H_
#define WIRELESS_BQ_DC_CHARGE_POLICY_MANAGER_H_
#include <linux/device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>

enum pm_state {
    CP_PM_STATE_ENTRY,
    CP_PM_STATE_FC2_ENTRY,
    CP_PM_STATE_FC2_ENTRY_1,
    CP_PM_STATE_FC2_ENTRY_2,
    CP_PM_STATE_FC2_ENTRY_3,
    CP_PM_STATE_FC2_TUNE,
    CP_PM_STATE_FC2_EXIT,
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

/* voters for wireless bq dc charge */
#define STEP_BMS_CHG_VOTER	"WL_STEP_BMS_CHG_VOTER"
#define BQ_TAPER_FCC_VOTER	"BQ_TAPER_FCC_VOTER"
#define BQ_TAPER_CELL_HGIH_FCC_VOTER	"BQ_TAPER_CELL_HGIH_FCC_VOTER"

#define WLDC_OPEN_DC_PATH_MAX_CNT			21
#define WLDC_OPEN_PATH_RX_IOUT_MIN           500
#define WLDC_VBAT_HIGH_TRH			4300
#define WLDC_REGULATE_STEP_MV			50
#define WLDC_MAX_VOL_THR_FOR_BQ			10000
#define WLDC_INIT_RX_VOUT_FOR_FC2			15000
//#define WLDC_DEFAULT_RX_VOUT_FOR_MAIN_CHARGER			5500
#define WLDC_XIAOMI_20W_IOUT_MAX			1000

/* defined already support quick wireless charge tx charger types */
#define ADAPTER_XIAOMI_QC3    0x09
#define ADAPTER_XIAOMI_PD     0x0a
#define ADAPTER_ZIMI_CAR_POWER    0x0b
#define ADAPTER_XIAOMI_PD_40W     0x0c
#define ADAPTER_XIAOMI_PD_50W     0x0e
#define ADAPTER_XIAOMI_PD_60W     0x0f
#define ADAPTER_VOICE_BOX     0x0d

#define NORMAL_ERR			1
#define VBAT_TOO_HIGH_ERR		2
#define RX_VOUT_SET_TOO_HIGH_ERR 	3
#define WL_DISCONNECTED_ERR             4

#define VBAT_HIGH_FOR_FC_HYS_MV		100
#define CAPACITY_TOO_HIGH_THR			95

#define WLDC_SOFT_TAPER_DECREASE_STEP_MA			100
#define WLDC_SOFT_TAPER_DECREASE_MIN_MA			1100

#define STEP_VFLOAT_INDEX_MAX			2


#define WLDC_BQ_TAPER_DECREASE_STEP_MA			200
#define WLDC_BQ_VBAT_REGULATION_STEP_MA			400

#define BQ_TAPER_HYS_MV			10
#define NON_FFC_BQ_TAPER_HYS_MV			50

#define CELL_VOLTAGE_HIGH_COUNT_MAX			2
#define CELL_VOLTAGE_MAX_COUNT_MAX			1


/* jeita related */
#define JEITA_WARM_THR			450
#define JEITA_WARM_THR_NON_QCOM_GAUGE			480
#define JEITA_COOL_NOT_ALLOW_CP_THR			100

#define MAX_THERMAL_LEVEL			15
#define JEITA_WARM_HYSTERESIS			20
#define JEITA_COOL_HYSTERESIS			5


struct sw_device {
	bool charge_enabled;
};

struct wireless_info {
	bool active;
	int max_volt;
	int max_curr;
	int step_mv;
};


struct cp_device {
	bool charge_enabled;

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

	bool therm_shutdown_flag;
	bool therm_shutdown_stat;

	bool vbat_reg;
	bool ibat_reg;

	int  vout_volt;
	int  vbat_volt;
	int  vbus_volt;
	int  ibat_curr;
	int  ibus_curr;
	int  bms_vbat_mv;

	int  bat_temp;
	int  bus_temp;
	int  die_temp;
};

#define PM_STATE_LOG_MAX    32
struct wireless_dc_device_info {
	struct device *dev;

	enum pm_state state;

	struct cp_device cp;
	struct cp_device cp_sec;

	struct sw_device sw;

	int	request_voltage;

	struct wireless_info wl_info;

	struct notifier_block nb;

	bool   psy_change_running;
	struct work_struct cp_psy_change_work;
	struct work_struct usb_psy_change_work;
	struct delayed_work wireles_dc_ctrl_work;
	struct work_struct wl_psy_change_work;

	spinlock_t psy_change_lock;

	struct votable		*fcc_votable;
	struct votable		*usb_icl_votable;
	struct power_supply *cp_psy;
	struct power_supply *cp_sec_psy;
	struct power_supply *sw_psy;
	struct power_supply *usb_psy;
	struct power_supply *bms_psy;
	struct power_supply *wl_psy;
	struct power_supply *dc_psy;

	/* dtsi properties */
	int			bat_volt_max;
	int			non_ffc_bat_volt_max;
	int			bat_curr_max;
	int			bus_volt_max;
	int			bus_curr_max;
	int			rx_iout_curr_max;
	bool		cp_sec_enable;
	bool		use_qcom_gauge;
	/* wireless policy related */
	int			rx_vout_set;
	int			rx_init_vout_for_fc2;

	/* jeita or thermal related */
	bool		jeita_triggered;
	bool		is_temp_out_fc2_range;
	bool		night_charging;
	int			warm_threshold_temp;

	/* bq taper related */
	int			over_cell_vol_high_count;
	int			over_cell_vol_max_count;
	int			step_charge_high_vol_curr_max;
	int			cell_vol_high_threshold_mv;
	int			cell_vol_max_threshold_mv;

};

struct cppm_config {
	int	bat_volt_lp_lmt; /*bat volt loop limit*/
	int	bat_curr_lp_lmt;
	int	bus_volt_lp_lmt;
	int	bus_curr_lp_lmt;

	int	fc2_taper_current;
	int	fc2_steps;

	int	min_adapter_volt_required;
	int min_adapter_curr_required;
	int	min_vbat_for_cp;

	bool	cp_sec_enable;
	bool	fc2_disable_sw;		/* disable switching charger during flash charge*/
};

#endif /* WIRELESS_BQ_DC_CHARGE_POLICY_MANAGER_H_ */
