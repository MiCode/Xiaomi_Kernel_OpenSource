/* Copyright (c) 2011-2015, The Linux Foundation. All rights reserved.
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

#ifndef _LINUX_SLIMBUS_H
#define _LINUX_SLIMBUS_H
#include <linux/module.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/mod_devicetable.h>

/* Interfaces between SLIMbus manager drivers and SLIMbus infrastructure. */

extern struct bus_type slimbus_type;

/* Standard values per SLIMbus spec needed by controllers and devices */
#define SLIM_CL_PER_SUPERFRAME		6144
#define SLIM_CL_PER_SUPERFRAME_DIV8	(SLIM_CL_PER_SUPERFRAME >> 3)
#define SLIM_MAX_TXNS			256
#define SLIM_MAX_CLK_GEAR		10
#define SLIM_MIN_CLK_GEAR		1
#define SLIM_CL_PER_SL			4
#define SLIM_SL_PER_SUPERFRAME		(SLIM_CL_PER_SUPERFRAME >> 2)
#define SLIM_FRM_SLOTS_PER_SUPERFRAME	16
#define SLIM_GDE_SLOTS_PER_SUPERFRAME	2

/*
 * SLIMbus message types. Related to interpretation of message code.
 * Values are defined in Table 32 (slimbus spec 1.01.01)
 */
#define SLIM_MSG_MT_CORE			0x0
#define SLIM_MSG_MT_DEST_REFERRED_CLASS		0x1
#define SLIM_MSG_MT_DEST_REFERRED_USER		0x2
#define SLIM_MSG_MT_SRC_REFERRED_CLASS		0x5
#define SLIM_MSG_MT_SRC_REFERRED_USER		0x6

/*
 * SLIMbus core type Message Codes.
 * Values are defined in Table 65 (slimbus spec 1.01.01)
 */
/* Device management messages */
#define SLIM_MSG_MC_REPORT_PRESENT               0x1
#define SLIM_MSG_MC_ASSIGN_LOGICAL_ADDRESS       0x2
#define SLIM_MSG_MC_RESET_DEVICE                 0x4
#define SLIM_MSG_MC_CHANGE_LOGICAL_ADDRESS       0x8
#define SLIM_MSG_MC_CHANGE_ARBITRATION_PRIORITY  0x9
#define SLIM_MSG_MC_REQUEST_SELF_ANNOUNCEMENT    0xC
#define SLIM_MSG_MC_REPORT_ABSENT                0xF

/* Data channel management messages */
#define SLIM_MSG_MC_CONNECT_SOURCE               0x10
#define SLIM_MSG_MC_CONNECT_SINK                 0x11
#define SLIM_MSG_MC_DISCONNECT_PORT              0x14
#define SLIM_MSG_MC_CHANGE_CONTENT               0x18

/* Information management messages */
#define SLIM_MSG_MC_REQUEST_INFORMATION          0x20
#define SLIM_MSG_MC_REQUEST_CLEAR_INFORMATION    0x21
#define SLIM_MSG_MC_REPLY_INFORMATION            0x24
#define SLIM_MSG_MC_CLEAR_INFORMATION            0x28
#define SLIM_MSG_MC_REPORT_INFORMATION           0x29

/* Reconfiguration messages */
#define SLIM_MSG_MC_BEGIN_RECONFIGURATION        0x40
#define SLIM_MSG_MC_NEXT_ACTIVE_FRAMER           0x44
#define SLIM_MSG_MC_NEXT_SUBFRAME_MODE           0x45
#define SLIM_MSG_MC_NEXT_CLOCK_GEAR              0x46
#define SLIM_MSG_MC_NEXT_ROOT_FREQUENCY          0x47
#define SLIM_MSG_MC_NEXT_PAUSE_CLOCK             0x4A
#define SLIM_MSG_MC_NEXT_RESET_BUS               0x4B
#define SLIM_MSG_MC_NEXT_SHUTDOWN_BUS            0x4C
#define SLIM_MSG_MC_NEXT_DEFINE_CHANNEL          0x50
#define SLIM_MSG_MC_NEXT_DEFINE_CONTENT          0x51
#define SLIM_MSG_MC_NEXT_ACTIVATE_CHANNEL        0x54
#define SLIM_MSG_MC_NEXT_DEACTIVATE_CHANNEL      0x55
#define SLIM_MSG_MC_NEXT_REMOVE_CHANNEL          0x58
#define SLIM_MSG_MC_RECONFIGURE_NOW              0x5F

/*
 * Clock pause flag to indicate that the reconfig message
 * corresponds to clock pause sequence
 */
#define SLIM_MSG_CLK_PAUSE_SEQ_FLG		(1U << 8)

/* Value management messages */
#define SLIM_MSG_MC_REQUEST_VALUE                0x60
#define SLIM_MSG_MC_REQUEST_CHANGE_VALUE         0x61
#define SLIM_MSG_MC_REPLY_VALUE                  0x64
#define SLIM_MSG_MC_CHANGE_VALUE                 0x68

/* Clock pause values defined in Table 66 (slimbus spec 1.01.01) */
#define SLIM_CLK_FAST				0
#define SLIM_CLK_CONST_PHASE			1
#define SLIM_CLK_UNSPECIFIED			2

struct slim_controller;
struct slim_device;

/* Destination type Values defined in Table 33 (slimbus spec 1.01.01) */
#define SLIM_MSG_DEST_LOGICALADDR	0
#define SLIM_MSG_DEST_ENUMADDR		1
#define	SLIM_MSG_DEST_BROADCAST		3

/*
 * @start_offset: Specifies starting offset in information/value element map
 * @num_bytes: Can be 1, 2, 3, 4, 6, 8, 12, 16 per spec. This ensures that the
 *	message will fit in the 40-byte message limit and the slicesize can be
 *	compatible with values in table 21 (slimbus spec 1.01.01)
 * @comp: Completion to indicate end of message-transfer. Used if client wishes
 *	to use the API asynchronously.
 */
struct slim_ele_access {
	u16			start_offset;
	u8			num_bytes;
	struct completion	*comp;
};

/*
 * struct slim_framer - Represents Slimbus framer.
 * Every controller may have multiple framers.
 * Manager is responsible for framer hand-over.
 * @e_addr: 6 byte Elemental address of the framer.
 * @rootfreq: Root Frequency at which the framer can run. This is maximum
 *	frequency (clock gear 10 per slimbus spec) at which the bus can operate.
 * @superfreq: Superframes per root frequency. Every frame is 6144 cells (bits)
 *	per slimbus specification.
 */
struct slim_framer {
	u8	e_addr[6];
	int	rootfreq;
	int	superfreq;
};
#define to_slim_framer(d) container_of(d, struct slim_framer, dev);

/*
 * struct slim_addrt: slimbus address used internally by the slimbus framework.
 * @valid: If the device is still there or if the address can be reused.
 * @eaddr: 6-bytes-long elemental address
 * @laddr: It is possible that controller will set a predefined logical address
 *	rather than the one assigned by framework. (i.e. logical address may
 *	not be same as index into this table). This entry will store the
 *	logical address value for this enumeration address.
 */
struct slim_addrt {
	bool	valid;
	u8	eaddr[6];
	u8	laddr;
};

/*
 * struct slim_val_inf: slimbus value/information element transaction
 * @start_offset: Specifies starting offset in information/value element map
 * @num_bytes: number of bytes to be read/written
 * @wbuf: buffer if this transaction has 'write' component in it
 * @rbuf: buffer if this transaction has 'read' component in it
 */
struct slim_val_inf {
	u16 start_offset;
	u8 num_bytes;
	u8 *wbuf;
	u8 *rbuf;
};

/*
 * struct slim_msg_txn: Message to be sent by the controller.
 * Linux framework uses this structure with drivers implementing controller.
 * This structure has packet header, payload and buffer to be filled (if any)
 * For the header information, refer to Table 34-36.
 * @rl: Header field. remaining length.
 * @mt: Header field. Message type.
 * @mc: Header field. LSB is message code for type mt. Framework will set MSB to
 *	SLIM_MSG_CLK_PAUSE_SEQ_FLG in case "mc" in the reconfiguration sequence
 *	is for pausing the clock.
 * @dt: Header field. Destination type.
 * @ec: Element size. Used for elemental access APIs.
 * @len: Length of payload. (excludes ec)
 * @tid: Transaction ID. Used for messages expecting response.
 *	(e.g. relevant for mc = SLIM_MSG_MC_REQUEST_INFORMATION)
 * @la: Logical address of the device this message is going to.
 *	(Not used when destination type is broadcast.)
 * @async: If this transaction is async
 * @rbuf: Buffer to be populated by controller when response is received.
 * @wbuf: Payload of the message. (e.g. channel number for DATA channel APIs)
 * @comp: Completion structure. Used by controller to notify response.
 *	(Field is relevant when tid is used)
 */
struct slim_msg_txn {
	u8			rl;
	u8			mt;
	u16			mc;
	u8			dt;
	u16			ec;
	u8			len;
	u8			tid;
	u8			la;
	bool			async;
	u8			*rbuf;
	const u8		*wbuf;
	struct completion	*comp;
};

/* Internal port state used by slimbus framework to manage data-ports */
enum slim_port_state {
	SLIM_P_FREE,
	SLIM_P_UNCFG,
	SLIM_P_CFG,
};

/*
 * enum slim_port_req: Request port type by user through APIs to manage ports
 * User can request default, half-duplex or port to be used in multi-channel
 * configuration. Default indicates a simplex port.
 */
enum slim_port_req {
	SLIM_REQ_DEFAULT,
	SLIM_REQ_HALF_DUP,
	SLIM_REQ_MULTI_CH,
};

/*
 * enum slim_port_opts: Port options requested.
 * User can request no configuration, packed data, and/or MSB aligned data port
 */
enum slim_port_opts {
	SLIM_OPT_NONE = 0,
	SLIM_OPT_NO_PACK = 1U,
	SLIM_OPT_ALIGN_MSB = 1U << 1,
};

/* enum slim_port_flow: Port flow type (inbound/outbound). */
enum slim_port_flow {
	SLIM_SRC,
	SLIM_SINK,
};

/* enum slim_port_err: Port errors */
enum slim_port_err {
	SLIM_P_INPROGRESS,
	SLIM_P_OVERFLOW,
	SLIM_P_UNDERFLOW,
	SLIM_P_DISCONNECT,
	SLIM_P_NOT_OWNED,
};

/*
 * struct slim_port_cfg: Port config for the manager port
 * port_opts: port options (bit-map) for this port
 * watermark: watermark level set for this port
 */
struct slim_port_cfg {
	u32 port_opts;
	u32 watermark;
};

/*
 * struct slim_port: Internal structure used by framework to manage ports
 * @err: Port error if any for this port. Refer to enum above.
 * @state: Port state. Refer to enum above.
 * @req: Port request for this port.
 * @cfg: Port configuration for this port.
 * @flow: Flow type of this port.
 * @ch: Channel association of this port.
 * @xcomp: Completion to indicate error, data transfer done event.
 * @ctrl: Controller to which this port belongs to. This is useful to associate
 *	port with the SW since port hardware interrupts may only contain port
 *	information.
 */
struct slim_port {
	enum slim_port_err	err;
	enum slim_port_state	state;
	enum slim_port_req	req;
	struct slim_port_cfg	cfg;
	enum slim_port_flow	flow;
	struct slim_ch		*ch;
	struct completion	*xcomp;
	struct slim_controller	*ctrl;
};

/*
 * enum slim_ch_state: Channel state of a channel.
 * Channel transition happens from free-to-allocated-to-defined-to-pending-
 * active-to-active.
 * Once active, channel can be removed or suspended. Suspended channels are
 * still scheduled, but data transfer doesn't happen.
 * Removed channels are not deallocated until dealloc_ch API is used.
 * Deallocation reset channel state back to free.
 * Removed channels can be defined with different parameters.
 */
enum slim_ch_state {
	SLIM_CH_FREE,
	SLIM_CH_ALLOCATED,
	SLIM_CH_DEFINED,
	SLIM_CH_PENDING_ACTIVE,
	SLIM_CH_ACTIVE,
	SLIM_CH_SUSPENDED,
	SLIM_CH_PENDING_REMOVAL,
};

/*
 * enum slim_ch_proto: Channel protocol used by the channel.
 * Hard Isochronous channel is not scheduled if current frequency doesn't allow
 * the channel to be run without flow-control.
 * Auto isochronous channel will be scheduled as hard-isochronous or push-pull
 * depending on current bus frequency.
 * Currently, Push-pull or async or extended channels are not supported.
 * For more details, refer to slimbus spec
 */
enum slim_ch_proto {
	SLIM_HARD_ISO,
	SLIM_AUTO_ISO,
	SLIM_PUSH,
	SLIM_PULL,
	SLIM_ASYNC_SMPLX,
	SLIM_ASYNC_HALF_DUP,
	SLIM_EXT_SMPLX,
	SLIM_EXT_HALF_DUP,
};

/*
 * enum slim_ch_rate: Most commonly used frequency rate families.
 * Use 1HZ for push-pull transport.
 * 4KHz and 11.025KHz are most commonly used in audio applications.
 * Typically, slimbus runs at frequencies to support channels running at 4KHz
 * and/or 11.025KHz isochronously.
 */
enum slim_ch_rate {
	SLIM_RATE_1HZ,
	SLIM_RATE_4000HZ,
	SLIM_RATE_11025HZ,
};

/*
 * enum slim_ch_coeff: Coefficient of a channel used internally by framework.
 * Coefficient is applicable to channels running isochronously.
 * Coefficient is calculated based on channel rate multiplier.
 * (If rate multiplier is power of 2, it's coeff.1 channel. Otherwise it's
 * coeff.3 channel.
 */
enum slim_ch_coeff {
	SLIM_COEFF_1,
	SLIM_COEFF_3,
};

/*
 * enum slim_ch_control: Channel control.
 * Activate will schedule channel and/or group of channels in the TDM frame.
 * Suspend will keep the schedule but data-transfer won't happen.
 * Remove will remove the channel/group from the TDM frame.
 */
enum slim_ch_control {
	SLIM_CH_ACTIVATE,
	SLIM_CH_SUSPEND,
	SLIM_CH_REMOVE,
};

/* enum slim_ch_dataf: Data format per table 60 from slimbus spec 1.01.01 */
enum slim_ch_dataf {
	SLIM_CH_DATAF_NOT_DEFINED = 0,
	SLIM_CH_DATAF_LPCM_AUDIO = 1,
	SLIM_CH_DATAF_IEC61937_COMP_AUDIO = 2,
	SLIM_CH_DATAF_PACKED_PDM_AUDIO = 3,
};

/* enum slim_ch_auxf: Auxiliary field format per table 59 from slimbus spec */
enum slim_ch_auxf {
	SLIM_CH_AUXF_NOT_APPLICABLE = 0,
	SLIM_CH_AUXF_ZCUV_TUNNEL_IEC60958 = 1,
	SLIM_CH_USER_DEFINED = 0xF,
};

/*
 * struct slim_ch: Channel structure used externally by users of channel APIs.
 * @prot: Desired slimbus protocol.
 * @baser: Desired base rate. (Typical isochronous rates are: 4KHz, or 11.025KHz
 * @dataf: Data format.
 * @auxf: Auxiliary format.
 * @ratem: Channel rate multiplier. (e.g. 48KHz channel will have 4KHz base rate
 *	and 12 as rate multiplier.
 * @sampleszbits: Sample size in bits.
 */
struct slim_ch {
	enum slim_ch_proto	prot;
	enum slim_ch_rate	baser;
	enum slim_ch_dataf	dataf;
	enum slim_ch_auxf	auxf;
	u32			ratem;
	u32			sampleszbits;
};

/*
 * struct slim_ich: Internal channel structure used by slimbus framework.
 * @prop: structure passed by the client.
 * @coeff: Coefficient of this channel.
 * @state: Current state of the channel.
 * @nextgrp: If this channel is part of group, next channel in this group.
 * @prrate: Presence rate of this channel (per table 62 of the spec)
 * @offset: Offset of this channel in the superframe.
 * @newoff: Used during scheduling to hold temporary new offset until the offset
 *	is accepted/rejected by slimbus reconfiguration.
 * @interval: Interval of this channel per superframe.
 * @newintr: Used during scheduling to new interval temporarily.
 * @seglen: Segment length of this channel.
 * @rootexp: root exponent of this channel. Rate can be found using rootexp and
 *	coefficient. Used during scheduling.
 * @srch: Source port used by this channel.
 * @sinkh: Sink ports used by this channel.
 * @nsink: number of sink ports used by this channel.
 * @chan: Channel number sent on hardware lines for this channel. May not be
 *	equal to array-index into chans if client requested to use number beyond
 *	channel-array for the controller.
 * @ref: Reference number to keep track of how many clients (upto 2) are using
 *	this channel.
 * @def: Used to keep track of how many times the channel definition is sent
 *	to hardware and this will decide if channel-remove can be sent for the
 *	channel. Channel definition may be sent upto twice (once per producer
 *	and once per consumer). Channel removal should be sent only once to
 *	avoid clients getting underflow/overflow errors.
 */
struct slim_ich {
	struct slim_ch		prop;
	enum slim_ch_coeff	coeff;
	enum slim_ch_state	state;
	u16			nextgrp;
	u32			prrate;
	u32			offset;
	u32			newoff;
	u32			interval;
	u32			newintr;
	u32			seglen;
	u8			rootexp;
	u32			srch;
	u32			*sinkh;
	int			nsink;
	u8			chan;
	int			ref;
	int			def;
};

/*
 * struct slim_sched: Framework uses this structure internally for scheduling.
 * @chc3: Array of all active coeffient 3 channels.
 * @num_cc3: Number of active coeffient 3 channels.
 * @chc1: Array of all active coeffient 1 channels.
 * @num_cc1: Number of active coeffient 1 channels.
 * @subfrmcode: Current subframe-code used by TDM. This is decided based on
 *	requested message bandwidth and current channels scheduled.
 * @usedslots: Slots used by all active channels.
 * @msgsl: Slots used by message-bandwidth.
 * @pending_msgsl: Used to store pending request of message bandwidth (in slots)
 *	until the scheduling is accepted by reconfiguration.
 * @m_reconf: This mutex is held until current reconfiguration (data channel
 *	scheduling, message bandwidth reservation) is done. Message APIs can
 *	use the bus concurrently when this mutex is held since elemental access
 *	messages can be sent on the bus when reconfiguration is in progress.
 * @slots: Used for debugging purposes to debug/verify current schedule in TDM.
 */
struct slim_sched {
	struct slim_ich	**chc3;
	int		num_cc3;
	struct slim_ich	**chc1;
	int		num_cc1;
	u32		subfrmcode;
	u32		usedslots;
	u32		msgsl;
	u32		pending_msgsl;
	struct mutex	m_reconf;
	u8		*slots;
};

/*
 * enum slim_clk_state: Slimbus controller's clock state used internally for
 *	maintaining current clock state.
 * @SLIM_CLK_ACTIVE: Slimbus clock is active
 * @SLIM_CLK_PAUSE_FAILED: Slimbus controlled failed to go in clock pause.
 *	Hardware-wise, this state is same as active but controller will wait on
 *	completion before making transition to SLIM_CLK_ACTIVE in framework
 * @SLIM_CLK_ENTERING_PAUSE: Slimbus clock pause sequence is being sent on the
 *	bus. If this succeeds, state changes to SLIM_CLK_PAUSED. If the
 *	transition fails, state changes to SLIM_CLK_PAUSE_FAILED
 * @SLIM_CLK_PAUSED: Slimbus controller clock has paused.
 */
enum slim_clk_state {
	SLIM_CLK_ACTIVE,
	SLIM_CLK_ENTERING_PAUSE,
	SLIM_CLK_PAUSE_FAILED,
	SLIM_CLK_PAUSED,
};
/*
 * struct slim_controller: Represents manager for a SlimBUS
 *				(similar to 'master' on I2C)
 * @dev: Device interface to this driver
 * @nr: Board-specific number identifier for this controller/bus
 * @list: Link with other slimbus controllers
 * @name: Name for this controller
 * @clkgear: Current clock gear in which this bus is running
 * @min_cg: Minimum clock gear supported by this controller (default value: 1)
 * @max_cg: Maximum clock gear supported by this controller (default value: 10)
 * @clk_state: Controller's clock state from enum slim_clk_state
 * @pause_comp: Signals completion of clock pause sequence. This is useful when
 *	client tries to call slimbus transaction when controller may be entering
 *	clock pause.
 * @a_framer: Active framer which is clocking the bus managed by this controller
 * @m_ctrl: Mutex protecting controller data structures (ports, channels etc)
 * @addrt: Logical address table
 * @num_dev: Number of active slimbus slaves on this bus
 * @devs: List of devices on this controller
 * @wq: Workqueue per controller used to notify devices when they report present
 * @txnt: Table of transactions having transaction ID
 * @last_tid: size of the table txnt (can't grow beyond 256 since TID is 8-bits)
 * @ports: Ports associated with this controller
 * @nports: Number of ports supported by the controller
 * @chans: Channels associated with this controller
 * @nchans: Number of channels supported
 * @reserved: Reserved channels that controller wants to use internally
 *		Clients will be assigned channel numbers after this number
 * @sched: scheduler structure used by the controller
 * @dev_released: completion used to signal when sysfs has released this
 *	controller so that it can be deleted during shutdown
 * @xfer_msg: Transfer a message on this controller (this can be a broadcast
 *	control/status message like data channel setup, or a unicast message
 *	like value element read/write.
 * @set_laddr: Setup logical address at laddr for the slave with elemental
 *	address e_addr. Drivers implementing controller will be expected to
 *	send unicast message to this device with its logical address.
 * @allocbw: Controller can override default reconfiguration and channel
 *	scheduling algorithm.
 * @get_laddr: It is possible that controller needs to set fixed logical
 *	address table and get_laddr can be used in that case so that controller
 *	can do this assignment.
 * @wakeup: This function pointer implements controller-specific procedure
 *	to wake it up from clock-pause. Framework will call this to bring
 *	the controller out of clock pause.
 * @alloc_port: Allocate a port and make it ready for data transfer. This is
 *	called by framework to make sure controller can take necessary steps
 *	to initialize its port
 * @dealloc_port: Counter-part of alloc_port. This is called by framework so
 *	that controller can free resources associated with this port
 * @framer_handover: If this controller has multiple framers, this API will
 *	be called to switch between framers if controller desires to change
 *	the active framer.
 * @port_xfer: Called to schedule a transfer on port pn. iobuf is physical
 *	address and the buffer may have to be DMA friendly since data channels
 *	will be using data from this buffers without SW intervention.
 * @port_xfer_status: Called by framework when client calls get_xfer_status
 *	API. Returns how much buffer is actually processed and the port
 *	errors (e.g. overflow/underflow) if any.
 * @xfer_user_msg: Send user message to specified logical address. Underlying
 *	controller has to support sending user messages. Returns error if any.
 * @xfer_bulk_wr: Send bulk of write messages to specified logical address.
 *	Underlying controller has to support this. Typically useful to transfer
 *	messages to download firmware, or messages where strict ordering for
 *	slave is necessary
 */
struct slim_controller {
	struct device		dev;
	unsigned int		nr;
	struct list_head	list;
	char			name[SLIMBUS_NAME_SIZE];
	int			clkgear;
	int			min_cg;
	int			max_cg;
	enum slim_clk_state	clk_state;
	struct completion	pause_comp;
	struct slim_framer	*a_framer;
	struct mutex		m_ctrl;
	struct slim_addrt	*addrt;
	u8			num_dev;
	struct list_head	devs;
	struct workqueue_struct *wq;
	struct slim_msg_txn	*txnt[SLIM_MAX_TXNS];
	u8			last_tid;
	spinlock_t		txn_lock;
	struct slim_port	*ports;
	int			nports;
	struct slim_ich		*chans;
	int			nchans;
	u8			reserved;
	struct slim_sched	sched;
	struct completion	dev_released;
	int			(*xfer_msg)(struct slim_controller *ctrl,
				struct slim_msg_txn *txn);
	int			(*set_laddr)(struct slim_controller *ctrl,
				const u8 *ea, u8 elen, u8 laddr);
	int			(*allocbw)(struct slim_device *sb,
				int *subfrmc, int *clkgear);
	int			(*get_laddr)(struct slim_controller *ctrl,
				const u8 *ea, u8 elen, u8 *laddr);
	int			(*wakeup)(struct slim_controller *ctrl);
	int			(*alloc_port)(struct slim_controller *ctrl,
				u8 port);
	void			(*dealloc_port)(struct slim_controller *ctrl,
				u8 port);
	int			(*framer_handover)(struct slim_controller *ctrl,
				struct slim_framer *new_framer);
	int			(*port_xfer)(struct slim_controller *ctrl,
				u8 pn, phys_addr_t iobuf, u32 len,
				struct completion *comp);
	enum slim_port_err	(*port_xfer_status)(struct slim_controller *ctr,
				u8 pn, phys_addr_t *done_buf, u32 *done_len);
	int			(*xfer_user_msg)(struct slim_controller *ctrl,
				u8 la, u8 mt, u8 mc,
				struct slim_ele_access *msg, u8 *buf, u8 len);
	int (*xfer_bulk_wr)(struct slim_controller *ctrl,
			u8 la, u8 mt, u8 mc, struct slim_val_inf msgs[],
			int n, int (*comp_cb)(void *ctx, int err),
			void *ctx);
};
#define to_slim_controller(d) container_of(d, struct slim_controller, dev)

/*
 * struct slim_driver: Manage Slimbus generic/slave device driver
 * @probe: Binds this driver to a slimbus device.
 * @remove: Unbinds this driver from the slimbus device.
 * @shutdown: Standard shutdown callback used during powerdown/halt.
 * @suspend: Standard suspend callback used during system suspend
 * @resume: Standard resume callback used during system resume
 * @device_up: This callback is called when the device reports present and
 *		gets a logical address assigned to it
 * @device_down: This callback is called when device reports absent, or the
 *		bus goes down. Device will report present when bus is up and
 *		device_up callback will be called again when that happens
 * @reset_device: This callback is called after framer is booted.
 *		Driver should do the needful to reset the device,
 *		so that device acquires sync and be operational.
 * @driver: Slimbus device drivers should initialize name and owner field of
 *	this structure
 * @id_table: List of slimbus devices supported by this driver
 */
struct slim_driver {
	int				(*probe)(struct slim_device *sldev);
	int				(*remove)(struct slim_device *sldev);
	void				(*shutdown)(struct slim_device *sldev);
	int				(*suspend)(struct slim_device *sldev,
					pm_message_t pmesg);
	int				(*resume)(struct slim_device *sldev);
	int				(*device_up)(struct slim_device *sldev);
	int				(*device_down)
						(struct slim_device *sldev);
	int				(*reset_device)
						(struct slim_device *sldev);

	struct device_driver		driver;
	const struct slim_device_id	*id_table;
};
#define to_slim_driver(d) container_of(d, struct slim_driver, driver)

/*
 * struct slim_pending_ch: List of pending channels used by framework.
 * @chan: Channel number
 * @pending: list of channels
 */
struct slim_pending_ch {
	u8	chan;
	struct	list_head pending;
};

/*
 * Client/device handle (struct slim_device):
 * ------------------------------------------
 *  This is the client/device handle returned when a slimbus
 *  device is registered with a controller. This structure can be provided
 *  during register_board_info, or can be allocated using slim_add_device API.
 *  Pointer to this structure is used by client-driver as a handle.
 *  @dev: Driver model representation of the device.
 *  @name: Name of driver to use with this device.
 *  @e_addr: 6-byte elemental address of this device.
 *  @driver: Device's driver. Pointer to access routines.
 *  @ctrl: Slimbus controller managing the bus hosting this device.
 *  @laddr: 1-byte Logical address of this device.
 *  @reported: Flag to indicate whether this device reported present. The flag
 *	is set when device reports present, and is reset when it reports
 *	absent. This flag alongwith notified flag below is used to call
 *	device_up, or device_down callbacks for driver of this device.
 *  @mark_define: List of channels pending definition/activation.
 *  @mark_suspend: List of channels pending suspend.
 *  @mark_removal: List of channels pending removal.
 *  @notified: Flag to indicate whether this device has been notified. The
 *	device may report present multiple times, but should be notified only
 *	first time it has reported present.
 *  @dev_list: List of devices on a controller
 *  @wd: Work structure associated with workqueue for presence notification
 *  @sldev_reconf: Mutex to protect the pending data-channel lists.
 *  @pending_msgsl: Message bandwidth reservation request by this client in
 *	slots that's pending reconfiguration.
 *  @cur_msgsl: Message bandwidth reserved by this client in slots.
 *  These 3 lists are managed by framework. Lists are populated when client
 *  calls channel control API without reconfig-flag set and the lists are
 *  emptied when the reconfiguration is done by this client.
 */
struct slim_device {
	struct device		dev;
	const char		*name;
	u8			e_addr[6];
	struct slim_driver	*driver;
	struct slim_controller	*ctrl;
	u8			laddr;
	bool			reported;
	struct list_head	mark_define;
	struct list_head	mark_suspend;
	struct list_head	mark_removal;
	bool			notified;
	struct list_head	dev_list;
	struct work_struct	wd;
	struct mutex		sldev_reconf;
	u32			pending_msgsl;
	u32			cur_msgsl;
};
#define to_slim_device(d) container_of(d, struct slim_device, dev)

/*
 * struct slim_boardinfo: Declare board info for Slimbus device bringup.
 * @bus_num: Controller number (bus) on which this device will sit.
 * @slim_slave: Device to be registered with slimbus.
 */
struct slim_boardinfo {
	int			bus_num;
	struct slim_device	*slim_slave;
};

/*
 * slim_get_logical_addr: Return the logical address of a slimbus device.
 * @sb: client handle requesting the adddress.
 * @e_addr: Elemental address of the device.
 * @e_len: Length of e_addr
 * @laddr: output buffer to store the address
 * context: can sleep
 * -EINVAL is returned in case of invalid parameters, and -ENXIO is returned if
 *  the device with this elemental address is not found.
 */

extern int slim_get_logical_addr(struct slim_device *sb, const u8 *e_addr,
					u8 e_len, u8 *laddr);


/* Message APIs Unicast message APIs used by slimbus slave drivers */

/*
 * Message API access routines.
 * @sb: client handle requesting elemental message reads, writes.
 * @msg: Input structure for start-offset, number of bytes to read.
 * @rbuf: data buffer to be filled with values read.
 * @len: data buffer size
 * @wbuf: data buffer containing value/information to be written
 * context: can sleep
 * Returns:
 * -EINVAL: Invalid parameters
 * -ETIMEDOUT: If controller could not complete the request. This may happen if
 *  the bus lines are not clocked, controller is not powered-on, slave with
 *  given address is not enumerated/responding.
 */
extern int slim_request_val_element(struct slim_device *sb,
					struct slim_ele_access *msg, u8 *buf,
					u8 len);
extern int slim_request_inf_element(struct slim_device *sb,
					struct slim_ele_access *msg, u8 *buf,
					u8 len);
extern int slim_change_val_element(struct slim_device *sb,
					struct slim_ele_access *msg,
					const u8 *buf, u8 len);
extern int slim_clear_inf_element(struct slim_device *sb,
					struct slim_ele_access *msg, u8 *buf,
					u8 len);
extern int slim_request_change_val_element(struct slim_device *sb,
					struct slim_ele_access *msg, u8 *rbuf,
					const u8 *wbuf, u8 len);
extern int slim_request_clear_inf_element(struct slim_device *sb,
					struct slim_ele_access *msg, u8 *rbuf,
					const u8 *wbuf, u8 len);

/*
 * Broadcast message API:
 * call this API directly with sbdev = NULL.
 * For broadcast reads, make sure that buffers are big-enough to incorporate
 * replies from all logical addresses.
 * All controllers may not support broadcast
 */
extern int slim_xfer_msg(struct slim_controller *ctrl,
			struct slim_device *sbdev, struct slim_ele_access *msg,
			u16 mc, u8 *rbuf, const u8 *wbuf, u8 len);

/*
 * User message:
 * slim_user_msg: Send user message that is interpreted by destination device
 * @sb: Client handle sending the message
 * @la: Destination device for this user message
 * @mt: Message Type (Soruce-referred, or Destination-referred)
 * @mc: Message Code
 * @msg: Message structure (start offset, number of bytes) to be sent
 * @buf: data buffer to be sent
 * @len: data buffer size in bytes
 */
extern int slim_user_msg(struct slim_device *sb, u8 la, u8 mt, u8 mc,
				struct slim_ele_access *msg, u8 *buf, u8 len);

/*
 * Queue bulk of message writes:
 * slim_bulk_msg_write: Write bulk of messages (e.g. downloading FW)
 * @sb: Client handle sending these messages
 * @la: Destination device for these messages
 * @mt: Message Type
 * @mc: Message Code
 * @msgs: List of messages to be written in bulk
 * @n: Number of messages in the list
 * @cb: Callback if client needs this to be non-blocking
 * @ctx: Context for this callback
 * If supported by controller, this message list will be sent in bulk to the HW
 * If the client specifies this to be non-blocking, the callback will be
 * called from atomic context.
 */
extern int slim_bulk_msg_write(struct slim_device *sb, u8 mt, u8 mc,
			struct slim_val_inf msgs[], int n,
			int (*comp_cb)(void *ctx, int err), void *ctx);
/* end of message apis */

/* Port management for manager device APIs */

/*
 * slim_alloc_mgrports: Allocate port on manager side.
 * @sb: device/client handle.
 * @req: Port request type.
 * @nports: Number of ports requested
 * @rh: output buffer to store the port handles
 * @hsz: size of buffer storing handles
 * context: can sleep
 * This port will be typically used by SW. e.g. client driver wants to receive
 * some data from audio codec HW using a data channel.
 * Port allocated using this API will be used to receive the data.
 * If half-duplex ports are requested, two adjacent ports are allocated for
 * 1 half-duplex port. So the handle-buffer size should be twice the number
 * of half-duplex ports to be allocated.
 * -EDQUOT is returned if all ports are in use.
 */
extern int slim_alloc_mgrports(struct slim_device *sb, enum slim_port_req req,
				int nports, u32 *rh, int hsz);

/* Deallocate the port(s) allocated using the API above */
extern int slim_dealloc_mgrports(struct slim_device *sb, u32 *hdl, int hsz);

/*
 * slim_config_mgrports: Configure manager side ports
 * @sb: device/client handle.
 * @ph: array of port handles for which this configuration is valid
 * @nports: Number of ports in ph
 * @cfg: configuration requested for port(s)
 * Configure port settings if they are different than the default ones.
 * Returns success if the config could be applied. Returns -EISCONN if the
 * port is in use
 */
extern int slim_config_mgrports(struct slim_device *sb, u32 *ph, int nports,
				struct slim_port_cfg *cfg);

/*
 * slim_port_xfer: Schedule buffer to be transferred/received using port-handle.
 * @sb: client handle
 * @ph: port-handle
 * @iobuf: buffer to be transferred or populated
 * @len: buffer size.
 * @comp: completion signal to indicate transfer done or error.
 * context: can sleep
 * Returns number of bytes transferred/received if used synchronously.
 * Will return 0 if used asynchronously.
 * Client will call slim_port_get_xfer_status to get error and/or number of
 * bytes transferred if used asynchronously.
 */
extern int slim_port_xfer(struct slim_device *sb, u32 ph, phys_addr_t iobuf,
				u32 len, struct completion *comp);

/*
 * slim_port_get_xfer_status: Poll for port transfers, or get transfer status
 *	after completion is done.
 * @sb: client handle
 * @ph: port-handle
 * @done_buf: return pointer (iobuf from slim_port_xfer) which is processed.
 * @done_len: Number of bytes transferred.
 * This can be called when port_xfer complition is signalled.
 * The API will return port transfer error (underflow/overflow/disconnect)
 * and/or done_len will reflect number of bytes transferred. Note that
 * done_len may be valid even if port error (overflow/underflow) has happened.
 * e.g. If the transfer was scheduled with a few bytes to be transferred and
 * client has not supplied more data to be transferred, done_len will indicate
 * number of bytes transferred with underflow error. To avoid frequent underflow
 * errors, multiple transfers can be queued (e.g. ping-pong buffers) so that
 * channel has data to be transferred even if client is not ready to transfer
 * data all the time. done_buf will indicate address of the last buffer
 * processed from the multiple transfers.
 */
extern enum slim_port_err slim_port_get_xfer_status(struct slim_device *sb,
			u32 ph, phys_addr_t *done_buf, u32 *done_len);

/*
 * slim_connect_src: Connect source port to channel.
 * @sb: client handle
 * @srch: source handle to be connected to this channel
 * @chanh: Channel with which the ports need to be associated with.
 * Per slimbus specification, a channel may have 1 source port.
 * Channel specified in chanh needs to be allocated first.
 * Returns -EALREADY if source is already configured for this channel.
 * Returns -ENOTCONN if channel is not allocated
 * Returns -EINVAL if invalid direction is specified for non-manager port,
 * or if the manager side port number is out of bounds, or in incorrect state
 */
extern int slim_connect_src(struct slim_device *sb, u32 srch, u16 chanh);

/*
 * slim_connect_sink: Connect sink port(s) to channel.
 * @sb: client handle
 * @sinkh: sink handle(s) to be connected to this channel
 * @nsink: number of sinks
 * @chanh: Channel with which the ports need to be associated with.
 * Per slimbus specification, a channel may have multiple sink-ports.
 * Channel specified in chanh needs to be allocated first.
 * Returns -EALREADY if sink is already configured for this channel.
 * Returns -ENOTCONN if channel is not allocated
 * Returns -EINVAL if invalid parameters are passed, or invalid direction is
 * specified for non-manager port, or if the manager side port number is out of
 * bounds, or in incorrect state
 */
extern int slim_connect_sink(struct slim_device *sb, u32 *sinkh, int nsink,
				u16 chanh);
/*
 * slim_disconnect_ports: Disconnect port(s) from channel
 * @sb: client handle
 * @ph: ports to be disconnected
 * @nph: number of ports.
 * Disconnects ports from a channel.
 */
extern int slim_disconnect_ports(struct slim_device *sb, u32 *ph, int nph);

/*
 * slim_get_slaveport: Get slave port handle
 * @la: slave device logical address.
 * @idx: port index at slave
 * @rh: return handle
 * @flw: Flow type (source or destination)
 * This API only returns a slave port's representation as expected by slimbus
 * driver. This port is not managed by the slimbus driver. Caller is expected
 * to have visibility of this port since it's a device-port.
 */
extern int slim_get_slaveport(u8 la, int idx, u32 *rh, enum slim_port_flow flw);


/* Channel functions. */

/*
 * slim_alloc_ch: Allocate a slimbus channel and return its handle.
 * @sb: client handle.
 * @chanh: return channel handle
 * Slimbus channels are limited to 256 per specification.
 * -EXFULL is returned if all channels are in use.
 * Although slimbus specification supports 256 channels, a controller may not
 * support that many channels.
 */
extern int slim_alloc_ch(struct slim_device *sb, u16 *chanh);

/*
 * slim_query_ch: Get reference-counted handle for a channel number. Every
 * channel is reference counted by one as producer and the others as
 * consumer)
 * @sb: client handle
 * @chan: slimbus channel number
 * @chanh: return channel handle
 * If request channel number is not in use, it is allocated, and reference
 * count is set to one. If the channel was was already allocated, this API
 * will return handle to that channel and reference count is incremented.
 * -EXFULL is returned if all channels are in use
 */
extern int slim_query_ch(struct slim_device *sb, u8 chan, u16 *chanh);
/*
 * slim_dealloc_ch: Deallocate channel allocated using the API above
 * -EISCONN is returned if the channel is tried to be deallocated without
 *  being removed first.
 *  -ENOTCONN is returned if deallocation is tried on a channel that's not
 *  allocated.
 */
extern int slim_dealloc_ch(struct slim_device *sb, u16 chanh);


/*
 * slim_define_ch: Define a channel.This API defines channel parameters for a
 *	given channel.
 * @sb: client handle.
 * @prop: slim_ch structure with channel parameters desired to be used.
 * @chanh: list of channels to be defined.
 * @nchan: number of channels in a group (1 if grp is false)
 * @grp: Are the channels grouped
 * @grph: return group handle if grouping of channels is desired.
 *	Channels can be grouped if multiple channels use same parameters
 *	(e.g. 5.1 audio has 6 channels with same parameters. They will all be
 *	grouped and given 1 handle for simplicity and avoid repeatedly calling
 *	the API)
 * -EISCONN is returned if channel is already used with different parameters.
 * -ENXIO is returned if the channel is not yet allocated.
 */
extern int slim_define_ch(struct slim_device *sb, struct slim_ch *prop,
				u16 *chanh, u8 nchan, bool grp, u16 *grph);

/*
 * slim_control_ch: Channel control API.
 * @sb: client handle
 * @grpchanh: group or channel handle to be controlled
 * @chctrl: Control command (activate/suspend/remove)
 * @commit: flag to indicate whether the control should take effect right-away.
 * This API activates, removes or suspends a channel (or group of channels)
 * grpchanh indicates the channel or group handle (returned by the define_ch
 * API). Reconfiguration may be time-consuming since it can change all other
 * active channel allocations on the bus, change in clock gear used by the
 * slimbus, and change in the control space width used for messaging.
 * commit makes sure that multiple channels can be activated/deactivated before
 * reconfiguration is started.
 * -EXFULL is returned if there is no space in TDM to reserve the bandwidth.
 * -EISCONN/-ENOTCONN is returned if the channel is already connected or not
 * yet defined.
 * -EINVAL is returned if individual control of a grouped-channel is attempted.
 */
extern int slim_control_ch(struct slim_device *sb, u16 grpchanh,
				enum slim_ch_control chctrl, bool commit);

/*
 * slim_get_ch_state: Channel state.
 * This API returns the channel's state (active, suspended, inactive etc)
 */
extern enum slim_ch_state slim_get_ch_state(struct slim_device *sb,
						u16 chanh);

/*
 * slim_reservemsg_bw: Request to reserve bandwidth for messages.
 * @sb: client handle
 * @bw_bps: message bandwidth in bits per second to be requested
 * @commit: indicates whether the reconfiguration needs to be acted upon.
 * This API call can be grouped with slim_control_ch API call with only one of
 * the APIs specifying the commit flag to avoid reconfiguration being called too
 * frequently. -EXFULL is returned if there is no space in TDM to reserve the
 * bandwidth. -EBUSY is returned if reconfiguration is requested, but a request
 * is already in progress.
 */
extern int slim_reservemsg_bw(struct slim_device *sb, u32 bw_bps, bool commit);

/*
 * slim_reconfigure_now: Request reconfiguration now.
 * @sb: client handle
 * This API does what commit flag in other scheduling APIs do.
 * -EXFULL is returned if there is no space in TDM to reserve the
 * bandwidth. -EBUSY is returned if reconfiguration request is already in
 * progress.
 */
extern int slim_reconfigure_now(struct slim_device *sb);

/*
 * slim_ctrl_clk_pause: Called by slimbus controller to request clock to be
 *	paused or woken up out of clock pause
 * @ctrl: controller requesting bus to be paused or woken up
 * @wakeup: Wakeup this controller from clock pause.
 * @restart: Restart time value per spec used for clock pause. This value
 *	isn't used when controller is to be woken up.
 * This API executes clock pause reconfiguration sequence if wakeup is false.
 * If wakeup is true, controller's wakeup is called
 * Slimbus clock is idle and can be disabled by the controller later.
 */
extern int slim_ctrl_clk_pause(struct slim_controller *ctrl, bool wakeup,
		u8 restart);

/*
 * slim_driver_register: Client driver registration with slimbus
 * @drv:Client driver to be associated with client-device.
 * This API will register the client driver with the slimbus
 * It is called from the driver's module-init function.
 */
extern int slim_driver_register(struct slim_driver *drv);

/*
 * slim_driver_unregister: Undo effects of slim_driver_register
 * @drv: Client driver to be unregistered
 */
extern void slim_driver_unregister(struct slim_driver *drv);

/*
 * slim_add_numbered_controller: Controller bring-up.
 * @ctrl: Controller to be registered.
 * A controller is registered with the framework using this API. ctrl->nr is the
 * desired number with which slimbus framework registers the controller.
 * Function will return -EBUSY if the number is in use.
 */
extern int slim_add_numbered_controller(struct slim_controller *ctrl);

/*
 * slim_del_controller: Controller tear-down.
 * Controller added with the above API is teared down using this API.
 */
extern int slim_del_controller(struct slim_controller *ctrl);

/*
 * slim_add_device: Add a new device without register board info.
 * @ctrl: Controller to which this device is to be added to.
 * Called when device doesn't have an explicit client-driver to be probed, or
 * the client-driver is a module installed dynamically.
 */
extern int slim_add_device(struct slim_controller *ctrl,
			struct slim_device *sbdev);

/* slim_remove_device: Remove the effect of slim_add_device() */
extern void slim_remove_device(struct slim_device *sbdev);

/*
 * slim_assign_laddr: Assign logical address to a device enumerated.
 * @ctrl: Controller with which device is enumerated.
 * @e_addr: 6-byte elemental address of the device.
 * @e_len: buffer length for e_addr
 * @laddr: Return logical address (if valid flag is false)
 * @valid: true if laddr holds a valid address that controller wants to
 *	set for this enumeration address. Otherwise framework sets index into
 *	address table as logical address.
 * Called by controller in response to REPORT_PRESENT. Framework will assign
 * a logical address to this enumeration address.
 * Function returns -EXFULL to indicate that all logical addresses are already
 * taken.
 */
extern int slim_assign_laddr(struct slim_controller *ctrl, const u8 *e_addr,
				u8 e_len, u8 *laddr, bool valid);

/*
 * slim_report_absent: Controller calls this function when a device
 *	reports absent, OR when the device cannot be communicated with
 * @sbdev: Device that cannot be reached, or that sent report absent
 */
void slim_report_absent(struct slim_device *sbdev);

/*
 * slim_framer_booted: This function is called by controller after the active
 * framer has booted (using Bus Reset sequence, or after it has shutdown and has
 * come back up). Components, devices on the bus may be in undefined state,
 * and this function triggers their drivers to do the needful
 * to bring them back in Reset state so that they can acquire sync, report
 * present and be operational again.
 */
void slim_framer_booted(struct slim_controller *ctrl);

/*
 * slim_msg_response: Deliver Message response received from a device to the
 *	framework.
 * @ctrl: Controller handle
 * @reply: Reply received from the device
 * @len: Length of the reply
 * @tid: Transaction ID received with which framework can associate reply.
 * Called by controller to inform framework about the response received.
 * This helps in making the API asynchronous, and controller-driver doesn't need
 * to manage 1 more table other than the one managed by framework mapping TID
 * with buffers
 */
extern void slim_msg_response(struct slim_controller *ctrl, u8 *reply, u8 tid,
				u8 len);

/*
 * slim_busnum_to_ctrl: Map bus number to controller
 * @busnum: Bus number
 * Returns controller representing this bus number
 */
extern struct slim_controller *slim_busnum_to_ctrl(u32 busnum);

/*
 * slim_ctrl_add_boarddevs: Add devices registered by board-info
 * @ctrl: Controller to which these devices are to be added to.
 * This API is called by controller when it is up and running.
 * If devices on a controller were registered before controller,
 * this will make sure that they get probed when controller is up
 */
extern void slim_ctrl_add_boarddevs(struct slim_controller *ctrl);

/*
 * slim_register_board_info: Board-initialization routine.
 * @info: List of all devices on all controllers present on the board.
 * @n: number of entries.
 * API enumerates respective devices on corresponding controller.
 * Called from board-init function.
 */
#ifdef CONFIG_SLIMBUS
extern int slim_register_board_info(struct slim_boardinfo const *info,
					unsigned n);
#else
static inline int slim_register_board_info(struct slim_boardinfo const *info,
					unsigned n)
{
	return 0;
}
#endif

static inline void *slim_get_ctrldata(const struct slim_controller *dev)
{
	return dev_get_drvdata(&dev->dev);
}

static inline void slim_set_ctrldata(struct slim_controller *dev, void *data)
{
	dev_set_drvdata(&dev->dev, data);
}

static inline void *slim_get_devicedata(const struct slim_device *dev)
{
	return dev_get_drvdata(&dev->dev);
}

static inline void slim_set_clientdata(struct slim_device *dev, void *data)
{
	dev_set_drvdata(&dev->dev, data);
}
#endif /* _LINUX_SLIMBUS_H */
