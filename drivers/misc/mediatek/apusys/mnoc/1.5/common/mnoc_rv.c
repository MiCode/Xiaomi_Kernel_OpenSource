// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/platform_device.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/interrupt.h>
#include <linux/syscalls.h>
#include <linux/cdev.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/rpmsg.h>

#include "apusys_power.h"
#include "apusys_core.h"

enum {
	MNOC_LOG_WARN,
	MNOC_LOG_INFO,
	MNOC_LOG_DEBUG,
};

u8 g_mnocRV_log_lv = MNOC_LOG_DEBUG;

#define MNOC_TAG "[mnoc]"

#define LOG_RV_DBG(x, args...) \
	{ \
		if (g_mnocRV_log_lv >= MNOC_LOG_DEBUG) \
			pr_info(MNOC_TAG "[debug] %s/%d "\
			x, __func__, __LINE__, ##args); \
	}

#define LOG_INF(format, args...)    pr_info(MNOC_TAG " " format, ##args)
#define LOG_WRN(format, args...)    pr_info(MNOC_TAG "[warn] " format, ##args)
#define LOG_ERR(format, args...)    pr_info(MNOC_TAG "[error] " format, ##args)

/* ipi type */
enum MNOC_IPI_TYPE {
	MNOC_IPI_NONE,
	MNOC_IPI_SET_LOG_LV,
	MNOC_IPI_UPDATE_TIMER,
};

/*
 * type: command type
 * data : command data
 */
struct mnoc_ipi_data {
	uint32_t type;
	uint32_t data;
};

struct mnoc_rv_dev {
	struct rpmsg_device *rpdev;
	struct rpmsg_endpoint *ept;
	struct kobject *mnocRV_root;

};

static struct mnoc_rv_dev eRdev;


static int mnoc_rpmsg_cb(struct rpmsg_device *rpdev, void *data,
				 int len, void *priv, u32 src)
{
	int ret = 0;

	LOG_INF("%s len=%d, priv=%p, src=%d\n", __func__, len, priv, src);
	//ret = reviser_remote_rx_cb(data, len);

	return ret;
}


static int mnoc_rpmsg_probe(struct rpmsg_device *rpdev)
{
	int ret = 0;
	struct rpmsg_channel_info chinfo = {};

	LOG_INF("%s in\n", __func__);

	eRdev.rpdev = rpdev;

	strscpy(chinfo.name, eRdev.rpdev->id.name, RPMSG_NAME_SIZE);
	chinfo.src = eRdev.rpdev->src;
	chinfo.dst = RPMSG_ADDR_ANY;
	eRdev.ept = rpmsg_create_ept(eRdev.rpdev,
		mnoc_rpmsg_cb, &eRdev, chinfo);


	// LOG_INF("Done, eRdev.ept = 0x%x\n", eRdev.ept);

	return ret;
}

static void mnoc_rpmsg_remove(struct rpmsg_device *rpdev)
{
	LOG_RV_DBG("%s in\n", __func__);
}

static const struct of_device_id mnoc_rpmsg_of_match[] = {
	{ .compatible = "mediatek,apu-mnoc-rpmsg", },
	{ },
};

static struct rpmsg_driver mnoc_rpmsg_driver = {
	.drv	= {
		.name	= "apu-mnoc-rpmsg",
		.of_match_table = mnoc_rpmsg_of_match,
	},
	.probe	= mnoc_rpmsg_probe,
	.remove	= mnoc_rpmsg_remove,
	.callback = mnoc_rpmsg_cb,
};


static ssize_t mnoc_rvlog_show(struct kobject *kobj,
				       struct kobj_attribute *attr,
				       char *buf)
{
	ssize_t count = 0;

	count += scnprintf(buf + count, PAGE_SIZE - count,
		"g_mnocRV_log_lv = %d\n", g_mnocRV_log_lv);

	return count;
}

static ssize_t mnoc_rvlog_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int val = 0;
	int ret;
	struct mnoc_ipi_data mData;

	ret = kstrtouint(buf, 10, &val);

	LOG_RV_DBG("set debug lv = %d\n", val);

	g_mnocRV_log_lv = val;

	mData.type = MNOC_IPI_SET_LOG_LV;
	mData.data = g_mnocRV_log_lv;

	ret = rpmsg_send(eRdev.ept, &mData, sizeof(mData));
	if (ret)
		LOG_ERR("send msg fail\n");


	return count;


}
static const struct kobj_attribute mnoc_log_lv_attr =
	__ATTR(mnoc_rv_log_lv, 0660, mnoc_rvlog_show,
		mnoc_rvlog_store);


int mnoc_rv_setup(struct apusys_core_info *info)
{
	int ret = 0;

	pr_info("%s in\n", __func__);

	memset(&eRdev, 0, sizeof(eRdev));

	eRdev.mnocRV_root = kobject_create_and_add("mnoc_rv", kernel_kobj);

	if (!eRdev.mnocRV_root)
		return -ENOMEM;

	ret = sysfs_create_file(eRdev.mnocRV_root, &mnoc_log_lv_attr.attr);
	if (ret)
		LOG_ERR("%s create mnoc_log_lv_attr attribute fail, ret %d\n", __func__, ret);



	if (register_rpmsg_driver(&mnoc_rpmsg_driver)) {
		LOG_ERR("failed to register RMPSG driver");
		return -ENODEV;
	}

	return ret;
}

