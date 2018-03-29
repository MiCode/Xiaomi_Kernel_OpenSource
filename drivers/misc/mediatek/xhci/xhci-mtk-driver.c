/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <xhci.h>
#include "xhci-mtk-driver.h"
#include "xhci-mtk-power.h"
#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#ifdef CONFIG_USB_MTK_DUALMODE
/*#include <mt-plat/eint.h>*/
#include <linux/irq.h>
#include <linux/switch.h>
#include <linux/sched.h>
#include <linux/module.h>
#undef DRV_Reg32
#undef DRV_WriteReg32
#include <mt-plat/battery_meter.h>


#undef DRV_Reg32
#undef DRV_WriteReg32
#include <mt-plat/mt_gpio.h>



#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#ifdef CONFIG_PROJECT_PHY
#include <mtk-phy-asic.h>
#endif
#endif

#include <hub.h>
#include <linux/jiffies.h>
#include <linux/list.h>
#include <linux/atomic.h>
#include <linux/usb/hcd.h>
/*#include <mach/mt_chip.h>*/

#ifdef CONFIG_USB_C_SWITCH
#include <typec.h>
#endif

#ifdef CONFIG_MTK_FPGA
#include <linux/mu3phy/mtk-phy.h>
#endif

#ifdef CONFIG_USB_MTK_DUALMODE
#ifdef CONFIG_MTK_OTG_PMIC_BOOST_5V
#ifdef CONFIG_MTK_OTG_OC_DETECTOR
#include <linux/kobject.h>
#include <linux/miscdevice.h>
#endif
#endif
#endif

#ifdef CONFIG_USBIF_COMPLIANCE
#include <linux/proc_fs.h>
#endif

#define RET_SUCCESS 0
#define RET_FAIL 1

struct xhci_hcd *mtk_xhci;
static int vbus_on;

static struct wake_lock mtk_xhci_wakelock;

#ifdef CONFIG_MTK_FPGA
#ifndef CONFIG_USB_MTK_DUALMODE
void __iomem *u3_base;
void __iomem *u3_sif_base;
void __iomem *u3_sif2_base;
void __iomem *i2c1_base;
#endif
#endif

/* struct u3phy_info *u3phy; */
/* struct u3phy_operator *u3phy_ops; */

#ifdef CONFIG_USB_MTK_DUALMODE
#ifdef CONFIG_USB_MTK_OTG_SWITCH
static bool otg_switch_state;
static struct pinctrl *pinctrl;
static struct pinctrl_state *pinctrl_iddig_init;
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
#endif

static const struct of_device_id otg_switch_of_match[] = {
	{.compatible = "mediatek,otg_switch"},
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
static struct switch_dev mtk_otg_state;

static struct delayed_work mtk_xhci_delaywork;
u32 xhci_debug_level = K_ALET | K_CRIT | K_ERR | K_WARNIN;

int mtk_iddig_debounce = 50;
module_param(mtk_iddig_debounce, int, 0644);
module_param(xhci_debug_level, int, 0644);

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

static bool mtk_is_charger_4_vol(void)
{
#if defined(CONFIG_USBIF_COMPLIANCE) || defined(CONFIG_POWER_EXT)
	return false;
#else
	int vol = battery_meter_get_charger_voltage();

	mtk_xhci_mtk_printk(K_DEBUG, "voltage(%d)\n", vol);

	return (vol > 4000) ? true : false;
#endif
}

bool mtk_is_usb_id_pin_short_gnd(void)
{
	return (mtk_idpin_cur_stat != IDPIN_OUT) ? true : false;
}

#ifdef CONFIG_MTK_OTG_PMIC_BOOST_5V
#define PMIC_REG_BAK_NUM (10)

#ifdef CONFIG_MTK_OTG_OC_DETECTOR
#define OC_DETECTOR_TIMER (2000)
static struct delayed_work mtk_xhci_oc_delaywork;

static struct miscdevice xhci_misc_uevent = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "xhci_misc_uevent",
	.fops = NULL,
};
#endif




U32 pmic_bak_regs[PMIC_REG_BAK_NUM][2] = {
	{0x8D22, 0}, {0x8D14, 0}, {0x803C, 0}, {0x8036, 0}, {0x8D24, 0},
	{0x8D16, 0}, {0x803A, 0}, {0x8046, 0}, {0x803E, 0}, {0x8044, 0}
};

static void pmic_save_regs(void)
{
	int i;

	for (i = 0; i < PMIC_REG_BAK_NUM; i++)
		pmic_read_interface(pmic_bak_regs[i][0], &pmic_bak_regs[i][1], 0xffffffff, 0);

}

static void pmic_restore_regs(void)
{
	int i;

	for (i = 0; i < PMIC_REG_BAK_NUM; i++)
		pmic_config_interface(pmic_bak_regs[i][0], pmic_bak_regs[i][1], 0xffffffff, 0);

}

void mtk_enable_pmic_otg_mode(void)
{
	int val = 0;
	int cnt = 0;

	vbus_on++;
	/* / vbus_on =1; */
	mtk_xhci_mtk_printk(K_DEBUG, "set pmic power on, %d\n", vbus_on);
#if 1
	if (vbus_on > 1)
		return;

#endif


	/* save PMIC related registers */
	pmic_save_regs();

	pmic_config_interface(0x8D22, 0x1, 0x1, 12);
	pmic_config_interface(0x8D14, 0x1, 0x1, 12);
	pmic_config_interface(0x803C, 0x3, 0x3, 0);
	pmic_config_interface(0x803C, 0x2, 0x3, 2);
	pmic_config_interface(0x803C, 0x1, 0x1, 14);
	pmic_config_interface(0x8036, 0x0, 0x0, 0);
	pmic_config_interface(0x8D24, 0xf, 0xf, 12);
	pmic_config_interface(0x8D16, 0x1, 0x1, 15);
	pmic_config_interface(0x803A, 0x1, 0x1, 6);
	pmic_config_interface(0x8046, 0x00A0, 0xffff, 0);
	pmic_config_interface(0x803E, 0x1, 0x1, 2);
	pmic_config_interface(0x803E, 0x1, 0x1, 3);
	pmic_config_interface(0x803E, 0x3, 0x3, 8);

#ifdef CONFIG_MTK_OTG_OC_DETECTOR
	/* Current limit is on */
	pmic_config_interface(0x803E, 0x1, 0x1, 10);
#else
	pmic_config_interface(0x803E, 0x0, 0x1, 10);
#endif

	pmic_config_interface(0x8044, 0x3, 0x3, 0);
	pmic_config_interface(0x8044, 0x3, 0x7, 8);
	pmic_config_interface(0x8044, 0x1, 0x1, 11);

	pmic_config_interface(0x809C, 0x8000, 0xFFFF, 0);

	val = 0;
	while (val == 0)
		pmic_read_interface(0x809A, &val, 0x1, 15);


	pmic_config_interface(0x8084, 0x1, 0x1, 0);
	mdelay(50);

	val = 0;
	while (val == 0 && cnt < 20) {
		pmic_read_interface(0x8060, &val, 0x1, 14);
		cnt++;
		mdelay(2);
	}

#ifdef CONFIG_MTK_OTG_OC_DETECTOR
	schedule_delayed_work_on(0, &mtk_xhci_oc_delaywork, msecs_to_jiffies(OC_DETECTOR_TIMER));
#endif
	mtk_xhci_mtk_printk(K_DEBUG, "set pmic power on(cnt:%d), done\n", cnt);

}

void mtk_disable_pmic_otg_mode(void)
{
	int val = 0;
	int cnt = 0;

	/* /vbus_on = 0; */

	vbus_on--;
	mtk_xhci_mtk_printk(K_DEBUG, "set pmic power off %d\n", vbus_on);

	if (vbus_on < 0 || vbus_on > 0) {
		if (vbus_on < 0)
			vbus_on = 0;
		return;
	}


	pmic_config_interface(0x8068, 0x0, 0x1, 0);
	pmic_config_interface(0x8084, 0x0, 0x1, 0);
	mdelay(50);
	pmic_config_interface(0x8068, 0x0, 0x1, 1);

	val = 1;
	while (val == 1 && cnt < 20) {
		pmic_read_interface(0x805E, &val, 0x1, 4);
		cnt++;
		mdelay(2);
	}

#if 0
	pmic_config_interface(0x809E, 0x8000, 0xFFFF, 0);

	val = 1;
	while (val == 1)
		pmic_read_interface(0x809A, &val, 0x1, 15);

#endif

	/* restore PMIC registers */
	pmic_restore_regs();
#ifdef CONFIG_MTK_OTG_OC_DETECTOR
	cancel_delayed_work(&mtk_xhci_oc_delaywork);
#endif
	mtk_xhci_mtk_printk(K_DEBUG, "set pmic power off(cnt:%d), done\n", cnt);
}

#ifdef CONFIG_MTK_OTG_OC_DETECTOR
void xhci_send_event(char *event)
{
	char udev_event[128];
	char *envp[2] = { udev_event, NULL };
	int ret;

	snprintf(udev_event, 128, "XHCI_MISC_UEVENT=%s", event);
	mtk_xhci_mtk_printk(K_DEBUG, "send %s in %s\n", udev_event,
			 kobject_get_path(&xhci_misc_uevent.this_device->kobj, GFP_KERNEL));
	ret = kobject_uevent_env(&xhci_misc_uevent.this_device->kobj, KOBJ_CHANGE, envp);
	if (ret < 0)
		mtk_xhci_mtk_printk(K_DEBUG, "fail, ret(%d)\n", ret);
}

static bool mtk_is_over_current(void)
{
	int vol = battery_meter_get_charger_voltage();

	if (vol < 4200) {
		mtk_xhci_mtk_printk(K_DEBUG, "over current occur, vol(%d)\n", vol);
		return true;
	}

	return false;
}

static void mtk_xhci_oc_detector(struct work_struct *work)
{
	int ret;

	if (mtk_is_over_current()) {
		xhci_send_event("OVER_CURRENT");
		mtk_disable_pmic_otg_mode();
	} else {
		ret =
			schedule_delayed_work_on(0, &mtk_xhci_oc_delaywork,
						 msecs_to_jiffies(OC_DETECTOR_TIMER));
	}
}
#endif

#endif

#if 0
void mtk_hub_event_steal(spinlock_t *lock, struct list_head *list)
{
	mtk_hub_event_lock = lock;
	mtk_hub_event_list = list;
}

void mtk_ep_count_inc(void)
{
	mtk_ep_count++;
}

void mtk_ep_count_dec(void)
{
	mtk_ep_count--;
}
#endif
#if 0
int mtk_is_hub_active(void)
{
	struct usb_hcd *hcd = xhci_to_hcd(mtk_xhci);
	struct usb_device *rhdev = hcd->self.root_hub;
	struct usb_hub *hub = usb_hub_to_struct_hub(rhdev);

	bool ret = true;

	spin_lock_irq(mtk_hub_event_lock);
	if ((mtk_ep_count == 0) && (list_empty(&hub->event_list) == 1)
		&& (atomic_read(&(hub->kref.refcount)) == 1)) {
		ret = false;
	}
	spin_unlock_irq(mtk_hub_event_lock);

	return ret;
}
#endif
static void mtk_enable_otg_mode(void)
{
#if defined(CONFIG_MTK_BQ25898_DUAL_SUPPORT)
	bq25898_otg_en(0x01);
	bq25898_set_boost_ilim(0x01);
#else
	set_chr_enable_otg(0x1);
	set_chr_boost_current_limit(1500);
#endif
}

static void mtk_disable_otg_mode(void)
{
#if defined(CONFIG_MTK_BQ25898_DUAL_SUPPORT)
	bq25898_otg_en(0x0);
#else
	set_chr_enable_otg(0x0);
#endif
}

static int mtk_xhci_hcd_init(void)
{
	int retval;

	retval = xhci_register_plat();
	if (retval < 0) {
		pr_err("Problem registering platform driver.\n");
		return retval;
	}
	retval = xhci_attrs_init();
	if (retval < 0) {
		pr_err("Problem creating xhci attributes.\n");
		goto unreg_plat;
	}
#ifdef CONFIG_MTK_OTG_PMIC_BOOST_5V
#ifdef CONFIG_MTK_OTG_OC_DETECTOR
	retval = misc_register(&xhci_misc_uevent);
	if (retval) {
		pr_err("create the xhci_uevent_device fail, ret(%d)\n", retval);
		goto unreg_attrs;
	}
#endif
#endif

	/*
	 * Check the compiler generated sizes of structures that must be laid
	 * out in specific ways for hardware access.
	 */
	BUILD_BUG_ON(sizeof(struct xhci_doorbell_array) != 256 * 32 / 8);
	BUILD_BUG_ON(sizeof(struct xhci_slot_ctx) != 8 * 32 / 8);
	BUILD_BUG_ON(sizeof(struct xhci_ep_ctx) != 8 * 32 / 8);
	/* xhci_device_control has eight fields, and also
	 * embeds one xhci_slot_ctx and 31 xhci_ep_ctx
	 */
	BUILD_BUG_ON(sizeof(struct xhci_stream_ctx) != 4 * 32 / 8);
	BUILD_BUG_ON(sizeof(union xhci_trb) != 4 * 32 / 8);
	BUILD_BUG_ON(sizeof(struct xhci_erst_entry) != 4 * 32 / 8);
	BUILD_BUG_ON(sizeof(struct xhci_cap_regs) != 7 * 32 / 8);
	BUILD_BUG_ON(sizeof(struct xhci_intr_reg) != 8 * 32 / 8);
	/* xhci_run_regs has eight fields and embeds 128 xhci_intr_regs */
	BUILD_BUG_ON(sizeof(struct xhci_run_regs) != (8 + 8 * 128) * 32 / 8);
	return 0;
#ifdef CONFIG_MTK_OTG_PMIC_BOOST_5V
#ifdef CONFIG_MTK_OTG_OC_DETECTOR
unreg_attrs:
	xhci_attrs_exit();
#endif
#endif
unreg_plat:
	xhci_unregister_plat();
	return retval;
}

static void mtk_xhci_hcd_cleanup(void)
{
#ifdef CONFIG_MTK_OTG_PMIC_BOOST_5V
#ifdef CONFIG_MTK_OTG_OC_DETECTOR
	misc_deregister(&xhci_misc_uevent);
#endif
#endif

	xhci_attrs_exit();
	xhci_unregister_plat();
}

static void mtk_xhci_imod_set(u32 imod)
{
	u32 temp;

	temp = readl(&mtk_xhci->ir_set->irq_control);
	temp &= ~0xFFFF;
	temp |= imod;
	writel(temp, &mtk_xhci->ir_set->irq_control);
}


static int mtk_xhci_driver_load(void)
{
	int ret = 0;

	/* recover clock/power setting and deassert reset bit of mac */
#ifdef CONFIG_PROJECT_PHY
	usb_phy_recover(0);
	usb20_pll_settings(true, true);
#endif
	ret = mtk_xhci_hcd_init();
	if (ret || !mtk_xhci) {
		ret = -ENXIO;
		goto _err;
	}

	/* for performance, fixed the interrupt moderation from 0xA0(default) to 0x30 */
	mtk_xhci_imod_set(0x30);

	mtk_enable_otg_mode();
	enableXhciAllPortPower(mtk_xhci);

	return 0;

_err:
	mtk_xhci_mtk_printk(K_ERR, "ret(%d), mtk_xhci(0x%p)\n", ret, mtk_xhci);
#ifdef CONFIG_PROJECT_PHY
	usb_phy_savecurrent(1);
#endif
	return ret;
}

static void mtk_xhci_disPortPower(void)
{
	mtk_disable_otg_mode();
	disableXhciAllPortPower(mtk_xhci);
}

static void mtk_xhci_driver_unload(void)
{
	mtk_xhci_hcd_cleanup();
	/* close clock/power setting and assert reset bit of mac */
#ifdef CONFIG_PROJECT_PHY
	usb_phy_savecurrent(1);
#endif

}

void mtk_xhci_switch_init(void)
{
	mtk_otg_state.name = "otg_state";
	mtk_otg_state.index = 0;
	mtk_otg_state.state = 0;

#ifndef CONFIG_USBIF_COMPLIANCE
	if (switch_dev_register(&mtk_otg_state))
		mtk_xhci_mtk_printk(K_DEBUG, "switch_dev_register fail\n");
	else
		mtk_xhci_mtk_printk(K_DEBUG, "switch_dev register success\n");
#endif
}

void mtk_xhci_mode_switch(struct work_struct *work)
{
	static bool is_load;
	static bool is_pwoff;
	int ret = 0;

	mtk_xhci_mtk_printk(K_DEBUG, "mtk_xhci_mode_switch\n");

	if (musb_check_ipo_state() == true) {
		enable_irq(mtk_idpin_irqnum); /* prevent from disable irq twice*/
		return;
	}

	if (mtk_idpin_cur_stat == IDPIN_OUT) {
		is_load = false;

		/* expect next isr is for id-pin out action */
		mtk_idpin_cur_stat = (mtk_is_charger_4_vol()) ? IDPIN_IN_DEVICE : IDPIN_IN_HOST;
		/* make id pin to detect the plug-out */
		mtk_set_iddig_out_detect();

		if (mtk_idpin_cur_stat == IDPIN_IN_DEVICE)
			goto done;
		ret = mtk_xhci_driver_load();
		if (!ret) {
			is_load = true;
			mtk_xhci_wakelock_lock();
#ifndef CONFIG_USBIF_COMPLIANCE
			switch_set_state(&mtk_otg_state, 1);
#endif
		}

	} else {		/* IDPIN_OUT */
		if (is_load) {
			if (!is_pwoff)
				mtk_xhci_disPortPower();

			/* prevent hang here */
			/* if(mtk_is_hub_active()){
			   is_pwoff = true;
			   schedule_delayed_work_on(0, &mtk_xhci_delaywork, msecs_to_jiffies(mtk_iddig_debounce));
			   mtk_xhci_mtk_printk(K_DEBUG, "wait, hub is still active, ep cnt %d !!!\n", mtk_ep_count);
			   return;
			   } */
			/* USB PLL Force settings */
#ifdef CONFIG_PROJECT_PHY
			usb20_pll_settings(true, false);
#endif
			mtk_xhci_driver_unload();
			is_pwoff = false;
			is_load = false;
#ifndef CONFIG_USBIF_COMPLIANCE
			switch_set_state(&mtk_otg_state, 0);
#endif
			mtk_xhci_wakelock_unlock();
		}

		/* expect next isr is for id-pin in action */
		mtk_idpin_cur_stat = IDPIN_OUT;
		/* make id pin to detect the plug-in */
		mtk_set_iddig_in_detect();
	}

done:
	mtk_xhci_mtk_printk(K_ALET, "current mode is %s, ret(%d), switch(%d)\n",
			 (mtk_idpin_cur_stat == IDPIN_IN_HOST) ? "host" :
			 (mtk_idpin_cur_stat == IDPIN_IN_DEVICE) ? "id_device" : "device",
			 ret, mtk_otg_state.state);
}

static irqreturn_t xhci_eint_iddig_isr(int irqnum, void *data)
{
	disable_irq_nosync(irqnum);
	schedule_delayed_work(&mtk_xhci_delaywork, msecs_to_jiffies(mtk_iddig_debounce));
	/* microseconds */
	/*
	ret =
		schedule_delayed_work_on(0, &mtk_xhci_delaywork, msecs_to_jiffies(mtk_iddig_debounce));
	*/
	mtk_xhci_mtk_printk(K_DEBUG, "xhci_eint_iddig_isr\n");
	/* disable_irq_nosync(irqnum);*/

	return IRQ_HANDLED;
}

static int mtk_xhci_eint_iddig_irq_en(void)
{
	int retval = 0;

	if (!otg_iddig_isr_enable) {
		retval =
			request_irq(mtk_idpin_irqnum, xhci_eint_iddig_isr, IRQF_TRIGGER_LOW, "iddig_eint",
			NULL);

		if (retval != 0) {
			mtk_xhci_mtk_printk(K_ERR, "request_irq fail, ret %d, irqnum %d!!!\n", retval,
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


int mtk_xhci_eint_iddig_init(void)
{
	int retval = 0;
	struct device_node *node;
	int iddig_gpio, iddig_debounce;
	u32 ints[2] = {0, 0};


	node = of_find_compatible_node(NULL, NULL, "mediatek,usb3_xhci");
	if (node) {
		node = of_find_compatible_node(NULL, NULL, "mediatek,usb_iddig_bi_eint");
		if (node) {
			retval = of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
			if (!retval) {
				iddig_gpio = ints[0];
				iddig_debounce = ints[1];
				mtk_idpin_irqnum = irq_of_parse_and_map(node, 0);
			   mtk_xhci_mtk_printk(K_DEBUG, "iddig gpio num = %d\n", mtk_idpin_irqnum);
			}
		}
	} else {
		mtk_xhci_mtk_printk(K_ERR, "cannot get the node\n");
		return -ENODEV;
	}

	INIT_DELAYED_WORK(&mtk_xhci_delaywork, mtk_xhci_mode_switch);
#ifdef CONFIG_MTK_OTG_PMIC_BOOST_5V
#ifdef CONFIG_MTK_OTG_OC_DETECTOR
	INIT_DELAYED_WORK(&mtk_xhci_oc_delaywork, mtk_xhci_oc_detector);
#endif
#endif

	/* microseconds */
/*	mt_gpio_set_debounce(iddig_gpio, iddig_debounce);*/
#ifndef CONFIG_USB_MTK_OTG_SWITCH
	retval = mtk_xhci_eint_iddig_irq_en();
#endif

	return retval;
}

void mtk_xhci_eint_iddig_deinit(void)
{
	if (otg_iddig_isr_enable) {
		/* mt_eint_registration(IDDIG_EINT_PIN, EINTF_TRIGGER_LOW, NULL, false); */
		disable_irq_nosync(mtk_idpin_irqnum);

		free_irq(mtk_idpin_irqnum, NULL);
		otg_iddig_isr_enable = false;
	}

	cancel_delayed_work(&mtk_xhci_delaywork);
#ifdef CONFIG_MTK_OTG_PMIC_BOOST_5V
#ifdef CONFIG_MTK_OTG_OC_DETECTOR
	cancel_delayed_work(&mtk_xhci_oc_delaywork);
#endif
#endif

	mtk_idpin_cur_stat = IDPIN_OUT;

	mtk_xhci_mtk_printk(K_DEBUG, "external iddig unregister done.\n");
}

void mtk_set_host_mode_in_host(void)
{
	mtk_idpin_cur_stat = IDPIN_IN_HOST;
}

void mtk_set_host_mode_out(void)
{
	mtk_idpin_cur_stat = IDPIN_OUT;
}

bool mtk_is_host_mode(void)
{
	return (vbus_on > 0 || mtk_idpin_cur_stat == IDPIN_IN_HOST) ? true : false;
}

void mtk_unload_xhci_on_ipo(void)
{
	mtk_xhci_disPortPower();
	/* USB PLL Force settings */
	usb20_pll_settings(true, false);
	mtk_xhci_driver_unload();
	switch_set_state(&mtk_otg_state, 0);
	mtk_xhci_wakelock_unlock();
	mtk_idpin_cur_stat = IDPIN_OUT;
}

#ifdef CONFIG_USB_MTK_OTG_SWITCH
static ssize_t otg_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!dev) {
		dev_err(dev, "otg_mode_store no dev\n");
		return 0;
	}

	return sprintf(buf, "%d\n", otg_switch_state);
}

static ssize_t otg_mode_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	unsigned int mode;

	if (!dev) {
		dev_err(dev, "otg_mode_store no dev\n");
		return count;
	}

	mutex_lock(&otg_switch_mutex);
	if (1 == sscanf(buf, "%ud", &mode)) {
		if (mode == 0) {
			mtk_xhci_mtk_printk(K_DEBUG, "otg_mode_enable start\n");
			if (otg_switch_state == true) {
				if (otg_iddig_isr_enable) {
					disable_irq(mtk_idpin_irqnum);
					irq_set_irq_type(mtk_idpin_irqnum, IRQF_TRIGGER_LOW);
				}
				cancel_delayed_work_sync(&mtk_xhci_delaywork);
				if (mtk_is_host_mode() == true)
					mtk_unload_xhci_on_ipo();

				if (!IS_ERR(pinctrl_iddig_disable))
					pinctrl_select_state(pinctrl, pinctrl_iddig_disable);

				otg_switch_state = false;
			}
			mtk_xhci_mtk_printk(K_DEBUG, "otg_mode_enable end\n");
		} else {
			mtk_xhci_mtk_printk(K_DEBUG, "otg_mode_disable start\n");
			if (otg_switch_state == false) {
				otg_switch_state = true;
				if (!IS_ERR(pinctrl_iddig_enable))
					pinctrl_select_state(pinctrl, pinctrl_iddig_enable);

				msleep(20);
				mtk_xhci_eint_iddig_irq_en();
			}
			mtk_xhci_mtk_printk(K_DEBUG, "otg_mode_disable end\n");
		}
	}
	mutex_unlock(&otg_switch_mutex);
	return count;
}

static int otg_switch_probe(struct platform_device *pdev)
{
	int retval = 0;

	pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pinctrl)) {
		dev_err(&pdev->dev, "Cannot find usb pinctrl!\n");
		return -1;
	}

	pinctrl_iddig_init = pinctrl_lookup_state(pinctrl, "iddig_init");
	if (IS_ERR(pinctrl_iddig_init))
		dev_err(&pdev->dev, "Cannot find usb pinctrl iddig_init\n");
	else
		pinctrl_select_state(pinctrl, pinctrl_iddig_init);

	pinctrl_iddig_enable = pinctrl_lookup_state(pinctrl, "iddig_enable");
	pinctrl_iddig_disable = pinctrl_lookup_state(pinctrl, "iddig_disable");
	if (IS_ERR(pinctrl_iddig_enable))
		dev_err(&pdev->dev, "Cannot find usb pinctrl iddig_enable\n");

	if (IS_ERR(pinctrl_iddig_disable))
		dev_err(&pdev->dev, "Cannot find usb pinctrl iddig_disable\n");
	else
		pinctrl_select_state(pinctrl, pinctrl_iddig_disable);

#ifdef CONFIG_SYSFS
	retval = sysfs_create_group(&pdev->dev.kobj, &otg_attr_group);
	if (retval < 0) {
		dev_err(&pdev->dev, "Cannot register USB bus sysfs attributes: %d\n",
			retval);
		return -1;
	}
#endif
	mutex_init(&otg_switch_mutex);
	return 0;
}

static int otg_switch_remove(struct platform_device *pdev)
{
#ifdef CONFIG_SYSFS
		sysfs_remove_group(&pdev->dev.kobj, &otg_attr_group);
#endif
	return 0;
}


static struct platform_driver otg_switch_driver = {
	.probe = otg_switch_probe,
	.remove = otg_switch_remove,
	.driver = {
		.name = "otg_switch",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(otg_switch_of_match),
	},
};
#endif

#endif

#ifdef CONFIG_USB_C_SWITCH
static int typec_otg_enable(void *data)
{
	int ret = 0;

	mtk_idpin_cur_stat = IDPIN_IN_HOST;

	ret = mtk_xhci_driver_load();
	if (!ret) {
		mtk_xhci_wakelock_lock();
		switch_set_state(&mtk_otg_state, 1);
	}
	return ret;
}

static int typec_otg_disable(void *data)
{
	mtk_xhci_disPortPower();
	/* USB PLL Force settings */
	usb20_pll_settings(true, false);
	mtk_xhci_driver_unload();
	switch_set_state(&mtk_otg_state, 0);
	mtk_xhci_wakelock_unlock();
	mtk_idpin_cur_stat = IDPIN_OUT;

	return 0;
}

static struct typec_switch_data typec_host_driver = {
	.name = "xhci-mtk",
	.type = HOST_TYPE,
	.enable = typec_otg_enable,
	.disable = typec_otg_disable,
};
#endif


void mtk_xhci_wakelock_init(void)
{
	wake_lock_init(&mtk_xhci_wakelock, WAKE_LOCK_SUSPEND, "xhci.wakelock");
#ifdef CONFIG_USB_C_SWITCH
	typec_host_driver.priv_data = NULL;
	register_typec_switch_callback(&typec_host_driver);
#endif
}

void mtk_xhci_wakelock_lock(void)
{
	if (!wake_lock_active(&mtk_xhci_wakelock))
		wake_lock(&mtk_xhci_wakelock);
	mtk_xhci_mtk_printk(K_DEBUG, "xhci_wakelock_lock done\n");
}

void mtk_xhci_wakelock_unlock(void)
{
	if (wake_lock_active(&mtk_xhci_wakelock))
		wake_unlock(&mtk_xhci_wakelock);
	mtk_xhci_mtk_printk(K_DEBUG, "xhci_wakelock_unlock done\n");
}

int mtk_xhci_set(struct usb_hcd *hcd, struct xhci_hcd *xhci)
{
	struct platform_device *pdev = to_platform_device(hcd->self.controller);
	struct resource *sif_res;

	xhci->base_regs = (unsigned long)hcd->regs;

	sif_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, XHCI_SIF_REGS_ADDR_RES_NAME);
	if (!sif_res) {
		pr_err("%s(%d): cannot get sif resources\n", __func__, __LINE__);
		return -ENXIO;
	}

	xhci->sif_regs = (unsigned long)ioremap(sif_res->start,
						resource_size(sif_res));
	if (!xhci->sif_regs) {
		pr_err("xhci->sif_regs map fail\n");
		return -ENOMEM;
	}
	mtk_xhci_mtk_printk(K_DEBUG, "%s(%d): sif_base, logic 0x%p, phys 0x%p\n",
			__func__, __LINE__,
			(void *)(unsigned long)sif_res->start,
			(void *)xhci->sif_regs);

	mtk_xhci_mtk_printk(K_DEBUG, "mtk_xhci = 0x%p\n", xhci);
	mtk_xhci = xhci;
	return 0;
}

void mtk_xhci_reset(struct xhci_hcd *xhci)
{
	if (xhci) {
		if (xhci->sif_regs) {
			iounmap((void __iomem *)xhci->sif_regs);
			mtk_xhci_mtk_printk(K_DEBUG, "iounmap, sif_reg, 0x%p\n", (void *)xhci->sif_regs);
		}
	}
	mtk_xhci = NULL;
}

void mtk_xhci_ck_timer_init(struct xhci_hcd *xhci)
{
#if 0
	void __iomem *addr;
	u32 temp = 0;
	int num_u3_port;

	unsigned int hw_code = mt_get_chip_hw_code();
	CHIP_SW_VER sw_code = mt_get_chip_sw_ver();

	mtk_xhci_mtk_printk(K_DEBUG, "hw code(0x%x), sw_code(0x%x)\n", hw_code, sw_code);

	if (0x6595 == hw_code) {
		/* The sys125_ck = 1/2 sys_ck = 62.5MHz */
		addr = (void __iomem *)_SSUSB_SYS_CK_CTRL(xhci->sif_regs);
		temp = readl(addr);
		temp |= SSUSB_SYS_CK_DIV2_EN;
		writel(temp, addr);
		mtk_xhci_mtk_printk(K_DEBUG, "mu3d sys_clk, addr 0x%p, value 0x%x\n",
				 (void *)_SSUSB_SYS_CK_CTRL(xhci->sif_regs),
				 readl((__u32 __iomem *) _SSUSB_SYS_CK_CTRL(xhci->sif_regs)));

		num_u3_port =
			SSUSB_U3_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP(xhci->sif_regs)));
		if (num_u3_port) {
#if 0
			/* set MAC reference clock speed */
			addr =
				(void __iomem *)(_SSUSB_U3_MAC_BASE(xhci->base_regs) +
						 U3_UX_EXIT_LFPS_TIMING_PAR);
			temp = readl(addr);
			temp &= ~(0xff << U3_RX_UX_EXIT_LFPS_REF_OFFSET);
			temp |= (U3_RX_UX_EXIT_LFPS_REF << U3_RX_UX_EXIT_LFPS_REF_OFFSET);
			writel(temp, addr);
			addr =
				(void __iomem *)(_SSUSB_U3_MAC_BASE(xhci->base_regs) + U3_REF_CK_PAR);
			temp = readl(addr);
			temp &= ~(0xff);
			temp |= U3_REF_CK_VAL;
			writel(temp, addr);
#endif

			/* set U3 MAC SYS_CK */
			addr =
				(void __iomem *)(_SSUSB_U3_SYS_BASE(xhci->base_regs) +
						 U3_TIMING_PULSE_CTRL);
			temp = readl(addr);
			temp &= ~(0xff);
			temp |= MTK_CNT_1US_VALUE;
			writel(temp, addr);
		}

		/* set U2 MAC SYS_CK */
		addr =
			(void __iomem *)(_SSUSB_U2_SYS_BASE(xhci->base_regs) + USB20_TIMING_PARAMETER);
		temp &= ~(0xff);
		temp |= MTK_TIME_VALUE_1US;
		writel(temp, addr);

		mtk_xhci_mtk_printk(K_DEBUG, "mu3d u2 mac sys_clk, addr 0x%p, value 0x%x\n",
				 (void *)(_SSUSB_U2_SYS_BASE(xhci->base_regs) +
					  USB20_TIMING_PARAMETER),
				 readl((void __iomem *)(_SSUSB_U2_SYS_BASE(xhci->base_regs) +
							(unsigned long)USB20_TIMING_PARAMETER)));

#if 0
		if (num_u3_port) {
			/* set LINK_PM_TIMER=3 */
			addr =
				(void __iomem *)(_SSUSB_U3_SYS_BASE(xhci->base_regs) + LINK_PM_TIMER);
			temp = readl(addr);
			temp &= ~(0xf);
			temp |= MTK_PM_LC_TIMEOUT_VALUE;
			writel(temp, addr);
		}
#endif
	}
#endif
}

static void setLatchSel(struct xhci_hcd *xhci)
{
	void __iomem *latch_sel_addr;
	u32 latch_sel_value;
	int num_u3_port;

	num_u3_port = SSUSB_U3_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP(xhci->sif_regs)));
	if (num_u3_port <= 0)
		return;

	latch_sel_addr = (void __iomem *)_U3_PIPE_LATCH_SEL_ADD(xhci->base_regs);
	latch_sel_value = ((U3_PIPE_LATCH_TX) << 2) | (U3_PIPE_LATCH_RX);
	writel(latch_sel_value, latch_sel_addr);
}

#ifndef CONFIG_USB_MTK_DUALMODE
static int mtk_xhci_phy_init(int argc, char **argv)
{
	/* initialize PHY related data structure */
	if (!u3phy_ops)
		u3phy_init();

	/* USB 2.0 slew rate calibration */
	if (u3phy_ops->u2_slew_rate_calibration)
		u3phy_ops->u2_slew_rate_calibration(u3phy);
	else
		mtk_xhci_mtk_printk(K_DEBUG, "WARN: PHY doesn't implement u2 slew rate calibration function\n");

	/* phy initialization */
	if (u3phy_ops->init(u3phy) != PHY_TRUE)
		return RET_FAIL;

	mtk_xhci_mtk_printk(K_DEBUG, "phy registers and operations initial done\n");
	return RET_SUCCESS;
}
#endif

int mtk_xhci_ip_init(struct usb_hcd *hcd, struct xhci_hcd *xhci)
{
	int retval;

	retval = mtk_xhci_set(hcd, xhci);
	if (retval)
		goto error;

#ifdef CONFIG_MTK_FPGA
	u3_base = (void __iomem *)xhci->base_regs;
	u3_sif_base = (void __iomem *)xhci->sif_regs;
	u3_sif2_base = (void __iomem *)xhci->sif2_regs;
	i2c1_base = ioremap(0x11008000, 0x1000);
	if (!(i2c1_base))
		pr_err("Can't remap I2C1 BASE\n");

	mtk_xhci_mtk_printk(K_DEBUG, "%s(%d): i2c1_base, logic x%x, phys 0x%p\n", __func__, __LINE__, 0x11008000,
		   (void *)i2c1_base);
#endif


	/* phy initialization is done by device, if target runs on dual mode */
#ifndef CONFIG_USB_MTK_DUALMODE
	mtk_xhci_phy_init(0, NULL);
	enableAllClockPower(xhci, 1);	/* host do reset ip */
#else
	enableAllClockPower(xhci, 1);	/* device do reset ip */
#endif

	setLatchSel(xhci);
	mtk_xhci_ck_timer_init(xhci);

	return 0;

error:
	return retval;
}

#if 0
int mtk_xhci_get_port_num(void)
{
	return SSUSB_U3_PORT_NUM(readl((void __iomem *)SSUSB_IP_CAP))
		+ SSUSB_U2_PORT_NUM(readl((void __iomem *)SSUSB_IP_CAP));
}
#endif

#ifdef CONFIG_USBIF_COMPLIANCE
#ifndef CONFIG_USB_MTK_DUALMODE
static int xhci_hcd_driver_init(void)
{
	int retval;

	retval = xhci_register_pci();
	if (retval < 0) {
		mtk_xhci_mtk_printk(K_DEBUG, "Problem registering PCI driver.");
		return retval;
	}

	#ifdef CONFIG_USB_XHCI_MTK
	mtk_xhci_ip_init();
	#endif

	retval = xhci_register_plat();
	if (retval < 0) {
		mtk_xhci_mtk_printk(K_DEBUG, "Problem registering platform driver.");
		goto unreg_pci;
	}

	#ifdef CONFIG_USB_XHCI_MTK
	retval = xhci_attrs_init();
	if (retval < 0) {
		mtk_xhci_mtk_printk(K_DEBUG, "Problem creating xhci attributes.");
		goto unreg_plat;
	}

	mtk_xhci_wakelock_init();
	#endif

	/*
	 * Check the compiler generated sizes of structures that must be laid
	 * out in specific ways for hardware access.
	 */
	BUILD_BUG_ON(sizeof(struct xhci_doorbell_array) != 256*32/8);
	BUILD_BUG_ON(sizeof(struct xhci_slot_ctx) != 8*32/8);
	BUILD_BUG_ON(sizeof(struct xhci_ep_ctx) != 8*32/8);
	/* xhci_device_control has eight fields, and also
	 * embeds one xhci_slot_ctx and 31 xhci_ep_ctx
	 */
	BUILD_BUG_ON(sizeof(struct xhci_stream_ctx) != 4*32/8);
	BUILD_BUG_ON(sizeof(union xhci_trb) != 4*32/8);
	BUILD_BUG_ON(sizeof(struct xhci_erst_entry) != 4*32/8);
	BUILD_BUG_ON(sizeof(struct xhci_cap_regs) != 7*32/8);
	BUILD_BUG_ON(sizeof(struct xhci_intr_reg) != 8*32/8);
	/* xhci_run_regs has eight fields and embeds 128 xhci_intr_regs */
	BUILD_BUG_ON(sizeof(struct xhci_run_regs) != (8+8*128)*32/8);
	return 0;

#ifdef CONFIG_USB_XHCI_MTK
unreg_plat:
	xhci_unregister_plat();
#endif
unreg_pci:
	xhci_unregister_pci();
	return retval;
}

static void xhci_hcd_driver_cleanup(void)
{
	xhci_unregister_pci();
	xhci_unregister_plat();
	xhci_attrs_exit();
}
#else
static int xhci_hcd_driver_init(void)
{
	/* init in mt_devs.c*/
	mtk_xhci_eint_iddig_init();
	mtk_xhci_switch_init();
	/*mtk_xhci_wakelock_init();*/
	return 0;
}

static void xhci_hcd_driver_cleanup(void)
{
	mtk_xhci_eint_iddig_deinit();
}

#endif

static int mu3h_normal_driver_on;

static int xhci_mu3h_proc_show(struct seq_file *seq, void *v)
{
		seq_printf(seq, "xhci_mu3h_proc_show, mu3h is %d (on:1, off:0)\n", mu3h_normal_driver_on);
		return 0;
}

static int xhci_mu3h_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, xhci_mu3h_proc_show, inode->i_private);
}

static ssize_t xhci_mu3h_proc_write(struct file *file, const char __user *buf, size_t length, loff_t *ppos)
{
	char msg[32];

	if (length >= sizeof(msg)) {
		mtk_xhci_mtk_printk(K_DEBUG, "write length error, the error len is %d\n", (unsigned int)length);
		return -EINVAL;
	}
	if (copy_from_user(msg, buf, length))
		return -EFAULT;

	msg[length] = 0;

	mtk_xhci_mtk_printk(K_DEBUG, "proc_write: %s, current driver on/off: %d\n", msg, mu3h_normal_driver_on);

	if ((msg[0] == '1') && (mu3h_normal_driver_on == 0)) {
		xhci_hcd_driver_init();
		mu3h_normal_driver_on = 1;
		mtk_xhci_mtk_printk(K_DEBUG, "registe mu3h driver : m3h xhci driver\n");
	} else if ((msg[0] == '0') && (mu3h_normal_driver_on == 1)) {
		xhci_hcd_driver_cleanup();
		mu3h_normal_driver_on = 0;
		mtk_xhci_mtk_printk(K_DEBUG, "unregiste m3h xhci driver.\n");
	} else
		mtk_xhci_mtk_printk(K_DEBUG, "xhci_mu3h_proc_write write faile !\n");

	return length;
}

static const struct file_operations mu3h_proc_fops = {
	.owner = THIS_MODULE,
	.open = xhci_mu3h_proc_open,
	.write = xhci_mu3h_proc_write,
	.read = seq_read,
	.llseek = seq_lseek,

};

static int __init xhci_hcd_init(void)
{
	struct proc_dir_entry *prEntry;

	mtk_xhci_mtk_printk(K_DEBUG, "xhci_hcd_init");
	/* set xhci up at boot up*/
	xhci_hcd_driver_init();
	mtk_xhci_wakelock_init();
	mu3h_normal_driver_on = 1;

	/* USBIF */
	prEntry = proc_create("mu3h_driver_init", 0666, NULL, &mu3h_proc_fops);
	if (prEntry)
		mtk_xhci_mtk_printk(K_DEBUG, "create the mu3h init proc OK!\n");
	else
		mtk_xhci_mtk_printk(K_DEBUG, "[ERROR] create the mu3h init proc FAIL\n");

#if 0

	if (!misc_register(&mu3h_uevent_device))
		mtk_xhci_mtk_printk(K_DEBUG, "create the mu3h_uevent_device uevent device OK!\n");
	else
		mtk_xhci_mtk_printk(K_DEBUG, "[ERROR] create the mu3h_uevent_device uevent device fail\n");

#endif

	return 0;

}
late_initcall(xhci_hcd_init);
static void __exit xhci_hcd_cleanup(void)
{
#if 0
	misc_deregister(&mu3h_uevent_device);
#endif
	mtk_xhci_mtk_printk(K_DEBUG, "xhci_hcd_cleanup");
}

module_exit(xhci_hcd_cleanup);

#ifdef CONFIG_USB_MTK_OTG_SWITCH
static int __init otg_switch_init(void)
{
	return platform_driver_register(&otg_switch_driver);
}
module_init(otg_switch_init);

static void __exit otg_iddit_cleanup(void)
{
	platform_driver_unregister(&otg_switch_driver);
}

module_exit(otg_iddit_cleanup);
#endif


#else
#ifndef CONFIG_USB_MTK_DUALMODE
static int __init xhci_hcd_init(void)
{
	int retval;

	retval = xhci_register_pci();
	if (retval < 0) {
		mtk_xhci_mtk_printk(K_DEBUG, "Problem registering PCI driver.");
		return retval;
	}
	retval = xhci_register_plat();
	if (retval < 0) {
		mtk_xhci_mtk_printk(K_DEBUG, "Problem registering platform driver.");
		goto unreg_pci;
	}

	retval = xhci_attrs_init();
	if (retval < 0) {
		mtk_xhci_mtk_printk(K_DEBUG, "Problem creating xhci attributes.");
		goto unreg_plat;
	}

	mtk_xhci_wakelock_init();

	/*
	 * Check the compiler generated sizes of structures that must be laid
	 * out in specific ways for hardware access.
	 */
	BUILD_BUG_ON(sizeof(struct xhci_doorbell_array) != 256*32/8);
	BUILD_BUG_ON(sizeof(struct xhci_slot_ctx) != 8*32/8);
	BUILD_BUG_ON(sizeof(struct xhci_ep_ctx) != 8*32/8);
	/* xhci_device_control has eight fields, and also
	 * embeds one xhci_slot_ctx and 31 xhci_ep_ctx
	 */
	BUILD_BUG_ON(sizeof(struct xhci_stream_ctx) != 4*32/8);
	BUILD_BUG_ON(sizeof(union xhci_trb) != 4*32/8);
	BUILD_BUG_ON(sizeof(struct xhci_erst_entry) != 4*32/8);
	BUILD_BUG_ON(sizeof(struct xhci_cap_regs) != 7*32/8);
	BUILD_BUG_ON(sizeof(struct xhci_intr_reg) != 8*32/8);
	/* xhci_run_regs has eight fields and embeds 128 xhci_intr_regs */
	BUILD_BUG_ON(sizeof(struct xhci_run_regs) != (8+8*128)*32/8);
	return 0;

#ifdef CONFIG_USB_XHCI_MTK
unreg_plat:
	xhci_unregister_plat();
#endif
unreg_pci:
	xhci_unregister_pci();
	return retval;
}
module_init(xhci_hcd_init);

static void __exit xhci_hcd_cleanup(void)
{
	xhci_unregister_pci();
	xhci_unregister_plat();
	xhci_attrs_exit();
}
module_exit(xhci_hcd_cleanup);
#else /*CONFIG_USB_MTK_DUALMODE*/
static int __init xhci_hcd_init(void)
{
	mtk_xhci_eint_iddig_init();
	mtk_xhci_switch_init();
	mtk_xhci_wakelock_init();
	return 0;
}

late_initcall(xhci_hcd_init);

static void __exit xhci_hcd_cleanup(void)
{
}

module_exit(xhci_hcd_cleanup);
#ifdef CONFIG_USB_MTK_OTG_SWITCH
static int __init otg_switch_init(void)
{
	return platform_driver_register(&otg_switch_driver);
}
module_init(otg_switch_init);

static void __exit otg_iddit_cleanup(void)
{
	platform_driver_unregister(&otg_switch_driver);
}

module_exit(otg_iddit_cleanup);
#endif
#endif
#endif

