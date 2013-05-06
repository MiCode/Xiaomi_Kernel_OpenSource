/* Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
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
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/i2c/smb350.h>
#include <linux/bitops.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/printk.h>

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

enum smb350_chg_status {
	SMB_CHG_STATUS_NONE		= 0,
	SMB_CHG_STATUS_PRE_CHARGE	= 1,
	SMB_CHG_STATUS_FAST_CHARGE	= 2,
	SMB_CHG_STATUS_TAPER_CHARGE	= 3,
};

static const char * const smb350_chg_status[] = {
	"none",
	"pre-charge",
	"fast-charge",
	"taper-charge"
};

struct smb350_device {
	/* setup */
	int			chg_current_ma;
	int			term_current_ma;
	int			chg_en_n_gpio;
	int			chg_susp_n_gpio;
	int			stat_gpio;
	int			irq;
	/* internal */
	enum smb350_chg_status	chg_status;
	struct i2c_client	*client;
	struct delayed_work	irq_work;
	struct dentry		*dent;
	struct wake_lock	chg_wake_lock;
	struct power_supply	dc_psy;
};

static struct smb350_device *smb350_dev;

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

static int smb350_get_prop_charge_type(struct smb350_device *dev)
{
	int status_b;
	enum smb350_chg_status status;
	int chg_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	bool chg_enabled;
	bool charger_err;
	struct i2c_client *client = dev->client;

	status_b = smb350_read_reg(client, STATUS_B_REG);
	if (status_b < 0) {
		pr_err("failed to read STATUS_B_REG.\n");
		return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}

	chg_enabled = (bool) (status_b & 0x01);
	charger_err = (bool) (status_b & (1<<6));

	if (!chg_enabled) {
		pr_warn("Charging not enabled.\n");
		/* release the wake-lock when DC power removed */
		if (wake_lock_active(&dev->chg_wake_lock))
			wake_unlock(&dev->chg_wake_lock);
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
	}

	if (charger_err) {
		pr_warn("Charger error detected.\n");
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
	}

	status = (status_b >> 1) & 0x3;

	if (status == SMB_CHG_STATUS_NONE)
		chg_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
	else if (status == SMB_CHG_STATUS_FAST_CHARGE) /* constant current */
		chg_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
	else if (status == SMB_CHG_STATUS_TAPER_CHARGE) /* constant voltage */
		chg_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
	else if (status == SMB_CHG_STATUS_PRE_CHARGE)
		chg_type = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;

	pr_debug("smb-chg-status=%d=%s.\n", status, smb350_chg_status[status]);

	if (dev->chg_status != status) { /* Status changed */
		if (status == SMB_CHG_STATUS_NONE) {
			pr_debug("Charging stopped.\n");
			wake_unlock(&dev->chg_wake_lock);
		} else {
			pr_debug("Charging started.\n");
			wake_lock(&dev->chg_wake_lock);
		}
	}

	dev->chg_status = status;

	return chg_type;
}

static void smb350_enable_charging(struct smb350_device *dev, bool enable)
{
	int val = !enable; /* active low */

	pr_debug("enable=%d.\n", enable);

	gpio_set_value_cansleep(dev->chg_en_n_gpio, val);
}

/* When the status bit of a certain condition is read,
 * the corresponding IRQ signal is cleared.
 */
static int smb350_clear_irq(struct i2c_client *client)
{
	int ret;

	ret = smb350_read_reg(client, IRQ_STATUS_A_REG);
	if (ret < 0)
		return ret;
	ret = smb350_read_reg(client, IRQ_STATUS_B_REG);
	if (ret < 0)
		return ret;
	ret = smb350_read_reg(client, IRQ_STATUS_C_REG);
	if (ret < 0)
		return ret;
	ret = smb350_read_reg(client, IRQ_STATUS_D_REG);
	if (ret < 0)
		return ret;
	ret = smb350_read_reg(client, IRQ_STATUS_E_REG);
	if (ret < 0)
		return ret;
	ret = smb350_read_reg(client, IRQ_STATUS_F_REG);
	if (ret < 0)
		return ret;

	return 0;
}

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
static void smb350_irq_worker(struct work_struct *work)
{
	int ret = 0;
	struct smb350_device *dev =
		container_of(work, struct smb350_device, irq_work.work);

	ret = smb350_clear_irq(dev->client);
	if (ret == 0) { /* Cleared ok */
		/* Notify Battery-psy about status changed */
		pr_debug("Notify power_supply_changed.\n");
		power_supply_changed(&dev->dc_psy);
	}
}

/*
 * The STAT pin is low when charging and high when not charging.
 * When the smb350 start/stop charging the STAT pin triggers an interrupt.
 * Interrupt is triggered on both rising or falling edge.
 */
static irqreturn_t smb350_irq(int irq, void *dev_id)
{
	struct smb350_device *dev = dev_id;

	pr_debug("\n");

	/* I2C transfers API should not run in interrupt context */
	schedule_delayed_work(&dev->irq_work, msecs_to_jiffies(100));

	return IRQ_HANDLED;
}

static enum power_supply_property pm_power_props[] = {
	/* real time */
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	/* fixed */
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_CURRENT_MAX,
};

static char *pm_power_supplied_to[] = {
	"battery",
};

static int smb350_get_property(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	int ret = 0;
	struct smb350_device *dev = container_of(psy,
						 struct smb350_device,
						 dc_psy);
	struct i2c_client *client = dev->client;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = smb350_is_charger_present(client);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = smb350_is_dc_present(client);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = smb350_get_prop_charge_type(dev);
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = SMB350_NAME;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = "Summit Microelectronics";
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = dev->chg_current_ma;
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
	struct smb350_device *dev =
		container_of(psy, struct smb350_device, dc_psy);

	switch (psp) {
	/*
	 *  Allow a smart battery to Start/Stop charging.
	 *  i.e. when End-Of-Charging detected.
	 *  The SMB350 can be configured to terminate charging
	 *  when charge-current reaching Termination-Current.
	 */
	case POWER_SUPPLY_PROP_ONLINE:
		smb350_enable_charging(dev, val->intval);
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
	struct i2c_client *client = smb350_dev->client;

	ret = smb350_write_reg(client, addr, (u8) val);

	return ret;
}

static int smb350_get_reg(void *data, u64 *val)
{
	u32 addr = (u32) data;
	int ret;
	struct i2c_client *client = smb350_dev->client;

	ret = smb350_read_reg(client, addr);
	if (ret < 0)
		return ret;

	*val = ret;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(reg_fops, smb350_get_reg, smb350_set_reg, "0x%02llx\n");

static int smb350_create_debugfs_entries(struct smb350_device *dev)
{
	int i;
	dev->dent = debugfs_create_dir(SMB350_NAME, NULL);
	if (IS_ERR(dev->dent)) {
		pr_err("smb350 driver couldn't create debugfs dir\n");
		return -EFAULT;
	}

	for (i = 0 ; i < ARRAY_SIZE(smb350_debug_regs) ; i++) {
		char *name = smb350_debug_regs[i].name;
		u32 reg = smb350_debug_regs[i].reg;
		struct dentry *file;

		file = debugfs_create_file(name, 0644, dev->dent,
					(void *) reg, &reg_fops);
		if (IS_ERR(file)) {
			pr_err("debugfs_create_file %s failed.\n", name);
			return -EFAULT;
		}
	}

	return 0;
}

static int smb350_set_volatile_params(struct smb350_device *dev)
{
	int ret;
	struct i2c_client *client = dev->client;

	pr_debug("\n");

	ret = smb350_write_reg(client, CMD_A_REG, CMD_A_VOLATILE_WR_PERM);
	if (ret) {
		pr_err("Failed to set VOLATILE_WR_PERM ret=%d\n", ret);
		return ret;
	}

	/* Disable SMB350 pulse-IRQ mechanism,
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

	/* Disable Automatic Recharge */
	smb350_masked_write(client, CHG_CTRL_REG, BIT(7), 1);

	/* Set fast-charge current */
	ret = smb350_set_chg_current(client, dev->chg_current_ma);
	if (ret) {
		pr_err("Failed to set FAST_CHG_CURRENT ret=%d\n", ret);
		return ret;
	}

	if (dev->term_current_ma > 0) {
		/* Enable Current Termination */
		smb350_masked_write(client, CHG_CTRL_REG, BIT(6), 0);

		/* Set Termination current */
		smb350_set_term_current(client, dev->term_current_ma);
	} else {
		/* Disable Current Termination */
		smb350_masked_write(client, CHG_CTRL_REG, BIT(6), 1);
	}

	return 0;
}

static int __devinit smb350_register_psy(struct smb350_device *dev)
{
	int ret;

	dev->dc_psy.name = "dc";
	dev->dc_psy.type = POWER_SUPPLY_TYPE_MAINS;
	dev->dc_psy.supplied_to = pm_power_supplied_to;
	dev->dc_psy.num_supplicants = ARRAY_SIZE(pm_power_supplied_to);
	dev->dc_psy.properties = pm_power_props;
	dev->dc_psy.num_properties = ARRAY_SIZE(pm_power_props);
	dev->dc_psy.get_property = smb350_get_property;
	dev->dc_psy.set_property = smb350_set_property;

	ret = power_supply_register(&dev->client->dev,
				&dev->dc_psy);
	if (ret) {
		pr_err("failed to register power_supply. ret=%d.\n", ret);
		return ret;
	}

	return 0;
}

static int __devinit smb350_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	int ret = 0;
	const struct smb350_platform_data *pdata;
	struct device_node *dev_node = client->dev.of_node;
	struct smb350_device *dev;
	u8 version;

	/* STAT pin change on start/stop charging */
	u32 irq_flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("i2c func fail.\n");
		return -EIO;
	}

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		pr_err("alloc fail.\n");
		return -ENOMEM;
	}

	smb350_dev = dev;
	dev->client = client;

	if (dev_node) {
		dev->chg_en_n_gpio =
			of_get_named_gpio(dev_node, "summit,chg-en-n-gpio", 0);
		pr_debug("chg_en_n_gpio = %d.\n", dev->chg_en_n_gpio);

		dev->chg_susp_n_gpio =
			of_get_named_gpio(dev_node,
					  "summit,chg-susp-n-gpio", 0);
		pr_debug("chg_susp_n_gpio = %d.\n", dev->chg_susp_n_gpio);

		dev->stat_gpio =
			of_get_named_gpio(dev_node, "summit,stat-gpio", 0);
		pr_debug("stat_gpio = %d.\n", dev->stat_gpio);

		ret = of_property_read_u32(dev_node, "summit,chg-current-ma",
					   &(dev->chg_current_ma));
		pr_debug("chg_current_ma = %d.\n", dev->chg_current_ma);
		if (ret) {
			pr_err("Unable to read chg_current.\n");
			return ret;
		}
		ret = of_property_read_u32(dev_node, "summit,term-current-ma",
					   &(dev->term_current_ma));
		pr_debug("term_current_ma = %d.\n", dev->term_current_ma);
		if (ret) {
			pr_err("Unable to read term_current_ma.\n");
			return ret;
		}
	} else {
		pdata = client->dev.platform_data;

		if (pdata == NULL) {
			pr_err("no platform data.\n");
			return -EINVAL;
		}

		dev->chg_en_n_gpio = pdata->chg_en_n_gpio;
		dev->chg_susp_n_gpio = pdata->chg_susp_n_gpio;
		dev->stat_gpio = pdata->stat_gpio;

		dev->chg_current_ma = pdata->chg_current_ma;
		dev->term_current_ma = pdata->term_current_ma;
	}

	ret = gpio_request(dev->stat_gpio, "smb350_stat");
	if (ret) {
		pr_err("gpio_request failed for %d ret=%d\n",
		       dev->stat_gpio, ret);
		goto err_stat_gpio;
	}
	dev->irq = gpio_to_irq(dev->stat_gpio);
	pr_debug("irq#=%d.\n", dev->irq);

	ret = gpio_request(dev->chg_susp_n_gpio, "smb350_suspend");
	if (ret) {
		pr_err("gpio_request failed for %d ret=%d\n",
			dev->chg_susp_n_gpio, ret);
		goto err_susp_gpio;
	}

	ret = gpio_request(dev->chg_en_n_gpio, "smb350_charger_enable");
	if (ret) {
		pr_err("gpio_request failed for %d ret=%d\n",
			dev->chg_en_n_gpio, ret);
		goto err_en_gpio;
	}

	i2c_set_clientdata(client, dev);

	/* Disable battery charging by default on power up.
	 * Battery charging is enabled by BMS or Battery-Gauge
	 * by using the set_property callback.
	 */
	smb350_enable_charging(dev, false);
	msleep(100);
	gpio_set_value_cansleep(dev->chg_susp_n_gpio, 1); /* Normal */
	msleep(100); /* Allow the device to exist shutdown */

	/* I2C transaction allowed only after device exit suspend */
	ret = smb350_read_reg(client, I2C_SLAVE_ADDR_REG);
	if ((ret>>1) != client->addr) {
		pr_err("No device.\n");
		ret = -ENODEV;
		goto err_no_dev;
	}

	version = smb350_read_reg(client, HW_VERSION_REG);
	version &= 0x0F; /* bits 0..3 */

	ret = smb350_set_volatile_params(dev);
	if (ret)
		goto err_set_params;

	ret = smb350_register_psy(dev);
	if (ret)
		goto err_set_params;

	ret = smb350_create_debugfs_entries(dev);
	if (ret)
		goto err_debugfs;

	INIT_DELAYED_WORK(&dev->irq_work, smb350_irq_worker);
	wake_lock_init(&dev->chg_wake_lock,
		       WAKE_LOCK_SUSPEND, SMB350_NAME);

	ret = request_irq(dev->irq, smb350_irq, irq_flags,
			  "smb350_irq", dev);
	if (ret) {
		pr_err("request_irq %d failed.ret=%d\n", dev->irq, ret);
		goto err_irq;
	}

	pr_info("HW Version = 0x%X.\n", version);

	return 0;

err_irq:
err_debugfs:
	if (dev->dent)
		debugfs_remove_recursive(dev->dent);
err_no_dev:
err_set_params:
	gpio_free(dev->chg_en_n_gpio);
err_en_gpio:
	gpio_free(dev->chg_susp_n_gpio);
err_susp_gpio:
	gpio_free(dev->stat_gpio);
err_stat_gpio:
	kfree(smb350_dev);
	smb350_dev = NULL;

	pr_info("FAIL.\n");

	return ret;
}

static int __devexit smb350_remove(struct i2c_client *client)
{
	struct smb350_device *dev = i2c_get_clientdata(client);

	power_supply_unregister(&dev->dc_psy);
	gpio_free(dev->chg_en_n_gpio);
	gpio_free(dev->chg_susp_n_gpio);
	if (dev->stat_gpio)
		gpio_free(dev->stat_gpio);
	if (dev->irq)
		free_irq(dev->irq, dev);
	if (dev->dent)
		debugfs_remove_recursive(dev->dent);
	kfree(smb350_dev);
	smb350_dev = NULL;

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
	.remove		= __devexit_p(smb350_remove),
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
