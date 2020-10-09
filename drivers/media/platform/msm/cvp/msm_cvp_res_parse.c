// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/iommu.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/of_reserved_mem.h>
#include "msm_cvp_debug.h"
#include "msm_cvp_resources.h"
#include "msm_cvp_res_parse.h"
#include "soc/qcom/secure_buffer.h"

enum clock_properties {
	CLOCK_PROP_HAS_SCALING = 1 << 0,
	CLOCK_PROP_HAS_MEM_RETENTION    = 1 << 1,
};

#define PERF_GOV "performance"

static inline struct device *msm_iommu_get_ctx(const char *ctx_name)
{
	return NULL;
}

static size_t get_u32_array_num_elements(struct device_node *np,
					char *name)
{
	int len;
	size_t num_elements = 0;

	if (!of_get_property(np, name, &len)) {
		dprintk(CVP_ERR, "Failed to read %s from device tree\n",
			name);
		goto fail_read;
	}

	num_elements = len / sizeof(u32);
	if (num_elements <= 0) {
		dprintk(CVP_ERR, "%s not specified in device tree\n",
			name);
		goto fail_read;
	}
	return num_elements;

fail_read:
	return 0;
}

static inline void msm_cvp_free_allowed_clocks_table(
		struct msm_cvp_platform_resources *res)
{
	res->allowed_clks_tbl = NULL;
}

static inline void msm_cvp_free_cycles_per_mb_table(
		struct msm_cvp_platform_resources *res)
{
	res->clock_freq_tbl.clk_prof_entries = NULL;
}

static inline void msm_cvp_free_reg_table(
			struct msm_cvp_platform_resources *res)
{
	res->reg_set.reg_tbl = NULL;
}

static inline void msm_cvp_free_qdss_addr_table(
			struct msm_cvp_platform_resources *res)
{
	res->qdss_addr_set.addr_tbl = NULL;
}

static inline void msm_cvp_free_bus_vectors(
			struct msm_cvp_platform_resources *res)
{
	kfree(res->bus_set.bus_tbl);
	res->bus_set.bus_tbl = NULL;
	res->bus_set.count = 0;
}

static inline void msm_cvp_free_regulator_table(
			struct msm_cvp_platform_resources *res)
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

static inline void msm_cvp_free_clock_table(
			struct msm_cvp_platform_resources *res)
{
	res->clock_set.clock_tbl = NULL;
	res->clock_set.count = 0;
}

void msm_cvp_free_platform_resources(
			struct msm_cvp_platform_resources *res)
{
	msm_cvp_free_clock_table(res);
	msm_cvp_free_regulator_table(res);
	msm_cvp_free_allowed_clocks_table(res);
	msm_cvp_free_reg_table(res);
	msm_cvp_free_qdss_addr_table(res);
	msm_cvp_free_bus_vectors(res);
}

static int msm_cvp_load_ipcc_regs(struct msm_cvp_platform_resources *res)
{
	int ret = 0;
	unsigned int reg_config[2];
	struct platform_device *pdev = res->pdev;

	ret = of_property_read_u32_array(pdev->dev.of_node, "qcom,ipcc-reg",
				reg_config, 2);
	if (ret) {
		dprintk(CVP_ERR, "Failed to read ipcc reg: %d\n", ret);
		return ret;
	}

	res->ipcc_reg_base = reg_config[0];
	res->ipcc_reg_size = reg_config[1];

	return ret;
}

static int msm_cvp_load_reg_table(struct msm_cvp_platform_resources *res)
{
	struct reg_set *reg_set;
	struct platform_device *pdev = res->pdev;
	int i;
	int rc = 0;

	if (!of_find_property(pdev->dev.of_node, "qcom,reg-presets", NULL)) {
		/*
		 * qcom,reg-presets is an optional property.  It likely won't be
		 * present if we don't have any register settings to program
		 */
		dprintk(CVP_CORE, "qcom,reg-presets not found\n");
		return 0;
	}

	reg_set = &res->reg_set;
	reg_set->count = get_u32_array_num_elements(pdev->dev.of_node,
			"qcom,reg-presets");
	reg_set->count /=  sizeof(*reg_set->reg_tbl) / sizeof(u32);

	if (!reg_set->count) {
		dprintk(CVP_CORE, "no elements in reg set\n");
		return rc;
	}

	reg_set->reg_tbl = devm_kzalloc(&pdev->dev, reg_set->count *
			sizeof(*(reg_set->reg_tbl)), GFP_KERNEL);
	if (!reg_set->reg_tbl) {
		dprintk(CVP_ERR, "%s Failed to alloc register table\n",
			__func__);
		return -ENOMEM;
	}

	if (of_property_read_u32_array(pdev->dev.of_node, "qcom,reg-presets",
		(u32 *)reg_set->reg_tbl, reg_set->count * 2)) {
		dprintk(CVP_ERR, "Failed to read register table\n");
		msm_cvp_free_reg_table(res);
		return -EINVAL;
	}
	for (i = 0; i < reg_set->count; i++) {
		dprintk(CVP_CORE,
			"reg = %x, value = %x\n",
			reg_set->reg_tbl[i].reg,
			reg_set->reg_tbl[i].value
		);
	}
	return rc;
}
static int msm_cvp_load_qdss_table(struct msm_cvp_platform_resources *res)
{
	struct addr_set *qdss_addr_set;
	struct platform_device *pdev = res->pdev;
	int i;
	int rc = 0;

	if (!of_find_property(pdev->dev.of_node, "qcom,qdss-presets", NULL)) {
		/*
		 * qcom,qdss-presets is an optional property. It likely won't be
		 * present if we don't have any register settings to program
		 */
		dprintk(CVP_CORE, "qcom,qdss-presets not found\n");
		return rc;
	}

	qdss_addr_set = &res->qdss_addr_set;
	qdss_addr_set->count = get_u32_array_num_elements(pdev->dev.of_node,
					"qcom,qdss-presets");
	qdss_addr_set->count /= sizeof(*qdss_addr_set->addr_tbl) / sizeof(u32);

	if (!qdss_addr_set->count) {
		dprintk(CVP_CORE, "no elements in qdss reg set\n");
		return rc;
	}

	qdss_addr_set->addr_tbl = devm_kzalloc(&pdev->dev,
			qdss_addr_set->count * sizeof(*qdss_addr_set->addr_tbl),
			GFP_KERNEL);
	if (!qdss_addr_set->addr_tbl) {
		dprintk(CVP_ERR, "%s Failed to alloc register table\n",
			__func__);
		rc = -ENOMEM;
		goto err_qdss_addr_tbl;
	}

	rc = of_property_read_u32_array(pdev->dev.of_node, "qcom,qdss-presets",
		(u32 *)qdss_addr_set->addr_tbl, qdss_addr_set->count * 2);
	if (rc) {
		dprintk(CVP_ERR, "Failed to read qdss address table\n");
		msm_cvp_free_qdss_addr_table(res);
		rc = -EINVAL;
		goto err_qdss_addr_tbl;
	}

	for (i = 0; i < qdss_addr_set->count; i++) {
		dprintk(CVP_CORE, "qdss addr = %x, value = %x\n",
				qdss_addr_set->addr_tbl[i].start,
				qdss_addr_set->addr_tbl[i].size);
	}
err_qdss_addr_tbl:
	return rc;
}

static int msm_cvp_load_fw_name(struct msm_cvp_platform_resources *res)
{
	struct platform_device *pdev = res->pdev;

	return of_property_read_string_index(pdev->dev.of_node,
				"cvp,firmware-name", 0, &res->fw_name);
}

static int msm_cvp_load_subcache_info(struct msm_cvp_platform_resources *res)
{
	int rc = 0, num_subcaches = 0, c;
	struct platform_device *pdev = res->pdev;
	struct subcache_set *subcaches = &res->subcache_set;

	num_subcaches = of_property_count_strings(pdev->dev.of_node,
		"cache-slice-names");
	if (num_subcaches <= 0) {
		dprintk(CVP_CORE, "No subcaches found\n");
		goto err_load_subcache_table_fail;
	}

	subcaches->subcache_tbl = devm_kzalloc(&pdev->dev,
		sizeof(*subcaches->subcache_tbl) * num_subcaches, GFP_KERNEL);
	if (!subcaches->subcache_tbl) {
		dprintk(CVP_ERR,
			"Failed to allocate memory for subcache tbl\n");
		rc = -ENOMEM;
		goto err_load_subcache_table_fail;
	}

	subcaches->count = num_subcaches;
	dprintk(CVP_CORE, "Found %d subcaches\n", num_subcaches);

	for (c = 0; c < num_subcaches; ++c) {
		struct subcache_info *vsc = &res->subcache_set.subcache_tbl[c];

		of_property_read_string_index(pdev->dev.of_node,
			"cache-slice-names", c, &vsc->name);
	}

	res->sys_cache_present = true;

	return 0;

err_load_subcache_table_fail:
	res->sys_cache_present = false;
	subcaches->count = 0;
	subcaches->subcache_tbl = NULL;

	return rc;
}

/**
 * msm_cvp_load_u32_table() - load dtsi table entries
 * @pdev: A pointer to the platform device.
 * @of_node:      A pointer to the device node.
 * @table_name:   A pointer to the dtsi table entry name.
 * @struct_size:  The size of the structure which is nothing but
 *                a single entry in the dtsi table.
 * @table:        A pointer to the table pointer which needs to be
 *                filled by the dtsi table entries.
 * @num_elements: Number of elements pointer which needs to be filled
 *                with the number of elements in the table.
 *
 * This is a generic implementation to load single or multiple array
 * table from dtsi. The array elements should be of size equal to u32.
 *
 * Return:        Return '0' for success else appropriate error value.
 */
int msm_cvp_load_u32_table(struct platform_device *pdev,
		struct device_node *of_node, char *table_name, int struct_size,
		u32 **table, u32 *num_elements)
{
	int rc = 0, num_elemts = 0;
	u32 *ptbl = NULL;

	if (!of_find_property(of_node, table_name, NULL)) {
		dprintk(CVP_CORE, "%s not found\n", table_name);
		return 0;
	}

	num_elemts = get_u32_array_num_elements(of_node, table_name);
	if (!num_elemts) {
		dprintk(CVP_ERR, "no elements in %s\n", table_name);
		return 0;
	}
	num_elemts /= struct_size / sizeof(u32);

	ptbl = devm_kzalloc(&pdev->dev, num_elemts * struct_size, GFP_KERNEL);
	if (!ptbl) {
		dprintk(CVP_ERR, "Failed to alloc table %s\n", table_name);
		return -ENOMEM;
	}

	if (of_property_read_u32_array(of_node, table_name, ptbl,
			num_elemts * struct_size / sizeof(u32))) {
		dprintk(CVP_ERR, "Failed to read %s\n", table_name);
		return -EINVAL;
	}

	*table = ptbl;
	if (num_elements)
		*num_elements = num_elemts;

	return rc;
}
EXPORT_SYMBOL(msm_cvp_load_u32_table);

/* A comparator to compare loads (needed later on) */
static int cmp(const void *a, const void *b)
{
	return ((struct allowed_clock_rates_table *)a)->clock_rate -
		((struct allowed_clock_rates_table *)b)->clock_rate;
}

static int msm_cvp_load_allowed_clocks_table(
		struct msm_cvp_platform_resources *res)
{
	int rc = 0;
	struct platform_device *pdev = res->pdev;

	if (!of_find_property(pdev->dev.of_node,
			"qcom,allowed-clock-rates", NULL)) {
		dprintk(CVP_CORE, "qcom,allowed-clock-rates not found\n");
		return 0;
	}

	rc = msm_cvp_load_u32_table(pdev, pdev->dev.of_node,
				"qcom,allowed-clock-rates",
				sizeof(*res->allowed_clks_tbl),
				(u32 **)&res->allowed_clks_tbl,
				&res->allowed_clks_tbl_size);
	if (rc) {
		dprintk(CVP_ERR,
			"%s: failed to read allowed clocks table\n", __func__);
		return rc;
	}

	sort(res->allowed_clks_tbl, res->allowed_clks_tbl_size,
		 sizeof(*res->allowed_clks_tbl), cmp, NULL);

	return 0;
}

static int msm_cvp_populate_mem_cdsp(struct device *dev,
		struct msm_cvp_platform_resources *res)
{
	struct device_node *mem_node;
	int ret;

	mem_node = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (mem_node) {
		ret = of_reserved_mem_device_init_by_idx(dev,
				dev->of_node, 0);
		of_node_put(dev->of_node);
		if (ret) {
			dprintk(CVP_ERR,
				"Failed to initialize reserved mem, ret %d\n",
				ret);
			return ret;
		}
	}
	res->mem_cdsp.dev = dev;

	return 0;
}

static int msm_cvp_populate_bus(struct device *dev,
		struct msm_cvp_platform_resources *res)
{
	struct bus_set *buses = &res->bus_set;
	const char *temp_name = NULL;
	struct bus_info *bus = NULL, *temp_table;
	u32 range[2];
	int rc = 0;

	temp_table = krealloc(buses->bus_tbl, sizeof(*temp_table) *
			(buses->count + 1), GFP_KERNEL);
	if (!temp_table) {
		dprintk(CVP_ERR, "%s: Failed to allocate memory", __func__);
		rc = -ENOMEM;
		goto err_bus;
	}

	buses->bus_tbl = temp_table;
	bus = &buses->bus_tbl[buses->count];

	memset(bus, 0x0, sizeof(struct bus_info));

	rc = of_property_read_string(dev->of_node, "label", &temp_name);
	if (rc) {
		dprintk(CVP_ERR, "'label' not found in node\n");
		goto err_bus;
	}
	/* need a non-const version of name, hence copying it over */
	bus->name = devm_kstrdup(dev, temp_name, GFP_KERNEL);
	if (!bus->name) {
		rc = -ENOMEM;
		goto err_bus;
	}

	rc = of_property_read_u32(dev->of_node, "qcom,bus-master",
			&bus->master);
	if (rc) {
		dprintk(CVP_ERR, "'qcom,bus-master' not found in node\n");
		goto err_bus;
	}

	rc = of_property_read_u32(dev->of_node, "qcom,bus-slave", &bus->slave);
	if (rc) {
		dprintk(CVP_ERR, "'qcom,bus-slave' not found in node\n");
		goto err_bus;
	}

	rc = of_property_read_string(dev->of_node, "qcom,bus-governor",
			&bus->governor);
	if (rc) {
		rc = 0;
		dprintk(CVP_CORE,
				"'qcom,bus-governor' not found, default to performance governor\n");
		bus->governor = PERF_GOV;
	}

	if (!strcmp(bus->governor, PERF_GOV))
		bus->is_prfm_gov_used = true;

	rc = of_property_read_u32_array(dev->of_node, "qcom,bus-range-kbps",
			range, ARRAY_SIZE(range));
	if (rc) {
		rc = 0;
		dprintk(CVP_CORE,
				"'qcom,range' not found defaulting to <0 INT_MAX>\n");
		range[0] = 0;
		range[1] = INT_MAX;
	}

	bus->range[0] = range[0]; /* min */
	bus->range[1] = range[1]; /* max */

	buses->count++;
	bus->dev = dev;
	dprintk(CVP_CORE, "Found bus %s [%d->%d] with governor %s\n",
			bus->name, bus->master, bus->slave, bus->governor);
err_bus:
	return rc;
}

static int msm_cvp_load_regulator_table(
		struct msm_cvp_platform_resources *res)
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
		dprintk(CVP_ERR,
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
			dprintk(CVP_WARN, "%s is not a phandle\n",
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
			dprintk(CVP_ERR,
					"Failed to alloc memory for regulator name\n");
			goto err_reg_name_alloc;
		}
		strlcpy(rinfo->name, domains_property->name,
			(supply - domains_property->name) + 1);

		rinfo->has_hw_power_collapse = of_property_read_bool(
			regulator_node, "qcom,support-hw-trigger");

		dprintk(CVP_CORE, "Found regulator %s: h/w collapse = %s\n",
				rinfo->name,
				rinfo->has_hw_power_collapse ? "yes" : "no");
	}

	if (!regulators->count)
		dprintk(CVP_CORE, "No regulators found");

	return 0;

err_reg_name_alloc:
err_reg_tbl_alloc:
	msm_cvp_free_regulator_table(res);
	return rc;
}

static int msm_cvp_load_clock_table(
		struct msm_cvp_platform_resources *res)
{
	int rc = 0, num_clocks = 0, c = 0;
	struct platform_device *pdev = res->pdev;
	int *clock_props = NULL;
	struct clock_set *clocks = &res->clock_set;

	num_clocks = of_property_count_strings(pdev->dev.of_node,
				"clock-names");
	if (num_clocks <= 0) {
		dprintk(CVP_CORE, "No clocks found\n");
		clocks->count = 0;
		rc = 0;
		goto err_load_clk_table_fail;
	}

	clock_props = devm_kzalloc(&pdev->dev, num_clocks *
			sizeof(*clock_props), GFP_KERNEL);
	if (!clock_props) {
		dprintk(CVP_ERR, "No memory to read clock properties\n");
		rc = -ENOMEM;
		goto err_load_clk_table_fail;
	}

	rc = of_property_read_u32_array(pdev->dev.of_node,
				"qcom,clock-configs", clock_props,
				num_clocks);
	if (rc) {
		dprintk(CVP_ERR, "Failed to read clock properties: %d\n", rc);
		goto err_load_clk_prop_fail;
	}

	clocks->clock_tbl = devm_kzalloc(&pdev->dev, sizeof(*clocks->clock_tbl)
			* num_clocks, GFP_KERNEL);
	if (!clocks->clock_tbl) {
		dprintk(CVP_ERR, "Failed to allocate memory for clock tbl\n");
		rc = -ENOMEM;
		goto err_load_clk_prop_fail;
	}

	clocks->count = num_clocks;
	dprintk(CVP_CORE, "Found %d clocks\n", num_clocks);

	for (c = 0; c < num_clocks; ++c) {
		struct clock_info *vc = &res->clock_set.clock_tbl[c];

		of_property_read_string_index(pdev->dev.of_node,
				"clock-names", c, &vc->name);

		if (clock_props[c] & CLOCK_PROP_HAS_SCALING) {
			vc->has_scaling = true;
		} else {
			vc->count = 0;
			vc->has_scaling = false;
		}

		if (clock_props[c] & CLOCK_PROP_HAS_MEM_RETENTION)
			vc->has_mem_retention = true;
		else
			vc->has_mem_retention = false;

		dprintk(CVP_CORE, "Found clock %s: scale-able = %s\n", vc->name,
			vc->count ? "yes" : "no");
	}

	return 0;

err_load_clk_prop_fail:
err_load_clk_table_fail:
	return rc;
}

#define MAX_CLK_RESETS 5

static int msm_cvp_load_reset_table(
		struct msm_cvp_platform_resources *res)
{
	struct platform_device *pdev = res->pdev;
	struct reset_set *rst = &res->reset_set;
	int num_clocks = 0, c = 0, ret = 0;
	int pwr_stats[MAX_CLK_RESETS];

	num_clocks = of_property_count_strings(pdev->dev.of_node,
				"reset-names");
	if (num_clocks <= 0 || num_clocks > MAX_CLK_RESETS) {
		dprintk(CVP_ERR, "Num reset clocks out of range\n");
		rst->count = 0;
		return 0;
	}

	rst->reset_tbl = devm_kcalloc(&pdev->dev, num_clocks,
			sizeof(*rst->reset_tbl), GFP_KERNEL);
	if (!rst->reset_tbl)
		return -ENOMEM;

	rst->count = num_clocks;
	dprintk(CVP_CORE, "Found %d reset clocks\n", num_clocks);
	ret = of_property_read_u32_array(pdev->dev.of_node,
				"reset-power-status", pwr_stats,
				num_clocks);
	if (ret) {
		dprintk(CVP_ERR, "Failed to read reset pwr state: %d\n", ret);
		devm_kfree(&pdev->dev, rst->reset_tbl);
		return ret;
	}

	for (c = 0; c < num_clocks; ++c) {
		struct reset_info *rc = &res->reset_set.reset_tbl[c];

		of_property_read_string_index(pdev->dev.of_node,
				"reset-names", c, &rc->name);
		rc->required_state = pwr_stats[c];
	}

	return 0;
}

static int find_key_value(struct msm_cvp_platform_data *platform_data,
	const char *key)
{
	int i = 0;
	struct msm_cvp_common_data *common_data = platform_data->common_data;
	int size = platform_data->common_data_length;

	for (i = 0; i < size; i++) {
		if (!strcmp(common_data[i].key, key))
			return common_data[i].value;
	}
	return 0;
}

int cvp_read_platform_resources_from_drv_data(
		struct msm_cvp_core *core)
{
	struct msm_cvp_platform_data *platform_data;
	struct msm_cvp_platform_resources *res;
	int rc = 0;

	if (!core || !core->platform_data) {
		dprintk(CVP_ERR, "%s Invalid data\n", __func__);
		return -ENOENT;
	}
	platform_data = core->platform_data;
	res = &core->resources;

	res->sku_version = platform_data->sku_version;

	res->auto_pil = find_key_value(platform_data,
			"qcom,auto-pil");

	res->max_load = find_key_value(platform_data,
			"qcom,max-hw-load");

	res->sw_power_collapsible = find_key_value(platform_data,
			"qcom,sw-power-collapse");

	res->never_unload_fw =  find_key_value(platform_data,
			"qcom,never-unload-fw");

	res->debug_timeout = find_key_value(platform_data,
			"qcom,debug-timeout");

	res->pm_qos_latency_us = find_key_value(platform_data,
			"qcom,pm-qos-latency-us");

	res->max_secure_inst_count = find_key_value(platform_data,
			"qcom,max-secure-instances");

	res->thermal_mitigable = find_key_value(platform_data,
			"qcom,enable-thermal-mitigation");
	res->msm_cvp_pwr_collapse_delay = find_key_value(platform_data,
			"qcom,power-collapse-delay");
	res->msm_cvp_firmware_unload_delay = find_key_value(platform_data,
			"qcom,fw-unload-delay");
	res->msm_cvp_hw_rsp_timeout = find_key_value(platform_data,
			"qcom,hw-resp-timeout");
	res->msm_cvp_dsp_rsp_timeout = find_key_value(platform_data,
			"qcom,dsp-resp-timeout");
	res->non_fatal_pagefaults = find_key_value(platform_data,
			"qcom,domain-attr-non-fatal-faults");

	res->vpu_ver = platform_data->vpu_ver;
	res->ubwc_config = platform_data->ubwc_config;
	return rc;

}

int cvp_read_platform_resources_from_dt(
		struct msm_cvp_platform_resources *res)
{
	struct platform_device *pdev = res->pdev;
	struct resource *kres = NULL;
	int rc = 0;
	uint32_t firmware_base = 0;

	if (!pdev->dev.of_node) {
		dprintk(CVP_ERR, "DT node not found\n");
		return -ENOENT;
	}

	INIT_LIST_HEAD(&res->context_banks);

	res->firmware_base = (phys_addr_t)firmware_base;

	kres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	res->register_base = kres ? kres->start : -1;
	res->register_size = kres ? (kres->end + 1 - kres->start) : -1;

	kres = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	res->irq = kres ? kres->start : -1;

	rc = msm_cvp_load_fw_name(res);
	dprintk(CVP_CORE, "EVA fw: %s found.\n", res->fw_name);
	if (rc)
		dprintk(CVP_WARN, "Failed to load fw name info: %d\n", rc);

	rc = msm_cvp_load_subcache_info(res);
	if (rc)
		dprintk(CVP_WARN, "Failed to load subcache info: %d\n", rc);

	rc = msm_cvp_load_qdss_table(res);
	if (rc)
		dprintk(CVP_WARN, "Failed to load qdss reg table: %d\n", rc);

	rc = msm_cvp_load_reg_table(res);
	if (rc) {
		dprintk(CVP_ERR, "Failed to load reg table: %d\n", rc);
		goto err_load_reg_table;
	}

	rc = msm_cvp_load_ipcc_regs(res);
	if (rc)
		dprintk(CVP_ERR, "Failed to load IPCC regs: %d\n", rc);

	rc = msm_cvp_load_regulator_table(res);
	if (rc) {
		dprintk(CVP_ERR, "Failed to load list of regulators %d\n", rc);
		goto err_load_regulator_table;
	}

	rc = msm_cvp_load_clock_table(res);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to load clock table: %d\n", rc);
		goto err_load_clock_table;
	}

	rc = msm_cvp_load_allowed_clocks_table(res);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to load allowed clocks table: %d\n", rc);
		goto err_load_allowed_clocks_table;
	}

	rc = msm_cvp_load_reset_table(res);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to load reset table: %d\n", rc);
		goto err_load_reset_table;
	}

	res->use_non_secure_pil = of_property_read_bool(pdev->dev.of_node,
			"qcom,use-non-secure-pil");

	if (res->use_non_secure_pil || !is_iommu_present(res)) {
		of_property_read_u32(pdev->dev.of_node, "qcom,fw-bias",
				&firmware_base);
		res->firmware_base = (phys_addr_t)firmware_base;
		dprintk(CVP_CORE,
				"Using fw-bias : %pa", &res->firmware_base);
	}

return rc;

err_load_reset_table:
	msm_cvp_free_allowed_clocks_table(res);
err_load_allowed_clocks_table:
	msm_cvp_free_clock_table(res);
err_load_clock_table:
	msm_cvp_free_regulator_table(res);
err_load_regulator_table:
	msm_cvp_free_reg_table(res);
err_load_reg_table:
	return rc;
}

static int msm_cvp_setup_context_bank(struct msm_cvp_platform_resources *res,
		struct context_bank_info *cb, struct device *dev)
{
	int rc = 0;
	struct bus_type *bus;

	if (!dev || !cb || !res) {
		dprintk(CVP_ERR,
			"%s: Invalid Input params\n", __func__);
		return -EINVAL;
	}
	cb->dev = dev;

	bus = cb->dev->bus;
	if (IS_ERR_OR_NULL(bus)) {
		dprintk(CVP_ERR, "%s - failed to get bus type\n", __func__);
		rc = PTR_ERR(bus) ?: -ENODEV;
		goto remove_cb;
	}

	/*
	 * configure device segment size and segment boundary to ensure
	 * iommu mapping returns one mapping (which is required for partial
	 * cache operations)
	 */
	if (!dev->dma_parms)
		dev->dma_parms =
			devm_kzalloc(dev, sizeof(*dev->dma_parms), GFP_KERNEL);
	dma_set_max_seg_size(dev, DMA_BIT_MASK(32));
	dma_set_seg_boundary(dev, DMA_BIT_MASK(64));

	dprintk(CVP_CORE, "Attached %s and created mapping\n", dev_name(dev));
	dprintk(CVP_CORE,
		"Context bank name:%s, buffer_type: %#x, is_secure: %d, address range start: %#x, size: %#x, dev: %pK",
		cb->name, cb->buffer_type, cb->is_secure, cb->addr_range.start,
		cb->addr_range.size, cb->dev);

	return rc;

remove_cb:
	return rc;
}

int msm_cvp_smmu_fault_handler(struct iommu_domain *domain,
		struct device *dev, unsigned long iova, int flags, void *token)
{
	struct msm_cvp_core *core = token;
	struct msm_cvp_inst *inst;
	u32 *pfaddr = &core->last_fault_addr;

	if (!domain || !core) {
		dprintk(CVP_ERR, "%s - invalid param %pK %pK\n",
			__func__, domain, core);
		return -EINVAL;
	}

	if (core->smmu_fault_handled) {
		if (core->resources.non_fatal_pagefaults) {
			pr_err_ratelimited(
				"msm_cvp: non-fatal pagefault address %lx\n",
				 iova);
			*pfaddr = (*pfaddr == 0) ? iova : (*pfaddr);
			return 0;
		}
	}

	dprintk(CVP_ERR, "%s - faulting address: %lx\n", __func__, iova);

	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		msm_cvp_print_inst_bufs(inst);
	}
	core->smmu_fault_handled = true;
	msm_cvp_noc_error_info(core);
	mutex_unlock(&core->lock);
	/*
	 * Return -EINVAL to elicit the default behaviour of smmu driver.
	 * If we return -ENOSYS, then smmu driver assumes page fault handler
	 * is not installed and prints a list of useful debug information like
	 * FAR, SID etc. This information is not printed if we return 0.
	 */
	return -EINVAL;
}

static int msm_cvp_populate_context_bank(struct device *dev,
		struct msm_cvp_core *core)
{
	int rc = 0;
	struct context_bank_info *cb = NULL;
	struct device_node *np = NULL;

	if (!dev || !core) {
		dprintk(CVP_ERR, "%s - invalid inputs\n", __func__);
		return -EINVAL;
	}

	np = dev->of_node;
	cb = devm_kzalloc(dev, sizeof(*cb), GFP_KERNEL);
	if (!cb) {
		dprintk(CVP_ERR, "%s - Failed to allocate cb\n", __func__);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&cb->list);
	list_add_tail(&cb->list, &core->resources.context_banks);

	rc = of_property_read_string(np, "label", &cb->name);
	if (rc) {
		dprintk(CVP_CORE,
			"Failed to read cb label from device tree\n");
		rc = 0;
	}

	dprintk(CVP_CORE, "%s: context bank has name %s\n", __func__, cb->name);
	rc = of_property_read_u32_array(np, "qcom,iommu-dma-addr-pool",
			(u32 *)&cb->addr_range, 2);
	if (rc) {
		dprintk(CVP_ERR,
			"Could not read addr pool for context bank : %s %d\n",
			cb->name, rc);
		goto err_setup_cb;
	}

	cb->is_secure = of_property_read_bool(np, "qcom,iommu-vmid");
	dprintk(CVP_CORE, "context bank %s : secure = %d\n",
			cb->name, cb->is_secure);

	/* setup buffer type for each sub device*/
	rc = of_property_read_u32(np, "buffer-types", &cb->buffer_type);
	if (rc) {
		dprintk(CVP_ERR, "failed to load buffer_type info %d\n", rc);
		rc = -ENOENT;
		goto err_setup_cb;
	}
	dprintk(CVP_CORE,
		"context bank %s address start = %x address size = %x buffer_type = %x\n",
		cb->name, cb->addr_range.start,
		cb->addr_range.size, cb->buffer_type);

	cb->domain = iommu_get_domain_for_dev(dev);
	if (IS_ERR_OR_NULL(cb->domain)) {
		dprintk(CVP_ERR, "Create domain failed\n");
		rc = -ENODEV;
		goto err_setup_cb;
	}

	rc = msm_cvp_setup_context_bank(&core->resources, cb, dev);
	if (rc) {
		dprintk(CVP_ERR, "Cannot setup context bank %d\n", rc);
		goto err_setup_cb;
	}

	iommu_set_fault_handler(cb->domain,
		msm_cvp_smmu_fault_handler, (void *)core);

	return 0;

err_setup_cb:
	list_del(&cb->list);
	return rc;
}

int cvp_read_context_bank_resources_from_dt(struct platform_device *pdev)
{
	struct msm_cvp_core *core;
	int rc = 0;

	if (!pdev) {
		dprintk(CVP_ERR, "Invalid platform device\n");
		return -EINVAL;
	} else if (!pdev->dev.parent) {
		dprintk(CVP_ERR, "Failed to find a parent for %s\n",
				dev_name(&pdev->dev));
		return -ENODEV;
	}

	core = dev_get_drvdata(pdev->dev.parent);
	if (!core) {
		dprintk(CVP_ERR, "Failed to find cookie in parent device %s",
				dev_name(pdev->dev.parent));
		return -EINVAL;
	}

	rc = msm_cvp_populate_context_bank(&pdev->dev, core);
	if (rc)
		dprintk(CVP_ERR, "Failed to probe context bank\n");
	else
		dprintk(CVP_CORE, "Successfully probed context bank\n");

	return rc;
}

int cvp_read_bus_resources_from_dt(struct platform_device *pdev)
{
	struct msm_cvp_core *core;

	if (!pdev) {
		dprintk(CVP_ERR, "Invalid platform device\n");
		return -EINVAL;
	} else if (!pdev->dev.parent) {
		dprintk(CVP_ERR, "Failed to find a parent for %s\n",
				dev_name(&pdev->dev));
		return -ENODEV;
	}

	core = dev_get_drvdata(pdev->dev.parent);
	if (!core) {
		dprintk(CVP_ERR, "Failed to find cookie in parent device %s",
				dev_name(pdev->dev.parent));
		return -EINVAL;
	}

	return msm_cvp_populate_bus(&pdev->dev, &core->resources);
}

int cvp_read_mem_cdsp_resources_from_dt(struct platform_device *pdev)
{
	struct msm_cvp_core *core;

	if (!pdev) {
		dprintk(CVP_ERR, "%s: invalid platform device\n", __func__);
		return -EINVAL;
	} else if (!pdev->dev.parent) {
		dprintk(CVP_ERR, "Failed to find a parent for %s\n",
				dev_name(&pdev->dev));
		return -ENODEV;
	}

	core = dev_get_drvdata(pdev->dev.parent);
	if (!core) {
		dprintk(CVP_ERR, "Failed to find cookie in parent device %s",
				dev_name(pdev->dev.parent));
		return -EINVAL;
	}

	return msm_cvp_populate_mem_cdsp(&pdev->dev, &core->resources);
}
