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

#define TEGRA_SYNCPT_CSI_WAIT_TIMEOUT                   200

#define TEGRA_VI_SYNCPT_CSI_A				NVSYNCPT_VI_0_3
#define TEGRA_VI_SYNCPT_CSI_B				NVSYNCPT_VI_1_3

#define TEGRA_VI_CFG_VI_INCR_SYNCPT			0x000
#define TEGRA_VI_CFG_VI_INCR_SYNCPT_CNTRL		0x004
#define TEGRA_VI_CFG_VI_INCR_SYNCPT_ERROR		0x008
#define TEGRA_VI_CFG_CTXSW				0x020
#define TEGRA_VI_CFG_INTSTATUS				0x024
#define TEGRA_VI_CFG_PWM_CONTROL			0x038
#define TEGRA_VI_CFG_PWM_HIGH_PULSE			0x03c
#define TEGRA_VI_CFG_PWM_LOW_PULSE			0x040
#define TEGRA_VI_CFG_PWM_SELECT_PULSE_A			0x044
#define TEGRA_VI_CFG_PWM_SELECT_PULSE_B			0x048
#define TEGRA_VI_CFG_PWM_SELECT_PULSE_C			0x04c
#define TEGRA_VI_CFG_PWM_SELECT_PULSE_D			0x050
#define TEGRA_VI_CFG_VGP1				0x064
#define TEGRA_VI_CFG_VGP2				0x068
#define TEGRA_VI_CFG_VGP3				0x06c
#define TEGRA_VI_CFG_VGP4				0x070
#define TEGRA_VI_CFG_VGP5				0x074
#define TEGRA_VI_CFG_VGP6				0x078
#define TEGRA_VI_CFG_INTERRUPT_MASK			0x08c
#define TEGRA_VI_CFG_INTERRUPT_TYPE_SELECT		0x090
#define TEGRA_VI_CFG_INTERRUPT_POLARITY_SELECT		0x094
#define TEGRA_VI_CFG_INTERRUPT_STATUS			0x098
#define TEGRA_VI_CFG_VGP_SYNCPT_CONFIG			0x0ac
#define TEGRA_VI_CFG_VI_SW_RESET			0x0b4
#define TEGRA_VI_CFG_CG_CTRL				0x0b8
#define TEGRA_VI_CFG_VI_MCCIF_FIFOCTRL			0x0e4
#define TEGRA_VI_CFG_TIMEOUT_WCOAL_VI			0x0e8
#define TEGRA_VI_CFG_DVFS				0x0f0
#define TEGRA_VI_CFG_RESERVE				0x0f4
#define TEGRA_VI_CFG_RESERVE_1				0x0f8

#define TEGRA_VI_CSI_0_SW_RESET				0x100
#define TEGRA_VI_CSI_0_SINGLE_SHOT			0x104
#define TEGRA_VI_CSI_0_SINGLE_SHOT_STATE_UPDATE		0x108
#define TEGRA_VI_CSI_0_IMAGE_DEF			0x10c
#define TEGRA_VI_CSI_0_RGB2Y_CTRL			0x110
#define TEGRA_VI_CSI_0_MEM_TILING			0x114
#define TEGRA_VI_CSI_0_CSI_IMAGE_SIZE			0x118
#define TEGRA_VI_CSI_0_CSI_IMAGE_SIZE_WC		0x11c
#define TEGRA_VI_CSI_0_CSI_IMAGE_DT			0x120
#define TEGRA_VI_CSI_0_SURFACE0_OFFSET_MSB		0x124
#define TEGRA_VI_CSI_0_SURFACE0_OFFSET_LSB		0x128
#define TEGRA_VI_CSI_0_SURFACE1_OFFSET_MSB		0x12c
#define TEGRA_VI_CSI_0_SURFACE1_OFFSET_LSB		0x130
#define TEGRA_VI_CSI_0_SURFACE2_OFFSET_MSB		0x134
#define TEGRA_VI_CSI_0_SURFACE2_OFFSET_LSB		0x138
#define TEGRA_VI_CSI_0_SURFACE0_BF_OFFSET_MSB		0x13c
#define TEGRA_VI_CSI_0_SURFACE0_BF_OFFSET_LSB		0x140
#define TEGRA_VI_CSI_0_SURFACE1_BF_OFFSET_MSB		0x144
#define TEGRA_VI_CSI_0_SURFACE1_BF_OFFSET_LSB		0x148
#define TEGRA_VI_CSI_0_SURFACE2_BF_OFFSET_MSB		0x14c
#define TEGRA_VI_CSI_0_SURFACE2_BF_OFFSET_LSB		0x150
#define TEGRA_VI_CSI_0_SURFACE0_STRIDE			0x154
#define TEGRA_VI_CSI_0_SURFACE1_STRIDE			0x158
#define TEGRA_VI_CSI_0_SURFACE2_STRIDE			0x15c
#define TEGRA_VI_CSI_0_SURFACE_HEIGHT0			0x160
#define TEGRA_VI_CSI_0_ISPINTF_CONFIG			0x164
#define TEGRA_VI_CSI_0_ERROR_STATUS			0x184
#define TEGRA_VI_CSI_0_ERROR_INT_MASK			0x188
#define TEGRA_VI_CSI_0_WD_CTRL				0x18c
#define TEGRA_VI_CSI_0_WD_PERIOD			0x190

#define TEGRA_VI_CSI_1_SW_RESET				0x200
#define TEGRA_VI_CSI_1_SINGLE_SHOT			0x204
#define TEGRA_VI_CSI_1_SINGLE_SHOT_STATE_UPDATE		0x208
#define TEGRA_VI_CSI_1_IMAGE_DEF			0x20c
#define TEGRA_VI_CSI_1_RGB2Y_CTRL			0x210
#define TEGRA_VI_CSI_1_MEM_TILING			0x214
#define TEGRA_VI_CSI_1_CSI_IMAGE_SIZE			0x218
#define TEGRA_VI_CSI_1_CSI_IMAGE_SIZE_WC		0x21c
#define TEGRA_VI_CSI_1_CSI_IMAGE_DT			0x220
#define TEGRA_VI_CSI_1_SURFACE0_OFFSET_MSB		0x224
#define TEGRA_VI_CSI_1_SURFACE0_OFFSET_LSB		0x228
#define TEGRA_VI_CSI_1_SURFACE1_OFFSET_MSB		0x22c
#define TEGRA_VI_CSI_1_SURFACE1_OFFSET_LSB		0x230
#define TEGRA_VI_CSI_1_SURFACE2_OFFSET_MSB		0x234
#define TEGRA_VI_CSI_1_SURFACE2_OFFSET_LSB		0x238
#define TEGRA_VI_CSI_1_SURFACE0_BF_OFFSET_MSB		0x23c
#define TEGRA_VI_CSI_1_SURFACE0_BF_OFFSET_LSB		0x240
#define TEGRA_VI_CSI_1_SURFACE1_BF_OFFSET_MSB		0x244
#define TEGRA_VI_CSI_1_SURFACE1_BF_OFFSET_LSB		0x248
#define TEGRA_VI_CSI_1_SURFACE2_BF_OFFSET_MSB		0x24c
#define TEGRA_VI_CSI_1_SURFACE2_BF_OFFSET_LSB		0x250
#define TEGRA_VI_CSI_1_SURFACE0_STRIDE			0x254
#define TEGRA_VI_CSI_1_SURFACE1_STRIDE			0x258
#define TEGRA_VI_CSI_1_SURFACE2_STRIDE			0x25c
#define TEGRA_VI_CSI_1_SURFACE_HEIGHT0			0x260
#define TEGRA_VI_CSI_1_ISPINTF_CONFIG			0x264
#define TEGRA_VI_CSI_1_ERROR_STATUS			0x284
#define TEGRA_VI_CSI_1_ERROR_INT_MASK			0x288
#define TEGRA_VI_CSI_1_WD_CTRL				0x28c
#define TEGRA_VI_CSI_1_WD_PERIOD			0x290

#define TEGRA_CSI_CSI_CAP_CIL				0x808
#define TEGRA_CSI_CSI_CAP_CSI				0x818
#define TEGRA_CSI_CSI_CAP_PP				0x828
#define TEGRA_CSI_INPUT_STREAM_A_CONTROL		0x838
#define TEGRA_CSI_PIXEL_STREAM_A_CONTROL0		0x83c
#define TEGRA_CSI_PIXEL_STREAM_A_CONTROL1		0x840
#define TEGRA_CSI_PIXEL_STREAM_A_GAP			0x844
#define TEGRA_CSI_PIXEL_STREAM_PPA_COMMAND		0x848
#define TEGRA_CSI_PIXEL_STREAM_A_EXPECTED_FRAME		0x84c
#define TEGRA_CSI_CSI_PIXEL_PARSER_A_INTERRUPT_MASK	0x850
#define TEGRA_CSI_CSI_PIXEL_PARSER_A_STATUS		0x854
#define TEGRA_CSI_CSI_SW_SENSOR_A_RESET			0x858
#define TEGRA_CSI_INPUT_STREAM_B_CONTROL		0x86c
#define TEGRA_CSI_PIXEL_STREAM_B_CONTROL0		0x870
#define TEGRA_CSI_PIXEL_STREAM_B_CONTROL1		0x874
#define TEGRA_CSI_PIXEL_STREAM_B_GAP			0x878
#define TEGRA_CSI_PIXEL_STREAM_PPB_COMMAND		0x87c
#define TEGRA_CSI_PIXEL_STREAM_B_EXPECTED_FRAME		0x880
#define TEGRA_CSI_CSI_PIXEL_PARSER_B_INTERRUPT_MASK	0x884
#define TEGRA_CSI_CSI_PIXEL_PARSER_B_STATUS		0x888
#define TEGRA_CSI_CSI_SW_SENSOR_B_RESET			0x88c
#define TEGRA_CSI_PHY_CIL_COMMAND			0x908
#define TEGRA_CSI_CIL_PAD_CONFIG0			0x90c

#define TEGRA_CSI_CILA_PAD_CONFIG0			0x92c
#define TEGRA_CSI_CILA_PAD_CONFIG1			0x930
#define TEGRA_CSI_PHY_CILA_CONTROL0			0x934
#define TEGRA_CSI_CSI_CIL_A_INTERRUPT_MASK		0x938
#define TEGRA_CSI_CSI_CIL_A_STATUS			0x93c
#define TEGRA_CSI_CSI_CILA_STATUS			0x940
#define TEGRA_CSI_CIL_A_ESCAPE_MODE_COMMAND		0x944
#define TEGRA_CSI_CIL_A_ESCAPE_MODE_DATA		0x948
#define TEGRA_CSI_CSICIL_SW_SENSOR_A_RESET		0x94c

#define TEGRA_CSI_CILB_PAD_CONFIG0			0x960
#define TEGRA_CSI_CILB_PAD_CONFIG1			0x964
#define TEGRA_CSI_PHY_CILB_CONTROL0			0x968
#define TEGRA_CSI_CSI_CIL_B_INTERRUPT_MASK		0x96c
#define TEGRA_CSI_CSI_CIL_B_STATUS			0x970
#define TEGRA_CSI_CSI_CILB_STATUS			0x974
#define TEGRA_CSI_CIL_B_ESCAPE_MODE_COMMAND		0x978
#define TEGRA_CSI_CIL_B_ESCAPE_MODE_DATA		0x97c
#define TEGRA_CSI_CSICIL_SW_SENSOR_B_RESET		0x980

#define TEGRA_CSI_CILC_PAD_CONFIG0			0x994
#define TEGRA_CSI_CILC_PAD_CONFIG1			0x998
#define TEGRA_CSI_PHY_CILC_CONTROL0			0x99c
#define TEGRA_CSI_CSI_CIL_C_INTERRUPT_MASK		0x9a0
#define TEGRA_CSI_CSI_CIL_C_STATUS			0x9a4
#define TEGRA_CSI_CSI_CILC_STATUS			0x9a8
#define TEGRA_CSI_CIL_C_ESCAPE_MODE_COMMAND		0x9ac
#define TEGRA_CSI_CIL_C_ESCAPE_MODE_DATA		0x9b0
#define TEGRA_CSI_CSICIL_SW_SENSOR_C_RESET		0x9b4

#define TEGRA_CSI_CILD_PAD_CONFIG0			0x9c8
#define TEGRA_CSI_CILD_PAD_CONFIG1			0x9cc
#define TEGRA_CSI_PHY_CILD_CONTROL0			0x9d0
#define TEGRA_CSI_CSI_CIL_D_INTERRUPT_MASK		0x9d4
#define TEGRA_CSI_CSI_CIL_D_STATUS			0x9d8
#define TEGRA_CSI_CSI_CILD_STATUS			0x9dc
#define TEGRA_CSI_CIL_D_ESCAPE_MODE_COMMAND		0x9ec
#define TEGRA_CSI_CIL_D_ESCAPE_MODE_DATA		0x9f0
#define TEGRA_CSI_CSICIL_SW_SENSOR_D_RESET		0x9f4

#define TEGRA_CSI_CILE_PAD_CONFIG0			0xa08
#define TEGRA_CSI_CILE_PAD_CONFIG1			0xa0c
#define TEGRA_CSI_PHY_CILE_CONTROL0			0xa10
#define TEGRA_CSI_CSI_CIL_E_INTERRUPT_MASK		0xa14
#define TEGRA_CSI_CSI_CIL_E_STATUS			0xa18
#define TEGRA_CSI_CIL_E_ESCAPE_MODE_COMMAND		0xa1c
#define TEGRA_CSI_CIL_E_ESCAPE_MODE_DATA		0xa20
#define TEGRA_CSI_CSICIL_SW_SENSOR_E_RESET		0xa24

#define TEGRA_CSI_PATTERN_GENERATOR_CTRL_A		0xa68
#define TEGRA_CSI_PG_BLANK_A				0xa6c
#define TEGRA_CSI_PG_PHASE_A				0xa70
#define TEGRA_CSI_PG_RED_FREQ_A				0xa74
#define TEGRA_CSI_PG_RED_FREQ_RATE_A			0xa78
#define TEGRA_CSI_PG_GREEN_FREQ_A			0xa7c
#define TEGRA_CSI_PG_GREEN_FREQ_RATE_A			0xa80
#define TEGRA_CSI_PG_BLUE_FREQ_A			0xa84
#define TEGRA_CSI_PG_BLUE_FREQ_RATE_A			0xa88

#define TEGRA_CSI_PATTERN_GENERATOR_CTRL_B		0xa9c
#define TEGRA_CSI_PG_BLANK_B				0xaa0
#define TEGRA_CSI_PG_PHASE_B				0xaa4
#define TEGRA_CSI_PG_RED_FREQ_B				0xaa8
#define TEGRA_CSI_PG_RED_FREQ_RATE_B			0xaac
#define TEGRA_CSI_PG_GREEN_FREQ_B			0xab0
#define TEGRA_CSI_PG_GREEN_FREQ_RATE_B			0xab4
#define TEGRA_CSI_PG_BLUE_FREQ_B			0xab8
#define TEGRA_CSI_PG_BLUE_FREQ_RATE_B			0xabc

#define TEGRA_CSI_DPCM_CTRL_A				0xad0
#define TEGRA_CSI_DPCM_CTRL_B				0xad4
#define TEGRA_CSI_STALL_COUNTER				0xae8
#define TEGRA_CSI_CSI_READONLY_STATUS			0xaec
#define TEGRA_CSI_CSI_SW_STATUS_RESET			0xaf0
#define TEGRA_CSI_CLKEN_OVERRIDE			0xaf4
#define TEGRA_CSI_DEBUG_CONTROL				0xaf8
#define TEGRA_CSI_DEBUG_COUNTER_0			0xafc
#define TEGRA_CSI_DEBUG_COUNTER_1			0xb00
#define TEGRA_CSI_DEBUG_COUNTER_2			0xb04

static int vi2_port_is_valid(int port)
{
	return (((port) >= TEGRA_CAMERA_PORT_CSI_A) &&
		((port) <= TEGRA_CAMERA_PORT_CSI_E));
}

/* Clock settings for camera */
static struct tegra_camera_clk vi2_clks0[] = {
	{
		.name = "vi",
		.freq = 150000000,
		.use_devname = 1,
	},
	{
		.name = "vi_sensor",
		.freq = 24000000,
	},
	{
		.name = "csi",
		.freq = 0,
		.use_devname = 1,
	},
	{
		.name = "isp",
		.freq = 0,
	},
	{
		.name = "csus",
		.freq = 0,
		.use_devname = 1,
	},
	{
		.name = "sclk",
		.freq = 80000000,
	},
	{
		.name = "emc",
		.freq = 375000000,
	},
	{
		.name = "cilab",
		.freq = 102000000,
		.use_devname = 1,
	},
	/* Always put "p11_d" at the end */
	{
		.name = "pll_d",
		.freq = 0,
	},
};

static struct tegra_camera_clk vi2_clks1[] = {
	{
		.name = "vi",
		.freq = 150000000,
		.use_devname = 1,
	},
	{
		.name = "vi_sensor",
		.freq = 24000000,
	},
	{
		.name = "csi",
		.freq = 0,
		.use_devname = 1,
	},
	{
		.name = "isp",
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
	{
		.name = "cilcd",
		.freq = 0,
		.use_devname = 1,
	},
	{
		.name = "cile",
		.freq = 0,
		.use_devname = 1,
	},
	/* Always put "p11_d" at the end */
	{
		.name = "pll_d",
		.freq = 0,
	},
};

#define MAX_DEVID_LENGTH	16

static int vi2_clks_init(struct tegra_camera_dev *cam)
{
	struct platform_device *pdev = cam->ndev;
	char devname[MAX_DEVID_LENGTH];
	const char *pdev_name;
	struct tegra_camera_clk *clks;
	int i, dev_id, ret;

	pdev_name = dev_name(&pdev->dev);
	ret = sscanf(pdev_name, "vi.%1d", &dev_id);
	if (ret != 1) {
		dev_err(&pdev->dev, "Read dev_id failed!\n");
		return -ENODEV;
	}
	snprintf(devname, MAX_DEVID_LENGTH, "tegra_%s", pdev_name);

	switch (dev_id) {
	case 0:
		cam->num_clks = ARRAY_SIZE(vi2_clks0);
		cam->clks = vi2_clks0;
		break;
	case 1:
		cam->num_clks = ARRAY_SIZE(vi2_clks1);
		cam->clks = vi2_clks1;
		break;
	default:
		dev_err(&pdev->dev, "Wrong device ID %d\n", dev_id);
		return -ENODEV;
	}

	for (i = 0; i < cam->num_clks; i++) {
		clks = &cam->clks[i];

		if (clks->use_devname)
			clks->clk = clk_get_sys(devname, clks->name);
		else
			clks->clk = clk_get(&pdev->dev, clks->name);
		if (IS_ERR_OR_NULL(clks->clk)) {
			dev_err(&pdev->dev, "Failed to get clock %s.\n",
				clks->name);
			return PTR_ERR(clks->clk);
		}

		if (clks->freq > 0)
			clk_set_rate(clks->clk, clks->freq);
	}

	return 0;
}

static void vi2_clks_deinit(struct tegra_camera_dev *cam)
{
	struct tegra_camera_clk *clks;
	int i;

	for (i = 0; i < cam->num_clks; i++) {
		clks = &cam->clks[i];
		if (clks->clk)
			clk_put(clks->clk);
	}
}

static void vi2_clks_enable(struct tegra_camera_dev *cam)
{
	struct tegra_camera_clk *clks;
	int i;

	for (i = 0; i < cam->num_clks - 1; i++) {
		clks = &cam->clks[i];
		if (clks->clk)
			clk_prepare_enable(clks->clk);
	}

	if (cam->tpg_mode) {
		clks = &cam->clks[i];
		if (clks->clk) {
			clk_prepare_enable(clks->clk);
			tegra_clk_cfg_ex(clks->clk,
					 TEGRA_CLK_PLLD_CSI_OUT_ENB, 1);
			tegra_clk_cfg_ex(clks->clk,
					 TEGRA_CLK_PLLD_DSI_OUT_ENB, 1);
			tegra_clk_cfg_ex(clks->clk,
					 TEGRA_CLK_MIPI_CSI_OUT_ENB, 0);
		}
	}
}

static void vi2_clks_disable(struct tegra_camera_dev *cam)
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
			tegra_clk_cfg_ex(clks->clk,
					 TEGRA_CLK_MIPI_CSI_OUT_ENB, 1);
			tegra_clk_cfg_ex(clks->clk,
					 TEGRA_CLK_PLLD_CSI_OUT_ENB, 0);
			tegra_clk_cfg_ex(clks->clk,
					 TEGRA_CLK_PLLD_DSI_OUT_ENB, 0);
			clk_disable_unprepare(clks->clk);
		}
	}
}

static void vi2_save_syncpts(struct tegra_camera_dev *cam)
{
	cam->syncpt_csi_a =
		nvhost_syncpt_read_ext(cam->ndev,
				       TEGRA_VI_SYNCPT_CSI_A);

	cam->syncpt_csi_b =
		nvhost_syncpt_read_ext(cam->ndev,
				       TEGRA_VI_SYNCPT_CSI_B);
}

static void vi2_incr_syncpts(struct tegra_camera_dev *cam)
{
	nvhost_syncpt_cpu_incr_ext(cam->ndev,
				   TEGRA_VI_SYNCPT_CSI_A);

	nvhost_syncpt_cpu_incr_ext(cam->ndev,
				   TEGRA_VI_SYNCPT_CSI_B);
}

static void vi2_capture_clean(struct tegra_camera_dev *cam)
{
	/* Clean up status */
	TC_VI_REG_WT(cam, TEGRA_CSI_CSI_CIL_A_STATUS, 0xFFFFFFFF);
	TC_VI_REG_WT(cam, TEGRA_CSI_CSI_CILA_STATUS, 0xFFFFFFFF);
	TC_VI_REG_WT(cam, TEGRA_CSI_CSI_CIL_B_STATUS, 0xFFFFFFFF);
	TC_VI_REG_WT(cam, TEGRA_CSI_CSI_CIL_C_STATUS, 0xFFFFFFFF);
	TC_VI_REG_WT(cam, TEGRA_CSI_CSI_CIL_D_STATUS, 0xFFFFFFFF);
	TC_VI_REG_WT(cam, TEGRA_CSI_CSI_CIL_E_STATUS, 0xFFFFFFFF);
	TC_VI_REG_WT(cam, TEGRA_CSI_CSI_PIXEL_PARSER_A_STATUS, 0xFFFFFFFF);
	TC_VI_REG_WT(cam, TEGRA_VI_CSI_0_ERROR_STATUS, 0xFFFFFFFF);
}

static int vi2_capture_setup_csi_0(struct tegra_camera_dev *cam,
				    struct soc_camera_device *icd)
{
	struct soc_camera_subdev_desc *ssdesc = &icd->sdesc->subdev_desc;
	struct tegra_camera_platform_data *pdata = ssdesc->drv_priv;

	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_PPA_COMMAND, 0xf007);
	TC_VI_REG_WT(cam, TEGRA_CSI_CSI_PIXEL_PARSER_A_INTERRUPT_MASK, 0x0);
	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_A_CONTROL0, 0x280301f0);
	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_PPA_COMMAND, 0xf007);
	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_A_CONTROL1, 0x11);
	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_A_GAP, 0x140000);

	TC_VI_REG_WT(cam, TEGRA_CSI_INPUT_STREAM_A_CONTROL,
			0x3f0000 | (pdata->lanes - 1));
	if (pdata->lanes == 4)
		TC_VI_REG_WT(cam, TEGRA_CSI_PHY_CIL_COMMAND, 0x22020101);
	else
		TC_VI_REG_WT(cam, TEGRA_CSI_PHY_CIL_COMMAND, 0x22020201);

	/* VI_MWA_REQ_DONE */
	TC_VI_REG_WT(cam, TEGRA_VI_CFG_VI_INCR_SYNCPT,
			(0x4 << 8) | TEGRA_VI_SYNCPT_CSI_A);

	if (cam->tpg_mode) {
		TC_VI_REG_WT(cam, TEGRA_CSI_PATTERN_GENERATOR_CTRL_A,
				((cam->tpg_mode - 1) << 2) | 0x1);
		TC_VI_REG_WT(cam, TEGRA_CSI_PG_PHASE_A, 0x0);
		TC_VI_REG_WT(cam, TEGRA_CSI_PG_RED_FREQ_A, 0x100010);
		TC_VI_REG_WT(cam, TEGRA_CSI_PG_RED_FREQ_RATE_A, 0x0);
		TC_VI_REG_WT(cam, TEGRA_CSI_PG_GREEN_FREQ_A, 0x100010);
		TC_VI_REG_WT(cam, TEGRA_CSI_PG_GREEN_FREQ_RATE_A, 0x0);
		TC_VI_REG_WT(cam, TEGRA_CSI_PG_BLUE_FREQ_A, 0x100010);
		TC_VI_REG_WT(cam, TEGRA_CSI_PG_BLUE_FREQ_RATE_A, 0x0);
		TC_VI_REG_WT(cam, TEGRA_CSI_PHY_CIL_COMMAND, 0x22020202);

		/* output format A8B8G8R8, only support direct to mem */
		TC_VI_REG_WT(cam, TEGRA_VI_CSI_0_IMAGE_DEF, (64 << 16) | 0x1);
		/* input format is RGB888 */
		TC_VI_REG_WT(cam, TEGRA_VI_CSI_0_CSI_IMAGE_DT, 36);
	}

	TC_VI_REG_WT(cam, TEGRA_VI_CSI_0_CSI_IMAGE_SIZE,
		     (icd->user_height << 16) | icd->user_width);

	TC_VI_REG_WT(cam, TEGRA_VI_CSI_0_CSI_IMAGE_SIZE_WC,
		     icd->user_width * 3);

	return 0;
}

static int vi2_capture_setup_csi_1(struct tegra_camera_dev *cam,
				     struct soc_camera_device *icd)
{
	struct soc_camera_subdev_desc *ssdesc = &icd->sdesc->subdev_desc;
	struct tegra_camera_platform_data *pdata = ssdesc->drv_priv;

	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_PPB_COMMAND, 0xf007);
	TC_VI_REG_WT(cam, TEGRA_CSI_CSI_PIXEL_PARSER_B_INTERRUPT_MASK, 0x0);
	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_B_CONTROL0, 0x280301f0);
	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_PPB_COMMAND, 0xf007);
	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_B_CONTROL1, 0x11);
	TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_B_GAP, 0x140000);

	TC_VI_REG_WT(cam, TEGRA_CSI_INPUT_STREAM_B_CONTROL,
			0x3f0000 | (pdata->lanes - 1));
	if (pdata->lanes == 4)
		TC_VI_REG_WT(cam, TEGRA_CSI_PHY_CIL_COMMAND, 0x21010202);
	else if (pdata->lanes == 1 && pdata->port == TEGRA_CAMERA_PORT_CSI_E)
		TC_VI_REG_WT(cam, TEGRA_CSI_PHY_CIL_COMMAND, 0x12020202);
	else
		TC_VI_REG_WT(cam, TEGRA_CSI_PHY_CIL_COMMAND, 0x22010202);

	/* VI_MWB_REQ_DONE */
	TC_VI_REG_WT(cam, TEGRA_VI_CFG_VI_INCR_SYNCPT,
			(0x5 << 8) | TEGRA_VI_SYNCPT_CSI_B);

	if (cam->tpg_mode) {
		TC_VI_REG_WT(cam, TEGRA_CSI_PATTERN_GENERATOR_CTRL_B,
				((cam->tpg_mode - 1) << 2) | 0x1);
		TC_VI_REG_WT(cam, TEGRA_CSI_PG_PHASE_B, 0x0);
		TC_VI_REG_WT(cam, TEGRA_CSI_PG_RED_FREQ_B, 0x100010);
		TC_VI_REG_WT(cam, TEGRA_CSI_PG_RED_FREQ_RATE_B, 0x0);
		TC_VI_REG_WT(cam, TEGRA_CSI_PG_GREEN_FREQ_B, 0x100010);
		TC_VI_REG_WT(cam, TEGRA_CSI_PG_GREEN_FREQ_RATE_B, 0x0);
		TC_VI_REG_WT(cam, TEGRA_CSI_PG_BLUE_FREQ_B, 0x100010);
		TC_VI_REG_WT(cam, TEGRA_CSI_PG_BLUE_FREQ_RATE_B, 0x0);
		TC_VI_REG_WT(cam, TEGRA_CSI_PHY_CIL_COMMAND, 0x22020202);

		/* output format A8B8G8R8, only support direct to mem */
		TC_VI_REG_WT(cam, TEGRA_VI_CSI_1_IMAGE_DEF, (64 << 16) | 0x1);
		/* input format is RGB888 */
		TC_VI_REG_WT(cam, TEGRA_VI_CSI_1_CSI_IMAGE_DT, 36);
	}

	TC_VI_REG_WT(cam, TEGRA_VI_CSI_1_CSI_IMAGE_SIZE,
		     (icd->user_height << 16) | icd->user_width);

	TC_VI_REG_WT(cam, TEGRA_VI_CSI_1_CSI_IMAGE_SIZE_WC,
		     icd->user_width * 3);

	return 0;
}

static int vi2_capture_setup(struct tegra_camera_dev *cam)
{
	struct vb2_buffer *vb = cam->active;
	struct tegra_camera_buffer *buf = to_tegra_vb(vb);
	struct soc_camera_device *icd = buf->icd;
	struct soc_camera_subdev_desc *ssdesc = &icd->sdesc->subdev_desc;
	struct tegra_camera_platform_data *pdata = ssdesc->drv_priv;
	int port = pdata->port;

	/*
	 * MIPI pad controls
	 * MIPI_CAL_MIPI_BIAS_PAD_CFG0_0 MIPI_BIAS_PAD_E_VCLAMP_REF 1
	 * MIPI_CAL_MIPI_BIAS_PAD_CFG2_0 PAD_PDVREG 0
	 */

	/*
	 * PAD_CILA_PDVCLAMP 0, PAD_CILA_PDIO_CLK 0,
	 * PAD_CILA_PDIO 0, PAD_AB_BK_MODE 1
	 */
	TC_VI_REG_WT(cam, TEGRA_CSI_CILA_PAD_CONFIG0, 0x10000);

	/* PAD_CILB_PDVCLAMP 0, PAD_CILB_PDIO_CLK 0, PAD_CILB_PDIO 0 */
	TC_VI_REG_WT(cam, TEGRA_CSI_CILB_PAD_CONFIG0, 0x0);

	/*
	 * PAD_CILC_PDVCLAMP 0, PAD_CILC_PDIO_CLK 0,
	 * PAD_CILC_PDIO 0, PAD_CD_BK_MODE 1
	 */
	TC_VI_REG_WT(cam, TEGRA_CSI_CILC_PAD_CONFIG0, 0x10000);

	/* PAD_CILD_PDVCLAMP 0, PAD_CILD_PDIO_CLK 0, PAD_CILD_PDIO 0 */
	TC_VI_REG_WT(cam, TEGRA_CSI_CILD_PAD_CONFIG0, 0x0);

	/* PAD_CILE_PDVCLAMP 0, PAD_CILE_PDIO_CLK 0, PAD_CILE_PDIO 0 */
	TC_VI_REG_WT(cam, TEGRA_CSI_CILE_PAD_CONFIG0, 0x0);

	/* Common programming set for any config */
	TC_VI_REG_WT(cam, TEGRA_CSI_CLKEN_OVERRIDE, 0x0);
	TC_VI_REG_WT(cam, TEGRA_CSI_PHY_CIL_COMMAND, 0x22020202);
	TC_VI_REG_WT(cam, TEGRA_CSI_CSI_CIL_A_INTERRUPT_MASK, 0x0);
	TC_VI_REG_WT(cam, TEGRA_CSI_CSI_CIL_B_INTERRUPT_MASK, 0x0);
	TC_VI_REG_WT(cam, TEGRA_CSI_CSI_CIL_C_INTERRUPT_MASK, 0x0);
	TC_VI_REG_WT(cam, TEGRA_CSI_CSI_CIL_D_INTERRUPT_MASK, 0x0);
	TC_VI_REG_WT(cam, TEGRA_CSI_CSI_CIL_E_INTERRUPT_MASK, 0x0);

	/*
	 * TODO: these values should be different with different
	 * sensor connected.
	 * Hardcode THS settle value just for TPG testing
	 */
	TC_VI_REG_WT(cam, TEGRA_CSI_PHY_CILA_CONTROL0, 0x8);
	TC_VI_REG_WT(cam, TEGRA_CSI_PHY_CILB_CONTROL0, 0x8);
	TC_VI_REG_WT(cam, TEGRA_CSI_PHY_CILC_CONTROL0, 0xa);
	TC_VI_REG_WT(cam, TEGRA_CSI_PHY_CILD_CONTROL0, 0xa);
	TC_VI_REG_WT(cam, TEGRA_CSI_PHY_CILE_CONTROL0, 0xa);

	/* Setup registers for CSI-A and CSI-B inputs */
	if (port == TEGRA_CAMERA_PORT_CSI_A)
		return vi2_capture_setup_csi_0(cam, icd);
	else if (port == TEGRA_CAMERA_PORT_CSI_B)
		return vi2_capture_setup_csi_1(cam, icd);
	else
		return -ENODEV;
}

static int vi2_capture_buffer_setup(struct tegra_camera_dev *cam,
			struct tegra_camera_buffer *buf)
{
	struct soc_camera_device *icd = buf->icd;
	int bytes_per_line = soc_mbus_bytes_per_line(icd->user_width,
			icd->current_fmt->host_fmt);

	switch (icd->current_fmt->host_fmt->fourcc) {
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		/* FIXME: Setup YUV buffer */

	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_RGB32:
		switch (buf->output_channel) {
		case 0:
			TC_VI_REG_WT(cam, TEGRA_VI_CSI_0_SURFACE0_OFFSET_MSB,
				     0x0);
			TC_VI_REG_WT(cam, TEGRA_VI_CSI_0_SURFACE0_OFFSET_LSB,
				     buf->buffer_addr);
			TC_VI_REG_WT(cam, TEGRA_VI_CSI_0_SURFACE0_STRIDE,
				     bytes_per_line);
			break;
		case 1:
			TC_VI_REG_WT(cam, TEGRA_VI_CSI_0_SURFACE1_OFFSET_MSB,
				     0x0);
			TC_VI_REG_WT(cam, TEGRA_VI_CSI_0_SURFACE1_OFFSET_LSB,
				     buf->buffer_addr);
			TC_VI_REG_WT(cam, TEGRA_VI_CSI_0_SURFACE1_STRIDE,
				     bytes_per_line);
			break;
		case 2:
			TC_VI_REG_WT(cam, TEGRA_VI_CSI_0_SURFACE2_OFFSET_MSB,
				     0x0);
			TC_VI_REG_WT(cam, TEGRA_VI_CSI_0_SURFACE2_OFFSET_LSB,
				     buf->buffer_addr);
			TC_VI_REG_WT(cam, TEGRA_VI_CSI_0_SURFACE2_STRIDE,
				     bytes_per_line);
			break;
		}
		break;

	default:
		dev_err(&cam->ndev->dev, "Wrong host format %d\n",
			icd->current_fmt->host_fmt->fourcc);
		return -EINVAL;
	}

	return 0;
}

static int vi2_capture_start(struct tegra_camera_dev *cam,
				      struct tegra_camera_buffer *buf)
{
	struct soc_camera_device *icd = buf->icd;
	struct soc_camera_subdev_desc *ssdesc = &icd->sdesc->subdev_desc;
	struct tegra_camera_platform_data *pdata = ssdesc->drv_priv;
	int port = pdata->port;
	int err;

	err = vi2_capture_buffer_setup(cam, buf);
	if (err < 0)
		return err;

	/* Only wait on CSI frame end syncpt if we're using CSI. */
	if (port == TEGRA_CAMERA_PORT_CSI_A) {
		cam->syncpt_csi_a++;
		TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_PPA_COMMAND,
				0x0000f005);
		TC_VI_REG_WT(cam, TEGRA_VI_CSI_0_SINGLE_SHOT, 0x1);
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
		TC_VI_REG_WT(cam, TEGRA_VI_CSI_1_SINGLE_SHOT, 0x1);
		err = nvhost_syncpt_wait_timeout_ext(cam->ndev,
				TEGRA_VI_SYNCPT_CSI_B,
				cam->syncpt_csi_b,
				TEGRA_SYNCPT_CSI_WAIT_TIMEOUT,
				NULL,
				NULL);
	}

	if (!err)
		return 0;

	err = TC_VI_REG_RD(cam, TEGRA_CSI_CSI_CIL_A_STATUS);
	if (err)
		pr_err("TEGRA_CSI_CSI_CIL_A_STATUS 0x%08x\n", err);
	err = TC_VI_REG_RD(cam, TEGRA_CSI_CSI_CILA_STATUS);
	if (err)
		pr_err("TEGRA_CSI_CSI_CILA_STATUS 0x%08x\n", err);
	err = TC_VI_REG_RD(cam, TEGRA_CSI_CSI_CIL_B_STATUS);
	if (err)
		pr_err("TEGRA_CSI_CSI_CIL_B_STATUS 0x%08x\n", err);
	err = TC_VI_REG_RD(cam, TEGRA_CSI_CSI_CIL_C_STATUS);
	if (err)
		pr_err("TEGRA_CSI_CSI_CIL_C_STATUS 0x%08x\n", err);
	err = TC_VI_REG_RD(cam, TEGRA_CSI_CSI_CIL_D_STATUS);
	if (err)
		pr_err("TEGRA_CSI_CSI_CIL_D_STATUS 0x%08x\n", err);
	err = TC_VI_REG_RD(cam, TEGRA_CSI_CSI_CIL_E_STATUS);
	if (err)
		pr_err("TEGRA_CSI_CSI_CIL_E_STATUS 0x%08x\n", err);

	err = TC_VI_REG_RD(cam, TEGRA_CSI_CSI_PIXEL_PARSER_A_STATUS);
	if (err)
		pr_err("TEGRA_CSI_CSI_PIXEL_PARSER_A_STATUS 0x%08x\n", err);

	err = TC_VI_REG_RD(cam, TEGRA_VI_CSI_0_ERROR_STATUS);
	if (err)
		pr_err("TEGRA_VI_CSI_0_ERROR_STATUS 0x%08x\n", err);

	return err;
}

static int vi2_capture_stop(struct tegra_camera_dev *cam, int port)
{
	if (port == TEGRA_CAMERA_PORT_CSI_A)
		TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_PPA_COMMAND,
			     0x0000f002);
	else if (port == TEGRA_CAMERA_PORT_CSI_B)
		TC_VI_REG_WT(cam, TEGRA_CSI_PIXEL_STREAM_PPB_COMMAND,
			     0x0000f002);

	return 0;
}

/* Reset VI2/CSI2 when activating, no sepecial ops for deactiving  */
static void vi2_sw_reset(struct tegra_camera_dev *cam)
{
	/* T12_CG_2ND_LEVEL_EN */
	TC_VI_REG_WT(cam, TEGRA_VI_CFG_CG_CTRL, 1);
	TC_VI_REG_WT(cam, TEGRA_VI_CSI_0_SW_RESET, 0x1F);
	TC_VI_REG_WT(cam, TEGRA_VI_CSI_1_SW_RESET, 0x1F);

	udelay(10);
}

struct tegra_camera_ops vi2_ops = {
	.clks_init = vi2_clks_init,
	.clks_deinit = vi2_clks_deinit,
	.clks_enable = vi2_clks_enable,
	.clks_disable = vi2_clks_disable,

	.capture_clean = vi2_capture_clean,
	.capture_setup = vi2_capture_setup,
	.capture_start = vi2_capture_start,
	.capture_stop = vi2_capture_stop,

	.activate = vi2_sw_reset,

	.save_syncpts = vi2_save_syncpts,
	.incr_syncpts = vi2_incr_syncpts,

	.port_is_valid = vi2_port_is_valid,
};

int vi2_register(struct tegra_camera_dev *cam)
{
	/* Init regulator */
	cam->regulator_name = "avdd_dsi_csi";

	/* Init VI2/CSI2 ops */
	cam->ops = &vi2_ops;

	return 0;
}
