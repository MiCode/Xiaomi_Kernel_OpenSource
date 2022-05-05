// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/of.h>

#include "apu_common.h"
#include "apu_log.h"
#include "apu_plat.h"
#include "apu_dbg.h"

/**
 * get_datas() - return governor data that may needs
 * @gov_data:	governor data with child_freq (input)
 * @pgov_data:	parent's governor data		  (output)
 * @ad:		apu_dev of this devfreq		  (output)
 * @dev:		struct device of this defreq  (output)
 *
 * This funciton will based on inputparamter, gov_data, to output
 * pgov_data, ad, dev with call by reference.
 *
 */
void get_datas(struct apu_gov_data *gov_data, struct apu_gov_data **pgov_data,
	       struct apu_dev **ad, struct device **dev)
{
	struct device *pdev = NULL;

	if (IS_ERR_OR_NULL(gov_data)) {
		pr_info("%s null gov_data\n", __func__);
		return;
	}

	pdev = gov_data->this->dev.parent;
	/* return pgov_data */
	if (!IS_ERR_OR_NULL(pgov_data))
		if (!IS_ERR_OR_NULL(gov_data->parent))
			*pgov_data = (struct apu_gov_data *)gov_data->parent->data;

	/* return apu_dev */
	if (ad)
		*ad = dev_get_drvdata(pdev);

	/* return struct device */
	if (dev)
		*dev = pdev;
}

int apu_cmp(void *priv, struct list_head *a, struct list_head *b)
{
	struct apu_req *ta, *tb;

	ta = container_of(a, struct apu_req, list);
	tb = container_of(b, struct apu_req, list);

	return ta->value - tb->value;
}

struct apu_gov_data *apu_gov_init(struct device *dev,
				  struct devfreq_dev_profile *pf, const char **gov_name)
{
	struct apu_gov_data *pgov_data;

	pgov_data = devm_kzalloc(dev, sizeof(*pgov_data), GFP_KERNEL);
	if (!pgov_data)
		return ERR_PTR(-ENOMEM);

	pgov_data->parent = devfreq_get_devfreq_by_phandle(dev, "devfreq", 0);
	if (of_property_read_u32(dev->of_node, "depth", &pgov_data->depth))
		goto free_passdata;
	if (of_property_read_string(dev->of_node, "gov", gov_name))
		goto free_passdata;
	if (!pgov_data->depth)
		pf->polling_ms = APUGOV_POLL_RATE;

	aprobe_info(dev, " has \"%s\" devfreq parent, depth %d, poll rate %dms, gov %s\n",
		    (!IS_ERR(pgov_data->parent)) ?
		    apu_dev_name(pgov_data->parent->dev.parent) : "no",
		    pgov_data->depth, pf->polling_ms, *gov_name);

	return pgov_data;

free_passdata:
	devm_kfree(dev, pgov_data);
	aprobe_err(dev, "[%s] get depth fail\n", __func__);
	return ERR_PTR(-EINVAL);
}

void apu_dump_list(struct apu_gov_data *gov_data)
{
	struct apu_req *ptr;
	char buffer[__LOG_BUF_LEN];
	int n_pos = 0;

	if (apupw_dbg_get_loglvl() >= VERBOSE_LVL) {
		n_pos += scnprintf(buffer + n_pos, (sizeof(buffer) - n_pos),
				   "[%s] head:", apu_dev_name(gov_data->this->dev.parent));
		list_for_each_entry(ptr, &gov_data->head, list)
			if (ptr->pe_flag && !gov_data->depth)
				n_pos += scnprintf(buffer + n_pos, (sizeof(buffer) - n_pos),
						   "->%s_pe[%d]",
						   apu_dev_name(ptr->dev), ptr->value);
			else if (!ptr->pe_flag)
				n_pos += scnprintf(buffer + n_pos, (sizeof(buffer) - n_pos),
						   "->%s[%d]",
						   apu_dev_name(ptr->dev), ptr->value);
		pr_info("%s", buffer);
	}
}

void apu_dump_pe_gov(struct apu_dev *ad, struct list_head *head)
{
	struct apu_req *ptr;
	char buffer[__LOG_BUF_LEN];
	int n_pos = 0;

	if (apupw_dbg_get_loglvl() >= VERBOSE_LVL) {
		n_pos += scnprintf(buffer + n_pos, (sizeof(buffer) - n_pos),
				   "[%s][pe_gov] head:", ad->name);
		list_for_each_entry(ptr, head, list)
				n_pos += scnprintf(buffer + n_pos, (sizeof(buffer) - n_pos),
						   "->%s[%d]",
						   apu_dev_name(ptr->dev), ptr->value);
		pr_info("%s", buffer);
	}
}

int apu_gov_setup(struct apu_dev *ad, void *data)
{
	const struct apu_plat_data *apu_data = data;
	struct apu_gov_data *gov_data, *parent_gov = NULL;

	gov_data = (struct apu_gov_data *)ad->df->data;
	get_datas(gov_data, &parent_gov, NULL, NULL);

	gov_data->max_opp = ad->df->profile->max_state - 1;
	ad->opp_div = DIV_ROUND_CLOSEST(BOOST_MAX, ad->df->profile->max_state);

	/*
	 * Put threshold_volt and child_volt_limit at last,
	 * since it needs devfreq->profile->table
	 */
	if (apu_data->threshold_volt)
		gov_data->threshold_opp = apu_volt2opp(ad, apu_data->threshold_volt);
	else
		gov_data->threshold_opp = -EINVAL;

	if (apu_data->child_volt_limit)
		gov_data->child_opp_limit = apu_volt2opp(ad, apu_data->child_volt_limit);
	else
		gov_data->child_opp_limit = -EINVAL;

	/* initialize voting mechanism */
	INIT_LIST_HEAD(&gov_data->head);
	gov_data->req.value = gov_data->max_opp;
	gov_data->req.dev = ad->dev;

	/* hook user and pe reqs to self's head */
	list_add(&gov_data->req.list, &gov_data->head);
	apu_dump_list(gov_data);

	/* hook req_parent to parent's head */
	if (!IS_ERR_OR_NULL(gov_data->parent)) {
		gov_data->req_parent.value = parent_gov->max_opp;
		gov_data->req_parent.dev = ad->dev;
		list_add(&gov_data->req_parent.list, &parent_gov->head);
		apu_dump_list(parent_gov);
	}

	aprobe_info(ad->dev, "[%s] child_limit volt/opp %d/%d\n",
		    __func__, apu_data->child_volt_limit, gov_data->child_opp_limit);
	aprobe_info(ad->dev, "[%s] threshold volt/opp %d/%d\n",
		    __func__, apu_data->threshold_volt, gov_data->threshold_opp);

	return 0;
}

void apu_gov_unsetup(struct apu_dev *ad)
{
	struct apu_gov_data *gov_data, *parent_gov = NULL;

	gov_data = (struct apu_gov_data *)ad->df->data;
	get_datas(gov_data, &parent_gov, NULL, NULL);

	/* del req from self's head */
	list_del(&gov_data->req.list);

	/* del req_parent from parent's head */
	if (!IS_ERR_OR_NULL(gov_data->parent)) {
		/* del req to self's head */
		list_del(&gov_data->req_parent.list);
		apu_dump_list(parent_gov);
	}
}
