/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <mach/board.h>
#include "msm_vidc_resources.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_res_parse.h"

struct master_slave {
	int masters_ocmem[2];
	int masters_ddr[2];
	int slaves_ocmem[2];
	int slaves_ddr[2];
};

static struct master_slave bus_vectors_masters_slaves = {
	.masters_ocmem = {MSM_BUS_MASTER_VIDEO_P0_OCMEM,
				MSM_BUS_MASTER_VIDEO_P1_OCMEM},
	.masters_ddr = {MSM_BUS_MASTER_VIDEO_P0, MSM_BUS_MASTER_VIDEO_P1},
	.slaves_ocmem = {MSM_BUS_SLAVE_OCMEM, MSM_BUS_SLAVE_OCMEM},
	.slaves_ddr = {MSM_BUS_SLAVE_EBI_CH0, MSM_BUS_SLAVE_EBI_CH0},
};

struct bus_pdata_config {
	int *masters;
	int *slaves;
	char *name;
};

static struct bus_pdata_config bus_pdata_config_vector[] = {
	{
		.masters = bus_vectors_masters_slaves.masters_ocmem,
		.slaves = bus_vectors_masters_slaves.slaves_ocmem,
		.name = "qcom,enc-ocmem-ab-ib",
	},
	{
		.masters = bus_vectors_masters_slaves.masters_ocmem,
		.slaves = bus_vectors_masters_slaves.slaves_ocmem,
		.name = "qcom,dec-ocmem-ab-ib",
	},
	{
		.masters = bus_vectors_masters_slaves.masters_ddr,
		.slaves = bus_vectors_masters_slaves.slaves_ddr,
		.name = "qcom,enc-ddr-ab-ib",
	},
	{
		.masters = bus_vectors_masters_slaves.masters_ddr,
		.slaves = bus_vectors_masters_slaves.slaves_ddr,
		.name = "qcom,dec-ddr-ab-ib",
	},
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
	return num_elements / 2;

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
	kfree(res->load_freq_tbl);
	res->load_freq_tbl = NULL;
}

static inline void msm_vidc_free_reg_table(
			struct msm_vidc_platform_resources *res)
{
	kfree(res->reg_set.reg_tbl);
	res->reg_set.reg_tbl = NULL;
}

static inline void msm_vidc_free_bus_vectors(
			struct msm_vidc_platform_resources *res)
{
	int i, j;
	if (res->bus_pdata) {
		for (i = 0; i < ARRAY_SIZE(bus_pdata_config_vector); i++) {
			for (j = 0; j < res->bus_pdata[i].num_usecases; j++) {
				kfree(res->bus_pdata[i].usecase[j].vectors);
				res->bus_pdata[i].usecase[j].vectors = NULL;
			}
			kfree(res->bus_pdata[i].usecase);
			res->bus_pdata[i].usecase = NULL;
		}
		kfree(res->bus_pdata);
		res->bus_pdata = NULL;
	}
}

static inline void msm_vidc_free_iommu_groups(
			struct msm_vidc_platform_resources *res)
{
	kfree(res->iommu_group_set.iommu_maps);
	res->iommu_group_set.iommu_maps = NULL;
}

static inline void msm_vidc_free_buffer_usage_table(
			struct msm_vidc_platform_resources *res)
{
	kfree(res->buffer_usage_set.buffer_usage_tbl);
	res->buffer_usage_set.buffer_usage_tbl = NULL;
}

void msm_vidc_free_platform_resources(
			struct msm_vidc_platform_resources *res)
{
	msm_vidc_free_freq_table(res);
	msm_vidc_free_reg_table(res);
	msm_vidc_free_bus_vectors(res);
	msm_vidc_free_iommu_groups(res);
	msm_vidc_free_buffer_usage_table(res);
}

static void msm_vidc_free_bus_vector(struct msm_bus_scale_pdata *bus_pdata)
{
	int i;
	for (i = 0; i < bus_pdata->num_usecases; i++) {
		kfree(bus_pdata->usecase[i].vectors);
		bus_pdata->usecase[i].vectors = NULL;
	}

	kfree(bus_pdata->usecase);
	bus_pdata->usecase = NULL;
}

static int msm_vidc_load_reg_table(struct msm_vidc_platform_resources *res)
{
	struct reg_set *reg_set;
	struct platform_device *pdev = res->pdev;
	int i;
	int rc = 0;

	reg_set = &res->reg_set;
	reg_set->count = get_u32_array_num_elements(pdev, "qcom,reg-presets");
	if (reg_set->count == 0) {
		dprintk(VIDC_DBG, "no elements in reg set\n");
		return rc;
	}

	reg_set->reg_tbl = kzalloc(reg_set->count *
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
static int msm_vidc_load_freq_table(struct msm_vidc_platform_resources *res)
{
	int rc = 0;
	int num_elements = 0;
	struct platform_device *pdev = res->pdev;

	num_elements = get_u32_array_num_elements(pdev, "qcom,load-freq-tbl");
	if (num_elements == 0) {
		dprintk(VIDC_ERR, "no elements in frequency table\n");
		return rc;
	}

	res->load_freq_tbl = kzalloc(num_elements * sizeof(*res->load_freq_tbl),
			GFP_KERNEL);
	if (!res->load_freq_tbl) {
		dprintk(VIDC_ERR,
				"%s Failed to alloc load_freq_tbl\n",
				__func__);
		return -ENOMEM;
	}

	if (of_property_read_u32_array(pdev->dev.of_node,
		"qcom,load-freq-tbl", (u32 *)res->load_freq_tbl,
		num_elements * 2)) {
		dprintk(VIDC_ERR, "Failed to read frequency table\n");
		msm_vidc_free_freq_table(res);
		return -EINVAL;
	}

	res->load_freq_tbl_size = num_elements;
	return rc;
}

static int msm_vidc_load_bus_vector(struct platform_device *pdev,
			struct msm_bus_scale_pdata *bus_pdata, u32 num_ports,
			struct bus_pdata_config *bus_pdata_config)
{
	struct bus_values {
	    u32 ab;
	    u32 ib;
	};
	struct bus_values *values;
	int i, j;
	int rc = 0;

	values = kzalloc(sizeof(*values) * bus_pdata->num_usecases, GFP_KERNEL);
	if (!values) {
		dprintk(VIDC_ERR, "%s Failed to alloc bus_values\n", __func__);
		rc = -ENOMEM;
		goto err_mem_alloc;
	}

	if (of_property_read_u32_array(pdev->dev.of_node,
		    bus_pdata_config->name, (u32 *)values,
		    bus_pdata->num_usecases * (sizeof(*values)/sizeof(u32)))) {
		dprintk(VIDC_ERR, "%s Failed to read bus values\n", __func__);
		rc = -EINVAL;
		goto err_parse_dt;
	}

	bus_pdata->usecase = kzalloc(sizeof(*bus_pdata->usecase) *
		    bus_pdata->num_usecases, GFP_KERNEL);
	if (!bus_pdata->usecase) {
		dprintk(VIDC_ERR,
			"%s Failed to alloc bus_pdata usecase\n", __func__);
		rc = -ENOMEM;
		goto err_parse_dt;
	}
	bus_pdata->name = bus_pdata_config->name;
	for (i = 0; i < bus_pdata->num_usecases; i++) {
		bus_pdata->usecase[i].vectors = kzalloc(
			sizeof(*bus_pdata->usecase[i].vectors) * num_ports,
			GFP_KERNEL);
		if (!bus_pdata->usecase) {
			dprintk(VIDC_ERR,
				"%s Failed to alloc bus_pdata usecase\n",
				__func__);
			break;
		}
		for (j = 0; j < num_ports; j++) {
			bus_pdata->usecase[i].vectors[j].ab = (u64)values[i].ab
									* 1000;
			bus_pdata->usecase[i].vectors[j].ib = (u64)values[i].ib
									* 1000;
			bus_pdata->usecase[i].vectors[j].src =
						bus_pdata_config->masters[j];
			bus_pdata->usecase[i].vectors[j].dst =
						bus_pdata_config->slaves[j];
			dprintk(VIDC_DBG,
				"ab = %llu, ib = %llu, src = %d, dst = %d\n",
				bus_pdata->usecase[i].vectors[j].ab,
				bus_pdata->usecase[i].vectors[j].ib,
				bus_pdata->usecase[i].vectors[j].src,
				bus_pdata->usecase[i].vectors[j].dst);
		}
		bus_pdata->usecase[i].num_paths = num_ports;
	}
	if (i < bus_pdata->num_usecases) {
		for (--i; i >= 0; i--) {
			kfree(bus_pdata->usecase[i].vectors);
			bus_pdata->usecase[i].vectors = NULL;
		}
		kfree(bus_pdata->usecase);
		bus_pdata->usecase = NULL;
		rc = -EINVAL;
	}
err_parse_dt:
	kfree(values);
err_mem_alloc:
	return rc;
}

static int msm_vidc_load_bus_vectors(struct msm_vidc_platform_resources *res)
{
	u32 num_ports = 0;
	int rc = 0;
	int i;
	struct platform_device *pdev = res->pdev;
	u32 num_bus_pdata = ARRAY_SIZE(bus_pdata_config_vector);

	if (of_property_read_u32_array(pdev->dev.of_node, "qcom,bus-ports",
			(u32 *)&num_ports, 1) || (num_ports == 0))
		goto err_mem_alloc;

	res->bus_pdata = kzalloc(sizeof(*res->bus_pdata) * num_bus_pdata,
				GFP_KERNEL);
	if (!res->bus_pdata) {
		dprintk(VIDC_ERR, "Failed to alloc memory\n");
		rc = -ENOMEM;
		goto err_mem_alloc;
	}
	for (i = 0; i < num_bus_pdata; i++) {
		if (!res->has_ocmem &&
			(!strcmp(bus_pdata_config_vector[i].name,
				"qcom,enc-ocmem-ab-ib")
			|| !strcmp(bus_pdata_config_vector[i].name,
				"qcom,dec-ocmem-ab-ib"))) {
			continue;
		}
		res->bus_pdata[i].num_usecases = get_u32_array_num_elements(
					pdev, bus_pdata_config_vector[i].name);
		if (res->bus_pdata[i].num_usecases == 0) {
			dprintk(VIDC_ERR, "no elements in %s\n",
				bus_pdata_config_vector[i].name);
			rc = -EINVAL;
			break;
		}

		rc = msm_vidc_load_bus_vector(pdev, &res->bus_pdata[i],
				num_ports, &bus_pdata_config_vector[i]);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to load bus vector: %d\n", i);
			break;
		}
	}
	if (i < num_bus_pdata) {
		for (--i; i >= 0; i--)
			msm_vidc_free_bus_vector(&res->bus_pdata[i]);
		kfree(res->bus_pdata);
		res->bus_pdata = NULL;
	}
err_mem_alloc:
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
	iommu_group_set->iommu_maps = kzalloc(iommu_group_set->count *
			sizeof(*(iommu_group_set->iommu_maps)), GFP_KERNEL);

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
				"domain %s : secure = %d",
				iommu_map->name,
				iommu_map->is_secure);

		/* setup partitions and buffer type per partition */
		rc = of_property_read_u32_array(domains_child_node,
				"qcom,vidc-partition-buffer-types",
				iommu_map->buffer_type,
				iommu_map->npartitions);

		if (rc) {
			dprintk(VIDC_ERR,
					"cannot load partition buffertype information (%d)",
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

static int msm_vidc_load_buffer_usage_table(
		struct msm_vidc_platform_resources *res)
{
	int rc = 0;
	struct platform_device *pdev = res->pdev;
	struct buffer_usage_set *buffer_usage_set = &res->buffer_usage_set;

	buffer_usage_set->count = get_u32_array_num_elements(
				    pdev, "qcom,buffer-type-tz-usage-table");
	if (buffer_usage_set->count == 0) {
		dprintk(VIDC_DBG, "no elements in buffer usage set\n");
		return 0;
	}

	buffer_usage_set->buffer_usage_tbl = kzalloc(buffer_usage_set->count *
			sizeof(*(buffer_usage_set->buffer_usage_tbl)),
			GFP_KERNEL);
	if (!buffer_usage_set->buffer_usage_tbl) {
		dprintk(VIDC_ERR, "%s Failed to alloc buffer usage table\n",
			__func__);
		rc = -ENOMEM;
		goto err_load_buf_usage;
	}

	rc = of_property_read_u32_array(pdev->dev.of_node,
		    "qcom,buffer-type-tz-usage-table",
		(u32 *)buffer_usage_set->buffer_usage_tbl,
		buffer_usage_set->count *
		(sizeof(*buffer_usage_set->buffer_usage_tbl)/sizeof(u32)));
	if (rc) {
		dprintk(VIDC_ERR, "Failed to read buffer usage table\n");
		goto err_load_buf_usage;
	}

	return 0;
err_load_buf_usage:
	msm_vidc_free_buffer_usage_table(res);
	return rc;
}

int read_platform_resources_from_dt(
		struct msm_vidc_platform_resources *res)
{
	struct platform_device *pdev = res->pdev;
	struct resource *kres = NULL;
	int rc = 0;

	if (!pdev->dev.of_node) {
		dprintk(VIDC_ERR, "DT node not found\n");
		return -ENOENT;
	}

	res->fw_base_addr = 0x0;

	kres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	res->register_base = kres ? kres->start : -1;
	res->register_size = kres ? (kres->end + 1 - kres->start) : -1;

	kres = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	res->irq = kres ? kres->start : -1;

	of_property_read_u32(pdev->dev.of_node,
			"qcom,ocmem-size", &res->has_ocmem);

	rc = msm_vidc_load_freq_table(res);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to load freq table: %d\n", rc);
		goto err_load_freq_table;
	}
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
	rc = msm_vidc_load_buffer_usage_table(res);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to load buffer usage table: %d\n", rc);
		goto err_load_buffer_usage_table;
	}

	rc = of_property_read_u32(pdev->dev.of_node, "qcom,max-hw-load",
			&res->max_load);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to determine max load supported: %d\n", rc);
		goto err_load_buffer_usage_table;
	}

	return rc;

err_load_buffer_usage_table:
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

int read_platform_resources_from_board(
		struct msm_vidc_platform_resources *res)
{
	struct resource *kres = NULL;
	struct platform_device *pdev = res->pdev;
	struct msm_vidc_v4l2_platform_data *pdata = pdev->dev.platform_data;
	int c = 0, rc = 0;

	if (!pdata) {
		dprintk(VIDC_ERR, "Platform data not found\n");
		return -ENOENT;
	}

	res->fw_base_addr = 0x0;

	kres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	res->register_base = kres ? kres->start : -1;
	res->register_size = kres ? (kres->end + 1 - kres->start) : -1;

	kres = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	res->irq = kres ? kres->start : -1;

	res->load_freq_tbl = kzalloc(pdata->num_load_table *
			sizeof(*res->load_freq_tbl), GFP_KERNEL);

	if (!res->load_freq_tbl) {
		dprintk(VIDC_ERR, "%s Failed to alloc load_freq_tbl\n",
				__func__);
		return -ENOMEM;
	}

	res->load_freq_tbl_size = pdata->num_load_table;
	for (c = 0; c > pdata->num_load_table; ++c) {
		res->load_freq_tbl[c].load = pdata->load_table[c][0];
		res->load_freq_tbl[c].freq = pdata->load_table[c][1];
	}

	res->max_load = pdata->max_load;
	return rc;
}
