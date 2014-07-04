/* Copyright (c) 2012-2014 The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/i2c/smb350.h>
#include <linux/bitops.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/printk.h>
#include <linux/wakelock.h>

/* Register definitions */
#define CHG_CURRENT_REG			0x00	/* Non-Volatile + mirror */
#define CHG_OTHER_CURRENT_REG		0x01	/* Non-Volatile + mirror */
#define VAR_FUNC_REG			0x02	/* Non-Volatile + mirror */
#define FLOAT_VOLTAGE_REG		0x03	/* Non-Volatile + mirror */
#define CHG_CTRL_REG			0x04	/* Non-Volatile + mirror */
#define STAT_TIMER_REG			0x05	/* Non-Volatile + mirror */
#define PIN_ENABLE_CTRL_REG		0x06	/* Non-Volatile + mirror */
#define THERM_CTRL_A_REG		0x07	/* Non-Volatile + mirror */
#define SYSOK_USB3_SELECT_REG		0x08	/* Non-Volatile + mirror */
#define CTRL_FUNCTIONS_REG		0x09	/* Non-Volatile + mirror */
#define OTG_TLIM_THERM_CNTRL_REG	0x0A	/* Non-Volatile + mirror */
#define TEMP_MONITOR_REG		0x0B	/* Non-Volatile + mirror */
#define FAULT_IRQ_REG			0x0C	/* Non-Volatile */
#define IRQ_ENABLE_REG			0x0D	/* Non-Volatile */
#define SYSOK_REG			0x0E	/* Non-Volatile + mirror */

#define AUTO_INPUT_VOLT_DETECT_REG	0x10	/* Non-Volatile Read-Only */
#define STATUS_IRQ_REG			0x11	/* Non-Volatile Read-Only */
#define I2C_SLAVE_ADDR_REG		0x12	/* Non-Volatile Read-Only */

#define CMD_A_REG			0x30	/* Volatile Read-Write */
#define CMD_B_REG			0x31	/* Volatile Read-Write */
#define CMD_C_REG			0x33	/* Volatile Read-Write */

#define HW_VERSION_REG			0x34	/* Volatile Read-Only */

#define IRQ_STATUS_A_REG		0x35	/* Volatile Read-Only */
#define IRQ_STATUS_B_REG		0x36	/* Volatile Read-Only */
#define IRQ_STATUS_C_REG		0x37	/* Volatile Read-Only */
#define IRQ_STATUS_D_REG		0x38	/* Volatile Read-Only */
#define IRQ_STATUS_E_REG		0x39	/* Volatile Read-Only */
#define IRQ_STATUS_F_REG		0x3A	/* Volatile Read-Only */

#define STATUS_A_REG			0x3B	/* Volatile Read-Only */
#define STATUS_B_REG			0x3D	/* Volatile Read-Only */
/* Note: STATUS_C_REG was removed from SMB349 to SMB350 */
#define STATUS_D_REG			0x3E	/* Volatile Read-Only */
#define STATUS_E_REG			0x3F	/* Volatile Read-Only */

#define IRQ_STATUS_NUM (IRQ_STATUS_F_REG - IRQ_STATUS_A_REG + 1)

/* Status bits and masks */
#define SMB350_MASK(BITS, POS)		((u8)(((1 << BITS) - 1) << POS))
#define FAST_CHG_CURRENT_MASK		SMB350_MASK(4, 4)

#define SMB350_FAST_CHG_MIN_MA		1000
#define SMB350_FAST_CHG_STEP_MA		200
#define SMB350_FAST_CHG_MAX_MA		3600

#define TERM_CURRENT_MASK		SMB350_MASK(3, 2)

#define SMB350_TERM_CUR_MIN_MA		200
#define SMB350_TERM_CUR_STEP_MA		100
#define SMB350_TERM_CUR_MAX_MA		700

#define CMD_A_VOLATILE_WR_PERM		BIT(7)
#define CHG_CTRL_CURR_TERM_END_CHG	BIT(6)

struct smb350_chg {
	int			chg_current_ma;
	int			term_current_ma;
	int			chg_en_n_gpio;
	int			chg_shdn_n_gpio;
	int			irq;
	int			fake_battery_soc;
	int			version;
	struct i2c_client	*client;
	struct dentry		*dent;
	struct power_supply	dc_psy;
	struct power_supply	batt_psy;
	const char		*fuel_gauge_name;
};

static struct smb350_chg *the_chip;

struct debug_reg {
	char	*name;
	u8	reg;
};

#define SMB350_DEBUG_REG(x) {#x, x##_REG}

static struct debug_reg smb350_debug_regs[] = {
	SMB350_DEBUG_REG(CHG_CURRENT),
	SMB350_DEBUG_REG(CHG_OTHER_CURRENT),
	SMB350_DEBUG_REG(VAR_FUNC),
	SMB350_DEBUG_REG(FLOAT_VOLTAGE),
	SMB350_DEBUG_REG(CHG_CTRL),
	SMB350_DEBUG_REG(STAT_TIMER),
	SMB350_DEBUG_REG(PIN_ENABLE_CTRL),
	SMB350_DEBUG_REG(THERM_CTRL_A),
	SMB350_DEBUG_REG(SYSOK_USB3_SELECT),
	SMB350_DEBUG_REG(CTRL_FUNCTIONS),
	SMB350_DEBUG_REG(OTG_TLIM_THERM_CNTRL),
	SMB350_DEBUG_REG(TEMP_MONITOR),
	SMB350_DEBUG_REG(FAULT_IRQ),
	SMB350_DEBUG_REG(IRQ_ENABLE),
	SMB350_DEBUG_REG(SYSOK),
	SMB350_DEBUG_REG(AUTO_INPUT_VOLT_DETECT),
	SMB350_DEBUG_REG(STATUS_IRQ),
	SMB350_DEBUG_REG(I2C_SLAVE_ADDR),
	SMB350_DEBUG_REG(CMD_A),
	SMB350_DEBUG_REG(CMD_B),
	SMB350_DEBUG_REG(CMD_C),
	SMB350_DEBUG_REG(HW_VERSION),
	SMB350_DEBUG_REG(IRQ_STATUS_A),
	SMB350_DEBUG_REG(IRQ_STATUS_B),
	SMB350_DEBUG_REG(IRQ_STATUS_C),
	SMB350_DEBUG_REG(IRQ_STATUS_D),
	SMB350_DEBUG_REG(IRQ_STATUS_E),
	SMB350_DEBUG_REG(IRQ_STATUS_F),
	SMB350_DEBUG_REG(STATUS_A),
	SMB350_DEBUG_REG(STATUS_B),
	SMB350_DEBUG_REG(STATUS_D),
	SMB350_DEBUG_REG(STATUS_E),
};

/*
 * Read 8-bit register value. return negative value on error.
 */
static int smb350_read_reg(struct i2c_client *client, u8 reg)
{
	int val;

	val = i2c_smbus_read_byte_data(client, reg);
	if (val < 0)
		pr_err("i2c read fail. reg=0x%x.ret=%d.\n", reg, val);
	else
		pr_debug("reg=0x%02X.val=0x%02X.\n", reg , val);

	return val;
}

/*
 * Write 8-bit register value. return negative value on error.
 */
static int smb350_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, val);
	if (ret < 0)
		pr_err("i2c read fail. reg=0x%x.val=0x%x.ret=%d.\n",
		       reg, val, ret);
	else
		pr_debug("reg=0x%02X.val=0x%02X.\n", reg , val);

	return ret;
}

static int smb350_masked_write(struct i2c_client *client, int reg, u8 mask,
			       u8 val)
{
	int ret;
	int temp;
	int shift = find_first_bit((unsigned long *) &mask, 8);

	temp = smb350_read_reg(client, reg);
	if (temp < 0)
		return temp;

	temp &= ~mask;
	temp |= (val << shift) & mask;
	ret = smb350_write_reg(client, reg, temp);

	return ret;
}

static bool smb350_is_dc_present(struct i2c_client *client)
{
	u16 irq_status_f = smb350_read_reg(client, IRQ_STATUS_F_REG);
	bool power_ok = irq_status_f & 0x01;

	/* Power-ok , IRQ_STATUS_F_REG bit#0 */
	if (power_ok)
		pr_debug("DC is present.\n");
	else
		pr_debug("DC is missing.\n");

	return power_ok;
}

static bool smb350_is_charger_present(struct i2c_client *client)
{
	int val;

	/* Normally the device is non-removable and embedded on the board.
	 * Verify that charger is present by getting I2C response.
	 */
	val = smb350_read_reg(client, STATUS_B_REG);
	if (val < 0)
		return false;

	return true;
}

static void smb350_enable_charging(struct smb350_chg *chip, bool enable)
{
	int val = !enable; /* active low */

	pr_debug("enable=%d.\n", enable);
	if (gpio_is_valid(chip->chg_en_n_gpio))
		gpio_set_value_cansleep(chip->chg_en_n_gpio, val);
}

/* When the status bit of a certain condition is read,
 * the corresponding IRQ signal is cleared.
 */
static int smb350_clear_irq(struct i2c_client *client)
{
	int ret;

	ret = smb350_read_reg(client, IRQ_STATUS_A_REG);
	if (ret < 0) {
		pr_err("Couldn't clear IRQ A\n");
		return ret;
	}
	ret = smb350_read_reg(client, IRQ_STATUS_B_REG);
	if (ret < 0) {
		pr_err("Couldn't clear IRQ B\n");
		return ret;
	}
	ret = smb350_read_reg(client, IRQ_STATUS_C_REG);
	if (ret < 0) {
		pr_err("Couldn't clear IRQ C\n");
		return ret;
	}
	ret = smb350_read_reg(client, IRQ_STATUS_D_REG);
	if (ret < 0) {
		pr_err("Couldn't clear IRQ D\n");
		return ret;
	}
	ret = smb350_read_reg(client, IRQ_STATUS_E_REG);
	if (ret < 0) {
		pr_err("Couldn't clear IRQ E\n");
		return ret;
	}
	ret = smb350_read_reg(client, IRQ_STATUS_F_REG);
	if (ret < 0) {
		pr_err("Couldn't clear IRQ F\n");
		return ret;
	}

	return 0;
}

/*
 * The STAT pin is low when charging and high when not charging.
 * When the smb350 start/stop charging the STAT pin triggers an interrupt.
 * Interrupt is triggered on both rising or falling edge.
 */
/*
 * Do the IRQ work from a thread context rather than interrupt context.
 * Read status registers to clear interrupt source.
 * Notify the power-supply driver about change detected.
 * Relevant events for start/stop charging:
 * 1. DC insert/remove
 * 2. End-Of-Charging
 * 3. Battery insert/remove
 * 4. Temperture too hot/cold
 * 5. Charging timeout expired.
 */
static irqreturn_t smb350_stat_handler(int irq, void *dev_id)
{
	int ret = 0;
	struct smb350_chg *chip = dev_id;

	ret = smb350_clear_irq(chip->client);
	if (ret < 0) {
		pr_err("Couldn't clear IRQ ret = %d\n", ret);
	} else {
		pr_debug("Notify power_supply_changed.\n");
		power_supply_changed(&chip->dc_psy);
	}
	return IRQ_HANDLED;
}

static enum power_supply_property pm_power_batt_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TECHNOLOGY,
};

#define CHG_STAT_MASK		0x06
#define CHG_STAT_SHIFT		1
static int smb350_get_battery_status(struct smb350_chg *chip)
{
	int reg;

	reg = smb350_read_reg(chip->client, STATUS_B_REG);
	if (reg < 0)
		return POWER_SUPPLY_STATUS_DISCHARGING;

	if (reg & CHG_STAT_MASK)
		return POWER_SUPPLY_STATUS_CHARGING;

	return POWER_SUPPLY_STATUS_DISCHARGING;
}

#define BATTERY_MISSING_BIT	BIT(4)
static int smb350_battery_present(struct smb350_chg *chip)
{
	int reg;

	reg = smb350_read_reg(chip->client, IRQ_STATUS_B_REG);
	if (reg < 0)
		return true;

	if (reg & BATTERY_MISSING_BIT)
		return false;

	return false;
}

static int smb350_get_charge_type(struct smb350_chg *chip)
{
	int type;
	int reg;

	reg = smb350_read_reg(chip->client, STATUS_B_REG);
	if (reg < 0)
		return POWER_SUPPLY_CHARGE_TYPE_NONE;

	type = (reg & CHG_STAT_MASK) >> CHG_STAT_SHIFT;
	switch (type) {
	case 0:
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
	case 1:
		return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
	case 2:
		return POWER_SUPPLY_CHARGE_TYPE_FAST;
	case 3:
		return POWER_SUPPLY_CHARGE_TYPE_FAST;
	}

	return POWER_SUPPLY_CHARGE_TYPE_NONE;
}

static int smb350_get_capacity(struct smb350_chg *chip)
{
	return 75;
}

#define HOT_HARD_LIMIT_BIT	BIT(6)
#define COLD_HARD_LIMIT_BIT	BIT(4)
#define HOT_SOFT_LIMIT_BIT	BIT(2)
#define COLD_SOFT_LIMIT_BIT	BIT(0)
static int smb350_get_health(struct smb350_chg *chip)
{
	int reg;

	reg = smb350_read_reg(chip->client, IRQ_STATUS_A_REG);
	if (reg & HOT_HARD_LIMIT_BIT)
		return POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (reg & COLD_HARD_LIMIT_BIT)
		return POWER_SUPPLY_HEALTH_COLD;
	else if (reg & HOT_SOFT_LIMIT_BIT)
		return POWER_SUPPLY_HEALTH_WARM;
	else if (reg & COLD_SOFT_LIMIT_BIT)
		return POWER_SUPPLY_HEALTH_COOL;
	return POWER_SUPPLY_HEALTH_GOOD;
}

static int smb350_batt_get_property(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	int ret = 0;
	struct smb350_chg *chip = container_of(psy,
						 struct smb350_chg,
						 batt_psy);
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = smb350_get_battery_status(chip);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = smb350_battery_present(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = smb350_get_charge_type(chip);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = smb350_get_capacity(chip);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = smb350_get_health(chip);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		val->intval = 0;
		break;
	default:
		pr_err("Invalid prop = %d.\n", psp);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int smb350_batt_set_property(struct power_supply *psy,
			       enum power_supply_property psp,
			       const union power_supply_propval *val)
{
	int ret = 0;
	struct smb350_chg *chip =
		container_of(psy, struct smb350_chg, batt_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
		chip->fake_battery_soc = val->intval;
		power_supply_changed(&chip->batt_psy);
		break;
	default:
		pr_err("Invalid prop = %d.\n", psp);
		ret = -EINVAL;
	}

	return ret;
}

static int smb350_battery_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

static enum power_supply_property pm_power_props[] = {
	/* real time */
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	/* fixed */
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_MODEL_NAME,
};

static char *pm_power_supplied_to[] = {
	"battery",
};

static int smb350_get_property(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	int ret = 0;
	struct smb350_chg *chip = container_of(psy,
						 struct smb350_chg,
						 dc_psy);
	struct i2c_client *client = chip->client;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = smb350_is_charger_present(client);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = smb350_is_dc_present(client);
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = SMB350_NAME;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = "Summit Microelectronics";
		break;
	default:
		pr_err("Invalid prop = %d.\n", psp);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int smb350_set_property(struct power_supply *psy,
			       enum power_supply_property psp,
			       const union power_supply_propval *val)
{
	int ret = 0;
	struct smb350_chg *chip =
		container_of(psy, struct smb350_chg, dc_psy);

	switch (psp) {
	/*
	 *  Allow a smart battery to Start/Stop charging.
	 *  i.e. when End-Of-Charging detected.
	 *  The SMB350 can be configured to terminate charging
	 *  when charge-current reaching Termination-Current.
	 */
	case POWER_SUPPLY_PROP_ONLINE:
		smb350_enable_charging(chip, val->intval);
		break;
	default:
		pr_err("Invalid prop = %d.\n", psp);
		ret = -EINVAL;
	}

	return ret;
}

static int smb350_set_chg_current(struct i2c_client *client, int current_ma)
{
	int ret;
	u8 temp;

	if ((current_ma < SMB350_FAST_CHG_MIN_MA) ||
	    (current_ma >  SMB350_FAST_CHG_MAX_MA)) {
		pr_err("invalid current %d mA.\n", current_ma);
		return -EINVAL;
	}

	temp = (current_ma - SMB350_FAST_CHG_MIN_MA) / SMB350_FAST_CHG_STEP_MA;

	pr_debug("fast-chg-current=%d mA setting %02x\n", current_ma, temp);

	ret = smb350_masked_write(client, CHG_CURRENT_REG,
				  FAST_CHG_CURRENT_MASK, temp);

	return ret;
}

static int smb350_set_term_current(struct i2c_client *client, int current_ma)
{
	int ret;
	u8 temp;

	if ((current_ma < SMB350_TERM_CUR_MIN_MA) ||
	    (current_ma >  SMB350_TERM_CUR_MAX_MA)) {
		pr_err("invalid current %d mA to set\n", current_ma);
		return -EINVAL;
	}

	temp = (current_ma - SMB350_TERM_CUR_MIN_MA) / SMB350_TERM_CUR_STEP_MA;

	pr_debug("term-current=%d mA setting %02x\n", current_ma, temp);

	ret = smb350_masked_write(client, CHG_OTHER_CURRENT_REG,
				  TERM_CURRENT_MASK, temp);

	return ret;
}

static int smb350_set_reg(void *data, u64 val)
{
	u32 addr = (u32) data;
	int ret;
	struct i2c_client *client = the_chip->client;

	ret = smb350_write_reg(client, addr, (u8) val);

	return ret;
}

static int smb350_get_reg(void *data, u64 *val)
{
	u32 addr = (u32) data;
	int ret;
	struct i2c_client *client = the_chip->client;

	ret = smb350_read_reg(client, addr);
	if (ret < 0)
		return ret;

	*val = ret;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(reg_fops, smb350_get_reg, smb350_set_reg, "0x%02llx\n");

static int smb350_create_debugfs_entries(struct smb350_chg *chip)
{
	int i;

	chip->dent = debugfs_create_dir(SMB350_NAME, NULL);
	if (IS_ERR(chip->dent)) {
		pr_err("smb350 driver couldn't create debugfs dir\n");
		return -EFAULT;
	}

	for (i = 0 ; i < ARRAY_SIZE(smb350_debug_regs) ; i++) {
		char *name = smb350_debug_regs[i].name;
		u32 reg = smb350_debug_regs[i].reg;
		struct dentry *file;

		file = debugfs_create_file(name, 0644, chip->dent,
					(void *) reg, &reg_fops);
		if (IS_ERR(file)) {
			pr_err("debugfs_create_file %s failed.\n", name);
			return -EFAULT;
		}
	}

	return 0;
}

static int smb350_hw_init(struct smb350_chg *chip)
{
	int ret;
	struct i2c_client *client = chip->client;

	pr_debug("\n");

	smb350_enable_charging(chip, true);
	msleep(100);

	if (gpio_is_valid(chip->chg_shdn_n_gpio)) {
		gpio_set_value_cansleep(chip->chg_shdn_n_gpio, 1); /* Normal */
		msleep(100); /* Allow the device to exist shutdown */
	}

	/* I2C transaction allowed only after device exit suspend */
	ret = smb350_read_reg(client, I2C_SLAVE_ADDR_REG);
	if ((ret>>1) != client->addr) {
		pr_err("No device.\n");
		return -ENODEV;
	}

	chip->version = smb350_read_reg(client, HW_VERSION_REG);
	chip->version &= 0x0F; /* bits 0..3 */

	ret = smb350_write_reg(client, CMD_A_REG, CMD_A_VOLATILE_WR_PERM);
	if (ret) {
		pr_err("Failed to set VOLATILE_WR_PERM ret=%d\n", ret);
		return ret;
	}

	/*
	 * Disable SMB350 pulse-IRQ mechanism,
	 * we use interrupts based on charging-status-transition
	 */

	/* Enable STATUS output (regardless of IRQ-pulses) */
	smb350_masked_write(client, CMD_A_REG, BIT(0), 0);

	/* Disable LED blinking - avoid periodic irq */
	smb350_masked_write(client, PIN_ENABLE_CTRL_REG, BIT(7), 0);

	/* Disable Failure SMB-IRQ */
	ret = smb350_write_reg(client, FAULT_IRQ_REG, 0x00);
	if (ret) {
		pr_err("Failed to set FAULT_IRQ_REG ret=%d\n", ret);
		return ret;
	}

	/* Disable Event IRQ */
	ret = smb350_write_reg(client, IRQ_ENABLE_REG, 0x00);
	if (ret) {
		pr_err("Failed to set IRQ_ENABLE_REG ret=%d\n", ret);
		return ret;
	}

	/* Enable charging/not-charging status output via STAT pin */
	smb350_masked_write(client, STAT_TIMER_REG, BIT(5), 0);

	/* Set fast-charge current */
	if (chip->chg_current_ma) {
		ret = smb350_set_chg_current(client, chip->chg_current_ma);
		if (ret) {
			pr_err("Failed to set FAST_CHG_CURRENT ret=%d\n", ret);
			return ret;
		}
	}

	if (chip->term_current_ma > 0) {
		/* Enable Current Termination */
		smb350_masked_write(client, CHG_CTRL_REG, BIT(6), 0);

		/* Set Termination current */
		smb350_set_term_current(client, chip->term_current_ma);
	} else if (chip->term_current_ma == 0) {
		/* Disable Current Termination */
		smb350_masked_write(client, CHG_CTRL_REG, BIT(6), 1);
	}
	return 0;
}

static int smb350_register_batt_psy(struct smb350_chg *chip)
{
	int ret;

	chip->batt_psy.name = "battery";
	chip->batt_psy.type = POWER_SUPPLY_TYPE_BATTERY;
	chip->batt_psy.properties = pm_power_batt_props;
	chip->batt_psy.num_properties = ARRAY_SIZE(pm_power_batt_props);
	chip->batt_psy.get_property = smb350_batt_get_property;
	chip->batt_psy.set_property = smb350_batt_set_property;
	chip->batt_psy.property_is_writeable = smb350_battery_is_writeable;
	ret = power_supply_register(&chip->client->dev, &chip->batt_psy);
	if (ret) {
		pr_err("Couldn't register power_supply ret=%d.\n", ret);
		return ret;
	}

	return 0;
}

static int smb350_register_psy(struct smb350_chg *chip)
{
	int ret;

	chip->dc_psy.name = "dc";
	chip->dc_psy.type = POWER_SUPPLY_TYPE_MAINS;
	chip->dc_psy.supplied_to = pm_power_supplied_to;
	chip->dc_psy.num_supplicants = ARRAY_SIZE(pm_power_supplied_to);
	chip->dc_psy.properties = pm_power_props;
	chip->dc_psy.num_properties = ARRAY_SIZE(pm_power_props);
	chip->dc_psy.get_property = smb350_get_property;
	chip->dc_psy.set_property = smb350_set_property;

	ret = power_supply_register(&chip->client->dev, &chip->dc_psy);
	if (ret) {
		pr_err("failed to register power_supply. ret=%d.\n", ret);
		return ret;
	}

	return 0;
}

static int smb350_parse_dt(struct smb350_chg *chip)
{
	int rc;
	struct device_node *node = chip->client->dev.of_node;

	if (!node) {
		pr_err("No DT data Failing Probe\n");
		return -EINVAL;
	}

	chip->chg_en_n_gpio =
		of_get_named_gpio(node, "summit,chg-en-n-gpio", 0);
	pr_debug("chg_en_n_gpio = %d.\n", chip->chg_en_n_gpio);

	chip->chg_shdn_n_gpio =
		of_get_named_gpio(node,
				  "summit,chg-shdn-n-gpio", 0);
	pr_debug("chg_shdn_n_gpio = %d.\n", chip->chg_shdn_n_gpio);

	rc = of_property_read_u32(node, "summit,chg-current-ma",
				   &(chip->chg_current_ma));
	if (rc < 0) {
		chip->chg_current_ma = 0;
		pr_debug("chg_current_ma = %d rc = %d\n",
						chip->chg_current_ma, rc);
	}

	rc = of_property_read_u32(node, "summit,term-current-ma",
				   &(chip->term_current_ma));
	if (rc < 0) {
		chip->term_current_ma = rc;
		pr_debug("term_current_ma = %d rc = %d\n",
						chip->term_current_ma, rc);
	}

	rc = of_property_read_string(node, "summit,fuel-gauge-name",
					&(chip->fuel_gauge_name));
	if (rc < 0) {
		pr_debug("read of summit,fuel-gauge-name failure, rc = %d\n",
			rc);
	}

	return 0;
}

static int smb350_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	int ret = 0;
	struct smb350_chg *chip;

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("i2c func fail.\n");
		return -EIO;
	}

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		pr_err("alloc fail.\n");
		return -ENOMEM;
	}

	chip->fake_battery_soc = 75;

	chip->client = client;
	ret = smb350_parse_dt(chip);
	if (ret < 0) {
		pr_err("Couldn't to parse dt ret = %d\n", ret);
		return ret;
	}

	chip->irq = client->irq;
	pr_debug("irq#=%d.\n", chip->irq);

	if (gpio_is_valid(chip->chg_shdn_n_gpio)) {
		ret = gpio_request(chip->chg_shdn_n_gpio, "smb350_suspend");
		if (ret) {
			pr_err("gpio_request failed for %d ret=%d\n",
				chip->chg_shdn_n_gpio, ret);
			return ret;
		}
	}

	if (gpio_is_valid(chip->chg_en_n_gpio)) {
		ret = gpio_request(chip->chg_en_n_gpio, "smb350_chg_enable");
		if (ret) {
			pr_err("gpio_request failed for %d ret=%d\n",
				chip->chg_en_n_gpio, ret);
			goto free_shdn_gpio;
		}
	}

	i2c_set_clientdata(client, chip);

	ret = smb350_hw_init(chip);
	if (ret < 0) {
		pr_err("Couldn't initialize hw ret = %d\n", ret);
		goto free_en_gpio;
	}

	ret = smb350_register_batt_psy(chip);
	if (ret < 0) {
		pr_err("Couldn't register batt psy ret = %d\n", ret);
		goto free_en_gpio;
	}

	ret = smb350_register_psy(chip);
	if (ret < 0) {
		pr_err("Couldn't register dc psy ret = %d\n", ret);
		goto unregister_batt;
	}

	ret = smb350_create_debugfs_entries(chip);
	if (ret < 0) {
		pr_err("Couldn't create debugfs entries ret = %d\n", ret);
		goto unregister_dc;
	}

	ret = devm_request_threaded_irq(&client->dev, chip->irq,
		NULL, smb350_stat_handler,
		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT,
		"smb350_irq", chip);
	if (ret) {
		pr_err("request_irq %d failed.ret=%d\n", chip->irq, ret);
		goto remove_debugfs;
	}

	the_chip = chip;
	pr_info("HW Version = 0x%X.\n", chip->version);

	return 0;

remove_debugfs:
	debugfs_remove_recursive(chip->dent);
unregister_dc:
	power_supply_unregister(&chip->dc_psy);
unregister_batt:
	power_supply_unregister(&chip->batt_psy);
free_en_gpio:
	if (gpio_is_valid(chip->chg_en_n_gpio))
		gpio_free(chip->chg_en_n_gpio);
free_shdn_gpio:
	if (gpio_is_valid(chip->chg_shdn_n_gpio))
		gpio_free(chip->chg_shdn_n_gpio);

	return ret;
}

static int smb350_remove(struct i2c_client *client)
{
	struct smb350_chg *chip = i2c_get_clientdata(client);

	debugfs_remove_recursive(chip->dent);
	power_supply_unregister(&chip->dc_psy);
	power_supply_unregister(&chip->batt_psy);
	if (gpio_is_valid(chip->chg_en_n_gpio))
		gpio_free(chip->chg_en_n_gpio);
	if (gpio_is_valid(chip->chg_shdn_n_gpio))
		gpio_free(chip->chg_shdn_n_gpio);
	if (chip->irq)
		free_irq(chip->irq, chip);
	the_chip = NULL;

	return 0;
}

static const struct i2c_device_id smb350_id[] = {
	{SMB350_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, smb350_id);

static const struct of_device_id smb350_match[] = {
	{ .compatible = "summit,smb350-charger", },
	{ },
};

static struct i2c_driver smb350_driver = {
	.driver	= {
			.name	= SMB350_NAME,
			.owner	= THIS_MODULE,
			.of_match_table = of_match_ptr(smb350_match),
	},
	.probe		= smb350_probe,
	.remove		= smb350_remove,
	.id_table	= smb350_id,
};

static int __init smb350_init(void)
{
	return i2c_add_driver(&smb350_driver);
}
module_init(smb350_init);

static void __exit smb350_exit(void)
{
	return i2c_del_driver(&smb350_driver);
}
module_exit(smb350_exit);

MODULE_DESCRIPTION("Driver for SMB350 charger chip");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:" SMB350_NAME);
