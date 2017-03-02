/* Copyright (c) 2011-2017, The Linux Foundation. All rights reserved.
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

#ifndef _USB_BAM_H_
#define _USB_BAM_H_
#include <linux/msm-sps.h>
#include <linux/ipa.h>
#include <linux/usb/ch9.h>

#define MAX_BAMS	NUM_CTRL	/* Bam per USB controllers */

/* Supported USB controllers*/
enum usb_ctrl {
	DWC3_CTRL = 0,  /* DWC3 controller */
	CI_CTRL,        /* ChipIdea controller */
	HSIC_CTRL,      /* HSIC controller */
	NUM_CTRL,
};

enum usb_bam_mode {
	USB_BAM_DEVICE = 0,
	USB_BAM_HOST,
};

enum peer_bam {
	QDSS_P_BAM = 0,
	IPA_P_BAM,
	MAX_PEER_BAMS,
};

enum usb_bam_pipe_dir {
	USB_TO_PEER_PERIPHERAL,
	PEER_PERIPHERAL_TO_USB,
};

enum usb_pipe_mem_type {
	SPS_PIPE_MEM = 0,	/* Default, SPS dedicated pipe memory */
	SYSTEM_MEM,		/* System RAM, requires allocation */
	OCI_MEM,		/* Shared memory among peripherals */
};

enum usb_bam_event_type {
	USB_BAM_EVENT_WAKEUP_PIPE = 0,	/* Wake a pipe */
	USB_BAM_EVENT_WAKEUP,		/* Wake a bam (first pipe waked) */
	USB_BAM_EVENT_INACTIVITY,	/* Inactivity on all pipes */
};

enum usb_bam_pipe_type {
	USB_BAM_PIPE_BAM2BAM = 0,	/* Connection is BAM2BAM (default) */
	USB_BAM_PIPE_SYS2BAM,		/* Connection is SYS2BAM or BAM2SYS
					 * depending on usb_bam_pipe_dir
					 */
	USB_BAM_MAX_PIPE_TYPES,
};

/*
 * struct usb_bam_connect_ipa_params: Connect Bam pipe to IPA peer information.
 * @ src_idx: Source pipe index in usb bam pipes lists.
 * @ dst_idx: Destination pipe index in usb bam pipes lists.
 * @ src_pipe: The source pipe index in the sps level.
 * @ dst_pipe: The destination pipe index in the sps level.
 * @ keep_ipa_awake: When true, IPA will not be clock gated.
 * @ ipa_cons_ep_idx: The pipe index on the IPA peer bam side, consumer.
 * @ ipa_prod_ep_idx: The pipe index on the IPA peer bam side, producer.
 * @ prod_clnt_hdl: Producer client handle returned by IPA driver
 * @ cons_clnt_hdl: Consumer client handle returned by IPA driver
 * @ src_client: Source IPA client type.
 * @ dst_client: Destination IPA client type.
 * @ ipa_ep_cfg: Configuration of IPA end-point (see struct ipa_ep_cfg)
 * @priv: Callback cookie to the notify event.
 * @notify: Callback on data path event by IPA (see enum ipa_dp_evt_type)
 *	 This call back gets back the priv cookie.
 *	 for Bam2Bam mode, this callback is in the tethering bridge.
 * @ activity_notify: Callback to be notified on and data being pushed into the
 *		 USB consumer pipe.
 * @ inactivity_notify: Callback to be notified on inactivity of all the current
 *		   open pipes between the USB bam and its peer.
 * @ skip_ep_cfg: boolean field that determines if Apps-processor
 *	            should or should not confiugre this end-point.
 *	            (Please see struct teth_bridge_init_params)
 * @ reset_pipe_after_lpm: bool to indicate if IPA should reset pipe after LPM.
 * @ usb_connection_speed: The actual speed the USB core currently works at.
 */
struct usb_bam_connect_ipa_params {
	u8 src_idx;
	u8 dst_idx;
	u32 *src_pipe;
	u32 *dst_pipe;
	bool keep_ipa_awake;
	enum usb_bam_pipe_dir dir;
	/* Parameters for Port Mapper */
	u32 ipa_cons_ep_idx;
	u32 ipa_prod_ep_idx;
	/* client handle assigned by IPA to client */
	u32 prod_clnt_hdl;
	u32 cons_clnt_hdl;
	/* params assigned by the CD */
	enum ipa_client_type src_client;
	enum ipa_client_type dst_client;
	struct ipa_ep_cfg ipa_ep_cfg;
	void *priv;
	void (*notify)(void *priv, enum ipa_dp_evt_type evt,
			unsigned long data);
	int (*activity_notify)(void *priv);
	int (*inactivity_notify)(void *priv);
	bool skip_ep_cfg;
	bool reset_pipe_after_lpm;
	enum usb_device_speed usb_connection_speed;
};

/*
 * struct usb_bam_event_info: suspend/resume event information.
 * @type: usb bam event type.
 * @event: holds event data.
 * @callback: suspend/resume callback.
 * @param: port num (for suspend) or NULL (for resume).
 * @event_w: holds work queue parameters.
 */
struct usb_bam_event_info {
	enum usb_bam_event_type type;
	struct sps_register_event event;
	int (*callback)(void *);
	void *param;
	struct work_struct event_w;
};

/*
 * struct usb_bam_pipe_connect: pipe connection information
 * between USB/HSIC BAM and another BAM. USB/HSIC BAM can be
 * either src BAM or dst BAM
 * @name: pipe description.
 * @mem_type: type of memory used for BAM FIFOs
 * @src_phy_addr: src bam physical address.
 * @src_pipe_index: src bam pipe index.
 * @dst_phy_addr: dst bam physical address.
 * @dst_pipe_index: dst bam pipe index.
 * @data_fifo_base_offset: data fifo offset.
 * @data_fifo_size: data fifo size.
 * @desc_fifo_base_offset: descriptor fifo offset.
 * @desc_fifo_size: descriptor fifo size.
 * @data_mem_buf: data fifo buffer.
 * @desc_mem_buf: descriptor fifo buffer.
 * @event: event for wakeup.
 * @enabled: true if pipe is enabled.
 * @suspended: true if pipe is suspended.
 * @cons_stopped: true is pipe has consumer requests stopped.
 * @prod_stopped: true if pipe has producer requests stopped.
 * @ipa_clnt_hdl : pipe handle to ipa api.
 * @priv: private data to return upon activity_notify
 *	or inactivity_notify callbacks.
 * @activity_notify: callback to invoke on activity on one of the in pipes.
 * @inactivity_notify: callback to invoke on inactivity on all pipes.
 * @start: callback to invoke to enqueue transfers on a pipe.
 * @stop: callback to invoke on dequeue transfers on a pipe.
 * @start_stop_param: param for the start/stop callbacks.
 */
struct usb_bam_pipe_connect {
	const char *name;
	u32 pipe_num;
	enum usb_pipe_mem_type mem_type;
	enum usb_bam_pipe_dir dir;
	enum usb_ctrl bam_type;
	enum usb_bam_mode bam_mode;
	enum peer_bam peer_bam;
	enum usb_bam_pipe_type pipe_type;
	u32 src_phy_addr;
	u32 src_pipe_index;
	u32 dst_phy_addr;
	u32 dst_pipe_index;
	u32 data_fifo_base_offset;
	u32 data_fifo_size;
	u32 desc_fifo_base_offset;
	u32 desc_fifo_size;
	struct sps_mem_buffer data_mem_buf;
	struct sps_mem_buffer desc_mem_buf;
	struct usb_bam_event_info event;
	bool enabled;
	bool suspended;
	bool cons_stopped;
	bool prod_stopped;
	int ipa_clnt_hdl;
	void *priv;
	int (*activity_notify)(void *priv);
	int (*inactivity_notify)(void *priv);
	void (*start)(void *, enum usb_bam_pipe_dir);
	void (*stop)(void *, enum usb_bam_pipe_dir);
	void *start_stop_param;
	bool reset_pipe_after_lpm;
};

/**
 * struct msm_usb_bam_platform_data: pipe connection information
 * between USB/HSIC BAM and another BAM. USB/HSIC BAM can be
 * either src BAM or dst BAM
 * @usb_bam_num_pipes: max number of pipes to use.
 * @active_conn_num: number of active pipe connections.
 * @usb_bam_fifo_baseaddr: base address for bam pipe's data and descriptor
 *                         fifos. This can be on chip memory (ocimem) or usb
 *                         private memory.
 * @ignore_core_reset_ack: BAM can ignore ACK from USB core during PIPE RESET
 * @reset_on_connect: BAM must be reset before its first pipe connect
 * @reset_on_disconnect: BAM must be reset after its last pipe disconnect
 * @disable_clk_gating: Disable clock gating
 * @override_threshold: Override the default threshold value for Read/Write
 *                         event generation by the BAM towards another BAM.
 * @max_mbps_highspeed: Maximum Mbits per seconds that the USB core
 *		can work at in bam2bam mode when connected to HS host.
 * @max_mbps_superspeed: Maximum Mbits per seconds that the USB core
 *		can work at in bam2bam mode when connected to SS host.
 * @enable_hsusb_bam_on_boot: Enable HSUSB BAM (non-NDP) on bootup itself
 */
struct msm_usb_bam_platform_data {
	u8 max_connections;
	int usb_bam_num_pipes;
	phys_addr_t usb_bam_fifo_baseaddr;
	bool ignore_core_reset_ack;
	bool reset_on_connect;
	bool reset_on_disconnect;
	bool disable_clk_gating;
	u32 override_threshold;
	u32 max_mbps_highspeed;
	u32 max_mbps_superspeed;
	bool enable_hsusb_bam_on_boot;
	enum usb_ctrl bam_type;
};

#if  IS_ENABLED(CONFIG_USB_BAM)
/**
 * Connect USB-to-Peripheral SPS connection.
 *
 * This function returns the allocated pipe number.
 *
 * @bam_type - USB BAM type - dwc3/CI/hsic
 *
 * @idx - Connection index.
 *
 * @bam_pipe_idx - allocated pipe index.
 *
 * @return 0 on success, negative value on error
 *
 */
int usb_bam_connect(enum usb_ctrl bam_type, int idx, u32 *bam_pipe_idx);

/**
 * Connect USB-to-IPA SPS connection.
 *
 * This function returns the allocated pipes number and clnt
 * handles. Assumes that the user first connects producer pipes
 * and only after that consumer pipes, since that's the correct
 * sequence for the handshake with the IPA.
 *
 * @bam_type - USB BAM type - dwc3/CI/hsic
 *
 * @ipa_params - in/out parameters
 *
 * @return 0 on success, negative value on error
 */
int usb_bam_connect_ipa(enum usb_ctrl bam_type,
			struct usb_bam_connect_ipa_params *ipa_params);

/**
 * Disconnect USB-to-IPA SPS connection.
 *
 * @bam_type - USB BAM type - dwc3/CI/hsic
 *
 * @ipa_params - in/out parameters
 *
 * @return 0 on success, negative value on error
 */
int usb_bam_disconnect_ipa(enum usb_ctrl bam_type,
		struct usb_bam_connect_ipa_params *ipa_params);

/**
 * Register a wakeup callback from peer BAM.
 *
 * @bam_type - USB BAM type - dwc3/CI/hsic
 *
 * @idx - Connection index.
 *
 * @callback - the callback function
 *
 * @return 0 on success, negative value on error
 */
int usb_bam_register_wake_cb(enum usb_ctrl bam_type, u8 idx,
	int (*callback)(void *), void *param);

/**
 * Register callbacks for start/stop of transfers.
 *
 * @bam_type - USB BAM type - dwc3/CI/hsic
 *
 * @idx - Connection index
 *
 * @start - the callback function that will be called in USB
 *				driver to start transfers
 * @stop - the callback function that will be called in USB
 *				driver to stop transfers
 *
 * @param - context that the caller can supply
 *
 * @return 0 on success, negative value on error
 */
int usb_bam_register_start_stop_cbs(enum usb_ctrl bam_type,
	u8 idx,
	void (*start)(void *, enum usb_bam_pipe_dir),
	void (*stop)(void *, enum usb_bam_pipe_dir),
	void *param);

/**
 * Start usb suspend sequence
 *
 * @ipa_params -  in/out parameters
 *
 * @bam_type - USB BAM type - dwc3/CI/hsic
 */
void usb_bam_suspend(enum usb_ctrl bam_type,
		     struct usb_bam_connect_ipa_params *ipa_params);

/**
 * Start usb resume sequence
 *
 * @bam_type - USB BAM type - dwc3/CI/hsic
 *
 * @ipa_params -  in/out parameters
 */
void usb_bam_resume(enum usb_ctrl bam_type,
		     struct usb_bam_connect_ipa_params *ipa_params);
/**
 * Disconnect USB-to-Periperal SPS connection.
 *
 * @bam_type - USB BAM type - dwc3/CI/hsic
 *
 * @idx - Connection index.
 *
 * @return 0 on success, negative value on error
 */
int usb_bam_disconnect_pipe(enum usb_ctrl bam_type, u8 idx);

/**
 * Returns usb bam connection parameters.
 *
 * @bam_type - USB BAM type - dwc3/CI/hsic
 *
 * @idx - Connection index.
 *
 * @usb_bam_pipe_idx - Usb bam pipe index.
 *
 * @desc_fifo - Descriptor fifo parameters.
 *
 * @data_fifo - Data fifo parameters.
 *
 * @return pipe index on success, negative value on error.
 */
int get_bam2bam_connection_info(enum usb_ctrl bam_type, u8 idx,
	u32 *usb_bam_pipe_idx, struct sps_mem_buffer *desc_fifo,
	struct sps_mem_buffer *data_fifo, enum usb_pipe_mem_type *mem_type);

/**
 * Returns usb bam connection parameters for qdss pipe.
 * @usb_bam_handle - Usb bam handle.
 * @usb_bam_pipe_idx - Usb bam pipe index.
 * @peer_pipe_idx - Peer pipe index.
 * @desc_fifo - Descriptor fifo parameters.
 * @data_fifo - Data fifo parameters.
 * @return pipe index on success, negative value on error.
 */
int get_qdss_bam_connection_info(
	unsigned long *usb_bam_handle, u32 *usb_bam_pipe_idx,
	u32 *peer_pipe_idx, struct sps_mem_buffer *desc_fifo,
	struct sps_mem_buffer *data_fifo, enum usb_pipe_mem_type *mem_type);

/*
 * Indicates if the client of the USB BAM is ready to start
 * sending/receiving transfers.
 *
 * @bam_type - USB BAM type - dwc3/CI/hsic
 *
 * @client - Usb pipe peer (a2, ipa, qdss...)
 *
 * @dir - In (from peer to usb) or out (from usb to peer)
 *
 * @num - Pipe number.
 *
 * @return 0 on success, negative value on error
 */
int usb_bam_get_connection_idx(enum usb_ctrl bam_type, enum peer_bam client,
	enum usb_bam_pipe_dir dir, enum usb_bam_mode bam_mode, u32 num);

/*
 * return the usb controller bam type used for the supplied connection index
 *
 * @core_name - Core name (ssusb/hsusb/hsic).
 *
 * @return usb control bam type
 */
int usb_bam_get_bam_type(const char *core_name);

/*
 * Indicates the type of connection the USB side of the connection is.
 *
 * @bam_type - USB BAM type - dwc3/CI/hsic
 *
 * @idx - Pipe number.
 *
 * @type - Type of connection
 *
 * @return 0 on success, negative value on error
 */
int usb_bam_get_pipe_type(enum usb_ctrl bam_type,
			  u8 idx, enum usb_bam_pipe_type *type);

/*
 * Indicates whether USB producer is granted to IPA resource manager.
 *
 * @return true when producer granted, false when prodcuer is released.
 */
bool usb_bam_get_prod_granted(enum usb_ctrl bam_type, u8 idx);

/* Allocates memory for data fifo and descriptor fifos. */
int usb_bam_alloc_fifos(enum usb_ctrl cur_bam, u8 idx);

/* Frees memory for data fifo and descriptor fifos. */
int usb_bam_free_fifos(enum usb_ctrl cur_bam, u8 idx);

bool msm_bam_hsic_lpm_ok(void);
bool msm_bam_hsic_host_pipe_empty(void);
bool msm_usb_bam_enable(enum usb_ctrl ctrl, bool bam_enable);
#else
static inline int usb_bam_connect(enum usb_ctrl bam, u8 idx, u32 *bam_pipe_idx)
{
	return -ENODEV;
}

static inline int usb_bam_connect_ipa(enum usb_ctrl bam_type,
			struct usb_bam_connect_ipa_params *ipa_params)
{
	return -ENODEV;
}

static inline int usb_bam_disconnect_ipa(enum usb_ctrl bam_type,
			struct usb_bam_connect_ipa_params *ipa_params)
{
	return -ENODEV;
}

static inline void usb_bam_wait_for_cons_granted(
			struct usb_bam_connect_ipa_params *ipa_params)
{ }

static inline int usb_bam_register_wake_cb(enum usb_ctrl bam_type, u8 idx,
	int (*callback)(void *), void *param)
{
	return -ENODEV;
}

static inline int usb_bam_register_start_stop_cbs(enum usb_ctrl bam, u8 idx,
	void (*start)(void *, enum usb_bam_pipe_dir),
	void (*stop)(void *, enum usb_bam_pipe_dir),
	void *param)
{
	return -ENODEV;
}

static inline void usb_bam_suspend(enum usb_ctrl bam_type,
	struct usb_bam_connect_ipa_params *ipa_params){}

static inline void usb_bam_resume(enum usb_ctrl bam_type,
	struct usb_bam_connect_ipa_params *ipa_params) {}

static inline int usb_bam_disconnect_pipe(enum usb_ctrl bam_type, u8 idx)
{
	return -ENODEV;
}

static inline int get_bam2bam_connection_info(enum usb_ctrl bam_type, u8 idx,
	u32 *usb_bam_pipe_idx, struct sps_mem_buffer *desc_fifo,
	struct sps_mem_buffer *data_fifo, enum usb_pipe_mem_type *mem_type)
{
	return -ENODEV;
}

static inline int get_qdss_bam_connection_info(
	unsigned long *usb_bam_handle, u32 *usb_bam_pipe_idx,
	u32 *peer_pipe_idx, struct sps_mem_buffer *desc_fifo,
	struct sps_mem_buffer *data_fifo, enum usb_pipe_mem_type *mem_type)
{
	return -ENODEV;
}

static inline int usb_bam_get_connection_idx(enum usb_ctrl bam_type,
		enum peer_bam client, enum usb_bam_pipe_dir dir,
		enum usb_bam_mode bam_mode, u32 num)
{
	return -ENODEV;
}

static inline int usb_bam_get_bam_type(const char *core_nam)
{
	return -ENODEV;
}

static inline int usb_bam_get_pipe_type(enum usb_ctrl bam_type, u8 idx,
					enum usb_bam_pipe_type *type)
{
	return -ENODEV;
}

static inline bool usb_bam_get_prod_granted(enum usb_ctrl bam_type, u8 idx)
{
	return false;
}

int usb_bam_alloc_fifos(enum usb_ctrl cur_bam, u8 idx)
{
	return false;
}

int usb_bam_free_fifos(enum usb_ctrl cur_bam, u8 idx)
{
	return false;
}

static inline bool msm_bam_hsic_lpm_ok(void) { return true; }
static inline bool msm_bam_hsic_host_pipe_empty(void) { return true; }
static inline bool msm_usb_bam_enable(enum usb_ctrl ctrl, bool bam_enable)
{ return true; }

#endif

#ifdef CONFIG_USB_CI13XXX_MSM
void msm_hw_bam_disable(bool bam_disable);
void msm_usb_irq_disable(bool disable);
#else
static inline void msm_hw_bam_disable(bool bam_disable)
{ }

static inline void msm_usb_irq_disable(bool disable)
{ }
#endif

/* CONFIG_PM */
#ifdef CONFIG_PM
static inline int get_pm_runtime_counter(struct device *dev)
{
	return atomic_read(&dev->power.usage_count);
}
#else
/* !CONFIG_PM */
static inline int get_pm_runtime_counter(struct device *dev)
{ return -EOPNOTSUPP; }
#endif
#endif				/* _USB_BAM_H_ */
