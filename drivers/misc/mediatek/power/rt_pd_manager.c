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
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/pm_wakeup.h>
#include <linux/reboot.h>
#include <linux/pm.h>
#include <linux/cpumask.h>

#include "tcpm.h"

#include <mt-plat/upmu_common.h>
#if CONFIG_MTK_GAUGE_VERSION == 30
#include <mt-plat/charger_class.h>
#include <mt-plat/mtk_battery.h>
#include <mt-plat/mtk_charger.h>
#include <mtk_charger_intf.h>
#else
#include <mt-plat/battery_meter.h>
#include <mt-plat/charging.h>
#endif /* CONFIG_MTK_GAUGE_VERSION */

#include <mt-plat/mtk_boot.h>

#define RT_PD_MANAGER_VERSION	"1.0.5_MTK"

static DEFINE_MUTEX(param_lock);

static struct tcpc_device *tcpc_dev;
static struct notifier_block pd_nb;
static int pd_sink_voltage_new;
static int pd_sink_voltage_old;
static int pd_sink_current_new;
static int pd_sink_current_old;
static unsigned char pd_sink_type;
static bool tcpc_kpoc;
static unsigned char bc12_chr_type;
#if 0 /* vconn is from vsys on mt6763 */
/* vconn boost gpio pin */
static int vconn_gpio;
static unsigned char vconn_on;
#endif

#if CONFIG_MTK_GAUGE_VERSION == 30
static struct charger_device *primary_charger;
static struct charger_consumer *chg_consumer;
#endif

#if CONFIG_MTK_GAUGE_VERSION == 20
#ifdef CONFIG_MTK_PUMP_EXPRESS_PLUS_30_SUPPORT

static DEFINE_MUTEX(pd_chr_mutex);
struct task_struct *pd_thread_handle;

static bool isCableIn;
static bool updatechrdet;

struct wakeup_source chrdet_Thread_lock;

void pd_wake_lock(void)
{
	__pm_stay_awake(&chrdet_Thread_lock);
}

void pd_wake_unlock(void)
{
	__pm_relax(&chrdet_Thread_lock);
}

void pd_chrdet_int_handler(void)
{
	pr_notice("[%s] CHRDET status = %d....\n", __func__,
		pmic_get_register_value(PMIC_RGS_CHRDET));

	if (!upmu_get_rgs_chrdet()) {
		int boot_mode = 0;

		boot_mode = get_boot_mode();

		if (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
			|| boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
			pr_notice("[%s] Unplug Charger/USB\n", __func__);
			kernel_power_off();
		}
	}

	pmic_set_register_value(PMIC_RG_USBDL_RST, 1);
	do_chrdet_int_task();
}

int chrdet_thread_kthread(void *x)
{
	struct sched_param param = {.sched_priority = 98 };

	sched_setscheduler(current, SCHED_FIFO, &param);
	set_current_state(TASK_INTERRUPTIBLE);

	pr_notice("[%s] enter\n", __func__);
	pmic_enable_interrupt(CHRDET_INT_NO, 0, "pd_manager");

	/* Run on a process content */
	while (1) {
		mutex_lock(&pd_chr_mutex);
		if (updatechrdet == true) {
			pr_notice("chrdet_work_handler\n");
			pd_chrdet_int_handler();
		} else
			pr_notice("chrdet_work_handler no update\n");
		mutex_unlock(&pd_chr_mutex);
		set_current_state(TASK_INTERRUPTIBLE);
		pd_wake_unlock();
		schedule();
	}

	return 0;
}

void wake_up_pd_chrdet(void)
{
	pr_notice("[%s]\n", __func__);
	pd_wake_lock();
	if (pd_thread_handle != NULL)
		wake_up_process(pd_thread_handle);
}
#endif /* CONFIG_MTK_PUMP_EXPRESS_PLUS_30_SUPPORT */
#endif /* This part is for GM20 */

enum {
	SINK_TYPE_REMOVE,
	SINK_TYPE_TYPEC,
	SINK_TYPE_PD_TRY,
	SINK_TYPE_PD_CONNECTED,
	SINK_TYPE_REQUEST,
};

void __attribute__((weak)) usb_dpdm_pulldown(bool enable)
{
	pr_notice("%s is not defined\n", __func__);
}

bool mtk_is_pep30_en_unlock(void)
{
	return false;
}

static int pd_tcp_notifier_call(struct notifier_block *nb,
					unsigned long event, void *data)
{
	struct tcp_notify *noti = data;

	switch (event) {
	case TCP_NOTIFY_SOURCE_VCONN:
#if 0 /* vconn is from vsys on mt6763 */
		if (noti->swap_state.new_role) {
			if (!vconn_on) {
				pr_info("%s set vconn enable\n", __func__);
				gpio_set_value(vconn_gpio, 1);
				vconn_on = 1;
			}
		} else {
			if (vconn_on) {
				pr_info("%s set vconn disable\n", __func__);
				gpio_set_value(vconn_gpio, 0);
				vconn_on = 0;
			}
		}
#endif
		break;
	case TCP_NOTIFY_SINK_VBUS:
		mutex_lock(&param_lock);
		pd_sink_voltage_new = noti->vbus_state.mv;
		pd_sink_current_new = noti->vbus_state.ma;

		if (noti->vbus_state.type & TCP_VBUS_CTRL_PD_DETECT)
			pd_sink_type = SINK_TYPE_PD_CONNECTED;
		else if (noti->vbus_state.type == TCP_VBUS_CTRL_REMOVE)
			pd_sink_type = SINK_TYPE_REMOVE;
		else if (noti->vbus_state.type == TCP_VBUS_CTRL_TYPEC)
			pd_sink_type = SINK_TYPE_TYPEC;
		else if (noti->vbus_state.type == TCP_VBUS_CTRL_PD)
			pd_sink_type = SINK_TYPE_PD_TRY;
		else if (noti->vbus_state.type == TCP_VBUS_CTRL_REQUEST)
			pd_sink_type = SINK_TYPE_REQUEST;
		pr_info("%s sink vbus %dmv %dma type(%d)\n", __func__,
			pd_sink_voltage_new, pd_sink_current_new, pd_sink_type);
		mutex_unlock(&param_lock);

		if ((pd_sink_voltage_new != pd_sink_voltage_old) ||
		    (pd_sink_current_new != pd_sink_current_old)) {
			if (pd_sink_voltage_new) {
				/* enable charger */
#if CONFIG_MTK_GAUGE_VERSION == 30
				charger_manager_enable_power_path(chg_consumer,
					MAIN_CHARGER, true);
#else
				mtk_chr_pd_enable_power_path(1);
#endif
				pd_sink_voltage_old = pd_sink_voltage_new;
				pd_sink_current_old = pd_sink_current_new;
			} else if (pd_sink_type == SINK_TYPE_REMOVE) {
				if (tcpc_kpoc)
					break;
#if CONFIG_MTK_GAUGE_VERSION == 30
				charger_manager_enable_power_path(chg_consumer,
					MAIN_CHARGER, false);
#else
				mtk_chr_pd_enable_power_path(0);
#endif
				pd_sink_voltage_old = pd_sink_voltage_new;
				pd_sink_current_old = pd_sink_current_new;
			} else {
				bc12_chr_type = mt_get_charger_type();
				if (bc12_chr_type >= STANDARD_HOST &&
				    bc12_chr_type <= STANDARD_CHARGER)
					break;
				if (tcpc_kpoc)
					break;
				/* disable charge */
#if CONFIG_MTK_GAUGE_VERSION == 30
				charger_manager_enable_power_path(chg_consumer,
					MAIN_CHARGER, false);
#else
				mtk_chr_pd_enable_power_path(0);
#endif
				pd_sink_voltage_old = pd_sink_voltage_new;
				pd_sink_current_old = pd_sink_current_new;
			}
		}
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
			noti->typec_state.new_state == TYPEC_ATTACHED_AUDIO) {
			/* AUDIO plug in */
			pr_info("%s audio plug in\n", __func__);

		} else if (noti->typec_state.old_state == TYPEC_ATTACHED_AUDIO
			&& noti->typec_state.new_state == TYPEC_UNATTACHED) {
			/* AUDIO plug out */
			pr_info("%s audio plug out\n", __func__);
		}
		break;
	case TCP_NOTIFY_PD_STATE:
		pr_info("%s pd state = %d\n",
			__func__, noti->pd_state.connected);
		break;
#ifdef CONFIG_TYPEC_NOTIFY_ATTACHWAIT_SNK
	case TCP_NOTIFY_ATTACHWAIT_SNK:
		pr_info("%s attach wait sink\n", __func__);
		break;
#endif /* CONFIG_TYPEC_NOTIFY_ATTACHWAIT_SNK */
	case TCP_NOTIFY_EXT_DISCHARGE:
		pr_info("%s ext discharge = %d\n", __func__, noti->en_state.en);
#if CONFIG_MTK_GAUGE_VERSION == 30
		charger_dev_enable_discharge(primary_charger,
			noti->en_state.en);
#endif
		break;

	case TCP_NOTIFY_WD_STATUS:
		pr_info("%s wd status = %d\n",
			__func__, noti->wd_status.water_detected);

		if (noti->wd_status.water_detected) {
			usb_dpdm_pulldown(false);
			if (tcpc_kpoc) {
				pr_info("Water is detected in KPOC, disable HV charging\n");
				charger_manager_enable_high_voltage_charging(
					chg_consumer, false);
			}
		} else {
			usb_dpdm_pulldown(true);
			if (tcpc_kpoc) {
				pr_info("Water is removed in KPOC, enable HV charging\n");
				charger_manager_enable_high_voltage_charging(
					chg_consumer, true);
			}
		}
		break;
	case TCP_NOTIFY_CABLE_TYPE:
		pr_info("%s cable type = %d\n", __func__,
			noti->cable_type.type);
		break;
	case TCP_NOTIFY_PLUG_OUT:
		pr_info("%s typec plug out\n", __func__);

		if (tcpc_kpoc) {
			pr_info("[%s] typec cable plug out, power off\n",
				__func__);
			kernel_power_off();
		}
		break;
	default:
		break;
	};
	return NOTIFY_OK;
}

static int rt_pd_manager_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *node = pdev->dev.of_node;

	pr_info("%s (%s)\n", __func__, RT_PD_MANAGER_VERSION);
	if (node == NULL) {
		pr_err("%s devicd of node not exist\n", __func__);
		return -ENODEV;
	}

	ret = get_boot_mode();
	if (ret == KERNEL_POWER_OFF_CHARGING_BOOT ||
	    ret == LOW_POWER_OFF_CHARGING_BOOT)
		tcpc_kpoc = true;
	pr_info("%s KPOC(%d)\n", __func__, tcpc_kpoc);


	/* Get charger device */
#if CONFIG_MTK_GAUGE_VERSION == 30
	primary_charger = get_charger_by_name("primary_chg");
	if (!primary_charger) {
		pr_err("%s: get primary charger device failed\n", __func__);
		return -ENODEV;
	}
	chg_consumer = charger_manager_get_by_name(&pdev->dev, "charger_port1");
	if (!chg_consumer) {
		pr_err("%s: get charger consumer device failed\n", __func__);
		return -ENODEV;
	}
#endif /* CONFIG_MTK_GAUGE_VERSION == 30 */

#if 0 /* vconn is from vsys on mt6759 */
#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
	ret = of_get_named_gpio(node, "rt,pd_vconn_gpio", 0);
	if (ret < 0) {
		pr_err("%s no pd_vconn_gpio info\n", __func__);
		return -EINVAL;
	}
	vconn_gpio = ret;
#else
	ret =  of_property_read_u32(node, "rt,pd_vconn_gpio_x", &vconn_gpio);
	if (ret < 0) {
		pr_err("%s no pd_vconn_gpio_x info\n", __func__);
		return -EINVAL;
	}
#endif
	pr_info("%s Vconn gpio = %d\n", __func__, vconn_gpio);
	ret = gpio_request_one(vconn_gpio, GPIOF_OUT_INIT_LOW,
		"pd,vconn_source");
	if (ret < 0) {
		pr_err("%s gpio request fail\n", __func__);
		return ret;
	}
#endif

	tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
	if (!tcpc_dev) {
		pr_err("%s get tcpc device type_c_port0 fail\n", __func__);
		return -ENODEV;
	}

	pd_nb.notifier_call = pd_tcp_notifier_call;
	ret = register_tcp_dev_notifier(tcpc_dev, &pd_nb, TCP_NOTIFY_TYPE_ALL);
	if (ret < 0) {
		pr_err("%s: register tcpc notifer fail\n", __func__);
		return -EINVAL;
	}


#if CONFIG_MTK_GAUGE_VERSION == 20
#ifdef CONFIG_MTK_PUMP_EXPRESS_PLUS_30_SUPPORT
	wakeup_source_init(&chrdet_Thread_lock, "pd chrdet wakelock");
	pd_thread_handle = kthread_create(chrdet_thread_kthread, (void *)NULL,
		"pd_chrdet_thread");
	if (IS_ERR(pd_thread_handle)) {
		pd_thread_handle = NULL;
		pr_err("[pd_thread_handle] creation fails\n");
	} else {
		pr_notice("[pd_thread_handle] kthread_create Done\n");
	}
#endif /* CONFIG_MTK_PUMP_EXPRESS_PLUS_30_SUPPORT */
#endif /* This part is for GM20 */

	pr_info("%s OK!!\n", __func__);
	return ret;
}

static int rt_pd_manager_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id rt_pd_manager_of_match[] = {
	{ .compatible = "mediatek,rt-pd-manager" },
	{ }
};
MODULE_DEVICE_TABLE(of, rt_pd_manager_of_match);

static struct platform_driver rt_pd_manager_driver = {
	.driver = {
		.name = "rt-pd-manager",
		.of_match_table = of_match_ptr(rt_pd_manager_of_match),
	},
	.probe = rt_pd_manager_probe,
	.remove = rt_pd_manager_remove,
};

static int __init rt_pd_manager_init(void)
{
	return platform_driver_register(&rt_pd_manager_driver);
}

static void __exit rt_pd_manager_exit(void)
{
	platform_driver_unregister(&rt_pd_manager_driver);
}

late_initcall(rt_pd_manager_init);
module_exit(rt_pd_manager_exit);

MODULE_AUTHOR("Jeff Chang");
MODULE_DESCRIPTION("Richtek pd manager driver");
MODULE_LICENSE("GPL");
