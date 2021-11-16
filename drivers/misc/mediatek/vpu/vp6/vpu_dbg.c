/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>

#ifdef CONFIG_MTK_M4U
#include <m4u.h>
#endif
#include "vpu_dbg.h"
#include "vpu_cmn.h"

#if IS_ENABLED(CONFIG_DEBUG_FS)

static int vpu_log_level_set(void *data, u64 val)
{
	struct vpu_device *vpu_device = (struct vpu_device *)data;

	vpu_device->vpu_log_level = val & 0xf;
	LOG_INF("g_vpu_log_level: %d\n", vpu_device->vpu_log_level);
	return 0;
}

static int vpu_log_level_get(void *data, u64 *val)
{
	struct vpu_device *vpu_device = (struct vpu_device *)data;

	*val = vpu_device->vpu_log_level;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(vpu_debug_log_level_fops, vpu_log_level_get,
				vpu_log_level_set, "%llu\n");

static int vpu_internal_log_level_set(void *data, u64 val)
{
	struct vpu_device *vpu_device = (struct vpu_device *)data;

	vpu_device->vpu_internal_log_level = val;
	LOG_INF("g_vpu_internal_log_level: %d\n",
			vpu_device->vpu_internal_log_level);
	return 0;
}

static int vpu_internal_log_level_get(void *data, u64 *val)
{
	struct vpu_device *vpu_device = (struct vpu_device *)data;

	*val = vpu_device->vpu_internal_log_level;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(vpu_debug_internal_log_level_fops,
	vpu_internal_log_level_get,
	vpu_internal_log_level_set,
	"%llu\n");

static int vpu_func_mask_set(void *data, u64 val)
{
	struct vpu_device *vpu_device = (struct vpu_device *)data;

	vpu_device->func_mask = val & 0xffffffff;
	LOG_INF("g_func_mask: 0x%x\n", vpu_device->func_mask);
	return 0;
}

static int vpu_func_mask_get(void *data, u64 *val)
{
	struct vpu_device *vpu_device = (struct vpu_device *)data;

	*val = vpu_device->func_mask;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(vpu_debug_func_mask_fops, vpu_func_mask_get,
				vpu_func_mask_set, "%llu\n");

#define IMPLEMENT_VPU_DEBUGFS(name)					\
static int vpu_debug_## name ##_show(struct seq_file *s, void *unused)\
{					\
	vpu_dump_## name(s, NULL);		\
	return 0;			\
}					\
static int vpu_debug_## name ##_open(struct inode *inode, struct file *file) \
{					\
	return single_open(file, vpu_debug_ ## name ## _show, \
				inode->i_private); \
}                                                                             \
static const struct file_operations vpu_debug_ ## name ## _fops = {   \
	.open = vpu_debug_ ## name ## _open,                               \
	.read = seq_read,                                                    \
	.llseek = seq_lseek,                                                \
	.release = seq_release,                                             \
}

/*IMPLEMENT_VPU_DEBUGFS(algo);*/
IMPLEMENT_VPU_DEBUGFS(register);
IMPLEMENT_VPU_DEBUGFS(user);
IMPLEMENT_VPU_DEBUGFS(vpu);
IMPLEMENT_VPU_DEBUGFS(image_file);
IMPLEMENT_VPU_DEBUGFS(mesg);
IMPLEMENT_VPU_DEBUGFS(opp_table);
IMPLEMENT_VPU_DEBUGFS(device_dbg);
IMPLEMENT_VPU_DEBUGFS(user_algo);
IMPLEMENT_VPU_DEBUGFS(vpu_memory);

#undef IMPLEMENT_VPU_DEBUGFS

static int vpu_debug_power_show(struct seq_file *s, void *unused)
{
	vpu_dump_power(s, NULL);
	return 0;
}

static int vpu_debug_power_open(struct inode *inode, struct file *file)
{
	return single_open(file, vpu_debug_power_show, inode->i_private);
}

static ssize_t vpu_debug_power_write(struct file *flip,
		const char __user *buffer,
		size_t count, loff_t *f_pos)
{
	char *tmp, *token, *cursor;
	int ret, i, param;
	const int max_arg = 5;
	unsigned int args[max_arg];
	struct seq_file *s = flip->private_data;
	struct vpu_device *vpu_device = s->private;

	tmp = kzalloc(count + 1, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	ret = copy_from_user(tmp, buffer, count);
	if (ret) {
		LOG_ERR("copy_from_user failed, ret=%d\n", ret);
		goto out;
	}

	tmp[count] = '\0';

	cursor = tmp;

	/* parse a command */
	token = strsep(&cursor, " ");
	if (strcmp(token, "fix_opp") == 0)
		param = VPU_POWER_PARAM_FIX_OPP;
	else if (strcmp(token, "dvfs_debug") == 0)
		param = VPU_POWER_PARAM_DVFS_DEBUG;
	else if (strcmp(token, "jtag") == 0)
		param = VPU_POWER_PARAM_JTAG;
	else if (strcmp(token, "lock") == 0)
		param = VPU_POWER_PARAM_LOCK;
	else if (strcmp(token, "volt_step") == 0)
		param = VPU_POWER_PARAM_VOLT_STEP;
	else if (strcmp(token, "power_hal") == 0)
		param = VPU_POWER_HAL_CTL;
	else if (strcmp(token, "eara") == 0)
		param = VPU_EARA_CTL;
	else if (strcmp(token, "ct") == 0)
		param = VPU_CT_INFO;
	else {
		ret = -EINVAL;
		LOG_ERR("no power param[%s]!\n", token);
		goto out;
	}

	/* parse arguments */
	for (i = 0; i < max_arg && (token = strsep(&cursor, " ")); i++) {
		ret = kstrtouint(token, 10, &args[i]);
		if (ret) {
			LOG_ERR("fail to parse args[%d]\n", i);
			goto out;
		}
	}

	vpu_set_power_parameter(vpu_device, param, i, args);

	ret = count;
out:

	kfree(tmp);
	return ret;
}

static const struct file_operations vpu_debug_power_fops = {
	.open = vpu_debug_power_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
	.write = vpu_debug_power_write,
};

static int vpu_debug_algo_show(struct seq_file *s, void *unused)
{
	vpu_dump_algo(s, NULL);
	return 0;
}

static int vpu_debug_algo_open(struct inode *inode, struct file *file)
{
	return single_open(file, vpu_debug_algo_show, inode->i_private);
}

static ssize_t vpu_debug_algo_write(struct file *flip,
		const char __user *buffer,
		size_t count, loff_t *f_pos)
{
	char *tmp, *token, *cursor;
	int ret, i, param;
	const int max_arg = 5;
	unsigned int args[max_arg];
	struct seq_file *s = flip->private_data;
	struct vpu_device *vpu_device = s->private;

	tmp = kzalloc(count + 1, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	ret = copy_from_user(tmp, buffer, count);
	if (ret) {
		LOG_ERR("copy_from_user failed, ret=%d\n", ret);
		goto out;
	}

	tmp[count] = '\0';

	cursor = tmp;

	/* parse a command */
	token = strsep(&cursor, " ");
	if (strcmp(token, "dump_algo") == 0)
		param = VPU_DEBUG_ALGO_PARAM_DUMP_ALGO;
	else {
		ret = -EINVAL;
		LOG_ERR("no power param[%s]!\n", token);
		goto out;
	}

	/* parse arguments */
	for (i = 0; i < max_arg && (token = strsep(&cursor, " ")); i++) {
		ret = kstrtouint(token, 10, &args[i]);
		if (ret) {
			LOG_ERR("fail to parse args[%d]\n", i);
			goto out;
		}
	}

	vpu_set_algo_parameter(vpu_device, param, i, args);

	ret = count;
out:

	kfree(tmp);
	return ret;
}

static const struct file_operations vpu_debug_algo_fops = {
	.open = vpu_debug_algo_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
	.write = vpu_debug_algo_write,
};

static int vpu_debug_util_show(struct seq_file *s, void *unused)
{
	vpu_dump_util(s, NULL);
	return 0;
}

static int vpu_debug_util_open(struct inode *inode, struct file *file)
{
	return single_open(file, vpu_debug_util_show, inode->i_private);
}

static ssize_t vpu_debug_util_write(struct file *flip,
		const char __user *buffer,
		size_t count, loff_t *f_pos)
{
	char *tmp, *token, *cursor;
	int ret, i, param;
	const int max_arg = 5;
	unsigned int args[max_arg];
	struct seq_file *s = flip->private_data;
	struct vpu_device *vpu_device = s->private;

	tmp = kzalloc(count + 1, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	ret = copy_from_user(tmp, buffer, count);
	if (ret) {
		LOG_ERR("copy_from_user failed, ret=%d\n", ret);
		goto out;
	}

	tmp[count] = '\0';

	cursor = tmp;

	/* parse a command */
	token = strsep(&cursor, " ");
	if (strcmp(token, "period") == 0)
		param = VPU_DEBUG_UTIL_PERIOD;
	else if (strcmp(token, "enable") == 0)
		param = VPU_DEBUG_UTIL_ENABLE;
	else {
		ret = -EINVAL;
		LOG_ERR("no power param[%s]!\n", token);
		goto out;
	}

	/* parse arguments */
	for (i = 0; i < max_arg && (token = strsep(&cursor, " ")); i++) {
		ret = kstrtouint(token, 10, &args[i]);
		if (ret) {
			LOG_ERR("fail to parse args[%d]\n", i);
			goto out;
		}
	}

	vpu_set_util_test_parameter(vpu_device, param, i, args);

	ret = count;
out:

	kfree(tmp);
	return ret;
}

static const struct file_operations vpu_debug_util_fops = {
	.open = vpu_debug_util_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
	.write = vpu_debug_util_write,
};

static int vpu_debug_sec_test_show(struct seq_file *s, void *unused)
{
	LOG_DBG("%s\n", __func__);
	return 0;
}

static int vpu_debug_sec_test_open(struct inode *inode, struct file *file)
{
	return single_open(file, vpu_debug_sec_test_show, inode->i_private);
}

static ssize_t vpu_debug_sec_test_write(struct file *flip,
		const char __user *buffer,
		size_t count, loff_t *f_pos)
{
	char *tmp, *token, *cursor;
	int ret, i, param;
	const int max_arg = 5;
	unsigned int args[max_arg];
	struct seq_file *s = flip->private_data;
	struct vpu_device *vpu_device = s->private;

	tmp = kzalloc(count + 1, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	ret = copy_from_user(tmp, buffer, count);
	if (ret) {
		LOG_ERR("copy_from_user failed, ret=%d\n", ret);
		goto out;
	}

	tmp[count] = '\0';

	cursor = tmp;

	/* parse a command */
	token = strsep(&cursor, " ");
	if (strcmp(token, "attach") == 0)
		param = VPU_DEBUG_SEC_ATTACH;
	else if (strcmp(token, "load") == 0)
		param = VPU_DEBUG_SEC_LOAD;
	else if (strcmp(token, "execute") == 0)
		param = VPU_DEBUG_SEC_EXCUTE;
	else if (strcmp(token, "unload") == 0)
		param = VPU_DEBUG_SEC_UNLOAD;
	else if (strcmp(token, "detach") == 0)
		param = VPU_DEBUG_SEC_DETACH;
	else if (strcmp(token, "test") == 0)
		param = VPU_DEBUG_SEC_TEST;
	else {
		ret = -EINVAL;
		LOG_ERR("no vpu sec param[%s]!\n", token);
		goto out;
	}

	/* parse arguments */
	for (i = 0; i < max_arg && (token = strsep(&cursor, " ")); i++) {
		ret = kstrtouint(token, 10, &args[i]);
		if (ret) {
			LOG_ERR("fail to parse args[%d]\n", i);
			goto out;
		}
	}

	vpu_set_sec_test_parameter(vpu_device, param, i, args);

	ret = count;
out:

	kfree(tmp);
	return ret;
}

static const struct file_operations vpu_debug_sec_test_fops = {
	.open = vpu_debug_sec_test_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
	.write = vpu_debug_sec_test_write,
};


struct debugfs_param {
	char *name;
	const struct file_operations *fops;
};

static struct debugfs_param param_files[] = {
	{"algo", &vpu_debug_algo_fops},
	{"func_mask", &vpu_debug_func_mask_fops},
	{"log_level", &vpu_debug_log_level_fops},
	{"internal_log_level", &vpu_debug_internal_log_level_fops},
	{"register", &vpu_debug_register_fops},
	{"user", &vpu_debug_user_fops},
	{"image_file", &vpu_debug_image_file_fops},
	{"mesg", &vpu_debug_mesg_fops},
	{"vpu", &vpu_debug_vpu_fops},
	{"opp_table", &vpu_debug_opp_table_fops},
	{"power", &vpu_debug_power_fops},
	{"device_dbg", &vpu_debug_device_dbg_fops},
	{"user_algo", &vpu_debug_user_algo_fops},
	{"vpu_memory", &vpu_debug_vpu_memory_fops},
	{"util", &vpu_debug_util_fops},
	{"sec_test", &vpu_debug_sec_test_fops},
	{NULL, NULL}
};

static void create_debugfs_params(struct vpu_device *vpu_device)
{
	int i = 0;
	struct dentry *debugfs_parent = vpu_device->debug_root;
	struct dentry *debug_file;

	while (param_files[i].name) {
		debug_file = debugfs_create_file(param_files[i].name,
						 0644,
						 debugfs_parent,
						 vpu_device,
						 param_files[i].fops);
		if (IS_ERR_OR_NULL(debug_file))
			LOG_ERR("failed to create debug file[ %s ].\n",
				param_files[i].name);
		i++;
	};
}
#endif

int vpu_init_debug(struct vpu_device *vpu_device)
{
	int ret = 0;

#if IS_ENABLED(CONFIG_DEBUG_FS)
	vpu_device->debug_root = debugfs_create_dir("vpu", NULL);

	ret = IS_ERR_OR_NULL(vpu_device->debug_root);
	if (ret) {
		LOG_ERR("failed to create debug dir.\n");
		goto out;
	}

	create_debugfs_params(vpu_device);
out:
#endif
	return ret;
}

void vpu_deinit_debug(struct vpu_device *vpu_device)
{
#if IS_ENABLED(CONFIG_DEBUG_FS)
	debugfs_remove_recursive(vpu_device->debug_root);
#endif
}

