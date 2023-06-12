/**
 * Copyright (c) 2018 HuaQin Technologies Co., Ltd. 2018-2019. All rights reserved.
 *Description: Core Defination For Foursemi Device .
 *Author: Fourier Semiconductor Inc.
 * Create: 2019-03-17 File created.
 */

#include "fsm_public.h"
#if defined(CONFIG_FSM_SYSFS)
#include "fsm_q6afe.h"
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/slab.h>

static int g_fsm_sysfs_inited = 0;

static ssize_t fsm_cali_re_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	fsm_config_t *cfg = fsm_get_config();
	struct fsadsp_cmd_re25 cmd_re25;
	uint32_t re25[FSM_DEV_MAX] = { 0 };
	int size;
	int idx;
	int ret;

	cfg->force_calib = true;
	fsm_set_calib_mode();
	fsm_delay_ms(2500);
	ret = fsm_afe_save_re25(&cmd_re25);
	cfg->force_calib = false;
	if (ret) {
		pr_err("save re25 failed");
		return ret;
	}
	fsm_delay_ms(20);
	ret = fsm_afe_read_re25(re25, cmd_re25.ndev);
	if (ret) {
		pr_err("read back re25 fail:%d", ret);
	}
	for (idx = 0; idx < cmd_re25.ndev; idx++) {
		if (re25[idx] != cmd_re25.cal_data[idx].re25) {
			pr_err("read back re25.%d[%d] not match!", idx, re25[idx]);
			return -EINVAL;
		}
		re25[idx] = re25[idx] * 1000 / 4096;
		pr_info("read back re25.%d: %d mOhms", idx, re25[idx]);
	}
	if (cmd_re25.ndev > 1) {
		// left, right
		size = scnprintf(buf, PAGE_SIZE,
			"left:%d mOhms right:%d mOhms\n",
			re25[0], re25[1]);
	} else {
		// mono
		size = scnprintf(buf, PAGE_SIZE,
			"left:%d mOhms\n", re25[0]);
	}

	return size;
}

static ssize_t fsm_cali_re_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t len)
{
	return len;
}

static ssize_t fsm_cali_f0_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int payload[FSM_CALIB_PAYLOAD_SIZE];
	struct preset_file *pfile;
	struct fsm_afe afe;
	int size;
	int ret;

	pfile = (struct preset_file *)fsm_get_presets();
	if (pfile == NULL) {
		pr_err("not found firmware");
		return -EINVAL;
	}
	//fsm_set_calib_mode();
	fsm_delay_ms(5000);
	afe.module_id = AFE_MODULE_ID_FSADSP_RX;
	afe.port_id = fsm_afe_get_rx_port();
	afe.param_id  = CAPI_V2_PARAM_FSADSP_CALIB;
	afe.op_set = false;
	ret = fsm_afe_send_apr(&afe, payload, sizeof(payload));
	if (ret) {
		pr_err("send apr failed:%d", ret);
		return ret;
	}
	if (pfile->hdr.ndev > 1) {
		// left, right
		size = scnprintf(buf, PAGE_SIZE, "%d,%d",
			payload[3] / 256, payload[9] / 256);
	} else {
		// mono
		size = scnprintf(buf, PAGE_SIZE, "%d",
			payload[3] / 256);
	}

	return size;
}

static ssize_t fsm_cali_f0_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t len)
{
	return len;
}

static DEVICE_ATTR(cali_re, S_IRUGO | S_IWUSR,
		fsm_cali_re_show, fsm_cali_re_store);

static DEVICE_ATTR(cali_f0, S_IRUGO | S_IWUSR,
		fsm_cali_f0_show, fsm_cali_f0_store);


static struct attribute *fsm_attributes[] = {
	&dev_attr_cali_re.attr,
	&dev_attr_cali_f0.attr,
	NULL
};

static const struct attribute_group fsm_attr_group = {
	.attrs = fsm_attributes,
};

int fsm_sysfs_init(struct device *dev)
{
	int ret;

	if (g_fsm_sysfs_inited) {
		return MODULE_INITED;
	}
	ret = sysfs_create_group(&dev->kobj, &fsm_attr_group);
	if (!ret) {
		g_fsm_sysfs_inited = 1;
	}

	return ret;
}

void fsm_sysfs_deinit(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &fsm_attr_group);
	g_fsm_sysfs_inited = 0;
}
#endif

