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

#include "../../kernel/sched/sched.h"

MODULE_DESCRIPTION("Power-aware Scheduler Controller");
MODULE_AUTHOR("Ted Hu");
MODULE_LICENSE("GPL");

//extern u32 PA_ENABLE;
extern u32 PA_MON_ENABLE;
extern char PA_MON[];

extern u32 PACK_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
extern u32 AVOID_WAKE_UP_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
extern u32 AVOID_LOAD_BALANCE_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
extern u32 TASK_PACK_CPU_COUNT[NR_CPUS];
extern struct cpumask buddy_cpu_map;

//extern int sd_pack_buddy;

//extern unsigned int hmp_up_threshold;
//extern unsigned int hmp_down_threshold;
//extern unsigned int hmp_up_prio;
//extern unsigned int hmp_next_up_threshold;
//extern unsigned int hmp_next_down_threshold;

//extern u32 FREQ_CPU;

//extern int sd_pack_buddy;


//DEFINE_PER_CPU(u64, IDLE_CPU_TIME_DIFF);
DECLARE_PER_CPU(int, sd_pack_buddy);

unsigned int jiffies_orig;
unsigned int jiffies_diff;

static void cut_end(char *input_temp)
{
	int index_temp = 0; 
   
	while (input_temp[index_temp]!='\0') {
		index_temp++;
	}
	
	while (input_temp[index_temp]=='\0' || input_temp[index_temp]==' ' || input_temp[index_temp]=='\n') {
		index_temp--;
	} 
	
	index_temp++;
	input_temp[index_temp]='\0';

}
/*
static ssize_t pa_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "PA_ENABLE = %u\n", PA_ENABLE);
}

static ssize_t pa_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	u32 temp;
	
	sscanf(buf, "%u", &temp);

	printk(KERN_EMERG "Set PA_ENABLE = %u\n", temp);
		
	PA_ENABLE = temp;
	
	return count;
}

static ssize_t lb_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "LB_ENABLE = %u\n", LB_ENABLE);
}

static ssize_t lb_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	u32 temp;
	
	sscanf(buf, "%u", &temp);

	printk(KERN_EMERG "Set LB_ENABLE = %u\n", temp);
		
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
		
	char tmp_buf[TMP_BUF_LENGTH];

	//Jiffies
/*	
	if (jiffies_orig == 0) {
		jiffies_diff = 0;		
	}
	else {
		jiffies_diff = jiffies - jiffies_orig;
	}
	jiffies_orig = jiffies;

	snprintf(tmp_buf, TMP_BUF_LENGTH, "Jiffies Diff = %u\n", jiffies_diff);
	strncat(buf, tmp_buf, TMP_BUF_LENGTH);		
 */ 
  //PACK Status
	for_each_possible_cpu(cpu_from) {
		snprintf(tmp_buf, TMP_BUF_LENGTH, "PACK FORM CPU%d TO", cpu_from);
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);
		
		for_each_possible_cpu(cpu_to) {
			snprintf(tmp_buf, TMP_BUF_LENGTH, " CPU%d:%04u", cpu_to, PACK_FROM_CPUX_TO_CPUY_COUNT[cpu_from][cpu_to]);
			strncat(buf, tmp_buf, TMP_BUF_LENGTH);
			
			//Reset statistic
			PACK_FROM_CPUX_TO_CPUY_COUNT[cpu_from][cpu_to] = 0;			
		}
		
		snprintf(tmp_buf, TMP_BUF_LENGTH, "\n");
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);		
	}

	for_each_possible_cpu(cpu_from) {
		snprintf(tmp_buf, TMP_BUF_LENGTH, "AVOID WAKE UP FORM CPU%d TO", cpu_from);
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);
		
		for_each_possible_cpu(cpu_to) {
			snprintf(tmp_buf, TMP_BUF_LENGTH, " CPU%d:%04u", cpu_to, AVOID_WAKE_UP_FROM_CPUX_TO_CPUY_COUNT[cpu_from][cpu_to]);
			strncat(buf, tmp_buf, TMP_BUF_LENGTH);
			
			//Reset statistic
			AVOID_WAKE_UP_FROM_CPUX_TO_CPUY_COUNT[cpu_from][cpu_to] = 0;				
		}
		
		snprintf(tmp_buf, TMP_BUF_LENGTH, "\n");
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);		
	}	


	for_each_possible_cpu(cpu_from) {
		snprintf(tmp_buf, TMP_BUF_LENGTH, "AVOID BALANCE FORM CPU%d TO", cpu_from);
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);
		
		for_each_possible_cpu(cpu_to) {
			snprintf(tmp_buf, TMP_BUF_LENGTH, " CPU%d:%04u", cpu_to, AVOID_LOAD_BALANCE_FROM_CPUX_TO_CPUY_COUNT[cpu_from][cpu_to]);
			strncat(buf, tmp_buf, TMP_BUF_LENGTH);
			
			//Reset statistic
			AVOID_LOAD_BALANCE_FROM_CPUX_TO_CPUY_COUNT[cpu_from][cpu_to] = 0;				
		}
		
		snprintf(tmp_buf, TMP_BUF_LENGTH, "\n");
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);		
	}
	for_each_online_cpu(cpu_from) {
		snprintf(tmp_buf, TMP_BUF_LENGTH, "CPU%d's buddy is CPU%d\n", cpu_from, per_cpu(sd_pack_buddy, cpu_from));
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);
	}
	for_each_cpu((cpu), &buddy_cpu_map) {
		snprintf(tmp_buf, TMP_BUF_LENGTH, "Buddy CPU is  CPU%d\n", cpu);
		strncat(buf, tmp_buf, TMP_BUF_LENGTH);
	}
	if(PA_MON_ENABLE) {
		if(strlen(PA_MON) != 0) {
			for_each_possible_cpu(cpu) {
				snprintf(tmp_buf, TMP_BUF_LENGTH, "%s pack to cpu%d count is %d\n", PA_MON, cpu, TASK_PACK_CPU_COUNT[cpu]);
				strncat(buf, tmp_buf, TMP_BUF_LENGTH);
				TASK_PACK_CPU_COUNT[cpu] = 0;
			}
		}
	}
	return strlen(buf);
}

static ssize_t pa_mon_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "PA_MON_ENABLE = %u, PA_MON = \"%s\"\n", PA_MON_ENABLE, PA_MON);
}

static ssize_t pa_mon_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	size_t len = strlen(buf);
	u8	i = 0;		
	
	if (len < TASK_COMM_LEN) {
		
		if (len == 1) {
			PA_MON_ENABLE = 0;			
		} else {
			strncpy(PA_MON, buf, TASK_COMM_LEN);
			cut_end(PA_MON);
			PA_MON_ENABLE = 1;
			for(i=0;i<NR_CPUS; i++)
				TASK_PACK_CPU_COUNT[i] = 0;
		}
		printk(KERN_EMERG "Set PA_MON_ENABLE = %u, PA_MON = \"%s\"\n", PA_MON_ENABLE, PA_MON);

	} else {
		printk(KERN_EMERG "Task name over %d\n", TASK_COMM_LEN);
	}
			
	return count;
}


static struct kobj_attribute pa_stat_attribute = __ATTR(pa_stat, 0444, pa_stat, NULL);
static struct kobj_attribute pa_mon_attribute = __ATTR(pa_mon, 0666, pa_mon_show, pa_mon_store);

static struct attribute *attrs[] = {
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
	
	printk(KERN_EMERG "PA Init\n");

	pa_kobj = kobject_create_and_add("pa", kernel_kobj);

	if (!pa_kobj) {
		return -ENOMEM;
	}

	retval = sysfs_create_group(pa_kobj, &attr_group);
	
	if (retval) {
		kobject_put(pa_kobj);
	}

	//Init Global Variable
	PA_MON_ENABLE = 0;
	
	strncpy(PA_MON, "", TASK_COMM_LEN);
		
	return retval;  
}

static void __exit pa_exit(void)
{
	printk(KERN_EMERG "PA Exit\n");
}

module_init(pa_init);
module_exit(pa_exit);
