// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/module.h>
#include "../../../../kernel/sched/sched.h"
#include "sched_main.h"

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

