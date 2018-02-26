/* drivers/misc/timed_gpio.c
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include "../staging/android/timed_output.h"
#include "../staging/android/timed_gpio.h"
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#define GPIO_VIB_NAME "gpio-vibrator"

struct timed_gpio_data {
	struct timed_output_dev dev;
	struct hrtimer timer;
	spinlock_t lock;
	unsigned gpio;
	int max_timeout;
	u8 active_low;
};
struct gpio_vib {
	const char *name;
	unsigned 	gpio;
	int 	max_timeout;
	unsigned	active_low : 1;
};
static void delete_gpio_vib(struct timed_gpio_data *vib)
{
	if (!gpio_is_valid(vib->gpio))
		return;
	timed_output_dev_unregister(&vib->dev);
	gpio_free(vib->gpio);
}
static enum hrtimer_restart gpio_timer_func(struct hrtimer *timer)
{
	struct timed_gpio_data *data =
			container_of(timer, struct timed_gpio_data, timer);

	gpio_direction_output(data->gpio, data->active_low ? 1 : 0);
	return HRTIMER_NORESTART;
}

static int gpio_get_time(struct timed_output_dev *dev)
{
	struct timed_gpio_data *data;
	struct timeval t;

	data = container_of(dev, struct timed_gpio_data, dev);

	if (!hrtimer_active(&data->timer))
		return 0;

	t = ktime_to_timeval(hrtimer_get_remaining(&data->timer));

	return t.tv_sec * 1000 + t.tv_usec / 1000;
}

static void gpio_enable(struct timed_output_dev *dev, int value)
{
	struct timed_gpio_data	*data =
			container_of(dev, struct timed_gpio_data, dev);
	unsigned long	flags;

	spin_lock_irqsave(&data->lock, flags);


	hrtimer_cancel(&data->timer);
	gpio_direction_output(data->gpio, data->active_low ? !value : !!value);

	if (value > 0) {
		if (value > data->max_timeout)
			value = data->max_timeout;
			printk("max_timeout is %d\n", data->max_timeout);
		hrtimer_start(&data->timer,
				ktime_set(value / 1000, (value % 1000) * 1000000),
				HRTIMER_MODE_REL);
	}

	spin_unlock_irqrestore(&data->lock, flags);
}
static int create_gpio_vib(const struct gpio_vib *template,
	struct timed_gpio_data *gpio_dat, struct device *parent)
{
	int ret;
	gpio_dat->gpio = template->gpio;
	hrtimer_init(&gpio_dat->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	gpio_dat->timer.function = gpio_timer_func;
	spin_lock_init(&gpio_dat->lock);
	gpio_dat->dev.name = template->name;
	gpio_dat->dev.get_time = gpio_get_time;
	gpio_dat->dev.enable = gpio_enable;
	ret = gpio_request(gpio_dat->gpio, gpio_dat->dev.name);
	if (ret < 0)
		return 0;
	ret = timed_output_dev_register(&gpio_dat->dev);
	if (ret < 0) {
		gpio_free(gpio_dat->gpio);
		return 0;
	}
	gpio_dat->max_timeout = template->max_timeout;
	gpio_dat->active_low = template->active_low;
	gpio_direction_output(gpio_dat->gpio, gpio_dat->active_low);
	return 0;
}
struct gpio_vibs_priv {
	int num_vibs;
	struct timed_gpio_data vibs[];
};

static inline int sizeof_gpio_vibs_priv(int num_vibs)
{
	return sizeof(struct gpio_vibs_priv) +
			(sizeof(struct timed_gpio_data) * num_vibs);
}


#define DEFAULT_TIME_MAX_MS 50000
static struct gpio_vibs_priv *gpio_vibs_create_of(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node, *child;
	int count, ret;
	unsigned timerv;
	struct gpio_vibs_priv *priv;
	count = of_get_child_count(np);
	if (!count)
		return ERR_PTR(-ENODEV);

	for_each_child_of_node(np, child)
		if (of_get_gpio(child, 0) == -EPROBE_DEFER)
			return ERR_PTR(-EPROBE_DEFER);

	priv = devm_kzalloc(&pdev->dev, sizeof_gpio_vibs_priv(count),
			GFP_KERNEL);
	if (!priv)
		return ERR_PTR(-ENOMEM);

	for_each_child_of_node(np, child) {
		struct gpio_vib vib = {};
		enum of_gpio_flags flags;
		vib.gpio = of_get_gpio_flags(child, 0, &flags);
		vib.active_low = flags & OF_GPIO_ACTIVE_LOW;
		vib.name = of_get_property(child, "label", NULL) ? : child->name;
		ret = of_property_read_u32(child, "max_timeout",  &timerv);
		if (!ret)  {
			vib.max_timeout = timerv;
			printk("wingtech vib.max_timeout = %d\n", vib.max_timeout);
		} else{
			vib.max_timeout = DEFAULT_TIME_MAX_MS;
			printk("wingtech vib.max_timeout used default = %d, ret=%d\n", vib.max_timeout, ret);
		}

		ret = create_gpio_vib(&vib, &priv->vibs[priv->num_vibs++],
				&pdev->dev);
		if (ret < 0) {
			of_node_put(child);
			goto err;
		}
	}
	return priv;
	err:
	for (count = priv->num_vibs - 2; count >= 0; count--)
		delete_gpio_vib(&priv->vibs[count]);
	return ERR_PTR(-ENODEV);
}


static int timed_gpio_probe(struct platform_device *pdev)
{
	struct gpio_vibs_priv *priv;
	priv = gpio_vibs_create_of(pdev);
		if (IS_ERR(priv))
			return PTR_ERR(priv);
	platform_set_drvdata(pdev, priv);
	return 0;
}

static int timed_gpio_remove(struct platform_device *pdev)
{
	struct timed_gpio_platform_data *pdata = pdev->dev.platform_data;
	struct timed_gpio_data *gpio_data = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < pdata->num_gpios; i++) {
		timed_output_dev_unregister(&gpio_data[i].dev);
		gpio_free(gpio_data[i].gpio);
	}

	return 0;
}
static struct of_device_id vib_match_table[] = {
	{ .compatible = "gpio-vibrator",
	},
	{}
};
static struct platform_driver timed_gpio_driver = {
	.probe = timed_gpio_probe,
	.remove = timed_gpio_remove,
	.driver = {
		.name = GPIO_VIB_NAME,
		.owner = THIS_MODULE,
		.of_match_table = vib_match_table,
	},
};

module_platform_driver(timed_gpio_driver);

MODULE_AUTHOR("Mike Lockwood <lockwood@android.com>");
MODULE_DESCRIPTION("timed gpio driver");
MODULE_LICENSE("GPL");
