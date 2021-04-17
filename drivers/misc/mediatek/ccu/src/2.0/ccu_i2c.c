/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>

#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/sched.h>

#include <linux/printk.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/i2c.h>

#include "i2c-mtk.h"
#include <m4u.h>

#include "ccu_cmn.h"
#include "ccu_i2c.h"
#include "ccu_i2c_hw.h"
#include "ccu_mva.h"

/*****************************************************************************/
/*I2C Channel offset*/
#define I2C_BASE_OFS_CH1 (0x200)
#define MAX_I2C_CMD_LEN 255
#define CCU_I2C_APDMA_TXLEN 128

#define CCU_I2C_2_HW_DRVNAME  "ccu_i2c_2_hwtrg"
#define CCU_I2C_4_HW_DRVNAME  "ccu_i2c_4_hwtrg"
#define CCU_I2C_7_HW_DRVNAME  "ccu_i2c_7_hwtrg"

static DEFINE_MUTEX(ccu_i2c_mutex);

/*i2c driver hook*/
static int ccu_i2c_probe_2(struct i2c_client *client,
	const struct i2c_device_id *id);
static int ccu_i2c_probe_4(struct i2c_client *client,
	const struct i2c_device_id *id);
static int ccu_i2c_probe_7(struct i2c_client *client,
	const struct i2c_device_id *id);
static int ccu_i2c_remove(struct i2c_client *client);
/*ccu i2c operation*/
static struct i2c_client *get_ccu_i2c_client(
	uint32_t i2c_id);
static int ccu_i2c_controller_en(uint32_t i2c_id,
	int enable);
static int i2c_query_dma_buffer_addr(uint32_t sensor_idx,
	void **va, uint32_t *va_h, uint32_t *va_l, uint32_t *i2c_id);
static int ccu_i2c_controller_uninit(uint32_t i2c_id);

/*i2c reg operation*/
static inline void i2c_writew(u16 value, struct mt_i2c *i2c, u16 offset);


//static enum CCU_I2C_CHANNEL g_ccuI2cChannel;
static struct i2c_client *g_ccuI2cClient2;
static struct i2c_client *g_ccuI2cClient4;
static struct i2c_client *g_ccuI2cClient7;

static const struct i2c_device_id ccu_i2c_2_ids[] = {
	{CCU_I2C_2_HW_DRVNAME, 0}, {} };
static const struct i2c_device_id ccu_i2c_4_ids[] = {
	{CCU_I2C_4_HW_DRVNAME, 0}, {} };
static const struct i2c_device_id ccu_i2c_7_ids[] = {
	{CCU_I2C_7_HW_DRVNAME, 0}, {} };
uint32_t i2c_mva[IMGSENSOR_SENSOR_IDX_MAX_NUM];
static bool ccu_i2c_initialized[I2C_MAX_CHANNEL] = {0};

#ifdef CONFIG_OF
static const struct of_device_id ccu_i2c_2_driver_of_ids[] = {
	{.compatible = "mediatek,ccu_sensor_i2c_2_hw",},
	{}
};

static const struct of_device_id ccu_i2c_4_driver_of_ids[] = {
	{.compatible = "mediatek,ccu_sensor_i2c_4_hw",},
	{}
};

static const struct of_device_id ccu_i2c_7_driver_of_ids[] = {
	{.compatible = "mediatek,ccu_sensor_i2c_7_hw",},
	{}
};
#endif

struct i2c_driver ccu_i2c_2_driver = {
	.probe = ccu_i2c_probe_2,
	.remove = ccu_i2c_remove,
	.driver = {
		   .name = CCU_I2C_2_HW_DRVNAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = ccu_i2c_2_driver_of_ids,
#endif
		   },
	.id_table = ccu_i2c_2_ids,
};

struct i2c_driver ccu_i2c_4_driver = {
	.probe = ccu_i2c_probe_4,
	.remove = ccu_i2c_remove,
	.driver = {
		   .name = CCU_I2C_4_HW_DRVNAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = ccu_i2c_4_driver_of_ids,
#endif
		   },
	.id_table = ccu_i2c_4_ids,
};

struct i2c_driver ccu_i2c_7_driver = {
	.probe = ccu_i2c_probe_7,
	.remove = ccu_i2c_remove,
	.driver = {
		   .name = CCU_I2C_7_HW_DRVNAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = ccu_i2c_7_driver_of_ids,
#endif
		   },
	.id_table = ccu_i2c_7_ids,
};

/*---------------------------------------------------------------------------*/
/* CCU Driver: i2c driver funcs                                              */
/*---------------------------------------------------------------------------*/
static int ccu_i2c_probe_2(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	LOG_DBG_MUST(
	"[ccu_i2c_probe] Attach I2C for HW trriger g_ccuI2cClient2 %p\n",
	client);

	/* get sensor i2c client */
	g_ccuI2cClient2 = client;

	LOG_DBG_MUST("[ccu_i2c_probe] Attached!!\n");

	return 0;
}

static int ccu_i2c_probe_4(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	LOG_DBG_MUST(
	"[ccu_i2c_probe] Attach I2C for HW trriger g_ccuI2cClient4 %p\n",
	client);

	/* get sensor i2c client */
	g_ccuI2cClient4 = client;

	LOG_DBG_MUST("[ccu_i2c_probe] Attached!!\n");

	return 0;
}

static int ccu_i2c_probe_7(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	LOG_DBG_MUST(
	"[ccu_i2c_probe] Attach I2C for HW trriger g_ccuI2cClient7 %p\n",
	client);

	/* get sensor i2c client */
	g_ccuI2cClient7 = client;

	LOG_DBG_MUST("[ccu_i2c_probe] Attached!!\n");

	return 0;
}

static int ccu_i2c_remove(struct i2c_client *client)
{
	return 0;
}

/*---------------------------------------------------------------------------*/
/* CCU i2c public funcs                                                      */
/*---------------------------------------------------------------------------*/
int ccu_i2c_register_driver(void)
{
	int i2c_ret = 0;

	mutex_lock(&ccu_i2c_mutex);
	LOG_DBG_MUST("i2c_add_driver(&ccu_i2c_2_driver)++\n");
	i2c_ret = i2c_add_driver(&ccu_i2c_2_driver);
	LOG_DBG_MUST("i2c_add_driver(&ccu_i2c_2_driver), ret: %d--\n",
		i2c_ret);
	LOG_DBG_MUST("i2c_add_driver(&ccu_i2c_4_driver)++\n");
	i2c_ret = i2c_add_driver(&ccu_i2c_4_driver);
	LOG_DBG_MUST("i2c_add_driver(&ccu_i2c_4_driver), ret: %d--\n",
		i2c_ret);
	LOG_DBG_MUST("i2c_add_driver(&ccu_i2c_7_driver)++\n");
	i2c_ret = i2c_add_driver(&ccu_i2c_7_driver);
	LOG_DBG_MUST("i2c_add_driver(&ccu_i2c_7_driver), ret: %d--\n",
		i2c_ret);
	mutex_unlock(&ccu_i2c_mutex);
	return 0;
}

int ccu_i2c_delete_driver(void)
{
	mutex_lock(&ccu_i2c_mutex);
	i2c_del_driver(&ccu_i2c_2_driver);
	i2c_del_driver(&ccu_i2c_4_driver);
	i2c_del_driver(&ccu_i2c_7_driver);
	mutex_unlock(&ccu_i2c_mutex);
	return 0;
}

int ccu_i2c_controller_init(uint32_t i2c_id)
{
	mutex_lock(&ccu_i2c_mutex);
	if (i2c_id >= I2C_MAX_CHANNEL) {
		LOG_ERR("i2c_id %d is invalid\n", i2c_id);
		mutex_unlock(&ccu_i2c_mutex);
		return -EINVAL;
	}

	if (ccu_i2c_initialized[i2c_id] == MTRUE) {
/*if not first time init, release mutex first to avoid deadlock*/
		LOG_DBG_MUST("reinit, temporily release mutex.\n");
	}
	if (ccu_i2c_controller_en(i2c_id, 1) == -1) {
		LOG_DBG("ccu_i2c_controller_en 1 fail\n");
		mutex_unlock(&ccu_i2c_mutex);
		return -1;
	}

	LOG_DBG_MUST("%s done.\n", __func__);
	mutex_unlock(&ccu_i2c_mutex);
	return 0;
}

int ccu_i2c_controller_uninit_all(void)
{
	int i;

	mutex_lock(&ccu_i2c_mutex);
	for (i = 0 ; i < I2C_MAX_CHANNEL ; i++) {
		if (ccu_i2c_initialized[i])
			ccu_i2c_controller_uninit(i);
	}

	LOG_INF_MUST("%s done.\n", __func__);

	mutex_unlock(&ccu_i2c_mutex);
	return 0;
}

int ccu_get_i2c_dma_buf_addr(struct ccu_i2c_buf_mva_ioarg *ioarg)
{
	int ret = 0;
	void *va;

	mutex_lock(&ccu_i2c_mutex);
	ret = i2c_query_dma_buffer_addr(ioarg->sensor_idx,
		&va, &ioarg->va_h, &ioarg->va_l, &ioarg->i2c_id);

	if (ret != 0) {
		mutex_unlock(&ccu_i2c_mutex);
		return ret;
	}

	/*If there is existing i2c buffer mva allocated, deallocate it first*/
	ccu_deallocate_mva(i2c_mva[ioarg->sensor_idx]);
	i2c_mva[ioarg->sensor_idx] = 0;
	ret = ccu_allocate_mva(&i2c_mva[ioarg->sensor_idx], va, 4096);
	ioarg->mva = i2c_mva[ioarg->sensor_idx];
	mutex_unlock(&ccu_i2c_mutex);
	return ret;
}


int ccu_i2c_free_dma_buf_mva_all(void)
{
	uint32_t i;

	mutex_lock(&ccu_i2c_mutex);
	for (i = IMGSENSOR_SENSOR_IDX_MIN_NUM;
		i < IMGSENSOR_SENSOR_IDX_MAX_NUM; i++) {
		ccu_deallocate_mva(i2c_mva[i]);
		i2c_mva[i] = 0;
	}

	LOG_INF_MUST("%s done.\n", __func__);
	mutex_unlock(&ccu_i2c_mutex);

	return 0;
}

/*---------------------------------------------------------------------------*/
/* CCU i2c static funcs                                              */
/*---------------------------------------------------------------------------*/
static int i2c_query_dma_buffer_addr(uint32_t sensor_idx,
	void **va, uint32_t *va_h, uint32_t *va_l, uint32_t *i2c_id)
{
	struct i2c_client *pClient = NULL;
	struct mt_i2c *i2c;

	pClient = get_ccu_i2c_client(*i2c_id);

	if (pClient == MNULL) {
		LOG_ERR("ccu client is NULL");
		return -EFAULT;
	}

	i2c = i2c_get_adapdata(pClient->adapter);

	/*i2c_get_dma_buffer_addr_imp(pClient->adapter ,va);*/
	*va = i2c->dma_buf.vaddr + PAGE_SIZE;
	*va_l = i2c->dma_buf.paddr + PAGE_SIZE;
	*va_h = 0;
#ifdef CONFIG_COMPAT
	*va_h = ((i2c->dma_buf.paddr + PAGE_SIZE) >> 32);
#endif
	*i2c_id = i2c->id;
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	LOG_DBG_MUST("$$pa(%lld), va(%p), i2c-id(%d)\n",
		i2c->dma_buf.paddr + PAGE_SIZE,
		i2c->dma_buf.vaddr + PAGE_SIZE, (uint32_t)i2c->id);
#else
	LOG_DBG_MUST("$$pa(%ld), va(%p), i2c-id(%d)\n",
		i2c->dma_buf.paddr + PAGE_SIZE,
		i2c->dma_buf.vaddr + PAGE_SIZE, (uint32_t)i2c->id);
#endif
	return 0;
}

static int ccu_i2c_controller_en(uint32_t i2c_id,
	int enable)
{
	int ret = 0;
	struct i2c_client *pClient = NULL;
	struct mt_i2c *i2c;

	LOG_INF_MUST("%s, id(%d):(%d)->(%d)\n", __func__,
	i2c_id, ccu_i2c_initialized[i2c_id], enable);

	pClient = get_ccu_i2c_client(i2c_id);
	LOG_DBG("%s, pClient: %p\n", __func__, pClient);

	if (pClient == NULL) {
		LOG_ERR("i2c_client is null\n");
		return -1;
	}

	if (enable) {
		if (ccu_i2c_initialized[i2c_id] == MFALSE) {
			ret = i2c_ccu_enable(pClient->adapter,
				I2C_BASE_OFS_CH1);
			ccu_i2c_initialized[i2c_id] = MTRUE;
			LOG_INF_MUST("i2c_ccu_enable done.\n");

			/*dump controller status*/
			i2c = i2c_get_adapdata(pClient->adapter);
			/*MCU_INTR re-direct to CCU only*/
			i2c_writew(2, i2c, 0x240);
		}
	} else {
		if (ccu_i2c_initialized[i2c_id] == MTRUE) {
			ret = i2c_ccu_disable(pClient->adapter);
			ccu_i2c_initialized[i2c_id] = MFALSE;
			LOG_INF_MUST("i2c_ccu_disable done.\n");
		}
	}
	return ret;
}

static int ccu_i2c_controller_uninit(uint32_t i2c_id)
{
	if (ccu_i2c_controller_en(i2c_id, 0) == -1) {
		LOG_DBG("ccu_i2c_controller_en 0 fail\n");
		return -1;
	}

	LOG_DBG_MUST("%s done: id(%d)\n", __func__,
		i2c_id);

	return 0;
}


static struct i2c_client *get_ccu_i2c_client(
	uint32_t i2c_id)
{
	switch (i2c_id) {
	case 2:
	{
		return g_ccuI2cClient2;
	}
	case 4:
	{
		return g_ccuI2cClient4;
	}
	case 7:
	{
		return g_ccuI2cClient7;
	}
	default:
	{
		return MNULL;
	}
	}
}

static inline void i2c_writew(u16 value, struct mt_i2c *i2c, u16 offset)
{
	writew(value, i2c->base + offset);
}
