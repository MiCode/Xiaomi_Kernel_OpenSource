/*
 * drivers/video/tegra/dc/sn65dsi86_dsi2edp.c
 *
 * Copyright (c) 2013, NVIDIA Corporation.
 *
 * Author:
 *	Bibek Basu <bbasu@nvidia.com>
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
#include "sn65dsi86_dsi2edp.h"
#include "dsi.h"

static struct tegra_dc_dsi2edp_data *sn65dsi86_dsi2edp;
static struct i2c_client *sn65dsi86_i2c_client;

enum i2c_transfer_type {
	I2C_WRITE,
	I2C_READ,
};

static inline int sn65dsi86_reg_write(struct tegra_dc_dsi2edp_data *dsi2edp,
					unsigned int addr, unsigned int val)
{
	return regmap_write(dsi2edp->regmap, addr, val);
}

static inline void sn65dsi86_reg_read(struct tegra_dc_dsi2edp_data *dsi2edp,
					unsigned int addr, unsigned int *val)
{
	regmap_read(dsi2edp->regmap, addr, val);
}

static const struct regmap_config sn65dsi86_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int sn65dsi86_dsi2edp_init(struct tegra_dc_dsi_data *dsi)
{
	int err = 0;

	if (sn65dsi86_dsi2edp) {
		tegra_dsi_set_outdata(dsi, sn65dsi86_dsi2edp);
		return err;
	}

	sn65dsi86_dsi2edp = devm_kzalloc(&dsi->dc->ndev->dev,
					sizeof(*sn65dsi86_dsi2edp),
					GFP_KERNEL);
	if (!sn65dsi86_dsi2edp)
		return -ENOMEM;

	sn65dsi86_dsi2edp->dsi = dsi;

	sn65dsi86_dsi2edp->client_i2c = sn65dsi86_i2c_client;

	sn65dsi86_dsi2edp->regmap = devm_regmap_init_i2c(sn65dsi86_i2c_client,
						&sn65dsi86_regmap_config);
	if (IS_ERR(sn65dsi86_dsi2edp->regmap)) {
		err = PTR_ERR(sn65dsi86_dsi2edp->regmap);
		dev_err(&dsi->dc->ndev->dev,
				"sn65dsi86_dsi2edp: regmap init failed\n");
		goto fail;
	}

	sn65dsi86_dsi2edp->mode = &dsi->dc->mode;

	tegra_dsi_set_outdata(dsi, sn65dsi86_dsi2edp);

	mutex_init(&sn65dsi86_dsi2edp->lock);
fail:
	return err;
}

static void sn65dsi86_dsi2edp_destroy(struct tegra_dc_dsi_data *dsi)
{
	struct tegra_dc_dsi2edp_data *dsi2edp =
				tegra_dsi_get_outdata(dsi);

	if (!dsi2edp)
		return;

	sn65dsi86_dsi2edp = NULL;
	mutex_destroy(&dsi2edp->lock);
}
static void sn65dsi86_dsi2edp_enable(struct tegra_dc_dsi_data *dsi)
{
	struct tegra_dc_dsi2edp_data *dsi2edp = tegra_dsi_get_outdata(dsi);
	unsigned val = 0;

	if (dsi2edp && dsi2edp->dsi2edp_enabled)
		return;
	mutex_lock(&dsi2edp->lock);
	/* REFCLK 19.2MHz */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_PLL_REFCLK_CFG, 0x02);
	usleep_range(10000, 12000);
	/* Single 4 DSI lanes */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_DSI_CFG1, 0x26);
	usleep_range(10000, 12000);
	/* DSI CLK FREQ 422.5MHz */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_DSI_CHA_CLK_RANGE, 0x55);
	usleep_range(10000, 12000);
	sn65dsi86_reg_read(dsi2edp, SN65DSI86_DSI_CHA_CLK_RANGE, &val);
	sn65dsi86_reg_read(dsi2edp, SN65DSI86_DSI_CHA_CLK_RANGE, &val);
	/* enhanced framing */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_FRAMING_CFG, 0x04);
	usleep_range(10000, 12000);
	/* Pre0dB 2 lanes no SSC */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_DP_SSC_CFG, 0x20);
	usleep_range(10000, 12000);
	/* L0mV HBR */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_DP_CFG, 0x80);
	usleep_range(10000, 12000);
	/* PLL ENABLE */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_PLL_EN, 0x01);
	usleep_range(10000, 12000);
	/* DP_PLL_LOCK */
	sn65dsi86_reg_read(dsi2edp, SN65DSI86_PLL_REFCLK_CFG, &val);
	usleep_range(10000, 12000);
	/* POST2 0dB */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_TRAINING_CFG, 0x00);
	usleep_range(10000, 12000);
	/* Semi-Auto TRAIN */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_ML_TX_MODE, 0x0a);
	usleep_range(10000, 12000);
	/* ADDR 0x96 CFR */
	sn65dsi86_reg_read(dsi2edp, SN65DSI86_ML_TX_MODE, &val);
	msleep(20);
	/* CHA_ACTIVE_LINE_LENGTH */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_VIDEO_CHA_LINE_LOW, 0x80);
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_VIDEO_CHA_LINE_HIGH, 0x07);
	usleep_range(10000, 12000);
	/* CHA_VERTICAL_DISPLAY_SIZE */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_CHA_VERT_DISP_SIZE_LOW, 0x38);
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_CHA_VERT_DISP_SIZE_HIGH, 0x04);
	usleep_range(10000, 12000);
	/* CHA_HSYNC_PULSE_WIDTH */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_CHA_HSYNC_PULSE_WIDTH_LOW, 0x10);
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_CHA_HSYNC_PULSE_WIDTH_HIGH,
			0x80);
	usleep_range(10000, 12000);
	/* CHA_VSYNC_PULSE_WIDTH */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_CHA_VSYNC_PULSE_WIDTH_LOW, 0x0e);
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_CHA_VSYNC_PULSE_WIDTH_HIGH,
			0x80);
	usleep_range(10000, 12000);
	/* CHA_HORIZONTAL_BACK_PORCH */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_CHA_HORIZONTAL_BACK_PORCH, 0x98);
	usleep_range(10000, 12000);
	/* CHA_VERTICAL_BACK_PORCH */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_CHA_VERTICAL_BACK_PORCH, 0x13);
	usleep_range(10000, 12000);
	/* CHA_HORIZONTAL_FRONT_PORCH */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_CHA_HORIZONTAL_FRONT_PORCH,
			0x10);
	usleep_range(10000, 12000);
	/* CHA_VERTICAL_FRONT_PORCH */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_CHA_VERTICAL_FRONT_PORCH, 0x03);
	usleep_range(10000, 12000);
	/* DP-18BPP Enable */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_DP_18BPP_EN, 0x00);
	msleep(100);
	/* COLOR BAR */
	/* sn65dsi86_reg_write(dsi2edp, SN65DSI86_COLOR_BAR_CFG, 0x10);*/
	/* enhanced framing and Vstream enable */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_FRAMING_CFG, 0x0c);
	dsi2edp->dsi2edp_enabled = true;
	mutex_unlock(&dsi2edp->lock);
}

static void sn65dsi86_dsi2edp_disable(struct tegra_dc_dsi_data *dsi)
{
	struct tegra_dc_dsi2edp_data *dsi2edp = tegra_dsi_get_outdata(dsi);

	mutex_lock(&dsi2edp->lock);
	/* enhanced framing and Vstream disable */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_FRAMING_CFG, 0x04);
	dsi2edp->dsi2edp_enabled = false;
	mutex_unlock(&dsi2edp->lock);

}

#ifdef CONFIG_PM
static void sn65dsi86_dsi2edp_suspend(struct tegra_dc_dsi_data *dsi)
{
	struct tegra_dc_dsi2edp_data *dsi2edp = tegra_dsi_get_outdata(dsi);

	mutex_lock(&dsi2edp->lock);
	/* configure GPIO1 for suspend */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_GPIO_CTRL_CFG, 0x02);
	dsi2edp->dsi2edp_enabled = false;
	mutex_unlock(&dsi2edp->lock);

}

static void sn65dsi86_dsi2edp_resume(struct tegra_dc_dsi_data *dsi)
{
	struct tegra_dc_dsi2edp_data *dsi2edp = tegra_dsi_get_outdata(dsi);

	mutex_lock(&dsi2edp->lock);
	/* disable configure GPIO1 for suspend */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_GPIO_CTRL_CFG, 0x00);
	/* enhanced framing and Vstream enable */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_FRAMING_CFG, 0x0c);
	dsi2edp->dsi2edp_enabled = true;
	mutex_unlock(&dsi2edp->lock);
}
#endif

struct tegra_dsi_out_ops tegra_dsi2edp_ops = {
	.init = sn65dsi86_dsi2edp_init,
	.destroy = sn65dsi86_dsi2edp_destroy,
	.enable = sn65dsi86_dsi2edp_enable,
	.disable = sn65dsi86_dsi2edp_disable,
#ifdef CONFIG_PM
	.suspend = sn65dsi86_dsi2edp_suspend,
	.resume = sn65dsi86_dsi2edp_resume,
#endif
};

static int sn65dsi86_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	sn65dsi86_i2c_client = client;

	return 0;
}

static int sn65dsi86_i2c_remove(struct i2c_client *client)
{
	sn65dsi86_i2c_client = NULL;

	return 0;
}

static const struct i2c_device_id sn65dsi86_id_table[] = {
	{"sn65dsi86_dsi2edp", 0},
	{},
};

static struct i2c_driver sn65dsi86_i2c_drv = {
	.driver = {
		.name = "sn65dsi86_dsi2edp",
		.owner = THIS_MODULE,
	},
	.probe = sn65dsi86_i2c_probe,
	.remove = sn65dsi86_i2c_remove,
	.id_table = sn65dsi86_id_table,
};

static int __init sn65dsi86_i2c_client_init(void)
{
	int err = 0;

	err = i2c_add_driver(&sn65dsi86_i2c_drv);
	if (err)
		pr_err("sn65dsi86_dsi2edp: Failed to add i2c client driver\n");

	return err;
}

static void __exit sn65dsi86_i2c_client_exit(void)
{
	i2c_del_driver(&sn65dsi86_i2c_drv);
}

subsys_initcall(sn65dsi86_i2c_client_init);
module_exit(sn65dsi86_i2c_client_exit);

MODULE_AUTHOR("Bibek Basu <bbasu@nvidia.com>");
MODULE_DESCRIPTION(" TI SN65DSI86 dsi bridge to edp");
MODULE_LICENSE("GPL");
