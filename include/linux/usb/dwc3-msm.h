/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __LINUX_USB_DWC3_MSM_H
#define __LINUX_USB_DWC3_MSM_H

#include <linux/scatterlist.h>
#include <linux/usb/gadget.h>
#include <linux/soc/qcom/llcc-tcm.h>

/* used for struct usb_phy flags */
#define PHY_HOST_MODE			BIT(0)
#define DEVICE_IN_SS_MODE		BIT(1)
#define PHY_LANE_A			BIT(2)
#define PHY_LANE_B			BIT(3)
#define PHY_HSFS_MODE			BIT(4)
#define PHY_LS_MODE			BIT(5)
#define EUD_SPOOF_DISCONNECT		BIT(6)
#define EUD_SPOOF_CONNECT		BIT(7)
#define PHY_SUS_OVERRIDE		BIT(8)
#define PHY_USB_DP_CONCURRENT_MODE	BIT(9)

/*
 * The following are bit fields describing the USB BAM options.
 * These bit fields are set by function drivers that wish to queue
 * usb_requests with sps/bam parameters.
 */
#define MSM_TX_PIPE_ID_OFS		(16)
#define MSM_SPS_MODE			BIT(5)
#define MSM_IS_FINITE_TRANSFER		BIT(6)
#define MSM_PRODUCER			BIT(7)
#define MSM_DISABLE_WB			BIT(8)
#define MSM_ETD_IOC			BIT(9)
#define MSM_INTERNAL_MEM		BIT(10)
#define MSM_VENDOR_ID			BIT(16)

/* Operations codes for GSI enabled EPs */
enum gsi_ep_op {
	GSI_EP_OP_CONFIG = 0,
	GSI_EP_OP_STARTXFER,
	GSI_EP_OP_STORE_DBL_INFO,
	GSI_EP_OP_ENABLE_GSI,
	GSI_EP_OP_UPDATEXFER,
	GSI_EP_OP_RING_DB,
	GSI_EP_OP_ENDXFER,
	GSI_EP_OP_GET_CH_INFO,
	GSI_EP_OP_GET_XFER_IDX,
	GSI_EP_OP_PREPARE_TRBS,
	GSI_EP_OP_FREE_TRBS,
	GSI_EP_OP_SET_CLR_BLOCK_DBL,
	GSI_EP_OP_CHECK_FOR_SUSPEND,
	GSI_EP_OP_DISABLE,
};

/*
 * @buf_base_addr: Base pointer to buffer allocated for each GSI enabled EP.
 *	TRBs point to buffers that are split from this pool. The size of the
 *	buffer is num_bufs times buf_len. num_bufs and buf_len are determined
	based on desired performance and aggregation size.
 * @dma: DMA address corresponding to buf_base_addr.
 * @num_bufs: Number of buffers associated with the GSI enabled EP. This
 *	corresponds to the number of non-zlp TRBs allocated for the EP.
 *	The value is determined based on desired performance for the EP.
 * @buf_len: Size of each individual buffer is determined based on aggregation
 *	negotiated as per the protocol. In case of no aggregation supported by
 *	the protocol, we use default values.
 * @db_reg_phs_addr_lsb: IPA channel doorbell register's physical address LSB
 * @mapped_db_reg_phs_addr_lsb: doorbell LSB IOVA address mapped with IOMMU
 * @db_reg_phs_addr_msb: IPA channel doorbell register's physical address MSB
 * @ep_intr_num: Interrupter number for EP.
 * @sgt_trb_xfer_ring: USB TRB ring related sgtable entries
 * @sgt_data_buff: Data buffer related sgtable entries
 * @dev: pointer to the DMA-capable dwc device
 */
struct usb_gsi_request {
	void *buf_base_addr;
	dma_addr_t dma;
	size_t num_bufs;
	size_t buf_len;
	u32 db_reg_phs_addr_lsb;
	dma_addr_t mapped_db_reg_phs_addr_lsb;
	u32 db_reg_phs_addr_msb;
	u8 ep_intr_num;
	struct sg_table sgt_trb_xfer_ring;
	struct sg_table sgt_data_buff;
	struct device *dev;
	bool use_tcm_mem;
	struct llcc_tcm_data *tcm_mem;
};

/*
 * @last_trb_addr: Address (LSB - based on alignment restrictions) of
 *	last TRB in queue. Used to identify rollover case.
 * @const_buffer_size: TRB buffer size in KB (similar to IPA aggregation
 *	configuration). Must be aligned to Max USB Packet Size.
 *	Should be 1 <= const_buffer_size <= 31.
 * @depcmd_low_addr: Used by GSI hardware to write "Update Transfer" cmd
 * @depcmd_hi_addr: Used to write "Update Transfer" command.
 * @gevntcount_low_addr: GEVNCOUNT low address for GSI hardware to read and
 *	clear processed events.
 * @gevntcount_hi_addr:	GEVNCOUNT high address.
 * @xfer_ring_len: length of transfer ring in bytes (must be integral
 *	multiple of TRB size - 16B for xDCI).
 * @xfer_ring_base_addr: physical base address of transfer ring. Address must
 *	be aligned to xfer_ring_len rounded to power of two.
 * @ch_req: Used to pass request specific info for certain operations on GSI EP
 */
struct gsi_channel_info {
	u16 last_trb_addr;
	u8 const_buffer_size;
	u32 depcmd_low_addr;
	u8 depcmd_hi_addr;
	u32 gevntcount_low_addr;
	u8 gevntcount_hi_addr;
	u16 xfer_ring_len;
	u64 xfer_ring_base_addr;
	struct usb_gsi_request *ch_req;
};

#if IS_ENABLED(CONFIG_USB_DWC3_MSM)
struct usb_ep *usb_ep_autoconfig_by_name(struct usb_gadget *gadget,
		struct usb_endpoint_descriptor *desc, const char *ep_name);
int usb_gsi_ep_op(struct usb_ep *ep, void *op_data, enum gsi_ep_op op);
int msm_ep_config(struct usb_ep *ep, struct usb_request *request, u32 bam_opts);
int msm_ep_unconfig(struct usb_ep *ep);
void msm_ep_set_endless(struct usb_ep *ep, bool set_clear);
void dwc3_tx_fifo_resize_request(struct usb_ep *ep, bool qdss_enable);
int msm_data_fifo_config(struct usb_ep *ep, unsigned long addr, u32 size,
	u8 dst_pipe_idx);
bool msm_dwc3_reset_ep_after_lpm(struct usb_gadget *gadget);
int msm_dwc3_reset_dbm_ep(struct usb_ep *ep);
int dwc3_msm_release_ss_lane(struct device *dev, bool usb_dp_concurrent_mode);
bool usb_get_remote_wakeup_status(struct usb_gadget *gadget);
#else
static inline struct usb_ep *usb_ep_autoconfig_by_name(
		struct usb_gadget *gadget, struct usb_endpoint_descriptor *desc,
		const char *ep_name)
{ return NULL; }
static inline int usb_gsi_ep_op(struct usb_ep *ep, void *op_data,
		enum gsi_ep_op op)
{ return 0; }
static inline int msm_data_fifo_config(struct usb_ep *ep, unsigned long addr,
	u32 size, u8 dst_pipe_idx)
{ return -ENODEV; }
static inline int msm_ep_config(struct usb_ep *ep, struct usb_request *request,
		u32 bam_opts)
{ return -ENODEV; }
static inline int msm_ep_unconfig(struct usb_ep *ep)
{ return -ENODEV; }
static inline void msm_ep_set_endless(struct usb_ep *ep, bool set_clear)
{ }
static inline void dwc3_tx_fifo_resize_request(struct usb_ep *ep,
	bool qdss_enable)
{ }
static inline bool msm_dwc3_reset_ep_after_lpm(struct usb_gadget *gadget)
{ return false; }
static inline int msm_dwc3_reset_dbm_ep(struct usb_ep *ep)
{ return -ENODEV; }
static inline int dwc3_msm_release_ss_lane(struct device *dev, bool usb_dp_concurrent_mode)
{ return -ENODEV; }
static bool __maybe_unused usb_get_remote_wakeup_status(struct usb_gadget *gadget)
{ return false; }
#endif

#if IS_ENABLED(CONFIG_USB_F_GSI)
void rmnet_gsi_update_in_buffer_mem_type(struct usb_function *f, bool use_tcm);
#else
static inline __maybe_unused void rmnet_gsi_update_in_buffer_mem_type(
		struct usb_function *f, bool use_tcm)
{ }
#endif

#endif /* __LINUX_USB_DWC3_MSM_H */
