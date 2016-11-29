#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <asm/errno.h>
#include <linux/cdev.h>
#include <linux/leds.h>
#include <linux/acpi.h>

#ifndef CONFIG_ACPI
#define CONFIG_ACPI
#endif

#ifdef CONFIG_ACPI

#define KTD_ACPI_NAME "KTD20260"
#endif

#define KTD_I2C_NAME			"ktd2026_i2c"

/* KTD2026 register map */
#define KTD2026_REG_EN_RST		0x00
#define KTD2026_REG_FLASH_PERIOD	0x01
#define KTD2026_REG_PWM1_TIMER		0x02
#define KTD2026_REG_PWM2_TIMER		0x03
#define KTD2026_REG_LED_EN		0x04
#define KTD2026_REG_TRISE_TFALL		0x05
#define KTD2026_REG_LED1		0x06
#define KTD2026_REG_LED2		0x07
#define KTD2026_REG_LED3		0x08
#define KTD2026_REG_MAX			0x09
#define KTD2026_TIME_UNIT		500
/* MASK */
#define CNT_TIMER_SLOT_MASK		0x07
#define CNT_ENABLE_MASK			0x18
#define CNT_RISEFALL_TSCALE_MASK	0x60

#define CNT_TIMER_SLOT_SHIFT		0x00
#define CNT_ENABLE_SHIFT		0x03
#define CNT_RISEFALL_TSCALE_SHIFT	0x05

#define LED_R_MASK		0x00ff0000
#define LED_G_MASK		0x0000ff00
#define LED_B_MASK		0x000000ff
#define LED_R_SHIFT		16
#define LED_G_SHIFT		8

#define KTD2026_RESET		0x07

#define LED_MAX_CURRENT		0x39   /* 5mA */
#define LED_OFF			0x00

#define MAX_NUM_LEDS		3

enum ktd2026_led_mode {
	LED_EN_OFF	= 0,
	LED_EN_ON	= 1,
	LED_EN_PWM1	= 2,
	LED_EN_PWM2	= 3,
};

enum ktd2026_pwm{
	PWM1 = 0,
	PWM2 = 1,
};

enum ktd2026_led_enum {
	LED_R = 4,
	LED_G = 0,
	LED_B = 2,
};

struct ktd2026_led_conf {
	const char      *name;
	int          brightness;
	int          max_brightness;
	bool          use_blink;
	int          flags;
};

static struct ktd2026_led_conf led_conf[] = {
	{
		.name = "blue",
		.brightness = LED_OFF,
		.max_brightness = LED_MAX_CURRENT,
		.use_blink = true,
		.flags = 0,
	},
	{
		.name = "green",
		.brightness = LED_OFF,
		.max_brightness = LED_MAX_CURRENT,
		.use_blink = true,
		.flags = 0,
	},
	{
		.name = "red",
		.brightness = LED_OFF,
		.max_brightness = LED_MAX_CURRENT,
		.use_blink = true,
		.flags = 0,
	}
};

struct ktd2026_led_data {
	u8	channel;
	u8	brightness;
	struct led_classdev	cdev;
	struct work_struct	work;
	bool use_blink;
};

struct ktd2026_data {
	struct i2c_client *client;
	struct mutex mutex;
	struct ktd2026_led_data leds[MAX_NUM_LEDS];
	u8     shadow_reg[KTD2026_REG_MAX];
};

struct i2c_client *b_client;

static void ktd2026_leds_on(enum ktd2026_led_enum led,
		enum ktd2026_led_mode mode, u8 bright)
{
	struct ktd2026_data *data = (struct ktd2026_data *)i2c_get_clientdata(b_client);

	data->shadow_reg[KTD2026_REG_LED1 + led/2] = bright;

	if (mode == LED_EN_OFF)
		data->shadow_reg[KTD2026_REG_LED_EN] &= ~(LED_EN_PWM2 << led);
	else
		data->shadow_reg[KTD2026_REG_LED_EN] |= mode << led;
}

void ktd2026_set_timerslot_control(int timer_slot)
{
	struct ktd2026_data *data = i2c_get_clientdata(b_client);

	data->shadow_reg[KTD2026_REG_EN_RST] &= ~(CNT_TIMER_SLOT_MASK);
	data->shadow_reg[KTD2026_REG_EN_RST]
		|= timer_slot << CNT_TIMER_SLOT_SHIFT;
}

/*  Flash period = period * 0.128 + 0.256
 *  exception  0 = 0.128s
 *  please refer to data sheet for detail */
void ktd2026_set_period(int period)
{
	struct ktd2026_data *data = i2c_get_clientdata(b_client);

	data->shadow_reg[KTD2026_REG_FLASH_PERIOD] = period;
}

/* MAX duty = 0xFF (99.6%) , min duty = 0x0 (0%) , 0.4% scale */
void ktd2026_set_pwm_duty(enum ktd2026_pwm pwm, int duty)
{
	struct ktd2026_data *data = i2c_get_clientdata(b_client);
	data->shadow_reg[KTD2026_REG_PWM1_TIMER + pwm] = duty;
}

/* Rise Ramp Time = trise * 96 (ms) */
/* minimum rise ramp time = 1.5ms when traise is set to 0 */
/* Tscale */
/* 0 = 1x      1 = 2x slower      2 = 4x slower    3 = 8x slower */
void ktd2026_set_trise_tfall(int trise, int tfall, int tscale)
{
	struct ktd2026_data *data = i2c_get_clientdata(b_client);

	data->shadow_reg[KTD2026_REG_TRISE_TFALL] = (tfall << 4) + trise;

	data->shadow_reg[KTD2026_REG_EN_RST] &= ~(CNT_RISEFALL_TSCALE_MASK);
	data->shadow_reg[KTD2026_REG_EN_RST]
			|= tscale << CNT_RISEFALL_TSCALE_SHIFT;
}

static int leds_i2c_write_all(struct i2c_client *client)
{
	struct ktd2026_data *data = i2c_get_clientdata(client);
	int ret = 0;

	mutex_lock(&data->mutex);
	ret = i2c_smbus_write_i2c_block_data(client,
			KTD2026_REG_EN_RST, KTD2026_REG_MAX,
			&data->shadow_reg[KTD2026_REG_EN_RST]);
	if (ret < 0) {
		dev_err(&client->adapter->dev,
			"%s: failure on i2c block write\n",
			__func__);
	}
	mutex_unlock(&data->mutex);
	return ret;
}

static void ktd2026_led_work(struct work_struct *work)
{
	struct ktd2026_led_data *led = container_of(work,
					struct ktd2026_led_data, work);

	if (led->cdev.brightness == 0)
		ktd2026_leds_on(led->channel, LED_EN_OFF, 0);
	else
		ktd2026_leds_on(led->channel, LED_EN_ON, led->cdev.brightness);

	leds_i2c_write_all(b_client);
	return;
}


static ssize_t blink_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct ktd2026_led_data *led;
	unsigned long blinking;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret = -EINVAL;

	ret = kstrtoul(buf, 10, &blinking);
	if (ret)
		return ret;
	led = container_of(led_cdev, struct ktd2026_led_data, cdev);
	led->cdev.brightness = blinking ? led->cdev.max_brightness : 0;

	if (!blinking)
		ktd2026_leds_on(led->channel, LED_EN_OFF, 0);
	else {
		ktd2026_leds_on(led->channel, LED_EN_PWM1, led->cdev.brightness);
		ktd2026_set_timerslot_control(0); /* Tslot 1*/
		ktd2026_set_period(0x12);
		ktd2026_set_pwm_duty(PWM1, 0x56);
		ktd2026_set_trise_tfall(10, 10, 1);
	}

	leds_i2c_write_all(b_client);

	return count;
}

static DEVICE_ATTR(blink, 0664, NULL, blink_store);

static struct attribute *blink_attrs[] = {
	&dev_attr_blink.attr,
	NULL
};
static const struct attribute_group blink_attr_group = {
	.attrs = blink_attrs,
};

static void ktd2026_led_set(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	struct ktd2026_led_data *led;

	led = container_of(led_cdev, struct ktd2026_led_data, cdev);

	led->cdev.brightness = value;
	schedule_work(&led->work);
};

static int initialize_channel(struct i2c_client *client,
					struct ktd2026_led_data *led, int channel)
{
	struct device *dev = &client->dev;
	int ret;

	led->channel = channel * 2;
	led->cdev.brightness_set = ktd2026_led_set;
	led->cdev.name = led_conf[channel].name;
	led->cdev.brightness = led_conf[channel].brightness;
	led->cdev.max_brightness = led_conf[channel].max_brightness;
	led->cdev.flags = led_conf[channel].flags;
	led->use_blink = led_conf[channel].use_blink;

	ret = led_classdev_register(dev, &led->cdev);

	if (ret < 0) {
		dev_err(dev, "can not register led channel : %d\n", channel);
		return ret;
	}

	if (led->use_blink) {
		ret = sysfs_create_group(&led->cdev.dev->kobj,
			&blink_attr_group);
		if (ret < 0) {
			dev_err(dev, "can not register sysfs attribute\n");
			return ret;
		}
	}

	return 0;
}

static int ktd20xx_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int i, ret = 0;
	struct acpi_device_id *aid;
	struct ktd2026_data *data = NULL;

	printk("[%s]: Enter!\n", __func__);

	aid = acpi_match_device(client->dev.driver->acpi_match_table, &client->dev);
	if (!aid)
		return -ENODEV;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev,
				"%s: check_functionality failed.", __func__);
		ret = -ENODEV;
		goto err_exit;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}

	data->client = client;
	b_client = client;
	i2c_set_clientdata(client, data);
	mutex_init(&data->mutex);

	/* initialize LED */
	/* turn off all leds */
	ktd2026_leds_on(LED_R, LED_EN_OFF, 0);
	ktd2026_leds_on(LED_G, LED_EN_OFF, 0);
	ktd2026_leds_on(LED_B, LED_EN_OFF, 0);

	ktd2026_set_timerslot_control(0); /* Tslot1 */
	ktd2026_set_period(0x12);
	ktd2026_set_pwm_duty(PWM1, 0x56);
	ktd2026_set_pwm_duty(PWM2, 0);
	ktd2026_set_trise_tfall(10, 10, 1);

	for (i = 0; i < MAX_NUM_LEDS; i++) {
		ret = initialize_channel(client, &data->leds[i], i);
		if (ret < 0) {
			dev_err(&client->dev, "failure on initialization\n");
			goto err_exit;
		}
		INIT_WORK(&data->leds[i].work, ktd2026_led_work);
	}

	leds_i2c_write_all(client);

	return 0;

err_exit:
	mutex_destroy(&data->mutex);
	kfree(data);
	return ret;
}

static void ktd20xx_shutdown(struct i2c_client *client)
{
	int i;
	struct ktd2026_data *data = i2c_get_clientdata(client);

	for (i = 0; i < MAX_NUM_LEDS; i++) {
		cancel_work_sync(&data->leds[i].work);
	}

	ktd2026_leds_on(LED_R, LED_EN_OFF, 0);
	ktd2026_leds_on(LED_G, LED_EN_OFF, 0);
	ktd2026_leds_on(LED_B, LED_EN_OFF, 0);
	leds_i2c_write_all(client);
}

static const struct i2c_device_id ktd2xx_id[] = {
	{KTD_I2C_NAME, 0},
	{ }
};

#ifdef CONFIG_ACPI
static const struct acpi_device_id ktd20xx_acpi_match[] = {
	{KTD_ACPI_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(acpi, ktd20xx_acpi_match);
#else
static struct of_device_id ktd20xx_match_table[] = {
	{ .compatible = "ktd,ktd2026",},
	{ },
};
#endif

static struct i2c_driver ktd20xx_driver = {
	.driver = {
		.name	= KTD_I2C_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_ACPI
		.acpi_match_table = ACPI_PTR(ktd20xx_acpi_match),
#else
		.of_match_table = ktd20xx_match_table,
#endif
	},
	.probe = ktd20xx_probe,
	.shutdown = ktd20xx_shutdown,
	.id_table = ktd2xx_id,
};

static int __init ktd20xx_init(void)
{
	int err;
	printk("Enter:%s\n", __func__);
	err = i2c_add_driver(&ktd20xx_driver);
	if (err) {
		printk(KERN_ERR "ktd20xx driver failed "
				"(errno = %d)\n", err);
	} else {
		printk("Successfully added driver %s\n",
				ktd20xx_driver.driver.name);
	}
	return err;
}

static void __exit ktd20xx_exit(void)
{
	printk("%s\n", __func__);
	i2c_del_driver(&ktd20xx_driver);
}

module_init(ktd20xx_init);
module_exit(ktd20xx_exit);

MODULE_LICENSE("GPL");

