// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Unisoc Inc.
 */
#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/types.h>

#define MAX_CMDLINE_PARAM_LEN 128

static char powerup_reason[MAX_CMDLINE_PARAM_LEN];
/* add poweroff reason by unisoc 2025/09/17 */
static char poweroff_reason[MAX_CMDLINE_PARAM_LEN];
/******************************************************************************
 * * Add pureason
 *****************************************************************************/
static ssize_t powerup_reason_show(struct kobject *kobj,
				   struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, sizeof(powerup_reason), "%s\n", powerup_reason);
};

static ssize_t poweroff_reason_show(struct kobject *kobj,
                   struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, sizeof(poweroff_reason), "%s\n", poweroff_reason);
};

static struct kobj_attribute powerup_reason_attr = {
	.attr = { .name = __stringify(powerup_reason), .mode = 0644 },
	.show = powerup_reason_show,
};

static struct kobj_attribute poweroff_reason_attr = {
	.attr = { .name = __stringify(poweroff_reason), .mode = 0644 },
	.show = poweroff_reason_show,
};

static struct attribute *bootinfo_attrs[] = {
	&powerup_reason_attr.attr,
	&poweroff_reason_attr.attr,
	NULL,
};

static struct attribute_group bootinfo_attr_group = {
	.attrs = bootinfo_attrs,
};

static struct kobject *bootinfo_kobj;

static const char * const bootcause_types[] = {
	"Pbint triggered",
	"RTC poweroff alarm expiry",
	"Reboot due to sysdump finish",
	NULL,
};

static const char *const bootcause_cvt_types[] = {
	"keypad",
	"rtc",
	"kpanic",
	NULL,
};

static int get_bootcause(char *cause_str)
{
	struct device_node *cmdline_node;
	char *p = NULL;
	const char *p_cmd_line;
	int  i = 0;
	int ret;

	cmdline_node = of_find_node_by_path("/chosen");
	if (!cmdline_node) {
		pr_err("%s: no chosen node\n", __func__);
		return -EINVAL;
	}
	ret = of_property_read_string(cmdline_node, "bootargs", &p_cmd_line);
	if (ret) {
		pr_err("%s: can not read bootargs\n", __func__);
		return ret;
	}

	if (!p_cmd_line)
		return -EINVAL;
	p = strstr(p_cmd_line, "bootcause=");
	if (!p) {
		pr_err("%s: no bootcause field\n", __func__);
		return -EINVAL;
	}

	p += strlen("bootcause=");
	if (*p == '\"')
		p++;
	while (true) {
		cause_str[i++] = *p++;
		if (*p == '\"')
			break;
	}

	return 0;
}

static int get_poweroffcause(char *cause_str)
{
	struct device_node *cmdline_node;
	char *p = NULL;
	const char *p_cmd_line;
	int  i = 0;
	int ret;

	cmdline_node = of_find_node_by_path("/chosen");
	if (!cmdline_node) {
		pr_err("%s: no chosen node\n", __func__);
		return -EINVAL;
	}
	ret = of_property_read_string(cmdline_node, "bootargs", &p_cmd_line);
	if (ret) {
		pr_err("%s: can not read bootargs\n", __func__);
		return ret;
	}

	if (!p_cmd_line)
		return -EINVAL;
	p = strstr(p_cmd_line, "pwroffcause=");
	if (!p) {
		pr_err("%s: no pwroffcause field\n", __func__);
		return -EINVAL;
	}

	p += strlen("pwroffcause=");
	if (*p == '\"')
		p++;
	while (true) {
		cause_str[i++] = *p++;
		if (*p == '\"')
		break;
	}

	return 0;
}

static int pur_init(void)
{
	int ret = -ENOMEM;
	int i = 0;
	char reason[MAX_CMDLINE_PARAM_LEN] = {0};

	bootinfo_kobj = kobject_create_and_add("bootinfo", NULL);
	if (!bootinfo_kobj) {
		pr_err("%s: set powerup reason failed\n", __func__);
		return ret;
	}

	ret = sysfs_create_group(bootinfo_kobj, &bootinfo_attr_group);
	if (ret) {
		pr_err("%s: failed to create attribute group.\n", __func__);
		kobject_put(bootinfo_kobj);
		return ret;
	}

	/* get power up reason */
	if (get_bootcause(reason) != 0)
		pr_err("%s: no bootcause type field, default reboot\n", __func__);

	while (true) {
		if (bootcause_types[i] == NULL) {
			strscpy(powerup_reason, "reboot", sizeof(powerup_reason));
			break;
		} else if (strcmp(bootcause_types[i], reason) == 0) {
			strscpy(powerup_reason, bootcause_cvt_types[i],
				sizeof(powerup_reason));
			break;
		} else if (i >= sizeof(bootcause_types) / sizeof(char *)) {
			strscpy(powerup_reason, "reboot", sizeof(powerup_reason));
			break;
		}
		i++;
	}
	pr_info("%s: bootcause_types=%s, powerup_reason=%s\n", __func__,
		bootcause_types[i], powerup_reason);

    /* get power off reason */
	memset(reason, 0, MAX_CMDLINE_PARAM_LEN);
	if (get_poweroffcause(reason) != 0)
		pr_err("%s: no poweroff cause type field\n", __func__);

	strscpy(poweroff_reason, reason, sizeof(poweroff_reason));
	pr_info("%s: poweroff_reason=%s\n", __func__, poweroff_reason);

	return ret;
}

void pur_exit(void)
{
	sysfs_remove_group(bootinfo_kobj, &bootinfo_attr_group);
	kobject_put(bootinfo_kobj);
}

module_init(pur_init);
module_exit(pur_exit);
module_param_string(pureason, powerup_reason, MAX_CMDLINE_PARAM_LEN, 0644);
MODULE_LICENSE("GPL");
