/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

static bool msm_bus_noc_update_bw_reg(int mode)
{
	bool ret = false;

	if ((mode == NOC_QOS_MODE_LIMITER) ||
			(mode == NOC_QOS_MODE_REGULATOR))
			ret = true;

	return ret;
}

static int msm_bus_noc_qos_init(struct msm_bus_node_device_type *info,
				void __iomem *qos_base,
				uint32_t qos_off, uint32_t qos_delta,
				uint32_t qos_freq)
{
	struct msm_bus_noc_qos_priority prio;
	int ret = 0;
	int i;

	prio.p1 = info->node_info->qos_params.prio1;
	prio.p0 = info->node_info->qos_params.prio0;

	if (!info->node_info->qport) {
		MSM_BUS_DBG("No QoS Ports to init\n");
		ret = 0;
		goto err_qos_init;
	}

	for (i = 0; i < info->node_info->num_qports; i++) {
		if (info->node_info->qos_params.mode != NOC_QOS_MODE_BYPASS) {
			noc_set_qos_priority(qos_base, qos_off,
					info->node_info->qport[i], qos_delta,
					&prio);

			if (info->node_info->qos_params.mode !=
							NOC_QOS_MODE_FIXED) {
				struct msm_bus_noc_qos_bw qbw;

				qbw.ws = info->node_info->qos_params.ws;
				qbw.bw = 0;
				msm_bus_noc_set_qos_bw(qos_base, qos_off,
					qos_freq,
					info->node_info->qport[i],
					qos_delta,
					info->node_info->qos_params.mode,
					&qbw);
			}
		}

		noc_set_qos_mode(qos_base, qos_off, info->node_info->qport[i],
				qos_delta, info->node_info->qos_params.mode,
				(1 << info->node_info->qos_params.mode));
	}
err_qos_init:
	return ret;
}

static int msm_bus_noc_set_bw(struct msm_bus_node_device_type *dev,
				void __iomem *qos_base,
				uint32_t qos_off, uint32_t qos_delta,
				uint32_t qos_freq)
{
	int ret = 0;
	uint64_t bw = 0;
	int i;
	struct msm_bus_node_info_type *info = dev->node_info;

	if (info && info->num_qports &&
		((info->qos_params.mode == NOC_QOS_MODE_REGULATOR) ||
		(info->qos_params.mode ==
			NOC_QOS_MODE_LIMITER))) {
		struct msm_bus_noc_qos_bw qos_bw;

		bw = msm_bus_div64(info->num_qports,
				dev->node_bw[ACTIVE_CTX].sum_ab);

		for (i = 0; i < info->num_qports; i++) {
			if (!info->qport) {
				MSM_BUS_DBG("No qos ports to update!\n");
				break;
			}

			qos_bw.bw = bw;
			qos_bw.ws = info->qos_params.ws;
			msm_bus_noc_set_qos_bw(qos_base, qos_off, qos_freq,
				info->qport[i], qos_delta,
				(1 << info->qos_params.mode), &qos_bw);
			MSM_BUS_DBG("NOC: QoS: Update mas_bw: ws: %u\n",
				qos_bw.ws);
		}
	}
	return ret;
}

static int msm_bus_noc_set_lim_mode(struct msm_bus_node_device_type *info,
				void __iomem *qos_base, uint32_t qos_off,
				uint32_t qos_delta, uint32_t qos_freq,
				u64 lim_bw)
{
	int i;

	if (info && info->node_info->num_qports) {
		struct msm_bus_noc_qos_bw qos_bw;

		if (lim_bw != info->node_info->lim_bw) {
			for (i = 0; i < info->node_info->num_qports; i++) {
				qos_bw.bw = lim_bw;
				qos_bw.ws = info->node_info->qos_params.ws;
					msm_bus_noc_set_qos_bw(qos_base,
					qos_off, qos_freq,
					info->node_info->qport[i], qos_delta,
					(1 << NOC_QOS_MODE_LIMITER), &qos_bw);
			}
			info->node_info->lim_bw = lim_bw;
		}

		for (i = 0; i < info->node_info->num_qports; i++) {
			noc_set_qos_mode(qos_base, qos_off,
					info->node_info->qport[i],
					qos_delta,
					NOC_QOS_MODE_LIMITER,
					(1 << NOC_QOS_MODE_LIMITER));
		}
	}

	return 0;
}

static int msm_bus_noc_set_reg_mode(struct msm_bus_node_device_type *info,
				void __iomem *qos_base, uint32_t qos_off,
				uint32_t qos_delta, uint32_t qos_freq,
				u64 lim_bw)
{
	int i;

	if (info && info->node_info->num_qports) {
		struct msm_bus_noc_qos_priority prio;
		struct msm_bus_noc_qos_bw qos_bw;

		for (i = 0; i < info->node_info->num_qports; i++) {
			prio.p1 =
				info->node_info->qos_params.reg_prio1;
			prio.p0 =
				info->node_info->qos_params.reg_prio0;
			noc_set_qos_priority(qos_base, qos_off,
					info->node_info->qport[i],
					qos_delta,
					&prio);
		}

		if (lim_bw != info->node_info->lim_bw) {
			for (i = 0; i < info->node_info->num_qports; i++) {
				qos_bw.bw = lim_bw;
				qos_bw.ws = info->node_info->qos_params.ws;
				msm_bus_noc_set_qos_bw(qos_base, qos_off,
					qos_freq,
					info->node_info->qport[i], qos_delta,
					(1 << NOC_QOS_MODE_REGULATOR), &qos_bw);
			}
			info->node_info->lim_bw = lim_bw;
		}

		for (i = 0; i < info->node_info->num_qports; i++) {
			noc_set_qos_mode(qos_base, qos_off,
					info->node_info->qport[i],
					qos_delta,
					NOC_QOS_MODE_REGULATOR,
					(1 << NOC_QOS_MODE_REGULATOR));
		}
	}
	return 0;
}

static int msm_bus_noc_set_def_mode(struct msm_bus_node_device_type *info,
				void __iomem *qos_base, uint32_t qos_off,
				uint32_t qos_delta, uint32_t qos_freq,
				u64 lim_bw)
{
	int i;

	for (i = 0; i < info->node_info->num_qports; i++) {
		if (info->node_info->qos_params.mode ==
						NOC_QOS_MODE_FIXED) {
			struct msm_bus_noc_qos_priority prio;

			prio.p1 =
				info->node_info->qos_params.prio1;
			prio.p0 =
				info->node_info->qos_params.prio0;
			noc_set_qos_priority(qos_base, qos_off,
					info->node_info->qport[i],
					qos_delta, &prio);
		}
		noc_set_qos_mode(qos_base, qos_off,
			info->node_info->qport[i],
			qos_delta,
			info->node_info->qos_params.mode,
			(1 << info->node_info->qos_params.mode));
	}
	return 0;
}

static int msm_bus_noc_limit_mport(struct msm_bus_node_device_type *info,
				void __iomem *qos_base, uint32_t qos_off,
				uint32_t qos_delta, uint32_t qos_freq,
				int enable_lim, u64 lim_bw)
{
	int ret = 0;

	if (!(info && info->node_info->num_qports)) {
		MSM_BUS_ERR("Invalid Node info or no Qports to program");
		ret = -ENXIO;
		goto exit_limit_mport;
	}

	if (lim_bw) {
		switch (enable_lim) {
		case THROTTLE_REG:
			msm_bus_noc_set_reg_mode(info, qos_base, qos_off,
						qos_delta, qos_freq, lim_bw);
			break;
		case THROTTLE_ON:
			msm_bus_noc_set_lim_mode(info, qos_base, qos_off,
						qos_delta, qos_freq, lim_bw);
			break;
		default:
			msm_bus_noc_set_def_mode(info, qos_base, qos_off,
						qos_delta, qos_freq, lim_bw);
			break;
		}
	} else
		msm_bus_noc_set_def_mode(info, qos_base, qos_off,
					qos_delta, qos_freq, lim_bw);

exit_limit_mport:
	return ret;
}

int msm_bus_noc_set_ops(struct msm_bus_node_device_type *bus_dev)
{
	if (!bus_dev)
		return -ENODEV;

	bus_dev->fabdev->noc_ops.qos_init = msm_bus_noc_qos_init;
	bus_dev->fabdev->noc_ops.set_bw = msm_bus_noc_set_bw;
	bus_dev->fabdev->noc_ops.limit_mport = msm_bus_noc_limit_mport;
	bus_dev->fabdev->noc_ops.update_bw_reg = msm_bus_noc_update_bw_reg;

	return 0;
}
EXPORT_SYMBOL(msm_bus_noc_set_ops);
