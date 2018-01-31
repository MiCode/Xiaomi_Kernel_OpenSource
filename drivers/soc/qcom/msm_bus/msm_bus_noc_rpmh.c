/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
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
#include <linux/msm-bus.h>
#include <linux/spinlock.h>
#include "msm_bus_core.h"
#include "msm_bus_noc.h"
#include "msm_bus_rpmh.h"

static DEFINE_SPINLOCK(noc_lock);

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
#define QM_BASE	0x010B8000
#define MSM_BUS_FAB_MEM_NOC 6152

#define NOC_QOS_REG_BASE(b, o)		((b) + (o))

#define NOC_QOS_MAINCTL_LOWn_ADDR(b, o, n, d)	\
	(NOC_QOS_REG_BASE(b, o) + 0x8 + (d) * (n))
enum noc_qos_id_mainctl_lown {
	NOC_QOS_MCTL_DFLT_PRIOn_BMSK	= 0x00000070,
	NOC_QOS_MCTL_DFLT_PRIOn_SHFT	= 0x4,
	NOC_QOS_MCTL_URGFWD_ENn_BMSK	= 0x00000008,
	NOC_QOS_MCTL_URGFWD_ENn_SHFT	= 0x3,
	NOC_QOS_MCTL_LIMIT_ENn_BMSK	= 0x00000001,
	NOC_QOS_MCTL_LIMIT_ENn_SHFT	= 0x0,
};

#define NOC_QOS_LIMITBWn_ADDR(b, o, n, d)	\
	(NOC_QOS_REG_BASE(b, o) + 0x18 + (d) * (n))
enum noc_qos_id_limitbwn {
	NOC_QOS_LIMITBW_BWn_BMSK	= 0x000007FF,
	NOC_QOS_LIMITBW_BWn_SHFT	= 0x0,
	NOC_QOS_LIMITBW_SATn_BMSK	= 0x03FF0000,
	NOC_QOS_LIMITBW_SATn_SHFT	= 0x11,
};

#define NOC_QOS_REGUL0CTLn_ADDR(b, o, n, d)	\
	(NOC_QOS_REG_BASE(b, o) + 0x40 + (d) * (n))
enum noc_qos_id_regul0ctln {
	NOC_QOS_REGUL0CTL_HI_PRIOn_BMSK	= 0x00007000,
	NOC_QOS_REGUL0CTL_HI_PRIOn_SHFT	= 0x8,
	NOC_QOS_REGUL0CTL_LW_PRIOn_BMSK	= 0x00000700,
	NOC_QOS_REGUL0CTL_LW_PRIOn_SHFT	= 0xC,
	NOC_QOS_REGUL0CTL_WRENn_BMSK	= 0x00000002,
	NOC_QOS_REGUL0CTL_WRENn_SHFT	= 0x1,
	NOC_QOS_REGUL0CTL_RDENn_BMSK	= 0x00000001,
	NOC_QOS_REGUL0CTL_RDENn_SHFT	= 0x0,
};

#define NOC_QOS_REGUL0BWn_ADDR(b, o, n, d)	\
	(NOC_QOS_REG_BASE(b, o) + 0x48 + (d) * (n))
enum noc_qos_id_regul0bwbwn {
	NOC_QOS_REGUL0BW_BWn_BMSK	= 0x000007FF,
	NOC_QOS_REGUL0BW_BWn_SHFT	= 0x0,
	NOC_QOS_REGUL0BW_SATn_BMSK	= 0x03FF0000,
	NOC_QOS_REGUL0BW_SATn_SHFT	= 0x11,
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

#define QM_CLn_TH_LVL_MUX_ADDR(b, o, n, d)	\
	(NOC_QOS_REG_BASE(b, o) + 0x8C0 + (d) * (n))
enum qm_cl_id_th_lvl_mux_cfg {
	QM_CLn_TH_LVL_SW_OVERRD_BMSK	= 0x80000000,
	QM_CLn_TH_LVL_SW_OVERRD_SHFT	= 0x1F,
	QM_CLn_TH_LVL_SW_BMSK		= 0x00000007,
	QM_CLn_TH_LVL_SW_SHFT		= 0x0,
};

static void __iomem *qm_base;
static void __iomem *memnoc_qos_base;

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

static void noc_set_qm_th_lvl_cfg(void __iomem *base, uint32_t off,
		uint32_t n, uint32_t delta,
		uint32_t override_val, uint32_t override)
{
	writel_relaxed(((override << QM_CLn_TH_LVL_SW_OVERRD_SHFT) |
		(override_val & QM_CLn_TH_LVL_SW_BMSK)),
		QM_CLn_TH_LVL_MUX_ADDR(base, off, n, delta));

	/* Ensure QM CFG is set before exiting */
	wmb();
}

static void noc_set_qos_dflt_prio(void __iomem *base, uint32_t qos_off,
		uint32_t mport, uint32_t qos_delta,
		uint32_t prio)
{
	uint32_t reg_val, val;

	reg_val = readl_relaxed(NOC_QOS_MAINCTL_LOWn_ADDR(base, qos_off, mport,
		qos_delta));
	val = prio << NOC_QOS_MCTL_DFLT_PRIOn_SHFT;
	writel_relaxed(((reg_val & (~(NOC_QOS_MCTL_DFLT_PRIOn_BMSK))) |
		(val & NOC_QOS_MCTL_DFLT_PRIOn_BMSK)),
		NOC_QOS_MAINCTL_LOWn_ADDR(base, qos_off, mport, qos_delta));

	/* Ensure qos priority is set before exiting */
	wmb();
}

static void noc_enable_qos_limiter(void __iomem *base, uint32_t qos_off,
		uint32_t mport, uint32_t qos_delta, uint32_t lim_en)
{
	uint32_t reg_val, val;
	reg_val = readl_relaxed(NOC_QOS_MAINCTL_LOWn_ADDR(base, qos_off, mport,
		qos_delta));
	val = lim_en << NOC_QOS_MCTL_LIMIT_ENn_SHFT;
	writel_relaxed(((reg_val & (~(NOC_QOS_MCTL_LIMIT_ENn_BMSK))) |
		(val & NOC_QOS_MCTL_LIMIT_ENn_BMSK)),
		NOC_QOS_MAINCTL_LOWn_ADDR(base, qos_off, mport, qos_delta));

	/* Ensure we disable/enable limiter before exiting*/
	wmb();
}

static void noc_set_qos_limit_bw(void __iomem *base, uint32_t qos_off,
		uint32_t mport, uint32_t qos_delta, uint32_t bw)
{
	uint32_t reg_val, val;
	reg_val = readl_relaxed(NOC_QOS_LIMITBWn_ADDR(base, qos_off, mport,
		qos_delta));
	val = bw << NOC_QOS_LIMITBW_BWn_SHFT;
	writel_relaxed(((reg_val & (~(NOC_QOS_LIMITBW_BWn_BMSK))) |
		(val & NOC_QOS_LIMITBW_BWn_BMSK)),
		NOC_QOS_LIMITBWn_ADDR(base, qos_off, mport, qos_delta));

	/* Ensure we set limiter bw before exiting*/
	wmb();
}

static void noc_set_qos_limit_sat(void __iomem *base, uint32_t qos_off,
		uint32_t mport, uint32_t qos_delta, uint32_t sat)
{
	uint32_t reg_val, val;
	reg_val = readl_relaxed(NOC_QOS_LIMITBWn_ADDR(base, qos_off, mport,
		qos_delta));
	val = sat << NOC_QOS_LIMITBW_SATn_SHFT;
	writel_relaxed(((reg_val & (~(NOC_QOS_LIMITBW_SATn_BMSK))) |
		(val & NOC_QOS_LIMITBW_SATn_BMSK)),
		NOC_QOS_LIMITBWn_ADDR(base, qos_off, mport, qos_delta));

	/* Ensure we set limiter sat before exiting*/
	wmb();
}

static void noc_set_qos_limiter(void __iomem *base, uint32_t qos_off,
		uint32_t mport, uint32_t qos_delta,
		struct msm_bus_noc_limiter *lim, uint32_t lim_en)
{
	noc_enable_qos_limiter(base, qos_off, mport, qos_delta, 0);
	noc_set_qos_limit_bw(base, qos_off, mport, qos_delta, lim->bw);
	noc_set_qos_limit_sat(base, qos_off, mport, qos_delta, lim->sat);
	noc_enable_qos_limiter(base, qos_off, mport, qos_delta, lim_en);
}

static void noc_set_qos_regulator(void __iomem *base, uint32_t qos_off,
		uint32_t mport, uint32_t qos_delta,
		struct msm_bus_noc_regulator *reg,
		struct msm_bus_noc_regulator_mode *reg_mode)
{
	uint32_t reg_val, val;

	reg_val = readl_relaxed(NOC_QOS_REGUL0CTLn_ADDR(base, qos_off, mport,
		qos_delta)) & (NOC_QOS_REGUL0CTL_WRENn_BMSK |
						NOC_QOS_REGUL0CTL_RDENn_BMSK);

	writel_relaxed((reg_val & (~(NOC_QOS_REGUL0CTL_WRENn_BMSK |
						NOC_QOS_REGUL0CTL_RDENn_BMSK))),
		NOC_QOS_REGUL0CTLn_ADDR(base, qos_off, mport, qos_delta));

	/* Ensure qos regulator is disabled before configuring */
	wmb();

	reg_val = readl_relaxed(NOC_QOS_REGUL0CTLn_ADDR(base, qos_off, mport,
		qos_delta)) & NOC_QOS_REGUL0CTL_HI_PRIOn_BMSK;
	val = reg->hi_prio << NOC_QOS_REGUL0CTL_HI_PRIOn_SHFT;
	writel_relaxed(((reg_val & (~(NOC_QOS_REGUL0CTL_HI_PRIOn_BMSK))) |
		(val & NOC_QOS_REGUL0CTL_HI_PRIOn_BMSK)),
		NOC_QOS_REGUL0CTLn_ADDR(base, qos_off, mport, qos_delta));

	reg_val = readl_relaxed(NOC_QOS_REGUL0CTLn_ADDR(base, qos_off, mport,
		qos_delta)) & NOC_QOS_REGUL0CTL_LW_PRIOn_BMSK;
	val = reg->low_prio << NOC_QOS_REGUL0CTL_LW_PRIOn_SHFT;
	writel_relaxed(((reg_val & (~(NOC_QOS_REGUL0CTL_LW_PRIOn_BMSK))) |
		(val & NOC_QOS_REGUL0CTL_LW_PRIOn_BMSK)),
		NOC_QOS_REGUL0CTLn_ADDR(base, qos_off, mport, qos_delta));

	reg_val = readl_relaxed(NOC_QOS_REGUL0BWn_ADDR(base, qos_off, mport,
		qos_delta)) & NOC_QOS_REGUL0BW_BWn_BMSK;
	val = reg->bw << NOC_QOS_REGUL0BW_BWn_SHFT;
	writel_relaxed(((reg_val & (~(NOC_QOS_REGUL0BW_BWn_BMSK))) |
		(val & NOC_QOS_REGUL0BW_BWn_BMSK)),
		NOC_QOS_REGUL0BWn_ADDR(base, qos_off, mport, qos_delta));

	reg_val = readl_relaxed(NOC_QOS_REGUL0BWn_ADDR(base, qos_off, mport,
		qos_delta)) & NOC_QOS_REGUL0BW_SATn_BMSK;
	val = reg->sat << NOC_QOS_REGUL0BW_SATn_SHFT;
	writel_relaxed(((reg_val & (~(NOC_QOS_REGUL0BW_SATn_BMSK))) |
		(val & NOC_QOS_REGUL0BW_SATn_BMSK)),
		NOC_QOS_REGUL0BWn_ADDR(base, qos_off, mport, qos_delta));

	/* Ensure regulator is configured before possibly enabling */
	wmb();

	reg_val = readl_relaxed(NOC_QOS_REGUL0CTLn_ADDR(base, qos_off, mport,
		qos_delta));
	val = reg_mode->write << NOC_QOS_REGUL0CTL_WRENn_SHFT;
	writel_relaxed(((reg_val & (~(NOC_QOS_REGUL0CTL_WRENn_BMSK))) |
		(val & NOC_QOS_REGUL0CTL_WRENn_BMSK)),
		NOC_QOS_REGUL0CTLn_ADDR(base, qos_off, mport, qos_delta));

	reg_val = readl_relaxed(NOC_QOS_REGUL0CTLn_ADDR(base, qos_off, mport,
		qos_delta));
	val = reg_mode->read << NOC_QOS_REGUL0CTL_RDENn_SHFT;
	writel_relaxed(((reg_val & (~(NOC_QOS_REGUL0CTL_RDENn_BMSK))) |
		(val & NOC_QOS_REGUL0CTL_RDENn_BMSK)),
		NOC_QOS_REGUL0CTLn_ADDR(base, qos_off, mport, qos_delta));

	/* Ensure regulator is ready before exiting */
	wmb();
}

static void noc_set_qos_forwarding(void __iomem *base, uint32_t qos_off,
		uint32_t mport, uint32_t qos_delta,
		bool urg_fwd_en)
{
	uint32_t reg_val, val;

	reg_val = readl_relaxed(NOC_QOS_MAINCTL_LOWn_ADDR(base, qos_off, mport,
		qos_delta));
	val = (urg_fwd_en ? 1:0) << NOC_QOS_MCTL_URGFWD_ENn_SHFT;
	writel_relaxed(((reg_val & (~(NOC_QOS_MCTL_URGFWD_ENn_BMSK))) |
		(val & NOC_QOS_MCTL_URGFWD_ENn_BMSK)),
		NOC_QOS_MAINCTL_LOWn_ADDR(base, qos_off, mport, qos_delta));

	/* Ensure qos priority is set before exiting */
	wmb();
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

static int msm_bus_noc_qos_init(struct msm_bus_node_device_type *info,
				struct msm_bus_node_device_type *fabdev,
				void __iomem *qos_base,
				uint32_t qos_off, uint32_t qos_delta,
				uint32_t qos_freq)
{
	struct msm_bus_noc_qos_params *qos_params;
	int ret = 0;
	int i;
	unsigned long flags;

	qos_params = &info->node_info->qos_params;

	if (!info->node_info->qport) {
		MSM_BUS_DBG("No QoS Ports to init\n");
		ret = 0;
		goto err_qos_init;
	}

	if (!qm_base) {
		qm_base = ioremap_nocache(QM_BASE, 0x4000);
		if (!qm_base) {
			MSM_BUS_ERR("%s: Error remapping address 0x%zx",
				__func__, (size_t)QM_BASE);
			ret = -ENOMEM;
			goto err_qos_init;
		}
	}

	spin_lock_irqsave(&noc_lock, flags);

	if (fabdev->node_info->id == MSM_BUS_FAB_MEM_NOC)
		memnoc_qos_base = qos_base;

	for (i = 0; i < info->node_info->num_qports; i++) {
		noc_set_qos_dflt_prio(qos_base, qos_off,
					info->node_info->qport[i],
					qos_delta,
					qos_params->prio_dflt);

		noc_set_qos_limiter(qos_base, qos_off,
					info->node_info->qport[i],
					qos_delta,
					&qos_params->limiter,
					qos_params->limiter_en);

		noc_set_qos_regulator(qos_base, qos_off,
					info->node_info->qport[i],
					qos_delta,
					&qos_params->reg,
					&qos_params->reg_mode);

		noc_set_qos_forwarding(qos_base, qos_off,
					info->node_info->qport[i],
					qos_delta,
					qos_params->urg_fwd_en);
	}
	spin_unlock_irqrestore(&noc_lock, flags);

err_qos_init:
	return ret;
}

int msm_bus_noc_throttle_wa(bool enable)
{
	unsigned long flags;

	spin_lock_irqsave(&noc_lock, flags);

	if (!qm_base) {
		MSM_BUS_ERR("QM CFG base address not found!");
		goto noc_throttle_exit;
	}

	if (enable) {
		noc_set_qm_th_lvl_cfg(qm_base, 0x1000, 8, 0x4, 0x3, 0x1);
		noc_set_qm_th_lvl_cfg(qm_base, 0x1000, 9, 0x4, 0x3, 0x1);
	} else {
		noc_set_qm_th_lvl_cfg(qm_base, 0x1000, 8, 0x4, 0, 0);
		noc_set_qm_th_lvl_cfg(qm_base, 0x1000, 9, 0x4, 0, 0);
	}

noc_throttle_exit:
	spin_unlock_irqrestore(&noc_lock, flags);
	return 0;
}
EXPORT_SYMBOL(msm_bus_noc_throttle_wa);

int msm_bus_noc_priority_wa(bool enable)
{
	unsigned long flags;

	spin_lock_irqsave(&noc_lock, flags);
	if (!memnoc_qos_base) {
		MSM_BUS_ERR("Memnoc QoS Base address not found!");
		goto noc_priority_exit;
	}

	if (enable)
		noc_set_qos_dflt_prio(memnoc_qos_base, 0x10000, 0,
								0x1000, 7);
	else
		noc_set_qos_dflt_prio(memnoc_qos_base, 0x10000, 0,
								0x1000, 6);
noc_priority_exit:
	spin_unlock_irqrestore(&noc_lock, flags);
	return 0;
}
EXPORT_SYMBOL(msm_bus_noc_priority_wa);

int msm_bus_noc_set_ops(struct msm_bus_node_device_type *bus_dev)
{
	if (!bus_dev)
		return -ENODEV;

	bus_dev->fabdev->noc_ops.qos_init = msm_bus_noc_qos_init;

	return 0;
}
EXPORT_SYMBOL(msm_bus_noc_set_ops);
