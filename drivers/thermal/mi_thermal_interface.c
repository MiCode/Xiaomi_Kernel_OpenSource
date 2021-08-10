#include <linux/cpu_cooling.h>
#include <linux/cpufreq.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/idr.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm_qos.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/suspend.h>
#include <linux/thermal.h>
#include <drm/drm_panel.h>
#include <net/netlink.h>
#include <net/genetlink.h>
#include <linux/power_supply.h>

#include "thermal_core.h"

#if defined(CONFIG_DRM_PANEL)
static struct drm_panel *active_panel;
#endif

struct mi_thermal_device  {
	struct device *dev;
	struct class *class;
	struct attribute_group attrs;
	struct notifier_block psy_nb;
	int usb_online;
};

static struct mi_thermal_device mi_thermal_dev;

struct screen_monitor {
	struct notifier_block thermal_notifier;
	int screen_state;
};
struct screen_monitor sm;

static ssize_t thermal_screen_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", sm.screen_state);
}

static DEVICE_ATTR(screen_state, 0664,
		thermal_screen_state_show, NULL);

static ssize_t thermal_usb_online_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", mi_thermal_dev.usb_online);
}

static DEVICE_ATTR(usb_online, 0664,
		thermal_usb_online_show, NULL);

static struct attribute *mi_thermal_dev_attr_group[] = {
	&dev_attr_screen_state.attr,
	&dev_attr_usb_online.attr,
	NULL,
};

static const char *get_screen_state_name(int mode)
{
	switch (mode) {
	case DRM_PANEL_BLANK_UNBLANK:
		return "On";
	case DRM_PANEL_BLANK_LP:
		return "Doze";
	case DRM_PANEL_BLANK_POWERDOWN:
		return "Off";
	default:
		return "Unknown";
	}
}

static int screen_state_for_thermal_callback(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct drm_panel_notifier *evdata = data;
	unsigned int blank;

	if (val != DRM_PANEL_EVENT_BLANK || !evdata || !evdata->data)
		return 0;

	blank = *(int *)(evdata->data);
	switch (blank) {
	case DRM_PANEL_BLANK_UNBLANK:
		sm.screen_state = 1;
		break;
	case DRM_PANEL_BLANK_LP:
	case DRM_PANEL_BLANK_POWERDOWN:
		sm.screen_state = 0;
		break;
	default:
		break;
	}

	pr_warn("%s: %s, sm.screen_state = %d\n", __func__, get_screen_state_name(blank),
			sm.screen_state);
	sysfs_notify(&mi_thermal_dev.dev->kobj, NULL, "screen_state");

	return NOTIFY_OK;
}

static int usb_online_callback(struct notifier_block *nb,
		unsigned long val, void *data)
{
	static struct power_supply *usb_psy;
	struct power_supply *psy = data;
	union power_supply_propval ret = {0,};
	int err = 0;

	if (strcmp(psy->desc->name, "usb"))
		return NOTIFY_OK;

	if (!usb_psy)
		usb_psy = power_supply_get_by_name("usb");
	if (usb_psy) {
		err = power_supply_get_property(usb_psy,
				POWER_SUPPLY_PROP_ONLINE, &ret);
		if (err) {
			pr_err("usb online read error:%d\n",err);
			return err;
		}
		mi_thermal_dev.usb_online = ret.intval;
		sysfs_notify(&mi_thermal_dev.dev->kobj, NULL, "usb_online");
	}

	return NOTIFY_OK;
}

static int create_thermal_message_node(void)
{
	int ret = 0;
	mi_thermal_dev.class = &thermal_class;
	mi_thermal_dev.dev = &thermal_message_dev;
	mi_thermal_dev.attrs.attrs = mi_thermal_dev_attr_group;
	ret = sysfs_create_group(&mi_thermal_dev.dev->kobj, &mi_thermal_dev.attrs);
	if (ret) {
		pr_err("%s ERROR: Cannot create sysfs structure!:%d\n", __func__, ret);
		ret = -ENODEV;
		return ret;
	}
	return ret;
}

static void destroy_thermal_message_node(void)
{
	sysfs_remove_group(&mi_thermal_dev.dev->kobj, &mi_thermal_dev.attrs);
	mi_thermal_dev.class = NULL;
	mi_thermal_dev.dev = NULL;
	mi_thermal_dev.attrs.attrs = NULL;
}

/**
 * pointer active_panel initlized function, used to checkout panel(config)from devices
 * tree ,later will be passed to drm_notifyXXX function.
 * @param device node contains the panel
 * @return pointer to that panel if panel truely  exists, otherwise negative number
 */
static int thermal_check_panel(struct device_node *np)
{
	int i;
	int count;
	struct device_node *node;
	struct drm_panel *panel;

	count = of_count_phandle_with_args(np, "panel", NULL);
	if (count <= 0)
		return 0;

	for (i = 0; i < count; i++) {
		node = of_parse_phandle(np, "panel", i);
		panel = of_drm_find_panel(node);
		of_node_put(node);
		if (!IS_ERR(panel)) {
			active_panel = panel;
			return 0;
		}
	}

	return PTR_ERR(panel);
}

static int __init mi_thermal_interface_init(void)
{
	int ret = 0;
	struct device_node *node;
	int error = 0;

	node = of_find_node_by_name(NULL, "thermal-screen");
	if (!node) {
		pr_err("%s ERROR: Cannot find node with panel!", __func__);
		return 0;
	}
#if defined(CONFIG_OF) && defined(CONFIG_DRM_PANEL)
	error = thermal_check_panel(node);
	if (error == -EPROBE_DEFER) {
		pr_err("%s ERROR: Cannot fine panel of node!", __func__);
		return 0;
	}
#endif
	if (create_thermal_message_node()){
		pr_warn("Thermal: create screen state failed\n");
	}
	sm.thermal_notifier.notifier_call = screen_state_for_thermal_callback;
#if defined(CONFIG_DRM_PANEL)
	if (active_panel && drm_panel_notifier_register(active_panel, &sm.thermal_notifier) < 0) {
		pr_warn("Thermal: register screen state callback failed\n");
	}
#endif
	mi_thermal_dev.psy_nb.notifier_call = usb_online_callback;
	ret = power_supply_reg_notifier(&mi_thermal_dev.psy_nb);
	if (ret < 0) {
		pr_err("usb online notifier registration error. defer. err:%d\n",
			ret);
		ret = -EPROBE_DEFER;
	}
	return 0;
}

static void __exit mi_thermal_interface_exit(void)
{
#if defined(CONFIG_DRM_PANEL)
	if (active_panel)
		drm_panel_notifier_unregister(active_panel, &sm.thermal_notifier);
#endif
	power_supply_unreg_notifier(&mi_thermal_dev.psy_nb);
	destroy_thermal_message_node();
}

module_init(mi_thermal_interface_init);
module_exit(mi_thermal_interface_exit);

MODULE_AUTHOR("Fankl");
MODULE_DESCRIPTION("Xiaomi thermal control interface");
MODULE_LICENSE("GPL v2");

