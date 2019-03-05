/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#include <linux/io.h>
#include "msm_bus_adhoc.h"
#include "msm_bus_core.h"
#include "msm_bus_noc.h"

/* NOC_QOS generic */
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

static int msm_bus_noc_qos_init(struct msm_bus_node_device_type *info,
				void __iomem *qos_base,
				uint32_t qos_off, uint32_t qos_delta,
				uint32_t qos_freq)
{
	struct qos_params_type *qos_params;
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

int msm_bus_qnoc_set_ops(struct msm_bus_node_device_type *bus_dev)
{
	if (!bus_dev)
		return -ENODEV;

	bus_dev->fabdev->noc_ops.qos_init = msm_bus_noc_qos_init;

	return 0;
}
