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

#ifdef CONFIG_USB_MTK_OTG
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include "musb_core.h"
#include <linux/platform_device.h>
#include "musbhsdma.h"
#include <linux/switch.h>
#include "usb20.h"
#include <linux/of_irq.h>
#include <linux/of_address.h>
#ifdef CONFIG_USB_C_SWITCH
#include <typec.h>
#ifdef CONFIG_TCPC_CLASS
#include "tcpm.h"
#include <linux/workqueue.h>
#include <linux/mutex.h>
static struct notifier_block otg_nb;
static bool usbc_otg_attached;
static struct tcpc_device *otg_tcpc_dev;
static struct workqueue_struct *otg_tcpc_power_workq;
static struct workqueue_struct *otg_tcpc_workq;
static struct work_struct tcpc_otg_power_work;
static struct work_struct tcpc_otg_work;
static bool usbc_otg_power_enable;
static bool usbc_otg_enable;
static struct mutex tcpc_otg_lock;
static struct mutex tcpc_otg_pwr_lock;
static int otg_tcp_notifier_call(struct notifier_block *nb,
		unsigned long event, void *data);
static struct delayed_work register_otg_work;
#define TCPC_OTG_DEV_NAME "type_c_port0"
void do_register_otg_work(struct work_struct *data)
{
#define REGISTER_OTG_WORK_DELAY 1000
	static int ret;

	if (!otg_tcpc_dev)
		otg_tcpc_dev = tcpc_dev_get_by_name(TCPC_OTG_DEV_NAME);

	if (!otg_tcpc_dev) {
		DBG(0, "get type_c_port0 fail\n");
		queue_delayed_work(mtk_musb->st_wq, &register_otg_work,
				msecs_to_jiffies(REGISTER_OTG_WORK_DELAY));
		return;
	}

	otg_nb.notifier_call = otg_tcp_notifier_call;
	ret = register_tcp_dev_notifier(otg_tcpc_dev, &otg_nb);
	if (ret < 0) {
		DBG(0, "register OTG <%p> fail\n", otg_tcpc_dev);
		queue_delayed_work(mtk_musb->st_wq, &register_otg_work,
				msecs_to_jiffies(REGISTER_OTG_WORK_DELAY));
		return;
	}

	DBG(0, "register OTG <%p> ok\n", otg_tcpc_dev);
}
#endif
#endif
#if CONFIG_MTK_GAUGE_VERSION == 30
#include <mt-plat/mtk_battery.h>
#include <mt-plat/charger_class.h>
static struct charger_device *primary_charger;
#endif

#include <mt-plat/mtk_boot_common.h>

struct device_node		*usb_node;
static int iddig_eint_num;
static ktime_t ktime_start, ktime_end;

static struct musb_fifo_cfg fifo_cfg_host[] = {
{ .hw_ep_num =  1, .style = MUSB_FIFO_TX,   .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =  1, .style = MUSB_FIFO_RX,   .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =  2, .style = MUSB_FIFO_TX,   .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =  2, .style = MUSB_FIFO_RX,   .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =  3, .style = MUSB_FIFO_TX,   .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =  3, .style = MUSB_FIFO_RX,   .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =  4, .style = MUSB_FIFO_TX,   .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =  4, .style = MUSB_FIFO_RX,   .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =  5, .style = MUSB_FIFO_TX,   .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =	5, .style = MUSB_FIFO_RX,   .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =  6, .style = MUSB_FIFO_TX,   .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =	6, .style = MUSB_FIFO_RX,   .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =	7, .style = MUSB_FIFO_TX,   .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =	7, .style = MUSB_FIFO_RX,   .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =	8, .style = MUSB_FIFO_TX,   .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =	8, .style = MUSB_FIFO_RX,   .maxpacket = 64,  .mode = MUSB_BUF_SINGLE},
};

int delay_time = 15;
module_param(delay_time, int, 0400);
int delay_time1 = 55;
module_param(delay_time1, int, 0400);
int iddig_cnt;
module_param(iddig_cnt, int, 0400);

void vbus_init(void)
{
	DBG(0, "+++\n");
#if CONFIG_MTK_GAUGE_VERSION == 30
	primary_charger = get_charger_by_name("primary_chg");
	if (!primary_charger) {
		pr_notice("%s: get primary charger device failed\n", __func__);
		return;
	}
#endif
	DBG(0, "---\n");

}

static bool vbus_on;
module_param(vbus_on, bool, 0400);
static int vbus_control;
module_param(vbus_control, int, 0400);
bool usb20_check_vbus_on(void)
{
	DBG(0, "vbus_on<%d>\n", vbus_on);
	return vbus_on;
}
void _set_vbus(struct musb *musb, int is_on)
{
	static int vbus_inited;

	if (!vbus_inited) {
		vbus_init();
		vbus_inited = 1;
	}

	DBG(0, "op<%d>, status<%d>\n", is_on, vbus_on);
	if (is_on && !vbus_on) {
		/* update flag 1st then enable VBUS to make host mode correct used by PMIC */
		vbus_on = true;

#if CONFIG_MTK_GAUGE_VERSION == 30
		charger_dev_enable_otg(primary_charger, true);
		charger_dev_set_boost_current_limit(primary_charger, 1500000);
#else
		set_chr_enable_otg(0x1);
		set_chr_boost_current_limit(1500);
#endif
	} else if (!is_on && vbus_on) {
#if CONFIG_MTK_GAUGE_VERSION == 30
		charger_dev_enable_otg(primary_charger, false);
#else
		set_chr_enable_otg(0x0);
#endif

		/* disable VBUS 1st then update flag to make host mode correct used by PMIC */
		vbus_on = false;
	}
}

void mt_usb_set_vbus(struct musb *musb, int is_on)
{
#ifndef CONFIG_FPGA_EARLY_PORTING

	DBG(0, "is_on<%d>, control<%d>\n", is_on, vbus_control);

	if (!vbus_control)
		return;

	if (is_on)
		_set_vbus(mtk_musb, 1);
	else
		_set_vbus(mtk_musb, 0);
#endif
}

int mt_usb_get_vbus_status(struct musb *musb)
{
#if 1
	return true;
#else
	int	ret = 0;

	if ((musb_readb(musb->mregs, MUSB_DEVCTL) & MUSB_DEVCTL_VBUS) != MUSB_DEVCTL_VBUS)
		ret = 1;
	else
		DBG(0, "VBUS error, devctl=%x, power=%d\n", musb_readb(musb->mregs, MUSB_DEVCTL), musb->power);
	pr_debug("vbus ready = %d\n", ret);
	return ret;
#endif
}

#if defined(CONFIG_USBIF_COMPLIANCE)
int sw_deboun_time = 1;
#else
int sw_deboun_time = 400;
#endif
module_param(sw_deboun_time, int, 0400);
struct switch_dev otg_state;

int typec_control;
module_param(typec_control, int, 0400);
static bool typec_req_host;
static bool iddig_req_host;

void musb_typec_host_connect(int delay)
{
	typec_req_host = true;
	DBG(0, "%s\n", typec_req_host ? "connect" : "disconnect");
	queue_delayed_work(mtk_musb->st_wq, &mtk_musb->host_work, msecs_to_jiffies(delay));
}
void musb_typec_host_disconnect(int delay)
{
	typec_req_host = false;
	DBG(0, "%s\n", typec_req_host ? "connect" : "disconnect");
	queue_delayed_work(mtk_musb->st_wq, &mtk_musb->host_work, msecs_to_jiffies(delay));
}
#ifdef CONFIG_USB_C_SWITCH
#ifdef CONFIG_TCPC_CLASS
int tcpc_otg_enable(void)
{
	if (!usbc_otg_attached) {
		musb_typec_host_connect(0);
		usbc_otg_attached = true;
	}
	return 0;
}

int tcpc_otg_disable(void)
{
	if (usbc_otg_attached) {
		musb_typec_host_disconnect(0);
		usbc_otg_attached = false;
	}
	return 0;
}

static void tcpc_otg_work_call(struct work_struct *work)
{
	bool enable;

	mutex_lock(&tcpc_otg_lock);
	enable = usbc_otg_enable;
	mutex_unlock(&tcpc_otg_lock);

	if (enable)
		tcpc_otg_enable();
	else
		tcpc_otg_disable();
}

static void tcpc_otg_power_work_call(struct work_struct *work)
{
	mutex_lock(&tcpc_otg_pwr_lock);
	if (usbc_otg_power_enable)
		_set_vbus(mtk_musb, 1);
	else
		_set_vbus(mtk_musb, 0);
	mutex_unlock(&tcpc_otg_pwr_lock);
}
static int otg_tcp_notifier_call(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct tcp_notify *noti = data;

	switch (event) {
	case TCP_NOTIFY_SOURCE_VBUS:
		pr_info("%s source vbus = %dmv\n",
				__func__, noti->vbus_state.mv);
		mutex_lock(&tcpc_otg_pwr_lock);
		usbc_otg_power_enable = (noti->vbus_state.mv) ? true : false;
		mutex_unlock(&tcpc_otg_pwr_lock);
		queue_work(otg_tcpc_power_workq, &tcpc_otg_power_work);
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		if (noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			pr_info("%s OTG Plug in\n", __func__);
			mutex_lock(&tcpc_otg_lock);
			usbc_otg_enable = true;
			mutex_unlock(&tcpc_otg_lock);
		} else if (noti->typec_state.old_state == TYPEC_ATTACHED_SRC &&
				noti->typec_state.new_state == TYPEC_UNATTACHED) {
			pr_info("%s OTG Plug out\n", __func__);
			mutex_lock(&tcpc_otg_lock);
			usbc_otg_enable = false;
			mutex_unlock(&tcpc_otg_lock);
		}
		queue_work(otg_tcpc_workq, &tcpc_otg_work);
		break;
	}
	return NOTIFY_OK;
}
#else
static int typec_otg_enable(void *data)
{
	pr_info("typec_otg_enable\n");
	musb_typec_host_connect(0);
	return 0;
}

static int typec_otg_disable(void *data)
{
	pr_info("typec_otg_disable\n");
	musb_typec_host_disconnect(0);
	return 0;
}

static int typec_vbus_enable(void *data)
{
	pr_info("typec_vbus_enable\n");
	_set_vbus(mtk_musb, 1);
	return 0;
}

static int typec_vbus_disable(void *data)
{
	pr_info("typec_vbus_disable\n");
	_set_vbus(mtk_musb, 0);
	return 0;
}

static struct typec_switch_data typec_host_driver = {
	.name = "usb20_host",
	.type = HOST_TYPE,
	.enable = typec_otg_enable,
	.disable = typec_otg_disable,
	.vbus_enable = typec_vbus_enable,
	.vbus_disable = typec_vbus_disable,
};
#endif
#endif

static bool musb_is_host(void)
{
	bool host_mode = 0;

	if (typec_control)
		host_mode = typec_req_host;
	else
		host_mode = iddig_req_host;

	return host_mode;
}

void musb_session_restart(struct musb *musb)
{
	void __iomem	*mbase = musb->mregs;

	musb_writeb(mbase, MUSB_DEVCTL, (musb_readb(mbase, MUSB_DEVCTL) & (~MUSB_DEVCTL_SESSION)));
	DBG(0, "[MUSB] stopped session for VBUSERROR interrupt\n");
	USBPHY_SET32(0x6c, (0x3c<<8));
	USBPHY_SET32(0x6c, (0x10<<0));
	USBPHY_CLR32(0x6c, (0x2c<<0));
	DBG(0, "[MUSB] force PHY to idle, 0x6c=%x\n", USBPHY_READ32(0x6c));
	mdelay(5);
	USBPHY_CLR32(0x6c, (0x3c<<8));
	USBPHY_CLR32(0x6c, (0x3c<<0));
	DBG(0, "[MUSB] let PHY resample VBUS, 0x6c=%x\n", USBPHY_READ32(0x6c));
	musb_writeb(mbase, MUSB_DEVCTL, (musb_readb(mbase, MUSB_DEVCTL) | MUSB_DEVCTL_SESSION));
	DBG(0, "[MUSB] restart session\n");
}

static struct delayed_work host_plug_test_work;
int host_plug_test_enable; /* default disable */
module_param(host_plug_test_enable, int, 0400);
int host_plug_in_test_period_ms = 5000;
module_param(host_plug_in_test_period_ms, int, 0400);
int host_plug_out_test_period_ms = 5000;
module_param(host_plug_out_test_period_ms, int, 0400);
int host_test_vbus_off_time_us = 3000;
module_param(host_test_vbus_off_time_us, int, 0400);
int host_test_vbus_only = 1;
module_param(host_test_vbus_only, int, 0400);
static int host_plug_test_triggered;
void switch_int_to_device(struct musb *musb)
{
	irq_set_irq_type(iddig_eint_num, IRQF_TRIGGER_HIGH);
	enable_irq(iddig_eint_num);
	DBG(0, "switch_int_to_device is done\n");
}

void switch_int_to_host(struct musb *musb)
{
	irq_set_irq_type(iddig_eint_num, IRQF_TRIGGER_LOW);
	enable_irq(iddig_eint_num);
	DBG(0, "switch_int_to_host is done\n");
}

void musb_disable_host(struct musb *musb)
{
	if (musb && musb->is_host) {	/* shut down USB host for IPO */
		if (wake_lock_active(&musb->usb_lock))
			wake_unlock(&musb->usb_lock);
		mt_usb_set_vbus(mtk_musb, 0);
		/* add sleep time to ensure vbus off and disconnect irq processed. */
		msleep(50);
		musb_stop(musb);
		MUSB_DEV_MODE(musb);
		/* Think about IPO shutdown with A-cable, then switch to B-cable and IPO bootup.
		 * We need a point to clear session bit
		 */
		musb_writeb(musb->mregs, MUSB_DEVCTL,
				(~MUSB_DEVCTL_SESSION) &
				musb_readb(musb->mregs, MUSB_DEVCTL));

		usb_prepare_clock(false);
	}

#ifdef CONFIG_TCPC_CLASS
	if (!otg_tcpc_dev) {
		DBG(0, "host not inited, directly return\n");
		return;
	}
	DBG(0, "OTG <%p, %p>\n",
			otg_tcpc_dev,
			tcpc_dev_get_by_name(TCPC_OTG_DEV_NAME));
	tcpm_typec_change_role(otg_tcpc_dev, TYPEC_ROLE_SNK);
#else
	if (!iddig_eint_num) {
		DBG(0, "host not inited, directly return\n");
		return;
	}
	DBG(0, "disable iddig<%d>\n", iddig_eint_num);
	/* MASK IDDIG EVENT */
	disable_irq(iddig_eint_num);
#endif

	DBG(0, "disable host done\n");
}
void musb_enable_host(struct musb *musb)
{
#ifdef CONFIG_TCPC_CLASS
	if (!otg_tcpc_dev) {
		DBG(0, "host not inited, directly return\n");
		return;
	}
	DBG(0, "OTG <%p, %p>\n",
			otg_tcpc_dev,
			tcpc_dev_get_by_name(TCPC_OTG_DEV_NAME));
	tcpm_typec_change_role(otg_tcpc_dev, TYPEC_ROLE_DRP);
#else
	if (!iddig_eint_num) {
		DBG(0, "host not inited, directly return\n");
		return;
	}
	DBG(0, "iddig_req_host to 0\n");
	iddig_req_host = 0;
	switch_int_to_host(mtk_musb);	/* resotre ID pin interrupt */
#endif
}
static void do_host_plug_test_work(struct work_struct *data)
{
	static ktime_t ktime_begin, ktime_end;
	static s64 diff_time;
	static int host_on;
	static struct wake_lock host_test_wakelock;
	static int wake_lock_inited;

	if (!wake_lock_inited) {
		DBG(0, "%s wake_lock_init\n", __func__);
		wake_lock_init(&host_test_wakelock, WAKE_LOCK_SUSPEND, "host.test.lock");
		wake_lock_inited = 1;
	}

	host_plug_test_triggered = 1;
	/* sync global status */
	mb();
	wake_lock(&host_test_wakelock);
	DBG(0, "BEGIN");
	ktime_begin = ktime_get();

	host_on  = 1;
	while (1) {
		if (!musb_is_host() && host_on) {
			DBG(0, "about to exit");
			break;
		}
		msleep(50);

		ktime_end = ktime_get();
		diff_time = ktime_to_ms(ktime_sub(ktime_end, ktime_begin));
		if (host_on && diff_time >= host_plug_in_test_period_ms) {
			host_on = 0;
			DBG(0, "OFF\n");

			ktime_begin = ktime_get();

			/* simulate plug out */
			_set_vbus(mtk_musb, 0);
			udelay(host_test_vbus_off_time_us);

			if (!host_test_vbus_only)
				schedule_delayed_work(&mtk_musb->host_work, 0);
		} else if (!host_on && diff_time >= host_plug_out_test_period_ms) {
			host_on = 1;
			DBG(0, "ON\n");

			ktime_begin = ktime_get();
			if (!host_test_vbus_only)
				schedule_delayed_work(&mtk_musb->host_work, 0);

			_set_vbus(mtk_musb, 1);
			msleep(100);

		}
	}

	/* wait host_work done */
	msleep(1000);
	host_plug_test_triggered = 0;
	wake_unlock(&host_test_wakelock);
	DBG(0, "END\n");
}

static int check_vbus(void)
{
	const int wait_cnt = 10;
	const int wait_period = 100;
	int i;
	int ret = -ERANGE;

	for (i = 0; i < wait_cnt; i++) {
		int vbus;

		vbus = pmic_get_vbus();

		DBG(0, "vbus=%d\n", vbus);

		/*
		 * USB_3_1_r1.0.pdf 11.4.5 VBUS Electrical Characteristics
		 * Port (downstream connector) VBUS 4.45~5.25 V
		 *
		 * usb_20.pdf 7.3.2 Bus Timing/Electrical Characteristics
		 * High-power Port VBUS Note 2, Section 7.2.1 4.75 5.25 V
		 */
		if (vbus > 4750 && vbus < 5250) {
			ret = 0;
			break;
		}
		msleep(wait_period);
	}
	return ret;
}

#define ID_PIN_WORK_RECHECK_TIME 30	/* 30 ms */
#define ID_PIN_WORK_BLOCK_TIMEOUT 30000 /* 30000 ms */
static void musb_host_work(struct work_struct *data)
{
	u8 devctl = 0;
	unsigned long flags;
	static int inited, timeout; /* default to 0 */
	static s64 diff_time;
	int host_mode;
	int usb_clk_state = NO_CHANGE;

	/* kernel_init_done should be set in early-init stage through init.$platform.usb.rc */
	if (!inited && !kernel_init_done && !mtk_musb->is_ready && !timeout) {
		ktime_end = ktime_get();
		diff_time = ktime_to_ms(ktime_sub(ktime_end, ktime_start));

		DBG_LIMIT(3, "init_done:%d, is_ready:%d, inited:%d, TO:%d, diff:%lld",
				kernel_init_done, mtk_musb->is_ready, inited, timeout,
				diff_time);

		if (diff_time > ID_PIN_WORK_BLOCK_TIMEOUT) {
			DBG(0, "diff_time:%lld\n", diff_time);
			timeout = 1;
		}

		queue_delayed_work(mtk_musb->st_wq, &mtk_musb->host_work, msecs_to_jiffies(ID_PIN_WORK_RECHECK_TIME));
		return;
	} else if (!inited) {
		DBG(0, "PASS, init_done:%d, is_ready:%d, inited:%d, TO:%d\n",
				kernel_init_done,  mtk_musb->is_ready, inited, timeout);
	}

	inited = 1;

	/* always prepare clock and check if need to unprepater later */
	/* clk_prepare_cnt +1 here */
	usb_prepare_clock(true);

	spin_lock_irqsave(&mtk_musb->lock, flags);
	musb_generic_disable(mtk_musb);
	spin_unlock_irqrestore(&mtk_musb->lock, flags);

	down(&mtk_musb->musb_lock);
	DBG(0, "work start, is_host=%d\n", mtk_musb->is_host);

	if (mtk_musb->in_ipo_off) {
		DBG(0, "do nothing due to in_ipo_off\n");
		goto out;
	}

	/* flip */
	if (host_plug_test_triggered)
		host_mode = !mtk_musb->is_host;
	else
		host_mode = musb_is_host();


	DBG(0, "musb is as %s\n", host_mode?"host":"device");
	switch_set_state((struct switch_dev *)&otg_state, host_mode);

	if (host_mode) {
		/* switch to HOST state before turn on VBUS */
		MUSB_HST_MODE(mtk_musb);

		/* to make sure all event clear */
		msleep(32);
#ifdef CONFIG_MTK_UAC_POWER_SAVING
		if (!usb_on_sram) {
			int ret;

			ret = gpd_switch_to_sram(mtk_musb->controller);
			DBG(0, "gpd_switch_to_sram, ret<%d>\n", ret);
			if (ret == 0)
				usb_on_sram = 1;
		}
#endif
		/* setup fifo for host mode */
		ep_config_from_table_for_host(mtk_musb);
		wake_lock(&mtk_musb->usb_lock);

		/* this make PHY operation workable */
		musb_platform_enable(mtk_musb);

		/* for no VBUS sensing IP*/
		#if 1
		/* wait VBUS ready */
		msleep(100);
		/* clear session*/
		devctl = musb_readb(mtk_musb->mregs, MUSB_DEVCTL);
		musb_writeb(mtk_musb->mregs, MUSB_DEVCTL, (devctl&(~MUSB_DEVCTL_SESSION)));
		set_usb_phy_mode(PHY_IDLE_MODE);
		/* wait */
		mdelay(5);
		/* restart session */
		devctl = musb_readb(mtk_musb->mregs, MUSB_DEVCTL);
		musb_writeb(mtk_musb->mregs, MUSB_DEVCTL, (devctl | MUSB_DEVCTL_SESSION));
		set_usb_phy_mode(PHY_HOST_ACTIVE);
		#endif

		musb_start(mtk_musb);
		if (!typec_control && !host_plug_test_triggered)
			switch_int_to_device(mtk_musb);

		mt_usb_set_vbus(mtk_musb, 1);

		if (check_vbus() < 0)
			DBG(0, "check vbus fail\n");

		if (host_plug_test_enable && !host_plug_test_triggered)
			queue_delayed_work(mtk_musb->st_wq, &host_plug_test_work, 0);

		usb_clk_state = OFF_TO_ON;
	} else {
		/* for device no disconnect interrupt */
		spin_lock_irqsave(&mtk_musb->lock, flags);
		if (mtk_musb->is_active) {
			DBG(0, "for not receiving disconnect interrupt\n");
			usb_hcd_resume_root_hub(musb_to_hcd(mtk_musb));
			musb_root_disconnect(mtk_musb);
		}
		spin_unlock_irqrestore(&mtk_musb->lock, flags);

		DBG(1, "devctl is %x\n", musb_readb(mtk_musb->mregs, MUSB_DEVCTL));
		musb_writeb(mtk_musb->mregs, MUSB_DEVCTL, 0);
		if (wake_lock_active(&mtk_musb->usb_lock))
			wake_unlock(&mtk_musb->usb_lock);
		mt_usb_set_vbus(mtk_musb, 0);

		/* for no VBUS sensing IP */
		#if 1
		set_usb_phy_mode(PHY_IDLE_MODE);
		#endif

		musb_stop(mtk_musb);

		if (!typec_control && !host_plug_test_triggered)
			switch_int_to_host(mtk_musb);

#ifdef CONFIG_MTK_UAC_POWER_SAVING
		if (usb_on_sram) {
			gpd_switch_to_dram(mtk_musb->controller);
			usb_on_sram = 0;
		}
#endif
		/* to make sure all event clear */
		msleep(32);

		mtk_musb->xceiv->otg->state = OTG_STATE_B_IDLE;
		/* switch to DEV state after turn off VBUS */
		MUSB_DEV_MODE(mtk_musb);

		usb_clk_state = ON_TO_OFF;
	}
out:
	DBG(0, "work end, is_host=%d\n", mtk_musb->is_host);
	up(&mtk_musb->musb_lock);

	if (usb_clk_state == ON_TO_OFF) {
		/* clock on -> of: clk_prepare_cnt -2 */
		usb_prepare_clock(false);
		usb_prepare_clock(false);
	} else if (usb_clk_state == NO_CHANGE) {
		/* clock no change : clk_prepare_cnt -1 */
		usb_prepare_clock(false);
	}
}

static irqreturn_t mt_usb_ext_iddig_int(int irq, void *dev_id)
{
	iddig_cnt++;

	iddig_req_host = !iddig_req_host;
	DBG(0, "id pin assert, %s\n", iddig_req_host ? "connect" : "disconnect");
	queue_delayed_work(mtk_musb->st_wq, &mtk_musb->host_work, msecs_to_jiffies(sw_deboun_time));
	disable_irq_nosync(iddig_eint_num);
	return IRQ_HANDLED;
}


static const struct of_device_id otg_iddig_of_match[] = {
	{.compatible = "mediatek,usb_iddig_bi_eint"},
	{},
};

static int otg_iddig_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;

	iddig_eint_num = irq_of_parse_and_map(node, 0);
	DBG(0, "iddig_eint_num<%d>\n", iddig_eint_num);
	if (iddig_eint_num < 0)
		return -ENODEV;

	ret = request_irq(iddig_eint_num, mt_usb_ext_iddig_int, IRQF_TRIGGER_LOW, "USB_IDDIG", NULL);
	if (ret) {
		DBG(0, "request EINT <%d> fail, ret<%d>\n", iddig_eint_num, ret);
		return ret;
	}

	return 0;
}

static struct platform_driver otg_iddig_driver = {
	.probe = otg_iddig_probe,
	/* .remove = otg_iddig_remove, */
	/* .shutdown = otg_iddig_shutdown, */
	.driver = {
		.name = "otg_iddig",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(otg_iddig_of_match),
	},
};


static int iddig_int_init(void)
{
	int	ret = 0;

	ret = platform_driver_register(&otg_iddig_driver);
	if (ret)
		DBG(0, "ret:%d\n", ret);

	return 0;
}

void mt_usb_otg_init(struct musb *musb)
{
	/* BYPASS OTG function in special mode */
	if (get_boot_mode() == META_BOOT
			|| get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT
			|| get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT
	   ) {
		DBG(0, "in special mode %d\n", get_boot_mode());
		return;
	}

	/* test */
	INIT_DELAYED_WORK(&host_plug_test_work, do_host_plug_test_work);
	ktime_start = ktime_get();
	INIT_DELAYED_WORK(&musb->host_work, musb_host_work);

	/* CONNECTION MANAGEMENT*/
#ifdef CONFIG_USB_C_SWITCH
	DBG(0, "host controlled by TYPEC\n");
	typec_control = 1;
#ifdef CONFIG_TCPC_CLASS
	mutex_init(&tcpc_otg_lock);
	mutex_init(&tcpc_otg_pwr_lock);
	otg_tcpc_workq = create_singlethread_workqueue("tcpc_otg_workq");
	otg_tcpc_power_workq = create_singlethread_workqueue("tcpc_otg_power_workq");
	INIT_WORK(&tcpc_otg_power_work, tcpc_otg_power_work_call);
	INIT_WORK(&tcpc_otg_work, tcpc_otg_work_call);
	INIT_DELAYED_WORK(&register_otg_work, do_register_otg_work);
	queue_delayed_work(mtk_musb->st_wq, &register_otg_work, 0);
	vbus_control = 0;
#else
	typec_host_driver.priv_data = NULL;
	register_typec_switch_callback(&typec_host_driver);
	vbus_control = 0;
#endif
#else
	DBG(0, "host controlled by IDDIG\n");
	iddig_int_init();
	vbus_control = 1;
#endif

	/* EP table */
	musb->fifo_cfg_host = fifo_cfg_host;
	musb->fifo_cfg_host_size = ARRAY_SIZE(fifo_cfg_host);

	otg_state.name = "otg_state";
	otg_state.index = 0;
	otg_state.state = 0;

	if (switch_dev_register(&otg_state))
		pr_notice("switch_dev_register fail\n");
	else
		pr_debug("switch_dev register success\n");
}
void mt_usb_otg_exit(struct musb *musb)
{
	DBG(0, "OTG disable vbus\n");
	mt_usb_set_vbus(mtk_musb, 0);
}

#include "hub.h"
static struct delayed_work ep0_stress_work;
struct usb_device *udev;
static void do_ep0_stress_work(struct work_struct *data)
{
	int ret;
	u16 status;

	ret = usb_get_status(udev, USB_RECIP_DEVICE, 0, &status);
	DBG_LIMIT(30, "usb_get_status, ret<%d>", ret);

	if (ret == 0)
		schedule_delayed_work(&ep0_stress_work, 0);
	else {
		DBG(0, "usb_get_status, ret<%d>\n", ret);
		usb_autosuspend_device(udev);
	}
}
static void trigger_ep0_stress_work(void)
{
	int ret;
	struct usb_hcd *hcd = musb_to_hcd(mtk_musb);
	struct usb_device *rhdev = hcd->self.root_hub;
	struct usb_hub *hub = usb_hub_to_struct_hub(rhdev);
	struct usb_port *port_dev = hub->ports[0];

	udev = port_dev->child;
	ret = usb_autoresume_device(udev);
	DBG(0, "usb_autoresume_device, ret<%d>\n", ret);
	if (ret)
		return;

	INIT_DELAYED_WORK(&ep0_stress_work, do_ep0_stress_work);
	schedule_delayed_work(&ep0_stress_work, 0);

}

enum {
	DO_IT = 0,
	REVERT,
};

static void bypass_disc_circuit(int act)
{
	u32 val;

	usb_prepare_enable_clock(true);

	val = USBPHY_READ32(0x18);
	DBG(0, "val<0x%x>\n", val);

	/* 0x18, 13-12 RG_USB20_HSRX_MMODE_SELE, dft:00 */
	if (act == DO_IT) {
		USBPHY_CLR32(0x18, (0x10<<8));
		USBPHY_SET32(0x18, (0x20<<8));
	} else {
		USBPHY_CLR32(0x18, (0x10<<8));
		USBPHY_CLR32(0x18, (0x20<<8));
	}
	val = USBPHY_READ32(0x18);
	DBG(0, "val<0x%x>\n", val);

	usb_prepare_enable_clock(false);
}

static void disc_threshold_to_max(int act)
{
	u32 val;

	usb_prepare_enable_clock(true);

	val = USBPHY_READ32(0x18);
	DBG(0, "val<0x%x>\n", val);

	/* 0x18, 7-4 RG_USB20_DISCTH, dft:1000 */
	if (act == DO_IT) {
		USBPHY_SET32(0x18, (0xf0<<0));
	} else {
		USBPHY_CLR32(0x18, (0x70<<0));
		USBPHY_SET32(0x18, (0x80<<0));
	}

	val = USBPHY_READ32(0x18);
	DBG(0, "val<0x%x>\n", val);

	usb_prepare_enable_clock(false);
}

static int option;
static int set_option(const char *val, const struct kernel_param *kp)
{
	int local_option;
	int rv;

	/* update module parameter */
	rv = param_set_int(val, kp);
	if (rv)
		return rv;

	/* update local_option */
	rv = kstrtoint(val, 10, &local_option);
	if (rv != 0)
		return rv;

	DBG(0, "option:%d, local_option:%d\n", option, local_option);

	switch (local_option) {
	case 0:
		DBG(0, "case %d\n", local_option);
		iddig_int_init();
		break;
	case 1:
		DBG(0, "case %d\n", local_option);
		musb_typec_host_connect(0);
		break;
	case 2:
		DBG(0, "case %d\n", local_option);
		musb_typec_host_disconnect(0);
		break;
	case 3:
		DBG(0, "case %d\n", local_option);
		musb_typec_host_connect(3000);
		break;
	case 4:
		DBG(0, "case %d\n", local_option);
		musb_typec_host_disconnect(3000);
		break;
	case 5:
		DBG(0, "case %d\n", local_option);
		trigger_ep0_stress_work();
		break;
	case 6:
		DBG(0, "case %d\n", local_option);
		disc_threshold_to_max(DO_IT);
		break;
	case 7:
		DBG(0, "case %d\n", local_option);
		disc_threshold_to_max(REVERT);
		break;
	case 8:
		DBG(0, "case %d\n", local_option);
		bypass_disc_circuit(DO_IT);
		break;
	case 9:
		DBG(0, "case %d\n", local_option);
		bypass_disc_circuit(REVERT);
		break;
	case 10:
		DBG(0, "case %d\n", local_option);
		_set_vbus(mtk_musb, 1);
		break;
	case 11:
		DBG(0, "case %d\n", local_option);
		_set_vbus(mtk_musb, 0);
		break;
	case 12:
		DBG(0, "vbus=%d\n", pmic_get_vbus());
		break;
	default:
		break;
	}
	return 0;
}
static struct kernel_param_ops option_param_ops = {
	.set = set_option,
	.get = param_get_int,
};
module_param_cb(option, &option_param_ops, &option, 0400);
#else
#include "musb_core.h"
/* for not define CONFIG_USB_MTK_OTG */
void mt_usb_otg_init(struct musb *musb) {}
void mt_usb_otg_exit(struct musb *musb) {}
void mt_usb_set_vbus(struct musb *musb, int is_on) {}
int mt_usb_get_vbus_status(struct musb *musb) {return 1; }
void switch_int_to_device(struct musb *musb) {}
void switch_int_to_host(struct musb *musb) {}
void musb_disable_host(struct musb *musb) {}
void musb_enable_host(struct musb *musb) {}
void musb_session_restart(struct musb *musb) {}
#endif
