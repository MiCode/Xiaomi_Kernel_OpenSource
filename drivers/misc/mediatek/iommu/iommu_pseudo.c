// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#define pr_fmt(fmt)    "mtk_iommu: pseudo " fmt

#include <linux/of.h>
#include <linux/platform_device.h>

#if IS_ENABLED(CONFIG_MTK_ENABLE_GENIEZONE)
#include "iommu_gz_sec.h"
#include "iommu_pseudo.h"

#define M4U_L2_ENABLE	1
#define SVP_FEATURE_DT_NAME	"SecureVideoPath"

static DEFINE_MUTEX(gM4u_gz_sec_init);
static bool m4u_gz_en[SEC_ID_COUNT];
static int iommu_on_mtee = -1;

static int __m4u_gz_sec_init(int mtk_iommu_sec_id)
{
	int ret;
	struct m4u_gz_sec_context *ctx;

	ctx = m4u_gz_sec_ctx_get();
	if (!ctx)
		return -EFAULT;

	// TODO: Power on all larbs

	ctx->gz_m4u_msg->cmd = CMD_M4UTY_INIT;
	ctx->gz_m4u_msg->iommu_sec_id = mtk_iommu_sec_id;
	ctx->gz_m4u_msg->init_param.nonsec_pt_pa = 0;
	ctx->gz_m4u_msg->init_param.l2_en = M4U_L2_ENABLE;
	ctx->gz_m4u_msg->init_param.sec_pt_pa = 0;

	pr_info("[MTEE]%s: mtk_iommu_sec_id:%d\n", __func__, mtk_iommu_sec_id);
	ret = m4u_gz_exec_cmd(ctx);
	if (ret) {
		pr_err("[MTEE]m4u exec command fail\n");
		goto out;
	}

out:
	// TODO: Power off all larbs

	m4u_gz_sec_ctx_put(ctx);
	return ret;
}

static int m4u_gz_sec_init(int mtk_iommu_sec_id)
{
	int ret;

	pr_info("[MTEE]%s: start\n", __func__);

	if (m4u_gz_en[mtk_iommu_sec_id]) {
		pr_info("re-initiation, %d\n", m4u_gz_en[mtk_iommu_sec_id]);
		goto m4u_gz_sec_reinit;
	}

	m4u_gz_sec_set_context();
	ret = m4u_gz_sec_context_init();
	if (ret)
		return ret;

	m4u_gz_en[mtk_iommu_sec_id] = 1;

m4u_gz_sec_reinit:
	ret = __m4u_gz_sec_init(mtk_iommu_sec_id);
	if (ret < 0) {
		m4u_gz_en[mtk_iommu_sec_id] = 0;
		m4u_gz_sec_context_deinit();
		pr_err("[MTEE]%s:init fail,ret:0x%x\n", __func__, ret);

		return ret;
	}

	/* don't deinit ha because of multiple init operation */

	return 0;

}

int mtk_iommu_sec_init(int mtk_iommu_sec_id)
{
	int ret = 0;

	if (iommu_on_mtee != 1) {
		pr_warn("%s is not support, iommu_on_mtee:%d\n", __func__,
			iommu_on_mtee);
		return -1;
	}

	mutex_lock(&gM4u_gz_sec_init);
	ret = m4u_gz_sec_init(mtk_iommu_sec_id);
	mutex_unlock(&gM4u_gz_sec_init);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_iommu_sec_init);

bool is_iommu_sec_on_mtee(void)
{
	return (iommu_on_mtee == 1);
}
EXPORT_SYMBOL_GPL(is_iommu_sec_on_mtee);

static int mtk_iommu_pseudo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *svp_node;

	svp_node = of_find_node_by_name(NULL, SVP_FEATURE_DT_NAME);
	if (!svp_node) {
		iommu_on_mtee = 0;
		pr_info("SVP on MTEE not support, skip init iommu_pseudo\n");
		return 0;
	}

	iommu_on_mtee = 1;
	of_node_put(svp_node);

	pr_info("%s done, dev:%s\n", __func__, dev_name(dev));

	return 0;
}

#else
int mtk_iommu_sec_init(int mtk_iommu_sec_id)
{
	pr_warn("%s is not support, geniezone is disabled\n", __func__);

	return -1;
}
EXPORT_SYMBOL_GPL(mtk_iommu_sec_init);

bool is_iommu_sec_on_mtee(void)
{
	return false;
}
EXPORT_SYMBOL_GPL(is_iommu_sec_on_mtee);

static int mtk_iommu_pseudo_probe(struct platform_device *pdev)
{
	pr_info("%s do nothing, geniezone is disabled\n", __func__);

	return 0;
}
#endif

static const struct of_device_id mtk_iommu_pseudo_of_ids[] = {
	{ .compatible = "mediatek,mt6833-iommu-pseudo" },
	{},
};

static struct platform_driver mtk_iommu_pseudo_drv = {
	.probe	= mtk_iommu_pseudo_probe,
	.driver	= {
		.name = "mtk-iommu-pseudo",
		.of_match_table = of_match_ptr(mtk_iommu_pseudo_of_ids),
	}
};

module_platform_driver(mtk_iommu_pseudo_drv);
MODULE_LICENSE("GPL v2");
