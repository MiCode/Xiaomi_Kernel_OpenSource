/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2011-2017, 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _USB_BAM_H_
#define _USB_BAM_H_

#include <linux/msm-sps.h>
#include <linux/ipa.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/types.h>
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
 *       This call back gets back the priv cookie.
 *       for Bam2Bam mode, this callback is in the tethering bridge.
 * @ activity_notify: Callback to be notified on and data being pushed into the
 *	       USB consumer pipe.
 * @ inactivity_notify: Callback to be notified on inactivity of all the current
 *		 open pipes between the USB bam and its peer.
 * @ skip_ep_cfg: boolean field that determines if Apps-processor
 *		  should or should not confiugre this end-point.
 *		  (Please see struct teth_bridge_init_params)
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
 * @iova - IPA address of USB peer BAM (i.e. QDSS BAM)
 *
 * @return 0 on success, negative value on error
 *
 */
int usb_bam_connect(enum usb_ctrl bam_type, int idx, u32 *bam_pipe_idx,
						unsigned long iova);

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
	enum usb_bam_pipe_dir dir, u32 num);

/*
 * return the usb controller bam type used for the supplied connection index
 *
 * @core_name - Core name (ssusb/hsusb/hsic).
 *
 * @return usb control bam type
 */
enum usb_ctrl usb_bam_get_bam_type(const char *core_name);

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
int get_qdss_bam_info(enum usb_ctrl cur_bam, u8 idx,
			phys_addr_t *p_addr, u32 *bam_size);
bool msm_usb_bam_enable(enum usb_ctrl ctrl, bool bam_enable);
void msm_bam_set_hsic_host_dev(struct device *dev);
bool msm_bam_hsic_lpm_ok(void);
void msm_bam_hsic_host_notify_on_resume(void);
void msm_bam_wait_for_hsic_host_prod_granted(void);
bool msm_bam_hsic_host_pipe_empty(void);
#else
static inline int usb_bam_connect(enum usb_ctrl bam, u8 idx, u32 *bam_pipe_idx,
							unsigned long iova)
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
		enum peer_bam client, enum usb_bam_pipe_dir dir, u32 num)
{
	return -ENODEV;
}

static inline enum usb_ctrl usb_bam_get_bam_type(const char *core_nam)
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

static inline int usb_bam_alloc_fifos(enum usb_ctrl cur_bam, u8 idx)
{
	return false;
}

static inline int usb_bam_free_fifos(enum usb_ctrl cur_bam, u8 idx)
{
	return false;
}

static inline int get_qdss_bam_info(enum usb_ctrl cur_bam, u8 idx,
				phys_addr_t *p_addr, u32 *bam_size)
{
	return false;
}

static inline bool msm_usb_bam_enable(enum usb_ctrl ctrl, bool bam_enable)
{
	return false;
}

static inline void msm_bam_set_hsic_host_dev(struct device *dev) {}

static inline bool msm_bam_hsic_lpm_ok(void)
{
	return false;
}

static inline void msm_bam_hsic_host_notify_on_resume(void) {}

static inline void msm_bam_wait_for_hsic_host_prod_granted(void) {}

static inline bool msm_bam_hsic_host_pipe_empty(void)
{
	return false;
}

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
#ifdef CONFIG_USB_CI13XXX_MSM
void msm_hw_bam_disable(bool bam_disable);
#else
static inline void msm_hw_bam_disable(bool bam_disable)
{}
#endif
#endif				/* _USB_BAM_H_ */
