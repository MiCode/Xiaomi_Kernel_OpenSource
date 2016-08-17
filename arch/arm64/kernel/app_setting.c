/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
#include <linux/cpu.h>
#include <linux/slab.h>
#include <linux/notifier.h>

#include <asm/app_api.h>

#define MAX_LEN		100

static char *lib_names[MAX_ENTRIES];
static unsigned int count;
static struct mutex mutex;

static char lib_str[MAX_LEN] = "";
static struct kparam_string kps = {
	.string			= lib_str,
	.maxlen			= MAX_LEN,
};
static int set_name(const char *str, struct kernel_param *kp);
module_param_call(lib_name, set_name, param_get_string, &kps, S_IWUSR);

bool use_app_setting = true;
module_param(use_app_setting, bool, 0644);
MODULE_PARM_DESC(use_app_setting, "control use of app specific settings");

bool use_32bit_app_setting = true;
module_param(use_32bit_app_setting, bool, 0644);
MODULE_PARM_DESC(use_32bit_app_setting, "control use of 32 bit app specific settings");

static int set_name(const char *str, struct kernel_param *kp)
{
	int len = strlen(str);
	char *name;

	if (len >= MAX_LEN) {
		pr_err("app_setting: name string too long\n");
		return -ENOSPC;
	}

	/*
	 * echo adds '\n' which we need to chop off later
	 */
	name = kzalloc(len + 1, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	strlcpy(name, str, len + 1);

	if (name[len - 1] == '\n')
		name[len - 1] = '\0';

	mutex_lock(&mutex);
	if (count < MAX_ENTRIES) {
		lib_names[count] = name;
		/*
		 * mb to ensure that the new lib_names entry is present
		 * before updating the view presented by get_lib_names
		 */
		mb();
		count++;
	} else {
		pr_err("app_setting: set name failed. Max entries reached\n");
		kfree(name);
		mutex_unlock(&mutex);
		return -EPERM;
	}
	mutex_unlock(&mutex);

	return 0;
}

void switch_app_setting_bit(struct task_struct *prev, struct task_struct *next)
{
	if (prev->mm && unlikely(prev->mm->app_setting))
		clear_app_setting_bit(APP_SETTING_BIT);

	if (next->mm && unlikely(next->mm->app_setting))
		set_app_setting_bit(APP_SETTING_BIT);
}
EXPORT_SYMBOL(switch_app_setting_bit);

void switch_32bit_app_setting_bit(struct task_struct *prev,
					struct task_struct *next)
{
	if (prev->mm && unlikely(is_compat_thread(task_thread_info(prev))))
		clear_app_setting_bit_for_32bit_apps();

	if (next->mm && unlikely(is_compat_thread(task_thread_info(next))))
		set_app_setting_bit_for_32bit_apps();
}
EXPORT_SYMBOL(switch_32bit_app_setting_bit);

void apply_app_setting_bit(struct file *file)
{
	bool found = false;
	int i;

	if (file && file->f_path.dentry) {
		const char *name = file->f_path.dentry->d_name.name;

		for (i = 0; i < count; i++) {
			if (unlikely(!strcmp(name, lib_names[i]))) {
				found = true;
				break;
			}
		}
		if (found) {
			preempt_disable();
			set_app_setting_bit(APP_SETTING_BIT);
			/* This will take care of child processes as well */
			current->mm->app_setting = 1;
			preempt_enable();
		}
	}
}
EXPORT_SYMBOL(apply_app_setting_bit);

static int __init app_setting_init(void)
{
	mutex_init(&mutex);
	return 0;
}
module_init(app_setting_init);
