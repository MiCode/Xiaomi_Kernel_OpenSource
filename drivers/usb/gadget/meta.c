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

#include "f_mass_storage.h"


MODULE_AUTHOR("Mike Lockwood");
MODULE_DESCRIPTION("Android Composite USB Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("2.0");

static const char longname[] = "Gadget Android";

/* Default vendor and product IDs, overridden by userspace */
#define VENDOR_ID		0x0E8D
#define PRODUCT_ID		0x0001

#ifdef CONFIG_MTK_BOOT
#include <mt-plat/mtk_boot_common.h>
#endif

#ifdef CONFIG_MTPROF
#include "bootprof.h"
#endif

static int quick_vcom_num;

struct android_usb_function {
	char *name;
	void *config;

	struct device *dev;
	char *dev_name;
	struct device_attribute **attributes;

	/* for android_dev.enabled_functions */
	struct list_head enabled_list;

	/* Optional: initialization during gadget bind */
	int (*init)(struct android_usb_function *f,
			struct usb_composite_dev *cdev);
	/* Optional: cleanup during gadget unbind */
	void (*cleanup)(struct android_usb_function *f);
	/* Optional: called when the function is added the list of
	 *		enabled functions
	 */
	void (*enable)(struct android_usb_function *f);
	/* Optional: called when it is removed */
	void (*disable)(struct android_usb_function *f);

	int (*bind_config)(struct android_usb_function *f,
			   struct usb_configuration *c);

	/* Optional: called when the configuration is removed */
	void (*unbind_config)(struct android_usb_function *f,
			      struct usb_configuration *c);
	/* Optional: handle ctrl requests before the device is configured */
	int (*ctrlrequest)(struct android_usb_function *f,
					struct usb_composite_dev *cdev,
					const struct usb_ctrlrequest *ctrl_req);
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
static int android_setup_config(struct usb_configuration *c,
		const struct usb_ctrlrequest *ctrl);

/* string IDs are assigned dynamically */
#define STRING_MANUFACTURER_IDX		0
#define STRING_PRODUCT_IDX		1
#define STRING_SERIAL_IDX		2

static char manufacturer_string[256];
static char product_string[256];
static char serial_str[256];

/* String Table */
static struct usb_string strings_dev[] = {
	[STRING_MANUFACTURER_IDX].s = manufacturer_string,
	[STRING_PRODUCT_IDX].s = product_string,
	[STRING_SERIAL_IDX].s = serial_str,
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
	.bmAttributes	= USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
#ifdef CONFIG_USB_MU3D_DRV
	/* for passing USB30CV Descriptor Test [USB3.0 devices]*/
	.MaxPower	= 192,
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
	char **uevent_envp = NULL;
	unsigned long flags;

	if (!cdev) {
		pr_notice("%s, !cdev\n", __func__);
		return;
	}

	spin_lock_irqsave(&cdev->lock, flags);
	if (cdev->config)
		uevent_envp = configured;
	else if (dev->connected != dev->sw_connected)
		uevent_envp = dev->connected ? connected : disconnected;
	dev->sw_connected = dev->connected;
	spin_unlock_irqrestore(&cdev->lock, flags);

	if (uevent_envp) {
		kobject_uevent_env(&dev->dev->kobj, KOBJ_CHANGE, uevent_envp);
		pr_notice("%s: sent uevent %s\n", __func__, uevent_envp[0]);
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
		pr_notice("%s: did not send uevent (%d %d %p)\n", __func__,
			 dev->connected, dev->sw_connected, cdev->config);
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
	pr_info("Could not usb_get_function_instance() %d\n", i);
	while (i-- > 0) {
		usb_put_function(config->f_acm[i]);
err_usb_get_function:
		pr_info("Could not usb_get_function() %d\n", i);
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
		if (config->port_index[i] != 0
				|| (quick_vcom_num & (0x1 << i))) {
			ret = usb_add_function(c, config->f_acm[i]);
			if (ret) {
				pr_info("Could not bind acm%u config\n", i);
				goto err_usb_add_function;
			}
			pr_notice("%s Open /dev/ttyGS%d\n", __func__, i);
			config->port_index[i] = 0;
			config->port_index_on[i] = 1;
			config->instances = 0;
		}
	}

	quick_vcom_num = 0;

	config->instances_on = config->instances;
	for (i = 0; i < config->instances_on; i++) {
		ret = usb_add_function(c, config->f_acm[i]);
		if (ret) {
			pr_info("Could not bind acm%u config\n", i);
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

static struct android_usb_function acm_function = {
	.name		= "acm",
	.init		= acm_function_init,
	.cleanup	= acm_function_cleanup,
	.bind_config	= acm_function_bind_config,
	.unbind_config	= acm_function_unbind_config,
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
	struct fsg_config m_config;


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

	fsg_config_from_params(&m_config, &fsg_mod_data, fsg_num_buffers);
	fsg_opts = fsg_opts_from_func_inst(config->f_ms_inst);

	ret = fsg_common_set_cdev(fsg_opts->common, cdev,
						m_config.can_stall);
	if (ret) {
		pr_info("%s(): error(%d) for fsg_common_set_cdev\n",
						__func__, ret);
	}

	/* this will affect lun create name */
	fsg_common_set_sysfs(fsg_opts->common, true);
	ret = fsg_common_create_luns(fsg_opts->common, &m_config);
	if (ret) {
		pr_info("%s(): error(%d) for fsg_common_create_luns\n",
						__func__, ret);
	}

	/* use default one currently */
	fsg_common_set_inquiry_string(fsg_opts->common, m_config.vendor_name,
							m_config.product_name);

	/* SYSFS create */
	fsg_sysfs_update(fsg_opts->common, f->dev, true);

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
	fsg_sysfs_update(fsg_opts->common, f->dev, false);

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
		pr_info("Could not bind config\n");

	return 0;
}

static ssize_t mass_storage_inquiry_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct fsg_opts *fsg_opts;
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct mass_storage_function_config *config = f->config;


	fsg_opts = fsg_opts_from_func_inst(config->f_ms_inst);

	return fsg_inquiry_show(fsg_opts->common, buf);
}

static ssize_t mass_storage_inquiry_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct fsg_opts *fsg_opts;
	struct android_usb_function *f = dev_get_drvdata(dev);
	struct mass_storage_function_config *config = f->config;


	fsg_opts = fsg_opts_from_func_inst(config->f_ms_inst);

	return fsg_inquiry_store(fsg_opts->common, buf, size);
}

static DEVICE_ATTR(inquiry_string, 0644,
					mass_storage_inquiry_show,
					mass_storage_inquiry_store);

static struct device_attribute *mass_storage_function_attributes[] = {
	&dev_attr_inquiry_string,
	NULL
};

static struct android_usb_function mass_storage_function = {
	.name		= "mass_storage",
	.init		= mass_storage_function_init,
	.cleanup	= mass_storage_function_cleanup,
	.bind_config	= mass_storage_function_bind_config,
	.attributes	= mass_storage_function_attributes,
};

static struct android_usb_function *supported_functions[] = {
	&acm_function,
	&mass_storage_function,
	NULL
};

static int android_init_functions(struct android_usb_function **functions,
				  struct usb_composite_dev *cdev)
{
	struct android_dev *dev = _android_dev;
	struct android_usb_function *f;
	struct device_attribute **attrs;
	struct device_attribute *attr;
	int err = 0;
	int index = 0;

	for (; (f = *functions++); index++) {
		f->dev_name = kasprintf(GFP_KERNEL, "f_%s", f->name);
		pr_notice("[USB]%s: f->dev_name = %s, f->name = %s\n",
				__func__, f->dev_name, f->name);
		f->dev = device_create(android_class, dev->dev,
				MKDEV(0, index), f, f->dev_name);
		if (IS_ERR(f->dev)) {
			pr_info("%s: Failed to create dev %s", __func__,
							f->dev_name);
			err = PTR_ERR(f->dev);
			goto err_create;
		}

		if (f->init) {
			err = f->init(f, cdev);
			if (err) {
				pr_info("%s: Failed to init %s", __func__,
								f->name);
				goto err_out;
			} else
				pr_notice("[USB]%s: init %s success!!\n",
						__func__, f->name);
		}

		attrs = f->attributes;
		if (attrs) {
			while ((attr = *attrs++) && !err)
				err = device_create_file(f->dev, attr);
		}
		if (err) {
			pr_info("%s: Failed to create function %s attributes",
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
			pr_info("%s: %s failed", __func__, f->name);
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

static int android_enable_function(struct android_dev *dev, const char *name)
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
				pr_info("android_usb: Cannot enable ffs (%d)",
									err);
			else
				ffs_enabled = 1;
			continue;
		}

		err = android_enable_function(dev, name);
		if (err)
			pr_info("android_usb: Cannot enable '%s' (%d)",
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

	pr_notice("[USB]%s: %d %d\n", __func__, enabled, dev->enabled);

	ret = kstrtoint(buff, 0, &enabled);

	if (enabled && !dev->enabled) {
		/* ALPS01770952
		 * Reset next_string_id to 0 before enabling the gadget driver.
		 * Otherwise, after a large number of enable/disable cycles,
		 * function bind will fail because
		 * we cannot allocate new string ids.
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
		cdev->desc.bcdDevice = device_desc.bcdDevice;
		cdev->desc.bDeviceClass = device_desc.bDeviceClass;
		cdev->desc.bDeviceSubClass = device_desc.bDeviceSubClass;
		cdev->desc.bDeviceProtocol = device_desc.bDeviceProtocol;

		/* special case for meta mode */
		if (strlen(serial_str) == 0)
			cdev->desc.iSerialNumber = 0;
		else
			cdev->desc.iSerialNumber = device_desc.iSerialNumber;

		list_for_each_entry(f, &dev->enabled_functions, enabled_list) {
			if (f->enable)
				f->enable(f);
		}
		android_enable(dev);
		dev->enabled = true;
		pr_notice("[USB]%s: enable 0->1 case, idVendor=0x%x, idProduct=0x%x\n",
				__func__,
				device_desc.idVendor, device_desc.idProduct);
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
		pr_notice("[USB]%s: enable 1->0 case, idVendor=0x%x, idProduct=0x%x\n",
				__func__,
				device_desc.idVendor, device_desc.idProduct);
		android_disable(dev);
		list_for_each_entry(f, &dev->enabled_functions, enabled_list) {
			pr_notice("[USB]disable function '%s'/%p\n",
					f->name, f);
			if (f->disable)
				f->disable(f);
		}
		dev->enabled = false;
	} else {
		pr_info("android_usb: already %s\n",
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
	pr_info("[USB]%s, state:%s\n", __func__, state);
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
	pr_info("[USB]%s, <%s>, n:%d, log_buf_idx:%d\n",
			__func__, buf, n, log_buf_idx);

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
static DEVICE_ATTR(field, 0444, field ## _show, field ## _store)

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
static DEVICE_ATTR(field, 0444, field ## _show, field ## _store)


DESCRIPTOR_ATTR(idVendor, "%04x\n");
DESCRIPTOR_ATTR(idProduct, "%04x\n");
DESCRIPTOR_ATTR(bcdDevice, "%04x\n");
DESCRIPTOR_ATTR(bDeviceClass, "%d\n");
DESCRIPTOR_ATTR(bDeviceSubClass, "%d\n");
DESCRIPTOR_ATTR(bDeviceProtocol, "%d\n");
DESCRIPTOR_STRING_ATTR(iManufacturer, manufacturer_string);
DESCRIPTOR_STRING_ATTR(iProduct, product_string);
DESCRIPTOR_STRING_ATTR(iSerial, serial_str);

static DEVICE_ATTR(functions, 0444, functions_show,
						 functions_store);
static DEVICE_ATTR(enable, 0444, enable_show, enable_store);
static DEVICE_ATTR(state, 0444, state_show, NULL);
static DEVICE_ATTR(log, 0644, log_show,
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

static int android_setup_config(struct usb_configuration *c,
		const struct usb_ctrlrequest *ctrl)
{
	int handled = -EINVAL;
	const u8 recip = ctrl->bRequestType & USB_RECIP_MASK;


	pr_notice("%s bRequestType=%x, bRequest=%x, recip=%x\n",
			__func__, ctrl->bRequestType, ctrl->bRequest, recip);

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
	strncpy(serial_str, "0123456789ABCDEF", sizeof(serial_str) - 1);

	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_SERIAL_IDX].id = id;
	device_desc.iSerialNumber = id;

	usb_gadget_set_selfpowered(gadget);

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
static int (*composite_setup_func)(struct usb_gadget *gadget,
		const struct usb_ctrlrequest *c);

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

	pr_info("usb_state<%s>\n", usb_state);
	schedule_delayed_work(&android_usb_state_monitor_work,
			msecs_to_jiffies(USB_STATE_MONITOR_DELAY));
}
void trigger_android_usb_state_monitor_work(void)
{
	static int inited;

#ifdef CONFIG_FPGA_EARLY_PORTING
	pr_info("SKIP %s\n", __func__);
	return;
#endif
	if (!inited) {
		/* TIMER_DEFERRABLE for not interfering with deep idle */
		INIT_DEFERRABLE_WORK(&android_usb_state_monitor_work,
				do_android_usb_state_monitor_work);
		inited = 1;
	}
	schedule_delayed_work(&android_usb_state_monitor_work,
			msecs_to_jiffies(USB_STATE_MONITOR_DELAY));

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

	trigger_android_usb_state_monitor_work();

	return 0;
}

void enable_meta_vcom(int mode)
{
	struct android_dev *dev = _android_dev;
	struct device *pdev = dev->dev;

	enable_store(pdev, NULL, "0", 1);

	if (mode == 1) {
		strncpy(serial_str, "", sizeof(serial_str) - 1);
		device_desc.idVendor = 0x0e8d;
		device_desc.idProduct = 0x2007;
		device_desc.bDeviceClass = 0x02;

		/*ttyGS0*/
		quick_vcom_num = (1 << 0);

		functions_store(pdev, NULL, "acm", 3);
	} else if (mode == 2) {


		strncpy(serial_str, "", sizeof(serial_str) - 1);
		device_desc.idVendor = 0x0e8d;
		device_desc.idProduct = 0x202d;

		/*ttyGS0 + ttyGS3*/
		quick_vcom_num = (1 << 0) + (1 << 3);

		functions_store(pdev, NULL, "mass_storage,acm", 16);
	}

	enable_store(pdev, NULL, "1", 1);
}

static const char TAG_NAME[] = "androidboot.usbconfig=";

int acm_shortcut(void)
{
	char *ptr;
	char mode = 0;

#ifdef CONFIG_MTK_BOOT
	if (get_boot_mode() != META_BOOT)
		return 0;
#else
	return 0;
#endif

	ptr = strstr(saved_command_line, TAG_NAME);
	if (ptr) {
		mode = *(ptr + strlen(TAG_NAME));
		if (mode == '1')
			return 1;
		else if (mode == '2')
			return 2;

		pr_notice("not fast mode [%c]\n", mode);
	} else
		pr_notice("cat not find \"androidboot.usbconfig=\" in cmdline\n");
	return 0;
}

static int __init meta_usb_init(void)
{
	struct android_dev *dev;
	int err;
	int config = 0;

	config = acm_shortcut();

	if (!config)
		return 0;

	pr_info("%s: config %d", __func__, config);

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
		pr_info("%s: failed to create android device %d\n",
				__func__, err);
		goto err_create;
	}

	_android_dev = dev;

	err = usb_composite_probe(&android_usb_driver);
	if (err) {
		pr_info("%s: failed to probe driver %d\n",
				__func__, err);
		_android_dev = NULL;
		goto err_probe;
	}

	/* HACK: exchange composite's setup with ours */
	composite_setup_func = android_usb_driver.gadget_driver.setup;
	android_usb_driver.gadget_driver.setup = android_setup;

	enable_meta_vcom(config);

	return 0;

err_probe:
	device_destroy(android_class, dev->dev->devt);
err_create:
	kfree(dev);
err_dev:
	class_destroy(android_class);
	return err;
}
late_initcall(meta_usb_init);

static void __exit cleanup(void)
{
	usb_composite_unregister(&android_usb_driver);
	class_destroy(android_class);
	kfree(_android_dev);
	_android_dev = NULL;
}
module_exit(cleanup);

