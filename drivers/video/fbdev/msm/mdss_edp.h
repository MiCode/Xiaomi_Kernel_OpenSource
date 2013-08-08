/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#ifndef MDSS_EDP_H
#define MDSS_EDP_H

#include <linux/of_gpio.h>

#define edp_read(offset) readl_relaxed((offset))
#define edp_write(offset, data) writel_relaxed((data), (offset))

#define AUX_CMD_FIFO_LEN	144
#define AUX_CMD_MAX		16
#define AUX_CMD_I2C_MAX		128

#define EDP_PORT_MAX		1
#define EDP_SINK_CAP_LEN	16

#define EDP_AUX_ERR_NONE	0
#define EDP_AUX_ERR_ADDR	-1
#define EDP_AUX_ERR_TOUT	-2
#define EDP_AUX_ERR_NACK	-3

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

#define EDP_MAX_LANE		4

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
	(EDP_INTR_HPD | EDP_INTR_AUX_I2C_DONE| \
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


#define EDP_MAINLINK_CTRL	0x004
#define EDP_STATE_CTRL		0x008
#define EDP_MAINLINK_READY	0x084

#define EDP_AUX_CTRL		0x300
#define EDP_INTERRUPT_STATUS	0x308
#define EDP_INTERRUPT_STATUS_2	0x30c
#define EDP_AUX_DATA		0x314
#define EDP_AUX_TRANS_CTRL	0x318
#define EDP_AUX_STATUS		0x324

#define EDP_PHY_EDPPHY_GLB_VM_CFG0	0x510
#define EDP_PHY_EDPPHY_GLB_VM_CFG1	0x514

struct edp_cmd {
	char read;	/* 1 == read, 0 == write */
	char i2c;	/* 1 == i2c cmd, 0 == native cmd */
	u32 addr;	/* 20 bits */
	char *datap;
	int len;	/* len to be tx OR len to be rx for read */
	char next;	/* next command */
};

struct edp_buf {
	char *start;	/* buffer start addr */
	char *end;	/* buffer end addr */
	int size;	/* size of buffer */
	char *data;	/* data pointer */
	int len;	/* dara length */
	char trans_num;	/* transaction number */
	char i2c;	/* 1 == i2c cmd, 0 == native cmd */
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
#define EV_IDLE_PATTERNS_SENT		BIT(30)
#define EV_VIDEO_READY			BIT(31)

/* edp state ctrl */
#define ST_TRAIN_PATTERN_1		BIT(0)
#define ST_TRAIN_PATTERN_2		BIT(1)
#define ST_TRAIN_PATTERN_3		BIT(2)
#define ST_SYMBOL_ERR_RATE_MEASUREMENT	BIT(3)
#define ST_PRBS7			BIT(4)
#define ST_CUSTOM_80_BIT_PATTERN	BIT(5)
#define ST_SEND_VIDEO			BIT(6)
#define ST_PUSH_IDLE			BIT(7)

/* sink power state  */
#define SINK_POWER_ON		1
#define SINK_POWER_OFF		2

#define EDP_LINK_RATE_162	6	/* 1.62G = 270M * 6 */
#define EDP_LINK_RATE_270	10	/* 2.70G = 270M * 10 */
#define EDP_LINK_RATE_MAX	EDP_LINK_RATE_270

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
	char video_intf;	/* edp == 0x5 */
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

struct edp_statistic {
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


#define DPCD_LINK_VOLTAGE_MAX	4
#define DPCD_LINK_PRE_EMPHASIS_MAX	4

#define HPD_EVENT_MAX   8

struct mdss_edp_drv_pdata {
	/* device driver */
	int (*on) (struct mdss_panel_data *pdata);
	int (*off) (struct mdss_panel_data *pdata);
	struct platform_device *pdev;

	struct mutex emutex;
	int clk_cnt;
	int cont_splash;

	/* edp specific */
	unsigned char *base;
	int base_size;
	unsigned char *mmss_cc_base;
	u32 mask1;
	u32 mask2;

	struct mdss_panel_data panel_data;

	int edp_on_cnt;
	int edp_off_cnt;

	u32 pixel_rate;
	u32 aux_rate;
	char link_rate;	/* X 27000000 for real rate */
	char lane_cnt;
	char train_link_rate;	/* X 27000000 for real rate */
	char train_lane_cnt;

	struct edp_edid edid;
	struct dpcd_cap dpcd;

	/* regulators */
	struct regulator *vdda_vreg;

	/* clocks */
	struct clk *aux_clk;
	struct clk *pixel_clk;
	struct clk *ahb_clk;
	struct clk *link_clk;
	struct clk *mdp_core_clk;
	int clk_on;

	/* gpios */
	int gpio_panel_en;

	/* backlight */
	struct pwm_device *bl_pwm;
	bool is_pwm_enabled;
	int lpg_channel;
	int pwm_period;

	/* hpd */
	int gpio_panel_hpd;
	enum of_gpio_flags hpd_flags;
	int hpd_irq;

	/* aux */
	struct completion aux_comp;
	struct completion train_comp;
	struct completion idle_comp;
	struct completion video_comp;
	struct mutex aux_mutex;
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
	struct edp_statistic edp_stat;

	/* event */
	wait_queue_head_t event_q;
	u32 event_pndx;
	u32 event_gndx;
	u32 event_todo_list[HPD_EVENT_MAX];
	spinlock_t event_lock;
	spinlock_t lock;
};

int mdss_edp_aux_clk_enable(struct mdss_edp_drv_pdata *edp_drv);
void mdss_edp_aux_clk_disable(struct mdss_edp_drv_pdata *edp_drv);
int mdss_edp_clk_enable(struct mdss_edp_drv_pdata *edp_drv);
void mdss_edp_clk_disable(struct mdss_edp_drv_pdata *edp_drv);
int mdss_edp_clk_init(struct mdss_edp_drv_pdata *edp_drv);
void mdss_edp_clk_deinit(struct mdss_edp_drv_pdata *edp_drv);
int mdss_edp_prepare_aux_clocks(struct mdss_edp_drv_pdata *edp_drv);
void mdss_edp_unprepare_aux_clocks(struct mdss_edp_drv_pdata *edp_drv);
int mdss_edp_prepare_clocks(struct mdss_edp_drv_pdata *edp_drv);
void mdss_edp_unprepare_clocks(struct mdss_edp_drv_pdata *edp_drv);

void mdss_edp_dpcd_cap_read(struct mdss_edp_drv_pdata *edp);
int mdss_edp_dpcd_status_read(struct mdss_edp_drv_pdata *edp);
void mdss_edp_edid_read(struct mdss_edp_drv_pdata *edp, int block);
int mdss_edp_link_train(struct mdss_edp_drv_pdata *edp);
void edp_aux_i2c_handler(struct mdss_edp_drv_pdata *edp, u32 isr);
void edp_aux_native_handler(struct mdss_edp_drv_pdata *edp, u32 isr);
void mdss_edp_aux_init(struct mdss_edp_drv_pdata *ep);

void mdss_edp_fill_link_cfg(struct mdss_edp_drv_pdata *ep);
void mdss_edp_sink_power_down(struct mdss_edp_drv_pdata *ep);
void mdss_edp_state_ctrl(struct mdss_edp_drv_pdata *ep, u32 state);
int mdss_edp_sink_power_state(struct mdss_edp_drv_pdata *ep, char state);
void mdss_edp_lane_power_ctrl(struct mdss_edp_drv_pdata *ep, int up);
void mdss_edp_config_ctrl(struct mdss_edp_drv_pdata *ep);

void mdss_edp_clk_debug(unsigned char *edp_base, unsigned char *mmss_cc_base);

#endif /* MDSS_EDP_H */
