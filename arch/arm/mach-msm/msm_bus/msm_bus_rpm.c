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
#include "../rpm_resources.h"

#define INTERLEAVED_BW(fab_pdata, bw, ports) \
	((fab_pdata->il_flag) ? ((bw) / (ports)) : (bw))
#define INTERLEAVED_VAL(fab_pdata, n) \
	((fab_pdata->il_flag) ? (n) : 1)

void msm_bus_rpm_set_mt_mask()
{
#ifdef CONFIG_MSM_BUS_RPM_MULTI_TIER_ENABLED
	struct msm_rpm_iv_pair mt[1];
	int mask = MSM_RPMRS_MASK_RPM_CTL_MULTI_TIER;
	mt[0].id = MSM_RPM_ID_RPM_CTL;
	mt[0].value = 2;
	msm_rpmrs_set_bits_noirq(MSM_RPM_CTX_SET_0, mt, 1,
		&mask);
#endif
}

bool msm_bus_rpm_is_mem_interleaved(void)
{
	int status = 0;
	struct msm_rpm_iv_pair il[2];
	uint16_t id[2];

	il[0].value = 0;
	il[1].value = 0;
	status = msm_bus_board_rpm_get_il_ids(id);
	if (status) {
		MSM_BUS_DBG("Dynamic check not supported, "
			"default: Interleaved memory\n");
		goto inter;
	}

	il[0].id = id[0];
	il[1].id = id[1];
	status = msm_rpm_get_status(il, ARRAY_SIZE(il));
	if (status) {
		MSM_BUS_ERR("Status read for interleaving returned: %d\n"
			"Using interleaved memory by default\n",
			status);
		goto inter;
	}

	/*
	 * If the start address of EBI1-CH0 is the same as
	 * the start address of EBI1-CH1, the memory is interleaved.
	 * The start addresses are stored in the 16 MSBs of the status
	 * register
	 */
	if ((il[0].value & 0xFFFF0000) != (il[1].value & 0xFFFF0000)) {
		MSM_BUS_DBG("Non-interleaved memory\n");
		return false;
	}

inter:
	MSM_BUS_DBG("Interleaved memory\n");
	return true;
}

#ifndef CONFIG_MSM_BUS_RPM_MULTI_TIER_ENABLED
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
		MSM_BUS_DBG("Couldn't alloc mem for cdata\n");
		return -ENOMEM;
	}
	(*cd)->bwsum = kzalloc((sizeof(uint16_t) * fab_pdata->nslaves),
			GFP_KERNEL);
	if (!(*cd)->bwsum) {
		MSM_BUS_DBG("Couldn't alloc mem for slaves\n");
		kfree(*cd);
		return -ENOMEM;
	}
	(*cd)->arb = kzalloc(((sizeof(uint16_t *)) *
		(fab_pdata->ntieredslaves * fab_pdata->nmasters) + 1),
		GFP_KERNEL);
	if (!(*cd)->arb) {
		MSM_BUS_DBG("Couldn't alloc memory for"
				" slaves\n");
		kfree((*cd)->bwsum);
		kfree(*cd);
		return -ENOMEM;
	}
	(*cd)->actarb = kzalloc(((sizeof(unsigned long *)) *
		(fab_pdata->ntieredslaves * fab_pdata->nmasters) + 1),
		GFP_KERNEL);
	if (!(*cd)->actarb) {
		MSM_BUS_DBG("Couldn't alloc memory for"
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
	int index, i, j, tiers, ports;
	struct commit_data *sel_cd = (struct commit_data *)sel_cdata;

	add_bw = INTERLEAVED_BW(fab_pdata, add_bw, info->node_info->num_mports);
	ports = INTERLEAVED_VAL(fab_pdata, info->node_info->num_mports);
	tiers = INTERLEAVED_VAL(fab_pdata, hop->node_info->num_tiers);
	for (i = 0; i < tiers; i++) {
		for (j = 0; j < ports; j++) {
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
					tieredbw += INTERLEAVED_BW(fab_pdata,
						add_bw, hop->node_info->
						num_sports);

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
	ports = INTERLEAVED_VAL(fab_pdata, hop->node_info->num_sports);
	for (i = 0; i < ports; i++) {
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
static int msm_bus_rpm_compare_cdata(
	struct msm_bus_fabric_registration *fab_pdata,
	struct commit_data *cd1, struct commit_data *cd2)
{
	size_t n;
	int ret;
	n = sizeof(uint16_t) * fab_pdata->nslaves;
	ret = memcmp(cd1->bwsum, cd2->bwsum, n);
	if (ret) {
		MSM_BUS_DBG("Commit Data bwsum not equal\n");
		return ret;
	}

	n = sizeof(uint16_t *) * ((fab_pdata->ntieredslaves *
		fab_pdata->nmasters) + 1);
	ret = memcmp(cd1->arb, cd2->arb, n);
	if (ret) {
		MSM_BUS_DBG("Commit Data arb[%d] not equal\n", i);
		return ret;
	}

	return 0;
}

static int msm_bus_rpm_commit_arb(struct msm_bus_fabric_registration
	*fab_pdata, int ctx, struct msm_rpm_iv_pair *rpm_data,
	struct commit_data *cd, bool valid)
{
	int i, j, offset = 0, status = 0, count, index = 0;
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

	MSM_BUS_DBG("rpm data for fab: %d\n", fab_pdata->id);
	for (i = 0; i < count; i++)
		MSM_BUS_DBG("%d %x\n", rpm_data[i].id, rpm_data[i].value);

	MSM_BUS_DBG("Commit Data: Fab: %d BWSum:\n", fab_pdata->id);
	for (i = 0; i < fab_pdata->nslaves; i++)
		MSM_BUS_DBG("fab_slaves:0x%x\n", cd->bwsum[i]);
	MSM_BUS_DBG("Commit Data: Fab: %d Arb:\n", fab_pdata->id);
	for (i = 0; i < fab_pdata->ntieredslaves; i++) {
		MSM_BUS_DBG("tiered-slave: %d\n", i);
		for (j = 0; j < fab_pdata->nmasters; j++)
			MSM_BUS_DBG(" 0x%x\n",
			cd->arb[(i * fab_pdata->nmasters) + j]);
	}

	MSM_BUS_DBG("calling msm_rpm_set:  %d\n", status);
	msm_bus_dbg_commit_data(fab_pdata->name, cd, fab_pdata->
		nmasters, fab_pdata->nslaves, fab_pdata->ntieredslaves,
		MSM_BUS_DBG_OP);
	if (fab_pdata->rpm_enabled) {
		if (valid) {
			if (ctx == ACTIVE_CTX) {
				status = msm_rpm_set(MSM_RPM_CTX_SET_0,
					rpm_data, count);
				MSM_BUS_DBG("msm_rpm_set returned: %d\n",
					status);
			} else if (ctx == DUAL_CTX) {
				status = msm_rpm_set(MSM_RPM_CTX_SET_SLEEP,
					rpm_data, count);
				MSM_BUS_DBG("msm_rpm_set returned: %d\n",
					status);
			}
		} else {
			if (ctx == ACTIVE_CTX) {
				status = msm_rpm_clear(MSM_RPM_CTX_SET_0,
					rpm_data, count);
				MSM_BUS_DBG("msm_rpm_clear returned: %d\n",
					status);
			} else if (ctx == DUAL_CTX) {
				status = msm_rpm_clear(MSM_RPM_CTX_SET_SLEEP,
					rpm_data, count);
				MSM_BUS_DBG("msm_rpm_clear returned: %d\n",
					status);
			}
		}
	}

	return status;
}

#else

#define NUM_TIERS 2
#define RPM_SHIFT24(n) ((n) << 24)
#define RPM_SHIFT16(n) ((n) << 16)
#define RPM_SHIFT8(n) ((n) << 8)
struct commit_data {
	uint16_t *bwsum;
	uint8_t *arb[NUM_TIERS];
	unsigned long *actarb[NUM_TIERS];
};

#define MODE_BIT(val) ((val) & 0x80)
#define MODE0_IMM(val) ((val) & 0xF)
#define MODE0_SHIFT(val) (((val) & 0x70) >> 4)
#define MODE1_STEP	48 /* 48 MB */
#define MODE1_OFFSET	512 /* 512 MB */
#define MODE1_IMM(val)	((val) & 0x7F)
#define __CLZ(x) ((8 * sizeof(uint32_t)) - 1 - __fls(x))

uint8_t msm_bus_set_bw_bytes(unsigned long val)
{
	unsigned int shift;
	unsigned int intVal;
	unsigned char result;

	/* Convert to MB */
	intVal = (unsigned int)((val + ((1 << 20) - 1)) >> 20);
	/**
	 * Divide by 2^20 and round up
	 * A value graeter than 0x1E0 will round up to 512 and overflow
	 * Mode 0 so it should be made Mode 1
	 */
	if (0x1E0 > intVal) {
		/**
		 * MODE 0
		 * Compute the shift value
		 * Shift value is 32 - the number of leading zeroes -
		 * 4 to save the most significant 4 bits of the value
		 */
		shift = 32 - 4 - min((uint8_t)28, (uint8_t)__CLZ(intVal));

		/* Add min value - 1 to force a round up when shifting right */
		intVal += (1 << shift) - 1;

		/* Recompute the shift value in case there was an overflow */
		shift = 32 - 4 - min((uint8_t)28, (uint8_t)__CLZ(intVal));

		/* Clear the mode bit (msb) and fill in the fields */
		result = ((0x70 & (shift << 4)) |
			(0x0F & (intVal >> shift)));
	} else {
		/* MODE 1 */
		result = (unsigned char)(0x80 |
			((intVal - MODE1_OFFSET + MODE1_STEP - 1) /
			MODE1_STEP));
	}

	return result;
}

uint64_t msm_bus_get_bw(unsigned long val)
{
	return MODE_BIT(val) ?
	 /* Mode 1 */
	 (MODE1_IMM(val) * MODE1_STEP + MODE1_OFFSET) :
	 /* Mode 0 */
	 (MODE0_IMM(val) << MODE0_SHIFT(val));
}

uint64_t msm_bus_get_bw_bytes(unsigned long val)
{
	return msm_bus_get_bw(val) << 20;
}

uint8_t msm_bus_create_bw_tier_pair_bytes(uint8_t type, unsigned long bw)
{
	return msm_bus_set_bw_bytes(bw);
};

uint8_t msm_bus_create_bw_tier_pair(uint8_t type, unsigned long bw)
{
	return msm_bus_create_bw_tier_pair_bytes(type, bw);
};

int allocate_commit_data(struct msm_bus_fabric_registration *fab_pdata,
	void **cdata)
{
	struct commit_data **cd = (struct commit_data **)cdata;
	int i;

	*cd = kzalloc(sizeof(struct commit_data), GFP_KERNEL);
	if (!*cdata) {
		MSM_BUS_DBG("Couldn't alloc mem for cdata\n");
		goto cdata_err;
	}

	(*cd)->bwsum = kzalloc((sizeof(uint16_t) * fab_pdata->nslaves),
			GFP_KERNEL);
	if (!(*cd)->bwsum) {
		MSM_BUS_DBG("Couldn't alloc mem for slaves\n");
		goto bwsum_err;
	}

	for (i = 0; i < NUM_TIERS; i++) {
		(*cd)->arb[i] = kzalloc(((sizeof(uint8_t *)) *
			(fab_pdata->ntieredslaves * fab_pdata->nmasters) + 1),
			GFP_KERNEL);
		if (!(*cd)->arb[i]) {
			MSM_BUS_DBG("Couldn't alloc memory for"
				" slaves\n");
			goto arb_err;
		}

		(*cd)->actarb[i] = kzalloc(((sizeof(unsigned long *)) *
			(fab_pdata->ntieredslaves * fab_pdata->nmasters) + 1),
			GFP_KERNEL);
		if (!(*cd)->actarb[i]) {
			MSM_BUS_DBG("Couldn't alloc memory for"
					" slaves\n");
			kfree((*cd)->arb[i]);
			goto arb_err;
		}
	}


	return 0;

arb_err:
	for (i = i - 1; i >= 0; i--) {
		kfree((*cd)->arb[i]);
		kfree((*cd)->actarb[i]);
	}
bwsum_err:
	kfree((*cd)->bwsum);
cdata_err:
	kfree(*cd);
	return -ENOMEM;
}

void free_commit_data(void *cdata)
{
	int i;
	struct commit_data *cd = (struct commit_data *)cdata;
	kfree(cd->bwsum);
	for (i = 0; i < NUM_TIERS; i++) {
		kfree(cd->arb[i]);
		kfree(cd->actarb[i]);
	}
	kfree(cd);
}

struct msm_rpm_iv_pair *allocate_rpm_data(struct msm_bus_fabric_registration
	*fab_pdata)
{
	struct msm_rpm_iv_pair *rpm_data;
	uint16_t count = (((fab_pdata->nmasters * fab_pdata->ntieredslaves *
		NUM_TIERS)/2) + fab_pdata->nslaves + 1)/2;

	rpm_data = kmalloc((sizeof(struct msm_rpm_iv_pair) * count),
		GFP_KERNEL);
	return rpm_data;
}

static int msm_bus_rpm_compare_cdata(
	struct msm_bus_fabric_registration *fab_pdata,
	struct commit_data *cd1, struct commit_data *cd2)
{
	size_t n;
	int i, ret;
	n = sizeof(uint16_t) * fab_pdata->nslaves;
	ret = memcmp(cd1->bwsum, cd2->bwsum, n);
	if (ret) {
		MSM_BUS_DBG("Commit Data bwsum not equal\n");
		return ret;
	}

	n = sizeof(uint8_t *) * ((fab_pdata->ntieredslaves *
		fab_pdata->nmasters) + 1);
	for (i = 0; i < NUM_TIERS; i++) {
		ret = memcmp(cd1->arb[i], cd2->arb[i], n);
		if (ret) {
			MSM_BUS_DBG("Commit Data arb[%d] not equal\n", i);
			return ret;
		}
	}

	return 0;
}

static int msm_bus_rpm_commit_arb(struct msm_bus_fabric_registration
	*fab_pdata, int ctx, struct msm_rpm_iv_pair *rpm_data,
	struct commit_data *cd, bool valid)
{
	int i, j, k, offset = 0, status = 0, count, index = 0;
	/*
	 * count is the number of 2-byte words required to commit the
	 * data to rpm. This is calculated by the following formula.
	 * Commit data is split into two arrays:
	 * 1. arb[nmasters * ntieredslaves][num_tiers]
	 * 2. bwsum[nslaves]
	 */
	count = (((fab_pdata->nmasters * fab_pdata->ntieredslaves * NUM_TIERS)
		/2) + fab_pdata->nslaves + 1)/2;

	offset = fab_pdata->offset;

	/*
	 * Copy bwsum to rpm data
	 * Since bwsum is uint16, the values need to be adjusted to
	 * be copied to value field of rpm-data, which is 32 bits.
	 */
	for (i = 0; i < (fab_pdata->nslaves - 1); i += 2) {
		rpm_data[index].id = offset + index;
		rpm_data[index].value = RPM_SHIFT16(*(cd->bwsum + i + 1)) |
			*(cd->bwsum + i);
		index++;
	}
	/* Account for odd number of slaves */
	if (fab_pdata->nslaves & 1) {
		rpm_data[index].id = offset + index;
		rpm_data[index].value = RPM_SHIFT8(*cd->arb[1]) |
			*(cd->arb[0]);
		rpm_data[index].value = RPM_SHIFT16(rpm_data[index].value) |
			*(cd->bwsum + i);
		index++;
		i = 1;
	} else
		i = 0;

	/* Copy arb values to rpm data */
	for (; i < (fab_pdata->ntieredslaves * fab_pdata->nmasters);
		i += 2) {
		uint16_t tv1, tv0;
		rpm_data[index].id = offset + index;
		tv0 = RPM_SHIFT8(*(cd->arb[1] + i)) | (*(cd->arb[0] + i));
		tv1 = RPM_SHIFT8(*(cd->arb[1] + i + 1)) | (*(cd->arb[0] + i
			+ 1));
		rpm_data[index].value = RPM_SHIFT16(tv1) | tv0;
			index++;
	}

	MSM_BUS_DBG("rpm data for fab: %d\n", fab_pdata->id);
	for (i = 0; i < count; i++)
		MSM_BUS_DBG("%d %x\n", rpm_data[i].id, rpm_data[i].value);

	MSM_BUS_DBG("Commit Data: Fab: %d BWSum:\n", fab_pdata->id);
	for (i = 0; i < fab_pdata->nslaves; i++)
		MSM_BUS_DBG("fab_slaves:0x%x\n", cd->bwsum[i]);
	MSM_BUS_DBG("Commit Data: Fab: %d Arb:\n", fab_pdata->id);
	for (k = 0; k < NUM_TIERS; k++) {
		MSM_BUS_DBG("Tier: %d\n", k);
		for (i = 0; i < fab_pdata->ntieredslaves; i++) {
			MSM_BUS_DBG("tiered-slave: %d\n", i);
			for (j = 0; j < fab_pdata->nmasters; j++)
				MSM_BUS_DBG(" 0x%x\n",
				cd->arb[k][(i * fab_pdata->nmasters)
				+ j]);
		}
	}

	MSM_BUS_DBG("calling msm_rpm_set:  %d\n", status);
	msm_bus_dbg_commit_data(fab_pdata->name, (void *)cd, fab_pdata->
		nmasters, fab_pdata->nslaves, fab_pdata->ntieredslaves,
		MSM_BUS_DBG_OP);
	if (fab_pdata->rpm_enabled) {
		if (valid) {
			if (ctx == ACTIVE_CTX) {
				status = msm_rpm_set(MSM_RPM_CTX_SET_0,
					rpm_data, count);
				MSM_BUS_DBG("msm_rpm_set returned: %d\n",
					status);
			} else if (ctx == DUAL_CTX) {
				status = msm_rpm_set(MSM_RPM_CTX_SET_SLEEP,
					rpm_data, count);
				MSM_BUS_DBG("msm_rpm_set returned: %d\n",
					status);
			}
		} else {
			if (ctx == ACTIVE_CTX) {
				status = msm_rpm_clear(MSM_RPM_CTX_SET_0,
					rpm_data, count);
				MSM_BUS_DBG("msm_rpm_clear returned: %d\n",
					status);
			} else if (ctx == DUAL_CTX) {
				status = msm_rpm_clear(MSM_RPM_CTX_SET_SLEEP,
					rpm_data, count);
				MSM_BUS_DBG("msm_rpm_clear returned: %d\n",
					status);
			}
		}
	}

	return status;
}

#define FORMAT_BW(x) \
	((x < 0) ? \
	-(msm_bus_get_bw_bytes(msm_bus_create_bw_tier_pair_bytes(0, -(x)))) : \
	(msm_bus_get_bw_bytes(msm_bus_create_bw_tier_pair_bytes(0, x))))

uint16_t msm_bus_pack_bwsum_bytes(unsigned long bw)
{
	return (bw + ((1 << 20) - 1)) >> 20;
};

void msm_bus_rpm_update_bw(struct msm_bus_inode_info *hop,
	struct msm_bus_inode_info *info,
	struct msm_bus_fabric_registration *fab_pdata,
	void *sel_cdata, int *master_tiers,
	long int add_bw)
{
	int index, i, j, tiers, ports;
	struct commit_data *sel_cd = (struct commit_data *)sel_cdata;

	add_bw = INTERLEAVED_BW(fab_pdata, add_bw, info->node_info->num_mports);
	ports = INTERLEAVED_VAL(fab_pdata, info->node_info->num_mports);
	tiers = INTERLEAVED_VAL(fab_pdata, hop->node_info->num_tiers);
	for (i = 0; i < tiers; i++) {
		for (j = 0; j < ports; j++) {
			uint16_t hop_tier;
			/*
			 * For interleaved gateway ports and slave ports,
			 * there is one-one mapping between gateway port and
			 * the slave port
			 */
			if (info->node_info->gateway && i != j
				&& hop->node_info->num_sports > 1)
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
				unsigned long tieredbw;
				if (master_tiers)
					tier = master_tiers[0] - 1;
				else
					tier = MSM_BUS_BW_TIER2 - 1;

				tieredbw = sel_cd->actarb[tier][index];
				/*
				 * Make sure gateway to slave port bandwidth
				 * is not divided when slave is interleaved
				 */
				if (info->node_info->gateway
					&& hop->node_info->num_sports > 1)
					tieredbw += add_bw;
				else
					tieredbw += INTERLEAVED_BW(fab_pdata,
						add_bw, hop->node_info->
						num_sports);

				/* Update Arb for fab,get HW Mport from enum */
				sel_cd->arb[tier][index] =
				msm_bus_create_bw_tier_pair_bytes(0, tieredbw);
				sel_cd->actarb[tier][index] = tieredbw;
				MSM_BUS_DBG("tier:%d mport: %d tiered_bw:%lu "
				"bwsum: %ld\n", hop_tier, info->node_info->
				masterp[i], tieredbw, *hop->link_info.sel_bw);
			}
		}
	}

	/* Update bwsum for slaves on fabric */

	ports = INTERLEAVED_VAL(fab_pdata, hop->node_info->num_sports);
	for (i = 0; i < ports; i++) {
		sel_cd->bwsum[hop->node_info->slavep[i]]
			= msm_bus_pack_bwsum_bytes((*hop->link_info.
			sel_bw/hop->node_info->num_sports));
		MSM_BUS_DBG("slavep:%d, link_bw: %ld\n",
			hop->node_info->slavep[i], (*hop->link_info.sel_bw/
			hop->node_info->num_sports));
	}
}


void msm_bus_rpm_fill_cdata_buffer(int *curr, char *buf, const int max_size,
	void *cdata, int nmasters, int nslaves, int ntslaves)
{
	int j, k, c;
	struct commit_data *cd = (struct commit_data *)cdata;

	*curr += scnprintf(buf + *curr, max_size - *curr, "BWSum:\n");
	for (c = 0; c < nslaves; c++)
		*curr += scnprintf(buf + *curr, max_size - *curr,
			"0x%x\t", cd->bwsum[c]);
	*curr += scnprintf(buf + *curr, max_size - *curr, "\nArb:");
	for (k = 0; k < NUM_TIERS; k++) {
		*curr += scnprintf(buf + *curr, max_size - *curr,
			"\nTier %d:\n", k);
		for (c = 0; c < ntslaves; c++) {
			*curr += scnprintf(buf + *curr, max_size - *curr,
			"TSlave %d:\n", c);
			for (j = 0; j < nmasters; j++)
				*curr += scnprintf(buf + *curr, max_size -
				*curr, " 0x%x\t",
				cd->arb[k][(c * nmasters) + j]);
		}
	}
}
#endif

/**
* msm_bus_rpm_commit() - Commit the arbitration data to RPM
* @fabric: Fabric for which the data should be committed
**/
int msm_bus_rpm_commit(struct msm_bus_fabric_registration
	*fab_pdata, struct msm_rpm_iv_pair *rpm_data,
	void **cdata)
{

	int ret;
	bool valid;
	struct commit_data *dual_cd, *act_cd;
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
