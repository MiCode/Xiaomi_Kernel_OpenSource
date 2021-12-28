// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#define pr_fmt(fmt)	"mi-disp-debugfs:[%s:%d] " fmt, __func__, __LINE__
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/delay.h>

#include <drm/mi_disp.h>

#include "dsi_panel.h"
#include "mi_disp_config.h"
#include "mi_disp_print.h"
#include "mi_disp_core.h"
#include "mi_disp_feature.h"
#include "mi_disp_lhbm.h"
#include "mi_sde_connector.h"

#if MI_DISP_DEBUGFS_ENABLE

#define DEBUG_LOG_DEBUGFS_NAME      "debug_log"
#define BACKLIGHT_LOG_DEBUGFS_NAME  "backlight_log"
#define FOD_TEST_DEBUGFS_NAME       "fod_test"

const char *esd_sw_str[MI_DISP_MAX] = {
	[MI_DISP_PRIMARY] = "esd_sw_prim",
	[MI_DISP_SECONDARY] = "esd_sw_sec",
};

struct disp_debugfs_t {
	struct dentry *debug_log;
	bool debug_log_enabled;
	bool backlight_log_initialized;
	u32 backlight_log_mask;
	struct dentry *esd_sw[MI_DISP_MAX];
	struct dentry *fod_test;
};

static struct disp_debugfs_t disp_debugfs = {0};

bool is_enable_debug_log(void)
{
	if (disp_debugfs.debug_log_enabled)
		return true;
	else
		return false;
}

u32 mi_get_backlight_log_mask(void)
{
	return disp_debugfs.backlight_log_mask;
}

static int mi_disp_debugfs_debug_log_init(void)
{
	struct disp_core *disp_core = mi_get_disp_core();
	int ret = 0;

	if (!disp_core) {
		DISP_ERROR("invalid disp_display or disp_core ptr\n");
		return -EINVAL;
	}

	if (disp_debugfs.debug_log) {
		DISP_DEBUG("debugfs entry %s already created, return!\n", DEBUG_LOG_DEBUGFS_NAME);
		return 0;
	}

	disp_debugfs.debug_log = debugfs_create_bool(DEBUG_LOG_DEBUGFS_NAME,
		S_IRUGO | S_IWUSR, disp_core->debugfs_dir,
		&disp_debugfs.debug_log_enabled);
	if (IS_ERR_OR_NULL(disp_debugfs.debug_log)) {
		DISP_ERROR("create debugfs entry failed for %s\n", DEBUG_LOG_DEBUGFS_NAME);
		ret = -ENODEV;
	} else {
		DISP_INFO("create debugfs %s success!\n", DEBUG_LOG_DEBUGFS_NAME);
		ret = 0;
	}

	return ret;
}

static int mi_disp_debugfs_backlight_log_init(void)
{
	struct disp_core *disp_core = mi_get_disp_core();


	if (!disp_core) {
		DISP_ERROR("invalid disp_display or disp_core ptr\n");
		return -EINVAL;
	}

	if (disp_debugfs.backlight_log_initialized) {
		DISP_DEBUG("debugfs entry %s already created, return!\n", BACKLIGHT_LOG_DEBUGFS_NAME);
		return 0;
	}

	debugfs_create_u32(BACKLIGHT_LOG_DEBUGFS_NAME, S_IRUGO | S_IWUSR,
		disp_core->debugfs_dir, &disp_debugfs.backlight_log_mask);

	DISP_INFO("create debugfs %s success!\n", BACKLIGHT_LOG_DEBUGFS_NAME);

	disp_debugfs.backlight_log_initialized = true;

	return 0;
}

static int mi_disp_debugfs_esd_sw_open(struct inode *inode, struct file *file)
{
	/* non-seekable */
	file->private_data = inode->i_private;
	return nonseekable_open(inode, file);
}

static ssize_t mi_disp_debugfs_esd_sw_write(struct file *file,
			const char __user *p, size_t count, loff_t *ppos)
{
	struct disp_display *dd_ptr = file->private_data;
	char *input;
	int ret = 0;

	if (dd_ptr->intf_type != MI_INTF_DSI) {
		DISP_ERROR("unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		return -EINVAL;
	}

	input = kmalloc(count + 1, GFP_KERNEL);
	if (!input)
		return -ENOMEM;

	if (copy_from_user(input, p, count)) {
		DISP_ERROR("copy from user failed\n");
		ret = -EFAULT;
		goto exit;
	}
	input[count] = '\0';
	DISP_INFO("[esd-test]intput: %s\n", input);

	if (!strncmp(input, "1", 1) || !strncmp(input, "on", 2) ||
		!strncmp(input, "true", 4)) {
		DISP_INFO("[esd-test]panel esd irq trigging \n");
	} else {
		goto exit;
	}

	ret = mi_sde_connector_debugfs_esd_sw_trigger(dd_ptr->display);

exit:
	kfree(input);
	return ret ? ret : count;
}

static const struct file_operations esd_sw_debugfs_fops = {
	.open  = mi_disp_debugfs_esd_sw_open,
	.write = mi_disp_debugfs_esd_sw_write,
};

static int mi_disp_debugfs_esd_sw_init(void *d_display, int disp_id)
{
	struct disp_display *dd_ptr = (struct disp_display *)d_display;
	struct disp_core *disp_core = mi_get_disp_core();
	int ret = 0;

	if (!dd_ptr || !disp_core) {
		DISP_ERROR("invalid disp_display or disp_core ptr\n");
		return -EINVAL;
	}

	if (dd_ptr->intf_type != MI_INTF_DSI) {
		DISP_ERROR("unsupported %s display(%s intf)\n", get_disp_id_name(disp_id),
			get_disp_intf_type_name(dd_ptr->intf_type));
		return -EINVAL;
	}

	if (is_support_disp_id(disp_id)) {
		disp_debugfs.esd_sw[disp_id] = debugfs_create_file(esd_sw_str[disp_id],
			S_IWUGO, disp_core->debugfs_dir,
			dd_ptr, &esd_sw_debugfs_fops);

		if (IS_ERR_OR_NULL(disp_debugfs.esd_sw[disp_id])) {
			DISP_ERROR("create debugfs entry failed for %s\n", esd_sw_str[disp_id]);
			ret = -ENODEV;
		} else {
			DISP_INFO("create debugfs %s success!\n", esd_sw_str[disp_id]);
			ret = 0;
		}
	} else {
		DISP_INFO("unknown display id\n");
		ret = -EINVAL;
	}

	return ret;
}

static int mi_disp_debugfs_esd_sw_deinit(void *d_display, int disp_id)
{
	struct disp_display *dd_ptr = (struct disp_display *)d_display;
	int ret = 0;

	if (!dd_ptr) {
		DISP_ERROR("invalid disp_display ptr\n");
		return -EINVAL;
	}

	if (dd_ptr->intf_type != MI_INTF_DSI) {
		DISP_ERROR("unsupported %s display(%s intf)\n", get_disp_id_name(disp_id),
			get_disp_intf_type_name(dd_ptr->intf_type));
		return -EINVAL;
	}

	if (is_support_disp_id(disp_id)) {
		if (disp_debugfs.esd_sw[disp_id]) {
			debugfs_remove(disp_debugfs.esd_sw[disp_id]);
			disp_debugfs.esd_sw[disp_id] = NULL;
		}
	} else {
		DISP_ERROR("unknown display id\n");
		ret = -EINVAL;
	}

	return ret;
}

static int mi_disp_debugfs_fod_test_open(struct inode *inode, struct file *file)
{
	/* non-seekable */
	file->private_data = inode->i_private;
	return nonseekable_open(inode, file);
}

static ssize_t mi_disp_debugfs_fod_test_write(struct file *file,
			const char __user *p, size_t count, loff_t *ppos)
{
	struct disp_display *dd_ptr = file->private_data;
	char *token, *input, *input_dup = NULL;
	const char *delim = " ";
	int ret = 0, i = 0;
	int retry_count = 0;
	int down_sleep_ms = 0, up_sleep_ms = 0;

	if (dd_ptr->intf_type != MI_INTF_DSI) {
		DISP_ERROR("unsupported display(%s intf)\n",
			get_disp_intf_type_name(dd_ptr->intf_type));
		return -EINVAL;
	}

	input = kmalloc(count + 1, GFP_KERNEL);
	if (!input)
		return -ENOMEM;

	if (copy_from_user(input, p, count)) {
		DISP_ERROR("copy from user failed\n");
		ret = -EFAULT;
		goto exit;
	}
	input[count] = '\0';
	DISP_INFO("[fod-test]intput: %s\n", input);

	input_dup = input;
	/* removes leading and trailing whitespace from input */
	input = strim(input);
	/* Split a string into token */
	token = strsep(&input, delim);
	if (token) {
		ret = kstrtoint(token, 10, &down_sleep_ms);
		if (ret) {
			DISP_ERROR("input buffer conversion failed\n");
			goto exit;
		}
		DISP_INFO("[fod-test]finger down sleep: %d(ms)\n", down_sleep_ms);
	}

	/* Removes leading whitespace from input_copy */
	if (input)
		input = skip_spaces(input);
	else
		goto exit;

	token = strsep(&input, delim);
	if (token) {
		ret = kstrtoint(token, 10, &up_sleep_ms);
		if (ret) {
			DISP_ERROR("input buffer conversion failed\n");
			goto exit;
		}
		DISP_INFO("[fod-test]finger up sleep: %d\n", up_sleep_ms);
	}

	/* Removes leading whitespace from input_copy */
	if (input)
		input = skip_spaces(input);
	else
		goto exit;

	token = strsep(&input, delim);
	if (token) {
		ret = kstrtoint(token, 10, &retry_count);
		if (ret) {
			DISP_ERROR("input buffer conversion failed\n");
			goto exit;
		}
		DISP_INFO("[fod-test]retry_count: %d\n", retry_count);
	}

	for (i = 0; i < retry_count; i++) {
		mi_disp_set_fod_queue_work(1, true);
		usleep_range((down_sleep_ms * 1000),
			(down_sleep_ms * 1000) + 10);
		mi_disp_set_fod_queue_work(0, true);
		usleep_range((up_sleep_ms * 1000),
			(up_sleep_ms * 1000) + 10);
	}

exit:
	kfree(input_dup);
	return ret ? ret : count;
}

static const struct file_operations fod_test_debugfs_fops = {
	.open  = mi_disp_debugfs_fod_test_open,
	.write = mi_disp_debugfs_fod_test_write,
};

static int mi_disp_debugfs_fod_test_init(void *d_display, int disp_id)
{
	struct disp_display *dd_ptr = (struct disp_display *)d_display;
	struct disp_core *disp_core = mi_get_disp_core();
	int ret = 0;

	if (!dd_ptr || !disp_core) {
		DISP_ERROR("invalid disp_display or disp_core ptr\n");
		return -EINVAL;
	}

	if (dd_ptr->intf_type != MI_INTF_DSI) {
		DISP_ERROR("unsupported %s display(%s intf)\n", get_disp_id_name(disp_id),
			get_disp_intf_type_name(dd_ptr->intf_type));
		return -EINVAL;
	}

	if (disp_debugfs.fod_test) {
		DISP_DEBUG("debugfs entry %s already created, return!\n", FOD_TEST_DEBUGFS_NAME);
		return 0;
	}

	if (is_support_disp_id(disp_id)) {
		disp_debugfs.fod_test = debugfs_create_file(FOD_TEST_DEBUGFS_NAME,
			S_IWUGO, disp_core->debugfs_dir,
			dd_ptr, &fod_test_debugfs_fops);
		if (!disp_debugfs.fod_test) {
			DISP_ERROR("create debugfs entry failed for %s\n", FOD_TEST_DEBUGFS_NAME);
			ret = -ENODEV;
		} else {
			DISP_INFO("create debugfs %s success!\n", FOD_TEST_DEBUGFS_NAME);
			ret = 0;
		}
	} else {
		DISP_INFO("unknown display id\n");
		ret = -EINVAL;
	}

	return ret;
}

static int mi_disp_debugfs_fod_test_deinit(void *d_display, int disp_id)
{
	struct disp_display *dd_ptr = (struct disp_display *)d_display;
	int ret = 0;

	if (!dd_ptr) {
		DISP_ERROR("invalid disp_display ptr\n");
		return -EINVAL;
	}

	if (dd_ptr->intf_type != MI_INTF_DSI) {
		DISP_ERROR("unsupported %s display(%s intf)\n", get_disp_id_name(disp_id),
			get_disp_intf_type_name(dd_ptr->intf_type));
		return -EINVAL;
	}

	if (is_support_disp_id(disp_id)) {
		if (disp_debugfs.fod_test) {
			debugfs_remove(disp_debugfs.fod_test);
			disp_debugfs.fod_test = NULL;
		}
	} else {
		DISP_ERROR("unknown display id\n");
		ret = -EINVAL;
	}

	return ret;
}


int mi_disp_debugfs_init(void *d_display, int disp_id)
{
	int ret = 0;

	ret = mi_disp_debugfs_debug_log_init();
	ret |= mi_disp_debugfs_backlight_log_init();
	ret |= mi_disp_debugfs_esd_sw_init(d_display, disp_id);
	ret |= mi_disp_debugfs_fod_test_init(d_display, disp_id);

	return ret;
}

int mi_disp_debugfs_deinit(void *d_display, int disp_id)
{
	int ret = 0;

	ret = mi_disp_debugfs_fod_test_deinit(d_display, disp_id);
	ret |= mi_disp_debugfs_esd_sw_deinit(d_display, disp_id);

	return ret;
}

#endif
