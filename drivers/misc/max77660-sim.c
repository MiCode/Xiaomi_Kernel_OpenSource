/*
 * MAXIM MAX77660 SIM driver
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 * Author: Shawn joo <sjoo@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mfd/max77660/max77660-core.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/miscdevice.h>

struct max77660_sim {
	struct device		*parent;
	struct device		*dev;
	struct device		*miscdev;
	int sim_irq;
	bool sim1_insert;
	bool sim2_insert;
};

static ssize_t max77660_sim1_state_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *device = to_platform_device(dev);
	struct max77660_sim *sim = dev_get_platdata(&device->dev);

	return sprintf(buf, "%s\n", sim->sim1_insert ? "1" : "0");
}

static DEVICE_ATTR(sim1_inserted, S_IRUSR,
		max77660_sim1_state_read, NULL);

static ssize_t max77660_sim2_state_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *device = to_platform_device(dev);
	struct max77660_sim *sim = dev_get_platdata(&device->dev);


	return sprintf(buf, "%s\n", sim->sim2_insert ? "1" : "0");
}

static DEVICE_ATTR(sim2_inserted, S_IRUSR,
		max77660_sim2_state_read, NULL);

static struct attribute *sim_attrs[] = {
	&dev_attr_sim1_inserted.attr,
	&dev_attr_sim2_inserted.attr,
	NULL
};

static const struct attribute_group sim_attr_group = {
	.attrs = sim_attrs,
};


static irqreturn_t max77660_sim_irq(int irq, void *data)
{
	struct max77660_sim *sim = data;
	u8 val, val2;
	int ret, ret2;
	bool sim1 = sim->sim1_insert;
	bool sim2 = sim->sim2_insert;

	ret = max77660_reg_read(sim->parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_SIM1INT, &val);
	ret2 = max77660_reg_read(sim->parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_SIM1STAT, &val2);

	if (ret || ret2)
		dev_err(sim->dev, "sim1 reg read fails. ret(%d, %d)\n",
				ret, ret2);
	else if (val & BIT(0) && val2 & BIT(0))
		sim1 = true;
	else if (val & BIT(1) && !(val2 & BIT(0)))
		sim1 = false;


	ret = max77660_reg_read(sim->parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_SIM2INT, &val);
	ret2 = max77660_reg_read(sim->parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_SIM2STAT, &val2);

	if (ret || ret2)
		dev_err(sim->dev, "sim2 reg read fails. ret(%d, %d)\n",
				ret, ret2);
	if (val & BIT(0) && val2 & BIT(0))
		sim2 = true;
	else if (val & BIT(1) && !(val2 & BIT(0)))
		sim2 = false;

	if (sim1 != sim->sim1_insert) {
		sim->sim1_insert = sim1;
		sysfs_notify(&sim->miscdev->kobj, NULL, "sim1_inserted");
		dev_dbg(sim->dev, "sim1(%s)\n", sim1 ? "insert" : "removal");
	}
	if (sim2 != sim->sim2_insert) {
		sim->sim2_insert = sim2;
		sysfs_notify(&sim->miscdev->kobj, NULL, "sim2_inserted");
		dev_dbg(sim->dev, "sim2(%s)\n", sim2 ? "insert" : "removal");
	}

	return IRQ_HANDLED;
}

static struct miscdevice sim_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sim",
};

static int max77660_sim_probe(struct platform_device *pdev)
{
	struct max77660_platform_data *pdata;
	struct max77660_sim *sim;
	int ret, val;
	int sim_irq;
	struct max77660_sim_platform_data *sim_pdata;
	struct platform_device *misc_pdev;

	pdata = dev_get_platdata(pdev->dev.parent);
	if (!pdata) {
		dev_err(&pdev->dev, "Platform data not found\n");
		return -ENODEV;
	}

	sim_pdata = pdata->sim_pdata;

	sim_irq = platform_get_irq(pdev, 0);
	if (sim_irq <= 0) {
		dev_err(&pdev->dev, "sim interrupt is not available\n");
		return -ENODEV;
	}

	/* SIM interrupt mask */
	ret = max77660_reg_write(pdev->dev.parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_SIM1NTM, 0x00);
	if (ret < 0)
		goto err_reg_access;

	ret = max77660_reg_write(pdev->dev.parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_SIM2NTM, 0x00);
	if (ret < 0)
		goto err_reg_access;

	/* SIM1 config1 */
	val = sim_pdata->sim_reg[0].detect_en << SIM_SIM1_2_CNFG1_SIM_EN_SHIFT;
	val |= sim_pdata->sim_reg[0].batremove_en <<
		SIM_SIM1_2_CNFG1_BATREM_EN_SHIFT;
	val |= sim_pdata->sim_reg[0].det_debouncecnt <<
		SIM_SIM1_2_CNFG1_SIMDBCNT_SHIFT;
	ret = max77660_reg_write(pdev->dev.parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_SIM1CNFG1, val);
	if (ret < 0)
		goto err_reg_access;

	/* SIM1 config2 */
	val = sim_pdata->sim_reg[0].auto_pwrdn_en <<
		SIM_SIM1_2_CNFG1_SIM_PWRDEN_SHIFT;
	val |= sim_pdata->sim_reg[0].inst_pol <<
		SIM_SIM1_2_CNFG1_SIMAH_SHIFT;
	val |= sim_pdata->sim_reg[0].pwrdn_debouncecnt <<
		SIM_SIM1_2_CNFG1_SIMPWRDNCNT_SHIFT;
	val |= (sim_pdata->sim_reg[0].sim_puen <<
		SIM_SIM1_2_CNFG1_SIM_PUEN_SHIFT);
	ret = max77660_reg_write(pdev->dev.parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_SIM1CNFG2, val);
	if (ret < 0)
		goto err_reg_access;

	/* SIM2 config1 */
	val = sim_pdata->sim_reg[1].detect_en << SIM_SIM1_2_CNFG1_SIM_EN_SHIFT;
	val |= sim_pdata->sim_reg[1].batremove_en <<
		SIM_SIM1_2_CNFG1_BATREM_EN_SHIFT;
	val |= sim_pdata->sim_reg[1].det_debouncecnt <<
		SIM_SIM1_2_CNFG1_SIMDBCNT_SHIFT;
	ret = max77660_reg_write(pdev->dev.parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_SIM2CNFG1, val);
	if (ret < 0)
		goto err_reg_access;

	/* SIM2 config2 */
	val = sim_pdata->sim_reg[1].auto_pwrdn_en <<
		SIM_SIM1_2_CNFG1_SIM_PWRDEN_SHIFT;
	val |= sim_pdata->sim_reg[1].inst_pol <<
		SIM_SIM1_2_CNFG1_SIMAH_SHIFT;
	val |= sim_pdata->sim_reg[1].pwrdn_debouncecnt <<
		SIM_SIM1_2_CNFG1_SIMPWRDNCNT_SHIFT;
	val |= (sim_pdata->sim_reg[1].sim_puen <<
		SIM_SIM1_2_CNFG1_SIM_PUEN_SHIFT);
	ret = max77660_reg_write(pdev->dev.parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_SIM2CNFG2, val);
	if (ret < 0)
		goto err_reg_access;

	if (misc_register(&sim_miscdev) != 0) {
		dev_err(&pdev->dev, "sim: cannot register miscdev\n");
		return -ENODEV;
	}

	if (sysfs_create_group(&sim_miscdev.this_device->kobj,
				&sim_attr_group)) {
		dev_err(&pdev->dev, "sysfs fails\n");
		ret = -ENODEV;
		goto err_sysfs;
	}

	sim = devm_kzalloc(&pdev->dev,
				sizeof(*sim), GFP_KERNEL);
	if (!sim) {
		dev_err(&pdev->dev, "Could not allocate max77660_sim\n");
		ret = -ENOMEM;
		goto err_malloc;
	}

	sim->parent = pdev->dev.parent;
	sim->dev = &pdev->dev;
	sim->miscdev = sim_miscdev.this_device;
	sim->sim_irq = sim_irq;
	platform_set_drvdata(pdev, sim);
	misc_pdev = to_platform_device(sim_miscdev.this_device);
	misc_pdev->dev.platform_data = sim;

	ret = request_threaded_irq(sim_irq, NULL,  max77660_sim_irq,
			IRQF_ONESHOT,
			"SIM_DETECT_IRQ", sim);
	if (ret < 0) {
		dev_err(&pdev->dev, "sim irq fails\n");
		ret = -ENODEV;
		goto err_malloc;
	}

	return 0;

err_malloc:
	sysfs_remove_group(&sim_miscdev.this_device->kobj, &sim_attr_group);
err_sysfs:
	misc_deregister(&sim_miscdev);
err_reg_access:
	dev_err(&pdev->dev, "probe fails. ret(%d)\n", ret);

	return ret;
}

static int max77660_sim_remove(struct platform_device *pdev)
{
	struct max77660_sim *sim = platform_get_drvdata(pdev);

	free_irq(sim->sim_irq, sim);
	sysfs_remove_group(&sim_miscdev.this_device->kobj, &sim_attr_group);
	misc_deregister(&sim_miscdev);
	return 0;
}

static struct platform_driver max77660_sim_driver = {
	.driver.name	= "max77660-sim",
	.driver.owner	= THIS_MODULE,
	.probe		= max77660_sim_probe,
	.remove		= max77660_sim_remove,
};

static int __init max77660_sim_init(void)
{
	return platform_driver_register(&max77660_sim_driver);
}
subsys_initcall(max77660_sim_init);

static void __exit max77660_sim_exit(void)
{
	platform_driver_unregister(&max77660_sim_driver);
}
module_exit(max77660_sim_exit);

MODULE_DESCRIPTION("sim interface for MAX77660 PMIC");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:max77660-sim");
