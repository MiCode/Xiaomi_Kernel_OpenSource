// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 */

#include <linux/interconnect.h>
#include <linux/interconnect-provider.h>
#include <linux/module.h>

#ifdef CONFIG_INTERCONNECT_QCOM_RPMH
#include "icc-rpmh.h"
#else
#include "icc-rpm.h"
#endif
#include "qnoc-qos.h"

#define QOSGEN_MAINCTL_LO(p, qp)	((p)->offsets[qp] + \
					(p)->regs[QOSGEN_OFF_MAINCTL_LO])
# define QOS_SLV_URG_MSG_EN		BIT(3)
# define QOS_DFLT_PRIO_MASK		0x7
# define QOS_DFLT_PRIO_SHFT		4

#ifndef CONFIG_INTERCONNECT_QCOM_RPMH
#define QOSGEN_M_BKE_HEALTH(p, qp, n)	((p)->offsets[qp] + ((n) * 4) + \
				(p)->regs[QOSGEN_OFF_MPORT_BKE_HEALTH])
#define QOS_PRIOLVL_MASK		0x7
#define QOS_PRIOLVL_SHFT		0x0
#define QOS_AREQPRIO_MASK		0x70
#define QOS_AREQPRIO_SHFT		0x8

#define QOSGEN_M_BKE_EN(p, qp)	((p)->offsets[qp] + \
				(p)->regs[QOSGEN_OFF_MPORT_BKE_EN])

#define QOS_BKE_EN_MASK		0x1
#define QOS_BKE_EN_SHFT		0x0

#define NUM_BKE_HEALTH_LEVELS		4
#endif

const u8 icc_qnoc_qos_regs[][QOSGEN_OFF_MAX_REGS] = {
	[ICC_QNOC_QOSGEN_TYPE_RPMH] = {
		[QOSGEN_OFF_MAINCTL_LO] = 0x8,
		[QOSGEN_OFF_LIMITBW_LO] = 0x18,
		[QOSGEN_OFF_SHAPING_LO] = 0x20,
		[QOSGEN_OFF_SHAPING_HI] = 0x24,
		[QOSGEN_OFF_REGUL0CTL_LO] = 0x40,
		[QOSGEN_OFF_REGUL0BW_LO] = 0x48,
	},
};
EXPORT_SYMBOL(icc_qnoc_qos_regs);

#ifndef CONFIG_INTERCONNECT_QCOM_RPMH
const u8 icc_bimc_qos_regs[][QOSGEN_OFF_MAX_REGS] = {
	[ICC_QNOC_QOSGEN_TYPE_RPMH] = {
		[QOSGEN_OFF_MPORT_BKE_EN] = 0x0,
		[QOSGEN_OFF_MPORT_BKE_HEALTH] = 0x40,
	},
};
EXPORT_SYMBOL(icc_bimc_qos_regs);
#endif
/**
 * qcom_icc_set_qos - initialize static QoS configurations
 * @node: qcom icc node to operate on
 */
static void qcom_icc_set_qos(struct qcom_icc_node *node)
{
	struct qcom_icc_qosbox *qos = node->qosbox;
	int port;

	if (!node->regmap)
		return;

	if (!qos)
		return;

	for (port = 0; port < qos->num_ports; port++) {
		regmap_update_bits(node->regmap, QOSGEN_MAINCTL_LO(qos, port),
				   QOS_DFLT_PRIO_MASK << QOS_DFLT_PRIO_SHFT,
				   qos->config->prio << QOS_DFLT_PRIO_SHFT);


		if (qos->config->urg_fwd)
			regmap_update_bits(node->regmap,
				QOSGEN_MAINCTL_LO(qos, port),
				QOS_SLV_URG_MSG_EN,
				QOS_SLV_URG_MSG_EN);
		else
			regmap_update_bits(node->regmap,
				QOSGEN_MAINCTL_LO(qos, port),
				QOS_SLV_URG_MSG_EN, 0x0);
	}
}

const struct qcom_icc_noc_ops qcom_qnoc4_ops = {
	.set_qos = qcom_icc_set_qos,
};
EXPORT_SYMBOL(qcom_qnoc4_ops);

#ifndef CONFIG_INTERCONNECT_QCOM_RPMH
/**
 * qcom_icc_set_bimc_qos - initialize static QoS configurations
 * @node: qcom icc node to operate on
 */
static void qcom_icc_set_bimc_qos(struct qcom_icc_node *node)
{
	struct qcom_icc_qosbox *qos = node->qosbox;
	int port, i;

	if (!node->regmap)
		return;

	if (!qos)
		return;

	for (port = 0; port < qos->num_ports; port++) {
		for (i = 0; i < NUM_BKE_HEALTH_LEVELS; i++) {
			regmap_update_bits(node->regmap,
				QOSGEN_M_BKE_HEALTH(qos, port, i),
				((qos->config->prio << QOS_PRIOLVL_SHFT) |
				(qos->config->prio << QOS_AREQPRIO_SHFT)),
				(QOS_PRIOLVL_MASK | QOS_AREQPRIO_MASK));
		};

		regmap_update_bits(node->regmap,
			QOSGEN_M_BKE_EN(qos, port),
			qos->config->bke_enable << QOS_BKE_EN_SHFT,
			QOS_BKE_EN_MASK);
	};
}

const struct qcom_icc_noc_ops qcom_bimc_ops = {
	.set_qos = qcom_icc_set_bimc_qos,
};
EXPORT_SYMBOL(qcom_bimc_ops);
#endif

MODULE_LICENSE("GPL v2");
