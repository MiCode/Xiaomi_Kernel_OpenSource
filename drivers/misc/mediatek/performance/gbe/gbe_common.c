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
#include <linux/sched/clock.h>
#include <linux/sched/mm.h>
#include <linux/sched/numa_balancing.h>
#include <linux/sched/task_stack.h>
#include <linux/sched/task.h>
#include <linux/sched/cputime.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/kallsyms.h>
#include <linux/trace_events.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>   /* for misc_register, and SYNTH_MINOR */
#include <linux/proc_fs.h>
#include "cpu_ctrl.h"
#include "eas_ctrl.h"

#include <linux/workqueue.h>
#include <linux/unistd.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/pm_qos.h>
#include "gbe_common.h"
#include "gbe1.h"
#include "gbe2.h"
#include "gbe_sysfs.h"

static DEFINE_MUTEX(gbe_lock);
static int cluster_num;
static struct pm_qos_request dram_req;
static int boost_set[KIR_NUM];
static unsigned long policy_mask;
static struct ppm_limit_data *pld;

enum GBE_BOOST_DEVICE {
	GBE_BOOST_CPU,
	GBE_BOOST_EAS,
	GBE_BOOST_VCORE,
	GBE_BOOST_IO,
	GBE_BOOST_HE,
	GBE_BOOST_GPU,
	GBE_BOOST_LLF,
	GBE_BOOST_NUM,
};

static unsigned long __read_mostly tracing_mark_write_addr;
static inline void __mt_update_tracing_mark_write_addr(void)
{
	if (unlikely(tracing_mark_write_addr == 0))
		tracing_mark_write_addr =
			kallsyms_lookup_name("tracing_mark_write");
}
void gbe_trace_printk(int pid, char *module, char *string)
{
	__mt_update_tracing_mark_write_addr();
	preempt_disable();
	event_trace_printk(tracing_mark_write_addr, "%d [%s] %s\n",
			pid, module, string);
	preempt_enable();
}

void gbe_trace_count(int tid, int val, const char *fmt, ...)
{
	char log[32];
	va_list args;
	int len;


	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 32))
		log[31] = '\0';

	__mt_update_tracing_mark_write_addr();
	preempt_disable();

	if (!strstr(CONFIG_MTK_PLATFORM, "mt8")) {
		event_trace_printk(tracing_mark_write_addr, "C|%d|%s|%d\n",
				tid, log, val);
	} else {
		event_trace_printk(tracing_mark_write_addr, "C|%s|%d\n",
				log, val);
	}

	preempt_enable();
}

static struct miscdevice gbe_object;
bool sentuevent(const char *src)
{
	int ret;
	char *envp[2];
	int string_size = 15;
	char  event_string[string_size];

	envp[0] = event_string;
	envp[1] = NULL;


	/*send uevent*/
	strlcpy(event_string, src, string_size);
	if (event_string[0] == '\0') { /*string is null*/

		return false;
	}
	ret = kobject_uevent_env(
			&gbe_object.this_device->kobj,
			KOBJ_CHANGE, envp);
	if (ret != 0) {
		pr_debug("uevent failed");

		return false;
	}

	return true;
}

void gbe_boost(enum GBE_KICKER kicker, int boost)
{
	int uclamp_pct, pm_req;
	int i;
	int boost_final = 0;
	char u_io_string[11];
	char u_boost_string[12];
	char u_gpu_string[12];
	char u_llf_string[12];

	if (!pld)
		return;

	mutex_lock(&gbe_lock);

	if (boost_set[kicker] == !!boost)
		goto out;

	boost_set[kicker] = !!boost;

	for (i = 0; i < KIR_NUM; i++)
		if (boost_set[i] == 1) {
			boost_final = 1;
			break;
		}


	if (!pm_qos_request_active(&dram_req))
		pm_qos_add_request(&dram_req, PM_QOS_DDR_OPP,
				PM_QOS_DDR_OPP_DEFAULT_VALUE);

	if (boost_final) {
		for (i = 0; i < cluster_num; i++) {
			pld[i].max = 3000000;
			pld[i].min = 3000000;
		}
		uclamp_pct = 100;
		pm_req = 0;
		strncpy(u_io_string, "IO_BOOST=1", 11);
		strncpy(u_gpu_string, "GPU_BOOST=1", 12);
		strncpy(u_boost_string, "GBE_BOOST=1", 12);
		strncpy(u_llf_string, "LLF_BOOST=1", 12);
	} else {
		for (i = 0; i < cluster_num; i++) {
			pld[i].max = -1;
			pld[i].min = -1;
		}
		uclamp_pct = 0;
		pm_req = PM_QOS_DDR_OPP_DEFAULT_VALUE;
		strncpy(u_io_string, "IO_BOOST=0", 11);
		strncpy(u_gpu_string, "GPU_BOOST=0", 12);
		strncpy(u_boost_string, "GBE_BOOST=0", 12);
		strncpy(u_llf_string, "LLF_BOOST=0", 12);
	}

	if (test_bit(GBE_BOOST_CPU, &policy_mask))
		update_userlimit_cpu_freq(CPU_KIR_GBE, cluster_num, pld);

	if (test_bit(GBE_BOOST_EAS, &policy_mask))
		update_eas_uclamp_min(EAS_UCLAMP_KIR_GBE,
			CGROUP_TA, uclamp_pct);

	if (test_bit(GBE_BOOST_VCORE, &policy_mask)) {
		if (pm_qos_request_active(&dram_req))
			pm_qos_update_request(&dram_req, pm_req);
	}

	if (test_bit(GBE_BOOST_IO, &policy_mask))
		sentuevent(u_io_string);

	if (test_bit(GBE_BOOST_HE, &policy_mask))
		sentuevent(u_boost_string);

	if (test_bit(GBE_BOOST_GPU, &policy_mask))
		sentuevent(u_gpu_string);

	if (test_bit(GBE_BOOST_LLF, &policy_mask))
		sentuevent(u_llf_string);

out:
	mutex_unlock(&gbe_lock);

}

static ssize_t gbe_policy_mask_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", policy_mask);
}

static ssize_t gbe_policy_mask_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = 0;
	char acBuffer[GBE_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < GBE_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GBE_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				return count;
		}
	}

	if (val > 1 << GBE_BOOST_NUM || val < 0)
		return count;

	if (!pld)
		return count;

	{
		int i;
		char u_io_string[11];
		char u_gpu_string[12];
		char u_llf_string[12];
		char u_boost_string[12];

		strncpy(u_io_string, "IO_BOOST=0", 11);
		strncpy(u_gpu_string, "GPU_BOOST=0", 12);
		strncpy(u_boost_string, "GBE_BOOST=0", 12);
		strncpy(u_llf_string, "GPU_BOOST=0", 12);
		for (i = 0; i < cluster_num; i++) {
			pld[i].max = -1;
			pld[i].min = -1;
		}

		mutex_lock(&gbe_lock);
		policy_mask = val;

		update_userlimit_cpu_freq(CPU_KIR_GBE, cluster_num, pld);
		update_eas_uclamp_min(EAS_UCLAMP_KIR_GBE, CGROUP_TA, 0);
		if (pm_qos_request_active(&dram_req))
			pm_qos_update_request(&dram_req, PM_QOS_DDR_OPP_DEFAULT_VALUE);
		sentuevent(u_io_string);
		sentuevent(u_gpu_string);
		sentuevent(u_llf_string);
		sentuevent(u_boost_string);

		mutex_unlock(&gbe_lock);
	}

	return count;
}

static KOBJ_ATTR_RW(gbe_policy_mask);

static int init_gbe_kobj(void)
{
	int ret = 0;

	/* dev init */

	gbe_object.name = "gbe";
	gbe_object.minor = MISC_DYNAMIC_MINOR;
	ret = misc_register(&gbe_object);
	if (ret) {
		pr_debug("misc_register error:%d\n", ret);
		return ret;
	}

	ret = kobject_uevent(
			&gbe_object.this_device->kobj, KOBJ_ADD);

	if (ret) {
		misc_deregister(&gbe_object);
		pr_debug("uevent creat fail:%d\n", ret);
		return ret;
	}

	return ret;

}

static void __exit gbe_common_exit(void)
{
	gbe_sysfs_remove_file(&kobj_attr_gbe_policy_mask);
	gbe_sysfs_exit();
}

struct dentry *gbe_debugfs_dir;
static int __init gbe_common_init(void)
{
	int ret = 0;

	gbe_sysfs_init();
	gbe1_init();
	gbe2_init();

	gbe_sysfs_create_file(&kobj_attr_gbe_policy_mask);

	cluster_num = arch_get_nr_clusters();
	pld = kcalloc(cluster_num,
			sizeof(struct ppm_limit_data),
			GFP_KERNEL);

	set_bit(GBE_BOOST_CPU, &policy_mask);
	set_bit(GBE_BOOST_EAS, &policy_mask);
	set_bit(GBE_BOOST_VCORE, &policy_mask);
	set_bit(GBE_BOOST_HE, &policy_mask);

	ret = init_gbe_kobj();
	if (ret) {
		pr_debug("init gbe_kobj failed");
		return ret;
	}

	return 0;
}

module_init(gbe_common_init);
module_exit(gbe_common_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek GBE");
MODULE_AUTHOR("MediaTek Inc.");
