/*
 * KTZ Semiconductor KTZ8866 LED Driver
 *
 * Copyright (C) 2013 Ideas on board SPRL
 *
 * Contact: Chenzilong  <chenzilong1@xiaomi.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/platform_data/dualktz8866.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#define u8  unsigned char

enum {
    KTZ8866_A = 0,
    KTZ8866_B,
};

struct ktz8866 {
    u8 chip;
    struct i2c_client *client;
    struct backlight_device *backlight;
    struct ktz8866_platform_data *pdata;
};

struct ktz8866 *bd;
struct ktz8866_status ktz8866_status;
struct pwm_to_51 pwm_map[6] = {
	{10, 0x199},
	{20, 0x333},
	{40, 0x666},
	{60, 0x999},
	{80, 0xccc},
	{100, 0xFFF},
};

struct ktz8866 *bd_a;
struct ktz8866 *bd_b;
char gresult[30];
int caseid = 0;

static struct ktz8866_led g_ktz8866_led;

extern int mi_mipi_dsi_dcs_set_pwm_value(u16 dbv_value);

int ktz8866_read(u8 reg, u8 *data)
{
    int ret;

    ret = i2c_smbus_read_byte_data(bd->client, reg);
    if (ret < 0) {
        dev_err(&bd->client->dev, "failed reading at 0x%02x\n", reg);
        return ret;
    }

    *data = (uint8_t)ret;

    return 0;
}

int ktz8866_write(u8 reg, u8 data)
{
    return i2c_smbus_write_byte_data(bd->client, reg, data);
}

int ktz8866_reads(struct ktz8866 *bd, u8 reg, u8 *data)
{
    int ret;

    ret = i2c_smbus_read_byte_data(bd->client, reg);
    if (ret < 0) {
        dev_err(&bd->client->dev, "failed reading at 0x%02x\n", reg);
        return ret;
    }

    *data = (uint8_t)ret;

    return 0;
}

int ktz8866_writes(struct ktz8866 *bd, u8 reg, u8 data)
{
    return i2c_smbus_write_byte_data(bd->client, reg, data);
}

static int pwm_to_51(int pwm)
{
    int i;
    for(i=0;i<6;i++) {
	if (pwm == pwm_map[i].pwm)
            break;
    }

    return pwm_map[i].bl_value;
}

static int ktz8866_case1_test(int pwm, char *result)
{
    int bl;
    struct pwm_reg pwm_reg_a;
    struct pwm_reg pwm_reg_b;
    u16 pwm_digital_value_a;
    u16 pwm_digital_value_b;

    bl = pwm_to_51(pwm);
    mi_mipi_dsi_dcs_set_pwm_value(bl);

    mdelay(500);
    ktz8866_reads(bd_a, 0x12, &pwm_reg_a.lbyte);
    ktz8866_reads(bd_a, 0x13, &pwm_reg_a.hbyte);

    ktz8866_reads(bd_b, 0x12, &pwm_reg_b.lbyte);
    ktz8866_reads(bd_b, 0x13, &pwm_reg_b.hbyte);

    pwm_digital_value_a = pwm_reg_a.hbyte << 8 | pwm_reg_a.lbyte;
    pwm_digital_value_b = pwm_reg_b.hbyte << 8 | pwm_reg_b.lbyte;

    sprintf(result, "case1_%d_%d",pwm_digital_value_a, pwm_digital_value_b);
    printk("bl_selftest:  ktz8866_case1_test result: %s\n", result);

    return 0;
}

static int ktz8866_case2_test(int pwm, char *result)
{
    int bl;
    struct pwm_reg pwm_reg_a;
    struct pwm_reg pwm_reg_b;
    ktime_t time_a;
    ktime_t time_b;
    ktime_t time_c;
    u64 diff_a;
    u64 diff_b;

    bl = pwm_to_51(pwm);
    mi_mipi_dsi_dcs_set_pwm_value(bl);

    mdelay(128);

    time_a = ktime_get();
    ktz8866_reads(bd_a, 0x12, &pwm_reg_a.lbyte);
    ktz8866_reads(bd_a, 0x13, &pwm_reg_a.hbyte);
    time_b = ktime_get();
    ktz8866_reads(bd_b, 0x12, &pwm_reg_b.lbyte);
    ktz8866_reads(bd_b, 0x13, &pwm_reg_b.hbyte);
    time_c = ktime_get();

    diff_a = (u64)ktime_us_delta(time_b, time_a);
    diff_b = (u64)ktime_us_delta(time_c, time_b);

    sprintf(result, "case2_%d_%d",diff_a, diff_b);

    printk("bl_selftest:  ktz8866_case2_test result: %s\n", result);
    return 0;
}

static ssize_t bl_selftest_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    int cnt = strlen(gresult);

    if (*pos != 0)
        return 0;

    if (copy_to_user(buf, gresult, cnt)) {
        printk("Failed to copy data to user space\n");
        return 0;
    }

    *pos += cnt;

    return cnt;
}

ssize_t bl_selftest_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
    char tmp[6] = {0};
    int pwm_value = 0;

    if (copy_from_user(tmp, buf, count)) {
        printk("Failed to copy data from user space\n");
        goto out;
    }

    if (!strncmp(tmp, "case1", 5))
        caseid = 0;
    else if (!strncmp(tmp, "case2", 5))
        caseid = 1;
    else {
        kstrtoint(tmp, 10, &pwm_value);
        switch (caseid) {
        case 0:
            ktz8866_case1_test(pwm_value, gresult);
            break;
        case 1:
            ktz8866_case2_test(pwm_value, gresult);
            break;
        }
    }

out:
	return count;
}

static const struct file_operations bl_selftest_fops = {
	.read = bl_selftest_read,
	.write = bl_selftest_write,
};

static int ktz8866_backlight_update_status(struct backlight_device *backlight)
{
    struct ktz8866 *bd = bl_get_data(backlight);
    int exponential_bl = backlight->props.brightness;
    int brightness = 0;
    u8 v[2];

    brightness = bl_level_remap[exponential_bl];
    /* brightness = exponential_bl; */

    if (brightness < 0 || brightness > BL_LEVEL_MAX)
        return 0;

    mutex_lock(&g_ktz8866_led.lock);

    dev_warn(&bd->client->dev,
        "ktz8866 backlight 0x%02x ,exponential brightness %d \n", brightness, exponential_bl);

    if (brightness > 0) {
        ktz8866_writes(bd, KTZ8866_DISP_BL_ENABLE, 0x4f);
        dev_warn(&bd->client->dev, "ktz8866 backlight enable,dimming close");
    } else if (brightness == 0) {
        ktz8866_writes(bd, KTZ8866_DISP_BL_ENABLE, 0x0f);
        // usleep_range((10 * 1000),(10 * 1000) + 10);
        dev_warn(&bd->client->dev, "ktz8866 backlight disable,dimming close");
    }

    v[0] = brightness & 0x7;
    v[1] = (brightness >> 3) & 0xff;

    ktz8866_writes(bd, KTZ8866_DISP_BB_LSB, v[0]);
    ktz8866_writes(bd, KTZ8866_DISP_BB_MSB, v[1]);

    g_ktz8866_led.level = brightness;

    mutex_unlock(&g_ktz8866_led.lock);
    return 0;
}

static int ktz8866_backlight_get_brightness(struct backlight_device *backlight)
{
    //struct ktz8866 *bd = bl_get_data(backlight);
    int brightness = backlight->props.brightness;
    u8 v[2];
    mutex_lock(&g_ktz8866_led.lock);

    ktz8866_read(0x5, &v[0]);
    ktz8866_read(0x4, &v[1]);

    brightness = (v[1] << 8)+v[0];

    mutex_unlock(&g_ktz8866_led.lock);
    return brightness;
}

static const struct backlight_ops ktz8866_backlight_ops = {
    .options    = BL_CORE_SUSPENDRESUME,
    .update_status  = ktz8866_backlight_update_status,
    .get_brightness = ktz8866_backlight_get_brightness,
};


static int parse_dt(struct device *dev, struct ktz8866_platform_data *pdata)
{
    struct device_node *np = dev->of_node;

    pdata->hw_en_gpio = of_get_named_gpio_flags(np, "ktz8866,hwen-gpio", 0, NULL);

    return 0;
}

static int ktz8866_probe(struct i2c_client *client,
              const struct i2c_device_id *id)
{
    //struct ktz8866_platform_data *pdata = dev_get_platdata(&client->dev);
    struct backlight_device *backlight;
    struct backlight_properties props;
    int ret = 0;
    u8 read;

    bd = devm_kzalloc(&client->dev, sizeof(*bd), GFP_KERNEL);
    if (!bd)
        return -ENOMEM;
    dev_warn(&client->dev,"ktz8866 bd = devm_kzalloc \n");

    bd->pdata = devm_kzalloc(&client->dev, sizeof(struct ktz8866_platform_data), GFP_KERNEL);
    if (!bd->pdata)
        return -ENOMEM;
    dev_warn(&client->dev,"bd->pdata = devm_kzalloc \n");

    bd->client = client;
    bd->chip = id->driver_data;

    dev_warn(&client->dev,
        "ktz8866 probe I2C adapter support I2C_FUNC_SMBUS_BYTE\n");

    if (!i2c_check_functionality(client->adapter,I2C_FUNC_SMBUS_BYTE_DATA)) {
        dev_warn(&client->dev,"ktz8866 I2C adapter doesn't support I2C_FUNC_SMBUS_BYTE\n");
        return -EIO;
    }
    dev_warn(&client->dev,
        "ktz8866 i2c_check_functionality I2C adapter support I2C_FUNC_SMBUS_BYTE\n");

    mutex_init(&g_ktz8866_led.lock);

    memset(&props, 0, sizeof(props));
    if (bd->chip == KTZ8866_A ){
        bd->client->dev.init_name = "KTZ8866A";
    }else{
        bd->client->dev.init_name = "KTZ8866B";
    }
    props.type = BACKLIGHT_RAW;
    props.max_brightness = 2047;
    props.brightness = clamp_t(unsigned int, 98, 16,props.max_brightness);
    dev_warn(&client->dev,"ktz8866 devm_backlight_device_register \n");
    backlight = devm_backlight_device_register(&client->dev,
                          dev_name(&client->dev),
                          &bd->client->dev, bd,
                          &ktz8866_backlight_ops, &props);
    if (IS_ERR(backlight)) {
        dev_err(&client->dev, "ktz8866 failed to register backlight\n");
        return PTR_ERR(backlight);
    }
    // dev_warn(&client->dev,"ktz8866 backlight_update_status \n");
    // backlight_update_status(backlight);
    dev_warn(&client->dev,"ktz8866 i2c_set_clientdata \n");
    i2c_set_clientdata(client, backlight);

    parse_dt(&client->dev, bd->pdata);
    dev_warn(&client->dev,"ktz8866 parse_dt \n");

    if (bd->chip == KTZ8866_A ){
        dev_warn(&client->dev,"ktz8866 ktz8866_probe KTZ8866_LCD_DRV_HW_EN\n");
        ret = devm_gpio_request_one(&client->dev, bd->pdata->hw_en_gpio,
                        GPIOF_DIR_OUT | GPIOF_INIT_HIGH, "HW_EN");
        if (ret < 0) {
            dev_err(&client->dev, "unable to request L81A HW_EN GPIO\n");
            return ret;
        }
    }

    ktz8866_read(KTZ8866_DISP_FLAGS, &read);
    dev_err(&bd->client->dev, "ktz8866 reading 0x%02x is 0x%02x\n", KTZ8866_DISP_FLAGS, read);

    if (bd->chip == KTZ8866_A) {
        ktz8866_status.ktz8866a_init = true;
        bd_a = bd;
    } else if (bd->chip == KTZ8866_B) {
        ktz8866_status.ktz8866b_init = true;
        bd_b = bd;
    }

    if (ktz8866_status.ktz8866a_init == true && ktz8866_status.ktz8866b_init == true) {
        dev_info(&client->dev,"ktz8866a and ktz8866b init success create test node\n");
        proc_create("bl_selftest", 0644, NULL, &bl_selftest_fops);
    }

    return ret;
}

static int ktz8866_remove(struct i2c_client *client)
{
    struct backlight_device *backlight = i2c_get_clientdata(client);

    backlight->props.brightness = 0;
    backlight_update_status(backlight);

    return 0;
}

static const struct i2c_device_id ktz8866_ids[] = {
    { "ktz8866a", 0 },
    { "ktz8866b", 1 },
};
MODULE_DEVICE_TABLE(i2c, ktz8866_ids);

static struct of_device_id ktz8866_match_table[] = {
    { .compatible = "ktz,ktz8866a",},
    { .compatible = "ktz,ktz8866b",},
    { },
};

static struct i2c_driver ktz8866_driver = {
    .driver = {
        .name = "ktz8866",
        .owner = THIS_MODULE,
        .of_match_table = ktz8866_match_table,
    },
    .probe = ktz8866_probe,
    .remove = ktz8866_remove,
    .id_table = ktz8866_ids,
};

module_i2c_driver(ktz8866_driver);

MODULE_DESCRIPTION("chenzilong ktz8866 Backlight Driver");
MODULE_AUTHOR("Laurent Pinchart <chenzilong1@xiaomi.com>");
MODULE_LICENSE("GPL");
