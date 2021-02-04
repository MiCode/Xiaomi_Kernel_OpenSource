/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/seq_file.h>
#include <linux/security.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <mt-plat/mtk_perfobserver.h>
#include "pob_qos.h"

#include <mt-plat/fpsgo_common.h>
#include "fpsgo_base.h"
#include "fstb.h"
#include "eara_job_usedext.h"
#include "mtk_upower.h"
#if defined(CONFIG_MTK_VPU_SUPPORT)
#include "vpu_dvfs.h"
#endif
#if defined(CONFIG_MTK_MDLA_SUPPORT)
#include "mdla_dvfs.h"
#endif


#define MAX_DEVICE 2
struct EARA_NN_JOB {
	struct hlist_node hlist;
	__u32 pid;
	__u32 tid;
	__u64 mid;
	__s32 errorno;
	__s32 priority;
	__s32 num_step;

	__s32 *device;
	__s32 *boost;
	__u64 *exec_time;
	__u64 *target_time;

	long long ts;
};

static HLIST_HEAD(eara_nn_jobs);
static long long last_fps_bound_ts;
static int bw_bound;
static int fps_active;
static int ai_bench;

static DEFINE_MUTEX(eara_lock);

static struct dentry *eara_debugfs_dir;
static int get_pwr_tbl_done;
static void get_pwr_tbl(void);
#define NR_FREQ_CPU 16
#define NR_CORE 8
struct eara_cpu_dvfs_info {
	unsigned int freq[NR_FREQ_CPU];
	unsigned long long capacity[NR_FREQ_CPU];
	unsigned int capacity_ratio[NR_FREQ_CPU];
	unsigned int power[NR_FREQ_CPU];
};
struct ppm_cobra_basic_pwr_data {
	unsigned short perf_idx;
	unsigned short power_idx;
};

#if !defined(CONFIG_MTK_VPU_SUPPORT)
#define VPU_OPP_NUM 1
#endif
#if !defined(CONFIG_MTK_MDLA_SUPPORT)
#define MDLA_OPP_NUM 1
#endif
struct eara_vpu_dvfs_info {
	unsigned int freq[VPU_OPP_NUM];
	unsigned int capacity_ratio[VPU_OPP_NUM];
	unsigned int power[VPU_OPP_NUM];
};

struct eara_mdla_dvfs_info {
	unsigned int freq[MDLA_OPP_NUM];
	unsigned int capacity_ratio[MDLA_OPP_NUM];
	unsigned int power[MDLA_OPP_NUM];
};

struct ppm_cobra_data {
	struct ppm_cobra_basic_pwr_data basic_pwr_tbl[NR_CORE][NR_FREQ_CPU];
};
static struct eara_cpu_dvfs_info cpu_dvfs[2], eara_cpu_table;
static struct ppm_cobra_basic_pwr_data thr_cobra_tbl[NR_CORE][NR_FREQ_CPU];
static struct eara_vpu_dvfs_info eara_vpu_table;
static struct eara_mdla_dvfs_info eara_mdla_table;

static struct EARA_NN_JOB *eara_find_entry(int pid,
	unsigned long long mid)
{
	struct EARA_NN_JOB *iter = NULL;

	hlist_for_each_entry(iter, &eara_nn_jobs, hlist)
		if (pid == iter->pid && mid == iter->mid)
			break;

	return iter;
}
static struct EARA_NN_JOB *eara_update_job_collect(int pid, int tid,
	unsigned long long mid, int priority, int num_step,
	__s32 *boost, __s32 *device, __u64 *exec_time)
{
	struct EARA_NN_JOB *iter = NULL;
	int arr_length;
	long long cur_time_us;

	cur_time_us = ktime_to_us(ktime_get());

	hlist_for_each_entry(iter, &eara_nn_jobs, hlist)
		if (pid == iter->pid && mid == iter->mid)
			break;

	arr_length = num_step * MAX_DEVICE;

	if (!iter) {
		struct EARA_NN_JOB *new_nn_job;

		new_nn_job = kmalloc(sizeof(struct EARA_NN_JOB), GFP_KERNEL);
		if (!new_nn_job || !arr_length)
			goto out;

		new_nn_job->pid = pid;
		new_nn_job->tid = tid;
		new_nn_job->mid = mid;
		new_nn_job->priority = priority;
		new_nn_job->num_step = num_step;

		new_nn_job->ts = cur_time_us;

		new_nn_job->target_time =
			kmalloc_array(arr_length, sizeof(__u64), GFP_KERNEL);
		new_nn_job->device =
			kmalloc_array(arr_length, sizeof(__s32), GFP_KERNEL);
		new_nn_job->boost =
			kmalloc_array(arr_length, sizeof(__s32), GFP_KERNEL);
		new_nn_job->exec_time =
			kmalloc_array(arr_length, sizeof(__u64), GFP_KERNEL);

		memcpy(new_nn_job->device, device,
			arr_length * sizeof(__s32));
		memcpy(new_nn_job->boost, boost,
			arr_length * sizeof(__s32));
		memcpy(new_nn_job->exec_time, exec_time,
			arr_length * sizeof(__u64));

		memset(new_nn_job->target_time, 0, arr_length * sizeof(__u64));

		iter = new_nn_job;
		hlist_add_head(&iter->hlist, &eara_nn_jobs);
	} else {

		if (num_step == iter->num_step) {
			memcpy(iter->device, device,
				arr_length * sizeof(__s32));
			memcpy(iter->boost, boost,
				arr_length * sizeof(__s32));
			memcpy(iter->exec_time, exec_time,
				arr_length * sizeof(__u64));
		}

		iter->pid = pid;
		iter->tid = tid;
		iter->mid = mid;
		iter->priority = priority;

		iter->ts = cur_time_us;
	}

out:
	return iter;
}

static void eara_remove_old_entry(void)
{
	struct EARA_NN_JOB *iter;
	struct hlist_node *n;
	long long cur_time_us;
	int count = 0;

	cur_time_us = ktime_to_us(ktime_get());

	hlist_for_each_entry_safe(iter, n, &eara_nn_jobs, hlist) {
		count++;
		if (cur_time_us - iter->ts > 1000000) {
			hlist_del(&iter->hlist);
			kfree(iter->target_time);
			kfree(iter->device);
			kfree(iter->boost);
			kfree(iter->exec_time);
			kfree(iter);
		}
	}

}

#define INVALID_EXEC_TIME_FPS_NOT_CTRL 0xFFFFFFFFFFFFFFFEULL
void eara_nn_nfps_ttime(struct EARA_NN_JOB *nn_job)
{
	int i, j;

	if (!nn_job)
		return;

	for (i = 0; i < nn_job->num_step; i++)
		for (j = 0; j < MAX_DEVICE; j++)
			nn_job->target_time[i * MAX_DEVICE + j] =
				INVALID_EXEC_TIME_FPS_NOT_CTRL;
}

/*
 * no bound: min_exec_time
 * fps or bw bound exec_time increase 25%
 */
void eara_nn_nvip_ttime(struct EARA_NN_JOB *nn_job)
{
	int i, j;
	long long cur_time_us;
	unsigned long long temp;

	if (!nn_job)
		return;

	cur_time_us = ktime_to_us(ktime_get());

	for (i = 0; i < nn_job->num_step; i++)
		for (j = 0; j < MAX_DEVICE; j++) {
			if (cur_time_us - last_fps_bound_ts < 1000000 &&
				bw_bound) {
				/*TODO: 1.25 may use same freq*/
				temp = nn_job->exec_time[i * MAX_DEVICE + j] *
					125ULL;
				do_div(temp, 100ULL);
				nn_job->target_time[i * MAX_DEVICE + j] = temp;
			} else {
				nn_job->target_time[i * MAX_DEVICE + j] = 0;
			}
		}
}

static void eara_set_exec_time(unsigned int pid,
	unsigned long long mid, __u64 v_ttime, __u64 m_ttime)
{
	struct EARA_NN_JOB *nn_job;
	int i, j;

	mutex_lock(&eara_lock);
	fpsgo_systrace_c_fstb(-1000, pid, "pid");
	nn_job = eara_find_entry(pid, mid);

	if (!nn_job)
		goto out;

	for (i = 0; i < nn_job->num_step; i++)
		for (j = 0; j < MAX_DEVICE; j++) {
			switch (nn_job->device[i * MAX_DEVICE + j]) {
			case DEVICE_VPU:
				nn_job->target_time[
					i * MAX_DEVICE + j] = v_ttime;
				break;
			case DEVICE_MDLA:
				nn_job->target_time[
					i * MAX_DEVICE + j] = m_ttime;
				break;
			default:
				break;
			}
		}

	fpsgo_systrace_c_fstb(-1000, v_ttime, "v_ttime");
	fpsgo_systrace_c_fstb(-1000, m_ttime, "m_ttime");
out:
	mutex_unlock(&eara_lock);

}

void fpsgo_fstb2eara_get_exec_time(int pid,
	unsigned long long mid, unsigned long long *t_v,
	unsigned long long *t_m)
{
	struct EARA_NN_JOB *nn_job;
	unsigned long long vpu_time = 0, mdla_time = 0;
	int i, j;

	mutex_lock(&eara_lock);
	nn_job = eara_find_entry(pid, mid);

	if (!nn_job)
		goto out;

	for (i = 0; i < nn_job->num_step; i++)
		for (j = 0; j < MAX_DEVICE; j++) {
			switch (nn_job->device[i * MAX_DEVICE + j]) {
			case DEVICE_VPU:
				vpu_time +=
					nn_job->exec_time[i * MAX_DEVICE + j];
				break;
			case DEVICE_MDLA:
				mdla_time +=
					nn_job->exec_time[i * MAX_DEVICE + j];
				break;
			default:
				break;
			}
		}

out:
	*t_v = vpu_time;
	*t_m = mdla_time;
	mutex_unlock(&eara_lock);
}

void fpsgo_fstb2eara_get_boost_value(int pid, unsigned long long mid,
	int *b_v, int *b_m)
{
	struct EARA_NN_JOB *nn_job;
	int vpu_boost = 0, mdla_boost = 0;
	int vpu_cnt = 0, mdla_cnt = 0;
	int i, j;

	mutex_lock(&eara_lock);
	nn_job = eara_find_entry(pid, mid);

	if (!nn_job)
		goto out;

	for (i = 0; i < nn_job->num_step; i++)
		for (j = 0; j < MAX_DEVICE; j++) {
			switch (nn_job->device[i * MAX_DEVICE + j]) {
			case DEVICE_VPU:
				vpu_boost +=
					nn_job->boost[i * MAX_DEVICE + j];
				vpu_cnt++;
				break;
			case DEVICE_MDLA:
				mdla_boost +=
					nn_job->boost[i * MAX_DEVICE + j];
				mdla_cnt++;
				break;
			default:
				break;
			}
		}

out:
	*b_v = vpu_cnt ? vpu_boost / vpu_cnt : 0;
	*b_m = mdla_cnt ? mdla_boost / mdla_cnt : 0;
	mutex_unlock(&eara_lock);
}

static int eara_job_qos_ind_cb(struct notifier_block *nb,
			unsigned long val, void *data)
{
	switch (val) {
	case POB_QOS_IND_BWBOUND_FREE:
			bw_bound = 0;
			fpsgo_systrace_c_fstb(-500, bw_bound, "bw_bound");
		break;
	case POB_QOS_IND_BWBOUND_CONGESTIVE:
			bw_bound = 0;
			fpsgo_systrace_c_fstb(-500, bw_bound, "bw_bound");
		break;
	case POB_QOS_IND_BWBOUND_FULL:
			bw_bound = 1;
			fpsgo_systrace_c_fstb(-500, bw_bound, "bw_bound");
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block eara_qos_ind_notifier = {
	.notifier_call = eara_job_qos_ind_cb,
};

static inline void eara_job_qos_ind_enable(void)
{
	bw_bound = 0;
	pob_qos_ind_register_client(&eara_qos_ind_notifier);
}

static inline void eara_job_qos_ind_disable(void)
{
	bw_bound = 0;
	pob_qos_ind_unregister_client(&eara_qos_ind_notifier);
}

void fpsgo_ctrl2eara_nn_job_collect(int pid, int tid,
	unsigned long long mid, int hw_type,
	int num_step, __s32 *boost,
	__s32 *device, __u64 *exec_time)
{
	struct EARA_NN_JOB *nn_job;

	mutex_lock(&eara_lock);

	if (!get_pwr_tbl_done)
		get_pwr_tbl();

	if (!get_pwr_tbl_done)
		goto pwr_tbl_err;

	nn_job = eara_update_job_collect(pid, tid, mid, hw_type,
		num_step, boost, device, exec_time);

	eara_remove_old_entry();

	if (!is_fstb_active(1000000LL))
		eara_nn_nfps_ttime(nn_job);
	else if (hw_type == BACKGROUND || hw_type == CRO_FRAME)
		eara_nn_nvip_ttime(nn_job);

pwr_tbl_err:
	kfree(boost);
	kfree(device);
	kfree(exec_time);

	mutex_unlock(&eara_lock);
}

int fpsgo_ctrl2eara_get_nn_priority(unsigned int pid,
	unsigned long long mid)
{
	struct EARA_NN_JOB *nn_job;
	int priority;

	mutex_lock(&eara_lock);
	nn_job = eara_find_entry(pid, mid);

	if (!nn_job || ai_bench)
		priority = PER_FRAME;
	else if (nn_job)
		priority = nn_job->priority;

	fpsgo_systrace_c_fstb(-1000,
		priority, "hw_type");

	mutex_unlock(&eara_lock);

	return priority;
}

void fpsgo_ctrl2eara_get_nn_ttime(unsigned int pid,
	unsigned long long mid, int num_step, __u64 *ttime)
{
	struct EARA_NN_JOB *nn_job;
	int i, j;

	mutex_lock(&eara_lock);
	nn_job = eara_find_entry(pid, mid);

	if (!nn_job || ai_bench) {
		for (i = 0; i < num_step; i++)
			for (j = 0; j < MAX_DEVICE; j++)
				ttime[i * MAX_DEVICE + j] = 0;
	} else if (nn_job) {
		for (i = 0; i < num_step; i++)
			for (j = 0; j < MAX_DEVICE; j++)
				ttime[i * MAX_DEVICE + j] =
					nn_job->target_time[i * MAX_DEVICE + j];
	}

	mutex_unlock(&eara_lock);

}

void fpsgo_fstb2eara_get_jobs_status(int *vpu_cross,
	int *mdla_cross, int *vpu_bg, int *mdla_bg)
{
	int vc = 0, mc = 0, vb = 0, mb = 0;
	struct EARA_NN_JOB *iter = NULL;
	long long cur_time_us;
	int i, j;

	cur_time_us = ktime_to_us(ktime_get());

	mutex_lock(&eara_lock);

	hlist_for_each_entry(iter, &eara_nn_jobs, hlist) {
		if (cur_time_us - iter->ts > 1000000LL)
			continue;

		if (vc && mc && vb && mb)
			break;

		if (iter->priority == BACKGROUND) {
			for (i = 0; i < iter->num_step; i++)
				for (j = 0; j < MAX_DEVICE; j++) {
					switch (iter->device[
						i * MAX_DEVICE + j]) {
					case DEVICE_VPU:
						vb = 1;
						break;
					case DEVICE_MDLA:
						mb = 1;
						break;
					default:
						break;
					}
				}
		} else if (iter->priority == CRO_FRAME) {
			for (i = 0; i < iter->num_step; i++)
				for (j = 0; j < MAX_DEVICE; j++) {
					switch (iter->device[
						i * MAX_DEVICE + j]) {
					case DEVICE_VPU:
						vc = 1;
						break;
					case DEVICE_MDLA:
						mc = 1;
						break;
					default:
						break;
					}
				}
		}
	}

	mutex_unlock(&eara_lock);

	*vpu_cross = vc;
	*mdla_cross = mc;
	*vpu_bg = vb;
	*mdla_bg = mb;
}

void fpsgo_fstb2eara_notify_fps_active(int active)
{
	mutex_lock(&eara_lock);

	if (active != fps_active) {
		if (active)
			eara_job_qos_ind_enable();
		else
			eara_job_qos_ind_disable();
		fps_active = active;
	}

	mutex_unlock(&eara_lock);
}

void fpsgo_fstb2eara_notify_fps_bound(void)
{
	long long cur_time_us;

	cur_time_us = ktime_to_us(ktime_get());
	last_fps_bound_ts = cur_time_us;
}

static unsigned int get_c_opp(unsigned int capacity_ratio)
{
	int opp;

	for (opp = 0; opp < NR_FREQ_CPU &&
		capacity_ratio < eara_cpu_table.capacity_ratio[opp]; opp++)
		;

	if (opp > 0)
		opp -= 1;

	return opp;
}

static unsigned int get_v_opp(unsigned int capacity_ratio)
{
	int opp;

	for (opp = 0; opp < VPU_OPP_NUM &&
		capacity_ratio < eara_vpu_table.capacity_ratio[opp]; opp++)
		;

	if (opp > 0)
		opp -= 1;

	return opp;
}

static unsigned int get_m_opp(unsigned int capacity_ratio)
{
	int opp;

	for (opp = 0; opp < MDLA_OPP_NUM &&
		capacity_ratio < eara_mdla_table.capacity_ratio[opp]; opp++)
		;

	if (opp > 0)
		opp -= 1;

	return opp;
}

/*decrease_xpu_cap
 * Frame exec too quickly
 * We decrease xpu cap to save power
 * input: t_t_t, min_xpu_time, xpu_time, xpu_cap
 * output: set target_xpu_time which total power is better
 *
 * 1. if all xpus at lowest cap ->set and return
 * 2. if cpu(or vpu ...) lowest cap.
 *		decrease other xpu other than cpu that save most power
 * 3. if decrease xpu cause time > total,
 *		choose another to decrease. until reach margin
 *
 * more power saving:
 *	--new power: chose i that maximize
 *		now_power(i)*now_time(i) - new_power(i) * new_time(i)
 */
#define TARGET_TIME_MARGIN 95LL
static int decrease_xpu_cap(long long *t_c_time,
	long long *t_v_time, long long *t_m_time,
	long long t_t_t, long long c_time, long long v_time,
	long long m_time, unsigned int c_cap, unsigned int v_cap,
	unsigned int m_cap)
{
	long long new_cpu_time, new_vpu_time, new_mdla_time;
	long long min_cpu_time, min_vpu_time, min_mdla_time;
	int new_cpu_cap = 0, new_vpu_cap = 0, new_mdla_cap = 0;
	int new_cpu_opp = 0, new_vpu_opp = 0, new_mdla_opp = 0;
	int new_cpu_power = 0, new_vpu_power = 0, new_mdla_power = 0;
	int ori_cpu_power, ori_vpu_power, ori_mdla_power;
	int power_diff_c, power_diff_v, power_diff_m;
	int dbg_cnt = 0;
	unsigned long long temp;

	temp = c_time * eara_cpu_table.capacity_ratio[get_c_opp(c_cap)];
	do_div(temp, 100ULL);
	min_cpu_time = temp;

	temp = v_time * eara_vpu_table.capacity_ratio[get_v_opp(v_cap)];
	do_div(temp, 100ULL);
	min_vpu_time = temp;

	temp = m_time * eara_mdla_table.capacity_ratio[get_m_opp(m_cap)];
	do_div(temp, 100ULL);
	min_mdla_time = temp;

	new_cpu_time = c_time;
	new_vpu_time = v_time;
	new_mdla_time = m_time;
	new_cpu_opp = get_c_opp(c_cap);
	new_vpu_opp =
		v_time ? get_v_opp(v_cap) : VPU_OPP_NUM - 1;
	new_mdla_opp =
		m_time ? get_m_opp(m_cap) : MDLA_OPP_NUM - 1;

	ori_cpu_power = c_time / 1000 *
		eara_cpu_table.power[new_cpu_opp];
	ori_vpu_power = v_time / 1000 *
		eara_vpu_table.power[new_vpu_opp];
	ori_mdla_power = m_time / 1000 *
		eara_mdla_table.power[new_mdla_opp];

	while ((new_cpu_time + new_vpu_time + new_mdla_time * 100) <
			t_t_t *TARGET_TIME_MARGIN &&
			(new_cpu_opp < NR_FREQ_CPU - 1 ||
			 new_vpu_opp < VPU_OPP_NUM - 1 ||
			 new_mdla_opp < MDLA_OPP_NUM - 1)) {

		dbg_cnt++;

		power_diff_c = power_diff_v = power_diff_m = 0;

		if (new_cpu_opp < NR_FREQ_CPU - 1) {
			new_cpu_opp = new_cpu_opp + 1;
			new_cpu_cap =
				eara_cpu_table.capacity_ratio[
				new_cpu_opp];

			temp = min_cpu_time * 100ULL;
			do_div(temp, new_cpu_cap);
			new_cpu_time = temp;

			temp = new_cpu_time * 1000ULL;
			do_div(temp, eara_cpu_table.power[new_cpu_opp]);
			new_cpu_power = temp;

			power_diff_c =
				ori_cpu_power - new_cpu_power;
		}

		if (new_vpu_opp < VPU_OPP_NUM - 1) {
			new_vpu_opp = new_vpu_opp + 1;
			new_vpu_cap =
				eara_vpu_table.capacity_ratio[
				new_vpu_opp];
			new_vpu_time =
				min_vpu_time * 100 / new_vpu_cap;
			new_vpu_power =
				new_vpu_time / 1000 *
				eara_vpu_table.power[new_vpu_opp];
			power_diff_v =
				ori_vpu_power - new_vpu_power;
		}

		if (new_mdla_opp < MDLA_OPP_NUM - 1) {
			new_mdla_opp = new_mdla_opp + 1;
			new_mdla_cap =
				eara_mdla_table.capacity_ratio[
				new_mdla_opp];
			new_mdla_time =
				min_mdla_time * 100 / new_mdla_cap;
			new_mdla_power =
				new_mdla_time / 1000 *
				eara_mdla_table.power[new_mdla_opp];
			power_diff_m =
				ori_mdla_power - new_mdla_power;
		}

		if (power_diff_c &&
				power_diff_c > power_diff_v &&
				power_diff_c > power_diff_m) {

			ori_cpu_power = new_cpu_power;

			if (power_diff_v) {
				new_vpu_opp =
					new_vpu_opp > 0 ?
					new_vpu_opp - 1 : new_vpu_opp;
				new_vpu_cap =
					eara_vpu_table.capacity_ratio[
					new_vpu_opp];
				new_vpu_time =
					min_vpu_time * 100 / new_vpu_cap;
			}

			if (power_diff_m) {
				new_mdla_opp =
					new_mdla_opp > 0 ?
					new_mdla_opp - 1 : new_mdla_opp;
				new_mdla_cap =
					eara_mdla_table.capacity_ratio[
					new_mdla_opp];
				new_mdla_time =
					min_mdla_time * 100 / new_mdla_cap;
			}

		} else if (power_diff_v &&
				power_diff_v > power_diff_c &&
				power_diff_v > power_diff_m) {

			ori_vpu_power = new_vpu_power;

			if (power_diff_c) {
				new_cpu_opp =
					new_cpu_opp > 0 ?
					new_cpu_opp - 1 : new_cpu_opp;
				new_cpu_cap =
					eara_cpu_table.capacity_ratio[
					new_cpu_opp];
				new_cpu_time =
					min_cpu_time * 100 / new_cpu_cap;
			}

			if (power_diff_m) {
				new_mdla_opp =
					new_mdla_opp > 0 ?
					new_mdla_opp - 1 : new_mdla_opp;
				new_mdla_cap =
					eara_mdla_table.capacity_ratio[
					new_mdla_opp];
				new_mdla_time =
					min_mdla_time * 100 / new_mdla_cap;
			}
		} else if (power_diff_m &&
				power_diff_m > power_diff_c &&
				power_diff_m > power_diff_v) {

			ori_mdla_power = new_mdla_power;

			if (power_diff_c) {
				new_cpu_opp =
					new_cpu_opp > 0 ?
					new_cpu_opp - 1 : new_cpu_opp;
				new_cpu_cap =
					eara_cpu_table.capacity_ratio[
					new_cpu_opp];
				new_cpu_time =
					min_cpu_time * 100 / new_cpu_cap;
			}

			if (power_diff_v) {
				new_vpu_opp =
					new_vpu_opp > 0 ?
					new_vpu_opp - 1 : new_vpu_opp;
				new_vpu_cap =
					eara_vpu_table.capacity_ratio[
					new_vpu_opp];
				new_vpu_time =
					min_vpu_time * 100 / new_vpu_cap;
			}
		}
	}

	*t_c_time = new_cpu_time;
	*t_v_time = new_vpu_time;
	*t_m_time = new_mdla_time;

	fpsgo_systrace_c_fstb(-500, (int)min_cpu_time, "min_cpu_time");
	fpsgo_systrace_c_fstb(-500, (int)min_vpu_time, "min_vpu_time");
	fpsgo_systrace_c_fstb(-500, (int)min_mdla_time, "min_mdla_time");
	fpsgo_systrace_c_fstb(-500, (int)new_cpu_power, "new_c_power");
	fpsgo_systrace_c_fstb(-500, (int)new_cpu_time, "new_c_time");
	fpsgo_systrace_c_fstb(-500, (int)new_cpu_opp, "new_c_opp");
	fpsgo_systrace_c_fstb(-500, (int)new_cpu_cap, "new_c_cap");
	fpsgo_systrace_c_fstb(-500, (int)new_vpu_power, "new_v_power");
	fpsgo_systrace_c_fstb(-500, (int)new_vpu_time, "new_v_time");
	fpsgo_systrace_c_fstb(-500, (int)new_vpu_opp, "new_v_opp");
	fpsgo_systrace_c_fstb(-500, (int)new_vpu_cap, "new_v_cap");
	fpsgo_systrace_c_fstb(-500, (int)new_mdla_power, "new_m_power");
	fpsgo_systrace_c_fstb(-500, (int)new_mdla_time, "new_m_time");
	fpsgo_systrace_c_fstb(-500, (int)new_mdla_opp, "new_m_opp");
	fpsgo_systrace_c_fstb(-500, (int)new_mdla_cap, "new_m_cap");

	return 0;
}

/*increase_xpu_cap
 * Frame drops due to not chasing hard enough
 * We increase xpu cap to survive frames
 * This function will not decrease xpu cap
 * input: t_t_t, min_xpu_time, xpu_time, xpu_cap
 * output: set target_xpu_time which total power is better
 *
 * 1. if cpu(or vpu ...) hard work,
 *    raise xpu other than cpu(or vpu ...)
 *    that consume less power to survive.
 * 2. if xpus not hard working,
 *		choose less power consumption xpu
 *		to raise until reach target time
 *
 * less power consume:
 *	--new power: chose i that minimize
 *		new_power(i) * new_time(i) - now_power(i) * now_time(i)
 */
static int increase_xpu_cap(long long *t_c_time, long long *t_v_time,
	long long *t_m_time, long long t_t_t, long long c_time,
	long long v_time, long long m_time, unsigned int c_cap,
	unsigned int v_cap, unsigned int m_cap)
{
	long long new_cpu_time, new_vpu_time, new_mdla_time;
	long long min_cpu_time, min_vpu_time, min_mdla_time;
	int new_cpu_cap = 0, new_vpu_cap = 0, new_mdla_cap = 0;
	int new_cpu_opp = 0, new_vpu_opp = 0, new_mdla_opp = 0;
	int new_cpu_power = 0, new_vpu_power = 0, new_mdla_power = 0;
	int ori_cpu_power, ori_vpu_power, ori_mdla_power;
	int power_diff_c, power_diff_v, power_diff_m;
	int dbg_cnt = 0;
	unsigned long long temp;

	temp = c_time * eara_cpu_table.capacity_ratio[get_c_opp(c_cap)];
	do_div(temp, 100ULL);
	min_cpu_time = temp;

	temp = v_time * eara_vpu_table.capacity_ratio[get_v_opp(v_cap)];
	do_div(temp, 100ULL);
	min_vpu_time = temp;

	temp = m_time * eara_mdla_table.capacity_ratio[get_m_opp(m_cap)];
	do_div(temp, 100ULL);
	min_mdla_time = temp;

	new_cpu_time = c_time;
	new_vpu_time = v_time;
	new_mdla_time = m_time;
	new_cpu_opp = get_c_opp(c_cap);
	new_vpu_opp = v_time ? get_v_opp(v_cap) : 0;
	new_mdla_opp = m_time ? get_m_opp(m_cap) : 0;

	ori_cpu_power = c_time / 1000 *
		eara_cpu_table.power[new_cpu_opp];
	ori_vpu_power = v_time / 1000 *
		eara_vpu_table.power[new_vpu_opp];
	ori_mdla_power = m_time / 1000 *
		eara_mdla_table.power[new_mdla_opp];

	while (new_cpu_time + new_vpu_time + new_mdla_time > t_t_t &&
		(new_cpu_opp > 0 || new_vpu_opp > 0
		|| new_mdla_opp > 0)) {

		dbg_cnt++;

		power_diff_c = power_diff_v = power_diff_m = INT_MAX;

		if (new_cpu_opp > 0) {
			new_cpu_opp = new_cpu_opp - 1;
			new_cpu_cap =
				eara_cpu_table.capacity_ratio[
				new_cpu_opp];
			temp = min_cpu_time * 100ULL;
			do_div(temp, new_cpu_cap);
			new_cpu_time = temp;

			temp = new_cpu_time * 1000ULL;
			do_div(temp, eara_cpu_table.power[new_cpu_opp]);
			new_cpu_power = temp;

			power_diff_c =
				new_cpu_power - ori_cpu_power;
		}

		if (new_vpu_opp > 0) {
			new_vpu_opp = new_vpu_opp - 1;
			new_vpu_cap =
				eara_vpu_table.capacity_ratio[
				new_vpu_opp];
			new_vpu_time =
				min_vpu_time * 100 / new_vpu_cap;
			new_vpu_power =
				new_vpu_time / 1000 *
				eara_vpu_table.power[new_vpu_opp];
			power_diff_v =
				new_vpu_power - ori_vpu_power;
		}

		if (new_mdla_opp > 0) {
			new_mdla_opp = new_mdla_opp - 1;
			new_mdla_cap =
				eara_mdla_table.capacity_ratio[
				new_mdla_opp];
			new_mdla_time =
				min_mdla_time * 100 / new_mdla_cap;
			new_mdla_power =
				new_mdla_time / 1000 *
				eara_mdla_table.power[new_mdla_opp];
			power_diff_m =
				new_mdla_power - ori_mdla_power;
		}

		if (power_diff_c &&
				power_diff_c <= power_diff_v &&
				power_diff_c <= power_diff_m) {

			ori_cpu_power = new_cpu_power;

#if defined(CONFIG_MTK_VPU_SUPPORT)
			if (power_diff_v != INT_MAX) {
				new_vpu_opp++;
				new_vpu_cap =
					eara_vpu_table.capacity_ratio[
					new_vpu_opp];
				new_vpu_time =
					min_vpu_time * 100 / new_vpu_cap;
			}
#endif

#if defined(CONFIG_MTK_MDLA_SUPPORT)
			if (power_diff_m != INT_MAX) {
				new_mdla_opp++;
				new_mdla_cap =
					eara_mdla_table.capacity_ratio[
					new_mdla_opp];
				new_mdla_time =
					min_mdla_time * 100 / new_mdla_cap;
			}
#endif

		} else if (power_diff_v &&
				power_diff_v <= power_diff_c &&
				power_diff_v <= power_diff_m) {

			ori_vpu_power = new_vpu_power;

			if (power_diff_c != INT_MAX) {
				new_cpu_opp++;
				new_cpu_cap =
					eara_cpu_table.capacity_ratio[
					new_cpu_opp];
				new_cpu_time =
					min_cpu_time * 100 / new_cpu_cap;
			}

#if defined(CONFIG_MTK_MDLA_SUPPORT)
			if (power_diff_m != INT_MAX) {
				new_mdla_opp++;
				new_mdla_cap =
					eara_mdla_table.capacity_ratio[
					new_mdla_opp];
				new_mdla_time =
					min_mdla_time * 100 / new_mdla_cap;
			}
#endif
		} else if (power_diff_m &&
				power_diff_m <= power_diff_c &&
				power_diff_m <= power_diff_v) {

			ori_mdla_power = new_mdla_power;

			if (power_diff_c != INT_MAX) {
				new_cpu_opp++;
				new_cpu_cap =
					eara_cpu_table.capacity_ratio[
					new_cpu_opp];
				new_cpu_time =
					min_cpu_time * 100 / new_cpu_cap;
			}

#if defined(CONFIG_MTK_VPU_SUPPORT)
			if (power_diff_v != INT_MAX) {
				new_vpu_opp++;
				new_vpu_cap =
					eara_vpu_table.capacity_ratio[
					new_vpu_opp];
				new_vpu_time =
					min_vpu_time * 100 / new_vpu_cap;
			}
#endif
		}
	}


	*t_c_time = new_cpu_time;
	*t_v_time = new_vpu_time;
	*t_m_time = new_mdla_time;

	fpsgo_systrace_c_fstb(-500, (int)min_cpu_time, "min_cpu_time");
	fpsgo_systrace_c_fstb(-500, (int)min_vpu_time, "min_vpu_time");
	fpsgo_systrace_c_fstb(-500, (int)min_mdla_time, "min_mdla_time");
	fpsgo_systrace_c_fstb(-500, (int)new_cpu_power, "new_c_power");
	fpsgo_systrace_c_fstb(-500, (int)new_cpu_time, "new_c_time");
	fpsgo_systrace_c_fstb(-500, (int)new_cpu_opp, "new_c_opp");
	fpsgo_systrace_c_fstb(-500, (int)new_cpu_cap, "new_c_cap");
	fpsgo_systrace_c_fstb(-500, (int)new_vpu_power, "new_v_power");
	fpsgo_systrace_c_fstb(-500, (int)new_vpu_time, "new_v_time");
	fpsgo_systrace_c_fstb(-500, (int)new_vpu_opp, "new_v_opp");
	fpsgo_systrace_c_fstb(-500, (int)new_vpu_cap, "new_v_cap");
	fpsgo_systrace_c_fstb(-500, (int)new_mdla_power, "new_m_power");
	fpsgo_systrace_c_fstb(-500, (int)new_mdla_time, "new_m_time");
	fpsgo_systrace_c_fstb(-500, (int)new_mdla_opp, "new_m_opp");
	fpsgo_systrace_c_fstb(-500, (int)new_mdla_cap, "new_m_cap");

	return 0;
}

void fpsgo_fstb2eara_optimize_power(unsigned long long mid,
	int tgid, long long *t_c_time, long long t_t_t,
	long long c_time, long long v_time, long long m_time,
	unsigned int c_cap, unsigned int v_cap, unsigned int m_cap)
{
	long long v_c_time, v_v_time, v_m_time;

	/*initial frame*/
	if (!v_time && !m_time) {
		v_c_time = t_t_t;
		v_v_time = 0;
		v_m_time = 0;
		/*drops*/
	} else if (c_time + v_time + m_time > t_t_t) {
		fpsgo_systrace_c_fstb(-500, 1, "pframe_state");
		increase_xpu_cap(&v_c_time, &v_v_time, &v_m_time, t_t_t,
			c_time, v_time, m_time, c_cap, v_cap, m_cap);
	} else if ((c_time + v_time + m_time * 100LL) <
		t_t_t *TARGET_TIME_MARGIN) {
		fpsgo_systrace_c_fstb(-500, 2, "pframe_state");
		decrease_xpu_cap(&v_c_time, &v_v_time, &v_m_time, t_t_t,
			c_time, v_time, m_time, c_cap, v_cap, m_cap);
	} else {
		fpsgo_systrace_c_fstb(-500, 0, "pframe_state");
		v_c_time = c_time;
		v_v_time = v_time;
		v_m_time = m_time;
	}

	/*TODO: v_m_time or boost ratio of each device*/
	eara_set_exec_time(tgid, mid, v_v_time, v_m_time);

	*t_c_time = v_c_time;
}

static void get_pwr_tbl(void)
{
	struct ppm_cobra_data *cobra_tbl = NULL;
	int cluster, opp;
	struct cpumask cluster_cpus;
	int cpu;
	const struct sched_group_energy *core_energy;
	unsigned long long temp = 0ULL;
	unsigned int temp2;
	int cluster_num = 2;

#if defined(CONFIG_MACH_MT6779)
	cobra_tbl = ppm_cobra_pass_tbl();
#endif
	if (!cobra_tbl)
		return;

	memcpy(thr_cobra_tbl, cobra_tbl, sizeof(*cobra_tbl));

	for (cluster = 0; cluster < cluster_num ; cluster++) {
		for (opp = 0; opp < NR_FREQ_CPU; opp++) {
			cpu_dvfs[cluster].freq[opp] =
				mt_cpufreq_get_freq_by_idx(cluster, opp);

			arch_get_cluster_cpus(&cluster_cpus, cluster);

			for_each_cpu(cpu, &cluster_cpus) {
				core_energy = cpu_core_energy(cpu);
				cpu_dvfs[cluster].capacity[opp] =
					core_energy->cap_states[opp].cap;
			}

			temp = cpu_dvfs[cluster].capacity[opp] * 100;
			do_div(temp, 1024);
			temp2 = (unsigned int)temp;
			temp2 = clamp(temp2, 1U, 100U);
			cpu_dvfs[cluster].capacity_ratio[
				NR_FREQ_CPU - 1 - opp] = temp2;
			if (cluster == cluster_num - 1)
				cpu_dvfs[cluster].power[opp] =
				thr_cobra_tbl[NR_CORE - 1][opp].power_idx;
			else
				cpu_dvfs[cluster].power[opp] = 0;
		}
	}

	get_pwr_tbl_done = 1;

	memcpy(&eara_cpu_table, &(cpu_dvfs[cluster_num - 1]),
		sizeof(eara_cpu_table));

#if defined(CONFIG_MTK_VPU_SUPPORT)
	for (opp = 0; opp < VPU_OPP_NUM; opp++) {
		eara_vpu_table.power[opp] =
			vpu_power_table[opp].power;
		eara_vpu_table.freq[opp] =
			get_vpu_opp_to_freq(opp);
		eara_vpu_table.capacity_ratio[opp] =
			get_vpu_opp_to_freq(opp) *
			100 / get_vpu_opp_to_freq(0);
	}
#endif

#if defined(CONFIG_MTK_MDLA_SUPPORT)
	for (opp = 0; opp < MDLA_OPP_NUM; opp++) {
		eara_mdla_table.power[opp] =
			mdla_power_table[opp].power;
		eara_mdla_table.freq[opp] =
			get_mdla_opp_to_freq(opp);
		eara_mdla_table.capacity_ratio[opp] =
			get_mdla_opp_to_freq(opp) *
			100 / get_mdla_opp_to_freq(0);
	}
#endif
}

static int eara_ai_bench_read(struct seq_file *m, void *v)
{
	mutex_lock(&eara_lock);
	seq_printf(m, "%d\n", ai_bench);
	mutex_unlock(&eara_lock);

	return 0;
}

static ssize_t eara_ai_bench_write(struct file *file, const char __user *buffer,
		size_t count, loff_t *data)
{
	int ret = 0;
	int arg;

	mutex_lock(&eara_lock);
	if (!kstrtoint_from_user(buffer, count, 0, &arg)) {
		if (arg != ai_bench)
			ai_bench = arg;
	} else
		ret = -EINVAL;
	mutex_unlock(&eara_lock);

	return (ret < 0) ? ret : count;
}

static int eara_ai_bench_open(struct inode *inode, struct file *file)
{
	return single_open(file, eara_ai_bench_read, NULL);
}

static const struct file_operations eara_ai_bench_fops = {
	.owner = THIS_MODULE,
	.open = eara_ai_bench_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = eara_ai_bench_write,
	.release = single_release,
};


static int eara_status_read(struct seq_file *m, void *v)
{
	long long cur_time_us;
	int i, j;
	struct EARA_NN_JOB *iter = NULL;

	cur_time_us = ktime_to_us(ktime_get());

	mutex_lock(&eara_lock);
	seq_puts(m, "fa\tbw\tfb\tct\n");
	seq_printf(m, "%d\t%d\t%d\t%d\n",
		fps_active, bw_bound, (int)last_fps_bound_ts, (int)cur_time_us);

	hlist_for_each_entry(iter, &eara_nn_jobs, hlist) {
		seq_puts(m, "\npid\ttid\tmid\tpriority\tnum_step\tts\n");
		seq_printf(m, "%d\t%d\t%d\t%d\t%d\t%d\n",
				(int)iter->pid, (int)iter->tid,
				(int)iter->mid, (int)iter->priority,
				(int)iter->num_step, (int)iter->ts);

		seq_puts(m, "\ndevice\tboost\texec_time\ttarget_time\n");
		for (i = 0; i < iter->num_step; i++)
			for (j = 0; j < MAX_DEVICE; j++)
				seq_printf(m, "%d\t%d\t%d\t%d\n",
				(int)iter->device[i * MAX_DEVICE + j],
				(int)iter->boost[i * MAX_DEVICE + j],
				(int)iter->exec_time[i * MAX_DEVICE + j],
				(int)iter->target_time[i * MAX_DEVICE + j]);
	}
	mutex_unlock(&eara_lock);

	return 0;
}

static ssize_t eara_status_write(struct file *file, const char __user *buffer,
		size_t count, loff_t *data)
{
	return count;
}

static int eara_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, eara_status_read, NULL);
}

static const struct file_operations eara_status_fops = {
	.owner = THIS_MODULE,
	.open = eara_status_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = eara_status_write,
	.release = single_release,
};

static int eara_pwr_tbl_read(struct seq_file *m, void *v)
{
	int opp;

	mutex_lock(&eara_lock);

	if (!get_pwr_tbl_done)
		get_pwr_tbl();

	seq_puts(m, "CPU\n");
	for (opp = 0; opp < NR_FREQ_CPU; opp++)
		seq_printf(m, "%d\t%d\t%d\n",
				eara_cpu_table.freq[opp],
				eara_cpu_table.capacity_ratio[opp],
				eara_cpu_table.power[opp]);

	seq_puts(m, "VPU\n");
	for (opp = 0; opp < VPU_OPP_NUM; opp++)
		seq_printf(m, "%d\t%d\t%d\n",
				eara_vpu_table.freq[opp],
				eara_vpu_table.capacity_ratio[opp],
				eara_vpu_table.power[opp]);

	seq_puts(m, "MDLA\n");
	for (opp = 0; opp < MDLA_OPP_NUM; opp++)
		seq_printf(m, "%d\t%d\t%d\n",
				eara_mdla_table.freq[opp],
				eara_mdla_table.capacity_ratio[opp],
				eara_mdla_table.power[opp]);
	mutex_unlock(&eara_lock);

	return 0;
}

static ssize_t eara_pwr_tbl_write(struct file *file, const char __user *buffer,
		size_t count, loff_t *data)
{
	return count;
}

static int eara_pwr_tbl_open(struct inode *inode, struct file *file)
{
	return single_open(file, eara_pwr_tbl_read, NULL);
}

static const struct file_operations eara_pwr_tbl_fops = {
	.owner = THIS_MODULE,
	.open = eara_pwr_tbl_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = eara_pwr_tbl_write,
	.release = single_release,
};

static int __init init_eara_job(void)
{

	if (!fpsgo_debugfs_dir)
		goto err;

	eara_debugfs_dir = debugfs_create_dir("eara",
			fpsgo_debugfs_dir);

	if (!eara_debugfs_dir)
		goto err;

	debugfs_create_file("pwr_tbl",
			0664,
			eara_debugfs_dir,
			NULL,
			&eara_pwr_tbl_fops);

	debugfs_create_file("status",
			0664,
			eara_debugfs_dir,
			NULL,
			&eara_status_fops);

	debugfs_create_file("ai_bench",
			0664,
			eara_debugfs_dir,
			NULL,
			&eara_ai_bench_fops);

	return 0;

err:
	return -1;
}

late_initcall(init_eara_job);
