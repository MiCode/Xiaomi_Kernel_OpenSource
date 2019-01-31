/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "SMB1351 %s: " fmt, __func__
#include "mt_battery_common.h"
#include "mt_charging.h"
#include "smb1351.h"
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <mt-plat/mt_boot.h>
#include <mt-plat/mt_reboot.h>
#include <mt-plat/upmu_common.h>
/*******************************************
 *    [Define]
 *******************************************
 */
#define smb1351_access_err(rc, write, reg)         \
	do {                                           \
		if (rc) {                                  \
			pr_debug("%s reg: %02xh failed\n",     \
				 (write) ? "write" : "read", reg); \
		}                                          \
	} while (0)

#define STATUS_OK 0
#define STATUS_UNSUPPORTED -1
#define smb1351_read_access 0
#define smb1351_write_access 1
#define GETARRAYNUM(array) (ARRAY_SIZE(array))

/******************************************
 *
 *   [Global Variable]
 *
 ******************************************
 */

static u8 g_reg_value_smb1351;
static bool charger_status;

/* for MT6391 */
static const u32 VCDT_HV_VTH[] = {
	BATTERY_VOLT_04_000000_V, BATTERY_VOLT_04_100000_V,
	BATTERY_VOLT_04_150000_V, BATTERY_VOLT_04_200000_V,
	BATTERY_VOLT_04_250000_V, BATTERY_VOLT_04_300000_V,
	BATTERY_VOLT_04_350000_V, BATTERY_VOLT_04_400000_V,
	BATTERY_VOLT_04_450000_V, BATTERY_VOLT_04_500000_V,
	BATTERY_VOLT_04_550000_V, BATTERY_VOLT_04_600000_V,
	BATTERY_VOLT_07_000000_V, BATTERY_VOLT_07_500000_V,
	BATTERY_VOLT_08_500000_V, BATTERY_VOLT_10_500000_V};

/* fast_chg_current[n] / 100 = mA */
static int fast_chg_current[] = {
100000, 120000, 140000, 160000, 180000, 200000,
220000, 240000, 260000, 280000, 300000, 340000,
360000, 380000, 400000, 450000};

/* input_current[n] / 100 = mA */
static int input_current[] = {
50000,  68500,  100000, 110000, 120000,
130000, 150000, 160000, 170000, 180000,
200000, 220000, 250000, 300000};

enum temp_state {
	unknown_temp_state = 0,
	less_than_15,
	from_15_to_100,
	from_100_to_500,
	from_500_to_600,
	greater_than_600,
};

static int of_get_smb1351_platform_data(struct device *dev);

struct smb1351_charger {
	struct i2c_client *client;
	struct device *dev;
	struct mutex read_write_lock;
	struct wake_lock jeita_setting_wake_lock;
	int last_charger_type;
	int last_temp_state;
	int otg_en_gpio;
};
struct smb1351_charger *chip;

/********************************************************
 *
 *   [I2C Function For Read/Write smb1351]
 *
 ********************************************************
 */
static int __smb1351_read_reg(u8 reg, u8 *val)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(chip->client, reg);
	if (ret < 0) {
		pr_debug("i2c read fail: can't read from %02x: %d\n", reg, ret);
		return ret;
	}

	*val = ret;

	return 0;
}

static int __smb1351_write_reg(int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(chip->client, reg, val);
	if (ret < 0) {
		pr_debug("i2c write fail: can't write %02x to %02x: %d\n", val,
			 reg, ret);
		return ret;
	}
	return 0;
}

static int smb1351_read_reg(int reg, u8 *val)
{
	int rc;
	int i;

	for (i = 0; i < I2C_TRANSFER_RETRY; i++) {
		mutex_lock(&chip->read_write_lock);
		rc = __smb1351_read_reg(reg, val);
		mutex_unlock(&chip->read_write_lock);
		if (rc) {
			pr_debug("Reading %02x failed.....retry : %d\n", reg,
				 i);
			msleep(100);
		} else
			break;
	}
	return rc;
}

static int smb1351_masked_write(int reg, u8 mask, u8 val)
{
	s32 rc;
	u8 temp;
	int i;

	rc = smb1351_read_reg(reg, &temp);
	if (rc) {
		pr_debug("Read Failed: reg=%02x, rc=%d\n", reg, rc);
		goto out;
	}
	temp &= ~mask;
	temp |= val & mask;
	for (i = 0; i < I2C_TRANSFER_RETRY; i++) {
		mutex_lock(&chip->read_write_lock);
		rc = __smb1351_write_reg(reg, temp);
		mutex_unlock(&chip->read_write_lock);
		if (rc) {
			pr_debug("Writing %02x failed.....retry : %d\n", reg,
				 i);
			msleep(100);
		} else
			break;
	}
	if (rc)
		pr_debug("Write Failed: reg=%02x, rc=%d\n", reg, rc);
out:
	return rc;
}

static int smb1351_enable_volatile_access(void)
{
	int rc;
	/* BQ configuration volatile access, 30h[6] = 1 */
	rc = smb1351_masked_write(0x30, 0x40, 0x40);
	smb1351_access_err(rc, smb1351_write_access, 0x30);
	return 0;
}

static void smb1351_dump_register(void)
{
	int i = 0;
	char tmp_buf[64], *buf;
	u8 reg;

	buf = kmalloc(sizeof(tmp_buf) * sizeof(char) * 30, GFP_KERNEL);

	smb1351_enable_volatile_access();
	sprintf(tmp_buf, "smb1351 Configuration Registers Detail\n"
			 "==================\n");
	strcpy(buf, tmp_buf);

	for (i = 0; i <= 20; i++) {
		smb1351_read_reg(0x0 + i, &reg);
		sprintf(tmp_buf, "Reg%02xh:\t0x%02x\n", 0x0 + i, reg);
		strcat(buf, tmp_buf);
	}
	pr_debug("%s", buf);
	sprintf(tmp_buf, "smb1351 Configuration Registers Detail\n"
			 "==================\n");
	strcpy(buf, tmp_buf);
	for (i = 0; i <= 26; i++) {
		smb1351_read_reg(0x15 + i, &reg);
		sprintf(tmp_buf, "Reg%02xh:\t0x%02x\n", 0x15 + i, reg);
		strcat(buf, tmp_buf);
	}
	pr_debug("%s", buf);
	sprintf(tmp_buf, "smb1351 Configuration Registers Detail\n"
			 "==================\n");
	strcpy(buf, tmp_buf);
	for (i = 0; i <= 24; i++) {
		smb1351_read_reg(0x30 + i, &reg);
		sprintf(tmp_buf, "Reg%02xh:\t0x%02x\n", 0x30 + i, reg);
		strcat(buf, tmp_buf);
	}
	pr_debug("%s", buf);
	kfree(buf);
}

static void smb1351_enable_AICL(bool enable)
{
	int rc;

	smb1351_enable_volatile_access();
	if (enable) {
		/* 02h[4] = "1" */
		rc = smb1351_masked_write(VARIOUS_FUNC, AUTO_AICL_LIMIT_MASK,
					  0x10);
		if (rc)
			pr_debug("failed to enable AICL\n");
	} else {
		/* 02h[4] = "0" */
		rc = smb1351_masked_write(VARIOUS_FUNC, AUTO_AICL_LIMIT_MASK,
					  0x0);
		if (rc)
			pr_debug("failed to disable AICL\n");
	}
}

static void smb1351_enable_charging(bool enable)
{
	int rc;

	smb1351_enable_volatile_access();
	if (enable) {

		/* clear suspend mode */
		rc = smb1351_masked_write(CMD_REG_IL, SUSPEND_MODE_MASK, 0x0);
		smb1351_access_err(rc, smb1351_write_access, CMD_REG_IL);

		/* Charging Enable, 06h[6:5] = "11" */
		rc = smb1351_masked_write(0x6, 0x60, 0x60);
		smb1351_access_err(rc, smb1351_write_access, 0x6);
	} else {
		/* Charging Disable, 06h[6:5] = "10" */
		rc = smb1351_masked_write(0x6, 0x60, 0x40);
		smb1351_access_err(rc, smb1351_write_access, 0x6);
	}
}

static void smb1351_initial_setting(void)
{
	int rc;

	/* BQ configuration volatile access, 30h[6] = 1 */
	smb1351_enable_volatile_access();

	/* change freq to 1M Hz, 0Ah[7:6] = "01" */
	rc = smb1351_masked_write(OTG_TLIM_CTRL_REG, SWITCHING_FREQ_MASK, 0x40);
	if (rc)
		pr_debug("failed to change charging freq. to 1M Hz\n");

	/* Set pre-charge current = 200mA, 01h[7:5] = "000" */
	rc = smb1351_masked_write(OTHER_CURRENT_REG, PRECHARGE_MASK, 0x0);
	if (rc)
		pr_debug("failed to set pre-charge current to 200 mA\n");

	/* Set Termination current = 120mA, 01h[4:2] = "110" */
	rc = smb1351_masked_write(OTHER_CURRENT_REG, TERMINATION_MASK, 0x18);
	if (rc)
		pr_debug("failed to set termination current to 120 mA\n");

	/* Enable Watchdog Timer & Set 72sec, 08h[6:5] = "10", 08h[0] = "1" */
	rc = smb1351_masked_write(WDT_TIMER_CTRL_REG, WATCHDOG_TIMER_MASK,
			0x40);
	if (rc)
		pr_debug("failed to set watchdog timer to 72 secs\n");
	rc = smb1351_masked_write(WDT_TIMER_CTRL_REG, WATCHDOG_EN_MASK, 0x1);
	if (rc)
		pr_debug("failed to enable watchdog timer\n");

	/* Disable Charging indicator in STAT, 05[5] = "1" */
	rc = smb1351_masked_write(STAT_CTRL_REG, STAT_OUTPUT_CTRL_MASK, 0x20);
	if (rc)
		pr_debug("failed to disable charger indicator in STAT\n");

	/* Input Voltage Range = 5~12V, 10h[6:4] = "100", 14h[7] = "1" */
	rc = smb1351_masked_write(FLEXCHARGER_REG, CHARGER_CONFIG_MASK, 0x40);
	if (rc)
		pr_debug("failed to enable input voltage range = 5 ~ 12V\n");

	rc = smb1351_masked_write(OTG_POWER_REG, ADAPTER_CONFIG_MASK, 0x80);
	if (rc)
		pr_debug("failed to enable input voltage range = 5 ~ 12V\n");

	/* Set Adapter identification mode = normal mode, 14h[1:0] = "00" */
	rc = smb1351_masked_write(OTG_POWER_REG, ADAPTER_ID_MODE_MASK, 0x0);
	if (rc)
		pr_debug(
			"failed to set adaptor identification mode = normal mode\n");

	/* Set IRQ (Fast, term, taper, recharge, or inhibit), 0Dh[4] = "1" */
	rc = smb1351_masked_write(STATUS_INT_REG, CHARGE_TYPE_MASK, 0x10);
	if (rc)
		pr_debug("failed to set IRQ\n");

	/* Set AICL fail forces suspend mode, 08h[7] = "0" */
	rc = smb1351_masked_write(0x8, 0x80, 0x0);
	smb1351_access_err(rc, smb1351_write_access, 0x8);

	/* Set soft cold current compensation = 1000mA, 0Eh[5] = "1" */
	rc = smb1351_masked_write(0xE, 0x20, 0x20);
	smb1351_access_err(rc, smb1351_write_access, 0xE);
	/* Disable HW JEITA, 07h[4] = "1" */
	rc = smb1351_masked_write(0x7, 0x10, 0x10);
	smb1351_access_err(rc, smb1351_write_access, 0x7);
}

/********************************************************
 *
 *   [Internal Function]
 *
 ********************************************************
 */

static u32 charging_parameter_to_value(const u32 *parameter,
				       const u32 array_size, const u32 val)
{
	u32 i;

	for (i = 0; i < array_size; i++) {
		if (val == *(parameter + i))
			return i;
	}

	/* not find the value */
	pr_debug("NO register value match. val=%d\r\n", val);
	/* TODO: ASSERT(0); */
	return 0;
}

static int get_closest_fast_chg_current(int target_current)
{
	int i;

	for (i = ARRAY_SIZE(fast_chg_current) - 1; i >= 0; i--) {
		if (fast_chg_current[i] <= target_current)
			break;
	}

	if (i < 0) {
		pr_debug("fast_chg current setting %d mA not supported. use 1000mA instead.\n",
			target_current / 100);
		i = 0;
	}
	return i;
}

static int get_closest_input_current(int target_current)
{
	int i;

	for (i = ARRAY_SIZE(input_current) - 1; i >= 0; i--) {
		if (input_current[i] <= target_current)
			break;
	}

	if (i < 0) {
		pr_debug("input current setting %dmA not supported. Use 500mA instead.\n",
			target_current / 100);
		i = 0;
	}
	return i;
}

static u32 bmt_find_closest_level(const u32 *pList, u32 number, u32 level)
{
	u32 i;
	u32 max_value_in_last_element;

	if (pList[0] < pList[1])
		max_value_in_last_element = true;
	else
		max_value_in_last_element = false;

	if (max_value_in_last_element == true) {
		for (i = (number - 1); i != 0;
		     i--) { /* max value in the last element */
			if (pList[i] <= level)
				return pList[i];
		}

		pr_debug(
			    "Can't find closest level, small value first \r\n");
		return pList[0];
	}

	for (i = 0; i < number; i++) { /* max value in the first element */
		if (pList[i] <= level)
			return pList[i];
	}

	pr_debug(
		    "Can't find closest level, large value first \r\n");
	return pList[number - 1];
}

static u32 charging_hw_init(void *data)
{
	u32 status = STATUS_OK;

	upmu_set_rg_bc11_bb_ctrl(1); /* BC11_BB_CTRL */
	upmu_set_rg_bc11_rst(1);     /* BC11_RST */

	smb1351_initial_setting();

	return status;
}

static u32 charging_dump_register(void *data)
{
	u32 status = STATUS_OK;

	smb1351_dump_register();

	return status;
}

static u32 charging_enable(void *data)
{
	u32 status = STATUS_OK;
	u32 enable = *(u32 *)(data);

	smb1351_enable_charging((!enable) ? false : true);

	return status;
}

static u32 charging_set_cv_voltage(void *data)
{
	u32 status = STATUS_OK;
	u32 reg_value = *(u32 *)(data);
	int rc;
	static u32 previous_cv;

	reg_value = ((reg_value / 1000) - 3500) / 20;

	smb1351_enable_volatile_access();

	if (previous_cv != *(u32 *)(data))
		smb1351_enable_charging(false);

	rc = smb1351_masked_write(0x3, 0x3F, reg_value);
	smb1351_access_err(rc, smb1351_write_access, 0x3);

	if (previous_cv != *(u32 *)(data))
		smb1351_enable_charging(true);

	previous_cv = *(u32 *)(data);

	return status;
}

static u32 charging_get_current(void *data)
{
	u32 status = STATUS_OK;
	int rc;
	u8 reg;

	smb1351_enable_volatile_access();
	rc = smb1351_read_reg(0x39, &reg);
	smb1351_access_err(rc, smb1351_read_access, 0x39);

	*(u32 *)data = fast_chg_current[reg & 0xf];

	return status;
}

static u32 charging_set_current(void *data)
{
	u32 status = STATUS_OK;
	int current_value = *(u32 *)data;
	int i, rc;

	smb1351_enable_volatile_access();

	/* Set Input current = command register, 31h[3] = "1" */
	rc = smb1351_masked_write(0x31, 0x8, 0x8);
	smb1351_access_err(rc, smb1351_write_access, 0x31);
	/* Set USB AC control = USB AC, 31h[0]  ="1" */
	rc = smb1351_masked_write(0x31, 0x1, 0x1);
	smb1351_access_err(rc, smb1351_write_access, 0x31);

	i = get_closest_fast_chg_current(current_value);
	rc = smb1351_masked_write(0x0, 0xf0, i << 4);
	smb1351_access_err(rc, smb1351_write_access, 0x0);

	return status;
}

static u32 charging_set_input_current(void *data)
{
	u32 status = STATUS_OK;
	int current_value = *(u32 *)data;
	int i, rc;

	smb1351_enable_volatile_access();
	i = get_closest_input_current(current_value);
	smb1351_enable_AICL(false);
	rc = smb1351_masked_write(0x0, 0xf, i);
	smb1351_access_err(rc, smb1351_write_access, 0x0);
	smb1351_enable_AICL(true);

	return status;
}

static u32 charging_get_input_current(void *data)
{
	int rc;
	u8 reg;

	smb1351_enable_volatile_access();
	rc = smb1351_read_reg(0x36, &reg);
	smb1351_access_err(rc, smb1351_read_access, 0x36);

	*(u32 *)data = input_current[reg & 0xf];
	return STATUS_OK;
}

static u32 charging_get_charging_status(void *data)
{
	u32 status = STATUS_OK;
	int rc;
	u8 reg;

	smb1351_enable_volatile_access();
	rc = smb1351_read_reg(0x42, &reg);
	smb1351_access_err(rc, smb1351_read_access, 0x42);
	*(u32 *)data = (reg & 0x1);

	return status;
}

static u32 charging_reset_watch_dog_timer(void *data)
{
	u32 status = STATUS_OK;
	u8 reg;
	int rc;

	/* touch register to kick watchdog */
	smb1351_enable_volatile_access();
	rc = smb1351_read_reg(0x8, &reg);
	smb1351_access_err(rc, smb1351_read_access, 0x8);

	return status;
}

static u32 charging_set_hv_threshold(void *data)
{
	u32 status = STATUS_OK;

	u32 set_hv_voltage;
	u32 array_size;
	u16 register_value;
	u32 voltage = *(u32 *)(data);

	array_size = GETARRAYNUM(VCDT_HV_VTH);
	set_hv_voltage =
		bmt_find_closest_level(VCDT_HV_VTH, array_size, voltage);
	register_value = charging_parameter_to_value(VCDT_HV_VTH, array_size,
						     set_hv_voltage);
	upmu_set_rg_vcdt_hv_vth(register_value);

	return status;
}

static u32 charging_get_hv_status(void *data)
{
	u32 status = STATUS_OK;

	*(bool *)(data) = upmu_get_rgs_vcdt_hv_det();
	return status;
}

static u32 charging_get_battery_status(void *data)
{
	u32 status = STATUS_OK;

	/* upmu_set_baton_tdet_en(1); */
	upmu_set_rg_baton_en(1);
	*(bool *)(data) = upmu_get_rgs_baton_undet();

	return status;
}

static u32 charging_get_charger_det_status(void *data)
{
	u32 status = STATUS_OK;

	*(bool *)(data) = upmu_get_rgs_chrdet();

	return status;
}

static u32 charging_get_charger_type(void *data)
{
	u32 status = STATUS_OK;
	*(int *)(data) = hw_charger_type_detection();
	return status;
}

static u32 charging_get_is_pcm_timer_trigger(void *data)
{
	u32 status = STATUS_OK;

	if (slp_get_wake_reason() == 3)
		*(bool *)(data) = true;
	else
		*(bool *)(data) = false;

	pr_debug("slp_get_wake_reason=%d\n",
		    slp_get_wake_reason());

	*(bool *)(data) = false;
	return status;
}

static u32 charging_set_platform_reset(void *data)
{
	u32 status = STATUS_OK;

	pr_debug("charging_set_platform_reset\n");

	if (system_state == SYSTEM_BOOTING)
		arch_reset(0, NULL);
	else
		orderly_reboot(true);

	return status;
}

static u32 charging_get_platform_boot_mode(void *data)
{
	u32 status = STATUS_OK;

	*(u32 *)(data) = get_boot_mode();

	pr_debug("get_boot_mode=%d\n", get_boot_mode());

	return status;
}

static u32 charging_enable_powerpath(void *data)
{
	u32 status = STATUS_OK;
	u32 enable = *(u32 *)(data);
	int rc;

	smb1351_enable_volatile_access();
	if (enable)
		rc = smb1351_masked_write(CMD_REG_IL, SUSPEND_MODE_MASK, 0x0);
	else
		rc = smb1351_masked_write(CMD_REG_IL, SUSPEND_MODE_MASK, 0x40);
	if (rc)
		pr_debug("failed to %s power path\n",
			 enable ? "enable" : "disable");
	return status;
}

static u32 charging_boost_enable(void *data)
{
	u32 status = STATUS_OK;
	u32 enable = *(u32 *)(data);
	int rc;

	smb1351_enable_volatile_access();
	if (enable == true) {
		pr_notice("Enable OTG\n");
		/* OTG current limit = 500mA, 0Ah[3:2] = "01" */
		rc = smb1351_masked_write(OTG_TLIM_CTRL_REG,
					  OTG_DCIN_CURRENT_MASK, 0x4);
		if (rc)
			pr_debug(
				"failed to set DCIN current limit to 500 mA\n");
		gpio_direction_output(chip->otg_en_gpio, 1);
		/* OTG current limit = 750 mA, 0Ah[3:2] = "10" */
		rc = smb1351_masked_write(OTG_TLIM_CTRL_REG,
					  OTG_DCIN_CURRENT_MASK, 0x8);
		if (rc)
			pr_debug(
				"failed to set DCIN current limit to 750 mA\n");
	} else {
		pr_notice("Disable OTG\n");
		/* OTG current limit = 500mA, 0Ah[3:2] = "01" */
		rc = smb1351_masked_write(OTG_TLIM_CTRL_REG,
					  OTG_DCIN_CURRENT_MASK, 0x4);
		if (rc)
			pr_debug(
				"failed to set DCIN current limit to 500 mA\n");
		gpio_direction_output(chip->otg_en_gpio, 0);
	}

	return status;
}

static u32 charging_set_ta_current_pattern(void *data)
{

	return STATUS_OK;
}

static u32 (*const charging_func[CHARGING_CMD_NUMBER])(void *data) = {
	charging_hw_init,
	charging_dump_register,
	charging_enable,
	charging_set_cv_voltage,
	charging_get_current,
	charging_set_current,
	charging_get_input_current,
	charging_set_input_current,
	charging_get_charging_status,
	charging_reset_watch_dog_timer,
	charging_set_hv_threshold,
	charging_get_hv_status,
	charging_get_battery_status,
	charging_get_charger_det_status,
	charging_get_charger_type,
	charging_get_is_pcm_timer_trigger,
	charging_set_platform_reset,
	charging_get_platform_boot_mode,
	charging_enable_powerpath,
	charging_boost_enable,
	charging_set_ta_current_pattern};

s32 smb1351_control_interface(int cmd, void *data)
{
	s32 status;

	if (cmd < CHARGING_CMD_NUMBER)
		status = charging_func[cmd](data);
	else
		return STATUS_UNSUPPORTED;

	return status;
}

static ssize_t reg_status_get(struct device *dev,
			      struct device_attribute *devattr, char *buf)
{
	int i = 0;
	char tmp_buf[64];
	u8 reg;

	smb1351_enable_volatile_access();
	sprintf(tmp_buf, "smb1351 Configuration Registers Detail\n"
			 "==================\n");
	strcpy(buf, tmp_buf);

	for (i = 0; i <= 20; i++) {
		smb1351_read_reg(0x0 + i, &reg);
		sprintf(tmp_buf, "Reg%02xh:\t0x%02x\n", 0x0 + i, reg);
		strcat(buf, tmp_buf);
	}
	for (i = 0; i <= 26; i++) {
		smb1351_read_reg(0x15 + i, &reg);
		sprintf(tmp_buf, "Reg%02xh:\t0x%02x\n", 0x15 + i, reg);
		strcat(buf, tmp_buf);
	}
	for (i = 0; i <= 24; i++) {
		smb1351_read_reg(0x30 + i, &reg);
		sprintf(tmp_buf, "Reg%02xh:\t0x%02x\n", 0x30 + i, reg);
		strcat(buf, tmp_buf);
	}

	return strlen(buf);
}

static ssize_t chargeric_status_get(struct device *dev,
				    struct device_attribute *devattr, char *buf)
{
	return sprintf(buf, "%d\n", charger_status);
}

static DEVICE_ATTR(chargerIC_status, 0444, chargeric_status_get, NULL);
static DEVICE_ATTR(reg_status, 0444, reg_status_get, NULL);

static struct attribute *smb1351_charger_attributes[] = {
	&dev_attr_chargerIC_status.attr, &dev_attr_reg_status.attr, NULL};

static const struct attribute_group smb1351_charger_group = {
	.attrs = smb1351_charger_attributes,
};

static int smb1351_driver_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}

static int smb1351_driver_resume(struct i2c_client *client)
{
	return 0;
}

static void smb1351_driver_shutdown(struct i2c_client *client)
{
}

static ssize_t show_smb1351_access(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	pr_info("0x%x\n", g_reg_value_smb1351);
	return sprintf(buf, "0x%x\n", g_reg_value_smb1351);
}

static ssize_t store_smb1351_access(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL, temp_buf[16];
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	pr_info("\n");
	smb1351_enable_volatile_access();

	if (buf != NULL && size != 0) {
		strncpy(temp_buf, buf, 15);
		temp_buf[15] = '\0';
		pvalue = temp_buf;

		if (size > 4) {
			ret = kstrtouint(strsep(&pvalue, " "), 0, &reg_address);
			if (ret) {
				pr_debug("wrong format!\n");
				return size;
			}
			ret = kstrtouint(pvalue, 0, &reg_value);
			if (ret) {
				pr_debug("wrong format!\n");
				return size;
			}
			pr_info("write smb1351 reg 0x%x with value 0x%x !\n",
				reg_address, reg_value);
			ret = smb1351_masked_write(reg_address, 0xFF,
						   reg_value);

		} else {
			ret = kstrtouint(pvalue, 0, &reg_address);
			if (ret) {
				pr_debug("wrong format!\n");
				return size;
			}
			ret = smb1351_read_reg(reg_address,
					       &g_reg_value_smb1351);
			pr_info("read smb1351 reg 0x%x with value 0x%x !\n",
				reg_address, g_reg_value_smb1351);
			pr_info("Please use \"cat smb1351_access\" to get value\r\n");
		}
	}
	return size;
}

static DEVICE_ATTR(smb1351_access, 0644, show_smb1351_access,
		   store_smb1351_access);

static int smb1351_user_space_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	pr_info("smb1351_user_space_probe!\n");
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_smb1351_access);

	return 0;
}

struct platform_device smb1351_user_space_device = {
	.name = "smb1351-user", .id = -1,
};

static struct platform_driver smb1351_user_space_driver = {
	.probe = smb1351_user_space_probe,
	.driver = {

			.name = "smb1351-user",
		},
};
static int smb1351_driver_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int ret;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->dev = &client->dev;
	chip->last_charger_type = CHARGER_UNKNOWN;
	chip->last_temp_state = unknown_temp_state;

	mutex_init(&chip->read_write_lock);
	wake_lock_init(&chip->jeita_setting_wake_lock, WAKE_LOCK_SUSPEND,
		       "jeita_setting_wake_lock");
	i2c_set_clientdata(client, chip);
	ret = of_get_smb1351_platform_data(chip->dev);
	if (ret) {
		pr_debug("failed to get smb1351 platform data through dt!!\n");
		return ret;
	}

	ret = gpio_request_one(chip->otg_en_gpio, GPIOF_OUT_INIT_LOW, "OTG_EN");
	if (ret) {
		pr_debug("Couldn't request GPIO for OTG pinctrl\n");
		return ret;
	}

	smb1351_initial_setting();

	bat_charger_register(smb1351_control_interface);

	/* smb1351 user space access interface */
	ret = platform_device_register(&smb1351_user_space_device);
	if (ret) {
		pr_debug("Unable to device register(%d)\n", ret);
		return ret;
	}
	ret = platform_driver_register(&smb1351_user_space_driver);
	if (ret) {
		pr_debug("Unable to register driver (%d)\n", ret);
		return ret;
	}

	ret = smb1351_enable_volatile_access();
	if (!ret)
		charger_status = true;
	ret = sysfs_create_group(&client->dev.kobj, &smb1351_charger_group);
	if (ret)
		pr_debug("unable to create the sysfs\n");

	return 0;
}

static int smb1351_driver_remove(struct i2c_client *client)
{
	mutex_destroy(&chip->read_write_lock);
	wake_lock_destroy(&chip->jeita_setting_wake_lock);
	gpio_free(chip->otg_en_gpio);
	return 0;
}

static const struct i2c_device_id smb1351_charger_id[] = {
	{"smb1351-charger", 0}, {} };

#ifdef CONFIG_OF
static const struct of_device_id smb1351_match_table[] = {
	{.compatible = "qcom,smb1351-charger"}, {},
};

MODULE_DEVICE_TABLE(of, smb1351_match_table);

static int of_get_smb1351_platform_data(struct device *dev)
{
	if (dev->of_node) {
		const struct of_device_id *match;

		match = of_match_device(of_match_ptr(smb1351_match_table), dev);
		if (!match) {
			pr_debug("Error: No device match found\n");
			return -ENODEV;
		}
	}
	chip->otg_en_gpio = of_get_named_gpio(dev->of_node, "otg-gpio", 0);
	pr_info("OTG enable gpio: %d\n", chip->otg_en_gpio);
	return 0;
}
#else
static int of_get_smb1351_platform_data(struct device *dev)
{
	return 0;
}
#endif

static struct i2c_driver smb1351_charger_driver = {
	.driver = {

			.name = "smb1351-charger",
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = smb1351_match_table,
#endif
		},
	.probe = smb1351_driver_probe,
	.shutdown = smb1351_driver_shutdown,
	.suspend = smb1351_driver_suspend,
	.resume = smb1351_driver_resume,
	.remove = smb1351_driver_remove,
	.id_table = smb1351_charger_id,
};

module_i2c_driver(smb1351_charger_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SMB1351 Charger Driver");
