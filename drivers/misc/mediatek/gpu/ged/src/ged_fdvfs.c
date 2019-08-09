/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "ged_fdvfs.h"
#ifdef GED_SSPM
#include <sspm_reservedmem_define.h>
#endif
#include <linux/types.h>
GED_LOG_BUF_HANDLE ghLogBuf_FDVFS;

static volatile void *g_MFG_base;
static int mfg_is_power_on;
unsigned long long g_timestamp;


/* ****************************************** */

GED_FDVFS_COUNTER counters[] = {
	{"RGX_PERF_TA_OR_3D_CYCLES", RGX_PERF_TA_OR_3D_CYCLES, 0u, 0u, 0u, 0, 0},
	{"RGX_PERF_3D_CYCLES",  RGX_PERF_3D_CYCLES, 0u, 0u, 0u, 0, 0},
};

#define FDVFS_COUNTER_SIZE (ARRAY_SIZE(counters))
#define FDVFS_SAMPLE_TIME 1000000


void fdvfs_print_info(void)
{
	unsigned long long stamp;

	stamp = ged_get_time();
	ged_log_buf_print(ghLogBuf_FDVFS, "[ts=%llu]\n", stamp);

	ged_log_trace_counter("GPU_active_raw", counters[MTK_CNT_TA_OR_3D].val_pre);
	ged_log_trace_counter("GPU_active_diff", counters[MTK_CNT_TA_OR_3D].diff_pre);

	ged_log_trace_counter("3D_active_raw", counters[MTK_CNT_3D].val_pre);
	ged_log_trace_counter("3D_active_diff", counters[MTK_CNT_3D].diff_pre);

	ged_log_trace_counter("3D_tick", counters[MTK_CNT_3D].tick_time);
}

void fdvfs_counter_update(void)
{
	int i;
	uint32_t val, diff;

	for (i = 0; i < FDVFS_COUNTER_SIZE; ++i) {

		val = MFG_read(counters[i].offset);

		if (val >= counters[i].val_pre)
			diff = val - counters[i].val_pre;
		else
			diff = val;

		if (diff)
			counters[i].tick_time++;

		if (counters[i].sum + diff < counters[i].sum) {
			counters[i].overflow = 1;
			counters[i].sum = (uint32_t)-1;
		} else if (counters[i].overflow == 0)
			counters[i].sum += diff;

		counters[i].val_pre = val;
		counters[i].diff_pre = diff;
	}

	/* fdvfs_print_info(); */
}

void fdvfs_counter_reset(void)
{
	int i;

	for (i = 0; i < FDVFS_COUNTER_SIZE; ++i) {
		counters[i].sum = 0u;
		counters[i].tick_time = 0;
	}
}

void mtk_gpu_dvfs_hint(unsigned int hint)
{
#ifdef GED_SSPM
#if defined(__LP64__) && defined(__aarch64__)
	ged_log_buf_print(ghLogBuf_FDVFS, "[%d]", (unsigned int)hint);
	*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_FDVFS_DVFS_HINT) = hint;
#endif
#endif
}
EXPORT_SYMBOL(mtk_gpu_dvfs_hint);


void mtk_gpu_ged_hint(int t_gpu_target, int boost_accum_gpu)
{
	/* ged_log_buf_print(ghLogBuf_FDVFS, "[%d]", (unsigned int)hint ); */
#ifdef GED_SSPM
#if defined(__LP64__) && defined(__aarch64__)
	*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_KPI_FPS_HINT)  = t_gpu_target;
	*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_KPI_ACCM_HINT) = boost_accum_gpu;
#endif
#endif
}
EXPORT_SYMBOL(mtk_gpu_ged_hint);


void mtk_gpu_gas_hint(unsigned int hint)
{
#ifdef GED_SSPM
#if defined(__LP64__) && defined(__aarch64__)
	unsigned int val;

	val =  *(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_GAS_TOUCH_HINT);
	/* GAS : 1 */
	if (hint == 1) {
		val |= 0x1;
	} else {
		/*  GAS : 0 */
		val &= 0xfffffffe;
	}
	*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_GAS_TOUCH_HINT) = val;
#endif
#endif
}
EXPORT_SYMBOL(mtk_gpu_gas_hint);
void mtk_gpu_touch_hint(unsigned int hint)
{
#ifdef GED_SSPM
#if defined(__LP64__) && defined(__aarch64__)
	unsigned int val;

	val =  *(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_GAS_TOUCH_HINT);
	/* TOUCH : 1 */
	if (hint == 1) {
		val |= (0x1<<1);
	} else {
	/* // TOUCH : 0 */
		val &= 0xfffffffd;
	}
	*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_GAS_TOUCH_HINT) = val;
#endif
#endif
}
EXPORT_SYMBOL(mtk_gpu_touch_hint);

void mtk_gpu_freq_hint(unsigned int val)
{
#ifdef GED_SSPM
#if defined(__LP64__) && defined(__aarch64__)
	*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_FREQ_HINT) = val;
#endif
#endif

}
EXPORT_SYMBOL(mtk_gpu_freq_hint);

bool mtk_gpu_get_freq_hint(unsigned int *val)
{
#ifdef GED_SSPM
#if defined(__LP64__) && defined(__aarch64__)
	*val = *(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_FREQ_HINT);
#endif
#endif
	return true;

}
EXPORT_SYMBOL(mtk_gpu_get_freq_hint);


/* ****************************************** */

#define DVFS_trace_counter ged_log_trace_counter

#define SS0 0
#define SS1 1
#define SS2 2

#define PERCNT_BUF_COUNT 16
#define CYCLE_SUM_COUNT  4
#define PERCNT_SYSTRACE  1

#define W1				 15
#define RCP_FPS			 16			/* rcp. of fps(ms) */

unsigned int g_level_state = SS0;
unsigned int g_gpu_active_buf[PERCNT_BUF_COUNT];
unsigned int g_js0_active_buf[PERCNT_BUF_COUNT];
unsigned int g_CYCLE_COUNT[CYCLE_SUM_COUNT];
unsigned int g_CHK_COUNT[CYCLE_SUM_COUNT];
unsigned int g_MODE;
unsigned int g_WF;

/* unsigned int g_pre_JS0_active = 0; */
unsigned int g_GPU_active;
unsigned int g_JS0_active;

unsigned int g_last_chk_count;
unsigned int g_last_predict_cycle;

/* unsigned int g_chk_count=0; */
unsigned int g_state;
unsigned int g_MODE;

/* unsigned int g_DVFS_CB_COUNT=0; */
unsigned int g_freq_idx;
unsigned int do_dvfs;

unsigned int do_rotate;
unsigned int cycle_SUM, g_freq;
unsigned int curr_freq, denominator, GPU_utility;
unsigned int predict_cycle, predict_freq;

int	diff[CYCLE_SUM_COUNT-1];
int	sum_diff, tmp1, tmp2, tmp3;
int	i;


void mt_do_systrace(void)
{

#ifdef GED_SSPM
#if defined(__LP64__) && defined(__aarch64__)
	DVFS_trace_counter("(GED)pc :GPU_Active",
		*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_FDVFS_GPU_ACTIVE));
	DVFS_trace_counter("(GED)pc :JS0_Active",
		*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_FDVFS_JS0_ACTIVE));
	DVFS_trace_counter("(GED)g_level_state:",
		*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_FDVFS_LEVEL_STATE));
	DVFS_trace_counter("(GED)g_chk_cnt",
		*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_FDVFS_CHK_CNT));
	DVFS_trace_counter("(GED)cycle sum",
		*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_FDVFS_CYCLE_SUM));
	DVFS_trace_counter("(GED)GPU utility",
		*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_FDVFS_GPU_UTILITY));
	DVFS_trace_counter("(GED)predict frequence",
		*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_FDVFS_PREDICT_FREQ));
	DVFS_trace_counter("(GED)predict mode",
		*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_FDVFS_PREDICT_MODE));
	DVFS_trace_counter("(GED)g_CYCLE_COUNT[0]",
		*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_FDVFS_CYCLE_COUNT_0));
	DVFS_trace_counter("(GED)g_CYCLE_COUNT[1]",
		*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_FDVFS_CYCLE_COUNT_1));
	DVFS_trace_counter("(GED)g_CYCLE_COUNT[2]",
		*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_FDVFS_CYCLE_COUNT_2));
	DVFS_trace_counter("(GED)g_CYCLE_COUNT[3]",
		*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_FDVFS_CYCLE_COUNT_3));
	DVFS_trace_counter("(GED)GPU freq",
		*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_FDVFS_GPU_FREQ));
	DVFS_trace_counter("(GED)do_dvfs",
		*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_FDVFS_DO_DVFS));
	DVFS_trace_counter("(GED)do_compute",
		*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_FDVFS_DO_COMPUTE));
	DVFS_trace_counter("(GED)cur_GPU_idx",
		*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_FDVFS_GPU_CUR_IDX));
	DVFS_trace_counter("(GED)GED_FDVFS_FRAME_DONE",
		*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_FDVFS_FRAME_DONE));
	DVFS_trace_counter("(GED)GAS_TOUCH",
		*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_GAS_TOUCH_HINT));
	DVFS_trace_counter("(GED)FREQ_HINT",
		*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_FDVFS_FREQ_HINT));
	DVFS_trace_counter("(GED)predict cycle",
		*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_FDVFS_PREDICT_CYCLE));
	DVFS_trace_counter("(GED)power status",
		*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_FDVFS_POWER_STATUS));
	DVFS_trace_counter("(GED)GPU utility2",
		*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_FDVFS_GPU_UTILITY2));
	/* DVFS_trace_counter("(GED)GPU_SCALE", *(phys_addr_t *)(gpu_fdvfs_virt_addr+4*GED_FDVFS_GPU_SCALE)); */
	/* DVFS_trace_counter("(GED)GPU_SCALE_X_100", *(phys_addr_t *)(gpu_fdvfs_virt_addr+ */
	/*						4*GED_FDVFS_GPU_SCALE_X_100)); */
	/* DVFS_trace_counter("(GED)GPU_SCALE_X_100_ROUNDING", *(phys_addr_t *)(gpu_fdvfs_virt_addr+ */
	/*						4*GED_FDVFS_GPU_SCALE_X_100_ROUNDING)); */

#endif
#endif
}

void mt_do_fdvfs(void)
{
}

/* ****************************************** */

static struct workqueue_struct *g_psFDVFSWorkQueue;
static struct hrtimer g_HT_fdvfs_debug;
static DEFINE_SPINLOCK(counter_info_lock);

/* ****************************************** */

enum hrtimer_restart ged_fdvfs_debug_cb(struct hrtimer *timer)
{
	mt_do_systrace();

	hrtimer_start(&g_HT_fdvfs_debug, ns_to_ktime(GED_FDVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

/* ****************************************** */

static void gpu_power_change_notify_fdvfs(int power_on)
{
	spin_lock(&counter_info_lock);

#ifdef GED_FDVFS_SYSTRACE
	if (power_on) {
		if (!hrtimer_active(&g_HT_fdvfs_debug)) {
			hrtimer_start(&g_HT_fdvfs_debug, ns_to_ktime(GED_FDVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
			ged_log_buf_print(ghLogBuf_FDVFS, "[%d] Start Timer g_HT_fdvfs_debug", __LINE__);
		}
	}
#endif
	mfg_is_power_on = power_on;

	spin_unlock(&counter_info_lock);
}

/* ****************************************** */
#if 0
static void *_mtk_of_ioremap(const char *node_name)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, node_name);
	if (node)
		return of_iomap(node, 0);

	pr_info("#@# %s:(%s::%d) Cannot find [%s] of_node\n", "FDVFS", __FILE__, __LINE__, node_name);
	return NULL;
}
#endif

GED_ERROR ged_fdvfs_system_init(void)
{
	ghLogBuf_FDVFS = ged_log_buf_alloc(20 * 60 * 10, 20 * 60 * 10 * 80,
		GED_LOG_BUF_TYPE_RINGBUFFER, NULL, "fdvfs_debug");
	g_timestamp = ged_get_time();

#if 0
	g_MFG_base = _mtk_of_ioremap("mediatek,AUSTIN");
	if (g_MFG_base == NULL)
		return GED_ERROR_FAIL;
#endif

	g_psFDVFSWorkQueue = NULL;
	g_psFDVFSWorkQueue = create_workqueue("ged_fdvfs");
	if (g_psFDVFSWorkQueue == NULL)
		return GED_ERROR_OOM;

#ifdef GED_FDVFS_SYSTRACE
	hrtimer_init(&g_HT_fdvfs_debug, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	g_HT_fdvfs_debug.function = ged_fdvfs_debug_cb;
	hrtimer_start(&g_HT_fdvfs_debug, ns_to_ktime(GED_FDVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
#endif

	mtk_register_gpu_power_change("fdvfs", gpu_power_change_notify_fdvfs);

#ifdef GED_SSPM
#if defined(__LP64__) && defined(__aarch64__)
	*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_GAS_TOUCH_HINT) = 0x0;
	*(unsigned int *)(gpu_fdvfs_virt_addr+4*GED_FREQ_HINT) = 1400;
#endif
#endif


	return GED_OK;
}

void ged_fdvfs_exit(void)
{
	mtk_unregister_gpu_power_change("fdvfs");

#ifdef GED_FDVFS_SYSTRACE
	hrtimer_cancel(&g_HT_fdvfs_debug);
#endif

	if (g_psFDVFSWorkQueue != NULL) {
		flush_workqueue(g_psFDVFSWorkQueue);
		destroy_workqueue(g_psFDVFSWorkQueue);
		g_psFDVFSWorkQueue = NULL;
	}

	ged_log_buf_free(ghLogBuf_FDVFS);
	ghLogBuf_FDVFS = 0;
}
