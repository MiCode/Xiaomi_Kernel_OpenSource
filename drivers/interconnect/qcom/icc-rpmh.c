// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 *
 */

#include <asm/div64.h>
#include <dt-bindings/interconnect/qcom,sdm845.h>
#include <linux/clk.h>
#include <linux/interconnect.h>
#include <linux/interconnect-provider.h>
#include <linux/module.h>

#include "icc-rpmh.h"
#include "bcm-voter.h"
#include "qnoc-qos.h"

/**
 * qcom_icc_pre_aggregate - cleans up stale values from prior icc_set
 * @node: icc node to operate on
 */
void qcom_icc_pre_aggregate(struct icc_node *node)
{
	size_t i;
	struct qcom_icc_node *qn;

	qn = node->data;

	for (i = 0; i < QCOM_ICC_NUM_BUCKETS; i++) {
		qn->sum_avg[i] = 0;
		qn->max_peak[i] = 0;
	}
}
EXPORT_SYMBOL(qcom_icc_pre_aggregate);

/**
 * qcom_icc_aggregate - aggregate bw for buckets indicated by tag
 * @node: node to aggregate
 * @tag: tag to indicate which buckets to aggregate
 * @avg_bw: new bw to sum aggregate
 * @peak_bw: new bw to max aggregate
 * @agg_avg: existing aggregate avg bw val
 * @agg_peak: existing aggregate peak bw val
 */
int qcom_icc_aggregate(struct icc_node *node, u32 tag, u32 avg_bw,
		       u32 peak_bw, u32 *agg_avg, u32 *agg_peak)
{
	size_t i;
	struct qcom_icc_node *qn;
	struct qcom_icc_provider *qp;

	qn = node->data;
	qp = to_qcom_provider(node->provider);

	if (!tag)
		tag = QCOM_ICC_TAG_ALWAYS;

	for (i = 0; i < QCOM_ICC_NUM_BUCKETS; i++) {
		if (tag & BIT(i)) {
			qn->sum_avg[i] += avg_bw;
			qn->max_peak[i] = max_t(u32, qn->max_peak[i], peak_bw);
		}
	}

	*agg_avg += avg_bw;
	*agg_peak = max_t(u32, *agg_peak, peak_bw);

	for (i = 0; i < qn->num_bcms; i++)
		qcom_icc_bcm_voter_add(qp->voters[qn->bcms[i]->voter_idx],
				       qn->bcms[i]);

	return 0;
}
EXPORT_SYMBOL(qcom_icc_aggregate);

/**
 * qcom_icc_set - set the constraints based on path
 * @src: source node for the path to set constraints on
 * @dst: destination node for the path to set constraints on
 *
 * Return: 0 on success, or an error code otherwise
 */
int qcom_icc_set(struct icc_node *src, struct icc_node *dst)
{
	struct qcom_icc_provider *qp;
	struct qcom_icc_node *qn;
	struct icc_node *node;
	int i, ret = 0;

	if (!src)
		node = dst;
	else
		node = src;

	qp = to_qcom_provider(node->provider);
	qn = node->data;

	for (i = 0; i < qp->num_voters; i++)
		qcom_icc_bcm_voter_commit(qp->voters[i]);

	/* Defer setting QoS until the first non-zero bandwidth request. */
	if (qn && qn->qosbox && !qn->qosbox->initialized &&
	    (node->avg_bw || node->peak_bw)) {
		ret = clk_bulk_prepare_enable(qp->num_clks, qp->clks);
		if (ret) {
			pr_err("%s: Clock enable failed for node %s\n",
				__func__, node->name);
			return ret;
		}

		qn->noc_ops->set_qos(qn);
		clk_bulk_disable_unprepare(qp->num_clks, qp->clks);
		qn->qosbox->initialized = true;
	}

	return ret;
}
EXPORT_SYMBOL(qcom_icc_set);

/**
 * qcom_icc_bcm_init - populates bcm aux data and connect qnodes
 * @bcm: bcm to be initialized
 * @dev: associated provider device
 *
 * Return: 0 on success, or an error code otherwise
 */
int qcom_icc_bcm_init(struct qcom_icc_bcm *bcm, struct device *dev)
{
	struct qcom_icc_node *qn;
	const struct bcm_db *data;
	size_t data_count;
	int i;

	/* BCM is already initialised*/
	if (bcm->addr)
		return 0;

	bcm->addr = cmd_db_read_addr(bcm->name);
	if (!bcm->addr) {
		dev_err(dev, "%s could not find RPMh address\n",
			bcm->name);
		return -EINVAL;
	}

	data = cmd_db_read_aux_data(bcm->name, &data_count);
	if (IS_ERR(data)) {
		dev_err(dev, "%s command db read error (%ld)\n",
			bcm->name, PTR_ERR(data));
		return PTR_ERR(data);
	}
	if (!data_count) {
		dev_err(dev, "%s command db missing or partial aux data\n",
			bcm->name);
		return -EINVAL;
	}

	bcm->aux_data.unit = data->unit;
	bcm->aux_data.width = data->width;
	bcm->aux_data.vcd = data->vcd;
	bcm->aux_data.reserved = data->reserved;
	INIT_LIST_HEAD(&bcm->list);
	INIT_LIST_HEAD(&bcm->ws_list);

	if (!bcm->vote_scale)
		bcm->vote_scale = 1000;

	/*
	 * Link Qnodes to their respective BCMs
	 */
	for (i = 0; i < bcm->num_nodes; i++) {
		qn = bcm->nodes[i];
		qn->bcms[qn->num_bcms] = bcm;
		qn->num_bcms++;
	}

	return 0;
}
EXPORT_SYMBOL(qcom_icc_bcm_init);

static bool bcm_needs_qos_proxy(struct qcom_icc_bcm *bcm)
{
	int i;

	if (bcm->voter_idx == 0)
		for (i = 0; i < bcm->num_nodes; i++)
			if (bcm->nodes[i]->qosbox)
				return true;

	return false;
}

/**
 * qcom_icc_enable_qos_deps - enable clocks and BCMs required for QoS
 * @qp: interconnect provider associated with masters whose QoS to be set
 *
 * Return: 0 on success, or an error code otherwise
 */
int qcom_icc_enable_qos_deps(struct qcom_icc_provider *qp)
{
	struct qcom_icc_bcm *bcm;
	struct bcm_voter *voter;
	bool keepalive;
	int ret, i;

	for (i = 0; i < qp->num_bcms; i++) {
		bcm = qp->bcms[i];
		if (bcm_needs_qos_proxy(bcm)) {
			keepalive = bcm->keepalive;
			bcm->keepalive = true;

			voter = qp->voters[bcm->voter_idx];
			qcom_icc_bcm_voter_add(voter, bcm);
			ret = qcom_icc_bcm_voter_commit(voter);

			bcm->keepalive = keepalive;

			if (ret) {
				dev_err(qp->dev, "failed to vote BW to %s for QoS\n",
					bcm->name);
				return ret;
			}
		}
	}

	ret = clk_bulk_prepare_enable(qp->num_clks, qp->clks);
	if (ret) {
		dev_err(qp->dev, "failed to enable clocks for QoS\n");
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(qcom_icc_enable_qos_deps);

/**
 * qcom_icc_disable_qos_deps - disable clocks and BCMs
 * @qp: interconnect provider associated with masters whose QoS to be set
 */
void qcom_icc_disable_qos_deps(struct qcom_icc_provider *qp)
{
	struct qcom_icc_bcm *bcm;
	struct bcm_voter *voter;
	int i;

	clk_bulk_disable_unprepare(qp->num_clks, qp->clks);

	for (i = 0; i < qp->num_bcms; i++) {
		bcm = qp->bcms[i];
		if (bcm_needs_qos_proxy(bcm)) {
			voter = qp->voters[bcm->voter_idx];
			qcom_icc_bcm_voter_add(voter, bcm);
			qcom_icc_bcm_voter_commit(voter);
		}
	}
}
EXPORT_SYMBOL(qcom_icc_disable_qos_deps);

MODULE_LICENSE("GPL v2");
