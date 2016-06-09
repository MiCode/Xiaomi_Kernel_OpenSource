#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/leds.h>
#include <linux/init.h>

#include <linux/mutex.h>

enum led_colors{
LED_RED = 0,
LED_GREEN,
LED_BLUE,
LED_WHITE,
LED_YELLOW,
LED_CYAN,
LED_PURPLE,
LED_COLORS_MAX
};

struct rgb_info{
int blinking;
int brightness;
int on_time;
int off_time;
};

struct aw2013_led_data {
	struct led_classdev cdev;
	enum led_colors color;
};


struct aw2013_leds_priv {
	int num_leds;
	struct aw2013_led_data leds[];
};

struct aw2013_dev_data {
	struct i2c_client	*i2c;
	struct regulator *regulator;
	struct aw2013_leds_priv *leds_priv;
	struct rgb_info leds[LED_COLORS_MAX];
	enum led_colors current_color;
	struct mutex		led_lock;
	struct work_struct	set_color_work;
};


struct aw2013_led{
	const char *name;
	const char *default_trigger;
	unsigned	retain_state_suspended:1;
	unsigned	default_state:2;

};

static inline int sizeof_aw2013_leds_priv(int num_leds)
{
	return sizeof(struct aw2013_leds_priv) +
		(sizeof(struct aw2013_led_data) * num_leds);
}



static struct aw2013_dev_data *s_aw2013;
bool blink_frequency_adjust;

#define AW2013_I2C_NAME   "aw2013"
#if 1

#define AW2013_RSTR		0x0
#define AW2013_GCR			0x01
#define AW2013_STATUS		0x02
#define AW2013_LEDE		0x30
#define AW2013_LCFG0		0x31
#define AW2013_LCFG1		0x32
#define AW2013_LCFG2		0x33
#define AW2013_PWM0		0x34
#define AW2013_PWM1		0x35
#define AW2013_PWM2		0x36
#define AW2013_LED0_T0		0x37
#define AW2013_LED0_T1		0x38
#define AW2013_LED0_T2		0x39
#define AW2013_LED1_T0		0x3A
#define AW2013_LED1_T1		0x3B
#define AW2013_LED1_T2		0x3C
#define AW2013_LED2_T0		0x3D
#define AW2013_LED2_T1		0x3E
#define AW2013_LED2_T2		0x3F
#define AW2013_ASR			0x77

#define Bre_Imax		  0x72
#define Rise_t  0x02
#define Fall_t   0x02
#define Hold_time   0x04
#define Off_time	  0x04
#define Delay_time   0x00
#define Period_Num  0x00
#define Imax_R 0x62
#define Imax_G 0x62
#define Imax_B 0x62

#define MAX_BRIGHTNESS_RED 255
#define MAX_BRIGHTNESS_GREEN 255
#define MAX_BRIGHTNESS_BLUE 255
u8 tp_color;

typedef unsigned char U8;
static int aw2013_debug_enable;
#define AW2013_DEBUG(format, args...) do {\
	if (aw2013_debug_enable) {\
		printk(format, ##args);\
	} \
} while (0)
struct i2c_client *aw2013_client;
static	int aw2013_pdata;
struct i2c_board_info aw2013_info = {
			.type = "aw2013",
			.addr = 0x45,
			.platform_data = &aw2013_pdata,
		};


static int aw2013_has_inited;

#endif

static int aw2013_i2c_write(unsigned char cmd, unsigned char data)
{
	int ret;
	ret = i2c_smbus_write_byte_data(s_aw2013->i2c, cmd, data);

	return ret;
}

enum led_colors devname_to_color(const char *dev_name)
{
	if (!strcmp(dev_name, "red"))
		return LED_RED;
	else if (!strcmp(dev_name, "green"))
		return LED_GREEN;
	else if (!strcmp(dev_name, "blue"))
		return LED_BLUE;
	else if (!strcmp(dev_name, "white"))
		return LED_WHITE;
	else if (!strcmp(dev_name, "yellow"))
		return LED_YELLOW;
	else if (!strcmp(dev_name, "cyan"))
		return LED_CYAN;
	else if (!strcmp(dev_name, "purple"))
		return LED_PURPLE;

	return LED_COLORS_MAX;
}

int aw2013_set_color_singlecolor(struct aw2013_dev_data *aw2013)
{
	unsigned char red_on = 0, green_on = 0, blue_on = 0;
	unsigned char blink_flag = 0;

	if (aw2013->leds[LED_RED].brightness)
		red_on = 1;
	if (aw2013->leds[LED_GREEN].brightness)
		green_on = 1;
	if (aw2013->leds[LED_BLUE].brightness)
		blue_on = 1;

	if (aw2013->leds[LED_RED].brightness > MAX_BRIGHTNESS_RED)
		aw2013->leds[LED_RED].brightness = MAX_BRIGHTNESS_RED;
	if (aw2013->leds[LED_GREEN].brightness > MAX_BRIGHTNESS_GREEN)
		aw2013->leds[LED_GREEN].brightness = MAX_BRIGHTNESS_GREEN;
	if (aw2013->leds[LED_BLUE].brightness > MAX_BRIGHTNESS_BLUE)
		aw2013->leds[LED_BLUE].brightness = MAX_BRIGHTNESS_BLUE;


	if (aw2013->leds[LED_RED].blinking || aw2013->leds[LED_GREEN].blinking || aw2013->leds[LED_BLUE].blinking) {
		blink_flag = 0x10;
		aw2013_i2c_write(0x0, 0x54);
	}

	if (red_on || green_on || blue_on) {
		if (0 == aw2013_has_inited) {
			aw2013_i2c_write(0x0, 0x55);
			aw2013_i2c_write(0x01, 0x1);
			mdelay(1);
			aw2013_has_inited = 1;
		}

		aw2013_i2c_write(0x01, 0xe1);
		printk("tp_color:%d", tp_color);
		switch (tp_color) {
		case 0x31:
			if (red_on)
				aw2013_i2c_write(0x31, blink_flag|0x63);
			if (green_on)
				aw2013_i2c_write(0x32, blink_flag|0x63);
			if (blue_on)
				aw2013_i2c_write(0x33, blink_flag|0x63);
			printk("tp_color is white\n");
			break;
		case 0x34:
			if (red_on)
				aw2013_i2c_write(0x31, blink_flag|0x63);
			if (green_on)
				aw2013_i2c_write(0x32, blink_flag|0x63);
			if (blue_on)
				aw2013_i2c_write(0x33, blink_flag|0x63);
			printk("tp_color is yellow\n");
			break;
		case 0x38:
			if (red_on)
				aw2013_i2c_write(0x31, blink_flag|0x63);
			if (green_on)
				aw2013_i2c_write(0x32, blink_flag|0x63);
			if (blue_on)
				aw2013_i2c_write(0x33, blink_flag|0x63);
			printk("tp_color is golden\n");
			break;
		default:
			if (red_on)
				aw2013_i2c_write(0x31, blink_flag|0x62);
			if (green_on)
				aw2013_i2c_write(0x32, blink_flag|0x62);
			if (blue_on)
				aw2013_i2c_write(0x33, blink_flag|0x62);
			printk("tp_color is black\n");
			break;
		}
		if (red_on)
			aw2013_i2c_write(0x34, aw2013->leds[LED_RED].brightness);
		if (green_on)
			aw2013_i2c_write(0x35, aw2013->leds[LED_GREEN].brightness);
		if (blue_on)
			aw2013_i2c_write(0x36, aw2013->leds[LED_BLUE].brightness);

	if (blink_frequency_adjust) {

		aw2013_i2c_write(0x37, Rise_t<<4 | aw2013->leds[LED_RED].blinking);
		aw2013_i2c_write(0x38, Fall_t<<4 | aw2013->leds[LED_RED].blinking);
		aw2013_i2c_write(0x39, Delay_time<<4 | Period_Num);

		aw2013_i2c_write(0x3a, Rise_t<<4 | aw2013->leds[LED_GREEN].blinking);
		aw2013_i2c_write(0x3b, Fall_t<<4 | aw2013->leds[LED_GREEN].blinking);
		aw2013_i2c_write(0x3c, Delay_time<<4 | Period_Num);

		aw2013_i2c_write(0x3d, Rise_t<<4 | aw2013->leds[LED_BLUE].blinking);
		aw2013_i2c_write(0x3e, Fall_t<<4 | aw2013->leds[LED_BLUE].blinking);
		aw2013_i2c_write(0x3f, Delay_time<<4 | Period_Num);
	} else {

		aw2013_i2c_write(0x37, Rise_t<<4 | Hold_time);
		aw2013_i2c_write(0x38, Fall_t<<4 | Off_time);
		aw2013_i2c_write(0x39, Delay_time<<4 | Period_Num);

		aw2013_i2c_write(0x3a, Rise_t<<4 | Hold_time);
		aw2013_i2c_write(0x3b, Fall_t<<4 | Off_time);
		aw2013_i2c_write(0x3c, Delay_time<<4 | Period_Num);

		aw2013_i2c_write(0x3d, Rise_t<<4 | Hold_time);
		aw2013_i2c_write(0x3e, Fall_t<<4 | Off_time);
		aw2013_i2c_write(0x3f, Delay_time<<4 | Period_Num);
	}
		aw2013_i2c_write(0x30, blue_on<<2|green_on<<1|red_on);
		mdelay(1);

	} else {
		aw2013_i2c_write(0x01, 0);
		mdelay(1);
		aw2013_i2c_write(0x30, 0);
		aw2013_i2c_write(0x34, 0);
		aw2013_i2c_write(0x35, 0);
		aw2013_i2c_write(0x36, 0);
		mdelay(1);
		aw2013_has_inited = 0;
	}

	return 0;

}

int aw2013_set_color_multicolor(struct aw2013_dev_data *aw2013, enum led_colors color)
{
	unsigned char blink_flag = 0;

	if (aw2013->leds[color].brightness) {
		if (aw2013->leds[color].blinking)
			blink_flag = 0x10;

		if (0 == aw2013_has_inited) {

			aw2013_i2c_write(0x0, 0x55);
			aw2013_i2c_write(0x01, 0x1);
			mdelay(1);
			aw2013_has_inited = 1;
		}

		switch (color) {

		case LED_WHITE:

			aw2013_i2c_write(0x0, 0x54);
			aw2013_i2c_write(0x01, 0xe1);
			aw2013_i2c_write(0x31, blink_flag|0x61);
			aw2013_i2c_write(0x32, blink_flag|0x62);
			aw2013_i2c_write(0x33, blink_flag|0x62);
			aw2013_i2c_write(0x34, MAX_BRIGHTNESS_RED);
			aw2013_i2c_write(0x35, MAX_BRIGHTNESS_GREEN);
			aw2013_i2c_write(0x36, MAX_BRIGHTNESS_BLUE);
			aw2013_i2c_write(0x30, 0x7);
			break;

		case LED_YELLOW:
			aw2013_i2c_write(0x0, 0x54);
			aw2013_i2c_write(0x01, 0xe1);
			aw2013_i2c_write(0x31, blink_flag|0x62);
			aw2013_i2c_write(0x32, blink_flag|0x62);
			aw2013_i2c_write(0x33, 0);
			aw2013_i2c_write(0x34, MAX_BRIGHTNESS_RED);
			aw2013_i2c_write(0x35, MAX_BRIGHTNESS_GREEN);
			aw2013_i2c_write(0x36, 0);
			aw2013_i2c_write(0x30, 0x3);
			break;

		case LED_PURPLE:
			aw2013_i2c_write(0x0, 0x54);
			aw2013_i2c_write(0x01, 0xe1);
			aw2013_i2c_write(0x31, blink_flag|0x62);
			aw2013_i2c_write(0x32, 0);
			aw2013_i2c_write(0x33, blink_flag|0x62);
			aw2013_i2c_write(0x34, MAX_BRIGHTNESS_RED);
			aw2013_i2c_write(0x35, 0);
			aw2013_i2c_write(0x36, MAX_BRIGHTNESS_BLUE);
			aw2013_i2c_write(0x30, 0x5);
			break;

		case LED_CYAN:
			aw2013_i2c_write(0x0, 0x54);
			aw2013_i2c_write(0x01, 0xe1);
			aw2013_i2c_write(0x31, 0);
			aw2013_i2c_write(0x32, blink_flag|0x62);
			aw2013_i2c_write(0x33, blink_flag|0x62);
			aw2013_i2c_write(0x34, 0);
			aw2013_i2c_write(0x35, MAX_BRIGHTNESS_GREEN);
			aw2013_i2c_write(0x36, MAX_BRIGHTNESS_BLUE);
			aw2013_i2c_write(0x30, 0x6);
			break;

		default:
			break;
		}
		aw2013_i2c_write(0x37, Rise_t<<4 | Hold_time);
		aw2013_i2c_write(0x38, Fall_t<<4 | Off_time);
		aw2013_i2c_write(0x39, Delay_time<<4 | Period_Num);

		aw2013_i2c_write(0x3a, Rise_t<<4 | Hold_time);
		aw2013_i2c_write(0x3b, Fall_t<<4 | Off_time);
		aw2013_i2c_write(0x3c, Delay_time<<4 | Period_Num);

		aw2013_i2c_write(0x3d, Rise_t<<4 | Hold_time);
		aw2013_i2c_write(0x3e, Fall_t<<4 | Off_time);
		aw2013_i2c_write(0x3f, Delay_time<<4 | Period_Num);
	} else {


		aw2013_i2c_write(0x01, 0);
		mdelay(1);
		aw2013_i2c_write(0x30, 0);
		aw2013_i2c_write(0x34, 0);
		aw2013_i2c_write(0x35, 0);
		aw2013_i2c_write(0x36, 0);
		mdelay(1);
		aw2013_has_inited = 0;
	}

	return 0;
}


static void set_color_delayed(struct work_struct *ws)
{
	enum led_colors color;
	struct aw2013_dev_data *aw2013 =
		container_of(ws, struct aw2013_dev_data, set_color_work);

	mutex_lock(&s_aw2013->led_lock);
	color = aw2013->current_color;

	AW2013_DEBUG("aw2013 set_color_delayed color = %d, brightness = %d, blink = %d \n", color, aw2013->leds[color].brightness, aw2013->leds[color].blinking);

	if (color == LED_RED || color == LED_GREEN || color == LED_BLUE)
		aw2013_set_color_singlecolor(aw2013);
	else
		aw2013_set_color_multicolor(aw2013, color);
	mutex_unlock(&s_aw2013->led_lock);
}

int aw2013_set_color(struct aw2013_dev_data *aw2013, enum led_colors color)
{

	aw2013->current_color = color;
	schedule_work(&s_aw2013->set_color_work);

	return 0;

}

static void aw2013_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	enum led_colors color;
	color = devname_to_color(led_cdev->name);
	s_aw2013->leds[color].brightness = value;
	s_aw2013->leds[color].blinking = 0;
	aw2013_set_color(s_aw2013, color);
}

static int aw2013_led_blink_set(struct led_classdev *led_cdev,
	unsigned long *delay_on, unsigned long *delay_off)
{
	enum led_colors color;

	color = devname_to_color(led_cdev->name);
	s_aw2013->leds[color].blinking = 1;
	s_aw2013->leds[color].brightness = 255;
	s_aw2013->leds[color].on_time = *delay_on;
	s_aw2013->leds[color].off_time = *delay_off;

	led_cdev->brightness = s_aw2013->leds[color].brightness;
	aw2013_set_color(s_aw2013, color);

	return 0;
}


static int aw2013_i2c_check_device(
	struct i2c_client *client)
{
	int err;
	int retreive_count = 0;
	while (retreive_count++ < 5) {
		msleep(10);
		err = aw2013_i2c_write(AW2013_RSTR, 0x55);
		if (err == 0)
			break;
	}



	return err;
}

static int aw2013_power_up(struct aw2013_dev_data *pdata, struct i2c_client *client, bool enable)
{
	int err = -1;

	pdata->regulator = devm_regulator_get(&client->dev, "rgb_led");
	if (IS_ERR(pdata->regulator)) {
		dev_err(&client->dev, "regulator get failed\n");
		err = PTR_ERR(pdata->regulator);
		pdata->regulator = NULL;
		return err;
	}

	if (enable) {
		regulator_set_voltage(pdata->regulator, 2800000, 2800000);
		err = regulator_enable(pdata->regulator);
		msleep(100);
	} else
		err = regulator_disable(pdata->regulator);

	return err;
}



static ssize_t blink_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", led_cdev->brightness);
}


static ssize_t blink_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	unsigned long blinking;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret = -EINVAL;
	enum led_colors color;

	ret = kstrtoul(buf, 10, &blinking);
	if (ret)
		return ret;

	color = devname_to_color(led_cdev->name);
	led_cdev->brightness =
	s_aw2013->leds[color].brightness = blinking ? 255 : 0;
	s_aw2013->leds[color].blinking = blinking;

	led_cdev->brightness = s_aw2013->leds[color].brightness;

	aw2013_set_color(s_aw2013, color);

	return count;
}
static DEVICE_ATTR(blink, 0664, blink_show, blink_store);


static int create_aw2013_led(const struct aw2013_led *template,
	struct aw2013_led_data *led_dat, struct device *parent,
	int (*blink_set)(unsigned, int, unsigned long *, unsigned long *))
{
	int ret;

	led_dat->cdev.name = template->name;
	led_dat->cdev.default_trigger = template->default_trigger;
	led_dat->color = devname_to_color(template->name);
	led_dat->cdev.brightness = 0;
	if (!template->retain_state_suspended)
		led_dat->cdev.flags |= LED_CORE_SUSPENDRESUME;

	led_dat->cdev.blink_set = aw2013_led_blink_set;
	led_dat->cdev.brightness_set = aw2013_led_set;

	ret = led_classdev_register(parent, &led_dat->cdev);

	device_create_file(led_dat->cdev.dev, &dev_attr_blink);

	if (ret < 0)
		return ret;

	return 0;
}

static void delete_aw2013_led(struct aw2013_led_data *led)
{
	device_remove_file(led->cdev.dev, &dev_attr_blink);
	led_classdev_unregister(&led->cdev);
}

static struct aw2013_leds_priv *aw2013_leds_create_of(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct device_node *child;
	struct aw2013_leds_priv *priv;
	int count, ret;

	/* count LEDs in this device, so we know how much to allocate */
	count = of_get_child_count(np);
	if (!count)
		return ERR_PTR(-ENODEV);

	priv = devm_kzalloc(&client->dev, sizeof_aw2013_leds_priv(count),
			GFP_KERNEL);
	if (!priv)
		return ERR_PTR(-ENOMEM);

	for_each_child_of_node(np, child) {
		struct aw2013_led led = {};

		led.name = of_get_property(child, "label", NULL) ? : child->name;
		led.default_trigger =
			of_get_property(child, "linux, default-trigger", NULL);
		led.retain_state_suspended =
			(unsigned)of_property_read_bool(child,
				"retain-state-suspended");

		ret = create_aw2013_led(&led, &priv->leds[priv->num_leds++],
					  &client->dev, NULL);
		if (ret < 0) {
			of_node_put(child);
			goto err;
		}
	}

	return priv;

err:
	for (count = priv->num_leds - 2; count >= 0; count--)
		delete_aw2013_led(&priv->leds[count]);

	devm_kfree(&client->dev, priv);

	return ERR_PTR(-ENODEV);
}


static int aw2013_led_probe(struct i2c_client *client, const struct i2c_device_id *id)
{

	int err = 0;
	struct aw2013_leds_priv *priv;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev,
				"%s: check_functionality failed.", __func__);
		err = -ENODEV;
		goto exit0;
	}

	/* Allocate memory for driver data */
	s_aw2013 = kzalloc(sizeof(struct aw2013_dev_data), GFP_KERNEL);
	if (!s_aw2013) {
		dev_err(&client->dev,
				"%s: memory allocation failed.", __func__);
		err = -ENOMEM;
		goto exit1;
	}

	/***** I2C initialization *****/
	s_aw2013->i2c = client;
	/* set client data */
	i2c_set_clientdata(client, s_aw2013);

	mutex_init(&s_aw2013->led_lock);

	INIT_WORK(&s_aw2013->set_color_work, set_color_delayed);

	if (0 != aw2013_power_up(s_aw2013, client, true))
		goto exit2;
	if (0 != aw2013_i2c_check_device(client)) {
		dev_err(&client->dev,
				"%s:  aw2013_i2c_check_device failed.", __func__);
		goto exit2;
	}

	blink_frequency_adjust = of_property_read_bool(client->dev.of_node, "blink-frequency-adjustable");

	priv = aw2013_leds_create_of(client);

	if (IS_ERR(priv))
		goto exit3;

	s_aw2013->leds_priv = priv;

	return 0;

exit3:
exit2:
	kfree(s_aw2013);
exit1:
exit0:
	return err;
}

static int aw2013_led_remove(struct i2c_client *client)
{
	int count = 0;
	struct aw2013_dev_data *aw2013 = i2c_get_clientdata(client);

	for (count = aw2013->leds_priv->num_leds - 2; count >= 0; count--)
	delete_aw2013_led(&(aw2013->leds_priv->leds[count]));

	devm_kfree(&client->dev, aw2013->leds_priv);

	kfree(aw2013);
	dev_info(&client->dev, "successfully removed.");
	return 0;
}

static const struct i2c_device_id aw2013_led_id[] = {
	{AW2013_I2C_NAME, 0},
	{}
};

static struct of_device_id aw2013_led_match_table[] = {
	{.compatible = "awinc,aw2013",},
	{},
};

static struct i2c_driver aw2013_led_driver = {
	.probe		= aw2013_led_probe,
	.remove		= aw2013_led_remove,
	.id_table	= aw2013_led_id,
	.driver = {
		.name	= AW2013_I2C_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = aw2013_led_match_table,

	},
};

static int __init aw2013_led_init(void)
{
	pr_info("aw2013 led driver: initialize.");
	return i2c_add_driver(&aw2013_led_driver);
}

static void __exit aw2013_led_exit(void)
{
	pr_info("aw2013 led driver: release.");
	i2c_del_driver(&aw2013_led_driver);
}

subsys_initcall(aw2013_led_init);
module_exit(aw2013_led_exit);

MODULE_DESCRIPTION("aw2013 driver");
MODULE_LICENSE("GPL");
