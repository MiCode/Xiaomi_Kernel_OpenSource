/*
 * kernel/drivers/input/misc/alps_gpio_scrollwheel.c
 *
 * Copyright (c) 2010, NVIDIA Corporation.
 *
 * Driver for ScrollWheel on GPIO lines capable of generating interrupts.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio_scrollwheel.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>

struct scrollwheel_button_data {
	struct gpio_scrollwheel_button *button;
	struct input_dev *input;
	struct timer_list timer;
	struct work_struct work;
	int timer_debounce;	/* in msecs */
	int rotgpio;
	bool disabled;
};

struct gpio_scrollwheel_drvdata {
	struct input_dev *input;
	struct mutex disable_lock;
	unsigned int n_buttons;
	int (*enable)(struct device *dev);
	void (*disable)(struct device *dev);
	struct scrollwheel_button_data data[0];
};

static void scrollwheel_report_key(struct scrollwheel_button_data *bdata)
{
	struct gpio_scrollwheel_button *button = bdata->button;
	struct input_dev *input = bdata->input;
	int state = (gpio_get_value(button->gpio) ? 1 : 0) ^ \
				button->active_low;
	int state2 = 0;

	switch (button->pinaction) {
	case GPIO_SCROLLWHEEL_PIN_PRESS:
		input_report_key(input, KEY_ENTER, 1);
		input_report_key(input, KEY_ENTER, 0);
		input_sync(input);
		break;

	case GPIO_SCROLLWHEEL_PIN_ROT1:
	case GPIO_SCROLLWHEEL_PIN_ROT2:
		state2 = (gpio_get_value(bdata->rotgpio) ? 1 : 0) \
					^ button->active_low;
		if (state != state2) {
			input_report_key(input, KEY_DOWN, 1);
			input_report_key(input, KEY_DOWN, 0);
		} else {
			input_report_key(input, KEY_UP, 1);
			input_report_key(input, KEY_UP, 0);
		}
		input_sync(input);
		break;

	default:
		pr_err("%s:Line=%d, Invalid Pinaction\n", __func__, __LINE__);
	}
}

static void scrollwheel_work_func(struct work_struct *work)
{
	struct scrollwheel_button_data *bdata =
		container_of(work, struct scrollwheel_button_data, work);

	scrollwheel_report_key(bdata);
}

static void scrollwheel_timer(unsigned long _data)
{
	struct scrollwheel_button_data *data = \
		(struct scrollwheel_button_data *)_data;

	schedule_work(&data->work);
}

static irqreturn_t scrollwheel_isr(int irq, void *dev_id)
{
	struct scrollwheel_button_data *bdata = dev_id;
	struct gpio_scrollwheel_button *button = bdata->button;

	BUG_ON(irq != gpio_to_irq(button->gpio));

	if (bdata->timer_debounce)
		mod_timer(&bdata->timer,
			jiffies + msecs_to_jiffies(bdata->timer_debounce));
	else
		schedule_work(&bdata->work);

	return IRQ_HANDLED;
}

static int gpio_scrollwheel_setup_key(struct platform_device *pdev,
			struct scrollwheel_button_data *bdata,
			struct gpio_scrollwheel_button *button)
{
	char *desc = button->desc ? button->desc : "gpio_scrollwheel";
	struct device *dev = &pdev->dev;
	unsigned long irqflags;
	int irq, error;

	setup_timer(&bdata->timer, scrollwheel_timer, (unsigned long)bdata);
	INIT_WORK(&bdata->work, scrollwheel_work_func);

	error = gpio_request(button->gpio, desc);
	if (error < 0) {
		dev_err(dev, "failed to request GPIO %d, error %d\n",
			button->gpio, error);
		return error;
	}

	error = gpio_direction_input(button->gpio);
	if (error < 0) {
		dev_err(dev, "failed to configure"
			" direction for GPIO %d, error %d\n",
			button->gpio, error);
		goto fail;
	}

	if (button->debounce_interval) {
		error = gpio_set_debounce(button->gpio,
					  button->debounce_interval * 1000);
		/* use timer if gpiolib doesn't provide debounce */
		if (error < 0)
			bdata->timer_debounce = button->debounce_interval;
	}

	irq = gpio_to_irq(button->gpio);
	if (irq < 0) {
		error = irq;
		dev_err(dev, "Unable to get irq no for GPIO %d, error %d\n",
			button->gpio, error);
		goto fail;
	}

	irqflags = IRQF_TRIGGER_FALLING;

	error = request_irq(irq, scrollwheel_isr, irqflags, desc, bdata);
	if (error) {
		dev_err(dev, "Unable to claim irq %d; error %d\n",
			irq, error);
		goto fail;
	}

	return 0;

fail:
	return error;
}

static int gpio_scrollwheel_open(struct input_dev *input)
{
	struct gpio_scrollwheel_drvdata *ddata = input_get_drvdata(input);

	return ddata->enable ? ddata->enable(input->dev.parent) : 0;
}

static void gpio_scrollwheel_close(struct input_dev *input)
{
	struct gpio_scrollwheel_drvdata *ddata = input_get_drvdata(input);

	if (ddata->disable)
		ddata->disable(input->dev.parent);
}

static int gpio_scrollwheel_probe(struct platform_device *pdev)
{
	struct gpio_scrollwheel_platform_data *pdata = pdev->dev.platform_data;
	struct gpio_scrollwheel_drvdata *ddata;
	struct device *dev = &pdev->dev;
	struct input_dev *input;
	int i, error;

	ddata = kzalloc(sizeof(struct gpio_scrollwheel_drvdata) +
		pdata->nbuttons * sizeof(struct scrollwheel_button_data),
		GFP_KERNEL);
	if (ddata == NULL) {
		dev_err(dev, "failed to allocate memory\n");
		error = -ENOMEM;
		return error;
	}

	input = input_allocate_device();
	if (input == NULL) {
		dev_err(dev, "failed to allocate input device\n");
		error = -ENOMEM;
		kfree(ddata);
		return error;
	}

	ddata->input = input;
	ddata->n_buttons = pdata->nbuttons;
	ddata->enable = pdata->enable;
	ddata->disable = pdata->disable;
	mutex_init(&ddata->disable_lock);

	platform_set_drvdata(pdev, ddata);
	input_set_drvdata(input, ddata);

	input->name = pdev->name;
	input->phys = "gpio-scrollwheel/input0";
	input->dev.parent = &pdev->dev;
	input->open = gpio_scrollwheel_open;
	input->close = gpio_scrollwheel_close;

	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0100;

	/* Enable auto repeat feature of Linux input subsystem */
	if (pdata->rep)
		__set_bit(EV_REP, input->evbit);

	for (i = 0; i < pdata->nbuttons; i++) {
		struct gpio_scrollwheel_button *button = &pdata->buttons[i];
		struct scrollwheel_button_data *bdata = &ddata->data[i];

		bdata->input = input;
		bdata->button = button;

		if (button->pinaction == GPIO_SCROLLWHEEL_PIN_PRESS ||
			button->pinaction == GPIO_SCROLLWHEEL_PIN_ROT1) {
			error = gpio_scrollwheel_setup_key(pdev, bdata, button);
			if (error)
				goto fail;
		} else {
			if (button->pinaction == GPIO_SCROLLWHEEL_PIN_ONOFF) {
				gpio_request(button->gpio, button->desc);
				gpio_direction_output(button->gpio, 0);
			}

			if (button->pinaction == GPIO_SCROLLWHEEL_PIN_ROT2) {
				gpio_request(button->gpio, button->desc);
				gpio_direction_input(button->gpio);
				/* Save rot2 gpio number in rot1 context */
				ddata->data[2].rotgpio = button->gpio;
			}
		}
	}

	/* set input capability */
	__set_bit(EV_KEY, input->evbit);
	__set_bit(KEY_ENTER, input->keybit);
	__set_bit(KEY_UP, input->keybit);
	__set_bit(KEY_DOWN, input->keybit);

	error = input_register_device(input);
	if (error) {
		dev_err(dev, "Unable to register input device, error: %d\n",
			error);
		goto fail;
	}

	input_sync(input);

	return 0;

fail:
	while (--i >= 0) {
		if (pdata->buttons[i].pinaction == GPIO_SCROLLWHEEL_PIN_PRESS ||
			pdata->buttons[i].pinaction == GPIO_SCROLLWHEEL_PIN_ROT1) {
			free_irq(gpio_to_irq(pdata->buttons[i].gpio), &ddata->data[i]);
			if (ddata->data[i].timer_debounce)
				del_timer_sync(&ddata->data[i].timer);
			cancel_work_sync(&ddata->data[i].work);
		}
		gpio_free(pdata->buttons[i].gpio);
	}

	platform_set_drvdata(pdev, NULL);
	input_free_device(input);
	kfree(ddata);
	return error;
}

static int gpio_scrollwheel_remove(struct platform_device *pdev)
{
	struct gpio_scrollwheel_platform_data *pdata = pdev->dev.platform_data;
	struct gpio_scrollwheel_drvdata *ddata = platform_get_drvdata(pdev);
	struct input_dev *input = ddata->input;
	int i;

	for (i = 0; i < pdata->nbuttons; i++) {
		if (pdata->buttons[i].pinaction == GPIO_SCROLLWHEEL_PIN_PRESS ||
			pdata->buttons[i].pinaction == GPIO_SCROLLWHEEL_PIN_ROT1) {
			int irq = gpio_to_irq(pdata->buttons[i].gpio);
			free_irq(irq, &ddata->data[i]);
			if (ddata->data[i].timer_debounce)
				del_timer_sync(&ddata->data[i].timer);
			cancel_work_sync(&ddata->data[i].work);
		}
		gpio_free(pdata->buttons[i].gpio);
	}

	input_unregister_device(input);

	return 0;
}


#ifdef CONFIG_PM
static int gpio_scrollwheel_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct gpio_scrollwheel_platform_data *pdata = pdev->dev.platform_data;
	struct gpio_scrollwheel_drvdata *ddata = platform_get_drvdata(pdev);
	int i, irq;

	for (i = 0; i < pdata->nbuttons; i++) {
		if (pdata->buttons[i].pinaction == GPIO_SCROLLWHEEL_PIN_PRESS ||
			pdata->buttons[i].pinaction == GPIO_SCROLLWHEEL_PIN_ROT1) {
			irq = gpio_to_irq(pdata->buttons[i].gpio);
			disable_irq(irq);
			if (ddata->data[i].timer_debounce)
				del_timer_sync(&ddata->data[i].timer);
			cancel_work_sync(&ddata->data[i].work);
		} else {
			if (pdata->buttons[i].pinaction == GPIO_SCROLLWHEEL_PIN_ONOFF)
				gpio_direction_output(pdata->buttons[i].gpio, 1);
			else {
				irq = gpio_to_irq(pdata->buttons[i].gpio);
				disable_irq(irq);
			}
		}
	}
	return 0;
}

static int gpio_scrollwheel_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct gpio_scrollwheel_platform_data *pdata = pdev->dev.platform_data;
	struct gpio_scrollwheel_drvdata *ddata = platform_get_drvdata(pdev);
	int i, irq;

	for (i = 0; i < pdata->nbuttons; i++) {
		if (pdata->buttons[i].pinaction == GPIO_SCROLLWHEEL_PIN_PRESS ||
			pdata->buttons[i].pinaction == GPIO_SCROLLWHEEL_PIN_ROT1) {
			irq = gpio_to_irq(pdata->buttons[i].gpio);
			enable_irq(irq);
			if (ddata->data[i].timer_debounce)
				setup_timer(&ddata->data[i].timer,\
				scrollwheel_timer, (unsigned long)&ddata->data[i]);

			INIT_WORK(&ddata->data[i].work, scrollwheel_work_func);
		} else {
			if (pdata->buttons[i].pinaction == GPIO_SCROLLWHEEL_PIN_ONOFF)
				gpio_direction_output(pdata->buttons[i].gpio, 0);
			else {
				irq = gpio_to_irq(pdata->buttons[i].gpio);
				enable_irq(irq);
			}
		}
	}

	return 0;
}

static const struct dev_pm_ops gpio_scrollwheel_pm_ops = {
	.suspend	= gpio_scrollwheel_suspend,
	.resume		= gpio_scrollwheel_resume,
};
#endif

static struct platform_driver gpio_scrollwheel_device_driver = {
	.probe		= gpio_scrollwheel_probe,
	.remove		= gpio_scrollwheel_remove,
	.driver		= {
		.name	= "alps-gpio-scrollwheel",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &gpio_scrollwheel_pm_ops,
#endif
	}
};

static int __init gpio_scrollwheel_init(void)
{
	return platform_driver_register(&gpio_scrollwheel_device_driver);
}

static void __exit gpio_scrollwheel_exit(void)
{
	platform_driver_unregister(&gpio_scrollwheel_device_driver);
}

module_init(gpio_scrollwheel_init);
module_exit(gpio_scrollwheel_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NVIDIA Corporation");
MODULE_DESCRIPTION("Alps SRBE ScrollWheel driver");

MODULE_ALIAS("platform:alps-gpio-scrollwheel");
