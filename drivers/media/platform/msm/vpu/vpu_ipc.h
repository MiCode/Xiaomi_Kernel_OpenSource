/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#ifndef __H_VPU_IPC_H__
#define __H_VPU_IPC_H__

#include "vpu_property.h"

/* error code from FW */
#define VPU_STS_SUCCESS                              0
#define VPU_STS_EFAILED                              1
#define VPU_STS_EALREADYREGISTERED                   2
#define VPU_STS_EINVALIDPARAM                        3
#define VPU_STS_ETIMEOUT                             4
#define VPU_STS_ERESOURCENOTFOUND                    5
#define VPU_STS_EFEATURENOTSUPPORTED                 6
#define VPU_STS_EMAXIMUMREACHED                      7
#define VPU_STS_EINTERNALERROR                       8
#define VPU_STS_EPRECONDITIONVIOLATED                9
#define VPU_STS_EINSUFFICIENTRESOURCEAMOUNT          10
#define VPU_STS_EINSUFFICIENTRESOURCEAMOUNTTOTAL     11
#define VPU_STS_EOSALLOCFAILURE                      12
#define VPU_STS_EREQUESTPENDING                      13
#define VPU_STS_EINSUFFICIENTPRIORITY                14
#define VPU_STS_ERESOURCEINUSE                       15
#define VPU_STS_ENULLPOINTER                         16
#define VPU_STS_EALREADYINITIALIZED                  17
#define VPU_STS_ENOTINITIALIZED                      18
#define VPU_STS_EGENERALERROR                        19
#define VPU_STS_EINVALIDHANDLE                       20
#define VPU_STS_EINVALIDCLIENTID                     21
#define VPU_STS_EINSUFFICIENTMEMAVAILABLE            22
#define VPU_STS_EBADSTATE                            23
#define VPU_STS_EFRAMEUNPROCESSED                    24

/* Bit fields */
#define VPU_IPC_BUFFER_RETURN_TO_HOST_BIT	0x0
#define VPU_IPC_BUFFER_MARK_BIT			0x1

/* Specifies buffers are to returned to host upon completion of processing */
#define VPU_IPC_BUFFER_FLAG_RETURN_TO_HOST	\
					(0x1<<VPU_IPC_BUFFER_RETURN_TO_HOST_BIT)

#define VPU_IPC_BUFFER_FLAG_MARK		(0x1<<VPU_IPC_BUFFER_MARK_BIT)

/* Maximum number of supported input/output buffers */
#define MAX_NUM_INPUT_BUFF			32
#define MAX_NUM_OUTPUT_BUFF			32

/*
 * The following values define all the interface commands between Host and FW.
 *
 * VPU_IPC_CMD_SYS_SET_PROPERTY		: Sets system properties like logging
 *					  configuration, swapping between main
 *					  and secondary images
 * VPU_IPC_CMD_SYS_GET_PROPERTY		: Gets system properties like hardware
 *					  versioning, host ID
 * VPU_IPC_CMD_SYS_SESSION_OPEN		: Creates a new session
 * VPU_IPC_CMD_SYS_SESSION_CLOSE	: Terminates an existing session
 * VPU_IPC_CMD_SESSION_START		: Triggers the session for packet
 *					  exchanges
 * VPU_IPC_CMD_SESSION_STOP		: Stops all operations within the
 *					  session
 * VPU_IPC_CMD_SESSION_PAUSE		: Commands the current session to be
 *					  suspended
 * VPU_IPC_CMD_SESSION_SET_PROPERTY	: Sets a session specific property
 * VPU_IPC_CMD_SESSION_GET_PROPERTY	: Gets a session specific property
 * VPU_IPC_CMD_SESSION_SET_BUFFERS	: Commands the firmware to set up the
 *					  given buffers as input or output
 *					  buffers, only applicable for tunneling
 *					  case
 * VPU_IPC_CMD_SESSION_PROCESS_BUFFERS  : Commands the firmware to process the
 *					  given input buffers, only applicable
 *					  for non-tunneling case
 * VPU_IPC_CMD_SESSION_RELEASE_BUFFERS	: Releases the specified buffer
 *					  currently being used in this video
 *					  processing session to Host
 * VPU_IPC_CMD_SESSION_FLUSH		: Commands the firmware to stop
 *					  processing the current buffers and
 */
#define VPU_IPC_CMD_SYS_SET_PROPERTY                    0x00000001
#define VPU_IPC_CMD_SYS_GET_PROPERTY                    0x00000002
#define VPU_IPC_CMD_SYS_SESSION_OPEN                    0x00000003
#define VPU_IPC_CMD_SYS_SESSION_CLOSE                   0x00000004
#define VPU_IPC_CMD_SYS_SHUTDOWN                        0x00000005
#define VPU_IPC_CMD_SYS_GENERIC                         0x00000ffe

#define VPU_IPC_CMD_SESSION_START                       0x00001001
#define VPU_IPC_CMD_SESSION_STOP                        0x00001002
#define VPU_IPC_CMD_SESSION_PAUSE                       0x00001003
#define VPU_IPC_CMD_SESSION_SET_PROPERTY                0x00001004
#define VPU_IPC_CMD_SESSION_GET_PROPERTY                0x00001005
#define VPU_IPC_CMD_SESSION_SET_BUFFERS                 0x00001006
#define VPU_IPC_CMD_SESSION_PROCESS_BUFFERS             0x00001007
#define VPU_IPC_CMD_SESSION_RELEASE_BUFFERS             0x00001008
#define VPU_IPC_CMD_SESSION_FLUSH                       0x00001009
#define VPU_IPC_CMD_SESSION_GENERIC                     0x00001ffe

/*
 * The following values define all the interface messages between FW and Host.
 *
 * VPU_IPC_MSG_SYS_SET_PROPERTY_DONE:
 * Notifies the Host that setting a property is done
 *
 * VPU_IPC_MSG_SYS_PROPERTY_INFO:
 * Notifies the Host with the requested system property info
 *
 * VPU_IPC_MSG_SYS_SESSION_OPEN_DONE:
 * Notifies the Host that the session is successfully created and initialized
 *
 * VPU_IPC_MSG_SYS_SESSION_CLOSE_DONE:
 * Notifies the Host that the session is successfully closed and de-initialized
 *
 * VPU_IPC_MSG_SESSION_START_DONE:
 * Notifies the Host that the session is successfully started
 *
 * VPU_IPC_MSG_SESSION_STOP_DONE:
 * Notifies the Host that the session is successfully stopped
 *
 * VPU_IPC_MSG_SESSION_PAUSE_DONE:
 * Notifies the Host that the session has been suspended.
 * No further output buffers will be delivered for this session until a resume
 * command is issued by Host
 *
 * VPU_IPC_MSG_SESSION_PROPERTY_INFO:
 * Notifies the Host with the requested property information for the session
 *
 * VPU_IPC_MSG_SESSION_SET_BUFFERS_DONE:
 * Notifies the Host that the associated input or output buffers have been
 * set by the Firmware.  Only applicable for tunneling case.
 *
 * VPU_IPC_MSG_SESSION_PROCESS_BUFFERS_DONE:
 * Notifies the Host that the associated buffers have been processed by the FW.
 * It can now be used to fill with new data by the Host (non-tunneling case).
 *
 * VPU_IPC_MSG_SESSION_FLUSH_DONE:
 * Notifies the Host that the flush processing is successfully completed
 *
 * VPU_IPC_MSG_SYS_EVENT_NOTIFY:
 * VPU_IPC_MSG_SESSION_EVENT_NOTIFY:
 * Notifies the Host of a particular event
 */
#define VPU_IPC_MSG_SYS_BASE                            0x00010000
#define VPU_IPC_MSG_SYS_SET_PROPERTY_DONE               0x00010001
#define VPU_IPC_MSG_SYS_PROPERTY_INFO                   0x00010002
#define VPU_IPC_MSG_SYS_SESSION_OPEN_DONE               0x00010003
#define VPU_IPC_MSG_SYS_SESSION_CLOSE_DONE              0x00010004

#define VPU_IPC_MSG_SYS_EVENT_NOTIFY                    0x00010020
#define VPU_IPC_MSG_SYS                                 0x0001fffe
#define VPU_IPC_MSG_SYS_END                             0x0001ffff

#define VPU_IPC_MSG_SESSION_BASE                        0x00011000
#define VPU_IPC_MSG_SESSION_START_DONE                  0x00011001
#define VPU_IPC_MSG_SESSION_STOP_DONE                   0x00011002
#define VPU_IPC_MSG_SESSION_PAUSE_DONE                  0x00011003
#define VPU_IPC_MSG_SESSION_SET_PROPERTY_DONE           0x00011004
#define VPU_IPC_MSG_SESSION_PROPERTY_INFO               0x00011005
#define VPU_IPC_MSG_SESSION_SET_BUFFERS_DONE            0x00011006
#define VPU_IPC_MSG_SESSION_PROCESS_BUFFERS_DONE        0x00011007
#define VPU_IPC_MSG_SESSION_RELEASE_BUFFERS_DONE        0x00011008
#define VPU_IPC_MSG_SESSION_FLUSH_DONE                  0x00011009

#define VPU_IPC_MSG_SESSION_ACTIVE_REGION               0x00011020
#define VPU_IPC_MSG_SESSION_EVENT_NOTIFY                0x00011021

#define VPU_IPC_MSG_SESSION                             0x00011ffe
#define VPU_IPC_MSG_SESSION_END                         0x00011fff

#define VPU_IPC_MSG_RESERVED_BASE                       0x40000000
#define VPU_IPC_MSG_RESERVED_END                        0x4fffffff
#define VPU_IPC_MSG_MAX                                 0x7fffffff

/* The channel_port_t enumeration defines all the possible VPU ports */
enum vpu_ipc_channel_port {
	VPU_IPC_PORT_UNUSED	= 0,
	VPU_IPC_PORT_INPUT	= 1,
	VPU_IPC_PORT_OUTPUT	= 2,
	VPU_IPC_PORT_OUTPUT2	= 3,
	VPU_IPC_PORT_NUM,
};

/* The vpu_ipc_buffer enumeration defines all the possible buffer types. */
enum vpu_ipc_buffer {
	VPU_IPC_BUFFER_INPUT	= 0,
	VPU_IPC_BUFFER_OUTPUT	= 1,
	VPU_IPC_BUFFER_NR	= 2,

	VPU_IPC_BUFFER_NUM,
	VPU_IPC_BUFFER_MAX	= 0x7fffffff
};

/* The vpu_ipc_flush enumeration defines all the possible types of flush. */
enum vpu_ipc_flush {
	VPU_IPC_FLUSH_INPUT	= 0,
	VPU_IPC_FLUSH_OUTPUT	= 1,
	VPU_IPC_FLUSH_ALL	= 2,

	VPU_IPC_FLUSH_NUM,
	VPU_IPC_FLUSH_MAX	= 0x7fffffff
};

/*
 * struct vpu_ipc_cmd_header_packet command packet definition
 * size:	size of the packet in bytes
 * cmd_id:	session or System command type
 * transId:	transaction ID to uniquely identify the cmd. Caller is
 *		responsible for providing a unique value. transId is returned in
 *		as part of acknowledgment message
 * sid:		unique ID associated with a session (only valid for session
 *		event)
 * port_id:	ID of the port (input/output) that command applies to (value
 *		from enum vpu_ipc_channel_port)
 * flags:	bit 0:  indication of need for acknowledgment from FW
 *		bit 1-31: reserved
 */
struct vpu_ipc_cmd_header_packet {
	u32	size;
	u32	cmd_id;
	u32	trans_id;
	u32	sid;
	u32	port_id;
	u32	flags;
};

/*
 * struct vpu_ipc_msg_header_packet message packet definition
 * size:	size of the packet in bytes
 * msg_id:	session or System message type
 * status:	return status from FW
 * trans_id:	see vpu_ipc_cmd_header_packet.trans_id
 * port_id:	ID of the port (input/output) that command applies to (value
 *		from enum vpu_ipc_channel_port)
 * sid:		unique ID associated with a session (only valid for session
 *		event)
 */
struct vpu_ipc_msg_header_packet {
	u32	size;
	u32	msg_id;
	u32	status;
	u32	trans_id;
	u32	sid;
	u32	port_id;
	u32	reserved;
};

/*
 * VPU_IPC_CMD_SYS_SET_PROPERTY command packet definition
 * prop_id	: property identifier
 * data_offset	: pointer to property data associated with name
 * data_size	: size of property data structure in bytes
 */
struct vpu_ipc_cmd_sys_set_property_packet {
	struct vpu_ipc_cmd_header_packet hdr;
	u32 prop_id;
	u32 data_offset;
	u32 data_size;
	u32 reserved;
};

/*
 * VPU_IPC_CMD_SYS_GET_PROPERTY command packet definition
 * prop_id	: property identifier
 * data_offset	: pointer to property data associated with name
 * data_size	: size of property data structure in bytes
 */
struct vpu_ipc_cmd_sys_get_property_packet {
	struct vpu_ipc_cmd_header_packet hdr;
	u32 prop_id;
	u32 data_offset;
	u32 data_size;
	u32 reserved;
};

/*
 * VPU_IPC_CMD_SYS_SESSION_OPEN command packet definition
 */
struct vpu_ipc_cmd_session_open_packet {
	struct vpu_ipc_cmd_header_packet hdr;
	u32	session_priority;
	u32	cmd_queue_id;
	u32	msg_queue_id;
};

/*
 * VPU_IPC_CMD_SYS_SESSION_CLOSE command packet definition
 */
struct vpu_ipc_cmd_session_close_packet {
	struct vpu_ipc_cmd_header_packet hdr;
};

/*
 * VPU_IPC_CMD_SYS_GENERIC command packet definition
 */
struct vpu_ipc_cmd_sys_generic_packet {
	struct vpu_ipc_cmd_header_packet hdr;
	struct vpu_data_pkt data;
};

/*
 * VPU_IPC_CMD_SESSION_START command packet definition
 */
struct vpu_ipc_cmd_session_start_packet {
	struct vpu_ipc_cmd_header_packet hdr;
};

/*
 * VPU_IPC_CMD_SESSION_STOP command packet definition
 */
struct vpu_ipc_cmd_session_stop_packet {
	struct vpu_ipc_cmd_header_packet hdr;
};

/*
 * VPU_IPC_CMD_SESSION_PAUSE command packet definition
 */
struct vpu_ipc_cmd_session_pause_packet {
	struct vpu_ipc_cmd_header_packet hdr;
};

/*
 * VPU_IPC_CMD_SESSION_SET_PROPERTY command packet definition
 * prop_id	: property identifier
 * data_offset	: pointer to property data associated with name
 * data_size	: size of property data structure in bytes
 */
struct vpu_ipc_cmd_session_set_property_packet {
	struct vpu_ipc_cmd_header_packet hdr;
	u32	prop_id;
	u32	data_offset;
	u32	data_size;
	u32	reserved;
};

/*
 * VPU_IPC_CMD_SESSION_GET_PROPERTY command packet definition
 * prop_id	: property identifier
 * data_offset	: pointer to property data associated with name
 * data_size	: size of property data structure in bytes
 */
struct vpu_ipc_cmd_session_get_property_packet {
	struct vpu_ipc_cmd_header_packet hdr;
	u32	prop_id;
	u32	data_offset;
	u32	data_size;
	u32	reserved;
};

/*
 * buffer info structure
 * normally carried by vpu_ipc_cmd_session_buffers_packet
 */
struct vpu_ipc_buf_info {
	u32	timestamp_hi;
	u32	timestamp_lo;
	u32	tag;
	/* number bytes of address data appended after this structure  */
	u32	buf_addr_size;
	u32	flag; /* reserved */
};

/*
 * If the meta_data present flag is set, then this struct is included at the
 * end of each struct vpu_ipc_buf_info
 */
struct vpu_ipc_buffer_meta_data_t {
	/* total size in bytes (size of nSize + payload size) */
	u32	size;
	/* variable payload starting at payload[0] */
	u32	payload[1];
};

#define BUFFER_PKT_FLAG_BUFFER_TYPE_MASK            0x00000007
#define BUFFER_PKT_FLAG_BUFFER_TYPE_SHIFT           0x00000000
#define BUFFER_PKT_FLAG_PROGRESSIVE_FRAME           0
#define BUFFER_PKT_FLAG_INTERLEAVED_FRAME_TOP_FIRST 1
#define BUFFER_PKT_FLAG_INTERLEAVED_FRAME_BOT_FIRST 2
#define BUFFER_PKT_FLAG_INTERLACED_TOP_FIRST        3
#define BUFFER_PKT_FLAG_INTERLACED_BOT_FIRST        4
#define BUFFER_PKT_FLAG_SINGLE_FIELD_TOP            5
#define BUFFER_PKT_FLAG_SINGLE_FIELD_BOT            6
/* Bits: 7 - Start index */
#define BUFFER_PKT_FLAG_START_INDEX_MASK            0x00000080
#define BUFFER_PKT_FLAG_START_INDEX_SHIFT           7
/* Bits: 8 - Input Src/sink addresses present */
#define BUFFER_PKT_FLAG_IN_SRC_SINK                 0x00000100
#define BUFFER_PKT_FLAG_IN_SRC_SINK_MASK            0x00000100
#define BUFFER_PKT_FLAG_IN_SRC_SINK_SHIFT           8
/* Bits: 9 - Output Src/sink addresses present */
#define BUFFER_PKT_FLAG_OUT_SRC_SINK                0x00000200
#define BUFFER_PKT_FLAG_OUT_SRC_SINK_MASK           0x00000200
#define BUFFER_PKT_FLAG_OUT_SRC_SINK_SHIFT          9
/* Bits: 10 - No Address Present */
#define BUFFER_PKT_FLAG_NO_ADDRESSES                0x00000400
#define BUFFER_PKT_FLAG_NO_ADDRESSES_SHIFT          10
/* Bits: 11 - Process Immediate*/
#define BUFFER_PKT_FLAG_PROC_IMMEDIATE              0x00000800
#define BUFFER_PKT_FLAG_PROC_IMMEDIATE_SHIFT        11
/* Bits: 12 - Meta data present */
#define BUFFER_PKT_FLAG_META_DATA                   0x00001000
#define BUFFER_PKT_FLAG_META_DATA_SHIFT             12
/* Bits: 12 - Input Buffer Return */
#define BUFFER_PKT_FLAG_INPUT_BUFFER_RETURN         0x00001000
#define BUFFER_PKT_FLAG_INPUT_BUFFER_RETURN_SHIFT   12
/* Bits: 13 - End of stream */
#define BUFFER_PKT_FLAG_EOS                         0x00002000
#define BUFFER_PKT_FLAG_EOS_SHIFT                   13
/* Bits: 16-17 - Number of output buffer planes */
#define BUFFER_PKT_FLAG_OUT_PLANE_NUM_MASK          0x00030000
#define BUFFER_PKT_FLAG_OUT_PLANE_NUM_SHIFT         16
#define BUFFER_PKT_GET_NUM_OUTPUT_PLANES(_n_buffer_pkt_info_) \
	((((_n_buffer_pkt_info_)&BUFFER_PKT_FLAG_OUT_PLANE_NUM_MASK) \
			>>BUFFER_PKT_FLAG_OUT_PLANE_NUM_SHIFT)+1)
/* Bits: 18-19 - Number of in buffers planes */
#define BUFFER_PKT_FLAG_IN_PLANE_NUM_MASK           0x000C0000
#define BUFFER_PKT_FLAG_IN_PLANE_NUM_SHIFT          18
#define BUFFER_PKT_GET_NUM_INPUT_PLANES(_n_buffer_pkt_info_)  \
	((((_n_buffer_pkt_info_)&BUFFER_PKT_FLAG_IN_PLANE_NUM_MASK) \
			>>BUFFER_PKT_FLAG_IN_PLANE_NUM_SHIFT)+1)
/* Bits: 24-25 - Number of past buffers */
#define BUFFER_PKT_FLAG_PAST_BUFFER_NUM_MASK        0x03000000
#define BUFFER_PKT_FLAG_PAST_BUFFER_NUM_SHIFT       24
/* Bits: 26-27 - Number of future buffers */
#define BUFFER_PKT_FLAG_FUTURE_BUFFER_NUM_MASK      0x0C000000
#define BUFFER_PKT_FLAG_FUTURE_BUFFER_NUM_SHIFT     26
/* Bits: 28 - Chroma down sampling */
#define BUFFER_PKT_FLAG_CDS_ENABLE                  0x10000000
#define BUFFER_PKT_FLAG_CDS_SHIFT                   28

/*
 * VPU_IPC_CMD_SESSION_SET_BUFFERS, VPU_IPC_CMD_SESSION_PROCESS_BUFFERS
 * num_out_buf: number of output buffers specified with this command
 * num_in_buf: number of input buffers specified with this command
 * buf_pkt_flag: bitmask of buffer type: input or output
 * buf_info_start: Offset to first vpu_ipc_buf_info structure
 */
struct vpu_ipc_cmd_session_buffers_packet {
	struct vpu_ipc_cmd_header_packet hdr;
	u32	num_out_buf;
	u32	num_in_buf;
	u32	buf_pkt_flag;
	u32	reserved;
};

/* Overall vpu_ipc_cmd_session_buffers_packet struct will appear as:
 *
 * vpu_ipc_cmd_session_buffers_packet    buffer_pkt;
 * struct vpu_ipc_buf_info buffer_info[n_in + n_out] // 0 - n
 */

/*
 * VPU_IPC_CMD_SESSION_RELEASE_BUFFERS command packet definition
 * buf_type	: indication of buffer type: input, output, NR or all
 * of enum vpu_ipc_buffer
 */
struct vpu_ipc_cmd_session_release_buffers_packet {
	struct vpu_ipc_cmd_header_packet hdr;
	u32	buf_type;
};

/*
 * VPU_IPC_CMD_SESSION_FLUSH command packet definition
 * flush_type	: buffer type to be flushed: input, output (enum vpu_ipc_flush)
 */
struct vpu_ipc_cmd_session_flush_packet {
	struct vpu_ipc_cmd_header_packet hdr;
	u32	flush_type;
};

/*
 * VPU_IPC_MSG_SYS_PROPERTY_INFO message packet definition
 * prop_id	: property identifier
 * data_offset	: pointer to property data associated with name
 * data_size	: size of property data structure in bytes
 */
struct vpu_ipc_msg_sys_property_info_packet {
	struct vpu_ipc_msg_header_packet hdr;
	u32 prop_id;
	u32 data_offset;
	u32 data_size;
};

/*
 * VPU_IPC_MSG_SYS_SET_PROPERTY_DONE message packet definition
 */
struct vpu_ipc_msg_sys_set_property_done_packet {
	struct vpu_ipc_msg_header_packet hdr;
};

/*
 * VPU_IPC_MSG_SESSION_OPEN_DONE message packet definition
 */
struct vpu_ipc_msg_session_open_done_packet {
	struct vpu_ipc_msg_header_packet hdr;
};

/*
 * VPU_IPC_MSG_SESSION_CLOSE_DONE message packet definition
 */
struct vpu_ipc_msg_session_close_done_packet {
	struct vpu_ipc_msg_header_packet hdr;
};

/*
 * VPU_IPC_MSG_SESSION_START_DONE message packet definition
 */
struct vpu_ipc_msg_session_start_done_packet {
	struct vpu_ipc_msg_header_packet hdr;
};

/*
 * VPU_IPC_MSG_SESSION_STOP_DONE message packet definition
 */
struct vpu_ipc_msg_session_stop_done_packet {
	struct vpu_ipc_msg_header_packet hdr;
};

/*
 * VPU_IPC_MSG_SESSION_PAUSE_DONE message packet definition
 */
struct vpu_ipc_msg_session_pause_done_packet {
	struct vpu_ipc_msg_header_packet hdr;
};

/*
 * VPU_IPC_MSG_SESSION_PROPERTY_INFO message packet definition
 * prop_id	: property identifier
 * data_offset	: pointer to property data associated with name
 * data_size	: size of property data structure in bytes
 */
struct vpu_ipc_msg_session_property_info_packet {
	struct vpu_ipc_msg_header_packet hdr;
	u32 prop_id;
	u32 data_offset;
	u32 data_size;
};


/*
 * VPU_IPC_MSG_SESSION_SET_PROPERTY_DONE message packet definition
 */
struct vpu_ipc_msg_session_set_property_done_packet {
	struct vpu_ipc_msg_header_packet hdr;
};

/*
 * VPU_IPC_MSG_SESSION_FLUSH_DONE message packet definition
 */
struct vpu_ipc_msg_session_flush_done_packet {
	struct vpu_ipc_msg_header_packet hdr;
};

/*
 * VPU_IPC_MSG_SESSION_ACTIVE_REGION message packet definition
 * This message is enabled using the VPU_PROP_SESSION_ACTIVE_REGION_DETEC cmd.
 * The message returns the active region within the stream excluding any
 * detected bars and pillars.
 * active_rect:	Rectangle specifying the active region.
 */
struct vpu_ipc_msg_session_active_region_packet {
	struct vpu_ipc_msg_header_packet hdr;
	struct rect active_rect;
};

/* The hfi_event enumeration defines all the possibles event types. */
enum hfi_event {
	VPU_IPC_EVENT_ERROR_NONE = 0,
	VPU_IPC_EVENT_ERROR_SYSTEM = 1,
	VPU_IPC_EVENT_ERROR_SESSION = 2,

	VPU_IPC_EVENT_ERROR_NUM,
	VPU_IPC_EVENT_MAX = 0x7fffffff
};

/*
 * VPU_IPC_MSG_SYS_EVENT_NOTIFY / VPU_IPC_MSG_SESSION_EVENT_NOTIFY message
 * packet definition
 * event_id:	The event ID (see enum hfi_event)
 * event_data:	data corresponding to the event ID (refer to VPU_STS_ defines)
 */
struct vpu_ipc_msg_event_notify_packet {
	struct vpu_ipc_msg_header_packet hdr;
	u32 event_id;
	u32 event_data;
};

/* VPU_IPC_MSG_SESSION_TIMESTAMP
 * Message is used to return the timestamp information in auto mode.
 * presentation: timestamp of last buffer output to MDSS
 * qtimer: timestamp of last buffer output to MDSS
 */
struct vpu_ipc_msg_session_timestamp {
	struct vpu_ipc_msg_header_packet hdr;
	struct vpu_timestamp_info presentation;
	struct vpu_timestamp_info qtimer;
	u32 reserved;
};

/*
 * VPU_IPC_MSG_SESSION_PROCESS_BUFFERS_DONE packet definition
 * num_out_buf:		The number of elements of vpu_ipc_buf_info held in an
 *			array at nBufferInfoOffset
 * num_in_buf:		Offset from the start of this structure to beginning of
 *			first struct vpu_ipc_buf_info
 * buf_pkt_flag:	Bit mask of type of buffers: input or output.
 * flags:		reserved
 * nBufferInfoStart:	Offset to first vpu_ipc_buf_info structure
 */
struct vpu_ipc_msg_session_process_buffers_done_packet {
	struct vpu_ipc_msg_header_packet hdr;
	u32 num_out_buf;
	u32 num_in_buf;
	u32 buf_pkt_flag;
	u32 reserved;
};

/*
 * VPU_IPC_MSG_SESSION_RELEASE_BUFFERS_DONE packet definition
 * buf_type:		input, output, NR or all of enum vpu_ipc_buffer
 */
struct vpu_ipc_msg_session_release_buffers_done_packet {
	struct vpu_ipc_msg_header_packet hdr;
	u32 buf_type;
};

/*
 * VPU hardware logs
 */
enum vpu_log_pkt {
	FW_MESSAGE_PACKET,
	FW_METADATA_PACKET,
	FW_PIXELDATA_PACKET,

	FRC_MESSAGE_PACKET,
	FRC_METADATA_PACKET,
	FRC_PIXELDATA_PACKET,

	LOG_PACKET_MAX,
};

enum vpu_log_msg {
	LOG_DEBUG,
	LOG_ERROR,
	LOG_MESSAGE_MAX,
};

enum vpu_log_priority {
	LOG_LEVEL_LOW,
	LOG_LEVEL_MEDIUM,
	LOG_LEVEL_HIGH,
	LOG_LEVEL_MAX,
};

/*
 * struct vpu_ipc_log_header_packet logging message packet definition
 * size:	size of the packet in bytes
 * pkt_type:	see enum vpu_log_pkt
 * time_stamp:	logging time stamp
 * msg_type:	see enum vpu_log_msg
 * priority:	see enum vpu_log_priority
 * msg_addr:	message Pointer.  NULL means MessageData contains the actual
 *		message
 * msg_size:	message size in bytes
 *
 */
struct vpu_ipc_log_header_packet {
	u32	size;
	u32	pkt_type;
	u32	time_stamp;
	u32	msg_type;
	u32	priority;
	u32	msg_addr;
	u32	msg_size;
};


#endif /* __H_VPU_IPC_H__ */
