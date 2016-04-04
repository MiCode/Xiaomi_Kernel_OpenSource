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

void get_lib_names(char *names[], unsigned int *cnt)
{
	int i;

	/*
	 * set_name is expected to be called before we
	 * access lib_names and count variable here for
	 * specific processes.
	 */
	*cnt = count;
	for (i = 0; i < count; i++)
		names[i] = lib_names[i];
}
EXPORT_SYMBOL(get_lib_names);

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
		lib_names[count++] = name;
	} else {
		pr_err("app_setting: set name failed. Max entries reached\n");
		mutex_unlock(&mutex);
		return -EPERM;
	}
	mutex_unlock(&mutex);

	return 0;
}

static int __init app_setting_init(void)
{
	mutex_init(&mutex);
	return 0;
}
module_init(app_setting_init);
