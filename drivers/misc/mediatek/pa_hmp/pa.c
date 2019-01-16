#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/tty.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include <asm/cputime.h>
#include <linux/tick.h>
#include <linux/kernel_stat.h>

#include <linux/jiffies.h>
#ifdef CONFIG_HMP_POWER_AWARE_CONTROLLER
#include "../../../kernel/sched/sched.h"

MODULE_DESCRIPTION("Power-aware Scheduler Controller");
MODULE_AUTHOR("Ted Hu");
MODULE_LICENSE("GPL");

extern u32 PA_ENABLE;
extern u32 LB_ENABLE;
extern u32 PA_MON_ENABLE;
extern char PA_MON[];

extern u32 HMP_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
extern u32 PACK_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
extern u32 AVOID_WAKE_UP_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
extern u32 AVOID_LOAD_BALANCE_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
extern u32 AVOID_FORCE_UP_MIGRATION_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];

extern int sd_pack_buddy;

extern unsigned int hmp_up_threshold;
extern unsigned int hmp_down_threshold;
extern unsigned int hmp_up_prio;
extern unsigned int hmp_next_up_threshold;
extern unsigned int hmp_next_down_threshold;

/* extern u32 FREQ_CPU; */

extern int sd_pack_buddy;

DEFINE_PER_CPU(u64, IDLE_CPU_TIME_DIFF);

unsigned int jiffies_orig;
unsigned int jiffies_diff;

static void cut_end(char *input_temp)
{
	int index_temp = 0;

	while (input_temp[index_temp] != '\0') {
		index_temp++;
	}

	while (input_temp[index_temp] == '\0' || input_temp[index_temp] == ' '
	       || input_temp[index_temp] == '\n') {
		index_temp--;
	}

	index_temp++;
	input_temp[index_temp] = '\0';

}

#ifdef arch_idle_time

static cputime64_t get_idle_time(int cpu)
{
	cputime64_t idle;

	idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	if (cpu_online(cpu) && !nr_iowait_cpu(cpu))
		idle += arch_idle_time(cpu);
	return idle;
}

#else

static u64 get_idle_time(int cpu)
{
	u64 idle, idle_time = -1ULL;

	if (cpu_online(cpu))
		idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.idle */
		idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	else
		idle = usecs_to_cputime64(idle_time);

	return idle;
}

#endif

static ssize_t pa_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "PA_ENABLE = %u\n", PA_ENABLE);
}

static ssize_t pa_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf,
			size_t count)
{
	u32 temp;

	sscanf(buf, "%lu", &temp);

	pr_emerg("Set PA_ENABLE = %u\n", temp);

	PA_ENABLE = temp;

	return count;
}

static ssize_t lb_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "LB_ENABLE = %u\n", LB_ENABLE);
}

static ssize_t lb_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf,
			size_t count)
{
	u32 temp;

	sscanf(buf, "%lu", &temp);

	pr_emerg("Set LB_ENABLE = %u\n", temp);

	LB_ENABLE = temp;

	return count;
}

#define TMP_BUF_LENGTH 256
static ssize_t pa_stat(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int cpu, cpu_from, cpu_to;
	u64 idle;
	u64 idle_time_diff;

	char tmp_buf[TMP_BUF_LENGTH];

	/* Jiffies */
	if (jiffies_orig == 0) {
		jiffies_diff = 0;
	} else {
		jiffies_diff = jiffies - jiffies_orig;
	}
	jiffies_orig = jiffies;

	snprintf(tmp_buf, TMP_BUF_LENGTH, "Jiffies Diff = %u\n", jiffies_diff);
	strncat(buf, tmp_buf, TMP_BUF_LENGTH);

	/* Power-aware Status */
	if (PA_ENABLE == 1) {
		snprintf(tmp_buf, TMP_BUF_LENGTH, "PA Enable\n");
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);
	} else {
		snprintf(tmp_buf, TMP_BUF_LENGTH, "PA Disable\n");
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);
	}

	if (LB_ENABLE == 1) {
		snprintf(tmp_buf, TMP_BUF_LENGTH, "LB Enable\n");
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);
	} else {
		snprintf(tmp_buf, TMP_BUF_LENGTH, "LB Disable\n");
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);
	}

	/* PACK Status */
	for_each_possible_cpu(cpu_from) {
		snprintf(tmp_buf, TMP_BUF_LENGTH, "PACK FORM CPU%d TO", cpu_from);
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);

		for_each_possible_cpu(cpu_to) {
			snprintf(tmp_buf, TMP_BUF_LENGTH, " CPU%d:%04u", cpu_to,
				 PACK_FROM_CPUX_TO_CPUY_COUNT[cpu_from][cpu_to]);
			strncat(buf, tmp_buf, TMP_BUF_LENGTH);

			/* Reset statistic */
			PACK_FROM_CPUX_TO_CPUY_COUNT[cpu_from][cpu_to] = 0;
		}

		snprintf(tmp_buf, TMP_BUF_LENGTH, "\n");
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);
	}

	for_each_possible_cpu(cpu_from) {
		snprintf(tmp_buf, TMP_BUF_LENGTH, "AVOID WAKE UP FORM CPU%d TO", cpu_from);
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);

		for_each_possible_cpu(cpu_to) {
			snprintf(tmp_buf, TMP_BUF_LENGTH, " CPU%d:%04u", cpu_to,
				 AVOID_WAKE_UP_FROM_CPUX_TO_CPUY_COUNT[cpu_from][cpu_to]);
			strncat(buf, tmp_buf, TMP_BUF_LENGTH);

			/* Reset statistic */
			AVOID_WAKE_UP_FROM_CPUX_TO_CPUY_COUNT[cpu_from][cpu_to] = 0;
		}

		snprintf(tmp_buf, TMP_BUF_LENGTH, "\n");
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);
	}


	for_each_possible_cpu(cpu_from) {
		snprintf(tmp_buf, TMP_BUF_LENGTH, "AVOID BALANCE FORM CPU%d TO", cpu_from);
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);

		for_each_possible_cpu(cpu_to) {
			snprintf(tmp_buf, TMP_BUF_LENGTH, " CPU%d:%04u", cpu_to,
				 AVOID_LOAD_BALANCE_FROM_CPUX_TO_CPUY_COUNT[cpu_from][cpu_to]);
			strncat(buf, tmp_buf, TMP_BUF_LENGTH);

			/* Reset statistic */
			AVOID_LOAD_BALANCE_FROM_CPUX_TO_CPUY_COUNT[cpu_from][cpu_to] = 0;
		}

		snprintf(tmp_buf, TMP_BUF_LENGTH, "\n");
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);
	}

	for_each_possible_cpu(cpu_from) {
		snprintf(tmp_buf, TMP_BUF_LENGTH, "AVOID FORCE UP FORM CPU%d TO", cpu_from);
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);

		for_each_possible_cpu(cpu_to) {
			snprintf(tmp_buf, TMP_BUF_LENGTH, " CPU%d:%04u", cpu_to,
				 AVOID_FORCE_UP_MIGRATION_FROM_CPUX_TO_CPUY_COUNT[cpu_from]
				 [cpu_to]);
			strncat(buf, tmp_buf, TMP_BUF_LENGTH);

			/* Reset statistic */
			AVOID_FORCE_UP_MIGRATION_FROM_CPUX_TO_CPUY_COUNT[cpu_from][cpu_to] = 0;
		}

		snprintf(tmp_buf, TMP_BUF_LENGTH, "\n");
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);
	}

	/* CPU Status */
	for_each_possible_cpu(cpu) {
		idle = get_idle_time(cpu);

		if (per_cpu(IDLE_CPU_TIME_DIFF, cpu) == 0) {
			idle_time_diff = idle;
			per_cpu(IDLE_CPU_TIME_DIFF, cpu) = idle;
		} else {
			idle_time_diff = idle - per_cpu(IDLE_CPU_TIME_DIFF, cpu);
			per_cpu(IDLE_CPU_TIME_DIFF, cpu) = idle;
		}

/*
		snprintf(tmp_buf, TMP_BUF_LENGTH, "cpu%d POWER:%04lu FREQ:%09u FREQ_SCALE:%04u BUDDY:%d CFS_RQ:%04u IDLE_TIME_DIFF:%09llu\n",
											cpu,
											cpu_rq(cpu)->cpu_power,
											per_cpu(FREQ_CPU, cpu),
											freq_scale[cpu].curr_scale,
											per_cpu(sd_pack_buddy, cpu),
											cpu_rq(cpu)->cfs.nr_running,
											cputime64_to_clock_t(idle_time_diff));
*/

		snprintf(tmp_buf, TMP_BUF_LENGTH,
			 "cpu%d POWER:%04lu BUDDY:%d CFS_RQ:%04u IDLE_TIME_DIFF:%09llu\n", cpu,
			 cpu_rq(cpu)->cpu_power, per_cpu(sd_pack_buddy, cpu),
			 cpu_rq(cpu)->cfs.nr_running, cputime64_to_clock_t(idle_time_diff));

		strncat(buf, tmp_buf, TMP_BUF_LENGTH);

	}

	return strlen(buf);
}

static ssize_t pa_mon_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "PA_MON_ENABLE = %u, PA_MON = \"%s\"\n", PA_MON_ENABLE, PA_MON);
}

static ssize_t pa_mon_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf,
			    size_t count)
{
	size_t len = strlen(buf);

	if (len < TASK_COMM_LEN) {

		if (len == 1) {
			PA_MON_ENABLE = 0;
		} else {
			strncpy(PA_MON, buf, TASK_COMM_LEN);
			cut_end(PA_MON);
			PA_MON_ENABLE = 1;
		}
		pr_emerg("Set PA_MON_ENABLE = %u, PA_MON = \"%s\"\n", PA_MON_ENABLE, PA_MON);

	} else {
		pr_emerg("Task name over %d\n", TASK_COMM_LEN);
	}

	return count;
}

static struct kobj_attribute pa_enable_attribute = __ATTR(pa_enable, 0664, pa_show, pa_store);
static struct kobj_attribute lb_enable_attribute = __ATTR(lb_enable, 0664, lb_show, lb_store);
static struct kobj_attribute pa_stat_attribute = __ATTR(pa_stat, 0444, pa_stat, NULL);
static struct kobj_attribute pa_mon_attribute = __ATTR(pa_mon, 0664, pa_mon_show, pa_mon_store);

static struct attribute *attrs[] = {
	&pa_enable_attribute.attr,
	&lb_enable_attribute.attr,
	&pa_stat_attribute.attr,
	&pa_mon_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *pa_kobj;

static int __init pa_init(void)
{
	int retval;

	pr_emerg("PA Init\n");

	pa_kobj = kobject_create_and_add("pa", kernel_kobj);

	if (!pa_kobj) {
		return -ENOMEM;
	}

	retval = sysfs_create_group(pa_kobj, &attr_group);

	if (retval) {
		kobject_put(pa_kobj);
	}
	/* Init Global Variable */
	PA_ENABLE = 1;
	PA_MON_ENABLE = 0;

	strncpy(PA_MON, "", TASK_COMM_LEN);

	return retval;
}

static void __exit pa_exit(void)
{
	pr_emerg("PA Exit\n");
}
module_init(pa_init);
module_exit(pa_exit);
#endif

#ifdef CONFIG_MTK_SCHED_CMP_POWER_AWARE_CONTROLLER
#include "arch/arm/include/asm/topology.h"

MODULE_DESCRIPTION("Power-aware Scheduler Controller");
MODULE_AUTHOR("Ted Hu");
MODULE_LICENSE("GPL");

/* extern u32 PA_ENABLE; */
extern u32 PA_MON_ENABLE;
extern char PA_MON[4][TASK_COMM_LEN];

extern u32 PACK_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
extern u32 AVOID_WAKE_UP_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
extern u32 AVOID_LOAD_BALANCE_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
extern u32 TASK_PACK_CPU_COUNT[4][NR_CPUS];
extern struct cpumask buddy_cpu_map;
extern u32 PA_ENABLE;
extern cputime64_t get_idle_time(int cpu);
extern struct list_head cpu_domains;
/* extern int sd_pack_buddy; */

/* extern unsigned int hmp_up_threshold; */
/* extern unsigned int hmp_down_threshold; */
/* extern unsigned int hmp_up_prio; */
/* extern unsigned int hmp_next_up_threshold; */
/* extern unsigned int hmp_next_down_threshold; */

/* extern u32 FREQ_CPU; */

/* extern int sd_pack_buddy; */


DEFINE_PER_CPU(u64, IDLE_CPU_TIME_DIFF);
DECLARE_PER_CPU(int, sd_pack_buddy);

unsigned int jiffies_orig = 0;
unsigned int jiffies_diff = 0;

static void cut_end(char *input_temp)
{
	int index_temp = 0;

	while (input_temp[index_temp] != '\0') {
		index_temp++;
	}

	while (input_temp[index_temp] == '\0' || input_temp[index_temp] == ' '
	       || input_temp[index_temp] == '\n') {
		index_temp--;
	}

	index_temp++;
	input_temp[index_temp] = '\0';

}

static ssize_t pa_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "PA_ENABLE = %u\n", PA_ENABLE);
}

static ssize_t pa_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf,
			size_t count)
{
	u32 temp;

	sscanf(buf, "%u", &temp);

	pr_emerg("Set PA_ENABLE = %u\n", temp);

	PA_ENABLE = temp;

	return count;
}

/*
static ssize_t lb_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "LB_ENABLE = %u\n", LB_ENABLE);
}

static ssize_t lb_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	u32 temp;

	sscanf(buf, "%u", &temp);

	pr_emerg("Set LB_ENABLE = %u\n", temp);

	LB_ENABLE = temp;

	return count;
}
*/
#define TMP_BUF_LENGTH 256

static ssize_t pa_stat(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int cpu, cpu_from, cpu_to;
	u64 idle;
	u64 idle_time_diff;
	struct list_head *pos;
	struct cpu_domain *cluster;
	char tmp_buf[TMP_BUF_LENGTH];
	u8 i = 0;

	/* PACK Status */
	snprintf(tmp_buf, TMP_BUF_LENGTH,
		 "-----------------------PACK status-----------------------------------\n");
	strncat(buf, tmp_buf, TMP_BUF_LENGTH);
	for_each_possible_cpu(cpu_from) {
		snprintf(tmp_buf, TMP_BUF_LENGTH, "PACK FORM CPU%d TO", cpu_from);
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);

		for_each_possible_cpu(cpu_to) {
			snprintf(tmp_buf, TMP_BUF_LENGTH, " CPU%d:%04u", cpu_to,
				 PACK_FROM_CPUX_TO_CPUY_COUNT[cpu_from][cpu_to]);
			strncat(buf, tmp_buf, TMP_BUF_LENGTH);

			/* Reset statistic */
			PACK_FROM_CPUX_TO_CPUY_COUNT[cpu_from][cpu_to] = 0;
		}

		snprintf(tmp_buf, TMP_BUF_LENGTH, "\n");
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);
	}

	for_each_possible_cpu(cpu_from) {
		snprintf(tmp_buf, TMP_BUF_LENGTH, "AVOID WAKE UP FORM CPU%d TO", cpu_from);
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);

		for_each_possible_cpu(cpu_to) {
			snprintf(tmp_buf, TMP_BUF_LENGTH, " CPU%d:%04u", cpu_to,
				 AVOID_WAKE_UP_FROM_CPUX_TO_CPUY_COUNT[cpu_from][cpu_to]);
			strncat(buf, tmp_buf, TMP_BUF_LENGTH);

			/* Reset statistic */
			AVOID_WAKE_UP_FROM_CPUX_TO_CPUY_COUNT[cpu_from][cpu_to] = 0;
		}

		snprintf(tmp_buf, TMP_BUF_LENGTH, "\n");
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);
	}


	for_each_possible_cpu(cpu_from) {
		snprintf(tmp_buf, TMP_BUF_LENGTH, "AVOID BALANCE FORM CPU%d TO", cpu_from);
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);

		for_each_possible_cpu(cpu_to) {
			snprintf(tmp_buf, TMP_BUF_LENGTH, " CPU%d:%04u", cpu_to,
				 AVOID_LOAD_BALANCE_FROM_CPUX_TO_CPUY_COUNT[cpu_from][cpu_to]);
			strncat(buf, tmp_buf, TMP_BUF_LENGTH);

			/* Reset statistic */
			AVOID_LOAD_BALANCE_FROM_CPUX_TO_CPUY_COUNT[cpu_from][cpu_to] = 0;
		}

		snprintf(tmp_buf, TMP_BUF_LENGTH, "\n");
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);
	}
	snprintf(tmp_buf, TMP_BUF_LENGTH,
		 "---------------------------------CPU buddy---------------------------------\n");
	strncat(buf, tmp_buf, TMP_BUF_LENGTH);
	for_each_online_cpu(cpu_from) {
		snprintf(tmp_buf, TMP_BUF_LENGTH, "CPU%d's buddy is CPU%d\n", cpu_from,
			 per_cpu(sd_pack_buddy, cpu_from));
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);
	}
	snprintf(tmp_buf, TMP_BUF_LENGTH,
		 "-----------------------------------Buddy CPUs-------------------------------\n");
	strncat(buf, tmp_buf, TMP_BUF_LENGTH);
	for_each_cpu(cpu, &buddy_cpu_map) {
		snprintf(tmp_buf, TMP_BUF_LENGTH, "Buddy CPU is  CPU%d\n", cpu);
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);
	}
	if (!list_empty(&cpu_domains)) {
		struct cpumask *cpu_mask;
		unsigned int j;
		snprintf(tmp_buf, TMP_BUF_LENGTH,
			 "--------------------------------Cluster info----------------------------\n");
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);
		for (j = 0; j < cluster_nr(); j++) {
			snprintf(tmp_buf, TMP_BUF_LENGTH, "cluster %d\n", j);
			strncat(buf, tmp_buf, TMP_BUF_LENGTH);
			cpu_mask = get_domain_cpus(j, 0);	/* possiable cpus */
			snprintf(tmp_buf, TMP_BUF_LENGTH, "Possible cpus: ");
			strncat(buf, tmp_buf, TMP_BUF_LENGTH);
			for_each_cpu(cpu, cpu_mask) {
				snprintf(tmp_buf, TMP_BUF_LENGTH, "cpu%d ", cpu);
				strncat(buf, tmp_buf, TMP_BUF_LENGTH);
			}
			snprintf(tmp_buf, TMP_BUF_LENGTH, "\n");
			strncat(buf, tmp_buf, TMP_BUF_LENGTH);
			cpu_mask = get_domain_cpus(j, 1);	/* Online cpus */
			snprintf(tmp_buf, TMP_BUF_LENGTH, "Online cpus: ");
			strncat(buf, tmp_buf, TMP_BUF_LENGTH);
			for_each_cpu(cpu, cpu_mask) {
				snprintf(tmp_buf, TMP_BUF_LENGTH, "cpu%d ", cpu);
				strncat(buf, tmp_buf, TMP_BUF_LENGTH);
			}
			snprintf(tmp_buf, TMP_BUF_LENGTH, "\n");
			strncat(buf, tmp_buf, TMP_BUF_LENGTH);
		}
	}
	if (PA_MON_ENABLE) {
		snprintf(tmp_buf, TMP_BUF_LENGTH,
			 "----------------------------------Task MONITOR-----------------------------\n");
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);
		for (i = 0; i < 4; i++) {
			if (strlen(&PA_MON[i][0]) != 0) {
				for_each_possible_cpu(cpu) {
					if (TASK_PACK_CPU_COUNT[i][cpu] != 0) {
						snprintf(tmp_buf, TMP_BUF_LENGTH,
							 "%s pack to cpu%d count is %d\n",
							 &PA_MON[i][0], cpu,
							 TASK_PACK_CPU_COUNT[i][cpu]);
						strncat(buf, tmp_buf, TMP_BUF_LENGTH);
						TASK_PACK_CPU_COUNT[i][cpu] = 0;
					}
				}
			}
		}
	}
	return strlen(buf);
}

static ssize_t pa_mon_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	char PA_MON_tmp[4 * TASK_COMM_LEN] = { 0 };
	char *PA_MON_ptr = PA_MON_tmp;
	u8 i;

	for (i = 0; i < 4; i++) {
		if (PA_MON[i][0] != NULL) {
			strcat(PA_MON_ptr, &PA_MON[i][0]);
			strcat(PA_MON_ptr, " ");
		}
	}
	return sprintf(buf, "PA_MON_ENABLE = %u, PA_MON = \"%s\"\n", PA_MON_ENABLE, PA_MON_tmp);
}

static ssize_t pa_mon_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf,
			    size_t count)
{
	size_t len = strlen(buf);
	u8 i = 0;
	u8 j = 0;
	u8 *chr_ptr = NULL;
	u64 ptr_offset = 0;

	if (len == 1) {
		PA_MON_ENABLE = 0;
	} else {
		for (j = 0; j < 4; j++)
			memset(&PA_MON[j][0], 0, TASK_COMM_LEN);
		while (i < 4) {
			if (buf[ptr_offset] == '\0')
				break;
			chr_ptr = strchr(&buf[ptr_offset], ' ');
			if (chr_ptr != (char *)NULL) {
				strncpy(&PA_MON[i][0], &buf[ptr_offset],
					(u64) chr_ptr - (u64) (&buf[ptr_offset]));
				cut_end(&PA_MON[i][0]);
				ptr_offset += ((u64) chr_ptr - (u64) (&buf[ptr_offset]) + 1);
			} else {
				strcpy(&PA_MON[i][0], &buf[ptr_offset]);
				cut_end(&PA_MON[i][0]);
				break;
			}
			i++;
		}
		PA_MON_ENABLE = 1;
	}
	return count;
}

static ssize_t pa_load(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int cpu, cpu_from, cpu_to;
	u64 idle;
	u64 idle_time_diff;
	struct list_head *pos;
	struct cpu_domain *cluster;
	char tmp_buf[TMP_BUF_LENGTH];
	u8 i = 0;

	/* Jiffies */

	if (jiffies_orig == 0) {
		jiffies_diff = 0;
	} else {
		jiffies_diff = jiffies - jiffies_orig;
	}
	jiffies_orig = jiffies;

	snprintf(tmp_buf, TMP_BUF_LENGTH, "Jiffies Diff = %u\n", jiffies_diff);
	strncat(buf, tmp_buf, TMP_BUF_LENGTH);
	if (jiffies_diff != 0) {
		snprintf(tmp_buf, TMP_BUF_LENGTH,
			 "--------------------------------CPU work load------------------------\n");
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);
		for_each_possible_cpu(cpu) {
			idle = usecs_to_cputime64(get_cpu_idle_time_us(cpu, NULL));	/* get_idle_time(cpu); */

			if (per_cpu(IDLE_CPU_TIME_DIFF, cpu) == 0) {
				idle_time_diff = idle;
				per_cpu(IDLE_CPU_TIME_DIFF, cpu) = idle;
			} else {
				idle_time_diff = idle - per_cpu(IDLE_CPU_TIME_DIFF, cpu);
				per_cpu(IDLE_CPU_TIME_DIFF, cpu) = idle;
			}

			/*
			   snprintf(tmp_buf, TMP_BUF_LENGTH, "cpu%d POWER:%04lu FREQ:%09u FREQ_SCALE:%04u BUDDY:%d CFS_RQ:%04u IDLE_TIME_DIFF:%09llu\n",
			   cpu,
			   cpu_rq(cpu)->cpu_power,
			   per_cpu(FREQ_CPU, cpu),
			   freq_scale[cpu].curr_scale,
			   per_cpu(sd_pack_buddy, cpu),
			   cpu_rq(cpu)->cfs.nr_running,
			   cputime64_to_clock_t(idle_time_diff));
			 */
			if (jiffies_diff >= idle_time_diff) {
				snprintf(tmp_buf, TMP_BUF_LENGTH,
					 "cpu%d POWER:%04lu BUDDY:%d CFS_RQ:%04u IDLE_TIME_DIFF:%09llu work_load:%04llu\n",
					 cpu, cpu_rq(cpu)->cpu_power, per_cpu(sd_pack_buddy, cpu),
					 cpu_rq(cpu)->cfs.nr_running,
					 cputime64_to_clock_t(idle_time_diff),
					 100 - div_u64(100 * cputime64_to_clock_t(idle_time_diff),
						       jiffies_diff));

				strncat(buf, tmp_buf, TMP_BUF_LENGTH);
			}
		}

	}
	return strlen(buf);
}

static struct kobj_attribute pa_enable_attribute = __ATTR(pa_enable, 0664, pa_show, pa_store);
static struct kobj_attribute pa_stat_attribute = __ATTR(pa_stat, 0444, pa_stat, NULL);
static struct kobj_attribute pa_mon_attribute = __ATTR(pa_mon, 0666, pa_mon_show, pa_mon_store);
static struct kobj_attribute pa_load_attribute = __ATTR(pa_load, 0444, pa_load, NULL);

static struct attribute *attrs[] = {
	&pa_enable_attribute.attr,
	&pa_stat_attribute.attr,
	&pa_mon_attribute.attr,
	&pa_load_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *pa_kobj;

static int __init pa_init(void)
{
	int retval;

	pr_emerg("PA Init\n");

	pa_kobj = kobject_create_and_add("pa", kernel_kobj);

	if (!pa_kobj) {
		return -ENOMEM;
	}

	retval = sysfs_create_group(pa_kobj, &attr_group);

	if (retval) {
		kobject_put(pa_kobj);
	}
	/* Init Global Variable */
	PA_MON_ENABLE = 0;

	strncpy(PA_MON, "", TASK_COMM_LEN);

	return retval;
}

static void __exit pa_exit(void)
{
	pr_emerg("PA Exit\n");
}
module_init(pa_init);
module_exit(pa_exit);
#endif
