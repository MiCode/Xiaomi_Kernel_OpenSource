/* Copyright (c) 2010-2011 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/i2c/smb137b.h>
#include <linux/power_supply.h>
#include <linux/msm-charger.h>

#define SMB137B_MASK(BITS, POS)  ((unsigned char)(((1 << BITS) - 1) << POS))

#define CHG_CURRENT_REG		0x00
#define FAST_CHG_CURRENT_MASK		SMB137B_MASK(3, 5)
#define PRE_CHG_CURRENT_MASK		SMB137B_MASK(2, 3)
#define TERM_CHG_CURRENT_MASK		SMB137B_MASK(2, 1)

#define INPUT_CURRENT_LIMIT_REG	0x01
#define IN_CURRENT_MASK			SMB137B_MASK(3, 5)
#define IN_CURRENT_LIMIT_EN_BIT		BIT(2)
#define IN_CURRENT_DET_THRESH_MASK	SMB137B_MASK(2, 0)

#define FLOAT_VOLTAGE_REG	0x02
#define STAT_OUT_POLARITY_BIT		BIT(7)
#define FLOAT_VOLTAGE_MASK		SMB137B_MASK(7, 0)

#define CONTROL_A_REG		0x03
#define AUTO_RECHARGE_DIS_BIT		BIT(7)
#define CURR_CYCLE_TERM_BIT		BIT(6)
#define PRE_TO_FAST_V_MASK		SMB137B_MASK(3, 3)
#define TEMP_BEHAV_BIT			BIT(2)
#define THERM_NTC_CURR_MODE_BIT		BIT(1)
#define THERM_NTC_47KOHM_BIT		BIT(0)

#define CONTROL_B_REG		0x04
#define STAT_OUTPUT_MODE_MASK		SMB137B_MASK(2, 6)
#define BATT_OV_ENDS_CYCLE_BIT		BIT(5)
#define AUTO_PRE_TO_FAST_DIS_BIT	BIT(4)
#define SAFETY_TIMER_EN_BIT		BIT(3)
#define OTG_LBR_WD_EN_BIT		BIT(2)
#define CHG_WD_TIMER_EN_BIT		BIT(1)
#define IRQ_OP_MASK			BIT(0)

#define PIN_CTRL_REG		0x05
#define AUTO_CHG_EN_BIT			BIT(7)
#define AUTO_LBR_EN_BIT			BIT(6)
#define OTG_LBR_BIT			BIT(5)
#define I2C_PIN_BIT			BIT(4)
#define PIN_EN_CTRL_MASK		SMB137B_MASK(2, 2)
#define OTG_LBR_PIN_CTRL_MASK		SMB137B_MASK(2, 0)

#define OTG_LBR_CTRL_REG	0x06
#define BATT_MISSING_DET_EN_BIT		BIT(7)
#define AUTO_RECHARGE_THRESH_MASK	BIT(6)
#define USB_DP_DN_DET_EN_MASK		BIT(5)
#define OTG_LBR_BATT_CURRENT_LIMIT_MASK	SMB137B_MASK(2, 3)
#define OTG_LBR_UVLO_THRESH_MASK	SMB137B_MASK(3, 0)

#define FAULT_INTR_REG		0x07
#define SAFETY_TIMER_EXP_MASK		SMB137B_MASK(1, 7)
#define BATT_TEMP_UNSAFE_MASK		SMB137B_MASK(1, 6)
#define INPUT_OVLO_IVLO_MASK		SMB137B_MASK(1, 5)
#define BATT_OVLO_MASK			SMB137B_MASK(1, 4)
#define INTERNAL_OVER_TEMP_MASK		SMB137B_MASK(1, 2)
#define ENTER_TAPER_CHG_MASK		SMB137B_MASK(1, 1)
#define CHG_MASK			SMB137B_MASK(1, 0)

#define CELL_TEMP_MON_REG	0x08
#define THERMISTOR_CURR_MASK		SMB137B_MASK(2, 6)
#define LOW_TEMP_CHG_INHIBIT_MASK	SMB137B_MASK(3, 3)
#define HIGH_TEMP_CHG_INHIBIT_MASK	SMB137B_MASK(3, 0)

#define	SAFETY_TIMER_THERMAL_SHUTDOWN_REG	0x09
#define DCIN_OVLO_SEL_MASK		SMB137B_MASK(2, 7)
#define RELOAD_EN_INPUT_VOLTAGE_MASK	SMB137B_MASK(1, 6)
#define THERM_SHUTDN_EN_MASK		SMB137B_MASK(1, 5)
#define STANDBY_WD_TIMER_EN_MASK		SMB137B_MASK(1, 4)
#define COMPLETE_CHG_TMOUT_MASK		SMB137B_MASK(2, 2)
#define PRE_CHG_TMOUT_MASK		SMB137B_MASK(2, 0)

#define	VSYS_REG	0x0A
#define	VSYS_MASK			SMB137B_MASK(3, 4)

#define IRQ_RESET_REG	0x30

#define COMMAND_A_REG	0x31
#define	VOLATILE_REGS_WRITE_PERM_BIT	BIT(7)
#define	POR_BIT				BIT(6)
#define	FAST_CHG_SETTINGS_BIT		BIT(5)
#define	BATT_CHG_EN_BIT			BIT(4)
#define	USBIN_MODE_500_BIT		BIT(3)
#define	USBIN_MODE_HCMODE_BIT		BIT(2)
#define	OTG_LBR_EN_BIT			BIT(1)
#define	STAT_OE_BIT			BIT(0)

#define STATUS_A_REG	0x32
#define INTERNAL_TEMP_IRQ_STAT		BIT(4)
#define DCIN_OV_IRQ_STAT		BIT(3)
#define DCIN_UV_IRQ_STAT		BIT(2)
#define USBIN_OV_IRQ_STAT		BIT(1)
#define USBIN_UV_IRQ_STAT		BIT(0)

#define STATUS_B_REG	0x33
#define USB_PIN_STAT			BIT(7)
#define USB51_MODE_STAT			BIT(6)
#define USB51_HC_MODE_STAT		BIT(5)
#define INTERNAL_TEMP_LIMIT_B_STAT	BIT(4)
#define DC_IN_OV_STAT			BIT(3)
#define DC_IN_UV_STAT			BIT(2)
#define USB_IN_OV_STAT			BIT(1)
#define USB_IN_UV_STAT			BIT(0)

#define	STATUS_C_REG	0x34
#define AUTO_IN_CURR_LIMIT_MASK		SMB137B_MASK(4, 4)
#define AUTO_IN_CURR_LIMIT_STAT		BIT(3)
#define AUTO_SOURCE_DET_COMP_STAT_MASK	SMB137B_MASK(2, 1)
#define AUTO_SOURCE_DET_RESULT_STAT	BIT(0)

#define	STATUS_D_REG	0x35
#define	VBATT_LESS_THAN_VSYS_STAT	BIT(7)
#define	USB_FAIL_STAT			BIT(6)
#define	BATT_TEMP_STAT_MASK		SMB137B_MASK(2, 4)
#define	INTERNAL_TEMP_LIMIT_STAT	BIT(2)
#define	OTG_LBR_MODE_EN_STAT		BIT(1)
#define	OTG_LBR_VBATT_UVLO_STAT		BIT(0)

#define	STATUS_E_REG	0x36
#define	CHARGE_CYCLE_COUNT_STAT		BIT(7)
#define	CHARGER_TERM_STAT		BIT(6)
#define	SAFETY_TIMER_STAT_MASK		SMB137B_MASK(2, 4)
#define	CHARGER_ERROR_STAT		BIT(3)
#define	CHARGING_STAT_E			SMB137B_MASK(2, 1)
#define	CHARGING_EN			BIT(0)

#define	STATUS_F_REG	0x37
#define	WD_IRQ_ACTIVE_STAT		BIT(7)
#define	OTG_OVERCURRENT_STAT		BIT(6)
#define	BATT_PRESENT_STAT		BIT(4)
#define	BATT_OV_LATCHED_STAT		BIT(3)
#define	CHARGER_OVLO_STAT		BIT(2)
#define	CHARGER_UVLO_STAT		BIT(1)
#define	BATT_LOW_STAT			BIT(0)

#define	STATUS_G_REG	0x38
#define	CHARGE_TIMEOUT_IRQ_STAT		BIT(7)
#define	PRECHARGE_TIMEOUT_IRQ_STAT	BIT(6)
#define	BATT_HOT_IRQ_STAT		BIT(5)
#define	BATT_COLD_IRQ_STAT		BIT(4)
#define	BATT_OV_IRQ_STAT		BIT(3)
#define	TAPER_CHG_IRQ_STAT		BIT(2)
#define	FAST_CHG_IRQ_STAT		BIT(1)
#define	CHARGING_IRQ_STAT		BIT(0)

#define	STATUS_H_REG	0x39
#define	CHARGE_TIMEOUT_STAT		BIT(7)
#define	PRECHARGE_TIMEOUT_STAT		BIT(6)
#define	BATT_HOT_STAT			BIT(5)
#define	BATT_COLD_STAT			BIT(4)
#define	BATT_OV_STAT			BIT(3)
#define	TAPER_CHG_STAT			BIT(2)
#define	FAST_CHG_STAT			BIT(1)
#define	CHARGING_STAT_H			BIT(0)

#define DEV_ID_REG	0x3B

#define COMMAND_B_REG	0x3C
#define	THERM_NTC_CURR_VERRIDE		BIT(7)

#define SMB137B_CHG_PERIOD	((HZ) * 150)

#define INPUT_CURRENT_REG_DEFAULT	0xE1
#define INPUT_CURRENT_REG_MIN		0x01
#define	COMMAND_A_REG_DEFAULT		0xA0
#define	COMMAND_A_REG_OTG_MODE		0xA2

#define	PIN_CTRL_REG_DEFAULT		0x00
#define	PIN_CTRL_REG_CHG_OFF		0x04

#define	FAST_CHG_E_STATUS 0x2

#define SMB137B_DEFAULT_BATT_RATING   950
struct smb137b_data {
	struct i2c_client *client;
	struct delayed_work charge_work;

	bool charging;
	int chgcurrent;
	int cur_charging_mode;
	int max_system_voltage;
	int min_system_voltage;

	int valid_n_gpio;

	int batt_status;
	int batt_chg_type;
	int batt_present;
	int min_design;
	int max_design;
	int batt_mah_rating;

	int usb_status;

	u8 dev_id_reg;
	struct msm_hardware_charger adapter_hw_chg;
};

static unsigned int disabled;
static DEFINE_MUTEX(init_lock);
static unsigned int init_otg_power;

enum charger_stat {
	SMB137B_ABSENT,
	SMB137B_PRESENT,
	SMB137B_ENUMERATED,
};

static struct smb137b_data *usb_smb137b_chg;

static int smb137b_read_reg(struct i2c_client *client, int reg,
	u8 *val)
{
	s32 ret;
	struct smb137b_data *smb137b_chg;

	smb137b_chg = i2c_get_clientdata(client);
	ret = i2c_smbus_read_byte_data(smb137b_chg->client, reg);
	if (ret < 0) {
		dev_err(&smb137b_chg->client->dev,
			"i2c read fail: can't read from %02x: %d\n", reg, ret);
		return ret;
	} else
		*val = ret;

	return 0;
}

static int smb137b_write_reg(struct i2c_client *client, int reg,
	u8 val)
{
	s32 ret;
	struct smb137b_data *smb137b_chg;

	smb137b_chg = i2c_get_clientdata(client);
	ret = i2c_smbus_write_byte_data(smb137b_chg->client, reg, val);
	if (ret < 0) {
		dev_err(&smb137b_chg->client->dev,
			"i2c write fail: can't write %02x to %02x: %d\n",
			val, reg, ret);
		return ret;
	}
	return 0;
}

static ssize_t id_reg_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct smb137b_data *smb137b_chg;

	smb137b_chg = i2c_get_clientdata(to_i2c_client(dev));

	return sprintf(buf, "%02x\n", smb137b_chg->dev_id_reg);
}
static DEVICE_ATTR(id_reg, S_IRUGO | S_IWUSR, id_reg_show, NULL);

#ifdef DEBUG
static void smb137b_dbg_print_status_regs(struct smb137b_data *smb137b_chg)
{
	int ret;
	u8 temp;

	ret = smb137b_read_reg(smb137b_chg->client, STATUS_A_REG, &temp);
	dev_dbg(&smb137b_chg->client->dev, "%s A=0x%x\n", __func__, temp);
	ret = smb137b_read_reg(smb137b_chg->client, STATUS_B_REG, &temp);
	dev_dbg(&smb137b_chg->client->dev, "%s B=0x%x\n", __func__, temp);
	ret = smb137b_read_reg(smb137b_chg->client, STATUS_C_REG, &temp);
	dev_dbg(&smb137b_chg->client->dev, "%s C=0x%x\n", __func__, temp);
	ret = smb137b_read_reg(smb137b_chg->client, STATUS_D_REG, &temp);
	dev_dbg(&smb137b_chg->client->dev, "%s D=0x%x\n", __func__, temp);
	ret = smb137b_read_reg(smb137b_chg->client, STATUS_E_REG, &temp);
	dev_dbg(&smb137b_chg->client->dev, "%s E=0x%x\n", __func__, temp);
	ret = smb137b_read_reg(smb137b_chg->client, STATUS_F_REG, &temp);
	dev_dbg(&smb137b_chg->client->dev, "%s F=0x%x\n", __func__, temp);
	ret = smb137b_read_reg(smb137b_chg->client, STATUS_G_REG, &temp);
	dev_dbg(&smb137b_chg->client->dev, "%s G=0x%x\n", __func__, temp);
	ret = smb137b_read_reg(smb137b_chg->client, STATUS_H_REG, &temp);
	dev_dbg(&smb137b_chg->client->dev, "%s H=0x%x\n", __func__, temp);
}
#else
static void smb137b_dbg_print_status_regs(struct smb137b_data *smb137b_chg)
{
}
#endif

static int smb137b_start_charging(struct msm_hardware_charger *hw_chg,
		int chg_voltage, int chg_current)
{
	int cmd_val = COMMAND_A_REG_DEFAULT;
	u8 temp = 0;
	int ret = 0;
	struct smb137b_data *smb137b_chg;

	smb137b_chg = container_of(hw_chg, struct smb137b_data, adapter_hw_chg);
	if (disabled) {
		dev_err(&smb137b_chg->client->dev,
			"%s called when disabled\n", __func__);
		goto out;
	}

	if (smb137b_chg->charging == true
		&& smb137b_chg->chgcurrent == chg_current)
		/* we are already charging with the same current*/
		 dev_err(&smb137b_chg->client->dev,
			 "%s charge with same current %d called again\n",
			  __func__, chg_current);

	dev_dbg(&smb137b_chg->client->dev, "%s\n", __func__);
	if (chg_current < 500)
		cmd_val &= ~USBIN_MODE_500_BIT;
	else if (chg_current == 500)
		cmd_val |= USBIN_MODE_500_BIT;
	else
		cmd_val |= USBIN_MODE_HCMODE_BIT;

	smb137b_chg->chgcurrent = chg_current;
	smb137b_chg->cur_charging_mode = cmd_val;

	/* Due to non-volatile reload feature,always enable volatile
	 * mirror writes before modifying any 00h~09h control register.
	 * Current mode needs to be programmed according to what's detected
	 * Otherwise default 100mA mode might cause VOUTL drop and fail
	 * the system in case of dead battery.
	 */
	ret = smb137b_write_reg(smb137b_chg->client,
					COMMAND_A_REG, cmd_val);
	if (ret) {
		dev_err(&smb137b_chg->client->dev,
			"%s couldn't write to command_reg\n", __func__);
		goto out;
	}
	ret = smb137b_write_reg(smb137b_chg->client,
					PIN_CTRL_REG, PIN_CTRL_REG_DEFAULT);
	if (ret) {
		dev_err(&smb137b_chg->client->dev,
			"%s couldn't write to pin ctrl reg\n", __func__);
		goto out;
	}
	smb137b_chg->charging = true;
	smb137b_chg->batt_status = POWER_SUPPLY_STATUS_CHARGING;
	smb137b_chg->batt_chg_type = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;

	ret = smb137b_read_reg(smb137b_chg->client, STATUS_E_REG, &temp);
	if (ret) {
		dev_err(&smb137b_chg->client->dev,
			"%s couldn't read status e reg %d\n", __func__, ret);
	} else {
		if (temp & CHARGER_ERROR_STAT) {
			dev_err(&smb137b_chg->client->dev,
				"%s chg error E=0x%x\n", __func__, temp);
			smb137b_dbg_print_status_regs(smb137b_chg);
		}
		if (((temp & CHARGING_STAT_E) >> 1) >= FAST_CHG_E_STATUS)
			smb137b_chg->batt_chg_type
						= POWER_SUPPLY_CHARGE_TYPE_FAST;
	}
	/*schedule charge_work to keep track of battery charging state*/
	schedule_delayed_work(&smb137b_chg->charge_work, SMB137B_CHG_PERIOD);

out:
	return ret;
}

static int smb137b_stop_charging(struct msm_hardware_charger *hw_chg)
{
	int ret = 0;
	struct smb137b_data *smb137b_chg;

	smb137b_chg = container_of(hw_chg, struct smb137b_data, adapter_hw_chg);

	dev_dbg(&smb137b_chg->client->dev, "%s\n", __func__);
	if (smb137b_chg->charging == false)
		return 0;

	smb137b_chg->charging = false;
	smb137b_chg->batt_status = POWER_SUPPLY_STATUS_DISCHARGING;
	smb137b_chg->batt_chg_type = POWER_SUPPLY_CHARGE_TYPE_NONE;

	ret = smb137b_write_reg(smb137b_chg->client, COMMAND_A_REG,
				smb137b_chg->cur_charging_mode);
	if (ret) {
		dev_err(&smb137b_chg->client->dev,
			"%s couldn't write to command_reg\n", __func__);
		goto out;
	}

	ret = smb137b_write_reg(smb137b_chg->client,
					PIN_CTRL_REG, PIN_CTRL_REG_CHG_OFF);
	if (ret)
		dev_err(&smb137b_chg->client->dev,
			"%s couldn't write to pin ctrl reg\n", __func__);

out:
	return ret;
}

static int smb137b_charger_switch(struct msm_hardware_charger *hw_chg)
{
	struct smb137b_data *smb137b_chg;

	smb137b_chg = container_of(hw_chg, struct smb137b_data, adapter_hw_chg);
	dev_dbg(&smb137b_chg->client->dev, "%s\n", __func__);
	return 0;
}

static irqreturn_t smb137b_valid_handler(int irq, void *dev_id)
{
	int val;
	struct smb137b_data *smb137b_chg;
	struct i2c_client *client = dev_id;

	smb137b_chg = i2c_get_clientdata(client);

	pr_debug("%s Cable Detected USB inserted\n", __func__);
	/*extra delay needed to allow CABLE_DET_N settling down and debounce
	 before	trying to sample its correct value*/
	usleep_range(1000, 1200);
	val = gpio_get_value_cansleep(smb137b_chg->valid_n_gpio);
	if (val < 0) {
		dev_err(&smb137b_chg->client->dev,
			"%s gpio_get_value failed for %d ret=%d\n", __func__,
			smb137b_chg->valid_n_gpio, val);
		goto err;
	}
	dev_dbg(&smb137b_chg->client->dev, "%s val=%d\n", __func__, val);

	if (val) {
		if (smb137b_chg->usb_status != SMB137B_ABSENT) {
			smb137b_chg->usb_status = SMB137B_ABSENT;
			msm_charger_notify_event(&smb137b_chg->adapter_hw_chg,
					CHG_REMOVED_EVENT);
		}
	} else {
		if (smb137b_chg->usb_status == SMB137B_ABSENT) {
			smb137b_chg->usb_status = SMB137B_PRESENT;
			msm_charger_notify_event(&smb137b_chg->adapter_hw_chg,
					CHG_INSERTED_EVENT);
		}
	}
err:
	return IRQ_HANDLED;
}

#ifdef CONFIG_DEBUG_FS
static struct dentry *dent;
static int debug_fs_otg;
static int otg_get(void *data, u64 *value)
{
	*value = debug_fs_otg;
	return 0;
}
static int otg_set(void *data, u64 value)
{
	smb137b_otg_power(debug_fs_otg);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(smb137b_otg_fops, otg_get, otg_set, "%llu\n");

static void smb137b_create_debugfs_entries(struct smb137b_data *smb137b_chg)
{
	dent = debugfs_create_dir("smb137b", NULL);
	if (dent) {
		debugfs_create_file("otg", 0644, dent, NULL, &smb137b_otg_fops);
	}
}
static void smb137b_destroy_debugfs_entries(void)
{
	if (dent)
		debugfs_remove_recursive(dent);
}
#else
static void smb137b_create_debugfs_entries(struct smb137b_data *smb137b_chg)
{
}
static void smb137b_destroy_debugfs_entries(void)
{
}
#endif

static int set_disable_status_param(const char *val, struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);
	if (ret)
		return ret;

	if (usb_smb137b_chg && disabled)
		msm_charger_notify_event(&usb_smb137b_chg->adapter_hw_chg,
				CHG_DONE_EVENT);

	pr_debug("%s disabled =%d\n", __func__, disabled);
	return 0;
}
module_param_call(disabled, set_disable_status_param, param_get_uint,
					&disabled, 0644);
static void smb137b_charge_sm(struct work_struct *smb137b_work)
{
	int ret;
	struct smb137b_data *smb137b_chg;
	u8 temp = 0;
	int notify_batt_changed = 0;

	smb137b_chg = container_of(smb137b_work, struct smb137b_data,
			charge_work.work);

	/*if not charging, exit smb137b charging state transition*/
	if (!smb137b_chg->charging)
		return;

	dev_dbg(&smb137b_chg->client->dev, "%s\n", __func__);

	ret = smb137b_read_reg(smb137b_chg->client, STATUS_F_REG, &temp);
	if (ret) {
		dev_err(&smb137b_chg->client->dev,
			"%s couldn't read status f reg %d\n", __func__, ret);
		goto out;
	}
	if (smb137b_chg->batt_present != !(temp & BATT_PRESENT_STAT)) {
		smb137b_chg->batt_present = !(temp & BATT_PRESENT_STAT);
		notify_batt_changed = 1;
	}

	if (!smb137b_chg->batt_present)
		smb137b_chg->batt_chg_type = POWER_SUPPLY_CHARGE_TYPE_NONE;

	if (!smb137b_chg->batt_present && smb137b_chg->charging)
		msm_charger_notify_event(&smb137b_chg->adapter_hw_chg,
				CHG_DONE_EVENT);

	if (smb137b_chg->batt_present
		&& smb137b_chg->charging
		&& smb137b_chg->batt_chg_type
			!= POWER_SUPPLY_CHARGE_TYPE_FAST) {
		ret = smb137b_read_reg(smb137b_chg->client,
						STATUS_E_REG, &temp);
		if (ret) {
			dev_err(&smb137b_chg->client->dev,
				"%s couldn't read cntrl reg\n", __func__);
			goto out;

		} else {
			if (temp & CHARGER_ERROR_STAT) {
				dev_err(&smb137b_chg->client->dev,
					"%s error E=0x%x\n", __func__, temp);
				smb137b_dbg_print_status_regs(smb137b_chg);
			}
			if (((temp & CHARGING_STAT_E) >> 1)
					>= FAST_CHG_E_STATUS) {
				smb137b_chg->batt_chg_type
					= POWER_SUPPLY_CHARGE_TYPE_FAST;
				notify_batt_changed = 1;
				msm_charger_notify_event(
					&smb137b_chg->adapter_hw_chg,
					CHG_BATT_BEGIN_FAST_CHARGING);
			} else {
				smb137b_chg->batt_chg_type
					= POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			}
		}
	}

out:
	schedule_delayed_work(&smb137b_chg->charge_work,
					SMB137B_CHG_PERIOD);
}

static void __smb137b_otg_power(int on)
{
	int ret;

	if (on) {
		ret = smb137b_write_reg(usb_smb137b_chg->client,
					PIN_CTRL_REG, PIN_CTRL_REG_CHG_OFF);
		if (ret) {
			pr_err("%s turning off charging in pin_ctrl err=%d\n",
								__func__, ret);
			/*
			 * don't change the command register if charging in
			 * pin control cannot be turned off
			 */
			return;
		}

		ret = smb137b_write_reg(usb_smb137b_chg->client,
			COMMAND_A_REG, COMMAND_A_REG_OTG_MODE);
		if (ret)
			pr_err("%s failed turning on OTG mode ret=%d\n",
								__func__, ret);
	} else {
		ret = smb137b_write_reg(usb_smb137b_chg->client,
			COMMAND_A_REG, COMMAND_A_REG_DEFAULT);
		if (ret)
			pr_err("%s failed turning off OTG mode ret=%d\n",
								__func__, ret);
		ret = smb137b_write_reg(usb_smb137b_chg->client,
				PIN_CTRL_REG, PIN_CTRL_REG_DEFAULT);
		if (ret)
			pr_err("%s failed writing to pn_ctrl ret=%d\n",
								__func__, ret);
	}
}
static int __devinit smb137b_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	const struct smb137b_platform_data *pdata;
	struct smb137b_data *smb137b_chg;
	int ret = 0;

	pdata = client->dev.platform_data;

	if (pdata == NULL) {
		dev_err(&client->dev, "%s no platform data\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA)) {
		ret = -EIO;
		goto out;
	}

	smb137b_chg = kzalloc(sizeof(*smb137b_chg), GFP_KERNEL);
	if (!smb137b_chg) {
		ret = -ENOMEM;
		goto out;
	}

	INIT_DELAYED_WORK(&smb137b_chg->charge_work, smb137b_charge_sm);
	smb137b_chg->client = client;
	smb137b_chg->valid_n_gpio = pdata->valid_n_gpio;
	smb137b_chg->batt_mah_rating = pdata->batt_mah_rating;
	if (smb137b_chg->batt_mah_rating == 0)
		smb137b_chg->batt_mah_rating = SMB137B_DEFAULT_BATT_RATING;

	/*It supports USB-WALL charger and PC USB charger */
	smb137b_chg->adapter_hw_chg.type = CHG_TYPE_USB;
	smb137b_chg->adapter_hw_chg.rating = pdata->batt_mah_rating;
	smb137b_chg->adapter_hw_chg.name = "smb137b-charger";
	smb137b_chg->adapter_hw_chg.start_charging = smb137b_start_charging;
	smb137b_chg->adapter_hw_chg.stop_charging = smb137b_stop_charging;
	smb137b_chg->adapter_hw_chg.charging_switched = smb137b_charger_switch;

	if (pdata->chg_detection_config)
		ret = pdata->chg_detection_config();
	if (ret) {
		dev_err(&client->dev, "%s valid config failed ret=%d\n",
			__func__, ret);
		goto free_smb137b_chg;
	}

	ret = gpio_request(pdata->valid_n_gpio, "smb137b_charger_valid");
	if (ret) {
		dev_err(&client->dev, "%s gpio_request failed for %d ret=%d\n",
			__func__, pdata->valid_n_gpio, ret);
		goto free_smb137b_chg;
	}

	i2c_set_clientdata(client, smb137b_chg);

	ret = msm_charger_register(&smb137b_chg->adapter_hw_chg);
	if (ret) {
		dev_err(&client->dev, "%s msm_charger_register\
			failed for ret=%d\n", __func__, ret);
		goto free_valid_gpio;
	}

	ret = irq_set_irq_wake(client->irq, 1);
	if (ret) {
		dev_err(&client->dev, "%s failed for irq_set_irq_wake %d ret =%d\n",
			 __func__, client->irq, ret);
		goto unregister_charger;
	}

	ret = request_threaded_irq(client->irq, NULL,
				   smb137b_valid_handler,
				   IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				   "smb137b_charger_valid", client);
	if (ret) {
		dev_err(&client->dev,
			"%s request_threaded_irq failed for %d ret =%d\n",
			__func__, client->irq, ret);
		goto disable_irq_wake;
	}

	ret = gpio_get_value_cansleep(smb137b_chg->valid_n_gpio);
	if (ret < 0) {
		dev_err(&client->dev,
			"%s gpio_get_value failed for %d ret=%d\n", __func__,
			pdata->valid_n_gpio, ret);
		/* assume absent */
		ret = 1;
	}
	if (!ret) {
		msm_charger_notify_event(&smb137b_chg->adapter_hw_chg,
			CHG_INSERTED_EVENT);
		smb137b_chg->usb_status = SMB137B_PRESENT;
	}

	ret = smb137b_read_reg(smb137b_chg->client, DEV_ID_REG,
			&smb137b_chg->dev_id_reg);

	ret = device_create_file(&smb137b_chg->client->dev, &dev_attr_id_reg);

	/* TODO read min_design and max_design from chip registers */
	smb137b_chg->min_design = 3200;
	smb137b_chg->max_design = 4200;

	smb137b_chg->batt_status = POWER_SUPPLY_STATUS_DISCHARGING;
	smb137b_chg->batt_chg_type = POWER_SUPPLY_CHARGE_TYPE_NONE;

	device_init_wakeup(&client->dev, 1);

	mutex_lock(&init_lock);
	usb_smb137b_chg = smb137b_chg;
	if (init_otg_power)
		__smb137b_otg_power(init_otg_power);
	mutex_unlock(&init_lock);

	smb137b_create_debugfs_entries(smb137b_chg);
	dev_dbg(&client->dev,
		"%s OK device_id = %x chg_state=%d\n", __func__,
		smb137b_chg->dev_id_reg, smb137b_chg->usb_status);
	return 0;

disable_irq_wake:
	irq_set_irq_wake(client->irq, 0);
unregister_charger:
	msm_charger_unregister(&smb137b_chg->adapter_hw_chg);
free_valid_gpio:
	gpio_free(pdata->valid_n_gpio);
free_smb137b_chg:
	kfree(smb137b_chg);
out:
	return ret;
}

void smb137b_otg_power(int on)
{
	pr_debug("%s Enter on=%d\n", __func__, on);

	mutex_lock(&init_lock);
	if (!usb_smb137b_chg) {
		init_otg_power = !!on;
		pr_warning("%s called when not initialized\n", __func__);
		mutex_unlock(&init_lock);
		return;
	}
	__smb137b_otg_power(on);
	mutex_unlock(&init_lock);
}

static int __devexit smb137b_remove(struct i2c_client *client)
{
	const struct smb137b_platform_data *pdata;
	struct smb137b_data *smb137b_chg = i2c_get_clientdata(client);

	pdata = client->dev.platform_data;
	device_init_wakeup(&client->dev, 0);
	irq_set_irq_wake(client->irq, 0);
	free_irq(client->irq, client);
	gpio_free(pdata->valid_n_gpio);
	cancel_delayed_work_sync(&smb137b_chg->charge_work);

	msm_charger_notify_event(&smb137b_chg->adapter_hw_chg,
			 CHG_REMOVED_EVENT);
	msm_charger_unregister(&smb137b_chg->adapter_hw_chg);
	smb137b_destroy_debugfs_entries();
	kfree(smb137b_chg);
	return 0;
}

#ifdef CONFIG_PM
static int smb137b_suspend(struct device *dev)
{
	struct smb137b_data *smb137b_chg = dev_get_drvdata(dev);

	dev_dbg(&smb137b_chg->client->dev, "%s\n", __func__);
	if (smb137b_chg->charging)
		return -EBUSY;
	return 0;
}

static int smb137b_resume(struct device *dev)
{
	struct smb137b_data *smb137b_chg = dev_get_drvdata(dev);

	dev_dbg(&smb137b_chg->client->dev, "%s\n", __func__);
	return 0;
}

static const struct dev_pm_ops smb137b_pm_ops = {
	.suspend = smb137b_suspend,
	.resume = smb137b_resume,
};
#endif

static const struct i2c_device_id smb137b_id[] = {
	{"smb137b", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, smb137b_id);

static struct i2c_driver smb137b_driver = {
	.driver = {
		   .name = "smb137b",
		   .owner = THIS_MODULE,
#ifdef CONFIG_PM
		   .pm = &smb137b_pm_ops,
#endif
	},
	.probe = smb137b_probe,
	.remove = __devexit_p(smb137b_remove),
	.id_table = smb137b_id,
};

static int __init smb137b_init(void)
{
	return i2c_add_driver(&smb137b_driver);
}
module_init(smb137b_init);

static void __exit smb137b_exit(void)
{
	return i2c_del_driver(&smb137b_driver);
}
module_exit(smb137b_exit);

MODULE_AUTHOR("Abhijeet Dharmapurikar <adharmap@codeaurora.org>");
MODULE_DESCRIPTION("Driver for SMB137B Charger chip");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:smb137b");
