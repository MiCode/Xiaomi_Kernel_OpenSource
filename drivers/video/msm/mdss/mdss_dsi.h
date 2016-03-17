/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
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

#ifndef MDSS_DSI_H
#define MDSS_DSI_H

#include <linux/list.h>
#include <linux/mdss_io_util.h>
#include <linux/irqreturn.h>
#include <linux/pinctrl/consumer.h>
#include <linux/gpio.h>

#include "mdss_panel.h"
#include "mdss_dsi_cmd.h"

#define MMSS_SERDES_BASE_PHY 0x04f01000 /* mmss (De)Serializer CFG */

#define MIPI_OUTP(addr, data) writel_relaxed((data), (addr))
#define MIPI_INP(addr) readl_relaxed(addr)

#define MIPI_OUTP_SECURE(addr, data) writel_relaxed((data), (addr))
#define MIPI_INP_SECURE(addr) readl_relaxed(addr)

#define MIPI_DSI_PRIM 1
#define MIPI_DSI_SECD 2

#define MIPI_DSI_PANEL_VGA	0
#define MIPI_DSI_PANEL_WVGA	1
#define MIPI_DSI_PANEL_WVGA_PT	2
#define MIPI_DSI_PANEL_FWVGA_PT	3
#define MIPI_DSI_PANEL_WSVGA_PT	4
#define MIPI_DSI_PANEL_QHD_PT 5
#define MIPI_DSI_PANEL_WXGA	6
#define MIPI_DSI_PANEL_WUXGA	7
#define MIPI_DSI_PANEL_720P_PT	8
#define DSI_PANEL_MAX	8

#define MDSS_DSI_HW_REV_100		0x10000000	/* 8974    */
#define MDSS_DSI_HW_REV_100_1		0x10000001	/* 8x26    */
#define MDSS_DSI_HW_REV_100_2		0x10000002	/* 8x26v2  */
#define MDSS_DSI_HW_REV_101		0x10010000	/* 8974v2  */
#define MDSS_DSI_HW_REV_101_1		0x10010001	/* 8974Pro */
#define MDSS_DSI_HW_REV_102		0x10020000	/* 8084    */
#define MDSS_DSI_HW_REV_103		0x10030000	/* 8994    */
#define MDSS_DSI_HW_REV_103_1		0x10030001	/* 8916/8936 */

#define NONE_PANEL "none"

enum {		/* mipi dsi panel */
	DSI_VIDEO_MODE,
	DSI_CMD_MODE,
};

enum {
	ST_DSI_CLK_OFF,
	ST_DSI_SUSPEND,
	ST_DSI_RESUME,
	ST_DSI_PLAYING,
	ST_DSI_NUM
};

enum {
	EV_DSI_UPDATE,
	EV_DSI_DONE,
	EV_DSI_TOUT,
	EV_DSI_NUM
};

enum {
	LANDSCAPE = 1,
	PORTRAIT = 2,
};

enum dsi_trigger_type {
	DSI_CMD_MODE_DMA,
	DSI_CMD_MODE_MDP,
};

enum dsi_panel_bl_ctrl {
	BL_PWM,
	BL_WLED,
	BL_DCS_CMD,
	UNKNOWN_CTRL,
};

enum dsi_panel_status_mode {
	ESD_BTA,
	ESD_REG,
	ESD_REG_NT35596,
	ESD_TE,
	ESD_MAX,
};

enum dsi_ctrl_op_mode {
	DSI_LP_MODE,
	DSI_HS_MODE,
};

enum dsi_lane_map_type {
	DSI_LANE_MAP_0123,
	DSI_LANE_MAP_3012,
	DSI_LANE_MAP_2301,
	DSI_LANE_MAP_1230,
	DSI_LANE_MAP_0321,
	DSI_LANE_MAP_1032,
	DSI_LANE_MAP_2103,
	DSI_LANE_MAP_3210,
};

enum dsi_pm_type {
	DSI_CORE_PM,
	DSI_CTRL_PM,
	DSI_PANEL_PM,
	DSI_MAX_PM
};

#define CTRL_STATE_UNKNOWN		0x00
#define CTRL_STATE_PANEL_INIT		BIT(0)
#define CTRL_STATE_MDP_ACTIVE		BIT(1)

#define DSI_NON_BURST_SYNCH_PULSE	0
#define DSI_NON_BURST_SYNCH_EVENT	1
#define DSI_BURST_MODE			2

#define DSI_RGB_SWAP_RGB	0
#define DSI_RGB_SWAP_RBG	1
#define DSI_RGB_SWAP_BGR	2
#define DSI_RGB_SWAP_BRG	3
#define DSI_RGB_SWAP_GRB	4
#define DSI_RGB_SWAP_GBR	5

#define DSI_VIDEO_DST_FORMAT_RGB565		0
#define DSI_VIDEO_DST_FORMAT_RGB666		1
#define DSI_VIDEO_DST_FORMAT_RGB666_LOOSE	2
#define DSI_VIDEO_DST_FORMAT_RGB888		3

#define DSI_CMD_DST_FORMAT_RGB111	0
#define DSI_CMD_DST_FORMAT_RGB332	3
#define DSI_CMD_DST_FORMAT_RGB444	4
#define DSI_CMD_DST_FORMAT_RGB565	6
#define DSI_CMD_DST_FORMAT_RGB666	7
#define DSI_CMD_DST_FORMAT_RGB888	8

#define DSI_INTR_DESJEW_MASK			BIT(31)
#define DSI_INTR_DYNAMIC_REFRESH_MASK		BIT(29)
#define DSI_INTR_DYNAMIC_REFRESH_DONE		BIT(28)
#define DSI_INTR_ERROR_MASK		BIT(25)
#define DSI_INTR_ERROR			BIT(24)
#define DSI_INTR_BTA_DONE_MASK          BIT(21)
#define DSI_INTR_BTA_DONE               BIT(20)
#define DSI_INTR_VIDEO_DONE_MASK	BIT(17)
#define DSI_INTR_VIDEO_DONE		BIT(16)
#define DSI_INTR_CMD_MDP_DONE_MASK	BIT(9)
#define DSI_INTR_CMD_MDP_DONE		BIT(8)
#define DSI_INTR_CMD_DMA_DONE_MASK	BIT(1)
#define DSI_INTR_CMD_DMA_DONE		BIT(0)
/* Update this if more interrupt masks are added in future chipsets */
#define DSI_INTR_TOTAL_MASK		0x2222AA02

#define DSI_INTR_MASK_ALL	\
		(DSI_INTR_DESJEW_MASK | \
		DSI_INTR_DYNAMIC_REFRESH_MASK | \
		DSI_INTR_ERROR_MASK | \
		DSI_INTR_BTA_DONE_MASK | \
		DSI_INTR_VIDEO_DONE_MASK | \
		DSI_INTR_CMD_MDP_DONE_MASK | \
		DSI_INTR_CMD_DMA_DONE_MASK)

#define DSI_INTR_MASK_ALL	\
		(DSI_INTR_DESJEW_MASK | \
		DSI_INTR_DYNAMIC_REFRESH_MASK | \
		DSI_INTR_ERROR_MASK | \
		DSI_INTR_BTA_DONE_MASK | \
		DSI_INTR_VIDEO_DONE_MASK | \
		DSI_INTR_CMD_MDP_DONE_MASK | \
		DSI_INTR_CMD_DMA_DONE_MASK)

#define DSI_CMD_TRIGGER_NONE		0x0	/* mdp trigger */
#define DSI_CMD_TRIGGER_TE		0x02
#define DSI_CMD_TRIGGER_SW		0x04
#define DSI_CMD_TRIGGER_SW_SEOF		0x05	/* cmd dma only */
#define DSI_CMD_TRIGGER_SW_TE		0x06

#define DSI_VIDEO_TERM  BIT(16)
#define DSI_MDP_TERM    BIT(8)
#define DSI_DYNAMIC_TERM    BIT(4)
#define DSI_BTA_TERM    BIT(1)
#define DSI_CMD_TERM    BIT(0)

#define DSI_DATA_LANES_STOP_STATE	0xF
#define DSI_CLK_LANE_STOP_STATE		BIT(4)
#define DSI_DATA_LANES_ENABLED		0xF0

/* offsets for dynamic refresh */
#define DSI_DYNAMIC_REFRESH_CTRL		0x200
#define DSI_DYNAMIC_REFRESH_PIPE_DELAY		0x204
#define DSI_DYNAMIC_REFRESH_PIPE_DELAY2		0x208
#define DSI_DYNAMIC_REFRESH_PLL_DELAY		0x20C

extern struct device dsi_dev;
extern u32 dsi_irq;
extern struct mdss_dsi_ctrl_pdata *ctrl_list[];

struct dsiphy_pll_divider_config {
	u32 clk_rate;
	u32 fb_divider;
	u32 ref_divider_ratio;
	u32 bit_clk_divider;	/* oCLK1 */
	u32 byte_clk_divider;	/* oCLK2 */
	u32 analog_posDiv;
	u32 digital_posDiv;
};

extern struct dsiphy_pll_divider_config pll_divider_config;

struct dsi_clk_mnd_table {
	u8 lanes;
	u8 bpp;
	u8 pll_digital_posDiv;
	u8 pclk_m;
	u8 pclk_n;
	u8 pclk_d;
};

static const struct dsi_clk_mnd_table mnd_table[] = {
	{ 1, 2,  8, 1, 1, 0},
	{ 1, 3, 12, 1, 1, 0},
	{ 2, 2,  4, 1, 1, 0},
	{ 2, 3,  6, 1, 1, 0},
	{ 3, 2,  1, 3, 8, 4},
	{ 3, 3,  4, 1, 1, 0},
	{ 4, 2,  2, 1, 1, 0},
	{ 4, 3,  3, 1, 1, 0},
};

struct dsi_clk_desc {
	u32 src;
	u32 m;
	u32 n;
	u32 d;
	u32 mnd_mode;
	u32 pre_div_func;
};


struct dsi_panel_cmds {
	char *buf;
	int blen;
	struct dsi_cmd_desc *cmds;
	int cmd_cnt;
	int link_state;
};

struct dsi_panel_timing {
	struct mdss_panel_timing timing;
	uint32_t phy_timing[12];
	/* DSI_CLKOUT_TIMING_CTRL */
	char t_clk_post;
	char t_clk_pre;
	struct dsi_panel_cmds on_cmds;
	struct dsi_panel_cmds switch_cmds;
};

struct dsi_kickoff_action {
	struct list_head act_entry;
	void (*action) (void *);
	void *data;
};

struct dsi_drv_cm_data {
	struct dss_io_data phy_regulator_io;
	int phy_disable_refcount;
};

struct dsi_pinctrl_res {
	struct pinctrl *pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
};

struct panel_horizontal_idle {
	int min;
	int max;
	int idle;
};

enum {
	DSI_CTRL_0,
	DSI_CTRL_1,
	DSI_CTRL_MAX,
};

#define DSI_CTRL_LEFT		DSI_CTRL_0
#define DSI_CTRL_RIGHT		DSI_CTRL_1
#define DSI_CTRL_CLK_SLAVE	DSI_CTRL_RIGHT
#define DSI_CTRL_CLK_MASTER	DSI_CTRL_LEFT

#define DSI_BUS_CLKS	BIT(0)
#define DSI_LINK_CLKS	BIT(1)
#define DSI_ALL_CLKS	((DSI_BUS_CLKS) | (DSI_LINK_CLKS))

#define DSI_EV_PLL_UNLOCKED		0x0001
#define DSI_EV_MDP_FIFO_UNDERFLOW	0x0002
#define DSI_EV_DSI_FIFO_EMPTY		0x0004
#define DSI_EV_DLNx_FIFO_OVERFLOW	0x0008
#define DSI_EV_LP_RX_TIMEOUT		0x0010
#define DSI_EV_STOP_HS_CLK_LANE		0x40000000
#define DSI_EV_MDP_BUSY_RELEASE		0x80000000

struct mdss_dsi_ctrl_pdata {
	int ndx;	/* panel_num */
	int (*on) (struct mdss_panel_data *pdata);
	int (*off) (struct mdss_panel_data *pdata);
	int (*low_power_config) (struct mdss_panel_data *pdata, int enable);
	int (*set_col_page_addr)(struct mdss_panel_data *pdata, bool force);
	int (*check_status) (struct mdss_dsi_ctrl_pdata *pdata);
	int (*check_read_status) (struct mdss_dsi_ctrl_pdata *pdata);
	int (*cmdlist_commit)(struct mdss_dsi_ctrl_pdata *ctrl, int from_mdp);
	void (*switch_mode) (struct mdss_panel_data *pdata, int mode);
	struct mdss_panel_data panel_data;
	unsigned char *ctrl_base;
	u32 hw_rev;
	struct dss_io_data ctrl_io;
	struct dss_io_data mmss_misc_io;
	struct dss_io_data phy_io;
	int reg_size;
	u32 bus_clk_cnt;
	u32 link_clk_cnt;
	u32 flags;
	struct clk *mdp_core_clk;
	struct clk *ahb_clk;
	struct clk *axi_clk;
	struct clk *mmss_misc_ahb_clk;
	struct clk *byte_clk;
	struct clk *esc_clk;
	struct clk *pixel_clk;
	struct clk *mux_byte_clk;
	struct clk *mux_pixel_clk;
	struct clk *pll_byte_clk;
	struct clk *pll_pixel_clk;
	struct clk *shadow_byte_clk;
	struct clk *shadow_pixel_clk;
	struct clk *vco_clk;
	u8 ctrl_state;
	int panel_mode;
	int irq_cnt;
	int disp_te_gpio;
	int rst_gpio;
	int disp_en_gpio;
	int bklt_en_gpio;
	int dual_en_gpio;	/*Mipi dsi dual port select pin*/
	int mode_gpio;
	int bklt_ctrl;	/* backlight ctrl */
	bool pwm_pmi;
	int pwm_period;
	int pwm_pmic_gpio;
	int pwm_lpg_chan;
	int bklt_max;
	int new_fps;
	int pwm_enabled;
	int on_cmds_tuning;
	int clk_lane_cnt;
	bool dmap_iommu_map;
	bool panel_bias_vreg;
	bool dsi_irq_line;
	atomic_t te_irq_ready;

	bool cmd_clk_ln_recovery_en;
	bool cmd_sync_wait_broadcast;
	bool cmd_sync_wait_trigger;

	struct mdss_rect roi;
	struct pwm_device *pwm_bl;
	struct dsi_drv_cm_data *shared_ctrl_data;
	u32 pclk_rate;
	u32 byte_clk_rate;
	bool refresh_clk_rate; /* flag to recalculate clk_rate */
	struct dss_module_power power_data[DSI_MAX_PM];
	u32 dsi_irq_mask;
	struct mdss_hw *dsi_hw;
	struct mdss_intf_recovery *recovery;

	struct dsi_panel_cmds on_cmds;
	struct dsi_panel_cmds post_dms_on_cmds;
	struct dsi_panel_cmds off_cmds;
	struct dsi_panel_cmds status_cmds;

	u32 status_cmds_rlen;
	u32 *status_value;
	u32 status_error_count;
	u32 max_status_error_count;

	struct dsi_panel_cmds video2cmd;
	struct dsi_panel_cmds cmd2video;

	struct dcs_cmd_list cmdlist;
	struct completion dma_comp;
	struct completion mdp_comp;
	struct completion video_comp;
	struct completion dynamic_comp;
	struct completion bta_comp;
	spinlock_t irq_lock;
	spinlock_t mdp_lock;
	int mdp_busy;
	struct mutex mutex;
	struct mutex cmd_mutex;
	struct mutex dsi_ctrl_mutex;
	struct regulator *lab; /* vreg handle */
	struct regulator *ibb; /* vreg handle */
	struct mutex clk_lane_mutex;

	u32 ulps_clamp_ctrl_off;
	u32 ulps_phyrst_ctrl_off;
	bool ulps;
	bool core_power;
	bool mmss_clamp;
	bool timing_db_mode;
	bool dsi_pipe_ready;

	struct dsi_buf tx_buf;
	struct dsi_buf rx_buf;
	struct dsi_buf status_buf;
	int status_mode;
	int rx_len;

	struct dsi_pinctrl_res pin_res;

	unsigned long dma_size;
	dma_addr_t dma_addr;
	bool cmd_cfg_restore;
	bool do_unicast;

	int horizontal_idle_cnt;
	struct panel_horizontal_idle *line_idle;
	struct mdss_util_intf *mdss_util;

	bool dfps_status;	/* dynamic refresh status */
};

struct dsi_status_data {
	struct notifier_block fb_notifier;
	struct delayed_work check_status;
	struct msm_fb_data_type *mfd;
};

int dsi_panel_device_register(struct device_node *pan_node,
				struct mdss_dsi_ctrl_pdata *ctrl_pdata);

int mdss_dsi_cmds_tx(struct mdss_dsi_ctrl_pdata *ctrl,
		struct dsi_cmd_desc *cmds, int cnt, int use_dma_tpg);

int mdss_dsi_cmds_rx(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_cmd_desc *cmds, int rlen, int use_dma_tpg);

void mdss_dsi_host_init(struct mdss_panel_data *pdata);
void mdss_dsi_op_mode_config(int mode,
				struct mdss_panel_data *pdata);
void mdss_dsi_restore_intr_mask(struct mdss_dsi_ctrl_pdata *ctrl);
void mdss_dsi_cmd_mode_ctrl(int enable);
void mdp4_dsi_cmd_trigger(void);
void mdss_dsi_cmd_mdp_start(struct mdss_dsi_ctrl_pdata *ctrl);
void mdss_dsi_cmd_bta_sw_trigger(struct mdss_panel_data *pdata);
void mdss_dsi_ack_err_status(struct mdss_dsi_ctrl_pdata *ctrl);
int mdss_dsi_clk_ctrl(struct mdss_dsi_ctrl_pdata *ctrl,
	u8 clk_type, int enable);
void mdss_dsi_clk_req(struct mdss_dsi_ctrl_pdata *ctrl,
				int enable);
void mdss_dsi_controller_cfg(int enable,
				struct mdss_panel_data *pdata);
void mdss_dsi_sw_reset(struct mdss_dsi_ctrl_pdata *ctrl_pdata, bool restore);

irqreturn_t mdss_dsi_isr(int irq, void *ptr);
irqreturn_t hw_vsync_handler(int irq, void *data);
void mdss_dsi_irq_handler_config(struct mdss_dsi_ctrl_pdata *ctrl_pdata);

void mdss_dsi_set_tx_power_mode(int mode, struct mdss_panel_data *pdata);
int mdss_dsi_clk_div_config(struct mdss_panel_info *panel_info,
			    int frame_rate);
int mdss_dsi_clk_refresh(struct mdss_panel_data *pdata);
int mdss_dsi_clk_init(struct platform_device *pdev,
		      struct mdss_dsi_ctrl_pdata *ctrl_pdata);
int mdss_dsi_shadow_clk_init(struct platform_device *pdev,
		      struct mdss_dsi_ctrl_pdata *ctrl_pdata);
int mdss_dsi_pll_1_clk_init(struct platform_device *pdev,
		      struct mdss_dsi_ctrl_pdata *ctrl_pdata);
void mdss_dsi_clk_deinit(struct mdss_dsi_ctrl_pdata *ctrl_pdata);
void mdss_dsi_shadow_clk_deinit(struct mdss_dsi_ctrl_pdata *ctrl_pdata);
int mdss_dsi_enable_bus_clocks(struct mdss_dsi_ctrl_pdata *ctrl_pdata);
void mdss_dsi_disable_bus_clocks(struct mdss_dsi_ctrl_pdata *ctrl_pdata);
int mdss_dsi_panel_reset(struct mdss_panel_data *pdata, int enable);
void mdss_dsi_phy_disable(struct mdss_dsi_ctrl_pdata *ctrl);
void mdss_dsi_cmd_test_pattern(struct mdss_dsi_ctrl_pdata *ctrl);
void mdss_dsi_video_test_pattern(struct mdss_dsi_ctrl_pdata *ctrl);
void mdss_dsi_panel_pwm_cfg(struct mdss_dsi_ctrl_pdata *ctrl);
void mdss_dsi_ctrl_phy_restore(struct mdss_dsi_ctrl_pdata *ctrl);
void mdss_dsi_phy_sw_reset(struct mdss_dsi_ctrl_pdata *ctrl);
void mdss_dsi_phy_init(struct mdss_dsi_ctrl_pdata *ctrl);
void mdss_dsi_ctrl_init(struct device *ctrl_dev,
			struct mdss_dsi_ctrl_pdata *ctrl);
void mdss_dsi_cmd_mdp_busy(struct mdss_dsi_ctrl_pdata *ctrl);
void mdss_dsi_wait4video_done(struct mdss_dsi_ctrl_pdata *ctrl);
int mdss_dsi_en_wait4dynamic_done(struct mdss_dsi_ctrl_pdata *ctrl);
int mdss_dsi_cmdlist_commit(struct mdss_dsi_ctrl_pdata *ctrl, int from_mdp);
void mdss_dsi_cmdlist_kickoff(int intf);
int mdss_dsi_bta_status_check(struct mdss_dsi_ctrl_pdata *ctrl);
int mdss_dsi_reg_status_check(struct mdss_dsi_ctrl_pdata *ctrl);
bool __mdss_dsi_clk_enabled(struct mdss_dsi_ctrl_pdata *ctrl, u8 clk_type);
void mdss_dsi_ctrl_setup(struct mdss_dsi_ctrl_pdata *ctrl);
void mdss_dsi_dln0_phy_err(struct mdss_dsi_ctrl_pdata *ctrl, bool print_en);
void mdss_dsi_lp_cd_rx(struct mdss_dsi_ctrl_pdata *ctrl);
void mdss_dsi_get_hw_revision(struct mdss_dsi_ctrl_pdata *ctrl);
u32 mdss_dsi_panel_cmd_read(struct mdss_dsi_ctrl_pdata *ctrl, char cmd0,
		char cmd1, void (*fxn)(int), char *rbuf, int len);
int mdss_dsi_panel_init(struct device_node *node,
		struct mdss_dsi_ctrl_pdata *ctrl_pdata,
		bool cmd_cfg_cont_splash);
int mdss_dsi_panel_timing_switch(struct mdss_dsi_ctrl_pdata *ctrl_pdata,
			struct mdss_panel_timing *timing);

int mdss_panel_get_dst_fmt(u32 bpp, char mipi_mode, u32 pixel_packing,
				char *dst_format);

int mdss_dsi_register_recovery_handler(struct mdss_dsi_ctrl_pdata *ctrl,
		struct mdss_intf_recovery *recovery);

static inline const char *__mdss_dsi_pm_name(enum dsi_pm_type module)
{
	switch (module) {
	case DSI_CORE_PM:	return "DSI_CORE_PM";
	case DSI_CTRL_PM:	return "DSI_CTRL_PM";
	case DSI_PANEL_PM:	return "PANEL_PM";
	default:		return "???";
	}
}

static inline const char *__mdss_dsi_pm_supply_node_name(
	enum dsi_pm_type module)
{
	switch (module) {
	case DSI_CORE_PM:	return "qcom,core-supply-entries";
	case DSI_CTRL_PM:	return "qcom,ctrl-supply-entries";
	case DSI_PANEL_PM:	return "qcom,panel-supply-entries";
	default:		return "???";
	}
}

static inline bool mdss_dsi_split_display_enabled(void)
{
	/*
	 * currently the only supported mode is split display.
	 * So, if both controllers are initialized, then assume that
	 * split display mode is enabled.
	 */
	return ctrl_list[DSI_CTRL_LEFT] && ctrl_list[DSI_CTRL_RIGHT];
}

static inline bool mdss_dsi_sync_wait_enable(struct mdss_dsi_ctrl_pdata *ctrl)
{
	return ctrl->cmd_sync_wait_broadcast;
}

static inline bool mdss_dsi_sync_wait_trigger(struct mdss_dsi_ctrl_pdata *ctrl)
{
	return ctrl->cmd_sync_wait_broadcast &&
				ctrl->cmd_sync_wait_trigger;
}

static inline bool mdss_dsi_is_left_ctrl(struct mdss_dsi_ctrl_pdata *ctrl)
{
	return ctrl->ndx == DSI_CTRL_LEFT;
}

static inline bool mdss_dsi_is_right_ctrl(struct mdss_dsi_ctrl_pdata *ctrl)
{
	return ctrl->ndx == DSI_CTRL_RIGHT;
}

static inline struct mdss_dsi_ctrl_pdata *mdss_dsi_get_other_ctrl(
					struct mdss_dsi_ctrl_pdata *ctrl)
{
	if (ctrl->ndx == DSI_CTRL_RIGHT)
		return ctrl_list[DSI_CTRL_LEFT];

	return ctrl_list[DSI_CTRL_RIGHT];
}

static inline struct mdss_dsi_ctrl_pdata *mdss_dsi_get_ctrl_by_index(int ndx)
{
	if (ndx >= DSI_CTRL_MAX)
		return NULL;

	return ctrl_list[ndx];
}

static inline bool mdss_dsi_is_ctrl_clk_slave(struct mdss_dsi_ctrl_pdata *ctrl)
{
	return mdss_dsi_split_display_enabled() &&
		(ctrl->ndx == DSI_CTRL_CLK_SLAVE);
}

static inline bool mdss_dsi_is_te_based_esd(struct mdss_dsi_ctrl_pdata *ctrl)
{
	return (ctrl->status_mode == ESD_TE) &&
		gpio_is_valid(ctrl->disp_te_gpio) &&
		mdss_dsi_is_left_ctrl(ctrl);
}

static inline struct mdss_dsi_ctrl_pdata *mdss_dsi_get_ctrl_clk_master(void)
{
	return ctrl_list[DSI_CTRL_CLK_MASTER];
}

static inline struct mdss_dsi_ctrl_pdata *mdss_dsi_get_ctrl_clk_slave(void)
{
	return ctrl_list[DSI_CTRL_CLK_SLAVE];
}

static inline bool mdss_dsi_is_panel_off(struct mdss_panel_data *pdata)
{
	return mdss_panel_is_power_off(pdata->panel_info.panel_power_state);
}

static inline bool mdss_dsi_is_panel_on(struct mdss_panel_data *pdata)
{
	return mdss_panel_is_power_on(pdata->panel_info.panel_power_state);
}

static inline bool mdss_dsi_is_panel_on_interactive(
	struct mdss_panel_data *pdata)
{
	return mdss_panel_is_power_on_interactive(
		pdata->panel_info.panel_power_state);
}

static inline bool mdss_dsi_is_panel_on_lp(struct mdss_panel_data *pdata)
{
	return mdss_panel_is_power_on_lp(pdata->panel_info.panel_power_state);
}

static inline bool mdss_dsi_ulps_feature_enabled(
	struct mdss_panel_data *pdata)
{
	return pdata->panel_info.ulps_feature_enabled;
}

static inline bool mdss_dsi_cmp_panel_reg(struct dsi_buf status_buf,
	u32 *status_val, int i)
{
	return status_buf.data[i] == status_val[i];
}

#endif /* MDSS_DSI_H */
