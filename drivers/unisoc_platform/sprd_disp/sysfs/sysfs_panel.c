/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <video/videomode.h>

#include "disp_lib.h"
#include "sprd_dsi.h"
#include "sprd_dsi_panel.h"
#include "sysfs_display.h"

#define host_to_dsi(host) \
	container_of(host, struct sprd_dsi, host)

static ssize_t name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct panel_info *info = &panel->info;
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->of_node->name);

	return ret;
}
static DEVICE_ATTR_RO(name);

static ssize_t lane_num_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct panel_info *info = &panel->info;
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "%u\n", info->lanes);

	return ret;
}
static DEVICE_ATTR_RO(lane_num);

static ssize_t pixel_clock_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct panel_info *info = &panel->info;
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "%u\n", info->mode.clock * 1000);

	return ret;
}
static DEVICE_ATTR_RO(pixel_clock);

static ssize_t resolution_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct panel_info *info = &panel->info;
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "%ux%u\n", info->mode.hdisplay,
						info->mode.vdisplay);

	return ret;
}
static DEVICE_ATTR_RO(resolution);

static ssize_t screen_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct panel_info *info = &panel->info;
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "%umm x %umm\n", info->mode.width_mm,
						info->mode.height_mm);

	return ret;
}
static DEVICE_ATTR_RO(screen_size);

static ssize_t hporch_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct videomode vm;
	int ret;

	drm_display_mode_to_videomode(&panel->info.mode, &vm);
	ret = snprintf(buf, PAGE_SIZE, "hfp=%u hbp=%u hsync=%u\n",
				   vm.hfront_porch,
				   vm.hback_porch,
				   vm.hsync_len);

	return ret;
}

static ssize_t hporch_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct mipi_dsi_host *host = panel->slave->host;
	struct sprd_dsi *dsi = container_of(host, struct sprd_dsi, host);
	struct drm_connector *connector = &dsi->connector;
	struct drm_device *drm = connector->dev;
	struct videomode vm;
	u32 val[4] = {0};
	int len;

	len = str_to_u32_array(buf, 0, val, 4);
	drm_display_mode_to_videomode(&panel->info.mode, &vm);

	switch (len) {
	case 3:
		vm.hsync_len = val[2];
		fallthrough;
	case 2:
		vm.hback_porch = val[1];
		fallthrough;
	case 1:
		vm.hfront_porch = val[0];
		fallthrough;
	default:
		break;
	}

	drm_display_mode_from_videomode(&vm, &panel->info.mode);
	mutex_lock(&drm->mode_config.mutex);
	drm_helper_probe_single_connector_modes(connector, 0, 0);
	mutex_unlock(&drm->mode_config.mutex);

	return count;
}
static DEVICE_ATTR_RW(hporch);

static ssize_t vporch_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct videomode vm;
	int ret;

	drm_display_mode_to_videomode(&panel->info.mode, &vm);
	ret = snprintf(buf, PAGE_SIZE, "vfp=%u vbp=%u vsync=%u\n",
				   vm.vfront_porch,
				   vm.vback_porch,
				   vm.vsync_len);

	return ret;
}

static ssize_t vporch_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct mipi_dsi_host *host = panel->slave->host;
	struct sprd_dsi *dsi = container_of(host, struct sprd_dsi, host);
	struct drm_connector *connector = &dsi->connector;
	struct drm_device *drm = connector->dev;
	struct videomode vm;
	u32 val[4] = {0};
	int len;

	len = str_to_u32_array(buf, 0, val, 4);
	drm_display_mode_to_videomode(&panel->info.mode, &vm);

	switch (len) {
	case 3:
		vm.vsync_len = val[2];
		fallthrough;
	case 2:
		vm.vback_porch = val[1];
		fallthrough;
	case 1:
		vm.vfront_porch = val[0];
		fallthrough;
	default:
		break;
	}

	drm_display_mode_from_videomode(&vm, &panel->info.mode);
	mutex_lock(&drm->mode_config.mutex);
	drm_helper_probe_single_connector_modes(connector, 0, 0);
	mutex_unlock(&drm->mode_config.mutex);

	return count;
}
static DEVICE_ATTR_RW(vporch);

static ssize_t esd_check_enable_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", panel->info.esd_check_en);
}

static ssize_t esd_check_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct panel_info *info = &panel->info;
	int enable;

	if (kstrtoint(buf, 10, &enable)) {
		pr_err("invalid input for esd check enable\n");
		return -EINVAL;
	}

	if (enable) {
		info->esd_check_en = true;
		if (!pm_runtime_suspended(panel->slave->host->dev) &&
		    !panel->esd_work_pending) {
			pr_info("schedule esd work\n");
			schedule_delayed_work(&panel->esd_work,
			      msecs_to_jiffies(info->esd_check_period));
			panel->esd_work_pending = true;
		}
	} else {
		if (panel->esd_work_pending) {
			pr_info("cancel esd work\n");
			cancel_delayed_work_sync(&panel->esd_work);
			panel->esd_work_pending = false;
		}
		info->esd_check_en = false;
	}

	return count;

}
static DEVICE_ATTR_RW(esd_check_enable);

static ssize_t esd_check_mode_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", panel->info.esd_check_mode);
}

static ssize_t esd_check_mode_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	u32 mode;

	if (kstrtouint(buf, 10, &mode)) {
		pr_err("invalid input for esd check mode\n");
		return -EINVAL;
	}

	panel->info.esd_check_mode = mode;

	return count;

}
static DEVICE_ATTR_RW(esd_check_mode);

static ssize_t esd_check_period_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", panel->info.esd_check_period);
}

static ssize_t esd_check_period_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	u32 period;

	if (kstrtouint(buf, 10, &period)) {
		pr_err("invalid input for esd check period\n");
		return -EINVAL;
	}

	panel->info.esd_check_period = period;

	return count;

}
static DEVICE_ATTR_RW(esd_check_period);

static ssize_t esd_check_register_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", panel->info.esd_check_reg);
}

static ssize_t esd_check_register_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	u32 reg;

	if (kstrtouint(buf, 16, &reg)) {
		pr_err("invalid input for esd check register\n");
		return -EINVAL;
	}

	panel->info.esd_check_reg = reg;

	return count;

}
static DEVICE_ATTR_RW(esd_check_register);

static ssize_t esd_check_value_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x\n",
			panel->info.esd_check_val);
}

static ssize_t esd_check_value_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	u32 value;

	if (kstrtouint(buf, 16, &value)) {
		pr_err("invalid input for esd check value\n");
		return -EINVAL;
	}

	panel->info.esd_check_val = value;

	return count;

}
static DEVICE_ATTR_RW(esd_check_value);

static ssize_t suspend_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct mipi_dsi_host *host = panel->slave->host;
	struct sprd_dsi *dsi = host_to_dsi(host);

	if (dsi->ctx.enabled && panel->enabled) {
		drm_panel_disable(&panel->base);
		drm_panel_unprepare(&panel->base);
		panel->enabled = false;
	}

	return count;
}
static DEVICE_ATTR_WO(suspend);

static ssize_t resume_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct mipi_dsi_host *host = panel->slave->host;
	struct sprd_dsi *dsi = host_to_dsi(host);

	if (dsi->ctx.enabled && (!panel->enabled)) {
		drm_panel_prepare(&panel->base);
		drm_panel_enable(&panel->base);
		panel->enabled = true;
	}

	return count;
}
static DEVICE_ATTR_WO(resume);

static struct attribute *panel_attrs[] = {
	&dev_attr_name.attr,
	&dev_attr_lane_num.attr,
	&dev_attr_resolution.attr,
	&dev_attr_screen_size.attr,
	&dev_attr_pixel_clock.attr,
	&dev_attr_hporch.attr,
	&dev_attr_vporch.attr,
	&dev_attr_esd_check_enable.attr,
	&dev_attr_esd_check_mode.attr,
	&dev_attr_esd_check_period.attr,
	&dev_attr_esd_check_register.attr,
	&dev_attr_esd_check_value.attr,
	&dev_attr_suspend.attr,
	&dev_attr_resume.attr,
	NULL,
};
ATTRIBUTE_GROUPS(panel);

int sprd_panel_sysfs_init(struct device *dev)
{
	int rc;

	rc = sysfs_create_groups(&(dev->kobj), panel_groups);
	if (rc)
		pr_err("create panel attr node failed, rc=%d.\n", rc);

	return rc;
}
EXPORT_SYMBOL(sprd_panel_sysfs_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("leon.he@spreadtrum.com");
MODULE_DESCRIPTION("Add panel attribute nodes for userspace");