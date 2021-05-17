// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <dt-bindings/regulator/qcom,rpmh-regulator-levels.h>
#include <linux/types.h>
#include <soc/qcom/cmd-db.h>
#include <soc/qcom/tcs.h>

#include "adreno.h"
#include "adreno_a6xx.h"
#include "adreno_hfi.h"
#include "kgsl_bus.h"
#include "kgsl_device.h"

struct rpmh_arc_vals {
	u32 num;
	const u16 *val;
};

struct bcm {
	const char *name;
	u32 buswidth;
	u32 channels;
	u32 unit;
	u16 width;
	u8 vcd;
	bool fixed;
};

struct bcm_data {
	__le32 unit;
	__le16 width;
	u8 vcd;
	u8 reserved;
};

struct rpmh_bw_votes {
	u32 wait_bitmask;
	u32 num_cmds;
	u32 *addrs;
	u32 num_levels;
	u32 **cmds;
};

#define ARC_VOTE_SET(pri, sec, vlvl) \
	((((vlvl) & 0xFFFF) << 16) | (((sec) & 0xFF) << 8) | ((pri) & 0xFF))

static int rpmh_arc_cmds(struct rpmh_arc_vals *arc, const char *res_id)
{
	size_t len = 0;

	arc->val = cmd_db_read_aux_data(res_id, &len);

	/*
	 * cmd_db_read_aux_data() gives us a zero-padded table of
	 * size len that contains the arc values. To determine the
	 * number of arc values, we loop through the table and count
	 * them until we get to the end of the buffer or hit the
	 * zero padding.
	 */
	for (arc->num = 1; arc->num < (len >> 1); arc->num++) {
		if (arc->val[arc->num - 1] != 0 &&  arc->val[arc->num] == 0)
			break;
	}

	return 0;
}

static int setup_volt_dependency_tbl(uint32_t *votes,
		struct rpmh_arc_vals *pri_rail, struct rpmh_arc_vals *sec_rail,
		u16 *vlvl, unsigned int num_entries)
{
	int i, j, k;
	uint16_t cur_vlvl;
	bool found_match;

	/* i tracks current KGSL GPU frequency table entry
	 * j tracks secondary rail voltage table entry
	 * k tracks primary rail voltage table entry
	 */
	for (i = 0; i < num_entries; i++) {
		found_match = false;

		/* Look for a primary rail voltage that matches a VLVL level */
		for (k = 0; k < pri_rail->num; k++) {
			if (pri_rail->val[k] >= vlvl[i]) {
				cur_vlvl = pri_rail->val[k];
				found_match = true;
				break;
			}
		}

		/* If we did not find a matching VLVL level then abort */
		if (!found_match)
			return -EINVAL;

		/*
		 * Look for a secondary rail index whose VLVL value
		 * is greater than or equal to the VLVL value of the
		 * corresponding index of the primary rail
		 */
		for (j = 0; j < sec_rail->num; j++) {
			if (sec_rail->val[j] >= cur_vlvl ||
					j + 1 == sec_rail->num)
				break;
		}

		if (j == sec_rail->num)
			j = 0;

		votes[i] = ARC_VOTE_SET(k, j, cur_vlvl);
	}

	return 0;
}

/* Generate a set of bandwidth votes for the list of BCMs */
static void tcs_cmd_data(struct bcm *bcms, int count, u32 ab, u32 ib,
		u32 *data)
{
	int i;

	for (i = 0; i < count; i++) {
		bool valid = true;
		bool commit = false;
		u64 avg, peak, x, y;

		if (i == count - 1 || bcms[i].vcd != bcms[i + 1].vcd)
			commit = true;

		/*
		 * On a660, the "ACV" y vote should be 0x08 if there is a valid
		 * vote and 0x00 if not. This is kind of hacky and a660 specific
		 * but we can clean it up when we add a new target
		 */
		if (bcms[i].fixed) {
			if (!ab && !ib)
				data[i] = BCM_TCS_CMD(commit, false, 0x0, 0x0);
			else
				data[i] = BCM_TCS_CMD(commit, true, 0x0, 0x8);
			continue;
		}

		/* Multiple the bandwidth by the width of the connection */
		avg = ((u64) ab) * bcms[i].width;

		/* And then divide by the total width across channels */
		do_div(avg, bcms[i].buswidth * bcms[i].channels);

		peak = ((u64) ib) * bcms[i].width;
		do_div(peak, bcms[i].buswidth);

		/* Input bandwidth value is in KBps */
		x = avg * 1000ULL;
		do_div(x, bcms[i].unit);

		/* Input bandwidth value is in KBps */
		y = peak * 1000ULL;
		do_div(y, bcms[i].unit);

		/*
		 * If a bandwidth value was specified but the calculation ends
		 * rounding down to zero, set a minimum level
		 */
		if (ab && x == 0)
			x = 1;

		if (ib && y == 0)
			y = 1;

		x = min_t(u64, x, BCM_TCS_CMD_VOTE_MASK);
		y = min_t(u64, y, BCM_TCS_CMD_VOTE_MASK);

		if (!x && !y)
			valid = false;

		data[i] = BCM_TCS_CMD(commit, valid, x, y);
	}
}

static void free_rpmh_bw_votes(struct rpmh_bw_votes *votes)
{
	int i;

	if (!votes)
		return;

	for (i = 0; votes->cmds && i < votes->num_levels; i++)
		kfree(votes->cmds[i]);

	kfree(votes->cmds);
	kfree(votes->addrs);
	kfree(votes);
}

/* Build the votes table from the specified bandwidth levels */
static struct rpmh_bw_votes *build_rpmh_bw_votes(struct bcm *bcms,
		int bcm_count, u32 *levels, int levels_count)
{
	struct rpmh_bw_votes *votes;
	int i;

	votes = kzalloc(sizeof(*votes), GFP_KERNEL);
	if (!votes)
		return ERR_PTR(-ENOMEM);

	votes->addrs = kcalloc(bcm_count, sizeof(*votes->cmds), GFP_KERNEL);
	if (!votes->addrs) {
		free_rpmh_bw_votes(votes);
		return ERR_PTR(-ENOMEM);
	}

	votes->cmds = kcalloc(levels_count, sizeof(*votes->cmds), GFP_KERNEL);
	if (!votes->cmds) {
		free_rpmh_bw_votes(votes);
		return ERR_PTR(-ENOMEM);
	}

	votes->num_cmds = bcm_count;
	votes->num_levels = levels_count;

	/* Get the cmd-db information for each BCM */
	for (i = 0; i < bcm_count; i++) {
		size_t l;
		const struct bcm_data *data;

		data = cmd_db_read_aux_data(bcms[i].name, &l);

		votes->addrs[i] = cmd_db_read_addr(bcms[i].name);

		bcms[i].unit = le32_to_cpu(data->unit);
		bcms[i].width = le16_to_cpu(data->width);
		bcms[i].vcd = data->vcd;
	}

	for (i = 0; i < bcm_count; i++) {
		if (i == (bcm_count - 1) || bcms[i].vcd != bcms[i + 1].vcd)
			votes->wait_bitmask |= (1 << i);
	}

	for (i = 0; i < levels_count; i++) {
		votes->cmds[i] = kcalloc(bcm_count, sizeof(u32), GFP_KERNEL);
		if (!votes->cmds[i]) {
			free_rpmh_bw_votes(votes);
			return ERR_PTR(-ENOMEM);
		}

		tcs_cmd_data(bcms, bcm_count, 0, levels[i], votes->cmds[i]);
	}

	return votes;
}

/*
 * setup_gmu_arc_votes - Build the gmu voting table
 * @adreno_dev: Pointer to adreno device
 * @pri_rail: Pointer to primary power rail vlvl table
 * @sec_rail: Pointer to second/dependent power rail vlvl table
 *
 * This function initializes the cx votes for all gmu frequencies
 * for gmu dcvs
 */
static int setup_cx_arc_votes(struct adreno_device *adreno_dev,
	struct rpmh_arc_vals *pri_rail, struct rpmh_arc_vals *sec_rail)
{
	/* Hardcoded values of GMU CX voltage levels */
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct a6xx_hfi *hfi = &gmu->hfi;
	u16 gmu_cx_vlvl[MAX_CX_LEVELS];
	u32 cx_votes[MAX_CX_LEVELS];
	struct hfi_dcvstable_cmd *table = &hfi->dcvs_table;
	int ret, i;

	gmu_cx_vlvl[0] = 0;
	gmu_cx_vlvl[1] = RPMH_REGULATOR_LEVEL_MIN_SVS;
	gmu_cx_vlvl[2] = RPMH_REGULATOR_LEVEL_SVS;

	if (adreno_is_a660(adreno_dev))
		gmu_cx_vlvl[1] = RPMH_REGULATOR_LEVEL_LOW_SVS;

	table->gmu_level_num = 3;

	table->cx_votes[0].freq = 0;
	table->cx_votes[1].freq = GMU_FREQ_MIN / 1000;
	table->cx_votes[2].freq = GMU_FREQ_MAX / 1000;

	ret = setup_volt_dependency_tbl(cx_votes, pri_rail,
			sec_rail, gmu_cx_vlvl, table->gmu_level_num);
	if (!ret) {
		for (i = 0; i < table->gmu_level_num; i++)
			table->cx_votes[i].vote = cx_votes[i];
	}

	return ret;
}

/*
 * setup_gx_arc_votes - Build the gpu dcvs voting table
 * @hfi: Pointer to hfi device
 * @pri_rail: Pointer to primary power rail vlvl table
 * @sec_rail: Pointer to second/dependent power rail vlvl table
 *
 * This function initializes the gx votes for all gpu frequencies
 * for gpu dcvs
 */
static int setup_gx_arc_votes(struct adreno_device *adreno_dev,
	struct rpmh_arc_vals *pri_rail, struct rpmh_arc_vals *sec_rail)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct hfi_dcvstable_cmd *table = &gmu->hfi.dcvs_table;
	u32 index;
	u16 vlvl_tbl[MAX_GX_LEVELS];
	u32 gx_votes[MAX_GX_LEVELS];
	int ret, i;

	/* Add the zero powerlevel for the perf table */
	table->gpu_level_num = device->pwrctrl.num_pwrlevels + 1;

	if (table->gpu_level_num > pri_rail->num ||
		table->gpu_level_num > ARRAY_SIZE(vlvl_tbl)) {
		dev_err(&gmu->pdev->dev,
			"Defined more GPU DCVS levels than RPMh can support\n");
		return -ERANGE;
	}

	memset(vlvl_tbl, 0, sizeof(vlvl_tbl));

	table->gx_votes[0].freq = 0;

	/* GMU power levels are in ascending order */
	for (index = 1, i = pwr->num_pwrlevels - 1; i >= 0; i--, index++) {
		vlvl_tbl[index] = pwr->pwrlevels[i].voltage_level;
		table->gx_votes[index].freq = pwr->pwrlevels[i].gpu_freq / 1000;
	}

	ret = setup_volt_dependency_tbl(gx_votes, pri_rail,
			sec_rail, vlvl_tbl, table->gpu_level_num);
	if (!ret) {
		for (i = 0; i < table->gpu_level_num; i++) {
			table->gx_votes[i].vote = gx_votes[i];
			table->gx_votes[i].acd = 0xffffffff;
		}
	}

	return ret;

}

static int build_dcvs_table(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct a6xx_hfi *hfi = &gmu->hfi;
	struct rpmh_arc_vals gx_arc, cx_arc, mx_arc;
	int ret;

	ret = CMD_MSG_HDR(hfi->dcvs_table, H2F_MSG_PERF_TBL);
	if (ret)
		return ret;

	ret = rpmh_arc_cmds(&gx_arc, "gfx.lvl");
	if (ret)
		return ret;

	ret = rpmh_arc_cmds(&cx_arc, "cx.lvl");
	if (ret)
		return ret;

	ret = rpmh_arc_cmds(&mx_arc, "mx.lvl");
	if (ret)
		return ret;

	ret = setup_cx_arc_votes(adreno_dev, &cx_arc, &mx_arc);
	if (ret)
		return ret;

	return setup_gx_arc_votes(adreno_dev, &gx_arc, &mx_arc);
}

/*
 * List of Bus Control Modules (BCMs) that need to be configured for the GPU
 * to access DDR. For each bus level we will generate a vote each BC
 */
static struct bcm a660_ddr_bcms[] = {
	{ .name = "SH0", .buswidth = 16 },
	{ .name = "MC0", .buswidth = 4 },
	{ .name = "ACV", .fixed = true },
};

/* Same as above, but for the CNOC BCMs */
static struct bcm a660_cnoc_bcms[] = {
	{ .name = "CN0", .buswidth = 4 },
};

static void build_bw_table_cmd(struct hfi_bwtable_cmd *cmd,
		struct rpmh_bw_votes *ddr, struct rpmh_bw_votes *cnoc)
{
	u32 i, j;

	cmd->bw_level_num = ddr->num_levels;
	cmd->ddr_cmds_num = ddr->num_cmds;
	cmd->ddr_wait_bitmask = ddr->wait_bitmask;

	for (i = 0; i < ddr->num_cmds; i++)
		cmd->ddr_cmd_addrs[i] = ddr->addrs[i];

	for (i = 0; i < ddr->num_levels; i++)
		for (j = 0; j < ddr->num_cmds; j++)
			cmd->ddr_cmd_data[i][j] = (u32) ddr->cmds[i][j];

	if (!cnoc)
		return;

	cmd->cnoc_cmds_num = cnoc->num_cmds;
		cmd->cnoc_wait_bitmask = cnoc->wait_bitmask;

	for (i = 0; i < cnoc->num_cmds; i++)
		cmd->cnoc_cmd_addrs[i] = cnoc->addrs[i];

	for (i = 0; i < cnoc->num_levels; i++)
		for (j = 0; j < cnoc->num_cmds; j++)
			cmd->cnoc_cmd_data[i][j] = (u32) cnoc->cmds[i][j];
}

static int build_bw_table(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct rpmh_bw_votes *ddr, *cnoc = NULL;
	u32 *cnoc_table;
	u32 count;
	int ret;

	ddr = build_rpmh_bw_votes(a660_ddr_bcms, ARRAY_SIZE(a660_ddr_bcms),
		pwr->ddr_table, pwr->ddr_table_count);
	if (IS_ERR(ddr))
		return PTR_ERR(ddr);

	cnoc_table = kgsl_bus_get_table(device->pdev, "qcom,bus-table-cnoc",
		&count);

	if (count > 0)
		cnoc = build_rpmh_bw_votes(a660_cnoc_bcms,
			ARRAY_SIZE(a660_cnoc_bcms), cnoc_table, count);

	kfree(cnoc_table);

	if (IS_ERR(cnoc)) {
		free_rpmh_bw_votes(ddr);
		return PTR_ERR(cnoc);
	}

	ret = CMD_MSG_HDR(gmu->hfi.bw_table, H2F_MSG_BW_VOTE_TBL);
	if (ret)
		return ret;

	build_bw_table_cmd(&gmu->hfi.bw_table, ddr, cnoc);

	free_rpmh_bw_votes(ddr);
	free_rpmh_bw_votes(cnoc);

	return 0;
}

int a6xx_build_rpmh_tables(struct adreno_device *adreno_dev)
{
	int ret;

	ret = build_dcvs_table(adreno_dev);
	if (ret)
		return ret;

	return build_bw_table(adreno_dev);
}
