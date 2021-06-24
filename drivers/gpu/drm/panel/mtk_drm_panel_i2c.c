// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_drm_gateic.h"

/* i2c control start */
#define MTK_PANEL_I2C_ID_NAME "MTK_I2C_LCD_BIAS"
static struct i2c_client *mtk_panel_i2c_client;

/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int mtk_panel_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id);
static int mtk_panel_i2c_remove(struct i2c_client *client);

/*****************************************************************************
 * Data Structure
 *****************************************************************************/
struct mtk_panel_i2c_dev {
	struct i2c_client *client;
};

static const struct of_device_id mtk_panel_i2c_of_match[] = {
	{.compatible = "mediatek,i2c_lcd_bias"},
	{},
};

static const struct i2c_device_id mtk_panel_i2c_id[] = {
	{MTK_PANEL_I2C_ID_NAME, 0},
	{},
};

struct i2c_driver mtk_panel_i2c_driver = {
	.id_table = mtk_panel_i2c_id,
	.probe = mtk_panel_i2c_probe,
	.remove = mtk_panel_i2c_remove,
	/* .detect		   = mtk_panel_i2c_detect, */
	.driver = {
		.owner = THIS_MODULE,
		.name = MTK_PANEL_I2C_ID_NAME,
		.of_match_table = mtk_panel_i2c_of_match,
	},
};

/*****************************************************************************
 * Function
 *****************************************************************************/

static int mtk_panel_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	DDPMSG("[LCM][I2C] %s\n", __func__);
	DDPMSG("[LCM][I2C] NT: info==>name=%s addr=0x%x\n", client->name,
		 client->addr);
	mtk_panel_i2c_client = client;
	return 0;
}

static int mtk_panel_i2c_remove(struct i2c_client *client)
{
	DDPMSG("[LCM][I2C] %s\n", __func__);
	mtk_panel_i2c_client = NULL;
	i2c_unregister_device(client);
	return 0;
}

int mtk_panel_i2c_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = mtk_panel_i2c_client;
	char write_data[2] = { 0 };

	if (client == NULL) {
		DDPPR_ERR("ERROR!! mtk_panel_i2c_client is null\n");
		return 0;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		DDPPR_ERR("[LCM][ERROR] _lcm_i2c write data fail !!\n");

	return ret;
}
EXPORT_SYMBOL(mtk_panel_i2c_write_bytes);

MODULE_AUTHOR("Cui Zhang <cui.zhang@mediatek.com>");
MODULE_DESCRIPTION("mediatek, panel i2c driver");
MODULE_LICENSE("GPL v2");
