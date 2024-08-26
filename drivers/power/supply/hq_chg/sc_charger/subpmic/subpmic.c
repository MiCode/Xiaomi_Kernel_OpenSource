// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2022 - 2023 SOUTHCHIP Semiconductor Technology(Shanghai) Co., Ltd.
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
#include <linux/of_gpio.h>

#include "subpmic_irq.h"

#define SUBPMIC_VERSION             "1.1.2"

#define SC6601_DEVICE_ID            0X66
#define SC6601_1P1_DEVICE_ID        0x61
#define SC6601A_DEVICE_ID           0x62

#define SUBPMIC_REG_HK_DID          0X00
#define SUBPMIC_REG_HK_IRQ          0x02
#define SUBPMIC_REG_HK_IRQ_MASK     0X03

enum {
    SUBPMIC_SLAVE_HK = 0,
    SUBPMIC_SLAVE_CHG = 0,
#ifdef CONFIG_SUBPMIC_6601
    SUBPMIC_SLAVE_LED = 0,
#endif /* CONFIG_SUBPMIC_6601 */
    SUBPMIC_SLAVE_DPDM = 0,
    SUBPMIC_SLAVE_VOOC = 0,
    SUBPMIC_SLAVE_UFCS,
#ifdef CONFIG_SUBPMIC_6607
    SUBPMIC_SLAVE_LED,
#endif /* CONFIG_SUBPMIC_6607 */
    SUBPMIC_SLAVE_MAX,
};

static const u8 subpmic_slave_addr[] = {
    0x61, 0x63, 0x64,
};

struct subpmic_device {
    struct device *dev;
    struct i2c_client *i2c[SUBPMIC_SLAVE_MAX];
    int irqn;
    struct regmap *rmap;
    struct irq_domain *domain;
    struct irq_chip irq_chip;
    struct mutex irq_lock;
    uint8_t irq_mask;

    atomic_t in_sleep;
    int sc6601_irq;
};

static inline struct i2c_client *bank_to_i2c(struct subpmic_device *sc, u8 bank)
{
    if (bank >= SUBPMIC_SLAVE_MAX)
        return NULL;
    return sc->i2c[bank];
}

static int subpmic_regmap_write(void *context, const void *data, size_t count)
{
    struct subpmic_device *sc = context;
    struct i2c_client *i2c;
    const u8 *_data = data;

    if (atomic_read(&sc->in_sleep)) {
        dev_info(sc->dev, "%s in sleep\n", __func__);
        return -EHOSTDOWN;
    }

    i2c = bank_to_i2c(sc, _data[0]);
    if (!i2c)
        return -EINVAL;

    return i2c_smbus_write_i2c_block_data(i2c, _data[1], count - 2, _data + 2);
}

static int subpmic_regmap_read(void *context, const void *reg_buf,
			      size_t reg_size, void *val_buf, size_t val_size)
{
    int ret;
    struct subpmic_device *sc = context;
    struct i2c_client *i2c;
    const u8 *_reg_buf = reg_buf;

    if (atomic_read(&sc->in_sleep)) {
        dev_info(sc->dev, "%s in sleep\n", __func__);
        return -EHOSTDOWN;
    }

    i2c = bank_to_i2c(sc, _reg_buf[0]);
    if (!i2c)
        return -EINVAL;

    ret = i2c_smbus_read_i2c_block_data(i2c, _reg_buf[1], val_size, val_buf);
    if (ret < 0)
        return ret;

    return ret != val_size ? -EIO : 0;
}

static const struct regmap_bus subpmic_regmap_bus = {
    .write = subpmic_regmap_write,
    .read = subpmic_regmap_read,
};

static const struct regmap_config subpmic_regmap_config = {
    .reg_bits = 16,
    .val_bits = 8,
    .reg_format_endian = REGMAP_ENDIAN_BIG,
};

static void subpmic_irq_lock(struct irq_data *data)
{
    struct subpmic_device *sc = irq_data_get_irq_chip_data(data);
    dev_info(sc->dev, "%s \n", __func__);
    mutex_lock(&sc->irq_lock);
}

static irqreturn_t subpmic_irq_thread(int irq, void *data);
static void subpmic_irq_sync_unlock(struct irq_data *data)
{
    struct subpmic_device *sc = irq_data_get_irq_chip_data(data);
    dev_info(sc->dev, "%s \n", __func__);
    regmap_bulk_write(sc->rmap, SUBPMIC_REG_HK_IRQ_MASK, &sc->irq_mask, 1);
    mutex_unlock(&sc->irq_lock);
}

static void subpmic_irq_enable(struct irq_data *data)
{
    struct subpmic_device *sc = irq_data_get_irq_chip_data(data);

    sc->irq_mask &= ~BIT(data->hwirq);
}

static void subpmic_irq_disable(struct irq_data *data)
{
    struct subpmic_device *sc = irq_data_get_irq_chip_data(data);

    sc->irq_mask |= BIT(data->hwirq);
}

static int subpmic_irq_map(struct irq_domain *h, unsigned int virq,
			  irq_hw_number_t hwirq)
{
    struct subpmic_device *sc = h->host_data;
    irq_set_chip_data(virq, sc);
    irq_set_chip(virq, &sc->irq_chip);
    irq_set_nested_thread(virq, 1);
    irq_set_parent(virq, sc->irqn);
    irq_set_noprobe(virq);
    return 0;
}

static const struct irq_domain_ops subpmic_domain_ops = {
    .map = subpmic_irq_map,
    .xlate = irq_domain_xlate_onetwocell,
};

static irqreturn_t subpmic_irq_thread(int irq, void *data)
{
    struct subpmic_device *sc = data;
    u8 evt = 0;
    bool handle = false;
    int i, ret;

    pm_stay_awake(sc->dev);

    ret = regmap_bulk_read(sc->rmap, SUBPMIC_REG_HK_IRQ, &evt, 1);
    if (ret) {
        dev_err(sc->dev, "failed to read irq event\n");
        return IRQ_HANDLED;
    }

    evt |= BIT(SUBPMIC_IRQ_HK);

    evt &= ~(sc->irq_mask);

    dev_info(sc->dev, "irq map -> %x, mask -> %x\n", evt, sc->irq_mask);

    for (i = 0; i < SUBPMIC_IRQ_MAX; i++) {
        if(evt & BIT(i)) {
            handle_nested_irq(irq_find_mapping(sc->domain, i));
            handle = true;
        }
    }

    pm_relax(sc->dev);
    return handle ? IRQ_HANDLED : IRQ_NONE;
}

static int subpmic_add_irq_chip(struct subpmic_device *sc)
{
    int ret = 0;
    int val;

    ret = regmap_bulk_read(sc->rmap, SUBPMIC_REG_HK_IRQ, &val, 1);
    if (ret < 0)
        return ret;
    sc->irq_mask = 0xff;
    ret = regmap_bulk_write(sc->rmap, SUBPMIC_REG_HK_IRQ_MASK, &sc->irq_mask, 1);

    sc->irq_chip.name = dev_name(sc->dev);
    sc->irq_chip.irq_disable = subpmic_irq_disable;
    sc->irq_chip.irq_enable = subpmic_irq_enable;
    sc->irq_chip.irq_bus_lock = subpmic_irq_lock;
    sc->irq_chip.irq_bus_sync_unlock = subpmic_irq_sync_unlock;

    sc->domain = irq_domain_add_linear(sc->dev->of_node,
                        SUBPMIC_IRQ_MAX, &subpmic_domain_ops, sc);
    if (!sc->domain) {
        dev_err(sc->dev, "failed to create irq domain\n");
        return -ENOMEM;
    }

    ret = devm_request_threaded_irq(sc->dev, sc->irqn,
                            NULL, subpmic_irq_thread,
                            IRQF_TRIGGER_RISING | IRQF_ONESHOT, dev_name(sc->dev), sc);
    if (ret) {
        dev_err(sc->dev, "failed to request irq %d for %s\n", sc->irqn, dev_name(sc->dev));
        irq_domain_remove(sc->domain);
        return ret;
    }

    dev_info(sc->dev, "subpmic irq = %d\n", sc->irqn);

    return 0;
}

static void subpmic_del_irq_chip(struct subpmic_device *sc)
{
    unsigned int virq;
    int hwirq;

    for (hwirq = 0; hwirq < SUBPMIC_IRQ_MAX; hwirq++) {
        virq = irq_find_mapping(sc->domain, hwirq);
    if (virq)
    irq_dispose_mapping(virq);
}

irq_domain_remove(sc->domain);
}

static int subpmic_check_chip(struct subpmic_device *sc)
{
    int ret;
    u8 did = 0;

    ret = regmap_bulk_read(sc->rmap, SUBPMIC_REG_HK_DID, &did, 1);
    if (ret < 0 || (did != SC6601_DEVICE_ID && did != SC6601_1P1_DEVICE_ID && did != SC6601A_DEVICE_ID)) {
        return -ENODEV;
    }
    return 0;
}

static int subpmic_probe(struct i2c_client *client)
{
    int i = 0, ret = 0;
    struct subpmic_device *sc;
    u8 evt = 0;

    dev_info(&client->dev, "%s (%s)\n", __func__, SUBPMIC_VERSION);

    sc = devm_kzalloc(&client->dev, sizeof(*sc), GFP_KERNEL);
    if (!sc)
        return -ENOMEM;
    sc->dev = &client->dev;
    i2c_set_clientdata(client, sc);
    mutex_init(&sc->irq_lock);
    atomic_set(&sc->in_sleep, 0);

    for (i = 0; i < SUBPMIC_SLAVE_MAX; i++) {
        if (i == SUBPMIC_SLAVE_CHG) {
            sc->i2c[i] = client;
            continue;
        }
        // kernel version different, need to check
#if 0
        sc->i2c[i] = i2c_new_dummy_device(client->adapter, subpmic_slave_addr[i]);
        if (IS_ERR(sc->i2c[i])) {
            dev_err(&client->dev, "failed to create new i2c[0x%02x] dev\n",
                                        subpmic_slave_addr[i]);
            ret = PTR_ERR(sc->i2c[i]);
            goto err;
        }
#endif
    }

    sc->rmap = devm_regmap_init(sc->dev, &subpmic_regmap_bus, sc,
                                    &subpmic_regmap_config);
    if (IS_ERR(sc->rmap)) {
        dev_err(sc->dev, "failed to init regmap\n");
        ret = PTR_ERR(sc->rmap);
        goto err;
    }
    // check id
    ret = subpmic_check_chip(sc);
    if (ret < 0) {
        dev_err(sc->dev, "failed to check device id\n");
        goto err;
    }

    sc->sc6601_irq = of_get_named_gpio(client->dev.of_node, "intr-gpio", 0);
    if (ret < 0) {
        dev_err(sc->dev, "%s no intr_gpio info\n", __func__);
        return ret;
    } else {
        dev_info(sc->dev, "%s intr_gpio infoi %d\n", __func__, sc->sc6601_irq);
    }
    ret = gpio_request(sc->sc6601_irq, "sc6601 irq pin");
    if (ret) {
        dev_err(sc->dev, "%s: %d gpio request failed\n", __func__, ret);
        return ret;
    }
    dev_info(sc->dev, "%s gpio_irq=%d\n", __func__, sc->sc6601_irq);
    sc->irqn = gpio_to_irq(sc->sc6601_irq);

    // clear flag
    ret = regmap_bulk_read(sc->rmap, SUBPMIC_REG_HK_IRQ, &evt, 1);
    // add irq
    ret = subpmic_add_irq_chip(sc);
    if (ret < 0) {
        dev_err(sc->dev, "failed to add irq chip\n");
        goto err;
    }

    enable_irq_wake(sc->irqn);
    device_init_wakeup(sc->dev, true);

    dev_info(&client->dev, "%s probe successfully!\n", __func__);

    return devm_of_platform_populate(sc->dev);
err:
    mutex_destroy(&sc->irq_lock);
    return ret;
}

static int subpmic_remove(struct i2c_client *client)
{
    struct subpmic_device *sc = i2c_get_clientdata(client);

    subpmic_del_irq_chip(sc);
    mutex_destroy(&sc->irq_lock);
    return 0;
}

static int subpmic_suspend(struct device *dev)
{
    struct i2c_client *i2c = to_i2c_client(dev);
    struct subpmic_device *sc = i2c_get_clientdata(i2c);
    if (device_may_wakeup(dev))
        enable_irq_wake(sc->irqn);
    disable_irq(sc->irqn);
    return 0;
}

static int subpmic_resume(struct device *dev)
{
    struct i2c_client *i2c = to_i2c_client(dev);
    struct subpmic_device *sc = i2c_get_clientdata(i2c);
    enable_irq(sc->irqn);
    if (device_may_wakeup(dev))
        disable_irq_wake(sc->irqn);
    return 0;
}

static int subpmic_suspend_noirq(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct subpmic_device *sc = i2c_get_clientdata(i2c);

	atomic_set(&sc->in_sleep, 1);
	return 0;
}

static int subpmic_resume_noirq(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct subpmic_device *sc = i2c_get_clientdata(i2c);

	atomic_set(&sc->in_sleep, 0);
	return 0;
}

static const struct dev_pm_ops subpmic_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(subpmic_suspend, subpmic_resume)
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(subpmic_suspend_noirq, subpmic_resume_noirq)
};

static const struct of_device_id subpmic_of_match[] = {
	{ .compatible = "southchip,subpmic_core",},
	{ },
};
MODULE_DEVICE_TABLE(of, subpmic_of_match);

static struct i2c_driver subpmic_driver = {
	.probe_new = subpmic_probe,
	.remove = subpmic_remove,
	.driver = {
		.name = "subpmic_core",
		.pm = &subpmic_pm_ops,
		.of_match_table = of_match_ptr(subpmic_of_match),
	},
};
module_i2c_driver(subpmic_driver);

MODULE_AUTHOR("Boqiang Liu <air-liu@southchip.com>");
MODULE_DESCRIPTION("Subpmic Core I2C Drvier");
MODULE_LICENSE("GPL v2");
