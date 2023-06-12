// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/msm-bus.h>
#include "cam_soc_bus.h"

/**
 * struct cam_soc_bus_client_data : Bus client data
 *
 * @pdata: Bus pdata information
 * @client_id: Bus client id
 * @num_paths: Number of paths for this client
 * @curr_vote_level: current voted index
 * @dyn_vote: whether dynamic voting enabled
 */
struct cam_soc_bus_client_data {
	struct msm_bus_scale_pdata *pdata;
	uint32_t client_id;
	int num_paths;
	unsigned int curr_vote_level;
	bool dyn_vote;
};

int cam_soc_bus_client_update_request(void *client, unsigned int idx)
{
	int rc = 0;
	struct cam_soc_bus_client *bus_client =
		(struct cam_soc_bus_client *) client;
	struct cam_soc_bus_client_data *bus_client_data =
		(struct cam_soc_bus_client_data *) bus_client->client_data;

	if (bus_client_data->dyn_vote) {
		CAM_ERR(CAM_UTIL,
			"Dyn update not allowed client[%d][%s], dyn_vote: %d",
			bus_client_data->client_id,
			bus_client->common_data->name,
			bus_client_data->dyn_vote);
		rc = -EINVAL;
		goto end;
	}

	if (idx >= bus_client->common_data->num_usecases) {
		CAM_ERR(CAM_UTIL, "Invalid vote level=%d, usecases=%d", idx,
			bus_client->common_data->num_usecases);
		rc = -EINVAL;
		goto end;
	}

	CAM_DBG(CAM_PERF, "Bus client=[%d][%s] index[%d]",
		bus_client_data->client_id, bus_client->common_data->name, idx);

	rc = msm_bus_scale_client_update_request(bus_client_data->client_id,
		idx);
	if (rc) {
		CAM_ERR(CAM_UTIL,
			"Update request failed, client[%d][%s], idx: %d",
			bus_client_data->client_id,
			bus_client->common_data->name, idx);
		goto end;
	}

end:
	return rc;
}

int cam_soc_bus_client_update_bw(void *client, uint64_t ab, uint64_t ib)
{
	int idx = 0;
	struct msm_bus_paths *path;
	struct msm_bus_scale_pdata *pdata;
	struct cam_soc_bus_client *bus_client =
		(struct cam_soc_bus_client *) client;
	struct cam_soc_bus_client_data *bus_client_data =
		(struct cam_soc_bus_client_data *) bus_client->client_data;
	int rc = 0;

	if ((bus_client->common_data->num_usecases != 2) ||
		(bus_client_data->num_paths != 1) ||
		(!bus_client_data->dyn_vote)) {
		CAM_ERR(CAM_UTIL,
			"dynamic update not allowed Bus client=[%d][%s], %d %d %d",
			bus_client_data->client_id,
			bus_client->common_data->name,
			bus_client->common_data->num_usecases,
			bus_client_data->num_paths,
			bus_client_data->dyn_vote);
		rc = -EINVAL;
		goto end;
	}

	idx = bus_client_data->curr_vote_level;
	idx = 1 - idx;
	bus_client_data->curr_vote_level = idx;

	pdata = bus_client_data->pdata;
	path = &(pdata->usecase[idx]);
	path->vectors[0].ab = ab;
	path->vectors[0].ib = ib;

	CAM_DBG(CAM_PERF, "Bus client=[%d][%s] :ab[%llu] ib[%llu], index[%d]",
		bus_client_data->client_id, bus_client->common_data->name, ab,
		ib, idx);
	rc = msm_bus_scale_client_update_request(bus_client_data->client_id,
		idx);
	if (rc) {
		CAM_ERR(CAM_UTIL,
			"Update request failed, client[%d][%s], idx: %d",
			bus_client_data->client_id,
			bus_client->common_data->name, idx);
		return rc;
	}

end:
	return rc;
}

int cam_soc_bus_client_register(struct platform_device *pdev,
	struct device_node *dev_node, void **client,
	struct cam_soc_bus_client_common_data *common_data)
{
	struct msm_bus_scale_pdata *pdata = NULL;
	struct cam_soc_bus_client *bus_client = NULL;
	struct cam_soc_bus_client_data *bus_client_data = NULL;
	uint32_t client_id;
	int rc;

	bus_client = kzalloc(sizeof(struct cam_soc_bus_client), GFP_KERNEL);
	if (!bus_client) {
		CAM_ERR(CAM_UTIL, "Non Enought Memroy");
		rc = -ENOMEM;
		goto end;
	}

	*client = bus_client;

	bus_client_data = kzalloc(sizeof(struct cam_soc_bus_client_data),
		GFP_KERNEL);
	if (!bus_client_data) {
		kfree(bus_client);
		*client = NULL;
		rc = -ENOMEM;
		goto end;
	}

	bus_client->client_data = bus_client_data;
	pdata = msm_bus_pdata_from_node(pdev,
		dev_node);
	if (!pdata) {
		CAM_ERR(CAM_UTIL, "failed get_pdata");
		rc = -EINVAL;
		goto error;
	}

	if ((pdata->num_usecases == 0) ||
		(pdata->usecase[0].num_paths == 0)) {
		CAM_ERR(CAM_UTIL, "usecase=%d", pdata->num_usecases);
		rc = -EINVAL;
		goto error;
	}

	client_id = msm_bus_scale_register_client(pdata);
	if (!client_id) {
		CAM_ERR(CAM_UTIL, "failed in register bus client_data");
		rc = -EINVAL;
		goto error;
	}

	bus_client->common_data = common_data;

	bus_client_data->dyn_vote = of_property_read_bool(dev_node,
		"qcom,msm-bus-vector-dyn-vote");

	if (bus_client_data->dyn_vote && (pdata->num_usecases != 2)) {
		CAM_ERR(CAM_UTIL, "Excess or less vectors %d",
			pdata->num_usecases);
		rc = -EINVAL;
		goto fail_unregister_client;
	}

	rc = msm_bus_scale_client_update_request(client_id, 0);
	if (rc) {
		CAM_ERR(CAM_UTIL, "Bus client update request failed, rc = %d",
			rc);
		goto fail_unregister_client;
	}

	bus_client->common_data->src_id = pdata->usecase[0].vectors[0].src;
	bus_client->common_data->dst_id = pdata->usecase[0].vectors[0].dst;
	bus_client_data->pdata = pdata;
	bus_client_data->client_id = client_id;
	bus_client->common_data->num_usecases = pdata->num_usecases;
	bus_client_data->num_paths = pdata->usecase[0].num_paths;
	bus_client->common_data->name = pdata->name;

	CAM_DBG(CAM_PERF, "Register Bus Client=[%d][%s] : src=%d, dst=%d",
		bus_client_data->client_id, bus_client->common_data->name,
		bus_client->common_data->src_id,
		bus_client->common_data->dst_id);

	return 0;
fail_unregister_client:
	msm_bus_scale_unregister_client(bus_client_data->client_id);
error:
	kfree(bus_client_data);
	bus_client->client_data = NULL;
	kfree(bus_client);
	*client = NULL;
end:
	return rc;

}

void cam_soc_bus_client_unregister(void **client)
{
	struct cam_soc_bus_client *bus_client =
		(struct cam_soc_bus_client *) (*client);
	struct cam_soc_bus_client_data *bus_client_data =
		(struct cam_soc_bus_client_data *) bus_client->client_data;

	if (bus_client_data->dyn_vote)
		cam_soc_bus_client_update_bw(bus_client, 0, 0);
	else
		cam_soc_bus_client_update_request(bus_client, 0);

	msm_bus_scale_unregister_client(bus_client_data->client_id);
	kfree(bus_client_data);
	bus_client->client_data = NULL;
	kfree(bus_client);
	*client = NULL;
}
