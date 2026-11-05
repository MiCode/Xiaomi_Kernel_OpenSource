// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "qcom_mpam_slc: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/of_platform.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/qcom_scmi_vendor.h>
#include <linux/scmi_protocol.h>
#include <soc/qcom/mpam_msc.h>
#include <soc/qcom/mpam_slc.h>

#define QCOM_SLC_MPAM_SCMI_STR	0x534c434d50414d /* SLCMPAM */
#define SCMI_GET_PARAM_LEN	32

enum mpam_slc_set_param_ids {
	PARAM_SET_CACHE_PARTITION_MSC = 1,
	PARAM_RESET_CACHE_PARTITION_MSC = 2,
	PARAM_SET_CONFIG_MON_MSC = 3,
	PARAM_SET_CONFIG_SLC_MPAM_START_STOP = 4,
};

enum mpam_slc_get_param_ids {
	PARAM_GET_CLIENT_INFO_MSC = 1,
	PARAM_GET_CACHE_CAPABILITY_MSC = 2,
	PARAM_GET_CACHE_PARTITION_MSC = 3,
};

static uint16_t slc_client_id[] = {
	APPS,
	GPU,
	NSP,
	SLC_CLIENT_MAX,
};

static int mpam_msc_slc_set_params(struct device *dev, void *param, int param_len,
		uint32_t param_id)
{
	struct qcom_mpam_msc *qcom_msc;

	qcom_msc = (struct qcom_mpam_msc *)dev_get_drvdata(dev);
	if (qcom_msc == NULL)
		return -EINVAL;

	return qcom_msc->scmi_ops->set_param(qcom_msc->ph, param, QCOM_SLC_MPAM_SCMI_STR,
				param_id, param_len);
}

static int mpam_msc_slc_get_params(struct device *dev, void *param_in, int param_in_len,
		void *param_out, int param_out_len, uint32_t param_id)
{
	int ret = -EPERM;
	uint8_t buf[SCMI_GET_PARAM_LEN];
	struct qcom_mpam_msc *qcom_msc;

	qcom_msc = (struct qcom_mpam_msc *)dev_get_drvdata(dev);
	if (qcom_msc == NULL)
		return ret;

	if (param_in_len && param_in_len <= SCMI_GET_PARAM_LEN) {
		memcpy(buf, param_in, param_in_len);
		ret = qcom_msc->scmi_ops->get_param(qcom_msc->ph, buf,
			QCOM_SLC_MPAM_SCMI_STR, param_id, param_in_len, param_out_len);
	}

	if (!ret)
		memcpy(param_out, buf, param_out_len);

	return ret;
}

static struct qcom_mpam_msc *slc_capability_check(struct device *dev, struct msc_query *query)
{
	struct qcom_mpam_msc *qcom_msc;
	struct qcom_slc_capability *slc_capability;
	struct slc_client_capability *slc_client_cap;
	int client_idx, partid_idx;

	client_idx = query->client_id;
	partid_idx = query->part_id;

	if (client_idx >= SLC_CLIENT_MAX) {
		dev_err(dev, "Invalid Client ID %d\n", client_idx);
		return NULL;
	}

	qcom_msc = (struct qcom_mpam_msc *)dev_get_drvdata(dev);
	if (qcom_msc == NULL)
		return NULL;

	slc_capability = (struct qcom_slc_capability *)qcom_msc->msc_capability;
	slc_client_cap = &(slc_capability->slc_client_cap[client_idx]);

	if (slc_client_cap->enabled == false) {
		dev_err(dev, "Client not enabled for configuration %d\n", client_idx);
		return NULL;
	}

	if (partid_idx >= slc_client_cap->client_info.num_part_id) {
		dev_err(dev, "Invalid PART id %d\n", partid_idx);
		return NULL;
	}

	return qcom_msc;
}

static int slc_set_cache_partition(struct device *dev, void *msc_partid, void *msc_partconfig)
{
	struct qcom_mpam_msc *qcom_msc;
	struct slc_partid_config slc_part_config;
	struct qcom_slc_capability *slc_capability;
	struct slc_client_capability *slc_client_cap;
	struct slc_partid_capability *slc_partid_cap;
	int client_idx, partid_idx, gear_idx;

	memcpy(&slc_part_config.query, msc_partid, sizeof(struct msc_query));
	memcpy(&slc_part_config.gear_config, msc_partconfig, sizeof(struct qcom_slc_gear_val));

	qcom_msc = slc_capability_check(dev, &slc_part_config.query);
	if (qcom_msc == NULL)
		return -EINVAL;

	client_idx = slc_part_config.query.client_id;
	partid_idx = slc_part_config.query.part_id;
	slc_capability = (struct qcom_slc_capability *)qcom_msc->msc_capability;
	slc_client_cap = &(slc_capability->slc_client_cap[client_idx]);
	slc_partid_cap = &(slc_client_cap->slc_partid_cap[partid_idx]);

	for (gear_idx = 0; gear_idx < slc_partid_cap->num_gears; gear_idx++) {
		if (slc_partid_cap->part_id_gears[gear_idx] == slc_part_config.gear_config.gear_val)
			break;
	}

	if (gear_idx == slc_partid_cap->num_gears) {
		dev_err(dev, "GEAR config not valid!\n");
		return -EINVAL;
	}

	return mpam_msc_slc_set_params(dev, &slc_part_config, sizeof(struct slc_partid_config),
			PARAM_SET_CACHE_PARTITION_MSC);
}

static int slc_reset_cache_partition(struct device *dev, void *msc_partid, void *msc_partconfig)
{
	struct qcom_mpam_msc *qcom_msc;
	struct slc_partid_config slc_part_config = {0};

	memcpy(&slc_part_config.query, msc_partid, sizeof(struct msc_query));
	memcpy(&slc_part_config.gear_config, msc_partconfig, sizeof(struct qcom_slc_gear_val));

	qcom_msc = slc_capability_check(dev, &slc_part_config.query);
	if (qcom_msc == NULL)
		return -EINVAL;

	return mpam_msc_slc_set_params(dev, &slc_part_config, sizeof(struct slc_partid_config),
			PARAM_RESET_CACHE_PARTITION_MSC);
}

static int slc_client_query(struct device *dev, void *msc_partid, void *msc_partconfig)
{
	struct qcom_mpam_msc *qcom_msc;
	struct msc_query *query;
	struct slc_client_info *client_info;

	query = (struct msc_query *)msc_partid;
	client_info = (struct slc_client_info *)msc_partconfig;

	qcom_msc = (struct qcom_mpam_msc *)dev_get_drvdata(dev);
	if (qcom_msc == NULL)
		return -EINVAL;

	return mpam_msc_slc_get_params(dev, query, sizeof(struct msc_query), client_info,
			sizeof(struct slc_client_info), PARAM_GET_CLIENT_INFO_MSC);
}

static int slc_get_cache_partition(struct device *dev, void *msc_partid, void *msc_partconfig)
{
	struct qcom_mpam_msc *qcom_msc;
	struct msc_query *query;
	struct qcom_slc_capability *qcom_slc_capability;
	struct slc_client_capability *slc_client_cap;
	struct qcom_slc_gear_val *gear_config;

	query = (struct msc_query *)msc_partid;
	qcom_msc = slc_capability_check(dev, query);
	if (qcom_msc == NULL)
		return -EINVAL;

	gear_config = (struct qcom_slc_gear_val *)msc_partconfig;
	qcom_slc_capability = (struct qcom_slc_capability *)qcom_msc->msc_capability;
	slc_client_cap = &qcom_slc_capability->slc_client_cap[query->client_id];
	if (slc_client_cap->enabled == false)
		return -EINVAL;

	return mpam_msc_slc_get_params(dev, query, sizeof(struct msc_query), gear_config,
			sizeof(struct qcom_slc_gear_val), PARAM_GET_CACHE_PARTITION_MSC);
}

static int slc_get_cache_partition_capability(struct device *dev, void *msc_partid,
		void *msc_partconfig)
{
	struct qcom_mpam_msc *qcom_msc;
	struct msc_query *query;
	struct qcom_slc_capability *qcom_slc_capability;
	struct slc_partid_capability *slc_partid_capability;
	struct slc_client_capability *slc_client_cap;

	query = (struct msc_query *)msc_partid;
	qcom_msc = slc_capability_check(dev, query);
	if (qcom_msc == NULL)
		return -EINVAL;

	slc_partid_capability = (struct slc_partid_capability *) msc_partconfig;
	qcom_slc_capability = (struct qcom_slc_capability *)qcom_msc->msc_capability;
	slc_client_cap = &qcom_slc_capability->slc_client_cap[query->client_id];
	if (slc_client_cap->enabled == false)
		return -EINVAL;

	return mpam_msc_slc_get_params(dev, query, sizeof(struct msc_query), slc_partid_capability,
			sizeof(struct slc_partid_capability), PARAM_GET_CACHE_CAPABILITY_MSC);
}

static int mon_idx_lookup(void __iomem *mem, int client_id, int part_id)
{
	int mon_idx;
	struct qcom_slc_mon_mem *mon_mem = (struct qcom_slc_mon_mem *)mem;
	struct slc_partid_info *part_info;

	for (mon_idx = 0; mon_idx < SLC_NUM_PARTIDS; mon_idx++) {
		part_info = &mon_mem->data[mon_idx].part_info;
		if ((client_id == part_info->client_id) &&
				(part_id == part_info->part_id))
			break;
	}

	if (mon_idx == SLC_NUM_PARTIDS)
		return -EINVAL;

	return mon_idx;
}

static struct qcom_mpam_msc *slc_config_request_check(struct device *dev, struct msc_query *query,
		struct slc_mon_config_val *mon_cfg)
{
	struct qcom_mpam_msc *qcom_msc;
	struct qcom_slc_capability *slc_capability;
	int mon_idx;

	qcom_msc = slc_capability_check(dev, query);
	if (qcom_msc == NULL)
		return NULL;

	slc_capability =  (struct qcom_slc_capability *)qcom_msc->msc_capability;
	mon_idx = mon_idx_lookup(qcom_msc->mon_base, query->client_id, query->part_id);
	if (mon_idx < 0)
		return NULL;

	switch (mon_cfg->slc_mon_function) {
	default:
		break;

	case CACHE_CAPACITY_CONFIG:
		switch (mon_cfg->enable) {
		default:
			break;

		case 0:
			if (slc_capability->slc_mon_configured.capacity_configured <= 0)
				return NULL;
			break;

		case 1:
			if (slc_capability->slc_mon_configured.capacity_configured >=
					slc_capability->slc_mon_list.capacity_config_available)
				return NULL;
			break;
		}
		break;

	case CACHE_READ_MISS_CONFIG:
		switch (mon_cfg->enable) {
		default:
			break;

		case 0:
			if (slc_capability->slc_mon_configured.read_miss_configured <= 0)
				return NULL;
			break;

		case 1:
			if (slc_capability->slc_mon_configured.read_miss_configured >=
					slc_capability->slc_mon_list.read_miss_config_available)
				return NULL;
			break;
		}
		break;
	}

	return qcom_msc;

}

static int update_mon_stats(struct device *dev, struct msc_query *query,
		struct slc_mon_config_val *mon_cfg)
{
	struct qcom_mpam_msc *qcom_msc;
	struct qcom_slc_capability *slc_capability;
	int mon_idx;

	qcom_msc = slc_capability_check(dev, query);
	if (qcom_msc == NULL)
		return -EINVAL;

	slc_capability =  (struct qcom_slc_capability *)qcom_msc->msc_capability;
	mon_idx = mon_idx_lookup(qcom_msc->mon_base, query->client_id, query->part_id);
	if (mon_idx < 0)
		return -EINVAL;

	switch (mon_cfg->slc_mon_function) {
	default:
		break;

	case CACHE_CAPACITY_CONFIG:
		switch (mon_cfg->enable) {
		default:
			break;

		case 0:
			slc_capability->slc_mon_configured.capacity_configured--;
			break;

		case 1:
			slc_capability->slc_mon_configured.capacity_configured++;
			break;

		}
		break;

	case CACHE_READ_MISS_CONFIG:
		switch (mon_cfg->enable) {
		default:
			break;

		case 0:
			slc_capability->slc_mon_configured.read_miss_configured--;
			break;

		case 1:
			slc_capability->slc_mon_configured.read_miss_configured++;
			break;

		}
		break;
	}

	return 0;
}

static int slc_mon_config(struct device *dev, void *msc_partid, void *msc_partconfig)
{
	struct qcom_mpam_msc *qcom_msc;
	struct msc_query *query;
	struct slc_mon_config_val *mon_cfg_val;
	struct slc_mon_config mon_cfg = {0};
	int ret = -EINVAL;

	query = (struct msc_query *)msc_partid;
	mon_cfg_val = (struct slc_mon_config_val *)msc_partconfig;

	qcom_msc = slc_config_request_check(dev, query, mon_cfg_val);
	if (qcom_msc == NULL)
		return ret;

	memcpy(&mon_cfg.query, msc_partid, sizeof(struct msc_query));
	memcpy(&mon_cfg.config, msc_partconfig, sizeof(struct slc_mon_config_val));
	ret = mpam_msc_slc_set_params(dev, &mon_cfg, sizeof(struct  slc_mon_config),
			PARAM_SET_CONFIG_MON_MSC);
	if (ret)
		pr_err("Failed to Config SLC Mon\n");
	else
		update_mon_stats(dev, query, mon_cfg_val);

	return ret;
};

#define MAX_SHARED_MEM_RETRY_CNT	5000
#define MAX_SHARED_MATCH_SEQ_CNT	10

static int slc_mon_stats_read(struct device *dev, void *msc_partid, void *data)
{
	struct qcom_mpam_msc *qcom_msc;
	struct msc_query *query;
	struct qcom_slc_mon_mem *mon_mem;
	union mon_values *mon_data;
	struct qcom_slc_mon_data *data_mem;
	int mon_idx;
	uint32_t match_seq, retry_cnt, match_seq_cnt = 0;

	qcom_msc = (struct qcom_mpam_msc *)dev_get_drvdata(dev);
	if (qcom_msc == NULL)
		return -EINVAL;

	query = (struct msc_query *)msc_partid;
	mon_data = (union mon_values *)data;
	mon_idx = mon_idx_lookup(qcom_msc->mon_base, query->client_id, query->part_id);
	if (mon_idx < 0)
		return -EINVAL;

	mon_mem = (struct qcom_slc_mon_mem *)qcom_msc->mon_base;
	data_mem = &(mon_mem->data[mon_idx]);
	do {
		retry_cnt = 0;
		while (unlikely((match_seq = mon_mem->match_seq) % 2) &&
				(retry_cnt++ < MAX_SHARED_MEM_RETRY_CNT))
			;

		if (retry_cnt == MAX_SHARED_MEM_RETRY_CNT)
			return -EINVAL;

		/* Read as zero if monitor not enabled */
		mon_data->ref.mon_data = 0;
		if ((mon_data->ref.slc_mon_function == CACHE_CAPACITY_CONFIG) &&
				(data_mem->cap_stats.cap_enabled))
			mon_data->capacity.num_cache_lines = data_mem->cap_stats.num_cache_lines;

		if ((mon_data->ref.slc_mon_function == CACHE_READ_MISS_CONFIG) &&
				(data_mem->rd_miss_stats.miss_enabled))
			mon_data->misses.num_rd_misses = data_mem->rd_miss_stats.rd_misses;

		mon_data->ref.last_capture_time = mon_mem->last_capture_time;
	} while ((match_seq != mon_mem->match_seq) &&
			(match_seq_cnt++ < MAX_SHARED_MATCH_SEQ_CNT));

	if (match_seq_cnt == MAX_SHARED_MATCH_SEQ_CNT)
		return -EINVAL;

	return 0;
}

static struct mpam_msc_ops slc_msc_ops = {
	.set_cache_partition = slc_set_cache_partition,
	.get_cache_partition = slc_get_cache_partition,
	.get_cache_partition_capability = slc_get_cache_partition_capability,
	.reset_cache_partition = slc_reset_cache_partition,
	.mon_config = slc_mon_config,
	.mon_stats_read = slc_mon_stats_read,
};

static int slc_client_info_read(struct device *dev, struct device_node *node)
{
	int client_idx, partid_idx, ret = -EINVAL;
	struct msc_query query;
	struct qcom_mpam_msc *qcom_msc;
	struct slc_client_capability *slc_client_cap;
	struct qcom_slc_capability *qcom_slc_capability;

	qcom_msc = (struct qcom_mpam_msc *)dev_get_drvdata(dev);
	if (qcom_msc->qcom_msc_id.qcom_msc_type != SLC)
		return -EINVAL;

	qcom_slc_capability = (struct qcom_slc_capability *)qcom_msc->msc_capability;
	query.qcom_msc_id.qcom_msc_type = qcom_msc->qcom_msc_id.qcom_msc_type;
	query.qcom_msc_id.qcom_msc_class = qcom_msc->qcom_msc_id.qcom_msc_class;
	query.qcom_msc_id.idx = qcom_msc->qcom_msc_id.idx;
	for (client_idx = 0; client_idx < qcom_slc_capability->num_clients; client_idx++) {
		slc_client_cap = &(qcom_slc_capability->slc_client_cap[client_idx]);
		query.client_id = slc_client_id[client_idx];
		slc_client_cap->client_info.client_id = slc_client_id[client_idx];
		ret = of_property_read_string_index(node, "qcom,slc-clients", client_idx,
							&slc_client_cap->client_name);
		slc_client_cap->enabled = false;
		if (slc_client_query(dev, &query, &(slc_client_cap->client_info)))
			continue;

		if ((slc_client_cap->client_info.num_part_id == 0) ||
				(slc_client_cap->client_info.num_part_id == SLC_INVALID_PARTID))
			continue;

		slc_client_cap->enabled = true;
		slc_client_cap->slc_partid_cap = devm_kcalloc(dev,
				slc_client_cap->client_info.num_part_id,
				sizeof(struct slc_partid_capability), GFP_KERNEL);
		if (slc_client_cap->slc_partid_cap == NULL) {
			ret = -ENOMEM;
			break;
		}

		for (partid_idx = 0; partid_idx < slc_client_cap->client_info.num_part_id;
				partid_idx++) {
			query.part_id = partid_idx;
			ret = msc_system_get_device_capability(qcom_msc->msc_id, &query,
					&(slc_client_cap->slc_partid_cap[partid_idx]));
			if (ret)
				continue;
		}
	}

	return ret;
}

static int slc_mpam_enable_disable(struct device *dev, void *msc_partid, void *msc_partconfig)
{
	struct qcom_slc_mpam_enable_cfg mon_cfg = {0};

	memcpy(&mon_cfg.query, msc_partid, sizeof(struct msc_query));
	memcpy(&mon_cfg.enable, msc_partconfig, sizeof(struct mpam_enable));

	return mpam_msc_slc_set_params(dev, &mon_cfg, sizeof(struct qcom_slc_mpam_enable_cfg),
			PARAM_SET_CONFIG_SLC_MPAM_START_STOP);
}

static int slc_mpam_start_stop(struct device *dev, bool start)
{
	struct qcom_mpam_msc *qcom_msc;
	struct msc_query query = {0};
	struct mpam_enable enable = {0};

	qcom_msc = (struct qcom_mpam_msc *)dev_get_drvdata(dev);
	if (qcom_msc == NULL)
		return -EINVAL;

	query.qcom_msc_id.qcom_msc_type = qcom_msc->qcom_msc_id.qcom_msc_type;
	query.qcom_msc_id.qcom_msc_class = qcom_msc->qcom_msc_id.qcom_msc_class;
	query.qcom_msc_id.idx = qcom_msc->qcom_msc_id.idx;
	query.client_id = 0;
	query.part_id = 0;
	enable.value = start;
	return slc_mpam_enable_disable(dev, &query, &enable);
}

static int mpam_msc_slc_probe(struct platform_device *pdev)
{
	int ret = -EINVAL;
	uint32_t val;
	struct device_node *node;
	struct qcom_mpam_msc *qcom_msc;
	struct qcom_slc_capability *qcom_slc_capability;
	struct qcom_slc_mon_mem *mon_mem;
	struct resource *res;

	qcom_msc = devm_kzalloc(&pdev->dev, sizeof(struct qcom_mpam_msc), GFP_KERNEL);
	if (qcom_msc == NULL)
		return -ENOMEM;

	qcom_msc->mpam_available = MPAM_UNINITIALIZAED;
	qcom_msc->sdev = get_qcom_scmi_device();
	if (IS_ERR(qcom_msc->sdev))
		return PTR_ERR(qcom_msc->sdev);

	qcom_msc->scmi_ops = qcom_msc->sdev->handle->devm_protocol_get(qcom_msc->sdev,
			QCOM_SCMI_VENDOR_PROTOCOL, &qcom_msc->ph);
	if (IS_ERR(qcom_msc->scmi_ops))
		return dev_err_probe(&pdev->dev, PTR_ERR(qcom_msc->scmi_ops),
				"Error getting vendor protocol ops\n");

	qcom_msc->qcom_msc_id.qcom_msc_type = SLC;
	qcom_msc->qcom_msc_id.qcom_msc_class = CACHE_TYPE;
	qcom_msc->ops = &slc_msc_ops;
	qcom_msc->dev = &pdev->dev;

	node = pdev->dev.of_node;
	ret = of_property_read_u32(node, "qcom,dev-index", &val);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "qcom,dev-index Node failed!\n");

	if (val > U8_MAX)
		return dev_err_probe(&pdev->dev, -EINVAL, "dev-index wrong value failed!\n");

	qcom_msc->qcom_msc_id.idx = val;
	ret = of_property_read_string(node, "qcom,msc-name", &qcom_msc->msc_name);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "qcom,msc-name Node failed!\n");

	ret = of_property_read_u32(node, "qcom,msc-id", &qcom_msc->msc_id);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "qcom,msc-id Node failed!\n");

	platform_set_drvdata(pdev, qcom_msc);
	if (slc_mpam_start_stop(&pdev->dev, true))
		return dev_err_probe(&pdev->dev, -EINVAL, "MAPM SCMI failure!\n");

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mon-base");
	qcom_msc->mon_base = devm_ioremap_resource(&pdev->dev, res);

	if (IS_ERR_OR_NULL(qcom_msc->mon_base))
		return dev_err_probe(&pdev->dev, PTR_ERR(qcom_msc->mon_base),
				"ioremap slc mpam_mon_base failed!\n");

	mon_mem = (struct qcom_slc_mon_mem *) qcom_msc->mon_base;
	if (mon_mem->msc_id != qcom_msc->msc_id)
		return dev_err_probe(&pdev->dev, -EINVAL, "IMEM Init failure!\n");

	ret = attach_mpam_msc(&pdev->dev, qcom_msc, SLC);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "MPAM MSC attach failed!\n");

	qcom_msc->msc_capability = devm_kzalloc(&pdev->dev, sizeof(struct qcom_slc_capability),
			GFP_KERNEL);
	if (qcom_msc->msc_capability == NULL) {
		ret = -ENOMEM;
		goto err_detach;
	}

	qcom_slc_capability = (struct qcom_slc_capability *)qcom_msc->msc_capability;
	qcom_slc_capability->num_clients = of_property_count_strings(node, "qcom,slc-clients");
	qcom_slc_capability->slc_client_cap = devm_kcalloc(&pdev->dev,
			qcom_slc_capability->num_clients, sizeof(struct  slc_client_capability),
			GFP_KERNEL);
	if (qcom_slc_capability->slc_client_cap == NULL) {
		ret = -ENOMEM;
		goto err_detach;
	}

	ret = of_property_read_u32(node, "qcom,num-read-miss-cfg", &val);
	if (ret)
		goto err_detach;

	qcom_slc_capability->slc_mon_list.read_miss_config_available = val;
	ret = of_property_read_u32(node, "qcom,num-cap-cfg", &val);
	if (ret)
		goto err_detach;

	qcom_slc_capability->slc_mon_list.capacity_config_available = val;

	qcom_slc_capability->slc_mon_configured.read_miss_configured = 0;
	qcom_slc_capability->slc_mon_configured.capacity_configured = 0;

	qcom_msc->mpam_available = MPAM_AVAILABLE;
	if (slc_client_info_read(&pdev->dev, node)) {
		dev_err(&pdev->dev, "Failed to detect SLC device\n");
		goto err_detach;
	}

	qcom_msc->mpam_available = MPAM_MONITRS_AVAILABLE;
	return 0;

err_detach:
	detach_mpam_msc(&pdev->dev, qcom_msc, SLC);
	return dev_err_probe(&pdev->dev, PTR_ERR(qcom_msc->mon_base),
			"MPAM SLC driver probe failed!\n");
}

static int mpam_msc_slc_remove(struct platform_device *pdev)
{
	struct qcom_mpam_msc *qcom_msc;

	qcom_msc = (struct qcom_mpam_msc *)platform_get_drvdata(pdev);
	qcom_msc->mpam_available = MPAM_AVAILABLE;
	if (qcom_msc != NULL) {
		if (slc_mpam_start_stop(&pdev->dev, false))
			dev_err(&pdev->dev, "Failed to stop SLC Monitor thread\n");

		detach_mpam_msc(&pdev->dev, qcom_msc, SLC);
	}

	qcom_msc->mpam_available = MPAM_UNINITIALIZAED;
	platform_set_drvdata(pdev, NULL);
	return 0;
}
static const struct of_device_id mpam_msc_slc_table[] = {
	{ .compatible = "qcom,slc-mpam" },
	{}
};
MODULE_DEVICE_TABLE(of, mpam_msc_slc_table);

static struct platform_driver mpam_msc_slc_driver = {
	.driver = {
		.name = "mpam-msc-slc",
		.of_match_table = mpam_msc_slc_table,
		.suppress_bind_attrs = true,
	},
	.probe = mpam_msc_slc_probe,
	.remove = mpam_msc_slc_remove,
};

module_platform_driver(mpam_msc_slc_driver);

MODULE_SOFTDEP("pre: llcc_qcom");
MODULE_SOFTDEP("pre: qcom_scmi_client");
MODULE_DESCRIPTION("QCOM MPAM MSC SLC driver");
MODULE_LICENSE("GPL");
