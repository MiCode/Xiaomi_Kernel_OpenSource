// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <generated/autoconf.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/atomic.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/clk.h>
#include <mt-plat/mtk_pwm.h>
#include <mach/mtk_pwm_prv.h>
#include <mt-plat/mtk_pwm_hal_pub.h>
#include <mt-plat/mtk_pwm_hal.h>

#define PWM_LDVT_FLAG		0
#if PWM_LDVT_FLAG
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
static u8 intr_pwm_nu[PWM_MAX];
#endif

#define T         "[PWM]"

void __iomem *pwm_base;
struct mutex pwm_power_lock;

struct pwm_device {
	const char	  *name;
	atomic_t		ref;
	dev_t		   devno;
	spinlock_t	  lock;
	unsigned long	power_flag;
	struct device   dev;
};

static struct pwm_device pwm_dat = {
	.name = PWM_DEVICE,
	.ref = ATOMIC_INIT(0),
	.power_flag = 0,
	.lock = __SPIN_LOCK_UNLOCKED(pwm_dat.lock)
};

static struct pwm_device *pwm_dev = &pwm_dat;

static void mt_pwm_power_on(u32 pwm_no, bool pmic_pad)
{
	mutex_lock(&pwm_power_lock);

	mt_pwm_power_on_hal(pwm_no, pmic_pad, &(pwm_dev->power_flag));

	mutex_unlock(&pwm_power_lock);
}

static void mt_pwm_power_off(u32 pwm_no, bool pmic_pad)
{
	mutex_lock(&pwm_power_lock);

	mt_pwm_power_off_hal(pwm_no, pmic_pad, &(pwm_dev->power_flag));

	mutex_unlock(&pwm_power_lock);
}

static s32 mt_pwm_sel_pmic(u32 pwm_no)
{
	unsigned long flags;
	s32 ret;
	struct pwm_device *dev = pwm_dev;

	spin_lock_irqsave(&dev->lock, flags);
	ret = mt_pwm_sel_pmic_hal(pwm_no);
	spin_unlock_irqrestore(&dev->lock, flags);

	if (ret == (-EEXCESSPWMNO))
		pr_debug(T "PWM1~PWM4 not support pmic_pad\n");

	return ret;
}

static s32 mt_pwm_sel_ap(u32 pwm_no)
{
	unsigned long flags;
	s32 ret;
	struct pwm_device *dev = pwm_dev;

	spin_lock_irqsave(&dev->lock, flags);
	ret = mt_pwm_sel_ap_hal(pwm_no);
	spin_unlock_irqrestore(&dev->lock, flags);

	if (ret == (-EEXCESSPWMNO))
		pr_debug(T "PWM1~PWM4 not support pmic_pad\n");

	return ret;
}

/*******************************************************
 *   Set PWM_ENABLE register bit to enable pwm1~pwm7
 *
 ********************************************************/
static s32 mt_set_pwm_enable(u32 pwm_no)
{
	unsigned long flags;

	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid!\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		pr_debug(T "pwm number is not between PWM1~PWM7\n");
		return -EEXCESSPWMNO;
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_pwm_enable_hal(pwm_no);
	spin_unlock_irqrestore(&dev->lock, flags);

	return RSUCCESS;
}


static s32 mt_set_pwm_disable(u32 pwm_no)
{
	unsigned long flags;
	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		pr_debug(T "pwm number is not between PWM1~PWM7\n");
		return -EEXCESSPWMNO;
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_pwm_disable_hal(pwm_no);
	spin_unlock_irqrestore(&dev->lock, flags);

	mdelay(1);

	return RSUCCESS;
}

void mt_pwm_disable(u32 pwm_no, u8 pmic_pad)
{
	mt_set_pwm_disable(pwm_no);
	mt_pwm_power_off(pwm_no, pmic_pad);
}
EXPORT_SYMBOL(mt_pwm_disable);

void mt_set_pwm_enable_seqmode(void)
{
	unsigned long flags;
	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return;
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_pwm_enable_seqmode_hal();
	spin_unlock_irqrestore(&dev->lock, flags);
}

void mt_set_pwm_disable_seqmode(void)
{
	unsigned long flags;
	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return;
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_pwm_disable_seqmode_hal();
	spin_unlock_irqrestore(&dev->lock, flags);
}

s32 mt_set_pwm_test_sel(u32 val)
{
	unsigned long flags;
	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not pwm_dev\n");
		return -EINVALID;
	}

	spin_lock_irqsave(&dev->lock, flags);
	if (mt_set_pwm_test_sel_hal(val))
		goto err;
	spin_unlock_irqrestore(&dev->lock, flags);
	return RSUCCESS;

err:
	spin_unlock_irqrestore(&dev->lock, flags);
	return -EPARMNOSUPPORT;
}

s32 mt_set_pwm_clk(u32 pwm_no, u32 clksrc, u32 div)
{
	unsigned long flags;
	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		pr_debug(T "pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	if (div >= CLK_DIV_MAX) {
		pr_debug(T "division excesses CLK_DIV_MAX\n");
		return -EPARMNOSUPPORT;
	}

	if ((clksrc & 0x7FFFFFFF) > CLK_BLOCK_BY_1625_OR_32K) {
		pr_debug(T "clksrc excesses CLK_BLOCK_BY_1625_OR_32K\n");
		return -EPARMNOSUPPORT;
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_pwm_clk_hal(pwm_no, clksrc, div);
	spin_unlock_irqrestore(&dev->lock, flags);

	return RSUCCESS;
}

s32 mt_get_pwm_clk(u32 pwm_no)
{
	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		pr_debug(T "pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	return mt_get_pwm_clk_hal(pwm_no);
}

/******************************************
 * Set PWM_CON register data source
 * pwm_no: pwm1~pwm7(0~6)
 * val: 0 is fifo mode
 *  1 is memory mode
 *******************************************/

static s32 mt_set_pwm_con_datasrc(u32 pwm_no, u32 val)
{
	unsigned long flags;
	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "pwm device doesn't exist\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		pr_debug(T "pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	spin_lock_irqsave(&dev->lock, flags);
	if (mt_set_pwm_con_datasrc_hal(pwm_no, val))
		goto err;
	spin_unlock_irqrestore(&dev->lock, flags);
	return RSUCCESS;

err:
	spin_unlock_irqrestore(&dev->lock, flags);
	return -EPARMNOSUPPORT;
}


/************************************************
 * set the PWM_CON register
 * pwm_no : pwm1~pwm7 (0~6)
 * val: 0 is period mode
 * 1 is random mode
 *
 ***************************************************/
static s32 mt_set_pwm_con_mode(u32 pwm_no, u32 val)
{
	unsigned long flags;

	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		pr_debug(T "pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	spin_lock_irqsave(&dev->lock, flags);
	if (mt_set_pwm_con_mode_hal(pwm_no, val))
		goto err;
	spin_unlock_irqrestore(&dev->lock, flags);
	return RSUCCESS;

err:
	spin_unlock_irqrestore(&dev->lock, flags);
	return -EPARMNOSUPPORT;
}

/***********************************************
 * Set PWM_CON register, idle value bit
 * val: 0 means that  idle state is not put out.
 *	   1 means that idle state is put out
 *
 *	  IDLE_FALSE: 0
 *	  IDLE_TRUE: 1
 ***********************************************/
static s32 mt_set_pwm_con_idleval(u32 pwm_no, u16 val)
{
	unsigned long flags;
	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		pr_debug(T "pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	spin_lock_irqsave(&dev->lock, flags);
	if (mt_set_pwm_con_idleval_hal(pwm_no, val))
		goto err;
	spin_unlock_irqrestore(&dev->lock, flags);
	return RSUCCESS;

err:
	spin_unlock_irqrestore(&dev->lock, flags);
	return -EPARMNOSUPPORT;
}

/*********************************************
 * Set PWM_CON register guardvalue bit
 *  val: 0 means guard state is not put out.
 *		1 mens guard state is put out.
 *
 *	GUARD_FALSE: 0
 *	GUARD_TRUE: 1
 **********************************************/
static s32 mt_set_pwm_con_guardval(u32 pwm_no, u16 val)
{
	unsigned long flags;
	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		pr_debug(T "pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	spin_lock_irqsave(&dev->lock, flags);
	if (mt_set_pwm_con_guardval_hal(pwm_no, val))
		goto err;
	spin_unlock_irqrestore(&dev->lock, flags);
	return RSUCCESS;

err:
	spin_unlock_irqrestore(&dev->lock, flags);
	return -EPARMNOSUPPORT;
}

/*************************************************
 * Set PWM_CON register stopbits
 * stop bits should be less then 0x3f
 *
 **************************************************/
static s32 mt_set_pwm_con_stpbit(u32 pwm_no, u32 stpbit, u32 srcsel)
{
	unsigned long flags;
	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		pr_debug(T "pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	if (srcsel == PWM_FIFO) {
		if (stpbit > 0x3f) {
			pr_debug(T "stpbit execess in fifo mode\n");
			return -EPARMNOSUPPORT;
		}
	} else if (srcsel == MEMORY) {
		if (stpbit > 0x1f) {
			pr_debug(T "stpbit excess in memory mode\n");
			return -EPARMNOSUPPORT;
		}
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_pwm_con_stpbit_hal(pwm_no, stpbit, srcsel);
	spin_unlock_irqrestore(&dev->lock, flags);

	return RSUCCESS;
}

/*****************************************************
 * Set PWM_CON register oldmode bit
 * val: 0 means disable oldmode
 *		1 means enable oldmode
 *
 *	  OLDMODE_DISABLE: 0
 *	  OLDMODE_ENABLE: 1
 ******************************************************/

static s32 mt_set_pwm_con_oldmode(u32 pwm_no, u32 val)
{
	unsigned long flags;

	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		pr_debug(T "pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	spin_lock_irqsave(&dev->lock, flags);
	if (mt_set_pwm_con_oldmode_hal(pwm_no, val))
		goto err;

	spin_unlock_irqrestore(&dev->lock, flags);
	return RSUCCESS;

err:
	spin_unlock_irqrestore(&dev->lock, flags);
	return -EPARMNOSUPPORT;
}

/***********************************************************
 * Set PWM_HIDURATION register
 *
 *************************************************************/

static s32 mt_set_pwm_HiDur(u32 pwm_no, u16 DurVal)
{
	unsigned long flags;

	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		pr_debug(T "pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_pwm_HiDur_hal(pwm_no, DurVal);
	spin_unlock_irqrestore(&dev->lock, flags);

	return RSUCCESS;
}

/************************************************
 * Set PWM Low Duration register
 *************************************************/
static s32 mt_set_pwm_LowDur(u32 pwm_no, u16 DurVal)
{
	unsigned long flags;

	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		pr_debug(T "pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_pwm_LowDur_hal(pwm_no, DurVal);
	spin_unlock_irqrestore(&dev->lock, flags);

	return RSUCCESS;
}

/***************************************************
 * Set PWM_GUARDDURATION register
 * pwm_no: PWM1~PWM7(0~6)
 * DurVal:   the value of guard duration
 ****************************************************/
static s32 mt_set_pwm_GuardDur(u32 pwm_no, u16 DurVal)
{
	unsigned long flags;

	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		pr_debug(T "pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_pwm_GuardDur_hal(pwm_no, DurVal);
	spin_unlock_irqrestore(&dev->lock, flags);

	return RSUCCESS;
}

/*****************************************************
 * Set pwm_buf0_addr register
 * pwm_no: pwm1~pwm7 (0~6)
 * addr: data address
 ******************************************************/
s32 mt_set_pwm_buf0_addr(u32 pwm_no, dma_addr_t addr)
{
	unsigned long flags;

	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		pr_debug(T "pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_pwm_buf0_addr_hal(pwm_no, addr);
	spin_unlock_irqrestore(&dev->lock, flags);
	return RSUCCESS;
}

/*****************************************************
 * Set pwm_buf0_size register
 * pwm_no: pwm1~pwm7 (0~6)
 * size: size of data
 ******************************************************/
s32 mt_set_pwm_buf0_size(u32 pwm_no, u16 size)
{
	unsigned long flags;

	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		pr_debug(T "pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_pwm_buf0_size_hal(pwm_no, size);
	spin_unlock_irqrestore(&dev->lock, flags);
	return RSUCCESS;
}


/*****************************************************
 * Set pwm_send_data0 register
 * pwm_no: pwm1~pwm7 (0~6)
 * data: the data in the register
 ******************************************************/
static s32 mt_set_pwm_send_data0(u32 pwm_no, u32 data)
{
	unsigned long flags;

	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		pr_debug(T "pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_pwm_send_data0_hal(pwm_no, data);
	spin_unlock_irqrestore(&dev->lock, flags);

	return RSUCCESS;
}

/*****************************************************
 * Set pwm_send_data1 register
 * pwm_no: pwm1~pwm7 (0~6)
 * data: the data in the register
 ******************************************************/
static s32 mt_set_pwm_send_data1(u32 pwm_no, u32 data)
{
	unsigned long flags;

	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		pr_debug(T "pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_pwm_send_data1_hal(pwm_no, data);
	spin_unlock_irqrestore(&dev->lock, flags);

	return RSUCCESS;
}

/*****************************************************
 * Set pwm_wave_num register
 * pwm_no: pwm1~pwm7 (0~6)
 * num:the wave number
 ******************************************************/
static s32 mt_set_pwm_wave_num(u32 pwm_no, u16 num)
{
	unsigned long flags;

	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		pr_debug(T "pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_pwm_wave_num_hal(pwm_no, num);
	spin_unlock_irqrestore(&dev->lock, flags);

	return RSUCCESS;
}

/*****************************************************
 * Set pwm_data_width register.
 * This is only for old mode
 * pwm_no: pwm1~pwm7 (0~6)
 * width: set the guard value in the old mode
 ******************************************************/
static s32 mt_set_pwm_data_width(u32 pwm_no, u16 width)
{
	unsigned long flags;

	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		pr_debug(T "pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_pwm_data_width_hal(pwm_no, width);
	spin_unlock_irqrestore(&dev->lock, flags);

	return RSUCCESS;
}

/*****************************************************
 * Set pwm_thresh register
 * pwm_no: pwm1~pwm7 (0~6)
 * thresh:  the thresh of the wave
 ******************************************************/
static s32 mt_set_pwm_thresh(u32 pwm_no, u16 thresh)
{
	unsigned long flags;

	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		pr_debug(T " pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_pwm_thresh_hal(pwm_no, thresh);
	spin_unlock_irqrestore(&dev->lock, flags);

	return RSUCCESS;
}

/*****************************************************
 * Set pwm_send_wavenum register
 * pwm_no: pwm1~pwm7 (0~6)
 *
 ******************************************************/
s32 mt_get_pwm_send_wavenum(u32 pwm_no)
{
	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return -EBADADDR;
	}

	if (pwm_no >= PWM_MAX) {
		pr_debug(T "pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	return mt_get_pwm_send_wavenum_hal(pwm_no);
}


/*******************************************
 * Set intr enable register
 * pwm_intr_enable_bit: the intr bit,
 *
 *********************************************/
s32 mt_set_intr_enable(u32 pwm_intr_enable_bit)
{
	unsigned long flags;

	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_intr_enable_bit >= PWM_INT_ENABLE_BITS_MAX) {
		pr_debug(T " pwm inter enable bit is not right.\n");
		return -EEXCESSBITS;
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_intr_enable_hal(pwm_intr_enable_bit);
	spin_unlock_irqrestore(&dev->lock, flags);

	return RSUCCESS;
}

/*****************************************************
 * Set intr status register
 * pwm_no: pwm1~pwm7 (0~6)
 * pwm_intr_status_bit
 ******************************************************/
s32 mt_get_intr_status(u32 pwm_intr_status_bit)
{
	unsigned long flags;
	int ret;

	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_intr_status_bit >= PWM_INT_STATUS_BITS_MAX) {
		pr_debug(T "status bit excesses PWM_INT_STATUS_BITS_MAX\n");
		return -EEXCESSBITS;
	}

	spin_lock_irqsave(&dev->lock, flags);
	ret = mt_get_intr_status_hal(pwm_intr_status_bit);
	spin_unlock_irqrestore(&dev->lock, flags);

	return ret;
}

/*****************************************************
 * Set intr ack register
 * pwm_no: pwm1~pwm7 (0~6)
 * pwm_intr_ack_bit
 ******************************************************/
s32 mt_set_intr_ack(u32 pwm_intr_ack_bit)
{
	unsigned long flags;
	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_intr_ack_bit >= PWM_INT_ACK_BITS_MAX) {
		pr_debug(T "ack bit excesses PWM_INT_ACK_BITS_MAX\n");
		return -EEXCESSBITS;
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_intr_ack_hal(pwm_intr_ack_bit);
	spin_unlock_irqrestore(&dev->lock, flags);

	return RSUCCESS;
}

void mt_set_pwm_3dlcm_enable(u8 enable)
{
	unsigned long flags;

	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return;
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_pwm_3dlcm_enable_hal(enable);
	spin_unlock_irqrestore(&dev->lock, flags);
}

void mt_set_pwm_3dlcm_inv(u32 pwm_no, u8 inv)
{
	unsigned long flags;

	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		pr_debug(T "dev is not valid\n");
		return;
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_pwm_3dlcm_inv_hal(pwm_no, inv);
	spin_unlock_irqrestore(&dev->lock, flags);
}

s32 pwm_set_easy_config(struct pwm_easy_config *conf)
{

	u32 duty = 0;
	u16 duration = 0;
	u32 data_AllH = 0xffffffff;
	u32 data0 = 0;
	u32 data1 = 0;

	if (conf->pwm_no >= PWM_MAX) {
		pr_debug(T "pwm number excess PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	if (conf->clk_div >= CLK_DIV_MAX) {
		pr_debug(T "PWM clock division invalid\n");
		return -EINVALID;
	}

	if (conf->clk_src >= PWM_CLK_SRC_INVALID) {
		pr_debug(T "PWM clock source invalid\n");
		return -EINVALID;
	}

	pr_debug(T "%s\n", __func__);

	if (conf->duty == 0) {
		mt_set_pwm_disable(conf->pwm_no);
		mt_pwm_power_off(conf->pwm_no, conf->pmic_pad);
		return RSUCCESS;
	}

	duty = conf->duty;
	duration = conf->duration;

	switch (conf->clk_src) {
	case PWM_CLK_OLD_MODE_BLOCK:
	case PWM_CLK_OLD_MODE_32K:
		if (duration > 8191 || duration < 0) {
			pr_debug(T "duration invalid parameter\n");
			return -EPARMNOSUPPORT;
		}
		if (duration < 10)
			duration = 10;
		break;

	case PWM_CLK_NEW_MODE_BLOCK:
	case PWM_CLK_NEW_MODE_BLOCK_DIV_BY_1625:
		if (duration > 65535 || duration < 0) {
			pr_debug(T "invalid parameters\n");
			return -EPARMNOSUPPORT;
		}
		break;
	default:
		pr_debug(T "invalid clock source\n");
		return -EPARMNOSUPPORT;
	}

	if (duty > 100)
		duty = 100;

	if (duty > 50) {
		data0 = data_AllH;
		data1 = data_AllH >> ((PWM_NEW_MODE_DUTY_TOTAL_BITS *
				(100 - duty))/100);
	} else {
		data0 = data_AllH >> ((PWM_NEW_MODE_DUTY_TOTAL_BITS *
				(50 - duty))/100);
		pr_debug(T "DATA0 :0x%x\n", data0);
		data1 = 0;
	}

	mt_pwm_power_on(conf->pwm_no, conf->pmic_pad);
	if (conf->pmic_pad)
		mt_pwm_sel_pmic(conf->pwm_no);
	else
		mt_pwm_sel_ap(conf->pwm_no);

	mt_set_pwm_con_guardval(conf->pwm_no, GUARD_TRUE);

	switch (conf->clk_src) {
	case PWM_CLK_OLD_MODE_32K:
		mt_set_pwm_con_oldmode(conf->pwm_no, OLDMODE_ENABLE);
		mt_set_pwm_clk(conf->pwm_no, CLK_BLOCK_BY_1625_OR_32K,
				conf->clk_div);
		break;

	case PWM_CLK_OLD_MODE_BLOCK:
		mt_set_pwm_con_oldmode(conf->pwm_no, OLDMODE_ENABLE);
		mt_set_pwm_clk(conf->pwm_no, CLK_BLOCK, conf->clk_div);
		break;

	case PWM_CLK_NEW_MODE_BLOCK:
		mt_set_pwm_con_oldmode(conf->pwm_no, OLDMODE_DISABLE);
		mt_set_pwm_clk(conf->pwm_no, CLK_BLOCK, conf->clk_div);
		mt_set_pwm_con_datasrc(conf->pwm_no, PWM_FIFO);
		mt_set_pwm_con_stpbit(conf->pwm_no, 0x3f, PWM_FIFO);
		break;

	case PWM_CLK_NEW_MODE_BLOCK_DIV_BY_1625:
		mt_set_pwm_con_oldmode(conf->pwm_no,  OLDMODE_DISABLE);
		mt_set_pwm_clk(conf->pwm_no, CLK_BLOCK_BY_1625_OR_32K,
				conf->clk_div);
		mt_set_pwm_con_datasrc(conf->pwm_no, PWM_FIFO);
		mt_set_pwm_con_stpbit(conf->pwm_no, 0x3f, PWM_FIFO);
		break;

	default:
		break;
	}
	pr_debug(T "The duration is:%x\n", duration);
	pr_debug(T "The data0 is:%x\n", data0);
	pr_debug(T "The data1 is:%x\n", data1);
	mt_set_pwm_HiDur(conf->pwm_no, duration);
	mt_set_pwm_LowDur(conf->pwm_no, duration);
	mt_set_pwm_GuardDur(conf->pwm_no, 0);
	mt_set_pwm_send_data0(conf->pwm_no, data0);
	mt_set_pwm_send_data1(conf->pwm_no, data1);
	mt_set_pwm_wave_num(conf->pwm_no, 0);
	mt_set_pwm_data_width(conf->pwm_no, duration);
	mt_set_pwm_thresh(conf->pwm_no, ((duration * conf->duty)/100));
	/* For memory barrier */
	mb();
	mt_set_pwm_enable(conf->pwm_no);
	pr_debug(T "mt_set_pwm_enable\n");

	return RSUCCESS;

}
EXPORT_SYMBOL(pwm_set_easy_config);

s32 pwm_set_spec_config(struct pwm_spec_config *conf)
{

	if (conf->pwm_no >= PWM_MAX) {
		pr_debug(T "pwm%d excess PWM_MAX(%d)\n", conf->pwm_no, PWM_MAX);
		return -EEXCESSPWMNO;
	}

	if (conf->mode >= PWM_MODE_INVALID) {
		pr_debug(T "PWM mode invalid\n");
		return -EINVALID;
	}

	if (conf->clk_src >= PWM_CLK_SRC_INVALID) {
		pr_debug(T "PWM clock source invalid\n");
		return -EINVALID;
	}

	if (conf->clk_div >= CLK_DIV_MAX) {
		pr_debug(T "PWM clock division invalid\n");
		return -EINVALID;
	}

	if ((conf->mode == PWM_MODE_OLD &&
		(conf->clk_src == PWM_CLK_NEW_MODE_BLOCK))
		|| (conf->mode != PWM_MODE_OLD &&
		(conf->clk_src == PWM_CLK_OLD_MODE_32K
		 || conf->clk_src == PWM_CLK_OLD_MODE_BLOCK))) {

		pr_debug(T "parameters match error\n");
		return -ERROR;
	}

	mt_pwm_power_on(conf->pwm_no, conf->pmic_pad);
	if (conf->pmic_pad)
		mt_pwm_sel_pmic(conf->pwm_no);

	switch (conf->mode) {
	case PWM_MODE_OLD:
		pr_debug(T "PWM_MODE_OLD\n");
		mt_set_pwm_con_oldmode(conf->pwm_no, OLDMODE_ENABLE);
		mt_set_pwm_con_idleval(conf->pwm_no,
			conf->PWM_MODE_OLD_REGS.IDLE_VALUE);
		mt_set_pwm_con_guardval(conf->pwm_no,
			conf->PWM_MODE_OLD_REGS.GUARD_VALUE);
		mt_set_pwm_GuardDur(conf->pwm_no,
			conf->PWM_MODE_OLD_REGS.GDURATION);
		mt_set_pwm_wave_num(conf->pwm_no,
			conf->PWM_MODE_OLD_REGS.WAVE_NUM);
		mt_set_pwm_data_width(conf->pwm_no,
			conf->PWM_MODE_OLD_REGS.DATA_WIDTH);
		mt_set_pwm_thresh(conf->pwm_no,
			conf->PWM_MODE_OLD_REGS.THRESH);
		pr_debug(T "PWM set old mode finish\n");
		break;
	case PWM_MODE_FIFO:
		pr_debug(T "PWM_MODE_FIFO\n");
		mt_set_pwm_con_oldmode(conf->pwm_no, OLDMODE_DISABLE);
		mt_set_pwm_con_datasrc(conf->pwm_no, PWM_FIFO);
		mt_set_pwm_con_mode(conf->pwm_no, PERIOD);
		mt_set_pwm_con_idleval(conf->pwm_no,
			conf->PWM_MODE_FIFO_REGS.IDLE_VALUE);
		mt_set_pwm_con_guardval(conf->pwm_no,
			conf->PWM_MODE_FIFO_REGS.GUARD_VALUE);
		mt_set_pwm_HiDur(conf->pwm_no,
			conf->PWM_MODE_FIFO_REGS.HDURATION);
		mt_set_pwm_LowDur(conf->pwm_no,
			conf->PWM_MODE_FIFO_REGS.LDURATION);
		mt_set_pwm_GuardDur(conf->pwm_no,
			conf->PWM_MODE_FIFO_REGS.GDURATION);
		mt_set_pwm_send_data0(conf->pwm_no,
			conf->PWM_MODE_FIFO_REGS.SEND_DATA0);
		mt_set_pwm_send_data1(conf->pwm_no,
			conf->PWM_MODE_FIFO_REGS.SEND_DATA1);
		mt_set_pwm_wave_num(conf->pwm_no,
			conf->PWM_MODE_FIFO_REGS.WAVE_NUM);
		mt_set_pwm_con_stpbit(conf->pwm_no,
			conf->PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE,
					  PWM_FIFO);
		break;
	case PWM_MODE_MEMORY:
		pr_debug(T "PWM_MODE_MEMORY\n");

		if (mt_get_pwm_version())
			mt_pwm_clk_sel_hal(conf->pwm_no, CLK_26M);
		else
			mt_pwm_26M_clk_enable_hal(1);

		mt_set_pwm_con_oldmode(conf->pwm_no, OLDMODE_DISABLE);
		mt_set_pwm_con_datasrc(conf->pwm_no, MEMORY);
		mt_set_pwm_con_mode(conf->pwm_no, PERIOD);
		mt_set_pwm_con_idleval(conf->pwm_no,
			conf->PWM_MODE_MEMORY_REGS.IDLE_VALUE);
		mt_set_pwm_con_guardval(conf->pwm_no,
			conf->PWM_MODE_MEMORY_REGS.GUARD_VALUE);
		mt_set_pwm_HiDur(conf->pwm_no,
			conf->PWM_MODE_MEMORY_REGS.HDURATION);
		mt_set_pwm_LowDur(conf->pwm_no,
			conf->PWM_MODE_MEMORY_REGS.LDURATION);
		mt_set_pwm_GuardDur(conf->pwm_no,
			conf->PWM_MODE_MEMORY_REGS.GDURATION);
		mt_set_pwm_buf0_addr(conf->pwm_no,
			conf->PWM_MODE_MEMORY_REGS.BUF0_BASE_ADDR);
		mt_set_pwm_buf0_size(conf->pwm_no,
			conf->PWM_MODE_MEMORY_REGS.BUF0_SIZE);
		mt_set_pwm_wave_num(conf->pwm_no,
			conf->PWM_MODE_MEMORY_REGS.WAVE_NUM);
		mt_set_pwm_con_stpbit(conf->pwm_no,
			conf->PWM_MODE_MEMORY_REGS.STOP_BITPOS_VALUE,
					  MEMORY);
		break;
	default:
		break;
		}

	switch (conf->clk_src) {
	case PWM_CLK_OLD_MODE_BLOCK:
		mt_set_pwm_clk(conf->pwm_no, CLK_BLOCK, conf->clk_div);
		pr_debug(T "Enable oldmode and set clock block\n");
		break;
	case PWM_CLK_OLD_MODE_32K:
		mt_set_pwm_clk(conf->pwm_no,
			0x80000000 | CLK_BLOCK_BY_1625_OR_32K, conf->clk_div);
		pr_debug(T "Enable oldmode and set clock 32K\n");
		break;
	case PWM_CLK_NEW_MODE_BLOCK:
		mt_set_pwm_clk(conf->pwm_no, CLK_BLOCK, conf->clk_div);
		pr_debug(T "Enable newmode and set clock block\n");
		break;
	case PWM_CLK_NEW_MODE_BLOCK_DIV_BY_1625:
		mt_set_pwm_clk(conf->pwm_no, CLK_BLOCK_BY_1625_OR_32K,
				conf->clk_div);
		pr_debug(T "Enable newmode and set clock 32K\n");
		break;
	default:
		break;
	}

	/* For memory barrier */
	mb();
	mt_set_pwm_enable(conf->pwm_no);
	pr_debug(T "mt_set_pwm_enable\n");

	return RSUCCESS;

}
EXPORT_SYMBOL(pwm_set_spec_config);

void mt_pwm_dump_regs(void)
{
	mt_pwm_dump_regs_hal();
	pr_debug(T "power_flag: 0x%x\n", (unsigned int)pwm_dev->power_flag);
}
EXPORT_SYMBOL(mt_pwm_dump_regs);

struct platform_device pwm_plat_dev = {
	.name = "mt-pwm",
};

#if PWM_LDVT_FLAG
static int pwm_gpio_config(struct device *dev, int pwm_no)
{
	int ret = 0;
	struct pinctrl *pwm_pinctrl;
	struct pinctrl_state *pwm_pins_default;
	struct pinctrl_state *pwm_pins_conf[3];

	pr_debug(T "set pwm[%d] gpio:%s\n", pwm_no, dev_name(dev));
	pwm_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pwm_pinctrl)) {
		ret = PTR_ERR(pwm_pinctrl);
		pr_debug(T "pinctrl get failed!!\n");
		return ret;
	}

	pwm_pins_default = pinctrl_lookup_state(pwm_pinctrl, "default");
	if (IS_ERR(pwm_pins_default)) {
		ret = PTR_ERR(pwm_pins_default);
		pr_debug("find pinctrl default fail\n");
	}

	if (pwm_no == 0) {
		pwm_pins_conf[0] =
			pinctrl_lookup_state(pwm_pinctrl, "pwm0_pins");
		if (IS_ERR(pwm_pins_conf[0])) {
			ret = PTR_ERR(pwm_pins_conf[0]);
			pr_debug(T "Cannot find pinctrl pwm0!\n");
			return ret;
		}
		pr_debug(T "PWM0 state find OK\n");
		pinctrl_select_state(pwm_pinctrl, pwm_pins_conf[0]);
	} else if (pwm_no == 1) {
		pwm_pins_conf[1] =
			pinctrl_lookup_state(pwm_pinctrl, "pwm1_pins");
		if (IS_ERR(pwm_pins_conf[1])) {
			ret = PTR_ERR(pwm_pins_conf[1]);
			pr_debug(T "Cannot find pinctrl pwm1!\n");
			return ret;
		}
		pr_debug(T "PWM1 state find OK\n");
		pinctrl_select_state(pwm_pinctrl, pwm_pins_conf[1]);
	} else if (pwm_no == 2) {
		pwm_pins_conf[2] =
			pinctrl_lookup_state(pwm_pinctrl, "pwm2_pins");
		if (IS_ERR(pwm_pins_conf[2])) {
			ret = PTR_ERR(pwm_pins_conf[2]);
			pr_debug(T "Cannot find pinctrl pwm2!\n");
			return ret;
		}
		pr_debug(T "PWM2 state find OK\n");
		pinctrl_select_state(pwm_pinctrl, pwm_pins_conf[2]);
	} else {
		pr_debug(T "Invalid PWM Number!\n");
		return 1;
	}

	return ret;
}
#endif


#if PWM_LDVT_FLAG/* PWM LDVT Hight Test Case */
	#define PWM_SCLK_SEL		1/* sub CLK 26M select */
	#define PWM_BCLK_SEL		0/* bus CLK(maybe 64M or 66M) select */
	#define LARGE_8G_DRAM_TEST 0/* default not en 8G */
#endif
static ssize_t pwm_debug_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
#if PWM_LDVT_FLAG
	int ret = 0;
	int cmd = 0;
	int sub_cmd = 0;
	int pwm_no = 0;

	pr_debug(T "power_flag: 0x%x\n", (unsigned int)pwm_dev->power_flag);

	ret = sscanf(buf, "%d,%d,%d",  &pwm_no, &cmd, &sub_cmd);
	if (!ret) {
		pr_debug("%s param get failed\n", __func__);
		return count;
	}

	pr_debug(T "pwm[%d]--cmd: %d.%d\n", pwm_no, cmd, sub_cmd);
/* set gpio mode */
	if (cmd) {
		if (pwm_no < PWM_NUM)
			ret = pwm_gpio_config(dev, pwm_no);
		else {
			pr_debug("pwm: No such pwm[%d]\n", pwm_no);
			return count;
		}
	}

	if (cmd == 0) {
		if (sub_cmd == 0) {/* dump RG */
			if (test_bit(pwm_no, &(pwm_dev->power_flag)))
				mt_pwm_dump_regs();
		} else if (sub_cmd == 1) {/* disable pwm */
			if (test_bit(pwm_no, &(pwm_dev->power_flag)))
				mt_pwm_disable(pwm_no, false);
			pr_debug(T "[PWM%d] poweroff\n", pwm_no);
		} else if (sub_cmd == 2) {/* get pwm clk */
			if (test_bit(pwm_no, &(pwm_dev->power_flag)))
				pr_debug(T "[PWM%d] get current clk:%d\n",
					pwm_no, mt_get_pwm_clk(pwm_no));
		} else
			pr_debug(T "[PWM%d] Invalid cmd:%d\n", pwm_no, sub_cmd);
	} else if (cmd == 1) {
		if (sub_cmd == 1) {/* 26M/66M/32K test */
			struct pwm_spec_config conf;

			pr_debug(T "[PWM%d] TEST:OLD CLK SOURCE 26M\n", pwm_no);
			conf.pwm_no = pwm_no;
			conf.mode = PWM_MODE_OLD;
			conf.clk_div = CLK_DIV1;
			conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
			conf.PWM_MODE_OLD_REGS.IDLE_VALUE = IDLE_FALSE;
			conf.PWM_MODE_OLD_REGS.GUARD_VALUE = GUARD_FALSE;
			conf.PWM_MODE_OLD_REGS.GDURATION = 0;
			conf.PWM_MODE_OLD_REGS.WAVE_NUM = 0;
			conf.PWM_MODE_OLD_REGS.DATA_WIDTH = 9;
			conf.PWM_MODE_OLD_REGS.THRESH = 4;
			if (!test_bit(pwm_no, &(pwm_dev->power_flag)))
				mt_pwm_power_on(pwm_no, 0);

			if (mt_get_pwm_version())
				mt_pwm_clk_sel_hal(pwm_no, CLK_26M);
			else
				mt_pwm_26M_clk_enable_hal(PWM_SCLK_SEL);

			ret = pwm_set_spec_config(&conf);
			if (ret != RSUCCESS)
				pr_debug(T "[PWM%d] TEST: CONFIG err! %d\n",
						pwm_no, ret);
		} else if (sub_cmd == 2) {
			struct pwm_spec_config conf;

			pr_debug(T "[PWM%d] TEST:OLD CLK SOURCE 66M\n", pwm_no);
			conf.pwm_no = pwm_no;
			conf.mode = PWM_MODE_OLD;
			conf.clk_div = CLK_DIV1;
			conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
			conf.PWM_MODE_OLD_REGS.IDLE_VALUE = IDLE_FALSE;
			conf.PWM_MODE_OLD_REGS.GUARD_VALUE = GUARD_FALSE;
			conf.PWM_MODE_OLD_REGS.GDURATION = 0;
			conf.PWM_MODE_OLD_REGS.WAVE_NUM = 0;
			conf.PWM_MODE_OLD_REGS.DATA_WIDTH = 9;
			conf.PWM_MODE_OLD_REGS.THRESH = 4;
			if (!test_bit(pwm_no, &(pwm_dev->power_flag)))
				mt_pwm_power_on(pwm_no, 0);
			if (mt_get_pwm_version())
				mt_pwm_clk_sel_hal(pwm_no, CLK_SEL_TOPCKGEN);
			else
				mt_pwm_26M_clk_enable_hal(PWM_BCLK_SEL);

			ret = pwm_set_spec_config(&conf);
			if (ret != RSUCCESS)
				pr_debug(T "[PWM%d] TEST: CONFIG err!%d\n",
						pwm_no, ret);
		} else if (sub_cmd == 3) {
			struct pwm_spec_config conf;

			pr_debug(T "[PWM%d] TEST:OLD CLK SOURCE 66M\n", pwm_no);
			conf.pwm_no = pwm_no;
			conf.mode = PWM_MODE_OLD;
			conf.clk_div = CLK_DIV1;
			conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
			conf.PWM_MODE_OLD_REGS.IDLE_VALUE = IDLE_FALSE;
			conf.PWM_MODE_OLD_REGS.GUARD_VALUE = GUARD_FALSE;
			conf.PWM_MODE_OLD_REGS.GDURATION = 0;
			conf.PWM_MODE_OLD_REGS.WAVE_NUM = 0;
			conf.PWM_MODE_OLD_REGS.DATA_WIDTH = 9;
			conf.PWM_MODE_OLD_REGS.THRESH = 4;
			if (!test_bit(pwm_no, &(pwm_dev->power_flag)))
				mt_pwm_power_on(pwm_no, 0);

			if (mt_get_pwm_version())
				mt_pwm_clk_sel_hal(pwm_no, CLK_32K);
			else
				mt_pwm_26M_clk_enable_hal(1);

			ret = pwm_set_spec_config(&conf);
			if (ret != RSUCCESS)
				pr_debug(T "[PWM%d] TEST: CONFIG err!%d\n",
						pwm_no, ret);
		} else {
			pr_debug(T "[PWM%d] TEST: Invalid sub_cmd:%d\n",
					pwm_no, sub_cmd);
		} /* end sub cmd */
	} else if (cmd == 2) {
		if (sub_cmd == 1) {
			struct pwm_spec_config conf;

			pr_debug(T "[PWM%d] TEST: FIFO STOP BIT 63\n", pwm_no);
			conf.pwm_no = pwm_no;
			conf.mode = PWM_MODE_FIFO;
			conf.clk_div = CLK_DIV1;
			conf.clk_src = PWM_CLK_NEW_MODE_BLOCK_DIV_BY_1625;
			conf.PWM_MODE_FIFO_REGS.IDLE_VALUE = IDLE_FALSE;
			conf.PWM_MODE_FIFO_REGS.GUARD_VALUE = GUARD_FALSE;
			conf.PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE = 63;
			conf.PWM_MODE_FIFO_REGS.HDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.LDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.GDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.SEND_DATA0 = 0x0000ff11;
			conf.PWM_MODE_FIFO_REGS.SEND_DATA1 = 0xffffffff;
			conf.PWM_MODE_FIFO_REGS.WAVE_NUM = 0;
			if (!test_bit(pwm_no, &(pwm_dev->power_flag)))
				mt_pwm_power_on(pwm_no, 0);

			if (mt_get_pwm_version())
				mt_pwm_clk_sel_hal(pwm_no, CLK_26M);
			else
				mt_pwm_26M_clk_enable_hal(PWM_SCLK_SEL);

			ret = pwm_set_spec_config(&conf);
			if (ret != RSUCCESS)
				pr_debug(T "[PWM%d] TEST: CONFIG err!%d\n",
						pwm_no, ret);
		} else if (sub_cmd == 2) {
			struct pwm_spec_config conf;

			pr_debug(T "[PWM%d] TEST: FIFO STOP BIT 62\n", pwm_no);
			conf.pwm_no = pwm_no;
			conf.mode = PWM_MODE_FIFO;
			conf.clk_div = CLK_DIV1;
			conf.clk_src = PWM_CLK_NEW_MODE_BLOCK_DIV_BY_1625;
			conf.PWM_MODE_FIFO_REGS.IDLE_VALUE = IDLE_FALSE;
			conf.PWM_MODE_FIFO_REGS.GUARD_VALUE = GUARD_FALSE;
			conf.PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE = 62;
			conf.PWM_MODE_FIFO_REGS.HDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.LDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.GDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.SEND_DATA0 = 0x0000ff11;
			conf.PWM_MODE_FIFO_REGS.SEND_DATA1 = 0xffffffff;
			conf.PWM_MODE_FIFO_REGS.WAVE_NUM = 0;
			if (!test_bit(pwm_no, &(pwm_dev->power_flag)))
				mt_pwm_power_on(pwm_no, 0);

			if (mt_get_pwm_version())
				mt_pwm_clk_sel_hal(pwm_no, CLK_26M);
			else
				mt_pwm_26M_clk_enable_hal(PWM_SCLK_SEL);

			ret = pwm_set_spec_config(&conf);
			if (ret != RSUCCESS)
				pr_debug(T "[PWM%d] TEST: CONFIG err!%d\n",
						pwm_no, ret);
		} else if (sub_cmd == 3) {
			struct pwm_spec_config conf;

			pr_debug(T "[PWM%d] TEST: FIFO STOP BIT 31\n", pwm_no);
			conf.pwm_no = pwm_no;
			conf.mode = PWM_MODE_FIFO;
			conf.clk_div = CLK_DIV1;
			conf.clk_src = PWM_CLK_NEW_MODE_BLOCK_DIV_BY_1625;
			conf.PWM_MODE_FIFO_REGS.IDLE_VALUE = IDLE_FALSE;
			conf.PWM_MODE_FIFO_REGS.GUARD_VALUE = GUARD_FALSE;
			conf.PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE = 31;
			conf.PWM_MODE_FIFO_REGS.HDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.LDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.GDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.SEND_DATA0 = 0x0000ff11;
			conf.PWM_MODE_FIFO_REGS.SEND_DATA1 = 0xffffffff;
			conf.PWM_MODE_FIFO_REGS.WAVE_NUM = 0;
			if (!test_bit(pwm_no, &(pwm_dev->power_flag)))
				mt_pwm_power_on(pwm_no, 0);

			if (mt_get_pwm_version())
				mt_pwm_clk_sel_hal(pwm_no, CLK_26M);
			else
				mt_pwm_26M_clk_enable_hal(PWM_SCLK_SEL);

			ret = pwm_set_spec_config(&conf);
			if (ret != RSUCCESS)
				pr_debug(T "[PWM%d] TEST: CONFIG err!%d\n",
						pwm_no, ret);
		} else
			pr_debug(T "[PWM%d] TEST: Invalid sub_cmd:%d\n",
					pwm_no, sub_cmd);
	} else if (cmd == 3) {
		if (sub_cmd == 1) {
			struct pwm_spec_config conf;

			pr_debug(T "[PWM%d] OLD CLK 32KHZ/WAVE\n", pwm_no);
			conf.pwm_no = pwm_no;
			conf.mode = PWM_MODE_OLD;
			conf.clk_div = CLK_DIV1;
			conf.clk_src = PWM_CLK_OLD_MODE_32K; /* 32KHz */
			conf.PWM_MODE_OLD_REGS.IDLE_VALUE = IDLE_FALSE;
			conf.PWM_MODE_OLD_REGS.GUARD_VALUE = GUARD_FALSE;
			conf.PWM_MODE_OLD_REGS.GDURATION = 0;
			conf.PWM_MODE_OLD_REGS.WAVE_NUM = 0;
			conf.PWM_MODE_OLD_REGS.DATA_WIDTH = 9;/* duty:50% */
			conf.PWM_MODE_OLD_REGS.THRESH = 4;
			if (!test_bit(pwm_no, &(pwm_dev->power_flag)))
				mt_pwm_power_on(pwm_no, 0);

			if (mt_get_pwm_version())
				mt_pwm_clk_sel_hal(pwm_no, CLK_26M);
			else
				mt_pwm_26M_clk_enable_hal(PWM_SCLK_SEL);

			ret = pwm_set_spec_config(&conf);
			if (ret != RSUCCESS)
				pr_debug(T "[PWM%d] TEST: CONFIG err!%d\n",
						pwm_no, ret);
		} else if (sub_cmd == 2) {
			struct pwm_spec_config conf;

			pr_debug(T "[PWM%d] TEST:OLD CLK 32KHZ/WAVE\n", pwm_no);
			conf.pwm_no = pwm_no;
			conf.mode = PWM_MODE_OLD;
			conf.clk_div = CLK_DIV1;
			conf.clk_src = PWM_CLK_OLD_MODE_32K;  /* 32KHz */
			conf.PWM_MODE_OLD_REGS.IDLE_VALUE = IDLE_FALSE;
			conf.PWM_MODE_OLD_REGS.GUARD_VALUE = GUARD_FALSE;
			conf.PWM_MODE_OLD_REGS.GDURATION = 0;
			conf.PWM_MODE_OLD_REGS.WAVE_NUM = 1;
			conf.PWM_MODE_OLD_REGS.DATA_WIDTH = 9;
			conf.PWM_MODE_OLD_REGS.THRESH = 4;/* duty:50% */
			if (!test_bit(pwm_no, &(pwm_dev->power_flag)))
				mt_pwm_power_on(pwm_no, 0);

			if (mt_get_pwm_version())
				mt_pwm_clk_sel_hal(pwm_no, CLK_26M);
			else
				mt_pwm_26M_clk_enable_hal(PWM_SCLK_SEL);

			ret = pwm_set_spec_config(&conf);
			if (ret != RSUCCESS)
				pr_debug(T "[PWM%d] TEST: CONFIG err!ret:%d\n",
					pwm_no, ret);
		} else if (sub_cmd == 3) {
			struct pwm_spec_config conf;

			pr_debug(T "[PWM%d] OLD CLK 32KHZ/WAVE\n", pwm_no);
			conf.pwm_no = pwm_no;
			conf.mode = PWM_MODE_OLD;
			conf.clk_div = CLK_DIV1;
			conf.clk_src = PWM_CLK_OLD_MODE_32K; /* 32KHz */
			conf.PWM_MODE_OLD_REGS.IDLE_VALUE = IDLE_FALSE;
			conf.PWM_MODE_OLD_REGS.GUARD_VALUE = GUARD_FALSE;
			conf.PWM_MODE_OLD_REGS.GDURATION = 0;
			conf.PWM_MODE_OLD_REGS.WAVE_NUM = 10;
			conf.PWM_MODE_OLD_REGS.DATA_WIDTH = 9;
			conf.PWM_MODE_OLD_REGS.THRESH = 4;/* duty:50% */
			if (!test_bit(pwm_no, &(pwm_dev->power_flag)))
				mt_pwm_power_on(pwm_no, 0);

			if (mt_get_pwm_version())
				mt_pwm_clk_sel_hal(pwm_no, CLK_26M);
			else
				mt_pwm_26M_clk_enable_hal(PWM_SCLK_SEL);

			ret = pwm_set_spec_config(&conf);
			if (ret != RSUCCESS)
				pr_debug(T "[PWM%d]CONFIG err%d\n",
					pwm_no, ret);
		} else if (sub_cmd == 4) {
			struct pwm_spec_config conf;

			pr_debug(T "[PWM%d]OLD CLK 16KHZ/WAVE\n", pwm_no);
			conf.pwm_no = pwm_no;
			conf.mode = PWM_MODE_OLD;
			conf.clk_div = CLK_DIV2;
			/* 16K */
			conf.clk_src = PWM_CLK_NEW_MODE_BLOCK_DIV_BY_1625;
			conf.PWM_MODE_OLD_REGS.IDLE_VALUE = IDLE_FALSE;
			conf.PWM_MODE_OLD_REGS.GUARD_VALUE = GUARD_FALSE;
			conf.PWM_MODE_OLD_REGS.GDURATION = 0;
			conf.PWM_MODE_OLD_REGS.WAVE_NUM = 0;
			conf.PWM_MODE_OLD_REGS.DATA_WIDTH = 9;
			conf.PWM_MODE_OLD_REGS.THRESH = 4;
			if (!test_bit(pwm_no, &(pwm_dev->power_flag)))
				mt_pwm_power_on(pwm_no, 0);

			if (mt_get_pwm_version())
				mt_pwm_clk_sel_hal(pwm_no, CLK_26M);
			else
				mt_pwm_26M_clk_enable_hal(PWM_SCLK_SEL);

			ret = pwm_set_spec_config(&conf);
			if (ret != RSUCCESS)
				pr_debug(T "[PWM%d] TEST: CONFIG err!ret:%d\n",
					pwm_no, ret);
		} else
			pr_debug(T "[PWM%d] TEST: Invalid sub_cmd:%d ===>\n",
				pwm_no, sub_cmd);
	} else if (cmd == 4) {
		if (sub_cmd == 1) {
			struct pwm_spec_config conf;

			pr_debug(T "[PWM%d] TEST: OLD CLK  26MHZ\n", pwm_no);
			conf.pwm_no = pwm_no;
			conf.mode = PWM_MODE_OLD;
			conf.clk_div = CLK_DIV1;
			conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
			conf.PWM_MODE_OLD_REGS.IDLE_VALUE = IDLE_FALSE;
			conf.PWM_MODE_OLD_REGS.GUARD_VALUE = GUARD_FALSE;
			conf.PWM_MODE_OLD_REGS.GDURATION = 0;
			conf.PWM_MODE_OLD_REGS.WAVE_NUM = 0;
			conf.PWM_MODE_OLD_REGS.DATA_WIDTH = 9;
			conf.PWM_MODE_OLD_REGS.THRESH = 4;
			if (!test_bit(pwm_no, &(pwm_dev->power_flag)))
				mt_pwm_power_on(pwm_no, 0);

			if (mt_get_pwm_version())
				mt_pwm_clk_sel_hal(pwm_no, CLK_26M);
			else
				mt_pwm_26M_clk_enable_hal(PWM_SCLK_SEL);

			ret = pwm_set_spec_config(&conf);
			if (ret != RSUCCESS)
				pr_debug(T "[PWM%d] TEST: CONFIG err!ret:%d\n",
					pwm_no, ret);
		} else if (sub_cmd == 2) {
			struct pwm_spec_config conf;

			pr_debug(T "[PWM%d] TEST: OLD CLK DIV  26MHz/1625\n",
				pwm_no);
			conf.pwm_no = pwm_no;
			conf.mode = PWM_MODE_OLD;
			conf.clk_div = CLK_DIV1;
			conf.clk_src = PWM_CLK_NEW_MODE_BLOCK_DIV_BY_1625;
			conf.PWM_MODE_OLD_REGS.IDLE_VALUE = IDLE_FALSE;
			conf.PWM_MODE_OLD_REGS.GUARD_VALUE = GUARD_FALSE;
			conf.PWM_MODE_OLD_REGS.GDURATION = 0;
			conf.PWM_MODE_OLD_REGS.WAVE_NUM = 0;
			conf.PWM_MODE_OLD_REGS.DATA_WIDTH = 9;
			conf.PWM_MODE_OLD_REGS.THRESH = 4;
			if (!test_bit(pwm_no, &(pwm_dev->power_flag)))
				mt_pwm_power_on(pwm_no, 0);

			if (mt_get_pwm_version())
				mt_pwm_clk_sel_hal(pwm_no, CLK_26M);
			else
				mt_pwm_26M_clk_enable_hal(PWM_SCLK_SEL);

			ret = pwm_set_spec_config(&conf);
			if (ret != RSUCCESS)
				pr_debug(T "[PWM%d] TEST: CONFIG err:%d\n",
					pwm_no, ret);
		} else if (sub_cmd == 3) {
			struct pwm_spec_config conf;

			pr_debug(T "[PWM%d]OLD CLK DIV 26MHz/16/13:125KHZ\n",
				pwm_no);
			conf.pwm_no = pwm_no;
			conf.mode = PWM_MODE_OLD;
			conf.clk_div = CLK_DIV16;
			conf.clk_src = PWM_CLK_OLD_MODE_BLOCK;
			conf.PWM_MODE_OLD_REGS.IDLE_VALUE = IDLE_FALSE;
			conf.PWM_MODE_OLD_REGS.GUARD_VALUE = GUARD_FALSE;
			conf.PWM_MODE_OLD_REGS.GDURATION = 0;
			conf.PWM_MODE_OLD_REGS.WAVE_NUM = 0;
			conf.PWM_MODE_OLD_REGS.DATA_WIDTH = 12;
			conf.PWM_MODE_OLD_REGS.THRESH = 6;
			if (!test_bit(pwm_no, &(pwm_dev->power_flag)))
				mt_pwm_power_on(pwm_no, 0);

			if (mt_get_pwm_version())
				mt_pwm_clk_sel_hal(pwm_no, CLK_26M);
			else
				mt_pwm_26M_clk_enable_hal(PWM_SCLK_SEL);

			ret = pwm_set_spec_config(&conf);
			if (ret != RSUCCESS)
				pr_debug(T "[PWM%d] TEST: CONFIG err:%d\n",
					pwm_no, ret);
		} else {
			pr_debug(T "[PWM%d] TEST: Invalid sub_cmd:%d\n",
				pwm_no, sub_cmd);
		}
	} else if (cmd == 5) {
		if (sub_cmd == 1) {
			struct pwm_spec_config conf;

			pr_debug(T "[PWM%d]OLD Finish interrupt\n", pwm_no);
			conf.pwm_no = pwm_no;
			conf.mode = PWM_MODE_OLD;
			conf.clk_div = CLK_DIV1;
			/* 32K */
			conf.clk_src = PWM_CLK_NEW_MODE_BLOCK_DIV_BY_1625;
			conf.PWM_MODE_OLD_REGS.IDLE_VALUE = IDLE_FALSE;
			conf.PWM_MODE_OLD_REGS.GUARD_VALUE = GUARD_FALSE;
			conf.PWM_MODE_OLD_REGS.GDURATION = 0;
			conf.PWM_MODE_OLD_REGS.WAVE_NUM = 10;
			conf.PWM_MODE_OLD_REGS.DATA_WIDTH = 9;
			conf.PWM_MODE_OLD_REGS.THRESH = 4;

			intr_pwm_nu[pwm_no]++;
			if (!test_bit(pwm_no, &(pwm_dev->power_flag)))
				mt_pwm_power_on(pwm_no, 0);
			mt_set_intr_enable(PWM1_INT_FINISH_EN+2*pwm_no);

			if (mt_get_pwm_version())
				mt_pwm_clk_sel_hal(pwm_no, CLK_SEL_TOPCKGEN);
			else
				mt_pwm_26M_clk_enable_hal(PWM_SCLK_SEL);

			ret = pwm_set_spec_config(&conf);
			if (ret != RSUCCESS)
				pr_debug(T "[PWM%d] TEST: CONFIG err:%d\n",
					pwm_no, ret);
		} else if (sub_cmd == 2) {
			struct pwm_spec_config conf;

			pr_debug(T "[PWM%d] FIFO Finish interrupt sig-pwm\n",
				pwm_no);
			conf.pwm_no = pwm_no;
			conf.mode = PWM_MODE_FIFO;
			conf.clk_div = CLK_DIV1;
			conf.clk_src = PWM_CLK_NEW_MODE_BLOCK_DIV_BY_1625;
			conf.PWM_MODE_FIFO_REGS.IDLE_VALUE = IDLE_FALSE;
			conf.PWM_MODE_FIFO_REGS.GUARD_VALUE = GUARD_FALSE;
			conf.PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE = 63;
			conf.PWM_MODE_FIFO_REGS.HDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.LDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.GDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.SEND_DATA0 = 0x0000ff11;
			conf.PWM_MODE_FIFO_REGS.SEND_DATA1 = 0xffffffff;
			conf.PWM_MODE_FIFO_REGS.WAVE_NUM = 10;

			intr_pwm_nu[pwm_no]++;
			if (!test_bit(pwm_no, &(pwm_dev->power_flag)))
				mt_pwm_power_on(pwm_no, 0);

			if (mt_get_pwm_version())
				mt_pwm_clk_sel_hal(pwm_no, CLK_SEL_TOPCKGEN);
			else
				mt_pwm_26M_clk_enable_hal(PWM_SCLK_SEL);

			mt_set_intr_enable(PWM1_INT_FINISH_EN+2*pwm_no);
			ret = pwm_set_spec_config(&conf);
			if (ret != RSUCCESS)
				pr_debug(T "[PWM%d] TEST: CONFIG err:%d\n",
					pwm_no, ret);
		} else if (sub_cmd == 3) {
			int t_nu = 0;
			struct pwm_spec_config conf[PWM_MAX];

			pr_debug(T "[PWM%d] FIFO Finish interrupt multi-pwm\n",
				pwm_no);
			for (t_nu = 0; t_nu <= pwm_no; t_nu++) {
				conf[t_nu].pwm_no = pwm_no;
				conf[t_nu].mode = PWM_MODE_FIFO;
				conf[t_nu].clk_div = CLK_DIV2;
				conf[t_nu].clk_src = PWM_CLK_NEW_MODE_BLOCK;
				conf[t_nu].PWM_MODE_FIFO_REGS.IDLE_VALUE =
						IDLE_FALSE;
				conf[t_nu].PWM_MODE_FIFO_REGS.GUARD_VALUE =
						GUARD_FALSE;
			conf[t_nu].PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE = 31;
				conf[t_nu].PWM_MODE_FIFO_REGS.HDURATION = 0;
				conf[t_nu].PWM_MODE_FIFO_REGS.LDURATION = 0;
				conf[t_nu].PWM_MODE_FIFO_REGS.GDURATION = 0;
				conf[t_nu].PWM_MODE_FIFO_REGS.SEND_DATA0 =
						0x0000ff11;
				conf[t_nu].PWM_MODE_FIFO_REGS.SEND_DATA1 =
						0xff00ffff;
				conf[t_nu].PWM_MODE_FIFO_REGS.WAVE_NUM = 0;

				intr_pwm_nu[t_nu]++;
			}

			for (t_nu = 0; t_nu <= pwm_no; t_nu++) {
				if (!test_bit(t_nu, &(pwm_dev->power_flag)))
					mt_pwm_power_on(t_nu, 0);
				mt_set_intr_enable(PWM1_INT_FINISH_EN+2*t_nu);

				if (mt_get_pwm_version())
					mt_pwm_clk_sel_hal(pwm_no, CLK_SEL_TOPCKGEN);
				else
					mt_pwm_26M_clk_enable_hal(PWM_SCLK_SEL);

				ret = pwm_set_spec_config(&conf[t_nu]);
				if (ret != RSUCCESS)
					pr_debug(T "[PWM%d]CONFIG err:%d\n",
						t_nu, ret);
			}
		} else {
			pr_debug(T "[PWM%d] TEST: Invalid sub_cmd:%d ===>\n",
					pwm_no, sub_cmd);
		} /* end sub cmd */
	} else if (cmd == 8) {
		pr_debug(T "[PWM%d] TEST: 3DLCM test: not implement===>\n",
				pwm_no);
	} else if (cmd == 9) {
		int i = 0;
		struct pwm_spec_config conf;
		#define PWM_MEM_DMA_SIZE  256
	#if LARGE_8G_DRAM_TEST
		#define PWM_DMA_TYPE unsigned long long
		/* dma_addr_t  phys; */
		PWM_DMA_TYPE  phys;
		PWM_DMA_TYPE *virt = NULL;
		PWM_DMA_TYPE *membuff = NULL;
	#else/* 4G address */
		#define PWM_DMA_TYPE  unsigned int
		/* dma_addr_t  phys; */
		PWM_DMA_TYPE phys;
		PWM_DMA_TYPE *virt = NULL;
		PWM_DMA_TYPE *membuff = NULL;
	#endif

		pr_debug(T "[PWM%d] TEST: MEMO/DMA ===>\n", pwm_no);
		conf.mode = PWM_MODE_MEMORY;
		conf.pwm_no = pwm_no;
		conf.clk_div = CLK_DIV8;
		conf.clk_src = PWM_CLK_NEW_MODE_BLOCK;
		conf.PWM_MODE_MEMORY_REGS.IDLE_VALUE = IDLE_FALSE;
		conf.PWM_MODE_MEMORY_REGS.GUARD_VALUE = GUARD_FALSE;
		conf.PWM_MODE_MEMORY_REGS.HDURATION = 119;
		conf.PWM_MODE_MEMORY_REGS.LDURATION = 119;
		conf.PWM_MODE_MEMORY_REGS.GDURATION = 0;
		conf.PWM_MODE_MEMORY_REGS.WAVE_NUM = 0;
		conf.PWM_MODE_MEMORY_REGS.STOP_BITPOS_VALUE = 30;

#if LARGE_8G_DRAM_TEST
		if (dma_set_coherent_mask(dev, DMA_BIT_MASK(36))) {
			pr_debug(T "[PWM] dma alloc fail, dma_mask:0x%llx",
					DMA_BIT_MASK(36));
			return count;
		}
		pr_debug(T "[PWM]set dma_mask:0x%llx ", DMA_BIT_MASK(36));

#endif
		virt = dma_alloc_coherent(dev,
			PWM_MEM_DMA_SIZE, (dma_addr_t *)&phys, GFP_KERNEL);
		if (virt == NULL)
			return count;

	#if LARGE_8G_DRAM_TEST
		pr_debug(T "[PWM] DMA get virt_addr:0x%p, phys_addr:0x%llx\n",
					virt, phys);
	#else
		pr_debug(T "[PWM] DMA get virt_addr:0x%p, phys_addr:0x%x\n",
					virt, phys);
	#endif

		membuff = virt;
		for (i = 0; i < (PWM_MEM_DMA_SIZE/(sizeof(PWM_DMA_TYPE)));
					i += (sizeof(PWM_DMA_TYPE))) {
			membuff[i] = 0xaaaaaaaa;
			membuff[i+1] = 0xffff0000;
		}
		conf.PWM_MODE_MEMORY_REGS.BUF0_SIZE = PWM_MEM_DMA_SIZE;
		conf.PWM_MODE_MEMORY_REGS.BUF0_BASE_ADDR = phys;

		if (!test_bit(pwm_no, &(pwm_dev->power_flag)))
			mt_pwm_power_on(pwm_no, 0);
		ret = pwm_set_spec_config(&conf);
		if (ret != RSUCCESS)
			pr_debug(T "[PWM%d] TEST:CONFIG err:%d\n", pwm_no, ret);
	} else {
		pr_debug(T "[PWM%d] TEST: Invalid cmd:%d\n", pwm_no, cmd);
	}
#endif/* end LDVT flag */
	return count;
}

static ssize_t pwm_debug_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	pwm_debug_show_hal();
	return sprintf(buf, "\n");
}

static DEVICE_ATTR_RW(pwm_debug);


static irqreturn_t mt_pwm_irq(int irq, void *intr_pwm_nu)
{
	u32 i = 0;
	u32 sts = 0;

#if PWM_LDVT_FLAG
	u8 *pwm_flag = (u8 *)intr_pwm_nu;

	for (i = 0; i < PWM_MAX; i++) {
		if (!pwm_flag[i])
			continue;

		sts = mt_get_intr_status(PWM1_INT_FINISH_EN + (2 * i));
		if (sts) {
			mt_set_intr_ack(PWM1_INT_FINISH_ACK + (2 * i));
			pwm_flag[i]--;
		}
		pr_debug(T "PWM%d Finished intr come:0x%x\n", i, sts);
		sts = mt_get_intr_status(PWM1_INT_UNDERFLOW_EN + (2 * i));
		if (sts) {
			/* clear interrupt */
			mt_set_intr_ack(PWM1_INT_UNDERFLOW_ACK + (2 * i));
			pwm_flag[i]--;
		}
		pr_debug(T "PWM%d underflow intr come:0x%x\n", i, pwm_flag[i]);
	}
#else
	for (i = 0; i < PWM_MAX*2; i++) {
		sts = mt_get_intr_status(i);
		if (sts) {
			mt_set_intr_ack(i);
			pr_info(T "PWM int!!ch=%x\n", i/2);
			break;
		}
	}
#endif
	return IRQ_HANDLED;
}
static int mt_pwm_probe(struct platform_device *pdev)
{
	int ret, pwm_irqnr;

	pwm_base = of_iomap(pdev->dev.of_node, 0);
	if (!pwm_base) {
		pr_err(T "PWM iomap failed\n");
		return -ENODEV;
	}

	mt_pwm_platform_init(pdev);

	ret = mt_get_pwm_clk_src(pdev);
	if (ret != 0)
		pr_err(T "[%s]: clk get Fail :%d\n", __func__, ret);

	platform_set_drvdata(pdev, pwm_dev);

	ret = device_create_file(&pdev->dev, &dev_attr_pwm_debug);
	if (ret)
		pr_debug(T "[pwm]error creating sysfs files: pwm_debug\n");

	pwm_irqnr = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (!pwm_irqnr) {
		pr_err(T "PWM get irqnr failed\n");
		return -ENODEV;
	}
	pr_info(T "pwm base: 0x%p pwm irq: %d\n",
			pwm_base, pwm_irqnr);

	pwm_irqnr = platform_get_irq(pdev, 0);
	if (pwm_irqnr <= 0) {
		pr_err("[pwm]get irq failed!\n");
		return -EINVAL;
	}
#if PWM_LDVT_FLAG
	ret = devm_request_irq(&pdev->dev, pwm_irqnr, mt_pwm_irq,
		IRQF_TRIGGER_NONE, PWM_DEVICE, (void *) intr_pwm_nu);
#else
	ret = devm_request_irq(&pdev->dev, pwm_irqnr, mt_pwm_irq,
		IRQF_TRIGGER_NONE, PWM_DEVICE, NULL);
#endif
	if (ret < 0) {
		pr_err(T "[PWM]Request IRQ %d failed-------\n", pwm_irqnr);
		return ret;
	}

	mutex_init(&pwm_power_lock);

	pr_info(T "pwm probe Done!!\n");

	return RSUCCESS;
}

static int  mt_pwm_remove(struct platform_device *pdev)
{
	if (!pdev) {
		pr_debug(T "The plaform device is not exist\n");
		return -EBADADDR;
	}
	device_remove_file(&pdev->dev, &dev_attr_pwm_debug);

	pr_debug(T "%s\n", __func__);
	return RSUCCESS;
}

static void mt_pwm_shutdown(struct platform_device *pdev)
{
	pr_debug(T "%s\n", __func__);
}

static const struct of_device_id pwm_of_match[] = {
	{.compatible = "mediatek,pwm",},
	{},
};

struct platform_driver pwm_plat_driver = {
	.probe = mt_pwm_probe,
	.remove = mt_pwm_remove,
	.shutdown = mt_pwm_shutdown,
	.driver = {
		.name = "mt-pwm",
		.of_match_table = pwm_of_match,
	},
};

static int __init mt_pwm_init(void)
{
	int ret;

	ret = platform_driver_register(&pwm_plat_driver);
	if (ret < 0) {
		pr_err(T "platform_driver_register error\n");
		goto out;
	}

out:
	mt_pwm_init_power_flag(&(pwm_dev->power_flag));
	return ret;
}

static void __exit mt_pwm_exit(void)
{
	platform_driver_unregister(&pwm_plat_driver);
}

module_init(mt_pwm_init);
module_exit(mt_pwm_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chun-Hung.wu");
MODULE_DESCRIPTION(" This module is used for chip of mediatek");

