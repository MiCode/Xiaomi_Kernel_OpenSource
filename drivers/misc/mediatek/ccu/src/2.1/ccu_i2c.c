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
#include <linux/dma-mapping.h>

#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/sched.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/i2c.h>

#include "i2c-mtk.h"
#include <m4u.h>
#include "kd_camera_feature.h"/*for IMGSENSOR_SENSOR_IDX*/

#include "ccu_cmn.h"
#include "ccu_i2c.h"
#include "ccu_i2c_hw.h"
#include "ccu_mva.h"
#include "ccu_imgsensor.h"
/*******************************************************************************
 *
 ******************************************************************************/
/*I2C Channel offset*/
#define I2C_BASE_OFS_CH1 (0x200)
#define MAX_I2C_CMD_LEN 255
#define CCU_I2C_APDMA_TXLEN 128
#define CCU_I2C_2_HW_DRVNAME  "ccu_i2c_2_hwtrg"
#define CCU_I2C_4_HW_DRVNAME  "ccu_i2c_4_hwtrg"
#define CCU_I2C_6_HW_DRVNAME  "ccu_i2c_6_hwtrg"
#define CCU_I2C_7_HW_DRVNAME  "ccu_i2c_7_hwtrg"


/*i2c driver hook*/

static int ccu_i2c_probe_2(struct i2c_client *client,
	const struct i2c_device_id *id);
static int ccu_i2c_probe_4(struct i2c_client *client,
	const struct i2c_device_id *id);
static int ccu_i2c_probe_6(struct i2c_client *client,
	const struct i2c_device_id *id);
static int ccu_i2c_probe_7(struct i2c_client *client,
	const struct i2c_device_id *id);

static int ccu_i2c_remove(struct i2c_client *client);

/*ccu i2c operation*/
static struct i2c_client *get_ccu_i2c_client(uint32_t i2c_id);
static void ccu_i2c_dump_info(struct mt_i2c *i2c);
static int ccu_i2c_controller_en(uint32_t i2c_id, int enable);
static int i2c_query_dma_buffer_addr(struct ccu_device_s
				     *g_ccu_device, uint32_t sensor_idx,
				     uint32_t *mva, uint32_t *va_h, uint32_t *va_l, uint32_t *i2c_id);
static int ccu_i2c_controller_uninit(uint32_t i2c_id);


/*i2c reg operation*/
static inline u32 i2c_readl_dma(struct mt_i2c *i2c, u8 offset);
static inline void i2c_writel_dma(u32 value, struct mt_i2c *i2c,
				  u8 offset);
static inline u16 i2c_readw(struct mt_i2c *i2c, u8 offset);
static inline void i2c_writew(u16 value, struct mt_i2c *i2c,
			      u16 offset);


static struct i2c_client *g_ccuI2cClient2;
static struct i2c_client *g_ccuI2cClient4;
static struct i2c_client *g_ccuI2cClient6;
static struct i2c_client *g_ccuI2cClient7;

static const struct i2c_device_id ccu_i2c_2_ids[]
	= { {CCU_I2C_2_HW_DRVNAME, 0}, {} };
static const struct i2c_device_id ccu_i2c_4_ids[]
	= { {CCU_I2C_4_HW_DRVNAME, 0}, {} };
static const struct i2c_device_id ccu_i2c_6_ids[]
	= { {CCU_I2C_6_HW_DRVNAME, 0}, {} };
static const struct i2c_device_id ccu_i2c_7_ids[]
	= { {CCU_I2C_7_HW_DRVNAME, 0}, {} };

static struct ion_handle *i2c_buffer_handle;
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

static const struct of_device_id ccu_i2c_6_driver_of_ids[] = {
	{.compatible = "mediatek,ccu_sensor_i2c_6_hw",},
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

struct i2c_driver ccu_i2c_6_driver = {
	.probe = ccu_i2c_probe_6,
	.remove = ccu_i2c_remove,
	.driver = {
		.name = CCU_I2C_6_HW_DRVNAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = ccu_i2c_6_driver_of_ids,
#endif
	},
	.id_table = ccu_i2c_6_ids,
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

static int ccu_i2c_probe_6(struct i2c_client *client,
			       const struct i2c_device_id *id)
{
	LOG_DBG_MUST(
	"[ccu_i2c_probe] Attach I2C for HW trriger g_ccuI2cClient6 %p\n",
	client);

	/* get sensor i2c client */
	g_ccuI2cClient6 = client;

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

	LOG_DBG_MUST("i2c_add_driver(&ccu_i2c_2_driver)++\n");
	i2c_ret = i2c_add_driver(&ccu_i2c_2_driver);
	LOG_DBG_MUST("i2c_add_driver(&ccu_i2c_2_driver), ret: %d--\n",
		i2c_ret);
	LOG_DBG_MUST("i2c_add_driver(&ccu_i2c_4_driver)++\n");
	i2c_ret = i2c_add_driver(&ccu_i2c_4_driver);
	LOG_DBG_MUST("i2c_add_driver(&ccu_i2c_4_driver), ret: %d--\n",
		i2c_ret);
	LOG_DBG_MUST("i2c_add_driver(&ccu_i2c_6_driver)++\n");
	i2c_ret = i2c_add_driver(&ccu_i2c_6_driver);
	LOG_DBG_MUST("i2c_add_driver(&ccu_i2c_6_driver), ret: %d--\n",
		i2c_ret);
	LOG_DBG_MUST("i2c_add_driver(&ccu_i2c_7_driver)++\n");
	i2c_ret = i2c_add_driver(&ccu_i2c_7_driver);
	LOG_DBG_MUST("i2c_add_driver(&ccu_i2c_7_driver), ret: %d--\n",
		i2c_ret);

	return 0;
}

int ccu_i2c_delete_driver(void)
{
	i2c_del_driver(&ccu_i2c_2_driver);
	i2c_del_driver(&ccu_i2c_4_driver);
	i2c_del_driver(&ccu_i2c_6_driver);
	i2c_del_driver(&ccu_i2c_7_driver);
	return 0;
}

int ccu_i2c_controller_init(uint32_t i2c_id)
{
	if (ccu_i2c_initialized[i2c_id] == MTRUE) {
		/*if not first time init, release mutex first to avoid deadlock*/
		LOG_DBG_MUST("reinit, temporily release mutex.\n");
	}
	if (ccu_i2c_controller_en(i2c_id, 1) == -1) {
		LOG_DBG("ccu_i2c_controller_en 1 fail\n");
		return -1;
	}

	LOG_DBG_MUST("%s done.\n", __func__);

	return 0;
}

int ccu_i2c_controller_uninit_all(void)
{
	int i;

	for (i = 0 ; i < I2C_MAX_CHANNEL ; i++) {
		if (ccu_i2c_initialized[i])
			ccu_i2c_controller_uninit(i);
	}

	LOG_INF_MUST("%s done.\n", __func__);

	return 0;
}

int ccu_get_i2c_dma_buf_addr(struct ccu_device_s *g_ccu_device,
			     struct ccu_i2c_buf_mva_ioarg *ioarg)
{
	int ret = 0;

	ret = i2c_query_dma_buffer_addr(g_ccu_device, ioarg->sensor_idx,
	 &ioarg->mva, &ioarg->va_h, &ioarg->va_l, &ioarg->i2c_id);

	return ret;
}


int ccu_i2c_free_dma_buf_mva_all(void)
{

	ccu_deallocate_mva(&i2c_buffer_handle);

	LOG_INF_MUST("%s done.\n", __func__);

	return 0;
}

void ccu_i2c_dump_errr(void)
{
	struct i2c_client *pClient = NULL;
	struct mt_i2c *i2c;
	int i;


	for (i = 0 ; i < I2C_MAX_CHANNEL ; i++) {
		LOG_INF_MUST(
		"CCU Dump I2C controller[%d] reg =========\n", i);
		pClient = get_ccu_i2c_client(i);
		if (pClient != NULL) {
			i2c = i2c_get_adapdata(pClient->adapter);
			ccu_i2c_dump_info(i2c);
		} else {
			LOG_INF_MUST("I2C controller[%d] CCU client is null\n", i);
		}
	}
}

/*---------------------------------------------------------------------------*/
/* CCU i2c static funcs                                              */
/*---------------------------------------------------------------------------*/
static int i2c_query_dma_buffer_addr(struct ccu_device_s *g_ccu_device,
	uint32_t sensor_idx, uint32_t *mva, uint32_t *va_h,
	uint32_t *va_l, uint32_t *i2c_id)
{
	struct i2c_client *pClient = NULL;
	struct mt_i2c *i2c;
	int ret = 0;
	*i2c_id = ccu_get_sensor_i2c_id(sensor_idx);
	pClient = get_ccu_i2c_client(*i2c_id);

	if (pClient == MNULL) {
		LOG_ERR("ccu client is NULL");
		return -EFAULT;
	}
	if (g_ccu_device->dev == MNULL) {
		LOG_ERR("ccu device is NULL");
		return -EFAULT;
	}

	i2c = i2c_get_adapdata(pClient->adapter);

	if (g_ccu_device->i2c_dma_mva == 0)	{
		ret = ccu_allocate_mva(&g_ccu_device->i2c_dma_mva,
				       g_ccu_device->i2c_dma_vaddr, &i2c_buffer_handle,
				       CCU_I2C_DMA_BUF_SIZE);
		if (ret != 0) {
			LOG_ERR("ccu alloc mva fail");
			return -EFAULT;
		}
	}
	/*i2c_get_dma_buffer_addr_imp(pClient->adapter ,va);*/
	*mva = g_ccu_device->i2c_dma_mva;
	*va_l = g_ccu_device->i2c_dma_paddr;
	*va_h = ((g_ccu_device->i2c_dma_paddr) >> 32);
	*i2c_id = i2c->id;
	LOG_DBG_MUST("$$pa(%lld), mva(%x), i2c-id(%d)\n",
		     g_ccu_device->i2c_dma_paddr,
		     g_ccu_device->i2c_dma_mva, (uint32_t)i2c->id);

	return 0;
}

static int ccu_i2c_controller_en(uint32_t i2c_id, int enable)
{
	int ret = 0;
	struct i2c_client *pClient = NULL;
	struct mt_i2c *i2c;

	LOG_INF_MUST("%s, id(%d):(%d)->(%d)\n",
		__func__, i2c_id, ccu_i2c_initialized[i2c_id], enable);

	pClient = get_ccu_i2c_client(i2c_id);
	LOG_DBG("%s, pClient: %p\n", __func__, pClient);

	if (pClient == NULL) {
		LOG_ERR("i2c_client is null\n");
		return -1;
	}

	if (enable) {
		if (ccu_i2c_initialized[i2c_id] == MFALSE) {
			ret = i2c_ccu_enable(pClient->adapter, I2C_BASE_OFS_CH1);
			ccu_i2c_initialized[i2c_id] = MTRUE;
			LOG_INF_MUST("i2c_ccu_enable done.\n");

			/*dump controller status*/
			i2c = i2c_get_adapdata(pClient->adapter);
			i2c_writew(2, i2c, 0x240);	/*MCU_INTR re-direct to CCU only*/
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

	LOG_DBG_MUST("%s done: id(%d)\n", __func__, i2c_id);

	return 0;
}

static struct i2c_client *get_ccu_i2c_client(uint32_t i2c_id)
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
	case 6:
	{
		return g_ccuI2cClient6;
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

static inline u32 i2c_readl_dma(struct mt_i2c *i2c, u8 offset)
{
	return readl(i2c->pdmabase + offset);
}

static inline void i2c_writel_dma(u32 value, struct mt_i2c *i2c,
				  u8 offset)
{
	writel(value, i2c->pdmabase + offset);
}

static inline u16 i2c_readw(struct mt_i2c *i2c, u8 offset)
{
	return readw(i2c->base + offset);
}

static inline void i2c_writew(u16 value, struct mt_i2c *i2c,
			      u16 offset)
{
	writew(value, i2c->base + offset);
}

static void ccu_i2c_dump_info(struct mt_i2c *i2c)
{
	/* I2CFUC(); */
	/* int val=0; */
	pr_info("i2c_dump_info ++++++++++++++++++++++++++++++++++++++++++\n");
	pr_info("I2C structure:\n"
		I2CTAG "Clk=%d,Id=%d,Op=%x,Irq_stat=%x,Total_len=%x\n"
		I2CTAG "Trans_len=%x,Trans_num=%x,Trans_auxlen=%x,speed=%d\n"
		I2CTAG "Trans_stop=%u\n",
		15600, i2c->id, i2c->op, i2c->irq_stat, i2c->total_len,
		i2c->msg_len, 1, i2c->msg_aux_len, i2c->speed_hz,
		i2c->trans_stop);

	pr_info("base address 0x%p\n", i2c->base);
	pr_info("I2C register:\n"
		I2CTAG "SLAVE_ADDR=%x,INTR_MASK=%x,INTR_STAT=%x,CONTROL=%x,TRANSFER_LEN=%x\n"
		I2CTAG "TRANSAC_LEN=%x,DELAY_LEN=%x,TIMING=%x,START=%x,FIFO_STAT=%x\n"
		I2CTAG "IO_CONFIG=%x,HS=%x,DCM_EN=%x,DEBUGSTAT=%x,EXT_CONF=%x,TRANSFER_LEN_AUX=%x\n",
		(i2c_readw(i2c, OFFSET_SLAVE_ADDR)),
		(i2c_readw(i2c, OFFSET_INTR_MASK)),
		(i2c_readw(i2c, OFFSET_INTR_STAT)),
		(i2c_readw(i2c, OFFSET_CONTROL)),
		(i2c_readw(i2c, OFFSET_TRANSFER_LEN)),
		(i2c_readw(i2c, OFFSET_TRANSAC_LEN)),
		(i2c_readw(i2c, OFFSET_DELAY_LEN)),
		(i2c_readw(i2c, OFFSET_TIMING)),
		(i2c_readw(i2c, OFFSET_START)),
		(i2c_readw(i2c, OFFSET_FIFO_STAT)),
		(i2c_readw(i2c, OFFSET_IO_CONFIG)),
		(i2c_readw(i2c, OFFSET_HS)),
		(i2c_readw(i2c, OFFSET_DCM_EN)),
		(i2c_readw(i2c, OFFSET_DEBUGSTAT)),
		(i2c_readw(i2c, OFFSET_EXT_CONF)), (i2c_readw(i2c,
				OFFSET_TRANSFER_LEN_AUX)));

	pr_info("DMA register(0x%p):\n"
		I2CTAG "INT_FLAG=%x,INT_EN=%x,EN=%x,RST=%x,\n"
		I2CTAG "STOP=%x,FLUSH=%x,CON=%x,TX_MEM_ADDR=%x, RX_MEM_ADDR=%x\n"
		I2CTAG "TX_LEN=%x,RX_LEN=%x,INT_BUF_SIZE=%x,DEBUG_STATUS=%x\n"
		I2CTAG "TX_MEM_ADDR2=%x, RX_MEM_ADDR2=%x\n",
		i2c->pdmabase,
		(i2c_readl_dma(i2c, OFFSET_INT_FLAG)),
		(i2c_readl_dma(i2c, OFFSET_INT_EN)),
		(i2c_readl_dma(i2c, OFFSET_EN)),
		(i2c_readl_dma(i2c, OFFSET_RST)),
		(i2c_readl_dma(i2c, OFFSET_STOP)),
		(i2c_readl_dma(i2c, OFFSET_FLUSH)),
		(i2c_readl_dma(i2c, OFFSET_CON)),
		(i2c_readl_dma(i2c, OFFSET_TX_MEM_ADDR)),
		(i2c_readl_dma(i2c, OFFSET_RX_MEM_ADDR)),
		(i2c_readl_dma(i2c, OFFSET_TX_LEN)),
		(i2c_readl_dma(i2c, OFFSET_RX_LEN)),
		(i2c_readl_dma(i2c, OFFSET_INT_BUF_SIZE)),
		(i2c_readl_dma(i2c, OFFSET_DEBUG_STA)),
		(i2c_readl_dma(i2c, OFFSET_TX_MEM_ADDR2)),
		(i2c_readl_dma(i2c, OFFSET_RX_MEM_ADDR2)));
	pr_info("i2c_dump_info ------------------------------------------\n");
}
