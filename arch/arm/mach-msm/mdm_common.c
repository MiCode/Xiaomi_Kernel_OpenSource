/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/debugfs.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#include <linux/mfd/pmic8058.h>
#include <linux/msm_charm.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <mach/mdm2.h>
#include <mach/restart.h>
#include <mach/subsystem_notif.h>
#include <mach/subsystem_restart.h>
#include <mach/rpm.h>
#include <mach/gpiomux.h>
#include <mach/socinfo.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include "msm_watchdog.h"
#include "mdm_private.h"
#include "sysmon.h"

#define MDM_MODEM_TIMEOUT	6000
#define MDM_MODEM_DELTA	100
#define MDM_BOOT_TIMEOUT	60000L
#define MDM_RDUMP_TIMEOUT	120000L
#define MDM2AP_STATUS_TIMEOUT_MS 120000L

/* Allow a maximum device id of this many digits */
#define MAX_DEVICE_DIGITS  10
#define EXTERNAL_MODEM "external_modem"
#define SUBSYS_NAME_LENGTH \
	(sizeof(EXTERNAL_MODEM) + MAX_DEVICE_DIGITS)

#define DEVICE_BASE_NAME "mdm"
#define DEVICE_NAME_LENGTH \
	(sizeof(DEVICE_BASE_NAME) + MAX_DEVICE_DIGITS)

#define RD_BUF_SIZE			100
#define SFR_MAX_RETRIES		10
#define SFR_RETRY_INTERVAL	1000

enum gpio_update_config {
	GPIO_UPDATE_BOOTING_CONFIG = 1,
	GPIO_UPDATE_RUNNING_CONFIG,
};

#define NORMAL_MODES_STR "normal"
#define NORMAL_MODES_VALUE 0x03

struct mdm_device {
	struct list_head		link;
	struct mdm_modem_drv	mdm_data;
	struct platform_device *pdev;

	int mdm2ap_status_valid_old_config;
	struct gpiomux_setting mdm2ap_status_old_config;
	int first_boot;
	struct workqueue_struct *mdm_queue;
	struct workqueue_struct *mdm_sfr_queue;
	unsigned int dump_timeout_ms;

	char subsys_name[SUBSYS_NAME_LENGTH];
	struct subsys_desc mdm_subsys;
	struct subsys_device *mdm_subsys_dev;

	char device_name[DEVICE_NAME_LENGTH];
	struct miscdevice misc_device;

	struct completion mdm_needs_reload;
	struct completion mdm_boot;
	struct completion mdm_ram_dumps;
	int mdm_errfatal_irq;
	int mdm_status_irq;
	int mdm_pblrdy_irq;

	struct delayed_work mdm2ap_status_check_work;
	struct work_struct mdm_status_work;
	struct work_struct sfr_reason_work;

	int ssr_started_internally;

	struct mdm_vddmin_resource vddmin_resource;
};

static struct list_head	mdm_devices;
static DEFINE_SPINLOCK(mdm_devices_lock);

static int ssr_count;
static DEFINE_SPINLOCK(ssr_lock);

static unsigned int mdm_debug_mask;
int vddmin_gpios_sent;
static struct mdm_ops *mdm_ops;

/* Required gpios */
static const int required_gpios[] = {
	MDM2AP_ERRFATAL,
	AP2MDM_ERRFATAL,
	MDM2AP_STATUS,
	AP2MDM_STATUS,
	AP2MDM_SOFT_RESET
};

static struct gpio_map {
	const char *name;
	int index;
} gpio_map[] = {
	{"qcom,mdm2ap-errfatal-gpio",   MDM2AP_ERRFATAL},
	{"qcom,ap2mdm-errfatal-gpio",   AP2MDM_ERRFATAL},
	{"qcom,mdm2ap-status-gpio",     MDM2AP_STATUS},
	{"qcom,ap2mdm-status-gpio",     AP2MDM_STATUS},
	{"qcom,mdm2ap-pblrdy-gpio",     MDM2AP_PBLRDY},
	{"qcom,ap2mdm-wakeup-gpio",     AP2MDM_WAKEUP},
	{"qcom,ap2mdm-chnlrdy-gpio",     AP2MDM_CHNLRDY},
	{"qcom,mdm2ap-wakeup-gpio",     MDM2AP_WAKEUP},
	{"qcom,ap2mdm-vddmin-gpio",     AP2MDM_VDDMIN},
	{"qcom,mdm2ap-vddmin-gpio",     MDM2AP_VDDMIN},
	{"qcom,ap2mdm-pmic-pwr-en-gpio", AP2MDM_PMIC_PWR_EN},
	{"qcom,use-usb-port-gpio",       USB_SW},
};

static void mdm_debug_gpio_show(struct mdm_device *mdev)
{
	struct mdm_modem_drv *mdm_drv = &mdev->mdm_data;

	pr_debug("%s: MDM2AP_ERRFATAL gpio = %d\n",
			__func__, MDM_GPIO(MDM2AP_ERRFATAL));
	pr_debug("%s: AP2MDM_ERRFATAL gpio = %d\n",
			__func__, MDM_GPIO(AP2MDM_ERRFATAL));
	pr_debug("%s: MDM2AP_STATUS gpio = %d\n",
			__func__, MDM_GPIO(MDM2AP_STATUS));
	pr_debug("%s: AP2MDM_STATUS gpio = %d\n",
			__func__, MDM_GPIO(AP2MDM_STATUS));
	pr_debug("%s: AP2MDM_SOFT_RESET gpio = %d\n",
			__func__, MDM_GPIO(AP2MDM_SOFT_RESET));
	pr_debug("%s: MDM2AP_WAKEUP gpio = %d\n",
			__func__, MDM_GPIO(MDM2AP_WAKEUP));
	pr_debug("%s: AP2MDM_WAKEUP gpio = %d\n",
			 __func__, MDM_GPIO(AP2MDM_WAKEUP));
	pr_debug("%s: AP2MDM_KPDPWR gpio = %d\n",
			 __func__, MDM_GPIO(AP2MDM_KPDPWR));
	pr_debug("%s: AP2MDM_PMIC_PWR_EN gpio = %d\n",
			 __func__, MDM_GPIO(AP2MDM_PMIC_PWR_EN));
	pr_debug("%s: MDM2AP_PBLRDY gpio = %d\n",
			 __func__, MDM_GPIO(MDM2AP_PBLRDY));
	pr_debug("%s: USB_SW gpio = %d\n",
			 __func__, MDM_GPIO(USB_SW));
	pr_debug("%s: AP2MDM_VDDMIN gpio = %d\n",
			 __func__, MDM_GPIO(AP2MDM_VDDMIN));
	pr_debug("%s: MDM2AP_VDDMIN gpio = %d\n",
			 __func__, MDM_GPIO(MDM2AP_VDDMIN));
}

static void mdm_device_list_add(struct mdm_device *mdev)
{
	unsigned long flags;

	spin_lock_irqsave(&mdm_devices_lock, flags);
	list_add_tail(&mdev->link, &mdm_devices);
	spin_unlock_irqrestore(&mdm_devices_lock, flags);
}

static void mdm_device_list_remove(struct mdm_device *mdev)
{
	unsigned long flags;
	struct mdm_device *lmdev, *tmp;

	spin_lock_irqsave(&mdm_devices_lock, flags);
	list_for_each_entry_safe(lmdev, tmp, &mdm_devices, link) {
		if (mdev && mdev == lmdev) {
			pr_debug("%s: removing device id %d\n",
			  __func__, mdev->mdm_data.device_id);
			list_del(&mdev->link);
			break;
		}
	}
	spin_unlock_irqrestore(&mdm_devices_lock, flags);
}

/* If the platform's cascading_ssr flag is set, the subsystem
 * restart module will restart the other modems so stop
 * monitoring them as well.
 * This function can be called from interrupt context.
 */
static void mdm_start_ssr(struct mdm_device *mdev)
{
	unsigned long flags;
	int start_ssr = 1;

	spin_lock_irqsave(&ssr_lock, flags);
	if (mdev->mdm_data.pdata->cascading_ssr &&
			ssr_count > 0) {
		start_ssr = 0;
	} else {
		ssr_count++;
		mdev->ssr_started_internally = 1;
	}
	spin_unlock_irqrestore(&ssr_lock, flags);

	if (start_ssr) {
		atomic_set(&mdev->mdm_data.mdm_ready, 0);
		pr_debug("%s: Resetting mdm id %d due to mdm error\n",
				__func__, mdev->mdm_data.device_id);
		subsystem_restart_dev(mdev->mdm_subsys_dev);
	} else {
		pr_debug("%s: Another modem is already in SSR\n",
				__func__);
	}
}

/* Increment the reference count to handle the case where
 * subsystem restart is initiated by the SSR service.
 */
static void mdm_ssr_started(struct mdm_device *mdev)
{
	unsigned long flags;

	spin_lock_irqsave(&ssr_lock, flags);
	ssr_count++;
	atomic_set(&mdev->mdm_data.mdm_ready, 0);
	spin_unlock_irqrestore(&ssr_lock, flags);
}

/* mdm_ssr_completed assumes that mdm_ssr_started has previously
 * been called.
 */
static void mdm_ssr_completed(struct mdm_device *mdev)
{
	unsigned long flags;

	spin_lock_irqsave(&ssr_lock, flags);
	ssr_count--;
	if (mdev->ssr_started_internally) {
		mdev->ssr_started_internally = 0;
		ssr_count--;
	}

	if (ssr_count < 0) {
		pr_err("%s: ssr_count = %d\n",
			    __func__, ssr_count);
		panic("%s: ssr_count = %d < 0\n",
			  __func__, ssr_count);
	}
	spin_unlock_irqrestore(&ssr_lock, flags);
}

static irqreturn_t mdm_vddmin_change(int irq, void *dev_id)
{
	struct mdm_device *mdev = (struct mdm_device *)dev_id;
	struct mdm_vddmin_resource *vddmin_res;
	int value;

	if (!mdev)
		goto handled;

	vddmin_res = mdev->mdm_data.pdata->vddmin_resource;
	if (!vddmin_res)
		goto handled;

	value = gpio_get_value(
	   vddmin_res->mdm2ap_vddmin_gpio);
	if (value == 0)
		pr_debug("External Modem id %d entered Vddmin\n",
				mdev->mdm_data.device_id);
	else
		pr_debug("External Modem id %d exited Vddmin\n",
				mdev->mdm_data.device_id);
handled:
	return IRQ_HANDLED;
}

/* The vddmin_res resource may not be supported by some platforms. */
static void mdm_setup_vddmin_gpios(void)
{
	unsigned long flags;
	struct msm_rpm_iv_pair req;
	struct mdm_device *mdev;
	struct mdm_vddmin_resource *vddmin_res;
	int irq, ret;

	spin_lock_irqsave(&mdm_devices_lock, flags);
	list_for_each_entry(mdev, &mdm_devices, link) {
		vddmin_res = mdev->mdm_data.pdata->vddmin_resource;
		if (!vddmin_res)
			continue;

		pr_debug("Enabling vddmin logging on modem id %d\n",
				mdev->mdm_data.device_id);
		req.id = vddmin_res->rpm_id;
		req.value =
			((uint32_t)vddmin_res->ap2mdm_vddmin_gpio & 0x0000FFFF)
						<< 16;
		req.value |= ((uint32_t)vddmin_res->modes & 0x000000FF) << 8;
		req.value |= (uint32_t)vddmin_res->drive_strength & 0x000000FF;

		msm_rpm_set(MSM_RPM_CTX_SET_0, &req, 1);

		/* Start monitoring low power gpio from mdm */
		irq = platform_get_irq_byname(mdev->pdev, "mdm2ap_vddmin_irq");
		if (irq < 0)
			pr_err("%s: could not get LPM POWER IRQ resource mdm id %d.\n",
				   __func__, mdev->mdm_data.device_id);
		else {
			ret = request_threaded_irq(irq, NULL, mdm_vddmin_change,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"mdm lpm", mdev);

			if (ret < 0)
				pr_err("%s: MDM LPM IRQ#%d request failed with error=%d",
					   __func__, irq, ret);
		}
	}
	spin_unlock_irqrestore(&mdm_devices_lock, flags);
	return;
}

static void mdm_restart_reason_fn(struct work_struct *work)
{
	int ret, ntries = 0;
	char sfr_buf[RD_BUF_SIZE];
	struct mdm_platform_data *pdata;
	struct mdm_device *mdev = container_of(work,
			struct mdm_device, sfr_reason_work);

	pdata = mdev->mdm_data.pdata;
	if (pdata->sysmon_subsys_id_valid) {
		do {
			ret = sysmon_get_reason(pdata->sysmon_subsys_id,
					sfr_buf, sizeof(sfr_buf));
			if (!ret) {
				pr_err("mdm restart reason: %s\n", sfr_buf);
				return;
			}
			/* Wait for the modem to be fully booted after a
			 * subsystem restart. This may take several seconds.
			 */
			msleep(SFR_RETRY_INTERVAL);
		} while (++ntries < SFR_MAX_RETRIES);
		pr_debug("%s: Error retrieving restart reason: %d\n",
				__func__, ret);
	}
}

static void mdm2ap_status_check(struct work_struct *work)
{
	struct mdm_device *mdev =
		container_of(work, struct mdm_device,
					 mdm2ap_status_check_work.work);
	struct mdm_modem_drv *mdm_drv = &mdev->mdm_data;
	/*
	 * If the mdm modem did not pull the MDM2AP_STATUS gpio
	 * high then call subsystem_restart.
	 */
	if (!mdm_drv->disable_status_check) {
		if (gpio_get_value(MDM_GPIO(MDM2AP_STATUS)) == 0) {
			pr_debug("%s: MDM2AP_STATUS did not go high on mdm id %d\n",
				   __func__, mdev->mdm_data.device_id);
			mdm_start_ssr(mdev);
		}
	}
}

static void mdm_update_gpio_configs(struct mdm_device *mdev,
				enum gpio_update_config gpio_config)
{
	struct mdm_modem_drv *mdm_drv = &mdev->mdm_data;

	/* Some gpio configuration may need updating after modem bootup.*/
	switch (gpio_config) {
	case GPIO_UPDATE_RUNNING_CONFIG:
		if (mdm_drv->pdata->mdm2ap_status_gpio_run_cfg) {
			if (msm_gpiomux_write(MDM_GPIO(MDM2AP_STATUS),
				GPIOMUX_ACTIVE,
				mdm_drv->pdata->mdm2ap_status_gpio_run_cfg,
				&mdev->mdm2ap_status_old_config))
				pr_err("%s: failed updating running gpio config mdm id %d\n",
					   __func__, mdev->mdm_data.device_id);
			else
				mdev->mdm2ap_status_valid_old_config = 1;
		}
		break;
	case GPIO_UPDATE_BOOTING_CONFIG:
		if (mdev->mdm2ap_status_valid_old_config) {
			msm_gpiomux_write(MDM_GPIO(MDM2AP_STATUS),
					GPIOMUX_ACTIVE,
					&mdev->mdm2ap_status_old_config,
					NULL);
			mdev->mdm2ap_status_valid_old_config = 0;
		}
		break;
	default:
		pr_err("%s: called with no config\n", __func__);
		break;
	}
}

static long mdm_modem_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	int status, ret = 0;
	struct mdm_device *mdev = filp->private_data;
	struct mdm_modem_drv *mdm_drv;

	if (_IOC_TYPE(cmd) != CHARM_CODE) {
		pr_err("%s: invalid ioctl code to mdm id %d\n",
			   __func__, mdev->mdm_data.device_id);
		return -EINVAL;
	}

	mdm_drv = &mdev->mdm_data;
	pr_debug("%s: Entering ioctl cmd = %d, mdm id = %d\n",
			 __func__, _IOC_NR(cmd), mdev->mdm_data.device_id);
	switch (cmd) {
	case WAKE_CHARM:
		pr_debug("%s: Powering on mdm id %d\n",
				__func__, mdev->mdm_data.device_id);
		mdm_ops->power_on_mdm_cb(mdm_drv);
		break;
	case CHECK_FOR_BOOT:
		if (gpio_get_value(MDM_GPIO(MDM2AP_STATUS)) == 0)
			put_user(1, (unsigned long __user *) arg);
		else
			put_user(0, (unsigned long __user *) arg);
		break;
	case NORMAL_BOOT_DONE:
		pr_debug("%s: check if mdm id %d is booted up\n",
				 __func__, mdev->mdm_data.device_id);
		get_user(status, (unsigned long __user *) arg);
		if (status) {
			pr_debug("%s: normal boot of mdm id %d failed\n",
					 __func__, mdev->mdm_data.device_id);
			mdm_drv->mdm_boot_status = -EIO;
		} else {
			pr_debug("%s: normal boot of mdm id %d done\n",
					__func__, mdev->mdm_data.device_id);
			mdm_drv->mdm_boot_status = 0;
		}
		atomic_set(&mdm_drv->mdm_ready, 1);

		if (mdm_ops->normal_boot_done_cb != NULL)
			mdm_ops->normal_boot_done_cb(mdm_drv);

		if (!mdev->first_boot)
			complete(&mdev->mdm_boot);
		else
			mdev->first_boot = 0;

		/* If userspace has reset the peripheral device then
		 * inform the modem here.
		 */
		if (GPIO_IS_VALID(MDM_GPIO(AP2MDM_CHNLRDY)))
				gpio_direction_output(
				   MDM_GPIO(AP2MDM_CHNLRDY), 1);

		/* If successful, start a timer to check that the mdm2ap_status
		 * gpio goes high.
		 */
		if (!status && gpio_get_value(MDM_GPIO(MDM2AP_STATUS)) == 0)
			schedule_delayed_work(&mdev->mdm2ap_status_check_work,
				msecs_to_jiffies(MDM2AP_STATUS_TIMEOUT_MS));
		break;
	case RAM_DUMP_DONE:
		pr_debug("%s: mdm done collecting RAM dumps\n", __func__);
		get_user(status, (unsigned long __user *) arg);
		if (status)
			mdm_drv->mdm_ram_dump_status = -EIO;
		else {
			pr_debug("%s: ramdump collection completed\n",
					 __func__);
			mdm_drv->mdm_ram_dump_status = 0;
		}
		complete(&mdev->mdm_ram_dumps);
		break;
	case WAIT_FOR_RESTART:
		pr_debug("%s: wait for mdm to need images reloaded\n",
				__func__);
		ret = wait_for_completion_interruptible(
				&mdev->mdm_needs_reload);
		if (!ret)
			put_user(mdm_drv->boot_type,
					 (unsigned long __user *) arg);
		init_completion(&mdev->mdm_needs_reload);
		break;
	case GET_DLOAD_STATUS:
		pr_debug("getting status of mdm2ap_errfatal_gpio\n");
		if (gpio_get_value(MDM_GPIO(MDM2AP_ERRFATAL)) == 1 &&
			!atomic_read(&mdm_drv->mdm_ready))
			put_user(1, (unsigned long __user *) arg);
		else
			put_user(0, (unsigned long __user *) arg);
		break;
	case IMAGE_UPGRADE:
		pr_debug("%s Image upgrade ioctl recieved\n", __func__);
		if (mdm_drv->pdata->image_upgrade_supported &&
				mdm_ops->image_upgrade_cb) {
			get_user(status, (unsigned long __user *) arg);
			mdm_ops->image_upgrade_cb(mdm_drv, status);
		} else
			pr_debug("%s Image upgrade not supported\n", __func__);
		break;
	case SHUTDOWN_CHARM:
		if (!mdm_drv->pdata->send_shdn)
			break;
		atomic_set(&mdm_drv->mdm_ready, 0);
		if (mdm_debug_mask & MDM_DEBUG_MASK_SHDN_LOG)
			pr_debug("Sending shutdown request to mdm\n");
		ret = sysmon_send_shutdown(mdm_drv->pdata->sysmon_subsys_id);
		if (ret)
			pr_err("%s:Graceful shutdown of mdm failed, ret = %d\n",
			   __func__, ret);
		break;
	default:
		pr_err("%s: invalid ioctl cmd = %d\n", __func__, _IOC_NR(cmd));
		ret = -EINVAL;
		break;
	}
	return ret;
}

static void mdm_status_fn(struct work_struct *work)
{
	struct mdm_device *mdev =
		container_of(work, struct mdm_device, mdm_status_work);
	struct mdm_modem_drv *mdm_drv = &mdev->mdm_data;
	int value = gpio_get_value(MDM_GPIO(MDM2AP_STATUS));

	pr_debug("%s: status:%d\n", __func__, value);
	if (atomic_read(&mdm_drv->mdm_ready) && mdm_ops->status_cb)
		mdm_ops->status_cb(mdm_drv, value);

	/* Update gpio configuration to "running" config. */
	mdm_update_gpio_configs(mdev, GPIO_UPDATE_RUNNING_CONFIG);
}

static void mdm_disable_irqs(struct mdm_device *mdev)
{
	if (!mdev)
		return;
	disable_irq_nosync(mdev->mdm_errfatal_irq);
	disable_irq_nosync(mdev->mdm_status_irq);
	disable_irq_nosync(mdev->mdm_pblrdy_irq);
}

static irqreturn_t mdm_errfatal(int irq, void *dev_id)
{
	struct mdm_modem_drv *mdm_drv;
	struct mdm_device *mdev = (struct mdm_device *)dev_id;
	if (!mdev)
		return IRQ_HANDLED;

	pr_debug("%s: mdm id %d sent errfatal interrupt\n",
			 __func__, mdev->mdm_data.device_id);
	mdm_drv = &mdev->mdm_data;
	if (atomic_read(&mdm_drv->mdm_ready) &&
		(gpio_get_value(MDM_GPIO(MDM2AP_STATUS)) == 1)) {
		pr_debug("%s: Received err fatal from mdm id %d\n",
				__func__, mdev->mdm_data.device_id);
		mdm_start_ssr(mdev);
	}
	return IRQ_HANDLED;
}

/* set the mdm_device as the file's private data */
static int mdm_modem_open(struct inode *inode, struct file *file)
{
	struct miscdevice *misc = file->private_data;
	struct mdm_device *mdev = container_of(misc,
			struct mdm_device, misc_device);

	file->private_data = mdev;
	return 0;
}

static void mdm_crash_shutdown(const struct subsys_desc *mdm_subsys)
{
	int i;
	struct mdm_modem_drv *mdm_drv;
	struct mdm_device *mdev = container_of(mdm_subsys, struct mdm_device,
								 mdm_subsys);

	mdm_drv = &mdev->mdm_data;

	pr_debug("%s: setting AP2MDM_ERRFATAL high for a non graceful reset\n",
			 __func__);
	mdm_disable_irqs(mdev);
	gpio_set_value(MDM_GPIO(AP2MDM_ERRFATAL), 1);

	for (i = MDM_MODEM_TIMEOUT; i > 0; i -= MDM_MODEM_DELTA) {
		pet_watchdog();
		mdelay(MDM_MODEM_DELTA);
		if (gpio_get_value(MDM_GPIO(MDM2AP_STATUS)) == 0)
			break;
	}
	if (i <= 0) {
		pr_err("%s: MDM2AP_STATUS never went low\n", __func__);
		/* Reset the modem so that it will go into download mode. */
		if (mdm_drv && mdm_ops->atomic_reset_mdm_cb)
			mdm_ops->atomic_reset_mdm_cb(mdm_drv);
	}
}

static irqreturn_t mdm_status_change(int irq, void *dev_id)
{
	struct mdm_modem_drv *mdm_drv;
	struct mdm_device *mdev = (struct mdm_device *)dev_id;
	int value;
	if (!mdev)
		return IRQ_HANDLED;

	mdm_drv = &mdev->mdm_data;
	value = gpio_get_value(MDM_GPIO(MDM2AP_STATUS));

	if ((mdm_debug_mask & MDM_DEBUG_MASK_SHDN_LOG) && (value == 0))
		pr_debug("%s: mdm2ap_status went low\n", __func__);

	if (value == 0 && atomic_read(&mdm_drv->mdm_ready)) {
		pr_debug("%s: unexpected reset external modem id %d\n",
				__func__, mdev->mdm_data.device_id);
		mdm_drv->mdm_unexpected_reset_occurred = 1;
		mdm_start_ssr(mdev);
	} else if (value == 1) {
		cancel_delayed_work(&mdev->mdm2ap_status_check_work);
		pr_debug("%s: status = 1: mdm id %d is now ready\n",
				__func__, mdev->mdm_data.device_id);
		queue_work(mdev->mdm_queue, &mdev->mdm_status_work);
	}
	return IRQ_HANDLED;
}

static irqreturn_t mdm_pblrdy_change(int irq, void *dev_id)
{
	struct mdm_modem_drv *mdm_drv;
	struct mdm_device *mdev = (struct mdm_device *)dev_id;
	if (!mdev)
		return IRQ_HANDLED;

	mdm_drv = &mdev->mdm_data;
	pr_debug("%s: mdm id %d: pbl ready:%d\n",
			__func__, mdev->mdm_data.device_id,
			gpio_get_value(MDM_GPIO(MDM2AP_PBLRDY)));
	return IRQ_HANDLED;
}

static int mdm_subsys_shutdown(const struct subsys_desc *crashed_subsys,
							bool force_stop)
{
	struct mdm_device *mdev =
	 container_of(crashed_subsys, struct mdm_device, mdm_subsys);
	struct mdm_modem_drv *mdm_drv = &mdev->mdm_data;

	pr_debug("%s: ssr on modem id %d\n", __func__,
			 mdev->mdm_data.device_id);

	mdm_ssr_started(mdev);
	cancel_delayed_work(&mdev->mdm2ap_status_check_work);
	if (!mdm_drv->pdata->no_a2m_errfatal_on_ssr)
		gpio_direction_output(MDM_GPIO(AP2MDM_ERRFATAL), 1);

	if (mdm_drv->pdata->ramdump_delay_ms > 0) {
		/* Wait for the external modem to complete
		 * its preparation for ramdumps.
		 */
		msleep(mdm_drv->pdata->ramdump_delay_ms);
	}
	mdm_drv->mdm_unexpected_reset_occurred = 0;
	return 0;
}

static int mdm_subsys_powerup(const struct subsys_desc *crashed_subsys)
{
	struct mdm_device *mdev =
		container_of(crashed_subsys, struct mdm_device,
					 mdm_subsys);
	struct mdm_modem_drv *mdm_drv = &mdev->mdm_data;

	pr_debug("%s: ssr on modem id %d\n",
			 __func__, mdev->mdm_data.device_id);

	gpio_direction_output(MDM_GPIO(AP2MDM_ERRFATAL), 0);
	gpio_direction_output(MDM_GPIO(AP2MDM_STATUS), 1);

	if (mdm_drv->pdata->ps_hold_delay_ms > 0)
		msleep(mdm_drv->pdata->ps_hold_delay_ms);

	mdm_ops->power_on_mdm_cb(mdm_drv);
	mdm_drv->boot_type = CHARM_NORMAL_BOOT;
	mdm_ssr_completed(mdev);
	complete(&mdev->mdm_needs_reload);
	if (!wait_for_completion_timeout(&mdev->mdm_boot,
			msecs_to_jiffies(MDM_BOOT_TIMEOUT))) {
		mdm_drv->mdm_boot_status = -ETIMEDOUT;
		pr_debug("%s: mdm modem restart timed out.\n", __func__);
	} else {
		pr_debug("%s: id %d: mdm modem has been restarted\n",
				__func__, mdm_drv->device_id);

		/* Log the reason for the restart */
		if (mdm_drv->pdata->sfr_query)
			queue_work(mdev->mdm_sfr_queue, &mdev->sfr_reason_work);
	}
	init_completion(&mdev->mdm_boot);
	return mdm_drv->mdm_boot_status;
}

static int mdm_subsys_ramdumps(int want_dumps,
				const struct subsys_desc *crashed_subsys)
{
	struct mdm_device *mdev =
		container_of(crashed_subsys, struct mdm_device,
					 mdm_subsys);
	struct mdm_modem_drv *mdm_drv = &mdev->mdm_data;

	pr_debug("%s: ssr on modem id %d\n", __func__,
			 mdev->mdm_data.device_id);

	mdm_drv->mdm_ram_dump_status = 0;
	cancel_delayed_work(&mdev->mdm2ap_status_check_work);
	if (want_dumps) {
		mdm_drv->boot_type = CHARM_RAM_DUMPS;
		complete(&mdev->mdm_needs_reload);
		if (!wait_for_completion_timeout(&mdev->mdm_ram_dumps,
				msecs_to_jiffies(mdev->dump_timeout_ms))) {
			mdm_drv->mdm_ram_dump_status = -ETIMEDOUT;
			pr_err("%s: mdm modem ramdumps timed out.\n",
					__func__);
		} else
			pr_debug("%s: mdm modem ramdumps completed.\n",
					__func__);
		init_completion(&mdev->mdm_ram_dumps);
		if (!mdm_drv->pdata->no_powerdown_after_ramdumps) {
			mdm_ops->power_down_mdm_cb(mdm_drv);
			/* Update gpio configuration to "booting" config. */
			mdm_update_gpio_configs(mdev,
					GPIO_UPDATE_BOOTING_CONFIG);
		}
	}
	return mdm_drv->mdm_ram_dump_status;
}

/* Once the gpios are sent to RPM and debugging
 * starts, there is no way to stop it without
 * rebooting the device.
 */
static int mdm_debug_mask_set(void *data, u64 val)
{
	if (!vddmin_gpios_sent &&
		(val & MDM_DEBUG_MASK_VDDMIN_SETUP)) {
		mdm_setup_vddmin_gpios();
		vddmin_gpios_sent = 1;
	}

	mdm_debug_mask = val;
	if (mdm_ops->debug_state_changed_cb)
		mdm_ops->debug_state_changed_cb(mdm_debug_mask);
	return 0;
}

static int mdm_debug_mask_get(void *data, u64 *val)
{
	*val = mdm_debug_mask;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(mdm_debug_mask_fops,
			mdm_debug_mask_get,
			mdm_debug_mask_set, "%llu\n");

static int mdm_debugfs_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("mdm_dbg", 0);
	if (IS_ERR(dent))
		return PTR_ERR(dent);

	debugfs_create_file("debug_mask", 0644, dent, NULL,
			&mdm_debug_mask_fops);
	return 0;
}

static const struct file_operations mdm_modem_fops = {
	.owner		= THIS_MODULE,
	.open		= mdm_modem_open,
	.unlocked_ioctl	= mdm_modem_ioctl,
};

/* Fail if any of the required gpios is absent. */
static int mdm_dt_to_gpio_rscs(struct device_node *node,
				struct mdm_device *mdev)
{
	struct mdm_modem_drv *mdm_drv = &mdev->mdm_data;
	int i, val, rc = 0;
	enum of_gpio_flags flags = OF_GPIO_ACTIVE_LOW;

	for (i = 0; i < GPIO_TOTAL; i++)
		mdm_drv->gpios[i] = INVALID_GPIO;

	for (i = 0; i < ARRAY_SIZE(gpio_map); i++) {
		val = of_get_named_gpio(node, gpio_map[i].name, 0);
		if (val >= 0)
			MDM_GPIO(gpio_map[i].index) = val;
	}

	/* These two are special because they can be inverted. */
	val = of_get_named_gpio_flags(node, "qcom,ap2mdm-soft-reset-gpio",
						0, &flags);
	if (val >= 0) {
		MDM_GPIO(AP2MDM_SOFT_RESET) = val;
		if (flags & OF_GPIO_ACTIVE_LOW)
			mdm_drv->pdata->soft_reset_inverted = 1;
	}

	val = of_get_named_gpio_flags(node, "qcom,ap2mdm-kpdpwr-gpio",
						0, &flags);
	if (val >= 0) {
		MDM_GPIO(AP2MDM_KPDPWR) = val;
		if (!(flags & OF_GPIO_ACTIVE_LOW))
			mdm_drv->pdata->kpd_not_inverted = 1;
	}

	/* Verify that the required gpios have valid values */
	for (i = 0; i < ARRAY_SIZE(required_gpios); i++) {
		if (MDM_GPIO(required_gpios[i]) == INVALID_GPIO) {
			rc = -ENXIO;
			break;
		}
	}
	mdm_debug_gpio_show(mdev);
	return rc;
}

static int mdm_dt_to_vddmin_rscs(struct device_node *node,
				struct mdm_device *mdev)
{
	int ret;
	struct mdm_modem_drv *mdm_drv = &mdev->mdm_data;
	const char *modes_str;

	/* vddmin resources may be absent */
	if ((MDM_GPIO(AP2MDM_VDDMIN) == INVALID_GPIO) ||
		 (MDM_GPIO(MDM2AP_VDDMIN) == INVALID_GPIO))
		return -ENXIO;

	ret = of_property_read_string(node, "qcom,vddmin-modes", &modes_str);
	if (ret < 0) {
		pr_debug("%s: vddmin_modes not set.\n", __func__);
		return -ENXIO;
	} else {
		if (!strcmp(modes_str, NORMAL_MODES_STR))
			mdev->vddmin_resource.modes = NORMAL_MODES_VALUE;
		else
			return -ENXIO;
	}

	ret = of_property_read_u32(node, "qcom,vddmin-drive-strength",
		&mdev->vddmin_resource.drive_strength);
	if (ret < 0) {
		pr_debug("%s: vddmin_drive_strength not set.\n", __func__);
		return -ENXIO;
	}
	mdev->vddmin_resource.ap2mdm_vddmin_gpio =
			MDM_GPIO(AP2MDM_VDDMIN);
	mdev->vddmin_resource.mdm2ap_vddmin_gpio =
			MDM_GPIO(MDM2AP_VDDMIN);

	return 0;
}

static int mdm_dt_to_pdata(struct device_node *node,
				struct mdm_device *mdev)
{
	struct mdm_platform_data *pdata = mdev->mdm_data.pdata;
	int ret;

	ret = mdm_dt_to_gpio_rscs(node, mdev);
	if (ret)
		return ret;

	ret = of_property_read_u32(node, "qcom,ramdump-delay-ms",
				   &pdata->ramdump_delay_ms);
	if (ret < 0)
		pr_debug("%s: ramdump_delay not set.\n", __func__);

	ret = of_property_read_u32(node, "qcom,ps-hold-delay-ms",
				&pdata->ps_hold_delay_ms);
	if (ret < 0)
		pr_debug("%s: ps_hold_delay not set.\n", __func__);

	pdata->early_power_on = of_property_read_bool(node,
					"qcom,early-power-on");

	pdata->sfr_query = of_property_read_bool(node,
					"qcom,sfr-query");

	ret = of_property_read_u32(node, "qcom,ramdump-timeout-ms",
				&pdata->ramdump_timeout_ms);
	if (ret < 0)
		pr_debug("%s: ramdump_timeout not set.\n", __func__);

	pdata->image_upgrade_supported = of_property_read_bool(node,
					"qcom,image-upgrade-supported");

	pdata->send_shdn = of_property_read_bool(node,
					"qcom,support-shutdown");

	ret = of_property_read_u32(node, "qcom,sysmon-subsys-id",
				  &pdata->sysmon_subsys_id);
	if (ret < 0)
		pr_debug("%s: sysmon_subsys_id not set.\n", __func__);
	else if (pdata->sysmon_subsys_id >= 0)
		pdata->sysmon_subsys_id_valid = 1;

	pdata->no_a2m_errfatal_on_ssr = of_property_read_bool(node,
					"qcom,no-a2m-errfatal-on-ssr");

	pdata->no_reset_on_first_powerup = of_property_read_bool(node,
					"qcom,no-reset-on-first-powerup");

	/* vddmin resources may be absent */
	if (!mdm_dt_to_vddmin_rscs(node, mdev))
		pdata->vddmin_resource = &mdev->vddmin_resource;

	return 0;
}

static int mdm_gpios_from_resources(struct platform_device *pdev,
			struct mdm_device *mdev)
{
	struct mdm_modem_drv *mdm_drv = &mdev->mdm_data;
	struct resource *pres;

	/* MDM2AP_ERRFATAL */
	pres = platform_get_resource_byname(pdev, IORESOURCE_IO,
							"MDM2AP_ERRFATAL");
	MDM_GPIO(MDM2AP_ERRFATAL) = pres ? pres->start : -1;

	/* AP2MDM_ERRFATAL */
	pres = platform_get_resource_byname(pdev, IORESOURCE_IO,
							"AP2MDM_ERRFATAL");
	MDM_GPIO(AP2MDM_ERRFATAL) = pres ? pres->start : -1;

	/* MDM2AP_STATUS */
	pres = platform_get_resource_byname(pdev, IORESOURCE_IO,
							"MDM2AP_STATUS");
	MDM_GPIO(MDM2AP_STATUS) = pres ? pres->start : -1;

	/* AP2MDM_STATUS */
	pres = platform_get_resource_byname(pdev, IORESOURCE_IO,
							"AP2MDM_STATUS");
	MDM_GPIO(AP2MDM_STATUS) = pres ? pres->start : -1;

	/* MDM2AP_WAKEUP */
	pres = platform_get_resource_byname(pdev, IORESOURCE_IO,
							"MDM2AP_WAKEUP");
	MDM_GPIO(MDM2AP_WAKEUP) = pres ? pres->start : -1;

	/* AP2MDM_WAKEUP */
	pres = platform_get_resource_byname(pdev, IORESOURCE_IO,
							"AP2MDM_WAKEUP");
	MDM_GPIO(AP2MDM_WAKEUP) = pres ? pres->start : -1;

	/* AP2MDM_SOFT_RESET */
	pres = platform_get_resource_byname(pdev, IORESOURCE_IO,
							"AP2MDM_SOFT_RESET");
	MDM_GPIO(AP2MDM_SOFT_RESET) = pres ? pres->start : -1;

	/* AP2MDM_KPDPWR_N */
	pres = platform_get_resource_byname(pdev, IORESOURCE_IO,
							"AP2MDM_KPDPWR_N");
	MDM_GPIO(AP2MDM_KPDPWR) = pres ? pres->start : -1;

	/* AP2MDM_PMIC_PWR_EN */
	pres = platform_get_resource_byname(pdev, IORESOURCE_IO,
							"AP2MDM_PMIC_PWR_EN");
	MDM_GPIO(AP2MDM_PMIC_PWR_EN) = pres ? pres->start : -1;

	/* MDM2AP_PBLRDY */
	pres = platform_get_resource_byname(pdev, IORESOURCE_IO,
							"MDM2AP_PBLRDY");
	MDM_GPIO(MDM2AP_PBLRDY) = pres ? pres->start : -1;

	/*USB_SW*/
	pres = platform_get_resource_byname(pdev, IORESOURCE_IO,
							"USB_SW");
	MDM_GPIO(USB_SW) = pres ? pres->start : -1;

	return 0;
}

static struct mdm_device
		*mdm_create_device_data(struct platform_device *pdev)
{
	struct mdm_device *mdev = NULL;
	struct mdm_modem_drv *mdm_drv;
	struct mdm_platform_data *pdata = NULL;
	int ret = -1;

	mdev = devm_kzalloc(&pdev->dev, sizeof(struct mdm_device), GFP_KERNEL);
	if (!mdev) {
		dev_err(&pdev->dev, "unable to allocate memory for device data\n");
		return ERR_PTR(-ENOMEM);
	}

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			dev_err(&pdev->dev, "unable to allocate memory for platform data\n");
			return ERR_PTR(-ENOMEM);
		}
		pdev->dev.platform_data = pdata;
	}

	mdm_drv = &mdev->mdm_data;
	mdm_drv->pdata = pdev->dev.platform_data;
	mdev->pdev = pdev;

	if (pdev->dev.of_node)
		ret = mdm_dt_to_pdata(pdev->dev.of_node, mdev);
	else
		ret = mdm_gpios_from_resources(pdev, mdev);

	if (ret) {
		dev_err(&pdev->dev, "invalid platform resources\n");
		return ERR_PTR(ret);
	}

	if (pdev->id < 0)
		mdm_drv->device_id   = 0;
	else
		mdm_drv->device_id   = pdev->id;

	if (mdev->mdm_data.device_id <= 0)
		snprintf(mdev->subsys_name, sizeof(mdev->subsys_name),
			 "%s",  EXTERNAL_MODEM);
	else
		snprintf(mdev->subsys_name, sizeof(mdev->subsys_name),
			 "%s.%d",  EXTERNAL_MODEM, mdev->mdm_data.device_id);
	mdev->mdm_subsys.shutdown = mdm_subsys_shutdown;
	mdev->mdm_subsys.ramdump = mdm_subsys_ramdumps;
	mdev->mdm_subsys.powerup = mdm_subsys_powerup;
	mdev->mdm_subsys.name = mdev->subsys_name;
	mdev->mdm_subsys.crash_shutdown = mdm_crash_shutdown;
	mdev->mdm_subsys.dev = &pdev->dev;

	if (mdev->mdm_data.device_id <= 0)
		snprintf(mdev->device_name, sizeof(mdev->device_name),
			 "%s",  DEVICE_BASE_NAME);
	else
		snprintf(mdev->device_name, sizeof(mdev->device_name),
			 "%s%d",  DEVICE_BASE_NAME, mdev->mdm_data.device_id);
	mdev->misc_device.minor	= MISC_DYNAMIC_MINOR;
	mdev->misc_device.name	= mdev->device_name;
	mdev->misc_device.fops	= &mdm_modem_fops;

	mdm_drv->boot_type = CHARM_NORMAL_BOOT;
	mdev->dump_timeout_ms = mdm_drv->pdata->ramdump_timeout_ms > 0 ?
		mdm_drv->pdata->ramdump_timeout_ms : MDM_RDUMP_TIMEOUT;

	init_completion(&mdev->mdm_needs_reload);
	init_completion(&mdev->mdm_boot);
	init_completion(&mdev->mdm_ram_dumps);

	mdev->first_boot = 1;
	mutex_init(&mdm_drv->peripheral_status_lock);
	if (pdev->dev.of_node)
		mdm_drv->peripheral_status = 1;

	return mdev;
}

static void mdm_deconfigure_ipc(struct mdm_device *mdev)
{
	struct mdm_modem_drv *mdm_drv = &mdev->mdm_data;
	int i;

	for (i = 0; i < GPIO_TOTAL; ++i) {
		if (GPIO_IS_VALID(MDM_GPIO(i)))
			gpio_free(MDM_GPIO(i));
	}
	if (mdev->mdm_queue) {
		destroy_workqueue(mdev->mdm_queue);
		mdev->mdm_queue = NULL;
	}
	if (mdev->mdm_sfr_queue) {
		destroy_workqueue(mdev->mdm_sfr_queue);
		mdev->mdm_sfr_queue = NULL;
	}
}

static int mdm_configure_ipc(struct mdm_device *mdev)
{
	struct mdm_modem_drv *mdm_drv = &mdev->mdm_data;
	int ret = -1, irq;

	/* Multilple gpio_request calls are allowed */
	if (gpio_request(MDM_GPIO(AP2MDM_STATUS), "AP2MDM_STATUS"))
		pr_err("%s Failed to configure AP2MDM_STATUS gpio\n",
			   __func__);

	/* Multilple gpio_request calls are allowed */
	if (gpio_request(MDM_GPIO(AP2MDM_ERRFATAL), "AP2MDM_ERRFATAL"))
		pr_err("%s Failed to configure AP2MDM_ERRFATAL gpio\n",
			   __func__);

	if (GPIO_IS_VALID(MDM_GPIO(AP2MDM_KPDPWR))) {
		if (gpio_request(MDM_GPIO(AP2MDM_KPDPWR), "AP2MDM_KPDPWR_N")) {
			pr_err("%s Failed to configure AP2MDM_KPDPWR gpio\n",
				   __func__);
			goto fatal_err;
		}
	}
	if (gpio_request(MDM_GPIO(MDM2AP_STATUS), "MDM2AP_STATUS")) {
		pr_err("%s Failed to configure MDM2AP_STATUS gpio\n",
			   __func__);
		goto fatal_err;
	}
	if (gpio_request(MDM_GPIO(MDM2AP_ERRFATAL), "MDM2AP_ERRFATAL")) {
		pr_err("%s Failed to configure MDM2AP_ERRFATAL gpio\n",
			   __func__);
		goto fatal_err;
	}
	if (GPIO_IS_VALID(MDM_GPIO(MDM2AP_PBLRDY))) {
		if (gpio_request(MDM_GPIO(MDM2AP_PBLRDY), "MDM2AP_PBLRDY")) {
			pr_err("%s Failed to configure MDM2AP_PBLRDY gpio\n",
				   __func__);
			goto fatal_err;
		}
	}
	if (GPIO_IS_VALID(MDM_GPIO(AP2MDM_PMIC_PWR_EN))) {
		if (gpio_request(MDM_GPIO(AP2MDM_PMIC_PWR_EN),
						 "AP2MDM_PMIC_PWR_EN")) {
			pr_err("%s Failed to configure AP2MDM_PMIC_PWR_EN gpio\n",
				   __func__);
			goto fatal_err;
		}
	}
	if (GPIO_IS_VALID(MDM_GPIO(AP2MDM_SOFT_RESET))) {
		if (gpio_request(MDM_GPIO(AP2MDM_SOFT_RESET),
					 "AP2MDM_SOFT_RESET")) {
			pr_err("%s Failed to configure AP2MDM_SOFT_RESET gpio\n",
				   __func__);
			goto fatal_err;
		}
	}
	if (GPIO_IS_VALID(MDM_GPIO(AP2MDM_WAKEUP))) {
		if (gpio_request(MDM_GPIO(AP2MDM_WAKEUP), "AP2MDM_WAKEUP")) {
			pr_err("%s Failed to configure AP2MDM_WAKEUP gpio\n",
				   __func__);
			goto fatal_err;
		}
	}
	if (GPIO_IS_VALID(MDM_GPIO(AP2MDM_CHNLRDY))) {
		if (gpio_request(MDM_GPIO(AP2MDM_CHNLRDY), "AP2MDM_CHNLRDY")) {
			pr_err("%s Failed to configure AP2MDM_CHNLRDY gpio\n",
				   __func__);
			goto fatal_err;
		}
	}
	if (GPIO_IS_VALID(MDM_GPIO(USB_SW))) {
		if (gpio_request(MDM_GPIO(USB_SW), "USB_SW"))
			pr_err("%s Failed to configure usb switch gpio\n",
				   __func__);
			goto fatal_err;
	}
	gpio_direction_output(MDM_GPIO(AP2MDM_STATUS), 0);
	gpio_direction_output(MDM_GPIO(AP2MDM_ERRFATAL), 0);

	if (GPIO_IS_VALID(MDM_GPIO(AP2MDM_CHNLRDY)))
		gpio_direction_output(MDM_GPIO(AP2MDM_CHNLRDY), 0);

	gpio_direction_input(MDM_GPIO(MDM2AP_STATUS));
	gpio_direction_input(MDM_GPIO(MDM2AP_ERRFATAL));

	mdev->mdm_queue = alloc_workqueue("mdm_queue", 0, 0);
	if (!mdev->mdm_queue) {
		pr_err("%s: could not create mdm_queue for mdm id %d\n",
			   __func__, mdev->mdm_data.device_id);
		ret = -ENOMEM;
		goto fatal_err;
	}

	mdev->mdm_sfr_queue = alloc_workqueue("mdm_sfr_queue", 0, 0);
	if (!mdev->mdm_sfr_queue) {
		pr_err("%s: could not create mdm_sfr_queue for mdm id %d\n",
			   __func__, mdev->mdm_data.device_id);
		ret = -ENOMEM;
		goto fatal_err;
	}

	/* Register subsystem handlers */
	mdev->mdm_subsys_dev = subsys_register(&mdev->mdm_subsys);
	if (IS_ERR(mdev->mdm_subsys_dev)) {
		ret = PTR_ERR(mdev->mdm_subsys_dev);
		goto fatal_err;
	}
	subsys_default_online(mdev->mdm_subsys_dev);

	/* ERR_FATAL irq. */
	irq = platform_get_irq_byname(mdev->pdev, "err_fatal_irq");
	if (irq < 0) {
		pr_err("%s: bad MDM2AP_ERRFATAL IRQ resource, err = %d\n",
			   __func__, irq);
		goto errfatal_err;
	}
	ret = request_irq(irq, mdm_errfatal,
			IRQF_TRIGGER_RISING , "mdm errfatal", mdev);

	if (ret < 0) {
		pr_err("%s: MDM2AP_ERRFATAL IRQ#%d request failed, err=%d\n",
					__func__, irq, ret);
		goto errfatal_err;
	}
	mdev->mdm_errfatal_irq = irq;

errfatal_err:
	 /* status irq */
	irq = platform_get_irq_byname(mdev->pdev, "status_irq");
	if (irq < 0) {
		pr_err("%s: bad MDM2AP_STATUS IRQ resource, err = %d\n",
				__func__, irq);
		goto status_err;
	}

	ret = request_threaded_irq(irq, NULL, mdm_status_change,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		"mdm status", mdev);

	if (ret < 0) {
		pr_err("%s: MDM2AP_STATUS IRQ#%d request failed, err=%d",
			 __func__, irq, ret);
		goto status_err;
	}
	mdev->mdm_status_irq = irq;

status_err:
	if (GPIO_IS_VALID(MDM_GPIO(MDM2AP_PBLRDY))) {
		irq =  platform_get_irq_byname(mdev->pdev, "plbrdy_irq");
		if (irq < 0) {
			pr_err("%s: could not get MDM2AP_PBLRDY IRQ resource\n",
				 __func__);
			goto pblrdy_err;
		}

		ret = request_threaded_irq(irq, NULL, mdm_pblrdy_change,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
				IRQF_ONESHOT,
				"mdm pbl ready", mdev);

		if (ret < 0) {
			pr_err("%s: MDM2AP_PBL IRQ#%d request failed error=%d\n",
				__func__, irq, ret);
			goto pblrdy_err;
		}
		mdev->mdm_pblrdy_irq = irq;
	}

pblrdy_err:
	/*
	 * If AP2MDM_PMIC_PWR_EN gpio is used, pull it high. It remains
	 * high until the whole phone is shut down.
	 */
	if (GPIO_IS_VALID(MDM_GPIO(AP2MDM_PMIC_PWR_EN))) {
		pr_debug("%s: pulling ap2mdm_pmic_pwr_en_gpio high\n",
				 __func__);
		gpio_direction_output(MDM_GPIO(AP2MDM_PMIC_PWR_EN), 1);
	}

	return 0;

fatal_err:
	mdm_deconfigure_ipc(mdev);
	return ret;
}

static int mdm_modem_probe(struct platform_device *pdev)
{
	struct mdm_device *mdev = NULL;
	int ret = -1;

	mdev = mdm_create_device_data(pdev);
	if (IS_ERR(mdev))
		return PTR_ERR(mdev);

	platform_set_drvdata(pdev, mdev);

	if (mdm_ops->debug_state_changed_cb)
		mdm_ops->debug_state_changed_cb(mdm_debug_mask);

	ret = mdm_configure_ipc(mdev);
	if (ret) {
		pr_err("%s: mdm_configure_ipc failed, id = %d\n",
			   __func__, mdev->mdm_data.device_id);
		goto end;
	}

	ret = misc_register(&mdev->misc_device);
	if (ret) {
		pr_err("%s: failed registering mdm id %d, ret = %d\n",
			   __func__, mdev->mdm_data.device_id, ret);
		mdm_deconfigure_ipc(mdev);
		goto end;
	} else {
		pr_info("%s: registered mdm id %d\n",
			   __func__, mdev->mdm_data.device_id);

		mdm_device_list_add(mdev);
		INIT_DELAYED_WORK(&mdev->mdm2ap_status_check_work,
					mdm2ap_status_check);
		INIT_WORK(&mdev->mdm_status_work, mdm_status_fn);
		INIT_WORK(&mdev->sfr_reason_work, mdm_restart_reason_fn);

		/* Perform early powerup of the external modem in order to
		 * allow tabla devices to be found.
		 */
		if (mdev->mdm_data.pdata->early_power_on)
			mdm_ops->power_on_mdm_cb(&mdev->mdm_data);
	}
end:
	return ret;
}

static int mdm_modem_remove(struct platform_device *pdev)
{
	int ret;
	struct mdm_device *mdev = platform_get_drvdata(pdev);

	mdm_deconfigure_ipc(mdev);
	ret = misc_deregister(&mdev->misc_device);
	mdm_device_list_remove(mdev);
	return ret;
}

static void mdm_modem_shutdown(struct platform_device *pdev)
{
	struct mdm_modem_drv *mdm_drv;
	struct mdm_device *mdev = platform_get_drvdata(pdev);

	pr_debug("%s: shutting down device id %d\n",
		 __func__, mdev->mdm_data.device_id);

	mdm_disable_irqs(mdev);
	mdm_drv = &mdev->mdm_data;
	mdm_ops->power_down_mdm_cb(mdm_drv);
	if (GPIO_IS_VALID(MDM_GPIO(AP2MDM_PMIC_PWR_EN)))
		gpio_direction_output(MDM_GPIO(AP2MDM_PMIC_PWR_EN), 0);
}

static struct of_device_id mdm_match_table[] = {
	{.compatible = "qcom,mdm2-modem"},
	{},
};

static struct platform_driver mdm_modem_driver = {
	.probe    = mdm_modem_probe,
	.remove   = mdm_modem_remove,
	.shutdown = mdm_modem_shutdown,
	.driver         = {
		.name = "mdm2_modem",
		.owner = THIS_MODULE,
		.of_match_table = mdm_match_table,
	},
};

static int __init mdm_modem_init(void)
{
	int ret;

	ret = mdm_get_ops(&mdm_ops);
	if (ret)
		return ret;

	INIT_LIST_HEAD(&mdm_devices);
	mdm_debugfs_init();
	return platform_driver_register(&mdm_modem_driver);
}

static void __exit mdm_modem_exit(void)
{
	platform_driver_unregister(&mdm_modem_driver);
}

module_init(mdm_modem_init);
module_exit(mdm_modem_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("mdm modem driver");
MODULE_ALIAS("mdm_modem");

