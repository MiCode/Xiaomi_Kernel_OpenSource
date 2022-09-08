// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#define pr_fmt(fmt)    "mtk_iommu: pseudo " fmt

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <dt-bindings/memory/mtk-memory-port.h>
#include "iommu_pseudo.h"

#if IS_ENABLED(CONFIG_MTK_ENABLE_GENIEZONE)
#include <linux/list.h>
#include <soc/mediatek/smi.h>
#include "iommu_gz_sec.h"

#define M4U_L2_ENABLE	1

struct iommu_pseudo_data {
	struct device	*dev;
	struct device	*larb_devs[MTK_LARB_NR_MAX];
	int		larb_ids[MTK_LARB_NR_MAX];
	int		larb_nr;
};

enum iommu_mtee_state {
	STATE_ENABLED,
	STATE_DISABLED,
	STATE_UNINITIALIZED,
	STATE_ERROR,
};

static DEFINE_MUTEX(gM4u_gz_sec_init);
static bool m4u_gz_en[SEC_ID_COUNT];
static int iommu_on_mtee = STATE_UNINITIALIZED;
static struct iommu_pseudo_data *iommu_data;

static int __m4u_gz_sec_init(int mtk_iommu_sec_id)
{
	int ret, i, count = 0;
	struct m4u_gz_sec_context *ctx;

	ctx = m4u_gz_sec_ctx_get();
	if (!ctx)
		return -EFAULT;

	/* Power on all larbs */
	for (i = 0; i < iommu_data->larb_nr; i++) {
		ret = mtk_smi_larb_get(iommu_data->larb_devs[i]);
		if (ret < 0) {
			pr_err("[MTEE]%s: enable larb%d fail, ret:%d\n",
			       __func__, iommu_data->larb_ids[i], ret);
			count = i;
			goto out;
		}
	}
	count = iommu_data->larb_nr;

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
	/* Power off all larbs */
	for (i = 0; i < count; i++)
		mtk_smi_larb_put(iommu_data->larb_devs[i]);

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

	if (iommu_on_mtee != STATE_ENABLED) {
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

bool is_disable_map_sec(void)
{
	return (iommu_on_mtee == STATE_ENABLED);
}
EXPORT_SYMBOL_GPL(is_disable_map_sec);

static int mtk_iommu_pseudo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *larbnode;
	struct platform_device	*plarbdev;
	struct iommu_pseudo_data *data;
	int ret, i, count = 0, larb_nr = 0;

	data = devm_kzalloc(dev, sizeof(struct iommu_pseudo_data),
			    GFP_KERNEL);
	if (!data) {
		iommu_on_mtee = STATE_ERROR;
		return -ENOMEM;
	}

	data->dev = dev;
	count = of_count_phandle_with_args(dev->of_node,
					     "mediatek,larbs", NULL);
	if (count < 0) {
		dev_err(dev, "%s, can't find mediatek,larbs!\n", __func__);
		iommu_on_mtee = STATE_ERROR;
		return count;
	}

	for (i = 0; i < count; i++) {
		u32 id;

		larbnode = of_parse_phandle(dev->of_node, "mediatek,larbs", i);
		if (!larbnode) {
			dev_err(dev, "%s, can't find larbnode:%d !\n",
				__func__, i);
			iommu_on_mtee = STATE_ERROR;
			return -EINVAL;
		}

		if (!of_device_is_available(larbnode)) {
			of_node_put(larbnode);
			continue;
		}

		ret = of_property_read_u32(larbnode, "mediatek,larb-id", &id);
		if (ret)/* The id is consecutive if there is no this property */
			id = i;

		plarbdev = of_find_device_by_node(larbnode);
		if (!plarbdev) {
			dev_err(dev, "%s, can't find larb dev:%d!\n",
				__func__, i);
			iommu_on_mtee = STATE_ERROR;
			of_node_put(larbnode);
			return -EPROBE_DEFER;
		}

		data->larb_devs[larb_nr] = &plarbdev->dev;
		data->larb_ids[larb_nr] = id;
		larb_nr++;
	}
	data->larb_nr = larb_nr;

	iommu_data = data;
	iommu_on_mtee = STATE_ENABLED;
	pr_info("%s done, dev:%s, larb_nr:%d\n", __func__, dev_name(dev),
		larb_nr);

	return 0;
}

#else
int mtk_iommu_sec_init(int mtk_iommu_sec_id)
{
	pr_warn("%s is not support, geniezone is disabled\n", __func__);

	return -1;
}
EXPORT_SYMBOL_GPL(mtk_iommu_sec_init);

bool is_disable_map_sec(void)
{
	return false;
}
EXPORT_SYMBOL_GPL(is_disable_map_sec);

static int mtk_iommu_pseudo_probe(struct platform_device *pdev)
{
	pr_info("%s do nothing, geniezone is disabled\n", __func__);

	return 0;
}
#endif

int tmem_type2sec_id(enum TRUSTED_MEM_REQ_TYPE tmem)
{
	switch (tmem) {
	case TRUSTED_MEM_REQ_PROT_REGION:
		return SEC_ID_SEC_CAM;
	case TRUSTED_MEM_REQ_SVP_REGION:
		return SEC_ID_SVP;
	case TRUSTED_MEM_REQ_WFD_REGION:
		return SEC_ID_WFD;
	default:
		return -1;
	}
}
EXPORT_SYMBOL_GPL(tmem_type2sec_id);

static const struct of_device_id mtk_iommu_pseudo_of_ids[] = {
	{ .compatible = "mediatek,mt6833-iommu-pseudo" },
	{ .compatible = "mediatek,mt6789-iommu-pseudo" },
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
