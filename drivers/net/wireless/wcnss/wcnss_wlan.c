/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/wcnss_wlan.h>
#include <linux/platform_data/qcom_wcnss_device.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <mach/peripheral-loader.h>
#include <mach/msm_smd.h>
#include <mach/msm_iomap.h>
#ifdef CONFIG_WCNSS_MEM_PRE_ALLOC
#include "wcnss_prealloc.h"
#endif

#define DEVICE "wcnss_wlan"
#define VERSION "1.01"
#define WCNSS_PIL_DEVICE "wcnss"

/* module params */
#define WCNSS_CONFIG_UNSPECIFIED (-1)

static int has_48mhz_xo = WCNSS_CONFIG_UNSPECIFIED;
module_param(has_48mhz_xo, int, S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(has_48mhz_xo, "Is an external 48 MHz XO present");

#define WCNSS_CTRL_CHANNEL			"WCNSS_CTRL"
#define WCNSS_MAX_FRAME_SIZE		500
#define WCNSS_VERSION_LEN			30

/* message types */
#define WCNSS_CTRL_MSG_START	0x01000000
#define	WCNSS_VERSION_REQ		(WCNSS_CTRL_MSG_START + 0)
#define	WCNSS_VERSION_RSP		(WCNSS_CTRL_MSG_START + 1)

#define VALID_VERSION(version) \
	((strncmp(version, "INVALID", WCNSS_VERSION_LEN)) ? 1 : 0)

struct smd_msg_hdr {
	unsigned int type;
	unsigned int len;
};

struct wcnss_version {
	struct smd_msg_hdr hdr;
	unsigned char  major;
	unsigned char  minor;
	unsigned char  version;
	unsigned char  revision;
};

static struct {
	struct platform_device *pdev;
	void		*pil;
	struct resource	*mmio_res;
	struct resource	*tx_irq_res;
	struct resource	*rx_irq_res;
	struct resource	*gpios_5wire;
	const struct dev_pm_ops *pm_ops;
	int		triggered;
	int		smd_channel_ready;
	smd_channel_t	*smd_ch;
	unsigned char	wcnss_version[WCNSS_VERSION_LEN];
	unsigned int	serial_number;
	int		thermal_mitigation;
	enum wcnss_hw_type	wcnss_hw_type;
	void		(*tm_notify)(struct device *, int);
	struct wcnss_wlan_config wlan_config;
	struct delayed_work wcnss_work;
	struct work_struct wcnssctrl_version_work;
	struct work_struct wcnssctrl_rx_work;
	struct wake_lock wcnss_wake_lock;
} *penv = NULL;

static ssize_t wcnss_serial_number_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (!penv)
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, "%08X\n", penv->serial_number);
}

static ssize_t wcnss_serial_number_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value;

	if (!penv)
		return -ENODEV;

	if (sscanf(buf, "%08X", &value) != 1)
		return -EINVAL;

	penv->serial_number = value;
	return count;
}

static DEVICE_ATTR(serial_number, S_IRUSR | S_IWUSR,
	wcnss_serial_number_show, wcnss_serial_number_store);


static ssize_t wcnss_thermal_mitigation_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (!penv)
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, "%u\n", penv->thermal_mitigation);
}

static ssize_t wcnss_thermal_mitigation_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int value;

	if (!penv)
		return -ENODEV;

	if (sscanf(buf, "%d", &value) != 1)
		return -EINVAL;
	penv->thermal_mitigation = value;
	if (penv->tm_notify)
		(penv->tm_notify)(dev, value);
	return count;
}

static DEVICE_ATTR(thermal_mitigation, S_IRUSR | S_IWUSR,
	wcnss_thermal_mitigation_show, wcnss_thermal_mitigation_store);


static ssize_t wcnss_version_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (!penv)
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, "%s", penv->wcnss_version);
}

static DEVICE_ATTR(wcnss_version, S_IRUSR,
		wcnss_version_show, NULL);

/* interface to reset Riva by sending the reset interrupt */
void wcnss_reset_intr(void)
{
	if (wcnss_hardware_type() == WCNSS_RIVA_HW) {
		wmb();
		__raw_writel(1 << 24, MSM_APCS_GCC_BASE + 0x8);
	} else {
		pr_err("%s: reset interrupt not supported\n", __func__);
	}
}
EXPORT_SYMBOL(wcnss_reset_intr);

static int wcnss_create_sysfs(struct device *dev)
{
	int ret;

	if (!dev)
		return -ENODEV;

	ret = device_create_file(dev, &dev_attr_serial_number);
	if (ret)
		return ret;

	ret = device_create_file(dev, &dev_attr_thermal_mitigation);
	if (ret)
		goto remove_serial;

	ret = device_create_file(dev, &dev_attr_wcnss_version);
	if (ret)
		goto remove_thermal;

	return 0;

remove_thermal:
	device_remove_file(dev, &dev_attr_thermal_mitigation);
remove_serial:
	device_remove_file(dev, &dev_attr_serial_number);

	return ret;
}

static void wcnss_remove_sysfs(struct device *dev)
{
	if (dev) {
		device_remove_file(dev, &dev_attr_serial_number);
		device_remove_file(dev, &dev_attr_thermal_mitigation);
		device_remove_file(dev, &dev_attr_wcnss_version);
	}
}
static void wcnss_smd_notify_event(void *data, unsigned int event)
{
	int len = 0;

	if (penv != data) {
		pr_err("wcnss: invalid env pointer in smd callback\n");
		return;
	}
	switch (event) {
	case SMD_EVENT_DATA:
		len = smd_read_avail(penv->smd_ch);
		if (len < 0)
			pr_err("wcnss: failed to read from smd %d\n", len);
		schedule_work(&penv->wcnssctrl_rx_work);
		break;

	case SMD_EVENT_OPEN:
		pr_debug("wcnss: opening WCNSS SMD channel :%s",
				WCNSS_CTRL_CHANNEL);
		if (!VALID_VERSION(penv->wcnss_version))
			schedule_work(&penv->wcnssctrl_version_work);
		break;

	case SMD_EVENT_CLOSE:
		pr_debug("wcnss: closing WCNSS SMD channel :%s",
				WCNSS_CTRL_CHANNEL);
		break;

	default:
		break;
	}
}

static void wcnss_post_bootup(struct work_struct *work)
{
	pr_info("%s: Cancel APPS vote for Iris & WCNSS\n", __func__);

	/* Since WCNSS is up, cancel any APPS vote for Iris & WCNSS VREGs  */
	wcnss_wlan_power(&penv->pdev->dev, &penv->wlan_config,
		WCNSS_WLAN_SWITCH_OFF);

}

static int
wcnss_pronto_gpios_config(struct device *dev, bool enable)
{
	int rc = 0;
	int i, j;
	int WCNSS_WLAN_NUM_GPIOS = 5;

	for (i = 0; i < WCNSS_WLAN_NUM_GPIOS; i++) {
		int gpio = of_get_gpio(dev->of_node, i);
		if (enable) {
			rc = gpio_request(gpio, "wcnss_wlan");
			if (rc) {
				pr_err("WCNSS gpio_request %d err %d\n",
					gpio, rc);
				goto fail;
			}
		} else
			gpio_free(gpio);
	}

	return rc;

fail:
	for (j = WCNSS_WLAN_NUM_GPIOS-1; j >= 0; j--) {
		int gpio = of_get_gpio(dev->of_node, i);
		gpio_free(gpio);
	}
	return rc;
}

static int
wcnss_gpios_config(struct resource *gpios_5wire, bool enable)
{
	int i, j;
	int rc = 0;

	for (i = gpios_5wire->start; i <= gpios_5wire->end; i++) {
		if (enable) {
			rc = gpio_request(i, gpios_5wire->name);
			if (rc) {
				pr_err("WCNSS gpio_request %d err %d\n", i, rc);
				goto fail;
			}
		} else
			gpio_free(i);
	}

	return rc;

fail:
	for (j = i-1; j >= gpios_5wire->start; j--)
		gpio_free(j);
	return rc;
}

static int __devinit
wcnss_wlan_ctrl_probe(struct platform_device *pdev)
{
	if (!penv)
		return -ENODEV;

	penv->smd_channel_ready = 1;

	pr_info("%s: SMD ctrl channel up\n", __func__);

	/* Schedule a work to do any post boot up activity */
	INIT_DELAYED_WORK(&penv->wcnss_work, wcnss_post_bootup);
	schedule_delayed_work(&penv->wcnss_work, msecs_to_jiffies(10000));

	return 0;
}

void wcnss_flush_delayed_boot_votes()
{
	flush_delayed_work_sync(&penv->wcnss_work);
}
EXPORT_SYMBOL(wcnss_flush_delayed_boot_votes);

static int __devexit
wcnss_wlan_ctrl_remove(struct platform_device *pdev)
{
	if (penv)
		penv->smd_channel_ready = 0;

	pr_info("%s: SMD ctrl channel down\n", __func__);

	return 0;
}


static struct platform_driver wcnss_wlan_ctrl_driver = {
	.driver = {
		.name	= "WLAN_CTRL",
		.owner	= THIS_MODULE,
	},
	.probe	= wcnss_wlan_ctrl_probe,
	.remove	= __devexit_p(wcnss_wlan_ctrl_remove),
};

static int __devexit
wcnss_ctrl_remove(struct platform_device *pdev)
{
	if (penv && penv->smd_ch)
		smd_close(penv->smd_ch);

	return 0;
}

static int __devinit
wcnss_ctrl_probe(struct platform_device *pdev)
{
	int ret = 0;

	if (!penv)
		return -ENODEV;

	ret = smd_named_open_on_edge(WCNSS_CTRL_CHANNEL, SMD_APPS_WCNSS,
			&penv->smd_ch, penv, wcnss_smd_notify_event);
	if (ret < 0) {
		pr_err("wcnss: cannot open the smd command channel %s: %d\n",
				WCNSS_CTRL_CHANNEL, ret);
		return -ENODEV;
	}
	smd_disable_read_intr(penv->smd_ch);

	return 0;
}

/* platform device for WCNSS_CTRL SMD channel */
static struct platform_driver wcnss_ctrl_driver = {
	.driver = {
		.name	= "WCNSS_CTRL",
		.owner	= THIS_MODULE,
	},
	.probe	= wcnss_ctrl_probe,
	.remove	= __devexit_p(wcnss_ctrl_remove),
};

struct device *wcnss_wlan_get_device(void)
{
	if (penv && penv->pdev && penv->smd_channel_ready)
		return &penv->pdev->dev;
	return NULL;
}
EXPORT_SYMBOL(wcnss_wlan_get_device);

struct platform_device *wcnss_get_platform_device(void)
{
	if (penv && penv->pdev)
		return penv->pdev;
	return NULL;
}
EXPORT_SYMBOL(wcnss_get_platform_device);

struct wcnss_wlan_config *wcnss_get_wlan_config(void)
{
	if (penv && penv->pdev)
		return &penv->wlan_config;
	return NULL;
}
EXPORT_SYMBOL(wcnss_get_wlan_config);

struct resource *wcnss_wlan_get_memory_map(struct device *dev)
{
	if (penv && dev && (dev == &penv->pdev->dev) && penv->smd_channel_ready)
		return penv->mmio_res;
	return NULL;
}
EXPORT_SYMBOL(wcnss_wlan_get_memory_map);

int wcnss_wlan_get_dxe_tx_irq(struct device *dev)
{
	if (penv && dev && (dev == &penv->pdev->dev) &&
				penv->tx_irq_res && penv->smd_channel_ready)
		return penv->tx_irq_res->start;
	return WCNSS_WLAN_IRQ_INVALID;
}
EXPORT_SYMBOL(wcnss_wlan_get_dxe_tx_irq);

int wcnss_wlan_get_dxe_rx_irq(struct device *dev)
{
	if (penv && dev && (dev == &penv->pdev->dev) &&
				penv->rx_irq_res && penv->smd_channel_ready)
		return penv->rx_irq_res->start;
	return WCNSS_WLAN_IRQ_INVALID;
}
EXPORT_SYMBOL(wcnss_wlan_get_dxe_rx_irq);

void wcnss_wlan_register_pm_ops(struct device *dev,
				const struct dev_pm_ops *pm_ops)
{
	if (penv && dev && (dev == &penv->pdev->dev) && pm_ops)
		penv->pm_ops = pm_ops;
}
EXPORT_SYMBOL(wcnss_wlan_register_pm_ops);

void wcnss_wlan_unregister_pm_ops(struct device *dev,
				const struct dev_pm_ops *pm_ops)
{
	if (penv && dev && (dev == &penv->pdev->dev) && pm_ops) {
		if (pm_ops->suspend != penv->pm_ops->suspend ||
				pm_ops->resume != penv->pm_ops->resume)
			pr_err("PM APIs dont match with registered APIs\n");
		penv->pm_ops = NULL;
	}
}
EXPORT_SYMBOL(wcnss_wlan_unregister_pm_ops);

void wcnss_register_thermal_mitigation(struct device *dev,
				void (*tm_notify)(struct device *, int))
{
	if (penv && dev && tm_notify)
		penv->tm_notify = tm_notify;
}
EXPORT_SYMBOL(wcnss_register_thermal_mitigation);

void wcnss_unregister_thermal_mitigation(
				void (*tm_notify)(struct device *, int))
{
	if (penv && tm_notify) {
		if (tm_notify != penv->tm_notify)
			pr_err("tm_notify doesn't match registered\n");
		penv->tm_notify = NULL;
	}
}
EXPORT_SYMBOL(wcnss_unregister_thermal_mitigation);

unsigned int wcnss_get_serial_number(void)
{
	if (penv)
		return penv->serial_number;
	return 0;
}
EXPORT_SYMBOL(wcnss_get_serial_number);

static int wcnss_wlan_suspend(struct device *dev)
{
	if (penv && dev && (dev == &penv->pdev->dev) &&
	    penv->smd_channel_ready &&
	    penv->pm_ops && penv->pm_ops->suspend)
		return penv->pm_ops->suspend(dev);
	return 0;
}

static int wcnss_wlan_resume(struct device *dev)
{
	if (penv && dev && (dev == &penv->pdev->dev) &&
	    penv->smd_channel_ready &&
	    penv->pm_ops && penv->pm_ops->resume)
		return penv->pm_ops->resume(dev);
	return 0;
}

void wcnss_prevent_suspend()
{
	if (penv)
		wake_lock(&penv->wcnss_wake_lock);
}
EXPORT_SYMBOL(wcnss_prevent_suspend);

void wcnss_allow_suspend()
{
	if (penv)
		wake_unlock(&penv->wcnss_wake_lock);
}
EXPORT_SYMBOL(wcnss_allow_suspend);

int wcnss_hardware_type(void)
{
	if (penv)
		return penv->wcnss_hw_type;
	else
		return -ENODEV;
}
EXPORT_SYMBOL(wcnss_hardware_type);

static int wcnss_smd_tx(void *data, int len)
{
	int ret = 0;

	ret = smd_write_avail(penv->smd_ch);
	if (ret < len) {
		pr_err("wcnss: no space available for smd frame\n");
		ret =  -ENOSPC;
	}
	ret = smd_write(penv->smd_ch, data, len);
	if (ret < len) {
		pr_err("wcnss: failed to write Command %d", len);
		ret = -ENODEV;
	}
	return ret;
}

static void wcnssctrl_rx_handler(struct work_struct *worker)
{
	int len = 0;
	int rc = 0;
	unsigned char buf[WCNSS_MAX_FRAME_SIZE];
	struct smd_msg_hdr *phdr;
	struct wcnss_version *pversion;

	len = smd_read_avail(penv->smd_ch);
	if (len > WCNSS_MAX_FRAME_SIZE) {
		pr_err("wcnss: frame larger than the allowed size\n");
		smd_read(penv->smd_ch, NULL, len);
		return;
	}
	if (len <= 0)
		return;

	rc = smd_read(penv->smd_ch, buf, len);
	if (rc < len) {
		pr_err("wcnss: incomplete data read from smd\n");
		return;
	}

	phdr = (struct smd_msg_hdr *)buf;

	switch (phdr->type) {

	case WCNSS_VERSION_RSP:
		pversion = (struct wcnss_version *)buf;
		if (len != sizeof(struct wcnss_version)) {
			pr_err("wcnss: invalid version data from wcnss %d\n",
				len);
			return;
		}
		snprintf(penv->wcnss_version, WCNSS_VERSION_LEN,
			"%02x%02x%02x%02x", pversion->major, pversion->minor,
					pversion->version, pversion->revision);
		pr_info("wcnss: version %s\n", penv->wcnss_version);
		break;

	default:
		pr_err("wcnss: invalid message type %d\n", phdr->type);
	}
	return;
}

static void wcnss_send_version_req(struct work_struct *worker)
{
	struct smd_msg_hdr smd_msg;
	int ret = 0;

	smd_msg.type = WCNSS_VERSION_REQ;
	smd_msg.len = sizeof(smd_msg);
	ret = wcnss_smd_tx(&smd_msg, smd_msg.len);
	if (ret < 0)
		pr_err("wcnss: smd tx failed\n");

	return;
}

static int
wcnss_trigger_config(struct platform_device *pdev)
{
	int ret;
	struct qcom_wcnss_opts *pdata;
	int has_pronto_hw = of_property_read_bool(pdev->dev.of_node,
									"qcom,has_pronto_hw");

	/* make sure we are only triggered once */
	if (penv->triggered)
		return 0;
	penv->triggered = 1;

	/* initialize the WCNSS device configuration */
	pdata = pdev->dev.platform_data;
	if (WCNSS_CONFIG_UNSPECIFIED == has_48mhz_xo) {
		if (has_pronto_hw) {
			has_48mhz_xo = of_property_read_bool(pdev->dev.of_node,
										"qcom,has_48mhz_xo");
			penv->wcnss_hw_type = WCNSS_PRONTO_HW;
		} else {
			penv->wcnss_hw_type = WCNSS_RIVA_HW;
			has_48mhz_xo = pdata->has_48mhz_xo;
		}
	}
	penv->wlan_config.use_48mhz_xo = has_48mhz_xo;

	penv->thermal_mitigation = 0;
	strlcpy(penv->wcnss_version, "INVALID", WCNSS_VERSION_LEN);

	/* Configure 5 wire GPIOs */
	if (!has_pronto_hw) {
		penv->gpios_5wire = platform_get_resource_byname(pdev,
					IORESOURCE_IO, "wcnss_gpios_5wire");

		/* allocate 5-wire GPIO resources */
		if (!penv->gpios_5wire) {
			dev_err(&pdev->dev, "insufficient IO resources\n");
			ret = -ENOENT;
			goto fail_gpio_res;
		}
		ret = wcnss_gpios_config(penv->gpios_5wire, true);
	} else
		ret = wcnss_pronto_gpios_config(&pdev->dev, true);

	if (ret) {
		dev_err(&pdev->dev, "WCNSS gpios config failed.\n");
		goto fail_gpio_res;
	}

	/* power up the WCNSS */
	ret = wcnss_wlan_power(&pdev->dev, &penv->wlan_config,
					WCNSS_WLAN_SWITCH_ON);
	if (ret) {
		dev_err(&pdev->dev, "WCNSS Power-up failed.\n");
		goto fail_power;
	}

	/* trigger initialization of the WCNSS */
	penv->pil = pil_get(WCNSS_PIL_DEVICE);
	if (IS_ERR(penv->pil)) {
		dev_err(&pdev->dev, "Peripheral Loader failed on WCNSS.\n");
		ret = PTR_ERR(penv->pil);
		penv->pil = NULL;
		goto fail_pil;
	}

	/* allocate resources */
	penv->mmio_res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"wcnss_mmio");
	penv->tx_irq_res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
							"wcnss_wlantx_irq");
	penv->rx_irq_res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
							"wcnss_wlanrx_irq");

	if (!(penv->mmio_res && penv->tx_irq_res && penv->rx_irq_res)) {
		dev_err(&pdev->dev, "insufficient resources\n");
		ret = -ENOENT;
		goto fail_res;
	}
	INIT_WORK(&penv->wcnssctrl_rx_work, wcnssctrl_rx_handler);
	INIT_WORK(&penv->wcnssctrl_version_work, wcnss_send_version_req);

	wake_lock_init(&penv->wcnss_wake_lock, WAKE_LOCK_SUSPEND, "wcnss");

	return 0;

fail_res:
	if (penv->pil)
		pil_put(penv->pil);
fail_pil:
	wcnss_wlan_power(&pdev->dev, &penv->wlan_config,
				WCNSS_WLAN_SWITCH_OFF);
fail_power:
	if (has_pronto_hw)
		ret = wcnss_pronto_gpios_config(&pdev->dev, false);
	else
		wcnss_gpios_config(penv->gpios_5wire, false);
fail_gpio_res:
	kfree(penv);
	penv = NULL;
	return ret;
}

#ifndef MODULE
static int wcnss_node_open(struct inode *inode, struct file *file)
{
	struct platform_device *pdev;

	pr_info(DEVICE " triggered by userspace\n");

	pdev = penv->pdev;
	return wcnss_trigger_config(pdev);
}

static const struct file_operations wcnss_node_fops = {
	.owner = THIS_MODULE,
	.open = wcnss_node_open,
};

static struct miscdevice wcnss_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE,
	.fops = &wcnss_node_fops,
};
#endif /* ifndef MODULE */


static int __devinit
wcnss_wlan_probe(struct platform_device *pdev)
{
	int ret = 0;

	/* verify we haven't been called more than once */
	if (penv) {
		dev_err(&pdev->dev, "cannot handle multiple devices.\n");
		return -ENODEV;
	}

	/* create an environment to track the device */
	penv = kzalloc(sizeof(*penv), GFP_KERNEL);
	if (!penv) {
		dev_err(&pdev->dev, "cannot allocate device memory.\n");
		return -ENOMEM;
	}
	penv->pdev = pdev;

	/* register sysfs entries */
	ret = wcnss_create_sysfs(&pdev->dev);
	if (ret)
		return -ENOENT;


#ifdef MODULE

	/*
	 * Since we were built as a module, we are running because
	 * the module was loaded, therefore we assume userspace
	 * applications are available to service PIL, so we can
	 * trigger the WCNSS configuration now
	 */
	pr_info(DEVICE " probed in MODULE mode\n");
	return wcnss_trigger_config(pdev);

#else

	/*
	 * Since we were built into the kernel we'll be called as part
	 * of kernel initialization.  We don't know if userspace
	 * applications are available to service PIL at this time
	 * (they probably are not), so we simply create a device node
	 * here.  When userspace is available it should touch the
	 * device so that we know that WCNSS configuration can take
	 * place
	 */
	pr_info(DEVICE " probed in built-in mode\n");
	return misc_register(&wcnss_misc);

#endif
}

static int __devexit
wcnss_wlan_remove(struct platform_device *pdev)
{
	wcnss_remove_sysfs(&pdev->dev);
	return 0;
}


static const struct dev_pm_ops wcnss_wlan_pm_ops = {
	.suspend	= wcnss_wlan_suspend,
	.resume		= wcnss_wlan_resume,
};

#ifdef CONFIG_WCNSS_CORE_PRONTO
static struct of_device_id msm_wcnss_pronto_match[] = {
	{.compatible = "qcom,wcnss_wlan"},
	{}
};
#endif

static struct platform_driver wcnss_wlan_driver = {
	.driver = {
		.name	= DEVICE,
		.owner	= THIS_MODULE,
		.pm	= &wcnss_wlan_pm_ops,
#ifdef CONFIG_WCNSS_CORE_PRONTO
		.of_match_table = msm_wcnss_pronto_match,
#endif
	},
	.probe	= wcnss_wlan_probe,
	.remove	= __devexit_p(wcnss_wlan_remove),
};

static int __init wcnss_wlan_init(void)
{
	int ret = 0;

	platform_driver_register(&wcnss_wlan_driver);
	platform_driver_register(&wcnss_wlan_ctrl_driver);
	platform_driver_register(&wcnss_ctrl_driver);

#ifdef CONFIG_WCNSS_MEM_PRE_ALLOC
	ret = wcnss_prealloc_init();
	if (ret < 0)
		pr_err("wcnss: pre-allocation failed\n");
#endif

	return ret;
}

static void __exit wcnss_wlan_exit(void)
{
	if (penv) {
		if (penv->pil)
			pil_put(penv->pil);


		kfree(penv);
		penv = NULL;
	}

	platform_driver_unregister(&wcnss_ctrl_driver);
	platform_driver_unregister(&wcnss_wlan_ctrl_driver);
	platform_driver_unregister(&wcnss_wlan_driver);
#ifdef CONFIG_WCNSS_MEM_PRE_ALLOC
	wcnss_prealloc_deinit();
#endif
}

module_init(wcnss_wlan_init);
module_exit(wcnss_wlan_exit);

MODULE_LICENSE("GPL v2");
MODULE_VERSION(VERSION);
MODULE_DESCRIPTION(DEVICE "Driver");
