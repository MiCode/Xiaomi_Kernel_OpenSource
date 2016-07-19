#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/of_device.h>
#include <linux/qpnp/pwm.h>
#include <linux/err.h>
#include <linux/of_gpio.h>
#include "../staging/android/timed_output.h"

#define ISA1000_VIB_DEFAULT_TIMEOUT	15000

struct isa1000_pwm_info {
	struct pwm_device *pwm_dev;
	u32 pwm_channel;
	u32 duty_us;
	u32 period_us;
};

struct isa1000_vib {
	int isa1000_chip_en_pin;
	int isa1000_vdd_en_pin;
	struct hrtimer vib_timer;
	struct timed_output_dev timed_dev;
	struct work_struct work;
	struct isa1000_pwm_info pwm_info;

	int pwm_pmic_gpio;

	struct pinctrl *pinctrl;
	struct pinctrl_state *gpio_state_default;

	int state;
	int timeout;
	struct mutex lock;
};

struct isa1000_vib *isa1000_vibr = NULL;

static int vib_level = 0;

/* This function assumes an input of [-127, 127] and mapped to [1, 255] */
/* If your PWM module does not take an input of [1, 255] and mapping to [1%, 99%]
**    Please modify accordingly
**/
void isa1000_vib_set_level(int level)
{
	int rc = 0;


	if (level != 0) {
		if (vib_level != level) {
			if (gpio_is_valid(isa1000_vibr->isa1000_chip_en_pin)) {
				rc = gpio_request(isa1000_vibr->isa1000_chip_en_pin, "chip_en_pin");
				if (rc) {
					pr_err("%s: gpio %d request failed\n", __func__, isa1000_vibr->isa1000_chip_en_pin);
					goto vdd_dwn;
				}
			} else {
				pr_err("%s: Invalid gpio %d\n", __func__, isa1000_vibr->isa1000_chip_en_pin);
				goto gpio_request_error;
			}

			rc = gpio_direction_output(isa1000_vibr->isa1000_chip_en_pin, 1);
			if (rc) {
				pr_err("set_direction for isa1000_chip_en_pin gpio failed\n");
				goto chip_dwn;
			}

			gpio_set_value(isa1000_vibr->isa1000_chip_en_pin, 1);

			if (gpio_is_valid(isa1000_vibr->isa1000_vdd_en_pin)) {
				rc = gpio_request(isa1000_vibr->isa1000_vdd_en_pin, "vdd_en_pin");
				if (rc) {
					pr_err("%s: gpio %d request failed\n", __func__, isa1000_vibr->isa1000_vdd_en_pin);
					goto vdd_dwn;
				}
			} else {
				pr_err("%s: Invalid gpio %d\n", __func__, isa1000_vibr->isa1000_vdd_en_pin);
				goto chip_dwn;
			}

			rc = gpio_direction_output(isa1000_vibr->isa1000_vdd_en_pin, 1);
			if (rc) {
				pr_err("set_direction for isa1000_vdd_en_pin gpio failed\n");
				goto vdd_dwn;
			}

			gpio_set_value(isa1000_vibr->isa1000_vdd_en_pin, 1);

			/* Set PWM duty cycle corresponding to the input 'level' */
			/* Xiaomi TODO: This is only an example.
			* Please modify for PWM on Hong Mi 2A platform
			*/
			pwm_disable(isa1000_vibr->pwm_info.pwm_dev);
			if (((vib_level + level) == 0) || (level < -120)) {
				rc = pwm_config(isa1000_vibr->pwm_info.pwm_dev,
						(isa1000_vibr->pwm_info.period_us * (level + 130)) / 256,
								isa1000_vibr->pwm_info.period_us);
			} else {
				rc = pwm_config(isa1000_vibr->pwm_info.pwm_dev,
								(isa1000_vibr->pwm_info.period_us * (level + 128)) / 256,
								isa1000_vibr->pwm_info.period_us);
			}

			if (rc < 0) {
				pr_err("%s: pwm_config fail\n", __func__);
				goto chip_dwn;
			}

			/* Enable the PWM output */
			/* Xiaomi TODO: This is only an example.
			** Please modify for PWM on Hong Mi 2A platform
			*/
			rc = pwm_enable(isa1000_vibr->pwm_info.pwm_dev);
			if (rc < 0) {
				pr_err("%s: pwm_enable fail\n", __func__);
				goto chip_dwn;
			}

			vib_level = level;

			/* Assert the GPIO_ISA1000_EN to enable ISA1000 */
			/* Xiaomi TODO: This is only an example.
			** Please modify for GPIO on Hong Mi 2A platform
			*/
		} else {
			goto gpio_request_error;
		}
	} else {
		if (gpio_is_valid(isa1000_vibr->isa1000_chip_en_pin)) {
			rc = gpio_request(isa1000_vibr->isa1000_chip_en_pin, "chip_en_pin");
			if (rc) {
				pr_err("%s: gpio %d request failed\n", __func__, isa1000_vibr->isa1000_chip_en_pin);
				goto chip_dwn;
			}
		} else {
			pr_err("%s: Invalid gpio %d\n", __func__, isa1000_vibr->isa1000_chip_en_pin);
			goto gpio_request_error;
		}
		rc = gpio_direction_output(isa1000_vibr->isa1000_chip_en_pin, 0);
		if (rc) {
			pr_err("set_direction for isa1000_chip_en_pin gpio failed\n");
			goto chip_dwn;
		}
		pwm_disable(isa1000_vibr->pwm_info.pwm_dev);

		if (gpio_is_valid(isa1000_vibr->isa1000_vdd_en_pin)) {
			rc = gpio_request(isa1000_vibr->isa1000_vdd_en_pin, "vdd_en_pin");
			if (rc) {
				pr_err("%s: gpio %d request failed\n", __func__, isa1000_vibr->isa1000_vdd_en_pin);
				goto vdd_dwn;
			}
		} else {
			pr_err("%s: Invalid gpio %d\n", __func__, isa1000_vibr->isa1000_vdd_en_pin);
			goto chip_dwn;
		}
		gpio_direction_output(isa1000_vibr->isa1000_vdd_en_pin, 0);
		if (rc) {
			pr_err("set_direction for isa1000_vdd_en_pin gpio failed\n");
			goto vdd_dwn;
		}
		vib_level = 0;
	}

vdd_dwn:
	if (gpio_is_valid(isa1000_vibr->isa1000_vdd_en_pin))
		gpio_free(isa1000_vibr->isa1000_vdd_en_pin);
chip_dwn:
	if (gpio_is_valid(isa1000_vibr->isa1000_chip_en_pin))
		gpio_free(isa1000_vibr->isa1000_chip_en_pin);
gpio_request_error:

	return;
}
EXPORT_SYMBOL(isa1000_vib_set_level);

static int isa1000_vib_set(struct isa1000_vib *vib, int on)
{
	int rc = 0;
	if (on) {
		if (gpio_is_valid(vib->isa1000_chip_en_pin)) {
			rc = gpio_request(vib->isa1000_chip_en_pin, "chip_en_pin");
			if (rc) {
				pr_err("%s: gpio %d request failed\n", __func__, vib->isa1000_chip_en_pin);
				goto vdd_dwn;
			}
		} else {
			pr_err("%s: Invalid gpio %d\n", __func__, vib->isa1000_chip_en_pin);
			goto gpio_request_error;
		}

		rc = gpio_direction_output(vib->isa1000_chip_en_pin, 1);
		if (rc) {
			pr_err("set_direction for isa1000_chip_en_pin gpio failed\n");
			goto chip_dwn;
		}

		gpio_set_value(vib->isa1000_chip_en_pin, 1);

		if (gpio_is_valid(vib->isa1000_vdd_en_pin)) {
			rc = gpio_request(vib->isa1000_vdd_en_pin, "vdd_en_pin");
			if (rc) {
				pr_err("%s: gpio %d request failed\n", __func__, vib->isa1000_vdd_en_pin);
				goto vdd_dwn;
			}
		} else {
			pr_err("%s: Invalid gpio %d\n", __func__, vib->isa1000_vdd_en_pin);
			goto chip_dwn;
		}

		rc = gpio_direction_output(vib->isa1000_vdd_en_pin, 1);
		if (rc) {
			pr_err("set_direction for isa1000_vdd_en_pin gpio failed\n");
			goto vdd_dwn;
		}

		gpio_set_value(vib->isa1000_vdd_en_pin, 1);

		pwm_disable(vib->pwm_info.pwm_dev);

		rc = pwm_config(vib->pwm_info.pwm_dev, vib->pwm_info.duty_us,
						vib->pwm_info.period_us);


		if (rc < 0) {
			pr_err("vib pwm config failed\n");
			pwm_free(vib->pwm_info.pwm_dev);
			goto vdd_dwn;
		}

		rc = pwm_enable(vib->pwm_info.pwm_dev);
		if (rc < 0) {
			pr_err("%s: pwm_enable fail\n", __func__);
		goto vdd_dwn;
		}
	} else {
		if (gpio_is_valid(vib->isa1000_chip_en_pin)) {
			rc = gpio_request(vib->isa1000_chip_en_pin, "chip_en_pin");
			if (rc) {
				pr_err("%s: gpio %d request failed\n", __func__, vib->isa1000_chip_en_pin);
				goto chip_dwn;
			}
		} else {
			pr_err("%s: Invalid gpio %d\n", __func__, vib->isa1000_chip_en_pin);
			goto gpio_request_error;
		}
		rc = gpio_direction_output(vib->isa1000_chip_en_pin, 0);
		if (rc) {
			pr_err("set_direction for isa1000_chip_en_pin gpio failed\n");
			goto chip_dwn;
		}
		pwm_disable(vib->pwm_info.pwm_dev);

		if (gpio_is_valid(vib->isa1000_vdd_en_pin)) {
			rc = gpio_request(vib->isa1000_vdd_en_pin, "vdd_en_pin");
			if (rc) {
				pr_err("%s: gpio %d request failed\n", __func__, vib->isa1000_vdd_en_pin);
				goto vdd_dwn;
			}
		} else {
			pr_err("%s: Invalid gpio %d\n", __func__, vib->isa1000_vdd_en_pin);
			goto chip_dwn;
		}
		gpio_direction_output(vib->isa1000_vdd_en_pin, 0);
		if (rc) {
			pr_err("set_direction for isa1000_vdd_en_pin gpio failed\n");
			goto vdd_dwn;
		}
		vib_level = 0;
	}

vdd_dwn:
	if (gpio_is_valid(vib->isa1000_vdd_en_pin))
		gpio_free(vib->isa1000_vdd_en_pin);
chip_dwn:
	if (gpio_is_valid(vib->isa1000_chip_en_pin))
		gpio_free(vib->isa1000_chip_en_pin);
gpio_request_error:
	return rc;
}

static void isa1000_vib_update(struct work_struct *work)
{
	struct isa1000_vib *vib = container_of(work, struct isa1000_vib,
					 work);
	isa1000_vib_set(vib, vib->state);
}

static void isa1000_vib_enable(struct timed_output_dev *dev, int value)
{
	struct isa1000_vib *vib = container_of(dev, struct isa1000_vib,
					 timed_dev);

	mutex_lock(&vib->lock);
	hrtimer_cancel(&vib->vib_timer);

	if (value == 0)
		vib->state = 0;
	else {
		value = (value > vib->timeout ?
				 vib->timeout : value);

		vib->state = 1;
		hrtimer_start(&vib->vib_timer,
				ktime_set(value / 1000, (value % 1000) * 1000000),
				HRTIMER_MODE_REL);
	}
	pr_debug("isa1000_vib_enable:: value is %d\n", value);
	mutex_unlock(&vib->lock);
	schedule_work(&vib->work);
}

static int isa1000_vib_get_time(struct timed_output_dev *dev)
{
	struct isa1000_vib *vib = container_of(dev, struct isa1000_vib,
							 timed_dev);

	if (hrtimer_active(&vib->vib_timer)) {
		ktime_t r = hrtimer_get_remaining(&vib->vib_timer);
		return (int)ktime_to_us(r);


	} else
		return 0;
}

static enum hrtimer_restart isa1000_vib_timer_func(struct hrtimer *timer)
{
	struct isa1000_vib *vib = container_of(timer, struct isa1000_vib,
							 vib_timer);

	vib->state = 0;
	schedule_work(&vib->work);
	return HRTIMER_NORESTART;
}

#ifdef CONFIG_PM
static int isa1000_vibrator_suspend(struct device *dev)
{
	struct isa1000_vib *vib = dev_get_drvdata(dev);

	hrtimer_cancel(&vib->vib_timer);
	cancel_work_sync(&vib->work);
	/* turn-off vibrator */
	isa1000_vib_set(vib, 0);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(isa1000_vibrator_pm_ops, isa1000_vibrator_suspend, NULL);

static int isa1000_vibrator_probe(struct platform_device *pdev)
{
	struct isa1000_vib *vib;
	struct device_node *node = pdev->dev.of_node;
	u32 temp_val;
	int rc;

	vib = devm_kzalloc(&pdev->dev, sizeof(*vib), GFP_KERNEL);
	if (!vib) {
		dev_err(&pdev->dev, "%s:%d Unable to allocate memory\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	vib->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(vib->pinctrl)) {
		pr_err("%s:failed to get pinctrl\n", __func__);
		rc = PTR_ERR(vib->pinctrl);
		goto error;
	}

	vib->gpio_state_default = pinctrl_lookup_state(vib->pinctrl, "isa1000_default");
	if (IS_ERR(vib->gpio_state_default)) {
		pr_err("%s:can not get default pinstate\n", __func__);
		rc = -EINVAL;
		goto error;
	}

	rc = pinctrl_select_state(vib->pinctrl, vib->gpio_state_default);
	if (rc)
		pr_err("%s:set state failed!\n", __func__);

	vib->isa1000_chip_en_pin = of_get_named_gpio(node, "imagis, chip-en", 0);
	if (vib->isa1000_chip_en_pin < 0) {
		dev_err(&pdev->dev,
			"Looking up %s property in node %s failed. rc =  %d\n",
			"isa1000-chip-en-pin", node->full_name, vib->isa1000_chip_en_pin);
		goto error;
	}

	vib->isa1000_vdd_en_pin = of_get_named_gpio(node, "imagis, vdd-en", 0);
	if (vib->isa1000_vdd_en_pin < 0) {
		dev_err(&pdev->dev,
			"Looking up %s property in node %s failed. rc =  %d\n",
			"isa1000-vdd-en-pin", node->full_name, vib->isa1000_vdd_en_pin);
		goto error;
	}

	vib->timeout = ISA1000_VIB_DEFAULT_TIMEOUT;
	rc = of_property_read_u32(node, "imagis, max-timeout", &temp_val);
	if (!rc) {
		vib->timeout = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&pdev->dev, "Unable to read vib timeout\n");
		goto error;
	}

	rc = of_property_read_u32(node, "imagis, pwm-frequency", &temp_val);
	if (rc) {
		pr_err("%s:%d, Error, isa1000 pwm input pwm_period\n", __func__, __LINE__);
		goto error;
	}
	vib->pwm_info.period_us = NSEC_PER_SEC / temp_val;


	rc = of_property_read_u32(node, "imagis, duty_us", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(&pdev->dev, "Unable to read duty cycle\n");
		goto error;
	}
	vib->pwm_info.duty_us = temp_val * vib->pwm_info.period_us / 100;

	rc = of_property_read_u32(node, "imagis, pwm-ch-id", &temp_val);
	if (rc) {
		pr_err("%s:%d, Error, isa1000 pwm input lpg channel\n", __func__, __LINE__);
		goto error;
	}
	vib->pwm_info.pwm_channel = temp_val;

	temp_val = of_get_named_gpio(node, "imagis, pwm-gpio", 0);
	vib->pwm_pmic_gpio = temp_val;
	if (vib->pwm_pmic_gpio < 0) {
		dev_err(&pdev->dev,
			"Looking up %s property in node %s failed. rc =  %d\n",
			"pwm-pmic-gpio", node->full_name, vib->pwm_pmic_gpio);
		goto error;
	}




	vib->pwm_info.pwm_dev = pwm_request(vib->pwm_info.pwm_channel, "isa1000-vib");
	if (IS_ERR_OR_NULL(vib->pwm_info.pwm_dev)) {
		dev_err(&pdev->dev, "vib pwm request failed\n");
		rc = -ENODEV;
		goto error;
	}

	mutex_init(&vib->lock);
	INIT_WORK(&vib->work, isa1000_vib_update);

	hrtimer_init(&vib->vib_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vib->vib_timer.function = isa1000_vib_timer_func;

	vib->timed_dev.name = "vibrator";
	vib->timed_dev.get_time = isa1000_vib_get_time;
	vib->timed_dev.enable = isa1000_vib_enable;

	platform_set_drvdata(pdev, vib);

	rc = timed_output_dev_register(&vib->timed_dev);
	if (rc < 0) {
		pr_err("time output device register failed\n");
		goto error;
	}

	if (gpio_is_valid(vib->isa1000_vdd_en_pin)) {
		rc = gpio_request(vib->isa1000_vdd_en_pin, "vdd_en_pin");
		if (rc) {
			dev_err(&pdev->dev, "%s: gpio %d request failed\n",
					__func__, vib->isa1000_vdd_en_pin);
			goto vdd_en_gpio_fail;
		}
	} else {
		dev_err(&pdev->dev, "%s: Invalid gpio %d\n", __func__,
					vib->isa1000_vdd_en_pin);
		goto vdd_en_gpio_fail;
	}

	rc = gpio_direction_output(vib->isa1000_vdd_en_pin, 0);
	if (rc) {
		pr_err("set_direction for isa1000_vdd_en_pin gpio failed\n");
		goto pwm_config_fial;
	}

	gpio_set_value(vib->isa1000_vdd_en_pin, 1);

	if (gpio_is_valid(vib->isa1000_vdd_en_pin))
		gpio_free(vib->isa1000_vdd_en_pin);

	isa1000_vibr = vib;

	pr_info("%s: successfull\n", __func__);
	return 0;

pwm_config_fial:
	if (gpio_is_valid(vib->isa1000_vdd_en_pin))
		gpio_free(vib->isa1000_vdd_en_pin);
vdd_en_gpio_fail:
	timed_output_dev_unregister(&vib->timed_dev);
error:
	devm_kfree(&pdev->dev, vib);
	return rc;
}

static int isa1000_vibrator_remove(struct platform_device *pdev)
{
	struct isa1000_vib *vib = platform_get_drvdata(pdev);

	cancel_work_sync(&vib->work);
	hrtimer_cancel(&vib->vib_timer);
	/* turn-off vibrator */
	isa1000_vib_set(vib, 0);

	timed_output_dev_unregister(&vib->timed_dev);
	mutex_destroy(&vib->lock);
	isa1000_vibr = NULL;

	return 0;
}

static struct of_device_id isa_match_table[] = {
	{	.compatible = "imagis, isa1000-vibrator",
	},
	{}
};

static struct platform_driver isa1000_vibrator_driver = {
	.driver		= {
		.name	= "imagis, isa1000-vibrator",
		.of_match_table = isa_match_table,
		.pm	= &isa1000_vibrator_pm_ops,
	},
	.probe		= isa1000_vibrator_probe,
	.remove		= isa1000_vibrator_remove,
};

static int __init isa1000_vibrator_init(void)
{
	return platform_driver_register(&isa1000_vibrator_driver);
}
module_init(isa1000_vibrator_init);

static void __exit isa1000_vibrator_exit(void)
{
	return platform_driver_unregister(&isa1000_vibrator_driver);
}
module_exit(isa1000_vibrator_exit);

MODULE_DESCRIPTION("imagis isa1000 vibrator driver");
MODULE_LICENSE("GPL v2");
