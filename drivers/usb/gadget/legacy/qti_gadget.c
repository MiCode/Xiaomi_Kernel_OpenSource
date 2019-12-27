/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
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
#include <linux/property.h>
#include <linux/usb/composite.h>
#include <linux/platform_device.h>

struct qti_usb_function {
	struct usb_function_instance *fi;
	struct usb_function *f;

	struct list_head list;
};

#define MAX_FUNC_NAME_LEN	48
#define MAX_CFG_NAME_LEN       128

struct qti_usb_config {
	struct usb_configuration c;

	/* List of functions bound to this config */
	struct list_head func_list;
	/* List of qti_usb_functions bound to this config */
	struct list_head qti_funcs;
};

struct qti_usb_gadget {
	struct usb_composite_dev cdev;
	struct usb_composite_driver composite;

	const char *composition_funcs;
	bool enabled;
	struct device *dev;
};

static char manufacturer_string[256] = "Qualcomm Technologies, Inc";
module_param_string(manufacturer, manufacturer_string,
		    sizeof(manufacturer_string), 0644);
MODULE_PARM_DESC(quirks, "String representing name of manufacturer");

static char product_string[256] = "USB_device_SN:12345";
module_param_string(product, product_string,
		    sizeof(product_string), 0644);
MODULE_PARM_DESC(quirks, "String representing product name");

static char serialno_string[256] = "12345";
module_param_string(serialno, serialno_string,
		    sizeof(serialno_string), 0644);
MODULE_PARM_DESC(quirks, "String representing name of manufacturer");

/* String Table */
static struct usb_string strings_dev[] = {
	[USB_GADGET_MANUFACTURER_IDX].s = manufacturer_string,
	[USB_GADGET_PRODUCT_IDX].s = product_string,
	[USB_GADGET_SERIAL_IDX].s = serialno_string,
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

static void qti_configs_remove_funcs(struct qti_usb_gadget *qg)
{
	struct usb_configuration	*c;

	list_for_each_entry(c, &qg->cdev.configs, list) {
		struct qti_usb_config *cfg;
		struct usb_function *f, *tmp;

		cfg = container_of(c, struct qti_usb_config, c);

		list_for_each_entry_safe_reverse(f, tmp, &c->functions, list) {

			list_move(&f->list, &cfg->func_list);
			if (f->unbind) {
				dev_dbg(&qg->cdev.gadget->dev,
					"unbind function '%s'/%pK\n",
					     f->name, f);
				f->unbind(c, f);
			}
		}
		c->fullspeed = 0;
		c->highspeed = 0;
		c->superspeed = 0;
		c->superspeed_plus = 0;
		c->next_interface_id = 0;
		memset(c->interface, 0, sizeof(c->interface));
	}
}

static int qti_composite_bind(struct usb_gadget *gadget,
		struct usb_gadget_driver *gdriver)
{
	struct usb_composite_driver *composite = to_cdriver(gdriver);
	struct qti_usb_gadget *qg = container_of(composite,
				     struct qti_usb_gadget, composite);
	struct usb_composite_dev *cdev = &qg->cdev;
	struct usb_configuration *c;
	struct usb_string *s;
	int ret = -EINVAL;

	cdev->gadget = gadget;
	set_gadget_data(gadget, cdev);
	spin_lock_init(&qg->cdev.lock);

	ret = composite_dev_prepare(composite, cdev);
	if (ret)
		return ret;

	if (list_empty(&cdev->configs)) {
		pr_err("No configurations found in %s.\n", composite->name);
		ret = -EINVAL;
		goto composite_cleanup;
	}

	list_for_each_entry(c, &cdev->configs, list) {
		struct qti_usb_config *qcfg;

		qcfg = container_of(c, struct qti_usb_config, c);
		if (list_empty(&qcfg->func_list)) {
			pr_err("Config %s/%d of %s doesn't have a function.\n",
			      c->label, c->bConfigurationValue,
			      qg->composite.name);
			goto composite_cleanup;
		}
	}

	s = usb_gstrings_attach(cdev, dev_strings, USB_GADGET_FIRST_AVAIL_IDX);
	if (IS_ERR(s)) {
		ret = PTR_ERR(s);
		goto composite_cleanup;
	}

	cdev->desc.iManufacturer = s[USB_GADGET_MANUFACTURER_IDX].id;
	cdev->desc.iProduct = s[USB_GADGET_PRODUCT_IDX].id;
	cdev->desc.iSerialNumber = s[USB_GADGET_SERIAL_IDX].id;

	/* Go through all configs, attach all functions */
	list_for_each_entry(c, &qg->cdev.configs, list) {
		struct qti_usb_config *qcfg;
		struct usb_function *f, *tmp;

		qcfg = container_of(c, struct qti_usb_config, c);

		list_for_each_entry_safe(f, tmp, &qcfg->func_list, list) {
			list_del(&f->list);
			ret = usb_add_function(c, f);
			if (ret) {
				list_add(&f->list, &qcfg->func_list);
				goto remove_funcs;
			}
		}
		usb_ep_autoconfig_reset(cdev->gadget);
	}

	usb_ep_autoconfig_reset(cdev->gadget);

	return 0;

remove_funcs:
	qti_configs_remove_funcs(qg);
composite_cleanup:
	composite_dev_cleanup(cdev);
	return ret;
}

static void qti_composite_unbind(struct usb_gadget *gadget)
{
	struct usb_composite_dev *cdev;
	struct qti_usb_gadget *qg;

	cdev = get_gadget_data(gadget);
	qg = container_of(cdev, struct qti_usb_gadget, cdev);

	qti_configs_remove_funcs(qg);
	composite_dev_cleanup(cdev);
	usb_ep_autoconfig_reset(cdev->gadget);
	cdev->gadget = NULL;
	set_gadget_data(gadget, NULL);
}

static const struct usb_gadget_driver qti_gadget_driver = {
	.bind           = qti_composite_bind,
	.unbind         = qti_composite_unbind,
	.setup          = composite_setup,
	.reset          = composite_disconnect,
	.disconnect     = composite_disconnect,
	.suspend	= composite_suspend,
	.resume		= composite_resume,

	.max_speed	= USB_SPEED_SUPER_PLUS,
	.driver = {
		.owner          = THIS_MODULE,
		.name		= "qti-gadget",
	},
};

static void qti_usb_funcs_free(struct qti_usb_config *qcfg)
{
	struct usb_function *f, *tmp;
	struct qti_usb_function *qf, *qf_tmp;

	list_for_each_entry_safe(f, tmp, &qcfg->func_list, list) {
		list_del(&f->list);
		usb_put_function(f);

		/* find corresponding function_instance and free it */
		list_for_each_entry_safe(qf, qf_tmp, &qcfg->qti_funcs, list) {
			if (qf->f == f) {
				list_del(&qf->list);
				usb_put_function_instance(qf->fi);
				kfree(qf);
				break;
			}
		}
	}
}

static void qti_cleanup_configs_funcs(struct qti_usb_gadget *qg)
{
	struct usb_configuration *c, *c_tmp;

	list_for_each_entry_safe(c, c_tmp, &qg->cdev.configs, list) {
		struct qti_usb_config *qcfg;

		qcfg = container_of(c, struct qti_usb_config, c);
		WARN_ON(!list_empty(&qcfg->c.functions));

		qti_usb_funcs_free(qcfg);

		list_del(&qcfg->c.list);
		kfree(qcfg->c.label);
		kfree(qcfg);
	}
}

static int qti_usb_func_alloc(struct qti_usb_config *qcfg,
				   const char *name)
{
	struct qti_usb_function *qf;
	struct usb_function_instance *fi;
	struct usb_function *f;
	char buf[MAX_FUNC_NAME_LEN];
	char *func_name;
	char *instance_name;
	int ret;

	ret = snprintf(buf, MAX_FUNC_NAME_LEN, "%s", name);
	if (ret >= MAX_FUNC_NAME_LEN)
		return -ENAMETOOLONG;

	func_name = buf;
	instance_name = strnchr(func_name, MAX_FUNC_NAME_LEN, '.');
	if (!instance_name) {
		pr_err("Can't find . in <func>.<instance>:%s\n", buf);
		return -EINVAL;
	}
	*instance_name = '\0';
	instance_name++;

	qf = kzalloc(sizeof(*qf), GFP_KERNEL);
	if (!qf)
		return -ENOMEM;

	fi = usb_get_function_instance(func_name);
	if (IS_ERR(fi)) {
		kfree(qf);
		return PTR_ERR(fi);
	}
	qf->fi = fi;

	if (fi->set_inst_name) {
		ret = fi->set_inst_name(fi, instance_name);
		if (ret) {
			kfree(qf);
			usb_put_function_instance(fi);
			return ret;
		}
	}

	f = usb_get_function(fi);
	if (IS_ERR(f)) {
		kfree(qf);
		usb_put_function_instance(fi);
		return PTR_ERR(f);
	}
	qf->f = f;
	list_add_tail(&qf->list, &qcfg->qti_funcs);

	/* stash the function until we bind it to the gadget */
	list_add_tail(&f->list, &qcfg->func_list);

	return 0;
}

static int qti_usb_funcs_alloc(struct qti_usb_config *qcfg,
				const char *funcs)
{
	char buf[MAX_CFG_NAME_LEN];
	char *fn_name, *next_fn;
	int ret = 0;

	ret = snprintf(buf, MAX_CFG_NAME_LEN, "%s", funcs);
	if (ret >= MAX_CFG_NAME_LEN)
		return -ENAMETOOLONG;

	fn_name = buf;
	while (fn_name) {
		next_fn = strnchr(fn_name, MAX_CFG_NAME_LEN, ',');
		if (next_fn)
			*next_fn++ = '\0';

		ret = qti_usb_func_alloc(qcfg, fn_name);
		if (ret) {
			qti_usb_funcs_free(qcfg);
			break;
		}

		fn_name = next_fn;
	};

	return ret;
}

static int qti_usb_config_add(struct qti_usb_gadget *gadget,
				  const char *name, u8 num)
{
	struct qti_usb_config *qcfg;
	int ret = 0;

	qcfg = kzalloc(sizeof(*qcfg), GFP_KERNEL);
	if (!qcfg)
		return -ENOMEM;

	qcfg->c.label = kstrdup(name, GFP_KERNEL);
	if (!qcfg->c.label) {
		ret = -ENOMEM;
		goto free_cfg;
	}
	qcfg->c.bConfigurationValue = num;
	qcfg->c.bmAttributes = USB_CONFIG_ATT_ONE;
	qcfg->c.MaxPower = CONFIG_USB_GADGET_VBUS_DRAW;
	INIT_LIST_HEAD(&qcfg->func_list);
	INIT_LIST_HEAD(&qcfg->qti_funcs);

	ret = usb_add_config_only(&gadget->cdev, &qcfg->c);
	if (ret)
		goto free_label;

	ret = qti_usb_funcs_alloc(qcfg, name);
	if (ret)
		goto cfg_del;

	return ret;

cfg_del:
	list_del(&qcfg->c.list);
free_label:
	kfree(qcfg->c.label);
free_cfg:
	kfree(qcfg);
	return ret;

}

static int qti_usb_configs_make(struct qti_usb_gadget *gadget,
				  const char *cfgs)
{
	char buf[MAX_CFG_NAME_LEN];
	char *cfg_name, *next_cfg;
	int ret = 0;
	u8 num = 1;

	ret = snprintf(buf, MAX_CFG_NAME_LEN, "%s", cfgs);
	if (ret >= MAX_CFG_NAME_LEN)
		return -ENAMETOOLONG;

	cfg_name = buf;
	while (cfg_name) {
		next_cfg = strnchr(cfg_name, MAX_CFG_NAME_LEN, '|');
		if (next_cfg)
			*next_cfg++ = '\0';

		ret = qti_usb_config_add(gadget, cfg_name, num);
		if (ret)
			break;

		cfg_name = next_cfg;
		num++;
	};

	return ret;
}

static int qti_gadget_register(struct qti_usb_gadget *qg)
{
	int ret;

	if (qg->enabled)
		return -EINVAL;

	ret = qti_usb_configs_make(qg, qg->composition_funcs);
	if (ret)
		return ret;

	qg->cdev.desc.bLength = USB_DT_DEVICE_SIZE;
	qg->cdev.desc.bDescriptorType = USB_DT_DEVICE;
	qg->cdev.desc.bcdDevice = cpu_to_le16(get_default_bcdDevice());

	qg->composite.gadget_driver = qti_gadget_driver;
	qg->composite.max_speed = qti_gadget_driver.max_speed;

	qg->composite.gadget_driver.function = kstrdup("qti-gadget",
							GFP_KERNEL);
	qg->composite.name = qg->composite.gadget_driver.function;

	if (!qg->composite.gadget_driver.function) {
		ret = -ENOMEM;
		goto free_configs;
	}

	ret = usb_gadget_probe_driver(&qg->composite.gadget_driver);
	if (ret)
		goto free_name;

	qg->enabled = true;

	return 0;

free_name:
	kfree(qg->composite.gadget_driver.function);
free_configs:
	qti_cleanup_configs_funcs(qg);

	return ret;
}

static void qti_gadget_unregister(struct qti_usb_gadget *qg)
{
	if (!qg->enabled)
		return;

	usb_gadget_unregister_driver(&qg->composite.gadget_driver);
	kfree(qg->composite.gadget_driver.function);
	qti_cleanup_configs_funcs(qg);

	qg->enabled = false;
}

static int qti_gadget_get_properties(struct qti_usb_gadget *gadget)
{
	struct device *dev = gadget->dev;
	int ret, val;

	ret = device_property_read_string(dev, "qcom,composition",
				    &gadget->composition_funcs);
	if (ret) {
		dev_err(dev, "USB gadget composition not specified\n");
		return ret;
	}

	/* bail out if ffs is specified and let userspace handle it */
	if (strstr(gadget->composition_funcs, "ffs.")) {
		dev_err(dev, "user should enable ffs\n");
		return -EINVAL;
	}

	ret = device_property_read_u32(dev, "qcom,vid", &val);
	if (ret) {
		dev_err(dev, "USB gadget idVendor not specified\n");
		return ret;
	}
	gadget->cdev.desc.idVendor = (u16)val;

	ret = device_property_read_u32(dev, "qcom,pid", &val);
	if (ret) {
		dev_err(dev, "USB gadget idProduct not specified\n");
		return ret;
	}
	gadget->cdev.desc.idProduct = (u16)val;

	ret = device_property_read_u32(dev, "qcom,class", &val);
	if (!ret)
		gadget->cdev.desc.bDeviceClass = (u8)val;

	ret = device_property_read_u32(dev, "qcom,subclass", &val);
	if (!ret)
		gadget->cdev.desc.bDeviceSubClass = (u8)val;

	ret = device_property_read_u32(dev, "qcom,protocol", &val);
	if (!ret)
		gadget->cdev.desc.bDeviceProtocol = (u8)val;

	return 0;
}

static ssize_t enabled_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct qti_usb_gadget *qg = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%c\n",
			qg->enabled ? 'Y' : 'N');
}

static ssize_t enabled_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct qti_usb_gadget *qg = dev_get_drvdata(dev);
	bool enable;
	int ret;

	ret = strtobool(buf, &enable);
	if (ret)
		return ret;

	if (enable)
		qti_gadget_register(qg);
	else
		qti_gadget_unregister(qg);

	return count;
}
static DEVICE_ATTR_RW(enabled);

static int qti_gadget_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct qti_usb_gadget *gadget;

	gadget = devm_kzalloc(dev, sizeof(*gadget), GFP_KERNEL);
	if (!gadget)
		return -ENOMEM;

	platform_set_drvdata(pdev, gadget);
	gadget->dev = dev;
	INIT_LIST_HEAD(&gadget->cdev.configs);
	INIT_LIST_HEAD(&gadget->cdev.gstrings);

	ret = qti_gadget_get_properties(gadget);
	if (ret)
		return ret;

	ret = qti_gadget_register(gadget);
	if (ret)
		return ret;

	device_create_file(&pdev->dev, &dev_attr_enabled);

	return 0;
}

static int qti_gadget_remove(struct platform_device *pdev)
{
	struct qti_usb_gadget *qg = platform_get_drvdata(pdev);

	device_remove_file(&pdev->dev, &dev_attr_enabled);
	qti_gadget_unregister(qg);

	return 0;
}

static const struct of_device_id qti_gadget_dt_match[] = {
	{
		.compatible = "qcom,usb-gadget",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, qti_gadget_dt_match);

static struct platform_driver qti_gadget_platform_driver = {
	.driver = {
		.name = "qti_usb_gadget",
		.of_match_table = qti_gadget_dt_match,
	},
	.probe = qti_gadget_probe,
	.remove = qti_gadget_remove,
};

static int __init gadget_qti_init(void)
{
	int ret;

	ret = platform_driver_register(&qti_gadget_platform_driver);
	if (ret) {
		pr_err("%s: Failed to register qti gadget platform driver\n",
			__func__);
	}

	return ret;
}
module_init(gadget_qti_init);

static void __exit gadget_qti_exit(void)
{
	platform_driver_unregister(&qti_gadget_platform_driver);
}
module_exit(gadget_qti_exit);
