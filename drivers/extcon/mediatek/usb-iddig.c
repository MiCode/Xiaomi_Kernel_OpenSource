/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/jiffies.h>
#include <linux/list.h>
#include <linux/atomic.h>
#include <extcon_usb.h>

#define RET_SUCCESS 0
#define RET_FAIL 1

struct usb_iddig_info {
	struct device *dev;
	struct gpio_desc *id_gpiod;
	int id_irq;
	unsigned long id_swdebounce;
	unsigned long id_hwdebounce;
	struct delayed_work id_delaywork;
	struct pinctrl *pinctrl;
	struct pinctrl_state *id_init;
	struct pinctrl_state *id_enable;
	struct pinctrl_state *id_disable;
};


enum idpin_state {
	IDPIN_OUT,
	IDPIN_IN_HOST,
	IDPIN_IN_DEVICE,
};

static enum idpin_state mtk_idpin_cur_stat = IDPIN_OUT;

static void mtk_set_iddig_out_detect(struct usb_iddig_info *info)
{
	irq_set_irq_type(info->id_irq, IRQF_TRIGGER_HIGH);
	enable_irq(info->id_irq);
}

static void mtk_set_iddig_in_detect(struct usb_iddig_info *info)
{
	irq_set_irq_type(info->id_irq, IRQF_TRIGGER_LOW);
	enable_irq(info->id_irq);
}


static void iddig_mode_switch(struct work_struct *work)
{
	struct usb_iddig_info *info = container_of(to_delayed_work(work),
						    struct usb_iddig_info,
						    id_delaywork);

	if (mtk_idpin_cur_stat == IDPIN_OUT) {
		mtk_idpin_cur_stat = IDPIN_IN_HOST;
		mt_vbus_on();
		mt_usbhost_connect();
		mtk_set_iddig_out_detect(info);
	} else {
		mtk_idpin_cur_stat = IDPIN_OUT;
		mt_usbhost_disconnect();
		mt_vbus_off();
		mtk_set_iddig_in_detect(info);
	}
}

static irqreturn_t iddig_eint_isr(int irqnum, void *data)
{
	struct usb_iddig_info *info = data;

	disable_irq_nosync(irqnum);
	schedule_delayed_work(&info->id_delaywork,
		msecs_to_jiffies(info->id_swdebounce));

	return IRQ_HANDLED;
}

static int otg_iddig_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct usb_iddig_info *info;
	struct pinctrl *pinctrl;
	u32 ints[2] = {0, 0};

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	info->dev = dev;

	info->id_irq = irq_of_parse_and_map(node, 0);
	if (info->id_irq < 0)
		return -ENODEV;

	pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pinctrl)) {
		dev_err(&pdev->dev, "No find id pinctrl!\n");
		return -1;
	}

	info->pinctrl = pinctrl;

	info->id_init = pinctrl_lookup_state(pinctrl, "id_init");
	if (IS_ERR(info->id_init))
		dev_err(&pdev->dev, "No find pinctrl id_init\n");
	else
		pinctrl_select_state(info->pinctrl, info->id_init);

	info->id_enable = pinctrl_lookup_state(pinctrl, "id_enable");
	info->id_disable = pinctrl_lookup_state(pinctrl, "id_disable");
	if (IS_ERR(info->id_enable))
		dev_err(&pdev->dev, "No find pinctrl iddig_enable\n");
	if (IS_ERR(info->id_disable))
		dev_err(&pdev->dev, "No find pinctrl iddig_disable\n");

	ret = of_property_read_u32_array(node, "debounce",
		ints, ARRAY_SIZE(ints));
	if (!ret)
		info->id_hwdebounce = ints[1];

	info->id_swdebounce = msecs_to_jiffies(50);

	INIT_DELAYED_WORK(&info->id_delaywork, iddig_mode_switch);

	ret = devm_request_irq(dev, info->id_irq, iddig_eint_isr,
					0, pdev->name, info);
	if (ret < 0) {
		dev_err(dev, "failed to request handler for ID IRQ\n");
		return ret;
	}

	platform_set_drvdata(pdev, info);

	return 0;
}

static int otg_iddig_remove(struct platform_device *pdev)
{
	struct usb_iddig_info *info = platform_get_drvdata(pdev);

	cancel_delayed_work(&info->id_delaywork);

	return 0;
}

static const struct of_device_id otg_iddig_of_match[] = {
	{.compatible = "mediatek,usb_iddig_bi_eint"},
	{},
};

static struct platform_driver otg_iddig_driver = {
	.probe = otg_iddig_probe,
	.remove = otg_iddig_remove,
	.driver = {
		.name = "otg_iddig",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(otg_iddig_of_match),
	},
};

static int __init otg_iddig_init(void)
{
	return platform_driver_register(&otg_iddig_driver);
}
late_initcall(otg_iddig_init);

static void __exit otg_iddig_cleanup(void)
{
	platform_driver_unregister(&otg_iddig_driver);
}

module_exit(otg_iddig_cleanup);

