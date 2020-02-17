// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */

#include <asm/dma-iommu.h>
#include <linux/iommu.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include "msm_vidc_debug.h"
#include "msm_vidc_resources.h"
#include "msm_vidc_res_parse.h"
#include "soc/qcom/secure_buffer.h"

enum clock_properties {
	CLOCK_PROP_HAS_SCALING = 1 << 0,
	CLOCK_PROP_HAS_MEM_RETENTION    = 1 << 1,
};

static inline struct device *msm_iommu_get_ctx(const char *ctx_name)
{
	return NULL;
}

static int msm_vidc_populate_legacy_context_bank(
			struct msm_vidc_platform_resources *res);

static size_t get_u32_array_num_elements(struct device_node *np,
					char *name)
{
	int len;
	size_t num_elements = 0;

	if (!of_get_property(np, name, &len)) {
		d_vpr_e("Failed to read %s from device tree\n", name);
		goto fail_read;
	}

	num_elements = len / sizeof(u32);
	if (num_elements <= 0) {
		d_vpr_e("%s not specified in device tree\n", name);
		goto fail_read;
	}
	return num_elements;

fail_read:
	return 0;
}

static inline void msm_vidc_free_allowed_clocks_table(
		struct msm_vidc_platform_resources *res)
{
	res->allowed_clks_tbl = NULL;
}

static inline void msm_vidc_free_cycles_per_mb_table(
		struct msm_vidc_platform_resources *res)
{
	res->clock_freq_tbl.clk_prof_entries = NULL;
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
	kfree(res->bus_set.bus_tbl);
	res->bus_set.bus_tbl = NULL;
	res->bus_set.count = 0;
}

static inline void msm_vidc_free_buffer_usage_table(
			struct msm_vidc_platform_resources *res)
{
	res->buffer_usage_set.buffer_usage_tbl = NULL;
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

static inline void msm_vidc_free_cx_ipeak_context(
			struct msm_vidc_platform_resources *res)
{
	cx_ipeak_unregister(res->cx_ipeak_context);
	res->cx_ipeak_context = NULL;
}

void msm_vidc_free_platform_resources(
			struct msm_vidc_platform_resources *res)
{
	msm_vidc_free_clock_table(res);
	msm_vidc_free_regulator_table(res);
	msm_vidc_free_allowed_clocks_table(res);
	msm_vidc_free_reg_table(res);
	msm_vidc_free_qdss_addr_table(res);
	msm_vidc_free_bus_vectors(res);
	msm_vidc_free_buffer_usage_table(res);
	msm_vidc_free_cx_ipeak_context(res);
}

static int msm_vidc_load_reg_table(struct msm_vidc_platform_resources *res)
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
		d_vpr_h("reg-presets not found\n");
		return 0;
	}

	reg_set = &res->reg_set;
	reg_set->count = get_u32_array_num_elements(pdev->dev.of_node,
			"qcom,reg-presets");
	reg_set->count /=  sizeof(*reg_set->reg_tbl) / sizeof(u32);

	if (!reg_set->count) {
		d_vpr_h("no elements in reg set\n");
		return rc;
	}

	reg_set->reg_tbl = devm_kzalloc(&pdev->dev, reg_set->count *
			sizeof(*(reg_set->reg_tbl)), GFP_KERNEL);
	if (!reg_set->reg_tbl) {
		d_vpr_e("%s: Failed to alloc register table\n", __func__);
		return -ENOMEM;
	}

	if (of_property_read_u32_array(pdev->dev.of_node, "qcom,reg-presets",
		(u32 *)reg_set->reg_tbl, reg_set->count * 2)) {
		d_vpr_e("Failed to read register table\n");
		msm_vidc_free_reg_table(res);
		return -EINVAL;
	}
	for (i = 0; i < reg_set->count; i++) {
		d_vpr_h("reg = %x, value = %x\n",
			reg_set->reg_tbl[i].reg, reg_set->reg_tbl[i].value
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
		/*
		 * qcom,qdss-presets is an optional property. It likely won't be
		 * present if we don't have any register settings to program
		 */
		d_vpr_h("qdss-presets not found\n");
		return rc;
	}

	qdss_addr_set = &res->qdss_addr_set;
	qdss_addr_set->count = get_u32_array_num_elements(pdev->dev.of_node,
					"qcom,qdss-presets");
	qdss_addr_set->count /= sizeof(*qdss_addr_set->addr_tbl) / sizeof(u32);

	if (!qdss_addr_set->count) {
		d_vpr_h("no elements in qdss reg set\n");
		return rc;
	}

	qdss_addr_set->addr_tbl = devm_kzalloc(&pdev->dev,
			qdss_addr_set->count * sizeof(*qdss_addr_set->addr_tbl),
			GFP_KERNEL);
	if (!qdss_addr_set->addr_tbl) {
		d_vpr_e("%s: Failed to alloc register table\n", __func__);
		rc = -ENOMEM;
		goto err_qdss_addr_tbl;
	}

	rc = of_property_read_u32_array(pdev->dev.of_node, "qcom,qdss-presets",
		(u32 *)qdss_addr_set->addr_tbl, qdss_addr_set->count * 2);
	if (rc) {
		d_vpr_e("Failed to read qdss address table\n");
		msm_vidc_free_qdss_addr_table(res);
		rc = -EINVAL;
		goto err_qdss_addr_tbl;
	}

	for (i = 0; i < qdss_addr_set->count; i++) {
		d_vpr_h("qdss addr = %x, value = %x\n",
				qdss_addr_set->addr_tbl[i].start,
				qdss_addr_set->addr_tbl[i].size);
	}
err_qdss_addr_tbl:
	return rc;
}

static int msm_vidc_load_subcache_info(struct msm_vidc_platform_resources *res)
{
	int rc = 0, num_subcaches = 0, c;
	struct platform_device *pdev = res->pdev;
	struct subcache_set *subcaches = &res->subcache_set;

	num_subcaches = of_property_count_strings(pdev->dev.of_node,
		"cache-slice-names");
	if (num_subcaches <= 0) {
		d_vpr_h("No subcaches found\n");
		goto err_load_subcache_table_fail;
	}

	subcaches->subcache_tbl = devm_kzalloc(&pdev->dev,
		sizeof(*subcaches->subcache_tbl) * num_subcaches, GFP_KERNEL);
	if (!subcaches->subcache_tbl) {
		d_vpr_e("Failed to allocate memory for subcache tbl\n");
		rc = -ENOMEM;
		goto err_load_subcache_table_fail;
	}

	subcaches->count = num_subcaches;
	d_vpr_h("Found %d subcaches\n", num_subcaches);

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
 * msm_vidc_load_u32_table() - load dtsi table entries
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
int msm_vidc_load_u32_table(struct platform_device *pdev,
		struct device_node *of_node, char *table_name, int struct_size,
		u32 **table, u32 *num_elements)
{
	int rc = 0, num_elemts = 0;
	u32 *ptbl = NULL;

	if (!of_find_property(of_node, table_name, NULL)) {
		d_vpr_h("%s not found\n", table_name);
		return 0;
	}

	num_elemts = get_u32_array_num_elements(of_node, table_name);
	if (!num_elemts) {
		d_vpr_e("no elements in %s\n", table_name);
		return 0;
	}
	num_elemts /= struct_size / sizeof(u32);

	ptbl = devm_kzalloc(&pdev->dev, num_elemts * struct_size, GFP_KERNEL);
	if (!ptbl) {
		d_vpr_e("Failed to alloc table %s\n", table_name);
		return -ENOMEM;
	}

	if (of_property_read_u32_array(of_node, table_name, ptbl,
			num_elemts * struct_size / sizeof(u32))) {
		d_vpr_e("Failed to read %s\n", table_name);
		return -EINVAL;
	}

	*table = ptbl;
	if (num_elements)
		*num_elements = num_elemts;

	return rc;
}
EXPORT_SYMBOL(msm_vidc_load_u32_table);

/* A comparator to compare loads (needed later on) */
static int cmp(const void *a, const void *b)
{
	/* want to sort in reverse so flip the comparison */
	return ((struct allowed_clock_rates_table *)b)->clock_rate -
		((struct allowed_clock_rates_table *)a)->clock_rate;
}

static int msm_vidc_load_allowed_clocks_table(
		struct msm_vidc_platform_resources *res)
{
	int rc = 0;
	struct platform_device *pdev = res->pdev;

	if (!of_find_property(pdev->dev.of_node,
			"qcom,allowed-clock-rates", NULL)) {
		d_vpr_h("allowed-clock-rates not found\n");
		return 0;
	}

	rc = msm_vidc_load_u32_table(pdev, pdev->dev.of_node,
				"qcom,allowed-clock-rates",
				sizeof(*res->allowed_clks_tbl),
				(u32 **)&res->allowed_clks_tbl,
				&res->allowed_clks_tbl_size);
	if (rc) {
		d_vpr_e("%s: failed to read allowed clocks table\n", __func__);
		return rc;
	}

	sort(res->allowed_clks_tbl, res->allowed_clks_tbl_size,
		 sizeof(*res->allowed_clks_tbl), cmp, NULL);

	return 0;
}

static int msm_vidc_populate_mem_cdsp(struct device *dev,
		struct msm_vidc_platform_resources *res)
{
	res->mem_cdsp.dev = dev;

	return 0;
}

static int msm_vidc_populate_bus(struct device *dev,
		struct msm_vidc_platform_resources *res)
{
	struct bus_set *buses = &res->bus_set;
	const char *temp_name = NULL;
	struct bus_info *bus = NULL, *temp_table;
	u32 range[2];
	int rc = 0;

	temp_table = krealloc(buses->bus_tbl, sizeof(*temp_table) *
			(buses->count + 1), GFP_KERNEL);
	if (!temp_table) {
		d_vpr_e("%s: Failed to allocate memory", __func__);
		rc = -ENOMEM;
		goto err_bus;
	}

	buses->bus_tbl = temp_table;
	bus = &buses->bus_tbl[buses->count];

	memset(bus, 0x0, sizeof(struct bus_info));

	rc = of_property_read_string(dev->of_node, "label", &temp_name);
	if (rc) {
		d_vpr_e("'label' not found in node\n");
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
		d_vpr_e("'bus-master' not found in node\n");
		goto err_bus;
	}

	rc = of_property_read_u32(dev->of_node, "qcom,bus-slave", &bus->slave);
	if (rc) {
		d_vpr_e("'bus-slave' not found in node\n");
		goto err_bus;
	}

	rc = of_property_read_string(dev->of_node, "qcom,mode",
			&bus->mode);

	if (!rc && !strcmp(bus->mode, "performance"))
		bus->is_prfm_mode = true;

	rc = of_property_read_u32_array(dev->of_node, "qcom,bus-range-kbps",
			range, ARRAY_SIZE(range));
	if (rc) {
		rc = 0;
		d_vpr_h("'bus-range' not found defaulting to <0 INT_MAX>\n");
		range[0] = 0;
		range[1] = INT_MAX;
	}

	bus->range[0] = range[0]; /* min */
	bus->range[1] = range[1]; /* max */

	buses->count++;
	bus->dev = dev;
	d_vpr_h("Found bus %s [%d->%d] with mode %s\n",
			bus->name, bus->master, bus->slave, bus->mode);
err_bus:
	return rc;
}

static int msm_vidc_load_buffer_usage_table(
		struct msm_vidc_platform_resources *res)
{
	int rc = 0;
	struct platform_device *pdev = res->pdev;
	struct buffer_usage_set *buffer_usage_set = &res->buffer_usage_set;

	if (!of_find_property(pdev->dev.of_node,
				"qcom,buffer-type-tz-usage-table", NULL)) {
		/*
		 * qcom,buffer-type-tz-usage-table is an optional property.  It
		 * likely won't be present if the core doesn't support content
		 * protection
		 */
		d_vpr_h("buffer-type-tz-usage-table not found\n");
		return 0;
	}

	buffer_usage_set->count = get_u32_array_num_elements(
		pdev->dev.of_node, "qcom,buffer-type-tz-usage-table");
	buffer_usage_set->count /=
		sizeof(*buffer_usage_set->buffer_usage_tbl) / sizeof(u32);
	if (!buffer_usage_set->count) {
		d_vpr_h("no elements in buffer usage set\n");
		return 0;
	}

	buffer_usage_set->buffer_usage_tbl = devm_kzalloc(&pdev->dev,
			buffer_usage_set->count *
			sizeof(*buffer_usage_set->buffer_usage_tbl),
			GFP_KERNEL);
	if (!buffer_usage_set->buffer_usage_tbl) {
		d_vpr_e("%s: Failed to alloc buffer usage table\n",
			__func__);
		rc = -ENOMEM;
		goto err_load_buf_usage;
	}

	rc = of_property_read_u32_array(pdev->dev.of_node,
		    "qcom,buffer-type-tz-usage-table",
		(u32 *)buffer_usage_set->buffer_usage_tbl,
		buffer_usage_set->count *
		sizeof(*buffer_usage_set->buffer_usage_tbl) / sizeof(u32));
	if (rc) {
		d_vpr_e("Failed to read buffer usage table\n");
		goto err_load_buf_usage;
	}

	return 0;
err_load_buf_usage:
	msm_vidc_free_buffer_usage_table(res);
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
		d_vpr_e("Failed to alloc memory for regulator table\n");
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
			d_vpr_e("%s is not a phandle\n",
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
			d_vpr_e("Failed to alloc memory for regulator name\n");
			goto err_reg_name_alloc;
		}
		strlcpy(rinfo->name, domains_property->name,
			(supply - domains_property->name) + 1);

		rinfo->has_hw_power_collapse = of_property_read_bool(
			regulator_node, "qcom,support-hw-trigger");

		d_vpr_h("Found regulator %s: h/w collapse = %s\n",
				rinfo->name,
				rinfo->has_hw_power_collapse ? "yes" : "no");
	}

	if (!regulators->count)
		d_vpr_h("No regulators found");

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
		d_vpr_h("No clocks found\n");
		clocks->count = 0;
		rc = 0;
		goto err_load_clk_table_fail;
	}

	clock_props = devm_kzalloc(&pdev->dev, num_clocks *
			sizeof(*clock_props), GFP_KERNEL);
	if (!clock_props) {
		d_vpr_e("No memory to read clock properties\n");
		rc = -ENOMEM;
		goto err_load_clk_table_fail;
	}

	rc = of_property_read_u32_array(pdev->dev.of_node,
				"qcom,clock-configs", clock_props,
				num_clocks);
	if (rc) {
		d_vpr_e("Failed to read clock properties: %d\n", rc);
		goto err_load_clk_prop_fail;
	}

	clocks->clock_tbl = devm_kzalloc(&pdev->dev, sizeof(*clocks->clock_tbl)
			* num_clocks, GFP_KERNEL);
	if (!clocks->clock_tbl) {
		d_vpr_e("Failed to allocate memory for clock tbl\n");
		rc = -ENOMEM;
		goto err_load_clk_prop_fail;
	}

	clocks->count = num_clocks;
	d_vpr_h("Found %d clocks\n", num_clocks);

	for (c = 0; c < num_clocks; ++c) {
		struct clock_info *vc = &res->clock_set.clock_tbl[c];

		of_property_read_string_index(pdev->dev.of_node,
				"clock-names", c, &vc->name);

		if (clock_props[c] & CLOCK_PROP_HAS_SCALING) {
			vc->has_scaling = true;
		} else {
			vc->has_scaling = false;
		}

		if (clock_props[c] & CLOCK_PROP_HAS_MEM_RETENTION)
			vc->has_mem_retention = true;
		else
			vc->has_mem_retention = false;

		d_vpr_h("Found clock %s: scale-able = %s\n", vc->name,
			vc->has_scaling ? "yes" : "no");
	}


	return 0;

err_load_clk_prop_fail:
err_load_clk_table_fail:
	return rc;
}

static int msm_vidc_load_reset_table(
		struct msm_vidc_platform_resources *res)
{
	struct platform_device *pdev = res->pdev;
	struct reset_set *rst = &res->reset_set;
	int num_clocks = 0, c = 0;

	num_clocks = of_property_count_strings(pdev->dev.of_node,
				"reset-names");
	if (num_clocks <= 0) {
		d_vpr_h("No reset clocks found\n");
		rst->count = 0;
		return 0;
	}

	rst->reset_tbl = devm_kcalloc(&pdev->dev, num_clocks,
			sizeof(*rst->reset_tbl), GFP_KERNEL);
	if (!rst->reset_tbl)
		return -ENOMEM;

	rst->count = num_clocks;
	d_vpr_h("Found %d reset clocks\n", num_clocks);

	for (c = 0; c < num_clocks; ++c) {
		struct reset_info *rc = &res->reset_set.reset_tbl[c];

		of_property_read_string_index(pdev->dev.of_node,
				"reset-names", c, &rc->name);
	}

	return 0;
}

static int msm_decide_dt_node(
		struct msm_vidc_platform_resources *res)
{
	struct platform_device *pdev = res->pdev;
	int rc = 0;
	u32 sku_index = 0;

	rc = of_property_read_u32(pdev->dev.of_node, "sku-index",
			&sku_index);
	if (rc) {
		d_vpr_h("'sku_index' not found in node\n");
		return 0;
	}

	if (sku_index != res->sku_version) {
		d_vpr_h("Failed to parse dt: sku_index %d sku_version %d\n",
			sku_index, res->sku_version);
		return -EINVAL;
	}

	return 0;
}

static int find_key_value(struct msm_vidc_platform_data *platform_data,
	const char *key)
{
	int i = 0;
	struct msm_vidc_common_data *common_data = platform_data->common_data;
	int size = platform_data->common_data_length;

	for (i = 0; i < size; i++) {
		if (!strcmp(common_data[i].key, key))
			return common_data[i].value;
	}
	return 0;
}

int read_platform_resources_from_drv_data(
		struct msm_vidc_core *core)
{
	struct msm_vidc_platform_data *platform_data;
	struct msm_vidc_platform_resources *res;
	int rc = 0;

	if (!core || !core->platform_data) {
		d_vpr_e("%s: Invalid data\n", __func__);
		return -ENOENT;
	}
	platform_data = core->platform_data;
	res = &core->resources;

	res->codecs = platform_data->codecs;
	res->codecs_count = platform_data->codecs_count;
	res->codec_caps = platform_data->codec_caps;
	res->codec_caps_count = platform_data->codec_caps_count;
	res->codec_data_count = platform_data->codec_data_length;
	res->codec_data = platform_data->codec_data;

	res->sku_version = platform_data->sku_version;

	res->fw_name = "venus";

	d_vpr_h("Firmware filename: %s\n", res->fw_name);

	res->max_load = find_key_value(platform_data,
			"qcom,max-hw-load");

	res->max_mbpf = find_key_value(platform_data,
			"qcom,max-mbpf");

	res->max_hq_mbs_per_frame = find_key_value(platform_data,
			"qcom,max-hq-mbs-per-frame");

	res->max_hq_mbs_per_sec = find_key_value(platform_data,
			"qcom,max-hq-mbs-per-sec");

	res->max_bframe_mbs_per_frame = find_key_value(platform_data,
			"qcom,max-b-frame-mbs-per-frame");

	res->max_bframe_mbs_per_sec = find_key_value(platform_data,
			"qcom,max-b-frame-mbs-per-sec");

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

	res->slave_side_cp = find_key_value(platform_data,
			"qcom,slave-side-cp");
	res->thermal_mitigable = find_key_value(platform_data,
			"qcom,enable-thermal-mitigation");
	res->msm_vidc_pwr_collapse_delay = find_key_value(platform_data,
			"qcom,power-collapse-delay");
	res->msm_vidc_firmware_unload_delay = find_key_value(platform_data,
			"qcom,fw-unload-delay");
	res->msm_vidc_hw_rsp_timeout = find_key_value(platform_data,
			"qcom,hw-resp-timeout");
	res->cvp_internal = find_key_value(platform_data,
			"qcom,cvp-internal");
	res->cvp_external = find_key_value(platform_data,
			"qcom,cvp-external");
	res->non_fatal_pagefaults = find_key_value(platform_data,
			"qcom,domain-attr-non-fatal-faults");
	res->cache_pagetables = find_key_value(platform_data,
			"qcom,domain-attr-cache-pagetables");
	res->decode_batching = find_key_value(platform_data,
			"qcom,decode-batching");
	res->batch_timeout = find_key_value(platform_data,
			"qcom,batch-timeout");
	res->dcvs = find_key_value(platform_data,
			"qcom,dcvs");
	res->fw_cycles = find_key_value(platform_data,
			"qcom,fw-cycles");
	res->fw_vpp_cycles = find_key_value(platform_data,
			"qcom,fw-vpp-cycles");
	res->avsync_window_size = find_key_value(platform_data,
			"qcom,avsync-window-size");

	res->csc_coeff_data = &platform_data->csc_data;

	res->vpu_ver = platform_data->vpu_ver;
	res->ubwc_config = platform_data->ubwc_config;

	return rc;

}

static int msm_vidc_populate_cx_ipeak_context(
		struct msm_vidc_platform_resources *res)
{
	struct platform_device *pdev = res->pdev;
	int rc = 0;

	if (of_find_property(pdev->dev.of_node,
			"qcom,cx-ipeak-data", NULL))
		res->cx_ipeak_context = cx_ipeak_register(
				pdev->dev.of_node, "qcom,cx-ipeak-data");
	else
		return rc;

	if (IS_ERR(res->cx_ipeak_context)) {
		rc = PTR_ERR(res->cx_ipeak_context);
		if (rc == -EPROBE_DEFER)
			d_vpr_h("cx-ipeak register failed. Deferring probe!");
		else
			d_vpr_e("cx-ipeak register failed. rc: %d", rc);

		res->cx_ipeak_context = NULL;
		return rc;
	}

	if (res->cx_ipeak_context)
		d_vpr_h("cx-ipeak register successful");
	else
		d_vpr_h("cx-ipeak register not implemented");

	of_property_read_u32(pdev->dev.of_node,
		"qcom,clock-freq-threshold",
		&res->clk_freq_threshold);
	d_vpr_h("cx ipeak threshold frequency = %u\n",
			res->clk_freq_threshold);

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
		d_vpr_e("DT node not found\n");
		return -ENOENT;
	}

	rc = msm_decide_dt_node(res);
	if (rc)
		return rc;


	INIT_LIST_HEAD(&res->context_banks);

	res->firmware_base = (phys_addr_t)firmware_base;

	kres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	res->register_base = kres ? kres->start : -1;
	res->register_size = kres ? (kres->end + 1 - kres->start) : -1;

	kres = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	res->irq = kres ? kres->start : -1;

	rc = msm_vidc_load_subcache_info(res);
	if (rc)
		d_vpr_e("Failed to load subcache info: %d\n", rc);

	rc = msm_vidc_load_qdss_table(res);
	if (rc)
		d_vpr_e("Failed to load qdss reg table: %d\n", rc);

	rc = msm_vidc_load_reg_table(res);
	if (rc) {
		d_vpr_e("Failed to load reg table: %d\n", rc);
		goto err_load_reg_table;
	}

	rc = msm_vidc_load_buffer_usage_table(res);
	if (rc) {
		d_vpr_e("Failed to load buffer usage table: %d\n", rc);
		goto err_load_buffer_usage_table;
	}

	rc = msm_vidc_load_regulator_table(res);
	if (rc) {
		d_vpr_e("Failed to load list of regulators %d\n", rc);
		goto err_load_regulator_table;
	}

	rc = msm_vidc_load_clock_table(res);
	if (rc) {
		d_vpr_e("Failed to load clock table: %d\n", rc);
		goto err_load_clock_table;
	}

	rc = msm_vidc_load_allowed_clocks_table(res);
	if (rc) {
		d_vpr_e("Failed to load allowed clocks table: %d\n", rc);
		goto err_load_allowed_clocks_table;
	}

	rc = msm_vidc_load_reset_table(res);
	if (rc) {
		d_vpr_e("Failed to load reset table: %d\n", rc);
		goto err_load_reset_table;
	}

	rc = msm_vidc_populate_legacy_context_bank(res);
	if (rc) {
		d_vpr_e("Failed to setup context banks %d\n", rc);
		goto err_setup_legacy_cb;
	}

	rc = msm_vidc_populate_cx_ipeak_context(res);
	if (rc) {
		d_vpr_e("Failed to setup cx-ipeak %d\n", rc);
		goto err_register_cx_ipeak;
	}

return rc;

err_register_cx_ipeak:
	msm_vidc_free_cx_ipeak_context(res);
err_setup_legacy_cb:
err_load_reset_table:
	msm_vidc_free_allowed_clocks_table(res);
err_load_allowed_clocks_table:
	msm_vidc_free_clock_table(res);
err_load_clock_table:
	msm_vidc_free_regulator_table(res);
err_load_regulator_table:
	msm_vidc_free_buffer_usage_table(res);
err_load_buffer_usage_table:
	msm_vidc_free_reg_table(res);
err_load_reg_table:
	return rc;
}

static int msm_vidc_setup_context_bank(struct msm_vidc_platform_resources *res,
		struct context_bank_info *cb, struct device *dev)
{
	int rc = 0;
	struct bus_type *bus;

	if (!dev || !cb || !res) {
		d_vpr_e("%s: Invalid Input params\n", __func__);
		return -EINVAL;
	}
	cb->dev = dev;

	bus = cb->dev->bus;
	if (IS_ERR_OR_NULL(bus)) {
		d_vpr_e("%s: failed to get bus type\n", __func__);
		rc = PTR_ERR(bus) ? PTR_ERR(bus) : -ENODEV;
		goto remove_cb;
	}

	 cb->domain = iommu_get_domain_for_dev(cb->dev);

	 /*
	 * configure device segment size and segment boundary to ensure
	 * iommu mapping returns one mapping (which is required for partial
	 * cache operations)
	 */
	if (!dev->dma_parms)
		dev->dma_parms =
			devm_kzalloc(dev, sizeof(*dev->dma_parms), GFP_KERNEL);
	dma_set_max_seg_size(dev, DMA_BIT_MASK(32));
	dma_set_seg_boundary(dev, (unsigned long)DMA_BIT_MASK(64));

	d_vpr_h("Attached %s and created mapping\n", dev_name(dev));
	d_vpr_h(
		"Context bank: %s, buffer_type: %#x, is_secure: %d, address range start: %#x, size: %#x, dev: %pK, domain: %pK",
		cb->name, cb->buffer_type, cb->is_secure, cb->addr_range.start,
		cb->addr_range.size, cb->dev, cb->domain);

remove_cb:
	return rc;
}

int msm_vidc_smmu_fault_handler(struct iommu_domain *domain,
		struct device *dev, unsigned long iova, int flags, void *token)
{
	struct msm_vidc_core *core = token;
	struct msm_vidc_inst *inst;

	if (!domain || !core) {
		d_vpr_e("%s: invalid params %pK %pK\n",
			__func__, domain, core);
		return -EINVAL;
	}

	if (core->smmu_fault_handled) {
		if (core->resources.non_fatal_pagefaults) {
			dprintk_ratelimit(VIDC_ERR,
					"%s: non-fatal pagefault address: %lx\n",
					__func__, iova);
			return 0;
		}
	}

	d_vpr_e("%s: faulting address: %lx\n", __func__, iova);

	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		msm_comm_print_inst_info(inst);
	}
	core->smmu_fault_handled = true;
	mutex_unlock(&core->lock);
	/*
	 * Return -EINVAL to elicit the default behaviour of smmu driver.
	 * If we return -EINVAL, then smmu driver assumes page fault handler
	 * is not installed and prints a list of useful debug information like
	 * FAR, SID etc. This information is not printed if we return 0.
	 */
	return -EINVAL;
}

static int msm_vidc_populate_context_bank(struct device *dev,
		struct msm_vidc_core *core)
{
	int rc = 0;
	struct context_bank_info *cb = NULL;
	struct device_node *np = NULL;

	if (!dev || !core) {
		d_vpr_e("%s: invalid inputs\n", __func__);
		return -EINVAL;
	}

	np = dev->of_node;
	cb = devm_kzalloc(dev, sizeof(*cb), GFP_KERNEL);
	if (!cb) {
		d_vpr_e("%s: Failed to allocate cb\n", __func__);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&cb->list);
	list_add_tail(&cb->list, &core->resources.context_banks);

	rc = of_property_read_string(np, "label", &cb->name);
	if (rc) {
		d_vpr_h("Failed to read cb label from device tree\n");
		rc = 0;
	}

	d_vpr_h("%s: context bank has name %s\n", __func__, cb->name);
	rc = of_property_read_u32_array(np, "virtual-addr-pool",
			(u32 *)&cb->addr_range, 2);
	if (rc) {
		d_vpr_e("Could not read addr pool: context bank: %s %d\n",
			cb->name, rc);
		goto err_setup_cb;
	}

	cb->is_secure = of_property_read_bool(np, "qcom,secure-context-bank");
	d_vpr_h("context bank %s: secure = %d\n",
			cb->name, cb->is_secure);

	/* setup buffer type for each sub device*/
	rc = of_property_read_u32(np, "buffer-types", &cb->buffer_type);
	if (rc) {
		d_vpr_e("failed to load buffer_type info %d\n", rc);
		rc = -ENOENT;
		goto err_setup_cb;
	}
	d_vpr_h("context bank %s address start %x size %x buffer_type %x\n",
		cb->name, cb->addr_range.start,
		cb->addr_range.size, cb->buffer_type);

	rc = msm_vidc_setup_context_bank(&core->resources, cb, dev);
	if (rc) {
		d_vpr_e("Cannot setup context bank %d\n", rc);
		goto err_setup_cb;
	}

	iommu_set_fault_handler(cb->domain,
		msm_vidc_smmu_fault_handler, (void *)core);

	return 0;

err_setup_cb:
	list_del(&cb->list);
	return rc;
}

static int msm_vidc_populate_legacy_context_bank(
			struct msm_vidc_platform_resources *res)
{
	int rc = 0;
	struct platform_device *pdev = NULL;
	struct device_node *domains_parent_node = NULL;
	struct device_node *domains_child_node = NULL;
	struct device_node *ctx_node = NULL;
	struct context_bank_info *cb;

	if (!res || !res->pdev) {
		d_vpr_e("%s: invalid inputs\n", __func__);
		return -EINVAL;
	}
	pdev = res->pdev;

	domains_parent_node = of_find_node_by_name(pdev->dev.of_node,
			"qcom,vidc-iommu-domains");
	if (!domains_parent_node) {
		d_vpr_h("%s: legacy iommu domains not present\n", __func__);
		return 0;
	}

	/* set up each context bank for legacy DT bindings*/
	for_each_child_of_node(domains_parent_node,
		domains_child_node) {
		cb = devm_kzalloc(&pdev->dev, sizeof(*cb), GFP_KERNEL);
		if (!cb) {
			d_vpr_e("%s: Failed to allocate cb\n", __func__);
			return -ENOMEM;
		}
		INIT_LIST_HEAD(&cb->list);
		list_add_tail(&cb->list, &res->context_banks);

		ctx_node = of_parse_phandle(domains_child_node,
				"qcom,vidc-domain-phandle", 0);
		if (!ctx_node) {
			d_vpr_e("%s: Unable to parse pHandle\n", __func__);
			rc = -EBADHANDLE;
			goto err_setup_cb;
		}

		rc = of_property_read_string(ctx_node, "label", &(cb->name));
		if (rc) {
			d_vpr_e("%s: Could not find label\n", __func__);
			goto err_setup_cb;
		}

		rc = of_property_read_u32_array(ctx_node,
			"qcom,virtual-addr-pool", (u32 *)&cb->addr_range, 2);
		if (rc) {
			d_vpr_e("%s: Could not read addr pool: %s (%d)\n",
				__func__, cb->name, rc);
			goto err_setup_cb;
		}

		cb->is_secure =
			of_property_read_bool(ctx_node, "qcom,secure-domain");

		rc = of_property_read_u32(domains_child_node,
				"qcom,vidc-buffer-types", &cb->buffer_type);
		if (rc) {
			d_vpr_e("%s: Could not read buffer type (%d)\n",
				__func__, rc);
			goto err_setup_cb;
		}

		cb->dev = msm_iommu_get_ctx(cb->name);
		if (IS_ERR_OR_NULL(cb->dev)) {
			d_vpr_e("%s: could not get device for cb %s\n",
				__func__, cb->name);
			rc = -ENOENT;
			goto err_setup_cb;
		}

		rc = msm_vidc_setup_context_bank(res, cb, cb->dev);
		if (rc) {
			d_vpr_e("Cannot setup context bank %d\n", rc);
			goto err_setup_cb;
		}
		d_vpr_h(
			"context bank %s secure %d addr start = %#x size = %#x buffer_type = %#x\n",
			cb->name, cb->is_secure, cb->addr_range.start,
			cb->addr_range.size, cb->buffer_type);
	}
	return rc;

err_setup_cb:
	list_del(&cb->list);
	return rc;
}

int read_context_bank_resources_from_dt(struct platform_device *pdev)
{
	struct msm_vidc_core *core;
	int rc = 0;

	if (!pdev) {
		d_vpr_e("Invalid platform device\n");
		return -EINVAL;
	} else if (!pdev->dev.parent) {
		d_vpr_e("Failed to find a parent for %s\n",
			dev_name(&pdev->dev));
		return -ENODEV;
	}

	core = dev_get_drvdata(pdev->dev.parent);
	if (!core) {
		d_vpr_e("Failed to find cookie in parent device %s",
				dev_name(pdev->dev.parent));
		return -EINVAL;
	}

	rc = msm_vidc_populate_context_bank(&pdev->dev, core);
	if (rc)
		d_vpr_e("Failed to probe context bank\n");
	else
		d_vpr_h("Successfully probed context bank\n");

	return rc;
}

int read_bus_resources_from_dt(struct platform_device *pdev)
{
	struct msm_vidc_core *core;

	if (!pdev) {
		d_vpr_e("Invalid platform device\n");
		return -EINVAL;
	} else if (!pdev->dev.parent) {
		d_vpr_e("Failed to find a parent for %s\n",
			dev_name(&pdev->dev));
		return -ENODEV;
	}

	core = dev_get_drvdata(pdev->dev.parent);
	if (!core) {
		d_vpr_e("Failed to find cookie in parent device %s",
			dev_name(pdev->dev.parent));
		return -EINVAL;
	}

	return msm_vidc_populate_bus(&pdev->dev, &core->resources);
}

int read_mem_cdsp_resources_from_dt(struct platform_device *pdev)
{
	struct msm_vidc_core *core;

	if (!pdev) {
		d_vpr_e("%s: invalid platform device\n", __func__);
		return -EINVAL;
	} else if (!pdev->dev.parent) {
		d_vpr_e("Failed to find a parent for %s\n",
				dev_name(&pdev->dev));
		return -ENODEV;
	}

	core = dev_get_drvdata(pdev->dev.parent);
	if (!core) {
		d_vpr_e("Failed to find cookie in parent device %s",
				dev_name(pdev->dev.parent));
		return -EINVAL;
	}

	return msm_vidc_populate_mem_cdsp(&pdev->dev, &core->resources);
}
