// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/pm_qos.h>

#include <mtk_lpm.h>
#include <mtk_lp_plat_reg.h>

#include "mtk_cpupm_dbg.h"
#include "mtk_cpuidle_cpc.h"

static DEFINE_SPINLOCK(cpc_prof_spin_lock);

/* profile latecny */
#define PROF_DEV_NAME_SIZE 36

#define get_core_type_index(cpu) ((cpu > 3) ? CPU_TYPE_B : CPU_TYPE_L)

struct mtk_cpc_lat_data {
	char name[PROF_DEV_NAME_SIZE];
	unsigned int on_sum;
	unsigned int on_max;
	unsigned int off_sum;
	unsigned int off_max;
	unsigned int on_cnt;
	unsigned int off_cnt;
};

struct mtk_cpc_device {
	union {
		struct mtk_cpc_lat_data p[DEV_TYPE_NUM];
		struct {
			struct mtk_cpc_lat_data cpu[NF_CPU_TYPE];
			struct mtk_cpc_lat_data cluster;
			struct mtk_cpc_lat_data mcusys;
		};
	};
	bool prof_en;
};

struct mtk_cpc_device cpc;


static void mtk_cpc_clr_lat(void)
{
	unsigned int size;
	unsigned int ofs;
	int i;

	ofs = sizeof(cpc.p[0].name);
	size = sizeof(struct mtk_cpc_lat_data) - ofs;

	for (i = 0; i < DEV_TYPE_NUM; i++)
		memset((char *)&cpc.p[i] + ofs, 0, size);
}

static void mtk_cpc_cal_lat(void)
{
}

static void mtk_cpc_get_cpu_lat(int cpu, unsigned int *on, unsigned int *off)
{
	unsigned int lat;

	lat = mtk_cpupm_mcusys_read(CPC_CPU_LATENCY(cpu));

	*on = (lat >> 16) & 0xFFFF;
	*off = lat & 0xFFFF;
}

static void mtk_cpc_get_cpusys_lat(unsigned int *on, unsigned int *off)
{
	unsigned int lat_on;
	unsigned int lat_off;

	lat_on = mtk_cpupm_mcusys_read(CPC_CLUSTER_ON_LATENCY);
	lat_off = mtk_cpupm_mcusys_read(CPC_CLUSTER_OFF_LATENCY);

	*on = lat_on & 0xFFFF;
	*off = lat_off & 0xFFFF;
}

static void mtk_cpc_get_mcusys_lat(unsigned int *on, unsigned int *off)
{
	unsigned int lat;

	lat = mtk_cpupm_mcusys_read(CPC_MCUSYS_LATENCY);

	*on = (lat >> 16) & 0xFFFF;
	*off = lat & 0xFFFF;
}

#define __mtk_cpc_record_lat(sum, max, lat)	\
		do {				\
			if (lat > max)		\
				max = lat;	\
			(sum) += (lat);		\
		} while (0)

static void mtk_cpc_on_record_lat(struct mtk_cpc_lat_data *lat,
					unsigned int ticks)
{
	unsigned long flags;

	if (ticks == 0)
		return;

	spin_lock_irqsave(&cpc_prof_spin_lock, flags);

	__mtk_cpc_record_lat(lat->on_sum, lat->on_max, ticks);
	lat->on_cnt++;

	spin_unlock_irqrestore(&cpc_prof_spin_lock, flags);
}

static void mtk_cpc_off_record_lat(struct mtk_cpc_lat_data *lat,
					unsigned int ticks)
{
	unsigned long flags;

	if (ticks == 0)
		return;

	spin_lock_irqsave(&cpc_prof_spin_lock, flags);

	__mtk_cpc_record_lat(lat->off_sum, lat->off_max, ticks);
	lat->off_cnt++;

	spin_unlock_irqrestore(&cpc_prof_spin_lock, flags);
}

static bool mtk_cpc_did_cluster_pwr_off(void)
{
	static unsigned int last_cnt;
	unsigned int cnt = mtk_cpupm_mcusys_read(CPC_DORMANT_COUNTER);

	/**
	 * Cluster off count
	 * bit[0:15] : memory retention
	 * bit[16:31] : memory off
	 */
	if ((cnt & 0x7FFF) == 0)
		cnt = ((cnt >> 16) & 0x7FFF);
	else
		cnt = cnt & 0x7FFF;

	cnt += mtk_cpupm_syssram_read(SYSRAM_CPC_CPUSYS_CNT_BACKUP);

	if (last_cnt == cnt)
		return false;

	last_cnt = cnt;

	return true;
}

static bool mtk_cpc_did_mcusys_pwr_off(void)
{
	static unsigned int last_cnt;
	unsigned int cnt = mtk_cpupm_syssram_read(SYSRAM_CPC_MCUSYS_CNT_BACKUP);

	cnt += mtk_cpupm_syssram_read(SYSRAM_MCUSYS_CNT);

	if (last_cnt == cnt)
		return false;

	last_cnt = cnt;

	return true;
}

static void mtk_cpc_save_latency(int cpu)
{
	unsigned int on, off;
	struct mtk_cpc_lat_data *lat;

	lat = &cpc.cpu[get_core_type_index(cpu)];

	mtk_cpc_get_cpu_lat(cpu, &on, &off);
	mtk_cpc_on_record_lat(lat, on);
	mtk_cpc_off_record_lat(lat, off);

	if (mtk_cpc_did_mcusys_pwr_off()) {

		lat = &cpc.mcusys;

		mtk_cpc_get_mcusys_lat(&on, &off);
		mtk_cpc_on_record_lat(lat, on);
		mtk_cpc_off_record_lat(lat, off);

	} else if (!mtk_cpc_did_cluster_pwr_off()) {
		return;
	}

	lat = &cpc.cluster;

	mtk_cpc_get_cpusys_lat(&on, &off);
	mtk_cpc_on_record_lat(lat, on);
	mtk_cpc_off_record_lat(lat, off);
}

void mtk_cpc_prof_lat_dump(struct seq_file *m)
{
	struct mtk_cpc_lat_data *lat;
	int i;

	for (i = 0; i < DEV_TYPE_NUM; i++) {
		lat = &cpc.p[i];

		seq_printf(m, "%s\n", lat->name);

		if (lat->off_cnt) {
			seq_printf(m, "\toff : avg = %2dus, max = %3dus, cnt = %d\n",
				cpc_tick_to_us(lat->off_sum / lat->off_cnt),
				cpc_tick_to_us(lat->off_max),
				lat->off_cnt);
		} else {
			seq_puts(m, "\toff : None\n");
		}

		if (lat->on_cnt) {
			seq_printf(m, "\ton  : avg = %2dus, max = %3dus, cnt = %d\n",
				cpc_tick_to_us(lat->on_sum / lat->on_cnt),
				cpc_tick_to_us(lat->on_max),
				lat->on_cnt);
		} else {
			seq_puts(m, "\ton  : None\n");
		}
	}
}

void mtk_cpc_prof_start(void)
{
	mtk_cpupm_block();

	cpc_prof_en();

	mtk_cpc_clr_lat();

	cpc.prof_en = true;

	mtk_cpupm_allow();
}

void mtk_cpc_prof_stop(void)
{
	mtk_cpupm_block();

	cpc.prof_en = false;

	mtk_cpc_cal_lat();

	cpc_prof_dis();

	mtk_cpupm_allow();
}

int mtk_cpc_notify(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct mtk_lpm_nb_data *nb_data = (struct mtk_lpm_nb_data *)data;

	if (!cpc.prof_en)
		return NOTIFY_OK;

	if (action & MTK_LPM_NB_BEFORE_REFLECT)
		mtk_cpc_save_latency(nb_data->cpu);

	return NOTIFY_OK;
}

struct notifier_block mtk_cpc_nb = {
	.notifier_call = mtk_cpc_notify,
};

int __init mtk_cpc_init(void)
{
	cpc.prof_en = false;

	snprintf(cpc.cpu[CPU_TYPE_L].name, PROF_DEV_NAME_SIZE, "cpu_L");
	snprintf(cpc.cpu[CPU_TYPE_B].name, PROF_DEV_NAME_SIZE, "cpu_B");
	snprintf(cpc.cluster.name, PROF_DEV_NAME_SIZE, "cluster");
	snprintf(cpc.mcusys.name, PROF_DEV_NAME_SIZE, "mcusys");

	mtk_lpm_notifier_register(&mtk_cpc_nb);
	return 0;
}

void __exit mtk_cpc_exit(void)
{
	mtk_lpm_notifier_unregister(&mtk_cpc_nb);
}

