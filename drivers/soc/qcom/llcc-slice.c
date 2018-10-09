/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)  "%s:" fmt, __func__

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/soc/qcom/llcc-qcom.h>

#define ACTIVATE                      0x1
#define DEACTIVATE                    0x2
#define ACT_CTRL_OPCODE_ACTIVATE      0x1
#define ACT_CTRL_OPCODE_DEACTIVATE    0x2
#define ACT_CTRL_ACT_TRIG             0x1
#define ACT_CTRL_OPCODE_SHIFT         0x1
#define ATTR1_PROBE_TARGET_WAYS_SHIFT 0x2
#define ATTR1_FIXED_SIZE_SHIFT        0x3
#define ATTR1_PRIORITY_SHIFT          0x4
#define ATTR1_MAX_CAP_SHIFT           0x10
#define ATTR0_RES_WAYS_MASK           0x00000fff
#define ATR0_BONUS_WAYS_MASK          0x0fff0000
#define ATR0_BONUS_WAYS_SHIFT         0x10
#define LLCC_STATUS_READ_DELAY 100

#define CACHE_LINE_SIZE_SHIFT 6

#define LLCC_COMMON_STATUS0		0x0003000C
#define LLCC_LB_CNT_MASK		0xf0000000
#define LLCC_LB_CNT_SHIFT		28

#define MAX_CAP_TO_BYTES(n) (n * 1024)
#define LLCC_TRP_ACT_CTRLn(n) (n * 0x1000)
#define LLCC_TRP_STATUSn(n)   (4 + n * 0x1000)
#define LLCC_TRP_ATTR0_CFGn(n) (0x21000 + 0x8 * n)
#define LLCC_TRP_ATTR1_CFGn(n) (0x21004 + 0x8 * n)
#define LLCC_TRP_PCB_ACT 0x21F04
#define LLCC_TRP_SCID_DIS_CAP_ALLOC 0x21F00

/**
 * Driver data for llcc
 * @llcc_virt_base: base address for llcc controller
 * @slice_data: pointer to llcc slice config data
 * @sz: Size of the config data table
 * @llcc_slice_map: Bit map to track the active slice ids
 */
struct llcc_drv_data {
	struct regmap *llcc_map;
	const struct llcc_slice_config *slice_data;
	struct mutex slice_mutex;
	u32 llcc_config_data_sz;
	u32 max_slices;
	u32 b_off;
	u32 no_banks;
	unsigned long *llcc_slice_map;
	bool cap_based_alloc_and_pwr_collapse;
};

/* Get the slice entry by index */
static struct llcc_slice_desc *llcc_slice_get_entry(struct device *dev, int n)
{
	struct of_phandle_args phargs;
	struct llcc_drv_data *drv;
	const struct llcc_slice_config *llcc_data_ptr;
	struct llcc_slice_desc *desc;
	struct platform_device *pdev;
	u32 sz, count;

	if (of_parse_phandle_with_args(dev->of_node, "cache-slices",
				       "#cache-cells", n, &phargs)) {
		pr_err("can't parse \"cache-slices\" property\n");
		return ERR_PTR(-ENODEV);
	}

	pdev = of_find_device_by_node(phargs.np);
	if (!pdev) {
		pr_err("Cannot find platform device from phandle\n");
		return ERR_PTR(-ENODEV);
	}

	drv = platform_get_drvdata(pdev);
	if (!drv) {
		pr_err("cannot find platform driver data\n");
		return ERR_PTR(-EFAULT);
	}

	llcc_data_ptr = drv->slice_data;
	sz = drv->llcc_config_data_sz;
	count = 0;

	while (llcc_data_ptr && count < sz) {
		if (llcc_data_ptr->usecase_id == phargs.args[0])
			break;
		llcc_data_ptr++;
		count++;
	}

	if (llcc_data_ptr == NULL || count == sz) {
		pr_err("can't find %d usecase id\n", phargs.args[0]);
		return ERR_PTR(-ENODEV);
	}

	desc = kzalloc(sizeof(struct llcc_slice_desc), GFP_KERNEL);
	if (!desc)
		return ERR_PTR(-ENOMEM);

	desc->llcc_slice_id = llcc_data_ptr->slice_id;
	desc->llcc_slice_size = llcc_data_ptr->max_cap;
	desc->dev = &pdev->dev;

	return desc;
}

/**
 * llcc_slice_getd - get llcc slice descriptor
 * @dev: Device pointer of the client
 * @name: Name of the use case
 *
 * A pointer to llcc slice descriptor will be returned on success and
 * and error pointer is returned on failure
 */
struct llcc_slice_desc *llcc_slice_getd(struct device *dev, const char *name)
{
	struct device_node *np = dev->of_node;
	int index = 0;
	const char *slice_name;
	struct property *prop;

	if (!np || !of_get_property(np, "cache-slice-names", NULL))
		return ERR_PTR(-ENOENT);

	of_property_for_each_string(np, "cache-slice-names", prop, slice_name) {
		if (!strcmp(name, slice_name))
			break;
		index++;
	}

	return llcc_slice_get_entry(dev, index);
}
EXPORT_SYMBOL(llcc_slice_getd);

/**
 * llcc_slice_putd - llcc slice descritpor
 * @desc: Pointer to llcc slice descriptor
 */
void llcc_slice_putd(struct llcc_slice_desc *desc)
{
	kfree(desc);
}
EXPORT_SYMBOL(llcc_slice_putd);

static int llcc_update_act_ctrl(struct llcc_drv_data *drv, u32 sid,
				u32 act_ctrl_reg_val, u32 status)
{
	u32 act_ctrl_reg;
	u32 status_reg;
	u32 slice_status;
	unsigned long timeout;

	act_ctrl_reg = drv->b_off + LLCC_TRP_ACT_CTRLn(sid);
	status_reg = drv->b_off + LLCC_TRP_STATUSn(sid);

	regmap_write(drv->llcc_map, act_ctrl_reg, act_ctrl_reg_val);

	/* Make sure the activate trigger is applied before clearing it */
	mb();

	/* Clear the ACTIVE trigger */
	act_ctrl_reg_val &= ~ACT_CTRL_ACT_TRIG;
	regmap_write(drv->llcc_map, act_ctrl_reg, act_ctrl_reg_val);

	timeout = jiffies + usecs_to_jiffies(LLCC_STATUS_READ_DELAY);
	while (time_before(jiffies, timeout)) {
		regmap_read(drv->llcc_map, status_reg, &slice_status);
		if (!(slice_status & status))
			return 0;
	}

	return -ETIMEDOUT;
}

/**
 * llcc_slice_activate - Activate the llcc slice
 * @desc: Pointer to llcc slice descriptor
 *
 * A value zero will be returned on success and a negative errno will
 * be returned in error cases
 */
int llcc_slice_activate(struct llcc_slice_desc *desc)
{
	int rc = -EINVAL;
	u32 act_ctrl_val;
	struct llcc_drv_data *drv;

	if (desc == NULL) {
		pr_err("Input descriptor supplied is invalid\n");
		return rc;
	}

	drv = dev_get_drvdata(desc->dev);
	if (!drv) {
		pr_err("Invalid device pointer in the desc\n");
		return rc;
	}

	mutex_lock(&drv->slice_mutex);
	if (test_bit(desc->llcc_slice_id, drv->llcc_slice_map)) {
		mutex_unlock(&drv->slice_mutex);
		return 0;
	}

	act_ctrl_val = ACT_CTRL_OPCODE_ACTIVATE << ACT_CTRL_OPCODE_SHIFT;
	act_ctrl_val |= ACT_CTRL_ACT_TRIG;

	rc = llcc_update_act_ctrl(drv, desc->llcc_slice_id, act_ctrl_val,
				  DEACTIVATE);

	__set_bit(desc->llcc_slice_id, drv->llcc_slice_map);
	mutex_unlock(&drv->slice_mutex);

	return rc;
}
EXPORT_SYMBOL(llcc_slice_activate);

/**
 * llcc_slice_deactivate - Deactivate the llcc slice
 * @desc: Pointer to llcc slice descriptor
 *
 * A value zero will be returned on success and a negative errno will
 * be returned in error cases
 */
int llcc_slice_deactivate(struct llcc_slice_desc *desc)
{
	u32 act_ctrl_val;
	int rc = -EINVAL;
	struct llcc_drv_data *drv;

	if (desc == NULL) {
		pr_err("Input descriptor supplied is invalid\n");
		return rc;
	}

	drv = dev_get_drvdata(desc->dev);
	if (!drv) {
		pr_err("Invalid device pointer in the desc\n");
		return rc;
	}

	mutex_lock(&drv->slice_mutex);
	if (!test_bit(desc->llcc_slice_id, drv->llcc_slice_map)) {
		mutex_unlock(&drv->slice_mutex);
		return 0;
	}
	act_ctrl_val = ACT_CTRL_OPCODE_DEACTIVATE << ACT_CTRL_OPCODE_SHIFT;
	act_ctrl_val |= ACT_CTRL_ACT_TRIG;

	rc = llcc_update_act_ctrl(drv, desc->llcc_slice_id, act_ctrl_val,
				  ACTIVATE);

	__clear_bit(desc->llcc_slice_id, drv->llcc_slice_map);
	mutex_unlock(&drv->slice_mutex);

	return rc;
}
EXPORT_SYMBOL(llcc_slice_deactivate);

/**
 * llcc_get_slice_id - return the slice id
 * @desc: Pointer to llcc slice descriptor
 *
 * A positive value will be returned on success and a negative errno will
 * be returned on error
 */
int llcc_get_slice_id(struct llcc_slice_desc *desc)
{
	if (!desc)
		return -EINVAL;

	return desc->llcc_slice_id;
}
EXPORT_SYMBOL(llcc_get_slice_id);

/**
 * llcc_get_slice_size - return the slice id
 * @desc: Pointer to llcc slice descriptor
 *
 * A positive value will be returned on success and zero will returned on
 * error
 */
size_t llcc_get_slice_size(struct llcc_slice_desc *desc)
{
	if (!desc)
		return 0;

	return desc->llcc_slice_size;
}
EXPORT_SYMBOL(llcc_get_slice_size);

static void qcom_llcc_cfg_program(struct platform_device *pdev)
{
	int i;
	u32 attr1_cfg;
	u32 attr0_cfg;
	u32 attr1_val;
	u32 attr0_val;
	u32 cad_off;
	u32 pcb_off;
	u32 max_cap_cacheline;
	u32 sz;
	u32 pcb = 0;
	u32 cad = 0;
	const struct llcc_slice_config *llcc_table;
	struct llcc_drv_data *drv = platform_get_drvdata(pdev);
	struct llcc_slice_desc desc;
	u32 b_off = drv->b_off;
	bool cap_based_alloc_and_pwr_collapse =
		drv->cap_based_alloc_and_pwr_collapse;

	sz = drv->llcc_config_data_sz;
	llcc_table = drv->slice_data;

	for (i = 0; i < sz; i++) {
		attr1_cfg = b_off + LLCC_TRP_ATTR1_CFGn(llcc_table[i].slice_id);
		attr0_cfg = b_off + LLCC_TRP_ATTR0_CFGn(llcc_table[i].slice_id);

		attr1_val = llcc_table[i].cache_mode;
		attr1_val |= (llcc_table[i].probe_target_ways <<
				ATTR1_PROBE_TARGET_WAYS_SHIFT);
		attr1_val |= (llcc_table[i].fixed_size <<
				ATTR1_FIXED_SIZE_SHIFT);
		attr1_val |= (llcc_table[i].priority << ATTR1_PRIORITY_SHIFT);

		max_cap_cacheline = MAX_CAP_TO_BYTES(llcc_table[i].max_cap);

		/* LLCC instances can vary for each target.
		 * The SW writes to broadcast register which gets propagated
		 * to each llcc instace (llcc0,.. llccN).
		 * Since the size of the memory is divided equally amongst the
		 * llcc instances, we need to configure the max cap accordingly.
		 */
		max_cap_cacheline = (max_cap_cacheline / drv->no_banks);
		max_cap_cacheline >>= CACHE_LINE_SIZE_SHIFT;
		attr1_val |= (max_cap_cacheline << ATTR1_MAX_CAP_SHIFT);

		attr0_val = llcc_table[i].res_ways & ATTR0_RES_WAYS_MASK;
		attr0_val |= llcc_table[i].bonus_ways << ATR0_BONUS_WAYS_SHIFT;

		regmap_write(drv->llcc_map, attr1_cfg, attr1_val);
		regmap_write(drv->llcc_map, attr0_cfg, attr0_val);

		if (cap_based_alloc_and_pwr_collapse) {
			cad_off = b_off + LLCC_TRP_SCID_DIS_CAP_ALLOC;
			cad |= llcc_table[i].dis_cap_alloc <<
				llcc_table[i].slice_id;
			regmap_write(drv->llcc_map, cad_off, cad);

			pcb_off = b_off + LLCC_TRP_PCB_ACT;
			pcb |= llcc_table[i].retain_on_pc <<
					llcc_table[i].slice_id;
			regmap_write(drv->llcc_map, pcb_off, pcb);
		}

		/* Make sure that the SCT is programmed before activating */
		mb();

		if (llcc_table[i].activate_on_init) {
			desc.llcc_slice_id = llcc_table[i].slice_id;
			desc.dev = &pdev->dev;
			if (llcc_slice_activate(&desc)) {
				pr_err("activate slice id: %d timed out\n",
						desc.llcc_slice_id);
			}
		}
	}
}

int qcom_llcc_probe(struct platform_device *pdev,
		      const struct llcc_slice_config *llcc_cfg, u32 sz)
{
	int rc = 0;
	u32 num_banks = 0;
	struct device *dev = &pdev->dev;
	static struct llcc_drv_data *drv_data;

	drv_data = devm_kzalloc(dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data)
		return PTR_ERR(drv_data);

	drv_data->llcc_map = syscon_node_to_regmap(dev->parent->of_node);
	if (!drv_data->llcc_map)
		return PTR_ERR(drv_data->llcc_map);

	regmap_read(drv_data->llcc_map, LLCC_COMMON_STATUS0,
		    &num_banks);

	num_banks &= LLCC_LB_CNT_MASK;
	num_banks >>= LLCC_LB_CNT_SHIFT;
	drv_data->no_banks = num_banks;

	rc = of_property_read_u32(pdev->dev.of_node, "max-slices",
				  &drv_data->max_slices);
	if (rc) {
		dev_err(&pdev->dev, "Invalid max-slices dt entry\n");
		devm_kfree(&pdev->dev, drv_data);
		return rc;
	}

	rc = of_property_read_u32(pdev->dev.parent->of_node,
			"qcom,llcc-broadcast-off", &drv_data->b_off);
	if (rc) {
		dev_err(&pdev->dev, "Invalid qcom,broadcast-off entry\n");
		devm_kfree(&pdev->dev, drv_data);
		return rc;
	}

	drv_data->cap_based_alloc_and_pwr_collapse =
		of_property_read_bool(pdev->dev.of_node,
				      "cap-based-alloc-and-pwr-collapse");

	drv_data->llcc_slice_map = kcalloc(BITS_TO_LONGS(drv_data->max_slices),
				   sizeof(unsigned long), GFP_KERNEL);

	if (!drv_data->llcc_slice_map) {
		devm_kfree(&pdev->dev, drv_data);
		return PTR_ERR(drv_data->llcc_slice_map);
	}

	bitmap_zero(drv_data->llcc_slice_map, drv_data->max_slices);
	drv_data->slice_data = llcc_cfg;
	drv_data->llcc_config_data_sz = sz;
	mutex_init(&drv_data->slice_mutex);
	platform_set_drvdata(pdev, drv_data);

	qcom_llcc_cfg_program(pdev);

	return rc;
}
EXPORT_SYMBOL(qcom_llcc_probe);

int qcom_llcc_remove(struct platform_device *pdev)
{
	static struct llcc_drv_data *drv_data;

	drv_data = platform_get_drvdata(pdev);

	mutex_destroy(&drv_data->slice_mutex);
	kfree(drv_data->llcc_slice_map);
	devm_kfree(&pdev->dev, drv_data);
	platform_set_drvdata(pdev, NULL);

	return 0;
}
EXPORT_SYMBOL(qcom_llcc_remove);
