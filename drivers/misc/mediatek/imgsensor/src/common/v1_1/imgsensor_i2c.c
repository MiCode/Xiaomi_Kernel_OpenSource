/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "imgsensor_common.h"
#include "imgsensor_i2c.h"
#include <linux/ratelimit.h>

struct IMGSENSOR_I2C gi2c;

static const struct i2c_device_id gi2c_dev_id[] = {
	{IMGSENSOR_I2C_DRV_NAME_0, 0},
	{IMGSENSOR_I2C_DRV_NAME_1, 0},
	{IMGSENSOR_I2C_DRV_NAME_2, 0},
	{}
};

#ifdef CONFIG_OF
static const struct of_device_id gof_device_id_0[] = {
	{.compatible = IMGSENSOR_I2C_OF_DRV_NAME_0,},
	{}
};

static const struct of_device_id gof_device_id_1[] = {
	{.compatible = IMGSENSOR_I2C_OF_DRV_NAME_1,},
	{}
};

static const struct of_device_id gof_device_id_2[] = {
	{.compatible = IMGSENSOR_I2C_OF_DRV_NAME_2,},
	{}
};
#endif

static int imgsensor_i2c_probe_0(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	gi2c.inst[IMGSENSOR_I2C_DEV_0].pi2c_client = client;
	return 0;
}

static int imgsensor_i2c_probe_1(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	gi2c.inst[IMGSENSOR_I2C_DEV_1].pi2c_client = client;
	return 0;
}

static int imgsensor_i2c_probe_2(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	gi2c.inst[IMGSENSOR_I2C_DEV_2].pi2c_client = client;
	return 0;
}

static int imgsensor_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static struct i2c_driver gi2c_driver[IMGSENSOR_I2C_DEV_MAX_NUM] = {
	{
		.probe = imgsensor_i2c_probe_0,
		.remove = imgsensor_i2c_remove,
		.driver = {
		.name = IMGSENSOR_I2C_DRV_NAME_0,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = gof_device_id_0,
#endif
		},
		.id_table = gi2c_dev_id,
	},
	{
		.probe = imgsensor_i2c_probe_1,
		.remove = imgsensor_i2c_remove,
		.driver = {
		.name = IMGSENSOR_I2C_DRV_NAME_1,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = gof_device_id_1,
#endif
		},
		.id_table = gi2c_dev_id,
	},
	{
		.probe = imgsensor_i2c_probe_2,
		.remove = imgsensor_i2c_remove,
		.driver = {
		.name = IMGSENSOR_I2C_DRV_NAME_2,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = gof_device_id_2,
#endif
		},
		.id_table = gi2c_dev_id,
	}
};

enum IMGSENSOR_RETURN imgsensor_i2c_create(void)
{
	int i;

	for (i = 0; i < IMGSENSOR_I2C_DEV_MAX_NUM; i++)
		i2c_add_driver(&gi2c_driver[i]);

	return IMGSENSOR_RETURN_SUCCESS;
}

enum IMGSENSOR_RETURN imgsensor_i2c_delete(void)
{
	int i;

	for (i = 0; i < IMGSENSOR_I2C_DEV_MAX_NUM; i++)
		i2c_del_driver(&gi2c_driver[i]);

	return IMGSENSOR_RETURN_SUCCESS;
}

enum IMGSENSOR_RETURN imgsensor_i2c_init(
		struct IMGSENSOR_I2C_CFG *pi2c_cfg,
		enum IMGSENSOR_I2C_DEV device)
{
	if (!pi2c_cfg ||
			device >= IMGSENSOR_I2C_DEV_MAX_NUM ||
			device < IMGSENSOR_I2C_DEV_0)
		return IMGSENSOR_RETURN_ERROR;

	pi2c_cfg->pinst       = &gi2c.inst[device];
	pi2c_cfg->pi2c_driver = &gi2c_driver[device];

	mutex_init(&pi2c_cfg->i2c_mutex);

	return IMGSENSOR_RETURN_SUCCESS;
}

enum IMGSENSOR_RETURN imgsensor_i2c_buffer_mode(int enable)
{
	struct IMGSENSOR_I2C_INST *pinst =
		&gi2c.inst[IMGSENSOR_I2C_BUFF_MODE_DEV];
	enum   IMGSENSOR_RETURN    ret   = IMGSENSOR_RETURN_SUCCESS;

	PK_DBG("i2c_buf_mode_en %d\n", enable);

	if (pinst->pi2c_client == NULL) {
		PK_PR_ERR("pi2c_client is NULL!\n");
		return IMGSENSOR_RETURN_ERROR;
	}

	ret = (enable) ?
		hw_trig_i2c_enable(pinst->pi2c_client->adapter) :
		hw_trig_i2c_disable(pinst->pi2c_client->adapter);

	return ret;
}

enum IMGSENSOR_RETURN imgsensor_i2c_read(
		struct IMGSENSOR_I2C_CFG *pi2c_cfg,
		u8 *pwrite_data,
		u16 write_length,
		u8 *pread_data,
		u16 read_length,
		u16 id,
		int speed)
{
	struct IMGSENSOR_I2C_INST *pinst = pi2c_cfg->pinst;
	enum   IMGSENSOR_RETURN    ret   = IMGSENSOR_RETURN_SUCCESS;

	if (pinst->pi2c_client == NULL) {
		PK_PR_ERR("pi2c_client is NULL!\n");
		return IMGSENSOR_RETURN_ERROR;
	}

	mutex_lock(&pi2c_cfg->i2c_mutex);

	pinst->msg[0].addr  = id >> 1;
	pinst->msg[0].flags = 0;
	pinst->msg[0].len   = write_length;
	pinst->msg[0].buf   = pwrite_data;

	pinst->msg[1].addr  = id >> 1;
	pinst->msg[1].flags = I2C_M_RD;
	pinst->msg[1].len   = read_length;
	pinst->msg[1].buf   = pread_data;

	if (mtk_i2c_transfer(
			pinst->pi2c_client->adapter,
			pinst->msg,
			IMGSENSOR_I2C_MSG_SIZE_READ,
			(pi2c_cfg->pinst->status.filter_msg)
				? I2C_A_FILTER_MSG : 0,
			((speed > 0) && (speed <= 1000))
				? speed * 1000 : IMGSENSOR_I2C_SPEED * 1000)
			!= IMGSENSOR_I2C_MSG_SIZE_READ) {
		static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 30);

		if (__ratelimit(&ratelimit))
			PK_PR_ERR(
			"I2C read failed (0x%x)! speed(0=%d) (0x%x)\n",
			ret, speed, *pwrite_data);
		ret = IMGSENSOR_RETURN_ERROR;
	}

	mutex_unlock(&pi2c_cfg->i2c_mutex);

	return ret;
}

enum IMGSENSOR_RETURN imgsensor_i2c_write(
		struct IMGSENSOR_I2C_CFG *pi2c_cfg,
		u8 *pwrite_data,
		u16 write_length,
		u16 write_per_cycle,
		u16 id,
		int speed)
{
	struct IMGSENSOR_I2C_INST *pinst = pi2c_cfg->pinst;
	enum   IMGSENSOR_RETURN    ret   = IMGSENSOR_RETURN_SUCCESS;
	struct i2c_msg     *pmsg  = pinst->msg;
	u8                 *pdata = pwrite_data;
	u8                 *pend  = pwrite_data + write_length;
	int i   = 0;

	if (pinst->pi2c_client == NULL) {
		PK_PR_ERR("pi2c_client is NULL!\n");
		return IMGSENSOR_RETURN_ERROR;
	}

	mutex_lock(&pi2c_cfg->i2c_mutex);

	while (pdata < pend && i < IMGSENSOR_I2C_CMD_LENGTH_MAX) {
		pmsg->addr  = id >> 1;
		pmsg->flags = 0;
		pmsg->len   = write_per_cycle;
		pmsg->buf   = pdata;

		i++;
		pmsg++;
		pdata += write_per_cycle;
	}

	if (mtk_i2c_transfer(
			pinst->pi2c_client->adapter,
			pinst->msg,
			i,
			(pi2c_cfg->pinst->status.filter_msg)
				? I2C_A_FILTER_MSG : 0,
			((speed > 0) && (speed <= 1000))
				? speed * 1000 : IMGSENSOR_I2C_SPEED * 1000)
			!= i) {
		static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 30);

		if (__ratelimit(&ratelimit))
			PK_PR_ERR(
				"I2C write failed (0x%x)! speed(0=%d) (0x%x)\n",
				ret, speed, *pwrite_data);
		ret = IMGSENSOR_RETURN_ERROR;
	}

	mutex_unlock(&pi2c_cfg->i2c_mutex);

	return ret;
}

void imgsensor_i2c_filter_msg(struct IMGSENSOR_I2C_CFG *pi2c_cfg, bool en)
{
	pi2c_cfg->pinst->status.filter_msg = en;
}

#ifdef IMGSENSOR_LEGACY_COMPAT
struct IMGSENSOR_I2C_CFG *pgi2c_cfg_legacy;
void imgsensor_i2c_set_device(struct IMGSENSOR_I2C_CFG *pi2c_cfg)
{
	pgi2c_cfg_legacy = pi2c_cfg;
}
#endif

