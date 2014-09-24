/*
 * intel_wcove_bcu.c: Intel Whiskey Cove PMIC Burst Contorl Unit Driver
 *
 * Copyright (C) 2014 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. Seee the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Albin B <albin.bala.krishnan@intel.com>
 */

#define pr_fmt(fmt)  DRIVER_NAME": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>
#include <linux/mfd/intel_soc_pmic.h>
#include <linux/version.h>
#include <asm/intel_wcove_bcu.h>

static DEFINE_MUTEX(bcu_update_lock);

struct wcpmic_bcu_info {
	struct device *dev;
	struct platform_device *pdev;
	int irq;
};

static void wcove_bcu_enable_trip_points(struct wcpmic_bcu_info *info)
{
	int i, ret;

	/**
	 * Enable the Voltage comparator logic, so that the output
	 * signals are asserted when a voltage drop occurs.
	 */
	for (i = 0; i < MAX_VOLTAGE_TRIP_POINTS; i++) {
		ret = intel_soc_pmic_setb(VWARNA_CFG_REG + i, (u8)VWARNA_EN);
		if (ret)
			dev_err(info->dev,
				"Error in %s setting register 0x%x\n",
				__func__, (VWARNA_CFG_REG + i));
	}

	/**
	 * Enable the Current trip points, so that the BCU logic will send
	 * interrupt to the SoC of IccMAX events.
	 */
	for (i = 0; i < MAX_CURRENT_TRIP_POINTS; i++) {
		ret = intel_soc_pmic_setb(ICCMAXVCC_CFG_REG + i,
				(u8)ICCMAXVCC_EN);
		if (ret)
			dev_err(info->dev,
				"Error in %s setting register 0x%x\n",
				__func__, (ICCMAXVCC_CFG_REG + i));
	}

}

static int wcove_bcu_program(struct wcpmic_bcu_config_data *config,
					int max_regs)
{
	int ret = 0, i;

	if (config == NULL) {
		ret = -EINVAL;
		pr_err("No config data Found!\n");
		goto invalid;
	}

	mutex_lock(&bcu_update_lock);
	for (i = 0; i < max_regs; i++) {
		ret = intel_soc_pmic_writeb(config[i].addr, config[i].data);
		if (ret < 0) {
			pr_err("In %s error(%d) while writing addr 0x%02x\n",
					__func__, ret, config[i].addr);
			break;
		}
	}

	mutex_unlock(&bcu_update_lock);
invalid:
	return ret;
}

#ifdef CONFIG_DEBUG_FS

static struct dentry *bcu_dbgfs_root;

static struct bcu_reg_info bcu_reg[] = {
	reg_info(BCUIRQ_REG),
	reg_info(IRQLVL1_REG),
	reg_info(MIRQLVL1_REG),
	reg_info(MBCUIRQ_REG),
	reg_info(SBCUIRQ_REG),
	reg_info(SBCUCTRL_REG),
	reg_info(VWARNA_CFG_REG),
	reg_info(VWARNB_CFG_REG),
	reg_info(VCRIT_CFG_REG),
	reg_info(ICCMAXVCC_CFG_REG),
	reg_info(ICCMAXVNN_CFG_REG),
	reg_info(ICCMAXVGG_CFG_REG),
	reg_info(BCUDISB_BEH_REG),
	reg_info(BCUDISCRIT_BEH_REG),
	reg_info(BCUVSYS_DRP_BEH_REG)
};

/**
 * wcove_bcu_dbgfs_write - debugfs: write the new state to an endpoint.
 * @file: The seq_file to write data to.
 * @user_buf: the user data which is need to write to an endpoint
 * @count: the size of the user data
 * @pos: loff_t" is a "long offset", which is the current reading or writing
 *       position.
 *
 * Send data to the device. If NULL,-EINVAL/-EFAULT return to the write call to
 * the calling program if it is non-negative return value represents the number
 * of bytes successfully written.
 */
static ssize_t wcove_bcu_dbgfs_write(struct file *file,
			const char __user *user_buf, size_t count, loff_t *pos)
{
	char buf[count];
	u8 data;
	u16 addr;
	int ret;
	struct seq_file *s = file->private_data;

	if (!s) {
		ret = -EINVAL;
		goto error;
	}

	addr = *((u16 *)s->private);
	if ((addr == BCUIRQ_REG) || (addr == IRQLVL1_REG) ||
			(addr == SBCUIRQ_REG)) {
		pr_err("DEBUGFS no permission to write Addr(0x%04x)\n", addr);
		ret = -EIO;
		goto error;
	}

	if (copy_from_user(buf, user_buf, count)) {
		pr_err("DEBUGFS unable to copy the user data.\n");
		ret = -EFAULT;
		goto error;
	}

	buf[count-1] = '\0';
	if (kstrtou8(buf, 16, &data)) {
		pr_err("DEBUGFS invalid user data.\n");
		ret = -EINVAL;
		goto error;
	}

	ret = intel_soc_pmic_writeb(addr, data);
	if (ret < 0) {
		pr_err("Dbgfs write error Addr: 0x%04x Data: 0x%02x\n",
				addr, data);
		goto error;
	}
	pr_debug("DEBUGFS written Data: 0x%02x Addr: 0x%04x\n",	data, addr);
	return count;

error:
	return ret;
}

/**
 * wcove_bcu_reg_show - debugfs: show the state of an endpoint.
 * @s: The seq_file to read data from.
 * @unused: not used
 *
 * This debugfs entry shows the content of the register
 * given in the data parameter.
 */
static int wcove_bcu_reg_show(struct seq_file *s, void *unused)
{
	u16 addr = 0;
	int value;

	addr = *((u16 *)s->private);
	value = intel_soc_pmic_readb(addr);
	if (value < 0) {
		pr_err("Error in reading 0x%04x register!!\n", addr);
		return value;
	}
	seq_printf(s, "0x%02x\n", value);

	return 0;
}

/**
 * wcove_bcu_dbgfs_open - debugfs: to open the endpoint for read/write operation
 * @inode: inode structure is used by the kernel internally to represent files.
 * @file: It is created by the kernel on open and is passed to any function
 *        that operates on the file, until the last close. After all instances
 *        of the file are closed, the kernel releases the data structure.
 *
 * This is the first operation of the files on the device, does not require the
 * driver to declare a corresponding method. If this is NULL, the device is
 * turned on has been successful, but the driver will not be notified
 */
static int wcove_bcu_dbgfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, wcove_bcu_reg_show, inode->i_private);
}

static const struct file_operations bcu_dbgfs_fops = {
	.owner		= THIS_MODULE,
	.open		= wcove_bcu_dbgfs_open,
	.release	= single_release,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.write		= wcove_bcu_dbgfs_write,
};

static void wcove_bcu_create_debugfs(struct wcpmic_bcu_info *info)
{
	char reg_name[MAX_REGNAME_LEN] = {0};
	u32 idx;
	u32 max_dbgfs_num = ARRAY_SIZE(bcu_reg);
	struct dentry *entry;

	bcu_dbgfs_root = debugfs_create_dir(DRIVER_NAME, NULL);
	if (IS_ERR(bcu_dbgfs_root)) {
		dev_warn(info->dev, "DEBUGFS directory(%s) create failed!\n",
				DRIVER_NAME);
		return;
	}

	for (idx = 0; idx < max_dbgfs_num; idx++) {
		snprintf(reg_name, MAX_REGNAME_LEN, "%s", bcu_reg[idx].name);
		entry = debugfs_create_file(reg_name,
						bcu_reg[idx].mode,
						bcu_dbgfs_root,
						&bcu_reg[idx].addr,
						&bcu_dbgfs_fops);
		if (IS_ERR(entry)) {
			debugfs_remove_recursive(bcu_dbgfs_root);
			bcu_dbgfs_root = NULL;
			dev_warn(info->dev, "DEBUGFS %s creation failed!!\n",
					reg_name);
			return;
		}
	}
	dev_info(info->dev, "DEBUGFS %s created successfully.\n", DRIVER_NAME);
}

static inline void wcove_bcu_remove_debugfs(struct wcpmic_bcu_info *info)
{
	debugfs_remove_recursive(bcu_dbgfs_root);
}
#else
static inline void wcove_bcu_create_debugfs(struct wcpmic_bcu_info *info) { }
static inline void wcove_bcu_remove_debugfs(struct wcpmic_bcu_info *info) { }
#endif /* CONFIG_DEBUG_FS */

static inline struct power_supply *wcove_bcu_get_psy_battery(void)
{
	struct class_dev_iter iter;
	struct device *dev;
	static struct power_supply *psy;

	class_dev_iter_init(&iter, power_supply_class, NULL, NULL);
	while ((dev = class_dev_iter_next(&iter))) {
		psy = (struct power_supply *)dev_get_drvdata(dev);
		if (psy->type == POWER_SUPPLY_TYPE_BATTERY) {
			class_dev_iter_exit(&iter);
			return psy;
		}
	}
	class_dev_iter_exit(&iter);

	return NULL;
}

/* Reading the Voltage now value of the battery */
static inline int wcove_bcu_get_battery_voltage(int *volt)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = wcove_bcu_get_psy_battery();
	if (!psy)
		return -EINVAL;

	ret = psy->get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	if (!ret)
		*volt = val.intval;

	return ret;
}

static void wcove_bcu_handle_vwarnb_event(void *dev_data, u8 status)
{
	int beh_data, ret;
	struct wcpmic_bcu_info *info = (struct wcpmic_bcu_info *)dev_data;

	/* If Vsys is below WARNB level no action required from driver */
	if (!(status & SVWARNB)) {
		/* Vsys is above WARN2 level */
		dev_info(info->dev, "Recovered from VWARNB Level\n");

		/* clearing SBCUDISB signal if asserted */
		beh_data = intel_soc_pmic_readb(BCUDISB_BEH_REG);
		if (beh_data < 0)
			goto fail;
		if (IS_ASSRT_ON_BCUDISB(beh_data) &&
				IS_BCUDISB_STICKY(beh_data)) {
			/* Clear the Status of the BCUDISB Output Signal */
			ret = intel_soc_pmic_setb(SBCUCTRL_REG, SBCUDISB);
			if (ret)
				goto fail;
		}
	}
	return;
fail:
	dev_err(info->dev, "Register read/write failed:func:%s()\n", __func__);
	return;
}

static void wcove_bcu_handle_vwarn_event(void *dev_data, int event)
{
	u8 irq_status;
	struct wcpmic_bcu_info *info = (struct wcpmic_bcu_info *)dev_data;
	int ret;

	ret = intel_soc_pmic_readb(SBCUIRQ_REG);
	if (ret < 0)
		return;

	irq_status = ret;
	dev_dbg(info->dev, "SBCUIRQ_REG: %x\n", irq_status);

	if (event == VWARNB) {
		dev_info(info->dev, "VWARNB Event has occured\n");
		wcove_bcu_handle_vwarnb_event(dev_data, irq_status);
	} else if (event == VWARNA) {
		dev_info(info->dev, "VWARNA Event has occured\n");
	}
}

static irqreturn_t wcove_bcu_intr_handler(int irq, void *dev_data)
{
	return IRQ_WAKE_THREAD;
}

static irqreturn_t wcove_bcu_intr_thread_handler(int irq, void *dev_data)
{
	int ret = IRQ_NONE;
	int bat_volt;
	int irq_data;
	struct wcpmic_bcu_info *info = (struct wcpmic_bcu_info *)dev_data;

	if (!info)
		return ret;

	mutex_lock(&bcu_update_lock);
	if (wcove_bcu_get_battery_voltage(&bat_volt))
		dev_err(info->dev, "Error in getting battery voltage\n");
	else
		dev_info(info->dev, "Battery Volatge= %dmV\n", (bat_volt/1000));

	irq_data = intel_soc_pmic_readb(BCUIRQ_REG);
	if (irq_data < 0)
		goto fail;

	/* here not handling(no action) for GSMPULSE and TXPWRTH events */
	if (irq_data & VCRIT)
		/* VCRIT shutdown based on register configuration */
		dev_info(info->dev, "VCRIT Event has occured\n");

	if (irq_data & VWARNB)
		wcove_bcu_handle_vwarn_event(dev_data, VWARNB);

	if (irq_data & VWARNA)
		wcove_bcu_handle_vwarn_event(dev_data, VWARNA);

	if (irq_data & GSMPULSE)
		dev_info(info->dev, "GSMPULSE Event has occured\n");

	if (irq_data & TXPWRTH)
		dev_info(info->dev, "TXPWRTH Event has occured\n");

	ret = IRQ_HANDLED;

fail:
	mutex_unlock(&bcu_update_lock);
	return ret;
}


static int wcove_bcu_probe(struct platform_device *pdev)
{
	int ret;
	struct wcpmic_bcu_info *info;
	struct wcpmic_bcu_config_data *bcu_config;
	struct wcove_bcu_platform_data *pdata = pdev->dev.platform_data;

	if (!pdata) {
		dev_err(&pdev->dev, "no platform data supplied\n");
		ret = -EINVAL;
		goto exit;
	}

	info = devm_kzalloc(&pdev->dev,
			sizeof(struct wcpmic_bcu_info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "kzalloc failed\n");
		ret = -ENOMEM;
		goto exit;
	}

	info->pdev = pdev;
	info->irq = platform_get_irq(pdev, 0);
	platform_set_drvdata(pdev, info);

	/* Registering with hwmon class */
	info->dev = hwmon_device_register(&pdev->dev);
	if (IS_ERR(info->dev)) {
		ret = PTR_ERR(info->dev);
		info->dev = NULL;
		dev_err(&pdev->dev, "hwmon_dev_regs failed\n");
		goto exit_free;
	}

	/* Unmask 1st level BCU interrupt in the mask register */
	ret = intel_soc_pmic_clearb(MIRQLVL1_REG, (u8)MBCU);
	if (ret) {
		dev_err(&pdev->dev, "Unmasking of BCU failed:%d\n", ret);
		goto exit_hwmon;
	}

	/* Register for Interrupt Handler */
	ret = request_threaded_irq(info->irq, wcove_bcu_intr_handler,
						wcove_bcu_intr_thread_handler,
						IRQF_NO_SUSPEND,
						DRIVER_NAME, info);
	if (ret) {
		dev_err(&pdev->dev,
			"request_threaded_irq failed:%d\n", ret);
		goto exit_hwmon;
	}
	bcu_config = pdata->config;

	/* Program the BCU with default values read from the platform */
	ret = wcove_bcu_program(bcu_config, pdata->num_regs);
	if (ret) {
		dev_err(&pdev->dev, "wcove_bcu_program() failed:%d\n", ret);
		goto exit_freeirq;
	}

	/* enable voltage and current trip points */
	wcove_bcu_enable_trip_points(info);

	/* Create debufs for the basincove bcu registers */
	wcove_bcu_create_debugfs(info);
	return 0;

exit_freeirq:
	free_irq(info->irq, info);
exit_hwmon:
	hwmon_device_unregister(info->dev);
exit_free:
	devm_kfree(&pdev->dev, (void *)info);
exit:
	return ret;
}

static int wcove_bcu_remove(struct platform_device *pdev)
{
	struct wcpmic_bcu_info *info = platform_get_drvdata(pdev);

	if (info) {
		free_irq(info->irq, info);
		wcove_bcu_remove_debugfs(info);
		hwmon_device_unregister(info->dev);
		devm_kfree(&pdev->dev, (void *)info);
	}
	return 0;
}

/*********************************************************************
 *		Driver initialisation and finalization
 *********************************************************************/

static const struct platform_device_id wcove_bcu_id_table[] = {
	{DRIVER_NAME, 1},
};

static struct platform_driver wcpmic_bcu_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
	},
	.probe = wcove_bcu_probe,
	.remove = wcove_bcu_remove,
	.id_table = wcove_bcu_id_table,
};
module_platform_driver(wcpmic_bcu_driver);

MODULE_AUTHOR("Albin B <albin.bala.krishnan@intel.com>");
MODULE_DESCRIPTION("Intel Whiskey Cove PMIC Burst Contorl Unit Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRIVER_NAME);
