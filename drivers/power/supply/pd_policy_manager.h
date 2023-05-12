// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
 */

#ifndef __PD_POLICY_MANAGER_H__
#define __PD_POLICY_MANAGER_H__
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <../../usb/typec/tcpc/inc/tcpm.h>

enum pm_state {
    PD_PM_STATE_ENTRY,
    PD_PM_STATE_FC2_ENTRY,
    PD_PM_STATE_FC2_ENTRY_1,
    PD_PM_STATE_FC2_ENTRY_2,
    PD_PM_STATE_FC2_ENTRY_3,
    PD_PM_STATE_FC2_TUNE,
    PD_PM_STATE_FC2_EXIT,
};

enum battery_id{
	FIRST_SUPPLIER,
	SECOND_SUPPLIER,
	THIRD_SUPPLIER,
	UNKNOW_SUPPLIER,
};

struct sw_device {
    bool charge_enabled;

    int vbat_volt;
    int ibat_curr;
    int ibus_curr;
};

struct cp_device {
    bool charge_enabled;
    bool vbus_err_low;
    bool vbus_err_high;

    int  vbus_volt;
    int  ibus_curr;

    int  die_temp;
};

struct usbpd_pm {
    enum pm_state state;
    struct cp_device cp;
    struct cp_device cp_sec;
    struct sw_device sw;

    struct tcpc_device *tcpc;
    struct notifier_block tcp_nb;
    bool is_pps_en_unlock;
    int hrst_cnt;

    bool cp_sec_stopped;

    bool pd_active;
    bool pps_supported;
    bool shutdown_flag;
    bool pps_temp_flag;

    int	request_voltage;
    int	request_current;

    int fc2_taper_timer;
    int ibus_lmt_change_timer;

    int	apdo_max_volt;
    int	apdo_max_curr;
    int	apdo_selected_pdo;
    int bat_temp;
    int therm_curr;
    int bat_cycle;
    int is_pps_en;
    int batt_auth;
    int cp_isln8000_flag;

    int input_suspend;
    int is_stop_charge;

    struct delayed_work pm_work;

    struct notifier_block nb;

    bool   psy_change_running;
    struct work_struct cp_psy_change_work;
    struct work_struct usb_psy_change_work;
    spinlock_t psy_change_lock;

    struct power_supply *cp_psy;
    struct power_supply *cp_sec_psy;
    struct power_supply *sw_psy;
    struct power_supply *usb_psy;
    struct power_supply *bms_psy;
    struct power_supply *batt_psy;
    struct power_supply *verify_psy;

    struct iio_channel	**apdo_iio;
    struct platform_device *pdev;
	struct device *dev;
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
