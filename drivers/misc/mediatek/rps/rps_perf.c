// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   rps_perf.c
 *
 * Project:
 * --------
 *
 *
 * Description:
 * ------------
 *   network performance interface
 *
 * Author:
 * -------
 *   Anny.Hu(mtk80401)
 *
 ****************************************************************************/
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/ipv6.h>
#include <net/ipv6.h>
#include <net/sch_generic.h>
#include <linux/skbuff.h>
#include <linux/module.h>
#include <linux/debugfs.h>

#include "rps_perf.h"

int set_rps_map(struct netdev_rx_queue *queue, unsigned long rps_value)
{
#ifdef CONFIG_RPS
	struct rps_map *old_map, *map;
	cpumask_var_t mask;
	int cpu, i, len = 3;
	static DEFINE_MUTEX(rps_map_mutex);

	if (!alloc_cpumask_var(&mask, GFP_KERNEL))
		return -ENOMEM;

	*cpumask_bits(mask) = rps_value;
	map = kzalloc(max_t(unsigned int,
			RPS_MAP_SIZE(cpumask_weight(mask)), L1_CACHE_BYTES),
			GFP_KERNEL);
	if (!map) {
		free_cpumask_var(mask);
		return -ENOMEM;
	}

	i = 0;
	for_each_cpu_and(cpu, mask, cpu_online_mask)
		map->cpus[i++] = cpu;

	if (i) {
		map->len = i;
	} else {
		kfree(map);
		map = NULL;
	}

	mutex_lock(&rps_map_mutex);
	old_map = rcu_dereference_protected(queue->rps_map,
				mutex_is_locked(&rps_map_mutex));
	rcu_assign_pointer(queue->rps_map, map);
	if (map)
		static_key_slow_inc(&rps_needed);
	if (old_map)
		static_key_slow_dec(&rps_needed);
	mutex_unlock(&rps_map_mutex);

	if (old_map)
		kfree_rcu(old_map, rcu);
	free_cpumask_var(mask);

	return map ? len : 0;
#else
	return 0;
#endif
}
EXPORT_SYMBOL(set_rps_map);
