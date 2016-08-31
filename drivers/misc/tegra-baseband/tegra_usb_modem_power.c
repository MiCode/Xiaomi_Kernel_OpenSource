/*
 * Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/usb.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/pm_qos.h>
#include <linux/regulator/consumer.h>
#include <linux/edp.h>
#include <linux/sysedp.h>
#include <mach/gpio-tegra.h>
#include <linux/platform_data/tegra_usb_modem_power.h>

#define BOOST_CPU_FREQ_MIN	1200000
#define BOOST_CPU_FREQ_TIMEOUT	5000

#define WAKELOCK_TIMEOUT_FOR_USB_ENUM		(HZ * 10)
#define WAKELOCK_TIMEOUT_FOR_REMOTE_WAKE	(HZ)

#define MAX_MODEM_EDP_STATES 10

struct tegra_usb_modem {
	struct tegra_usb_modem_power_platform_data *pdata;
	struct platform_device *pdev;
	struct regulator *regulator; /* modem power regulator */
	unsigned int wake_cnt;	/* remote wakeup counter */
	unsigned int wake_irq;	/* remote wakeup irq */
	bool wake_irq_wakeable; /* LP0 wakeable */
	unsigned int boot_irq;	/* modem boot irq */
	bool boot_irq_wakeable; /* LP0 wakeable */
	struct mutex lock;
	struct wake_lock wake_lock;	/* modem wake lock */
	unsigned int vid;	/* modem vendor id */
	unsigned int pid;	/* modem product id */
	struct usb_device *udev;	/* modem usb device */
	struct usb_device *parent;	/* parent device */
	struct usb_interface *intf;	/* first modem usb interface */
	struct workqueue_struct *wq;	/* modem workqueue */
	struct delayed_work recovery_work;	/* modem recovery work */
	struct work_struct host_load_work;	/* usb host load work */
	struct work_struct host_unload_work;	/* usb host unload work */
	struct pm_qos_request cpu_boost_req; /* min CPU freq request */
	struct work_struct cpu_boost_work;	/* CPU freq boost work */
	struct delayed_work cpu_unboost_work;	/* CPU freq unboost work */
	const struct tegra_modem_operations *ops;	/* modem operations */
	unsigned int capability;	/* modem capability */
	int system_suspend;	/* system suspend flag */
	struct notifier_block pm_notifier;	/* pm event notifier */
	struct notifier_block usb_notifier;	/* usb event notifier */
	int sysfs_file_created;
	int short_autosuspend_enabled;
	struct platform_device *hc;	/* USB host controller */
	struct mutex hc_lock;
	struct edp_client *modem_boot_edp_client;
	struct edp_client modem_edp_client;
	unsigned int modem_edp_states[MAX_MODEM_EDP_STATES];
	int edp_client_registered;
	int edp_boot_client_registered;
	int edp_initialized;
	char *edp_manager_name;
	unsigned int i_breach_ppm; /* percent time current exceeds i_thresh */
	unsigned int i_thresh_3g_adjperiod; /* 3g i_thresh adj period */
	unsigned int i_thresh_lte_adjperiod; /* lte i_thresh adj period */
	struct work_struct edp_work;
	struct mutex edp_lock;
	struct sysedp_consumer *sysedpc;
	unsigned int sysedpc_state_updated;
	struct work_struct sysedp_work;
};


/* supported modems */
static const struct usb_device_id modem_list[] = {
	{USB_DEVICE(0x1983, 0x0310),	/* Icera 450 rev1 */
	 .driver_info = TEGRA_MODEM_AUTOSUSPEND,
	 },
	{USB_DEVICE(0x1983, 0x0321),	/* Icera 450 rev2 */
	 .driver_info = TEGRA_MODEM_AUTOSUSPEND,
	 },
	{USB_DEVICE(0x1983, 0x0327),	/* Icera 450 5AE */
	 .driver_info = TEGRA_MODEM_AUTOSUSPEND,
	 },
	{USB_DEVICE(0x1983, 0x0427),	/* Icera 500 5AN */
	 .driver_info = TEGRA_MODEM_AUTOSUSPEND,
	 },
	{USB_DEVICE(0x1983, 0x1005),	/* Icera 500 5AN (BSD) */
	/* .driver_info = TEGRA_MODEM_AUTOSUSPEND, */
	 },
	{USB_DEVICE(0x1983, 0x1007),	/* Icera 500 Bruce */
	 .driver_info = TEGRA_USB_HOST_RELOAD|TEGRA_MODEM_AUTOSUSPEND,
	 },
	{}
};

static ssize_t load_unload_usb_host(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count);

static void cpu_freq_unboost(struct work_struct *ws)
{
	struct tegra_usb_modem *modem = container_of(ws, struct tegra_usb_modem,
						     cpu_unboost_work.work);

	pm_qos_update_request(&modem->cpu_boost_req, PM_QOS_DEFAULT_VALUE);
}

static void cpu_freq_boost(struct work_struct *ws)
{
	struct tegra_usb_modem *modem = container_of(ws, struct tegra_usb_modem,
						     cpu_boost_work);

	cancel_delayed_work_sync(&modem->cpu_unboost_work);
	pm_qos_update_request(&modem->cpu_boost_req, BOOST_CPU_FREQ_MIN);
	queue_delayed_work(modem->wq, &modem->cpu_unboost_work,
			      msecs_to_jiffies(BOOST_CPU_FREQ_TIMEOUT));
}

static void edp_work(struct work_struct *ws)
{
	struct tegra_usb_modem *modem = container_of(ws, struct tegra_usb_modem,
						     edp_work);
	struct edp_manager *mgr;
	int ret;

	mutex_lock(&modem->edp_lock);
	if (!modem->edp_client_registered &&
	    modem->edp_boot_client_registered) {
		pr_err("modem boot client already registered\n");
		goto done;
	}

	/* unregister modem client */
	ret = edp_unregister_client(&modem->modem_edp_client);
	if (ret) {
		pr_err("edp unregistration failed\n");
		goto done;
	}

	modem->edp_client_registered = 0;

	/* register modem boot client */
	mgr = edp_get_manager(modem->edp_manager_name);
	if (!mgr) {
		pr_err("can't get edp manager\n");
		goto done;
	}

	ret = edp_register_client(mgr, modem->modem_boot_edp_client);
	if (ret) {
		pr_err("unable to register modem boot edp client\n");
		goto done;
	}

	/* request E0 */
	ret = edp_update_client_request(modem->modem_boot_edp_client,
					0, NULL);
	if (ret) {
		pr_err("unable to set E0 state\n");
		goto done;
	}

	modem->edp_boot_client_registered = 1;

done:
	mutex_unlock(&modem->edp_lock);
}

static void sysedp_work(struct work_struct *ws)
{
	struct tegra_usb_modem *modem = container_of(ws, struct tegra_usb_modem,
						     sysedp_work);

	mutex_lock(&modem->lock);

	sysedp_set_state(modem->sysedpc, 1);
	modem->sysedpc_state_updated = 1;

	mutex_unlock(&modem->lock);
}

static irqreturn_t tegra_usb_modem_wake_thread(int irq, void *data)
{
	struct tegra_usb_modem *modem = (struct tegra_usb_modem *)data;

	mutex_lock(&modem->lock);
	if (modem->udev && modem->udev->state != USB_STATE_NOTATTACHED) {
		wake_lock_timeout(&modem->wake_lock,
				  WAKELOCK_TIMEOUT_FOR_REMOTE_WAKE);

		dev_info(&modem->pdev->dev, "remote wake (%u)\n",
			 ++(modem->wake_cnt));

		if (!modem->system_suspend) {
			usb_lock_device(modem->udev);
			if (usb_autopm_get_interface(modem->intf) == 0)
				usb_autopm_put_interface_async(modem->intf);
			usb_unlock_device(modem->udev);
		}
#ifdef CONFIG_PM
		if (modem->capability & TEGRA_MODEM_AUTOSUSPEND &&
		    modem->short_autosuspend_enabled) {
			pm_runtime_set_autosuspend_delay(&modem->udev->dev,
					modem->pdata->autosuspend_delay);
			modem->short_autosuspend_enabled = 0;
		}
#endif
	}
	mutex_unlock(&modem->lock);

	return IRQ_HANDLED;
}

static irqreturn_t tegra_usb_modem_boot_thread(int irq, void *data)
{
	struct tegra_usb_modem *modem = (struct tegra_usb_modem *)data;
	int v = gpio_get_value(modem->pdata->boot_gpio);

	dev_info(&modem->pdev->dev, "MDM_COLDBOOT %s\n", v ? "high" : "low");
	if (modem->capability & TEGRA_USB_HOST_RELOAD)
		if (!work_pending(&modem->host_load_work) &&
		    !work_pending(&modem->host_unload_work))
			queue_work(modem->wq, v ? &modem->host_load_work :
				   &modem->host_unload_work);

	/* hold wait lock to complete the enumeration */
	wake_lock_timeout(&modem->wake_lock, WAKELOCK_TIMEOUT_FOR_USB_ENUM);

	/* boost CPU freq */
	if (!work_pending(&modem->cpu_boost_work))
		queue_work(modem->wq, &modem->cpu_boost_work);

	if (modem->edp_initialized && !v)
		queue_work(modem->wq, &modem->edp_work);

	if (modem->sysedpc && !modem->sysedpc_state_updated)
		queue_work(modem->wq, &modem->sysedp_work);

	/* USB disconnect maybe on going... */
	mutex_lock(&modem->lock);
	if (modem->udev && modem->udev->state != USB_STATE_NOTATTACHED)
		pr_warn("Device is not disconnected!\n");
	mutex_unlock(&modem->lock);

	return IRQ_HANDLED;
}

static void tegra_usb_modem_recovery(struct work_struct *ws)
{
	struct tegra_usb_modem *modem = container_of(ws, struct tegra_usb_modem,
						     recovery_work.work);

	mutex_lock(&modem->lock);
	if (!modem->udev) {	/* assume modem crashed */
		if (modem->ops && modem->ops->reset)
			modem->ops->reset();
	}
	mutex_unlock(&modem->lock);
}

static void tegra_usb_host_load(struct work_struct *ws)
{
	struct tegra_usb_modem *modem = container_of(ws, struct tegra_usb_modem,
						     host_load_work);
	load_unload_usb_host(&modem->pdev->dev, NULL, "1", 1);
}

static void tegra_usb_host_unload(struct work_struct *ws)
{
	struct tegra_usb_modem *modem = container_of(ws, struct tegra_usb_modem,
						     host_unload_work);
	load_unload_usb_host(&modem->pdev->dev, NULL, "0", 1);
}

static void device_add_handler(struct tegra_usb_modem *modem,
			       struct usb_device *udev)
{
	const struct usb_device_descriptor *desc = &udev->descriptor;
	struct usb_interface *intf = usb_ifnum_to_if(udev, 0);
	const struct usb_device_id *id = NULL;

	if (intf) {
		/* only look for specific modems if modem_list is provided in
		   platform data. Otherwise, look for the modems in the default
		   supported modem list */
		if (modem->pdata->modem_list)
			id = usb_match_id(intf, modem->pdata->modem_list);
		else
			id = usb_match_id(intf, modem_list);
	}

	if (id) {
		/* hold wakelock to ensure ril has enough time to restart */
		wake_lock_timeout(&modem->wake_lock,
				  WAKELOCK_TIMEOUT_FOR_USB_ENUM);

		pr_info("Add device %d <%s %s>\n", udev->devnum,
			udev->manufacturer, udev->product);

		mutex_lock(&modem->lock);
		modem->udev = udev;
		modem->parent = udev->parent;
		modem->intf = intf;
		modem->vid = desc->idVendor;
		modem->pid = desc->idProduct;
		modem->wake_cnt = 0;
		modem->capability = id->driver_info;
		mutex_unlock(&modem->lock);

		pr_info("persist_enabled: %u\n", udev->persist_enabled);

#ifdef CONFIG_PM
		if (modem->capability & TEGRA_MODEM_AUTOSUSPEND) {
			pm_runtime_set_autosuspend_delay(&udev->dev,
					modem->pdata->autosuspend_delay);
			modem->short_autosuspend_enabled = 0;
			usb_enable_autosuspend(udev);
			pr_info("enable autosuspend for %s %s\n",
				udev->manufacturer, udev->product);
		}

		/* allow the device to wake up the system */
		if (udev->actconfig->desc.bmAttributes &
		    USB_CONFIG_ATT_WAKEUP)
			device_set_wakeup_enable(&udev->dev, true);

#endif
	}
}

static void device_remove_handler(struct tegra_usb_modem *modem,
				  struct usb_device *udev)
{
	const struct usb_device_descriptor *desc = &udev->descriptor;

	if (desc->idVendor == modem->vid && desc->idProduct == modem->pid) {
		pr_info("Remove device %d <%s %s>\n", udev->devnum,
			udev->manufacturer, udev->product);

		mutex_lock(&modem->lock);
		modem->udev = NULL;
		modem->intf = NULL;
		modem->vid = 0;
		mutex_unlock(&modem->lock);

		if (modem->capability & TEGRA_MODEM_RECOVERY)
			queue_delayed_work(modem->wq,
					   &modem->recovery_work, HZ * 10);
	}
}

static int mdm_usb_notifier(struct notifier_block *notifier,
			    unsigned long usb_event, void *udev)
{
	struct tegra_usb_modem *modem =
	    container_of(notifier, struct tegra_usb_modem, usb_notifier);

	switch (usb_event) {
	case USB_DEVICE_ADD:
		device_add_handler(modem, udev);
		break;
	case USB_DEVICE_REMOVE:
		device_remove_handler(modem, udev);
		break;
	}
	return NOTIFY_OK;
}

static int mdm_pm_notifier(struct notifier_block *notifier,
			   unsigned long pm_event, void *unused)
{
	struct tegra_usb_modem *modem =
	    container_of(notifier, struct tegra_usb_modem, pm_notifier);

	mutex_lock(&modem->lock);
	if (!modem->udev) {
		mutex_unlock(&modem->lock);
		return NOTIFY_DONE;
	}

	pr_info("%s: event %ld\n", __func__, pm_event);
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		if (wake_lock_active(&modem->wake_lock)) {
			pr_warn("%s: wakelock was active, aborting suspend\n",
				__func__);
			mutex_unlock(&modem->lock);
			return NOTIFY_STOP;
		}

		modem->system_suspend = 1;
#ifdef CONFIG_PM
		if (modem->capability & TEGRA_MODEM_AUTOSUSPEND &&
		    modem->udev &&
		    modem->udev->state != USB_STATE_NOTATTACHED) {
			pm_runtime_set_autosuspend_delay(&modem->udev->dev,
					modem->pdata->short_autosuspend_delay);
			modem->short_autosuspend_enabled = 1;
		}
#endif
		mutex_unlock(&modem->lock);
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		modem->system_suspend = 0;
		mutex_unlock(&modem->lock);
		return NOTIFY_OK;
	}

	mutex_unlock(&modem->lock);
	return NOTIFY_DONE;
}

static int mdm_request_irq(struct tegra_usb_modem *modem,
			   irq_handler_t thread_fn,
			   unsigned int irq_gpio,
			   unsigned long irq_flags,
			   const char *label,
			   unsigned int *irq,
			   bool *is_wakeable)
{
	int ret;

	ret = gpio_request(irq_gpio, label);
	if (ret)
		return ret;

	/* enable IRQ for GPIO */
	*irq = gpio_to_irq(irq_gpio);

	/* request threaded irq for GPIO */
	ret = request_threaded_irq(*irq, NULL, thread_fn, irq_flags, label,
				   modem);
	if (ret) {
		*irq = 0;
		return ret;
	}

	ret = enable_irq_wake(*irq);
	*is_wakeable = (ret) ? false : true;

	return 0;
}

static void tegra_usb_modem_post_remote_wakeup(void)
{
	struct device *dev;
	struct tegra_usb_modem *modem;

	dev = bus_find_device_by_name(&platform_bus_type, NULL,
					"MDM");
	if (!dev) {
		pr_warn("%s unable to find device name\n", __func__);
		return;
	}

	modem = dev_get_drvdata(dev);

	mutex_lock(&modem->lock);
#ifdef CONFIG_PM
	if (modem->capability & TEGRA_MODEM_AUTOSUSPEND &&
	    modem->udev &&
	    modem->udev->state != USB_STATE_NOTATTACHED &&
	    modem->short_autosuspend_enabled) {
		pm_runtime_set_autosuspend_delay(&modem->udev->dev,
				modem->pdata->autosuspend_delay);
		modem->short_autosuspend_enabled = 0;
	}
#endif
	wake_lock_timeout(&modem->wake_lock, WAKELOCK_TIMEOUT_FOR_REMOTE_WAKE);
	mutex_unlock(&modem->lock);

	return;
}

/* load USB host controller */
static struct platform_device *tegra_usb_host_register(
				const struct tegra_usb_modem *modem)
{
	const struct platform_device *hc_device =
	    modem->pdata->tegra_ehci_device;
	struct platform_device *pdev;
	int val;

	pdev = platform_device_alloc(hc_device->name, hc_device->id);
	if (!pdev)
		return NULL;

	val = platform_device_add_resources(pdev, hc_device->resource,
					    hc_device->num_resources);
	if (val)
		goto error;

	pdev->dev.dma_mask = hc_device->dev.dma_mask;
	pdev->dev.coherent_dma_mask = hc_device->dev.coherent_dma_mask;

	val = platform_device_add_data(pdev, modem->pdata->tegra_ehci_pdata,
				       sizeof(struct tegra_usb_platform_data));
	if (val)
		goto error;

	val = platform_device_add(pdev);
	if (val)
		goto error;

	return pdev;

error:
	pr_err("%s: err %d\n", __func__, val);
	platform_device_put(pdev);
	return NULL;
}

/* unload USB host controller */
static void tegra_usb_host_unregister(struct platform_device *pdev)
{
	platform_device_unregister(pdev);
}

static ssize_t show_usb_host(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct tegra_usb_modem *modem = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", (modem->hc) ? 1 : 0);
}

static ssize_t load_unload_usb_host(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct tegra_usb_modem *modem = dev_get_drvdata(dev);
	int host;

	if (sscanf(buf, "%d", &host) != 1 || host < 0 || host > 1)
		return -EINVAL;

	pr_info("%s USB host\n", (host) ? "load" : "unload");

	mutex_lock(&modem->hc_lock);
	if (host) {
		if (!modem->hc)
			modem->hc = tegra_usb_host_register(modem);
	} else {
		if (modem->hc) {
			tegra_usb_host_unregister(modem->hc);
			modem->hc = NULL;
		}
	}
	mutex_unlock(&modem->hc_lock);

	return count;
}

static DEVICE_ATTR(load_host, S_IRUSR | S_IWUSR, show_usb_host,
		   load_unload_usb_host);

#define EDP_INT_ATTR(field)						\
static ssize_t								\
field ## _show(struct device *pdev, struct device_attribute *attr,      \
	       char *buf)						\
{									\
	struct tegra_usb_modem *modem = dev_get_drvdata(pdev);		\
									\
	if (!modem)							\
		return -EAGAIN;						\
									\
	return sprintf(buf, "%d\n", modem->field);			\
}									\
static DEVICE_ATTR(field, S_IRUSR, field ## _show, NULL);

EDP_INT_ATTR(i_breach_ppm);
EDP_INT_ATTR(i_thresh_3g_adjperiod);
EDP_INT_ATTR(i_thresh_lte_adjperiod);

static ssize_t i_max_show(struct device *pdev, struct device_attribute *attr,
			  char *buf)
{
	struct tegra_usb_modem *modem = dev_get_drvdata(pdev);
	int i = 0;
	int count = 0;

	mutex_lock(&modem->edp_lock);
	for (i = 0; i < MAX_MODEM_EDP_STATES; i++) {
		if (modem->modem_edp_states[i] <= 0)
			break;

		count += sprintf(&buf[count], "%d ",
				 modem->modem_edp_states[i]);
	}
	mutex_unlock(&modem->edp_lock);

	count += sprintf(&buf[count], "\n");

	return count;
}

static ssize_t i_max_store(struct device *pdev, struct device_attribute *attr,
			   const char *buff, size_t size)
{
	struct tegra_usb_modem *modem;
	char *s, *state_i_max, buf[50];
	unsigned int num_states = 0;
	struct edp_manager *mgr;
	int ret;

	modem = (struct tegra_usb_modem *)dev_get_drvdata(pdev);

	mutex_lock(&modem->edp_lock);

	/* client should only be registered once per modem boot */
	if (modem->edp_client_registered) {
		pr_err("modem edp client already registered\n");
		ret = -EBUSY;
		goto done;
	}

	memset(modem->modem_edp_states, 0, sizeof(modem->modem_edp_states));
	memset(&modem->modem_edp_client, 0, sizeof(modem->modem_edp_client));

	/* retrieve max current for supported states */
	strlcpy(buf, buff, sizeof(buf));
	s = strim(buf);
	while (s && (num_states < MAX_MODEM_EDP_STATES)) {
		state_i_max = strsep(&s, ",");
		ret = kstrtoul(state_i_max, 10,
			(unsigned long *)&modem->modem_edp_states[num_states]);
		if (ret) {
			pr_err("invalid modem state-current setting\n");
			goto done;
		}
		num_states++;
	}

	if (s && (num_states == MAX_MODEM_EDP_STATES)) {
		pr_err("number of modem EDP states exceeded max\n");
		ret = -EINVAL;
		goto done;
	}

	strncpy(modem->modem_edp_client.name, "modem", EDP_NAME_LEN);
	modem->modem_edp_client.name[EDP_NAME_LEN - 1] = '\0';
	modem->modem_edp_client.states = modem->modem_edp_states;
	modem->modem_edp_client.num_states = num_states;
	modem->modem_edp_client.e0_index = 0;
	modem->modem_edp_client.priority = EDP_MAX_PRIO;

	mgr = edp_get_manager(modem->edp_manager_name);
	if (!mgr) {
		dev_err(pdev, "can't get edp manager\n");
		ret = -EINVAL;
		goto done;
	}

	/* register modem client */
	ret = edp_register_client(mgr, &modem->modem_edp_client);
	if (ret) {
		dev_err(pdev, "unable to register modem edp client\n");
		goto done;
	}
	modem->edp_client_registered = 1;

	/* unregister modem_boot_client */
	ret = edp_unregister_client(modem->modem_boot_edp_client);
	if (ret) {
		dev_err(pdev, "unable to register modem boot edp client\n");
		goto done;
	}

	modem->edp_boot_client_registered = 0;
	ret = size;

done:
	mutex_unlock(&modem->edp_lock);
	return ret;
}
static DEVICE_ATTR(i_max, S_IRUSR | S_IWUSR, i_max_show, i_max_store);

static ssize_t request_store(struct device *pdev, struct device_attribute *attr,
			     const char *buff, size_t size)
{
	struct tegra_usb_modem *modem = dev_get_drvdata(pdev);
	struct edp_client *c;
	unsigned int id;
	int ret;

	if (sscanf(buff, "%u", &id) != 1)
		return -EINVAL;

	if (!modem->edp_client_registered)
		return -EINVAL;

	c = &modem->modem_edp_client;
	if (id >= c->num_states)
		return -EINVAL;

	ret = edp_update_client_request(c, id, NULL);
	if (ret)
		dev_err(pdev, "state update to %u failed\n", id);
	else
		ret = size;

	return ret;
}
static DEVICE_ATTR(request, S_IWUSR, NULL, request_store);

static ssize_t threshold_store(struct device *pdev,
			       struct device_attribute *attr,
			       const char *buff, size_t size)
{
	struct tegra_usb_modem *modem = dev_get_drvdata(pdev);
	unsigned int tv;
	int ret;

	if (sscanf(buff, "%u", &tv) != 1)
		return -EINVAL;

	if (!modem->edp_client_registered)
		return -EINVAL;

	ret = edp_update_loan_threshold(&modem->modem_edp_client, tv);
	if (ret)
		dev_err(pdev, "threshold update to %u failed\n", tv);
	else
		ret = size;

	return ret;
}
static DEVICE_ATTR(threshold, S_IWUSR, NULL, threshold_store);

static struct device_attribute *edp_attributes[] = {
	&dev_attr_i_breach_ppm,
	&dev_attr_i_thresh_3g_adjperiod,
	&dev_attr_i_thresh_lte_adjperiod,
	&dev_attr_i_max,
	&dev_attr_request,
	&dev_attr_threshold,
	NULL
};

static struct tegra_usb_phy_platform_ops tegra_usb_modem_platform_ops = {
	.post_remote_wakeup = tegra_usb_modem_post_remote_wakeup,
};

static int mdm_init(struct tegra_usb_modem *modem, struct platform_device *pdev)
{
	struct tegra_usb_modem_power_platform_data *pdata =
	    pdev->dev.platform_data;
	int ret = 0;
	struct edp_manager *mgr;
	struct device_attribute **edp_attrs;
	struct device_attribute *attr;

	modem->pdata = pdata;
	modem->pdev = pdev;

	/* turn on modem regulator if required */
	if (pdata->regulator_name) {
		modem->regulator =
			regulator_get(&pdev->dev, pdata->regulator_name);
		if (!IS_ERR(modem->regulator)) {
			ret = regulator_enable(modem->regulator);
			if (ret)
				goto error;
		} else {
			dev_err(&pdev->dev, "failed to get regulator %s\n",
				pdata->regulator_name);
			ret = PTR_ERR(modem->regulator);
			goto error;
		}
	}

	if (pdata->modem_boot_edp_client && pdata->edp_manager_name) {
		mutex_init(&modem->edp_lock);

		/* register modem boot client */
		modem->edp_manager_name = pdata->edp_manager_name;
		mgr = edp_get_manager(pdata->edp_manager_name);
		if (!mgr) {
			dev_err(&pdev->dev, "can't get edp manager\n");
			goto error;
		}

		modem->modem_boot_edp_client = pdata->modem_boot_edp_client;
		ret = edp_register_client(mgr, modem->modem_boot_edp_client);
		if (ret) {
			dev_err(&pdev->dev,
				"unable to register modem boot edp client\n");
			goto error;
		}

		/* request E0 */
		ret = edp_update_client_request(modem->modem_boot_edp_client,
						0, NULL);
		if (ret) {
			dev_err(&pdev->dev,
				"unable to set e0 state\n");
			goto error;
		}

		modem->edp_boot_client_registered = 1;

		modem->i_breach_ppm = pdata->i_breach_ppm;
		modem->i_thresh_3g_adjperiod = pdata->i_thresh_3g_adjperiod;
		modem->i_thresh_lte_adjperiod = pdata->i_thresh_lte_adjperiod;

		edp_attrs = edp_attributes;
		while ((attr = *edp_attrs++)) {
			ret = device_create_file(&pdev->dev, attr);
			if (ret) {
				dev_err(&pdev->dev, "can't create sysfs file\n");
				goto error;
			}
		}

		INIT_WORK(&modem->edp_work, edp_work);

		modem->edp_initialized = 1;
	}

	if (pdata->sysedpc_name) {
		modem->sysedpc = sysedp_create_consumer(pdata->sysedpc_name,
							pdata->sysedpc_name);

		if (modem->sysedpc)
			INIT_WORK(&modem->sysedp_work, sysedp_work);
	}

	/* get modem operations from platform data */
	modem->ops = (const struct tegra_modem_operations *)pdata->ops;

	/* modem init */
	if (modem->ops && modem->ops->init) {
		ret = modem->ops->init();
		if (ret)
			goto error;
	}

	/* if wake gpio is not specified we rely on native usb remote wake */
	if (gpio_is_valid(pdata->wake_gpio)) {
		/* request remote wakeup irq from platform data */
		ret = mdm_request_irq(modem,
				      tegra_usb_modem_wake_thread,
				      pdata->wake_gpio,
				      pdata->wake_irq_flags,
				      "mdm_wake",
				      &modem->wake_irq,
				      &modem->wake_irq_wakeable);
		if (ret) {
			dev_err(&pdev->dev, "request wake irq error\n");
			goto error;
		}
	} else {
		modem->pdata->tegra_ehci_pdata->ops =
						&tegra_usb_modem_platform_ops;
	}

	if (gpio_is_valid(pdata->boot_gpio)) {
		/* request boot irq from platform data */
		ret = mdm_request_irq(modem,
				      tegra_usb_modem_boot_thread,
				      pdata->boot_gpio,
				      pdata->boot_irq_flags,
				      "mdm_boot",
				      &modem->boot_irq,
				      &modem->boot_irq_wakeable);
		if (ret) {
			dev_err(&pdev->dev, "request boot irq error\n");
			goto error;
		}
	} else
		dev_warn(&pdev->dev, "boot irq not specified\n");

	/* create sysfs node to load/unload host controller */
	ret = device_create_file(&pdev->dev, &dev_attr_load_host);
	if (ret) {
		dev_err(&pdev->dev, "can't create sysfs file\n");
		goto error;
	}
	modem->sysfs_file_created = 1;
	modem->capability = TEGRA_USB_HOST_RELOAD;
	mutex_init(&(modem->lock));
	mutex_init(&modem->hc_lock);
	wake_lock_init(&modem->wake_lock, WAKE_LOCK_SUSPEND, "mdm_lock");
	if (pdev->id >= 0)
		dev_set_name(&pdev->dev, "MDM%d", pdev->id);
	else
		dev_set_name(&pdev->dev, "MDM");

	/* create work queue platform_driver_registe */
	modem->wq = create_workqueue("tegra_usb_mdm_queue");
	INIT_DELAYED_WORK(&modem->recovery_work, tegra_usb_modem_recovery);
	INIT_WORK(&modem->host_load_work, tegra_usb_host_load);
	INIT_WORK(&modem->host_unload_work, tegra_usb_host_unload);
	INIT_WORK(&modem->cpu_boost_work, cpu_freq_boost);
	INIT_DELAYED_WORK(&modem->cpu_unboost_work, cpu_freq_unboost);

	pm_qos_add_request(&modem->cpu_boost_req, PM_QOS_CPU_FREQ_MIN,
			   PM_QOS_DEFAULT_VALUE);

	modem->pm_notifier.notifier_call = mdm_pm_notifier;
	modem->usb_notifier.notifier_call = mdm_usb_notifier;

	usb_register_notify(&modem->usb_notifier);
	register_pm_notifier(&modem->pm_notifier);

	/* start modem */
	if (modem->ops && modem->ops->start)
		modem->ops->start();

	return ret;
error:
	if (modem->sysfs_file_created)
		device_remove_file(&pdev->dev, &dev_attr_load_host);

	if (modem->wake_irq)
		free_irq(modem->wake_irq, modem);

	if (modem->boot_irq)
		free_irq(modem->boot_irq, modem);

	if (modem->sysedpc) {
		cancel_work_sync(&modem->sysedp_work);
		sysedp_free_consumer(modem->sysedpc);
	}

	if (modem->edp_initialized) {
		edp_attrs = edp_attributes;
		while ((attr = *edp_attrs++))
			device_remove_file(&pdev->dev, attr);
	}

	if (modem->edp_boot_client_registered)
		edp_unregister_client(modem->modem_boot_edp_client);

	if (!IS_ERR(modem->regulator))
		regulator_put(modem->regulator);

	return ret;
}

static int tegra_usb_modem_probe(struct platform_device *pdev)
{
	struct tegra_usb_modem_power_platform_data *pdata =
	    pdev->dev.platform_data;
	struct tegra_usb_modem *modem;
	int ret = 0;

	if (!pdata) {
		dev_dbg(&pdev->dev, "platform_data not available\n");
		return -EINVAL;
	}

	modem = kzalloc(sizeof(struct tegra_usb_modem), GFP_KERNEL);
	if (!modem) {
		dev_dbg(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	ret = mdm_init(modem, pdev);
	if (ret) {
		kfree(modem);
		return ret;
	}

	dev_set_drvdata(&pdev->dev, modem);

	return ret;
}

static int __exit tegra_usb_modem_remove(struct platform_device *pdev)
{
	struct tegra_usb_modem *modem = platform_get_drvdata(pdev);
	struct device_attribute **edp_attrs;
	struct device_attribute *attr;

	unregister_pm_notifier(&modem->pm_notifier);
	usb_unregister_notify(&modem->usb_notifier);

	if (modem->wake_irq)
		free_irq(modem->wake_irq, modem);

	if (modem->boot_irq)
		free_irq(modem->boot_irq, modem);

	if (modem->sysfs_file_created)
		device_remove_file(&pdev->dev, &dev_attr_load_host);

	if (modem->sysedpc) {
		cancel_work_sync(&modem->sysedp_work);
		sysedp_free_consumer(modem->sysedpc);
	}

	if (modem->edp_initialized) {
		cancel_work_sync(&modem->edp_work);

		edp_attrs = edp_attributes;
		while ((attr = *edp_attrs++))
			device_remove_file(&pdev->dev, attr);
	}

	if (modem->edp_boot_client_registered)
		edp_unregister_client(modem->modem_boot_edp_client);

	if (modem->edp_client_registered)
		edp_unregister_client(&modem->modem_edp_client);

	if (!IS_ERR(modem->regulator))
		regulator_put(modem->regulator);

	cancel_delayed_work_sync(&modem->recovery_work);
	cancel_work_sync(&modem->host_load_work);
	cancel_work_sync(&modem->host_unload_work);
	cancel_work_sync(&modem->cpu_boost_work);
	cancel_delayed_work_sync(&modem->cpu_unboost_work);
	destroy_workqueue(modem->wq);

	pm_qos_remove_request(&modem->cpu_boost_req);

	kfree(modem);
	return 0;
}

static void tegra_usb_modem_shutdown(struct platform_device *pdev)
{
	struct tegra_usb_modem *modem = platform_get_drvdata(pdev);

	if (modem->ops && modem->ops->stop)
		modem->ops->stop();
}

#ifdef CONFIG_PM
static int tegra_usb_modem_suspend(struct platform_device *pdev,
				   pm_message_t state)
{
	struct tegra_usb_modem *modem = platform_get_drvdata(pdev);
	int ret = 0;

	/* send L3 hint to modem */
	if (modem->ops && modem->ops->suspend)
		modem->ops->suspend();

	if (modem->wake_irq) {
		ret = enable_irq_wake(modem->wake_irq);
		if (ret) {
			pr_info("%s, wake irq=%d, error=%d\n",
				__func__, modem->wake_irq, ret);
			goto fail;
		}
	}

	if (modem->boot_irq) {
		ret = enable_irq_wake(modem->boot_irq);
		if (ret)
			pr_info("%s, wake irq=%d, error=%d\n",
				__func__, modem->boot_irq, ret);
	}
fail:
	return ret;
}

static int tegra_usb_modem_resume(struct platform_device *pdev)
{
	struct tegra_usb_modem *modem = platform_get_drvdata(pdev);
	int ret = 0;

	if (modem->boot_irq) {
		ret = disable_irq_wake(modem->boot_irq);
		if (ret)
			pr_err("Failed to disable modem boot_irq\n");
	}

	if (modem->wake_irq) {
		ret = disable_irq_wake(modem->wake_irq);
		if (ret)
			pr_err("Failed to disable modem wake_irq\n");
	}

	/* send L3->L0 hint to modem */
	if (modem->ops && modem->ops->resume)
		modem->ops->resume();
	return 0;
}
#endif

static struct platform_driver tegra_usb_modem_power_driver = {
	.driver = {
		   .name = "tegra_usb_modem_power",
		   .owner = THIS_MODULE,
		   },
	.probe = tegra_usb_modem_probe,
	.remove = __exit_p(tegra_usb_modem_remove),
	.shutdown = tegra_usb_modem_shutdown,
#ifdef CONFIG_PM
	.suspend = tegra_usb_modem_suspend,
	.resume = tegra_usb_modem_resume,
#endif
};

static int __init tegra_usb_modem_power_init(void)
{
	return platform_driver_register(&tegra_usb_modem_power_driver);
}

module_init(tegra_usb_modem_power_init);

static void __exit tegra_usb_modem_power_exit(void)
{
	platform_driver_unregister(&tegra_usb_modem_power_driver);
}

module_exit(tegra_usb_modem_power_exit);

MODULE_DESCRIPTION("Tegra usb modem power management driver");
MODULE_LICENSE("GPL");
