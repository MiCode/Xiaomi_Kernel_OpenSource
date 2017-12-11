/*
 * Obtain energy cost data from DT and populate relevant scheduler data
 * structures.
 *
 * Copyright (C) 2015 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#define pr_fmt(fmt) "sched-energy: " fmt

#include <linux/gfp.h>
#include <linux/of.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/sched_energy.h>
#include <linux/stddef.h>
#include <linux/cpu.h>
#include <linux/pm_opp.h>
#include <linux/platform_device.h>

#include "sched.h"

struct sched_group_energy *sge_array[NR_CPUS][NR_SD_LEVELS];

static void free_resources(void)
{
	int cpu, sd_level;
	struct sched_group_energy *sge;

	for_each_possible_cpu(cpu) {
		for_each_possible_sd_level(sd_level) {
			sge = sge_array[cpu][sd_level];
			if (sge) {
				kfree(sge->cap_states);
				kfree(sge->idle_states);
				kfree(sge);
			}
		}
	}
}

void init_sched_energy_costs(void)
{
	struct device_node *cn, *cp;
	struct capacity_state *cap_states;
	struct idle_state *idle_states;
	struct sched_group_energy *sge;
	const struct property *prop;
	int sd_level, i, nstates, cpu;
	const __be32 *val;

	if (!sched_is_energy_aware())
		return;

	for_each_possible_cpu(cpu) {
		cn = of_get_cpu_node(cpu, NULL);
		if (!cn) {
			pr_warn("CPU device node missing for CPU %d\n", cpu);
			return;
		}

		if (!of_find_property(cn, "sched-energy-costs", NULL)) {
			pr_warn("CPU device node has no sched-energy-costs\n");
			return;
		}

		for_each_possible_sd_level(sd_level) {
			cp = of_parse_phandle(cn, "sched-energy-costs", sd_level);
			if (!cp)
				break;

			prop = of_find_property(cp, "busy-cost-data", NULL);
			if (!prop || !prop->value) {
				pr_warn("No busy-cost data, skipping sched_energy init\n");
				goto out;
			}

			sge = kcalloc(1, sizeof(struct sched_group_energy),
				      GFP_NOWAIT);
			if (!sge)
				goto out;

			nstates = (prop->length / sizeof(u32)) / 2;
			cap_states = kcalloc(nstates,
					     sizeof(struct capacity_state),
					     GFP_NOWAIT);
			if (!cap_states) {
				kfree(sge);
				goto out;
			}

			for (i = 0, val = prop->value; i < nstates; i++) {
				cap_states[i].cap = SCHED_CAPACITY_SCALE;
				cap_states[i].frequency = be32_to_cpup(val++);
				cap_states[i].power = be32_to_cpup(val++);
			}

			sge->nr_cap_states = nstates;
			sge->cap_states = cap_states;

			prop = of_find_property(cp, "idle-cost-data", NULL);
			if (!prop || !prop->value) {
				pr_warn("No idle-cost data, skipping sched_energy init\n");
				kfree(sge);
				kfree(cap_states);
				goto out;
			}

			nstates = (prop->length / sizeof(u32));
			idle_states = kcalloc(nstates,
					      sizeof(struct idle_state),
					      GFP_NOWAIT);
			if (!idle_states) {
				kfree(sge);
				kfree(cap_states);
				goto out;
			}

			for (i = 0, val = prop->value; i < nstates; i++)
				idle_states[i].power = be32_to_cpup(val++);

			sge->nr_idle_states = nstates;
			sge->idle_states = idle_states;

			sge_array[cpu][sd_level] = sge;
		}
	}

	pr_info("Sched-energy-costs installed from DT\n");
	return;

out:
	free_resources();
}

static int sched_energy_probe(struct platform_device *pdev)
{
	unsigned long max_freq = 0;
	int max_efficiency = INT_MIN;
	int cpu;
	unsigned long *max_frequencies = NULL;
	int ret;

	if (!sched_is_energy_aware())
		return 0;

	max_frequencies = kmalloc_array(nr_cpu_ids, sizeof(unsigned long),
					GFP_KERNEL);
	if (!max_frequencies) {
		ret = -ENOMEM;
		goto exit;
	}

	/*
	 * Find system max possible frequency and max frequencies for each
	 * CPUs.
	 */
	for_each_possible_cpu(cpu) {
		struct device *cpu_dev;
		struct dev_pm_opp *opp;
		int efficiency = arch_get_cpu_efficiency(cpu);

		max_efficiency = max(efficiency, max_efficiency);

		cpu_dev = get_cpu_device(cpu);
		if (IS_ERR_OR_NULL(cpu_dev)) {
			if (!cpu_dev)
				ret = -EINVAL;
			else
				ret = PTR_ERR(cpu_dev);
			goto exit;
		}

		max_frequencies[cpu] = ULONG_MAX;

		rcu_read_lock();
		opp = dev_pm_opp_find_freq_floor(cpu_dev,
						 &max_frequencies[cpu]);
		if (IS_ERR_OR_NULL(opp)) {
			if (!opp || PTR_ERR(opp) == -ENODEV)
				ret = -EPROBE_DEFER;
			else
				ret = PTR_ERR(opp);
			goto exit_rcu_unlock;
		}
		rcu_read_unlock();

		/* Convert HZ to KHZ */
		max_frequencies[cpu] /= 1000;
		max_freq = max(max_freq, max_frequencies[cpu]);
	}

	/* update capacity in energy model */
	for_each_possible_cpu(cpu) {
		unsigned long cpu_max_cap;
		struct sched_group_energy *sge_l0, *sge;
		int efficiency = arch_get_cpu_efficiency(cpu);

		cpu_max_cap = DIV_ROUND_UP(SCHED_CAPACITY_SCALE *
					   max_frequencies[cpu], max_freq);
		cpu_max_cap = DIV_ROUND_UP(cpu_max_cap * efficiency,
					   max_efficiency);

		/*
		 * All the cap_states have same frequency table so use
		 * SD_LEVEL0's.
		 */
		sge_l0 = sge_array[cpu][SD_LEVEL0];
		if (sge_l0 && sge_l0->nr_cap_states > 0) {
			int i;
			int ncapstates = sge_l0->nr_cap_states;

			for (i = 0; i < ncapstates; i++) {
				int sd_level;
				unsigned long freq, cap;

				/*
				 * Energy model can contain more frequency
				 * steps than actual for multiple speedbin
				 * support. Ceil the max capacity with actual
				 * one.
				 */
				freq = min(sge_l0->cap_states[i].frequency,
					   max_frequencies[cpu]);
				cap = DIV_ROUND_UP(cpu_max_cap * freq,
						   max_frequencies[cpu]);

				for_each_possible_sd_level(sd_level) {
					sge = sge_array[cpu][sd_level];
					if (!sge)
						break;
					sge->cap_states[i].cap = cap;
				}

				dev_dbg(&pdev->dev,
					"cpu=%d freq=%ld cap=%ld power_d0=%ld\n",
					cpu, freq, sge_l0->cap_states[i].cap,
					sge_l0->cap_states[i].power);
			}

			dev_info(&pdev->dev,
				"cpu=%d eff=%d [freq=%ld cap=%ld power_d0=%ld] -> [freq=%ld cap=%ld power_d0=%ld]\n",
				cpu, efficiency,
				sge_l0->cap_states[0].frequency,
				sge_l0->cap_states[0].cap,
				sge_l0->cap_states[0].power,
				sge_l0->cap_states[ncapstates - 1].frequency,
				sge_l0->cap_states[ncapstates - 1].cap,
				sge_l0->cap_states[ncapstates - 1].power
				);
		}


		dev_dbg(&pdev->dev,
			"cpu=%d efficiency=%d max_frequency=%ld max_efficiency=%d cpu_max_capacity=%ld\n",
			cpu, efficiency, max_frequencies[cpu], max_efficiency,
			cpu_max_cap);

		arch_update_cpu_capacity(cpu);
	}

	kfree(max_frequencies);

	walt_sched_energy_populated_callback();
	dev_info(&pdev->dev, "Sched-energy-costs capacity updated\n");
	return 0;

exit_rcu_unlock:
	rcu_read_unlock();

exit:
	if (ret != -EPROBE_DEFER)
		dev_err(&pdev->dev, "error=%d\n", ret);

	kfree(max_frequencies);
	return ret;
}

static const struct of_device_id of_sched_energy_dt[] = {
	{
		.compatible = "sched-energy",
	},
	{ }
};

static struct platform_driver energy_driver = {
	.driver = {
		.name = "sched-energy",
		.of_match_table = of_sched_energy_dt,
	},
	.probe = sched_energy_probe,
};

static int __init sched_energy_init(void)
{
	return platform_driver_register(&energy_driver);
}
subsys_initcall(sched_energy_init);
