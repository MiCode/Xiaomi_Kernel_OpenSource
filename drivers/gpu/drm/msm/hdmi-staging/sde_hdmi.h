/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#ifndef _SDE_HDMI_H_
#define _SDE_HDMI_H_

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/of_device.h>
#include <linux/msm_ext_display.h>
#include <linux/hdcp_qseecom.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <media/cec-notifier.h>
#include "hdmi.h"
#include "sde_kms.h"
#include "sde_connector.h"
#include "msm_drv.h"
#include "sde_edid_parser.h"
#include "sde_hdmi_util.h"
#include "sde_hdcp.h"

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif
#ifdef HDMI_DEBUG_ENABLE
#define SDE_HDMI_DEBUG(fmt, args...)   SDE_ERROR(fmt, ##args)
#else
#define SDE_HDMI_DEBUG(fmt, args...)   SDE_DEBUG(fmt, ##args)
#endif

/* HW Revisions for different SDE targets */
#define SDE_GET_MAJOR_VER(rev)((rev) >> 28)
#define SDE_GET_MINOR_VER(rev)(((rev) >> 16) & 0xFFF)

/**
 * struct sde_hdmi_info - defines hdmi display properties
 * @display_type:      Display type as defined by device tree.
 * @is_hot_pluggable:  Can panel be hot plugged.
 * @is_connected:      Is panel connected.
 * @is_edid_supported: Does panel support reading EDID information.
 * @width_mm:          Physical width of panel in millimeters.
 * @height_mm:         Physical height of panel in millimeters.
 */
struct sde_hdmi_info {
	const char *display_type;

	/* HPD */
	bool is_hot_pluggable;
	bool is_connected;
	bool is_edid_supported;

	/* Physical properties */
	u32 width_mm;
	u32 height_mm;
};

/**
 * struct sde_hdmi_ctrl - hdmi ctrl/phy information for the display
 * @ctrl:           Handle to the HDMI controller device.
 * @ctrl_of_node:   pHandle to the HDMI controller device.
 * @hdmi_ctrl_idx:   HDMI controller instance id.
 */
struct sde_hdmi_ctrl {
	/* controller info */
	struct hdmi *ctrl;
	struct device_node *ctrl_of_node;
	u32 hdmi_ctrl_idx;
};

enum hdmi_tx_io_type {
	HDMI_TX_CORE_IO,
	HDMI_TX_QFPROM_IO,
	HDMI_TX_HDCP_IO,
	HDMI_TX_MAX_IO
};

enum hdmi_tx_feature_type {
	SDE_HDCP_1x,
	SDE_HDCP_2P2
};

/**
 * struct sde_hdmi - hdmi display information
 * @pdev:             Pointer to platform device.
 * @drm_dev:          DRM device associated with the display.
 * @name:             Name of the display.
 * @display_type:     Display type as defined in device tree.
 * @list:             List pointer.
 * @display_lock:     Mutex for sde_hdmi interface.
 * @ctrl:             Controller information for HDMI display.
 * @non_pluggable:    If HDMI display is non pluggable
 * @num_of_modes:     Number of modes supported by display if non pluggable.
 * @mode_list:        Mode list if non pluggable.
 * @mode:             Current display mode.
 * @connected:        If HDMI display is connected.
 * @is_tpg_enabled:   TPG state.
 * @hdmi_tx_version:  HDMI TX version
 * @hdmi_tx_major_version: HDMI TX major version
 * @max_pclk_khz: Max pixel clock supported
 * @hdcp1_use_sw_keys: If HDCP1 engine uses SW keys
 * @hdcp14_present: If the sink supports HDCP 1.4
 * @hdcp22_present: If the sink supports HDCP 2.2
 * @hdcp_status: Current HDCP status
 * @sink_hdcp_ver: HDCP version of the sink
 * @enc_lvl: Current encryption level
 * @curr_hdr_state: Current HDR state of the HDMI connector
 * @auth_state: Current authentication state of HDCP
 * @sink_hdcp22_support: If the sink supports HDCP 2.2
 * @src_hdcp22_support: If the source supports HDCP 2.2
 * @hdcp_data: Call back data registered by the client with HDCP lib
 * @hdcp_feat_data: Handle to HDCP feature data
 * @hdcp_ops: Function ops registered by the client with the HDCP lib
 * @ddc_ctrl: Handle to HDMI DDC Controller
 * @hpd_work:         HPD work structure.
 * @codec_ready:      If audio codec is ready.
 * @client_notify_pending: If there is client notification pending.
 * @irq_domain:       IRQ domain structure.
 * @notifier:         CEC notifider to convey physical address information.
 * @pll_update_enable: if it's allowed to update HDMI PLL ppm.
 * @dc_enable:        If deep color is enabled. Only DC_30 so far.
 * @dc_feature_supported: If deep color feature is supported.
 * @bt2020_colorimetry: If BT2020 colorimetry is supported by sink
 * @hdcp_cb_work: Callback function for HDCP
 * @io: Handle to IO base addresses for HDMI
 * @root:             Debug fs root entry.
 */
struct sde_hdmi {
	struct platform_device *pdev;
	struct drm_device *drm_dev;

	const char *name;
	const char *display_type;
	struct list_head list;
	struct mutex display_lock;
	struct mutex hdcp_mutex;
	struct sde_hdmi_ctrl ctrl;

	struct platform_device *ext_pdev;
	struct msm_ext_disp_init_data ext_audio_data;
	struct sde_edid_ctrl *edid_ctrl;

	bool non_pluggable;
	bool skip_ddc;
	u32 num_of_modes;
	struct list_head mode_list;
	struct drm_display_mode mode;
	bool connected;
	bool is_tpg_enabled;
	u32 hdmi_tx_version;
	u32 hdmi_tx_major_version;
	u32 max_pclk_khz;
	bool hdcp1_use_sw_keys;
	u32 hdcp14_present;
	u32 hdcp22_present;
	u8 hdcp_status;
	u8 sink_hdcp_ver;
	u32 enc_lvl;
	u8 curr_hdr_state;
	bool auth_state;
	bool sink_hdcp22_support;
	bool src_hdcp22_support;

	/*hold final data
	 *based on hdcp support
	 */
	void *hdcp_data;
	/*hold hdcp init data*/
	void *hdcp_feat_data[2];
	struct sde_hdcp_ops *hdcp_ops;
	struct sde_hdmi_tx_ddc_ctrl ddc_ctrl;
	struct work_struct hpd_work;
	bool codec_ready;
	bool client_notify_pending;

	struct irq_domain *irq_domain;
	struct cec_notifier *notifier;
	bool pll_update_enable;
	bool dc_enable;
	bool dc_feature_supported;
	bool bt2020_colorimetry;

	struct delayed_work hdcp_cb_work;
	struct dss_io_data io[HDMI_TX_MAX_IO];
	/* DEBUG FS */
	struct dentry *root;

	bool cont_splash_enabled;
};

/**
 * hdmi_tx_scdc_access_type() - hdmi 2.0 DDC functionalities.
 */
enum hdmi_tx_scdc_access_type {
	HDMI_TX_SCDC_SCRAMBLING_STATUS,
	HDMI_TX_SCDC_SCRAMBLING_ENABLE,
	HDMI_TX_SCDC_TMDS_BIT_CLOCK_RATIO_UPDATE,
	HDMI_TX_SCDC_CLOCK_DET_STATUS,
	HDMI_TX_SCDC_CH0_LOCK_STATUS,
	HDMI_TX_SCDC_CH1_LOCK_STATUS,
	HDMI_TX_SCDC_CH2_LOCK_STATUS,
	HDMI_TX_SCDC_CH0_ERROR_COUNT,
	HDMI_TX_SCDC_CH1_ERROR_COUNT,
	HDMI_TX_SCDC_CH2_ERROR_COUNT,
	HDMI_TX_SCDC_READ_ENABLE,
	HDMI_TX_SCDC_MAX,
};

#define HDMI_KHZ_TO_HZ 1000
#define HDMI_MHZ_TO_HZ 1000000
#define HDMI_YUV420_24BPP_PCLK_TMDS_CH_RATE_RATIO 2
#define HDMI_RGB_24BPP_PCLK_TMDS_CH_RATE_RATIO 1

#define HDMI_GEN_PKT_CTRL_CLR_MASK 0x7

/* for AVI program */
#define HDMI_AVI_INFOFRAME_BUFFER_SIZE \
	(HDMI_INFOFRAME_HEADER_SIZE + HDMI_AVI_INFOFRAME_SIZE)
#define HDMI_VS_INFOFRAME_BUFFER_SIZE (HDMI_INFOFRAME_HEADER_SIZE + 6)

#define LEFT_SHIFT_BYTE(x) ((x) << 8)
#define LEFT_SHIFT_WORD(x) ((x) << 16)
#define LEFT_SHIFT_24BITS(x) ((x) << 24)

/* Maximum pixel clock rates for hdmi tx */
#define HDMI_DEFAULT_MAX_PCLK_RATE	148500
#define HDMI_TX_3_MAX_PCLK_RATE		297000
#define HDMI_TX_4_MAX_PCLK_RATE		600000
/**
 * hdmi_tx_ddc_timer_type() - hdmi DDC timer functionalities.
 */
enum hdmi_tx_ddc_timer_type {
	HDMI_TX_DDC_TIMER_HDCP2P2_RD_MSG,
	HDMI_TX_DDC_TIMER_SCRAMBLER_STATUS,
	HDMI_TX_DDC_TIMER_UPDATE_FLAGS,
	HDMI_TX_DDC_TIMER_STATUS_FLAGS,
	HDMI_TX_DDC_TIMER_CED,
	HDMI_TX_DDC_TIMER_MAX,
	};

#ifdef CONFIG_DRM_SDE_HDMI
/**
 * sde_hdmi_get_num_of_displays() - returns number of display devices
 *				       supported.
 *
 * Return: number of displays.
 */
u32 sde_hdmi_get_num_of_displays(void);

/**
 * sde_hdmi_get_displays() - returns the display list that's available.
 * @display_array: Pointer to display list
 * @max_display_count: Number of maximum displays in the list
 *
 * Return: number of available displays.
 */
int sde_hdmi_get_displays(void **display_array, u32 max_display_count);

/**
 * sde_hdmi_connector_pre_deinit()- perform additional deinitialization steps
 * @connector: Pointer to drm connector structure
 * @display: Pointer to private display handle
 *
 * Return: error code
 */
int sde_hdmi_connector_pre_deinit(struct drm_connector *connector,
		void *display);

/**
 * sde_hdmi_connector_post_init()- perform additional initialization steps
 * @connector: Pointer to drm connector structure
 * @info: Pointer to sde connector info structure
 * @display: Pointer to private display handle
 *
 * Return: error code
 */
int sde_hdmi_connector_post_init(struct drm_connector *connector,
		void *info,
		void *display);

/**
 * sde_hdmi_connector_detect()- determine if connector is connected
 * @connector: Pointer to drm connector structure
 * @force: Force detect setting from drm framework
 * @display: Pointer to private display handle
 *
 * Return: error code
 */
enum drm_connector_status
sde_hdmi_connector_detect(struct drm_connector *connector,
		bool force,
		void *display);

/**
 * sde_hdmi_core_enable()- turn on clk and pwr for hdmi core
 * @sde_hdmi: Pointer to sde_hdmi structure
 *
 * Return: error code
 */
int sde_hdmi_core_enable(struct sde_hdmi *sde_hdmi);

/**
 * sde_hdmi_core_disable()- turn off clk and pwr for hdmi core
 * @sde_hdmi: Pointer to sde_hdmi structure
 *
 * Return: none
 */
void sde_hdmi_core_disable(struct sde_hdmi *sde_hdmi);

/**
 * sde_hdmi_connector_get_modes - add drm modes via drm_mode_probed_add()
 * @connector: Pointer to drm connector structure
 * @display: Pointer to private display handle

 * Returns: Number of modes added
 */
int sde_hdmi_connector_get_modes(struct drm_connector *connector,
		void *display);

/**
 * sde_hdmi_mode_valid - determine if specified mode is valid
 * @connector: Pointer to drm connector structure
 * @mode: Pointer to drm mode structure
 * @display: Pointer to private display handle
 *
 * Returns: Validity status for specified mode
 */
enum drm_mode_status sde_hdmi_mode_valid(struct drm_connector *connector,
		struct drm_display_mode *mode,
		void *display);

/**
 * sde_hdmi_dev_init() - Initializes the display device
 * @display:         Handle to the display.
 *
 * Initialization will acquire references to the resources required for the
 * display hardware to function.
 *
 * Return: error code.
 */
int sde_hdmi_dev_init(struct sde_hdmi *display);

/**
 * sde_hdmi_dev_deinit() - Desinitializes the display device
 * @display:        Handle to the display.
 *
 * All the resources acquired during device init will be released.
 *
 * Return: error code.
 */
int sde_hdmi_dev_deinit(struct sde_hdmi *display);

/**
 * sde_hdmi_drm_init() - initializes DRM objects for the display device.
 * @display:            Handle to the display.
 * @encoder:            Pointer to the encoder object which is connected to the
 *			display.
 *
 * Return: error code.
 */
int sde_hdmi_drm_init(struct sde_hdmi *display,
				struct drm_encoder *enc);

/**
 * sde_hdmi_drm_deinit() - destroys DRM objects assosciated with the display
 * @display:        Handle to the display.
 *
 * Return: error code.
 */
int sde_hdmi_drm_deinit(struct sde_hdmi *display);

/**
 * sde_hdmi_get_info() - returns the display properties
 * @display:          Handle to the display.
 * @info:             Pointer to the structure where info is stored.
 *
 * Return: error code.
 */
int sde_hdmi_get_info(struct msm_display_info *info,
				void *display);

/**
 * sde_hdmi_set_property() - set the connector properties
 * @connector:        Handle to the connector.
 * @state:            Handle to the connector state.
 * @property_index:   property index.
 * @value:            property value.
 * @display:          Handle to the display.
 *
 * Return: error code.
 */
int sde_hdmi_set_property(struct drm_connector *connector,
			struct drm_connector_state *state,
			int property_index,
			uint64_t value,
			void *display);
/**
 * sde_hdmi_bridge_power_on -- A wrapper of _sde_hdmi_bridge_power_on.
 * @bridge:          Handle to the drm bridge.
 *
 * Return: void.
 */
void sde_hdmi_bridge_power_on(struct drm_bridge *bridge);

/**
 * sde_hdmi_get_property() - get the connector properties
 * @connector:        Handle to the connector.
 * @state:            Handle to the connector state.
 * @property_index:   property index.
 * @value:            property value.
 * @display:          Handle to the display.
 *
 * Return: error code.
 */
int sde_hdmi_get_property(struct drm_connector *connector,
			struct drm_connector_state *state,
			int property_index,
			uint64_t *value,
			void *display);

/**
 * sde_hdmi_bridge_init() - init sde hdmi bridge
 * @hdmi:          Handle to the hdmi.
 * @display:       Handle to the sde_hdmi
 *
 * Return: struct drm_bridge *.
 */
struct drm_bridge *sde_hdmi_bridge_init(struct hdmi *hdmi,
			struct sde_hdmi *display);

/**
 * sde_hdmi_set_mode() - Set HDMI mode API.
 * @hdmi:          Handle to the hdmi.
 * @power_on:      Power on/off request.
 *
 * Return: void.
 */
void sde_hdmi_set_mode(struct hdmi *hdmi, bool power_on);

/**
 * sde_hdmi_scdc_read() - hdmi 2.0 ddc read API.
 * @hdmi:          Handle to the hdmi.
 * @data_type:     DDC data type, refer to enum hdmi_tx_scdc_access_type.
 * @val:           Read back value.
 *
 * Return: error code.
 */
int sde_hdmi_scdc_read(struct hdmi *hdmi, u32 data_type, u32 *val);

/**
 * sde_hdmi_scdc_write() - hdmi 2.0 ddc write API.
 * @hdmi:          Handle to the hdmi.
 * @data_type:     DDC data type, refer to enum hdmi_tx_scdc_access_type.
 * @val:           Value write through DDC.
 *
 * Return: error code.
 */
int sde_hdmi_scdc_write(struct hdmi *hdmi, u32 data_type, u32 val);

/**
 * sde_hdmi_audio_on() - enable hdmi audio.
 * @hdmi:          Handle to the hdmi.
 * @params:        audio setup parameters from codec.
 *
 * Return: error code.
 */
int sde_hdmi_audio_on(struct hdmi *hdmi,
	struct msm_ext_disp_audio_setup_params *params);

/**
 * sde_hdmi_audio_off() - disable hdmi audio.
 * @hdmi:          Handle to the hdmi.
 *
 * Return: void.
 */
void sde_hdmi_audio_off(struct hdmi *hdmi);

/**
 * sde_hdmi_config_avmute() - mute hdmi.
 * @hdmi:          Handle to the hdmi.
 * @set:           enable/disable avmute.
 *
 * Return: error code.
 */
int sde_hdmi_config_avmute(struct hdmi *hdmi, bool set);

/**
 * sde_hdmi_notify_clients() - notify hdmi clients of the connection status.
 * @display:       Handle to sde_hdmi.
 * @connected:     connection status.
 *
 * Return: void.
 */
void sde_hdmi_notify_clients(struct sde_hdmi *display, bool connected);

/**
 * sde_hdmi_ack_state() - acknowledge the connection status.
 * @connector:     Handle to the drm_connector.
 * @status:        connection status.
 *
 * Return: void.
 */
void sde_hdmi_ack_state(struct drm_connector *connector,
	enum drm_connector_status status);

bool sde_hdmi_tx_is_hdcp_enabled(struct sde_hdmi *hdmi_ctrl);
bool sde_hdmi_tx_is_encryption_set(struct sde_hdmi *hdmi_ctrl);
bool sde_hdmi_tx_is_stream_shareable(struct sde_hdmi *hdmi_ctrl);
bool sde_hdmi_tx_is_panel_on(struct sde_hdmi *hdmi_ctrl);
int sde_hdmi_start_hdcp(struct drm_connector *connector);
void sde_hdmi_hdcp_off(struct sde_hdmi *hdmi_ctrl);


/*
 * sde_hdmi_pre_kickoff - program kickoff-time features
 * @display: Pointer to private display structure
 * @params: Parameters for kickoff-time programming
 * Returns: Zero on success
 */
int sde_hdmi_pre_kickoff(struct drm_connector *connector,
		void *display,
		struct msm_display_kickoff_params *params);

/*
 * sde_hdmi_mode_needs_full_range - does mode need full range
 * quantization
 * @display: Pointer to private display structure
 * Returns: true or false based on mode
 */
bool sde_hdmi_mode_needs_full_range(void *display);

/*
 * sde_hdmi_get_csc_type - returns the CSC type to be
 * used based on state of HDR playback
 * @conn: Pointer to DRM connector
 * @display: Pointer to private display structure
 * Returns: true or false based on mode
 */
enum sde_csc_type sde_hdmi_get_csc_type(struct drm_connector *conn,
	void *display);
#else /*#ifdef CONFIG_DRM_SDE_HDMI*/

static inline u32 sde_hdmi_get_num_of_displays(void)
{
	return 0;
}

static inline int sde_hdmi_get_displays(void **display_array,
		u32 max_display_count)
{
	return 0;
}

static inline int sde_hdmi_connector_pre_deinit(struct drm_connector *connector,
		void *display)
{
	return 0;
}

static inline int sde_hdmi_connector_post_init(struct drm_connector *connector,
		void *info,
		void *display)
{
	return 0;
}

static inline enum drm_connector_status
sde_hdmi_connector_detect(struct drm_connector *connector,
		bool force,
		void *display)
{
	return connector_status_disconnected;
}

static inline int sde_hdmi_connector_get_modes(struct drm_connector *connector,
		void *display)
{
	return 0;
}

static inline enum drm_mode_status sde_hdmi_mode_valid(
		struct drm_connector *connector,
		struct drm_display_mode *mode,
		void *display)
{
	return MODE_OK;
}

static inline int sde_hdmi_dev_init(struct sde_hdmi *display)
{
	return 0;
}

static inline int sde_hdmi_dev_deinit(struct sde_hdmi *display)
{
	return 0;
}

bool hdmi_tx_is_hdcp_enabled(struct sde_hdmi *hdmi_ctrl)
{
	return false;
}

bool sde_hdmi_tx_is_encryption_set(struct sde_hdmi *hdmi_ctrl)
{
	return false;
}

bool sde_hdmi_tx_is_stream_shareable(struct sde_hdmi *hdmi_ctrl)
{
	return false;
}

bool sde_hdmi_tx_is_panel_on(struct sde_hdmi *hdmi_ctrl)
{
	return false;
}

static inline int sde_hdmi_drm_init(struct sde_hdmi *display,
				struct drm_encoder *enc)
{
	return 0;
}

int sde_hdmi_start_hdcp(struct drm_connector *connector)
{
	return 0;
}

void sde_hdmi_hdcp_off(struct sde_hdmi *hdmi_ctrl)
{

}

static inline int sde_hdmi_drm_deinit(struct sde_hdmi *display)
{
	return 0;
}

static inline int sde_hdmi_get_info(struct msm_display_info *info,
				void *display)
{
	return 0;
}

static inline int sde_hdmi_set_property(struct drm_connector *connector,
			struct drm_connector_state *state,
			int property_index,
			uint64_t value,
			void *display)
{
	return 0;
}

static inline bool sde_hdmi_mode_needs_full_range(void *display)
{
	return false;
}

enum sde_csc_type sde_hdmi_get_csc_type(struct drm_connector *conn,
	void *display)
{
	return 0;
}

#endif /*#else of CONFIG_DRM_SDE_HDMI*/
#endif /* _SDE_HDMI_H_ */
