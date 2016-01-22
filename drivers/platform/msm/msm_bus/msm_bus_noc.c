/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "AXI: NOC: %s(): " fmt, __func__

#include <linux/slab.h>
#include <linux/io.h>
#include <linux/msm-bus-board.h>
#include "msm_bus_core.h"
#include "msm_bus_noc.h"
#include "msm_bus_adhoc.h"

/* NOC_QOS generic */
#define __CLZ(x) ((8 * sizeof(uint32_t)) - 1 - __fls(x))
#define SAT_SCALE 16	/* 16 bytes minimum for saturation */
#define BW_SCALE  256	/* 1/256 byte per cycle unit */
#define QOS_DEFAULT_BASEOFFSET		0x00003000
#define QOS_DEFAULT_DELTA		0x80
#define MAX_BW_FIELD (NOC_QOS_BWn_BW_BMSK >> NOC_QOS_BWn_BW_SHFT)
#define MAX_SAT_FIELD (NOC_QOS_SATn_SAT_BMSK >> NOC_QOS_SATn_SAT_SHFT)
#define MIN_SAT_FIELD	1
#define MIN_BW_FIELD	1

#define NOC_QOS_REG_BASE(b, o)		((b) + (o))

#define NOC_QOS_PRIORITYn_ADDR(b, o, n, d)	\
	(NOC_QOS_REG_BASE(b, o) + 0x8 + (d) * (n))
enum noc_qos_id_priorityn {
	NOC_QOS_PRIORITYn_RMSK		= 0x0000000f,
	NOC_QOS_PRIORITYn_MAXn		= 32,
	NOC_QOS_PRIORITYn_P1_BMSK	= 0xc,
	NOC_QOS_PRIORITYn_P1_SHFT	= 0x2,
	NOC_QOS_PRIORITYn_P0_BMSK	= 0x3,
	NOC_QOS_PRIORITYn_P0_SHFT	= 0x0,
};

#define NOC_QOS_MODEn_ADDR(b, o, n, d) \
	(NOC_QOS_REG_BASE(b, o) + 0xC + (d) * (n))
enum noc_qos_id_moden_rmsk {
	NOC_QOS_MODEn_RMSK		= 0x00000003,
	NOC_QOS_MODEn_MAXn		= 32,
	NOC_QOS_MODEn_MODE_BMSK		= 0x3,
	NOC_QOS_MODEn_MODE_SHFT		= 0x0,
};

#define NOC_QOS_BWn_ADDR(b, o, n, d) \
	(NOC_QOS_REG_BASE(b, o) + 0x10 + (d) * (n))
enum noc_qos_id_bwn {
	NOC_QOS_BWn_RMSK		= 0x0000ffff,
	NOC_QOS_BWn_MAXn		= 32,
	NOC_QOS_BWn_BW_BMSK		= 0xffff,
	NOC_QOS_BWn_BW_SHFT		= 0x0,
};

/* QOS Saturation registers */
#define NOC_QOS_SATn_ADDR(b, o, n, d) \
	(NOC_QOS_REG_BASE(b, o) + 0x14 + (d) * (n))
enum noc_qos_id_saturationn {
	NOC_QOS_SATn_RMSK		= 0x000003ff,
	NOC_QOS_SATn_MAXn		= 32,
	NOC_QOS_SATn_SAT_BMSK		= 0x3ff,
	NOC_QOS_SATn_SAT_SHFT		= 0x0,
};

static int noc_div(uint64_t *a, uint32_t b)
{
	if ((*a > 0) && (*a < b)) {
		*a = 0;
		return 1;
	} else {
		return do_div(*a, b);
	}
}

/**
 * Calculates bw hardware is using from register values
 * bw returned is in bytes/sec
 */
static uint64_t noc_bw(uint32_t bw_field, uint32_t qos_freq)
{
	uint64_t res;
	uint32_t rem, scale;

	res = 2 * qos_freq * bw_field;
	scale = BW_SCALE * 1000;
	rem = noc_div(&res, scale);
	MSM_BUS_DBG("NOC: Calculated bw: %llu\n", res * 1000000ULL);
	return res * 1000000ULL;
}

/**
 * Calculate the max BW in Bytes/s for a given time-base.
 */
static uint32_t noc_bw_ceil(long int bw_field, uint32_t qos_freq_khz)
{
	uint64_t bw_temp = 2 * qos_freq_khz * bw_field;
	uint32_t scale = 1000 * BW_SCALE;
	noc_div(&bw_temp, scale);
	return bw_temp * 1000000;
}
#define MAX_BW(timebase) noc_bw_ceil(MAX_BW_FIELD, (timebase))

/**
 * Calculates ws hardware is using from register values
 * ws returned is in nanoseconds
 */
static uint32_t noc_ws(uint64_t bw, uint32_t sat, uint32_t qos_freq)
{
	if (bw && qos_freq) {
		uint32_t bwf = bw * qos_freq;
		uint64_t scale = 1000000000000LL * BW_SCALE *
			SAT_SCALE * sat;
		noc_div(&scale, bwf);
		MSM_BUS_DBG("NOC: Calculated ws: %llu\n", scale);
		return scale;
	}

	return 0;
}
#define MAX_WS(bw, timebase) noc_ws((bw), MAX_SAT_FIELD, (timebase))

/* Calculate bandwidth field value for requested bandwidth  */
static uint32_t noc_bw_field(uint64_t bw_bps, uint32_t qos_freq_khz)
{
	uint32_t bw_field = 0;

	if (bw_bps) {
		uint32_t rem;
		uint64_t bw_capped = min_t(uint64_t, bw_bps,
						MAX_BW(qos_freq_khz));
		uint64_t bwc = bw_capped * BW_SCALE;
		uint64_t qf = 2 * qos_freq_khz * 1000;

		rem = noc_div(&bwc, qf);
		bw_field = (uint32_t)max_t(unsigned long, bwc, MIN_BW_FIELD);
		bw_field = (uint32_t)min_t(unsigned long, bw_field,
								MAX_BW_FIELD);
	}

	MSM_BUS_DBG("NOC: bw_field: %u\n", bw_field);
	return bw_field;
}

static uint32_t noc_sat_field(uint64_t bw, uint32_t ws, uint32_t qos_freq)
{
	uint32_t sat_field = 0;

	if (bw) {
		/* Limit to max bw and scale bw to 100 KB increments */
		uint64_t tbw, tscale;
		uint64_t bw_scaled = min_t(uint64_t, bw, MAX_BW(qos_freq));
		uint32_t rem = noc_div(&bw_scaled, 100000);

		/**
			SATURATION =
			(BW [MBps] * integration window [us] *
				time base frequency [MHz]) / (256 * 16)
		 */
		tbw = bw_scaled * ws * qos_freq;
		tscale = BW_SCALE * SAT_SCALE * 1000000LL;
		rem = noc_div(&tbw, tscale);
		sat_field = (uint32_t)max_t(unsigned long, tbw, MIN_SAT_FIELD);
		sat_field = (uint32_t)min_t(unsigned long, sat_field,
							MAX_SAT_FIELD);
	}

	MSM_BUS_DBG("NOC: sat_field: %d\n", sat_field);
	return sat_field;
}

static void noc_set_qos_mode(void __iomem *base, uint32_t qos_off,
		uint32_t mport, uint32_t qos_delta, uint8_t mode,
		uint8_t perm_mode)
{
	if (mode < NOC_QOS_MODE_MAX &&
		((1 << mode) & perm_mode)) {
		uint32_t reg_val;

		reg_val = readl_relaxed(NOC_QOS_MODEn_ADDR(base, qos_off,
			mport, qos_delta)) & NOC_QOS_MODEn_RMSK;
		writel_relaxed(((reg_val & (~(NOC_QOS_MODEn_MODE_BMSK))) |
			(mode & NOC_QOS_MODEn_MODE_BMSK)),
			NOC_QOS_MODEn_ADDR(base, qos_off, mport, qos_delta));
	}
	/* Ensure qos mode is set before exiting */
	wmb();
}

static void noc_set_qos_priority(void __iomem *base, uint32_t qos_off,
		uint32_t mport, uint32_t qos_delta,
		struct msm_bus_noc_qos_priority *priority)
{
	uint32_t reg_val, val;

	reg_val = readl_relaxed(NOC_QOS_PRIORITYn_ADDR(base, qos_off, mport,
		qos_delta)) & NOC_QOS_PRIORITYn_RMSK;
	val = priority->p1 << NOC_QOS_PRIORITYn_P1_SHFT;
	writel_relaxed(((reg_val & (~(NOC_QOS_PRIORITYn_P1_BMSK))) |
		(val & NOC_QOS_PRIORITYn_P1_BMSK)),
		NOC_QOS_PRIORITYn_ADDR(base, qos_off, mport, qos_delta));

	reg_val = readl_relaxed(NOC_QOS_PRIORITYn_ADDR(base, qos_off, mport,
								qos_delta))
		& NOC_QOS_PRIORITYn_RMSK;
	writel_relaxed(((reg_val & (~(NOC_QOS_PRIORITYn_P0_BMSK))) |
		(priority->p0 & NOC_QOS_PRIORITYn_P0_BMSK)),
		NOC_QOS_PRIORITYn_ADDR(base, qos_off, mport, qos_delta));
	/* Ensure qos priority is set before exiting */
	wmb();
}

static void msm_bus_noc_set_qos_bw(void __iomem *base, uint32_t qos_off,
		uint32_t qos_freq, uint32_t mport, uint32_t qos_delta,
		uint8_t perm_mode, struct msm_bus_noc_qos_bw *qbw)
{
	uint32_t reg_val, val, mode;

	if (!qos_freq) {
		MSM_BUS_DBG("Zero QoS Freq\n");
		return;
	}

	/* If Limiter or Regulator modes are not supported, bw not available*/
	if (perm_mode & (NOC_QOS_PERM_MODE_LIMITER |
		NOC_QOS_PERM_MODE_REGULATOR)) {
		uint32_t bw_val = noc_bw_field(qbw->bw, qos_freq);
		uint32_t sat_val = noc_sat_field(qbw->bw, qbw->ws,
			qos_freq);

		MSM_BUS_DBG("NOC: BW: perm_mode: %d bw_val: %d, sat_val: %d\n",
			perm_mode, bw_val, sat_val);
		/*
		 * If in Limiter/Regulator mode, first go to fixed mode.
		 * Clear QoS accumulator
		 **/
		mode = readl_relaxed(NOC_QOS_MODEn_ADDR(base, qos_off,
			mport, qos_delta)) & NOC_QOS_MODEn_MODE_BMSK;
		if (mode == NOC_QOS_MODE_REGULATOR || mode ==
			NOC_QOS_MODE_LIMITER) {
			reg_val = readl_relaxed(NOC_QOS_MODEn_ADDR(
				base, qos_off, mport, qos_delta));
			val = NOC_QOS_MODE_FIXED;
			writel_relaxed((reg_val & (~(NOC_QOS_MODEn_MODE_BMSK)))
				| (val & NOC_QOS_MODEn_MODE_BMSK),
				NOC_QOS_MODEn_ADDR(base, qos_off, mport,
								qos_delta));
		}

		reg_val = readl_relaxed(NOC_QOS_BWn_ADDR(base, qos_off, mport,
								qos_delta));
		val = bw_val << NOC_QOS_BWn_BW_SHFT;
		writel_relaxed(((reg_val & (~(NOC_QOS_BWn_BW_BMSK))) |
			(val & NOC_QOS_BWn_BW_BMSK)),
			NOC_QOS_BWn_ADDR(base, qos_off, mport, qos_delta));

		MSM_BUS_DBG("NOC: BW: Wrote value: 0x%x\n", ((reg_val &
			(~NOC_QOS_BWn_BW_BMSK)) | (val &
			NOC_QOS_BWn_BW_BMSK)));

		reg_val = readl_relaxed(NOC_QOS_SATn_ADDR(base, qos_off,
			mport, qos_delta));
		val = sat_val << NOC_QOS_SATn_SAT_SHFT;
		writel_relaxed(((reg_val & (~(NOC_QOS_SATn_SAT_BMSK))) |
			(val & NOC_QOS_SATn_SAT_BMSK)),
			NOC_QOS_SATn_ADDR(base, qos_off, mport, qos_delta));

		MSM_BUS_DBG("NOC: SAT: Wrote value: 0x%x\n", ((reg_val &
			(~NOC_QOS_SATn_SAT_BMSK)) | (val &
			NOC_QOS_SATn_SAT_BMSK)));

		/* Set mode back to what it was initially */
		reg_val = readl_relaxed(NOC_QOS_MODEn_ADDR(base, qos_off,
			mport, qos_delta));
		writel_relaxed((reg_val & (~(NOC_QOS_MODEn_MODE_BMSK)))
			| (mode & NOC_QOS_MODEn_MODE_BMSK),
			NOC_QOS_MODEn_ADDR(base, qos_off, mport, qos_delta));
		/* Ensure that all writes for bandwidth registers have
		 * completed before returning
		 */
		wmb();
	}
}

uint8_t msm_bus_noc_get_qos_mode(void __iomem *base, uint32_t qos_off,
	uint32_t mport, uint32_t qos_delta, uint32_t mode, uint32_t perm_mode)
{
	if (NOC_QOS_MODES_ALL_PERM == perm_mode)
		return readl_relaxed(NOC_QOS_MODEn_ADDR(base, qos_off,
			mport, qos_delta)) & NOC_QOS_MODEn_MODE_BMSK;
	else
		return 31 - __CLZ(mode &
			NOC_QOS_MODES_ALL_PERM);
}

void msm_bus_noc_get_qos_priority(void __iomem *base, uint32_t qos_off,
	uint32_t mport, uint32_t qos_delta,
	struct msm_bus_noc_qos_priority *priority)
{
	priority->p1 = (readl_relaxed(NOC_QOS_PRIORITYn_ADDR(base, qos_off,
		mport, qos_delta)) & NOC_QOS_PRIORITYn_P1_BMSK) >>
		NOC_QOS_PRIORITYn_P1_SHFT;

	priority->p0 = (readl_relaxed(NOC_QOS_PRIORITYn_ADDR(base, qos_off,
		mport, qos_delta)) & NOC_QOS_PRIORITYn_P0_BMSK) >>
		NOC_QOS_PRIORITYn_P0_SHFT;
}

void msm_bus_noc_get_qos_bw(void __iomem *base, uint32_t qos_off,
	uint32_t qos_freq,
	uint32_t mport, uint32_t qos_delta, uint8_t perm_mode,
	struct msm_bus_noc_qos_bw *qbw)
{
	if (perm_mode & (NOC_QOS_PERM_MODE_LIMITER |
		NOC_QOS_PERM_MODE_REGULATOR)) {
		uint32_t bw_val = readl_relaxed(NOC_QOS_BWn_ADDR(
			base, qos_off, mport, qos_delta)) & NOC_QOS_BWn_BW_BMSK;
		uint32_t sat = readl_relaxed(NOC_QOS_SATn_ADDR(
			base, qos_off, mport, qos_delta))
						& NOC_QOS_SATn_SAT_BMSK;

		qbw->bw = noc_bw(bw_val, qos_freq);
		qbw->ws = noc_ws(qbw->bw, sat, qos_freq);
	} else {
		qbw->bw = 0;
		qbw->ws = 0;
	}
}

static int msm_bus_noc_mas_init(struct msm_bus_noc_info *ninfo,
	struct msm_bus_inode_info *info)
{
	int i;
	struct msm_bus_noc_qos_priority *prio;
	prio = kzalloc(sizeof(struct msm_bus_noc_qos_priority),
		GFP_KERNEL);
	if (!prio) {
		MSM_BUS_WARN("Couldn't alloc prio data for node: %d\n",
			info->node_info->id);
		return -ENOMEM;
	}

	prio->read_prio = info->node_info->prio_rd;
	prio->write_prio = info->node_info->prio_wr;
	prio->p1 = info->node_info->prio1;
	prio->p0 = info->node_info->prio0;
	info->hw_data = (void *)prio;

	if (!info->node_info->qport) {
		MSM_BUS_DBG("No QoS Ports to init\n");
		return 0;
	}

	for (i = 0; i < info->node_info->num_mports; i++) {
		if (info->node_info->mode != NOC_QOS_MODE_BYPASS) {
			noc_set_qos_priority(ninfo->base, ninfo->qos_baseoffset,
				info->node_info->qport[i], ninfo->qos_delta,
				prio);

			if (info->node_info->mode != NOC_QOS_MODE_FIXED) {
				struct msm_bus_noc_qos_bw qbw;
				qbw.ws = info->node_info->ws;
				qbw.bw = 0;
				msm_bus_noc_set_qos_bw(ninfo->base,
					ninfo->qos_baseoffset,
					ninfo->qos_freq, info->node_info->
					qport[i], ninfo->qos_delta,
					info->node_info->perm_mode,
					&qbw);
			}
		}

		noc_set_qos_mode(ninfo->base, ninfo->qos_baseoffset,
			info->node_info->qport[i], ninfo->qos_delta,
			info->node_info->mode,
			info->node_info->perm_mode);
	}

	return 0;
}

static void msm_bus_noc_node_init(void *hw_data,
	struct msm_bus_inode_info *info)
{
	struct msm_bus_noc_info *ninfo =
		(struct msm_bus_noc_info *)hw_data;

	if (!IS_SLAVE(info->node_info->priv_id))
		if (info->node_info->hw_sel != MSM_BUS_RPM)
			msm_bus_noc_mas_init(ninfo, info);
}

static int msm_bus_noc_allocate_commit_data(struct msm_bus_fabric_registration
	*fab_pdata, void **cdata, int ctx)
{
	struct msm_bus_noc_commit **cd = (struct msm_bus_noc_commit **)cdata;
	struct msm_bus_noc_info *ninfo =
		(struct msm_bus_noc_info *)fab_pdata->hw_data;

	*cd = kzalloc(sizeof(struct msm_bus_noc_commit), GFP_KERNEL);
	if (!*cd) {
		MSM_BUS_DBG("Couldn't alloc mem for cdata\n");
		return -ENOMEM;
	}

	(*cd)->mas = ninfo->cdata[ctx].mas;
	(*cd)->slv = ninfo->cdata[ctx].slv;

	return 0;
}

static void *msm_bus_noc_allocate_noc_data(struct platform_device *pdev,
	struct msm_bus_fabric_registration *fab_pdata)
{
	struct resource *noc_mem;
	struct resource *noc_io;
	struct msm_bus_noc_info *ninfo;
	int i;

	ninfo = kzalloc(sizeof(struct msm_bus_noc_info), GFP_KERNEL);
	if (!ninfo) {
		MSM_BUS_DBG("Couldn't alloc mem for noc info\n");
		return NULL;
	}

	ninfo->nmasters = fab_pdata->nmasters;
	ninfo->nqos_masters = fab_pdata->nmasters;
	ninfo->nslaves = fab_pdata->nslaves;
	ninfo->qos_freq = fab_pdata->qos_freq;

	if (!fab_pdata->qos_baseoffset)
		ninfo->qos_baseoffset = QOS_DEFAULT_BASEOFFSET;
	else
		ninfo->qos_baseoffset = fab_pdata->qos_baseoffset;

	if (!fab_pdata->qos_delta)
		ninfo->qos_delta = QOS_DEFAULT_DELTA;
	else
		ninfo->qos_delta = fab_pdata->qos_delta;

	ninfo->mas_modes = kzalloc(sizeof(uint32_t) * fab_pdata->nmasters,
		GFP_KERNEL);
	if (!ninfo->mas_modes) {
		MSM_BUS_DBG("Couldn't alloc mem for noc master-modes\n");
		return NULL;
	}

	for (i = 0; i < NUM_CTX; i++) {
		ninfo->cdata[i].mas = kzalloc(sizeof(struct
			msm_bus_node_hw_info) * fab_pdata->nmasters * 2,
			GFP_KERNEL);
		if (!ninfo->cdata[i].mas) {
			MSM_BUS_DBG("Couldn't alloc mem for noc master-bw\n");
			kfree(ninfo->mas_modes);
			kfree(ninfo);
			return NULL;
		}

		ninfo->cdata[i].slv = kzalloc(sizeof(struct
			msm_bus_node_hw_info) * fab_pdata->nslaves * 2,
			GFP_KERNEL);
		if (!ninfo->cdata[i].slv) {
			MSM_BUS_DBG("Couldn't alloc mem for noc master-bw\n");
			kfree(ninfo->cdata[i].mas);
			goto err;
		}
	}

	/* If it's a virtual fabric, don't get memory info */
	if (fab_pdata->virt)
		goto skip_mem;

	noc_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!noc_mem && !fab_pdata->virt) {
		MSM_BUS_ERR("Cannot get NoC Base address\n");
		goto err;
	}

	noc_io = request_mem_region(noc_mem->start,
			resource_size(noc_mem), pdev->name);
	if (!noc_io) {
		MSM_BUS_ERR("NoC memory unavailable\n");
		goto err;
	}

	ninfo->base = ioremap(noc_mem->start, resource_size(noc_mem));
	if (!ninfo->base) {
		MSM_BUS_ERR("IOremap failed for NoC!\n");
		release_mem_region(noc_mem->start, resource_size(noc_mem));
		goto err;
	}

skip_mem:
	fab_pdata->hw_data = (void *)ninfo;
	return (void *)ninfo;

err:
	kfree(ninfo->mas_modes);
	kfree(ninfo);
	return NULL;
}

static void free_commit_data(void *cdata)
{
	struct msm_bus_noc_commit *cd = (struct msm_bus_noc_commit *)cdata;

	kfree(cd->mas);
	kfree(cd->slv);
	kfree(cd);
}

static bool msm_bus_noc_update_bw_reg(int mode)
{
	bool ret = false;

	if ((mode == NOC_QOS_MODE_LIMITER) ||
			(mode == NOC_QOS_MODE_REGULATOR))
			ret = true;

	return ret;
}

static void msm_bus_noc_update_bw(struct msm_bus_inode_info *hop,
	struct msm_bus_inode_info *info,
	struct msm_bus_fabric_registration *fab_pdata,
	void *sel_cdata, int *master_tiers,
	int64_t add_bw)
{
	struct msm_bus_noc_info *ninfo;
	struct msm_bus_noc_qos_bw qos_bw;
	int i, ports;
	int64_t bw;
	struct msm_bus_noc_commit *sel_cd =
		(struct msm_bus_noc_commit *)sel_cdata;

	ninfo = (struct msm_bus_noc_info *)fab_pdata->hw_data;
	if (!ninfo->qos_freq) {
		MSM_BUS_DBG("NOC: No qos frequency to update bw\n");
		return;
	}

	if (info->node_info->num_mports == 0) {
		MSM_BUS_DBG("NOC: Skip Master BW\n");
		goto skip_mas_bw;
	}

	ports = info->node_info->num_mports;
	bw = INTERLEAVED_BW(fab_pdata, add_bw, ports);

	MSM_BUS_DBG("NOC: Update bw for: %d: %lld\n",
		info->node_info->priv_id, add_bw);
	for (i = 0; i < ports; i++) {
		sel_cd->mas[info->node_info->masterp[i]].bw += bw;
		sel_cd->mas[info->node_info->masterp[i]].hw_id =
			info->node_info->mas_hw_id;
		MSM_BUS_DBG("NOC: Update mas_bw: ID: %d, BW: %llu ports:%d\n",
			info->node_info->priv_id,
			sel_cd->mas[info->node_info->masterp[i]].bw,
			ports);
		/* Check if info is a shared master.
		 * If it is, mark it dirty
		 * If it isn't, then set QOS Bandwidth
		 **/
		if (info->node_info->hw_sel == MSM_BUS_RPM)
			sel_cd->mas[info->node_info->masterp[i]].dirty = 1;
		else {
			if (!info->node_info->qport) {
				MSM_BUS_DBG("No qos ports to update!\n");
				break;
			}

			if (!(info->node_info->mode == NOC_QOS_MODE_REGULATOR)
					|| (info->node_info->mode ==
						NOC_QOS_MODE_LIMITER)) {
				MSM_BUS_DBG("Skip QoS reg programming\n");
				break;
			}
			qos_bw.bw = sel_cd->mas[info->node_info->masterp[i]].
				bw;
			qos_bw.ws = info->node_info->ws;
			msm_bus_noc_set_qos_bw(ninfo->base,
				ninfo->qos_baseoffset,
				ninfo->qos_freq,
				info->node_info->qport[i], ninfo->qos_delta,
				info->node_info->perm_mode, &qos_bw);
			MSM_BUS_DBG("NOC: QoS: Update mas_bw: ws: %u\n",
				qos_bw.ws);
		}
	}

skip_mas_bw:
	ports = hop->node_info->num_sports;
	for (i = 0; i < ports; i++) {
		sel_cd->slv[hop->node_info->slavep[i]].bw += add_bw;
		sel_cd->slv[hop->node_info->slavep[i]].hw_id =
			hop->node_info->slv_hw_id;
		MSM_BUS_DBG("NOC: Update slave_bw for ID: %d -> %llu\n",
			hop->node_info->priv_id,
			sel_cd->slv[hop->node_info->slavep[i]].bw);
		MSM_BUS_DBG("NOC: Update slave_bw for hw_id: %d, index: %d\n",
			hop->node_info->slv_hw_id, hop->node_info->slavep[i]);
		/* Check if hop is a shared slave.
		 * If it is, mark it dirty
		 * If it isn't, then nothing to be done as the
		 * slaves are in bypass mode.
		 **/
		if (hop->node_info->hw_sel == MSM_BUS_RPM)
			sel_cd->slv[hop->node_info->slavep[i]].dirty = 1;
	}
}

static int msm_bus_noc_commit(struct msm_bus_fabric_registration
	*fab_pdata, void *hw_data, void **cdata)
{
	MSM_BUS_DBG("\nReached NOC Commit\n");
	msm_bus_remote_hw_commit(fab_pdata, hw_data, cdata);
	return 0;
}

static int msm_bus_noc_port_halt(uint32_t haltid, uint8_t mport)
{
	return 0;
}

static int msm_bus_noc_port_unhalt(uint32_t haltid, uint8_t mport)
{
	return 0;
}

int msm_bus_noc_hw_init(struct msm_bus_fabric_registration *pdata,
	struct msm_bus_hw_algorithm *hw_algo)
{
	/* Set interleaving to true by default */
	pdata->il_flag = true;
	hw_algo->allocate_commit_data = msm_bus_noc_allocate_commit_data;
	hw_algo->allocate_hw_data = msm_bus_noc_allocate_noc_data;
	hw_algo->node_init = msm_bus_noc_node_init;
	hw_algo->free_commit_data = free_commit_data;
	hw_algo->update_bw = msm_bus_noc_update_bw;
	hw_algo->commit = msm_bus_noc_commit;
	hw_algo->port_halt = msm_bus_noc_port_halt;
	hw_algo->port_unhalt = msm_bus_noc_port_unhalt;
	hw_algo->update_bw_reg = msm_bus_noc_update_bw_reg;
	hw_algo->config_master = NULL;
	hw_algo->config_limiter = NULL;

	return 0;
}

