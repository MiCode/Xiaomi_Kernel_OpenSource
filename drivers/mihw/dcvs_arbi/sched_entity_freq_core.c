// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025, Xiaomi Inc. All rights reserved.
 *
 * 基于调度实体的调频控制
 * 通过监控run queue的入队/出队，实现快速响应的CPU频率调节
 */

#undef pr_fmt
#define pr_fmt(fmt) "sched_entity_freq: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpumask.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/cpufreq.h>
#include <linux/cpuidle.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/ktime.h>
#include <linux/topology.h>
#include <linux/pm_qos.h>
#include <linux/limits.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/cpuidle.h>
#include <soc/qcom/dcvs.h>
#include <soc/qcom/pmu_lib.h>
#include "../../../kernel/sched/sched.h"

/* DDR频率联动函数声明 */
static void update_ddr_freq(bool power_save);
static bool all_clusters_power_save(void);

#include "sched_entity_freq.h"

/* 配置参数 */
#define DEFAULT_IDLE_THRESHOLD		1		/* runq <= 2 进入省电 */
#define DEFAULT_ACTIVE_THRESHOLD		2		/* runq >= 3 退出省电 */
#define DEFAULT_CLUSTER_IDLE_THRESHOLD	0		/* 0=自动(CPU数/2)，sysfs写入非0值则覆盖 */
#define DEFAULT_CLUSTER_ACTIVE_THRESHOLD	0		/* 0=自动(CPU数/2+1)，sysfs写入非0值则覆盖 */
#define DEFAULT_SINGLE_TASK_TIMEOUT_NS	6000000ULL	/* 单任务运行6ms后快速恢复 */
#define DEFAULT_FAST_SWITCH_DELAY_MS	5		/* 快速切换延迟(ms) */
#define DEFAULT_MIN_FREQ_KHZ		600000		/* 最低频率600MHz */
#define DEFAULT_MID_FREQ_KHZ		1017600		/* 中间频率~1GHz(分级降频) */
#define DEFAULT_NORMAL_FREQ_KHZ		2500000		/* 正常频率2.5GHz */
#define DEFAULT_DDR_MIN_FREQ_KHZ	100000		/* DDR最低频率100MHz */

/* CPU电源状态 */
enum cpu_power_state {
	CPU_STATE_ACTIVE,
	CPU_STATE_POWER_SAVE
};

/* CPU状态结构 */
struct cpu_state {
	int cpu_id;
	unsigned int cluster_id;
	int runq_counter;		/* run queue计数器 */
	bool idle_mode;			/* 是否进入CPU idle */
	bool skip;			/* 该CPU所属cluster不参与调频(<=2 CPUs) */
	unsigned long last_active_time;	/* 最后活跃时间(jiffies) */
	u64 single_task_start_ns;	/* 单任务开始时间(ns) */
	enum cpu_power_state power_state;	/* CPU电源状态 */
	unsigned int stable_count;	/* 连续无变化tick计数，用于降低稳定态检查频率 */
};

/* Cluster状态结构 */
struct cluster_state {
	unsigned int cluster_id;
	cpumask_t cpumask;		/* Cluster包含的CPU */
	unsigned int active_cpus;	/* 活跃CPU数量(非省电模式) */
	bool power_save_mode;		/* 是否处于省电模式 */
	unsigned int min_freq_khz;	/* Cluster最低频率 */
	unsigned int max_freq_khz;	/* Cluster最高频率 */
	unsigned int cur_freq_khz;	/* Cluster当前频率 */
	bool pending_freq_change;	/* 是否有待处理的频率变更 */
	unsigned int target_freq_khz;	/* 目标频率 */
	unsigned int nr_cpus;		/* Cluster包含的CPU数量(缓存) */
	unsigned int idle_threshold_2;	/* 分级降频第二档阈值(active_cpus<=此值时用mid_freq) */
	struct freq_qos_request qos_max_req;  /* MAX频率约束 */
	bool qos_initialized;		/* QoS是否已初始化 */
	unsigned int saved_max_freq_khz;	/* 进入省电前保存的max freq */
};

/* 全局状态 */
#define MAX_CPUS NR_CPUS
#define MAX_CLUSTERS 8

static struct cpu_state cpu_states[MAX_CPUS];
static struct cluster_state cluster_states[MAX_CLUSTERS];
static unsigned int num_clusters;
static DEFINE_SPINLOCK(state_lock);

/* sysfs是否手动覆盖了cluster阈值 */
bool cluster_idle_threshold_override;
bool cluster_active_threshold_override;

/* 模块引用计数，用于防止模块卸载时hook回调访问已释放内存 */
static atomic_t module_ref_count = ATOMIC_INIT(0);

/* 工作队列 */
static struct workqueue_struct *freq_workqueue;
static struct work_struct cluster_freq_work;

/* DDR调频投票器 */
static struct {
	bool registered;
	char voter_name[32];
	struct dcvs_freq min_vote;
	struct dcvs_freq normal_vote;
	struct dcvs_freq voted_freqs[1]; /* 用于qcom_dcvs_update_votes的数组 */
	bool all_clusters_in_power_save;	/* 所有cluster是否都省电 */
} ddr_voter;

/* 配置参数 - 可通过sysfs运行时修改，不使用__read_mostly */
struct sched_entity_config config = {
	.idle_threshold = DEFAULT_IDLE_THRESHOLD,
	.active_threshold = DEFAULT_ACTIVE_THRESHOLD,
	.cluster_idle_threshold = DEFAULT_CLUSTER_IDLE_THRESHOLD,
	.cluster_active_threshold = DEFAULT_CLUSTER_ACTIVE_THRESHOLD,
	.fast_switch_delay_ms = DEFAULT_FAST_SWITCH_DELAY_MS,
	.min_freq_khz = DEFAULT_MIN_FREQ_KHZ,
	.mid_freq_khz = DEFAULT_MID_FREQ_KHZ,
	.normal_freq_khz = DEFAULT_NORMAL_FREQ_KHZ,
	.ddr_freq_sync = true,
	.ddr_min_freq_khz = DEFAULT_DDR_MIN_FREQ_KHZ,
	.enable = false,
};

/* 保护 config 结构体的 mutex - 用于 sysfs 写入路径 */
DEFINE_MUTEX(config_mutex);

/*
 * 获取cluster降频阈值（active_cpus <= 该值时降频）
 * sysfs写入过则用写入值，否则自动计算为 cluster CPU数 / 2
 */
static unsigned int get_cluster_idle_threshold(unsigned int cluster_id)
{
	if (cluster_idle_threshold_override && config.cluster_idle_threshold > 0)
		return config.cluster_idle_threshold;
	return cluster_states[cluster_id].nr_cpus / 2;
}

/*
 * 获取cluster升频阈值（active_cpus >= 该值时升频）
 * sysfs写入过则用写入值，否则自动计算为 cluster CPU数 / 2 + 1
 */
static unsigned int get_cluster_active_threshold(unsigned int cluster_id)
{
	if (cluster_active_threshold_override && config.cluster_active_threshold > 0)
		return config.cluster_active_threshold;
	return cluster_states[cluster_id].nr_cpus / 2 + 1;
}

/* Fast switch函数 - 立即设置CPU频率 */
static void fast_switch_cluster_freq(int cluster_id, unsigned int freq_khz)
{
	struct cpufreq_policy *policy;
	unsigned int cpu;

	/* 获取cluster中的第一个CPU */
	for_each_cpu(cpu, &cluster_states[cluster_id].cpumask) {
		policy = cpufreq_cpu_get(cpu);
		if (policy) {
			/* 使用cpufreq_driver_fast_switch接口立即设置频率 */
			if (policy->fast_switch_possible) {
				unsigned int target_freq = freq_khz;
				/*
				 * cpufreq_driver_fast_switch 要求在抢占禁用的上下文中调用
				 * 因为它可能直接操作硬件寄存器
				 */
				preempt_disable();
				cpufreq_driver_fast_switch(policy, target_freq);
				preempt_enable();
			}
			cpufreq_cpu_put(policy);
			break; /* 只需要设置一个CPU，其他会自动跟随 */
		}
	}
}

/* 在每个 CPU 上读取 runqueue 状态 */
static void read_runq_on_cpu(void *info)
{
	int cpu = smp_processor_id();
	int *runq_counts = (int *)info;
	struct rq *rq = this_rq();

	/* 在本地 CPU 上安全读取，无需 rq->lock */
	runq_counts[cpu] = rq->nr_running;
}

/* 扫描当前runqueue状态并初始化active_cpus计数器 */
static void init_active_cpus_from_runqueues(void)
{
	int cpu;
	unsigned long irqflags;
	static int runq_counts[MAX_CPUS];

	/* 使用 on_each_cpu 在每个 CPU 上安全地读取 runqueue 状态 */
	on_each_cpu(read_runq_on_cpu, runq_counts, 1);

	/* 在持有 state_lock 的情况下更新状态 */
	spin_lock_irqsave(&state_lock, irqflags);

	for_each_possible_cpu(cpu) {
		unsigned int cluster_id = cpu_states[cpu].cluster_id;
		int runq_count = runq_counts[cpu];

		if (cluster_id >= num_clusters)
			continue;

		cpu_states[cpu].runq_counter = runq_count;

		/* 如果runqueue有任务，标记CPU为active */
		if (runq_count > 0) {
			cpu_states[cpu].power_state = CPU_STATE_ACTIVE;
			cluster_states[cluster_id].active_cpus++;
		} else {
			cpu_states[cpu].power_state = CPU_STATE_POWER_SAVE;
		}

		pr_debug("CPU %d (cluster %u): initial runq_count=%d\n",
			 cpu, cluster_id, runq_count);
	}

	spin_unlock_irqrestore(&state_lock, irqflags);
}

/* 检测cluster拓扑 */
static void detect_clusters(void)
{
	int cpu;
	unsigned int cluster_id;

	for_each_possible_cpu(cpu) {
		cluster_id = topology_cluster_id(cpu);
		if (cluster_id == (unsigned int)-1) {
			/* Fallback to physical package ID if cluster ID not available */
			cluster_id = topology_physical_package_id(cpu);
		}

		if (cluster_id >= MAX_CLUSTERS) {
			pr_warn("Cluster ID %u exceeds maximum supported clusters\n", cluster_id);
			continue;
		}

		if (cluster_id >= num_clusters)
			num_clusters = cluster_id + 1;

		cpumask_set_cpu(cpu, &cluster_states[cluster_id].cpumask);
		cluster_states[cluster_id].cluster_id = cluster_id;

		/* 初始化CPU状态 */
		cpu_states[cpu].cpu_id = cpu;
		cpu_states[cpu].cluster_id = cluster_id;
		cpu_states[cpu].runq_counter = 0;
		cpu_states[cpu].idle_mode = false;
		cpu_states[cpu].last_active_time = jiffies;
		cpu_states[cpu].single_task_start_ns = 0;
		cpu_states[cpu].power_state = CPU_STATE_ACTIVE;
		cpu_states[cpu].stable_count = 0;
	}

	/* 缓存每个cluster的CPU数量，并标记小cluster的CPU为skip */
	for (cluster_id = 0; cluster_id < num_clusters; cluster_id++) {
		unsigned int nr = cpumask_weight(&cluster_states[cluster_id].cpumask);
		cluster_states[cluster_id].nr_cpus = nr;

		if (nr <= 2) {
			for_each_cpu(cpu, &cluster_states[cluster_id].cpumask)
				cpu_states[cpu].skip = true;
			pr_info("Cluster %u (%u CPUs): skipped\n", cluster_id, nr);
		}
	}

	pr_info("Detected %u clusters with %d CPUs\n", num_clusters, num_possible_cpus());
}

/* 初始化cluster频率QoS约束 */
static int init_cluster_qos(unsigned int cluster_id)
{
	struct cluster_state *cluster = &cluster_states[cluster_id];
	int cpu, ret;
	struct cpufreq_policy *policy;

	/* 获取cluster中的第一个CPU的policy */
	cpu = cpumask_first(&cluster->cpumask);
	policy = cpufreq_cpu_get(cpu);
	if (!policy) {
		pr_err("Failed to get cpufreq policy for cluster %u cpu %d\n", cluster_id, cpu);
		return -ENODEV;
	}

	/* 初始化freq_qos约束 */
	ret = freq_qos_add_request(&policy->constraints, &cluster->qos_max_req,
				   FREQ_QOS_MAX, FREQ_QOS_MAX_DEFAULT_VALUE);
	cpufreq_cpu_put(policy);

	if (ret < 0) {
		pr_err("Failed to add freq_qos request for cluster %u: %d\n", cluster_id, ret);
		return ret;
	}

	cluster->qos_initialized = true;
	pr_debug("Cluster %u QoS initialized\n", cluster_id);
	return 0;
}

/* 清理cluster频率QoS约束 */
static void cleanup_cluster_qos(unsigned int cluster_id)
{
	struct cluster_state *cluster = &cluster_states[cluster_id];

	if (cluster->qos_initialized) {
		freq_qos_remove_request(&cluster->qos_max_req);
		cluster->qos_initialized = false;
		pr_debug("Cluster %u QoS cleaned up\n", cluster_id);
	}
}

/* 设置cluster频率约束 - 必须在进程上下文(workqueue)中调用 */
static int set_cluster_freq_constraint(unsigned int cluster_id, unsigned int freq_khz)
{
	struct cluster_state *cluster = &cluster_states[cluster_id];
	int ret;

	if (!cluster->qos_initialized) {
		pr_warn("Cluster %u QoS not initialized\n", cluster_id);
		return -EINVAL;
	}

	ret = freq_qos_update_request(&cluster->qos_max_req, freq_khz);
	if (ret < 0) {
		pr_err("Failed to update freq_qos for cluster %u: %d\n", cluster_id, ret);
		return ret;
	}

	cluster->cur_freq_khz = freq_khz;
	pr_debug("Cluster %u freq constraint set to %u kHz\n", cluster_id, freq_khz);
	return 0;
}

/* 初始化cluster频率信息 */
static void init_cluster_freq_info(void)
{
	unsigned int i;
	int cpu;

	for (i = 0; i < num_clusters; i++) {
		cpu = cpumask_first(&cluster_states[i].cpumask);
		if (cpu >= nr_cpu_ids)
			continue;

		/* 获取cluster的频率范围 */
		cluster_states[i].min_freq_khz = config.min_freq_khz;
		cluster_states[i].max_freq_khz = config.normal_freq_khz;
		cluster_states[i].cur_freq_khz = config.normal_freq_khz;
		cluster_states[i].active_cpus = 0;  /* 初始化为0，稍后根据实际状态设置 */
		cluster_states[i].power_save_mode = false;
		cluster_states[i].pending_freq_change = false;
		cluster_states[i].target_freq_khz = config.normal_freq_khz;
		cluster_states[i].qos_initialized = false;
		cluster_states[i].saved_max_freq_khz = 0;
		/* 分级降频第二档阈值：active_cpus <= nr_cpus*3/4 时用 mid_freq */
		cluster_states[i].idle_threshold_2 = cluster_states[i].nr_cpus * 3 / 4;
	}
}

/*
 * CPU电源状态转换 - 必须持有state_lock调用
 * 统一管理active_cpus计数，防止漂移
 */
static void cpu_set_power_state_locked(int cpu, enum cpu_power_state new_state)
{
	struct cpu_state *state = &cpu_states[cpu];
	struct cluster_state *cluster = &cluster_states[state->cluster_id];

	if (state->power_state == new_state)
		return;

	/* 状态变化时重置稳定计数 */
	cpu_states[cpu].stable_count = 0;

	if (new_state == CPU_STATE_POWER_SAVE) {
		if (cluster->active_cpus > 0)
			cluster->active_cpus--;
	} else {
		cluster->active_cpus++;
	}

	state->power_state = new_state;
}

/*
 * 读取cluster当前的scaling_max_freq - 必须在进程上下文中调用
 */
static unsigned int read_cluster_max_freq(unsigned int cluster_id)
{
	struct cpufreq_policy *policy;
	unsigned int cpu, max_freq = 0;

	cpu = cpumask_first(&cluster_states[cluster_id].cpumask);
	policy = cpufreq_cpu_get(cpu);
	if (policy) {
		max_freq = policy->max;
		cpufreq_cpu_put(policy);
	}
	return max_freq;
}

/*
 * 统一的cluster频率变更工作处理函数 - 在进程上下文中执行
 * 处理 freq_qos 约束更新和 DDR 频率联动
 *
 * 进入省电: 保存当前max_freq → 设QoS上限为min_freq
 * 退出省电: 读当前max_freq，若仍为min_freq则恢复保存的值，否则保留
 */
static void cluster_freq_work_handler(struct work_struct *work)
{
	unsigned int i;
	unsigned long flags;
	bool ddr_power_save;

	/* 处理各cluster的待处理频率变更 */
	for (i = 0; i < num_clusters; i++) {
		struct cluster_state *cluster = &cluster_states[i];
		unsigned int target_freq;
		unsigned int saved_freq;
		bool do_update;
		bool is_enter;

		spin_lock_irqsave(&state_lock, flags);
		do_update = cluster->pending_freq_change;
		target_freq = cluster->target_freq_khz;
		saved_freq = cluster->saved_max_freq_khz;
		is_enter = (target_freq == config.min_freq_khz ||
			    target_freq == config.mid_freq_khz);
		cluster->pending_freq_change = false;
		spin_unlock_irqrestore(&state_lock, flags);

		if (!do_update)
			continue;

		if (is_enter) {
			/* 进入省电: 保存当前max_freq，然后设QoS约束 */
			unsigned int cur_max = read_cluster_max_freq(i);
			if (cur_max > 0) {
				spin_lock_irqsave(&state_lock, flags);
				cluster->saved_max_freq_khz = cur_max;
				spin_unlock_irqrestore(&state_lock, flags);
			}
			set_cluster_freq_constraint(i, target_freq);
		} else {
			/* 退出省电: 检查当前max_freq决定恢复策略 */
			unsigned int cur_max = read_cluster_max_freq(i);
			unsigned int restore_freq;

			spin_lock_irqsave(&state_lock, flags);
			if (cur_max == config.min_freq_khz && cluster->saved_max_freq_khz > 0) {
				/* 仍是我们设的最低频，恢复原值 */
				restore_freq = cluster->saved_max_freq_khz;
			} else {
				/* 别的模块已经改过了，保留当前值不动 */
				restore_freq = 0;
			}
			cluster->saved_max_freq_khz = 0;
			spin_unlock_irqrestore(&state_lock, flags);

			if (restore_freq > 0)
				set_cluster_freq_constraint(i, restore_freq);
			else
				set_cluster_freq_constraint(i, FREQ_QOS_MAX_DEFAULT_VALUE);
		}
	}

	/* 检查并更新DDR频率 */
	spin_lock_irqsave(&state_lock, flags);
	ddr_power_save = all_clusters_power_save();
	if (ddr_power_save != ddr_voter.all_clusters_in_power_save) {
		ddr_voter.all_clusters_in_power_save = ddr_power_save;
		spin_unlock_irqrestore(&state_lock, flags);
		update_ddr_freq(ddr_power_save);
	} else {
		spin_unlock_irqrestore(&state_lock, flags);
	}
}

/* 初始化DDR调频投票器 */
static int init_ddr_voter(void)
{
	int ret;
	u32 min_freq, max_freq;

	if (!config.ddr_freq_sync)
		return 0;

	/* 获取DDR频率范围 */
	ret = qcom_dcvs_hw_minmax_get(DCVS_DDR, &min_freq, &max_freq);
	if (ret) {
		pr_warn("Failed to get DDR frequency range, using defaults\n");
		min_freq = config.ddr_min_freq_khz;
		max_freq = min_freq * 10;
	}

	snprintf(ddr_voter.voter_name, sizeof(ddr_voter.voter_name), "sched_entity_freq");

	ddr_voter.min_vote.hw_type = DCVS_DDR;
	ddr_voter.min_vote.ib = min_freq;
	ddr_voter.min_vote.ab = min_freq;

	ddr_voter.normal_vote.hw_type = DCVS_DDR;
	ddr_voter.normal_vote.ib = max_freq;
	ddr_voter.normal_vote.ab = max_freq;

	/* 初始化voted_freqs数组的hw_type */
	ddr_voter.voted_freqs[0].hw_type = DCVS_DDR;

	ret = qcom_dcvs_register_voter(ddr_voter.voter_name, DCVS_DDR, DCVS_FAST_PATH);
	if (ret) {
		pr_err("Failed to register DDR voter: %d\n", ret);
		return ret;
	}

	ddr_voter.registered = true;
	ddr_voter.all_clusters_in_power_save = false;
	pr_info("DDR voter registered: min=%u, max=%u\n", min_freq, max_freq);
	return 0;
}

/* 清理DDR调频投票器 */
static void cleanup_ddr_voter(void)
{
	if (ddr_voter.registered) {
		qcom_dcvs_unregister_voter(ddr_voter.voter_name, DCVS_DDR, DCVS_FAST_PATH);
		ddr_voter.registered = false;
		pr_info("DDR voter unregistered\n");
	}
}

/* 更新DDR频率 - 在工作队列上下文中调用 */
static void update_ddr_freq(bool power_save)
{
	if (!config.ddr_freq_sync || !ddr_voter.registered)
		return;

	if (power_save) {
		/* 复制min_vote到voted_freqs数组 */
		ddr_voter.voted_freqs[0] = ddr_voter.min_vote;
		qcom_dcvs_update_votes(ddr_voter.voter_name, ddr_voter.voted_freqs, 0x1, DCVS_FAST_PATH);
		pr_debug("DDR: set to minimum frequency %u\n", ddr_voter.min_vote.ib);
	} else {
		/* 复制normal_vote到voted_freqs数组 */
		ddr_voter.voted_freqs[0] = ddr_voter.normal_vote;
		qcom_dcvs_update_votes(ddr_voter.voter_name, ddr_voter.voted_freqs, 0x1, DCVS_FAST_PATH);
		pr_debug("DDR: set to normal frequency %u\n", ddr_voter.normal_vote.ib);
	}
}

/* 检查所有cluster是否都处于省电模式 */
static bool all_clusters_power_save(void)
{
	unsigned int i;

	for (i = 0; i < num_clusters; i++) {
		if (!cluster_states[i].power_save_mode)
			return false;
	}
	return true;
}

/* 调度器tick hook - 定期检查runqueue状态并快速响应 */
static void sched_tick_hook(void *unused, struct rq *rq)
{
	int cpu = rq->cpu;
	unsigned long irqflags;
	int runq_count;
	struct cpu_state *state;
	struct cluster_state *cluster;
	bool need_freq_update = false;
	bool enter_power_save = false;
	unsigned int saved_cluster_id = 0;
	unsigned int saved_target_freq = 0;

	/* 检查模块是否正在运行 */
	if (atomic_read(&module_ref_count) == 0)
		return;

	if (cpu >= MAX_CPUS || !READ_ONCE(config.enable))
		return;

	/* 小cluster(<=2 CPUs)不参与调频控制 - 无锁快速跳过 */
	if (cpu_states[cpu].skip)
		return;

	/* 获取当前runqueue的任务数量 */
	runq_count = rq->nr_running;

	/*
	 * 无锁快速判断：runq未变化 且 不在单任务计时中 → 直接跳过
	 * 用READ_ONCE避免编译器优化，不需要精确一致性（下次tick会再检查）
	 */
	if (READ_ONCE(cpu_states[cpu].runq_counter) == runq_count &&
	    READ_ONCE(cpu_states[cpu].single_task_start_ns) == 0) {
		/*
		 * 方向二：稳定态降频跳过
		 * 连续多次无变化后，每4个tick才检查一次，减少对CPU idle的干扰
		 */
		unsigned int sc = cpu_states[cpu].stable_count;
		if (sc < 255)
			cpu_states[cpu].stable_count = sc + 1;
		if (sc > 8 && (runq_count & 3))
			return;
		return;
	}
	cpu_states[cpu].stable_count = 0;

	spin_lock_irqsave(&state_lock, irqflags);
	state = &cpu_states[cpu];
	cluster = &cluster_states[state->cluster_id];

	/* 更新runq计数器 */
	state->runq_counter = runq_count;

	/*
	 * 单任务运行时间检测：仅当 runq==1 且 CPU处于省电模式时才调ktime
	 * 避免在非省电状态或runq!=1时浪费ktime_get_ns开销
	 */
	if (runq_count == 1 && state->power_state == CPU_STATE_POWER_SAVE) {
		if (state->single_task_start_ns == 0) {
			/* 刚进入单任务状态，开始计时 */
			state->single_task_start_ns = ktime_get_ns();
		} else if (ktime_get_ns() - state->single_task_start_ns > DEFAULT_SINGLE_TASK_TIMEOUT_NS) {
			/* 单任务运行超时，必须升频 */
			cpu_set_power_state_locked(cpu, CPU_STATE_ACTIVE);
			state->single_task_start_ns = 0;
			goto check_cluster_exit;
		}
		goto out;
	}

	/* runq != 1 或非省电状态，清除单任务计时 */
	state->single_task_start_ns = 0;

	/* CPU状态转换 */
	if (runq_count >= config.active_threshold &&
	    state->power_state == CPU_STATE_POWER_SAVE) {
		/* 升频：runq达到active阈值 */
		cpu_set_power_state_locked(cpu, CPU_STATE_ACTIVE);
		goto check_cluster_exit;
	}

	if (runq_count <= config.idle_threshold &&
	    state->power_state == CPU_STATE_ACTIVE) {
		/* 降频：runq低于idle阈值 */
		cpu_set_power_state_locked(cpu, CPU_STATE_POWER_SAVE);

		/*
		 * 方向四：分级降频
		 * active_cpus <= idle_threshold  → min_freq (最激进)
		 * active_cpus <= idle_threshold_2 → mid_freq (温和)
		 * 否则不降频
		 */
		if (!cluster->power_save_mode &&
		    cluster->active_cpus <= get_cluster_idle_threshold(state->cluster_id)) {
			cluster->power_save_mode = true;
			cluster->pending_freq_change = true;
			cluster->target_freq_khz = config.min_freq_khz;
			saved_cluster_id = state->cluster_id;
			saved_target_freq = config.min_freq_khz;
			need_freq_update = true;
			enter_power_save = true;
		} else if (!cluster->power_save_mode &&
			   cluster->active_cpus <= cluster->idle_threshold_2) {
			/* 温和降频到 mid_freq，不标记 cluster 为 power_save */
			cluster->pending_freq_change = true;
			cluster->target_freq_khz = config.mid_freq_khz;
			saved_cluster_id = state->cluster_id;
			saved_target_freq = config.mid_freq_khz;
			need_freq_update = true;
			enter_power_save = true;
		}
	}
	goto out;

check_cluster_exit:
	/* 统一的cluster退出省电检查（单任务超时和runq升频共用） */
	if (cluster->power_save_mode &&
	    cluster->active_cpus >= get_cluster_active_threshold(state->cluster_id)) {
		cluster->power_save_mode = false;
		cluster->pending_freq_change = true;
		cluster->target_freq_khz = FREQ_QOS_MAX_DEFAULT_VALUE;
		saved_cluster_id = state->cluster_id;
		need_freq_update = true;
	}

out:
	spin_unlock_irqrestore(&state_lock, irqflags);

	if (need_freq_update) {
		/* 降频路径: fast_switch立即生效 */
		if (enter_power_save)
			fast_switch_cluster_freq(saved_cluster_id, saved_target_freq);
		/* QoS约束更新 + DDR联动通过workqueue在进程上下文中执行 */
		queue_work(freq_workqueue, &cluster_freq_work);
	}
}


/* 实现头文件声明的接口函数 */
void sched_entity_freq_enable(bool enable)
{
	unsigned long flags;
	unsigned int i;

	/* 先获取 config_mutex 保护 config.enable */
	mutex_lock(&config_mutex);
	config.enable = enable;
	mutex_unlock(&config_mutex);

	spin_lock_irqsave(&state_lock, flags);

	if (!enable) {
		/* 关闭时必须解除所有 QoS 约束并重置状态 */
		int cpu;

		/* 重置所有 CPU 状态 */
		for_each_possible_cpu(cpu) {
			cpu_states[cpu].runq_counter = 0;
			cpu_states[cpu].single_task_start_ns = 0;
			cpu_states[cpu].power_state = CPU_STATE_ACTIVE;
			cpu_states[cpu].stable_count = 0;
		}

		/* 重置所有 cluster 状态 */
		for (i = 0; i < num_clusters; i++) {
			cluster_states[i].power_save_mode = false;
			cluster_states[i].pending_freq_change = true;
			cluster_states[i].target_freq_khz = FREQ_QOS_MAX_DEFAULT_VALUE;
			cluster_states[i].active_cpus = 0;
			cluster_states[i].saved_max_freq_khz = 0;
		}
	}
	spin_unlock_irqrestore(&state_lock, flags);

	if (!enable) {
		/* 先flush工作队列，确保没有pending的工作项 */
		if (freq_workqueue)
			flush_workqueue(freq_workqueue);

		/* 锁外执行 QoS 更新 */
		for (i = 0; i < num_clusters; i++) {
			if (cluster_states[i].qos_initialized)
				freq_qos_update_request(&cluster_states[i].qos_max_req,
							FREQ_QOS_MAX_DEFAULT_VALUE);
		}

		/* 清除pending标志，需要持锁 */
		spin_lock_irqsave(&state_lock, flags);
		for (i = 0; i < num_clusters; i++)
			cluster_states[i].pending_freq_change = false;
		spin_unlock_irqrestore(&state_lock, flags);
	} else {
		/* 启用时重新初始化active_cpus */
		init_active_cpus_from_runqueues();
	}

	pr_info("Feature %s\n", enable ? "enabled" : "disabled");
}
EXPORT_SYMBOL(sched_entity_freq_enable);

bool sched_entity_freq_is_power_save_mode(unsigned int cluster_id)
{
	bool result = false;
	unsigned long flags;

	if (cluster_id < num_clusters) {
		spin_lock_irqsave(&state_lock, flags);
		result = cluster_states[cluster_id].power_save_mode;
		spin_unlock_irqrestore(&state_lock, flags);
	}

	return result;
}
EXPORT_SYMBOL(sched_entity_freq_is_power_save_mode);

unsigned int sched_entity_freq_get_runq_count(int cpu)
{
	unsigned int result = 0;
	unsigned long flags;

	if (cpu >= 0 && cpu < MAX_CPUS) {
		spin_lock_irqsave(&state_lock, flags);
		result = cpu_states[cpu].runq_counter;
		spin_unlock_irqrestore(&state_lock, flags);
	}

	return result;
}
EXPORT_SYMBOL(sched_entity_freq_get_runq_count);

/* 初始化函数 */
static int __init sched_entity_freq_init(void)
{
	int ret;
	unsigned int i;

	pr_info("Initializing sched entity frequency control\n");

	/* 检测cluster拓扑 */
	detect_clusters();

	/* 初始化cluster频率信息 */
	init_cluster_freq_info();

	/* 初始化cluster QoS约束 */
	for (i = 0; i < num_clusters; i++) {
		if (init_cluster_qos(i) < 0) {
			pr_warn("Failed to initialize QoS for cluster %u\n", i);
		}
	}

	/* 根据当前runqueue状态初始化active_cpus计数器 */
	init_active_cpus_from_runqueues();

	/* 初始化DDR调频投票器 */
	ret = init_ddr_voter();
	if (ret) {
		pr_warn("Failed to initialize DDR voter: %d\n", ret);
	}

	/* 初始化工作队列 */
	freq_workqueue = alloc_workqueue("sef_freq_wq", WQ_HIGHPRI, 0);
	if (!freq_workqueue) {
		ret = -ENOMEM;
		goto err_cleanup;
	}
	INIT_WORK(&cluster_freq_work, cluster_freq_work_handler);

	/* 初始化sysfs接口 */
	ret = sched_entity_freq_sysfs_init();
	if (ret) {
		pr_warn("Failed to initialize sysfs interface: %d\n", ret);
	}

	/* 注册调度hooks */
	ret = register_trace_android_vh_scheduler_tick(sched_tick_hook, NULL);
	if (ret) {
		pr_err("Failed to register scheduler tick hook: %d\n", ret);
		goto err_cleanup;
	}

	/* 增加模块引用计数 */
	atomic_inc(&module_ref_count);

	pr_info("Sched entity frequency control initialized successfully\n");
	return 0;
err_cleanup:
	if (freq_workqueue) {
		destroy_workqueue(freq_workqueue);
		freq_workqueue = NULL;
	}
	cleanup_ddr_voter();
	sched_entity_freq_sysfs_exit();
	return ret;
}

/* 退出函数 */
static void __exit sched_entity_freq_exit(void)
{
	unsigned int i;

	pr_info("Exiting sched entity frequency control\n");

	/* 减少模块引用计数，防止hook回调访问已释放内存 */
	atomic_dec(&module_ref_count);

	/* 注销调度hooks */
	unregister_trace_android_vh_scheduler_tick(sched_tick_hook, NULL);

	/* 等待所有CPU退出hook，防止UAF */
	tracepoint_synchronize_unregister();

	/* 清理cluster QoS约束 */
	for (i = 0; i < num_clusters; i++) {
		cleanup_cluster_qos(i);
	}

	/* 清理DDR调频投票器 */
	cleanup_ddr_voter();

	/* 清理工作队列 */
	if (freq_workqueue) {
		flush_workqueue(freq_workqueue);
		destroy_workqueue(freq_workqueue);
		freq_workqueue = NULL;
	}

	/* 清理sysfs接口 */
	sched_entity_freq_sysfs_exit();
}

module_init(sched_entity_freq_init);
module_exit(sched_entity_freq_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Sched Entity Frequency Control");
MODULE_AUTHOR("Xiaomi Inc.");
