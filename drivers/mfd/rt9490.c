// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <dt-bindings/mfd/richtek,rt9490.h>
#include <linux/bits.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pm.h>
#include <linux/regmap.h>

#define RT9490_REG_EOC_CTRL		0x09
#define RT9490_REG_CHG_IRQ_FLAG0	0x22
#define RT9490_REG_CHG_IRQ_MASK0	0x28
#define RT9490_REG_DEVICE_INFO		0x48
#define RT9490_REG_ADD_IRQ_FLAG		0x4D
#define RT9490_REG_ADD_IRQ_MASK		0x4E

#define RT9490_NUM_IRQ_REGS		7
#define RT9490_ADD_IRQ_OFFSET		6
#define RT9490_RSTRG_MASK		BIT(6)
#define RT9490_DEVICE_ID_MASK		GENMASK(6, 3)
#define RT9490_VENDOR_ID		0x60
//#define RT9490_VENDOR_ID		0x00

struct rt9490_data {
	struct i2c_client *i2c;
	struct device *dev;
	struct regmap *regmap;
	struct irq_domain *irq_domain;
	struct mutex lock;
	struct mutex hidden_mode_lock;
	struct irq_chip irq_chip;
	u8 mask_buf[RT9490_NUM_IRQ_REGS];
	int hidden_mode_cnt;
};

static const struct mfd_cell rt9490_devs[] = {
	MFD_CELL_OF("rt9490-adc", NULL, NULL, 0, 0, "richtek,rt9490-adc"),
	MFD_CELL_OF("rt9490-chg", NULL, NULL, 0, 0, "richtek,rt9490-chg")
};

static void rt9490_irq_bus_lock(struct irq_data *irq_data)
{
	struct rt9490_data *data = irq_data_get_irq_chip_data(irq_data);

	mutex_lock(&data->lock);
}

static void rt9490_irq_bus_unlock(struct irq_data *irq_data)
{
	struct rt9490_data *data = irq_data_get_irq_chip_data(irq_data);
	unsigned int reg_offset = irq_data->hwirq / 8;
	unsigned int reg_addr;
	int ret;

	if (reg_offset == RT9490_ADD_IRQ_OFFSET)
		reg_addr = RT9490_REG_ADD_IRQ_MASK;
	else
		reg_addr = RT9490_REG_CHG_IRQ_MASK0 + reg_offset;

	ret = regmap_write(data->regmap, reg_addr, data->mask_buf[reg_offset]);
	if (ret)
		dev_err(data->dev, "Failed to write mask [%d]\n", reg_offset);

	mutex_unlock(&data->lock);
}

static void rt9490_irq_enable(struct irq_data *irq_data)
{
	struct rt9490_data *data = irq_data_get_irq_chip_data(irq_data);

	data->mask_buf[irq_data->hwirq / 8] &= ~BIT(irq_data->hwirq % 8);
}

static void rt9490_irq_disable(struct irq_data *irq_data)
{
	struct rt9490_data *data = irq_data_get_irq_chip_data(irq_data);

	data->mask_buf[irq_data->hwirq / 8] |= BIT(irq_data->hwirq % 8);
}

static int rt9490_irq_map(struct irq_domain *h, unsigned int virq,
			  irq_hw_number_t hwirq)
{
	struct rt9490_data *data = h->host_data;
	struct i2c_client *i2c = to_i2c_client(data->dev);

	irq_set_chip_data(virq, data);
	irq_set_chip(virq, &data->irq_chip);
	irq_set_nested_thread(virq, true);
	irq_set_parent(virq, i2c->irq);
	irq_set_noprobe(virq);

	return 0;
}

static const struct irq_domain_ops rt9490_irq_domain_ops = {
	.map = rt9490_irq_map,
	.xlate = irq_domain_xlate_onetwocell,
};

static irqreturn_t rt9490_irq_handler(int irq, void *priv)
{
	struct rt9490_data *data = priv;
	u8 status_buf[RT9490_NUM_IRQ_REGS], evts;
	unsigned int add_event;
	bool handled = false;
	int i, j, ret;

	ret = regmap_raw_read(data->regmap, RT9490_REG_CHG_IRQ_FLAG0,
			      status_buf, RT9490_ADD_IRQ_OFFSET);

	ret |= regmap_read(data->regmap, RT9490_REG_ADD_IRQ_FLAG, &add_event);
	if (ret) {
		dev_err(data->dev, "Failed to read irq flag\n");
		return IRQ_NONE;
	}
	status_buf[RT9490_ADD_IRQ_OFFSET] = add_event;

	for (i = 0; i < RT9490_NUM_IRQ_REGS; i++) {
		evts = status_buf[i] & ~data->mask_buf[i];
		if (!evts)
			continue;

		for (j = 0; j < 8; j++) {
			if (!(evts & BIT(j)))
				continue;

			handle_nested_irq(irq_find_mapping(data->irq_domain,
							   i * 8 + j));
			handled = true;
		}
	}

	return handled ? IRQ_HANDLED : IRQ_NONE;
}

static int rt9490_add_irq_chip(struct rt9490_data *data)
{
	struct i2c_client *i2c = to_i2c_client(data->dev);
	u8 status_buf[RT9490_ADD_IRQ_OFFSET];
	unsigned int add_event;
	int ret;

	mutex_init(&data->lock);

	data->irq_chip.name = dev_name(data->dev);
	data->irq_chip.irq_bus_lock = rt9490_irq_bus_lock;
	data->irq_chip.irq_bus_sync_unlock = rt9490_irq_bus_unlock;
	data->irq_chip.irq_enable = rt9490_irq_enable;
	data->irq_chip.irq_disable = rt9490_irq_disable;

	/* Mask and read clear all events by default */
	memset(data->mask_buf, 0xff, RT9490_NUM_IRQ_REGS);
	ret = regmap_raw_write(data->regmap, RT9490_REG_CHG_IRQ_MASK0,
			       data->mask_buf, RT9490_ADD_IRQ_OFFSET);

	ret |= regmap_write(data->regmap, RT9490_REG_ADD_IRQ_MASK,
			    data->mask_buf[RT9490_ADD_IRQ_OFFSET]);
	if (ret) {
		dev_err(data->dev, "Failed to write irq mask\n");
		return ret;
	}

	ret = regmap_raw_read(data->regmap, RT9490_REG_CHG_IRQ_FLAG0,
			      status_buf, RT9490_ADD_IRQ_OFFSET);

	ret |= regmap_read(data->regmap, RT9490_REG_ADD_IRQ_FLAG, &add_event);
	if (ret) {
		dev_err(data->dev, "Failed to read clear irq flag\n");
		return ret;
	}

	data->irq_domain = irq_domain_add_linear(data->dev->of_node,
						 RT9490_NUM_IRQ_REGS * 8,
						 &rt9490_irq_domain_ops, data);
	if (!data->irq_domain) {
		dev_err(data->dev, "Failed to create irq domain\n");
		return -ENOMEM;
	}

	ret = request_threaded_irq(i2c->irq, NULL, rt9490_irq_handler,
				   IRQF_ONESHOT, dev_name(data->dev), data);
	if (ret) {
		dev_err(data->dev, "Failed to request irq(%d)\n", ret);
		irq_domain_remove(data->irq_domain);
		return ret;
	}

	device_init_wakeup(data->dev, true);
	return 0;
}

static void rt9490_del_irq_chip(struct rt9490_data *data)
{
	struct i2c_client *i2c = to_i2c_client(data->dev);

	device_init_wakeup(data->dev, false);
	free_irq(i2c->irq, data);
	irq_domain_remove(data->irq_domain);
	mutex_destroy(&data->lock);
}

static const struct regmap_config rt9490_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = RT9490_REG_ADD_IRQ_MASK,
};

static int rt9490_check_vendor_info(struct rt9490_data *data)
{
	unsigned int devid;
	int ret;

	ret = regmap_read(data->regmap, RT9490_REG_DEVICE_INFO, &devid);
	if (ret)
		return ret;

	if ((devid & RT9490_DEVICE_ID_MASK) != RT9490_VENDOR_ID) {
		dev_err(data->dev, "VID not correct [0x%02x]\n", devid);
		return -ENODEV;
	}

	return 0;
}

static int rt9490_probe(struct i2c_client *i2c)
{
	struct rt9490_data *data;
	int ret;

	data = devm_kzalloc(&i2c->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	mutex_init(&data->hidden_mode_lock);
	data->hidden_mode_cnt = 0;
	data->dev = &i2c->dev;
	data->i2c = i2c;
	i2c_set_clientdata(i2c, data);

	data->regmap = devm_regmap_init_i2c(i2c, &rt9490_regmap_config);
	if (IS_ERR(data->regmap))
		return PTR_ERR(data->regmap);

	ret = rt9490_check_vendor_info(data);
	if (ret) {
		dev_err(&i2c->dev, "Failed to check vendor info\n");
		return ret;
	}

	ret = rt9490_add_irq_chip(data);
	if (ret)
		return ret;

	ret = devm_mfd_add_devices(&i2c->dev, PLATFORM_DEVID_AUTO, rt9490_devs,
				   ARRAY_SIZE(rt9490_devs), NULL, 0,
				   data->irq_domain);
	if (ret) {
		dev_err(&i2c->dev, "Failed to add sub devices\n");
		rt9490_del_irq_chip(data);
	}

	return 0;
}

static int rt9490_remove(struct i2c_client *i2c)
{
	struct rt9490_data *data = i2c_get_clientdata(i2c);

	rt9490_del_irq_chip(data);
	return 0;
}

static void rt9490_shutdown(struct i2c_client *i2c)
{
	struct rt9490_data *data = i2c_get_clientdata(i2c);
	int ret;

	/* Trigger the whole chip register reset */
	ret = regmap_update_bits(data->regmap, RT9490_REG_EOC_CTRL,
				 RT9490_RSTRG_MASK, RT9490_RSTRG_MASK);
	if (ret)
		dev_err(&i2c->dev, "Failed to reset registers(%d)\n", ret);
}

static int __maybe_unused rt9490_suspend(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);

	if (device_may_wakeup(dev))
		enable_irq_wake(i2c->irq);

	return 0;
}

static int __maybe_unused rt9490_resume(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);

	if (device_may_wakeup(dev))
		disable_irq_wake(i2c->irq);

	return 0;
}

static SIMPLE_DEV_PM_OPS(rt9490_pm_ops, rt9490_suspend, rt9490_resume);

static const struct of_device_id rt9490_of_match_table[] = {
	{ .compatible = "richtek,rt9490", },
	{ }
};
MODULE_DEVICE_TABLE(of, rt9490_of_match_table);

static struct i2c_driver rt9490_driver = {
	.driver = {
		.name = "rt9490",
		.of_match_table = rt9490_of_match_table,
		.pm = &rt9490_pm_ops,
	},
	.probe_new = rt9490_probe,
	.remove = rt9490_remove,
	.shutdown = rt9490_shutdown,
};
module_i2c_driver(rt9490_driver);

MODULE_DESCRIPTION("Richtek RT9490 MFD driver");
MODULE_AUTHOR("ChiYuan Huang <cy_huang@richtek.com>");
MODULE_LICENSE("GPL");
