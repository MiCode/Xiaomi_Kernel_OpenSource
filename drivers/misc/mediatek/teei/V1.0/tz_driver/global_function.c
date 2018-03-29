#include <linux/types.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/freezer.h>
#include <linux/semaphore.h>

#include "nt_smc_call.h"
#include "global_function.h"
#include "sched_status.h"

#define SCHED_CALL      0x04

extern int add_work_entry(int work_type, unsigned long buff);

static void secondary_nt_sched_t(void *info)
{
	nt_sched_t();
}


void nt_sched_t_call(void)
{
	int cpu_id = 0;
#if 0
	get_online_cpus();
	cpu_id = get_current_cpuid();
	smp_call_function_single(cpu_id, secondary_nt_sched_t, NULL, 1);
	put_online_cpus();
#else
	int retVal = 0;

	retVal = add_work_entry(SCHED_CALL, NULL);
	if (retVal != 0)
		pr_err("[%s][%d] add_work_entry function failed!\n", __func__, __LINE__);

#endif

	return;
}


int global_fn(void)
{
	int retVal = 0;

	while (1) {
		set_freezable();
		set_current_state(TASK_INTERRUPTIBLE);
#if 1
		if (teei_config_flag == 1) {
			retVal = wait_for_completion_interruptible(&global_down_lock);
			if (retVal == -ERESTARTSYS) {
				pr_err("[%s][%d]*********down &global_down_lock failed *****************\n", __func__, __LINE__);
				continue;
			}
		}
#endif
		retVal = down_interruptible(&smc_lock);
		if (retVal != 0) {
			pr_err("[%s][%d]*********down &smc_lock failed *****************\n", __func__, __LINE__);
			complete(&global_down_lock);
			continue;
		}

		if (forward_call_flag == GLSCH_FOR_SOTER) {
			forward_call_flag = GLSCH_NONE;
			msleep(10);
			nt_sched_t_call();
		} else if (irq_call_flag == GLSCH_HIGH) {
			/* pr_debug("[%s][%d]**************************\n", __func__, __LINE__ ); */
			irq_call_flag = GLSCH_NONE;
			nt_sched_t_call();
			/*msleep_interruptible(10);*/
		} else if (fp_call_flag == GLSCH_HIGH) {
			/* pr_debug("[%s][%d]**************************\n", __func__, __LINE__ ); */
			if (teei_vfs_flag == 0) {
				nt_sched_t_call();
			} else {
				up(&smc_lock);
				msleep_interruptible(1);
			}
		} else if (forward_call_flag == GLSCH_LOW) {
			/* pr_debug("[%s][%d]**************************\n", __func__, __LINE__ ); */
			if (teei_vfs_flag == 0)	{
				nt_sched_t_call();
			} else {
				up(&smc_lock);
				msleep_interruptible(1);
			}
		} else {
			/* pr_debug("[%s][%d]**************************\n", __func__, __LINE__ ); */
			up(&smc_lock);
			msleep_interruptible(1);
		}
	}
}
