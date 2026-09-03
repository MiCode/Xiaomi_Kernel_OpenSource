/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __SPRD_MULTI_CONTROL_H__
#define __SPRD_MULTI_CONTROL_H__

#include <linux/module.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/llist.h>
#include <linux/list.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <trace/hooks/vendor_hooks.h>
#include <trace/hooks/cpufreq.h>

#define cpufreq_attr_rw(_name) \
static struct freq_attr _name = \
__ATTR(_name, 0644, show_##_name, store_##_name)

struct sprd_multi_control {
	struct cpufreq_policy	*policy;

	/* used to add the policy to sprd_multi_control_list */
	struct list_head		node;

	/* for multi control */
	unsigned int high_level_limit_max;
	unsigned int high_level_limit_min;
	unsigned int hl_control_enabled;
	unsigned int user_max_freq;
	struct freq_qos_request qos_req;

#ifdef CONFIG_UNISOC_FIX_FREQ
	/* fix freq for debug */
	unsigned int scaling_fixed_freq;
#endif

};

#endif
