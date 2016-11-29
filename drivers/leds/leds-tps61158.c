#include <linux/module.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/mutex.h>
#include <linux/acpi.h>
#include <linux/pwm.h>

#define TPS61158_DEVICE_ACPI_NAME "TPS61158"
#define MAX_BRIGHTNESS			255
#define FIXED_BRIGHTNESS			80
#define DEFAULT_LED_NAME		"button-backlight"

#define PERIOD_NS   19200
#define INIT_LED_BRIGHTNESS 0

#define KEYPAD_LED_PWM_CHIP  1

struct tps61158_led {
	struct mutex lock;
	struct work_struct work;
	struct led_classdev cdev;
	int pwm_num;
};

static int tps61158_led_init_device(struct tps61158_led *led)
{
	return 0;
}

static void brightness_set(struct tps61158_led *led, u8 val)
{
	struct pwm_chip *chip;
	struct pwm_device *pwm;
	int pwm_id;
	int duty;

	chip = find_pwm_dev(1);
	if (chip == NULL) {
		printk(KERN_ERR " find_pwm_dev(1) device failed!! -\n");
		return ;
	}
	pwm_id = chip->pwms[0].pwm;

	pwm = pwm_request(pwm_id, "tps61158-pwm");
	if (!pwm) {
		printk(KERN_ERR"%s, could not get pwm device", __func__);
		return ;
	}

	if (val) {
		if (val > led->cdev.max_brightness)
			val = led->cdev.max_brightness;
		duty = val * PERIOD_NS / MAX_BRIGHTNESS;
		chip->ops->config(chip, pwm, duty, PERIOD_NS);
		chip->ops->enable(chip, pwm);
	} else {
		chip->ops->disable(chip, pwm);
	}
	pr_info("TPS61158 : button backlight  %s -- %d \n", __func__, val);
	return;
}

static void tps61158_brightness_set(struct led_classdev *led_cdev,
				enum led_brightness brt_val)
{
	struct tps61158_led *led =
			container_of(led_cdev, struct tps61158_led, cdev);

	led->cdev.brightness = brt_val;
	schedule_work(&led->work);
}

static void tps61158_led_work(struct work_struct *work)
{
	struct tps61158_led *led = container_of(work, struct tps61158_led, work);
	u8 val = led->cdev.brightness;

	mutex_lock(&led->lock);

	brightness_set(led, val);

	mutex_unlock(&led->lock);
}

static int tps61158_led_probe(struct platform_device *pdev)
{
	struct tps61158_led *led;
	struct device *dev = &pdev->dev;
	int ret;

	printk("Enter func: %s - \n", __func__);
	led = devm_kzalloc(dev, sizeof(struct tps61158_led), GFP_KERNEL);
	if (!led)
		return -ENOMEM;
	memset((void *) led, 0, sizeof(struct tps61158_led));

	led->cdev.max_brightness = FIXED_BRIGHTNESS;
	led->cdev.brightness_set = tps61158_brightness_set;

	led->cdev.name = DEFAULT_LED_NAME;
	led->cdev.brightness = INIT_LED_BRIGHTNESS;

	led->pwm_num = KEYPAD_LED_PWM_CHIP;

	mutex_init(&led->lock);
	INIT_WORK(&led->work, tps61158_led_work);

	platform_set_drvdata(pdev, led);

	ret = tps61158_led_init_device(led);
	if (ret) {
		dev_err(dev, "led init device err: %d\n", ret);
		return ret;
	}

	ret = led_classdev_register(dev, &led->cdev);
	if (ret) {
		dev_err(dev, "led register err: %d\n", ret);
		return ret;
	}

	mutex_lock(&led->lock);
	brightness_set(led, led->cdev.brightness);
	mutex_unlock(&led->lock);
	return 0;
}

static int tps61158_led_remove(struct platform_device *pdev)
{
	struct tps61158_led *led = platform_get_drvdata(pdev);

	led_classdev_unregister(&led->cdev);

	return 0;
}

static const struct acpi_device_id tps61158_acpi_match[] = {
	{TPS61158_DEVICE_ACPI_NAME, 0},
	{ },
};
MODULE_DEVICE_TABLE(acpi, tps61158_acpi_match);

static struct platform_driver tps61158_led_driver = {
	.probe = tps61158_led_probe,
	.remove = tps61158_led_remove,
	.driver = {
		.name = "TPS61158-LED",
		.owner = THIS_MODULE,
		.acpi_match_table = ACPI_PTR(tps61158_acpi_match),
	},
};

static int __init tps61158_init(void)
{
	int ret;
	ret = platform_driver_register(&tps61158_led_driver);
	if (ret)
		printk(KERN_ERR"Platform dev registration failed");
	return ret;
}
late_initcall(tps61158_init);

static void __exit tps61158_exit(void)
{
	platform_driver_unregister(&tps61158_led_driver);
}
module_exit(tps61158_exit);

MODULE_DESCRIPTION("Key Pad Led Driver");
MODULE_LICENSE("GPL");
