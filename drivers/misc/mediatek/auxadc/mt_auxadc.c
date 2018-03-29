/*
 * Copyright (C) 2015 MediaTek Inc.
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

/*****************************************************************************
 *
 * Filename:
 * ---------
 *    mt_auxadc.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines functions of AUXADC common code
 *
 * Author:
 * -------
 * Zhong Wang
 *
 ****************************************************************************/

#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/seq_file.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <mt-plat/sync_write.h>

#include "mt_auxadc.h"
#include "mt_auxadc_hw.h"

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#endif

/* the new efuse cali has 2 regs, and the old has one only */
#if defined(NEW_EFUSE_CALI_REG)
#define EFUSE_CALI_REG        1
#else
#define EFUSE_CALI_REG        0
#endif

#if !defined(CONFIG_MTK_CLKMGR)
#ifdef CONFIG_OF
static struct clk *clk_auxadc;
#endif
#endif
#ifdef CONFIG_OF
void __iomem *auxadc_base = NULL;
void __iomem *auxadc_apmix_base = NULL;
#if defined(EFUSE_CALI)
void __iomem *auxadc_efuse_base = NULL;
#endif
#endif

#if !defined(CONFIG_MTK_CLKMGR)
#include <linux/clk.h>
#else
/*#include <cust_adc.h>*//* generate by DCT Tool */
#include <mach/mt_clkmgr.h>
#endif

typedef unsigned short UINT16;

#define READ_REGISTER_UINT16(reg) (*(volatile UINT16 * const)(reg))

#define INREG16(x)          READ_REGISTER_UINT16((UINT16 *)((void *)(x)))
#define DRV_Reg16(addr)             INREG16(addr)
#define DRV_Reg(addr)               DRV_Reg16(addr)

/******************************************************/
#define DRV_ClearBits(addr, data) {\
unsigned short temp;\
temp = DRV_Reg16(addr);\
temp &=  ~(data);\
mt_reg_sync_writew(temp, addr);\
}

#define DRV_SetBits(addr, data) {\
unsigned short temp;\
temp = DRV_Reg16(addr);\
temp |= (data);\
mt_reg_sync_writew(temp, addr);\
}

#define DRV_SetData(addr, bitmask, value) {\
unsigned short temp;\
temp = (~(bitmask)) & DRV_Reg16(addr);\
temp |= (value);\
mt_reg_sync_writew(temp, addr);\
}

#define AUXADC_DRV_ClearBits16(addr, data)           DRV_ClearBits(addr, data)
#define AUXADC_DRV_SetBits16(addr, data)             DRV_SetBits(addr, data)
#define AUXADC_DRV_WriteReg16(addr, data)            mt_reg_sync_writew(data, addr)
#define AUXADC_DRV_ReadReg16(addr)                   DRV_Reg(addr)
#define AUXADC_DRV_SetData16(addr, bitmask, value)   DRV_SetData(addr, bitmask, value)

#define AUXADC_CLR_BITS(BS, REG)     {\
unsigned int temp;\
temp = DRV_Reg32(REG);\
temp &=  ~(BS);\
mt_reg_sync_writel(temp, REG);\
}

#define AUXADC_SET_BITS(BS, REG)     {\
unsigned int temp;\
temp = DRV_Reg32(REG);\
temp |= (BS);\
mt_reg_sync_writel(temp, REG);\
}

#define VOLTAGE_FULL_RANGE  1500	/* VA voltage */
#define AUXADC_PRECISE      4096	/* 12 bits */
/******************************************************/


/*****************************************************************************
 * Integrate with NVRAM
****************************************************************************/
#define AUXADC_CALI_DEVNAME    "mtk-adc-cali"

#define TEST_ADC_CALI_PRINT _IO('k', 0)
#define SET_ADC_CALI_Slop   _IOW('k', 1, int)
#define SET_ADC_CALI_Offset _IOW('k', 2, int)
#define SET_ADC_CALI_Cal    _IOW('k', 3, int)
#define ADC_CHANNEL_READ    _IOW('k', 4, int)

typedef struct adc_info {
	char channel_name[64];
	int channel_number;
	int reserve1;
	int reserve2;
	int reserve3;
} ADC_INFO;

static ADC_INFO g_adc_info[ADC_CHANNEL_MAX];
static int auxadc_cali_slop[ADC_CHANNEL_MAX] = { 0 };
static int auxadc_cali_offset[ADC_CHANNEL_MAX] = { 0 };

static bool g_AUXADC_Cali;

static int auxadc_cali_cal[1] = { 0 };
static int auxadc_in_data[2] = { 1, 1 };
static int auxadc_out_data[2] = { 1, 1 };

static DEFINE_MUTEX(auxadc_mutex);
static DEFINE_MUTEX(mutex_get_cali_value);
static int adc_auto_set;
#if !defined(CONFIG_AUXADC_NOT_CONTROL_APMIXED_BASE)
static int adc_rtp_set = 1;
#endif

static dev_t auxadc_cali_devno;
static int auxadc_cali_major;
static struct cdev *auxadc_cali_cdev;
static struct class *auxadc_cali_class;

static struct task_struct *thread;
static int g_start_debug_thread;

static int g_adc_init_flag;

static u32 cali_reg;
#if defined(EFUSE_CALI)
#if EFUSE_CALI_REG
static u32 cali_ge_reg;
static u32 cali_oe_reg;
#endif
#endif
static u32 cali_oe;
static u32 cali_ge;
static u32 cali_ge_a;
static u32 cali_oe_a;
static u32 gain;

static void mt_auxadc_update_cali(void)
{
	cali_oe = 0;
	cali_ge = 0;

#if defined(EFUSE_CALI)
	cali_reg = (*(volatile unsigned int *const)(ADC_CALI_EN_A_REG));
#if EFUSE_CALI_REG
	cali_ge_reg = (*(volatile unsigned int *const)(ADC_GE_A_REG));
	cali_oe_reg = (*(volatile unsigned int *const)(ADC_OE_A_REG));
#endif
	if (((cali_reg & ADC_CALI_EN_A_MASK) >> ADC_CALI_EN_A_SHIFT) != 0) {
#if EFUSE_CALI_REG
		cali_oe_a = (cali_oe_reg & ADC_OE_A_MASK) >> ADC_OE_A_SHIFT;
		cali_ge_a = ((cali_ge_reg & ADC_GE_A_MASK) >> ADC_GE_A_SHIFT);
#else
		cali_oe_a = (cali_reg & ADC_OE_A_MASK) >> ADC_OE_A_SHIFT;
		cali_ge_a = ((cali_reg & ADC_GE_A_MASK) >> ADC_GE_A_SHIFT);
#endif
		cali_ge = cali_ge_a - 512;
		cali_oe = cali_oe_a - 512;
		gain = 1 + cali_ge;
	}
	/*pr_debug("[AUXADC] cali_reg=%x,cali_oe_a(%x), cali_ge_a(%x),cali_ge(%x),cali_oe(%x),gain(%x)\n",
	     cali_reg, cali_oe_a, cali_ge_a, cali_ge, cali_oe, gain);*/
#endif
}

static void mt_auxadc_get_cali_data(unsigned int rawdata, int data[4], bool enable_cali)
{

	if (enable_cali == true) {
#if defined(EFUSE_CALI)
		rawdata = rawdata - cali_oe;
		data[0] = (rawdata * 1500 / (4096 + cali_ge)) / 1000;	/* convert to volt */
		data[1] = (rawdata * 150 / (4096 + cali_ge)) % 100;  /* convert to mv, need multiply 10 */
		data[2] = (rawdata * 1500 / (4096 + cali_ge)) % 1000; /* data[2] provide high precision mv */
#else
		data[0] = (rawdata * 150 / AUXADC_PRECISE / 100);
		data[1] = ((rawdata * 150 / AUXADC_PRECISE) % 100);
		data[2] = ((rawdata * 1500 / AUXADC_PRECISE) % 1000);
#endif
	} else {
		data[0] = (rawdata * 150 / AUXADC_PRECISE / 100);
		data[1] = ((rawdata * 150 / AUXADC_PRECISE) % 100);
		data[2] = ((rawdata * 1500 / AUXADC_PRECISE) % 1000);
	}
}


#if defined(CONFIG_AUXADC_NOT_CONTROL_APMIXED_BASE)
static void mt_auxadc_disable_penirq(void)
{
}
#else
static u16 mt_tpd_read_adc(u16 pos)
{
	AUXADC_DRV_SetBits16((volatile u16 *)AUXADC_TP_ADDR, pos);
	AUXADC_DRV_SetBits16((volatile u16 *)AUXADC_TP_CON0, 0x01);
	while (0x01 & AUXADC_DRV_ReadReg16((volatile u16 *)AUXADC_TP_CON0))
		pr_debug("AUXADC_TP_CON0 waiting.\n");	/* wait for write finish */
	return AUXADC_DRV_ReadReg16((volatile u16 *)AUXADC_TP_DATA0);
}

static void mt_auxadc_disable_penirq(void)
{
	if (adc_rtp_set) {
		adc_rtp_set = 0;
		AUXADC_DRV_SetBits16((volatile u16 *)AUXADC_CON_RTP, 1);
		/* Turn off PENIRQ detection circuit */
		AUXADC_DRV_SetBits16((volatile u16 *)AUXADC_TP_CMD, 1);
		/* run once touch function */
		mt_tpd_read_adc(TP_CMD_ADDR_X);
	}
}
#endif

/* HAL API */
static int IMM_auxadc_GetOneChannelValue(int dwChannel, int data[4], int *rawdata)
{
	unsigned int channel[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	int idle_count = 0;
	int data_ready_count = 0;
	int ret = 0;

	mutex_lock(&mutex_get_cali_value);

#if !defined(CONFIG_MTK_CLKMGR)
	if (clk_auxadc) {
		ret = clk_prepare_enable(clk_auxadc);
		if (ret) {
			pr_err("hwEnableClock AUXADC failed.");
			mutex_unlock(&mutex_get_cali_value);
			return -1;
		}
	} else {
		pr_err("hwEnableClock AUXADC failed.");
		mutex_unlock(&mutex_get_cali_value);
		return -1;
	}
#else
#ifndef CONFIG_MTK_FPGA
	if (enable_clock(MT_PDN_PERI_AUXADC, "AUXADC")) {
		pr_err("hwEnableClock AUXADC failed.");
		mutex_unlock(&mutex_get_cali_value);
		return -1;
	}
#endif
#endif

	mt_auxadc_update_cali();

	if (dwChannel == PAD_AUX_XP || dwChannel == PAD_AUX_YM)
		mt_auxadc_disable_penirq();
	/* step1 check con2 if auxadc is busy */
	while (AUXADC_DRV_ReadReg16((volatile u16 *)AUXADC_CON2) & 0x01) {
		pr_debug("[adc_api]: wait for module idle\n");
		mdelay(1);
		idle_count++;
		if (idle_count > 30) {
			/* wait for idle time out */
			pr_err("[adc_api]: wait for auxadc idle time out\n");
			mutex_unlock(&mutex_get_cali_value);
			return -1;
		}
	}
	/* step2 clear bit */
	if (0 == adc_auto_set) {
		/* clear bit */
		AUXADC_DRV_ClearBits16((volatile u16 *)AUXADC_CON1, (1 << dwChannel));
	}


	/* step3  read channel and make sure old ready bit ==0 */
	while (AUXADC_DRV_ReadReg16(AUXADC_DAT0 + dwChannel * 0x04) & (1 << 12)) {
		pr_debug("[adc_api]: wait for channel[%d] ready bit clear\n", dwChannel);
		mdelay(1);
		data_ready_count++;
		if (data_ready_count > 30) {
			/* wait for idle time out */
			pr_err("[adc_api]: wait for channel[%d] ready bit clear time out\n",
			       dwChannel);
			mutex_unlock(&mutex_get_cali_value);
			return -2;
		}
	}

	/* step4 set bit  to trigger sample */
	if (0 == adc_auto_set)
		AUXADC_DRV_SetBits16((volatile u16 *)AUXADC_CON1, (1 << dwChannel));

	/* step5  read channel and make sure  ready bit ==1 */
	udelay(25);		/* we must dealay here for hw sample cahnnel data */
	while (0 == (AUXADC_DRV_ReadReg16(AUXADC_DAT0 + dwChannel * 0x04) & (1 << 12))) {
		pr_debug("[adc_api]: wait for channel[%d] ready bit ==1\n", dwChannel);
		mdelay(1);
		data_ready_count++;

		if (data_ready_count > 30) {
			/* wait for idle time out */
			pr_err("[adc_api]: wait for channel[%d] data ready time out\n", dwChannel);
			mutex_unlock(&mutex_get_cali_value);
			return -3;
		}
	}
	/* step6 read data */

	channel[dwChannel] = AUXADC_DRV_ReadReg16(AUXADC_DAT0 + dwChannel * 0x04) & 0x0FFF;

	if (NULL != rawdata)
		*rawdata = channel[dwChannel];
	mt_auxadc_get_cali_data(channel[dwChannel], data, true);

#if !defined(CONFIG_MTK_CLKMGR)
	if (clk_auxadc) {
		clk_disable_unprepare(clk_auxadc);
	} else {
		pr_err("hwdisableClock AUXADC failed.");
		mutex_unlock(&mutex_get_cali_value);
		return -1;
	}
#else
#ifndef CONFIG_MTK_FPGA
	if (disable_clock(MT_PDN_PERI_AUXADC, "AUXADC")) {
		pr_err("hwdisableClock AUXADC failed.");
		mutex_unlock(&mutex_get_cali_value);
		return -1;
	}
#endif
#endif

	mutex_unlock(&mutex_get_cali_value);

	return ret;

}

/* 1v == 1000000 uv */
/* this function voltage Unit is uv */
static int IMM_auxadc_GetOneChannelValue_Cali(int Channel, int *voltage)
{
	int ret = 0, data[4], rawvalue;
	u_int64_t temp_vol;

	ret = IMM_auxadc_GetOneChannelValue(Channel, data, &rawvalue);
	if (ret) {
		pr_err("[adc_api]:IMM_auxadc_GetOneChannelValue_Cali get raw value error %d\n",
		       ret);
		return -1;
	}
	temp_vol = (u_int64_t) rawvalue * 1500000 / AUXADC_PRECISE;
	*voltage = temp_vol;
	/* pr_debug("[adc_api]:IMM_auxadc_GetOneChannelValue_Cali  voltage= %d uv\n",*voltage); */
	return 0;

}

static void mt_auxadc_cal_prepare(void)
{
	/* no voltage calibration */
}

#if defined(CONFIG_AUXADC_NEED_POWER_ON)
static void mt_auxadc_power_on(void)
{
	AUXADC_DRV_SetBits16((volatile u16 *)AUXADC_MISC, 1 << 14);	/* power on ADC */
}
#else
static void mt_auxadc_power_on(void)
{
}
#endif

void mt_auxadc_hal_init(struct platform_device *dev)
{
#ifdef CONFIG_OF
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "mediatek,APMIXED");
	if (node) {
		/* Setup IO addresses */
		auxadc_apmix_base = of_iomap(node, 0);
		pr_debug("[AUXADC] auxadc auxadc_apmix_base=0x%p\n", auxadc_apmix_base);
	} else
		pr_err("[AUXADC] auxadc_apmix_base error\n");

	node = of_find_compatible_node(NULL, NULL, AUXADC_NODE);
	if (!node)
		pr_err("[AUXADC] find node failed\n");

	auxadc_base = of_iomap(node, 0);
	if (!auxadc_base)
		pr_err("[AUXADC] base failed\n");

#if defined(EFUSE_CALI)
	node = of_find_compatible_node(NULL, NULL, "mediatek,EFUSEC");
	if (!node)
		pr_err("[AUXADC] find node failed\n");

	auxadc_efuse_base = of_iomap(node, 0);
	if (!auxadc_efuse_base)
		pr_err("[AUXADC] auxadc_efuse_base base failed\n");

	pr_debug("[AUXADC]: auxadc_efuse_base:0x%p\n", auxadc_efuse_base);
#endif
	pr_debug("[AUXADC]: auxadc:0x%p\n", auxadc_base);

#endif

	mt_auxadc_cal_prepare();
	mt_auxadc_power_on();

	/* AUXADC_DRV_SetBits16((volatile u16 *)AUXADC_CON_RTP, 1);             //disable RTP */
}


#ifdef CONFIG_PM
static void mt_auxadc_hal_suspend(void)
{
	pr_debug("******** MT auxadc driver suspend!! ********\n");
#if !defined(AUXADC_CLOCK_BY_SPM)
#if !defined(CONFIG_MTK_CLKMGR)
	if (clk_auxadc)
		clk_disable_unprepare(clk_auxadc);
#else
#ifndef CONFIG_MTK_FPGA
	if (disable_clock(MT_PDN_PERI_AUXADC, "AUXADC"))
		pr_err("hwEnableClock AUXADC failed.");
#endif
#endif
#endif
}

static void mt_auxadc_hal_resume(void)
{
	pr_debug("******** MT auxadc driver resume!! ********\n");
#if !defined(AUXADC_CLOCK_BY_SPM)
#if !defined(CONFIG_MTK_CLKMGR)
	if (clk_auxadc)
		if (clk_prepare_enable(clk_auxadc))
			pr_err("AUXADC clk_prepare_enable failed!");
#else
#ifndef CONFIG_MTK_FPGA
	if (enable_clock(MT_PDN_PERI_AUXADC, "AUXADC"))
		pr_err("hwEnableClock AUXADC failed!!!.");
#endif
#endif
#endif
	mt_auxadc_power_on();
	/* AUXADC_DRV_SetBits16((volatile u16 *)AUXADC_CON_RTP, 1);             //disable RTP */
}
#endif

static int mt_auxadc_dump_register(char *buf)
{
	pr_debug("[auxadc]: AUXADC_CON0=%x\n", *(volatile u16 *)AUXADC_CON0);
	pr_debug("[auxadc]: AUXADC_CON1=%x\n", *(volatile u16 *)AUXADC_CON1);
	pr_debug("[auxadc]: AUXADC_CON2=%x\n", *(volatile u16 *)AUXADC_CON2);

	return sprintf(buf, "AUXADC_CON0:%x\n AUXADC_CON1:%x\n AUXADC_CON2:%x\n",
		       *(volatile u16 *)AUXADC_CON0, *(volatile u16 *)AUXADC_CON1,
		       *(volatile u16 *)AUXADC_CON2);
}

/*  */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // fop Common API */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
int IMM_IsAdcInitReady(void)
{
	return g_adc_init_flag;
}

int IMM_get_adc_channel_num(char *channel_name, int len)
{
	unsigned int i = 0;
	int ret = 0;

	if (NULL == channel_name) {
		pr_err("[ADC] error: channel_name is NULL!\n");
		return -1;
	}

	pr_debug("[ADC] name=%s, name_len=%d\n", channel_name, len);
	for (i = 0; i < ADC_CHANNEL_MAX; i++) {
		ret = strncmp(channel_name, g_adc_info[i].channel_name, len);
		if (!ret)
			return g_adc_info[i].channel_number;
	}
	pr_err("[ADC] find channel number failed\n");
	return -1;
}

int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata)
{
	return IMM_auxadc_GetOneChannelValue(dwChannel, data, rawdata);
}

/* 1v == 1000000 uv */
/* this function voltage Unit is uv */
int IMM_GetOneChannelValue_Cali(int Channel, int *voltage)
{
	return IMM_auxadc_GetOneChannelValue_Cali(Channel, voltage);
}


/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // fop API */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static long auxadc_cali_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int i = 0, ret = 0;
	long *user_data_addr;
	long *nvram_data_addr;

	mutex_lock(&auxadc_mutex);

	switch (cmd) {
	case TEST_ADC_CALI_PRINT:
		g_AUXADC_Cali = false;
		break;

	case SET_ADC_CALI_Slop:
		nvram_data_addr = (long *)arg;
		ret = copy_from_user(auxadc_cali_slop, nvram_data_addr, 36);
		g_AUXADC_Cali = false;
		/* Protection */
		for (i = 0; i < ADC_CHANNEL_MAX; i++) {
			if ((*(auxadc_cali_slop + i) == 0) || (*(auxadc_cali_slop + i) == 1))
				*(auxadc_cali_slop + i) = 1000;
		}
		for (i = 0; i < ADC_CHANNEL_MAX; i++)
			pr_debug("auxadc_cali_slop[%d] = %d\n", i, *(auxadc_cali_slop + i));

		pr_debug("**** MT auxadc_cali ioctl : SET_ADC_CALI_Slop Done!\n");
		break;

	case SET_ADC_CALI_Offset:
		nvram_data_addr = (long *)arg;
		ret = copy_from_user(auxadc_cali_offset, nvram_data_addr, 36);
		g_AUXADC_Cali = false;
		for (i = 0; i < ADC_CHANNEL_MAX; i++)
			pr_debug("auxadc_cali_offset[%d] = %d\n", i, *(auxadc_cali_offset + i));

		pr_debug("**** MT auxadc_cali ioctl : SET_ADC_CALI_Offset Done!\n");
		break;

	case SET_ADC_CALI_Cal:
		nvram_data_addr = (long *)arg;
		ret = copy_from_user(auxadc_cali_cal, nvram_data_addr, 4);
		g_AUXADC_Cali = true;	/* enable calibration after setting AUXADC_CALI_Cal */
		if (auxadc_cali_cal[0] == 1)
			g_AUXADC_Cali = true;
		else
			g_AUXADC_Cali = false;

		for (i = 0; i < 1; i++)
			pr_debug("auxadc_cali_cal[%d] = %d\n", i, *(auxadc_cali_cal + i));

		pr_debug("**** MT auxadc_cali ioctl : SET_ADC_CALI_Cal Done!\n");
		break;

	case ADC_CHANNEL_READ:
		g_AUXADC_Cali = false;	/* 20100508 Infinity */
		user_data_addr = (long *)arg;
		ret = copy_from_user(auxadc_in_data, user_data_addr, 8);	/* 2*int = 2*4 */

		pr_debug("this api is removed !!\n");
		ret = copy_to_user(user_data_addr, auxadc_out_data, 8);
		pr_debug("**** ioctl : AUXADC Channel %d * %d times = %d\n", auxadc_in_data[0],
			 auxadc_in_data[1], auxadc_out_data[0]);
		break;

	default:
		g_AUXADC_Cali = false;
		break;
	}

	mutex_unlock(&auxadc_mutex);

	return 0;
}

#ifdef CONFIG_COMPAT
static long compat_auxadc_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		pr_err("compat_ion_ioctl file has no f_op or no f_op->unlocked_ioctl.\n");
		return -ENOTTY;
	}

	switch (cmd) {
	case TEST_ADC_CALI_PRINT:
	case SET_ADC_CALI_Slop:
	case SET_ADC_CALI_Offset:
	case SET_ADC_CALI_Cal:
	case ADC_CHANNEL_READ:
		return filp->f_op->unlocked_ioctl(filp, cmd,
			(unsigned long)compat_ptr(arg));
	default:
		pr_err("compat_ion_ioctl : No such command!! 0x%x\n", cmd);
		return -ENOIOCTLCMD;
	}
}
#endif

static int auxadc_cali_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int auxadc_cali_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations auxadc_cali_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = auxadc_cali_unlocked_ioctl,
	.open = auxadc_cali_open,
	.release = auxadc_cali_release,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = compat_auxadc_unlocked_ioctl,
#endif
};

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : AUXADC_Channel_X_Slope/Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
#if ADC_CHANNEL_MAX > 0
static ssize_t show_AUXADC_Channel_0_Slope(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_slop + 0));
	pr_debug("[EM] AUXADC_Channel_0_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_0_Slope(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_0_Slope, 0664, show_AUXADC_Channel_0_Slope,
		   store_AUXADC_Channel_0_Slope);
static ssize_t show_AUXADC_Channel_0_Offset(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_offset + 0));
	pr_debug("[EM] AUXADC_Channel_0_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_0_Offset(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_0_Offset, 0664, show_AUXADC_Channel_0_Offset,
		   store_AUXADC_Channel_0_Offset);
#endif


#if ADC_CHANNEL_MAX > 1
static ssize_t show_AUXADC_Channel_1_Slope(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_slop + 1));
	pr_debug("[EM] AUXADC_Channel_1_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_1_Slope(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_1_Slope, 0664, show_AUXADC_Channel_1_Slope,
		   store_AUXADC_Channel_1_Slope);
static ssize_t show_AUXADC_Channel_1_Offset(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_offset + 1));
	pr_debug("[EM] AUXADC_Channel_1_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_1_Offset(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_1_Offset, 0664, show_AUXADC_Channel_1_Offset,
		   store_AUXADC_Channel_1_Offset);
#endif


#if ADC_CHANNEL_MAX > 2
static ssize_t show_AUXADC_Channel_2_Slope(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_slop + 2));
	pr_debug("[EM] AUXADC_Channel_2_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_2_Slope(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_2_Slope, 0664, show_AUXADC_Channel_2_Slope,
		   store_AUXADC_Channel_2_Slope);
static ssize_t show_AUXADC_Channel_2_Offset(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_offset + 2));
	pr_debug("[EM] AUXADC_Channel_2_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_2_Offset(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_2_Offset, 0664, show_AUXADC_Channel_2_Offset,
		   store_AUXADC_Channel_2_Offset);
#endif


#if ADC_CHANNEL_MAX > 3
static ssize_t show_AUXADC_Channel_3_Slope(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_slop + 3));
	pr_debug("[EM] AUXADC_Channel_3_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_3_Slope(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_3_Slope, 0664, show_AUXADC_Channel_3_Slope,
		   store_AUXADC_Channel_3_Slope);
static ssize_t show_AUXADC_Channel_3_Offset(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_offset + 3));
	pr_debug("[EM] AUXADC_Channel_3_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_3_Offset(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_3_Offset, 0664, show_AUXADC_Channel_3_Offset,
		   store_AUXADC_Channel_3_Offset);
#endif


#if ADC_CHANNEL_MAX > 4
static ssize_t show_AUXADC_Channel_4_Slope(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_slop + 4));
	pr_debug("[EM] AUXADC_Channel_4_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_4_Slope(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_4_Slope, 0664, show_AUXADC_Channel_4_Slope,
		   store_AUXADC_Channel_4_Slope);
static ssize_t show_AUXADC_Channel_4_Offset(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_offset + 4));
	pr_debug("[EM] AUXADC_Channel_4_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_4_Offset(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_4_Offset, 0664, show_AUXADC_Channel_4_Offset,
		   store_AUXADC_Channel_4_Offset);
#endif


#if ADC_CHANNEL_MAX > 5
static ssize_t show_AUXADC_Channel_5_Slope(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_slop + 5));
	pr_debug("[EM] AUXADC_Channel_5_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_5_Slope(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_5_Slope, 0664, show_AUXADC_Channel_5_Slope,
		   store_AUXADC_Channel_5_Slope);
static ssize_t show_AUXADC_Channel_5_Offset(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_offset + 5));
	pr_debug("[EM] AUXADC_Channel_5_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_5_Offset(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_5_Offset, 0664, show_AUXADC_Channel_5_Offset,
		   store_AUXADC_Channel_5_Offset);
#endif


#if ADC_CHANNEL_MAX > 6
static ssize_t show_AUXADC_Channel_6_Slope(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_slop + 6));
	pr_debug("[EM] AUXADC_Channel_6_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_6_Slope(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_6_Slope, 0664, show_AUXADC_Channel_6_Slope,
		   store_AUXADC_Channel_6_Slope);
static ssize_t show_AUXADC_Channel_6_Offset(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_offset + 6));
	pr_debug("[EM] AUXADC_Channel_6_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_6_Offset(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_6_Offset, 0664, show_AUXADC_Channel_6_Offset,
		   store_AUXADC_Channel_6_Offset);
#endif


#if ADC_CHANNEL_MAX > 7
static ssize_t show_AUXADC_Channel_7_Slope(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_slop + 7));
	pr_debug("[EM] AUXADC_Channel_7_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_7_Slope(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_7_Slope, 0664, show_AUXADC_Channel_7_Slope,
		   store_AUXADC_Channel_7_Slope);
static ssize_t show_AUXADC_Channel_7_Offset(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_offset + 7));
	pr_debug("[EM] AUXADC_Channel_7_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_7_Offset(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_7_Offset, 0664, show_AUXADC_Channel_7_Offset,
		   store_AUXADC_Channel_7_Offset);
#endif


#if ADC_CHANNEL_MAX > 8
static ssize_t show_AUXADC_Channel_8_Slope(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_slop + 8));
	pr_debug("[EM] AUXADC_Channel_8_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_8_Slope(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_8_Slope, 0664, show_AUXADC_Channel_8_Slope,
		   store_AUXADC_Channel_8_Slope);
static ssize_t show_AUXADC_Channel_8_Offset(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_offset + 8));
	pr_debug("[EM] AUXADC_Channel_8_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_8_Offset(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_8_Offset, 0664, show_AUXADC_Channel_8_Offset,
		   store_AUXADC_Channel_8_Offset);
#endif


#if ADC_CHANNEL_MAX > 9
static ssize_t show_AUXADC_Channel_9_Slope(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_slop + 9));
	pr_debug("[EM] AUXADC_Channel_9_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_9_Slope(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_9_Slope, 0664, show_AUXADC_Channel_9_Slope,
		   store_AUXADC_Channel_9_Slope);
static ssize_t show_AUXADC_Channel_9_Offset(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_offset + 9));
	pr_debug("[EM] AUXADC_Channel_9_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_9_Offset(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_9_Offset, 0664, show_AUXADC_Channel_9_Offset,
		   store_AUXADC_Channel_9_Offset);
#endif


#if ADC_CHANNEL_MAX > 10
static ssize_t show_AUXADC_Channel_10_Slope(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_slop + 10));
	pr_debug("[EM] AUXADC_Channel_10_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_10_Slope(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_10_Slope, 0664, show_AUXADC_Channel_10_Slope,
		   store_AUXADC_Channel_10_Slope);
static ssize_t show_AUXADC_Channel_10_Offset(struct device *dev, struct device_attribute *attr,
					     char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_offset + 10));
	pr_debug("[EM] AUXADC_Channel_10_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_10_Offset(struct device *dev, struct device_attribute *attr,
					      const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_10_Offset, 0664, show_AUXADC_Channel_10_Offset,
		   store_AUXADC_Channel_10_Offset);
#endif


#if ADC_CHANNEL_MAX > 11
static ssize_t show_AUXADC_Channel_11_Slope(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_slop + 11));
	pr_debug("[EM] AUXADC_Channel_11_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_11_Slope(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_11_Slope, 0664, show_AUXADC_Channel_11_Slope,
		   store_AUXADC_Channel_11_Slope);
static ssize_t show_AUXADC_Channel_11_Offset(struct device *dev, struct device_attribute *attr,
					     char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_offset + 11));
	pr_debug("[EM] AUXADC_Channel_11_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_11_Offset(struct device *dev, struct device_attribute *attr,
					      const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_11_Offset, 0664, show_AUXADC_Channel_11_Offset,
		   store_AUXADC_Channel_11_Offset);
#endif


#if ADC_CHANNEL_MAX > 12
static ssize_t show_AUXADC_Channel_12_Slope(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_slop + 12));
	pr_debug("[EM] AUXADC_Channel_12_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_12_Slope(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_12_Slope, 0664, show_AUXADC_Channel_12_Slope,
		   store_AUXADC_Channel_12_Slope);
static ssize_t show_AUXADC_Channel_12_Offset(struct device *dev, struct device_attribute *attr,
					     char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_offset + 12));
	pr_debug("[EM] AUXADC_Channel_12_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_12_Offset(struct device *dev, struct device_attribute *attr,
					      const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_12_Offset, 0664, show_AUXADC_Channel_12_Offset,
		   store_AUXADC_Channel_12_Offset);
#endif


#if ADC_CHANNEL_MAX > 13
static ssize_t show_AUXADC_Channel_13_Slope(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_slop + 13));
	pr_debug("[EM] AUXADC_Channel_13_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_13_Slope(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_13_Slope, 0664, show_AUXADC_Channel_13_Slope,
		   store_AUXADC_Channel_13_Slope);
static ssize_t show_AUXADC_Channel_13_Offset(struct device *dev, struct device_attribute *attr,
					     char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_offset + 13));
	pr_debug("[EM] AUXADC_Channel_13_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_13_Offset(struct device *dev, struct device_attribute *attr,
					      const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_13_Offset, 0664, show_AUXADC_Channel_13_Offset,
		   store_AUXADC_Channel_13_Offset);
#endif


#if ADC_CHANNEL_MAX > 14
static ssize_t show_AUXADC_Channel_14_Slope(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_slop + 14));
	pr_debug("[EM] AUXADC_Channel_14_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_14_Slope(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_14_Slope, 0664, show_AUXADC_Channel_14_Slope,
		   store_AUXADC_Channel_14_Slope);
static ssize_t show_AUXADC_Channel_14_Offset(struct device *dev, struct device_attribute *attr,
					     char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_offset + 14));
	pr_debug("[EM] AUXADC_Channel_14_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_14_Offset(struct device *dev, struct device_attribute *attr,
					      const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_14_Offset, 0664, show_AUXADC_Channel_14_Offset,
		   store_AUXADC_Channel_14_Offset);
#endif


#if ADC_CHANNEL_MAX > 15
static ssize_t show_AUXADC_Channel_15_Slope(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_slop + 15));
	pr_debug("[EM] AUXADC_Channel_15_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_15_Slope(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_15_Slope, 0664, show_AUXADC_Channel_15_Slope,
		   store_AUXADC_Channel_15_Slope);
static ssize_t show_AUXADC_Channel_15_Offset(struct device *dev, struct device_attribute *attr,
					     char *buf)
{
	int ret_value = 1;

	ret_value = (*(auxadc_cali_offset + 15));
	pr_debug("[EM] AUXADC_Channel_15_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_15_Offset(struct device *dev, struct device_attribute *attr,
					      const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_15_Offset, 0664, show_AUXADC_Channel_15_Offset,
		   store_AUXADC_Channel_15_Offset);
#endif

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : AUXADC_Channel_Is_Calibration */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_AUXADC_Channel_Is_Calibration(struct device *dev, struct device_attribute *attr,
						  char *buf)
{
	int ret_value = 2;

	ret_value = g_AUXADC_Cali;
	pr_debug("[EM] AUXADC_Channel_Is_Calibration : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_Is_Calibration(struct device *dev,
						   struct device_attribute *attr, const char *buf,
						   size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_Is_Calibration, 0664, show_AUXADC_Channel_Is_Calibration,
		   store_AUXADC_Channel_Is_Calibration);

static ssize_t show_AUXADC_register(struct device *dev, struct device_attribute *attr, char *buf)
{
	return mt_auxadc_dump_register(buf);
}

static ssize_t store_AUXADC_register(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t size)
{
	pr_debug("[EM] Not Support store_AUXADC_register\n");
	return size;
}

static DEVICE_ATTR(AUXADC_register, 0664, show_AUXADC_register, store_AUXADC_register);


static ssize_t show_AUXADC_chanel(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* read data */
	int i = 0, data[4] = { 0, 0, 0, 0 };
	char buf_temp[256];
	int res = 0;

	for (i = 0; i < 5; i++) {
		res = IMM_auxadc_GetOneChannelValue(i, data, NULL);
		if (res < 0) {
			pr_debug("[adc_driver]: get data error\n");
		} else {
			pr_debug("[adc_driver]: channel[%d]=%d.%d\n", i, data[0], data[1]);
			sprintf(buf_temp, "channel[%d]=%d.%d\n", i, data[0], data[1]);
			strncat(buf, buf_temp, strlen(buf_temp));
		}
	}
	mt_auxadc_dump_register(buf_temp);
	strncat(buf, buf_temp, strlen(buf_temp));
	return strlen(buf);
}

static int dbug_thread(void *unused)
{
	int i = 0, data[4] = { 0, 0, 0, 0 };
	int res = 0;
	int rawdata = 0;
	int cali_voltage = 0;

	while (g_start_debug_thread) {
		for (i = 0; i < ADC_CHANNEL_MAX; i++) {
			res = IMM_auxadc_GetOneChannelValue(i, data, &rawdata);
			if (res < 0) {
				pr_debug("[adc_driver]: get data error\n");
			} else {
				pr_debug("[adc_driver]: channel[%d]raw =%d\n", i, rawdata);
				pr_debug("[adc_driver]: channel[%d]=%d.%.02d\n", i, data[0],
					 data[1]);
			}
			res = IMM_auxadc_GetOneChannelValue_Cali(i, &cali_voltage);
			if (res < 0) {
				pr_debug("[adc_driver]: get cali voltage error\n");
			} else
				pr_debug("[adc_driver]: channel[%d] cali_voltage =%d\n", i,
					 cali_voltage);

			msleep(500);

		}
		msleep(500);

	}
	return 0;
}


static ssize_t store_AUXADC_channel(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t size)
{
	char start_flag[10];
	int error;

	if (strlen(buf) != 1) {
		pr_debug("[adc_driver]: Invalid values\n");
		return -EINVAL;
	} else
		snprintf(start_flag, sizeof(start_flag), "%s", buf);
	pr_debug("[adc_driver] start flag =%d\n", start_flag[0]);
	g_start_debug_thread = start_flag[0];
	if ('1' == start_flag[0]) {
		thread = kthread_run(dbug_thread, 0, "AUXADC");

		if (IS_ERR(thread)) {
			error = PTR_ERR(thread);
			pr_debug("[adc_driver] failed to create kernel thread: %d\n", error);
		}
	}

	return size;
}

static DEVICE_ATTR(AUXADC_read_channel, 0664, show_AUXADC_chanel, store_AUXADC_channel);

static int mt_auxadc_create_device_attr(struct device *dev)
{
	int ret = 0;
	/* For EM */
	ret = device_create_file(dev, &dev_attr_AUXADC_register);
	if (ret != 0)
		goto exit;
	ret = device_create_file(dev, &dev_attr_AUXADC_read_channel);
	if (ret != 0)
		goto exit;
#if ADC_CHANNEL_MAX > 0
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_0_Slope);
	if (ret != 0)
		goto exit;
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_0_Offset);
	if (ret != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX > 1
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_1_Slope);
	if (ret != 0)
		goto exit;
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_1_Offset);
	if (ret != 0)
		goto exit;

#endif
#if ADC_CHANNEL_MAX > 2
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_2_Slope);
	if (ret != 0)
		goto exit;
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_2_Offset);
	if (ret != 0)
		goto exit;

#endif
#if ADC_CHANNEL_MAX > 3
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_3_Slope);
	if (ret != 0)
		goto exit;
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_3_Offset);
	if (ret != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX > 4
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_4_Slope);
	if (ret != 0)
		goto exit;
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_4_Offset);
	if (ret != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX > 5
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_5_Slope);
	if (ret != 0)
		goto exit;
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_5_Offset);
	if (ret != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX > 6
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_6_Slope);
	if (ret != 0)
		goto exit;
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_6_Offset);
	if (ret != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX > 7
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_7_Slope);
	if (ret != 0)
		goto exit;
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_7_Offset);
	if (ret != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX > 8
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_8_Slope);
	if (ret != 0)
		goto exit;
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_8_Offset);
	if (ret != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX > 9
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_9_Slope);
	if (ret != 0)
		goto exit;
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_9_Offset);
	if (ret != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX > 10
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_10_Slope);
	if (ret != 0)
		goto exit;
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_10_Offset);
	if (ret != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX > 11
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_11_Slope);
	if (ret != 0)
		goto exit;
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_11_Offset);
	if (ret != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX > 12
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_12_Slope);
	if (ret != 0)
		goto exit;
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_12_Offset);
	if (ret != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX > 13
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_13_Slope);
	if (ret != 0)
		goto exit;
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_13_Offset);
	if (ret != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX > 14
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_14_Slope);
	if (ret != 0)
		goto exit;
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_14_Offset);
	if (ret != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX > 15
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_15_Slope);
	if (ret != 0)
		goto exit;
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_15_Offset);
	if (ret != 0)
		goto exit;
#endif
	ret = device_create_file(dev, &dev_attr_AUXADC_Channel_Is_Calibration);
	if (ret != 0)
		goto exit;

	return 0;
exit:
	return 1;
}


static int proc_utilization_show(struct seq_file *m, void *v)
{
	int i, res;
	int data[4] = { 0, 0, 0, 0 };

	seq_puts(m, "********** Auxadc status dump **********\n");

	seq_printf(m, "reg=0x%x ADC_GE_A=%d ADC_OE_A=%d GE:%d OE:%d gain:0x%x\n",
	cali_reg, cali_ge_a, cali_oe_a, cali_ge, cali_oe, gain);
#if defined(EFUSE_CALI)
	seq_printf(m, "ADC_GE_A_MASK:0x%x ADC_GE_A_SHIFT:0x%x\n", ADC_GE_A_MASK, ADC_GE_A_SHIFT);
	seq_printf(m, "ADC_OE_A_MASK:0x%x ADC_OE_A_SHIFT:0x%x\n", ADC_OE_A_MASK, ADC_OE_A_SHIFT);
	seq_printf(m, "ADC_CALI_EN_A_MASK:0x%x ADC_CALI_EN_A_SHIFT:0x%x\n", ADC_CALI_EN_A_MASK,
		ADC_CALI_EN_A_SHIFT);
#endif
	for (i = 100; i <= 1000; i = i + 100) {
		mt_auxadc_get_cali_data(i, data, true);
		seq_printf(m, "raw:%d data:%d %d %d with cali\n", i, data[0], data[1], data[2]);
		mt_auxadc_get_cali_data(i, data, false);
		seq_printf(m, "raw:%d data:%d %d %d without cali\n", i, data[0], data[1], data[2]);
	}

	for (i = 0; i < 5; i++) {
		res = IMM_auxadc_GetOneChannelValue(i, data, NULL);
		if (res < 0)
			seq_printf(m, "[adc_driver]: get data error res:%d\n", res);
		else
			seq_printf(m, "channel[%d]=%d.%d %d\n", i, data[0], data[1], data[2]);
	}

	return 0;
}

static int proc_utilization_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_utilization_show, NULL);
}

static const struct file_operations auxadc_debug_proc_fops = {
	.open = proc_utilization_open,
	.read = seq_read,
};


static void adc_debug_init(void)
{
	struct proc_dir_entry *mt_auxadc_dir;

	mt_auxadc_dir = proc_mkdir("mt-auxadc", NULL);
	if (!mt_auxadc_dir) {
		pr_err("fail to mkdir /proc/mt-auxadc\n");
		return;
	}
	proc_create("dump_auxadc_status", S_IRUGO | S_IWUSR, mt_auxadc_dir, &auxadc_debug_proc_fops);
	pr_err("proc_create auxadc_debug_proc_fops\n");
}



/* platform_driver API */
static int mt_auxadc_probe(struct platform_device *dev)
{
	int ret = 0;
	struct device *adc_dev = NULL;

#ifdef CONFIG_OF
	int used_channel_counter = 0;
	int of_value = 0;
#endif

#ifdef CONFIG_OF
	struct device_node *node;
#endif

	pr_debug("******** MT AUXADC driver probe!! ********\n");

#if !defined(CONFIG_MTK_CLKMGR)
#else
#ifndef CONFIG_MTK_FPGA
	if (clock_is_on(MT_PDN_PERI_AUXADC) == 0) {
		if (enable_clock(MT_PDN_PERI_AUXADC, "AUXADC"))
			pr_debug("hwEnableClock AUXADC failed.");
	}
#endif
#endif

	/* Integrate with NVRAM */
	ret = alloc_chrdev_region(&auxadc_cali_devno, 0, 1, AUXADC_CALI_DEVNAME);
	if (ret)
		pr_err("[AUXADC_AP]Error: Can't Get Major number for auxadc_cali\n");

	auxadc_cali_cdev = cdev_alloc();
	auxadc_cali_cdev->owner = THIS_MODULE;
	auxadc_cali_cdev->ops = &auxadc_cali_fops;
	ret = cdev_add(auxadc_cali_cdev, auxadc_cali_devno, 1);
	if (ret)
		pr_err("auxadc_cali Error: cdev_add\n");

	auxadc_cali_major = MAJOR(auxadc_cali_devno);
	auxadc_cali_class = class_create(THIS_MODULE, AUXADC_CALI_DEVNAME);
	adc_dev = (struct device *)device_create(auxadc_cali_class,
						 NULL, auxadc_cali_devno, NULL,
						 AUXADC_CALI_DEVNAME);
	pr_debug("[MT AUXADC_probe] NVRAM prepare : done !!\n");

	/* read calibration data from EFUSE */
	mt_auxadc_hal_init(dev);

	pr_debug("[MT AUXADC_probe2] mt_auxadc_hal_init : done !!\n");

#ifdef CONFIG_OF
	pr_warn("[MT AUXADC_probe3] get device tree info : start !!\n");

	node = of_find_compatible_node(NULL, NULL, "mediatek,adc_channel");
	if (node) {
		ret = of_property_read_u32_array(node, "mediatek,temperature0", &of_value, 1);
		if (ret == 0) {
			sprintf(g_adc_info[used_channel_counter].channel_name, "ADC_RFTMP");
			g_adc_info[used_channel_counter].channel_number = of_value;
			pr_warn("[AUXADC_AP] find node TEMPERATURE:%d\n", of_value);
			used_channel_counter++;
		}
		ret = of_property_read_u32_array(node, "mediatek,temperature1", &of_value, 1);
		if (ret == 0) {
			sprintf(g_adc_info[used_channel_counter].channel_name, "ADC_APTMP");
			g_adc_info[used_channel_counter].channel_number = of_value;
			pr_warn("[AUXADC_AP] find node TEMPERATURE1:%d\n", of_value);
			used_channel_counter++;
		}
		ret =
		    of_property_read_u32_array(node, "mediatek,adc_fdd_rf_params_dynamic_custom_ch",
					       &of_value, 1);
		if (ret == 0) {
			sprintf(g_adc_info[used_channel_counter].channel_name,
				"ADC_FDD_Rf_Params_Dynamic_Custom");
			g_adc_info[used_channel_counter].channel_number = of_value;
			pr_warn("[AUXADC_AP] find node ADC_FDD_RF_PARAMS_DYNAMIC_CUSTOM_CH:%d\n",
				 of_value);
			used_channel_counter++;
		}
		ret = of_property_read_u32_array(node, "mediatek,hf_mic", &of_value, 1);
		if (ret == 0) {
			sprintf(g_adc_info[used_channel_counter].channel_name, "ADC_MIC");
			g_adc_info[used_channel_counter].channel_number = of_value;
			pr_warn("[AUXADC_AP] find node HF_MIC:%d\n", of_value);
			used_channel_counter++;
		}
		ret = of_property_read_u32_array(node, "mediatek,lcm_voltage", &of_value, 1);
		if (ret == 0) {
			sprintf(g_adc_info[used_channel_counter].channel_name, "ADC_LCM_VOLTAGE");
			g_adc_info[used_channel_counter].channel_number = of_value;
			pr_warn("[AUXADC_AP] find node LCM_VOLTAGE:%d\n", of_value);
			used_channel_counter++;
		}
		ret = of_property_read_u32_array(node, "mediatek,battery_voltage", &of_value, 1);
		if (ret == 0) {
			sprintf(g_adc_info[used_channel_counter].channel_name,
				"ADC_BATTERY_VOLTAGE");
			g_adc_info[used_channel_counter].channel_number = of_value;
			pr_warn("[AUXADC_AP] find node BATTERY_VOLTAGE:%d\n", of_value);
			used_channel_counter++;
		}
		ret = of_property_read_u32_array(node, "mediatek,charger_voltage", &of_value, 1);
		if (ret == 0) {
			sprintf(g_adc_info[used_channel_counter].channel_name,
				"ADC_CHARGER_VOLTAGE");
			g_adc_info[used_channel_counter].channel_number = of_value;
			pr_warn("[AUXADC_AP] find node CHARGER_VOLTAGE:%d\n", of_value);
			used_channel_counter++;
		}
		ret = of_property_read_u32_array(node, "mediatek,utms", &of_value, 1);
		if (ret == 0) {
			sprintf(g_adc_info[used_channel_counter].channel_name, "ADC_UTMS");
			g_adc_info[used_channel_counter].channel_number = of_value;
			pr_warn("[AUXADC_AP] find node UTMS:%d\n", of_value);
			used_channel_counter++;
		}
		ret = of_property_read_u32_array(node, "mediatek,ref_current", &of_value, 1);
		if (ret == 0) {
			sprintf(g_adc_info[used_channel_counter].channel_name, "ADC_REF_CURRENT");
			g_adc_info[used_channel_counter].channel_number = of_value;
			pr_warn("[AUXADC_AP] find node REF_CURRENT:%d\n", of_value);
			used_channel_counter++;
		}
		ret = of_property_read_u32_array(node, "mediatek,board_id", &of_value, 1);
		if (ret == 0) {
			sprintf(g_adc_info[used_channel_counter].channel_name, "BOARD_ID");
			g_adc_info[used_channel_counter].channel_number = of_value;
			pr_warn("[AUXADC_AP] find node BOARD_ID:%d\n", of_value);
			used_channel_counter++;
		}
		ret = of_property_read_u32_array(node, "mediatek,board_id_2", &of_value, 1);
		if (ret == 0) {
			sprintf(g_adc_info[used_channel_counter].channel_name, "BOARD_ID_2");
			g_adc_info[used_channel_counter].channel_number = of_value;
			pr_warn("[AUXADC_AP] find node BOARD_ID_2:%d\n", of_value);
			used_channel_counter++;
		}
		ret = of_property_read_u32_array(node, "mediatek,board_id_3", &of_value, 1);
		if (ret == 0) {
			sprintf(g_adc_info[used_channel_counter].channel_name, "BOARD_ID_3");
			g_adc_info[used_channel_counter].channel_number = of_value;
			pr_warn("[AUXADC_AP] find node BOARD_ID_3:%d\n", of_value);
			used_channel_counter++;
		}
	} else {
		pr_err("[AUXADC_AP] find node failed\n");
	}
#endif

	pr_warn("[MT AUXADC_AP] adc_channel_info_init : done !!\n");

#if !defined(CONFIG_MTK_CLKMGR)
#ifdef CONFIG_OF
	clk_auxadc = devm_clk_get(&dev->dev, "auxadc-main");
	if (!clk_auxadc) {
		pr_err("[AUXADC] devm_clk_get failed\n");
		return -1;
	}
	pr_debug("[AUXADC]: auxadc CLK:0x%p\n", clk_auxadc);
	ret = clk_prepare_enable(clk_auxadc);
	if (ret)
		pr_err("hwEnableClock AUXADC failed.");
#endif
#else
#endif

	adc_debug_init();

	g_adc_init_flag = 1;

	mt_auxadc_create_device_attr(adc_dev);
	return ret;
}

static int mt_auxadc_remove(struct platform_device *dev)
{
	pr_debug("******** MT auxadc driver remove!! ********\n");
	return 0;
}

static void mt_auxadc_shutdown(struct platform_device *dev)
{
	pr_debug("******** MT auxadc driver shutdown!! ********\n");
}


#ifdef CONFIG_PM
static int mt_auxadc_suspend(struct platform_device *dev, pm_message_t state)
{
	/* pr_debug("******** MT auxadc driver suspend!! ********\n" ); */
	mt_auxadc_hal_suspend();
	return 0;
}

static int mt_auxadc_resume(struct platform_device *dev)
{
	/* pr_debug("******** MT auxadc driver resume!! ********\n" ); */
	mt_auxadc_hal_resume();
	return 0;
}
#endif

#ifdef CONFIG_OF
static const struct of_device_id mt_auxadc_of_match[] = {
	{.compatible = "mediatek,mt6735-auxadc",},
	{.compatible = "mediatek,mt6797-auxadc",},
	{.compatible = "mediatek,mt6755-auxadc",},
	{.compatible = "mediatek,mt6757-auxadc",},
	{.compatible = "mediatek,elbrus-auxadc",},
	{.compatible = "mediatek,ap-auxadc",},
	{},
};
#endif

static struct platform_driver mt_auxadc_driver = {
	.probe = mt_auxadc_probe,
	.remove = mt_auxadc_remove,
	.shutdown = mt_auxadc_shutdown,
#ifdef CONFIG_PM
	.suspend = mt_auxadc_suspend,
	.resume = mt_auxadc_resume,
#endif
	.driver = {
		   .name = "mt-auxadc",
#ifdef CONFIG_OF
		   .of_match_table = mt_auxadc_of_match,
#endif
		   },
};

static int __init mt_auxadc_init(void)
{
	int ret;

#if !defined(CONFIG_MTK_CLKMGR)
#else
#ifndef CONFIG_MTK_FPGA
	if (enable_clock(MT_PDN_PERI_AUXADC, "AUXADC"))
		pr_err("hwEnableClock AUXADC failed.");
#endif
#endif

	ret = platform_driver_register(&mt_auxadc_driver);
	if (ret) {
		pr_err("****[mt_auxadc_driver] Unable to register driver (%d)\n", ret);
		return ret;
	}
	pr_debug("****[mt_auxadc_driver] Initialization : DONE\n");

	return 0;
}

static void __exit mt_auxadc_exit(void)
{
}

module_init(mt_auxadc_init);
module_exit(mt_auxadc_exit);

MODULE_AUTHOR("MTK");
MODULE_DESCRIPTION("MTK AUXADC Device Driver");
MODULE_LICENSE("GPL");
