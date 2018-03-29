/*
 *viatel_cbp_power.c
 *
 *VIA CBP driver for Linux
 *
 *Copyright (C) 2009 VIA TELECOM Corporation, Inc.
 *Author: VIA TELECOM Corporation, Inc.
 *
 *This package is free software; you can redistribute it and/or modify
 *it under the terms of the GNU General Public License version 2 as
 *published by the Free Software Foundation.
 *
 *THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 *IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION,
 *THE IMPLIED *WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <mt-plat/mt_boot.h>
#include <mt-plat/mt_ccci_common.h>
#include "c2k_hw.h"
#include "core.h"
#include "modem_sdio.h"

/*add by yfu to control LDO VGP2*/
/*#include <mach/mt_pm_ldo.h>*/

/*ioctl for vomodem, which must be same as viatelutils.h  */
#define CMDM_IOCTL_RESET	_IO('c', 0x01)
#define CMDM_IOCTL_POWER	_IOW('c', 0x02, int)
#define CMDM_IOCTL_CRL		_IOW('c', 0x03, int)
#define CMDM_IOCTL_DIE		_IO('c', 0x04)
#define CMDM_IOCTL_WAKE		_IO('c', 0x05)
#define CMDM_IOCTL_IGNORE	_IOW('c', 0x06, int)
#define CMDM_IOCTL_GET_MD_STATUS	_IOR('c', 0x07, int)
#define CMDM_IOCTL_ENTER_FLIGHT_MODE	_IO('c', 0x08)
#define CMDM_IOCTL_LEAVE_FLIGHT_MODE	_IO('c', 0x09)
#define CMDM_IOCTL_READY	_IO('c', 0x0A)
#define CMDM_IOCTL_RESET_PCCIF	_IO('c', 0x0B)
#define CMDM_IOCTL_FORCE_ASSERT			_IO('c', 0x0C)
#define CMDM_IOCTL_RESET_FROM_RIL			_IO('c', 0x0D)
/*reserve some bit*/
#define CMDM_IOCTL_GET_SDIO_STATUS      _IO('c', 0x10)
#define CMDM_IOCTL_DUMP_C2K_IRAM      _IO('c', 0x11)
#define CMDM_IOCTL_DUMP_BOOTUP_STATUS      _IO('c', 0x12)

/*event for vmodem, which must be same as viatelutilis.h */
enum ASC_USERSPACE_NOTIFIER_CODE {
	ASC_USER_USB_WAKE = (__SI_POLL | 100),
	ASC_USER_USB_SLEEP,
	ASC_USER_UART_WAKE,
	ASC_USER_UART_SLEEP,
	ASC_USER_SDIO_WAKE,
	ASC_USER_SDIO_SLEEP,
	ASC_USER_MDM_POWER_ON = (__SI_POLL | 200),
	ASC_USER_MDM_POWER_OFF,
	ASC_USER_MDM_RESET_ON,
	ASC_USER_MDM_RESET_OFF,
	ASC_USER_MDM_ERR = (__SI_POLL | 300),
	ASC_USER_MDM_ERR_ENHANCE,
	ASC_USER_MDM_IPOH = (__SI_POLL | 400),
	ASC_USER_MDM_WDT,
	ASC_USER_MDM_EXCEPTION,
};

#define MDM_RST_LOCK_TIME   (120)
#define MDM_EXCP_LOCK_TIME   (120)
#define MDM_RST_HOLD_DELAY  (100)	/*ms */
#define MDM_PWR_HOLD_DELAY  (500)	/*ms */

struct c2k_modem_data {
	struct fasync_struct *fasync;
	struct kobject *modem_kobj;
	struct raw_notifier_head ntf;
	struct notifier_block rst_ntf;
	struct notifier_block pwr_ntf;
	struct notifier_block err_ntf;
#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	struct notifier_block wdt_ntf;
	struct notifier_block excp_ntf;
#endif
	struct wake_lock wlock;
	struct work_struct work;
	atomic_t count;
	unsigned long ntf_flags;
	struct sdio_modem *modem;
};

static struct c2k_modem_data *cmdata;
static unsigned char via_ignore_notifier;
static atomic_t reset_on_going;
static atomic_t modem_not_ready;

int c2k_modem_not_ready(void)
{
	return atomic_read(&modem_not_ready);
}

static void modem_signal_user(int event)
{
	if (cmdata && cmdata->fasync) {
		pr_debug("%s: evnet %d.\n", __func__, (short)event);
		kill_fasync(&cmdata->fasync, SIGIO, event);
	}
}

/*Protection for the above */
static DEFINE_RAW_SPINLOCK(rslock);

void c2k_reset_modem(void)
{
	unsigned long flags;

	if (atomic_add_return(1, &reset_on_going) > 1) {
		pr_debug("[C2K] %s: one reset on going, %d\n", __func__,
			 atomic_read(&reset_on_going));
		return;
	}
	atomic_set(&modem_not_ready, 1);

	wake_lock_timeout(&cmdata->wlock, MDM_RST_LOCK_TIME * HZ);

	pr_debug("[C2K] %s: set md reset.\n", __func__);
	spin_lock_irqsave(&cmdata->modem->status_lock, flags);
	cmdata->modem->status = MD_OFF;
	spin_unlock_irqrestore(&cmdata->modem->status_lock, flags);

	atomic_set(&cmdata->modem->tx_fifo_cnt, TX_FIFO_SZ);
	wake_up_all(&cmdata->modem->wait_tx_done_q);

	modem_pre_stop();

	c2k_wake_host(0);

	c2k_modem_reset_platform();
	if (GPIO_C2K_VALID(GPIO_C2K_MDM_RST)) {
		c2k_gpio_direction_output(GPIO_C2K_CRASH_CBP, 1);
		c2k_gpio_direction_output(GPIO_C2K_MDM_RST, 1);
		mdelay(MDM_RST_HOLD_DELAY);
		c2k_gpio_direction_output(GPIO_C2K_MDM_RST, 0);
		mdelay(MDM_RST_HOLD_DELAY);
	}
	modem_notify_event(MDM_EVT_NOTIFY_RESET_ON);
	pr_debug("[C2K] Warnning: reset vmodem\n");
	atomic_set(&reset_on_going, 0);
}

void c2k_power_on_modem(void)
{
	/*add by yfu to control LDO VGP2 */
	/*turn on VGP2 and set 2.8v */
	/*hwPowerOn(MT6323_POWER_LDO_VGP2, VOL_2800,"VIA"); */
	c2k_modem_power_on_platform();

	if (GPIO_C2K_VALID(GPIO_C2K_MDM_PWR_EN)) {
		if (GPIO_C2K_VALID(GPIO_C2K_MDM_RST)) {
			c2k_gpio_direction_output(GPIO_C2K_CRASH_CBP, 1);
			c2k_gpio_direction_output(GPIO_C2K_MDM_RST, 0);
			mdelay(MDM_RST_HOLD_DELAY);
		}
		c2k_gpio_direction_output(GPIO_C2K_MDM_PWR_EN, 1);
		mdelay(MDM_PWR_HOLD_DELAY);
	}
	pr_debug("[C2K] Warnning: power on vmodem\n");
}

void c2k_power_off_modem(void)
{
	unsigned long flags;

	if (atomic_add_return(1, &reset_on_going) > 1) {
		pr_debug("[C2K] %s: one power off on going, %d\n", __func__,
			 atomic_read(&reset_on_going));
		return;
	}
	atomic_set(&modem_not_ready, 1);

	if (GPIO_C2K_VALID(GPIO_C2K_MDM_PWR_EN))
		c2k_gpio_direction_output(GPIO_C2K_MDM_PWR_EN, 0);

	if (GPIO_C2K_VALID(GPIO_C2K_MDM_RST)) {
		c2k_gpio_direction_output(GPIO_C2K_MDM_RST, 1);
		/*just hold the reset pin if no power enable pin */
		if (GPIO_C2K_VALID(GPIO_C2K_MDM_PWR_EN)) {
			mdelay(MDM_RST_HOLD_DELAY);
			c2k_gpio_direction_output(GPIO_C2K_MDM_RST, 0);
		}
	}
	/*add by yfu to control LDO VGP2 */
	/*turn off VGP2 */
	/*hwPowerDown(MT6323_POWER_LDO_VGP2, "VIA"); */

	pr_debug("[C2K] %s: set md off.\n", __func__);
	spin_lock_irqsave(&cmdata->modem->status_lock, flags);
	cmdata->modem->status = MD_OFF;
	spin_unlock_irqrestore(&cmdata->modem->status_lock, flags);

	atomic_set(&cmdata->modem->tx_fifo_cnt, TX_FIFO_SZ);
	wake_up_all(&cmdata->modem->wait_tx_done_q);

	modem_pre_stop();

	c2k_wake_host(0);
	c2k_modem_power_off_platform();

	pr_debug("[C2K] Warnning: power off vmodem\n");
	atomic_set(&reset_on_going, 0);
}

ssize_t modem_power_show(struct kobject *kobj, struct kobj_attribute *attr,
			 char *buf)
{
	int power = 0;
	int ret = 0;

	if (GPIO_C2K_VALID(GPIO_C2K_MDM_PWR_IND)) {
		power = !!c2k_gpio_get_value(GPIO_C2K_MDM_PWR_IND);
	} else if (GPIO_C2K_VALID(GPIO_C2K_MDM_PWR_EN)) {
		pr_debug("No MDM_PWR_IND, just detect MDM_PWR_EN\n");
		power = !!c2k_gpio_get_value(GPIO_C2K_MDM_PWR_EN);
	} else if (GPIO_C2K_VALID(GPIO_C2K_MDM_RST)) {
		pr_debug("No MDM_PWR_IND, just detect MDM_PWR_RST\n");
		power = !!c2k_gpio_get_value(GPIO_C2K_MDM_RST);
	}
	if (power)
		ret += snprintf(buf + ret, 8, "on\n");
	else
		ret += snprintf(buf + ret, 8, "off\n");

	return ret;
}

ssize_t modem_power_store(struct kobject *kobj, struct kobj_attribute *attr,
			  const char *buf, size_t n)
{
	int power;

	/*power the modem */
	if (!strncmp(buf, "on", strlen("on")))
		power = 1;
	else if (!strncmp(buf, "off", strlen("off"))) {
		power = 0;
	} else {
		pr_debug("%s: input %s is invalid.\n", __func__, buf);
		return n;
	}

	if (power) {
		c2k_power_on_modem();
		modem_notify_event(MDM_EVT_NOTIFY_RESET_ON);
	} else {
		c2k_power_off_modem();
	}
	return n;
}

ssize_t modem_reset_show(struct kobject *kobj, struct kobj_attribute *attr,
			 char *buf)
{
	int reset = 0;
	int ret = 0;

	if (GPIO_C2K_VALID(GPIO_C2K_MDM_RST_IND))
		reset = !!c2k_gpio_get_value(GPIO_C2K_MDM_RST_IND);
	else if (GPIO_C2K_VALID(GPIO_C2K_MDM_RST))
		reset = !!c2k_gpio_get_value(GPIO_C2K_MDM_RST);

	if (reset)
		ret += snprintf(buf + ret, 8, "reset\n");
	else
		ret += snprintf(buf + ret, 8, "work\n");

	return ret;
}

ssize_t modem_reset_store(struct kobject *kobj, struct kobj_attribute *attr,
			  const char *buf, size_t n)
{
	/*reset the modem */
	c2k_reset_modem();
	return n;
}

ssize_t modem_ets_select_show(struct kobject *kobj,
			      struct kobj_attribute *attr, char *buf)
{
	int level = 0;
	int ret = 0;

	if (GPIO_C2K_VALID(GPIO_C2K_MDM_ETS_SEL))
		level = !!c2k_gpio_get_value(GPIO_C2K_MDM_ETS_SEL);

	ret += snprintf(buf, 8, "%d\n", level);
	return ret;
}

ssize_t modem_ets_select_store(struct kobject *kobj,
			       struct kobj_attribute *attr, const char *buf,
			       size_t n)
{
	if (GPIO_C2K_VALID(GPIO_C2K_MDM_ETS_SEL)) {
		if (!strncmp(buf, "1", strlen("1")))
			c2k_gpio_direction_output(GPIO_C2K_MDM_ETS_SEL, 1);
		else if (!strncmp(buf, "0", strlen("0")))
			c2k_gpio_direction_output(GPIO_C2K_MDM_ETS_SEL, 0);
		else
			pr_debug("Unknown command.\n");
	}

	return n;
}

ssize_t modem_boot_select_show(struct kobject *kobj,
			       struct kobj_attribute *attr, char *buf)
{
	int ret = 0;
	int level = 0;

	if (GPIO_C2K_VALID(GPIO_C2K_MDM_BOOT_SEL))
		level = !!c2k_gpio_get_value(GPIO_C2K_MDM_BOOT_SEL);

	ret += snprintf(buf, 8, "%d\n", level);
	return ret;
}

ssize_t modem_boot_select_store(struct kobject *kobj,
				struct kobj_attribute *attr, const char *buf,
				size_t n)
{
	if (GPIO_C2K_VALID(GPIO_C2K_MDM_BOOT_SEL)) {
		if (!strncmp(buf, "1", strlen("1")))
			c2k_gpio_direction_output(GPIO_C2K_MDM_BOOT_SEL, 1);
		else if (!strncmp(buf, "0", strlen("0")))
			c2k_gpio_direction_output(GPIO_C2K_MDM_BOOT_SEL, 0);
		else
			pr_debug("Unknown command.\n");

	}

	return n;
}

int modem_err_indication_usr(int revocery)
{
	pr_debug("%s %d revocery=%d\n", __func__, __LINE__, revocery);
	if (revocery) {
		pr_debug("%s %d MDM_EVT_NOTIFY_HD_ERR\n", __func__, __LINE__);
		modem_notify_event(MDM_EVT_NOTIFY_HD_ERR);
	} else {
		pr_debug("%s %d MDM_EVT_NOTIFY_HD_ENHANCE\n", __func__,
			 __LINE__);
		modem_notify_event(MDM_EVT_NOTIFY_HD_ENHANCE);
	}
	return 0;
}

int modem_ipoh_indication_usr(void)
{
	pr_debug("%s %d MDM_EVT_NOTIFY_IPOH\n", __func__, __LINE__);
	/*c2k_gpio_set_irq_type(GPIO_C2K_MDM_RST_IND, IRQF_TRIGGER_FALLING); */
	modem_notify_event(MDM_EVT_NOTIFY_IPOH);
	return 0;
}

void c2k_let_cbp_die(void)
{
	if (GPIO_C2K_VALID(GPIO_C2K_CRASH_CBP)) {
		c2k_gpio_direction_output(GPIO_C2K_CRASH_CBP, 0);
		mdelay(MDM_RST_HOLD_DELAY);
		c2k_gpio_direction_output(GPIO_C2K_CRASH_CBP, 1);
	}
	pr_debug("let cbp die\n");
}

ssize_t modem_diecbp_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	int ret = 0;
	int level = 0;

	if (GPIO_C2K_VALID(GPIO_C2K_CRASH_CBP))
		level = !!c2k_gpio_get_value(GPIO_C2K_CRASH_CBP);

	ret += snprintf(buf, 8, "%d\n", level);
	return ret;
}

ssize_t modem_diecbp_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{
	if (GPIO_C2K_VALID(GPIO_C2K_CRASH_CBP)) {
		if (!strncmp(buf, "1", strlen("1")))
			c2k_gpio_direction_output(GPIO_C2K_CRASH_CBP, 1);
		else if (!strncmp(buf, "0", strlen("0")))
			c2k_gpio_direction_output(GPIO_C2K_CRASH_CBP, 0);
		else
			pr_debug("Unknown command.\n");

	} else
		pr_debug("invalid gpio.\n");

	return n;
}

ssize_t modem_hderr_show(struct kobject *kobj, struct kobj_attribute *attr,
			 char *buf)
{
	int ret = 0;
	int level = 0;

	if (GPIO_C2K_VALID(GPIO_C2K_CRASH_CBP))
		level = !!c2k_gpio_get_value(GPIO_C2K_CRASH_CBP);

	ret += snprintf(buf, 8, "%d\n", level);
	return ret;
}

ssize_t modem_hderr_store(struct kobject *kobj, struct kobj_attribute *attr,
			  const char *buf, size_t n)
{
	pr_debug("signal modem_err_indication_usr\n");
	if (!strncmp(buf, "1", strlen("1")))
		modem_err_indication_usr(1);
	else if (!strncmp(buf, "0", strlen("0")))
		modem_err_indication_usr(0);
	else
		pr_debug("Unknown command.\n");

	return n;
}

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT

ssize_t modem_force_assert_store(struct kobject *kobj,
				 struct kobj_attribute *attr, const char *buf,
				 size_t n)
{
	if (cmdata->modem)
		force_c2k_assert(cmdata->modem);
	return n;
}

ssize_t modem_force_assert_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	int ret = 0;

	ret += snprintf(buf + ret, 16, "capable\n");

	return ret;
}
#endif

static int modem_reset_notify_misc(struct notifier_block *nb,
				   unsigned long event, void *ptr)
{
	int ret = 0;

	if (via_ignore_notifier) {
		pr_debug
		    ("Warnning: via ignore notifer just return NOTIFY_OK.\n");
		return NOTIFY_OK;
	}
	switch (event) {
	case MDM_EVT_NOTIFY_RESET_ON:
		modem_signal_user(ASC_USER_MDM_RESET_ON);
		break;
	case MDM_EVT_NOTIFY_RESET_OFF:
		modem_signal_user(ASC_USER_MDM_RESET_OFF);
		break;
	default:
		break;
	}

	return ret ? NOTIFY_DONE : NOTIFY_OK;
}

static int modem_power_notify_misc(struct notifier_block *nb,
				   unsigned long event, void *ptr)
{
	switch (event) {
	case MDM_EVT_NOTIFY_POWER_ON:
		modem_signal_user(ASC_USER_MDM_POWER_ON);
		break;
	case MDM_EVT_NOTIFY_POWER_OFF:
		modem_signal_user(ASC_USER_MDM_POWER_OFF);
		break;

	default:
		break;
	}

	return NOTIFY_OK;
}

static int modem_err_notify_misc(struct notifier_block *nb, unsigned long event,
				 void *ptr)
{
	pr_debug("%s %d event=%ld\n", __func__, __LINE__, event);
	switch (event) {
	case MDM_EVT_NOTIFY_HD_ERR:
		pr_debug("%s %d ASC_USER_MDM_ERR\n", __func__, __LINE__);
		if (cmdata->modem->status == MD_READY)
			modem_signal_user(ASC_USER_MDM_ERR);
		else
			pr_debug("%s %d modem is not ready, ignore\n", __func__, __LINE__);
		break;
	case MDM_EVT_NOTIFY_HD_ENHANCE:
		pr_debug("%s %d ASC_USER_MDM_ERR_ENHANCE\n", __func__,
			 __LINE__);
		modem_signal_user(ASC_USER_MDM_ERR_ENHANCE);
		break;
	case MDM_EVT_NOTIFY_IPOH:
		pr_debug("%s %d MDM_EVT_NOTIFY_IPOH\n", __func__, __LINE__);
		modem_signal_user(ASC_USER_MDM_IPOH);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT

static int modem_wdt_notify_misc(struct notifier_block *nb, unsigned long event,
				 void *ptr)
{
	pr_debug("%s %d event=%ld\n", __func__, __LINE__, event);
	switch (event) {
	case MDM_EVT_NOTIFY_WDT:
		pr_debug("%s %d ASC_USER_MDM_WDT\n", __func__, __LINE__);
		modem_signal_user(ASC_USER_MDM_WDT);
#ifdef CONFIG_MTK_SVLTE_SUPPORT
		exec_ccci_kern_func_by_md_id(0, ID_RESET_MD, NULL, 0);
#endif
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

static int modem_excp_notify_misc(struct notifier_block *nb,
				  unsigned long event, void *ptr)
{
	pr_debug("%s %d event=%ld\n", __func__, __LINE__, event);
	switch (event) {
	case MDM_EVT_NOTIFY_EXCP:
		pr_debug("%s %d ASC_USER_MDM_EXCEPTION\n", __func__, __LINE__);
		modem_signal_user(ASC_USER_MDM_EXCEPTION);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

#endif

#define modem_attr(_name) \
static struct kobj_attribute _name##_attr = { \
	.attr = {                        \
		.name = __stringify(_name),   \
		.mode = 0640,                \
	},                               \
	.show   = modem_##_name##_show,  \
	.store  = modem_##_name##_store, \
}

modem_attr(power);
modem_attr(reset);
modem_attr(ets_select);
modem_attr(boot_select);
modem_attr(diecbp);
modem_attr(hderr);
#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
modem_attr(force_assert);
#endif

static struct attribute *g_attr[] = {
	&power_attr.attr,
	&reset_attr.attr,
	&ets_select_attr.attr,
	&boot_select_attr.attr,
	&diecbp_attr.attr,
	&hderr_attr.attr,
#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	&force_assert_attr.attr,
#endif
	NULL
};

static struct attribute_group g_attr_group = {
	.attrs = g_attr,
};

static void modem_shutdown(struct platform_device *dev)
{
	/*c2k_power_off_modem();*/
}

/*
 *Notify about a modem event change.
 *
 */
static void modem_notify_task(struct work_struct *work)
{
	int i = 0;

	for (i = 0; i < MDM_EVT_NOTIFY_NUM; i++) {
		if (test_and_clear_bit(i, &cmdata->ntf_flags))
			raw_notifier_call_chain(&cmdata->ntf, i, NULL);
	}
}

void modem_notify_event(int event)
{
	if (cmdata && event < MDM_EVT_NOTIFY_NUM) {
		set_bit(event, &cmdata->ntf_flags);
		schedule_work(&cmdata->work);
	}
}

/*
 *register a modem events change listener
 */
int modem_register_notifier(struct notifier_block *nb)
{
	int ret = -ENODEV;
	unsigned long flags;

	if (cmdata) {
		raw_spin_lock_irqsave(&rslock, flags);
		ret = raw_notifier_chain_register(&cmdata->ntf, nb);
		raw_spin_unlock_irqrestore(&rslock, flags);
	}
	return ret;
}

/*
 *unregister a modem events change listener
 */
int modem_unregister_notifier(struct notifier_block *nb)
{
	int ret = -ENODEV;
	unsigned long flags;

	if (cmdata) {
		raw_spin_lock_irqsave(&rslock, flags);
		ret = raw_notifier_chain_unregister(&cmdata->ntf, nb);
		raw_spin_unlock_irqrestore(&rslock, flags);
	}
	return ret;
}

static irqreturn_t modem_reset_indication_irq(int irq, void *data)
{
	pr_debug("[C2K MODEM] %s %d\n", __func__, __LINE__);

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	c2k_gpio_to_ls(GPIO_C2K_MDM_RST_IND);
#endif

	if (GPIO_C2K_VALID(GPIO_C2K_MDM_RST_IND)) {
		/*c2k_gpio_set_irq_type(GPIO_C2K_MDM_RST_IND, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING); */
		if (c2k_gpio_get_value(GPIO_C2K_MDM_RST_IND)) {
			pr_debug("[C2K] %s %d ON, md is off now...\n", __func__,
				 __LINE__);
			wake_lock_timeout(&cmdata->wlock,
					  MDM_RST_LOCK_TIME * HZ);
/*#ifdef CONFIG_EVDO_DT_VIA_SUPPORT*/
			modem_notify_event(MDM_EVT_NOTIFY_RESET_ON);
/*#endif*/
		} else {
			pr_debug("%s %d OFF, md is on now...\n", __func__,
				 __LINE__);
/*#ifdef CONFIG_EVDO_DT_VIA_SUPPORT*/
			modem_notify_event(MDM_EVT_NOTIFY_RESET_OFF);
/*#endif*/
		}
	}
	gpio_irq_cbp_rst_ind();
#if defined(CONFIG_MTK_LEGACY)
	c2k_gpio_irq_unmask(GPIO_C2K_MDM_RST_IND);
#endif
	return IRQ_HANDLED;
}

static irqreturn_t modem_power_indication_irq(int irq, void *data)
{
	if (GPIO_C2K_VALID(GPIO_C2K_MDM_PWR_IND)) {
		c2k_gpio_set_irq_type(GPIO_C2K_MDM_PWR_IND,
				      IRQF_TRIGGER_RISING |
				      IRQF_TRIGGER_FALLING);
		if (c2k_gpio_get_value(GPIO_C2K_MDM_PWR_IND))
			modem_notify_event(MDM_EVT_NOTIFY_POWER_ON);
		else
			modem_notify_event(MDM_EVT_NOTIFY_POWER_OFF);

	}

	return IRQ_HANDLED;
}

/*enable if support 4 pin sync in userspace*/
#if 0				/*defined(CONFIG_C2KECOM_SYNC_CBP) */
static int modem_userspace_notifier(int msg, void *data)
{
	int ret = 0;
	int wake, sleep;
	char *hd = (char *)data;

	if (!hd) {
		pr_debug("%s:error sync user\n", __func__);
		return -ENODEV;
	}

	if (!strncmp(hd, USB_RX_HD_NAME, ASC_NAME_LEN)) {
		wake = ASC_USER_USB_WAKE;
		sleep = ASC_USER_USB_SLEEP;
	} else if (!strncmp(hd, UART_RX_HD_NAME, ASC_NAME_LEN)) {
		wake = ASC_USER_UART_WAKE;
		sleep = ASC_USER_UART_SLEEP;
	} else if (!strncmp(hd, SDIO_RX_HD_NAME, ASC_NAME_LEN)) {
		wake = ASC_USER_SDIO_WAKE;
		sleep = ASC_USER_SDIO_SLEEP;
	} else {
		return -ENODEV;
	}

	if (!atomic_read(&cmdata->count))
		return 0;

	switch (msg) {
	case ASC_NTF_RX_PREPARE:
		modem_signal_user(wake);
		break;

	case ASC_NTF_RX_POST:
		modem_signal_user(sleep);
		break;

	default:
		pr_debug("%s Unknown message %d\n", __func__, msg);
	}

	return ret;
}

static int modem_sync_init(void)
{
	int ret = 0;
	struct asc_infor user;
	struct asc_config cfg;

	/*Registe the cbp tx handle */
	if (GPIO_C2K_VALID(GPIO_C2K_AP_WAKE_MDM)
	    && GPIO_c2k_VALID(GPIO_C2K_MDM_RDY)) {
		memset(&cfg, 0, sizeof(struct asc_config));
		strncpy(cfg.name, CBP_TX_HD_NAME, ASC_NAME_LEN);
		cfg.gpio_wake = GPIO_C2K_AP_WAKE_MDM;
		cfg.gpio_ready = GPIO_C2K_MDM_RDY;
		cfg.polar = 1;
		ret = asc_tx_register_handle(&cfg);
		if (ret < 0) {
			pr_debug("%s: fail to regist tx handle %s\n", __func__,
				 CBP_TX_HD_NAME);
			goto end_sync_init;
		}
	}

	/*Registe the usb rx handle */
	if (GPIO_C2K_VALID(GPIO_C2K_USB_MDM_WAKE_AP)
	    && GPIO_C2K_VALID(GPIO_C2K_USB_AP_RDY)) {
		memset(&cfg, 0, sizeof(struct asc_config));
		strncpy(cfg.name, USB_RX_HD_NAME, ASC_NAME_LEN);
		cfg.gpio_wake = GPIO_C2K_USB_MDM_WAKE_AP;
		cfg.gpio_ready = GPIO_C2K_USB_AP_RDY;
		cfg.polar = 1;
		ret = asc_rx_register_handle(&cfg);
		if (ret < 0) {
			pr_debug("%s: fail to regist rx handle %s\n", __func__,
				 USB_RX_HD_NAME);
			goto end_sync_init;
		}
		memset(&user, 0, sizeof(struct asc_infor));
		user.notifier = modem_userspace_notifier,
		    user.data = USB_RX_HD_NAME,
		    snprintf(user.name, ASC_NAME_LEN, USB_RX_USER_NAME);
		ret = asc_rx_add_user(USB_RX_HD_NAME, &user);
		if (ret < 0) {
			pr_debug("%s: fail to regist rx user %s\n", __func__,
				 USB_RX_USER_NAME);
			goto end_sync_init;
		}
	}

	/*Registe the uart rx handle */
	if (GPIO_OEM_VALID(GPIO_C2K_UART_MDM_WAKE_AP)
	    && GPIO_OEM_VALID(GPIO_C2K_UART_AP_RDY)) {
		memset(&cfg, 0, sizeof(struct asc_config));
		strncpy(cfg.name, UART_RX_HD_NAME, ASC_NAME_LEN);
		cfg.gpio_wake = GPIO_C2K_UART_MDM_WAKE_AP;
		cfg.gpio_ready = GPIO_C2K_UART_AP_RDY;
		cfg.polar = 1;
		ret = asc_rx_register_handle(&cfg);
		if (ret < 0) {
			pr_debug("%s: fail to regist rx handle %s\n", __func__,
				 UART_RX_HD_NAME);
			goto end_sync_init;
		}

		memset(&user, 0, sizeof(struct asc_infor));
		user.notifier = modem_userspace_notifier,
		    user.data = UART_RX_HD_NAME,
		    snprintf(user.name, ASC_NAME_LEN, UART_RX_USER_NAME);
		ret = asc_rx_add_user(UART_RX_HD_NAME, &user);
		if (ret < 0) {
			pr_debug("%s: fail to regist rx user %s\n", __func__,
				 UART_RX_USER_NAME);
			goto end_sync_init;
		}
	}

 end_sync_init:
	if (ret)
		pr_debug("%s: error\n", __func__);

	return ret;
}

late_initcall(modem_sync_init);
#endif

static struct platform_driver platform_modem_driver = {
	.driver.name = "c2k_modem",
	.shutdown = modem_shutdown,
};

static struct platform_device platform_modem_device = {
	.name = "c2k_modem",
};

static int misc_modem_open(struct inode *inode, struct file *filp)
{
	int ret = -ENODEV;

	if (cmdata) {
		filp->private_data = cmdata;
		atomic_inc(&cmdata->count);
		ret = 0;
	}

	return ret;
}

static long misc_modem_ioctl(struct file *file, unsigned int
			     cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int flag, ret = -1;

	switch (cmd) {
	case CMDM_IOCTL_RESET:
		pr_debug("[C2K]Reset C2K.\n");
		c2k_reset_modem();
		break;
	case CMDM_IOCTL_READY:
		pr_debug("[C2K SDIO]modem boot up done.\n");
		atomic_set(&modem_not_ready, 0);
		break;
	case CMDM_IOCTL_RESET_PCCIF:
		pr_debug("[C2K SDIO]reset PCCIF\n");
		c2k_modem_reset_pccif();
		break;
	case CMDM_IOCTL_RESET_FROM_RIL:
		pr_debug("[C2K]Reset C2K from RIL.\n");
		c2k_reset_modem();
		if (ccci_get_opt_val("opt_c2k_lte_mode") == 1) /* SVLTE */
			exec_ccci_kern_func_by_md_id(0, ID_RESET_MD, NULL, 0);

		break;
	case CMDM_IOCTL_POWER:
		if (copy_from_user(&flag, argp, sizeof(flag)))
			return -EFAULT;
		pr_debug("[C2K]power C2K for %d.\n", flag);
		switch (flag) {
		case 0:
			c2k_power_off_modem();
			break;
		case 1:
			c2k_power_on_modem();
			break;
		case 2:
			c2k_power_off_modem();
			if (ccci_get_opt_val("opt_c2k_lte_mode") == 1) /* SVLTE */
				exec_ccci_kern_func_by_md_id(0, ID_RESET_MD, NULL, 0);
			break;
		default:
			return -EINVAL;
		}
		break;
	case CMDM_IOCTL_CRL:
		if (copy_from_user(&flag, argp, sizeof(flag)))
			return -EFAULT;
		if (flag < 0 || flag > 1)
			return -EINVAL;
		if (flag)
			ret = modem_on_off_ctrl_chan(1);
		else
			ret = modem_on_off_ctrl_chan(0);

		break;
	case CMDM_IOCTL_DIE:
		c2k_let_cbp_die();
		break;
	case CMDM_IOCTL_WAKE:
		if (copy_from_user(&flag, argp, sizeof(flag)))
			return -EFAULT;
		if (flag < 0 || flag > 1)
			return -EINVAL;
		if (flag) {
			pr_debug("hold on wakelock.\n");
			wake_lock(&cmdata->wlock);
		} else {
			pr_debug("release wakelock.\n");
			wake_unlock(&cmdata->wlock);
		}
		break;
	case CMDM_IOCTL_IGNORE:
		if (copy_from_user(&flag, argp, sizeof(flag)))
			return -EFAULT;
		if (flag < 0 || flag > 1)
			return -EINVAL;
		if (flag) {
			pr_debug("Warnning: via ignore notifer.\n");
			via_ignore_notifier = 1;
		} else {
			pr_debug("Warnning: via receive notifer.\n");
			via_ignore_notifier = 0;
		}
		break;

	case CMDM_IOCTL_GET_MD_STATUS:
		if (cmdata->modem)
			ret =
			    put_user((unsigned int)cmdata->modem->status,
				     (unsigned int __user *)arg);
		else
			return -EFAULT;
		break;

	case CMDM_IOCTL_ENTER_FLIGHT_MODE:
		pr_debug("[C2K SDIO]enter flight mode.\n");
		c2k_power_off_modem();
		if (!GPIO_C2K_VALID(GPIO_C2K_MDM_PWR_IND))
			modem_notify_event(MDM_EVT_NOTIFY_POWER_OFF);
		asc_rx_reset(SDIO_RX_HD_NAME);	/*to let AP release Rx wakelock */
		break;
	case CMDM_IOCTL_LEAVE_FLIGHT_MODE:
		pr_debug("[C2K SDIO]leave flight mode.\n");
		c2k_power_on_modem();
		modem_notify_event(MDM_EVT_NOTIFY_RESET_ON);
		break;
#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	case CMDM_IOCTL_FORCE_ASSERT:
		pr_debug("[C2K SDIO]force C2K assert ioctl.\n");
		if (cmdata->modem)
			force_c2k_assert(cmdata->modem);

		break;
#endif
	case CMDM_IOCTL_GET_SDIO_STATUS:
		pr_debug("[C2K SDIO]get sdio status.\n");
		if (cmdata->modem)
			dump_c2k_sdio_status(cmdata->modem);
		break;
	case CMDM_IOCTL_DUMP_C2K_IRAM:
		pr_debug("[C2K SDIO]dump c2k iram.\n");
		dump_c2k_iram_seg2();
		break;
	case CMDM_IOCTL_DUMP_BOOTUP_STATUS:
		pr_debug("[C2K SDIO]dump c2k bootup status.\n");
		dump_c2k_bootup_status();
		break;
	default:
		break;

	}

	return 0;
}

#ifdef CONFIG_COMPAT
static long misc_modem_compat_ioctl(struct file *filp, unsigned int cmd,
				    unsigned long arg)
{
	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		pr_warn("[SDIO MODEM]!filp->f_op || !filp->f_op->unlocked_ioctl)\n");
		return -ENOTTY;
	}
	pr_debug("[SDIO MODEM] compat ioctl %d\n", cmd);
	switch (cmd) {
	default:
		{
			return filp->f_op->unlocked_ioctl(filp, cmd,
							  (unsigned long)
							  compat_ptr(arg));
		}
	}
}
#endif

static int misc_modem_release(struct inode *inode, struct file *filp)
{
	struct c2k_modem_data *d =
	    (struct c2k_modem_data *)(filp->private_data);

	if (atomic_read(&cmdata->count) > 0)
		atomic_dec(&cmdata->count);

	return fasync_helper(-1, filp, 0, &d->fasync);
}

static int misc_modem_fasync(int fd, struct file *filp, int on)
{
	struct c2k_modem_data *d =
	    (struct c2k_modem_data *)(filp->private_data);

	return fasync_helper(fd, filp, on, &d->fasync);
}

static const struct file_operations misc_modem_fops = {
	.owner = THIS_MODULE,
	.open = misc_modem_open,
	.unlocked_ioctl = misc_modem_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = &misc_modem_compat_ioctl,
#endif
	.release = misc_modem_release,
	.fasync = misc_modem_fasync,
};

static struct miscdevice misc_modem_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "vmodem",
	.fops = &misc_modem_fops,
};

static int modem_data_init(struct c2k_modem_data *d)
{
	int ret = 0;

	d->modem_kobj = c2k_kobject_add("modem");
	if (!d->modem_kobj) {
		ret = -ENOMEM;
		goto end;
	}
	d->ntf_flags = 0;
	RAW_INIT_NOTIFIER_HEAD(&d->ntf);
	wake_lock_init(&d->wlock, WAKE_LOCK_SUSPEND, "cbp_rst");
	INIT_WORK(&d->work, modem_notify_task);
	d->rst_ntf.notifier_call = modem_reset_notify_misc;
#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	d->wdt_ntf.notifier_call = modem_wdt_notify_misc;
	d->excp_ntf.notifier_call = modem_excp_notify_misc;
#endif
	d->pwr_ntf.notifier_call = modem_power_notify_misc;
	d->err_ntf.notifier_call = modem_err_notify_misc;
	atomic_set(&d->count, 0);

	d->modem = c2k_modem;
 end:
	return ret;
}

static int __init modem_init(void)
{
	int ret = 0;

	cmdata = kzalloc(sizeof(struct c2k_modem_data), GFP_KERNEL);
	if (!cmdata) {
		ret = -ENOMEM;
		pr_debug("No memory to alloc cmdata");
		goto err_create_cmdata;
	}

	ret = modem_data_init(cmdata);
	if (ret < 0) {
		pr_debug("Fail to init modem data\n");
		goto err_init_modem_data;
	}
	atomic_set(&modem_not_ready, 1);

	ret = platform_device_register(&platform_modem_device);
	if (ret) {
		pr_debug("platform_device_register failed\n");
		goto err_platform_device_register;
	}
	ret = platform_driver_register(&platform_modem_driver);
	if (ret) {
		pr_debug("platform_driver_register failed\n");
		goto err_platform_driver_register;
	}

	ret = misc_register(&misc_modem_device);
	if (ret < 0) {
		pr_debug("misc regiser via modem failed\n");
		goto err_misc_device_register;
	}

	/*make the default ETS output through USB */
#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	if (GPIO_C2K_VALID(GPIO_C2K_MDM_ETS_SEL) &&
	    (is_meta_mode() || get_boot_mode() == FACTORY_BOOT)) {
		set_ets_sel(1);
	}
#else
	if (GPIO_C2K_VALID(GPIO_C2K_MDM_ETS_SEL))
		c2k_gpio_direction_output(GPIO_C2K_MDM_ETS_SEL, 1);

#endif

	if (GPIO_C2K_VALID(GPIO_C2K_MDM_PWR_IND)) {
#if defined(CONFIG_MTK_LEGACY)
		c2k_gpio_irq_mask(GPIO_C2K_MDM_PWR_IND);
#endif
		c2k_gpio_direction_input_for_irq(GPIO_C2K_MDM_PWR_IND);
		c2k_gpio_set_irq_type(GPIO_C2K_MDM_PWR_IND,
				      IRQF_TRIGGER_RISING |
				      IRQF_TRIGGER_FALLING);
		ret =
		    c2k_gpio_request_irq(GPIO_C2K_MDM_PWR_IND,
					 modem_power_indication_irq,
					 IRQF_SHARED | IRQF_NO_SUSPEND |
					 IRQF_TRIGGER_RISING |
					 IRQF_TRIGGER_FALLING, "mdm_power_ind",
					 cmdata);
#if defined(CONFIG_MTK_LEGACY)
		c2k_gpio_irq_unmask(GPIO_C2K_MDM_PWR_IND);
#endif
		if (ret < 0)
			pr_debug("fail to request mdm_power_ind irq\n");

	}
	modem_register_notifier(&cmdata->pwr_ntf);	/*for SW triggered power state chaning (flight mode) */

	if (GPIO_C2K_VALID(GPIO_C2K_MDM_RST_IND)) {
#if defined(CONFIG_MTK_LEGACY)
		c2k_gpio_irq_mask(GPIO_C2K_MDM_RST_IND);
#endif
		c2k_gpio_direction_input_for_irq(GPIO_C2K_MDM_RST_IND);
		c2k_gpio_set_irq_type(GPIO_C2K_MDM_RST_IND, IRQF_TRIGGER_FALLING);

		ret =
		    c2k_gpio_request_irq(GPIO_C2K_MDM_RST_IND,
					 modem_reset_indication_irq,
					 IRQF_SHARED | IRQF_NO_SUSPEND |
					 IRQF_TRIGGER_FALLING, "mdm_reset_ind",
					 cmdata);
#if defined(CONFIG_MTK_LEGACY)
		c2k_gpio_irq_unmask(GPIO_C2K_MDM_RST_IND);
#endif
		if (ret < 0)
			pr_debug("fail to request mdm_rst_ind irq\n");

		modem_register_notifier(&cmdata->rst_ntf);
	}
#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	modem_register_notifier(&cmdata->wdt_ntf);
	modem_register_notifier(&cmdata->excp_ntf);
#endif

	if (GPIO_C2K_VALID(GPIO_C2K_CRASH_CBP)) {
		pr_debug("%s %d GPIO_C2K_CRASH_CBP", __func__, __LINE__);
		c2k_gpio_direction_output(GPIO_C2K_CRASH_CBP, 1);
	}
	modem_register_notifier(&cmdata->err_ntf);
	/*c2k_gpio_direction_output(GPIO_C2K_MDM_RST, 0); */
	/*c2k_gpio_direction_output(GPIO_C2K_MDM_PWR_EN, 1); */
	ret = sysfs_create_group(cmdata->modem_kobj, &g_attr_group);

	if (ret) {
		pr_debug("sysfs_create_group failed\n");
		goto err_sysfs_create_group;
	}

	return 0;
 err_sysfs_create_group:
	misc_deregister(&misc_modem_device);
 err_misc_device_register:
	platform_driver_unregister(&platform_modem_driver);
 err_platform_driver_register:
	platform_device_unregister(&platform_modem_device);
 err_platform_device_register:
 err_init_modem_data:
	kfree(cmdata);
	cmdata = NULL;
 err_create_cmdata:
	return ret;
}

static void __exit modem_exit(void)
{
	if (GPIO_C2K_VALID(GPIO_C2K_MDM_PWR_IND))
		modem_unregister_notifier(&cmdata->pwr_ntf);

	if (GPIO_C2K_VALID(GPIO_C2K_MDM_RST_IND))
		modem_unregister_notifier(&cmdata->pwr_ntf);

	modem_unregister_notifier(&cmdata->err_ntf);

	if (cmdata)
		wake_lock_destroy(&cmdata->wlock);
}

late_initcall_sync(modem_init);
module_exit(modem_exit);
