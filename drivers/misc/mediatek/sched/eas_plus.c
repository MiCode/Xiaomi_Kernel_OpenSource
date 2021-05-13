// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/module.h>
#include "../../../../kernel/sched/sched.h"
#include "sched_main.h"
#include <linux/sort.h>
#include "../../../../drivers/thermal/mediatek/thermal_interface.h"

MODULE_LICENSE("GPL");

static struct perf_domain *find_pd(struct perf_domain *pd, int cpu)
{
	while (pd) {
		if (cpumask_test_cpu(cpu, perf_domain_span(pd))){
			return pd;
		}
		pd = pd->next;
	}

	return NULL;
}

void mtk_find_busiest_group(void *data, struct sched_group *busiest,
		struct rq *dst_rq, int *out_balance)
{
	int src_cpu = -1;

	if (busiest) {
			struct perf_domain *pd = NULL;
			int dst_cpu = dst_rq->cpu;

			pd = dst_rq->rd->pd;
			pd = find_pd(pd, dst_cpu);
			src_cpu = group_first_cpu(busiest);

			if (cpumask_test_cpu(src_cpu, perf_domain_span(pd)))
				*out_balance = 0;
	}
}

#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)

struct thermal_struct{
	int cpu_id;
	int headroom;
};

static int cmp(const void *a, const void *b)
{

	const struct thermal_struct *a1=a;
	const struct thermal_struct *b1=b;

	return b1->headroom - a1->headroom;
}

int sort_thermal_headroom(struct cpumask *cpus, int *cpu_order)
{
	int i, cpu, cnt=0;
	struct thermal_struct thermal_order[NR_CPUS];

	for_each_cpu_and(cpu, cpus, cpu_online_mask) {
		thermal_order[cnt].cpu_id = cpu;
		thermal_order[cnt].headroom = get_thermal_headroom(cpu);
		cnt++;
	}

	sort(thermal_order, cnt, sizeof(struct thermal_struct), cmp, NULL);

	for(i = 0; i < cnt; i++) {
		*cpu_order++ = thermal_order[i].cpu_id;
	}

	return cnt;

}

#endif

