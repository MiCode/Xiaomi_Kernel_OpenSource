/*
 * Copyright (C) 2016 MediaTek Inc.
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

/* system includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/sched/rt.h>
#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/bitops.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/types.h>
#include <linux/suspend.h>
#include <linux/topology.h>
#include <linux/math64.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/mtk_io.h>
#include <mt-plat/aee.h>
#include <trace/events/mtk_events.h>

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#include "sspm_ipi.h"
#endif

#include <mt-plat/met_drv.h>

#ifdef CONFIG_HYBRID_CPU_DVFS

#include "mtk_cpufreq_internal.h"
#include "mtk_cpufreq_platform.h"
#include "mtk_cpufreq_hybrid.h"
#include "mtk_cpufreq_opp_pv_table.h"
#include "mtk_cpufreq_debug.h"

#include <linux/of_address.h>
u32 *g_dbg_repo;
static u32 dvfsp_probe_done;
void __iomem *log_repo;
static void __iomem *csram_base;
/* static void __iomem *cspm_base; */
#define csram_read(offs)		__raw_readl(csram_base + (offs))
#define csram_write(offs, val)		\
	mt_reg_sync_writel(val, csram_base + (offs))

#define OFFS_TBL_S		0x0010
#define OFFS_TBL_E		0x0250
#define PVT_TBL_SIZE    (OFFS_TBL_E - OFFS_TBL_S)
#define OFFS_DATA_S		0x02a0
#define OFFS_LOG_S		0x03d0
#define OFFS_LOG_E		(OFFS_LOG_S + DVFS_LOG_NUM * ENTRY_EACH_LOG * 4)

#define MAX_LOG_FETCH 40
/* log_box_parsed[MAX_LOG_FETCH] is also used to save last log entry */
static struct cpu_dvfs_log_box log_box_parsed[1 + MAX_LOG_FETCH];

void parse_time_log_content(unsigned int time_stamp_l_log,
	unsigned int time_stamp_h_log, int idx)
{
	if (time_stamp_h_log == 0 && time_stamp_l_log == 0)
		log_box_parsed[idx].time_stamp = 0;

	log_box_parsed[idx].time_stamp =
	((unsigned long long)time_stamp_h_log << 32) |
		(unsigned long long)(time_stamp_l_log);
}

void parse_log_content(unsigned int *local_buf, int idx)
{
	struct cpu_dvfs_log *log_box = (struct cpu_dvfs_log *)local_buf;
	struct mt_cpu_dvfs *p;
	int i;

	for_each_cpu_dvfs(i, p) {
		log_box_parsed[idx].cluster_opp_cfg[i].limit_idx =
			log_box->cluster_opp_cfg[i].limit;
		log_box_parsed[idx].cluster_opp_cfg[i].base_idx =
			log_box->cluster_opp_cfg[i].base;
		log_box_parsed[idx].cluster_opp_cfg[i].freq_idx =
			log_box->cluster_opp_cfg[i].opp_idx_log;
	}
}

spinlock_t cpudvfs_lock;
static struct task_struct *Ripi_cpu_dvfs_task;
struct ipi_action cpufreq_act;
uint32_t cpufreq_buf[4];
int Ripi_cpu_dvfs_thread(void *data)
{
	int i, ret;
	struct mt_cpu_dvfs *p;
	unsigned long flags;
	uint32_t pwdata[4];
	struct cpufreq_freqs freqs;

	int previous_limit = -1;
	int num_log;
	unsigned int buf[ENTRY_EACH_LOG] = {0};
	unsigned int bk_log_offs;
	unsigned int buf_freq;
	unsigned long long tf_sum, t_diff, avg_f;
	int j;
	struct cpu_dvfs_log_box tmp;
	unsigned int tmp_f;
	unsigned int tmp_limit;
	unsigned int tmp_base;

	/* tag_pr_info("CPU DVFS received thread\n"); */
	cpufreq_act.data = (void *)cpufreq_buf;
	ret = sspm_ipi_recv_registration_ex(IPI_ID_CPU_DVFS,
		&cpudvfs_lock, &cpufreq_act);

	if (ret != 0) {
		pr_info("Error: ipi_recv_registration CPU DVFS error: %d\n",
			ret);
		do {
			msleep(1000);
		} while (!kthread_should_stop());
		return (-1);
	}
/*
 * tag_pr_info("sspm_ipi_recv_registration IPI_ID_CPU_DVFS pass!!(%d)\n",
 * ret);
 */

	/* an endless loop in which we are doing our work */
	do {
		/* tag_pr_info("sspm_ipi_recv_wait IPI_ID_CPU_DVFS\n"); */
		sspm_ipi_recv_wait(IPI_ID_CPU_DVFS);
/*
 * tag_pr_info("Info: CPU DVFS thread received ID=%d, i=%d\n",
 * cpufreq_act.id, i);
 */
		spin_lock_irqsave(&cpudvfs_lock, flags);
		memcpy(pwdata, cpufreq_buf, sizeof(pwdata));
		spin_unlock_irqrestore(&cpudvfs_lock, flags);

		bk_log_offs = pwdata[0];
		num_log = 0;
		while (bk_log_offs != pwdata[1]) {
			buf[0] = csram_read(bk_log_offs);
			bk_log_offs += 4;
			if (bk_log_offs >= OFFS_LOG_E)
				bk_log_offs = OFFS_LOG_S;
			buf[1] = csram_read(bk_log_offs);
			bk_log_offs += 4;
			if (bk_log_offs >= OFFS_LOG_E)
				bk_log_offs = OFFS_LOG_S;

			/* For parsing timestamp */
			parse_time_log_content(buf[0], buf[1], num_log);
			for (j = 2; j < ENTRY_EACH_LOG; j++) {
				/* Read out sram content */
				buf[j] = csram_read(bk_log_offs);
				bk_log_offs += 4;
				if (bk_log_offs >= OFFS_LOG_E)
					bk_log_offs = OFFS_LOG_S;
			}

			/* For parsing freq idx */
			parse_log_content(buf, num_log);
			num_log++;
		}

		cpufreq_lock(flags);
		for_each_cpu_dvfs_only(i, p) {
			if (!p->armpll_is_available)
				continue;

			if (num_log == 1)
				j =
				log_box_parsed[0].cluster_opp_cfg[i].freq_idx;
			else {
				tf_sum = 0;
				for (j = num_log - 1; j >= 1; j--) {
					tmp = log_box_parsed[j - 1];
					tmp_f = tmp.cluster_opp_cfg[i].freq_idx;
					buf_freq = cpu_dvfs_get_freq_by_idx(p,
						tmp_freq);
					tf_sum += (log_box_parsed[j].time_stamp
					- log_box_parsed[j-1].time_stamp) *
					(buf_freq/1000);
				}
				t_diff = log_box_parsed[num_log - 1].time_stamp
					- log_box_parsed[0].time_stamp;
#if defined(__LP64__) || defined(_LP64)
				avg_f = tf_sum / t_diff;
#else
				avg_f = div64_u64(tf_sum, t_diff);
#endif
				avg_f *= 1000;
				for (j = p->nr_opp_tbl - 1; j >= 1; j--) {
					if (cpu_dvfs_get_freq_by_idx(p, j)
						>= avg_f)
						break;
				}
			}

			/* Avoid memory issue */
			if (p->mt_policy && p->mt_policy->governor &&
				p->mt_policy->governor_enabled &&
				(p->mt_policy->cpu < 10)
				&& (p->mt_policy->cpu >= 0)) {
				int cid;

				previous_limit = p->idx_opp_ppm_limit;
				tmp = log_box_parsed[num_log - 1];
				tmp_limit = tmp.cluster_opp_cfg[i].limit_idx;
				tmp_base = tmp.cluster_opp_cfg[i].base_idx;
				p->idx_opp_ppm_limit = (int)tmp_limit;
				p->idx_opp_ppm_base = (int)tmp_base;

				if (j < p->idx_opp_ppm_limit)
					j = p->idx_opp_ppm_limit;

				if (j > p->idx_opp_ppm_base)
					j = p->idx_opp_ppm_base;

				/* Update policy min/max */
				p->mt_policy->min =
					cpu_dvfs_get_freq_by_idx(p,
						p->idx_opp_ppm_base);
				p->mt_policy->max =
					cpu_dvfs_get_freq_by_idx(p,
						p->idx_opp_ppm_limit);

				cid = arch_get_cluster_id(p->mt_policy->cpu);
				if (cid == 0)
					met_tag_oneshot(0, "sched_dvfs_max_c0",
						p->mt_policy->max);
				else if (cid == 1)
					met_tag_oneshot(0, "sched_dvfs_max_c1",
						p->mt_policy->max);
				else if (cid == 2)
					met_tag_oneshot(0, "sched_dvfs_max_c2",
						p->mt_policy->max);

				/* Policy notification */
				if (p->idx_opp_tbl != j ||
					(p->idx_opp_ppm_limit
						!= previous_limit)) {
					freqs.old = cpu_dvfs_get_cur_freq(p);
					freqs.new = cpu_dvfs_get_freq_by_idx(
							p, j);
					p->idx_opp_tbl = j;
					/* Update frequency change */
					cpufreq_freq_transition_begin(
						p->mt_policy, &freqs);
					cpufreq_freq_transition_end(
						p->mt_policy, &freqs, 0);
				}
			}
		}
		cpufreq_unlock(flags);

	} while (!kthread_should_stop());
	return 0;
}

int dvfs_to_spm2_command(u32 cmd, struct cdvfs_data *cdvfs_d)
{
#define OPT				(0) /* reserve for extensibility */
#define DVFS_D_LEN		(4) /* # of cmd + arg0 + arg1 + ... */
	unsigned int len = DVFS_D_LEN;
	int ack_data;
	unsigned int ret = 0;

	/* cpufreq_ver_dbg("#@# %s(%d) cmd %x\n", __func__, __LINE__, cmd); */
	switch (cmd) {
	case IPI_DVFS_INIT_PTBL:
		cdvfs_d->cmd = cmd;

		pr_info("I'd like to initialize sspm DVFS, segment code = %d\n",
			cdvfs_d->u.set_fv.arg[0]);

		ret = sspm_ipi_send_sync_new(IPI_ID_CPU_DVFS,
			IPI_OPT_POLLING, cdvfs_d, len, &ack_data, 1);
		if (ret != 0) {
			cpufreq_ver_dbg("#@# %s(%d) sspm_ipi_send_sync ret %d\n",
				__func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			cpufreq_ver_dbg("#@# %s(%d) cmd(%d) return %d\n",
				__func__, __LINE__, cmd, ret);
		}
		break;

	case IPI_DVFS_INIT:
		cdvfs_d->cmd = cmd;

		pr_info("I'd like to initialize sspm DVFS, segment code = %d\n",
			cdvfs_d->u.set_fv.arg[0]);

		ret = sspm_ipi_send_sync_new(IPI_ID_CPU_DVFS,
			IPI_OPT_POLLING, cdvfs_d, len, &ack_data, 1);
		if (ret != 0) {
			cpufreq_ver_dbg("#@# %s(%d) sspm_ipi_send_sync ret %d\n",
				__func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			cpufreq_ver_dbg("#@# %s(%d) cmd(%d) return %d\n",
				__func__, __LINE__, cmd, ret);
		}
		break;

	case IPI_SET_CLUSTER_ON_OFF:
		cdvfs_d->cmd = cmd;

		cpufreq_ver_dbg("I'd like to set cluster%d ON/OFF state to %d)\n",
			cdvfs_d->u.set_fv.arg[0], cdvfs_d->u.set_fv.arg[1]);

		aee_record_cpu_dvfs_cb(6);
		ret = sspm_ipi_send_sync_new(IPI_ID_CPU_DVFS,
			IPI_OPT_POLLING, cdvfs_d, len, &ack_data, 1);
		aee_record_cpu_dvfs_cb(7);
		if (ret != 0) {
			tag_pr_notice("ret = %d, set cluster%d ON/OFF state to %d\n",
				ret, cdvfs_d->u.set_fv.arg[0],
				cdvfs_d->u.set_fv.arg[1]);
#if 0
			cpufreq_ver_dbg("#@# %s(%d) sspm_ipi_send_sync ret %d\n",
				__func__, __LINE__, ret);
#endif
		} else if (ack_data < 0) {
			tag_pr_notice("ret = %d, set cluster%d ON/OFF state to %d\n",
				ret, cdvfs_d->u.set_fv.arg[0],
				cdvfs_d->u.set_fv.arg[1]);
#if 0
			ret = ack_data;
			cpufreq_ver_dbg("#@# %s(%d) cmd(%d) return %d\n",
				__func__, __LINE__, cmd, ret);
#endif
		}
		aee_record_cpu_dvfs_cb(8);
		break;
#if 0
	case IPI_SET_FREQ:
		cdvfs_d->cmd = cmd;

		cpufreq_ver_dbg("I'd like to set cluster%d freq to %d)\n",
			cdvfs_d->u.set_fv.arg[0], cdvfs_d->u.set_fv.arg[1]);

		ret = sspm_ipi_send_sync_new(IPI_ID_CPU_DVFS,
			IPI_OPT_POLLING, cdvfs_d, len, &ack_data, 1);
		if (ret != 0) {
			cpufreq_ver_dbg("#@# %s(%d) sspm_ipi_send_sync ret %d\n",
				__func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			cpufreq_ver_dbg("#@# %s(%d) cmd(%d) return %d\n",
				__func__, __LINE__, cmd, ret);
		}
		break;

	case IPI_SET_VOLT:
		cdvfs_d->cmd = cmd;

		cpufreq_ver_dbg("I'd like to set cluster%d volt to %d)\n",
			cdvfs_d->u.set_fv.arg[0], cdvfs_d->u.set_fv.arg[1]);

		ret = sspm_ipi_send_sync_new(IPI_ID_CPU_DVFS, IPI_OPT_POLLING,
			cdvfs_d, len, &ack_data, 1);
		if (ret != 0) {
			cpufreq_ver_dbg("#@# %s(%d) sspm_ipi_send_sync ret %d\n",
				__func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			cpufreq_ver_dbg("#@# %s(%d) cmd(%d) return %d\n",
				__func__, __LINE__, cmd, ret);
		}
		break;

	case IPI_GET_VOLT:
		cdvfs_d->cmd = cmd;

		cpufreq_ver_dbg("I'd like to get volt from Buck%d\n",
			cdvfs_d->u.set_fv.arg[0]);

		ret = sspm_ipi_send_sync_new(IPI_ID_CPU_DVFS,
			IPI_OPT_POLLING, cdvfs_d, len, &ack_data, 1);
		cpufreq_ver_dbg("Get volt = %d\n", ack_data);
		if (ret != 0) {
			cpufreq_ver_dbg("#@# %s(%d) sspm_ipi_send_sync ret %d\n",
				__func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			cpufreq_ver_dbg("#@# %s(%d) cmd(%d) return %d\n",
				__func__, __LINE__, cmd, ret);
		}
		ret = ack_data;
		break;

	case IPI_GET_FREQ:
		cdvfs_d->cmd = cmd;

		cpufreq_ver_dbg("I'd like to get freq from pll%d\n",
			cdvfs_d->u.set_fv.arg[0]);

		ret = sspm_ipi_send_sync_new(IPI_ID_CPU_DVFS,
			IPI_OPT_POLLING, cdvfs_d, len, &ack_data, 1);
		cpufreq_ver_dbg("Get freq = %d\n", ack_data);
		if (ret != 0) {
			cpufreq_ver_dbg("#@# %s(%d) sspm_ipi_send_sync ret %d\n",
				__func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			cpufreq_ver_dbg("#@# %s(%d) cmd(%d) return %d\n",
				__func__, __LINE__, cmd, ret);
		}
		ret = ack_data;
		break;
#endif
	case IPI_TURBO_MODE:
		cdvfs_d->cmd = cmd;

		cpufreq_ver_dbg("I'd like to set turbo mode to %d(%d, %d)\n",
			cdvfs_d->u.set_fv.arg[0], cdvfs_d->u.set_fv.arg[1],
			cdvfs_d->u.set_fv.arg[2]);

		ret = sspm_ipi_send_sync_new(IPI_ID_CPU_DVFS,
			IPI_OPT_POLLING, cdvfs_d, len, &ack_data, 1);
		if (ret != 0) {
			cpufreq_ver_dbg("#@# %s(%d) sspm_ipi_send_sync ret %d\n",
				__func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			cpufreq_ver_dbg("#@# %s(%d) cmd(%d) return %d\n",
				__func__, __LINE__, cmd, ret);
		}
		break;

	case IPI_TIME_PROFILE:
		cdvfs_d->cmd = cmd;

		cpufreq_ver_dbg("I'd like to dump time profile data(%d, %d, %d)\n",
			cdvfs_d->u.set_fv.arg[0], cdvfs_d->u.set_fv.arg[1],
			cdvfs_d->u.set_fv.arg[2]);

		ret = sspm_ipi_send_sync_new(IPI_ID_CPU_DVFS,
			IPI_OPT_POLLING, cdvfs_d, len, &ack_data, 1);
		if (ret != 0) {
			cpufreq_ver_dbg("#@# %s(%d) sspm_ipi_send_sync ret %d\n",
				__func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			cpufreq_ver_dbg("#@# %s(%d) cmd(%d) return %d\n",
				__func__, __LINE__, cmd, ret);
		}
		break;

	default:
		cpufreq_ver_dbg("#@# %s(%d) cmd(%d) wrong!!!\n",
			__func__, __LINE__, cmd);
		break;
	}

	return ret;
}

#define DBG_REPO_S		CSRAM_BASE
#define DBG_REPO_E		(DBG_REPO_S + CSRAM_SIZE)
#define DBG_REPO_TBL_S		(DBG_REPO_S + OFFS_TBL_S)
#define DBG_REPO_TBL_E		(DBG_REPO_S + OFFS_TBL_E)
#define DBG_REPO_DATA_S		(DBG_REPO_S + OFFS_DATA_S)
#define DBG_REPO_DATA_E		(DBG_REPO_S + OFFS_LOG_S)
#define DBG_REPO_LOG_S		(DBG_REPO_S + OFFS_LOG_S)
#define DBG_REPO_LOG_E		(DBG_REPO_S + OFFS_LOG_E)
#define DBG_REPO_SIZE		(DBG_REPO_E - DBG_REPO_S)
#define DBG_REPO_NUM		(DBG_REPO_SIZE / sizeof(u32))
#define REPO_I_DATA_S		(OFFS_DATA_S / sizeof(u32))
#define REPO_I_LOG_S		(OFFS_LOG_S / sizeof(u32))
#define REPO_GUARD0		0x55aa55aa
#define REPO_GUARD1		0xaa55aa55

#define OFFS_TURBO_FREQ		0x02a4	/* 169 */
#define OFFS_TURBO_VOLT		0x02a8	/* 170 */

#define OFFS_TURBO_DIS		0x02b8	/* 174 */
#define OFFS_SCHED_DIS		0x02bc	/* 175 */
#define OFFS_STRESS_EN		0x02c0	/* 176 */

/* EEM Update Flag */
#define OFFS_EEM_S		0x0300	/* 192 */
#define OFFS_EEM_E		0x030c	/* 195 */

/* ICCS idx */
#define OFFS_ICCS_IDX_S		0x0310	/* 196 */
#define OFFS_ICCS_IDX_E		0x0318	/* 198 */

/* PPM idx */
#define OFFS_PPM_LIMIT_S	0x0320	/* 200 */

/* CUR Vproc */
#define OFFS_CUR_VPROC_S	0x032c	/* 203 */
#define OFFS_CUR_VPROC_E	0x0350	/* 212 */

/* CUR idx */
#define OFFS_CUR_FREQ_S		0x0354	/* 213 */
#define OFFS_CUR_FREQ_E		0x0378	/* 222 */

/* WFI idx */
#define OFFS_WFI_S		0x037c	/* 223 */
#define OFFS_WFI_E		0x03a0	/* 232 */

/* Schedule assist idx */
#define OFFS_SCHED_S		0x03a4	/* 233 */
#define OFFS_SCHED_E		0x03c8	/* 242 */

static u32 g_dbg_repo_bak[DBG_REPO_NUM];
static int _mt_dvfsp_pdrv_probe(struct platform_device *pdev)
{
	/* cspm_base = of_iomap(pdev->dev.of_node, 0); */

	csram_base = of_iomap(pdev->dev.of_node, 1);

	if (!csram_base)
		return -ENOMEM;

	dvfsp_probe_done = 1;

	return 0;
}

static int _mt_dvfsp_pdrv_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id dvfsp_of_match[] = {
	{ .compatible = DVFSP_DT_NODE, },
	{}
};
#endif

static struct platform_driver _mt_dvfsp_pdrv = {
	.probe = _mt_dvfsp_pdrv_probe,
	.remove = _mt_dvfsp_pdrv_remove,
	.driver = {
		   .name = "dvfsp",
		   .owner = THIS_MODULE,
		   .of_match_table	= of_match_ptr(dvfsp_of_match),
	},
};

int cpuhvfs_set_init_ptbl(void)
{
	struct cdvfs_data cdvfs_d;

	/* seg code */
	cdvfs_d.u.set_fv.arg[0] = 0;
	dvfs_to_spm2_command(IPI_DVFS_INIT_PTBL, &cdvfs_d);

	return 0;
}

int cpuhvfs_set_turbo_scale(unsigned int turbo_f, unsigned int turbo_v)
{
	csram_write(OFFS_TURBO_FREQ, turbo_f);
	csram_write(OFFS_TURBO_VOLT, turbo_v);

	return 0;
}

int cpuhvfs_set_init_sta(void)
{
	struct cdvfs_data cdvfs_d;

	/* seg code */
	cdvfs_d.u.set_fv.arg[0] = 0;
	dvfs_to_spm2_command(IPI_DVFS_INIT, &cdvfs_d);

	return 0;
}

int cpuhvfs_set_cluster_on_off(int cluster_id, int state)
{
	struct cdvfs_data cdvfs_d;

	/* Cluster, ON:1/OFF:0 */
	cdvfs_d.u.set_fv.arg[0] = cluster_id;
	cdvfs_d.u.set_fv.arg[1] = state;

	aee_record_cpu_dvfs_cb(5);
	dvfs_to_spm2_command(IPI_SET_CLUSTER_ON_OFF, &cdvfs_d);

	return 0;
}

int cpuhvfs_set_min_max(int cluster_id, int base, int limit)
{
#ifdef PPM_AP_SIDE
	csram_write((OFFS_PPM_LIMIT_S + (cluster_id * 4)),
		(limit << 16 | base));
#endif
	return 0;
}

int cpuhvfs_set_dvfs_stress(unsigned int en)
{
	csram_write(OFFS_STRESS_EN, en);
	return 0;
}

int cpuhvfs_get_sched_dvfs_disable(void)
{
	unsigned int disable;

	disable = csram_read(OFFS_SCHED_DIS);

	return disable;
}

int cpuhvfs_set_sched_dvfs_disable(unsigned int disable)
{
	csram_write(OFFS_SCHED_DIS, disable);
	return 0;
}

int cpuhvfs_set_turbo_disable(unsigned int disable)
{
	csram_write(OFFS_TURBO_DIS, disable);
	return 0;
}

int cpuhvfs_get_cur_dvfs_freq_idx(int cluster_id)
{
	int idx = 0;

	idx = csram_read(OFFS_CUR_FREQ_S + (cluster_id * 4));

	return idx;
}

int cpuhvfs_set_dvfs(int cluster_id, unsigned int freq)
{
	struct mt_cpu_dvfs *p;
	unsigned int freq_idx = 0;

	p = id_to_cpu_dvfs(cluster_id);

	/* [3:0] freq_idx */
	freq_idx = _search_available_freq_idx(p, freq, 0);
	csram_write((OFFS_WFI_S + (cluster_id * 4)), freq_idx);

	return 0;
}

int cpuhvfs_get_cur_volt(int cluster_id)
{
	struct mt_cpu_dvfs *p;

	p = id_to_cpu_dvfs(cluster_id);

	return csram_read(OFFS_CUR_VPROC_S + (p->Vproc_buck_id * 4));
}

int cpuhvfs_get_volt(int buck_id)
{
#if 0
	struct cdvfs_data cdvfs_d;
	int ret = 0;

	/* Cluster, Volt */
	cdvfs_d.u.set_fv.arg[0] = buck_id;

	ret = dvfs_to_spm2_command(IPI_GET_VOLT, &cdvfs_d);

	return ret;
#else
	return csram_read(OFFS_CUR_VPROC_S + (buck_id * 4));
#endif
}

int cpuhvfs_get_freq(int pll_id)
{
#if 0
	struct cdvfs_data cdvfs_d;
	int ret = 0;

	/* Cluster, Freq */
	cdvfs_d.u.set_fv.arg[0] = pll_id;

	ret = dvfs_to_spm2_command(IPI_GET_FREQ, &cdvfs_d);

	return ret;
#else
	return 0;
#endif
}

int cpuhvfs_set_volt(int cluster_id, unsigned int volt)
{
#if 0
	struct cdvfs_data cdvfs_d;

	cdvfs_d.u.set_fv.arg[0] = cluster_id;
	cdvfs_d.u.set_fv.arg[1] = volt;

	dvfs_to_spm2_command(IPI_SET_VOLT, &cdvfs_d);
#endif
	return 0;
}

int cpuhvfs_set_freq(int cluster_id, unsigned int freq)
{
#if 0
	struct cdvfs_data cdvfs_d;

	cdvfs_d.u.set_fv.arg[0] = cluster_id;
	cdvfs_d.u.set_fv.arg[1] = freq;

	dvfs_to_spm2_command(IPI_SET_FREQ, &cdvfs_d);
#endif
	return 0;
}

int cpuhvfs_set_turbo_mode(int turbo_mode, int freq_step, int volt_step)
{
	struct cdvfs_data cdvfs_d;

	/* Turbo, ON:1/OFF:0 */
	cdvfs_d.u.set_fv.arg[0] = turbo_mode;
	cdvfs_d.u.set_fv.arg[1] = freq_step;
	cdvfs_d.u.set_fv.arg[2] = volt_step;

	dvfs_to_spm2_command(IPI_TURBO_MODE, &cdvfs_d);

	return 0;
}

int cpuhvfs_get_time_profile(void)
{
#ifdef CPUDVFS_TIME_PROFILE
	struct cdvfs_data cdvfs_d;

	cdvfs_d.u.set_fv.arg[0] = 0;
	cdvfs_d.u.set_fv.arg[1] = 0;
	cdvfs_d.u.set_fv.arg[2] = 0;

	dvfs_to_spm2_command(IPI_TIME_PROFILE, &cdvfs_d);
#endif
	return 0;
}

int cpuhvfs_set_iccs_freq(enum mt_cpu_dvfs_id id, unsigned int freq)
{
	struct mt_cpu_dvfs *p;
	int freq_idx = 0;
	unsigned int cluster;

	p = id_to_cpu_dvfs(id);

	cluster = (id == MT_CPU_DVFS_LL) ? 0 :
		(id == MT_CPU_DVFS_L) ? 1 : 2;

	cpufreq_ver_dbg("ICCS: cluster = %d, freq = %d\n", cluster, freq);

	freq_idx = _search_available_freq_idx(p, freq, 0);
	/* [3:0] freq_idx */
	csram_write((OFFS_ICCS_IDX_S + (cluster * 4)), freq_idx);

	cpuhvfs_set_cluster_on_off(id, 2);

	return 0;
}

unsigned int counter;
int cpuhvfs_set_cluster_load_freq(enum mt_cpu_dvfs_id id, unsigned int freq)
{
	struct mt_cpu_dvfs *p;
	int freq_idx = 0;
	unsigned int cluster;
	unsigned int buf;

	p = id_to_cpu_dvfs(id);

	cluster = (id == MT_CPU_DVFS_LL) ? 0 :
		(id == MT_CPU_DVFS_L) ? 1 : 2;

	cpufreq_ver_dbg("sched: cluster = %d, freq = %d\n", cluster, freq);

	counter++;
	if (counter > 255)
		counter = 1;

	/* [3:0] freq_idx, [11:4] counter */
	freq_idx = _search_available_freq_idx(p, freq, 0);

	buf = ((counter << 4) | freq_idx);

	csram_write((OFFS_SCHED_S + (cluster * 4)), buf);

	cpufreq_ver_dbg("sched: buf = 0x%x\n", buf);

	trace_sched_update(cluster, csram_read(OFFS_SCHED_S + (cluster * 4)));

	return 0;
}

u32 *recordRef;
static unsigned int *recordTbl;

int cpuhvfs_update_volt(unsigned int cluster_id,
	unsigned int *volt_tbl, char nr_volt_tbl)
{
#ifdef EEM_AP_SIDE
	int i;
	int index;

	for (i = 0; i < nr_volt_tbl; i++) {
		index = (cluster_id * 36) + i;
		recordRef[index] = ((volt_tbl[i] & 0xFFF) << 16) |
			(recordRef[index] & 0xFFFF);
	}
	csram_write((OFFS_EEM_S + (cluster_id * 4)), 1);
#endif

	return 0;
}

/*
 * Module driver
 */
void cpuhvfs_pvt_tbl_create(void)
{
	int i;
	unsigned int lv = _mt_cpufreq_get_cpu_level();

	recordRef = ioremap_nocache(DBG_REPO_TBL_S, PVT_TBL_SIZE);
	tag_pr_info("DVFS - @(Record)%s----->(%p)\n", __func__, recordRef);
	memset_io((u8 *)recordRef, 0x00, PVT_TBL_SIZE);

	recordTbl = xrecordTbl[lv];

	for (i = 0; i < NR_FREQ; i++) {
		/* Freq, Vproc, post_div, clk_div */
		/* LL [31:16] = Vproc, [15:0] = Freq */
		recordRef[i] =
			((*(recordTbl + (i * ARRAY_COL_SIZE) + 1) &
			0xFFF) << 16) |
			(*(recordTbl + (i * ARRAY_COL_SIZE)) & 0xFFFF);
		cpufreq_ver_dbg("DVFS - recordRef[%d] = 0x%x\n",
			i, recordRef[i]);
		/* LL [31:16] = clk_div, [15:0] = post_div */
		recordRef[i + NR_FREQ] =
			((*(recordTbl + (i * ARRAY_COL_SIZE) + 3) & 0xFF) << 16)
			| (*(recordTbl + (i * ARRAY_COL_SIZE) + 2) & 0xFF);
		cpufreq_ver_dbg("DVFS - recordRef[%d] = 0x%x\n",
			i + NR_FREQ, recordRef[i + NR_FREQ]);
		/* L [31:16] = Vproc, [15:0] = Freq */
		recordRef[i + 36] =
			((*(recordTbl + ((NR_FREQ * 1) + i) *
			ARRAY_COL_SIZE + 1) & 0xFFF) << 16) |
			(*(recordTbl + ((NR_FREQ * 1) + i) *
			ARRAY_COL_SIZE) & 0xFFFF);
		cpufreq_ver_dbg("DVFS - recordRef[%d] = 0x%x\n",
			i + 36, recordRef[i + 36]);
		/* L [31:16] = clk_div, [15:0] = post_div */
		recordRef[i + 36 + NR_FREQ] =
			((*(recordTbl + ((NR_FREQ * 1) + i) *
			ARRAY_COL_SIZE + 3) & 0xFF) << 16) |
			(*(recordTbl + ((NR_FREQ * 1) + i) *
			ARRAY_COL_SIZE + 2) & 0xFF);
		cpufreq_ver_dbg("DVFS - recordRef[%d] = 0x%x\n",
			i + 36 + NR_FREQ, recordRef[i + 36 + NR_FREQ]);
		/* B/CCI [31:16] = Vproc, [15:0] = Freq */
		recordRef[i + 72] =
			((*(recordTbl + ((NR_FREQ * 2) + i) *
			ARRAY_COL_SIZE + 1) & 0xFFF) << 16) |
			(*(recordTbl + ((NR_FREQ * 2) + i) *
			ARRAY_COL_SIZE) & 0xFFFF);
		cpufreq_ver_dbg("DVFS - recordRef[%d] = 0x%x\n",
			i + 72, recordRef[i + 72]);
		/* B/CCI [31:16] = clk_div, [15:0] = post_div */
		recordRef[i + 72 + NR_FREQ] =
			((*(recordTbl + ((NR_FREQ * 2) + i) *
			ARRAY_COL_SIZE + 3) & 0xFFF) << 16) |
			(*(recordTbl + ((NR_FREQ * 2) + i) *
			ARRAY_COL_SIZE + 2) & 0xFF);
		cpufreq_ver_dbg("DVFS - recordRef[%d] = 0x%x\n",
			i + 72 + NR_FREQ, recordRef[i + 72 + NR_FREQ]);

		if (NR_MT_CPU_DVFS > 3) {
			/* CCI [31:16] = Vproc, [15:0] = Freq */
			recordRef[i + 108] =
				((*(recordTbl + ((NR_FREQ * 3) + i) *
				ARRAY_COL_SIZE + 1) & 0xFFF) << 16) |
				(*(recordTbl + ((NR_FREQ * 3) + i) *
				ARRAY_COL_SIZE) & 0xFFFF);
			cpufreq_ver_dbg("DVFS - recordRef[%d] = 0x%x\n",
				i + 108, recordRef[i + 108]);
			/* CCI [31:16] = clk_div, [15:0] = post_div */
			recordRef[i + 108 + NR_FREQ] =
				((*(recordTbl + ((NR_FREQ * 3) + i) *
				ARRAY_COL_SIZE + 3) & 0xFF) << 16) |
				(*(recordTbl + ((NR_FREQ * 3) + i) *
				ARRAY_COL_SIZE + 2) & 0xFF);
			cpufreq_ver_dbg("DVFS - recordRef[%d] = 0x%x\n",
				i + 108 + NR_FREQ,
				recordRef[i + 108 + NR_FREQ]);
		}
	}
	recordRef[i*2] = 0xffffffff;
	recordRef[i*2+36] = 0xffffffff;
	recordRef[i*2+72] = 0xffffffff;
	recordRef[i*2+108] = 0xffffffff;
	mb(); /* SRAM writing */
}

static int dbg_repo_proc_show(struct seq_file *m, void *v)
{
	int i;
	u32 *repo = m->private;
	char ch;

	for (i = 0; i < DBG_REPO_NUM; i++) {
		if (i >= REPO_I_LOG_S
			&& (i - REPO_I_LOG_S) % ENTRY_EACH_LOG == 0)
			ch = ':';	/* timestamp */
		else
			ch = '.';

		seq_printf(m, "%4d%c%08x%c",
			i, ch, repo[i], i % 4 == 3 ? '\n' : ' ');
	}

	return 0;
}

static int dbg_repo_bak_proc_show(struct seq_file *m, void *v)
{
	int i;
	u32 *repo = m->private;
	char ch;

	for (i = 0; i < DBG_REPO_NUM; i++) {
		if (i >= REPO_I_LOG_S
			&& (i - REPO_I_LOG_S) % ENTRY_EACH_LOG == 0)
			ch = ':';	/* timestamp */
		else
			ch = '.';

		seq_printf(m, "%4d%c%08x%c",
			i, ch, repo[i], i % 4 == 3 ? '\n' : ' ');
	}

	return 0;
}

PROC_FOPS_RO(dbg_repo);
PROC_FOPS_RO(dbg_repo_bak);

static int create_cpuhvfs_debug_fs(void)
{
	int i;
	struct proc_dir_entry *dir = NULL;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
		void *data;
	};

	const struct pentry entries[] = {
		PROC_ENTRY_DATA(dbg_repo),
		PROC_ENTRY_DATA(dbg_repo_bak),
	};

	/* create /proc/cpuhvfs */
	dir = proc_mkdir("cpuhvfs", NULL);
	if (!dir) {
		tag_pr_notice("fail to create /proc/cpuhvfs @ %s()\n",
			__func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create_data(entries[i].name, 0664,
			dir, entries[i].fops, entries[i].data))
			tag_pr_notice("%s(), create /proc/cpuhvfs/%s failed\n",
				__func__,
				    entries[i].name);
	}

	return 0;
}

int cpuhvfs_module_init(void)
{
	int r;

	if (!log_repo) {
		tag_pr_notice("FAILED TO PRE-INIT CPUHVFS\n");
		return -ENODEV;
	}

	r = create_cpuhvfs_debug_fs();
	if (r) {
		tag_pr_notice("FAILED TO CREATE DEBUG FILESYSTEM (%d)\n", r);
		return r;
	}

	/* SW Governor Report */
	spin_lock_init(&cpudvfs_lock);
	Ripi_cpu_dvfs_task = kthread_run(Ripi_cpu_dvfs_thread,
				NULL, "ipi_cpu_dvfs_rtask");

	return 0;
}

static int dvfsp_module_init(void)
{
	int r;

	r = platform_driver_register(&_mt_dvfsp_pdrv);
	if (r)
		tag_pr_notice("fail to register sspm driver @ %s()\n",
			__func__);

	if (!dvfsp_probe_done) {
		tag_pr_notice("FAILED TO PROBE SSPM DEVICE\n");
		return -ENODEV;
	}

	log_repo = csram_base;

	return 0;
}

static void init_cpuhvfs_debug_repo(void)
{
	u32 __iomem *dbg_repo = csram_base;
	int c, repo_i;

	/* backup debug repo for later analysis */
	memcpy_fromio(g_dbg_repo_bak, dbg_repo, DBG_REPO_SIZE);

	dbg_repo[0] = REPO_GUARD0;
	dbg_repo[1] = REPO_GUARD1;
	dbg_repo[2] = REPO_GUARD0;
	dbg_repo[3] = REPO_GUARD1;

/*
 * Clean 0x00100000(CSRAM_BASE) +
 * 0x02a0 count END_SRAM - (DBG_REPO_S + OFFS_DATA_S)
 */
	memset_io((void __iomem *)dbg_repo + DBG_REPO_DATA_S - DBG_REPO_S,
		  0,
		  DBG_REPO_E - DBG_REPO_DATA_S);

	dbg_repo[REPO_I_DATA_S] = REPO_GUARD0;

	for (c = 0; c < NR_MT_CPU_DVFS && c != MT_CPU_DVFS_CCI; c++) {
		repo_i = OFFS_PPM_LIMIT_S / sizeof(u32) + c;
		dbg_repo[repo_i] = (0 << 16) | (NR_FREQ - 1);
	}

	g_dbg_repo = dbg_repo;
}

static int cpuhvfs_pre_module_init(void)
{
	int r;

#ifdef CPU_DVFS_NOT_READY
	return 0;
#endif

	r = dvfsp_module_init();
	if (r) {
		tag_pr_notice("FAILED TO INIT DVFS SSPM (%d)\n", r);
		return r;
	}

	init_cpuhvfs_debug_repo();
	cpuhvfs_pvt_tbl_create();
	cpuhvfs_set_init_ptbl();

	return 0;
}
fs_initcall(cpuhvfs_pre_module_init);

#endif	/* CONFIG_HYBRID_CPU_DVFS */

MODULE_DESCRIPTION("Hybrid CPU DVFS Driver v0.1.1");
