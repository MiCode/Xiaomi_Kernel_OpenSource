/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/platform_device.h>

#include <media/soc_camera.h>
#include <media/soc_mediabus.h>
#include <media/tegra_v4l2_camera.h>

#include <mach/clk.h>

#include "nvhost_syncpt.h"
#include "common.h"

#define TEGRA_SYNCPT_VI_WAIT_TIMEOUT                    200
#define TEGRA_SYNCPT_CSI_WAIT_TIMEOUT                   200

#define TEGRA_VIP_H_ACTIVE_START			0x98
#define TEGRA_VIP_V_ACTIVE_START			0x10

/* SYNCPTs 12-17 are reserved for VI. */
#define TEGRA_VI_SYNCPT_VI                              NVSYNCPT_VI_ISP_2
#define TEGRA_VI_SYNCPT_CSI_A                           NVSYNCPT_VI_ISP_3
#define TEGRA_VI_SYNCPT_CSI_B                           NVSYNCPT_VI_ISP_4

/* Tegra CSI-MIPI registers. */
#define TEGRA_VI_OUT_1_INCR_SYNCPT			0x000
#define TEGRA_VI_OUT_1_INCR_SYNCPT_CNTRL		0x004
#define TEGRA_VI_OUT_1_INCR_SYNCPT_ERROR		0x008
#define TEGRA_VI_OUT_2_INCR_SYNCPT			0x020
#define TEGRA_VI_OUT_2_INCR_SYNCPT_CNTRL		0x024
#define TEGRA_VI_OUT_2_INCR_SYNCPT_ERROR		0x028
#define TEGRA_VI_MISC_INCR_SYNCPT			0x040
#define TEGRA_VI_MISC_INCR_SYNCPT_CNTRL			0x044
#define TEGRA_VI_MISC_INCR_SYNCPT_ERROR			0x048
#define TEGRA_VI_CONT_SYNCPT_OUT_1			0x060
#define TEGRA_VI_CONT_SYNCPT_OUT_2			0x064
#define TEGRA_VI_CONT_SYNCPT_VIP_VSYNC			0x068
#define TEGRA_VI_CONT_SYNCPT_VI2EPP			0x06c
#define TEGRA_VI_CONT_SYNCPT_CSI_PPA_FRAME_START	0x070
#define TEGRA_VI_CONT_SYNCPT_CSI_PPA_FRAME_END		0x074
#define TEGRA_VI_CONT_SYNCPT_CSI_PPB_FRAME_START	0x078
#define TEGRA_VI_CONT_SYNCPT_CSI_PPB_FRAME_END		0x07c
#define TEGRA_VI_CTXSW					0x080
#define TEGRA_VI_INTSTATUS				0x084
#define TEGRA_VI_VI_INPUT_CONTROL			0x088
#define TEGRA_VI_VI_CORE_CONTROL			0x08c
#define TEGRA_VI_VI_FIRST_OUTPUT_CONTROL		0x090
#define TEGRA_VI_VI_SECOND_OUTPUT_CONTROL		0x094
#define TEGRA_VI_HOST_INPUT_FRAME_SIZE			0x098
#define TEGRA_VI_HOST_H_ACTIVE				0x09c
#define TEGRA_VI_HOST_V_ACTIVE				0x0a0
#define TEGRA_VI_VIP_H_ACTIVE				0x0a4
#define TEGRA_VI_VIP_V_ACTIVE				0x0a8
#define TEGRA_VI_VI_PEER_CONTROL			0x0ac
#define TEGRA_VI_VI_DMA_SELECT				0x0b0
#define TEGRA_VI_HOST_DMA_WRITE_BUFFER			0x0b4
#define TEGRA_VI_HOST_DMA_BASE_ADDRESS			0x0b8
#define TEGRA_VI_HOST_DMA_WRITE_BUFFER_STATUS		0x0bc
#define TEGRA_VI_HOST_DMA_WRITE_PEND_BUFCOUNT		0x0c0
#define TEGRA_VI_VB0_START_ADDRESS_FIRST		0x0c4
#define TEGRA_VI_VB0_BASE_ADDRESS_FIRST			0x0c8
#define TEGRA_VI_VB0_START_ADDRESS_U			0x0cc
#define TEGRA_VI_VB0_BASE_ADDRESS_U			0x0d0
#define TEGRA_VI_VB0_START_ADDRESS_V			0x0d4
#define TEGRA_VI_VB0_BASE_ADDRESS_V			0x0d8
#define TEGRA_VI_VB_SCRATCH_ADDRESS_UV			0x0dc
#define TEGRA_VI_FIRST_OUTPUT_FRAME_SIZE		0x0e0
#define TEGRA_VI_VB0_COUNT_FIRST			0x0e4
#define TEGRA_VI_VB0_SIZE_FIRST				0x0e8
#define TEGRA_VI_VB0_BUFFER_STRIDE_FIRST		0x0ec
#define TEGRA_VI_VB0_START_ADDRESS_SECOND		0x0f0
#define TEGRA_VI_VB0_BASE_ADDRESS_SECOND		0x0f4
#define TEGRA_VI_SECOND_OUTPUT_FRAME_SIZE		0x0f8
#define TEGRA_VI_VB0_COUNT_SECOND			0x0fc
#define TEGRA_VI_VB0_SIZE_SECOND			0x100
#define TEGRA_VI_VB0_BUFFER_STRIDE_SECOND		0x104
#define TEGRA_VI_H_LPF_CONTROL				0x108
#define TEGRA_VI_H_DOWNSCALE_CONTROL			0x10c
#define TEGRA_VI_V_DOWNSCALE_CONTROL			0x110
#define TEGRA_VI_CSC_Y					0x114
#define TEGRA_VI_CSC_UV_R				0x118
#define TEGRA_VI_CSC_UV_G				0x11c
#define TEGRA_VI_CSC_UV_B				0x120
#define TEGRA_VI_CSC_ALPHA				0x124
#define TEGRA_VI_HOST_VSYNC				0x128
#define TEGRA_VI_COMMAND				0x12c
#define TEGRA_VI_HOST_FIFO_STATUS			0x130
#define TEGRA_VI_INTERRUPT_MASK				0x134
#define TEGRA_VI_INTERRUPT_TYPE_SELECT			0x138
#define TEGRA_VI_INTERRUPT_POLARITY_SELECT		0x13c
#define TEGRA_VI_INTERRUPT_STATUS			0x140
#define TEGRA_VI_VIP_INPUT_STATUS			0x144
#define TEGRA_VI_VIDEO_BUFFER_STATUS			0x148
#define TEGRA_VI_SYNC_OUTPUT				0x14c
#define TEGRA_VI_VVS_OUTPUT_DELAY			0x150
#define TEGRA_VI_PWM_CONTROL				0x154
#define TEGRA_VI_PWM_SELECT_PULSE_A			0x158
#define TEGRA_VI_PWM_SELECT_PULSE_B			0x15c
#define TEGRA_VI_PWM_SELECT_PULSE_C			0x160
#define TEGRA_VI_PWM_SELECT_PULSE_D			0x164
#define TEGRA_VI_VI_DATA_INPUT_CONTROL			0x168
#define TEGRA_VI_PIN_INPUT_ENABLE			0x16c
#define TEGRA_VI_PIN_OUTPUT_ENABLE			0x170
#define TEGRA_VI_PIN_INVERSION				0x174
#define TEGRA_VI_PIN_INPUT_DATA				0x178
#define TEGRA_VI_PIN_OUTPUT_DATA			0x17c
#define TEGRA_VI_PIN_OUTPUT_SELECT			0x180
#define TEGRA_VI_RAISE_VIP_BUFFER_FIRST_OUTPUT		0x184
#define TEGRA_VI_RAISE_VIP_FRAME_FIRST_OUTPUT		0x188
#define TEGRA_VI_RAISE_VIP_BUFFER_SECOND_OUTPUT		0x18c
#define TEGRA_VI_RAISE_VIP_FRAME_SECOND_OUTPUT		0x190
#define TEGRA_VI_RAISE_HOST_FIRST_OUTPUT		0x194
#define TEGRA_VI_RAISE_HOST_SECOND_OUTPUT		0x198
#define TEGRA_VI_RAISE_EPP				0x19c
#define TEGRA_VI_CAMERA_CONTROL				0x1a0
#define TEGRA_VI_VI_ENABLE				0x1a4
#define TEGRA_VI_VI_ENABLE_2				0x1a8
#define TEGRA_VI_VI_RAISE				0x1ac
#define TEGRA_VI_Y_FIFO_WRITE				0x1b0
#define TEGRA_VI_U_FIFO_WRITE				0x1b4
#define TEGRA_VI_V_FIFO_WRITE				0x1b8
#define TEGRA_VI_VI_MCCIF_FIFOCTRL			0x1bc
#define TEGRA_VI_TIMEOUT_WCOAL_VI			0x1c0
#define TEGRA_VI_MCCIF_VIRUV_HP				0x1c4
#define TEGRA_VI_MCCIF_VIWSB_HP				0x1c8
#define TEGRA_VI_MCCIF_VIWU_HP				0x1cc
#define TEGRA_VI_MCCIF_VIWV_HP				0x1d0
#define TEGRA_VI_MCCIF_VIWY_HP				0x1d4
#define TEGRA_VI_CSI_PPA_RAISE_FRAME_START		0x1d8
#define TEGRA_VI_CSI_PPA_RAISE_FRAME_END		0x1dc
#define TEGRA_VI_CSI_PPB_RAISE_FRAME_START		0x1e0
#define TEGRA_VI_CSI_PBB_RAISE_FRAME_END		0x1e4
#define TEGRA_VI_CSI_PPA_H_ACTIVE			0x1e8
#define TEGRA_VI_CSI_PPA_V_ACTIVE			0x1ec
#define TEGRA_VI_CSI_PPB_H_ACTIVE			0x1f0
#define TEGRA_VI_CSI_PPB_V_ACTIVE			0x1f4
#define TEGRA_VI_ISP_H_ACTIVE				0x1f8
#define TEGRA_VI_ISP_V_ACTIVE				0x1fc
#define TEGRA_VI_STREAM_1_RESOURCE_DEFINE		0x200
#define TEGRA_VI_STREAM_2_RESOURCE_DEFINE		0x204
#define TEGRA_VI_RAISE_STREAM_1_DONE			0x208
#define TEGRA_VI_RAISE_STREAM_2_DONE			0x20c
#define TEGRA_VI_TS_MODE				0x210
#define TEGRA_VI_TS_CONTROL				0x214
#define TEGRA_VI_TS_PACKET_COUNT			0x218
#define TEGRA_VI_TS_ERROR_COUNT				0x21c
#define TEGRA_VI_TS_CPU_FLOW_CTL			0x220
#define TEGRA_VI_VB0_CHROMA_BUFFER_STRIDE_FIRST		0x224
#define TEGRA_VI_VB0_CHROMA_LINE_STRIDE_FIRST		0x228
#define TEGRA_VI_EPP_LINES_PER_BUFFER			0x22c
#define TEGRA_VI_BUFFER_RELEASE_OUTPUT1			0x230
#define TEGRA_VI_BUFFER_RELEASE_OUTPUT2			0x234
#define TEGRA_VI_DEBUG_FLOW_CONTROL_COUNTER_OUTPUT1	0x238
#define TEGRA_VI_DEBUG_FLOW_CONTROL_COUNTER_OUTPUT2	0x23c
#define TEGRA_VI_TERMINATE_BW_FIRST			0x240
#define TEGRA_VI_TERMINATE_BW_SECOND			0x244
#define TEGRA_VI_VB0_FIRST_BUFFER_ADDR_MODE		0x248
#define TEGRA_VI_VB0_SECOND_BUFFER_ADDR_MODE		0x24c
#define TEGRA_VI_RESERVE			0x250
#define TEGRA_VI_RESERVE_1				0x254
#define TEGRA_VI_RESERVE_2				0x258
#define TEGRA_VI_RESERVE_3				0x25c
#define TEGRA_VI_RESERVE_4				0x260
#define TEGRA_VI_MCCIF_VIRUV_HYST			0x264
#define TEGRA_VI_MCCIF_VIWSB_HYST			0x268
#define TEGRA_VI_MCCIF_VIWU_HYST			0x26c
#define TEGRA_VI_MCCIF_VIWV_HYST			0x270
#define TEGRA_VI_MCCIF_VIWY_HYST			0x274

#define TEGRA_CSI_VI_INPUT_STREAM_CONTROL		0x800
#define TEGRA_CSI_HOST_INPUT_STREAM_CONTROL		0x808
#define TEGRA_CSI_INPUT_STREAM_A_CONTROL		0x810
#define TEGRA_CSI_PIXEL_STREAM_A_CONTROL0		0x818
#define TEGRA_CSI_PIXEL_STREAM_A_CONTROL1		0x81c
#define TEGRA_CSI_PIXEL_STREAM_A_WORD_COUNT		0x820
#define TEGRA_CSI_PIXEL_STREAM_A_GAP			0x824
#define TEGRA_CSI_PIXEL_STREAM_PPA_COMMAND		0x828
#define TEGRA_CSI_INPUT_STREAM_B_CONTROL		0x83c
#define TEGRA_CSI_PIXEL_STREAM_B_CONTROL0		0x844
#define TEGRA_CSI_PIXEL_STREAM_B_CONTROL1		0x848
#define TEGRA_CSI_PIXEL_STREAM_B_WORD_COUNT		0x84c
#define TEGRA_CSI_PIXEL_STREAM_B_GAP			0x850
#define TEGRA_CSI_PIXEL_STREAM_PPB_COMMAND		0x854
#define TEGRA_CSI_PHY_CIL_COMMAND			0x868
#define TEGRA_CSI_PHY_CILA_CONTROL0			0x86c
#define TEGRA_CSI_PHY_CILB_CONTROL0			0x870
#define TEGRA_CSI_CSI_PIXEL_PARSER_STATUS		0x878
#define TEGRA_CSI_CSI_CIL_STATUS			0x87c
#define TEGRA_CSI_CSI_PIXEL_PARSER_INTERRUPT_MASK	0x880
#define TEGRA_CSI_CSI_CIL_INTERRUPT_MASK		0x884
#define TEGRA_CSI_CSI_READONLY_STATUS			0x888
#define TEGRA_CSI_ESCAPE_MODE_COMMAND			0x88c
#define TEGRA_CSI_ESCAPE_MODE_DATA			0x890
#define TEGRA_CSI_CILA_PAD_CONFIG0			0x894
#define TEGRA_CSI_CILA_PAD_CONFIG1			0x898
#define TEGRA_CSI_CILB_PAD_CONFIG0			0x89c
#define TEGRA_CSI_CILB_PAD_CONFIG1			0x8a0
#define TEGRA_CSI_CIL_PAD_CONFIG			0x8a4
#define TEGRA_CSI_CILA_MIPI_CAL_CONFIG			0x8a8
#define TEGRA_CSI_CILB_MIPI_CAL_CONFIG			0x8ac
#define TEGRA_CSI_CIL_MIPI_CAL_STATUS			0x8b0
#define TEGRA_CSI_CLKEN_OVERRIDE			0x8b4
#define TEGRA_CSI_DEBUG_CONTROL				0x8b8
#define TEGRA_CSI_DEBUG_COUNTER				0x8bc
#define TEGRA_CSI_DEBUG_COUNTER_1			0x8c0
#define TEGRA_CSI_DEBUG_COUNTER_2			0x8c4
#define TEGRA_CSI_PIXEL_STREAM_A_EXPECTED_FRAME		0x8c8
#define TEGRA_CSI_PIXEL_STREAM_B_EXPECTED_FRAME		0x8cc
#define TEGRA_CSI_DSI_MIPI_CAL_CONFIG			0x8d0

/* Test Pattern Generator of CSI */
#define TEGRA_CSI_PATTERN_GENERATOR_CTRL_A		0x940
#define TEGRA_CSI_PG_BLANK_A				0x944
#define TEGRA_CSI_PG_PHASE_A				0x948
#define TEGRA_CSI_PG_RED_FREQ_A				0x94c
#define TEGRA_CSI_PG_RED_FREQ_RATE_A			0x950
#define TEGRA_CSI_PG_GREEN_FREQ_A			0x954
#define TEGRA_CSI_PG_GREEN_FREQ_RATE_A			0x958
#define TEGRA_CSI_PG_BLUE_FREQ_A			0x95c
#define TEGRA_CSI_PG_BLUE_FREQ_RATE_A			0x960
#define TEGRA_CSI_PATTERN_GENERATOR_CTRL_B		0x974
#define TEGRA_CSI_PG_BLANK_B				0x978
#define TEGRA_CSI_PG_PHASE_B				0x97c
#define TEGRA_CSI_PG_RED_FREQ_B				0x980
#define TEGRA_CSI_PG_RED_FREQ_RATE_B			0x984
#define TEGRA_CSI_PG_GREEN_FREQ_B			0x988
#define TEGRA_CSI_PG_GREEN_FREQ_RATE_B			0x98c
#define TEGRA_CSI_PG_BLUE_FREQ_B			0x990
#define TEGRA_CSI_PG_BLUE_FREQ_RATE_B			0x994

static int vi_port_is_valid(int port)
{
	return (((port) >= TEGRA_CAMERA_PORT_CSI_A) &&
		((port) <= TEGRA_CAMERA_PORT_VIP));
}

static int vi_port_is_csi(int port)
{
	return (((port) == TEGRA_CAMERA_PORT_CSI_A) ||
		((port) == TEGRA_CAMERA_PORT_CSI_B));
}

/* Clock settings for camera */
static struct tegra_camera_clk vi_clks[] = {
	{
		.name = "vi",
		.freq = 150000000,
	},
	{
		.name = "vi_sensor",
		.freq = 24000000,
	},
	{
		.name = "csi",
		.freq = 0,
	},
	{
		.name = "isp",
		.freq = 0,
	},
	{
		.name = "csus",
		.freq = 0,
	},
	{
		.name = "sclk",
		.freq = 80000000,
	},
	{
		.name = "emc",
		.freq = 375000000,
	},
#ifdef TEGRA_11X_OR_HIGHER_CONFIG
	{
		.name = "cilab",
		.freq = 102000000,
	},
	{
		.name = "cilcd",
		.freq = 0,
	},
	{
		.name = "cile",
		.freq = 0,
	},
	/* Always put "p11_d2" at the end */
	{
		.name = "pll_d2",
		.freq = 0,
	},
#endif
};

static int vi_clks_init(struct tegra_camera_dev *cam)
{
	struct platform_device *pdev = cam->ndev;
	struct tegra_camera_clk *clks;
	int i;

	cam->num_clks = ARRAY_SIZE(vi_clks);
	cam->clks = vi_clks;

	for (i = 0; i < cam->num_clks; i++) {
		clks = &cam->clks[i];
		clks->clk = devm_clk_get(&pdev->dev, clks->name);
		if (IS_ERR_OR_NULL(clks->clk)) {
			clks->clk = NULL;
			dev_err(&pdev->dev, "Failed to get clock %s.\n",
				clks->name);
			return PTR_ERR(clks->clk);
		}

		if (clks->freq > 0)
			clk_set_rate(clks->clk, clks->freq);
	}

	return 0;
}

static void vi_clks_deinit(struct tegra_camera_dev *cam)
{
	/* We don't need cleanup for devm_clk_get() */
	return;
}

static void vi_clks_enable(struct tegra_camera_dev *cam)
{
	struct tegra_camera_clk *clks;
	int i;

	for (i = 0; i < cam->num_clks - 1; i++) {
		clks = &cam->clks[i];
		if (clks->clk)
			clk_prepare_enable(clks->clk);
	}

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
#define TEGRA_APB_MISC_BASE		0x70000000
	{
		u32 val;
		void __iomem *apb_misc = IO_ADDRESS(TEGRA_APB_MISC_BASE);

		val = readl(apb_misc + 0x42c);
		writel(val | 0x1, apb_misc + 0x42c);
	}
#endif

	if (cam->tpg_mode) {
		clks = &cam->clks[i];
		if (clks->clk) {
			clk_prepare_enable(clks->clk);
#ifdef TEGRA_11X_OR_HIGHER_CONFIG
			tegra_clk_cfg_ex(clks->clk,
					 TEGRA_CLK_PLLD_CSI_OUT_ENB, 1);
			tegra_clk_cfg_ex(clks->clk,
					 TEGRA_CLK_PLLD_DSI_OUT_ENB, 1);
#else
			/*
			 * bit 25: 0 = pd2vi_Clk,
			 *         1 = vi_sensor_clk
			 * bit 24: 0 = internal clock,
			 *	   1 = external clock (pd2vi_clk)
			 */
			tegra_clk_cfg_ex(clks->clk, TEGRA_CLK_VI_INP_SEL, 2);
#endif
		}
	}
}

static void vi_clks_disable(struct tegra_camera_dev *cam)
{
	struct tegra_camera_clk *clks;
	int i;

	for (i = 0; i < cam->num_clks - 1; i++) {
		clks = &cam->clks[i];
		if (clks->clk)
			clk_disable_unprepare(clks->clk);
	}

	if (cam->tpg_mode) {
		clks = &cam->clks[i];
		if (clks->clk) {
#ifdef TEGRA_11X_OR_HIGHER_CONFIG
			tegra_clk_cfg_ex(clks->clk,
					 TEGRA_CLK_PLLD_CSI_OUT_ENB, 0);
			tegra_clk_cfg_ex(clks->clk,
					 TEGRA_CLK_PLLD_DSI_OUT_ENB, 0);
#endif
			clk_disable_unprepare(clks->clk);
		}
	}
}

static void vi_save_syncpts(struct tegra_camera_dev *cam)
{
	cam->syncpt_csi_a =
		nvhost_syncpt_read_ext(cam->ndev,
				       TEGRA_VI_SYNCPT_CSI_A);

	cam->syncpt_csi_b =
		nvhost_syncpt_read_ext(cam->ndev,
				       TEGRA_VI_SYNCPT_CSI_B);

	cam->syncpt_vip =
		nvhost_syncpt_read_ext(cam->ndev,
				       TEGRA_VI_SYNCPT_VI);
}

static void vi_incr_syncpts(struct tegra_camera_dev *cam)
{
	nvhost_syncpt_cpu_incr_ext(cam->ndev,
				   TEGRA_VI_SYNCPT_CSI_A);

	nvhost_syncpt_cpu_incr_ext(cam->ndev,
				   TEGRA_VI_SYNCPT_CSI_B);

	nvhost_syncpt_cpu_incr_ext(cam->ndev,
				   TEGRA_VI_SYNCPT_VI);
}

static void vi_capture_clean(struct tegra_camera_dev *cam)
{
	TC_VI_REG_WT(cam, TEGRA_CSI_VI_INPUT_STREAM_CONTROL, 0x00000000);
	TC_VI_REG_WT(cam, TEGRA_CSI_HOST_INPUT_STREAM_CONTROL, 0x00000000);

	TC_VI_REG_WT(cam, TEGRA_CSI_CSI_PIXEL_PARSER_STATUS, 0x0);
	TC_VI_REG_WT(cam, TEGRA_CSI_CSI_CIL_STATUS, 0x0);
	TC_VI_REG_WT(cam, TEGRA_CSI_CSI_PIXEL_PARSER_INTERRUPT_MASK, 0x0);
	TC_VI_REG_WT(cam, TEGRA_CSI_CSI_CIL_INTERRUPT_MASK, 0x0);
	TC_VI_REG_WT(cam, TEGRA_CSI_CSI_READONLY_STATUS, 0x0);
	TC_VI_REG_WT(cam, TEGRA_CSI_ESCAPE_MODE_COMMAND, 0x0);
	TC_VI_REG_WT(cam, TEGRA_CSI_ESCAPE_MODE_DATA, 0x0);

	TC_VI_REG_WT(cam, TEGRA_CSI_CIL_PAD_CONFIG, 0x00000000);
	TC_VI_REG_WT(cam, TEGRA_CSI_CIL_MIPI_CAL_STATUS, 0x00000000);
	TC_VI_REG_WT(cam, TEGRA_CSI_CLKEN_OVERRIDE, 0x00000000);

	TC_VI_REG_WT(cam, TEGRA_CSI_DEBUG_CONTROL, 0x0);
	TC_VI_REG_WT(cam, TEGRA_CSI_DEBUG_COUNTER, 0x0);
	TC_VI_REG_WT(cam, TEGRA_CSI_DEBUG_COUNTER_1, 0x0);
	TC_VI_REG_WT(cam, TEGRA_CSI_DEBUG_COUNTER_2, 0x0);
}

static void vi_capture_setup_csi_a(struct tegra_camera_dev *cam,
				   struct soc_camera_device *icd,
				   u32 hdr)
{
	struct soc_camera_subdev_desc *ssdesc = &icd->sdesc->subdev_desc;
	struct tegra_camera_platform_data *pdata = ssdesc->drv_priv;
	int bytes_per_line = soc_mbus_bytes_per_line(icd->user_width,
			icd->current_fmt->host_fmt);

	TC_VI_REG_WT(cam, TEGRA_CSI_INPUT_STREAM_A_CONTROL, 0x00000000);
	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_A_CONTROL0, 0x00000000);
	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_A_CONTROL1, 0x00000000);
	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_A_WORD_COUNT, 0x00000000);
	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_A_GAP, 0x00000000);
	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_PPA_COMMAND, 0x00000000);
	TC_VI_REG_WT(cam, TEGRA_CSI_PHY_CILA_CONTROL0, 0x00000000);
	TC_VI_REG_WT(cam, TEGRA_CSI_CILA_PAD_CONFIG0, 0x00000000);
	TC_VI_REG_WT(cam, TEGRA_CSI_CILA_PAD_CONFIG1, 0x00000000);
	TC_VI_REG_WT(cam, TEGRA_CSI_CILA_MIPI_CAL_CONFIG, 0x00000000);
	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_A_EXPECTED_FRAME, 0x0);

	TC_VI_REG_WT(cam, TEGRA_VI_VI_CORE_CONTROL, 0x02000000);

	/* CSI-A H_ACTIVE and V_ACTIVE */
	TC_VI_REG_WT(cam, TEGRA_VI_CSI_PPA_H_ACTIVE,
		     (icd->user_width << 16));
	TC_VI_REG_WT(cam, TEGRA_VI_CSI_PPA_V_ACTIVE,
		     (icd->user_height << 16));

	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_A_CONTROL1,
		0x1); /* Frame # for top field detect for interlaced */

	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_A_WORD_COUNT,
		bytes_per_line);
	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_A_GAP, 0x00140000);

	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_A_EXPECTED_FRAME,
			((icd->user_height + cam->tpg_mode) << 16));

	/* pad 0s enabled, virtual channel ID 00 */
	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_A_CONTROL0,
		(0x1 << 16) | /* Output 1 pixel per clock */
		(hdr << 8) | /* If hdr shows wrong fmt, use right value */
		(0x1 << 7) | /* Check header CRC */
		(0x1 << 6) | /* Use word count field in the header */
		(0x1 << 5) | /* Look at data identifier byte in hdr */
		(0x1 << 4));  /* Expect packet header */

	TC_VI_REG_WT(cam, TEGRA_CSI_INPUT_STREAM_A_CONTROL,
		     (0x3f << 16) | /* Skip packet threshold */
		     (pdata->lanes - 1));

	/* Use 0x00000022 for continuous clock mode. */
	TC_VI_REG_WT(cam, TEGRA_CSI_PHY_CILA_CONTROL0,
		(pdata->continuous_clk << 5) |
		0x5); /* Clock settle time */

	TC_VI_REG_WT(cam, TEGRA_VI_CONT_SYNCPT_CSI_PPA_FRAME_END,
		(0x1 << 8) | /* Enable continuous syncpt */
		TEGRA_VI_SYNCPT_CSI_A);

	TC_VI_REG_WT(cam, TEGRA_CSI_PHY_CIL_COMMAND, 0x00020001);

	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_PPA_COMMAND, 0x0000f002);

	if (cam->tpg_mode) {
		TC_VI_REG_WT(cam, TEGRA_CSI_PATTERN_GENERATOR_CTRL_A,
				((cam->tpg_mode - 1) << 2) | 0x1);
		TC_VI_REG_WT(cam, TEGRA_CSI_PG_PHASE_A, 0x0);
		TC_VI_REG_WT(cam, TEGRA_CSI_PG_RED_FREQ_A, 0x00800080);
		TC_VI_REG_WT(cam, TEGRA_CSI_PG_RED_FREQ_RATE_A, 0x0);
		TC_VI_REG_WT(cam, TEGRA_CSI_PG_GREEN_FREQ_A, 0x00800080);
		TC_VI_REG_WT(cam, TEGRA_CSI_PG_GREEN_FREQ_RATE_A, 0x0);
		TC_VI_REG_WT(cam, TEGRA_CSI_PG_BLUE_FREQ_A, 0x00800080);
		TC_VI_REG_WT(cam, TEGRA_CSI_PG_BLUE_FREQ_RATE_A, 0x0);
		TC_VI_REG_WT(cam, TEGRA_CSI_PG_BLANK_A, 0x0000FFFF);
	}

}

static void vi_capture_setup_csi_b(struct tegra_camera_dev *cam,
				   struct soc_camera_device *icd,
				   u32 hdr)
{
	struct soc_camera_subdev_desc *ssdesc = &icd->sdesc->subdev_desc;
	struct tegra_camera_platform_data *pdata = ssdesc->drv_priv;
	int bytes_per_line = soc_mbus_bytes_per_line(icd->user_width,
						icd->current_fmt->host_fmt);

	TC_VI_REG_WT(cam, TEGRA_CSI_INPUT_STREAM_B_CONTROL, 0x00000000);
	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_B_CONTROL0, 0x00000000);
	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_B_CONTROL1, 0x00000000);
	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_B_WORD_COUNT, 0x00000000);
	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_B_GAP, 0x00000000);
	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_PPB_COMMAND, 0x00000000);
	TC_VI_REG_WT(cam, TEGRA_CSI_PHY_CILB_CONTROL0, 0x00000000);
	TC_VI_REG_WT(cam, TEGRA_CSI_CILB_PAD_CONFIG0, 0x00000000);
	TC_VI_REG_WT(cam, TEGRA_CSI_CILB_PAD_CONFIG1, 0x00000000);
	TC_VI_REG_WT(cam, TEGRA_CSI_CILB_MIPI_CAL_CONFIG, 0x00000000);
	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_B_EXPECTED_FRAME, 0x0);

	TC_VI_REG_WT(cam, TEGRA_VI_VI_CORE_CONTROL, 0x04000000);

	/* CSI-B H_ACTIVE and V_ACTIVE */
	TC_VI_REG_WT(cam, TEGRA_VI_CSI_PPB_H_ACTIVE,
		(icd->user_width << 16));
	TC_VI_REG_WT(cam, TEGRA_VI_CSI_PPB_V_ACTIVE,
		(icd->user_height << 16));

	/* pad 0s enabled, virtual channel ID 00 */
	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_B_CONTROL0,
		(0x1 << 16) | /* Output 1 pixel per clock */
		(hdr << 8) | /* If hdr shows wrong fmt, use right value */
		(0x1 << 7) | /* Check header CRC */
		(0x1 << 6) | /* Use word count field in the header */
		(0x1 << 5) | /* Look at data identifier byte in hdr */
		(0x1 << 4) | /* Expect packet header */
		0x1); /* Set PPB stream source to CSI B */

	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_B_CONTROL1,
		0x1); /* Frame # for top field detect for interlaced */

	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_B_WORD_COUNT,
		bytes_per_line);
	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_B_GAP, 0x00140000);

	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_B_EXPECTED_FRAME,
		(icd->user_height << 16) |
		(0x100 << 4) | /* Wait 0x100 vi clks for timeout */
		0x1); /* Enable line timeout */

	TC_VI_REG_WT(cam, TEGRA_CSI_INPUT_STREAM_B_CONTROL,
		     (0x3f << 16) | /* Skip packet threshold */
		     (pdata->lanes - 1));

	/* Use 0x00000022 for continuous clock mode. */
	TC_VI_REG_WT(cam, TEGRA_CSI_PHY_CILB_CONTROL0,
		(pdata->continuous_clk << 5) |
		0x5); /* Clock settle time */

	TC_VI_REG_WT(cam, TEGRA_VI_CONT_SYNCPT_CSI_PPB_FRAME_END,
		(0x1 << 8) | /* Enable continuous syncpt */
		TEGRA_VI_SYNCPT_CSI_B);

	TC_VI_REG_WT(cam, TEGRA_CSI_PHY_CIL_COMMAND, 0x00010002);

	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_PPB_COMMAND, 0x0000f002);
}

static void vi_capture_setup_vip(struct tegra_camera_dev *cam,
					   struct soc_camera_device *icd,
					   u32 input_control)
{
	TC_VI_REG_WT(cam, TEGRA_VI_VI_CORE_CONTROL, 0x00000000);

	TC_VI_REG_WT(cam, TEGRA_VI_VI_INPUT_CONTROL,
		(1 << 27) | /* field detect */
		(1 << 25) | /* hsync/vsync decoded from data (BT.656) */
		(1 << 1) | /* VIP_INPUT_ENABLE */
		input_control);

	TC_VI_REG_WT(cam, TEGRA_VI_H_DOWNSCALE_CONTROL, 0x00000000);
	TC_VI_REG_WT(cam, TEGRA_VI_V_DOWNSCALE_CONTROL, 0x00000000);

	/* VIP H_ACTIVE and V_ACTIVE */
	TC_VI_REG_WT(cam, TEGRA_VI_VIP_H_ACTIVE,
		(icd->user_width << 16) |
		TEGRA_VIP_H_ACTIVE_START);
	TC_VI_REG_WT(cam, TEGRA_VI_VIP_V_ACTIVE,
		(icd->user_height << 16) |
		TEGRA_VIP_V_ACTIVE_START);

	/*
	 * For VIP, D9..D2 is mapped to the video decoder's P7..P0.
	 * Disable/mask out the other Dn wires.
	 */
	TC_VI_REG_WT(cam, TEGRA_VI_PIN_INPUT_ENABLE, 0x000003fc);
	TC_VI_REG_WT(cam, TEGRA_VI_VI_DATA_INPUT_CONTROL, 0x000003fc);
	TC_VI_REG_WT(cam, TEGRA_VI_PIN_INVERSION, 0x00000000);

	TC_VI_REG_WT(cam, TEGRA_VI_CONT_SYNCPT_VIP_VSYNC,
		(0x1 << 8) | /* Enable continuous syncpt */
		TEGRA_VI_SYNCPT_VI);

	TC_VI_REG_WT(cam, TEGRA_VI_CAMERA_CONTROL, 0x00000004);
}

static int vi_capture_output_channel_setup(
		struct tegra_camera_dev *cam,
		struct soc_camera_device *icd)
{
	struct soc_camera_subdev_desc *ssdesc = &icd->sdesc->subdev_desc;
	struct tegra_camera_platform_data *pdata = ssdesc->drv_priv;
	int port = pdata->port;
	int bytes_per_line = soc_mbus_bytes_per_line(icd->user_width,
						icd->current_fmt->host_fmt);
	const struct soc_camera_format_xlate *current_fmt = icd->current_fmt;
	u32 output_fourcc = current_fmt->host_fmt->fourcc;
	u32 output_format, output_control;
	struct tegra_camera_buffer *buf = to_tegra_vb(cam->active);

	switch (output_fourcc) {
	case V4L2_PIX_FMT_UYVY:
		output_format = 0x3; /* Default to YUV422 */
		break;
	case V4L2_PIX_FMT_VYUY:
		output_format = (0x1 << 17) | 0x3;
		break;
	case V4L2_PIX_FMT_YUYV:
		output_format = (0x2 << 17) | 0x3;
		break;
	case V4L2_PIX_FMT_YVYU:
		output_format = (0x3 << 17) | 0x3;
		break;
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		output_format = 0x6; /* YUV420 planar */
		break;
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SBGGR10:
		/* Use second output channel for RAW8/RAW10 */
		buf->output_channel = 1;

		if (port == TEGRA_CAMERA_PORT_CSI_A)
			output_format = 0x7;
		else if (port == TEGRA_CAMERA_PORT_CSI_B)
			output_format = 0x8;
		else
			output_format = 0x9;
		break;
	default:
		dev_err(&cam->ndev->dev, "Wrong output format %d\n",
			output_fourcc);
		return -EINVAL;
	}

	output_control = (pdata->flip_v ? (0x1 << 20) : 0) |
			(pdata->flip_h ? (0x1 << 19) : 0) |
			output_format;

	if (buf->output_channel == 0) {
		TC_VI_REG_WT(cam, TEGRA_VI_VI_FIRST_OUTPUT_CONTROL,
				output_control);
		/*
		 * Set up frame size.  Bits 31:16 are the number of lines, and
		 * bits 15:0 are the number of pixels per line.
		 */
		TC_VI_REG_WT(cam, TEGRA_VI_FIRST_OUTPUT_FRAME_SIZE,
				(icd->user_height << 16) | icd->user_width);

		/* First output memory enabled */
		TC_VI_REG_WT(cam, TEGRA_VI_VI_ENABLE, 0x00000000);

		/* Set the number of frames in the buffer. */
		TC_VI_REG_WT(cam, TEGRA_VI_VB0_COUNT_FIRST, 0x00000001);

		/* Set up buffer frame size. */
		TC_VI_REG_WT(cam, TEGRA_VI_VB0_SIZE_FIRST,
				(icd->user_height << 16) | icd->user_width);

		TC_VI_REG_WT(cam, TEGRA_VI_VB0_BUFFER_STRIDE_FIRST,
				(icd->user_height * bytes_per_line));

		TC_VI_REG_WT(cam, TEGRA_VI_CONT_SYNCPT_OUT_1,
				(0x1 << 8) | /* Enable continuous syncpt */
				TEGRA_VI_SYNCPT_VI);

		TC_VI_REG_WT(cam, TEGRA_VI_VI_ENABLE, 0x00000000);
	} else if (buf->output_channel == 1) {
		TC_VI_REG_WT(cam, TEGRA_VI_VI_SECOND_OUTPUT_CONTROL,
				output_control);

		TC_VI_REG_WT(cam, TEGRA_VI_SECOND_OUTPUT_FRAME_SIZE,
				(icd->user_height << 16) | icd->user_width);

		TC_VI_REG_WT(cam, TEGRA_VI_VI_ENABLE_2, 0x00000000);

		/* Set the number of frames in the buffer. */
		TC_VI_REG_WT(cam, TEGRA_VI_VB0_COUNT_SECOND, 0x00000001);

		/* Set up buffer frame size. */
		TC_VI_REG_WT(cam, TEGRA_VI_VB0_SIZE_SECOND,
				(icd->user_height << 16) | icd->user_width);

		TC_VI_REG_WT(cam, TEGRA_VI_VB0_BUFFER_STRIDE_SECOND,
				(icd->user_height * bytes_per_line));

		TC_VI_REG_WT(cam, TEGRA_VI_CONT_SYNCPT_OUT_2,
				(0x1 << 8) | /* Enable continuous syncpt */
				TEGRA_VI_SYNCPT_VI);

		TC_VI_REG_WT(cam, TEGRA_VI_VI_ENABLE_2, 0x00000000);
	} else {
		dev_err(&cam->ndev->dev, "Wrong output channel %d\n",
			buf->output_channel);
		return -EINVAL;
	}

	return 0;
}


static int vi_capture_setup(struct tegra_camera_dev *cam)
{
	struct vb2_buffer *vb = cam->active;
	struct tegra_camera_buffer *buf = to_tegra_vb(vb);
	struct soc_camera_device *icd = buf->icd;
	struct soc_camera_subdev_desc *ssdesc = &icd->sdesc->subdev_desc;
	struct tegra_camera_platform_data *pdata = ssdesc->drv_priv;
	int port = pdata->port;
	const struct soc_camera_format_xlate *current_fmt = icd->current_fmt;
	enum v4l2_mbus_pixelcode input_code = current_fmt->code;
	u32 hdr, input_control = 0x0;

	switch (input_code) {
	case V4L2_MBUS_FMT_UYVY8_2X8:
		input_control |= 0x2 << 8;
		hdr = 30;
		break;
	case V4L2_MBUS_FMT_VYUY8_2X8:
		input_control |= 0x3 << 8;
		hdr = 30;
		break;
	case V4L2_MBUS_FMT_YUYV8_2X8:
		input_control |= 0x0;
		hdr = 30;
		break;
	case V4L2_MBUS_FMT_YVYU8_2X8:
		input_control |= 0x1 << 8;
		hdr = 30;
		break;
	case V4L2_MBUS_FMT_SBGGR8_1X8:
	case V4L2_MBUS_FMT_SGBRG8_1X8:
		input_control |= 0x2 << 2;	/* Input Format = Bayer */
		hdr = 42;
		break;
	case V4L2_MBUS_FMT_SBGGR10_1X10:
		input_control |= 0x2 << 2;	/* Input Format = Bayer */
		hdr = 43;
		break;
	default:
		dev_err(&cam->ndev->dev, "Input format %d is not supported\n",
			input_code);
		return -EINVAL;
	}

	/*
	 * Set up low pass filter.  Use 0x240 for chromaticity and 0x240
	 * for luminance, which is the default and means not to touch
	 * anything.
	 */
	TC_VI_REG_WT(cam, TEGRA_VI_H_LPF_CONTROL, 0x02400240);

	/* Set up raise-on-edge, so we get an interrupt on end of frame. */
	TC_VI_REG_WT(cam, TEGRA_VI_VI_RAISE, 0x00000001);

	/* Setup registers for CSI-A, CSI-B and VIP inputs */
	if (port == TEGRA_CAMERA_PORT_CSI_A)
		vi_capture_setup_csi_a(cam, icd, hdr);
	else if (port == TEGRA_CAMERA_PORT_CSI_B)
		vi_capture_setup_csi_b(cam, icd, hdr);
	else
		vi_capture_setup_vip(cam, icd, input_control);

	/* Setup registers for output channels */
	return vi_capture_output_channel_setup(cam, icd);
}

static int vi_capture_buffer_setup(struct tegra_camera_dev *cam,
			struct tegra_camera_buffer *buf)
{
	struct soc_camera_device *icd = buf->icd;

	switch (icd->current_fmt->host_fmt->fourcc) {
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		TC_VI_REG_WT(cam, TEGRA_VI_VB0_BASE_ADDRESS_U,
			     buf->buffer_addr_u);
		TC_VI_REG_WT(cam, TEGRA_VI_VB0_START_ADDRESS_U,
			     buf->start_addr_u);

		TC_VI_REG_WT(cam, TEGRA_VI_VB0_BASE_ADDRESS_V,
			     buf->buffer_addr_v);
		TC_VI_REG_WT(cam, TEGRA_VI_VB0_START_ADDRESS_V,
			     buf->start_addr_v);

	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_RGB32:
		/* output 1 */
		if (buf->output_channel == 0) {
			TC_VI_REG_WT(cam, TEGRA_VI_VB0_BASE_ADDRESS_FIRST,
					buf->buffer_addr);
			TC_VI_REG_WT(cam, TEGRA_VI_VB0_START_ADDRESS_FIRST,
					buf->start_addr);
		/* output 2 */
		} else if (buf->output_channel == 1) {
			TC_VI_REG_WT(cam, TEGRA_VI_VB0_BASE_ADDRESS_SECOND,
					buf->buffer_addr);
			TC_VI_REG_WT(cam, TEGRA_VI_VB0_START_ADDRESS_SECOND,
					buf->start_addr);
		} else {
			dev_err(&cam->ndev->dev, "Wrong output channel %d\n",
				buf->output_channel);
			return -EINVAL;
		}
	break;

	default:
		dev_err(&cam->ndev->dev, "Wrong host format %d\n",
			icd->current_fmt->host_fmt->fourcc);
		return -EINVAL;
	}

	return 0;
}

static int vi_capture_start(struct tegra_camera_dev *cam,
				      struct tegra_camera_buffer *buf)
{
	struct soc_camera_device *icd = buf->icd;
	struct soc_camera_subdev_desc *ssdesc = &icd->sdesc->subdev_desc;
	struct tegra_camera_platform_data *pdata = ssdesc->drv_priv;
	int port = pdata->port;
	int err;

	err = vi_capture_buffer_setup(cam, buf);
	if (err < 0)
		return err;
	/*
	 * Only wait on CSI frame end syncpt if we're using CSI.  Otherwise,
	 * wait on VIP VSYNC syncpt.
	 */
	if (port == TEGRA_CAMERA_PORT_CSI_A) {
		cam->syncpt_csi_a++;
		TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_PPA_COMMAND,
				0x0000f005);
		err = nvhost_syncpt_wait_timeout_ext(cam->ndev,
				TEGRA_VI_SYNCPT_CSI_A,
				cam->syncpt_csi_a,
				TEGRA_SYNCPT_CSI_WAIT_TIMEOUT,
				NULL,
				NULL);
	} else if (port == TEGRA_CAMERA_PORT_CSI_B) {
		cam->syncpt_csi_b++;
		TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_PPB_COMMAND,
				0x0000f005);
		err = nvhost_syncpt_wait_timeout_ext(cam->ndev,
				TEGRA_VI_SYNCPT_CSI_B,
				cam->syncpt_csi_b,
				TEGRA_SYNCPT_CSI_WAIT_TIMEOUT,
				NULL,
				NULL);
	} else {
		cam->syncpt_vip++;
		TC_VI_REG_WT(cam, TEGRA_VI_CAMERA_CONTROL,
				0x00000001);
		err = nvhost_syncpt_wait_timeout_ext(cam->ndev,
				TEGRA_VI_SYNCPT_VI,
				cam->syncpt_csi_a,
				TEGRA_SYNCPT_VI_WAIT_TIMEOUT,
				NULL,
				NULL);
	}

	if (!err)
		return 0;

	if (vi_port_is_csi(port)) {
		u32 ppstatus;
		u32 cilstatus;
		u32 rostatus;

		dev_warn(&icd->vdev->dev, "Timeout on CSI syncpt\n");
		dev_warn(&icd->vdev->dev, "buffer_addr = 0x%08x\n",
			buf->buffer_addr);

		ppstatus = TC_VI_REG_RD(cam,
			TEGRA_CSI_CSI_PIXEL_PARSER_STATUS);
		cilstatus = TC_VI_REG_RD(cam,
			 TEGRA_CSI_CSI_CIL_STATUS);
		rostatus = TC_VI_REG_RD(cam,
			TEGRA_CSI_CSI_READONLY_STATUS);

		dev_warn(&icd->vdev->dev,
			"PPSTATUS = 0x%08x, "
			"CILSTATUS = 0x%08x, "
			"ROSTATUS = 0x%08x\n",
			ppstatus, cilstatus, rostatus);
	} else {
		u32 vip_input_status;

		dev_warn(&cam->ndev->dev, "Timeout on VI syncpt\n");
		dev_warn(&cam->ndev->dev, "buffer_addr = 0x%08x\n",
			buf->buffer_addr);

		vip_input_status = TC_VI_REG_RD(cam,
			TEGRA_VI_VIP_INPUT_STATUS);

		dev_warn(&cam->ndev->dev,
			"VIP_INPUT_STATUS = 0x%08x\n",
			vip_input_status);
	}

	return err;
}

static int vi_capture_stop(struct tegra_camera_dev *cam, int port)
{
	struct tegra_camera_buffer *buf = to_tegra_vb(cam->active);
	int err;

	if (vi_port_is_csi(port))
		err = nvhost_syncpt_wait_timeout_ext(cam->ndev,
			TEGRA_VI_SYNCPT_VI,
			cam->syncpt_vip,
			TEGRA_SYNCPT_VI_WAIT_TIMEOUT,
			NULL,
			NULL);
	else
		err = 0;

	if (err) {
		u32 buffer_addr;
		u32 ppstatus;
		u32 cilstatus;

		dev_warn(&cam->ndev->dev, "Timeout on VI syncpt\n");

		if (buf->output_channel == 0)
			buffer_addr = TC_VI_REG_RD(cam,
					   TEGRA_VI_VB0_BASE_ADDRESS_FIRST);
		else if (buf->output_channel == 1)
			buffer_addr = TC_VI_REG_RD(cam,
					   TEGRA_VI_VB0_BASE_ADDRESS_SECOND);
		else {
			dev_err(&cam->ndev->dev, "Wrong output channel %d\n",
				buf->output_channel);
			return -EINVAL;
		}

		dev_warn(&cam->ndev->dev, "buffer_addr = 0x%08x\n",
			buffer_addr);

		ppstatus = TC_VI_REG_RD(cam,
					TEGRA_CSI_CSI_PIXEL_PARSER_STATUS);
		cilstatus = TC_VI_REG_RD(cam,
					 TEGRA_CSI_CSI_CIL_STATUS);
		dev_warn(&cam->ndev->dev,
			"PPSTATUS = 0x%08x, CILSTATUS = 0x%08x\n",
			ppstatus, cilstatus);
	}

	return err;
}

static void vi_unpowergate(struct tegra_camera_dev *cam)
{
	/*
	 * Powergating DIS must powergate VE partition. Camera
	 * module needs to increase the ref-count of disa to
	 * avoid itself powergated by DIS inadvertently.
	 */
#if defined(CONFIG_ARCH_TEGRA_11x_SOC) || defined(CONFIG_ARCH_TEGRA_14x_SOC)
	tegra_unpowergate_partition(TEGRA_POWERGATE_DISA);
#endif
}

static void vi_powergate(struct tegra_camera_dev *cam)
{
#if defined(CONFIG_ARCH_TEGRA_11x_SOC) || defined(CONFIG_ARCH_TEGRA_14x_SOC)
	tegra_powergate_partition(TEGRA_POWERGATE_DISA);
#endif
}

struct tegra_camera_ops vi_ops = {
	.clks_init = vi_clks_init,
	.clks_deinit = vi_clks_deinit,
	.clks_enable = vi_clks_enable,
	.clks_disable = vi_clks_disable,

	.capture_clean = vi_capture_clean,
	.capture_setup = vi_capture_setup,
	.capture_start = vi_capture_start,
	.capture_stop = vi_capture_stop,

	.activate = vi_unpowergate,
	.deactivate = vi_powergate,

	.save_syncpts = vi_save_syncpts,
	.incr_syncpts = vi_incr_syncpts,

	.port_is_valid = vi_port_is_valid,
};

int vi_register(struct tegra_camera_dev *cam)
{
	/* Init regulator */
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	cam->regulator_name = "vcsi";
#else
	cam->regulator_name = "avdd_dsi_csi";
#endif

	/* Init VI/CSI ops */
	cam->ops = &vi_ops;

	return 0;
}
