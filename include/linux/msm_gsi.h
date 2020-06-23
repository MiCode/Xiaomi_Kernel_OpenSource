/* Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
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
#include <linux/interrupt.h>

enum gsi_ver {
	GSI_VER_ERR = 0,
	GSI_VER_1_0 = 1,
	GSI_VER_1_2 = 2,
	GSI_VER_1_3 = 3,
	GSI_VER_2_0 = 4,
	GSI_VER_2_2 = 5,
	GSI_VER_2_5 = 6,
	GSI_VER_MAX,
};

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
	GSI_STATUS_PENDING_IRQ = 13,
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
 * @gsi:        GSI core version
 * @ee:         EE where this driver and peripheral driver runs
 * @intr:       control interrupt type
 * @intvec:     write data for MSI write
 * @msi_addr:   MSI address
 * @irq:        IRQ number
 * @phys_addr:  physical address of GSI block
 * @size:       register size of GSI block
 * @emulator_intcntrlr_addr: the location of emulator's interrupt control block
 * @emulator_intcntrlr_size: the sise of emulator_intcntrlr_addr
 * @emulator_intcntrlr_client_isr: client's isr. Called by the emulator's isr
 * @mhi_er_id_limits_valid: valid flag for mhi_er_id_limits
 * @mhi_er_id_limits: MHI event ring start and end ids
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
 * @clk_status_cb: callback to update the current msm bus clock vote
 *
 * @enable_clk_bug_on: enable IPA clock for dump saving before assert
 * All the callbacks are in interrupt context
 *
 */
struct gsi_per_props {
	enum gsi_ver ver;
	unsigned int ee;
	enum gsi_intr_type intr;
	uint32_t intvec;
	uint64_t msi_addr;
	unsigned int irq;
	phys_addr_t phys_addr;
	unsigned long size;
	phys_addr_t emulator_intcntrlr_addr;
	unsigned long emulator_intcntrlr_size;
	irq_handler_t emulator_intcntrlr_client_isr;
	bool mhi_er_id_limits_valid;
	uint32_t mhi_er_id_limits[2];
	void (*notify_cb)(struct gsi_per_notify *notify);
	void (*req_clk_cb)(void *user_data, bool *granted);
	int (*rel_clk_cb)(void *user_data);
	void *user_data;
	int (*clk_status_cb)(void);
	void (*enable_clk_bug_on)(void);
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
	GSI_EVT_CHTYPE_XDCI_EV = 0x3,
	GSI_EVT_CHTYPE_WDI2_EV = 0x4,
	GSI_EVT_CHTYPE_GCI_EV = 0x5,
	GSI_EVT_CHTYPE_WDI3_EV = 0x6,
	GSI_EVT_CHTYPE_MHIP_EV = 0x7,
	GSI_EVT_CHTYPE_AQC_EV = 0x8,
	GSI_EVT_CHTYPE_11AD_EV = 0x9,
};

enum gsi_evt_ring_elem_size {
	GSI_EVT_RING_RE_SIZE_4B = 4,
	GSI_EVT_RING_RE_SIZE_8B = 8,
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
	GSI_CHAN_PROT_XDCI = 0x3,
	GSI_CHAN_PROT_WDI2 = 0x4,
	GSI_CHAN_PROT_GCI = 0x5,
	GSI_CHAN_PROT_WDI3 = 0x6,
	GSI_CHAN_PROT_MHIP = 0x7,
	GSI_CHAN_PROT_AQC = 0x8,
	GSI_CHAN_PROT_11AD = 0x9,
	GSI_CHAN_PROT_MHIC = 0xA,
	GSI_CHAN_PROT_QDSS = 0xB,
};

enum gsi_chan_dir {
	GSI_CHAN_DIR_FROM_GSI = 0x0,
	GSI_CHAN_DIR_TO_GSI = 0x1
};

enum gsi_max_prefetch {
	GSI_ONE_PREFETCH_SEG = 0x0,
	GSI_TWO_PREFETCH_SEG = 0x1
};

/**
 * @GSI_USE_PREFETCH_BUFS: Channel will use normal prefetch buffers if possible
 * @GSI_ESCAPE_BUF_ONLY: Channel will always use escape buffers only
 * @GSI_SMART_PRE_FETCH: Channel will work in smart prefetch mode.
 *	relevant starting GSI 2.5
 * @GSI_FREE_PRE_FETCH: Channel will work in free prefetch mode.
 *	relevant starting GSI 2.5
 */
enum gsi_prefetch_mode {
	GSI_USE_PREFETCH_BUFS = 0x0,
	GSI_ESCAPE_BUF_ONLY = 0x1,
	GSI_SMART_PRE_FETCH = 0x2,
	GSI_FREE_PRE_FETCH = 0x3,
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
 * gsi_chan_xfer_veid - Virtual Channel ID
 *
 * @GSI_VEID_0: transfer completed for VEID 0
 * @GSI_VEID_1: transfer completed for VEID 1
 * @GSI_VEID_2: transfer completed for VEID 2
 * @GSI_VEID_3: transfer completed for VEID 3
 * @GSI_VEID_DEFAULT: used when veid is invalid
 */
enum gsi_chan_xfer_veid {
	GSI_VEID_0 = 0,
	GSI_VEID_1 = 1,
	GSI_VEID_2 = 2,
	GSI_VEID_3 = 3,
	GSI_VEID_DEFAULT,
	GSI_VEID_MAX
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
 * @veid:           virtual endpoint id. Valid for GCI completions only
 *
 */
struct gsi_chan_xfer_notify {
	void *chan_user_data;
	void *xfer_user_data;
	enum gsi_chan_evt evt_id;
	uint16_t bytes_xfered;
	uint8_t veid;
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
	GSI_CHAN_RE_SIZE_8B = 8,
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
 * @max_re_expected: maximal number of ring elements expected to be queued.
 *                   used for data path statistics gathering. if 0 provided
 *                   ring_len / re_size will be used.
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
 * @empty_lvl_threshold:
 *                   The thershold number of free entries available in the
 *                   receiving fifos of GSI-peripheral. If Smart PF mode
 *                   is used, REE will fetch/send new TRE to peripheral only
 *                   if peripheral's empty_level_count is higher than
 *                   EMPTY_LVL_THRSHOLD defined for this channel
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
 * @cleanup_cb;	     cleanup rx-pkt/skb callback
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
	uint16_t max_re_expected;
	uint64_t ring_base_addr;
	void *ring_base_vaddr;
	enum gsi_chan_use_db_eng use_db_eng;
	enum gsi_max_prefetch max_prefetch;
	uint8_t low_weight;
	enum gsi_prefetch_mode prefetch_mode;
	uint8_t empty_lvl_threshold;
	void (*xfer_cb)(struct gsi_chan_xfer_notify *notify);
	void (*err_cb)(struct gsi_chan_err_notify *notify);
	void (*cleanup_cb)(void *chan_user_data, void *xfer_user_data);
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
	GSI_XFER_ELEM_NOP,
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
 *		    GSI_XFER_ELEM_NOP: for event generation only
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
 * gsi_gpi_channel_scratch - GPI protocol SW config area of
 * channel scratch
 *
 * @dl_nlo_channel:      Whether this is DL NLO Channel or not? Relevant for
 *                       GSI 2.5 and above where DL NLO introduced.
 * @max_outstanding_tre: Used for the prefetch management sequence by the
 *                       sequencer. Defines the maximum number of allowed
 *                       outstanding TREs in IPA/GSI (in Bytes). RE engine
 *                       prefetch will be limited by this configuration. It
 *                       is suggested to configure this value to IPA_IF
 *                       channel TLV queue size times element size. To disable
 *                       the feature in doorbell mode (DB Mode=1). Maximum
 *                       outstanding TREs should be set to 64KB
 *                       (or any value larger or equal to ring length . RLEN)
 *                       The field is irrelevant starting GSI 2.5 where smart
 *                       prefetch implemented by the H/W.
 * @outstanding_threshold: Used for the prefetch management sequence by the
 *                       sequencer. Defines the threshold (in Bytes) as to when
 *                       to update the channel doorbell. Should be smaller than
 *                       Maximum outstanding TREs. value. It is suggested to
 *                       configure this value to 2 * element size.
 *                       The field is irrelevant starting GSI 2.5 where smart
 *                       prefetch implemented by the H/W.
 */
struct __packed gsi_gpi_channel_scratch {
	uint64_t dl_nlo_channel:1; /* Relevant starting GSI 2.5 */
	uint64_t resvd1:63;
	uint32_t resvd2:16;
	uint32_t max_outstanding_tre:16; /* Not relevant starting GSI 2.5 */
	uint32_t resvd3:16;
	uint32_t outstanding_threshold:16; /* Not relevant starting GSI 2.5 */
};

/**
 * gsi_mhi_channel_scratch - MHI protocol SW config area of
 * channel scratch
 *
 * @mhi_host_wp_addr:    Valid only when UL/DL Sync En is asserted. Defines
 *                       address in host from which channel write pointer
 *                       should be read in polling mode
 * @assert_bit40:        1: bit #41 in address should be asserted upon
 *                       IPA_IF.ProcessDescriptor routine (for MHI over PCIe
 *                       transfers)
 *                       0: bit #41 in address should be deasserted upon
 *                       IPA_IF.ProcessDescriptor routine (for non-MHI over
 *                       PCIe transfers)
 * @polling_configuration: Uplink channels: Defines timer to poll on MHI
 *                       context. Range: 1 to 31 milliseconds.
 *                       Downlink channel: Defines transfer ring buffer
 *                       availability threshold to poll on MHI context in
 *                       multiple of 8. Range: 0 to 31, meaning 0 to 258 ring
 *                       elements. E.g., value of 2 indicates 16 ring elements.
 *                       Valid only when Burst Mode Enabled is set to 1
 * @burst_mode_enabled:  0: Burst mode is disabled for this channel
 *                       1: Burst mode is enabled for this channel
 * @polling_mode:        0: the channel is not in polling mode, meaning the
 *                       host should ring DBs.
 *                       1: the channel is in polling mode, meaning the host
 * @oob_mod_threshold:   Defines OOB moderation threshold. Units are in 8
 *                       ring elements.
 *                       should not ring DBs until notified of DB mode/OOB mode
 * @max_outstanding_tre: Used for the prefetch management sequence by the
 *                       sequencer. Defines the maximum number of allowed
 *                       outstanding TREs in IPA/GSI (in Bytes). RE engine
 *                       prefetch will be limited by this configuration. It
 *                       is suggested to configure this value to IPA_IF
 *                       channel TLV queue size times element size.
 *                       To disable the feature in doorbell mode (DB Mode=1).
 *                       Maximum outstanding TREs should be set to 64KB
 *                       (or any value larger or equal to ring length . RLEN)
 *                       The field is irrelevant starting GSI 2.5 where smart
 *                       prefetch implemented by the H/W.
 * @outstanding_threshold: Used for the prefetch management sequence by the
 *                       sequencer. Defines the threshold (in Bytes) as to when
 *                       to update the channel doorbell. Should be smaller than
 *                       Maximum outstanding TREs. value. It is suggested to
 *                       configure this value to min(TLV_FIFO_SIZE/2,8) *
 *                       element size.
 *                       The field is irrelevant starting GSI 2.5 where smart
 *                       prefetch implemented by the H/W.
 */
struct __packed gsi_mhi_channel_scratch {
	uint64_t mhi_host_wp_addr;
	uint32_t rsvd1:1;
	uint32_t assert_bit40:1;
	uint32_t polling_configuration:5;
	uint32_t burst_mode_enabled:1;
	uint32_t polling_mode:1;
	uint32_t oob_mod_threshold:5;
	uint32_t resvd2:2;
	uint32_t max_outstanding_tre:16; /* Not relevant starting GSI 2.5 */
	uint32_t resvd3:16;
	uint32_t outstanding_threshold:16; /* Not relevant starting GSI 2.5 */
};

/**
 * gsi_mhi_channel_scratch_v2 - MHI protocol SW config area of
 * channel scratch
 *
 * @mhi_host_wp_addr_lo: Valid only when UL/DL Sync En is asserted. Defines
 *                       address in host from which channel write pointer
 *                       should be read in polling mode
 * @mhi_host_wp_addr_hi: Valid only when UL/DL Sync En is asserted. Defines
 *                       address in host from which channel write pointer
 *                       should be read in polling mode
 * @assert_bit40:        1: bit #41 in address should be asserted upon
 *                       IPA_IF.ProcessDescriptor routine (for MHI over PCIe
 *                       transfers)
 *                       0: bit #41 in address should be deasserted upon
 *                       IPA_IF.ProcessDescriptor routine (for non-MHI over
 *                       PCIe transfers)
 * @polling_configuration: Uplink channels: Defines timer to poll on MHI
 *                       context. Range: 1 to 31 milliseconds.
 *                       Downlink channel: Defines transfer ring buffer
 *                       availability threshold to poll on MHI context in
 *                       multiple of 8. Range: 0 to 31, meaning 0 to 258 ring
 *                       elements. E.g., value of 2 indicates 16 ring elements.
 *                       Valid only when Burst Mode Enabled is set to 1
 * @burst_mode_enabled:  0: Burst mode is disabled for this channel
 *                       1: Burst mode is enabled for this channel
 * @polling_mode:        0: the channel is not in polling mode, meaning the
 *                       host should ring DBs.
 *                       1: the channel is in polling mode, meaning the host
 * @oob_mod_threshold:   Defines OOB moderation threshold. Units are in 8
 *                       ring elements.
 *                       should not ring DBs until notified of DB mode/OOB mode
 */
struct __packed gsi_mhi_channel_scratch_v2 {
	uint32_t mhi_host_wp_addr_lo;
	uint32_t mhi_host_wp_addr_hi:9;
	uint32_t polling_configuration:5;
	uint32_t rsvd1:18;
	uint32_t rsvd2:1;
	uint32_t assert_bit40:1;
	uint32_t resvd3:5;
	uint32_t burst_mode_enabled:1;
	uint32_t polling_mode:1;
	uint32_t oob_mod_threshold:5;
	uint32_t resvd4:18; /* Not configured by AP */
	uint32_t resvd5; /* Not configured by AP */
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
 *                       is suggested to configure this value to IPA_IF
 *                       channel TLV queue size times element size.
 *                       To disable the feature in doorbell mode (DB Mode=1)
 *                       Maximum outstanding TREs should be set to 64KB
 *                       (or any value larger or equal to ring length . RLEN)
 *                       The field is irrelevant starting GSI 2.5 where smart
 *                       prefetch implemented by the H/W.
 * @depcmd_hi_addr: Used to generate "Update Transfer" command
 * @outstanding_threshold: Used for the prefetch management sequence by the
 *                       sequencer. Defines the threshold (in Bytes) as to when
 *                       to update the channel doorbell. Should be smaller than
 *                       Maximum outstanding TREs. value. It is suggested to
 *                       configure this value to 2 * element size. for MBIM the
 *                       suggested configuration is the element size.
 *                       The field is irrelevant starting GSI 2.5 where smart
 *                       prefetch implemented by the H/W.
 */
struct __packed gsi_xdci_channel_scratch {
	uint32_t last_trb_addr:16;
	uint32_t resvd1:4;
	uint32_t xferrscidx:7;
	uint32_t const_buffer_size:5;
	uint32_t depcmd_low_addr;
	uint32_t depcmd_hi_addr:8;
	uint32_t resvd2:8;
	uint32_t max_outstanding_tre:16; /* Not relevant starting GSI 2.5 */
	uint32_t resvd3:16;
	uint32_t outstanding_threshold:16; /* Not relevant starting GSI 2.5 */
};

/**
 * gsi_wdi_channel_scratch - WDI protocol SW config area of
 * channel scratch
 *
 * @wifi_rx_ri_addr_low: Low 32 bits of Transfer ring Read Index address.
 * @wifi_rx_ri_addr_high: High 32 bits of Transfer ring Read Index address.
 * @update_ri_moderation_threshold: Threshold N for Transfer ring Read Index
 *                                  N is the number of packets that IPA will
 *                                  process before Wifi transfer ring Ri will
 *                                  be updated.
 * @update_ri_moderation_counter: This field is incremented with each TRE
 *                                processed in MCS.
 * @wdi_rx_tre_proc_in_progress: It is set if IPA IF returned BECAME FULL
 *                               status after MCS submitted an inline immediate
 *                               command to update the metadata. It allows MCS
 *                               to know that it has to retry sending the TRE
 *                               to IPA.
 * @wdi_rx_vdev_id: Rx only. Initialized to 0xFF by SW after allocating channel
 *                  and before starting it. Both FW_DESC and VDEV_ID are part
 *                  of a scratch word that is Read/Write for both MCS and SW.
 *                  To avoid race conditions, SW should not update this field
 *                  after starting the channel.
 * @wdi_rx_fw_desc: Rx only. Initialized to 0xFF by SW after allocating channel
 *                  and before starting it. After Start, this is a Read only
 *                  field for SW.
 * @endp_metadatareg_offset: Rx only, the offset of IPA_ENDP_INIT_HDR_METADATA
 *                           of the corresponding endpoint in 4B words from IPA
 *                           base address. Read only field for MCS.
 *                           Write for SW.
 * @qmap_id: Rx only, used for setting metadata register in IPA. Read only field
 *           for MCS. Write for SW.
 * @wdi_rx_pkt_length: If WDI_RX_TRE_PROC_IN_PROGRESS is set, this field is
 *                     valid and contains the packet length of the TRE that
 *                     needs to be submitted to IPA.
 * @resv1: reserved bits.
 * @pkt_comp_count: It is incremented on each AOS received. When event ring
 *                  Write index is updated, it is decremented by the same
 *                  amount.
 * @stop_in_progress_stm: If a Stop request is in progress, this will indicate
 *                        the current stage of processing of the stop within MCS
 * @resv2: reserved bits.
 * wdi_rx_qmap_id_internal: Initialized to 0 by MCS when the channel is
 *                          allocated. It is updated to the current value of SW
 *                          QMAP ID that is being written by MCS to the IPA
 *                          metadata register.
 */
struct __packed gsi_wdi_channel_scratch {
	uint32_t wifi_rx_ri_addr_low;
	uint32_t wifi_rx_ri_addr_high;
	uint32_t update_ri_moderation_threshold:5;
	uint32_t update_ri_moderation_counter:6;
	uint32_t wdi_rx_tre_proc_in_progress:1;
	uint32_t resv1:4;
	uint32_t wdi_rx_vdev_id:8;
	uint32_t wdi_rx_fw_desc:8;
	uint32_t endp_metadatareg_offset:16;
	uint32_t qmap_id:16;
	uint32_t wdi_rx_pkt_length:16;
	uint32_t resv2:2;
	uint32_t pkt_comp_count:11;
	uint32_t stop_in_progress_stm:3;
	uint32_t resv3:16;
	uint32_t wdi_rx_qmap_id_internal:16;
};

/**
 * gsi_wdi2_channel_scratch_lito - WDI protocol SW config area of
 * channel scratch
 *
 * @wifi_rx_ri_addr_low: Low 32 bits of Transfer ring Read Index address.
 * @wifi_rx_ri_addr_high: High 32 bits of Transfer ring Read Index address.
 * @update_ri_moderation_threshold: Threshold N for Transfer ring Read Index
 *                                  N is the number of packets that IPA will
 *                                  process before Wifi transfer ring Ri will
 *                                  be updated.
 * @qmap_id: Rx only, used for setting metadata register in IPA. Read only field
 *           for MCS. Write for SW.
 * @endp_metadatareg_offset: Rx only, the offset of IPA_ENDP_INIT_HDR_METADATA
 *                           of the corresponding endpoint in 4B words from IPA
 *                           base address. Read only field for MCS.
 *                           Write for SW.
 * @wdi_rx_vdev_id: Rx only. Initialized to 0xFF by SW after allocating channel
 *                  and before starting it. Both FW_DESC and VDEV_ID are part
 *                  of a scratch word that is Read/Write for both MCS and SW.
 *                  To avoid race conditions, SW should not update this field
 *                  after starting the channel.
 * @wdi_rx_fw_desc: Rx only. Initialized to 0xFF by SW after allocating channel
 *                  and before starting it. After Start, this is a Read only
 *                  field for SW.
 * @update_ri_moderation_counter: This field is incremented with each TRE
 *                                processed in MCS.
 * @wdi_rx_tre_proc_in_progress: It is set if IPA IF returned BECAME FULL
 *                               status after MCS submitted an inline immediate
 *                               command to update the metadata. It allows MCS
 *                               to know that it has to retry sending the TRE
 *                               to IPA.
 * @outstanding_tlvs_counter: It is the count of outstanding TLVs submitted to
 *                           IPA by MCS and waiting for AOS completion from IPA.
 * @wdi_rx_pkt_length: If WDI_RX_TRE_PROC_IN_PROGRESS is set, this field is
 *                     valid and contains the packet length of the TRE that
 *                     needs to be submitted to IPA.
 * @resv1: reserved bits.
 * @pkt_comp_count: It is incremented on each AOS received. When event ring
 *                  Write index is updated, it is decremented by the same
 *                  amount.
 * @stop_in_progress_stm: If a Stop request is in progress, this will indicate
 *                        the current stage of processing of the stop within MCS
 * @resv2: reserved bits.
 * wdi_rx_qmap_id_internal: Initialized to 0 by MCS when the channel is
 *                          allocated. It is updated to the current value of SW
 *                          QMAP ID that is being written by MCS to the IPA
 *                          metadata register.
 */
struct __packed gsi_wdi2_channel_scratch_new {
	uint32_t wifi_rx_ri_addr_low;
	uint32_t wifi_rx_ri_addr_high;
	uint32_t update_ri_moderation_threshold:5;
	uint32_t qmap_id:8;
	uint32_t resv1:3;
	uint32_t endp_metadatareg_offset:16;
	uint32_t wdi_rx_vdev_id:8;
	uint32_t wdi_rx_fw_desc:8;
	uint32_t update_ri_moderation_counter:6;
	uint32_t wdi_rx_tre_proc_in_progress:1;
	uint32_t resv4:1;
	uint32_t outstanding_tlvs_counter:8;
	uint32_t wdi_rx_pkt_length:16;
	uint32_t resv2:2;
	uint32_t pkt_comp_count:11;
	uint32_t stop_in_progress_stm:3;
	uint32_t resv3:16;
	uint32_t wdi_rx_qmap_id_internal:16;
};
/**
* gsi_mhip_channel_scratch - MHI PRIME protocol SW config area of
* channel scratch
* @assert_bit_40: Valid only for non-host channels.
* Set to 1 for MHI’ channels when running over PCIe.
* @host_channel: Set to 1 for MHIP channel running on host.
*
*/
struct __packed gsi_mhip_channel_scratch {
	uint32_t assert_bit_40:1;
	uint32_t host_channel:1;
	uint32_t resvd1:30;
};


/**
* gsi_11ad_rx_channel_scratch - 11AD protocol SW config area of
* RX channel scratch
*
* @status_ring_hwtail_address_lsb: Low 32 bits of status ring hwtail address.
* @status_ring_hwtail_address_msb: High 32 bits of status ring hwtail address.
* @data_buffers_base_address_lsb: Low 32 bits of the data buffers address.
* @data_buffers_base_address_msb: High 32 bits of the data buffers address.
* @fixed_data_buffer_size_pow_2: the fixed buffer size power of 2 (> MTU).
* @resv1: reserved bits.
*/
struct __packed gsi_11ad_rx_channel_scratch {
	uint32_t status_ring_hwtail_address_lsb;
	uint32_t status_ring_hwtail_address_msb;
	uint32_t data_buffers_base_address_lsb;
	uint32_t data_buffers_base_address_msb:8;
	uint32_t fixed_data_buffer_size_pow_2:16;
	uint32_t resv1:8;
};

/**
 * gsi_11ad_tx_channel_scratch - 11AD protocol SW config area of
 * TX channel scratch
 *
 * @status_ring_hwtail_address_lsb: Low 32 bits of status ring hwtail address.
 * @status_ring_hwhead_address_lsb: Low 32 bits of status ring hwhead address.
 * @status_ring_hwhead_hwtail_8_msb: higher 8 msbs of status ring
 *	hwhead\hwtail addresses (should be identical).
 * @update_status_hwtail_mod_threshold: The threshold in (32B) elements for
 *	updating descriptor ring 11ad HWTAIL pointer moderation.
 * @status_ring_num_elem - the number of elements in the status ring.
 * @resv1: reserved bits.
 * @fixed_data_buffer_size_pow_2: the fixed buffer size power of 2 (> MTU).
 * @resv2: reserved bits.
 */
struct __packed gsi_11ad_tx_channel_scratch {
	uint32_t status_ring_hwtail_address_lsb;
	uint32_t status_ring_hwhead_address_lsb;
	uint32_t status_ring_hwhead_hwtail_8_msb:8;
	uint32_t update_status_hwtail_mod_threshold:8;
	uint32_t status_ring_num_elem:16;
	uint32_t resv1:8;
	uint32_t fixed_data_buffer_size_pow_2:16;
	uint32_t resv2:8;
};

/**
 * gsi_wdi3_channel_scratch - WDI protocol 3 SW config area of
 * channel scratch
 *
 * @wifi_rx_ri_addr_low: Low 32 bits of Transfer ring Read Index address.
 * @wifi_rx_ri_addr_high: High 32 bits of Transfer ring Read Index address.
 * @update_ri_moderation_threshold: Threshold N for Transfer ring Read Index
 *                                  N is the number of packets that IPA will
 *                                  process before Wifi transfer ring Ri will
 *                                  be updated.
 * @qmap_id: Rx only, used for setting metadata register in IPA. Read only field
 *           for MCS. Write for SW.
 * @resv: reserved bits.
 * @endp_metadata_reg_offset: Rx only, the offset of
 *                 IPA_ENDP_INIT_HDR_METADATA_n of the
 *                 corresponding endpoint in 4B words from IPA
 *                 base address.
 * @rx_pkt_offset: Rx only, Since Rx header length is not fixed,
 *                  WLAN host will pass this information to IPA.
 * @resv: reserved bits.
 */
struct __packed gsi_wdi3_channel_scratch {
	uint32_t wifi_rp_address_low;
	uint32_t wifi_rp_address_high;
	uint32_t update_rp_moderation_threshold : 5;
	uint32_t qmap_id : 8;
	uint32_t reserved1 : 3;
	uint32_t endp_metadata_reg_offset : 16;
	uint32_t rx_pkt_offset : 16;
	uint32_t reserved2 : 16;
};

/**
 * gsi_qdss_channel_scratch - QDSS SW config area of
 * channel scratch
 *
 * @bam_p_evt_dest_addr: equivalent to event_ring_doorbell_pa
 *			physical address of the doorbell that IPA uC
 *			will update the headpointer of the event ring.
 *			QDSS should send BAM_P_EVNT_REG address in this var
 *			Configured with the GSI Doorbell Address.
 *			GSI sends Update RP by doing a write to this address
 * @data_fifo_base_addr: Base address of the data FIFO used by BAM
 * @data_fifo_size: Size of the data FIFO
 * @bam_p_evt_threshold: Threshold level of how many bytes consumed
 * @override_eot: if override EOT==1, it doesn't check the EOT bit in
 *			the descriptor
 */
struct __packed gsi_qdss_channel_scratch {
	uint32_t bam_p_evt_dest_addr;
	uint32_t data_fifo_base_addr;
	uint32_t data_fifo_size : 16;
	uint32_t bam_p_evt_threshold : 16;
	uint32_t reserved1 : 2;
	uint32_t override_eot : 1;
	uint32_t reserved2 : 29;
};

/**
 * gsi_wdi3_channel_scratch2 - WDI3 protocol SW config area of
 * channel scratch2
 *
 * @update_ri_moderation_threshold: Threshold N for Transfer ring Read Index
 *		N is the number of packets that IPA will
 *		process before Wifi transfer ring Ri will
 *		be updated.
 * @qmap_id: Rx only, used for setting metadata register in IPA. Read only
 *		field for MCS. Write for SW.
 * @resv: reserved bits.
 * @endp_metadata_reg_offset: Rx only, the offset of
 *		IPA_ENDP_INIT_HDR_METADATA_n of the
 *		corresponding endpoint in 4B words from IPA
 *		base address.
 */

struct __packed gsi_wdi3_channel_scratch2 {
	uint32_t update_rp_moderation_threshold : 5;
	uint32_t qmap_id : 8;
	uint32_t reserved1 : 3;
	uint32_t endp_metadata_reg_offset : 16;
};

/**
 * gsi_wdi3_channel_scratch2_reg - channel scratch2 SW config area
 *
 */

union __packed gsi_wdi3_channel_scratch2_reg {
	struct __packed gsi_wdi3_channel_scratch2 wdi;
	struct __packed {
		uint32_t word1;
	} data;
};


/**
 * gsi_channel_scratch - channel scratch SW config area
 *
 */
union __packed gsi_channel_scratch {
	struct __packed gsi_gpi_channel_scratch gpi;
	struct __packed gsi_mhi_channel_scratch mhi;
	struct __packed gsi_mhi_channel_scratch_v2 mhi_v2;
	struct __packed gsi_xdci_channel_scratch xdci;
	struct __packed gsi_wdi_channel_scratch wdi;
	struct __packed gsi_11ad_rx_channel_scratch rx_11ad;
	struct __packed gsi_11ad_tx_channel_scratch tx_11ad;
	struct __packed gsi_wdi3_channel_scratch wdi3;
	struct __packed gsi_mhip_channel_scratch mhip;
	struct __packed gsi_wdi2_channel_scratch_new wdi2_new;
	struct __packed gsi_qdss_channel_scratch qdss;
	struct __packed {
		uint32_t word1;
		uint32_t word2;
		uint32_t word3;
		uint32_t word4;
	} data;
};

/**
 * gsi_wdi_channel_scratch3 - WDI protocol SW config area of
 * channel scratch3
 */

struct __packed gsi_wdi_channel_scratch3 {
	uint32_t endp_metadatareg_offset:16;
	uint32_t qmap_id:16;
};

/**
 * gsi_wdi_channel_scratch3_reg - channel scratch3 SW config area
 *
 */

union __packed gsi_wdi_channel_scratch3_reg {
	struct __packed gsi_wdi_channel_scratch3 wdi;
	struct __packed {
		uint32_t word1;
	} data;
};

/**
 * gsi_wdi2_channel_scratch2 - WDI protocol SW config area of
 * channel scratch2
 */

struct __packed gsi_wdi2_channel_scratch2 {
	uint32_t update_ri_moderation_threshold:5;
	uint32_t qmap_id:8;
	uint32_t resv1:3;
	uint32_t endp_metadatareg_offset:16;
};

/**
 * gsi_wdi_channel_scratch2_reg - channel scratch2 SW config area
 *
 */

union __packed gsi_wdi2_channel_scratch2_reg {
	struct __packed gsi_wdi2_channel_scratch2 wdi;
	struct __packed {
		uint32_t word1;
	} data;
};

/**
 * gsi_mhi_evt_scratch - MHI protocol SW config area of
 * event scratch
 */
struct __packed gsi_mhi_evt_scratch {
	uint32_t resvd1;
	uint32_t resvd2;
};

/**
* gsi_mhip_evt_scratch - MHI PRIME protocol SW config area of
* event scratch
*/
struct __packed gsi_mhip_evt_scratch {
	uint32_t rp_mod_threshold:8;
	uint32_t rp_mod_timer:4;
	uint32_t rp_mod_counter:8;
	uint32_t rp_mod_timer_id:4;
	uint32_t rp_mod_timer_running:1;
	uint32_t resvd1:7;
	uint32_t fixed_buffer_sz:16;
	uint32_t resvd2:16;
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
 * gsi_wdi_evt_scratch - WDI protocol SW config area of
 * event scratch
 *
 */

struct __packed gsi_wdi_evt_scratch {
	uint32_t update_ri_moderation_config:8;
	uint32_t resvd1:8;
	uint32_t update_ri_mod_timer_running:1;
	uint32_t evt_comp_count:14;
	uint32_t resvd2:1;
	uint32_t last_update_ri:16;
	uint32_t resvd3:16;
};

/**
 * gsi_11ad_evt_scratch - 11AD protocol SW config area of
 * event scratch
 *
 */
struct __packed gsi_11ad_evt_scratch {
	uint32_t update_status_hwtail_mod_threshold : 8;
	uint32_t resvd1:8;
	uint32_t resvd2:16;
	uint32_t resvd3;
};

/**
 * gsi_wdi3_evt_scratch - wdi3 protocol SW config area of
 * event scratch
 * @update_ri_moderation_threshold: Threshold N for Transfer ring Read Index
 *                                  N is the number of packets that IPA will
 *                                  process before Wifi transfer ring Ri will
 *                                  be updated.
 * @reserved1: reserve bit.
 * @reserved2: reserve bit.
 */
struct __packed gsi_wdi3_evt_scratch {
	uint32_t update_rp_moderation_config : 8;
	uint32_t reserved1 : 24;
	uint32_t reserved2;
};

/**
 * gsi_evt_scratch - event scratch SW config area
 *
 */
union __packed gsi_evt_scratch {
	struct __packed gsi_mhi_evt_scratch mhi;
	struct __packed gsi_xdci_evt_scratch xdci;
	struct __packed gsi_wdi_evt_scratch wdi;
	struct __packed gsi_11ad_evt_scratch w11ad;
	struct __packed gsi_wdi3_evt_scratch wdi3;
	struct __packed gsi_mhip_evt_scratch mhip;
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
 *                           64, 512 and 1024)
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
 * gsi_is_mcs_enabled - Peripheral should call this function to
 * check if MCS is already loaded.
 *
 * @Return -GSI_STATUS_NODEV if node is already created.
 *	   other error codes for failure
 */
int gsi_is_mcs_enabled(void);

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
 * gsi_ring_evt_ring_db - Peripheral should call this function for
 * ringing the event ring doorbell with given value
 *
 * @evt_ring_hdl:    Client handle previously obtained from
 *	     gsi_alloc_evt_ring
 * @value:           The value to be used for ringing the doorbell
 *
 * @Return gsi_status
 */
int gsi_ring_evt_ring_db(unsigned long evt_ring_hdl, uint64_t value);

/**
* gsi_ring_ch_ring_db - Peripheral should call this function for
* ringing the channel ring doorbell with given value
*
* @chan_hdl:    Client handle previously obtained from
*	     gsi_alloc_channel
* @value:           The value to be used for ringing the doorbell
*
* @Return gsi_status
*/
int gsi_ring_ch_ring_db(unsigned long chan_hdl, uint64_t value);

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
 * gsi_write_channel_scratch3_reg - Peripheral should call this function to
 * write to the scratch3 reg area of the channel context
 *
 * @chan_hdl:  Client handle previously obtained from
 *             gsi_alloc_channel
 * @val:       Value to write
 *
 * @Return gsi_status
 */
int gsi_write_channel_scratch3_reg(unsigned long chan_hdl,
		union __packed gsi_wdi_channel_scratch3_reg val);

/**
 * gsi_write_wdi3_channel_scratch2_reg - Peripheral should call this function
 * to write to the WDI3 scratch 3 register area of the channel context
 *
 * @chan_hdl:  Client handle previously obtained from
 *             gsi_alloc_channel
 * @val:       Read value
 *
 * @Return gsi_status
 */
int gsi_write_wdi3_channel_scratch2_reg(unsigned long chan_hdl,
		union __packed gsi_wdi3_channel_scratch2_reg val);
/**
 * gsi_write_channel_scratch2_reg - Peripheral should call this function to
 * write to the scratch2 reg area of the channel context
 *
 * @chan_hdl:  Client handle previously obtained from
 *             gsi_alloc_channel
 * @val:       Value to write
 *
 * @Return gsi_status
 */
int gsi_write_channel_scratch2_reg(unsigned long chan_hdl,
		union __packed gsi_wdi2_channel_scratch2_reg val);

/**
 * gsi_read_channel_scratch - Peripheral should call this function to
 * read to the scratch area of the channel context
 *
 * @chan_hdl:  Client handle previously obtained from
 *             gsi_alloc_channel
 * @val:       Read value
 *
 * @Return gsi_status
 */
int gsi_read_channel_scratch(unsigned long chan_hdl,
		union __packed gsi_channel_scratch *val);

/**
 * gsi_read_wdi3_channel_scratch2_reg - Peripheral should call this function to
 * read to the WDI3 scratch 2 register area of the channel context
 *
 * @chan_hdl:  Client handle previously obtained from
 *             gsi_alloc_channel
 * @val:       Read value
 *
 * @Return gsi_status
 */
int gsi_read_wdi3_channel_scratch2_reg(unsigned long chan_hdl,
		union __packed gsi_wdi3_channel_scratch2_reg *val);

/**
 * gsi_update_mhi_channel_scratch - MHI Peripheral should call this
 * function to update the scratch area of the channel context. Updating
 * will be by read-modify-write method, so non SWI fields will not be
 * affected
 *
 * @chan_hdl:  Client handle previously obtained from
 *             gsi_alloc_channel
 * @mscr:      MHI Channel Scratch value
 *
 * @Return gsi_status
 */
int gsi_update_mhi_channel_scratch(unsigned long chan_hdl,
		struct __packed gsi_mhi_channel_scratch mscr);

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
 * gsi_poll_n_channel - Peripheral should call this function to query for
 * completed transfer descriptors.
 *
 * @chan_hdl:  Client handle previously obtained from
 *             gsi_alloc_channel
 * @notify:    Information about the completed transfer if any
 * @expected_num:  Number of descriptor we want to poll each time.
 * @actual_num:    Actual number of descriptor we polled successfully.
 *
 * @Return gsi_status (GSI_STATUS_POLL_EMPTY is returned if no transfers
 * completed)
 */
int gsi_poll_n_channel(unsigned long chan_hdl,
		struct gsi_chan_xfer_notify *notify,
		int expected_num, int *actual_num);


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
 * @per_base_addr: Base address of the peripheral using GSI
 * @ver: GSI core version
 *
 * @Return gsi_status
 */
int gsi_configure_regs(phys_addr_t per_base_addr, enum gsi_ver ver);

/**
 * gsi_enable_fw - Peripheral should call this function
 * to enable the GSI FW after the FW has been loaded to the SRAM.
 *
 * @gsi_base_addr: Base address of GSI register space
 * @gsi_size: Mapping size of the GSI register space
 * @ver: GSI core version

 * @Return gsi_status
 */
int gsi_enable_fw(phys_addr_t gsi_base_addr, u32 gsi_size, enum gsi_ver ver);

/**
 * gsi_get_inst_ram_offset_and_size - Peripheral should call this function
 * to get instruction RAM base address offset and size. Peripheral typically
 * uses this info to load GSI FW into the IRAM.
 *
 * @base_offset:[OUT] - IRAM base offset address
 * @size:	[OUT] - IRAM size
 * @ver: GSI core version

 * @Return none
 */
void gsi_get_inst_ram_offset_and_size(unsigned long *base_offset,
		unsigned long *size, enum gsi_ver ver);

/**
 * gsi_halt_channel_ee - Peripheral should call this function
 * to stop other EE's channel. This is usually used in SSR clean
 *
 * @chan_idx: Virtual channel index
 * @ee: EE
 * @code: [out] response code for operation

 * @Return gsi_status
 */
int gsi_halt_channel_ee(unsigned int chan_idx, unsigned int ee, int *code);

/**
 * gsi_wdi3_write_evt_ring_db - write event ring doorbell address
 *
 * @chan_hdl: gsi channel handle
 * @Return gsi_status
 */
void gsi_wdi3_write_evt_ring_db(unsigned long chan_hdl, uint32_t db_addr_low,
	uint32_t db_addr_high);

/**
 * gsi_wdi3_dump_register - dump wdi3 related gsi registers
 *
 * @chan_hdl: gsi channel handle
 */
void gsi_wdi3_dump_register(unsigned long chan_hdl);


/**
 * gsi_map_base - Peripheral should call this function to configure
 * access to the GSI registers.

 * @gsi_base_addr: Base address of GSI register space
 * @gsi_size: Mapping size of the GSI register space
 *
 * @Return gsi_status
 */
int gsi_map_base(phys_addr_t gsi_base_addr, u32 gsi_size);

/**
 * gsi_unmap_base - Peripheral should call this function to undo the
 * effects of gsi_map_base
 *
 * @Return gsi_status
 */
int gsi_unmap_base(void);

/**
 * gsi_map_virtual_ch_to_per_ep - Peripheral should call this function
 * to configure each GSI virtual channel with the per endpoint index.
 *
 * @ee: The ee to be used
 * @chan_num: The channel to be used
 * @per_ep_index: value to assign
 *
 * @Return gsi_status
 */
int gsi_map_virtual_ch_to_per_ep(u32 ee, u32 chan_num, u32 per_ep_index);

/**
 * gsi_alloc_channel_ee - Peripheral should call this function
 * to alloc other EE's channel. This is usually done in bootup to allocate all
 * chnnels.
 *
 * @chan_idx: Virtual channel index
 * @ee: EE
 * @code: [out] response code for operation

 * @Return gsi_status
 */
int gsi_alloc_channel_ee(unsigned int chan_idx, unsigned int ee, int *code);


int gsi_chk_intset_value(void);

/**
 * gsi_enable_flow_control_ee - Peripheral should call this function
 * to enable flow control other EE's channel. This is usually done in USB
 * connent and SSR scenarios.
 *
 * @chan_idx: Virtual channel index
 * @ee: EE
 * @code: [out] response code for operation

 * @Return gsi_status
 */
int gsi_enable_flow_control_ee(unsigned int chan_idx, unsigned int ee,
								int *code);

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
 * gsi_read_channel_scratch
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

static inline int gsi_is_mcs_enabled(void)
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

static inline int gsi_ring_evt_ring_db(unsigned long evt_ring_hdl,
		uint64_t value)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_ring_ch_ring_db(unsigned long chan_hdl, uint64_t value)
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
static inline int gsi_write_channel_scratch3_reg(unsigned long chan_hdl,
		union __packed gsi_wdi_channel_scratch3_reg val)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_write_channel_scratch2_reg(unsigned long chan_hdl,
		union __packed gsi_wdi2_channel_scratch2_reg val)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_read_channel_scratch(unsigned long chan_hdl,
		union __packed gsi_channel_scratch *val)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_update_mhi_channel_scratch(unsigned long chan_hdl,
		struct __packed gsi_mhi_channel_scratch mscr)
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

static inline int gsi_poll_n_channel(unsigned long chan_hdl,
		struct gsi_chan_xfer_notify *notify,
		int expected_num, int *actual_num)
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

static inline int gsi_configure_regs(
	phys_addr_t per_base_addr, enum gsi_ver ver)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_enable_fw(
	phys_addr_t gsi_base_addr, u32 gsi_size, enum gsi_ver ver)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline void gsi_get_inst_ram_offset_and_size(unsigned long *base_offset,
		unsigned long *size, enum gsi_ver ver)
{
}

static inline int gsi_halt_channel_ee(unsigned int chan_idx, unsigned int ee,
	 int *code)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_map_base(phys_addr_t gsi_base_addr, u32 gsi_size)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_unmap_base(void)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_map_virtual_ch_to_per_ep(
	u32 ee, u32 chan_num, u32 per_ep_index)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_alloc_channel_ee(unsigned int chan_idx, unsigned int ee,
	int *code)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_chk_intset_value(void)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline int gsi_enable_flow_control_ee(unsigned int chan_idx,
			unsigned int ee, int *code)
{
	return -GSI_STATUS_UNSUPPORTED_OP;
}

static inline void gsi_wdi3_write_evt_ring_db(
	unsigned long chan_hdl, uint32_t db_addr_low,
	uint32_t db_addr_high)
{
}

static inline void gsi_wdi3_dump_register(unsigned long chan_hdl)
{
}

#endif
#endif
