/*
 * Palmas SIM driver
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 * Author: Neil Patel <neilp@nvidia.com>
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

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mfd/palmas.h>
#include <linux/platform_device.h>

struct palmas_sim {
	struct device	*parent;
	struct device	*dev;
	struct device	*miscdev;
	struct palmas	*palmas;
	int		sim1_irq;
	int		sim2_irq;
	bool		sim1_inserted;
	bool		sim2_inserted;
};

static ssize_t palmas_sim1_state_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct palmas_sim *sim = dev_get_platdata(dev);

	return sprintf(buf, "%s\n", sim->sim1_inserted ? "1" : "0");
}

static DEVICE_ATTR(sim1_inserted, S_IRUSR, palmas_sim1_state_read, NULL);

static ssize_t palmas_sim2_state_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct palmas_sim *sim = dev_get_platdata(dev);

	return sprintf(buf, "%s\n", sim->sim2_inserted ? "1" : "0");
}
static DEVICE_ATTR(sim2_inserted, S_IRUSR, palmas_sim2_state_read, NULL);

static struct attribute *sim_attrs[] = {
	&dev_attr_sim1_inserted.attr,
	&dev_attr_sim2_inserted.attr,
	NULL
};

static const struct attribute_group sim_attr_group = {
	.attrs = sim_attrs,
};

static irqreturn_t palmas_sim_irq(int irq, void *data)
{
	struct palmas_sim *sim = data;
	bool sim1_inserted = false;
	bool sim2_inserted = false;
	int ret = 0;
	int val;

	dev_dbg(sim->dev, "sim irq %d hit\n", irq);

	if (irq == sim->sim1_irq) {
		ret = palmas_read(sim->palmas, PALMAS_SIMCARD_BASE,
					PALMAS_SIM_DEBOUNCE, &val);
		if (ret < 0)
			goto done;

		if (val & PALMAS_SIM_DEBOUNCE_SIM_DET1_PIN_STATE)
			sim1_inserted = true;

		if (sim1_inserted != sim->sim1_inserted) {
			ret = palmas_update_bits(sim->palmas,
					PALMAS_SIMCARD_BASE,
					PALMAS_SIM_DEBOUNCE,
					PALMAS_SIM_DEBOUNCE_SIM1_IR,
					(sim1_inserted ? 1 : 0) <<
					PALMAS_SIM_DEBOUNCE_SIM1_IR_SHIFT);
			if (ret < 0)
				goto done;

			sim->sim1_inserted = sim1_inserted;
			sysfs_notify(&sim->miscdev->kobj, NULL,
				"sim1_inserted");
			dev_dbg(sim->dev, "sim1(%s)\n",
				sim1_inserted ? "inserted" : "removed");
		}
	} else if (irq == sim->sim2_irq) {
		ret = palmas_read(sim->palmas, PALMAS_SIMCARD_BASE,
					PALMAS_SIM_PWR_DOWN, &val);
		if (ret < 0)
			goto done;

		if (val & PALMAS_SIM_PWR_DOWN_SIM_DET2_PIN_STATE)
			sim2_inserted = true;

		if (sim2_inserted != sim->sim2_inserted) {
			ret = palmas_update_bits(sim->palmas,
					PALMAS_SIMCARD_BASE,
					PALMAS_SIM_DEBOUNCE,
					PALMAS_SIM_DEBOUNCE_SIM2_IR,
					(sim2_inserted ? 1 : 0) <<
					PALMAS_SIM_DEBOUNCE_SIM2_IR_SHIFT);
			if (ret < 0)
				goto done;

			sim->sim2_inserted = sim2_inserted;
			sysfs_notify(&sim->miscdev->kobj, NULL,
				"sim2_inserted");
			dev_dbg(sim->dev, "sim2(%s)\n",
				sim2_inserted ? "inserted" : "removed");
		}
	}

done:
	if (ret)
		dev_err(sim->dev, "reg access failed, ret %d, irq %d\n", ret,
			irq);

	return IRQ_HANDLED;
}

static struct miscdevice sim_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sim",
};

static int palmas_sim_probe(struct platform_device *pdev)
{
	struct palmas_platform_data *pdata;
	struct palmas_sim *sim;
	struct palmas_sim_platform_data *sim_pdata;
	struct palmas *palmas;
	struct platform_device *misc_pdev;
	int sim1_irq;
	int sim2_irq;
	bool sim1_inserted = false;
	bool sim2_inserted = false;
	int val;
	int mask;
	int ret;

	pdata = dev_get_platdata(pdev->dev.parent);
	if (!pdata) {
		dev_err(&pdev->dev, "Platform data not found\n");
		return -ENODEV;
	}

	sim_pdata = pdata->sim_pdata;
	palmas = dev_get_drvdata(pdev->dev.parent);

	sim1_irq = palmas_irq_get_virq(palmas, PALMAS_SIM1_IRQ);
	if (sim1_irq <= 0) {
		dev_err(&pdev->dev, "sim1 interrupt is not available\n");
		return -ENODEV;
	}

	sim2_irq = palmas_irq_get_virq(palmas, PALMAS_SIM2_IRQ);
	if (sim2_irq <= 0) {
		dev_err(&pdev->dev, "sim2 interrupt is not available\n");
		return -ENODEV;
	}

	/* set detection polarity - 0 is active high */
	val = sim_pdata->det_polarity <<
		PALMAS_POLARITY_CTRL2_DET_POLARITY_SHIFT;
	ret = palmas_update_bits(palmas, PALMAS_PU_PD_OD_BASE,
				PALMAS_POLARITY_CTRL2,
				PALMAS_POLARITY_CTRL2_DET_POLARITY, val);
	if (ret < 0)
		goto err_reg_access;

	/* initialize pull-ups and pull-downs */
	val = (sim_pdata->det1_pu << PALMAS_PU_PD_INPUT_CTRL5_DET1_PU_SHIFT) |
	      (sim_pdata->det1_pd << PALMAS_PU_PD_INPUT_CTRL5_DET1_PD_SHIFT) |
	      (sim_pdata->det2_pu << PALMAS_PU_PD_INPUT_CTRL5_DET2_PU_SHIFT) |
	      (sim_pdata->det2_pd << PALMAS_PU_PD_INPUT_CTRL5_DET2_PD_SHIFT);
	mask = (PALMAS_PU_PD_INPUT_CTRL5_DET1_PU |
		PALMAS_PU_PD_INPUT_CTRL5_DET1_PD |
		PALMAS_PU_PD_INPUT_CTRL5_DET2_PU |
		PALMAS_PU_PD_INPUT_CTRL5_DET2_PD);
	ret = palmas_update_bits(palmas, PALMAS_PU_PD_OD_BASE,
				PALMAS_PU_PD_INPUT_CTRL5, mask, val);
	if (ret < 0)
		goto err_reg_access;

	/* SIM_DEBOUNCE register configuration */
	val = sim_pdata->dbcnt << PALMAS_SIM_DEBOUNCE_DBCNT_SHIFT;
	ret = palmas_write(palmas, PALMAS_SIMCARD_BASE, PALMAS_SIM_DEBOUNCE,
				val);
	if (ret < 0)
		goto err_reg_access;

	/* SIM_PWR_DOWN register configuration */
	val = sim_pdata->pwrdnen2 << PALMAS_SIM_PWR_DOWN_PWRDNEN2_SHIFT;
	val |= sim_pdata->pwrdnen1 << PALMAS_SIM_PWR_DOWN_PWRDNEN1_SHIFT;
	val |= sim_pdata->pwrdncnt << PALMAS_SIM_PWR_DOWN_PWRDNCNT_SHIFT;
	ret = palmas_write(palmas, PALMAS_SIMCARD_BASE, PALMAS_SIM_PWR_DOWN,
				val);
	if (ret < 0)
		goto err_reg_access;

	/* 1 ms is the max debounce time */
	usleep_range(1000, 2000);

	/* read sim1 state */
	ret = palmas_read(palmas, PALMAS_SIMCARD_BASE, PALMAS_SIM_DEBOUNCE,
				&val);
	if (ret < 0)
		goto err_reg_access;
	if (val & PALMAS_SIM_DEBOUNCE_SIM_DET1_PIN_STATE)
		sim1_inserted = true;

	/* read sim2 state */
	ret = palmas_read(palmas, PALMAS_SIMCARD_BASE, PALMAS_SIM_PWR_DOWN,
				&val);
	if (ret < 0)
		goto err_reg_access;
	if (val & PALMAS_SIM_PWR_DOWN_SIM_DET2_PIN_STATE)
		sim2_inserted = true;

	if (misc_register(&sim_miscdev) != 0) {
		dev_err(&pdev->dev, "sim: cannot register miscdev\n");
		return -ENODEV;
	}

	if (sysfs_create_group(&sim_miscdev.this_device->kobj,
				&sim_attr_group)) {
		dev_err(&pdev->dev, "sysfs group create fails\n");
		ret = -ENODEV;
		goto err_sysfs;
	}

	sim = devm_kzalloc(&pdev->dev, sizeof(*sim), GFP_KERNEL);
	if (!sim) {
		dev_err(&pdev->dev, "Could not allocate palmas_sim\n");
		ret = -ENOMEM;
		goto err_malloc;
	}

	sim->parent = pdev->dev.parent;
	sim->dev = &pdev->dev;
	sim->miscdev = sim_miscdev.this_device;
	sim->sim1_irq = sim1_irq;
	sim->sim2_irq = sim2_irq;
	sim->sim1_inserted = sim1_inserted;
	sim->sim2_inserted = sim2_inserted;
	sim->palmas = palmas;
	platform_set_drvdata(pdev, sim);

	misc_pdev = to_platform_device(sim_miscdev.this_device);
	misc_pdev->dev.platform_data = sim;

	ret = request_threaded_irq(sim1_irq, NULL, palmas_sim_irq,
				IRQF_ONESHOT, "SIM1_DETECT_IRQ", sim);
	if (ret < 0) {
		dev_err(&pdev->dev, "sim1 irq request fails\n");
		ret = -ENODEV;
		goto err_malloc;
	}

	ret = request_threaded_irq(sim2_irq, NULL, palmas_sim_irq,
				IRQF_ONESHOT, "SIM2_DETECT_IRQ", sim);
	if (ret < 0) {
		dev_err(&pdev->dev, "sim2 irq request fails\n");
		ret = -ENODEV;
		goto err_sim1_irq;
	}

	/* enable insertion/removal detection based on sim status */
	val = ((sim1_inserted ? 1 : 0) << PALMAS_SIM_DEBOUNCE_SIM1_IR_SHIFT) |
	      ((sim2_inserted ? 1 : 0) << PALMAS_SIM_DEBOUNCE_SIM2_IR_SHIFT);
	ret = palmas_update_bits(palmas, PALMAS_SIMCARD_BASE,
				PALMAS_SIM_DEBOUNCE,
				PALMAS_SIM_DEBOUNCE_SIM1_IR |
				PALMAS_SIM_DEBOUNCE_SIM2_IR, val);
	if (ret < 0)
		goto err_sim2_irq;

	return 0;

err_sim2_irq:
	free_irq(sim2_irq, sim);
err_sim1_irq:
	free_irq(sim1_irq, sim);
err_malloc:
	sysfs_remove_group(&sim_miscdev.this_device->kobj, &sim_attr_group);
err_sysfs:
	misc_deregister(&sim_miscdev);
err_reg_access:
	dev_err(&pdev->dev, "probe fails due to reg access %d\n", ret);

	return ret;
}

static int palmas_sim_remove(struct platform_device *pdev)
{
	struct palmas_sim *sim = platform_get_drvdata(pdev);

	free_irq(sim->sim2_irq, sim);
	free_irq(sim->sim1_irq, sim);
	sysfs_remove_group(&sim_miscdev.this_device->kobj, &sim_attr_group);
	misc_deregister(&sim_miscdev);

	return 0;
}

static struct platform_driver palmas_sim_driver = {
	.driver.name	= "palmas-sim",
	.driver.owner	= THIS_MODULE,
	.probe		= palmas_sim_probe,
	.remove		= palmas_sim_remove,
};

static int __init palmas_sim_init(void)
{
	return platform_driver_register(&palmas_sim_driver);
}
subsys_initcall(palmas_sim_init);

static void __exit palmas_sim_exit(void)
{
	platform_driver_unregister(&palmas_sim_driver);
}
module_exit(palmas_sim_exit);

MODULE_DESCRIPTION("sim driver for palmas pmic");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Neil Patel <neilp@nvidia.com>");
MODULE_ALIAS("platform:palmas-sim");
