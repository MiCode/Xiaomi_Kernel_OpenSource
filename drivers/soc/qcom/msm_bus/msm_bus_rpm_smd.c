// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2016, 2018, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "AXI: %s(): " fmt, __func__

#include "msm_bus_core.h"
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <soc/qcom/rpm-smd.h>

/* Stubs for backward compatibility */
void msm_bus_rpm_set_mt_mask(void)
{
}

bool msm_bus_rpm_is_mem_interleaved(void)
{
	return true;
}

struct commit_data {
	struct msm_bus_node_hw_info *mas_arb;
	struct msm_bus_node_hw_info *slv_arb;
};

#ifdef CONFIG_DEBUG_FS
void msm_bus_rpm_fill_cdata_buffer(int *curr, char *buf, const int max_size,
	void *cdata, int nmasters, int nslaves, int ntslaves)
{
	int c;
	struct commit_data *cd = (struct commit_data *)cdata;

	*curr += scnprintf(buf + *curr, max_size - *curr, "\nMas BW:\n");
	for (c = 0; c < nmasters; c++)
		*curr += scnprintf(buf + *curr, max_size - *curr,
			"%d: %llu\t", cd->mas_arb[c].hw_id,
			cd->mas_arb[c].bw);
	*curr += scnprintf(buf + *curr, max_size - *curr, "\nSlave BW:\n");
	for (c = 0; c < nslaves; c++) {
		*curr += scnprintf(buf + *curr, max_size - *curr,
		"%d: %llu\t", cd->slv_arb[c].hw_id,
		cd->slv_arb[c].bw);
	}
}
#endif

static int msm_bus_rpm_compare_cdata(
	struct msm_bus_fabric_registration *fab_pdata,
	struct commit_data *cd1, struct commit_data *cd2)
{
	size_t n;
	int ret;

	n = sizeof(struct msm_bus_node_hw_info) * fab_pdata->nmasters * 2;
	ret = memcmp(cd1->mas_arb, cd2->mas_arb, n);
	if (ret) {
		MSM_BUS_DBG("Master Arb Data not equal\n");
		return ret;
	}

	n = sizeof(struct msm_bus_node_hw_info) * fab_pdata->nslaves * 2;
	ret = memcmp(cd1->slv_arb, cd2->slv_arb, n);
	if (ret) {
		MSM_BUS_DBG("Master Arb Data not equal\n");
		return ret;
	}

	return 0;
}

static int msm_bus_rpm_req(int ctx, uint32_t rsc_type, uint32_t key,
	struct msm_bus_node_hw_info *hw_info, bool valid)
{
	struct msm_rpm_request *rpm_req;
	int ret = 0, msg_id;

	if (ctx == ACTIVE_CTX)
		ctx = MSM_RPM_CTX_ACTIVE_SET;
	else if (ctx == DUAL_CTX)
		ctx = MSM_RPM_CTX_SLEEP_SET;

	rpm_req = msm_rpm_create_request(ctx, rsc_type, hw_info->hw_id, 1);
	if (rpm_req == NULL) {
		MSM_BUS_WARN("RPM: Couldn't create RPM Request\n");
		return -ENXIO;
	}

	if (valid) {
		ret = msm_rpm_add_kvp_data(rpm_req, key, (const uint8_t *)
			&hw_info->bw, (int)(sizeof(uint64_t)));
		if (ret) {
			MSM_BUS_WARN("RPM: Add KVP failed for RPM Req:%u\n",
				rsc_type);
			goto free_rpm_request;
		}

		MSM_BUS_DBG("Added Key: %d, Val: %llu, size: %zu\n", key,
			hw_info->bw, sizeof(uint64_t));
	} else {
		/* Invalidate RPM requests */
		ret = msm_rpm_add_kvp_data(rpm_req, 0, NULL, 0);
		if (ret) {
			MSM_BUS_WARN("RPM: Add KVP failed for RPM Req:%u\n",
				rsc_type);
			goto free_rpm_request;
		}
	}

	msg_id = msm_rpm_send_request(rpm_req);
	if (!msg_id) {
		MSM_BUS_WARN("RPM: No message ID for req\n");
		ret = -ENXIO;
		goto free_rpm_request;
	}

	ret = msm_rpm_wait_for_ack(msg_id);
	if (ret) {
		MSM_BUS_WARN("RPM: Ack failed\n");
		goto free_rpm_request;
	}

free_rpm_request:
	msm_rpm_free_request(rpm_req);

	return ret;
}

static int msm_bus_rpm_commit_arb(struct msm_bus_fabric_registration
	*fab_pdata, int ctx, void *rpm_data,
	struct commit_data *cd, bool valid)
{
	int i, status = 0, rsc_type, key;

	MSM_BUS_DBG("Context: %d\n", ctx);
	rsc_type = RPM_BUS_MASTER_REQ;
	key = RPM_MASTER_FIELD_BW;
	for (i = 0; i < fab_pdata->nmasters; i++) {
		if (!cd->mas_arb[i].dirty)
			continue;

		MSM_BUS_DBG("MAS HWID: %d, BW: %llu DIRTY: %d\n",
			cd->mas_arb[i].hw_id,
			cd->mas_arb[i].bw,
			cd->mas_arb[i].dirty);
		status = msm_bus_rpm_req(ctx, rsc_type, key,
			&cd->mas_arb[i], valid);
		if (status) {
			MSM_BUS_ERR("RPM: Req fail: mas:%d, bw:%llu\n",
				cd->mas_arb[i].hw_id,
				cd->mas_arb[i].bw);
			break;
		}
		cd->mas_arb[i].dirty = false;
	}

	rsc_type = RPM_BUS_SLAVE_REQ;
	key = RPM_SLAVE_FIELD_BW;
	for (i = 0; i < fab_pdata->nslaves; i++) {
		if (!cd->slv_arb[i].dirty)
			continue;

		MSM_BUS_DBG("SLV HWID: %d, BW: %llu DIRTY: %d\n",
			cd->slv_arb[i].hw_id,
			cd->slv_arb[i].bw,
			cd->slv_arb[i].dirty);
		status = msm_bus_rpm_req(ctx, rsc_type, key,
			&cd->slv_arb[i], valid);
		if (status) {
			MSM_BUS_ERR("RPM: Req fail: slv:%d, bw:%llu\n",
				cd->slv_arb[i].hw_id,
				cd->slv_arb[i].bw);
			break;
		}
		cd->slv_arb[i].dirty = false;
	}

	return status;
}

/*
 * msm_bus_remote_hw_commit() - Commit the arbitration data to RPM
 * @fabric: Fabric for which the data should be committed
 */
int msm_bus_remote_hw_commit(struct msm_bus_fabric_registration
	*fab_pdata, void *hw_data, void **cdata)
{

	int ret;
	bool valid;
	struct commit_data *dual_cd, *act_cd;
	void *rpm_data = hw_data;

	MSM_BUS_DBG("\nReached RPM Commit\n");
	dual_cd = (struct commit_data *)cdata[DUAL_CTX];
	act_cd = (struct commit_data *)cdata[ACTIVE_CTX];

	/*
	 * If the arb data for active set and sleep set is
	 * different, commit both sets.
	 * If the arb data for active set and sleep set is
	 * the same, invalidate the sleep set.
	 */
	ret = msm_bus_rpm_compare_cdata(fab_pdata, act_cd, dual_cd);
	if (!ret)
		/* Invalidate sleep set.*/
		valid = false;
	else
		valid = true;

	ret = msm_bus_rpm_commit_arb(fab_pdata, DUAL_CTX, rpm_data,
		dual_cd, valid);
	if (ret)
		MSM_BUS_ERR("Error comiting fabric:%d in %d ctx\n",
			fab_pdata->id, DUAL_CTX);

	valid = true;
	ret = msm_bus_rpm_commit_arb(fab_pdata, ACTIVE_CTX, rpm_data, act_cd,
		valid);
	if (ret)
		MSM_BUS_ERR("Error comiting fabric:%d in %d ctx\n",
			fab_pdata->id, ACTIVE_CTX);

	return ret;
}

int msm_bus_rpm_hw_init(struct msm_bus_fabric_registration *pdata,
	struct msm_bus_hw_algorithm *hw_algo)
{
	if (!pdata->ahb)
		pdata->rpm_enabled = 1;
	return 0;
}
