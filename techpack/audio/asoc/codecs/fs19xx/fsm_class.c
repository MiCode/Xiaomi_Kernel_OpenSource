/**
 * Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
 * 2020-01-20 File created.
 */

#if defined(CONFIG_FSM_SYSFS)
#include "fsm_public.h"
//#include "fsm_mtk_ipi.h"
#include "fsm_q6afe.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/version.h>


static int g_fsm_class_inited = 0;


static ssize_t fsm_re25_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	int payload[FSM_CALIB_PAYLOAD_SIZE];
	struct preset_file *pfile;
	struct fsm_afe afe;
	int size;
	int ret;

	fsm_set_calib_mode();
	fsm_delay_ms(2500);
	afe.module_id = AFE_MODULE_ID_FSADSP_RX;
	afe.port_id = fsm_afe_get_rx_port();
	afe.param_id  = CAPI_V2_PARAM_FSADSP_CALIB;
	afe.op_set = false;
	ret = fsm_afe_send_apr(&afe, payload, sizeof(payload));
	if (ret) {
		pr_err("send apr failed:%d", ret);
		return ret;
	}
	pfile = fsm_get_presets();
	if (!pfile) {
		pr_debug("not found firmware");
		return -EINVAL;
	}
	if (pfile->hdr.ndev > 1) {
		// left, right
		size = scnprintf(buf, PAGE_SIZE, "%d,%d", payload[0], payload[6]);
	} else {
		// mono
		size = scnprintf(buf, PAGE_SIZE, "%d", payload[0]);
	}

	return size;
}

static ssize_t fsm_f0_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	int payload[FSM_CALIB_PAYLOAD_SIZE];
	struct preset_file *pfile;
	struct fsm_afe afe;
	int size;
	int ret;

	// fsm_set_calib_mode();
	// fsm_delay_ms(5000);
	afe.module_id = AFE_MODULE_ID_FSADSP_RX;
	afe.port_id = fsm_afe_get_rx_port();
	afe.param_id  = CAPI_V2_PARAM_FSADSP_CALIB;
	afe.op_set = false;
	ret = fsm_afe_send_apr(&afe, payload, sizeof(payload));
	if (ret) {
		pr_err("send apr failed:%d", ret);
		return ret;
	}
	pfile = fsm_get_presets();
	if (!pfile) {
		pr_debug("not found firmware");
		return -EINVAL;
	}
	if (pfile->hdr.ndev > 1) {
		// left, right
		size = scnprintf(buf, PAGE_SIZE, "%d,%d", payload[3], payload[9]);
	} else {
		// mono
		size = scnprintf(buf, PAGE_SIZE, "%d", payload[3]);
	}

	return size;
}

static ssize_t fsm_info_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	fsm_version_t version;
	struct preset_file *pfile;
	int dev_count;
	int len = 0;

	fsm_get_version(&version);
	len  = scnprintf(buf + len, PAGE_SIZE, "version: %s\n",
			version.code_version);
	len += scnprintf(buf + len, PAGE_SIZE, "branch : %s\n",
			version.git_branch);
	len += scnprintf(buf + len, PAGE_SIZE, "commit : %s\n",
			version.git_commit);
	len += scnprintf(buf + len, PAGE_SIZE, "date   : %s\n",
			version.code_date);
	pfile = fsm_get_presets();
	dev_count = (pfile ? pfile->hdr.ndev : 0);
	len += scnprintf(buf + len, PAGE_SIZE, "device : [%d, %d]\n",
			dev_count, fsm_dev_count());

	return len;
}

static ssize_t fsm_debug_store(struct class *class,
				struct class_attribute *attr, const char *buf, size_t len)
{
	fsm_config_t *cfg = fsm_get_config();
	int value = simple_strtoul(buf, NULL, 0);

	if (cfg) {
		cfg->i2c_debug = !!value;
	}
	pr_info("i2c debug: %s", (cfg->i2c_debug ? "ON" : "OFF"));

	return len;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
static struct class_attribute g_fsm_class_attrs[] = {
	__ATTR(fsm_re25, S_IRUGO, fsm_re25_show, NULL),
	__ATTR(fsm_f0, S_IRUGO, fsm_f0_show, NULL),
	__ATTR(fsm_info, S_IRUGO, fsm_info_show, NULL),
	__ATTR(fsm_debug, S_IWUSR, NULL, fsm_debug_store),
	__ATTR_NULL
};

static struct class g_fsm_class = {
	.name = FSM_DRV_NAME,
	.class_attrs = g_fsm_class_attrs,
};

#else
static CLASS_ATTR_RO(fsm_re25);
static CLASS_ATTR_RO(fsm_f0);
static CLASS_ATTR_RO(fsm_info);
static CLASS_ATTR_WO(fsm_debug);

static struct attribute *fsm_class_attrs[] = {
	&class_attr_fsm_re25.attr,
	&class_attr_fsm_f0.attr,
	&class_attr_fsm_info.attr,
	&class_attr_fsm_debug.attr,
	NULL,
};
ATTRIBUTE_GROUPS(fsm_class);

/** Device model classes */
struct class g_fsm_class = {
	.name = FSM_DRV_NAME,
	.class_groups = fsm_class_groups,
};
#endif

int fsm_sysfs_init(struct device *dev)
{
	int ret;

	if (g_fsm_class_inited) {
		return MODULE_INITED;
	}
	// path: sys/class/$(FSM_DRV_NAME)
	ret = class_register(&g_fsm_class);
	if (!ret) {
		g_fsm_class_inited = 1;
	}

	return ret;
}

void fsm_sysfs_deinit(struct device *dev)
{
	class_unregister(&g_fsm_class);
	g_fsm_class_inited = 0;
}
#endif
