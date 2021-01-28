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

#ifndef __DDP_PATH_MANAGER_H__
#define __DDP_PATH_MANAGER_H__

#include <linux/sched.h>
#include "ddp_info.h"
#include "ddp_path.h"

#include "cmdq_record.h"

#define MAKE_DDP_IRQ_BIT(module, shift)	  ((module<<24)|(0x1<<shift))
#define IRQBIT_MODULE(irqbit)             (irqbit >> 24)
#define IRQBIT_BIT(irqbit)                (irqbit & 0xffffff)

/* IRQ and module are combined to consist DDP IRQ */
enum DDP_IRQ_BIT {
	DDP_IRQ_OVL0_FRAME_COMPLETE = MAKE_DDP_IRQ_BIT(DISP_MODULE_OVL0, 1),
	DDP_IRQ_AAL0_OUT_END_FRAME = MAKE_DDP_IRQ_BIT(DISP_MODULE_AAL0, 1),

	DDP_IRQ_RDMA0_REG_UPDATE = MAKE_DDP_IRQ_BIT(DISP_MODULE_RDMA0, 0),
	DDP_IRQ_RDMA0_START = MAKE_DDP_IRQ_BIT(DISP_MODULE_RDMA0, 1),
	DDP_IRQ_RDMA0_DONE = MAKE_DDP_IRQ_BIT(DISP_MODULE_RDMA0, 2),
	DDP_IRQ_RDMA0_ABNORMAL = MAKE_DDP_IRQ_BIT(DISP_MODULE_RDMA0, 3),
	DDP_IRQ_RDMA0_UNDERFLOW = MAKE_DDP_IRQ_BIT(DISP_MODULE_RDMA0, 4),
	DDP_IRQ_RDMA0_TARGET_LINE = MAKE_DDP_IRQ_BIT(DISP_MODULE_RDMA0, 5),

	DDP_IRQ_RDMA1_REG_UPDATE = MAKE_DDP_IRQ_BIT(DISP_MODULE_RDMA1, 0),
	DDP_IRQ_RDMA1_START = MAKE_DDP_IRQ_BIT(DISP_MODULE_RDMA1, 1),
	DDP_IRQ_RDMA1_DONE = MAKE_DDP_IRQ_BIT(DISP_MODULE_RDMA1, 2),
	DDP_IRQ_RDMA1_ABNORMAL = MAKE_DDP_IRQ_BIT(DISP_MODULE_RDMA1, 3),
	DDP_IRQ_RDMA1_UNDERFLOW = MAKE_DDP_IRQ_BIT(DISP_MODULE_RDMA1, 4),
	DDP_IRQ_RDMA1_TARGET_LINE = MAKE_DDP_IRQ_BIT(DISP_MODULE_RDMA1, 5),

	DDP_IRQ_WDMA0_FRAME_COMPLETE = MAKE_DDP_IRQ_BIT(DISP_MODULE_WDMA0, 0),

	DDP_IRQ_DSI0_CMD_DONE = MAKE_DDP_IRQ_BIT(DISP_MODULE_DSI0, 1),
	DDP_IRQ_DSI0_EXT_TE = MAKE_DDP_IRQ_BIT(DISP_MODULE_DSI0, 2),
	DDP_IRQ_DSI0_FRAME_DONE = MAKE_DDP_IRQ_BIT(DISP_MODULE_DSI0, 4),

	DDP_IRQ_DPI_VSYNC = MAKE_DDP_IRQ_BIT(DISP_MODULE_DPI, 0),

	DDP_IRQ_MUTEX0_SOF = MAKE_DDP_IRQ_BIT(DISP_MODULE_MUTEX, 0),
	DDP_IRQ_MUTEX1_SOF = MAKE_DDP_IRQ_BIT(DISP_MODULE_MUTEX, 1),

	DDP_IRQ_UNKNOWN = MAKE_DDP_IRQ_BIT(DISP_MODULE_UNKNOWN, 0),

};

/* path handle */
#define disp_path_handle void *

/* dpmgr_set_power_state
 * primary display init: set power state = 1
 * primary display suspend: set power state = 0
 * primary display resume: set power state = 1
 * lowpower...
 */
void dpmgr_set_power_state(unsigned int state);

/* Init ddp manager, now only register irq handler to ddp_irq.c
 * return 0 if ok or -1 if fail.
 */
int dpmgr_init(void);

/**
 * dpmgr_create_path - create disp path handle
 *
 * it will assign mutex to this handle,
 * and cache this handle to modules in scenario, and will assign
 * default irq event to this handle.
 * return NULL if fail to create handle.
 * scenario: used to demininate path, and  ddp manager will do operations
 *                according to current scenario.
 * @cmdq_handle: will save current config cmdqhandle, and if cmdq is enable,
 * will use cmdq to write regitsers.
 */
disp_path_handle dpmgr_create_path(enum DDP_SCENARIO_ENUM scenario,
				   struct cmdqRecStruct *cmdq_handle);

int dpmgr_get_scenario(disp_path_handle dp_handle);

/* NOTES: modify path should call API like this :*/
/*old_scenario = dpmgr_get_scenario(handle);*/
/*dpmgr_modify_path_power_on_new_modules();*/
/*dpmgr_modify_path();*/
/*after cmdq handle exec done:*/
/*dpmgr_modify_path_power_off_old_modules();*/
int dpmgr_modify_path(disp_path_handle dp_handle,
		      enum DDP_SCENARIO_ENUM new_scenario,
		      struct cmdqRecStruct *cmdq_handle,
		      enum DDP_MODE isvdomode, int sw_only);

/**
 * destroy path, it will release mutex to pool, and disconnect path,
 * clear  mapping between handle and modules.
 * return 0;
 */
int dpmgr_destroy_path(disp_path_handle dp_handle,
		       struct cmdqRecStruct *cmdq_handle);

/* only destroy handle, don't disconnect path */
int dpmgr_destroy_path_handle(disp_path_handle dp_handle);

/**
 * add dump to path, for primary path , support OVL0, UFOE and OD.
 * for sub path ,support OVL1.
 * return 0 if success or -1 if fail.
 * dp_handle: disp path handle.
 */
int dpmgr_path_memout_clock(disp_path_handle dp_handle, int clock_switch);

int dpmgr_path_add_memout(disp_path_handle dp_handle,
			  enum DISP_MODULE_ENUM engine, void *cmdq_handle);

/**
 * remove dump to path. should match between  add dump and remove dump.
 * return 0 if success or -1 if fail.
 * dp_handle: disp path handle.
 * encmdq: 1 use command queue, 0 not.
 */
int dpmgr_path_remove_memout(disp_path_handle dp_handle, void *cmdq_handle);

/**
 * dpmgr_path_get_mutex - query current path mutex
 * @dp_handle: disp path handle
 *
 * return mutex
 */
int dpmgr_path_get_mutex(disp_path_handle dp_handle);

/**
 * dpmgr_path_get_dst_module - query current dst module
 * @dp_handle: disp path handle
 *
 * return module enum
 */
enum DISP_MODULE_ENUM dpmgr_path_get_dst_module(disp_path_handle dp_handle);
enum dst_module_type dpmgr_path_get_dst_module_type(disp_path_handle dp_handle);

/**
 * @dp_handle: disp path handle
 *
 * set dst module, the default dst module maybe not right,
 * so set real dst module on this path.
 * return 0.
 * dst_module(one of bellow):
 *   DISP_MODULE_DSI0
 *   DISP_MODULE_DSI1
 *   DISP_MODULE_DSIDUAL(DISP_MODULE_DSI0+DISP_MODULE_DSI1)
 *   DISP_MODULE_DPI
 */
int dpmgr_path_set_dst_module(disp_path_handle dp_handle,
			      enum DISP_MODULE_ENUM dst_module);

/**
 * @dp_handle: disp path handle
 *
 * set mode type(sof source): cmd or video mode.
 * return 0.
 * is_vdo_mode:  0 is cmd mode, 1 is video mode.
 */
int dpmgr_path_set_video_mode(disp_path_handle dp_handle, int is_vdo_mode);

/**
 * dpmgr_path_init - init path
 * @dp_handle: disp path handle
 * @encmdq: 1 use command queue, 0 not
 *
 * it will set mutex according to modules on this path
 * and sof sorce. And it will connect path, then initialize modules
 * on this path.
 * return 0.
 */
int dpmgr_path_init(disp_path_handle dp_handle, int encmdq);

/**
 * dpmgr_path_connect - connect path
 * @dp_handle: disp path handle
 * @encmdq: 1 use command queue, 0 not
 *
 * it will set mutex according to modules on this path
 * and sof sorce.
 * and it will connect path , then initialize modules on this path.
 * return 0.
 */
int dpmgr_path_connect(disp_path_handle dp_handle, int encmdq);

/**
 * dpmgr_path_disconnect - disconnect path
 * @dp_handle: disp path handle
 * @encmdq: 1 use command queue, 0 not
 *
 * it will set mutex according to modules on this path
 * and sof sorce.
 * and it will connect path , then initialize modules on this path.
 * return 0.
 */
int dpmgr_path_disconnect(disp_path_handle dp_handle, int encmdq);

/**
 * dpmgr_path_deinit - deinit path
 * @dp_handle: disp path handle
 * @encmdq: 1 use command queue, 0 not
 *
 * It will clear mutex and dissconnect path.
 * return 0.
 */
int dpmgr_path_deinit(disp_path_handle dp_handle, int encmdq);

/**
 * dpmgr_path_start - start path
 * @dp_handle: disp path handle
 * @encmdq: 1 use command queue, 0 not
 *
 * it will start this path by calling each drviers start function.
 * return 0.
 */
int dpmgr_path_start(disp_path_handle dp_handle, int encmdq);


/**
 * dpmgr_path_stop - start path
 * @dp_handle: disp path handle
 * @encmdq: 1 use command queue, 0 not
 *
 * it will stop this path by calling each drviers stop function.
 * return 0.
 */
int dpmgr_path_stop(disp_path_handle dp_handle, int encmdq);


/**
 * dpmgr_path_reset - reset path
 * @dp_handle: disp path handle
 * @encmdq: 1 use command queue, 0 not
 *
 * it will reset this path by calling each drviers reset function.
 * return 0.
 */
int dpmgr_path_reset(disp_path_handle dp_handle, int encmdq);


/**
 * dpmgr_path_config - config data
 * @dp_handle: disp path handle.
 * @encmdq: 1 use command queue, 0 not.
 *
 * it will config input or output data of  this path.
 * now config contains three parts:
 *    1. dst dirty .it means dst with & dst hight need be updated.
 *    2. ovl dirty. it means ovl config need be updated.
 *    3. rdma dirty. it means rdma need be updated.
 *    4. wdma dirty. it means wdam need be updated.
 * return 0.
 */
int dpmgr_path_config(disp_path_handle dp_handle,
		      struct disp_ddp_path_config *config, void *cmdq_handle);

int dpmgr_path_update_partial_roi(disp_path_handle dp_handle,
				struct disp_rect partial, void *cmdq_handle);

/**
 * dpmgr_path_flush - path flush
 * @dp_handle: disp path handle
 * @encmdq: 1 use command queue, 0 not
 *
 * this will enable mutex
 * return 0.
 */
int dpmgr_path_flush(disp_path_handle dp_handle, int encmdq);

/**
 * dpmgr_check_status
 * @dp_handle: disp path handle
 *
 * this will dump modules info on this path.
 * return 0.
 */
int dpmgr_check_status(disp_path_handle dp_handle);
int dpmgr_check_status_by_scenario(enum DDP_SCENARIO_ENUM scenario);

/**
 * dpmgr_debug_path_status
 * @mutex_id: mutex idex[0-4], if not in this range, will dump all.
 *
 * this will dump modules info on mutex path.
 * return 0.
 */
void dpmgr_debug_path_status(int mutex_id);

/**
 * dpmgr_path_build_cmdq
 * @dp_handle: disp path handle
 * @trigger_loop_handle:  triger thread
 *
 * this will deal with cmdq message:
 * return 0.
 * state :
 *      CMDQ_BEFORE_STREAM_SOF
 *	CMDQ_WAIT_STREAM_EOF_EVENT
 *	CMDQ_CHECK_IDLE_AFTER_STREAM_EOF
 *	CMDQ_AFTER_STREAM_EOF
 */
int dpmgr_path_build_cmdq(disp_path_handle dp_handle, void *trigger_loop_handle,
			  enum CMDQ_STATE state, int reverse);

/**
 * dpmgr_path_trigger
 * @dp_handle: disp path handle
 * @trigger_loop_handle: trigger thread
 *
 * this will trigger this path. it will trigger each module and enable mutex.
 * return 0.
 */
int dpmgr_path_trigger(disp_path_handle dp_handle, void *trigger_loop_handle,
		       int encmdq);


/**
 * dpmgr_signal_event
 * @dp_handle: disp path handle
 * event: path event
 *
 * set signal to event. this if not irq signal, but user send event
 * return 0.
 */
int dpmgr_signal_event(disp_path_handle dp_handle, enum DISP_PATH_EVENT event);


/**
 * enable init will initialize wakequeue.
 * return 0.
 * dp_handle:   disp path handle.
 * event: path event.
 */
int dpmgr_enable_event(disp_path_handle dp_handle, enum DISP_PATH_EVENT event);


/**
 * dpmgr_disable_event - disable event
 * @dp_handle: disp path handle
 * @event: path event
 *
 * related irq will not be received.
 * return 0.
 */
int dpmgr_disable_event(disp_path_handle dp_handle, enum DISP_PATH_EVENT event);

/**
 * dpmgr_map_event_to_irq
 * @dp_handle: disp path handle
 * @event: path event
 * @irq_bit
 *
 * map event to irq can change mappling between path event and irq
 * return 0.
 */
int dpmgr_map_event_to_irq(disp_path_handle dp_handle,
			   enum DISP_PATH_EVENT event,
			   enum DDP_IRQ_BIT irq_bit);

/**
 * dpmgr_wait_event_timeout - wait event timeout
 * event: disp event
 * timeout:(ms)
 *
 * return
 *     < 0,  error.
 *     0      timeout
 *     > 0   no  timeout
 */
int dpmgr_wait_event_timeout(disp_path_handle dp_handle,
			     enum DISP_PATH_EVENT event, int timeout);

/**
 * dpmgr_wait_event - wait event
 * event: disp event
 * timeout: (ms)
 *
 * return :
 * 0 , wait succesfull.
 * <0, wait error.
 */
int dpmgr_wait_event(disp_path_handle dp_handle, enum DISP_PATH_EVENT event);
int dpmgr_wait_event_ts(disp_path_handle dp_handle, enum DISP_PATH_EVENT event,
			unsigned long long *event_ts);

/**
 * dpmgr_path_power_on
 * @dp_handle: disp path handle
 * @encmdq: 1 use command queue, 0 not
 *
 * power on, turn on each modules clk.
 * return 0.
 */
int dpmgr_path_power_on(disp_path_handle dp_handle, enum CMDQ_SWITCH encmdq);
int dpmgr_path_power_on_bypass_pwm(disp_path_handle dp_handle,
				   enum CMDQ_SWITCH encmdq);

/**
 * dpmgr_path_power_off - power off
 * @dp_handle: disp path handle
 * @encmdq: 1 use command queue, 0 not
 *
 * turn off each modules clk, if all hande are closed.
 * top clock will be off.
 * return 0.
 */
int dpmgr_path_power_off(disp_path_handle dp_handle, enum CMDQ_SWITCH encmdq);
int dpmgr_path_power_off_bypass_pwm(disp_path_handle dp_handle,
				    enum CMDQ_SWITCH encmdq);

/**
 * dpmgr_set_lcm_utils
 * @dp_handle: disp path handle
 * @lcm_drv: lcm driver
 *
 * set lcm utils. now only dis/dpi used.
 * return 0.
 */
int dpmgr_set_lcm_utils(disp_path_handle dp_handle, void *lcm_drv);


/**
 * dpmgr_path_is_busy
 * @dp_handle: disp path handle
 *
 * check if this path is busy. it wil check each module on this path.
 * return 0.
 */
int dpmgr_path_is_busy(disp_path_handle dp_handle);

/**
 * dpmgr_path_is_idle
 * @dp_handle: disp path handle
 *
 * check if this path is idle. it wil check each module on this path.
 * return 0 if idle.
 */
int dpmgr_path_is_idle(disp_path_handle dp_handle);

/**
 * dpmgr_path_user_cmd - add parameter to this path.
 * return 0.
 * @dp_handle: disp path handle.
 * @msg: not defined.
 * @arg    :  data
 * @cmdq_handle: cmdq handle
 */
int dpmgr_path_user_cmd(disp_path_handle dp_handle, unsigned int msg,
		unsigned long arg, void *cmdqhandle);

/**
 * dpmgr_path_set_parameter - add parameter to this path
 * @dp_handle: disp path handle
 * @io_evnet: not defined
 * @data: data
 */
int dpmgr_path_set_parameter(disp_path_handle dp_handle, int io_evnet,
			     void *data);

/**
 * dpmgr_path_get_parameter - get parameter of this path
 * @dp_handle: disp path handle
 * @io_evnet: not defined
 * @data: data
 *
 * return 0.
 */
int dpmgr_path_get_parameter(disp_path_handle dp_handle, int io_evnet,
			     void *data);

/**
 * dpmgr_path_ioctl - dpmgr_ioctl
 * @dp_handle: disp path handle
 * @cmdq_handle: cmdq handle
 * @ioctl_cmd: ioctl cmd
 * @params: ioctl parameters
 *
 * it will call ioctl of ddp modules ioctl() to do some
 * special config or setting.
 * return 0.
 */
int dpmgr_path_ioctl(disp_path_handle dp_handle, void *cmdq_handle,
		     enum DDP_IOCTL_NAME ioctl_cmd, void *params);

int dpmgr_path_enable_irq(disp_path_handle dp_handle, void *cmdq_handle,
			  enum DDP_IRQ_LEVEL irq_level);

/**
 * dpmgr_path_get_last_config - get last config parameter of path
 * @dp_handle: disp path handle
 *
 * return  pointer to last config
 */
struct disp_ddp_path_config *dpmgr_path_get_last_config(
						disp_path_handle dp_handle);
struct disp_ddp_path_config *dpmgr_path_get_last_config_notclear(
						disp_path_handle dp_handle);

void dpmgr_get_input_buffer(disp_path_handle dp_handle, unsigned long *addr);
int dpmgr_module_notify(enum DISP_MODULE_ENUM module,
			enum DISP_PATH_EVENT event);

int dpmgr_wait_ovl_available(int ovl_num);
int switch_module_to_nonsec(disp_path_handle dp_handle, void *cmdqhandle,
			    const char *caller);

/**
 * dpmgr_get_input_address - get physical address from register
 *
 * dpmgr_get_input_address for extenal display
 * return address in addr[]
 */
void dpmgr_get_input_address(disp_path_handle dp_handle, unsigned long *addr);

/**
 * dpmgr_factory_mode_test - dpmgr_factory_mode_test for extenal display
 *
 * to kick dsi1 to show a pattern
 */
int dpmgr_factory_mode_test(int module_name, void *cmdqhandle, void *config);
int dpmgr_factory_mode_reset(int module_name, void *cmdqhandle, void *config);

#endif /* __DDP_PATH_MANAGER_H__ */
