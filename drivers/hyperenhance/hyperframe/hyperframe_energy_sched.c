// MIUI ADD: Performance_FramePredictBoost
#include "hyperframe_energy_sched.h"

#define MAX_COUNT_FREQ 40
#define MAX_COUNT_POLICY 4

struct freq_qos_request *req_min2 = NULL;
struct freq_qos_request *req_max2 = NULL;
struct freq_qos_request *req_max3 = NULL;
struct freq_qos_request *req_max7 = NULL;
struct freq_qos_request *req_min5 = NULL;
struct freq_qos_request *req_min_super = NULL;
unsigned int min_freq = 0;
extern atomic_t g_run_siginal;

static DEFINE_PER_CPU(struct freq_qos_request, qos_max_req);

unsigned int cpu0_freq[] = {
		364800,460800,556800,672000,787200,902400,1017600,1132800,1248000,1344000,
		1459200,1574400,1689600,1804800,1920000,2035200,2150400,2265600}; //N8

unsigned int cpu2_freq[] = {
		499200,614400,729600,844800,960000,1075200,1190400,1286400,1401600,1497600,
		1612800,1708800,1824000,1920000,2035200,2131200,2188800,2246400,2323200,2380800,
		2438400,2515200,2572800,2630400,2707200,2764800,2841600,2899200,2956800,3014400,
		3072000,3148800};//N8 32

unsigned int cpu5_freq[] = {
		499200,614400,729600,844800,960000,1075200,1190400,1286400,1401600,1497600,
		1612800,1708800,1824000,1920000,2035200,2131200,2188800,2246400,2323200,2380800,
		2438400,2515200,2572800,2630400,2707200,2764800,2841600,2899200,2956800};//N8 29

unsigned int cpu7_freq[] = {
		480000,576000,672000,787200,902400,1017600,1132800,1248000,1363200,1478400,
		1593600,1708800,1824000,1939200,2035200,2112000,2169600,2246400,2304000,2380800,
		2438400,2496000,2553600,2630400,2688000,2745600,2803200,2880000,2937600,2995200,3052800};//N8 31

int max_cap [MAX_COUNT_POLICY] = {379, 923, 867, 1024};
int freq_size[MAX_COUNT_POLICY] = {
		sizeof(cpu0_freq) / sizeof(cpu0_freq[0]),
		sizeof(cpu2_freq) / sizeof(cpu2_freq[0]),
		sizeof(cpu5_freq) / sizeof(cpu5_freq[0]),
		sizeof(cpu7_freq) / sizeof(cpu7_freq[0])};
struct KeyValuePair KeyValuePair0[MAX_COUNT_FREQ];
struct KeyValuePair KeyValuePair2[MAX_COUNT_FREQ];
struct KeyValuePair KeyValuePair5[MAX_COUNT_FREQ];
struct KeyValuePair KeyValuePair7[MAX_COUNT_FREQ];
unsigned int policy0_freq[MAX_COUNT_FREQ];
unsigned int policy1_freq[MAX_COUNT_FREQ];
unsigned int policy2_freq[MAX_COUNT_FREQ];
unsigned int policy3_freq[MAX_COUNT_FREQ];
unsigned int policy0_cap[MAX_COUNT_FREQ];
unsigned int policy1_cap[MAX_COUNT_FREQ];
unsigned int policy2_cap[MAX_COUNT_FREQ];
unsigned int policy3_cap[MAX_COUNT_FREQ];
int policy_freq_size[MAX_COUNT_POLICY];
int policy_cap[MAX_COUNT_POLICY];
int cluster_count = 0;
struct hrtimer hrtimer_timer;
struct work_struct sWork;
extern struct workqueue_struct *hyperframe_notify_wq;

void hyperframe_init_freq_cap(void)
{
	int i = 0, j = 0;

	for (j = 0; j < cluster_count; j++) {
		if (0 == j) {
			for(i = 0; i < freq_size[j]; i++) {
				KeyValuePair0[i].key = policy0_freq[i];
				KeyValuePair0[i].value =
						(unsigned int)(policy0_freq[i] * policy_cap[j] / policy0_freq[policy_freq_size[j] - 1]);
				policy0_cap[i] = KeyValuePair0[i].value;
			}
		} else if (1 == j) {
			for(i = 0; i < freq_size[j]; i++) {
				KeyValuePair2[i].key = policy1_freq[i];
				KeyValuePair2[i].value =
						(unsigned int)(policy1_freq[i] * policy_cap[j] / policy1_freq[policy_freq_size[j] - 1]);
				policy1_cap[i] = KeyValuePair2[i].value;
			}
		} else if (2 == j) {
			for(i = 0; i < freq_size[j]; i++) {
				KeyValuePair5[i].key = policy2_freq[i];
				KeyValuePair5[i].value =
						(unsigned int)(policy2_freq[i] * policy_cap[j] / policy2_freq[policy_freq_size[j] - 1]);
				policy2_cap[i] = KeyValuePair5[i].value;
			}
		} else if (3 == j) {
			for(i = 0; i < freq_size[j]; i++) {
				KeyValuePair7[i].key = policy3_freq[i];
				KeyValuePair7[i].value =
						(unsigned int)(policy3_freq[i] * policy_cap[j] / policy3_freq[policy_freq_size[j] - 1]);
				policy3_cap[i] = KeyValuePair7[i].value;
			}
		}
	}
}

void hyperframe_setaffinity(int pid, int arr[], int len)
{
	struct task_struct *p = NULL;
	struct cpumask current_mask = {CPU_BITS_NONE};
	int i = 0, ret = -1;

	cpumask_clear(&current_mask);
	for (i = 0; i < len; i++)
		cpumask_set_cpu(arr[i], &current_mask);

	p = find_task_by_vpid(pid);
	if (!p) {
		pr_err("hyperframe_setaffinity find_task_by_vpid fail");
	} else {
		get_task_struct(p);
		ret = set_cpus_allowed_ptr(p, &current_mask);
		if (ret)
			pr_err("hyperframe_setaffinity  fail! pid = %d ret = %d", pid, ret);
		put_task_struct(p);
	}
}

int hyperframe_init_sched_policy(int ui_thread_id, int render_thread_id)
{
	int core[] = {2, 3, 4, 7};

	hyperframe_setaffinity(ui_thread_id, core, 4);
	return 1;
}

unsigned int get_capacity_by_freq(int cluster, unsigned int freq)
{
	int i = 0;
	if (cluster < 0 || cluster >= cluster_count)
		return 0;

	if (0 == cluster) {
		for (i = 0; i < freq_size[0]; i++) {
			if (freq == KeyValuePair0[i].key)
				return KeyValuePair0[i].value;
		}
	} else if (1== cluster) {
		for (i = 0; i < freq_size[1]; i++) {
			if (freq == KeyValuePair2[i].key)
				return KeyValuePair2[i].value;
		}
	} else if (2 == cluster) {
		for (i = 0; i < freq_size[2]; i++) {
			if (freq == KeyValuePair5[i].key)
				return KeyValuePair5[i].value;
		}
	} else if (3 == cluster) {
		for (i = 0; i < freq_size[3]; i++) {
			if (freq == KeyValuePair7[i].key)
				return KeyValuePair7[i].value;
		}
	}
	return 0;
}

int hyperframe_boost_uclamp_start(unsigned int pid, int dangerSignal)
{
	int ret = 0;
	struct sched_attr attr = {};
	struct task_struct *task = NULL;

	task = find_task_by_vpid(pid);
	attr.size = sizeof(attr);
	attr.sched_flags = (SCHED_FLAG_KEEP_ALL | SCHED_FLAG_UTIL_CLAMP);
	attr.sched_util_min = 400 + 200 * dangerSignal;
	attr.sched_util_max = 1024;
	if (task) {
		ret = sched_setattr(task, &attr);
	}
	if (ret == 0)
		atomic_set(&g_run_siginal, dangerSignal);

	htrace_b_predict(current->tgid,
			"[HYPERFRAME#INTER#%s] sched_util_min: %d, ret: %d", __func__, attr.sched_util_min, ret);
	htrace_e_predict();
	return ret;
}

int hyperframe_boost_uclamp_end(unsigned int pid)
{
	int ret = 0;
	struct sched_attr attr = {};
	struct task_struct *task = NULL;

	task = find_task_by_vpid(pid);
	attr.size = sizeof(attr);
	attr.sched_flags = (SCHED_FLAG_KEEP_ALL | SCHED_FLAG_UTIL_CLAMP);
	attr.sched_util_min = 180;
	attr.sched_util_max = 1024;
	if (task) {
		ret = sched_setattr(task, &attr);
	}

	return ret;
}

int hyperframe_boost_cpu(struct freq_qos_request *qos_req, int cpu, int min)
{
	struct cpufreq_policy *policy = NULL;
	int ret = -1;

	if (min < 1) {
		pr_err("hyperframe_boost_cpu fail, min < 1, %d", min);
		return -1;
	}

	if (qos_req == NULL) {
		pr_err("hyperframe_boost_cpu failed, qos_req is NULL!!!");
		return ret;
	}
	policy = cpufreq_cpu_get(cpu);
	if (policy) {
		if (min_freq >= policy->max) {
			pr_err("hyperframe_boost_cpu fail, min_freq >= policy->max, %d, %d", min_freq, policy->max);
			cpufreq_cpu_put(policy);
			return ret;
		}
		if (freq_qos_request_active(qos_req)) {
			ret = freq_qos_update_request(qos_req, min);
			if (ret <= 0)
				pr_err("hyperframe_boost_cpu freq_qos_update_request failed!!!");
		} else {
			ret = freq_qos_add_request(&policy->constraints, qos_req, FREQ_QOS_MIN, min);
			if (ret <= 0)
				pr_err("hyperframe_boost_cpu freq_qos_add_request failed!!!");
		}
		cpufreq_cpu_put(policy);
		htrace_b_sched(current->tgid, "[HYPERFRAME_SCHED|%s] min_freq: %u", __func__, min);
		htrace_e_sched();
	}

	return ret;
}

//TODO: update
int hyperframe_boost_cpu_cancel(struct freq_qos_request *qos_req)
{
	int ret = -1;

	if (qos_req && freq_qos_request_active(qos_req)) {
		ret = freq_qos_remove_request(qos_req);
		if (ret != 1)
			pr_err("hyperframe_boost_cpu_cancel failed!!!");
	}
	return ret;
}

int hyperframe_reset_cpufreq_qos(unsigned int pid)
{
	int ret1 = -1, ret2 = -1;

	ret1 = hyperframe_limit_cpu_cancel(req_max2, 2);
	htrace_c_sched_debug(pid, ret1, "cpu2_sched_ret");
	htrace_c_sched_debug(pid, 0, "cpu2_sched_ret");

	ret2 = hyperframe_limit_cpu_cancel(req_max7, 7);
	htrace_c_sched_debug(pid, ret2, "cpu7_sched_ret");
	htrace_c_sched_debug(pid, 0, "cpu7_sched_ret");

	if (hrtimer_active(&hrtimer_timer))
		hrtimer_cancel(&hrtimer_timer);
	return ret1 + ret2;
}

int limit_cpu_cancel_reset(void)
{
	int ret1 = -1, ret2 = -1;

	ret1 = hyperframe_limit_cpu_cancel(req_max2, 2);
	ret2 = hyperframe_limit_cpu_cancel(req_max7, 7);
	return ret1 + ret2;
}

int hyperframe_cpufreq_boost_exit(int danger)
{
	int ret = 0;

	ret = hyperframe_boost_cpu_cancel(req_min2);
	if (ret != 1)
		pr_err("hyperframe_cpufreq_boost_exit fail, cpu2");

	ret = hyperframe_boost_cpu_cancel(req_min5);
	if (ret != 1)
		pr_err("hyperframe_cpufreq_boost_exit fail, cpu5");

	ret = hyperframe_boost_cpu_cancel(req_min_super);
	if (ret != 1)
		pr_err("hyperframe_cpufreq_boost_exit fail, cpu7");

	return ret;
}

int hyperframe_limit_cpu_cancel(struct freq_qos_request *qos_req, int cpu)
{
	int ret = -1;

	if (qos_req == NULL || !freq_qos_request_active(qos_req))
		return -1;

	ret = freq_qos_remove_request(qos_req);

	htrace_b_sched_debug(current->tgid,
			"[HYPERFRAME#SCHED#%s] req_max2:%p, req_max7:%p", __func__, req_max2, req_max7);
	htrace_e_sched_debug();

	return ret;
}

int hyperframe_limit_cpu(struct freq_qos_request *qos_req, int cpu, int max)
{
	struct cpufreq_policy *policy = NULL;
	int ret = -1;

	if (!qos_req)
		return ret;

	policy = cpufreq_cpu_get(cpu);

	if (!policy)
		return -2;

	if (max <= policy->min) {
		cpufreq_cpu_put(policy);
		return -3;
	}

	if (freq_qos_request_active(qos_req))
		ret = freq_qos_update_request(qos_req, max);
	else
		ret = freq_qos_add_request(&policy->constraints, qos_req, FREQ_QOS_MAX, max);

	htrace_b_sched_debug(current->tgid,
			"[HYPERFRAME_SCHED|%s] cpu: %d, limit_ret: %d, max: %d", __func__, cpu, ret, max);
	htrace_e_sched_debug();

	cpufreq_cpu_put(policy);

	return ret;
}

int search(unsigned int arr[], int len)
{
	int number = 0;
	unsigned int min = arr[0];

	for (int i = 1; i < len; i++){
		if (arr[i] < min) {
			min = arr[i];
			number = i;
		}
	}

	return number;
}

int hyperframe_cpufreq_limit_intra_qos(unsigned int cap)
{
	int ret2 = -1, ret7 = -1;
	unsigned int freq2, freq7;
	unsigned int arr1[MAX_COUNT_FREQ], arr2[MAX_COUNT_FREQ];
	int flag = 0;
	int len1 = policy_freq_size[1], len3 = policy_freq_size[3];

	if (cap < 1)
		return -1024;

	if (!req_max2)
		req_max2 = &per_cpu(qos_max_req, 2);
	if (!req_max2)
		return -2;
	if (!req_max7)
		req_max7 = &per_cpu(qos_max_req, 7);
	if (!req_max7)
		return -7;

	for (int i = 0; i < len1; i++)
		arr1[i] = policy1_cap[i] >= cap ? policy1_cap[i] - cap : cap - policy1_cap[i];

	for (int i = 0; i < len3; i++)
		arr2[i] = policy3_cap[i] >= cap ? policy3_cap[i] - cap : cap - policy3_cap[i];

	flag = search(arr1, len1);
	freq2 = policy1_freq[flag];
	ret2 = hyperframe_limit_cpu(req_max2, 2, freq2);

	flag = search(arr2, len3);
	freq7 = policy3_freq[flag];
	ret7 = hyperframe_limit_cpu(req_max7, 7, freq7);

	return ret2 + ret7;
}

int hyperframe_set_capacity_qos(int pid, int target)
{
	int ret = -1;
	unsigned int cap = 0;
	ktime_t interval;

	if (target < 1)
		return ret;

	cap = target * 12 / 10;
	if (cap < 1)
		return ret;

	if (cap < 350)
		cap = 350;
	ret = hyperframe_cpufreq_limit_intra_qos(cap);
	if (ret == 1 || ret == 2) {
		interval = ktime_set(0, 200 * 1000000);
		if (hrtimer_active(&hrtimer_timer)) {
			hrtimer_cancel(&hrtimer_timer);
		}
		hrtimer_start(&hrtimer_timer, interval, HRTIMER_MODE_REL);
	}

	return ret;
}

void limit_cpu_cancel_wq_cb(struct work_struct *work)
{
	limit_cpu_cancel_reset();
}

enum hrtimer_restart hrtimer_timer_poll(struct hrtimer *timer)
{
	if (hyperframe_notify_wq != NULL) {
		queue_work(hyperframe_notify_wq, &sWork);
	}
	return HRTIMER_NORESTART;
}

void hyperframe_init_energy(void)
{
	struct cpufreq_policy* policy;
	struct cpufreq_frequency_table* pos;
	struct cpufreq_frequency_table* table = NULL;
	int cpu = 0, count = 0;
	unsigned long cap = 0;

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			break;

		table = policy->freq_table;
		cpufreq_for_each_valid_entry(pos, table) {
			if (count >= MAX_COUNT_FREQ) {
				pr_err("hyperframe_init_energy over freq count");
				break;
			}

			if (pos->flags & CPUFREQ_BOOST_FREQ) {
				continue;
			} else {
				if (0 == cluster_count)
					policy0_freq[count] = pos->frequency;
				else if (1 == cluster_count)
					policy1_freq[count] = pos->frequency;
				else if (2 == cluster_count)
					policy2_freq[count] = pos->frequency;
				else if (3 == cluster_count)
					policy3_freq[count] = pos->frequency;

				count ++;
			}
		}

		cpu = cpumask_last(policy->related_cpus);
		cap = per_cpu(cpu_scale, cpu);
		policy_freq_size[cluster_count] = count;
		policy_cap[cluster_count] = cap;
		cluster_count++;
		count = 0;
		cpufreq_cpu_put(policy);
	}
	hyperframe_init_freq_cap();

	hrtimer_init(&hrtimer_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrtimer_timer.function = hrtimer_timer_poll;
	INIT_WORK(&sWork, limit_cpu_cancel_wq_cb);
}
// END Performance_FramePredictBoost
