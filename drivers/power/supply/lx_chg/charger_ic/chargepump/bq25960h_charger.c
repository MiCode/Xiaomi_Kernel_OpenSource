// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2024 TI Semiconductor Technology(Shanghai) Co., Ltd.
*/
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/regmap.h>
#include <linux/iio/iio.h>
#include <linux/iio/consumer.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include "bq25960h_charger.h"
#include <linux/hardware_info.h>

#define BQ25960H_DRV_VERSION              "1.0.3F_G"

struct bq25960 {
    struct device *dev;
    struct i2c_client *client;
    struct regmap *regmap;
    int irq;
    int mode;
    bool charge_enabled;
    int usb_present;
    int vbus_volt;
    int ibus_curr;
    int vbat_volt;
    int ibat_curr;
    int die_temp;
    struct charger_device *chg_dev;
    const char *chg_dev_name;
    struct power_supply_desc psy_desc;
    struct power_supply_config psy_cfg;
    struct power_supply *psy;
    struct iio_dev          *indio_dev;
    struct iio_chan_spec    *iio_chan;
    struct iio_channel	*int_iio_chans;
    bool batt_present;
    bool vbus_present;
    struct mutex data_lock;
    int vbus_error;
    struct delayed_work set_rcp_work;
    struct delayed_work set_reverse_rcp_work;
};

static struct bq25960 *bq;
static struct mutex g_i2c_lock;

static int bq25960h_regmap_read(struct regmap *map, unsigned int reg, unsigned int *val)
{
    int ret;

    mutex_lock(&g_i2c_lock);
    ret = regmap_read(map, reg, val);
    if (ret < 0) {
        pr_err("bq25960h read field %d fail: %d\n", reg, ret);
    }
    mutex_unlock(&g_i2c_lock);

    return ret;
}

static void bq25960h_set_reverse_rcp_workfunc(struct work_struct *work)
{
    int ret = 0;

    ret = regmap_write(bq->regmap, 0xF1, 0x08);
    if (ret != 0) {
        dev_err(bq->dev, "%s regmap_write 0xF1 fail \n", __func__);
    }
    schedule_delayed_work(&bq->set_rcp_work, msecs_to_jiffies(500));
    dev_err(bq->dev, "%s end\n", __func__);
}

static void bq25960h_set_rcp_workfunc(struct work_struct *work)
{
    int ret = 0;

    ret = regmap_write(bq->regmap, 0x05, 0x9E);
    if (ret != 0) {
        dev_err(bq->dev, "%s regmap_write 0x05 fail \n", __func__);
    }

    dev_err(bq->dev, "%s end\n", __func__);
}

int bq25960h_set_otg_preconfigure(bool en)
{
    int ret = 0;

    if(en) {//i2c addr:0x3f
        ret = regmap_write(bq->regmap, 0xF9, 0x40);
        if (ret != 0) {
            dev_err(bq->dev, "%s regmap_write 0xF9 fail ret:%d\n", __func__, ret);
        }

        ret = regmap_write(bq->regmap, 0xFA, 0x31);
        if (ret != 0) {
           dev_err(bq->dev, "%s regmap_write 0xFA fail ret:%d\n", __func__, ret);
        }

        ret = regmap_write(bq->regmap, 0xA8, 0xD7);
        if (ret != 0) {
           dev_err(bq->dev, "%s regmap_write 0xA8 fail ret:%d\n", __func__, ret);
        }
    }

    return 0;
}
EXPORT_SYMBOL(bq25960h_set_otg_preconfigure);

int bq25960h_enable_otg(bool en)
{
    int ret = 0;
    int i = 5;
    int val = 0;

    dev_err(bq->dev, "%s I2C address:0x%x\n", __func__, bq->client->addr);

    if (en) {//i2c addr:0x3f
        mdelay(10);
        ret = regmap_write(bq->regmap, 0xA8, 0x00);
        if (ret != 0) {
            dev_err(bq->dev, "%s regmap_write 0xA8 fail \n", __func__);
        }

        ret = regmap_write(bq->regmap, 0x05, 0xAE);
        if (ret != 0) {
           dev_err(bq->dev, "%s regmap_write 0x05 fail \n", __func__);
        }

        ret = regmap_write(bq->regmap, 0x0F, 0x12);
        if (ret != 0) {
            dev_err(bq->dev, "%s regmap_write 0x0F fail \n", __func__);
        }

        mdelay(10);

        while (i--) {
            ret = regmap_write(bq->regmap, 0x0F, 0x12);
            if (ret != 0) {
                dev_err(bq->dev, "%s regmap_write 0x0F fail \n", __func__);
            }

            ret = bq25960h_regmap_read(bq->regmap, 0x0F, &val);
            if (ret == 0) {
                dev_err(bq->dev, "%s %d read reg 0X0F = 0x%02x\n", __func__, __LINE__, val);
            } else {
                dev_err(bq->dev, "%s %d read reg 0X0F error\n", __func__, __LINE__);
            }

            if (0x12 == val)
                break;

            mdelay(10);
        }

        ret = regmap_write(bq->regmap, 0xFA, 0x21);
        if (ret != 0) {
            dev_err(bq->dev, "%s regmap_write 0xFA fail \n", __func__);
        }

        ret = regmap_write(bq->regmap, 0x05, 0xBE);
        if (ret != 0) {
            dev_err(bq->dev, "%s regmap_write 0x05 fail \n", __func__);
        }
        schedule_delayed_work(&bq->set_reverse_rcp_work, msecs_to_jiffies(500));
    } else {
        ret = regmap_write(bq->regmap, 0x0F, 0x00);
        if (ret != 0) {
            dev_err(bq->dev, "%s regmap_write 0x0F fail \n", __func__);
        }
        ret = regmap_write(bq->regmap, 0x05, 0x0E);
        if (ret != 0) {
            dev_err(bq->dev, "%s regmap_write 0x05 fail \n", __func__);
        }
        ret = regmap_write(bq->regmap, 0xFA, 0x20);
        if (ret != 0) {
            dev_err(bq->dev, "%s regmap_write 0xFA fail \n", __func__);
        }
        ret = regmap_write(bq->regmap, 0xF9, 0x00);
        if (ret != 0) {
            dev_err(bq->dev, "%s regmap_write 0xF9 fail \n", __func__);
        }
        mdelay(10);
        ret = regmap_write(bq->regmap, 0xA0, 0x00);
        if (ret != 0) {
            dev_err(bq->dev, "%s regmap_write 0xA0 fail \n", __func__);
        }
    }

    return 0;
}
EXPORT_SYMBOL(bq25960h_enable_otg);

static const struct regmap_config bq25960h_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,
    .max_register = 0XFF,
};

static struct of_device_id bq25960h_charger_match_table[] = {
    { .compatible = "bq,bq25960h-standalone", },
    {},
};

#if 0
static int bq25960h_dump_reg(struct bq25960 *bq)
{
    int ret;
    int i;
    int val;

    for (i = 5; i <= 0x23; i++) {
        ret = bq25960h_regmap_read(bq->regmap, i, &val);
        if (ret != 0) {
            dev_info(bq->dev,"not in 3f\n");
            return 0;
        }
        dev_info(bq->dev, "%s reg[0x%02x] = 0x%02x\n",
                __func__, i, val);
    }
    return ret;
}
#endif

static int bq25960h_charger_probe(struct i2c_client *client,
                    const struct i2c_device_id *id)
{
    const struct of_device_id *match;
    struct device_node *node = client->dev.of_node;
    int ret;
    int val = 0;

    dev_err(&client->dev, "%s (%s)\n", __func__, BQ25960H_DRV_VERSION);

    bq = devm_kzalloc(&client->dev, sizeof(struct bq25960), GFP_KERNEL);
    if (!bq) {
        ret = -ENOMEM;
        goto err_kzalloc;
    }
    bq->dev = &client->dev;
    bq->client = client;
    bq->regmap = devm_regmap_init_i2c(client, &bq25960h_regmap_config);
    if (IS_ERR(bq->regmap)) {
        dev_err(bq->dev, "Failed to initialize regmap\n");
        ret = PTR_ERR(bq->regmap);
        goto err_regmap_init;
    }

    if (of_property_read_string(node, "charger_name", &bq->chg_dev_name) < 0) {
        bq->chg_dev_name = "charger";
        dev_err(bq->dev, "no charger name\n");
    }
    match = of_match_node(bq25960h_charger_match_table, node);
    if (match == NULL) {
        dev_err(bq->dev, "device tree match not found!\n");
        ret = -ENODEV;
        goto err_match_node;
    }
    i2c_set_clientdata(client, bq);
    mutex_init(&g_i2c_lock);
    INIT_DELAYED_WORK(&bq->set_reverse_rcp_work, bq25960h_set_reverse_rcp_workfunc);
    INIT_DELAYED_WORK(&bq->set_rcp_work, bq25960h_set_rcp_workfunc);
    ret = bq25960h_regmap_read(bq->regmap, 0x05, &val);
    if (ret!= 0) {
        dev_err(bq->dev, "%s detect device fail\n", __func__);
        ret = -ENODEV;
    } else {
        bq25960h_enable_otg(false);
    }

    hardwareinfo_set_prop(HARDWARE_SUB_CHARGER_MASTER, "chargepump");

    dev_err(bq->dev, "bq25960h probe successfully!\n");

    return 0;
err_kzalloc:
err_regmap_init:
err_match_node:
    mutex_destroy(&g_i2c_lock);
    dev_err(&client->dev,"bq25960h probe fail\n");
    return ret;
}

static int bq25960h_charger_remove(struct i2c_client *client)
{
    bq = i2c_get_clientdata(client);

    cancel_delayed_work_sync(&bq->set_rcp_work);
    power_supply_unregister(bq->psy);
    mutex_destroy(&g_i2c_lock);
    devm_kfree(&client->dev, bq);

    return 0;
}

static void bq25960h_charger_shutdown(struct i2c_client *client)
{
    bq = i2c_get_clientdata(client);
    mutex_destroy(&g_i2c_lock);
}

static struct i2c_driver bq25960h_charger_driver = {
    .driver     = {
        .name   = "bq25960h",
        .owner  = THIS_MODULE,
        .of_match_table = bq25960h_charger_match_table,
    },
    .probe      = bq25960h_charger_probe,
    .remove     = bq25960h_charger_remove,
    .shutdown	= bq25960h_charger_shutdown,
};

module_i2c_driver(bq25960h_charger_driver);
MODULE_DESCRIPTION("TI BQ25960H Driver");
MODULE_LICENSE("GPL");