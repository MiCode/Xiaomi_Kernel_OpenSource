/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt) "[topo_ctrl]"fmt

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/topology.h>

#include "tchbst.h"
#include "topo_ctrl.h"
#include "mtk_perfmgr_internal.h"

static int *calc_cpu_cap, *calc_cpu_num;
static int  s_clstr_core, b_clstr_core;

int topo_ctrl_get_nr_clusters(void)
{
	return perfmgr_clusters;

}
EXPORT_SYMBOL(topo_ctrl_get_nr_clusters);

/***************************************/
static void calc_min_cpucap(void)
{

	int i, cpu_num = 0, min = INT_MAX, max = 0;
	struct cpumask cpus;
	struct cpumask cpu_online_cpumask;

	for (i = 0; i < perfmgr_clusters ; i++) {
		arch_get_cluster_cpus(&cpus, i);
		cpumask_and(&cpu_online_cpumask,
				&cpus, cpu_possible_mask);

		calc_cpu_num[i] = cpumask_weight(&cpu_online_cpumask);
		calc_cpu_cap[i] = arch_get_max_cpu_capacity(cpu_num);

		if (calc_cpu_cap[i] < min) {
			s_clstr_core = i;
			min = calc_cpu_cap[i];
		}
		if (calc_cpu_cap[i] > max) {
			b_clstr_core = i;
			max = calc_cpu_cap[i];
		}
		cpu_num += calc_cpu_num[i];
	}
}

int get_min_clstr_cap(void)
{
	/*get smallest cluster num*/
	return s_clstr_core;
}

int get_max_clstr_cap(void)
{
	/*get biggest cluster num*/
	return b_clstr_core;
}

static int perfmgr_glb_info_proc_show(struct seq_file *m, void *v)
{
	int i;

	seq_printf(m, "perfmgr_clusters : %d\n", perfmgr_clusters);

	for (i = 0 ; i < perfmgr_clusters ; i++) {
		seq_printf(m, "calc_cpu_cap[%d]:%d\n", i, calc_cpu_cap[i]);
		seq_printf(m, "calc_cpu_num[%d]:%d\n", i, calc_cpu_num[i]);
	}
	seq_printf(m, "s_clstr_core : %d\n", s_clstr_core);
	seq_printf(m, "b_clstr_core : %d\n", b_clstr_core);
	return 0;
}
static int perfmgr_topo_ctrl_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", perfmgr_clusters);
	return 0;
}
PROC_FOPS_RO(topo_ctrl);
PROC_FOPS_RO(glb_info);
/************************************************/
int topo_ctrl_init(struct proc_dir_entry *parent)
{
	struct proc_dir_entry *topo_dir = NULL;
	int i, ret = 0;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(topo_ctrl),
		PROC_ENTRY(glb_info),
	};

	topo_dir = proc_mkdir("topo_ctrl", parent);

	if (!topo_dir)
		pr_debug("topo_dir null\n ");



	/* create procfs */
	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0644,
					topo_dir, entries[i].fops)) {
			pr_debug("%s(), create /topo_ctrl%s failed\n",
					__func__, entries[i].name);
			ret = -EINVAL;
			goto out;
		}
	}


	calc_cpu_cap =  kcalloc(perfmgr_clusters, sizeof(int), GFP_KERNEL);
	calc_cpu_num =  kcalloc(perfmgr_clusters, sizeof(int), GFP_KERNEL);
	calc_min_cpucap();

out:
	return ret;
}

void topo_ctrl_exit(void)
{
	kfree(calc_cpu_cap);
	kfree(calc_cpu_num);
}
