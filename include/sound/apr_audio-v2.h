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


#ifndef _APR_AUDIO_V2_H_
#define _APR_AUDIO_V2_H_

#include <linux/qdsp6v2/apr.h>

#define ADSP_ADM_VERSION    0x00070000

#define ADM_CMD_SHARED_MEM_MAP_REGIONS    0x00010322
#define ADM_CMDRSP_SHARED_MEM_MAP_REGIONS 0x00010323
#define ADM_CMD_SHARED_MEM_UNMAP_REGIONS 0x00010324

#define ADM_CMD_MATRIX_MAP_ROUTINGS_V5 0x00010325

/* Enumeration for an audio Rx matrix ID.*/
#define ADM_MATRIX_ID_AUDIO_RX              0

#define ADM_MATRIX_ID_AUDIO_TX              1

/* Enumeration for an audio Tx matrix ID.*/
#define ADM_MATRIX_ID_AUDIOX              1

#define ADM_MAX_COPPS 5


/*  Session map node structure.
*	Immediately following this structure are num_copps
*	entries of COPP IDs. The COPP IDs are 16 bits, so
*	there might be a padding 16-bit field if num_copps
*	is odd.
*/
struct adm_session_map_node_v5 {
	u16                  session_id;
/* Handle of the ASM session to be routed. Supported values: 1
* to 8.
*/


	u16                  num_copps;
	/* Number of COPPs to which this session is to be routed.
			Supported values: 0 < num_copps <= ADM_MAX_COPPS.
	*/
} __packed;

/*  Payload of the #ADM_CMD_MATRIX_MAP_ROUTINGS_V5 command.
*	Immediately following this structure are num_sessions of the session map
*	node payload (adm_session_map_node_v5).
*/

struct adm_cmd_matrix_map_routings_v5 {
	struct apr_hdr	hdr;

	u32                  matrix_id;
/* Specifies whether the matrix ID is Audio Rx (0) or Audio Tx
* (1). Use the ADM_MATRIX_ID_AUDIO_RX or ADM_MATRIX_ID_AUDIOX
* macros to set this field.
*/
	u32                  num_sessions;
	/* Number of sessions being updated by this command (optional).*/
} __packed;

/* This command allows a client to open a COPP/Voice Proc. TX module
*	and sets up	the device session: Matrix -> COPP -> AFE on the RX
*	and AFE -> COPP -> Matrix on the TX. This enables PCM data to
*	be transferred to/from the endpoint (AFEPortID).
*
*	@return
*	#ADM_CMDRSP_DEVICE_OPEN_V5 with the resulting status and
*	COPP ID.
*/
#define ADM_CMD_DEVICE_OPEN_V5                          0x00010326

/* Definition for a low latency stream session. */
#define ADM_LOW_LATENCY_DEVICE_SESSION			0x2000

/* Definition for a ultra low latency stream session. */
#define ADM_ULTRA_LOW_LATENCY_DEVICE_SESSION		0x4000

/* Definition for a legacy device session. */
#define ADM_LEGACY_DEVICE_SESSION                                      0

/* Indicates that endpoint_id_2 is to be ignored.*/
#define ADM_CMD_COPP_OPEN_END_POINT_ID_2_IGNORE				0xFFFF

#define ADM_CMD_COPP_OPEN_MODE_OF_OPERATION_RX_PATH_COPP		 1

#define ADM_CMD_COPP_OPEN_MODE_OF_OPERATIONX_PATH_LIVE_COPP		 2

#define ADM_CMD_COPP_OPEN_MODE_OF_OPERATIONX_PATH_NON_LIVE_COPP	 3

/* Indicates that an audio COPP is to send/receive a mono PCM
 * stream to/from
 *	END_POINT_ID_1.
 */
#define ADM_CMD_COPP_OPEN_CHANNEL_CONFIG_MONO		1

/* Indicates that an audio COPP is to send/receive a
 *	stereo PCM stream to/from END_POINT_ID_1.
 */
#define ADM_CMD_COPP_OPEN_CHANNEL_CONFIG_STEREO		2

/* Sample rate is 8000 Hz.*/
#define ADM_CMD_COPP_OPEN_SAMPLE_RATE_8K 8000

/* Sample rate is 16000 Hz.*/
#define ADM_CMD_COPP_OPEN_SAMPLE_RATE_16K 16000

/* Sample rate is 48000 Hz.*/
#define ADM_CMD_COPP_OPEN_SAMPLE_RATE_48K 48000

/* Definition for a COPP live input flag bitmask.*/
#define ADM_BIT_MASK_COPP_LIVE_INPUT_FLAG (0x0001U)

/* Definition for a COPP live shift value bitmask.*/
#define ADM_SHIFT_COPP_LIVE_INPUT_FLAG	 0

/* Definition for the COPP ID bitmask.*/
#define ADM_BIT_MASK_COPP_ID  (0x0000FFFFUL)

/* Definition for the COPP ID shift value.*/
#define ADM_SHIFT_COPP_ID	0

/* Definition for the service ID bitmask.*/
#define ADM_BIT_MASK_SERVICE_ID  (0x00FF0000UL)

/* Definition for the service ID shift value.*/
#define ADM_SHIFT_SERVICE_ID	16

/* Definition for the domain ID bitmask.*/
#define ADM_BIT_MASK_DOMAIN_ID    (0xFF000000UL)

/* Definition for the domain ID shift value.*/
#define ADM_SHIFT_DOMAIN_ID	24

/*  ADM device open command payload of the
	#ADM_CMD_DEVICE_OPEN_V5 command.
*/
struct adm_cmd_device_open_v5 {
	struct apr_hdr		hdr;
	u16                  flags;
/* Reserved for future use. Clients must set this field
 * to zero.
 */

	u16                  mode_of_operation;
/* Specifies whether the COPP must be opened on the Tx or Rx
 * path. Use the ADM_CMD_COPP_OPEN_MODE_OF_OPERATION_* macros for
 * supported values and interpretation.
 * Supported values:
 * - 0x1 -- Rx path COPP
 * - 0x2 -- Tx path live COPP
 * - 0x3 -- Tx path nonlive COPP
 * Live connections cause sample discarding in the Tx device
 * matrix if the destination output ports do not pull them
 * fast enough. Nonlive connections queue the samples
 * indefinitely.
 */

	u16                  endpoint_id_1;
/* Logical and physical endpoint ID of the audio path.
 * If the ID is a voice processor Tx block, it receives near
 * samples.	Supported values: Any pseudoport, AFE Rx port,
 * or AFE Tx port For a list of valid IDs, refer to
 * @xhyperref{Q4,[Q4]}.
 * Q4 = Hexagon Multimedia: AFE Interface Specification
 */

	u16                  endpoint_id_2;
/* Logical and physical endpoint ID 2 for a voice processor
 * Tx block.
 * This is not applicable to audio COPP.
 * Supported values:
 * - AFE Rx port
 * - 0xFFFF -- Endpoint 2 is unavailable and the voice
 * processor Tx
 * block ignores this endpoint
 * When the voice processor Tx block is created on the audio
 * record path,
 * it can receive far-end samples from an AFE Rx port if the
 * voice call
 * is active. The ID of the AFE port is provided in this
 * field.
 * For a list of valid IDs, refer @xhyperref{Q4,[Q4]}.
 */

	u32                  topology_id;
	/* Audio COPP topology ID; 32-bit GUID. */

	u16                  dev_num_channel;
/* Number of channels the audio COPP sends to/receives from
 * the endpoint.
 * Supported values: 1 to 8.
 * The value is ignored for the voice processor Tx block,
 * where channel
 * configuration is derived from the topology ID.
 */

	u16                  bit_width;
/* Bit width (in bits) that the audio COPP sends to/receives
 * from the
 * endpoint. The value is ignored for the voice processing
 * Tx block,
 * where the PCM width is 16 bits.
 */

	u32                  sample_rate;
/* Sampling rate at which the audio COPP/voice processor
 * Tx block
 * interfaces with the endpoint.
 * Supported values for voice processor Tx: 8000, 16000,
 * 48000 Hz
 * Supported values for audio COPP: >0 and <=192 kHz
 */

	u8                   dev_channel_mapping[8];
/* Array of channel mapping of buffers that the audio COPP
 * sends to the endpoint. Channel[i] mapping describes channel
 * I inside the buffer, where 0 < i < dev_num_channel.
 * This value is relevent only for an audio Rx COPP.
 * For the voice processor block and Tx audio block, this field
 * is set to zero and is ignored.
 */
} __packed;

/*
 *	This command allows the client to close a COPP and disconnect
 *	the device session.
 */
#define ADM_CMD_DEVICE_CLOSE_V5                         0x00010327

/* Sets one or more parameters to a COPP.
*/
#define ADM_CMD_SET_PP_PARAMS_V5                        0x00010328

/*  Payload of the #ADM_CMD_SET_PP_PARAMS_V5 command.
 *	If the data_payload_addr_lsw and data_payload_addr_msw element
 *	are NULL, a series of adm_param_datastructures immediately
 *	follows, whose total size is data_payload_size bytes.
 */
struct adm_cmd_set_pp_params_v5 {
	struct apr_hdr hdr;
	u32		payload_addr_lsw;
	/* LSW of parameter data payload address.*/
	u32		payload_addr_msw;
	/* MSW of parameter data payload address.*/

	u32		mem_map_handle;
/* Memory map handle returned by ADM_CMD_SHARED_MEM_MAP_REGIONS
 * command */
/* If mem_map_handle is zero implies the message is in
 * the payload */

	u32		payload_size;
/* Size in bytes of the variable payload accompanying this
 * message or
 * in shared memory. This is used for parsing the parameter
 * payload.
 */
} __packed;

/*  Payload format for COPP parameter data.
 * Immediately following this structure are param_size bytes
 * of parameter
 * data.
 */
struct adm_param_data_v5 {
	u32                  module_id;
	/* Unique ID of the module. */
	u32                  param_id;
	/* Unique ID of the parameter. */
	u16                  param_size;
	/* Data size of the param_id/module_id combination.
	This value is a
		multiple of 4 bytes. */
	u16                  reserved;
	/* Reserved for future enhancements.
	 * This field must be set to zero.
	 */
} __packed;

/* set customized mixing on matrix mixer */
#define ADM_CMD_SET_PSPD_MTMX_STRTR_PARAMS_V5                        0x00010344
struct adm_cmd_set_pspd_mtmx_strtr_params_v5 {
	struct apr_hdr hdr;
	/* LSW of parameter data payload address.*/
	u32		payload_addr_lsw;
	/* MSW of parameter data payload address.*/
	u32		payload_addr_msw;
	/* Memory map handle returned by ADM_CMD_SHARED_MEM_MAP_REGIONS */
	/* command. If mem_map_handle is zero implies the message is in */
	/* the payload */
	u32		mem_map_handle;
	/* Size in bytes of the variable payload accompanying this */
	/* message or in shared memory. This is used for parsing the */
	/* parameter payload. */
	u32		payload_size;
	u16		direction;
	u16		sessionid;
	u16		deviceid;
	u16		reserved;
} __packed;

/* Defined specifically for in-band use, includes params */
struct adm_cmd_set_pp_params_inband_v5 {
	struct apr_hdr hdr;
	/* LSW of parameter data payload address.*/
	u32             payload_addr_lsw;
	/* MSW of parameter data payload address.*/
	u32             payload_addr_msw;
	/* Memory map handle returned by ADM_CMD_SHARED_MEM_MAP_REGIONS */
	/* command. If mem_map_handle is zero implies the message is in */
	/* the payload */
	u32             mem_map_handle;
	/* Size in bytes of the variable payload accompanying this */
	/* message or in shared memory. This is used for parsing the */
	/* parameter payload. */
	u32             payload_size;
	/* Parameters passed for in band payload */
	struct adm_param_data_v5        params;
} __packed;

/* Returns the status and COPP ID to an #ADM_CMD_DEVICE_OPEN_V5 command.
 */
#define ADM_CMDRSP_DEVICE_OPEN_V5                      0x00010329

/*  Payload of the #ADM_CMDRSP_DEVICE_OPEN_V5 message,
 *	which returns the
 *	status and COPP ID to an #ADM_CMD_DEVICE_OPEN_V5 command.
 */
struct adm_cmd_rsp_device_open_v5 {
	u32                  status;
	/* Status message (error code).*/

	u16                  copp_id;
	/* COPP ID:  Supported values: 0 <= copp_id < ADM_MAX_COPPS*/

	u16                  reserved;
	/* Reserved. This field must be set to zero.*/
} __packed;

/* This command allows a query of one COPP parameter.
*/
#define ADM_CMD_GET_PP_PARAMS_V5                                0x0001032A

/*  Payload an #ADM_CMD_GET_PP_PARAMS_V5 command.
*/
struct adm_cmd_get_pp_params_v5 {
	struct apr_hdr hdr;
	u32                  data_payload_addr_lsw;
	/* LSW of parameter data payload address.*/

	u32                  data_payload_addr_msw;
	/* MSW of parameter data payload address.*/

	/* If the mem_map_handle is non zero,
	 * on ACK, the ParamData payloads begin at
	 * the address specified (out-of-band).
	 */

	u32                  mem_map_handle;
	/* Memory map handle returned
	 * by ADM_CMD_SHARED_MEM_MAP_REGIONS command.
	 * If the mem_map_handle is 0, it implies that
	 * the ACK's payload will contain the ParamData (in-band).
	 */

	u32                  module_id;
	/* Unique ID of the module. */

	u32                  param_id;
	/* Unique ID of the parameter. */

	u16                  param_max_size;
	/* Maximum data size of the parameter
	 *ID/module ID combination. This
	 * field is a multiple of 4 bytes.
	 */
	u16                  reserved;
	/* Reserved for future enhancements.
	 * This field must be set to zero.
	 */
} __packed;

/* Returns parameter values
 *	in response to an #ADM_CMD_GET_PP_PARAMS_V5 command.
 */
#define ADM_CMDRSP_GET_PP_PARAMS_V5		0x0001032B

/* Payload of the #ADM_CMDRSP_GET_PP_PARAMS_V5 message,
 * which returns parameter values in response
 * to an #ADM_CMD_GET_PP_PARAMS_V5 command.
 * Immediately following this
 * structure is the adm_param_data_v5
 * structure containing the pre/postprocessing
 * parameter data. For an in-band
 * scenario, the variable payload depends
 * on the size of the parameter.
*/
struct adm_cmd_rsp_get_pp_params_v5 {
	u32                  status;
	/* Status message (error code).*/
} __packed;

/* Allows a client to control the gains on various session-to-COPP paths.
 */
#define ADM_CMD_MATRIX_RAMP_GAINS_V5                                 0x0001032C

/* Indicates that the target gain in the
 *	current adm_session_copp_gain_v5
 *	structure is to be applied to all
 *	the session-to-COPP paths that exist for
 *	the specified session.
 */
#define ADM_CMD_MATRIX_RAMP_GAINS_COPP_ID_ALL_CONNECTED_COPPS     0xFFFF

/* Indicates that the target gain is
 * to be immediately applied to the
 * specified session-to-COPP path,
 * without a ramping fashion.
 */
#define ADM_CMD_MATRIX_RAMP_GAINS_RAMP_DURATION_IMMEDIATE         0x0000

/* Enumeration for a linear ramping curve.*/
#define ADM_CMD_MATRIX_RAMP_GAINS_RAMP_CURVE_LINEAR               0x0000

/*  Payload of the #ADM_CMD_MATRIX_RAMP_GAINS_V5 command.
 * Immediately following this structure are num_gains of the
 * adm_session_copp_gain_v5structure.
 */
struct adm_cmd_matrix_ramp_gains_v5 {
	u32                  matrix_id;
/* Specifies whether the matrix ID is Audio Rx (0) or Audio Tx (1).
 * Use the ADM_MATRIX_ID_AUDIO_RX or  ADM_MATRIX_ID_AUDIOX
 * macros to set this field.
*/

	u16                  num_gains;
	/* Number of gains being applied. */

	u16                  reserved_for_align;
	/* Reserved. This field must be set to zero.*/
} __packed;

/*  Session-to-COPP path gain structure, used by the
 *	#ADM_CMD_MATRIX_RAMP_GAINS_V5 command.
 *	This structure specifies the target
 *	gain (per channel) that must be applied
 *	to a particular session-to-COPP path in
 *	the audio matrix. The structure can
 *	also be used to apply the gain globally
 *	to all session-to-COPP paths that
 *	exist for the given session.
 *	The aDSP uses device channel mapping to
 *	determine which channel gains to
 *	use from this command. For example,
 *	if the device is configured as stereo,
 *	the aDSP uses only target_gain_ch_1 and
 *	target_gain_ch_2, and it ignores
 *	the others.
 */
struct adm_session_copp_gain_v5 {
	u16                  session_id;
/* Handle of the ASM session.
 *	Supported values: 1 to 8.
 */

	u16                  copp_id;
/* Handle of the COPP. Gain will be applied on the Session ID
 * COPP ID path.
 */

	u16                  ramp_duration;
/* Duration (in milliseconds) of the ramp over
 * which target gains are
 * to be applied. Use
 * #ADM_CMD_MATRIX_RAMP_GAINS_RAMP_DURATION_IMMEDIATE
 * to indicate that gain must be applied immediately.
 */

	u16                  step_duration;
/* Duration (in milliseconds) of each step in the ramp.
 * This parameter is ignored if ramp_duration is equal to
 * #ADM_CMD_MATRIX_RAMP_GAINS_RAMP_DURATION_IMMEDIATE.
 * Supported value: 1
 */

	u16                  ramp_curve;
/* Type of ramping curve.
 * Supported value: #ADM_CMD_MATRIX_RAMP_GAINS_RAMP_CURVE_LINEAR
 */

	u16                  reserved_for_align;
	/* Reserved. This field must be set to zero. */

	u16                  target_gain_ch_1;
	/* Target linear gain for channel 1 in Q13 format; */

	u16                  target_gain_ch_2;
	/* Target linear gain for channel 2 in Q13 format; */

	u16                  target_gain_ch_3;
	/* Target linear gain for channel 3 in Q13 format; */

	u16                  target_gain_ch_4;
	/* Target linear gain for channel 4 in Q13 format; */

	u16                  target_gain_ch_5;
	/* Target linear gain for channel 5 in Q13 format; */

	u16                  target_gain_ch_6;
	/* Target linear gain for channel 6 in Q13 format; */

	u16                  target_gain_ch_7;
	/* Target linear gain for channel 7 in Q13 format; */

	u16                  target_gain_ch_8;
	/* Target linear gain for channel 8 in Q13 format; */
} __packed;

/* Allows to set mute/unmute on various session-to-COPP paths.
 *	For every session-to-COPP path (stream-device interconnection),
 *	mute/unmute can be set individually on the output channels.
 */
#define ADM_CMD_MATRIX_MUTE_V5                                0x0001032D

/* Indicates that mute/unmute in the
 *	current adm_session_copp_mute_v5structure
 *	is to be applied to all the session-to-COPP
 *	paths that exist for the specified session.
 */
#define ADM_CMD_MATRIX_MUTE_COPP_ID_ALL_CONNECTED_COPPS     0xFFFF

/*  Payload of the #ADM_CMD_MATRIX_MUTE_V5 command*/
struct adm_cmd_matrix_mute_v5 {
	u32                  matrix_id;
/* Specifies whether the matrix ID is Audio Rx (0) or Audio Tx (1).
 * Use the ADM_MATRIX_ID_AUDIO_RX or  ADM_MATRIX_ID_AUDIOX
 * macros to set this field.
 */

	u16                  session_id;
/* Handle of the ASM session.
 * Supported values: 1 to 8.
 */

	u16                  copp_id;
/* Handle of the COPP.
 * Use ADM_CMD_MATRIX_MUTE_COPP_ID_ALL_CONNECTED_COPPS
 * to indicate that mute/unmute must be applied to
 * all the COPPs connected to session_id.
 * Supported values:
 * - 0xFFFF -- Apply mute/unmute to all connected COPPs
 * - Other values -- Valid COPP ID
 */

	u8                  mute_flag_ch_1;
	/* Mute flag for channel 1 is set to unmute (0) or mute (1). */

	u8                  mute_flag_ch_2;
	/* Mute flag for channel 2 is set to unmute (0) or mute (1). */

	u8                  mute_flag_ch_3;
	/* Mute flag for channel 3 is set to unmute (0) or mute (1). */

	u8                  mute_flag_ch_4;
	/* Mute flag for channel 4 is set to unmute (0) or mute (1). */

	u8                  mute_flag_ch_5;
	/* Mute flag for channel 5 is set to unmute (0) or mute (1). */

	u8                  mute_flag_ch_6;
	/* Mute flag for channel 6 is set to unmute (0) or mute (1). */

	u8                  mute_flag_ch_7;
	/* Mute flag for channel 7 is set to unmute (0) or mute (1). */

	u8                  mute_flag_ch_8;
	/* Mute flag for channel 8 is set to unmute (0) or mute (1). */

	u16                 ramp_duration;
/* Period (in milliseconds) over which the soft mute/unmute will be
 * applied.
 * Supported values: 0 (Default) to 0xFFFF
 * The default of 0 means mute/unmute will be applied immediately.
 */

	u16                 reserved_for_align;
	/* Clients must set this field to zero.*/
} __packed;

#define ASM_PARAM_ID_AAC_STEREO_MIX_COEFF_SELECTION_FLAG_V2 (0x00010DD8)

struct asm_aac_stereo_mix_coeff_selection_param_v2 {
	struct apr_hdr          hdr;
	u32                     param_id;
	u32                     param_size;
	u32                     aac_stereo_mix_coeff_flag;
} __packed;

/* Allows a client to connect the desired stream to
 * the desired AFE port through the stream router
 *
 * This command allows the client to connect specified session to
 * specified AFE port. This is used for compressed streams only
 * opened using the #ASM_STREAM_CMD_OPEN_WRITE_COMPRESSED or
 * #ASM_STREAM_CMD_OPEN_READ_COMPRESSED command.
 *
 * @prerequisites
 * Session ID and AFE Port ID must be valid.
 * #ASM_STREAM_CMD_OPEN_WRITE_COMPRESSED or
 * #ASM_STREAM_CMD_OPEN_READ_COMPRESSED
 * must have been called on this session.
 */

#define ADM_CMD_CONNECT_AFE_PORT_V5	0x0001032E
#define ADM_CMD_DISCONNECT_AFE_PORT_V5	0x0001032F
/* Enumeration for the Rx stream router ID.*/
#define ADM_STRTR_ID_RX                    0
/* Enumeration for the Tx stream router ID.*/
#define ADM_STRTR_IDX                    1

/*  Payload of the #ADM_CMD_CONNECT_AFE_PORT_V5 command.*/
struct adm_cmd_connect_afe_port_v5 {
	struct apr_hdr     hdr;
	u8                  mode;
/* ID of the stream router (RX/TX). Use the
 * ADM_STRTR_ID_RX or ADM_STRTR_IDX macros
 * to set this field.
 */

	u8                  session_id;
	/* Session ID of the stream to connect */

	u16                 afe_port_id;
	/* Port ID of the AFE port to connect to.*/
	u32                 num_channels;
/* Number of device channels
 * Supported values: 2(Audio Sample Packet),
 * 8 (HBR Audio Stream Sample Packet)
 */

	u32                 sampling_rate;
/* Device sampling rate
* Supported values: Any
*/
} __packed;


/* adsp_adm_api.h */


/* Port ID. Update afe_get_port_index
 *	when a new port is added here. */
#define PRIMARY_I2S_RX 0
#define PRIMARY_I2S_TX 1
#define SECONDARY_I2S_RX 4
#define SECONDARY_I2S_TX 5
#define MI2S_RX 6
#define MI2S_TX 7
#define HDMI_RX 8
#define RSVD_2 9
#define RSVD_3 10
#define DIGI_MIC_TX 11
#define VOICE2_PLAYBACK_TX 0x8002
#define VOICE_RECORD_RX 0x8003
#define VOICE_RECORD_TX 0x8004
#define VOICE_PLAYBACK_TX 0x8005

/* Slimbus Multi channel port id pool  */
#define SLIMBUS_0_RX		0x4000
#define SLIMBUS_0_TX		0x4001
#define SLIMBUS_1_RX		0x4002
#define SLIMBUS_1_TX		0x4003
#define SLIMBUS_2_RX		0x4004
#define SLIMBUS_2_TX		0x4005
#define SLIMBUS_3_RX		0x4006
#define SLIMBUS_3_TX		0x4007
#define SLIMBUS_4_RX		0x4008
#define SLIMBUS_4_TX		0x4009
#define SLIMBUS_5_RX		0x400a
#define SLIMBUS_5_TX		0x400b
#define SLIMBUS_6_RX		0x400c
#define SLIMBUS_6_TX		0x400d
#define SLIMBUS_PORT_LAST	SLIMBUS_6_TX
#define INT_BT_SCO_RX 0x3000
#define INT_BT_SCO_TX 0x3001
#define INT_BT_A2DP_RX 0x3002
#define INT_FM_RX 0x3004
#define INT_FM_TX 0x3005
#define RT_PROXY_PORT_001_RX	0x2000
#define RT_PROXY_PORT_001_TX	0x2001

#define AFE_PORT_INVALID 0xFFFF
#define SLIMBUS_INVALID AFE_PORT_INVALID

#define AFE_PORT_CMD_START 0x000100ca

#define AFE_EVENT_RTPORT_START 0
#define AFE_EVENT_RTPORT_STOP 1
#define AFE_EVENT_RTPORT_LOW_WM 2
#define AFE_EVENT_RTPORT_HI_WM 3

#define ADSP_AFE_VERSION    0x00200000

/* Size of the range of port IDs for the audio interface. */
#define  AFE_PORT_ID_AUDIO_IF_PORT_RANGE_SIZE	0xF

/* Size of the range of port IDs for internal BT-FM ports. */
#define AFE_PORT_ID_INTERNAL_BT_FM_RANGE_SIZE	0x6

/* Size of the range of port IDs for SLIMbus<sup>&reg;
 * </sup> multichannel
 * ports.
 */
#define AFE_PORT_ID_SLIMBUS_RANGE_SIZE	0xA

/* Size of the range of port IDs for real-time proxy ports. */
#define  AFE_PORT_ID_RT_PROXY_PORT_RANGE_SIZE	0x2

/* Size of the range of port IDs for pseudoports. */
#define AFE_PORT_ID_PSEUDOPORT_RANGE_SIZE	0x5

/* Start of the range of port IDs for the audio interface. */
#define  AFE_PORT_ID_AUDIO_IF_PORT_RANGE_START	0x1000

/* End of the range of port IDs for the audio interface. */
#define  AFE_PORT_ID_AUDIO_IF_PORT_RANGE_END \
	(AFE_PORT_ID_AUDIO_IF_PORT_RANGE_START +\
	AFE_PORT_ID_AUDIO_IF_PORT_RANGE_SIZE - 1)

/* Start of the range of port IDs for real-time proxy ports. */
#define  AFE_PORT_ID_RT_PROXY_PORT_RANGE_START	0x2000

/* End of the range of port IDs for real-time proxy ports. */
#define  AFE_PORT_ID_RT_PROXY_PORT_RANGE_END \
	(AFE_PORT_ID_RT_PROXY_PORT_RANGE_START +\
	AFE_PORT_ID_RT_PROXY_PORT_RANGE_SIZE-1)

/* Start of the range of port IDs for internal BT-FM devices. */
#define AFE_PORT_ID_INTERNAL_BT_FM_RANGE_START	0x3000

/* End of the range of port IDs for internal BT-FM devices. */
#define AFE_PORT_ID_INTERNAL_BT_FM_RANGE_END \
	(AFE_PORT_ID_INTERNAL_BT_FM_RANGE_START +\
	AFE_PORT_ID_INTERNAL_BT_FM_RANGE_SIZE-1)

/*	Start of the range of port IDs for SLIMbus devices. */
#define AFE_PORT_ID_SLIMBUS_RANGE_START	0x4000

/*	End of the range of port IDs for SLIMbus devices. */
#define AFE_PORT_ID_SLIMBUS_RANGE_END \
	(AFE_PORT_ID_SLIMBUS_RANGE_START +\
	AFE_PORT_ID_SLIMBUS_RANGE_SIZE-1)

/* Start of the range of port IDs for pseudoports. */
#define AFE_PORT_ID_PSEUDOPORT_RANGE_START	0x8001

/* End of the range of port IDs for pseudoports.  */
#define AFE_PORT_ID_PSEUDOPORT_RANGE_END \
	(AFE_PORT_ID_PSEUDOPORT_RANGE_START +\
	AFE_PORT_ID_PSEUDOPORT_RANGE_SIZE-1)

#define AFE_PORT_ID_PRIMARY_MI2S_RX         0x1000
#define AFE_PORT_ID_PRIMARY_MI2S_TX         0x1001
#define AFE_PORT_ID_SECONDARY_MI2S_RX       0x1002
#define AFE_PORT_ID_SECONDARY_MI2S_TX       0x1003
#define AFE_PORT_ID_TERTIARY_MI2S_RX        0x1004
#define AFE_PORT_ID_TERTIARY_MI2S_TX        0x1005
#define AFE_PORT_ID_QUATERNARY_MI2S_RX      0x1006
#define AFE_PORT_ID_QUATERNARY_MI2S_TX      0x1007
#define AUDIO_PORT_ID_I2S_RX				0x1008
#define AFE_PORT_ID_DIGITAL_MIC_TX          0x1009
#define AFE_PORT_ID_PRIMARY_PCM_RX          0x100A
#define AFE_PORT_ID_PRIMARY_PCM_TX          0x100B
#define AFE_PORT_ID_SECONDARY_PCM_RX        0x100C
#define AFE_PORT_ID_SECONDARY_PCM_TX        0x100D
#define AFE_PORT_ID_MULTICHAN_HDMI_RX       0x100E
#define AFE_PORT_ID_SECONDARY_MI2S_RX_SD1	0x1010
#define AFE_PORT_ID_SPDIF_RX                0x5000
#define  AFE_PORT_ID_RT_PROXY_PORT_001_RX   0x2000
#define  AFE_PORT_ID_RT_PROXY_PORT_001_TX   0x2001
#define AFE_PORT_ID_INTERNAL_BT_SCO_RX      0x3000
#define AFE_PORT_ID_INTERNAL_BT_SCO_TX      0x3001
#define AFE_PORT_ID_INTERNAL_BT_A2DP_RX     0x3002
#define AFE_PORT_ID_INTERNAL_FM_RX          0x3004
#define AFE_PORT_ID_INTERNAL_FM_TX          0x3005
/* SLIMbus Rx port on channel 0. */
#define AFE_PORT_ID_SLIMBUS_MULTI_CHAN_0_RX      0x4000
/* SLIMbus Tx port on channel 0. */
#define AFE_PORT_ID_SLIMBUS_MULTI_CHAN_0_TX      0x4001
/* SLIMbus Rx port on channel 1. */
#define AFE_PORT_ID_SLIMBUS_MULTI_CHAN_1_RX      0x4002
/* SLIMbus Tx port on channel 1. */
#define AFE_PORT_ID_SLIMBUS_MULTI_CHAN_1_TX      0x4003
/* SLIMbus Rx port on channel 2. */
#define AFE_PORT_ID_SLIMBUS_MULTI_CHAN_2_RX      0x4004
/* SLIMbus Tx port on channel 2. */
#define AFE_PORT_ID_SLIMBUS_MULTI_CHAN_2_TX      0x4005
/* SLIMbus Rx port on channel 3. */
#define AFE_PORT_ID_SLIMBUS_MULTI_CHAN_3_RX      0x4006
/* SLIMbus Tx port on channel 3. */
#define AFE_PORT_ID_SLIMBUS_MULTI_CHAN_3_TX      0x4007
/* SLIMbus Rx port on channel 4. */
#define AFE_PORT_ID_SLIMBUS_MULTI_CHAN_4_RX      0x4008
/* SLIMbus Tx port on channel 4. */
#define AFE_PORT_ID_SLIMBUS_MULTI_CHAN_4_TX      0x4009
/* SLIMbus Rx port on channel 5. */
#define AFE_PORT_ID_SLIMBUS_MULTI_CHAN_5_RX      0x400a
/* SLIMbus Tx port on channel 5. */
#define AFE_PORT_ID_SLIMBUS_MULTI_CHAN_5_TX      0x400b
/* SLIMbus Rx port on channel 6. */
#define AFE_PORT_ID_SLIMBUS_MULTI_CHAN_6_RX      0x400c
/* SLIMbus Tx port on channel 6. */
#define AFE_PORT_ID_SLIMBUS_MULTI_CHAN_6_TX      0x400d

/* Generic pseudoport 1. */
#define AFE_PORT_ID_PSEUDOPORT_01      0x8001
/* Generic pseudoport 2. */
#define AFE_PORT_ID_PSEUDOPORT_02      0x8002

/* @xreflabel{hdr:AfePortIdPrimaryAuxPcmTx}
	Primary Aux PCM Tx port ID.
*/
#define AFE_PORT_ID_PRIMARY_PCM_TX      0x100B
/* Pseudoport that corresponds to the voice Rx path.
 * For recording, the voice Rx path samples are written to this
 * port and consumed by the audio path.
 */

#define AFE_PORT_ID_VOICE_RECORD_RX	0x8003

/* Pseudoport that corresponds to the voice Tx path.
 * For recording, the voice Tx path samples are written to this
 * port and consumed by the audio path.
 */

#define AFE_PORT_ID_VOICE_RECORD_TX	0x8004
/* Pseudoport that corresponds to in-call voice delivery samples.
 * During in-call audio delivery, the audio path delivers samples
 * to this port from where the voice path delivers them on the
 * Rx path.
 */
#define AFE_PORT_ID_VOICE2_PLAYBACK_TX  0x8002
#define AFE_PORT_ID_VOICE_PLAYBACK_TX   0x8005
#define AFE_PORT_ID_INVALID             0xFFFF

#define AAC_ENC_MODE_AAC_LC 0x02
#define AAC_ENC_MODE_AAC_P 0x05
#define AAC_ENC_MODE_EAAC_P 0x1D

#define AFE_PSEUDOPORT_CMD_START 0x000100cf
struct afe_pseudoport_start_command {
	struct apr_hdr hdr;
	u16 port_id;		/* Pseudo Port 1 = 0x8000 */
				/* Pseudo Port 2 = 0x8001 */
				/* Pseudo Port 3 = 0x8002 */
	u16 timing;		/* FTRT = 0 , AVTimer = 1, */
} __packed;

#define AFE_PSEUDOPORT_CMD_STOP 0x000100d0
struct afe_pseudoport_stop_command {
	struct apr_hdr hdr;
	u16 port_id;		/* Pseudo Port 1 = 0x8000 */
				/* Pseudo Port 2 = 0x8001 */
				/* Pseudo Port 3 = 0x8002 */
	u16 reserved;
} __packed;


#define AFE_MODULE_SIDETONE_IIR_FILTER	0x00010202
#define AFE_PARAM_ID_ENABLE	0x00010203

/*  Payload of the #AFE_PARAM_ID_ENABLE
 * parameter, which enables or
 * disables any module.
 * The fixed size of this structure is four bytes.
 */

struct afe_mod_enable_param {
	u16                  enable;
	/* Enables (1) or disables (0) the module. */

	u16                  reserved;
	/* This field must be set to zero.
		*/
} __packed;

/* ID of the configuration parameter used by the
 * #AFE_MODULE_SIDETONE_IIR_FILTER module.
 */
#define AFE_PARAM_ID_SIDETONE_IIR_FILTER_CONFIG	0x00010204

struct afe_sidetone_iir_filter_config_params {
	u16                  num_biquad_stages;
/* Number of stages.
 * Supported values: Minimum of 5 and maximum of 10
 */

	u16                  pregain;
/* Pregain for the compensating filter response.
 * Supported values: Any number in Q13 format
 */
} __packed;

#define AFE_MODULE_LOOPBACK	0x00010205
#define AFE_PARAM_ID_LOOPBACK_GAIN_PER_PATH	0x00010206

/* Payload of the #AFE_PARAM_ID_LOOPBACK_GAIN_PER_PATH parameter,
 * which gets/sets loopback gain of a port to an Rx port.
 * The Tx port ID of the loopback is part of the set_param command.
 */

/*  Payload of the #AFE_PORT_CMD_SET_PARAM_V2 command's
 * configuration/calibration settings for the AFE port.
 */
struct afe_port_cmd_set_param_v2 {
	u16 port_id;
/* Port interface and direction (Rx or Tx) to start.
 */

	u16 payload_size;
/* Actual size of the payload in bytes.
 * This is used for parsing the parameter payload.
 * Supported values: > 0
 */

u32 payload_address_lsw;
/* LSW of 64 bit Payload address.
 * Address should be 32-byte,
 * 4kbyte aligned and must be contiguous memory.
 */

u32 payload_address_msw;
/* MSW of 64 bit Payload address.
 * In case of 32-bit shared memory address,
 * this field must be set to zero.
 * In case of 36-bit shared memory address,
 * bit-4 to bit-31 must be set to zero.
 * Address should be 32-byte, 4kbyte aligned
 * and must be contiguous memory.
 */

u32 mem_map_handle;
/* Memory map handle returned by
 * AFE_SERVICE_CMD_SHARED_MEM_MAP_REGIONS commands.
 * Supported Values:
 * - NULL -- Message. The parameter data is in-band.
 * - Non-NULL -- The parameter data is Out-band.Pointer to
 * the physical address
 * in shared memory of the payload data.
 * An optional field is available if parameter
 * data is in-band:
 * afe_param_data_v2 param_data[...].
 * For detailed payload content, see the
 * afe_port_param_data_v2 structure.
 */
} __packed;

#define AFE_PORT_CMD_SET_PARAM_V2	0x000100EF

struct afe_port_param_data_v2 {
	u32 module_id;
/* ID of the module to be configured.
 * Supported values: Valid module ID
 */

u32 param_id;
/* ID of the parameter corresponding to the supported parameters
 * for the module ID.
 * Supported values: Valid parameter ID
 */

u16 param_size;
/* Actual size of the data for the
 * module_id/param_id pair. The size is a
 * multiple of four bytes.
 * Supported values: > 0
 */

u16 reserved;
/* This field must be set to zero.
 */
} __packed;

struct afe_loopback_gain_per_path_param {
	struct apr_hdr	hdr;
	struct afe_port_cmd_set_param_v2 param;
	struct afe_port_param_data_v2    pdata;
	u16                  rx_port_id;
/* Rx port of the loopback. */

u16                  gain;
/* Loopback gain per path of the port.
 * Supported values: Any number in Q13 format
 */
} __packed;

/* Parameter ID used to configure and enable/disable the
 * loopback path. The difference with respect to the existing
 * API, AFE_PORT_CMD_LOOPBACK, is that it allows Rx port to be
 * configured as source port in loopback path. Port-id in
 * AFE_PORT_CMD_SET_PARAM cmd is the source port whcih can be
 * Tx or Rx port. In addition, we can configure the type of
 * routing mode to handle different use cases.
 */
#define AFE_PARAM_ID_LOOPBACK_CONFIG	0x0001020B
#define AFE_API_VERSION_LOOPBACK_CONFIG	0x1

enum afe_loopback_routing_mode {
	LB_MODE_DEFAULT = 1,
	/* Regular loopback from source to destination port */
	LB_MODE_SIDETONE,
	/* Sidetone feed from Tx source to Rx destination port */
	LB_MODE_EC_REF_VOICE_AUDIO,
	/* Echo canceller reference, voice + audio + DTMF */
	LB_MODE_EC_REF_VOICE
	/* Echo canceller reference, voice alone */
} __packed;

/*  Payload of the #AFE_PARAM_ID_LOOPBACK_CONFIG ,
 * which enables/disables one AFE loopback.
 */
struct afe_loopback_cfg_v1 {
	struct apr_hdr	hdr;
	struct afe_port_cmd_set_param_v2 param;
	struct afe_port_param_data_v2    pdata;
	u32		loopback_cfg_minor_version;
/* Minor version used for tracking the version of the RMC module
 * configuration interface.
 * Supported values: #AFE_API_VERSION_LOOPBACK_CONFIG
 */
	u16                  dst_port_id;
	/* Destination Port Id. */
	u16                  routing_mode;
/* Specifies data path type from src to dest port.
 * Supported values:
 * #LB_MODE_DEFAULT
 * #LB_MODE_SIDETONE
 * #LB_MODE_EC_REF_VOICE_AUDIO
 * #LB_MODE_EC_REF_VOICE_A
 * #LB_MODE_EC_REF_VOICE
 */

	u16                  enable;
/* Specifies whether to enable (1) or
 * disable (0) an AFE loopback.
 */
	u16                  reserved;
/* Reserved for 32-bit alignment. This field must be set to 0.
 */

} __packed;

#define AFE_MODULE_SPEAKER_PROTECTION	0x00010209
#define AFE_PARAM_ID_SPKR_PROT_CONFIG	0x0001020a
#define AFE_API_VERSION_SPKR_PROT_CONFIG	0x1
#define AFE_SPKR_PROT_EXCURSIONF_LEN	512
struct afe_spkr_prot_cfg_param_v1 {
	u32       spkr_prot_minor_version;
/*
 * Minor version used for tracking the version of the
 * speaker protection module configuration interface.
 * Supported values: #AFE_API_VERSION_SPKR_PROT_CONFIG
 */

int16_t        win_size;
/* Analysis and synthesis window size (nWinSize).
 * Supported values: 1024, 512, 256 samples
 */

int16_t        margin;
/* Allowable margin for excursion prediction,
 * in L16Q15 format. This is a
 * control parameter to allow
 * for overestimation of peak excursion.
 */

int16_t        spkr_exc_limit;
/* Speaker excursion limit, in L16Q15 format.*/

int16_t        spkr_resonance_freq;
/* Resonance frequency of the speaker; used
 * to define a frequency range
 * for signal modification.
 *
 * Supported values: 0 to 2000 Hz */

int16_t        limhresh;
/* Threshold of the hard limiter; used to
 * prevent overshooting beyond a
 * signal level that was set by the limiter
 * prior to speaker protection.
 * Supported values: 0 to 32767
 */

int16_t        hpf_cut_off_freq;
/* High pass filter cutoff frequency.
 * Supported values: 100, 200, 300 Hz
 */

int16_t        hpf_enable;
/* Specifies whether the high pass filter
 * is enabled (0) or disabled (1).
 */

int16_t        reserved;
/* This field must be set to zero. */

int32_t        amp_gain;
/* Amplifier gain in L32Q15 format.
 * This is the RMS voltage at the
 * loudspeaker when a 0dBFS tone
 * is played in the digital domain.
 */

int16_t        excursionf[AFE_SPKR_PROT_EXCURSIONF_LEN];
/* Array of the excursion transfer function.
 * The peak excursion of the
 * loudspeaker diaphragm is
 * measured in millimeters for 1 Vrms Sine
 * tone at all FFT bin frequencies.
 * Supported values: Q15 format
 */
} __packed;


#define AFE_SERVICE_CMD_REGISTER_RT_PORT_DRIVER	0x000100E0

/*  Payload of the #AFE_SERVICE_CMD_REGISTER_RT_PORT_DRIVER
 * command, which registers a real-time port driver
 * with the AFE service.
 */
struct afe_service_cmd_register_rt_port_driver {
	struct apr_hdr hdr;
	u16                  port_id;
/* Port ID with which the real-time driver exchanges data
 * (registers for events).
 * Supported values: #AFE_PORT_ID_RT_PROXY_PORT_RANGE_START to
 * #AFE_PORT_ID_RT_PROXY_PORT_RANGE_END
 */

	u16                  reserved;
	/* This field must be set to zero. */
} __packed;

#define AFE_SERVICE_CMD_UNREGISTER_RT_PORT_DRIVER	0x000100E1

/*  Payload of the #AFE_SERVICE_CMD_UNREGISTER_RT_PORT_DRIVER
 * command, which unregisters a real-time port driver from
 * the AFE service.
 */
struct afe_service_cmd_unregister_rt_port_driver {
	struct apr_hdr hdr;
	u16                  port_id;
/* Port ID from which the real-time
 * driver unregisters for events.
 * Supported values: #AFE_PORT_ID_RT_PROXY_PORT_RANGE_START to
 * #AFE_PORT_ID_RT_PROXY_PORT_RANGE_END
 */

	u16                  reserved;
	/* This field must be set to zero.	*/
} __packed;

#define AFE_EVENT_RT_PROXY_PORT_STATUS	0x00010105
#define AFE_EVENTYPE_RT_PROXY_PORT_START	0
#define AFE_EVENTYPE_RT_PROXY_PORT_STOP	1
#define AFE_EVENTYPE_RT_PROXY_PORT_LOW_WATER_MARK	2
#define AFE_EVENTYPE_RT_PROXY_PORT_HIGH_WATER_MARK	3
#define AFE_EVENTYPE_RT_PROXY_PORT_INVALID	0xFFFF

/*  Payload of the #AFE_EVENT_RT_PROXY_PORT_STATUS
 * message, which sends an event from the AFE service
 * to a registered client.
 */
struct afe_event_rt_proxy_port_status {
	u16                  port_id;
/* Port ID to which the event is sent.
 * Supported values: #AFE_PORT_ID_RT_PROXY_PORT_RANGE_START to
 * #AFE_PORT_ID_RT_PROXY_PORT_RANGE_END
 */

	u16                  eventype;
/* Type of event.
 * Supported values:
 * - #AFE_EVENTYPE_RT_PROXY_PORT_START
 * - #AFE_EVENTYPE_RT_PROXY_PORT_STOP
 * - #AFE_EVENTYPE_RT_PROXY_PORT_LOW_WATER_MARK
 * - #AFE_EVENTYPE_RT_PROXY_PORT_HIGH_WATER_MARK
 */
} __packed;

#define AFE_PORT_DATA_CMD_RT_PROXY_PORT_WRITE_V2 0x000100ED

struct afe_port_data_cmd_rt_proxy_port_write_v2 {
	struct apr_hdr hdr;
	u16                  port_id;
/* Tx (mic) proxy port ID with which the real-time
 * driver exchanges data.
 * Supported values: #AFE_PORT_ID_RT_PROXY_PORT_RANGE_START to
 * #AFE_PORT_ID_RT_PROXY_PORT_RANGE_END
 */

	u16                  reserved;
	/* This field must be set to zero. */

	u32                  buffer_address_lsw;
/* LSW Address of the buffer containing the
 * data from the real-time source
 * device on a client.
 */

	u32                  buffer_address_msw;
/* MSW Address of the buffer containing the
 * data from the real-time source
 * device on a client.
 */

	u32					mem_map_handle;
/* A memory map handle encapsulating shared memory
 * attributes is returned if
 * AFE_SERVICE_CMD_SHARED_MEM_MAP_REGIONS
 * command is successful.
 * Supported Values:
 * - Any 32 bit value
 */

	u32                  available_bytes;
/* Number of valid bytes available
 * in the buffer (including all
 * channels: number of bytes per
 * channel = availableBytesumChannels).
 * Supported values: > 0
 *
 * This field must be equal to the frame
 * size specified in the #AFE_PORT_AUDIO_IF_CONFIG
 * command that was sent to configure this
 * port.
 */
} __packed;

#define AFE_PORT_DATA_CMD_RT_PROXY_PORT_READ_V2	0x000100EE

/*  Payload of the
 * #AFE_PORT_DATA_CMD_RT_PROXY_PORT_READ_V2 command, which
 * delivers an empty buffer to the AFE service. On
 * acknowledgment, data is filled in the buffer.
 */
struct afe_port_data_cmd_rt_proxy_port_read_v2 {
	struct apr_hdr hdr;
	u16                  port_id;
/* Rx proxy port ID with which the real-time
 * driver exchanges data.
 * Supported values: #AFE_PORT_ID_RT_PROXY_PORT_RANGE_START to
 * #AFE_PORT_ID_RT_PROXY_PORT_RANGE_END
 * (This must be an Rx (speaker) port.)
 */

	u16                  reserved;
	/* This field must be set to zero. */

	u32                  buffer_address_lsw;
/* LSW Address of the buffer containing the data sent from the AFE
 * service to a real-time sink device on the client.
 */


	u32                  buffer_address_msw;
/* MSW Address of the buffer containing the data sent from the AFE
 * service to a real-time sink device on the client.
 */

		u32				mem_map_handle;
/* A memory map handle encapsulating shared memory attributes is
 * returned if AFE_SERVICE_CMD_SHARED_MEM_MAP_REGIONS command is
 * successful.
 * Supported Values:
 * - Any 32 bit value
 */

	u32                  available_bytes;
/* Number of valid bytes available in the buffer (including all
 * channels).
 * Supported values: > 0
 * This field must be equal to the frame size specified in the
 * #AFE_PORT_AUDIO_IF_CONFIG command that was sent to configure
 * this port.
 */
} __packed;

/* This module ID is related to device configuring like I2S,PCM,
 * HDMI, SLIMBus etc. This module supports follwing parameter ids.
 * - #AFE_PARAM_ID_I2S_CONFIG
 * - #AFE_PARAM_ID_PCM_CONFIG
 * - #AFE_PARAM_ID_DIGI_MIC_CONFIG
 * - #AFE_PARAM_ID_HDMI_CONFIG
 * - #AFE_PARAM_ID_INTERNAL_BT_FM_CONFIG
 * - #AFE_PARAM_ID_SLIMBUS_CONFIG
 * - #AFE_PARAM_ID_RT_PROXY_CONFIG
 */

#define AFE_MODULE_AUDIO_DEV_INTERFACE    0x0001020C
#define AFE_PORT_SAMPLE_RATE_8K           8000
#define AFE_PORT_SAMPLE_RATE_16K          16000
#define AFE_PORT_SAMPLE_RATE_48K          48000
#define AFE_PORT_SAMPLE_RATE_96K          96000
#define AFE_PORT_SAMPLE_RATE_192K         192000
#define AFE_LINEAR_PCM_DATA				0x0
#define AFE_NON_LINEAR_DATA				0x1
#define AFE_LINEAR_PCM_DATA_PACKED_60958 0x2
#define AFE_NON_LINEAR_DATA_PACKED_60958 0x3

/* This param id is used to configure I2S interface */
#define AFE_PARAM_ID_I2S_CONFIG	0x0001020D
#define AFE_API_VERSION_I2S_CONFIG	0x1
/*	Enumeration for setting the I2S configuration
 * channel_mode parameter to
 * serial data wire number 1-3 (SD3).
 */
#define AFE_PORT_I2S_SD0                     0x1
#define AFE_PORT_I2S_SD1                     0x2
#define AFE_PORT_I2S_SD2                     0x3
#define AFE_PORT_I2S_SD3                     0x4
#define AFE_PORT_I2S_QUAD01                  0x5
#define AFE_PORT_I2S_QUAD23                  0x6
#define AFE_PORT_I2S_6CHS                    0x7
#define AFE_PORT_I2S_8CHS                    0x8
#define AFE_PORT_I2S_MONO                    0x0
#define AFE_PORT_I2S_STEREO                  0x1
#define AFE_PORT_CONFIG_I2S_WS_SRC_EXTERNAL  0x0
#define AFE_PORT_CONFIG_I2S_WS_SRC_INTERNAL  0x1

/*  Payload of the #AFE_PARAM_ID_I2S_CONFIG
 * command's (I2S configuration
 * parameter).
 */
struct afe_param_id_i2s_cfg {
	u32                  i2s_cfg_minor_version;
/* Minor version used for tracking the version of the I2S
 * configuration interface.
 * Supported values: #AFE_API_VERSION_I2S_CONFIG
 */

	u16                  bit_width;
/* Bit width of the sample.
 * Supported values: 16, 24
 */

	u16                  channel_mode;
/* I2S lines and multichannel operation.
 * Supported values:
 * - #AFE_PORT_I2S_SD0
 * - #AFE_PORT_I2S_SD1
 * - #AFE_PORT_I2S_SD2
 * - #AFE_PORT_I2S_SD3
 * - #AFE_PORT_I2S_QUAD01
 * - #AFE_PORT_I2S_QUAD23
 * - #AFE_PORT_I2S_6CHS
 * - #AFE_PORT_I2S_8CHS
 */

	u16                  mono_stereo;
/* Specifies mono or stereo. This applies only when
 * a single I2S line is used.
 * Supported values:
 * - #AFE_PORT_I2S_MONO
 * - #AFE_PORT_I2S_STEREO
 */

	u16                  ws_src;
/* Word select source: internal or external.
 * Supported values:
 * - #AFE_PORT_CONFIG_I2S_WS_SRC_EXTERNAL
 * - #AFE_PORT_CONFIG_I2S_WS_SRC_INTERNAL
 */

	u32                  sample_rate;
/* Sampling rate of the port.
 * Supported values:
 * - #AFE_PORT_SAMPLE_RATE_8K
 * - #AFE_PORT_SAMPLE_RATE_16K
 * - #AFE_PORT_SAMPLE_RATE_48K
 * - #AFE_PORT_SAMPLE_RATE_96K
 * - #AFE_PORT_SAMPLE_RATE_192K
 */

	u16					data_format;
/* data format
 * Supported values:
 * - #LINEAR_PCM_DATA
 * - #NON_LINEAR_DATA
 * - #LINEAR_PCM_DATA_PACKED_IN_60958
 * - #NON_LINEAR_DATA_PACKED_IN_60958
 */
		u16                  reserved;
	/* This field must be set to zero. */
} __packed;

/*
 * This param id is used to configure PCM interface
 */

#define AFE_API_VERSION_SPDIF_CONFIG 0x1
#define AFE_API_VERSION_SPDIF_CH_STATUS_CONFIG 0x1
#define AFE_API_VERSION_SPDIF_CLK_CONFIG 0x1
#define AFE_CH_STATUS_A 1
#define AFE_CH_STATUS_B 2

#define AFE_PARAM_ID_SPDIF_CONFIG 0x00010244
#define AFE_PARAM_ID_CH_STATUS_CONFIG 0x00010245
#define AFE_PARAM_ID_SPDIF_CLK_CONFIG 0x00010246

#define AFE_PORT_CLK_ROOT_LPAPLL 0x3
#define AFE_PORT_CLK_ROOT_LPAQ6PLL   0x4

struct afe_param_id_spdif_cfg {
/* Minor version used for tracking the version of the SPDIF
 * configuration interface.
 * Supported values: #AFE_API_VERSION_SPDIF_CONFIG
 */
	u32	spdif_cfg_minor_version;

/* Sampling rate of the port.
 * Supported values:
 * - #AFE_PORT_SAMPLE_RATE_22_05K
 * - #AFE_PORT_SAMPLE_RATE_32K
 * - #AFE_PORT_SAMPLE_RATE_44_1K
 * - #AFE_PORT_SAMPLE_RATE_48K
 * - #AFE_PORT_SAMPLE_RATE_96K
 * - #AFE_PORT_SAMPLE_RATE_176_4K
 * - #AFE_PORT_SAMPLE_RATE_192K
 */
	u32	sample_rate;

/* data format
 * Supported values:
 * - #AFE_LINEAR_PCM_DATA
 * - #AFE_NON_LINEAR_DATA
 */
	u16	data_format;
/* Number of channels supported by the port
 * - PCM - 1, Compressed Case - 2
 */
	u16	num_channels;
/* Bit width of the sample.
 * Supported values: 16, 24
 */
	u16	bit_width;
/* This field must be set to zero. */
	u16	reserved;
} __packed;

struct afe_param_id_spdif_ch_status_cfg {
	u32 ch_status_cfg_minor_version;
/* Minor version used for tracking the version of channel
 * status configuration. Current supported version is 1
 */

	u32 status_type;
/* Indicate if the channel status is for channel A or B
 * Supported values:
 * - #AFE_CH_STATUS_A
 * - #AFE_CH_STATUS_B
 */

	u8 status_bits[24];
/* Channel status - 192 bits for channel
 * Byte ordering as defined by IEC60958-3
 */

	u8 status_mask[24];
/* Channel status with mask bits 1 will be applied.
 * Byte ordering as defined by IEC60958-3
 */
} __packed;

struct afe_param_id_spdif_clk_cfg {
	u32 clk_cfg_minor_version;
/* Minor version used for tracking the version of SPDIF
 * interface clock configuration. Current supported version
 * is 1
 */

	u32 clk_value;
/* Specifies the clock frequency in Hz to set
 * Supported values:
 * 0 - Disable the clock
 * 2 (byphase) * 32 (60958 subframe size) * sampling rate * 2
 * (channels A and B)
 */

	u32 clk_root;
/* Specifies SPDIF root clk source
 * Supported Values:
 * - #AFE_PORT_CLK_ROOT_LPAPLL
 * - #AFE_PORT_CLK_ROOT_LPAQ6PLL
 */
} __packed;

struct afe_spdif_clk_config_command {
	struct apr_hdr                    hdr;
	struct afe_port_cmd_set_param_v2  param;
	struct afe_port_param_data_v2     pdata;
	struct afe_param_id_spdif_clk_cfg clk_cfg;
} __packed;

struct afe_spdif_chstatus_config_command {
	struct apr_hdr                    hdr;
	struct afe_port_cmd_set_param_v2  param;
	struct afe_port_param_data_v2     pdata;
	struct afe_param_id_spdif_ch_status_cfg ch_status;
} __packed;

struct afe_spdif_port_config {
	struct afe_param_id_spdif_cfg            cfg;
	struct afe_param_id_spdif_ch_status_cfg  ch_status;
} __packed;

#define AFE_PARAM_ID_PCM_CONFIG        0x0001020E
#define AFE_API_VERSION_PCM_CONFIG	0x1
/* Enumeration for the auxiliary PCM synchronization signal
 * provided by an external source.
 */

#define AFE_PORT_PCM_SYNC_SRC_EXTERNAL 0x0
/*	Enumeration for the auxiliary PCM synchronization signal
 * provided by an internal source.
 */
#define AFE_PORT_PCM_SYNC_SRC_INTERNAL  0x1
/*	Enumeration for the PCM configuration aux_mode parameter,
 * which configures the auxiliary PCM interface to use
 * short synchronization.
 */
#define AFE_PORT_PCM_AUX_MODE_PCM  0x0
/*
 * Enumeration for the PCM configuration aux_mode parameter,
 * which configures the auxiliary PCM interface to use long
 * synchronization.
 */
#define AFE_PORT_PCM_AUX_MODE_AUX    0x1
/*
 * Enumeration for setting the PCM configuration frame to 8.
 */
#define AFE_PORT_PCM_BITS_PER_FRAME_8  0x0
/*
 * Enumeration for setting the PCM configuration frame to 16.
 */
#define AFE_PORT_PCM_BITS_PER_FRAME_16   0x1

/*	Enumeration for setting the PCM configuration frame to 32.*/
#define AFE_PORT_PCM_BITS_PER_FRAME_32 0x2

/*	Enumeration for setting the PCM configuration frame to 64.*/
#define AFE_PORT_PCM_BITS_PER_FRAME_64   0x3

/*	Enumeration for setting the PCM configuration frame to 128.*/
#define AFE_PORT_PCM_BITS_PER_FRAME_128 0x4

/*	Enumeration for setting the PCM configuration frame to 256.*/
#define AFE_PORT_PCM_BITS_PER_FRAME_256 0x5

/*	Enumeration for setting the PCM configuration
 * quantype parameter to A-law with no padding.
 */
#define AFE_PORT_PCM_ALAW_NOPADDING 0x0

/* Enumeration for setting the PCM configuration quantype
 * parameter to mu-law with no padding.
 */
#define AFE_PORT_PCM_MULAW_NOPADDING 0x1
/*	Enumeration for setting the PCM configuration quantype
 * parameter to linear with no padding.
 */
#define AFE_PORT_PCM_LINEAR_NOPADDING 0x2
/*	Enumeration for setting the PCM configuration quantype
 * parameter to A-law with padding.
 */
#define AFE_PORT_PCM_ALAW_PADDING  0x3
/*	Enumeration for setting the PCM configuration quantype
 * parameter to mu-law with padding.
 */
#define AFE_PORT_PCM_MULAW_PADDING 0x4
/*	Enumeration for setting the PCM configuration quantype
 * parameter to linear with padding.
 */
#define AFE_PORT_PCM_LINEAR_PADDING 0x5
/*	Enumeration for disabling the PCM configuration
 * ctrl_data_out_enable parameter.
 * The PCM block is the only master.
 */
#define AFE_PORT_PCM_CTRL_DATA_OE_DISABLE 0x0
/*
 * Enumeration for enabling the PCM configuration
 * ctrl_data_out_enable parameter. The PCM block shares
 * the signal with other masters.
 */
#define AFE_PORT_PCM_CTRL_DATA_OE_ENABLE  0x1

/*  Payload of the #AFE_PARAM_ID_PCM_CONFIG command's
 * (PCM configuration parameter).
 */

struct afe_param_id_pcm_cfg {
	u32                  pcm_cfg_minor_version;
/* Minor version used for tracking the version of the AUX PCM
 * configuration interface.
 * Supported values: #AFE_API_VERSION_PCM_CONFIG
 */

	u16                  aux_mode;
/* PCM synchronization setting.
 * Supported values:
 * - #AFE_PORT_PCM_AUX_MODE_PCM
 * - #AFE_PORT_PCM_AUX_MODE_AUX
 */

	u16                  sync_src;
/* Synchronization source.
 * Supported values:
 * - #AFE_PORT_PCM_SYNC_SRC_EXTERNAL
 * - #AFE_PORT_PCM_SYNC_SRC_INTERNAL
 */

	u16                  frame_setting;
/* Number of bits per frame.
 * Supported values:
 * - #AFE_PORT_PCM_BITS_PER_FRAME_8
 * - #AFE_PORT_PCM_BITS_PER_FRAME_16
 * - #AFE_PORT_PCM_BITS_PER_FRAME_32
 * - #AFE_PORT_PCM_BITS_PER_FRAME_64
 * - #AFE_PORT_PCM_BITS_PER_FRAME_128
 * - #AFE_PORT_PCM_BITS_PER_FRAME_256
 */

	u16                  quantype;
/* PCM quantization type.
 * Supported values:
 * - #AFE_PORT_PCM_ALAW_NOPADDING
 * - #AFE_PORT_PCM_MULAW_NOPADDING
 * - #AFE_PORT_PCM_LINEAR_NOPADDING
 * - #AFE_PORT_PCM_ALAW_PADDING
 * - #AFE_PORT_PCM_MULAW_PADDING
 * - #AFE_PORT_PCM_LINEAR_PADDING
 */

	u16                  ctrl_data_out_enable;
/* Specifies whether the PCM block shares the data-out
 * signal to the drive with other masters.
 * Supported values:
 * - #AFE_PORT_PCM_CTRL_DATA_OE_DISABLE
 * - #AFE_PORT_PCM_CTRL_DATA_OE_ENABLE
 */
		u16                  reserved;
	/* This field must be set to zero. */

	u32                  sample_rate;
/* Sampling rate of the port.
 * Supported values:
 * - #AFE_PORT_SAMPLE_RATE_8K
 * - #AFE_PORT_SAMPLE_RATE_16K
 */

	u16                  bit_width;
/* Bit width of the sample.
 * Supported values: 16
 */

	u16                  num_channels;
/* Number of channels.
 * Supported values: 1 to 4
 */

	u16                  slot_number_mapping[4];
/* Specifies the slot number for the each channel in
 * multi channel scenario.
 * Supported values: 1 to 32
 */
} __packed;

/*
 * This param id is used to configure DIGI MIC interface
 */
#define AFE_PARAM_ID_DIGI_MIC_CONFIG	0x0001020F
/*  This version information is used to handle the new
 *   additions to the config interface in future in backward
 *   compatible manner.
 */
#define AFE_API_VERSION_DIGI_MIC_CONFIG 0x1

/* Enumeration for setting the digital mic configuration
 * channel_mode parameter to left 0.
 */

#define AFE_PORT_DIGI_MIC_MODE_LEFT0  0x1

/*Enumeration for setting the digital mic configuration
 * channel_mode parameter to right 0.
 */


#define AFE_PORT_DIGI_MIC_MODE_RIGHT0  0x2

/* Enumeration for setting the digital mic configuration
 * channel_mode parameter to left 1.
 */

#define AFE_PORT_DIGI_MIC_MODE_LEFT1  0x3

/* Enumeration for setting the digital mic configuration
 * channel_mode parameter to right 1.
 */

#define AFE_PORT_DIGI_MIC_MODE_RIGHT1 0x4

/* Enumeration for setting the digital mic configuration
 * channel_mode parameter to stereo 0.
 */
#define AFE_PORT_DIGI_MIC_MODE_STEREO0  0x5

/* Enumeration for setting the digital mic configuration
 * channel_mode parameter to stereo 1.
 */


#define AFE_PORT_DIGI_MIC_MODE_STEREO1    0x6

/* Enumeration for setting the digital mic configuration
 * channel_mode parameter to quad.
 */

#define AFE_PORT_DIGI_MIC_MODE_QUAD     0x7

/*  Payload of the #AFE_PARAM_ID_DIGI_MIC_CONFIG command's
 * (DIGI MIC configuration
 * parameter).
 */
struct afe_param_id_digi_mic_cfg {
	u32                  digi_mic_cfg_minor_version;
/* Minor version used for tracking the version of the DIGI Mic
 * configuration interface.
 * Supported values: #AFE_API_VERSION_DIGI_MIC_CONFIG
 */

	u16                  bit_width;
/* Bit width of the sample.
 * Supported values: 16
 */

	u16                  channel_mode;
/* Digital mic and multichannel operation.
 * Supported values:
 * - #AFE_PORT_DIGI_MIC_MODE_LEFT0
 * - #AFE_PORT_DIGI_MIC_MODE_RIGHT0
 * - #AFE_PORT_DIGI_MIC_MODE_LEFT1
 * - #AFE_PORT_DIGI_MIC_MODE_RIGHT1
 * - #AFE_PORT_DIGI_MIC_MODE_STEREO0
 * - #AFE_PORT_DIGI_MIC_MODE_STEREO1
 * - #AFE_PORT_DIGI_MIC_MODE_QUAD
 */

	u32                  sample_rate;
/* Sampling rate of the port.
 * Supported values:
 * - #AFE_PORT_SAMPLE_RATE_8K
 * - #AFE_PORT_SAMPLE_RATE_16K
 * - #AFE_PORT_SAMPLE_RATE_48K
 */
} __packed;

/*
* This param id is used to configure HDMI interface
*/
#define AFE_PARAM_ID_HDMI_CONFIG     0x00010210

/*  This version information is used to handle the new
*   additions to the config interface in future in backward
*   compatible manner.
*/
#define AFE_API_VERSION_HDMI_CONFIG 0x1

/* Payload of the #AFE_PARAM_ID_HDMI_CONFIG command,
 * which configures a multichannel HDMI audio interface.
 */
struct afe_param_id_hdmi_multi_chan_audio_cfg {
	u32                  hdmi_cfg_minor_version;
/* Minor version used for tracking the version of the HDMI
 * configuration interface.
 * Supported values: #AFE_API_VERSION_HDMI_CONFIG
 */

u16                  datatype;
/* data type
 * Supported values:
 * - #LINEAR_PCM_DATA
 * - #NON_LINEAR_DATA
 * - #LINEAR_PCM_DATA_PACKED_IN_60958
 * - #NON_LINEAR_DATA_PACKED_IN_60958
 */

u16                  channel_allocation;
/* HDMI channel allocation information for programming an HDMI
 * frame. The default is 0 (Stereo).
 *
 * This information is defined in the HDMI standard, CEA 861-D
 * (refer to @xhyperref{S1,[S1]}). The number of channels is also
 * inferred from this parameter.
*/


u32                  sample_rate;
/* Sampling rate of the port.
 * Supported values:
 * - #AFE_PORT_SAMPLE_RATE_8K
 * - #AFE_PORT_SAMPLE_RATE_16K
 * - #AFE_PORT_SAMPLE_RATE_48K
 * - #AFE_PORT_SAMPLE_RATE_96K
 * - 22050, 44100, 176400 for compressed streams
 */

	u16                  bit_width;
/* Bit width of the sample.
 * Supported values: 16, 24
 */
		u16                  reserved;
	/* This field must be set to zero. */
} __packed;

/*
* This param id is used to configure BT or FM(RIVA) interface
*/
#define AFE_PARAM_ID_INTERNAL_BT_FM_CONFIG  0x00010211

/*  This version information is used to handle the new
*   additions to the config interface in future in backward
*   compatible manner.
*/
#define AFE_API_VERSION_INTERNAL_BT_FM_CONFIG	0x1

/*  Payload of the #AFE_PARAM_ID_INTERNAL_BT_FM_CONFIG
 * command's BT voice/BT audio/FM configuration parameter.
 */
struct afe_param_id_internal_bt_fm_cfg {
	u32                  bt_fm_cfg_minor_version;
/* Minor version used for tracking the version of the BT and FM
 * configuration interface.
 * Supported values: #AFE_API_VERSION_INTERNAL_BT_FM_CONFIG
 */

	u16                  num_channels;
/* Number of channels.
 * Supported values: 1 to 2
 */

	u16                  bit_width;
/* Bit width of the sample.
 * Supported values: 16
 */

	u32                  sample_rate;
/* Sampling rate of the port.
 * Supported values:
 * - #AFE_PORT_SAMPLE_RATE_8K (only for BTSCO)
 * - #AFE_PORT_SAMPLE_RATE_16K (only for BTSCO)
 * - #AFE_PORT_SAMPLE_RATE_48K (FM and A2DP)
 */
} __packed;

/* This param id is used to configure SLIMBUS interface using
 * shared channel approach.
 */


#define AFE_PARAM_ID_SLIMBUS_CONFIG    0x00010212

/*  This version information is used to handle the new
*   additions to the config interface in future in backward
*   compatible manner.
*/
#define AFE_API_VERSION_SLIMBUS_CONFIG 0x1

/*	Enumeration for setting SLIMbus device ID 1.
*/
#define AFE_SLIMBUS_DEVICE_1           0x0

/*	Enumeration for setting SLIMbus device ID 2.
*/
#define AFE_SLIMBUS_DEVICE_2          0x1

/*	Enumeration for setting the SLIMbus data formats.
*/
#define AFE_SB_DATA_FORMAT_NOT_INDICATED 0x0

/* Enumeration for setting the maximum number of streams per
 * device.
 */

#define AFE_PORT_MAX_AUDIO_CHAN_CNT	0x8

/* Payload of the #AFE_PORT_CMD_SLIMBUS_CONFIG command's SLIMbus
 * port configuration parameter.
 */

struct afe_param_id_slimbus_cfg {
	u32                  sb_cfg_minor_version;
/* Minor version used for tracking the version of the SLIMBUS
 * configuration interface.
 * Supported values: #AFE_API_VERSION_SLIMBUS_CONFIG
 */

	u16                  slimbus_dev_id;
/* SLIMbus hardware device ID, which is required to handle
 * multiple SLIMbus hardware blocks.
 * Supported values: - #AFE_SLIMBUS_DEVICE_1 - #AFE_SLIMBUS_DEVICE_2
 */


	u16                  bit_width;
/* Bit width of the sample.
 * Supported values: 16, 24
 */

	u16                  data_format;
/* Data format supported by the SLIMbus hardware. The default is
 * 0 (#AFE_SB_DATA_FORMAT_NOT_INDICATED), which indicates the
 * hardware does not perform any format conversions before the data
 * transfer.
 */


	u16                  num_channels;
/* Number of channels.
 * Supported values: 1 to #AFE_PORT_MAX_AUDIO_CHAN_CNT
 */

	u8  shared_ch_mapping[AFE_PORT_MAX_AUDIO_CHAN_CNT];
/* Mapping of shared channel IDs (128 to 255) to which the
 * master port is to be connected.
 * Shared_channel_mapping[i] represents the shared channel assigned
 * for audio channel i in multichannel audio data.
 */

	u32              sample_rate;
/* Sampling rate of the port.
 * Supported values:
 * - #AFE_PORT_SAMPLE_RATE_8K
 * - #AFE_PORT_SAMPLE_RATE_16K
 * - #AFE_PORT_SAMPLE_RATE_48K
 * - #AFE_PORT_SAMPLE_RATE_96K
 * - #AFE_PORT_SAMPLE_RATE_192K
 */
} __packed;

/*
* This param id is used to configure Real Time Proxy interface.
*/
#define AFE_PARAM_ID_RT_PROXY_CONFIG 0x00010213

/*  This version information is used to handle the new
*   additions to the config interface in future in backward
*   compatible manner.
*/
#define AFE_API_VERSION_RT_PROXY_CONFIG 0x1

/*  Payload of the #AFE_PARAM_ID_RT_PROXY_CONFIG
 * command (real-time proxy port configuration parameter).
 */
struct afe_param_id_rt_proxy_port_cfg {
	u32                  rt_proxy_cfg_minor_version;
/* Minor version used for tracking the version of rt-proxy
 * config interface.
 */

	u16                  bit_width;
/* Bit width of the sample.
 * Supported values: 16
 */

	u16                  interleaved;
/* Specifies whether the data exchanged between the AFE
 * interface and real-time port is interleaved.
 * Supported values: - 0 -- Non-interleaved (samples from each
 * channel are contiguous in the buffer) - 1 -- Interleaved
 * (corresponding samples from each input channel are interleaved
 * within the buffer)
 */


	u16                  frame_size;
 /* Size of the frames that are used for PCM exchanges with this
 * port.
 * Supported values: > 0, in bytes
 * For example, 5 ms buffers of 16 bits and 16 kHz stereo samples
 * is 5 ms * 16 samples/ms * 2 bytes/sample * 2 channels = 320
 * bytes.
 */
	u16                  jitter_allowance;
/* Configures the amount of jitter that the port will allow.
 * Supported values: > 0
 * For example, if +/-10 ms of jitter is anticipated in the timing
 * of sending frames to the port, and the configuration is 16 kHz
 * mono with 16-bit samples, this field is 10 ms * 16 samples/ms * 2
 * bytes/sample = 320.
 */

	u16                  low_water_mark;
/* Low watermark in bytes (including all channels).
 * Supported values:
 * - 0 -- Do not send any low watermark events
 * - > 0 -- Low watermark for triggering an event
 * If the number of bytes in an internal circular buffer is lower
 * than this low_water_mark parameter, a LOW_WATER_MARK event is
 * sent to applications (via the #AFE_EVENT_RT_PROXY_PORT_STATUS
 * event).
 * Use of watermark events is optional for debugging purposes.
 */

	u16                  high_water_mark;
/* High watermark in bytes (including all channels).
 * Supported values:
 * - 0 -- Do not send any high watermark events
 * - > 0 -- High watermark for triggering an event
 * If the number of bytes in an internal circular buffer exceeds
 * TOTAL_CIRC_BUF_SIZE minus high_water_mark, a high watermark event
 * is sent to applications (via the #AFE_EVENT_RT_PROXY_PORT_STATUS
 * event).
 * The use of watermark events is optional and for debugging
 * purposes.
 */


	u32					sample_rate;
/* Sampling rate of the port.
 * Supported values:
 * - #AFE_PORT_SAMPLE_RATE_8K
 * - #AFE_PORT_SAMPLE_RATE_16K
 * - #AFE_PORT_SAMPLE_RATE_48K
 */

	u16                  num_channels;
/* Number of channels.
 * Supported values: 1 to #AFE_PORT_MAX_AUDIO_CHAN_CNT
 */

	u16                  reserved;
	/* For 32 bit alignment. */
} __packed;


/* This param id is used to configure the Pseudoport interface */

#define AFE_PARAM_ID_PSEUDO_PORT_CONFIG	0x00010219

/* Version information used to handle future additions to the configuration
 * interface (for backward compatibility).
 */
#define AFE_API_VERSION_PSEUDO_PORT_CONFIG                          0x1

/* Enumeration for setting the timing_mode parameter to faster than real
 * time.
 */
#define AFE_PSEUDOPORT_TIMING_MODE_FTRT                             0x0

/* Enumeration for setting the timing_mode parameter to real time using
 * timers.
 */
#define AFE_PSEUDOPORT_TIMING_MODE_TIMER                            0x1

/* Payload of the AFE_PARAM_ID_PSEUDO_PORT_CONFIG parameter used by
    AFE_MODULE_AUDIO_DEV_INTERFACE.
*/
struct afe_param_id_pseudo_port_cfg {
	u32                  pseud_port_cfg_minor_version;
	/*
	 * Minor version used for tracking the version of the pseudoport
	 * configuration interface.
	 */

	u16                  bit_width;
	/* Bit width of the sample at values 16, 24 */

	u16                  num_channels;
	/* Number of channels at values  1 to 8 */

	u16                  data_format;
	/* Non-linear data format supported by the pseudoport (for future use).
	 * At values #AFE_LINEAR_PCM_DATA
	 */

	u16                  timing_mode;
	/* Indicates whether the pseudoport synchronizes to the clock or
	 * operates faster than real time.
	 * at values
	 * - #AFE_PSEUDOPORT_TIMING_MODE_FTRT
	 * - #AFE_PSEUDOPORT_TIMING_MODE_TIMER @tablebulletend
	 */

	u32                  sample_rate;
	/* Sample rate at which the pseudoport will run.
	 * at values
	 * - #AFE_PORT_SAMPLE_RATE_8K
	 * - #AFE_PORT_SAMPLE_RATE_32K
	 * - #AFE_PORT_SAMPLE_RATE_48K
	 * - #AFE_PORT_SAMPLE_RATE_96K
	 * - #AFE_PORT_SAMPLE_RATE_192K @tablebulletend
	 */
} __packed;

#define AFE_PARAM_ID_DEVICE_HW_DELAY     0x00010243
#define AFE_API_VERSION_DEVICE_HW_DELAY  0x1

struct afe_param_id_device_hw_delay_cfg {
	uint32_t    device_hw_delay_minor_version;
	uint32_t    delay_in_us;
} __packed;

union afe_port_config {
	struct afe_param_id_pcm_cfg               pcm;
	struct afe_param_id_i2s_cfg               i2s;
	struct afe_param_id_hdmi_multi_chan_audio_cfg hdmi_multi_ch;
	struct afe_param_id_slimbus_cfg           slim_sch;
	struct afe_param_id_rt_proxy_port_cfg     rtproxy;
	struct afe_param_id_internal_bt_fm_cfg    int_bt_fm;
	struct afe_param_id_pseudo_port_cfg       pseudo_port;
	struct afe_param_id_device_hw_delay_cfg   hw_delay;
	struct afe_param_id_spdif_cfg             spdif;
} __packed;

struct afe_audioif_config_command_no_payload {
	struct apr_hdr			hdr;
	struct afe_port_cmd_set_param_v2 param;
} __packed;

struct afe_audioif_config_command {
	struct apr_hdr			hdr;
	struct afe_port_cmd_set_param_v2 param;
	struct afe_port_param_data_v2    pdata;
	union afe_port_config            port;
} __packed;

#define AFE_PORT_CMD_DEVICE_START 0x000100E5

/*  Payload of the #AFE_PORT_CMD_DEVICE_START.*/
struct afe_port_cmd_device_start {
	struct apr_hdr hdr;
	u16                  port_id;
/* Port interface and direction (Rx or Tx) to start. An even
 * number represents the Rx direction, and an odd number represents
 * the Tx direction.
 */


	u16                  reserved;
/* Reserved for 32-bit alignment. This field must be set to 0.*/

} __packed;

#define AFE_PORT_CMD_DEVICE_STOP  0x000100E6

/*  Payload of the #AFE_PORT_CMD_DEVICE_STOP.
*/
struct afe_port_cmd_device_stop {
	struct apr_hdr hdr;
	u16                  port_id;
/* Port interface and direction (Rx or Tx) to start. An even
 * number represents the Rx direction, and an odd number represents
 * the Tx direction.
 */

	u16                  reserved;
/* Reserved for 32-bit alignment. This field must be set to 0.*/
} __packed;

#define AFE_SERVICE_CMD_SHARED_MEM_MAP_REGIONS 0x000100EA

/*  Memory map regions command payload used by the
 * #AFE_SERVICE_CMD_SHARED_MEM_MAP_REGIONS .
 * This structure allows clients to map multiple shared memory
 * regions in a single command. Following this structure are
 * num_regions of afe_service_shared_map_region_payload.
 */
struct afe_service_cmd_shared_mem_map_regions {
	struct apr_hdr hdr;
u16                  mem_pool_id;
/* Type of memory on which this memory region is mapped.
 * Supported values:
 * - #ADSP_MEMORY_MAP_EBI_POOL
 * - #ADSP_MEMORY_MAP_SMI_POOL
 * - #ADSP_MEMORY_MAP_SHMEM8_4K_POOL
 * - Other values are reserved
 *
 * The memory pool ID implicitly defines the characteristics of the
 * memory. Characteristics may include alignment type, permissions,
 * etc.
 *
 * ADSP_MEMORY_MAP_EBI_POOL is External Buffer Interface type memory
 * ADSP_MEMORY_MAP_SMI_POOL is Shared Memory Interface type memory
 * ADSP_MEMORY_MAP_SHMEM8_4K_POOL is shared memory, byte
 * addressable, and 4 KB aligned.
 */


	u16                  num_regions;
/* Number of regions to map.
 * Supported values:
 * - Any value greater than zero
 */

	u32                  property_flag;
/* Configures one common property for all the regions in the
 * payload.
 *
 * Supported values: - 0x00000000 to 0x00000001
 *
 * b0 - bit 0 indicates physical or virtual mapping 0 Shared memory
 * address provided in afe_service_shared_map_region_payloadis a
 * physical address. The shared memory needs to be mapped( hardware
 * TLB entry) and a software entry needs to be added for internal
 * book keeping.
 *
 * 1 Shared memory address provided in
 * afe_service_shared_map_region_payloadis a virtual address. The
 * shared memory must not be mapped (since hardware TLB entry is
 * already available) but a software entry needs to be added for
 * internal book keeping. This can be useful if two services with in
 * ADSP is communicating via APR. They can now directly communicate
 * via the Virtual address instead of Physical address. The virtual
 * regions must be contiguous. num_regions must be 1 in this case.
 *
 * b31-b1 - reserved bits. must be set to zero
 */


} __packed;
/*  Map region payload used by the
 * afe_service_shared_map_region_payloadstructure.
 */
struct afe_service_shared_map_region_payload {
	u32                  shm_addr_lsw;
/* least significant word of starting address in the memory
 * region to map. It must be contiguous memory, and it must be 4 KB
 * aligned.
 * Supported values: - Any 32 bit value
 */


	u32                  shm_addr_msw;
/* most significant word of startng address in the memory region
 * to map. For 32 bit shared memory address, this field must be set
 * to zero. For 36 bit shared memory address, bit31 to bit 4 must be
 * set to zero
 *
 * Supported values: - For 32 bit shared memory address, this field
 * must be set to zero. - For 36 bit shared memory address, bit31 to
 * bit 4 must be set to zero - For 64 bit shared memory address, any
 * 32 bit value
 */


	u32                  mem_size_bytes;
/* Number of bytes in the region. The aDSP will always map the
 * regions as virtual contiguous memory, but the memory size must be
 * in multiples of 4 KB to avoid gaps in the virtually contiguous
 * mapped memory.
 *
 * Supported values: - multiples of 4KB
 */

} __packed;

#define AFE_SERVICE_CMDRSP_SHARED_MEM_MAP_REGIONS 0x000100EB
struct afe_service_cmdrsp_shared_mem_map_regions {
	u32                  mem_map_handle;
/* A memory map handle encapsulating shared memory attributes is
 * returned iff AFE_SERVICE_CMD_SHARED_MEM_MAP_REGIONS command is
 * successful. In the case of failure , a generic APR error response
 * is returned to the client.
 *
 * Supported Values: - Any 32 bit value
 */

} __packed;
#define AFE_SERVICE_CMD_SHARED_MEM_UNMAP_REGIONS 0x000100EC
/* Memory unmap regions command payload used by the
 * #AFE_SERVICE_CMD_SHARED_MEM_UNMAP_REGIONS
 *
 * This structure allows clients to unmap multiple shared memory
 * regions in a single command.
 */


struct afe_service_cmd_shared_mem_unmap_regions {
	struct apr_hdr hdr;
u32                  mem_map_handle;
/* memory map handle returned by
 * AFE_SERVICE_CMD_SHARED_MEM_MAP_REGIONS commands
 *
 * Supported Values:
 * - Any 32 bit value
 */
} __packed;

#define  AFE_PORT_CMD_GET_PARAM_V2 0x000100F0

/*  Payload of the #AFE_PORT_CMD_GET_PARAM_V2 command,
 * which queries for one post/preprocessing parameter of a
 * stream.
 */
struct afe_port_cmd_get_param_v2 {
	u16 port_id;
/* Port interface and direction (Rx or Tx) to start. */

	u16 payload_size;
/* Maximum data size of the parameter ID/module ID combination.
 * This is a multiple of four bytes
 * Supported values: > 0
 */

	u32 payload_address_lsw;
/* LSW of 64 bit Payload address. Address should be 32-byte,
 * 4kbyte aligned and must be contig memory.
 */


	u32 payload_address_msw;
/* MSW of 64 bit Payload address. In case of 32-bit shared
 * memory address, this field must be set to zero. In case of 36-bit
 * shared memory address, bit-4 to bit-31 must be set to zero.
 * Address should be 32-byte, 4kbyte aligned and must be contiguous
 * memory.
 */

	u32 mem_map_handle;
/* Memory map handle returned by
 * AFE_SERVICE_CMD_SHARED_MEM_MAP_REGIONS commands.
 * Supported Values: - NULL -- Message. The parameter data is
 * in-band. - Non-NULL -- The parameter data is Out-band.Pointer to
 * - the physical address in shared memory of the payload data.
 * For detailed payload content, see the afe_port_param_data_v2
 * structure
 */


	u32 module_id;
/* ID of the module to be queried.
 * Supported values: Valid module ID
 */

	u32 param_id;
/* ID of the parameter to be queried.
 * Supported values: Valid parameter ID
 */
} __packed;

#define AFE_PORT_CMDRSP_GET_PARAM_V2 0x00010106

/* Payload of the #AFE_PORT_CMDRSP_GET_PARAM_V2 message, which
 * responds to an #AFE_PORT_CMD_GET_PARAM_V2 command.
 *
 * Immediately following this structure is the parameters structure
 * (afe_port_param_data) containing the response(acknowledgment)
 * parameter payload. This payload is included for an in-band
 * scenario. For an address/shared memory-based set parameter, this
 * payload is not needed.
 */


struct afe_port_cmdrsp_get_param_v2 {
	u32                  status;
} __packed;

/* adsp_afe_service_commands.h */

#define ADSP_MEMORY_MAP_EBI_POOL      0

#define ADSP_MEMORY_MAP_SMI_POOL      1
#define ADSP_MEMORY_MAP_IMEM_POOL      2
#define ADSP_MEMORY_MAP_SHMEM8_4K_POOL      3
/*
* Definition of virtual memory flag
*/
#define ADSP_MEMORY_MAP_VIRTUAL_MEMORY 1

/*
* Definition of physical memory flag
*/
#define ADSP_MEMORY_MAP_PHYSICAL_MEMORY 0

#define NULL_POPP_TOPOLOGY				0x00010C68
#define NULL_COPP_TOPOLOGY				0x00010312
#define DEFAULT_COPP_TOPOLOGY				0x00010BE3
#define DEFAULT_POPP_TOPOLOGY				0x00010BE4
#define VPM_TX_SM_ECNS_COPP_TOPOLOGY			0x00010F71
#define VPM_TX_DM_FLUENCE_COPP_TOPOLOGY			0x00010F72
#define VPM_TX_QMIC_FLUENCE_COPP_TOPOLOGY		0x00010F75
#define VPM_TX_DM_RFECNS_COPP_TOPOLOGY			0x00010F86
#define ADM_CMD_COPP_OPEN_TOPOLOGY_ID_DTS_HPX_0		0x00010347
#define ADM_CMD_COPP_OPEN_TOPOLOGY_ID_DTS_HPX_1		0x00010348

/* Memory map regions command payload used by the
 * #ASM_CMD_SHARED_MEM_MAP_REGIONS ,#ADM_CMD_SHARED_MEM_MAP_REGIONS
 * commands.
 *
 * This structure allows clients to map multiple shared memory
 * regions in a single command. Following this structure are
 * num_regions of avs_shared_map_region_payload.
 */


struct avs_cmd_shared_mem_map_regions {
	struct apr_hdr hdr;
	u16                  mem_pool_id;
/* Type of memory on which this memory region is mapped.
 *
 * Supported values: - #ADSP_MEMORY_MAP_EBI_POOL -
 * #ADSP_MEMORY_MAP_SMI_POOL - #ADSP_MEMORY_MAP_IMEM_POOL
 * (unsupported) - #ADSP_MEMORY_MAP_SHMEM8_4K_POOL - Other values
 * are reserved
 *
 * The memory ID implicitly defines the characteristics of the
 * memory. Characteristics may include alignment type, permissions,
 * etc.
 *
 * SHMEM8_4K is shared memory, byte addressable, and 4 KB aligned.
 */


	u16                  num_regions;
	/* Number of regions to map.*/

	u32                  property_flag;
/* Configures one common property for all the regions in the
 * payload. No two regions in the same memory map regions cmd can
 * have differnt property. Supported values: - 0x00000000 to
 * 0x00000001
 *
 * b0 - bit 0 indicates physical or virtual mapping 0 shared memory
 * address provided in avs_shared_map_regions_payload is physical
 * address. The shared memory needs to be mapped( hardware TLB
 * entry)
 *
 * and a software entry needs to be added for internal book keeping.
 *
 * 1 Shared memory address provided in MayPayload[usRegions] is
 * virtual address. The shared memory must not be mapped (since
 * hardware TLB entry is already available) but a software entry
 * needs to be added for internal book keeping. This can be useful
 * if two services with in ADSP is communicating via APR. They can
 * now directly communicate via the Virtual address instead of
 * Physical address. The virtual regions must be contiguous.
 *
 * b31-b1 - reserved bits. must be set to zero
 */

} __packed;

struct avs_shared_map_region_payload {
	u32                  shm_addr_lsw;
/* least significant word of shared memory address of the memory
 * region to map. It must be contiguous memory, and it must be 4 KB
 * aligned.
 */

	u32                  shm_addr_msw;
/* most significant word of shared memory address of the memory
 * region to map. For 32 bit shared memory address, this field must
 * tbe set to zero. For 36 bit shared memory address, bit31 to bit 4
 * must be set to zero
 */

	u32                  mem_size_bytes;
/* Number of bytes in the region.
 *
 * The aDSP will always map the regions as virtual contiguous
 * memory, but the memory size must be in multiples of 4 KB to avoid
 * gaps in the virtually contiguous mapped memory.
 */

} __packed;

struct avs_cmd_shared_mem_unmap_regions {
	struct apr_hdr       hdr;
	u32                  mem_map_handle;
/* memory map handle returned by ASM_CMD_SHARED_MEM_MAP_REGIONS
 * , ADM_CMD_SHARED_MEM_MAP_REGIONS, commands
 */

} __packed;

/* Memory map command response payload used by the
 * #ASM_CMDRSP_SHARED_MEM_MAP_REGIONS
 * ,#ADM_CMDRSP_SHARED_MEM_MAP_REGIONS
 */


struct avs_cmdrsp_shared_mem_map_regions {
	u32                  mem_map_handle;
/* A memory map handle encapsulating shared memory attributes is
 * returned
 */

} __packed;

/*adsp_audio_memmap_api.h*/

/* ASM related data structures */
struct asm_wma_cfg {
	u16 format_tag;
	u16 ch_cfg;
	u32 sample_rate;
	u32 avg_bytes_per_sec;
	u16 block_align;
	u16 valid_bits_per_sample;
	u32 ch_mask;
	u16 encode_opt;
	u16 adv_encode_opt;
	u32 adv_encode_opt2;
	u32 drc_peak_ref;
	u32 drc_peak_target;
	u32 drc_ave_ref;
	u32 drc_ave_target;
} __packed;

struct asm_wmapro_cfg {
	u16 format_tag;
	u16 ch_cfg;
	u32 sample_rate;
	u32 avg_bytes_per_sec;
	u16 block_align;
	u16 valid_bits_per_sample;
	u32 ch_mask;
	u16 encode_opt;
	u16 adv_encode_opt;
	u32 adv_encode_opt2;
	u32 drc_peak_ref;
	u32 drc_peak_target;
	u32 drc_ave_ref;
	u32 drc_ave_target;
} __packed;

struct asm_aac_cfg {
	u16 format;
	u16 aot;
	u16 ep_config;
	u16 section_data_resilience;
	u16 scalefactor_data_resilience;
	u16 spectral_data_resilience;
	u16 ch_cfg;
	u16 reserved;
	u32 sample_rate;
} __packed;

struct asm_amrwbplus_cfg {
	u32  size_bytes;
	u32  version;
	u32  num_channels;
	u32  amr_band_mode;
	u32  amr_dtx_mode;
	u32  amr_frame_fmt;
	u32  amr_lsf_idx;
} __packed;

struct asm_flac_cfg {
	u32 sample_rate;
	u32 ext_sample_rate;
	u32 min_frame_size;
	u32 max_frame_size;
	u16 stream_info_present;
	u16 min_blk_size;
	u16 max_blk_size;
	u16 ch_cfg;
	u16 sample_size;
	u16 md5_sum;
};

struct asm_softpause_params {
	u32 enable;
	u32 period;
	u32 step;
	u32 rampingcurve;
} __packed;

struct asm_softvolume_params {
	u32 period;
	u32 step;
	u32 rampingcurve;
} __packed;

#define ASM_END_POINT_DEVICE_MATRIX     0

#define PCM_CHANNEL_NULL 0

/* Front left channel. */
#define PCM_CHANNEL_FL    1

/* Front right channel. */
#define PCM_CHANNEL_FR    2

/* Front center channel. */
#define PCM_CHANNEL_FC    3

/* Left surround channel.*/
#define PCM_CHANNEL_LS   4

/* Right surround channel.*/
#define PCM_CHANNEL_RS   5

/* Low frequency effect channel. */
#define PCM_CHANNEL_LFE  6

/* Center surround channel; Rear center channel. */
#define PCM_CHANNEL_CS   7

/* Left back channel; Rear left channel. */
#define PCM_CHANNEL_LB   8

/* Right back channel; Rear right channel. */
#define PCM_CHANNEL_RB   9

/* Top surround channel. */
#define PCM_CHANNELS   10

/* Center vertical height channel.*/
#define PCM_CHANNEL_CVH  11

/* Mono surround channel.*/
#define PCM_CHANNEL_MS   12

/* Front left of center. */
#define PCM_CHANNEL_FLC  13

/* Front right of center. */
#define PCM_CHANNEL_FRC  14

/* Rear left of center. */
#define PCM_CHANNEL_RLC  15

/* Rear right of center. */
#define PCM_CHANNEL_RRC  16

#define PCM_FORMAT_MAX_NUM_CHANNEL  8

#define ASM_MEDIA_FMT_MULTI_CHANNEL_PCM_V2 0x00010DA5

#define ASM_MEDIA_FMT_EVRCB_FS 0x00010BEF

#define ASM_MEDIA_FMT_EVRCWB_FS 0x00010BF0

#define ASM_MAX_EQ_BANDS 12

#define ASM_DATA_CMD_MEDIA_FMT_UPDATE_V2 0x00010D98

struct asm_data_cmd_media_fmt_update_v2 {
u32                    fmt_blk_size;
	/* Media format block size in bytes.*/
}  __packed;

struct asm_multi_channel_pcm_fmt_blk_v2 {
	struct apr_hdr hdr;
	struct asm_data_cmd_media_fmt_update_v2 fmt_blk;

	u16  num_channels;
	/* Number of channels. Supported values: 1 to 8 */
	u16  bits_per_sample;
/* Number of bits per sample per channel. * Supported values:
 * 16, 24 * When used for playback, the client must send 24-bit
 * samples packed in 32-bit words. The 24-bit samples must be placed
 * in the most significant 24 bits of the 32-bit word. When used for
 * recording, the aDSP sends 24-bit samples packed in 32-bit words.
 * The 24-bit samples are placed in the most significant 24 bits of
 * the 32-bit word.
 */


	u32  sample_rate;
/* Number of samples per second (in Hertz).
 * Supported values: 2000 to 48000
 */

	u16  is_signed;
	/* Flag that indicates the samples are signed (1). */

	u16  reserved;
	/* reserved field for 32 bit alignment. must be set to zero. */

	u8   channel_mapping[8];
/* Channel array of size 8.
 * Supported values:
 * - #PCM_CHANNEL_L
 * - #PCM_CHANNEL_R
 * - #PCM_CHANNEL_C
 * - #PCM_CHANNEL_LS
 * - #PCM_CHANNEL_RS
 * - #PCM_CHANNEL_LFE
 * - #PCM_CHANNEL_CS
 * - #PCM_CHANNEL_LB
 * - #PCM_CHANNEL_RB
 * - #PCM_CHANNELS
 * - #PCM_CHANNEL_CVH
 * - #PCM_CHANNEL_MS
 * - #PCM_CHANNEL_FLC
 * - #PCM_CHANNEL_FRC
 * - #PCM_CHANNEL_RLC
 * - #PCM_CHANNEL_RRC
 *
 * Channel[i] mapping describes channel I. Each element i of the
 * array describes channel I inside the buffer where 0 @le I <
 * num_channels. An unused channel is set to zero.
 */
} __packed;

struct asm_stream_cmd_set_encdec_param {
	u32                  param_id;
	/* ID of the parameter. */

	u32                  param_size;
/* Data size of this parameter, in bytes. The size is a multiple
 * of 4 bytes.
 */

} __packed;

struct asm_enc_cfg_blk_param_v2 {
	u32                  frames_per_buf;
/* Number of encoded frames to pack into each buffer.
 *
 * @note1hang This is only guidance information for the aDSP. The
 * number of encoded frames put into each buffer (specified by the
 * client) is less than or equal to this number.
 */

	u32                  enc_cfg_blk_size;
/* Size in bytes of the encoder configuration block that follows
 * this member.
 */

} __packed;

/* @brief Dolby Digital Plus end point configuration structure
 */
struct asm_dec_ddp_endp_param_v2 {
	struct apr_hdr hdr;
	struct asm_stream_cmd_set_encdec_param  encdec;
	int endp_param_value;
} __packed;

/* @brief Multichannel PCM encoder configuration structure used
 * in the #ASM_STREAM_CMD_OPEN_READ_V2 command.
 */

struct asm_multi_channel_pcm_enc_cfg_v2 {
	struct apr_hdr hdr;
	struct asm_stream_cmd_set_encdec_param  encdec;
	struct asm_enc_cfg_blk_param_v2	encblk;
	uint16_t  num_channels;
/*< Number of PCM channels.
 *
 * Supported values: - 0 -- Native mode - 1 -- 8 Native mode
 * indicates that encoding must be performed with the number of
 * channels at the input.
 */

	uint16_t  bits_per_sample;
/*< Number of bits per sample per channel.
 * Supported values: 16, 24
 */

	uint32_t  sample_rate;
/*< Number of samples per second (in Hertz).
 *
 * Supported values: 0, 8000 to 48000 A value of 0 indicates the
 * native sampling rate. Encoding is performed at the input sampling
 * rate.
 */

	uint16_t  is_signed;
/*< Specifies whether the samples are signed (1). Currently,
 * only signed samples are supported.
 */

	uint16_t  reserved;
/*< reserved field for 32 bit alignment. must be set to zero.*/


	uint8_t   channel_mapping[8];
} __packed;

#define ASM_MEDIA_FMT_MP3 0x00010BE9
#define ASM_MEDIA_FMT_AAC_V2 0x00010DA6

/* @xreflabel
 * {hdr:AsmMediaFmtDolbyAac} Media format ID for the
 * Dolby AAC decoder. This format ID is be used if the client wants
 * to use the Dolby AAC decoder to decode MPEG2 and MPEG4 AAC
 * contents.
 */

#define ASM_MEDIA_FMT_DOLBY_AAC 0x00010D86

/* Enumeration for the audio data transport stream AAC format. */
#define ASM_MEDIA_FMT_AAC_FORMAT_FLAG_ADTS 0

/* Enumeration for low overhead audio stream AAC format. */
#define ASM_MEDIA_FMT_AAC_FORMAT_FLAG_LOAS                      1

/* Enumeration for the audio data interchange format
 * AAC format.
 */
#define ASM_MEDIA_FMT_AAC_FORMAT_FLAG_ADIF   2

/* Enumeration for the raw AAC format. */
#define ASM_MEDIA_FMT_AAC_FORMAT_FLAG_RAW    3

#define ASM_MEDIA_FMT_AAC_AOT_LC             2
#define ASM_MEDIA_FMT_AAC_AOT_SBR            5
#define ASM_MEDIA_FMT_AAC_AOT_PS             29
#define ASM_MEDIA_FMT_AAC_AOT_BSAC           22

struct asm_aac_fmt_blk_v2 {
	struct apr_hdr hdr;
	struct asm_data_cmd_media_fmt_update_v2 fmt_blk;

		u16          aac_fmt_flag;
/* Bitstream format option.
 * Supported values:
 * - #ASM_MEDIA_FMT_AAC_FORMAT_FLAG_ADTS
 * - #ASM_MEDIA_FMT_AAC_FORMAT_FLAG_LOAS
 * - #ASM_MEDIA_FMT_AAC_FORMAT_FLAG_ADIF
 * - #ASM_MEDIA_FMT_AAC_FORMAT_FLAG_RAW
 */

	u16          audio_objype;
/* Audio Object Type (AOT) present in the AAC stream.
 * Supported values:
 * - #ASM_MEDIA_FMT_AAC_AOT_LC
 * - #ASM_MEDIA_FMT_AAC_AOT_SBR
 * - #ASM_MEDIA_FMT_AAC_AOT_BSAC
 * - #ASM_MEDIA_FMT_AAC_AOT_PS
 * - Otherwise -- Not supported
 */

	u16          channel_config;
/* Number of channels present in the AAC stream.
 * Supported values:
 * - 1 -- Mono
 * - 2 -- Stereo
 * - 6 -- 5.1 content
 */

	u16          total_size_of_PCE_bits;
/* greater or equal to zero. * -In case of RAW formats and
 * channel config = 0 (PCE), client can send * the bit stream
 * containing PCE immediately following this structure * (in-band).
 * -This number does not include bits included for 32 bit alignment.
 * -If zero, then the PCE info is assumed to be available in the
 * audio -bit stream & not in-band.
 */

	u32          sample_rate;
/* Number of samples per second (in Hertz).
 *
 * Supported values: 8000, 11025, 12000, 16000, 22050, 24000, 32000,
 * 44100, 48000
 *
 * This field must be equal to the sample rate of the AAC-LC
 * decoder's output. - For MP4 or 3GP containers, this is indicated
 * by the samplingFrequencyIndex field in the AudioSpecificConfig
 * element. - For ADTS format, this is indicated by the
 * samplingFrequencyIndex in the ADTS fixed header. - For ADIF
 * format, this is indicated by the samplingFrequencyIndex in the
 * program_config_element present in the ADIF header.
 */

} __packed;

struct asm_aac_enc_cfg_v2 {
	struct apr_hdr hdr;
	struct asm_stream_cmd_set_encdec_param  encdec;
	struct asm_enc_cfg_blk_param_v2	encblk;

	u32          bit_rate;
	/* Encoding rate in bits per second. */
	u32          enc_mode;
/* Encoding mode.
 * Supported values:
 * - #ASM_MEDIA_FMT_AAC_AOT_LC
 * - #ASM_MEDIA_FMT_AAC_AOT_SBR
 * - #ASM_MEDIA_FMT_AAC_AOT_PS
 */
	u16          aac_fmt_flag;
/* AAC format flag.
 * Supported values:
 * - #ASM_MEDIA_FMT_AAC_FORMAT_FLAG_ADTS
 * - #ASM_MEDIA_FMT_AAC_FORMAT_FLAG_RAW
 */
	u16          channel_cfg;
/* Number of channels to encode.
 * Supported values:
 * - 0 -- Native mode
 * - 1 -- Mono
 * - 2 -- Stereo
 * - Other values are not supported.
 * @note1hang The eAAC+ encoder mode supports only stereo.
 * Native mode indicates that encoding must be performed with the
 * number of channels at the input.
 * The number of channels must not change during encoding.
 */

	u32          sample_rate;
/* Number of samples per second.
 * Supported values: - 0 -- Native mode - For other values,
 * Native mode indicates that encoding must be performed with the
 * sampling rate at the input.
 * The sampling rate must not change during encoding.
 */

} __packed;

struct asm_flac_fmt_blk_v2 {
	struct apr_hdr hdr;
	struct asm_data_cmd_media_fmt_update_v2 fmtblk;

	u16 is_stream_info_present;
/* Specifies whether stream information is present in the FLAC format
 * block.
 *
 * Supported values:
 * - 0 -- Stream information is not present in this message
 * - 1 -- Stream information is present in this message
 *
 * When set to 1, the FLAC bitstream was successfully parsed by the
 * client, and other fields in the FLAC format block can be read by the
 * decoder to get metadata stream information.
 */

	u16 num_channels;
/* Number of channels for decoding.
 * Supported values: 1 to 2
 */

	u16 min_blk_size;
/* Minimum block size (in samples) used in the stream. It must be less
 * than or equal to max_blk_size.
 */

	u16 max_blk_size;
/* Maximum block size (in samples) used in the stream. If the
 * minimum block size equals the maximum block size, a fixed block
 * size stream is implied.
 */

	u16 md5_sum[8];
/* MD5 signature array of the unencoded audio data. This allows the
 * decoder to determine if an error exists in the audio data, even when
 * the error does not result in an invalid bitstream.
 */

	u32 sample_rate;
/* Number of samples per second.
 * Supported values: 8000 to 48000 Hz
 */

	u32 min_frame_size;
/* Minimum frame size used in the stream.
 * Supported values:
 * - > 0 bytes
 * - 0 -- The value is unknown
 */

	u32 max_frame_size;
/* Maximum frame size used in the stream.
 * Supported values:
 * -- > 0 bytes
 * -- 0 . The value is unknown
 */

	u16 sample_size;
/* Bits per sample.Supported values: 8, 16 */

	u16 reserved;
/* Clients must set this field to zero
 */

} __packed;

#define ASM_MEDIA_FMT_AMRNB_FS                  0x00010BEB

/* Enumeration for 4.75 kbps AMR-NB Encoding mode. */
#define ASM_MEDIA_FMT_AMRNB_FS_ENCODE_MODE_MR475                0

/* Enumeration for 5.15 kbps AMR-NB Encoding mode. */
#define ASM_MEDIA_FMT_AMRNB_FS_ENCODE_MODE_MR515                1

/* Enumeration for 5.90 kbps AMR-NB Encoding mode. */
#define ASM_MEDIA_FMT_AMRNB_FS_ENCODE_MODE_MMR59                2

/* Enumeration for 6.70 kbps AMR-NB Encoding mode. */
#define ASM_MEDIA_FMT_AMRNB_FS_ENCODE_MODE_MMR67                3

/* Enumeration for 7.40 kbps AMR-NB Encoding mode. */
#define ASM_MEDIA_FMT_AMRNB_FS_ENCODE_MODE_MMR74                4

/* Enumeration for 7.95 kbps AMR-NB Encoding mode. */
#define ASM_MEDIA_FMT_AMRNB_FS_ENCODE_MODE_MMR795               5

/* Enumeration for 10.20 kbps AMR-NB Encoding mode. */
#define ASM_MEDIA_FMT_AMRNB_FS_ENCODE_MODE_MMR102               6

/* Enumeration for 12.20 kbps AMR-NB Encoding mode. */
#define ASM_MEDIA_FMT_AMRNB_FS_ENCODE_MODE_MMR122               7

/* Enumeration for AMR-NB Discontinuous Transmission mode off. */
#define ASM_MEDIA_FMT_AMRNB_FS_DTX_MODE_OFF                     0

/* Enumeration for AMR-NB DTX mode VAD1. */
#define ASM_MEDIA_FMT_AMRNB_FS_DTX_MODE_VAD1                    1

/* Enumeration for AMR-NB DTX mode VAD2. */
#define ASM_MEDIA_FMT_AMRNB_FS_DTX_MODE_VAD2                    2

/* Enumeration for AMR-NB DTX mode auto.
	*/
#define ASM_MEDIA_FMT_AMRNB_FS_DTX_MODE_AUTO                    3

struct asm_amrnb_enc_cfg {
	struct apr_hdr hdr;
	struct asm_stream_cmd_set_encdec_param  encdec;
	struct asm_enc_cfg_blk_param_v2	encblk;

	u16          enc_mode;
/* AMR-NB encoding rate.
 * Supported values:
 * Use the ASM_MEDIA_FMT_AMRNB_FS_ENCODE_MODE_*
 * macros
 */

	u16          dtx_mode;
/* Specifies whether DTX mode is disabled or enabled.
 * Supported values:
 * - #ASM_MEDIA_FMT_AMRNB_FS_DTX_MODE_OFF
 * - #ASM_MEDIA_FMT_AMRNB_FS_DTX_MODE_VAD1
 */
} __packed;

#define ASM_MEDIA_FMT_AMRWB_FS                  0x00010BEC

/* Enumeration for 6.6 kbps AMR-WB Encoding mode. */
#define ASM_MEDIA_FMT_AMRWB_FS_ENCODE_MODE_MR66                 0

/* Enumeration for 8.85 kbps AMR-WB Encoding mode. */
#define ASM_MEDIA_FMT_AMRWB_FS_ENCODE_MODE_MR885                1

/* Enumeration for 12.65 kbps AMR-WB Encoding mode. */
#define ASM_MEDIA_FMT_AMRWB_FS_ENCODE_MODE_MR1265               2

/* Enumeration for 14.25 kbps AMR-WB Encoding mode. */
#define ASM_MEDIA_FMT_AMRWB_FS_ENCODE_MODE_MR1425               3

/* Enumeration for 15.85 kbps AMR-WB Encoding mode. */
#define ASM_MEDIA_FMT_AMRWB_FS_ENCODE_MODE_MR1585               4

/* Enumeration for 18.25 kbps AMR-WB Encoding mode. */
#define ASM_MEDIA_FMT_AMRWB_FS_ENCODE_MODE_MR1825               5

/* Enumeration for 19.85 kbps AMR-WB Encoding mode. */
#define ASM_MEDIA_FMT_AMRWB_FS_ENCODE_MODE_MR1985               6

/* Enumeration for 23.05 kbps AMR-WB Encoding mode. */
#define ASM_MEDIA_FMT_AMRWB_FS_ENCODE_MODE_MR2305               7

/* Enumeration for 23.85 kbps AMR-WB Encoding mode.
	*/
#define ASM_MEDIA_FMT_AMRWB_FS_ENCODE_MODE_MR2385               8

struct asm_amrwb_enc_cfg {
	struct apr_hdr hdr;
	struct asm_stream_cmd_set_encdec_param  encdec;
	struct asm_enc_cfg_blk_param_v2	encblk;

	u16          enc_mode;
/* AMR-WB encoding rate.
 * Suupported values:
 * Use the ASM_MEDIA_FMT_AMRWB_FS_ENCODE_MODE_*
 * macros
 */

	u16          dtx_mode;
/* Specifies whether DTX mode is disabled or enabled.
 * Supported values:
 * - #ASM_MEDIA_FMT_AMRNB_FS_DTX_MODE_OFF
 * - #ASM_MEDIA_FMT_AMRNB_FS_DTX_MODE_VAD1
 */
} __packed;

#define ASM_MEDIA_FMT_V13K_FS                      0x00010BED

/* Enumeration for 14.4 kbps V13K Encoding mode. */
#define ASM_MEDIA_FMT_V13K_FS_ENCODE_MODE_MR1440                0

/* Enumeration for 12.2 kbps V13K Encoding mode. */
#define ASM_MEDIA_FMT_V13K_FS_ENCODE_MODE_MR1220                1

/* Enumeration for 11.2 kbps V13K Encoding mode. */
#define ASM_MEDIA_FMT_V13K_FS_ENCODE_MODE_MR1120                2

/* Enumeration for 9.0 kbps V13K Encoding mode. */
#define ASM_MEDIA_FMT_V13K_FS_ENCODE_MODE_MR90                  3

/* Enumeration for 7.2 kbps V13K eEncoding mode. */
#define ASM_MEDIA_FMT_V13K_FS_ENCODE_MODE_MR720                 4

/* Enumeration for 1/8 vocoder rate.*/
#define ASM_MEDIA_FMT_VOC_ONE_EIGHTH_RATE          1

/* Enumeration for 1/4 vocoder rate. */
#define ASM_MEDIA_FMT_VOC_ONE_FOURTH_RATE       2

/* Enumeration for 1/2 vocoder rate. */
#define ASM_MEDIA_FMT_VOC_HALF_RATE             3

/* Enumeration for full vocoder rate.
	*/
#define ASM_MEDIA_FMT_VOC_FULL_RATE             4

struct asm_v13k_enc_cfg {
	struct apr_hdr hdr;
	struct asm_stream_cmd_set_encdec_param  encdec;
	struct asm_enc_cfg_blk_param_v2	encblk;
		u16          max_rate;
/* Maximum allowed encoder frame rate.
 * Supported values:
 * - #ASM_MEDIA_FMT_VOC_ONE_EIGHTH_RATE
 * - #ASM_MEDIA_FMT_VOC_ONE_FOURTH_RATE
 * - #ASM_MEDIA_FMT_VOC_HALF_RATE
 * - #ASM_MEDIA_FMT_VOC_FULL_RATE
 */

	u16          min_rate;
/* Minimum allowed encoder frame rate.
 * Supported values:
 * - #ASM_MEDIA_FMT_VOC_ONE_EIGHTH_RATE
 * - #ASM_MEDIA_FMT_VOC_ONE_FOURTH_RATE
 * - #ASM_MEDIA_FMT_VOC_HALF_RATE
 * - #ASM_MEDIA_FMT_VOC_FULL_RATE
 */

	u16          reduced_rate_cmd;
/* Reduced rate command, used to change
 * the average bitrate of the V13K
 * vocoder.
 * Supported values:
 * - #ASM_MEDIA_FMT_V13K_FS_ENCODE_MODE_MR1440 (Default)
 * - #ASM_MEDIA_FMT_V13K_FS_ENCODE_MODE_MR1220
 * - #ASM_MEDIA_FMT_V13K_FS_ENCODE_MODE_MR1120
 * - #ASM_MEDIA_FMT_V13K_FS_ENCODE_MODE_MR90
 * - #ASM_MEDIA_FMT_V13K_FS_ENCODE_MODE_MR720
 */

	u16          rate_mod_cmd;
/* Rate modulation command. Default = 0.
 *- If bit 0=1, rate control is enabled.
 *- If bit 1=1, the maximum number of consecutive full rate
 *			frames is limited with numbers supplied in
 *			bits 2 to 10.
 *- If bit 1=0, the minimum number of non-full rate frames
 *			in between two full rate frames is forced to
 * the number supplied in bits 2 to 10. In both cases, if necessary,
 * half rate is used to substitute full rate. - Bits 15 to 10 are
 * reserved and must all be set to zero.
 */

} __packed;

#define ASM_MEDIA_FMT_EVRC_FS                   0x00010BEE

/*  EVRC encoder configuration structure used in the
 * #ASM_STREAM_CMD_OPEN_READ_V2 command.
 */
struct asm_evrc_enc_cfg {
	struct apr_hdr hdr;
	struct asm_stream_cmd_set_encdec_param  encdec;
	struct asm_enc_cfg_blk_param_v2	encblk;
	u16          max_rate;
/* Maximum allowed encoder frame rate.
 * Supported values:
 * - #ASM_MEDIA_FMT_VOC_ONE_EIGHTH_RATE
 * - #ASM_MEDIA_FMT_VOC_ONE_FOURTH_RATE
 * - #ASM_MEDIA_FMT_VOC_HALF_RATE
 * - #ASM_MEDIA_FMT_VOC_FULL_RATE
 */

	u16          min_rate;
/* Minimum allowed encoder frame rate.
 * Supported values:
 * - #ASM_MEDIA_FMT_VOC_ONE_EIGHTH_RATE
 * - #ASM_MEDIA_FMT_VOC_ONE_FOURTH_RATE
 * - #ASM_MEDIA_FMT_VOC_HALF_RATE
 * - #ASM_MEDIA_FMT_VOC_FULL_RATE
 */

	u16          rate_mod_cmd;
/* Rate modulation command. Default: 0.
 * - If bit 0=1, rate control is enabled.
 * - If bit 1=1, the maximum number of consecutive full rate frames
 * is limited with numbers supplied in bits 2 to 10.
 *
 * - If bit 1=0, the minimum number of non-full rate frames in
 * between two full rate frames is forced to the number supplied in
 * bits 2 to 10. In both cases, if necessary, half rate is used to
 * substitute full rate.
 *
 * - Bits 15 to 10 are reserved and must all be set to zero.
 */

	u16          reserved;
	/* Reserved. Clients must set this field to zero. */
} __packed;

#define ASM_MEDIA_FMT_WMA_V10PRO_V2                0x00010DA7

struct asm_wmaprov10_fmt_blk_v2 {
	struct apr_hdr hdr;
	struct asm_data_cmd_media_fmt_update_v2 fmtblk;

	u16          fmtag;
/* WMA format type.
 * Supported values:
 * - 0x162 -- WMA 9 Pro
 * - 0x163 -- WMA 9 Pro Lossless
 * - 0x166 -- WMA 10 Pro
 * - 0x167 -- WMA 10 Pro Lossless
 */

	u16          num_channels;
/* Number of channels encoded in the input stream.
 * Supported values: 1 to 8
 */

	u32          sample_rate;
/* Number of samples per second (in Hertz).
 * Supported values: 11025, 16000, 22050, 32000, 44100, 48000,
 * 88200, 96000
 */

	u32          avg_bytes_per_sec;
/* Bitrate expressed as the average bytes per second.
 * Supported values: 2000 to 96000
 */

	u16          blk_align;
/* Size of the bitstream packet size in bytes. WMA Pro files
 * have a payload of one block per bitstream packet.
 * Supported values: @le 13376
 */

	u16          bits_per_sample;
/* Number of bits per sample in the encoded WMA stream.
 * Supported values: 16, 24
 */

	u32          channel_mask;
/* Bit-packed double word (32-bits) that indicates the
 * recommended speaker positions for each source channel.
 */

	u16          enc_options;
/* Bit-packed word with values that indicate whether certain
 * features of the bitstream are used.
 * Supported values: - 0x0001 -- ENCOPT3_PURE_LOSSLESS - 0x0006 --
 * ENCOPT3_FRM_SIZE_MOD - 0x0038 -- ENCOPT3_SUBFRM_DIV - 0x0040 --
 * ENCOPT3_WRITE_FRAMESIZE_IN_HDR - 0x0080 --
 * ENCOPT3_GENERATE_DRC_PARAMS - 0x0100 -- ENCOPT3_RTMBITS
 */


	u16          usAdvancedEncodeOpt;
	/* Advanced encoding option.  */

	u32          advanced_enc_options2;
	/* Advanced encoding option 2. */

} __packed;

#define ASM_MEDIA_FMT_WMA_V9_V2                    0x00010DA8
struct asm_wmastdv9_fmt_blk_v2 {
	struct apr_hdr hdr;
	struct asm_data_cmd_media_fmt_update_v2 fmtblk;
	u16          fmtag;
/* WMA format tag.
 * Supported values: 0x161 (WMA 9 standard)
 */

	u16          num_channels;
/* Number of channels in the stream.
 * Supported values: 1, 2
 */

	u32          sample_rate;
/* Number of samples per second (in Hertz).
 * Supported values: 48000
 */

	u32          avg_bytes_per_sec;
	/* Bitrate expressed as the average bytes per second. */

	u16          blk_align;
/* Block align. All WMA files with a maximum packet size of
 * 13376 are supported.
 */


	u16          bits_per_sample;
/* Number of bits per sample in the output.
 * Supported values: 16
 */

	u32          channel_mask;
/* Channel mask.
 * Supported values:
 * - 3 -- Stereo (front left/front right)
 * - 4 -- Mono (center)
 */

	u16          enc_options;
	/* Options used during encoding. */

	u16          reserved;

} __packed;

#define ASM_MEDIA_FMT_WMA_V8                    0x00010D91

struct asm_wmastdv8_enc_cfg {
	struct apr_hdr hdr;
	struct asm_stream_cmd_set_encdec_param  encdec;
	struct asm_enc_cfg_blk_param_v2	encblk;
	u32          bit_rate;
	/* Encoding rate in bits per second. */

	u32          sample_rate;
/* Number of samples per second.
 *
 * Supported values:
 * - 0 -- Native mode
 * - Other Supported values are 22050, 32000, 44100, and 48000.
 *
 * Native mode indicates that encoding must be performed with the
 * sampling rate at the input.
 * The sampling rate must not change during encoding.
 */

	u16          channel_cfg;
/* Number of channels to encode.
 * Supported values:
 * - 0 -- Native mode
 * - 1 -- Mono
 * - 2 -- Stereo
 * - Other values are not supported.
 *
 * Native mode indicates that encoding must be performed with the
 * number of channels at the input.
 * The number of channels must not change during encoding.
 */

	u16          reserved;
	/* Reserved. Clients must set this field to zero.*/
	} __packed;

#define ASM_MEDIA_FMT_AMR_WB_PLUS_V2               0x00010DA9

struct asm_amrwbplus_fmt_blk_v2 {
	struct apr_hdr hdr;
	struct asm_data_cmd_media_fmt_update_v2 fmtblk;
	u32          amr_frame_fmt;
/* AMR frame format.
 * Supported values:
 * - 6 -- Transport Interface Format (TIF)
 * - Any other value -- File storage format (FSF)
 *
 * TIF stream contains 2-byte header for each frame within the
 * superframe. FSF stream contains one 2-byte header per superframe.
 */

} __packed;

#define ASM_MEDIA_FMT_AC3_DEC                   0x00010BF6
#define ASM_MEDIA_FMT_EAC3_DEC                   0x00010C3C
#define ASM_MEDIA_FMT_DTS                    0x00010D88
#define ASM_MEDIA_FMT_MP2                    0x00010DE9
#define ASM_MEDIA_FMT_FLAC                   0x00010C16


/* Media format ID for adaptive transform acoustic coding. This
 * ID is used by the #ASM_STREAM_CMD_OPEN_WRITE_COMPRESSED command
 * only.
 */

#define ASM_MEDIA_FMT_ATRAC                  0x00010D89

/* Media format ID for metadata-enhanced audio transmission.
 * This ID is used by the #ASM_STREAM_CMD_OPEN_WRITE_COMPRESSED
 * command only.
 */

#define ASM_MEDIA_FMT_MAT                    0x00010D8A

/*  adsp_media_fmt.h */

#define ASM_DATA_CMD_WRITE_V2 0x00010DAB

struct asm_data_cmd_write_v2 {
	struct apr_hdr hdr;
	u32                  buf_addr_lsw;
/* The 64 bit address msw-lsw should be a valid, mapped address.
 * 64 bit address should be a multiple of 32 bytes
 */

	u32                  buf_addr_msw;
/* The 64 bit address msw-lsw should be a valid, mapped address.
 * 64 bit address should be a multiple of 32 bytes.
 * -Address of the buffer containing the data to be decoded.
 * The buffer should be aligned to a 32 byte boundary.
 * -In the case of 32 bit Shared memory address, msw field must
 * -be set to zero.
 * -In the case of 36 bit shared memory address, bit 31 to bit 4
 * -of msw must be set to zero.
 */
	u32                  mem_map_handle;
/* memory map handle returned by DSP through
 * ASM_CMD_SHARED_MEM_MAP_REGIONS command
 */
	u32                  buf_size;
/* Number of valid bytes available in the buffer for decoding. The
 * first byte starts at buf_addr.
 */

	u32                  seq_id;
	/* Optional buffer sequence ID. */

	u32                  timestamp_lsw;
/* Lower 32 bits of the 64-bit session time in microseconds of the
 * first buffer sample.
 */

	u32                  timestamp_msw;
/* Upper 32 bits of the 64-bit session time in microseconds of the
 * first buffer sample.
 */

	u32                  flags;
/* Bitfield of flags.
 * Supported values for bit 31:
 * - 1 -- Valid timestamp.
 * - 0 -- Invalid timestamp.
 * - Use #ASM_BIT_MASKIMESTAMP_VALID_FLAG as the bitmask and
 * #ASM_SHIFTIMESTAMP_VALID_FLAG as the shift value to set this bit.
 * Supported values for bit 30:
 * - 1 -- Last buffer.
 * - 0 -- Not the last buffer.
 *
 * Supported values for bit 29:
 * - 1 -- Continue the timestamp from the previous buffer.
 * - 0 -- Timestamp of the current buffer is not related
 * to the timestamp of the previous buffer.
 * - Use #ASM_BIT_MASKS_CONTINUE_FLAG and #ASM_SHIFTS_CONTINUE_FLAG
 * to set this bit.
 *
 * Supported values for bit 4:
 * - 1 -- End of the frame.
 * - 0 -- Not the end of frame, or this information is not known.
 * - Use #ASM_BIT_MASK_EOF_FLAG as the bitmask and #ASM_SHIFT_EOF_FLAG
 * as the shift value to set this bit.
 *
 * All other bits are reserved and must be set to 0.
 *
 * If bit 31=0 and bit 29=1: The timestamp of the first sample in
 * this buffer continues from the timestamp of the last sample in
 * the previous buffer. If there is no previous buffer (i.e., this
 * is the first buffer sent after opening the stream or after a
 * flush operation), or if the previous buffer does not have a valid
 * timestamp, the samples in the current buffer also do not have a
 * valid timestamp. They are played out as soon as possible.
 *
 *
 * If bit 31=0 and bit 29=0: No timestamp is associated with the
 * first sample in this buffer. The samples are played out as soon
 * as possible.
 *
 *
 * If bit 31=1 and bit 29 is ignored: The timestamp specified in
 * this payload is honored.
 *
 *
 * If bit 30=0: Not the last buffer in the stream. This is useful
 * in removing trailing samples.
 *
 *
 * For bit 4: The client can set this flag for every buffer sent in
 * which the last byte is the end of a frame. If this flag is set,
 * the buffer can contain data from multiple frames, but it should
 * always end at a frame boundary. Restrictions allow the aDSP to
 * detect an end of frame without requiring additional processing.
 */

} __packed;

#define ASM_DATA_CMD_READ_V2 0x00010DAC

struct asm_data_cmd_read_v2 {
	struct apr_hdr       hdr;
	u32                  buf_addr_lsw;
/* the 64 bit address msw-lsw should be a valid mapped address
 * and should be a multiple of 32 bytes
 */


	u32                  buf_addr_msw;
/* the 64 bit address msw-lsw should be a valid mapped address
 * and should be a multiple of 32 bytes.
* - Address of the buffer where the DSP puts the encoded data,
* potentially, at an offset specified by the uOffset field in
* ASM_DATA_EVENT_READ_DONE structure. The buffer should be aligned
* to a 32 byte boundary.
*- In the case of 32 bit Shared memory address, msw field must
*- be set to zero.
*- In the case of 36 bit shared memory address, bit 31 to bit
*- 4 of msw must be set to zero.
*/
	u32                  mem_map_handle;
/* memory map handle returned by DSP through
 * ASM_CMD_SHARED_MEM_MAP_REGIONS command.
 */

	u32                  buf_size;
/* Number of bytes available for the aDSP to write. The aDSP
 * starts writing from buf_addr.
 */

	u32                  seq_id;
	/* Optional buffer sequence ID.
			*/
} __packed;

#define ASM_DATA_CMD_EOS               0x00010BDB
#define ASM_DATA_EVENT_RENDERED_EOS    0x00010C1C
#define ASM_DATA_EVENT_EOS             0x00010BDD

#define ASM_DATA_EVENT_WRITE_DONE_V2 0x00010D99
struct asm_data_event_write_done_v2 {
	u32                  buf_addr_lsw;
	/* lsw of the 64 bit address */
	u32                  buf_addr_msw;
	/* msw of the 64 bit address. address given by the client in
	* ASM_DATA_CMD_WRITE_V2 command.
	*/
	u32                  mem_map_handle;
	/* memory map handle in the ASM_DATA_CMD_WRITE_V2  */

	u32                  status;
/* Status message (error code) that indicates whether the
 * referenced buffer has been successfully consumed.
 * Supported values: Refer to @xhyperref{Q3,[Q3]}
 */
} __packed;

#define ASM_DATA_EVENT_READ_DONE_V2 0x00010D9A

/* Definition of the frame metadata flag bitmask.*/
#define ASM_BIT_MASK_FRAME_METADATA_FLAG (0x40000000UL)

/* Definition of the frame metadata flag shift value. */
#define ASM_SHIFT_FRAME_METADATA_FLAG 30

struct asm_data_event_read_done_v2 {
	u32                  status;
/* Status message (error code).
 * Supported values: Refer to @xhyperref{Q3,[Q3]}
 */

u32                  buf_addr_lsw;
/* 64 bit address msw-lsw is a valid, mapped address. 64 bit
 * address is a multiple of 32 bytes.
 */

u32                  buf_addr_msw;
/* 64 bit address msw-lsw is a valid, mapped address. 64 bit
* address is a multiple of 32 bytes.
*
* -Same address provided by the client in ASM_DATA_CMD_READ_V2
* -In the case of 32 bit Shared memory address, msw field is set to
* zero.
* -In the case of 36 bit shared memory address, bit 31 to bit 4
* -of msw is set to zero.
*/

u32                  mem_map_handle;
/* memory map handle in the ASM_DATA_CMD_READ_V2  */

u32                  enc_framesotal_size;
/* Total size of the encoded frames in bytes.
 * Supported values: >0
 */

u32                  offset;
/* Offset (from buf_addr) to the first byte of the first encoded
 * frame. All encoded frames are consecutive, starting from this
 * offset.
 * Supported values: > 0
 */

u32                  timestamp_lsw;
/* Lower 32 bits of the 64-bit session time in microseconds of
 * the first sample in the buffer. If Bit 5 of mode_flags flag of
 * ASM_STREAM_CMD_OPEN_READ_V2 is 1 then the 64 bit timestamp is
 * absolute capture time otherwise it is relative session time. The
 * absolute timestamp doesnt reset unless the system is reset.
 */


u32                  timestamp_msw;
/* Upper 32 bits of the 64-bit session time in microseconds of
 * the first sample in the buffer.
 */


u32                  flags;
/* Bitfield of flags. Bit 30 indicates whether frame metadata is
 * present. If frame metadata is present, num_frames consecutive
 * instances of @xhyperref{hdr:FrameMetaData,Frame metadata} start
 * at the buffer address.
 * Supported values for bit 31:
 * - 1 -- Timestamp is valid.
 * - 0 -- Timestamp is invalid.
 * - Use #ASM_BIT_MASKIMESTAMP_VALID_FLAG and
 * #ASM_SHIFTIMESTAMP_VALID_FLAG to set this bit.
 *
 * Supported values for bit 30:
 * - 1 -- Frame metadata is present.
 * - 0 -- Frame metadata is absent.
 * - Use #ASM_BIT_MASK_FRAME_METADATA_FLAG and
 * #ASM_SHIFT_FRAME_METADATA_FLAG to set this bit.
 *
 * All other bits are reserved; the aDSP sets them to 0.
 */

u32                  num_frames;
/* Number of encoded frames in the buffer. */

u32                  seq_id;
/* Optional buffer sequence ID.	*/
} __packed;

struct asm_data_read_buf_metadata_v2 {
	u32          offset;
/* Offset from buf_addr in #ASM_DATA_EVENT_READ_DONE_PAYLOAD to
 * the frame associated with this metadata.
 * Supported values: > 0
 */

u32          frm_size;
/* Size of the encoded frame in bytes.
 * Supported values: > 0
 */

u32          num_encoded_pcm_samples;
/* Number of encoded PCM samples (per channel) in the frame
 * associated with this metadata.
 * Supported values: > 0
 */

u32          timestamp_lsw;
/* Lower 32 bits of the 64-bit session time in microseconds of the
 * first sample for this frame.
 * If Bit 5 of mode_flags flag of ASM_STREAM_CMD_OPEN_READ_V2 is 1
 * then the 64 bit timestamp is absolute capture time otherwise it
 * is relative session time. The absolute timestamp doesnt reset
 * unless the system is reset.
 */


u32          timestamp_msw;
/* Lower 32 bits of the 64-bit session time in microseconds of the
 * first sample for this frame.
 */

u32          flags;
/* Frame flags.
 * Supported values for bit 31:
 * - 1 -- Time stamp is valid
 * - 0 -- Time stamp is not valid
 * - All other bits are reserved; the aDSP sets them to 0.
*/
} __packed;

/* Notifies the client of a change in the data sampling rate or
 * Channel mode. This event is raised by the decoder service. The
 * event is enabled through the mode flags of
 * #ASM_STREAM_CMD_OPEN_WRITE_V2 or
 * #ASM_STREAM_CMD_OPEN_READWRITE_V2. - The decoder detects a change
 * in the output sampling frequency or the number/positioning of
 * output channels, or if it is the first frame decoded.The new
 * sampling frequency or the new channel configuration is
 * communicated back to the client asynchronously.
 */

#define ASM_DATA_EVENT_SR_CM_CHANGE_NOTIFY 0x00010C65

/*  Payload of the #ASM_DATA_EVENT_SR_CM_CHANGE_NOTIFY event.
 * This event is raised when the following conditions are both true:
 * - The event is enabled through the mode_flags of
 * #ASM_STREAM_CMD_OPEN_WRITE_V2 or
 * #ASM_STREAM_CMD_OPEN_READWRITE_V2. - The decoder detects a change
 * in either the output sampling frequency or the number/positioning
 * of output channels, or if it is the first frame decoded.
 * This event is not raised (even if enabled) if the decoder is
 * MIDI, because
 */


struct asm_data_event_sr_cm_change_notify {
	u32                  sample_rate;
/* New sampling rate (in Hertz) after detecting a change in the
 * bitstream.
 * Supported values: 2000 to 48000
 */

	u16                  num_channels;
/* New number of channels after detecting a change in the
 * bitstream.
 * Supported values: 1 to 8
 */


	u16                  reserved;
	/* Reserved for future use. This field must be set to 0.*/

	u8                   channel_mapping[8];

} __packed;

/* Notifies the client of a data sampling rate or channel mode
 * change. This event is raised by the encoder service.
 * This event is raised when :
 * - Native mode encoding was requested in the encoder
 * configuration (i.e., the channel number was 0), the sample rate
 * was 0, or both were 0.
 *
 * - The input data frame at the encoder is the first one, or the
 * sampling rate/channel mode is different from the previous input
 * data frame.
 *
 */
#define ASM_DATA_EVENT_ENC_SR_CM_CHANGE_NOTIFY 0x00010BDE

struct asm_data_event_enc_sr_cm_change_notify {
	u32                  sample_rate;
/* New sampling rate (in Hertz) after detecting a change in the
 * input data.
 * Supported values: 2000 to 48000
 */


	u16                  num_channels;
/* New number of channels after detecting a change in the input
 * data. Supported values: 1 to 8
 */


	u16                  bits_per_sample;
/* New bits per sample after detecting a change in the input
 * data.
 * Supported values: 16, 24
 */


	u8                   channel_mapping[8];

} __packed;
#define ASM_DATA_CMD_IEC_60958_FRAME_RATE 0x00010D87


/* Payload of the #ASM_DATA_CMD_IEC_60958_FRAME_RATE command,
 * which is used to indicate the IEC 60958 frame rate of a given
 * packetized audio stream.
 */

struct asm_data_cmd_iec_60958_frame_rate {
	u32                  frame_rate;
/* IEC 60958 frame rate of the incoming IEC 61937 packetized stream.
 * Supported values: Any valid frame rate
 */
} __packed;

/* adsp_asm_data_commands.h*/
#define ASM_SVC_CMD_GET_STREAM_HANDLES         0x00010C0B

#define ASM_SVC_CMDRSP_GET_STREAM_HANDLES      0x00010C1B

/* Definition of the stream ID bitmask.*/
#define ASM_BIT_MASK_STREAM_ID                 (0x000000FFUL)

/* Definition of the stream ID shift value.*/
#define ASM_SHIFT_STREAM_ID                    0

/* Definition of the session ID bitmask.*/
#define ASM_BIT_MASK_SESSION_ID                (0x0000FF00UL)

/* Definition of the session ID shift value.*/
#define ASM_SHIFT_SESSION_ID                   8

/* Definition of the service ID bitmask.*/
#define ASM_BIT_MASK_SERVICE_ID                (0x00FF0000UL)

/* Definition of the service ID shift value.*/
#define ASM_SHIFT_SERVICE_ID                   16

/* Definition of the domain ID bitmask.*/
#define ASM_BIT_MASK_DOMAIN_ID                (0xFF000000UL)

/* Definition of the domain ID shift value.*/
#define ASM_SHIFT_DOMAIN_ID                    24

/* Payload of the #ASM_SVC_CMDRSP_GET_STREAM_HANDLES message,
 * which returns a list of currently active stream handles.
 * Immediately following this structure are num_handles of uint32
 * stream handles.
 */


struct asm_svc_cmdrsp_get_stream_handles {
	u32                  num_handles;
	/* Number of active stream handles.	*/
} __packed;

#define ASM_CMD_SHARED_MEM_MAP_REGIONS               0x00010D92
#define ASM_CMDRSP_SHARED_MEM_MAP_REGIONS     0x00010D93
#define ASM_CMD_SHARED_MEM_UNMAP_REGIONS              0x00010D94

/* adsp_asm_service_commands.h */

#define ASM_MAX_SESSION_ID  (8)

/* Maximum number of sessions.*/
#define ASM_MAX_NUM_SESSIONS                ASM_MAX_SESSION_ID

/* Maximum number of streams per session.*/
#define ASM_MAX_STREAMS_PER_SESSION (8)
#define ASM_SESSION_CMD_RUN_V2                   0x00010DAA
#define ASM_SESSION_CMD_RUN_STARTIME_RUN_IMMEDIATE  0
#define ASM_SESSION_CMD_RUN_STARTIME_RUN_AT_ABSOLUTEIME 1
#define ASM_SESSION_CMD_RUN_STARTIME_RUN_AT_RELATIVEIME 2
#define ASM_SESSION_CMD_RUN_STARTIME_RUN_WITH_DELAY     3

#define ASM_BIT_MASK_RUN_STARTIME                 (0x00000003UL)

/* Bit shift value used to specify the start time for the
 * ASM_SESSION_CMD_RUN_V2 command.
 */
#define ASM_SHIFT_RUN_STARTIME 0
struct asm_session_cmd_run_v2 {
	struct apr_hdr hdr;
	u32                  flags;
/* Specifies whether to run immediately or at a specific
 * rendering time or with a specified delay. Run with delay is
 * useful for delaying in case of ASM loopback opened through
 * ASM_STREAM_CMD_OPEN_LOOPBACK_V2. Use #ASM_BIT_MASK_RUN_STARTIME
 * and #ASM_SHIFT_RUN_STARTIME to set this 2-bit flag.
 *
 *
 *Bits 0 and 1 can take one of four possible values:
 *
 *- #ASM_SESSION_CMD_RUN_STARTIME_RUN_IMMEDIATE
 *- #ASM_SESSION_CMD_RUN_STARTIME_RUN_AT_ABSOLUTEIME
 *- #ASM_SESSION_CMD_RUN_STARTIME_RUN_AT_RELATIVEIME
 *- #ASM_SESSION_CMD_RUN_STARTIME_RUN_WITH_DELAY
 *
 *All other bits are reserved; clients must set them to zero.
 */

	u32                  time_lsw;
/* Lower 32 bits of the time in microseconds used to align the
 * session origin time. When bits 0-1 of flags is
 * ASM_SESSION_CMD_RUN_START_RUN_WITH_DELAY, time lsw is the lsw of
 * the delay in us. For ASM_SESSION_CMD_RUN_START_RUN_WITH_DELAY,
 * maximum value of the 64 bit delay is 150 ms.
 */

	u32                  time_msw;
/* Upper 32 bits of the time in microseconds used to align the
 * session origin time. When bits 0-1 of flags is
 * ASM_SESSION_CMD_RUN_START_RUN_WITH_DELAY, time msw is the msw of
 * the delay in us. For ASM_SESSION_CMD_RUN_START_RUN_WITH_DELAY,
 * maximum value of the 64 bit delay is 150 ms.
 */

} __packed;

#define ASM_SESSION_CMD_PAUSE 0x00010BD3
#define ASM_SESSION_CMD_SUSPEND 0x00010DEC
#define ASM_SESSION_CMD_GET_SESSIONTIME_V3 0x00010D9D
#define ASM_SESSION_CMD_REGISTER_FOR_RX_UNDERFLOW_EVENTS 0x00010BD5

struct asm_session_cmd_rgstr_rx_underflow {
	struct apr_hdr hdr;
	u16                  enable_flag;
/* Specifies whether a client is to receive events when an Rx
 * session underflows.
 * Supported values:
 * - 0 -- Do not send underflow events
 * - 1 -- Send underflow events
 */
	u16                  reserved;
	/* Reserved. This field must be set to zero.*/
} __packed;

#define ASM_SESSION_CMD_REGISTER_FORX_OVERFLOW_EVENTS 0x00010BD6

struct asm_session_cmd_regx_overflow {
	struct apr_hdr hdr;
	u16                  enable_flag;
/* Specifies whether a client is to receive events when a Tx
* session overflows.
 * Supported values:
 * - 0 -- Do not send overflow events
 * - 1 -- Send overflow events
 */

	u16                  reserved;
	/* Reserved. This field must be set to zero.*/
} __packed;

#define ASM_SESSION_EVENT_RX_UNDERFLOW        0x00010C17
#define ASM_SESSION_EVENTX_OVERFLOW           0x00010C18
#define ASM_SESSION_CMDRSP_GET_SESSIONTIME_V3 0x00010D9E

struct asm_session_cmdrsp_get_sessiontime_v3 {
	u32                  status;
	/* Status message (error code).
	* Supported values: Refer to @xhyperref{Q3,[Q3]}
	*/

	u32                  sessiontime_lsw;
	/* Lower 32 bits of the current session time in microseconds.*/

	u32                  sessiontime_msw;
	/* Upper 32 bits of the current session time in microseconds.*/

	u32                  absolutetime_lsw;
/* Lower 32 bits in micro seconds of the absolute time at which
 * the * sample corresponding to the above session time gets
 * rendered * to hardware. This absolute time may be slightly in the
 * future or past.
 */


	u32                  absolutetime_msw;
/* Upper 32 bits in micro seconds of the absolute time at which
 * the * sample corresponding to the above session time gets
 * rendered to * hardware. This absolute time may be slightly in the
 * future or past.
 */

} __packed;

#define ASM_SESSION_CMD_ADJUST_SESSION_CLOCK_V2     0x00010D9F

struct asm_session_cmd_adjust_session_clock_v2 {
	struct apr_hdr hdr;
u32                  adjustime_lsw;
/* Lower 32 bits of the signed 64-bit quantity that specifies the
 * adjustment time in microseconds to the session clock.
 *
 * Positive values indicate advancement of the session clock.
 * Negative values indicate delay of the session clock.
 */


	u32                  adjustime_msw;
/* Upper 32 bits of the signed 64-bit quantity that specifies
 * the adjustment time in microseconds to the session clock.
 * Positive values indicate advancement of the session clock.
 * Negative values indicate delay of the session clock.
 */

} __packed;

#define ASM_SESSION_CMDRSP_ADJUST_SESSION_CLOCK_V2    0x00010DA0

struct asm_session_cmdrsp_adjust_session_clock_v2 {
	u32                  status;
/* Status message (error code).
 * Supported values: Refer to @xhyperref{Q3,[Q3]}
 * An error means the session clock is not adjusted. In this case,
 * the next two fields are irrelevant.
 */


	u32                  actual_adjustime_lsw;
/* Lower 32 bits of the signed 64-bit quantity that specifies
 * the actual adjustment in microseconds performed by the aDSP.
 * A positive value indicates advancement of the session clock. A
 * negative value indicates delay of the session clock.
 */


	u32                  actual_adjustime_msw;
/* Upper 32 bits of the signed 64-bit quantity that specifies
 * the actual adjustment in microseconds performed by the aDSP.
 * A positive value indicates advancement of the session clock. A
 * negative value indicates delay of the session clock.
 */


	u32                  cmd_latency_lsw;
/* Lower 32 bits of the unsigned 64-bit quantity that specifies
 * the amount of time in microseconds taken to perform the session
 * clock adjustment.
 */


	u32                  cmd_latency_msw;
/* Upper 32 bits of the unsigned 64-bit quantity that specifies
 * the amount of time in microseconds taken to perform the session
 * clock adjustment.
 */

} __packed;

#define ASM_SESSION_CMD_GET_PATH_DELAY_V2	 0x00010DAF
#define ASM_SESSION_CMDRSP_GET_PATH_DELAY_V2 0x00010DB0

struct asm_session_cmdrsp_get_path_delay_v2 {
	u32                  status;
/* Status message (error code). Whether this get delay operation
 * is successful or not. Delay value is valid only if status is
 * success.
 * Supported values: Refer to @xhyperref{Q5,[Q5]}
 */

	u32                  audio_delay_lsw;
	/* Upper 32 bits of the aDSP delay in microseconds. */

	u32                  audio_delay_msw;
	/* Lower 32 bits of the aDSP delay  in microseconds. */

} __packed;

/* adsp_asm_session_command.h*/
#define ASM_STREAM_CMD_OPEN_WRITE_V2       0x00010D8F
#define ASM_STREAM_CMD_OPEN_WRITE_V3       0x00010DB3

#define ASM_LOW_LATENCY_STREAM_SESSION				0x10000000

#define ASM_ULTRA_LOW_LATENCY_STREAM_SESSION			0x20000000

#define ASM_LEGACY_STREAM_SESSION                                      0


struct asm_stream_cmd_open_write_v3 {
	struct apr_hdr			hdr;
	uint32_t                    mode_flags;
/* Mode flags that configure the stream to notify the client
 * whenever it detects an SR/CM change at the input to its POPP.
 * Supported values for bits 0 to 1:
 * - Reserved; clients must set them to zero.
 * Supported values for bit 2:
 * - 0 -- SR/CM change notification event is disabled.
 * - 1 -- SR/CM change notification event is enabled.
 * - Use #ASM_BIT_MASK_SR_CM_CHANGE_NOTIFY_FLAG and
 * #ASM_SHIFT_SR_CM_CHANGE_NOTIFY_FLAG to set or get this bit.
 *
 * Supported values for bit 31:
 * - 0 -- Stream to be opened in on-Gapless mode.
 * - 1 -- Stream to be opened in Gapless mode. In Gapless mode,
 * successive streams must be opened with same session ID but
 * different stream IDs.
 *
 * - Use #ASM_BIT_MASK_GAPLESS_MODE_FLAG and
 * #ASM_SHIFT_GAPLESS_MODE_FLAG to set or get this bit.
 *
 *
 * @note1hang MIDI and DTMF streams cannot be opened in Gapless mode.
 */

	uint16_t                    sink_endpointype;
/*< Sink point type.
 * Supported values:
 * - 0 -- Device matrix
 * - Other values are reserved.
 *
 * The device matrix is the gateway to the hardware ports.
 */

	uint16_t                    bits_per_sample;
/*< Number of bits per sample processed by ASM modules.
 * Supported values: 16 and 24 bits per sample
 */

	uint32_t                    postprocopo_id;
/*< Specifies the topology (order of processing) of
 * postprocessing algorithms. <i>None</i> means no postprocessing.
 * Supported values:
 * - #ASM_STREAM_POSTPROCOPO_ID_DEFAULT
 * - #ASM_STREAM_POSTPROCOPO_ID_MCH_PEAK_VOL
 * - #ASM_STREAM_POSTPROCOPO_ID_NONE
 *
 * This field can also be enabled through SetParams flags.
 */

	uint32_t                    dec_fmt_id;
/*< Configuration ID of the decoder media format.
 *
 * Supported values:
 * - #ASM_MEDIA_FMT_MULTI_CHANNEL_PCM_V2
 * - #ASM_MEDIA_FMT_ADPCM
 * - #ASM_MEDIA_FMT_MP3
 * - #ASM_MEDIA_FMT_AAC_V2
 * - #ASM_MEDIA_FMT_DOLBY_AAC
 * - #ASM_MEDIA_FMT_AMRNB_FS
 * - #ASM_MEDIA_FMT_AMRWB_FS
 * - #ASM_MEDIA_FMT_AMR_WB_PLUS_V2
 * - #ASM_MEDIA_FMT_V13K_FS
 * - #ASM_MEDIA_FMT_EVRC_FS
 * - #ASM_MEDIA_FMT_EVRCB_FS
 * - #ASM_MEDIA_FMT_EVRCWB_FS
 * - #ASM_MEDIA_FMT_SBC
 * - #ASM_MEDIA_FMT_WMA_V10PRO_V2
 * - #ASM_MEDIA_FMT_WMA_V9_V2
 * - #ASM_MEDIA_FMT_AC3_DEC
 * - #ASM_MEDIA_FMT_EAC3_DEC
 * - #ASM_MEDIA_FMT_G711_ALAW_FS
 * - #ASM_MEDIA_FMT_G711_MLAW_FS
 * - #ASM_MEDIA_FMT_G729A_FS
 * - #ASM_MEDIA_FMT_FR_FS
 * - #ASM_MEDIA_FMT_VORBIS
 * - #ASM_MEDIA_FMT_FLAC
 * - #ASM_MEDIA_FMT_EXAMPLE
 */
} __packed;

#define ASM_STREAM_CMD_OPEN_READ_V2                 0x00010D8C

#define ASM_STREAM_CMD_OPEN_READ_V3                 0x00010DB4

/* Definition of the timestamp type flag bitmask */
#define ASM_BIT_MASKIMESTAMPYPE_FLAG        (0x00000020UL)

/* Definition of the timestamp type flag shift value. */
#define ASM_SHIFTIMESTAMPYPE_FLAG 5

/* Relative timestamp is identified by this value.*/
#define ASM_RELATIVEIMESTAMP      0

/* Absolute timestamp is identified by this value.*/
#define ASM_ABSOLUTEIMESTAMP      1

/* Bit shift for the stream_perf_mode subfield. */
#define ASM_SHIFT_STREAM_PERF_MODE_FLAG_IN_OPEN_READ              29

struct asm_stream_cmd_open_read_v3 {
	struct apr_hdr hdr;
	u32                    mode_flags;
/* Mode flags that indicate whether meta information per encoded
 * frame is to be provided.
 * Supported values for bit 4:
 *
 * - 0 -- Return data buffer contains all encoded frames only; it
 * does not contain frame metadata.
 *
 * - 1 -- Return data buffer contains an array of metadata and
 * encoded frames.
 *
 * - Use #ASM_BIT_MASK_META_INFO_FLAG as the bitmask and
 * #ASM_SHIFT_META_INFO_FLAG as the shift value for this bit.
 *
 *
 * Supported values for bit 5:
 *
 * - ASM_RELATIVEIMESTAMP -- ASM_DATA_EVENT_READ_DONE_V2 will have
 * - relative time-stamp.
 * - ASM_ABSOLUTEIMESTAMP -- ASM_DATA_EVENT_READ_DONE_V2 will
 * - have absolute time-stamp.
 *
 * - Use #ASM_BIT_MASKIMESTAMPYPE_FLAG as the bitmask and
 * #ASM_SHIFTIMESTAMPYPE_FLAG as the shift value for this bit.
 *
 * All other bits are reserved; clients must set them to zero.
 */

	u32                    src_endpointype;
/* Specifies the endpoint providing the input samples.
 * Supported values:
 * - 0 -- Device matrix
 * - All other values are reserved; clients must set them to zero.
 * Otherwise, an error is returned.
 * The device matrix is the gateway from the tunneled Tx ports.
 */

	u32                    preprocopo_id;
/* Specifies the topology (order of processing) of preprocessing
 * algorithms. <i>None</i> means no preprocessing.
 * Supported values:
 * - #ASM_STREAM_PREPROCOPO_ID_DEFAULT
 * - #ASM_STREAM_PREPROCOPO_ID_NONE
 *
 * This field can also be enabled through SetParams flags.
 */

	u32                    enc_cfg_id;
/* Media configuration ID for encoded output.
 * Supported values:
 * - #ASM_MEDIA_FMT_MULTI_CHANNEL_PCM_V2
 * - #ASM_MEDIA_FMT_AAC_V2
 * - #ASM_MEDIA_FMT_AMRNB_FS
 * - #ASM_MEDIA_FMT_AMRWB_FS
 * - #ASM_MEDIA_FMT_V13K_FS
 * - #ASM_MEDIA_FMT_EVRC_FS
 * - #ASM_MEDIA_FMT_EVRCB_FS
 * - #ASM_MEDIA_FMT_EVRCWB_FS
 * - #ASM_MEDIA_FMT_SBC
 * - #ASM_MEDIA_FMT_G711_ALAW_FS
 * - #ASM_MEDIA_FMT_G711_MLAW_FS
 * - #ASM_MEDIA_FMT_G729A_FS
 * - #ASM_MEDIA_FMT_EXAMPLE
 * - #ASM_MEDIA_FMT_WMA_V8
 */

	u16                    bits_per_sample;
/* Number of bits per sample processed by ASM modules.
 * Supported values: 16 and 24 bits per sample
 */

	u16                    reserved;
/* Reserved for future use. This field must be set to zero.*/
} __packed;

#define ASM_POPP_OUTPUT_SR_NATIVE_RATE                                  0

/* Enumeration for the maximum sampling rate at the POPP output.*/
#define ASM_POPP_OUTPUT_SR_MAX_RATE             48000

#define ASM_STREAM_CMD_OPEN_READWRITE_V2        0x00010D8D
#define ASM_STREAM_CMD_OPEN_READWRITE_V2        0x00010D8D
#define ASM_STREAM_CMD_OPEN_READ_V2             0x00010D8C

struct asm_stream_cmd_open_readwrite_v2 {
	struct apr_hdr         hdr;
	u32                    mode_flags;
/* Mode flags.
 * Supported values for bit 2:
 * - 0 -- SR/CM change notification event is disabled.
 * - 1 -- SR/CM change notification event is enabled. Use
 * #ASM_BIT_MASK_SR_CM_CHANGE_NOTIFY_FLAG and
 * #ASM_SHIFT_SR_CM_CHANGE_NOTIFY_FLAG to set or
 * getting this flag.
 *
 * Supported values for bit 4:
 * - 0 -- Return read data buffer contains all encoded frames only; it
 * does not contain frame metadata.
 * - 1 -- Return read data buffer contains an array of metadata and
 * encoded frames.
 *
 * All other bits are reserved; clients must set them to zero.
 */

	u32                    postprocopo_id;
/* Specifies the topology (order of processing) of postprocessing
 * algorithms. <i>None</i> means no postprocessing.
 *
 * Supported values:
 * - #ASM_STREAM_POSTPROCOPO_ID_DEFAULT
 * - #ASM_STREAM_POSTPROCOPO_ID_MCH_PEAK_VOL
 * - #ASM_STREAM_POSTPROCOPO_ID_NONE
 */

	u32                    dec_fmt_id;
/* Specifies the media type of the input data. PCM indicates that
 * no decoding must be performed, e.g., this is an NT encoder
 * session.
 * Supported values:
 * - #ASM_MEDIA_FMT_MULTI_CHANNEL_PCM_V2
 * - #ASM_MEDIA_FMT_ADPCM
 * - #ASM_MEDIA_FMT_MP3
 * - #ASM_MEDIA_FMT_AAC_V2
 * - #ASM_MEDIA_FMT_DOLBY_AAC
 * - #ASM_MEDIA_FMT_AMRNB_FS
 * - #ASM_MEDIA_FMT_AMRWB_FS
 * - #ASM_MEDIA_FMT_V13K_FS
 * - #ASM_MEDIA_FMT_EVRC_FS
 * - #ASM_MEDIA_FMT_EVRCB_FS
 * - #ASM_MEDIA_FMT_EVRCWB_FS
 * - #ASM_MEDIA_FMT_SBC
 * - #ASM_MEDIA_FMT_WMA_V10PRO_V2
 * - #ASM_MEDIA_FMT_WMA_V9_V2
 * - #ASM_MEDIA_FMT_AMR_WB_PLUS_V2
 * - #ASM_MEDIA_FMT_AC3_DEC
 * - #ASM_MEDIA_FMT_G711_ALAW_FS
 * - #ASM_MEDIA_FMT_G711_MLAW_FS
 * - #ASM_MEDIA_FMT_G729A_FS
 * - #ASM_MEDIA_FMT_EXAMPLE
 */

	u32                    enc_cfg_id;
/* Specifies the media type for the output of the stream. PCM
 * indicates that no encoding must be performed, e.g., this is an NT
 * decoder session.
 * Supported values:
 * - #ASM_MEDIA_FMT_MULTI_CHANNEL_PCM_V2
 * - #ASM_MEDIA_FMT_AAC_V2
 * - #ASM_MEDIA_FMT_AMRNB_FS
 * - #ASM_MEDIA_FMT_AMRWB_FS
 * - #ASM_MEDIA_FMT_V13K_FS
 * - #ASM_MEDIA_FMT_EVRC_FS
 * - #ASM_MEDIA_FMT_EVRCB_FS
 * - #ASM_MEDIA_FMT_EVRCWB_FS
 * - #ASM_MEDIA_FMT_SBC
 * - #ASM_MEDIA_FMT_G711_ALAW_FS
 * - #ASM_MEDIA_FMT_G711_MLAW_FS
 * - #ASM_MEDIA_FMT_G729A_FS
 * - #ASM_MEDIA_FMT_EXAMPLE
 * - #ASM_MEDIA_FMT_WMA_V8
 */

	u16                    bits_per_sample;
/* Number of bits per sample processed by ASM modules.
 * Supported values: 16 and 24 bits per sample
 */

	u16                    reserved;
/* Reserved for future use. This field must be set to zero.*/

} __packed;

#define ASM_STREAM_CMD_OPEN_LOOPBACK_V2 0x00010D8E
struct asm_stream_cmd_open_loopback_v2 {
	struct apr_hdr         hdr;
	u32                    mode_flags;
/* Mode flags.
 * Bit 0-31: reserved; client should set these bits to 0
 */
	u16                    src_endpointype;
	/* Endpoint type. 0 = Tx Matrix */
	u16                    sink_endpointype;
	/* Endpoint type. 0 = Rx Matrix */
	u32                    postprocopo_id;
/* Postprocessor topology ID. Specifies the topology of
 * postprocessing algorithms.
 */

	u16                    bits_per_sample;
/* The number of bits per sample processed by ASM modules
 * Supported values: 16 and 24 bits per sample
 */
	u16                    reserved;
/* Reserved for future use. This field must be set to zero. */
} __packed;

#define ASM_STREAM_CMD_CLOSE             0x00010BCD
#define ASM_STREAM_CMD_FLUSH             0x00010BCE


#define ASM_STREAM_CMD_FLUSH_READBUFS   0x00010C09
#define ASM_STREAM_CMD_SET_PP_PARAMS_V2 0x00010DA1

struct asm_stream_cmd_set_pp_params_v2 {
	u32                  data_payload_addr_lsw;
/* LSW of parameter data payload address. Supported values: any. */
	u32                  data_payload_addr_msw;
/* MSW of Parameter data payload address. Supported values: any.
 * - Must be set to zero for in-band data.
 * - In the case of 32 bit Shared memory address, msw  field must be
 * - set to zero.
 * - In the case of 36 bit shared memory address, bit 31 to bit 4 of
 * msw
 *
 * - must be set to zero.
 */
	u32                  mem_map_handle;
/* Supported Values: Any.
* memory map handle returned by DSP through
* ASM_CMD_SHARED_MEM_MAP_REGIONS
* command.
* if mmhandle is NULL, the ParamData payloads are within the
* message payload (in-band).
* If mmhandle is non-NULL, the ParamData payloads begin at the
* address specified in the address msw and lsw (out-of-band).
*/

	u32                  data_payload_size;
/* Size in bytes of the variable payload accompanying the
message, or in shared memory. This field is used for parsing the
parameter payload. */

} __packed;


struct asm_stream_param_data_v2 {
	u32                  module_id;
	/* Unique module ID. */

	u32                  param_id;
	/* Unique parameter ID. */

	u16                  param_size;
/* Data size of the param_id/module_id combination. This is
 * a multiple of 4 bytes.
 */

	u16                  reserved;
/* Reserved for future enhancements. This field must be set to
 * zero.
 */

} __packed;

#define ASM_STREAM_CMD_GET_PP_PARAMS_V2		0x00010DA2

struct asm_stream_cmd_get_pp_params_v2 {
	u32                  data_payload_addr_lsw;
	/* LSW of the parameter data payload address. */
	u32                  data_payload_addr_msw;
/* MSW of the parameter data payload address.
 * - Size of the shared memory, if specified, shall be large enough
 * to contain the whole ParamData payload, including Module ID,
 * Param ID, Param Size, and Param Values
 * - Must be set to zero for in-band data
 * - In the case of 32 bit Shared memory address, msw field must be
 * set to zero.
 * - In the case of 36 bit shared memory address, bit 31 to bit 4 of
 * msw must be set to zero.
 */

	u32                  mem_map_handle;
/* Supported Values: Any.
* memory map handle returned by DSP through ASM_CMD_SHARED_MEM_MAP_REGIONS
* command.
* if mmhandle is NULL, the ParamData payloads in the ACK are within the
* message payload (in-band).
* If mmhandle is non-NULL, the ParamData payloads in the ACK begin at the
* address specified in the address msw and lsw.
* (out-of-band).
*/

	u32                  module_id;
	/* Unique module ID. */

	u32                  param_id;
	/* Unique parameter ID. */

	u16                  param_max_size;
/* Maximum data size of the module_id/param_id combination. This
 * is a multiple of 4 bytes.
 */


	u16                  reserved;
/* Reserved for backward compatibility. Clients must set this
* field to zero.
*/

} __packed;

#define ASM_STREAM_CMD_SET_ENCDEC_PARAM 0x00010C10

#define ASM_PARAM_ID_ENCDEC_BITRATE     0x00010C13

struct asm_bitrate_param {
	u32                  bitrate;
/* Maximum supported bitrate. Only the AAC encoder is supported.*/

} __packed;

#define ASM_PARAM_ID_ENCDEC_ENC_CFG_BLK_V2 0x00010DA3
#define ASM_PARAM_ID_AAC_SBR_PS_FLAG		 0x00010C63

/* Flag to turn off both SBR and PS processing, if they are
 * present in the bitstream.
 */

#define ASM_AAC_SBR_OFF_PS_OFF (2)

/* Flag to turn on SBR but turn off PS processing,if they are
 * present in the bitstream.
 */

#define ASM_AAC_SBR_ON_PS_OFF  (1)

/* Flag to turn on both SBR and PS processing, if they are
 * present in the bitstream (default behavior).
 */


#define ASM_AAC_SBR_ON_PS_ON   (0)

/* Structure for an AAC SBR PS processing flag. */

/*  Payload of the #ASM_PARAM_ID_AAC_SBR_PS_FLAG parameter in the
 * #ASM_STREAM_CMD_SET_ENCDEC_PARAM command.
 */
struct asm_aac_sbr_ps_flag_param {
	struct apr_hdr hdr;
	struct asm_stream_cmd_set_encdec_param  encdec;
	struct asm_enc_cfg_blk_param_v2	encblk;

	u32                  sbr_ps_flag;
/* Control parameter to enable or disable SBR/PS processing in
 * the AAC bitstream. Use the following macros to set this field:
 * - #ASM_AAC_SBR_OFF_PS_OFF -- Turn off both SBR and PS
 * processing, if they are present in the bitstream.
 * - #ASM_AAC_SBR_ON_PS_OFF -- Turn on SBR processing, but not PS
 * processing, if they are present in the bitstream.
 * - #ASM_AAC_SBR_ON_PS_ON -- Turn on both SBR and PS processing,
 * if they are present in the bitstream (default behavior).
 * - All other values are invalid.
 * Changes are applied to the next decoded frame.
 */
} __packed;

#define ASM_PARAM_ID_AAC_DUAL_MONO_MAPPING                      0x00010C64

/*	First single channel element in a dual mono bitstream.*/
#define ASM_AAC_DUAL_MONO_MAP_SCE_1                                 (1)

/*	Second single channel element in a dual mono bitstream.*/
#define ASM_AAC_DUAL_MONO_MAP_SCE_2                                 (2)

/* Structure for AAC decoder dual mono channel mapping. */


struct asm_aac_dual_mono_mapping_param {
	struct apr_hdr							hdr;
	struct asm_stream_cmd_set_encdec_param	encdec;
	u16    left_channel_sce;
	u16    right_channel_sce;

} __packed;

#define ASM_STREAM_CMDRSP_GET_PP_PARAMS_V2 0x00010DA4

struct asm_stream_cmdrsp_get_pp_params_v2 {
	u32                  status;
} __packed;

#define ASM_PARAM_ID_AC3_KARAOKE_MODE 0x00010D73

/* Enumeration for both vocals in a karaoke stream.*/
#define AC3_KARAOKE_MODE_NO_VOCAL     (0)

/* Enumeration for only the left vocal in a karaoke stream.*/
#define AC3_KARAOKE_MODE_LEFT_VOCAL   (1)

/* Enumeration for only the right vocal in a karaoke stream.*/
#define AC3_KARAOKE_MODE_RIGHT_VOCAL (2)

/* Enumeration for both vocal channels in a karaoke stream.*/
#define AC3_KARAOKE_MODE_BOTH_VOCAL             (3)
#define ASM_PARAM_ID_AC3_DRC_MODE               0x00010D74
/* Enumeration for the Custom Analog mode.*/
#define AC3_DRC_MODE_CUSTOM_ANALOG              (0)

/* Enumeration for the Custom Digital mode.*/
#define AC3_DRC_MODE_CUSTOM_DIGITAL             (1)
/* Enumeration for the Line Out mode (light compression).*/
#define AC3_DRC_MODE_LINE_OUT  (2)

/* Enumeration for the RF remodulation mode (heavy compression).*/
#define AC3_DRC_MODE_RF_REMOD                         (3)
#define ASM_PARAM_ID_AC3_DUAL_MONO_MODE               0x00010D75

/* Enumeration for playing dual mono in stereo mode.*/
#define AC3_DUAL_MONO_MODE_STEREO                     (0)

/* Enumeration for playing left mono.*/
#define AC3_DUAL_MONO_MODE_LEFT_MONO                  (1)

/* Enumeration for playing right mono.*/
#define AC3_DUAL_MONO_MODE_RIGHT_MONO                 (2)

/* Enumeration for mixing both dual mono channels and playing them.*/
#define AC3_DUAL_MONO_MODE_MIXED_MONO        (3)
#define ASM_PARAM_ID_AC3_STEREO_DOWNMIX_MODE 0x00010D76

/* Enumeration for using the Downmix mode indicated in the bitstream. */

#define AC3_STEREO_DOWNMIX_MODE_AUTO_DETECT  (0)

/* Enumeration for Surround Compatible mode (preserves the
 * surround information).
 */

#define AC3_STEREO_DOWNMIX_MODE_LT_RT        (1)
/* Enumeration for Mono Compatible mode (if the output is to be
 * further downmixed to mono).
 */

#define AC3_STEREO_DOWNMIX_MODE_LO_RO (2)

/* ID of the AC3 PCM scale factor parameter in the
 * #ASM_STREAM_CMD_SET_ENCDEC_PARAM command.
 */
#define ASM_PARAM_ID_AC3_PCM_SCALEFACTOR 0x00010D78

/* ID of the AC3 DRC boost scale factor parameter in the
 * #ASM_STREAM_CMD_SET_ENCDEC_PARAM command.
 */
#define ASM_PARAM_ID_AC3_DRC_BOOST_SCALEFACTOR 0x00010D79

/* ID of the AC3 DRC cut scale factor parameter in the
 * #ASM_STREAM_CMD_SET_ENCDEC_PARAM command.
 */
#define ASM_PARAM_ID_AC3_DRC_CUT_SCALEFACTOR 0x00010D7A

/* Structure for AC3 Generic Parameter. */

/*  Payload of the AC3 parameters in the
 * #ASM_STREAM_CMD_SET_ENCDEC_PARAM command.
 */
struct asm_ac3_generic_param {
	struct apr_hdr hdr;
	struct asm_stream_cmd_set_encdec_param  encdec;
	struct asm_enc_cfg_blk_param_v2	encblk;
	u32                  generic_parameter;
/* AC3 generic parameter. Select from one of the following
 * possible values.
 *
 * For #ASM_PARAM_ID_AC3_KARAOKE_MODE, supported values are:
 * - AC3_KARAOKE_MODE_NO_VOCAL
 * - AC3_KARAOKE_MODE_LEFT_VOCAL
 * - AC3_KARAOKE_MODE_RIGHT_VOCAL
 * - AC3_KARAOKE_MODE_BOTH_VOCAL
 *
 * For #ASM_PARAM_ID_AC3_DRC_MODE, supported values are:
 * - AC3_DRC_MODE_CUSTOM_ANALOG
 * - AC3_DRC_MODE_CUSTOM_DIGITAL
 * - AC3_DRC_MODE_LINE_OUT
 * - AC3_DRC_MODE_RF_REMOD
 *
 * For #ASM_PARAM_ID_AC3_DUAL_MONO_MODE, supported values are:
 * - AC3_DUAL_MONO_MODE_STEREO
 * - AC3_DUAL_MONO_MODE_LEFT_MONO
 * - AC3_DUAL_MONO_MODE_RIGHT_MONO
 * - AC3_DUAL_MONO_MODE_MIXED_MONO
 *
 * For #ASM_PARAM_ID_AC3_STEREO_DOWNMIX_MODE, supported values are:
 * - AC3_STEREO_DOWNMIX_MODE_AUTO_DETECT
 * - AC3_STEREO_DOWNMIX_MODE_LT_RT
 * - AC3_STEREO_DOWNMIX_MODE_LO_RO
 *
 * For #ASM_PARAM_ID_AC3_PCM_SCALEFACTOR, supported values are
 * 0 to 1 in Q31 format.
 *
 * For #ASM_PARAM_ID_AC3_DRC_BOOST_SCALEFACTOR, supported values are
 * 0 to 1 in Q31 format.
 *
 * For #ASM_PARAM_ID_AC3_DRC_CUT_SCALEFACTOR, supported values are
 * 0 to 1 in Q31 format.
 */
} __packed;

/* Enumeration for Raw mode (no downmixing), which specifies
 * that all channels in the bitstream are to be played out as is
 * without any downmixing. (Default)
 */

#define WMAPRO_CHANNEL_MASK_RAW (-1)

/* Enumeration for setting the channel mask to 0. The 7.1 mode
 * (Home Theater) is assigned.
 */


#define WMAPRO_CHANNEL_MASK_ZERO 0x0000

/* Speaker layout mask for one channel (Home Theater, mono).
 * - Speaker front center
 */
#define WMAPRO_CHANNEL_MASK_1_C 0x0004

/* Speaker layout mask for two channels (Home Theater, stereo).
 * - Speaker front left
 * - Speaker front right
 */
#define WMAPRO_CHANNEL_MASK_2_L_R 0x0003

/* Speaker layout mask for three channels (Home Theater).
 * - Speaker front left
 * - Speaker front right
 * - Speaker front center
 */
#define WMAPRO_CHANNEL_MASK_3_L_C_R 0x0007

/* Speaker layout mask for two channels (stereo).
 * - Speaker back left
 * - Speaker back right
 */
#define WMAPRO_CHANNEL_MASK_2_Bl_Br  0x0030

/* Speaker layout mask for four channels.
 * - Speaker front left
 * - Speaker front right
 * - Speaker back left
 * - Speaker back right
*/
#define WMAPRO_CHANNEL_MASK_4_L_R_Bl_Br 0x0033

/* Speaker layout mask for four channels (Home Theater).
 * - Speaker front left
 * - Speaker front right
 * - Speaker front center
 * - Speaker back center
*/
#define WMAPRO_CHANNEL_MASK_4_L_R_C_Bc_HT 0x0107
/* Speaker layout mask for five channels.
 * - Speaker front left
 * - Speaker front right
 * - Speaker front center
 * - Speaker back left
 * - Speaker back right
 */
#define WMAPRO_CHANNEL_MASK_5_L_C_R_Bl_Br  0x0037

/* Speaker layout mask for five channels (5 mode, Home Theater).
 * - Speaker front left
 * - Speaker front right
 * - Speaker front center
 * - Speaker side left
 * - Speaker side right
 */
#define WMAPRO_CHANNEL_MASK_5_L_C_R_Sl_Sr_HT   0x0607
/* Speaker layout mask for six channels (5.1 mode).
 * - Speaker front left
 * - Speaker front right
 * - Speaker front center
 * - Speaker low frequency
 * - Speaker back left
 * - Speaker back right
 */
#define WMAPRO_CHANNEL_MASK_5DOT1_L_C_R_Bl_Br_SLF  0x003F
/* Speaker layout mask for six channels (5.1 mode, Home Theater).
 * - Speaker front left
 * - Speaker front right
 * - Speaker front center
 * - Speaker low frequency
 * - Speaker side left
 * - Speaker side right
 */
#define WMAPRO_CHANNEL_MASK_5DOT1_L_C_R_Sl_Sr_SLF_HT  0x060F
/* Speaker layout mask for six channels (5.1 mode, no LFE).
 * - Speaker front left
 * - Speaker front right
 * - Speaker front center
 * - Speaker back left
 * - Speaker back right
 * - Speaker back center
 */
#define WMAPRO_CHANNEL_MASK_5DOT1_L_C_R_Bl_Br_Bc  0x0137
/* Speaker layout mask for six channels (5.1 mode, Home Theater,
  * no LFE).
  * - Speaker front left
  * - Speaker front right
  * - Speaker front center
  * - Speaker back center
  * - Speaker side left
  * - Speaker side right
 */
#define WMAPRO_CHANNEL_MASK_5DOT1_L_C_R_Sl_Sr_Bc_HT   0x0707

/* Speaker layout mask for seven channels (6.1 mode).
 * - Speaker front left
 * - Speaker front right
 * - Speaker front center
 * - Speaker low frequency
 * - Speaker back left
 * - Speaker back right
 * - Speaker back center
 */
#define WMAPRO_CHANNEL_MASK_6DOT1_L_C_R_Bl_Br_Bc_SLF   0x013F

/* Speaker layout mask for seven channels (6.1 mode, Home
  * Theater).
  * - Speaker front left
  * - Speaker front right
  * - Speaker front center
  * - Speaker low frequency
  * - Speaker back center
  * - Speaker side left
  * - Speaker side right
*/
#define WMAPRO_CHANNEL_MASK_6DOT1_L_C_R_Sl_Sr_Bc_SLF_HT 0x070F

/* Speaker layout mask for seven channels (6.1 mode, no LFE).
 * - Speaker front left
 * - Speaker front right
 * - Speaker front center
 * - Speaker back left
 * - Speaker back right
 * - Speaker front left of center
 * - Speaker front right of center
*/
#define WMAPRO_CHANNEL_MASK_6DOT1_L_C_R_Bl_Br_SFLOC_SFROC   0x00F7

/* Speaker layout mask for seven channels (6.1 mode, Home
 * Theater, no LFE).
 * - Speaker front left
 * - Speaker front right
 * - Speaker front center
 * - Speaker side left
 * - Speaker side right
 * - Speaker front left of center
 * - Speaker front right of center
*/
#define WMAPRO_CHANNEL_MASK_6DOT1_L_C_R_Sl_Sr_SFLOC_SFROC_HT 0x0637

/* Speaker layout mask for eight channels (7.1 mode).
 * - Speaker front left
 * - Speaker front right
 * - Speaker front center
 * - Speaker back left
 * - Speaker back right
 * - Speaker low frequency
 * - Speaker front left of center
 * - Speaker front right of center
 */
#define WMAPRO_CHANNEL_MASK_7DOT1_L_C_R_Bl_Br_SLF_SFLOC_SFROC \
					0x00FF

/* Speaker layout mask for eight channels (7.1 mode, Home Theater).
 * - Speaker front left
 * - Speaker front right
 * - Speaker front center
 * - Speaker side left
 * - Speaker side right
 * - Speaker low frequency
 * - Speaker front left of center
 * - Speaker front right of center
 *
*/
#define WMAPRO_CHANNEL_MASK_7DOT1_L_C_R_Sl_Sr_SLF_SFLOC_SFROC_HT \
					0x063F

#define ASM_PARAM_ID_DEC_OUTPUT_CHAN_MAP  0x00010D82

/*	Maximum number of decoder output channels.*/
#define MAX_CHAN_MAP_CHANNELS  16

/* Structure for decoder output channel mapping. */

/* Payload of the #ASM_PARAM_ID_DEC_OUTPUT_CHAN_MAP parameter in the
 * #ASM_STREAM_CMD_SET_ENCDEC_PARAM command.
 */
struct asm_dec_out_chan_map_param {
	struct apr_hdr hdr;
	struct asm_stream_cmd_set_encdec_param  encdec;
	u32                 num_channels;
/* Number of decoder output channels.
 * Supported values: 0 to #MAX_CHAN_MAP_CHANNELS
 *
 * A value of 0 indicates native channel mapping, which is valid
 * only for NT mode. This means the output of the decoder is to be
 * preserved as is.
 */
	u8                  channel_mapping[MAX_CHAN_MAP_CHANNELS];
} __packed;

#define ASM_STREAM_CMD_OPEN_WRITE_COMPRESSED  0x00010D84

/* Bitmask for the IEC 61937 enable flag.*/
#define ASM_BIT_MASK_IEC_61937_STREAM_FLAG   (0x00000001UL)

/* Shift value for the IEC 61937 enable flag.*/
#define ASM_SHIFT_IEC_61937_STREAM_FLAG  0

/* Bitmask for the IEC 60958 enable flag.*/
#define ASM_BIT_MASK_IEC_60958_STREAM_FLAG   (0x00000002UL)

/* Shift value for the IEC 60958 enable flag.*/
#define ASM_SHIFT_IEC_60958_STREAM_FLAG   1

/* Payload format for open write compressed comand */

/* Payload format for the #ASM_STREAM_CMD_OPEN_WRITE_COMPRESSED
 * comand, which opens a stream for a given session ID and stream ID
 * to be rendered in the compressed format.
 */

struct asm_stream_cmd_open_write_compressed {
	struct apr_hdr hdr;
	u32                    flags;
/* Mode flags that configure the stream for a specific format.
 * Supported values:
 * - Bit 0 -- IEC 61937 compatibility
 *   - 0 -- Stream is not in IEC 61937 format
 *   - 1 -- Stream is in IEC 61937 format
 * - Bit 1 -- IEC 60958 compatibility
 *   - 0 -- Stream is not in IEC 60958 format
 *   - 1 -- Stream is in IEC 60958 format
 * - Bits 2 to 31 -- 0 (Reserved)
 *
 * For the same stream, bit 0 cannot be set to 0 and bit 1 cannot
 * be set to 1. A compressed stream connot have IEC 60958
 * packetization applied without IEC 61937 packetization.
 * @note1hang Currently, IEC 60958 packetized input streams are not
 * supported.
 */


	u32                    fmt_id;
/* Specifies the media type of the HDMI stream to be opened.
 * Supported values:
 * - #ASM_MEDIA_FMT_AC3_DEC
 * - #ASM_MEDIA_FMT_EAC3_DEC
 * - #ASM_MEDIA_FMT_DTS
 * - #ASM_MEDIA_FMT_ATRAC
 * - #ASM_MEDIA_FMT_MAT
 *
 * @note1hang This field must be set to a valid media type even if
 * IEC 61937 packetization is not performed by the aDSP.
 */

} __packed;


/*
    Indicates the number of samples per channel to be removed from the
    beginning of the stream.
*/
#define ASM_DATA_CMD_REMOVE_INITIAL_SILENCE 0x00010D67
/*
    Indicates the number of samples per channel to be removed from
    the end of the stream.
*/
#define ASM_DATA_CMD_REMOVE_TRAILING_SILENCE 0x00010D68
struct asm_data_cmd_remove_silence {
	struct apr_hdr hdr;
	u32	num_samples_to_remove;
	/**< Number of samples per channel to be removed.

	   @values 0 to (2@sscr{32}-1) */
} __packed;

#define ASM_STREAM_CMD_OPEN_READ_COMPRESSED                        0x00010D95

struct asm_stream_cmd_open_read_compressed {
	struct apr_hdr hdr;
	u32                    mode_flags;
/* Mode flags that indicate whether meta information per encoded
 * frame is to be provided.
 * Supported values for bit 4:
 * - 0 -- Return data buffer contains all encoded frames only; it does
 *      not contain frame metadata.
 * - 1 -- Return data buffer contains an array of metadata and encoded
 *      frames.
 * - Use #ASM_BIT_MASK_META_INFO_FLAG to set the bitmask and
 * #ASM_SHIFT_META_INFO_FLAG to set the shift value for this bit.
 * All other bits are reserved; clients must set them to zero.
 */

	u32                    frames_per_buf;
/* Indicates the number of frames that need to be returned per
 * read buffer
 * Supported values: should be greater than 0
 */

} __packed;

/* adsp_asm_stream_commands.h*/


/* adsp_asm_api.h (no changes)*/
#define ASM_STREAM_POSTPROCOPO_ID_DEFAULT \
								0x00010BE4
#define ASM_STREAM_POSTPROCOPO_ID_PEAKMETER \
								0x00010D83
#define ASM_STREAM_POSTPROCOPO_ID_NONE \
								0x00010C68
#define ASM_STREAM_POSTPROCOPO_ID_MCH_PEAK_VOL \
								0x00010D8B
#define ASM_STREAM_PREPROCOPO_ID_DEFAULT \
			ASM_STREAM_POSTPROCOPO_ID_DEFAULT
#define ASM_STREAM_PREPROCOPO_ID_NONE \
			ASM_STREAM_POSTPROCOPO_ID_NONE
#define ADM_CMD_COPP_OPENOPOLOGY_ID_NONE_AUDIO_COPP \
			0x00010312
#define ADM_CMD_COPP_OPENOPOLOGY_ID_SPEAKER_MONO_AUDIO_COPP \
								0x00010313
#define ADM_CMD_COPP_OPENOPOLOGY_ID_SPEAKER_STEREO_AUDIO_COPP \
								0x00010314
#define ADM_CMD_COPP_OPENOPOLOGY_ID_SPEAKER_STEREO_IIR_AUDIO_COPP\
								0x00010704
#define ADM_CMD_COPP_OPENOPOLOGY_ID_SPEAKER_MONO_AUDIO_COPP_MBDRCV2\
								0x0001070D
#define ADM_CMD_COPP_OPENOPOLOGY_ID_SPEAKER_STEREO_AUDIO_COPP_MBDRCV2\
								0x0001070E
#define ADM_CMD_COPP_OPENOPOLOGY_ID_SPEAKER_STEREO_IIR_AUDIO_COPP_MBDRCV2\
								0x0001070F
#define ADM_CMD_COPP_OPENOPOLOGY_ID_SPEAKER_MCH_PEAK_VOL \
								0x0001031B
#define ADM_CMD_COPP_OPENOPOLOGY_ID_MIC_MONO_AUDIO_COPP  0x00010315
#define ADM_CMD_COPP_OPENOPOLOGY_ID_MIC_STEREO_AUDIO_COPP 0x00010316
#define AUDPROC_COPPOPOLOGY_ID_MCHAN_IIR_AUDIO           0x00010715
#define ADM_CMD_COPP_OPENOPOLOGY_ID_DEFAULT_AUDIO_COPP   0x00010BE3
#define ADM_CMD_COPP_OPENOPOLOGY_ID_PEAKMETER_AUDIO_COPP 0x00010317
#define AUDPROC_MODULE_ID_AIG   0x00010716
#define AUDPROC_PARAM_ID_AIG_ENABLE		0x00010717
#define AUDPROC_PARAM_ID_AIG_CONFIG		0x00010718

struct Audio_AigParam {
	uint16_t	mode;
/*< Mode word for enabling AIG/SIG mode .
 * Byte offset: 0
 */
	int16_t		staticGainL16Q12;
/*< Static input gain when aigMode is set to 1.
 * Byte offset: 2
 */
	int16_t		initialGainDBL16Q7;
/*<Initial value that the adaptive gain update starts from dB
 * Q7 Byte offset: 4
 */
	int16_t		idealRMSDBL16Q7;
/*<Average RMS level that AIG attempts to achieve Q8.7
 * Byte offset: 6
 */
	int32_t		noiseGateL32;
/*Threshold below which signal is considered as noise and AIG
 * Byte offset: 8
 */
	int32_t		minGainL32Q15;
/*Minimum gain that can be provided by AIG Q16.15
 * Byte offset: 12
 */
	int32_t		maxGainL32Q15;
/*Maximum gain that can be provided by AIG Q16.15
 * Byte offset: 16
 */
	uint32_t		gainAtRtUL32Q31;
/*Attack/release time for AIG update Q1.31
 * Byte offset: 20
 */
	uint32_t		longGainAtRtUL32Q31;
/*Long attack/release time while updating gain for
 * noise/silence Q1.31 Byte offset: 24
 */

	uint32_t		rmsTavUL32Q32;
/* RMS smoothing time constant used for long-term RMS estimate
 * Q0.32 Byte offset: 28
 */

	uint32_t		gainUpdateStartTimMsUL32Q0;
/* The waiting time before which AIG starts to apply adaptive
 * gain update Q32.0 Byte offset: 32
 */

} __packed;


#define ADM_MODULE_ID_EANS                            0x00010C4A
#define ADM_PARAM_ID_EANS_ENABLE                      0x00010C4B
#define ADM_PARAM_ID_EANS_PARAMS                      0x00010C4C

struct adm_eans_enable {

	uint32_t                  enable_flag;
/*< Specifies whether EANS is disabled (0) or enabled
 * (nonzero).
 * This is supported only for sampling rates of 8, 12, 16, 24, 32,
 * and 48 kHz. It is not supported for sampling rates of 11.025,
 * 22.05, or 44.1 kHz.
 */

} __packed;


struct adm_eans_params {
	int16_t                         eans_mode;
/*< Mode word for enabling/disabling submodules.
 * Byte offset: 0
 */

	int16_t                         eans_input_gain;
/*< Q2.13 input gain to the EANS module.
 * Byte offset: 2
 */

	int16_t                         eans_output_gain;
/*< Q2.13 output gain to the EANS module.
 * Byte offset: 4
 */

	int16_t                         eansarget_ns;
/*< Target noise suppression level in dB.
 * Byte offset: 6
 */

	int16_t                         eans_s_alpha;
/*< Q3.12 over-subtraction factor for stationary noise
 * suppression.
 * Byte offset: 8
 */

	int16_t                         eans_n_alpha;
/* < Q3.12 over-subtraction factor for nonstationary noise
 * suppression.
 * Byte offset: 10
 */

	int16_t                         eans_n_alphamax;
/*< Q3.12 maximum over-subtraction factor for nonstationary
 * noise suppression.
 * Byte offset: 12
 */
	int16_t                         eans_e_alpha;
/*< Q15 scaling factor for excess noise suppression.
 * Byte offset: 14
 */

	int16_t                         eans_ns_snrmax;
/*< Upper boundary in dB for SNR estimation.
 * Byte offset: 16
 */

	int16_t                         eans_sns_block;
/*< Quarter block size for stationary noise suppression.
 * Byte offset: 18
 */

	int16_t                         eans_ns_i;
/*< Initialization block size for noise suppression.
 * Byte offset: 20
 */
	int16_t                         eans_np_scale;
/*< Power scale factor for nonstationary noise update.
 * Byte offset: 22
 */

	int16_t                         eans_n_lambda;
/*< Smoothing factor for higher level nonstationary noise
 * update.
 * Byte offset: 24
 */

	int16_t                         eans_n_lambdaf;
/*< Medium averaging factor for noise update.
 * Byte offset: 26
 */

	int16_t                         eans_gs_bias;
/*< Bias factor in dB for gain calculation.
 * Byte offset: 28
 */

	int16_t                         eans_gs_max;
/*< SNR lower boundary in dB for aggressive gain calculation.
 * Byte offset: 30
 */

	int16_t                         eans_s_alpha_hb;
/*< Q3.12 over-subtraction factor for high-band stationary
 * noise suppression.
 * Byte offset: 32
 */

	int16_t                         eans_n_alphamax_hb;
/*< Q3.12 maximum over-subtraction factor for high-band
 * nonstationary noise suppression.
 * Byte offset: 34
 */

	int16_t                         eans_e_alpha_hb;
/*< Q15 scaling factor for high-band excess noise suppression.
 * Byte offset: 36
 */

	int16_t                         eans_n_lambda0;
/*< Smoothing factor for nonstationary noise update during
 * speech activity.
 * Byte offset: 38
 */

	int16_t                         thresh;
/*< Threshold for generating a binary VAD decision.
 * Byte offset: 40
 */

	int16_t                         pwr_scale;
/*< Indirect lower boundary of the noise level estimate.
 * Byte offset: 42
 */

	int16_t                         hangover_max;
/*< Avoids mid-speech clipping and reliably detects weak speech
 * bursts at the end of speech activity.
 * Byte offset: 44
 */

	int16_t                         alpha_snr;
/*< Controls responsiveness of the VAD.
 * Byte offset: 46
 */

	int16_t                         snr_diff_max;
/*< Maximum SNR difference. Decreasing this parameter value may
 * help in making correct decisions during abrupt changes; however,
 * decreasing too much may increase false alarms during long
 * pauses/silences.
 * Byte offset: 48
 */

	int16_t                         snr_diff_min;
/*< Minimum SNR difference. Decreasing this parameter value may
 * help in making correct decisions during abrupt changes; however,
 * decreasing too much may increase false alarms during long
 * pauses/silences.
 * Byte offset: 50
 */

	int16_t                         init_length;
/*< Defines the number of frames for which a noise level
 * estimate is set to a fixed value.
 * Byte offset: 52
 */

	int16_t                         max_val;
/*< Defines the upper limit of the noise level.
 * Byte offset: 54
 */

	int16_t                         init_bound;
/*< Defines the initial bounding value for the noise level
 * estimate. This is used during the initial segment defined by the
 * init_length parameter.
 * Byte offset: 56
 */

	int16_t                         reset_bound;
/*< Reset boundary for noise tracking.
 * Byte offset: 58
 */

	int16_t                         avar_scale;
/*< Defines the bias factor in noise estimation.
 * Byte offset: 60
 */

	int16_t                         sub_nc;
/*< Defines the window length for noise estimation.
 * Byte offset: 62
 */

	int16_t                         spow_min;
/*< Defines the minimum signal power required to update the
 * boundaries for the noise floor estimate.
 * Byte offset: 64
 */

	int16_t                         eans_gs_fast;
/*< Fast smoothing factor for postprocessor gain.
 * Byte offset: 66
 */

	int16_t                         eans_gs_med;
/*< Medium smoothing factor for postprocessor gain.
 * Byte offset: 68
 */

	int16_t                         eans_gs_slow;
/*< Slow smoothing factor for postprocessor gain.
 * Byte offset: 70
 */

	int16_t                         eans_swb_salpha;
/*< Q3.12 super wideband aggressiveness factor for stationary
 * noise suppression.
 * Byte offset: 72
 */

	int16_t                         eans_swb_nalpha;
/*< Q3.12 super wideband aggressiveness factor for
 * nonstationary noise suppression.
 * Byte offset: 74
 */
} __packed;
#define ADM_MODULE_IDX_MIC_GAIN_CTRL   0x00010C35

/* @addtogroup audio_pp_param_ids
 * ID of the Tx mic gain control parameter used by the
 * #ADM_MODULE_IDX_MIC_GAIN_CTRL module.
 * @messagepayload
 * @structure{admx_mic_gain}
 * @tablespace
 * @inputtable{Audio_Postproc_ADM_PARAM_IDX_MIC_GAIN.tex}
 */
#define ADM_PARAM_IDX_MIC_GAIN       0x00010C36

/* Structure for a Tx mic gain parameter for the mic gain
 * control module.
 */


/* @brief Payload of the #ADM_PARAM_IDX_MIC_GAIN parameter in the
 * Tx Mic Gain Control module.
 */
struct admx_mic_gain {
	uint16_t                  tx_mic_gain;
	/*< Linear gain in Q13 format. */

	uint16_t                  reserved;
	/*< Clients must set this field to zero. */
} __packed;

/* end_addtogroup audio_pp_param_ids */

/* @ingroup audio_pp_module_ids
 * ID of the Rx Codec Gain Control module.
 *
 * This module supports the following parameter ID:
 * - #ADM_PARAM_ID_RX_CODEC_GAIN
 */
#define ADM_MODULE_ID_RX_CODEC_GAIN_CTRL       0x00010C37

/* @addtogroup audio_pp_param_ids
 * ID of the Rx codec gain control parameter used by the
 * #ADM_MODULE_ID_RX_CODEC_GAIN_CTRL module.
 *
 * @messagepayload
 * @structure{adm_rx_codec_gain}
 * @tablespace
 * @inputtable{Audio_Postproc_ADM_PARAM_ID_RX_CODEC_GAIN.tex}
*/
#define ADM_PARAM_ID_RX_CODEC_GAIN   0x00010C38

/* Structure for the Rx common codec gain control module. */


/* @brief Payload of the #ADM_PARAM_ID_RX_CODEC_GAIN parameter
 * in the Rx Codec Gain Control module.
 */


struct adm_rx_codec_gain {
	uint16_t                  rx_codec_gain;
	/*< Linear gain in Q13 format. */

	uint16_t                  reserved;
	/*< Clients must set this field to zero.*/
} __packed;

/* end_addtogroup audio_pp_param_ids */

/* @ingroup audio_pp_module_ids
 * ID of the HPF Tuning Filter module on the Tx path.
 * This module supports the following parameter IDs:
 * - #ADM_PARAM_ID_HPF_IIRX_FILTER_ENABLE_CONFIG
 * - #ADM_PARAM_ID_HPF_IIRX_FILTER_PRE_GAIN
 * - #ADM_PARAM_ID_HPF_IIRX_FILTER_CONFIG_PARAMS
 */
#define ADM_MODULE_ID_HPF_IIRX_FILTER    0x00010C3D

/* @addtogroup audio_pp_param_ids */
/* ID of the Tx HPF IIR filter enable parameter used by the
 * #ADM_MODULE_ID_HPF_IIRX_FILTER module.
 * @parspace Message payload
 * @structure{adm_hpfx_iir_filter_enable_cfg}
 * @tablespace
 * @inputtable{Audio_Postproc_ADM_PARAM_ID_HPF_IIRX_FILTER_ENABLE_CONFIG.tex}
 */
#define ADM_PARAM_ID_HPF_IIRX_FILTER_ENABLE_CONFIG   0x00010C3E

/* ID of the Tx HPF IIR filter pregain parameter used by the
 * #ADM_MODULE_ID_HPF_IIRX_FILTER module.
 * @parspace Message payload
 * @structure{adm_hpfx_iir_filter_pre_gain}
 * @tablespace
 * @inputtable{Audio_Postproc_ADM_PARAM_ID_HPF_IIRX_FILTER_PRE_GAIN.tex}
 */
#define ADM_PARAM_ID_HPF_IIRX_FILTER_PRE_GAIN   0x00010C3F

/* ID of the Tx HPF IIR filter configuration parameters used by the
 * #ADM_MODULE_ID_HPF_IIRX_FILTER module.
 * @parspace Message payload
 * @structure{adm_hpfx_iir_filter_cfg_params}
 * @tablespace
 * @inputtable{Audio_Postproc_ADM_PARAM_ID_HPF_IIRX_FILTER_CONFIG_PA
 * RAMS.tex}
 */
#define ADM_PARAM_ID_HPF_IIRX_FILTER_CONFIG_PARAMS  0x00010C40

/* Structure for enabling a configuration parameter for
 * the HPF IIR tuning filter module on the Tx path.
 */

/* @brief Payload of the #ADM_PARAM_ID_HPF_IIRX_FILTER_ENABLE_CONFIG
 * parameter in the Tx path HPF Tuning Filter module.
 */
struct adm_hpfx_iir_filter_enable_cfg {
	uint32_t                  enable_flag;
/*< Specifies whether the HPF tuning filter is disabled (0) or
 * enabled (nonzero).
 */
} __packed;


/* Structure for the pregain parameter for the HPF
	IIR tuning filter module on the Tx path. */


/* @brief Payload of the #ADM_PARAM_ID_HPF_IIRX_FILTER_PRE_GAIN parameter
 * in the Tx path HPF Tuning Filter module.
 */
struct adm_hpfx_iir_filter_pre_gain {
	uint16_t                  pre_gain;
	/*< Linear gain in Q13 format. */

	uint16_t                  reserved;
	/*< Clients must set this field to zero.*/
} __packed;


/* Structure for the configuration parameter for the
	HPF IIR tuning filter module on the Tx path. */


/* @brief Payload of the #ADM_PARAM_ID_HPF_IIRX_FILTER_CONFIG_PARAMS
 * parameters in the Tx path HPF Tuning Filter module. \n
 * \n
 * This structure is followed by tuning filter coefficients as follows: \n
 * - Sequence of int32_t FilterCoeffs.
 * Each band has five coefficients, each in int32_t format in the order of
 * b0, b1, b2, a1, a2.
 * - Sequence of int16_t NumShiftFactor.
 * One int16_t per band. The numerator shift factor is related to the Q
 * factor of the filter coefficients.
 * - Sequence of uint16_t PanSetting.
 * One uint16_t for each band to indicate application of the filter to
 * left (0), right (1), or both (2) channels.
 */
struct adm_hpfx_iir_filter_cfg_params {
	uint16_t                  num_biquad_stages;
/*< Number of bands.
 * Supported values: 0 to 20
 */

	uint16_t                  reserved;
	/*< Clients must set this field to zero.*/
} __packed;

/* end_addtogroup audio_pp_module_ids */

/* @addtogroup audio_pp_module_ids */
/* ID of the Tx path IIR Tuning Filter module.
 *	This module supports the following parameter IDs:
 *	- #ADM_PARAM_IDX_IIR_FILTER_ENABLE_CONFIG
 */
#define ADM_MODULE_IDX_IIR_FILTER 0x00010C41

/* ID of the Rx path IIR Tuning Filter module for the left channel.
 *	The parameter IDs of the IIR tuning filter module
 *	(#ASM_MODULE_ID_IIRUNING_FILTER) are used for the left IIR Rx tuning
 *	filter.
 *
 * Pan parameters are not required for this per-channel IIR filter; the pan
 * parameters are ignored by this module.
 */
#define ADM_MODULE_ID_LEFT_IIRUNING_FILTER      0x00010705

/* ID of the the Rx path IIR Tuning Filter module for the right
 * channel.
 * The parameter IDs of the IIR tuning filter module
 * (#ASM_MODULE_ID_IIRUNING_FILTER) are used for the right IIR Rx
 * tuning filter.
 *
 * Pan parameters are not required for this per-channel IIR filter;
 * the pan parameters are ignored by this module.
 */
#define ADM_MODULE_ID_RIGHT_IIRUNING_FILTER    0x00010706

/* end_addtogroup audio_pp_module_ids */

/* @addtogroup audio_pp_param_ids */

/* ID of the Tx IIR filter enable parameter used by the
 * #ADM_MODULE_IDX_IIR_FILTER module.
 * @parspace Message payload
 * @structure{admx_iir_filter_enable_cfg}
 * @tablespace
 * @inputtable{Audio_Postproc_ADM_PARAM_IDX_IIR_FILTER_ENABLE_CONFIG.tex}
 */
#define ADM_PARAM_IDX_IIR_FILTER_ENABLE_CONFIG   0x00010C42

/* ID of the Tx IIR filter pregain parameter used by the
 * #ADM_MODULE_IDX_IIR_FILTER module.
 * @parspace Message payload
 * @structure{admx_iir_filter_pre_gain}
 * @tablespace
 * @inputtable{Audio_Postproc_ADM_PARAM_IDX_IIR_FILTER_PRE_GAIN.tex}
 */
#define ADM_PARAM_IDX_IIR_FILTER_PRE_GAIN    0x00010C43

/* ID of the Tx IIR filter configuration parameters used by the
 * #ADM_MODULE_IDX_IIR_FILTER module.
 * @parspace Message payload
 * @structure{admx_iir_filter_cfg_params}
 * @tablespace
 * @inputtable{Audio_Postproc_ADM_PARAM_IDX_IIR_FILTER_CONFIG_PARAMS.tex}
 */
#define ADM_PARAM_IDX_IIR_FILTER_CONFIG_PARAMS     0x00010C44

/* Structure for enabling the configuration parameter for the
 * IIR filter module on the Tx path.
 */

/* @brief Payload of the #ADM_PARAM_IDX_IIR_FILTER_ENABLE_CONFIG
 * parameter in the Tx Path IIR Tuning Filter module.
 */

struct admx_iir_filter_enable_cfg {
	uint32_t                  enable_flag;
/*< Specifies whether the IIR tuning filter is disabled (0) or
 * enabled (nonzero).
 */

} __packed;


/* Structure for the pregain parameter for the
 * IIR filter module on the Tx path.
 */


/* @brief Payload of the #ADM_PARAM_IDX_IIR_FILTER_PRE_GAIN
 * parameter in the Tx Path IIR Tuning Filter module.
 */

struct admx_iir_filter_pre_gain {
	uint16_t                  pre_gain;
	/*< Linear gain in Q13 format. */

	uint16_t                  reserved;
	/*< Clients must set this field to zero.*/
} __packed;


/* Structure for the configuration parameter for the
 * IIR filter module on the Tx path.
 */


/* @brief Payload of the #ADM_PARAM_IDX_IIR_FILTER_CONFIG_PARAMS
 * parameter in the Tx Path IIR Tuning Filter module. \n
 *	\n
 * This structure is followed by the HPF IIR filter coefficients on
 * the Tx path as follows: \n
 * - Sequence of int32_t ulFilterCoeffs. Each band has five
 * coefficients, each in int32_t format in the order of b0, b1, b2,
 * a1, a2.
 * - Sequence of int16_t sNumShiftFactor. One int16_t per band. The
 * numerator shift factor is related to the Q factor of the filter
 * coefficients.
 * - Sequence of uint16_t usPanSetting. One uint16_t for each band
 * to indicate if the filter is applied to left (0), right (1), or
 * both (2) channels.
 */
struct admx_iir_filter_cfg_params {
	uint16_t                  num_biquad_stages;
/*< Number of bands.
 * Supported values: 0 to 20
 */

	uint16_t                  reserved;
	/*< Clients must set this field to zero.*/
} __packed;

/* end_addtogroup audio_pp_module_ids */

/* @ingroup audio_pp_module_ids
 *	ID of the QEnsemble module.
 *	This module supports the following parameter IDs:
 *	- #ADM_PARAM_ID_QENSEMBLE_ENABLE
 *	- #ADM_PARAM_ID_QENSEMBLE_BACKGAIN
 *	- #ADM_PARAM_ID_QENSEMBLE_SET_NEW_ANGLE
 */
#define ADM_MODULE_ID_QENSEMBLE    0x00010C59

/* @addtogroup audio_pp_param_ids */
/* ID of the QEnsemble enable parameter used by the
 * #ADM_MODULE_ID_QENSEMBLE module.
 * @messagepayload
 * @structure{adm_qensemble_enable}
 * @tablespace
 * @inputtable{Audio_Postproc_ADM_PARAM_ID_QENSEMBLE_ENABLE.tex}
 */
#define ADM_PARAM_ID_QENSEMBLE_ENABLE   0x00010C60

/* ID of the QEnsemble back gain parameter used by the
 * #ADM_MODULE_ID_QENSEMBLE module.
 * @messagepayload
 * @structure{adm_qensemble_param_backgain}
 * @tablespace
 * @inputtable{Audio_Postproc_ADM_PARAM_ID_QENSEMBLE_BACKGAIN.tex}
 */
#define ADM_PARAM_ID_QENSEMBLE_BACKGAIN   0x00010C61

/* ID of the QEnsemble new angle parameter used by the
 * #ADM_MODULE_ID_QENSEMBLE module.
 * @messagepayload
 * @structure{adm_qensemble_param_set_new_angle}
 * @tablespace
 * @inputtable{Audio_Postproc_ADM_PARAM_ID_QENSEMBLE_SET_NEW_ANGLE.tex}
 */
#define ADM_PARAM_ID_QENSEMBLE_SET_NEW_ANGLE    0x00010C62

/* Structure for enabling the configuration parameter for the
 * QEnsemble module.
 */


/* @brief Payload of the #ADM_PARAM_ID_QENSEMBLE_ENABLE
 * parameter used by the QEnsemble module.
 */
struct adm_qensemble_enable {
	uint32_t                  enable_flag;
/*< Specifies whether the QEnsemble module is disabled (0) or enabled
 * (nonzero).
 */
} __packed;


/* Structure for the background gain for the QEnsemble module. */


/* @brief Payload of the #ADM_PARAM_ID_QENSEMBLE_BACKGAIN
 * parameter used by
 * the QEnsemble module.
 */
struct adm_qensemble_param_backgain {
	int16_t                  back_gain;
/*< Linear gain in Q15 format.
 * Supported values: 0 to 32767
 */

	uint16_t                 reserved;
	/*< Clients must set this field to zero.*/
} __packed;
/* Structure for setting a new angle for the QEnsemble module. */


/* @brief Payload of the #ADM_PARAM_ID_QENSEMBLE_SET_NEW_ANGLE
 * parameter used
 * by the QEnsemble module.
 */
struct adm_qensemble_param_set_new_angle {
	int16_t                    new_angle;
/*< New angle in degrees.
 * Supported values: 0 to 359
 */

	int16_t                    time_ms;
/*< Transition time in milliseconds to set the new angle.
 * Supported values: 0 to 32767
 */
} __packed;

/* end_addtogroup audio_pp_module_ids */

/* @ingroup audio_pp_module_ids
 * ID of the Volume Control module pre/postprocessing block.
 * This module supports the following parameter IDs:
 * - #ASM_PARAM_ID_VOL_CTRL_MASTER_GAIN
 * - #ASM_PARAM_ID_VOL_CTRL_LR_CHANNEL_GAIN
 * - #ASM_PARAM_ID_VOL_CTRL_MUTE_CONFIG
 * - #ASM_PARAM_ID_SOFT_VOL_STEPPING_PARAMETERS
 * - #ASM_PARAM_ID_SOFT_PAUSE_PARAMETERS
 * - #ASM_PARAM_ID_MULTICHANNEL_GAIN
 * - #ASM_PARAM_ID_MULTICHANNEL_MUTE
 */
#define ASM_MODULE_ID_VOL_CTRL   0x00010BFE

/* @addtogroup audio_pp_param_ids */
/* ID of the master gain parameter used by the #ASM_MODULE_ID_VOL_CTRL
 * module.
 * @messagepayload
 * @structure{asm_volume_ctrl_master_gain}
 * @tablespace
 * @inputtable{Audio_Postproc_ASM_PARAM_ID_VOL_CTRL_MASTER_GAIN.tex}
 */
#define ASM_PARAM_ID_VOL_CTRL_MASTER_GAIN    0x00010BFF

/* ID of the left/right channel gain parameter used by the
 * #ASM_MODULE_ID_VOL_CTRL module.
 * @messagepayload
 * @structure{asm_volume_ctrl_lr_chan_gain}
 * @tablespace
 * @inputtable{Audio_Postproc_ASM_PARAM_ID_VOL_CTRL_LR_CHANNEL_GAIN.tex}
 */
#define ASM_PARAM_ID_VOL_CTRL_LR_CHANNEL_GAIN     0x00010C00

/* ID of the mute configuration parameter used by the
 * #ASM_MODULE_ID_VOL_CTRL module.
 * @messagepayload
 * @structure{asm_volume_ctrl_mute_config}
 * @tablespace
 * @inputtable{Audio_Postproc_ASM_PARAM_ID_VOL_CTRL_MUTE_CONFIG.tex}
 */
#define ASM_PARAM_ID_VOL_CTRL_MUTE_CONFIG   0x00010C01

/* ID of the soft stepping volume parameters used by the
 * #ASM_MODULE_ID_VOL_CTRL module.
 * @messagepayload
 * @structure{asm_soft_step_volume_params}
 * @tablespace
 * @inputtable{Audio_Postproc_ASM_PARAM_ID_SOFT_VOL_STEPPING_PARAMET
 * ERS.tex}
 */
#define ASM_PARAM_ID_SOFT_VOL_STEPPING_PARAMETERS  0x00010C29

/* ID of the soft pause parameters used by the #ASM_MODULE_ID_VOL_CTRL
 * module.
 */
#define ASM_PARAM_ID_SOFT_PAUSE_PARAMETERS   0x00010D6A

/* ID of the multiple-channel volume control parameters used by the
 * #ASM_MODULE_ID_VOL_CTRL module.
 */
#define ASM_PARAM_ID_MULTICHANNEL_GAIN  0x00010713

/* ID of the multiple-channel mute configuration parameters used by the
 * #ASM_MODULE_ID_VOL_CTRL module.
 */

#define ASM_PARAM_ID_MULTICHANNEL_MUTE  0x00010714

/* Structure for the master gain parameter for a volume control
 * module.
 */


/* @brief Payload of the #ASM_PARAM_ID_VOL_CTRL_MASTER_GAIN
 * parameter used by the Volume Control module.
 */



struct asm_volume_ctrl_master_gain {
	struct apr_hdr	hdr;
	struct asm_stream_cmd_set_pp_params_v2 param;
	struct asm_stream_param_data_v2 data;
	uint16_t                  master_gain;
	/*< Linear gain in Q13 format. */

	uint16_t                  reserved;
	/*< Clients must set this field to zero.
		*/
} __packed;


/* Structure for the left/right channel gain parameter for a
 * volume control module.
 */


/* @brief Payload of the #ASM_PARAM_ID_VOL_CTRL_LR_CHANNEL_GAIN
 * parameters used by the Volume Control module.
 */



struct asm_volume_ctrl_lr_chan_gain {
	struct apr_hdr	hdr;
	struct asm_stream_cmd_set_pp_params_v2 param;
	struct asm_stream_param_data_v2 data;

	uint16_t                  l_chan_gain;
	/*< Linear gain in Q13 format for the left channel. */

	uint16_t                  r_chan_gain;
	/*< Linear gain in Q13 format for the right channel.*/
} __packed;


/* Structure for the mute configuration parameter for a
	volume control module. */


/* @brief Payload of the #ASM_PARAM_ID_VOL_CTRL_MUTE_CONFIG
 * parameter used by the Volume Control module.
 */


struct asm_volume_ctrl_mute_config {
	struct apr_hdr	hdr;
	struct asm_stream_cmd_set_pp_params_v2 param;
	struct asm_stream_param_data_v2 data;
	uint32_t                  mute_flag;
/*< Specifies whether mute is disabled (0) or enabled (nonzero).*/

} __packed;

/*
 * Supported parameters for a soft stepping linear ramping curve.
 */
#define ASM_PARAM_SVC_RAMPINGCURVE_LINEAR  0

/*
 * Exponential ramping curve.
 */
#define ASM_PARAM_SVC_RAMPINGCURVE_EXP    1

/*
 * Logarithmic ramping curve.
 */
#define ASM_PARAM_SVC_RAMPINGCURVE_LOG    2

/* Structure for holding soft stepping volume parameters. */


/*  Payload of the #ASM_PARAM_ID_SOFT_VOL_STEPPING_PARAMETERS
 * parameters used by the Volume Control module.
 */
struct asm_soft_step_volume_params {
	struct apr_hdr	hdr;
	struct asm_stream_cmd_set_pp_params_v2 param;
	struct asm_stream_param_data_v2 data;
	uint32_t                  period;
/*< Period in milliseconds.
 * Supported values: 0 to 15000
 */

	uint32_t                  step;
/*< Step in microseconds.
 * Supported values: 0 to 15000000
 */

	uint32_t                  ramping_curve;
/*< Ramping curve type.
 * Supported values:
 * - #ASM_PARAM_SVC_RAMPINGCURVE_LINEAR
 * - #ASM_PARAM_SVC_RAMPINGCURVE_EXP
 * - #ASM_PARAM_SVC_RAMPINGCURVE_LOG
 */
} __packed;


/* Structure for holding soft pause parameters. */


/* Payload of the #ASM_PARAM_ID_SOFT_PAUSE_PARAMETERS
 * parameters used by the Volume Control module.
 */


struct asm_soft_pause_params {
	struct apr_hdr	hdr;
	struct asm_stream_cmd_set_pp_params_v2 param;
	struct asm_stream_param_data_v2 data;
	uint32_t                  enable_flag;
/*< Specifies whether soft pause is disabled (0) or enabled
 * (nonzero).
 */



	uint32_t                  period;
/*< Period in milliseconds.
 * Supported values: 0 to 15000
 */

	uint32_t                  step;
/*< Step in microseconds.
 * Supported values: 0 to 15000000
 */

	uint32_t                  ramping_curve;
/*< Ramping curve.
 * Supported values:
 * - #ASM_PARAM_SVC_RAMPINGCURVE_LINEAR
 * - #ASM_PARAM_SVC_RAMPINGCURVE_EXP
 * - #ASM_PARAM_SVC_RAMPINGCURVE_LOG
 */
} __packed;


/* Maximum number of channels.*/
#define VOLUME_CONTROL_MAX_CHANNELS                       8

/* Structure for holding one channel type - gain pair. */


/* Payload of the #ASM_PARAM_ID_MULTICHANNEL_GAIN channel
 * type/gain pairs used by the Volume Control module. \n \n This
 * structure immediately follows the
 * asm_volume_ctrl_multichannel_gain structure.
 */


struct asm_volume_ctrl_channelype_gain_pair {
	struct apr_hdr	hdr;
	struct asm_stream_cmd_set_pp_params_v2 param;
	struct asm_stream_param_data_v2 data;
	uint8_t                   channelype;
/*< Channel type for which the gain setting is to be applied.
 * Supported values:
 * - #PCM_CHANNEL_L
 * - #PCM_CHANNEL_R
 * - #PCM_CHANNEL_C
 * - #PCM_CHANNEL_LS
 * - #PCM_CHANNEL_RS
 * - #PCM_CHANNEL_LFE
 * - #PCM_CHANNEL_CS
 * - #PCM_CHANNEL_LB
 * - #PCM_CHANNEL_RB
 * - #PCM_CHANNELS
 * - #PCM_CHANNEL_CVH
 * - #PCM_CHANNEL_MS
 * - #PCM_CHANNEL_FLC
 * - #PCM_CHANNEL_FRC
 * - #PCM_CHANNEL_RLC
 * - #PCM_CHANNEL_RRC
 */

	uint8_t                   reserved1;
	/*< Clients must set this field to zero. */

	uint8_t                   reserved2;
	/*< Clients must set this field to zero. */

	uint8_t                   reserved3;
	/*< Clients must set this field to zero. */

	uint32_t                  gain;
/*< Gain value for this channel in Q28 format.
 * Supported values: Any
 */
} __packed;


/* Structure for the multichannel gain command */


/* Payload of the #ASM_PARAM_ID_MULTICHANNEL_GAIN
 * parameters used by the Volume Control module.
 */


struct asm_volume_ctrl_multichannel_gain {
	struct apr_hdr	hdr;
	struct asm_stream_cmd_set_pp_params_v2 param;
	struct asm_stream_param_data_v2 data;
	uint32_t                  num_channels;
/*< Number of channels for which gain values are provided. Any
 * channels present in the data for which gain is not provided are
 * set to unity gain.
 * Supported values: 1 to 8
 */


	struct asm_volume_ctrl_channelype_gain_pair
		gain_data[VOLUME_CONTROL_MAX_CHANNELS];
	/*< Array of channel type/gain pairs.*/
} __packed;


/* Structure for holding one channel type - mute pair. */


/* Payload of the #ASM_PARAM_ID_MULTICHANNEL_MUTE channel
 * type/mute setting pairs used by the Volume Control module. \n \n
 * This structure immediately follows the
 * asm_volume_ctrl_multichannel_mute structure.
 */


struct asm_volume_ctrl_channelype_mute_pair {
	struct apr_hdr	hdr;
	struct asm_stream_cmd_set_pp_params_v2 param;
	struct asm_stream_param_data_v2 data;
	uint8_t                   channelype;
/*< Channel type for which the mute setting is to be applied.
 * Supported values:
 * - #PCM_CHANNEL_L
 * - #PCM_CHANNEL_R
 * - #PCM_CHANNEL_C
 * - #PCM_CHANNEL_LS
 * - #PCM_CHANNEL_RS
 * - #PCM_CHANNEL_LFE
 * - #PCM_CHANNEL_CS
 * - #PCM_CHANNEL_LB
 * - #PCM_CHANNEL_RB
 * - #PCM_CHANNELS
 * - #PCM_CHANNEL_CVH
 * - #PCM_CHANNEL_MS
 * - #PCM_CHANNEL_FLC
 * - #PCM_CHANNEL_FRC
 * - #PCM_CHANNEL_RLC
 * - #PCM_CHANNEL_RRC
 */

	uint8_t                   reserved1;
	/*< Clients must set this field to zero. */

	uint8_t                   reserved2;
	/*< Clients must set this field to zero. */

	uint8_t                   reserved3;
	/*< Clients must set this field to zero. */

	uint32_t                  mute;
/*< Mute setting for this channel.
 * Supported values:
 * - 0 = Unmute
 * - Nonzero = Mute
 */
} __packed;


/* Structure for the multichannel mute command */


/* @brief Payload of the #ASM_PARAM_ID_MULTICHANNEL_MUTE
 * parameters used by the Volume Control module.
 */


struct asm_volume_ctrl_multichannel_mute {
	struct apr_hdr	hdr;
	struct asm_stream_cmd_set_pp_params_v2 param;
	struct asm_stream_param_data_v2 data;
	uint32_t                  num_channels;
/*< Number of channels for which mute configuration is
 * provided. Any channels present in the data for which mute
 * configuration is not provided are set to unmute.
 * Supported values: 1 to 8
 */

struct asm_volume_ctrl_channelype_mute_pair
				mute_data[VOLUME_CONTROL_MAX_CHANNELS];
	/*< Array of channel type/mute setting pairs.*/
} __packed;
/* end_addtogroup audio_pp_param_ids */

/* audio_pp_module_ids
 * ID of the IIR Tuning Filter module.
 * This module supports the following parameter IDs:
 * - #ASM_PARAM_ID_IIRUNING_FILTER_ENABLE_CONFIG
 * - #ASM_PARAM_ID_IIRUNING_FILTER_PRE_GAIN
 * - #ASM_PARAM_ID_IIRUNING_FILTER_CONFIG_PARAMS
 */
#define ASM_MODULE_ID_IIRUNING_FILTER   0x00010C02

/* @addtogroup audio_pp_param_ids */
/* ID of the IIR tuning filter enable parameter used by the
 * #ASM_MODULE_ID_IIRUNING_FILTER module.
 * @messagepayload
 * @structure{asm_iiruning_filter_enable}
 * @tablespace
 * @inputtable{Audio_Postproc_ASM_PARAM_ID_IIRUNING_FILTER_ENABLE_CO
 * NFIG.tex}
 */
#define ASM_PARAM_ID_IIRUNING_FILTER_ENABLE_CONFIG   0x00010C03

/* ID of the IIR tuning filter pregain parameter used by the
 * #ASM_MODULE_ID_IIRUNING_FILTER module.
 */
#define ASM_PARAM_ID_IIRUNING_FILTER_PRE_GAIN  0x00010C04

/* ID of the IIR tuning filter configuration parameters used by the
 * #ASM_MODULE_ID_IIRUNING_FILTER module.
 */
#define ASM_PARAM_ID_IIRUNING_FILTER_CONFIG_PARAMS  0x00010C05

/* Structure for an enable configuration parameter for an
 * IIR tuning filter module.
 */


/* @brief Payload of the #ASM_PARAM_ID_IIRUNING_FILTER_ENABLE_CONFIG
 * parameter used by the IIR Tuning Filter module.
 */
struct asm_iiruning_filter_enable {
	uint32_t                  enable_flag;
/*< Specifies whether the IIR tuning filter is disabled (0) or
 * enabled (1).
 */
} __packed;

/* Structure for the pregain parameter for an IIR tuning filter module. */


/* Payload of the #ASM_PARAM_ID_IIRUNING_FILTER_PRE_GAIN
 * parameters used by the IIR Tuning Filter module.
 */
struct asm_iiruning_filter_pregain {
	uint16_t                  pregain;
	/*< Linear gain in Q13 format. */

	uint16_t                  reserved;
	/*< Clients must set this field to zero.*/
} __packed;

/* Structure for the configuration parameter for an IIR tuning filter
 * module.
 */


/* @brief Payload of the #ASM_PARAM_ID_IIRUNING_FILTER_CONFIG_PARAMS
 * parameters used by the IIR Tuning Filter module. \n
 * \n
 * This structure is followed by the IIR filter coefficients: \n
 * - Sequence of int32_t FilterCoeffs \n
 * Five coefficients for each band. Each coefficient is in int32_t format, in
 * the order of b0, b1, b2, a1, a2.
 * - Sequence of int16_t NumShiftFactor \n
 * One int16_t per band. The numerator shift factor is related to the Q
 * factor of the filter coefficients.
 * - Sequence of uint16_t PanSetting \n
 * One uint16_t per band, indicating if the filter is applied to left (0),
 * right (1), or both (2) channels.
 */
struct asm_iir_filter_config_params {
	uint16_t                  num_biquad_stages;
/*< Number of bands.
 * Supported values: 0 to 20
 */

	uint16_t                  reserved;
	/*< Clients must set this field to zero.*/
} __packed;

/* audio_pp_module_ids
 * ID of the Multiband Dynamic Range Control (MBDRC) module on the Tx/Rx
 * paths.
 * This module supports the following parameter IDs:
 * - #ASM_PARAM_ID_MBDRC_ENABLE
 * - #ASM_PARAM_ID_MBDRC_CONFIG_PARAMS
 */
#define ASM_MODULE_ID_MBDRC   0x00010C06

/* audio_pp_param_ids */
/* ID of the MBDRC enable parameter used by the #ASM_MODULE_ID_MBDRC module.
 * @messagepayload
 * @structure{asm_mbdrc_enable}
 * @tablespace
 * @inputtable{Audio_Postproc_ASM_PARAM_ID_MBDRC_ENABLE.tex}
 */
#define ASM_PARAM_ID_MBDRC_ENABLE   0x00010C07

/* ID of the MBDRC configuration parameters used by the
 * #ASM_MODULE_ID_MBDRC module.
 * @messagepayload
 * @structure{asm_mbdrc_config_params}
 * @tablespace
 * @inputtable{Audio_Postproc_ASM_PARAM_ID_MBDRC_CONFIG_PARAMS.tex}
 *
 * @parspace Sub-band DRC configuration parameters
 * @structure{asm_subband_drc_config_params}
 * @tablespace
 * @inputtable{Audio_Postproc_ASM_PARAM_ID_MBDRC_CONFIG_PARAMS_subband_DRC.tex}
 *
 * @keep{6}
 * To obtain legacy ADRC from MBDRC, use the calibration tool to:
 *
 * - Enable MBDRC (EnableFlag = TRUE)
 * - Set number of bands to 1 (uiNumBands = 1)
 * - Enable the first MBDRC band (DrcMode[0] = DRC_ENABLED = 1)
 * - Clear the first band mute flag (MuteFlag[0] = 0)
 * - Set the first band makeup gain to unity (compMakeUpGain[0] = 0x2000)
 * - Use the legacy ADRC parameters to calibrate the rest of the MBDRC
 * parameters.
 */
#define ASM_PARAM_ID_MBDRC_CONFIG_PARAMS  0x00010C08

/* end_addtogroup audio_pp_param_ids */

/* audio_pp_module_ids
 * ID of the MMBDRC module version 2 pre/postprocessing block.
 * This module differs from the original MBDRC (#ASM_MODULE_ID_MBDRC) in
 * the length of the filters used in each sub-band.
 * This module supports the following parameter ID:
 * - #ASM_PARAM_ID_MBDRC_CONFIG_PARAMS_IMPROVED_FILTBANK_V2
 */
#define ASM_MODULE_ID_MBDRCV2                                0x0001070B

/* @addtogroup audio_pp_param_ids */
/* ID of the configuration parameters used by the
 * #ASM_MODULE_ID_MBDRCV2 module for the improved filter structure
 * of the MBDRC v2 pre/postprocessing block.
 * The update to this configuration structure from the original
 * MBDRC is the number of filter coefficients in the filter
 * structure. The sequence for is as follows:
 * - 1 band = 0 FIR coefficient + 1 mute flag + uint16_t padding
 * - 2 bands = 141 FIR coefficients + 2 mute flags + uint16_t padding
 * - 3 bands = 141+81 FIR coefficients + 3 mute flags + uint16_t padding
 * - 4 bands = 141+81+61 FIR coefficients + 4 mute flags + uint16_t
 * padding
 * - 5 bands = 141+81+61+61 FIR coefficients + 5 mute flags +
 * uint16_t padding
 *	This block uses the same parameter structure as
 *	#ASM_PARAM_ID_MBDRC_CONFIG_PARAMS.
 */
#define ASM_PARAM_ID_MBDRC_CONFIG_PARAMS_IMPROVED_FILTBANK_V2 \
								0x0001070C

/* Structure for the enable parameter for an MBDRC module. */


/* Payload of the #ASM_PARAM_ID_MBDRC_ENABLE parameter used by the
 * MBDRC module.
 */
struct asm_mbdrc_enable {
	uint32_t                  enable_flag;
/*< Specifies whether MBDRC is disabled (0) or enabled (nonzero).*/
} __packed;

/* Structure for the configuration parameters for an MBDRC module. */


/* Payload of the #ASM_PARAM_ID_MBDRC_CONFIG_PARAMS
 * parameters used by the MBDRC module. \n \n Following this
 * structure is the payload for sub-band DRC configuration
 * parameters (asm_subband_drc_config_params). This sub-band
 * structure must be repeated for each band.
 */


struct asm_mbdrc_config_params {
	uint16_t                  num_bands;
/*< Number of bands.
 * Supported values: 1 to 5
 */

	int16_t                   limiterhreshold;
/*< Threshold in decibels for the limiter output.
 * Supported values: -72 to 18 \n
 * Recommended value: 3994 (-0.22 db in Q3.12 format)
 */

	int16_t                   limiter_makeup_gain;
/*< Makeup gain in decibels for the limiter output.
 * Supported values: -42 to 42 \n
 * Recommended value: 256 (0 dB in Q7.8 format)
 */

	int16_t                   limiter_gc;
/*< Limiter gain recovery coefficient.
 * Supported values: 0.5 to 0.99 \n
 * Recommended value: 32440 (0.99 in Q15 format)
 */

	int16_t                   limiter_delay;
/*< Limiter delay in samples.
 * Supported values: 0 to 10 \n
 * Recommended value: 262 (0.008 samples in Q15 format)
 */

	int16_t                   limiter_max_wait;
/*< Maximum limiter waiting time in samples.
 * Supported values: 0 to 10 \n
 * Recommended value: 262 (0.008 samples in Q15 format)
 */
} __packed;

/* DRC configuration structure for each sub-band of an MBDRC module. */


/* Payload of the #ASM_PARAM_ID_MBDRC_CONFIG_PARAMS DRC
 * configuration parameters for each sub-band in the MBDRC module.
 * After this DRC structure is configured for valid bands, the next
 * MBDRC setparams expects the sequence of sub-band MBDRC filter
 * coefficients (the length depends on the number of bands) plus the
 * mute flag for that band plus uint16_t padding.
 *
 * @keep{10}
 * The filter coefficient and mute flag are of type int16_t:
 * - FIR coefficient = int16_t firFilter
 * - Mute flag = int16_t fMuteFlag
 *
 * The sequence is as follows:
 * - 1 band = 0 FIR coefficient + 1 mute flag + uint16_t padding
 * - 2 bands = 97 FIR coefficients + 2 mute flags + uint16_t padding
 * - 3 bands = 97+33 FIR coefficients + 3 mute flags + uint16_t padding
 * - 4 bands = 97+33+33 FIR coefficients + 4 mute flags + uint16_t padding
 * - 5 bands = 97+33+33+33 FIR coefficients + 5 mute flags + uint16_t padding
 *
 * For improved filterbank, the sequence is as follows:
 * - 1 band = 0 FIR coefficient + 1 mute flag + uint16_t padding
 * - 2 bands = 141 FIR coefficients + 2 mute flags + uint16_t padding
 * - 3 bands = 141+81 FIR coefficients + 3 mute flags + uint16_t padding
 * - 4 bands = 141+81+61 FIR coefficients + 4 mute flags + uint16_t padding
 * - 5 bands = 141+81+61+61 FIR coefficients + 5 mute flags + uint16_t padding
 */
struct asm_subband_drc_config_params {
	int16_t                   drc_stereo_linked_flag;
/*< Specifies whether all stereo channels have the same applied
 * dynamics (1) or if they process their dynamics independently (0).
 * Supported values:
 * - 0 -- Not linked
 * - 1 -- Linked
 */

	int16_t                   drc_mode;
/*< Specifies whether DRC mode is bypassed for sub-bands.
 * Supported values:
 * - 0 -- Disabled
 * - 1 -- Enabled
 */

	int16_t                   drc_down_sample_level;
/*< DRC down sample level.
 * Supported values: @ge 1
 */

	int16_t                   drc_delay;
/*< DRC delay in samples.
 * Supported values: 0 to 1200
 */

	uint16_t                  drc_rmsime_avg_const;
/*< RMS signal energy time-averaging constant.
 * Supported values: 0 to 2^16-1
 */

	uint16_t                  drc_makeup_gain;
/*< DRC makeup gain in decibels.
 * Supported values: 258 to 64917
 */
	/* Down expander settings */
	int16_t                   down_expdrhreshold;
/*< Down expander threshold.
 * Supported Q7 format values: 1320 to up_cmpsrhreshold
 */

	int16_t                   down_expdr_slope;
/*< Down expander slope.
 * Supported Q8 format values: -32768 to 0.
 */

	uint32_t                  down_expdr_attack;
/*< Down expander attack constant.
 * Supported Q31 format values: 196844 to 2^31.
 */

	uint32_t                  down_expdr_release;
/*< Down expander release constant.
 * Supported Q31 format values: 19685 to 2^31
 */

	uint16_t                  down_expdr_hysteresis;
/*< Down expander hysteresis constant.
 * Supported Q14 format values: 1 to 32690
 */

	uint16_t                  reserved;
	/*< Clients must set this field to zero. */

	int32_t                   down_expdr_min_gain_db;
/*< Down expander minimum gain.
 * Supported Q23 format values: -805306368 to 0.
 */

	/* Up compressor settings */

	int16_t                   up_cmpsrhreshold;
/*< Up compressor threshold.
 * Supported Q7 format values: down_expdrhreshold to
 * down_cmpsrhreshold.
 */

	uint16_t                  up_cmpsr_slope;
/*< Up compressor slope.
 * Supported Q16 format values: 0 to 64881.
 */

	uint32_t                  up_cmpsr_attack;
/*< Up compressor attack constant.
 * Supported Q31 format values: 196844 to 2^31.
 */

	uint32_t                  up_cmpsr_release;
/*< Up compressor release constant.
 * Supported Q31 format values: 19685 to 2^31.
 */

	uint16_t                  up_cmpsr_hysteresis;
/*< Up compressor hysteresis constant.
  * Supported Q14 format values: 1 to 32690.
  */

	/* Down compressor settings */

	int16_t                   down_cmpsrhreshold;
/*< Down compressor threshold.
 * Supported Q7 format values: up_cmpsrhreshold to 11560.
 */

	uint16_t                  down_cmpsr_slope;
/*< Down compressor slope.
 * Supported Q16 format values: 0 to 64881.
 */

	uint16_t                  reserved1;
/*< Clients must set this field to zero. */

	uint32_t                  down_cmpsr_attack;
/*< Down compressor attack constant.
 * Supported Q31 format values: 196844 to 2^31.
 */

	uint32_t                  down_cmpsr_release;
/*< Down compressor release constant.
 * Supported Q31 format values: 19685 to 2^31.
 */

	uint16_t                  down_cmpsr_hysteresis;
/*< Down compressor hysteresis constant.
 * Supported Q14 values: 1 to 32690.
 */

	uint16_t                  reserved2;
/*< Clients must set this field to zero.*/
} __packed;

#define ASM_MODULE_ID_EQUALIZER            0x00010C27
#define ASM_PARAM_ID_EQUALIZER_PARAMETERS  0x00010C28

#define ASM_MAX_EQ_BANDS 12

struct asm_eq_per_band_params {
	uint32_t                  band_idx;
/*< Band index.
 * Supported values: 0 to 11
 */

	uint32_t                  filterype;
/*< Type of filter.
 * Supported values:
 * - #ASM_PARAM_EQYPE_NONE
 * - #ASM_PARAM_EQ_BASS_BOOST
 * - #ASM_PARAM_EQ_BASS_CUT
 * - #ASM_PARAM_EQREBLE_BOOST
 * - #ASM_PARAM_EQREBLE_CUT
 * - #ASM_PARAM_EQ_BAND_BOOST
 * - #ASM_PARAM_EQ_BAND_CUT
 */

	uint32_t                  center_freq_hz;
	/*< Filter band center frequency in Hertz. */

	int32_t                   filter_gain;
/*< Filter band initial gain.
 * Supported values: +12 to -12 dB in 1 dB increments
 */

	int32_t                   q_factor;
/*< Filter band quality factor expressed as a Q8 number, i.e., a
 * fixed-point number with q factor of 8. For example, 3000/(2^8).
 */
} __packed;

struct asm_eq_params {
	struct apr_hdr	hdr;
	struct asm_stream_cmd_set_pp_params_v2 param;
	struct asm_stream_param_data_v2 data;
		uint32_t                  enable_flag;
/*< Specifies whether the equalizer module is disabled (0) or enabled
 * (nonzero).
 */

		uint32_t                  num_bands;
/*< Number of bands.
 * Supported values: 1 to 12
 */
	struct asm_eq_per_band_params eq_bands[ASM_MAX_EQ_BANDS];

} __packed;

/*	No equalizer effect.*/
#define ASM_PARAM_EQYPE_NONE      0

/*	Bass boost equalizer effect.*/
#define ASM_PARAM_EQ_BASS_BOOST     1

/*Bass cut equalizer effect.*/
#define ASM_PARAM_EQ_BASS_CUT       2

/*	Treble boost equalizer effect */
#define ASM_PARAM_EQREBLE_BOOST   3

/*	Treble cut equalizer effect.*/
#define ASM_PARAM_EQREBLE_CUT     4

/*	Band boost equalizer effect.*/
#define ASM_PARAM_EQ_BAND_BOOST     5

/*	Band cut equalizer effect.*/
#define ASM_PARAM_EQ_BAND_CUT       6

/* Voice get & set params */
#define VOICE_CMD_SET_PARAM				0x0001133D
#define VOICE_CMD_GET_PARAM				0x0001133E
#define VOICE_EVT_GET_PARAM_ACK				0x00011008


/** ID of the Bass Boost module.
    This module supports the following parameter IDs:
    - #AUDPROC_PARAM_ID_BASS_BOOST_ENABLE
    - #AUDPROC_PARAM_ID_BASS_BOOST_MODE
    - #AUDPROC_PARAM_ID_BASS_BOOST_STRENGTH
*/
#define AUDPROC_MODULE_ID_BASS_BOOST                             0x000108A1
/** ID of the Bass Boost enable parameter used by
    AUDPROC_MODULE_ID_BASS_BOOST.
*/
#define AUDPROC_PARAM_ID_BASS_BOOST_ENABLE                       0x000108A2
/** ID of the Bass Boost mode parameter used by
    AUDPROC_MODULE_ID_BASS_BOOST.
*/
#define AUDPROC_PARAM_ID_BASS_BOOST_MODE                         0x000108A3
/** ID of the Bass Boost strength parameter used by
    AUDPROC_MODULE_ID_BASS_BOOST.
*/
#define AUDPROC_PARAM_ID_BASS_BOOST_STRENGTH                     0x000108A4

/** ID of the Virtualizer module. This module supports the
    following parameter IDs:
    - #AUDPROC_PARAM_ID_VIRTUALIZER_ENABLE
    - #AUDPROC_PARAM_ID_VIRTUALIZER_STRENGTH
    - #AUDPROC_PARAM_ID_VIRTUALIZER_OUT_TYPE
    - #AUDPROC_PARAM_ID_VIRTUALIZER_GAIN_ADJUST
*/
#define AUDPROC_MODULE_ID_VIRTUALIZER                            0x000108A5
/** ID of the Virtualizer enable parameter used by
    AUDPROC_MODULE_ID_VIRTUALIZER.
*/
#define AUDPROC_PARAM_ID_VIRTUALIZER_ENABLE                      0x000108A6
/** ID of the Virtualizer strength parameter used by
    AUDPROC_MODULE_ID_VIRTUALIZER.
*/
#define AUDPROC_PARAM_ID_VIRTUALIZER_STRENGTH                    0x000108A7
/** ID of the Virtualizer out type parameter used by
    AUDPROC_MODULE_ID_VIRTUALIZER.
*/
#define AUDPROC_PARAM_ID_VIRTUALIZER_OUT_TYPE                    0x000108A8
/** ID of the Virtualizer out type parameter used by
    AUDPROC_MODULE_ID_VIRTUALIZER.
*/
#define AUDPROC_PARAM_ID_VIRTUALIZER_GAIN_ADJUST                 0x000108A9

/** ID of the Reverb module. This module supports the following
    parameter IDs:
    - #AUDPROC_PARAM_ID_REVERB_ENABLE
    - #AUDPROC_PARAM_ID_REVERB_MODE
    - #AUDPROC_PARAM_ID_REVERB_PRESET
    - #AUDPROC_PARAM_ID_REVERB_WET_MIX
    - #AUDPROC_PARAM_ID_REVERB_GAIN_ADJUST
    - #AUDPROC_PARAM_ID_REVERB_ROOM_LEVEL
    - #AUDPROC_PARAM_ID_REVERB_ROOM_HF_LEVEL
    - #AUDPROC_PARAM_ID_REVERB_DECAY_TIME
    - #AUDPROC_PARAM_ID_REVERB_DECAY_HF_RATIO
    - #AUDPROC_PARAM_ID_REVERB_REFLECTIONS_LEVEL
    - #AUDPROC_PARAM_ID_REVERB_REFLECTIONS_DELAY
    - #AUDPROC_PARAM_ID_REVERB_LEVEL
    - #AUDPROC_PARAM_ID_REVERB_DELAY
    - #AUDPROC_PARAM_ID_REVERB_DIFFUSION
    - #AUDPROC_PARAM_ID_REVERB_DENSITY
*/
#define AUDPROC_MODULE_ID_REVERB                          0x000108AA
/** ID of the Reverb enable parameter used by
    AUDPROC_MODULE_ID_REVERB.
*/
#define AUDPROC_PARAM_ID_REVERB_ENABLE                    0x000108AB
/** ID of the Reverb mode parameter used by
    AUDPROC_MODULE_ID_REVERB.
*/
#define AUDPROC_PARAM_ID_REVERB_MODE                      0x000108AC
/** ID of the Reverb preset parameter used by
    AUDPROC_MODULE_ID_REVERB.
*/
#define AUDPROC_PARAM_ID_REVERB_PRESET                    0x000108AD
/** ID of the Reverb wet mix parameter used by
    AUDPROC_MODULE_ID_REVERB.
*/
#define AUDPROC_PARAM_ID_REVERB_WET_MIX                   0x000108AE
/** ID of the Reverb gain adjust parameter used by
    AUDPROC_MODULE_ID_REVERB.
*/
#define AUDPROC_PARAM_ID_REVERB_GAIN_ADJUST               0x000108AF
/** ID of the Reverb room level parameter used by
    AUDPROC_MODULE_ID_REVERB.
*/
#define AUDPROC_PARAM_ID_REVERB_ROOM_LEVEL                0x000108B0
/** ID of the Reverb room hf level parameter used by
    AUDPROC_MODULE_ID_REVERB.
*/
#define AUDPROC_PARAM_ID_REVERB_ROOM_HF_LEVEL             0x000108B1
/** ID of the Reverb decay time parameter used by
    AUDPROC_MODULE_ID_REVERB.
*/
#define AUDPROC_PARAM_ID_REVERB_DECAY_TIME                0x000108B2
/** ID of the Reverb decay hf ratio parameter used by
    AUDPROC_MODULE_ID_REVERB.
*/
#define AUDPROC_PARAM_ID_REVERB_DECAY_HF_RATIO            0x000108B3
/** ID of the Reverb reflections level parameter used by
    AUDPROC_MODULE_ID_REVERB.
*/
#define AUDPROC_PARAM_ID_REVERB_REFLECTIONS_LEVEL         0x000108B4
/** ID of the Reverb reflections delay parameter used by
    AUDPROC_MODULE_ID_REVERB.
*/
#define AUDPROC_PARAM_ID_REVERB_REFLECTIONS_DELAY         0x000108B5
/** ID of the Reverb level parameter used by
    AUDPROC_MODULE_ID_REVERB.
*/
#define AUDPROC_PARAM_ID_REVERB_LEVEL                      0x000108B6
/** ID of the Reverb delay parameter used by
    AUDPROC_MODULE_ID_REVERB.
*/
#define AUDPROC_PARAM_ID_REVERB_DELAY                      0x000108B7
/** ID of the Reverb diffusion parameter used by
    AUDPROC_MODULE_ID_REVERB.
*/
#define AUDPROC_PARAM_ID_REVERB_DIFFUSION                  0x000108B8
/** ID of the Reverb density parameter used by
    AUDPROC_MODULE_ID_REVERB.
*/
#define AUDPROC_PARAM_ID_REVERB_DENSITY                    0x000108B9

/** ID of the Popless Equalizer module. This module supports the
    following parameter IDs:
    - #AUDPROC_PARAM_ID_EQ_ENABLE
    - #AUDPROC_PARAM_ID_EQ_CONFIG
    - #AUDPROC_PARAM_ID_EQ_NUM_BANDS
    - #AUDPROC_PARAM_ID_EQ_BAND_LEVELS
    - #AUDPROC_PARAM_ID_EQ_BAND_LEVEL_RANGE
    - #AUDPROC_PARAM_ID_EQ_BAND_FREQS
    - #AUDPROC_PARAM_ID_EQ_SINGLE_BAND_FREQ_RANGE
    - #AUDPROC_PARAM_ID_EQ_SINGLE_BAND_FREQ
    - #AUDPROC_PARAM_ID_EQ_BAND_INDEX
    - #AUDPROC_PARAM_ID_EQ_PRESET_ID
    - #AUDPROC_PARAM_ID_EQ_NUM_PRESETS
    - #AUDPROC_PARAM_ID_EQ_GET_PRESET_NAME
*/
#define AUDPROC_MODULE_ID_POPLESS_EQUALIZER                    0x000108BA
/** ID of the Popless Equalizer enable parameter used by
    AUDPROC_MODULE_ID_POPLESS_EQUALIZER.
*/
#define AUDPROC_PARAM_ID_EQ_ENABLE                             0x000108BB
/** ID of the Popless Equalizer config parameter used by
    AUDPROC_MODULE_ID_POPLESS_EQUALIZER.
*/
#define AUDPROC_PARAM_ID_EQ_CONFIG                             0x000108BC
/** ID of the Popless Equalizer number of bands parameter used
    by AUDPROC_MODULE_ID_POPLESS_EQUALIZER. This param ID is
    used for get param only.
*/
#define AUDPROC_PARAM_ID_EQ_NUM_BANDS                          0x000108BD
/** ID of the Popless Equalizer band levels parameter used by
    AUDPROC_MODULE_ID_POPLESS_EQUALIZER. This param ID is
    used for get param only.
*/
#define AUDPROC_PARAM_ID_EQ_BAND_LEVELS                        0x000108BE
/** ID of the Popless Equalizer band level range parameter used
    by AUDPROC_MODULE_ID_POPLESS_EQUALIZER. This param ID is
    used for get param only.
*/
#define AUDPROC_PARAM_ID_EQ_BAND_LEVEL_RANGE                   0x000108BF
/** ID of the Popless Equalizer band frequencies parameter used
    by AUDPROC_MODULE_ID_POPLESS_EQUALIZER. This param ID is
    used for get param only.
*/
#define AUDPROC_PARAM_ID_EQ_BAND_FREQS                         0x000108C0
/** ID of the Popless Equalizer single band frequency range
    parameter used by AUDPROC_MODULE_ID_POPLESS_EQUALIZER.
    This param ID is used for get param only.
*/
#define AUDPROC_PARAM_ID_EQ_SINGLE_BAND_FREQ_RANGE             0x000108C1
/** ID of the Popless Equalizer single band frequency parameter
    used by AUDPROC_MODULE_ID_POPLESS_EQUALIZER. This param ID
    is used for set param only.
*/
#define AUDPROC_PARAM_ID_EQ_SINGLE_BAND_FREQ                   0x000108C2
/** ID of the Popless Equalizer band index parameter used by
    AUDPROC_MODULE_ID_POPLESS_EQUALIZER.
*/
#define AUDPROC_PARAM_ID_EQ_BAND_INDEX                         0x000108C3
/** ID of the Popless Equalizer preset id parameter used by
    AUDPROC_MODULE_ID_POPLESS_EQUALIZER. This param ID is used
    for get param only.
*/
#define AUDPROC_PARAM_ID_EQ_PRESET_ID                          0x000108C4
/** ID of the Popless Equalizer number of presets parameter used
    by AUDPROC_MODULE_ID_POPLESS_EQUALIZER. This param ID is used
    for get param only.
*/
#define AUDPROC_PARAM_ID_EQ_NUM_PRESETS                        0x000108C5
/** ID of the Popless Equalizer preset name parameter used by
    AUDPROC_MODULE_ID_POPLESS_EQUALIZER. This param ID is used
    for get param only.
*/
#define AUDPROC_PARAM_ID_EQ_PRESET_NAME                        0x000108C6

/* Set Q6 topologies */
#define ASM_CMD_ADD_TOPOLOGIES				0x00010DBE
#define ADM_CMD_ADD_TOPOLOGIES				0x00010335

/* structure used for both ioctls */
struct cmd_set_topologies {
	struct apr_hdr hdr;
	u32		payload_addr_lsw;
	/* LSW of parameter data payload address.*/
	u32		payload_addr_msw;
	/* MSW of parameter data payload address.*/
	u32		mem_map_handle;
	/* Memory map handle returned by mem map command */
	u32		payload_size;
	/* Size in bytes of the variable payload in shared memory */
} __packed;

/* This module represents the Rx processing of Feedback speaker protection.
 * It contains the excursion control, thermal protection,
 * analog clip manager features in it.
 * This module id will support following param ids.
 * - AFE_PARAM_ID_FBSP_MODE_RX_CFG
 */

#define AFE_MODULE_FB_SPKR_PROT_RX 0x0001021C

#define AFE_PARAM_ID_FBSP_MODE_RX_CFG 0x0001021D

struct asm_fbsp_mode_rx_cfg {
	uint32_t minor_version;
	uint32_t mode;
} __packed;

/* This module represents the VI processing of feedback speaker protection.
 * It will receive Vsens and Isens from codec and generates necessary
 * parameters needed by Rx processing.
 * This module id will support following param ids.
 * - AFE_PARAM_ID_SPKR_CALIB_VI_PROC_CFG
 * - AFE_PARAM_ID_CALIB_RES_CFG
 * - AFE_PARAM_ID_FEEDBACK_PATH_CFG
 */

#define AFE_MODULE_FB_SPKR_PROT_VI_PROC 0x00010226

#define AFE_PARAM_ID_SPKR_CALIB_VI_PROC_CFG 0x0001022A

struct asm_spkr_calib_vi_proc_cfg {
	uint32_t minor_version;
	int32_t	r0_cali_q24;
	int16_t	t0_cali_q6;
	int16_t	reserved;
} __packed;

#define AFE_PARAM_ID_CALIB_RES_CFG 0x0001022B

struct asm_calib_res_cfg {
	uint32_t minor_version;
	int32_t	r0_cali_q24;
	uint32_t th_vi_ca_state;
} __packed;

#define AFE_PARAM_ID_FEEDBACK_PATH_CFG 0x0001022C

struct asm_feedback_path_cfg {
	uint32_t minor_version;
	int32_t	dst_portid;
	int32_t	num_channels;
	int32_t	chan_info[4];
} __packed;

#define AFE_PARAM_ID_MODE_VI_PROC_CFG 0x00010227

struct asm_mode_vi_proc_cfg {
	uint32_t minor_version;
	uint32_t cal_mode;
} __packed;

union afe_spkr_prot_config {
	struct asm_fbsp_mode_rx_cfg mode_rx_cfg;
	struct asm_spkr_calib_vi_proc_cfg vi_proc_cfg;
	struct asm_feedback_path_cfg feedback_path_cfg;
	struct asm_mode_vi_proc_cfg mode_vi_proc_cfg;
} __packed;

struct afe_spkr_prot_config_command {
	struct apr_hdr hdr;
	struct afe_port_cmd_set_param_v2 param;
	struct afe_port_param_data_v2 pdata;
	union afe_spkr_prot_config prot_config;
} __packed;

struct afe_spkr_prot_get_vi_calib {
	struct apr_hdr hdr;
	struct afe_port_cmd_get_param_v2 get_param;
	struct afe_port_param_data_v2 pdata;
	struct asm_calib_res_cfg res_cfg;
} __packed;

struct afe_spkr_prot_calib_get_resp {
	uint32_t status;
	struct afe_port_param_data_v2 pdata;
	struct asm_calib_res_cfg res_cfg;
} __packed;


/* SRS TRUMEDIA start */
/* topology */
#define SRS_TRUMEDIA_TOPOLOGY_ID			0x00010D90
/* module */
#define SRS_TRUMEDIA_MODULE_ID				0x10005010
/* parameters */
#define SRS_TRUMEDIA_PARAMS				0x10005011
#define SRS_TRUMEDIA_PARAMS_WOWHD			0x10005012
#define SRS_TRUMEDIA_PARAMS_CSHP			0x10005013
#define SRS_TRUMEDIA_PARAMS_HPF				0x10005014
#define SRS_TRUMEDIA_PARAMS_PEQ				0x10005015
#define SRS_TRUMEDIA_PARAMS_HL				0x10005016

#define SRS_ID_GLOBAL	0x00000001
#define SRS_ID_WOWHD	0x00000002
#define SRS_ID_CSHP	0x00000003
#define SRS_ID_HPF	0x00000004
#define SRS_ID_PEQ	0x00000005
#define SRS_ID_HL	0x00000006

#define SRS_CMD_UPLOAD		0x7FFF0000
#define SRS_PARAM_INDEX_MASK	0x80000000
#define SRS_PARAM_OFFSET_MASK	0x3FFF0000
#define SRS_PARAM_VALUE_MASK	0x0000FFFF

struct srs_trumedia_params_GLOBAL {
	uint8_t                  v1;
	uint8_t                  v2;
	uint8_t                  v3;
	uint8_t                  v4;
	uint8_t                  v5;
	uint8_t                  v6;
	uint8_t                  v7;
	uint8_t                  v8;
} __packed;

struct srs_trumedia_params_WOWHD {
	uint32_t				v1;
	uint16_t				v2;
	uint16_t				v3;
	uint16_t				v4;
	uint16_t				v5;
	uint16_t				v6;
	uint16_t				v7;
	uint16_t				v8;
	uint16_t				v____A1;
	uint32_t				v9;
	uint16_t				v10;
	uint16_t				v11;
	uint32_t				v12[16];
} __packed;

struct srs_trumedia_params_CSHP {
	uint32_t				v1;
	uint16_t				v2;
	uint16_t				v3;
	uint16_t				v4;
	uint16_t				v5;
	uint16_t				v6;
	uint16_t				v____A1;
	uint32_t				v7;
	uint16_t				v8;
	uint16_t				v9;
	uint32_t				v10[16];
} __packed;

struct srs_trumedia_params_HPF {
	uint32_t				v1;
	uint32_t				v2[26];
} __packed;

struct srs_trumedia_params_PEQ {
	uint32_t				v1;
	uint16_t				v2;
	uint16_t				v3;
	uint16_t				v4;
	uint16_t				v____A1;
	uint32_t				v5[26];
	uint32_t				v6[26];
} __packed;

struct srs_trumedia_params_HL {
	uint16_t				v1;
	uint16_t				v2;
	uint16_t				v3;
	uint16_t				v____A1;
	int32_t					v4;
	uint32_t				v5;
	uint16_t				v6;
	uint16_t				v____A2;
	uint32_t				v7;
} __packed;

struct srs_trumedia_params {
	struct srs_trumedia_params_GLOBAL	global;
	struct srs_trumedia_params_WOWHD	wowhd;
	struct srs_trumedia_params_CSHP		cshp;
	struct srs_trumedia_params_HPF		hpf;
	struct srs_trumedia_params_PEQ		peq;
	struct srs_trumedia_params_HL		hl;
} __packed;
/* SRS TruMedia end */

/* DTS Eagle */
#define AUDPROC_MODULE_ID_DTS_HPX_PREMIX 0x0001077C
#define AUDPROC_MODULE_ID_DTS_HPX_POSTMIX 0x0001077B
#define ASM_STREAM_POSTPROC_TOPO_ID_DTS_HPX 0x00010DED
struct asm_dts_eagle_param {
	struct apr_hdr	hdr;
	struct asm_stream_cmd_set_pp_params_v2 param;
	struct asm_stream_param_data_v2 data;
} __packed;

struct asm_dts_eagle_param_get {
	struct apr_hdr	hdr;
	struct asm_stream_cmd_get_pp_params_v2 param;
} __packed;

/* LSM Specific */
#define VW_FEAT_DIM					(39)

#define APRV2_IDS_SERVICE_ID_ADSP_LSM_V			(0xD)
#define APRV2_IDS_DOMAIN_ID_ADSP_V			(0x4)
#define APRV2_IDS_DOMAIN_ID_APPS_V			(0x5)

#define LSM_SESSION_CMD_SHARED_MEM_MAP_REGIONS		(0x00012A7F)
#define LSM_SESSION_CMDRSP_SHARED_MEM_MAP_REGIONS	(0x00012A80)
#define LSM_SESSION_CMD_SHARED_MEM_UNMAP_REGIONS	(0x00012A81)
#define LSM_SESSION_CMD_OPEN_TX				(0x00012A82)
#define LSM_SESSION_CMD_CLOSE_TX			(0x00012A88)
#define LSM_SESSION_CMD_SET_PARAMS			(0x00012A83)
#define LSM_SESSION_CMD_REGISTER_SOUND_MODEL		(0x00012A84)
#define LSM_SESSION_CMD_DEREGISTER_SOUND_MODEL		(0x00012A85)
#define LSM_SESSION_CMD_START				(0x00012A86)
#define LSM_SESSION_CMD_STOP				(0x00012A87)

#define LSM_SESSION_EVENT_DETECTION_STATUS		(0x00012B00)
#define LSM_SESSION_EVENT_DETECTION_STATUS_V2		(0x00012B01)

#define LSM_MODULE_ID_VOICE_WAKEUP			(0x00012C00)
#define LSM_PARAM_ID_ENDPOINT_DETECT_THRESHOLD		(0x00012C01)
#define LSM_PARAM_ID_OPERATION_MODE			(0x00012C02)
#define LSM_PARAM_ID_GAIN				(0x00012C03)
#define LSM_PARAM_ID_CONNECT_TO_PORT			(0x00012C04)
#define LSM_PARAM_ID_KEYWORD_DETECT_SENSITIVITY		(0x00012C05)
#define LSM_PARAM_ID_USER_DETECT_SENSITIVITY		(0x00012C06)
#define LSM_PARAM_ID_FEATURE_COMPENSATION_DATA		(0x00012C07)
#define LSM_PARAM_ID_MIN_CONFIDENCE_LEVELS		(0x00012C07)

/* HW MAD specific */
#define AFE_MODULE_HW_MAD				(0x00010230)
#define AFE_PARAM_ID_HW_MAD_CFG				(0x00010231)
#define AFE_PARAM_ID_HW_MAD_CTRL			(0x00010232)
#define AFE_PARAM_ID_SLIMBUS_SLAVE_PORT_CFG		(0x00010233)

/* SW MAD specific */
#define AFE_MODULE_SW_MAD				(0x0001022D)
#define AFE_PARAM_ID_SW_MAD_CFG				(0x0001022E)
#define AFE_PARAM_ID_SVM_MODEL				(0x0001022F)

/* Commands/Params to pass the codec/slimbus data to DSP */
#define AFE_SVC_CMD_SET_PARAM				(0x000100f3)
#define AFE_MODULE_CDC_DEV_CFG				(0x00010234)
#define AFE_PARAM_ID_CDC_SLIMBUS_SLAVE_CFG		(0x00010235)
#define AFE_PARAM_ID_CDC_REG_CFG			(0x00010236)
#define AFE_PARAM_ID_CDC_REG_CFG_INIT			(0x00010237)

#define AFE_MAX_CDC_REGISTERS_TO_CONFIG			(20)

/* AANC Port Config Specific */
#define AFE_PARAM_ID_AANC_PORT_CONFIG			(0x00010215)
#define AFE_API_VERSION_AANC_PORT_CONFIG		(0x1)
#define AANC_TX_MIC_UNUSED				(0)
#define AANC_TX_VOICE_MIC				(1)
#define AANC_TX_ERROR_MIC				(2)
#define AANC_TX_NOISE_MIC				(3)
#define AFE_PORT_MAX_CHANNEL_CNT			(8)
#define AFE_MODULE_AANC					(0x00010214)
#define AFE_PARAM_ID_CDC_AANC_VERSION			(0x0001023A)
#define AFE_API_VERSION_CDC_AANC_VERSION		(0x1)
#define AANC_HW_BLOCK_VERSION_1				(1)
#define AANC_HW_BLOCK_VERSION_2				(2)

/*Clip bank selection*/
#define AFE_API_VERSION_CLIP_BANK_SEL_CFG 0x1
#define AFE_CLIP_MAX_BANKS		4
#define AFE_PARAM_ID_CLIP_BANK_SEL_CFG 0x00010242

struct afe_param_aanc_port_cfg {
	/* Minor version used for tracking the version of the module's
	* source port configuration.
	*/
	uint32_t aanc_port_cfg_minor_version;

	/* Sampling rate of the source Tx port. 8k - 192k*/
	uint32_t tx_port_sample_rate;

	/* Channel mapping for the Tx port signal carrying Noise (X),
	* Error (E), and Voice (V) signals.
	*/
	uint8_t tx_port_channel_map[AFE_PORT_MAX_CHANNEL_CNT];

	/* Number of channels on the source Tx port. */
	uint16_t tx_port_num_channels;

	/* Port ID of the Rx path reference signal. */
	uint16_t rx_path_ref_port_id;

	/* Sampling rate of the reference port. 8k - 192k*/
	uint32_t ref_port_sample_rate;
} __packed;

struct afe_param_id_cdc_aanc_version {
	/* Minor version used for tracking the version of the module's
	* hw version
	*/
	uint32_t cdc_aanc_minor_version;

	/* HW version. */
	uint32_t aanc_hw_version;
} __packed;

struct afe_param_id_clip_bank_sel {
	/* Minor version used for tracking the version of the module's
	* hw version
	*/
	uint32_t minor_version;

	/* Number of banks to be read */
	uint32_t num_banks;

	uint32_t bank_map[AFE_CLIP_MAX_BANKS];
} __packed;

/* ERROR CODES */
/* Success. The operation completed with no errors. */
#define ADSP_EOK          0x00000000
/* General failure. */
#define ADSP_EFAILED      0x00000001
/* Bad operation parameter. */
#define ADSP_EBADPARAM    0x00000002
/* Unsupported routine or operation. */
#define ADSP_EUNSUPPORTED 0x00000003
/* Unsupported version. */
#define ADSP_EVERSION     0x00000004
/* Unexpected problem encountered. */
#define ADSP_EUNEXPECTED  0x00000005
/* Unhandled problem occurred. */
#define ADSP_EPANIC       0x00000006
/* Unable to allocate resource. */
#define ADSP_ENORESOURCE  0x00000007
/* Invalid handle. */
#define ADSP_EHANDLE      0x00000008
/* Operation is already processed. */
#define ADSP_EALREADY     0x00000009
/* Operation is not ready to be processed. */
#define ADSP_ENOTREADY    0x0000000A
/* Operation is pending completion. */
#define ADSP_EPENDING     0x0000000B
/* Operation could not be accepted or processed. */
#define ADSP_EBUSY        0x0000000C
/* Operation aborted due to an error. */
#define ADSP_EABORTED     0x0000000D
/* Operation preempted by a higher priority. */
#define ADSP_EPREEMPTED   0x0000000E
/* Operation requests intervention to complete. */
#define ADSP_ECONTINUE    0x0000000F
/* Operation requests immediate intervention to complete. */
#define ADSP_EIMMEDIATE   0x00000010
/* Operation is not implemented. */
#define ADSP_ENOTIMPL     0x00000011
/* Operation needs more data or resources. */
#define ADSP_ENEEDMORE    0x00000012
/* Operation does not have memory. */
#define ADSP_ENOMEMORY     0x00000014
/* Item does not exist. */
#define ADSP_ENOTEXIST      0x00000015
/* Operation is finished. */
#define ADSP_ETERMINATED    0x00011174

/*bharath, adsp_error_codes.h */

/* LPASS clock for I2S Interface */

/* Supported OSR clock values */
#define Q6AFE_LPASS_OSR_CLK_12_P288_MHZ		0xBB8000
#define Q6AFE_LPASS_OSR_CLK_8_P192_MHZ		0x7D0000
#define Q6AFE_LPASS_OSR_CLK_6_P144_MHZ		0x5DC000
#define Q6AFE_LPASS_OSR_CLK_4_P096_MHZ		0x3E8000
#define Q6AFE_LPASS_OSR_CLK_3_P072_MHZ		0x2EE000
#define Q6AFE_LPASS_OSR_CLK_2_P048_MHZ		0x1F4000
#define Q6AFE_LPASS_OSR_CLK_1_P536_MHZ		0x177000
#define Q6AFE_LPASS_OSR_CLK_1_P024_MHZ		 0xFA000
#define Q6AFE_LPASS_OSR_CLK_768_kHZ		 0xBB800
#define Q6AFE_LPASS_OSR_CLK_512_kHZ		 0x7D000
#define Q6AFE_LPASS_OSR_CLK_DISABLE		     0x0

/* Supported Bit clock values */
#define Q6AFE_LPASS_IBIT_CLK_8_P192_MHZ		0x7D0000
#define Q6AFE_LPASS_IBIT_CLK_6_P144_MHZ		0x5DC000
#define Q6AFE_LPASS_IBIT_CLK_4_P096_MHZ		0x3E8000
#define Q6AFE_LPASS_IBIT_CLK_3_P072_MHZ		0x2EE000
#define Q6AFE_LPASS_IBIT_CLK_2_P048_MHZ		0x1F4000
#define Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ		0x177000
#define Q6AFE_LPASS_IBIT_CLK_1_P024_MHZ		 0xFA000
#define Q6AFE_LPASS_IBIT_CLK_768_KHZ		 0xBB800
#define Q6AFE_LPASS_IBIT_CLK_512_KHZ		 0x7D000
#define Q6AFE_LPASS_IBIT_CLK_256_KHZ		 0x3E800
#define Q6AFE_LPASS_IBIT_CLK_DISABLE		     0x0

/* Supported LPASS CLK sources */
#define Q6AFE_LPASS_CLK_SRC_EXTERNAL 0
#define Q6AFE_LPASS_CLK_SRC_INTERNAL 1

/* Supported LPASS CLK root*/
#define Q6AFE_LPASS_CLK_ROOT_DEFAULT 0

enum afe_lpass_clk_mode {
	Q6AFE_LPASS_MODE_BOTH_INVALID,
	Q6AFE_LPASS_MODE_CLK1_VALID,
	Q6AFE_LPASS_MODE_CLK2_VALID,
	Q6AFE_LPASS_MODE_BOTH_VALID,
} __packed;

struct afe_clk_cfg {
/* Minor version used for tracking the version of the I2S
 * configuration interface.
 * Supported values: #AFE_API_VERSION_I2S_CONFIG
 */
	u32                  i2s_cfg_minor_version;

/* clk value 1 in MHz. */
	u32                  clk_val1;

/* clk value 2 in MHz. */
	u32                  clk_val2;

/* clk_src
 * #Q6AFE_LPASS_CLK_SRC_EXTERNAL
 * #Q6AFE_LPASS_CLK_SRC_INTERNAL
 */

	u16                  clk_src;

/* clk_root -0 for default */
	u16                  clk_root;

/* clk_set_mode
 * #Q6AFE_LPASS_MODE_BOTH_INVALID
 * #Q6AFE_LPASS_MODE_CLK1_VALID
 * #Q6AFE_LPASS_MODE_CLK2_VALID
 * #Q6AFE_LPASS_MODE_BOTH_VALID
 */
	u16                  clk_set_mode;

/* This param id is used to configure I2S clk */
	u16                  reserved;
} __packed;

/* This param id is used to configure I2S clk */
#define AFE_PARAM_ID_LPAIF_CLK_CONFIG	0x00010238


struct afe_lpass_clk_config_command {
	struct apr_hdr			 hdr;
	struct afe_port_cmd_set_param_v2 param;
	struct afe_port_param_data_v2    pdata;
	struct afe_clk_cfg clk_cfg;
} __packed;

enum afe_lpass_digital_clk_src {
	Q6AFE_LPASS_DIGITAL_ROOT_INVALID,
	Q6AFE_LPASS_DIGITAL_ROOT_PRI_MI2S_OSR,
	Q6AFE_LPASS_DIGITAL_ROOT_SEC_MI2S_OSR,
	Q6AFE_LPASS_DIGITAL_ROOT_TER_MI2S_OSR,
	Q6AFE_LPASS_DIGITAL_ROOT_QUAD_MI2S_OSR,
	Q6AFE_LPASS_DIGITAL_ROOT_CDC_ROOT_CLK,
} __packed;

/* This param id is used to configure internal clk */
#define AFE_PARAM_ID_INTERNAL_DIGIATL_CDC_CLK_CONFIG	0x00010239

struct afe_digital_clk_cfg {
/* Minor version used for tracking the version of the I2S
 * configuration interface.
 * Supported values: #AFE_API_VERSION_I2S_CONFIG
 */
	u32                  i2s_cfg_minor_version;

/* clk value in MHz. */
	u32                  clk_val;

/*	INVALID
 *	PRI_MI2S_OSR
 *	SEC_MI2S_OSR
 *	TER_MI2S_OSR
 *	QUAD_MI2S_OSR
 *	DIGT_CDC_ROOT
 */
	u16                  clk_root;

/* This field must be set to zero. */
	u16                  reserved;
} __packed;


struct afe_lpass_digital_clk_config_command {
	struct apr_hdr			 hdr;
	struct afe_port_cmd_set_param_v2 param;
	struct afe_port_param_data_v2    pdata;
	struct afe_digital_clk_cfg clk_cfg;
} __packed;

/*
 * Opcode for AFE to start DTMF.
 */
#define AFE_PORTS_CMD_DTMF_CTL	0x00010102

/** DTMF payload.*/
struct afe_dtmf_generation_command {
	struct apr_hdr hdr;

	/*
	 * Duration of the DTMF tone in ms.
	 * -1      -> continuous,
	 *  0      -> disable
	 */
	int64_t                   duration_in_ms;

	/*
	 * The DTMF high tone frequency.
	 */
	uint16_t                  high_freq;

	/*
	 * The DTMF low tone frequency.
	 */
	uint16_t                  low_freq;

	/*
	 * The DTMF volume setting
	 */
	uint16_t                  gain;

	/*
	 * The number of ports to enable/disable on.
	 */
	uint16_t                  num_ports;

	/*
	 * The Destination ports - array  .
	 * For DTMF on multiple ports, portIds needs to
	 * be populated numPorts times.
	 */
	uint16_t                  port_ids;

	/*
	 * variable for 32 bit alignment of APR packet.
	 */
	uint16_t                  reserved;
} __packed;

enum afe_config_type {
	AFE_SLIMBUS_SLAVE_PORT_CONFIG,
	AFE_SLIMBUS_SLAVE_CONFIG,
	AFE_CDC_REGISTERS_CONFIG,
	AFE_AANC_VERSION,
	AFE_CDC_CLIP_REGISTERS_CONFIG,
	AFE_CLIP_BANK_SEL,
	AFE_MAX_CONFIG_TYPES,
};

struct afe_param_slimbus_slave_port_cfg {
	uint32_t minor_version;
	uint16_t slimbus_dev_id;
	uint16_t slave_dev_pgd_la;
	uint16_t slave_dev_intfdev_la;
	uint16_t bit_width;
	uint16_t data_format;
	uint16_t num_channels;
	uint16_t slave_port_mapping[AFE_PORT_MAX_AUDIO_CHAN_CNT];
} __packed;

struct afe_param_cdc_slimbus_slave_cfg {
	uint32_t minor_version;
	uint32_t device_enum_addr_lsw;
	uint32_t device_enum_addr_msw;
	uint16_t tx_slave_port_offset;
	uint16_t rx_slave_port_offset;
} __packed;

struct afe_param_cdc_reg_cfg {
	uint32_t minor_version;
	uint32_t reg_logical_addr;
	uint32_t reg_field_type;
	uint32_t reg_field_bit_mask;
	uint16_t reg_bit_width;
	uint16_t reg_offset_scale;
} __packed;

struct afe_param_cdc_reg_cfg_data {
	uint32_t num_registers;
	struct afe_param_cdc_reg_cfg *reg_data;
} __packed;

struct afe_svc_cmd_set_param {
	uint32_t payload_size;
	uint32_t payload_address_lsw;
	uint32_t payload_address_msw;
	uint32_t mem_map_handle;
} __packed;

struct afe_param_hw_mad_ctrl {
	uint32_t minor_version;
	uint16_t mad_type;
	uint16_t mad_enable;
} __packed;

struct afe_cmd_hw_mad_ctrl {
	struct apr_hdr hdr;
	struct afe_port_cmd_set_param_v2 param;
	struct afe_port_param_data_v2 pdata;
	struct afe_param_hw_mad_ctrl payload;
} __packed;

struct afe_cmd_hw_mad_slimbus_slave_port_cfg {
	struct apr_hdr hdr;
	struct afe_port_cmd_set_param_v2 param;
	struct afe_port_param_data_v2 pdata;
	struct afe_param_slimbus_slave_port_cfg sb_port_cfg;
} __packed;

struct afe_cmd_sw_mad_enable {
	struct apr_hdr hdr;
	struct afe_port_cmd_set_param_v2 param;
	struct afe_port_param_data_v2 pdata;
} __packed;

struct afe_param_cdc_reg_cfg_payload {
	struct afe_port_param_data_v2 common;
	struct afe_param_cdc_reg_cfg  reg_cfg;
} __packed;

/*
 * reg_data's size can be up to AFE_MAX_CDC_REGISTERS_TO_CONFIG
 */
struct afe_svc_cmd_cdc_reg_cfg {
	struct apr_hdr hdr;
	struct afe_svc_cmd_set_param param;
	struct afe_param_cdc_reg_cfg_payload reg_data[0];
} __packed;

struct afe_svc_cmd_init_cdc_reg_cfg {
	struct apr_hdr hdr;
	struct afe_svc_cmd_set_param param;
	struct afe_port_param_data_v2 init;
} __packed;

struct afe_svc_cmd_sb_slave_cfg {
	struct apr_hdr hdr;
	struct afe_svc_cmd_set_param param;
	struct afe_port_param_data_v2 pdata;
	struct afe_param_cdc_slimbus_slave_cfg sb_slave_cfg;
} __packed;

struct afe_svc_cmd_cdc_aanc_version {
	struct apr_hdr hdr;
	struct afe_svc_cmd_set_param param;
	struct afe_port_param_data_v2 pdata;
	struct afe_param_id_cdc_aanc_version version;
} __packed;

struct afe_port_cmd_set_aanc_param {
	struct apr_hdr hdr;
	struct afe_port_cmd_set_param_v2 param;
	struct afe_port_param_data_v2 pdata;
	union {
		struct afe_param_aanc_port_cfg aanc_port_cfg;
		struct afe_mod_enable_param    mod_enable;
	} __packed data;
} __packed;

struct afe_port_cmd_set_aanc_acdb_table {
	struct apr_hdr hdr;
	struct afe_port_cmd_set_param_v2 param;
} __packed;

/* Dolby DAP topology */
#define DOLBY_ADM_COPP_TOPOLOGY_ID	0x0001033B

/* RMS value from DSP */
#define RMS_MODULEID_APPI_PASSTHRU  0x10009011
#define RMS_PARAM_FIRST_SAMPLE 0x10009012
#define RMS_PAYLOAD_LEN 4

/* Customized mixing in matix mixer */
#define MTMX_MODULE_ID_DEFAULT_CHMIXER  0x00010341
#define DEFAULT_CHMIXER_PARAM_ID_COEFF  0x00010342
#define CUSTOM_STEREO_PAYLOAD_SIZE	9
#define CUSTOM_STEREO_CMD_PARAM_SIZE	24
#define CUSTOM_STEREO_NUM_OUT_CH	0x0002
#define CUSTOM_STEREO_NUM_IN_CH		0x0002
#define CUSTOM_STEREO_INDEX_PARAM	0x0002
#define Q14_GAIN_ZERO_POINT_FIVE	0x2000
#define Q14_GAIN_UNITY			0x4000

struct afe_svc_cmd_set_clip_bank_selection {
	struct apr_hdr hdr;
	struct afe_svc_cmd_set_param param;
	struct afe_port_param_data_v2 pdata;
	struct afe_param_id_clip_bank_sel bank_sel;
} __packed;

/* Ultrasound supported formats */
#define US_POINT_EPOS_FORMAT_V2 0x0001272D
#define US_RAW_FORMAT_V2        0x0001272C
#define US_PROX_FORMAT_V2       0x0001272E
#define US_RAW_SYNC_FORMAT      0x0001272F
#define US_GES_SYNC_FORMAT      0x00012730

#define AFE_MODULE_GROUP_DEVICE	0x00010254
#define AFE_PARAM_ID_GROUP_DEVICE_CFG	0x00010255
#define AFE_PARAM_ID_GROUP_DEVICE_ENABLE 0x00010256
#define AFE_GROUP_DEVICE_ID_SECONDARY_MI2S_RX	0x1102

/*  Payload of the #AFE_PARAM_ID_GROUP_DEVICE_CFG
 * parameter, which configures max of 8 AFE ports
 * into a group.
 * The fixed size of this structure is sixteen bytes.
 */
struct afe_group_device_group_cfg {
	u32 minor_version;
	u16 group_id;
	u16 num_channels;
	u16 port_id[8];
} __packed;


/*  Payload of the #AFE_PARAM_ID_GROUP_DEVICE_ENABLE
 * parameter, which enables or
 * disables any module.
 * The fixed size of this structure is four bytes.
 */

struct afe_group_device_enable {
	u16 group_id;
	/* valid value is AFE_GROUP_DEVICE_ID_SECONDARY_MI2S_RX */
	u16 enable;
/* Enables (1) or disables (0) the module. */
} __packed;

struct afe_port_group_create {
	struct apr_hdr hdr;
	struct afe_svc_cmd_set_param param;
	struct afe_port_param_data_v2 pdata;
	union {
		struct afe_group_device_group_cfg group_cfg;
		struct afe_group_device_enable group_enable;
	} __packed data;
} __packed;

#endif /*_APR_AUDIO_V2_H_ */
