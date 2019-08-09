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

#define LOG_TAG "ddp_manager"

#include <linux/slab.h>
#include <linux/mutex.h>

#include "disp_helper.h"
#include "lcm_drv.h"
#include "ddp_reg.h"
#include "ddp_path.h"
#include "ddp_irq.h"
#include "ddp_drv.h"
#include "ddp_debug.h"
#include "ddp_manager.h"
#include "ddp_rdma.h"
#include "ddp_rdma_ex.h"
#include "ddp_ovl.h"
#include "ddp_color.h"
#include "ddp_clkmgr.h"

#include "ddp_log.h"

/* #define __GED_NOTIFICATION_SUPPORT__ */
#ifdef __GED_NOTIFICATION_SUPPORT__
#include "ged.h"
#endif

static int ddp_manager_init;
#define DDP_MAX_MANAGER_HANDLE (DISP_MUTEX_DDP_COUNT+DISP_MUTEX_DDP_FIRST)

struct DPMGR_WQ_HANDLE {
	unsigned int init;
	enum DISP_PATH_EVENT event;
	wait_queue_head_t wq;
	unsigned long long data;
};

struct DDP_IRQ_EVENT_MAPPING {
	enum DDP_IRQ_BIT irq_bit;
};

struct ddp_path_handle {
	struct cmdqRecStruct *cmdqhandle;
	int hwmutexid;
	int power_state;
	enum DDP_MODE mode;
	struct mutex mutex_lock;
	struct DDP_IRQ_EVENT_MAPPING irq_event_map[DISP_PATH_EVENT_NUM];
	struct DPMGR_WQ_HANDLE wq_list[DISP_PATH_EVENT_NUM];
	enum DDP_SCENARIO_ENUM scenario;
	enum DISP_MODULE_ENUM mem_module;
	struct disp_ddp_path_config last_config;
};

struct DDP_MANAGER_CONTEXT {
	int handle_cnt;
	int mutex_idx;
	int power_state;
	struct mutex mutex_lock;
	int module_usage_table[DISP_MODULE_NUM];
	struct ddp_path_handle *module_path_table[DISP_MODULE_NUM];
	struct ddp_path_handle *handle_pool[DDP_MAX_MANAGER_HANDLE];
};

static bool ddp_valid_engine[DISP_MODULE_NUM] = {
	1, /* DISP_MODULE_OVL0 */
	1, /* DISP_MODULE_OVL0_2L */
	0, /* DISP_MODULE_OVL1_2L */
	1, /* DISP_MODULE_RDMA0 */
	0, /* DISP_MODULE_RDMA1 */

	1, /* DISP_MODULE_WDMA0 */
	0, /* DISP_MODULE_WDMA_VIRTUAL0 */
	0, /* DISP_MODULE_WDMA_VIRTUAL1 */
	1, /* DISP_MODULE_COLOR0 */
	1, /* DISP_MODULE_CCORR0 */

	1, /* DISP_MODULE_AAL0 */
	1, /* DISP_MODULE_GAMMA0 */
	1, /* DISP_MODULE_DITHER0 */
	0, /* DISP_MODULE_DSI0 */
	0, /* DISP_MODULE_DSI1 */

	0, /* DISP_MODULE_DSIDUAL */
	0, /* DISP_MODULE_PWM0 */
	0, /* DISP_MODULE_CONFIG */
	0, /* DISP_MODULE_MUTEX */
	0, /* DISP_MODULE_SMI_COMMON */

	0, /* DISP_MODULE_SMI_LARB0 */
	0, /* DISP_MODULE_SMI_LARB1 */
	0, /* DISP_MODULE_MIPI0 */
	0, /* DISP_MODULE_MIPI1 */
	1, /* DISP_MODULE_RSZ0 */

	0, /* DISP_MODULE_RSZ0_VIRT0 */
	0, /* DISP_MODULE_RSZ0_VIRT1 */
	0, /* DISP_MODULE_DPI */
	0, /* DISP_MODULE_UNKNOWN */
};

int get_ddp_valid_engine(int module_num)
{
	return ddp_valid_engine[module_num];
}

#define DEFAULT_IRQ_EVENT_SCENARIO (4)
static struct DDP_IRQ_EVENT_MAPPING
	ddp_irq_event_list[DEFAULT_IRQ_EVENT_SCENARIO][DISP_PATH_EVENT_NUM] = {
	{			/* ovl0 path */
	 {DDP_IRQ_RDMA0_DONE},	/*FRAME_DONE */
	 {DDP_IRQ_RDMA0_START},	/*FRAME_START */
	 {DDP_IRQ_RDMA0_REG_UPDATE},	/*FRAME_REG_UPDATE */
	 {DDP_IRQ_RDMA0_TARGET_LINE},	/*FRAME_TARGET_LINE */
	 {DDP_IRQ_WDMA0_FRAME_COMPLETE},	/*FRAME_COMPLETE */
	 {DDP_IRQ_RDMA0_TARGET_LINE},	/*FRAME_STOP */
	 {DDP_IRQ_RDMA0_REG_UPDATE},	/*IF_CMD_DONE */
	 {DDP_IRQ_DSI0_EXT_TE},	/*IF_VSYNC */
	 {DDP_IRQ_UNKNOWN}, /*TRIGER*/
	 {DDP_IRQ_AAL0_OUT_END_FRAME},	/*AAL_OUT_END_EVENT */
	 },
	{			/* ovl1 path */
	 {DDP_IRQ_RDMA1_DONE},	/*FRAME_DONE */
	 {DDP_IRQ_RDMA1_START},	/*FRAME_START */
	 {DDP_IRQ_RDMA1_REG_UPDATE},	/*FRAME_REG_UPDATE */
	 {DDP_IRQ_RDMA1_TARGET_LINE},	/*FRAME_TARGET_LINE */
	 {DDP_IRQ_WDMA0_FRAME_COMPLETE},	/*FRAME_COMPLETE */
	 {DDP_IRQ_RDMA1_TARGET_LINE},	/*FRAME_STOP */
	 {DDP_IRQ_RDMA1_REG_UPDATE},	/*IF_CMD_DONE */
	 {DDP_IRQ_DPI_VSYNC},	/*IF_VSYNC */
	 {DDP_IRQ_UNKNOWN}, /*TRIGER*/
	 {DDP_IRQ_AAL0_OUT_END_FRAME},	/*AAL_OUT_END_EVENT */
	 },
	{			/* rdma path */
	 {DDP_IRQ_RDMA1_DONE},	/*FRAME_DONE */
	 {DDP_IRQ_RDMA1_START},	/*FRAME_START */
	 {DDP_IRQ_RDMA1_REG_UPDATE},	/*FRAME_REG_UPDATE */
	 {DDP_IRQ_RDMA1_TARGET_LINE},	/*FRAME_TARGET_LINE */
	 {DDP_IRQ_UNKNOWN},	/*FRAME_COMPLETE */
	 {DDP_IRQ_RDMA1_TARGET_LINE},	/*FRAME_STOP */
	 {DDP_IRQ_RDMA1_REG_UPDATE},	/*IF_CMD_DONE */
	 {DDP_IRQ_RDMA1_TARGET_LINE},	/*IF_VSYNC */
	 {DDP_IRQ_UNKNOWN}, /*TRIGER*/
	 {DDP_IRQ_UNKNOWN},	/*AAL_OUT_END_EVENT */
	 },
	{			/* ovl0 path */
	 {DDP_IRQ_RDMA0_DONE},	/*FRAME_DONE */
	 {DDP_IRQ_MUTEX1_SOF},	/*FRAME_START */
	 {DDP_IRQ_RDMA0_REG_UPDATE},	/*FRAME_REG_UPDATE */
	 {DDP_IRQ_RDMA0_TARGET_LINE},	/*FRAME_TARGET_LINE */
	 {DDP_IRQ_WDMA0_FRAME_COMPLETE},	/*FRAME_COMPLETE */
	 {DDP_IRQ_RDMA0_TARGET_LINE},	/*FRAME_STOP */
	 {DDP_IRQ_RDMA0_REG_UPDATE},	/*IF_CMD_DONE */
	 {DDP_IRQ_DSI0_EXT_TE},	/*IF_VSYNC */
	 {DDP_IRQ_UNKNOWN}, /*TRIGER*/
	 {DDP_IRQ_AAL0_OUT_END_FRAME},	/*AAL_OUT_END_EVENT */
	}
};

static char *path_event_name(enum DISP_PATH_EVENT event)
{
	switch (event) {
	case DISP_PATH_EVENT_FRAME_START:
		return "FRAME_START";
	case DISP_PATH_EVENT_FRAME_DONE:
		return "FRAME_DONE";
	case DISP_PATH_EVENT_FRAME_REG_UPDATE:
		return "REG_UPDATE";
	case DISP_PATH_EVENT_FRAME_TARGET_LINE:
		return "TARGET_LINE";
	case DISP_PATH_EVENT_FRAME_COMPLETE:
		return "FRAME COMPLETE";
	case DISP_PATH_EVENT_FRAME_STOP:
		return "FRAME_STOP";
	case DISP_PATH_EVENT_IF_CMD_DONE:
		return "FRAME_STOP";
	case DISP_PATH_EVENT_IF_VSYNC:
		return "VSYNC";
	case DISP_PATH_EVENT_TRIGGER:
		return "TRIGGER";
	case DISP_PATH_EVENT_DELAYED_TRIGGER_33ms:
		return "DELAY_TRIG";
	default:
		return "unknown event";
	}
	return "unknown event";
}

static struct DDP_MANAGER_CONTEXT *_get_context(void)
{
	static int is_context_inited;
	static struct DDP_MANAGER_CONTEXT context;

	if (!is_context_inited) {
		memset((void *)&context, 0,
			sizeof(struct DDP_MANAGER_CONTEXT));
		context.mutex_idx = (1 << DISP_MUTEX_DDP_COUNT) - 1;
		mutex_init(&context.mutex_lock);
		is_context_inited = 1;
	}
	return &context;
}

static int path_top_clock_off(void)
{
	int i = 0;
	struct DDP_MANAGER_CONTEXT *c = _get_context();

	if (!c->power_state)
		return 0;

	for (i = 0; i < DDP_MAX_MANAGER_HANDLE; i++) {
		if (c->handle_pool[i] && c->handle_pool[i]->power_state)
			return 0;
	}

	c->power_state = 0;
	for (i = 0; i < DISP_MODULE_NUM; i++) {
		if (ddp_valid_engine[i])
			ddp_clk_disable_by_module(i);
	}

	ddp_path_top_clock_off();

	return 0;
}

static int path_top_clock_on(void)
{
	struct DDP_MANAGER_CONTEXT *c = _get_context();
	int i = 0;

	if (c->power_state)
		return 0;

	c->power_state = 1;
	ddp_path_top_clock_on();
	for (i = 0; i < DISP_MODULE_NUM; i++) {
		if (ddp_valid_engine[i])
			ddp_clk_enable_by_module(i);
	}

	return 0;
}

static int module_power_off(enum DISP_MODULE_ENUM module)
{
	if (module == DISP_MODULE_DSI0)
		return 0;

	if (ddp_get_module_driver(module) != 0) {
		if (ddp_get_module_driver(module)->power_off != 0)
			ddp_get_module_driver(module)->power_off(module, NULL);
	}
	return 0;
}

static int module_power_on(enum DISP_MODULE_ENUM module)
{
	if (module == DISP_MODULE_DSI0)
		return 0;

	if (ddp_get_module_driver(module) != 0) {
		if (ddp_get_module_driver(module)->power_on != 0)
			ddp_get_module_driver(module)->power_on(module, NULL);
	}
	return 0;
}

static struct ddp_path_handle *find_handle_by_module(
	enum DISP_MODULE_ENUM module)
{
	if ((module == DISP_MODULE_DSI0) &&
		(!_get_context()->module_path_table[DISP_MODULE_DSI0]))
		return _get_context()->module_path_table[DISP_MODULE_DSIDUAL];

	return _get_context()->module_path_table[module];
}

int dpmgr_module_notify(enum DISP_MODULE_ENUM module,
	enum DISP_PATH_EVENT event)
{
	int ret = 0;

	struct ddp_path_handle *handle = find_handle_by_module(module);

	if (handle)
		ret = dpmgr_signal_event(handle, event);

	mmprofile_log_ex(ddp_mmp_get_events()->primary_display_aalod_trigger,
		MMPROFILE_FLAG_PULSE, module, event);
	return ret;
}

static int assign_default_irqs_table(enum DDP_SCENARIO_ENUM scenario,
	struct DDP_IRQ_EVENT_MAPPING *irq_events)
{
	int idx = 0;

	switch (scenario) {
	case DDP_SCENARIO_PRIMARY_DISP:
	case DDP_SCENARIO_PRIMARY_BYPASS_PQ_DISP:
	case DDP_SCENARIO_PRIMARY_RDMA0_COLOR0_DISP:
	case DDP_SCENARIO_PRIMARY_RDMA0_DISP:
	case DDP_SCENARIO_PRIMARY_ALL:
		idx = 0;
		break;
	case DDP_SCENARIO_SUB_DISP:
	case DDP_SCENARIO_SUB_RDMA1_DISP:
	case DDP_SCENARIO_SUB_OVL_MEMOUT:
	case DDP_SCENARIO_SUB_ALL:
		idx = 1;
		break;
	case DDP_SCENARIO_PRIMARY_OVL_MEMOUT:
		idx = 3;
		break;
	default:
		DISP_LOG_E("unknown scenario %d\n", scenario);
	}
	memcpy(irq_events, ddp_irq_event_list[idx],
		sizeof(ddp_irq_event_list[idx]));
	return 0;
}

#if 0  /* defined but not used */
static int acquire_free_bit(unsigned int total)
{
	int free_id = 0;

	while (total) {
		if (total & 0x1)
			return free_id;

		total >>= 1;
		++free_id;
	}
	return -1;
}
#endif

static int acquire_mutex(enum DDP_SCENARIO_ENUM scenario)
{
/* /: primay use mutex 0 */
	int mutex_id = 0;
	int mutex_idx_free = 0;

	struct DDP_MANAGER_CONTEXT *ctx = _get_context();

	ASSERT(scenario >= 0 && scenario < DDP_SCENARIO_MAX);
	mutex_idx_free = ctx->mutex_idx;
	while (mutex_idx_free) {
		if (mutex_idx_free & 0x1) {
			ctx->mutex_idx &= (~(0x1 << mutex_id));
			mutex_id += DISP_MUTEX_DDP_FIRST;
			break;
		}
		mutex_idx_free >>= 1;
		++mutex_id;
	}
	ASSERT(mutex_id < (DISP_MUTEX_DDP_FIRST + DISP_MUTEX_DDP_COUNT));
	DDPDBG("scenario %s acquire mutex %d, left mutex 0x%x!\n",
		ddp_get_scenario_name(scenario), mutex_id,
		ctx->mutex_idx);
	return mutex_id;
}

static int release_mutex(int mutex_idx)
{
	struct DDP_MANAGER_CONTEXT *ctx = _get_context();

	ASSERT(mutex_idx < (DISP_MUTEX_DDP_FIRST + DISP_MUTEX_DDP_COUNT));

	ctx->mutex_idx |= 1 << (mutex_idx - DISP_MUTEX_DDP_FIRST);

	DDPDBG("release mutex %d, left mutex 0x%x!\n",
		mutex_idx, ctx->mutex_idx);
	return 0;
}

int dpmgr_path_set_video_mode(disp_path_handle dp_handle, int is_vdo_mode)
{
	struct ddp_path_handle *handle = NULL;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	handle->mode = is_vdo_mode ? DDP_VIDEO_MODE : DDP_CMD_MODE;
	DDPDBG("set scenario %s mode: %s\n",
		ddp_get_scenario_name(handle->scenario),
		is_vdo_mode ? "Video_Mode" : "Cmd_Mode");
	return 0;
}

disp_path_handle dpmgr_create_path(enum DDP_SCENARIO_ENUM scenario,
	struct cmdqRecStruct *cmdq_handle)
{
	int i = 0;
	int module_name;
	struct ddp_path_handle *path_handle = NULL;
	int *modules = ddp_get_scenario_list(scenario);
	int module_num = ddp_get_module_num(scenario);
	struct DDP_MANAGER_CONTEXT *content = _get_context();

	mutex_lock(&content->mutex_lock);
	path_handle = kzalloc(sizeof(*path_handle), GFP_KERNEL);
	if (path_handle != NULL) {
		path_handle->cmdqhandle = cmdq_handle;
		path_handle->scenario = scenario;
		path_handle->hwmutexid = acquire_mutex(scenario);
		path_handle->last_config.path_handle = path_handle;
		assign_default_irqs_table(scenario,
			path_handle->irq_event_map);
		DDPDBG("create handle %p on scenario %s\n",
			path_handle, ddp_get_scenario_name(scenario));
		DDPDBG(" scenario %s include module: ",
			ddp_get_scenario_name(scenario));
		for (i = 0; i < module_num; i++) {
			module_name = modules[i];
			DDPDBG("%s\n", ddp_get_module_name(module_name));
			content->module_usage_table[module_name]++;
			content->module_path_table[module_name] = path_handle;
		}
		content->handle_cnt++;
		content->handle_pool[path_handle->hwmutexid] = path_handle;
	} else {
		DISP_LOG_E("Fail to create handle on scenario %s\n",
			ddp_get_scenario_name(scenario));
	}
	mutex_unlock(&content->mutex_lock);
	return path_handle;
}

int dpmgr_get_scenario(disp_path_handle dp_handle)
{
	struct ddp_path_handle *handle = (struct ddp_path_handle *)dp_handle;

	return handle->scenario;
}

static int _dpmgr_path_connect(enum DDP_SCENARIO_ENUM scenario, void *handle)
{
	int i = 0, module;
	int *modules = ddp_get_scenario_list(scenario);
	int module_num = ddp_get_module_num(scenario);

	ddp_connect_path(scenario, handle);

	for (i = 0; i < module_num; i++) {
		module = modules[i];
		if (ddp_get_module_driver(module) &&
			ddp_get_module_driver(module)->connect) {
			int prev = i == 0 ?
				DISP_MODULE_UNKNOWN : modules[i - 1];
			int next = i == module_num - 1 ?
				DISP_MODULE_UNKNOWN : modules[i + 1];

			ddp_get_module_driver(module)->connect(module, prev,
				next, 1, handle);
		}
	}

	return 0;
}

static int _dpmgr_path_disconnect(enum DDP_SCENARIO_ENUM scenario,
	void *handle)
{
	int i = 0, module;
	int *modules = ddp_get_scenario_list(scenario);
	int module_num = ddp_get_module_num(scenario);

	ddp_disconnect_path(scenario, handle);

	for (i = 0; i < module_num; i++) {
		module = modules[i];
		if (ddp_get_module_driver(module) &&
			ddp_get_module_driver(module)->connect) {
			int prev = i == 0 ?
				DISP_MODULE_UNKNOWN : modules[i - 1];
			int next = i == module_num - 1 ?
				DISP_MODULE_UNKNOWN : modules[i + 1];

			ddp_get_module_driver(module)->connect(module,
				prev, next, 1, handle);
		}
	}

	return 0;
}

int dpmgr_modify_path_power_on_new_modules(disp_path_handle dp_handle,
	enum DDP_SCENARIO_ENUM new_scenario, int sw_only)
{
	int i = 0;
	int module_name = 0;
	struct DDP_MANAGER_CONTEXT *content = _get_context();
	struct ddp_path_handle *handle = (struct ddp_path_handle *)dp_handle;

	int *new_modules = ddp_get_scenario_list(new_scenario);
	int new_module_num = ddp_get_module_num(new_scenario);

	for (i = 0; i < new_module_num; i++) {
		module_name = new_modules[i];
		/* new module's count =0 */
		if (content->module_usage_table[module_name] == 0) {
			content->module_usage_table[module_name]++;
			content->module_path_table[module_name] = handle;
			if (!sw_only)
				module_power_on(module_name);
		}
	}
	return 0;
}

/**
 * NOTES: modify path should call API like this :
 *   old_scenario = dpmgr_get_scenario(handle);
 *   dpmgr_modify_path_power_on_new_modules();
 *   dpmgr_modify_path();
 *
 * after cmdq handle exec done:
 *   dpmgr_modify_path_power_off_old_modules();
 */
int dpmgr_modify_path(disp_path_handle dp_handle,
	enum DDP_SCENARIO_ENUM new_scenario, struct cmdqRecStruct *cmdq_handle,
	enum DDP_MODE mode, int sw_only)
{
	struct ddp_path_handle *handle;
	enum DDP_SCENARIO_ENUM old_scenario;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	old_scenario = handle->scenario;
	handle->cmdqhandle = cmdq_handle;
	handle->scenario = new_scenario;
	DISP_LOG_I("modify handle %p from %s to %s\n",
		handle, ddp_get_scenario_name(old_scenario),
		ddp_get_scenario_name(new_scenario));

	if (!sw_only) {
		/* mutex set will clear old settings */
		ddp_mutex_set(handle->hwmutexid, new_scenario, mode,
			cmdq_handle);

		ddp_mutex_interrupt_enable(handle->hwmutexid, cmdq_handle);
		/* disconnect old path first */
		_dpmgr_path_disconnect(old_scenario, cmdq_handle);

		/* connect new path */
		_dpmgr_path_connect(new_scenario, cmdq_handle);
	}
	return 0;
}

int dpmgr_modify_path_power_off_old_modules(
	enum DDP_SCENARIO_ENUM old_scenario,
	enum DDP_SCENARIO_ENUM new_scenario, int sw_only)
{
	int i = 0;
	int module_name = 0;

	struct DDP_MANAGER_CONTEXT *content = _get_context();

	int *old_modules = ddp_get_scenario_list(old_scenario);
	int old_module_num = ddp_get_module_num(old_scenario);

	for (i = 0; i < old_module_num; i++) {
		module_name = old_modules[i];
		if (!ddp_is_module_in_scenario(new_scenario, module_name)) {
			content->module_usage_table[module_name]--;
			content->module_path_table[module_name] = NULL;
			if (!sw_only)
				module_power_off(module_name);
		}
	}

	return 0;
}

int dpmgr_destroy_path_handle(disp_path_handle dp_handle)
{
	int i = 0;
	int module_name;
	struct ddp_path_handle *handle;
	int *modules;
	int module_num;
	struct DDP_MANAGER_CONTEXT *content;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);
	content = _get_context();

	DDPDBG("destroy path handle %p on scenario %s\n", handle,
		   ddp_get_scenario_name(handle->scenario));
	mutex_lock(&content->mutex_lock);
	if (handle != NULL) {
		release_mutex(handle->hwmutexid);
		for (i = 0; i < module_num; i++) {
			module_name = modules[i];
			content->module_usage_table[module_name]--;
			content->module_path_table[module_name] = NULL;
		}
		content->handle_cnt--;
		ASSERT(content->handle_cnt >= 0);
		content->handle_pool[handle->hwmutexid] = NULL;
		kfree(handle);
	}
	mutex_unlock(&content->mutex_lock);
	return 0;
}

int dpmgr_destroy_path(disp_path_handle dp_handle,
	struct cmdqRecStruct *cmdq_handle)
{
	struct ddp_path_handle *handle = (struct ddp_path_handle *)dp_handle;

	if (handle)
		_dpmgr_path_disconnect(handle->scenario, cmdq_handle);

	dpmgr_destroy_path_handle(dp_handle);
	return 0;
}

int dpmgr_path_memout_clock(disp_path_handle dp_handle, int clock_switch)
{
	return 0;
}

int dpmgr_path_add_memout(disp_path_handle dp_handle,
	enum DISP_MODULE_ENUM engine, void *cmdq_handle)
{
	struct ddp_path_handle *handle;
	enum DISP_MODULE_ENUM wdma;
	struct DDP_MANAGER_CONTEXT *context;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	ASSERT(handle->scenario == DDP_SCENARIO_PRIMARY_DISP ||
	       handle->scenario == DDP_SCENARIO_SUB_DISP ||
	       handle->scenario == DDP_SCENARIO_PRIMARY_OVL_MEMOUT);

	wdma = DISP_MODULE_WDMA0;

	if (ddp_is_module_in_scenario(handle->scenario, wdma) == 1) {
		DDPERR("%s error, wdma is already in scenario=%s\n",
		       __func__, ddp_get_scenario_name(handle->scenario));
		return -1;
	}
	/* update contxt */
	context = _get_context();
	context->module_usage_table[wdma]++;
	context->module_path_table[wdma] = handle;
	if (engine == DISP_MODULE_OVL0) {
		handle->scenario = DDP_SCENARIO_PRIMARY_ALL;
	} else {
		pr_info("%s error: engine=%d\n", __func__, engine);
		ASSERT(0);
		return 0;
	}
	/* update connected */
	_dpmgr_path_connect(handle->scenario, cmdq_handle);
	ddp_mutex_set(handle->hwmutexid, handle->scenario, handle->mode,
		cmdq_handle);

	/* wdma just need start. */
	if (ddp_get_module_driver(wdma) != 0) {
		if (ddp_get_module_driver(wdma)->init != 0)
			ddp_get_module_driver(wdma)->init(wdma, cmdq_handle);

		if (ddp_get_module_driver(wdma)->start != 0)
			ddp_get_module_driver(wdma)->start(wdma, cmdq_handle);
	}
	return 0;
}

int dpmgr_path_remove_memout(disp_path_handle dp_handle, void *cmdq_handle)
{
	enum DDP_SCENARIO_ENUM dis_scn;
	enum DDP_SCENARIO_ENUM new_scn;
	struct ddp_path_handle *handle;
	enum DISP_MODULE_ENUM wdma;
	struct DDP_MANAGER_CONTEXT *context;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	ASSERT(handle->scenario == DDP_SCENARIO_PRIMARY_DISP ||
	       handle->scenario == DDP_SCENARIO_PRIMARY_ALL ||
	       handle->scenario == DDP_SCENARIO_SUB_DISP ||
	       handle->scenario == DDP_SCENARIO_SUB_ALL);
	wdma = DISP_MODULE_WDMA0;

	if (ddp_is_module_in_scenario(handle->scenario, wdma) == 0) {
		DDPERR("%s error, wdma is not in scenario=%s\n",
		       __func__, ddp_get_scenario_name(handle->scenario));
		return -1;
	}
	/* update contxt */
	context = _get_context();
	context->module_usage_table[wdma]--;
	context->module_path_table[wdma] = 0;
	/* wdma just need stop */
	if (ddp_get_module_driver(wdma) != 0) {
		if (ddp_get_module_driver(wdma)->stop != 0)
			ddp_get_module_driver(wdma)->stop(wdma, cmdq_handle);

		/* notes: deinit is moved to cmdq callback */
		/* if (ddp_get_module_driver(wdma)->deinit != 0)
		 *	ddp_get_module_driver(wdma)->deinit(wdma, cmdq_handle);
		 */
	}
	if (handle->scenario == DDP_SCENARIO_PRIMARY_ALL) {
		dis_scn = DDP_SCENARIO_PRIMARY_OVL_MEMOUT;
		new_scn = DDP_SCENARIO_PRIMARY_DISP;
	} else if (handle->scenario == DDP_SCENARIO_SUB_ALL) {
		dis_scn = DDP_SCENARIO_SUB_OVL_MEMOUT;
		new_scn = DDP_SCENARIO_SUB_DISP;
	} else {
		pr_info("%s: error scenario =%d\n",
			__func__, handle->scenario);
		ASSERT(0);
		return 0;
	}
	_dpmgr_path_disconnect(dis_scn, cmdq_handle);
	handle->scenario = new_scn;
	/* update connected */
	_dpmgr_path_connect(handle->scenario, cmdq_handle);
	ddp_mutex_set(handle->hwmutexid, handle->scenario, handle->mode,
		cmdq_handle);

	return 0;
}

int dpmgr_path_set_dst_module(disp_path_handle dp_handle,
	enum DISP_MODULE_ENUM dst_module)
{
	struct ddp_path_handle *handle;

	if (dp_handle == NULL) {
		ASSERT(0);
		return -1;
	}

	handle = (struct ddp_path_handle *)dp_handle;
	if (!(handle->scenario >= 0 && handle->scenario < DDP_SCENARIO_MAX)) {
		ASSERT(0);
		return -1;
	}
	DDPDBG("set dst module on scenario %s, module %s\n",
		ddp_get_scenario_name(handle->scenario),
		ddp_get_module_name(dst_module));
	return ddp_set_dst_module(handle->scenario, dst_module);
}

int dpmgr_path_get_mutex(disp_path_handle dp_handle)
{
	struct ddp_path_handle *handle = NULL;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	return handle->hwmutexid;
}

enum DISP_MODULE_ENUM dpmgr_path_get_dst_module(disp_path_handle dp_handle)
{
	struct ddp_path_handle *handle;
	enum DISP_MODULE_ENUM dst_module;

	if (dp_handle == NULL) {
		ASSERT(0);
		return -1;
	}
	handle = (struct ddp_path_handle *)dp_handle;

	if (!(handle->scenario >= 0 && handle->scenario < DDP_SCENARIO_MAX)) {
		ASSERT(0);
		return -1;
	}
	dst_module = ddp_get_dst_module(handle->scenario);

	DISP_LOG_V("get dst module on scenario %s, module %s\n",
		ddp_get_scenario_name(handle->scenario),
		ddp_get_module_name(dst_module));
	return dst_module;
}

enum dst_module_type dpmgr_path_get_dst_module_type(disp_path_handle dp_handle)
{
	enum DISP_MODULE_ENUM dst_module =
		dpmgr_path_get_dst_module(dp_handle);

	if (dst_module == DISP_MODULE_WDMA0)
		return DST_MOD_WDMA;
	else
		return DST_MOD_REAL_TIME;
}

int dpmgr_path_connect(disp_path_handle dp_handle, int encmdq)
{
	struct ddp_path_handle *handle;
	struct cmdqRecStruct *cmdqHandle;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	cmdqHandle = encmdq ? handle->cmdqhandle : NULL;

	_dpmgr_path_connect(handle->scenario, cmdqHandle);

	ddp_mutex_set(handle->hwmutexid, handle->scenario,
		handle->mode, cmdqHandle);
	ddp_mutex_interrupt_enable(handle->hwmutexid, cmdqHandle);
	return 0;
}

int dpmgr_path_disconnect(disp_path_handle dp_handle, int encmdq)
{
	struct ddp_path_handle *handle;
	struct cmdqRecStruct *cmdqHandle;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	cmdqHandle = encmdq ? handle->cmdqhandle : NULL;

	DDPDBG("dpmgr_path_disconnect on scenario %s\n",
		   ddp_get_scenario_name(handle->scenario));
	ddp_mutex_clear(handle->hwmutexid, cmdqHandle);
	ddp_mutex_interrupt_disable(handle->hwmutexid, cmdqHandle);
	_dpmgr_path_disconnect(handle->scenario, cmdqHandle);
	return 0;
}

int dpmgr_path_init(disp_path_handle dp_handle, int encmdq)
{
	int i = 0;
	int module_name, module_num;
	struct ddp_path_handle *handle;
	int *modules;
	struct cmdqRecStruct *cmdqHandle;
	struct DDP_MANAGER_CONTEXT *c = _get_context();
	struct DDP_MODULE_DRIVER *mod_drv;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);
	cmdqHandle = encmdq ? handle->cmdqhandle : NULL;

	DDPDBG("path init on scenario %s\n",
		ddp_get_scenario_name(handle->scenario));
	mutex_lock(&c->mutex_lock);
	/* open top clock */
	path_top_clock_on();
	/* seting mutex */
	ddp_mutex_set(handle->hwmutexid, handle->scenario, handle->mode,
		cmdqHandle);
	ddp_mutex_interrupt_enable(handle->hwmutexid, cmdqHandle);
	/* connect path; */
	_dpmgr_path_connect(handle->scenario, cmdqHandle);

	/* each module init */
	for (i = 0; i < module_num; i++) {
		module_name = modules[i];
		mod_drv = ddp_get_module_driver(module_name);
		if (mod_drv != 0) {
			if (mod_drv->init != 0)
				mod_drv->init(module_name, cmdqHandle);

			if (mod_drv->set_listener != 0)
				mod_drv->set_listener(module_name,
					dpmgr_module_notify);
		}
	}
	/* after init this path will power on; */
	handle->power_state = 1;
	mutex_unlock(&c->mutex_lock);
	return 0;
}

int dpmgr_path_deinit(disp_path_handle dp_handle, int encmdq)
{
	int i = 0;
	int module_name, module_num;
	int *modules;
	struct cmdqRecStruct *cmdqHandle;
	struct ddp_path_handle *handle;
	struct DDP_MANAGER_CONTEXT *c = _get_context();
	struct DDP_MODULE_DRIVER *mod_drv;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);
	cmdqHandle = encmdq ? handle->cmdqhandle : NULL;

	DDPDBG("path deinit on scenario %s\n",
		ddp_get_scenario_name(handle->scenario));
	mutex_lock(&c->mutex_lock);
	ddp_mutex_interrupt_disable(handle->hwmutexid, cmdqHandle);
	ddp_mutex_clear(handle->hwmutexid, cmdqHandle);
	_dpmgr_path_disconnect(handle->scenario, cmdqHandle);
	for (i = 0; i < module_num; i++) {
		module_name = modules[i];
		mod_drv = ddp_get_module_driver(module_name);
		if (mod_drv != 0) {
			if (mod_drv->deinit != 0)
				mod_drv->deinit(module_name, cmdqHandle);

			if (mod_drv->set_listener != 0)
				mod_drv->set_listener(module_name, NULL);
		}
	}
	handle->power_state = 0;
	/* close top clock when last path init */
	path_top_clock_off();
	mutex_unlock(&c->mutex_lock);
	return 0;
}

int dpmgr_path_start(disp_path_handle dp_handle, int encmdq)
{
	int i = 0;
	int module_name, module_num;
	struct ddp_path_handle *handle;
	int *modules;
	struct cmdqRecStruct *cmdqHandle;
	struct DDP_MODULE_DRIVER *mod_drv;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);
	cmdqHandle = encmdq ? handle->cmdqhandle : NULL;

	DISP_LOG_I("path start on scenario %s\n",
		ddp_get_scenario_name(handle->scenario));
	for (i = 0; i < module_num; i++) {
		module_name = modules[i];
		mod_drv = ddp_get_module_driver(module_name);
		if (ddp_get_module_driver(module_name) != 0) {
			if (mod_drv->start != 0)
				mod_drv->start(module_name, cmdqHandle);
		}
	}

	return 0;
}

int dpmgr_path_stop(disp_path_handle dp_handle, int encmdq)
{

	int i = 0;
	int module_name, module_num;
	struct ddp_path_handle *handle;
	int *modules;
	struct cmdqRecStruct *cmdqHandle;
	struct DDP_MODULE_DRIVER *mod_drv;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);
	cmdqHandle = encmdq ? handle->cmdqhandle : NULL;

	DISP_LOG_I("path stop on scenario %s\n",
		ddp_get_scenario_name(handle->scenario));
	for (i = module_num - 1; i >= 0; i--) {
		module_name = modules[i];
		mod_drv = ddp_get_module_driver(module_name);
		if (mod_drv != 0) {
			if (mod_drv->stop != 0)
				mod_drv->stop(module_name, cmdqHandle);
		}
	}
	return 0;
}

int dpmgr_path_ioctl(disp_path_handle dp_handle, void *cmdq_handle,
	enum DDP_IOCTL_NAME ioctl_cmd, void *params)
{
	int i = 0;
	int ret = 0;
	int module_name, module_num;
	int *modules;
	struct ddp_path_handle *handle;
	struct DDP_MODULE_DRIVER *mod_drv;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);

	DDPDBG("path IOCTL(%s) on scenario %s\n",
			ddp_get_ioctl_name(ioctl_cmd),
			ddp_get_scenario_name(handle->scenario));
	for (i = module_num - 1; i >= 0; i--) {
		module_name = modules[i];
		mod_drv = ddp_get_module_driver(module_name);
		if (mod_drv != 0 && mod_drv->ioctl != 0)
			ret += mod_drv->ioctl(module_name, cmdq_handle,
				ioctl_cmd, params);
	}
	return ret;
}

int dpmgr_path_enable_irq(disp_path_handle dp_handle, void *cmdq_handle,
	enum DDP_IRQ_LEVEL irq_level)
{
	int i = 0;
	int ret = 0;
	int module_name, module_num;
	struct ddp_path_handle *handle;
	int *modules;
	struct DDP_MODULE_DRIVER *mod_drv;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);

	DDPDBG("path enable irq on scenario %s, level %d\n",
		   ddp_get_scenario_name(handle->scenario), irq_level);

	if (irq_level != DDP_IRQ_LEVEL_ALL)
		ddp_mutex_interrupt_disable(handle->hwmutexid, cmdq_handle);
	else
		ddp_mutex_interrupt_enable(handle->hwmutexid, cmdq_handle);


	for (i = module_num - 1; i >= 0; i--) {
		module_name = modules[i];
		mod_drv = ddp_get_module_driver(module_name);
		if (mod_drv && mod_drv->enable_irq) {
			DDPDBG("scenario %s, module %s enable irq level %d\n",
				ddp_get_scenario_name(handle->scenario),
				ddp_get_module_name(module_name), irq_level);
			ret += mod_drv->enable_irq(module_name, cmdq_handle,
				irq_level);
		}
	}
	return ret;
}

int dpmgr_path_reset(disp_path_handle dp_handle, int encmdq)
{
	int i = 0;
	int ret = 0;
	int error = 0;
	int module_name, module_num;
	int *modules;
	struct ddp_path_handle *handle;
	struct cmdqRecStruct *cmdqHandle;
	struct DDP_MODULE_DRIVER *mod_drv;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);
	cmdqHandle = encmdq ? handle->cmdqhandle : NULL;

	DISP_LOG_I("path reset on scenario %s\n",
		ddp_get_scenario_name(handle->scenario));
	/* first reset mutex */
	ddp_mutex_reset(handle->hwmutexid, cmdqHandle);

	for (i = 0; i < module_num; i++) {
		module_name = modules[i];
		mod_drv = ddp_get_module_driver(module_name);
		if (mod_drv != 0) {
			if (mod_drv->reset != 0) {
				ret = mod_drv->reset(module_name, cmdqHandle);
				if (ret != 0)
					error++;
			}
		}
	}
	return error > 0 ? -1 : 0;
}

static unsigned int dpmgr_is_PQ(enum DISP_MODULE_ENUM module)
{
	unsigned int isPQ = 0;

	switch (module) {
	case DISP_MODULE_COLOR0:
	case DISP_MODULE_CCORR0:
	case DISP_MODULE_AAL0:
	case DISP_MODULE_GAMMA0:
	case DISP_MODULE_DITHER0:
		isPQ = 1;
		break;
	default:
		isPQ = 0;
	}

	return isPQ;
}

int dpmgr_path_update_partial_roi(disp_path_handle dp_handle,
		struct disp_rect partial, void *cmdq_handle)
{
	return dpmgr_path_ioctl(dp_handle, cmdq_handle, DDP_PARTIAL_UPDATE,
			&partial);
}

int dpmgr_path_config(disp_path_handle dp_handle,
	struct disp_ddp_path_config *config, void *cmdq_handle)
{
	int i = 0;
	int module_name, module_num;
	struct ddp_path_handle *handle;
	int *modules;
	struct DDP_MODULE_DRIVER *mod_drv;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);

	DISP_LOG_V("%s ovl %d, rdma %d, wdma %d, dst %d on handle %p %s\n",
		__func__,
		config->ovl_dirty, config->rdma_dirty, config->wdma_dirty,
		config->dst_dirty, handle,
		ddp_get_scenario_name(handle->scenario));

	memcpy(&handle->last_config, config, sizeof(*config));
	for (i = 0; i < module_num; i++) {
		module_name = modules[i];
		mod_drv = ddp_get_module_driver(module_name);
		if (mod_drv == 0)
			continue;

		if (mod_drv->config != 0)
			mod_drv->config(module_name, config, cmdq_handle);

		if (disp_helper_get_option(DISP_OPT_BYPASS_PQ)
		    && dpmgr_is_PQ(module_name) == 1) {
			if (mod_drv->bypass != NULL)
				mod_drv->bypass(module_name, 1);
		}
	}
	return 0;
}

struct disp_ddp_path_config *dpmgr_path_get_last_config_notclear(
	disp_path_handle dp_handle)
{
	struct ddp_path_handle *handle = (struct ddp_path_handle *)dp_handle;

	ASSERT(dp_handle != NULL);
	return &handle->last_config;
}

struct disp_ddp_path_config *dpmgr_path_get_last_config(
	disp_path_handle dp_handle)
{
	struct ddp_path_handle *handle = (struct ddp_path_handle *)dp_handle;

	ASSERT(dp_handle != NULL);
	handle->last_config.ovl_dirty = 0;
	handle->last_config.rdma_dirty = 0;
	handle->last_config.wdma_dirty = 0;
	handle->last_config.dst_dirty = 0;
	handle->last_config.ovl_layer_dirty = 0;
	handle->last_config.ovl_layer_scanned = 0;
	handle->last_config.ovl_partial_dirty = 0;
	handle->last_config.sbch_enable = 0;
	return &handle->last_config;
}

void dpmgr_get_input_address(disp_path_handle dp_handle, unsigned long *addr)
{
	struct ddp_path_handle *handle = (struct ddp_path_handle *)dp_handle;
	int *modules = ddp_get_scenario_list(handle->scenario);

	if (modules[0] == DISP_MODULE_OVL0 ||
		modules[0] == DISP_MODULE_OVL1_2L)
		ovl_get_address(modules[0], addr);
	else if (modules[0] == DISP_MODULE_RDMA0 ||
		modules[0] == DISP_MODULE_RDMA1)
		rdma_get_address(modules[0], addr);
}

int dpmgr_path_build_cmdq(disp_path_handle dp_handle,
	void *trigger_loop_handle, enum CMDQ_STATE state, int reverse)
{
	int ret = 0;
	int i = 0;
	int module_name, module_num;
	struct ddp_path_handle *handle;
	int *modules;
	struct DDP_MODULE_DRIVER *mod_drv;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);

	if (reverse) {
		for (i = module_num - 1; i >= 0; i--) {
			module_name = modules[i];
			mod_drv = ddp_get_module_driver(module_name);
			if (mod_drv && mod_drv->build_cmdq)
				ret = mod_drv->build_cmdq(module_name,
					trigger_loop_handle, state);
		}
	} else {
		for (i = 0; i < module_num; i++) {
			module_name = modules[i];
			mod_drv = ddp_get_module_driver(module_name);
			if (mod_drv && mod_drv->build_cmdq) {
				ret = mod_drv->build_cmdq(module_name,
					trigger_loop_handle, state);
			}
		}
	}
	return ret;
}

int dpmgr_path_trigger(disp_path_handle dp_handle, void *trigger_loop_handle,
	int encmdq)
{
	struct ddp_path_handle *handle;
	int *modules;
	int module_num, module_name;
	int i;
	struct DDP_MODULE_DRIVER *mod_drv;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	DISP_LOG_I("dpmgr_path_trigger on scenario %s\n",
		ddp_get_scenario_name(handle->scenario));
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);

	ddp_mutex_enable(handle->hwmutexid, handle->scenario, handle->mode,
		trigger_loop_handle);
	for (i = 0; i < module_num; i++) {
		module_name = modules[i];
		mod_drv = ddp_get_module_driver(module_name);
		if (mod_drv == 0)
			continue;
		if (mod_drv->trigger == 0)
			continue;
		mod_drv->trigger(module_name, trigger_loop_handle);
	}
	return 0;
}

int dpmgr_path_flush(disp_path_handle dp_handle, int encmdq)
{
	struct ddp_path_handle *handle;
	struct cmdqRecStruct *cmdqHandle;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	cmdqHandle = encmdq ? handle->cmdqhandle : NULL;
	DDPDBG("path flush on scenario %s\n",
		ddp_get_scenario_name(handle->scenario));
	return ddp_mutex_enable(handle->hwmutexid, handle->scenario,
		handle->mode, cmdqHandle);
}

int dpmgr_path_power_off(disp_path_handle dp_handle, enum CMDQ_SWITCH encmdq)
{
	int i = 0;
	int module_name, module_num;
	struct ddp_path_handle *handle;
	int *modules;
	struct DDP_MANAGER_CONTEXT *c = _get_context();
	struct DDP_MODULE_DRIVER *mod_drv;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);

	DISP_LOG_I("path power off on scenario %s\n",
		ddp_get_scenario_name(handle->scenario));
	mutex_lock(&c->mutex_lock);
	for (i = 0; i < module_num; i++) {
		module_name = modules[i];
		mod_drv = ddp_get_module_driver(module_name);
		if (mod_drv && mod_drv->power_off) {
			mod_drv->power_off(module_name,
				encmdq ? handle->cmdqhandle : NULL);
		}
	}
	handle->power_state = 0;
	path_top_clock_off();
	mutex_unlock(&c->mutex_lock);
	return 0;
}

int dpmgr_path_power_on(disp_path_handle dp_handle, enum CMDQ_SWITCH encmdq)
{
	int i = 0;
	int module_name, module_num;
	int *modules;
	struct ddp_path_handle *handle;
	struct DDP_MANAGER_CONTEXT *c = _get_context();
	struct DDP_MODULE_DRIVER *mod_drv;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);

	DISP_LOG_I("path power on scenario %s\n",
		ddp_get_scenario_name(handle->scenario));
	mutex_lock(&c->mutex_lock);
	path_top_clock_on();
	for (i = 0; i < module_num; i++) {
		module_name = modules[i];
		mod_drv = ddp_get_module_driver(module_name);
		if (mod_drv && mod_drv->power_on) {
			mod_drv->power_on(module_name,
				encmdq ? handle->cmdqhandle : NULL);
		}
	}
	/* modules on this path will resume power on; */
	handle->power_state = 1;
	mutex_unlock(&c->mutex_lock);
	return 0;
}

int dpmgr_path_power_off_bypass_pwm(disp_path_handle dp_handle,
	enum CMDQ_SWITCH encmdq)
{
	int i = 0;
	int module_name, module_num;
	struct ddp_path_handle *handle;
	int *modules;
	struct DDP_MANAGER_CONTEXT *c = _get_context();
	struct DDP_MODULE_DRIVER *mod_drv;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);

	DISP_LOG_I("path power off on scenario %s\n",
		ddp_get_scenario_name(handle->scenario));
	mutex_lock(&c->mutex_lock);
	for (i = 0; i < module_num; i++) {
		module_name = modules[i];
		mod_drv = ddp_get_module_driver(module_name);
		if (mod_drv && mod_drv->power_off) {
			if (module_name == DISP_MODULE_PWM0) {
				DDPMSG(" %s power off -- bypass\n",
					ddp_get_module_name(module_name));
			} else {
				mod_drv->power_off(module_name,
					encmdq ? handle->cmdqhandle : NULL);
			}
		}
	}
	handle->power_state = 0;
	path_top_clock_off();
	mutex_unlock(&c->mutex_lock);
	return 0;
}

int dpmgr_path_power_on_bypass_pwm(disp_path_handle dp_handle,
	enum CMDQ_SWITCH encmdq)
{
	int i = 0;
	int module_name, module_num;
	int *modules;
	struct ddp_path_handle *handle;
	struct DDP_MANAGER_CONTEXT *c = _get_context();
	struct DDP_MODULE_DRIVER *mod_drv;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);

	DISP_LOG_I("path power on scenario %s\n",
		ddp_get_scenario_name(handle->scenario));
	mutex_lock(&c->mutex_lock);
	path_top_clock_on();
	for (i = 0; i < module_num; i++) {
		module_name = modules[i];
		mod_drv = ddp_get_module_driver(module_name);
		if (mod_drv && mod_drv->power_on) {
			if (module_name == DISP_MODULE_PWM0) {
				DDPMSG(" %s power on -- bypass\n",
					ddp_get_module_name(module_name));
			} else {
				mod_drv->power_on(module_name,
					encmdq ? handle->cmdqhandle : NULL);
			}
		}
	}
	/* modules on this path will resume power on; */
	handle->power_state = 1;
	mutex_unlock(&c->mutex_lock);
	return 0;
}

static int is_module_in_path(enum DISP_MODULE_ENUM module,
	struct ddp_path_handle *handle)
{
	struct DDP_MANAGER_CONTEXT *context = _get_context();

	ASSERT(module < DISP_MODULE_UNKNOWN);
	if (context->module_path_table[module] == handle)
		return 1;

	return 0;
}

int dpmgr_path_user_cmd(disp_path_handle dp_handle, unsigned int msg,
		unsigned long arg, void *cmdqhandle)
{
	int ret = -1;
	struct ddp_path_handle *handle = NULL;
	struct DDP_MODULE_DRIVER *mod_drv;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	/* DISP_LOG_W("dpmgr_path_user_cmd msg 0x%08x\n",msg); */
	switch (msg) {
	case DISP_IOCTL_AAL_EVENTCTL:
	case DISP_IOCTL_AAL_GET_HIST:
	case DISP_IOCTL_AAL_INIT_REG:
	case DISP_IOCTL_AAL_SET_PARAM:
	case DISP_IOCTL_SET_SMARTBACKLIGHT:
		/* TODO: just for verify rootcause, will be removed soon */
#ifndef CONFIG_FOR_SOURCE_PQ
		if (!is_module_in_path(DISP_MODULE_AAL0, handle))
			break;

		mod_drv = ddp_get_module_driver(DISP_MODULE_AAL0);
		if (!mod_drv || mod_drv->cmd == NULL)
			break;
		ret = mod_drv->cmd(DISP_MODULE_AAL0, msg, arg, cmdqhandle);
#endif
		break;
	case DISP_IOCTL_SET_GAMMALUT:
		mod_drv = ddp_get_module_driver(DISP_MODULE_GAMMA0);
		if (mod_drv->cmd == NULL)
			break;
		ret = mod_drv->cmd(DISP_MODULE_GAMMA0, msg, arg, cmdqhandle);
		break;
	case DISP_IOCTL_SET_CCORR:
	case DISP_IOCTL_CCORR_EVENTCTL:
	case DISP_IOCTL_CCORR_GET_IRQ:
		mod_drv = ddp_get_module_driver(DISP_MODULE_CCORR0);
		if (mod_drv->cmd == NULL)
			break;
		ret = mod_drv->cmd(DISP_MODULE_CCORR0, msg, arg, cmdqhandle);
		break;
	case DISP_IOCTL_SET_PQPARAM:
	case DISP_IOCTL_GET_PQPARAM:
	case DISP_IOCTL_SET_PQINDEX:
	case DISP_IOCTL_GET_PQINDEX:
	case DISP_IOCTL_SET_COLOR_REG:
	case DISP_IOCTL_SET_TDSHPINDEX:
	case DISP_IOCTL_GET_TDSHPINDEX:
	case DISP_IOCTL_SET_PQ_CAM_PARAM:
	case DISP_IOCTL_GET_PQ_CAM_PARAM:
	case DISP_IOCTL_SET_PQ_GAL_PARAM:
	case DISP_IOCTL_GET_PQ_GAL_PARAM:
	case DISP_IOCTL_PQ_SET_BYPASS_COLOR:
	case DISP_IOCTL_PQ_SET_WINDOW:
	case DISP_IOCTL_WRITE_REG:
	case DISP_IOCTL_READ_REG:
	case DISP_IOCTL_MUTEX_CONTROL:
	case DISP_IOCTL_PQ_GET_TDSHP_FLAG:
	case DISP_IOCTL_PQ_SET_TDSHP_FLAG:
	case DISP_IOCTL_PQ_GET_DC_PARAM:
	case DISP_IOCTL_PQ_SET_DC_PARAM:
	case DISP_IOCTL_PQ_GET_DS_PARAM:
	case DISP_IOCTL_PQ_GET_MDP_COLOR_CAP:
	case DISP_IOCTL_PQ_GET_MDP_TDSHP_REG:
	case DISP_IOCTL_WRITE_SW_REG:
	case DISP_IOCTL_READ_SW_REG:
		/* use function in COLOR module to do ioctl */
		ret = disp_color_ioctl(DISP_MODULE_COLOR0, msg, arg,
			cmdqhandle);
		break;
	default:
		DISP_LOG_W("dpmgr_path_user_cmd io not supported\n");
		break;
	}
	return ret;
}

int dpmgr_path_set_parameter(disp_path_handle dp_handle, int io_evnet,
	void *data)
{
	return 0;
}

int dpmgr_path_get_parameter(disp_path_handle dp_handle, int io_evnet,
	void *data)
{
	return 0;
}

int dpmgr_path_is_idle(disp_path_handle dp_handle)
{
	ASSERT(dp_handle != NULL);
	return !dpmgr_path_is_busy(dp_handle);
}

int dpmgr_path_is_busy(disp_path_handle dp_handle)
{
	int i = 0;
	int module_name, module_num;
	struct ddp_path_handle *handle;
	int *modules;
	struct DDP_MODULE_DRIVER *mod_drv;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);

	DISP_LOG_V("path check busy on scenario %s\n",
		ddp_get_scenario_name(handle->scenario));
	for (i = module_num - 1; i >= 0; i--) {
		module_name = modules[i];
		mod_drv = ddp_get_module_driver(module_name);
		if (mod_drv == 0)
			continue;
		if (mod_drv->is_busy != 0) {
			if (mod_drv->is_busy(module_name)) {
				DISP_LOG_V("%s is busy\n",
					ddp_get_module_name(module_name));
				return 1;
			}
		}
	}
	return 0;
}

int dpmgr_set_lcm_utils(disp_path_handle dp_handle, void *lcm_drv)
{
	int i = 0;
	int module_name, module_num;
	int *modules;
	struct ddp_path_handle *handle;
	struct DDP_MODULE_DRIVER *mod_drv;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);

	DISP_LOG_V("path set lcm drv handle %p\n", handle);
	for (i = 0; i < module_num; i++) {
		module_name = modules[i];
		mod_drv = ddp_get_module_driver(module_name);
		if (mod_drv != 0) {
			if ((mod_drv->set_lcm_utils != 0) && lcm_drv) {
				DDPDBG("%s set lcm utils\n",
					ddp_get_module_name(module_name));
				mod_drv->set_lcm_utils(module_name, lcm_drv);
			}
		}
	}
	return 0;
}

int dpmgr_enable_event(disp_path_handle dp_handle, enum DISP_PATH_EVENT event)
{
	struct ddp_path_handle *handle;
	struct DPMGR_WQ_HANDLE *wq_handle;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	wq_handle = &handle->wq_list[event];

	DDPDBG("enable event %s on scenario %s, irtbit 0x%x\n",
	       path_event_name(event),
	       ddp_get_scenario_name(handle->scenario),
	       handle->irq_event_map[event].irq_bit);

	if (!wq_handle->init) {
		init_waitqueue_head(&(wq_handle->wq));
		wq_handle->init = 1;
		wq_handle->data = 0;
		wq_handle->event = event;
	}
	return 0;
}

int dpmgr_map_event_to_irq(disp_path_handle dp_handle,
	enum DISP_PATH_EVENT event, enum DDP_IRQ_BIT irq_bit)
{
	struct ddp_path_handle *handle;
	struct DDP_IRQ_EVENT_MAPPING *irq_table;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	irq_table = handle->irq_event_map;

	if (event < DISP_PATH_EVENT_NUM) {
		DDPDBG("map event %s to irq 0x%x on scenario %s\n",
			path_event_name(event), irq_bit,
			ddp_get_scenario_name(handle->scenario));
		irq_table[event].irq_bit = irq_bit;
		return 0;
	}
	DISP_LOG_E("fail to map event %s to irq 0x%x on scenario %s\n",
		path_event_name(event), irq_bit,
		ddp_get_scenario_name(handle->scenario));
	return -1;
}

int dpmgr_disable_event(disp_path_handle dp_handle, enum DISP_PATH_EVENT event)
{
	struct ddp_path_handle *handle;
	struct DPMGR_WQ_HANDLE *wq_handle;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;

	DDPDBG("disable event %s on scenario %s\n",
		path_event_name(event),
		ddp_get_scenario_name(handle->scenario));
	wq_handle = &handle->wq_list[event];
	wq_handle->init = 0;
	wq_handle->data = 0;
	return 0;
}

int dpmgr_check_status_by_scenario(enum DDP_SCENARIO_ENUM scenario)
{
	int i = 0;
	int *modules = ddp_get_scenario_list(scenario);
	int module_num = ddp_get_module_num(scenario);
	struct DDP_MANAGER_CONTEXT *context = _get_context();

	DDPDUMP("--> check status on scenario %s\n",
		ddp_get_scenario_name(scenario));

	if (!(context->power_state)) {
		DDPDUMP("cannot check ddp status due to already power off\n");
		return 0;
	}

	ddp_check_path(scenario);

	{
		DDPDUMP("path:\n");
		for (i = 0; i < module_num; i++)
			DDPDUMP("%s-\n", ddp_get_module_name(modules[i]));

		DDPDUMP("\n");
	}

	for (i = 0; i < module_num; i++)
		ddp_dump_analysis(modules[i]);

	for (i = 0; i < module_num; i++)
		ddp_dump_reg(modules[i]);

	return 0;
}

int dpmgr_check_status(disp_path_handle dp_handle)
{
	int i = 0;
	int *modules;
	int module_num;
	struct ddp_path_handle *handle;
	struct DDP_MANAGER_CONTEXT *context = _get_context();

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);

	DDPDUMP("--> check status on scenario %s\n",
		ddp_get_scenario_name(handle->scenario));

	if (!(context->power_state)) {
		DDPDUMP("cannot check ddp status due to already power off\n");
		return 0;
	}

	ddp_dump_analysis(DISP_MODULE_CONFIG);
	ddp_check_path(handle->scenario);
	ddp_check_mutex(handle->hwmutexid, handle->scenario, handle->mode);

	/* dump path */
	{
		DDPDUMP("path:\n");
		for (i = 0; i < module_num; i++)
			DDPDUMP("%s-\n", ddp_get_module_name(modules[i]));

		DDPDUMP("\n");
	}
	ddp_dump_analysis(DISP_MODULE_MUTEX);

	for (i = 0; i < module_num; i++)
		ddp_dump_analysis(modules[i]);

	for (i = 0; i < module_num; i++)
		ddp_dump_reg(modules[i]);


	ddp_dump_reg(DISP_MODULE_CONFIG);
	ddp_dump_reg(DISP_MODULE_MUTEX);

	return 0;
}

void dpmgr_debug_path_status(int mutex_id)
{
	int i = 0;
	struct DDP_MANAGER_CONTEXT *content = _get_context();
	disp_path_handle handle = NULL;

	mutex_lock(&content->mutex_lock);
	if (mutex_id >= DISP_MUTEX_DDP_FIRST &&
	    mutex_id < (DISP_MUTEX_DDP_FIRST + DISP_MUTEX_DDP_COUNT)) {

		handle = (disp_path_handle) content->handle_pool[mutex_id];
		if (handle)
			dpmgr_check_status(handle);
	} else {
		for (i = DISP_MUTEX_DDP_FIRST;
			i < (DISP_MUTEX_DDP_FIRST + DISP_MUTEX_DDP_COUNT);
		    i++) {

			handle = (disp_path_handle) content->handle_pool[i];
			if (handle)
				dpmgr_check_status(handle);
		}
	}
	mutex_unlock(&content->mutex_lock);
}

int dpmgr_wait_event_timeout(disp_path_handle dp_handle,
	enum DISP_PATH_EVENT event, int timeout)
{
	int ret = -1;
	struct ddp_path_handle *handle;
	struct DPMGR_WQ_HANDLE *wq_handle;
	unsigned long long cur_time;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	wq_handle = &handle->wq_list[event];

	if (wq_handle->init) {
		cur_time = ktime_to_ns(ktime_get());
		mmprofile_log_ex(ddp_mmp_get_events()->event_wait,
			MMPROFILE_FLAG_PULSE, event, cur_time);

		ret = wait_event_interruptible_timeout(wq_handle->wq,
			cur_time < wq_handle->data, timeout);

		mmprofile_log_ex(ddp_mmp_get_events()->event_wait,
			MMPROFILE_FLAG_PULSE, event, ret);

		if (ret == 0) {
			DISP_LOG_E("wait %s timeout on scenario %s\n",
				path_event_name(event),
				ddp_get_scenario_name(handle->scenario));
			/* dpmgr_check_status(dp_handle); */
		} else if (ret < 0) {
			DISP_LOG_E("wait %s ir by other timeleft %d on %s\n",
				path_event_name(event), ret,
				ddp_get_scenario_name(handle->scenario));
		} else {
			DISP_LOG_V("received event %s timeleft %d on %s\n",
				path_event_name(event), ret,
				ddp_get_scenario_name(handle->scenario));
		}
		return ret;
	}
	DISP_LOG_E("wait event %s not initialized on scenario %s\n",
		path_event_name(event),
		ddp_get_scenario_name(handle->scenario));
	return ret;
}

int _dpmgr_wait_event(disp_path_handle dp_handle, enum DISP_PATH_EVENT event,
	unsigned long long *event_ts)
{
	int ret = -1;
	struct ddp_path_handle *handle;
	struct DPMGR_WQ_HANDLE *wq_handle;
	unsigned long long cur_time;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	wq_handle = &handle->wq_list[event];

	if (!wq_handle->init) {
		DISP_LOG_E("wait event %s not initialized on scenario %s\n",
			path_event_name(event),
			ddp_get_scenario_name(handle->scenario));
		return -2;
	}

	cur_time = ktime_to_ns(ktime_get());

	mmprofile_log_ex(ddp_mmp_get_events()->event_wait,
		MMPROFILE_FLAG_PULSE, event, cur_time);

	ret = wait_event_interruptible(wq_handle->wq,
		cur_time < wq_handle->data);

	mmprofile_log_ex(ddp_mmp_get_events()->event_wait,
		MMPROFILE_FLAG_PULSE, event, ret);

	if (ret < 0) {
		DISP_LOG_E("wait %s interrupt by other ret %d on scenario %s\n",
		   path_event_name(event), ret,
		   ddp_get_scenario_name(handle->scenario));
	}
	if (event_ts)
		*event_ts = wq_handle->data;

	return ret;
}

int dpmgr_wait_event(disp_path_handle dp_handle, enum DISP_PATH_EVENT event)
{
	return _dpmgr_wait_event(dp_handle, event, NULL);
}

int dpmgr_wait_event_ts(disp_path_handle dp_handle, enum DISP_PATH_EVENT event,
	unsigned long long *event_ts)
{
	return _dpmgr_wait_event(dp_handle, event, event_ts);
}

int dpmgr_signal_event(disp_path_handle dp_handle, enum DISP_PATH_EVENT event)
{
	struct ddp_path_handle *handle;
	struct DPMGR_WQ_HANDLE *wq_handle;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	wq_handle = &handle->wq_list[event];

	if (handle->wq_list[event].init) {
		wq_handle->data = ktime_to_ns(ktime_get());
		wake_up_interruptible(&(handle->wq_list[event].wq));
		mmprofile_log_ex(ddp_mmp_get_events()->event_signal,
			MMPROFILE_FLAG_PULSE,
			event, wq_handle->data);
	}
	return 0;
}

static void dpmgr_irq_handler(enum DISP_MODULE_ENUM module,
	unsigned int regvalue)
{
	int i = 0, j = 0;
	int irq_bits_num = 0;
	int irq_bit = 0;
	struct ddp_path_handle *handle = NULL;

	handle = find_handle_by_module(module);
	if (handle == NULL)
		return;

	irq_bits_num = ddp_get_module_max_irq_bit(module);
	for (i = 0; i <= irq_bits_num; i++) {
		if (!(regvalue & (0x1 << i)))
			continue;

		irq_bit = MAKE_DDP_IRQ_BIT(module, i);
		dprec_stub_irq(irq_bit);
		for (j = 0; j < DISP_PATH_EVENT_NUM; j++) {
			if (!handle->wq_list[j].init ||
				irq_bit != handle->irq_event_map[j].irq_bit)
				continue;
			dprec_stub_event(j);
			DDPIRQ("irq signal event %s on cycle %llu on %s\n",
		       path_event_name(j), handle->wq_list[j].data,
		       ddp_get_scenario_name(handle->scenario));

			dpmgr_signal_event(handle, j);
		}
	}
}

int dpmgr_init(void)
{
	DISP_LOG_I("ddp manager init\n");
	if (ddp_manager_init)
		return 0;

	ddp_manager_init = 1;
	ddp_debug_init();
	disp_init_irq();
	disp_register_irq_callback(dpmgr_irq_handler);
	return 0;
}

/* dpmgr_factory_mode_test
 *  dpmgr_factory_mode_reset
 *  to be implemented in dsi driver
 *  these two functions are used for external display factory mode
 */
int dpmgr_factory_mode_test(int module_name, void *cmdqhandle, void *config)
{
	if (ddp_get_module_driver(module_name) == 0 ||
		ddp_get_module_driver(module_name)->ioctl == 0)
		return 0;

	DISP_LOG_I(" %s factory_mode_test\n",
		ddp_get_module_name(DISP_MODULE_DPI));
	ddp_get_module_driver(DISP_MODULE_DPI)->ioctl(module_name, cmdqhandle,
		DDP_DPI_FACTORY_TEST, config);

	return 0;
}

int dpmgr_factory_mode_reset(int module_name, void *cmdqhandle, void *config)
{
	if (ddp_get_module_driver(module_name) == 0 ||
		ddp_get_module_driver(module_name)->ioctl == 0)
		return 0;

	DISP_LOG_I(" %s factory_mode_test\n",
		ddp_get_module_name(DISP_MODULE_DPI));
	ddp_get_module_driver(DISP_MODULE_DPI)->ioctl(module_name, cmdqhandle,
		DDP_DPI_FACTORY_TEST, config);

	return 0;
}

int dpmgr_wait_ovl_available(int ovl_num)
{
	int ret = 1;

	return ret;
}

int switch_module_to_nonsec(disp_path_handle dp_handle, void *cmdqhandle,
	const char *caller)
{
	int i = 0;
	int module_name;
	struct ddp_path_handle *handle;
	int *modules;
	int module_num;
	struct DDP_MODULE_DRIVER *mod_drv;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);
	DDPMSG("[SVP] switch module to nonsec on scenario %s, caller=%s\n",
		ddp_get_scenario_name(handle->scenario), caller);

	for (i = module_num - 1; i >= 0; i--) {
		module_name = modules[i];
		mod_drv = ddp_get_module_driver(module_name);
		if (mod_drv == 0 || mod_drv->switch_to_nonsec == 0)
			continue;
		mod_drv->switch_to_nonsec(module_name, cmdqhandle);
	}
	return 0;
}


int dpmgr_path_dsi_power_off(disp_path_handle dp_handle, void *cmdqhandle)
{
	int module_name;
	struct ddp_path_handle *handle;
	enum DISP_MODULE_ENUM dst_module;
	struct DDP_MODULE_DRIVER *mod_drv;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	dst_module = ddp_get_dst_module(handle->scenario);

	module_name = dst_module;
	mod_drv = ddp_get_module_driver(dst_module);
	if (mod_drv != 0) {
		if (mod_drv->power_off != 0) {
			DDPDBG(" %s power off\n",
				ddp_get_module_name(dst_module));
			mod_drv->power_off(dst_module, cmdqhandle);
		}
	}
	return 0;
}


int dpmgr_path_dsi_power_on(disp_path_handle dp_handle, void *cmdqhandle)
{
	int module_name;
	struct ddp_path_handle *handle;
	enum DISP_MODULE_ENUM dst_module;
	struct DDP_MODULE_DRIVER *mod_drv;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle *)dp_handle;
	dst_module = ddp_get_dst_module(handle->scenario);

	module_name = dst_module;
	mod_drv = ddp_get_module_driver(dst_module);
	if (mod_drv != 0) {
		if (mod_drv->power_on != 0) {
			DDPDBG(" %s power on\n",
				ddp_get_module_name(dst_module));
			mod_drv->power_on(dst_module, cmdqhandle);
		}
	}
	return 0;
}
