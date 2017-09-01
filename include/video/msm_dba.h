/*
 * Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#ifndef _MSM_DBA_H
#define _MSM_DBA_H

#include <linux/types.h>
#include <linux/bitops.h>

#define MSM_DBA_CHIP_NAME_MAX_LEN 20
#define MSM_DBA_CLIENT_NAME_LEN   20

#define MSM_DBA_DEFER_PROPERTY_FLAG 0x1
#define MSM_DBA_ASYNC_FLAG          0x2

/**
 * enum msm_dba_callback_event - event types for callback notification
 * @MSM_DBA_CB_REMOTE_INT: Event associated with remote devices on an interface
 *			   that supports a bi-directional control channel.
 * @MSM_DBA_CB_HDCP_LINK_AUTHENTICATED: Authentication session is successful.
 *					The link is authenticated and encryption
 *					can be enabled if not enabled already.
 * @MSM_DBA_CB_HDCP_LINK_UNAUTHENTICATED: A previously authenticated link has
 *					  failed. The content on the interface
 *					  is no longer secure.
 * @MSM_DBA_CB_HPD_CONNECT: Detected a cable connect event.
 * @MSM_DBA_CB_HPD_DISCONNECT: Detected a cable disconnect event.
 * @MSM_DBA_CB_VIDEO_FAILURE: Detected a failure with respect to video data on
 *			      the interface. This is a generic failure and
 *			      client should request a debug dump to debug the
 *			      issue. Client can also attempt a reset to recover
 *			      the device.
 * @MSM_DBA_CB_AUDIO_FAILURE: Detected a failure with respect to audio data on
 *			      the interface. This is a generic failure and
 *			      client should request a debug dump. Client can
 *			      also attempt a reset to recover the device.
 * @MSM_DBA_CB_CEC_WRITE_SUCCESS: The asynchronous CEC write request is
 *				  successful.
 * @MSM_DBA_CB_CEC_WRITE_FAIL: The asynchronous CEC write request failed.
 * @MSM_DBA_CB_CEC_READ_PENDING: There is a pending CEC read message.
 * @MSM_DBA_CB_PRE_RESET: This callback is called just before the device is
 *			  being reset.
 * @MSM_DBA_CB_POST_RESET: This callback is called after device reset is
 *			   complete and the driver has applied back all the
 *			   properties.
 *
 * Clients for this driver can register for receiving callbacks for specific
 * events. This enum defines the type of events supported by the driver. An
 * event mask is typically used to denote multiple events.
 */
enum msm_dba_callback_event {
	MSM_DBA_CB_REMOTE_INT = BIT(0),
	MSM_DBA_CB_HDCP_LINK_AUTHENTICATED = BIT(1),
	MSM_DBA_CB_HDCP_LINK_UNAUTHENTICATED = BIT(2),
	MSM_DBA_CB_HPD_CONNECT = BIT(3),
	MSM_DBA_CB_HPD_DISCONNECT = BIT(4),
	MSM_DBA_CB_VIDEO_FAILURE = BIT(5),
	MSM_DBA_CB_AUDIO_FAILURE = BIT(6),
	MSM_DBA_CB_CEC_WRITE_SUCCESS = BIT(7),
	MSM_DBA_CB_CEC_WRITE_FAIL = BIT(8),
	MSM_DBA_CB_CEC_READ_PENDING = BIT(9),
	MSM_DBA_CB_PRE_RESET = BIT(10),
	MSM_DBA_CB_POST_RESET = BIT(11),
};

/**
 * enum msm_dba_audio_interface_type - audio interface type
 * @MSM_DBA_AUDIO_I2S_INTERFACE: I2S interface for audio
 * @MSM_DBA_AUDIO_SPDIF_INTERFACE: SPDIF interface for audio
 */
enum msm_dba_audio_interface_type {
	MSM_DBA_AUDIO_I2S_INTERFACE = BIT(0),
	MSM_DBA_AUDIO_SPDIF_INTERFACE = BIT(1),
};

/**
 * enum msm_dba_audio_format_type - audio format type
 * @MSM_DBA_AUDIO_FMT_UNCOMPRESSED_LPCM: uncompressed format
 * @MSM_DBA_AUDIO_FMT_COMPRESSED: compressed formats
 */
enum msm_dba_audio_format_type {
	MSM_DBA_AUDIO_FMT_UNCOMPRESSED_LPCM = BIT(0),
	MSM_DBA_AUDIO_FMT_COMPRESSED = BIT(1),
};

/**
 * enum msm_dba_audio_copyright_type - audio copyright
 * @MSM_DBA_AUDIO_COPYRIGHT_PROTECTED: copy right protected
 * @MSM_DBA_AUDIO_COPYRIGHT_NOT_PROTECTED: not copy right protected
 */
enum msm_dba_audio_copyright_type {
	MSM_DBA_AUDIO_COPYRIGHT_PROTECTED = BIT(0),
	MSM_DBA_AUDIO_COPYRIGHT_NOT_PROTECTED = BIT(1),
};

/**
 * enum msm_dba_audio_pre_emphasis_type - pre-emphasis
 * @MSM_DBA_AUDIO_NO_PRE_EMPHASIS: 2 audio channels w/o pre-emphasis
 * @MSM_DBA_AUDIO_PRE_EMPHASIS_50_15us: 2 audio channels with 50/15uS
 */
enum msm_dba_audio_pre_emphasis_type {
	MSM_DBA_AUDIO_NO_PRE_EMPHASIS = BIT(0),
	MSM_DBA_AUDIO_PRE_EMPHASIS_50_15us = BIT(1),
};

/**
 * enum msm_dba_audio_clock_accuracy - Audio Clock Accuracy
 * @MSM_DBA_AUDIO_CLOCK_ACCURACY_LVL1: normal accuracy +/-1000 x 10^-6
 * @MSM_DBA_AUDIO_CLOCK_ACCURACY_LVL2: high accuracy +/- 50 x 10^-6
 * @MSM_DBA_AUDIO_CLOCK_ACCURACY_LVL3: variable pitch shifted clock
 */
enum msm_dba_audio_clock_accuracy {
	MSM_DBA_AUDIO_CLOCK_ACCURACY_LVL1 = BIT(1),
	MSM_DBA_AUDIO_CLOCK_ACCURACY_LVL2 = BIT(0),
	MSM_DBA_AUDIO_CLOCK_ACCURACY_LVL3 = BIT(2),
};

/**
 * enum msm_dba_channel_status_source - CS override
 * @MSM_DBA_AUDIO_CS_SOURCE_I2S_STREAM: use channel status bits from I2S stream
 * @MSM_DBA_AUDIO_CS_SOURCE_REGISTERS: use channel status bits from registers
 */
enum msm_dba_channel_status_source {
	MSM_DBA_AUDIO_CS_SOURCE_I2S_STREAM,
	MSM_DBA_AUDIO_CS_SOURCE_REGISTERS
};

/**
 * enum msm_dba_audio_sampling_rates_type - audio sampling rates
 * @MSM_DBA_AUDIO_32KHZ: 32KHz sampling rate
 * @MSM_DBA_AUDIO_44P1KHZ: 44.1KHz sampling rate
 * @MSM_DBA_AUDIO_48KHZ: 48KHz sampling rate
 * @MSM_DBA_AUDIO_96KHZ: 96KHz sampling rate
 * @MSM_DBA_AUDIO_192KHZ: 192KHz sampling rate
 */
enum msm_dba_audio_sampling_rates_type {
	MSM_DBA_AUDIO_32KHZ = BIT(0),
	MSM_DBA_AUDIO_44P1KHZ = BIT(1),
	MSM_DBA_AUDIO_48KHZ = BIT(2),
	MSM_DBA_AUDIO_88P2KHZ = BIT(1),
	MSM_DBA_AUDIO_96KHZ = BIT(3),
	MSM_DBA_AUDIO_176P4KHZ = BIT(1),
	MSM_DBA_AUDIO_192KHZ = BIT(4),
};

/**
 * enum msm_dba_audio_word_bit_depth - audio word size
 * @MSM_DBA_AUDIO_WORD_16BIT: 16 bits per word
 * @MSM_DBA_AUDIO_WORD_24BIT: 24 bits per word
 * @MSM_DBA_AUDIO_WORD_32BIT: 32 bits per word
 */
enum msm_dba_audio_word_bit_depth {
	MSM_DBA_AUDIO_WORD_16BIT = BIT(1),
	MSM_DBA_AUDIO_WORD_24BIT = BIT(2),
	MSM_DBA_AUDIO_WORD_32BIT = BIT(3),
};

/**
 * enum msm_dba_audio_channel_count - audio channel count
 * @MSM_DBA_AUDIO_CHANNEL_2: 2 channel audio
 * @MSM_DBA_AUDIO_CHANNEL_4: 4 channel audio
 * @MSM_DBA_AUDIO_CHANNEL_8: 8 channel audio
 */
enum msm_dba_audio_channel_count {
	MSM_DBA_AUDIO_CHANNEL_2 = BIT(0),
	MSM_DBA_AUDIO_CHANNEL_4 = BIT(1),
	MSM_DBA_AUDIO_CHANNEL_8 = BIT(2),
};

/**
 * enum msm_dba_audio_i2s_format - i2s audio data format
 * @MSM_DBA_AUDIO_I2S_FMT_STANDARD: Standard format
 * @MSM_DBA_AUDIO_I2S_FMT_RIGHT_JUSTIFIED: i2s data is right justified
 * @MSM_DBA_AUDIO_I2S_FMT_LEFT_JUSTIFIED: i2s data is left justified
 * @MSM_DBA_AUDIO_I2S_FMT_AES3_DIRECT: AES signal format
 */
enum msm_dba_audio_i2s_format {
	MSM_DBA_AUDIO_I2S_FMT_STANDARD = 0,
	MSM_DBA_AUDIO_I2S_FMT_RIGHT_JUSTIFIED,
	MSM_DBA_AUDIO_I2S_FMT_LEFT_JUSTIFIED,
	MSM_DBA_AUDIO_I2S_FMT_AES3_DIRECT,
	MSM_DBA_AUDIO_I2S_FMT_MAX,
};

enum msm_dba_video_aspect_ratio {
	MSM_DBA_AR_UNKNOWN = 0,
	MSM_DBA_AR_4_3,
	MSM_DBA_AR_5_4,
	MSM_DBA_AR_16_9,
	MSM_DBA_AR_16_10,
	MSM_DBA_AR_64_27,
	MSM_DBA_AR_256_135,
	MSM_DBA_AR_MAX
};

enum msm_dba_audio_word_endian_type {
	MSM_DBA_AUDIO_WORD_LITTLE_ENDIAN = 0,
	MSM_DBA_AUDIO_WORD_BIG_ENDIAN,
	MSM_DBA_AUDIO_WORD_ENDIAN_MAX
};

/**
 * msm_dba_audio_op_mode - i2s audio operation mode
 * @MSM_DBA_AUDIO_MODE_MANUAL: Manual mode
 * @MSM_DBA_AUDIO_MODE_AUTOMATIC: Automatic mode
 */
enum msm_dba_audio_op_mode {
	MSM_DBA_AUDIO_MODE_MANUAL,
	MSM_DBA_AUDIO_MODE_AUTOMATIC,
};

/**
 * typedef *msm_dba_cb() - Prototype for callback function
 * @data: Pointer to user data provided with register API
 * @event: Event type associated with callback. This can be a bitmask.
 */
typedef void (*msm_dba_cb)(void *data, enum msm_dba_callback_event event);

/**
 * struct msm_dba_reg_info - Client information used with register API
 * @client_name: Name of the client for debug purposes
 * @chip_name: Bridge chip ID
 * @instance_id: Instance ID of the bridge chip in case of multiple instances
 * @cb: callback function called in case of events.
 * @cb_data: pointer to a data structure that will be returned with callback
 *
 * msm_dba_reg_info structure will be used to provide information during
 * registering with driver. This structure will contain the information required
 * to identify the specific bridge chip the client wants to use.
 *
 * Client should also specify the callback function which needs to be called in
 * case of events. There is an optional data field which is a pointer that will
 * be returned as one of arguments in the callback function. This data field can
 * be NULL if client does not wish to use it.
 */
struct msm_dba_reg_info {
	char client_name[MSM_DBA_CLIENT_NAME_LEN];
	char chip_name[MSM_DBA_CHIP_NAME_MAX_LEN];
	u32 instance_id;
	msm_dba_cb cb;
	void *cb_data;
};

/**
 * struct msm_dba_video_caps_info - video capabilities of the bridge chip
 * @hdcp_support: if hdcp is supported
 * @edid_support: if reading edid from sink is supported
 * @data_lanes_lp_support: if low power mode is supported on data lanes
 * @clock_lanes_lp_support: If low power mode is supported on clock lanes
 * @max_pclk_khz: maximum pixel clock supported
 * @num_of_input_lanes: Number of input data lanes supported by the bridge chip
 */
struct msm_dba_video_caps_info {
	bool hdcp_support;
	bool edid_support;
	bool data_lanes_lp_support;
	bool clock_lanes_lp_support;
	u32 max_pclk_khz;
	u32 num_of_input_lanes;
};

/**
 * struct msm_dba_audio_caps_info - audio capabilities of the bridge chip
 * @audio_support: if audio is supported
 * @audio_rates: audio sampling rates supported
 * @audio_fmts: audio formats supported
 */
struct msm_dba_audio_caps_info {
	u32 audio_support;
	u32 audio_rates;
	u32 audio_fmts;
};

/**
 * struct msm_dba_capabilities - general capabilities of the bridge chip
 * @vid_caps: video capabilities
 * @aud_caps: audio capabilities
 * @av_mute_support: av mute support in bridge chip
 * @deferred_commit_support: support for deferred commit
 */
struct msm_dba_capabilities {
	struct msm_dba_video_caps_info vid_caps;
	struct msm_dba_audio_caps_info aud_caps;
	bool av_mute_support;
	bool deferred_commit_support;
};

/**
 * struct msm_dba_audio_cfg - Structure for audio configuration
 * @interface: Specifies audio interface type. Client should check the
 *	       capabilities for the interfaces supported by the bridge.
 * @format: Compressed vs Uncompressed formats.
 * @channels: Number of channels.
 * @i2s_fmt: I2S data packing format. This is valid only if interface is I2S.
 * @sampling_rate: sampling rate of audio data
 * @word_size: word size
 * @word_endianness: little or big endian words
 */
struct msm_dba_audio_cfg {
	enum msm_dba_audio_interface_type interface;
	enum msm_dba_audio_format_type format;
	enum msm_dba_audio_channel_count channels;
	enum msm_dba_audio_i2s_format i2s_fmt;
	enum msm_dba_audio_sampling_rates_type sampling_rate;
	enum msm_dba_audio_word_bit_depth word_size;
	enum msm_dba_audio_word_endian_type word_endianness;
	enum msm_dba_audio_copyright_type copyright;
	enum msm_dba_audio_pre_emphasis_type pre_emphasis;
	enum msm_dba_audio_clock_accuracy clock_accuracy;
	enum msm_dba_channel_status_source channel_status_source;
	enum msm_dba_audio_op_mode mode;

	u32 channel_status_category_code;
	u32 channel_status_source_number;
	u32 channel_status_v_bit;
	u32 channel_allocation;
	u32 channel_status_word_length;

	u32 n;
	u32 cts;
};

/**
 * struct msm_dba_video_cfg - video configuration data
 * @h_active: active width of the video signal
 * @h_front_porch: horizontal front porch in pixels
 * @h_pulse_width: pulse width of hsync in pixels
 * @h_back_porch: horizontal back porch in pixels
 * @h_polarity: polarity of hsync signal
 * @v_active: active height of the video signal
 * @v_front_porch: vertical front porch in lines
 * @v_pulse_width: pulse width of vsync in lines
 * @v_back_porch: vertical back porch in lines
 * @v_polarity: polarity of vsync signal
 * @pclk_khz: pixel clock in KHz
 * @interlaced: if video is interlaced
 * @vic: video indetification code
 * @hdmi_mode: hdmi or dvi mode for the sink
 * @ar: aspect ratio of the signal
 * @num_of_input_lanes: number of input lanes in case of DSI/LVDS
 */
struct msm_dba_video_cfg {
	u32  h_active;
	u32  h_front_porch;
	u32  h_pulse_width;
	u32  h_back_porch;
	bool h_polarity;
	u32  v_active;
	u32  v_front_porch;
	u32  v_pulse_width;
	u32  v_back_porch;
	bool v_polarity;
	u32  pclk_khz;
	bool interlaced;
	u32  vic;
	bool hdmi_mode;
	enum msm_dba_video_aspect_ratio ar;
	u32  num_of_input_lanes;
	u8 scaninfo;
};

struct mdss_dba_timing_info {
	u16 xres;
	u16 yres;
	u8 bpp;
	u8 fps;
	u8 lanes;
};

/**
 * struct msm_dba_ops- operation supported by bridge chip
 * @get_caps: returns the bridge chip capabilities
 *	      DEFER and ASYNC flags are not supported.
 * @power_on: powers on/off the bridge chip. This usually involves turning on
 *	      the power regulators and bringing the chip out of reset. Chip
 *	      should be capable of raising interrupts at this point.
 *	      DEFER and ASYNC flags are supported.
 * @video_on: turn on/off video stream. This function also requires the video
 *	      timing information that might be needed for programming the bridge
 *	      chip.
 *	      DEFER flag is supported.
 *	      ASYNC flag is not supported.
 * @audio_on: turn on/off audio stream.
 *	      DEFER flag is supported.
 *	      ASYNC flag is not supported.
 * @configure_audio: setup audio configuration
 *		     DEFER flag is supported.
 *		     ASYNC flag is not supported.
 * @av_mute: controls av mute functionalities if supported. AV mute is different
 *	     from audio_on and video_on where in even though the actual data is
 *	     sent, mute is specified through control packets.
 *	     DEFER flag is supported.
 *	     ASYNC flag is not supported.
 * @interupts_enable: enables interrupts to get event callbacks. Clients need
 *		      to specify an event mask of the events they are
 *		      interested in. If a client provides an event as part of
 *		      the mask, it will receive the interrupt regardless of the
 *		      client modifying the property.
 *		      DEFER flag is supported.
 *		      ASYNC flag is not supported.
 * @hdcp_enable: enable/disable hdcp. If HDCP is enabled, this function will
 *		 start a new authentication session. There is a separate
 *		 argument for enabling encryption. Encryption can be enabled any
 *		 time after HDCP has been fully authenticated. This function
 *		 will support an asynchronous mode where calling this function
 *		 will kick off HDCP and return to the caller. Caller has to wait
 *		 for MSM_DBA_CB_HDCP_SUCCESS callback to ensure link is
 *		 authenticated.
 *		 DEFER flag is not supported.
 *		 ASYNC flag is supported.
 * @hdcp_get_ksv_list_size: returns the KSV list size. In case of a simple sink
 *			    the size will be 1. In case of a repeater, this can
 *			    be more than one.
 *			    DEFER and ASYNC flags are not supported.
 * @hdcp_get_ksv_list: return the KSV list. Client can query the KSV information
 *		       from the bridge. Client should call
 *		       hdcp_get_ksv_list_size first and then allocate 40*size
 *		       bytes to hold all the KSVs.
 *		       DEFER and ASYNC flags are not supported.
 * @hdmi_cec_on: enable or disable cec module. Clients need to enable CEC
 *		 feature before they do read or write CEC messages.
 * @hdmi_cec_write: perform a CEC write. For bridges with HDMI as output
 *		    interface, this function allows clients to send a CEC
 *		    message. Client should pack the data according to the CEC
 *		    specification and provide the final buffer. Since CEC writes
 *		    can take longer time to ascertaining if they are successful,
 *		    this function supports the ASYNC flag. Driver will return
 *		    either MSM_DBA_CB_CEC_WRITE_SUCCESS or
 *		    MSM_DBA_CB_CEC_WRITE_FAIL callbacks.
 *		    DEFER is not supported.
 *		    ASYNC flag is supported.
 * @hdmi_cec_read: get a pending CEC read message. In case of an incoming CEC
 *		   message, driver will return MSM_DBA_CB_CEC_READ_PENDING
 *		   callback. On getting this event callback, client should call
 *		   hdmi_cec_read to get the message. The buffer should at least
 *		   be 15 bytes or more. Client should read the CEC message from
 *		   a thread different from the callback.
 *		   DEFER and ASYNC flags are not supported.
 * @get_edid_size: returns size of the edid.
 *		   DEFER and ASYNC flags are not supported.
 * @get_raw_edid: returns raw edid data.
 *		   DEFER and ASYNC flags are not supported.
 * @enable_remote_comm: enable/disable remote communication. Some interfaces
 *		        like FPDLINK III support a bi-directional control
 *		        channel that could be used to send control data using an
 *		        I2C or SPI protocol. This Function will enable this
 *		        control channel if supported.
 *		        DEFER and ASYNC flags are not supported.
 * @add_remote_device: add slaves on remote side for enabling communication. For
 *		       interfaces that support bi directional control channel,
 *		       this function allows clients to specify slave IDs of
 *		       devices on remote bus. Messages addressed to these IDs
 *		       will be trapped by the bridge chip and put on the remote
 *		       bus.
 *		       DEFER and ASYNC flags are not supported.
 * @commit_deferred_props: commits deferred properties
 *			   DEFER and ASYNC flags are not supported.
 * @force_reset: reset the device forcefully. In case the device goes into a bad
 *		 state, a client can force reset to try and recover the device.
 *		 The reset will be applied in spite of different configurations
 *		 from other clients. Driver will apply all the properties that
 *		 have been applied so far after the reset is complete. In case
 *		 of multiple clients, driver will issue a reset callback.
 * @dump_debug_info: dumps debug information to dmesg.
 * @check_hpd: Check if cable is connected or not. if cable is connected we
 *		send notification to display framework.
 * @set_audio_block: This function will populate the raw audio speaker block
 *		     data along with size of each block in bridgechip buffer.
 * @get_audio_block: This function will return the raw audio speaker block
 *		     along with size of each block.
 *
 * The msm_dba_ops structure represents a set of operations that can be
 * supported by each bridge chip. Depending on the functionality supported by a
 * specific bridge chip, some of the operations need not be supported. For
 * example if a bridge chip does not support reading EDID from a sink device,
 * get_edid_size and get_raw_edid can be NULL.
 *
 * Deferring properties: The deferred flag allows us to address any quirks with
 * respect to specific bridge chips. If there is a need for some properties to
 * be committed together, turning on video and audio at the same time, the
 * deferred flag can be used. Properties that are set using a DEFER flag will
 * not be committed to hardware until commit_deferred_props() function is
 * called.
 *
 */
struct msm_dba_ops {
	int (*get_caps)(void *client,
			struct msm_dba_capabilities *caps);

	int (*power_on)(void *client,
			bool on,
			u32 flags);

	int (*video_on)(void *client,
			bool on,
			struct msm_dba_video_cfg *cfg,
			u32 flags);

	int (*audio_on)(void *client,
			bool on,
			u32 flags);

	int (*configure_audio)(void *client,
			       struct msm_dba_audio_cfg *cfg,
			       u32 flags);

	int (*av_mute)(void *client,
		       bool video_mute,
		       bool audio_mute,
		       u32 flags);

	int (*interrupts_enable)(void *client,
				bool on,
				u32 event_mask,
				u32 flags);

	int (*hdcp_enable)(void *client,
			   bool hdcp_on,
			   bool enc_on,
			   u32 flags);

	int (*hdcp_get_ksv_list_size)(void *client,
				      u32 *count,
				      u32 flags);

	int (*hdcp_get_ksv_list)(void *client,
				 u32 count,
				 char *buf,
				 u32 flags);

	int (*hdmi_cec_on)(void *client,
			      bool enable,
			      u32 flags);

	int (*hdmi_cec_write)(void *client,
			      u32 size,
			      char *buf,
			      u32 flags);

	int (*hdmi_cec_read)(void *client,
			     u32 *size,
			     char *buf,
			     u32 flags);

	int (*get_edid_size)(void *client,
			     u32 *size,
			     u32 flags);

	int (*get_raw_edid)(void *client,
			    u32 size,
			    char *buf,
			    u32 flags);

	int (*enable_remote_comm)(void *client,
				  bool on,
				  u32 flags);

	int (*add_remote_device)(void *client,
				 u32 *slave_ids,
				 u32 count,
				 u32 flags);

	int (*commit_deferred_props)(void *client,
				    u32 flags);

	int (*force_reset)(void *client, u32 flags);
	int (*dump_debug_info)(void *client, u32 flags);
	int (*check_hpd)(void *client, u32 flags);
	void (*set_audio_block)(void *client, u32 size, void *buf);
	void (*get_audio_block)(void *client, u32 size, void *buf);
	void* (*get_supp_timing_info)(void);
};

/**
 * msm_dba_register_client() - Allows a client to register with the driver.
 * @info: Client information along with the bridge chip id the client wishes to
 *	  program.
 * @ops: Function pointers to bridge chip operations. Some function pointers can
 *	 be NULL depending on the functionalities supported by bridge chip.
 *
 * The register API supports multiple clients to register for the same bridge
 * chip. If Successful, this will return a pointer that should be used as a
 * handle for all subsequent function calls.
 */
void *msm_dba_register_client(struct msm_dba_reg_info *info,
			      struct msm_dba_ops *ops);

/**
 * msm_dba_deregister_client() - Allows client to de-register with the driver.
 * @client: client handle returned by register API.
 *
 * This function will release all the resources used by a particular client. If
 * it is the only client using the bridge chip, the bridge chip will be powered
 * down and put into reset.
 */
int msm_dba_deregister_client(void *client);

#endif /* _MSM_DBA_H */
