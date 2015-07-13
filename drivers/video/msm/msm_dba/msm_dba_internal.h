/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#ifndef _MSM_DBA_INTERNAL_H
#define _MSM_DBA_INTERNAL_H

#include <video/msm_dba.h>

struct msm_dba_client_info;
struct msm_dba_device_info;

/**
 * struct msm_dba_device_ops - Function pointers to device specific operations
 * @dev_power_on: Power on operation called by msm_dba_helper_power_on. Mutex
 *		   protection is handled by the caller.
 * @dev_video_on: Video on operation called by msm_dba_helper_video_on. Mutex
 *		   protection is handled by the caller.
 * @handle_interrupts: Function pointer called when an interrupt is fired. If
 *		        the bridge driver uses msm_dba_helper_register_irq
 *		        for handling interrupts, irq handler will call
 *		        handle_interrupts to figure out the event mask.
 * @unmask_interrupts: Function pointer called by irq handler for unmasking
 *		       interrupts.
 * @hdcp_reset: Function pointer to reset the HDCP block. This needs to be valid
 *		if HDCP monitor is used.
 * @hdcp_retry: Function pointer to retry HDCP authentication. This needs to be
 *		valid if HDCP monitor is used.
 * @write_reg: Function pointer to write to device specific register.
 * @read_reg: Function pointer to read device specific register.
 * @force_reset: Function pointer to force reset the device.
 * @dump_debug_info: Function pointer to trigger a dump to dmesg.
 *
 * The device operation function pointers are used if bridge driver uses helper
 * functions in place of some client operations. If used, the helper functions
 * will call the device function pointers to perform device specific
 * programming.
 */
struct msm_dba_device_ops {
	int (*dev_power_on)(struct msm_dba_device_info *dev, bool on);
	int (*dev_video_on)(struct msm_dba_device_info *dev,
			    struct msm_dba_video_cfg *cfg, bool on);
	int (*handle_interrupts)(struct msm_dba_device_info *dev, u32 *mask);
	int (*unmask_interrupts)(struct msm_dba_device_info *dev, u32 mask);
	int (*hdcp_reset)(struct msm_dba_device_info *dev);
	int (*hdcp_retry)(struct msm_dba_device_info *dev, u32 flags);
	int (*write_reg)(struct msm_dba_device_info *dev, u32 reg, u32 val);
	int (*read_reg)(struct msm_dba_device_info *dev, u32 reg, u32 *val);
	int (*force_reset)(struct msm_dba_device_info *dev, u32 flags);
	int (*dump_debug_info)(struct msm_dba_device_info *dev, u32 flags);
};

/**
 * struct msm_dba_device_info - Device specific information
 * @chip_name: chip name
 * @instance_id: Instance id
 * @caps: Capabilities of the bridge chip
 * @dev_ops: function pointers to device specific operations
 * @client_ops: function pointers to client operations
 * @dev_mutex: mutex for protecting device access
 * @hdcp_wq: HDCP workqueue for handling failures.
 * @client_list: list head for client list
 * @reg_fxn: Function pointer called when a client registers with dba driver
 * @dereg_fxn: Function pointer called when a client deregisters.
 * @power_status: current power status of device
 * @video_status: current video status of device
 * @audio_status: current audio status of device
 * @hdcp_on: hdcp enable status.
 * @enc-on: encryption enable status.
 * @hdcp_status: hdcp link status.
 * @hdcp_monitor_on: hdcp monitor status
 * @register_val: debug field used to support read register.
 *
 * Structure containing device specific information. This structure is allocated
 * by the bridge driver. This structure should be unique to each device.
 *
 */
struct msm_dba_device_info {
	char chip_name[MSM_DBA_CHIP_NAME_MAX_LEN];
	u32 instance_id;
	struct msm_dba_capabilities caps;
	struct msm_dba_device_ops dev_ops;
	struct msm_dba_ops client_ops;
	struct mutex dev_mutex;
	struct workqueue_struct *hdcp_wq;
	struct work_struct hdcp_work;
	struct list_head client_list;
	int (*reg_fxn)(struct msm_dba_client_info *client);
	int (*dereg_fxn)(struct msm_dba_client_info *client);

	bool power_status;
	bool video_status;
	bool audio_status;
	bool hdcp_on;
	bool enc_on;
	bool hdcp_status;
	bool hdcp_monitor_on;

	/* Debug info */
	u32 register_val;
};

/**
 * struct msm_dba_client_info - Client specific information
 * @dev: pointer to device information
 * @client_name: client name
 * @power_on: client power on status
 * @video_on: client video on status
 * @audio_on: client audio on status
 * @event_mask: client event mask for callbacks.
 * @cb: callback function for the client
 * @cb_data: callback data pointer.
 * @list: list pointer
 *
 * This structure is used to uniquely identify a client for a bridge chip. The
 * pointer to this structure is returned as a handle from
 * msm_dba_register_client.
 */
struct msm_dba_client_info {
	struct msm_dba_device_info *dev;
	char client_name[MSM_DBA_CLIENT_NAME_LEN];
	bool power_on;
	bool video_on;
	bool audio_on;
	u32 event_mask;
	msm_dba_cb cb;
	void *cb_data;
	struct list_head list;
};

/**
 * msm_dba_add_probed_device() - Add a new device to the probed devices list.
 * @info: Pointer to structure containing the device information. This should be
 *	  allocated by the specific bridge driver and kept until
 *	  msm_dba_remove_probed_device() is called.
 *
 * Once a bridge chip is initialized and probed, it should add its device to the
 * existing list of all probed display bridge chips. This list is maintained by
 * the MSM DBA driver and is checked whenever there is a client register
 * request.
 */
int msm_dba_add_probed_device(struct msm_dba_device_info *info);

/**
 * msm_dba_remove_probed_device() - Remove a device from the probed devices list
 * @info: Pointer to structure containing the device info. This should be the
 *	  same pointer used for msm_dba_add_probed_device().
 *
 * Bridge chip driver should call this to remove device from probed list.
 */
int msm_dba_remove_probed_device(struct msm_dba_device_info *info);

/**
 * msm_dba_get_probed_device() - Check if a device is present in the device list
 * @reg: Pointer to structure containing the chip info received from the client
 *	 driver
 * @info: Pointer to the device info pointer that will be returned if the device
 *	  has been found in the device list
 *
 * When clients of the MSM DBA driver call msm_dba_register_client(), the MSM
 * DBA driver will use this function to check if the specific device requested
 * by the client has been probed. If probed, function will return a pointer to
 * the device information structure.
 */
int msm_dba_get_probed_device(struct msm_dba_reg_info *reg,
			      struct msm_dba_device_info **info);

/**
 * msm_dba_helper_i2c_read() - perform an i2c read transaction
 * @client: i2c client pointer
 * @addr: i2c slave address
 * @reg: register where the data should be read from
 * @buf: buffer where the read data is stored.
 * @size: bytes to read from slave. buffer should be atleast size bytes.
 *
 * Helper function to perform a read from an i2c slave. Internally this calls
 * i2c_transfer().
 */
int msm_dba_helper_i2c_read(struct i2c_client *client,
			    u8 addr,
			    u8 reg,
			    char *buf,
			    u32 size);

/**
 * msm_dba_helper_i2c_write_buffer() - write buffer to i2c slave.
 * @client: i2c client pointer
 * @addr: i2c slave address
 * @buf: buffer where the data will be read from.
 * @size: bytes to write.
 *
 * Helper function to perform a write to an i2c slave. Internally this calls
 * i2c_transfer().
 */
int msm_dba_helper_i2c_write_buffer(struct i2c_client *client,
				    u8 addr,
				    u8 *buf,
				    u32 size);

/**
 * msm_dba_helper_i2c_write_byte() - write to a register on an i2c slave.
 * @client: i2c client pointer
 * @addr: i2c slave address
 * @reg: slave register to write to
 * @val: data to write.
 *
 * Helper function to perform a write to an i2c slave. Internally this calls
 * i2c_transfer().
 */
int msm_dba_helper_i2c_write_byte(struct i2c_client *client,
				  u8 addr,
				  u8 reg,
				  u8 val);

/**
 * msm_dba_helper_power_on() - power on bridge chip
 * @client: client handle
 * @on: on/off
 * @flags: flags
 *
 * This helper function can be used as power_on() function defined in struct
 * msm_dba_ops. Internally, this function does some bookkeeping to figure out
 * when to actually power on/off the device. If used, bridge driver should
 * provide a dev_power_on to do the device specific power change.
 */
int msm_dba_helper_power_on(void *client, bool on, u32 flags);

/**
 * msm_dba_helper_video_on() - video on bridge chip
 * @client: client handle
 * @on: on/off
 * @flags: flags
 *
 * This helper function can be used as video_on() function defined in struct
 * msm_dba_ops. Internally, this function does some bookkeeping to figure out
 * when to actually video on/off the device. If used, bridge driver should
 * provide a dev_video_on to do the device specific video change.
 */
int msm_dba_helper_video_on(void *client, bool on,
			    struct msm_dba_video_cfg *cfg, u32 flags);

/**
 * msm_dba_helper_interrupts_enable() - manage interrupt callbacks
 * @client: client handle
 * @on: on/off
 * @events_mask: events on which callbacks are required.
 * @flags: flags
 *
 * This helper function provides the functionality needed for interrupts_enable
 * function pointer in struct msm_dba_ops.
 */
int msm_dba_helper_interrupts_enable(void *client, bool on,
				     u32 events_mask, u32 flags);

/**
 * msm_dba_helper_get_caps() - return device capabilities
 * @client: client handle
 * @flags: flags
 *
 * Helper function to replace get_caps function pointer in struct msm_dba_ops
 * structure.
 */
int msm_dba_helper_get_caps(void *client, struct msm_dba_capabilities *caps);

/**
 * msm_dba_helper_register_irq() - register irq and handle interrupts.
 * @dev: pointer to device structure
 * @irq: irq number
 * @irq_flags: irq_flags.
 *
 * Helper function register an irq and handling interrupts. This will attach a
 * threaded interrupt handler to the irq provided as input. When the irq
 * handler is triggered, handler will call handle_interrupts in the device
 * specific functions pointers so that bridge driver can parse the interrupt
 * status registers and return the event mask. IRQ handler will use this event
 * mask to provide callbacks to the clients. Once the callbacks are done,
 * handler will call unmask_interrupts() before returning,
 */
int msm_dba_helper_register_irq(struct msm_dba_device_info *dev,
				u32 irq, u32 irq_flags);

/**
 * msm_dba_register_hdcp_monitor() - kicks off monitoring for hdcp failures
 * @dev: pointer to device structure.
 * @enable: enable/disable
 *
 * Helper function to enable HDCP monitoring. This should be called only if irq
 * is handled throught msm dba helper functions.
 */
int msm_dba_register_hdcp_monitor(struct msm_dba_device_info *dev, bool enable);

/**
 * msm_dba_helper_sysfs_init() - create sysfs attributes for debugging
 * @dev: pointer to struct device structure.
 *
 */
int msm_dba_helper_sysfs_init(struct device *dev);

/**
 * msm_dba_helper_sysfs_remove() - remove sysfs attributes
 * @dev: pointer to struct device structure.
 *
 */
void msm_dba_helper_sysfs_remove(struct device *dev);

/**
 * msm_dba_helper_force_reset() - force reset bridge chip
 * @client: client handle
 * @flags: flags
 *
 * Helper function to replace force_reset function pointer in struct msm_dba_ops
 * structure. Driver should set dev_ops.force_reset to a valid function.
 */
int msm_dba_helper_force_reset(void *client, u32 flags);
#endif /* _MSM_DBA_INTERNAL_H */
