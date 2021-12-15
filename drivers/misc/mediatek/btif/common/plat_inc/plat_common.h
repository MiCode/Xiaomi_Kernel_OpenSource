/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __HAL_PUB_H_
#define __HAL_PUB_H_

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>

#ifdef CONFIG_OF
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of.h>
#else
#include <mach/mt_reg_base.h>
#include <mach/mt_irq.h>
#endif
#if defined(CONFIG_MTK_CLKMGR)
#include <mach/mt_clkmgr.h>
#else
#include <linux/clk.h>
#include <linux/platform_device.h>
#endif /* defined(CONFIG_MTK_CLKMGR) */

extern int mtk_btif_hal_get_log_lvl(void);

#define MTK_BTIF_MARK_UNUSED_API
#define MTK_BTIF_ENABLE_CLK_CTL 1
#define MTK_BTIF_ENABLE_CLK_REF_COUNTER 1

#define DBG_LOG_STR_SIZE 256

/*Log defination*/
static int hal_log_print(const char *str, ...)
{
	va_list args;
	int ret;
	char temp_sring[DBG_LOG_STR_SIZE];

	va_start(args, str);
	ret = vsnprintf(temp_sring, DBG_LOG_STR_SIZE, str, args);
	va_end(args);

	if (ret > 0)
		pr_info("%s", temp_sring);

	return ret;
}

#define BTIF_LOG_LOUD    4
#define BTIF_LOG_DBG     3
#define BTIF_LOG_INFO    2
#define BTIF_LOG_WARN    1
#define BTIF_LOG_ERR     0

#ifndef DFT_TAG
#define DFT_TAG         "[BTIF-DFT]"
#endif

#define BTIF_LOUD_FUNC(fmt, arg ...) \
do { \
	if (mtk_btif_hal_get_log_lvl() >= BTIF_LOG_LOUD) \
		hal_log_print(DFT_TAG "[L]%s:"  fmt, \
		__func__, ## arg); \
} while (0)

#define BTIF_INFO_FUNC(fmt, arg ...) \
do { \
	if (mtk_btif_hal_get_log_lvl() >= BTIF_LOG_INFO)\
		hal_log_print(DFT_TAG "[I]%s:"  fmt, \
		__func__, ## arg); \
} while (0)

#define BTIF_WARN_FUNC(fmt, arg ...) \
do { \
	if (mtk_btif_hal_get_log_lvl() >= BTIF_LOG_WARN)\
		hal_log_print(DFT_TAG "[W]%s:"  fmt, \
		__func__, ## arg); \
} while (0)

#define BTIF_ERR_FUNC(fmt, arg ...)\
do {\
	if (mtk_btif_hal_get_log_lvl() >= BTIF_LOG_ERR)\
		hal_log_print(DFT_TAG "[E]%s(%d):"  fmt,\
		__func__, __LINE__, ## arg);\
} while (0)

#define BTIF_DBG_FUNC(fmt, arg ...) \
do { \
	if (mtk_btif_hal_get_log_lvl() >= BTIF_LOG_DBG) \
		hal_log_print(DFT_TAG "[D]%s:"  fmt, \
		__func__, ## arg); \
} while (0)

#define BTIF_TRC_FUNC(f) \
do { \
	if (mtk_btif_hal_get_log_lvl() >= BTIF_LOG_DBG) \
		hal_log_print(DFT_TAG "<%s> <%d>\n", \
		__func__, __LINE__); \
} while (0)

/*-------------------------------Enum Defination----------------------------*/
/*IRQ sensetive type */
enum _ENUM_IRQ_SENS_TYPE_ {
	IRQ_SENS_EDGE = 0,
	IRQ_SENS_LVL = IRQ_SENS_EDGE + 1,
	IRQ_SENS_TYPE_MAX
};

/*IRQ level trigger type */
enum _ENUM_IRQ_LVL_TYPE_ {
	IRQ_LVL_LOW = 0,
	IRQ_LVL_HIGH = IRQ_LVL_LOW + 1,
	IRQ_LVL_MAX
};

/*IRQ edge trigger type */
enum _ENUM_IRQ_EDGE_TYPE_ {
	IRQ_EDGE_FALL = 0,
	IRQ_EDGE_RAISE = IRQ_EDGE_FALL + 1,
	IRQ_EDGE_BOTH = IRQ_EDGE_RAISE + 1,
	IRQ_EDGE_MAX
};

enum _ENUM_CLOCK_CTRL_ {
	CLK_OUT_DISABLE = 0,
	CLK_OUT_ENABLE = CLK_OUT_DISABLE + 1,
	CLK_OUT_MAX
};

/*Error No. table */
enum _ENUM_ERROR_CODE_ {
	ERR_NO_ERROR = 0,
	ERR_INVALID_PAR = ERR_NO_ERROR - 1,
	ERR_MAX = ERR_INVALID_PAR - 1,
};

enum _ENUM_BTIF_DIR_ {
	BTIF_TX = 0,
	BTIF_RX = BTIF_TX + 1,
	BTIF_DIR_MAX,
};

enum _ENUM_DMA_DIR_ {
	DMA_DIR_RX = 0,
	DMA_DIR_TX = DMA_DIR_RX + 1,
	DMA_DIR_BOTH,
};

enum _ENUM_BTIF_REG_ID_ {
	REG_IIR = 0,		/*Interrupt Identification Register */
	REG_LSR = 1,		/*Line Status Register */
	REG_FAKE_LCR = 2,	/*Fake Lcr Regiseter */
	REG_FIFO_CTRL = 3,	/*FIFO Control Register */
	REG_IER = 4,		/*Interrupt Enable Register */
	REG_SLEEP_EN = 5,	/*Sleep Enable Register */
	REG_RTO_COUNTER = 6,	/*Rx Timeout Counter Register */
	REG_DMA_EN = 7,		/*DMA Enalbe Register */
	REG_TRIG_LVL = 8,	/*Tx/Rx Trigger Level Register */
	REG_WAT_TIME = 9,	/*Async Wait Time Register */
	REG_HANDSHAKE = 10,	/*New HandShake Mode Register */
	REG_SLP_WAK = 11,	/*Sleep Wakeup Reigster */
	REG_ALL = 12,	/*all  registers */
	REG_IRQ = 13,	/*IRQ  registers */
	REG_MAX
};

enum _MTK_BTIF_PM_OPID_ {
	BTIF_PM_DPIDLE_EN,
	BTIF_PM_DPIDLE_DIS,
	BTIF_PM_SUSPEND,
	BTIF_PM_RESUME,
	BTIF_PM_RESTORE_NOIRQ,
};

#define BTIF_HAL_TX_FIFO_SIZE (1024 * 4)

/*----------------------------Enum Defination End---------------------------*/

/*****************************structure definition***************************/
/*IRQ related information*/
struct _MTK_BTIF_IRQ_STR_ {
	const char *name;
	bool is_irq_sup;
	unsigned int irq_id;
#ifdef CONFIG_OF
	unsigned int irq_flags;
#else
	enum _ENUM_IRQ_SENS_TYPE_ sens_type;
	union {
		enum _ENUM_IRQ_LVL_TYPE_ lvl_type;
		enum _ENUM_IRQ_EDGE_TYPE_ edge_type;
	};
#endif
	bool reg_flag;
	irq_handler_t p_irq_handler;
};

struct _DMA_VFIFO_ {
	/*[Driver Access] vFIFO memory'svirtual address */
	unsigned char *p_vir_addr;
	/*[HW Access] dma handle, physical address, set to DMA's HW Register */
	dma_addr_t phy_addr;
	/*DMA's vFIFO size */
	unsigned int vfifo_size;
	/*DMA's threshold value */
	unsigned int thre;
};

struct _MTK_DMA_INFO_STR_;
struct _MTK_BTIF_INFO_STR_;

typedef unsigned int (*dma_rx_buf_write) (
				struct _MTK_DMA_INFO_STR_ *p_dma_info,
				unsigned char *p_buf,
				unsigned int buf_len);
typedef unsigned int (*btif_rx_buf_write) (
				struct _MTK_BTIF_INFO_STR_ *p_btif_info,
				unsigned char *p_buf,
				unsigned int buf_len);

/*DMA related information*/
struct _MTK_DMA_INFO_STR_ {
	unsigned long base;
	enum _ENUM_DMA_DIR_ dir;
	struct _MTK_BTIF_IRQ_STR_ *p_irq;
	dma_rx_buf_write rx_cb;
	struct _DMA_VFIFO_ *p_vfifo;
};

/*DMA related information*/
struct _MTK_BTIF_INFO_STR_ {
	unsigned long base;	/*base address */
	struct _MTK_BTIF_IRQ_STR_ *p_irq;	/*irq related information */

	unsigned int tx_fifo_size;	/*BTIF tx FIFO size */
	unsigned int rx_fifo_size;	/*BTIF rx FIFO size */

	unsigned int tx_tri_lvl;	/*BTIFtx trigger level in FIFO mode */
	unsigned int rx_tri_lvl;	/*BTIFrx trigger level in FIFO mode */

	unsigned int clk_gat_addr;	/*clock gating address */
	unsigned int set_bit;	/*enable clock gating bit */
	unsigned int clr_bit;	/*clear clock gating bit */

	unsigned int rx_data_len;	/*rx data length */

	btif_rx_buf_write rx_cb;

	struct kfifo *p_tx_fifo;	/*tx fifo */
	spinlock_t tx_fifo_spinlock;	/*tx fifo spinlock */
	};

/**********End of Structure Definition***********/

/***********register operation***********/
#ifdef __KERNEL__
/*byte write  <1 byte> */
#define btif_reg_sync_writeb(v, a) \
do { \
	writeb(v, (void __iomem *)a); \
	/* call mb () */ \
	mb(); \
} while (0)
/*word write  <2 byte> */
#define btif_reg_sync_writew(v, a) \
do { \
	writew(v, (void __iomem *)a); \
	/* call mb () */ \
	mb(); \
} while (0)
/*long write   <4 byte> */
#define btif_reg_sync_writel(v, a) \
do { \
	writel(v, (void __iomem *)a); \
	/* call mb () */ \
	mb(); \
} while (0)
#else
/*byte write  <1 byte> */
#define btif_reg_sync_writeb(v, a)    mt65xx_reg_sync_writeb(v, a)
/*word write  <2 byte> */
#define btif_reg_sync_writew(v, a)    mt65xx_reg_sync_writew(v, a)
/*long write   <4 byte> */
#define btif_reg_sync_writel(v, a)    mt65xx_reg_sync_writel(v, a)
#endif
#define BTIF_READ8(REG)               __raw_readb((unsigned char *)(REG))
#define BTIF_READ16(REG)              __raw_readw((unsigned short *)(REG))
#define BTIF_READ32(REG)              __raw_readl((unsigned int *)(REG))

#define BTIF_SET_BIT(REG, BITVAL)    do { \
writel(readl((unsigned int *)(REG)) | ((unsigned int)(BITVAL)), \
(unsigned int *)(REG)); \
mb(); /**/ \
} \
while (0)
#define BTIF_CLR_BIT(REG, BITVAL)    do { \
writel(readl((unsigned int *)(REG)) & ~((unsigned int)(BITVAL)), \
(unsigned int *)(REG)); \
mb(); /**/\
} \
while (0)

/***********end of register operation *********/

#endif /*__HAL_PUB_H_*/
