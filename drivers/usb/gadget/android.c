/*
 * Gadget Driver for Android
 *
 * Copyright (C) 2008 Google, Inc.
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

#include <linux/usb/ch9.h>
#include <linux/usb/composite.h>
#include <linux/usb/gadget.h>

/* Add for HW/SW connect */
#include "mtk_gadget.h"
/* Add for HW/SW connect */

#include "u_fs.h"

#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
#include "f_hid.c"
#endif
#ifdef CONFIG_SND_RAWMIDI
#include "f_midi.c"
#endif


#include "f_accessory.c"
#include "f_mass_storage.h"
#include "f_mtp.c"
#include "f_eem.c"
#include "f_rndis.c"
/* note ERROR macro both appear on cdev & u_ether, make sure what you want */
#include "rndis.c"
#include "u_ether.c"

USB_ETHERNET_MODULE_PARAMETERS();

#ifdef CONFIG_MTK_ECCCI_C2K_TMP
#include "viatel_rawbulk.h"
#endif

#ifdef CONFIG_MTK_USB2JTAG_SUPPORT
#include <mt-plat/mtk_usb2jtag.h>
#endif

MODULE_AUTHOR("Mike Lockwood");
MODULE_DESCRIPTION("Android Composite USB Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

static const char longname[] = "Gadget Android";

/* Default vendor and product IDs, overridden by userspace */
#define VENDOR_ID		0x18D1
#define PRODUCT_ID		0x0001

#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
#include <mt-plat/mtk_boot_common.h>
#define KPOC_USB_FUNC "hid"
#define KPOC_USB_VENDOR_ID 0x0E8D
#define KPOC_USB_PRODUCT_ID 0x20FF
#endif

#ifdef CONFIG_SND_RAWMIDI
/* f_midi configuration */
#define MIDI_INPUT_PORTS    1
#define MIDI_OUTPUT_PORTS   1
#define MIDI_BUFFER_SIZE    1024
#define MIDI_QUEUE_LENGTH   32
#endif
#ifdef CONFIG_MTPROF
#include "bootprof.h"
#endif

struct android_usb_function {
	char *name;
	void *config;

	struct device *dev;
	char *dev_name;
	struct device_attribute **attributes;

	/* for android_dev.enabled_functions */
	struct list_head enabled_list;

	/* Optional: initialization during gadget bind */
	int (*init)(struct android_usb_function *, struct usb_composite_dev *);
	/* Optional: cleanup during gadget unbind */
	void (*cleanup)(struct android_usb_function *);
	/* Optional: called when the function is added the list of
	 *		enabled functions
	 */
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

struct android_dev {
	struct android_usb_function **functions;
	struct list_head enabled_functions;
	struct usb_composite_dev *cdev;
	struct device *dev;

	void (*setup_complete)(struct usb_ep *ep,
				struct usb_request *req);

	bool enabled;
	int disable_depth;
	struct mutex mutex;
	bool connected;
	bool sw_connected;
	struct work_struct work;
	char ffs_aliases[256];
};

static struct class *android_class;
static struct android_dev *_android_dev;
static int android_bind_config(struct usb_configuration *c);
static void android_unbind_config(struct usb_configuration *c);
static int android_setup_config(struct usb_configuration *c, const struct usb_ctrlrequest *ctrl);

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
#ifdef CONFIG_USB_MU3D_DRV
	.bcdUSB               = cpu_to_le16(0x0300),
#else
	.bcdUSB               = cpu_to_le16(0x0200),
#endif
	.bDeviceClass         = USB_CLASS_PER_INTERFACE,
	.idVendor             = cpu_to_le16(VENDOR_ID),
	.idProduct            = cpu_to_le16(PRODUCT_ID),
	.bcdDevice            = cpu_to_le16(0xffff),
	.bNumConfigurations   = 1,
};

static struct usb_configuration android_config_driver = {
	.label		= "android",
	.setup		= android_setup_config,
	.unbind		= android_unbind_config,
	.bConfigurationValue = 1,
#ifdef CONFIG_USBIF_COMPLIANCE
	.bmAttributes	= USB_CONFIG_ATT_ONE,
#else
	.bmAttributes	= USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
#endif
#ifdef CONFIG_USB_MU3D_DRV
	.MaxPower	= 192, /* Only consume 192ma for passing USB30CV Descriptor Test [USB3.0 devices]*/
#else
	.MaxPower	= 500, /* 500ma */
#endif
};

static void android_work(struct work_struct *data)
{
	struct android_dev *dev = container_of(data, struct android_dev, work);
	struct usb_composite_dev *cdev = dev->cdev;
	char *disconnected[2] = { "USB_STATE=DISCONNECTED", NULL };
	char *connected[2]    = { "USB_STATE=CONNECTED", NULL };
	char *configured[2]   = { "USB_STATE=CONFIGURED", NULL };
	/* Add for HW/SW connect */
	char *hwdisconnected[2] = { "USB_STATE=HWDISCONNECTED", NULL };
	bool is_hwconnected = true;
	char **uevent_envp = NULL;
	unsigned long flags;

	if (!cdev) {
		pr_notice("android_work, !cdev\n");
		return;
	}

	/* be aware this could not be used in non-sleep context */
	if (usb_cable_connected())
		is_hwconnected = true;
	else
		is_hwconnected = false;

	spin_lock_irqsave(&cdev->lock, flags);
	if (cdev->config)
		uevent_envp = configured;
	else if (dev->connected != dev->sw_connected)
		uevent_envp = dev->connected ? connected : disconnected;
	dev->sw_connected = dev->connected;
	spin_unlock_irqrestore(&cdev->lock, flags);

	if (uevent_envp) {
		kobject_uevent_env(&dev->dev->kobj, KOBJ_CHANGE, uevent_envp);
		pr_notice("%s: sent uevent %s, is_hwconnected=%d\n", __func__, uevent_envp[0], is_hwconnected);
#ifdef CONFIG_MTPROF
		if (uevent_envp == configured) {
			static int first_shot = 1;

			if (first_shot) {
				log_boot("USB configured");
				first_shot = 0;
			}
		}
#endif
	} else {
		pr_notice("%s: did not send uevent (%d %d %p), is_hwconnected=%d\n", __func__,
			 dev->connected, dev->sw_connected, cdev->config, is_hwconnected);
	}

	if (!is_hwconnected) {
		kobject_uevent_env(&dev->dev->kobj, KOBJ_CHANGE, hwdisconnected);
		pr_notice("[USB]%s: sent uevent %s\n", __func__, hwdisconnected[0]);
	}
}

static void android_enable(struct android_dev *dev)
{
	struct usb_composite_dev *cdev = dev->cdev;


	if (WARN_ON(!dev->disable_depth))
		return;

	if (--dev->disable_depth == 0) {
		usb_add_config(cdev, &android_config_driver,
					android_bind_config);
		usb_gadget_connect(cdev->gadget);
	}
}

static void android_disable(struct android_dev *dev)
{
	struct usb_composite_dev *cdev = dev->cdev;


	if (dev->disable_depth++ == 0) {
		usb_gadget_disconnect(cdev->gadget);
		/* Cancel pending control requests */
		usb_ep_dequeue(cdev->gadget->ep0, cdev->req);
		usb_remove_config(cdev, &android_config_driver);
	}
}

/*-------------------------------------------------------------------------*/
/* Supported functions initialization */
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
static int hid_function_init(struct android_usb_function *f,
		struct usb_composite_dev *cdev)
{
	return ghid_setup(cdev->gadget, 2);
}
static int hid_function_bind_config(struct android_usb_function *f,
		struct usb_configuration *c)
{
	return hidg_bind_config(c, NULL, 0);
}
static void hid_function_cleanup(struct android_usb_function *f)
{
	ghid_cleanup();
}
static struct android_usb_function hid_function = {
	.name		= "hid",
	.init		= hid_function_init,
	.cleanup	= hid_function_cleanup,
	.bind_config	= hid_function_bind_config,
};
#endif

struct functionfs_config {
	bool opened;
	bool enabled;
	struct usb_function *func;
	struct usb_function_instance *fi;
	struct ffs_data *data;
};

static int functionfs_ready_callback(struct ffs_data *ffs);
static void functionfs_closed_callback(struct ffs_data *ffs);

static int ffs_function_init(struct android_usb_function *f,
			     struct usb_composite_dev *cdev)
{
	struct functionfs_config *config;
	struct f_fs_opts *opts;

	f->config = kzalloc(sizeof(struct functionfs_config), GFP_KERNEL);
	if (!f->config)
		return -ENOMEM;

	config = f->config;
	config->fi = usb_get_function_instance("ffs");
	if (IS_ERR(config->fi))
		return PTR_ERR(config->fi);

	opts = to_f_fs_opts(config->fi);
	opts->dev->ffs_ready_callback = functionfs_ready_callback;
	opts->dev->ffs_closed_callback = functionfs_closed_callback;
	opts->no_configfs = true;

	return ffs_single_dev(opts->dev);
}

static void ffs_function_cleanup(struct android_usb_function *f)
{
	struct functionfs_config *config = f->config;


	if (config)
		usb_put_function_instance(config->fi);

	kfree(f->config);
}

static void ffs_function_enable(struct android_usb_function *f)
{
	struct android_dev *dev = _android_dev;
	struct functionfs_config *config = f->config;


	config->enabled = true;

	/* Disable the gadget until the function is ready */
	if (!config->opened)
		android_disable(dev);
}

static void ffs_function_disable(struct android_usb_function *f)
{
	struct android_dev *dev = _android_dev;
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
	int ret;

	config->func = usb_get_function(config->fi);
	if (IS_ERR(config->func))
		return PTR_ERR(config->func);

	ret = usb_add_function(c, config->func);
	if (ret) {
		pr_err("%s(): usb_add_function() fails (err:%d) for ffs\n",
							__func__, ret);

		usb_put_function(config->func);
		config->func = NULL;
	}

	return ret;
}

static ssize_t
ffs_aliases_show(struct device *pdev, struct device_attribute *attr, char *buf)
{
	struct android_dev *dev = _android_dev;
	int ret;

	mutex_lock(&dev->mutex);
	ret = sprintf(buf, "%s\n", dev->ffs_aliases);
	mutex_unlock(&dev->mutex);

	return ret;
}

static ssize_t
ffs_aliases_store(struct device *pdev, struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct android_dev *dev = _android_dev;
	char buff[256];

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
	struct android_dev *dev = _android_dev;
	struct functionfs_config *config = ffs_function.config;
	int ret = 0;

	mutex_lock(&dev->mutex);

	config->data = ffs;
	config->opened = true;

	if (config->enabled)
		android_enable(dev);

	mutex_unlock(&dev->mutex);
	return ret;
}

static void functionfs_closed_callback(struct ffs_data *ffs)
{
	struct android_dev *dev = _android_dev;
	struct functionfs_config *config = ffs_function.config;

	mutex_lock(&dev->mutex);

	if (config->enabled)
		android_disable(dev);

	config->opened = false;
	config->data = NULL;

	if (config->func) {
		usb_put_function(config->func);
		config->func = NULL;
	}

	mutex_unlock(&dev->mutex);
}

/* note all serial port number could not exceed MAX_U_SERIAL_PORTS */
#define MAX_ACM_INSTANCES 4
struct acm_function_config {
	int instances;
	int instances_on;
	struct usb_function *f_acm[MAX_ACM_INSTANCES];
	struct usb_function_instance *f_acm_inst[MAX_ACM_INSTANCES];
	int port_index[MAX_ACM_INSTANCES];
	int port_index_on[MAX_ACM_INSTANCES];
};

static int
acm_function_init(struct android_usb_function *f,
		struct usb_composite_dev *cdev)
{
	int i;
	int ret;
	struct acm_function_config *config;

	config = kzalloc(sizeof(struct acm_function_config), GFP_KERNEL);
	if (!config)
		return -ENOMEM;
	f->config = config;

	for (i = 0; i < MAX_ACM_INSTANCES; i++) {
		config->f_acm_inst[i] = usb_get_function_instance("acm");
		if (IS_ERR(config->f_acm_inst[i])) {
			ret = PTR_ERR(config->f_acm_inst[i]);
			goto err_usb_get_function_instance;
		}
		config->f_acm[i] = usb_get_function(config->f_acm_inst[i]);
		if (IS_ERR(config->f_acm[i])) {
			ret = PTR_ERR(config->f_acm[i]);
			goto err_usb_get_function;
		}
	}
	return 0;
err_usb_get_function_instance:
	pr_err("Could not usb_get_function_instance() %d\n", i);
	while (i-- > 0) {
		usb_put_function(config->f_acm[i]);
err_usb_get_function:
		pr_err("Could not usb_get_function() %d\n", i);
		usb_put_function_instance(config->f_acm_inst[i]);
	}
	return ret;
}

static void acm_function_cleanup(struct android_usb_function *f)
{
	int i;
	struct acm_function_config *config = f->config;

	for (i = 0; i < MAX_ACM_INSTANCES; i++) {
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
	int i;
	int ret = 0;
	struct acm_function_config *config = f->config;
	/*1st:Modem, 2nd:Modem, 3rd:BT, 4th:MD logger*/
	for (i = 0; i < MAX_ACM_INSTANCES; i++) {
		if (config->port_index[i] != 0) {
			ret = usb_add_function(c, config->f_acm[i]);
			if (ret) {
				pr_err("Could not bind acm%u config\n", i);
				goto err_usb_add_function;
			}
			pr_notice("%s Open /dev/ttyGS%d\n", __func__, i);
			config->port_index[i] = 0;
			config->port_index_on[i] = 1;
			config->instances = 0;
		}
	}


	config->instances_on = config->instances;
	for (i = 0; i < config->instances_on; i++) {
		ret = usb_add_function(c, config->f_acm[i]);
		if (ret) {
			pr_err("Could not bind acm%u config\n", i);
			goto err_usb_add_function;
		}
	}

	return 0;

err_usb_add_function:
	while (i-- > 0)
		usb_remove_function(c, config->f_acm[i]);
	return ret;
}

static void acm_function_unbind_config(struct android_usb_function *f,
				       struct usb_configuration *c)
{
	struct acm_function_config *config = f->config;

	config->instances_on = 0;
}

static ssize_t acm_instances_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct acm_function_config *config = f->config;

	return sprintf(buf, "%d\n", config->instances);
}

static ssize_t acm_instances_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct acm_function_config *config = f->config;

	int value, ret;


	ret = kstrtoint(buf, 0, &value);
	if (value > MAX_ACM_INSTANCES)
		value = MAX_ACM_INSTANCES;
	config->instances = value;
	return size;
}

static DEVICE_ATTR(instances, S_IRUGO | S_IWUSR, acm_instances_show,
						 acm_instances_store);

static ssize_t acm_port_index_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct acm_function_config *config = f->config;

	return sprintf(buf, "%d,%d,%d,%d\n", config->port_index[0], config->port_index[1],
		config->port_index[2], config->port_index[3]);
}

static ssize_t acm_port_index_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct acm_function_config *config = f->config;
	int val[MAX_ACM_INSTANCES] = {0};
	int num = 0;
	int tmp = 0;

	num = sscanf(buf, "%d,%d,%d,%d", &(val[0]), &(val[1]), &(val[2]), &(val[3]));

	pr_notice("%s [0]=%d,[1]=%d,[2]=%d,[3]=%d, num=%d\n", __func__, val[0], val[1], val[2], val[3], num);

	/* Set all port_index as 0*/
	for (tmp = 0; tmp < MAX_ACM_INSTANCES; tmp++)
		config->port_index[tmp] = 0;

	for (tmp = 0; tmp < num; tmp++) {
		int port = (val[tmp] > MAX_ACM_INSTANCES || val[tmp] < 1) ? 0 : val[tmp]-1;

		config->port_index[port] = 1;
	}

	return size;
}

static DEVICE_ATTR(port_index, S_IRUGO | S_IWUSR, acm_port_index_show,
						 acm_port_index_store);

static struct device_attribute *acm_function_attributes[] = {
	&dev_attr_instances,
	&dev_attr_port_index, /*Only open the specific port*/
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

#ifdef CONFIG_USB_F_SS_LB
#define MAX_LOOPBACK_INSTANCES 1

struct loopback_function_config {
	int port_num;
	struct usb_function *f_lp[MAX_LOOPBACK_INSTANCES];
	struct usb_function_instance *f_lp_inst[MAX_LOOPBACK_INSTANCES];
};

static int loopback_function_init(struct android_usb_function *f, struct usb_composite_dev *cdev)
{
	int i;
	int ret;
	struct loopback_function_config *config;


	config = kzalloc(sizeof(struct loopback_function_config), GFP_KERNEL);
	if (!config)
		return -ENOMEM;
	f->config = config;

	for (i = 0; i < MAX_LOOPBACK_INSTANCES; i++) {
		config->f_lp_inst[i] = usb_get_function_instance("Loopback");
		if (IS_ERR(config->f_lp_inst[i])) {
			ret = PTR_ERR(config->f_lp_inst[i]);
			goto err_usb_get_function_instance;
		}
		config->f_lp[i] = usb_get_function(config->f_lp_inst[i]);
		if (IS_ERR(config->f_lp[i])) {
			ret = PTR_ERR(config->f_lp[i]);
			goto err_usb_get_function;
		}
	}
	return 0;
err_usb_get_function_instance:
	pr_err("Could not usb_get_function_instance() %d\n", i);
	while (i-- > 0) {
		usb_put_function(config->f_lp[i]);
err_usb_get_function:
		pr_err("Could not usb_get_function() %d\n", i);
		usb_put_function_instance(config->f_lp_inst[i]);
	}
	return ret;
}

static void loopback_function_cleanup(struct android_usb_function *f)
{
	int i;
	struct loopback_function_config *config = f->config;


	for (i = 0; i < MAX_LOOPBACK_INSTANCES; i++) {
		usb_put_function(config->f_lp[i]);
		usb_put_function_instance(config->f_lp_inst[i]);
	}
	kfree(f->config);
	f->config = NULL;
}

static int
loopback_function_bind_config(struct android_usb_function *f, struct usb_configuration *c)
{
	int ret = 0;
	struct loopback_function_config *config = f->config;


	ret = usb_add_function(c, config->f_lp[config->port_num]);
	if (ret) {
		pr_err("Could not bind loopback%u config\n", config->port_num);
		goto err_usb_add_function;
	}
	pr_notice("%s Open loopback\n", __func__);

	return 0;

err_usb_add_function:
	usb_remove_function(c, config->f_lp[config->port_num]);
	return ret;
}

static struct android_usb_function loopback_function = {
	.name		= "loopback",
	.init		= loopback_function_init,
	.cleanup	= loopback_function_cleanup,
	.bind_config	= loopback_function_bind_config,
};
#endif

/* note all serial port number could not exceed MAX_U_SERIAL_PORTS */
#define MAX_SERIAL_INSTANCES 4

struct serial_function_config {
	int port_num;
	struct usb_function *f_ser[MAX_SERIAL_INSTANCES];
	struct usb_function_instance *f_ser_inst[MAX_SERIAL_INSTANCES];
};

static int serial_function_init(struct android_usb_function *f, struct usb_composite_dev *cdev)
{
	int i;
	int ret;
	struct serial_function_config *config;

	config = kzalloc(sizeof(struct serial_function_config), GFP_KERNEL);
	if (!config)
		return -ENOMEM;
	f->config = config;

	for (i = 0; i < MAX_SERIAL_INSTANCES; i++) {
		config->f_ser_inst[i] = usb_get_function_instance("gser");
		if (IS_ERR(config->f_ser_inst[i])) {
			ret = PTR_ERR(config->f_ser_inst[i]);
			goto err_usb_get_function_instance;
		}
		config->f_ser[i] = usb_get_function(config->f_ser_inst[i]);
		if (IS_ERR(config->f_ser[i])) {
			ret = PTR_ERR(config->f_ser[i]);
			goto err_usb_get_function;
		}
	}
		return 0;
err_usb_get_function_instance:
	pr_err("Could not usb_get_function_instance() %d\n", i);
	while (i-- > 0) {
		usb_put_function(config->f_ser[i]);
err_usb_get_function:
		pr_err("Could not usb_get_function() %d\n", i);
		usb_put_function_instance(config->f_ser_inst[i]);
	}
	return ret;
}

static void serial_function_cleanup(struct android_usb_function *f)
{
	int i;
	struct serial_function_config *config = f->config;

	for (i = 0; i < MAX_SERIAL_INSTANCES; i++) {
		usb_put_function(config->f_ser[i]);
		usb_put_function_instance(config->f_ser_inst[i]);
	}
	kfree(f->config);
	f->config = NULL;
}

static int
serial_function_bind_config(struct android_usb_function *f, struct usb_configuration *c)
{
	int ret = 0;
	struct serial_function_config *config = f->config;

	ret = usb_add_function(c, config->f_ser[config->port_num]);
	if (ret) {
		pr_err("Could not bind ser%u config\n", config->port_num);
		goto err_usb_add_function;
	}
	pr_notice("%s Open /dev/ttyGS%d\n", __func__, config->port_num);

	return 0;

err_usb_add_function:
	usb_remove_function(c, config->f_ser[config->port_num]);
	return ret;
}

static ssize_t serial_port_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct serial_function_config *config = f->config;

	return sprintf(buf, "%d\n", config->port_num);
}

static ssize_t serial_port_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct serial_function_config *config = f->config;
	int ret;
	u32 value;

	ret = kstrtou32(buf, 10, &value);
	if (ret) {
		pr_notice("serial_port_store err, ret:%d\n", ret);
		return size;
	}
	if (value > MAX_SERIAL_INSTANCES)
		value = MAX_SERIAL_INSTANCES;
	config->port_num = value;
	return size;
}

static DEVICE_ATTR(port, S_IRUGO | S_IWUSR, serial_port_show, serial_port_store);
static struct device_attribute *serial_function_attributes[] = { &dev_attr_port, NULL };

static struct android_usb_function serial_function = {
	.name		= "gser",
	.init		= serial_function_init,
	.cleanup	= serial_function_cleanup,
	.bind_config	= serial_function_bind_config,
	.attributes	= serial_function_attributes,
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

static int cpumask_to_int(const struct cpumask *cpu_mask)
{
	int mask = 0;
	int cpu;

	for_each_cpu(cpu, cpu_mask) {
		pr_debug("[USB]%d\n", cpu);
		mask |= (1 << cpu);
	}

	return mask;
}

static ssize_t cpu_mask_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cpumask *cpu_mask = mtp_get_cpu_mask();

	return sprintf(buf, "0x%X\n", (cpu_mask?cpumask_to_int(cpu_mask):0xFFFFFFFF));
}

static ssize_t cpu_mask_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int mask;

	if (kstrtouint(buf, 16, &mask) != 0)
		return -EINVAL;

	pr_info("Store => 0x%x\n", mask);

	mtp_set_cpu_mask(mask);

	return size;
}

static DEVICE_ATTR(cpu_mask, S_IRUGO | S_IWUSR, cpu_mask_show,
					       cpu_mask_store);

static ssize_t mtp_server_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", mtp_get_mtp_server());
}

static DEVICE_ATTR(mtp_server, S_IRUGO, mtp_server_show,
					       NULL);

static struct device_attribute *mtp_function_attributes[] = {
	&dev_attr_cpu_mask,
	&dev_attr_mtp_server,
	NULL
};

static struct android_usb_function mtp_function = {
	.name		= "mtp",
	.init		= mtp_function_init,
	.cleanup	= mtp_function_cleanup,
	.bind_config	= mtp_function_bind_config,
	.ctrlrequest	= mtp_function_ctrlrequest,
	.attributes	= mtp_function_attributes,
};

/* PTP function is same as MTP with slightly different interface descriptor */
static struct android_usb_function ptp_function = {
	.name		= "ptp",
	.init		= ptp_function_init,
	.cleanup	= ptp_function_cleanup,
	.bind_config	= ptp_function_bind_config,
	.ctrlrequest	= mtp_function_ctrlrequest,
};
struct eem_function_config {
	u8      ethaddr[ETH_ALEN];
	char	manufacturer[256];
	struct eth_dev *dev;
};

static int
eem_function_init(struct android_usb_function *f,
		struct usb_composite_dev *cdev)
{
	f->config = kzalloc(sizeof(struct eem_function_config), GFP_KERNEL);
	if (!f->config)
		return -ENOMEM;
	return 0;
}

static void eem_function_cleanup(struct android_usb_function *f)
{
	kfree(f->config);
	f->config = NULL;
}

static int
eem_function_bind_config(struct android_usb_function *f,
		struct usb_configuration *c)
{
	int ret;
	struct eth_dev *dev;
	struct eem_function_config *eem = f->config;


	pr_notice("[USB]%s:\n", __func__);

	if (!eem) {
		pr_err("%s: rndis_pdata\n", __func__);
		return -1;
	}

	pr_notice("%s MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", __func__,
		eem->ethaddr[0], eem->ethaddr[1], eem->ethaddr[2],
		eem->ethaddr[3], eem->ethaddr[4], eem->ethaddr[5]);

	dev = gether_setup_name(c->cdev->gadget, dev_addr, host_addr,
			eem->ethaddr, qmult, "rndis");
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		pr_err("%s: gether_setup failed\n", __func__);
		return ret;
	}
	eem->dev = dev;

	return eem_bind_config(c, eem->dev);
}

static void eem_function_unbind_config(struct android_usb_function *f,
						struct usb_configuration *c)
{
	struct eem_function_config *eem = f->config;


	gether_cleanup(eem->dev);
}

static ssize_t eem_ethaddr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct eem_function_config *eem = f->config;


	return sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x\n",
		eem->ethaddr[0], eem->ethaddr[1], eem->ethaddr[2],
		eem->ethaddr[3], eem->ethaddr[4], eem->ethaddr[5]);
}

static ssize_t eem_ethaddr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct eem_function_config *eem = f->config;


	if (sscanf(buf, "%02x:%02x:%02x:%02x:%02x:%02x\n",
		    (int *)&eem->ethaddr[0], (int *)&eem->ethaddr[1],
		    (int *)&eem->ethaddr[2], (int *)&eem->ethaddr[3],
		    (int *)&eem->ethaddr[4], (int *)&eem->ethaddr[5]) == 6)
		return size;
	return -EINVAL;
}

static DEVICE_ATTR(eem_ethaddr, S_IRUGO | S_IWUSR, eem_ethaddr_show,
					       eem_ethaddr_store);

static struct device_attribute *eem_function_attributes[] = {
	&dev_attr_eem_ethaddr,
	NULL
};

static struct android_usb_function eem_function = {
	.name		= "eem",
	.init		= eem_function_init,
	.cleanup	= eem_function_cleanup,
	.bind_config	= eem_function_bind_config,
	.unbind_config	= eem_function_unbind_config,
	.attributes	= eem_function_attributes,
};

struct rndis_function_config {
	u8      ethaddr[ETH_ALEN];
	u32     vendorID;
	char	manufacturer[256];
	/* "Wireless" RNDIS; auto-detected by Windows */
	bool	wceis;
	struct eth_dev *dev;
#ifdef CONFIG_MTK_MD_DIRECT_TETHERING_SUPPORT
	bool	direct_feature_on;
	u8      direct_value;
#endif
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

	pr_notice("%s MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", __func__,
		rndis->ethaddr[0], rndis->ethaddr[1], rndis->ethaddr[2],
		rndis->ethaddr[3], rndis->ethaddr[4], rndis->ethaddr[5]);

	dev = gether_setup_name(c->cdev->gadget, dev_addr, host_addr,
			rndis->ethaddr, qmult, "rndis");
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

	return rndis_bind_config_vendor(c, rndis->ethaddr,
				rndis->vendorID, rndis->manufacturer, rndis->dev);
}

static void rndis_function_unbind_config(struct android_usb_function *f,
						struct usb_configuration *c)
{
	struct rndis_function_config *rndis = f->config;


	gether_cleanup(rndis->dev);
}

static ssize_t rndis_manufacturer_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;


	return sprintf(buf, "%s\n", config->manufacturer);
}

static ssize_t rndis_manufacturer_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;


	if (size >= sizeof(config->manufacturer))
		return -EINVAL;
	if (sscanf(buf, "%s", config->manufacturer) == 1)
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


	return sprintf(buf, "%d\n", config->wceis);
}

static ssize_t rndis_wceis_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;
	int value;


	if (kstrtoint(buf, 0, &value) == 0) {
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


	return sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x\n",
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

	return sprintf(buf, "%04x\n", config->vendorID);
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
#ifdef CONFIG_MTK_MD_DIRECT_TETHERING_SUPPORT
static ssize_t rndis_direct_feature_on_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;

	return sprintf(buf, "%d\n", config->direct_feature_on);
}

static ssize_t rndis_direct_feature_on_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;
	int value;
	int ret;

	ret = kstrtoint(buf, 10, &value);
	if (ret)
		return -EINVAL;

	if (value > 0)
		config->direct_feature_on = true;
	else
		config->direct_feature_on = false;
	return size;
}

static DEVICE_ATTR(direct_feature_on, S_IRUGO | S_IWUSR, rndis_direct_feature_on_show,
						rndis_direct_feature_on_store);

static ssize_t rndis_direct_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct android_usb_function *android_f = dev_get_drvdata(dev);
	struct rndis_function_config *config = android_f->config;

	if (config != NULL)
		return sprintf(buf, "%d\n", config->direct_value);
	else
		return sprintf(buf, "%d\n", 0);
}

static ssize_t rndis_direct_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct android_usb_function *android_f = dev_get_drvdata(dev);
	struct rndis_function_config *config = android_f->config;
	struct android_dev *pdev = _android_dev;
	struct usb_composite_dev *cdev = pdev->cdev;
	struct usb_function		*f;
	int value;
	int ret;

	ret = kstrtoint(buf, 10, &value);

	if (ret)
		return -EINVAL;

	if (!config)
		return -EFAULT;

	if (config->direct_feature_on)
		pr_info("%s value:%d->%d\n", __func__, config->direct_value, value);
	else
		pr_info("%s direct feature is false\n", __func__);

	if (config->direct_feature_on && value != config->direct_value) {
		config->direct_value = value;
		if (cdev->config != NULL) {
			list_for_each_entry(f, &cdev->config->functions, list) {
				if (f != NULL && !strcmp(f->name, "rndis")) {
					if (value == 0)
						rndis_set_direct_tethering(f, false);
					else if (value == 1)
						rndis_set_direct_tethering(f, true);
				}
			}
		}
	}

	return size;
}

static DEVICE_ATTR(direct, S_IRUGO | S_IWUSR, rndis_direct_show,
						rndis_direct_store);
#endif

static struct device_attribute *rndis_function_attributes[] = {
	&dev_attr_manufacturer,
	&dev_attr_wceis,
	&dev_attr_ethaddr,
	&dev_attr_vendorID,
#ifdef CONFIG_MTK_MD_DIRECT_TETHERING_SUPPORT
	&dev_attr_direct_feature_on,
	&dev_attr_direct,
#endif
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

struct mass_storage_function_config {
	struct usb_function *f_ms;
	struct usb_function_instance *f_ms_inst;
};
#define fsg_num_buffers	CONFIG_USB_GADGET_STORAGE_NUM_BUFFERS
static struct fsg_module_parameters fsg_mod_data;
FSG_MODULE_PARAMETERS(/* no prefix */, fsg_mod_data);

static int mass_storage_function_init(struct android_usb_function *f,
					struct usb_composite_dev *cdev)
{
	struct mass_storage_function_config *config;
	int ret, i;
	struct fsg_opts *fsg_opts;
	//struct fsg_config m_config;


	pr_debug("%s(): Inside\n", __func__);
	config = kzalloc(sizeof(struct mass_storage_function_config),
								GFP_KERNEL);
	if (!config)
		return -ENOMEM;
	f->config = config;

	config->f_ms_inst = usb_get_function_instance("mass_storage");
	if (IS_ERR(config->f_ms_inst)) {
		ret = PTR_ERR(config->f_ms_inst);
		goto err_usb_get_function_instance;
	}

	config->f_ms = usb_get_function(config->f_ms_inst);
	if (IS_ERR(config->f_ms)) {
		ret = PTR_ERR(config->f_ms);
		goto err_usb_get_function;
	}

	fsg_mod_data.file_count = 1;
	for (i = 0 ; i < fsg_mod_data.file_count; i++) {
		fsg_mod_data.file[i] = "";
		fsg_mod_data.removable[i] = true;
		fsg_mod_data.nofua[i] = true;
	}

	//fsg_config_from_params(&m_config, &fsg_mod_data, fsg_num_buffers);
	fsg_opts = fsg_opts_from_func_inst(config->f_ms_inst);


	/* note this is important for sysfs manipulation and this will be override when fsg_main_thread be created*/
	//ret = fsg_common_set_cdev(fsg_opts->common, cdev,
	//					m_config.can_stall);
	ret = 0;
	if (ret) {
		pr_err("%s(): error(%d) for fsg_common_set_cdev\n",
						__func__, ret);
	}

	/* this will affect lun create name */
	//fsg_common_set_sysfs(fsg_opts->common, true);
	//ret = fsg_common_create_luns(fsg_opts->common, &m_config);
    ret = 0;
	if (ret) {
		pr_err("%s(): error(%d) for fsg_common_create_luns\n",
						__func__, ret);
	}

	/* use default one currently */
	//fsg_common_set_inquiry_string(fsg_opts->common, m_config.vendor_name,
	//						m_config.product_name);

	/* SYSFS create */
	//fsg_sysfs_update(fsg_opts->common, f->dev, true);

	/* invoke thread */
	/* ret = fsg_common_run_thread(fsg_opts->common); */
	/*if (ret) */
	/*	return ret; */

	/* setup this to avoid create fsg thread in fsg_bind again */
	fsg_opts->no_configfs = false;

	return 0;


err_usb_get_function:
	usb_put_function_instance(config->f_ms_inst);

err_usb_get_function_instance:
	return ret;
}

static void mass_storage_function_cleanup(struct android_usb_function *f)
{
	struct mass_storage_function_config *config = f->config;

	/* release what we required */
	struct fsg_opts *fsg_opts;


	fsg_opts = fsg_opts_from_func_inst(config->f_ms_inst);
	//fsg_sysfs_update(fsg_opts->common, f->dev, false);

	usb_put_function(config->f_ms);
	usb_put_function_instance(config->f_ms_inst);

	kfree(f->config);
	f->config = NULL;
}

static int mass_storage_function_bind_config(struct android_usb_function *f,
						struct usb_configuration *c)
{
	struct mass_storage_function_config *config = f->config;
	int ret = 0;


	/* no_configfs :true, make fsg_bind skip for creating fsg thread */
	ret = usb_add_function(c, config->f_ms);
	if (ret)
		pr_err("Could not bind config\n");

	return 0;
}

static ssize_t mass_storage_inquiry_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct fsg_opts *fsg_opts;
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct mass_storage_function_config *config = f->config;


	fsg_opts = fsg_opts_from_func_inst(config->f_ms_inst);

	//return fsg_inquiry_show(fsg_opts->common, buf);
	return 0;
}

static ssize_t mass_storage_inquiry_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct fsg_opts *fsg_opts;
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct mass_storage_function_config *config = f->config;


	fsg_opts = fsg_opts_from_func_inst(config->f_ms_inst);

    return 0;
	//return fsg_inquiry_store(fsg_opts->common, buf, size);
}

static DEVICE_ATTR(inquiry_string, S_IRUGO | S_IWUSR,
					mass_storage_inquiry_show,
					mass_storage_inquiry_store);

static ssize_t mass_storage_bicr_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
    #if 0
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct mass_storage_function_config *config = f->config;
	struct fsg_opts *fsg_opts = fsg_opts_from_func_inst(config->f_ms_inst);
#endif
    return 0;
	//return fsg_bicr_show(fsg_opts->common, buf);
}

static ssize_t mass_storage_bicr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
    #if 0
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct mass_storage_function_config *config = f->config;
	struct fsg_opts *fsg_opts = fsg_opts_from_func_inst(config->f_ms_inst);
#endif
    return 0;
	//return fsg_bicr_store(fsg_opts->common, buf, size);
}

static DEVICE_ATTR(bicr, S_IRUGO | S_IWUSR,
					mass_storage_bicr_show,
					mass_storage_bicr_store);


static struct device_attribute *mass_storage_function_attributes[] = {
	&dev_attr_inquiry_string,
	&dev_attr_bicr,
	NULL
};

static struct android_usb_function mass_storage_function = {
	.name		= "mass_storage",
	.init		= mass_storage_function_init,
	.cleanup	= mass_storage_function_cleanup,
	.bind_config	= mass_storage_function_bind_config,
	.attributes	= mass_storage_function_attributes,
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


struct audio_source_function_config {
	struct usb_function *f_aud;
	struct usb_function_instance *f_aud_inst;
};

static int audio_source_function_init(struct android_usb_function *f,
			struct usb_composite_dev *cdev)
{
	struct audio_source_function_config *config;

	config = kzalloc(sizeof(*config), GFP_KERNEL);
	if (!config)
		return -ENOMEM;

	config->f_aud_inst = usb_get_function_instance("audio_source");
	if (IS_ERR(config->f_aud_inst))
		return PTR_ERR(config->f_aud_inst);

	config->f_aud = usb_get_function(config->f_aud_inst);
	if (IS_ERR(config->f_aud)) {
		usb_put_function_instance(config->f_aud_inst);
		return PTR_ERR(config->f_aud);
	}

	f->config = config;
	return 0;
}

static void audio_source_function_cleanup(struct android_usb_function *f)
{
	struct audio_source_function_config *config = f->config;


	usb_put_function(config->f_aud);
	usb_put_function_instance(config->f_aud_inst);

	kfree(f->config);
	f->config = NULL;
}

static int audio_source_function_bind_config(struct android_usb_function *f,
						struct usb_configuration *c)
{
	struct audio_source_function_config *config = f->config;


	return usb_add_function(c, config->f_aud);
}

static struct android_usb_function audio_source_function = {
	.name		= "audio_source",
	.init		= audio_source_function_init,
	.cleanup	= audio_source_function_cleanup,
	.bind_config	= audio_source_function_bind_config,
};


#ifdef CONFIG_MTK_ECCCI_C2K_TMP
static int rawbulk_function_init(struct android_usb_function *f,
					struct usb_composite_dev *cdev)
{
	return 0;
}

static void rawbulk_function_cleanup(struct android_usb_function *f)
{
	;
}

static int rawbulk_function_bind_config(struct android_usb_function *f,
						struct usb_configuration *c)
{
	char *i = f->name + strlen("via_");

	if (!strncmp(i, "modem", 5))
		return rawbulk_bind_config(c, RAWBULK_TID_MODEM);
	else if (!strncmp(i, "ets", 3))
		return rawbulk_bind_config(c, RAWBULK_TID_ETS);
	else if (!strncmp(i, "atc", 3))
		return rawbulk_bind_config(c, RAWBULK_TID_AT);
	else if (!strncmp(i, "pcv", 3))
		return rawbulk_bind_config(c, RAWBULK_TID_PCV);
	else if (!strncmp(i, "gps", 3))
		return rawbulk_bind_config(c, RAWBULK_TID_GPS);
	return -EINVAL;
}

static int rawbulk_function_modem_ctrlrequest(struct android_usb_function *f,
						struct usb_composite_dev *cdev,
						const struct usb_ctrlrequest *c)
{
	if ((c->bRequestType & USB_RECIP_MASK) == USB_RECIP_DEVICE &&
			(c->bRequestType & USB_TYPE_MASK) == USB_TYPE_VENDOR) {
		struct rawbulk_function *fn = rawbulk_lookup_function(RAWBULK_TID_MODEM);

		return rawbulk_function_setup(&fn->function, c);
	}
	return -1;
}

static struct android_usb_function rawbulk_modem_function = {
	.name		= "via_modem",
	.init		= rawbulk_function_init,
	.cleanup	= rawbulk_function_cleanup,
	.bind_config	= rawbulk_function_bind_config,
	.ctrlrequest	= rawbulk_function_modem_ctrlrequest,
};

static struct android_usb_function rawbulk_ets_function = {
	.name		= "via_ets",
	.init		= rawbulk_function_init,
	.cleanup	= rawbulk_function_cleanup,
	.bind_config	= rawbulk_function_bind_config,
};

static struct android_usb_function rawbulk_atc_function = {
	.name		= "via_atc",
	.init		= rawbulk_function_init,
	.cleanup	= rawbulk_function_cleanup,
	.bind_config	= rawbulk_function_bind_config,
};

static struct android_usb_function rawbulk_pcv_function = {
	.name		= "via_pcv",
	.init		= rawbulk_function_init,
	.cleanup	= rawbulk_function_cleanup,
	.bind_config	= rawbulk_function_bind_config,
};

static struct android_usb_function rawbulk_gps_function = {
	.name		= "via_gps",
	.init		= rawbulk_function_init,
	.cleanup	= rawbulk_function_cleanup,
	.bind_config	= rawbulk_function_bind_config,
};
#endif

#ifdef CONFIG_SND_RAWMIDI
static int midi_function_init(struct android_usb_function *f,
					struct usb_composite_dev *cdev)
{
	struct midi_alsa_config *config;

	config = kzalloc(sizeof(struct midi_alsa_config), GFP_KERNEL);
	f->config = config;
	if (!config)
		return -ENOMEM;
	config->card = -1;
	config->device = -1;
	return 0;
}

static void midi_function_cleanup(struct android_usb_function *f)
{
	kfree(f->config);
}

static int midi_function_bind_config(struct android_usb_function *f,
						struct usb_configuration *c)
{
	struct midi_alsa_config *config = f->config;

	return f_midi_bind_config(c, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1,
			MIDI_INPUT_PORTS, MIDI_OUTPUT_PORTS, MIDI_BUFFER_SIZE,
			MIDI_QUEUE_LENGTH, config);
}

static ssize_t midi_alsa_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct midi_alsa_config *config = f->config;

	/* print ALSA card and device numbers */
	return sprintf(buf, "%d %d\n", config->card, config->device);
}

static DEVICE_ATTR(alsa, S_IRUGO, midi_alsa_show, NULL);

static struct device_attribute *midi_function_attributes[] = {
	&dev_attr_alsa,
	NULL
};

static struct android_usb_function midi_function = {
	.name		= "midi",
	.init		= midi_function_init,
	.cleanup	= midi_function_cleanup,
	.bind_config	= midi_function_bind_config,
	.attributes	= midi_function_attributes,
};
#endif

static struct android_usb_function *supported_functions[] = {
	&ffs_function,
	&acm_function,
	&mtp_function,
	&ptp_function,
	&eem_function,
	&serial_function,
	&rndis_function,
	&mass_storage_function,
	&accessory_function,
	&audio_source_function,
#ifdef CONFIG_SND_RAWMIDI
	&midi_function,
#endif
#ifdef CONFIG_MTK_ECCCI_C2K_TMP
	&rawbulk_modem_function,
	&rawbulk_ets_function,
	&rawbulk_atc_function,
	&rawbulk_pcv_function,
	&rawbulk_gps_function,
#endif
#ifdef CONFIG_USB_F_SS_LB
	&loopback_function,
#endif
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
	&hid_function,
#endif
	NULL
};

struct device *create_function_device(char *name)
{
	struct android_dev *dev = _android_dev;
	struct android_usb_function **functions;
	struct android_usb_function *f;

	functions = dev->functions;


	while ((f = *functions++))
		if (!strcmp(name, f->dev_name))
			return f->dev;

	return ERR_PTR(-EINVAL);
}

static int android_init_functions(struct android_usb_function **functions,
				  struct usb_composite_dev *cdev)
{
	struct android_dev *dev = _android_dev;
	struct android_usb_function *f;
	struct device_attribute **attrs;
	struct device_attribute *attr;
	int err;
#ifdef CONFIG_USBIF_COMPLIANCE
	int index = 1;
#else
	int index = 0;
#endif

	for (; (f = *functions++); index++) {
		f->dev_name = kasprintf(GFP_KERNEL, "f_%s", f->name);
		pr_notice("[USB]%s: f->dev_name = %s, f->name = %s\n", __func__, f->dev_name, f->name);
		f->dev = device_create(android_class, dev->dev,
				MKDEV(0, index), f, f->dev_name);
		if (IS_ERR(f->dev)) {
			pr_err("%s: Failed to create dev %s", __func__,
							f->dev_name);
			err = PTR_ERR(f->dev);
			goto err_create;
		}

		if (f->init) {
			err = f->init(f, cdev);
			if (err) {
				pr_err("%s: Failed to init %s", __func__,
								f->name);
				goto err_out;
			} else
				pr_notice("[USB]%s: init %s success!!\n", __func__, f->name);
		}

		attrs = f->attributes;
		if (attrs) {
			while ((attr = *attrs++) && !err)
				err = device_create_file(f->dev, attr);
		}
		if (err) {
			pr_err("%s: Failed to create function %s attributes",
					__func__, f->name);
			goto err_out;
		}
	}
	return 0;

err_out:
	device_destroy(android_class, f->dev->devt);
err_create:
	kfree(f->dev_name);
	return err;
}

static void android_cleanup_functions(struct android_usb_function **functions)
{
	struct android_usb_function *f;


	while (*functions) {
		f = *functions++;

		if (f->dev) {
			device_destroy(android_class, f->dev->devt);
			kfree(f->dev_name);
		}

		if (f->cleanup)
			f->cleanup(f);
	}
}

static int
android_bind_enabled_functions(struct android_dev *dev,
			       struct usb_configuration *c)
{
	struct android_usb_function *f;
	int ret;


	list_for_each_entry(f, &dev->enabled_functions, enabled_list) {
		pr_notice("[USB]bind_config function '%s'/%p\n", f->name, f);
		ret = f->bind_config(f, c);
		if (ret) {
			pr_err("%s: %s failed", __func__, f->name);
			return ret;
		}
	}
	return 0;
}

static void
android_unbind_enabled_functions(struct android_dev *dev,
			       struct usb_configuration *c)
{
	struct android_usb_function *f;


	list_for_each_entry(f, &dev->enabled_functions, enabled_list) {
		if (f->unbind_config)
			f->unbind_config(f, c);
	}
}

static int android_enable_function(struct android_dev *dev, char *name)
{
	struct android_usb_function **functions = dev->functions;
	struct android_usb_function *f;


	while ((f = *functions++)) {
		if (!strcmp(name, f->name)) {
			list_add_tail(&f->enabled_list,
						&dev->enabled_functions);
			return 0;
		}
	}
	return -EINVAL;
}

/*-------------------------------------------------------------------------*/
/* /sys/class/android_usb/android%d/ interface */

static ssize_t
functions_show(struct device *pdev, struct device_attribute *attr, char *buf)
{
	struct android_dev *dev = dev_get_drvdata(pdev);
	struct android_usb_function *f;
	char *buff = buf;

	pr_notice("[USB]%s: ", __func__);
	mutex_lock(&dev->mutex);

	list_for_each_entry(f, &dev->enabled_functions, enabled_list)
		buff += sprintf(buff, "%s,", f->name);

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

	INIT_LIST_HEAD(&dev->enabled_functions);
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
	if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT || get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT) {
		pr_notice("[USB]KPOC, func%s\n", KPOC_USB_FUNC);
		err = android_enable_function(dev, KPOC_USB_FUNC);
		if (err)
			pr_err("android_usb: Cannot enable '%s' (%d)",
					KPOC_USB_FUNC, err);
		mutex_unlock(&dev->mutex);
		return size;
	}
#endif

	strlcpy(buf, buff, sizeof(buf));
	b = strim(buf);

	while (b) {
		name = strsep(&b, ",");
		pr_notice("[USB]%s: name = %s\n", __func__, name);
		if (!name)
			continue;

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
			err = android_enable_function(dev, "ffs");
			if (err)
				pr_err("android_usb: Cannot enable ffs (%d)",
									err);
			else
				ffs_enabled = 1;
			continue;
		}

		err = android_enable_function(dev, name);
		if (err)
			pr_err("android_usb: Cannot enable '%s' (%d)",
							   name, err);
	}

	mutex_unlock(&dev->mutex);

	return size;
}

static ssize_t enable_show(struct device *pdev, struct device_attribute *attr,
			   char *buf)
{
	struct android_dev *dev = dev_get_drvdata(pdev);


	return sprintf(buf, "%d\n", dev->enabled);
}

static ssize_t enable_store(struct device *pdev, struct device_attribute *attr,
			    const char *buff, size_t size)
{
	struct android_dev *dev = dev_get_drvdata(pdev);

	struct usb_composite_dev *cdev = dev->cdev;
	struct android_usb_function *f;
	int enabled = 0;
	int ret;

	if (!cdev)
		return -ENODEV;


	mutex_lock(&dev->mutex);

	pr_notice("[USB]%s: device_attr->attr.name: %s\n", __func__, attr->attr.name);

	ret = kstrtoint(buff, 0, &enabled);

	if (enabled && !dev->enabled) {
		/* ALPS01770952
		 * Reset next_string_id to 0 before enabling the gadget driver.
		 * Otherwise, after a large number of enable/disable cycles,
		 * function bind will fail because we cannot allocate new string ids.
		 * String ids cannot be larger than 254 per USB spec.
		 * 0~15 are reserved for usb device descriptor
		 * 16~254 are for functions.
		 */
		cdev->next_string_id = 0x10;

		/*
		 * Update values in composite driver's copy of
		 * device descriptor.
		 */
		cdev->desc.idVendor = device_desc.idVendor;
		cdev->desc.idProduct = device_desc.idProduct;
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
		if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT
				|| get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT) {
			pr_notice("[USB]KPOC, vid:%d, pid:%d\n", KPOC_USB_VENDOR_ID, KPOC_USB_PRODUCT_ID);
			cdev->desc.idVendor = cpu_to_le16(KPOC_USB_VENDOR_ID);
			cdev->desc.idProduct = cpu_to_le16(KPOC_USB_PRODUCT_ID);
		}
#endif
		cdev->desc.bcdDevice = device_desc.bcdDevice;
		cdev->desc.bDeviceClass = device_desc.bDeviceClass;
		cdev->desc.bDeviceSubClass = device_desc.bDeviceSubClass;
		cdev->desc.bDeviceProtocol = device_desc.bDeviceProtocol;

		/* special case for meta mode */
		if (serial_string[0] == 0x20)
			cdev->desc.iSerialNumber = 0;
		else
			cdev->desc.iSerialNumber = device_desc.iSerialNumber;

		list_for_each_entry(f, &dev->enabled_functions, enabled_list) {
			if (f->enable)
				f->enable(f);
		}
		android_enable(dev);
		dev->enabled = true;
		pr_notice("[USB]%s: enable 0->1 case, device_desc.idVendor = 0x%x, device_desc.idProduct = 0x%x\n",
				__func__, device_desc.idVendor, device_desc.idProduct);
#ifdef CONFIG_MTPROF
		{
			static int first_shot = 1;

			if (first_shot) {
				log_boot("USB ready");
				first_shot = 0;
			}
		}
#endif
	} else if (!enabled && dev->enabled) {
		pr_notice("[USB]%s: enable 1->0 case, device_desc.idVendor = 0x%x, device_desc.idProduct = 0x%x\n",
				__func__, device_desc.idVendor, device_desc.idProduct);
		android_disable(dev);
		list_for_each_entry(f, &dev->enabled_functions, enabled_list) {
			pr_notice("[USB]disable function '%s'/%p\n", f->name, f);
			if (f->disable)
				f->disable(f);
		}
		dev->enabled = false;
	} else {
		pr_err("android_usb: already %s\n",
				dev->enabled ? "enabled" : "disabled");
	}

	mutex_unlock(&dev->mutex);
	return size;
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
	pr_warn("[USB]%s, state:%s\n", __func__, state);
	spin_unlock_irqrestore(&cdev->lock, flags);
out:
	return sprintf(buf, "%s\n", state);
}

#define LOG_BUG_SZ 2048
static char log_buf[LOG_BUG_SZ];
static int log_buf_idx;
static ssize_t
log_show(struct device *pdev, struct device_attribute *attr, char *buf)
{
	struct android_dev *dev = dev_get_drvdata(pdev);

	mutex_lock(&dev->mutex);

	memcpy(buf, log_buf, log_buf_idx);

	mutex_unlock(&dev->mutex);
	return log_buf_idx;
}

static ssize_t
log_store(struct device *pdev, struct device_attribute *attr,
			       const char *buff, size_t size)
{
	struct android_dev *dev = dev_get_drvdata(pdev);
	char buf[256], n;

	mutex_lock(&dev->mutex);

	n = strlcpy(buf, buff, sizeof(buf));

	if ((log_buf_idx + (n + 1)) > LOG_BUG_SZ)
		log_buf_idx = 0;

	memcpy(log_buf + log_buf_idx, buf, n);
	log_buf_idx += n;
	log_buf[log_buf_idx++] = ' ';
	pr_warn("[USB]%s, <%s>, n:%d, log_buf_idx:%d\n", __func__, buf, n, log_buf_idx);

	mutex_unlock(&dev->mutex);
	return size;
}

#define DESCRIPTOR_ATTR(field, format_string)				\
static ssize_t								\
field ## _show(struct device *dev, struct device_attribute *attr,	\
		char *buf)						\
{									\
	return sprintf(buf, format_string, device_desc.field);		\
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
static DEVICE_ATTR(field, S_IRUGO | S_IWUSR, field ## _show, field ## _store)

#define DESCRIPTOR_STRING_ATTR(field, buffer)				\
static ssize_t								\
field ## _show(struct device *dev, struct device_attribute *attr,	\
		char *buf)						\
{									\
	return sprintf(buf, "%s", buffer);				\
}									\
static ssize_t								\
field ## _store(struct device *dev, struct device_attribute *attr,	\
		const char *buf, size_t size)				\
{									\
	if (size >= sizeof(buffer))					\
		return -EINVAL;						\
	return strlcpy(buffer, buf, sizeof(buffer));			\
}									\
static DEVICE_ATTR(field, S_IRUGO | S_IWUSR, field ## _show, field ## _store)


DESCRIPTOR_ATTR(idVendor, "%04x\n");
DESCRIPTOR_ATTR(idProduct, "%04x\n");
DESCRIPTOR_ATTR(bcdDevice, "%04x\n");
DESCRIPTOR_ATTR(bDeviceClass, "%d\n");
DESCRIPTOR_ATTR(bDeviceSubClass, "%d\n");
DESCRIPTOR_ATTR(bDeviceProtocol, "%d\n");
DESCRIPTOR_STRING_ATTR(iManufacturer, manufacturer_string);
DESCRIPTOR_STRING_ATTR(iProduct, product_string);
DESCRIPTOR_STRING_ATTR(iSerial, serial_string);

static DEVICE_ATTR(functions, S_IRUGO | S_IWUSR, functions_show,
						 functions_store);
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR, enable_show, enable_store);
static DEVICE_ATTR(state, S_IRUGO, state_show, NULL);
static DEVICE_ATTR(log, S_IRUGO | S_IWUSR, log_show,
						 log_store);

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
	&dev_attr_state,
	&dev_attr_log,
	NULL
};

/*-------------------------------------------------------------------------*/
/* Composite driver */

static int android_bind_config(struct usb_configuration *c)
{
	struct android_dev *dev = _android_dev;
	int ret = 0;

	ret = android_bind_enabled_functions(dev, c);
	if (ret)
		return ret;

	return 0;
}

static void android_unbind_config(struct usb_configuration *c)
{
	struct android_dev *dev = _android_dev;


	android_unbind_enabled_functions(dev, c);
}

static int android_setup_config(struct usb_configuration *c, const struct usb_ctrlrequest *ctrl)
{
	int handled = -EINVAL;
	const u8 recip = ctrl->bRequestType & USB_RECIP_MASK;


	pr_notice("%s bRequestType=%x, bRequest=%x, recip=%x\n", __func__, ctrl->bRequestType, ctrl->bRequest, recip);

	if (!((ctrl->bRequestType & USB_TYPE_MASK) == USB_TYPE_STANDARD))
		return handled;

	switch (ctrl->bRequest) {

	case USB_REQ_CLEAR_FEATURE:
		switch (recip) {
		case USB_RECIP_DEVICE:
			switch (ctrl->wValue) {
			case USB_DEVICE_U1_ENABLE:
				handled = 1;
				pr_notice("Clear Feature->U1 Enable\n");
				break;

			case USB_DEVICE_U2_ENABLE:
				handled = 1;
				pr_notice("Clear Feature->U2 Enable\n");
				break;

			default:
				handled = -EINVAL;
				break;
			}
			break;
		default:
			handled = -EINVAL;
			break;
		}
		break;

	case USB_REQ_SET_FEATURE:
		switch (recip) {
		case USB_RECIP_DEVICE:
			switch (ctrl->wValue) {
			case USB_DEVICE_U1_ENABLE:
				pr_notice("Set Feature->U1 Enable\n");
				handled = 1;
				break;
			case USB_DEVICE_U2_ENABLE:
				pr_notice("Set Feature->U2 Enable\n");
				handled = 1;
				break;
			default:
				handled = -EINVAL;
				break;
			}
			break;

		default:
			handled = -EINVAL;
			break;
		}
		break;

	default:
		handled = -EINVAL;
		break;
	}

	return handled;
}

static int android_bind(struct usb_composite_dev *cdev)
{
	struct android_dev *dev = _android_dev;
	struct usb_gadget	*gadget = cdev->gadget;
	int			id, ret;

	/* Save the default handler */
	dev->setup_complete = cdev->req->complete;

	/*
	 * Start disconnected. Userspace will connect the gadget once
	 * it is done configuring the functions.
	 */
	usb_gadget_disconnect(gadget);

	ret = android_init_functions(dev->functions, cdev);
	if (ret)
		return ret;

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
	strncpy(manufacturer_string, "Android", sizeof(manufacturer_string)-1);
	strncpy(product_string, "Android", sizeof(product_string) - 1);
	strncpy(serial_string, "0123456789ABCDEF", sizeof(serial_string) - 1);

	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_SERIAL_IDX].id = id;
	device_desc.iSerialNumber = id;

#ifdef CONFIG_USBIF_COMPLIANCE
	usb_gadget_clear_selfpowered(gadget);
#else
	usb_gadget_set_selfpowered(gadget);
#endif
	dev->cdev = cdev;

	return 0;
}

static int android_usb_unbind(struct usb_composite_dev *cdev)
{
	struct android_dev *dev = _android_dev;


	cancel_work_sync(&dev->work);
	android_cleanup_functions(dev->functions);
	return 0;
}

/* HACK: android needs to override setup for accessory to work */
static int (*composite_setup_func)(struct usb_gadget *gadget, const struct usb_ctrlrequest *c);

static int
android_setup(struct usb_gadget *gadget, const struct usb_ctrlrequest *c)
{
	struct android_dev		*dev = _android_dev;
	struct usb_composite_dev	*cdev = get_gadget_data(gadget);
	struct usb_request		*req = cdev->req;
	struct android_usb_function	*f;
	int value = -EOPNOTSUPP;
	unsigned long flags;


	req->zero = 0;
	req->length = 0;
	/* req->complete = dev->setup_complete; */
	req->complete = composite_setup_complete;
	gadget->ep0->driver_data = cdev;

	list_for_each_entry(f, &dev->enabled_functions, enabled_list) {
		if (f->ctrlrequest) {
			value = f->ctrlrequest(f, cdev, c);
			if (value >= 0)
				break;
		}
	}

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
		schedule_work(&dev->work);
	} else if (c->bRequest == USB_REQ_SET_CONFIGURATION &&
						cdev->config) {
		schedule_work(&dev->work);
	}
	spin_unlock_irqrestore(&cdev->lock, flags);

	return value;
}

static void android_disconnect(struct usb_composite_dev *cdev)
{
	struct android_dev *dev = _android_dev;

	/* accessory HID support can be active while the
	 * accessory function is not actually enabled,
	 * so we need to inform it when we are disconnected.
	 */

	acc_disconnect();

	dev->connected = 0;
	schedule_work(&dev->work);
	pr_notice("[USB]%s: dev->connected = %d\n", __func__, dev->connected);
}

static struct usb_composite_driver android_usb_driver = {
	.name		= "android_usb",
	.dev		= &device_desc,
	.strings	= dev_strings,
	.bind		= android_bind,
	.unbind		= android_usb_unbind,
	.disconnect	= android_disconnect,
#ifdef CONFIG_USB_MU3D_DRV
	.max_speed	= USB_SPEED_SUPER
#else
	.max_speed	= USB_SPEED_HIGH
#endif
};

#define USB_STATE_MONITOR_DELAY 3000
static struct delayed_work android_usb_state_monitor_work;
static void do_android_usb_state_monitor_work(struct work_struct *work)
{
	struct android_dev *dev = _android_dev;
	char *usb_state = "NO-DEV";

	if (dev && dev->cdev)
		usb_state = "DISCONNECTED";

	if (dev && dev->cdev && dev->cdev->config)
		usb_state = "CONFIGURED";

	pr_warn("usb_state<%s>\n", usb_state);
	schedule_delayed_work(&android_usb_state_monitor_work, msecs_to_jiffies(USB_STATE_MONITOR_DELAY));
}
void trigger_android_usb_state_monitor_work(void)
{
	static int inited;

	if (!inited) {
		/* TIMER_DEFERRABLE for not interfering with deep idle */
		INIT_DEFERRABLE_WORK(&android_usb_state_monitor_work, do_android_usb_state_monitor_work);
		inited = 1;
	}
	schedule_delayed_work(&android_usb_state_monitor_work, msecs_to_jiffies(USB_STATE_MONITOR_DELAY));

};

static int android_create_device(struct android_dev *dev)
{
	struct device_attribute **attrs = android_usb_attributes;
	struct device_attribute *attr;
	int err;


	dev->dev = device_create(android_class, NULL,
					MKDEV(0, 0), NULL, "android0");
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
#ifdef CONFIG_MTK_USB2JTAG_SUPPORT
	if (usb2jtag_mode())
		pr_err("[USB2JTAG] in usb2jtag mode, not to initialize usb driver\n");
	else
		trigger_android_usb_state_monitor_work();
#else
		trigger_android_usb_state_monitor_work();
#endif

	return 0;
}

#ifdef CONFIG_USBIF_COMPLIANCE

#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>


static int andoid_usbif_driver_on;

static int android_start(void)
{
	int err;

	pr_notice("android_start ===>\n");

	err = usb_composite_probe(&android_usb_driver);
	if (err)
		pr_err("%s: failed to probe driver %d", __func__, err);

	/* HACK: exchange composite's setup with ours */
	composite_setup_func = android_usb_driver.gadget_driver.setup;
	android_usb_driver.gadget_driver.setup = android_setup;

	pr_notice("android_start <===\n");

	return err;
}

static int android_stop(void)
{
	pr_notice("android_stop ===>\n");

	usb_composite_unregister(&android_usb_driver);

	pr_notice("android_stop <===\n");
	return 0;
}

static int andoid_usbif_proc_show(struct seq_file *seq, void *v)
{
	seq_printf(seq, "andoid_usbif_proc_show, andoid_usbif_driver_on is %d (on:1, off:0)\n", andoid_usbif_driver_on);
	return 0;
}

static int andoid_usbif_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, andoid_usbif_proc_show, inode->i_private);
}

static ssize_t andoid_usbif_proc_write(struct file *file, const char __user *buf, size_t length, loff_t *ppos)
{
	char msg[32];

	if (length >= sizeof(msg)) {
		pr_notice("andoid_usbif_proc_write length error, the error len is %d\n", (unsigned int)length);
		return -EINVAL;
	}
	if (copy_from_user(msg, buf, length))
		return -EFAULT;

	msg[length] = 0;

	pr_notice("andoid_usbif_proc_write: %s, current driver on/off: %d\n", msg, andoid_usbif_driver_on);

	if ((msg[0] == '1') && (andoid_usbif_driver_on == 0)) {
		pr_notice("start usb android driver ===>\n");
		pr_warn("start usb android driver ===>\n");
		android_start();
		andoid_usbif_driver_on = 1;
		pr_notice("start usb android driver <===\n");
	} else if ((msg[0] == '0') && (andoid_usbif_driver_on == 1)) {
		pr_notice("stop usb android driver ===>\n");
		pr_warn("stop usb android driver ===>\n");
		andoid_usbif_driver_on = 0;
		android_stop();

		pr_notice("stop usb android driver <===\n");
	}

	return length;
}

static const struct file_operations andoid_usbif_proc_fops = {
	.owner = THIS_MODULE,
	.open = andoid_usbif_proc_open,
	.write = andoid_usbif_proc_write,
	.read = seq_read,
	.llseek = seq_lseek,

};

static int __init init(void)
{
	struct android_dev *dev;
	int err;
	struct proc_dir_entry *prEntry;



	android_class = class_create(THIS_MODULE, "android_usb");
	if (IS_ERR(android_class))
		return PTR_ERR(android_class);

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		err = -ENOMEM;
		goto err_dev;
	}

	dev->disable_depth = 1;
	dev->functions = supported_functions;
	INIT_LIST_HEAD(&dev->enabled_functions);
	INIT_WORK(&dev->work, android_work);
	mutex_init(&dev->mutex);

	err = android_create_device(dev);
	if (err) {
		pr_err("%s: failed to create android device %d", __func__, err);
		goto err_create;
	}

	_android_dev = dev;

	prEntry = proc_create("android_usbif_init", 0644, NULL, &andoid_usbif_proc_fops);

	if (prEntry)
		pr_warn("create the android_usbif_init proc OK!\n");
	else
		pr_warn("[ERROR] create the android_usbif_init proc FAIL\n");

	/* set android up at boot up */
	android_start();
	andoid_usbif_driver_on = 1;

	return 0;

err_create:
	kfree(dev);
err_dev:
	class_destroy(android_class);
	return err;
}

late_initcall(init);


static void __exit cleanup(void)
{
	pr_warn("[U3D] android cleanup ===>\n");
	class_destroy(android_class);
	kfree(_android_dev);
	_android_dev = NULL;
	pr_warn("[U3D] android cleanup <===\n");
}
module_exit(cleanup);

#else
static int __init init(void)
{
	struct android_dev *dev;
	int err;

	android_class = class_create(THIS_MODULE, "android_usb");
	if (IS_ERR(android_class))
		return PTR_ERR(android_class);


	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		err = -ENOMEM;
		goto err_dev;
	}

	dev->disable_depth = 1;
	dev->functions = supported_functions;
	INIT_LIST_HEAD(&dev->enabled_functions);
	INIT_WORK(&dev->work, android_work);
	mutex_init(&dev->mutex);

	err = android_create_device(dev);
	if (err) {
		pr_err("%s: failed to create android device %d", __func__, err);
		goto err_create;
	}

	_android_dev = dev;

	err = usb_composite_probe(&android_usb_driver);
	if (err) {
		pr_err("%s: failed to probe driver %d", __func__, err);
		_android_dev = NULL;
		goto err_probe;
	}

	/* HACK: exchange composite's setup with ours */
	composite_setup_func = android_usb_driver.gadget_driver.setup;
	android_usb_driver.gadget_driver.setup = android_setup;

	return 0;

err_probe:
	device_destroy(android_class, dev->dev->devt);
err_create:
	kfree(dev);
err_dev:
	class_destroy(android_class);
	return err;
}
late_initcall(init);

static void __exit cleanup(void)
{
	usb_composite_unregister(&android_usb_driver);
	class_destroy(android_class);
	kfree(_android_dev);
	_android_dev = NULL;
}
module_exit(cleanup);
#endif
