
#ifdef CONFIG_SCHED_TUNE

#include <linux/reciprocal_div.h>

/*
 * System energy normalization constants
 */
struct target_nrg {
	unsigned long min_power;
	unsigned long max_power;
	struct reciprocal_value rdiv;
#ifdef CONFIG_MTK_UNIFY_POWER
	unsigned long max_dyn_pwr[32];
	unsigned long max_stc_pwr[32];
#else
	unsigned long max_pwr[32];
#endif
};

struct target_cap {
	int cap;
	int freq;
};

#ifdef CONFIG_CPU_FREQ_GOV_SCHEDPLUS
extern unsigned long int min_boost_freq[3];
extern unsigned long int cap_min_freq[3];
extern void update_cpu_freq_quick(int cpu, int freq);
#ifdef CONFIG_MTK_CPU_FREQ
extern unsigned int
mt_cpufreq_get_freq_by_idx(int id, int idx);
extern unsigned int
mt_cpufreq_find_close_freq(unsigned int id, unsigned int freq);
#else
static inline unsigned int
mt_cpufreq_get_freq_by_idx(int id, int idx) { return 0; }
static inline unsigned int
mt_cpufreq_find_close_freq(unsigned int id, unsigned int freq) { return 0; }
#endif
#endif

extern unsigned long boosted_cpu_util(int cpu);
extern raw_spinlock_t stune_lock;

#ifdef CONFIG_CGROUP_SCHEDTUNE
#ifdef CONFIG_UCLAMP_TASK_GROUP
extern int uclamp_group_get(struct task_struct *p,
				   struct cgroup_subsys_state *css,
				   int clamp_id, struct uclamp_se *uc_se,
				   unsigned int clamp_value);
extern void uclamp_group_put(int clamp_id, int group_id);
extern struct mutex uclamp_mutex;
#endif

int schedtune_cpu_boost(int cpu);
int schedtune_task_boost(struct task_struct *tsk);

int schedtune_cpu_capacity_min(int cpu);
int schedtune_task_capacity_min(struct task_struct *tsk);

int schedtune_prefer_idle(struct task_struct *tsk);

void schedtune_exit_task(struct task_struct *tsk);

void schedtune_enqueue_task(struct task_struct *p, int cpu);
void schedtune_dequeue_task(struct task_struct *p, int cpu);

extern int stune_task_threshold;

#else /* CONFIG_CGROUP_SCHEDTUNE */

#define schedtune_cpu_boost(cpu)  get_sysctl_sched_cfs_boost()
#define schedtune_task_boost(tsk) get_sysctl_sched_cfs_boost()
#define schedtune_cpu_capacity_min(cpu) 0
#define schedtune_task_capacity_min(cpu) 0

#define schedtune_exit_task(task) do { } while (0)

#define schedtune_enqueue_task(task, cpu) do { } while (0)
#define schedtune_dequeue_task(task, cpu) do { } while (0)

#define stune_task_threshold 0
#endif /* CONFIG_CGROUP_SCHEDTUNE */

int schedtune_normalize_energy(int energy);
int schedtune_accept_deltas(int nrg_delta, int cap_delta,
			    struct task_struct *task);

#else /* CONFIG_SCHED_TUNE */

#define schedtune_cpu_boost(cpu)  0
#define schedtune_task_boost(tsk) 0
#define schedtune_cpu_capacity_min(cpu)  0
#define schedtune_task_capacity_min(tsk) 0

#define schedtune_exit_task(task) do { } while (0)

#define schedtune_enqueue_task(task, cpu) do { } while (0)
#define schedtune_dequeue_task(task, cpu) do { } while (0)

#define schedtune_accept_deltas(nrg_delta, cap_delta, task) nrg_delta

#endif /* CONFIG_SCHED_TUNE */
