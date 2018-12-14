/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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
#include "mdss_dsi_clk.h"

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
#define MDSS_DSI_HW_REV_104             0x10040000      /* 8996   */
#define MDSS_DSI_HW_REV_104_1           0x10040001      /* 8996   */
#define MDSS_DSI_HW_REV_104_2           0x10040002      /* 8937   */

#define MDSS_DSI_HW_REV_STEP_0		0x0
#define MDSS_DSI_HW_REV_STEP_1		0x1
#define MDSS_DSI_HW_REV_STEP_2		0x2

#define MDSS_STATUS_TE_WAIT_MAX		3
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
	ESD_NONE = 0,
	ESD_BTA,
	ESD_REG,
	ESD_REG_NT35596,
	ESD_TE_NT35596,
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
	/* PANEL_PM not used as part of power_data in dsi_shared_data */
	DSI_PANEL_PM,
	DSI_CORE_PM,
	DSI_CTRL_PM,
	DSI_PHY_PM,
	DSI_MAX_PM
};

/*
 * DSI controller states.
 *	CTRL_STATE_UNKNOWN - Unknown state of DSI controller.
 *	CTRL_STATE_PANEL_INIT - State specifies that the panel is initialized.
 *	CTRL_STATE_MDP_ACTIVE - State specifies that MDP is ready to send
 *				data to DSI.
 *	CTRL_STATE_DSI_ACTIVE - State specifies that DSI controller/PHY is
 *				initialized.
 */
#define CTRL_STATE_UNKNOWN		0x00
#define CTRL_STATE_PANEL_INIT		BIT(0)
#define CTRL_STATE_MDP_ACTIVE		BIT(1)
#define CTRL_STATE_DSI_ACTIVE		BIT(2)
#define CTRL_STATE_PANEL_LP		BIT(3)

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

#define MAX_ERR_INDEX			10

extern struct device dsi_dev;
extern u32 dsi_irq;
extern struct mdss_dsi_ctrl_pdata *ctrl_list[];

#ifdef CONFIG_PROJECT_VINCE
extern bool synaptics_gesture_func_on;
extern bool synaptics_gesture_func_on_lansi;
extern bool NVT_gesture_func_on;
#endif

enum {
	DSI_CTRL_0,
	DSI_CTRL_1,
	DSI_CTRL_MAX,
};

/*
 * Common DSI properties for each controller. The DSI root probe will create the
 * shared_data struct which should be accessible to each controller. The goal is
 * to only access ctrl_pdata and ctrl_pdata->shared_data during the lifetime of
 * each controller i.e. mdss_dsi_res should not be used directly.
 */
struct dsi_shared_data {
	u32 hw_config; /* DSI setup configuration i.e. single/dual/split */
	u32 pll_src_config; /* PLL source selection for DSI link clocks */
	u32 hw_rev; /* DSI h/w revision */
	u32 phy_rev; /* DSI PHY revision*/

	/* DSI ULPS clamp register offsets */
	u32 ulps_clamp_ctrl_off;
	u32 ulps_phyrst_ctrl_off;

	bool cmd_clk_ln_recovery_en;
	bool dsi0_active;
	bool dsi1_active;

	/* DSI bus clocks */
	struct clk *mdp_core_clk;
	struct clk *ahb_clk;
	struct clk *axi_clk;
	struct clk *mmss_misc_ahb_clk;

	/* Other shared clocks */
	struct clk *ext_byte0_clk;
	struct clk *ext_pixel0_clk;
	struct clk *ext_byte1_clk;
	struct clk *ext_pixel1_clk;

	/* Clock sources for branch clocks */
	struct clk *byte0_parent;
	struct clk *pixel0_parent;
	struct clk *byte1_parent;
	struct clk *pixel1_parent;

	/* DSI core regulators */
	struct dss_module_power power_data[DSI_MAX_PM];

	/* Shared mutex for DSI PHY regulator */
	struct mutex phy_reg_lock;

	/* Data bus(AXI) scale settings */
	struct msm_bus_scale_pdata *bus_scale_table;
	u32 bus_handle;
	u32 bus_refcount;

	/* Shared mutex for pm_qos ref count */
	struct mutex pm_qos_lock;
	u32 pm_qos_req_cnt;
};

struct mdss_dsi_data {
	bool res_init;
	struct platform_device *pdev;
	/* List of controller specific struct data */
	struct mdss_dsi_ctrl_pdata *ctrl_pdata[DSI_CTRL_MAX];
	/*
	 * This structure should hold common data structures like
	 * mutex, clocks, regulator information, setup information
	 */
	struct dsi_shared_data *shared_data;
};

/*
 * enum mdss_dsi_hw_config - Supported DSI h/w configurations
 *
 * @SINGLE_DSI:		Single DSI panel driven by either DSI0 or DSI1.
 * @DUAL_DSI:		Two DSI panels driven independently by DSI0 & DSI1.
 * @SPLIT_DSI:		A split DSI panel driven by both the DSI controllers
 *			with the DSI link clocks sourced by a single DSI PLL.
 */
enum mdss_dsi_hw_config {
	SINGLE_DSI,
	DUAL_DSI,
	SPLIT_DSI,
};

/*
 * enum mdss_dsi_pll_src_config - The PLL source for DSI link clocks
 *
 * @PLL_SRC_0:		The link clocks are sourced out of PLL0.
 * @PLL_SRC_1:		The link clocks are sourced out of PLL1.
 */
enum mdss_dsi_pll_src_config {
	PLL_SRC_DEFAULT,
	PLL_SRC_0,
	PLL_SRC_1,
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
	uint32_t phy_timing_8996[40];
	/* DSI_CLKOUT_TIMING_CTRL */
	char t_clk_post;
	char t_clk_pre;
	struct dsi_panel_cmds on_cmds;
	struct dsi_panel_cmds post_panel_on_cmds;
	struct dsi_panel_cmds switch_cmds;
};

struct dsi_kickoff_action {
	struct list_head act_entry;
	void (*action) (void *);
	void *data;
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

struct dsi_err_container {
	u32 fifo_err_cnt;
	u32 phy_err_cnt;
	u32 err_cnt;
	u32 err_time_delta;
	u32 max_err_index;

	u32 index;
	s64 err_time[MAX_ERR_INDEX];
};

#define DSI_CTRL_LEFT		DSI_CTRL_0
#define DSI_CTRL_RIGHT		DSI_CTRL_1
#define DSI_CTRL_CLK_SLAVE	DSI_CTRL_RIGHT
#define DSI_CTRL_CLK_MASTER	DSI_CTRL_LEFT

#define DSI_EV_PLL_UNLOCKED		0x0001
#define DSI_EV_DLNx_FIFO_UNDERFLOW	0x0002
#define DSI_EV_DSI_FIFO_EMPTY		0x0004
#define DSI_EV_DLNx_FIFO_OVERFLOW	0x0008
#define DSI_EV_LP_RX_TIMEOUT		0x0010
#define DSI_EV_STOP_HS_CLK_LANE		0x40000000
#define DSI_EV_MDP_BUSY_RELEASE		0x80000000

#define MDSS_DSI_VIDEO_COMPRESSION_MODE_CTRL	0x02a0
#define MDSS_DSI_VIDEO_COMPRESSION_MODE_CTRL2	0x02a4
#define MDSS_DSI_COMMAND_COMPRESSION_MODE_CTRL	0x02a8
#define MDSS_DSI_COMMAND_COMPRESSION_MODE_CTRL2	0x02ac
#define MDSS_DSI_COMMAND_COMPRESSION_MODE_CTRL3	0x02b0
#define MSM_DBA_CHIP_NAME_MAX_LEN				20

struct mdss_dsi_ctrl_pdata {
	int ndx;	/* panel_num */
	int (*on) (struct mdss_panel_data *pdata);
	int (*post_panel_on)(struct mdss_panel_data *pdata);
	int (*off) (struct mdss_panel_data *pdata);
	int (*low_power_config) (struct mdss_panel_data *pdata, int enable);
	int (*set_col_page_addr)(struct mdss_panel_data *pdata, bool force);
	int (*check_status) (struct mdss_dsi_ctrl_pdata *pdata);
	int (*check_read_status) (struct mdss_dsi_ctrl_pdata *pdata);
	int (*cmdlist_commit)(struct mdss_dsi_ctrl_pdata *ctrl, int from_mdp);
	void (*switch_mode) (struct mdss_panel_data *pdata, int mode);
	struct mdss_panel_data panel_data;
	unsigned char *ctrl_base;
	struct dss_io_data ctrl_io;
	struct dss_io_data mmss_misc_io;
	struct dss_io_data phy_io;
	struct dss_io_data phy_regulator_io;
	int reg_size;
	u32 flags;
	struct clk *byte_clk;
	struct clk *esc_clk;
	struct clk *pixel_clk;
	struct clk *mux_byte_clk;
	struct clk *mux_pixel_clk;
	struct clk *pll_byte_clk;
	struct clk *pll_pixel_clk;
	struct clk *shadow_byte_clk;
	struct clk *shadow_pixel_clk;
	struct clk *byte_clk_rcg;
	struct clk *pixel_clk_rcg;
	struct clk *vco_dummy_clk;
	u8 ctrl_state;
	int panel_mode;
	int irq_cnt;
	int disp_te_gpio;
	int rst_gpio;
	int disp_en_gpio;
	int bklt_en_gpio;
	int mode_gpio;
	int intf_mux_gpio;
	int bklt_ctrl;	/* backlight ctrl */
	bool pwm_pmi;
	int pwm_period;
	int pwm_pmic_gpio;
	int pwm_lpg_chan;
	int bklt_max;
	int new_fps;
	int pwm_enabled;
	int clk_lane_cnt;
	bool dmap_iommu_map;
	bool dsi_irq_line;
	bool dcs_cmd_insert;
	atomic_t te_irq_ready;
	bool idle;

	bool cmd_sync_wait_broadcast;
	bool cmd_sync_wait_trigger;

	struct mdss_rect roi;
	struct pwm_device *pwm_bl;
	u32 pclk_rate;
	u32 byte_clk_rate;
	u32 pclk_rate_bkp;
	u32 byte_clk_rate_bkp;
	bool refresh_clk_rate; /* flag to recalculate clk_rate */
	struct dss_module_power panel_power_data;
	struct dss_module_power power_data[DSI_MAX_PM]; /* for 8x10 */
	u32 dsi_irq_mask;
	struct mdss_hw *dsi_hw;
	struct mdss_intf_recovery *recovery;
	struct mdss_intf_recovery *mdp_callback;

	struct dsi_panel_cmds CABC_on_cmds;
	struct dsi_panel_cmds CABC_off_cmds;
	struct dsi_panel_cmds CE_on_cmds;
	struct dsi_panel_cmds CE_off_cmds;
	struct dsi_panel_cmds cold_gamma_cmds;
	struct dsi_panel_cmds warm_gamma_cmds;
	struct dsi_panel_cmds default_gamma_cmds;
	struct dsi_panel_cmds white_gamma_cmds;
#ifdef CONFIG_PROJECT_VINCE
	struct dsi_panel_cmds sRGB_on_cmds;
	struct dsi_panel_cmds sRGB_off_cmds;
#endif
	struct dsi_panel_cmds PM1_cmds;
	struct dsi_panel_cmds PM2_cmds;
	struct dsi_panel_cmds PM3_cmds;
	struct dsi_panel_cmds PM4_cmds;
	struct dsi_panel_cmds PM5_cmds;
	struct dsi_panel_cmds PM6_cmds;
	struct dsi_panel_cmds PM7_cmds;
	struct dsi_panel_cmds PM8_cmds;

	struct dsi_panel_cmds on_cmds;
	struct dsi_panel_cmds post_dms_on_cmds;
	struct dsi_panel_cmds post_panel_on_cmds;
	struct dsi_panel_cmds off_cmds;
	struct dsi_panel_cmds lp_on_cmds;
	struct dsi_panel_cmds lp_off_cmds;
	struct dsi_panel_cmds status_cmds;
	struct dsi_panel_cmds idle_on_cmds; /* for lp mode */
	struct dsi_panel_cmds idle_off_cmds;
	u32 *status_valid_params;
	u32 *status_cmds_rlen;
	u32 *status_value;
	unsigned char *return_buf;
	u32 groups; /* several alternative values to compare */
	u32 status_error_count;
	u32 max_status_error_count;

	struct dsi_panel_cmds video2cmd;
	struct dsi_panel_cmds cmd2video;

	char pps_buf[DSC_PPS_LEN];	/* dsc pps */

	struct dcs_cmd_list cmdlist;
	struct completion dma_comp;
	struct completion mdp_comp;
	struct completion video_comp;
	struct completion dynamic_comp;
	struct completion bta_comp;
	struct completion te_irq_comp;
	spinlock_t irq_lock;
	spinlock_t mdp_lock;
	int mdp_busy;
	struct mutex mutex;
	struct mutex cmd_mutex;
	struct mutex cmdlist_mutex;
	struct regulator *lab; /* vreg handle */
	struct regulator *ibb; /* vreg handle */
	struct mutex clk_lane_mutex;

	bool null_insert_enabled;
	bool ulps;
	bool core_power;
	bool mmss_clamp;
	char dlane_swap;	/* data lane swap */
	bool is_phyreg_enabled;
	bool burst_mode_enabled;

	struct dsi_buf tx_buf;
	struct dsi_buf rx_buf;
	struct dsi_buf status_buf;
	int status_mode;
	int rx_len;
	int cur_max_pkt_size;

	struct dsi_pinctrl_res pin_res;

	unsigned long dma_size;
	dma_addr_t dma_addr;
	bool cmd_cfg_restore;
	bool do_unicast;

	bool idle_enabled;
	int horizontal_idle_cnt;
	struct panel_horizontal_idle *line_idle;
	struct mdss_util_intf *mdss_util;
	struct dsi_shared_data *shared_data;

	void *clk_mngr;
	void *dsi_clk_handle;
	void *mdp_clk_handle;
	int m_dsi_vote_cnt;
	int m_mdp_vote_cnt;
	/* debugfs structure */
	struct mdss_dsi_debugfs_info *debugfs_info;

	struct dsi_err_container err_cont;

	struct kobject *kobj;
	int fb_node;

	/* DBA data */
	struct workqueue_struct *workq;
	struct delayed_work dba_work;
	char bridge_name[MSM_DBA_CHIP_NAME_MAX_LEN];
	uint32_t bridge_index;
	bool ds_registered;

	bool timing_db_mode;
	bool update_phy_timing; /* flag to recalculate PHY timings */

	bool phy_power_off;
};

struct dsi_status_data {
	struct notifier_block fb_notifier;
	struct delayed_work check_status;
	struct msm_fb_data_type *mfd;
};

void mdss_dsi_read_hw_revision(struct mdss_dsi_ctrl_pdata *ctrl);
int dsi_panel_device_register(struct platform_device *ctrl_pdev,
	struct device_node *pan_node, struct mdss_dsi_ctrl_pdata *ctrl_pdata);

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
bool mdss_dsi_ack_err_status(struct mdss_dsi_ctrl_pdata *ctrl);
int mdss_dsi_clk_ctrl(struct mdss_dsi_ctrl_pdata *ctrl, void *clk_handle,
	enum mdss_dsi_clk_type clk_type, enum mdss_dsi_clk_state clk_state);
void mdss_dsi_clk_req(struct mdss_dsi_ctrl_pdata *ctrl,
	struct dsi_panel_clk_ctrl *clk_ctrl);
void mdss_dsi_controller_cfg(int enable,
				struct mdss_panel_data *pdata);
void mdss_dsi_sw_reset(struct mdss_dsi_ctrl_pdata *ctrl_pdata, bool restore);
int mdss_dsi_wait_for_lane_idle(struct mdss_dsi_ctrl_pdata *ctrl);

irqreturn_t mdss_dsi_isr(int irq, void *ptr);
irqreturn_t hw_vsync_handler(int irq, void *data);
void disable_esd_thread(void);
void mdss_dsi_irq_handler_config(struct mdss_dsi_ctrl_pdata *ctrl_pdata);

void mdss_dsi_set_tx_power_mode(int mode, struct mdss_panel_data *pdata);
int mdss_dsi_clk_div_config(struct mdss_panel_info *panel_info,
			    int frame_rate);
int mdss_dsi_clk_refresh(struct mdss_panel_data *pdata, bool update_phy);
int mdss_dsi_link_clk_init(struct platform_device *pdev,
		      struct mdss_dsi_ctrl_pdata *ctrl_pdata);
void mdss_dsi_link_clk_deinit(struct device *dev,
			struct mdss_dsi_ctrl_pdata *ctrl_pdata);
int mdss_dsi_core_clk_init(struct platform_device *pdev,
			struct dsi_shared_data *sdata);
void mdss_dsi_core_clk_deinit(struct device *dev,
			struct dsi_shared_data *sdata);
int mdss_dsi_shadow_clk_init(struct platform_device *pdev,
		      struct mdss_dsi_ctrl_pdata *ctrl_pdata);
void mdss_dsi_shadow_clk_deinit(struct device *dev,
			struct mdss_dsi_ctrl_pdata *ctrl_pdata);
int mdss_dsi_pre_clkoff_cb(void *priv,
			   enum mdss_dsi_clk_type clk_type,
			   enum mdss_dsi_clk_state new_state);
int mdss_dsi_post_clkoff_cb(void *priv,
			    enum mdss_dsi_clk_type clk_type,
			    enum mdss_dsi_clk_state curr_state);
int mdss_dsi_post_clkon_cb(void *priv,
			   enum mdss_dsi_clk_type clk_type,
			   enum mdss_dsi_clk_state curr_state);
int mdss_dsi_pre_clkon_cb(void *priv,
			  enum mdss_dsi_clk_type clk_type,
			  enum mdss_dsi_clk_state new_state);
int mdss_dsi_panel_reset(struct mdss_panel_data *pdata, int enable);
void mdss_dsi_phy_disable(struct mdss_dsi_ctrl_pdata *ctrl);
void mdss_dsi_cmd_test_pattern(struct mdss_dsi_ctrl_pdata *ctrl);
void mdss_dsi_video_test_pattern(struct mdss_dsi_ctrl_pdata *ctrl);
void mdss_dsi_panel_pwm_cfg(struct mdss_dsi_ctrl_pdata *ctrl);
bool mdss_dsi_panel_pwm_enable(struct mdss_dsi_ctrl_pdata *ctrl);
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
int mdss_dsi_TE_NT35596_check(struct mdss_dsi_ctrl_pdata *ctrl);
bool __mdss_dsi_clk_enabled(struct mdss_dsi_ctrl_pdata *ctrl, u8 clk_type);
void mdss_dsi_ctrl_setup(struct mdss_dsi_ctrl_pdata *ctrl);
bool mdss_dsi_dln0_phy_err(struct mdss_dsi_ctrl_pdata *ctrl, bool print_en);
void mdss_dsi_lp_cd_rx(struct mdss_dsi_ctrl_pdata *ctrl);
void mdss_dsi_read_phy_revision(struct mdss_dsi_ctrl_pdata *ctrl);
int mdss_dsi_panel_cmd_read(struct mdss_dsi_ctrl_pdata *ctrl, char cmd0,
		char cmd1, void (*fxn)(int), char *rbuf, int len);
int mdss_dsi_panel_init(struct device_node *node,
		struct mdss_dsi_ctrl_pdata *ctrl_pdata,
		int ndx);
int mdss_dsi_panel_timing_switch(struct mdss_dsi_ctrl_pdata *ctrl_pdata,
			struct mdss_panel_timing *timing);

int mdss_panel_parse_bl_settings(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl_pdata);
int mdss_panel_get_dst_fmt(u32 bpp, char mipi_mode, u32 pixel_packing,
				char *dst_format);

int mdss_dsi_register_recovery_handler(struct mdss_dsi_ctrl_pdata *ctrl,
		struct mdss_intf_recovery *recovery);
void mdss_dsi_unregister_bl_settings(struct mdss_dsi_ctrl_pdata *ctrl_pdata);
void mdss_dsi_panel_dsc_pps_send(struct mdss_dsi_ctrl_pdata *ctrl,
				struct mdss_panel_info *pinfo);
void mdss_dsi_dsc_config(struct mdss_dsi_ctrl_pdata *ctrl,
	struct dsc_desc *dsc);
void mdss_dsi_dfps_config_8996(struct mdss_dsi_ctrl_pdata *ctrl);
void mdss_dsi_set_burst_mode(struct mdss_dsi_ctrl_pdata *ctrl);
void mdss_dsi_set_reg(struct mdss_dsi_ctrl_pdata *ctrl, int off,
	u32 mask, u32 val);
int mdss_dsi_phy_pll_reset_status(struct mdss_dsi_ctrl_pdata *ctrl);
int mdss_dsi_panel_power_ctrl(struct mdss_panel_data *pdata, int power_state);

static inline const char *__mdss_dsi_pm_name(enum dsi_pm_type module)
{
	switch (module) {
	case DSI_CORE_PM:	return "DSI_CORE_PM";
	case DSI_CTRL_PM:	return "DSI_CTRL_PM";
	case DSI_PHY_PM:	return "DSI_PHY_PM";
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
	case DSI_PHY_PM:	return "qcom,phy-supply-entries";
	case DSI_PANEL_PM:	return "qcom,panel-supply-entries";
	default:		return "???";
	}
}

#ifdef CONFIG_PROJECT_VINCE
/*Add by HQ-zmc [Date: 2017-12-18 11:02:02]*/
struct NVT_CSOT_ESD{
	bool nova_csot_panel;
	bool ESD_TE_status;
};

struct NVT_CSOT_ESD *get_nvt_csot_esd_status(void);
#endif

void mdss_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_panel_cmds *pcmds, u32 flags);

static inline u32 mdss_dsi_get_hw_config(struct dsi_shared_data *sdata)
{
	return sdata->hw_config;
}

static inline bool mdss_dsi_is_hw_config_single(struct dsi_shared_data *sdata)
{
	return mdss_dsi_get_hw_config(sdata) == SINGLE_DSI;
}

static inline bool mdss_dsi_is_hw_config_split(struct dsi_shared_data *sdata)
{
	return mdss_dsi_get_hw_config(sdata) == SPLIT_DSI;
}

static inline bool mdss_dsi_is_hw_config_dual(struct dsi_shared_data *sdata)
{
	return mdss_dsi_get_hw_config(sdata) == DUAL_DSI;
}

static inline bool mdss_dsi_get_pll_src_config(struct dsi_shared_data *sdata)
{
	return sdata->pll_src_config;
}

/*
 * mdss_dsi_is_pll_src_default: Check if the DSI device uses default PLL src
 * For single-dsi and dual-dsi configuration, PLL source need not be
 * explicitly specified. In this case, the default PLL source configuration
 * is assumed.
 *
 * @sdata: pointer to DSI shared data structure
 */
static inline bool mdss_dsi_is_pll_src_default(struct dsi_shared_data *sdata)
{
	return sdata->pll_src_config == PLL_SRC_DEFAULT;
}

/*
 * mdss_dsi_is_pll_src_pll0: Check if the PLL source for a DSI device is PLL0
 * The function is only valid if the DSI configuration is single/split DSI.
 * Not valid for dual DSI configuration.
 *
 * @sdata: pointer to DSI shared data structure
 */
static inline bool mdss_dsi_is_pll_src_pll0(struct dsi_shared_data *sdata)
{
	return sdata->pll_src_config == PLL_SRC_0;
}

/*
 * mdss_dsi_is_pll_src_pll1: Check if the PLL source for a DSI device is PLL1
 * The function is only valid if the DSI configuration is single/split DSI.
 * Not valid for dual DSI configuration.
 *
 * @sdata: pointer to DSI shared data structure
 */
static inline bool mdss_dsi_is_pll_src_pll1(struct dsi_shared_data *sdata)
{
	return sdata->pll_src_config == PLL_SRC_1;
}

static inline bool mdss_dsi_is_dsi0_active(struct dsi_shared_data *sdata)
{
	return sdata->dsi0_active;
}

static inline bool mdss_dsi_is_dsi1_active(struct dsi_shared_data *sdata)
{
	return sdata->dsi1_active;
}

static inline u32 mdss_dsi_get_phy_revision(struct mdss_dsi_ctrl_pdata *ctrl)
{
	return ctrl->shared_data->phy_rev;
}

static inline const char *mdss_dsi_get_fb_name(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_panel_info *pinfo = &(ctrl->panel_data.panel_info);

	if (mdss_dsi_is_hw_config_dual(ctrl->shared_data)) {
		if (pinfo->is_prim_panel)
			return "qcom,mdss-fb-map-prim";
		else
			return "qcom,mdss-fb-map-sec";
	} else {
		return "qcom,mdss-fb-map-prim";
	}
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

static inline bool mdss_dsi_is_ctrl_clk_master(struct mdss_dsi_ctrl_pdata *ctrl)
{
	return mdss_dsi_is_hw_config_split(ctrl->shared_data) &&
		(ctrl->ndx == DSI_CTRL_CLK_MASTER);
}

static inline bool mdss_dsi_is_ctrl_clk_slave(struct mdss_dsi_ctrl_pdata *ctrl)
{
	return mdss_dsi_is_hw_config_split(ctrl->shared_data) &&
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

static inline bool mdss_dsi_is_panel_on_ulp(struct mdss_panel_data *pdata)
{
	return mdss_panel_is_power_on_ulp(pdata->panel_info.panel_power_state);
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
