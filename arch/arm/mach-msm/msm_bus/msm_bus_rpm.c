/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/module.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include <mach/board.h>
#include <mach/rpm.h>
#include "msm_bus_core.h"

struct commit_data {
	uint16_t *bwsum;
	uint16_t *arb;
	unsigned long *actarb;
};


/*
 * The following macros are used for various operations on commit data.
 * Commit data is an array of 32 bit integers. The size of arrays is unique
 * to the fabric. Commit arrays are allocated at run-time based on the number
 * of masters, slaves and tiered-slaves registered.
 */

#define MSM_BUS_GET_BW_INFO(val, type, bw) \
	do { \
		(type) = MSM_BUS_GET_BW_TYPE(val); \
		(bw) = MSM_BUS_GET_BW(val);	\
	} while (0)


#define MSM_BUS_GET_BW_INFO_BYTES (val, type, bw) \
	do { \
		(type) = MSM_BUS_GET_BW_TYPE(val); \
		(bw) = msm_bus_get_bw_bytes(val); \
	} while (0)

#define ROUNDED_BW_VAL_FROM_BYTES(bw) \
	((((bw) >> 17) + 1) & 0x8000 ? 0x7FFF : (((bw) >> 17) + 1))

#define BW_VAL_FROM_BYTES(bw) \
	((((bw) >> 17) & 0x8000) ? 0x7FFF : ((bw) >> 17))

uint32_t msm_bus_set_bw_bytes(unsigned long bw)
{
	return ((((bw) & 0x1FFFF) && (((bw) >> 17) == 0)) ?
		ROUNDED_BW_VAL_FROM_BYTES(bw) : BW_VAL_FROM_BYTES(bw));

}

uint64_t msm_bus_get_bw_bytes(unsigned long val)
{
	return ((val) & 0x7FFF) << 17;
}

uint16_t msm_bus_get_bw(unsigned long val)
{
	return (val)&0x7FFF;
}

uint16_t msm_bus_create_bw_tier_pair_bytes(uint8_t type, unsigned long bw)
{
	return ((((type) == MSM_BUS_BW_TIER1 ? 1 : 0) << 15) |
	 (msm_bus_set_bw_bytes(bw)));
};

uint16_t msm_bus_create_bw_tier_pair(uint8_t type, unsigned long bw)
{
	return (((type) == MSM_BUS_BW_TIER1 ? 1 : 0) << 15) | ((bw) & 0x7FFF);
}

void msm_bus_rpm_fill_cdata_buffer(int *curr, char *buf, const int max_size,
	void *cdata, int nmasters, int nslaves, int ntslaves)
{
	int j, c;
	struct commit_data *cd = (struct commit_data *)cdata;

	*curr += scnprintf(buf + *curr, max_size - *curr, "BWSum:\n");
	for (c = 0; c < nslaves; c++)
		*curr += scnprintf(buf + *curr, max_size - *curr,
			"0x%x\t", cd->bwsum[c]);
	*curr += scnprintf(buf + *curr, max_size - *curr, "\nArb:");
	for (c = 0; c < ntslaves; c++) {
		*curr += scnprintf(buf + *curr, max_size - *curr,
		"\nTSlave %d:\n", c);
		for (j = 0; j < nmasters; j++)
			*curr += scnprintf(buf + *curr, max_size - *curr,
				" 0x%x\t", cd->arb[(c * nmasters) + j]);
	}
}

/**
 * allocate_commit_data() - Allocate the data for commit array in the
 * format specified by RPM
 * @fabric: Fabric device for which commit data is allocated
 */
int allocate_commit_data(struct msm_bus_fabric_registration *fab_pdata,
	void **cdata)
{
	struct commit_data **cd = (struct commit_data **)cdata;
	*cd = kzalloc(sizeof(struct commit_data), GFP_KERNEL);
	if (!*cd) {
		MSM_FAB_DBG("Couldn't alloc mem for cdata\n");
		return -ENOMEM;
	}
	(*cd)->bwsum = kzalloc((sizeof(uint16_t) * fab_pdata->nslaves),
			GFP_KERNEL);
	if (!(*cd)->bwsum) {
		MSM_FAB_DBG("Couldn't alloc mem for slaves\n");
		kfree(*cd);
		return -ENOMEM;
	}
	(*cd)->arb = kzalloc(((sizeof(uint16_t *)) *
		(fab_pdata->ntieredslaves * fab_pdata->nmasters) + 1),
		GFP_KERNEL);
	if (!(*cd)->arb) {
		MSM_FAB_DBG("Couldn't alloc memory for"
				" slaves\n");
		kfree((*cd)->bwsum);
		kfree(*cd);
		return -ENOMEM;
	}
	(*cd)->actarb = kzalloc(((sizeof(unsigned long *)) *
		(fab_pdata->ntieredslaves * fab_pdata->nmasters) + 1),
		GFP_KERNEL);
	if (!(*cd)->actarb) {
		MSM_FAB_DBG("Couldn't alloc memory for"
				" slaves\n");
		kfree((*cd)->bwsum);
		kfree((*cd)->arb);
		kfree(*cd);
		return -ENOMEM;
	}

	return 0;
}

void free_commit_data(void *cdata)
{
	struct commit_data *cd = (struct commit_data *)cdata;

	kfree(cd->bwsum);
	kfree(cd->arb);
	kfree(cd->actarb);
	kfree(cd);
}

/**
 * allocate_rpm_data() - Allocate the id-value pairs to be
 * sent to RPM
 */
struct msm_rpm_iv_pair *allocate_rpm_data(struct msm_bus_fabric_registration
	*fab_pdata)
{
	struct msm_rpm_iv_pair *rpm_data;
	uint16_t count = ((fab_pdata->nmasters * fab_pdata->ntieredslaves) +
		fab_pdata->nslaves + 1)/2;

	rpm_data = kmalloc((sizeof(struct msm_rpm_iv_pair) * count),
		GFP_KERNEL);
	return rpm_data;
}

#define BWMASK 0x7FFF
#define TIERMASK 0x8000
#define GET_TIER(n) (((n) & TIERMASK) >> 15)

void msm_bus_rpm_update_bw(struct msm_bus_inode_info *hop,
	struct msm_bus_inode_info *info,
	struct msm_bus_fabric_registration *fab_pdata,
	void *sel_cdata, int *master_tiers,
	long int add_bw)
{
	int index, i, j;
	struct commit_data *sel_cd = (struct commit_data *)sel_cdata;

	add_bw /= info->node_info->num_mports;
	for (i = 0; i < hop->node_info->num_tiers; i++) {
		for (j = 0; j < info->node_info->num_mports; j++) {

			uint16_t hop_tier;
			/*
			 * For interleaved gateway ports and slave ports,
			 * there is one-one mapping between gateway port and
			 * the slave port
			 */
			if (info->node_info->gateway && i != j &&
				(hop->node_info->num_sports > 1))
				continue;

			if (!hop->node_info->tier)
				hop_tier = MSM_BUS_BW_TIER2 - 1;
			else
				hop_tier = hop->node_info->tier[i] - 1;
			index = ((hop_tier * fab_pdata->nmasters) +
				(info->node_info->masterp[j]));
			/* If there is tier, calculate arb for commit */
			if (hop->node_info->tier) {
				uint16_t tier;
				unsigned long tieredbw = sel_cd->actarb[index];
				if (GET_TIER(sel_cd->arb[index]))
					tier = MSM_BUS_BW_TIER1;
				else if (master_tiers)
					/*
					 * By default master is only in the
					 * tier specified by default.
					 * To change the default tier, client
					 * needs to explicitly request for a
					 * different supported tier */
					tier = master_tiers[0];
				else
					tier = MSM_BUS_BW_TIER2;

				/*
				 * Make sure gateway to slave port bandwidth
				 * is not divided when slave is interleaved
				 */
				if (info->node_info->gateway
					&& hop->node_info->num_sports > 1)
					tieredbw += add_bw;
				else
					tieredbw += add_bw/
						hop->node_info->num_sports;

				/* If bw is 0, update tier to default */
				if (!tieredbw)
					tier = MSM_BUS_BW_TIER2;
				/* Update Arb for fab,get HW Mport from enum */
				sel_cd->arb[index] =
					msm_bus_create_bw_tier_pair_bytes(tier,
					tieredbw);
				sel_cd->actarb[index] = tieredbw;
				MSM_BUS_DBG("tier:%d mport: %d tiered_bw:%ld "
				"bwsum: %ld\n", hop_tier, info->node_info->
				masterp[i], tieredbw, *hop->link_info.sel_bw);
			}
		}
	}

	/* Update bwsum for slaves on fabric */
	for (i = 0; i < hop->node_info->num_sports; i++) {
		sel_cd->bwsum[hop->node_info->slavep[i]]
			= (uint16_t)msm_bus_create_bw_tier_pair_bytes(0,
			(*hop->link_info.sel_bw/hop->node_info->num_sports));
		MSM_BUS_DBG("slavep:%d, link_bw: %ld\n",
			hop->node_info->slavep[i], (*hop->link_info.sel_bw/
			hop->node_info->num_sports));
	}
}

#define RPM_SHIFT_VAL 16
#define RPM_SHIFT(n) ((n) << RPM_SHIFT_VAL)
/**
 * msm_bus_rpm_commit() - Commit the arbitration data to RPM
 * @fabric: Fabric for which the data should be committed
 * */
int msm_bus_rpm_commit(struct msm_bus_fabric_registration
	*fab_pdata, int ctx, struct msm_rpm_iv_pair *rpm_data,
	void *cdata)
{
	int i, j, offset = 0, status = 0, count, index = 0;
	struct commit_data *cd = (struct commit_data *)cdata;
	/*
	 * count is the number of 2-byte words required to commit the
	 * data to rpm. This is calculated by the following formula.
	 * Commit data is split into two arrays:
	 * 1. arb[nmasters * ntieredslaves]
	 * 2. bwsum[nslaves]
	 */
	count = ((fab_pdata->nmasters * fab_pdata->ntieredslaves)
		+ (fab_pdata->nslaves) + 1)/2;

	offset = fab_pdata->offset;

	/*
	 * Copy bwsum to rpm data
	 * Since bwsum is uint16, the values need to be adjusted to
	 * be copied to value field of rpm-data, which is 32 bits.
	 */
	for (i = 0; i < (fab_pdata->nslaves - 1); i += 2) {
		rpm_data[index].id = offset + index;
		rpm_data[index].value = RPM_SHIFT(*(cd->bwsum + i + 1)) |
			*(cd->bwsum + i);
		index++;
	}
	/* Account for odd number of slaves */
	if (fab_pdata->nslaves & 1) {
		rpm_data[index].id = offset + index;
		rpm_data[index].value = *(cd->arb);
		rpm_data[index].value = RPM_SHIFT(rpm_data[index].value) |
			*(cd->bwsum + i);
		index++;
		i = 1;
	} else
		i = 0;

	/* Copy arb values to rpm data */
	for (; i < (fab_pdata->ntieredslaves * fab_pdata->nmasters);
		i += 2) {
		rpm_data[index].id = offset + index;
		rpm_data[index].value = RPM_SHIFT(*(cd->arb + i + 1)) |
			*(cd->arb + i);
		index++;
	}

	MSM_FAB_DBG("rpm data for fab: %d\n", fab_pdata->id);
	for (i = 0; i < count; i++)
		MSM_FAB_DBG("%d %x\n", rpm_data[i].id, rpm_data[i].value);

	MSM_FAB_DBG("Commit Data: Fab: %d BWSum:\n", fab_pdata->id);
	for (i = 0; i < fab_pdata->nslaves; i++)
		MSM_FAB_DBG("fab_slaves:0x%x\n", cd->bwsum[i]);
	MSM_FAB_DBG("Commit Data: Fab: %d Arb:\n", fab_pdata->id);
	for (i = 0; i < fab_pdata->ntieredslaves; i++) {
		MSM_FAB_DBG("tiered-slave: %d\n", i);
		for (j = 0; j < fab_pdata->nmasters; j++)
			MSM_FAB_DBG(" 0x%x\n",
			cd->arb[(i * fab_pdata->nmasters) + j]);
	}

	MSM_FAB_DBG("calling msm_rpm_set:  %d\n", status);
	msm_bus_dbg_commit_data(fab_pdata->name, cd, fab_pdata->
		nmasters, fab_pdata->nslaves, fab_pdata->ntieredslaves,
		MSM_BUS_DBG_OP);
	if (fab_pdata->rpm_enabled) {
		if (ctx == ACTIVE_CTX)
			status = msm_rpm_set(MSM_RPM_CTX_SET_0, rpm_data,
				count);
	}

	MSM_FAB_DBG("msm_rpm_set returned: %d\n", status);
	return status;
}

