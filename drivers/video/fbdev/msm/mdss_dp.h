/* Copyright (c) 2012-2014, 2016 The Linux Foundation. All rights reserved.
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

#ifndef MDSS_DP_H
#define MDSS_DP_H

#include <linux/list.h>
#include <linux/mdss_io_util.h>
#include <linux/irqreturn.h>
#include <linux/pinctrl/consumer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/usb/usbpd.h>
#include <linux/switch.h>

#include "mdss_hdmi_util.h"
#include "mdss_hdmi_edid.h"
#include "video/msm_hdmi_modes.h"
#include "mdss.h"
#include "mdss_panel.h"

#define dp_read(offset) readl_relaxed((offset))
#define dp_write(offset, data) writel_relaxed((data), (offset))

#define AUX_CMD_FIFO_LEN	144
#define AUX_CMD_MAX		16
#define AUX_CMD_I2C_MAX		128

#define EDP_PORT_MAX		1
#define EDP_SINK_CAP_LEN	16

/* 4 bits of aux command */
#define EDP_CMD_AUX_WRITE	0x8
#define EDP_CMD_AUX_READ	0x9

/* 4 bits of i2c command */
#define EDP_CMD_I2C_MOT		0x4	/* i2c middle of transaction */
#define EDP_CMD_I2C_WRITE	0x0
#define EDP_CMD_I2C_READ	0x1
#define EDP_CMD_I2C_STATUS	0x2	/* i2c write status request */

/* cmd reply: bit 0, 1 for aux */
#define EDP_AUX_ACK		0x0
#define EDP_AUX_NACK	0x1
#define EDP_AUX_DEFER	0x2

/* cmd reply: bit 2, 3 for i2c */
#define EDP_I2C_ACK		0x0
#define EDP_I2C_NACK	0x4
#define EDP_I2C_DEFER	0x8

#define EDP_CMD_TIMEOUT	400	/* us */
#define EDP_CMD_LEN		16

#define EDP_INTR_ACK_SHIFT	1
#define EDP_INTR_MASK_SHIFT	2

/* isr */
#define EDP_INTR_HPD		BIT(0)
#define EDP_INTR_AUX_I2C_DONE	BIT(3)
#define EDP_INTR_WRONG_ADDR	BIT(6)
#define EDP_INTR_TIMEOUT	BIT(9)
#define EDP_INTR_NACK_DEFER	BIT(12)
#define EDP_INTR_WRONG_DATA_CNT	BIT(15)
#define EDP_INTR_I2C_NACK	BIT(18)
#define EDP_INTR_I2C_DEFER	BIT(21)
#define EDP_INTR_PLL_UNLOCKED	BIT(24)
#define EDP_INTR_AUX_ERROR	BIT(27)


#define EDP_INTR_STATUS1 \
	(EDP_INTR_AUX_I2C_DONE| \
	EDP_INTR_WRONG_ADDR | EDP_INTR_TIMEOUT | \
	EDP_INTR_NACK_DEFER | EDP_INTR_WRONG_DATA_CNT | \
	EDP_INTR_I2C_NACK | EDP_INTR_I2C_DEFER | \
	EDP_INTR_PLL_UNLOCKED | EDP_INTR_AUX_ERROR)

#define EDP_INTR_MASK1		(EDP_INTR_STATUS1 << 2)


#define EDP_INTR_READY_FOR_VIDEO	BIT(0)
#define EDP_INTR_IDLE_PATTERNs_SENT	BIT(3)
#define EDP_INTR_FRAME_END		BIT(6)
#define EDP_INTR_CRC_UPDATED		BIT(9)

#define EDP_INTR_STATUS2 \
	(EDP_INTR_READY_FOR_VIDEO | EDP_INTR_IDLE_PATTERNs_SENT | \
	EDP_INTR_FRAME_END | EDP_INTR_CRC_UPDATED)

#define EDP_INTR_MASK2		(EDP_INTR_STATUS2 << 2)
#define DP_ENUM_STR(x)		#x

struct edp_buf {
	char *start;	/* buffer start addr */
	char *end;	/* buffer end addr */
	int size;	/* size of buffer */
	char *data;	/* data pointer */
	int len;	/* dara length */
	char trans_num;	/* transaction number */
	char i2c;	/* 1 == i2c cmd, 0 == native cmd */
};

/* USBPD-TypeC specific Macros */
#define VDM_VERSION		0x0
#define USB_C_DP_SID		0xFF01

enum dp_pm_type {
	DP_CORE_PM,
	DP_CTRL_PM,
	DP_PHY_PM,
	DP_MAX_PM
};

#define PIN_ASSIGN_A		BIT(0)
#define PIN_ASSIGN_B		BIT(1)
#define PIN_ASSIGN_C		BIT(2)
#define PIN_ASSIGN_D		BIT(3)
#define PIN_ASSIGN_E		BIT(4)
#define PIN_ASSIGN_F		BIT(5)

#define SVDM_HDR(svid, ver, mode, cmd_type, cmd) \
	(((svid) << 16) | (1 << 15) | ((ver) <<  13) \
	| ((mode) << 8) | ((cmd_type) << 6) | (cmd))

/* DP specific VDM commands */
#define DP_VDM_STATUS		0x10
#define DP_VDM_CONFIGURE	0x11

enum dp_port_cap {
	PORT_NONE = 0,
	PORT_UFP_D,
	PORT_DFP_D,
	PORT_D_UFP_D,
};

struct usbpd_dp_capabilities {
	u32 response;
	enum dp_port_cap s_port;
	bool receptacle_state;
	u8 ulink_pin_config;
	u8 dlink_pin_config;
};

struct usbpd_dp_status {
	u32 response;
	enum dp_port_cap c_port;
	bool low_pow_st;
	bool adaptor_dp_en;
	bool multi_func;
	bool switch_to_usb_config;
	bool exit_dp_mode;
	bool hpd_high;
	bool hpd_irq;
};

enum dp_alt_mode_state {
	UNKNOWN_STATE       = 0,
	ALT_MODE_INIT_STATE = BIT(0),
	DISCOVER_MODES_DONE = BIT(1),
	ENTER_MODE_DONE     = BIT(2),
	DP_STATUS_DONE      = BIT(3),
	DP_CONFIGURE_DONE   = BIT(4),
};

struct dp_alt_mode {
	struct usbpd_dp_capabilities dp_cap;
	struct usbpd_dp_status dp_status;
	u32 usbpd_dp_config;
	enum dp_alt_mode_state current_state;
};

#define DPCD_ENHANCED_FRAME	BIT(0)
#define DPCD_TPS3	BIT(1)
#define DPCD_MAX_DOWNSPREAD_0_5	BIT(2)
#define DPCD_NO_AUX_HANDSHAKE	BIT(3)
#define DPCD_PORT_0_EDID_PRESENTED	BIT(4)

/* event */
#define EV_EDP_AUX_SETUP		BIT(0)
#define EV_EDID_READ			BIT(1)
#define EV_DPCD_CAP_READ		BIT(2)
#define EV_DPCD_STATUS_READ		BIT(3)
#define EV_LINK_TRAIN			BIT(4)
#define EV_IDLE_PATTERNS_SENT		BIT(5)
#define EV_VIDEO_READY			BIT(6)

#define EV_USBPD_DISCOVER_MODES		BIT(7)
#define EV_USBPD_ENTER_MODE		BIT(8)
#define EV_USBPD_DP_STATUS		BIT(9)
#define EV_USBPD_DP_CONFIGURE		BIT(10)
#define EV_USBPD_CC_PIN_POLARITY	BIT(11)
#define EV_USBPD_EXIT_MODE		BIT(12)
#define EV_USBPD_ATTENTION		BIT(13)

/* dp state ctrl */
#define ST_TRAIN_PATTERN_1		BIT(0)
#define ST_TRAIN_PATTERN_2		BIT(1)
#define ST_TRAIN_PATTERN_3		BIT(2)
#define ST_TRAIN_PATTERN_4		BIT(3)
#define ST_SYMBOL_ERR_RATE_MEASUREMENT	BIT(4)
#define ST_PRBS7			BIT(5)
#define ST_CUSTOM_80_BIT_PATTERN	BIT(6)
#define ST_SEND_VIDEO			BIT(7)
#define ST_PUSH_IDLE			BIT(8)

/* sink power state  */
#define SINK_POWER_ON		1
#define SINK_POWER_OFF		2

#define DP_LINK_RATE_162	6	/* 1.62G = 270M * 6 */
#define DP_LINK_RATE_270	10	/* 2.70G = 270M * 10 */
#define DP_LINK_RATE_540	20	/* 5.40G = 270M * 20 */
#define DP_LINK_RATE_MAX	DP_LINK_RATE_540

#define DP_LINK_RATE_MULTIPLIER	27000000
#define DP_KHZ_TO_HZ            1000
#define DP_MAX_PIXEL_CLK_KHZ	675000
struct downstream_port_config {
	/* Byte 02205h */
	bool dfp_present;
	u32 dfp_type;
	bool format_conversion;
	bool detailed_cap_info_available;
	/* Byte 02207h */
	u32 dfp_count;
	bool msa_timing_par_ignored;
	bool oui_support;
};

struct dpcd_cap {
	char major;
	char minor;
	char max_lane_count;
	char num_rx_port;
	char i2c_speed_ctrl;
	char scrambler_reset;
	char enhanced_frame;
	u32 max_link_rate;  /* 162, 270 and 540 Mb, divided by 10 */
	u32 flags;
	u32 rx_port0_buf_size;
	u32 training_read_interval;/* us */
	struct downstream_port_config downstream_port;
};

struct dpcd_link_status {
	char lane_01_status;
	char lane_23_status;
	char interlane_align_done;
	char downstream_port_status_changed;
	char link_status_updated;
	char port_0_in_sync;
	char port_1_in_sync;
	char req_voltage_swing[4];
	char req_pre_emphasis[4];
};

struct dpcd_test_request {
	u32 test_requested;
	u32 test_link_rate;
	u32 test_lane_count;
	u32 phy_test_pattern_sel;
	u32 response;
};

struct dpcd_sink_count {
	u32 count;
	bool cp_ready;
};

struct display_timing_desc {
	u32 pclk;
	u32 h_addressable; /* addressable + boder = active */
	u32 h_border;
	u32 h_blank;	/* fporch + bporch + sync_pulse = blank */
	u32 h_fporch;
	u32 h_sync_pulse;
	u32 v_addressable; /* addressable + boder = active */
	u32 v_border;
	u32 v_blank;	/* fporch + bporch + sync_pulse = blank */
	u32 v_fporch;
	u32 v_sync_pulse;
	u32 width_mm;
	u32 height_mm;
	u32 interlaced;
	u32 stereo;
	u32 sync_type;
	u32 sync_separate;
	u32 vsync_pol;
	u32 hsync_pol;
};

#define EDID_DISPLAY_PORT_SUPPORT 0x05

struct edp_edid {
	char id_name[4];
	short id_product;
	char version;
	char revision;
	char video_intf;	/* dp == 0x5 */
	char color_depth;	/* 6, 8, 10, 12 and 14 bits */
	char color_format;	/* RGB 4:4:4, YCrCb 4:4:4, Ycrcb 4:2:2 */
	char dpm;		/* display power management */
	char sync_digital;	/* 1 = digital */
	char sync_separate;	/* 1 = separate */
	char vsync_pol;		/* 0 = negative, 1 = positive */
	char hsync_pol;		/* 0 = negative, 1 = positive */
	char ext_block_cnt;
	struct display_timing_desc timing[4];
};

struct dp_statistic {
	u32 intr_hpd;
	u32 intr_aux_i2c_done;
	u32 intr_wrong_addr;
	u32 intr_tout;
	u32 intr_nack_defer;
	u32 intr_wrong_data_cnt;
	u32 intr_i2c_nack;
	u32 intr_i2c_defer;
	u32 intr_pll_unlock;
	u32 intr_crc_update;
	u32 intr_frame_end;
	u32 intr_idle_pattern_sent;
	u32 intr_ready_for_video;
	u32 aux_i2c_tx;
	u32 aux_i2c_rx;
	u32 aux_native_tx;
	u32 aux_native_rx;
};

enum dpcd_link_voltage_level {
	DPCD_LINK_VOLTAGE_LEVEL_0	= 0,
	DPCD_LINK_VOLTAGE_LEVEL_1	= 1,
	DPCD_LINK_VOLTAGE_LEVEL_2	= 2,
	DPCD_LINK_VOLTAGE_MAX		= DPCD_LINK_VOLTAGE_LEVEL_2,
};

enum dpcd_link_preemaphasis_level {
	DPCD_LINK_PRE_EMPHASIS_LEVEL_0	= 0,
	DPCD_LINK_PRE_EMPHASIS_LEVEL_1	= 1,
	DPCD_LINK_PRE_EMPHASIS_LEVEL_2	= 2,
	DPCD_LINK_PRE_EMPHASIS_MAX	= DPCD_LINK_PRE_EMPHASIS_LEVEL_2,
};

struct dp_pinctrl_res {
	struct pinctrl *pinctrl;
	struct pinctrl_state *state_active;
	struct pinctrl_state *state_hpd_active;
	struct pinctrl_state *state_suspend;
};

irqreturn_t dp_isr(int irq, void *ptr);

struct dp_hdcp {
	void *data;
	struct hdcp_ops *ops;

	void *hdcp1;
	void *hdcp2;

	int enc_lvl;

	bool auth_state;
	bool hdcp1_present;
	bool hdcp2_present;
	bool feature_enabled;
};

struct mdss_dp_drv_pdata {
	/* device driver */
	int (*on) (struct mdss_panel_data *pdata);
	int (*off) (struct mdss_panel_data *pdata);
	struct platform_device *pdev;
	struct platform_device *ext_pdev;

	struct usbpd *pd;
	enum plug_orientation orientation;
	struct dp_hdcp hdcp;
	struct usbpd_svid_handler svid_handler;
	struct dp_alt_mode alt_mode;
	bool dp_initialized;
	struct msm_ext_disp_init_data ext_audio_data;

	struct mutex emutex;
	int clk_cnt;
	int cont_splash;
	bool inited;
	bool core_power;
	bool core_clks_on;
	bool link_clks_on;
	bool power_on;
	bool sink_info_read;
	bool hpd;
	bool psm_enabled;

	/* dp specific */
	unsigned char *base;
	struct dss_io_data ctrl_io;
	struct dss_io_data phy_io;
	struct dss_io_data tcsr_reg_io;
	struct dss_io_data dp_cc_io;
	struct dss_io_data qfprom_io;
	struct dss_io_data hdcp_io;
	int base_size;
	unsigned char *mmss_cc_base;
	u32 mask1;
	u32 mask2;

	struct mdss_panel_data panel_data;
	struct mdss_util_intf *mdss_util;

	int dp_on_cnt;
	int dp_off_cnt;

	u32 pixel_rate; /* KHz */
	u32 aux_rate;
	char link_rate;	/* X 27000000 for real rate */
	char lane_cnt;
	char train_link_rate;	/* X 27000000 for real rate */
	char train_lane_cnt;

	u8 *edid_buf;
	u32 edid_buf_size;
	struct edp_edid edid;
	struct dpcd_cap dpcd;

	/* regulators */
	struct dss_module_power power_data[DP_MAX_PM];
	struct dp_pinctrl_res pin_res;
	int aux_sel_gpio;
	int aux_sel_gpio_output;
	int aux_en_gpio;
	int usbplug_cc_gpio;
	int hpd_gpio;
	int clk_on;

	/* hpd */
	int gpio_panel_hpd;
	enum of_gpio_flags hpd_flags;
	int hpd_irq;

	/* aux */
	struct completion aux_comp;
	struct completion train_comp;
	struct completion idle_comp;
	struct completion video_comp;
	struct completion irq_comp;
	struct mutex aux_mutex;
	struct mutex train_mutex;
	struct mutex pd_msg_mutex;
	struct mutex attention_lock;
	struct mutex hdcp_mutex;
	bool cable_connected;
	u32 s3d_mode;
	u32 aux_cmd_busy;
	u32 aux_cmd_i2c;
	int aux_trans_num;
	int aux_error_num;
	u32 aux_ctrl_reg;
	struct edp_buf txp;
	struct edp_buf rxp;
	char txbuf[256];
	char rxbuf[256];
	struct dpcd_link_status link_status;
	char v_level;
	char p_level;
	/* transfer unit */
	char tu_desired;
	char valid_boundary;
	char delay_start;
	u32 bpp;
	struct dp_statistic dp_stat;
	bool hpd_irq_on;
	bool hpd_irq_toggled;
	bool hpd_irq_clients_notified;

	/* event */
	struct workqueue_struct *workq;
	struct work_struct work;
	struct delayed_work hdcp_cb_work;
	u32 current_event;
	spinlock_t event_lock;
	spinlock_t lock;
	struct switch_dev sdev;
	struct kobject *kobj;
	u32 max_pclk_khz;
	u32 vic;
	u32 new_vic;
	u16 dpcd_version;
	int fb_node;
	int hdcp_status;

	struct dpcd_test_request test_data;
	struct dpcd_sink_count sink_count;

	struct list_head attention_head;
};

enum dp_lane_count {
	DP_LANE_COUNT_1	= 1,
	DP_LANE_COUNT_2	= 2,
	DP_LANE_COUNT_4	= 4,
};

enum phy_test_pattern {
	PHY_TEST_PATTERN_NONE,
	PHY_TEST_PATTERN_D10_2_NO_SCRAMBLING,
	PHY_TEST_PATTERN_SYMBOL_ERR_MEASUREMENT_CNT,
	PHY_TEST_PATTERN_PRBS7,
	PHY_TEST_PATTERN_80_BIT_CUSTOM_PATTERN,
	PHY_TEST_PATTERN_HBR2_CTS_EYE_PATTERN,
};

static inline char *mdss_dp_get_phy_test_pattern(u32 phy_test_pattern_sel)
{
	switch (phy_test_pattern_sel) {
	case PHY_TEST_PATTERN_NONE:
		return DP_ENUM_STR(PHY_TEST_PATTERN_NONE);
	case PHY_TEST_PATTERN_D10_2_NO_SCRAMBLING:
		return DP_ENUM_STR(PHY_TEST_PATTERN_D10_2_NO_SCRAMBLING);
	case PHY_TEST_PATTERN_SYMBOL_ERR_MEASUREMENT_CNT:
		return DP_ENUM_STR(PHY_TEST_PATTERN_SYMBOL_ERR_MEASUREMENT_CNT);
	case PHY_TEST_PATTERN_PRBS7:
		return DP_ENUM_STR(PHY_TEST_PATTERN_PRBS7);
	case PHY_TEST_PATTERN_80_BIT_CUSTOM_PATTERN:
		return DP_ENUM_STR(PHY_TEST_PATTERN_80_BIT_CUSTOM_PATTERN);
	case PHY_TEST_PATTERN_HBR2_CTS_EYE_PATTERN:
		return DP_ENUM_STR(PHY_TEST_PATTERN_HBR2_CTS_EYE_PATTERN);
	default:
		return "unknown";
	}
}

static inline bool mdss_dp_is_phy_test_pattern_supported(
		u32 phy_test_pattern_sel)
{
	switch (phy_test_pattern_sel) {
	case PHY_TEST_PATTERN_NONE:
	case PHY_TEST_PATTERN_D10_2_NO_SCRAMBLING:
	case PHY_TEST_PATTERN_SYMBOL_ERR_MEASUREMENT_CNT:
	case PHY_TEST_PATTERN_PRBS7:
	case PHY_TEST_PATTERN_80_BIT_CUSTOM_PATTERN:
	case PHY_TEST_PATTERN_HBR2_CTS_EYE_PATTERN:
		return true;
	default:
		return false;
	}
}

enum dp_aux_error {
	EDP_AUX_ERR_NONE	= 0,
	EDP_AUX_ERR_ADDR	= -1,
	EDP_AUX_ERR_TOUT	= -2,
	EDP_AUX_ERR_NACK	= -3,
	EDP_AUX_ERR_DEFER	= -4,
	EDP_AUX_ERR_NACK_DEFER	= -5,
};

static inline char *mdss_dp_get_aux_error(u32 aux_error)
{
	switch (aux_error) {
	case EDP_AUX_ERR_NONE:
		return DP_ENUM_STR(EDP_AUX_ERR_NONE);
	case EDP_AUX_ERR_ADDR:
		return DP_ENUM_STR(EDP_AUX_ERR_ADDR);
	case EDP_AUX_ERR_TOUT:
		return DP_ENUM_STR(EDP_AUX_ERR_TOUT);
	case EDP_AUX_ERR_NACK:
		return DP_ENUM_STR(EDP_AUX_ERR_NACK);
	case EDP_AUX_ERR_DEFER:
		return DP_ENUM_STR(EDP_AUX_ERR_DEFER);
	case EDP_AUX_ERR_NACK_DEFER:
		return DP_ENUM_STR(EDP_AUX_ERR_NACK_DEFER);
	default:
		return "unknown";
	}
}

enum test_response {
	TEST_ACK			= 0x1,
	TEST_NACK			= 0x2,
	TEST_EDID_CHECKSUM_WRITE	= 0x4,
};

static inline char *mdss_dp_get_test_response(u32 test_response)
{
	switch (test_response) {
	case TEST_NACK:
		return DP_ENUM_STR(TEST_NACK);
	case TEST_ACK:
		return DP_ENUM_STR(TEST_ACK);
	case TEST_EDID_CHECKSUM_WRITE:
		return DP_ENUM_STR(TEST_EDID_CHECKSUM_WRITE);
	default:
		return "unknown";
	}
}

enum test_type {
	UNKNOWN_TEST		= 0,
	TEST_LINK_TRAINING	= BIT(0),
	PHY_TEST_PATTERN	= BIT(3),
	TEST_EDID_READ		= BIT(2),
};

static inline char *mdss_dp_get_test_name(u32 test_requested)
{
	switch (test_requested) {
	case TEST_LINK_TRAINING:	return DP_ENUM_STR(TEST_LINK_TRAINING);
	case PHY_TEST_PATTERN:		return DP_ENUM_STR(PHY_TEST_PATTERN);
	case TEST_EDID_READ:		return DP_ENUM_STR(TEST_EDID_READ);
	default:			return "unknown";
	}
}

static inline const char *__mdss_dp_pm_name(enum dp_pm_type module)
{
	switch (module) {
	case DP_CORE_PM:	return "DP_CORE_PM";
	case DP_CTRL_PM:	return "DP_CTRL_PM";
	case DP_PHY_PM:		return "DP_PHY_PM";
	default:		return "???";
	}
}

static inline const char *__mdss_dp_pm_supply_node_name(
	enum dp_pm_type module)
{
	switch (module) {
	case DP_CORE_PM:	return "qcom,core-supply-entries";
	case DP_CTRL_PM:	return "qcom,ctrl-supply-entries";
	case DP_PHY_PM:		return "qcom,phy-supply-entries";
	default:		return "???";
	}
}

static inline char *mdss_dp_ev_event_to_string(int event)
{
	switch (event) {
	case EV_EDP_AUX_SETUP:
		return DP_ENUM_STR(EV_EDP_AUX_SETUP);
	case EV_EDID_READ:
		return DP_ENUM_STR(EV_EDID_READ);
	case EV_DPCD_CAP_READ:
		return DP_ENUM_STR(EV_DPCD_CAP_READ);
	case EV_DPCD_STATUS_READ:
		return DP_ENUM_STR(EV_DPCD_STATUS_READ);
	case EV_LINK_TRAIN:
		return DP_ENUM_STR(EV_LINK_TRAIN);
	case EV_IDLE_PATTERNS_SENT:
		return DP_ENUM_STR(EV_IDLE_PATTERNS_SENT);
	case EV_VIDEO_READY:
		return DP_ENUM_STR(EV_VIDEO_READY);
	default:
		return "unknown";
	}
}

void mdss_dp_phy_initialize(struct mdss_dp_drv_pdata *dp);

void mdss_dp_dpcd_cap_read(struct mdss_dp_drv_pdata *dp);
int mdss_dp_dpcd_status_read(struct mdss_dp_drv_pdata *dp);
void mdss_dp_aux_parse_sink_status_field(struct mdss_dp_drv_pdata *dp);
int mdss_dp_edid_read(struct mdss_dp_drv_pdata *dp);
int mdss_dp_link_train(struct mdss_dp_drv_pdata *dp);
void dp_aux_i2c_handler(struct mdss_dp_drv_pdata *dp, u32 isr);
void dp_aux_native_handler(struct mdss_dp_drv_pdata *dp, u32 isr);
void mdss_dp_aux_init(struct mdss_dp_drv_pdata *ep);

void mdss_dp_fill_link_cfg(struct mdss_dp_drv_pdata *ep);
void mdss_dp_sink_power_down(struct mdss_dp_drv_pdata *ep);
void mdss_dp_lane_power_ctrl(struct mdss_dp_drv_pdata *ep, int up);
void mdss_dp_config_ctrl(struct mdss_dp_drv_pdata *ep);
char mdss_dp_gen_link_clk(struct mdss_panel_info *pinfo, char lane_cnt);
int mdss_dp_aux_set_sink_power_state(struct mdss_dp_drv_pdata *ep, char state);
int mdss_dp_aux_send_psm_request(struct mdss_dp_drv_pdata *dp, bool enable);
void mdss_dp_aux_send_test_response(struct mdss_dp_drv_pdata *ep);
void *mdss_dp_get_hdcp_data(struct device *dev);
int mdss_dp_hdcp2p2_init(struct mdss_dp_drv_pdata *dp_drv);
bool mdss_dp_aux_clock_recovery_done(struct mdss_dp_drv_pdata *ep);
bool mdss_dp_aux_channel_eq_done(struct mdss_dp_drv_pdata *ep);
bool mdss_dp_aux_is_link_rate_valid(u32 link_rate);
bool mdss_dp_aux_is_lane_count_valid(u32 lane_count);
int mdss_dp_aux_link_status_read(struct mdss_dp_drv_pdata *ep, int len);
void mdss_dp_aux_update_voltage_and_pre_emphasis_lvl(
		struct mdss_dp_drv_pdata *dp);

#endif /* MDSS_DP_H */
