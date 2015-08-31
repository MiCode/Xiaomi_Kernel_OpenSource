/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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
#define pr_fmt(fmt)	"ACC: %s: " fmt, __func__

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/string.h>
#include <soc/qcom/scm.h>

#define MEM_ACC_DEFAULT_SEL_SIZE	2

#define BYTES_PER_FUSE_ROW		8

/* mem-acc config flags */
#define MEM_ACC_SKIP_L1_CONFIG		BIT(0)
#define FUSE_MAP_NO_MATCH		(-1)
#define FUSE_PARAM_MATCH_ANY		(-1)

enum {
	MEMORY_L1,
	MEMORY_L2,
	MEMORY_MAX,
};

#define MEM_ACC_TYPE_MAX		6

struct mem_acc_regulator {
	struct device		*dev;
	struct regulator_desc	rdesc;
	struct regulator_dev	*rdev;

	int			corner;
	bool			mem_acc_supported[MEMORY_MAX];
	bool			mem_acc_custom_supported[MEMORY_MAX];

	u32			*acc_sel_mask[MEMORY_MAX];
	u32			*acc_sel_bit_pos[MEMORY_MAX];
	u32			acc_sel_bit_size[MEMORY_MAX];
	u32			num_acc_sel[MEMORY_MAX];
	u32			*acc_en_bit_pos;
	u32			num_acc_en;
	u32			*corner_acc_map;
	u32			num_corners;
	u32			override_fuse_value;
	int			override_map_match;
	int			override_map_count;


	void __iomem		*acc_sel_base[MEMORY_MAX];
	void __iomem		*acc_en_base;
	phys_addr_t		acc_sel_addr[MEMORY_MAX];
	phys_addr_t		acc_en_addr;
	u32			flags;

	void __iomem		*acc_custom_addr[MEMORY_MAX];
	u32			*acc_custom_data[MEMORY_MAX];

	phys_addr_t		mem_acc_type_addr[MEM_ACC_TYPE_MAX];
	u32			*mem_acc_type_data;

	/* eFuse parameters */
	phys_addr_t		efuse_addr;
	void __iomem		*efuse_base;
};

static DEFINE_MUTEX(mem_acc_memory_mutex);

static u64 mem_acc_read_efuse_row(struct mem_acc_regulator *mem_acc_vreg,
					u32 row_num, bool use_tz_api)
{
	int rc;
	u64 efuse_bits;
	struct scm_desc desc = {0};
	struct mem_acc_read_req {
		u32 row_address;
		int addr_type;
	} req;

	struct mem_acc_read_rsp {
		u32 row_data[2];
		u32 status;
	} rsp;

	if (!use_tz_api) {
		efuse_bits = readq_relaxed(mem_acc_vreg->efuse_base
			+ row_num * BYTES_PER_FUSE_ROW);
		return efuse_bits;
	}

	desc.args[0] = req.row_address = mem_acc_vreg->efuse_addr +
					row_num * BYTES_PER_FUSE_ROW;
	desc.args[1] = req.addr_type = 0;
	desc.arginfo = SCM_ARGS(2);
	efuse_bits = 0;

	if (!is_scm_armv8()) {
		rc = scm_call(SCM_SVC_FUSE, SCM_FUSE_READ,
			&req, sizeof(req), &rsp, sizeof(rsp));
	} else {
		rc = scm_call2(SCM_SIP_FNID(SCM_SVC_FUSE, SCM_FUSE_READ),
				&desc);
		rsp.row_data[0] = desc.ret[0];
		rsp.row_data[1] = desc.ret[1];
		rsp.status = desc.ret[2];
	}

	if (rc) {
		pr_err("read row %d failed, err code = %d", row_num, rc);
	} else {
		efuse_bits = ((u64)(rsp.row_data[1]) << 32) +
				(u64)rsp.row_data[0];
	}

	return efuse_bits;
}

static int mem_acc_fuse_is_setting_expected(
		struct mem_acc_regulator *mem_acc_vreg, u32 sel_array[5])
{
	u64 fuse_bits;
	u32 ret;

	fuse_bits = mem_acc_read_efuse_row(mem_acc_vreg, sel_array[0],
							sel_array[4]);
	ret = (fuse_bits >> sel_array[1]) & ((1 << sel_array[2]) - 1);
	if (ret == sel_array[3])
		ret = 1;
	else
		ret = 0;

	pr_info("[row:%d] = 0x%llx @%d:%d == %d ?: %s\n",
			sel_array[0], fuse_bits,
			sel_array[1], sel_array[2],
			sel_array[3],
			(ret == 1) ? "yes" : "no");
	return ret;
}

static inline u32 apc_to_acc_corner(struct mem_acc_regulator *mem_acc_vreg,
								int corner)
{
	/*
	 * corner_acc_map maps the corner from index 0 and  APC corner value
	 * starts from the value 1
	 */
	return mem_acc_vreg->corner_acc_map[corner - 1];
}

static void __update_acc_sel(struct mem_acc_regulator *mem_acc_vreg,
						int corner, int mem_type)
{
	u32 acc_data, acc_data_old, i, bit, acc_corner;

	/*
	 * Do not configure the L1 ACC corner if the the corresponding flag is
	 * set.
	 */
	if ((mem_type == MEMORY_L1)
			&& (mem_acc_vreg->flags & MEM_ACC_SKIP_L1_CONFIG))
		return;

	acc_data = readl_relaxed(mem_acc_vreg->acc_sel_base[mem_type]);
	acc_data_old = acc_data;
	for (i = 0; i < mem_acc_vreg->num_acc_sel[mem_type]; i++) {
		bit = mem_acc_vreg->acc_sel_bit_pos[mem_type][i];
		acc_data &= ~mem_acc_vreg->acc_sel_mask[mem_type][i];
		acc_corner = apc_to_acc_corner(mem_acc_vreg, corner);
		acc_data |= (acc_corner << bit) &
			mem_acc_vreg->acc_sel_mask[mem_type][i];
	}
	pr_debug("corner=%d old_acc_sel=0x%02x new_acc_sel=0x%02x mem_type=%d\n",
			corner, acc_data_old, acc_data, mem_type);
	writel_relaxed(acc_data, mem_acc_vreg->acc_sel_base[mem_type]);
}

static void __update_acc_type(struct mem_acc_regulator *mem_acc_vreg,
				int corner)
{
	int i, rc;

	for (i = 0; i < MEM_ACC_TYPE_MAX; i++) {
		if (mem_acc_vreg->mem_acc_type_addr[i]) {
			rc = scm_io_write(mem_acc_vreg->mem_acc_type_addr[i],
				mem_acc_vreg->mem_acc_type_data[corner - 1 + i *
				mem_acc_vreg->num_corners]);
			if (rc)
				pr_err("scm_io_write: %pa failure rc:%d\n",
					&(mem_acc_vreg->mem_acc_type_addr[i]),
					rc);
		}
	}
}

static void __update_acc_custom(struct mem_acc_regulator *mem_acc_vreg,
						int corner, int mem_type)
{
	writel_relaxed(
		mem_acc_vreg->acc_custom_data[mem_type][corner-1],
		mem_acc_vreg->acc_custom_addr[mem_type]);
	pr_debug("corner=%d mem_type=%d custom_data=0x%2x\n", corner,
		mem_type, mem_acc_vreg->acc_custom_data[mem_type][corner-1]);
}

static void update_acc_sel(struct mem_acc_regulator *mem_acc_vreg, int corner)
{
	int i;

	for (i = 0; i < MEMORY_MAX; i++) {
		if (mem_acc_vreg->mem_acc_supported[i])
			__update_acc_sel(mem_acc_vreg, corner, i);
		if (mem_acc_vreg->mem_acc_custom_supported[i])
			__update_acc_custom(mem_acc_vreg, corner, i);
	}

	if (mem_acc_vreg->mem_acc_type_data)
		__update_acc_type(mem_acc_vreg, corner);
}

static int mem_acc_regulator_set_voltage(struct regulator_dev *rdev,
		int corner, int corner_max, unsigned *selector)
{
	struct mem_acc_regulator *mem_acc_vreg = rdev_get_drvdata(rdev);
	int i;

	if (corner > mem_acc_vreg->num_corners) {
		pr_err("Invalid corner=%d requested\n", corner);
		return -EINVAL;
	}

	pr_debug("old corner=%d, new corner=%d\n",
			mem_acc_vreg->corner, corner);

	if (corner == mem_acc_vreg->corner)
		return 0;

	/* go up or down one level at a time */
	mutex_lock(&mem_acc_memory_mutex);
	if (corner > mem_acc_vreg->corner) {
		for (i = mem_acc_vreg->corner + 1; i <= corner; i++) {
			pr_debug("UP: to corner %d\n", i);
			update_acc_sel(mem_acc_vreg, i);
		}
	} else {
		for (i = mem_acc_vreg->corner - 1; i >= corner; i--) {
			pr_debug("DOWN: to corner %d\n", i);
			update_acc_sel(mem_acc_vreg, i);
		}
	}
	mutex_unlock(&mem_acc_memory_mutex);

	pr_debug("new voltage corner set %d\n", corner);

	mem_acc_vreg->corner = corner;

	return 0;
}

static int mem_acc_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct mem_acc_regulator *mem_acc_vreg = rdev_get_drvdata(rdev);

	return mem_acc_vreg->corner;
}

static struct regulator_ops mem_acc_corner_ops = {
	.set_voltage		= mem_acc_regulator_set_voltage,
	.get_voltage		= mem_acc_regulator_get_voltage,
};

static int __mem_acc_sel_init(struct mem_acc_regulator *mem_acc_vreg,
							int mem_type)
{
	int i;
	u32 bit, mask;

	mem_acc_vreg->acc_sel_mask[mem_type] = devm_kzalloc(mem_acc_vreg->dev,
		mem_acc_vreg->num_acc_sel[mem_type] * sizeof(u32), GFP_KERNEL);
	if (!mem_acc_vreg->acc_sel_mask[mem_type]) {
		pr_err("Unable to allocate memory for mem_type=%d\n", mem_type);
		return -ENOMEM;
	}

	for (i = 0; i < mem_acc_vreg->num_acc_sel[mem_type]; i++) {
		bit = mem_acc_vreg->acc_sel_bit_pos[mem_type][i];
		mask = BIT(mem_acc_vreg->acc_sel_bit_size[mem_type]) - 1;
		mem_acc_vreg->acc_sel_mask[mem_type][i] = mask << bit;
	}

	return 0;
}

static int mem_acc_sel_init(struct mem_acc_regulator *mem_acc_vreg)
{
	int i, rc;

	for (i = 0; i < MEMORY_MAX; i++) {
		if (mem_acc_vreg->mem_acc_supported[i]) {
			rc = __mem_acc_sel_init(mem_acc_vreg, i);
			if (rc) {
				pr_err("Unable to intialize mem_type=%d rc=%d\n",
								i, rc);
				return rc;
			}
		}
	}

	return 0;
}

static void mem_acc_en_init(struct mem_acc_regulator *mem_acc_vreg)
{
	int i, bit;
	u32 acc_data;

	acc_data = readl_relaxed(mem_acc_vreg->acc_en_base);
	pr_debug("init: acc_en_register=%x\n", acc_data);
	for (i = 0; i < mem_acc_vreg->num_acc_en; i++) {
		bit = mem_acc_vreg->acc_en_bit_pos[i];
		acc_data |= BIT(bit);
	}
	pr_debug("final: acc_en_register=%x\n", acc_data);
	writel_relaxed(acc_data, mem_acc_vreg->acc_en_base);
}

static int populate_acc_data(struct mem_acc_regulator *mem_acc_vreg,
			const char *prop_name, u32 **value, u32 *len)
{
	int rc;

	if (!of_get_property(mem_acc_vreg->dev->of_node, prop_name, len)) {
		pr_err("Unable to find %s property\n", prop_name);
		return -EINVAL;
	}
	*len /= sizeof(u32);
	if (!(*len)) {
		pr_err("Incorrect entries in %s\n", prop_name);
		return -EINVAL;
	}

	*value = devm_kzalloc(mem_acc_vreg->dev, (*len) * sizeof(u32),
							GFP_KERNEL);
	if (!(*value)) {
		pr_err("Unable to allocate memory for %s\n", prop_name);
		return -ENOMEM;
	}

	pr_debug("Found %s, data-length = %d\n", prop_name, *len);

	rc = of_property_read_u32_array(mem_acc_vreg->dev->of_node,
					prop_name, *value, *len);
	if (rc) {
		pr_err("Unable to populate %s rc=%d\n", prop_name, rc);
		return rc;
	}

	return 0;
}

static int mem_acc_sel_setup(struct mem_acc_regulator *mem_acc_vreg,
			struct resource *res, int mem_type)
{
	int len, rc;
	char *mem_select_str;
	char *mem_select_size_str;

	mem_acc_vreg->acc_sel_addr[mem_type] = res->start;
	len = res->end - res->start + 1;
	pr_debug("'acc_sel_addr' = %pa mem_type=%d (len=%d)\n",
					&res->start, mem_type, len);

	mem_acc_vreg->acc_sel_base[mem_type] = devm_ioremap(mem_acc_vreg->dev,
			mem_acc_vreg->acc_sel_addr[mem_type], len);
	if (!mem_acc_vreg->acc_sel_base[mem_type]) {
		pr_err("Unable to map 'acc_sel_addr' %pa for mem_type=%d\n",
			&mem_acc_vreg->acc_sel_addr[mem_type], mem_type);
		return -EINVAL;
	}

	switch (mem_type) {
	case MEMORY_L1:
		mem_select_str = "qcom,acc-sel-l1-bit-pos";
		mem_select_size_str = "qcom,acc-sel-l1-bit-size";
		break;
	case MEMORY_L2:
		mem_select_str = "qcom,acc-sel-l2-bit-pos";
		mem_select_size_str = "qcom,acc-sel-l2-bit-size";
		break;
	default:
		pr_err("Invalid memory type: %d\n", mem_type);
		return -EINVAL;
	}

	mem_acc_vreg->acc_sel_bit_size[mem_type] = MEM_ACC_DEFAULT_SEL_SIZE;
	of_property_read_u32(mem_acc_vreg->dev->of_node, mem_select_size_str,
			&mem_acc_vreg->acc_sel_bit_size[mem_type]);

	rc = populate_acc_data(mem_acc_vreg, mem_select_str,
			&mem_acc_vreg->acc_sel_bit_pos[mem_type],
			&mem_acc_vreg->num_acc_sel[mem_type]);
	if (rc)
		pr_err("Unable to populate '%s' rc=%d\n", mem_select_str, rc);

	return rc;
}

static int mem_acc_efuse_init(struct platform_device *pdev,
				 struct mem_acc_regulator *mem_acc_vreg)
{
	struct resource *res;
	int len, rc = 0;
	u32 l1_config_skip_fuse_sel[5];

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "efuse_addr");
	if (!res || !res->start) {
		mem_acc_vreg->efuse_base = NULL;
		pr_debug("'efuse_addr' resource missing or not used.\n");
		return 0;
	}

	mem_acc_vreg->efuse_addr = res->start;
	len = res->end - res->start + 1;

	pr_info("efuse_addr = %pa (len=0x%x)\n", &res->start, len);

	mem_acc_vreg->efuse_base = devm_ioremap(&pdev->dev,
						mem_acc_vreg->efuse_addr, len);
	if (!mem_acc_vreg->efuse_base) {
		pr_err("Unable to map efuse_addr %pa\n",
				&mem_acc_vreg->efuse_addr);
		return -EINVAL;
	}

	if (of_find_property(mem_acc_vreg->dev->of_node,
				"qcom,l1-config-skip-fuse-sel", NULL)) {
		rc = of_property_read_u32_array(mem_acc_vreg->dev->of_node,
					"qcom,l1-config-skip-fuse-sel",
					l1_config_skip_fuse_sel, 5);
		if (rc < 0) {
			pr_err("Read failed - qcom,l1-config-skip-fuse-sel rc=%d\n",
					rc);
			return rc;
		}

		if (mem_acc_fuse_is_setting_expected(mem_acc_vreg,
						l1_config_skip_fuse_sel)) {
			mem_acc_vreg->flags |= MEM_ACC_SKIP_L1_CONFIG;
			pr_debug("Skip L1 configuration enabled\n");
		}
	}

	return 0;
}

static int mem_acc_custom_data_init(struct platform_device *pdev,
				 struct mem_acc_regulator *mem_acc_vreg,
				 int mem_type)
{
	struct resource *res;
	char *custom_apc_addr_str, *custom_apc_data_str;
	int len, rc = 0;

	switch (mem_type) {
	case MEMORY_L1:
		custom_apc_addr_str = "acc-l1-custom";
		custom_apc_data_str = "qcom,l1-acc-custom-data";
		break;
	case MEMORY_L2:
		custom_apc_addr_str = "acc-l2-custom";
		custom_apc_data_str = "qcom,l2-acc-custom-data";
		break;
	default:
		pr_err("Invalid memory type: %d\n", mem_type);
		return -EINVAL;
	}

	if (!of_find_property(mem_acc_vreg->dev->of_node,
				custom_apc_data_str, NULL)) {
		pr_debug("%s custom_data not specified\n", custom_apc_data_str);
		return 0;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						custom_apc_addr_str);
	if (!res || !res->start) {
		pr_debug("%s resource missing\n", custom_apc_addr_str);
		return -EINVAL;
	} else {
		len = res->end - res->start + 1;
		mem_acc_vreg->acc_custom_addr[mem_type] =
			devm_ioremap(mem_acc_vreg->dev, res->start, len);
		if (!mem_acc_vreg->acc_custom_addr[mem_type]) {
			pr_err("Unable to map %s %pa\n", custom_apc_addr_str,
							&res->start);
			return -EINVAL;
		}
	}

	rc = populate_acc_data(mem_acc_vreg, custom_apc_data_str,
				&mem_acc_vreg->acc_custom_data[mem_type], &len);
	if (rc) {
		pr_err("Unable to find %s rc=%d\n", custom_apc_data_str, rc);
		return rc;
	}

	if (mem_acc_vreg->num_corners != len) {
		pr_err("Custom data is not present for all the corners\n");
		return -EINVAL;
	}

	mem_acc_vreg->mem_acc_custom_supported[mem_type] = true;

	return 0;
}

static int override_mem_acc_custom_data(struct platform_device *pdev,
				 struct mem_acc_regulator *mem_acc_vreg,
				 int mem_type)
{
	char *custom_apc_data_str;
	int len, rc = 0, i;
	int tuple_count, tuple_match;
	u32 index = 0, value = 0;

	switch (mem_type) {
	case MEMORY_L1:
		custom_apc_data_str = "qcom,override-l1-acc-custom-data";
		break;
	case MEMORY_L2:
		custom_apc_data_str = "qcom,override-l2-acc-custom-data";
		break;
	default:
		pr_err("Invalid memory type: %d\n", mem_type);
		return -EINVAL;
	}

	if (!of_find_property(mem_acc_vreg->dev->of_node,
				custom_apc_data_str, &len)) {
		pr_debug("%s not specified\n", custom_apc_data_str);
		return 0;
	}

	if (mem_acc_vreg->override_map_count) {
		if (mem_acc_vreg->override_map_match == FUSE_MAP_NO_MATCH)
			return 0;
		tuple_count = mem_acc_vreg->override_map_count;
		tuple_match = mem_acc_vreg->override_map_match;
	} else {
		tuple_count = 1;
		tuple_match = 0;
	}

	if (len != mem_acc_vreg->num_corners * tuple_count * sizeof(u32)) {
		pr_err("%s length=%d is invalid\n", custom_apc_data_str, len);
		return -EINVAL;
	}

	for (i = 0; i < mem_acc_vreg->num_corners; i++) {
		index = (tuple_match * mem_acc_vreg->num_corners) + i;
		rc = of_property_read_u32_index(mem_acc_vreg->dev->of_node,
					custom_apc_data_str, index, &value);
		if (rc) {
			pr_err("Unable read %s index %u, rc=%d\n",
					custom_apc_data_str, index, rc);
			return rc;
		}
		mem_acc_vreg->acc_custom_data[mem_type][i] = value;
	}

	return 0;
}

static int mem_acc_override_corner_map(struct mem_acc_regulator *mem_acc_vreg)
{
	int len = 0, i, rc;
	int tuple_count, tuple_match;
	u32 index = 0, value = 0;
	char *prop_str = "qcom,override-corner-acc-map";

	if (!of_find_property(mem_acc_vreg->dev->of_node, prop_str, &len))
		return 0;

	if (mem_acc_vreg->override_map_count) {
		if (mem_acc_vreg->override_map_match ==	FUSE_MAP_NO_MATCH)
			return 0;
		tuple_count = mem_acc_vreg->override_map_count;
		tuple_match = mem_acc_vreg->override_map_match;
	} else {
		tuple_count = 1;
		tuple_match = 0;
	}

	if (len != mem_acc_vreg->num_corners * tuple_count * sizeof(u32)) {
		pr_err("%s length=%d is invalid\n", prop_str, len);
		return -EINVAL;
	}

	for (i = 0; i < mem_acc_vreg->num_corners; i++) {
		index = (tuple_match * mem_acc_vreg->num_corners) + i;
		rc = of_property_read_u32_index(mem_acc_vreg->dev->of_node,
						prop_str, index, &value);
		if (rc) {
			pr_err("Unable read %s index %u, rc=%d\n",
						prop_str, index, rc);
			return rc;
		}
		mem_acc_vreg->corner_acc_map[i] = value;
	}

	return 0;

}

static int mem_acc_find_override_map_match(struct platform_device *pdev,
				 struct mem_acc_regulator *mem_acc_vreg)
{
	struct device_node *of_node = pdev->dev.of_node;
	int i, rc, tuple_size;
	int len = 0;
	u32 *tmp;
	char *prop_str = "qcom,override-fuse-version-map";

	/* Specify default no match case. */
	mem_acc_vreg->override_map_match = FUSE_MAP_NO_MATCH;
	mem_acc_vreg->override_map_count = 0;

	if (!of_find_property(of_node, prop_str, &len)) {
		/* No mapping present. */
		return 0;
	}

	tuple_size = 1;
	mem_acc_vreg->override_map_count = len / (sizeof(u32) * tuple_size);

	if (len == 0 || len % (sizeof(u32) * tuple_size)) {
		pr_err("%s length=%d is invalid\n", prop_str, len);
		return -EINVAL;
	}

	tmp = kzalloc(len, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	rc = of_property_read_u32_array(of_node, prop_str, tmp,
			mem_acc_vreg->override_map_count * tuple_size);
	if (rc) {
		pr_err("could not read %s rc=%d\n", prop_str, rc);
		goto done;
	}

	for (i = 0; i < mem_acc_vreg->override_map_count; i++) {
		if (tmp[i * tuple_size] != mem_acc_vreg->override_fuse_value
		    && tmp[i * tuple_size] != FUSE_PARAM_MATCH_ANY) {
			continue;
		} else {
			mem_acc_vreg->override_map_match = i;
			break;
		}
	}

	if (mem_acc_vreg->override_map_match != FUSE_MAP_NO_MATCH)
		pr_debug("%s tuple match found: %d\n", prop_str,
				mem_acc_vreg->override_map_match);
	else
		pr_err("%s tuple match not found\n", prop_str);

done:
	kfree(tmp);
	return rc;
}

#define MEM_TYPE_STRING_LEN	20
static int mem_acc_init(struct platform_device *pdev,
		struct mem_acc_regulator *mem_acc_vreg)
{
	struct resource *res;
	int len, rc, i, j;
	u32 fuse_sel[4];
	u64 fuse_bits;
	bool acc_type_present = false;
	char tmps[MEM_TYPE_STRING_LEN];

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "acc-en");
	if (!res || !res->start) {
		pr_debug("'acc-en' resource missing or not used.\n");
	} else {
		mem_acc_vreg->acc_en_addr = res->start;
		len = res->end - res->start + 1;
		pr_debug("'acc_en_addr' = %pa (len=0x%x)\n", &res->start, len);

		mem_acc_vreg->acc_en_base = devm_ioremap(mem_acc_vreg->dev,
				mem_acc_vreg->acc_en_addr, len);
		if (!mem_acc_vreg->acc_en_base) {
			pr_err("Unable to map 'acc_en_addr' %pa\n",
					&mem_acc_vreg->acc_en_addr);
			return -EINVAL;
		}

		rc = populate_acc_data(mem_acc_vreg, "qcom,acc-en-bit-pos",
				&mem_acc_vreg->acc_en_bit_pos,
				&mem_acc_vreg->num_acc_en);
		if (rc) {
			pr_err("Unable to populate 'qcom,acc-en-bit-pos' rc=%d\n",
					rc);
			return rc;
		}
	}

	rc = mem_acc_efuse_init(pdev, mem_acc_vreg);
	if (rc) {
		pr_err("Wrong eFuse address specified: rc=%d\n", rc);
		return rc;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "acc-sel-l1");
	if (!res || !res->start) {
		pr_debug("'acc-sel-l1' resource missing or not used.\n");
	} else {
		rc = mem_acc_sel_setup(mem_acc_vreg, res, MEMORY_L1);
		if (rc) {
			pr_err("Unable to setup mem-acc for mem_type=%d rc=%d\n",
					MEMORY_L1, rc);
			return rc;
		}
		mem_acc_vreg->mem_acc_supported[MEMORY_L1] = true;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "acc-sel-l2");
	if (!res || !res->start) {
		pr_debug("'acc-sel-l2' resource missing or not used.\n");
	} else {
		rc = mem_acc_sel_setup(mem_acc_vreg, res, MEMORY_L2);
		if (rc) {
			pr_err("Unable to setup mem-acc for mem_type=%d rc=%d\n",
					MEMORY_L2, rc);
			return rc;
		}
		mem_acc_vreg->mem_acc_supported[MEMORY_L2] = true;
	}

	for (i = 0; i < MEM_ACC_TYPE_MAX; i++) {
		snprintf(tmps, MEM_TYPE_STRING_LEN, "mem-acc-type%d", i + 1);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, tmps);

		if (!res || !res->start) {
			pr_debug("'%s' resource missing or not used.\n", tmps);
		} else {
			mem_acc_vreg->mem_acc_type_addr[i] = res->start;
			acc_type_present = true;
		}
	}

	rc = populate_acc_data(mem_acc_vreg, "qcom,corner-acc-map",
			&mem_acc_vreg->corner_acc_map,
			&mem_acc_vreg->num_corners);
	if (rc) {
		pr_err("Unable to find 'qcom,corner-acc-map' rc=%d\n", rc);
		return rc;
	}

	pr_debug("num_corners = %d\n", mem_acc_vreg->num_corners);

	/* Check if at least one valid mem-acc config. is specified */
	for (i = 0; i < MEMORY_MAX; i++) {
		if (mem_acc_vreg->mem_acc_supported[i])
			break;
	}
	if (i == MEMORY_MAX && !acc_type_present) {
		pr_err("No mem-acc configuration specified\n");
		return -EINVAL;
	}

	if (mem_acc_vreg->num_acc_en)
		mem_acc_en_init(mem_acc_vreg);

	rc = mem_acc_sel_init(mem_acc_vreg);
	if (rc) {
		pr_err("Unable to intialize mem_acc_sel reg rc=%d\n", rc);
		return rc;
	}

	for (i = 0; i < MEMORY_MAX; i++) {
		rc = mem_acc_custom_data_init(pdev, mem_acc_vreg, i);
		if (rc) {
			pr_err("Unable to initialize custom data for mem_type=%d rc=%d\n",
					i, rc);
			return rc;
		}
	}

	if (of_find_property(mem_acc_vreg->dev->of_node,
				"qcom,override-acc-fuse-sel", NULL)) {
		rc = of_property_read_u32_array(mem_acc_vreg->dev->of_node,
			"qcom,override-acc-fuse-sel", fuse_sel, 4);
		if (rc < 0) {
			pr_err("Read failed - qcom,override-acc-fuse-sel rc=%d\n",
					rc);
			return rc;
		}

		fuse_bits = mem_acc_read_efuse_row(mem_acc_vreg, fuse_sel[0],
								fuse_sel[3]);
		/*
		 * fuse_sel[1] = LSB position in row (shift)
		 * fuse_sel[2] = num of bits (mask)
		 */
		mem_acc_vreg->override_fuse_value = (fuse_bits >> fuse_sel[1]) &
						((1 << fuse_sel[2]) - 1);

		rc = mem_acc_find_override_map_match(pdev, mem_acc_vreg);
		if (rc) {
			pr_err("Unable to find fuse map match rc=%d\n", rc);
			return rc;
		}

		pr_debug("override_fuse_val=%d override_map_match=%d\n",
					mem_acc_vreg->override_fuse_value,
					mem_acc_vreg->override_map_match);

		rc = mem_acc_override_corner_map(mem_acc_vreg);
		if (rc) {
			pr_err("Unable to override corner map rc=%d\n", rc);
			return rc;
		}

		for (i = 0; i < MEMORY_MAX; i++) {
			rc = override_mem_acc_custom_data(pdev,
							mem_acc_vreg, i);
			if (rc) {
				pr_err("Unable to override custom data for mem_type=%d rc=%d\n",
					i, rc);
				return rc;
			}
		}
	}

	if (acc_type_present) {
		mem_acc_vreg->mem_acc_type_data = devm_kzalloc(
			mem_acc_vreg->dev, mem_acc_vreg->num_corners *
			MEM_ACC_TYPE_MAX * sizeof(u32), GFP_KERNEL);

		if (!mem_acc_vreg->mem_acc_type_data) {
			pr_err("Unable to allocate memory for mem_acc_type\n");
			return -ENOMEM;
		}

		for (i = 0; i < MEM_ACC_TYPE_MAX; i++) {
			if (mem_acc_vreg->mem_acc_type_addr[i]) {
				snprintf(tmps, MEM_TYPE_STRING_LEN,
					"qcom,mem-acc-type%d", i + 1);

				j = i * mem_acc_vreg->num_corners;
				rc = of_property_read_u32_array(
					mem_acc_vreg->dev->of_node,
					tmps,
					&mem_acc_vreg->mem_acc_type_data[j],
					mem_acc_vreg->num_corners);
				if (rc) {
					pr_err("Unable to get property %s rc=%d\n",
						tmps, rc);
					return rc;
				}
			}
		}
	}

	return 0;
}

static int mem_acc_regulator_probe(struct platform_device *pdev)
{
	struct regulator_config reg_config = {};
	struct mem_acc_regulator *mem_acc_vreg;
	struct regulator_desc *rdesc;
	struct regulator_init_data *init_data;
	int rc;

	if (!pdev->dev.of_node) {
		pr_err("Device tree node is missing\n");
		return -EINVAL;
	}

	init_data = of_get_regulator_init_data(&pdev->dev, pdev->dev.of_node);
	if (!init_data) {
		pr_err("regulator init data is missing\n");
		return -EINVAL;
	} else {
		init_data->constraints.input_uV
			= init_data->constraints.max_uV;
		init_data->constraints.valid_ops_mask
			|= REGULATOR_CHANGE_VOLTAGE;
	}

	mem_acc_vreg = devm_kzalloc(&pdev->dev, sizeof(*mem_acc_vreg),
			GFP_KERNEL);
	if (!mem_acc_vreg) {
		pr_err("Can't allocate mem_acc_vreg memory\n");
		return -ENOMEM;
	}
	mem_acc_vreg->dev = &pdev->dev;

	rc = mem_acc_init(pdev, mem_acc_vreg);
	if (rc) {
		pr_err("Unable to initialize mem_acc configuration rc=%d\n",
				rc);
		return rc;
	}

	rdesc			= &mem_acc_vreg->rdesc;
	rdesc->owner		= THIS_MODULE;
	rdesc->type		= REGULATOR_VOLTAGE;
	rdesc->ops		= &mem_acc_corner_ops;
	rdesc->name		= init_data->constraints.name;

	reg_config.dev = &pdev->dev;
	reg_config.init_data = init_data;
	reg_config.driver_data = mem_acc_vreg;
	reg_config.of_node = pdev->dev.of_node;
	mem_acc_vreg->rdev = regulator_register(rdesc, &reg_config);
	if (IS_ERR(mem_acc_vreg->rdev)) {
		rc = PTR_ERR(mem_acc_vreg->rdev);
		if (rc != -EPROBE_DEFER)
			pr_err("regulator_register failed: rc=%d\n", rc);
		return rc;
	}

	platform_set_drvdata(pdev, mem_acc_vreg);

	return 0;
}

static int mem_acc_regulator_remove(struct platform_device *pdev)
{
	struct mem_acc_regulator *mem_acc_vreg = platform_get_drvdata(pdev);

	regulator_unregister(mem_acc_vreg->rdev);

	return 0;
}

static struct of_device_id mem_acc_regulator_match_table[] = {
	{ .compatible = "qcom,mem-acc-regulator", },
	{}
};

static struct platform_driver mem_acc_regulator_driver = {
	.probe		= mem_acc_regulator_probe,
	.remove		= mem_acc_regulator_remove,
	.driver		= {
		.name		= "qcom,mem-acc-regulator",
		.of_match_table = mem_acc_regulator_match_table,
		.owner		= THIS_MODULE,
	},
};

int __init mem_acc_regulator_init(void)
{
	return platform_driver_register(&mem_acc_regulator_driver);
}
postcore_initcall(mem_acc_regulator_init);

static void __exit mem_acc_regulator_exit(void)
{
	platform_driver_unregister(&mem_acc_regulator_driver);
}
module_exit(mem_acc_regulator_exit);

MODULE_DESCRIPTION("MEM-ACC-SEL regulator driver");
MODULE_LICENSE("GPL v2");
