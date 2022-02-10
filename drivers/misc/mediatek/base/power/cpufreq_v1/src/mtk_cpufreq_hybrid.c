// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
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
//#include <mt-plat/mtk_io.h>
#include <mt-plat/aee.h>
#include <trace/events/power.h>
/* #include <trace/events/mtk_events.h> */
#if !defined(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT)

#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V2)
#include <sspm_ipi_id.h>
#include <sspm_define.h>
#elif defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V1)
#include "v1/sspm_ipi.h"
#include "v1/sspm_ipi_pin.h"
#else
#endif

#else /* CONFIG_MTK_TINYSYS_MCUPM_SUPPORT */

#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V2)
#include <sspm_ipi_id.h>
#include <sspm_define.h>
#elif defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V1)
#include "v1/sspm_ipi.h"
#include "v1/sspm_ipi_pin.h"
#else
#include <linux/soc/mediatek/mtk-mbox.h>
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include <mcupm_ipi_id.h>
#endif

#endif /* CONFIG_MTK_TINYSYS_MCUPM_SUPPORT */

#ifdef MET_DRV
#include <mt-plat/met_drv.h>
#endif

#include "mtk_cpufreq_internal.h"
#include "mtk_cpufreq_platform.h"
#include "mtk_cpufreq_hybrid.h"
#include "mtk_cpufreq_opp_pv_table.h"
#include "mtk_cpufreq_debug.h"
#ifdef DSU_DVFS_ENABLE
#include "swpm_v1/mtk_swpm_interface.h"
#endif

#ifdef CONFIG_MTK_CPU_MSSV
unsigned int __attribute__((weak)) cpumssv_get_state(void) { return 0; }
#endif

#ifdef CONFIG_HYBRID_CPU_DVFS
#include <linux/of_address.h>
u32 *g_dbg_repo;
static u32 dvfsp_probe_done;
void __iomem *log_repo;
static void __iomem *csram_base;
/* static void __iomem *cspm_base; */
#define csram_read(offs)	__raw_readl(csram_base + (offs))
#define csram_write(offs, val)	mt_reg_sync_writel(val, csram_base + (offs))

#define OFFS_TBL_S	0x0010
#define OFFS_TBL_E	0x0250
#define PVT_TBL_SIZE    (OFFS_TBL_E - OFFS_TBL_S)

#define OFFS_CCI_TBL_USER	0x0F94   /* 997 */
#define OFFS_CCI_TOGGLE_BIT	0x0F98   /* 998 */
#define OFFS_CCI_TBL_MODE	0x0F9C   /* 999 */
#define OFFS_CCI_TBL_S		0x0FA0	/* 1000 */
#define OFFS_CCI_TBL_E		0x119C	/* 1127 */
#define OFFS_IMAX_TBL_S      0x11A0   /* 1128 */
#define OFFS_IMAX_TBL_E      0x11C0   /* 1136 */
#define OFFS_IMAX_EN         0x11C4   /* 1137 */
#define OFFS_IMAX_CHANGE     0x11C8   /* 1138 */
#define OFFS_IMAX_THERMAL_INFO     0x11CC   /* 1139 */
#define OFFS_IMAX_THERMAL_CHANGE   0x11D0   /* 1140 */
#define OFFS_IMAX_THERMAL_EN       0x11D4   /* 1141 */
#define OFFS_VPROC_CHANGE_STOP     0x11D8   /* 1142 */
#define OFFS_VPROC_CHANGE_ACK      0x11DC   /* 1143 */
#define PVT_CCI_TBL_SIZE    (OFFS_CCI_TBL_E - OFFS_CCI_TBL_S)
#define PVT_IMAX_TBL_SIZE    (OFFS_IMAX_TBL_E - OFFS_IMAX_TBL_S)
#define MCUCFG_BASE          0x0c530000

#define OFFS_DATA_S	0x02a0
#define OFFS_LOG_S	0x03d0
#define OFFS_LOG_E	(OFFS_LOG_S + DVFS_LOG_NUM * ENTRY_EACH_LOG * 4)

#ifdef REPORT_IDLE_FREQ
#define MAX_LOG_FETCH 80
#else
#define MAX_LOG_FETCH 40
#endif
/* log_box_parsed[MAX_LOG_FETCH] is also used to save last log entry */
static struct cpu_dvfs_log_box log_box_parsed[1 + MAX_LOG_FETCH];
#ifdef DSU_DVFS_ENABLE
unsigned int force_disable;
#endif

void parse_time_log_content(unsigned int time_stamp_l_log,
	unsigned int time_stamp_h_log, unsigned int idx)
{
	if (idx > MAX_LOG_FETCH) {
		tag_pr_notice
		("Error: %s wrong idx %d\n", __func__, idx);
		idx = 0;
	}

	if (time_stamp_h_log == 0 && time_stamp_l_log == 0)
		log_box_parsed[idx].time_stamp = 0;

	log_box_parsed[idx].time_stamp =
		((unsigned long long)time_stamp_h_log << 32) |
		(unsigned long long)(time_stamp_l_log);
}

void parse_log_content(unsigned int *local_buf, unsigned int idx)
{
	struct cpu_dvfs_log *log_box = (struct cpu_dvfs_log *)local_buf;
	struct mt_cpu_dvfs *p;
	int i;

	if (idx > MAX_LOG_FETCH) {
		tag_pr_notice
		("Error: %s wrong idx %d\n", __func__, idx);
		idx = 0;
	}

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
#ifndef CONFIG_MTK_TINYSYS_MCUPM_SUPPORT
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V2)
static DECLARE_COMPLETION(cpuhvfs_setup_done);
#elif defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V1)
struct ipi_action cpufreq_act;
#else /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
#else /* CONFIG_MTK_TINYSYS_MCUPM_SUPPORT */
static DECLARE_COMPLETION(cpuhvfs_setup_done);
#endif /* CONFIG_MTK_TINYSYS_MCUPM_SUPPORT */
uint32_t cpufreq_buf[4];
int cpufreq_ipi_ackdata;

int Ripi_cpu_dvfs_thread(void *data)
{
	int i;
	struct mt_cpu_dvfs *p;
	unsigned long flags;
	uint32_t pwdata[4];
	struct cpufreq_freqs freqs;

	int previous_limit = -1;
	int previous_base = -1;
	unsigned int num_log;
	unsigned int buf[ENTRY_EACH_LOG] = {0};
	unsigned int bk_log_offs;
	unsigned int buf_freq;
	unsigned long long tf_sum, t_diff, avg_f;
	int j;

#ifndef CONFIG_MTK_TINYSYS_MCUPM_SUPPORT
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V2)
#elif defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V1)
	int ret;
#else
#endif
#else
#endif
	memset(pwdata, 0, sizeof(pwdata));
	/* tag_pr_info("CPU DVFS received thread\n"); */
#ifndef CONFIG_MTK_TINYSYS_MCUPM_SUPPORT
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V2)
	wait_for_completion(&cpuhvfs_setup_done);
#elif defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V1)
	cpufreq_act.data = (void *)cpufreq_buf;
	ret = sspm_ipi_recv_registration_ex(IPI_ID_CPU_DVFS, &cpudvfs_lock,
					    &cpufreq_act);

	if (ret != 0) {
		tag_pr_notice
		("Error: ipi_recv_registration CPU DVFS error: %d\n", ret);
		do {
			msleep(1000);
		} while (!kthread_should_stop());
		return (-1);
	}
	/* tag_pr_info("sspm_ipi_recv_registration */
	/*IPI_ID_CPU_DVFS pass!!(%d)\n", ret); */
#else
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
#else /* CONFIG_MTK_TINYSYS_MCUPM_SUPPORT */
	wait_for_completion(&cpuhvfs_setup_done);
#endif
	/* an endless loop in which we are doing our work */
	do {
		/* tag_pr_info("sspm_ipi_recv_wait IPI_ID_CPU_DVFS\n"); */
#ifdef CONFIG_MTK_TINYSYS_MCUPM_SUPPORT
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V2)
		mtk_ipi_recv(&sspm_ipidev, IPIR_C_GPU_DVFS);
#else
		mtk_ipi_recv(&mcupm_ipidev, CH_S_CPU_DVFS);
#endif
#else
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V2)
		mtk_ipi_recv(&sspm_ipidev, IPIR_C_GPU_DVFS);
#elif defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V1)
		sspm_ipi_recv_wait(IPI_ID_CPU_DVFS);
#else
#endif
#endif
		spin_lock_irqsave(&cpudvfs_lock, flags);
		memcpy(pwdata, cpufreq_buf, sizeof(pwdata));
		spin_unlock_irqrestore(&cpudvfs_lock, flags);
		bk_log_offs = pwdata[0];
		num_log = 0;
#ifdef REPORT_IDLE_FREQ
		while ((bk_log_offs != pwdata[1]) &&
			(num_log < MAX_LOG_FETCH)) {
#else
		while (bk_log_offs != pwdata[1]) {
#endif
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

		if (num_log > MAX_LOG_FETCH)
			num_log = MAX_LOG_FETCH;

		cpufreq_lock(flags);
		for_each_cpu_dvfs_only(i, p) {
			if (!p->armpll_is_available)
				continue;
			if (num_log == 1)
				j = log_box_parsed[0].cluster_opp_cfg[
					i].freq_idx;
			else {
				tf_sum = 0;
				for (j = num_log - 1; j >= 1; j--) {
					buf_freq =
					cpu_dvfs_get_freq_by_idx
						(p, log_box_parsed[
						j - 1].cluster_opp_cfg[
						i].freq_idx);
					tf_sum += (
						log_box_parsed[
						j].time_stamp -
						log_box_parsed[
						j-1].time_stamp)
						* (buf_freq/1000);
				}
				if (!num_log)
					t_diff = 1;
				else {
					t_diff = log_box_parsed[
					num_log - 1].time_stamp -
					log_box_parsed[0].time_stamp;
				}
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
				/* p->mt_policy->governor_enabled && */
				(p->mt_policy->cpu < 10) &&
				(p->mt_policy->cpu >= 0)) {
				int cid;

				previous_limit = p->idx_opp_ppm_limit;
				previous_base = p->idx_opp_ppm_base;
				if (num_log) {
					p->idx_opp_ppm_limit =
	(int)(log_box_parsed[num_log - 1].cluster_opp_cfg[i].limit_idx);
					p->idx_opp_ppm_base =
	(int)(log_box_parsed[num_log - 1].cluster_opp_cfg[i].base_idx);
				}

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

#ifdef SINGLE_CLUSTER
				cid = cpufreq_get_cluster_id(
					p->mt_policy->cpu
				);
#else
				cid = arch_get_cluster_id(
					p->mt_policy->cpu
				);
#endif

#ifdef MET_DRV
				if (cid == 0) {
					met_tag_oneshot(0, "sched_dvfs_max_c0",
							p->mt_policy->max);
					met_tag_oneshot(0, "sched_dvfs_min_c0",
							p->mt_policy->min);
				} else if (cid == 1) {
					met_tag_oneshot(0, "sched_dvfs_max_c1",
							p->mt_policy->max);
					met_tag_oneshot(0, "sched_dvfs_min_c1",
							p->mt_policy->min);
				} else if (cid == 2) {
					met_tag_oneshot(0, "sched_dvfs_max_c2",
						p->mt_policy->max);
					met_tag_oneshot(0, "sched_dvfs_min_c2",
						p->mt_policy->min);
				}
#endif

#if defined(CONFIG_MACH_MT6893) || defined(CONFIG_MACH_MT6877) \
	|| defined(CONFIG_MACH_MT6781)
				if (p->mt_policy->cur > p->mt_policy->max) {
					freqs.old = p->mt_policy->cur;
					freqs.new = p->mt_policy->max;
					cpufreq_freq_transition_begin(p->mt_policy, &freqs);
					cpufreq_freq_transition_end(p->mt_policy, &freqs, 0);
					p->idx_opp_tbl = _search_available_freq_idx(p,
						freqs.new, 0);
				} else if (p->mt_policy->cur < p->mt_policy->min) {
					freqs.old = p->mt_policy->cur;
					freqs.new = p->mt_policy->min;
					cpufreq_freq_transition_begin(p->mt_policy, &freqs);
					cpufreq_freq_transition_end(p->mt_policy, &freqs, 0);
					p->idx_opp_tbl = _search_available_freq_idx(p,
						freqs.new, 0);
				} else if (cpu_dvfs_get_freq_by_idx(p, p->idx_opp_tbl) !=
						p->mt_policy->cur) {
					freqs.old = cpu_dvfs_get_freq_by_idx(p, p->idx_opp_tbl);
					freqs.new = p->mt_policy->cur;
					cpufreq_freq_transition_begin(p->mt_policy, &freqs);
					cpufreq_freq_transition_end(p->mt_policy, &freqs, 0);
					p->idx_opp_tbl = _search_available_freq_idx(p,
						freqs.new, 0);
				}
#endif
				trace_cpu_frequency_limits(p->mt_policy);

				/* Policy notification */
				if (p->idx_opp_tbl != j ||
				(p->idx_opp_ppm_limit != previous_limit) ||
				(p->idx_opp_ppm_base != previous_base)) {
#if !defined(CONFIG_MACH_MT6893) && !defined(CONFIG_MACH_MT6877) \
	&& !defined(CONFIG_MACH_MT6781)
					freqs.old = cpu_dvfs_get_cur_freq(p);
					freqs.new =
					cpu_dvfs_get_freq_by_idx(p, j);
					p->idx_opp_tbl = j;
					/* Update frequency change */
					cpufreq_freq_transition_begin(
					p->mt_policy, &freqs);
					cpufreq_freq_transition_end(
					p->mt_policy, &freqs, 0);
#endif
				}
			}
		}
		cpufreq_unlock(flags);
	} while (!kthread_should_stop());

	return 0;
}
#if defined(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT) || \
	(defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V2))
int dvfs_to_mcupm_command(u32 cmd, struct cdvfs_data *cdvfs_d)
{
#define OPT				(0) /* reserve for extensibility */
#define DVFS_D_LEN		(4) /* # of cmd + arg0 + arg1 + ... */
	int ack_data = 0;
	unsigned int ret = 0;

	/* cpufreq_ver("#@# %s(%d) cmd %x\n", __func__, __LINE__, cmd); */
	switch (cmd) {
	case IPI_DVFS_INIT_PTBL:
		cdvfs_d->cmd = cmd;

		cpufreq_ver
		("I'd like to initialize mcupm DVFS, segment code = %d\n",
		cdvfs_d->u.set_fv.arg[0]);
#if defined(USE_SSPM_VER_V2)
		ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_GPU_DVFS,
#else
		ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_CPU_DVFS,
#endif
			IPI_SEND_POLLING, cdvfs_d,
			sizeof(struct cdvfs_data)/MBOX_SLOT_SIZE, 2000);
		if (ret != 0) {
			cpufreq_ver("#@# %s(%d) mcupm_ipi_send_sync ret %d\n",
			__func__, __LINE__, ret);
		} else {
			ret = ack_data;
			cpufreq_ver("#@# %s(%d) cmd(%d) return %d\n",
			__func__, __LINE__, cmd, ret);
		}
		break;

	case IPI_DVFS_INIT:
		cdvfs_d->cmd = cmd;

		cpufreq_ver
		("I'd like to initialize mcupm DVFS, segment code = %d\n",
		cdvfs_d->u.set_fv.arg[0]);
#if defined(USE_SSPM_VER_V2)
		ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_GPU_DVFS,
#else
		ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_CPU_DVFS,
#endif
			IPI_SEND_POLLING, cdvfs_d,
			sizeof(struct cdvfs_data)/MBOX_SLOT_SIZE, 2000);
		if (ret != 0) {
			cpufreq_ver("#@# %s(%d) mcupm_ipi_send_sync ret %d\n",
			__func__, __LINE__, ret);
		} else {
			ret = ack_data;
			cpufreq_ver("#@# %s(%d) cmd(%d) return %d\n",
			__func__, __LINE__, cmd, ret);
		}
		break;

	case IPI_SET_CLUSTER_ON_OFF:
		cdvfs_d->cmd = cmd;

		cpufreq_ver("I'd like to set cluster%d ON/OFF state to %d\n",
			cdvfs_d->u.set_fv.arg[0], cdvfs_d->u.set_fv.arg[1]);

		aee_record_cpu_dvfs_cb(6);
#if defined(USE_SSPM_VER_V2)
		ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_GPU_DVFS,
#else
		ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_CPU_DVFS,
#endif
				IPI_SEND_POLLING, cdvfs_d,
				sizeof(struct cdvfs_data)/MBOX_SLOT_SIZE,
				10000);

		aee_record_cpu_dvfs_cb(7);
		if (ret != 0) {
			tag_pr_notice
			("ret = %d, set cluster%d ON/OFF state to %d\n",
				ret, cdvfs_d->u.set_fv.arg[0],
				cdvfs_d->u.set_fv.arg[1]);
			/*
			 * cpufreq_ver("#@# %s(%d) mcupm_ipi_send_sync ret %d\n",
			 * __func__, __LINE__, ret);
			 */
		} else {
			tag_pr_notice
			("ret = %d, set cluster%d ON/OFF state to %d\n",
			ret, cdvfs_d->u.set_fv.arg[0],
			cdvfs_d->u.set_fv.arg[1]);
			/*
			 * ret = ack_data;
			 * cpufreq_ver("#@# %s(%d) cmd(%d) return %d\n",
			 * __func__, __LINE__, cmd, ret);
			 */
		}
		aee_record_cpu_dvfs_cb(8);
		break;
#ifdef CONFIG_MTK_CPU_MSSV
	case IPI_SET_FREQ:
		cdvfs_d->cmd = cmd;

		cpufreq_ver("I'd like to set cluster%d freq to %d)\n",
			cdvfs_d->u.set_fv.arg[0], cdvfs_d->u.set_fv.arg[1]);
#if defined(USE_SSPM_VER_V2)
		ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_GPU_DVFS,
#else
		ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_CPU_DVFS,
#endif
			IPI_SEND_POLLING, cdvfs_d,
			sizeof(struct cdvfs_data)/MBOX_SLOT_SIZE, 2000);
		if (ret != 0) {
			cpufreq_ver("#@# %s(%d) mcupm_ipi_send_sync ret %d\n",
			__func__, __LINE__, ret);
		} else {
			ret = ack_data;
			cpufreq_ver("#@# %s(%d) cmd(%d) return %d\n",
			__func__, __LINE__, cmd, ret);
		}
		break;

	case IPI_SET_VOLT:
		cdvfs_d->cmd = cmd;

		cpufreq_ver("I'd like to set cluster%d volt to %d)\n",
			cdvfs_d->u.set_fv.arg[0], cdvfs_d->u.set_fv.arg[1]);
#if defined(USE_SSPM_VER_V2)
		ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_GPU_DVFS,
#else
		ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_CPU_DVFS,
#endif
			IPI_SEND_POLLING, cdvfs_d,
			sizeof(struct cdvfs_data)/MBOX_SLOT_SIZE, 2000);
		if (ret != 0) {
			cpufreq_ver("#@# %s(%d) mcupm_ipi_send_sync ret %d\n",
			__func__, __LINE__, ret);
		} else {
			ret = ack_data;
			cpufreq_ver("#@# %s(%d) cmd(%d) return %d\n",
			__func__, __LINE__, cmd, ret);
		}
		break;

	case IPI_GET_VOLT:
		cdvfs_d->cmd = cmd;

		cpufreq_ver("I'd like to get volt from Buck%d\n",
		cdvfs_d->u.set_fv.arg[0]);
#if defined(USE_SSPM_VER_V2)
		ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_GPU_DVFS,
#else
		ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_CPU_DVFS,
#endif
			IPI_SEND_POLLING, cdvfs_d,
			sizeof(struct cdvfs_data)/MBOX_SLOT_SIZE, 2000);
		cpufreq_ver("Get volt = %d\n", ack_data);
		if (ret != 0) {
			cpufreq_ver("#@# %s(%d) mcupm_ipi_send_sync ret %d\n",
			__func__, __LINE__, ret);
		} else {
			ret = ack_data;
			cpufreq_ver("#@# %s(%d) cmd(%d) return %d\n",
			__func__, __LINE__, cmd, ret);
		}
		ret = ack_data;
		break;

	case IPI_GET_FREQ:
		cdvfs_d->cmd = cmd;

		cpufreq_ver("I'd like to get freq from pll%d\n",
		cdvfs_d->u.set_fv.arg[0]);
#if defined(USE_SSPM_VER_V2)
		ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_GPU_DVFS,
#else
		ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_CPU_DVFS,
#endif
			IPI_SEND_POLLING, cdvfs_d,
			sizeof(struct cdvfs_data)/MBOX_SLOT_SIZE, 2000);
		cpufreq_ver("Get freq = %d\n", ack_data);
		if (ret != 0) {
			cpufreq_ver("#@# %s(%d) mcupm_ipi_send_sync ret %d\n",
			__func__, __LINE__, ret);
		} else {
			ret = ack_data;
			cpufreq_ver("#@# %s(%d) cmd(%d) return %d\n",
			__func__, __LINE__, cmd, ret);
		}
		ret = ack_data;
		break;
#endif
	case IPI_TURBO_MODE:
		cdvfs_d->cmd = cmd;

		cpufreq_ver("I'd like to set turbo mode to %d(%d, %d)\n",
			cdvfs_d->u.set_fv.arg[0], cdvfs_d->u.set_fv.arg[1],
			cdvfs_d->u.set_fv.arg[2]);
#if defined(USE_SSPM_VER_V2)
		ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_GPU_DVFS,
#else
		ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_CPU_DVFS,
#endif
			IPI_SEND_POLLING, cdvfs_d,
			sizeof(struct cdvfs_data)/MBOX_SLOT_SIZE, 4000);
		if (ret != 0) {
			cpufreq_ver("#@# %s(%d) mcupm_ipi_send_sync ret %d\n",
			__func__, __LINE__, ret);
		} else {
			ret = ack_data;
			cpufreq_ver("#@# %s(%d) cmd(%d) return %d\n",
			__func__, __LINE__, cmd, ret);
		}
		break;

	case IPI_TIME_PROFILE:
		cdvfs_d->cmd = cmd;

		cpufreq_ver("I'd like to dump time profile data(%d, %d, %d)\n",
			cdvfs_d->u.set_fv.arg[0], cdvfs_d->u.set_fv.arg[1],
			cdvfs_d->u.set_fv.arg[2]);
#if defined(USE_SSPM_VER_V2)
		ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_GPU_DVFS,
#else
		ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_CPU_DVFS,
#endif
			IPI_SEND_POLLING, cdvfs_d,
			sizeof(struct cdvfs_data)/MBOX_SLOT_SIZE, 2000);
		if (ret != 0) {
			cpufreq_ver("#@# %s(%d) mcupm_ipi_send_sync ret %d\n",
			__func__, __LINE__, ret);
		} else {
			ret = ack_data;
			cpufreq_ver("#@# %s(%d) cmd(%d) return %d\n",
			__func__, __LINE__, cmd, ret);
		}
		break;

	case IPI_INIT_VOLT_SETTING:
		cdvfs_d->cmd = cmd;

		cpufreq_ver
		("init MCUPM voltage, cluster = %d, vproc = %d, vsram = %d\n",
			cdvfs_d->u.set_fv.arg[0],
			cdvfs_d->u.set_fv.arg[1],
			cdvfs_d->u.set_fv.arg[2]);
#if defined(USE_SSPM_VER_V2)
		ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_GPU_DVFS,
#else
		ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_CPU_DVFS,
#endif
				IPI_SEND_POLLING, cdvfs_d,
				sizeof(struct cdvfs_data)/MBOX_SLOT_SIZE, 2000);

		if (ret != 0) {
			cpufreq_ver("#@# %s(%d) mcupm_ipi_send_sync ret %d\n",
			__func__, __LINE__, ret);
		} else {
			ret = ack_data;
			cpufreq_ver("#@# %s(%d) cmd(%d) return %d\n",
			__func__, __LINE__, cmd, ret);
		}
		break;

	default:
		cpufreq_ver("#@# %s(%d) cmd(%d) wrong!!!\n",
		__func__, __LINE__, cmd);
		break;
	}

	return ret;
}
#elif defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V1)
static int dvfs_to_spm2_command(u32 cmd, struct cdvfs_data *cdvfs_d)
{
#define OPT				(0) /* reserve for extensibility */
#define DVFS_D_LEN		(4) /* # of cmd + arg0 + arg1 + ... */
	unsigned int len = DVFS_D_LEN;
	int ack_data = 0;
	unsigned int ret = 0;

	/* cpufreq_ver("#@# %s(%d) cmd %x\n", __func__, __LINE__, cmd); */
	switch (cmd) {
	case IPI_DVFS_INIT_PTBL:
		cdvfs_d->cmd = cmd;

		cpufreq_ver
		("I'd like to initialize sspm DVFS, segment code = %d\n",
		cdvfs_d->u.set_fv.arg[0]);

		ret = sspm_ipi_send_sync_new(IPI_ID_CPU_DVFS, IPI_OPT_POLLING,
		cdvfs_d, len, &ack_data, 1);
		if (ret != 0) {
			cpufreq_ver("#@# %s(%d) sspm_ipi_send_sync ret %d\n",
			__func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			cpufreq_ver("#@# %s(%d) cmd(%d) return %d\n",
			__func__, __LINE__, cmd, ret);
		}
		break;

	case IPI_DVFS_INIT:
		cdvfs_d->cmd = cmd;

		cpufreq_ver
		("I'd like to initialize sspm DVFS, segment code = %d\n",
		cdvfs_d->u.set_fv.arg[0]);

		ret = sspm_ipi_send_sync_new(IPI_ID_CPU_DVFS, IPI_OPT_POLLING,
		cdvfs_d, len, &ack_data, 1);
		if (ret != 0) {
			cpufreq_ver("#@# %s(%d) sspm_ipi_send_sync ret %d\n",
			__func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			cpufreq_ver("#@# %s(%d) cmd(%d) return %d\n",
			__func__, __LINE__, cmd, ret);
		}
		break;

	case IPI_SET_CLUSTER_ON_OFF:
		cdvfs_d->cmd = cmd;

		cpufreq_ver("I'd like to set cluster%d ON/OFF state to %d)\n",
			cdvfs_d->u.set_fv.arg[0], cdvfs_d->u.set_fv.arg[1]);

		aee_record_cpu_dvfs_cb(6);
		ret = sspm_ipi_send_sync_new(IPI_ID_CPU_DVFS, IPI_OPT_POLLING,
		cdvfs_d, len, &ack_data, 1);
		aee_record_cpu_dvfs_cb(7);
		if (ret != 0) {
			tag_pr_notice
			("ret = %d, set cluster%d ON/OFF state to %d\n",
				ret, cdvfs_d->u.set_fv.arg[0],
				cdvfs_d->u.set_fv.arg[1]);
		} else if (ack_data < 0) {
			tag_pr_notice
			("ret = %d, set cluster%d ON/OFF state to %d\n",
			ret, cdvfs_d->u.set_fv.arg[0],
			cdvfs_d->u.set_fv.arg[1]);
		}
		aee_record_cpu_dvfs_cb(8);
		break;
	/*
	 *
	 * case IPI_SET_FREQ:
	 * cdvfs_d->cmd = cmd;
	 *
	 * cpufreq_ver("I'd like to set cluster%d freq to %d)\n",
	 * cdvfs_d->u.set_fv.arg[0], cdvfs_d->u.set_fv.arg[1]);
	 *
	 * ret = sspm_ipi_send_sync_new(IPI_ID_CPU_DVFS, IPI_OPT_POLLING,
	 * cdvfs_d, len, &ack_data, 1);
	 * if (ret != 0) {
	 * cpufreq_ver("#@# %s(%d) sspm_ipi_send_sync ret %d\n",
	 * __func__, __LINE__, ret);
	 * } else if (ack_data < 0) {
	 * ret = ack_data;
	 * cpufreq_ver("#@# %s(%d) cmd(%d) return %d\n",
	 * __func__, __LINE__, cmd, ret);
	 * }
	 * break;
	 *
	 * case IPI_SET_VOLT:
	 * cdvfs_d->cmd = cmd;
	 *
	 * cpufreq_ver("I'd like to set cluster%d volt to %d)\n",
	 * cdvfs_d->u.set_fv.arg[0], cdvfs_d->u.set_fv.arg[1]);
	 *
	 * ret = sspm_ipi_send_sync_new(IPI_ID_CPU_DVFS, IPI_OPT_POLLING,
	 * cdvfs_d, len, &ack_data, 1);
	 * if (ret != 0) {
	 * cpufreq_ver("#@# %s(%d) sspm_ipi_send_sync ret %d\n",
	 * __func__, __LINE__, ret);
	 * } else if (ack_data < 0) {
	 * ret = ack_data;
	 * cpufreq_ver("#@# %s(%d) cmd(%d) return %d\n",
	 * __func__, __LINE__, cmd, ret);
	 * }
	 * break;
	 *
	 * case IPI_GET_VOLT:
	 * cdvfs_d->cmd = cmd;
	 *
	 * cpufreq_ver("I'd like to get volt from Buck%d\n",
	 * cdvfs_d->u.set_fv.arg[0]);
	 *
	 * ret = sspm_ipi_send_sync_new(IPI_ID_CPU_DVFS, IPI_OPT_POLLING,
	 * cdvfs_d, len, &ack_data, 1);
	 * cpufreq_ver("Get volt = %d\n", ack_data);
	 * if (ret != 0) {
	 * cpufreq_ver("#@# %s(%d) sspm_ipi_send_sync ret %d\n",
	 * __func__, __LINE__, ret);
	 * } else if (ack_data < 0) {
	 * ret = ack_data;
	 * cpufreq_ver("#@# %s(%d) cmd(%d) return %d\n",
	 * __func__, __LINE__, cmd, ret);
	 * }
	 * ret = ack_data;
	 * break;
	 *
	 * case IPI_GET_FREQ:
	 * cdvfs_d->cmd = cmd;
	 *
	 * cpufreq_ver("I'd like to get freq from pll%d\n",
	 * cdvfs_d->u.set_fv.arg[0]);
	 *
	 * ret = sspm_ipi_send_sync_new(IPI_ID_CPU_DVFS, IPI_OPT_POLLING,
	 * cdvfs_d, len, &ack_data, 1);
	 * cpufreq_ver("Get freq = %d\n", ack_data);
	 * if (ret != 0) {
	 * cpufreq_ver("#@# %s(%d) sspm_ipi_send_sync ret %d\n",
	 * __func__, __LINE__, ret);
	 * } else if (ack_data < 0) {
	 * ret = ack_data;
	 * cpufreq_ver("#@# %s(%d) cmd(%d) return %d\n",
	 * __func__, __LINE__, cmd, ret);
	 * }
	 * ret = ack_data;
	 * break;
	 */
	case IPI_TURBO_MODE:
		cdvfs_d->cmd = cmd;

		cpufreq_ver("I'd like to set turbo mode to %d(%d, %d)\n",
			cdvfs_d->u.set_fv.arg[0], cdvfs_d->u.set_fv.arg[1],
			cdvfs_d->u.set_fv.arg[2]);

		ret = sspm_ipi_send_sync_new(IPI_ID_CPU_DVFS, IPI_OPT_POLLING,
		cdvfs_d, len, &ack_data, 1);
		if (ret != 0) {
			cpufreq_ver("#@# %s(%d) sspm_ipi_send_sync ret %d\n",
			__func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			cpufreq_ver("#@# %s(%d) cmd(%d) return %d\n",
			__func__, __LINE__, cmd, ret);
		}
		break;

	case IPI_TIME_PROFILE:
		cdvfs_d->cmd = cmd;

		cpufreq_ver("I'd like to dump time profile data(%d, %d, %d)\n",
			cdvfs_d->u.set_fv.arg[0], cdvfs_d->u.set_fv.arg[1],
			cdvfs_d->u.set_fv.arg[2]);

		ret = sspm_ipi_send_sync_new(IPI_ID_CPU_DVFS, IPI_OPT_POLLING,
		cdvfs_d, len, &ack_data, 1);
		if (ret != 0) {
			cpufreq_ver("#@# %s(%d) sspm_ipi_send_sync ret %d\n",
			__func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			cpufreq_ver("#@# %s(%d) cmd(%d) return %d\n",
			__func__, __LINE__, cmd, ret);
		}
		break;

	default:
		cpufreq_ver("#@# %s(%d) cmd(%d) wrong!!!\n",
		__func__, __LINE__, cmd);
		break;
	}

	return ret;
}
#else
#endif

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

#define DBG_REPO_CCI_TBL_S		(DBG_REPO_S + OFFS_CCI_TBL_S)
#define DBG_REPO_CCI_TBL_E		(DBG_REPO_S + OFFS_CCI_TBL_E)

#define DBG_REPO_IMAX_TBL_S		(DBG_REPO_S + OFFS_IMAX_TBL_S)
#define DBG_REPO_IMAX_TBL_E		(DBG_REPO_S + OFFS_IMAX_TBL_E)

/* CCI Volt Clamp */
#define OFFS_CCI_VOLT_CLAMP (0x024c)   /* 147 */

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

/* Voltage settle */
#define OFFS_VOLT2_RISE    0x033c   /* 207 */
#define OFFS_VOLT2_FALL    0x0340   /* 208 */
#define OFFS_VOLT1_RISE    0x0344   /* 209 */
#define OFFS_VOLT1_FALL    0x0348   /* 210 */

/* CUR idx */
#define OFFS_CUR_FREQ_S		0x0354	/* 213 */
#define OFFS_CUR_FREQ_E		0x0378	/* 222 */

/* WFI idx */
#define OFFS_WFI_S		0x037c	/* 223 */
#define OFFS_WFI_E		0x03a0	/* 232 */

/* Schedule assist idx */
#define OFFS_SCHED_S		0x03a4	/* 233 */
#define OFFS_SCHED_E		0x03c8	/* 242 */

#define OFFS_HAS_ADVISE_FREQ_S  0x1218  /* 1158 */
#define OFFS_HAS_ADVISE_FREQ_E  0x1220  /* 1160 */
#define CPUDVFS_CLUSTER_ON	0x43434f4e
#define CPUDVFS_CLUSTER_OFF	0x43434f46
/* Schedule assist idx */
#define OFFS_CLUSTER_ONOFF_S	0x1224	/* 1161 */
#define OFFS_CLUSTER_ONOFF_E	0x1248	/* 1170 */

#define ADVI			0x41445649
#define NOAD			0x4E4F4144

static u32 g_dbg_repo_bak[DBG_REPO_NUM];

#ifdef ENABLE_DOE
void srate_doe(void)
{
	struct device_node *node = NULL;
	struct cpudvfs_doe *d = &dvfs_doe;
	int ret;

	node = of_find_compatible_node(NULL, NULL, DVFSP_DT_NODE);

	ret = of_property_read_u32(node, "change_flag", &d->change_flag);

	if (ret)
		tag_pr_info("Cant find change_flag attr\n");

	if (!d->change_flag)
		return;

	/* little up srate */
	ret = of_property_read_u32(node,
			"little-rise-time", &d->lt_rs_t);
	tag_pr_notice("@@~%s DVFS little rise time = %d\n",
			__func__, d->lt_rs_t);
	if (ret)
		csram_write(OFFS_VOLT2_RISE, UP_SRATE);
	else
		csram_write(OFFS_VOLT2_RISE, d->lt_rs_t);
	/* little fall srate */
	ret = of_property_read_u32(node, "little-down-time",
			&d->lt_dw_t);
	tag_pr_notice("@@~%s DVFS little down time = %d\n", __func__,
			d->lt_dw_t);
	if (ret)
		csram_write(OFFS_VOLT2_FALL, DOWN_SRATE);
	else
		csram_write(OFFS_VOLT2_FALL, d->lt_dw_t);
	/* big raise srate */
	ret = of_property_read_u32(node, "big-rise-time",
			&d->bg_rs_t);
	tag_pr_notice("@@~%s DVFS big raise time = %d\n", __func__,
			d->bg_rs_t);
	if (ret)
		csram_write(OFFS_VOLT1_RISE, UP_SRATE);
	else
		csram_write(OFFS_VOLT1_RISE, d->bg_rs_t);

	/* big fall srate */
	ret = of_property_read_u32(node, "big-down-time",
			&d->bg_dw_t);
	tag_pr_notice("@@~%s DVFS big down time = %d\n", __func__,
			d->bg_dw_t);
	if (ret)
		csram_write(OFFS_VOLT1_FALL, DOWN_SRATE);
	else
		csram_write(OFFS_VOLT1_FALL, d->bg_dw_t);

}
#endif

static int _mt_dvfsp_pdrv_probe(struct platform_device *pdev)
{
	/* cspm_base = of_iomap(pdev->dev.of_node, 0); */
#ifdef ENABLE_DOE
	int i, j;
	int ret;
	int flag;
	struct cpudvfs_doe *d = &dvfs_doe;
#endif
	csram_base = of_iomap(pdev->dev.of_node, 1);

#ifdef ENABLE_DOE
	ret = of_property_read_u32(pdev->dev.of_node,
			"change_flag", &d->change_flag);
	if (ret)
		tag_pr_info("Cant find change_flag attr\n");
	if (d->change_flag) {
		for (i = 0; i < NR_MT_CPU_DVFS; i++) {
			flag = 0;
			ret = of_property_read_u32_array(pdev->dev.of_node,
					d->dtsn[i],
			d->dts_opp_tbl[i], ARRAY_SIZE(d->dts_opp_tbl[i]));
			if (ret)
				tag_pr_info("Cant find %s node\n", d->dtsn[i]);
			else {
				for (j = 0; j < ARRAY_SIZE(d->dts_opp_tbl[i]); j++) {
					if (!d->dts_opp_tbl[i][j]) {
						flag = 1;
						tag_pr_info
						("@@ %s contain illegal value\n",
						d->dtsn[i]);
						break;
					}
				}
				if (!flag)
					d->doe_flag |= BIT(i);
			}
			/*
			 * for (j = 0; j < NR_FREQ * ARRAY_COL_SIZE; j++)
			 * tag_pr_info("@@@ %d pvt[%d] = %u\n",
			 * i, j, d->dts_opp_tbl[i][j]);
			 */
		}
	}
#endif

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

int __init cpuhvfs_set_init_ptbl(void)
{
	struct cdvfs_data cdvfs_d;

	/* seg code */
	cdvfs_d.u.set_fv.arg[0] = 0;
#if defined(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT) || \
	defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V2)
	dvfs_to_mcupm_command(IPI_DVFS_INIT_PTBL, &cdvfs_d);
#elif defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V1)
	dvfs_to_spm2_command(IPI_DVFS_INIT_PTBL, &cdvfs_d);
#else
#endif
	return 0;
}

int cpuhvfs_set_turbo_scale(unsigned int turbo_f, unsigned int turbo_v)
{
	csram_write(OFFS_TURBO_FREQ, turbo_f);
	csram_write(OFFS_TURBO_VOLT, turbo_v);

	return 0;
}
#ifdef DFD_WORKAROUND
int cpuhvfs_read_ack(void)
{
	return csram_read(OFFS_VPROC_CHANGE_ACK);
}

void cpuhvfs_write(void)
{
	csram_write(OFFS_VPROC_CHANGE_STOP, 0x50415553);
}
#endif
int cpuhvfs_set_init_sta(void)
{
	struct cdvfs_data cdvfs_d;

	/* seg code */
	cdvfs_d.u.set_fv.arg[0] = 0;
#if defined(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT) || \
	(defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V2))
	cdvfs_d.u.set_fv.arg[0] = _mt_cpufreq_get_cpu_level();
	dvfs_to_mcupm_command(IPI_DVFS_INIT, &cdvfs_d);
	complete_all(&cpuhvfs_setup_done);
#elif defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V1)
	dvfs_to_spm2_command(IPI_DVFS_INIT, &cdvfs_d);
#else
#endif

	return 0;
}

#ifdef INIT_MCUPM_VOLTAGE_SETTING
int cpuhvfs_set_init_volt(void)
{
	struct cdvfs_data cdvfs_d;
	struct mt_cpu_dvfs *p;
	struct buck_ctrl_t *vproc_p;
	struct buck_ctrl_t *vsram_p;
	int j;

	for_each_cpu_dvfs(j, p) {
		vproc_p = id_to_buck_ctrl(p->Vproc_buck_id);
		vsram_p = id_to_buck_ctrl(p->Vsram_buck_id);
		if (vproc_p == NULL || vsram_p == NULL)
			return 0;
		cdvfs_d.u.set_fv.arg[0] = j;
		cdvfs_d.u.set_fv.arg[1] = (p->dvfs_disable_by_suspend) ?
				vproc_p->cur_volt :
				vproc_p->buck_ops->get_cur_volt(vproc_p);
		cdvfs_d.u.set_fv.arg[2] = (p->dvfs_disable_by_suspend) ?
				vsram_p->cur_volt :
				vsram_p->buck_ops->get_cur_volt(vsram_p);
		dvfs_to_mcupm_command(IPI_INIT_VOLT_SETTING, &cdvfs_d);
	}
	return 0;
}
#endif

int cpuhvfs_set_cluster_on_off(int cluster_id, int state)
{
#ifdef ENABLE_CLUSTER_ONOFF_SRAM
	csram_write((OFFS_CLUSTER_ONOFF_S + (cluster_id * 4)),
			state ? CPUDVFS_CLUSTER_ON : CPUDVFS_CLUSTER_OFF);
#else
	struct cdvfs_data cdvfs_d;

	/* Cluster, ON:1/OFF:0 */
	cdvfs_d.u.set_fv.arg[0] = cluster_id;
	cdvfs_d.u.set_fv.arg[1] = state;

	aee_record_cpu_dvfs_cb(5);
#if defined(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT) || \
	(defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V2))
	dvfs_to_mcupm_command(IPI_SET_CLUSTER_ON_OFF, &cdvfs_d);
#elif defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V1)
	dvfs_to_spm2_command(IPI_SET_CLUSTER_ON_OFF, &cdvfs_d);
#else
#endif
#endif
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

void cpuhvfs_write_advise_freq(int cluster_id, unsigned int has_advise_freq)
{
	if (has_advise_freq)
		csram_write((OFFS_HAS_ADVISE_FREQ_S + (cluster_id * 4)),
			ADVI);
	else
		csram_write((OFFS_HAS_ADVISE_FREQ_S + (cluster_id * 4)),
			NOAD);
}

int cpuhvfs_set_dvfs_stress(unsigned int en)
{
	csram_write(OFFS_STRESS_EN, en);
	return 0;
}

int cpuhvfs_get_sched_dvfs_disable(void)
{
	unsigned int disable;
#ifdef CPU_DVFS_NOT_READY
	return 0;
#endif

	disable = csram_read(OFFS_SCHED_DIS);

	return disable;
}

int cpuhvfs_set_sched_dvfs_disable(unsigned int disable)
{
#ifdef CPU_DVFS_NOT_READY
	return 0;
#endif

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
#ifdef CONFIG_MTK_CPU_MSSV
	struct cdvfs_data cdvfs_d;
	int ret = 0;

	if (!cpumssv_get_state())
		return csram_read(OFFS_CUR_VPROC_S + (buck_id * 4));

	/* Cluster, Volt */
	cdvfs_d.u.set_fv.arg[0] = buck_id;

#if defined(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT) || \
	(defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V2))
	dvfs_to_mcupm_command(IPI_GET_VOLT, &cdvfs_d);
#elif defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V1)
	dvfs_to_spm2_command(IPI_GET_VOLT, &cdvfs_d);
#else
#endif

	return ret;
#else
	return csram_read(OFFS_CUR_VPROC_S + (buck_id * 4));
#endif
}

int cpuhvfs_get_freq(int pll_id)
{
#ifdef CONFIG_MTK_CPU_MSSV
	struct cdvfs_data cdvfs_d;
	int ret = 0;

	if (!cpumssv_get_state())
		return 0;

	/* Cluster, Freq */
	cdvfs_d.u.set_fv.arg[0] = pll_id;

#if defined(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT) || \
	(defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V2))
	dvfs_to_mcupm_command(IPI_GET_FREQ, &cdvfs_d);
#elif defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V1)
	dvfs_to_spm2_command(IPI_GET_FREQ, &cdvfs_d);
#else
#endif

	return ret;
#else
	return 0;
#endif
}

int cpuhvfs_set_volt(int cluster_id, unsigned int volt)
{
#ifdef CONFIG_MTK_CPU_MSSV
	struct cdvfs_data cdvfs_d;

	if (!cpumssv_get_state())
		return 0;

	cdvfs_d.u.set_fv.arg[0] = cluster_id;
	cdvfs_d.u.set_fv.arg[1] = volt;

#if defined(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT) || \
	(defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V2))
	dvfs_to_mcupm_command(IPI_SET_VOLT, &cdvfs_d);
#elif defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V1)
	dvfs_to_spm2_command(IPI_SET_VOLT, &cdvfs_d);
#else
#endif
#endif
	return 0;
}

int cpuhvfs_set_freq(int cluster_id, unsigned int freq)
{
#ifdef CONFIG_MTK_CPU_MSSV
	struct cdvfs_data cdvfs_d;

	if (!cpumssv_get_state())
		return 0;

	cdvfs_d.u.set_fv.arg[0] = cluster_id;
	cdvfs_d.u.set_fv.arg[1] = freq;

#if defined(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT) || \
	(defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V2))
	dvfs_to_mcupm_command(IPI_SET_FREQ, &cdvfs_d);
#elif defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) || defined(USE_SSPM_VER_V1)
	dvfs_to_spm2_command(IPI_SET_FREQ, &cdvfs_d);
#else
#endif
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
#if defined(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT) || \
	(defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V2))
	dvfs_to_mcupm_command(IPI_TURBO_MODE, &cdvfs_d);
#elif defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V1)
	dvfs_to_spm2_command(IPI_TURBO_MODE, &cdvfs_d);
#else
#endif
	return 0;
}

int cpuhvfs_get_time_profile(void)
{
#ifdef CPUDVFS_TIME_PROFILE
	struct cdvfs_data cdvfs_d;

	cdvfs_d.u.set_fv.arg[0] = 0;
	cdvfs_d.u.set_fv.arg[1] = 0;
	cdvfs_d.u.set_fv.arg[2] = 0;
#if defined(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT) || \
	(defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V2))
	dvfs_to_mcupm_command(IPI_TIME_PROFILE, &cdvfs_d);
#elif defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V1)
	dvfs_to_spm2_command(IPI_TIME_PROFILE, &cdvfs_d);
#else
#endif
#endif
	return 0;
}

int cpuhvfs_set_iccs_freq(enum mt_cpu_dvfs_id id, unsigned int freq)
{
	struct mt_cpu_dvfs *p;
	int freq_idx = 0;
	unsigned int cluster;

	p = id_to_cpu_dvfs(id);

#ifndef ONE_CLUSTER
#ifdef DVFS_CLUSTER_REMAPPING
	cluster = (id == MT_CPU_DVFS_LL) ? DVFS_CLUSTER_LL :
		(id == MT_CPU_DVFS_L) ? DVFS_CLUSTER_L : DVFS_CLUSTER_B;
#else
	cluster = (id == MT_CPU_DVFS_LL) ? 0 :
		(id == MT_CPU_DVFS_L) ? 1 : 2;
#endif
#else
	cluster = 0;
#endif

	cpufreq_ver("ICCS: cluster = %d, freq = %d\n", cluster, freq);

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

#ifndef ONE_CLUSTER
#ifdef DVFS_CLUSTER_REMAPPING
	cluster = (id == MT_CPU_DVFS_LL) ? DVFS_CLUSTER_LL :
		(id == MT_CPU_DVFS_L) ? DVFS_CLUSTER_L : DVFS_CLUSTER_B;
#else
	cluster = (id == MT_CPU_DVFS_LL) ? 0 :
		(id == MT_CPU_DVFS_L) ? 1 : 2;
#endif
#else
	cluster = 0;
#endif

	cpufreq_ver("sched: cluster = %d, freq = %d\n", cluster, freq);

	counter++;
	if (counter > 255)
		counter = 1;

	/* [3:0] freq_idx, [11:4] counter */
	freq_idx = _search_available_freq_idx(p, freq, 0);

	buf = ((counter << 4) | freq_idx);

	csram_write((OFFS_SCHED_S + (cluster * 4)), buf);

	cpufreq_ver("sched: buf = 0x%x\n", buf);

	return 0;
}

int cpuhvfs_set_set_cci_volt(unsigned int volt)
{
	csram_write(OFFS_CCI_VOLT_CLAMP, volt);

	return 0;
}

u32 *recordRef;
static unsigned int *recordTbl;

#ifdef IMAX_ENABLE
u8 *record_IMAX_Ref;
unsigned char *record_IMAX_Tbl;

unsigned int cpuhvfs_get_imax_state(void)
{
	return csram_read(OFFS_IMAX_EN);
}

void cpuhvfs_update_imax_state(unsigned int state)
{
	csram_write(OFFS_IMAX_EN, state);
	csram_write(OFFS_IMAX_CHANGE, 1);
}

unsigned int cpuhvfs_get_imax_thermal_state(void)
{
	return csram_read(OFFS_IMAX_THERMAL_EN);
}
void cpuhvfs_update_imax_thermal_state(unsigned int state)
{
	csram_write(OFFS_IMAX_THERMAL_EN, state);
}
#endif

#ifdef CCI_MAP_TBL_SUPPORT
u8 *record_CCI_Ref;
unsigned char *record_CCI_Tbl;

unsigned int cpuhvfs_get_cci_result(unsigned int idx_1,
	unsigned int idx_2, unsigned int mode)
{
	if (idx_1 < NR_FREQ && idx_2 < NR_FREQ) {
		if (mode == 0)
			return record_CCI_Ref[(idx_1 * NR_FREQ) + idx_2];
		else
			return record_CCI_Ref[((idx_1 + NR_FREQ) * NR_FREQ)
				+ idx_2];
	} else
		return 0;
}

void cpuhvfs_update_cci_map_tbl(unsigned int idx_1, unsigned int idx_2,
	unsigned char result, unsigned int mode, unsigned int use_id)
{
	if (idx_1 < NR_FREQ && idx_2 < NR_FREQ && mode < NR_CCI_TBL) {
		csram_write(OFFS_CCI_TBL_USER, use_id);
		if (mode == 0)
			record_CCI_Ref[(idx_1 * NR_FREQ) + idx_2] = result;
		else
			record_CCI_Ref[((idx_1 + NR_FREQ) * NR_FREQ)
				+ idx_2] = result;
		csram_write(OFFS_CCI_TOGGLE_BIT, 1);
	}
}

void cpuhvfs_update_cci_mode(unsigned int mode, unsigned int use_id)
{
	/* mode = 0(Normal as 50%) mode = 1(Perf as 70%) */
#ifdef ENABLE_DOE
	struct cpudvfs_doe *d = &dvfs_doe;

	if (!d->state)
		return;
#endif
#ifdef CPU_DVFS_NOT_READY
	return;
#endif

#ifdef DSU_DVFS_ENABLE
	if (use_id == FPS_PERF && force_disable)
		return;
	if (!use_id) {
		if (mode == PERF) {
			//FIXME
			//swpm_pmu_enable(0);
			force_disable = 1;
		} else {
			//FIXME
			//swpm_pmu_enable(1);
			force_disable = 0;
		}
	}
#endif

	if (mode < NR_CCI_TBL) {
		csram_write(OFFS_CCI_TBL_USER, use_id);
		/* mode = 0(Normal as 50%) mode = 1(Perf as 70%) */
		csram_write(OFFS_CCI_TBL_MODE, mode);
		csram_write(OFFS_CCI_TOGGLE_BIT, 1);
	}
}

unsigned int cpuhvfs_get_cci_mode(void)
{
	return csram_read(OFFS_CCI_TBL_MODE);
}
#endif

int cpuhvfs_update_volt(unsigned int cluster_id, unsigned int *volt_tbl,
	char nr_volt_tbl)
{
#ifdef EEM_AP_SIDE
	int i;
	int index;
	int checkFlag = 0;

	for (i = 0; i < nr_volt_tbl; i++) {
		if (volt_tbl[i] == 0)
			checkFlag = 1;
	}

	if (checkFlag == 1)
		return 0;

	for (i = 0; i < nr_volt_tbl; i++) {
		index = (cluster_id * 36) + i;
		recordRef[index] = ((volt_tbl[i] & 0xFFF) << 16) |
			(recordRef[index] & 0xFFFF);
	}
	csram_write((OFFS_EEM_S + (cluster_id * 4)), 1);
#endif

	return 0;
}

#ifdef READ_SRAM_VOLT
unsigned int get_sram_table_volt(unsigned int cluster_id, int idx)
{
	unsigned int volt;
	struct buck_ctrl_t *vproc_p;
	struct mt_cpu_dvfs *p;

	p = id_to_cpu_dvfs(cluster_id);
	if (p == NULL)
		return 0;

	vproc_p = id_to_buck_ctrl(p->Vproc_buck_id);
	if (vproc_p == NULL)
		return 0;

	volt = vproc_p->buck_ops->transfer2volt
		((recordRef[idx + 36 * cluster_id] >> 16) & 0xFFF);

	return volt;
}
#endif

#ifdef ENABLE_DOE
void update_pvt_tbl_by_doe(void)
{
	int i;
	struct cpudvfs_doe *d = &dvfs_doe;

	for (i = 0; i < NR_MT_CPU_DVFS; i++) {
		if ((d->doe_flag >> i) & 1) {
			memcpy(&(*(recordTbl + (NR_FREQ * i) * ARRAY_COL_SIZE)),
				d->dts_opp_tbl[i], sizeof(d->dts_opp_tbl[i]));
			/*
			 * tag_pr_info("@@@[%s] %d update doe_flag = %d\n",
			 * __func__, i, d->doe_flag);
			 */
		}
	}

}
#endif

/* Module driver */
void __init cpuhvfs_pvt_tbl_create(void)
{
	int i;
	unsigned int lv = CPU_LEVEL_0;
#ifdef IMAX_ENABLE
	unsigned int imax_state = IMAX_INIT_STATE;
#ifdef ENABLE_DOE
	struct device_node *node = NULL;
	int ret;
#endif
#endif
#ifdef CCI_MAP_TBL_SUPPORT
	int j;
#endif

	lv = _mt_cpufreq_get_cpu_level();
	recordRef = ioremap_nocache(DBG_REPO_TBL_S, PVT_TBL_SIZE);
	tag_pr_info("DVFS - @(Record)%s----->(%p)\n", __func__, recordRef);
	memset_io((u8 *)recordRef, 0x00, PVT_TBL_SIZE);

	recordTbl = xrecordTbl[lv];
#ifdef ENABLE_DOE
	update_pvt_tbl_by_doe();
	dsb(sy);
#endif
	for (i = 0; i < NR_FREQ; i++) {
		/* Freq, Vproc, post_div, clk_div */
		/* LL [31:16] = Vproc, [15:0] = Freq */
		recordRef[i] =
			((*(recordTbl +
			(i * ARRAY_COL_SIZE) + 1) & 0xFFF) << 16) |
			(*(recordTbl +
			(i * ARRAY_COL_SIZE)) & 0xFFFF);
		cpufreq_ver("DVFS - recordRef[%d] = 0x%x\n", i, recordRef[i]);
		/* LL [31:16] = clk_div, [15:0] = post_div */
		recordRef[i + NR_FREQ] =
			((*(recordTbl +
			(i * ARRAY_COL_SIZE) + 3) & 0xFF) << 16) |
			(*(recordTbl +
			(i * ARRAY_COL_SIZE) + 2) & 0xFF);
		cpufreq_ver("DVFS - recordRef[%d] = 0x%x\n",
				i + NR_FREQ, recordRef[i + NR_FREQ]);

		if (NR_MT_CPU_DVFS > 2) {
			/* L [31:16] = Vproc, [15:0] = Freq */
			recordRef[i + 36] =
				((*(recordTbl +
				((NR_FREQ * 1) + i) * ARRAY_COL_SIZE + 1) &
				0xFFF) << 16) |
				(*(recordTbl +
				((NR_FREQ * 1) + i) * ARRAY_COL_SIZE) &
				0xFFFF);
			cpufreq_ver("DVFS - recordRef[%d] = 0x%x\n",
				i + 36, recordRef[i + 36]);
			/* L [31:16] = clk_div, [15:0] = post_div */
			recordRef[i + 36 + NR_FREQ] =
				((*(recordTbl +
				((NR_FREQ * 1) + i) * ARRAY_COL_SIZE + 3) &
				0xFF) << 16) |
				(*(recordTbl +
				((NR_FREQ * 1) + i) * ARRAY_COL_SIZE + 2) &
				0xFF);
			cpufreq_ver("DVFS - recordRef[%d] = 0x%x\n",
				i + 36 + NR_FREQ, recordRef[i + 36 + NR_FREQ]);
			/* B/CCI [31:16] = Vproc, [15:0] = Freq */
			recordRef[i + 72] =
				((*(recordTbl +
				((NR_FREQ * 2) + i) * ARRAY_COL_SIZE + 1) &
				0xFFF) << 16) |
				(*(recordTbl +
				((NR_FREQ * 2) + i) * ARRAY_COL_SIZE) &	0xFFFF);
			cpufreq_ver("DVFS - recordRef[%d] = 0x%x\n",
				i + 72, recordRef[i + 72]);
			/* B/CCI [31:16] = clk_div, [15:0] = post_div */
			recordRef[i + 72 + NR_FREQ] =
				((*(recordTbl +
				((NR_FREQ * 2) + i) * ARRAY_COL_SIZE + 3) &
				0xFFF) << 16) |
				(*(recordTbl +
				((NR_FREQ * 2) + i) * ARRAY_COL_SIZE + 2) &
				0xFF);
			cpufreq_ver("DVFS - recordRef[%d] = 0x%x\n",
				i + 72 + NR_FREQ, recordRef[i + 72 + NR_FREQ]);
		}

		if (NR_MT_CPU_DVFS > 3) {
			/* CCI [31:16] = Vproc, [15:0] = Freq */
			recordRef[i + 108] =
				((*(recordTbl +
				((NR_FREQ * 3) + i) * ARRAY_COL_SIZE + 1) &
				0xFFF) << 16) |
				(*(recordTbl +
				((NR_FREQ * 3) + i) * ARRAY_COL_SIZE) &
				0xFFFF);
			cpufreq_ver("DVFS - recordRef[%d] = 0x%x\n",
				i + 108, recordRef[i + 108]);
			/* CCI [31:16] = clk_div, [15:0] = post_div */
			recordRef[i + 108 + NR_FREQ] =
				((*(recordTbl +
				((NR_FREQ * 3) + i) * ARRAY_COL_SIZE + 3) &
				0xFF) << 16) |
				(*(recordTbl +
				((NR_FREQ * 3) + i) * ARRAY_COL_SIZE + 2) &
				0xFF);
			cpufreq_ver("DVFS - recordRef[%d] = 0x%x\n",
				i + 108 + NR_FREQ, recordRef[i + 108 + NR_FREQ]
			);
		}
	}
	recordRef[i*2] = 0xffffffff;
	recordRef[i*2+36] = 0xffffffff;
	recordRef[i*2+72] = 0xffffffff;
	recordRef[i*2+108] = 0xffffffff;
	mb(); /* SRAM writing */

#ifdef CCI_MAP_TBL_SUPPORT
	record_CCI_Ref = ioremap_nocache(DBG_REPO_CCI_TBL_S, PVT_CCI_TBL_SIZE);
	tag_pr_info("DVFS - @(Record)%s----->(%p)\n", __func__, record_CCI_Ref);
	memset_io((u8 *)record_CCI_Ref, 0x00, PVT_CCI_TBL_SIZE);

	record_CCI_Tbl = xrecord_CCI_Tbl[lv];

	for (i = 0; i < NR_FREQ * NR_CCI_TBL; i++) {
		for (j = 0; j < NR_FREQ; j++) {
			record_CCI_Ref[(i * NR_FREQ) + j] =
				*(record_CCI_Tbl + (i * NR_FREQ) + j);
			cpufreq_ver("%d ", record_CCI_Ref[(i * NR_FREQ) + j]);
		}
		cpufreq_ver("\n");
	}
	mb(); /* SRAM writing */
#endif

#ifdef IMAX_ENABLE
#ifdef ENABLE_DOE
	node = of_find_compatible_node(NULL, NULL, DVFSP_DT_NODE);
	ret = of_property_read_u32(node, "imax_state", &imax_state);
	if (ret)
		tag_pr_info(" %s Cant find imax state node\n", __func__);
#endif
	record_IMAX_Ref = ioremap_nocache(DBG_REPO_IMAX_TBL_S,
			PVT_IMAX_TBL_SIZE);
	tag_pr_info("DVFS - @(IMAX Record)%s----->(%p)\n", __func__,
			record_IMAX_Ref);
	memset_io((u8 *)record_IMAX_Ref, 0x00, PVT_IMAX_TBL_SIZE);

	record_IMAX_Tbl = xrecord_IMAX_Tbl[lv];
	for (i = 0; i < (NR_FREQ + 1) * IMAX_EN_RATIO_TBL_NUM; i++)
		record_IMAX_Ref[i] = *(record_IMAX_Tbl + i);
	csram_write(OFFS_IMAX_EN, imax_state);
	csram_write(OFFS_IMAX_THERMAL_EN, 1);
	mb(); /* SRAM writing */
#endif
}

static int dbg_repo_proc_show(struct seq_file *m, void *v)
{
	int i;
	u32 *repo = m->private;
	char ch;

	for (i = 0; i < DBG_REPO_NUM; i++) {
		if (i >= REPO_I_LOG_S && (i - REPO_I_LOG_S) %
						ENTRY_EACH_LOG == 0)
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
		if (i >= REPO_I_LOG_S && (i - REPO_I_LOG_S) %
						ENTRY_EACH_LOG == 0)
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
						__func__, entries[i].name);
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
	Ripi_cpu_dvfs_task = kthread_run(Ripi_cpu_dvfs_thread, NULL,
						"ipi_cpu_dvfs_rtask");

	return 0;
}

static int __init dvfsp_module_init(void)
{
	int r;
	void __iomem *dvfs_node;

	r = platform_driver_register(&_mt_dvfsp_pdrv);
	if (r)
		tag_pr_notice("fail to register sspm driver @ %s()\n",
			      __func__);

	if (!dvfsp_probe_done) {
		dvfs_node = of_find_compatible_node(NULL, NULL, DVFSP_DT_NODE);
		if (dvfs_node) {
			csram_base = of_iomap(dvfs_node, 1);
			dvfsp_probe_done = 1;
		} else
			return -ENODEV;
	}

	log_repo = csram_base;

	return 0;
}

static void __init init_cpuhvfs_debug_repo(void)
{
	u32 __iomem *dbg_repo = csram_base;
	int c, repo_i;

	/* backup debug repo for later analysis */
	memcpy_fromio(g_dbg_repo_bak, dbg_repo, DBG_REPO_SIZE);

	dbg_repo[0] = REPO_GUARD0;
	dbg_repo[1] = REPO_GUARD1;
	dbg_repo[2] = REPO_GUARD0;
	dbg_repo[3] = REPO_GUARD1;

	/* Clean 0x00100000(CSRAM_BASE) + 0x02a0 */
	/*count END_SRAM - (DBG_REPO_S + OFFS_DATA_S)*/
	memset_io((void __iomem *)dbg_repo + DBG_REPO_DATA_S - DBG_REPO_S,
		  0,
		  DBG_REPO_E - DBG_REPO_DATA_S);

	dbg_repo[REPO_I_DATA_S] = REPO_GUARD0;

#ifndef ONE_CLUSTER
	for (c = 0; c < NR_MT_CPU_DVFS && c != MT_CPU_DVFS_CCI; c++) {
#else
	for (c = 0; c < NR_MT_CPU_DVFS; c++) {
#endif
		repo_i = OFFS_PPM_LIMIT_S / sizeof(u32) + c;
		dbg_repo[repo_i] = (0 << 16) | (NR_FREQ - 1);
	}

	g_dbg_repo = dbg_repo;
}

static int __init cpuhvfs_pre_module_init(void)
{
	int r;

#ifdef CPU_DVFS_NOT_READY
	return 0;
#endif

	r = dvfsp_module_init();
	if (r) {
		tag_pr_notice("FAILED TO INIT DVFS MODULE (%d)\n", r);
		return r;
	}

#if defined(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT) \
	|| (defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) \
	&& defined(USE_SSPM_VER_V2))
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_SSPM_VER_V2)
	ret = mtk_ipi_register(&sspm_ipidev, IPIS_C_GPU_DVFS, NULL, NULL,
		(void *) &cpufreq_ipi_ackdata);
	if (ret)
		return -1;

	ret = mtk_ipi_register(&sspm_ipidev, IPIR_C_GPU_DVFS, NULL, NULL,
		(void *) &cpufreq_buf);
	if (ret)
		return -1;
#else
	tag_pr_notice("yeet %s:%d\n", __func__, __LINE__);

	mtk_ipi_register(&mcupm_ipidev, CH_S_CPU_DVFS, NULL, NULL,
		(void *) &cpufreq_buf);
#endif
#else
#endif

	init_cpuhvfs_debug_repo();
	cpuhvfs_pvt_tbl_create();
	cpuhvfs_set_init_ptbl();

	return 0;
}
fs_initcall(cpuhvfs_pre_module_init);

#endif	/* CONFIG_HYBRID_CPU_DVFS */

MODULE_DESCRIPTION("Hybrid CPU DVFS Driver v0.1.1");
