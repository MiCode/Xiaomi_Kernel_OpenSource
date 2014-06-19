/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#include <linux/coresight-cti.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/smp.h>
#include <soc/qcom/cti-pmu-irq.h>

static struct coresight_cti *msm_cti_cpux[NR_CPUS];
static const char * const coresight_cpu_name[] = {
	"coresight-cti-cpu0",
	"coresight-cti-cpu1",
	"coresight-cti-cpu2",
	"coresight-cti-cpu3",
	"coresight-cti-cpu4",
	"coresight-cti-cpu5",
	"coresight-cti-cpu6",
	"coresight-cti-cpu7",
};

struct coresight_cti *msm_get_cpu_cti(int cpu)
{
	return coresight_cti_get(coresight_cpu_name[cpu]);
}

void msm_cti_pmu_irq_ack(int cpu)
{
	int ret = coresight_cti_ack_trig(msm_cti_cpux[cpu], 2);
	if (ret)
		pr_err("Failed to Acknowledge CTI-PMU Irq on CPU %d - %d\n",
								cpu, ret);
}

void msm_enable_cti_pmu_workaround(struct work_struct *work)
{
	struct coresight_cti *cti_cpux;
	int trigin = 1;
	int trigout = 2;
	int ch = 2;
	int cpu = smp_processor_id();
	int ret;

	cti_cpux = coresight_cti_get(coresight_cpu_name[cpu]);
	if (IS_ERR(cti_cpux))
		goto err;

	msm_cti_cpux[cpu] = cti_cpux;

	ret = coresight_cti_map_trigin(cti_cpux, trigin, ch);
	if (ret)
		goto err_in;
	ret = coresight_cti_map_trigout(cti_cpux, trigout, ch);
	if (ret)
		goto err_out;
	coresight_cti_enable_gate(cti_cpux, ch);
	pr_info("%s for CPU %d\n", __func__, cpu);

	return;
err_out:
	coresight_cti_unmap_trigin(cti_cpux, trigin, ch);
err_in:
	coresight_cti_put(cti_cpux);
err:
	pr_err("Failed to enable CTI-PMU workaround on CPU %d - %d\n",
								cpu, ret);
}
