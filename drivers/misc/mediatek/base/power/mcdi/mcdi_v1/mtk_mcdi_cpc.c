/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

#include <mtk_mcdi_governor.h>
#include <mtk_mcdi_util.h>
#include <mtk_mcdi_cpc.h>

#include <mtk_mcdi_reg.h>

#ifdef MCDI_CPC_MODE

static struct mcdi_cpc_dev cpc;

static char *cpc_prof_dist_str[NF_MCDI_PROF_DIST] = {
	"     < 10us",
	"10us ~ 20us",
	"20us ~ 50us",
	"     > 50us",
};

static DEFINE_SPINLOCK(mcdi_cpc_prof_spin_lock);

static void mcdi_cpc_clr_lat(void)
{
	unsigned int size;
	unsigned int ofs;
	int i;

	ofs = sizeof(cpc.p[0].name);
	size = sizeof(struct mcdi_cpc_prof) - ofs;

	for (i = 0; i < PROF_TYPE_NUM; i++)
		memset((char *)&cpc.p[i] + ofs, 0, size);

	cpc.mp_off_cnt[0] = mcdi_read(CPC_DORMANT_COUNTER);
}

static void mcdi_cpc_cal_lat(void)
{
}

static void __mcdi_cpc_prof_enable(void)
{
	mcdi_mbox_write(MCDI_MBOX_PROF_CMD, 0x1);

	mcdi_cpc_clr_lat();

	cpc.sta.prof_en = true;
}

static void __mcdi_cpc_prof_disable(void)
{
	cpc.sta.prof_en = false;

	mcdi_cpc_cal_lat();

	mcdi_mbox_write(MCDI_MBOX_PROF_CMD, 0x0);
}

void mcdi_cpc_prof_en(bool enable)
{
	static bool last_en;
	unsigned long flags;

	spin_lock_irqsave(&mcdi_cpc_prof_spin_lock, flags);

	if (last_en == enable)
		goto out;

	if (enable)
		__mcdi_cpc_prof_enable();
	else
		__mcdi_cpc_prof_disable();

	last_en = enable;

out:
	spin_unlock_irqrestore(&mcdi_cpc_prof_spin_lock, flags);
}

static void mcdi_cpc_get_cpu_lat(int cpu, unsigned int *on, unsigned int *off)
{
	unsigned int lat;

	lat = mcdi_read(CPC_CPU_LATENCY(cpu));

	*on = (lat >> 16) & 0xFFFF;
	*off = lat & 0xFFFF;
}

static void mcdi_cpc_get_cpusys_lat(
		unsigned int *on, unsigned int *off)
{
	unsigned int lat_on;
	unsigned int lat_off;

	lat_on = mcdi_read(CPC_CLUSTER_ON_LATENCY);
	lat_off = mcdi_read(CPC_CLUSTER_OFF_LATENCY);

	*on = lat_on & 0xFFFF;
	*off = lat_off & 0xFFFF;
}

static void mcdi_cpc_get_mcusys_lat(unsigned int *on, unsigned int *off)
{
	unsigned int lat;

	lat = mcdi_read(CPC_MCUSYS_LATENCY);

	*on = (lat >> 16) & 0xFFFF;
	*off = lat & 0xFFFF;
}

#define __mcdi_cpc_lat_dist(d, lat)			\
		do {					\
			int idx;			\
							\
			if (lat < 130)			\
				idx = MCDI_PROF_U10_US;	\
			else if (lat < 260)		\
				idx = MCDI_PROF_U20_US;	\
			else if (lat < 651)		\
				idx = MCDI_PROF_U50_US;	\
			else				\
				idx = MCDI_PROF_O50_US;	\
							\
			d[idx]++;			\
		} while (0)

static void mcdi_cpc_lat_dist(struct mcdi_cpc_distribute *dist,
				unsigned int on, unsigned int off)
{
	unsigned long flags;

	spin_lock_irqsave(&mcdi_cpc_prof_spin_lock, flags);

	__mcdi_cpc_lat_dist(dist->on, on);
	__mcdi_cpc_lat_dist(dist->off, off);
	dist->cnt++;

	spin_unlock_irqrestore(&mcdi_cpc_prof_spin_lock, flags);
}

static bool mcdi_cpc_cluster_resume(int cluster)
{
	unsigned int cnt;
	bool mp_resume = false;

	cnt = mcdi_read(CPC_DORMANT_COUNTER);

	if (cpc.mp_off_cnt[cluster] != cnt) {
		cpc.mp_off_cnt[cluster] = cnt;
		mp_resume = true;
	}

	return mp_resume;
}

#define __mcdi_cpc_record_lat(sum, max, lat)	\
		do {				\
			if (lat > max)		\
				max = lat;	\
			(sum) += (lat);		\
		} while (0)

static void mcdi_cpc_on_record_lat(struct mcdi_cpc_prof *prof,
					unsigned int lat)
{
	unsigned long flags;

	if (lat == 0)
		return;

	spin_lock_irqsave(&mcdi_cpc_prof_spin_lock, flags);

	__mcdi_cpc_record_lat(prof->on_sum, prof->on_max, lat);
	prof->on_cnt++;

	spin_unlock_irqrestore(&mcdi_cpc_prof_spin_lock, flags);
}

static void mcdi_cpc_off_record_lat(struct mcdi_cpc_prof *prof,
					unsigned int lat)
{
	unsigned long flags;

	if (lat == 0)
		return;

	spin_lock_irqsave(&mcdi_cpc_prof_spin_lock, flags);

	__mcdi_cpc_record_lat(prof->off_sum, prof->off_max, lat);
	prof->off_cnt++;

	spin_unlock_irqrestore(&mcdi_cpc_prof_spin_lock, flags);
}

static void mcdi_cpc_record_lat(struct mcdi_cpc_prof *prof,
					unsigned int on, unsigned int off)
{
	mcdi_cpc_on_record_lat(prof, on);
	mcdi_cpc_off_record_lat(prof, off);
}

static void mcdi_cpc_save_cluster_off(int cpu, int core_off, int cpu_type)
{
	if (cpc.mp_off_lat[cpu]) {
		mcdi_cpc_off_record_lat(&cpc.cluster[cpu_type],
					cpc.mp_off_lat[cpu] + core_off);
		cpc.mp_off_lat[cpu] = 0;
	}
}

static void mcdi_cpc_save_latency(int cpu, int last_core_taken)
{
	struct mcdi_cpc_prof *prof;
	unsigned int cpu_type;
	unsigned int core_on, core_off;
	unsigned int cluster_on, cluster_off;
	unsigned int mcusys_on, mcusys_off;
	unsigned int last_core_id;
	bool mcusys_resume;

	if (!cpc.sta.prof_en || cpc.sta.prof_pause)
		return;

	if (unlikely(cpu_is_invalid(cpu)))
		return;

	cpc.sta.prof_saving = true;

	/* Record CPU on/off latency */
	cpu_type = cpu_type_idx_get(cpu);
	prof = &cpc.cpu[cpu_type];

	mcdi_cpc_get_cpu_lat(cpu, &core_on, &core_off);
	mcdi_cpc_record_lat(prof, core_on, core_off);
	mcdi_cpc_lat_dist(&cpc.dist_cnt[cpu_type], core_on, core_off);

	mcusys_resume = (cpu == last_core_taken);

	/* Record cluster on latency */
	if (mcdi_cpc_cluster_resume(cluster_idx_get(cpu))
			|| mcusys_resume) {

		mcdi_cpc_get_cpusys_lat(&cluster_on, &cluster_off);

		prof = &cpc.cluster[cpu_type];

		mcdi_cpc_on_record_lat(prof, cluster_on + core_on);

		last_core_id = get_cluster_off_token(cpu);

		if (!(mcusys_resume || cpu_is_invalid(last_core_id)))
			cpc.mp_off_lat[last_core_id] = cluster_off;
	}

	/* Record cluster off and mcusys on/off latency */
	if (mcusys_resume) {

		prof = &cpc.cluster[cpu_type];

		mcdi_cpc_off_record_lat(prof, cluster_off + core_off);

		mcdi_cpc_get_mcusys_lat(&mcusys_on, &mcusys_off);

		prof = &cpc.mcusys;

		mcdi_cpc_record_lat(prof, mcusys_on, mcusys_off);
	} else {
		mcdi_cpc_save_cluster_off(cpu, core_off, cpu_type);
	}

	cpc.sta.prof_saving = false;
}

void mcdi_cpc_reflect(int cpu, int last_core_taken)
{
	mcdi_cpc_save_latency(cpu, last_core_taken);
	mcdi_write(CPC_CPU_ON_SW_HINT_CLR, 1 << cpu);
}

/* procfs */
static char dbg_buf[4096] = { 0 };
static char cmd_buf[512] = { 0 };

static ssize_t mcdi_cpc_read(struct file *filp,
		char __user *userbuf, size_t count, loff_t *f_pos)
{
	int i, j, len = 0;
	char *p = dbg_buf;
	struct mcdi_cpc_prof *prof;
	struct mcdi_cpc_distribute *dist;

	cpc.sta.prof_pause = true;

	while (cpc.sta.prof_saving)
		udelay(1);

	for (i = 0; i < PROF_TYPE_NUM; i++) {
		prof = &cpc.p[i];

		mcdi_log("%s\n", prof->name);

		if (prof->off_cnt) {
			mcdi_log("\toff : avg = %2dus, max = %3dus, cnt = %d\n",
				cpc_tick_to_us(prof->off_sum / prof->off_cnt),
				cpc_tick_to_us(prof->off_max),
				prof->off_cnt);
		} else {
			mcdi_log("\toff : None\n");
		}

		if (prof->on_cnt) {
			mcdi_log("\ton  : avg = %2dus, max = %3dus, cnt = %d\n",
				cpc_tick_to_us(prof->on_sum / prof->on_cnt),
				cpc_tick_to_us(prof->on_max),
				prof->on_cnt);
		} else {
			mcdi_log("\ton  : None\n");
		}
	}

	mcdi_log("\n");

	for (i = 0; i < NF_CPU_TYPE; i++) {
		dist = &cpc.dist_cnt[i];
		prof = &cpc.cpu[i];

		if (dist->cnt == 0)
			continue;

		mcdi_log("%s\n", prof->name);

		for (j = 0; j < NF_MCDI_PROF_DIST; j++) {
			mcdi_log("pwr off latency (%s) :%3d%% (%d)\n",
					cpc_prof_dist_str[j],
					(100 * dist->off[j]) / dist->cnt,
					dist->off[j]);
		}

		for (j = 0; j < NF_MCDI_PROF_DIST; j++) {
			mcdi_log("pwr on  latency (%s) :%3d%% (%d)\n",
					cpc_prof_dist_str[j],
					(100 * dist->on[j]) / dist->cnt,
					dist->on[j]);
		}
	}

	cpc.sta.prof_pause = false;

	mcdi_log("\n");

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t mcdi_cpc_write(struct file *filp,
		const char __user *userbuf, size_t count, loff_t *f_pos)
{
	int ret = 0;
	unsigned long param = 0;
	char *cmd_ptr = cmd_buf;
	char *cmd_str = NULL;
	char *param_str = NULL;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	cmd_str = strsep(&cmd_ptr, " ");

	if (cmd_str == NULL)
		return -EINVAL;

	param_str = strsep(&cmd_ptr, " ");

	if (param_str == NULL)
		return -EINVAL;

	ret = kstrtoul(param_str, 16, &param);

	if (ret < 0)
		return -EINVAL;

	if (!strncmp(cmd_str, "profile", sizeof("profile")))
		mcdi_cpc_prof_en(!!param);
	else
		return -EINVAL;

	return count;
}

PROC_FOPS_MCDI(cpc);

void mcdi_procfs_cpc_init(struct proc_dir_entry *mcdi_dir)
{
	PROC_CREATE_MCDI(mcdi_dir, cpc);
}

void mcdi_cpc_init(void)
{
	int i;

	for (i = 0; i < NF_CPU_TYPE; i++) {
		snprintf(cpc.cpu[i].name, CPC_LAT_NAME_SIZE,
				get_cpu_type_str(i));
		snprintf(cpc.cluster[i].name, CPC_LAT_NAME_SIZE,
				"cluster off(%s)", get_cpu_type_str(i));
	}
	snprintf(cpc.mcusys.name, CPC_LAT_NAME_SIZE, "mcusys");
}

#else

void mcdi_cpc_prof_en(bool enable) {}
void mcdi_cpc_reflect(int cpu, int last_core_taken) {}
void mcdi_procfs_cpc_init(struct proc_dir_entry *mcdi_dir) {}
void mcdi_cpc_init(void) {}

#endif

