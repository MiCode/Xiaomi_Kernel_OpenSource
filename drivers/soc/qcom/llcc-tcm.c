// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/mutex.h>
#include <linux/sizes.h>

#include <linux/soc/qcom/llcc-qcom.h>
#include <linux/soc/qcom/llcc-tcm.h>

struct llcc_tcm_drv_data {
	struct device *dev;
	struct llcc_slice_desc *tcm_slice;
	struct llcc_tcm_data *tcm_data;
	bool is_active;
	bool activate_on_init;
	struct mutex lock;
};

static struct llcc_tcm_drv_data *drv_data = (void *) -EPROBE_DEFER;

/**
 * qcom_llcc_tcm_probe - Probes the tcm manager
 * @pdev: the platform device for the llcc driver
 * @table: the llcc slice table
 * @size: the size of the llcc slice table
 * @node: the memory-regions node in the llcc device tree entry
 *
 * Returns 0 on success and a negative error code on failure
 */
int qcom_llcc_tcm_probe(struct platform_device *pdev,
		const struct llcc_slice_config *table, size_t size,
		struct device_node *node)
{
	u32 i;
	int ret;
	struct resource r;

	drv_data = devm_kzalloc(&pdev->dev, sizeof(struct llcc_tcm_drv_data),
			GFP_KERNEL);

	if (!drv_data) {
		pr_err("Failed to allocate tcm driver data\n");
		ret = -ENOMEM;
		goto cfg_err;
	}

	drv_data->tcm_data = devm_kzalloc(&pdev->dev,
			sizeof(struct llcc_tcm_data), GFP_KERNEL);

	if (!drv_data->tcm_data) {
		pr_err("Failed to allocate tcm user data\n");
		ret = -ENOMEM;
		goto cfg_err;
	}

	drv_data->dev = &pdev->dev;
	drv_data->tcm_slice = llcc_slice_getd(LLCC_APTCM);
	if (IS_ERR_OR_NULL(drv_data->tcm_slice)) {
		pr_err("Failed to get tcm slice from llcc driver\n");
		ret = -ENODEV;
		goto cfg_err;
	}

	for (i = 0; i < size; i++) {
		if (table[i].usecase_id == LLCC_APTCM) {
			drv_data->activate_on_init = table[i].activate_on_init;
			break;
		}
	}

	ret = of_address_to_resource(node, 0, &r);
	if (ret)
		goto slice_cfg_err;
	of_node_put(node);

	drv_data->tcm_data->phys_addr = r.start;
	drv_data->tcm_data->mem_size =
		drv_data->tcm_slice->slice_size * SZ_1K;
	drv_data->tcm_data->virt_addr = ioremap(drv_data->tcm_data->phys_addr,
			drv_data->tcm_data->mem_size);
	if (IS_ERR_OR_NULL(drv_data->tcm_data->virt_addr))
		goto slice_cfg_err;


	mutex_init(&drv_data->lock);

	return 0;

slice_cfg_err:
	llcc_slice_putd(drv_data->tcm_slice);
cfg_err:
	drv_data = ERR_PTR(-ENODEV);
	return ret;
}
EXPORT_SYMBOL(qcom_llcc_tcm_probe);

/**
 * llcc_tcm_activate - Activate the TCM slice and give exclusive access
 *
 * A valid pointer to a struct llcc_tcm_data will be returned on success
 * and error pointer on failure
 */
struct llcc_tcm_data *llcc_tcm_activate(void)
{
	int ret;

	if (IS_ERR(drv_data))
		return ERR_PTR(-EPROBE_DEFER);

	mutex_lock(&drv_data->lock);
	if (IS_ERR_OR_NULL(drv_data->tcm_slice) ||
			IS_ERR_OR_NULL(drv_data->tcm_data) ||
			drv_data->is_active) {
		ret = -EBUSY;
		goto act_err;
	}

	/* Should go through anyways if slice is already activated, */
	/* but if not already activated through the TCM manager */
	ret = llcc_slice_activate(drv_data->tcm_slice);
	if (ret) {
		if (drv_data->activate_on_init)
			goto act_err;
		else
			goto act_err_deact;
	}

	drv_data->is_active = true;

	mutex_unlock(&drv_data->lock);
	return drv_data->tcm_data;

act_err_deact:
	llcc_slice_deactivate(drv_data->tcm_slice);
act_err:
	mutex_unlock(&drv_data->lock);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(llcc_tcm_activate);

/**
 * llcc_tcm_deactivate - Deactivate the TCM slice and revoke exclusive access
 * @tcm_data: Pointer to the tcm data descriptor
 */
void llcc_tcm_deactivate(struct llcc_tcm_data *tcm_data)
{
	if (IS_ERR(drv_data) || IS_ERR_OR_NULL(tcm_data))
		return;

	mutex_lock(&drv_data->lock);
	if (IS_ERR_OR_NULL(drv_data->tcm_slice) ||
			IS_ERR_OR_NULL(drv_data->tcm_data) ||
			!drv_data->is_active) {
		mutex_unlock(&drv_data->lock);
		return;
	}

	if (!drv_data->activate_on_init)
		llcc_slice_deactivate(drv_data->tcm_slice);

	drv_data->is_active = false;

	mutex_unlock(&drv_data->lock);
}
EXPORT_SYMBOL(llcc_tcm_deactivate);

/**
 * llcc_tcm_get_phys_addr - Gets the physical address of the tcm slice
 * @tcm_data: Pointer to the tcm data descriptor
 *
 * Returns the physical address on success and 0 on failure
 */
phys_addr_t llcc_tcm_get_phys_addr(struct llcc_tcm_data *tcm_data)
{
	if (IS_ERR_OR_NULL(tcm_data))
		return 0;

	return tcm_data->phys_addr;
}
EXPORT_SYMBOL(llcc_tcm_get_phys_addr);

/**
 * llcc_tcm_get_virt_addr - Gets the virtual address of the tcm slice
 * @tcm_data: Pointer to the tcm data descriptor
 *
 * Returns the virtual address on success and NULL on failure
 */
void __iomem *llcc_tcm_get_virt_addr(struct llcc_tcm_data *tcm_data)
{
	if (IS_ERR_OR_NULL(tcm_data))
		return NULL;

	return tcm_data->virt_addr;
}
EXPORT_SYMBOL(llcc_tcm_get_virt_addr);

/**
 * llcc_tcm_get_slice_size - Gets the size of the tcm slice
 * @tcm_data: Pointer to the tcm data descriptor
 *
 * Returns the size of the slice on success and 0 on failure
 */
size_t llcc_tcm_get_slice_size(struct llcc_tcm_data *tcm_data)
{
	if (IS_ERR_OR_NULL(tcm_data))
		return 0;

	return tcm_data->mem_size;
}
EXPORT_SYMBOL(llcc_tcm_get_slice_size);
