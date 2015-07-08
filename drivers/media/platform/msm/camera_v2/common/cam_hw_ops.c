/* Copyright (c) 2015 The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "CAM-AHB %s:%d " fmt, __func__, __LINE__
#define TRUE   1
#include <linux/module.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <linux/of_platform.h>
#include "cam_hw_ops.h"

struct cam_ahb_client {
	enum cam_ahb_clk_vote vote;
};

struct cam_bus_vector {
	const char *name;
};

struct cam_ahb_client_data {
	struct msm_bus_scale_pdata *pbus_data;
	u32 ahb_client;
	u32 ahb_clk_state;
	struct msm_bus_vectors *paths;
	struct msm_bus_paths *usecases;
	struct cam_bus_vector *vectors;
	u32 *votes;
	u32 cnt;
	u32 probe_done;
	struct cam_ahb_client clients[CAM_AHB_CLIENT_MAX];
	struct mutex lock;
};

/* Note: The mask array defined here should match
 * the order of strings and number of strings
 * in dtsi bus-vectors
 */

static enum cam_ahb_clk_vote mask[] = {
	CAMERA_AHB_SUSPEND_VOTE,
	CAMERA_AHB_SVS_VOTE,
	CAMERA_AHB_NOMINAL_VOTE,
	CAMERA_AHB_TURBO_VOTE
};

static struct cam_ahb_client_data data;

int get_vector_index(char *name)
{
	int i = 0, rc = -1;

	for (i = 0; i < data.cnt; i++) {
		if (strcmp(name, data.vectors[i].name) == 0)
			return i;
	}

	return rc;
}

int cam_ahb_clk_init(struct platform_device *pdev)
{
	int i = 0, cnt = 0, rc = 0, index = 0;
	struct device_node *of_node;

	if (!pdev) {
		pr_err("invalid pdev argument\n");
		return -EINVAL;
	}

	of_node = pdev->dev.of_node;
	data.cnt = of_property_count_strings(of_node, "bus-vectors");
	if (data.cnt == 0) {
		pr_err("no vectors strings found in device tree, count=%d",
			data.cnt);
		return 0;
	}

	cnt = of_property_count_u32_elems(of_node, "qcom,bus-votes");
	if (cnt == 0) {
		pr_err("no vector values found in device tree, count=%d", cnt);
		return 0;
	}

	if (data.cnt != cnt) {
		pr_err("vector mismatch num of strings=%u, num of values %d\n",
			data.cnt, cnt);
		return -EINVAL;
	}

	pr_debug("number of bus vectors: %d\n", data.cnt);

	data.vectors = devm_kzalloc(&pdev->dev,
		sizeof(struct cam_bus_vector) * cnt,
		GFP_KERNEL);
	if (!data.vectors)
		return -ENOMEM;

	for (i = 0; i < data.cnt; i++) {
		rc = of_property_read_string_index(of_node, "bus-vectors",
				i, &(data.vectors[i].name));
		pr_debug("dbg: names[%d] = %s\n", i, data.vectors[i].name);
		if (rc < 0) {
			pr_err("failed\n");
			rc = -EINVAL;
			goto err1;
		}
	}

	data.paths = devm_kzalloc(&pdev->dev,
		sizeof(struct msm_bus_vectors) * cnt,
		GFP_KERNEL);
	if (!data.paths) {
		rc = -ENOMEM;
		goto err1;
	}

	data.usecases = devm_kzalloc(&pdev->dev,
		sizeof(struct msm_bus_paths) * cnt,
		GFP_KERNEL);
	if (!data.usecases) {
		rc = -ENOMEM;
		goto err2;
	}

	data.pbus_data = devm_kzalloc(&pdev->dev,
		sizeof(struct msm_bus_scale_pdata),
		GFP_KERNEL);
	if (!data.pbus_data) {
		rc = -ENOMEM;
		goto err3;
	}

	data.votes = devm_kzalloc(&pdev->dev, sizeof(u32) * cnt,
		GFP_KERNEL);
	if (!data.votes) {
		rc = -ENOMEM;
		goto err4;
	}

	rc = of_property_read_u32_array(of_node, "qcom,bus-votes",
		data.votes, cnt);

	for (i = 0; i < data.cnt; i++) {
		data.paths[i] = (struct msm_bus_vectors) {
			MSM_BUS_MASTER_AMPSS_M0,
			MSM_BUS_SLAVE_CAMERA_CFG,
			0,
			data.votes[i]
		};
		data.usecases[i] = (struct msm_bus_paths) {
			.num_paths = 1,
			.vectors   = &data.paths[i],
		};
		pr_debug("dbg: votes[%d] = %u\n", i, data.votes[i]);
	}

	*data.pbus_data = (struct msm_bus_scale_pdata) {
		.name = "msm_camera_ahb",
		.num_usecases = data.cnt,
		.usecase = data.usecases,
	};

	data.ahb_client =
		msm_bus_scale_register_client(data.pbus_data);
	if (!data.ahb_client) {
		pr_err("ahb vote registering failed\n");
		rc = -EINVAL;
		goto err5;
	}

	index = get_vector_index("suspend");
	if (index < 0) {
		pr_err("svs vector not supported\n");
		rc = -EINVAL;
		goto err6;
	}

	/* request for svs in init */
	msm_bus_scale_client_update_request(data.ahb_client,
		index);
	data.ahb_clk_state = CAMERA_AHB_SUSPEND_VOTE;
	data.probe_done = TRUE;
	mutex_init(&data.lock);

	pr_debug("dbg, done registering ahb votes\n");
	pr_debug("dbg, clk state :%u, probe :%d\n",
		data.ahb_clk_state, data.probe_done);
	return rc;

err6:
	msm_bus_scale_unregister_client(data.ahb_client);
err5:
	devm_kfree(&pdev->dev, data.votes);
	data.votes = NULL;
err4:
	devm_kfree(&pdev->dev, data.pbus_data);
	data.pbus_data = NULL;
err3:
	devm_kfree(&pdev->dev, data.usecases);
	data.usecases = NULL;
err2:
	devm_kfree(&pdev->dev, data.paths);
	data.paths = NULL;
err1:
	devm_kfree(&pdev->dev, data.vectors);
	data.vectors = NULL;
	return rc;
}
EXPORT_SYMBOL(cam_ahb_clk_init);

int cam_config_ahb_clk(enum cam_ahb_clk_client id, enum cam_ahb_clk_vote vote)
{
	int i = 0, n = 0;
	u32 final_vote = 0;

	if (data.probe_done != TRUE) {
		pr_err("ahb init is not done yet\n");
		return -EINVAL;
	}

	if (vote > CAMERA_AHB_TURBO_VOTE || id >= CAM_AHB_CLIENT_MAX) {
		pr_err("err: invalid argument\n");
		return -EINVAL;
	}

	pr_debug("dbg: id :%u, vote : %u\n", id, vote);
	data.clients[id].vote = vote;

	mutex_lock(&data.lock);

	if (vote == data.ahb_clk_state) {
		pr_debug("dbg: already at desired vote\n");
		mutex_unlock(&data.lock);
		return 0;
	}

	/* oring all the client votes */
	for (i = 0; i < CAM_AHB_CLIENT_MAX; i++)
		final_vote |= data.clients[i].vote;

	pr_debug("dbg: final vote : %u\n", final_vote);
	/* find the max client vote */
	for (n = data.cnt - 1; n >= 0; n--) {
		if (!(final_vote & mask[n]))
			continue;
		else
			break;
	}

	if (n >= 0) {
		if (mask[n] != data.ahb_clk_state) {
			msm_bus_scale_client_update_request(data.ahb_client, n);
			data.ahb_clk_state = mask[n];
			pr_debug("dbg: state : %u, vote : %d\n",
				data.ahb_clk_state, n);
		}
	} else {
		pr_err("err: no bus vector found\n");
		return -EINVAL;
	}
	mutex_unlock(&data.lock);
	return 0;
}
EXPORT_SYMBOL(cam_config_ahb_clk);
