// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <soc/mediatek/mmqos.h>
#include "mtk_ccci_common.h"

#define MMQOS_MD_REQ_RETRY_MAX (20)
static int md_type = -1;

static int mmqos_req_md_type(void *data)
{
	u32 retry_cnt = 0;
	int ret = 0;

	while (retry_cnt < MMQOS_MD_REQ_RETRY_MAX) {
#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
		ret = exec_ccci_kern_func_by_md_id(
			MD_SYS1, MD_NR_BAND_ACTIVATE_INFO, NULL, 0);
#endif
		if (ret == 0 && md_type != -1)
			break;
		pr_notice("%s ccci not ready ret=%d md_type=%d\n", __func__, ret, md_type);
		retry_cnt++;
		msleep(5000);
	}
	if (retry_cnt < MMQOS_MD_REQ_RETRY_MAX)
		mtk_mmqos_set_md_type(md_type);
	pr_notice("%s retry_cnt=%u md_type=%d\n", __func__, retry_cnt, md_type);
	return 0;
}

int mmqos_md_band_req_hdlr(int md_id, int data)
{
	if (data < 0) {
		pr_notice("%s wrong data(%d)\n", __func__, data);
		return 0;
	}
	md_type = data;
	pr_notice("%s data(%d)\n", __func__, data);
	return 0;
}

int mtk_mmqos_md_probe(struct platform_device *pdev)
{
	struct task_struct *pKThread;
	int ret = 0;

#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
	ret = register_ccci_sys_call_back(MD_SYS1,
		MD_NR_BAND_ACTIVATE_INFO, mmqos_md_band_req_hdlr);
	if (ret) {
		pr_notice("%s: fail to register ccci sys callback(%d)\n", __func__, ret);
		return ret;
	}
#endif

	pKThread = kthread_run(mmqos_req_md_type,
		NULL, "request md type");
	pr_notice("%s done\n", __func__);
	return ret;
}

static const struct of_device_id mtk_mmqos_md_of_ids[] = {
	{
		.compatible = "mediatek,mmqos-md",
	},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_mmqos_md_of_ids);

static struct platform_driver mtk_mmqos_md_driver = {
	.probe = mtk_mmqos_md_probe,
	.driver = {
		.name = "mtk-mmqos-md",
		.of_match_table = mtk_mmqos_md_of_ids,
	},
};
module_platform_driver(mtk_mmqos_md_driver);

MODULE_LICENSE("GPL v2");
