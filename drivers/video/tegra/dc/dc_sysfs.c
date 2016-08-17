/*
 * drivers/video/tegra/dc/dc_sysfs.c
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION, All rights reserved.
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

#include <linux/fb.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>

#include <mach/dc.h>
#include <mach/fb.h>

#include <linux/../../drivers/video/tegra/nvmap/nvmap.h>
#include "dc_reg.h"
#include "dc_priv.h"
#include "nvsd.h"

static ssize_t mode_show(struct device *device,
	struct device_attribute *attr, char *buf)
{
	struct platform_device *ndev = to_platform_device(device);
	struct tegra_dc *dc = platform_get_drvdata(ndev);
	struct tegra_dc_mode *m;
	ssize_t res;

	mutex_lock(&dc->lock);
	m = &dc->mode;
	res = snprintf(buf, PAGE_SIZE,
		"pclk: %d\n"
		"h_ref_to_sync: %d\n"
		"v_ref_to_sync: %d\n"
		"h_sync_width: %d\n"
		"v_sync_width: %d\n"
		"h_back_porch: %d\n"
		"v_back_porch: %d\n"
		"h_active: %d\n"
		"v_active: %d\n"
		"h_front_porch: %d\n"
		"v_front_porch: %d\n"
		"stereo_mode: %d\n",
		m->pclk, m->h_ref_to_sync, m->v_ref_to_sync,
		m->h_sync_width, m->v_sync_width,
		m->h_back_porch, m->v_back_porch,
		m->h_active, m->v_active,
		m->h_front_porch, m->v_front_porch,
		m->stereo_mode);
	mutex_unlock(&dc->lock);

	return res;
}

static DEVICE_ATTR(mode, S_IRUGO, mode_show, NULL);

static ssize_t stats_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct platform_device *ndev = to_platform_device(dev);
	struct tegra_dc *dc = platform_get_drvdata(ndev);
	bool enabled;

	if (mutex_lock_killable(&dc->lock))
		return -EINTR;
	enabled = tegra_dc_stats_get(dc);
	mutex_unlock(&dc->lock);

	return snprintf(buf, PAGE_SIZE, "%d", enabled);
}

static ssize_t stats_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct platform_device *ndev = to_platform_device(dev);
	struct tegra_dc *dc = platform_get_drvdata(ndev);
	unsigned long val = 0;

	if (strict_strtoul(buf, 10, &val) < 0)
		return -EINVAL;

	if (mutex_lock_killable(&dc->lock))
		return -EINTR;
	tegra_dc_stats_enable(dc, !!val);
	mutex_unlock(&dc->lock);

	return count;
}

static DEVICE_ATTR(stats_enable, S_IRUGO|S_IWUSR,
	stats_enable_show, stats_enable_store);

static ssize_t enable_show(struct device *device,
	struct device_attribute *attr, char *buf)
{
	struct platform_device *ndev = to_platform_device(device);
	struct tegra_dc *dc = platform_get_drvdata(ndev);
	ssize_t res;

	mutex_lock(&dc->lock);
	res = snprintf(buf, PAGE_SIZE, "%d\n", dc->enabled);
	mutex_unlock(&dc->lock);
	return res;
}

static ssize_t enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct platform_device *ndev = to_platform_device(dev);
	struct tegra_dc *dc = platform_get_drvdata(ndev);
	unsigned long val = 0;

	if (strict_strtoul(buf, 10, &val) < 0)
		return -EINVAL;

	if (val) {
		tegra_dc_enable(dc);
	} else {
		tegra_dc_disable(dc);
	}

	return count;
}

static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR, enable_show, enable_store);

#ifdef CONFIG_TEGRA_DC_WIN_H
static ssize_t win_h_show(struct device *device,
	struct device_attribute *attr, char *buf)
{
	struct platform_device *ndev = to_platform_device(device);
	struct tegra_dc *dc = platform_get_drvdata(ndev);
	unsigned long val = 0;

	mutex_lock(&dc->lock);
	tegra_dc_io_start(dc);

	val = tegra_dc_readl(dc, DC_DISP_BLEND_CURSOR_CONTROL);

	tegra_dc_io_end(dc);
	mutex_unlock(&dc->lock);

	return snprintf(buf, PAGE_SIZE, "%u\n", !!(val & WINH_CURS_SELECT(1)));
}

/* win_h sysfs controls hybrid window.
 *
 * 0 = cursor mode and 1 = window mode (default on T14x) */
static ssize_t win_h_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct platform_device *ndev = to_platform_device(dev);
	struct tegra_dc *dc = platform_get_drvdata(ndev);
	unsigned int val = 0;
	unsigned long cursor_val = 0;

	if (!dc->enabled) {
		dev_err(&dc->ndev->dev, "%s: DC not enabled.\n", __func__);
		return -EFAULT;
	}

	if (kstrtouint(buf, 10, &val) < 0)
		return -EINVAL;

	mutex_lock(&dc->lock);
	tegra_dc_io_start(dc);

	cursor_val = tegra_dc_readl(dc, DC_DISP_BLEND_CURSOR_CONTROL);
	cursor_val &= ~WINH_CURS_SELECT(1);
	if (val)
		cursor_val |= WINH_CURS_SELECT(1);
	tegra_dc_writel(dc, cursor_val, DC_DISP_BLEND_CURSOR_CONTROL);

	tegra_dc_io_end(dc);
	mutex_unlock(&dc->lock);

	return count;
}

static DEVICE_ATTR(win_h, S_IRUGO|S_IWUSR, win_h_show, win_h_store);
#endif

static ssize_t crc_checksum_latched_show(struct device *device,
	struct device_attribute *attr, char *buf)
{
	struct platform_device *ndev = to_platform_device(device);
	struct tegra_dc *dc = platform_get_drvdata(ndev);

	u32 crc;

	if (!dc->enabled) {
		dev_err(&dc->ndev->dev, "%s: DC not enabled.\n", __func__);
		return -EFAULT;
	}

	crc = tegra_dc_read_checksum_latched(dc);

	return snprintf(buf, PAGE_SIZE, "%u", crc);
}

static ssize_t crc_checksum_latched_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct platform_device *ndev = to_platform_device(dev);
	struct tegra_dc *dc = platform_get_drvdata(ndev);
	unsigned long val = 0;

	if (!dc->enabled) {
		dev_err(&dc->ndev->dev, "%s: DC not enabled.\n", __func__);
		return -EFAULT;
	}

	if (strict_strtoul(buf, 10, &val) < 0)
		return -EINVAL;

	if (val == 1) {
		tegra_dc_enable_crc(dc);
		dev_dbg(&dc->ndev->dev, "crc is enabled.\n");
	} else if (val == 0) {
		tegra_dc_disable_crc(dc);
		dev_dbg(&dc->ndev->dev, "crc is disabled.\n");
	} else
		dev_err(&dc->ndev->dev, "Invalid input.\n");

	return count;
}
static DEVICE_ATTR(crc_checksum_latched, S_IRUGO|S_IWUSR,
		crc_checksum_latched_show, crc_checksum_latched_store);

#define ORIENTATION_PORTRAIT	"portrait"
#define ORIENTATION_LANDSCAPE	"landscape"

static ssize_t orientation_3d_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct platform_device *ndev = to_platform_device(dev);
	struct tegra_dc *dc = platform_get_drvdata(ndev);
	struct tegra_dc_out *dc_out = dc->out;
	const char *orientation;
	switch (dc_out->stereo->orientation) {
	case TEGRA_DC_STEREO_LANDSCAPE:
		orientation = ORIENTATION_LANDSCAPE;
		break;
	case TEGRA_DC_STEREO_PORTRAIT:
		orientation = ORIENTATION_PORTRAIT;
		break;
	default:
		pr_err("Invalid value is stored for stereo_orientation.\n");
		return -EINVAL;
	}
	return snprintf(buf, PAGE_SIZE, "%s\n", orientation);
}

static ssize_t orientation_3d_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct platform_device *ndev = to_platform_device(dev);
	struct tegra_dc *dc = platform_get_drvdata(ndev);
	struct tegra_dc_out *dc_out = dc->out;
	struct tegra_stereo_out *stereo = dc_out->stereo;
	int orientation;

	if (0 == strncmp(buf, ORIENTATION_PORTRAIT,
			min(cnt, ARRAY_SIZE(ORIENTATION_PORTRAIT) - 1))) {
		orientation = TEGRA_DC_STEREO_PORTRAIT;
	} else if (0 == strncmp(buf, ORIENTATION_LANDSCAPE,
			min(cnt, ARRAY_SIZE(ORIENTATION_LANDSCAPE) - 1))) {
		orientation = TEGRA_DC_STEREO_LANDSCAPE;
	} else {
		pr_err("Invalid property value for stereo_orientation.\n");
		return -EINVAL;
	}
	stereo->orientation = orientation;
	stereo->set_orientation(orientation);
	return cnt;
}

static DEVICE_ATTR(stereo_orientation,
	S_IRUGO|S_IWUSR, orientation_3d_show, orientation_3d_store);

#define MODE_2D		"2d"
#define MODE_3D		"3d"

static ssize_t mode_3d_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct platform_device *ndev = to_platform_device(dev);
	struct tegra_dc *dc = platform_get_drvdata(ndev);
	struct tegra_dc_out *dc_out = dc->out;
	const char *mode;
	switch (dc_out->stereo->mode_2d_3d) {
	case TEGRA_DC_STEREO_MODE_2D:
		mode = MODE_2D;
		break;
	case TEGRA_DC_STEREO_MODE_3D:
		mode = MODE_3D;
		break;
	default:
		pr_err("Invalid value is stored for stereo_mode.\n");
		return -EINVAL;
	}
	return snprintf(buf, PAGE_SIZE, "%s\n", mode);
}

static ssize_t mode_3d_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct platform_device *ndev = to_platform_device(dev);
	struct tegra_dc *dc = platform_get_drvdata(ndev);
	struct tegra_dc_out *dc_out = dc->out;
	struct tegra_stereo_out *stereo = dc_out->stereo;
	int mode;

	if (0 == strncmp(buf, MODE_2D, min(cnt, ARRAY_SIZE(MODE_2D) - 1))) {
		mode = TEGRA_DC_STEREO_MODE_2D;
	} else if (0 == strncmp(buf, MODE_3D,
			min(cnt, ARRAY_SIZE(MODE_3D) - 1))) {
		mode = TEGRA_DC_STEREO_MODE_3D;
	} else {
		pr_err("Invalid property value for stereo_mode.\n");
		return -EINVAL;
	}
	stereo->mode_2d_3d = mode;
	stereo->set_mode(mode);
	return cnt;
}

static DEVICE_ATTR(stereo_mode,
	S_IRUGO|S_IWUSR, mode_3d_show, mode_3d_store);

static ssize_t nvdps_show(struct device *device,
	struct device_attribute *attr, char *buf)
{
	int refresh_rate;
	struct platform_device *ndev = to_platform_device(device);
	struct tegra_dc *dc = platform_get_drvdata(ndev);

	refresh_rate = tegra_fb_get_mode(dc);
	return snprintf(buf, PAGE_SIZE, "%d\n", refresh_rate);
}


static ssize_t nvdps_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct platform_device *ndev = to_platform_device(dev);
	struct tegra_dc *dc = platform_get_drvdata(ndev);
	int refresh_rate;
	int e;

	e = kstrtoint(buf, 10, &refresh_rate);
	if (e)
		return e;
	e = tegra_fb_set_mode(dc, refresh_rate);

	return count;
}

static DEVICE_ATTR(nvdps, S_IRUGO|S_IWUSR, nvdps_show, nvdps_store);

#ifdef CONFIG_TEGRA_DC_CMU
static ssize_t cmu_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int val;
	int e;
	struct platform_device *ndev = to_platform_device(dev);
	struct tegra_dc *dc = platform_get_drvdata(ndev);

	e = kstrtoint(buf, 10, &val);
	if (e)
		return e;

	tegra_dc_cmu_enable(dc, val);

	return count;
}

static ssize_t cmu_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct platform_device *ndev = to_platform_device(dev);
	struct tegra_dc *dc = platform_get_drvdata(ndev);

	return snprintf(buf, PAGE_SIZE, "%d\n", dc->pdata->cmu_enable);
}

static DEVICE_ATTR(cmu_enable,
		S_IRUGO|S_IWUSR, cmu_enable_show, cmu_enable_store);
#endif
static ssize_t smart_panel_show(struct device *device,
	struct device_attribute *attr, char  *buf)
{
	return snprintf(buf, PAGE_SIZE, "1\n");
}

static DEVICE_ATTR(smart_panel, S_IRUGO, smart_panel_show, NULL);

void __devexit tegra_dc_remove_sysfs(struct device *dev)
{
	struct platform_device *ndev = to_platform_device(dev);
	struct tegra_dc *dc = platform_get_drvdata(ndev);
	struct tegra_dc_sd_settings *sd_settings = dc->out->sd_settings;

	device_remove_file(dev, &dev_attr_mode);
	device_remove_file(dev, &dev_attr_nvdps);
	device_remove_file(dev, &dev_attr_enable);
	device_remove_file(dev, &dev_attr_stats_enable);
	device_remove_file(dev, &dev_attr_crc_checksum_latched);
#ifdef CONFIG_TEGRA_DC_WIN_H
	device_remove_file(dev, &dev_attr_win_h);
#endif
#ifdef CONFIG_TEGRA_DC_CMU
	device_remove_file(dev, &dev_attr_cmu_enable);
#endif

	if (dc->out->stereo) {
		device_remove_file(dev, &dev_attr_stereo_orientation);
		device_remove_file(dev, &dev_attr_stereo_mode);
	}

	if (sd_settings)
		nvsd_remove_sysfs(dev);

	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE)
		device_remove_file(dev, &dev_attr_smart_panel);
}

void tegra_dc_create_sysfs(struct device *dev)
{
	struct platform_device *ndev = to_platform_device(dev);
	struct tegra_dc *dc = platform_get_drvdata(ndev);
	struct tegra_dc_sd_settings *sd_settings = dc->out->sd_settings;
	int error = 0;

	error |= device_create_file(dev, &dev_attr_mode);
	error |= device_create_file(dev, &dev_attr_nvdps);
	error |= device_create_file(dev, &dev_attr_enable);
	error |= device_create_file(dev, &dev_attr_stats_enable);
	error |= device_create_file(dev, &dev_attr_crc_checksum_latched);
#ifdef CONFIG_TEGRA_DC_WIN_H
	error |= device_create_file(dev, &dev_attr_win_h);
#endif
#ifdef CONFIG_TEGRA_DC_CMU
	error |= device_create_file(dev, &dev_attr_cmu_enable);
#endif

	if (dc->out->stereo) {
		error |= device_create_file(dev, &dev_attr_stereo_orientation);
		error |= device_create_file(dev, &dev_attr_stereo_mode);
	}

	if (sd_settings)
		error |= nvsd_create_sysfs(dev);

	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE)
		error |= device_create_file(dev, &dev_attr_smart_panel);

	if (error)
		dev_err(&ndev->dev, "Failed to create sysfs attributes!\n");
}
