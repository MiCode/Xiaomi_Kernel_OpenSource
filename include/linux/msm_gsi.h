/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
#ifndef MSM_GSI_H
#define MSM_GSI_H
#include <linux/types.h>

enum gsi_status {
	GSI_STATUS_SUCCESS = 0,
	GSI_STATUS_ERROR = 1,
	GSI_STATUS_RING_INSUFFICIENT_SPACE = 2,
	GSI_STATUS_RING_EMPTY = 3,
	GSI_STATUS_RES_ALLOC_FAILURE = 4,
	GSI_STATUS_BAD_STATE = 5,
	GSI_STATUS_INVALID_PARAMS = 6,
	GSI_STATUS_UNSUPPORTED_OP = 7,
	GSI_STATUS_NODEV = 8,
	GSI_STATUS_POLL_EMPTY = 9,
	GSI_STATUS_EVT_RING_INCOMPATIBLE = 10,
	GSI_STATUS_TIMED_OUT = 11,
	GSI_STATUS_AGAIN = 12,
};

enum gsi_per_evt {
	GSI_PER_EVT_GLOB_ERROR,
	GSI_PER_EVT_GLOB_GP1,
	GSI_PER_EVT_GLOB_GP2,
	GSI_PER_EVT_GLOB_GP3,
	GSI_PER_EVT_GENERAL_BREAK_POINT,
	GSI_PER_EVT_GENERAL_BUS_ERROR,
	GSI_PER_EVT_GENERAL_CMD_FIFO_OVERFLOW,
	GSI_PER_EVT_GENERAL_MCS_STACK_OVERFLOW,
};

/**
 * gsi_per_notify - Peripheral callback info
 *
 * @user_data: cookie supplied in gsi_register_device
 * @evt_id:    type of notification
 * @err_desc:  error related information
 *
 */
struct gsi_per_notify {
	void *user_data;
	enum gsi_per_evt evt_id;
	union {
		uint16_t err_desc;
	} data;
};

enum gsi_intr_type {
	GSI_INTR_MSI = 0x0,
	GSI_INTR_IRQ = 0x1
};


/**
 * gsi_per_props - Peripheral related properties
 *
 * @ee:         EE where this driver and peripheral driver runs
 * @intr:       control interrupt type
 * @intvec:     write data for MSI write
 * @msi_addr:   MSI address
 * @irq:        IRQ number
 * @phys_addr:  physical address of GSI block
 * @size:       register size of GSI block
 * @notify_cb:  general notification callback
 * @req_clk_cb: callback to request peripheral clock
 *		granted should be set to true if request is completed
 *		synchronously, false otherwise (peripheral needs
 *		to call gsi_complete_clk_grant later when request is
 *		completed)
 *		if this callback is not provided, then GSI will assume
 *		peripheral is clocked at all times
 * @rel_clk_cb: callback to release peripheral clock
 * @user_data:  cookie used for notifications
 *
 * All the callbacks are in interrupt context
 *
 */
struct gsi_per_props {
	unsigned int ee;
	enum gsi_intr_type intr;
	uint32_t intvec;
	uint64_t msi_addr;
	unsigned int irq;
	phys_addr_t phys_addr;
	unsigned long size;
	void (*notify_cb)(struct gsi_per_notify *notify);
	void (*req_clk_cb)(void *user_data, bool *granted);
	int (*rel_clk_cb)(void *user_data);
	void *user_data;
};

enum gsi_evt_err {
	GSI_EVT_OUT_OF_BUFFERS_ERR = 0x0,
	GSI_EVT_OUT_OF_RESOURCES_ERR = 0x1,
	GSI_EVT_UNSUPPORTED_INTER_EE_OP_ERR = 0x2,
	GSI_EVT_EVT_RING_EMPTY_ERR = 0x3,
};

/**
 * gsi_evt_err_notify - event ring error callback info
 *
 * @user_data: cookie supplied in gsi_alloc_evt_ring
 * @evt_id:    type of error
 * @err_desc:  more info about the error
 *
 */
struct gsi_evt_err_notify {
	void *user_data;
	enum gsi_evt_err evt_id;
	uint16_t err_desc;
};

enum gsi_evt_chtype {
	GSI_EVT_CHTYPE_MHI_EV = 0x0,
	GSI_EVT_CHTYPE_XHCI_EV = 0x1,
	GSI_EVT_CHTYPE_GPI_EV = 0x2,
	GSI_EVT_CHTYPE_XDCI_EV = 0x3
};

enum gsi_evt_ring_elem_size {
	GSI_EVT_RING_RE_SIZE_4B = 4,
	GSI_EVT_RING_RE_SIZE_16B = 16,
};

/**
 * gsi_evt_ring_props - Event ring related properties
 *
 * @intf:            interface type (of the associated channel)
 * @intr:            interrupt type
 * @re_size:         size of event ring element
 * @ring_len:        length of ring in bytes (must be integral multiple of
 *                   re_size)
 * @ring_base_addr:  physical base address of ring. Address must be aligned to
 *		     ring_len rounded to power of two
 * @ring_base_vaddr: virtual base address of ring (set to NULL when not
 *                   applicable)
 * @int_modt:        cycles base interrupt moderation (32KHz clock)
 * @int_modc:        interrupt moderation packet counter
 * @intvec:          write data for MSI write
 * @msi_addr:        MSI address
 * @rp_update_addr:  physical address to which event read pointer should be
 *                   written on every event generation. must be set to 0 when
 *                   no update is desdired
 * @exclusive:       if true, only one GSI channel can be associated with this
 *                   event ring. if false, the event ring can be shared among
 *                   multiple GSI channels but in that case no polling
 *                   (GSI_CHAN_MODE_POLL) is supported on any of those channels
 * @err_cb:          error notification callback
 * @user_data:       cookie used for error notifications
 * @evchid_valid:    is evchid valid?
 * @evchid:          the event ID that is being specifically requested (this is
 *                   relevant for MHI where doorbell routing requires ERs to be
 *                   physically contiguous)
 */
struct gsi_evt_ring_props {
	enum gsi_evt_chtype intf;
	enum gsi_intr_type intr;
	enum gsi_evt_ring_elem_size re_size;
	uint16_t ring_len;
	uint64_t ring_base_addr;
	void *ring_base_vaddr;
	uint16_t int_modt;
	uint8_t int_modc;
	uint32_t intvec;
	uint64_t msi_addr;
	uint64_t rp_update_addr;
	bool exclusive;
	void (*err_cb)(struct gsi_evt_err_notify *notify);
	void *user_data;
	bool evchid_valid;
	uint8_t evchid;
};

enum gsi_chan_mode {
	GSI_CHAN_MODE_CALLBACK = 0x0,
	GSI_CHAN_MODE_POLL = 0x1,
};

enum gsi_chan_prot {
	GSI_CHAN_PROT_MHI = 0x0,
	GSI_CHAN_PROT_XHCI = 0x1,
	GSI_CHAN_PROT_GPI = 0x2,
	GSI_CHAN_PROT_XDCI = 0x3
};

enum gsi_chan_dir {
	GSI_CHAN_DIR_FROM_GSI = 0x0,
	GSI_CHAN_DIR_TO_GSI = 0x1
};

enum gsi_max_prefetch {
	GSI_ONE_PREFETCH_SEG = 0x0,
	GSI_TWO_PREFETCH_SEG = 0x1
};

enum gsi_chan_evt {
	GSI_CHAN_EVT_INVALID = 0x0,
	GSI_CHAN_EVT_SUCCESS = 0x1,
	GSI_CHAN_EVT_EOT = 0x2,
	GSI_CHAN_EVT_OVERFLOW = 0x3,
	GSI_CHAN_EVT_EOB = 0x4,
	GSI_CHAN_EVT_OOB = 0x5,
	GSI_CHAN_EVT_DB_MODE = 0x6,
	GSI_CHAN_EVT_UNDEFINED = 0x10,
	GSI_CHAN_EVT_RE_ERROR = 0x11,
};

/**
 * gsi_chan_xfer_notify - Channel callback info
 *
 * @chan_user_data: cookie supplied in gsi_alloc_channel
 * @xfer_user_data: cookie of the gsi_xfer_elem that caused the
 *                  event to be generated
 * @evt_id:         type of event triggered by the associated TRE
 *                  (corresponding to xfer_user_data)
 * @bytes_xfered:   number of bytes transferred by the associated TRE
 *                  (corresponding to xfer_user_data)
 *
 */
struct gsi_chan_xfer_notify {
	void *chan_user_data;
	void *xfer_user_data;
	enum gsi_chan_evt evt_id;
	uint16_t bytes_xfered;
};

enum gsi_chan_err {
	GSI_CHAN_INVALID_TRE_ERR = 0x0,
	GSI_CHAN_NON_ALLOCATED_EVT_ACCESS_ERR = 0x1,
	GSI_CHAN_OUT_OF_BUFFERS_ERR = 0x2,
	GSI_CHAN_OUT_OF_RESOURCES_ERR = 0x3,
	GSI_CHAN_UNSUPPORTED_INTER_EE_OP_ERR = 0x4,
	GSI_CHAN_HWO_1_ERR = 0x5
};

/**
 * gsi_chan_err_notify - Channel general callback info
 *
 * @chan_user_data: cookie supplied in gsi_alloc_channel
 * @evt_id:         type of error
 * @err_desc:  more info about the error
 *
 */
struct gsi_chan_err_notify {
	void *chan_user_data;
	enum gsi_chan_err evt_id;
	uint16_t err_desc;
};

enum gsi_chan_ring_elem_size {
	GSI_CHAN_RE_SIZE_4B = 4,
	GSI_CHAN_RE_SIZE_16B = 16,
	GSI_CHAN_RE_SIZE_32B = 32,
};

enum gsi_chan_use_db_eng {
	GSI_CHAN_DIRECT_MODE = 0x0,
	GSI_CHAN_DB_MODE = 0x1,
};

/**
 * gsi_chan_props - Channel related properties
 *
 * @prot:            interface type
 * @dir:             channel direction
 * @ch_id:           virtual channel ID
 * @evt_ring_hdl:    handle of associated event ring. set to ~0 if no
 *                   event ring associated
 * @re_size:         size of channel ring element
 * @ring_len:        length of ring in bytes (must be integral multiple of
 *                   re_size)
 * @ring_base_addr:  physical base address of ring. Address must be aligned to
 *                   ring_len rounded to power of two
 * @ring_base_vaddr: virtual base address of ring (set to NULL when not
 *                   applicable)
 * @use_db_eng:      0 => direct mode (doorbells are written directly to RE
 *                   engine)
 *                   1 => DB mode (doorbells are written to DB engine)
 * @max_prefetch:    limit number of pre-fetch segments for channel
 * @low_weight:      low channel weight (priority of channel for RE engine
 *                   round robin algorithm); must be >= 1
 * @xfer_cb:         transfer notification callback, this callback happens
 *                   on event boundaries
 *
 *                   e.g. 1
 *
 *                   out TD with 3 REs
 *
 *                   RE1: EOT=0, EOB=0, CHAIN=1;
 *                   RE2: EOT=0, EOB=0, CHAIN=1;
 *                   RE3: EOT=1, EOB=0, CHAIN=0;
 *
 *                   the callback will be triggered for RE3 using the
 *                   xfer_user_data of that RE
 *
 *                   e.g. 2
 *
 *                   in REs
 *
 *                   RE1: EOT=1, EOB=0, CHAIN=0;
 *                   RE2: EOT=1, EOB=0, CHAIN=0;
 *                   RE3: EOT=1, EOB=0, CHAIN=0;
 *
 *	             received packet consumes all of RE1, RE2 and part of RE3
 *	             for EOT condition. there will be three callbacks in below
 *	             order
 *
 *	             callback for RE1 using GSI_CHAN_EVT_OVERFLOW
 *	             callback for RE2 using GSI_CHAN_EVT_OVERFLOW
 *	             callback for RE3 using GSI_CHAN_EVT_EOT
 *
 * @err_cb:          error notification callback
 * @chan_user_data:  cookie used for notifications
 *
 * All the callbacks are in interrupt context
 *
 */
struct gsi_chan_props {
	enum gsi_chan_prot prot;
	enum gsi_chan_dir dir;
	uint8_t ch_id;
	unsigned long evt_ring_hdl;
	enum gsi_chan_ring_elem_size re_size;
	uint16_t ring_len;
	uint64_t ring_base_addr;
	void *ring_base_vaddr;
	enum gsi_chan_use_db_eng use_db_eng;
	enum gsi_max_prefetch max_prefetch;
	uint8_t low_weight;
	void (*xfer_cb)(struct gsi_chan_xfer_notify *notify);
	void (*err_cb)(struct gsi_chan_err_notify *notify);
	void *chan_user_data;
};

enum gsi_xfer_flag {
	GSI_XFER_FLAG_CHAIN = 0x1,
	GSI_XFER_FLAG_EOB = 0x100,
	GSI_XFER_FLAG_EOT = 0x200,
	GSI_XFER_FLAG_BEI = 0x400
};

enum gsi_xfer_elem_type {
	GSI_XFER_ELEM_DATA,
	GSI_XFER_ELEM_IMME_CMD,
};

/**
 * gsi_xfer_elem - Metadata about a single transfer
 *
 * @addr:           physical address of buffer
 * @len:            size of buffer for GSI_XFER_ELEM_DATA:
 *		    for outbound transfers this is the number of bytes to
 *		    transfer.
 *		    for inbound transfers, this is the maximum number of
 *		    bytes the host expects from device in this transfer
 *
 *                  immediate command opcode for GSI_XFER_ELEM_IMME_CMD
 * @flags:          transfer flags, OR of all the applicable flags
 *
 *		    GSI_XFER_FLAG_BEI: Block event interrupt
 *		    1: Event generated by this ring element must not assert
 *		    an interrupt to the host
 *		    0: Event generated by this ring element must assert an
 *		    interrupt to the host
 *
 *		    GSI_XFER_FLAG_EOT: Interrupt on end of transfer
 *		    1: If an EOT condition is encountered when processing
 *		    this ring element, an event is generated by the device
 *		    with its completion code set to EOT.
 *		    0: If an EOT condition is encountered for this ring
 *		    element, a completion event is not be generated by the
 *		    device, unless IEOB is 1
 *
 *		    GSI_XFER_FLAG_EOB: Interrupt on end of block
 *		    1: Device notifies host after processing this ring element
 *		    by sending a completion event
 *		    0: Completion event is not required after processing this
 *		    ring element
 *
 *		    GSI_XFER_FLAG_CHAIN: Chain bit that identifies the ring
 *		    elements in a TD
 *
 * @type:           transfer type
 *
 *		    GSI_XFER_ELEM_DATA: for all data transfers
 *		    GSI_XFER_ELEM_IMME_CMD: for IPA immediate commands
 *
 * @xfer_user_data: cookie used in xfer_cb
 *
 */
struct gsi_xfer_elem {
	uint64_t addr;
	uint16_t len;
	uint16_t flags;
	enum gsi_xfer_elem_type type;
	void *xfer_user_data;
};

/**
 * gsi_mhi_channel_scratch - MHI protocol SW config area of
 * channel scratch
 *
 * @mhi_host_wp_addr:    Valid only when UL/DL Sync En is asserted. Defines
 *                       address in host from which channel write pointer
 *                       should be read in polling mode
 * @max_outstanding_tre: Used for the prefetch management sequence by the
 *                       sequencer. Defines the maximum number of allowed
 *                       outstanding TREs in IPA/GSI (in Bytes). RE engine
 *                       prefetch will be limited by this configuration. It
 *                       is suggested to configure this value with the IPA_IF
 *                       channel AOS queue size. To disable the feature in
 *                       doorbell mode (DB Mode=1) Maximum outstanding TREs
 *                       should be set to 64KB (or any value larger or equal
 *                       to ring length . RLEN)
 * @assert_bit40:        1: bit #41 in address should be asserted upon
 *                       IPA_IF.ProcessDescriptor routine (for MHI over PCIe
 *                       transfers)
 *                       0: bit #41 in address should be deasserted upon
 *                       IPA_IF.ProcessDescriptor routine (for non-MHI over
 *                       PCIe transfers)
 * @ul_dl_sync_en:       When asserted, UL/DL synchronization feature is
 *                       enabled for the channel. Supported only for predefined
 *                       UL/DL endpoint pair
 * @outstanding_threshold: Used for the prefetch management sequence by the
 *                       sequencer. Defines the threshold (in Bytes) as to when
 *                       to update the channel doorbell. Should be smaller than
 *                       Maximum outstanding TREs. value.
 */
struct __packed gsi_mhi_channel_scratch {
	uint64_t mhi_host_wp_addr;
	uint32_t ul_dl_sync_en:1;
	uint32_t assert_bit40:1;
	uint32_t resvd1:14;
	uint32_t max_outstanding_tre:16;
	uint32_t resvd2:16;
	uint32_t outstanding_threshold:16;
};

/**
 * gsi_xdci_channel_scratch - xDCI protocol SW config area of
 * channel scratch
 *
 * @const_buffer_size:   TRB buffer size in KB (similar to IPA aggregationi
 *                       configuration). Must be aligned to Max USB Packet Size
 * @xferrscidx: Transfer Resource Index (XferRscIdx). The hardware-assigned
 *                       transfer resource index for the transfer, which was
 *                       returned in response to the Start Transfer command.
 *                       This field is used for "Update Transfer" command
 * @last_trb_addr:       Address (LSB - based on alignment restrictions) of
 *                       last TRB in queue. Used to identify rollover case
 * @depcmd_low_addr:     Used to generate "Update Transfer" command
 * @max_outstanding_tre: Used for the prefetch management sequence by the
 *                       sequencer. Defines the maximum number of allowed
 *                       outstanding TREs in IPA/GSI (in Bytes). RE engine
 *                       prefetch will be limited by this configuration. It
 *                       is suggested to configure this value with the IPA_IF
 *                       channel AOS queue size. To disable the feature in
 *                       doorbell mode (DB Mode=1) Maximum outstanding TREs
 *                       should be set to 64KB (or any value larger or equal
 *                       to ring length . RLEN)
 * @depcmd_hi_addr: Used to generate "Update Transfer" command
 * @outstanding_threshold: Used for the prefetch management sequence by the
 *                       sequencer. Defines the threshold (in Bytes) as to when
 *                       to update the channel doorbell. Should be smaller than
 *                       Maximum outstanding TREs. value.
 */
struct __packed gsi_xdci_channel_scratch {
	uint32_t last_trb_addr:16;
	uint32_t resvd1:4;
	uint32_t xferrscidx:7;
	uint32_t const_buffer_size:5;
	uint32_t depcmd_low_addr;
	uint32_t depcmd_hi_addr:8;
	uint32_t resvd2:8;
	uint32_t max_outstanding_tre:16;
	uint32_t resvd3:16;
	uint32_t outstanding_threshold:16;
};

/**
 * gsi_channel_scratch - channel scratch SW config area
 *
 */
union __packed gsi_channel_scratch {
	struct __packed gsi_mhi_channel_scratch mhi;
	struct __packed gsi_xdci_channel_scratch xdci;
	struct __packed {
		uint32_t word1;
		uint32_t word2;
		uint32_t word3;
		uint32_t word4;
	} data;
};

/**
 * gsi_mhi_evt_scratch - MHI protocol SW config area of
 * event scratch
 *
 * @ul_dl_sync_en: When asserted, UL/DL synchronization feature is enabled for
 *                 the channel. Supported only for predefined UL/DL endpoint
 *                 pair
 */
struct __packed gsi_mhi_evt_scratch {
	uint32_t resvd1:31;
	uint32_t ul_dl_sync_en:1;
	uint32_t resvd2;
};

/**
 * gsi_xdci_evt_scratch - xDCI protocol SW config area of
 * event scratch
 *
 */
struct __packed gsi_xdci_evt_scratch {
	uint32_t gevntcount_low_addr;
	uint32_t gevntcount_hi_addr:8;
	uint32_t resvd1:24;
};

/**
 * gsi_evt_scratch - event scratch SW config area
 *
 */
union __packed gsi_evt_scratch {
	struct __packed gsi_mhi_evt_scratch mhi;
	struct __packed gsi_xdci_evt_scratch xdci;
	struct __packed {
		uint32_t word1;
		uint32_t word2;
	} data;
};

/**
 * gsi_device_scratch - EE scratch config parameters
 *
 * @mhi_base_chan_idx_valid: is mhi_base_chan_idx valid?
 * @mhi_base_chan_idx:       base index of IPA MHI channel indexes.
 *                           IPA MHI channel index = GSI channel ID +
 *                           MHI base channel index
 * @max_usb_pkt_size_valid:  is max_usb_pkt_size valid?
 * @max_usb_pkt_size:        max USB packet size in bytes (valid values are
 *                           512 and 1024)
 */
struct gsi_device_scratch {
	bool mhi_base_chan_idx_valid;
	uint8_t mhi_base_chan_idx;
	bool max_usb_pkt_size_valid;
	uint16_t max_usb_pkt_size;
};

/**
 * gsi_chan_info - information about channel occupancy
 *
 * @wp: channel write pointer (physical address)
 * @rp: channel read pointer (physical address)
 * @evt_valid: is evt* info valid?
 * @evt_wp: event ring write pointer (physical address)
 * @evt_rp: event ring read pointer (physical address)
 */
struct gsi_chan_info {
	uint64_t wp;
	uint64_t rp;
	bool evt_valid;
	uint64_t evt_wp;
	uint64_t evt_rp;
};

#ifdef CONFIG_GSI
/**
 * gsi_register_device - Peripheral should call this function to
 * register itself with GSI before invoking any other APIs
 *
 * @props:  Peripheral properties
 * @dev_hdl:  Handle populated by GSI, opaque to client
 *
 * @Return -GSI_STATUS_AGAIN if request should be re-tried later
 *	   other error codes for failure
 */
int gsi_register_device(struct gsi_per_props *props, unsigned long *dev_hdl);

/**
 * gsi_complete_clk_grant - Peripheral should call this function to
 * grant the clock resource requested by GSI previously that could not
 * be granted synchronously. GSI will release the clock resource using
 * the rel_clk_cb when appropriate
 *
 * @dev_hdl:	   Client handle previously obtained from
 *	   gsi_register_device
 *
 * @Return gsi_status
 */
int gsi_complete_clk_grant(unsigned long dev_hdl);

/**
 * gsi_write_device_scratch - Peripheral should call this function to
 * write to the EE scratch area
 *
 * @dev_hdl:  Client handle previously obtained from
 *            gsi_register_device
 * @val:      Value to write
 *
 * @Return gsi_status
 */
int gsi_write_device_scratch(unsigned long dev_hdl,
		struct gsi_device_scratch *val);

/**
 * gsi_deregister_device - Peripheral should call this function to
 * de-register itself with GSI
 *
 * @dev_hdl:  Client handle previously obtained from
 *            gsi_register_device
 * @force:    When set to true, cleanup is performed even if there
 *            are in use resources like channels, event rings, etc.
 *            this would be used after GSI reset to recover from some
 *            fatal error
 *            When set to false, there must not exist any allocated
 *            channels and event rings.
 *
 * @Return gsi_status
 */
int gsi_deregister_device(unsigned long dev_hdl, bool force);

/**
 * gsi_alloc_evt_ring - Peripheral should call this function to
 * allocate an event ring
 *
 * @props:	   Event ring properties
 * @dev_hdl:	   Client handle previously obtained from
 *	   gsi_register_device
 * @evt_ring_hdl:  Handle populated by GSI, opaque to client
 *
 * This function can sleep
 *
 * @Return gsi_status
 */
int gsi_alloc_evt_ring(struct gsi_evt_ring_props *props, unsigned long dev_hdl,
		unsigned long *evt_ring_hdl);

/**
 * gsi_write_evt_ring_scratch - Peripheral should call this function to
 * write to the scratch area of the event ring context
 *
 * @evt_ring_hdl:  Client handle previously obtained from
 *	   gsi_alloc_evt_ring
 * @val:           Value to write
 *
 * @Return gsi_status
 */
int gsi_write_evt_ring_scratch(unsigned long evt_ring_hdl,
		union __packed gsi_evt_scratch val);

/**
 * gsi_dealloc_evt_ring - Peripheral should call this function to
 * de-allocate an event ring. There should not exist any active
 * channels using this event ring
 *
 * @evt_ring_hdl:  Client handle previously obtained from
 *	   gsi_alloc_evt_ring
 *
 * This function can sleep
 *
 * @Return gsi_status
 */
int gsi_dealloc_evt_ring(unsigned long evt_ring_hdl);

/**
 * gsi_query_evt_ring_db_addr - Peripheral should call this function to
 * query the physical addresses of the event ring doorbell registers
 *
 * @evt_ring_hdl:    Client handle previously obtained from
 *	     gsi_alloc_evt_ring
 * @db_addr_wp_lsb:  Physical address of doorbell register where the 32
 *                   LSBs of the doorbell value should be written
 * @db_addr_wp_msb:  Physical address of doorbell register where the 32
 *                   MSBs of the doorbell value should be written
 *
 * @Return gsi_status
 */
int gsi_query_evt_ring_db_addr(unsigned long evt_ring_hdl,
		uint32_t *db_addr_wp_lsb, uint32_t *db_addr_wp_msb);

/**
 * gsi_reset_evt_ring - Peripheral should call this function to
 * reset an event ring to recover from error state
 *
 * @evt_ring_hdl:  Client handle previously obtained from
 *             gsi_alloc_evt_ring
 *
 * This function can sleep
 *
 * @Return gsi_status
 */
int gsi_reset_evt_ring(unsigned long evt_ring_hdl);

/**
 * gsi_get_evt_ring_cfg - This function returns the current config
 * of the specified event ring
 *
 * @evt_ring_hdl:  Client handle previously obtained from
 *             gsi_alloc_evt_ring
 * @props:         where to copy properties to
 * @scr:           where to copy scratch info to
 *
 * @Return gsi_status
 */
int gsi_get_evt_ring_cfg(unsigned long evt_ring_hdl,
		struct gsi_evt_ring_props *props, union gsi_evt_scratch *scr);

/**
 * gsi_set_evt_ring_cfg - This function applies the supplied config
 * to the specified event ring.
 *
 * exclusive property of the event ring cannot be changed after
 * gsi_alloc_evt_ring
 *
 * @evt_ring_hdl:  Client handle previously obtained from
 *             gsi_alloc_evt_ring
 * @props:         the properties to apply
 * @scr:           the scratch info to apply
 *
 * @Return gsi_status
 */
int gsi_set_evt_ring_cfg(unsigned long evt_ring_hdl,
		struct gsi_evt_ring_props *props, union gsi_evt_scratch *scr);

/**
 * gsi_alloc_channel - Peripheral should call this function to
 * allocate a channel
 *
 * @props:     Channel properties
 * @dev_hdl:   Client handle previously obtained from
 *             gsi_register_device
 * @chan_hdl:  Handle populated by GSI, opaque to client
 *
 * This function can sleep
 *
 * @Return gsi_status
 */
int gsi_alloc_channel(struct gsi_chan_props *props, unsigned long dev_hdl,
		unsigned long *chan_hdl);

/**
 * gsi_write_channel_scratch - Peripheral should call this function to
 * write to the scratch area of the channel context
 *
 * @chan_hdl:  Client handle previously obtained from
 *             gsi_alloc_channel
 * @val:       Value to write
 *
 * @Return gsi_status
 */
int gsi_write_channel_scratch(unsigned long chan_hdl,
		union __packed gsi_channel_scratch val);

/**
 * gsi_start_channel - Peripheral should call this function to
 * start a channel i.e put into running state
 *
 * @chan_hdl:  Client handle previously obtained from
 *             gsi_alloc_channel
 *
 * This function can sleep
 *
 * @Return gsi_status
 */
int gsi_start_channel(unsigned long chan_hdl);

/**
 * gsi_stop_channel - Peripheral should call this function to
 * stop a channel. Stop will happen on a packet boundary
 *
 * @chan_hdl:  Client handle previously obtained from
 *             gsi_alloc_channel
 *
 * This function can sleep
 *
 * @Return -GSI_STATUS_AGAIN if client should call stop/stop_db again
 *	   other error codes for failure
 */
int gsi_stop_channel(unsigned long chan_hdl);

/**
 * gsi_reset_channel - Peripheral should call this function to
 * reset a channel to recover from error state
 *
 * @chan_hdl:  Client handle previously obtained from
 *             gsi_alloc_channel
 *
 * This function can sleep
 *
 * @Return gsi_status
 */
int gsi_reset_channel(unsigned long chan_hdl);

/**
 * gsi_dealloc_channel - Peripheral should call this function to
 * de-allocate a channel
 *
 * @chan_hdl:  Client handle previously obtained from
 *             gsi_alloc_channel
 *
 * This function can sleep
 *
 * @Return gsi_status
 */
int gsi_dealloc_channel(unsigned long chan_hdl);

/**
 * gsi_stop_db_channel - Peripheral should call this function to
 * stop a channel when all transfer elements till the doorbell
 * have been processed
 *
 * @chan_hdl:  Client handle previously obtained from
 *             gsi_alloc_channel
 *
 * This function can sleep
 *
 * @Return -GSI_STATUS_AGAIN if client should call stop/stop_db again
 *	   other error codes for failure
 */
int gsi_stop_db_channel(unsigned long chan_hdl);

/**
 * gsi_query_channel_db_addr - Peripheral should call this function to
 * query the physical addresses of the channel doorbell registers
 *
 * @chan_hdl:        Client handle previously obtained from
 *	     gsi_alloc_channel
 * @db_addr_wp_lsb:  Physical address of doorbell register where the 32
 *                   LSBs of the doorbell value should be written
 * @db_addr_wp_msb:  Physical address of doorbell register where the 32
 *                   MSBs of the doorbell value should be written
 *
 * @Return gsi_status
 */
int gsi_query_channel_db_addr(unsigned long chan_hdl,
		uint32_t *db_addr_wp_lsb, uint32_t *db_addr_wp_msb);

/**
 * gsi_query_channel_info - Peripheral can call this function to query the
 * channel and associated event ring (if any) status.
 *
 * @chan_hdl:  Client handle previously obtained from
 *             gsi_alloc_channel
 * @info:      Where to read the values into
 *
 * @Return gsi_status
 */
int gsi_query_channel_info(unsigned long chan_hdl,
		struct gsi_chan_info *info);

/**
 * gsi_is_channel_empty - Peripheral can call this function to query if
 * the channel is empty. This is only applicable to GPI. "Empty" means
 * GSI has consumed all descriptors for a TO_GSI channel and SW has
 * processed all completed descriptors for a FROM_GSI channel.
 *
 * @chan_hdl:  Client handle previously obtained from gsi_alloc_channel
 * @is_empty:  set by GSI based on channel emptiness
 *
 * @Return gsi_status
 */
int gsi_is_channel_empty(unsigned long chan_hdl, bool *is_empty);

/**
 * gsi_get_channel_cfg - This function returns the current config
 * of the specified channel
 *
 * @chan_hdl:  Client handle previously obtained from
 *             gsi_alloc_channel
 * @props:     where to copy properties to
 * @scr:       where to copy scratch info to
 *
 * @Return gsi_status
 */
int gsi_get_channel_cfg(unsigned long chan_hdl, struct gsi_chan_props *props,
		union gsi_channel_scratch *scr);

/**
 * gsi_set_channel_cfg - This function applies the supplied config
 * to the specified channel
 *
 * ch_id and evt_ring_hdl of the channel cannot be changed after
 * gsi_alloc_channel
 *
 * @chan_hdl:  Client handle previously obtained from
 *             gsi_alloc_channel
 * @props:     the properties to apply
 * @scr:       the scratch info to apply
 *
 * @Return gsi_status
 */
int gsi_set_channel_cfg(unsigned long chan_hdl, struct gsi_chan_props *props,
		union gsi_channel_scratch *scr);

/**
 * gsi_poll_channel - Peripheral should call this function to query for
 * completed transfer descriptors.
 *
 * @chan_hdl:  Client handle previously obtained from
 *             gsi_alloc_channel
 * @notify:    Information about the completed transfer if any
 *
 * @Return gsi_status (GSI_STATUS_POLL_EMPTY is returned if no transfers
 * completed)
 */
int gsi_poll_channel(unsigned long chan_hdl,
		struct gsi_chan_xfer_notify *notify);

/**
 * gsi_config_channel_mode - Peripheral should call this function
 * to configure the channel mode.
 *
 * @chan_hdl:  Client handle previously obtained from
 *             gsi_alloc_channel
 * @mode:      Mode to move the channel into
 *
 * @Return gsi_status
 */
int gsi_config_channel_mode(unsigned long chan_hdl, enum gsi_chan_mode mode);

/**
 * gsi_queue_xfer - Peripheral should call this function
 * to queue transfers on the given channel
 *
 * @chan_hdl:  Client handle previously obtained from
 *             gsi_alloc_channel
 * @num_xfers: Number of transfer in the array @ xfer
 * @xfer:      Array of num_xfers transfer descriptors
 * @ring_db:   If true, tell HW about these queued xfers
 *             If false, do not notify HW at this time
 *
 * @Return gsi_status
 */
int gsi_queue_xfer(unsigned long chan_hdl, uint16_t num_xfers,
		struct gsi_xfer_elem *xfer, bool ring_db);

/**
 * gsi_start_xfer - Peripheral should call this function to
 * inform HW about queued xfers
 *
 * @chan_hdl:  Client handle previously obtained from
 *             gsi_alloc_channel
 *
 * @Return gsi_status
 */
int gsi_start_xfer(unsigned long chan_hdl);

/**
 * gsi_configure_regs - Peripheral should call this function
 * to configure the GSI registers before/after the FW is
 * loaded but before it is enabled.
 *
 * @gsi_base_addr: Base address of GSI register space
 * @gsi_size: Mapping size of the GSI register space
 * @per_base_addr: Base address of the peripheral using GSI
 *
 * @Return gsi_status
 */
int gsi_configure_regs(phys_addr_t gsi_base_addr, u32 gsi_size,
		phys_addr_t per_base_addr);

/**
 * gsi_enable_fw - Peripheral should call this function
 * to enable the GSI FW after the FW has been loaded to the SRAM.
 *
 * @gsi_base_addr: Base address of GSI register space
 * @gsi_size: Mapping size of the GSI register space

 * @Return gsi_status
 */
int gsi_enable_fw(phys_addr_t gsi_base_addr, u32 gsi_size);

/*
 * Here is a typical sequence of calls
 *
 * gsi_register_device
 *
 * gsi_write_device_scratch (if the protocol needs this)
 *
 * gsi_alloc_evt_ring (for as many event rings as needed)
 * gsi_write_evt_ring_scratch
 *
 * gsi_alloc_channel (for as many channels as needed; channels can have
 * no event ring, an exclusive event ring or a shared event ring)
 * gsi_write_channel_scratch
 * gsi_start_channel
 * gsi_queue_xfer/gsi_start_xfer
 * gsi_config_channel_mode/gsi_poll_channel (if clients wants to poll on
 * xfer completions)
 * gsi_stop_db_channel/gsi_stop_channel
 *
 * gsi_dealloc_channel
 *
 * gsi_dealloc_evt_ring
 *
 * gsi_deregister_device
 *
 */
#else
static inline int gsi_register_device(struct gsi_per_props *props,
		unsigned long *dev_hdl)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_complete_clk_grant(unsigned long dev_hdl)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_write_device_scratch(unsigned long dev_hdl,
		struct gsi_device_scratch *val)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_deregister_device(unsigned long dev_hdl, bool force)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_alloc_evt_ring(struct gsi_evt_ring_props *props,
		unsigned long dev_hdl,
		unsigned long *evt_ring_hdl)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_write_evt_ring_scratch(unsigned long evt_ring_hdl,
		union __packed gsi_evt_scratch val)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_dealloc_evt_ring(unsigned long evt_ring_hdl)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_query_evt_ring_db_addr(unsigned long evt_ring_hdl,
		uint32_t *db_addr_wp_lsb, uint32_t *db_addr_wp_msb)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_reset_evt_ring(unsigned long evt_ring_hdl)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_alloc_channel(struct gsi_chan_props *props,
		unsigned long dev_hdl,
		unsigned long *chan_hdl)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_write_channel_scratch(unsigned long chan_hdl,
		union __packed gsi_channel_scratch val)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_start_channel(unsigned long chan_hdl)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_stop_channel(unsigned long chan_hdl)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_reset_channel(unsigned long chan_hdl)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_dealloc_channel(unsigned long chan_hdl)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_stop_db_channel(unsigned long chan_hdl)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_query_channel_db_addr(unsigned long chan_hdl,
		uint32_t *db_addr_wp_lsb, uint32_t *db_addr_wp_msb)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_query_channel_info(unsigned long chan_hdl,
		struct gsi_chan_info *info)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_is_channel_empty(unsigned long chan_hdl, bool *is_empty)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_poll_channel(unsigned long chan_hdl,
		struct gsi_chan_xfer_notify *notify)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_config_channel_mode(unsigned long chan_hdl,
		enum gsi_chan_mode mode)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_queue_xfer(unsigned long chan_hdl, uint16_t num_xfers,
		struct gsi_xfer_elem *xfer, bool ring_db)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_start_xfer(unsigned long chan_hdl)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_get_channel_cfg(unsigned long chan_hdl,
		struct gsi_chan_props *props,
		union gsi_channel_scratch *scr)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_set_channel_cfg(unsigned long chan_hdl,
		struct gsi_chan_props *props,
		union gsi_channel_scratch *scr)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_get_evt_ring_cfg(unsigned long evt_ring_hdl,
		struct gsi_evt_ring_props *props, union gsi_evt_scratch *scr)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_set_evt_ring_cfg(unsigned long evt_ring_hdl,
		struct gsi_evt_ring_props *props, union gsi_evt_scratch *scr)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_configure_regs(phys_addr_t gsi_base_addr, u32 gsi_size,
		phys_addr_t per_base_addr)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}
static inline int gsi_enable_fw(phys_addr_t gsi_base_addr, u32 gsi_size)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}
#endif
#endif
