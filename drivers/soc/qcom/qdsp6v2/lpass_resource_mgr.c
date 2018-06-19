/*
 * Copyright (c) 2018 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/qdsp6v2/apr.h>
#include <linux/of_device.h>
#include <linux/sysfs.h>
#include <sound/q6afe-v2.h>
#include <sound/q6core.h>

#define LPASS_LPAIF_PCM_CTLa(a)        (0x1500 + 0x1000 * (a))
#define LPASS_LPAIF_PCM_CTLa_ELEM              4
#define LPASS_LPAIF_PCM_CTLa_MAX               3
#define LPASS_LPAIF_PCM_CTLa__ENABLE_TX___M    0x02000000

#define LPASS_RES_MGR_THREAD_NAME "lpass_resource_mgr_thread"

#define lpass_io_r(a) readl_relaxed(a)
#define LPASS_REG_OFFSET(_virt_addr_, _phys_addr_) \
	((_virt_addr_)-(_phys_addr_))

#define CHECK_EARLY_AUDIO_CMD   0
#define MAX_TIMEOUT_COUNT       20
#define LPASS_CHECK_DELAY_MS    1000
#define LPASS_BOOT_DELAY_MS     2000
#define LPASS_STATUS_DELAY_MS   500

static ssize_t check_early_audio_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf, size_t count);

struct lpass_resource_mgr_private {
	struct kobject *lpass_resource_mgr_obj;
	struct attribute_group *attr_group;
	void __iomem *lpaif_mapped_base;
	struct task_struct *lpass_res_mgr_thread;
	uint32_t lpass_lpaif_base_addr;
	uint32_t lpass_lpaif_reg_size;
	uint32_t lpass_max_rddma;
	uint32_t lpass_max_wrdma;
	uint32_t num_reserved_rddma;
	uint32_t num_reserved_wrdma;
	uint32_t *reserved_rddma;
	uint32_t *reserved_wrdma;
	uint32_t early_audio_pcm_idx;
	u32 is_early_audio_enabled;
};

static struct kobj_attribute check_early_audio_attribute =
	__ATTR(check_early_audio, 0220, NULL, check_early_audio_store);

static struct attribute *attrs[] = {
	&check_early_audio_attribute.attr,
	NULL,
};

static struct lpass_resource_mgr_private *priv;

static struct platform_device *dev_private;

static uint32_t lpass_read_reg(void __iomem *phys_addr, uint32_t virt_offset)
{
	uint32_t read_val;

	read_val = lpass_io_r(phys_addr+virt_offset);
	return read_val;
}

static void lpass_resource_mgr_check_early_audio(struct platform_device *pdev)
{
	if (priv->is_early_audio_enabled)
		dev_err(&pdev->dev, "%s: Online\n",
			__func__);
	else
		dev_err(&pdev->dev, "%s: Offline\n",
			__func__);
}

static int lpass_resource_mgr_thread(void *data)
{
	struct platform_device *pdev = dev_private;
	int i, ret = 0;
	bool *ret_rddma;
	bool *ret_wrdma;
	int total_num_allocated_dma;
	int timeout_count = 0;

	if (!pdev) {
		dev_err(&pdev->dev, "%s: Platform device null\n", __func__);
		goto done;
	}

	/* Check early audio status if it's enabled */
	if (priv->is_early_audio_enabled) {
		int mask, read_val = 0;
		bool is_check_done = false;
		int pcm_idx = priv->early_audio_pcm_idx;

		mask = LPASS_LPAIF_PCM_CTLa__ENABLE_TX___M;
		while (!is_check_done) {
			if (timeout_count > MAX_TIMEOUT_COUNT) {
				dev_err(&pdev->dev, "%s: Early audio check TIMED OUT.\n",
					__func__);
				ret = -ETIMEDOUT;
				goto done;
			}

			read_val = lpass_read_reg(priv->lpaif_mapped_base,
				LPASS_LPAIF_PCM_CTLa(pcm_idx));

			if (!(read_val & mask)) {
				dev_dbg(&pdev->dev, "%s: PCM interface %d is disabled\n",
					__func__, pcm_idx);
				is_check_done = true;
			} else {
				dev_dbg_ratelimited(&pdev->dev,
					"%s: PCM Interface %d enabled\n",
					__func__, pcm_idx);
			}

			msleep(LPASS_CHECK_DELAY_MS);
			timeout_count++;
		}
		priv->is_early_audio_enabled = false;
	}

	total_num_allocated_dma = priv->num_reserved_rddma +
		priv->num_reserved_wrdma;
	if (total_num_allocated_dma == 0) {
		dev_dbg(&pdev->dev, "%s: No DMAs to allocate\n",
			__func__);
		goto done;
	}

	timeout_count = 0;
	while (apr_get_q6_state() == APR_SUBSYS_DOWN) {
		if (timeout_count > MAX_TIMEOUT_COUNT) {
			dev_err(&pdev->dev, "%s: apr_get_q6_state() TIMED OUT.\n",
				__func__);
			ret = -ETIMEDOUT;
			goto done;
		}

		dev_dbg_ratelimited(&pdev->dev, "%s: ADSP is down\n",
			__func__);
		msleep(LPASS_BOOT_DELAY_MS);
		timeout_count++;
	}

	timeout_count = 0;
	while (q6core_is_adsp_ready() != AVCS_SERVICE_AND_ALL_MODULES_READY) {
		if (timeout_count > MAX_TIMEOUT_COUNT) {
			dev_err(&pdev->dev, "%s: q6core_is_adsp_ready() TIMED OUT.\n",
				__func__);
			ret = -ETIMEDOUT;
			goto done;
		}

		dev_dbg_ratelimited(&pdev->dev,
			"%s: Not All QADSP6 Services are ready!!\n",
			__func__);
		msleep(LPASS_STATUS_DELAY_MS);
		timeout_count++;
	}

	/* Allocated resources then check DMA indices allocated */
	ret = afe_request_dma_resources(AFE_LPAIF_DEFAULT_DMA_TYPE,
		priv->num_reserved_rddma,
		priv->num_reserved_wrdma);

	if (ret) {
		dev_err(&pdev->dev, "%s: AFE DMA Request failed with code %d\n",
			__func__, ret);
		goto done;
	}

	ret = afe_get_dma_idx(&ret_rddma, &ret_wrdma);

	if (ret) {
		dev_err(&pdev->dev, "%s: Cannot obtain DMA info %d\n",
			__func__, ret);
		goto done;
	}

	for (i = 0; i < priv->num_reserved_rddma; i++) {
		if (ret_rddma[priv->reserved_rddma[i]])
			break;

		dev_err(&pdev->dev, "%s: ret rddma %d idx no match\n",
			__func__, priv->reserved_rddma[i]);
	}

	for (i = 0; i < priv->num_reserved_wrdma; i++) {
		if (ret_wrdma[priv->reserved_wrdma[i]])
			break;

		dev_err(&pdev->dev, "%s: ret wrdma %d idx no match\n",
			__func__, priv->reserved_wrdma[i]);
	}

done:
	return ret;
}

static ssize_t check_early_audio_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t count)
{
	struct platform_device *pdev = dev_private;
	int cmd = 0;
	int ret = 0;

	if (!pdev) {
		dev_err(&pdev->dev, "%s: Platform device null\n", __func__);
		goto store_end;
	}

	ret = sscanf(buf, "%du", &cmd);

	if (ret != 1) {
		dev_err(&pdev->dev, "%s: Invalid number of arguments %d\n",
			__func__, ret);
		goto store_end;
	}

	switch (cmd) {
	case CHECK_EARLY_AUDIO_CMD:
		lpass_resource_mgr_check_early_audio(dev_private);
		break;
	default:
		dev_err(&pdev->dev, "%s: Unrecoginized cmd %d\n",
			__func__, cmd);
		break;
	}

store_end:
	dev_dbg(&pdev->dev, "%s: Exiting. Count is %d\n",
		__func__, (int) count);
	return count;
}

static int lpass_resource_mgr_init_sysfs(struct platform_device *pdev)
{
	int ret = -EINVAL;
	u32 max_num_pcm_interfaces;
	u32 lpass_lpaif_vals[2];

	dev_private = NULL;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto priv_err_ret;
	}

	platform_set_drvdata(pdev, priv);

	priv->lpass_resource_mgr_obj = NULL;
	priv->attr_group = devm_kzalloc(&pdev->dev,
				sizeof(*(priv->attr_group)),
				GFP_KERNEL);
	if (!priv->attr_group) {
		ret = -ENOMEM;
		goto priv_err_ret;
	}

	priv->attr_group->attrs = attrs;

	priv->lpass_resource_mgr_obj = kobject_create_and_add(
		"lpass_resource_mgr", kernel_kobj);
	if (!priv->lpass_resource_mgr_obj) {
		dev_err(&pdev->dev, "%s: sysfs create and add failed\n",
			__func__);
		ret = -ENOMEM;
		goto priv_err_ret;
	}

	ret = sysfs_create_group(priv->lpass_resource_mgr_obj,
		priv->attr_group);
	if (ret) {
		dev_err(&pdev->dev, "%s: sysfs create group failed %d\n",
			__func__, ret);
		goto lpass_obj_err_ret;
	}

	dev_private = pdev;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "%s: Device tree information is missing\n",
			__func__);
		ret = -ENODATA;
		goto lpass_obj_err_ret;
	}

	/* Read Device Tree Information */
	ret = of_property_read_u32_array(pdev->dev.of_node,
		"qcom,lpass-lpaif-reg", lpass_lpaif_vals, 2);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: Error %d reading lpass-lpaif-reg.\n",
			__func__, ret);
		goto lpass_obj_err_ret;
	}

	priv->lpass_lpaif_base_addr = lpass_lpaif_vals[0];
	priv->lpass_lpaif_reg_size = lpass_lpaif_vals[1];
	priv->lpaif_mapped_base = ioremap(priv->lpass_lpaif_base_addr,
		priv->lpass_lpaif_reg_size);
	if (!priv->lpaif_mapped_base) {
		dev_err(&pdev->dev, "%s: Failed to map LPASS LPAIF Base Address 0x%08x\n",
			__func__, priv->lpass_lpaif_base_addr);
		ret = -ENOMEM;
		goto lpass_obj_err_ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
		"qcom,lpass-max-rddma", &priv->lpass_max_rddma);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: Error %d reading lpass-max-rddma.\n",
			__func__, ret);
		goto lpaif_map_err_ret;
	}

	if (priv->lpass_max_rddma > AFE_MAX_RDDMA) {
		dev_err(&pdev->dev,
			"%s: Device tree max RDDMA > kernel max\n",
			__func__);
		ret = -EINVAL;
		goto lpaif_map_err_ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
		"qcom,lpass-max-wrdma", &priv->lpass_max_wrdma);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: Error %d reading lpass-max-wrdma.\n",
			__func__, ret);
		goto lpaif_map_err_ret;
	}

	if (priv->lpass_max_wrdma > AFE_MAX_WRDMA) {
		dev_err(&pdev->dev,
			"%s: Device tree max WRDMA > kernel max\n",
			__func__);
		ret = -EINVAL;
		goto lpaif_map_err_ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
		"qcom,num-reserved-rddma", &priv->num_reserved_rddma);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: Error %d reading num-reserved-rddma.\n",
			__func__, ret);
		goto lpaif_map_err_ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
		"qcom,num-reserved-wrdma", &priv->num_reserved_wrdma);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: Error %d reading num-reserved-wrdma.\n",
			__func__, ret);
		ret = -EINVAL;
		goto lpaif_map_err_ret;
	}

	if ((priv->num_reserved_rddma > priv->lpass_max_rddma) ||
		(priv->num_reserved_wrdma > priv->lpass_max_wrdma)) {
		dev_err(&pdev->dev,
			"%s: Reserved DMA greater than max\n",
			__func__);
		ret = -EINVAL;
		goto lpaif_map_err_ret;
	}

	if (priv->num_reserved_rddma > 0) {
		priv->reserved_rddma = devm_kcalloc(&pdev->dev,
			priv->num_reserved_rddma,
			sizeof(uint32_t),
			GFP_KERNEL);

		if (!priv->reserved_rddma) {
			ret = -ENOMEM;
			goto lpaif_map_err_ret;
		}

		ret = of_property_read_u32_array(pdev->dev.of_node,
			"qcom,reserved-rddma", priv->reserved_rddma,
			priv->num_reserved_rddma);
		if (ret) {
			dev_err(&pdev->dev,
				"%s: Error %d reading reserved-rddma.\n",
				__func__, ret);
			goto lpaif_map_err_ret;
		}
	}

	if (priv->num_reserved_wrdma > 0) {
		priv->reserved_wrdma = devm_kcalloc(&pdev->dev,
			priv->num_reserved_wrdma,
			sizeof(uint32_t),
			GFP_KERNEL);

		if (!priv->reserved_wrdma) {
			ret = -ENOMEM;
			goto lpaif_map_err_ret;
		}
		ret = of_property_read_u32_array(pdev->dev.of_node,
			"qcom,reserved-wrdma", priv->reserved_wrdma,
			priv->num_reserved_wrdma);
		if (ret) {
			dev_err(&pdev->dev,
				"%s: Error %d reading reserved-wrdma.\n",
				__func__, ret);
			goto lpaif_map_err_ret;
		}
	}

	ret = of_property_read_u32(pdev->dev.of_node,
		"qcom,early-audio-enabled", &priv->is_early_audio_enabled);
	if (ret) {
		dev_dbg(&pdev->dev,
			"%s: Error %d reading early-audio-enabled\n",
			__func__, ret);
		priv->is_early_audio_enabled = 0;
	}

	if (priv->is_early_audio_enabled) {
		ret = of_property_read_u32(pdev->dev.of_node,
			 "qcom,max-num-pcm-intf", &max_num_pcm_interfaces);
		if (ret)
			dev_err(&pdev->dev,
				"%s: Error %d reading max-num-pcm-intf\n",
				__func__, ret);

		ret = of_property_read_u32(pdev->dev.of_node,
			 "qcom,early-audio-pcm", &priv->early_audio_pcm_idx);
		if (ret)
			dev_err(&pdev->dev,
				"%s: Error %d reading early-audio-pcm\n",
				__func__, ret);
	}

	priv->lpass_res_mgr_thread = kthread_run(
		lpass_resource_mgr_thread,
		NULL,
		LPASS_RES_MGR_THREAD_NAME);

	return 0;
lpaif_map_err_ret:
	if (priv->lpaif_mapped_base)
		iounmap(priv->lpaif_mapped_base);

lpass_obj_err_ret:
	if (priv->lpass_resource_mgr_obj) {
		kobject_del(priv->lpass_resource_mgr_obj);
		priv->lpass_resource_mgr_obj = NULL;
	}

priv_err_ret:
	return ret;
}

static int lpass_resource_mgr_remove(struct platform_device *pdev)
{
	struct lpass_resource_mgr_private *priv = NULL;

	priv = platform_get_drvdata(pdev);

	if (!priv)
		return 0;

	if (priv->lpaif_mapped_base)
		iounmap(priv->lpaif_mapped_base);

	if (priv->lpass_resource_mgr_obj) {
		sysfs_remove_group(priv->lpass_resource_mgr_obj,
			priv->attr_group);

		kobject_del(priv->lpass_resource_mgr_obj);
		priv->lpass_resource_mgr_obj = NULL;
	}

	kthread_stop(priv->lpass_res_mgr_thread);
	afe_release_all_dma_resources();
	return 0;
}

static int lpass_resource_mgr_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = lpass_resource_mgr_init_sysfs(pdev);

	if (ret != 0) {
		dev_err(&pdev->dev, "%s: Error in initing sysfs\n", __func__);
		return ret;
	}

	return 0;
}

static const struct of_device_id lpass_resource_mgr_dt_match[] = {
	{ .compatible = "qcom,lpass-resource-manager" },
	{ }
};
MODULE_DEVICE_TABLE(of, lpass_resource_mgr_dt_match);

static struct platform_driver lpass_resource_mgr_driver = {
	.driver = {
		.name = "lpass-resource-manager",
		.owner = THIS_MODULE,
		.of_match_table = lpass_resource_mgr_dt_match,
	},
	.probe = lpass_resource_mgr_probe,
	.remove = lpass_resource_mgr_remove,
};

static int __init lpass_resource_mgr_init(void)
{
	return platform_driver_register(&lpass_resource_mgr_driver);
}
module_init(lpass_resource_mgr_init);

static void __exit lpass_resource_mgr_exit(void)
{
	platform_driver_unregister(&lpass_resource_mgr_driver);
}
module_exit(lpass_resource_mgr_exit);

MODULE_DESCRIPTION("LPASS Resource Manager module");
MODULE_LICENSE("GPL v2");
