/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __QDSP6VOICE_H__
#define __QDSP6VOICE_H__

#include <mach/qdsp6v2/apr.h>

/* Device Event */
#define DEV_CHANGE_READY                0x1

#define VOICE_CALL_START        0x1
#define VOICE_CALL_END          0

#define VOICE_DEV_ENABLED       0x1
#define VOICE_DEV_DISABLED      0

#define MAX_VOC_PKT_SIZE 642

#define SESSION_NAME_LEN 20

struct voice_header {
	uint32_t id;
	uint32_t data_len;
};

struct voice_init {
	struct voice_header hdr;
	void *cb_handle;
};


/* Device information payload structure */

struct device_data {
	uint32_t dev_acdb_id;
	uint32_t volume; /* in percentage */
	uint32_t mute;
	uint32_t sample;
	uint32_t enabled;
	uint32_t dev_id;
	uint32_t dev_port_id;
};

enum {
	VOC_INIT = 0,
	VOC_RUN,
	VOC_CHANGE,
	VOC_RELEASE,
};

/* TO MVM commands */
#define VSS_IMVM_CMD_CREATE_PASSIVE_CONTROL_SESSION	0x000110FF
/**< No payload. Wait for APRV2_IBASIC_RSP_RESULT response. */

#define VSS_IMVM_CMD_CREATE_FULL_CONTROL_SESSION	0x000110FE
/* Create a new full control MVM session. */

#define APRV2_IBASIC_CMD_DESTROY_SESSION		0x0001003C
/**< No payload. Wait for APRV2_IBASIC_RSP_RESULT response. */

#define VSS_IMVM_CMD_ATTACH_STREAM			0x0001123C
/* Attach a stream to the MVM. */

#define VSS_IMVM_CMD_DETACH_STREAM			0x0001123D
/* Detach a stream from the MVM. */

#define VSS_IMVM_CMD_ATTACH_VOCPROC			0x0001123E
/* Attach a vocproc to the MVM. The MVM will symmetrically connect this vocproc
 * to all the streams currently attached to it.
 */

#define VSS_IMVM_CMD_DETACH_VOCPROC			0x0001123F
/* Detach a vocproc from the MVM. The MVM will symmetrically disconnect this
 * vocproc from all the streams to which it is currently attached.
 */

#define VSS_IMVM_CMD_START_VOICE			0x00011190
/**< No payload. Wait for APRV2_IBASIC_RSP_RESULT response. */

#define VSS_IMVM_CMD_STOP_VOICE				0x00011192
/**< No payload. Wait for APRV2_IBASIC_RSP_RESULT response. */

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

struct vss_imvm_cmd_create_control_session_t {
	char name[SESSION_NAME_LEN];
	/*
	 * A variable-sized stream name.
	 *
	 * The stream name size is the payload size minus the size of the other
	 * fields.
	 */
} __packed;

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
} __attribute__((packed));

struct vss_istream_cmd_attach_vocproc_t {
	uint16_t handle;
	/**< Handle of vocproc being attached. */
} __attribute__((packed));

struct vss_istream_cmd_detach_vocproc_t {
	uint16_t handle;
	/**< Handle of vocproc being detached. */
} __attribute__((packed));

struct vss_imvm_cmd_attach_stream_t {
	uint16_t handle;
	/* The stream handle to attach. */
} __attribute__((packed));

struct vss_imvm_cmd_detach_stream_t {
	uint16_t handle;
	/* The stream handle to detach. */
} __attribute__((packed));

struct vss_icommon_cmd_set_network_t {
	uint32_t network_id;
	/* Network ID. (Refer to VSS_NETWORK_ID_XXX). */
} __attribute__((packed));

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
} __attribute__((packed));

struct mvm_attach_vocproc_cmd {
	struct apr_hdr hdr;
	struct vss_istream_cmd_attach_vocproc_t mvm_attach_cvp_handle;
} __attribute__((packed));

struct mvm_detach_vocproc_cmd {
	struct apr_hdr hdr;
	struct vss_istream_cmd_detach_vocproc_t mvm_detach_cvp_handle;
} __attribute__((packed));

struct mvm_create_ctl_session_cmd {
	struct apr_hdr hdr;
	struct vss_imvm_cmd_create_control_session_t mvm_session;
} __packed;

struct mvm_set_tty_mode_cmd {
	struct apr_hdr hdr;
	struct vss_istream_cmd_set_tty_mode_t tty_mode;
} __attribute__((packed));

struct mvm_attach_stream_cmd {
	struct apr_hdr hdr;
	struct vss_imvm_cmd_attach_stream_t attach_stream;
} __attribute__((packed));

struct mvm_detach_stream_cmd {
	struct apr_hdr hdr;
	struct vss_imvm_cmd_detach_stream_t detach_stream;
} __attribute__((packed));

struct mvm_set_network_cmd {
	struct apr_hdr hdr;
	struct vss_icommon_cmd_set_network_t network;
} __attribute__((packed));

struct mvm_set_voice_timing_cmd {
	struct apr_hdr hdr;
	struct vss_icommon_cmd_set_voice_timing_t timing;
} __attribute__((packed));

/* TO CVS commands */
#define VSS_ISTREAM_CMD_CREATE_PASSIVE_CONTROL_SESSION	0x00011140
/**< Wait for APRV2_IBASIC_RSP_RESULT response. */

#define VSS_ISTREAM_CMD_CREATE_FULL_CONTROL_SESSION	0x000110F7
/* Create a new full control stream session. */

#define APRV2_IBASIC_CMD_DESTROY_SESSION		0x0001003C

#define VSS_ISTREAM_CMD_CACHE_CALIBRATION_DATA		0x000110FB

#define VSS_ISTREAM_CMD_SET_MUTE			0x00011022

#define VSS_ISTREAM_CMD_SET_MEDIA_TYPE			0x00011186
/* Set media type on the stream. */

#define VSS_ISTREAM_EVT_SEND_ENC_BUFFER			0x00011015
/* Event sent by the stream to its client to provide an encoded packet. */

#define VSS_ISTREAM_EVT_REQUEST_DEC_BUFFER		0x00011017
/* Event sent by the stream to its client requesting for a decoder packet.
 * The client should respond with a VSS_ISTREAM_EVT_SEND_DEC_BUFFER event.
 */

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

#define VSS_ISTREAM_CMD_START_RECORD			0x00011236
/* Start in-call conversation recording. */

#define VSS_ISTREAM_CMD_STOP_RECORD			0x00011237
/* Stop in-call conversation recording. */

#define VSS_ISTREAM_CMD_START_PLAYBACK			0x00011238
/* Start in-call music delivery on the Tx voice path. */

#define VSS_ISTREAM_CMD_STOP_PLAYBACK			0x00011239
/* Stop the in-call music delivery on the Tx voice path. */

struct vss_istream_cmd_create_passive_control_session_t {
	char name[SESSION_NAME_LEN];
	/**<
	* A variable-sized stream name.
	*
	* The stream name size is the payload size minus the size of the other
	* fields.
	*/
} __attribute__((packed));

struct vss_istream_cmd_set_mute_t {
	uint16_t direction;
	/**<
	* 0 : TX only
	* 1 : RX only
	* 2 : TX and Rx
	*/
	uint16_t mute_flag;
	/**<
	* Mute, un-mute.
	*
	* 0 : Silence disable
	* 1 : Silence enable
	* 2 : CNG enable. Applicable to TX only. If set on RX behavior
	*     will be the same as 1
	*/
} __attribute__((packed));

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
} __attribute__((packed));

struct vss_istream_cmd_set_media_type_t {
	uint32_t rx_media_id;
	/* Set the Rx vocoder type. (Refer to VSS_MEDIA_ID_XXX). */
	uint32_t tx_media_id;
	/* Set the Tx vocoder type. (Refer to VSS_MEDIA_ID_XXX). */
} __attribute__((packed));

struct vss_istream_evt_send_enc_buffer_t {
	uint32_t media_id;
      /* Media ID of the packet. */
	uint8_t packet_data[MAX_VOC_PKT_SIZE];
      /* Packet data buffer. */
} __attribute__((packed));

struct vss_istream_evt_send_dec_buffer_t {
	uint32_t media_id;
      /* Media ID of the packet. */
	uint8_t packet_data[MAX_VOC_PKT_SIZE];
      /* Packet data. */
} __attribute__((packed));

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
} __attribute__((packed));

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
} __attribute__((packed));

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
} __attribute__((packed));

struct vss_istream_cmd_set_enc_dtx_mode_t {
	uint32_t enable;
	/* Toggle DTX on or off.
	 *
	 * 0 : Disables DTX
	 * 1 : Enables DTX
	 */
} __attribute__((packed));

#define VSS_TAP_POINT_NONE				0x00010F78
/* Indicates no tapping for specified path. */

#define VSS_TAP_POINT_STREAM_END			0x00010F79
/* Indicates that specified path should be tapped at the end of the stream. */

struct vss_istream_cmd_start_record_t {
	uint32_t rx_tap_point;
	/* Tap point to use on the Rx path. Supported values are:
	 * VSS_TAP_POINT_NONE : Do not record Rx path.
	 * VSS_TAP_POINT_STREAM_END : Rx tap point is at the end of the stream.
	 */
	uint32_t tx_tap_point;
	/* Tap point to use on the Tx path. Supported values are:
	 * VSS_TAP_POINT_NONE : Do not record tx path.
	 * VSS_TAP_POINT_STREAM_END : Tx tap point is at the end of the stream.
	 */
} __attribute__((packed));

struct cvs_create_passive_ctl_session_cmd {
	struct apr_hdr hdr;
	struct vss_istream_cmd_create_passive_control_session_t cvs_session;
} __attribute__((packed));

struct cvs_create_full_ctl_session_cmd {
	struct apr_hdr hdr;
	struct vss_istream_cmd_create_full_control_session_t cvs_session;
} __attribute__((packed));

struct cvs_destroy_session_cmd {
	struct apr_hdr hdr;
} __attribute__((packed));

struct cvs_cache_calibration_data_cmd {
	struct apr_hdr hdr;
} __attribute__ ((packed));

struct cvs_set_mute_cmd {
	struct apr_hdr hdr;
	struct vss_istream_cmd_set_mute_t cvs_set_mute;
} __attribute__((packed));

struct cvs_set_media_type_cmd {
	struct apr_hdr hdr;
	struct vss_istream_cmd_set_media_type_t media_type;
} __attribute__((packed));

struct cvs_send_dec_buf_cmd {
	struct apr_hdr hdr;
	struct vss_istream_evt_send_dec_buffer_t dec_buf;
} __attribute__((packed));

struct cvs_set_amr_enc_rate_cmd {
	struct apr_hdr hdr;
	struct vss_istream_cmd_voc_amr_set_enc_rate_t amr_rate;
} __attribute__((packed));

struct cvs_set_amrwb_enc_rate_cmd {
	struct apr_hdr hdr;
	struct vss_istream_cmd_voc_amrwb_set_enc_rate_t amrwb_rate;
} __attribute__((packed));

struct cvs_set_cdma_enc_minmax_rate_cmd {
	struct apr_hdr hdr;
	struct vss_istream_cmd_cdma_set_enc_minmax_rate_t cdma_rate;
} __attribute__((packed));

struct cvs_set_enc_dtx_mode_cmd {
	struct apr_hdr hdr;
	struct vss_istream_cmd_set_enc_dtx_mode_t dtx_mode;
} __attribute__((packed));

struct cvs_start_record_cmd {
		struct apr_hdr hdr;
		struct vss_istream_cmd_start_record_t rec_mode;
} __attribute__((packed));

/* TO CVP commands */

#define VSS_IVOCPROC_CMD_CREATE_FULL_CONTROL_SESSION	0x000100C3
/**< Wait for APRV2_IBASIC_RSP_RESULT response. */

#define APRV2_IBASIC_CMD_DESTROY_SESSION		0x0001003C

#define VSS_IVOCPROC_CMD_SET_DEVICE			0x000100C4

#define VSS_IVOCPROC_CMD_CACHE_CALIBRATION_DATA		0x000110E3

#define VSS_IVOCPROC_CMD_CACHE_VOLUME_CALIBRATION_TABLE	0x000110E4

#define VSS_IVOCPROC_CMD_SET_VP3_DATA			0x000110EB

#define VSS_IVOCPROC_CMD_SET_RX_VOLUME_INDEX		0x000110EE

#define VSS_IVOCPROC_CMD_ENABLE				0x000100C6
/**< No payload. Wait for APRV2_IBASIC_RSP_RESULT response. */

#define VSS_IVOCPROC_CMD_DISABLE			0x000110E1
/**< No payload. Wait for APRV2_IBASIC_RSP_RESULT response. */

#define VSS_IVOCPROC_TOPOLOGY_ID_NONE			0x00010F70
#define VSS_IVOCPROC_TOPOLOGY_ID_TX_SM_ECNS		0x00010F71
#define VSS_IVOCPROC_TOPOLOGY_ID_TX_DM_FLUENCE		0x00010F72

#define VSS_IVOCPROC_TOPOLOGY_ID_RX_DEFAULT		0x00010F77

/* Newtwork IDs */
#define VSS_NETWORK_ID_DEFAULT				0x00010037
#define VSS_NETWORK_ID_VOIP_NB				0x00011240
#define VSS_NETWORK_ID_VOIP_WB				0x00011241
#define VSS_NETWORK_ID_VOIP_WV				0x00011242

/* Media types */
#define VSS_MEDIA_ID_13K_MODEM		0x00010FC1
/* Qcelp vocoder modem format */
#define VSS_MEDIA_ID_EVRC_MODEM		0x00010FC2
/* 80-VF690-47 CDMA enhanced variable rate vocoder modem format. */
#define VSS_MEDIA_ID_4GV_NB_MODEM  0x00010FC3
/* 4GV Narrowband modem format */
#define VSS_MEDIA_ID_4GV_WB_MODEM  0x00010FC4
/* 4GV Wideband modem format */
#define VSS_MEDIA_ID_AMR_NB_MODEM	0x00010FC6
/* 80-VF690-47 UMTS AMR-NB vocoder modem format. */
#define VSS_MEDIA_ID_AMR_WB_MODEM	0x00010FC7
/* 80-VF690-47 UMTS AMR-WB vocoder modem format. */
#define VSS_MEDIA_ID_EFR_MODEM		0x00010FC8
/*EFR modem format */
#define VSS_MEDIA_ID_FR_MODEM		0x00010FC9
/*FR modem format */
#define VSS_MEDIA_ID_HR_MODEM		0x00010FCA
/*HR modem format */
#define VSS_MEDIA_ID_PCM_NB		0x00010FCB
/* Linear PCM (16-bit, little-endian). */
#define VSS_MEDIA_ID_PCM_WB		0x00010FCC
/* Linear wideband PCM vocoder modem format (16 bits, little endian). */
#define VSS_MEDIA_ID_G711_ALAW		0x00010FCD
/* G.711 a-law (contains two 10ms vocoder frames). */
#define VSS_MEDIA_ID_G711_MULAW		0x00010FCE
/* G.711 mu-law (contains two 10ms vocoder frames). */
#define VSS_MEDIA_ID_G729		0x00010FD0
/* G.729AB (contains two 10ms vocoder frames. */

#define VOICE_CMD_SET_PARAM				0x00011006
#define VOICE_CMD_GET_PARAM				0x00011007
#define VOICE_EVT_GET_PARAM_ACK				0x00011008

struct vss_ivocproc_cmd_create_full_control_session_t {
	uint16_t direction;
	/*
	 * stream direction.
	 * 0 : TX only
	 * 1 : RX only
	 * 2 : TX and RX
	 */
	uint32_t tx_port_id;
	/*
	 * TX device port ID which vocproc will connect to. If not supplying a
	 * port ID set to VSS_IVOCPROC_PORT_ID_NONE.
	 */
	uint32_t tx_topology_id;
	/*
	 * Tx leg topology ID. If not supplying a topology ID set to
	 * VSS_IVOCPROC_TOPOLOGY_ID_NONE.
	 */
	uint32_t rx_port_id;
	/*
	 * RX device port ID which vocproc will connect to. If not supplying a
	 * port ID set to VSS_IVOCPROC_PORT_ID_NONE.
	 */
	uint32_t rx_topology_id;
	/*
	 * Rx leg topology ID. If not supplying a topology ID set to
	 * VSS_IVOCPROC_TOPOLOGY_ID_NONE.
	 */
	int32_t network_id;
	/*
	 * Network ID. (Refer to VSS_NETWORK_ID_XXX). If not supplying a network
	 * ID set to VSS_NETWORK_ID_DEFAULT.
	 */
} __attribute__((packed));

struct vss_ivocproc_cmd_set_device_t {
	uint32_t tx_port_id;
	/**<
	* TX device port ID which vocproc will connect to.
	* VSS_IVOCPROC_PORT_ID_NONE means vocproc will not connect to any port.
	*/
	uint32_t tx_topology_id;
	/**<
	* TX leg topology ID.
	* VSS_IVOCPROC_TOPOLOGY_ID_NONE means vocproc does not contain any
	* pre/post-processing blocks and is pass-through.
	*/
	int32_t rx_port_id;
	/**<
	* RX device port ID which vocproc will connect to.
	* VSS_IVOCPROC_PORT_ID_NONE means vocproc will not connect to any port.
	*/
	uint32_t rx_topology_id;
	/**<
	* RX leg topology ID.
	* VSS_IVOCPROC_TOPOLOGY_ID_NONE means vocproc does not contain any
	* pre/post-processing blocks and is pass-through.
	*/
} __attribute__((packed));

struct vss_ivocproc_cmd_set_volume_index_t {
	uint16_t vol_index;
	/**<
	* Volume index utilized by the vocproc to index into the volume table
	* provided in VSS_IVOCPROC_CMD_CACHE_VOLUME_CALIBRATION_TABLE and set
	* volume on the VDSP.
	*/
} __attribute__((packed));

struct cvp_create_full_ctl_session_cmd {
	struct apr_hdr hdr;
	struct vss_ivocproc_cmd_create_full_control_session_t cvp_session;
} __attribute__ ((packed));

struct cvp_command {
	struct apr_hdr hdr;
} __attribute__((packed));

struct cvp_set_device_cmd {
	struct apr_hdr hdr;
	struct vss_ivocproc_cmd_set_device_t cvp_set_device;
} __attribute__ ((packed));

struct cvp_cache_calibration_data_cmd {
	struct apr_hdr hdr;
} __attribute__((packed));

struct cvp_cache_volume_calibration_table_cmd {
	struct apr_hdr hdr;
} __attribute__((packed));

struct cvp_set_vp3_data_cmd {
	struct apr_hdr hdr;
} __attribute__((packed));

struct cvp_set_rx_volume_index_cmd {
	struct apr_hdr hdr;
	struct vss_ivocproc_cmd_set_volume_index_t cvp_set_vol_idx;
} __attribute__((packed));

/* CB for up-link packets. */
typedef void (*ul_cb_fn)(uint8_t *voc_pkt,
			 uint32_t pkt_len,
			 void *private_data);

/* CB for down-link packets. */
typedef void (*dl_cb_fn)(uint8_t *voc_pkt,
			 uint32_t *pkt_len,
			 void *private_data);

struct q_min_max_rate {
	uint32_t min_rate;
	uint32_t max_rate;
};

struct mvs_driver_info {
	uint32_t media_type;
	uint32_t rate;
	uint32_t network_type;
	uint32_t dtx_mode;
	struct q_min_max_rate q_min_max_rate;
	ul_cb_fn ul_cb;
	dl_cb_fn dl_cb;
	void *private_data;
};

struct incall_rec_info {
	uint32_t pending;
	uint32_t rec_mode;
};

struct incall_music_info {
	uint32_t pending;
	uint32_t playing;
};

struct voice_data {
	int voc_state;/*INIT, CHANGE, RELEASE, RUN */

	wait_queue_head_t mvm_wait;
	wait_queue_head_t cvs_wait;
	wait_queue_head_t cvp_wait;

	/* cache the values related to Rx and Tx */
	struct device_data dev_rx;
	struct device_data dev_tx;

	/* call status */
	int v_call_status; /* Start or End */

	u32 mvm_state;
	u32 cvs_state;
	u32 cvp_state;

	/* Handle to MVM */
	u16 mvm_handle;
	/* Handle to CVS */
	u16 cvs_handle;
	/* Handle to CVP */
	u16 cvp_handle;

	struct mutex lock;

	struct incall_rec_info rec_info;

	struct incall_music_info music_info;

	u16 session_id;
};

#define MAX_VOC_SESSIONS 2
#define SESSION_ID_BASE 0xFFF0

struct common_data {
	uint32_t voc_path;
	uint32_t adsp_version;
	uint32_t device_events;

	/* These default values are for all devices */
	uint32_t default_mute_val;
	uint32_t default_vol_val;
	uint32_t default_sample_val;

	/* APR to MVM in the modem */
	void *apr_mvm;
	/* APR to CVS in the modem */
	void *apr_cvs;
	/* APR to CVP in the modem */
	void *apr_cvp;

	/* APR to MVM in the Q6 */
	void *apr_q6_mvm;
	/* APR to CVS in the Q6 */
	void *apr_q6_cvs;
	/* APR to CVP in the Q6 */
	void *apr_q6_cvp;

	struct mutex common_lock;

	struct mvs_driver_info mvs_info;

	struct voice_data voice[MAX_VOC_SESSIONS];
};

int voice_set_voc_path_full(uint32_t set);

void voice_register_mvs_cb(ul_cb_fn ul_cb,
			   dl_cb_fn dl_cb,
			   void *private_data);

void voice_config_vocoder(uint32_t media_type,
			  uint32_t rate,
			  uint32_t network_type,
			  uint32_t dtx_mode,
			  struct q_min_max_rate q_min_max_rate);

int voice_start_record(uint32_t rec_mode, uint32_t set);

int voice_start_playback(uint32_t set);

u16 voice_get_session_id(const char *name);
#endif
