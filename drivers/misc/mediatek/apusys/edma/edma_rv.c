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

#include "edma_dbgfs.h"
#include "edma_driver.h"
#include "edma_cmd_hnd.h"
#include "apusys_power.h"
#include "apusys_core.h"
#include "edma_plat_internal.h"

u8 g_edmaRV_log_lv = EDMA_LOG_DEBUG;

#define LOG_RV_DBG(x, args...) \
	{ \
		if (g_edmaRV_log_lv >= EDMA_LOG_DEBUG) \
			pr_info(EDMA_TAG "[debug] %s/%d "\
			x, __func__, __LINE__, ##args); \
	}

#define LOG_INF(format, args...)    pr_info(EDMA_TAG " " format, ##args)
#define LOG_WRN(format, args...)    pr_info(EDMA_TAG "[warn] " format, ##args)
#define LOG_ERR(format, args...)    pr_info(EDMA_TAG "[error] " format, ##args)

/* ipi type */
enum EDMA_IPI_TYPE {
	EDMA_IPI_NONE,
	EDMA_IPI_SET_LOG_LV,
};

/*
 * type: command type
 * data : command data
 */
struct edma_ipi_data {
	uint32_t type;
	uint32_t data;
};

struct edma_rv_dev {
	struct rpmsg_device *rpdev;
	struct rpmsg_endpoint *ept;
	struct kobject *edmaRV_root;

};

static struct edma_rv_dev eRdev;


static int edma_rpmsg_cb(struct rpmsg_device *rpdev, void *data,
				 int len, void *priv, u32 src)
{
	int ret = 0;

	LOG_INF("%s len=%d, priv=%p, src=%d\n", __func__, len, priv, src);
	//ret = reviser_remote_rx_cb(data, len);

	return ret;
}


static int edma_rpmsg_probe(struct rpmsg_device *rpdev)
{
	int ret = 0;
	struct rpmsg_channel_info chinfo = {};

	LOG_INF("%s in\n", __func__);

	eRdev.rpdev = rpdev;

	strscpy(chinfo.name, eRdev.rpdev->id.name, RPMSG_NAME_SIZE);
	chinfo.src = eRdev.rpdev->src;
	chinfo.dst = RPMSG_ADDR_ANY;
	eRdev.ept = rpmsg_create_ept(eRdev.rpdev,
		edma_rpmsg_cb, &eRdev, chinfo);


	LOG_INF("Done, eRdev.ept = %p\n", eRdev.ept);

	return ret;
}

static void edma_rpmsg_remove(struct rpmsg_device *rpdev)
{
	LOG_DBG("%s in\n", __func__);
}

static const struct of_device_id edma_rpmsg_of_match[] = {
	{ .compatible = "mediatek,apu-edma-rpmsg", },
	{ },
};

static struct rpmsg_driver edma_rpmsg_driver = {
	.drv	= {
		.name	= "apu-edma-rpmsg",
		.of_match_table = edma_rpmsg_of_match,
	},
	.probe	= edma_rpmsg_probe,
	.remove	= edma_rpmsg_remove,
	.callback = edma_rpmsg_cb,
};


static ssize_t edma_rvlog_show(struct kobject *kobj,
				       struct kobj_attribute *attr,
				       char *buf)
{
	ssize_t count = 0;

	count += scnprintf(buf + count, PAGE_SIZE - count,
		"g_edmaRV_log_lv = %d\n", g_edmaRV_log_lv);

	return count;
}

static ssize_t edma_rvlog_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int val = 0;
	int ret;
	struct edma_ipi_data mData;

	ret = kstrtouint(buf, 10, &val);

	LOG_RV_DBG("set debug lv = %d\n", val);

	g_edmaRV_log_lv = val;

	mData.type = EDMA_IPI_SET_LOG_LV;
	mData.data = g_edmaRV_log_lv;

	ret = rpmsg_send(eRdev.ept, &mData, sizeof(mData));
	if (ret)
		LOG_ERR("send msg fail\n");


	return count;


}
static const struct kobj_attribute edma_log_lv_attr =
	__ATTR(edma_rv_log_lv, 0660, edma_rvlog_show,
		edma_rvlog_store);


int edma_rv_setup(struct apusys_core_info *info)
{
	int ret = 0;

	pr_info("%s in\n", __func__);

	memset(&eRdev, 0, sizeof(eRdev));

	eRdev.edmaRV_root = kobject_create_and_add("edma_rv", kernel_kobj);

	if (!eRdev.edmaRV_root)
		return -ENOMEM;

	ret = sysfs_create_file(eRdev.edmaRV_root, &edma_log_lv_attr.attr);
	if (ret)
		LOG_ERR("%s create edma_log_lv_attr attribute fail, ret %d\n", __func__, ret);



	if (register_rpmsg_driver(&edma_rpmsg_driver)) {
		LOG_ERR("failed to register RMPSG driver");
		return -ENODEV;
	}

	return ret;
}

