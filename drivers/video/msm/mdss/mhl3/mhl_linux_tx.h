/*
 * SiI8620 Linux Driver
 *
 * Copyright (C) 2013-2014 Silicon Image, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 * This program is distributed AS-IS WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; INCLUDING without the implied warranty
 * of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
 * See the GNU General Public License for more details at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */

#if !defined(MHL_LINUX_TX_H)
#define MHL_LINUX_TX_H

#if (INCLUDE_HID == 1)
#include "si_emsc_hid.h"
#endif
#include "si_app_devcap.h"

/*
 * EMSC Block Transaction stuff
 */
#define EMSC_RCV_BUFFER_DEFAULT		256
#define EMSC_BLK_MAX_LENGTH		256
#define EMSC_BLK_STD_HDR_LEN		2
#define EMSC_BLK_CMD_MAX_LEN		(EMSC_BLK_MAX_LENGTH - \
					EMSC_BLK_STD_HDR_LEN)
#define LOCAL_BLK_RCV_BUFFER_SIZE	288

/*
 * Event codes
 *
 */
/* No event worth reporting */
#define	MHL_TX_EVENT_NONE		0x00

/* MHL connection has been lost */
#define	MHL_TX_EVENT_DISCONNECTION	0x01

/* MHL connection has been established */
#define	MHL_TX_EVENT_CONNECTION		0x02

/* Received an RCP key code */
#define	MHL_TX_EVENT_RCP_RECEIVED	0x04

/* Received an RCPK message */
#define	MHL_TX_EVENT_RCPK_RECEIVED	0x05

/* Received an RCPE message */
#define	MHL_TX_EVENT_RCPE_RECEIVED	0x06

/* Received an UTF-8 key code */
#define	MHL_TX_EVENT_UCP_RECEIVED	0x07

/* Received an UCPK message */
#define	MHL_TX_EVENT_UCPK_RECEIVED	0x08

/* Received an UCPE message */
#define	MHL_TX_EVENT_UCPE_RECEIVED	0x09

/* Scratch Pad Data received */
#define MHL_TX_EVENT_SPAD_RECEIVED	0x0A

/* Peer's power capability has changed */
#define	MHL_TX_EVENT_POW_BIT_CHG	0x0B

/* Received a Request Action Protocol (RAP) message */
#define MHL_TX_EVENT_RAP_RECEIVED	0x0C

#if (INCLUDE_RBP == 1)
/* Received an RBP button code */
#define	MHL_TX_EVENT_RBP_RECEIVED	0x0D

/* Received an RBPK message */
#define	MHL_TX_EVENT_RBPK_RECEIVED	0x0E

/* Received an RBPE message */
#define	MHL_TX_EVENT_RBPE_RECEIVED	0x0F
#endif

/* Received a BIST_READY message */
#define	MHL_TX_EVENT_BIST_READY_RECEIVED	0x10

/* A triggered BIST test has completed */
#define	MHL_TX_EVENT_BIST_TEST_DONE			0x11

/* Received a BIST_STATUS message */
#define	MHL_TX_EVENT_BIST_STATUS_RECEIVED	0x12

/* T_RAP_MAX expired */
#define MHL_TX_EVENT_T_RAP_MAX_EXPIRED	0x13

/* peer sent AUD_DELAY burst */
#define MHL_EVENT_AUD_DELAY_RCVD 0x14

#define ADOPTER_ID_SIZE			2
#define MAX_SCRATCH_PAD_TRANSFER_SIZE	16
#define SCRATCH_PAD_SIZE		64

#define SCRATCHPAD_SIZE 16
union scratch_pad_u {
	struct MHL2_video_format_data_t videoFormatData;
	struct MHL3_hev_vic_data_t hev_vic_data;
	struct MHL3_hev_dtd_a_data_t hev_dtd_a_data;
	struct MHL3_hev_dtd_b_data_t hev_dtd_b_data;
	uint8_t asBytes[SCRATCHPAD_SIZE];
};

struct timer_obj {
	struct list_head list_link;
	struct work_struct work_item;
	struct hrtimer hr_timer;
	struct mhl_dev_context *dev_context;
	uint8_t flags;
#define TIMER_OBJ_FLAG_WORK_IP	0x01
#define TIMER_OBJ_FLAG_DEL_REQ	0x02
	void *callback_param;
	void (*timer_callback_handler) (void *callback_param);
};

union misc_flags_u {
	struct {
		unsigned rcv_scratchpad_busy:1;
		unsigned req_wrt_pending:1;
		unsigned write_burst_pending:1;
		unsigned have_complete_devcap:1;

		unsigned sent_dcap_rdy:1;
		unsigned sent_path_en:1;
		unsigned rap_content_on:1;
		unsigned mhl_hpd:1;

		unsigned mhl_rsen:1;
		unsigned edid_loop_active:1;
		unsigned cbus_abort_delay_active:1;
		unsigned have_complete_xdevcap:1;

		unsigned bist_role_TE:1;

		unsigned reserved:19;
	} flags;
	uint32_t as_uint32;
};

struct mhl_device_status {
	uint8_t write_stat[3];
	uint8_t write_xstat[4];
};

/*
 *  structure used by interrupt handler to return
 * information about an interrupt.
 */
struct interrupt_info {
	uint16_t flags;
/* Flags returned by low level driver interrupt handler */
#define DRV_INTR_MSC_DONE	0x0001	/* message send done */
#define DRV_INTR_MSC_RECVD	0x0002	/* MSC message received */
#define DRV_INTR_MSC_NAK	0x0004	/* message send unsuccessful */
#define DRV_INTR_WRITE_STAT	0x0008	/* write stat msg received */
#define DRV_INTR_SET_INT	0x0010	/* set int message received */
#define DRV_INTR_WRITE_BURST	0x0020	/* write burst received */
#define DRV_INTR_HPD_CHANGE	0x0040	/* Hot plug detect change */
#define DRV_INTR_CONNECT	0x0080	/* MHL connection established */
#define DRV_INTR_DISCONNECT	0x0100	/* MHL connection lost */
#define DRV_INTR_CBUS_ABORT	0x0200	/* CBUS msg transfer aborted */
#define DRV_INTR_COC_CAL	0x0400	/* CoC Calibration done */
#define DRV_INTR_TDM_SYNC	0x0800	/* TDM Sync Complete */
#define DRV_INTR_EMSC_INCOMING	0x1000

	void *edid_parser_context;
	uint8_t msc_done_data;
	uint8_t hpd_status;	/* status of hot plug detect */
	/* received write stat data for CONNECTED_RDY and/or LINK_MODE,
	 * and/or MHL_VERSION_STAT
	 */
	struct mhl_device_status dev_status;
	uint8_t msc_msg[2];	/* received msc message data */
	uint8_t int_msg[2];	/* received SET INT message data */
};

enum tdm_vc_assignments {
	TDM_VC_CBUS1 = 0,
	TDM_VC_E_MSC = 1,
	TDM_VC_T_CBUS = 2,
	TDM_VC_MAX = TDM_VC_T_CBUS + 1
};

/* allow for two WRITE_STAT, and one SET_INT immediately upon MHL_EST */
#define NUM_CBUS_EVENT_QUEUE_EVENTS 16

#define MHL_DEV_CONTEXT_SIGNATURE \
	(('M' << 24) | ('H' << 16) | ('L' << 8) | ' ')

struct mhl_dev_context {
	uint32_t signature;	/* identifies an instance of
				   this struct */
	struct mhl_drv_info const *drv_info;
#if (INCLUDE_SII6031 == 1)
	struct	completion sem_mhl_discovery_complete;
	bool	mhl_discovery_in_progress;
	bool	mhl_detected;
	void	(*notify_mhl)(int mhl_detected);
	void *usb_ctxt;
#endif
	struct i2c_client *client;
	struct cdev mhl_cdev;
	struct device *mhl_dev;
	struct interrupt_info intr_info;
	void *edid_parser_context;
	u8 dev_flags;
#define DEV_FLAG_SHUTDOWN	0x01	/* Device is shutting down */
#define DEV_FLAG_COMM_MODE	0x02	/* Halt INTR processing */

	u16 mhl_flags;		/* various state flags */
#define MHL_STATE_FLAG_CONNECTED	0x0001	/* MHL connection
						   established */
#define MHL_STATE_FLAG_RCP_SENT		0x0002	/* last RCP event was a key
						   send */
#define MHL_STATE_FLAG_RCP_RECEIVED	0x0004	/* last RCP event was a key
						   code receive */
#define MHL_STATE_FLAG_RCP_ACK		0x0008	/* last RCP key code sent was
						   ACK'd */
#define MHL_STATE_FLAG_RCP_NAK		0x0010	/* last RCP key code sent was
						   NAK'd */
#define MHL_STATE_FLAG_UCP_SENT		0x0020	/* last UCP event was a key
						   send */
#define MHL_STATE_FLAG_UCP_RECEIVED	0x0040	/* last UCP event was a key
						   code receive */
#define MHL_STATE_FLAG_UCP_ACK		0x0080	/* last UCP key code sent was
						   ACK'd */
#define MHL_STATE_FLAG_UCP_NAK		0x0100	/* last UCP key code sent was
						   NAK'd */
#if (INCLUDE_RBP == 1)
#define MHL_STATE_FLAG_RBP_RECEIVED	0x0200	/* last RBP event was a button
						   code receive */
#define MHL_STATE_FLAG_RBP_ACK		0x0400	/* last RBP event was a button
						   code receive */
#define MHL_STATE_FLAG_RBP_NAK		0x0800	/* last RBP event was a button
						   code receive */
#define MHL_STATE_FLAG_RBP_SENT		0x1000	/* last RBP event was a button
						   send */
#endif
#define MHL_STATE_FLAG_SPAD_SENT	0x2000	/* scratch pad send in
						   process */
#define MHL_STATE_APPLICATION_RAP_BUSY	0x4000	/* application has indicated
						   that it is processing an
						   outstanding request */
	u8 dev_cap_local_offset;
	u8 dev_cap_remote_offset;
	u8 rap_in_sub_command;
	u8 rap_in_status;
	u8 rap_out_sub_command;
	u8 rap_out_status;
	u8 rcp_in_key_code;
	u8 rcp_out_key_code;
	u8 rcp_err_code;
	u8 rcp_send_status;
	u8 ucp_in_key_code;
	u8 ucp_out_key_code;
	u8 ucp_err_code;
	u8 ucp_send_status;
#if (INCLUDE_RBP == 1)
	u8 rbp_in_button_code;
	u8 rbp_out_button_code;
	u8 rbp_err_code;
	u8 rbp_send_status;
#endif
	u8 spad_offset;
	u8 spad_xfer_length;
	u8 spad_send_status;
	u8 debug_i2c_address;
	u8 debug_i2c_offset;
	u8 debug_i2c_xfer_length;

#ifdef MEDIA_DATA_TUNNEL_SUPPORT
	struct mdt_inputdevs mdt_devs;
#endif

#if (INCLUDE_HID == 1)
	struct mhl3_hid_global_data mhl_ghid;
	struct mhl3_hid_data *mhl_hid[16];
	struct workqueue_struct *hid_work_queue;
#endif

	u8 error_key;
	struct input_dev *rcp_input_dev;
#if (INCLUDE_RBP == 1)
	struct input_dev *rbp_input_dev;
#endif
	struct semaphore isr_lock;	/* semaphore used to prevent driver
					 * access from user mode from colliding
					 * with the threaded interrupt handler
					 */

	u8 status_0;		/* Received status from peer saved here */
	u8 status_1;
	u8 peer_mhl3_version;
	u8 xstatus_1;
	u8 xstatus_3;
	bool msc_msg_arrived;
	u8 msc_msg_sub_command;
	u8 msc_msg_data;

	u8 msc_msg_last_data;
	u8 msc_save_rcp_key_code;
#if (INCLUDE_RBP == 1)
	u8 msc_save_rbp_button_code;
#endif
	u8 msc_save_ucp_key_code;
	u8 link_mode;		/* outgoing MHL LINK_MODE register value */
	bool mhl_connection_event;
	u8 mhl_connected;
	struct workqueue_struct *timer_work_queue;
	struct list_head timer_list;
	struct list_head cbus_queue;
	struct list_head cbus_free_list;
	struct cbus_req cbus_req_entries[NUM_CBUS_EVENT_QUEUE_EVENTS];
	struct cbus_req *current_cbus_req;
	int sequence;
	void *cbus_abort_timer;
	void *dcap_rdy_timer;
	void *dcap_chg_timer;
	void *t_rap_max_timer;
	union MHLDevCap_u dev_cap_cache;
	union MHLXDevCap_u xdev_cap_cache;
	u8 preferred_clk_mode;
	union scratch_pad_u incoming_scratch_pad;
	union scratch_pad_u outgoing_scratch_pad;

	uint8_t virt_chan_slot_counts[TDM_VC_MAX];
	uint8_t prev_virt_chan_slot_counts[TDM_VC_MAX];

	void *cbus_mode_up_timer;

	void *bist_timer;
	uint32_t bist_timeout_value;
	uint32_t bist_timeout_total;
	struct bist_setup_info bist_setup;
	struct bist_setup_info sysfs_bist_setup;
	struct bist_stat_info bist_stat;
	uint8_t bist_trigger_info;
	uint8_t bist_ready_status;
	union misc_flags_u misc_flags;

	struct {
		int sequence;
		unsigned long local_blk_rx_buffer_size;
		struct list_head queue;
		struct list_head free_list;
#define NUM_BLOCK_QUEUE_REQUESTS 4
		struct block_req *marshalling_req;
		struct block_req req_entries[NUM_BLOCK_QUEUE_REQUESTS];
	} block_protocol;

	bool sii_adopter_id;
	bool edid_valid;
	uint8_t numEdidExtensions;
#ifndef OLD_KEYMAP_TABLE
	void *timer_T_press_mode;
	void *timer_T_hold_maintain;
#endif

	void *drv_context;	/* pointer aligned start of mhl
				   transmitter driver context area */
};

#define PACKED_PIXEL_AVAILABLE(dev_context) \
	((MHL_DEV_VID_LINK_SUPP_PPIXEL & \
	dev_context->dev_cap_cache.devcap_cache[DEVCAP_OFFSET_VID_LINK_MODE]) \
	&& (MHL_DEV_VID_LINK_SUPP_PPIXEL & DEVCAP_VAL_VID_LINK_MODE))

#define _16_BPP_AVAILABLE(dev_context) \
	((MHL_DEV_VID_LINK_SUPP_16BPP & \
	dev_context->dev_cap_cache.devcap_cache[DEVCAP_OFFSET_VID_LINK_MODE]) \
	&& (MHL_DEV_VID_LINK_SUPP_16BPP & DEVCAP_VAL_VID_LINK_MODE))

enum scratch_pad_status {
	SCRATCHPAD_FAIL = -4,
	SCRATCHPAD_BAD_PARAM = -3,
	SCRATCHPAD_NOT_SUPPORTED = -2,
	SCRATCHPAD_BUSY = -1,
	SCRATCHPAD_SUCCESS = 0
};

struct drv_hw_context;

struct mhl_drv_info {
	int drv_context_size;
	struct {
		uint8_t major:4;
		uint8_t minor:4;
	} mhl_version_support;
	int irq;

	/* APIs required to be supported by the low level MHL TX driver */
	int (*mhl_device_initialize) (struct drv_hw_context *hw_context);
	void (*mhl_device_isr) (struct drv_hw_context *hw_context,
		struct interrupt_info *intr_info);
	int (*mhl_device_dbg_i2c_reg_xfer) (void *dev_context, u8 page,
		u8 offset, u16 count, bool rw_flag, u8 *buffer);
	int (*mhl_device_get_aksv) (struct drv_hw_context *hw_context,
		u8 *buffer);
};

/* APIs provided by the Linux layer to the lower level driver */
int mhl_handle_power_change_request(struct device *parent_dev, bool power_up);

int mhl_tx_init(struct mhl_drv_info const *drv_info, struct device *parent_dev);

int mhl_tx_remove(struct device *parent_dev);

void mhl_event_notify(struct mhl_dev_context *dev_context, u32 event,
	u32 event_param, void *data);

struct mhl_dev_context *get_mhl_device_context(void *context);

void *si_mhl_tx_get_drv_context(void *dev_context);

int mhl_tx_create_timer(void *context,
	void (*callback_handler) (void *callback_param), void *callback_param,
	void **timer_handle);

int mhl_tx_delete_timer(void *context, void **timer_handle);

int mhl_tx_start_timer(void *context, void *timer_handle, uint32_t time_msec);

int mhl_tx_stop_timer(void *context, void *timer_handle);
void mhl_tx_stop_all_timers(struct mhl_dev_context *dev_context);

void si_mhl_tx_request_first_edid_block(struct mhl_dev_context *dev_context);
void si_mhl_tx_handle_atomic_hw_edid_read_complete(struct edid_3d_data_t
	*mhl_edid_3d_data);

/* APIs used within the Linux layer of the driver. */
uint8_t si_mhl_tx_get_peer_dev_cap_entry(struct mhl_dev_context *dev_context,
	uint8_t index, uint8_t *data);

enum scratch_pad_status si_get_scratch_pad_vector(struct mhl_dev_context
	*dev_context, uint8_t offset, uint8_t length, uint8_t *data);

#if (INCLUDE_SII6031 == 1)
void mhl_tx_notify_otg(struct mhl_dev_context *dev_context, bool mhl_detected);

int otg_register_mhl_discovery(void *mhl_ctx, int (*mhl_discover_device)
	(void *, int, void (*)(void *, int online), void *));
int otg_unregister_mhl_discovery(void);
void otg_mhl_notify(void *ctxt, int on);

#endif
#endif /* if !defined(MHL_LINUX_TX_H) */
