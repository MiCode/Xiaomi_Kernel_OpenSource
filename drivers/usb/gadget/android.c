/*
 * Gadget Driver for Android
 *
 * Copyright (C) 2008 Google, Inc.
 *.Copyright (c) 2014, The Linux Foundation. All rights reserved.
 * Author: Mike Lockwood <lockwood@android.com>
 *         Benoit Goby <benoit@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/utsname.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/of.h>

#include <linux/usb/ch9.h>
#include <linux/usb/composite.h>
#include <linux/usb/gadget.h>
#include <linux/usb/android.h>

#include <linux/qcom/diag_dload.h>

#include "gadget_chips.h"

#include "f_fs.c"
#ifdef CONFIG_SND_PCM
#include "f_audio_source.c"
#endif
#include "f_mass_storage.c"
#define USB_ETH_RNDIS y
#include "f_diag.c"
#include "f_qdss.c"
#include "f_rmnet_smd.c"
#include "f_rmnet.c"
#include "f_gps.c"
#include "u_smd.c"
#include "u_bam.c"
#include "u_rmnet_ctrl_smd.c"
#include "u_rmnet_ctrl_qti.c"
#include "u_ctrl_hsic.c"
#include "u_data_hsic.c"
#include "u_ctrl_hsuart.c"
#include "u_data_hsuart.c"
#include "f_ccid.c"
#include "f_mtp.c"
#include "f_accessory.c"
#include "f_rndis.c"
#include "rndis.c"
#include "f_qc_ecm.c"
#include "f_mbim.c"
#include "f_qc_rndis.c"
#include "u_bam_data.c"
#include "f_ecm.c"
#include "u_ether.c"
#include "u_qc_ether.c"
#ifdef CONFIG_TARGET_CORE
#endif
#ifdef CONFIG_SND_PCM
#include "u_uac1.c"
#include "f_uac1.c"
#endif
#include "f_ncm.c"
#include "f_charger.c"

MODULE_AUTHOR("Mike Lockwood");
MODULE_DESCRIPTION("Android Composite USB Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

static const char longname[] = "Gadget Android";

/* Default vendor and product IDs, overridden by userspace */
#define VENDOR_ID		0x18D1
#define PRODUCT_ID		0x0001

#define ANDROID_DEVICE_NODE_NAME_LENGTH 11

struct android_usb_function {
	char *name;
	void *config;

	struct device *dev;
	char *dev_name;
	struct device_attribute **attributes;

	struct android_dev *android_dev;

	/* Optional: initialization during gadget bind */
	int (*init)(struct android_usb_function *, struct usb_composite_dev *);
	/* Optional: cleanup during gadget unbind */
	void (*cleanup)(struct android_usb_function *);
	/* Optional: called when the function is added the list of
	 *		enabled functions */
	void (*enable)(struct android_usb_function *);
	/* Optional: called when it is removed */
	void (*disable)(struct android_usb_function *);

	int (*bind_config)(struct android_usb_function *,
			   struct usb_configuration *);

	/* Optional: called when the configuration is removed */
	void (*unbind_config)(struct android_usb_function *,
			      struct usb_configuration *);
	/* Optional: handle ctrl requests before the device is configured */
	int (*ctrlrequest)(struct android_usb_function *,
					struct usb_composite_dev *,
					const struct usb_ctrlrequest *);
};

struct android_usb_function_holder {

	struct android_usb_function *f;

	/* for android_conf.enabled_functions */
	struct list_head enabled_list;
};

/**
* struct android_dev - represents android USB gadget device
* @name: device name.
* @functions: an array of all the supported USB function
*    drivers that this gadget support but not necessarily
*    added to one of the gadget configurations.
* @cdev: The internal composite device. Android gadget device
*    is a composite device, such that it can support configurations
*    with more than one function driver.
* @dev: The kernel device that represents this android device.
* @enabled: True if the android gadget is enabled, means all
*    the configurations were set and all function drivers were
*    bind and ready for USB enumeration.
* @disable_depth: Number of times the device was disabled, after
*    symmetrical number of enables the device willl be enabled.
*    Used for controlling ADB userspace disable/enable requests.
* @mutex: Internal mutex for protecting device member fields.
* @pdata: Platform data fetched from the kernel device platfrom data.
* @connected: True if got connect notification from the gadget UDC.
*    False if got disconnect notification from the gadget UDC.
* @sw_connected: Equal to 'connected' only after the connect
*    notification was handled by the android gadget work function.
* @suspended: True if got suspend notification from the gadget UDC.
*    False if got resume notification from the gadget UDC.
* @sw_suspended: Equal to 'suspended' only after the susped
*    notification was handled by the android gadget work function.
* @pm_qos: An attribute string that can be set by user space in order to
*    determine pm_qos policy. Set to 'high' for always demand pm_qos
*    when USB bus is connected and resumed. Set to 'low' for disable
*    any setting of pm_qos by this driver. Default = 'high'.
* @work: workqueue used for handling notifications from the gadget UDC.
* @configs: List of configurations currently configured into the device.
*    The android gadget supports more than one configuration. The host
*    may choose one configuration from the suggested.
* @configs_num: Number of configurations currently configured and existing
*    in the configs list.
* @list_item: This driver supports more than one android gadget device (for
*    example in order to support multiple USB cores), therefore this is
*    a item in a linked list of android devices.
*/
struct android_dev {
	const char *name;
	struct android_usb_function **functions;
	struct usb_composite_dev *cdev;
	struct device *dev;

	void (*setup_complete)(struct usb_ep *ep,
				struct usb_request *req);

	bool enabled;
	int disable_depth;
	struct mutex mutex;
	struct android_usb_platform_data *pdata;

	bool connected;
	bool sw_connected;
	bool suspended;
	bool sw_suspended;
	char pm_qos[5];
	struct pm_qos_request pm_qos_req_dma;
	unsigned up_pm_qos_sample_sec;
	unsigned up_pm_qos_threshold;
	unsigned down_pm_qos_sample_sec;
	unsigned down_pm_qos_threshold;
	unsigned idle_pc_rpm_no_int_secs;
	struct delayed_work pm_qos_work;
	enum android_pm_qos_state curr_pm_qos_state;
	struct work_struct work;
	char ffs_aliases[256];

	/* A list of struct android_configuration */
	struct list_head configs;
	int configs_num;

	/* A list node inside the android_dev_list */
	struct list_head list_item;
};

struct android_configuration {
	struct usb_configuration usb_config;

	/* A list of the functions supported by this config */
	struct list_head enabled_functions;

	/* A list node inside the struct android_dev.configs list */
	struct list_head list_item;
};

struct dload_struct __iomem *diag_dload;
static struct class *android_class;
static struct list_head android_dev_list;
static int android_dev_count;
static int android_bind_config(struct usb_configuration *c);
static void android_unbind_config(struct usb_configuration *c);
static struct android_dev *cdev_to_android_dev(struct usb_composite_dev *cdev);
static struct android_configuration *alloc_android_config
						(struct android_dev *dev);
static void free_android_config(struct android_dev *dev,
				struct android_configuration *conf);
static int usb_diag_update_pid_and_serial_num(uint32_t pid, const char *snum);

/* string IDs are assigned dynamically */
#define STRING_MANUFACTURER_IDX		0
#define STRING_PRODUCT_IDX		1
#define STRING_SERIAL_IDX		2

static char manufacturer_string[256];
static char product_string[256];
static char serial_string[256];

/* String Table */
static struct usb_string strings_dev[] = {
	[STRING_MANUFACTURER_IDX].s = manufacturer_string,
	[STRING_PRODUCT_IDX].s = product_string,
	[STRING_SERIAL_IDX].s = serial_string,
	{  }			/* end of list */
};

static struct usb_gadget_strings stringtab_dev = {
	.language	= 0x0409,	/* en-us */
	.strings	= strings_dev,
};

static struct usb_gadget_strings *dev_strings[] = {
	&stringtab_dev,
	NULL,
};

static struct usb_device_descriptor device_desc = {
	.bLength              = sizeof(device_desc),
	.bDescriptorType      = USB_DT_DEVICE,
	.bcdUSB               = __constant_cpu_to_le16(0x0200),
	.bDeviceClass         = USB_CLASS_PER_INTERFACE,
	.idVendor             = __constant_cpu_to_le16(VENDOR_ID),
	.idProduct            = __constant_cpu_to_le16(PRODUCT_ID),
	.bcdDevice            = __constant_cpu_to_le16(0xffff),
	.bNumConfigurations   = 1,
};

static struct usb_otg_descriptor otg_descriptor = {
	.bLength =		sizeof otg_descriptor,
	.bDescriptorType =	USB_DT_OTG,
	.bmAttributes =		USB_OTG_SRP | USB_OTG_HNP,
	.bcdOTG               = __constant_cpu_to_le16(0x0200),
};

static const struct usb_descriptor_header *otg_desc[] = {
	(struct usb_descriptor_header *) &otg_descriptor,
	NULL,
};

static const char *pm_qos_to_string(enum android_pm_qos_state state)
{
	switch (state) {
	case NO_USB_VOTE:	return "NO_USB_VOTE";
	case WFI:		return "WFI";
	case IDLE_PC:		return "IDLE_PC";
	case IDLE_PC_RPM:	return "IDLE_PC_RPM";
	default:		return "INVALID_STATE";
	}
}

static void android_pm_qos_update_latency(struct android_dev *dev, u32 latency)
{
	static int last_vote = -1;

	if (latency == last_vote || !latency)
		return;

	pr_debug("%s: latency updated to: %d\n", __func__, latency);

	pm_qos_update_request(&dev->pm_qos_req_dma, latency);

	last_vote = latency;
}

#define DOWN_PM_QOS_SAMPLE_SEC		5
#define DOWN_PM_QOS_THRESHOLD		100
#define UP_PM_QOS_SAMPLE_SEC		3
#define UP_PM_QOS_THRESHOLD		70
#define IDLE_PC_RPM_NO_INT_SECS		10

static void android_pm_qos_work(struct work_struct *data)
{
	struct android_dev *dev = container_of(data, struct android_dev,
							pm_qos_work.work);
	struct usb_gadget *gadget = dev->cdev->gadget;
	unsigned next_latency, curr_sample_int_count;
	unsigned next_sample_delay_sec;
	enum android_pm_qos_state next_state = dev->curr_pm_qos_state;
	static unsigned no_int_sample_count;

	curr_sample_int_count = gadget->xfer_isr_count;
	gadget->xfer_isr_count = 0;

	switch (dev->curr_pm_qos_state) {
	case WFI:
		if (curr_sample_int_count <= dev->down_pm_qos_threshold) {
			next_state = IDLE_PC;
			next_sample_delay_sec = dev->up_pm_qos_sample_sec;
			no_int_sample_count = 0;
		} else {
			next_sample_delay_sec = dev->down_pm_qos_sample_sec;
		}
		break;
	case IDLE_PC:
		if (!curr_sample_int_count)
			no_int_sample_count++;
		else
			no_int_sample_count = 0;

		if (curr_sample_int_count >= dev->up_pm_qos_threshold) {
			next_state = WFI;
			next_sample_delay_sec = dev->down_pm_qos_sample_sec;
		} else if (no_int_sample_count >=
		      dev->idle_pc_rpm_no_int_secs/dev->up_pm_qos_sample_sec) {
			next_state = IDLE_PC_RPM;
			next_sample_delay_sec = dev->up_pm_qos_sample_sec;
		} else {
			next_sample_delay_sec = dev->up_pm_qos_sample_sec;
		}
		break;
	case IDLE_PC_RPM:
		if (curr_sample_int_count) {
			next_state = WFI;
			next_sample_delay_sec = dev->down_pm_qos_sample_sec;
			no_int_sample_count = 0;
		} else {
			next_sample_delay_sec = 2 * dev->up_pm_qos_sample_sec;
		}
		break;
	default:
		pr_debug("invalid pm_qos_state (%u)\n", dev->curr_pm_qos_state);
		return;
	}

	if (next_state != dev->curr_pm_qos_state) {
		dev->curr_pm_qos_state = next_state;
		next_latency = dev->pdata->pm_qos_latency[next_state];
		android_pm_qos_update_latency(dev, next_latency);
		pr_debug("%s: pm_qos_state:%s, interrupts in last sample:%d\n",
				 __func__, pm_qos_to_string(next_state),
				curr_sample_int_count);
	}

	queue_delayed_work(system_nrt_wq, &dev->pm_qos_work,
			msecs_to_jiffies(1000*next_sample_delay_sec));
}

enum android_device_state {
	USB_DISCONNECTED,
	USB_CONNECTED,
	USB_CONFIGURED,
	USB_SUSPENDED,
	USB_RESUMED
};

static void android_work(struct work_struct *data)
{
	struct android_dev *dev = container_of(data, struct android_dev, work);
	struct usb_composite_dev *cdev = dev->cdev;
	struct android_usb_platform_data *pdata = dev->pdata;
	char *disconnected[2] = { "USB_STATE=DISCONNECTED", NULL };
	char *connected[2]    = { "USB_STATE=CONNECTED", NULL };
	char *configured[2]   = { "USB_STATE=CONFIGURED", NULL };
	char *suspended[2]   = { "USB_STATE=SUSPENDED", NULL };
	char *resumed[2]   = { "USB_STATE=RESUMED", NULL };
	char **uevent_envp = NULL;
	static enum android_device_state last_uevent, next_state;
	unsigned long flags;
	int pm_qos_vote = -1;

	spin_lock_irqsave(&cdev->lock, flags);
	if (dev->suspended != dev->sw_suspended && cdev->config) {
		if (strncmp(dev->pm_qos, "low", 3))
			pm_qos_vote = dev->suspended ? 0 : 1;
		next_state = dev->suspended ? USB_SUSPENDED : USB_RESUMED;
		uevent_envp = dev->suspended ? suspended : resumed;
	} else if (cdev->config) {
		uevent_envp = configured;
		next_state = USB_CONFIGURED;
	} else if (dev->connected != dev->sw_connected) {
		uevent_envp = dev->connected ? connected : disconnected;
		next_state = dev->connected ? USB_CONNECTED : USB_DISCONNECTED;
		if (dev->connected && strncmp(dev->pm_qos, "low", 3))
			pm_qos_vote = 1;
		else if (!dev->connected || !strncmp(dev->pm_qos, "low", 3))
			pm_qos_vote = 0;
	}
	dev->sw_connected = dev->connected;
	dev->sw_suspended = dev->suspended;
	spin_unlock_irqrestore(&cdev->lock, flags);

	if (pdata->pm_qos_latency[0] && pm_qos_vote == 1) {
		cancel_delayed_work_sync(&dev->pm_qos_work);
		android_pm_qos_update_latency(dev, pdata->pm_qos_latency[WFI]);
		dev->curr_pm_qos_state = WFI;
		queue_delayed_work(system_nrt_wq, &dev->pm_qos_work,
			    msecs_to_jiffies(1000*dev->down_pm_qos_sample_sec));
	} else if (pdata->pm_qos_latency[0] && pm_qos_vote == 0) {
		cancel_delayed_work_sync(&dev->pm_qos_work);
		android_pm_qos_update_latency(dev, PM_QOS_DEFAULT_VALUE);
		dev->curr_pm_qos_state = NO_USB_VOTE;
	}

	if (uevent_envp) {
		/*
		 * Some userspace modules, e.g. MTP, work correctly only if
		 * CONFIGURED uevent is preceded by DISCONNECT uevent.
		 * Check if we missed sending out a DISCONNECT uevent. This can
		 * happen if host PC resets and configures device really quick.
		 */
		if (((uevent_envp == connected) &&
		      (last_uevent != USB_DISCONNECTED)) ||
		    ((uevent_envp == configured) &&
		      (last_uevent == USB_CONFIGURED))) {
			pr_info("%s: sent missed DISCONNECT event\n", __func__);
			kobject_uevent_env(&dev->dev->kobj, KOBJ_CHANGE,
								disconnected);
			msleep(20);
		}
		/*
		 * Before sending out CONFIGURED uevent give function drivers
		 * a chance to wakeup userspace threads and notify disconnect
		 */
		if (uevent_envp == configured)
			msleep(50);

		/* Do not notify on suspend / resume */
		if (next_state != USB_SUSPENDED && next_state != USB_RESUMED) {
			kobject_uevent_env(&dev->dev->kobj, KOBJ_CHANGE,
					   uevent_envp);
			last_uevent = next_state;
		}
		pr_info("%s: sent uevent %s\n", __func__, uevent_envp[0]);
	} else {
		pr_info("%s: did not send uevent (%d %d %p)\n", __func__,
			 dev->connected, dev->sw_connected, cdev->config);
	}
}

static int android_enable(struct android_dev *dev)
{
	struct usb_composite_dev *cdev = dev->cdev;
	struct android_configuration *conf;
	int err = 0;

	if (WARN_ON(!dev->disable_depth))
		return err;

	if (--dev->disable_depth == 0) {

		list_for_each_entry(conf, &dev->configs, list_item) {
			err = usb_add_config(cdev, &conf->usb_config,
						android_bind_config);
			if (err < 0) {
				pr_err("%s: usb_add_config failed : err: %d\n",
						__func__, err);
				return err;
			}
		}
		usb_gadget_connect(cdev->gadget);
	}

	return err;
}

static void android_disable(struct android_dev *dev)
{
	struct usb_composite_dev *cdev = dev->cdev;
	struct android_configuration *conf;

	if (dev->disable_depth++ == 0) {
		usb_gadget_disconnect(cdev->gadget);
		/* Cancel pending control requests */
		usb_ep_dequeue(cdev->gadget->ep0, cdev->req);

		list_for_each_entry(conf, &dev->configs, list_item)
			usb_remove_config(cdev, &conf->usb_config);
	}
}

/*-------------------------------------------------------------------------*/
/* Supported functions initialization */

struct functionfs_config {
	bool opened;
	bool enabled;
	struct ffs_data *data;
	struct android_dev *dev;
};

static int ffs_function_init(struct android_usb_function *f,
			     struct usb_composite_dev *cdev)
{
	f->config = kzalloc(sizeof(struct functionfs_config), GFP_KERNEL);
	if (!f->config)
		return -ENOMEM;

	return functionfs_init();
}

static void ffs_function_cleanup(struct android_usb_function *f)
{
	functionfs_cleanup();
	kfree(f->config);
}

static void ffs_function_enable(struct android_usb_function *f)
{
	struct android_dev *dev = f->android_dev;
	struct functionfs_config *config = f->config;

	config->enabled = true;

	/* Disable the gadget until the function is ready */
	if (!config->opened)
		android_disable(dev);
}

static void ffs_function_disable(struct android_usb_function *f)
{
	struct android_dev *dev = f->android_dev;
	struct functionfs_config *config = f->config;

	config->enabled = false;

	/* Balance the disable that was called in closed_callback */
	if (!config->opened)
		android_enable(dev);
}

static int ffs_function_bind_config(struct android_usb_function *f,
				    struct usb_configuration *c)
{
	struct functionfs_config *config = f->config;
	return functionfs_bind_config(c->cdev, c, config->data);
}

static ssize_t
ffs_aliases_show(struct device *pdev, struct device_attribute *attr, char *buf)
{
	struct android_dev *dev;
	int ret;

	dev = list_first_entry(&android_dev_list, struct android_dev,
					list_item);

	mutex_lock(&dev->mutex);
	ret = sprintf(buf, "%s\n", dev->ffs_aliases);
	mutex_unlock(&dev->mutex);

	return ret;
}

static ssize_t
ffs_aliases_store(struct device *pdev, struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct android_dev *dev;
	char buff[256];

	dev = list_first_entry(&android_dev_list, struct android_dev,
					list_item);
	mutex_lock(&dev->mutex);

	if (dev->enabled) {
		mutex_unlock(&dev->mutex);
		return -EBUSY;
	}

	strlcpy(buff, buf, sizeof(buff));
	strlcpy(dev->ffs_aliases, strim(buff), sizeof(dev->ffs_aliases));

	mutex_unlock(&dev->mutex);

	return size;
}

static DEVICE_ATTR(aliases, S_IRUGO | S_IWUSR, ffs_aliases_show,
					       ffs_aliases_store);
static struct device_attribute *ffs_function_attributes[] = {
	&dev_attr_aliases,
	NULL
};

static struct android_usb_function ffs_function = {
	.name		= "ffs",
	.init		= ffs_function_init,
	.enable		= ffs_function_enable,
	.disable	= ffs_function_disable,
	.cleanup	= ffs_function_cleanup,
	.bind_config	= ffs_function_bind_config,
	.attributes	= ffs_function_attributes,
};

static int functionfs_ready_callback(struct ffs_data *ffs)
{
	struct android_dev *dev = ffs_function.android_dev;
	struct functionfs_config *config = ffs_function.config;
	int ret = 0;

	/* dev is null in case ADB is not in the composition */
	if (dev) {
		mutex_lock(&dev->mutex);
		ret = functionfs_bind(ffs, dev->cdev);
		if (ret) {
			mutex_unlock(&dev->mutex);
			return ret;
		}
	}

	config->data = ffs;
	config->opened = true;

	if (config->enabled && dev)
		android_enable(dev);

	if (dev)
		mutex_unlock(&dev->mutex);

	return 0;
}

static void functionfs_closed_callback(struct ffs_data *ffs)
{
	struct android_dev *dev = ffs_function.android_dev;
	struct functionfs_config *config = ffs_function.config;

	/* In case new composition is without ADB, use saved one */
	if (!dev)
		dev = config->dev;

	if (!dev)
		pr_err("adb_closed_callback: config->dev is NULL");

	if (dev)
		mutex_lock(&dev->mutex);

	config->opened = false;

	if (config->enabled && dev)
		android_disable(dev);

	config->dev = NULL;

	config->opened = false;
	config->data = NULL;

	functionfs_unbind(ffs);

	if (dev)
		mutex_unlock(&dev->mutex);
}

static void *functionfs_acquire_dev_callback(const char *dev_name)
{
	return 0;
}

static void functionfs_release_dev_callback(struct ffs_data *ffs_data)
{
}

/* ACM */
static char acm_transports[32];	/*enabled ACM ports - "tty[,sdio]"*/
#define MAX_ACM_INSTANCES 4
struct acm_function_config {
	int instances;
	int instances_on;
	struct usb_function *f_acm[MAX_ACM_INSTANCES];
	struct usb_function_instance *f_acm_inst[MAX_ACM_INSTANCES];
};

static int
acm_function_init(struct android_usb_function *f,
		struct usb_composite_dev *cdev)
{
	struct acm_function_config *config;

	config = kzalloc(sizeof(struct acm_function_config), GFP_KERNEL);
	if (!config)
		return -ENOMEM;
	f->config = config;

	return 0;
}

static void acm_function_cleanup(struct android_usb_function *f)
{
	int i;
	struct acm_function_config *config = f->config;

	acm_port_cleanup();
	for (i = 0; i < config->instances_on; i++) {
		usb_put_function(config->f_acm[i]);
		usb_put_function_instance(config->f_acm_inst[i]);
	}
	kfree(f->config);
	f->config = NULL;
}

static int
acm_function_bind_config(struct android_usb_function *f,
		struct usb_configuration *c)
{
	char *name;
	char buf[32], *b;
	int err = -1, i;
	static int acm_initialized, ports;
	struct acm_function_config *config = f->config;

	if (acm_initialized)
		goto bind_config;

	acm_initialized = 1;
	strlcpy(buf, acm_transports, sizeof(buf));
	b = strim(buf);

	while (b) {
		name = strsep(&b, ",");

		if (name) {
			err = acm_init_port(ports, name);
			if (err) {
				pr_err("acm: Cannot open port '%s'", name);
				goto out;
			}
			ports++;
			if (ports >= MAX_ACM_INSTANCES) {
				pr_err("acm: max ports reached '%s'", name);
				goto out;
			}
		}
	}
	err = acm_port_setup(c);
	if (err) {
		pr_err("acm: Cannot setup transports");
		goto out;
	}

	for (i = 0; i < ports; i++) {
		config->f_acm_inst[i] = usb_get_function_instance("acm");
		if (IS_ERR(config->f_acm_inst[i])) {
			err = PTR_ERR(config->f_acm_inst[i]);
			goto err_usb_get_function_instance;
		}
		config->f_acm[i] = usb_get_function(config->f_acm_inst[i]);
		if (IS_ERR(config->f_acm[i])) {
			err = PTR_ERR(config->f_acm[i]);
			goto err_usb_get_function;
		}
	}
	config->instances_on = ports;

bind_config:
	for (i = 0; i < ports; i++) {
		err = usb_add_function(c, config->f_acm[i]);
		if (err) {
			pr_err("Could not bind acm%u config\n", i);
			goto err_usb_add_function;
		}
	}

	return 0;

err_usb_add_function:
	while (i-- > 0)
		usb_remove_function(c, config->f_acm[i]);

	config->instances_on = 0;
	return err;

err_usb_get_function_instance:
	while (i-- > 0) {
		usb_put_function(config->f_acm[i]);
err_usb_get_function:
		usb_put_function_instance(config->f_acm_inst[i]);
	}

out:
	config->instances_on = 0;
	return err;
}

static void acm_function_unbind_config(struct android_usb_function *f,
				       struct usb_configuration *c)
{
	struct acm_function_config *config = f->config;
	config->instances_on = 0;
}

static ssize_t acm_transports_store(
		struct device *device, struct device_attribute *attr,
		const char *buff, size_t size)
{
	strlcpy(acm_transports, buff, sizeof(acm_transports));

	return size;
}

static DEVICE_ATTR(acm_transports, S_IWUSR, NULL, acm_transports_store);
static struct device_attribute *acm_function_attributes[] = {
	&dev_attr_acm_transports,
	NULL
};

static struct android_usb_function acm_function = {
	.name		= "acm",
	.init		= acm_function_init,
	.cleanup	= acm_function_cleanup,
	.bind_config	= acm_function_bind_config,
	.unbind_config	= acm_function_unbind_config,
	.attributes	= acm_function_attributes,
};

/* RMNET_SMD */
static int rmnet_smd_function_bind_config(struct android_usb_function *f,
					  struct usb_configuration *c)
{
	return rmnet_smd_bind_config(c);
}

static struct android_usb_function rmnet_smd_function = {
	.name		= "rmnet_smd",
	.bind_config	= rmnet_smd_function_bind_config,
};

/*rmnet transport string format(per port):"ctrl0,data0,ctrl1,data1..." */
#define MAX_XPORT_STR_LEN 50
static char rmnet_transports[MAX_XPORT_STR_LEN];

/*rmnet transport name string - "rmnet_hsic[,rmnet_hsusb]" */
static char rmnet_xport_names[MAX_XPORT_STR_LEN];

/*qdss transport string format(per port):"bam [, hsic]" */
static char qdss_transports[MAX_XPORT_STR_LEN];

/*qdss transport name string - "qdss_bam [, qdss_hsic]" */
static char qdss_xport_names[MAX_XPORT_STR_LEN];

/*qdss debug interface setting 0: disable   1:enable */
static bool qdss_debug_intf;

static void rmnet_function_cleanup(struct android_usb_function *f)
{
	frmnet_cleanup();
}

static int rmnet_function_bind_config(struct android_usb_function *f,
					 struct usb_configuration *c)
{
	int i;
	int err = 0;
	char *ctrl_name;
	char *data_name;
	char *tname = NULL;
	char buf[MAX_XPORT_STR_LEN], *b;
	char xport_name_buf[MAX_XPORT_STR_LEN], *tb;
	static int rmnet_initialized, ports;

	if (!rmnet_initialized) {
		rmnet_initialized = 1;
		strlcpy(buf, rmnet_transports, sizeof(buf));
		b = strim(buf);

		strlcpy(xport_name_buf, rmnet_xport_names,
				sizeof(xport_name_buf));
		tb = strim(xport_name_buf);

		while (b) {
			ctrl_name = strsep(&b, ",");
			data_name = strsep(&b, ",");
			if (ctrl_name && data_name) {
				if (tb)
					tname = strsep(&tb, ",");
				err = frmnet_init_port(ctrl_name, data_name,
						tname);
				if (err) {
					pr_err("rmnet: Cannot open ctrl port:"
						"'%s' data port:'%s'\n",
						ctrl_name, data_name);
					goto out;
				}
				ports++;
			}
		}

		err = rmnet_gport_setup();
		if (err) {
			pr_err("rmnet: Cannot setup transports");
			goto out;
		}
	}

	for (i = 0; i < ports; i++) {
		err = frmnet_bind_config(c, i);
		if (err) {
			pr_err("Could not bind rmnet%u config\n", i);
			break;
		}
	}
out:
	return err;
}

static void rmnet_function_unbind_config(struct android_usb_function *f,
						struct usb_configuration *c)
{
	frmnet_unbind_config();
}

static ssize_t rmnet_transports_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", rmnet_transports);
}

static ssize_t rmnet_transports_store(
		struct device *device, struct device_attribute *attr,
		const char *buff, size_t size)
{
	strlcpy(rmnet_transports, buff, sizeof(rmnet_transports));

	return size;
}

static ssize_t rmnet_xport_names_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", rmnet_xport_names);
}

static ssize_t rmnet_xport_names_store(
		struct device *device, struct device_attribute *attr,
		const char *buff, size_t size)
{
	strlcpy(rmnet_xport_names, buff, sizeof(rmnet_xport_names));

	return size;
}

static struct device_attribute dev_attr_rmnet_transports =
					__ATTR(transports, S_IRUGO | S_IWUSR,
						rmnet_transports_show,
						rmnet_transports_store);

static struct device_attribute dev_attr_rmnet_xport_names =
				__ATTR(transport_names, S_IRUGO | S_IWUSR,
				rmnet_xport_names_show,
				rmnet_xport_names_store);

static struct device_attribute *rmnet_function_attributes[] = {
					&dev_attr_rmnet_transports,
					&dev_attr_rmnet_xport_names,
					NULL };

static struct android_usb_function rmnet_function = {
	.name		= "rmnet",
	.cleanup	= rmnet_function_cleanup,
	.bind_config	= rmnet_function_bind_config,
	.unbind_config	= rmnet_function_unbind_config,
	.attributes	= rmnet_function_attributes,
};

static void gps_function_cleanup(struct android_usb_function *f)
{
	gps_cleanup();
}

static int gps_function_bind_config(struct android_usb_function *f,
					 struct usb_configuration *c)
{
	int err;
	static int gps_initialized;

	if (!gps_initialized) {
		gps_initialized = 1;
		err = gps_init_port();
		if (err) {
			pr_err("gps: Cannot init gps port");
			return err;
		}
	}

	err = gps_gport_setup();
	if (err) {
		pr_err("gps: Cannot setup transports");
		return err;
	}
	err = gps_bind_config(c);
	if (err) {
		pr_err("Could not bind gps config\n");
		return err;
	}

	return 0;
}

static struct android_usb_function gps_function = {
	.name		= "gps",
	.cleanup	= gps_function_cleanup,
	.bind_config	= gps_function_bind_config,
};

/* ncm */
struct ncm_function_config {
	u8      ethaddr[ETH_ALEN];
	struct eth_dev *dev;
};
static int
ncm_function_init(struct android_usb_function *f, struct usb_composite_dev *c)
{
	f->config = kzalloc(sizeof(struct ncm_function_config), GFP_KERNEL);
	if (!f->config)
		return -ENOMEM;

	return 0;
}

static void ncm_function_cleanup(struct android_usb_function *f)
{
	kfree(f->config);
	f->config = NULL;
}

static int
ncm_function_bind_config(struct android_usb_function *f,
				struct usb_configuration *c)
{
	struct ncm_function_config *ncm = f->config;
	int ret;
	struct eth_dev *dev;

	if (!ncm) {
		pr_err("%s: ncm config is null\n", __func__);
		return -EINVAL;
	}

	pr_info("%s MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", __func__,
		ncm->ethaddr[0], ncm->ethaddr[1], ncm->ethaddr[2],
		ncm->ethaddr[3], ncm->ethaddr[4], ncm->ethaddr[5]);

	dev = gether_setup_name(c->cdev->gadget, ncm->ethaddr, "ncm");
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		pr_err("%s: gether setup failed err:%d\n", __func__, ret);
		return ret;
	}
	ncm->dev = dev;

	ret = ncm_bind_config(c, ncm->ethaddr, dev);
	if (ret) {
		pr_err("%s: ncm bind config failed err:%d", __func__, ret);
		gether_cleanup(dev);
		return ret;
	}

	return ret;
}

static void ncm_function_unbind_config(struct android_usb_function *f,
						struct usb_configuration *c)
{
	struct ncm_function_config *ncm = f->config;
	gether_cleanup(ncm->dev);
}

static ssize_t ncm_ethaddr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct ncm_function_config *ncm = f->config;
	return snprintf(buf, PAGE_SIZE, "%02x:%02x:%02x:%02x:%02x:%02x\n",
		ncm->ethaddr[0], ncm->ethaddr[1], ncm->ethaddr[2],
		ncm->ethaddr[3], ncm->ethaddr[4], ncm->ethaddr[5]);
}

static ssize_t ncm_ethaddr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct ncm_function_config *ncm = f->config;

	if (sscanf(buf, "%02x:%02x:%02x:%02x:%02x:%02x\n",
		    (int *)&ncm->ethaddr[0], (int *)&ncm->ethaddr[1],
		    (int *)&ncm->ethaddr[2], (int *)&ncm->ethaddr[3],
		    (int *)&ncm->ethaddr[4], (int *)&ncm->ethaddr[5]) == 6)
		return size;
	return -EINVAL;
}

static DEVICE_ATTR(ncm_ethaddr, S_IRUGO | S_IWUSR, ncm_ethaddr_show,
					       ncm_ethaddr_store);
static struct device_attribute *ncm_function_attributes[] = {
	&dev_attr_ncm_ethaddr,
	NULL
};

static struct android_usb_function ncm_function = {
	.name		= "ncm",
	.init		= ncm_function_init,
	.cleanup	= ncm_function_cleanup,
	.bind_config	= ncm_function_bind_config,
	.unbind_config	= ncm_function_unbind_config,
	.attributes	= ncm_function_attributes,
};
/* ecm transport string */
static char ecm_transports[MAX_XPORT_STR_LEN];

struct ecm_function_config {
	u8      ethaddr[ETH_ALEN];
	struct eth_dev *dev;
};

static int ecm_function_init(struct android_usb_function *f,
				struct usb_composite_dev *cdev)
{
	f->config = kzalloc(sizeof(struct ecm_function_config), GFP_KERNEL);
	if (!f->config)
		return -ENOMEM;
	return 0;
}

static int ecm_qc_function_init(struct android_usb_function *f,
				struct usb_composite_dev *cdev)
{
	f->config = kzalloc(sizeof(struct ecm_function_config), GFP_KERNEL);
	if (!f->config)
		return -ENOMEM;
	return ecm_qc_init();
}
static void ecm_function_cleanup(struct android_usb_function *f)
{
	kfree(f->config);
	f->config = NULL;
}

static int ecm_qc_function_bind_config(struct android_usb_function *f,
					struct usb_configuration *c)
{
	int ret;
	char *trans;
	struct ecm_function_config *ecm = f->config;

	if (!ecm) {
		pr_err("%s: ecm_pdata\n", __func__);
		return -EINVAL;
	}

	pr_info("%s MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", __func__,
		ecm->ethaddr[0], ecm->ethaddr[1], ecm->ethaddr[2],
		ecm->ethaddr[3], ecm->ethaddr[4], ecm->ethaddr[5]);

	pr_debug("%s: ecm_transport is %s", __func__, ecm_transports);

	trans = strim(ecm_transports);
	if (strcmp("BAM2BAM_IPA", trans)) {
		ret = gether_qc_setup_name(c->cdev->gadget,
						ecm->ethaddr, "ecm");
		if (ret) {
			pr_err("%s: gether_setup failed\n", __func__);
			return ret;
		}
	}

	return ecm_qc_bind_config(c, ecm->ethaddr, trans);
}

static void ecm_qc_function_unbind_config(struct android_usb_function *f,
						struct usb_configuration *c)
{
	char *trans = strim(ecm_transports);

	if (strcmp("BAM2BAM_IPA", trans))
		gether_qc_cleanup_name("ecm0");
}

static ssize_t ecm_ethaddr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct ecm_function_config *ecm = f->config;
	return snprintf(buf, PAGE_SIZE, "%02x:%02x:%02x:%02x:%02x:%02x\n",
		ecm->ethaddr[0], ecm->ethaddr[1], ecm->ethaddr[2],
		ecm->ethaddr[3], ecm->ethaddr[4], ecm->ethaddr[5]);
}

static ssize_t ecm_ethaddr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct ecm_function_config *ecm = f->config;

	if (sscanf(buf, "%02x:%02x:%02x:%02x:%02x:%02x\n",
		    (int *)&ecm->ethaddr[0], (int *)&ecm->ethaddr[1],
		    (int *)&ecm->ethaddr[2], (int *)&ecm->ethaddr[3],
		    (int *)&ecm->ethaddr[4], (int *)&ecm->ethaddr[5]) == 6)
		return size;
	return -EINVAL;
}

static DEVICE_ATTR(ecm_ethaddr, S_IRUGO | S_IWUSR, ecm_ethaddr_show,
					       ecm_ethaddr_store);

static ssize_t ecm_transports_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", ecm_transports);
}

static ssize_t ecm_transports_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	strlcpy(ecm_transports, buf, sizeof(ecm_transports));
	return size;
}

static DEVICE_ATTR(ecm_transports, S_IRUGO | S_IWUSR, ecm_transports_show,
					       ecm_transports_store);

static struct device_attribute *ecm_function_attributes[] = {
	&dev_attr_ecm_transports,
	&dev_attr_ecm_ethaddr,
	NULL
};

static struct android_usb_function ecm_qc_function = {
	.name		= "ecm_qc",
	.init		= ecm_qc_function_init,
	.cleanup	= ecm_function_cleanup,
	.bind_config	= ecm_qc_function_bind_config,
	.unbind_config	= ecm_qc_function_unbind_config,
	.attributes	= ecm_function_attributes,
};

/* MBIM - used with BAM */
#define MAX_MBIM_INSTANCES 1

static int mbim_function_init(struct android_usb_function *f,
					 struct usb_composite_dev *cdev)
{
	return mbim_init(MAX_MBIM_INSTANCES);
}

static void mbim_function_cleanup(struct android_usb_function *f)
{
	fmbim_cleanup();
}


/* mbim transport string */
static char mbim_transports[MAX_XPORT_STR_LEN];

static int mbim_function_bind_config(struct android_usb_function *f,
					  struct usb_configuration *c)
{
	char *trans;

	pr_debug("%s: mbim transport is %s", __func__, mbim_transports);
	trans = strim(mbim_transports);
	return mbim_bind_config(c, 0, trans);
}

static int mbim_function_ctrlrequest(struct android_usb_function *f,
					struct usb_composite_dev *cdev,
					const struct usb_ctrlrequest *c)
{
	return mbim_ctrlrequest(cdev, c);
}

static ssize_t mbim_transports_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", mbim_transports);
}

static ssize_t mbim_transports_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	strlcpy(mbim_transports, buf, sizeof(mbim_transports));
	return size;
}

static DEVICE_ATTR(mbim_transports, S_IRUGO | S_IWUSR, mbim_transports_show,
				mbim_transports_store);

static ssize_t wMTU_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", ext_mbb_desc.wMTU);
}

static ssize_t wMTU_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int value;
	if (sscanf(buf, "%d", &value) == 1) {
		if (value < 0 || value > USHRT_MAX)
			pr_err("illegal MTU %d, enter unsigned 16 bits\n",
				value);
		else
			ext_mbb_desc.wMTU = cpu_to_le16(value);
		return size;
	}
	return -EINVAL;
}

static DEVICE_ATTR(wMTU, S_IRUGO | S_IWUSR, wMTU_show,
				wMTU_store);

static struct device_attribute *mbim_function_attributes[] = {
	&dev_attr_mbim_transports,
	&dev_attr_wMTU,
	NULL
};

static struct android_usb_function mbim_function = {
	.name		= "usb_mbim",
	.cleanup	= mbim_function_cleanup,
	.bind_config	= mbim_function_bind_config,
	.init		= mbim_function_init,
	.ctrlrequest	= mbim_function_ctrlrequest,
	.attributes		= mbim_function_attributes,
};

#ifdef CONFIG_SND_PCM
/* PERIPHERAL AUDIO */
static int audio_function_bind_config(struct android_usb_function *f,
					  struct usb_configuration *c)
{
	return audio_bind_config(c);
}

static struct android_usb_function audio_function = {
	.name		= "audio",
	.bind_config	= audio_function_bind_config,
};
#endif


/* DIAG */
static char diag_clients[32];	    /*enabled DIAG clients- "diag[,diag_mdm]" */
static ssize_t clients_store(
		struct device *device, struct device_attribute *attr,
		const char *buff, size_t size)
{
	strlcpy(diag_clients, buff, sizeof(diag_clients));

	return size;
}

static DEVICE_ATTR(clients, S_IWUSR, NULL, clients_store);
static struct device_attribute *diag_function_attributes[] =
					 { &dev_attr_clients, NULL };

static int diag_function_init(struct android_usb_function *f,
				 struct usb_composite_dev *cdev)
{
	return diag_setup();
}

static void diag_function_cleanup(struct android_usb_function *f)
{
	diag_cleanup();
}

static int diag_function_bind_config(struct android_usb_function *f,
					struct usb_configuration *c)
{
	char *name;
	char buf[32], *b;
	int once = 0, err = -1;
	int (*notify)(uint32_t, const char *);
	struct android_dev *dev = cdev_to_android_dev(c->cdev);

	strlcpy(buf, diag_clients, sizeof(buf));
	b = strim(buf);

	while (b) {
		notify = NULL;
		name = strsep(&b, ",");
		/* Allow only first diag channel to update pid and serial no */
		if (!once++) {
			if (dev->pdata && dev->pdata->update_pid_and_serial_num)
				notify = dev->pdata->update_pid_and_serial_num;
			else
				notify = usb_diag_update_pid_and_serial_num;
		}

		if (name) {
			err = diag_function_add(c, name, notify);
			if (err)
				pr_err("diag: Cannot open channel '%s'", name);
		}
	}

	return err;
}

static struct android_usb_function diag_function = {
	.name		= "diag",
	.init		= diag_function_init,
	.cleanup	= diag_function_cleanup,
	.bind_config	= diag_function_bind_config,
	.attributes	= diag_function_attributes,
};

/* DEBUG */
static int qdss_function_init(struct android_usb_function *f,
	struct usb_composite_dev *cdev)
{
	return qdss_setup();
}

static void qdss_function_cleanup(struct android_usb_function *f)
{
	qdss_cleanup();
}

static int qdss_init_transports(int *portnum)
{
	char *ts_port;
	char *tname = NULL;
	char buf[MAX_XPORT_STR_LEN], *type;
	char xport_name_buf[MAX_XPORT_STR_LEN], *tn;
	int err = 0;

	strlcpy(buf, qdss_transports, sizeof(buf));
	type = strim(buf);

	strlcpy(xport_name_buf, qdss_xport_names,
			sizeof(xport_name_buf));
	tn = strim(xport_name_buf);

	pr_debug("%s: qdss_debug_intf = %d\n",
		__func__, qdss_debug_intf);

	while (type) {
		ts_port = strsep(&type, ",");
		if (ts_port) {
			if (tn)
				tname = strsep(&tn, ",");

			err = qdss_init_port(
				ts_port,
				tname,
				qdss_debug_intf);

			if (err) {
				pr_err("%s: Cannot open transport port:'%s'\n",
					__func__, ts_port);
				return err;
			}
			(*portnum)++;
		}
	}
	return err;
}

static int qdss_function_bind_config(struct android_usb_function *f,
					struct usb_configuration *c)
{
	int i;
	int err = 0;
	static int qdss_initialized = 0, portsnum;

	if (!qdss_initialized) {
		qdss_initialized = 1;

		err = qdss_init_transports(&portsnum);
		if (err) {
			pr_err("qdss: Cannot init transports");
			goto out;
		}

		err = qdss_gport_setup();
		if (err) {
			pr_err("qdss: Cannot setup transports");
			goto out;
		}
	}

	pr_debug("%s: port number is %d\n", __func__, portsnum);

	for (i = 0; i < portsnum; i++) {
		err = qdss_bind_config(c, i);
		if (err) {
			pr_err("Could not bind qdss%u config\n", i);
			break;
		}
	}
out:
	return err;
}

static ssize_t qdss_transports_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", qdss_transports);
}

static ssize_t qdss_transports_store(
		struct device *device, struct device_attribute *attr,
		const char *buff, size_t size)
{
	strlcpy(qdss_transports, buff, sizeof(qdss_transports));

	return size;
}

static ssize_t qdss_xport_names_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", qdss_xport_names);
}

static ssize_t qdss_xport_names_store(
		struct device *device, struct device_attribute *attr,
		const char *buff, size_t size)
{
	strlcpy(qdss_xport_names, buff, sizeof(qdss_xport_names));
	return size;
}

static ssize_t qdss_debug_intf_store(
		struct device *device, struct device_attribute *attr,
		const char *buff, size_t size)
{
	strtobool(buff, &qdss_debug_intf);
	return size;
}

static ssize_t qdss_debug_intf_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", qdss_debug_intf);
}

static struct device_attribute dev_attr_qdss_transports =
					__ATTR(transports, S_IRUGO | S_IWUSR,
						qdss_transports_show,
						qdss_transports_store);

static struct device_attribute dev_attr_qdss_xport_names =
				__ATTR(transport_names, S_IRUGO | S_IWUSR,
				qdss_xport_names_show,
				qdss_xport_names_store);

/* 1(enable)/0(disable) the qdss debug interface */
static struct device_attribute dev_attr_qdss_debug_intf =
				__ATTR(debug_intf, S_IRUGO | S_IWUSR,
				qdss_debug_intf_show,
				qdss_debug_intf_store);

static struct device_attribute *qdss_function_attributes[] = {
					&dev_attr_qdss_transports,
					&dev_attr_qdss_xport_names,
					&dev_attr_qdss_debug_intf,
					NULL };

static struct android_usb_function qdss_function = {
	.name		= "qdss",
	.init		= qdss_function_init,
	.cleanup	= qdss_function_cleanup,
	.bind_config	= qdss_function_bind_config,
	.attributes	= qdss_function_attributes,
};

/* SERIAL */
#define MAX_SERIAL_INSTANCES 4
struct serial_function_config {
	int instances_on;
	struct usb_function *f_serial[MAX_SERIAL_INSTANCES];
	struct usb_function_instance *f_serial_inst[MAX_SERIAL_INSTANCES];
};

static char serial_transports[32];	/*enabled FSERIAL ports - "tty[,sdio]"*/
static ssize_t serial_transports_store(
		struct device *device, struct device_attribute *attr,
		const char *buff, size_t size)
{
	strlcpy(serial_transports, buff, sizeof(serial_transports));

	return size;
}

/*enabled FSERIAL transport names - "serial_hsic[,serial_hsusb]"*/
static char serial_xport_names[32];
static ssize_t serial_xport_names_store(
		struct device *device, struct device_attribute *attr,
		const char *buff, size_t size)
{
	strlcpy(serial_xport_names, buff, sizeof(serial_xport_names));

	return size;
}

static ssize_t serial_xport_names_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", serial_xport_names);
}
static ssize_t serial_modem_is_connected_show(
		struct device *device, struct device_attribute *attr,
		char *buff)
{
	unsigned int is_connected = gserial_is_connected();

	return snprintf(buff, PAGE_SIZE, "%u\n", is_connected);
}

static ssize_t dun_w_softap_enable_show(
		struct device *device, struct device_attribute *attr,
		char *buff)
{
	unsigned int dun_w_softap_enable = gserial_is_dun_w_softap_enabled();

	return snprintf(buff, PAGE_SIZE, "%u\n", dun_w_softap_enable);
}

static ssize_t dun_w_softap_enable_store(
		struct device *device, struct device_attribute *attr,
		const char *buff, size_t size)
{
	unsigned int dun_w_softap_enable;

	sscanf(buff, "%u", &dun_w_softap_enable);

	gserial_dun_w_softap_enable(dun_w_softap_enable);

	return size;
}

static ssize_t dun_w_softap_active_show(
		struct device *device, struct device_attribute *attr,
		char *buff)
{
	unsigned int dun_w_softap_active = gserial_is_dun_w_softap_active();

	return snprintf(buff, PAGE_SIZE, "%u\n", dun_w_softap_active);
}

static DEVICE_ATTR(is_connected_flag, S_IRUGO, serial_modem_is_connected_show,
		NULL);
static DEVICE_ATTR(dun_w_softap_enable, S_IRUGO | S_IWUSR,
	dun_w_softap_enable_show, dun_w_softap_enable_store);
static DEVICE_ATTR(dun_w_softap_active, S_IRUGO, dun_w_softap_active_show,
		NULL);


static DEVICE_ATTR(transports, S_IWUSR, NULL, serial_transports_store);
static struct device_attribute dev_attr_serial_xport_names =
				__ATTR(transport_names, S_IRUGO | S_IWUSR,
				serial_xport_names_show,
				serial_xport_names_store);

static struct device_attribute *serial_function_attributes[] = {
					&dev_attr_transports,
					&dev_attr_serial_xport_names,
					&dev_attr_is_connected_flag,
					&dev_attr_dun_w_softap_enable,
					&dev_attr_dun_w_softap_active,
					NULL };

static int serial_function_init(struct android_usb_function *f,
					struct usb_composite_dev *cdev)
{
	struct serial_function_config *config;

	config = kzalloc(sizeof(struct serial_function_config), GFP_KERNEL);
	if (!config)
		return -ENOMEM;

	f->config = config;

	return 0;
}

static void serial_function_cleanup(struct android_usb_function *f)
{
	int i;
	struct serial_function_config *config = f->config;

	gport_cleanup();
	for (i = 0; i < config->instances_on; i++) {
		usb_put_function(config->f_serial[i]);
		usb_put_function_instance(config->f_serial_inst[i]);
	}
	kfree(f->config);
	f->config = NULL;
}

static int serial_function_bind_config(struct android_usb_function *f,
					struct usb_configuration *c)
{
	char *name, *xport_name = NULL;
	char buf[32], *b, xport_name_buf[32], *tb;
	int err = -1, i;
	static int serial_initialized = 0, ports = 0;
	struct serial_function_config *config = f->config;

	if (serial_initialized)
		goto bind_config;

	serial_initialized = 1;
	strlcpy(buf, serial_transports, sizeof(buf));
	b = strim(buf);

	strlcpy(xport_name_buf, serial_xport_names, sizeof(xport_name_buf));
	tb = strim(xport_name_buf);

	while (b) {
		name = strsep(&b, ",");

		if (name) {
			if (tb)
				xport_name = strsep(&tb, ",");
			err = gserial_init_port(ports, name, xport_name);
			if (err) {
				pr_err("serial: Cannot open port '%s'", name);
				goto out;
			}
			ports++;
			if (ports >= MAX_SERIAL_INSTANCES) {
				pr_err("serial: max ports reached '%s'", name);
				goto out;
			}
		}
	}
	err = gport_setup(c);
	if (err) {
		pr_err("serial: Cannot setup transports");
		goto out;
	}

	for (i = 0; i < ports; i++) {
		config->f_serial_inst[i] = usb_get_function_instance("gser");
		if (IS_ERR(config->f_serial_inst[i])) {
			err = PTR_ERR(config->f_serial_inst[i]);
			goto err_gser_usb_get_function_instance;
		}
		config->f_serial[i] = usb_get_function(config->f_serial_inst[i]);
		if (IS_ERR(config->f_serial[i])) {
			err = PTR_ERR(config->f_serial[i]);
			goto err_gser_usb_get_function;
		}
	}
	config->instances_on = ports;

bind_config:
	for (i = 0; i < ports; i++) {
		err = usb_add_function(c, config->f_serial[i]);
		if (err) {
			pr_err("Could not bind gser%u config\n", i);
			goto err_gser_usb_add_function;
		}
	}
	return 0;

err_gser_usb_add_function:
	while (i-- > 0)
		usb_remove_function(c, config->f_serial[i]);

	return err;

err_gser_usb_get_function_instance:
	while (i-- > 0) {
		usb_put_function(config->f_serial[i]);
err_gser_usb_get_function:
		usb_put_function_instance(config->f_serial_inst[i]);
	}

out:
	return err;
}

static struct android_usb_function serial_function = {
	.name		= "serial",
	.init		= serial_function_init,
	.cleanup	= serial_function_cleanup,
	.bind_config	= serial_function_bind_config,
	.attributes	= serial_function_attributes,
};

/* CCID */
static int ccid_function_init(struct android_usb_function *f,
					struct usb_composite_dev *cdev)
{
	return ccid_setup();
}

static void ccid_function_cleanup(struct android_usb_function *f)
{
	ccid_cleanup();
}

static int ccid_function_bind_config(struct android_usb_function *f,
						struct usb_configuration *c)
{
	return ccid_bind_config(c);
}

static struct android_usb_function ccid_function = {
	.name		= "ccid",
	.init		= ccid_function_init,
	.cleanup	= ccid_function_cleanup,
	.bind_config	= ccid_function_bind_config,
};

/* Charger */
static int charger_function_bind_config(struct android_usb_function *f,
						struct usb_configuration *c)
{
	return charger_bind_config(c);
}

static struct android_usb_function charger_function = {
	.name		= "charging",
	.bind_config	= charger_function_bind_config,
};


static int
mtp_function_init(struct android_usb_function *f,
		struct usb_composite_dev *cdev)
{
	return mtp_setup();
}

static void mtp_function_cleanup(struct android_usb_function *f)
{
	mtp_cleanup();
}

static int
mtp_function_bind_config(struct android_usb_function *f,
		struct usb_configuration *c)
{
	return mtp_bind_config(c, false);
}

static int
ptp_function_init(struct android_usb_function *f,
		struct usb_composite_dev *cdev)
{
	/* nothing to do - initialization is handled by mtp_function_init */
	return 0;
}

static void ptp_function_cleanup(struct android_usb_function *f)
{
	/* nothing to do - cleanup is handled by mtp_function_cleanup */
}

static int
ptp_function_bind_config(struct android_usb_function *f,
		struct usb_configuration *c)
{
	return mtp_bind_config(c, true);
}

static int mtp_function_ctrlrequest(struct android_usb_function *f,
					struct usb_composite_dev *cdev,
					const struct usb_ctrlrequest *c)
{
	return mtp_ctrlrequest(cdev, c);
}

static struct android_usb_function mtp_function = {
	.name		= "mtp",
	.init		= mtp_function_init,
	.cleanup	= mtp_function_cleanup,
	.bind_config	= mtp_function_bind_config,
	.ctrlrequest	= mtp_function_ctrlrequest,
};

/* PTP function is same as MTP with slightly different interface descriptor */
static struct android_usb_function ptp_function = {
	.name		= "ptp",
	.init		= ptp_function_init,
	.cleanup	= ptp_function_cleanup,
	.bind_config	= ptp_function_bind_config,
};

/* rndis transport string */
static char rndis_transports[MAX_XPORT_STR_LEN];

struct rndis_function_config {
	u8      ethaddr[ETH_ALEN];
	u32     vendorID;
	u8      max_pkt_per_xfer;
	u8	pkt_alignment_factor;
	char	manufacturer[256];
	/* "Wireless" RNDIS; auto-detected by Windows */
	bool	wceis;
	struct eth_dev *dev;
};

static int
rndis_function_init(struct android_usb_function *f,
		struct usb_composite_dev *cdev)
{
	f->config = kzalloc(sizeof(struct rndis_function_config), GFP_KERNEL);
	if (!f->config)
		return -ENOMEM;
	return 0;
}

static void rndis_function_cleanup(struct android_usb_function *f)
{
	kfree(f->config);
	f->config = NULL;
}

static int rndis_qc_function_init(struct android_usb_function *f,
					struct usb_composite_dev *cdev)
{
	struct rndis_function_config *rndis;

	rndis = kzalloc(sizeof(struct rndis_function_config), GFP_KERNEL);
	if (!rndis)
		return -ENOMEM;

	rndis->wceis = true;

	f->config = rndis;
	return rndis_qc_init();
}

static void rndis_qc_function_cleanup(struct android_usb_function *f)
{
	rndis_qc_cleanup();
	kfree(f->config);
}

static int
rndis_function_bind_config(struct android_usb_function *f,
		struct usb_configuration *c)
{
	int ret;
	struct eth_dev *dev;
	struct rndis_function_config *rndis = f->config;

	if (!rndis) {
		pr_err("%s: rndis_pdata\n", __func__);
		return -1;
	}

	pr_info("%s MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", __func__,
		rndis->ethaddr[0], rndis->ethaddr[1], rndis->ethaddr[2],
		rndis->ethaddr[3], rndis->ethaddr[4], rndis->ethaddr[5]);

	if (rndis->ethaddr[0])
		dev = gether_setup_name(c->cdev->gadget, NULL, "rndis");
	else
		dev = gether_setup_name(c->cdev->gadget, rndis->ethaddr,
								"rndis");
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		pr_err("%s: gether_setup failed\n", __func__);
		return ret;
	}
	rndis->dev = dev;

	if (rndis->wceis) {
		/* "Wireless" RNDIS; auto-detected by Windows */
		rndis_iad_descriptor.bFunctionClass =
						USB_CLASS_WIRELESS_CONTROLLER;
		rndis_iad_descriptor.bFunctionSubClass = 0x01;
		rndis_iad_descriptor.bFunctionProtocol = 0x03;
		rndis_control_intf.bInterfaceClass =
						USB_CLASS_WIRELESS_CONTROLLER;
		rndis_control_intf.bInterfaceSubClass =	 0x01;
		rndis_control_intf.bInterfaceProtocol =	 0x03;
	}

	return rndis_bind_config_vendor(c, rndis->ethaddr, rndis->vendorID,
					   rndis->manufacturer, rndis->dev);
}

static int rndis_qc_function_bind_config(struct android_usb_function *f,
					struct usb_configuration *c)
{
	int ret;
	char *trans;
	struct rndis_function_config *rndis = f->config;

	if (!rndis) {
		pr_err("%s: rndis_pdata\n", __func__);
		return -EINVAL;
	}

	pr_info("%s MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", __func__,
		rndis->ethaddr[0], rndis->ethaddr[1], rndis->ethaddr[2],
		rndis->ethaddr[3], rndis->ethaddr[4], rndis->ethaddr[5]);

	pr_debug("%s: rndis_transport is %s", __func__, rndis_transports);

	trans = strim(rndis_transports);
	if (strcmp("BAM2BAM_IPA", trans)) {
		ret = gether_qc_setup_name(c->cdev->gadget,
					rndis->ethaddr, "rndis");
		if (ret) {
			pr_err("%s: gether_setup failed\n", __func__);
			return ret;
		}
	}

	if (rndis->wceis) {
		/* "Wireless" RNDIS; auto-detected by Windows */
		rndis_qc_iad_descriptor.bFunctionClass =
						USB_CLASS_WIRELESS_CONTROLLER;
		rndis_qc_iad_descriptor.bFunctionSubClass = 0x01;
		rndis_qc_iad_descriptor.bFunctionProtocol = 0x03;
		rndis_qc_control_intf.bInterfaceClass =
						USB_CLASS_WIRELESS_CONTROLLER;
		rndis_qc_control_intf.bInterfaceSubClass =	 0x01;
		rndis_qc_control_intf.bInterfaceProtocol =	 0x03;
	}

	return rndis_qc_bind_config_vendor(c, rndis->ethaddr, rndis->vendorID,
			rndis->manufacturer, rndis->max_pkt_per_xfer,
			rndis->pkt_alignment_factor, trans);
}

static void rndis_function_unbind_config(struct android_usb_function *f,
						struct usb_configuration *c)
{
	struct rndis_function_config *rndis = f->config;
	gether_cleanup(rndis->dev);
}

static void rndis_qc_function_unbind_config(struct android_usb_function *f,
						struct usb_configuration *c)
{
	char *trans = strim(rndis_transports);

	if (strcmp("BAM2BAM_IPA", trans))
		gether_qc_cleanup_name("rndis0");
}

static ssize_t rndis_manufacturer_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;

	return snprintf(buf, PAGE_SIZE, "%s\n", config->manufacturer);
}

static ssize_t rndis_manufacturer_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;

	if (size >= sizeof(config->manufacturer))
		return -EINVAL;

	if (sscanf(buf, "%255s", config->manufacturer) == 1)
		return size;
	return -1;
}

static DEVICE_ATTR(manufacturer, S_IRUGO | S_IWUSR, rndis_manufacturer_show,
						    rndis_manufacturer_store);

static ssize_t rndis_wceis_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;

	return snprintf(buf, PAGE_SIZE, "%d\n", config->wceis);
}

static ssize_t rndis_wceis_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;
	int value;

	if (sscanf(buf, "%d", &value) == 1) {
		config->wceis = value;
		return size;
	}
	return -EINVAL;
}

static DEVICE_ATTR(wceis, S_IRUGO | S_IWUSR, rndis_wceis_show,
					     rndis_wceis_store);

static ssize_t rndis_ethaddr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *rndis = f->config;

	return snprintf(buf, PAGE_SIZE, "%02x:%02x:%02x:%02x:%02x:%02x\n",
		rndis->ethaddr[0], rndis->ethaddr[1], rndis->ethaddr[2],
		rndis->ethaddr[3], rndis->ethaddr[4], rndis->ethaddr[5]);
}

static ssize_t rndis_ethaddr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *rndis = f->config;

	if (sscanf(buf, "%02x:%02x:%02x:%02x:%02x:%02x\n",
		    (int *)&rndis->ethaddr[0], (int *)&rndis->ethaddr[1],
		    (int *)&rndis->ethaddr[2], (int *)&rndis->ethaddr[3],
		    (int *)&rndis->ethaddr[4], (int *)&rndis->ethaddr[5]) == 6)
		return size;
	return -EINVAL;
}

static DEVICE_ATTR(ethaddr, S_IRUGO | S_IWUSR, rndis_ethaddr_show,
					       rndis_ethaddr_store);

static ssize_t rndis_vendorID_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;

	return snprintf(buf, PAGE_SIZE, "%04x\n", config->vendorID);
}

static ssize_t rndis_vendorID_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;
	int value;

	if (sscanf(buf, "%04x", &value) == 1) {
		config->vendorID = value;
		return size;
	}
	return -EINVAL;
}

static DEVICE_ATTR(vendorID, S_IRUGO | S_IWUSR, rndis_vendorID_show,
						rndis_vendorID_store);

static ssize_t rndis_max_pkt_per_xfer_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;
	return snprintf(buf, PAGE_SIZE, "%d\n", config->max_pkt_per_xfer);
}

static ssize_t rndis_max_pkt_per_xfer_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;
	int value;

	if (sscanf(buf, "%d", &value) == 1) {
		config->max_pkt_per_xfer = value;
		return size;
	}
	return -EINVAL;
}

static DEVICE_ATTR(max_pkt_per_xfer, S_IRUGO | S_IWUSR,
				   rndis_max_pkt_per_xfer_show,
				   rndis_max_pkt_per_xfer_store);
static ssize_t rndis_transports_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", rndis_transports);
}

static ssize_t rndis_transports_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	strlcpy(rndis_transports, buf, sizeof(rndis_transports));
	return size;
}

static DEVICE_ATTR(rndis_transports, S_IRUGO | S_IWUSR, rndis_transports_show,
					       rndis_transports_store);

static ssize_t rndis_rx_trigger_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int value;

	if (sscanf(buf, "%d", &value) == 1) {
		rndis_rx_trigger();
		return size;
	}
	return -EINVAL;
}

static DEVICE_ATTR(rx_trigger, S_IWUSR, NULL,
					     rndis_rx_trigger_store);

static ssize_t rndis_pkt_alignment_factor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;

	return snprintf(buf, PAGE_SIZE, "%d\n", config->pkt_alignment_factor);
}

static ssize_t rndis_pkt_alignment_factor_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;
	int value;

	if (sscanf(buf, "%d", &value) == 1) {
		config->pkt_alignment_factor = value;
		return size;
	}

	return -EINVAL;
}

static DEVICE_ATTR(pkt_alignment_factor, S_IRUGO | S_IWUSR,
					rndis_pkt_alignment_factor_show,
					rndis_pkt_alignment_factor_store);

static struct device_attribute *rndis_function_attributes[] = {
	&dev_attr_manufacturer,
	&dev_attr_wceis,
	&dev_attr_ethaddr,
	&dev_attr_vendorID,
	&dev_attr_max_pkt_per_xfer,
	&dev_attr_rndis_transports,
	&dev_attr_rx_trigger,
	&dev_attr_pkt_alignment_factor,
	NULL
};

static struct android_usb_function rndis_function = {
	.name		= "rndis",
	.init		= rndis_function_init,
	.cleanup	= rndis_function_cleanup,
	.bind_config	= rndis_function_bind_config,
	.unbind_config	= rndis_function_unbind_config,
	.attributes	= rndis_function_attributes,
};

static struct android_usb_function rndis_qc_function = {
	.name		= "rndis_qc",
	.init		= rndis_qc_function_init,
	.cleanup	= rndis_qc_function_cleanup,
	.bind_config	= rndis_qc_function_bind_config,
	.unbind_config	= rndis_qc_function_unbind_config,
	.attributes	= rndis_function_attributes,
};

static int ecm_function_bind_config(struct android_usb_function *f,
					struct usb_configuration *c)
{
	int ret;
	struct eth_dev *dev;
	struct ecm_function_config *ecm = f->config;

	if (!ecm) {
		pr_err("%s: ecm_pdata\n", __func__);
		return -EINVAL;
	}

	pr_info("%s MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", __func__,
		ecm->ethaddr[0], ecm->ethaddr[1], ecm->ethaddr[2],
		ecm->ethaddr[3], ecm->ethaddr[4], ecm->ethaddr[5]);

	dev = gether_setup_name(c->cdev->gadget, ecm->ethaddr, "ecm");
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		pr_err("%s: gether_setup failed\n", __func__);
		return ret;
	}
	ecm->dev = dev;

	ret = ecm_bind_config(c, ecm->ethaddr, dev);
	if (ret) {
		pr_err("%s: ecm_bind_config failed\n", __func__);
		gether_cleanup(dev);
	}
	return ret;
}

static void ecm_function_unbind_config(struct android_usb_function *f,
						struct usb_configuration *c)
{
	struct ecm_function_config *ecm = f->config;
	gether_cleanup(ecm->dev);
}

static struct android_usb_function ecm_function = {
	.name		= "ecm",
	.init		= ecm_function_init,
	.cleanup	= ecm_function_cleanup,
	.bind_config	= ecm_function_bind_config,
	.unbind_config	= ecm_function_unbind_config,
	.attributes	= ecm_function_attributes,
};

#define MAX_LUN_STR_LEN 25
static char lun_info[MAX_LUN_STR_LEN] = {'\0'};
struct mass_storage_function_config {
	struct fsg_config fsg;
	struct fsg_common *common;
};

#define MAX_LUN_NAME 8
static int mass_storage_function_init(struct android_usb_function *f,
					struct usb_composite_dev *cdev)
{
	struct android_dev *dev = cdev_to_android_dev(cdev);
	struct mass_storage_function_config *config;
	struct fsg_common *common;
	int err;
	int i, n;
	char name[FSG_MAX_LUNS][MAX_LUN_NAME];
	u8 uicc_nluns = dev->pdata ? dev->pdata->uicc_nluns : 0;

	config = kzalloc(sizeof(struct mass_storage_function_config),
							GFP_KERNEL);
	if (!config) {
		pr_err("Memory allocation failed.\n");
		return -ENOMEM;
	}

	config->fsg.nluns = 1;
	snprintf(name[0], MAX_LUN_NAME, "lun");
	config->fsg.luns[0].removable = 1;

	if (dev->pdata && dev->pdata->cdrom) {
		config->fsg.luns[config->fsg.nluns].cdrom = 1;
		config->fsg.luns[config->fsg.nluns].ro = 1;
		config->fsg.luns[config->fsg.nluns].removable = 0;
		snprintf(name[config->fsg.nluns], MAX_LUN_NAME, "rom");
		config->fsg.nluns++;
	}

	if (uicc_nluns > FSG_MAX_LUNS - config->fsg.nluns) {
		uicc_nluns = FSG_MAX_LUNS - config->fsg.nluns;
		pr_debug("limiting uicc luns to %d\n", uicc_nluns);
	}

	for (i = 0; i < uicc_nluns; i++) {
		n = config->fsg.nluns;
		snprintf(name[n], MAX_LUN_NAME, "uicc%d", i);
		config->fsg.luns[n].removable = 1;
		config->fsg.nluns++;
	}

	common = fsg_common_init(NULL, cdev, &config->fsg);
	if (IS_ERR(common)) {
		kfree(config);
		return PTR_ERR(common);
	}

	for (i = 0; i < config->fsg.nluns; i++) {
		err = sysfs_create_link(&f->dev->kobj,
					&common->luns[i].dev.kobj,
					name[i]);
		if (err)
			goto error;
	}

	config->common = common;
	f->config = config;
	return 0;
error:
	for (; i > 0; i--)
		sysfs_remove_link(&f->dev->kobj, name[i-1]);

	fsg_common_release(&common->ref);
	kfree(config);
	return err;
}

static int mass_storage_lun_init(struct android_usb_function *f,
						char *lun_info)
{
	struct mass_storage_function_config *config = f->config;
	bool inc_lun = true;
	int i = config->fsg.nluns, index, length;
	static int number_of_luns;

	length = strlen(lun_info);
	if (!length) {
		pr_err("LUN_INFO is null.\n");
		return -EINVAL;
	}

	index = i + number_of_luns;
	if (index >= FSG_MAX_LUNS) {
		pr_err("Number of LUNs exceed the limit.\n");
		return -EINVAL;
	}

	if (!strcmp(lun_info, "disk")) {
		config->fsg.luns[index].removable = 1;
	} else if (!strcmp(lun_info, "rom")) {
		config->fsg.luns[index].cdrom = 1;
		config->fsg.luns[index].removable = 0;
		config->fsg.luns[index].ro = 1;
	} else {
		pr_err("Invalid LUN info.\n");
		inc_lun = false;
		return -EINVAL;
	}

	if (inc_lun)
		number_of_luns++;

	pr_debug("number_of_luns:%d\n", number_of_luns);
	return number_of_luns;
}

static void mass_storage_function_cleanup(struct android_usb_function *f)
{
	kfree(f->config);
	f->config = NULL;
}

static void mass_storage_function_enable(struct android_usb_function *f)
{
	struct usb_composite_dev *cdev = f->android_dev->cdev;
	struct mass_storage_function_config *config = f->config;
	struct fsg_common *common = config->common;
	char *lun_type;
	int i, err, prev_nluns;
	char buf[MAX_LUN_STR_LEN], *b;
	int number_of_luns = 0;
	char buf1[5];
	char *lun_name = buf1;
	static int msc_initialized;

	if (msc_initialized)
		return;

	prev_nluns = config->fsg.nluns;

	if (lun_info[0] != '\0') {
		strlcpy(buf, lun_info, sizeof(buf));
		b = strim(buf);

		while (b) {
			lun_type = strsep(&b, ",");
			if (lun_type)
				number_of_luns =
					mass_storage_lun_init(f, lun_type);
				if (number_of_luns <= 0)
					return;
		}
	} else {
		pr_debug("No extra msc lun required.\n");
		return;
	}

	err = fsg_add_lun(common, cdev, &config->fsg, number_of_luns);
	if (err) {
		pr_err("Failed adding LUN.\n");
		return;
	}

	pr_debug("fsg.nluns:%d\n", config->fsg.nluns);
	for (i = prev_nluns; i < config->fsg.nluns; i++) {
		snprintf(lun_name, sizeof(buf), "lun%d", (i-prev_nluns));
		pr_debug("sysfs: LUN name:%s\n", lun_name);
		err = sysfs_create_link(&f->dev->kobj,
			&common->luns[i].dev.kobj, lun_name);
		if (err)
			pr_err("sysfs file creation failed: lun%d err:%d\n",
							(i-prev_nluns), err);
	}

	msc_initialized = 1;
}

static int mass_storage_function_bind_config(struct android_usb_function *f,
						struct usb_configuration *c)
{
	struct mass_storage_function_config *config = f->config;
	int ret;

	ret = fsg_bind_config(c->cdev, c, config->common);
	if (ret)
		pr_err("fsg_bind_config failed. ret:%x\n", ret);

	return ret;
}

static ssize_t mass_storage_inquiry_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct mass_storage_function_config *config = f->config;
	return snprintf(buf, PAGE_SIZE, "%s\n", config->common->inquiry_string);
}

static ssize_t mass_storage_inquiry_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct mass_storage_function_config *config = f->config;
	if (size >= sizeof(config->common->inquiry_string))
		return -EINVAL;
	if (sscanf(buf, "%28s", config->common->inquiry_string) != 1)
		return -EINVAL;
	return size;
}

static DEVICE_ATTR(inquiry_string, S_IRUGO | S_IWUSR,
					mass_storage_inquiry_show,
					mass_storage_inquiry_store);

static ssize_t mass_storage_lun_info_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{

	return snprintf(buf, PAGE_SIZE, "%s\n", lun_info);
}

static ssize_t mass_storage_lun_info_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	strlcpy(lun_info, buf, sizeof(lun_info));
	return size;
}

static DEVICE_ATTR(luns, S_IRUGO | S_IWUSR,
				mass_storage_lun_info_show,
				mass_storage_lun_info_store);

static struct device_attribute *mass_storage_function_attributes[] = {
	&dev_attr_inquiry_string,
	&dev_attr_luns,
	NULL
};

static struct android_usb_function mass_storage_function = {
	.name		= "mass_storage",
	.init		= mass_storage_function_init,
	.cleanup	= mass_storage_function_cleanup,
	.bind_config	= mass_storage_function_bind_config,
	.attributes	= mass_storage_function_attributes,
	.enable		= mass_storage_function_enable,
};


static int accessory_function_init(struct android_usb_function *f,
					struct usb_composite_dev *cdev)
{
	return acc_setup();
}

static void accessory_function_cleanup(struct android_usb_function *f)
{
	acc_cleanup();
}

static int accessory_function_bind_config(struct android_usb_function *f,
						struct usb_configuration *c)
{
	return acc_bind_config(c);
}

static int accessory_function_ctrlrequest(struct android_usb_function *f,
						struct usb_composite_dev *cdev,
						const struct usb_ctrlrequest *c)
{
	return acc_ctrlrequest(cdev, c);
}

static struct android_usb_function accessory_function = {
	.name		= "accessory",
	.init		= accessory_function_init,
	.cleanup	= accessory_function_cleanup,
	.bind_config	= accessory_function_bind_config,
	.ctrlrequest	= accessory_function_ctrlrequest,
};

#ifdef CONFIG_SND_PCM
static int audio_source_function_init(struct android_usb_function *f,
			struct usb_composite_dev *cdev)
{
	struct audio_source_config *config;

	config = kzalloc(sizeof(struct audio_source_config), GFP_KERNEL);
	if (!config)
		return -ENOMEM;
	config->card = -1;
	config->device = -1;
	f->config = config;
	return 0;
}

static void audio_source_function_cleanup(struct android_usb_function *f)
{
	kfree(f->config);
}

static int audio_source_function_bind_config(struct android_usb_function *f,
						struct usb_configuration *c)
{
	struct audio_source_config *config = f->config;

	return audio_source_bind_config(c, config);
}

static void audio_source_function_unbind_config(struct android_usb_function *f,
						struct usb_configuration *c)
{
	struct audio_source_config *config = f->config;

	config->card = -1;
	config->device = -1;
}

static ssize_t audio_source_pcm_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct audio_source_config *config = f->config;

	/* print PCM card and device numbers */
	return sprintf(buf, "%d %d\n", config->card, config->device);
}

static DEVICE_ATTR(pcm, S_IRUGO, audio_source_pcm_show, NULL);

static struct device_attribute *audio_source_function_attributes[] = {
	&dev_attr_pcm,
	NULL
};

static struct android_usb_function audio_source_function = {
	.name		= "audio_source",
	.init		= audio_source_function_init,
	.cleanup	= audio_source_function_cleanup,
	.bind_config	= audio_source_function_bind_config,
	.unbind_config	= audio_source_function_unbind_config,
	.attributes	= audio_source_function_attributes,
};
#endif

static int android_uasp_connect_cb(bool connect)
{
	/*
	 * TODO
	 * We may have to disable gadget till UASP configfs nodes
	 * are configured which includes mapping LUN with the
	 * backing file. It is a fundamental difference between
	 * f_mass_storage and f_tcp. That means UASP can not be
	 * in default composition.
	 *
	 * For now, assume that UASP configfs nodes are configured
	 * before enabling android gadget. Or cable should be
	 * reconnected after mapping the LUN.
	 *
	 * Also consider making UASP to respond to Host requests when
	 * Lun is not mapped.
	 */
	pr_debug("UASP %s\n", connect ? "connect" : "disconnect");

	return 0;
}

static int uasp_function_init(struct android_usb_function *f,
					struct usb_composite_dev *cdev)
{
	return f_tcm_init(&android_uasp_connect_cb);
}

static void uasp_function_cleanup(struct android_usb_function *f)
{
	f_tcm_exit();
}

static int uasp_function_bind_config(struct android_usb_function *f,
						struct usb_configuration *c)
{
	return tcm_bind_config(c);
}

static struct android_usb_function uasp_function = {
	.name		= "uasp",
	.init		= uasp_function_init,
	.cleanup	= uasp_function_cleanup,
	.bind_config	= uasp_function_bind_config,
};

static struct android_usb_function *supported_functions[] = {
	&ffs_function,
	&mbim_function,
	&ecm_qc_function,
#ifdef CONFIG_SND_PCM
	&audio_function,
#endif
	&rmnet_smd_function,
	&rmnet_function,
	&gps_function,
	&diag_function,
	&qdss_function,
	&serial_function,
	&ccid_function,
	&acm_function,
	&mtp_function,
	&ptp_function,
	&rndis_function,
	&rndis_qc_function,
	&ecm_function,
	&ncm_function,
	&mass_storage_function,
	&accessory_function,
#ifdef CONFIG_SND_PCM
	&audio_source_function,
#endif
	&uasp_function,
	&charger_function,
	NULL
};

static void android_cleanup_functions(struct android_usb_function **functions)
{
	struct android_usb_function *f;
	struct device_attribute **attrs;
	struct device_attribute *attr;

	while (*functions) {
		f = *functions++;

		if (f->dev) {
			device_destroy(android_class, f->dev->devt);
			kfree(f->dev_name);
		} else
			continue;

		if (f->cleanup)
			f->cleanup(f);

		attrs = f->attributes;
		if (attrs) {
			while ((attr = *attrs++))
				device_remove_file(f->dev, attr);
		}
	}
}

static int android_init_functions(struct android_usb_function **functions,
				  struct usb_composite_dev *cdev)
{
	struct android_dev *dev = cdev_to_android_dev(cdev);
	struct android_usb_function *f;
	struct device_attribute **attrs;
	struct device_attribute *attr;
	int err = 0;
	int index = 1; /* index 0 is for android0 device */

	for (; (f = *functions++); index++) {
		f->dev_name = kasprintf(GFP_KERNEL, "f_%s", f->name);
		f->android_dev = NULL;
		if (!f->dev_name) {
			err = -ENOMEM;
			goto err_out;
		}
		f->dev = device_create(android_class, dev->dev,
				MKDEV(0, index), f, f->dev_name);
		if (IS_ERR(f->dev)) {
			pr_err("%s: Failed to create dev %s", __func__,
							f->dev_name);
			err = PTR_ERR(f->dev);
			f->dev = NULL;
			goto err_create;
		}

		if (f->init) {
			err = f->init(f, cdev);
			if (err) {
				pr_err("%s: Failed to init %s", __func__,
								f->name);
				goto err_init;
			}
		}

		attrs = f->attributes;
		if (attrs) {
			while ((attr = *attrs++) && !err)
				err = device_create_file(f->dev, attr);
		}
		if (err) {
			pr_err("%s: Failed to create function %s attributes",
					__func__, f->name);
			goto err_attrs;
		}
	}
	return 0;

err_attrs:
	for (attr = *(attrs -= 2); attrs != f->attributes; attr = *(attrs--))
		device_remove_file(f->dev, attr);
	if (f->cleanup)
		f->cleanup(f);
err_init:
	device_destroy(android_class, f->dev->devt);
err_create:
	f->dev = NULL;
	kfree(f->dev_name);
err_out:
	android_cleanup_functions(dev->functions);
	return err;
}

static int
android_bind_enabled_functions(struct android_dev *dev,
			       struct usb_configuration *c)
{
	struct android_usb_function_holder *f_holder;
	struct android_configuration *conf =
		container_of(c, struct android_configuration, usb_config);
	int ret;

	list_for_each_entry(f_holder, &conf->enabled_functions, enabled_list) {
		ret = f_holder->f->bind_config(f_holder->f, c);
		if (ret) {
			pr_err("%s: %s failed\n", __func__, f_holder->f->name);
			while (!list_empty(&c->functions)) {
				struct usb_function		*f;

				f = list_first_entry(&c->functions,
					struct usb_function, list);
				list_del(&f->list);
				if (f->unbind)
					f->unbind(c, f);
			}
			if (c->unbind)
				c->unbind(c);
			return ret;
		}
	}
	return 0;
}

static void
android_unbind_enabled_functions(struct android_dev *dev,
			       struct usb_configuration *c)
{
	struct android_usb_function_holder *f_holder;
	struct android_configuration *conf =
		container_of(c, struct android_configuration, usb_config);

	list_for_each_entry(f_holder, &conf->enabled_functions, enabled_list) {
		if (f_holder->f->unbind_config)
			f_holder->f->unbind_config(f_holder->f, c);
	}
}
static inline void check_streaming_func(struct usb_gadget *gadget,
		struct android_usb_platform_data *pdata,
		char *name)
{
	int i;

	for (i = 0; i < pdata->streaming_func_count; i++) {
		if (!strcmp(name, pdata->streaming_func[i])) {
			pr_debug("set streaming_enabled to true\n");
			gadget->streaming_enabled = true;
			break;
		}
	}
}
static int android_enable_function(struct android_dev *dev,
				   struct android_configuration *conf,
				   char *name)
{
	struct android_usb_function **functions = dev->functions;
	struct android_usb_function *f;
	struct android_usb_function_holder *f_holder;
	struct android_usb_platform_data *pdata = dev->pdata;
	struct usb_gadget *gadget = dev->cdev->gadget;

	while ((f = *functions++)) {
		if (!strcmp(name, f->name)) {
			if (f->android_dev && f->android_dev != dev)
				pr_err("%s is enabled in other device\n",
					f->name);
			else {
				f_holder = kzalloc(sizeof(*f_holder),
						GFP_KERNEL);
				if (!f_holder) {
					pr_err("Failed to alloc f_holder\n");
					return -ENOMEM;
				}

				f->android_dev = dev;
				f_holder->f = f;
				list_add_tail(&f_holder->enabled_list,
					      &conf->enabled_functions);
				pr_debug("func:%s is enabled.\n", f->name);
				/*
				 * compare enable function with streaming func
				 * list and based on the same request streaming.
				 */
				check_streaming_func(gadget, pdata, f->name);

				return 0;
			}
		}
	}
	return -EINVAL;
}

/*-------------------------------------------------------------------------*/
/* /sys/class/android_usb/android%d/ interface */

static ssize_t remote_wakeup_show(struct device *pdev,
		struct device_attribute *attr, char *buf)
{
	struct android_dev *dev = dev_get_drvdata(pdev);
	struct android_configuration *conf;

	/*
	 * Show the wakeup attribute of the first configuration,
	 * since all configurations have the same wakeup attribute
	 */
	if (dev->configs_num == 0)
		return 0;
	conf = list_entry(dev->configs.next,
			  struct android_configuration,
			  list_item);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			!!(conf->usb_config.bmAttributes &
				USB_CONFIG_ATT_WAKEUP));
}

static ssize_t remote_wakeup_store(struct device *pdev,
		struct device_attribute *attr, const char *buff, size_t size)
{
	struct android_dev *dev = dev_get_drvdata(pdev);
	struct android_configuration *conf;
	int enable = 0;

	sscanf(buff, "%d", &enable);

	pr_debug("android_usb: %s remote wakeup\n",
			enable ? "enabling" : "disabling");

	list_for_each_entry(conf, &dev->configs, list_item)
		if (enable)
			conf->usb_config.bmAttributes |=
					USB_CONFIG_ATT_WAKEUP;
		else
			conf->usb_config.bmAttributes &=
					~USB_CONFIG_ATT_WAKEUP;

	return size;
}

static ssize_t
functions_show(struct device *pdev, struct device_attribute *attr, char *buf)
{
	struct android_dev *dev = dev_get_drvdata(pdev);
	struct android_configuration *conf;
	struct android_usb_function_holder *f_holder;
	char *buff = buf;

	mutex_lock(&dev->mutex);

	list_for_each_entry(conf, &dev->configs, list_item) {
		if (buff != buf)
			*(buff-1) = ':';
		list_for_each_entry(f_holder, &conf->enabled_functions,
					enabled_list)
			buff += snprintf(buff, PAGE_SIZE, "%s,",
					f_holder->f->name);
	}

	mutex_unlock(&dev->mutex);

	if (buff != buf)
		*(buff-1) = '\n';
	return buff - buf;
}

static ssize_t
functions_store(struct device *pdev, struct device_attribute *attr,
			       const char *buff, size_t size)
{
	struct android_dev *dev = dev_get_drvdata(pdev);
	struct list_head *curr_conf = &dev->configs;
	struct android_configuration *conf;
	char *conf_str;
	struct android_usb_function_holder *f_holder;
	char *name;
	char buf[256], *b;
	char aliases[256], *a;
	int err;
	int is_ffs;
	int ffs_enabled = 0;

	mutex_lock(&dev->mutex);

	if (dev->enabled) {
		mutex_unlock(&dev->mutex);
		return -EBUSY;
	}

	/* Clear previous enabled list */
	list_for_each_entry(conf, &dev->configs, list_item) {
		while (conf->enabled_functions.next !=
				&conf->enabled_functions) {
			f_holder = list_entry(conf->enabled_functions.next,
					typeof(*f_holder),
					enabled_list);
			f_holder->f->android_dev = NULL;
			list_del(&f_holder->enabled_list);
			kfree(f_holder);
		}
		INIT_LIST_HEAD(&conf->enabled_functions);
	}

	strlcpy(buf, buff, sizeof(buf));
	b = strim(buf);

	while (b) {
		conf_str = strsep(&b, ":");
		if (!conf_str)
			continue;

		/* If the next not equal to the head, take it */
		if (curr_conf->next != &dev->configs)
			conf = list_entry(curr_conf->next,
					  struct android_configuration,
					  list_item);
		else
			conf = alloc_android_config(dev);

		curr_conf = curr_conf->next;
		while (conf_str) {
			name = strsep(&conf_str, ",");
			is_ffs = 0;
			strlcpy(aliases, dev->ffs_aliases, sizeof(aliases));
			a = aliases;

			while (a) {
				char *alias = strsep(&a, ",");
				if (alias && !strcmp(name, alias)) {
					is_ffs = 1;
					break;
				}
			}

			if (is_ffs) {
				if (ffs_enabled)
					continue;
				err = android_enable_function(dev, conf, "ffs");
				if (err)
					pr_err("android_usb: Cannot enable ffs (%d)",
									err);
				else
					ffs_enabled = 1;
				continue;
			}

			if (!strcmp(name, "rndis") &&
				!strcmp(strim(rndis_transports), "BAM2BAM_IPA"))
				name = "rndis_qc";

			err = android_enable_function(dev, conf, name);
			if (err)
				pr_err("android_usb: Cannot enable '%s' (%d)",
							name, err);
		}
	}

	/* Free uneeded configurations if exists */
	while (curr_conf->next != &dev->configs) {
		conf = list_entry(curr_conf->next,
				  struct android_configuration, list_item);
		free_android_config(dev, conf);
	}

	mutex_unlock(&dev->mutex);

	return size;
}

static ssize_t enable_show(struct device *pdev, struct device_attribute *attr,
			   char *buf)
{
	struct android_dev *dev = dev_get_drvdata(pdev);

	return snprintf(buf, PAGE_SIZE, "%d\n", dev->enabled);
}

static ssize_t enable_store(struct device *pdev, struct device_attribute *attr,
			    const char *buff, size_t size)
{
	struct android_dev *dev = dev_get_drvdata(pdev);
	struct usb_composite_dev *cdev = dev->cdev;
	struct android_usb_function_holder *f_holder;
	struct android_configuration *conf;
	int enabled = 0;
	bool audio_enabled = false;
	static DEFINE_RATELIMIT_STATE(rl, 10*HZ, 1);
	int err = 0;

	if (!cdev)
		return -ENODEV;

	mutex_lock(&dev->mutex);

	sscanf(buff, "%d", &enabled);
	if (enabled && !dev->enabled) {
		/*
		 * Update values in composite driver's copy of
		 * device descriptor.
		 */
		cdev->desc.idVendor = device_desc.idVendor;
		cdev->desc.idProduct = device_desc.idProduct;
		cdev->desc.bcdDevice = device_desc.bcdDevice;
		cdev->desc.bDeviceClass = device_desc.bDeviceClass;
		cdev->desc.bDeviceSubClass = device_desc.bDeviceSubClass;
		cdev->desc.bDeviceProtocol = device_desc.bDeviceProtocol;

		/* Audio dock accessory is unable to enumerate device if
		 * pull-up is enabled immediately. The enumeration is
		 * reliable with 100 msec delay.
		 */
		list_for_each_entry(conf, &dev->configs, list_item)
			list_for_each_entry(f_holder, &conf->enabled_functions,
						enabled_list) {
				if (f_holder->f->enable)
					f_holder->f->enable(f_holder->f);
				if (!strncmp(f_holder->f->name,
						"audio_source", 12))
					audio_enabled = true;
			}
		if (audio_enabled)
			msleep(100);
		err = android_enable(dev);
		if (err < 0) {
			pr_err("%s: android_enable failed\n", __func__);
			dev->connected = 0;
			dev->enabled = false;
			mutex_unlock(&dev->mutex);
			return size;
		}
		dev->enabled = true;
	} else if (!enabled && dev->enabled) {
		android_disable(dev);
		list_for_each_entry(conf, &dev->configs, list_item)
			list_for_each_entry(f_holder, &conf->enabled_functions,
						enabled_list) {
				if (f_holder->f->disable)
					f_holder->f->disable(f_holder->f);
			}
		dev->enabled = false;
	} else if (__ratelimit(&rl)) {
		pr_err("android_usb: already %s\n",
				dev->enabled ? "enabled" : "disabled");
	}

	mutex_unlock(&dev->mutex);

	return size;
}

static ssize_t pm_qos_show(struct device *pdev,
			   struct device_attribute *attr, char *buf)
{
	struct android_dev *dev = dev_get_drvdata(pdev);

	return snprintf(buf, PAGE_SIZE, "%s\n", dev->pm_qos);
}

static ssize_t pm_qos_store(struct device *pdev,
			   struct device_attribute *attr,
			   const char *buff, size_t size)
{
	struct android_dev *dev = dev_get_drvdata(pdev);

	strlcpy(dev->pm_qos, buff, sizeof(dev->pm_qos));

	return size;
}

static ssize_t pm_qos_state_show(struct device *pdev,
			struct device_attribute *attr, char *buf)
{
	struct android_dev *dev = dev_get_drvdata(pdev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
				pm_qos_to_string(dev->curr_pm_qos_state));
}

static ssize_t state_show(struct device *pdev, struct device_attribute *attr,
			   char *buf)
{
	struct android_dev *dev = dev_get_drvdata(pdev);
	struct usb_composite_dev *cdev = dev->cdev;
	char *state = "DISCONNECTED";
	unsigned long flags;

	if (!cdev)
		goto out;

	spin_lock_irqsave(&cdev->lock, flags);
	if (cdev->config)
		state = "CONFIGURED";
	else if (dev->connected)
		state = "CONNECTED";
	spin_unlock_irqrestore(&cdev->lock, flags);
out:
	return snprintf(buf, PAGE_SIZE, "%s\n", state);
}

#define ANDROID_DEV_ATTR(field, format_string)				\
static ssize_t								\
field ## _show(struct device *pdev, struct device_attribute *attr,	\
		char *buf)						\
{									\
	struct android_dev *dev = dev_get_drvdata(pdev);		\
									\
	return snprintf(buf, PAGE_SIZE,					\
			format_string, dev->field);			\
}									\
static ssize_t								\
field ## _store(struct device *pdev, struct device_attribute *attr,	\
		const char *buf, size_t size)				\
{									\
	unsigned value;							\
	struct android_dev *dev = dev_get_drvdata(pdev);		\
									\
	if (sscanf(buf, format_string, &value) == 1) {			\
		dev->field = value;					\
		return size;						\
	}								\
	return -EINVAL;							\
}									\
static DEVICE_ATTR(field, S_IRUGO | S_IWUSR, field ## _show, field ## _store);

#define DESCRIPTOR_ATTR(field, format_string)				\
static ssize_t								\
field ## _show(struct device *dev, struct device_attribute *attr,	\
		char *buf)						\
{									\
	return snprintf(buf, PAGE_SIZE,					\
			format_string, device_desc.field);		\
}									\
static ssize_t								\
field ## _store(struct device *dev, struct device_attribute *attr,	\
		const char *buf, size_t size)				\
{									\
	int value;							\
	if (sscanf(buf, format_string, &value) == 1) {			\
		device_desc.field = value;				\
		return size;						\
	}								\
	return -1;							\
}									\
static DEVICE_ATTR(field, S_IRUGO | S_IWUSR, field ## _show, field ## _store);

#define DESCRIPTOR_STRING_ATTR(field, buffer)				\
static ssize_t								\
field ## _show(struct device *dev, struct device_attribute *attr,	\
		char *buf)						\
{									\
	return snprintf(buf, PAGE_SIZE, "%s", buffer);			\
}									\
static ssize_t								\
field ## _store(struct device *dev, struct device_attribute *attr,	\
		const char *buf, size_t size)				\
{									\
	if (size >= sizeof(buffer))					\
		return -EINVAL;						\
	strlcpy(buffer, buf, sizeof(buffer));				\
	strim(buffer);							\
	return size;							\
}									\
static DEVICE_ATTR(field, S_IRUGO | S_IWUSR, field ## _show, field ## _store);


DESCRIPTOR_ATTR(idVendor, "%04x\n")
DESCRIPTOR_ATTR(idProduct, "%04x\n")
DESCRIPTOR_ATTR(bcdDevice, "%04x\n")
DESCRIPTOR_ATTR(bDeviceClass, "%d\n")
DESCRIPTOR_ATTR(bDeviceSubClass, "%d\n")
DESCRIPTOR_ATTR(bDeviceProtocol, "%d\n")
DESCRIPTOR_STRING_ATTR(iManufacturer, manufacturer_string)
DESCRIPTOR_STRING_ATTR(iProduct, product_string)
DESCRIPTOR_STRING_ATTR(iSerial, serial_string)

static DEVICE_ATTR(functions, S_IRUGO | S_IWUSR, functions_show,
						 functions_store);
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR, enable_show, enable_store);

static DEVICE_ATTR(pm_qos, S_IRUGO | S_IWUSR, pm_qos_show, pm_qos_store);
static DEVICE_ATTR(pm_qos_state, S_IRUGO, pm_qos_state_show, NULL);
ANDROID_DEV_ATTR(up_pm_qos_sample_sec, "%u\n");
ANDROID_DEV_ATTR(down_pm_qos_sample_sec, "%u\n");
ANDROID_DEV_ATTR(up_pm_qos_threshold, "%u\n");
ANDROID_DEV_ATTR(down_pm_qos_threshold, "%u\n");
ANDROID_DEV_ATTR(idle_pc_rpm_no_int_secs, "%u\n");

static DEVICE_ATTR(state, S_IRUGO, state_show, NULL);
static DEVICE_ATTR(remote_wakeup, S_IRUGO | S_IWUSR,
		remote_wakeup_show, remote_wakeup_store);

static struct device_attribute *android_usb_attributes[] = {
	&dev_attr_idVendor,
	&dev_attr_idProduct,
	&dev_attr_bcdDevice,
	&dev_attr_bDeviceClass,
	&dev_attr_bDeviceSubClass,
	&dev_attr_bDeviceProtocol,
	&dev_attr_iManufacturer,
	&dev_attr_iProduct,
	&dev_attr_iSerial,
	&dev_attr_functions,
	&dev_attr_enable,
	&dev_attr_pm_qos,
	&dev_attr_up_pm_qos_sample_sec,
	&dev_attr_down_pm_qos_sample_sec,
	&dev_attr_up_pm_qos_threshold,
	&dev_attr_down_pm_qos_threshold,
	&dev_attr_idle_pc_rpm_no_int_secs,
	&dev_attr_pm_qos_state,
	&dev_attr_state,
	&dev_attr_remote_wakeup,
	NULL
};

/*-------------------------------------------------------------------------*/
/* Composite driver */

static int android_bind_config(struct usb_configuration *c)
{
	struct android_dev *dev = cdev_to_android_dev(c->cdev);
	int ret = 0;

	ret = android_bind_enabled_functions(dev, c);
	if (ret)
		return ret;

	return 0;
}

static void android_unbind_config(struct usb_configuration *c)
{
	struct android_dev *dev = cdev_to_android_dev(c->cdev);

	if (c->cdev->gadget->streaming_enabled) {
		c->cdev->gadget->streaming_enabled = false;
		pr_debug("setting streaming_enabled to false.\n");
	}
	android_unbind_enabled_functions(dev, c);
}

static int android_bind(struct usb_composite_dev *cdev)
{
	struct android_dev *dev;
	struct usb_gadget	*gadget = cdev->gadget;
	struct android_configuration *conf;
	int			id, ret;

	/* Bind to the last android_dev that was probed */
	dev = list_entry(android_dev_list.prev, struct android_dev, list_item);

	dev->cdev = cdev;

	/* Save the default handler */
	dev->setup_complete = cdev->req->complete;

	/*
	 * Start disconnected. Userspace will connect the gadget once
	 * it is done configuring the functions.
	 */
	usb_gadget_disconnect(gadget);

	/* Init the supported functions only once, on the first android_dev */
	if (android_dev_count == 1) {
		ret = android_init_functions(dev->functions, cdev);
		if (ret)
			return ret;
	}

	/* Allocate string descriptor numbers ... note that string
	 * contents can be overridden by the composite_dev glue.
	 */
	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_MANUFACTURER_IDX].id = id;
	device_desc.iManufacturer = id;

	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_PRODUCT_IDX].id = id;
	device_desc.iProduct = id;

	/* Default strings - should be updated by userspace */
	strlcpy(manufacturer_string, "Android",
		sizeof(manufacturer_string) - 1);
	strlcpy(product_string, "Android", sizeof(product_string) - 1);
	strlcpy(serial_string, "0123456789ABCDEF", sizeof(serial_string) - 1);

	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_SERIAL_IDX].id = id;
	device_desc.iSerialNumber = id;

	if (gadget_is_otg(cdev->gadget))
		list_for_each_entry(conf, &dev->configs, list_item)
			conf->usb_config.descriptors = otg_desc;

	return 0;
}

static int android_usb_unbind(struct usb_composite_dev *cdev)
{
	struct android_dev *dev = cdev_to_android_dev(cdev);

	manufacturer_string[0] = '\0';
	product_string[0] = '\0';
	serial_string[0] = '0';
	cancel_work_sync(&dev->work);
	cancel_delayed_work_sync(&dev->pm_qos_work);
	android_cleanup_functions(dev->functions);
	return 0;
}

/* HACK: android needs to override setup for accessory to work */
static int (*composite_setup_func)(struct usb_gadget *gadget, const struct usb_ctrlrequest *c);
static void (*composite_suspend_func)(struct usb_gadget *gadget);
static void (*composite_resume_func)(struct usb_gadget *gadget);

static int
android_setup(struct usb_gadget *gadget, const struct usb_ctrlrequest *c)
{
	struct usb_composite_dev	*cdev = get_gadget_data(gadget);
	struct android_dev		*dev = cdev_to_android_dev(cdev);
	struct usb_request		*req = cdev->req;
	struct android_usb_function	*f;
	struct android_usb_function_holder *f_holder;
	struct android_configuration	*conf;
	int value = -EOPNOTSUPP;
	unsigned long flags;
	bool do_work = false;
	bool prev_configured = false;

	req->zero = 0;
	req->length = 0;
	req->complete = dev->setup_complete;
	gadget->ep0->driver_data = cdev;

	list_for_each_entry(conf, &dev->configs, list_item)
		list_for_each_entry(f_holder,
				    &conf->enabled_functions,
				    enabled_list) {
			f = f_holder->f;
			if (f->ctrlrequest) {
				value = f->ctrlrequest(f, cdev, c);
				if (value >= 0)
					break;
			}
		}

	/*
	 * skip the  work when 2nd set config arrives
	 * with same value from the host.
	 */
	if (cdev->config)
		prev_configured = true;
	/* Special case the accessory function.
	 * It needs to handle control requests before it is enabled.
	 */
	if (value < 0)
		value = acc_ctrlrequest(cdev, c);

	if (value < 0)
		value = composite_setup_func(gadget, c);

	spin_lock_irqsave(&cdev->lock, flags);
	if (!dev->connected) {
		dev->connected = 1;
		do_work = true;
	} else if (c->bRequest == USB_REQ_SET_CONFIGURATION &&
						cdev->config) {
		if (!prev_configured)
			do_work = true;
	}
	spin_unlock_irqrestore(&cdev->lock, flags);
	if (do_work)
		schedule_work(&dev->work);
	return value;
}

static void android_disconnect(struct usb_composite_dev *cdev)
{
	struct android_dev *dev = cdev_to_android_dev(cdev);

	/* accessory HID support can be active while the
	   accessory function is not actually enabled,
	   so we need to inform it when we are disconnected.
	 */
	acc_disconnect();

	dev->connected = 0;
	schedule_work(&dev->work);
}

static struct usb_composite_driver android_usb_driver = {
	.name		= "android_usb",
	.dev		= &device_desc,
	.strings	= dev_strings,
	.bind		= android_bind,
	.unbind		= android_usb_unbind,
	.disconnect	= android_disconnect,
	.max_speed	= USB_SPEED_SUPER
};

static void android_suspend(struct usb_gadget *gadget)
{
	struct usb_composite_dev *cdev = get_gadget_data(gadget);
	struct android_dev *dev = cdev_to_android_dev(cdev);
	unsigned long flags;

	spin_lock_irqsave(&cdev->lock, flags);
	if (!dev->suspended) {
		dev->suspended = 1;
		schedule_work(&dev->work);
	}
	spin_unlock_irqrestore(&cdev->lock, flags);

	composite_suspend_func(gadget);
}

static void android_resume(struct usb_gadget *gadget)
{
	struct usb_composite_dev *cdev = get_gadget_data(gadget);
	struct android_dev *dev = cdev_to_android_dev(cdev);
	unsigned long flags;

	spin_lock_irqsave(&cdev->lock, flags);
	if (dev->suspended) {
		dev->suspended = 0;
		schedule_work(&dev->work);
	}
	spin_unlock_irqrestore(&cdev->lock, flags);

	composite_resume_func(gadget);
}

static int android_create_device(struct android_dev *dev, u8 usb_core_id)
{
	struct device_attribute **attrs = android_usb_attributes;
	struct device_attribute *attr;
	char device_node_name[ANDROID_DEVICE_NODE_NAME_LENGTH];
	int err;

	/*
	 * The primary usb core should always have usb_core_id=0, since
	 * Android user space is currently interested in android0 events.
	 */
	snprintf(device_node_name, ANDROID_DEVICE_NODE_NAME_LENGTH,
		 "android%d", usb_core_id);
	pr_debug("%s(): creating android%d device\n", __func__, usb_core_id);
	dev->dev = device_create(android_class, NULL,
					MKDEV(0, 0), NULL, device_node_name);
	if (IS_ERR(dev->dev))
		return PTR_ERR(dev->dev);

	dev_set_drvdata(dev->dev, dev);

	while ((attr = *attrs++)) {
		err = device_create_file(dev->dev, attr);
		if (err) {
			device_destroy(android_class, dev->dev->devt);
			return err;
		}
	}
	return 0;
}

static void android_destroy_device(struct android_dev *dev)
{
	struct device_attribute **attrs = android_usb_attributes;
	struct device_attribute *attr;

	while ((attr = *attrs++))
		device_remove_file(dev->dev, attr);
	device_destroy(android_class, dev->dev->devt);
}

static struct android_dev *cdev_to_android_dev(struct usb_composite_dev *cdev)
{
	struct android_dev *dev = NULL;

	/* Find the android dev from the list */
	list_for_each_entry(dev, &android_dev_list, list_item) {
		if (dev->cdev == cdev)
			break;
	}

	return dev;
}

static struct android_configuration *alloc_android_config
						(struct android_dev *dev)
{
	struct android_configuration *conf;

	conf = kzalloc(sizeof(*conf), GFP_KERNEL);
	if (!conf) {
		pr_err("%s(): Failed to alloc memory for android conf\n",
			__func__);
		return ERR_PTR(-ENOMEM);
	}

	dev->configs_num++;
	conf->usb_config.label = dev->name;
	conf->usb_config.unbind = android_unbind_config;
	conf->usb_config.bConfigurationValue = dev->configs_num;

	INIT_LIST_HEAD(&conf->enabled_functions);

	list_add_tail(&conf->list_item, &dev->configs);

	return conf;
}

static void free_android_config(struct android_dev *dev,
			     struct android_configuration *conf)
{
	list_del(&conf->list_item);
	dev->configs_num--;
	kfree(conf);
}

static int usb_diag_update_pid_and_serial_num(u32 pid, const char *snum)
{
	struct dload_struct local_diag_dload = { 0 };
	int *src, *dst, i;

	if (!diag_dload) {
		pr_debug("%s: unable to update PID and serial_no\n", __func__);
		return -ENODEV;
	}

	pr_debug("%s: dload:%p pid:%x serial_num:%s\n",
				__func__, diag_dload, pid, snum);

	/* update pid */
	local_diag_dload.magic_struct.pid = PID_MAGIC_ID;
	local_diag_dload.pid = pid;

	/* update serial number */
	if (!snum) {
		local_diag_dload.magic_struct.serial_num = 0;
		memset(&local_diag_dload.serial_number, 0,
				SERIAL_NUMBER_LENGTH);
	} else {
		local_diag_dload.magic_struct.serial_num = SERIAL_NUM_MAGIC_ID;
		strlcpy((char *)&local_diag_dload.serial_number, snum,
				SERIAL_NUMBER_LENGTH);
	}

	/* Copy to shared struct (accesses need to be 32 bit aligned) */
	src = (int *)&local_diag_dload;
	dst = (int *)diag_dload;

	for (i = 0; i < sizeof(*diag_dload) / 4; i++)
		*dst++ = *src++;

	return 0;
}

static int android_probe(struct platform_device *pdev)
{
	struct android_usb_platform_data *pdata;
	struct android_dev *android_dev;
	struct resource *res;
	int ret = 0, i, len = 0, prop_len = 0;
	u32 usb_core_id = 0;

	if (pdev->dev.of_node) {
		dev_dbg(&pdev->dev, "device tree enabled\n");
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			pr_err("unable to allocate platform data\n");
			return -ENOMEM;
		}

		of_get_property(pdev->dev.of_node, "qcom,pm-qos-latency",
								&prop_len);
		if (prop_len == sizeof(pdata->pm_qos_latency)) {
			of_property_read_u32_array(pdev->dev.of_node,
				"qcom,pm-qos-latency", pdata->pm_qos_latency,
				 prop_len/sizeof(*pdata->pm_qos_latency));
		} else {
			pr_info("pm_qos latency not specified %d\n", prop_len);
		}

		ret = of_property_read_u32(pdev->dev.of_node,
					"qcom,usb-core-id",
					&usb_core_id);
		if (!ret)
			pdata->usb_core_id = usb_core_id;

		len = of_property_count_strings(pdev->dev.of_node,
				"qcom,streaming-func");
		if (len > MAX_STREAMING_FUNCS) {
			pr_err("Invalid number of functions used.\n");
			return -EINVAL;
		}

		for (i = 0; i < len; i++) {
			const char *name = NULL;

			of_property_read_string_index(pdev->dev.of_node,
				"qcom,streaming-func", i, &name);

			if (!name)
				continue;

			if (sizeof(name) > FUNC_NAME_LEN) {
				pr_err("Function name is bigger than allowed.\n");
				continue;
			}

			strlcpy(pdata->streaming_func[i], name,
				sizeof(pdata->streaming_func[i]));
			pr_debug("name of streaming function:%s\n",
				pdata->streaming_func[i]);
		}

		pdata->streaming_func_count = len;

		pdata->cdrom = of_property_read_bool(pdev->dev.of_node,
			"qcom,android-usb-cdrom");
		ret = of_property_read_u8(pdev->dev.of_node,
				"qcom,android-usb-uicc-nluns",
				&pdata->uicc_nluns);
	} else {
		pdata = pdev->dev.platform_data;
	}

	if (!android_class) {
		android_class = class_create(THIS_MODULE, "android_usb");
		if (IS_ERR(android_class))
			return PTR_ERR(android_class);
	}

	android_dev = kzalloc(sizeof(*android_dev), GFP_KERNEL);
	if (!android_dev) {
		pr_err("%s(): Failed to alloc memory for android_dev\n",
			__func__);
		ret = -ENOMEM;
		goto err_alloc;
	}

	android_dev->name = pdev->name;
	android_dev->disable_depth = 1;
	android_dev->functions = supported_functions;
	android_dev->configs_num = 0;
	INIT_LIST_HEAD(&android_dev->configs);
	INIT_WORK(&android_dev->work, android_work);
	INIT_DELAYED_WORK(&android_dev->pm_qos_work, android_pm_qos_work);
	mutex_init(&android_dev->mutex);

	android_dev->pdata = pdata;

	list_add_tail(&android_dev->list_item, &android_dev_list);
	android_dev_count++;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) {
		diag_dload = devm_ioremap(&pdev->dev, res->start,
							resource_size(res));
		if (!diag_dload) {
			dev_err(&pdev->dev, "ioremap failed\n");
			ret = -ENOMEM;
			goto err_dev;
		}
	} else {
		dev_dbg(&pdev->dev, "failed to get mem resource\n");
	}

	if (pdata)
		android_usb_driver.gadget_driver.usb_core_id =
						pdata->usb_core_id;
	ret = android_create_device(android_dev,
			android_usb_driver.gadget_driver.usb_core_id);
	if (ret) {
		pr_err("%s(): android_create_device failed\n", __func__);
		goto err_dev;
	}

	pr_debug("%s(): registering android_usb_driver with core id:%d\n",
		__func__, android_usb_driver.gadget_driver.usb_core_id);
	ret = usb_composite_probe(&android_usb_driver);
	if (ret) {
		/* Perhaps UDC hasn't probed yet, try again later */
		if (ret == -ENODEV)
			ret = -EPROBE_DEFER;
		else
			pr_err("%s(): Failed to register android composite driver\n",
				__func__);
		goto err_probe;
	}

	/* pm qos request to prevent apps idle power collapse */
	android_dev->curr_pm_qos_state = NO_USB_VOTE;
	if (pdata && pdata->pm_qos_latency[0]) {
		pm_qos_add_request(&android_dev->pm_qos_req_dma,
			PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
		android_dev->down_pm_qos_sample_sec = DOWN_PM_QOS_SAMPLE_SEC;
		android_dev->down_pm_qos_threshold = DOWN_PM_QOS_THRESHOLD;
		android_dev->up_pm_qos_sample_sec = UP_PM_QOS_SAMPLE_SEC;
		android_dev->up_pm_qos_threshold = UP_PM_QOS_THRESHOLD;
		android_dev->idle_pc_rpm_no_int_secs = IDLE_PC_RPM_NO_INT_SECS;
	}
	strlcpy(android_dev->pm_qos, "high", sizeof(android_dev->pm_qos));

	return ret;
err_probe:
	android_destroy_device(android_dev);
err_dev:
	list_del(&android_dev->list_item);
	android_dev_count--;
	kfree(android_dev);
err_alloc:
	if (list_empty(&android_dev_list)) {
		class_destroy(android_class);
		android_class = NULL;
	}
	return ret;
}

static int android_remove(struct platform_device *pdev)
{
	struct android_dev *dev = NULL;
	struct android_usb_platform_data *pdata = pdev->dev.platform_data;
	int usb_core_id = 0;

	if (pdata)
		usb_core_id = pdata->usb_core_id;

	/* Find the android dev from the list */
	list_for_each_entry(dev, &android_dev_list, list_item) {
		if (!dev->pdata)
			break; /*To backward compatibility*/
		if (dev->pdata->usb_core_id == usb_core_id)
			break;
	}

	if (dev) {
		android_destroy_device(dev);
		if (pdata && pdata->pm_qos_latency[0])
			pm_qos_remove_request(&dev->pm_qos_req_dma);
		list_del(&dev->list_item);
		android_dev_count--;
		kfree(dev);
	}

	if (list_empty(&android_dev_list)) {
		class_destroy(android_class);
		android_class = NULL;
		usb_composite_unregister(&android_usb_driver);
	}

	return 0;
}

static const struct platform_device_id android_id_table[] = {
	{
		.name = "android_usb",
	},
	{
		.name = "android_usb_hsic",
	},
};

static struct of_device_id usb_android_dt_match[] = {
	{	.compatible = "qcom,android-usb",
	},
	{}
};

static struct platform_driver android_platform_driver = {
	.driver = {
		.name = "android_usb",
		.of_match_table = usb_android_dt_match,
	},
	.probe = android_probe,
	.remove = android_remove,
	.id_table = android_id_table,
};

static int __init init(void)
{
	int ret;

	INIT_LIST_HEAD(&android_dev_list);
	android_dev_count = 0;

	ret = platform_driver_register(&android_platform_driver);
	if (ret) {
		pr_err("%s(): Failed to register android"
				 "platform driver\n", __func__);
	}

	/* HACK: exchange composite's setup with ours */
	composite_setup_func = android_usb_driver.gadget_driver.setup;
	android_usb_driver.gadget_driver.setup = android_setup;
	composite_suspend_func = android_usb_driver.gadget_driver.suspend;
	android_usb_driver.gadget_driver.suspend = android_suspend;
	composite_resume_func = android_usb_driver.gadget_driver.resume;
	android_usb_driver.gadget_driver.resume = android_resume;

	return ret;
}
late_initcall(init);

static void __exit cleanup(void)
{
	platform_driver_unregister(&android_platform_driver);
}
module_exit(cleanup);
