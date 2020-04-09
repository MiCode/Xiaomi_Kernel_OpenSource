/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/ktime.h>
#include <trace/events/mtk_events.h>
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#include <sspm_reservedmem_define.h>
#endif
#ifdef CONFIG_MTK_DRAMC
#include <mtk_dramc.h>
#endif
#ifdef CONFIG_MTK_GPU_SWPM_SUPPORT
#include <mtk_gpu_power_sspm_ipi.h>
#endif
#include <mtk_swpm_common.h>
#include <mtk_swpm_platform.h>
#include <mtk_swpm.h>
#include <swpm_v1/mtk_swpm_interface.h>
/****************************************************************************
 *  Macro Definitions
 ****************************************************************************/
#define DEFAULT_AVG_WINDOW		(50)
/* #define LOG_LOOP_TIME_PROFILE */
/* #define IDD_TBL_DBG */

#define MAX(a, b)			((a) >= (b) ? (a) : (b))
#define MIN(a, b)			((a) >= (b) ? (b) : (a))

/* PROCFS */
#define PROC_FOPS_RW(name)                                                 \
	static int name ## _proc_open(struct inode *inode,                 \
		struct file *file)                                         \
	{                                                                  \
		return single_open(file, name ## _proc_show,               \
			PDE_DATA(inode));                                  \
	}                                                                  \
	static const struct file_operations name ## _proc_fops = {         \
		.owner      = THIS_MODULE,                                 \
		.open       = name ## _proc_open,                          \
		.read	    = seq_read,                                    \
		.llseek	    = seq_lseek,                                   \
		.release    = single_release,                              \
		.write      = name ## _proc_write,                         \
	}
#define PROC_FOPS_RO(name)                                                 \
	static int name ## _proc_open(struct inode *inode,                 \
		struct file *file)                                         \
	{                                                                  \
		return single_open(file, name ## _proc_show,               \
			PDE_DATA(inode));                                  \
	}                                                                  \
	static const struct file_operations name ## _proc_fops = {         \
		.owner = THIS_MODULE,                                      \
		.open  = name ## _proc_open,                               \
		.read  = seq_read,                                         \
		.llseek = seq_lseek,                                       \
		.release = single_release,                                 \
	}
#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

/****************************************************************************
 *  Type Definitions
 ****************************************************************************/
struct swpm_manager {
	bool initialize;
	struct swpm_mem_ref_tbl *mem_ref_tbl;
	unsigned int ref_tbl_size;
};

/****************************************************************************
 *  Local Variables
 ****************************************************************************/
static struct swpm_manager swpm_m = {
	.initialize = 0,
	.mem_ref_tbl = NULL,
	.ref_tbl_size = 0,
};

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
static phys_addr_t rec_phys_addr, rec_virt_addr;
static unsigned long long rec_size;
#endif
static unsigned char avg_window = DEFAULT_AVG_WINDOW;
static struct timer_list log_timer;
static unsigned int log_interval_ms = DEFAULT_LOG_INTERVAL_MS;
static unsigned int log_mask = DEFAULT_LOG_MASK;

/****************************************************************************
 *  Global Variables
 ****************************************************************************/
/* swpm periodic timer for ftrace output */
unsigned int swpm_log_mask = DEFAULT_LOG_MASK;
struct timer_list swpm_timer;
struct swpm_rec_data *swpm_info_ref;
unsigned int swpm_status;
bool swpm_debug;
#ifdef CONFIG_MTK_GPU_SWPM_SUPPORT
bool swpm_gpu_debug;
#endif
DEFINE_MUTEX(swpm_mutex);

/****************************************************************************
 *  Static Function
 ****************************************************************************/
static char *_copy_from_user_for_proc(const char __user *buffer, size_t count)
{
	static char buf[64];
	unsigned int len = 0;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);

	if (copy_from_user(buf, buffer, len))
		return NULL;

	buf[len] = '\0';

	return buf;
}

static int dump_power_proc_show(struct seq_file *m, void *v)
{
	char buf[256];
	char *ptr = buf;
	unsigned int i;

	for (i = 0; i < NR_POWER_RAIL; i++) {
		ptr += snprintf(ptr, 256, "%s",
			swpm_power_rail_to_string((enum power_rail)i));
		if (i != NR_POWER_RAIL - 1)
			ptr += sprintf(ptr, "/");
		else
			ptr += sprintf(ptr, " = ");
	}

	for (i = 0; i < NR_POWER_RAIL; i++) {
		ptr += snprintf(ptr, 256, "%d",
			swpm_get_avg_power(i, avg_window));
		if (i != NR_POWER_RAIL - 1)
			ptr += sprintf(ptr, "/");
		else
			ptr += sprintf(ptr, " uA");
	}

	seq_printf(m, "%s\n", buf);

	return 0;
}

#ifndef CPU_LKG_NOT_SUPPORT
static int dump_lkg_power_proc_show(struct seq_file *m, void *v)
{
	int i, j;

	if (!swpm_info_ref)
		return 0;

	for (i = 0; i < NR_CPU_LKG_TYPE; i++) {
		for (j = 0; j < 16; j++) {
			seq_printf(m, "type %d opp%d lkg = %d\n", i, j,
				swpm_info_ref->cpu_lkg_pwr[i][j]);
		}
	}

	return 0;
}
#endif

#ifdef CONFIG_MTK_GPU_SWPM_SUPPORT
static int gpu_debug_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "\nSWPM gpu_debug is %s\n",
		(swpm_gpu_debug == true) ? "enabled" : "disabled");

	if (swpm_gpu_debug == true) {
		seq_printf(m, "gpu freq urate : %u\n",
			swpm_info_ref->gpu_reserved[gfreq + 1]);
		seq_printf(m, "gpu volt : %u\n",
			swpm_info_ref->gpu_reserved[gvolt + 1]);
		seq_printf(m, "gpu loading : %u\n",
			swpm_info_ref->gpu_reserved[gloading + 1]);
		seq_printf(m, "alu fma urate : %u\n",
			swpm_info_ref->gpu_reserved[galu_fma_urate + 1]);
		seq_printf(m, "tex urate : %u\n",
			swpm_info_ref->gpu_reserved[gtex_urate + 1]);
		seq_printf(m, "lsc urate : %u\n",
			swpm_info_ref->gpu_reserved[glsc_urate + 1]);
		seq_printf(m, "l2c urate : %u\n",
			swpm_info_ref->gpu_reserved[gl2c_urate + 1]);
		seq_printf(m, "vary urate : %u\n",
			swpm_info_ref->gpu_reserved[gvary_urate + 1]);
		seq_printf(m, "tiler urate : %u\n",
			swpm_info_ref->gpu_reserved[gtiler_urate + 1]);
	}

	return 0;
}

static ssize_t gpu_debug_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	int enable = 0;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtouint(buf, 10, &enable)) {
		swpm_gpu_debug = (enable) ? true : false;
		if (swpm_gpu_debug)
			MTKGPUPower_model_start(1000000);
		else
			MTKGPUPower_model_stop();
	} else {
		swpm_err("echo 1/0 > /proc/swpm/debug\n");
	}
	return count;
}
#endif

static int debug_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "\nSWPM debug is %s\n",
		(swpm_debug == true) ? "enabled" : "disabled");

	return 0;
}

static ssize_t debug_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	int enable = 0;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtouint(buf, 10, &enable))
		swpm_debug = (enable) ? true : false;
	else
		swpm_err("echo 1/0 > /proc/swpm/debug\n");

	return count;
}

static int enable_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "\nSWPM status = 0x%x\n", swpm_status);

	return 0;
}

static ssize_t enable_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	int type = 0, enable = 0;
#endif
	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	if (sscanf(buf, "%d %d", &type, &enable) == 2) {
		swpm_lock(&swpm_mutex);
		swpm_set_enable(type, enable);
		if (swpm_status) {
			unsigned long expires;

			if (log_timer.function != NULL) {
				expires = jiffies +
					msecs_to_jiffies(log_interval_ms);
				mod_timer(&log_timer, expires);
			}
		} else {
			if (log_timer.function != NULL)
				del_timer(&log_timer);
		}
		swpm_unlock(&swpm_mutex);
	} else {
		swpm_err("echo <type or 65535> <0 or 1> > /proc/swpm/enable\n");
	}
#endif

	return count;
}

static int update_cnt_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static ssize_t update_cnt_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	int type = 0, cnt = 0;
#endif
	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	swpm_lock(&swpm_mutex);
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	if (sscanf(buf, "%d %d", &type, &cnt) == 2)
		swpm_set_update_cnt(type, cnt);
	else
		swpm_err("echo <type or 65535> <cnt> > /proc/swpm/update_cnt\n");
#endif
	swpm_unlock(&swpm_mutex);

	return count;
}

static int profile_proc_show(struct seq_file *m, void *v)
{
	if (!swpm_info_ref)
		return 0;

	seq_printf(m, "monitor time avg/max = %llu/%llu ns, cnt = %llu\n",
		swpm_info_ref->avg_latency[MON_TIME],
		swpm_info_ref->max_latency[MON_TIME],
		swpm_info_ref->prof_cnt[MON_TIME]);
	seq_printf(m, "calculate time avg/max = %llu/%llu ns, cnt = %llu\n",
		swpm_info_ref->avg_latency[CALC_TIME],
		swpm_info_ref->max_latency[CALC_TIME],
		swpm_info_ref->prof_cnt[CALC_TIME]);
	seq_printf(m, "proc record time avg/max = %llu/%llu ns, cnt = %llu\n",
		swpm_info_ref->avg_latency[REC_TIME],
		swpm_info_ref->max_latency[REC_TIME],
		swpm_info_ref->prof_cnt[REC_TIME]);
	seq_printf(m, "total time avg/max = %llu/%llu ns, cnt = %llu\n",
		swpm_info_ref->avg_latency[TOTAL_TIME],
		swpm_info_ref->max_latency[TOTAL_TIME],
		swpm_info_ref->prof_cnt[TOTAL_TIME]);

	seq_printf(m, "\nSWPM profile is %s\n",
		(swpm_info_ref->profile_enable) ? "enabled" : "disabled");

	return 0;
}

static ssize_t profile_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	int enable = 0;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!swpm_info_ref)
		goto end;

	if (!kstrtouint(buf, 10, &enable))
		swpm_info_ref->profile_enable = enable;
	else
		swpm_err("echo <1/0> > /proc/swpm/profile\n");

end:
	return count;
}

static int avg_window_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "Current Avg Window is %d\n",
		MIN(MAX_RECORD_CNT, avg_window));

	return 0;
}

static ssize_t avg_window_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int window = 0;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtouint(buf, 10, &window))
		avg_window = MIN(MAX_RECORD_CNT, window);
	else
		swpm_err("echo <window> > /proc/swpm/avg_window\n");

	return count;
}

static int log_interval_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "Current log interval is %d ms\n", log_interval_ms);

	return 0;
}

static ssize_t log_interval_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int interval = 0;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtouint(buf, 10, &interval))
		log_interval_ms = interval;
	else
		swpm_err("echo <interval_ms> > /proc/swpm/log_interval\n");

	return count;
}

static int log_mask_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "Current log mask is 0x%x\n", log_mask);

	return 0;
}

static ssize_t log_mask_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int mask = 0;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtouint(buf, 10, &mask))
		log_mask = mask;
	else
		swpm_err("echo <mask> > /proc/swpm/log_mask\n");

	return count;
}

#ifdef IDD_TBL_DBG
static int idd_tbl_proc_show(struct seq_file *m, void *v)
{
	int i;

	if (!swpm_info_ref)
		return 0;

	for (i = 0; i < NR_DRAM_PWR_TYPE; i++) {
		seq_puts(m, "==========================\n");
		seq_printf(m, "idx %d i_dd0 = %d\n", i,
			swpm_info_ref->dram_conf[i].i_dd0);
		seq_printf(m, "idx %d i_dd2p = %d\n", i,
			swpm_info_ref->dram_conf[i].i_dd2p);
		seq_printf(m, "idx %d i_dd2n = %d\n", i,
			swpm_info_ref->dram_conf[i].i_dd2n);
		seq_printf(m, "idx %d i_dd4r = %d\n", i,
			swpm_info_ref->dram_conf[i].i_dd4r);
		seq_printf(m, "idx %d i_dd4w = %d\n", i,
			swpm_info_ref->dram_conf[i].i_dd4w);
		seq_printf(m, "idx %d i_dd5 = %d\n", i,
			swpm_info_ref->dram_conf[i].i_dd5);
		seq_printf(m, "idx %d i_dd6 = %d\n", i,
			swpm_info_ref->dram_conf[i].i_dd6);
	}

	seq_puts(m, "==========================\n");

	return 0;
}

static ssize_t idd_tbl_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int type = 0, idd_idx = 0, val = 0;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!swpm_info_ref)
		goto end;

	if (sscanf(buf, "%d %d %d", &type, &idd_idx, &val) == 3) {
		if (type >= NR_DRAM_PWR_TYPE || idd_idx > 6)
			goto end;
		swpm_lock(&swpm_mutex);
		*(&swpm_info_ref->dram_conf[type].i_dd0 + idd_idx) = val;
		swpm_unlock(&swpm_mutex);
	} else {
		swpm_err("echo <type> <idx> <val> > /proc/swpm/idd_tbl\n");
	}

end:
	return count;
}
#endif

PROC_FOPS_RO(dump_power);
#ifndef CPU_LKG_NOT_SUPPORT
PROC_FOPS_RO(dump_lkg_power);
#endif
#ifdef CONFIG_MTK_GPU_SWPM_SUPPORT
PROC_FOPS_RW(gpu_debug);
#endif
PROC_FOPS_RW(debug);
PROC_FOPS_RW(enable);
PROC_FOPS_RW(update_cnt);
PROC_FOPS_RW(profile);
PROC_FOPS_RW(avg_window);
PROC_FOPS_RW(log_interval);
PROC_FOPS_RW(log_mask);
#ifdef IDD_TBL_DBG
PROC_FOPS_RW(idd_tbl);
#endif

int swpm_create_procfs(void)
{
	struct proc_dir_entry *swpm_dir = NULL;
	int i = 0;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry swpm_entries[] = {
		PROC_ENTRY(dump_power),
#ifndef CPU_LKG_NOT_SUPPORT
		PROC_ENTRY(dump_lkg_power),
#endif
		PROC_ENTRY(debug),
		PROC_ENTRY(enable),
		PROC_ENTRY(update_cnt),
		PROC_ENTRY(profile),
		PROC_ENTRY(avg_window),
		PROC_ENTRY(log_interval),
		PROC_ENTRY(log_mask),
#ifdef CONFIG_MTK_GPU_SWPM_SUPPORT
		PROC_ENTRY(gpu_debug),
#endif
#ifdef IDD_TBL_DBG
		PROC_ENTRY(idd_tbl),
#endif
	};

	swpm_dir = proc_mkdir("swpm", NULL);
	if (!swpm_dir) {
		swpm_err("[%s] mkdir /proc/swpm failed\n", __func__);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(swpm_entries); i++) {
		if (!proc_create(swpm_entries[i].name,
			0664,
			swpm_dir, swpm_entries[i].fops)) {
			swpm_err("[%s]: create /proc/swpm/%s failed\n",
				__func__, swpm_entries[i].name);
			return -1;
		}
	}

	return 0;
}

void swpm_get_rec_addr(phys_addr_t *phys,
		       phys_addr_t *virt,
		       unsigned long long *size)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	/* get sspm reserved mem */
	*phys = sspm_reserve_mem_get_phys(SWPM_MEM_ID);
	*virt = sspm_reserve_mem_get_virt(SWPM_MEM_ID);
	*size = sspm_reserve_mem_get_size(SWPM_MEM_ID);

	swpm_info("phy_addr = 0x%llx, virt_addr=0x%llx, size = %llu\n",
		(unsigned long long) *phys,
		(unsigned long long) *virt,
		*size);

#endif
}

static void get_rec_addr(void)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	int i;
	unsigned char *ptr;

	/* get sspm reserved mem */
	rec_phys_addr = sspm_reserve_mem_get_phys(SWPM_MEM_ID);
	rec_virt_addr = sspm_reserve_mem_get_virt(SWPM_MEM_ID);
	rec_size = sspm_reserve_mem_get_size(SWPM_MEM_ID);

	swpm_info("phy_addr = 0x%llx, virt_addr=0x%llx, size = %llu\n",
		(unsigned long long)rec_phys_addr,
		(unsigned long long)rec_virt_addr,
		rec_size);

	/* clear */
	ptr = (unsigned char *)(uintptr_t)rec_virt_addr;
	for (i = 0; i < rec_size; i++)
		ptr[i] = 0x0;

	swpm_info_ref = (struct swpm_rec_data *)(uintptr_t)rec_virt_addr;
#endif
}

static int log_loop(void)
{
	unsigned long expires;
	char buf[256] = {0};
	char *ptr = buf;
	unsigned int i;
#ifdef LOG_LOOP_TIME_PROFILE
	ktime_t t1, t2;
	unsigned long long diff, diff2;

	t1 = ktime_get();
#endif

	for (i = 0; i < NR_POWER_RAIL; i++) {
		if ((1 << i) & log_mask) {
			ptr += snprintf(ptr, 256, "%s/",
				swpm_power_rail_to_string((enum power_rail)i));
		}
	}
	ptr--;
	ptr += sprintf(ptr, " = ");

	for (i = 0; i < NR_POWER_RAIL; i++) {
		if ((1 << i) & log_mask) {
			ptr += snprintf(ptr, 256, "%d/",
				swpm_get_avg_power(i, 50));
		}
	}
	ptr--;
	ptr += sprintf(ptr, " uA");

	trace_swpm_power(buf);
#ifdef LOG_LOOP_TIME_PROFILE
	t2 = ktime_get();
#endif

	swpm_update_lkg_table();

#ifdef LOG_LOOP_TIME_PROFILE
	diff = ktime_to_us(ktime_sub(t2, t1));
	diff2 = ktime_to_us(ktime_sub(ktime_get(), t2));
	swpm_err("exe time = %llu/%lluus\n", diff, diff2);
#endif

	expires = jiffies + msecs_to_jiffies(log_interval_ms);
	mod_timer(&log_timer, expires);

	return 0;
}

int swpm_reserve_mem_init(phys_addr_t *virt,
			   unsigned long long *size)
{
	int i;
	unsigned char *ptr;

	if (!virt)
		return -1;

	/* clear reserve mem */
	ptr = (unsigned char *)(uintptr_t)*virt;
	for (i = 0; i < *size; i++)
		ptr[i] = 0x0;

	return 0;
}

int swpm_interface_manager_init(struct swpm_mem_ref_tbl *ref_tbl,
				unsigned int tbl_size)
{
	if (!ref_tbl)
		return -1;

	swpm_lock(&swpm_mutex);
	swpm_m.initialize = true;
	swpm_m.mem_ref_tbl = ref_tbl;
	swpm_m.ref_tbl_size = tbl_size;
	swpm_unlock(&swpm_mutex);

	return 0;
}

int swpm_set_periodic_timer(void *func)
{
	swpm_lock(&swpm_mutex);

	if (func != NULL) {
		swpm_timer.function = func;
		swpm_timer.data = (unsigned long)&swpm_timer;
		init_timer_deferrable(&swpm_timer);
	}
	swpm_unlock(&swpm_mutex);

	return 0;
}

void swpm_update_periodic_timer(void)
{
	mod_timer(&swpm_timer, jiffies + msecs_to_jiffies(log_interval_ms));
}

int swpm_mem_addr_request(enum swpm_type id, phys_addr_t **ptr)
{
	int ret = 0;

	if (!swpm_m.initialize || !swpm_m.mem_ref_tbl) {
		swpm_err("swpm not initialize\n");
		ret = -1;
		goto end;
	} else if (id >= swpm_m.ref_tbl_size) {
		swpm_err("swpm_type invalid\n");
		ret = -2;
		goto end;
	} else if (!(swpm_m.mem_ref_tbl[id].valid)
		   || !(swpm_m.mem_ref_tbl[id].virt)) {
		ret = -3;
		swpm_err("swpm_mem_ref id not initialize\n");
		goto end;
	}

	swpm_lock(&swpm_mutex);
	*ptr = (swpm_m.mem_ref_tbl[id].virt);
	swpm_unlock(&swpm_mutex);

end:
	return ret;
}

static int __init swpm_init(void)
{
#ifdef BRINGUP_DISABLE
	swpm_err("swpm is disabled\n");
	goto end;
#endif
	get_rec_addr();
	if (!swpm_info_ref) {
		swpm_err("get sspm dram addr failed\n");
		goto end;
	}
	swpm_create_procfs();

	swpm_platform_init();

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#ifdef CONFIG_MTK_DRAMC
	swpm_send_init_ipi((unsigned int)(rec_phys_addr & 0xFFFFFFFF),
		(unsigned int)(rec_size & 0xFFFFFFFF), get_emi_ch_num());
#else
	swpm_send_init_ipi((unsigned int)(rec_phys_addr & 0xFFFFFFFF),
		(unsigned int)(rec_size & 0xFFFFFFFF), 2);
#endif
#endif

	/* init log timer */
	init_timer_deferrable(&log_timer);
	log_timer.function = (void *)&log_loop;
	log_timer.data = (unsigned long)&log_timer;

	swpm_info("SWPM init done!\n");

end:
	return 0;
}
late_initcall(swpm_init);

/***************************************************************************
 *  API
 ***************************************************************************/
unsigned int swpm_get_avg_power(unsigned int type, unsigned int avg_window)
{
	unsigned int *ptr;
	unsigned int cnt, idx, sum = 0, pwr = 0;

	if (type >= NR_POWER_RAIL) {
		swpm_err("Invalid SWPM type = %d\n", type);
		return 0;
	}

	/* window should be 1 to MAX_RECORD_CNT */
	avg_window = MAX(avg_window, 1);
	avg_window = MIN(avg_window, MAX_RECORD_CNT);

	/* get ptr of the target meter record */
	ptr = &swpm_info_ref->pwr[type][0];

	/* calculate avg */
	for (idx = swpm_info_ref->cur_idx, cnt = 0; cnt < avg_window; cnt++) {
		sum += ptr[idx];

		if (!idx)
			idx = MAX_RECORD_CNT - 1;
		else
			idx--;
	}

	pwr = sum / avg_window;

	swpm_dbg("avg pwr of meter %d = %d uA\n", type, pwr);

	return pwr;
}
EXPORT_SYMBOL(swpm_get_avg_power);

