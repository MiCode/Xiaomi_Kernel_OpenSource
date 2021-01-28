
#ifdef CONFIG_SCHED_TUNE

#include <linux/reciprocal_div.h>

/*
 * System energy normalization constants
 */
struct target_nrg {
	unsigned long min_power;
	unsigned long max_power;
	struct reciprocal_value rdiv;
};

int schedtune_cpu_boost(int cpu);
int schedtune_task_boost(struct task_struct *tsk);

int schedtune_prefer_idle(struct task_struct *tsk);

void schedtune_enqueue_task(struct task_struct *p, int cpu);
void schedtune_dequeue_task(struct task_struct *p, int cpu);

unsigned long stune_util(int cpu, unsigned long other_util);

#ifdef CONFIG_UCLAMP_TASK_GROUP
extern struct mutex uclamp_mutex;
extern void uclamp_group_get(struct task_struct *p,
			     struct cgroup_subsys_state *css,
			     struct uclamp_se *uc_se,
			     unsigned int clamp_id, unsigned int clamp_value);
extern void uclamp_group_put(unsigned int clamp_id, unsigned int group_id);

extern void
uclamp_task_update_active(struct task_struct *p, unsigned int clamp_id);
#endif
#else /* CONFIG_SCHED_TUNE */

#define schedtune_cpu_boost(cpu)  0
#define schedtune_task_boost(tsk) 0

#define schedtune_prefer_idle(tsk) 0

#define schedtune_enqueue_task(task, cpu) do { } while (0)
#define schedtune_dequeue_task(task, cpu) do { } while (0)

#endif /* CONFIG_SCHED_TUNE */
