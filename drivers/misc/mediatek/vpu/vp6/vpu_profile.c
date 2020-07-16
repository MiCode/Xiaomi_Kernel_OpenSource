/*
 * Copyright (C) 2016 MediaTek Inc.
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
	char c = 'E';

	tracing_mark_write(&c);
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
void vpu_met_event_dvfs(int vcore_opp, int apu_freq, int apu_if_freq)
{
	#if defined(VPU_MET_READY)
	trace_VPU__DVFS(vcore_opp, apu_freq, apu_if_freq);
	#endif
}

/*
 * VPU event based MET funcs
 */
void vpu_met_event_busyrate(int core, int busyrate)
{
	#if defined(VPU_MET_READY)
	trace_VPU__busyrate(core, busyrate);
	#endif
}

/*
 * VPU counter reader
 */
static void vpu_profile_core_read(struct vpu_core *vpu_core)
{
	int i;
	uint32_t value[4];
	int core = vpu_core->core;

	/* read register and send to met */
	for (i = 0; i < 4; i++) {
		uint32_t counter;

		counter = vpu_read_reg32(vpu_core->vpu_base,
					 DEBUG_BASE_OFFSET + 0x1080 + i * 4);
		value[i] = (counter - vpu_core->vpu_counter[i]);
		vpu_core->vpu_counter[i] = counter;
	}
	LOG_DBG("[vpu_profile_%d] read %d/%d/%d/%d\n",
		core, value[0], value[1], value[2], value[3]);

#if defined(VPU_MET_READY)
	trace_VPU__polling(core, value[0], value[1], value[2], value[3]);
#endif
}

static void vpu_profile_register_read(struct vpu_device *vpu_device)
{
	int i = 0;

	for (i = 0 ; i < vpu_device->core_num; i++)
		if (vpu_device->vpu_core[i]->vpu_on)
			vpu_profile_core_read(vpu_device->vpu_core[i]);
}

/*
 * VPU Polling Function
 */
static enum hrtimer_restart vpu_profile_polling(struct hrtimer *timer)
{
	struct vpu_device *vpu_device = container_of(timer, struct vpu_device,
							hr_timer);
	LOG_DBG("%s +\n", __func__);
	/*call functions need to be called periodically*/
	vpu_profile_register_read(vpu_device);

	hrtimer_forward_now(&vpu_device->hr_timer,
				ns_to_ktime(DEFAULT_POLLING_PERIOD_NS));
	LOG_DBG("%s -\n", __func__);
	return HRTIMER_RESTART;
}

static int vpu_profile_timer_start(struct vpu_device *vpu_device)
{
	hrtimer_init(&vpu_device->hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vpu_device->hr_timer.function = vpu_profile_polling;
	hrtimer_start(&vpu_device->hr_timer,
			ns_to_ktime(DEFAULT_POLLING_PERIOD_NS),
			HRTIMER_MODE_REL);
	return 0;
}

static int vpu_profile_timer_try_stop(struct vpu_device *vpu_device)
{
	int ret = 0;

	ret = hrtimer_try_to_cancel(&vpu_device->hr_timer);
	return ret;
}

static int vpu_profile_timer_stop(struct vpu_device *vpu_device)
{
	hrtimer_cancel(&vpu_device->hr_timer);
	return 0;
}

static uint32_t make_pm_ctrl(uint32_t selector, uint32_t mask)
{
	uint32_t ctrl = PERF_PMCTRL_TRACELEVEL;

	ctrl |= selector << PERF_PMCTRL_SELECT_SHIFT;
	ctrl |= mask << PERF_PMCTRL_MASK_SHIFT;
	return ctrl;
}

static void vpu_profile_core_start(struct vpu_core *vpu_core)
{
	uint64_t ptr_axi_2;
	uint64_t ptr_pmg, ptr_pmctrl0, ptr_pmstat0, ptr_pmcounter0;
	uint32_t ctrl[4];
	int i;

	ptr_axi_2 = vpu_core->vpu_base +
			g_vpu_reg_descs[REG_AXI_DEFAULT2].offset;
	ptr_pmg   = vpu_core->vpu_base + DEBUG_BASE_OFFSET + 0x1000;

	ptr_pmcounter0 = vpu_core->vpu_base + DEBUG_BASE_OFFSET + 0x1080;
	ptr_pmctrl0    = vpu_core->vpu_base + DEBUG_BASE_OFFSET + 0x1100;
	ptr_pmstat0    = vpu_core->vpu_base + DEBUG_BASE_OFFSET + 0x1180;
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
		vpu_core->vpu_counter[i] = 0;
	}

	/* 0: PERF_PMG_ENABLE */
	VPU_SET_BIT(ptr_pmg, 0);
}

static int vpu_profile_start(struct vpu_core *vpu_core)
{
	LOG_DBG("%s +\n", __func__);
	if (vpu_core->vpu_on)
		vpu_profile_core_start(vpu_core);
	LOG_DBG("%s -\n", __func__);
	return 0;
}

static int vpu_profile_stop(struct vpu_device *vpu_device, int type)
{
	LOG_INF("%s (%d)+\n", __func__, type);
	if (type)
		vpu_device->stop_result =
			vpu_profile_timer_try_stop(vpu_device);
	else
		vpu_profile_timer_stop(vpu_device);

	LOG_DBG("%s (%d/%d)-\n", __func__, type, vpu_device->stop_result);
	return 0;
}

int vpu_profile_state_set(struct vpu_core *vpu_core, int val)
{
	struct vpu_device *vpu_device = vpu_core->vpu_device;
	int start_prof_timer = false;
	int core = vpu_core->core;

	switch (val) {
	case 0:
		mutex_lock(&vpu_device->profile_mutex);
		vpu_device->profiling_counter--;
		vpu_core->vpu_on = false;
		LOG_INF("[vpu_profile_%d->%d] (stop) counter(%d, %d)\n",
			core, vpu_core->vpu_on, vpu_device->vpu_profile_state,
			vpu_device->profiling_counter);

		if (vpu_device->profiling_counter == 0) {
			vpu_device->vpu_profile_state = val;
			mutex_unlock(&vpu_device->profile_mutex);
			vpu_profile_stop(vpu_device, 0);
		} else {
			mutex_unlock(&vpu_device->profile_mutex);
		}
		break;
	case 1:
		mutex_lock(&vpu_device->profile_mutex);
		vpu_device->profiling_counter++;
		vpu_core->vpu_on = true;
		LOG_INF("[vpu_profile_%d->%d] (start) counter(%d, %d)\n",
			core, vpu_core->vpu_on, vpu_device->vpu_profile_state,
			vpu_device->profiling_counter);
			vpu_device->vpu_profile_state = val;
		if (vpu_device->profiling_counter == 1)
			start_prof_timer = true;
		mutex_unlock(&vpu_device->profile_mutex);
		vpu_profile_start(vpu_core);

		if (start_prof_timer)
			vpu_profile_timer_start(vpu_device);
		break;
	default:
		/*unsupported command*/
		return -1;
	}

	LOG_DBG("vpu_profile (%d) -\n", val);
	return 0;
}

int vpu_profile_state_get(struct vpu_device *vpu_device)
{
	return vpu_device->vpu_profile_state;
}

int vpu_init_profile(struct vpu_device *vpu_device)
{
	int i = 0;

	vpu_device->vpu_profile_state = 0;
	vpu_device->stop_result = 0;
	for (i = 0 ; i < vpu_device->core_num; i++)
		vpu_device->vpu_core[i]->vpu_on = false;
	vpu_device->profiling_counter = 0;
	mutex_init(&vpu_device->profile_mutex);

	return 0;
}

int vpu_uninit_profile(struct vpu_device *vpu_device)
{
	int i = 0;

	for (i = 0 ; i < vpu_device->core_num; i++)
		vpu_device->vpu_core[i]->vpu_on = false;
	if (vpu_device->stop_result)
		vpu_profile_stop(vpu_device, 0);

	return 0;
}

