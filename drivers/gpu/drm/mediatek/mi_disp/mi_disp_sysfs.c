// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#define pr_fmt(fmt)	"mi_disp_sysfs:[%s] " fmt, __func__

#include <linux/device.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/gfp.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>

#include "mi_disp_feature.h"
#include "mi_dsi_display.h"
#include "mi_disp_print.h"
#include "mi_dsi_panel.h"
//#define to_dsi_bridge(x)     container_of((x), struct dsi_bridge, base)

#define to_disp_display(d) dev_get_drvdata(d)

static ssize_t disp_param_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	struct disp_feature_ctl ctl;
	char *token, *input_copy, *input_dup = NULL;
	const char *delim = " ";
	u32 tmp_data = 0;
	int ret = 0, index = 0;

	memset(&ctl, 0, sizeof(struct disp_feature_ctl));

	input_copy = kstrdup(buf, GFP_KERNEL);
	if (!input_copy) {
		DISP_ERROR("can not allocate memory\n");
		ret = -ENOMEM;
		goto exit;
	}
	input_dup = input_copy;
	/* removes leading and trailing whitespace from input_copy */
	input_copy = strim(input_copy);
	/* Split a string into token */
	token = strsep(&input_copy, delim);
	if (token) {
		ret = kstrtoint(token, 10, &tmp_data);
		if (ret) {
			DISP_ERROR("input buffer conversion failed\n");
			ret = -EAGAIN;
			goto exit_free;
		}
		if (is_support_disp_feature_id(tmp_data)) {
			ctl.feature_id = tmp_data;
		} else {
			DISP_ERROR("unsupported disp feature id\n");
			ret = -EAGAIN;
			goto exit_free;
		}
	}

	if (ctl.feature_id == DISP_FEATURE_BIST_MODE_COLOR) {
		ctl.tx_ptr = kzalloc(sizeof(u8) * 3, GFP_KERNEL);
		if (!ctl.tx_ptr) {
			DISP_ERROR("vmalloc failed for DISP_FEATURE_BIST_MODE\n");
			goto exit_free;
		}
		for (index = 0; index < 3; index++) {
			/* Removes leading whitespace from input_copy */
			if (input_copy) {
				input_copy = skip_spaces(input_copy);
			} else {
				DISP_ERROR("please check the number of parameters\n");
				ret = -EAGAIN;
				kfree(ctl.tx_ptr);
				goto exit_free;
			}

			token = strsep(&input_copy, delim);
			if (token) {
				ret = kstrtoint(token, 10, &tmp_data);
				if (ret) {
					DISP_ERROR("input buffer conversion failed\n");
					ret = -EAGAIN;
					kfree(ctl.tx_ptr);
					goto exit_free;
				}
				ctl.tx_ptr[index] = tmp_data & 0xFF;
			}
		}
	} else {
		/* Removes leading whitespace from input_copy */
		if (input_copy) {
			input_copy = skip_spaces(input_copy);
		} else {
			DISP_ERROR("please check the number of parameters\n");
			ret = -EAGAIN;
			goto exit_free;
		}

		ret = kstrtoint(input_copy, 10, &tmp_data);
		if (ret) {
			DISP_ERROR("input buffer conversion failed\n");
			ret = -EAGAIN;
			goto exit_free;
		}
		ctl.feature_val = tmp_data;
	}

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		ret = mi_dsi_display_set_disp_param(dd_ptr->display, &ctl);
	} else {
		DISP_ERROR("Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		ret = -EINVAL;
	}

exit_free:
	kfree(input_dup);
	if (ctl.tx_ptr)
		kfree(ctl.tx_ptr);
exit:
	return ret ? ret : count;
}

static ssize_t disp_param_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct disp_display *dd_ptr = to_disp_display(device);

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		return mi_dsi_display_get_disp_param(dd_ptr->display, buf, PAGE_SIZE);
	} else {
		return snprintf(buf, PAGE_SIZE, "Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
	}
}

static ssize_t mipi_rw_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	int ret = 0;

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		ret = mi_dsi_display_write_mipi_reg(dd_ptr->display, (char *)buf);
	} else {
		DISP_ERROR("Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		ret = -EINVAL;
	}

	return ret ? ret : count;
}

static ssize_t mipi_rw_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct disp_display *dd_ptr = to_disp_display(device);

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		return mi_dsi_display_read_mipi_reg(dd_ptr->display, buf);
	} else {
		return snprintf(buf, PAGE_SIZE, "Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
	}
}

static ssize_t gir_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	int ret = 0;

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		if (strstr(buf, "on")) {
			ret = mi_dsi_display_enable_gir(dd_ptr->display, (char *)buf);
		} else if (strstr(buf, "off")) {
			ret = mi_dsi_display_disable_gir(dd_ptr->display, (char *)buf);
		}
	} else {
		DISP_ERROR("Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		ret = -EINVAL;
	}

	return ret ? ret : count;
}

static ssize_t gir_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct disp_display *dd_ptr = to_disp_display(device);

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		int gir_status = mi_dsi_display_get_gir_status(dd_ptr->display);
		if (gir_status < 0) {
			return snprintf(buf, PAGE_SIZE, "invalid\n");
		} else {
			return snprintf(buf, PAGE_SIZE, "%s\n", gir_status ? "on" : "off");
		}
	} else {
		return snprintf(buf, PAGE_SIZE, "Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
	}
}

static ssize_t panel_info_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct disp_display *dd_ptr = to_disp_display(device);

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		return mi_dsi_display_read_panel_info(dd_ptr->display, buf);
	} else {
		return snprintf(buf, PAGE_SIZE, "Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
	}
}

static ssize_t wp_info_show(struct device *device,
			struct device_attribute *attr, char *buf)
{
	size_t wpinfo_size = 64;
	struct disp_display *dd_ptr = to_disp_display(device);
	if (dd_ptr->intf_type == MI_INTF_DSI) {
		return mi_dsi_display_read_wp_info(dd_ptr->display, buf, wpinfo_size);
	} else {
		return snprintf(buf, PAGE_SIZE, "Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
	}

}

static ssize_t dynamic_fps_show(struct device *device,
			struct device_attribute *attr, char *buf)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	u32 fps = 0;
	int ret = 0;

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		ret = mi_dsi_display_get_fps(dd_ptr->display, &fps);
		if (ret)
			return snprintf(buf, PAGE_SIZE, "%s\n", "null");
		else
			return snprintf(buf, PAGE_SIZE, "%d\n", fps);
	} else {
		return snprintf(buf, PAGE_SIZE, "Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
	}
}

static ssize_t doze_brightness_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	u32 doze_brightness;
	int ret = 0;

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		ret = kstrtoint(buf, 0, &doze_brightness);;
		if (ret)
			return ret;
		ret = mi_dsi_display_set_doze_brightness(dd_ptr->display, doze_brightness);
	} else {
		DISP_ERROR("Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		ret = -EINVAL;
	}

	return ret ? ret : count;
}

static ssize_t doze_brightness_show(struct device *device,
			struct device_attribute *attr, char *buf)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	u32 doze_brightness;
	int ret = 0;

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		ret = mi_dsi_display_get_doze_brightness(dd_ptr->display, &doze_brightness);
		if (ret)
			return snprintf(buf, PAGE_SIZE, "%s\n", "null");
		else
			return snprintf(buf, PAGE_SIZE, "%d\n", doze_brightness);
	} else {
		return snprintf(buf, PAGE_SIZE, "Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
	}
}

static ssize_t brightness_clone_show(struct device *device,
		struct device_attribute *attr, char *buf)
{
	int brightness_clone = 0;
	struct disp_display *dd_ptr = to_disp_display(device);
	mi_dsi_display_get_brightness_clone(dd_ptr->display, &brightness_clone);
	return snprintf(buf, PAGE_SIZE, "%d\n", brightness_clone);
}

static ssize_t brightness_clone_store(struct device *device,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	struct disp_display *dd_ptr = to_disp_display(device);
	unsigned long brightness;

	ret = kstrtoul(buf, 0, &brightness);
	if (ret)
		return ret;

	ret = mi_dsi_display_set_brightness_clone(dd_ptr->display, brightness);

	return ret ? ret : count;
}

static ssize_t max_brightness_clone_show(struct device *device,
				struct device_attribute *attr, char *buf)
{
	int max_brightness_clone = 4095;
	struct disp_display *dd_ptr = to_disp_display(device);

	mi_dsi_display_get_max_brightness_clone(dd_ptr->display, &max_brightness_clone);

	return snprintf(buf, PAGE_SIZE, "%d\n", max_brightness_clone);

}

static ssize_t panel_event_show(struct device *device,
		struct device_attribute *attr,
		char *buf)
{
	ssize_t ret = 0;
	struct disp_display *dd_ptr = to_disp_display(device);
	struct mtk_dsi *dsi = (struct mtk_dsi *)dd_ptr->display;
	if (!dsi) {
		pr_info("%s-%d dsi is NULL \r\n",__func__, __LINE__);
		return ret;
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", dsi->panel_event);
}

static ssize_t dc_status_store(struct device *device,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	struct mtk_dsi *display = (struct mtk_dsi *)dd_ptr->display;
	uint32_t dc_status;
	int ret = kstrtouint(buf, 0, &dc_status);
	if (ret)
		return ret;

	display->dc_status = dc_status;
	mi_disp_feature_sysfs_notify(MI_SYSFS_DC);
	return count;
}

static ssize_t dc_status_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	struct mtk_dsi *display = (struct mtk_dsi *)dd_ptr->display;
	return snprintf(buf, PAGE_SIZE, "%u\n", display->dc_status);
}

static ssize_t led_i2c_reg_store(struct device *device,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	struct mtk_dsi *display = (struct mtk_dsi *)dd_ptr->display;

	return mi_dsi_display_write_led_i2c_reg(display, (char *)buf, count);
}

static ssize_t led_i2c_reg_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	struct mtk_dsi *display = (struct mtk_dsi *)dd_ptr->display;

	return mi_dsi_display_read_led_i2c_reg(display, buf);
}

static ssize_t idle_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	int ret = 0;

	if (dd_ptr && dd_ptr->intf_type == MI_INTF_DSI) {
		struct mtk_dsi *dsi = (struct mtk_dsi *)dd_ptr->display;
		int idle;
		sscanf(buf, "%d", &idle);
		if (!dsi) {
			DISP_ERROR("Invalid dsi\n");
			ret = -EINVAL;
		} else if (!idle) {
			mtk_drm_idlemgr_kick(__func__, dsi->encoder.crtc, 1);
		}
	} else {
		DISP_ERROR("Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		ret = -EINVAL;
	}

	return ret ? ret : count;
}

static ssize_t idle_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct disp_display *dd_ptr = to_disp_display(device);

	if (dd_ptr && dd_ptr->intf_type == MI_INTF_DSI) {
		struct mtk_dsi *dsi = (struct mtk_dsi *)dd_ptr->display;
		bool idle = false;
		if (!dsi) {
			DISP_ERROR("Invalid display ptr\n");
			return snprintf(buf, PAGE_SIZE, "invalid\n");
		}

		mutex_lock(&dsi->dsi_lock);
		idle = mtk_drm_is_idle(dsi->encoder.crtc);
		mutex_unlock(&dsi->dsi_lock);
		return snprintf(buf, PAGE_SIZE, "%d\n", idle);
	} else {
		return snprintf(buf, PAGE_SIZE, "Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
	}
}

#if 0
static ssize_t gamma_test_show(struct device *device,
		struct device_attribute *attr, char *buf)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	int ret = 0;

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		ret = mi_dsi_display_read_gamma_param(dd_ptr->display);
		if (ret)
			DISP_ERROR("Failed to read gamma param!\n");

		ret = mi_dsi_display_print_gamma_param(dd_ptr->display, buf, PAGE_SIZE);
	} else {
		DISP_ERROR("Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		ret = -EINVAL;
	}

	return ret;
}

static ssize_t hw_vsync_info_show(struct device *device,
		struct device_attribute *attr, char *buf)
{
	struct disp_display *dd_ptr = to_disp_display(device);
	int ret = 0;

	if (dd_ptr->intf_type == MI_INTF_DSI) {
		ret = mi_dsi_display_get_hw_vsync_info(dd_ptr->display, buf, PAGE_SIZE);
	} else {
		DISP_ERROR("Unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		ret = -EINVAL;
	}

	return ret;
}

#endif
static DEVICE_ATTR_RW(disp_param);
static DEVICE_ATTR_RW(mipi_rw);
static DEVICE_ATTR_RO(panel_info);
static DEVICE_ATTR_RO(wp_info);
static DEVICE_ATTR_RO(dynamic_fps);
static DEVICE_ATTR_RW(doze_brightness);
static DEVICE_ATTR_RW(brightness_clone);
static DEVICE_ATTR_RO(max_brightness_clone);
static DEVICE_ATTR_RW(gir);
static DEVICE_ATTR_RW(dc_status);
static DEVICE_ATTR_RW(led_i2c_reg);
static DEVICE_ATTR_RO(panel_event);
static DEVICE_ATTR_RW(idle);

#if 0
static DEVICE_ATTR_RO(gamma_test);
static DEVICE_ATTR_RO(hw_vsync_info);
#endif

static struct attribute *disp_feature_attrs[] = {
	&dev_attr_disp_param.attr,
	&dev_attr_mipi_rw.attr,
	&dev_attr_panel_info.attr,
	&dev_attr_wp_info.attr,
	&dev_attr_dynamic_fps.attr,
	&dev_attr_doze_brightness.attr,
	&dev_attr_brightness_clone.attr,
	&dev_attr_max_brightness_clone.attr,
	&dev_attr_gir.attr,
	&dev_attr_dc_status.attr,
	&dev_attr_led_i2c_reg.attr,
	&dev_attr_panel_event.attr,
	&dev_attr_idle.attr,
#if 0
	&dev_attr_gamma_test.attr,
	&dev_attr_hw_vsync_info.attr,
#endif
	NULL
};

static const struct attribute_group disp_feature_group = {
	.attrs = disp_feature_attrs,
};

static const struct attribute_group *disp_feature_groups[] = {
	&disp_feature_group,
	NULL
};

int mi_disp_create_device_attributes(struct device *dev)
{
	return sysfs_create_groups(&dev->kobj, disp_feature_groups);
}

void mi_disp_remove_device_attributes(struct device *dev)
{
	sysfs_remove_groups(&dev->kobj, disp_feature_groups);
}

ssize_t mi_drm_sysfs_read_panel_info(struct drm_connector *connector,
			char *buf)
{
	struct mtk_dsi *dsi = NULL;

	if (!connector) {
		pr_err("Invalid connector/encoder/bridge ptr\n");
		return -EINVAL;
	}

	dsi = (struct mtk_dsi *)to_mtk_dsi(connector);
	return mi_dsi_display_read_panel_info(dsi, buf);
}
