/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef MSM_MHI_H
#define MSM_MHI_H
#include <linux/types.h>
#include <linux/device.h>

#define MHI_DMA_MASK       0xFFFFFFFFFFULL
#define MHI_MAX_MTU        0xFFFF

struct mhi_client_config;
struct mhi_device_ctxt;

enum MHI_CLIENT_CHANNEL {
	MHI_CLIENT_LOOPBACK_OUT = 0,
	MHI_CLIENT_LOOPBACK_IN = 1,
	MHI_CLIENT_SAHARA_OUT = 2,
	MHI_CLIENT_SAHARA_IN = 3,
	MHI_CLIENT_DIAG_OUT = 4,
	MHI_CLIENT_DIAG_IN = 5,
	MHI_CLIENT_SSR_OUT = 6,
	MHI_CLIENT_SSR_IN = 7,
	MHI_CLIENT_QDSS_OUT = 8,
	MHI_CLIENT_QDSS_IN = 9,
	MHI_CLIENT_EFS_OUT = 10,
	MHI_CLIENT_EFS_IN = 11,
	MHI_CLIENT_MBIM_OUT = 12,
	MHI_CLIENT_MBIM_IN = 13,
	MHI_CLIENT_QMI_OUT = 14,
	MHI_CLIENT_QMI_IN = 15,
	MHI_CLIENT_IP_CTRL_0_OUT = 16,
	MHI_CLIENT_IP_CTRL_0_IN = 17,
	MHI_CLIENT_IP_CTRL_1_OUT = 18,
	MHI_CLIENT_IP_CTRL_1_IN = 19,
	MHI_CLIENT_DCI_OUT = 20,
	MHI_CLIENT_DCI_IN = 21,
	MHI_CLIENT_TF_OUT = 22,
	MHI_CLIENT_TF_IN = 23,
	MHI_CLIENT_BL_OUT = 24,
	MHI_CLIENT_BL_IN = 25,
	MHI_CLIENT_DUN_OUT = 32,
	MHI_CLIENT_DUN_IN = 33,
	MHI_CLIENT_IPC_ROUTER_OUT = 34,
	MHI_CLIENT_IPC_ROUTER_IN = 35,
	MHI_CLIENT_IP_SW_1_OUT = 36,
	MHI_CLIENT_IP_SW_1_IN = 37,
	MHI_CLIENT_IP_SW_2_OUT = 38,
	MHI_CLIENT_IP_SW_2_IN = 39,
	MHI_CLIENT_IP_SW_3_OUT = 40,
	MHI_CLIENT_IP_SW_3_IN = 41,
	MHI_CLIENT_CSVT_OUT = 42,
	MHI_CLIENT_CSVT_IN = 43,
	MHI_CLIENT_SMCT_OUT = 44,
	MHI_CLIENT_SMCT_IN = 45,
	MHI_CLIENT_RESERVED_1_LOWER = 46,
	MHI_CLIENT_RESERVED_1_UPPER = 99,
	MHI_CLIENT_IP_HW_0_OUT = 100,
	MHI_CLIENT_IP_HW_0_IN = 101,
	MHI_CLIENT_IP_HW_ADPL_IN = 102,
	MHI_CLIENT_RESERVED_2_LOWER = 103,
	MHI_CLIENT_RESERVED_2_UPPER = 127,
	MHI_MAX_CHANNELS = 103
};

enum MHI_CB_REASON {
	MHI_CB_XFER,
	MHI_CB_MHI_DISABLED,
	MHI_CB_MHI_ENABLED,
	MHI_CB_MHI_SHUTDOWN,
	MHI_CB_SYS_ERROR,
};

enum MHI_FLAGS {
	MHI_EOB = 0x100,
	MHI_EOT = 0x200,
	MHI_CHAIN = 0x1,
	MHI_FLAGS_reserved = 0x80000000,
};

struct mhi_result {
	void *user_data;
	void *buf_addr;
	size_t bytes_xferd;
	int transaction_status;
	enum MHI_FLAGS flags;
};

struct mhi_cb_info {
	struct mhi_result *result;
	enum MHI_CB_REASON cb_reason;
	u32 chan;
};

struct mhi_client_info_t {
	enum MHI_CLIENT_CHANNEL chan;
	const struct device *dev;
	const char *node_name;
	void (*mhi_client_cb)(struct mhi_cb_info *);
	bool pre_allocate;
	size_t max_payload;
	void *user_data;
};

struct mhi_client_handle {
	u32 dev_id;
	u32 domain;
	u32 bus;
	u32 slot;
	struct mhi_client_config *client_config;
};

struct __packed bhi_vec_entry {
	u64 phys_addr;
	u64 size;
};

/**
 * struct mhi_device - IO resources for MHI
 * @dev: device node points to of_node
 * @pdev: pci device node
 * @resource: bar memory space and IRQ resources
 * @pm_runtime_get: fp for bus masters rpm pm_runtime_get
 * @pm_runtime_noidle: fp for bus masters rpm pm_runtime_noidle
 * @mhi_dev_ctxt: private data for host
 */
struct mhi_device {
	struct device *dev;
	struct pci_dev *pci_dev;
	struct resource resources[2];
	int (*pm_runtime_get)(struct pci_dev *pci_dev);
	void (*pm_runtime_noidle)(struct pci_dev *pci_dev);
	struct mhi_device_ctxt *mhi_dev_ctxt;
};

enum mhi_dev_ctrl {
	MHI_DEV_CTRL_INIT,
	MHI_DEV_CTRL_DE_INIT,
	MHI_DEV_CTRL_SUSPEND,
	MHI_DEV_CTRL_RESUME,
	MHI_DEV_CTRL_POWER_OFF,
	MHI_DEV_CTRL_POWER_ON,
	MHI_DEV_CTRL_RAM_DUMP,
	MHI_DEV_CTRL_NOTIFY_LINK_ERROR,
};

/**
 * mhi_is_device_ready - Check if MHI is ready to register clients
 *
 * @dev: device node that points to DT node
 * @node_name: device tree node that links MHI node
 *
 * @Return true if ready
 */
bool mhi_is_device_ready(const struct device * const dev,
			 const char *node_name);

/**
 * mhi_resgister_device - register hardware resources with MHI
 *
 * @mhi_device: resources to be used
 * @node_name: DT node name
 * @userdata: cb data for client
 * @Return 0 on success
 */
int mhi_register_device(struct mhi_device *mhi_device,
			const char *node_name,
			unsigned long user_data);

/**
 * mhi_pm_control_device - power management control api
 * @mhi_device: registered device structure
 * @ctrl: specific command
 * @Return 0 on success
 */
int mhi_pm_control_device(struct mhi_device *mhi_device,
			  enum mhi_dev_ctrl ctrl);

/**
 * mhi_deregister_channel - de-register callbacks from MHI
 *
 * @client_handle: Handle populated by MHI, opaque to client
 *
 * @Return errno
 */
int mhi_deregister_channel(struct mhi_client_handle *client_handle);

/**
 * mhi_register_channel - Client must call this function to obtain a handle for
 *			  any MHI operations
 *
 *  @client_handle:  Handle populated by MHI, opaque to client
 *  @client_info:    Channel\device information provided by client to
 *                   which the handle maps to.
 *
 * @Return errno
 */
int mhi_register_channel(struct mhi_client_handle **client_handle,
			 struct mhi_client_info_t *client_info);

/**
 * mhi_open_channel - Client must call this function to open a channel
 *
 * @client_handle:  Handle populated by MHI, opaque to client
 *
 *  Not thread safe, caller must ensure concurrency protection.
 *
 * @Return errno
 */
int mhi_open_channel(struct mhi_client_handle *client_handle);

/**
 * mhi_queue_xfer - Client called function to add a buffer to MHI channel
 *
 *  @client_handle  Pointer to client handle previously obtained from
 *                  mhi_open_channel
 *  @buf            Pointer to client buffer
 *  @buf_len        Length of the client buffer
 *  @chain          Specify whether to set the chain bit on this buffer
 *  @eob            Specify whether this buffer should trigger EOB interrupt
 *
 *  NOTE:
 *  Not thread safe, caller must ensure concurrency protection.
 *  User buffer must be physically contiguous.
 *
 * @Return errno
 */
int mhi_queue_xfer(struct mhi_client_handle *client_handle,
		void *buf, size_t buf_len, enum MHI_FLAGS mhi_flags);

/**
 * mhi_close_channel - Client can request channel to be closed and handle freed
 *
 *  @client_handle  Pointer to client handle previously obtained from
 *                  mhi_open_channel
 *  Not thread safe, caller must ensure concurrency protection.
 *
 * @client_handle  Pointer to handle to be released
 */
void mhi_close_channel(struct mhi_client_handle *client_handle);

/**
 * mhi_get_free_desc - Get the number of free descriptors on channel.
 *  client_handle  Pointer to client handle previously obtained from
 *                      mhi_open_channel.
 *
 * This API returns a snapshot of available descriptors on the given
 * channel
 *
 * @Return  non negative on success
 */
int mhi_get_free_desc(struct mhi_client_handle *client_handle);

/*
 * mhi_poll_inbound - Poll a buffer from MHI channel
 * @client_handle  Pointer to client handle previously obtained from
 *                      mhi_open_channel.
 * @result         Result structure to be populated with buffer info
 *			if available;
 *
 * Client may asynchronously poll on an inbound channel for descriptors
 * which have been populated. This API is used by client to receive data
 * from device after a callback notification has been received.
 *
 *  Not thread safe, caller must ensure concurrency protection.
 *
 * @Return  non negative on success
 */
int mhi_poll_inbound(struct mhi_client_handle *client_handle,
			     struct mhi_result *result);

/**
 * mhi_get_max_desc - Get the maximum number of descriptors
 *			supported on the channel.
 * @client_handle  Pointer to client handle previously obtained from
 *                      mhi_open_channel.
 * @Return  non negative on success
 */
int mhi_get_max_desc(struct mhi_client_handle *client_handle);

/* RmNET Reserved APIs, This APIs are reserved for use by the linux network
* stack only. Use by other clients will introduce system wide issues
*/
int mhi_set_lpm(struct mhi_client_handle *client_handle, bool enable_lpm);
int mhi_get_epid(struct mhi_client_handle *mhi_handle);
struct mhi_result *mhi_poll(struct mhi_client_handle *client_handle);
void mhi_mask_irq(struct mhi_client_handle *client_handle);
void mhi_unmask_irq(struct mhi_client_handle *client_handle);
#endif
