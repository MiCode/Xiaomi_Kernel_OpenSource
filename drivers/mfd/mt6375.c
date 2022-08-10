// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 *
 * Author: ShuFan Lee <shufan_lee@richtek.com>
 */

#include <dt-bindings/mfd/mt6375.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/atomic.h>

#define MT6375_SLAVEID_TCPC	0x4E
#define MT6375_SLAVEID_PMU	0x34
#define MT6375_SLAVEID_BM	0x1A
#define MT6375_SLAVEID_HK1	0x4A
#define MT6375_SLAVEID_HK2	0x64
#define MT6375_SLAVEID_TM	0x3F

#define MT6375_VID		0x70
#define MT6375_REGADDR_SIZE	2
#define MT6375_REG_DEV_INFO	0x100
#define MT6375_REG_IRQ_SET	0x10D
#define MT6375_REG_CHG_IRQ0	0x1D0
#define MT6375_REG_PD_EVT	0x1DF
#define MT6375_REG_CHG_MSK0	0x1F0
#define MT6375_IRQ_REGS		(MT6375_REG_PD_EVT - MT6375_REG_CHG_IRQ0 + 1)

#define MT6375_REG_HK2_END	0x4FF
#define MT6375_BANK_TCPC_RT2	0xF2
#define MT6375_REG_RT2_START	0xF200
#define MT6375_REG_RT2_END	0xF2FF

#define MT6375_MSK_VID		0xF0
#define MT6375_MSK_CHIP_REV	0x0F

enum {
	MT6375_SLAVE_TCPC,
	MT6375_SLAVE_PMU,
	MT6375_SLAVE_BM,
	MT6375_SLAVE_HK1,
	MT6375_SLAVE_HK2,
	MT6375_SLAVE_TM,
	MT6375_SLAVE_MAX,
};

struct mt6375_data {
	struct device *dev;
	struct i2c_client *i2c[MT6375_SLAVE_MAX];
	struct regmap *rmap;
	struct irq_domain *domain;
	struct irq_chip irq_chip;
	struct mutex irq_lock;
	u8 mask_buf[MT6375_IRQ_REGS];
	u8 chip_rev;
	atomic_t in_sleep;
};

static const u8 mt6375_slave_addr[MT6375_SLAVE_MAX] = {
	MT6375_SLAVEID_TCPC,
	MT6375_SLAVEID_PMU,
	MT6375_SLAVEID_BM,
	MT6375_SLAVEID_HK1,
	MT6375_SLAVEID_HK2,
	MT6375_SLAVEID_TM
};

static inline struct i2c_client *bank_to_i2c(struct mt6375_data *ddata, u8 bank)
{
	if (bank >= MT6375_SLAVE_MAX && bank != MT6375_BANK_TCPC_RT2)
		return NULL;
	return (bank == MT6375_BANK_TCPC_RT2) ? ddata->i2c[MT6375_SLAVE_TCPC] :
						ddata->i2c[bank];
}

static int mt6375_regmap_write(void *context, const void *data, size_t count)
{
	int ret;
	struct mt6375_data *ddata = context;
	struct i2c_client *i2c;
	const u8 *_data = data;

	if (atomic_read(&ddata->in_sleep)) {
		dev_info(ddata->dev, "%s in sleep\n", __func__);
		return -EHOSTDOWN;
	}

	i2c = bank_to_i2c(ddata, _data[0]);
	if (!i2c)
		return -EINVAL;

	if (_data[0] == MT6375_BANK_TCPC_RT2) {
		ret = i2c_master_send(i2c, _data, count);
		if (ret < 0)
			return ret;
		return ret != count ? -EIO : 0;
	}
	return i2c_smbus_write_i2c_block_data(i2c, _data[1],
					      count - MT6375_REGADDR_SIZE,
					      _data + MT6375_REGADDR_SIZE);
}

static int mt6375_regmap_read(void *context, const void *reg_buf,
			      size_t reg_size, void *val_buf, size_t val_size)
{
	int ret;
	size_t len;
	struct mt6375_data *ddata = context;
	struct i2c_client *i2c;
	const u8 *_reg_buf = reg_buf;

	if (atomic_read(&ddata->in_sleep)) {
		dev_info(ddata->dev, "%s in sleep\n", __func__);
		return -EHOSTDOWN;
	}

	i2c = bank_to_i2c(ddata, _reg_buf[0]);
	if (!i2c)
		return -EINVAL;

	if (_reg_buf[0] == MT6375_BANK_TCPC_RT2) {
		u8 buf[2] = { _reg_buf[0], _reg_buf[1] };
		struct i2c_msg msg[2] = {
			{
				.addr = i2c->addr,
				.flags = i2c->flags,
				.len = 2,
				.buf = buf,
			}, {
				.addr = i2c->addr,
				.flags = i2c->flags | I2C_M_RD,
				.len = val_size,
				.buf = val_buf,
			},
		};
		len = 2;
		ret = i2c_transfer(i2c->adapter, msg, len);
	} else {
		len = val_size;
		ret = i2c_smbus_read_i2c_block_data(i2c, _reg_buf[1], len,
						    val_buf);
	}
	if (ret < 0)
		return ret;
	return ret != len ? -EIO : 0;
}

static const struct regmap_bus mt6375_regmap_bus = {
	.write = mt6375_regmap_write,
	.read = mt6375_regmap_read,
};

static bool mt6375_is_accessible_reg(struct device *dev, unsigned int reg)
{
	if (reg <= MT6375_REG_HK2_END ||
	    (reg >= MT6375_REG_RT2_START && reg <= MT6375_REG_RT2_END))
		return true;
	return false;
}

static const struct regmap_config mt6375_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.reg_format_endian = REGMAP_ENDIAN_BIG,
	.max_register = MT6375_REG_RT2_END,
	.writeable_reg = mt6375_is_accessible_reg,
	.readable_reg = mt6375_is_accessible_reg,
};

static void mt6375_irq_lock(struct irq_data *data)
{
	struct mt6375_data *ddata = irq_data_get_irq_chip_data(data);

	mutex_lock(&ddata->irq_lock);
}

static void mt6375_irq_sync_unlock(struct irq_data *data)
{
	struct mt6375_data *ddata = irq_data_get_irq_chip_data(data);
	int ret = 0;
	unsigned long idx = data->hwirq / 8;

	ret = regmap_write(ddata->rmap, MT6375_REG_CHG_MSK0 + idx,
			   ddata->mask_buf[idx]);
	if (ret)
		dev_err(ddata->dev, "failed to mask/unmask irq %d\n",
			data->hwirq);
	mutex_unlock(&ddata->irq_lock);
}

static void mt6375_irq_enable(struct irq_data *data)
{
	struct mt6375_data *ddata = irq_data_get_irq_chip_data(data);

	ddata->mask_buf[data->hwirq / 8] &= ~BIT(data->hwirq % 8);
}

static void mt6375_irq_disable(struct irq_data *data)
{
	struct mt6375_data *ddata = irq_data_get_irq_chip_data(data);

	ddata->mask_buf[data->hwirq / 8] |= BIT(data->hwirq % 8);
}

static int mt6375_irq_map(struct irq_domain *h, unsigned int virq,
			  irq_hw_number_t hwirq)
{
	struct mt6375_data *ddata = h->host_data;
	struct i2c_client *client = to_i2c_client(ddata->dev);

	irq_set_chip_data(virq, ddata);
	if (hwirq == MT6375_GM30_EVT)
		irq_set_chip_and_handler(virq, &ddata->irq_chip,
					 handle_simple_irq);
	else {
		irq_set_chip(virq, &ddata->irq_chip);
		irq_set_nested_thread(virq, 1);
	}
	irq_set_parent(virq, client->irq);
	irq_set_noprobe(virq);

	return 0;
}

static const struct irq_domain_ops mt6375_domain_ops = {
	.map = mt6375_irq_map,
	.xlate = irq_domain_xlate_onetwocell,
};

static irqreturn_t mt6375_irq_handler(int irq, void *data)
{
	struct mt6375_data *ddata = data;

	generic_handle_irq(irq_find_mapping(ddata->domain, MT6375_GM30_EVT));
	return IRQ_WAKE_THREAD;
}

static irqreturn_t mt6375_irq_thread(int irq, void *data)
{
	struct mt6375_data *ddata = data;
	u8 evt[MT6375_IRQ_REGS];
	bool handled = false;
	int i, j, ret;

	ret = regmap_bulk_read(ddata->rmap, MT6375_REG_CHG_IRQ0, evt,
			       MT6375_IRQ_REGS);
	if (ret) {
		dev_err(ddata->dev, "failed to read irq event\n");
		return IRQ_HANDLED;
	}

	/* ignore masked irq and ack */
	for (i = 0; i < MT6375_IRQ_REGS; i++)
		evt[i] &= ~ddata->mask_buf[i];
	ret = regmap_bulk_write(ddata->rmap, MT6375_REG_CHG_IRQ0, evt,
				MT6375_IRQ_REGS);
	if (ret < 0)
		dev_err(ddata->dev, "failed to ack irq status\n");

	/* handle irq */
	for (i = 0; i < MT6375_IRQ_REGS; i++) {
		if (!evt[i] || i == (MT6375_GM30_EVT / 8))
			continue;
		for (j = 0; j < 8; j++) {
			if (!(evt[i] & BIT(j)))
				continue;
			handle_nested_irq(irq_find_mapping(ddata->domain,
							   i * 8 + j));
			handled = true;
		}
	}

	return handled ? IRQ_HANDLED : IRQ_NONE;
}

static int mt6375_add_irq_chip(struct mt6375_data *ddata)
{
	int ret;
	struct i2c_client *client = to_i2c_client(ddata->dev);

	memset(ddata->mask_buf, 0xff, MT6375_IRQ_REGS);
	ret = regmap_bulk_write(ddata->rmap, MT6375_REG_CHG_MSK0,
				ddata->mask_buf, MT6375_IRQ_REGS);
	if (ret) {
		dev_err(ddata->dev, "failed to mask all irqs\n");
		return ret;
	}

	ret = regmap_bulk_write(ddata->rmap, MT6375_REG_CHG_IRQ0,
				ddata->mask_buf, MT6375_IRQ_REGS);
	if (ret) {
		dev_err(ddata->dev, "failed to clear all irqs\n");
		return ret;
	}

	ddata->irq_chip.name = dev_name(ddata->dev);
	ddata->irq_chip.irq_bus_lock = mt6375_irq_lock;
	ddata->irq_chip.irq_bus_sync_unlock = mt6375_irq_sync_unlock;
	ddata->irq_chip.irq_disable = mt6375_irq_disable;
	ddata->irq_chip.irq_enable = mt6375_irq_enable;
	ddata->irq_chip.flags = IRQCHIP_SKIP_SET_WAKE;

	ddata->domain = irq_domain_add_linear(ddata->dev->of_node,
					      MT6375_IRQ_REGS * 8,
					      &mt6375_domain_ops, ddata);
	if (!ddata->domain) {
		dev_err(ddata->dev, "failed to create irq domain\n");
		return -ENOMEM;
	}

	ret = devm_request_threaded_irq(ddata->dev, client->irq,
					mt6375_irq_handler, mt6375_irq_thread,
					IRQF_ONESHOT, dev_name(ddata->dev),
					ddata);
	if (ret) {
		dev_err(ddata->dev, "failed to request irq %d for %s\n",
			client->irq, dev_name(ddata->dev));
		irq_domain_remove(ddata->domain);
		return ret;
	}

	return 0;
}

static void mt6375_del_irq_chip(struct mt6375_data *ddata)
{
	unsigned int virq;
	int hwirq;

	for (hwirq = 0; hwirq < MT6375_IRQ_REGS * 8; hwirq++) {
		virq = irq_find_mapping(ddata->domain, hwirq);
		if (virq)
			irq_dispose_mapping(virq);
	}

	irq_domain_remove(ddata->domain);
}

static int mt6375_check_devid(struct mt6375_data *ddata)
{
	int ret;
	u8 vid;
	u32 val;

	ret = regmap_read(ddata->rmap, MT6375_REG_DEV_INFO, &val);
	if (ret < 0)
		return ret;
	vid = val & MT6375_MSK_VID;
	if (vid != MT6375_VID) {
		dev_err(ddata->dev, "vendor id 0x%02X is incorrect\n", vid);
		return -ENODEV;
	}
	ddata->chip_rev = val & MT6375_MSK_CHIP_REV;
	return 0;
}

static int mt6375_probe(struct i2c_client *client)
{
	int i, ret;
	struct mt6375_data *ddata;

	dev_info(&client->dev, "%s\n", __func__);
	ddata = devm_kzalloc(&client->dev, sizeof(*ddata), GFP_KERNEL);
	if (!ddata)
		return -ENOMEM;
	ddata->dev = &client->dev;
	mutex_init(&ddata->irq_lock);
	i2c_set_clientdata(client, ddata);
	atomic_set(&ddata->in_sleep, 0);

	for (i = 0; i < MT6375_SLAVE_MAX; i++) {
		if (i == MT6375_SLAVE_PMU) {
			ddata->i2c[i] = client;
			continue;
		}
		ddata->i2c[i] = devm_i2c_new_dummy_device(ddata->dev,
							  client->adapter,
							  mt6375_slave_addr[i]);
		if (IS_ERR(ddata->i2c[i])) {
			dev_err(&client->dev, "failed to new i2c(0x%02X)\n",
				mt6375_slave_addr[i]);
			ret = PTR_ERR(ddata->i2c[i]);
			goto err;
		}
	}

	ddata->rmap = devm_regmap_init(ddata->dev, &mt6375_regmap_bus, ddata,
				       &mt6375_regmap_config);
	if (IS_ERR(ddata->rmap)) {
		dev_err(ddata->dev, "failed to init regmap\n");
		ret = PTR_ERR(ddata->rmap);
		goto err;
	}

	ret = mt6375_check_devid(ddata);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to check device id\n");
		goto err;
	}

	ret = mt6375_add_irq_chip(ddata);
	if (ret < 0) {
		dev_err(ddata->dev, "failed to add irq chip\n");
		goto err;
	}
	return devm_of_platform_populate(ddata->dev);

err:
	mutex_destroy(&ddata->irq_lock);
	return ret;
}

static int mt6375_remove(struct i2c_client *client)
{
	struct mt6375_data *ddata = i2c_get_clientdata(client);

	mt6375_del_irq_chip(ddata);
	mutex_destroy(&ddata->irq_lock);
	return 0;
}

static int __maybe_unused mt6375_suspend(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);

	if (device_may_wakeup(dev))
		enable_irq_wake(i2c->irq);
	disable_irq(i2c->irq);
	return 0;
}

static int __maybe_unused mt6375_resume(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);

	enable_irq(i2c->irq);
	if (device_may_wakeup(dev))
		disable_irq_wake(i2c->irq);
	return 0;
}

static int mt6375_suspend_noirq(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct mt6375_data *data = i2c_get_clientdata(i2c);

	atomic_set(&data->in_sleep, 1);
	return 0;
}

static int mt6375_resume_noirq(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct mt6375_data *data = i2c_get_clientdata(i2c);

	atomic_set(&data->in_sleep, 0);
	return 0;
}

static const struct dev_pm_ops mt6375_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mt6375_suspend, mt6375_resume)
		SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(mt6375_suspend_noirq,
					      mt6375_resume_noirq)
};

static const struct of_device_id __maybe_unused mt6375_of_match[] = {
	{ .compatible = "mediatek,mt6375", },
	{ },
};
MODULE_DEVICE_TABLE(of, mt6375_of_match);

static struct i2c_driver mt6375_driver = {
	.probe_new = mt6375_probe,
	.remove = mt6375_remove,
	.driver = {
		.name = "mt6375",
		.pm = &mt6375_pm_ops,
		.of_match_table = of_match_ptr(mt6375_of_match),
	},
};
module_i2c_driver(mt6375_driver);

MODULE_AUTHOR("ShuFan Lee <shufan_lee@richtek.com>");
MODULE_DESCRIPTION("MT6375 Core I2C Drvier");
MODULE_LICENSE("GPL v2");
