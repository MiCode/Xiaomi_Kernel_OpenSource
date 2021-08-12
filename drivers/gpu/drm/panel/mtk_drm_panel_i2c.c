// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_drm_gateic.h"

/* i2c control start */
#define MTK_PANEL_I2C_ID_NAME "I2C_LCD_BIAS"
static struct i2c_client *mtk_panel_i2c_client;

struct mtk_panel_i2c_dev {
	struct i2c_client *client;
};

int mtk_panel_i2c_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = mtk_panel_i2c_client;
	char write_data[2] = { 0 };

	if (client == NULL) {
		DDPPR_ERR("%s: ERROR!! client is null\n", __func__);
		return 0;
	}

	/*DDPMSG("%s: name=%s addr=0x%x, write:0x%x, value:0x%x\n", __func__, client->name,
	 *	 client->addr, addr, value);
	 */
	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		DDPPR_ERR("%s: ERROR write 0x%x with 0x%x fail %d\n",
			__func__, addr, value, ret);

	return ret;
}
EXPORT_SYMBOL(mtk_panel_i2c_write_bytes);

int mtk_panel_i2c_read_bytes(unsigned char addr, unsigned char *returnData)
{
	char cmd_buf[2] = { 0x00, 0x00 };
	int ret = 0;
	struct i2c_client *client = mtk_panel_i2c_client;

	if (client == NULL) {
		DDPPR_ERR("%s: ERROR!! mtk_panel_i2c_client is null\n", __func__);
		return 0;
	}

	cmd_buf[0] = addr;
	ret = i2c_master_send(client, &cmd_buf[0], 1);
	ret = i2c_master_recv(client, &cmd_buf[1], 1);
	if (ret < 0)
		DDPPR_ERR("%s: ERROR read 0x%x fail %d\n",
			__func__, addr, ret);

	*returnData = cmd_buf[1];

	return ret;
}
EXPORT_SYMBOL(mtk_panel_i2c_read_bytes);

int mtk_panel_i2c_write_multiple_bytes(unsigned char addr, unsigned char *value,
		unsigned int size)
{
	int ret = 0, i = 0;
	struct i2c_client *client = mtk_panel_i2c_client;
	char *write_data = NULL;

	if (client == NULL) {
		DDPPR_ERR("%s: ERROR!! mtk_panel_i2c_client is null\n", __func__);
		return 0;
	}

	if (IS_ERR_OR_NULL(value) || size == 0) {
		DDPPR_ERR("%s: ERROR!! value is null, size:%u\n", __func__, size);
		return -EINVAL;
	}
	write_data = kzalloc(roundup(size + 1, 4), GFP_KERNEL);
	if (IS_ERR_OR_NULL(write_data)) {
		DDPPR_ERR("%s: ERROR!! allocate buffer, size:%u\n", __func__, size);
		return -ENOMEM;
	}

	write_data[0] = addr;
	for (i = 0; i < size; i++)
		write_data[i + 1] = value[i];

	ret = i2c_master_send(client, write_data, size + 1);
	if (ret < 0)
		DDPPR_ERR("%s: ERROR i2c write 0x%x fail %d\n", __func__, ret, addr);

	kfree(write_data);
	write_data = NULL;
	return ret;
}
EXPORT_SYMBOL(mtk_panel_i2c_write_multiple_bytes);

static int mtk_panel_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	DDPMSG("%s: name=%s addr=0x%x\n", __func__, client->name,
		 client->addr);
	mtk_panel_i2c_client = client;
	return 0;
}

static int mtk_panel_i2c_remove(struct i2c_client *client)
{
	DDPMSG("%s: name=%s addr=0x%x\n", __func__, client->name,
		 client->addr);
	mtk_panel_i2c_client = NULL;
	i2c_unregister_device(client);
	return 0;
}

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

MODULE_AUTHOR("Cui Zhang <cui.zhang@mediatek.com>");
MODULE_DESCRIPTION("mediatek, panel i2c driver");
MODULE_LICENSE("GPL v2");
