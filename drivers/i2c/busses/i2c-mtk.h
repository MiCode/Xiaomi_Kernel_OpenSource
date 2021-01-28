/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __I2C_MTK_H__
#define __I2C_MTK_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/clk.h>

#define I2C_DEBUG_FS

#define I3C_EN                           (0x01 << 15)
#define I3C_UNLOCK_HFIFO                (0x01 << 15)
#define I3C_NINTH_BIT                   (0x02 << 8)
#define MASTER_CODE                     0x08
#define I2C_HFIFO_ADDR_CLR              0x2

#define I2C_HS_HOLD_SEL                 (0x01 << 15)
#define I2C_HS_HOLD_TIME                (0x01 << 2)

#define I2C_BUS_ERR			(0x01 << 8)
#define I2C_IBI				(0x01 << 7)
#define I2C_DMAERR			(0x01 << 6)
#define I2C_TIMEOUT			(0x01 << 5)
#define I2C_RS_MULTI		(0x01 << 4)
#define I2C_ARB_LOST		(0x01 << 3)
#define I2C_HS_NACKERR		(0x01 << 2)
#define I2C_ACKERR			(0x01 << 1)
#define I2C_TRANSAC_COMP	(0x01 << 0)
#define I2C_INTR_ALL	(I2C_BUS_ERR | I2C_IBI | I2C_DMAERR | \
				I2C_TIMEOUT | I2C_RS_MULTI | \
				I2C_ARB_LOST | I2C_HS_NACKERR | \
				I2C_ACKERR | I2C_TRANSAC_COMP)
#define I2C_TRANSAC_START					(0x01 << 0)
#define I2C_RESUME_ARBIT					(0x01 << 1)
#define I2C_TIMING_STEP_DIV_MASK			(0x3f << 0)
#define I2C_TIMING_SAMPLE_COUNT_MASK		(0x7 << 0)
#define I2C_TIMING_SAMPLE_DIV_MASK		(0x7 << 8)
#define I2C_TIMING_DATA_READ_MASK		(0x7 << 12)
#define I2C_DCM_DISABLE					0x0000
#define I2C_DCM_ENABLE						0x0007
#define I2C_IO_CONFIG_OPEN_DRAIN			0x0003
#define I2C_IO_CONFIG_PUSH_PULL			0x0000
#define I2C_IO_CONFIG_OPEN_DRAIN_AED		0x0000
#define I2C_IO_CONFIG_PUSH_PULL_AED		0x0000
#define I2C_IO_CONFIG_AED_MASK			(0xfff << 4)
#define I2C_SOFT_RST				0x0001
#define I2C_FIFO_ADDR_CLR			0x0001
#define I2C_FIFO_ADDR_CLR_MCH		0x0004
#define I2C_DELAY_LEN				0x000A/* not use 0x02 */
#define I2C_ST_START_CON			0x8001
#define I2C_FS_START_CON			0x1800
#define I2C_FS_PLUS_START_CON                   0xa0f
#define I2C_TIME_CLR_VALUE			0x0000
#define I2C_TIME_DEFAULT_VALUE		0x0001
#define I2C_HS_SPEED			0x0080
#define I2C_TIMEOUT_EN				0x0001
#define I2C_ROLLBACK				0x0001
#define I2C_SHADOW_REG_MODE		0x0002

#define I2C_HS_NACK_DET_EN		(0x1 << 1)

#define I2C_DMA_CON_TX			0x0000
#define I2C_DMA_CON_RX			0x0001
#define I2C_DMA_START_EN		0x0001
#define I2C_DMA_INT_FLAG_NONE	0x0000
#define I2C_DMA_CLR_FLAG		0x0000
#define I2C_DMA_WARM_RST		0x0001
#define I2C_DMA_4G_MODE		0x0001

#define I2C_DMA_DIR_CHANGE              (0x1 << 9)
#define I2C_DMA_SKIP_CONFIG             (0x1 << 4)
#define I2C_DMA_ASYNC_MODE              (0x1 << 2)

#define I2C_DEFAUT_SPEED		100000/* hz */
#define MAX_FS_MODE_SPEED		400000/* hz */
#define MAX_FS_PLUS_MODE_SPEED          1000000/* hz */
#define MAX_HS_MODE_SPEED		3400000/* hz */
#define MAX_DMA_TRANS_SIZE	4096/* 255 */
#define MAX_CLOCK_DIV			8
#define MAX_SAMPLE_CNT_DIV	8
#define MAX_STEP_CNT_DIV		64
#define MAX_HS_STEP_CNT_DIV	8

#define HALF_DUTY_CYCLE		50
#define DUTY_CYCLE				45

#define I2C_CONTROL_RS				(0x1 << 1)
#define I2C_CONTROL_DMA_EN		(0x1 << 2)
#define I2C_CONTROL_CLK_EXT_EN	(0x1 << 3)
#define I2C_CONTROL_DIR_CHANGE	(0x1 << 4)
#define I2C_CONTROL_ACKERR_DET_EN	(0x1 << 5)
#define I2C_CONTROL_TRANSFER_LEN_CHANGE (0x1 << 6)
#define I2C_CONTROL_IRQ_SEL     (0x1 << 7)
#define I2C_CONTROL_DMAACK_EN	(0x1 << 8)
#define I2C_CONTROL_ASYNC_MODE	(0x1 << 9)
#define I2C_CONTROL_WRAPPER		(0x1 << 0)
#define I2C_MCU_INTR_EN			0x1
#define I2C_CCU_INTR_EN			0x2

#define I2C_RECORD_LEN			10
#define I2C_MAX_CHANNEL		16

#define MAX_SCL_LOW_TIME		2/* unit: milli-second */
#define LSAMPLE_MSK			0x1C0
#define LSTEP_MSK				0x3F

#define I2C_DRV_NAME		"mt-i2c"
#define I2CTAG					"[I2C]"

enum {
	DMA_HW_VERSION0 = 0,
	DMA_HW_VERSION1 = 1,
	MDA_SUPPORT_8G  = 2,
	DMA_SUPPORT_64G = 3,
};

enum DMA_REGS_OFFSET {
	OFFSET_INT_FLAG = 0x0,
	OFFSET_INT_EN = 0x04,
	OFFSET_EN = 0x08,
	OFFSET_RST = 0x0C,
	OFFSET_STOP = 0x10,
	OFFSET_FLUSH = 0x14,
	OFFSET_CON = 0x18,
	OFFSET_TX_MEM_ADDR = 0x1C,
	OFFSET_RX_MEM_ADDR = 0x20,
	OFFSET_TX_LEN = 0x24,
	OFFSET_RX_LEN = 0x28,
	OFFSET_INT_BUF_SIZE = 0x38,
	OFFSET_DEBUG_STA = 0x50,
	OFFSET_TX_MEM_ADDR2 = 0x54,
	OFFSET_RX_MEM_ADDR2 = 0x58,
	OFFSET_USR_DEF_ADDR = 0x5C,
	OFFSET_USR_DEF_CTRL = 0x60,
};

struct i2c_dma_info {
	unsigned long base;
	unsigned int int_flag;
	unsigned int int_en;
	unsigned int en;
	unsigned int rst;
	unsigned int stop;
	unsigned int flush;
	unsigned int con;
	unsigned int tx_mem_addr;
	unsigned int rx_mem_addr;
	unsigned int tx_len;
	unsigned int rx_len;
	unsigned int int_buf_size;
	unsigned int debug_sta;
	unsigned int tx_mem_addr2;
	unsigned int rx_mem_addr2;
	unsigned int usr_def_addr;
	unsigned int use_def_addr;
};

enum i2c_trans_st_rs {
	I2C_TRANS_STOP = 0,
	I2C_TRANS_REPEATED_START,
};

enum {
	FS_MODE,
	HS_MODE,
};

enum mt_trans_op {
	I2C_MASTER_WR = 1,
	I2C_MASTER_RD,
	I2C_MASTER_WRRD,
	I2C_MASTER_MULTI_WR,
};

enum I2C_REGS_OFFSET {
	OFFSET_DATA_PORT = 0x0,
	OFFSET_SLAVE_ADDR = 0x04,
	OFFSET_INTR_MASK = 0x08,
	OFFSET_INTR_STAT = 0x0c,
	OFFSET_CONTROL = 0x10,
	OFFSET_TRANSFER_LEN = 0x14,
	OFFSET_TRANSAC_LEN = 0x18,
	OFFSET_DELAY_LEN = 0x1c,
	OFFSET_TIMING = 0x20,
	OFFSET_START = 0x24,
	OFFSET_EXT_CONF = 0x28,
	OFFSET_LTIMING = 0x2c,
	OFFSET_FIFO_STAT = 0x30,
	OFFSET_FIFO_THRESH = 0x34,
	OFFSET_FIFO_ADDR_CLR = 0x38,
	OFFSET_IO_CONFIG = 0x40,
	OFFSET_RSV_DEBUG = 0x44,
	OFFSET_HS = 0x48,
	OFFSET_SOFTRESET = 0x50,
	OFFSET_DCM_EN = 0x54,
	OFFSET_PATH_DIR = 0x60,
	OFFSET_DEBUGSTAT = 0x64,
	OFFSET_DEBUGCTRL = 0x68,
	OFFSET_TRANSFER_LEN_AUX = 0x6c,
	OFFSET_CLOCK_DIV = 0x70,

	/* v2 add */
	OFFSET_HW_TIMEOUT = 0xfff,
	OFFSET_MCU_INTR = 0xfff,
	OFFSET_TRAFFIC = 0xfff,
	OFFSET_COMMAND = 0xfff,
	OFFSET_CRC_CODE_ = 0xfff,
	OFFSET_TERNARY = 0xfff,
	OFFSET_IBI_TIMING = 0xfff,
	OFFSET_SHAPE = 0xfff,
	OFFSET_HFIFO_DATA = 0xfff,
	OFFSET_ERROR = 0xfff,
	OFFSET_DELAY_STEP = 0xfff,
	OFFSET_DELAY_SAMPLE = 0xfff,
	OFFSET_DMA_INFO = 0xfff,
	OFFSET_IRQ_INFO = 0xfff,
	OFFSET_DMA_FSM_DEBUG = 0xfff,
	OFFSET_HFIFO_STAT = 0xfff,
	OFFSET_MULTI_DMA = 0xfff,
	OFFSET_ROLLBACK = 0xfff,
};

enum I2C_REGS_OFFSET_V2 {
	V2_OFFSET_DATA_PORT = 0x0,
	V2_OFFSET_SLAVE_ADDR = 0x04,
	V2_OFFSET_INTR_MASK = 0x08,
	V2_OFFSET_INTR_STAT = 0x0c,
	V2_OFFSET_CONTROL = 0x10,
	V2_OFFSET_TRANSFER_LEN = 0x14,
	V2_OFFSET_TRANSAC_LEN = 0x18,
	V2_OFFSET_DELAY_LEN = 0x1c,
	V2_OFFSET_TIMING = 0x20,
	V2_OFFSET_START = 0x24,
	V2_OFFSET_EXT_CONF = 0x28,
	V2_OFFSET_LTIMING = 0x2c,
	V2_OFFSET_FIFO_ADDR_CLR = 0x38,
	V2_OFFSET_SOFTRESET = 0x50,

	/* v2 use different offset */
	V2_OFFSET_HS = 0x30,
	V2_OFFSET_IO_CONFIG = 0x34,
	V2_OFFSET_TRANSFER_LEN_AUX = 0x44,
	V2_OFFSET_CLOCK_DIV = 0x48,
	V2_OFFSET_HW_TIMEOUT = 0x4c,
	V2_OFFSET_DEBUGSTAT = 0xe4,
	V2_OFFSET_DEBUGCTRL = 0xe8,
	V2_OFFSET_FIFO_STAT = 0xf4,
	V2_OFFSET_FIFO_THRESH = 0xf8,
	V2_OFFSET_AED_PATCH = 0x80,

	/* v2 add */
	V2_OFFSET_MCU_INTR = 0x40,
	V2_OFFSET_TRAFFIC = 0x54,
	V2_OFFSET_COMMAND = 0x58,
	V2_OFFSET_CRC_CODE_ = 0x5c,
	V2_OFFSET_TERNARY = 0x60,
	V2_OFFSET_IBI_TIMING = 0x64,
	V2_OFFSET_SHAPE = 0x6c,
	V2_OFFSET_HFIFO_DATA = 0x70,
	V2_OFFSET_ERROR = 0x84,
	V2_OFFSET_DELAY_STEP = 0xd4,
	V2_OFFSET_DELAY_SAMPLE = 0xd8,
	V2_OFFSET_DMA_INFO = 0xdc,
	V2_OFFSET_IRQ_INFO = 0xe0,
	V2_OFFSET_DMA_FSM_DEBUG = 0xec,
	V2_OFFSET_HFIFO_STAT = 0xfc,
	V2_OFFSET_MULTI_DMA = 0xf8c,
	V2_OFFSET_ROLLBACK = 0xf98,

	/* not in v2 */
	V2_OFFSET_DCM_EN = 0xfff,/*0x54*/
	V2_OFFSET_PATH_DIR = 0xfff,/*0x60*/
};

struct i2c_info {
	unsigned int slave_addr;
	unsigned int intr_stat;
	unsigned int control;
	unsigned int fifo_stat;
	unsigned int debug_stat;
	unsigned int tmo;
	unsigned long long end_time;
};

enum PERICFG_OFFSET {
	OFFSET_PERI_I2C_MODE_ENABLE = 0x0410,
};

struct mt_i2c_data {
	unsigned int clk_frequency;	/* bus speed in Hz */
	unsigned int flags;
	unsigned int clk_src_div;
};

struct i2c_dma_buf {
	u8 *vaddr;
	dma_addr_t paddr;
};

struct mt_i2c_ext {
#define I2C_A_FILTER_MSG	0x00000001
	bool isEnable;
	bool isFilterMsg;
	bool is_ch_offset;
	u32 timing;
	u16 ch_offset;
	u16 ch_offset_dma;
};

struct mtk_i2c_compatible {
	unsigned char dma_support;
	/* 0 : original; 1: 4gb  support 2: 33bit support; 3: 36 bit support */
	unsigned char idvfs_i2c;
	/* compatible before chip, set 1 if no TRANSFER_LEN_AUX */
	unsigned char set_dt_div;/* use dt to set div */
	unsigned char check_max_freq;/* check max freq */
	unsigned char set_ltiming;/* need to set LTIMING */
	unsigned char set_aed;/* need to set AED */
	unsigned char ver;/* controller version */
	unsigned char dma_ver;/* dma controller version */
	/* for constraint of SAMPLE_CNT_DIV and STEP_CNT_DIV of mt6765 */
	/* 1, has-a-constraint; 0, no constraint */
	unsigned char cnt_constraint;
	/* only for MT6768 */
	/* this option control defined when nack error or ack error occurs */
	/* 0 : disable, 1 : enable*/
	unsigned char control_irq_sel;
	u16 ext_time_config;
	char clk_compatible[128];
	u16 clk_sta_offset[I2C_MAX_CHANNEL];/* I2C clock status register */
	u8 cg_bit[I2C_MAX_CHANNEL];/* i2c clock bit */
	u32 clk_sel_offset;
	u32 arbit_offset;
};

struct mt_i2c {
	struct i2c_adapter adap;/* i2c host adapter */
	struct device *dev;
	wait_queue_head_t wait;/* i2c transfer wait queue */
	/* set in i2c probe */
	void __iomem *base;/* i2c base addr */
	void __iomem *pdmabase;/* dma base address*/
	void __iomem *gpiobase;/* gpio base address */
	int irqnr;	/* i2c interrupt number */
	unsigned int id;
	int scl_gpio_id; /* SCL GPIO number */
	int sda_gpio_id; /* SDA GPIO number */
	unsigned int gpio_start;
	unsigned int mem_len;
	unsigned int offset_eh_cfg;
	unsigned int offset_pu_cfg;
	unsigned int offset_rsel_cfg;
	struct i2c_dma_buf dma_buf;/* memory alloc for DMA mode */
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
	enum mt_trans_op op;
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
	unsigned int rec_idx;/* next record idx */
	u32 ch_offset_default;
	u32 ch_offset;
	u32 ch_offset_dma_default;
	u32 ch_offset_dma;
	bool skip_scp_sema;
	bool has_ccu;
	u32 ccu_offset;
	unsigned long main_clk;
	struct mutex i2c_mutex;
	struct mt_i2c_ext ext_data;
	const struct mtk_i2c_compatible *dev_comp;
	struct i2c_info rec_info[I2C_RECORD_LEN];
};

#if defined(CONFIG_MTK_FPGA) || defined(CONFIG_FPGA_EARLY_PORTING)
#define CONFIG_MT_I2C_FPGA_ENABLE
#endif

#if (defined(CONFIG_MT_I2C_FPGA_ENABLE))
#define FPGA_CLOCK		12000/* FPGA crystal frequency (KHz) */
#define I2C_CLK_DIV		(5)/* frequency divider */
#define I2C_CLK_RATE	((FPGA_CLOCK / I2C_CLK_DIV) * 1000)
/* Hz for FPGA I2C work frequency */
#endif

extern void gpio_dump_regs_range(int start, int end);
extern void i2c_dump_info(struct mt_i2c *i2c);
#if defined(CONFIG_MTK_GIC_EXT)
extern void mt_irq_dump_status(unsigned int irq);
#endif
extern unsigned int enable_4G(void);
extern int mtk_i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs,
					int num, u32 ext_flag, u32 timing);
extern void mt_irq_dump_status(unsigned int irq);
extern int hw_trig_i2c_enable(struct i2c_adapter *adap);
extern int hw_trig_i2c_disable(struct i2c_adapter *adap);
extern int hw_trig_i2c_transfer(struct i2c_adapter *adap,
					struct i2c_msg *msgs, int num);
extern int i2c_ccu_enable(struct i2c_adapter *adap, u16 ch_offset);
extern int i2c_ccu_disable(struct i2c_adapter *adap);

#endif
