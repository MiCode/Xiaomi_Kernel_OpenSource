/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Xudong.chen <xudong.chen@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

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
#include <mt_cpufreq_hybrid.h>
#ifdef CONFIG_MTK_GPU_SPM_DVFS_SUPPORT
#include <mtk_kbase_spm.h>
#endif
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#include <scp_helper.h>
#endif
#include "i2c-mtk.h"

static struct i2c_dma_info g_dma_regs[10];

static inline void i2c_writew(u16 value, struct mt_i2c *i2c, u8 offset)
{
	writew(value, i2c->base + offset);
}

static inline u16 i2c_readw(struct mt_i2c *i2c, u8 offset)
{
	return readw(i2c->base + offset);
}

void __iomem *infra_base;

s32 map_cg_regs(void)
{
	struct device_node *infrasys_node;

	infrasys_node = of_find_compatible_node(NULL, NULL, "mediatek,infracfg_ao");
	if (!infrasys_node) {
		pr_err("Cannot find infrasys_node\n");
		return -ENODEV;
	}
	infra_base = of_iomap(infrasys_node, 0);
	if (!infra_base) {
		pr_err("infra_base iomap failed\n");
		return -ENOMEM;
	}
	return 0;
}

void dump_cg_regs(void)
{
	pr_err("[I2C] cg regs dump:\n"
		"%8s : 0x%08x 0x%08x 0x%08x\n%8s : 0x%08x 0x%08x 0x%08x\n",
		"Address", 0x10001090, 0x10001094, 0x100010b0,
		"Values", readw(infra_base + 0x90), readw(infra_base + 0x94),
		readw(infra_base + 0xb0));
}

void __iomem *dma_base;

s32 map_dma_regs(void)
{
	struct device_node *dma_node;

	dma_node = of_find_compatible_node(NULL, NULL, "mediatek,ap_dma");
	if (!dma_node) {
		pr_err("Cannot find dma_node\n");
		return -ENODEV;
	}
	dma_base = of_iomap(dma_node, 0);
	if (!dma_base) {
		pr_err("dma_base iomap failed\n");
		return -ENOMEM;
	}
	return 0;
}

void dump_dma_regs(void)
{
	int status;
	int i;

	status =  readl(dma_base + 8);
	pr_err("DMA RUNNING STATUS : 0x%x .\n", status);
	for	(i = 0; i < 21 ; i++) {
		if (status & (0x1 << i))
			pr_err("DMA[%d] CONTROL REG : 0x%x, DEBUG : 0x%x .\n", i,
				readl(dma_base + 0x80 + 0x80 * i + 0x18),
				readl(dma_base + 0x80 + 0x80 * i + 0x50));
	}

}

static inline void i2c_writel_dma(u32 value, struct mt_i2c *i2c, u8 offset)
{
	writel(value, i2c->pdmabase + offset);
}

static inline u32 i2c_readl_dma(struct mt_i2c *i2c, u8 offset)
{
	return readl(i2c->pdmabase + offset);
}

static void record_i2c_dma_info(struct mt_i2c *i2c)
{
	g_dma_regs[i2c->id].base = (unsigned long)i2c->pdmabase;
	g_dma_regs[i2c->id].int_flag = i2c_readl_dma(i2c, OFFSET_INT_FLAG);
	g_dma_regs[i2c->id].int_en = i2c_readl_dma(i2c, OFFSET_INT_EN);
	g_dma_regs[i2c->id].en = i2c_readl_dma(i2c, OFFSET_EN);
	g_dma_regs[i2c->id].rst = i2c_readl_dma(i2c, OFFSET_RST);
	g_dma_regs[i2c->id].stop = i2c_readl_dma(i2c, OFFSET_STOP);
	g_dma_regs[i2c->id].flush = i2c_readl_dma(i2c, OFFSET_FLUSH);
	g_dma_regs[i2c->id].con = i2c_readl_dma(i2c, OFFSET_CON);
	g_dma_regs[i2c->id].tx_mem_addr = i2c_readl_dma(i2c, OFFSET_TX_MEM_ADDR);
	g_dma_regs[i2c->id].rx_mem_addr = i2c_readl_dma(i2c, OFFSET_RX_MEM_ADDR);
	g_dma_regs[i2c->id].tx_len = i2c_readl_dma(i2c, OFFSET_TX_LEN);
	g_dma_regs[i2c->id].rx_len = i2c_readl_dma(i2c, OFFSET_RX_LEN);
	g_dma_regs[i2c->id].int_buf_size = i2c_readl_dma(i2c, OFFSET_INT_BUF_SIZE);
	g_dma_regs[i2c->id].debug_sta = i2c_readl_dma(i2c, OFFSET_DEBUG_STA);
	g_dma_regs[i2c->id].tx_mem_addr2 = i2c_readl_dma(i2c, OFFSET_TX_MEM_ADDR2);
	g_dma_regs[i2c->id].rx_mem_addr2 = i2c_readl_dma(i2c, OFFSET_RX_MEM_ADDR2);
}

static int mt_i2c_clock_enable(struct mt_i2c *i2c)
{
#if !defined(CONFIG_MT_I2C_FPGA_ENABLE)
	int ret;

	ret = clk_prepare_enable(i2c->clk_dma);
	if (ret)
		return ret;

	if (i2c->clk_arb != NULL) {
		ret = clk_prepare_enable(i2c->clk_arb);
		if (ret)
			return ret;
	}
	ret = clk_prepare_enable(i2c->clk_main);
	if (ret)
		goto err_main;

	if (i2c->have_pmic) {
		ret = clk_prepare_enable(i2c->clk_pmic);
		if (ret)
			goto err_pmic;
	}
	return 0;

err_pmic:
	clk_disable_unprepare(i2c->clk_main);
err_main:
	clk_disable_unprepare(i2c->clk_dma);
	return ret;
#else
	return 0;
#endif
}

static void mt_i2c_clock_disable(struct mt_i2c *i2c)
{
#if !defined(CONFIG_MT_I2C_FPGA_ENABLE)
	if (i2c->have_pmic)
		clk_disable_unprepare(i2c->clk_pmic);

	clk_disable_unprepare(i2c->clk_main);
	if (i2c->clk_arb != NULL)
		clk_disable_unprepare(i2c->clk_arb);

	clk_disable_unprepare(i2c->clk_dma);
#endif
}

static int i2c_get_semaphore(struct mt_i2c *i2c)
{
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	int count = 100;
#endif

	if (i2c->appm) {
		if (cpuhvfs_get_dvfsp_semaphore(SEMA_I2C_DRV) != 0) {
			dev_err(i2c->dev, "sema time out 2ms\n");
			if (cpuhvfs_get_dvfsp_semaphore(SEMA_I2C_DRV) != 0) {
				dev_err(i2c->dev, "sema time out 4ms\n");
				i2c_dump_info(i2c);
				BUG_ON(1);
				return -EBUSY;
			}
		}
	}

#ifdef CONFIG_MTK_GPU_SPM_DVFS_SUPPORT
	if (i2c->gpupm) {
		if (dvfs_gpu_pm_spin_lock_for_vgpu() != 0) {
			dev_err(i2c->dev, "sema time out.\n");
			return -EBUSY;
		}
	}
#endif

	switch (i2c->id) {
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	case 0:
		while (1 != get_scp_semaphore(SEMAPHORE_I2C0) && count > 0)
			count--;
		return count > 0 ? 0 : -EBUSY;
	case 1:
		while (1 != get_scp_semaphore(SEMAPHORE_I2C1) && count > 0)
			count--;
		return count > 0 ? 0 : -EBUSY;
#endif
	default:
		return 0;
	}
}

static int i2c_release_semaphore(struct mt_i2c *i2c)
{
	if (i2c->appm)
		cpuhvfs_release_dvfsp_semaphore(SEMA_I2C_DRV);

#ifdef CONFIG_MTK_GPU_SPM_DVFS_SUPPORT
	if (i2c->gpupm)
		dvfs_gpu_pm_spin_unlock_for_vgpu();
#endif

	switch (i2c->id) {
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	case 0:
		return release_scp_semaphore(SEMAPHORE_I2C0) == 1 ? 0 : -EBUSY;
	case 1:
		return release_scp_semaphore(SEMAPHORE_I2C1) == 1 ? 0 : -EBUSY;
#endif
	default:
		return 0;
	}
}

static void free_i2c_dma_bufs(struct mt_i2c *i2c)
{
	dma_free_coherent(i2c->adap.dev.parent, PAGE_SIZE,
		i2c->dma_buf.vaddr, i2c->dma_buf.paddr);
}

static inline void mt_i2c_init_hw(struct mt_i2c *i2c)
{
	i2c_writew(I2C_SOFT_RST, i2c, OFFSET_SOFTRESET);
	/* Set ioconfig */
	if (i2c->use_push_pull)
		i2c_writew(I2C_IO_CONFIG_PUSH_PULL, i2c, OFFSET_IO_CONFIG);
	else
		i2c_writew(I2C_IO_CONFIG_OPEN_DRAIN, i2c, OFFSET_IO_CONFIG);
	if (i2c->have_dcm)
		i2c_writew(I2C_DCM_DISABLE, i2c, OFFSET_DCM_EN);
	i2c_writew(i2c->timing_reg, i2c, OFFSET_TIMING);
	i2c_writew(i2c->high_speed_reg, i2c, OFFSET_HS);
	/* DMA warm reset, and waits for EN to become 0 */
	i2c_writel_dma(I2C_DMA_WARM_RST, i2c, OFFSET_RST);
	udelay(5);
	if (i2c_readl_dma(i2c, OFFSET_EN) != 0) {
		dev_err(i2c->dev, "DMA bus hang .\n");
		dump_dma_regs();
		BUG_ON(1);
	}
}

/* calculate i2c port speed */
static int i2c_set_speed(struct mt_i2c *i2c, unsigned int clk_src_in_hz)
{
	int mode;
	unsigned int khz;
	unsigned int step_cnt;
	unsigned int sample_cnt;
	unsigned int sclk;
	unsigned int hclk;
	unsigned int max_step_cnt;
	unsigned int sample_div = MAX_SAMPLE_CNT_DIV;
	unsigned int step_div;
	unsigned int min_div;
	unsigned int best_mul;
	unsigned int cnt_mul;
	unsigned int speed_hz;

	if (i2c->ext_data.isEnable && i2c->ext_data.timing)
		speed_hz = i2c->ext_data.timing;
	else
		speed_hz = i2c->speed_hz;

	if (speed_hz > MAX_HS_MODE_SPEED) {
		return -EINVAL;
	} else if (speed_hz > MAX_FS_MODE_SPEED) {
		mode = HS_MODE;
		max_step_cnt = MAX_HS_STEP_CNT_DIV;
	} else {
		mode = FS_MODE;
		max_step_cnt = MAX_STEP_CNT_DIV;
	}
	step_div = max_step_cnt;

	/* Find the best combination */
	khz = speed_hz / 1000;
	hclk = clk_src_in_hz / 1000;
	min_div = ((hclk >> 1) + khz - 1) / khz;
	best_mul = MAX_SAMPLE_CNT_DIV * max_step_cnt;
	for (sample_cnt = 1; sample_cnt <= MAX_SAMPLE_CNT_DIV; sample_cnt++) {
		step_cnt = (min_div + sample_cnt - 1) / sample_cnt;
		cnt_mul = step_cnt * sample_cnt;
		if (step_cnt > max_step_cnt)
			continue;
		if (cnt_mul < best_mul) {
			best_mul = cnt_mul;
			sample_div = sample_cnt;
			step_div = step_cnt;
			if (best_mul == min_div)
				break;
		}
	}
	sample_cnt = sample_div;
	step_cnt = step_div;
	sclk = hclk / (2 * sample_cnt * step_cnt);
	if (sclk > khz) {
		dev_dbg(i2c->dev, "%s mode: unsupported speed (%ldkhz)\n",
			(mode == HS_MODE) ? "HS" : "ST/FT", (long int)khz);
		return -ENOTSUPP;
	}

	step_cnt--;
	sample_cnt--;

	if (mode == HS_MODE) {
		/* Set the hign speed mode register */
		i2c->timing_reg = 0x1303;
		i2c->high_speed_reg = I2C_TIME_DEFAULT_VALUE |
			(sample_cnt & I2C_TIMING_SAMPLE_COUNT_MASK) << 12 |
			(step_cnt & I2C_TIMING_SAMPLE_COUNT_MASK) << 8;
	} else {
		i2c->timing_reg =
			(sample_cnt & I2C_TIMING_SAMPLE_COUNT_MASK) << 8 |
			(step_cnt & I2C_TIMING_STEP_DIV_MASK) << 0;
		/* Disable the high speed transaction */
		i2c->high_speed_reg = I2C_TIME_CLR_VALUE;
	}

	return 0;
}

#ifdef I2C_DEBUG_FS
void i2c_dump_info1(struct mt_i2c *i2c)
{
	if (i2c->ext_data.isEnable && i2c->ext_data.timing)
		dev_err(i2c->dev, "I2C structure:\nspeed %d\n",
			i2c->ext_data.timing);
	else
		dev_err(i2c->dev, "I2C structure:\nspeed %d\n",
			i2c->speed_hz);
	dev_err(i2c->dev, "I2C structure:\nOp %x\n", i2c->op);
	dev_err(i2c->dev,
		"I2C structure:\nData_size %x\nIrq_stat %x\nTrans_stop %d\n",
		i2c->msg_len, i2c->irq_stat, i2c->trans_stop);
	dev_err(i2c->dev, "base address %p\n", i2c->base);
	dev_err(i2c->dev,
		"I2C register:\nSLAVE_ADDR %x\nINTR_MASK %x\n",
		(i2c_readw(i2c, OFFSET_SLAVE_ADDR)),
		(i2c_readw(i2c, OFFSET_INTR_MASK)));
	dev_err(i2c->dev,
		"I2C register:\nINTR_STAT %x\nCONTROL %x\n",
		(i2c_readw(i2c, OFFSET_INTR_STAT)),
		(i2c_readw(i2c, OFFSET_CONTROL)));
	dev_err(i2c->dev,
		"I2C register:\nTRANSFER_LEN %x\nTRANSAC_LEN %x\n",
		(i2c_readw(i2c, OFFSET_TRANSFER_LEN)),
		(i2c_readw(i2c, OFFSET_TRANSAC_LEN)));
	dev_err(i2c->dev,
		"I2C register:\nDELAY_LEN %x\nTIMING %x\n",
		(i2c_readw(i2c, OFFSET_DELAY_LEN)),
		(i2c_readw(i2c, OFFSET_TIMING)));
	dev_err(i2c->dev,
		"I2C register:\nSTART %x\nFIFO_STAT %x\n",
		(i2c_readw(i2c, OFFSET_START)),
		(i2c_readw(i2c, OFFSET_FIFO_STAT)));
	dev_err(i2c->dev,
		"I2C register:\nIO_CONFIG %x\nHS %x\n",
		(i2c_readw(i2c, OFFSET_IO_CONFIG)),
		(i2c_readw(i2c, OFFSET_HS)));
	dev_err(i2c->dev,
		"I2C register:\nDEBUGSTAT %x\nEXT_CONF %x\nPATH_DIR %x\n",
		(i2c_readw(i2c, OFFSET_DEBUGSTAT)),
		(i2c_readw(i2c, OFFSET_EXT_CONF)),
		(i2c_readw(i2c, OFFSET_PATH_DIR)));
}

void i2c_dump_info(struct mt_i2c *i2c)
{
	/* I2CFUC(); */
	/* int val=0; */
	pr_err("i2c_dump_info ++++++++++++++++++++++++++++++++++++++++++\n");
	pr_err("I2C structure:\n"
	       I2CTAG "Clk=%d,Id=%d,Op=%x,Irq_stat=%x,Total_len=%x\n"
	       I2CTAG "Trans_len=%x,Trans_num=%x,Trans_auxlen=%x,speed=%d\n"
	       I2CTAG "Trans_stop=%u\n",
	       15600, i2c->id, i2c->op, i2c->irq_stat, i2c->total_len,
			i2c->msg_len, 1, i2c->msg_aux_len, i2c->speed_hz, i2c->trans_stop);

	pr_err("base address 0x%p\n", i2c->base);
	pr_err("I2C register:\n"
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
	       (i2c_readw(i2c, OFFSET_EXT_CONF)), (i2c_readw(i2c, OFFSET_TRANSFER_LEN_AUX)));

	pr_err("before enable DMA register(0x%ld):\n"
	       I2CTAG "INT_FLAG=%x,INT_EN=%x,EN=%x,RST=%x,\n"
	       I2CTAG "STOP=%x,FLUSH=%x,CON=%x,TX_MEM_ADDR=%x, RX_MEM_ADDR=%x\n"
	       I2CTAG "TX_LEN=%x,RX_LEN=%x,INT_BUF_SIZE=%x,DEBUG_STATUS=%x\n"
	       I2CTAG "TX_MEM_ADDR2=%x, RX_MEM_ADDR2=%x\n",
	       g_dma_regs[i2c->id].base,
	       g_dma_regs[i2c->id].int_flag,
	       g_dma_regs[i2c->id].int_en,
	       g_dma_regs[i2c->id].en,
	       g_dma_regs[i2c->id].rst,
	       g_dma_regs[i2c->id].stop,
	       g_dma_regs[i2c->id].flush,
	       g_dma_regs[i2c->id].con,
	       g_dma_regs[i2c->id].tx_mem_addr,
	       g_dma_regs[i2c->id].tx_mem_addr,
	       g_dma_regs[i2c->id].tx_len,
	       g_dma_regs[i2c->id].rx_len,
	       g_dma_regs[i2c->id].int_buf_size, g_dma_regs[i2c->id].debug_sta,
	       g_dma_regs[i2c->id].tx_mem_addr2,
	       g_dma_regs[i2c->id].tx_mem_addr2);
	pr_err("DMA register(0x%p):\n"
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
	pr_err("i2c_dump_info ------------------------------------------\n");

}
#else
void i2c_dump_info(struct mt_i2c *i2c)
{
}
#endif
static int mt_i2c_do_transfer(struct mt_i2c *i2c)
{
	u16 addr_reg;
	u16 control_reg;
	int tmo = i2c->adap.timeout;
	unsigned int speed_hz;
	bool isDMA = false;
	int data_size;
	u8 *ptr;
	int ret;

	i2c->trans_stop = false;
	i2c->irq_stat = 0;
	if (i2c->total_len > 8 || i2c->msg_aux_len > 8)
		isDMA = true;
	if (i2c->ext_data.isEnable && i2c->ext_data.timing)
		speed_hz = i2c->ext_data.timing;
	else
		speed_hz = i2c->speed_hz;
#if !defined(CONFIG_MT_I2C_FPGA_ENABLE)
	ret = i2c_set_speed(i2c, clk_get_rate(i2c->clk_main) / i2c->clk_src_div);
#else
	ret = i2c_set_speed(i2c, I2C_CLK_RATE);
#endif
	if (ret) {
		dev_err(i2c->dev, "Failed to set the speed\n");
		return -EINVAL;
	}
	/* If use i2c pin from PMIC mt6397 side, need set PATH_DIR first */
	if (i2c->have_pmic)
		i2c_writew(I2C_CONTROL_WRAPPER, i2c, OFFSET_PATH_DIR);
	control_reg = I2C_CONTROL_ACKERR_DET_EN | I2C_CONTROL_CLK_EXT_EN;
	if (isDMA == true) /* DMA */
		control_reg |= I2C_CONTROL_DMA_EN;
	if (speed_hz > 400000)
		control_reg |= I2C_CONTROL_RS;
	if (i2c->op == I2C_MASTER_WRRD)
		control_reg |= I2C_CONTROL_DIR_CHANGE | I2C_CONTROL_RS;
	i2c_writew(control_reg, i2c, OFFSET_CONTROL);

	/* set start condition */
	if (speed_hz <= 100000)
		i2c_writew(I2C_ST_START_CON, i2c, OFFSET_EXT_CONF);
	else
		i2c_writew(I2C_FS_START_CON, i2c, OFFSET_EXT_CONF);

	if (~control_reg & I2C_CONTROL_RS)
		i2c_writew(I2C_DELAY_LEN, i2c, OFFSET_DELAY_LEN);

	/* Set ioconfig */
	if (i2c->use_push_pull)
		i2c_writew(I2C_IO_CONFIG_PUSH_PULL, i2c, OFFSET_IO_CONFIG);
	else
		i2c_writew(I2C_IO_CONFIG_OPEN_DRAIN, i2c, OFFSET_IO_CONFIG);

	i2c_writew(i2c->timing_reg, i2c, OFFSET_TIMING);
	i2c_writew(i2c->high_speed_reg, i2c, OFFSET_HS);

	addr_reg = i2c->addr << 1;
	if (i2c->op == I2C_MASTER_RD)
		addr_reg |= 0x1;
	i2c_writew(addr_reg, i2c, OFFSET_SLAVE_ADDR);
	/* Clear interrupt status */
	i2c_writew(I2C_HS_NACKERR | I2C_ACKERR | I2C_TRANSAC_COMP,
		i2c, OFFSET_INTR_STAT);
	i2c_writew(I2C_FIFO_ADDR_CLR, i2c, OFFSET_FIFO_ADDR_CLR);
	/* Enable interrupt */
	i2c_writew(I2C_HS_NACKERR | I2C_ACKERR | I2C_TRANSAC_COMP,
		i2c, OFFSET_INTR_MASK);
	/* Set transfer and transaction len */
	if (i2c->op == I2C_MASTER_WRRD) {
		if ((i2c->appm) && (i2c->dev_comp->idvfs_i2c)) {
			i2c_writew((i2c->msg_len & 0xFF) | ((i2c->msg_aux_len<<8) & 0x1F00),
				i2c, OFFSET_TRANSFER_LEN);
		} else {
			i2c_writew(i2c->msg_len, i2c, OFFSET_TRANSFER_LEN);
			i2c_writew(i2c->msg_aux_len, i2c, OFFSET_TRANSFER_LEN_AUX);
		}
		i2c_writew(0x02, i2c, OFFSET_TRANSAC_LEN);
	} else if (i2c->op == I2C_MASTER_MULTI_WR) {
		i2c_writew(i2c->msg_len, i2c, OFFSET_TRANSFER_LEN);
		i2c_writew(i2c->total_len / i2c->msg_len, i2c, OFFSET_TRANSAC_LEN);
	} else {
		i2c_writew(i2c->msg_len, i2c, OFFSET_TRANSFER_LEN);
		i2c_writew(0x01, i2c, OFFSET_TRANSAC_LEN);
	}

	/* Prepare buffer data to start transfer */
	if (isDMA == true) {
#ifdef CONFIG_MTK_LM_MODE
		if ((i2c->dev_comp->dma_support == 1) && (enable_4G())) {
			i2c_writel_dma(0x1, i2c, OFFSET_TX_MEM_ADDR2);
			i2c_writel_dma(0x1, i2c, OFFSET_RX_MEM_ADDR2);
		}
#endif
		if (i2c->op == I2C_MASTER_RD) {
			i2c_writel_dma(I2C_DMA_INT_FLAG_NONE, i2c, OFFSET_INT_FLAG);
			i2c_writel_dma(I2C_DMA_CON_RX, i2c, OFFSET_CON);
			i2c_writel_dma((u32)i2c->dma_buf.paddr, i2c, OFFSET_RX_MEM_ADDR);
			if ((i2c->dev_comp->dma_support >= 2))
				i2c_writel_dma(i2c->dma_buf.paddr >> 32, i2c, OFFSET_RX_MEM_ADDR2);

			i2c_writel_dma(i2c->msg_len, i2c, OFFSET_RX_LEN);
		} else if (i2c->op == I2C_MASTER_WR || i2c->op == I2C_MASTER_MULTI_WR) {
			i2c_writel_dma(I2C_DMA_INT_FLAG_NONE, i2c, OFFSET_INT_FLAG);
			i2c_writel_dma(I2C_DMA_CON_TX, i2c, OFFSET_CON);
			i2c_writel_dma((u32)i2c->dma_buf.paddr, i2c, OFFSET_TX_MEM_ADDR);
			if ((i2c->dev_comp->dma_support >= 2))
				i2c_writel_dma(i2c->dma_buf.paddr >> 32, i2c, OFFSET_TX_MEM_ADDR2);

			i2c_writel_dma(i2c->total_len, i2c, OFFSET_TX_LEN);
		} else {
			i2c_writel_dma(0x0000, i2c, OFFSET_INT_FLAG);
			i2c_writel_dma(0x0000, i2c, OFFSET_CON);
			i2c_writel_dma((u32)i2c->dma_buf.paddr, i2c, OFFSET_TX_MEM_ADDR);
			i2c_writel_dma((u32)i2c->dma_buf.paddr, i2c, OFFSET_RX_MEM_ADDR);
			if ((i2c->dev_comp->dma_support >= 2)) {
				i2c_writel_dma(i2c->dma_buf.paddr >> 32, i2c, OFFSET_TX_MEM_ADDR2);
				i2c_writel_dma(i2c->dma_buf.paddr >> 32, i2c, OFFSET_RX_MEM_ADDR2);
			}
			i2c_writel_dma(i2c->msg_len, i2c, OFFSET_TX_LEN);
			i2c_writel_dma(i2c->msg_aux_len, i2c, OFFSET_RX_LEN);
		}
		record_i2c_dma_info(i2c);
		/* flush before sending DMA start */
		mb();
		i2c_writel_dma(I2C_DMA_START_EN, i2c, OFFSET_EN);
	} else {
		if (i2c->op != I2C_MASTER_RD) {
			data_size = i2c->total_len;
			ptr = i2c->dma_buf.vaddr;
			while (data_size--) {
				i2c_writew(*ptr, i2c, OFFSET_DATA_PORT);
				ptr++;
			}
		}
	}
	/* flush before sending start */
	mb();
	if (!i2c->is_hw_trig)
		i2c_writew(I2C_TRANSAC_START, i2c, OFFSET_START);
	else {
		dev_err(i2c->dev, "I2C hw trig.\n");
		return 0;
	}

	tmo = wait_event_timeout(i2c->wait, i2c->trans_stop, tmo);

	if (tmo == 0) {
		dev_err(i2c->dev, "addr: %x, transfer timeout\n", i2c->addr);
		i2c_dump_info(i2c);
		mt_irq_dump_status(i2c->irqnr);
		dump_cg_regs();
		mt_i2c_init_hw(i2c);
		return -ETIMEDOUT;
	}
	if (i2c->irq_stat & (I2C_HS_NACKERR | I2C_ACKERR)) {
		dev_err(i2c->dev, "addr: %x, transfer ACK error\n", i2c->addr);
		if (i2c->ext_data.isEnable ==  false || i2c->ext_data.isFilterMsg == false)
			i2c_dump_info(i2c);
		mt_i2c_init_hw(i2c);
		return -EREMOTEIO;
	}
	if (i2c->op != I2C_MASTER_WR && isDMA == false) {
		data_size = (i2c_readw(i2c, OFFSET_FIFO_STAT) >> 4) & 0x000F;
		ptr = i2c->dma_buf.vaddr;
		while (data_size--) {
			*ptr = i2c_readw(i2c, OFFSET_DATA_PORT);
			/* I2CLOG("addr %x read byte = 0x%x\n", i2c->addr, *ptr); */
			ptr++;
		}
	}
	dev_dbg(i2c->dev, "i2c transfer done.\n");
	return 0;
}

static inline void mt_i2c_copy_to_dma(struct mt_i2c *i2c, struct i2c_msg *msg)
{
	/* if the operate is write, write-read, multi-write, need to copy the data
		 to DMA memory */
	if (!(msg->flags & I2C_M_RD))
		memcpy(i2c->dma_buf.vaddr + i2c->total_len - msg->len,
			msg->buf, msg->len);
}

static inline void mt_i2c_copy_from_dma(struct mt_i2c *i2c,
	struct i2c_msg *msg)
{
	/* if the operate is read, need to copy the data from DMA memory */
	if (msg->flags & I2C_M_RD)
		memcpy(msg->buf, i2c->dma_buf.vaddr, msg->len);
}

/*
 * In MTK platform the STOP will be issued after each
 * message was transferred which is not flow the clarify
 * for i2c_transfer(), several I2C devices tolerate the STOP,
 * but some device need Repeat-Start and do not compatible with STOP
 * MTK platform has WRRD mode which can write then read with
 * Repeat-Start between two message, so we combined two
 * messages into one transaction.
 * The max read length is 4096
 */
static bool mt_i2c_should_combine(struct i2c_msg *msg)
{
	struct i2c_msg *next_msg = msg + 1;

	if ((next_msg->len < 4096) &&
			msg->addr == next_msg->addr &&
			!(msg->flags & I2C_M_RD) &&
			(next_msg->flags & I2C_M_RD) == I2C_M_RD) {
		return true;
	}
	return false;
}

static bool mt_i2c_should_batch(struct i2c_msg *prev, struct i2c_msg *next)
{
	if ((prev->flags & I2C_M_RD) || (next->flags & I2C_M_RD))
		return false;
	if (prev->len == next->len && prev->addr == next->addr)
		return true;
	return false;
}

#if 0
static int __mt_i2c_transfer(struct mt_i2c *i2c,
	struct i2c_msg msgs[], int num)
{
	int ret;
	int left_num = num;

	ret = mt_i2c_clock_enable(i2c);
	if (ret)
		return ret;
	while (left_num--) {
		/* In MTK platform the max transfer number is 4096 */
		if (msgs->len > MAX_DMA_TRANS_SIZE) {
			dev_dbg(i2c->dev,
				" message data length is more than 255\n");
			ret = -EINVAL;
			goto err_exit;
		}
		if (msgs->addr == 0) {
			dev_dbg(i2c->dev, " addr is invalid.\n");
			ret = -EINVAL;
			goto err_exit;
		}
		if (msgs->buf == NULL) {
			dev_dbg(i2c->dev, " data buffer is NULL.\n");
			ret = -EINVAL;
			goto err_exit;
		}

		i2c->addr = msgs->addr;
		i2c->msg_len = msgs->len;
		i2c->msg_buf = msgs->buf;
		i2c->msg_aux_len = 0;
		if (msgs->flags & I2C_M_RD)
			i2c->op = I2C_MASTER_RD;
		else
			i2c->op = I2C_MASTER_WR;
		/* combined two messages into one transaction */
		if (left_num >= 1 && mt_i2c_should_combine(msgs)) {
			i2c->msg_aux_len = (msgs + 1)->len;
			i2c->op = I2C_MASTER_WRRD;
			left_num--;
		}
		/*
		 * always use DMA mode.
		 * 1st when write need copy the data of message to dma memory
		 * 2nd when read need copy the DMA data to the message buffer.
		 * The length should be less than 255.
		 */
		mt_i2c_copy_to_dma(i2c, msgs);
		i2c->msg_buf = (u8 *)i2c->dma_buf.paddr;

		/* Use HW semaphore to protect mt6313 access between AP and SPM */
		if (i2c_get_semaphore(i2c) != 0)
			return -EBUSY;
		ret = mt_i2c_do_transfer(i2c);
		/* Use HW semaphore to protect mt6313 access between AP and SPM */
		if (i2c_release_semaphore(i2c) != 0)
			ret = -EBUSY;
		if (ret < 0)
			goto err_exit;
		if (i2c->op == I2C_MASTER_WRRD)
			mt_i2c_copy_from_dma(i2c, msgs + 1);
		else
			mt_i2c_copy_from_dma(i2c, msgs);
		msgs++;
		/* after combined two messages so we need ignore one */
		if (left_num > 0 && i2c->op == I2C_MASTER_WRRD)
			msgs++;
	}
	/* the return value is number of executed messages */
	ret = num;
err_exit:
	mt_i2c_clock_disable(i2c);
	return ret;
}
#else
static int __mt_i2c_transfer(struct mt_i2c *i2c,
	struct i2c_msg msgs[], int num)
{
	int ret;
	int left_num = num;

	while (left_num--) {
		/* In MTK platform the max transfer number is 4096 */
		if (msgs->len > MAX_DMA_TRANS_SIZE) {
			dev_dbg(i2c->dev,
				" message data length is more than 255\n");
			ret = -EINVAL;
			goto err_exit;
		}
		if (msgs->addr == 0) {
			dev_dbg(i2c->dev, " addr is invalid.\n");
			ret = -EINVAL;
			goto err_exit;
		}
		if (msgs->buf == NULL) {
			dev_dbg(i2c->dev, " data buffer is NULL.\n");
			ret = -EINVAL;
			goto err_exit;
		}

		i2c->addr = msgs->addr;
		i2c->msg_len = msgs->len;
		i2c->msg_aux_len = 0;

		if ((left_num + 1 == num) || !mt_i2c_should_batch(msgs - 1, msgs)) {
			i2c->total_len = msgs->len;
			if (msgs->flags & I2C_M_RD)
				i2c->op = I2C_MASTER_RD;
			else
				i2c->op = I2C_MASTER_WR;
		} else {
			i2c->total_len += msgs->len;
		}

		/*
		 * always use DMA mode.
		 * 1st when write need copy the data of message to dma memory
		 * 2nd when read need copy the DMA data to the message buffer.
		 * The length should be less than 255.
		 */
		mt_i2c_copy_to_dma(i2c, msgs);

		if (left_num >= 1) {
			if (mt_i2c_should_batch(msgs, msgs + 1)) {
				i2c->op = I2C_MASTER_MULTI_WR;
				msgs++;
				continue;
			}
			if (mt_i2c_should_combine(msgs)) {
				i2c->msg_aux_len = (msgs + 1)->len;
				i2c->op = I2C_MASTER_WRRD;
				left_num--;
			}
		}

		/* Use HW semaphore to protect device access between AP and SPM, or SCP */
		if (i2c_get_semaphore(i2c) != 0) {
			dev_err(i2c->dev, "get hw semaphore failed.\n");
			return -EBUSY;
		}
		ret = mt_i2c_do_transfer(i2c);
		/* Use HW semaphore to protect device access between AP and SPM, or SCP */
		if (i2c_release_semaphore(i2c) != 0) {
			dev_err(i2c->dev, "release hw semaphore failed.\n");
			ret = -EBUSY;
		}

		if (ret < 0)
			goto err_exit;
		if (i2c->op == I2C_MASTER_WRRD)
			mt_i2c_copy_from_dma(i2c, msgs + 1);
		else
			mt_i2c_copy_from_dma(i2c, msgs);

		msgs++;
		/* after combined two messages so we need ignore one */
		if (left_num > 0 && i2c->op == I2C_MASTER_WRRD)
			msgs++;
	}
	/* the return value is number of executed messages */
	ret = num;
err_exit:
	return ret;
}
#endif

#ifdef CONFIG_TRUSTONIC_TEE_SUPPORT
int i2c_tui_enable_clock(void)
{
	struct i2c_adapter *adap;
	struct mt_i2c *i2c;

	adap = i2c_get_adapter(4);
	if (!adap) {
		pr_err("Cannot get adapter\n");
		return -1;
	}

	i2c = i2c_get_adapdata(adap);
	clk_prepare_enable(i2c->clk_main);
	clk_prepare_enable(i2c->clk_dma);

	return 0;
}

int i2c_tui_disable_clock(void)
{
	struct i2c_adapter *adap;
	struct mt_i2c *i2c;

	adap = i2c_get_adapter(4);
	if (!adap) {
		pr_err("Cannot get adapter\n");
		return -1;
	}

	i2c = i2c_get_adapdata(adap);
	clk_disable_unprepare(i2c->clk_dma);
	clk_disable_unprepare(i2c->clk_main);

	return 0;
}
#endif

static int mt_i2c_transfer(struct i2c_adapter *adap,
	struct i2c_msg msgs[], int num)
{
	int ret;
	struct mt_i2c *i2c = i2c_get_adapdata(adap);

	ret = mt_i2c_clock_enable(i2c);
	if (ret)
		return -EBUSY;

	mutex_lock(&i2c->i2c_mutex);
	ret = __mt_i2c_transfer(i2c, msgs, num);
	mutex_unlock(&i2c->i2c_mutex);

	mt_i2c_clock_disable(i2c);
	return ret;
}


static void mt_i2c_parse_extension(struct mt_i2c_ext *pext, u32 ext_flag, u32 timing)
{
	if (ext_flag & I2C_A_FILTER_MSG)
		pext->isFilterMsg = true;
	if (timing)
		pext->timing = timing;
}

int mtk_i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num,
					u32 ext_flag, u32 timing)
{
	int ret;
	struct mt_i2c *i2c = i2c_get_adapdata(adap);

	ret = mt_i2c_clock_enable(i2c);
	if (ret)
		return -EBUSY;

	mutex_lock(&i2c->i2c_mutex);
	i2c->ext_data.isEnable = true;

	mt_i2c_parse_extension(&i2c->ext_data, ext_flag, timing);
	ret = __mt_i2c_transfer(i2c, msgs, num);

	i2c->ext_data.isEnable = false;
	mutex_unlock(&i2c->i2c_mutex);

	mt_i2c_clock_disable(i2c);
	return ret;
}
EXPORT_SYMBOL(mtk_i2c_transfer);

int hw_trig_i2c_enable(struct i2c_adapter *adap)
{
	struct mt_i2c *i2c = i2c_get_adapdata(adap);

	if (!i2c->buffermode)
		return -1;
	if (mt_i2c_clock_enable(i2c))
		return -EBUSY;

	mutex_lock(&i2c->i2c_mutex);
	i2c->is_hw_trig = true;
	mutex_unlock(&i2c->i2c_mutex);
	return 0;
}
EXPORT_SYMBOL(hw_trig_i2c_enable);

int hw_trig_i2c_disable(struct i2c_adapter *adap)
{
	struct mt_i2c *i2c = i2c_get_adapdata(adap);

	if (!i2c->buffermode)
		return -1;
	mutex_lock(&i2c->i2c_mutex);
	i2c->is_hw_trig = false;
	mutex_unlock(&i2c->i2c_mutex);
	mt_i2c_clock_disable(i2c);
	return 0;
}
EXPORT_SYMBOL(hw_trig_i2c_disable);

int hw_trig_i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs,
		int num)
{
	int ret;
	struct mt_i2c *i2c = i2c_get_adapdata(adap);

	if (!i2c->buffermode)
		return -1;
	mutex_lock(&i2c->i2c_mutex);
	ret = __mt_i2c_transfer(i2c, msgs, num);
	mutex_unlock(&i2c->i2c_mutex);
	return ret;
}
EXPORT_SYMBOL(hw_trig_i2c_transfer);

static irqreturn_t mt_i2c_irq(int irqno, void *dev_id)
{
	struct mt_i2c *i2c = dev_id;

	/* Clear interrupt mask */
	i2c_writew(~(I2C_HS_NACKERR | I2C_ACKERR | I2C_TRANSAC_COMP),
		i2c, OFFSET_INTR_MASK);
	i2c->irq_stat = i2c_readw(i2c, OFFSET_INTR_STAT);
	i2c_writew(I2C_HS_NACKERR | I2C_ACKERR | I2C_TRANSAC_COMP,
		i2c, OFFSET_INTR_STAT);
	i2c->trans_stop = true;
	if (!i2c->is_hw_trig)
		wake_up(&i2c->wait);
	else {	/* dump regs info for hw trig i2c if ACK err */
		if (i2c->irq_stat & (I2C_HS_NACKERR | I2C_ACKERR)) {
			dev_err(i2c->dev, "addr: %x, transfer ACK error\n", i2c->addr);
			i2c_dump_info(i2c);
			mt_i2c_init_hw(i2c);
		}
	}
	return IRQ_HANDLED;
}

static u32 mt_i2c_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_10BIT_ADDR | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm mt_i2c_algorithm = {
	.master_xfer = mt_i2c_transfer,
	.functionality = mt_i2c_functionality,
};

static int mt_i2c_parse_dt(struct device_node *np, struct mt_i2c *i2c)
{
	i2c->speed_hz = I2C_DEFAUT_SPEED;
	of_property_read_u32(np, "clock-frequency", &i2c->speed_hz);
	of_property_read_u32(np, "clock-div", &i2c->clk_src_div);
	of_property_read_u32(np, "id", (u32 *)&i2c->id);
	i2c->have_pmic = of_property_read_bool(np, "mediatek,have-pmic");
	i2c->have_dcm = of_property_read_bool(np, "mediatek,have-dcm");
	i2c->use_push_pull = of_property_read_bool(np, "mediatek,use-push-pull");
	i2c->appm = of_property_read_bool(np, "mediatek,appm_used");
	i2c->gpupm = of_property_read_bool(np, "mediatek,gpupm_used");
	i2c->buffermode = of_property_read_bool(np, "mediatek,buffermode_used");
	pr_err("[I2C] id : %d, freq : %d, div : %d.\n", i2c->id, i2c->speed_hz, i2c->clk_src_div);
	if (i2c->clk_src_div == 0)
		return -EINVAL;
	return 0;
}

static const struct mtk_i2c_compatible mt6735_compat = {
	.dma_support = 0,
	.idvfs_i2c = 0,
};

static const struct mtk_i2c_compatible mt6797_compat = {
	.dma_support = 1,
	.idvfs_i2c = 1,
};

static const struct mtk_i2c_compatible mt6757_compat = {
	.dma_support = 2,
	.idvfs_i2c = 0,
};

static const struct mtk_i2c_compatible elbrus_compat = {
	.dma_support = 2,
	.idvfs_i2c = 1,
};


static const struct of_device_id mtk_i2c_of_match[] = {
	{ .compatible = "mediatek,mt6735-i2c", .data = &mt6735_compat },
	{ .compatible = "mediatek,mt6797-i2c", .data = &mt6797_compat },
	{ .compatible = "mediatek,mt6757-i2c", .data = &mt6757_compat },
	{ .compatible = "mediatek,elbrus-i2c", .data = &elbrus_compat },
	{},
};

MODULE_DEVICE_TABLE(of, mtk_i2c_of_match);

static int mt_i2c_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mt_i2c *i2c;
	unsigned int clk_src_in_hz;
	struct resource *res;
	const struct of_device_id *of_id;

	i2c = devm_kzalloc(&pdev->dev, sizeof(struct mt_i2c), GFP_KERNEL);
	if (i2c == NULL)
		return -ENOMEM;

	ret = mt_i2c_parse_dt(pdev->dev.of_node, i2c);
	if (ret)
		return -EINVAL;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	i2c->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(i2c->base))
		return PTR_ERR(i2c->base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);

	i2c->pdmabase = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(i2c->pdmabase))
		return PTR_ERR(i2c->pdmabase);

	i2c->irqnr = platform_get_irq(pdev, 0);
	if (i2c->irqnr <= 0)
		return -EINVAL;
	init_waitqueue_head(&i2c->wait);

	ret = devm_request_irq(&pdev->dev, i2c->irqnr, mt_i2c_irq,
		IRQF_TRIGGER_NONE, I2C_DRV_NAME, i2c);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Request I2C IRQ %d fail\n", i2c->irqnr);
		return ret;
	}
	of_id = of_match_node(mtk_i2c_of_match, pdev->dev.of_node);
	if (!of_id)
		return -EINVAL;

	i2c->dev_comp = of_id->data;
	i2c->adap.dev.of_node = pdev->dev.of_node;
	i2c->dev = &i2c->adap.dev;
	i2c->adap.dev.parent = &pdev->dev;
	i2c->adap.owner = THIS_MODULE;
	i2c->adap.algo = &mt_i2c_algorithm;
	i2c->adap.algo_data = NULL;
	i2c->adap.timeout = 2 * HZ;
	i2c->adap.retries = 1;
	i2c->adap.nr = i2c->id;

	if (i2c->dev_comp->dma_support == 2) {
		if (dma_set_mask(&pdev->dev, DMA_BIT_MASK(33))) {
			dev_err(&pdev->dev, "dma_set_mask return error.\n");
			return -EINVAL;
		}
	} else if (i2c->dev_comp->dma_support == 3) {
		if (dma_set_mask(&pdev->dev, DMA_BIT_MASK(36))) {
			dev_err(&pdev->dev, "dma_set_mask return error.\n");
			return -EINVAL;
		}
	}

#if !defined(CONFIG_MT_I2C_FPGA_ENABLE)
	i2c->clk_main = devm_clk_get(&pdev->dev, "main");
	if (IS_ERR(i2c->clk_main)) {
		dev_err(&pdev->dev, "cannot get main clock\n");
		return PTR_ERR(i2c->clk_main);
	}
	i2c->clk_dma = devm_clk_get(&pdev->dev, "dma");
	if (IS_ERR(i2c->clk_dma)) {
		dev_err(&pdev->dev, "cannot get dma clock\n");
		return PTR_ERR(i2c->clk_dma);
	}
	i2c->clk_arb = devm_clk_get(&pdev->dev, "arb");
	if (IS_ERR(i2c->clk_arb))
		i2c->clk_arb = NULL;
	else
		dev_dbg(&pdev->dev, "i2c%d has the relevant arbitrator clk.\n", i2c->id);
#endif

	if (i2c->have_pmic) {
		i2c->clk_pmic = devm_clk_get(&pdev->dev, "pmic");
		if (IS_ERR(i2c->clk_pmic)) {
			dev_err(&pdev->dev, "cannot get pmic clock\n");
			return PTR_ERR(i2c->clk_pmic);
		}
		clk_src_in_hz = clk_get_rate(i2c->clk_pmic) / i2c->clk_src_div;
	} else {
		clk_src_in_hz = clk_get_rate(i2c->clk_main) / i2c->clk_src_div;
	}
	dev_dbg(&pdev->dev, "clock source %p,clock src frequency %d\n",
		i2c->clk_main, clk_src_in_hz);

	strlcpy(i2c->adap.name, I2C_DRV_NAME, sizeof(i2c->adap.name));
	mutex_init(&i2c->i2c_mutex);
	ret = i2c_set_speed(i2c, clk_src_in_hz);
	if (ret) {
		dev_err(&pdev->dev, "Failed to set the speed\n");
		return -EINVAL;
	}
	ret = mt_i2c_clock_enable(i2c);
	if (ret) {
		dev_err(&pdev->dev, "clock enable failed!\n");
		return ret;
	}
	mt_i2c_init_hw(i2c);
	mt_i2c_clock_disable(i2c);
	i2c->dma_buf.vaddr = dma_alloc_coherent(&pdev->dev,
		PAGE_SIZE, &i2c->dma_buf.paddr, GFP_KERNEL);
	if (i2c->dma_buf.vaddr == NULL) {
		dev_err(&pdev->dev, "dma_alloc_coherent fail\n");
		return -ENOMEM;
	}
	i2c_set_adapdata(&i2c->adap, i2c);
	/* ret = i2c_add_adapter(&i2c->adap); */
	ret = i2c_add_numbered_adapter(&i2c->adap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to add i2c bus to i2c core\n");
		free_i2c_dma_bufs(i2c);
		return ret;
	}
	platform_set_drvdata(pdev, i2c);
	return 0;
}

static int mt_i2c_remove(struct platform_device *pdev)
{
	struct mt_i2c *i2c = platform_get_drvdata(pdev);

	i2c_del_adapter(&i2c->adap);
	free_i2c_dma_bufs(i2c);
	platform_set_drvdata(pdev, NULL);
	return 0;
}


MODULE_DEVICE_TABLE(of, mt_i2c_match);

static struct platform_driver mt_i2c_driver = {
	.probe = mt_i2c_probe,
	.remove = mt_i2c_remove,
	.driver = {
		.name = I2C_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mtk_i2c_of_match),
	},
};

#ifdef CONFIG_MTK_I2C_ARBITRATION
static s32 enable_arbitration(void)
{
	struct device_node *pericfg_node;
	void __iomem *pericfg_base;

	pericfg_node = of_find_compatible_node(NULL, NULL, "mediatek,pericfg");
	if (!pericfg_node) {
		pr_err("Cannot find pericfg node\n");
		return -ENODEV;
	}
	pericfg_base = of_iomap(pericfg_node, 0);
	if (!pericfg_base) {
		pr_err("pericfg iomap failed\n");
		return -ENOMEM;
	}
	/* Enable the I2C arbitration */
	writew(0x3, pericfg_base + OFFSET_PERI_I2C_MODE_ENABLE);
	return 0;
}
#endif

static s32 __init mt_i2c_init(void)
{
#ifdef CONFIG_MTK_I2C_ARBITRATION
	int ret;

	ret = enable_arbitration();
	if (ret) {
		pr_err("Cannot enalbe arbitration.\n");
		return ret;
	}
#endif
	if (!map_cg_regs())
		pr_warn("Mapp cg regs successfully.\n");

	if (!map_dma_regs())
		pr_warn("Mapp dma regs successfully.\n");
	pr_err(" mt_i2c_init driver as platform device\n");
	return platform_driver_register(&mt_i2c_driver);
}

static void __exit mt_i2c_exit(void)
{
	platform_driver_unregister(&mt_i2c_driver);
}

module_init(mt_i2c_init);
module_exit(mt_i2c_exit);

/* module_platform_driver(mt_i2c_driver); */

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek I2C Bus Driver");
MODULE_AUTHOR("Xudong Chen <xudong.chen@mediatek.com>");
