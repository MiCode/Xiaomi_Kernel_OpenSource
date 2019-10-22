/*
 * State machine for qc3 when it works on cp
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#define pr_fmt(fmt)	"[FC2-PM]: %s: " fmt, __func__
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/poll.h>

#include "cp_qc30.h"

#ifdef pr_debug
#undef pr_debug
#define pr_debug pr_err
#endif

#define BATT_MAX_CHG_VOLT		4400
#define BATT_FAST_CHG_CURR		3600
#define	BUS_OVP_THRESHOLD		12000
#define	BUS_OVP_ALARM_THRESHOLD		9500

#define BUS_VOLT_INIT_UP		200

#define BAT_VOLT_LOOP_LMT		BATT_MAX_CHG_VOLT
#define BAT_CURR_LOOP_LMT		BATT_FAST_CHG_CURR
#define BUS_VOLT_LOOP_LMT		BUS_OVP_THRESHOLD

#define VOLT_UP		true
#define VOLT_DOWN	false

#define ADC_ERR			1
#define CP_ENABLE_FAIL			2

static struct sys_config sys_config = {
	.bat_volt_lp_lmt		= BAT_VOLT_LOOP_LMT,
	.bat_curr_lp_lmt		= BAT_CURR_LOOP_LMT/* + 1000*/,
	.bus_volt_lp_lmt		= BUS_VOLT_LOOP_LMT,
	.bus_curr_lp_lmt		= BAT_CURR_LOOP_LMT >> 1,

	.fc2_taper_current		= 1600,
	.flash2_policy.down_steps	= -1,
	.flash2_policy.volt_hysteresis	= 50,

	.min_vbat_start_flash2		= 3500,
	.cp_sec_enable			= true,
};

struct cp_qc30_data {
	struct device *dev;
	int			bat_volt_max;
	int			bat_curr_max;
	int			bus_volt_max;
	int			bus_curr_max;
	bool			cp_sec_enable;

	/* notifiers */
	struct notifier_block	nb;
};

static pm_t pm_state;

static int fc2_taper_timer;
static int ibus_lmt_change_timer;

static struct power_supply *cp_get_sw_psy(void)
{

	if (!pm_state.sw_psy)
		pm_state.sw_psy = power_supply_get_by_name("battery");

	return pm_state.sw_psy;
}

static struct power_supply *cp_get_usb_psy(void)
{

	if (!pm_state.usb_psy)
		pm_state.usb_psy = power_supply_get_by_name("usb");

	return pm_state.usb_psy;
}

static struct power_supply *cp_get_bms_psy(void)
{
	if (!pm_state.bms_psy)
		pm_state.bms_psy = power_supply_get_by_name("bms");

	return pm_state.bms_psy;
}

static struct power_supply *cp_get_fc_psy(void)
{
	if (!pm_state.fc_psy) {
		if (sys_config.cp_sec_enable)
			pm_state.fc_psy = power_supply_get_by_name("bq2597x-master");
		else
			pm_state.fc_psy = power_supply_get_by_name("bq2597x-standalone");
	}

	return pm_state.fc_psy;
}

static void cp_update_bms_ibat(void)
{
	int ret;
	struct power_supply *psy;
	union power_supply_propval val = {0,};

	psy = cp_get_bms_psy();
	if (!psy)
		return;

	ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	if (!ret)
		pm_state.ibat_now = -(val.intval / 1000);

}

/* set bq27z561 fastcharge mode to enable or disabled */
static int qc3_set_bms_fastcharge_mode(bool enable)
{
	union power_supply_propval pval = {0,};
	int rc;

	cp_get_bms_psy();

	if (!pm_state.bms_psy)
		return 0;

	pval.intval = enable;

	rc = power_supply_set_property(pm_state.bms_psy,
				POWER_SUPPLY_PROP_FASTCHARGE_MODE, &pval);
	if (rc < 0) {
		pr_info("Couldn't write fastcharge mode:%d\n", rc);
		return rc;
	}

	pm_state.bms_fastcharge_mode = enable;

	return 0;
}

static void cp_update_sw_status(void)
{
	int ret;
	struct power_supply *psy;
	union power_supply_propval val = {0,};

	psy = cp_get_sw_psy();
	if (!psy)
		return;

	ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED, &val);
	if (!ret)
		pm_state.sw_chager.charge_enabled = val.intval;
}

static void cp_update_fc_status(void)
{
	int ret;
	struct power_supply *psy;
	union power_supply_propval val = {0,};

	psy = cp_get_fc_psy();
	if (!psy)
		return;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TI_BATTERY_VOLTAGE, &val);
	if (!ret)
		pm_state.bq2597x.vbat_volt = val.intval;

	/*ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TI_BATTERY_CURRENT, &val);
	if (!ret)
		pm_state.bq2597x.ibat_curr = val.intval; */

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TI_BUS_VOLTAGE, &val);
	if (!ret)
		pm_state.bq2597x.vbus_volt = val.intval;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TI_BUS_CURRENT, &val);
	if (!ret)
		pm_state.bq2597x.ibus_curr = val.intval;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TI_BUS_TEMPERATURE, &val);
	if (!ret)
		pm_state.bq2597x.bus_temp = val.intval;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TI_BATTERY_TEMPERATURE, &val);
	if (!ret)
		pm_state.bq2597x.bat_temp = val.intval;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TI_DIE_TEMPERATURE, &val);
	if (!ret)
		pm_state.bq2597x.die_temp = val.intval;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TI_BATTERY_PRESENT, &val);
	if (!ret)
		pm_state.bq2597x.batt_pres = val.intval;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TI_VBUS_PRESENT, &val);
	if (!ret)
		pm_state.bq2597x.vbus_pres = val.intval;

	if (pm_state.bq2597x.vbus_pres == 1) {
		cp_update_bms_ibat();
		pm_state.bq2597x.ibat_curr = pm_state.ibat_now;
	}

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CHARGE_ENABLED, &val);
	if (!ret)
		pm_state.bq2597x.charge_enabled = val.intval;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TI_ALARM_STATUS, &val);
	if (!ret) {
		pm_state.bq2597x.bat_ovp_alarm = !!(val.intval & BAT_OVP_ALARM_MASK);
		pm_state.bq2597x.bat_ocp_alarm = !!(val.intval & BAT_OCP_ALARM_MASK);
		pm_state.bq2597x.bus_ovp_alarm = !!(val.intval & BUS_OVP_ALARM_MASK);
		pm_state.bq2597x.bus_ocp_alarm = !!(val.intval & BUS_OCP_ALARM_MASK);
		pm_state.bq2597x.bat_ucp_alarm = !!(val.intval & BAT_UCP_ALARM_MASK);
		pm_state.bq2597x.bat_therm_alarm = !!(val.intval & BAT_THERM_ALARM_MASK);
		pm_state.bq2597x.bus_therm_alarm = !!(val.intval & BUS_THERM_ALARM_MASK);
		pm_state.bq2597x.die_therm_alarm = !!(val.intval & DIE_THERM_ALARM_MASK);
	}

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TI_FAULT_STATUS, &val);
	if (!ret) {
		pm_state.bq2597x.bat_ovp_fault = !!(val.intval & BAT_OVP_FAULT_MASK);
		pm_state.bq2597x.bat_ocp_fault = !!(val.intval & BAT_OCP_FAULT_MASK);
		pm_state.bq2597x.bus_ovp_fault = !!(val.intval & BUS_OVP_FAULT_MASK);
		pm_state.bq2597x.bus_ocp_fault = !!(val.intval & BUS_OCP_FAULT_MASK);
		pm_state.bq2597x.bat_therm_fault = !!(val.intval & BAT_THERM_FAULT_MASK);
		pm_state.bq2597x.bus_therm_fault = !!(val.intval & BUS_THERM_FAULT_MASK);
		pm_state.bq2597x.die_therm_fault = !!(val.intval & DIE_THERM_FAULT_MASK);
	}

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TI_REG_STATUS, &val);
	if (!ret) {
		pm_state.bq2597x.vbat_reg = !!(val.intval & VBAT_REG_STATUS_MASK);
		pm_state.bq2597x.ibat_reg = !!(val.intval & IBAT_REG_STATUS_MASK);
	}
}


static int cp_enable_fc(bool enable)
{
	int ret;
	struct power_supply *psy;
	union power_supply_propval val = {0,};

	psy = cp_get_fc_psy();
	if (!psy)
		return -ENODEV;

	val.intval = enable;
	ret = power_supply_set_property(psy,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);

	return ret;
}

static int cp_set_qc_bus_protections(int hvdcp3_type)
{
	int ret;
	struct power_supply *psy;
	union power_supply_propval val = {0,};

	psy = cp_get_fc_psy();
	if (!psy)
		return -ENODEV;

	val.intval = hvdcp3_type;
	ret = power_supply_set_property(psy,
			POWER_SUPPLY_PROP_TI_SET_BUS_PROTECTION_FOR_QC3, &val);

	return ret;
}

static int cp_enable_sw(bool enable)
{
	int ret;
	struct power_supply *psy;
	union power_supply_propval val = {0,};

	psy = cp_get_sw_psy();
	if (!psy)
		return -ENODEV;

	val.intval = enable;
	ret = power_supply_set_property(psy,
			POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED, &val);

	return ret;
}

static int cp_check_fc_enabled(void)
{
	int ret;
	struct power_supply *psy;
	union power_supply_propval val = {0,};

	psy = cp_get_fc_psy();
	if (!psy)
		return -ENODEV;

	ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
	if (!ret)
		pm_state.bq2597x.charge_enabled = !!val.intval;

	return ret;
}

static int cp_check_sw_enabled(void)
{
	int ret;
	struct power_supply *psy;
	union power_supply_propval val = {0,};

	psy = cp_get_sw_psy();
	if (!psy)
		return -ENODEV;

	ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED, &val);
	if (!ret)
		pm_state.sw_chager.charge_enabled = !!val.intval;

	pr_info("pm_state.sw_chager.charge_enabled: %d\n",
			pm_state.sw_chager.charge_enabled);
	return ret;
}

static int cp_tune_vbus_volt(bool up)
{
	int ret;
	struct power_supply *psy;
	union power_supply_propval val = {0,};

	psy = cp_get_sw_psy();
	if (!psy)
		return -ENODEV;

	if (up)
		val.intval = POWER_SUPPLY_DP_DM_DP_PULSE;
	else
		val.intval = POWER_SUPPLY_DP_DM_DM_PULSE;

	ret = power_supply_set_property(psy,
			POWER_SUPPLY_PROP_DP_DM_BQ, &val);

	pr_err("tune adapter voltage %s %s\n", up ? "up" : "down",
			ret ? "fail" : "successfully");

	return ret;

}

static int cp_reset_vbus_volt(void)
{
	int ret;
	struct power_supply *psy;
	union power_supply_propval val = {0,};

	psy = cp_get_sw_psy();
	if (!psy)
		return -ENODEV;

	val.intval = POWER_SUPPLY_DP_DM_FORCE_5V;

	ret = power_supply_set_property(psy,
			POWER_SUPPLY_PROP_DP_DM_BQ, &val);

	pr_err("reset vbus volt %s\n", ret ? "fail" : "successfully");

	return ret;

}

static int cp_get_usb_type(void)
{
	int ret;
	struct power_supply *psy;
	union power_supply_propval val = {0,};

	psy = cp_get_usb_psy();
	if (!psy)
		return -ENODEV;

	ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_REAL_TYPE, &val);
	if (!ret)
		pm_state.usb_type = val.intval;

	return ret;
}

static int cp_get_usb_present(void)
{
	int ret;
	struct power_supply *psy;
	union power_supply_propval val = {0,};

	psy = cp_get_usb_psy();
	if (!psy)
		return -ENODEV;

	ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_PRESENT, &val);
	if (!ret)
		pm_state.usb_present = val.intval;

	return ret;
}

static int cp_get_qc_hvdcp3_type(void)
{
	int ret;
	struct power_supply *psy;
	union power_supply_propval val = {0,};

	psy = cp_get_usb_psy();
	if (!psy)
		return -ENODEV;

	ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_HVDCP3_TYPE, &val);
	if (!ret)
		pm_state.hvdcp3_type = val.intval;

	return ret;
}


#define TAPER_TIMEOUT	10
#define IBUS_CHANGE_TIMEOUT  5
static int cp_flash2_charge(unsigned int port)
{
	static int ibus_limit;

	if (ibus_limit == 0)
		ibus_limit = pm_state.ibus_lmt_curr;

	pr_err("vbus=%d, ibus=%d, vbat=%d, ibat=%d\n",
				pm_state.bq2597x.vbus_volt,
				pm_state.bq2597x.ibus_curr,
				pm_state.bq2597x.vbat_volt,
				pm_state.bq2597x.ibat_curr);

	if (pm_state.bq2597x.vbus_volt <= 9500
		&& pm_state.bq2597x.ibus_curr < ibus_limit
		&& !pm_state.bq2597x.bus_ocp_alarm
		&& !pm_state.bq2597x.bus_ovp_alarm
		&& pm_state.bq2597x.vbat_volt < sys_config.bat_volt_lp_lmt - 100
		&& pm_state.bq2597x.ibat_curr < sys_config.bat_curr_lp_lmt - 700)

		cp_tune_vbus_volt(VOLT_UP);

	if (pm_state.bq2597x.bus_ocp_alarm
		|| pm_state.bq2597x.bus_ovp_alarm
		|| pm_state.bq2597x.vbat_reg
		|| pm_state.bq2597x.vbat_volt > sys_config.bat_volt_lp_lmt - 50
		|| pm_state.bq2597x.ibat_curr > sys_config.bat_curr_lp_lmt - 100) {

		cp_tune_vbus_volt(VOLT_DOWN);
	}

	cp_check_fc_enabled();

	/* battery overheat, stop charge */
	if (pm_state.bq2597x.bat_therm_fault)
		return -ADC_ERR;
	else if (pm_state.bq2597x.bus_ocp_fault
			|| pm_state.bq2597x.bat_ovp_fault
			|| pm_state.bq2597x.bus_ovp_fault)
		return -ADC_ERR;
	else if (!pm_state.bq2597x.charge_enabled)
		return -CP_ENABLE_FAIL;

	if (pm_state.bq2597x.vbat_volt > sys_config.bat_volt_lp_lmt - 100 &&
			pm_state.bq2597x.ibat_curr < sys_config.fc2_taper_current) {
		if (fc2_taper_timer++ > TAPER_TIMEOUT) {
			fc2_taper_timer = 0;
			return 1;
		}
	} else {
		fc2_taper_timer = 0;
	}

	return 0;
}

const unsigned char *pm_state_str[] = {
	"CP_STATE_ENTRY",
	"CP_STATE_DISCONNECT",
	"CP_STATE_SW_ENTRY",
	"CP_STATE_SW_ENTRY_2",

	"CP_STATE_SW_LOOP",
	"CP_STATE_FLASH2_ENTRY",
	"CP_STATE_FLASH2_ENTRY_1",

	"CP_STATE_FLASH2_ENTRY_3",

	"CP_STATE_FLASH2_TUNE",
	"CP_STATE_FLASH2_DELAY",
	"CP_STATE_STOP_CHARGE",
};

static void cp_move_state(pm_sm_state_t state)
{
#if 1
	pr_debug("pm_state change:%s -> %s\n",
		pm_state_str[pm_state.state], pm_state_str[state]);
	pm_state.state_log[pm_state.log_idx] = pm_state.state;
	pm_state.log_idx++;
	pm_state.log_idx %= PM_STATE_LOG_MAX;
#endif
	pm_state.state = state;
}

void cp_statemachine(unsigned int port)
{
	int ret;
	static int tune_vbus_retry;

	if (!pm_state.bq2597x.vbus_pres) {
		pm_state.state = CP_STATE_DISCONNECT;
	} else if (pm_state.state == CP_STATE_DISCONNECT) {
		pr_err("vbus connected\n");
		cp_move_state(CP_STATE_ENTRY);
	}

	switch (pm_state.state) {
	case CP_STATE_DISCONNECT:
		if (pm_state.bq2597x.charge_enabled) {
			cp_enable_fc(false);
			cp_check_fc_enabled();
		}

		if (!pm_state.sw_chager.charge_enabled) {
			cp_reset_vbus_volt();
			cp_enable_sw(true);
			cp_check_sw_enabled();
		}

		if (pm_state.bms_fastcharge_mode)
			qc3_set_bms_fastcharge_mode(false);
		pm_state.usb_type = 0;
		pm_state.sw_from_flash2 = false;
		pm_state.sw_fc2_init_fail = false;
		pm_state.sw_near_cv = false;
		sys_config.bat_curr_lp_lmt = HVDCP3_CLASS_A_BAT_CURRENT_MA;
		sys_config.bus_curr_lp_lmt = HVDCP3_CLASS_A_BUS_CURRENT_MA;
		pm_state.ibus_lmt_curr = HVDCP3_CLASS_A_BUS_CURRENT_MA;
		cp_set_qc_bus_protections(HVDCP3_NONE);
		break;

	case CP_STATE_ENTRY:
		cp_get_usb_type();
		if (pm_state.usb_type == POWER_SUPPLY_TYPE_USB_HVDCP_3) {
			pr_err("vbus_volt:%d\n", pm_state.bq2597x.vbus_volt);
			cp_reset_vbus_volt();
			msleep(100);
			if (pm_state.bq2597x.vbat_volt < sys_config.min_vbat_start_flash2)
				cp_move_state(CP_STATE_SW_ENTRY);
			else if (pm_state.bq2597x.vbat_volt > sys_config.bat_volt_lp_lmt - 200) {
				pm_state.sw_near_cv = true;
				cp_move_state(CP_STATE_SW_ENTRY);
			} else {
				cp_move_state(CP_STATE_FLASH2_ENTRY);
			}
		}
		break;

	case CP_STATE_SW_ENTRY:
		cp_reset_vbus_volt();
		if (pm_state.bq2597x.charge_enabled) {
			cp_enable_fc(false);
			cp_check_fc_enabled();
		}

		if (!pm_state.bq2597x.charge_enabled)
			cp_move_state(CP_STATE_SW_ENTRY_2);
		break;

	case CP_STATE_SW_ENTRY_2:
		pr_err("enable sw charger and check enable\n");
		cp_enable_sw(true);
		cp_check_sw_enabled();
		if (pm_state.sw_chager.charge_enabled)
			cp_move_state(CP_STATE_SW_LOOP);
		break;

	case CP_STATE_SW_LOOP:
		if (!pm_state.sw_from_flash2 && !pm_state.sw_near_cv) {
			if (pm_state.bq2597x.vbat_volt > sys_config.min_vbat_start_flash2) {
				pr_err("battery volt: %d is ok, proceeding to flash charging...\n",
					pm_state.bq2597x.vbat_volt);
				cp_move_state(CP_STATE_FLASH2_ENTRY);
				break;
			}
		}
		break;

	case CP_STATE_FLASH2_ENTRY:
		if (pm_state.sw_chager.charge_enabled) {
			cp_enable_sw(false);
			cp_check_sw_enabled();
		}

		if (!pm_state.sw_chager.charge_enabled) {
			cp_move_state(CP_STATE_FLASH2_ENTRY_1);
			tune_vbus_retry = 0;
		}

		cp_get_qc_hvdcp3_type();
		if (pm_state.hvdcp3_type == HVDCP3_CLASSB_27W) {
			sys_config.bat_curr_lp_lmt = HVDCP3_CLASS_B_BAT_CURRENT_MA;
			sys_config.bus_curr_lp_lmt = HVDCP3_CLASS_B_BUS_CURRENT_MA;
			pm_state.ibus_lmt_curr = sys_config.bus_curr_lp_lmt;
			cp_set_qc_bus_protections(HVDCP3_CLASSB_27W);
		} else if (pm_state.hvdcp3_type == HVDCP3_CLASSA_18W) {
			sys_config.bat_curr_lp_lmt = HVDCP3_CLASS_A_BAT_CURRENT_MA;
			sys_config.bus_curr_lp_lmt = HVDCP3_CLASS_A_BUS_CURRENT_MA;
			pm_state.ibus_lmt_curr = sys_config.bus_curr_lp_lmt;
			cp_set_qc_bus_protections(HVDCP3_CLASSA_18W);
		} else {
			cp_set_qc_bus_protections(HVDCP3_NONE);
		}
		break;

	case CP_STATE_FLASH2_ENTRY_1:
		cp_update_fc_status();
		if (pm_state.bq2597x.vbus_volt < (pm_state.bq2597x.vbat_volt * 2 + BUS_VOLT_INIT_UP - 50)) {
			tune_vbus_retry++;
			cp_tune_vbus_volt(VOLT_UP);
	    } else {
			pr_err("voltage tuned above expected voltage, retry %d times\n", tune_vbus_retry);
			cp_move_state(CP_STATE_FLASH2_ENTRY_3);
			break;
	    }

		if (tune_vbus_retry > 20) {
			pr_err("Failed to tune adapter volt into valid range, charge with switching charger\n");
			pm_state.sw_fc2_init_fail = true;
			cp_move_state(CP_STATE_SW_ENTRY);
		}
		break;

	case CP_STATE_FLASH2_ENTRY_3:
		if (pm_state.bq2597x.vbus_volt >
				(pm_state.bq2597x.vbat_volt * 2 + BUS_VOLT_INIT_UP + 200)) {
			pr_err("vbat volt is too high, wait it down\n");
			/* voltage is too high, wait for voltage down, keep charge disabled to discharge */
		} else {
			pr_err("vbat volt is ok, enable flash charging\n");
			if (!pm_state.bq2597x.charge_enabled) {
				cp_enable_fc(true);
				cp_check_fc_enabled();
				if (pm_state.bq2597x.charge_enabled)
					cp_move_state(CP_STATE_FLASH2_TUNE);
			}
			ibus_lmt_change_timer = 0;
			fc2_taper_timer = 0;
		}
		break;

	case CP_STATE_FLASH2_TUNE:
		if (!pm_state.bms_fastcharge_mode)
			qc3_set_bms_fastcharge_mode(true);

		ret = cp_flash2_charge(port);
		if (ret == -ADC_ERR) {
			pr_err("Move to stop charging:%d\n", ret);
			cp_move_state(CP_STATE_STOP_CHARGE);
			break;
		} else if (ret == -CP_ENABLE_FAIL || ret == 1) {
			pr_err("Move to switch charging:%d\n", ret);
			cp_move_state(CP_STATE_SW_ENTRY);
			pm_state.sw_from_flash2 = true;
			break;
		} else if (ret == 2) {
			pr_err("Move to switch charging, will try to recover to flash charging:%d\n",
					ret);
			cp_move_state(CP_STATE_SW_ENTRY);
		} else {
			cp_move_state(CP_STATE_FLASH2_DELAY);
		}
		break;

	case CP_STATE_FLASH2_DELAY:
		cp_move_state(CP_STATE_FLASH2_TUNE);
		break;

	case CP_STATE_STOP_CHARGE:
		pr_err("Stop charging\n");
		if (pm_state.bq2597x.charge_enabled) {
			cp_enable_fc(false);
			cp_check_fc_enabled();
		}
		if (pm_state.sw_chager.charge_enabled) {
			cp_enable_sw(false);
			cp_check_sw_enabled();
		}
		if (pm_state.bms_fastcharge_mode)
			qc3_set_bms_fastcharge_mode(false);
		break;

	default:
		pr_err("No state defined! Move to stop charging\n");
		if (pm_state.bms_fastcharge_mode)
			qc3_set_bms_fastcharge_mode(false);
		cp_move_state(CP_STATE_STOP_CHARGE);
		break;
	}
}

static void cp_workfunc(struct work_struct *work)
{
	cp_get_usb_type();

	cp_update_sw_status();
	cp_update_fc_status();

	cp_statemachine(0);

	cp_get_usb_present();
	pr_info("pm_state.usb_present: %d\n", pm_state.usb_present);
	/* check whether usb is present */
	if (pm_state.usb_present == 0) {
		cp_set_qc_bus_protections(HVDCP3_NONE);
		if (pm_state.bms_fastcharge_mode)
			qc3_set_bms_fastcharge_mode(false);
		return;
	}

	if (pm_state.usb_type == POWER_SUPPLY_TYPE_USB_HVDCP_3)
		schedule_delayed_work(&pm_state.qc3_pm_work, HZ);
}

static int cp_qc30_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
	struct power_supply *psy = v;

	if (ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if (strcmp(psy->desc->name, "usb") == 0) {
		cp_get_usb_type();
		if (pm_state.usb_type == POWER_SUPPLY_TYPE_USB_HVDCP_3)
			schedule_delayed_work(&pm_state.qc3_pm_work, 3*HZ);
	}

	return NOTIFY_OK;
}

static int cp_qc30_register_notifier(struct cp_qc30_data *chip)
{
	int rc;

	chip->nb.notifier_call = cp_qc30_notifier_call;
	rc = power_supply_reg_notifier(&chip->nb);
	if (rc < 0) {
		pr_err("Couldn't register psy notifier rc = %d\n", rc);
		return rc;
	}

	return 0;
}

static int cp_qc30_parse_dt(struct cp_qc30_data *chip)
{
	struct device_node *node = chip->dev->of_node;
	int rc = 0;

	if (!node) {
		pr_err("device tree node missing\n");
		return -EINVAL;
	}

	rc = of_property_read_u32(node,
			"mi,qc3-bat-volt-max", &chip->bat_volt_max);
	if (rc < 0)
		pr_err("qc3-bat-volt-max property missing, use default val\n");
	else
		sys_config.bat_volt_lp_lmt = chip->bat_volt_max;

	rc = of_property_read_u32(node,
			"mi,qc3-bat-curr-max", &chip->bat_curr_max);
	if (rc < 0)
		pr_err("qc3-bat-curr-max property missing, use default val\n");
	else
		sys_config.bat_curr_lp_lmt = chip->bat_curr_max;

	rc = of_property_read_u32(node,
			"mi,qc3-bus-volt-max", &chip->bus_volt_max);
	if (rc < 0)
		pr_err("qc3-bus-volt-max property missing, use default val\n");
	else
		sys_config.bus_volt_lp_lmt = chip->bus_volt_max;

	rc = of_property_read_u32(node,
			"mi,qc3-bus-curr-max", &chip->bus_curr_max);
	if (rc < 0)
		pr_err("qc3-bus-curr-max property missing, use default val\n");
	else
		sys_config.bus_curr_lp_lmt = chip->bus_curr_max;

	chip->cp_sec_enable = of_property_read_bool(node,
				"mi,cp-sec-enable");

	sys_config.cp_sec_enable = chip->cp_sec_enable;

	return rc;
}

static int cp_qc30_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct cp_qc30_data *chip;

	pr_info("%s enter\n", __func__);

	chip = devm_kzalloc(dev, sizeof(struct cp_qc30_data), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = dev;
	ret = cp_qc30_parse_dt(chip);
	if (ret < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, chip);

	pm_state.state = CP_STATE_DISCONNECT;
	pm_state.usb_type = POWER_SUPPLY_TYPE_UNKNOWN;
	pm_state.ibus_lmt_curr = sys_config.bus_curr_lp_lmt;

	INIT_DELAYED_WORK(&pm_state.qc3_pm_work, cp_workfunc);

	cp_qc30_register_notifier(chip);

	pr_info("charge pump qc3 probe\n");

	return ret;
}

static int cp_qc30_remove(struct platform_device *pdev)
{
	cancel_delayed_work(&pm_state.qc3_pm_work);
	return 0;
}

static const struct of_device_id cp_qc30_of_match[] = {
	{ .compatible = "xiaomi,cp-qc30", },
	{},
};

static struct platform_driver cp_qc30_driver = {
	.driver = {
		.name = "cp-qc30",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(cp_qc30_of_match),
	},
	.probe = cp_qc30_probe,
	.remove = cp_qc30_remove,
};

module_platform_driver(cp_qc30_driver);
MODULE_AUTHOR("Fei Jiang<jiangfei1@xiaomi.com>");
MODULE_DESCRIPTION("Xiaomi cp qc30");
MODULE_LICENSE("GPL");

