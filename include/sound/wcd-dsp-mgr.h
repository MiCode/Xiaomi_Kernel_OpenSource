/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __WCD_DSP_MGR_H__
#define __WCD_DSP_MGR_H__

#include <linux/types.h>

/*
 * These enums correspond to the component types
 * that wcd-dsp-manager driver will use. The order
 * of the enums specifies the order in which the
 * manager driver will perform the sequencing.
 * Changing this will cause the sequencing order
 * to be changed as well.
 */
enum wdsp_cmpnt_type {
	/* Component to control the DSP */
	WDSP_CMPNT_CONTROL = 0,
	/* Component to perform data transfer to/from DSP */
	WDSP_CMPNT_TRANSPORT,
	/* Component that performs high level IPC */
	WDSP_CMPNT_IPC,

	WDSP_CMPNT_TYPE_MAX,
};

enum wdsp_event_type {
	/* Initialization related */
	WDSP_EVENT_POST_INIT,

	/* Image download related */
	WDSP_EVENT_PRE_DLOAD_CODE,
	WDSP_EVENT_DLOAD_SECTION,
	WDSP_EVENT_POST_DLOAD_CODE,
	WDSP_EVENT_PRE_DLOAD_DATA,
	WDSP_EVENT_POST_DLOAD_DATA,
	WDSP_EVENT_DLOAD_FAILED,

	WDSP_EVENT_READ_SECTION,

	/* DSP boot related */
	WDSP_EVENT_PRE_BOOTUP,
	WDSP_EVENT_DO_BOOT,
	WDSP_EVENT_POST_BOOTUP,
	WDSP_EVENT_PRE_SHUTDOWN,
	WDSP_EVENT_DO_SHUTDOWN,
	WDSP_EVENT_POST_SHUTDOWN,

	/* IRQ handling related */
	WDSP_EVENT_IPC1_INTR,

	/* Suspend/Resume related */
	WDSP_EVENT_SUSPEND,
	WDSP_EVENT_RESUME,
};

enum wdsp_signal {
	/* Hardware generated interrupts signalled to manager */
	WDSP_IPC1_INTR,
	WDSP_ERR_INTR,

	/* Other signals */
	WDSP_CDC_DOWN_SIGNAL,
	WDSP_CDC_UP_SIGNAL,
};

/*
 * wdsp_cmpnt_ops: ops/function callbacks for components
 * @init: called by manager driver, component is expected
 *	  to initialize itself in this callback
 * @deinit: called by manager driver, component should
 *	    de-initialize itself in this callback
 * @event_handler: Event handler for each component, called
 *		   by the manager as per sequence
 */
struct wdsp_cmpnt_ops {
	int (*init)(struct device *, void *priv_data);
	int (*deinit)(struct device *, void *priv_data);
	int (*event_handler)(struct device *, void *priv_data,
			     enum wdsp_event_type, void *data);
};

struct wdsp_img_section {
	u32 addr;
	size_t size;
	u8 *data;
};

struct wdsp_err_signal_arg {
	bool mem_dumps_enabled;
	u32 remote_start_addr;
	size_t dump_size;
};

/*
 * wdsp_ops: ops/function callbacks for manager driver
 * @register_cmpnt_ops: components will use this to register
 *			their own ops to manager driver
 * @get_dev_for_cmpnt: components can use this to get handle
 *		       to struct device * of any other component
 * @signal_handler: callback to notify manager driver that signal
 *		    has occurred. Cannot be called from interrupt
 *		    context as this can sleep
 * @vote_for_dsp: notifies manager that dsp should be booted up
 * @suspend: notifies manager that one component wants to suspend.
 *	     Manager will make sure to suspend all components in order
 * @resume: notifies manager that one component wants to resume.
 *	    Manager will make sure to resume all components in order
 */

struct wdsp_mgr_ops {
	int (*register_cmpnt_ops)(struct device *wdsp_dev,
				  struct device *cdev,
				  void *priv_data,
				  struct wdsp_cmpnt_ops *ops);
	struct device *(*get_dev_for_cmpnt)(struct device *wdsp_dev,
					    enum wdsp_cmpnt_type type);
	int (*signal_handler)(struct device *wdsp_dev,
			      enum wdsp_signal signal, void *arg);
	int (*vote_for_dsp)(struct device *wdsp_dev, bool vote);
	int (*suspend)(struct device *wdsp_dev);
	int (*resume)(struct device *wdsp_dev);
};

#endif /* end of __WCD_DSP_MGR_H__ */
