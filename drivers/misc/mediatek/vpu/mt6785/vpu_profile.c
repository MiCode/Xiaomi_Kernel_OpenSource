// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define  MET_USER_EVENT_SUPPORT /*Turn met user event*/

#include <linux/hrtimer.h>
#include <linux/mutex.h>
#include <mt-plat/met_drv.h>
#include "vpu_cmn.h"
#include "vpu_reg.h"
#include "vpu_profile.h"

/* MET: define to enable MET */
#if defined(VPU_MET_READY)
#define CREATE_TRACE_POINTS
#include "met_vpusys_events.h"
#endif


#define DEFAULT_POLLING_PERIOD_NS (1000000)
#define COUNTER_PID (0) /*(65535)*/ /*task_pid_nr(current)*/
#define BEGINEND_PID (0) /*(65535)*/ /*task_pid_nr(current)*/
#define ASYNCBEGINEND_PID (0) /*(65535)*/ /*task_pid_nr(current)*/



static int m_vpu_profile_state;
static struct hrtimer hr_timer;
static uint64_t vpu_base[MTK_VPU_CORE];
static bool vpu_on[MTK_VPU_CORE];
static struct mutex profile_mutex;
static int profiling_counter;
static int stop_result;
static int vpu_counter[MTK_VPU_CORE][4];
/*
 * mini trace system
 * [KATRACE] mini trace system
 */
#define KATRACE_MESSAGE_LENGTH 1024

static noinline int tracing_mark_write(const char *buf)
{
	TRACE_PUTS(buf);
	return 0;
}

/*
 * [KATRACE] Begin-End
 */
#define KATRACE_BEGIN(name) katrace_begin_body(name)
void katrace_begin_body(const char *name)
{
	char buf[KATRACE_MESSAGE_LENGTH];
	int len = snprintf(buf, sizeof(buf), "B|%d|%s", BEGINEND_PID, name);

	if (len >= (int) sizeof(buf)) {
		LOG_DBG("Truncated name in %s: %s\n", __func__, name);
		len = sizeof(buf) - 1;
	}
	tracing_mark_write(buf);
}

#define KATRACE_END() katrace_end()
inline void katrace_end(void)
{
	tracing_mark_write("E");
}


#define WRITE_MSG(format_begin, format_end, pid, name, value) { \
	char buf[KATRACE_MESSAGE_LENGTH]; \
	int len = snprintf(buf, sizeof(buf), format_begin "%s" format_end, \
		pid, name, value); \
	if (len >= (int) sizeof(buf)) { \
		/* Given the sizeof(buf), and all of the current format */ \
		/* buffers, it is impossible for name_len to be < 0 if */ \
		/* len >= sizeof(buf). */ \
		int name_len = strlen(name) - (len - sizeof(buf)) - 1; \
		/* Truncate the name to make the message fit. */ \
		LOG_DBG("Truncated name in %s: %s\n", __func__, name); \
		len = snprintf(buf, sizeof(buf), \
			format_begin "%.*s" format_end, pid, \
			name_len, name, value); \
	} \
	tracing_mark_write(buf); \
}

/*
 * [KATRACE] Counter Integer
 */
#define KATRACE_INT(name, value) katrace_int_body(name, value)
void katrace_int_body(const char *name, int32_t value)
{
	WRITE_MSG("C|%d|", "|%d", COUNTER_PID, name, value);
}

/*
 * [KATRACE] Async Begin-End
 */
#define KATRACE_ASYNC_BEGIN(name, cookie) \
	katrace_async_begin_body(name, cookie)
void katrace_async_begin_body(const char *name, int32_t cookie)
{
	WRITE_MSG("S|%d|", "|%d", ASYNCBEGINEND_PID, name, cookie);
}

#define KATRACE_ASYNC_END(name, cookie) katrace_async_end_body(name, cookie)
void katrace_async_end_body(const char *name, int32_t cookie)
{
	WRITE_MSG("F|%d|", "|%d", ASYNCBEGINEND_PID, name, cookie);
}


/*
 * VPU event based MET funcs
 */
void vpu_met_event_enter(int core, int algo_id, int dsp_freq)
{
	#if defined(VPU_MET_READY)
	trace_VPU__D2D_enter(core, algo_id, dsp_freq);
	#endif
}

void vpu_met_event_leave(int core, int algo_id)
{
	#if defined(VPU_MET_READY)
	trace_VPU__D2D_leave(core, algo_id, 0);
	#endif
}

void vpu_met_packet(long long wclk, char action, int core, int pid,
	int sessid, char *str_desc, int val)
{
	#if defined(VPU_MET_READY)
	trace___MET_PACKET__(wclk, action, core, pid, sessid, str_desc, val);
	#endif
}

/*
 * VPU event based MET funcs
 */
void vpu_met_event_dvfs(int vcore_opp,
	int dsp_freq, int ipu_if_freq, int dsp1_freq, int dsp2_freq)
{
	#if defined(VPU_MET_READY)
	trace_VPU__DVFS(vcore_opp, dsp_freq, ipu_if_freq, dsp1_freq, dsp2_freq);
	#endif
}

/*
 * VPU counter reader
 */
static void vpu_profile_core_read(int core)
{
	int i;
	uint32_t value[4];

	/* read register and send to met */
	for (i = 0; i < 4; i++) {
		uint32_t counter;

		counter = vpu_read_reg32(vpu_base[core],
					 DEBUG_BASE_OFFSET + 0x1080 + i * 4);
		value[i] = (counter - vpu_counter[core][i]);
		vpu_counter[core][i] = counter;
	}
	LOG_DBG("[vpu_profile_%d] read %d/%d/%d/%d\n",
		core, value[0], value[1], value[2], value[3]);

	#if defined(VPU_MET_READY)
	trace_VPU__polling(core, value[0], value[1], value[2], value[3]);
	#endif
}

static void vpu_profile_register_read(void)
{
	int i = 0;

	for (i = 0 ; i < MTK_VPU_CORE; i++)
		if (vpu_on[i])
			vpu_profile_core_read(i);
}

/*
 * VPU Polling Function
 */
static enum hrtimer_restart vpu_profile_polling(struct hrtimer *timer)
{
	LOG_DBG("%s +\n", __func__);
	/*call functions need to be called periodically*/
	vpu_profile_register_read();

	hrtimer_forward_now(&hr_timer, ns_to_ktime(DEFAULT_POLLING_PERIOD_NS));
	LOG_DBG("%s -\n", __func__);
	return HRTIMER_RESTART;
}



static int vpu_profile_timer_start(void)
{
	hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hr_timer.function = vpu_profile_polling;
	hrtimer_start(&hr_timer, ns_to_ktime(DEFAULT_POLLING_PERIOD_NS),
			HRTIMER_MODE_REL);
	return 0;
}

static int vpu_profile_timer_try_stop(void)
{
	int ret = 0;

	ret = hrtimer_try_to_cancel(&hr_timer);
	return ret;
}

static int vpu_profile_timer_stop(void)
{
	hrtimer_cancel(&hr_timer);
	return 0;
}

static uint32_t make_pm_ctrl(uint32_t selector, uint32_t mask)
{
	uint32_t ctrl = PERF_PMCTRL_TRACELEVEL;

	ctrl |= selector << PERF_PMCTRL_SELECT_SHIFT;
	ctrl |= mask << PERF_PMCTRL_MASK_SHIFT;
	return ctrl;
}

static void vpu_profile_core_start(int core)
{
	uint64_t ptr_axi_2;
	uint64_t ptr_pmg, ptr_pmctrl0, ptr_pmstat0, ptr_pmcounter0;
	uint32_t ctrl[4];
	int i;

	ptr_axi_2 = vpu_base[core] + g_vpu_reg_descs[REG_AXI_DEFAULT2].offset;
	ptr_pmg   = vpu_base[core] + DEBUG_BASE_OFFSET + 0x1000;

	ptr_pmcounter0 = vpu_base[core] + DEBUG_BASE_OFFSET + 0x1080;
	ptr_pmctrl0    = vpu_base[core] + DEBUG_BASE_OFFSET + 0x1100;
	ptr_pmstat0    = vpu_base[core] + DEBUG_BASE_OFFSET + 0x1180;
	/* enable */
	VPU_SET_BIT(ptr_axi_2, 0);
	VPU_SET_BIT(ptr_axi_2, 1);
	VPU_SET_BIT(ptr_axi_2, 2);
	VPU_SET_BIT(ptr_axi_2, 3);

	ctrl[0] = make_pm_ctrl(XTPERF_CNT_INSN,
		XTPERF_MASK_INSN_ALL);
	ctrl[1] = make_pm_ctrl(XTPERF_CNT_IDMA,
		XTPERF_MASK_IDMA_ACTIVE_CYCLES);
	ctrl[2] = make_pm_ctrl(XTPERF_CNT_D_STALL,
		XTPERF_MASK_D_STALL_UNCACHED_LOAD);
	ctrl[3] = make_pm_ctrl(XTPERF_CNT_I_STALL,
		XTPERF_MASK_I_STALL_CACHE_MISS);


	/* read register and send to met */
	for (i = 0; i < 4; i++) {
		vpu_write_reg32(ptr_pmctrl0, i * 4, ctrl[i]);
		vpu_write_reg32(ptr_pmstat0, i * 4, 0);
		vpu_write_reg32(ptr_pmcounter0, i * 4, 0);
		vpu_counter[core][i] = 0;
	}

	/* 0: PERF_PMG_ENABLE */
	VPU_SET_BIT(ptr_pmg, 0);
}

static int vpu_profile_start(int core)
{
	LOG_DBG("%s +\n", __func__);
	if (vpu_on[core])
		vpu_profile_core_start(core);
	LOG_DBG("%s -\n", __func__);
	return 0;
}

static int vpu_profile_stop(int type)
{
	LOG_INF("%s (%d)+\n", __func__, type);
	if (type)
		stop_result = vpu_profile_timer_try_stop();
	else
		vpu_profile_timer_stop();

	LOG_DBG("%s (%d/%d)-\n", __func__, type, stop_result);
	return 0;
}


int vpu_profile_state_set(int core, int val)
{
	int start_prof_timer = false;

	switch (val) {
	case 0:
		mutex_lock(&profile_mutex);
		profiling_counter--;
		vpu_on[core] = false;
		LOG_INF("[vpu_profile_%d->%d] (stop) counter(%d, %d)\n",
			core, vpu_on[core], m_vpu_profile_state,
			profiling_counter);

		if (profiling_counter == 0) {
			m_vpu_profile_state = val;
			mutex_unlock(&profile_mutex);
			vpu_profile_stop(0);
		} else {
			mutex_unlock(&profile_mutex);
		}
		break;
	case 1:
		mutex_lock(&profile_mutex);
		profiling_counter++;
		vpu_on[core] = true;
		LOG_INF("[vpu_profile_%d->%d] (start) counter(%d, %d)\n",
			core, vpu_on[core], m_vpu_profile_state,
			profiling_counter);
		m_vpu_profile_state = val;
		if (profiling_counter == 1)
			start_prof_timer = true;
		mutex_unlock(&profile_mutex);
		vpu_profile_start(core);

		if (start_prof_timer)
			vpu_profile_timer_start();
		break;
	default:
		/*unsupported command*/
		return -1;
	}

	LOG_DBG("vpu_profile (%d) -\n", val);
	return 0;
}

int vpu_profile_state_get(void)
{
	return m_vpu_profile_state;
}


int vpu_init_profile(int core, struct vpu_device *device)
{
	int i = 0;

	m_vpu_profile_state = 0;
	stop_result = 0;
	vpu_base[core] = device->vpu_base[core];
	for (i = 0 ; i < MTK_VPU_CORE; i++)
		vpu_on[i] = false;
	profiling_counter = 0;
	mutex_init(&profile_mutex);

	return 0;
}

int vpu_uninit_profile(void)
{
	int i = 0;

	for (i = 0 ; i < MTK_VPU_CORE; i++)
		vpu_on[i] = false;
	if (stop_result)
		vpu_profile_stop(0);

	return 0;
}

