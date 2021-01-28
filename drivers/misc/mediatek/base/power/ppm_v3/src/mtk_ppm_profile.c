// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>
#include <linux/string.h>

#include "mtk_ppm_internal.h"


struct ppm_profile {
	bool is_profiling;
	struct mutex lock;
	/* client execution time profiling */
	unsigned long long max_client_exec_time_us[NR_PPM_CLIENTS];
	unsigned long long avg_client_exec_time_us[NR_PPM_CLIENTS];
#ifdef PPM_SSPM_SUPPORT
	/* ipi execution time profiling */
	unsigned long long max_ipi_exec_time_us[NR_PPM_IPI];
	unsigned long long avg_ipi_exec_time_us[NR_PPM_IPI];
#endif
} ppm_profile_data;


static void ppm_profile_dump_client_exec_time(struct seq_file *m)
{
	int i;

	seq_puts(m, "\n==========================================\n");
	seq_puts(m, "PPM Client Execution Time (us)");
	seq_puts(m, "\n==========================================\n");

	seq_printf(m, "%-10s%-10s%-10s\n", "Client", "Avg Time", "Max Time");
	for_each_ppm_clients(i) {
		seq_printf(m, "%-10s%-10lld%-10lld\n",
			ppm_main_info.client_info[i].name,
			ppm_profile_data.avg_client_exec_time_us[i],
			ppm_profile_data.max_client_exec_time_us[i]);
	}
}

#ifdef PPM_SSPM_SUPPORT
static void ppm_profile_dump_ipi_exec_time(struct seq_file *m)
{
	int i;

	seq_puts(m, "\n==========================================\n");
	seq_puts(m, "PPM IPI Execution Time (us)");
	seq_puts(m, "\n==========================================\n");

	seq_printf(m, "%-10s%-10s%-10s\n", "IPI type",
		"Avg Time", "Max Time");
	for (i = 0; i < NR_PPM_IPI; i++) {
		seq_printf(m, "%-10d%-10lld%-10lld\n", i,
			ppm_profile_data.avg_ipi_exec_time_us[i],
			ppm_profile_data.max_ipi_exec_time_us[i]);
	}
}
#endif

void ppm_profile_update_client_exec_time(enum ppm_client client,
	unsigned long long time)
{
	ppm_lock(&ppm_profile_data.lock);

	if (!ppm_profile_data.is_profiling)
		goto end;

	if (client >= NR_PPM_CLIENTS)
		goto end;

	if (time > ppm_profile_data.max_client_exec_time_us[client])
		ppm_profile_data.max_client_exec_time_us[client] = time;
	ppm_profile_data.avg_client_exec_time_us[client] =
		(ppm_profile_data.avg_client_exec_time_us[client] + time) / 2;

	ppm_dbg(TIME_PROFILE, "@%s: client = %s, time = %lld\n",
		__func__, ppm_main_info.client_info[client].name, time);

end:
	ppm_unlock(&ppm_profile_data.lock);
}

#ifdef PPM_SSPM_SUPPORT
void ppm_profile_update_ipi_exec_time(int id,
	unsigned long long time)
{
	ppm_lock(&ppm_profile_data.lock);

	if (!ppm_profile_data.is_profiling)
		goto end;

	if (id >= NR_PPM_IPI)
		goto end;

	if (time > ppm_profile_data.max_ipi_exec_time_us[id])
		ppm_profile_data.max_ipi_exec_time_us[id] = time;
	ppm_profile_data.avg_ipi_exec_time_us[id] =
		(ppm_profile_data.avg_ipi_exec_time_us[id] + time) / 2;

	ppm_dbg(TIME_PROFILE, "@%s: IPI id = %d, time = %lld\n",
		__func__, id, time);

end:
	ppm_unlock(&ppm_profile_data.lock);
}
#endif

static int ppm_dump_info_proc_show(struct seq_file *m, void *v)
{
	ppm_lock(&ppm_profile_data.lock);
	ppm_profile_dump_client_exec_time(m);
#ifdef PPM_SSPM_SUPPORT
	ppm_profile_dump_ipi_exec_time(m);
#endif
	ppm_unlock(&ppm_profile_data.lock);

	return 0;
}

static int ppm_client_exec_time_proc_show(struct seq_file *m, void *v)
{
	ppm_lock(&ppm_profile_data.lock);
	ppm_profile_dump_client_exec_time(m);
	ppm_unlock(&ppm_profile_data.lock);

	return 0;
}

#ifdef PPM_SSPM_SUPPORT
static int ppm_ipi_exec_time_proc_show(struct seq_file *m, void *v)
{
	ppm_lock(&ppm_profile_data.lock);
	ppm_profile_dump_ipi_exec_time(m);
	ppm_unlock(&ppm_profile_data.lock);

	return 0;
}
#endif

static int ppm_profile_on_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", ppm_profile_data.is_profiling);

	return 0;
}

static ssize_t ppm_profile_on_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int enable;
	struct ppm_profile *p = &ppm_profile_data;

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtoint(buf, 10, &enable)) {
		ppm_lock(&(p->lock));

		if ((p->is_profiling && enable)
			|| (!p->is_profiling && !enable))
			goto end;

		/* on: record current time and clear all data */
		/* off: update time_in_state */
		if (!enable) {
			ppm_info("profiling stop!\n");
			p->is_profiling = false;
		} else {
			int i;

			ppm_info("profiling start!\n");

			/* clear data */
			for_each_ppm_clients(i) {
				p->max_client_exec_time_us[i] = 0;
				p->avg_client_exec_time_us[i] = 0;
#ifdef PPM_SSPM_SUPPORT
				p->max_ipi_exec_time_us[i] = 0;
				p->avg_ipi_exec_time_us[i] = 0;
#endif
			}

			/* start profiling */
			p->is_profiling = true;
		}

end:
		ppm_unlock(&(p->lock));
	} else
		ppm_err("echo [0/1] > /proc/ppm/profile/profile_on\n");

	free_page((unsigned long)buf);
	return count;
}

PROC_FOPS_RO(dump_info);
PROC_FOPS_RO(client_exec_time);
#ifdef PPM_SSPM_SUPPORT
PROC_FOPS_RO(ipi_exec_time);
#endif
PROC_FOPS_RW(profile_on);

int ppm_profile_init(void)
{
	int i, ret = 0;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(dump_info),
		PROC_ENTRY(client_exec_time),
#ifdef PPM_SSPM_SUPPORT
		PROC_ENTRY(ipi_exec_time),
#endif
		PROC_ENTRY(profile_on),
	};

	/* create procfs */
	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0644,
			profile_dir, entries[i].fops)) {
			ppm_err("%s(), create /proc/ppm/profile/%s failed\n",
				__func__, entries[i].name);
			ret = -EINVAL;
			goto out;
		}
	}

	mutex_init(&ppm_profile_data.lock);

	ppm_info("%s done\n", __func__);

out:
	return ret;
}

void ppm_profile_exit(void)
{
}

