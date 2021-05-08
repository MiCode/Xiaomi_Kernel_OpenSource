/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * Version: v1.0.1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/leds-aw2023.h>

/* register address */
#define AW2023_REG_RESET				0x00
#define AW2023_REG_GCR1					0x01
#define AW2023_REG_STATUS				0x02
#define AW2023_REG_PATST				0x03
#define AW2023_REG_GCR2					0x04
#define AW2023_REG_LEDEN				0x30
#define AW2023_REG_LCFG0				0x31
#define AW2023_REG_LCFG1				0x32
#define AW2023_REG_LCFG2				0x33
#define AW2023_REG_PWM0					0x34
#define AW2023_REG_PWM1					0x35
#define AW2023_REG_PWM2					0x36
#define AW2023_REG_LED0T0				0x37
#define AW2023_REG_LED0T1				0x38
#define AW2023_REG_LED0T2				0x39
#define AW2023_REG_LED1T0				0x3A
#define AW2023_REG_LED1T1				0x3B
#define AW2023_REG_LED1T2				0x3C
#define AW2023_REG_LED2T0				0x3D
#define AW2023_REG_LED2T1				0x3E
#define AW2023_REG_LED2T2				0x3F

/* register bits */
#define AW2023_CHIPID					0x09
#define AW2023_RESET_MASK				0x55
#define AW2023_CHIP_DISABLE_MASK		0x00
#define AW2023_CHIP_ENABLE_MASK			0x01
#define AW2023_LED_BREATH_MODE_MASK		0x10
#define AW2023_LED_MANUAL_MODE_MASK		0x00
#define AW2023_LED_BREATHE_PWM_MASK		0xFF
#define AW2023_LED_MANUAL_PWM_MASK		0xFF
#define AW2023_LED_FADEIN_MODE_MASK		0x20
#define AW2023_LED_FADEOUT_MODE_MASK	0x40

#define MAX_RISE_TIME_MS				15
#define MAX_HOLD_TIME_MS				15
#define MAX_FALL_TIME_MS				15
#define MAX_OFF_TIME_MS					15

/* aw2023 register read/write access*/
#define REG_NONE_ACCESS					0
#define REG_RD_ACCESS					1 << 0
#define REG_WR_ACCESS					1 << 1
#define AW2023_REG_MAX					0x7F

const unsigned char aw2023_reg_access[AW2023_REG_MAX] = {
	[AW2023_REG_RESET]  = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2023_REG_GCR1]   = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2023_REG_STATUS] = REG_RD_ACCESS,
	[AW2023_REG_PATST]  = REG_RD_ACCESS,
	[AW2023_REG_GCR2]   = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2023_REG_LEDEN]  = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2023_REG_LCFG0]  = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2023_REG_LCFG1]  = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2023_REG_LCFG2]  = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2023_REG_PWM0]   = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2023_REG_PWM1]   = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2023_REG_PWM2]   = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2023_REG_LED0T0] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2023_REG_LED0T1] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2023_REG_LED0T2] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2023_REG_LED1T0] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2023_REG_LED1T1] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2023_REG_LED1T2] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2023_REG_LED2T0] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2023_REG_LED2T1] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2023_REG_LED2T2] = REG_RD_ACCESS|REG_WR_ACCESS,
};

struct aw2023_led {
	struct i2c_client *client;
	struct led_classdev cdev;
	struct aw2023_platform_data *pdata;
	struct work_struct brightness_work;
	struct mutex lock;
	int num_leds;
	int id;
};

static int aw2023_write(struct aw2023_led *led, u8 reg, u8 val)
{
	int ret = -EINVAL, retry_times = 0;

	do {
		ret = i2c_smbus_write_byte_data(led->client, reg, val);
		retry_times ++;
		if(retry_times == 5)
			break;
	}while (ret < 0);
	
	return ret;	
}

static int aw2023_read(struct aw2023_led *led, u8 reg, u8 *val)
{
	int ret = -EINVAL, retry_times = 0;

	do{
		ret = i2c_smbus_read_byte_data(led->client, reg);
		retry_times ++;
		if(retry_times == 5)
			break;
	}while (ret < 0);
	if (ret < 0)
		return ret;

	*val = ret;
	return 0;
}

static void aw2023_brightness_work(struct work_struct *work)
{
	struct aw2023_led *led = container_of(work, struct aw2023_led,
					brightness_work);
	u8 val;

	mutex_lock(&led->pdata->led->lock);


	/* enable aw2023 if disabled */
	aw2023_read(led, AW2023_REG_GCR1, &val);
	if (!(val&AW2023_CHIP_ENABLE_MASK)) {
		aw2023_write(led, AW2023_REG_GCR1, AW2023_CHIP_ENABLE_MASK);
		msleep(2);
	}

	if (led->cdev.brightness > 0) {
		if (led->cdev.brightness > led->cdev.max_brightness)
			led->cdev.brightness = led->cdev.max_brightness;
		aw2023_write(led, AW2023_REG_GCR2, led->pdata->imax);
		aw2023_write(led, AW2023_REG_LCFG0 + led->id,
					(AW2023_LED_MANUAL_MODE_MASK | led->pdata->led_current));
		aw2023_write(led, AW2023_REG_PWM0 + led->id, led->cdev.brightness);
		aw2023_read(led, AW2023_REG_LEDEN, &val);
		aw2023_write(led, AW2023_REG_LEDEN, val | (1 << led->id));
	} else {
		aw2023_read(led, AW2023_REG_LEDEN, &val);
		aw2023_write(led, AW2023_REG_LEDEN, val & (~(1 << led->id)));
	}

/*
	 * If value in AW_REG_LED_ENABLE is 0, it means the RGB leds are
	 * all off. So we need to power it off.
	 */
	aw2023_read(led, AW2023_REG_LEDEN, &val);
	if (val == 0) {
		aw2023_write(led, AW2023_REG_GCR1, AW2023_CHIP_DISABLE_MASK);
			mutex_unlock(&led->pdata->led->lock);
			return;
		}

	mutex_unlock(&led->pdata->led->lock);
}

static void aw2023_led_blink_set(struct aw2023_led *led, unsigned long blinking)
{
	u8 val;


	/* enable aw2023 if disabled */
	aw2023_read(led, AW2023_REG_GCR1, &val);
	if (!(val&AW2023_CHIP_ENABLE_MASK)) {
		aw2023_write(led, AW2023_REG_GCR1, AW2023_CHIP_ENABLE_MASK);
		msleep(2);
	}
	
	led->cdev.brightness = blinking ? led->cdev.max_brightness : 0;
	if (blinking > 0) {
		aw2023_write(led, AW2023_REG_GCR2, led->pdata->imax);
		aw2023_write(led, AW2023_REG_PWM0 + led->id, led->cdev.brightness);
		aw2023_write(led, AW2023_REG_LED0T0 + led->id*3,
					(led->pdata->rise_time_ms << 4 | led->pdata->hold_time_ms));
		aw2023_write(led, AW2023_REG_LED0T1 + led->id*3,
					(led->pdata->fall_time_ms << 4 | led->pdata->off_time_ms));
		aw2023_write(led, AW2023_REG_LCFG0 + led->id,
					(AW2023_LED_BREATH_MODE_MASK | led->pdata->led_current));
		aw2023_read(led, AW2023_REG_LEDEN, &val);
		aw2023_write(led, AW2023_REG_LEDEN, val | (1 << led->id));
	} else {
		aw2023_read(led, AW2023_REG_LEDEN, &val);
		aw2023_write(led, AW2023_REG_LEDEN, val & (~(1 << led->id)));
	}

	/*
	 * If value in AW_REG_LED_ENABLE is 0, it means the RGB leds are
	 * all off. So we need to power it off.
	 */
	aw2023_read(led, AW2023_REG_LEDEN, &val);
	if (val == 0) {
		aw2023_write(led, AW2023_REG_GCR1, AW2023_CHIP_DISABLE_MASK);
	}
}

static void aw2023_set_brightness(struct led_classdev *cdev,
			     enum led_brightness brightness)
{
	struct aw2023_led *led = container_of(cdev, struct aw2023_led, cdev);

	led->cdev.brightness = brightness;

	schedule_work(&led->brightness_work);
}

static ssize_t aw2023_store_blink(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	unsigned long blinking;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2023_led *led =
			container_of(led_cdev, struct aw2023_led, cdev);
	ssize_t ret = -EINVAL;

	ret = kstrtoul(buf, 10, &blinking);
	if (ret)
		return ret;
	mutex_lock(&led->pdata->led->lock);
	aw2023_led_blink_set(led, blinking);
	mutex_unlock(&led->pdata->led->lock);

	return len;
}

static ssize_t aw2023_led_time_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2023_led *led =
			container_of(led_cdev, struct aw2023_led, cdev);

	return snprintf(buf, PAGE_SIZE, "%d %d %d %d\n",
			led->pdata->rise_time_ms, led->pdata->hold_time_ms,
			led->pdata->fall_time_ms, led->pdata->off_time_ms);
}

static ssize_t aw2023_led_time_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2023_led *led =
			container_of(led_cdev, struct aw2023_led, cdev);
	int rc, rise_time_ms, hold_time_ms, fall_time_ms, off_time_ms;

	rc = sscanf(buf, "%d %d %d %d",
			&rise_time_ms, &hold_time_ms,
			&fall_time_ms, &off_time_ms);

	mutex_lock(&led->pdata->led->lock);
	led->pdata->rise_time_ms = (rise_time_ms > MAX_RISE_TIME_MS) ?
				MAX_RISE_TIME_MS : rise_time_ms;
	led->pdata->hold_time_ms = (hold_time_ms > MAX_HOLD_TIME_MS) ?
				MAX_HOLD_TIME_MS : hold_time_ms;
	led->pdata->fall_time_ms = (fall_time_ms > MAX_FALL_TIME_MS) ?
				MAX_FALL_TIME_MS : fall_time_ms;
	led->pdata->off_time_ms = (off_time_ms > MAX_OFF_TIME_MS) ?
				MAX_OFF_TIME_MS : off_time_ms;
	aw2023_led_blink_set(led, 1);
	mutex_unlock(&led->pdata->led->lock);
	return len;
}

static ssize_t aw2023_reg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2023_led *led =
			container_of(led_cdev, struct aw2023_led, cdev);

	unsigned char i, reg_val;
	ssize_t len = 0;

    for(i=0; i<AW2023_REG_MAX; i++) {
        if(!(aw2023_reg_access[i]&REG_RD_ACCESS))
           continue;
        aw2023_read(led, i, &reg_val);
        len += snprintf(buf+len, PAGE_SIZE-len, "reg:0x%02x=0x%02x\n", i, reg_val);
    }

	return len;
}

static ssize_t aw2023_reg_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2023_led *led =
			container_of(led_cdev, struct aw2023_led, cdev);

	unsigned int databuf[2];

	if(2 == sscanf(buf, "%x %x", &databuf[0], &databuf[1]))
	{
		aw2023_write(led, (unsigned char)databuf[0], (unsigned char)databuf[1]);
	}

	return len;
}
static DEVICE_ATTR(blink, 0664, NULL, aw2023_store_blink);
static DEVICE_ATTR(led_time, 0664, aw2023_led_time_show, aw2023_led_time_store);
static DEVICE_ATTR(reg, 0664, aw2023_reg_show, aw2023_reg_store);

static struct attribute *aw2023_led_attributes[] = {
	&dev_attr_blink.attr,
	&dev_attr_led_time.attr,
	&dev_attr_reg.attr,
	NULL,
};

static struct attribute_group aw2023_led_attr_group = {
	.attrs = aw2023_led_attributes
};

static int aw2023_check_chipid(struct aw2023_led *led)
{
	u8 val;
	u8 cnt;

	for(cnt = 5; cnt > 0; cnt --)
	{
		aw2023_read(led, AW2023_REG_RESET, &val);
		dev_notice(&led->client->dev,"aw2023 chip id %0x",val);
		if (val == AW2023_CHIPID)
			return 0;
	}
		return -EINVAL;
}

static int aw2023_led_err_handle(struct aw2023_led *led_array,
				int parsed_leds)
{
	int i;
	/*
	 * If probe fails, cannot free resource of all LEDs, only free
	 * resources of LEDs which have allocated these resource really.
	 */
	for (i = 0; i < parsed_leds; i++) {
		sysfs_remove_group(&led_array[i].cdev.dev->kobj,
				&aw2023_led_attr_group);
		led_classdev_unregister(&led_array[i].cdev);
		cancel_work_sync(&led_array[i].brightness_work);
		devm_kfree(&led_array->client->dev, led_array[i].pdata);
		led_array[i].pdata = NULL;
	}
	return i;
}

static int aw2023_led_parse_child_node(struct aw2023_led *led_array,
				struct device_node *node)
{
	struct aw2023_led *led;
	struct device_node *temp;
	struct aw2023_platform_data *pdata;
	int rc = 0, parsed_leds = 0;

	for_each_child_of_node(node, temp) {
		led = &led_array[parsed_leds];
		led->client = led_array->client;

		pdata = devm_kzalloc(&led->client->dev,
				sizeof(struct aw2023_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&led->client->dev,
				"Failed to allocate memory\n");
			goto free_err;
		}
		pdata->led = led_array;
		led->pdata = pdata;

		rc = of_property_read_string(temp, "aw2023,name",
			&led->cdev.name);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading led name, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2023,id",
			&led->id);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading id, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2023,imax",
			&led->pdata->imax);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading id, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2023,led-current",
			&led->pdata->led_current);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading led-current, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2023,max-brightness",
			&led->cdev.max_brightness);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading max-brightness, rc = %d\n",
				rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2023,rise-time-ms",
			&led->pdata->rise_time_ms);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading rise-time-ms, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2023,hold-time-ms",
			&led->pdata->hold_time_ms);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading hold-time-ms, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2023,fall-time-ms",
			&led->pdata->fall_time_ms);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading fall-time-ms, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2023,off-time-ms",
			&led->pdata->off_time_ms);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading off-time-ms, rc = %d\n", rc);
			goto free_pdata;
		}

		INIT_WORK(&led->brightness_work, aw2023_brightness_work);

		led->cdev.brightness_set = aw2023_set_brightness;

		rc = led_classdev_register(&led->client->dev, &led->cdev);
		if (rc) {
			dev_err(&led->client->dev,
				"unable to register led %d,rc=%d\n",
				led->id, rc);
			goto free_pdata;
		}

		rc = sysfs_create_group(&led->cdev.dev->kobj,
				&aw2023_led_attr_group);
		if (rc) {
			dev_err(&led->client->dev, "led sysfs rc: %d\n", rc);
			goto free_class;
		}
		parsed_leds++;
	}

	return 0;

free_class:
	aw2023_led_err_handle(led_array, parsed_leds);
	led_classdev_unregister(&led_array[parsed_leds].cdev);
	cancel_work_sync(&led_array[parsed_leds].brightness_work);
	devm_kfree(&led->client->dev, led_array[parsed_leds].pdata);
	led_array[parsed_leds].pdata = NULL;
	return rc;

free_pdata:
	aw2023_led_err_handle(led_array, parsed_leds);
	devm_kfree(&led->client->dev, led_array[parsed_leds].pdata);
	return rc;

free_err:
	aw2023_led_err_handle(led_array, parsed_leds);
	return rc;
}

static int aw2023_led_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct aw2023_led *led_array;
	struct device_node *node;
	int ret, num_leds = 0;
	printk(KERN_EMERG "%s %d\n", __func__, __LINE__);
	node = client->dev.of_node;
	if (node == NULL)
		return -EINVAL;

	num_leds = of_get_child_count(node);
	printk(KERN_EMERG "%s %d\n", __func__, __LINE__);

	if (!num_leds)
		return -EINVAL;

	printk(KERN_EMERG "%s %d\n", __func__, __LINE__);
	led_array = devm_kzalloc(&client->dev,
			(sizeof(struct aw2023_led) * num_leds), GFP_KERNEL);
	if (!led_array)
		return -ENOMEM;

	printk(KERN_EMERG "%s %d\n", __func__, __LINE__);
	led_array->client = client;
	led_array->num_leds = num_leds;

	mutex_init(&led_array->lock);

	ret = aw2023_led_parse_child_node(led_array, node);
	printk(KERN_EMERG "%s %d\n", __func__, __LINE__);
	if (ret) {
		dev_err(&client->dev, "parsed node error\n");
		goto free_led_arry;
	}

	printk(KERN_EMERG "%s %d\n", __func__, __LINE__);
	i2c_set_clientdata(client, led_array);
	

    ret = aw2023_check_chipid(led_array);
	if (ret) {
		dev_err(&client->dev, "Check chip id error\n");
		goto fail_parsed_node;
	}

	printk(KERN_EMERG "%s %d\n", __func__, __LINE__);
	return 0;

fail_parsed_node:
	aw2023_led_err_handle(led_array, num_leds);
free_led_arry:
	mutex_destroy(&led_array->lock);
	devm_kfree(&client->dev, led_array);
	led_array = NULL;
	return ret;
}

static int aw2023_led_remove(struct i2c_client *client)
{
	struct aw2023_led *led_array = i2c_get_clientdata(client);
	int i, parsed_leds = led_array->num_leds;

	for (i = 0; i < parsed_leds; i++) {
		sysfs_remove_group(&led_array[i].cdev.dev->kobj,
				&aw2023_led_attr_group);
		led_classdev_unregister(&led_array[i].cdev);
		cancel_work_sync(&led_array[i].brightness_work);
		devm_kfree(&client->dev, led_array[i].pdata);
		led_array[i].pdata = NULL;
	}
	mutex_destroy(&led_array->lock);
	devm_kfree(&client->dev, led_array);
	led_array = NULL;
	return 0;
}

static void aw2023_led_shutdown(struct i2c_client *client){
	int ret = -EINVAL, retry_times = 0;
	do{
		printk(KERN_EMERG "zql led aw2023 %d\n", retry_times++);
		ret = i2c_smbus_write_byte_data( client, AW2023_REG_GCR1, AW2023_CHIP_DISABLE_MASK);
		retry_times ++;
		if(retry_times == 5)
			break;
	}while (ret < 0);
}

static const struct i2c_device_id aw2023_led_id[] = {
	{"aw2023_led", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, aw2023_led_id);

static struct of_device_id aw2023_match_table[] = {
	{ .compatible = "awinic,aw2023_led",},
	{ },
};

static struct i2c_driver aw2023_led_driver = {
	.probe = aw2023_led_probe,
	.remove = aw2023_led_remove,
	.shutdown = aw2023_led_shutdown,
	.driver = {
		.name = "aw2023_led",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(aw2023_match_table),
	},
	.id_table = aw2023_led_id,
};

static int __init aw2023_led_init(void)
{
	printk(KERN_EMERG "%s %d\n", __func__, __LINE__);
	return i2c_add_driver(&aw2023_led_driver);
}
module_init(aw2023_led_init);

static void __exit aw2023_led_exit(void)
{
	i2c_del_driver(&aw2023_led_driver);
}
module_exit(aw2023_led_exit);

MODULE_AUTHOR("<liweilei@awinic.com.cn>");
MODULE_DESCRIPTION("AWINIC AW2023 LED driver");
MODULE_LICENSE("GPL v2");
