// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Yuan Jung Kuo <yuan-jung.kuo@mediatek.com>
 */

#include <linux/io.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/notifier.h>
#include <linux/timekeeping.h>
#include <linux/remoteproc/mtk_ccu.h>
#include <mt-plat/ssc.h>  /* SRAMRC header */

#define ISPDVFS_DBG
#ifdef ISPDVFS_DBG
#define ISP_LOGD(fmt, args...) \
	do { \
		if (mtk_ispdvfs_dbg_level) \
			pr_notice("[ISPDVFS] %s(): " fmt "\n",\
				__func__, ##args); \
	} while (0)
#else
#define ISPDVFS_DBG(fmt, args...)
#endif
#define ISP_LOGI(fmt, args...) \
	pr_notice("[ISPDVFS] %s(): " fmt "\n", \
		__func__, ##args)
#define ISP_LOGE(fmt, args...) \
	pr_notice("[ISPDVFS] error %s(),%d: " fmt "\n", \
		__func__, __LINE__, ##args)
#define ISP_LOGF(fmt, args...) \
	pr_notice("[ISPDVFS] fatal %s(),%d: " fmt "\n", \
		__func__, __LINE__, ##args)

struct ccu_handle {
	phandle handle;
	struct platform_device *ccu_pdev;
};
struct ccu_handle gHandle;

static int vmm_dbg_event(struct notifier_block *notifier, unsigned long event,
				void *data)
{
	struct ccu_handle *pHandle = &gHandle;
	unsigned int reg28 = 28, reg28_val = 0;
	unsigned int reg29 = 29, reg29_val = 0;

	if (event == SSC_TIMEOUT) {
		mtk_ccu_rproc_get_inforeg(pHandle->ccu_pdev, reg28, &reg28_val);
		mtk_ccu_rproc_get_inforeg(pHandle->ccu_pdev, reg29, &reg29_val);
		ISP_LOGI("SRAMRC timeout reg28(0x%x), reg29(0x%x)\n",
				reg28_val, reg29_val);
	}

	return NOTIFY_OK;
}

static struct notifier_block vmm_dbg_notifier = {
	.notifier_call = vmm_dbg_event,
	.priority = 0,
};

static int vmm_dbg_probe(struct platform_device *pdev)
{
	int ret = 0;
	phandle handle;
	struct device_node *node = NULL, *rproc_np = NULL;
	struct ccu_handle *pHandle = &gHandle;

	node = of_find_compatible_node(NULL, NULL, "mediatek,ispdvfs");
	if (node == NULL) {
		ISP_LOGE("of_find mediatek,ispdvfs fail\n");
		ret = PTR_ERR(node);
		goto error_handle;
	}

	ret = of_property_read_u32(node, "mediatek,ccu_rproc", &handle);
	if (ret < 0) {
		ISP_LOGE("get CCU phandle fail\n");
		goto error_handle;
	}

	rproc_np = of_find_node_by_phandle(handle);
	if (rproc_np) {
		pHandle->ccu_pdev = of_find_device_by_node(rproc_np);
		if (pHandle->ccu_pdev == NULL) {
			ISP_LOGF("find ccu rproc pdev fail\n");
			ret = PTR_ERR(pHandle->ccu_pdev);
			goto error_handle;
		}
	}


	ssc_vlogic_bound_register_notifier(&vmm_dbg_notifier);
	return ret;

error_handle:
	pHandle->ccu_pdev = NULL;
	WARN_ON(ret);
	return ret;
}

static const struct of_device_id of_vmm_dbg_match_tbl[] = {
	{
		.compatible = "mediatek,vmm_dbg",
	},
	{}
};

static struct platform_driver drv_vmm_dbg = {
	.probe = vmm_dbg_probe,
	.driver = {
		.name = "mtk-vmm-dbg",
		.of_match_table = of_vmm_dbg_match_tbl,
	},
};

static int __init mtk_vmm_dbg_init(void)
{
	s32 status;

	status = platform_driver_register(&drv_vmm_dbg);
	if (status) {
		pr_notice("Failed to register VMM dbg driver(%d)\n", status);
		return -ENODEV;
	}
	return 0;
}

static void __exit mtk_vmm_dbg_exit(void)
{
	platform_driver_unregister(&drv_vmm_dbg);
}

module_init(mtk_vmm_dbg_init);
module_exit(mtk_vmm_dbg_exit);
MODULE_DESCRIPTION("MTK VMM debug driver");
MODULE_AUTHOR("Yuan Jung Kuo <yuan-jung.kuo@mediatek.com>");
MODULE_LICENSE("GPL v2");
