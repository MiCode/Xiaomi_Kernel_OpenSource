/*
 * drivers/video/tegra/dc/tc358770_dsi2edp.c
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/swab.h>
#include <linux/module.h>

#include <mach/dc.h>

#include "dc_priv.h"
#include "tc358770_dsi2edp.h"
#include "dsi.h"

static struct tegra_dc_dsi2edp_data *tc358770_dsi2edp;
static struct i2c_client *tc358770_i2c_client;

enum i2c_transfer_type {
	I2C_WRITE,
	I2C_READ,
};

/*
 * TC358770 requires register address in big endian
 * and register value in little endian.
 * Regmap currently sends it all in big endian.
*/
#define TO_LITTLE_ENDIAN	(true)

static inline void tc358770_reg_write(struct tegra_dc_dsi2edp_data *dsi2edp,
					unsigned int addr, unsigned int val)
{
	regmap_write(dsi2edp->regmap, addr,
		TO_LITTLE_ENDIAN ? __swab32(val) : val);
}

static inline void tc358770_reg_read(struct tegra_dc_dsi2edp_data *dsi2edp,
					unsigned int addr, unsigned int *val)
{
	regmap_read(dsi2edp->regmap, addr, val);
	*val = TO_LITTLE_ENDIAN ? __swab32(*val) : *val;
}

static const struct regmap_config tc358770_regmap_config = {
	.reg_bits = 16,
	.val_bits = 32,
};

static int tc358770_dsi2edp_init(struct tegra_dc_dsi_data *dsi)
{
	int err = 0;

	if (tc358770_dsi2edp) {
		tegra_dsi_set_outdata(dsi, tc358770_dsi2edp);
		return err;
	}

	tc358770_dsi2edp = devm_kzalloc(&dsi->dc->ndev->dev,
					sizeof(*tc358770_dsi2edp),
					GFP_KERNEL);
	if (!tc358770_dsi2edp)
		return -ENOMEM;

	tc358770_dsi2edp->dsi = dsi;

	tc358770_dsi2edp->client_i2c = tc358770_i2c_client;

	tc358770_dsi2edp->regmap = devm_regmap_init_i2c(tc358770_i2c_client,
						&tc358770_regmap_config);
	if (IS_ERR(tc358770_dsi2edp->regmap)) {
		err = PTR_ERR(tc358770_dsi2edp->regmap);
		dev_err(&dsi->dc->ndev->dev,
				"tc358770_dsi2edp: regmap init failed\n");
		goto fail;
	}

	tc358770_dsi2edp->mode = &dsi->dc->mode;

	tegra_dsi_set_outdata(dsi, tc358770_dsi2edp);

	mutex_init(&tc358770_dsi2edp->lock);

fail:
	return err;
}

static void tc358770_dsi2edp_destroy(struct tegra_dc_dsi_data *dsi)
{
	struct tegra_dc_dsi2edp_data *dsi2edp =
				tegra_dsi_get_outdata(dsi);

	if (!dsi2edp)
		return;

	tc358770_dsi2edp = NULL;
	mutex_destroy(&dsi2edp->lock);
}

static void tc358770_dsi2edp_enable(struct tegra_dc_dsi_data *dsi)
{
	struct tegra_dc_dsi2edp_data *dsi2edp = tegra_dsi_get_outdata(dsi);
	unsigned val;
	unsigned chip_id;

	if (dsi2edp && dsi2edp->dsi2edp_enabled)
		return;

	mutex_lock(&dsi2edp->lock);

	/* Chip ID */
	tc358770_reg_read(dsi2edp, TC358770_CHIP_ID, &chip_id);

	tc358770_reg_write(dsi2edp, TC358770_LINK_TRAINING_CTRL, 0xC0B5);
	tc358770_reg_write(dsi2edp, TC358770_SYSTEM_CLK_PARAM, 0x0105);
	tc358770_reg_write(dsi2edp, TC358770_PHY_CTRL, 0x0F);
	tc358770_reg_write(dsi2edp, TC358770_LINK_CLK_PLL_CTRL, 0x05);
	msleep(70);

	tc358770_reg_write(dsi2edp,
		TC358770_STREAM_CLK_PLL_PARAM, 0x518146);
	tc358770_reg_write(dsi2edp,
		TC358770_STREAM_CLK_PLL_CTRL, 0x05);

	tc358770_reg_write(dsi2edp, TC358770_PHY_CTRL, 0x1F);
	tc358770_reg_write(dsi2edp, TC358770_PHY_CTRL, 0x0F);
	msleep(70);

	/* Check main channel ready */
	tc358770_reg_read(dsi2edp, TC358770_PHY_CTRL, &val);

	tc358770_reg_write(dsi2edp,
		TC358770_AUX_CHANNEL_CONFIG1, 0x01063F);
	tc358770_reg_write(dsi2edp,
		TC358770_AUX_CHANNEL_DPCD_ADDR, 0x01);
	tc358770_reg_write(dsi2edp,
		TC358770_AUX_CHANNEL_CONFIG0, 0x09);
	msleep(70);

	/* Check aux channel status */
	tc358770_reg_read(dsi2edp, TC358770_AUX_CHANNEL_STATUS, &val);
	if (val & (0x01 << 1))
		goto timeout;

	tc358770_reg_read(dsi2edp, TC358770_AUX_CHANNEL_DPCD_RD_DATA0, &val);

	tc358770_reg_write(dsi2edp,
		TC358770_AUX_CHANNEL_DPCD_ADDR, 0x02);
	tc358770_reg_write(dsi2edp,
		TC358770_AUX_CHANNEL_CONFIG0, 0x09);
	msleep(70);

	tc358770_reg_read(dsi2edp, TC358770_AUX_CHANNEL_STATUS, &val);
	tc358770_reg_read(dsi2edp, TC358770_AUX_CHANNEL_DPCD_RD_DATA0, &val);

	tc358770_reg_write(dsi2edp,
		TC358770_AUX_CHANNEL_DPCD_ADDR, 0x0100);
	tc358770_reg_write(dsi2edp,
		TC358770_AUX_CHANNEL_DPCD_WR_DATA0, 0x040A);
	tc358770_reg_write(dsi2edp,
		TC358770_AUX_CHANNEL_CONFIG0, 0x0108);
	tc358770_reg_write(dsi2edp,
		TC358770_AUX_CHANNEL_DPCD_ADDR, 0x0108);
	tc358770_reg_write(dsi2edp,
		TC358770_AUX_CHANNEL_DPCD_WR_DATA0, 0x01);
	tc358770_reg_write(dsi2edp,
		TC358770_AUX_CHANNEL_CONFIG0, 0x08);
	tc358770_reg_write(dsi2edp,
		TC358770_LINK_TRAINING_SINK_CONFIG, 0x21);
	tc358770_reg_write(dsi2edp,
		TC358770_LINK_TRAINING_LOOP_CTRL, 0x7600000D);
	tc358770_reg_write(dsi2edp, TC358770_LINK_TRAINING_CTRL, 0xC1B5);
	tc358770_reg_write(dsi2edp, TC358770_DP_CTRL, 0x41);
	msleep(70);

	tc358770_reg_read(dsi2edp, TC358770_LINK_TRAINING_STATUS, &val);

	tc358770_reg_write(dsi2edp,
		TC358770_LINK_TRAINING_SINK_CONFIG, 0x22);
	tc358770_reg_write(dsi2edp, TC358770_LINK_TRAINING_CTRL, 0xC2B5);
	msleep(70);

	tc358770_reg_read(dsi2edp, TC358770_LINK_TRAINING_STATUS, &val);

	tc358770_reg_write(dsi2edp,
		TC358770_AUX_CHANNEL_DPCD_ADDR, 0x0102);
	tc358770_reg_write(dsi2edp,
		TC358770_AUX_CHANNEL_DPCD_WR_DATA0, 0x00);
	tc358770_reg_write(dsi2edp,
		TC358770_AUX_CHANNEL_CONFIG0, 0x08);
	tc358770_reg_write(dsi2edp, TC358770_LINK_TRAINING_CTRL, 0x40B5);
	msleep(70);

	tc358770_reg_write(dsi2edp,
		TC358770_AUX_CHANNEL_DPCD_ADDR , 0x0200);
	tc358770_reg_write(dsi2edp,
		TC358770_AUX_CHANNEL_CONFIG0 , 0x0409);

	tc358770_reg_read(dsi2edp, TC358770_AUX_CHANNEL_STATUS, &val);
	tc358770_reg_read(dsi2edp, TC358770_AUX_CHANNEL_DPCD_RD_DATA0, &val);
	tc358770_reg_read(dsi2edp, TC358770_AUX_CHANNEL_DPCD_RD_DATA1, &val);
	msleep(100);

	/* ASSR configuration, 770A(reg 0x0500[0] = 1) supports ASSR,
	 * need to check the ASSR capability for eDP panel(0x0500[1] = 0).
	 */
	if (chip_id & 0x01) {
		tc358770_reg_write(dsi2edp,
			TC358770_AUX_CHANNEL_DPCD_ADDR , 0x000D);
		tc358770_reg_write(dsi2edp,
			TC358770_AUX_CHANNEL_CONFIG0 , 0x0009);
		tc358770_reg_read(dsi2edp, TC358770_AUX_CHANNEL_STATUS, &val);
		tc358770_reg_read(dsi2edp,
			TC358770_AUX_CHANNEL_DPCD_RD_DATA0, &val);

		if (val & 0x01) {
			/* Enable ASSR*/
			tc358770_reg_write(dsi2edp,
				TC358770_AUX_CHANNEL_DPCD_ADDR, 0x010A);
			tc358770_reg_write(dsi2edp,
				TC358770_AUX_CHANNEL_DPCD_WR_DATA0, 0x01);
			tc358770_reg_write(dsi2edp,
				TC358770_AUX_CHANNEL_CONFIG0, 0x08);
		}
	}

	/* DSI0 setting */
	tc358770_reg_write(dsi2edp, TC358770_DSI0_PPI_TX_RX_TA , 0x000A000C);
	tc358770_reg_write(dsi2edp,
		TC358770_DSI0_PPI_LPTXTIMECNT , 0x8);
	tc358770_reg_write(dsi2edp,
		TC358770_DSI0_PPI_D0S_CLRSIPOCOUNT , 0x8);
	tc358770_reg_write(dsi2edp,
		TC358770_DSI0_PPI_D1S_CLRSIPOCOUNT , 0x8);
	tc358770_reg_write(dsi2edp,
		TC358770_DSI0_PPI_D2S_CLRSIPOCOUNT , 0x8);
	tc358770_reg_write(dsi2edp,
		TC358770_DSI0_PPI_D3S_CLRSIPOCOUNT , 0x8);
	tc358770_reg_write(dsi2edp,
		TC358770_DSI0_PPI_LANEENABLE , 0x1F);
	tc358770_reg_write(dsi2edp,
		TC358770_DSI0_DSI_LANEENABLE , 0x1F);
	tc358770_reg_write(dsi2edp, TC358770_DSI0_PPI_START , 0x01);
	tc358770_reg_write(dsi2edp, TC358770_DSI0_DSI_START , 0x01);

	/* DSI1 setting */
	tc358770_reg_write(dsi2edp, TC358770_DSI1_PPI_TX_RX_TA , 0x000A000C);
	tc358770_reg_write(dsi2edp,
		TC358770_DSI1_PPI_LPTXTIMECNT , 0x08);
	tc358770_reg_write(dsi2edp,
		TC358770_DSI1_PPI_D0S_CLRSIPOCOUNT , 0x8);
	tc358770_reg_write(dsi2edp,
		TC358770_DSI1_PPI_D1S_CLRSIPOCOUNT , 0x8);
	tc358770_reg_write(dsi2edp,
		TC358770_DSI1_PPI_D2S_CLRSIPOCOUNT , 0x8);
	tc358770_reg_write(dsi2edp,
		TC358770_DSI1_PPI_D3S_CLRSIPOCOUNT , 0x8);
	tc358770_reg_write(dsi2edp,
		TC358770_DSI1_PPI_LANEENABLE , 0x1F);
	tc358770_reg_write(dsi2edp,
		TC358770_DSI1_DSI_LANEENABLE , 0x1F);
	tc358770_reg_write(dsi2edp, TC358770_DSI1_PPI_START , 0x01);
	tc358770_reg_write(dsi2edp, TC358770_DSI1_DSI_START , 0x01);

	/* Combiner logic */
	tc358770_reg_write(dsi2edp, TC358770_CMD_CTRL , 0x1);
	tc358770_reg_write(dsi2edp, TC358770_LR_SIZE , 0x05000500);
	tc358770_reg_write(dsi2edp, TC358770_RM_PXL , 0x00);
	msleep(70);

	/* lcd ctrl frame size */
	tc358770_reg_write(dsi2edp, TC358770_VIDEO_FRAME_CTRL , 0x28A00100);

	val = dsi2edp->mode->h_back_porch << 16 | dsi2edp->mode->h_sync_width;
	tc358770_reg_write(dsi2edp, TC358770_HORIZONTAL_TIME0 , val);

	val = dsi2edp->mode->h_front_porch << 16 | dsi2edp->mode->h_active;
	tc358770_reg_write(dsi2edp, TC358770_HORIZONTAL_TIME1 , val);

	val = dsi2edp->mode->v_back_porch << 16 | dsi2edp->mode->v_sync_width;
	tc358770_reg_write(dsi2edp, TC358770_VERTICAL_TIME0 , val);

	val = dsi2edp->mode->v_front_porch << 16 | dsi2edp->mode->v_active;
	tc358770_reg_write(dsi2edp, TC358770_VERTICAL_TIME1 , val);

	tc358770_reg_write(dsi2edp,
		TC358770_VIDEO_FRAME_UPDATE_ENABLE , 0x01);
	msleep(70);

	/* DP main stream attributes */
	tc358770_reg_write(dsi2edp,
		TC358770_VIDEO_FRAME_OUTPUT_DELAY , 0x001F0A70);
	tc358770_reg_write(dsi2edp, TC358770_VIDEO_FRAME_SIZE , 0x066E0AA0);
	tc358770_reg_write(dsi2edp, TC358770_VIDEO_FRAME_START , 0x002B0070);
	tc358770_reg_write(dsi2edp,
		TC358770_VIDEO_FRAME_ACTIVE_REGION_SIZE , 0x06400A00);
	tc358770_reg_write(dsi2edp,
		TC358770_VIDEO_FRAME_SYNC_WIDTH , 0x80068020);
	msleep(70);

	/* DP flow shape & timestamp */
	tc358770_reg_write(dsi2edp, TC358770_DP_CONFIG , 0x1EBF0020);

	tc358770_reg_write(dsi2edp,
		TC358770_NVALUE_VIDEO_CLK_REGEN , 0x0465);
	tc358770_reg_write(dsi2edp, TC358770_DP_CTRL , 0x41);
	tc358770_reg_write(dsi2edp, TC358770_DP_CTRL , 0x43);

	tc358770_reg_write(dsi2edp, TC358770_SYSTEM_CTRL , 0x01);

	msleep(70);
	dsi2edp->dsi2edp_enabled = true;

timeout:
	mutex_unlock(&dsi2edp->lock);
}

static void tc358770_dsi2edp_disable(struct tegra_dc_dsi_data *dsi)
{
	struct tegra_dc_dsi2edp_data *dsi2edp = tegra_dsi_get_outdata(dsi);

	dsi2edp->dsi2edp_enabled = false;
}

#ifdef CONFIG_PM
static void tc358770_dsi2edp_suspend(struct tegra_dc_dsi_data *dsi)
{
	/* To be done */
}

static void tc358770_dsi2edp_resume(struct tegra_dc_dsi_data *dsi)
{
	/* To be done */
}
#endif

struct tegra_dsi_out_ops tegra_dsi2edp_ops = {
	.init = tc358770_dsi2edp_init,
	.destroy = tc358770_dsi2edp_destroy,
	.enable = tc358770_dsi2edp_enable,
	.disable = tc358770_dsi2edp_disable,
#ifdef CONFIG_PM
	.suspend = tc358770_dsi2edp_suspend,
	.resume = tc358770_dsi2edp_resume,
#endif
};

static int tc358770_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	tc358770_i2c_client = client;

	return 0;
}

static int tc358770_i2c_remove(struct i2c_client *client)
{
	tc358770_i2c_client = NULL;

	return 0;
}

static const struct i2c_device_id tc358770_id_table[] = {
	{"tc358770_dsi2edp", 0},
	{},
};

static struct i2c_driver tc358770_i2c_drv = {
	.driver = {
		.name = "tc358770_dsi2edp",
		.owner = THIS_MODULE,
	},
	.probe = tc358770_i2c_probe,
	.remove = tc358770_i2c_remove,
	.id_table = tc358770_id_table,
};

static int __init tc358770_i2c_client_init(void)
{
	int err = 0;

	err = i2c_add_driver(&tc358770_i2c_drv);
	if (err)
		pr_err("tc358770_dsi2edp: Failed to add i2c client driver\n");

	return err;
}

static void __exit tc358770_i2c_client_exit(void)
{
	i2c_del_driver(&tc358770_i2c_drv);
}

subsys_initcall(tc358770_i2c_client_init);
module_exit(tc358770_i2c_client_exit);

MODULE_LICENSE("GPL");
