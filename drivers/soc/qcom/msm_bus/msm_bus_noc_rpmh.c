/* Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
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

#include <linux/bitops.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/msm-bus-board.h>
#include "msm_bus_core.h"
#include "msm_bus_noc.h"
#include "msm_bus_rpmh.h"

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
#define READ_TIMEOUT_MS	msecs_to_jiffies(1)
#define READ_DELAY_US	10

#define NOC_QOS_REG_BASE(b, o)		((b) + (o))

/*Sideband Manager Disable Macros*/
#define DISABLE_SBM_FLAGOUTCLR0_LOW_OFF		0x80
#define DISABLE_SBM_FLAGOUTCLR0_HIGH_OFF	0x84
#define DISABLE_SBM_FLAGOUTSET0_LOW_OFF		0x88
#define DISABLE_SBM_FLAGOUTSET0_HIGH_OFF	0x8C
#define DISABLE_SBM_FLAGOUTSTATUS0_LOW_OFF	0x90
#define DISABLE_SBM_FLAGOUTSTATUS0_HIGH_OFF	0x94
#define DISABLE_SBM_SENSEIN0_LOW_OFF		0x100
#define DISABLE_SBM_SENSEIN0_HIGH_OFF		0x104

#define DISABLE_SBM_REG_BASE(b, o, d)	((b) + (o) + (d))

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

static void noc_set_qos_limiter(void __iomem *base, uint32_t qos_off,
		uint32_t mport, uint32_t qos_delta,
		struct msm_bus_noc_limiter *lim, uint32_t lim_en)
{
	uint32_t reg_val, val;

	reg_val = readl_relaxed(NOC_QOS_MAINCTL_LOWn_ADDR(base, qos_off, mport,
		qos_delta));

	writel_relaxed((reg_val & (~(NOC_QOS_MCTL_LIMIT_ENn_BMSK))),
		NOC_QOS_MAINCTL_LOWn_ADDR(base, qos_off, mport, qos_delta));

	/* Ensure we disable limiter before config*/
	wmb();

	reg_val = readl_relaxed(NOC_QOS_LIMITBWn_ADDR(base, qos_off, mport,
		qos_delta));
	val = lim->bw << NOC_QOS_LIMITBW_BWn_SHFT;
	writel_relaxed(((reg_val & (~(NOC_QOS_LIMITBW_BWn_BMSK))) |
		(val & NOC_QOS_LIMITBW_BWn_BMSK)),
		NOC_QOS_LIMITBWn_ADDR(base, qos_off, mport, qos_delta));

	reg_val = readl_relaxed(NOC_QOS_LIMITBWn_ADDR(base, qos_off, mport,
		qos_delta));
	val = lim->sat << NOC_QOS_LIMITBW_SATn_SHFT;
	writel_relaxed(((reg_val & (~(NOC_QOS_LIMITBW_SATn_BMSK))) |
		(val & NOC_QOS_LIMITBW_SATn_BMSK)),
		NOC_QOS_LIMITBWn_ADDR(base, qos_off, mport, qos_delta));

	/* Ensure qos limiter settings in place before possibly enabling */
	wmb();

	reg_val = readl_relaxed(NOC_QOS_MAINCTL_LOWn_ADDR(base, qos_off, mport,
		qos_delta));
	val = lim_en << NOC_QOS_MCTL_LIMIT_ENn_SHFT;
	writel_relaxed(((reg_val & (~(NOC_QOS_MCTL_LIMIT_ENn_BMSK))) |
		(val & NOC_QOS_MCTL_LIMIT_ENn_BMSK)),
		NOC_QOS_MAINCTL_LOWn_ADDR(base, qos_off, mport, qos_delta));

	/* Ensure qos limiter writes take place before exiting*/
	wmb();
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
				void __iomem *qos_base,
				uint32_t qos_off, uint32_t qos_delta,
				uint32_t qos_freq)
{
	struct msm_bus_noc_qos_params *qos_params;
	int ret = 0;
	int i;

	qos_params = &info->node_info->qos_params;

	if (!info->node_info->qport) {
		MSM_BUS_DBG("No QoS Ports to init\n");
		ret = 0;
		goto err_qos_init;
	}

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
err_qos_init:
	return ret;
}

static int msm_bus_noc_sbm_config(struct msm_bus_node_device_type *node_dev,
				void __iomem *noc_base, uint32_t sbm_offset,
				bool enable)
{
	int ret = 0, idx;
	unsigned long j, j_timeout;
	uint32_t flagset_offset, flagclr_offset, sense_offset;

	for (idx = 0; idx < node_dev->node_info->num_disable_ports; idx++) {
		uint32_t disable_port = node_dev->node_info->disable_ports[idx];
		uint32_t reg_val = 0;

		if (disable_port >= 64) {
			return -EINVAL;
		} else if (disable_port < 32) {
			flagset_offset = DISABLE_SBM_FLAGOUTSET0_LOW_OFF;
			flagclr_offset = DISABLE_SBM_FLAGOUTCLR0_LOW_OFF;
			sense_offset = DISABLE_SBM_SENSEIN0_LOW_OFF;
		} else {
			flagset_offset = DISABLE_SBM_FLAGOUTSET0_HIGH_OFF;
			flagclr_offset = DISABLE_SBM_FLAGOUTCLR0_HIGH_OFF;
			sense_offset = DISABLE_SBM_SENSEIN0_HIGH_OFF;
			disable_port = disable_port - 32;
		}

		if (enable) {
			reg_val |= 0x1 << disable_port;
			writel_relaxed(reg_val, DISABLE_SBM_REG_BASE(noc_base,
					sbm_offset, flagclr_offset));
			/* Ensure SBM reconnect took place */
			wmb();

			j = jiffies;
			j_timeout = j + READ_TIMEOUT_MS;
			while (((0x1 << disable_port) &
				readl_relaxed(DISABLE_SBM_REG_BASE(noc_base,
				sbm_offset, sense_offset)))) {
				udelay(READ_DELAY_US);
				j = jiffies;
				if (time_after(j, j_timeout)) {
					MSM_BUS_ERR("%s: SBM enable timeout.\n",
								 __func__);
					goto sbm_timeout;
				}
			}
		} else {
			reg_val |= 0x1 << disable_port;
			writel_relaxed(reg_val, DISABLE_SBM_REG_BASE(noc_base,
					sbm_offset, flagset_offset));
			/* Ensure SBM disconnect took place */
			wmb();

			j = jiffies;
			j_timeout = j + READ_TIMEOUT_MS;
			while (!((0x1 << disable_port) &
				readl_relaxed(DISABLE_SBM_REG_BASE(noc_base,
				sbm_offset, sense_offset)))) {
				udelay(READ_DELAY_US);
				j = jiffies;
				if (time_after(j, j_timeout)) {
					MSM_BUS_ERR("%s: SBM disable timeout.\n"
								, __func__);
					goto sbm_timeout;
				}
			}
		}
	}
	return ret;

sbm_timeout:
	return -ETIME;

}

int msm_bus_noc_set_ops(struct msm_bus_node_device_type *bus_dev)
{
	if (!bus_dev)
		return -ENODEV;

	bus_dev->fabdev->noc_ops.qos_init = msm_bus_noc_qos_init;
	bus_dev->fabdev->noc_ops.sbm_config = msm_bus_noc_sbm_config;

	return 0;
}
EXPORT_SYMBOL(msm_bus_noc_set_ops);
