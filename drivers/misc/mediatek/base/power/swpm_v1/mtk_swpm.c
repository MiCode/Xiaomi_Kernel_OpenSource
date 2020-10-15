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
#include <mtk_swpm_interface.h>

/****************************************************************************
 *  Macro Definitions
 ****************************************************************************/
#define DEFAULT_AVG_WINDOW		(50)
#define IDD_TBL_DBG

#define MAX(a, b)			((a) >= (b) ? (a) : (b))
#define MIN(a, b)			((a) >= (b) ? (b) : (a))

#define SWPM_OPS (swpm_m.plat_ops)
/****************************************************************************
 *  Type Definitions
 ****************************************************************************/
struct swpm_manager {
	bool initialize;
	bool plat_ready;
	struct swpm_mem_ref_tbl *mem_ref_tbl;
	unsigned int ref_tbl_size;
	struct swpm_core_internal_ops *plat_ops;
};

/****************************************************************************
 *  Local Variables
 ****************************************************************************/
static struct swpm_manager swpm_m = {
	.initialize = 0,
	.plat_ready = 0,
	.mem_ref_tbl = NULL,
	.ref_tbl_size = 0,
};

static struct proc_dir_entry *swpm_dir;
static unsigned char avg_window = DEFAULT_AVG_WINDOW;
static unsigned int log_interval_ms = DEFAULT_LOG_INTERVAL_MS;
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
	int i;

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
			swpm_get_avg_power((enum power_rail)i, avg_window));
		if (i != NR_POWER_RAIL - 1)
			ptr += sprintf(ptr, "/");
		else
			ptr += sprintf(ptr, " uA");
	}
	seq_printf(m, "%s\n", buf);

	return 0;
}

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
	int enable_time = 0;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtouint(buf, 10, &enable_time)) {
		swpm_gpu_debug = (enable_time) ? true : false;
		if (swpm_gpu_debug) {
			if (enable_time < 1000000) {
				if (enable_time == 1)
					MTKGPUPower_model_start_swpm(1000000);
				else if (enable_time == 2)
					MTKGPUPower_model_sspm_enable();
			} else
				MTKGPUPower_model_start_swpm(enable_time);
		} else
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
	int type, enable;
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

			if (swpm_timer.function != NULL) {
				expires = jiffies +
					msecs_to_jiffies(log_interval_ms);
				mod_timer(&swpm_timer, expires);
			}
		} else {
			if (swpm_timer.function != NULL)
				del_timer(&swpm_timer);
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
	int type, cnt;
#endif
	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	if (sscanf(buf, "%d %d", &type, &cnt) == 2)
		swpm_set_update_cnt(type, cnt);
	else
		swpm_err("echo <type or 65535> <cnt> > /proc/swpm/update_cnt\n");
#endif

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
	seq_printf(m, "Current log mask is 0x%x\n", swpm_log_mask);

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
		swpm_log_mask = mask;
	else
		swpm_err("echo <mask> > /proc/swpm/log_mask\n");

	return count;
}

PROC_FOPS_RO(dump_power);
PROC_FOPS_RO(dump_lkg_power);
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

static int swpm_core_ops_ready_chk(void)
{
	bool func_ready = false;
	struct swpm_core_internal_ops *ops_chk = swpm_m.plat_ops;

	if (ops_chk &&
	    ops_chk->cmd)
		func_ready = true;

	return func_ready;
}
/***************************************************************************
 *  API
 ***************************************************************************/
int swpm_core_ops_register(struct swpm_core_internal_ops *ops)
{
	if (!swpm_m.plat_ops && ops) {
		swpm_m.plat_ops = ops;
		swpm_m.plat_ready = swpm_core_ops_ready_chk();
	} else
		return -1;

	return 0;
}

#undef swpm_pmu_enable
int swpm_pmu_enable(enum swpm_pmu_user id,
		    unsigned int enable)
{
	unsigned int cmd_code;

	if (!swpm_m.plat_ready)
		return SWPM_INIT_ERR;
	else if (id >= NR_SWPM_PMU_USER)
		return SWPM_ARGS_ERR;

	cmd_code = (!!enable) | (id << SWPM_CODE_USER_BIT);
	SWPM_OPS->cmd(SET_PMU, cmd_code);

	return SWPM_SUCCESS;
}

int swpm_append_procfs(struct swpm_entry *p)
{
	if (!swpm_dir) {
		swpm_err("[%s] /proc/swpm failed creation\n", __func__);
		return -1;
	}
	if (!p) {
		swpm_err("[%s] append failure, fp null\n", __func__);
		return -1;
	}

	if (!proc_create(p->name, 0664, swpm_dir, p->fops)) {
		swpm_err("[%s]: append /proc/swpm/%s failed\n",
			__func__, p->name);
		return -1;
	}

	return 0;
}

int swpm_create_procfs(void)
{
	int i = 0;

	struct swpm_entry swpm_entries[] = {
		PROC_ENTRY(dump_power),
		PROC_ENTRY(dump_lkg_power),
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

int swpm_set_periodic_timer(void (*func)(unsigned long))
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

unsigned int swpm_get_avg_power(enum power_rail type, unsigned int avg_window)
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

