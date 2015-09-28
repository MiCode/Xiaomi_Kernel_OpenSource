/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/of.h>
#include <linux/slab.h>
#include "msm_vidc_resources.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_res_parse.h"

enum clock_properties {
	CLOCK_PROP_HAS_SCALING = 1 << 0,
	CLOCK_PROP_HAS_GATING = 1 << 1,
};

static size_t get_u32_array_num_elements(struct platform_device *pdev,
					char *name)
{
	struct device_node *np = pdev->dev.of_node;
	int len;
	size_t num_elements = 0;
	if (!of_get_property(np, name, &len)) {
		dprintk(VIDC_ERR, "Failed to read %s from device tree\n",
			name);
		goto fail_read;
	}

	num_elements = len / sizeof(u32);
	if (num_elements <= 0) {
		dprintk(VIDC_ERR, "%s not specified in device tree\n",
			name);
		goto fail_read;
	}
	return num_elements;

fail_read:
	return 0;
}

int read_hfi_type(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int rc = 0;
	const char *hfi_name = NULL;

	if (np) {
		rc = of_property_read_string(np, "qcom,hfi", &hfi_name);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to read hfi from device tree\n");
			goto err_hfi_read;
		}
		if (!strcmp(hfi_name, "venus"))
			rc = VIDC_HFI_VENUS;
		else if (!strcmp(hfi_name, "q6"))
			rc = VIDC_HFI_Q6;
		else
			rc = -EINVAL;
	} else
		rc = VIDC_HFI_Q6;

err_hfi_read:
	return rc;
}

static inline void msm_vidc_free_freq_table(
		struct msm_vidc_platform_resources *res)
{
	res->load_freq_tbl = NULL;
}

static inline void msm_vidc_free_reg_table(
			struct msm_vidc_platform_resources *res)
{
	res->reg_set.reg_tbl = NULL;
}

static inline void msm_vidc_free_qdss_addr_table(
			struct msm_vidc_platform_resources *res)
{
	res->qdss_addr_set.addr_tbl = NULL;
}

static inline void msm_vidc_free_bus_vectors(
			struct msm_vidc_platform_resources *res)
{
	int i = 0;
	for (i = 0; i < res->bus_set.count; i++) {
		if (res->bus_set.bus_tbl[i].pdata)
			msm_bus_cl_clear_pdata(res->bus_set.bus_tbl[i].pdata);
	}
}

static inline void msm_vidc_free_iommu_groups(
			struct msm_vidc_platform_resources *res)
{
	res->iommu_group_set.iommu_maps = NULL;
}

static inline void msm_vidc_free_regulator_table(
			struct msm_vidc_platform_resources *res)
{
	int c = 0;
	for (c = 0; c < res->regulator_set.count; ++c) {
		struct regulator_info *rinfo =
			&res->regulator_set.regulator_tbl[c];

		rinfo->name = NULL;
	}

	res->regulator_set.regulator_tbl = NULL;
	res->regulator_set.count = 0;
}

static inline void msm_vidc_free_clock_table(
			struct msm_vidc_platform_resources *res)
{
	res->clock_set.clock_tbl = NULL;
	res->clock_set.count = 0;
}

static inline void msm_vidc_free_clock_voltage_table(
			struct msm_vidc_platform_resources *res)
{
	res->cv_info.cv_table = NULL;
	res->cv_info.count = 0;
	res->cv_info_vp9d.cv_table = NULL;
	res->cv_info_vp9d.count = 0;
}

void msm_vidc_free_platform_resources(
			struct msm_vidc_platform_resources *res)
{
	msm_vidc_free_clock_table(res);
	msm_vidc_free_clock_voltage_table(res);
	msm_vidc_free_regulator_table(res);
	msm_vidc_free_freq_table(res);
	msm_vidc_free_reg_table(res);
	msm_vidc_free_qdss_addr_table(res);
	msm_vidc_free_bus_vectors(res);
	msm_vidc_free_iommu_groups(res);
}

static int msm_vidc_load_reg_table(struct msm_vidc_platform_resources *res)
{
	struct reg_set *reg_set;
	struct platform_device *pdev = res->pdev;
	int i;
	int rc = 0;

	if (!of_find_property(pdev->dev.of_node, "qcom,reg-presets", NULL)) {
		/* qcom,reg-presets is an optional property.  It likely won't be
		 * present if we don't have any register settings to program */
		dprintk(VIDC_DBG, "qcom,reg-presets not found\n");
		return 0;
	}

	reg_set = &res->reg_set;
	reg_set->count = get_u32_array_num_elements(pdev, "qcom,reg-presets");
	reg_set->count /=  sizeof(*reg_set->reg_tbl) / sizeof(u32);

	if (reg_set->count == 0) {
		dprintk(VIDC_DBG, "no elements in reg set\n");
		return rc;
	}

	reg_set->reg_tbl = devm_kzalloc(&pdev->dev, reg_set->count *
			sizeof(*(reg_set->reg_tbl)), GFP_KERNEL);
	if (!reg_set->reg_tbl) {
		dprintk(VIDC_ERR, "%s Failed to alloc register table\n",
			__func__);
		return -ENOMEM;
	}

	if (of_property_read_u32_array(pdev->dev.of_node, "qcom,reg-presets",
		(u32 *)reg_set->reg_tbl, reg_set->count * 2)) {
		dprintk(VIDC_ERR, "Failed to read register table\n");
		msm_vidc_free_reg_table(res);
		return -EINVAL;
	}
	for (i = 0; i < reg_set->count; i++) {
		dprintk(VIDC_DBG,
			"reg = %x, value = %x\n",
			reg_set->reg_tbl[i].reg,
			reg_set->reg_tbl[i].value
		);
	}
	return rc;
}
static int msm_vidc_load_qdss_table(struct msm_vidc_platform_resources *res)
{
	struct addr_set *qdss_addr_set;
	struct platform_device *pdev = res->pdev;
	int i;
	int rc = 0;

	if (!of_find_property(pdev->dev.of_node, "qcom,qdss-presets", NULL)) {
		/* qcom,qdss-presets is an optional property. It likely won't be
		 * present if we don't have any register settings to program */
		dprintk(VIDC_DBG, "qcom,qdss-presets not found\n");
		return rc;
	}

	qdss_addr_set = &res->qdss_addr_set;
	qdss_addr_set->count = get_u32_array_num_elements(pdev,
					"qcom,qdss-presets");
	qdss_addr_set->count /= sizeof(*qdss_addr_set->addr_tbl) / sizeof(u32);

	if (qdss_addr_set->count == 0) {
		dprintk(VIDC_DBG, "no elements in qdss reg set\n");
		return rc;
	}

	qdss_addr_set->addr_tbl = devm_kzalloc(&pdev->dev,
			qdss_addr_set->count * sizeof(*qdss_addr_set->addr_tbl),
			GFP_KERNEL);
	if (!qdss_addr_set->addr_tbl) {
		dprintk(VIDC_ERR, "%s Failed to alloc register table\n",
			__func__);
		rc = -ENOMEM;
		goto err_qdss_addr_tbl;
	}

	rc = of_property_read_u32_array(pdev->dev.of_node, "qcom,qdss-presets",
		(u32 *)qdss_addr_set->addr_tbl, qdss_addr_set->count * 2);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to read qdss address table\n");
		msm_vidc_free_qdss_addr_table(res);
		rc = -EINVAL;
		goto err_qdss_addr_tbl;
	}

	for (i = 0; i < qdss_addr_set->count; i++) {
		dprintk(VIDC_DBG, "qdss addr = %x, value = %x\n",
				qdss_addr_set->addr_tbl[i].start,
				qdss_addr_set->addr_tbl[i].size);
	}
err_qdss_addr_tbl:
	return rc;
}

static int msm_vidc_load_freq_table(struct msm_vidc_platform_resources *res)
{
	int rc = 0;
	int num_elements = 0;
	struct platform_device *pdev = res->pdev;

	if (!of_find_property(pdev->dev.of_node, "qcom,load-freq-tbl", NULL)) {
		/* qcom,load-freq-tbl is an optional property.  It likely won't
		 * be present on cores that we can't clock scale on. */
		dprintk(VIDC_DBG, "qcom,load-freq-tbl not found\n");
		return 0;
	}

	num_elements = get_u32_array_num_elements(pdev, "qcom,load-freq-tbl");
	num_elements /= sizeof(*res->load_freq_tbl) / sizeof(u32);
	if (num_elements == 0) {
		dprintk(VIDC_ERR, "no elements in frequency table\n");
		return rc;
	}

	res->load_freq_tbl = devm_kzalloc(&pdev->dev, num_elements *
			sizeof(*res->load_freq_tbl), GFP_KERNEL);
	if (!res->load_freq_tbl) {
		dprintk(VIDC_ERR,
				"%s Failed to alloc load_freq_tbl\n",
				__func__);
		return -ENOMEM;
	}

	if (of_property_read_u32_array(pdev->dev.of_node,
		"qcom,load-freq-tbl", (u32 *)res->load_freq_tbl,
		num_elements * sizeof(*res->load_freq_tbl) / sizeof(u32))) {
		dprintk(VIDC_ERR, "Failed to read frequency table\n");
		msm_vidc_free_freq_table(res);
		return -EINVAL;
	}

	res->load_freq_tbl_size = num_elements;
	return rc;
}

static int msm_vidc_load_bus_vectors(struct msm_vidc_platform_resources *res)
{
	struct platform_device *pdev = res->pdev;
	struct device_node *child_node, *bus_node;
	struct bus_set *buses = &res->bus_set;
	int rc = 0, c = 0;
	u32 num_buses = 0;

	bus_node = of_find_node_by_name(pdev->dev.of_node,
			"qcom,msm-bus-clients");
	if (!bus_node) {
		/* Not a required property */
		dprintk(VIDC_DBG, "qcom,msm-bus-clients not found\n");
		rc = 0;
		goto err_bad_node;
	}

	for_each_child_of_node(bus_node, child_node)
		++num_buses;

	buses->bus_tbl = devm_kzalloc(&pdev->dev, sizeof(*buses->bus_tbl) *
			num_buses, GFP_KERNEL);
	if (!buses->bus_tbl) {
		dprintk(VIDC_ERR, "%s: Failed to allocate memory\n", __func__);
		rc = -ENOMEM;
		goto err_bad_node;
	}

	buses->count = num_buses;
	c = 0;

	for_each_child_of_node(bus_node, child_node) {
		bool passive = false;
		bool low_power = false;
		bool low_latency = false;
		u32 configs = 0;
		struct bus_info *bus = &buses->bus_tbl[c];

		passive = of_property_read_bool(child_node, "qcom,bus-passive");
		low_power = of_property_read_bool(child_node,
			"qcom,bus-low-power");
		low_latency = of_property_read_bool(child_node,
			"qcom,bus-low-latency");
		rc = of_property_read_u32(child_node, "qcom,bus-configs",
				&configs);
		if (rc) {
			dprintk(VIDC_ERR,
					"Failed to read qcom,bus-configs in %s: %d\n",
					child_node->name, rc);
			break;
		}
		if (low_power)
			bus->power_mode = VIDC_POWER_LOW;
		else if (low_latency)
			bus->power_mode = VIDC_POWER_LOW_LATENCY;
		else
			bus->power_mode = VIDC_POWER_NORMAL;
		bus->passive = passive;
		bus->sessions_supported = configs;
		bus->pdata = msm_bus_pdata_from_node(pdev, child_node);
		if (IS_ERR_OR_NULL(bus->pdata)) {
			rc = PTR_ERR(bus->pdata) ?: -EBADHANDLE;
			dprintk(VIDC_ERR, "Failed to get bus pdata: %d\n", rc);
			break;
		}
		res->power_modes |= bus->power_mode;

		dprintk(VIDC_DBG,
				"Bus %s supports: %x, passive: %d, power_mode: %d\n",
				bus->pdata->name, bus->sessions_supported,
				passive, bus->power_mode);
		++c;
	}

	if (c < num_buses) {
		for (c--; c >= 0; c--)
			msm_bus_cl_clear_pdata(buses->bus_tbl[c].pdata);

		goto err_bad_node;
	}

err_bad_node:
	return rc;
}

static int msm_vidc_load_iommu_groups(struct msm_vidc_platform_resources *res)
{
	int rc = 0;
	struct platform_device *pdev = res->pdev;
	struct device_node *domains_parent_node = NULL;
	struct device_node *domains_child_node = NULL;
	struct iommu_set *iommu_group_set = &res->iommu_group_set;
	int domain_idx = 0;
	struct iommu_info *iommu_map;
	int array_size = 0;

	domains_parent_node = of_find_node_by_name(pdev->dev.of_node,
				"qcom,vidc-iommu-domains");
	if (!domains_parent_node) {
		dprintk(VIDC_DBG, "Node qcom,vidc-iommu-domains not found.\n");
		return 0;
	}

	iommu_group_set->count = 0;
	for_each_child_of_node(domains_parent_node, domains_child_node) {
		iommu_group_set->count++;
	}

	if (iommu_group_set->count == 0) {
		dprintk(VIDC_ERR, "No group present in iommu_domains\n");
		rc = -ENOENT;
		goto err_no_of_node;
	}
	iommu_group_set->iommu_maps = devm_kzalloc(&pdev->dev,
			iommu_group_set->count *
			sizeof(*iommu_group_set->iommu_maps), GFP_KERNEL);

	if (!iommu_group_set->iommu_maps) {
		dprintk(VIDC_ERR, "Cannot allocate iommu_maps\n");
		rc = -ENOMEM;
		goto err_no_of_node;
	}

	/* set up each context bank */
	for_each_child_of_node(domains_parent_node, domains_child_node) {
		struct device_node *ctx_node = of_parse_phandle(
						domains_child_node,
						"qcom,vidc-domain-phandle",
						0);
		if (domain_idx >= iommu_group_set->count)
			break;

		iommu_map = &iommu_group_set->iommu_maps[domain_idx];
		if (!ctx_node) {
			dprintk(VIDC_ERR, "Unable to parse pHandle\n");
			rc = -EBADHANDLE;
			goto err_load_groups;
		}

		/* domain info from domains.dtsi */
		rc = of_property_read_string(ctx_node, "label",
				&(iommu_map->name));
		if (rc) {
			dprintk(VIDC_ERR, "Could not find label property\n");
			goto err_load_groups;
		}

		dprintk(VIDC_DBG,
				"domain %d has name %s\n",
				domain_idx,
				iommu_map->name);

		if (!of_get_property(ctx_node, "qcom,virtual-addr-pool",
				&array_size)) {
			dprintk(VIDC_ERR,
				"Could not find any addr pool for group : %s\n",
				iommu_map->name);
			rc = -EBADHANDLE;
			goto err_load_groups;
		}

		iommu_map->npartitions = array_size / sizeof(u32) / 2;

		dprintk(VIDC_DBG,
				"%d partitions in domain %d",
				iommu_map->npartitions,
				domain_idx);

		rc = of_property_read_u32_array(ctx_node,
				"qcom,virtual-addr-pool",
				(u32 *)iommu_map->addr_range,
				iommu_map->npartitions * 2);
		if (rc) {
			dprintk(VIDC_ERR,
				"Could not read addr pool for group : %s (%d)\n",
				iommu_map->name,
				rc);
			goto err_load_groups;
		}

		iommu_map->is_secure =
			of_property_read_bool(ctx_node,	"qcom,secure-domain");

		dprintk(VIDC_DBG,
				"domain %s : secure = %d\n",
				iommu_map->name,
				iommu_map->is_secure);

		/* setup partitions and buffer type per partition */
		rc = of_property_read_u32_array(domains_child_node,
				"qcom,vidc-partition-buffer-types",
				iommu_map->buffer_type,
				iommu_map->npartitions);

		if (rc) {
			dprintk(VIDC_ERR,
					"cannot load partition buffertype information (%d)\n",
					rc);
			rc = -ENOENT;
			goto err_load_groups;
		}
		domain_idx++;
	}
	return rc;
err_load_groups:
	msm_vidc_free_iommu_groups(res);
err_no_of_node:
	return rc;
}

static int msm_vidc_load_regulator_table(
		struct msm_vidc_platform_resources *res)
{
	int rc = 0;
	struct platform_device *pdev = res->pdev;
	struct regulator_set *regulators = &res->regulator_set;
	struct device_node *domains_parent_node = NULL;
	struct property *domains_property = NULL;
	int reg_count = 0;

	regulators->count = 0;
	regulators->regulator_tbl = NULL;

	domains_parent_node = pdev->dev.of_node;
	for_each_property_of_node(domains_parent_node, domains_property) {
		const char *search_string = "-supply";
		char *supply;
		bool matched = false;

		/* check if current property is possibly a regulator */
		supply = strnstr(domains_property->name, search_string,
				strlen(domains_property->name) + 1);
		matched = supply && (*(supply + strlen(search_string)) == '\0');
		if (!matched)
			continue;

		reg_count++;
	}

	regulators->regulator_tbl = devm_kzalloc(&pdev->dev,
			sizeof(*regulators->regulator_tbl) *
			reg_count, GFP_KERNEL);

	if (!regulators->regulator_tbl) {
		rc = -ENOMEM;
		dprintk(VIDC_ERR,
			"Failed to alloc memory for regulator table\n");
		goto err_reg_tbl_alloc;
	}

	for_each_property_of_node(domains_parent_node, domains_property) {
		const char *search_string = "-supply";
		char *supply;
		bool matched = false;
		struct device_node *regulator_node = NULL;
		struct regulator_info *rinfo = NULL;

		/* check if current property is possibly a regulator */
		supply = strnstr(domains_property->name, search_string,
				strlen(domains_property->name) + 1);
		matched = supply && (supply[strlen(search_string)] == '\0');
		if (!matched)
			continue;

		/* make sure prop isn't being misused */
		regulator_node = of_parse_phandle(domains_parent_node,
				domains_property->name, 0);
		if (IS_ERR(regulator_node)) {
			dprintk(VIDC_WARN, "%s is not a phandle\n",
					domains_property->name);
			continue;
		}
		regulators->count++;

		/* populate regulator info */
		rinfo = &regulators->regulator_tbl[regulators->count - 1];
		rinfo->name = devm_kzalloc(&pdev->dev,
			(supply - domains_property->name) + 1, GFP_KERNEL);
		if (!rinfo->name) {
			rc = -ENOMEM;
			dprintk(VIDC_ERR,
					"Failed to alloc memory for regulator name\n");
			goto err_reg_name_alloc;
		}
		strlcpy(rinfo->name, domains_property->name,
			(supply - domains_property->name) + 1);

		rinfo->has_hw_power_collapse = of_property_read_bool(
			regulator_node, "qcom,support-hw-trigger");

		dprintk(VIDC_DBG, "Found regulator %s: h/w collapse = %s\n",
				rinfo->name,
				rinfo->has_hw_power_collapse ? "yes" : "no");
	}

	if (!regulators->count)
		dprintk(VIDC_DBG, "No regulators found");

	return 0;

err_reg_name_alloc:
err_reg_tbl_alloc:
	msm_vidc_free_regulator_table(res);
	return rc;
}

static int msm_vidc_load_clock_table(
		struct msm_vidc_platform_resources *res)
{
	int rc = 0, num_clocks = 0, c = 0;
	struct platform_device *pdev = res->pdev;
	int *clock_props = NULL;
	struct clock_set *clocks = &res->clock_set;

	num_clocks = of_property_count_strings(pdev->dev.of_node,
				"clock-names");
	if (num_clocks <= 0) {
		/* Devices such as Q6 might not have any control over clocks
		 * hence have none specified, which is ok. */
		dprintk(VIDC_DBG, "No clocks found\n");
		clocks->count = 0;
		rc = 0;
		goto err_load_clk_table_fail;
	}

	clock_props = devm_kzalloc(&pdev->dev, num_clocks *
			sizeof(*clock_props), GFP_KERNEL);
	if (!clock_props) {
		dprintk(VIDC_ERR, "No memory to read clock properties\n");
		rc = -ENOMEM;
		goto err_load_clk_table_fail;
	}

	rc = of_property_read_u32_array(pdev->dev.of_node,
				"qcom,clock-configs", clock_props,
				num_clocks);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to read clock properties: %d\n", rc);
		goto err_load_clk_prop_fail;
	}

	clocks->clock_tbl = devm_kzalloc(&pdev->dev, sizeof(*clocks->clock_tbl)
			* num_clocks, GFP_KERNEL);
	if (!clocks->clock_tbl) {
		dprintk(VIDC_ERR, "Failed to allocate memory for clock tbl\n");
		rc = -ENOMEM;
		goto err_load_clk_prop_fail;
	}

	clocks->count = num_clocks;
	dprintk(VIDC_DBG, "Found %d clocks\n", num_clocks);

	for (c = 0; c < num_clocks; ++c) {
		struct clock_info *vc = &res->clock_set.clock_tbl[c];

		of_property_read_string_index(pdev->dev.of_node,
				"clock-names", c, &vc->name);

		if (clock_props[c] & CLOCK_PROP_HAS_SCALING) {
			vc->count = res->load_freq_tbl_size;
			vc->load_freq_tbl = res->load_freq_tbl;
		} else {
			vc->count = 0;
			vc->load_freq_tbl = NULL;
		}

		vc->has_gating = !!(clock_props[c] & CLOCK_PROP_HAS_GATING);

		dprintk(VIDC_DBG,
			"Found clock %s: scale-able = %s, gate-able = %s\n",
			vc->name, vc->count ? "yes" : "no",
			vc->has_gating ? "yes" : "no");
	}

	res->sw_power_collapsible = of_property_read_bool(pdev->dev.of_node,
					"qcom,sw-power-collapse");
	dprintk(VIDC_DBG, "Power collapse supported = %s\n",
		res->sw_power_collapsible ? "yes" : "no");

	res->early_fw_load = of_property_read_bool(pdev->dev.of_node,
				"qcom,early-fw-load");
	dprintk(VIDC_DBG, "Early fw load = %s\n",
				res->early_fw_load ? "yes" : "no");

	return 0;

err_load_clk_prop_fail:
err_load_clk_table_fail:
	return rc;
}

static int msm_vidc_load_clock_voltage_table(
		struct msm_vidc_platform_resources *res)
{
	int rc = 0, i = 0, num_elements = 0;
	struct platform_device *pdev = res->pdev;
	struct clock_voltage_info *cv_info = &res->cv_info;
	struct clock_voltage_info *cv_info_vp9d = &res->cv_info_vp9d;
	int *cv_table = NULL;
	bool reset_clock_control = false;
	bool regulator_scaling = false;

	reset_clock_control = of_property_read_bool(pdev->dev.of_node,
			"qcom,reset-clock-control");
	if (reset_clock_control)
		msm_vidc_reset_clock_control = 1;

	regulator_scaling = of_property_read_bool(pdev->dev.of_node,
			"qcom,regulator-scaling");
	if (regulator_scaling)
		msm_vidc_regulator_scaling = 1;

	num_elements = get_u32_array_num_elements(pdev,
			"qcom,clock-voltage-tbl");
	if (num_elements <= 0) {
		dprintk(VIDC_DBG,
			"No clocks and voltage elements found, numelements %d\n",
			num_elements);
		cv_info->count = 0;
		rc = 0;
		goto err_load_clk_vltg_table_fail;
	}
	cv_info->count =  num_elements /
			(sizeof(*cv_info->cv_table) / sizeof(u32));

	cv_table = devm_kzalloc(&pdev->dev,
			num_elements * sizeof(u32), GFP_KERNEL);
	if (!cv_table) {
		dprintk(VIDC_ERR, "No memory to read clock voltage tables\n");
		rc = -ENOMEM;
		goto err_load_clk_vltg_table_fail;
	}

	rc = of_property_read_u32_array(pdev->dev.of_node,
				"qcom,clock-voltage-tbl", cv_table,
				num_elements);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to read clock properties: %d\n", rc);
		goto err_load_clk_vltg_table_fail;
	}
	cv_info->cv_table = (struct clock_voltage_table *)cv_table;

	dprintk(VIDC_DBG, "%s: clock voltage table size %d\n",
		__func__, cv_info->count);
	for (i = 0; i < cv_info->count; i++) {
		dprintk(VIDC_DBG,
			"clock freq: %d, voltage index: %d\n",
			cv_info->cv_table[i].clock_freq,
			cv_info->cv_table[i].voltage_idx);
	}

	/* load vp9 decoder specific clock voltage table */
	num_elements = get_u32_array_num_elements(pdev,
			"qcom,vp9d-clock-voltage-tbl");
	if (num_elements <= 0) {
		dprintk(VIDC_DBG,
			"No vp9 clocks and voltage elements found, num elements %d\n",
			num_elements);
		cv_info_vp9d->count = 0;
		rc = 0;
		goto err_load_clk_vltg_table_fail;
	}
	cv_info_vp9d->count =  num_elements /
			(sizeof(*cv_info_vp9d->cv_table) / sizeof(u32));

	cv_table = devm_kzalloc(&pdev->dev,
			num_elements * sizeof(u32), GFP_KERNEL);
	if (!cv_table) {
		dprintk(VIDC_ERR,
			"No memory to read vp9 clock voltage tables\n");
		rc = -ENOMEM;
		goto err_load_clk_vltg_table_fail;
	}

	rc = of_property_read_u32_array(pdev->dev.of_node,
				"qcom,vp9d-clock-voltage-tbl", cv_table,
				num_elements);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to read vp9 clock properties: %d\n",
			rc);
		goto err_load_clk_vltg_table_fail;
	}
	cv_info_vp9d->cv_table = (struct clock_voltage_table *)cv_table;

	/*
	 * enable regulator scaling if vp9 decoder clock vs voltage
	 * table is available and hw fuse version = 1 which supports
	 * vp9 decoder in video hardware.
	 */
	if (cv_info_vp9d->count && vidc_driver->version == 1)
		msm_vidc_regulator_scaling = 1;

	dprintk(VIDC_DBG, "%s: vp9d clock voltage table size %d\n",
		__func__, cv_info_vp9d->count);
	for (i = 0; i < cv_info_vp9d->count; i++) {
		dprintk(VIDC_DBG,
			"vp9d clock freq: %d, voltage index: %d\n",
			cv_info_vp9d->cv_table[i].clock_freq,
			cv_info_vp9d->cv_table[i].voltage_idx);
	}

	dprintk(VIDC_DBG, "video reset clock control enabled = %s\n",
			msm_vidc_reset_clock_control ? "yes" : "no");
	dprintk(VIDC_DBG, "regulator scaling enabled = %s\n",
			msm_vidc_regulator_scaling ? "yes" : "no");

err_load_clk_vltg_table_fail:
	return rc;
}

int read_platform_resources_from_dt(
		struct msm_vidc_platform_resources *res)
{
	struct platform_device *pdev = res->pdev;
	struct resource *kres = NULL;
	int rc = 0;
	uint32_t firmware_base = 0;

	if (!pdev->dev.of_node) {
		dprintk(VIDC_ERR, "DT node not found\n");
		return -ENOENT;
	}

	res->firmware_base = (phys_addr_t)firmware_base;

	kres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	res->register_base = kres ? kres->start : -1;
	res->register_size = kres ? (kres->end + 1 - kres->start) : -1;

	kres = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	res->irq = kres ? kres->start : -1;

	of_property_read_u32(pdev->dev.of_node,
			"qcom,ocmem-size", &res->ocmem_size);

	res->dynamic_bw_update = of_property_read_bool(pdev->dev.of_node,
			"qcom,use-dynamic-bw-update");
	res->sys_idle_indicator = of_property_read_bool(pdev->dev.of_node,
			"qcom,enable-idle-indicator");

	res->thermal_mitigable =
			of_property_read_bool(pdev->dev.of_node,
			"qcom,enable-thermal-mitigation");

	rc = of_property_read_string(pdev->dev.of_node, "qcom,firmware-name",
			&res->fw_name);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to read firmware name: %d\n", rc);
		goto err_load_freq_table;
	}
	dprintk(VIDC_DBG, "Firmware filename: %s\n", res->fw_name);

	rc = msm_vidc_load_freq_table(res);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to load freq table: %d\n", rc);
		goto err_load_freq_table;
	}

	rc = msm_vidc_load_qdss_table(res);
	if (rc)
		dprintk(VIDC_WARN, "Failed to load qdss reg table: %d\n", rc);

	rc = msm_vidc_load_reg_table(res);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to load reg table: %d\n", rc);
		goto err_load_reg_table;
	}

	rc = msm_vidc_load_bus_vectors(res);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to load bus vectors: %d\n", rc);
		goto err_load_bus_vectors;
	}
	rc = msm_vidc_load_iommu_groups(res);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to load iommu groups: %d\n", rc);
		goto err_load_iommu_groups;
	}

	rc = msm_vidc_load_regulator_table(res);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to load list of regulators %d\n", rc);
		goto err_load_regulator_table;
	}

	rc = msm_vidc_load_clock_table(res);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to load clock table: %d\n", rc);
		goto err_load_clock_table;
	}

	rc = msm_vidc_load_clock_voltage_table(res);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to load clock voltage table: %d\n", rc);
		goto err_load_clock_voltage_table;
	}

	rc = of_property_read_u32(pdev->dev.of_node, "qcom,dcvs-min-load",
			&res->dcvs_min_load);
	if (rc)
		dprintk(VIDC_ERR,
			"Failed to determine dcvs min load: %d\n", rc);

	rc = of_property_read_u32(pdev->dev.of_node, "qcom,dcvs-min-mbperframe",
			&res->dcvs_min_mbperframe);
	if (rc)
		dprintk(VIDC_ERR,
			"Failed to determine dcvs min MB per frame: %d\n", rc);

	rc = of_property_read_u32(pdev->dev.of_node, "qcom,max-hw-load",
			&res->max_load);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to determine max load supported: %d\n", rc);
		goto err_load_max_hw_load;
	}

	res->use_non_secure_pil = of_property_read_bool(pdev->dev.of_node,
			"qcom,use-non-secure-pil");

	if (res->use_non_secure_pil || !is_iommu_present(res)) {
		of_property_read_u32(pdev->dev.of_node, "qcom,fw-bias",
				&firmware_base);
		res->firmware_base = (phys_addr_t)firmware_base;
		dprintk(VIDC_DBG,
				"Using fw-bias : %pa", &res->firmware_base);
	}
	return rc;
err_load_max_hw_load:
	msm_vidc_free_clock_voltage_table(res);
err_load_clock_voltage_table:
	msm_vidc_free_clock_table(res);
err_load_clock_table:
	msm_vidc_free_regulator_table(res);
err_load_regulator_table:
	msm_vidc_free_iommu_groups(res);
err_load_iommu_groups:
	msm_vidc_free_bus_vectors(res);
err_load_bus_vectors:
	msm_vidc_free_reg_table(res);
err_load_reg_table:
	msm_vidc_free_freq_table(res);
err_load_freq_table:
	return rc;
}
