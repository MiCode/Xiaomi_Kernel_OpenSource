
/*
 * drm_sysfs.c - Modifications to drm_sysfs_class.c to support
 *               extra sysfs attribute from DRM. Normal drm_sysfs_class
 *               does not allow adding attributes.
 *
 * Copyright (c) 2004 Jon Smirl <jonsmirl@gmail.com>
 * Copyright (C) 2021 XiaoMi, Inc.
 * Copyright (c) 2003-2004 Greg Kroah-Hartman <greg@kroah.com>
 * Copyright (c) 2003-2004 IBM Corp.
 *
 * This file is released under the GPLv2
 *
 */

#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/gfp.h>
#include <linux/err.h>
#include <linux/export.h>

#include <drm/drm_encoder.h>
#include <drm/drm_sysfs.h>
#include <drm/drmP.h>
#include "drm_internal.h"

#define to_drm_minor(d) dev_get_drvdata(d)
#define to_drm_connector(d) dev_get_drvdata(d)

/**
 * DOC: overview
 *
 * DRM provides very little additional support to drivers for sysfs
 * interactions, beyond just all the standard stuff. Drivers who want to expose
 * additional sysfs properties and property groups can attach them at either
 * &drm_device.dev or &drm_connector.kdev.
 *
 * Registration is automatically handled when calling drm_dev_register(), or
 * drm_connector_register() in case of hot-plugged connectors. Unregistration is
 * also automatically handled by drm_dev_unregister() and
 * drm_connector_unregister().
 */

static struct device_type drm_sysfs_device_minor = {
	.name = "drm_minor"
};

struct class *drm_class;

static char *drm_devnode(struct device *dev, umode_t *mode)
{
	return kasprintf(GFP_KERNEL, "dri/%s", dev_name(dev));
}

static CLASS_ATTR_STRING(version, S_IRUGO, "drm 1.1.0 20060810");

/**
 * drm_sysfs_init - initialize sysfs helpers
 *
 * This is used to create the DRM class, which is the implicit parent of any
 * other top-level DRM sysfs objects.
 *
 * You must call drm_sysfs_destroy() to release the allocated resources.
 *
 * Return: 0 on success, negative error code on failure.
 */
int drm_sysfs_init(void)
{
	int err;

	drm_class = class_create(THIS_MODULE, "drm");
	if (IS_ERR(drm_class))
		return PTR_ERR(drm_class);

	err = class_create_file(drm_class, &class_attr_version.attr);
	if (err) {
		class_destroy(drm_class);
		drm_class = NULL;
		return err;
	}

	drm_class->devnode = drm_devnode;
	return 0;
}

/**
 * drm_sysfs_destroy - destroys DRM class
 *
 * Destroy the DRM device class.
 */
void drm_sysfs_destroy(void)
{
	if (IS_ERR_OR_NULL(drm_class))
		return;
	class_remove_file(drm_class, &class_attr_version.attr);
	class_destroy(drm_class);
	drm_class = NULL;
}

/*
 * Connector properties
 */
static ssize_t status_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct drm_connector *connector = to_drm_connector(device);
	struct drm_device *dev = connector->dev;
	enum drm_connector_force old_force;
	int ret;

	ret = mutex_lock_interruptible(&dev->mode_config.mutex);
	if (ret)
		return ret;

	old_force = connector->force;

	if (sysfs_streq(buf, "detect"))
		connector->force = 0;
	else if (sysfs_streq(buf, "on"))
		connector->force = DRM_FORCE_ON;
	else if (sysfs_streq(buf, "on-digital"))
		connector->force = DRM_FORCE_ON_DIGITAL;
	else if (sysfs_streq(buf, "off"))
		connector->force = DRM_FORCE_OFF;
	else
		ret = -EINVAL;

	if (old_force != connector->force || !connector->force) {
		DRM_DEBUG_KMS("[CONNECTOR:%d:%s] force updated from %d to %d or reprobing\n",
			      connector->base.id,
			      connector->name,
			      old_force, connector->force);

		connector->funcs->fill_modes(connector,
					     dev->mode_config.max_width,
					     dev->mode_config.max_height);
	}

	mutex_unlock(&dev->mode_config.mutex);

	return ret ? ret : count;
}

static ssize_t status_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct drm_connector *connector = to_drm_connector(device);
	enum drm_connector_status status;

	status = READ_ONCE(connector->status);

	return snprintf(buf, PAGE_SIZE, "%s\n",
			drm_get_connector_status_name(status));
}

static ssize_t dpms_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct drm_connector *connector = to_drm_connector(device);
	int dpms;

	dpms = READ_ONCE(connector->dpms);

	return snprintf(buf, PAGE_SIZE, "%s\n",
			drm_get_dpms_name(dpms));
}

static ssize_t enabled_show(struct device *device,
			    struct device_attribute *attr,
			   char *buf)
{
	struct drm_connector *connector = to_drm_connector(device);
	bool enabled;

	enabled = READ_ONCE(connector->encoder);

	return snprintf(buf, PAGE_SIZE, enabled ? "enabled\n" : "disabled\n");
}

static ssize_t edid_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *connector_dev = kobj_to_dev(kobj);
	struct drm_connector *connector = to_drm_connector(connector_dev);
	unsigned char *edid;
	size_t size;
	ssize_t ret = 0;

	mutex_lock(&connector->dev->mode_config.mutex);
	if (!connector->edid_blob_ptr)
		goto unlock;

	edid = connector->edid_blob_ptr->data;
	size = connector->edid_blob_ptr->length;
	if (!edid)
		goto unlock;

	if (off >= size)
		goto unlock;

	if (off + count > size)
		count = size - off;
	memcpy(buf, edid + off, count);

	ret = count;
unlock:
	mutex_unlock(&connector->dev->mode_config.mutex);

	return ret;
}

static ssize_t modes_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct drm_connector *connector = to_drm_connector(device);
	struct drm_display_mode *mode;
	int written = 0;

	mutex_lock(&connector->dev->mode_config.mutex);
	list_for_each_entry(mode, &connector->modes, head) {
		written += snprintf(buf + written, PAGE_SIZE - written, "%s\n",
				    mode->name);
	}
	mutex_unlock(&connector->dev->mode_config.mutex);

	return written;
}

extern int drm_get_panel_info(struct drm_bridge *bridge, char *name);
static ssize_t panel_info_show(struct device *device,
			    struct device_attribute *attr,
			   char *buf)
{
	int written = 0;
	char pname[128] = {0};
	struct drm_connector *connector = NULL;
	struct drm_encoder *encoder = NULL;
	struct drm_bridge *bridge = NULL;

	connector = to_drm_connector(device);
	if (!connector)
		return written;

	encoder = connector->encoder;
	if (!encoder)
		return written;

	bridge = encoder->bridge;
	if (!bridge)
		return written;

	written = drm_get_panel_info(bridge, pname);
	if (written)
		return snprintf(buf, PAGE_SIZE, "panel_name=%s\n", pname);

	return written;
}

int dsi_bridge_disp_set_doze_backlight(struct drm_connector *connector,
			int doze_backlight);
ssize_t dsi_bridge_disp_get_doze_backlight(struct drm_connector *connector,
			char *buf);

static ssize_t doze_brightness_show(struct device *device,
			    struct device_attribute *attr,
			   char *buf)
{
	struct drm_connector *connector = to_drm_connector(device);
	struct drm_device *dev = connector->dev;

	return snprintf(buf, PAGE_SIZE, "%d\n",
			dev->doze_brightness);
}

static ssize_t doze_backlight_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct drm_connector *connector = to_drm_connector(device);
	int doze_backlight;
	int ret;

	ret = kstrtoint(buf, 0, &doze_backlight);
	if (ret)
		return ret;

	ret = dsi_bridge_disp_set_doze_backlight(connector, doze_backlight);

	return ret ? ret : count;
}

static ssize_t doze_backlight_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct drm_connector *connector = to_drm_connector(dev);
	return dsi_bridge_disp_get_doze_backlight(connector, buf);
}

ssize_t drm_bridge_disp_param_get(struct drm_bridge *bridge, char *buf);
static ssize_t disp_param_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	ssize_t ret = 0;
	struct drm_encoder *encoder = NULL;
	struct drm_bridge *bridge = NULL;
	struct drm_connector *connector = to_drm_connector(device);
	if (!connector)
		return ret;
	encoder = connector->encoder;
	if (!encoder)
		return ret;
	bridge = encoder->bridge;
	if (!bridge)
		return ret;
	ret = drm_bridge_disp_param_get(bridge, buf);
	return ret;
}

void drm_bridge_disp_param_set(struct drm_bridge *bridge, int cmd);
static ssize_t disp_param_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	int param;
	struct drm_connector *connector = NULL;
	struct drm_encoder *encoder = NULL;
	struct drm_bridge *bridge = NULL;

	if (!device)
		return count;

	connector = to_drm_connector(device);
	if (!connector)
		return count;

	encoder = connector->encoder;
	if (!encoder)
		return count;

	bridge = encoder->bridge;
	if (!bridge)
		return count;
	sscanf(buf, "0x%x", &param);

	drm_bridge_disp_param_set(bridge, param);

	return count;
}

extern ssize_t get_fod_ui_status(struct drm_connector *connector);
static ssize_t fod_ui_ready_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct drm_connector *connector = NULL;
	u32 fod_ui_ready = 0;

	connector = to_drm_connector(device);
	if (!connector)
		return 0;

	fod_ui_ready = get_fod_ui_status(connector);
	return snprintf(buf, PAGE_SIZE, "%d\n", fod_ui_ready);
}

ssize_t xm_fod_dim_layer_alpha_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count);

extern void set_fod_dimlayer_status(struct drm_connector *connector, bool enabled);
ssize_t dim_layer_enable_store(struct device *device,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct drm_connector *connector = NULL;
	bool fod_dimlayer_enabled = false;

	connector = to_drm_connector(device);
	if (!connector)
		return 0;

	kstrtobool(buf, &fod_dimlayer_enabled);
	set_fod_dimlayer_status(connector, fod_dimlayer_enabled);

	pr_info("set fod dimlayer %s", fod_dimlayer_enabled ? "true" : "false");
	return count;
}

extern bool get_fod_dimlayer_status(struct drm_connector *connector);
static ssize_t dim_layer_enable_show(struct device *device,
				struct device_attribute *attr,
				char *buf)
{
	struct drm_connector *connector = NULL;
	bool fod_dimlayer_enabled = false;

	connector = to_drm_connector(device);
	if (!connector)
		return 0;

	fod_dimlayer_enabled = get_fod_dimlayer_status(connector);

	return snprintf(buf, PAGE_SIZE, fod_dimlayer_enabled ? "enabled\n" : "disabled\n");
}

static DEVICE_ATTR_RW(dim_layer_enable);
static DEVICE_ATTR(dim_alpha, S_IRUGO|S_IWUSR, NULL, xm_fod_dim_layer_alpha_store);
static DEVICE_ATTR_RW(status);
static DEVICE_ATTR_RO(enabled);
static DEVICE_ATTR_RO(dpms);
static DEVICE_ATTR_RO(modes);
static DEVICE_ATTR_RO(panel_info);
static DEVICE_ATTR_RW(disp_param);
static DEVICE_ATTR_RO(doze_brightness);
static DEVICE_ATTR_RW(doze_backlight);
static DEVICE_ATTR_RO(fod_ui_ready);

static struct attribute *connector_dev_attrs[] = {
	&dev_attr_status.attr,
	&dev_attr_enabled.attr,
	&dev_attr_dpms.attr,
	&dev_attr_modes.attr,
	&dev_attr_panel_info.attr,
	&dev_attr_disp_param.attr,
	&dev_attr_doze_brightness.attr,
	&dev_attr_doze_backlight.attr,
	&dev_attr_dim_alpha.attr,
	&dev_attr_fod_ui_ready.attr,
	&dev_attr_dim_layer_enable.attr,
	NULL
};

static struct bin_attribute edid_attr = {
	.attr.name = "edid",
	.attr.mode = 0444,
	.size = 0,
	.read = edid_show,
};

static struct bin_attribute *connector_bin_attrs[] = {
	&edid_attr,
	NULL
};

static const struct attribute_group connector_dev_group = {
	.attrs = connector_dev_attrs,
	.bin_attrs = connector_bin_attrs,
};

static const struct attribute_group *connector_dev_groups[] = {
	&connector_dev_group,
	NULL
};

int drm_sysfs_connector_add(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;

	if (connector->kdev)
		return 0;

	connector->kdev =
		device_create_with_groups(drm_class, dev->primary->kdev, 0,
					  connector, connector_dev_groups,
					  "card%d-%s", dev->primary->index,
					  connector->name);
	DRM_DEBUG("adding \"%s\" to sysfs\n",
		  connector->name);

	if (IS_ERR(connector->kdev)) {
		DRM_ERROR("failed to register connector device: %ld\n", PTR_ERR(connector->kdev));
		return PTR_ERR(connector->kdev);
	}

	/* Let userspace know we have a new connector */
	drm_sysfs_hotplug_event(dev);

	return 0;
}

void drm_sysfs_connector_remove(struct drm_connector *connector)
{
	if (!connector->kdev)
		return;
	DRM_DEBUG("removing \"%s\" from sysfs\n",
		  connector->name);

	device_unregister(connector->kdev);
	connector->kdev = NULL;
}

/**
 * drm_sysfs_hotplug_event - generate a DRM uevent
 * @dev: DRM device
 *
 * Send a uevent for the DRM device specified by @dev.  Currently we only
 * set HOTPLUG=1 in the uevent environment, but this could be expanded to
 * deal with other types of events.
 */
void drm_sysfs_hotplug_event(struct drm_device *dev)
{
	char *event_string = "HOTPLUG=1";
	char *envp[] = { event_string, NULL };

	DRM_DEBUG("generating hotplug event\n");

	kobject_uevent_env(&dev->primary->kdev->kobj, KOBJ_CHANGE, envp);
}
EXPORT_SYMBOL(drm_sysfs_hotplug_event);

static void drm_sysfs_release(struct device *dev)
{
	kfree(dev);
}

struct device *drm_sysfs_minor_alloc(struct drm_minor *minor)
{
	const char *minor_str;
	struct device *kdev;
	int r;

	if (minor->type == DRM_MINOR_CONTROL)
		minor_str = "controlD%d";
	else if (minor->type == DRM_MINOR_RENDER)
		minor_str = "renderD%d";
	else
		minor_str = "card%d";

	kdev = kzalloc(sizeof(*kdev), GFP_KERNEL);
	if (!kdev)
		return ERR_PTR(-ENOMEM);

	device_initialize(kdev);
	kdev->devt = MKDEV(DRM_MAJOR, minor->index);
	kdev->class = drm_class;
	kdev->type = &drm_sysfs_device_minor;
	kdev->parent = minor->dev->dev;
	kdev->release = drm_sysfs_release;
	dev_set_drvdata(kdev, minor);

	r = dev_set_name(kdev, minor_str, minor->index);
	if (r < 0)
		goto err_free;

	return kdev;

err_free:
	put_device(kdev);
	return ERR_PTR(r);
}

/**
 * drm_class_device_register - register new device with the DRM sysfs class
 * @dev: device to register
 *
 * Registers a new &struct device within the DRM sysfs class. Essentially only
 * used by ttm to have a place for its global settings. Drivers should never use
 * this.
 */
int drm_class_device_register(struct device *dev)
{
	if (!drm_class || IS_ERR(drm_class))
		return -ENOENT;

	dev->class = drm_class;
	return device_register(dev);
}
EXPORT_SYMBOL_GPL(drm_class_device_register);

/**
 * drm_class_device_unregister - unregister device with the DRM sysfs class
 * @dev: device to unregister
 *
 * Unregisters a &struct device from the DRM sysfs class. Essentially only used
 * by ttm to have a place for its global settings. Drivers should never use
 * this.
 */
void drm_class_device_unregister(struct device *dev)
{
	return device_unregister(dev);
}
EXPORT_SYMBOL_GPL(drm_class_device_unregister);
