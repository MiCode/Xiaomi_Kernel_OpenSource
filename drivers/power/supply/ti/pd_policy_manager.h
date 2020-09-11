/*
 * usb_pd_policy_manager.h
 *
 *  Created on: Mar 27, 2017
 *      Author: a0220433
 */

#ifndef SRC_PDLIB_USB_PD_POLICY_MANAGER_H_
#define SRC_PDLIB_USB_PD_POLICY_MANAGER_H_
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/power/mtk_charger_intf_mi.h>

enum pm_state {
	PD_PM_STATE_ENTRY,
	PD_PM_STATE_FC2_ENTRY,
	PD_PM_STATE_FC2_ENTRY_1,
	PD_PM_STATE_FC2_ENTRY_2,
	PD_PM_STATE_FC2_ENTRY_3,
	PD_PM_STATE_FC2_TUNE,
	PD_PM_STATE_FC2_EXIT,
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

#define STEP_BMS_CHG_VOTER	"STEP_BMS_CHG_VOTER"
#define BQ_TAPER_FCC_VOTER	"BQ_TAPER_FCC_VOTER"

#define NON_VERIFIED_PPS_FCC_MAX		4800
#define START_DRIECT_CHARGE_FCC_MIN_THR			2000
#define MAX_THERMAL_LEVEL			15
#define BYPASS_IN		3000
#define BYPASS_OUT		3500
#define MAX_BYPASS_CURRENT_MA		3000
#define JEITA_WARM_THR			480
#define JEITA_COOL_NOT_ALLOW_CP_THR			100

#define PDO_MAX_NUM			7

#define JEITA_HYSTERESIS			20

#define LOW_POWER_PPS_CURR_THR			2000
#define XIAOMI_LOW_POWER_PPS_CURR_MAX			1500
#define XIAOMI_LOW_POWER_PPS_CURR_MAX			1500
#define PPS_VOL_MAX			11000
#define PPS_VOL_HYS			1000

#define STEP_MV			20
#define TAPER_VOL_HYS			80
#define TAPER_WITH_IBUS_HYS			60
#define TAPER_IBUS_THR			450

#define BQ_TAPER_HYS_MV			20
#define BQ_TAPER_DECREASE_STEP_MA			100
#define HIGH_VOL_THR_MV			4350

#define SC8551_TAPER_HYS_MV			5

struct sw_device {
	bool charge_enabled;
};

struct usbpd_pdo {
	bool pps;
	int type;
	int max_volt_mv;
	int min_volt_mv;
	int curr_ma;
	int pos;
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

	int  bat_temp;
	int  bus_temp;
	int  die_temp;

	int  sc8551_charge_mode;
	int  sc8551_stage;
};

#define PM_STATE_LOG_MAX    32
struct usbpd_pm {
	struct device *dev;

	enum pm_state state;

	struct cp_device cp;
	struct cp_device cp_sec;

	struct sw_device sw;

	bool	pd_active;
	bool	pps_supported;

	int	request_voltage;
	int	request_current;

	struct usbpd *pd;
	struct usbpd_pdo pdo[7];

	int	apdo_max_volt;
	int	apdo_max_curr;
	int	apdo_selected_pdo;

	int	adapter_voltage;
	int	adapter_current;
	int	adapter_ptf;
	bool	adapter_omf;

	struct delayed_work pm_work;

	struct notifier_block nb;

	bool   psy_change_running;
	struct work_struct cp_psy_change_work;
	struct work_struct usb_psy_change_work;
	spinlock_t psy_change_lock;

	struct votable		*fcc_votable;
	struct power_supply *cp_psy;
	struct power_supply *cp_sec_psy;
	struct power_supply *sw_psy;
	struct power_supply *usb_psy;
	struct power_supply *bms_psy;

	int			bat_volt_max;
	int			bat_curr_max;
	int			bus_volt_max;
	int			bus_curr_max;
	int			non_ffc_bat_volt_max;
	bool		cp_sec_enable;
	bool		sc_bypass_enable;

	bool			jeita_triggered;
	bool			is_temp_out_fc2_range;
	bool			pd_verified_checked;

	bool			sc8551_bypass;
};

struct pdpm_config {
	int	bat_volt_lp_lmt; /*bat volt loop limit*/
	int	bat_curr_lp_lmt;
	int	bus_volt_lp_lmt;
	int	bus_curr_lp_lmt;

	int	fc2_taper_current;
	int	fc2_taper_bus_current;
	int	fc2_steps;

	int	min_adapter_volt_required;
	int min_adapter_curr_required;
	int	min_vbat_for_cp;

	bool	cp_sec_enable;
	bool	fc2_disable_sw;
};

extern int usbpd_get_pps_status(struct usbpd *pd, u32 *status);
extern int usbpd_fetch_pdo(struct usbpd *pd, struct usbpd_pdo *pdos);
extern int usbpd_select_pdo(struct usbpd *pd, int pdo, int uv, int ua);
extern struct usbpd *smb_get_usbpd(void);
extern int charger_manager_set_current_limit(int data, int type);


#endif /* SRC_PDLIB_USB_PD_POLICY_MANAGER_H_ */
