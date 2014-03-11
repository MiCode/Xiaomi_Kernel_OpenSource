/*
 * Synopsys DesignWare I2C adapter driver (master only).
 *
 * Based on the TI DAVINCI I2C adapter driver.
 *
 * Copyright (C) 2006 Texas Instruments.
 * Copyright (C) 2007 MontaVista Software Inc.
 * Copyright (C) 2009 Provigent Ltd.
 *
 * ----------------------------------------------------------------------------
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * ----------------------------------------------------------------------------
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/acpi.h>

#include <acpi/actypes.h>
#include "i2c-designware-core.h"

static struct i2c_algorithm i2c_dw_algo = {
	.master_xfer	= i2c_dw_xfer,
	.functionality	= i2c_dw_func,
};
static u32 i2c_dw_get_clk_rate_khz(struct dw_i2c_dev *dev)
{
	return clk_get_rate(dev->clk)/1000;
}

#ifdef CONFIG_ACPI
struct dw_i2c_acpi_handler_data {
	struct acpi_connection_info info;
	struct platform_device *pdev;
};

static acpi_status
dw_i2c_acpi_space_handler(u32 function, acpi_physical_address address,
			u32 bits, u64 *value64,
			void *handler_context, void *region_context)
{
	struct dw_i2c_acpi_handler_data *data = handler_context;
	struct acpi_connection_info *info = &data->info;
	struct dw_i2c_dev *dev = platform_get_drvdata(data->pdev);
	struct acpi_resource_i2c_serialbus *sb;
	struct acpi_resource *ares;
	u8 target;
	int ret, length;
	u8 *value = (u8 *)value64;
	u8 *buffer;
	u32 accessor_type = function >> 16;
	u8 addr = (u8)address;
	struct i2c_msg msgs[2];


	acpi_buffer_to_resource(info->connection, info->length, &ares);
	if (ares->type != ACPI_RESOURCE_TYPE_SERIAL_BUS)
		return AE_BAD_PARAMETER;

	sb = &ares->data.i2c_serial_bus;
	if (sb->type != ACPI_RESOURCE_SERIAL_TYPE_I2C)
		return AE_BAD_PARAMETER;

	pr_info("%s: Found I2C Resource type, addr %d\n",
				__func__, sb->slave_address);
	target = sb->slave_address;

	length = acpi_get_serial_access_length(accessor_type, info->access_length);
	pr_info("%s: access opeation region, addr 0x%x operation %d len %d\n",
		__func__, addr, function, length);

	if (!value64)
		return AE_BAD_PARAMETER;

	function &= ACPI_IO_MASK; 
	if (function == ACPI_READ) {
		buffer = kzalloc(length, GFP_KERNEL);
	
		msgs[0].addr = target;
		msgs[0].flags = 0;
		msgs[0].len = 1;
		msgs[0].buf = &addr;
	
		msgs[1].addr = target;
		msgs[1].flags = I2C_M_RD;
		msgs[1].len = length;
		msgs[1].buf = buffer;
	
		ret = i2c_transfer(&dev->adapter, msgs, 2);
		if (ret < 0) {
			pr_info("%s: i2c read failed\n", __func__);	
			return AE_ERROR;		
		}
	
		memcpy(value + 2, buffer, length - 2);
		value[0] = value[1] = 0;
		kfree(buffer);
	} else if (function == ACPI_WRITE) {
//		buffer = kzalloc(length - 1, GFP_KERNEL);
//		
//		buffer[0] = addr;
//		memcpy(buffer + 1, value + 2, length - 2);
//		msgs[0].addr = target;
//		msgs[0].flags = 0;
//		msgs[0].len = length - 1;
//		msgs[0].buf = buffer;
//
//		ret = i2c_transfer(&dev->adapter, msgs, 2);
//		if (ret < 0) {
//			pr_info("%s: i2c read failed\n", __func__);	
//			return AE_ERROR;		
//		}
//		kfree(buffer);
//
	}

	return AE_OK;
}

static int dw_i2c_acpi_install_space_handler(struct platform_device *pdev)
{
	struct acpi_device *adev = ACPI_COMPANION(&pdev->dev);
	struct dw_i2c_acpi_handler_data *data;
	acpi_status status;

	if (!adev)
		return -EFAULT;

	data = devm_kzalloc(&pdev->dev, sizeof(struct dw_i2c_acpi_handler_data),
			    GFP_KERNEL);

	if(!data)
		return -ENOMEM;

	data->pdev = pdev;
	status = acpi_install_address_space_handler(adev->handle,
				ACPI_ADR_SPACE_GSBUS,
				&dw_i2c_acpi_space_handler,
				NULL,
				data);
	if (ACPI_FAILURE(status))
		return -EFAULT;
	return 0;
}


static void dw_i2c_acpi_params(struct platform_device *pdev, char method[],
			       u16 *hcnt, u16 *lcnt, u32 *sda_hold)
{
	struct acpi_buffer buf = { ACPI_ALLOCATE_BUFFER };
	acpi_handle handle = ACPI_HANDLE(&pdev->dev);
	union acpi_object *obj;

	if (ACPI_FAILURE(acpi_evaluate_object(handle, method, NULL, &buf)))
		return;

	obj = (union acpi_object *)buf.pointer;
	if (obj->type == ACPI_TYPE_PACKAGE && obj->package.count == 3) {
		const union acpi_object *objs = obj->package.elements;

		*hcnt = (u16)objs[0].integer.value;
		*lcnt = (u16)objs[1].integer.value;
		if (sda_hold)
			*sda_hold = (u32)objs[2].integer.value;
	}

	kfree(buf.pointer);
}

static int dw_i2c_acpi_configure(struct platform_device *pdev)
{
	struct dw_i2c_dev *dev = platform_get_drvdata(pdev);
	bool fs_mode = dev->master_cfg & DW_IC_CON_SPEED_FAST;

	if (!ACPI_HANDLE(&pdev->dev))
		return -ENODEV;

	dev->adapter.nr = -1;
	dev->tx_fifo_depth = 32;
	dev->rx_fifo_depth = 32;

	/*
	 * Try to get SDA hold time and *CNT values from an ACPI method if
	 * it exists for both supported speed modes.
	 */
	dw_i2c_acpi_params(pdev, "SSCN", &dev->ss_hcnt, &dev->ss_lcnt,
			   fs_mode ? NULL : &dev->sda_hold_time);
	dw_i2c_acpi_params(pdev, "FMCN", &dev->fs_hcnt, &dev->fs_lcnt,
			   fs_mode ? &dev->sda_hold_time : NULL);

	return 0;
}

static const struct acpi_device_id dw_i2c_acpi_match[] = {
	{ "INT33C2", 0 },
	{ "INT33C3", 0 },
	{ "INT3432", 0 },
	{ "INT3433", 0 },
	{ "80860F41", 0 },
	{ }
};
MODULE_DEVICE_TABLE(acpi, dw_i2c_acpi_match);
#else
static inline int dw_i2c_acpi_configure(struct platform_device *pdev)
{
	return -ENODEV;
}
#endif

static int dw_i2c_probe(struct platform_device *pdev)
{
	struct dw_i2c_dev *dev;
	struct i2c_adapter *adap;
	struct resource *mem;
	int irq, r;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "no irq resource?\n");
		return irq; /* -ENXIO */
	}

	dev = devm_kzalloc(&pdev->dev, sizeof(struct dw_i2c_dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dev->base = devm_ioremap_resource(&pdev->dev, mem);
	if (IS_ERR(dev->base))
		return PTR_ERR(dev->base);

	init_completion(&dev->cmd_complete);
	mutex_init(&dev->lock);
	dev->dev = &pdev->dev;
	dev->irq = irq;
	platform_set_drvdata(pdev, dev);

	dev->clk = devm_clk_get(&pdev->dev, NULL);
	dev->get_clk_rate_khz = i2c_dw_get_clk_rate_khz;

	if (IS_ERR(dev->clk))
		return PTR_ERR(dev->clk);
	clk_prepare_enable(dev->clk);

	if (pdev->dev.of_node) {
		u32 ht = 0;
		u32 ic_clk = dev->get_clk_rate_khz(dev);

		of_property_read_u32(pdev->dev.of_node,
					"i2c-sda-hold-time-ns", &ht);
		dev->sda_hold_time = div_u64((u64)ic_clk * ht + 500000,
					     1000000);
	}

	dev->functionality =
		I2C_FUNC_I2C |
		I2C_FUNC_10BIT_ADDR |
		I2C_FUNC_SMBUS_BYTE |
		I2C_FUNC_SMBUS_BYTE_DATA |
		I2C_FUNC_SMBUS_WORD_DATA |
		I2C_FUNC_SMBUS_I2C_BLOCK;
	dev->master_cfg =  DW_IC_CON_MASTER | DW_IC_CON_SLAVE_DISABLE |
		DW_IC_CON_RESTART_EN | DW_IC_CON_SPEED_FAST;

	/* Try first if we can configure the device from ACPI */
	r = dw_i2c_acpi_configure(pdev);
	if (r) {
		u32 param1 = i2c_dw_read_comp_param(dev);

		dev->tx_fifo_depth = ((param1 >> 16) & 0xff) + 1;
		dev->rx_fifo_depth = ((param1 >> 8)  & 0xff) + 1;
		dev->adapter.nr = pdev->id;
	}
	r = i2c_dw_init(dev);
	if (r)
		return r;

	i2c_dw_disable_int(dev);
	r = devm_request_irq(&pdev->dev, dev->irq, i2c_dw_isr, IRQF_SHARED,
			pdev->name, dev);
	if (r) {
		dev_err(&pdev->dev, "failure requesting irq %i\n", dev->irq);
		return r;
	}

	adap = &dev->adapter;
	i2c_set_adapdata(adap, dev);
	adap->owner = THIS_MODULE;
	adap->class = I2C_CLASS_HWMON;
	strlcpy(adap->name, "Synopsys DesignWare I2C adapter",
			sizeof(adap->name));
	adap->algo = &i2c_dw_algo;
	adap->dev.parent = &pdev->dev;
	adap->dev.of_node = pdev->dev.of_node;

	r = i2c_add_numbered_adapter(adap);
	if (r) {
		dev_err(&pdev->dev, "failure adding adapter\n");
		return r;
	}

	pm_runtime_set_autosuspend_delay(&pdev->dev, 1000);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	dw_i2c_acpi_install_space_handler(pdev);
	acpi_walk_dep_device_list();
	return 0;
}

static int dw_i2c_remove(struct platform_device *pdev)
{
	struct dw_i2c_dev *dev = platform_get_drvdata(pdev);

	pm_runtime_get_sync(&pdev->dev);

	i2c_del_adapter(&dev->adapter);

	i2c_dw_disable(dev);

	pm_runtime_put(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id dw_i2c_of_match[] = {
	{ .compatible = "snps,designware-i2c", },
	{},
};
MODULE_DEVICE_TABLE(of, dw_i2c_of_match);
#endif

#ifdef CONFIG_PM_SLEEP
static int dw_i2c_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct dw_i2c_dev *i_dev = platform_get_drvdata(pdev);

	clk_disable_unprepare(i_dev->clk);

	return 0;
}

static int dw_i2c_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct dw_i2c_dev *i_dev = platform_get_drvdata(pdev);

	clk_prepare_enable(i_dev->clk);
	i2c_dw_init(i_dev);

	return 0;
}

static SIMPLE_DEV_PM_OPS(dw_i2c_dev_pm_ops, dw_i2c_suspend, dw_i2c_resume);
#define DW_I2C_DEV_PM_OPS	(&dw_i2c_dev_pm_ops)
#else
#define DW_I2C_DEV_PM_OPS	NULL
#endif

/* work with hotplug and coldplug */
MODULE_ALIAS("platform:i2c_designware");

static struct platform_driver dw_i2c_driver = {
	.probe = dw_i2c_probe,
	.remove = dw_i2c_remove,
	.driver		= {
		.name	= "i2c_designware",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(dw_i2c_of_match),
		.acpi_match_table = ACPI_PTR(dw_i2c_acpi_match),
		.pm	= DW_I2C_DEV_PM_OPS,
	},
};

static int __init dw_i2c_init_driver(void)
{
	return platform_driver_register(&dw_i2c_driver);
}
subsys_initcall(dw_i2c_init_driver);

static void __exit dw_i2c_exit_driver(void)
{
	platform_driver_unregister(&dw_i2c_driver);
}
module_exit(dw_i2c_exit_driver);

MODULE_AUTHOR("Baruch Siach <baruch@tkos.co.il>");
MODULE_DESCRIPTION("Synopsys DesignWare I2C bus adapter");
MODULE_LICENSE("GPL");
