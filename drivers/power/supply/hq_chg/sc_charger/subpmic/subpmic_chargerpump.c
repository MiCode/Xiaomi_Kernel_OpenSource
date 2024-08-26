// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2022 - 2023 SOUTHCHIP Semiconductor Technology(Shanghai) Co., Ltd.
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/types.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/usb/phy.h>
#include <linux/acpi.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/sched/prio.h>
#include <uapi/linux/sched/types.h>
#include <linux/platform_device.h>

#include "subpmic.h"
#include "../../charger_class/hq_cp_class.h"

#define SUBPMIC_CHARGERPUMP_VERSION           "1.0.0"

struct subpmic_chargerpump_device {
    struct i2c_client *client;
    struct device *dev;
    struct regmap *rmap;

    struct chargerpump_dev *chargerpump;
};

static int subpmic_chargerpump_bulk_read(struct subpmic_chargerpump_device *sc, u8 reg,
                                u8 *val,size_t count)
{
    int ret = 0;
    ret = regmap_bulk_read(sc->rmap, reg, val, count);
    if (ret < 0) {
        dev_err(sc->dev, "i2c bulk read failed\n");
    }
    return ret;
}

static int subpmic_chargerpump_bulk_write(struct subpmic_chargerpump_device *sc, u8 reg,
                                u8 *val, size_t count)
{
    int ret = 0;
    ret = regmap_bulk_write(sc->rmap, reg, val, count);
    if (ret < 0) {
        dev_err(sc->dev, "i2c bulk write failed\n");
    }
    return ret;
}

static int subpmic_chargerpump_write_byte(struct subpmic_chargerpump_device *sc, u8 reg,
                                                    u8 val)
{
    u8 temp = val;
    return subpmic_chargerpump_bulk_write(sc, reg, &temp, 1);
}

static int subpmic_chargerpump_read_byte(struct subpmic_chargerpump_device *sc, u8 reg,
                                                    u8 *val)
{
    return subpmic_chargerpump_bulk_read(sc, reg, val, 1);
}

static int subpmic_chargerpump_set_enable(struct chargerpump_dev *chargerpump, bool enable)
{
    struct subpmic_chargerpump_device *sc = chargerpump_get_private(chargerpump);
    int ret = 0;
    uint8_t val = 0;
    
    ret = subpmic_chargerpump_read_byte(sc, SUBPMIC_REG_CHG_CTRL, &val);

    if (enable) {
        subpmic_chargerpump_write_byte(sc, SUBPMIC_REG_CHG_CTRL, val | 0x40);
    } else {
        subpmic_chargerpump_write_byte(sc, SUBPMIC_REG_CHG_CTRL, val & (~0x40));
    }

    ret = subpmic_chargerpump_read_byte(sc, 0x64, &val);

    val = enable ? val | 0x01 : val & (~0x01);

    ret = subpmic_chargerpump_write_byte(sc, 0x64, val);
    return 0;
}

static int subpmic_chargerpump_get_is_enable(struct chargerpump_dev *chargerpump, bool *enable)
{
    struct subpmic_chargerpump_device *sc = chargerpump_get_private(chargerpump);
    int ret = 0;
    uint8_t val = 0;
    ret = subpmic_chargerpump_read_byte(sc, 0x66, &val);

    *enable = val & 0x01;
    return 0;
}

static int subpmic_chargerpump_get_status(struct chargerpump_dev *chargerpump, uint32_t *status)
{
    struct subpmic_chargerpump_device *sc = chargerpump_get_private(chargerpump);
    int ret = 0;
    uint8_t val = 0;
    ret = subpmic_chargerpump_read_byte(sc, 0x66, &val);
    *status = 0;
    if (val & BIT(1))
        *status |= CHARGERPUMP_ERROR_VBUS_LOW;
    if (val & BIT(2))
        *status |= CHARGERPUMP_ERROR_VBUS_HIGH;
    return 0;
}

static struct chargerpump_ops chargerpump_ops = {
    .set_enable = subpmic_chargerpump_set_enable,
    .get_status = subpmic_chargerpump_get_status,
    .get_is_enable = subpmic_chargerpump_get_is_enable,
};

static int subpmic_chargerpump_probe(struct platform_device *pdev)
{
    struct subpmic_chargerpump_device *sc;
    struct device *dev = &pdev->dev;
    int ret = 0;

    dev_info(dev, "%s (%s)\n", __func__, SUBPMIC_CHARGERPUMP_VERSION);

    sc = devm_kzalloc(dev, sizeof(*sc), GFP_KERNEL);
    if (!sc)
        return -ENOMEM;

    sc->rmap = dev_get_regmap(dev->parent, NULL);
    if (!sc->rmap) {
        dev_err(dev, "failed to get regmap\n");
        return -ENODEV;
    }
    sc->dev = dev;
    platform_set_drvdata(pdev, sc);

    sc->chargerpump = chargerpump_register("sc_chargerpump",
                             sc->dev, &chargerpump_ops, sc);
    if (!sc->chargerpump) {
        ret = PTR_ERR(sc->chargerpump);
        goto err;
    }

    dev_info(dev, "%s success probe\n", __func__);
    return 0;

err:
    return ret;
}

static int subpmic_chargerpump_remove(struct platform_device *pdev)
{
    return 0;
}

static const struct of_device_id sc6607_of_match[] = {
    {.compatible = "HUAQIN,subpmic_chargerpump",},
    {},
};
MODULE_DEVICE_TABLE(of, sc6607_of_match);

static struct platform_driver subpmic_chargerpump_driver = {
    .driver = {
        .name = "subpmic_chargerpump",
        .of_match_table = of_match_ptr(sc6607_of_match),
    },
    .probe = subpmic_chargerpump_probe,
    .remove = subpmic_chargerpump_remove,
};

module_platform_driver(subpmic_chargerpump_driver);

MODULE_AUTHOR("BSP3 <BSP3@huaqin.com>");
MODULE_DESCRIPTION("subpmic chargerpump core driver");
MODULE_LICENSE("GPL v2");
