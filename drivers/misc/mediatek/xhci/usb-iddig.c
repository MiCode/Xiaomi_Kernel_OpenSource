/*
 * Copyright (C) 2016 MediaTek Inc.
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
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/jiffies.h>
#include <linux/list.h>
#include <linux/atomic.h>

#include <xhci-mtk-driver.h>

#define RET_SUCCESS 0
#define RET_FAIL 1

static struct pinctrl *pinctrl;
static struct pinctrl_state *pinctrl_iddig_init;

#ifdef CONFIG_USB_MTK_OTG_SWITCH
static bool otg_switch_state;
static struct pinctrl_state *pinctrl_iddig_enable;
static struct pinctrl_state *pinctrl_iddig_disable;

static struct mutex otg_switch_mutex;
static ssize_t otg_mode_show(struct device *dev,
				struct device_attribute *attr, char *buf);
static ssize_t otg_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

DEVICE_ATTR(otg_mode, 0664, otg_mode_show, otg_mode_store);

static struct attribute *otg_attributes[] = {
	&dev_attr_otg_mode.attr,
	NULL
};

static const struct attribute_group otg_attr_group = {
	.attrs = otg_attributes,
};

static const struct of_device_id otg_switch_of_match[] = {
	{.compatible = "mediatek,otg_switch"},
	{},
};

#endif

#ifdef CONFIG_USB_VBUS_GPIO
struct platform_device *g_pdev;
#endif
static const struct of_device_id otg_iddig_of_match[] = {
	{.compatible = "mediatek,usb_iddig_bi_eint"},
	{},
};

static bool otg_iddig_isr_enable;
static int mtk_xhci_eint_iddig_irq_en(void);

enum idpin_state {
	IDPIN_OUT,
	IDPIN_IN_HOST,
	IDPIN_IN_DEVICE,
};

static int mtk_idpin_irqnum;
static enum idpin_state mtk_idpin_cur_stat = IDPIN_OUT;

static struct delayed_work mtk_xhci_delaywork;

int mtk_iddig_debounce = 50;
module_param(mtk_iddig_debounce, int, 0400);

void switch_int_to_host_and_mask(void)
{
	irq_set_irq_type(mtk_idpin_irqnum, IRQF_TRIGGER_LOW);
	disable_irq(mtk_idpin_irqnum);
}

void switch_int_to_host(void)
{
	irq_set_irq_type(mtk_idpin_irqnum, IRQF_TRIGGER_LOW);
	enable_irq(mtk_idpin_irqnum);
}

static void mtk_set_iddig_out_detect(void)
{
	irq_set_irq_type(mtk_idpin_irqnum, IRQF_TRIGGER_HIGH);
	enable_irq(mtk_idpin_irqnum);
}

static void mtk_set_iddig_in_detect(void)
{
	irq_set_irq_type(mtk_idpin_irqnum, IRQF_TRIGGER_LOW);
	enable_irq(mtk_idpin_irqnum);
}

bool mtk_is_usb_id_pin_short_gnd(void)
{
	return (mtk_idpin_cur_stat != IDPIN_OUT) ? true : false;
}

void mtk_set_host_mode_in_host(void)
{
	mtk_idpin_cur_stat = IDPIN_IN_HOST;
}

void mtk_set_host_mode_out(void)
{
	mtk_idpin_cur_stat = IDPIN_OUT;
}


void mtk_enable_host(void)
{
	switch_int_to_host();	/* resotre ID pin interrupt */
}

void mtk_disable_host(void)
{
	switch_int_to_host_and_mask();
	if (mtk_is_host_mode() == true)
		mtk_xhci_driver_unload(true);
	mtk_idpin_cur_stat = IDPIN_OUT;
}


void mtk_xhci_mode_switch(struct work_struct *work)
{
	static bool is_load;
	int ret = 0;

	mtk_xhci_mtk_printk(K_DEBUG, "%s\n", __func__);

	if (mtk_idpin_cur_stat == IDPIN_OUT) {
		is_load = false;

		/* expect next isr is for id-pin out action */
		mtk_idpin_cur_stat =
		(mtk_is_charger_4_vol()) ? IDPIN_IN_DEVICE : IDPIN_IN_HOST;
		/* make id pin to detect the plug-out */
		mtk_set_iddig_out_detect();

		if (mtk_idpin_cur_stat == IDPIN_IN_DEVICE)
			goto done;

		ret = mtk_xhci_driver_load(true);
		if (!ret)
			is_load = true;
	} else {
		if (is_load) {
			mtk_xhci_driver_unload(true);
			is_load = false;
		}

		/* expect next isr is for id-pin in action */
		mtk_idpin_cur_stat = IDPIN_OUT;
		/* make id pin to detect the plug-in */
		mtk_set_iddig_in_detect();
	}

done:
	mtk_xhci_mtk_printk(K_ALET, "current mode is %s, ret(%d)\n",
		(mtk_idpin_cur_stat == IDPIN_IN_HOST) ? "host" :
		(mtk_idpin_cur_stat == IDPIN_IN_DEVICE) ? "id_device" :
		"device", ret);
}

static irqreturn_t xhci_eint_iddig_isr(int irqnum, void *data)
{
	disable_irq_nosync(irqnum);
	schedule_delayed_work(&mtk_xhci_delaywork,
		msecs_to_jiffies(mtk_iddig_debounce));
	mtk_xhci_mtk_printk(K_DEBUG, "%s\n", __func__);

	return IRQ_HANDLED;
}

static int mtk_xhci_eint_iddig_irq_en(void)
{
	int retval = 0;

	if (!otg_iddig_isr_enable) {
		retval =
			request_irq(mtk_idpin_irqnum, xhci_eint_iddig_isr,
			IRQF_TRIGGER_LOW, "iddig_eint", NULL);

		if (retval != 0) {
			mtk_xhci_mtk_printk(K_ERR,
			"request_irq fail, ret %d, irqnum %d!!!\n", retval,
			 mtk_idpin_irqnum);
		} else {
			enable_irq_wake(mtk_idpin_irqnum);
			otg_iddig_isr_enable = true;
		}
	} else {
#ifdef CONFIG_USB_MTK_OTG_SWITCH
		switch_int_to_host();	/* restore ID pin interrupt */
#endif
	}
	return retval;
}

static int otg_iddig_probe(struct platform_device *pdev)
{
	int retval = 0;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	int iddig_gpio, iddig_debounce;
	u32 ints[2] = {0, 0};

#ifdef CONFIG_USB_VBUS_GPIO
	g_pdev = pdev;
#endif

	mtk_idpin_irqnum = irq_of_parse_and_map(node, 0);
	if (mtk_idpin_irqnum < 0)
		return -ENODEV;

	pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pinctrl)) {
		dev_info(&pdev->dev, "Cannot find usb pinctrl!\n");
		return -1;
	}

	pinctrl_iddig_init = pinctrl_lookup_state(pinctrl, "iddig_init");
	if (IS_ERR(pinctrl_iddig_init))
		dev_info(&pdev->dev, "Cannot find usb pinctrl iddig_init\n");
	else
		pinctrl_select_state(pinctrl, pinctrl_iddig_init);


#ifdef CONFIG_USB_MTK_OTG_SWITCH
	pinctrl_iddig_enable = pinctrl_lookup_state(pinctrl, "iddig_enable");
	pinctrl_iddig_disable = pinctrl_lookup_state(pinctrl, "iddig_disable");
	if (IS_ERR(pinctrl_iddig_enable))
		dev_info(&pdev->dev, "Cannot find usb pinctrl iddig_enable\n");

	if (IS_ERR(pinctrl_iddig_disable))
		dev_info(&pdev->dev, "Cannot find usb pinctrl iddig_disable\n");
	else
		pinctrl_select_state(pinctrl, pinctrl_iddig_disable);
#endif

	retval = of_property_read_u32_array(node, "debounce", ints,
			ARRAY_SIZE(ints));
	if (!retval) {
		iddig_gpio = ints[0];
		iddig_debounce = ints[1];
		mtk_xhci_mtk_printk(K_DEBUG, "iddig gpio num = %d\n",
			mtk_idpin_irqnum);
		/*mt_gpio_set_debounce(iddig_gpio, iddig_debounce);*/
	}

	INIT_DELAYED_WORK(&mtk_xhci_delaywork, mtk_xhci_mode_switch);

#ifdef CONFIG_USB_MTK_OTG_SWITCH
#ifdef CONFIG_SYSFS
	retval = sysfs_create_group(&pdev->dev.kobj, &otg_attr_group);
	if (retval < 0) {
		dev_info(&pdev->dev,
			"Cannot register USB bus sysfs attributes: %d\n",
			retval);
		return retval;
	}
	mutex_init(&otg_switch_mutex);
#endif
#else
	retval = mtk_xhci_eint_iddig_irq_en();
#endif
	return 0;
}

static int otg_iddig_remove(struct platform_device *pdev)
{
	if (otg_iddig_isr_enable) {
		disable_irq_nosync(mtk_idpin_irqnum);
		free_irq(mtk_idpin_irqnum, NULL);
		otg_iddig_isr_enable = false;
	}

	cancel_delayed_work(&mtk_xhci_delaywork);
	mtk_idpin_cur_stat = IDPIN_OUT;

	mtk_xhci_mtk_printk(K_DEBUG, "external iddig unregister done.\n");

#ifdef CONFIG_USB_MTK_OTG_SWITCH
#ifdef CONFIG_SYSFS
	sysfs_remove_group(&pdev->dev.kobj, &otg_attr_group);
#endif
#endif
	return 0;
}

static void otg_iddig_shutdown(struct platform_device *pdev)
{
	if (mtk_is_host_mode() == true) {
		mtk_xhci_disable_vbus();
		mtk_xhci_mtk_printk(K_ALET, "otg_disable_vbus\n");
	}
}

static struct platform_driver otg_iddig_driver = {
	.probe = otg_iddig_probe,
	.remove = otg_iddig_remove,
	.shutdown = otg_iddig_shutdown,
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


#ifdef CONFIG_USB_MTK_OTG_SWITCH
static ssize_t otg_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (!dev) {
		dev_info(dev, "otg_mode_store no dev\n");
		return 0;
	}

	return sprintf(buf, "%d\n", otg_switch_state);
}

static ssize_t otg_mode_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	unsigned int mode;

	if (!dev) {
		dev_info(dev, "%s no dev\n", __func__);
		return count;
	}

	mutex_lock(&otg_switch_mutex);
	if (sscanf(buf, "%ud", &mode) == 1) {
		if (mode == 0) {
			mtk_xhci_mtk_printk(K_DEBUG, "otg_mode_enable start\n");
			if (otg_switch_state == true) {
				if (otg_iddig_isr_enable) {
					disable_irq(mtk_idpin_irqnum);
					irq_set_irq_type(mtk_idpin_irqnum,
						IRQF_TRIGGER_LOW);
				}
				cancel_delayed_work_sync(&mtk_xhci_delaywork);
				if (mtk_is_host_mode() == true)
					mtk_xhci_driver_unload(true);
				mtk_idpin_cur_stat = IDPIN_OUT;


				if (!IS_ERR(pinctrl_iddig_disable))
					pinctrl_select_state(pinctrl,
						pinctrl_iddig_disable);

				otg_switch_state = false;
			}
			mtk_xhci_mtk_printk(K_DEBUG, "otg_mode_enable end\n");
		} else {
			mtk_xhci_mtk_printk(K_DEBUG,
				"otg_mode_disable start\n");
			if (otg_switch_state == false) {
				otg_switch_state = true;
				if (!IS_ERR(pinctrl_iddig_enable))
					pinctrl_select_state(pinctrl,
						pinctrl_iddig_enable);

				msleep(20);
				mtk_xhci_eint_iddig_irq_en();
			}
			mtk_xhci_mtk_printk(K_DEBUG, "otg_mode_disable end\n");
		}
	}
	mutex_unlock(&otg_switch_mutex);
	return count;
}
#endif




