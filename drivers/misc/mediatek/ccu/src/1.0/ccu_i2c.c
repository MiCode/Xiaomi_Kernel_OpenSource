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

#define MAX_I2C_CMD_LEN 255
#define CCU_I2C_APDMA_TXLEN 128
#define CCU_I2C_MAIN_HW_DRVNAME  "ccu_i2c_main_hwtrg"
#define CCU_I2C_SUB_HW_DRVNAME  "ccu_i2c_sub_hwtrg"

/*--todo: check if need id-table & name, id of_match_table is given*/
static int ccu_i2c_probe_main(
	struct i2c_client *client, const struct i2c_device_id *id);
static int ccu_i2c_probe_sub(
	struct i2c_client *client, const struct i2c_device_id *id);
static int ccu_i2c_remove(struct i2c_client *client);
static struct i2c_client *getCcuI2cClient(void);
static inline u32 i2c_readl_dma(struct mt_i2c *i2c, u8 offset);
static inline void i2c_writel_dma(u32 value, struct mt_i2c *i2c, u8 offset);
static inline u16 i2c_readw(struct mt_i2c *i2c, u8 offset);
static inline void i2c_writew(u16 value, struct mt_i2c *i2c, u8 offset);
static void ccu_record_i2c_dma_info(struct mt_i2c *i2c);
static void ccu_i2c_dump_info(struct mt_i2c *i2c);
static int ccu_reset_i2c_apdma(struct mt_i2c *i2c);

static enum CCU_I2C_CHANNEL g_ccuI2cChannel = CCU_I2C_CHANNEL_UNDEF;
static struct i2c_client *g_ccuI2cClientMain;
static struct i2c_client *g_ccuI2cClientSub;
static struct i2c_dma_info g_dma_reg;
static struct i2c_msg ccu_i2c_msg[MAX_I2C_CMD_LEN];

static const struct i2c_device_id
	ccu_i2c_main_ids[] = { {CCU_I2C_MAIN_HW_DRVNAME, 0}, {} };
static const struct i2c_device_id
	ccu_i2c_sub_ids[] = { {CCU_I2C_SUB_HW_DRVNAME, 0}, {} };

#ifdef CONFIG_OF
static const struct of_device_id ccu_i2c_main_driver_of_ids[] = {
	{.compatible = "mediatek,ccu_sensor_i2c_main_hw",},
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

/*---------------------------------------------------------------------------*/
/* CCU Driver: i2c driver funcs                                              */
/*---------------------------------------------------------------------------*/
static int ccu_i2c_probe_main(
	struct i2c_client *client, const struct i2c_device_id *id)
{
	/*int i4RetValue = 0;*/
	LOG_DBG("[ccu_i2c_probe] Attach g_ccuI2cClientMain %p\n", client);

	/* get sensor i2c client */
	/*--todo: add subcam implementation*/
	g_ccuI2cClientMain = client;

	/* set I2C clock rate */
	/*#ifdef CONFIG_MTK_I2C_EXTENSION*/
	/*g_pstI2Cclient3->timing = 100;*/ /* 100k */
	/*g_pstI2Cclient3->ext_flag &= ~I2C_POLLING_FLAG;*/
	/*#endif*/

	LOG_DBG("[ccu_i2c_probe] Attached!!\n");
	return 0;
}

static int ccu_i2c_probe_sub(
	struct i2c_client *client, const struct i2c_device_id *id)
{
	/*int i4RetValue = 0;*/
	LOG_DBG("[ccu_i2c_probe] Attach g_ccuI2cClientSub %p\n", client);

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
	LOG_DBG("i2c_add_driver(&ccu_i2c_sub_driver)++\n");
	i2c_ret = i2c_add_driver(&ccu_i2c_sub_driver);
	LOG_DBG("i2c_add_driver(&ccu_i2c_sub_driver), ret: %d--\n", i2c_ret);

	return 0;
}

int ccu_i2c_delete_driver(void)
{
	i2c_del_driver(&ccu_i2c_main_driver);
	i2c_del_driver(&ccu_i2c_sub_driver);

	return 0;
}

int ccu_i2c_set_channel(enum CCU_I2C_CHANNEL channel)
{
	if ((channel == CCU_I2C_CHANNEL_MAINCAM) ||
		(channel == CCU_I2C_CHANNEL_SUBCAM)) {
		g_ccuI2cChannel = channel;
		return 0;
	} else
		return -EFAULT;
}

int ccu_i2c_frame_reset(void)
{
	struct i2c_client *pClient = NULL;
	struct mt_i2c *i2c;

	pClient = getCcuI2cClient();
	i2c = i2c_get_adapdata(pClient->adapter);

	ccu_reset_i2c_apdma(i2c);

	/*--todo:remove dump log on production*/
	/*ccu_record_i2c_dma_info(i2c);*/

	i2c_writew(I2C_FIFO_ADDR_CLR, i2c, OFFSET_FIFO_ADDR_CLR);
	i2c_writew(I2C_HS_NACKERR | I2C_ACKERR | I2C_TRANSAC_COMP,
		i2c, OFFSET_INTR_MASK);
	/* memory barrier to check mailbox value wrote into DRAM*/
    /* instead of keep in CPU write buffer*/
	mb();
	/*--todo:remove dump log on production*/
	/*ccu_i2c_dump_info(i2c);*/

	return 0;
}


int ccu_trigger_i2c(int transac_len, MBOOL do_dma_en)
{
	struct i2c_client *pClient = NULL;
	struct mt_i2c *i2c;

	u8 *dmaBufVa;

	pClient = getCcuI2cClient();
	i2c = i2c_get_adapdata(pClient->adapter);

	dmaBufVa = i2c->dma_buf.vaddr;

	/*set i2c transaction length & enable apdma*/
	i2c_writew(transac_len, i2c, OFFSET_TRANSAC_LEN);

	/*ccu_record_i2c_dma_info(i2c);*/

	i2c_writel_dma(I2C_DMA_START_EN, i2c, OFFSET_EN);

	/*ccu_i2c_dump_info(i2c);*/

	/*trigger i2c start from n3d_a*/
	ccu_trigger_i2c_hw(g_ccuI2cChannel, transac_len, do_dma_en);

	/*ccu_i2c_dump_info(i2c);*/

	return 0;
}


int ccu_config_i2c_buf_mode(int transfer_len)
{
	struct i2c_client *pClient = NULL;
	struct mt_i2c *i2c;

	pClient = getCcuI2cClient();
	i2c = i2c_get_adapdata(pClient->adapter);

	/*write i2c controller tx len*/
	i2c->total_len = transfer_len;
	i2c->msg_len = transfer_len;
	i2c_writew(transfer_len, i2c, OFFSET_TRANSFER_LEN);

	/*ccu_reset_i2c_apdma(i2c);*/

	ccu_record_i2c_dma_info(i2c);

	/*flush before sending DMA start*/
	/*mb();*/
	/*i2c_writel_dma(I2C_DMA_START_EN, i2c, OFFSET_EN);*/

	ccu_i2c_frame_reset();

	ccu_i2c_dump_info(i2c);

	return 0;
}


/*--todo: add subcam implementation*/

int ccu_init_i2c_buf_mode(u16 i2cId)
{
	int ret = 0;
	unsigned char dummy_data[] = {
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18
	};

	struct i2c_client *pClient = NULL;
	int trans_num = 1;

	memset(ccu_i2c_msg, 0, MAX_I2C_CMD_LEN * sizeof(struct i2c_msg));

	pClient = getCcuI2cClient();

	/*for(i = 0 ; i<trans_num ; i++){*/
	ccu_i2c_msg[0].addr = i2cId >> 1;
	ccu_i2c_msg[0].flags = 0;
	ccu_i2c_msg[0].len = 16;
	ccu_i2c_msg[0].buf = dummy_data;
	/*}*/

	ret = hw_trig_i2c_transfer(pClient->adapter, ccu_i2c_msg, trans_num);
	return ret;
}

int ccu_i2c_buf_mode_en(int enable)
{
	static MBOOL ccu_i2c_enabled = MFALSE;
	int ret = 0;
	struct i2c_client *pClient = NULL;

	LOG_DBG_MUST("i2c_buf_mode_en %d\n", enable);

	pClient = getCcuI2cClient();

	LOG_DBG("i2c_buf_mode_en, pClient: %p\n", pClient);
	if (pClient == NULL)
		return -1;

	if (enable) {
		if (ccu_i2c_enabled == MFALSE) {
			ret = hw_trig_i2c_enable(pClient->adapter);
			ccu_i2c_enabled = MTRUE;
			LOG_DBG_MUST("hw_trig_i2c_enable done.\n");
		}
	} else {
		if (ccu_i2c_enabled == MTRUE) {
			ret = hw_trig_i2c_disable(pClient->adapter);
			ccu_i2c_enabled = MFALSE;
			LOG_DBG_MUST("hw_trig_i2c_disable done.\n");
		}
	}
	return ret;
}

int i2c_get_dma_buffer_addr(void **va)
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
	*va = i2c->dma_buf.vaddr;
	LOG_DBG("got i2c buf va: %p\n", *va);

	return 0;
}

/*---------------------------------------------------------------------------*/
/* CCU i2c static funcs                                              */
/*---------------------------------------------------------------------------*/
static struct i2c_client *getCcuI2cClient(void)
{
	switch (g_ccuI2cChannel) {
	case CCU_I2C_CHANNEL_MAINCAM:
		{
			return g_ccuI2cClientMain;
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

static inline u32 i2c_readl_dma(struct mt_i2c *i2c, u8 offset)
{
	return readl(i2c->pdmabase + offset);
}

static inline void i2c_writel_dma(u32 value, struct mt_i2c *i2c, u8 offset)
{
	writel(value, i2c->pdmabase + offset);
}

static inline u16 i2c_readw(struct mt_i2c *i2c, u8 offset)
{
	return readw(i2c->base + offset);
}

static inline void i2c_writew(u16 value, struct mt_i2c *i2c, u8 offset)
{
	writew(value, i2c->base + offset);
}

static void ccu_record_i2c_dma_info(struct mt_i2c *i2c)
{
	g_dma_reg.base = (unsigned long)i2c->pdmabase;
	g_dma_reg.int_flag = i2c_readl_dma(i2c, OFFSET_INT_FLAG);
	g_dma_reg.int_en = i2c_readl_dma(i2c, OFFSET_INT_EN);
	g_dma_reg.en = i2c_readl_dma(i2c, OFFSET_EN);
	g_dma_reg.rst = i2c_readl_dma(i2c, OFFSET_RST);
	g_dma_reg.stop = i2c_readl_dma(i2c, OFFSET_STOP);
	g_dma_reg.flush = i2c_readl_dma(i2c, OFFSET_FLUSH);
	g_dma_reg.con = i2c_readl_dma(i2c, OFFSET_CON);
	g_dma_reg.tx_mem_addr = i2c_readl_dma(i2c, OFFSET_TX_MEM_ADDR);
	g_dma_reg.rx_mem_addr = i2c_readl_dma(i2c, OFFSET_RX_MEM_ADDR);
	g_dma_reg.tx_len = i2c_readl_dma(i2c, OFFSET_TX_LEN);
	g_dma_reg.rx_len = i2c_readl_dma(i2c, OFFSET_RX_LEN);
	g_dma_reg.int_buf_size = i2c_readl_dma(i2c, OFFSET_INT_BUF_SIZE);
	g_dma_reg.debug_sta = i2c_readl_dma(i2c, OFFSET_DEBUG_STA);
	g_dma_reg.tx_mem_addr2 = i2c_readl_dma(i2c, OFFSET_TX_MEM_ADDR2);
	g_dma_reg.rx_mem_addr2 = i2c_readl_dma(i2c, OFFSET_RX_MEM_ADDR2);
}

static void ccu_i2c_dump_info(struct mt_i2c *i2c)
{
	/* I2CFUC(); */
	/* int val=0; */
}

/*do i2c apdma warm reset & re-write dma buf addr, txlen*/
static int ccu_reset_i2c_apdma(struct mt_i2c *i2c)
{
	i2c_writel_dma(I2C_DMA_WARM_RST, i2c, OFFSET_RST);

#ifdef CONFIG_MTK_LM_MODE
	if ((i2c->dev_comp->dma_support == 1) && (enable_4G())) {
		i2c_writel_dma(0x1, i2c, OFFSET_TX_MEM_ADDR2);
		i2c_writel_dma(0x1, i2c, OFFSET_RX_MEM_ADDR2);
	}
#endif

	i2c_writel_dma(I2C_DMA_INT_FLAG_NONE, i2c, OFFSET_INT_FLAG);
	i2c_writel_dma(I2C_DMA_CON_TX, i2c, OFFSET_CON);
	i2c_writel_dma((u32) i2c->dma_buf.paddr, i2c, OFFSET_TX_MEM_ADDR);
	if ((i2c->dev_comp->dma_support >= 2))
		i2c_writel_dma(upper_32_bits(i2c->dma_buf.paddr),
			i2c, OFFSET_TX_MEM_ADDR2);

	/*write ap mda tx len = 128(must > totoal tx len within a frame)*/
	i2c_writel_dma(CCU_I2C_APDMA_TXLEN, i2c, OFFSET_TX_LEN);

	return 0;
}
