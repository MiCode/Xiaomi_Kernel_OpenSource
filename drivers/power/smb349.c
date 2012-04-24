/* Copyright (c) 2012 Code Aurora Forum. All rights reserved.
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
#include <linux/i2c/smb349.h>
#include <linux/power_supply.h>

#define SMB349_MASK(BITS, POS)  ((unsigned char)(((1 << BITS) - 1) << POS))

/* Register definitions */
#define CHG_CURRENT_REG			0x00
#define CHG_OTHER_CURRENT_REG	0x01
#define VAR_FUNC_REG			0x02
#define FLOAT_VOLTAGE_REG		0x03
#define CHG_CTRL_REG			0x04
#define STAT_TIMER_REG			0x05
#define PIN_ENABLE_CTRL_REG		0x06
#define THERM_CTRL_A_REG		0x07
#define SYSOK_USB3_SELECT_REG	0x08
#define CTRL_FUNCTIONS_REG		0x09
#define OTG_TLIM_THERM_CNTRL_REG	0x0A
#define HARD_SOFT_LIMIT_CELL_TEMP_MONITOR_REG 0x0B
#define FAULT_IRQ_REG	0x0C
#define STATUS_IRQ_REG	0x0D
#define SYSOK_REG		0x0E
#define CMD_A_REG		0x30
#define CMD_B_REG		0x31
#define CMD_C_REG		0x33
#define IRQ_A_REG		0x35
#define IRQ_B_REG		0x36
#define IRQ_C_REG		0x37
#define IRQ_D_REG		0x38
#define IRQ_E_REG		0x39
#define IRQ_F_REG		0x3A
#define STATUS_A_REG	0x3B
#define STATUS_B_REG	0x3C
#define STATUS_C_REG	0x3D
#define STATUS_D_REG	0x3E
#define STATUS_E_REG	0x3F

/* Status bits and masks */
#define CHG_STATUS_MASK		SMB349_MASK(2, 1)
#define CHG_ENABLE_STATUS_BIT		BIT(0)

/* Control bits and masks */
#define FAST_CHG_CURRENT_MASK			SMB349_MASK(4, 4)
#define AC_INPUT_CURRENT_LIMIT_MASK		SMB349_MASK(4, 0)
#define PRE_CHG_CURRENT_MASK			SMB349_MASK(3, 5)
#define TERMINATION_CURRENT_MASK		SMB349_MASK(3, 2)
#define PRE_CHG_TO_FAST_CHG_THRESH_MASK	SMB349_MASK(2, 6)
#define FLOAT_VOLTAGE_MASK				SMB349_MASK(6, 0)
#define CHG_ENABLE_BIT			BIT(1)
#define VOLATILE_W_PERM_BIT		BIT(7)
#define USB_SELECTION_BIT		BIT(1)
#define SYSTEM_FET_ENABLE_BIT	BIT(7)
#define AUTOMATIC_INPUT_CURR_LIMIT_BIT			BIT(4)
#define AUTOMATIC_POWER_SOURCE_DETECTION_BIT	BIT(2)
#define BATT_OV_END_CHG_BIT		BIT(1)
#define VCHG_FUNCTION			BIT(0)
#define CURR_TERM_END_CHG_BIT	BIT(6)

struct smb349_struct {
	struct			i2c_client *client;
	bool			charging;
	bool			present;
	int			chg_current_ma;

	int			en_n_gpio;
	int			chg_susp_gpio;
	struct dentry			*dent;
	spinlock_t			lock;
	struct work_struct			hwinit_work;

	struct power_supply			dc_psy;
};

struct chg_ma_limit_entry {
	int fast_chg_ma_limit;
	int ac_input_ma_limit;
	u8  chg_current_value;
};

static struct smb349_struct *the_smb349_chg;

static int smb349_read_reg(struct i2c_client *client, int reg,
				u8 *val)
{
	s32 ret;
	struct smb349_struct *smb349_chg;

	smb349_chg = i2c_get_clientdata(client);
	ret = i2c_smbus_read_byte_data(smb349_chg->client, reg);
	if (ret < 0) {
		dev_err(&smb349_chg->client->dev,
			"i2c read fail: can't read from %02x: %d\n", reg, ret);
		return ret;
	} else {
		*val = ret;
	}

	return 0;
}

static int smb349_write_reg(struct i2c_client *client, int reg,
						u8 val)
{
	s32 ret;
	struct smb349_struct *smb349_chg;

	smb349_chg = i2c_get_clientdata(client);
	ret = i2c_smbus_write_byte_data(smb349_chg->client, reg, val);
	if (ret < 0) {
		dev_err(&smb349_chg->client->dev,
			"i2c write fail: can't write %02x to %02x: %d\n",
			val, reg, ret);
		return ret;
	}
	return 0;
}

static int smb349_masked_write(struct i2c_client *client, int reg,
		u8 mask, u8 val)
{
	s32 rc;
	u8 temp;

	rc = smb349_read_reg(client, reg, &temp);
	if (rc) {
		pr_err("smb349_read_reg failed: reg=%03X, rc=%d\n", reg, rc);
		return rc;
	}
	temp &= ~mask;
	temp |= val & mask;
	rc = smb349_write_reg(client, reg, temp);
	if (rc) {
		pr_err("smb349_write failed: reg=%03X, rc=%d\n", reg, rc);
		return rc;
	}
	return 0;
}

static enum power_supply_property pm_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
};

static char *pm_power_supplied_to[] = {
	"battery",
};

static int get_prop_charge_type(struct smb349_struct *smb349_chg)
{
	if (smb349_chg->charging)
		return POWER_SUPPLY_CHARGE_TYPE_FAST;

	return POWER_SUPPLY_CHARGE_TYPE_NONE;
}

static int pm_power_get_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	struct smb349_struct *smb349_chg = container_of(psy,
						struct smb349_struct,
						dc_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = smb349_chg->chg_current_ma;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = (int)smb349_chg->present;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = get_prop_charge_type(smb349_chg);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

#define SMB349_FAST_CHG_MIN_MA	1000
#define SMB349_FAST_CHG_STEP_MA	200
#define SMB349_FAST_CHG_MAX_MA	4000
#define SMB349_FAST_CHG_SHIFT	4
static int chg_current_set(struct smb349_struct *smb349_chg)
{
	u8 temp;

	if ((smb349_chg->chg_current_ma < SMB349_FAST_CHG_MIN_MA) ||
		(smb349_chg->chg_current_ma >  SMB349_FAST_CHG_MAX_MA)) {
		pr_err("bad mA=%d asked to set\n", smb349_chg->chg_current_ma);
		return -EINVAL;
	}

	temp = (smb349_chg->chg_current_ma - SMB349_FAST_CHG_MIN_MA)
			/ SMB349_FAST_CHG_STEP_MA;

	temp = temp << SMB349_FAST_CHG_SHIFT;
	pr_debug("fastchg limit=%d setting %02x\n",
				smb349_chg->chg_current_ma, temp);
	return smb349_masked_write(smb349_chg->client, CHG_CURRENT_REG,
			FAST_CHG_CURRENT_MASK, temp);
}

static int set_reg(void *data, u64 val)
{
	int addr = (int)data;
	int ret;
	u8 temp;

	temp = (u16) val;
	ret = smb349_write_reg(the_smb349_chg->client, addr, temp);

	if (ret) {
		pr_err("smb349_write_reg to %x value =%d errored = %d\n",
			addr, temp, ret);
		return -EAGAIN;
	}
	return 0;
}
static int get_reg(void *data, u64 *val)
{
	int addr = (int)data;
	int ret;
	u8 temp;

	ret = smb349_read_reg(the_smb349_chg->client, addr, &temp);
	if (ret) {
		pr_err("smb349_read_reg to %x value =%d errored = %d\n",
			addr, temp, ret);
		return -EAGAIN;
	}

	*val = temp;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(reg_fops, get_reg, set_reg, "0x%02llx\n");

static void create_debugfs_entries(struct smb349_struct *smb349_chg)
{
	struct dentry *file;
	smb349_chg->dent = debugfs_create_dir(SMB349_NAME, NULL);
	if (IS_ERR(smb349_chg->dent)) {
		pr_err("smb349 driver couldn't create debugfs dir\n");
		return;
	}

	file = debugfs_create_file("CHG_CURRENT_REG", 0644, smb349_chg->dent,
				(void *) CHG_CURRENT_REG, &reg_fops);
	if (IS_ERR(file)) {
		pr_err("smb349 driver couldn't create debugfs files\n");
		return;
	}
	file = debugfs_create_file("CHG_OTHER_CURRENT_REG", 0644,
		smb349_chg->dent, (void *) CHG_OTHER_CURRENT_REG, &reg_fops);
	if (IS_ERR(file)) {
		pr_err("smb349 driver couldn't create debugfs files\n");
		return;
	}
	file = debugfs_create_file("VAR_FUNC_REG", 0644, smb349_chg->dent,
				(void *) VAR_FUNC_REG, &reg_fops);
	if (IS_ERR(file)) {
		pr_err("smb349 driver couldn't create debugfs files\n");
		return;
	}
	file = debugfs_create_file("FLOAT_VOLTAGE_REG", 0644, smb349_chg->dent,
				(void *) FLOAT_VOLTAGE_REG, &reg_fops);
	if (IS_ERR(file)) {
		pr_err("smb349 driver couldn't create debugfs files\n");
		return;
	}
	file = debugfs_create_file("CHG_CTRL_REG", 0644, smb349_chg->dent,
				(void *) CHG_CTRL_REG, &reg_fops);
	if (IS_ERR(file)) {
		pr_err("smb349 driver couldn't create debugfs files\n");
		return;
	}
	file = debugfs_create_file("STAT_TIMER_REG", 0644, smb349_chg->dent,
				(void *) STAT_TIMER_REG, &reg_fops);
	if (IS_ERR(file)) {
		pr_err("smb349 driver couldn't create debugfs files\n");
		return;
	}
	file = debugfs_create_file("PIN_ENABLE_CTRL_REG", 0644,
		smb349_chg->dent, (void *) PIN_ENABLE_CTRL_REG, &reg_fops);
	if (IS_ERR(file)) {
		pr_err("smb349 driver couldn't create debugfs files\n");
		return;
	}
	file = debugfs_create_file("PIN_ENABLE_CTRL_REG", 0644,
		smb349_chg->dent, (void *) PIN_ENABLE_CTRL_REG, &reg_fops);
	if (IS_ERR(file)) {
		pr_err("smb349 driver couldn't create debugfs files\n");
		return;
	}
	file = debugfs_create_file("PIN_ENABLE_CTRL_REG", 0644,
		smb349_chg->dent, (void *) PIN_ENABLE_CTRL_REG, &reg_fops);
	if (IS_ERR(file)) {
		pr_err("smb349 driver couldn't create debugfs files\n");
		return;
	}
	file = debugfs_create_file("THERM_CTRL_A_REG", 0644, smb349_chg->dent,
				(void *) THERM_CTRL_A_REG, &reg_fops);
	if (IS_ERR(file)) {
		pr_err("smb349 driver couldn't create debugfs files\n");
		return;
	}
	file = debugfs_create_file("SYSOK_USB3_SELECT_REG", 0644,
		smb349_chg->dent, (void *) SYSOK_USB3_SELECT_REG, &reg_fops);
	if (IS_ERR(file)) {
		pr_err("smb349 driver couldn't create debugfs files\n");
		return;
	}
	file = debugfs_create_file("CTRL_FUNCTIONS_REG", 0644,
		smb349_chg->dent, (void *) CTRL_FUNCTIONS_REG, &reg_fops);
	if (IS_ERR(file)) {
		pr_err("smb349 driver couldn't create debugfs files\n");
		return;
	}
	file = debugfs_create_file("OTG_TLIM_THERM_CNTRL_REG", 0644,
		smb349_chg->dent, (void *) OTG_TLIM_THERM_CNTRL_REG, &reg_fops);
	if (IS_ERR(file)) {
		pr_err("smb349 driver couldn't create debugfs files\n");
		return;
	}
	file = debugfs_create_file("HARD_SOFT_LIMIT_CELL_TEMP_MONITOR_REG",
		0644, smb349_chg->dent,
		(void *) HARD_SOFT_LIMIT_CELL_TEMP_MONITOR_REG, &reg_fops);
	if (IS_ERR(file)) {
		pr_err("smb349 driver couldn't create debugfs files\n");
		return;
	}
	file = debugfs_create_file("SYSOK_REG", 0644, smb349_chg->dent,
				(void *) SYSOK_REG, &reg_fops);
	if (IS_ERR(file)) {
		pr_err("smb349 driver couldn't create debugfs files\n");
		return;
	}
	file = debugfs_create_file("CMD_A_REG", 0644, smb349_chg->dent,
				(void *) CMD_A_REG, &reg_fops);
	if (IS_ERR(file)) {
		pr_err("smb349 driver couldn't create debugfs files\n");
		return;
	}
	file = debugfs_create_file("CMD_B_REG", 0644, smb349_chg->dent,
				(void *) CMD_B_REG, &reg_fops);
	if (IS_ERR(file)) {
		pr_err("smb349 driver couldn't create debugfs files\n");
		return;
	}
	file = debugfs_create_file("CMD_C_REG", 0644, smb349_chg->dent,
				(void *) CMD_C_REG, &reg_fops);
	if (IS_ERR(file)) {
		pr_err("smb349 driver couldn't create debugfs files\n");
		return;
	}
	file = debugfs_create_file("STATUS_A_REG", 0644, smb349_chg->dent,
				(void *) STATUS_A_REG, &reg_fops);
	if (IS_ERR(file)) {
		pr_err("smb349 driver couldn't create debugfs files\n");
		return;
	}
	file = debugfs_create_file("STATUS_B_REG", 0644, smb349_chg->dent,
				(void *) STATUS_B_REG, &reg_fops);
	if (IS_ERR(file)) {
		pr_err("smb349 driver couldn't create debugfs files\n");
		return;
	}
	file = debugfs_create_file("STATUS_C_REG", 0644, smb349_chg->dent,
				(void *) STATUS_C_REG, &reg_fops);
	if (IS_ERR(file)) {
		pr_err("smb349 driver couldn't create debugfs files\n");
		return;
	}
	file = debugfs_create_file("STATUS_D_REG", 0644, smb349_chg->dent,
				(void *) STATUS_D_REG, &reg_fops);
	if (IS_ERR(file)) {
		pr_err("smb349 driver couldn't create debugfs files\n");
		return;
	}
	file = debugfs_create_file("STATUS_E_REG", 0644, smb349_chg->dent,
				(void *) STATUS_E_REG, &reg_fops);
	if (IS_ERR(file)) {
		pr_err("smb349 driver couldn't create debugfs files\n");
		return;
	}
}

static void remove_debugfs_entries(struct smb349_struct *smb349_chg)
{
	if (smb349_chg->dent)
		debugfs_remove_recursive(smb349_chg->dent);
}

static int smb349_hwinit(struct smb349_struct *smb349_chg)
{
	int ret;

	ret = smb349_write_reg(smb349_chg->client, CMD_A_REG,
			VOLATILE_W_PERM_BIT);
	if (ret) {
		pr_err("Failed to set VOLATILE_W_PERM_BIT rc=%d\n", ret);
		return ret;
	}

	ret = smb349_masked_write(smb349_chg->client, CHG_CTRL_REG,
				CURR_TERM_END_CHG_BIT, CURR_TERM_END_CHG_BIT);
	if (ret) {
		pr_err("Failed to set CURR_TERM_END_CHG_BIT rc=%d\n", ret);
		return ret;
	}

	ret = chg_current_set(smb349_chg);
	if (ret) {
		pr_err("Failed to set FAST_CHG_CURRENT rc=%d\n", ret);
		return ret;
	}

	return 0;
}

static int smb349_stop_charging(struct smb349_struct *smb349_chg)
{
	unsigned long flags;

	if (smb349_chg->charging)
		gpio_set_value_cansleep(smb349_chg->en_n_gpio, 0);

	spin_lock_irqsave(&smb349_chg->lock, flags);
	pr_debug("stop charging %d\n", smb349_chg->charging);
	smb349_chg->charging = 0;
	spin_unlock_irqrestore(&smb349_chg->lock, flags);
	power_supply_changed(&smb349_chg->dc_psy);
	return 0;
}

static int smb349_start_charging(struct smb349_struct *smb349_chg)
{
	unsigned long flags;
	int rc;

	rc = 0;
	if (!smb349_chg->charging) {
		gpio_set_value_cansleep(smb349_chg->en_n_gpio, 1);
		/*
		 * Write non-default values, charger chip reloads from
		 * non-volatile memory if it was in suspend mode
		 *
		 */
		rc = schedule_work(&smb349_chg->hwinit_work);
	}

	spin_lock_irqsave(&smb349_chg->lock, flags);
	pr_debug("start charging %d\n", smb349_chg->charging);
	smb349_chg->charging = 1;
	spin_unlock_irqrestore(&smb349_chg->lock, flags);
	power_supply_changed(&smb349_chg->dc_psy);
	return rc;
}

static int pm_power_set_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	struct smb349_struct *smb349_chg = container_of(psy,
						struct smb349_struct,
						dc_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (val->intval) {
			smb349_chg->present = val->intval;
		} else {
			smb349_chg->present = 0;
			if (smb349_chg->charging)
				return smb349_stop_charging(smb349_chg);
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (val->intval) {
			if (smb349_chg->chg_current_ma != val->intval)
				return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (val->intval && smb349_chg->present) {
			if (val->intval == POWER_SUPPLY_CHARGE_TYPE_FAST)
				return smb349_start_charging(smb349_chg);
			if (val->intval == POWER_SUPPLY_CHARGE_TYPE_NONE)
				return smb349_stop_charging(smb349_chg);
		} else {
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	power_supply_changed(&smb349_chg->dc_psy);
	return 0;
}

static void hwinit_worker(struct work_struct *work)
{
	int ret;
	struct smb349_struct *smb349_chg = container_of(work,
				struct smb349_struct, hwinit_work);

	ret = smb349_hwinit(smb349_chg);
	if (ret)
		pr_err("Failed to re-initilaze registers\n");
}

static int __devinit smb349_init_ext_chg(struct smb349_struct *smb349_chg)
{
	int ret;

	smb349_chg->dc_psy.name = "dc";
	smb349_chg->dc_psy.type = POWER_SUPPLY_TYPE_MAINS;
	smb349_chg->dc_psy.supplied_to = pm_power_supplied_to;
	smb349_chg->dc_psy.num_supplicants = ARRAY_SIZE(pm_power_supplied_to);
	smb349_chg->dc_psy.properties = pm_power_props;
	smb349_chg->dc_psy.num_properties = ARRAY_SIZE(pm_power_props);
	smb349_chg->dc_psy.get_property = pm_power_get_property;
	smb349_chg->dc_psy.set_property = pm_power_set_property;

	ret = power_supply_register(&smb349_chg->client->dev,
				&smb349_chg->dc_psy);
	if (ret) {
		pr_err("failed to register power_supply. ret=%d.\n", ret);
		return ret;
	}

	return 0;
}

static int __devinit smb349_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	const struct smb349_platform_data *pdata;
	struct smb349_struct *smb349_chg;
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

	smb349_chg = kzalloc(sizeof(*smb349_chg), GFP_KERNEL);
	if (!smb349_chg) {
		ret = -ENOMEM;
		goto out;
	}

	smb349_chg->client = client;
	smb349_chg->chg_current_ma = pdata->chg_current_ma;
	ret = gpio_request(pdata->chg_susp_gpio, "smb349_suspend");
	if (ret) {
		dev_err(&client->dev, "%s gpio_request failed for %d ret=%d\n",
			__func__, pdata->chg_susp_gpio, ret);
		goto free_smb349_chg;
	}
	smb349_chg->chg_susp_gpio = pdata->chg_susp_gpio;

	ret = gpio_request(pdata->en_n_gpio, "smb349_charger_enable");
	if (ret) {
		dev_err(&client->dev, "%s gpio_request failed for %d ret=%d\n",
			__func__, pdata->en_n_gpio, ret);
		goto chg_susp_gpio_fail;
	}
	smb349_chg->en_n_gpio = pdata->en_n_gpio;

	i2c_set_clientdata(client, smb349_chg);

	ret = smb349_hwinit(smb349_chg);
	if (ret)
		goto free_smb349_chg;

	ret = smb349_init_ext_chg(smb349_chg);
	if (ret)
		goto chg_en_gpio_fail;

	the_smb349_chg = smb349_chg;

	create_debugfs_entries(smb349_chg);
	INIT_WORK(&smb349_chg->hwinit_work, hwinit_worker);

	pr_info("OK connector present = %d\n", smb349_chg->present);
	return 0;

chg_en_gpio_fail:
	gpio_free(smb349_chg->en_n_gpio);
chg_susp_gpio_fail:
	gpio_free(smb349_chg->chg_susp_gpio);
free_smb349_chg:
	kfree(smb349_chg);
out:
	return ret;
}

static int __devexit smb349_remove(struct i2c_client *client)
{
	const struct smb349_platform_data *pdata;
	struct smb349_struct *smb349_chg = i2c_get_clientdata(client);

	flush_work(&smb349_chg->hwinit_work);
	pdata = client->dev.platform_data;
	power_supply_unregister(&smb349_chg->dc_psy);
	gpio_free(pdata->en_n_gpio);
	gpio_free(pdata->chg_susp_gpio);
	remove_debugfs_entries(smb349_chg);
	kfree(smb349_chg);
	return 0;
}

static int smb349_suspend(struct device *dev)
{
	struct smb349_struct *smb349_chg = dev_get_drvdata(dev);

	pr_debug("suspend\n");
	if (smb349_chg->charging)
		return -EBUSY;
	return 0;
}

static int smb349_resume(struct device *dev)
{
	pr_debug("resume\n");

	return 0;
}

static const struct dev_pm_ops smb349_pm_ops = {
	.suspend	= smb349_suspend,
	.resume		= smb349_resume,
};

static const struct i2c_device_id smb349_id[] = {
	{SMB349_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, smb349_id);

static struct i2c_driver smb349_driver = {
	.driver	= {
		   .name	= SMB349_NAME,
		   .owner	= THIS_MODULE,
		   .pm		= &smb349_pm_ops,
	},
	.probe		= smb349_probe,
	.remove		= __devexit_p(smb349_remove),
	.id_table	= smb349_id,
};

static int __init smb349_init(void)
{
	return i2c_add_driver(&smb349_driver);
}
module_init(smb349_init);

static void __exit smb349_exit(void)
{
	return i2c_del_driver(&smb349_driver);
}
module_exit(smb349_exit);

MODULE_DESCRIPTION("Driver for SMB349 charger chip");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:" SMB349_NAME);
