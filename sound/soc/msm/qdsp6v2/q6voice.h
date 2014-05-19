/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#ifndef __QDSP6VOICE_H__
#define __QDSP6VOICE_H__

#include <linux/qdsp6v2/apr.h>
#include <linux/qdsp6v2/rtac.h>
#include <linux/msm_ion.h>
#include <sound/voice_params.h>
#include <linux/power_supply.h>
#include <uapi/linux/vm_bms.h>

#define MAX_VOC_PKT_SIZE 642
#define SESSION_NAME_LEN 20
#define NUM_OF_MEMORY_BLOCKS 1
#define NUM_OF_BUFFERS 2
/*
 * BUFFER BLOCK SIZE based on
 * the supported page size
 */
#define BUFFER_BLOCK_SIZE       4096

#define MAX_COL_INFO_SIZE	324

#define VOC_REC_UPLINK		0x00
#define VOC_REC_DOWNLINK	0x01
#define VOC_REC_BOTH		0x02

struct voice_header {
	uint32_t id;
	uint32_t data_len;
};

struct voice_init {
	struct voice_header hdr;
	void *cb_handle;
};

/* Stream information payload structure */
struct stream_data {
	uint32_t stream_mute;
	uint32_t stream_mute_ramp_duration_ms;
};

/* Device information payload structure */
struct device_data {
	uint32_t dev_mute;
	uint32_t sample;
	uint32_t enabled;
	uint32_t dev_id;
	uint32_t port_id;
	uint32_t volume_step_value;
	uint32_t volume_ramp_duration_ms;
	uint32_t dev_mute_ramp_duration_ms;
};

struct voice_dev_route_state {
	u16 rx_route_flag;
	u16 tx_route_flag;
};

struct voice_rec_route_state {
	u16 ul_flag;
	u16 dl_flag;
};

enum {
	VOC_INIT = 0,
	VOC_RUN,
	VOC_CHANGE,
	VOC_RELEASE,
	VOC_ERROR,
	VOC_STANDBY,
};

struct mem_buffer {
	dma_addr_t		phys;
	void			*data;
	uint32_t		size; /* size of buffer */
};

struct share_mem_buf {
	struct ion_handle	*handle;
	struct ion_client	*client;
	struct mem_buffer	buf[NUM_OF_BUFFERS];
};

struct mem_map_table {
	dma_addr_t		phys;
	void			*data;
	uint32_t		size; /* size of buffer */
	struct ion_handle	*handle;
	struct ion_client	*client;
};

/* Common */
#define VSS_ICOMMON_CMD_SET_UI_PROPERTY 0x00011103
/* Set a UI property */
#define VSS_ICOMMON_CMD_MAP_MEMORY   0x00011025
#define VSS_ICOMMON_CMD_UNMAP_MEMORY 0x00011026
/* General shared memory; byte-accessible, 4 kB-aligned. */
#define VSS_ICOMMON_MAP_MEMORY_SHMEM8_4K_POOL  3

struct vss_icommon_cmd_map_memory_t {
	uint32_t phys_addr;
	/* Physical address of a memory region; must be at least
	 *  4 kB aligned.
	 */

	uint32_t mem_size;
	/* Number of bytes in the region; should be a multiple of 32. */

	uint16_t mem_pool_id;
	/* Type of memory being provided. The memory ID implicitly defines
	 *  the characteristics of the memory. The characteristics might include
	 *  alignment type, permissions, etc.
	 * Memory pool ID. Possible values:
	 * 3 -- VSS_ICOMMON_MEM_TYPE_SHMEM8_4K_POOL.
	 */
} __packed;

struct vss_icommon_cmd_unmap_memory_t {
	uint32_t phys_addr;
	/* Physical address of a memory region; must be at least
	 *  4 kB aligned.
	 */
} __packed;

struct vss_map_memory_cmd {
	struct apr_hdr hdr;
	struct vss_icommon_cmd_map_memory_t vss_map_mem;
} __packed;

struct vss_unmap_memory_cmd {
	struct apr_hdr hdr;
	struct vss_icommon_cmd_unmap_memory_t vss_unmap_mem;
} __packed;

/* TO MVM commands */
#define VSS_IMVM_CMD_CREATE_PASSIVE_CONTROL_SESSION	0x000110FF
/**< No payload. Wait for APRV2_IBASIC_RSP_RESULT response. */

#define VSS_IMVM_CMD_SET_POLICY_DUAL_CONTROL	0x00011327
/*
 * VSS_IMVM_CMD_SET_POLICY_DUAL_CONTROL
 * Description: This command is required to let MVM know
 * who is in control of session.
 * Payload: Defined by vss_imvm_cmd_set_policy_dual_control_t.
 * Result: Wait for APRV2_IBASIC_RSP_RESULT response.
 */

#define VSS_IMVM_CMD_CREATE_FULL_CONTROL_SESSION	0x000110FE
/* Create a new full control MVM session. */

#define APRV2_IBASIC_CMD_DESTROY_SESSION		0x0001003C
/**< No payload. Wait for APRV2_IBASIC_RSP_RESULT response. */

#define VSS_IMVM_CMD_ATTACH_STREAM			0x0001123C
/* Attach a stream to the MVM. */

#define VSS_IMVM_CMD_DETACH_STREAM			0x0001123D
/* Detach a stream from the MVM. */

#define VSS_IMVM_CMD_ATTACH_VOCPROC		       0x0001123E
/* Attach a vocproc to the MVM. The MVM will symmetrically connect this vocproc
 * to all the streams currently attached to it.
 */

#define VSS_IMVM_CMD_DETACH_VOCPROC			0x0001123F
/* Detach a vocproc from the MVM. The MVM will symmetrically disconnect this
 * vocproc from all the streams to which it is currently attached.
*/

#define VSS_IMVM_CMD_START_VOICE			0x00011190
/**< No payload. Wait for APRV2_IBASIC_RSP_RESULT response. */

#define VSS_IMVM_CMD_STANDBY_VOICE                       0x00011191
/**< No payload. Wait for APRV2_IBASIC_RSP_RESULT response. */

#define VSS_IMVM_CMD_STOP_VOICE				0x00011192
/**< No payload. Wait for APRV2_IBASIC_RSP_RESULT response. */

#define VSS_IMVM_CMD_PAUSE_VOICE			0x0001137D
/* No payload. Wait for APRV2_IBASIC_RSP_RESULT response. */

#define VSS_ISTREAM_CMD_ATTACH_VOCPROC			0x000110F8
/**< Wait for APRV2_IBASIC_RSP_RESULT response. */

#define VSS_ISTREAM_CMD_DETACH_VOCPROC			0x000110F9
/**< Wait for APRV2_IBASIC_RSP_RESULT response. */


#define VSS_ISTREAM_CMD_SET_TTY_MODE			0x00011196
/**< Wait for APRV2_IBASIC_RSP_RESULT response. */

#define VSS_ICOMMON_CMD_SET_NETWORK			0x0001119C
/* Set the network type. */

#define VSS_ICOMMON_CMD_SET_VOICE_TIMING		0x000111E0
/* Set the voice timing parameters. */

#define VSS_IMEMORY_CMD_MAP_PHYSICAL			0x00011334
#define VSS_IMEMORY_RSP_MAP				0x00011336
#define VSS_IMEMORY_CMD_UNMAP				0x00011337
#define VSS_IMVM_CMD_SET_CAL_NETWORK			0x0001137A
#define VSS_IMVM_CMD_SET_CAL_MEDIA_TYPE		0x0001137B

enum msm_audio_voc_rate {
		VOC_0_RATE, /* Blank frame */
		VOC_8_RATE, /* 1/8 rate    */
		VOC_4_RATE, /* 1/4 rate    */
		VOC_2_RATE, /* 1/2 rate    */
		VOC_1_RATE,  /* Full rate   */
		VOC_8_RATE_NC  /* Noncritical 1/8 rate   */
};

struct vss_istream_cmd_set_tty_mode_t {
	uint32_t mode;
	/**<
	* TTY mode.
	*
	* 0 : TTY disabled
	* 1 : HCO
	* 2 : VCO
	* 3 : FULL
	*/
} __packed;

struct vss_istream_cmd_attach_vocproc_t {
	uint16_t handle;
	/**< Handle of vocproc being attached. */
} __packed;

struct vss_istream_cmd_detach_vocproc_t {
	uint16_t handle;
	/**< Handle of vocproc being detached. */
} __packed;

struct vss_imvm_cmd_attach_stream_t {
	uint16_t handle;
	/* The stream handle to attach. */
} __packed;

struct vss_imvm_cmd_detach_stream_t {
	uint16_t handle;
	/* The stream handle to detach. */
} __packed;

struct vss_icommon_cmd_set_network_t {
	uint32_t network_id;
	/* Network ID. (Refer to VSS_NETWORK_ID_XXX). */
} __packed;

struct vss_icommon_cmd_set_voice_timing_t {
	uint16_t mode;
	/*
	 * The vocoder frame synchronization mode.
	 *
	 * 0 : No frame sync.
	 * 1 : Hard VFR (20ms Vocoder Frame Reference interrupt).
	 */
	uint16_t enc_offset;
	/*
	 * The offset in microseconds from the VFR to deliver a Tx vocoder
	 * packet. The offset should be less than 20000us.
	 */
	uint16_t dec_req_offset;
	/*
	 * The offset in microseconds from the VFR to request for an Rx vocoder
	 * packet. The offset should be less than 20000us.
	 */
	uint16_t dec_offset;
	/*
	 * The offset in microseconds from the VFR to indicate the deadline to
	 * receive an Rx vocoder packet. The offset should be less than 20000us.
	 * Rx vocoder packets received after this deadline are not guaranteed to
	 * be processed.
	 */
} __packed;

struct vss_imvm_cmd_create_control_session_t {
	char name[SESSION_NAME_LEN];
	/*
	* A variable-sized stream name.
	*
	* The stream name size is the payload size minus the size of the other
	* fields.
	*/
} __packed;


struct vss_imvm_cmd_set_policy_dual_control_t {
	bool enable_flag;
	/* Set to TRUE to enable modem state machine control */
} __packed;

struct mvm_attach_vocproc_cmd {
	struct apr_hdr hdr;
	struct vss_istream_cmd_attach_vocproc_t mvm_attach_cvp_handle;
} __packed;

struct mvm_detach_vocproc_cmd {
	struct apr_hdr hdr;
	struct vss_istream_cmd_detach_vocproc_t mvm_detach_cvp_handle;
} __packed;

struct mvm_create_ctl_session_cmd {
	struct apr_hdr hdr;
	struct vss_imvm_cmd_create_control_session_t mvm_session;
} __packed;

struct mvm_modem_dual_control_session_cmd {
	struct apr_hdr hdr;
	struct vss_imvm_cmd_set_policy_dual_control_t voice_ctl;
} __packed;

struct mvm_set_tty_mode_cmd {
	struct apr_hdr hdr;
	struct vss_istream_cmd_set_tty_mode_t tty_mode;
} __packed;

struct mvm_attach_stream_cmd {
	struct apr_hdr hdr;
	struct vss_imvm_cmd_attach_stream_t attach_stream;
} __packed;

struct mvm_detach_stream_cmd {
	struct apr_hdr hdr;
	struct vss_imvm_cmd_detach_stream_t detach_stream;
} __packed;

struct mvm_set_network_cmd {
	struct apr_hdr hdr;
	struct vss_icommon_cmd_set_network_t network;
} __packed;

struct mvm_set_voice_timing_cmd {
	struct apr_hdr hdr;
	struct vss_icommon_cmd_set_voice_timing_t timing;
} __packed;

struct vss_imemory_table_descriptor_t {
	uint64_t mem_address;
	/*
	 * Base physical address of the table. The address must be aligned
	 * to LCM( cache_line_size, page_align, max_data_width ), where the
	 * attributes are specified in #VSS_IMEMORY_CMD_MAP_PHYSICAL, and
	 * LCM = Least Common Multiple. The table at the address must have
	 * the format specified by #vss_imemory_table_t.
	 */
	uint32_t mem_size;
	/* Size in bytes of the table. */
} __packed;

struct vss_imemory_block_t {
	uint64_t mem_address;
	/*
	 * Base address of the memory block. The address is virtual for virtual
	 * memory and physical for physical memory. The address must be aligned
	 * to LCM( cache_line_size, page_align, max_data_width ), where the
	 * attributes are specified in VSS_IMEMORY_CMD_MAP_VIRTUAL or
	 * VSS_IMEMORY_CMD_MAP_PHYSICAL, and LCM = Least Common Multiple.
	 */
	uint32_t mem_size;
	/*
	 * Size in bytes of the memory block. The size must be multiple of
	 * page_align, where page_align is specified in
	 * VSS_IMEMORY_CMD_MAP_VIRTUAL or #VSS_IMEMORY_CMD_MAP_PHYSICAL.
	 */
} __packed;

struct vss_imemory_table_t {
	struct vss_imemory_table_descriptor_t next_table_descriptor;
	/*
	 * Specifies the next table. If there is no next table,
	 * set the size of the table to 0 and the table address is ignored.
	 */
	struct vss_imemory_block_t blocks[NUM_OF_MEMORY_BLOCKS];
	/* Specifies one ore more memory blocks. */
} __packed;

struct vss_imemory_cmd_map_physical_t {
	struct apr_hdr hdr;
	struct vss_imemory_table_descriptor_t table_descriptor;
	bool is_cached;
	/*
	 * Indicates cached or uncached memory. Supported values:
	 * TRUE - Cached.
	 */
	uint16_t cache_line_size;
	/* Cache line size in bytes. Supported values: 128 */
	uint32_t access_mask;
	/*
	 * CVD's access permission to the memory while it is mapped.
	 * Supported values:
	 * bit 0 - If set, the memory is readable.
	 * bit 1 - If set, the memory is writable.
	 */
	uint32_t page_align;
	/* Page frame alignment in bytes. Supported values: 4096 */
	uint8_t min_data_width;
	/*
	 * Minimum native data type width in bits that can be accessed.
	 * Supported values: 8
	 */
	uint8_t max_data_width;
	/*
	 * Maximum native data type width in bits that can be accessed.
	 * Supported values: 64
	 */
} __packed;

struct vss_imvm_cmd_set_cal_network_t {
	struct apr_hdr hdr;
	uint32_t network_id;
} __packed;

struct vss_imvm_cmd_set_cal_media_type_t {
	struct apr_hdr hdr;
	uint32_t media_id;
} __packed;

struct vss_imemory_cmd_unmap_t {
	struct apr_hdr hdr;
	uint32_t mem_handle;
} __packed;

/* TO CVS commands */
#define VSS_ISTREAM_CMD_CREATE_PASSIVE_CONTROL_SESSION	0x00011140
/**< Wait for APRV2_IBASIC_RSP_RESULT response. */

#define VSS_ISTREAM_CMD_CREATE_FULL_CONTROL_SESSION	0x000110F7
/* Create a new full control stream session. */

#define APRV2_IBASIC_CMD_DESTROY_SESSION		0x0001003C

/*
 * This command changes the mute setting. The new mute setting will
 * be applied over the specified ramp duration.
 */
#define VSS_IVOLUME_CMD_MUTE_V2				0x0001138B

#define VSS_ISTREAM_CMD_REGISTER_CALIBRATION_DATA_V2    0x00011369

#define VSS_ISTREAM_CMD_DEREGISTER_CALIBRATION_DATA     0x0001127A

#define VSS_ISTREAM_CMD_SET_MEDIA_TYPE			0x00011186
/* Set media type on the stream. */

#define VSS_ISTREAM_EVT_SEND_ENC_BUFFER			0x00011015
/* Event sent by the stream to its client to provide an encoded packet. */

#define VSS_ISTREAM_EVT_REQUEST_DEC_BUFFER		0x00011017
/* Event sent by the stream to its client requesting for a decoder packet.
 * The client should respond with a VSS_ISTREAM_EVT_SEND_DEC_BUFFER event.
 */

#define VSS_ISTREAM_EVT_OOB_NOTIFY_DEC_BUFFER_REQUEST	0x0001136E

#define VSS_ISTREAM_EVT_SEND_DEC_BUFFER			0x00011016
/* Event sent by the client to the stream in response to a
 * VSS_ISTREAM_EVT_REQUEST_DEC_BUFFER event, providing a decoder packet.
 */

#define VSS_ISTREAM_CMD_VOC_AMR_SET_ENC_RATE		0x0001113E
/* Set AMR encoder rate. */

#define VSS_ISTREAM_CMD_VOC_AMRWB_SET_ENC_RATE		0x0001113F
/* Set AMR-WB encoder rate. */

#define VSS_ISTREAM_CMD_CDMA_SET_ENC_MINMAX_RATE	0x00011019
/* Set encoder minimum and maximum rate. */

#define VSS_ISTREAM_CMD_SET_ENC_DTX_MODE		0x0001101D
/* Set encoder DTX mode. */

#define MODULE_ID_VOICE_MODULE_ST			0x00010EE3
#define VOICE_PARAM_MOD_ENABLE				0x00010E00
#define MOD_ENABLE_PARAM_LEN				4

#define VSS_IPLAYBACK_CMD_START				0x000112BD
/* Start in-call music delivery on the Tx voice path. */

#define VSS_IPLAYBACK_CMD_STOP				0x00011239
/* Stop the in-call music delivery on the Tx voice path. */

#define VSS_IPLAYBACK_PORT_ID_DEFAULT			0x0000FFFF
/* Default AFE port ID. */

#define VSS_IPLAYBACK_PORT_ID_VOICE			0x00008005
/* AFE port ID for VOICE 1. */

#define VSS_IPLAYBACK_PORT_ID_VOICE2			0x00008002
/* AFE port ID for VOICE 2. */

#define VSS_IRECORD_CMD_START				0x000112BE
/* Start in-call conversation recording. */
#define VSS_IRECORD_CMD_STOP				0x00011237
/* Stop in-call conversation recording. */

#define VSS_IRECORD_PORT_ID_DEFAULT			0x0000FFFF
/* Default AFE port ID. */

#define VSS_IRECORD_TAP_POINT_NONE			0x00010F78
/* Indicates no tapping for specified path. */

#define VSS_IRECORD_TAP_POINT_STREAM_END		0x00010F79
/* Indicates that specified path should be tapped at the end of the stream. */

#define VSS_IRECORD_MODE_TX_RX_STEREO			0x00010F7A
/* Select Tx on left channel and Rx on right channel. */

#define VSS_IRECORD_MODE_TX_RX_MIXING			0x00010F7B
/* Select mixed Tx and Rx paths. */

#define VSS_ISTREAM_EVT_NOT_READY			0x000110FD

#define VSS_ISTREAM_EVT_READY				0x000110FC

#define VSS_ISTREAM_EVT_OOB_NOTIFY_DEC_BUFFER_READY	0x0001136F
/*notify dsp that decoder buffer is ready*/

#define VSS_ISTREAM_EVT_OOB_NOTIFY_ENC_BUFFER_READY	0x0001136C
/*dsp notifying client that encoder buffer is ready*/

#define VSS_ISTREAM_EVT_OOB_NOTIFY_ENC_BUFFER_CONSUMED	0x0001136D
/*notify dsp that encoder buffer is consumed*/

#define VSS_ISTREAM_CMD_SET_OOB_PACKET_EXCHANGE_CONFIG	0x0001136B

#define VSS_ISTREAM_PACKET_EXCHANGE_MODE_INBAND	0
/* In-band packet exchange mode. */

#define VSS_ISTREAM_PACKET_EXCHANGE_MODE_OUT_OF_BAND	1
/* Out-of-band packet exchange mode. */

#define VSS_ISTREAM_CMD_SET_PACKET_EXCHANGE_MODE	0x0001136A

struct vss_iplayback_cmd_start_t {
	uint16_t port_id;
	/*
	 * AFE Port ID from which the audio samples are available.
	 * To use the default AFE pseudo port (0x8005), set this value to
	 * #VSS_IPLAYBACK_PORT_ID_DEFAULT.
	 */
}  __packed;

struct vss_irecord_cmd_start_t {
	uint32_t rx_tap_point;
	/* Tap point to use on the Rx path. Supported values are:
	 * VSS_IRECORD_TAP_POINT_NONE : Do not record Rx path.
	 * VSS_IRECORD_TAP_POINT_STREAM_END : Rx tap point is at the end of
	 * the stream.
	 */
	uint32_t tx_tap_point;
	/* Tap point to use on the Tx path. Supported values are:
	 * VSS_IRECORD_TAP_POINT_NONE : Do not record tx path.
	 * VSS_IRECORD_TAP_POINT_STREAM_END : Tx tap point is at the end of
	 * the stream.
	 */
	uint16_t port_id;
	/* AFE Port ID to whcih the conversation recording stream needs to be
	 * sent. Set this to #VSS_IRECORD_PORT_ID_DEFAULT to use default AFE
	 * pseudo ports (0x8003 for Rx and 0x8004 for Tx).
	 */
	uint32_t mode;
	/* Recording Mode. The mode parameter value is ignored if the port_id
	 * is set to #VSS_IRECORD_PORT_ID_DEFAULT.
	 * The supported values:
	 * #VSS_IRECORD_MODE_TX_RX_STEREO
	 * #VSS_IRECORD_MODE_TX_RX_MIXING
	 */
} __packed;

struct vss_istream_cmd_create_passive_control_session_t {
	char name[SESSION_NAME_LEN];
	/**<
	* A variable-sized stream name.
	*
	* The stream name size is the payload size minus the size of the other
	* fields.
	*/
} __packed;

#define VSS_IVOLUME_DIRECTION_TX	0
#define VSS_IVOLUME_DIRECTION_RX	1

#define VSS_IVOLUME_MUTE_OFF		0
#define VSS_IVOLUME_MUTE_ON		1

#define DEFAULT_MUTE_RAMP_DURATION	500
#define DEFAULT_VOLUME_RAMP_DURATION	20
#define MAX_RAMP_DURATION		5000

struct vss_ivolume_cmd_mute_v2_t {
	uint16_t direction;
	/*
	 * The direction field sets the direction to apply the mute command.
	 * The Supported values:
	 * VSS_IVOLUME_DIRECTION_TX
	 * VSS_IVOLUME_DIRECTION_RX
	 */
	uint16_t mute_flag;
	/*
	 * Turn mute on or off. The Supported values:
	 * VSS_IVOLUME_MUTE_OFF
	 * VSS_IVOLUME_MUTE_ON
	 */
	uint16_t ramp_duration_ms;
	/*
	 * Mute change ramp duration in milliseconds.
	 * The Supported values: 0 to 5000.
	 */
} __packed;

struct vss_istream_cmd_create_full_control_session_t {
	uint16_t direction;
	/*
	 * Stream direction.
	 *
	 * 0 : TX only
	 * 1 : RX only
	 * 2 : TX and RX
	 * 3 : TX and RX loopback
	 */
	uint32_t enc_media_type;
	/* Tx vocoder type. (Refer to VSS_MEDIA_ID_XXX). */
	uint32_t dec_media_type;
	/* Rx vocoder type. (Refer to VSS_MEDIA_ID_XXX). */
	uint32_t network_id;
	/* Network ID. (Refer to VSS_NETWORK_ID_XXX). */
	char name[SESSION_NAME_LEN];
	/*
	 * A variable-sized stream name.
	 *
	 * The stream name size is the payload size minus the size of the other
	 * fields.
	 */
} __packed;

struct vss_istream_cmd_set_media_type_t {
	uint32_t rx_media_id;
	/* Set the Rx vocoder type. (Refer to VSS_MEDIA_ID_XXX). */
	uint32_t tx_media_id;
	/* Set the Tx vocoder type. (Refer to VSS_MEDIA_ID_XXX). */
} __packed;

struct vss_istream_evt_send_enc_buffer_t {
	uint32_t media_id;
      /* Media ID of the packet. */
	uint8_t packet_data[MAX_VOC_PKT_SIZE];
      /* Packet data buffer. */
} __packed;

struct vss_istream_evt_send_dec_buffer_t {
	uint32_t media_id;
      /* Media ID of the packet. */
	uint8_t packet_data[MAX_VOC_PKT_SIZE];
      /* Packet data. */
} __packed;

struct vss_istream_cmd_voc_amr_set_enc_rate_t {
	uint32_t mode;
	/* Set the AMR encoder rate.
	 *
	 * 0x00000000 : 4.75 kbps
	 * 0x00000001 : 5.15 kbps
	 * 0x00000002 : 5.90 kbps
	 * 0x00000003 : 6.70 kbps
	 * 0x00000004 : 7.40 kbps
	 * 0x00000005 : 7.95 kbps
	 * 0x00000006 : 10.2 kbps
	 * 0x00000007 : 12.2 kbps
	 */
} __packed;

struct vss_istream_cmd_voc_amrwb_set_enc_rate_t {
	uint32_t mode;
	/* Set the AMR-WB encoder rate.
	 *
	 * 0x00000000 :  6.60 kbps
	 * 0x00000001 :  8.85 kbps
	 * 0x00000002 : 12.65 kbps
	 * 0x00000003 : 14.25 kbps
	 * 0x00000004 : 15.85 kbps
	 * 0x00000005 : 18.25 kbps
	 * 0x00000006 : 19.85 kbps
	 * 0x00000007 : 23.05 kbps
	 * 0x00000008 : 23.85 kbps
	 */
} __packed;

struct vss_istream_cmd_cdma_set_enc_minmax_rate_t {
	uint16_t min_rate;
	/* Set the lower bound encoder rate.
	 *
	 * 0x0000 : Blank frame
	 * 0x0001 : Eighth rate
	 * 0x0002 : Quarter rate
	 * 0x0003 : Half rate
	 * 0x0004 : Full rate
	 */
	uint16_t max_rate;
	/* Set the upper bound encoder rate.
	 *
	 * 0x0000 : Blank frame
	 * 0x0001 : Eighth rate
	 * 0x0002 : Quarter rate
	 * 0x0003 : Half rate
	 * 0x0004 : Full rate
	 */
} __packed;

struct vss_istream_cmd_set_enc_dtx_mode_t {
	uint32_t enable;
	/* Toggle DTX on or off.
	 *
	 * 0 : Disables DTX
	 * 1 : Enables DTX
	 */
} __packed;

struct vss_istream_cmd_register_calibration_data_v2_t {
	uint32_t cal_mem_handle;
	/* Handle to the shared memory that holds the calibration data. */
	uint64_t cal_mem_address;
	/* Location of calibration data. */
	uint32_t cal_mem_size;
	/* Size of the calibration data in bytes. */
	uint8_t column_info[MAX_COL_INFO_SIZE];
	/*
	 * Column info contains the number of columns and the array of columns
	 * in the calibration table. The order in which the columns are provided
	 * here must match the order in which they exist in the calibration
	 * table provided.
	 */
} __packed;

struct vss_icommon_cmd_set_ui_property_enable_t {
	uint32_t module_id;
	/* Unique ID of the module. */
	uint32_t param_id;
	/* Unique ID of the parameter. */
	uint16_t param_size;
	/* Size of the parameter in bytes: MOD_ENABLE_PARAM_LEN */
	uint16_t reserved;
	/* Reserved; set to 0. */
	uint16_t enable;
	uint16_t reserved_field;
	/* Reserved, set to 0. */
};

/*
 * Event sent by the stream to the client that enables Rx DTMF
 * detection whenever DTMF is detected in the Rx path.
 *
 * The DTMF detection feature can only be used to detect DTMF
 * frequencies as listed in the vss_istream_evt_rx_dtmf_detected_t
 * structure.
 */

#define VSS_ISTREAM_EVT_RX_DTMF_DETECTED 0x0001101A

struct vss_istream_cmd_set_rx_dtmf_detection {
	/*
	 * Enables/disables Rx DTMF detection
	 *
	 * Possible values are
	 * 0 - disable
	 * 1 - enable
	 *
	 */
	uint32_t enable;
};

#define VSS_ISTREAM_CMD_SET_RX_DTMF_DETECTION 0x00011027

struct vss_istream_evt_rx_dtmf_detected {
	uint16_t low_freq;
	/*
	 * Detected low frequency. Possible values:
	 * 697 Hz
	 * 770 Hz
	 * 852 Hz
	 * 941 Hz
	 */
	uint16_t high_freq;
	/*
	 * Detected high frequency. Possible values:
	 * 1209 Hz
	 * 1336 Hz
	 * 1477 Hz
	 * 1633 Hz
	 */
};

struct cvs_set_rx_dtmf_detection_cmd {
	struct apr_hdr hdr;
	struct vss_istream_cmd_set_rx_dtmf_detection cvs_dtmf_det;
} __packed;


struct cvs_create_passive_ctl_session_cmd {
	struct apr_hdr hdr;
	struct vss_istream_cmd_create_passive_control_session_t cvs_session;
} __packed;

struct cvs_create_full_ctl_session_cmd {
	struct apr_hdr hdr;
	struct vss_istream_cmd_create_full_control_session_t cvs_session;
} __packed;

struct cvs_destroy_session_cmd {
	struct apr_hdr hdr;
} __packed;

struct cvs_set_mute_cmd {
	struct apr_hdr hdr;
	struct vss_ivolume_cmd_mute_v2_t cvs_set_mute;
} __packed;

struct cvs_set_media_type_cmd {
	struct apr_hdr hdr;
	struct vss_istream_cmd_set_media_type_t media_type;
} __packed;

struct cvs_send_dec_buf_cmd {
	struct apr_hdr hdr;
	struct vss_istream_evt_send_dec_buffer_t dec_buf;
} __packed;

struct cvs_set_amr_enc_rate_cmd {
	struct apr_hdr hdr;
	struct vss_istream_cmd_voc_amr_set_enc_rate_t amr_rate;
} __packed;

struct cvs_set_amrwb_enc_rate_cmd {
	struct apr_hdr hdr;
	struct vss_istream_cmd_voc_amrwb_set_enc_rate_t amrwb_rate;
} __packed;

struct cvs_set_cdma_enc_minmax_rate_cmd {
	struct apr_hdr hdr;
	struct vss_istream_cmd_cdma_set_enc_minmax_rate_t cdma_rate;
} __packed;

struct cvs_set_enc_dtx_mode_cmd {
	struct apr_hdr hdr;
	struct vss_istream_cmd_set_enc_dtx_mode_t dtx_mode;
} __packed;

struct cvs_register_cal_data_cmd {
	struct apr_hdr hdr;
	struct vss_istream_cmd_register_calibration_data_v2_t cvs_cal_data;
} __packed;

struct cvs_deregister_cal_data_cmd {
	struct apr_hdr hdr;
} __packed;

struct cvs_set_pp_enable_cmd {
	struct apr_hdr hdr;
	struct vss_icommon_cmd_set_ui_property_enable_t vss_set_pp;
} __packed;
struct cvs_start_record_cmd {
	struct apr_hdr hdr;
	struct vss_irecord_cmd_start_t rec_mode;
} __packed;

struct cvs_start_playback_cmd {
	struct apr_hdr hdr;
	struct vss_iplayback_cmd_start_t playback_mode;
} __packed;

struct cvs_dec_buffer_ready_cmd {
	struct apr_hdr hdr;
} __packed;

struct cvs_enc_buffer_consumed_cmd {
	struct apr_hdr hdr;
} __packed;

struct vss_istream_cmd_set_oob_packet_exchange_config_t {
	struct apr_hdr hdr;
	uint32_t mem_handle;
	uint64_t enc_buf_addr;
	uint32_t enc_buf_size;
	uint64_t dec_buf_addr;
	uint32_t dec_buf_size;
} __packed;

struct vss_istream_cmd_set_packet_exchange_mode_t {
	struct apr_hdr hdr;
	uint32_t mode;
} __packed;

/* TO CVP commands */

#define VSS_IVOCPROC_CMD_CREATE_FULL_CONTROL_SESSION	0x000100C3
/**< Wait for APRV2_IBASIC_RSP_RESULT response. */

#define APRV2_IBASIC_CMD_DESTROY_SESSION		0x0001003C

#define VSS_IVOCPROC_CMD_SET_DEVICE_V2			0x000112C6

#define VSS_IVOCPROC_CMD_SET_VP3_DATA			0x000110EB

#define VSS_IVOLUME_CMD_SET_STEP			0x000112C2

#define VSS_IVOCPROC_CMD_ENABLE				0x000100C6
/**< No payload. Wait for APRV2_IBASIC_RSP_RESULT response. */

#define VSS_IVOCPROC_CMD_DISABLE			0x000110E1
/**< No payload. Wait for APRV2_IBASIC_RSP_RESULT response. */

/*
 * Registers the memory that contains device specific configuration data with
 * the vocproc. The client must register device configuration data with the
 * vocproc that corresponds with the device being set on the vocproc.
 */
#define VSS_IVOCPROC_CMD_REGISTER_DEVICE_CONFIG		0x00011371

/*
 * Deregisters the memory that holds device configuration data from the
  vocproc.
*/
#define VSS_IVOCPROC_CMD_DEREGISTER_DEVICE_CONFIG	0x00011372

#define VSS_IVOCPROC_CMD_REGISTER_CALIBRATION_DATA_V2	0x00011373
#define VSS_IVOCPROC_CMD_DEREGISTER_CALIBRATION_DATA	0x00011276

#define VSS_IVOCPROC_CMD_REGISTER_VOL_CALIBRATION_DATA	0x00011374
#define VSS_IVOCPROC_CMD_DEREGISTER_VOL_CALIBRATION_DATA	0x00011375

#define VSS_IVOCPROC_TOPOLOGY_ID_NONE			0x00010F70
#define VSS_IVOCPROC_TOPOLOGY_ID_TX_SM_ECNS		0x00010F71
#define VSS_IVOCPROC_TOPOLOGY_ID_TX_DM_FLUENCE		0x00010F72

#define VSS_IVOCPROC_TOPOLOGY_ID_RX_DEFAULT		0x00010F77

/* Newtwork IDs */
#define VSS_ICOMMON_CAL_NETWORK_ID_NONE		0x0001135E

/* Select internal mixing mode. */
#define VSS_IVOCPROC_VOCPROC_MODE_EC_INT_MIXING	0x00010F7C

/* Select external mixing mode. */
#define VSS_IVOCPROC_VOCPROC_MODE_EC_EXT_MIXING	0x00010F7D

/* Default AFE port ID. Applicable to Tx and Rx. */
#define VSS_IVOCPROC_PORT_ID_NONE			0xFFFF

#define VSS_NETWORK_ID_DEFAULT				0x00010037
#define VSS_NETWORK_ID_VOIP_NB				0x00011240
#define VSS_NETWORK_ID_VOIP_WB				0x00011241
#define VSS_NETWORK_ID_VOIP_WV				0x00011242

/* Media types */
#define VSS_MEDIA_ID_EVRC_MODEM		0x00010FC2
/* 80-VF690-47 CDMA enhanced variable rate vocoder modem format. */
#define VSS_MEDIA_ID_AMR_NB_MODEM	0x00010FC6
/* 80-VF690-47 UMTS AMR-NB vocoder modem format. */
#define VSS_MEDIA_ID_AMR_WB_MODEM	0x00010FC7
/* 80-VF690-47 UMTS AMR-WB vocoder modem format. */
#define VSS_MEDIA_ID_PCM_NB		0x00010FCB
#define VSS_MEDIA_ID_PCM_WB		0x00010FCC
/* Linear PCM (16-bit, little-endian). */
#define VSS_MEDIA_ID_G711_ALAW		0x00010FCD
/* G.711 a-law (contains two 10ms vocoder frames). */
#define VSS_MEDIA_ID_G711_MULAW		0x00010FCE
/* G.711 mu-law (contains two 10ms vocoder frames). */
#define VSS_MEDIA_ID_G729		0x00010FD0
/* G.729AB (contains two 10ms vocoder frames. */
#define VSS_MEDIA_ID_4GV_NB_MODEM	0x00010FC3
/*CDMA EVRC-B vocoder modem format */
#define VSS_MEDIA_ID_4GV_WB_MODEM	0x00010FC4
/*CDMA EVRC-WB vocoder modem format */
#define VSS_MEDIA_ID_4GV_NW_MODEM	0x00010FC5
/*CDMA EVRC-NW vocoder modem format */

#define VSS_IVOCPROC_CMD_CREATE_FULL_CONTROL_SESSION_V2	0x000112BF

struct vss_ivocproc_cmd_create_full_control_session_v2_t {
	uint16_t direction;
	/*
	 * Vocproc direction. The supported values:
	 * VSS_IVOCPROC_DIRECTION_RX
	 * VSS_IVOCPROC_DIRECTION_TX
	 * VSS_IVOCPROC_DIRECTION_RX_TX
	 */
	uint16_t tx_port_id;
	/*
	 * Tx device port ID to which the vocproc connects. If a port ID is
	 * not being supplied, set this to #VSS_IVOCPROC_PORT_ID_NONE.
	 */
	uint32_t tx_topology_id;
	/*
	 * Tx path topology ID. If a topology ID is not being supplied, set
	 * this to #VSS_IVOCPROC_TOPOLOGY_ID_NONE.
	 */
	uint16_t rx_port_id;
	/*
	 * Rx device port ID to which the vocproc connects. If a port ID is
	 * not being supplied, set this to #VSS_IVOCPROC_PORT_ID_NONE.
	 */
	uint32_t rx_topology_id;
	/*
	 * Rx path topology ID. If a topology ID is not being supplied, set
	 * this to #VSS_IVOCPROC_TOPOLOGY_ID_NONE.
	 */
	uint32_t profile_id;
	/* Voice calibration profile ID. */
	uint32_t vocproc_mode;
	/*
	 * Vocproc mode. The supported values:
	 * VSS_IVOCPROC_VOCPROC_MODE_EC_INT_MIXING
	 * VSS_IVOCPROC_VOCPROC_MODE_EC_EXT_MIXING
	 */
	uint16_t ec_ref_port_id;
	/*
	 * Port ID to which the vocproc connects for receiving echo
	 * cancellation reference signal. If a port ID is not being supplied,
	 * set this to #VSS_IVOCPROC_PORT_ID_NONE. This parameter value is
	 * ignored when the vocproc_mode parameter is set to
	 * VSS_IVOCPROC_VOCPROC_MODE_EC_INT_MIXING.
	 */
	char name[SESSION_NAME_LEN];
	/*
	 * Session name string used to identify a session that can be shared
	 * with passive controllers (optional). The string size, including the
	 * NULL termination character, is limited to 31 characters.
	 */
} __packed;

struct vss_ivocproc_cmd_set_volume_index_t {
	uint16_t vol_index;
	/*
	 * Volume index utilized by the vocproc to index into the volume table
	 * provided in VSS_IVOCPROC_CMD_CACHE_VOLUME_CALIBRATION_TABLE and set
	 * volume on the VDSP.
	 */
} __packed;

struct vss_ivolume_cmd_set_step_t {
	uint16_t direction;
	/*
	* The direction field sets the direction to apply the volume command.
	* The supported values:
	* #VSS_IVOLUME_DIRECTION_RX
	*/
	uint32_t value;
	/*
	* Volume step used to find the corresponding linear volume and
	* the best match index in the registered volume calibration table.
	*/
	uint16_t ramp_duration_ms;
	/*
	* Volume change ramp duration in milliseconds.
	* The supported values: 0 to 5000.
	*/
} __packed;

struct vss_ivocproc_cmd_set_device_v2_t {
	uint16_t tx_port_id;
	/*
	 * TX device port ID which vocproc will connect to.
	 * VSS_IVOCPROC_PORT_ID_NONE means vocproc will not connect to any port.
	 */
	uint32_t tx_topology_id;
	/*
	 * TX leg topology ID.
	 * VSS_IVOCPROC_TOPOLOGY_ID_NONE means vocproc does not contain any
	 * pre/post-processing blocks and is pass-through.
	 */
	uint16_t rx_port_id;
	/*
	 * RX device port ID which vocproc will connect to.
	 * VSS_IVOCPROC_PORT_ID_NONE means vocproc will not connect to any port.
	 */
	uint32_t rx_topology_id;
	/*
	 * RX leg topology ID.
	 * VSS_IVOCPROC_TOPOLOGY_ID_NONE means vocproc does not contain any
	 * pre/post-processing blocks and is pass-through.
	 */
	uint32_t vocproc_mode;
	/* Vocproc mode. The supported values:
	 * VSS_IVOCPROC_VOCPROC_MODE_EC_INT_MIXING - 0x00010F7C
	 * VSS_IVOCPROC_VOCPROC_MODE_EC_EXT_MIXING - 0x00010F7D
	 */
	uint16_t ec_ref_port_id;
	/* Port ID to which the vocproc connects for receiving
	 * echo
	 */
} __packed;

struct vss_ivocproc_cmd_register_device_config_t {
	uint32_t mem_handle;
	/*
	 * Handle to the shared memory that holds the per-network calibration
	 * data.
	 */
	uint64_t mem_address;
	/* Location of calibration data. */
	uint32_t mem_size;
	/* Size of the calibration data in bytes. */
} __packed;

struct vss_ivocproc_cmd_register_calibration_data_v2_t {
	uint32_t cal_mem_handle;
	/*
	 * Handle to the shared memory that holds the per-network calibration
	 * data.
	 */
	uint64_t cal_mem_address;
	/* Location of calibration data. */
	uint32_t cal_mem_size;
	/* Size of the calibration data in bytes. */
	uint8_t column_info[MAX_COL_INFO_SIZE];
	/*
	 * Column info contains the number of columns and the array of columns
	 * in the calibration table. The order in which the columns are provided
	 * here must match the order in which they exist in the calibration
	 * table provided.
	 */
} __packed;

struct vss_ivocproc_cmd_register_volume_cal_data_t {
	uint32_t cal_mem_handle;
	/*
	 * Handle to the shared memory that holds the volume calibration
	 * data.
	 */
	uint64_t cal_mem_address;
	/* Location of volume calibration data. */
	uint32_t cal_mem_size;
	/* Size of the volume calibration data in bytes. */
	uint8_t column_info[MAX_COL_INFO_SIZE];
	/*
	 * Column info contains the number of columns and the array of columns
	 * in the calibration table. The order in which the columns are provided
	 * here must match the order in which they exist in the calibration
	 * table provided.
	 */
} __packed;

/* Starts a vocoder PCM session */
#define VSS_IVPCM_CMD_START_V2	0x00011339

/* Default tap point location on the TX path. */
#define VSS_IVPCM_TAP_POINT_TX_DEFAULT	0x00011289

/* Default tap point location on the RX path. */
#define VSS_IVPCM_TAP_POINT_RX_DEFAULT	0x0001128A

/* Indicates tap point direction is output. */
#define VSS_IVPCM_TAP_POINT_DIR_OUT	0

/* Indicates tap point direction is input. */
#define VSS_IVPCM_TAP_POINT_DIR_IN	1

/* Indicates tap point direction is output and input. */
#define VSS_IVPCM_TAP_POINT_DIR_OUT_IN	2


#define VSS_IVPCM_SAMPLING_RATE_AUTO	0

/* Indicates 8 KHz vocoder PCM sampling rate. */
#define VSS_IVPCM_SAMPLING_RATE_8K	8000

/* Indicates 16 KHz vocoder PCM sampling rate. */
#define VSS_IVPCM_SAMPLING_RATE_16K	16000

/* RX and TX */
#define MAX_TAP_POINTS_SUPPORTED	1

struct vss_ivpcm_tap_point {
	uint32_t tap_point;
	uint16_t direction;
	uint16_t sampling_rate;
	uint16_t duration;
} __packed;


struct vss_ivpcm_cmd_start_v2_t {
	uint32_t mem_handle;
	uint32_t num_tap_points;
	struct vss_ivpcm_tap_point tap_points[MAX_TAP_POINTS_SUPPORTED];
} __packed;

#define VSS_IVPCM_EVT_PUSH_BUFFER_V2	0x0001133A

/* Push buffer event mask indicating output buffer is filled. */
#define VSS_IVPCM_PUSH_BUFFER_MASK_OUTPUT_BUFFER 1

/* Push buffer event mask indicating input buffer is consumed. */
#define VSS_IVPCM_PUSH_BUFFER_MASK_INPUT_BUFFER 2


struct vss_ivpcm_evt_push_buffer_v2_t {
	uint32_t tap_point;
	uint32_t push_buf_mask;
	uint64_t out_buf_mem_address;
	uint16_t out_buf_mem_size;
	uint64_t in_buf_mem_address;
	uint16_t in_buf_mem_size;
	uint16_t sampling_rate;
	uint16_t num_in_channels;
} __packed;

#define VSS_IVPCM_EVT_NOTIFY_V2 0x0001133B

/* Notify event mask indicates output buffer is filled. */
#define VSS_IVPCM_NOTIFY_MASK_OUTPUT_BUFFER 1

/* Notify event mask indicates input buffer is consumed. */
#define VSS_IVPCM_NOTIFY_MASK_INPUT_BUFFER 2

/* Notify event mask indicates a timetick */
#define VSS_IVPCM_NOTIFY_MASK_TIMETICK 4

/* Notify event mask indicates an error occured in output buffer operation */
#define VSS_IVPCM_NOTIFY_MASK_OUTPUT_ERROR 8

/* Notify event mask indicates an error occured in input buffer operation */
#define VSS_IVPCM_NOTIFY_MASK_INPUT_ERROR 16


struct vss_ivpcm_evt_notify_v2_t {
	uint32_t tap_point;
	uint32_t notify_mask;
	uint64_t out_buff_addr;
	uint64_t in_buff_addr;
	uint16_t filled_out_size;
	uint16_t request_buf_size;
	uint16_t sampling_rate;
	uint16_t num_out_channels;
} __packed;

struct cvp_start_cmd {
	struct apr_hdr hdr;
	struct vss_ivpcm_cmd_start_v2_t vpcm_start_cmd;
} __packed;

struct cvp_push_buf_cmd {
	struct apr_hdr hdr;
	struct vss_ivpcm_evt_push_buffer_v2_t vpcm_evt_push_buffer;
} __packed;

#define VSS_IVPCM_CMD_STOP 0x0001100B

struct cvp_create_full_ctl_session_cmd {
	struct apr_hdr hdr;
	struct vss_ivocproc_cmd_create_full_control_session_v2_t cvp_session;
} __packed;

struct cvp_command {
	struct apr_hdr hdr;
} __packed;

struct cvp_set_device_cmd {
	struct apr_hdr hdr;
	struct vss_ivocproc_cmd_set_device_v2_t cvp_set_device_v2;
} __packed;

struct cvp_set_vp3_data_cmd {
	struct apr_hdr hdr;
} __packed;

struct cvp_set_rx_volume_index_cmd {
	struct apr_hdr hdr;
	struct vss_ivocproc_cmd_set_volume_index_t cvp_set_vol_idx;
} __packed;

struct cvp_set_rx_volume_step_cmd {
	struct apr_hdr hdr;
	struct vss_ivolume_cmd_set_step_t cvp_set_vol_step;
} __packed;

struct cvp_register_dev_cfg_cmd {
	struct apr_hdr hdr;
	struct vss_ivocproc_cmd_register_device_config_t cvp_dev_cfg_data;
} __packed;

struct cvp_deregister_dev_cfg_cmd {
	struct apr_hdr hdr;
} __packed;

struct cvp_register_cal_data_cmd {
	struct apr_hdr hdr;
	struct vss_ivocproc_cmd_register_calibration_data_v2_t cvp_cal_data;
} __packed;

struct cvp_deregister_cal_data_cmd {
	struct apr_hdr hdr;
} __packed;

struct cvp_register_vol_cal_data_cmd {
	struct apr_hdr hdr;
	struct vss_ivocproc_cmd_register_volume_cal_data_t cvp_vol_cal_data;
} __packed;

struct cvp_deregister_vol_cal_data_cmd {
	struct apr_hdr hdr;
} __packed;

struct cvp_set_mute_cmd {
	struct apr_hdr hdr;
	struct vss_ivolume_cmd_mute_v2_t cvp_set_mute;
} __packed;

/* CB for up-link packets. */
typedef void (*ul_cb_fn)(uint8_t *voc_pkt,
			 uint32_t pkt_len,
			 uint32_t timestamp,
			 void *private_data);

/* CB for down-link packets. */
typedef void (*dl_cb_fn)(uint8_t *voc_pkt,
			 void *private_data);

/* CB for DTMF RX Detection */
typedef void (*dtmf_rx_det_cb_fn)(uint8_t *pkt,
				  char *session,
				  void *private_data);

typedef void (*hostpcm_cb_fn)(uint8_t *data,
			   char *session,
			   void *private_data);

struct mvs_driver_info {
	uint32_t media_type;
	uint32_t rate;
	uint32_t network_type;
	uint32_t dtx_mode;
	ul_cb_fn ul_cb;
	dl_cb_fn dl_cb;
	void *private_data;
	uint32_t evrc_min_rate;
	uint32_t evrc_max_rate;
};

struct dtmf_driver_info {
	dtmf_rx_det_cb_fn dtmf_rx_ul_cb;
	void *private_data;
};

struct hostpcm_driver_info {
	hostpcm_cb_fn hostpcm_evt_cb;
	void *private_data;
};

struct incall_rec_info {
	uint32_t rec_enable;
	uint32_t rec_mode;
	uint32_t recording;
};

struct incall_music_info {
	uint32_t play_enable;
	uint32_t playing;
	int count;
	int force;
	uint16_t port_id;
};

struct share_memory_info {
	u32			mem_handle;
	struct share_mem_buf	sh_buf;
	struct mem_map_table	memtbl;
};

struct voice_data {
	int voc_state;/*INIT, CHANGE, RELEASE, RUN */

	/* Shared mem to store decoder and encoder packets */
	struct share_memory_info	shmem_info;

	wait_queue_head_t mvm_wait;
	wait_queue_head_t cvs_wait;
	wait_queue_head_t cvp_wait;

	/* Cache the values related to Rx and Tx devices */
	struct device_data dev_rx;
	struct device_data dev_tx;

	/* Cache the values related to Rx and Tx streams */
	struct stream_data stream_rx;
	struct stream_data stream_tx;

	u32 mvm_state;
	u32 cvs_state;
	u32 cvp_state;

	/* Handle to MVM in the Q6 */
	u16 mvm_handle;
	/* Handle to CVS in the Q6 */
	u16 cvs_handle;
	/* Handle to CVP in the Q6 */
	u16 cvp_handle;

	struct mutex lock;

	uint16_t sidetone_gain;
	uint8_t tty_mode;
	/* slowtalk enable value */
	uint32_t st_enable;
	uint32_t dtmf_rx_detect_en;
	/* Local Call Hold mode */
	uint8_t lch_mode;

	struct voice_dev_route_state voc_route_state;

	u32 session_id;

	struct incall_rec_info rec_info;

	struct incall_music_info music_info;

	struct voice_rec_route_state rec_route_state;

	struct power_supply *psy;
};

struct cal_mem {
	struct ion_handle *handle;
	uint32_t phy;
	void *buf;
};

#define MAX_VOC_SESSIONS 6

struct common_data {
	/* these default values are for all devices */
	uint32_t default_mute_val;
	uint32_t default_sample_val;
	uint32_t default_vol_step_val;
	uint32_t default_vol_ramp_duration_ms;
	uint32_t default_mute_ramp_duration_ms;
	bool ec_ref_ext;
	uint16_t ec_port_id;

	/* APR to MVM in the Q6 */
	void *apr_q6_mvm;
	/* APR to CVS in the Q6 */
	void *apr_q6_cvs;
	/* APR to CVP in the Q6 */
	void *apr_q6_cvp;

	struct mem_map_table cal_mem_map_table;
	uint32_t cal_mem_handle;

	struct mem_map_table rtac_mem_map_table;
	uint32_t rtac_mem_handle;

	uint32_t voice_host_pcm_mem_handle;

	struct cal_mem cvp_cal;
	struct cal_mem cvs_cal;

	struct mutex common_lock;

	struct mvs_driver_info mvs_info;

	struct dtmf_driver_info dtmf_info;

	struct hostpcm_driver_info hostpcm_info;

	struct voice_data voice[MAX_VOC_SESSIONS];

	bool srvcc_rec_flag;
	bool is_destroy_cvd;
	bool is_vote_bms;
};

struct voice_session_itr {
	int cur_idx;
	int session_idx;
};

void voc_register_mvs_cb(ul_cb_fn ul_cb,
			dl_cb_fn dl_cb,
			void *private_data);

void voc_register_dtmf_rx_detection_cb(dtmf_rx_det_cb_fn dtmf_rx_ul_cb,
				       void *private_data);

void voc_config_vocoder(uint32_t media_type,
			uint32_t rate,
			uint32_t network_type,
			uint32_t dtx_mode,
			uint32_t evrc_min_rate,
			uint32_t evrc_max_rate);

enum {
	DEV_RX = 0,
	DEV_TX,
};

enum {
	RX_PATH = 0,
	TX_PATH,
};


#define VOC_PATH_PASSIVE 0
#define VOC_PATH_FULL 1
#define VOC_PATH_VOLTE_PASSIVE 2
#define VOC_PATH_VOICE2_PASSIVE 3
#define VOC_PATH_QCHAT_PASSIVE 4
#define VOC_PATH_VOWLAN_PASSIVE 5

#define MAX_SESSION_NAME_LEN 32
#define VOICE_SESSION_NAME   "Voice session"
#define VOIP_SESSION_NAME    "VoIP session"
#define VOLTE_SESSION_NAME   "VoLTE session"
#define VOICE2_SESSION_NAME  "Voice2 session"
#define QCHAT_SESSION_NAME   "QCHAT session"
#define VOWLAN_SESSION_NAME  "VoWLAN session"

#define VOICE2_SESSION_VSID_STR "10DC1000"
#define QCHAT_SESSION_VSID_STR "10803000"
#define VOWLAN_SESSION_VSID_STR "10002000"
#define VOICE_SESSION_VSID  0x10C01000
#define VOICE2_SESSION_VSID 0x10DC1000
#define VOLTE_SESSION_VSID  0x10C02000
#define VOIP_SESSION_VSID   0x10004000
#define QCHAT_SESSION_VSID  0x10803000
#define VOWLAN_SESSION_VSID 0x10002000
#define ALL_SESSION_VSID    0xFFFFFFFF
#define VSID_MAX            ALL_SESSION_VSID

#define APP_ID_MASK         0x3F000
#define APP_ID_SHIFT		12
enum vsid_app_type {
	VSID_APP_NONE = 0,
	VSID_APP_CS_VOICE = 1,
	VSID_APP_IMS = 2, /* IMS voice services covering VoLTE etc */
	VSID_APP_QCHAT = 3,
	VSID_APP_VOIP = 4, /* VoIP on AP HLOS without modem processor */
	VSID_APP_MAX,
};

/* called  by alsa driver */
int voc_set_pp_enable(uint32_t session_id, uint32_t module_id,
		      uint32_t enable);
int voc_get_pp_enable(uint32_t session_id, uint32_t module_id);
uint8_t voc_get_tty_mode(uint32_t session_id);
int voc_set_tty_mode(uint32_t session_id, uint8_t tty_mode);
int voc_start_voice_call(uint32_t session_id);
int voc_end_voice_call(uint32_t session_id);
int voc_standby_voice_call(uint32_t session_id);
int voc_resume_voice_call(uint32_t session_id);
int voc_set_lch(uint32_t session_id, enum voice_lch_mode lch_mode);
int voc_set_rxtx_port(uint32_t session_id,
		      uint32_t dev_port_id,
		      uint32_t dev_type);
int voc_set_rx_vol_step(uint32_t session_id, uint32_t dir, uint32_t vol_step,
			uint32_t ramp_duration);
int voc_set_tx_mute(uint32_t session_id, uint32_t dir, uint32_t mute,
		    uint32_t ramp_duration);
int voc_set_device_mute(uint32_t session_id, uint32_t dir, uint32_t mute,
			uint32_t ramp_duration);
int voc_get_rx_device_mute(uint32_t session_id);
int voc_set_route_flag(uint32_t session_id, uint8_t path_dir, uint8_t set);
uint8_t voc_get_route_flag(uint32_t session_id, uint8_t path_dir);
int voc_enable_dtmf_rx_detection(uint32_t session_id, uint32_t enable);
void voc_disable_dtmf_det_on_active_sessions(void);
int voc_alloc_cal_shared_memory(void);
int voc_alloc_voip_shared_memory(void);
int is_voc_initialized(void);
int voc_register_vocproc_vol_table(void);
int voc_deregister_vocproc_vol_table(void);
int voc_send_cvp_map_vocpcm_memory(uint32_t session_id,
				   struct mem_map_table *tp_mem_table,
				   phys_addr_t paddr, uint32_t bufsize);
int voc_send_cvp_unmap_vocpcm_memory(uint32_t session_id);
int voc_send_cvp_start_vocpcm(uint32_t session_id,
			      struct vss_ivpcm_tap_point *vpcm_tp,
			      uint32_t no_of_tp);
int voc_send_cvp_vocpcm_push_buf_evt(uint32_t session_id,
			struct vss_ivpcm_evt_push_buffer_v2_t *push_buff_evt);
int voc_send_cvp_stop_vocpcm(uint32_t session_id);
void voc_register_hpcm_evt_cb(hostpcm_cb_fn hostpcm_cb,
			      void *private_data);
void voc_deregister_hpcm_evt_cb(void);

int voc_unmap_cal_blocks(void);
int voc_map_rtac_block(struct rtac_cal_block_data *cal_block);
int voc_unmap_rtac_block(uint32_t *mem_map_handle);

uint32_t voc_get_session_id(char *name);

int voc_start_playback(uint32_t set, uint16_t port_id);
int voc_start_record(uint32_t port_id, uint32_t set, uint32_t session_id);
int voice_get_idx_for_session(u32 session_id);
int voc_set_ext_ec_ref(uint16_t port_id, bool state);
int voc_update_amr_vocoder_rate(uint32_t session_id);
int voc_disable_device(uint32_t session_id);
int voc_enable_device(uint32_t session_id);
void voc_set_destroy_cvd_flag(bool is_destroy_cvd);
void voc_set_vote_bms_flag(bool is_vote_bms);

#endif
