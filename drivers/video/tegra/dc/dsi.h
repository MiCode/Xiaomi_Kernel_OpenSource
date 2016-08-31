/*
 * drivers/video/tegra/dc/dsi.h
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION, All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __DRIVERS_VIDEO_TEGRA_DC_DSI_H__
#define __DRIVERS_VIDEO_TEGRA_DC_DSI_H__

/* Defines the DSI phy timing parameters */
struct dsi_phy_timing_inclk {
	unsigned	t_hsdexit;
	unsigned	t_hstrail;
	unsigned	t_hsprepare;
	unsigned	t_datzero;

	unsigned	t_clktrail;
	unsigned	t_clkpost;
	unsigned	t_clkzero;
	unsigned	t_tlpx;

	unsigned	t_clkpre;
	unsigned	t_clkprepare;
	unsigned	t_wakeup;

	unsigned	t_taget;
	unsigned	t_tasure;
	unsigned	t_tago;
};

struct dsi_status {
	unsigned init:2;

	unsigned lphs:2;

	unsigned vtype:2;
	unsigned driven:2;

	unsigned clk_out:2;
	unsigned clk_mode:2;
	unsigned clk_burst:2;

	unsigned lp_op:2;

	unsigned dc_stream:1;
};
#define MAX_DSI_INSTANCE	2

struct tegra_dc_dsi_data {
	struct tegra_dc *dc;
	void __iomem *base[MAX_DSI_INSTANCE];
	struct resource *base_res[MAX_DSI_INSTANCE];

	struct clk *dc_clk;
	struct clk *dsi_clk[MAX_DSI_INSTANCE];
	struct clk *dsi_fixed_clk;
	struct clk *dsi_lp_clk[MAX_DSI_INSTANCE];
	bool clk_ref;

	struct mutex lock;

	int max_instances;

	struct tegra_dsi_out_ops *out_ops;
	void			*out_data;

	/* data from board info */
	struct tegra_dsi_out info;

	struct tegra_mipi_cal *mipi_cal;

	struct dsi_status status;

	struct dsi_phy_timing_inclk phy_timing;

	bool ulpm;
	bool enabled;
	bool host_suspended;
	struct mutex host_lock;
	struct delayed_work idle_work;
	unsigned long idle_delay;
	atomic_t host_ref;

	u8 driven_mode;
	u8 controller_index;

	u8 pixel_scaler_mul;
	u8 pixel_scaler_div;

	struct tegra_dc_shift_clk_div default_shift_clk_div;
	u32 default_pixel_clk_khz;
	u32 default_hs_clk_khz;

	struct tegra_dc_shift_clk_div shift_clk_div;
	u32 target_hs_clk_khz;
	u32 target_lp_clk_khz;

	u32 syncpt_id;
	u32 syncpt_val;

	u32 current_bit_clk_ps;
	u32 current_dsi_clk_khz;

	struct regulator *avdd_dsi_csi;

	u32 dsi_control_val;
};

/* Max number of data lanes supported */
#if defined(CONFIG_ARCH_TEGRA_3x_SOC) || \
	defined(CONFIG_ARCH_TEGRA_2x_SOC)
#define MAX_DSI_DATA_LANES	2
#else
#define MAX_DSI_DATA_LANES	8
#endif

/* Default Peripheral reset timeout */
#define DSI_PR_TO_VALUE		0x2000

/* DCS commands for command mode */
#define DSI_ENTER_PARTIAL_MODE	0x12
#define DSI_SET_PIXEL_FORMAT	0x3A
#define DSI_AREA_COLOR_MODE	0x4C
#define DSI_SET_PARTIAL_AREA	0x30
#define DSI_SET_PAGE_ADDRESS	0x2B
#define DSI_SET_ADDRESS_MODE	0x36
#define DSI_SET_COLUMN_ADDRESS	0x2A
#define DSI_WRITE_MEMORY_START	0x2C
#define DSI_WRITE_MEMORY_CONTINUE	0x3C
#define DSI_MAX_COMMAND_DELAY_USEC	250000
#define DSI_COMMAND_DELAY_STEPS_USEC	10

/* Trigger message */
#define DSI_ESCAPE_CMD	0x87
#define DSI_ACK_NO_ERR	0x84

/* DSI return packet types */
#define GEN_LONG_RD_RES 0x1A
#define DCS_LONG_RD_RES 0x1C
#define GEN_1_BYTE_SHORT_RD_RES 0x11
#define DCS_1_BYTE_SHORT_RD_RES 0x21
#define GEN_2_BYTE_SHORT_RD_RES 0x12
#define DCS_2_BYTE_SHORT_RD_RES 0x22
#define ACK_ERR_RES 0x02

/* End of Transmit command for HS mode */
#define DSI_CMD_HS_EOT_PACKAGE          0x000F0F08

/* Delay required after issuing the trigger*/
#define DSI_COMMAND_COMPLETION_DELAY_USEC   5

#define DSI_DELAY_FOR_READ_FIFO 5

/* Dsi virtual channel bit position, refer to the DSI specs */
#define DSI_VIR_CHANNEL_BIT_POSITION	6

/* DSI packet commands from Host to peripherals */
enum {
	dsi_command_v_sync_start = 0x01,
	dsi_command_v_sync_end = 0x11,
	dsi_command_h_sync_start = 0x21,
	dsi_command_h_sync_end = 0x31,
	dsi_command_end_of_transaction = 0x08,
	dsi_command_blanking = 0x19,
	dsi_command_null_packet = 0x09,
	dsi_command_h_active_length_16bpp = 0x0E,
	dsi_command_h_active_length_18bpp = 0x1E,
	dsi_command_h_active_length_18bpp_np = 0x2E,
	dsi_command_h_active_length_24bpp = 0x3E,
	dsi_command_h_sync_active = dsi_command_blanking,
	dsi_command_h_back_porch = dsi_command_blanking,
	dsi_command_h_front_porch = dsi_command_blanking,
	dsi_command_writ_no_param = 0x05,
	dsi_command_long_write = 0x39,
	dsi_command_max_return_pkt_size = 0x37,
	dsi_command_generic_read_request_with_2_param = 0x24,
	dsi_command_dcs_read_with_no_params = 0x06,
};

/* Maximum polling time for reading the dsi status register */
#define DSI_STATUS_POLLING_DURATION_USEC    100000
#define DSI_STATUS_POLLING_DELAY_USEC       100

/*
  * Horizontal Sync Blank Packet Over head
  * DSI_overhead = size_of(HS packet header)
  *             + size_of(BLANK packet header) + size_of(checksum)
  * DSI_overhead = 4 + 4 + 2 = 10
  */
#define DSI_HSYNC_BLNK_PKT_OVERHEAD  10

/*
 * Horizontal Front Porch Packet Overhead
 * DSI_overhead = size_of(checksum)
 *            + size_of(BLANK packet header) + size_of(checksum)
 * DSI_overhead = 2 + 4 + 2 = 8
 */
#define DSI_HFRONT_PORCH_PKT_OVERHEAD 8

/*
 * Horizontal Back Porch Packet
 * DSI_overhead = size_of(HE packet header)
 *            + size_of(BLANK packet header) + size_of(checksum)
 *            + size_of(RGB packet header)
 * DSI_overhead = 4 + 4 + 2 + 4 = 14
 */
#define DSI_HBACK_PORCH_PKT_OVERHEAD  14

/* Additional Hs TX timeout margin */
#define DSI_HTX_TO_MARGIN   720

#define DSI_CYCLE_COUNTER_VALUE     512

#define DSI_LRXH_TO_VALUE   0x2000

/* Turn around timeout terminal count */
#define DSI_TA_TO_VALUE     0x2000

/* Turn around timeout tally */
#define DSI_TA_TALLY_VALUE      0x0
/* LP Rx timeout tally */
#define DSI_LRXH_TALLY_VALUE    0x0
/* HS Tx Timeout tally */
#define DSI_HTX_TALLY_VALUE     0x0

/* DSI Power control settle time 10 micro seconds */
#define DSI_POWER_CONTROL_SETTLE_TIME_US    10

#define DSI_HOST_FIFO_DEPTH     64
#define DSI_VIDEO_FIFO_DEPTH    480
#define DSI_READ_FIFO_DEPTH	(32 << 2)

#define NUMOF_BIT_PER_BYTE			8
#define DEFAULT_LP_CMD_MODE_CLK_KHZ		10000
#define DEFAULT_MAX_DSI_PHY_CLK_KHZ		(500*1000)
#define DEFAULT_PANEL_RESET_TIMEOUT		2
#define DEFAULT_PANEL_BUFFER_BYTE		512

/*
 * TODO: are DSI_HOST_DSI_CONTROL_CRC_RESET(RESET_CRC) and
 * DSI_HOST_DSI_CONTROL_HOST_TX_TRIG_SRC(IMMEDIATE) required for everyone?
 */
#define HOST_DSI_CTRL_COMMON \
			(DSI_HOST_DSI_CONTROL_PHY_CLK_DIV(DSI_PHY_CLK_DIV1) | \
			DSI_HOST_DSI_CONTROL_ULTRA_LOW_POWER(NORMAL) | \
			DSI_HOST_DSI_CONTROL_PERIPH_RESET(TEGRA_DSI_DISABLE) | \
			DSI_HOST_DSI_CONTROL_RAW_DATA(TEGRA_DSI_DISABLE) | \
			DSI_HOST_DSI_CONTROL_IMM_BTA(TEGRA_DSI_DISABLE) | \
			DSI_HOST_DSI_CONTROL_PKT_BTA(TEGRA_DSI_DISABLE) | \
			DSI_HOST_DSI_CONTROL_CS_ENABLE(TEGRA_DSI_ENABLE) | \
			DSI_HOST_DSI_CONTROL_ECC_ENABLE(TEGRA_DSI_ENABLE) | \
			DSI_HOST_DSI_CONTROL_PKT_WR_FIFO_SEL(HOST_ONLY))

#define HOST_DSI_CTRL_HOST_DRIVEN \
			(DSI_HOST_DSI_CONTROL_CRC_RESET(RESET_CRC) | \
			DSI_HOST_DSI_CONTROL_HOST_TX_TRIG_SRC(IMMEDIATE))

#define HOST_DSI_CTRL_DC_DRIVEN 0

#define DSI_CTRL_HOST_DRIVEN	(DSI_CONTROL_VID_ENABLE(TEGRA_DSI_DISABLE) | \
				DSI_CONTROL_HOST_ENABLE(TEGRA_DSI_ENABLE))

#define DSI_CTRL_DC_DRIVEN	(DSI_CONTROL_VID_TX_TRIG_SRC(SOL) | \
				DSI_CONTROL_VID_ENABLE(TEGRA_DSI_ENABLE) | \
				DSI_CONTROL_HOST_ENABLE(TEGRA_DSI_DISABLE))

#define DSI_CTRL_CMD_MODE	(DSI_CONTROL_VID_DCS_ENABLE(TEGRA_DSI_ENABLE))

#define DSI_CTRL_VIDEO_MODE	(DSI_CONTROL_VID_DCS_ENABLE(TEGRA_DSI_DISABLE))

/* Mipi v1.00.00 phy timing range */
#define NOT_DEFINED			-1
#define MIPI_T_HSEXIT_PS_MIN		(100 * 1000)
#define MIPI_T_HSEXIT_PS_MAX		NOT_DEFINED
#define	MIPI_T_HSTRAIL_PS_MIN(clk_ps)	max((8 * (clk_ps)), \
					(60 * 1000 + 4 * (clk_ps)))
#define MIPI_T_HSTRAIL_PS_MAX		NOT_DEFINED
#define MIPI_T_HSZERO_PS_MIN		NOT_DEFINED
#define MIPI_T_HSZERO_PS_MAX		NOT_DEFINED
#define MIPI_T_HSPREPARE_PS_MIN(clk_ps)	(40 * 1000 + 4 * (clk_ps))
#define MIPI_T_HSPREPARE_PS_MAX(clk_ps)	(85 * 1000 + 6 * (clk_ps))
#define MIPI_T_CLKTRAIL_PS_MIN		(60 * 1000)
#define MIPI_T_CLKTRAIL_PS_MAX		NOT_DEFINED
#define	MIPI_T_CLKPOST_PS_MIN(clk_ps)	(60 * 1000 + 52 * (clk_ps))
#define MIPI_T_CLKPOST_PS_MAX		NOT_DEFINED
#define	MIPI_T_CLKZERO_PS_MIN		NOT_DEFINED
#define MIPI_T_CLKZERO_PS_MAX		NOT_DEFINED
#define MIPI_T_TLPX_PS_MIN		(50 * 1000)
#define MIPI_T_TLPX_PS_MAX		NOT_DEFINED
#define MIPI_T_CLKPREPARE_PS_MIN	(38 * 1000)
#define MIPI_T_CLKPREPARE_PS_MAX	(95 * 1000)
#define MIPI_T_CLKPRE_PS_MIN		(8 * 1000)
#define MIPI_T_CLKPRE_PS_MAX		NOT_DEFINED
#define	MIPI_T_WAKEUP_PS_MIN		(1 * 1000)
#define MIPI_T_WAKEUP_PS_MAX		NOT_DEFINED
#define MIPI_T_TASURE_PS_MIN(tlpx_ps)	(tlpx_ps)
#define MIPI_T_TASURE_PS_MAX(tlpx_ps)	(2 * (tlpx_ps))
#define MIPI_T_HSPREPARE_ADD_HSZERO_PS_MIN(clk_ps) \
					(145 * 1000 + 10 * (clk_ps))
#define MIPI_T_HSPREPARE_ADD_HSZERO_PS_MAX		NOT_DEFINED
#define	MIPI_T_CLKPREPARE_ADD_CLKZERO_PS_MIN		(300 * 1000)
#define MIPI_T_CLKPREPARE_ADD_CLKZERO_PS_MAX		NOT_DEFINED

#define DSI_TBYTE(clk_ps)	((clk_ps) * (BITS_PER_BYTE))
#define DSI_CONVERT_T_PHY_PS_TO_T_PHY(t_phy_ps, clk_ps, hw_inc) \
				((int)((DIV_ROUND_CLOSEST((t_phy_ps), \
				(DSI_TBYTE(clk_ps)))) - (hw_inc)))

#define DSI_CONVERT_T_PHY_TO_T_PHY_PS(t_phy, clk_ps, hw_inc) \
				(((t_phy) + (hw_inc)) * (DSI_TBYTE(clk_ps)))

/* Default phy timing in ns */
#define T_HSEXIT_PS_DEFAULT		(120 * 1000)
#define T_HSTRAIL_PS_DEFAULT(clk_ps) \
					max((8 * (clk_ps)), \
					(60 * 1000 + 4 * (clk_ps)))
#define T_DATZERO_PS_DEFAULT(clk_ps)	(145 * 1000 + 5 * (clk_ps))
#define T_HSPREPARE_PS_DEFAULT(clk_ps)	(65 * 1000 + 5 * (clk_ps))
#define T_CLKTRAIL_PS_DEFAULT		(80 * 1000)
#define T_CLKPOST_PS_DEFAULT(clk_ps)	(70 * 1000 + 52 * (clk_ps))
#define T_CLKZERO_PS_DEFAULT		(260 * 1000)
#define T_TLPX_PS_DEFAULT		(60 * 1000)
#define T_CLKPREPARE_PS_DEFAULT		(65 * 1000)
#define T_TAGO_PS_DEFAULT		(4 * (T_TLPX_PS_DEFAULT))
#define T_TASURE_PS_DEFAULT		(2 * (T_TLPX_PS_DEFAULT))
#define T_TAGET_PS_DEFAULT		(5 * (T_TLPX_PS_DEFAULT))

/* HW increment to phy register values */
#define T_HSEXIT_HW_INC		1
#define T_HSTRAIL_HW_INC	0
#define T_DATZERO_HW_INC	3
#define T_HSPREPARE_HW_INC	1
#define T_CLKTRAIL_HW_INC	1
#define T_CLKPOST_HW_INC	1
#define T_CLKZERO_HW_INC	1
#define T_TLPX_HW_INC		1
#define T_CLKPREPARE_HW_INC	1
#define T_TAGO_HW_INC		1
#define T_TASURE_HW_INC		1
#define T_TAGET_HW_INC		1
#define T_CLKPRE_HW_INC		1
#define T_WAKEUP_HW_INC		1

/* Default phy timing reg values */
#define T_HSEXIT_DEFAULT(clk_ps) \
(DSI_CONVERT_T_PHY_PS_TO_T_PHY( \
T_HSEXIT_PS_DEFAULT, clk_ps, T_HSEXIT_HW_INC))

#define T_HSTRAIL_DEFAULT(clk_ps) \
(3 + (DSI_CONVERT_T_PHY_PS_TO_T_PHY( \
T_HSTRAIL_PS_DEFAULT(clk_ps), clk_ps, T_HSTRAIL_HW_INC)))

#define T_DATZERO_DEFAULT(clk_ps) \
(DSI_CONVERT_T_PHY_PS_TO_T_PHY( \
T_DATZERO_PS_DEFAULT(clk_ps), clk_ps, T_DATZERO_HW_INC))

#define T_HSPREPARE_DEFAULT(clk_ps) \
(DSI_CONVERT_T_PHY_PS_TO_T_PHY( \
T_HSPREPARE_PS_DEFAULT(clk_ps), clk_ps, T_HSPREPARE_HW_INC))

#define T_CLKTRAIL_DEFAULT(clk_ps) \
(DSI_CONVERT_T_PHY_PS_TO_T_PHY( \
T_CLKTRAIL_PS_DEFAULT, clk_ps, T_CLKTRAIL_HW_INC))

#define T_CLKPOST_DEFAULT(clk_ps) \
(DSI_CONVERT_T_PHY_PS_TO_T_PHY( \
T_CLKPOST_PS_DEFAULT(clk_ps), clk_ps, T_CLKPOST_HW_INC))

#define T_CLKZERO_DEFAULT(clk_ps) \
(DSI_CONVERT_T_PHY_PS_TO_T_PHY( \
T_CLKZERO_PS_DEFAULT, clk_ps, T_CLKZERO_HW_INC))

#define T_TLPX_DEFAULT(clk_ps) \
(DSI_CONVERT_T_PHY_PS_TO_T_PHY( \
T_TLPX_PS_DEFAULT, clk_ps, T_TLPX_HW_INC))

#define T_CLKPREPARE_DEFAULT(clk_ps) \
(DSI_CONVERT_T_PHY_PS_TO_T_PHY( \
T_CLKPREPARE_PS_DEFAULT, clk_ps, T_CLKPREPARE_HW_INC))

#define T_CLKPRE_DEFAULT	0x1
#define T_WAKEUP_DEFAULT	0xff

#define T_TAGO_DEFAULT(clk_ps) \
(DSI_CONVERT_T_PHY_PS_TO_T_PHY( \
T_TAGO_PS_DEFAULT, clk_ps, T_TAGO_HW_INC))

#define T_TASURE_DEFAULT(clk_ps) \
(DSI_CONVERT_T_PHY_PS_TO_T_PHY( \
T_TASURE_PS_DEFAULT, clk_ps, T_TASURE_HW_INC))

#define T_TAGET_DEFAULT(clk_ps) \
(DSI_CONVERT_T_PHY_PS_TO_T_PHY( \
T_TAGET_PS_DEFAULT, clk_ps, T_TAGET_HW_INC))

struct tegra_dsi_out_ops {
	/* initialize output.  dsi clocks are not on at this point */
	int (*init)(struct tegra_dc_dsi_data *);
	/* destroy output.  dsi clocks are not on at this point */
	void (*destroy)(struct tegra_dc_dsi_data *);
	/* enable output.  dsi clocks are on at this point */
	void (*enable)(struct tegra_dc_dsi_data *);
	/* disable output.  dsi clocks are on at this point */
	void (*disable)(struct tegra_dc_dsi_data *dc);
	/* suspend output.  dsi clocks are on at this point */
	void (*suspend)(struct tegra_dc_dsi_data *);
	/* resume output.  dsi clocks are on at this point */
	void (*resume)(struct tegra_dc_dsi_data *);
};
extern struct tegra_dsi_out_ops tegra_dsi2lvds_ops;
#if defined(CONFIG_TEGRA_DSI2EDP_TC358767) || \
		defined(CONFIG_TEGRA_DSI2EDP_SN65DSI86)
extern struct tegra_dsi_out_ops tegra_dsi2edp_ops;
#else
#define tegra_dsi2edp_ops (*(struct tegra_dsi_out_ops *)NULL)
#endif

struct sanity_status {
	u32 sot_error:1;
	u32 sot_sync_error:1;
	u32 eot_sync_error:1;
	u32 escape_mode_entry_comand_error:1;
	u32 low_power_transmit_sync_error:1;
	u32 hs_receive_timeout_error:1;
	u32 false_control_error:1;
	u32 reserved1:1;
	u32 ecc_error_single_bit:1;
	u32 ecc_error_multi_bit:1;
	u32 checksum_error:1;
	u32 dsi_data_type_not_recognized:1;
	u32 dsi_vc_id_invalid:1;
	u32 dsi_protocol_violation:1;
	u32 reserved2:1;
	u32 reserved3:1;
};

#ifdef CONFIG_DEBUG_FS
void tegra_dc_dsi_debug_create(struct tegra_dc_dsi_data *dsi);
#endif
void tegra_dsi_clk_enable(struct tegra_dc_dsi_data *dsi);
void tegra_dsi_clk_disable(struct tegra_dc_dsi_data *dsi);
unsigned long tegra_dsi_controller_readl(struct tegra_dc_dsi_data *dsi,
							u32 reg, int index);
unsigned long tegra_dsi_readl(struct tegra_dc_dsi_data *dsi, u32 reg);
void tegra_dsi_controller_writel(struct tegra_dc_dsi_data *dsi,
						u32 val, u32 reg, int index);
void tegra_dsi_writel(struct tegra_dc_dsi_data *dsi, u32 val, u32 reg);
int tegra_dsi_read_data(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi,
				u16 max_ret_payload_size,
				u8 panel_reg_addr, u8 *read_data);
int tegra_dsi_panel_sanity_check(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi,
				struct sanity_status *san);
bool tegra_dsi_enable_read_debug(struct tegra_dc_dsi_data *dsi);
bool tegra_dsi_disable_read_debug(struct tegra_dc_dsi_data *dsi);
int tegra_dsi_start_host_cmd_v_blank_dcs(struct tegra_dc_dsi_data *dsi,
						struct tegra_dsi_cmd *cmd);
void tegra_dsi_stop_host_cmd_v_blank_dcs(struct tegra_dc_dsi_data *dsi);
int tegra_dsi_write_data(struct tegra_dc *dc,
			struct tegra_dc_dsi_data *dsi,
			struct tegra_dsi_cmd *cmd, u8 delay_ms);

static inline void *tegra_dsi_get_outdata(struct tegra_dc_dsi_data *dsi)
{
	return dsi->out_data;
}

static inline void tegra_dsi_set_outdata(struct tegra_dc_dsi_data *dsi,
						void *data)
{
	dsi->out_data = data;
}

#endif
