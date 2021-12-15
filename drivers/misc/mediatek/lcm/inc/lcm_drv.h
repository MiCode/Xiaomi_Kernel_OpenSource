/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
 */

#ifndef __LCM_DRV_H__
#define __LCM_DRV_H__

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#ifndef ARY_SIZE
#define ARY_SIZE(x) (sizeof((x)) / sizeof((x[0])))
#endif

/* ------------------------------------------------------------------------- */

/* common enumerations */

enum LCM_TYPE {
	LCM_TYPE_DBI = 0,
	LCM_TYPE_DPI,
	LCM_TYPE_DSI
};


enum LCM_CTRL {
	LCM_CTRL_NONE = 0,
	LCM_CTRL_SERIAL_DBI,
	LCM_CTRL_PARALLEL_DBI,
	LCM_CTRL_GPIO
};


enum LCM_POLARITY {
	LCM_POLARITY_RISING = 0,
	LCM_POLARITY_FALLING = 1
};


enum LCM_CLOCK_PHASE {
	LCM_CLOCK_PHASE_0 = 0,
	LCM_CLOCK_PHASE_90 = 1
};


enum LCM_COLOR_ORDER {
	LCM_COLOR_ORDER_RGB = 0,
	LCM_COLOR_ORDER_BGR = 1
};


enum LCM_DRIVING_CURRENT {
	LCM_DRIVING_CURRENT_DEFAULT,
	LCM_DRIVING_CURRENT_8MA = (1 << 0),
	LCM_DRIVING_CURRENT_4MA = (1 << 1),
	LCM_DRIVING_CURRENT_2MA = (1 << 2),
	LCM_DRIVING_CURRENT_SLEW_CNTL = (1 << 3),
	LCM_DRIVING_CURRENT_6575_4MA = (1 << 4),
	LCM_DRIVING_CURRENT_6575_8MA = (3 << 4),
	LCM_DRIVING_CURRENT_6575_12MA = (2 << 4),
	LCM_DRIVING_CURRENT_6575_16MA = (4 << 4),
	LCM_DRIVING_CURRENT_6MA,
	LCM_DRIVING_CURRENT_10MA,
	LCM_DRIVING_CURRENT_12MA,
	LCM_DRIVING_CURRENT_14MA,
	LCM_DRIVING_CURRENT_16MA,
	LCM_DRIVING_CURRENT_20MA,
	LCM_DRIVING_CURRENT_24MA,
	LCM_DRIVING_CURRENT_28MA,
	LCM_DRIVING_CURRENT_32MA
};

enum LCM_INTERFACE_ID {
	LCM_INTERFACE_NOTDEFINED = 0,
	LCM_INTERFACE_DSI0,
	LCM_INTERFACE_DSI1,
	LCM_INTERFACE_DSI_DUAL,
	LCM_INTERFACE_DPI0,
	LCM_INTERFACE_DPI1,
	LCM_INTERFACE_DBI0
};

enum LCM_IOCTL {
	LCM_IOCTL_NULL = 0,
};

/* DBI related enumerations */

enum LCM_DBI_CLOCK_FREQ {
	LCM_DBI_CLOCK_FREQ_104M = 0,
	LCM_DBI_CLOCK_FREQ_52M,
	LCM_DBI_CLOCK_FREQ_26M,
	LCM_DBI_CLOCK_FREQ_13M,
	LCM_DBI_CLOCK_FREQ_7M
};


enum LCM_DBI_DATA_WIDTH {
	LCM_DBI_DATA_WIDTH_8BITS = 0,
	LCM_DBI_DATA_WIDTH_9BITS = 1,
	LCM_DBI_DATA_WIDTH_16BITS = 2,
	LCM_DBI_DATA_WIDTH_18BITS = 3,
	LCM_DBI_DATA_WIDTH_24BITS = 4,
	LCM_DBI_DATA_WIDTH_32BITS = 5
};


enum LCM_DBI_CPU_WRITE_BITS {
	LCM_DBI_CPU_WRITE_8_BITS = 8,
	LCM_DBI_CPU_WRITE_16_BITS = 16,
	LCM_DBI_CPU_WRITE_32_BITS = 32,
};


enum LCM_DBI_FORMAT {
	LCM_DBI_FORMAT_RGB332 = 0,
	LCM_DBI_FORMAT_RGB444 = 1,
	LCM_DBI_FORMAT_RGB565 = 2,
	LCM_DBI_FORMAT_RGB666 = 3,
	LCM_DBI_FORMAT_RGB888 = 4
};


enum LCM_DBI_TRANS_SEQ {
	LCM_DBI_TRANS_SEQ_MSB_FIRST = 0,
	LCM_DBI_TRANS_SEQ_LSB_FIRST = 1
};


enum LCM_DBI_PADDING {
	LCM_DBI_PADDING_ON_LSB = 0,
	LCM_DBI_PADDING_ON_MSB = 1
};


enum LCM_DBI_TE_MODE {
	LCM_DBI_TE_MODE_DISABLED = 0,
	LCM_DBI_TE_MODE_VSYNC_ONLY = 1,
	LCM_DBI_TE_MODE_VSYNC_OR_HSYNC = 2,
};


enum LCM_DBI_TE_VS_WIDTH_CNT_DIV {
	LCM_DBI_TE_VS_WIDTH_CNT_DIV_8 = 0,
	LCM_DBI_TE_VS_WIDTH_CNT_DIV_16 = 1,
	LCM_DBI_TE_VS_WIDTH_CNT_DIV_32 = 2,
	LCM_DBI_TE_VS_WIDTH_CNT_DIV_64 = 3,
};


/* DPI related enumerations */

enum LCM_DPI_FORMAT {
	LCM_DPI_FORMAT_RGB565 = 0,
	LCM_DPI_FORMAT_RGB666 = 1,
	LCM_DPI_FORMAT_RGB888 = 2
};

enum LCM_SERIAL_CLOCK_FREQ {
	LCM_SERIAL_CLOCK_FREQ_104M = 0,
	LCM_SERIAL_CLOCK_FREQ_26M,
	LCM_SERIAL_CLOCK_FREQ_52M
};

enum LCM_SERIAL_CLOCK_DIV {
	LCM_SERIAL_CLOCK_DIV_2 = 0,
	LCM_SERIAL_CLOCK_DIV_4 = 1,
	LCM_SERIAL_CLOCK_DIV_8 = 2,
	LCM_SERIAL_CLOCK_DIV_16 = 3,
};


/* DSI related enumerations */

enum LCM_DSI_MODE_CON {
	CMD_MODE = 0,
	SYNC_PULSE_VDO_MODE = 1,
	SYNC_EVENT_VDO_MODE = 2,
	BURST_VDO_MODE = 3
};


enum LCM_LANE_NUM {
	LCM_ONE_LANE = 1,
	LCM_TWO_LANE = 2,
	LCM_THREE_LANE = 3,
	LCM_FOUR_LANE = 4,
};


enum LCM_DSI_FORMAT {
	LCM_DSI_FORMAT_RGB565 = 0,
	LCM_DSI_FORMAT_RGB666_LOOSELY = 1,
	LCM_DSI_FORMAT_RGB666 = 2,
	LCM_DSI_FORMAT_RGB888 = 3,
	LCM_DSI_FORMAT_RGB101010 = 4,
};


enum LCM_DSI_TRANS_SEQ {
	LCM_DSI_TRANS_SEQ_MSB_FIRST = 0,
	LCM_DSI_TRANS_SEQ_LSB_FIRST = 1
};


enum LCM_DSI_PADDING {
	LCM_DSI_PADDING_ON_LSB = 0,
	LCM_DSI_PADDING_ON_MSB = 1
};


enum LCM_PS_TYPE {
	LCM_PACKED_PS_16BIT_RGB565 = 0,
	LCM_LOOSELY_PS_18BIT_RGB666 = 1,
	LCM_PACKED_PS_24BIT_RGB888 = 2,
	LCM_PACKED_PS_18BIT_RGB666 = 3
};


enum LCM_SCALE_TYPE {
	LCM_Hx1_Vx1 = 0,
	LCM_Hx1_Vx2 = 1,
	LCM_Hx2_Vx1 = 2,
	LCM_Hx2_Vx2 = 3
};



enum LCM_DSI_PLL_CLOCK {
	LCM_DSI_6589_PLL_CLOCK_NULL = 0,
	LCM_DSI_6589_PLL_CLOCK_201_5 = 1,
	LCM_DSI_6589_PLL_CLOCK_208 = 2,
	LCM_DSI_6589_PLL_CLOCK_214_5 = 3,
	LCM_DSI_6589_PLL_CLOCK_221 = 4,
	LCM_DSI_6589_PLL_CLOCK_227_5 = 5,
	LCM_DSI_6589_PLL_CLOCK_234 = 6,
	LCM_DSI_6589_PLL_CLOCK_240_5 = 7,
	LCM_DSI_6589_PLL_CLOCK_247 = 8,
	LCM_DSI_6589_PLL_CLOCK_253_5 = 9,
	LCM_DSI_6589_PLL_CLOCK_260 = 10,
	LCM_DSI_6589_PLL_CLOCK_266_5 = 11,
	LCM_DSI_6589_PLL_CLOCK_273 = 12,
	LCM_DSI_6589_PLL_CLOCK_279_5 = 13,
	LCM_DSI_6589_PLL_CLOCK_286 = 14,
	LCM_DSI_6589_PLL_CLOCK_292_5 = 15,
	LCM_DSI_6589_PLL_CLOCK_299 = 16,
	LCM_DSI_6589_PLL_CLOCK_305_5 = 17,
	LCM_DSI_6589_PLL_CLOCK_312 = 18,
	LCM_DSI_6589_PLL_CLOCK_318_5 = 19,
	LCM_DSI_6589_PLL_CLOCK_325 = 20,
	LCM_DSI_6589_PLL_CLOCK_331_5 = 21,
	LCM_DSI_6589_PLL_CLOCK_338 = 22,
	LCM_DSI_6589_PLL_CLOCK_344_5 = 23,
	LCM_DSI_6589_PLL_CLOCK_351 = 24,
	LCM_DSI_6589_PLL_CLOCK_357_5 = 25,
	LCM_DSI_6589_PLL_CLOCK_364 = 26,
	LCM_DSI_6589_PLL_CLOCK_370_5 = 27,
	LCM_DSI_6589_PLL_CLOCK_377 = 28,
	LCM_DSI_6589_PLL_CLOCK_383_5 = 29,
	LCM_DSI_6589_PLL_CLOCK_390 = 30,
	LCM_DSI_6589_PLL_CLOCK_396_5 = 31,
	LCM_DSI_6589_PLL_CLOCK_403 = 32,
	LCM_DSI_6589_PLL_CLOCK_409_5 = 33,
	LCM_DSI_6589_PLL_CLOCK_416 = 34,
	LCM_DSI_6589_PLL_CLOCK_422_5 = 35,
	LCM_DSI_6589_PLL_CLOCK_429 = 36,
	LCM_DSI_6589_PLL_CLOCK_435_5 = 37,
	LCM_DSI_6589_PLL_CLOCK_442 = 38,
	LCM_DSI_6589_PLL_CLOCK_448_5 = 39,
	LCM_DSI_6589_PLL_CLOCK_455 = 40,
	LCM_DSI_6589_PLL_CLOCK_461_5 = 41,
	LCM_DSI_6589_PLL_CLOCK_468 = 42,
	LCM_DSI_6589_PLL_CLOCK_474_5 = 43,
	LCM_DSI_6589_PLL_CLOCK_481 = 44,
	LCM_DSI_6589_PLL_CLOCK_487_5 = 45,
	LCM_DSI_6589_PLL_CLOCK_494 = 46,
	LCM_DSI_6589_PLL_CLOCK_500_5 = 47,
	LCM_DSI_6589_PLL_CLOCK_507 = 48,
	LCM_DSI_6589_PLL_CLOCK_513_5 = 49,
	LCM_DSI_6589_PLL_CLOCK_520 = 50,
};

/* ------------------------------------------------------------------------- */

struct LCM_DBI_DATA_FORMAT {
	enum LCM_COLOR_ORDER color_order;
	enum LCM_DBI_TRANS_SEQ trans_seq;
	enum LCM_DBI_PADDING padding;
	enum LCM_DBI_FORMAT format;
	enum LCM_DBI_DATA_WIDTH width;
};


struct LCM_DBI_SERIAL_PARAMS {
	enum LCM_POLARITY cs_polarity;
	enum LCM_POLARITY clk_polarity;
	enum LCM_CLOCK_PHASE clk_phase;
	unsigned int is_non_dbi_mode;

	enum LCM_SERIAL_CLOCK_FREQ clock_base;
	enum LCM_SERIAL_CLOCK_DIV clock_div;

	unsigned int css;
	unsigned int csh;
	unsigned int rd_1st;
	unsigned int rd_2nd;
	unsigned int wr_1st;
	unsigned int wr_2nd;

	unsigned int sif_3wire;
	unsigned int sif_sdi;
	enum LCM_POLARITY sif_1st_pol;
	enum LCM_POLARITY sif_sck_def;
	unsigned int sif_div2;
	unsigned int sif_hw_cs;
/* ////////////////////////////////// */
};


struct LCM_DBI_PARALLEL_PARAMS {
	/* timing parameters */
	unsigned int write_setup;
	unsigned int write_hold;
	unsigned int write_wait;
	unsigned int read_setup;
	unsigned int read_hold;
	unsigned int read_latency;
	unsigned int wait_period;
	/*only for 6575 */
	unsigned int cs_high_width;
};


struct LCM_DSI_DATA_FORMAT {
	enum LCM_COLOR_ORDER color_order;
	enum LCM_DSI_TRANS_SEQ trans_seq;
	enum LCM_DSI_PADDING padding;
	enum LCM_DSI_FORMAT format;
};

struct LCM_DSI_MODE_SWITCH_CMD {
	enum LCM_DSI_MODE_CON mode;
	unsigned int cmd_if;
	unsigned int addr;
	unsigned int val[4];
};

struct LCM_UFOE_CONFIG_PARAMS {
	unsigned int compress_ratio;
	unsigned int lr_mode_en;
	unsigned int vlc_disable;
	unsigned int vlc_config;
};
/* ------------------------------------------------------------------------- */

struct LCM_DSC_CONFIG_PARAMS {
	unsigned int slice_width;
	unsigned int slice_hight;
	unsigned int bit_per_pixel;
	unsigned int slice_mode;
	unsigned int rgb_swap;
	unsigned int dsc_cfg;
	unsigned int dsc_line_buf_depth;
	unsigned int bit_per_channel;
	unsigned int rct_on;
	unsigned int bp_enable;

	unsigned int dec_delay;
	unsigned int xmit_delay;
	unsigned int scale_value;

	unsigned int increment_interval;
	unsigned int line_bpg_offset;
	unsigned int decrement_interval;
	unsigned int nfl_bpg_offset;
	unsigned int slice_bpg_offset;
	unsigned int initial_offset;
	unsigned int final_offset;

	unsigned int flatness_minqp;
	unsigned int flatness_maxqp;
	unsigned int rc_mode1_size;
};


struct LCM_DBI_PARAMS {
	/* common parameters for serial & parallel interface */
	unsigned int port;
	enum LCM_DBI_CLOCK_FREQ clock_freq;
	enum LCM_DBI_DATA_WIDTH data_width;
	struct LCM_DBI_DATA_FORMAT data_format;
	enum LCM_DBI_CPU_WRITE_BITS cpu_write_bits;
	enum LCM_DRIVING_CURRENT io_driving_current;
	enum LCM_DRIVING_CURRENT msb_io_driving_current;
	enum LCM_DRIVING_CURRENT ctrl_io_driving_current;

	/* tearing control */
	enum LCM_DBI_TE_MODE te_mode;
	enum LCM_POLARITY te_edge_polarity;
	unsigned int te_hs_delay_cnt;
	unsigned int te_vs_width_cnt;
	enum LCM_DBI_TE_VS_WIDTH_CNT_DIV te_vs_width_cnt_div;

	/* particular parameters for serial & parallel interface */
	struct LCM_DBI_SERIAL_PARAMS serial;
	struct LCM_DBI_PARALLEL_PARAMS parallel;
};


struct LCM_DPI_PARAMS {
	/* Pixel Clock Frequency = 26MHz * mipi_pll_clk_div1*/
	/*   / (mipi_pll_clk_ref + 1)*/
	/*   / (2 * mipi_pll_clk_div2)*/
	/*   / dpi_clk_div */
	unsigned int mipi_pll_clk_ref;	/* 0..1 */
	unsigned int mipi_pll_clk_div1;	/* 0..63 */
	unsigned int mipi_pll_clk_div2;	/* 0..15 */
	/* PCLK=> 8: 26MHz, 10: 35MHz, 12: 40MHz */
	unsigned int mipi_pll_clk_fbk_div;
	unsigned int dpi_clk_div;	/* 2..32 */
	unsigned int dpi_clk_duty;	/* (dpi_clk_div - 1) .. 31 */
	unsigned int PLL_CLOCK;
	unsigned int dpi_clock;
	unsigned int ssc_disable;
	unsigned int ssc_range;

	unsigned int width;
	unsigned int height;
	unsigned int bg_width;
	unsigned int bg_height;

	/* polarity parameters */
	enum LCM_POLARITY clk_pol;
	enum LCM_POLARITY de_pol;
	enum LCM_POLARITY vsync_pol;
	enum LCM_POLARITY hsync_pol;

	/* timing parameters */
	unsigned int hsync_pulse_width;
	unsigned int hsync_back_porch;
	unsigned int hsync_front_porch;
	unsigned int vsync_pulse_width;
	unsigned int vsync_back_porch;
	unsigned int vsync_front_porch;

	/* output format parameters */
	enum LCM_DPI_FORMAT format;
	enum LCM_COLOR_ORDER rgb_order;
	unsigned int is_serial_output;
	unsigned int i2x_en;
	unsigned int i2x_edge;
	unsigned int embsync;
	unsigned int lvds_tx_en;
	unsigned int bit_swap;
	unsigned int is_dual_lvds_tx;
	unsigned int is_vesa;
	/* intermediate buffers parameters */
	unsigned int intermediat_buffer_num;	/* 2..3 */

	unsigned int dsc_enable;
	struct LCM_DSC_CONFIG_PARAMS dsc_params;

	/* iopad parameters */
	enum LCM_DRIVING_CURRENT io_driving_current;
	enum LCM_DRIVING_CURRENT msb_io_driving_current;
	enum LCM_DRIVING_CURRENT lsb_io_driving_current;
	enum LCM_DRIVING_CURRENT ctrl_io_driving_current;
};


/* ------------------------------------------------------------------------- */
#define RT_MAX_NUM 10
#define ESD_CHECK_NUM 3
struct LCM_esd_check_item {
	unsigned char cmd;
	unsigned char count;
	unsigned char para_list[RT_MAX_NUM];
};
enum DUAL_DSI_TYPE {
	DUAL_DSI_NONE = 0x0,
	DUAL_DSI_CMD = 0x1,
	DUAL_DSI_VDO = 0x2,
};

enum MIPITX_PHY_LANE_SWAP {
	MIPITX_PHY_LANE_0 = 0,
	MIPITX_PHY_LANE_1,
	MIPITX_PHY_LANE_2,
	MIPITX_PHY_LANE_3,
	MIPITX_PHY_LANE_CK,
	MIPITX_PHY_LANE_RX,
	MIPITX_PHY_LANE_NUM
};

enum MIPITX_PHY_PORT {
	MIPITX_PHY_PORT_0 = 0,
	MIPITX_PHY_PORT_1,
	MIPITX_PHY_PORT_NUM
};

/*ARR*/
#define DYNAMIC_FPS_LEVELS 10
struct dynamic_fps_info {
	unsigned int fps;
	unsigned int vfp; /*lines*/
	/*unsigned int idle_check_interval;*//*ms*/
};


/*DynFPS*/
enum DynFPS_LEVEL {
	DFPS_LEVEL0 = 0,
	DFPS_LEVEL1,
	DFPS_LEVELNUM,
};

#define DFPS_LEVELS 2
enum FPS_CHANGE_INDEX {
	DYNFPS_NOT_DEFINED = 0,
	DYNFPS_DSI_VFP = 1,
	DYNFPS_DSI_HFP = 2,
	DYNFPS_DSI_MIPI_CLK = 4,
};

struct dfps_info {
	enum DynFPS_LEVEL level;
	unsigned int fps; /*real fps *100*/

	unsigned int vertical_sync_active;
	unsigned int vertical_backporch;
	unsigned int vertical_frontporch;
	unsigned int vertical_frontporch_for_low_power;

	unsigned int horizontal_sync_active;
	unsigned int horizontal_backporch;
	unsigned int horizontal_frontporch;

	unsigned int PLL_CLOCK;
	/* data_rate = PLL_CLOCK x 2 */
	unsigned int data_rate;
	/*real fps during active*/
	unsigned int vact_timing_fps; /*real vact timing fps * 100*/

	/*mipi hopping*/
	unsigned int dynamic_switch_mipi;
	unsigned int vertical_sync_active_dyn;
	unsigned int vertical_backporch_dyn;
	unsigned int vertical_frontporch_dyn;
	unsigned int vertical_frontporch_for_low_power_dyn;
	unsigned int vertical_active_line_dyn;

	unsigned int horizontal_sync_active_dyn;
	unsigned int horizontal_backporch_dyn;
	unsigned int horizontal_frontporch_dyn;
	unsigned int horizontal_active_pixel_dyn;

	unsigned int PLL_CLOCK_dyn;	/* PLL_CLOCK = (int) PLL_CLOCK */
	unsigned int data_rate_dyn;	/* data_rate = PLL_CLOCK x 2 */

	/*real fps during active*/
	unsigned int vact_timing_fps_dyn;
};


struct LCM_DSI_PARAMS {
	enum LCM_DSI_MODE_CON mode;
	enum LCM_DSI_MODE_CON switch_mode;
	unsigned int DSI_WMEM_CONTI;
	unsigned int DSI_RMEM_CONTI;
	unsigned int VC_NUM;

	enum LCM_LANE_NUM LANE_NUM;
	struct LCM_DSI_DATA_FORMAT data_format;

	/* intermediate buffers parameters */
	unsigned int intermediat_buffer_num;	/* 2..3 */

	enum LCM_PS_TYPE PS;
	unsigned int word_count;

	unsigned int packet_size;
	unsigned int packet_size_mult;
	unsigned int vertical_sync_active;
	unsigned int vertical_backporch;
	unsigned int vertical_frontporch;
	unsigned int vertical_frontporch_for_low_power;
	unsigned int vertical_active_line;

	unsigned int horizontal_sync_active;
	unsigned int horizontal_backporch;
	unsigned int horizontal_frontporch;
	unsigned int horizontal_blanking_pixel;
	unsigned int horizontal_active_pixel;
	unsigned int horizontal_bllp;

	unsigned int line_byte;
	unsigned int horizontal_sync_active_byte;
	unsigned int horizontal_backporch_byte;
	unsigned int horizontal_frontporch_byte;
	unsigned int rgb_byte;

	unsigned int horizontal_sync_active_word_count;
	unsigned int horizontal_backporch_word_count;
	unsigned int horizontal_frontporch_word_count;

	unsigned char HS_TRAIL;
	unsigned char HS_ZERO;
	unsigned char HS_PRPR;
	unsigned char LPX;

	unsigned char TA_SACK;
	unsigned char TA_GET;
	unsigned char TA_SURE;
	unsigned char TA_GO;

	unsigned char CLK_TRAIL;
	unsigned char CLK_ZERO;
	unsigned char LPX_WAIT;
	unsigned char CONT_DET;

	unsigned char CLK_HS_PRPR;
	unsigned char CLK_HS_POST;
	unsigned char DA_HS_EXIT;
	unsigned char CLK_HS_EXIT;

	unsigned int pll_select;
	unsigned int pll_div1;
	unsigned int pll_div2;
	unsigned int fbk_div;

	unsigned int pll_prediv;
	unsigned int pll_posdiv;
	unsigned int pll_s2qdiv;

	unsigned int fbk_sel;
	unsigned int rg_bir;
	unsigned int rg_bic;
	unsigned int rg_bp;
	/* PLL_CLOCK = (int) PLL_CLOCK */
	unsigned int PLL_CLOCK;
	/* data_rate = PLL_CLOCK x 2 */
	unsigned int data_rate;
	unsigned int PLL_CK_VDO;
	unsigned int PLL_CK_CMD;
	unsigned int dsi_clock;
	unsigned int ssc_disable;
	unsigned int ssc_range;
	unsigned int compatibility_for_nvk;
	unsigned int cont_clock;
	unsigned int ufoe_enable;
	unsigned int dsc_enable;
	unsigned int bdg_dsc_enable;
	unsigned int bdg_ssc_disable;
	struct LCM_UFOE_CONFIG_PARAMS ufoe_params;
	struct LCM_DSC_CONFIG_PARAMS dsc_params;
	unsigned int edp_panel;
	unsigned int customization_esd_check_enable;
	unsigned int esd_check_enable;
	unsigned int lcm_int_te_monitor;
	unsigned int lcm_int_te_period;

	unsigned int lcm_ext_te_monitor;
	unsigned int lcm_ext_te_enable;

	unsigned int noncont_clock;
	unsigned int noncont_clock_period;
	unsigned int clk_lp_per_line_enable;
	struct LCM_esd_check_item lcm_esd_check_table[ESD_CHECK_NUM];
	unsigned int switch_mode_enable;
	enum DUAL_DSI_TYPE dual_dsi_type;
	unsigned int lane_swap_en;
	enum MIPITX_PHY_LANE_SWAP
		lane_swap[MIPITX_PHY_PORT_NUM][MIPITX_PHY_LANE_NUM];

	unsigned int vertical_vfp_lp;
	unsigned int PLL_CLOCK_lp;
	unsigned int ulps_sw_enable;
	unsigned int null_packet_en;
	unsigned int mixmode_enable;
	unsigned int mixmode_mipi_clock;
	unsigned int pwm_fps;
	unsigned int send_frame_enable;

	unsigned int lfr_enable;
	unsigned int lfr_mode;
	unsigned int lfr_type;
	unsigned int lfr_skip_num;

	unsigned int ext_te_edge;
	unsigned int eint_disable;

	unsigned int IsCphy;
	unsigned int PHY_SEL0;
	unsigned int PHY_SEL1;
	unsigned int PHY_SEL2;
	unsigned int PHY_SEL3;

	unsigned int dynamic_switch_mipi;
	unsigned int vertical_sync_active_dyn;
	unsigned int vertical_backporch_dyn;
	unsigned int vertical_frontporch_dyn;
	unsigned int vertical_active_line_dyn;

	unsigned int horizontal_sync_active_dyn;
	unsigned int horizontal_backporch_dyn;
	unsigned int horizontal_frontporch_dyn;
	unsigned int horizontal_active_pixel_dyn;

	unsigned int PLL_CLOCK_dyn;	/* PLL_CLOCK = (int) PLL_CLOCK */
	unsigned int data_rate_dyn;	/* data_rate = PLL_CLOCK x 2 */

	/*for ARR*/
	unsigned int dynamic_fps_levels;
	struct dynamic_fps_info dynamic_fps_table[DYNAMIC_FPS_LEVELS];

#ifdef CONFIG_MTK_HIGH_FRAME_RATE
	/****DynFPS start****/
	unsigned int dfps_enable;
	unsigned int dfps_default_fps;
	unsigned int dfps_def_vact_tim_fps;
	unsigned int dfps_num;
	/*unsigned int dfps_solution;*/
	struct dfps_info dfps_params[DFPS_LEVELS];
	/****DynFPS end****/
#endif
};

/* ------------------------------------------------------------------------- */
struct LCM_PARAMS {
	enum LCM_TYPE type;
	enum LCM_CTRL ctrl;		/* ! how to control LCM registers */
	enum LCM_INTERFACE_ID lcm_if;
	enum LCM_INTERFACE_ID lcm_cmd_if;
	/* common parameters */
	unsigned int lcm_x;
	unsigned int lcm_y;
	unsigned int width;
	unsigned int height;
	unsigned int virtual_width;
	unsigned int virtual_height;
	unsigned int density;
	/* DBI or DPI should select IO mode according to chip spec */
	unsigned int io_select_mode;

	/* particular parameters */
	struct LCM_DBI_PARAMS dbi;
	struct LCM_DPI_PARAMS dpi;
	struct LCM_DSI_PARAMS dsi;
	unsigned int physical_width; /* length: mm, for legacy use */
	unsigned int physical_height; /* length: mm, for legacy use */
	/* length: um, for more precise precision */
	unsigned int physical_width_um;
	unsigned int physical_height_um;
	unsigned int od_table_size;
	void *od_table;
	unsigned int max_refresh_rate;
	unsigned int min_refresh_rate;

	unsigned int round_corner_en;
	unsigned int full_content;
	unsigned int corner_pattern_width;
	unsigned int corner_pattern_height;
	unsigned int corner_pattern_height_bot;
	unsigned int corner_pattern_tp_size;
	void *corner_pattern_lt_addr;

	int lcm_color_mode;
	unsigned int min_luminance;
	unsigned int average_luminance;
	unsigned int max_luminance;

	unsigned int hbm_en_time;
	unsigned int hbm_dis_time;
};


#ifndef MAX
#define MAX(x, y)   (((x) >= (y)) ? (x) : (y))
#endif				/* MAX */

#ifndef MIN
#define MIN(x, y)   (((x) <= (y)) ? (x) : (y))
#endif				/* MIN */

#define INIT_SIZE			(640)
#define COMPARE_ID_SIZE	(32)
#define SUSPEND_SIZE		(32)
#define BACKLIGHT_SIZE		(32)
#define BACKLIGHT_CMDQ_SIZE		(32)
#define MAX_SIZE (MAX(MAX(MAX(MAX(INIT_SIZE, COMPARE_ID_SIZE), \
	SUSPEND_SIZE), BACKLIGHT_SIZE), BACKLIGHT_CMDQ_SIZE))


struct LCM_DATA_T1 {
	char data;
	char padding[131];
};


struct LCM_DATA_T2 {
	char cmd;
	char data;
	char padding[130];
};


struct LCM_DATA_T3 {
	char cmd;
	char size;
	char data[128];
	char padding[2];
};


struct LCM_DATA_T4 {
	char cmd;
	char location;
	char data;
	char padding[129];
};


struct LCM_DATA_T5 {
	char size;
	char cmd[128];
	char padding[3];
};


struct LCM_DATA {
	char func;
	char type;
	char size;
	char padding;

	union {
		struct LCM_DATA_T1 data_t1;
		struct LCM_DATA_T2 data_t2;
		struct LCM_DATA_T3 data_t3;
		struct LCM_DATA_T4 data_t4;
		struct LCM_DATA_T5 data_t5;
	};
};


struct LCM_DTS {
	unsigned int parsing;
	unsigned int id;
	unsigned int init_size;
	unsigned int compare_id_size;
	unsigned int suspend_size;
	unsigned int backlight_size;
	unsigned int backlight_cmdq_size;

	struct LCM_PARAMS params;
	struct LCM_DATA init[INIT_SIZE];
	struct LCM_DATA compare_id[COMPARE_ID_SIZE];
	struct LCM_DATA suspend[SUSPEND_SIZE];
	struct LCM_DATA backlight[BACKLIGHT_SIZE];
	struct LCM_DATA backlight_cmdq[BACKLIGHT_CMDQ_SIZE];
};


/* ------------------------------------------------------------------------- */

#define REGFLAG_ESCAPE_ID		(0x00)
#define REGFLAG_DELAY_MS_V3		(0xFF)

struct LCM_setting_table_V3 {
	unsigned char id;
	unsigned char cmd;
	unsigned char count;
	unsigned char para_list[128];
};

/*
 * dtype	---- data type
 * vc		---- virtual channel
 * dlen	---- data length
 * link_state ---- HS:0  LP:1
 * payload ---- payload
 */
struct dsi_cmd_desc {
	unsigned int dtype;
	unsigned int vc;
	unsigned int dlen;
	unsigned int link_state;
	unsigned int cmd;
	char *payload;
};

struct LCM_UTIL_FUNCS {
	void (*set_reset_pin)(unsigned int value);
	void (*set_chip_select)(unsigned int value);
	int (*set_gpio_out)(unsigned int gpio, unsigned int value);
	void (*set_te_pin)(void);

	void (*udelay)(unsigned int us);
	void (*mdelay)(unsigned int ms);
	void (*rar)(unsigned int ms);

	void (*send_cmd)(unsigned int cmd);
	void (*send_data)(unsigned int data);
	unsigned int (*read_data)(void);

	void (*dsi_set_cmdq_V3)(struct LCM_setting_table_V3 *para_list,
			unsigned int size, unsigned char force_update);
	void (*dsi_set_cmdq_V2)(unsigned int cmd, unsigned char count,
			unsigned char *para_list, unsigned char force_update);
	void (*dsi_set_cmdq)(unsigned int *pdata, unsigned int queue_size,
			unsigned char force_update);
	void (*dsi_set_null)(unsigned int cmd, unsigned char count,
			unsigned char *para_list, unsigned char force_update);
	void (*dsi_write_cmd)(unsigned int cmd);
	void (*dsi_write_regs)(unsigned int addr, unsigned int *para,
		unsigned int nums);
	unsigned int (*dsi_read_reg)(void);
	unsigned int (*dsi_dcs_read_lcm_reg)(unsigned char cmd);
	unsigned int (*dsi_dcs_read_lcm_reg_v2)(unsigned char cmd,
		unsigned char *buffer, unsigned char buffer_size);
	void (*wait_transfer_done)(void);

	/*   FIXME: GPIO mode should not be configured in lcm driver*/
	/*      REMOVE ME after GPIO customization is done	*/
	int (*set_gpio_mode)(unsigned int pin, unsigned int mode);
	int (*set_gpio_dir)(unsigned int pin, unsigned int dir);
	int (*set_gpio_pull_enable)(unsigned int pin, unsigned char pull_en);
	long (*set_gpio_lcd_enp_bias)(unsigned int value);
	void (*dsi_set_cmdq_V11)(void *cmdq, unsigned int *pdata,
			unsigned int queue_size, unsigned char force_update);
	void (*dsi_set_cmdq_V22)(void *cmdq, unsigned int cmd,
			unsigned char count, unsigned char *para_list,
			unsigned char force_update);
	void (*dsi_swap_port)(int swap);
	void (*dsi_set_cmdq_V23)(void *cmdq, unsigned int cmd,
		unsigned char count, unsigned char *para_list,
		unsigned char force_update);	/* dual */
	void (*mipi_dsi_cmds_tx)(void *cmdq, struct dsi_cmd_desc *cmds);
	unsigned int (*mipi_dsi_cmds_rx)(char *out,
		struct dsi_cmd_desc *cmds, unsigned int len);
	/*Dynfps*/
	void (*dsi_dynfps_send_cmd)(
		void *cmdq, unsigned int cmd,
		unsigned char count, unsigned char *para_list,
		unsigned char force_update);

};
enum LCM_DRV_IOCTL_CMD {
	LCM_DRV_IOCTL_ENABLE_CMD_MODE = 0x100,
};

struct LCM_DRIVER {
	const char *name;
	void (*set_util_funcs)(const struct LCM_UTIL_FUNCS *util);
	void (*get_params)(struct LCM_PARAMS *params);

	void (*init)(void);
	void (*suspend)(void);
	void (*resume)(void);

	/* for power-on sequence refinement */
	void (*init_power)(void);
	void (*suspend_power)(void);
	void (*resume_power)(void);

	void (*update)(unsigned int x, unsigned int y, unsigned int width,
			unsigned int height);
	unsigned int (*compare_id)(void);
	void (*parse_dts)(const struct LCM_DTS *DTS,
			unsigned char force_update);

	/* /////////////////////////CABC backlight related function */
	void (*set_backlight)(unsigned int level);
	void (*set_backlight_cmdq)(void *handle, unsigned int level);
	bool (*get_hbm_state)(void);
	bool (*get_hbm_wait)(void);
	bool (*set_hbm_wait)(bool wait);
	bool (*set_hbm_cmdq)(bool en, void *qhandle);
	void (*set_pwm)(unsigned int divider);
	unsigned int (*get_pwm)(unsigned int divider);
	void (*set_backlight_mode)(unsigned int mode);
	/* ///////////////////////// */

	int (*adjust_fps)(void *cmdq, int fps, struct LCM_PARAMS *params);
	void (*validate_roi)(int *x, int *y, int *width, int *height);

	void (*scale)(void *handle, enum LCM_SCALE_TYPE scale);
	void (*setroi)(int x, int y, int width, int height, void *handle);
	/* ///////////ESD_RECOVERY////////////////////// */
	unsigned int (*esd_check)(void);
	unsigned int (*esd_recover)(void);
	unsigned int (*check_status)(void);
	unsigned int (*ata_check)(unsigned char *buffer);
	void (*read_fb)(unsigned char *buffer);
	int (*ioctl)(enum LCM_DRV_IOCTL_CMD cmd, unsigned int data);
	/* /////////////////////////////////////////////// */

	void (*enter_idle)(void);
	void (*exit_idle)(void);
	void (*change_fps)(unsigned int mode);

	/* //switch mode */
	void *(*switch_mode)(int mode);
	void (*set_cmd)(void *handle, int *mode, unsigned int cmd_num);
	void (*set_lcm_cmd)(void *handle, unsigned int *lcm_cmd,
		unsigned int *lcm_count, unsigned int *lcm_value);
	/* /////////////PWM///////////////////////////// */
	void (*set_pwm_for_mix)(int enable);

	void (*aod)(int enter);

	/* /////////////DynFPS///////////////////////////// */
	void (*dfps_send_lcm_cmd)(void *cmdq_handle,
		unsigned int from_level, unsigned int to_level);
	bool (*dfps_need_send_cmd)(
	unsigned int from_level, unsigned int to_level);
};

/* LCM Driver Functions */
/* ------------------------------------------------------------------------- */

const struct LCM_DRIVER *LCM_GetDriver(void);
unsigned char which_lcd_module_triple(void);
int lcm_vgp_supply_enable(void);
int lcm_vgp_supply_disable(void);
extern enum LCM_DSI_MODE_CON lcm_dsi_mode;

extern int display_bias_enable(void);
extern int display_bias_disable(void);
extern int display_bias_regulator_init(void);



#endif /* __LCM_DRV_H__ */
