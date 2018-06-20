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

#include <linux/list_sort.h>
#include <linux/msm-bus-board.h>
#include <linux/msm_bus_rules.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/msm-bus.h>
#include <trace/events/trace_msm_bus.h>

struct node_vote_info {
	int id;
	u64 ib;
	u64 ab;
	u64 clk;
};

struct rules_def {
	int rule_id;
	int num_src;
	int state;
	struct node_vote_info *src_info;
	struct bus_rule_type rule_ops;
	bool state_change;
	struct list_head link;
};

struct rule_node_info {
	int id;
	void *data;
	struct raw_notifier_head rule_notify_list;
	struct rules_def *cur_rule;
	int num_rules;
	struct list_head node_rules;
	struct list_head link;
	struct rule_apply_rcm_info apply;
};

DEFINE_MUTEX(msm_bus_rules_lock);
static LIST_HEAD(node_list);
static struct rule_node_info *get_node(u32 id, void *data);
static int node_rules_compare(void *priv, struct list_head *a,
					struct list_head *b);

#define LE(op1, op2)	(op1 <= op2)
#define LT(op1, op2)	(op1 < op2)
#define GE(op1, op2)	(op1 >= op2)
#define GT(op1, op2)	(op1 > op2)
#define NB_ID		(0x201)

static struct rule_node_info *get_node(u32 id, void *data)
{
	struct rule_node_info *node_it = NULL;
	struct rule_node_info *node_match = NULL;

	list_for_each_entry(node_it, &node_list, link) {
		if (node_it->id == id) {
			if (id == NB_ID) {
				if (node_it->data == data) {
					node_match = node_it;
					break;
				}
			} else {
				node_match = node_it;
				break;
			}
		}
	}
	return node_match;
}

static struct rule_node_info *gen_node(u32 id, void *data)
{
	struct rule_node_info *node_it = NULL;
	struct rule_node_info *node_match = NULL;

	list_for_each_entry(node_it, &node_list, link) {
		if (node_it->id == id) {
			node_match = node_it;
			break;
		}
	}

	if (!node_match) {
		node_match = kzalloc(sizeof(struct rule_node_info), GFP_KERNEL);
		if (!node_match)
			goto exit_node_match;

		node_match->id = id;
		node_match->cur_rule = NULL;
		node_match->num_rules = 0;
		node_match->data = data;
		list_add_tail(&node_match->link, &node_list);
		INIT_LIST_HEAD(&node_match->node_rules);
		RAW_INIT_NOTIFIER_HEAD(&node_match->rule_notify_list);
		pr_debug("Added new node %d to list\n", id);
	}
exit_node_match:
	return node_match;
}

static bool do_compare_op(u64 op1, u64 op2, int op)
{
	bool ret = false;

	switch (op) {
	case OP_LE:
		ret = LE(op1, op2);
		break;
	case OP_LT:
		ret = LT(op1, op2);
		break;
	case OP_GT:
		ret = GT(op1, op2);
		break;
	case OP_GE:
		ret = GE(op1, op2);
		break;
	case OP_NOOP:
		ret = true;
		break;
	default:
		pr_info("Invalid OP %d", op);
		break;
	}
	return ret;
}

static void update_src_id_vote(struct rule_update_path_info *inp_node,
				struct rule_node_info *rule_node)
{
	struct rules_def *rule;
	int i;

	list_for_each_entry(rule, &rule_node->node_rules, link) {
		for (i = 0; i < rule->num_src; i++) {
			if (rule->src_info[i].id == inp_node->id) {
				rule->src_info[i].ib = inp_node->ib;
				rule->src_info[i].ab = inp_node->ab;
				rule->src_info[i].clk = inp_node->clk;
			}
		}
	}
}

static u64 get_field(struct rules_def *rule, int src_id)
{
	u64 field = 0;
	int i;

	for (i = 0; i < rule->num_src; i++) {
		switch (rule->rule_ops.src_field) {
		case FLD_IB:
			field += rule->src_info[i].ib;
			break;
		case FLD_AB:
			field += rule->src_info[i].ab;
			break;
		case FLD_CLK:
			field += rule->src_info[i].clk;
			break;
		}
	}

	return field;
}

static bool check_rule(struct rules_def *rule,
			struct rule_update_path_info *inp)
{
	bool ret = false;

	if (!rule)
		return ret;

	switch (rule->rule_ops.op) {
	case OP_LE:
	case OP_LT:
	case OP_GT:
	case OP_GE:
	{
		u64 src_field = get_field(rule, inp->id);

		ret = do_compare_op(src_field, rule->rule_ops.thresh,
							rule->rule_ops.op);
		break;
	}
	default:
		pr_err("Unsupported op %d", rule->rule_ops.op);
		break;
	}
	return ret;
}

static void match_rule(struct rule_update_path_info *inp_node,
			struct rule_node_info *node)
{
	struct rules_def *rule;
	int i;

	list_for_each_entry(rule, &node->node_rules, link) {
		for (i = 0; i < rule->num_src; i++) {
			if (rule->src_info[i].id != inp_node->id)
				continue;

			if (check_rule(rule, inp_node)) {
				trace_bus_rules_matches(
				(node->cur_rule ?
					node->cur_rule->rule_id : -1),
				inp_node->id, inp_node->ab,
				inp_node->ib, inp_node->clk);
				if (rule->state ==
					RULE_STATE_NOT_APPLIED)
					rule->state_change = true;
				rule->state = RULE_STATE_APPLIED;
			} else {
				if (rule->state ==
					RULE_STATE_APPLIED)
					rule->state_change = true;
				rule->state = RULE_STATE_NOT_APPLIED;
			}
		}
	}
}

static void apply_rule(struct rule_node_info *node,
			struct list_head *output_list)
{
	struct rules_def *rule;
	struct rules_def *last_rule;

	last_rule = node->cur_rule;
	node->cur_rule = NULL;
	list_for_each_entry(rule, &node->node_rules, link) {
		if ((rule->state == RULE_STATE_APPLIED) &&
						!node->cur_rule)
			node->cur_rule = rule;

		if (node->id == NB_ID) {
			if (rule->state_change) {
				rule->state_change = false;
				raw_notifier_call_chain(&node->rule_notify_list,
					rule->state, (void *)&rule->rule_ops);
			}
		} else {
			if ((rule->state == RULE_STATE_APPLIED) &&
			     (node->cur_rule &&
				(node->cur_rule->rule_id == rule->rule_id))) {
				node->apply.id = rule->rule_ops.dst_node[0];
				node->apply.throttle = rule->rule_ops.mode;
				node->apply.lim_bw = rule->rule_ops.dst_bw;
				node->apply.after_clk_commit = false;
				if (last_rule != node->cur_rule)
					list_add_tail(&node->apply.link,
								output_list);
				if (last_rule) {
					if (node_rules_compare(NULL,
						&last_rule->link,
						&node->cur_rule->link) == -1)
						node->apply.after_clk_commit =
									true;
				}
			}
			rule->state_change = false;
		}
	}

}

int msm_rules_update_path(struct list_head *input_list,
			struct list_head *output_list)
{
	int ret = 0;
	struct rule_update_path_info  *inp_node;
	struct rule_node_info *node_it = NULL;

	mutex_lock(&msm_bus_rules_lock);
	list_for_each_entry(inp_node, input_list, link) {
		list_for_each_entry(node_it, &node_list, link) {
			update_src_id_vote(inp_node, node_it);
			match_rule(inp_node, node_it);
		}
	}

	list_for_each_entry(node_it, &node_list, link)
		apply_rule(node_it, output_list);
	mutex_unlock(&msm_bus_rules_lock);
	return ret;
}

static bool ops_equal(int op1, int op2)
{
	bool ret = false;

	switch (op1) {
	case OP_GT:
	case OP_GE:
	case OP_LT:
	case OP_LE:
		if (abs(op1 - op2) <= 1)
			ret = true;
		break;
	default:
		ret = (op1 == op2);
	}

	return ret;
}

static bool is_throttle_rule(int mode)
{
	bool ret = true;

	if (mode == THROTTLE_OFF)
		ret = false;

	return ret;
}

static int node_rules_compare(void *priv, struct list_head *a,
					struct list_head *b)
{
	struct rules_def *ra = container_of(a, struct rules_def, link);
	struct rules_def *rb = container_of(b, struct rules_def, link);
	int ret = -1;
	int64_t th_diff = 0;


	if (ra->rule_ops.mode == rb->rule_ops.mode) {
		if (ops_equal(ra->rule_ops.op, rb->rule_ops.op)) {
			if ((ra->rule_ops.op == OP_LT) ||
				(ra->rule_ops.op == OP_LE)) {
				th_diff = ra->rule_ops.thresh -
						rb->rule_ops.thresh;
				if (th_diff > 0)
					ret = 1;
				else
					ret = -1;
			} else if ((ra->rule_ops.op == OP_GT) ||
					(ra->rule_ops.op == OP_GE)) {
				th_diff = rb->rule_ops.thresh -
							ra->rule_ops.thresh;
				if (th_diff > 0)
					ret = 1;
				else
					ret = -1;
			}
		} else {
			ret = ra->rule_ops.op - rb->rule_ops.op;
		}
	} else if (is_throttle_rule(ra->rule_ops.mode) &&
				is_throttle_rule(rb->rule_ops.mode)) {
		if (ra->rule_ops.mode == THROTTLE_ON)
			ret = -1;
		else
			ret = 1;
	} else if ((ra->rule_ops.mode == THROTTLE_OFF) &&
		is_throttle_rule(rb->rule_ops.mode)) {
		ret = 1;
	} else if (is_throttle_rule(ra->rule_ops.mode) &&
		(rb->rule_ops.mode == THROTTLE_OFF)) {
		ret = -1;
	}

	return ret;
}

static void print_rules(struct rule_node_info *node_it)
{
	struct rules_def *node_rule = NULL;
	int i;

	if (!node_it) {
		pr_err("%s: no node for found", __func__);
		return;
	}

	pr_info("\n Now printing rules for Node %d  cur rule %d\n",
			node_it->id,
			(node_it->cur_rule ? node_it->cur_rule->rule_id : -1));
	list_for_each_entry(node_rule, &node_it->node_rules, link) {
		pr_info("\n num Rules %d  rule Id %d\n",
				node_it->num_rules, node_rule->rule_id);
		pr_info("Rule: src_field %d\n", node_rule->rule_ops.src_field);
		for (i = 0; i < node_rule->rule_ops.num_src; i++)
			pr_info("Rule: src %d\n",
					node_rule->rule_ops.src_id[i]);
		for (i = 0; i < node_rule->rule_ops.num_dst; i++)
			pr_info("Rule: dst %d dst_bw %llu\n",
						node_rule->rule_ops.dst_node[i],
						node_rule->rule_ops.dst_bw);
		pr_info("Rule: thresh %llu op %d mode %d State %d\n",
					node_rule->rule_ops.thresh,
					node_rule->rule_ops.op,
					node_rule->rule_ops.mode,
					node_rule->state);
	}
}

void print_all_rules(void)
{
	struct rule_node_info *node_it = NULL;

	mutex_lock(&msm_bus_rules_lock);
	list_for_each_entry(node_it, &node_list, link)
		print_rules(node_it);
	mutex_unlock(&msm_bus_rules_lock);
}

void print_rules_buf(char *buf, int max_buf)
{
	struct rule_node_info *node_it = NULL;
	struct rules_def *node_rule = NULL;
	int i;
	int cnt = 0;

	mutex_lock(&msm_bus_rules_lock);
	list_for_each_entry(node_it, &node_list, link) {
		cnt += scnprintf(buf + cnt, max_buf - cnt,
			"\n Now printing rules for Node %d cur_rule %d\n",
			node_it->id,
			(node_it->cur_rule ? node_it->cur_rule->rule_id : -1));
		list_for_each_entry(node_rule, &node_it->node_rules, link) {
			cnt += scnprintf(buf + cnt, max_buf - cnt,
				"\nNum Rules:%d ruleId %d STATE:%d change:%d\n",
				node_it->num_rules, node_rule->rule_id,
				node_rule->state, node_rule->state_change);
			cnt += scnprintf(buf + cnt, max_buf - cnt,
				"Src_field %d\n",
				node_rule->rule_ops.src_field);
			for (i = 0; i < node_rule->rule_ops.num_src; i++)
				cnt += scnprintf(buf + cnt, max_buf - cnt,
					"Src %d Cur Ib %llu Ab %llu\n",
					node_rule->rule_ops.src_id[i],
					node_rule->src_info[i].ib,
					node_rule->src_info[i].ab);
			for (i = 0; i < node_rule->rule_ops.num_dst; i++)
				cnt += scnprintf(buf + cnt, max_buf - cnt,
					"Dst %d dst_bw %llu\n",
					node_rule->rule_ops.dst_node[0],
					node_rule->rule_ops.dst_bw);
			cnt += scnprintf(buf + cnt, max_buf - cnt,
					"Thresh %llu op %d mode %d\n",
					node_rule->rule_ops.thresh,
					node_rule->rule_ops.op,
					node_rule->rule_ops.mode);
		}
	}
	mutex_unlock(&msm_bus_rules_lock);
}

static int copy_rule(struct bus_rule_type *src, struct rules_def *node_rule,
			struct notifier_block *nb)
{
	int i;
	int ret = 0;

	memcpy(&node_rule->rule_ops, src,
				sizeof(struct bus_rule_type));
	node_rule->rule_ops.src_id = kzalloc(
			(sizeof(int) * node_rule->rule_ops.num_src),
							GFP_KERNEL);
	if (!node_rule->rule_ops.src_id) {
		pr_err("%s:Failed to allocate for src_id",
					__func__);
		return -ENOMEM;
	}
	memcpy(node_rule->rule_ops.src_id, src->src_id,
				sizeof(int) * src->num_src);


	if (!nb) {
		node_rule->rule_ops.dst_node = kzalloc(
			(sizeof(int) * node_rule->rule_ops.num_dst),
						GFP_KERNEL);
		if (!node_rule->rule_ops.dst_node)
			return -ENOMEM;
		memcpy(node_rule->rule_ops.dst_node, src->dst_node,
						sizeof(int) * src->num_dst);
	}

	node_rule->num_src = src->num_src;
	node_rule->src_info = kzalloc(
		(sizeof(struct node_vote_info) * node_rule->rule_ops.num_src),
							GFP_KERNEL);
	if (!node_rule->src_info) {
		pr_err("%s:Failed to allocate for src_id",
						__func__);
		return -ENOMEM;
	}
	for (i = 0; i < src->num_src; i++)
		node_rule->src_info[i].id = src->src_id[i];

	return ret;
}

static bool __rule_register(int num_rules, struct bus_rule_type *rule,
					struct notifier_block *nb)
{
	struct rule_node_info *node = NULL;
	int i, j;
	struct rules_def *node_rule = NULL;
	int num_dst = 0;
	bool reg_success = true;

	if (num_rules <= 0)
		return false;

	for (i = 0; i < num_rules; i++) {
		if (nb)
			num_dst = 1;
		else
			num_dst = rule[i].num_dst;

		for (j = 0; j < num_dst; j++) {
			int id = 0;

			if (nb)
				id = NB_ID;
			else
				id = rule[i].dst_node[j];

			node = gen_node(id, nb);
			if (!node) {
				pr_info("Error getting rule");
				reg_success = false;
				goto exit_rule_register;
			}
			node_rule = kzalloc(sizeof(struct rules_def),
						GFP_KERNEL);
			if (!node_rule) {
				reg_success = false;
				goto exit_rule_register;
			}

			if (copy_rule(&rule[i], node_rule, nb)) {
				pr_err("Error copying rule");
				reg_success = false;
				goto exit_rule_register;
			}

			node_rule->rule_id = node->num_rules++;
			if (nb)
				node->data = nb;

			list_add_tail(&node_rule->link, &node->node_rules);
		}
	}
	list_sort(NULL, &node->node_rules, node_rules_compare);
	if (nb && nb != node->rule_notify_list.head)
		raw_notifier_chain_register(&node->rule_notify_list, nb);
exit_rule_register:
	return reg_success;
}

static int comp_rules(struct bus_rule_type *rulea, struct bus_rule_type *ruleb)
{
	int ret = 1;

	if (rulea->num_src == ruleb->num_src)
		ret = memcmp(rulea->src_id, ruleb->src_id,
				(sizeof(int) * rulea->num_src));
	if (!ret && (rulea->num_dst == ruleb->num_dst))
		ret = memcmp(rulea->dst_node, ruleb->dst_node,
				(sizeof(int) * rulea->num_dst));
	if (ret || (rulea->dst_bw != ruleb->dst_bw) ||
		(rulea->op != ruleb->op) || (rulea->thresh != ruleb->thresh))
		ret = 1;
	return ret;
}

void msm_rule_register(int num_rules, struct bus_rule_type *rule,
					struct notifier_block *nb)
{
	if (!rule || num_rules <= 0)
		return;

	mutex_lock(&msm_bus_rules_lock);
	__rule_register(num_rules, rule, nb);
	mutex_unlock(&msm_bus_rules_lock);
}

static bool __rule_unregister(int num_rules, struct bus_rule_type *rule,
					struct notifier_block *nb)
{
	int i = 0;
	struct rule_node_info *node = NULL;
	struct rule_node_info *node_tmp = NULL;
	struct rules_def *node_rule;
	struct rules_def *node_rule_tmp;
	bool match_found = false;

	if (num_rules <= 0)
		return false;

	if (nb) {
		node = get_node(NB_ID, nb);
		if (!node) {
			pr_err("%s: Can't find node", __func__);
			goto exit_unregister_rule;
		}
		match_found = true;
		list_for_each_entry_safe(node_rule, node_rule_tmp,
					&node->node_rules, link) {
			if (comp_rules(&node_rule->rule_ops,
					&rule[i]) == 0) {
				list_del(&node_rule->link);
				kfree(node_rule);
				match_found = true;
				node->num_rules--;
				list_sort(NULL,
					&node->node_rules,
					node_rules_compare);
				break;
			}
		}
		if (!node->num_rules)
			raw_notifier_chain_unregister(
					&node->rule_notify_list, nb);
	} else {
		for (i = 0; i < num_rules; i++) {
			match_found = false;

			list_for_each_entry(node, &node_list, link) {
				list_for_each_entry_safe(node_rule,
				node_rule_tmp, &node->node_rules, link) {
					if (comp_rules(&node_rule->rule_ops,
						&rule[i]) != 0)
						continue;
					list_del(&node_rule->link);
					kfree(node_rule);
					match_found = true;
					node->num_rules--;
					list_sort(NULL,
						&node->node_rules,
						node_rules_compare);
					break;
				}
			}
		}
	}

	list_for_each_entry_safe(node, node_tmp,
					&node_list, link) {
		if (!node->num_rules) {
			pr_debug("Deleting Rule node %d", node->id);
			list_del(&node->link);
			kfree(node);
		}
	}
exit_unregister_rule:
	return match_found;
}

void msm_rule_unregister(int num_rules, struct bus_rule_type *rule,
					struct notifier_block *nb)
{
	if (!rule || num_rules <= 0)
		return;

	mutex_lock(&msm_bus_rules_lock);
	__rule_unregister(num_rules, rule, nb);
	mutex_unlock(&msm_bus_rules_lock);
}

bool msm_rule_update(struct bus_rule_type *old_rule,
			struct bus_rule_type *new_rule,
			struct notifier_block *nb)
{
	bool rc = true;

	if (!old_rule || !new_rule)
		return false;
	mutex_lock(&msm_bus_rules_lock);
	if (!__rule_unregister(1, old_rule, nb)) {
		rc = false;
		goto exit_rule_update;
	}

	if (!__rule_register(1, new_rule, nb)) {
		/*
		 * Registering new rule has failed for some reason, attempt
		 * to re-register the old rule and return error.
		 */
		__rule_register(1, old_rule, nb);
		rc = false;
	}
exit_rule_update:
	mutex_unlock(&msm_bus_rules_lock);
	return rc;
}

void msm_rule_evaluate_rules(int node)
{
	struct msm_bus_client_handle *handle;

	handle = msm_bus_scale_register(node, node, "tmp-rm", false);
	if (!handle)
		return;
	msm_bus_scale_update_bw(handle, 0, 0);
	msm_bus_scale_unregister(handle);
}

bool msm_rule_are_rules_registered(void)
{
	bool ret = false;

	mutex_lock(&msm_bus_rules_lock);
	if (list_empty(&node_list))
		ret = false;
	else
		ret = true;
	mutex_unlock(&msm_bus_rules_lock);
	return ret;
}

