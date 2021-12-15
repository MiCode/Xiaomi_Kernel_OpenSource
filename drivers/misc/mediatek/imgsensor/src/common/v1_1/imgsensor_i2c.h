/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __IMGSENSOR_I2C_H__
#define __IMGSENSOR_I2C_H__

#include <linux/i2c.h>
#include <linux/mutex.h>

#ifndef NO_I2C_MTK
#include "i2c-mtk.h"
#else
#define mtk_i2c_transfer(adap, msgs, num, ext_flag, timing) \
	i2c_transfer(adap, msgs, num)
struct mt_i2c {
	struct i2c_adapter adap;/* i2c host adapter */
	struct device *dev;
	wait_queue_head_t wait;/* i2c transfer wait queue */
	/* set in i2c probe */
	void __iomem *base;/* i2c base addr */
	void __iomem *pdmabase;/* dma base address*/
	void __iomem *gpiobase;/* gpio base address */
	int irqnr;	/* i2c interrupt number */
	int id;
	int scl_gpio_id; /* SCL GPIO number */
	int sda_gpio_id; /* SDA GPIO number */
	unsigned int gpio_start;
	unsigned int mem_len;
	unsigned int offset_eh_cfg;
	unsigned int offset_pu_cfg;
	unsigned int offset_rsel_cfg;
	// struct i2c_dma_buf dma_buf;/* memory alloc for DMA mode */
	struct clk *clk_main;/* main clock for i2c bus */
	struct clk *clk_dma;/* DMA clock for i2c via DMA */
	struct clk *clk_pmic;/* PMIC clock for i2c from PMIC */
	struct clk *clk_arb;/* Arbitrator clock for i2c */
	struct clk *clk_pal;
	bool have_pmic;/* can use i2c pins form PMIC */
	bool have_dcm;/* HW DCM function */
	bool use_push_pull;/* IO config push-pull mode */
	bool appm;/* I2C for APPM */
	bool gpupm;/* I2C for GPUPM */
	bool buffermode;	/* I2C Buffer mode support */
	bool hs_only;	/* I2C HS only */
	bool fifo_only;  /* i2c fifo mode only, does not have dma HW support */
	/* set when doing the transfer */
	u16 irq_stat;	/* interrupt status */
	u16 i3c_en;     /* i3c enalbe */
	unsigned int speed_hz;/* The speed in transfer */
	unsigned int clk_src_div;
	unsigned int aed;/* aed value from dt */
	spinlock_t cg_lock;
	int cg_cnt;
	bool trans_stop;/* i2c transfer stop */
	// enum mt_trans_op op;
	u16 total_len;
	u16 msg_len;
	u8 *msg_buf;	/* pointer to msg data */
	u16 msg_aux_len;/* WRRD mode to set AUX_LEN register */
	u16 addr;/* 7bit slave address, without read/write bit */
	u16 timing_reg;
	u16 ltiming_reg;
	u16 high_speed_reg;
	u16 clk_sta_offset;
	u8 cg_bit;
	bool is_hw_trig;
	bool is_ccu_trig;
	bool suspended;
	int rec_idx;/* next record idx */
	u32 ch_offset_default;
	u32 ch_offset;
	u32 ch_offset_dma_default;
	u32 ch_offset_dma;
	bool skip_scp_sema;
	bool has_ccu;
	u32 apdma_size;
	u32 ccu_offset;
	unsigned long main_clk;
	struct mutex i2c_mutex;
	// struct mt_i2c_ext ext_data;
	const struct mtk_i2c_compatible *dev_comp;
	struct mtk_i2c_pll *i2c_pll_info;
	// struct i2c_info rec_info[I2C_RECORD_LEN];
};
#endif

#include "imgsensor_cfg_table.h"
#include "imgsensor_common.h"

#define IMGSENSOR_I2C_MSG_SIZE_READ      2
#define IMGSENSOR_I2C_BURST_WRITE_LENGTH MAX_DMA_TRANS_SIZE
#define IMGSENSOR_I2C_CMD_LENGTH_MAX     255

#define IMGSENSOR_I2C_BUFF_MODE_DEV      IMGSENSOR_I2C_DEV_2

#ifdef IMGSENSOR_I2C_1000K
#define IMGSENSOR_I2C_SPEED              1000
#else
#define IMGSENSOR_I2C_SPEED              400
#endif

struct IMGSENSOR_I2C_STATUS {
	u8 reserved:7;
	u8 filter_msg:1;
};

struct IMGSENSOR_I2C_INST {
	struct IMGSENSOR_I2C_STATUS status;
	struct i2c_client          *pi2c_client;
};

struct IMGSENSOR_I2C_CFG {
	struct IMGSENSOR_I2C_INST *pinst;
	struct i2c_driver         *pi2c_driver;
	struct i2c_msg             msg[IMGSENSOR_I2C_CMD_LENGTH_MAX];
	struct mutex               i2c_mutex;
};

struct IMGSENSOR_I2C {
	struct IMGSENSOR_I2C_INST inst[IMGSENSOR_I2C_DEV_MAX_NUM];
};

enum IMGSENSOR_RETURN imgsensor_i2c_create(void);
enum IMGSENSOR_RETURN imgsensor_i2c_delete(void);
enum IMGSENSOR_RETURN imgsensor_i2c_init(
	struct IMGSENSOR_I2C_CFG *pi2c_cfg,
	enum IMGSENSOR_I2C_DEV device);
enum IMGSENSOR_RETURN imgsensor_i2c_buffer_mode(int enable);
enum IMGSENSOR_RETURN imgsensor_i2c_read(
	struct IMGSENSOR_I2C_CFG *pi2c_cfg,
	u8 *pwrite_data,
	u16 write_length,
	u8 *pread_data,
	u16 read_length,
	u16 id,
	int speed);
enum IMGSENSOR_RETURN imgsensor_i2c_write(
	struct IMGSENSOR_I2C_CFG *pi2c_cfg,
	u8 *pwrite_data,
	u16 write_length,
	u16 write_per_cycle,
	u16 id,
	int speed);

void imgsensor_i2c_filter_msg(struct IMGSENSOR_I2C_CFG *pi2c_cfg, bool en);

#ifdef IMGSENSOR_LEGACY_COMPAT
void imgsensor_i2c_set_device(struct IMGSENSOR_I2C_CFG *pi2c_cfg);
struct IMGSENSOR_I2C_CFG *imgsensor_i2c_get_device(void);
#endif

#endif

