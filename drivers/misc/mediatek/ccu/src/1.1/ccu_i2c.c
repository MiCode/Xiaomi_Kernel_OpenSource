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

/**************************************************************************
 *
 **************************************************************************/
 /*I2C Channel offset*/
#define I2C_BASE_OFS_CH1 (0x200)
#define MAX_I2C_CMD_LEN 255
#define CCU_I2C_APDMA_TXLEN 128
#define CCU_I2C_MAIN_HW_DRVNAME  "ccu_i2c_main_hwtrg"
#define CCU_I2C_SUB_HW_DRVNAME  "ccu_i2c_sub_hwtrg"
#define CCU_I2C_MAIN3_HW_DRVNAME  "ccu_i2c_main3_hwtrg"

/*--todo: check if need id-table & name, id of_match_table is given*/
static int ccu_i2c_probe_main(struct i2c_client *client,
	const struct i2c_device_id *id);
static int ccu_i2c_probe_main3(struct i2c_client *client,
	const struct i2c_device_id *id);
static int ccu_i2c_probe_sub(struct i2c_client *client,
	const struct i2c_device_id *id);
static int ccu_i2c_remove(struct i2c_client *client);
static struct i2c_client *getCcuI2cClient(void);
static inline u32 i2c_readl_dma(struct mt_i2c *i2c, u16 offset);
static inline void i2c_writel_dma(u32 value, struct mt_i2c *i2c, u16 offset);
static inline u16 i2c_readw(struct mt_i2c *i2c, u16 offset);
static inline void i2c_writew(u16 value, struct mt_i2c *i2c, u16 offset);
static void ccu_i2c_dump_info(struct mt_i2c *i2c);


static enum CCU_I2C_CHANNEL g_ccuI2cChannel = CCU_I2C_CHANNEL_UNDEF;
static struct i2c_client *g_ccuI2cClientMain;
static struct i2c_client *g_ccuI2cClientMain3;
static struct i2c_client *g_ccuI2cClientSub;
static MBOOL ccu_i2c_enabled = MFALSE;

static const struct i2c_device_id ccu_i2c_main_ids[] = {
	{CCU_I2C_MAIN_HW_DRVNAME, 0},
	{}
};
static const struct i2c_device_id ccu_i2c_sub_ids[] = {
	{CCU_I2C_SUB_HW_DRVNAME, 0},
	{}
};

static const struct i2c_device_id ccu_i2c_main3_ids[] = {
	{CCU_I2C_MAIN3_HW_DRVNAME, 0},
	{}
};

#ifdef CONFIG_OF
static const struct of_device_id ccu_i2c_main_driver_of_ids[] = {
	{.compatible = "mediatek,ccu_sensor_i2c_main_hw",},
	{}
};

static const struct of_device_id ccu_i2c_main3_driver_of_ids[] = {
	{.compatible = "mediatek,ccu_sensor_i2c_main3_hw",},
	{}
};

static const struct of_device_id ccu_i2c_sub_driver_of_ids[] = {
	{.compatible = "mediatek,ccu_sensor_i2c_sub_hw",},
	{}
};
#endif

struct i2c_driver ccu_i2c_main_driver = {
	.probe = ccu_i2c_probe_main,
	.remove = ccu_i2c_remove,
	.driver = {
		   .name = CCU_I2C_MAIN_HW_DRVNAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = ccu_i2c_main_driver_of_ids,
#endif
		   },
	.id_table = ccu_i2c_main_ids,
};

struct i2c_driver ccu_i2c_main3_driver = {
	.probe = ccu_i2c_probe_main3,
	.remove = ccu_i2c_remove,
	.driver = {
		   .name = CCU_I2C_MAIN3_HW_DRVNAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = ccu_i2c_main3_driver_of_ids,
#endif
		   },
	.id_table = ccu_i2c_main3_ids,
};

struct i2c_driver ccu_i2c_sub_driver = {
	.probe = ccu_i2c_probe_sub,
	.remove = ccu_i2c_remove,
	.driver = {
		   .name = CCU_I2C_SUB_HW_DRVNAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = ccu_i2c_sub_driver_of_ids,
#endif
		   },
	.id_table = ccu_i2c_sub_ids,
};

/*----------------------------------------------------------------------*/
/* CCU Driver: i2c driver funcs                                         */
/*----------------------------------------------------------------------*/
static int ccu_i2c_probe_main(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	/*int i4RetValue = 0;*/
	LOG_DBG("[%s] Attach I2C for HW trriger g_ccuI2cClientMain %p\n",
		"ccu_i2c_probe", client);

	/* get sensor i2c client */
	/*--todo: add subcam implementation*/
	g_ccuI2cClientMain = client;

	/* set I2C clock rate */
	/*#ifdef CONFIG_MTK_I2C_EXTENSION*/
	/*g_pstI2Cclient3->timing = 100;*/ /* 100k */
	/* No I2C polling busy waiting */
	/*g_pstI2Cclient3->ext_flag &= ~I2C_POLLING_FLAG;*/
	/*#endif*/

	LOG_DBG("[ccu_i2c_probe] Attached!!\n");
	return 0;
}

static int ccu_i2c_probe_main3(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	/*int i4RetValue = 0;*/
	LOG_DBG("[%s] Attach I2C for HW trriger g_ccuI2cClientMain3 %p\n",
		"ccu_i2c_probe", client);

	/* get sensor i2c client */
	/*--todo: add subcam implementation*/
	g_ccuI2cClientMain3 = client;

	/* set I2C clock rate */
	/*#ifdef CONFIG_MTK_I2C_EXTENSION*/
	/*g_pstI2Cclient3->timing = 100;*/ /* 100k */
	/* No I2C polling busy waiting */
	/*g_pstI2Cclient3->ext_flag &= ~I2C_POLLING_FLAG;*/
	/*#endif*/

	LOG_DBG("[ccu_i2c_probe] Attached!!\n");
	return 0;
}

static int ccu_i2c_probe_sub(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	/*int i4RetValue = 0;*/
	LOG_DBG("[%s] Attach I2C for HW trriger g_ccuI2cClientSub %p\n",
		"ccu_i2c_probe", client);

	g_ccuI2cClientSub = client;

	LOG_DBG("[ccu_i2c_probe] Attached!!\n");
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

	LOG_DBG("i2c_add_driver(&ccu_i2c_main_driver)++\n");
	i2c_ret = i2c_add_driver(&ccu_i2c_main_driver);
	LOG_DBG("i2c_add_driver(&ccu_i2c_main_driver), ret: %d--\n", i2c_ret);
	LOG_DBG("i2c_add_driver(&ccu_i2c_main3_driver)++\n");
	i2c_ret = i2c_add_driver(&ccu_i2c_main3_driver);
	LOG_DBG("i2c_add_driver(&ccu_i2c_main3_driver), ret: %d--\n", i2c_ret);
	LOG_DBG("i2c_add_driver(&ccu_i2c_sub_driver)++\n");
	i2c_ret = i2c_add_driver(&ccu_i2c_sub_driver);
	LOG_DBG("i2c_add_driver(&ccu_i2c_sub_driver), ret: %d--\n", i2c_ret);

	return 0;
}

int ccu_i2c_delete_driver(void)
{
	i2c_del_driver(&ccu_i2c_main_driver);
	i2c_del_driver(&ccu_i2c_main3_driver);
	i2c_del_driver(&ccu_i2c_sub_driver);

	return 0;
}

int ccu_i2c_set_channel(enum CCU_I2C_CHANNEL channel)
{
	if ((channel == CCU_I2C_CHANNEL_MAINCAM) ||
		(channel == CCU_I2C_CHANNEL_MAINCAM3) ||
		(channel == CCU_I2C_CHANNEL_SUBCAM)) {
		g_ccuI2cChannel = channel;
		return 0;
	} else
		return -EFAULT;
}

int ccu_i2c_buf_mode_init(unsigned char i2c_write_id, int transfer_len)
{
	if (ccu_i2c_buf_mode_en(1) == -1) {
		LOG_DBG("i2c_buf_mode_en fail\n");
		return -1;
	}

	LOG_DBG_MUST("%s done.\n", __func__);

	return 0;
}

int ccu_i2c_buf_mode_en(int enable)
{
	int ret = 0;
	struct i2c_client *pClient = NULL;
	struct mt_i2c *i2c;

	LOG_DBG_MUST("i2c_buf_mode_en %d\n", enable);

	pClient = getCcuI2cClient();

	LOG_DBG("i2c_buf_mode_en, pClient: %p\n", pClient);

	if (pClient == NULL) {
		LOG_ERR("i2c_client is null\n");
		return -1;
	}

	LOG_DBG_MUST("ccu_i2c_enabled %d\n", ccu_i2c_enabled);

	if (enable) {
		if (ccu_i2c_enabled == MFALSE) {
			ret = i2c_ccu_enable(
				pClient->adapter, I2C_BASE_OFS_CH1);
			ccu_i2c_enabled = MTRUE;

			LOG_DBG_MUST("i2c_ccu_enable done(%d).\n", ret);
			i2c = i2c_get_adapdata(pClient->adapter);
			i2c_writew(2, i2c, 0x240);
		}
	} else {
		if (ccu_i2c_enabled == MTRUE) {
			ret = i2c_ccu_disable(pClient->adapter);
			ccu_i2c_enabled = MFALSE;

			LOG_DBG_MUST("i2c_ccu_disable done(%d).\n", ret);
		}
	}
	return ret;
}

int i2c_get_dma_buffer_addr(void **va,
	uint32_t *pa_h, uint32_t *pa_l, uint32_t *i2c_id)
{
	struct i2c_client *pClient = NULL;
	struct mt_i2c *i2c;

	pClient = getCcuI2cClient();

	if (pClient == MNULL) {
		LOG_ERR("ccu client is NULL");
		return -EFAULT;
	}

	i2c = i2c_get_adapdata(pClient->adapter);

	/*i2c_get_dma_buffer_addr_imp(pClient->adapter ,va);*/
	*va = i2c->dma_buf.vaddr + PAGE_SIZE;
	*pa_l = i2c->dma_buf.paddr + PAGE_SIZE;
	*pa_h = 0;
#ifdef CONFIG_COMPAT
	*pa_h = ((i2c->dma_buf.paddr  + PAGE_SIZE) >> 32);
#endif
	*i2c_id = i2c->id;
	LOG_DBG_MUST("va(%p), pal(%d), pah(%d), id(%d)\n",
		*va, *pa_l, *pa_h, *i2c_id);

	return 0;
}

/*-------------------------------------------------------------------*/
/* CCU i2c static funcs                                              */
/*-------------------------------------------------------------------*/
static struct i2c_client *getCcuI2cClient(void)
{
	switch (g_ccuI2cChannel) {
	case CCU_I2C_CHANNEL_MAINCAM:
		{
			return g_ccuI2cClientMain;
		}
	case CCU_I2C_CHANNEL_MAINCAM3:
		{
			return g_ccuI2cClientMain3;
		}
	case CCU_I2C_CHANNEL_SUBCAM:
		{
			return g_ccuI2cClientSub;
		}
	default:
		{
			return MNULL;
		}
	}
}

static inline u32 i2c_readl_dma(struct mt_i2c *i2c, u16 offset)
{
	return readl(i2c->pdmabase + offset);
}

static inline void i2c_writel_dma(u32 value, struct mt_i2c *i2c, u16 offset)
{
	writel(value, i2c->pdmabase + offset);
}

static inline u16 i2c_readw(struct mt_i2c *i2c, u16 offset)
{
	return readw(i2c->base + offset);
}

static inline void i2c_writew(u16 value, struct mt_i2c *i2c, u16 offset)
{
	writew(value, i2c->base + offset);
}

void ccu_i2c_dump_errr(void)
{
	struct i2c_client *pClient = NULL;
	struct mt_i2c *i2c;

	LOG_DBG_MUST("CCU Dump I2C reg\n");

	pClient = getCcuI2cClient();
	if (pClient == NULL) {
		LOG_ERR("i2c_client is null\n");
		return;
	}

	i2c = i2c_get_adapdata(pClient->adapter);
	ccu_i2c_dump_info(i2c);
}

static void ccu_i2c_dump_info(struct mt_i2c *i2c)
{
	/* I2CFUC(); */
	/* int val=0; */
	pr_info("i2c_dump_info ++++++++++++++++++++++++++++++++++++++++++\n");
	pr_info("base address 0x%p\n", i2c->base);
	pr_info("I2C register:\n"
	       I2CTAG "SLAVE_ADDR=%x,INTR_MASK=%x,INTR_STAT=%x,"
	       I2CTAG "CONTROL=%x,TRANSFER_LEN=%x\n"
	       I2CTAG "TRANSAC_LEN=%x,DELAY_LEN=%x,"
	       I2CTAG "TIMING=%x,START=%x,FIFO_STAT=%x\n"
	       I2CTAG "IO_CONFIG=%x,HS=%x,DCM_EN=%x,DEBUGSTAT=%x,"
	       I2CTAG "EXT_CONF=%x,TRANSFER_LEN_AUX=%x\n",
	       (i2c_readw(i2c, 0x200 + OFFSET_SLAVE_ADDR)),
	       (i2c_readw(i2c, 0x200 + OFFSET_INTR_MASK)),
	       (i2c_readw(i2c, 0x200 + OFFSET_INTR_STAT)),
	       (i2c_readw(i2c, 0x200 + OFFSET_CONTROL)),
	       (i2c_readw(i2c, 0x200 + OFFSET_TRANSFER_LEN)),
	       (i2c_readw(i2c, 0x200 + OFFSET_TRANSAC_LEN)),
	       (i2c_readw(i2c, 0x200 + OFFSET_DELAY_LEN)),
	       (i2c_readw(i2c, 0x200 + OFFSET_TIMING)),
	       (i2c_readw(i2c, 0x200 + OFFSET_START)),
	       (i2c_readw(i2c, 0x200 + OFFSET_FIFO_STAT)),
	       (i2c_readw(i2c, 0x200 + OFFSET_IO_CONFIG)),
	       (i2c_readw(i2c, 0x200 + OFFSET_HS)),
	       (i2c_readw(i2c, 0x200 + OFFSET_DCM_EN)),
	       (i2c_readw(i2c, 0x200 + OFFSET_DEBUGSTAT)),
	       (i2c_readw(i2c, 0x200 + OFFSET_EXT_CONF)),
		   (i2c_readw(i2c, 0x200 + OFFSET_TRANSFER_LEN_AUX)));

	pr_info("DMA register(0x%p):\n"
	       I2CTAG "INT_FLAG=%x,INT_EN=%x,EN=%x,RST=%x,\n"
	       I2CTAG "STOP=%x,FLUSH=%x,CON=%x,TX_MEM_ADDR=%x, RX_MEM_ADDR=%x\n"
	       I2CTAG "TX_LEN=%x,RX_LEN=%x,INT_BUF_SIZE=%x,DEBUG_STATUS=%x\n"
	       I2CTAG "TX_MEM_ADDR2=%x, RX_MEM_ADDR2=%x\n",
	       i2c->pdmabase,
	       (i2c_readl_dma(i2c, 0x80 + OFFSET_INT_FLAG)),
	       (i2c_readl_dma(i2c, 0x80 + OFFSET_INT_EN)),
	       (i2c_readl_dma(i2c, 0x80 + OFFSET_EN)),
	       (i2c_readl_dma(i2c, 0x80 + OFFSET_RST)),
	       (i2c_readl_dma(i2c, 0x80 + OFFSET_STOP)),
	       (i2c_readl_dma(i2c, 0x80 + OFFSET_FLUSH)),
	       (i2c_readl_dma(i2c, 0x80 + OFFSET_CON)),
	       (i2c_readl_dma(i2c, 0x80 + OFFSET_TX_MEM_ADDR)),
	       (i2c_readl_dma(i2c, 0x80 + OFFSET_RX_MEM_ADDR)),
	       (i2c_readl_dma(i2c, 0x80 + OFFSET_TX_LEN)),
	       (i2c_readl_dma(i2c, 0x80 + OFFSET_RX_LEN)),
	       (i2c_readl_dma(i2c, 0x80 + OFFSET_INT_BUF_SIZE)),
	       (i2c_readl_dma(i2c, 0x80 + OFFSET_DEBUG_STA)),
	       (i2c_readl_dma(i2c, 0x80 + OFFSET_TX_MEM_ADDR2)),
	       (i2c_readl_dma(i2c, 0x80 + OFFSET_RX_MEM_ADDR2)));
	pr_info("i2c_dump_info ------------------------------------------\n");

}
