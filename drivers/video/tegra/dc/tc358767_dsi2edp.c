/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/swab.h>
#include <linux/module.h>

#include <mach/dc.h>

#include "dc_priv.h"
#include "tc358767_dsi2edp.h"
#include "dsi.h"

static struct tegra_dsi2edp *dsi2edp;
static struct i2c_client *dsi2edp_i2c_client;

enum i2c_transfer_type {
	I2C_WRITE,
	I2C_READ,
};

static inline void dsi2edp_reg_write(struct tegra_dsi2edp *dsi2edp,
					unsigned int addr,
					unsigned int val)
{
	BUG_ON(!dsi2edp);

	if (dsi2edp->i2c_shutdown) {
		pr_err("dsi2edp: i2c shutdown\n");
		return;
	}

	/*
	 * TC358767 requires register address in big endian
	 * and register value in little endian.
	 * Regmap currently sends it all in big endian.
	 */
	regmap_write(dsi2edp->regmap, addr, __swab32(val));
	msleep(50);
}

static inline void dsi2edp_reg_read(struct tegra_dsi2edp *dsi2edp,
					unsigned int addr,
					unsigned int *val)
{
	BUG_ON(!dsi2edp);

	if (dsi2edp->i2c_shutdown) {
		pr_err("dsi2edp: i2c shutdown\n");
		return;
	}

	regmap_read(dsi2edp->regmap, addr, val);

	/*
	 * TC358767 returns register value in little endian.
	 * Covert it to big endian.
	 */
	*val = __swab32(*val);
}

static const struct regmap_config dsi2edp_regmap_config = {
	.reg_bits = 16,
	.val_bits = 32,
	.max_register = 0xa00,
	.reg_stride = 4,
};

static int dsi2edp_init(struct tegra_dc_dsi_data *dsi)
{
	int err = 0;

	if (dsi2edp) {
		tegra_dsi_set_outdata(dsi, dsi2edp);
		return err;
	}

	dsi2edp = devm_kzalloc(&dsi->dc->ndev->dev,
				sizeof(*dsi2edp),
				GFP_KERNEL);
	if (!dsi2edp)
		return -ENOMEM;

	dsi2edp->dsi = dsi;

	dsi2edp->client_i2c = dsi2edp_i2c_client;

	dsi2edp->regmap = devm_regmap_init_i2c(dsi2edp_i2c_client,
						&dsi2edp_regmap_config);
	if (IS_ERR(dsi2edp->regmap)) {
		err = PTR_ERR(dsi2edp->regmap);
		dev_err(&dsi->dc->ndev->dev,
				"tc358767_dsi2edp: regmap init failed\n");
		goto fail;
	}

	dsi2edp->mode = &dsi->dc->mode;

	tegra_dsi_set_outdata(dsi, dsi2edp);

	mutex_init(&dsi2edp->lock);
fail:
	return err;
}

static const struct regmap_config dsi2edp_bl_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};
static void dsi2edp_enable(struct tegra_dc_dsi_data *dsi)
{
	struct tegra_dsi2edp *dsi2edp = tegra_dsi_get_outdata(dsi);
	unsigned int val;

	if (dsi2edp && dsi2edp->enabled)
		return;

	mutex_lock(&dsi2edp->lock);

	dsi2edp_reg_write(dsi2edp, DP0_SRC_CTRL, 0x3086);
	dsi2edp_reg_write(dsi2edp, DP1_SRC_CTRL, 0x2);
	dsi2edp_reg_write(dsi2edp, SYS_PLL_PARAM, 0x0101);
	dsi2edp_reg_write(dsi2edp, DP_PHY_CTRL, 0x3000007);
	dsi2edp_reg_write(dsi2edp, DP1_PLL_CTRL, 0x5);
	dsi2edp_reg_write(dsi2edp, DP0_PLL_CTRL, 0x5);
	msleep(70);
	dsi2edp_reg_write(dsi2edp, PXL_PLL_PARAM, 0x22011f);
	dsi2edp_reg_write(dsi2edp, PXL_PLL_CTRL, 0x5);
	msleep(70);

	/* reset main channel 0 and 1*/
	dsi2edp_reg_write(dsi2edp, DP_PHY_CTRL, 0x13001107);
	msleep(100);
	dsi2edp_reg_write(dsi2edp, DP_PHY_CTRL, 0x3000007);
	msleep(100);

	dsi2edp_reg_read(dsi2edp, 0x800, &val);
	if (!(val & (1<<16))) {
		dev_err(&dsi->dc->ndev->dev,
			"TC358767: main link not ready\n");
	}

	dsi2edp_reg_write(dsi2edp, DP0_AUX_ADDR, 0x1);
	msleep(100);
	dsi2edp_reg_write(dsi2edp, DP0_AUX_CFG1, 0x1063f);
	msleep(100);
	dsi2edp_reg_write(dsi2edp, DP0_AUX_ADDR, 0x1);
	msleep(100);
	dsi2edp_reg_write(dsi2edp, DP0_AUX_CFG0, 0x9);
	msleep(100);

	dsi2edp_reg_write(dsi2edp, DP0_AUX_ADDR, 0x2);
	dsi2edp_reg_write(dsi2edp, DP0_AUX_CFG0, 0x9);

	dsi2edp_reg_write(dsi2edp, DP0_AUX_ADDR, 0x100);
	dsi2edp_reg_write(dsi2edp, DP0_AUX_WDATA0, 0x20a);
	dsi2edp_reg_write(dsi2edp, DP0_AUX_CFG0, 0x108);
	dsi2edp_reg_write(dsi2edp, DP0_AUX_ADDR, 0x108);
	dsi2edp_reg_write(dsi2edp, DP0_AUX_WDATA0, 0x1);
	dsi2edp_reg_write(dsi2edp, DP0_AUX_CFG0, 0x8);
	dsi2edp_reg_write(dsi2edp, DP0_SNK_LTCTRL, 0x21);
	dsi2edp_reg_write(dsi2edp, DP0_LTLOOP_CTRL, 0xf600000d);
	dsi2edp_reg_write(dsi2edp, DP0_SRC_CTRL, 0x3187);
	dsi2edp_reg_write(dsi2edp, DP0_CTRL, 0x1);

	dsi2edp_reg_write(dsi2edp, DP0_SNK_LTCTRL, 0x22);
	dsi2edp_reg_write(dsi2edp, DP0_SRC_CTRL, 0x3287);

	dsi2edp_reg_write(dsi2edp, DP0_AUX_ADDR, 0x102);
	dsi2edp_reg_write(dsi2edp, DP0_AUX_WDATA0, 0x0);
	dsi2edp_reg_write(dsi2edp, DP0_AUX_CFG0, 0x8);
	dsi2edp_reg_write(dsi2edp, DP0_SRC_CTRL, 0x1087);
	dsi2edp_reg_write(dsi2edp, DP0_AUX_ADDR, 0x200);
	dsi2edp_reg_write(dsi2edp, DP0_AUX_CFG0, 0x409);

	dsi2edp_reg_write(dsi2edp, PPI_TX_RX_TA, 0x040004);
	dsi2edp_reg_write(dsi2edp, PPI_LPTX_TIME_CNT, 0x4);
	dsi2edp_reg_write(dsi2edp, PPI_D0S_CLRSIPOCOUNT, 0x7);
	dsi2edp_reg_write(dsi2edp, PPI_D1S_CLRSIPOCOUNT, 0x7);
	dsi2edp_reg_write(dsi2edp, PPI_D2S_CLRSIPOCOUNT, 0x7);
	dsi2edp_reg_write(dsi2edp, PPI_D3S_CLRSIPOCOUNT, 0x7);
	dsi2edp_reg_write(dsi2edp, PPI_LANE_ENABLE, 0x1f);
	dsi2edp_reg_write(dsi2edp, DSI_LANE_ENABLE, 0x1f);
	dsi2edp_reg_write(dsi2edp, PPI_STARTPPI, 0x1);
	dsi2edp_reg_write(dsi2edp, DSI_STARTDSI, 0x1);

	dsi2edp_reg_write(dsi2edp, VIDEO_PATH0_CTRL, 0x3000101);
	dsi2edp_reg_write(dsi2edp, H_TIMING_CTRL01, 0x500020);
	dsi2edp_reg_write(dsi2edp, H_TIMING_CTRL02, 0x300780);
	dsi2edp_reg_write(dsi2edp, V_TIMING_CTRL01, 0x170005);
	dsi2edp_reg_write(dsi2edp, V_TIMING_CTRL02, 0x30438);
	dsi2edp_reg_write(dsi2edp, VIDEO_FRAME_TIMING_UPLOAD_ENB0, 0x1);
	dsi2edp_reg_write(dsi2edp, DP0_VIDEO_SYNC_DELAY, 0x3e07f0);
	dsi2edp_reg_write(dsi2edp, DP0_TOT_VAL, 0x4570820);
	dsi2edp_reg_write(dsi2edp, DP0_START_VAL, 0x1c0070);
	dsi2edp_reg_write(dsi2edp, DP0_ACTIVE_VAL, 0x4380780);
	dsi2edp_reg_write(dsi2edp, DP0_SYNC_VAL, 0x80058020);
	dsi2edp_reg_write(dsi2edp, DP0_MISC, 0x1f3f0020);
	dsi2edp_reg_write(dsi2edp, DP0_AUX_ADDR, 0x202);
	dsi2edp_reg_write(dsi2edp, DP0_AUX_CFG0, 0x9);
	dsi2edp_reg_write(dsi2edp, VIDEO_MN_GEN0, 0x36);
	dsi2edp_reg_write(dsi2edp, VIDEO_MN_GEN1, 0x2a30);
	dsi2edp_reg_write(dsi2edp, DP0_CTRL, 0x41);
	dsi2edp_reg_write(dsi2edp, DP0_CTRL, 0x43);
	dsi2edp_reg_write(dsi2edp, SYS_CTRL, 0x1);
	msleep(100);

	dsi2edp->enabled = true;
	mutex_unlock(&dsi2edp->lock);
}

static void dsi2edp_destroy(struct tegra_dc_dsi_data *dsi)
{
	/* TODO */
}

static void dsi2edp_disable(struct tegra_dc_dsi_data *dsi)
{
	dsi2edp->enabled = false;
}

#ifdef CONFIG_PM
static void dsi2edp_suspend(struct tegra_dc_dsi_data *dsi)
{
	/* TODO */
}

static void dsi2edp_resume(struct tegra_dc_dsi_data *dsi)
{
	/* TODO */
}
#endif

struct tegra_dsi_out_ops tegra_dsi2edp_ops = {
	.init = dsi2edp_init,
	.destroy = dsi2edp_destroy,
	.enable = dsi2edp_enable,
	.disable = dsi2edp_disable,
#ifdef CONFIG_PM
	.suspend = dsi2edp_suspend,
	.resume = dsi2edp_resume,
#endif
};

static int dsi2edp_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	dsi2edp_i2c_client = client;

	if (dsi2edp)
		dsi2edp->i2c_shutdown = false;

	return 0;
}

static int dsi2edp_i2c_remove(struct i2c_client *client)
{
	dsi2edp_i2c_client = NULL;

	return 0;
}

static void dsi2edp_i2c_shutdown(struct i2c_client *client)
{
	if (dsi2edp)
		dsi2edp->i2c_shutdown = true;
}

static const struct i2c_device_id dsi2edp_id_table[] = {
	{"tc358767_dsi2edp", 0},
	{},
};

static struct i2c_driver dsi2edp_i2c_drv = {
	.driver = {
		.name = "tc358767_dsi2edp",
		.owner = THIS_MODULE,
	},
	.probe = dsi2edp_i2c_probe,
	.remove = dsi2edp_i2c_remove,
	.shutdown = dsi2edp_i2c_shutdown,
	.id_table = dsi2edp_id_table,
};

static int __init dsi2edp_i2c_client_init(void)
{
	int err = 0;

	err = i2c_add_driver(&dsi2edp_i2c_drv);
	if (err)
		pr_err("tc358767_dsi2edp: Failed to add i2c client driver\n");

	return err;
}

static void __exit dsi2edp_i2c_client_exit(void)
{
	i2c_del_driver(&dsi2edp_i2c_drv);
}

subsys_initcall(dsi2edp_i2c_client_init);
module_exit(dsi2edp_i2c_client_exit);

MODULE_LICENSE("GPL");
