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
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <../../../usb/typec/tcpc/inc/tcpm.h>

#define IIO_SECOND_CHANNEL_OFFSET 15
enum pm_state {
    PD_PM_STATE_ENTRY,
    PD_PM_STATE_FC2_ENTRY,
    PD_PM_STATE_FC2_ENTRY_1,
    PD_PM_STATE_FC2_ENTRY_2,
    PD_PM_STATE_FC2_ENTRY_3,
    PD_PM_STATE_FC2_TUNE,
    PD_PM_STATE_FC2_EXIT,
};

#define	BAT_OVP_FAULT_SHIFT         0
#define	BAT_OCP_FAULT_SHIFT         1
#define	BUS_OVP_FAULT_SHIFT         2
#define	BUS_OCP_FAULT_SHIFT         3
#define	BAT_THERM_FAULT_SHIFT       4
#define	BUS_THERM_FAULT_SHIFT       5
#define	DIE_THERM_FAULT_SHIFT       6

#define	BAT_OVP_FAULT_MASK          (1 << BAT_OVP_FAULT_SHIFT)
#define	BAT_OCP_FAULT_MASK          (1 << BAT_OCP_FAULT_SHIFT)
#define	BUS_OVP_FAULT_MASK          (1 << BUS_OVP_FAULT_SHIFT)
#define	BUS_OCP_FAULT_MASK          (1 << BUS_OCP_FAULT_SHIFT)
#define	BAT_THERM_FAULT_MASK        (1 << BAT_THERM_FAULT_SHIFT)
#define	BUS_THERM_FAULT_MASK        (1 << BUS_THERM_FAULT_SHIFT)
#define	DIE_THERM_FAULT_MASK        (1 << DIE_THERM_FAULT_SHIFT)

#define	BAT_OVP_ALARM_SHIFT         0
#define	BAT_OCP_ALARM_SHIFT         1
#define	BUS_OVP_ALARM_SHIFT         2
#define	BUS_OCP_ALARM_SHIFT         3
#define	BAT_THERM_ALARM_SHIFT       4
#define	BUS_THERM_ALARM_SHIFT       5
#define	DIE_THERM_ALARM_SHIFT       6
#define BAT_UCP_ALARM_SHIFT         7

#define	BAT_OVP_ALARM_MASK          (1 << BAT_OVP_ALARM_SHIFT)
#define	BAT_OCP_ALARM_MASK          (1 << BAT_OCP_ALARM_SHIFT)
#define	BUS_OVP_ALARM_MASK          (1 << BUS_OVP_ALARM_SHIFT)
#define	BUS_OCP_ALARM_MASK          (1 << BUS_OCP_ALARM_SHIFT)
#define	BAT_THERM_ALARM_MASK        (1 << BAT_THERM_ALARM_SHIFT)
#define	BUS_THERM_ALARM_MASK        (1 << BUS_THERM_ALARM_SHIFT)
#define	DIE_THERM_ALARM_MASK        (1 << DIE_THERM_ALARM_SHIFT)
#define	BAT_UCP_ALARM_MASK          (1 << BAT_UCP_ALARM_SHIFT)

#define VBAT_REG_STATUS_SHIFT       0
#define IBAT_REG_STATUS_SHIFT       1

#define VBAT_REG_STATUS_MASK        (1 << VBAT_REG_STATUS_SHIFT)
#define IBAT_REG_STATUS_MASK        (1 << VBAT_REG_STATUS_SHIFT)


struct sw_device {
    bool charge_enabled;
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
    int  vbus_volt_old;
    int  batt_temp;
    int  set_ibat_cur;
    int  batt_volt_max;
    int  ibat_curr;
    int  ibat_curr_cp;
    int  ibat_curr_sw;
    int  ibus_curr;
    int  ibus_curr_cp;
    int  ibus_curr_sw;
    int  battery_cycle;

    int  vbus_error_low;
    int	 vbus_error_high;

    int  bat_temp; 
    int  bus_temp;
    int  die_temp;
};

#define PM_STATE_LOG_MAX    32
struct usbpd_pm {
    struct tcpc_device *tcpc;
    struct notifier_block tcp_nb;
    struct charger_device *sw_chg;

    enum pm_state state;
    struct cp_device cp;
    struct cp_device cp_sec;

    struct sw_device sw;

    bool cp_sec_stopped;
    bool pd_active;
    bool pps_supported;
    bool is_pps_en_unlock;
    bool adapter_omf;
    bool psy_change_running;
    bool pps_leave;

    int hrst_cnt;
    int	request_voltage;
    int	request_current;
    int request_voltage_old;

	int	apdo_min_volt;
    int	apdo_max_volt;
    int	apdo_max_curr;
    int	apdo_selected_pdo;

    int	adapter_voltage;
    int	adapter_current;
    int	adapter_ptf;

    struct delayed_work pm_work;
    struct delayed_work tcp_work;
    struct delayed_work pd_work;

    struct notifier_block nb;

    //struct work_struct cp_psy_change_work;
    struct work_struct usb_psy_change_work;
    spinlock_t psy_change_lock;

    struct power_supply *usb_psy;
	struct power_supply *sw_psy;

	int pps_temp_flag;
	int voltage_max;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	struct iio_channel	**cp_iio;
	struct iio_channel	**cp_sec_iio;
	struct iio_channel	**bms_iio;
	struct iio_channel	**main_iio;
	struct iio_channel	**apdo_iio;
	struct platform_device *pdev;
	struct device *dev;
	int shutdown_flag;
#else
    struct power_supply *cp_psy;
    struct power_supply *cp_sec_psy;
	struct power_supply *bms_psy;
#endif
	bool isln8000flg;
};

struct pdpm_config {
    int	bat_volt_lp_lmt; /*bat volt loop limit*/
    int	bat_curr_lp_lmt;
    int	bus_volt_lp_lmt;
    int	bus_curr_lp_lmt;

    int	fc2_taper_current;
    int	fc2_steps;

    int	min_adapter_volt_required;
    int min_adapter_curr_required;

    int	min_vbat_for_cp;

    bool cp_sec_enable;
    bool fc2_disable_sw;		/* disable switching charger during flash charge*/

};

#endif /* SRC_PDLIB_USB_PD_POLICY_MANAGER_H_ */
