// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
 */
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
#include <linux/of.h>
#include <linux/gpio.h>
#include <dt-bindings/subpmic/subpmic_chip.h>

#define SC6601_DEVICE_ID        0X66
#define SC6601_1P1_DEVICE_ID    0x61
#define SC6601A_DEVICE_ID       0x62
#define SC6601_REG_HK_DID       0X00
#define SC6601_REG_HK_IRQ       0x02
#define SC6601_REG_HK_IRQ_MASK  0X03

#define SUBPMIC_SC6601_DEVICE   1
#define SUBPMIC_NU6601_DEVICE   2
#define SUBPMIC_SY6976_DEVICE   3//uu422+ next version 3

#define SY6976_DEVICE_ID        0X66 //uu422 next version 0x67
#define SY6979_DEVICE_ID_V1     0x67 //uu674
#define SY6976_REG_HK_DID       0X00 //uu422 next version 0x00
#define SY6976_REG_HK_IRQ       0x02
#define SY6976_REG_HK_IRQ_MASK  0X03

enum {
    SC6601_SLAVE_HK = 0,
    SC6601_SLAVE_CHG = 0,
    SC6601_SLAVE_LED = 0,
    SC6601_SLAVE_DPDM = 0,
    SC6601_SLAVE_UFCS,

    NU6601_SLAVE_ADC,
    NU6601_SLAVE_CHG,
    NU6601_SLAVE_DPDM,

    SY6976_SLAVE_HK = 5,
    SY6976_SLAVE_CHG = 5,
    SY6976_SLAVE_LED = 5,
    SY6976_SLAVE_DPDM = 5,
    SY6976_SLAVE_UFCS,

    SUBPMIC_SLAVE_MAX,
};

static const u8 xm_subpmic_slave_addr[SUBPMIC_SLAVE_MAX] = {
	/* sc6601 i2c addr */
	0x61,
	0x63,

	/* nu6601 i2c addr */
	0x30,  //HK,ADC
	0x31,  //CHARGER LED
	0x32,  //DPDM

//uu420+ sy i2c addr
	//0x7d,
	//0x7f,
//uu420+.

//uu674
	0x3d,
	0x3f,
//uu674

};

struct xm_subpmic_device {
    struct device *dev;
	struct i2c_client *i2c[SUBPMIC_SLAVE_MAX];
	int irqn;
	struct regmap *rmap;
	struct irq_domain *domain;
	struct irq_chip irq_chip;
	struct mutex irq_lock;
	uint8_t irq_mask;
	atomic_t in_sleep;
	u8 devid;
};

#include "nu6601_base.h"

static inline struct i2c_client *bank_to_i2c(struct xm_subpmic_device *subpmic_dev, u8 bank)
{
	if (bank >= SUBPMIC_SLAVE_MAX)
		return NULL;
	return subpmic_dev->i2c[bank];
}

static int xm_subpmic_regmap_write(void *context, const void *data, size_t count)
{
	struct xm_subpmic_device *subpmic_dev = context;
	struct i2c_client *i2c;
	const u8 *_data = data;

	if (atomic_read(&subpmic_dev->in_sleep)) {
		dev_info(subpmic_dev->dev, "%s in sleep\n", __func__);
		return -EHOSTDOWN;
	}

	i2c = bank_to_i2c(subpmic_dev, _data[0]);
	if (!i2c)
		return -EINVAL;

	return i2c_smbus_write_i2c_block_data(i2c, _data[1], count - 2, _data + 2);
}

static int xm_subpmic_regmap_read(void *context, const void *reg_buf,
			      size_t reg_size, void *val_buf, size_t val_size)
{
	int ret;
	struct xm_subpmic_device *subpmic_dev = context;
	struct i2c_client *i2c;
	const u8 *_reg_buf = reg_buf;

	if (atomic_read(&subpmic_dev->in_sleep)) {
		dev_info(subpmic_dev->dev, "%s in sleep\n", __func__);
		return -EHOSTDOWN;
	}

	i2c = bank_to_i2c(subpmic_dev, _reg_buf[0]);
	if (!i2c)
		return -EINVAL;

	ret = i2c_smbus_read_i2c_block_data(i2c, _reg_buf[1], val_size, val_buf);
	if (ret < 0)
		return ret;

	return ret != val_size ? -EIO : 0;
}

static const struct regmap_bus xm_subpmic_regmap_bus = {
	.write = xm_subpmic_regmap_write,
	.read = xm_subpmic_regmap_read,
};

static const struct regmap_config xm_subpmic_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.reg_format_endian = REGMAP_ENDIAN_BIG,
};

void xm_subpmic_irq_lock(struct irq_data *data)
{
    struct xm_subpmic_device *subpmic_dev = irq_data_get_irq_chip_data(data);
    mutex_lock(&subpmic_dev->irq_lock);
}

void sc6601_irq_sync_unlock(struct irq_data *data)
{
    struct xm_subpmic_device *subpmic_dev = irq_data_get_irq_chip_data(data);
    regmap_bulk_write(subpmic_dev->rmap, SC6601_REG_HK_IRQ_MASK, &subpmic_dev->irq_mask, 1);
    mutex_unlock(&subpmic_dev->irq_lock);
}

void sy6976_irq_sync_unlock(struct irq_data *data)
{
    struct xm_subpmic_device *subpmic_dev = irq_data_get_irq_chip_data(data);
    regmap_bulk_write(subpmic_dev->rmap, SY6976_REG_HK_IRQ_MASK | (5 << 8), &subpmic_dev->irq_mask, 1);
    mutex_unlock(&subpmic_dev->irq_lock);
}

void xm_subpmic_irq_enable(struct irq_data *data)
{
    struct xm_subpmic_device *subpmic_dev = irq_data_get_irq_chip_data(data);

    subpmic_dev->irq_mask &= ~BIT(data->hwirq);
}

void xm_subpmic_irq_disable(struct irq_data *data)
{
    struct xm_subpmic_device *subpmic_dev = irq_data_get_irq_chip_data(data);

    subpmic_dev->irq_mask |= BIT(data->hwirq);
}

static int xm_subpmic_irq_map(struct irq_domain *h, unsigned int virq,
			  irq_hw_number_t hwirq)
{
	struct xm_subpmic_device *subpmic_dev = h->host_data;
	irq_set_chip_data(virq, subpmic_dev);
	irq_set_chip(virq, &subpmic_dev->irq_chip);
	irq_set_nested_thread(virq, 1);
	irq_set_parent(virq, subpmic_dev->irqn);
	irq_set_noprobe(virq);
	return 0;
}

static const struct irq_domain_ops xm_subpmic_domain_ops = {
    .map = xm_subpmic_irq_map,
    .xlate = irq_domain_xlate_onetwocell,
};

static irqreturn_t sc6601_irq_thread(int irq, void *data)
{
    struct xm_subpmic_device *subpmic_dev = data;
    u8 evt, mask;
    bool handle = false;
    int i, ret;
	// keep wakeup
    pm_wakeup_event(subpmic_dev->dev, 500);

	ret = regmap_bulk_read(subpmic_dev->rmap, SC6601_REG_HK_IRQ, &evt, 1);
	if (ret) {
		dev_err(subpmic_dev->dev, "failed to read irq event\n");
		return IRQ_HANDLED;
	}

    ret = regmap_bulk_read(subpmic_dev->rmap, SC6601_REG_HK_IRQ_MASK, &mask, 1);
    if (ret) {
		dev_err(subpmic_dev->dev, "failed to read irq mask\n");
		return IRQ_HANDLED;
	}

    evt |= BIT(SC6601_IRQ_HK);
    evt &= ~mask;

    dev_err(subpmic_dev->dev, "irq map -> %x\n", evt);

    for (i = 0; i < SC6601_IRQ_MAX; i++) {
        if(evt & BIT(i)) {
            handle_nested_irq(irq_find_mapping(subpmic_dev->domain, i));
            handle = true;
        }
    }

    return handle ? IRQ_HANDLED : IRQ_NONE;
}

static irqreturn_t sy6976_irq_thread(int irq, void *data)
{
    struct xm_subpmic_device *subpmic_dev = data;
    u8 evt, mask;
    bool handle = false;
    int i, ret;
	// keep wakeup
    pm_wakeup_event(subpmic_dev->dev, 500);

	ret = regmap_bulk_read(subpmic_dev->rmap, SY6976_REG_HK_IRQ | (5 << 8), &evt, 1);
	if (ret) {
		dev_err(subpmic_dev->dev, "failed to read irq event\n");
		return IRQ_HANDLED;
	}

    ret = regmap_bulk_read(subpmic_dev->rmap, SY6976_REG_HK_IRQ_MASK | (5 << 8), &mask, 1);
    if (ret) {
		dev_err(subpmic_dev->dev, "failed to read irq mask\n");
		return IRQ_HANDLED;
	}

    evt |= BIT(SC6601_IRQ_HK);
    evt &= ~mask;

    dev_err(subpmic_dev->dev, "irq map -> %x\n", evt);

    for (i = 0; i < SC6601_IRQ_MAX; i++) {
        if(evt & BIT(i)) {
            handle_nested_irq(irq_find_mapping(subpmic_dev->domain, i));
            handle = true;
        }
    }

    return handle ? IRQ_HANDLED : IRQ_NONE;
}

static int sc6601_irq_register(struct xm_subpmic_device *subpmic_dev)
{
    int ret = 0;
    int val;

    ret = regmap_bulk_read(subpmic_dev->rmap, SC6601_REG_HK_IRQ, &val, 1);
    if (ret < 0)
        return ret;
    subpmic_dev->irq_mask = 0xff;
    ret = regmap_bulk_write(subpmic_dev->rmap, SC6601_REG_HK_IRQ_MASK,
				&subpmic_dev->irq_mask, 1);
    subpmic_dev->irq_chip.name = dev_name(subpmic_dev->dev);
    subpmic_dev->irq_chip.irq_disable = xm_subpmic_irq_disable;
    subpmic_dev->irq_chip.irq_enable = xm_subpmic_irq_enable;
	subpmic_dev->irq_chip.irq_bus_lock = xm_subpmic_irq_lock;
	subpmic_dev->irq_chip.irq_bus_sync_unlock = sc6601_irq_sync_unlock;

    subpmic_dev->domain = irq_domain_add_linear(subpmic_dev->dev->of_node,
                        128, &xm_subpmic_domain_ops, subpmic_dev);
    if (!subpmic_dev->domain) {
        dev_err(subpmic_dev->dev, "failed to create irq domain\n");
        return -ENOMEM;
    }

    ret = devm_request_threaded_irq(subpmic_dev->dev, subpmic_dev->irqn,
                            NULL, sc6601_irq_thread,
                            IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                            dev_name(subpmic_dev->dev), subpmic_dev);
    if (ret) {
        dev_err(subpmic_dev->dev, "failed to request irq %d for %s\n",
			subpmic_dev->irqn, dev_name(subpmic_dev->dev));
		irq_domain_remove(subpmic_dev->domain);
		return ret;
    }

    dev_info(subpmic_dev->dev, "sc6601 irq = %d\n", subpmic_dev->irqn);

    return 0;
}

static int nu6601_irq_register(struct xm_subpmic_device *subpmic_dev)
{
	int ret = 0;

	ret = nu6601_irq_init(subpmic_dev);
	if (ret < 0)
		return ret;

	subpmic_dev->irq_chip.name = dev_name(subpmic_dev->dev);
    subpmic_dev->irq_chip.irq_disable = nu6601_irq_disable;
    subpmic_dev->irq_chip.irq_enable = nu6601_irq_enable;
	subpmic_dev->irq_chip.irq_bus_lock = nu6601_irq_bus_lock;
	subpmic_dev->irq_chip.irq_bus_sync_unlock = nu6601_irq_bus_unlock;

	subpmic_dev->domain = irq_domain_add_linear(subpmic_dev->dev->of_node,
						 NU6601_IRQ_EVT_MAX,
						 &xm_subpmic_domain_ops,
						 subpmic_dev);
	if (!subpmic_dev->domain) {
        dev_err(subpmic_dev->dev, "failed to create irq domain\n");
        return -ENOMEM;
	}

	ret = devm_request_threaded_irq(subpmic_dev->dev, subpmic_dev->irqn, NULL,
					nu6601_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"nu6601_irq", subpmic_dev);
    if (ret) {
        dev_err(subpmic_dev->dev, "failed to request irq %d for %s\n",
			subpmic_dev->irqn, "nu6601_irq");
		irq_domain_remove(subpmic_dev->domain);
		return ret;
	}

    dev_info(subpmic_dev->dev, "nu6601 irq = %d\n", subpmic_dev->irqn);

	return 0;
}

static int sy6976_irq_register(struct xm_subpmic_device *subpmic_dev)
{
    int ret = 0;
    int val;

    ret = regmap_bulk_read(subpmic_dev->rmap, SY6976_REG_HK_IRQ | (5 << 8), &val, 1);
    if (ret < 0)
        return ret;
    subpmic_dev->irq_mask = 0xff;
    ret = regmap_bulk_write(subpmic_dev->rmap, SY6976_REG_HK_IRQ_MASK | (5 << 8),
				&subpmic_dev->irq_mask, 1);
    subpmic_dev->irq_chip.name = dev_name(subpmic_dev->dev);
    subpmic_dev->irq_chip.irq_disable = xm_subpmic_irq_disable;
    subpmic_dev->irq_chip.irq_enable = xm_subpmic_irq_enable;
	subpmic_dev->irq_chip.irq_bus_lock = xm_subpmic_irq_lock;
	subpmic_dev->irq_chip.irq_bus_sync_unlock = sy6976_irq_sync_unlock;

    subpmic_dev->domain = irq_domain_add_linear(subpmic_dev->dev->of_node,
                        SC6601_IRQ_MAX, &xm_subpmic_domain_ops, subpmic_dev);
    if (!subpmic_dev->domain) {
        dev_err(subpmic_dev->dev, "failed to create irq domain\n");
        return -ENOMEM;
    }

    ret = devm_request_threaded_irq(subpmic_dev->dev, subpmic_dev->irqn,
                            NULL, sy6976_irq_thread,
                            IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                            dev_name(subpmic_dev->dev), subpmic_dev);
    if (ret) {
        dev_err(subpmic_dev->dev, "failed to request irq %d for %s\n",
			subpmic_dev->irqn, dev_name(subpmic_dev->dev));
		irq_domain_remove(subpmic_dev->domain);
		return ret;
    }

    dev_info(subpmic_dev->dev, "sy6976 irq = %d\n", subpmic_dev->irqn);

    return 0;
}

static int xm_subpmic_check_chip(struct xm_subpmic_device *subpmic_dev)
{
    int ret;
    u8 did = 0;
    ret = regmap_bulk_read(subpmic_dev->rmap, SC6601_REG_HK_DID, &did, 1);
    if (ret == 0 && (did == SC6601_DEVICE_ID || did == SC6601_1P1_DEVICE_ID ||
			did == SC6601A_DEVICE_ID)) {
        subpmic_dev->devid = SUBPMIC_SC6601_DEVICE;
        dev_err(subpmic_dev->dev, "find device sc6601: 0x%x \n", did);
        return 0;
    }

    ret = regmap_bulk_read(subpmic_dev->rmap, NU6601_REG_DEVICE_ID | (2 << 8), &did, 1);
    if (ret == 0 && did == NU6601_DEVICE_ID) {
        subpmic_dev->devid = SUBPMIC_NU6601_DEVICE;
        dev_err(subpmic_dev->dev, "find device nu6601: 0x%x \n", did);
        return 0;
    }

    ret = regmap_bulk_read(subpmic_dev->rmap, SY6976_REG_HK_DID | (5 << 8), &did, 1);
    if (ret == 0 && did == SY6976_DEVICE_ID) {
        subpmic_dev->devid = SUBPMIC_SY6976_DEVICE;
        dev_err(subpmic_dev->dev, "find device sy6976: 0x%x \n", did);
        return 0;
    }
    if (ret == 0 && did == SY6979_DEVICE_ID_V1) { //uu674 add new id
        subpmic_dev->devid = SUBPMIC_SY6976_DEVICE;
        dev_err(subpmic_dev->dev, "find device sy6976 V1: 0x%x \n", did);
        return 0;
    }
    dev_err(subpmic_dev->dev, "find device chip id: 0x%x \n", did);//uu675

    return -ENODEV;
}

static int xm_subpmic_probe(struct i2c_client *client)
{
    int i = 0, ret = 0;
    struct xm_subpmic_device *subpmic_dev;
    u8 evt = 0;

    pr_err("%s: Start\n", __func__);

    subpmic_dev = devm_kzalloc(&client->dev, sizeof(*subpmic_dev), GFP_KERNEL);
    if (!subpmic_dev)
        return -ENOMEM;
    subpmic_dev->dev = &client->dev;
    i2c_set_clientdata(client, subpmic_dev);
    mutex_init(&subpmic_dev->irq_lock);
    atomic_set(&subpmic_dev->in_sleep, 0);
    subpmic_dev->irqn = client->irq;

    for (i = 0; i < SUBPMIC_SLAVE_MAX; i++) {
        if (i == SC6601_SLAVE_CHG) {
            subpmic_dev->i2c[i] = client;
            continue;
        }
        subpmic_dev->i2c[i] = devm_i2c_new_dummy_device(subpmic_dev->dev,
							client->adapter, xm_subpmic_slave_addr[i]);
        if (IS_ERR(subpmic_dev->i2c[i])) {
            dev_err(&client->dev, "failed to create new i2c[0x%02x] dev\n",
                                        xm_subpmic_slave_addr[i]);
            ret = PTR_ERR(subpmic_dev->i2c[i]);
            goto err;
        }
    }

    subpmic_dev->rmap = devm_regmap_init(subpmic_dev->dev, &xm_subpmic_regmap_bus,
						subpmic_dev, &xm_subpmic_regmap_config);
    if (IS_ERR(subpmic_dev->rmap)) {
        dev_err(subpmic_dev->dev, "failed to init regmap\n");
        ret = PTR_ERR(subpmic_dev->rmap);
        goto err;
    }

    ret = xm_subpmic_check_chip(subpmic_dev);
	if (ret < 0) {
		dev_err(subpmic_dev->dev, "failed to check device id\n");
		goto err;
	}

	if (subpmic_dev->devid == SUBPMIC_SC6601_DEVICE) {
		// clear flag
		ret = regmap_bulk_read(subpmic_dev->rmap, SC6601_REG_HK_IRQ, &evt, 1);
		ret = sc6601_irq_register(subpmic_dev);
		if (ret < 0) {
			dev_err(subpmic_dev->dev, "failed to add sc6601 irq chip\n");
			goto err;
		}
	} else if (subpmic_dev->devid == SUBPMIC_NU6601_DEVICE) {
		ret = nu6601_irq_register(subpmic_dev);
		if (ret < 0) {
			dev_err(subpmic_dev->dev, "failed to add nu6601 irq chip\n");
			goto err;
		}
	} else if (subpmic_dev->devid == SUBPMIC_SY6976_DEVICE) {
		ret = sy6976_irq_register(subpmic_dev);
		if (ret < 0) {
			dev_err(subpmic_dev->dev, "failed to add sy6976 irq chip\n");
			goto err;
		}
	}

	enable_irq_wake(subpmic_dev->irqn);
	device_init_wakeup(subpmic_dev->dev, true);

	pr_err("%s: End\n", __func__);

	return devm_of_platform_populate(subpmic_dev->dev);
err:
	mutex_destroy(&subpmic_dev->irq_lock);
	return ret;
}

static void xm_subpmic_del_irq_chip(struct xm_subpmic_device *subpmic_dev)
{
    unsigned int virq;
	int hwirq;

    for (hwirq = 0; hwirq < SC6601_IRQ_MAX; hwirq++) {
		virq = irq_find_mapping(subpmic_dev->domain, hwirq);
		if (virq)
			irq_dispose_mapping(virq);
	}

	irq_domain_remove(subpmic_dev->domain);
}

static int xm_subpmic_remove(struct i2c_client *client)
{
	struct xm_subpmic_device *subpmic_dev = i2c_get_clientdata(client);

    xm_subpmic_del_irq_chip(subpmic_dev);
    mutex_destroy(&subpmic_dev->irq_lock);
    return 0;
}

static int xm_subpmic_suspend(struct device *dev)
{
    struct i2c_client *i2c = to_i2c_client(dev);
    struct xm_subpmic_device *subpmic_dev = i2c_get_clientdata(i2c);
    if (device_may_wakeup(dev))
        enable_irq_wake(subpmic_dev->irqn);
    disable_irq(subpmic_dev->irqn);
    return 0;
}

static int xm_subpmic_resume(struct device *dev)
{
    struct i2c_client *i2c = to_i2c_client(dev);
    struct xm_subpmic_device *subpmic_dev = i2c_get_clientdata(i2c);
    enable_irq(subpmic_dev->irqn);
    if (device_may_wakeup(dev))
        disable_irq_wake(subpmic_dev->irqn);
    return 0;
}

static int xm_subpmic_suspend_noirq(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct xm_subpmic_device *subpmic_dev = i2c_get_clientdata(i2c);

	atomic_set(&subpmic_dev->in_sleep, 1);
	return 0;
}

static int xm_subpmic_resume_noirq(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct xm_subpmic_device *subpmic_dev = i2c_get_clientdata(i2c);

	atomic_set(&subpmic_dev->in_sleep, 0);
	return 0;
}

static const struct dev_pm_ops xm_subpmic_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(xm_subpmic_suspend, xm_subpmic_resume)
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(xm_subpmic_suspend_noirq, xm_subpmic_resume_noirq)
};

static const struct of_device_id xm_subpmic_of_match[] = {
	{ .compatible = "xiaomi,subpmic-sc6601", },
	{ },
};

static struct i2c_driver xm_subpmic_driver = {
	.probe_new = xm_subpmic_probe,
	.remove = xm_subpmic_remove,
	.driver = {
		.name = "xm_subpmic",
		.pm = &xm_subpmic_pm_ops,
		.of_match_table = of_match_ptr(xm_subpmic_of_match),
	},
};
module_i2c_driver(xm_subpmic_driver);

MODULE_AUTHOR("Boqiang Liu <air-liu@southchip.com>");
MODULE_DESCRIPTION("Xiaomi Sub Pmic I2C Drvier");
MODULE_LICENSE("GPL v2");
