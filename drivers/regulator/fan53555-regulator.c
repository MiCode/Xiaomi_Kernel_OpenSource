/*
 * driver/regultor/fan53555-regulator.c
 *
 * Driver for FAN53555UC00X, FAN53555UC01X, FAN53555UC03X,
 *            FAN53555UC04X, FAN53555UC05X
 *
 * Copyright (c) 2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fan53555-regulator.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>

/* Register definitions */
#define FAN53555_REG_VSEL0		0
#define FAN53555_REG_VSEL1		1
#define FAN53555_REG_CONTROL		2
#define FAN53555_REG_ID1		3
#define FAN53555_REG_ID2		4
#define FAN53555_REG_MONITOR		5

#define FAN53555_VSEL_BUCK_EN		BIT(7)
#define FAN53555_VSEL_MODE		BIT(6)
#define FAN53555_VSEL_NSEL_SHIFT	0
#define FAN53555_VSEL_NSEL_MASK		0x3F

#define FAN53555_CONTROL_DISCHARGE	BIT(7)
#define FAN53555_CONTROL_SLEW_SHIFT	4
#define FAN53555_CONTROL_SLEW_MASK	0x70
#define FAN53555_CONTROL_RESET		BIT(2)

#define FAN53555_ID1_VENDOR_SHIFT	4
#define FAN53555_ID1_VENDOR_MASK	0xF0
#define FAN53555_ID1_DIE_ID_SHIFT	0
#define FAN53555_ID1_DIE_ID_MASK	0x0F

#define FAN53555_ID2_REV_SHIFT		0
#define FAN53555_ID2_REV_MASK		0x0F

#define FAN53555_MONITOR_ILIM		BIT(7)
#define FAN53555_MONITOR_UVLO		BIT(6)
#define FAN53555_MONITOR_OVP		BIT(5)
#define FAN53555_MONITOR_POS		BIT(4)
#define FAN53555_MONITOR_NEG		BIT(3)
#define FAN53555_MONITOR_RESET_STAT	BIT(2)
#define FAN53555_MONITOR_OT		BIT(1)
#define FAN53555_MONITOR_BUCK_STATUS	BIT(0)

#define FAN53555_VSEL0_ID		0
#define FAN53555_VSEL1_ID		1

#define FAN53555UC00X_ID		0x80
#define FAN53555UC01X_ID		0x81
#define FAN53555UC03X_ID		0x83
#define FAN53555UC04X_ID		0x84
#define FAN53555UC05X_ID		0x85

#define FAN53555_N_VOLTAGES		64

/* FAN53555 chip information */
struct fan53555_chip {
	const char *name;
	struct device *dev;
	struct regulator_desc desc;
	struct i2c_client *client;
	struct regulator_dev *rdev;
	struct mutex io_lock;
	int chip_id;
	int vsel_id;
	u8 shadow[6];
};

#define FAN53555_VOLTAGE(chip_id, vsel)				\
	(((chip_id) == FAN53555UC04X_ID) ?			\
	 ((vsel) * 12826 + 600000) : ((vsel) * 10000 + 600000))

static int fan53555_read(struct fan53555_chip *fan, u8 reg)
{
	u8 data;
	u8 val;
	int ret;

	data = reg;

	ret = i2c_master_send(fan->client, &data, 1);
	if (ret < 0)
		goto out;

	ret = i2c_master_recv(fan->client, &val, 1);
	if (ret < 0)
		goto out;

	ret = val;
out:
	return ret;
}

static inline int fan53555_write(struct fan53555_chip *fan, u8 reg, u8 val)
{
	u8 msg[2];
	int ret;

	msg[0] = reg;
	msg[1] = val;

	ret = i2c_master_send(fan->client, msg, 2);
	if (ret < 0)
		return ret;
	if (ret != 2)
		return -EIO;
	return 0;
}

static int fan53555_read_reg(struct fan53555_chip *fan, u8 reg)
{
	int data;

	mutex_lock(&fan->io_lock);
	data = fan53555_read(fan, reg);
	if (data < 0)
		dev_err(fan->dev, "Read from reg 0x%x failed\n", reg);
	mutex_unlock(&fan->io_lock);

	return data;
}

static int fan53555_set_bits(struct fan53555_chip *fan, u8 reg, u8 mask, u8 val)
{
	int err;
	u8 data;

	mutex_lock(&fan->io_lock);
	data = fan->shadow[reg];
	data &= ~mask;
	val &= mask;
	data |= val;
	err = fan53555_write(fan, reg, data);
	if (err)
		dev_err(fan->dev, "write for reg 0x%x failed\n", reg);
	else
		fan->shadow[reg] = data;
	mutex_unlock(&fan->io_lock);

	return err;
}

static int __fan53555_dcdc_set_voltage(struct fan53555_chip *fan,
				       int vsel_id, int min_uV, int max_uV,
				       unsigned *selector)
{
	int nsel;
	int uV;
	int chip_id;
	int n_voltages;

	chip_id = fan->chip_id;
	n_voltages = fan->desc.n_voltages;

	if (max_uV < min_uV) {
		dev_err(fan->dev, "max_uV(%d) < min_uV(%d)\n", max_uV, min_uV);
		return -EINVAL;
	}
	if (min_uV > FAN53555_VOLTAGE(chip_id, n_voltages - 1)) {
		dev_err(fan->dev, "min_uV(%d) > %d[uV]\n",
			min_uV, FAN53555_VOLTAGE(chip_id, n_voltages - 1));
		return -EINVAL;
	}
	if (max_uV < FAN53555_VOLTAGE(chip_id, 0)) {
		dev_err(fan->dev, "max_uV(%d) < %d[uV]\n",
			max_uV, FAN53555_VOLTAGE(chip_id, 0));
		return -EINVAL;
	}
	if ((vsel_id != FAN53555_VSEL0_ID) && (vsel_id != FAN53555_VSEL1_ID)) {
		dev_err(fan->dev,
			"%d is not valid VSEL register ID\n", vsel_id);
		return -EINVAL;
	}
	for (nsel = 0; nsel < n_voltages; nsel++) {
		uV = FAN53555_VOLTAGE(chip_id, nsel);
		if (min_uV <= uV && uV <= max_uV) {
			if (selector)
				*selector = nsel;
			return fan53555_set_bits(fan,
						 FAN53555_REG_VSEL0 + vsel_id,
						 FAN53555_VSEL_NSEL_MASK,
						 nsel <<
						 FAN53555_VSEL_NSEL_SHIFT);
		}
	}

	return -EINVAL;
}

#ifdef CONFIG_DEBUG_FS

#include <linux/debugfs.h>
#include <linux/seq_file.h>

static int dbg_fan_show(struct seq_file *s, void *unused)
{
	struct fan53555_chip *fan = s->private;
	int val;

	seq_printf(s, "FAN53555 Registers\n");
	seq_printf(s, "------------------\n");

	val = fan53555_read_reg(fan, FAN53555_REG_VSEL0);
	if (val >= 0)
		seq_printf(s, "Reg VSEL0   Value 0x%02x\n", val);

	val = fan53555_read_reg(fan, FAN53555_REG_VSEL1);
	if (val >= 0)
		seq_printf(s, "Reg VSEL1   Value 0x%02x\n", val);

	val = fan53555_read_reg(fan, FAN53555_REG_CONTROL);
	if (val >= 0)
		seq_printf(s, "Reg CONTROL Value 0x%02x\n", val);

	val = fan53555_read_reg(fan, FAN53555_REG_ID1);
	if (val >= 0)
		seq_printf(s, "Reg ID1     Value 0x%02x\n", val);

	val = fan53555_read_reg(fan, FAN53555_REG_ID2);
	if (val >= 0)
		seq_printf(s, "Reg ID2     Value 0x%02x\n", val);

	val = fan53555_read_reg(fan, FAN53555_REG_MONITOR);
	if (val >= 0)
		seq_printf(s, "Reg MONITOR Value 0x%02x\n", val);

	return 0;
}

static int dbg_fan_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_fan_show, inode->i_private);
}

static const struct file_operations debug_fops = {
	.open = dbg_fan_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void __init fan53555_debuginit(struct fan53555_chip *fan)
{
	(void)debugfs_create_file("fan53555", S_IRUGO, NULL, fan, &debug_fops);
}
#else
static void __init fan53555_debuginit(struct fan53555_chip *fan)
{
}
#endif

static int fan53555_dcdc_init(struct fan53555_chip *fan,
			      struct i2c_client *client,
			      struct fan53555_regulator_platform_data *pdata)
{
	int err;
	int val;

	err = fan53555_read_reg(fan, FAN53555_REG_VSEL0);
	if (err < 0)
		return err;
	fan->shadow[FAN53555_REG_VSEL0] = (u8)err;

	err = fan53555_read_reg(fan, FAN53555_REG_VSEL1);
	if (err < 0)
		return err;
	fan->shadow[FAN53555_REG_VSEL1] = (u8)err;

	err = fan53555_read_reg(fan, FAN53555_REG_CONTROL);
	if (err < 0)
		return err;
	fan->shadow[FAN53555_REG_CONTROL] = (u8)err;

	err = __fan53555_dcdc_set_voltage(fan,
					  FAN53555_VSEL0_ID,
					  pdata->init_vsel0_min_uV,
					  pdata->init_vsel0_max_uV,
					  NULL);
	if (err < 0)
		return err;

	val = pdata->vsel0_buck_en ? FAN53555_VSEL_BUCK_EN : 0;
	val |= pdata->vsel0_mode ? FAN53555_VSEL_MODE : 0;
	err = fan53555_set_bits(fan,
				FAN53555_REG_VSEL0,
				FAN53555_VSEL_BUCK_EN | FAN53555_VSEL_MODE,
				val);
	if (err < 0)
		return err;

	err = __fan53555_dcdc_set_voltage(fan,
					  FAN53555_VSEL1_ID,
					  pdata->init_vsel1_min_uV,
					  pdata->init_vsel1_max_uV,
					  NULL);
	if (err < 0)
		return err;

	val = pdata->vsel1_buck_en ? FAN53555_VSEL_BUCK_EN : 0;
	val |= pdata->vsel1_mode ? FAN53555_VSEL_MODE : 0;
	err = fan53555_set_bits(fan,
				FAN53555_REG_VSEL1,
				FAN53555_VSEL_BUCK_EN | FAN53555_VSEL_MODE,
				val);
	if (err < 0)
		return err;

	val = pdata->slew_rate;
	val <<= FAN53555_CONTROL_SLEW_SHIFT;
	val |= pdata->output_discharge ? FAN53555_CONTROL_DISCHARGE : 0;
	err = fan53555_set_bits(fan,
				FAN53555_REG_CONTROL,
				FAN53555_CONTROL_DISCHARGE |
				FAN53555_CONTROL_SLEW_MASK, val);
	return err;
}

static int fan53555_dcdc_list_voltage(struct regulator_dev *dev,
				      unsigned selector)
{
	struct fan53555_chip *fan = rdev_get_drvdata(dev);

	if ((selector < 0) || (selector >= fan->desc.n_voltages))
		return -EINVAL;

	return FAN53555_VOLTAGE(fan->chip_id, selector);
}

static int fan53555_dcdc_set_voltage(struct regulator_dev *dev,
				     int min_uV, int max_uV,
				     unsigned *selector)
{
	struct fan53555_chip *fan = rdev_get_drvdata(dev);

	return __fan53555_dcdc_set_voltage(fan, fan->vsel_id, min_uV, max_uV,
					   selector);
}

static int fan53555_dcdc_get_voltage(struct regulator_dev *dev)
{
	struct fan53555_chip *fan = rdev_get_drvdata(dev);
	u8 data;

	if ((fan->vsel_id != FAN53555_VSEL0_ID) &&
	    (fan->vsel_id != FAN53555_VSEL1_ID)) {
		dev_err(fan->dev,
			"%d is not valid VSEL register ID\n", fan->vsel_id);
		return -EINVAL;
	}
	data = fan->shadow[FAN53555_REG_VSEL0 + fan->vsel_id];
	data &= FAN53555_VSEL_NSEL_MASK;
	data >>= FAN53555_VSEL_NSEL_SHIFT;

	return FAN53555_VOLTAGE(fan->chip_id, data);
}

static int fan53555_dcdc_enable(struct regulator_dev *dev)
{
	struct fan53555_chip *fan = rdev_get_drvdata(dev);

	if ((fan->vsel_id != FAN53555_VSEL0_ID) &&
	    (fan->vsel_id != FAN53555_VSEL1_ID)) {
		dev_err(fan->dev,
			"%d is not valid VSEL register ID\n", fan->vsel_id);
		return -EINVAL;
	}

	return fan53555_set_bits(fan,
				 FAN53555_REG_VSEL0 + fan->vsel_id,
				 FAN53555_VSEL_BUCK_EN, FAN53555_VSEL_BUCK_EN);
}

static int fan53555_dcdc_disable(struct regulator_dev *dev)
{
	struct fan53555_chip *fan = rdev_get_drvdata(dev);

	if ((fan->vsel_id != FAN53555_VSEL0_ID) &&
	    (fan->vsel_id != FAN53555_VSEL1_ID)) {
		dev_err(fan->dev,
			"%d is not valid VSEL register ID\n", fan->vsel_id);
		return -EINVAL;
	}

	return fan53555_set_bits(fan,
				 FAN53555_REG_VSEL0 + fan->vsel_id,
				 FAN53555_VSEL_BUCK_EN, 0);
}

static int fan53555_dcdc_is_enabled(struct regulator_dev *dev)
{
	struct fan53555_chip *fan = rdev_get_drvdata(dev);
	u8 data;

	if ((fan->vsel_id != FAN53555_VSEL0_ID) &&
	    (fan->vsel_id != FAN53555_VSEL1_ID)) {
		dev_err(fan->dev,
			"%d is not valid VSEL register ID\n", fan->vsel_id);
		return -EINVAL;
	}
	data = fan->shadow[FAN53555_REG_VSEL0 + fan->vsel_id];

	return (data & FAN53555_VSEL_BUCK_EN) ? 1 : 0;
}

static struct regulator_ops fan53555_dcdc_ops = {
	.list_voltage = fan53555_dcdc_list_voltage,
	.set_voltage = fan53555_dcdc_set_voltage,
	.get_voltage = fan53555_dcdc_get_voltage,
	.enable = fan53555_dcdc_enable,
	.disable = fan53555_dcdc_disable,
	.is_enabled = fan53555_dcdc_is_enabled,
};

static int __devinit fan53555_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	struct fan53555_regulator_platform_data *pdata;
	struct regulator_init_data *init_data;
	struct regulator_dev *rdev;
	struct fan53555_chip *fan;
	int chip_id;
	int err;

	pdata = client->dev.platform_data;
	if (!pdata) {
		dev_err(&client->dev, "Err: Platform data not found\n");
		return -EIO;
	}
	init_data = &pdata->reg_init_data;
	fan = kzalloc(sizeof(*fan), GFP_KERNEL);
	if (!fan) {
		dev_err(&client->dev, "Err: Memory allocation fails\n");
		return -ENOMEM;
	}
	mutex_init(&fan->io_lock);
	fan->client = client;
	fan->dev = &client->dev;
	fan->vsel_id = pdata->vsel_id;
	fan->name = id->name;
	fan->desc.name = id->name;
	fan->desc.id = 0;
	fan->desc.irq = 0;
	fan->desc.ops = &fan53555_dcdc_ops;
	fan->desc.type = REGULATOR_VOLTAGE;
	fan->desc.owner = THIS_MODULE;
	fan->desc.n_voltages = FAN53555_N_VOLTAGES;
	i2c_set_clientdata(client, fan);

	chip_id = fan53555_read_reg(fan, FAN53555_REG_ID1);
	if (chip_id < 0) {
		err = chip_id;
		dev_err(fan->dev, "Error in reading device %d\n", err);
		goto fail;
	}

	switch (chip_id) {
	case FAN53555UC00X_ID:
	case FAN53555UC01X_ID:
	case FAN53555UC03X_ID:
	case FAN53555UC04X_ID:
	case FAN53555UC05X_ID:
		fan->chip_id = chip_id;
		break;
	default:
		dev_err(fan->dev, "Err: not supported device chip id 0x%x",
			chip_id);
		err = -ENODEV;
		goto fail;
	}

	err = fan53555_dcdc_init(fan, client, pdata);
	if (err < 0) {
		dev_err(fan->dev, "FAN53555 init fails with %d\n", err);
		goto fail;
	}

	rdev = regulator_register(&fan->desc, &client->dev, init_data, fan);
	if (IS_ERR(rdev)) {
		dev_err(fan->dev, "Failed to register %s\n", id->name);
		err = PTR_ERR(rdev);
		goto fail;
	}
	fan->rdev = rdev;

	fan53555_debuginit(fan);
	return 0;

fail:
	kfree(fan);
	return err;
}

/**
 * fan53555_remove - fan53555 driver i2c remove handler
 * @client: i2c driver client device structure
 *
 * Unregister fan53555 driver as an i2c client device driver
 */
static int __devexit fan53555_remove(struct i2c_client *client)
{
	struct fan53555_chip *chip = i2c_get_clientdata(client);

	regulator_unregister(chip->rdev);
	kfree(chip);
	return 0;
}

static const struct i2c_device_id fan53555_id[] = {
	{.name = "fan53555", .driver_data = 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, fan53555_id);

static struct i2c_driver fan53555_i2c_driver = {
	.driver = {
		.name = "fan53555",
		.owner = THIS_MODULE,
	},
	.probe = fan53555_probe,
	.remove = __devexit_p(fan53555_remove),
	.id_table = fan53555_id,
};

/* Module init function */
static int __init fan53555_init(void)
{
	return i2c_add_driver(&fan53555_i2c_driver);
}
subsys_initcall_sync(fan53555_init);

/* Module exit function */
static void __exit fan53555_cleanup(void)
{
	i2c_del_driver(&fan53555_i2c_driver);
}
module_exit(fan53555_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jake Park<jakep@nvidia.com>");
MODULE_DESCRIPTION("Regulator Driver for Fairchild FAN53555 Regulator");
MODULE_ALIAS("platform:fan53555-regulator");
