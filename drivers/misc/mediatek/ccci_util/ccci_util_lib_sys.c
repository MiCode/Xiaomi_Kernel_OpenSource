// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

//#include <mt-plat/sync_write.h>
#include "mt-plat/mtk_ccci_common.h"
#include <linux/slab.h>
#include <linux/kobject.h>
#include "ccci_util_log.h"
#include "ccci_util_lib_sys.h"

#define CCCI_KOBJ_NAME "ccci"

struct ccci_info {
	struct kobject kobj;
	unsigned int ccci_attr_count;
};

struct ccci_attribute {
	struct attribute attr;
	ssize_t (*show)(char *buf);
	ssize_t (*store)(const char *buf, size_t count);
};

#define CCCI_ATTR(_name, _mode, _show, _store)			\
static struct ccci_attribute ccci_attr_##_name = {		\
	.attr = {.name = __stringify(_name), .mode = _mode },	\
	.show = _show,						\
	.store = _store,					\
}

static struct ccci_info *ccci_sys_info;

static void ccci_obj_release(struct kobject *kobj)
{
	struct ccci_info *ccci_info_temp =
		container_of(kobj, struct ccci_info, kobj);

	kfree(ccci_info_temp);
	/* as ccci_info_temp==ccci_sys_info */
	ccci_sys_info = NULL;
}

static ssize_t ccci_attr_show(struct kobject *kobj,
	struct attribute *attr, char *buf)
{
	ssize_t len = 0;

	struct ccci_attribute *a =
		container_of(attr, struct ccci_attribute, attr);

	if (a->show)
		len = a->show(buf);

	return len;
}

static ssize_t ccci_attr_store(struct kobject *kobj,
	struct attribute *attr, const char *buf, size_t count)
{
	ssize_t len = 0;

	struct ccci_attribute *a =
		container_of(attr, struct ccci_attribute, attr);

	if (a->store)
		len = a->store(buf, count);

	return len;
}

static const struct sysfs_ops ccci_sysfs_ops = {
	.show = ccci_attr_show,
	.store = ccci_attr_store
};

/*======================================= */
/* CCCI common sys attribute part */
/*======================================= */
/* Sys -- boot status */
static get_status_func_t get_status_func;
static boot_md_func_t boot_md_func;
static int get_md_status(char val[], int size)
{
	int ret = 0;

	if (get_status_func != NULL)
		(get_status_func) (val, size);
	else {
		ret = snprintf(val, 32, "md1:n/a");
		if (ret < 0 || ret >= 32) {
			CCCI_UTIL_INF_MSG("%s-%d:snprintf fail,ret=%d\n",
				__func__, __LINE__, ret);
			return -1;
		}
	}
	return 0;
}

static int trigger_md_boot(void)
{
	if (boot_md_func != NULL)
		return (boot_md_func) ();

	return -1;
}

static ssize_t boot_status_show(char *buf)
{
	char md1_sta_str[32];

	get_md_status(md1_sta_str, 32);

	/* Final string */
	return snprintf(buf, 32 + 1, "%s\n", md1_sta_str);
}

static ssize_t boot_status_store(const char *buf, size_t count)
{
	CCCI_UTIL_INF_MSG("md get boot store\n");

	if (trigger_md_boot() != 0)
		CCCI_UTIL_INF_MSG("md n/a\n");

	return count;
}

CCCI_ATTR(boot, 0660, &boot_status_show, &boot_status_store);

/* Sys -- enable status */
static ssize_t ccci_md_enable_show(char *buf)
{
	char md_en;

	if (get_modem_is_enabled())
		md_en = 'E';
	else
		md_en = 'D';

	/* Final string */
	return snprintf(buf, 32, "md_en=%c\n", md_en);
}

CCCI_ATTR(md_en, 0444, &ccci_md_enable_show, NULL);

/* Sys -- post fix */
static ssize_t ccci_md1_post_fix_show(char *buf)
{
	get_md_postfix(NULL, buf, NULL);
	return strlen(buf);
}

CCCI_ATTR(md1_postfix, 0444, &ccci_md1_post_fix_show, NULL);

/* Sys -- dump buff usage */
static ssize_t ccci_dump_buff_usage_show(char *buf)
{
	return get_dump_buf_usage(buf, 4095);
}

CCCI_ATTR(dump_max, 0660, &ccci_dump_buff_usage_show, NULL);

/* Sys -- ccci stage change(chn) log  */
static ssize_t ccci_dump_event_show(char *buf)
{
	return (ssize_t)ccci_event_log_cpy(buf, 4095);
}

CCCI_ATTR(md_chn, 0444, &ccci_dump_event_show, NULL);

/* Sys -- Versin */
static unsigned int ccci_port_ver = 6; /* ECCCI_FSM */
static ssize_t ccci_version_show(char *buf)
{
	return snprintf(buf, 16, "%d\n", ccci_port_ver);
}

void update_ccci_port_ver(unsigned int new_ver)
{
	ccci_port_ver = new_ver;
}

CCCI_ATTR(version, 0644, &ccci_version_show, NULL);

static ssize_t debug_enable_show(char *buf)
{
	int curr = 0;

	curr = snprintf(buf, 16, "%d\n", 2);/* ccci_debug_enable); */
	if (curr < 0 || curr >= 16) {
		CCCI_UTIL_INF_MSG(
			"%s-%d:snprintf fail,curr=%d\n", __func__, __LINE__, curr);
		return -1;
	}
	return curr;
}

static ssize_t debug_enable_store(const char *buf, size_t count)
{
	/* ccci_debug_enable = buf[0] - '0'; */
	return count;
}

CCCI_ATTR(debug, 0660, &debug_enable_show, &debug_enable_store);
/* Sys -- dump lk load md info */
static ssize_t ccci_lk_load_md_show(char *buf)
{
	return get_lk_load_md_info(buf, 4095);
}

CCCI_ATTR(lk_md, 0444, &ccci_lk_load_md_show, NULL);

/* Sys -- get ccci private feature info */
/* If platform has special feature setting,
 * platform code will implemet this function
 */
int __attribute__((weak)) ccci_get_plat_ft_inf(char buf[], int size)
{
	return (ssize_t)snprintf(buf, size, "ft_inf_ver:1");
}

static ssize_t ccci_ft_inf_show(char *buf)
{
	if (ccci_get_plat_ft_inf) {
		CCCI_UTIL_INF_MSG("using platform setting\n");
		return (ssize_t)ccci_get_plat_ft_inf(buf, 4095);
	}
	/* Enter here means using default setting */
	return (ssize_t)ccci_get_plat_ft_inf(buf, 4095);
}

CCCI_ATTR(ft_info, 0444, &ccci_ft_inf_show, NULL);

static ssize_t kcfg_setting_show(char *buf)
{
	unsigned int curr = 0;
	unsigned int actual_write;
	char md_en;
	char c_en;

	if (get_modem_is_enabled())
		md_en = '1';
	else
		md_en = '0';

	/* MD enable setting part */
	/* Reserve 16 byte to store size info */
	actual_write = snprintf(&buf[curr],
					4096 - 16 - curr,
					"[modem num]:1\n");
	if (actual_write < 0) {
		CCCI_UTIL_ERR_MSG(
			"%s-%d:snprintf fail,actual_write=%d\n",
			__func__, __LINE__, actual_write);
		actual_write = 0;
	} else if (actual_write >= 4096 - 16 - curr)
		actual_write = 4096 - 16 - curr - 1;
	curr += actual_write;
	/* Reserve 16 byte to store size info */
	actual_write = snprintf(&buf[curr], 4096 - 16 - curr,
		"[modem en]:%c\n", md_en);
	if (actual_write < 0) {
		CCCI_UTIL_ERR_MSG(
			"%s-%d:snprintf fail,actual_write=%d\n",
			__func__, __LINE__, actual_write);
		actual_write = 0;
	} else if (actual_write >= 4096 - 16 - curr)
		actual_write = 4096 - 16 - curr - 1;
	curr += actual_write;

	/* ECCCI_C2K */
	if (check_rat_at_md_img("C"))
		c_en = '1';
	else
		c_en = '0';
	actual_write = snprintf(&buf[curr],
			4096 - curr, "[MTK_ECCCI_C2K]:%c\n", c_en);
	if (actual_write < 0) {
		CCCI_UTIL_ERR_MSG(
			"%s-%d:snprintf fail,actual_write=%d\n",
			__func__, __LINE__, actual_write);
		actual_write = 0;
	} else if (actual_write >= 4096 - curr)
		actual_write = 4096 - curr - 1;
	curr += actual_write;

	/* ECCCI_FSM */
	if (ccci_port_ver == 6)
		/* FSM using v2 */
		actual_write = snprintf(&buf[curr], 4096 - curr,
			"[ccci_drv_ver]:V2\n");
	else
		actual_write = snprintf(&buf[curr], 4096 - curr,
			"[ccci_drv_ver]:V1\n");
	if (actual_write < 0) {
		CCCI_UTIL_ERR_MSG(
			"%s-%d:snprintf fail,actual_write=%d\n",
			__func__, __LINE__, actual_write);
		actual_write = 0;
	} else if (actual_write >= 4096 - curr)
		actual_write = 4096 - curr - 1;
	curr += actual_write;

	actual_write = snprintf(&buf[curr],
		4096 - curr, "[MTK_MD_CAP]:%d\n", get_md_img_type());
	if (actual_write > 0 && actual_write < (4096 - curr))
		curr += actual_write;

	/* Add total size to tail */
	actual_write = snprintf(&buf[curr],
		4096 - curr, "total:%d\n", curr);
	if (actual_write < 0) {
		CCCI_UTIL_ERR_MSG(
			"%s-%d:snprintf fail,actual_write=%d\n",
			__func__, __LINE__, actual_write);
		actual_write = 0;
	} else if (actual_write >= 4096 - curr)
		actual_write = 4096 - curr - 1;
	curr += actual_write;

	CCCI_UTIL_INF_MSG("cfg_info_buffer size:%d\n",
		curr);
	return (ssize_t) curr;
}

static ssize_t kcfg_setting_store(const char *buf, size_t count)
{
	return count;
}

CCCI_ATTR(kcfg_setting, 0444, &kcfg_setting_show, &kcfg_setting_store);

static ssize_t ccci_pin_cfg_store(const char *buf, size_t count)
{
	unsigned int pin_val;

	pin_val = buf[0] - '0';
	inject_pin_status_event(pin_val, "RF_cable");
	return count;
}

CCCI_ATTR(pincfg, 0220, NULL, &ccci_pin_cfg_store);
/* Sys -- Add to group */
static struct attribute *ccci_default_attrs[] = {
	&ccci_attr_boot.attr,
	&ccci_attr_version.attr,
	&ccci_attr_md_en.attr,
	&ccci_attr_debug.attr,
	&ccci_attr_kcfg_setting.attr,
	&ccci_attr_dump_max.attr,
	&ccci_attr_lk_md.attr,
	&ccci_attr_md_chn.attr,
	&ccci_attr_ft_info.attr,
	&ccci_attr_md1_postfix.attr,
	&ccci_attr_pincfg.attr,
	NULL
};

static struct kobj_type ccci_ktype = {
	.release = ccci_obj_release,
	.sysfs_ops = &ccci_sysfs_ops,
	.default_attrs = ccci_default_attrs
};

int ccci_sysfs_add_modem(void *kobj, void *ktype,
	get_status_func_t get_sta_func, boot_md_func_t boot_func)
{
	int ret;
	static int md_add_flag;

	md_add_flag = 0;
	if (!ccci_sys_info) {
		CCCI_UTIL_ERR_MSG("common sys not ready\n");
		return -CCCI_ERR_SYSFS_NOT_READY;
	}

	if (md_add_flag) {
		CCCI_UTIL_ERR_MSG("md sys dup add\n");
		return -CCCI_ERR_SYSFS_NOT_READY;
	}

	ret =
	    kobject_init_and_add((struct kobject *)kobj,
		(struct kobj_type *)ktype, &ccci_sys_info->kobj, "mdsys");
	if (ret < 0) {
		kobject_put(kobj);
		CCCI_UTIL_ERR_MSG("fail to add md kobject\n");
	} else {
		md_add_flag |= (1 << 0);
		get_status_func = get_sta_func;
		boot_md_func = boot_func;
	}

	return ret;
}
EXPORT_SYMBOL(ccci_sysfs_add_modem);

int ccci_common_sysfs_init(void)
{
	int ret = 0;

	ccci_sys_info = kmalloc(sizeof(struct ccci_info), GFP_KERNEL);
	if (!ccci_sys_info)
		return -ENOMEM;

	memset(ccci_sys_info, 0, sizeof(struct ccci_info));

	ret = kobject_init_and_add(&ccci_sys_info->kobj,
			&ccci_ktype, kernel_kobj, CCCI_KOBJ_NAME);
	if (ret < 0) {
		kobject_put(&ccci_sys_info->kobj);
		CCCI_UTIL_ERR_MSG("fail to add ccci kobject\n");
		return ret;
	}

	get_status_func = NULL;
	boot_md_func = NULL;

	ccci_sys_info->ccci_attr_count = ARRAY_SIZE(ccci_default_attrs) - 1;
	CCCI_UTIL_DBG_MSG("ccci attr cnt %d\n", ccci_sys_info->ccci_attr_count);
	return ret;
}
