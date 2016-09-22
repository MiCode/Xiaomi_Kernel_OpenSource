/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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

#ifndef _ARCH_ARM_MACH_MSM_BUS_RULES_H
#define _ARCH_ARM_MACH_MSM_BUS_RULES_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/notifier.h>
#include <dt-bindings/msm/msm-bus-rule-ops.h>

#define MAX_NODES		(5)

struct rule_update_path_info {
	u32 id;
	u64 ab;
	u64 ib;
	u64 clk;
	bool added;
	struct list_head link;
};

struct rule_apply_rcm_info {
	u32 id;
	u64 lim_bw;
	int throttle;
	bool after_clk_commit;
	struct list_head link;
};

struct bus_rule_type {
	int num_src;
	int *src_id;
	int *src_field;
	int *op;
	int combo_op;
	int num_thresh;
	u64 *thresh;
	int num_dst;
	int *dst_node;
	u64 dst_bw;
	int mode;
	void *client_data;
	u64 curr_bw;
};

#if (defined(CONFIG_BUS_TOPOLOGY_ADHOC))
void msm_rule_register(int num_rules, struct bus_rule_type *rule,
				struct notifier_block *nb);
void msm_rule_unregister(int num_rules, struct bus_rule_type *rule,
						struct notifier_block *nb);
bool msm_rule_update(struct bus_rule_type *old_rule,
				struct bus_rule_type *new_rule,
				struct notifier_block *nb);
void msm_rule_evaluate_rules(int node);
void print_rules_buf(char *buf, int count);
bool msm_rule_are_rules_registered(void);
int msm_rule_query_bandwidth(struct bus_rule_type *rule,
			u64 *bw, struct notifier_block *nb);
#else
static inline void msm_rule_register(int num_rules, struct bus_rule_type *rule,
				struct notifier_block *nb)
{
}
static inline void msm_rule_unregister(int num_rules,
					struct bus_rule_type *rule,
					struct notifier_block *nb)
{
}
static inline void print_rules_buf(char *buf, int count)
{
}
static inline bool msm_rule_are_rules_registered(void)
{
	return false;
}
static inline bool msm_rule_update(struct bus_rule_type *old_rule,
					struct bus_rule_type *new_rule,
					struct notifier_block *nb)
{
	return false;
}
static inline void msm_rule_evaluate_rules(int node)
{
}
static inline int msm_rule_query_bandwidth(struct bus_rule_type *rule,
			u64 *bw, struct notifier_block *nb)
{
	return false;
}
#endif /* defined(CONFIG_BUS_TOPOLOGY_ADHOC) */
#endif /* _ARCH_ARM_MACH_MSM_BUS_RULES_H */
