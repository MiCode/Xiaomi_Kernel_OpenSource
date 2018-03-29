/*******************************************************************************
* mt_pwm.c PWM Drvier
*
* Copyright (c) 2012, Media Teck.inc
*
* This program is free software; you can redistribute it and/or modify it
* under the terms and conditions of the GNU General Public Licence,
* version 2, as publish by the Free Software Foundation.
*
* This program is distributed and in hope it will be useful, but WITHOUT
* ANY WARRNTY; without even the implied warranty of MERCHANTABITLITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
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
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#include <mt-plat/mt_pwm.h>
#include <mt-plat/mt_pwm_hal_pub.h>
#include <mach/mt_pwm_hal.h>

#if !defined(CONFIG_MTK_CLKMGR)
#include <linux/clk.h>
#endif


#ifdef CONFIG_OF
void __iomem *pwm_base;
#endif

struct pwm_device {
	const char	  *name;
	atomic_t		ref;
	dev_t		   devno;
	spinlock_t	  lock;
	unsigned long	power_flag;/* bitwise, bit(8):map to MT_CG_PERI0_PWM */
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
	mt_pwm_power_on_hal(pwm_no, pmic_pad, &(pwm_dev->power_flag));
}

static void mt_pwm_power_off(u32 pwm_no, bool pmic_pad)
{
	mt_pwm_power_off_hal(pwm_no, pmic_pad, &(pwm_dev->power_flag));
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
		PWMDBG("PWM1~PWM4 not support pmic_pad\n");

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
		PWMDBG("PWM1~PWM4 not support pmic_pad\n");

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
		PWMDBG("dev is not valid!\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		PWMDBG("pwm number is not between PWM1~PWM7\n");
		return -EEXCESSPWMNO;
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_pwm_enable_hal(pwm_no);
	spin_unlock_irqrestore(&dev->lock, flags);

	return RSUCCESS;
}


/*******************************************************/
static s32 mt_set_pwm_disable(u32 pwm_no)
{
	unsigned long flags;
	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		PWMDBG("dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		PWMDBG("pwm number is not between PWM1~PWM7\n");
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

void mt_set_pwm_enable_seqmode(void)
{
	unsigned long flags;
	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		PWMDBG("dev is not valid\n");
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
		PWMDBG("dev is not valid\n");
		return;
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_pwm_disable_seqmode_hal();
	spin_unlock_irqrestore(&dev->lock, flags);
}

s32 mt_set_pwm_test_sel(u32 val)  /* val as 0 or 1 */
{
	unsigned long flags;
	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		PWMDBG("dev is not pwm_dev\n");
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
		PWMDBG("dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		PWMDBG("pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	if (div >= CLK_DIV_MAX) {
		PWMDBG("division excesses CLK_DIV_MAX\n");
		return -EPARMNOSUPPORT;
	}

	if ((clksrc & 0x7FFFFFFF) > CLK_BLOCK_BY_1625_OR_32K) {
		PWMDBG("clksrc excesses CLK_BLOCK_BY_1625_OR_32K\n");
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
		PWMDBG("dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		PWMDBG("pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	return mt_get_pwm_clk_hal(pwm_no);
}

/******************************************
* Set PWM_CON register data source
* pwm_no: pwm1~pwm7(0~6)
*val: 0 is fifo mode
*  1 is memory mode
*******************************************/

static s32 mt_set_pwm_con_datasrc(u32 pwm_no, u32 val)
{
	unsigned long flags;
	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		PWMDBG("pwm device doesn't exist\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		PWMDBG("pwm number excesses PWM_MAX\n");
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
*  set the PWM_CON register
* pwm_no : pwm1~pwm7 (0~6)
* val: 0 is period mode
*	   1 is random mode
*
***************************************************/
static s32 mt_set_pwm_con_mode(u32 pwm_no, u32 val)
{
	unsigned long flags;

	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		PWMDBG("dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		PWMDBG("pwm number excesses PWM_MAX\n");
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
*Set PWM_CON register, idle value bit
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
		PWMDBG("dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		PWMDBG("pwm number excesses PWM_MAX\n");
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
		PWMDBG("dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		PWMDBG("pwm number excesses PWM_MAX\n");
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
*stop bits should be less then 0x3f
*
**************************************************/
static s32 mt_set_pwm_con_stpbit(u32 pwm_no, u32 stpbit, u32 srcsel)
{
	unsigned long flags;

	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		PWMDBG("dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		PWMDBG("pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}


	if (srcsel == PWM_FIFO) {
		if (stpbit > 0x3f) {
			PWMDBG("stpbit execesses the most of 0x3f in fifo mode\n");
			return -EPARMNOSUPPORT;
		}
	} else if (srcsel == MEMORY) {
		if (stpbit > 0x1f) {
			PWMDBG("stpbit excesses the most of 0x1f in memory mode\n");
			return -EPARMNOSUPPORT;
		}
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_pwm_con_stpbit_hal(pwm_no, stpbit, srcsel);
	spin_unlock_irqrestore(&dev->lock, flags);

	return RSUCCESS;
}

/*****************************************************
*Set PWM_CON register oldmode bit
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
		PWMDBG("dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		PWMDBG("pwm number excesses PWM_MAX\n");
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
		PWMDBG("dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		PWMDBG("pwm number excesses PWM_MAX\n");
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
		PWMDBG("dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		PWMDBG("pwm number excesses PWM_MAX\n");
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
		PWMDBG("dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		PWMDBG("pwm number excesses PWM_MAX\n");
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
		PWMDBG("dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		PWMDBG("pwm number excesses PWM_MAX\n");
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
		PWMDBG("dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		PWMDBG("pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_pwm_buf0_size_hal(pwm_no, size);
	spin_unlock_irqrestore(&dev->lock, flags);
	return RSUCCESS;
}

/*****************************************************
* Set pwm_buf1_addr register
* pwm_no: pwm1~pwm7 (0~6)
* addr: data address
*****************************************************
s32 mt_set_pwm_buf1_addr (u32 pwm_no, u32 addr )
{
	unsigned long flags;
	u32 reg_buff1_addr;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ( "dev is not valid\n" );
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ( "pwm number excesses PWM_MAX\n" );
		return -EEXCESSPWMNO;
	}

	reg_buff1_addr = PWM_register[pwm_no] + 4 * PWM_BUF1_BASE_ADDR;

	spin_lock_irqsave ( &dev->lock, flags );
	OUTREG32 ( reg_buff1_addr, addr );
	spin_unlock_irqrestore ( &dev->lock, flags );

	return RSUCCESS;
}*/

/*****************************************************
* Set pwm_buf1_size register
* pwm_no: pwm1~pwm7 (0~6)
* size: size of data
*****************************************************
s32 mt_set_pwm_buf1_size ( u32 pwm_no, u16 size)
{
	unsigned long flags;
	u32 reg_buff1_size;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ( "dev is not valid\n" );
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ( "pwm number excesses PWM_MAX\n" );
		return -EEXCESSPWMNO;
	}

	reg_buff1_size = PWM_register[pwm_no] + 4* PWM_BUF1_SIZE;

	spin_lock_irqsave ( &dev->lock, flags );
	OUTREG32 ( reg_buff1_size, size );
	spin_unlock_irqrestore ( &dev->lock, flags );

	return RSUCCESS;

}*/

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
		PWMDBG("dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		PWMDBG("pwm number excesses PWM_MAX\n");
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
		PWMDBG("dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		PWMDBG("pwm number excesses PWM_MAX\n");
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
		PWMDBG("dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		PWMDBG("pwm number excesses PWM_MAX\n");
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
		PWMDBG("dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		PWMDBG("pwm number excesses PWM_MAX\n");
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
		PWMDBG("dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		PWMDBG(" pwm number excesses PWM_MAX\n");
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
		PWMDBG("dev is not valid\n");
		return -EBADADDR;
	}

	if (pwm_no >= PWM_MAX) {
		PWMDBG("pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	return mt_get_pwm_send_wavenum_hal(pwm_no);
}

/*****************************************************
* Set pwm_send_data1 register
* pwm_no: pwm1~pwm7 (0~6)
* buf_valid_bit:
* for buf0: bit0 and bit1 should be set 1.
* for buf1: bit2 and bit3 should be set 1.
*****************************************************
s32 mt_set_pwm_valid ( u32 pwm_no, u32 buf_valid_bit )   //set 0  for BUF0 bit or set 1 for BUF1 bit
{
	unsigned long flags;
	u32 reg_valid;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ("dev is not valid\n");
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ( "pwm number excesses PWM_MAX\n" );
		return -EEXCESSPWMNO;
	}

	if ( !buf_valid_bit>= BUF_EN_MAX) {
		PWMDBG ( "inavlid bit\n" );
		return -EPARMNOSUPPORT;
	}

	if ( (pwm_no <= PWM2)||(pwm_no == PWM6))
		reg_valid = PWM_register[pwm_no] + 4 * PWM_VALID;
	else
		reg_valid = PWM_register[pwm_no] + 4* (PWM_VALID -2);

	spin_lock_irqsave ( &dev->lock, flags );
	SETREG32 ( reg_valid, 1 << buf_valid_bit );
	spin_unlock_irqrestore ( &dev->lock, flags );

	return RSUCCESS;
}*/

/*************************************************
*  set PWM4_delay when using SEQ mode
*
*************************************************
s32 mt_set_pwm_delay_duration(u32 pwm_delay_reg, u16 val)
{
	unsigned long flags;
	struct pwm_device *pwmdev = pwm_dev;

	if (!pwmdev) {
		PWMDBG( "device doesn't exist\n" );
		return -EBADADDR;
	}

	spin_lock_irqsave ( &pwmdev->lock, flags );
	MASKREG32 ( pwm_delay_reg, PWM_DELAY_DURATION_MASK, val );
	spin_unlock_irqrestore ( &pwmdev->lock, flags );

	return RSUCCESS;
}*/

/*******************************************************
* Set pwm delay clock
*
*
*******************************************************
s32 mt_set_pwm_delay_clock (u32 pwm_delay_reg, u32 clksrc)
{
	unsigned long flags;
	struct pwm_device *pwmdev = pwm_dev;
	if ( ! pwmdev ) {
		PWMDBG ( "device doesn't exist\n" );
		return -EBADADDR;
	}

	spin_lock_irqsave ( &pwmdev->lock, flags );
	MASKREG32 (pwm_delay_reg, PWM_DELAY_CLK_MASK, clksrc );
	spin_unlock_irqrestore (&pwmdev->lock, flags);

	return RSUCCESS;
}*/

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
		PWMDBG("dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_intr_enable_bit >= PWM_INT_ENABLE_BITS_MAX) {
		PWMDBG(" pwm inter enable bit is not right.\n");
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
		PWMDBG("dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_intr_status_bit >= PWM_INT_STATUS_BITS_MAX) {
		PWMDBG("status bit excesses PWM_INT_STATUS_BITS_MAX\n");
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
		PWMDBG("dev is not valid\n");
		return -EINVALID;
	}

	if (pwm_intr_ack_bit >= PWM_INT_ACK_BITS_MAX) {
		PWMDBG("ack bit excesses PWM_INT_ACK_BITS_MAX\n");
		return -EEXCESSBITS;
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_intr_ack_hal(pwm_intr_ack_bit);
	spin_unlock_irqrestore(&dev->lock, flags);

	return RSUCCESS;
}

/*----------3dLCM support-----------*/
/*
 base pwm2, select pwm3&4&5 same as pwm2 or inversion of pwm2
 */
void mt_set_pwm_3dlcm_enable(u8 enable)
{
	unsigned long flags;

	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		PWMDBG("dev is not valid\n");
		return;
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_pwm_3dlcm_enable_hal(enable);
	spin_unlock_irqrestore(&dev->lock, flags);
}

/*
 set "pwm_no" inversion of pwm base or not
 */
void mt_set_pwm_3dlcm_inv(u32 pwm_no, u8 inv)
{
	unsigned long flags;

	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		PWMDBG("dev is not valid\n");
		return;
	}

	spin_lock_irqsave(&dev->lock, flags);
	mt_set_pwm_3dlcm_inv_hal(pwm_no, inv);
	spin_unlock_irqrestore(&dev->lock, flags);
}
/*
void mt_set_pwm_3dlcm_base(u32 pwm_no)
{
	unsigned long flags;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ( "dev is not valid\n" );
		return;
	}

	spin_lock_irqsave ( &dev->lock, flags );
	mt_set_pwm_3dlcm_base_hal(pwm_no);
	spin_unlock_irqrestore ( &dev->lock, flags );
	return;
}
*/
/*
void mt_pwm_26M_clk_enable(u32 enable)
{
	unsigned long flags;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ( "dev is not valid\n" );
		return;
	}

	spin_lock_irqsave ( &dev->lock, flags );
	mt_pwm_26M_clk_enable_hal(enable);
	spin_unlock_irqrestore ( &dev->lock, flags );
	return;
}
*/
s32 pwm_set_easy_config(struct pwm_easy_config *conf)
{

	u32 duty = 0;
	u16 duration = 0;
	u32 data_AllH = 0xffffffff;
	u32 data0 = 0;
	u32 data1 = 0;

	if (conf->pwm_no >= PWM_MAX || conf->pwm_no < PWM_MIN) {
		PWMDBG("pwm number excess PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	if ((conf->clk_div >= CLK_DIV_MAX) || (conf->clk_div < CLK_DIV_MIN)) {
		PWMDBG("PWM clock division invalid\n");
		return -EINVALID;
	}

	if ((conf->clk_src >= PWM_CLK_SRC_INVALID) || (conf->clk_src < PWM_CLK_SRC_MIN)) {
		PWMDBG("PWM clock source invalid\n");
		return -EINVALID;
	}

	if  (conf->duty < 0) {
		PWMDBG("duty parameter is invalid\n");
		return -EINVALID;
	}

	PWMDBG("pwm_set_easy_config\n");

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
			PWMDBG("duration invalid parameter\n");
			return -EPARMNOSUPPORT;
		}
		if (duration < 10)
			duration = 10;
		break;

	case PWM_CLK_NEW_MODE_BLOCK:
	case PWM_CLK_NEW_MODE_BLOCK_DIV_BY_1625:
		if (duration > 65535 || duration < 0) {
			PWMDBG("invalid parameters\n");
			return -EPARMNOSUPPORT;
		}
		break;
	default:
		PWMDBG("invalid clock source\n");
		return -EPARMNOSUPPORT;
	}

	if (duty > 100)
		duty = 100;

	if (duty > 50) {
		data0 = data_AllH;
		data1 = data_AllH >> ((PWM_NEW_MODE_DUTY_TOTAL_BITS * (100 - duty))/100);
	} else {
		data0 = data_AllH >> ((PWM_NEW_MODE_DUTY_TOTAL_BITS * (50 - duty))/100);
		PWMDBG("DATA0 :0x%x\n", data0);
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
		mt_set_pwm_clk(conf->pwm_no, CLK_BLOCK_BY_1625_OR_32K, conf->clk_div);
		break;

	case PWM_CLK_OLD_MODE_BLOCK:
		mt_set_pwm_con_oldmode(conf->pwm_no, OLDMODE_ENABLE);
		mt_set_pwm_clk(conf->pwm_no, CLK_BLOCK, conf->clk_div);
		break;

	case PWM_CLK_NEW_MODE_BLOCK:
		mt_set_pwm_con_oldmode(conf->pwm_no, OLDMODE_DISABLE);
		mt_set_pwm_clk(conf->pwm_no, CLK_BLOCK , conf->clk_div);
		mt_set_pwm_con_datasrc(conf->pwm_no, PWM_FIFO);
		mt_set_pwm_con_stpbit(conf->pwm_no, 0x3f, PWM_FIFO);
		break;

	case PWM_CLK_NEW_MODE_BLOCK_DIV_BY_1625:
		mt_set_pwm_con_oldmode(conf->pwm_no,  OLDMODE_DISABLE);
		mt_set_pwm_clk(conf->pwm_no, CLK_BLOCK_BY_1625_OR_32K, conf->clk_div);
		mt_set_pwm_con_datasrc(conf->pwm_no, PWM_FIFO);
		mt_set_pwm_con_stpbit(conf->pwm_no, 0x3f, PWM_FIFO);
		break;

	default:
		break;
	}
	PWMDBG("The duration is:%x\n", duration);
	PWMDBG("The data0 is:%x\n", data0);
	PWMDBG("The data1 is:%x\n", data1);
	mt_set_pwm_HiDur(conf->pwm_no, duration);
	mt_set_pwm_LowDur(conf->pwm_no, duration);
	mt_set_pwm_GuardDur(conf->pwm_no, 0);
/* mt_set_pwm_buf0_addr (conf->pwm_no, 0 ); */
/* mt_set_pwm_buf0_size( conf->pwm_no, 0 ); */
/* mt_set_pwm_buf1_addr (conf->pwm_no, 0 ); */
/* mt_set_pwm_buf1_size (conf->pwm_no, 0 ); */
	mt_set_pwm_send_data0(conf->pwm_no, data0);
	mt_set_pwm_send_data1(conf->pwm_no, data1);
	mt_set_pwm_wave_num(conf->pwm_no, 0);

/* if ( conf->pwm_no <= PWM2 || conf->pwm_no == PWM6) */
/* { */
	mt_set_pwm_data_width(conf->pwm_no, duration);
	mt_set_pwm_thresh(conf->pwm_no, ((duration * conf->duty)/100));
/* mt_set_pwm_valid (conf->pwm_no, BUF0_EN_VALID ); */
/* mt_set_pwm_valid ( conf->pwm_no, BUF1_EN_VALID ); */

/* } */

	mb();/* For memory barrier */
	mt_set_pwm_enable(conf->pwm_no);
	PWMDBG("mt_set_pwm_enable\n");

	return RSUCCESS;

}
EXPORT_SYMBOL(pwm_set_easy_config);

s32 pwm_set_spec_config(struct pwm_spec_config *conf)
{

	if (conf->pwm_no >= PWM_MAX) {
		PWMDBG("pwm number excess PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	   if ((conf->mode >= PWM_MODE_INVALID) || (conf->mode < PWM_MODE_MIN)) {
		PWMDBG("PWM mode invalid\n");
		return -EINVALID;
	   }

	if ((conf->clk_src >= PWM_CLK_SRC_INVALID) || (conf->clk_src < PWM_CLK_SRC_MIN)) {
		PWMDBG("PWM clock source invalid\n");
		return -EINVALID;
	}

	if ((conf->clk_div >= CLK_DIV_MAX) || (conf->clk_div < CLK_DIV_MIN)) {
		PWMDBG("PWM clock division invalid\n");
		return -EINVALID;
	}

	if ((conf->mode == PWM_MODE_OLD && (conf->clk_src == PWM_CLK_NEW_MODE_BLOCK))
		|| (conf->mode != PWM_MODE_OLD &&
		(conf->clk_src == PWM_CLK_OLD_MODE_32K
		 || conf->clk_src == PWM_CLK_OLD_MODE_BLOCK))) {

		PWMDBG("parameters match error\n");
		return -ERROR;
	}

	mt_pwm_power_on(conf->pwm_no, conf->pmic_pad);
	if (conf->pmic_pad)
		mt_pwm_sel_pmic(conf->pwm_no);

	switch (conf->mode) {
	case PWM_MODE_OLD:
		PWMDBG("PWM_MODE_OLD\n");
		mt_set_pwm_con_oldmode(conf->pwm_no, OLDMODE_ENABLE);
		mt_set_pwm_con_idleval(conf->pwm_no, conf->PWM_MODE_OLD_REGS.IDLE_VALUE);
		mt_set_pwm_con_guardval(conf->pwm_no, conf->PWM_MODE_OLD_REGS.GUARD_VALUE);
		mt_set_pwm_GuardDur(conf->pwm_no, conf->PWM_MODE_OLD_REGS.GDURATION);
		mt_set_pwm_wave_num(conf->pwm_no, conf->PWM_MODE_OLD_REGS.WAVE_NUM);
		mt_set_pwm_data_width(conf->pwm_no, conf->PWM_MODE_OLD_REGS.DATA_WIDTH);
		mt_set_pwm_thresh(conf->pwm_no, conf->PWM_MODE_OLD_REGS.THRESH);
		PWMDBG("PWM set old mode finish\n");
		break;
	case PWM_MODE_FIFO:
		PWMDBG("PWM_MODE_FIFO\n");
		mt_set_pwm_con_oldmode(conf->pwm_no, OLDMODE_DISABLE);
		mt_set_pwm_con_datasrc(conf->pwm_no, PWM_FIFO);
		mt_set_pwm_con_mode(conf->pwm_no, PERIOD);
		mt_set_pwm_con_idleval(conf->pwm_no, conf->PWM_MODE_FIFO_REGS.IDLE_VALUE);
		mt_set_pwm_con_guardval(conf->pwm_no, conf->PWM_MODE_FIFO_REGS.GUARD_VALUE);
		mt_set_pwm_HiDur(conf->pwm_no, conf->PWM_MODE_FIFO_REGS.HDURATION);
		mt_set_pwm_LowDur(conf->pwm_no, conf->PWM_MODE_FIFO_REGS.LDURATION);
		mt_set_pwm_GuardDur(conf->pwm_no, conf->PWM_MODE_FIFO_REGS.GDURATION);
		mt_set_pwm_send_data0(conf->pwm_no, conf->PWM_MODE_FIFO_REGS.SEND_DATA0);
		mt_set_pwm_send_data1(conf->pwm_no, conf->PWM_MODE_FIFO_REGS.SEND_DATA1);
		mt_set_pwm_wave_num(conf->pwm_no, conf->PWM_MODE_FIFO_REGS.WAVE_NUM);
		mt_set_pwm_con_stpbit(conf->pwm_no, conf->PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE,
					  PWM_FIFO);
		break;
	case PWM_MODE_MEMORY:
		PWMDBG("PWM_MODE_MEMORY\n");
		mt_set_pwm_con_oldmode(conf->pwm_no, OLDMODE_DISABLE);
		mt_set_pwm_con_datasrc(conf->pwm_no, MEMORY);
		mt_set_pwm_con_mode(conf->pwm_no, PERIOD);
		mt_set_pwm_con_idleval(conf->pwm_no, conf->PWM_MODE_MEMORY_REGS.IDLE_VALUE);
		mt_set_pwm_con_guardval(conf->pwm_no, conf->PWM_MODE_MEMORY_REGS.GUARD_VALUE);
		mt_set_pwm_HiDur(conf->pwm_no, conf->PWM_MODE_MEMORY_REGS.HDURATION);
		mt_set_pwm_LowDur(conf->pwm_no, conf->PWM_MODE_MEMORY_REGS.LDURATION);
		mt_set_pwm_GuardDur(conf->pwm_no, conf->PWM_MODE_MEMORY_REGS.GDURATION);
		mt_set_pwm_buf0_addr(conf->pwm_no, conf->PWM_MODE_MEMORY_REGS.BUF0_BASE_ADDR);
		mt_set_pwm_buf0_size(conf->pwm_no, conf->PWM_MODE_MEMORY_REGS.BUF0_SIZE);
		mt_set_pwm_wave_num(conf->pwm_no, conf->PWM_MODE_MEMORY_REGS.WAVE_NUM);
		mt_set_pwm_con_stpbit(conf->pwm_no, conf->PWM_MODE_MEMORY_REGS.STOP_BITPOS_VALUE,
					  MEMORY);
		break;
/*
	case PWM_MODE_RANDOM:
		PWMDBG("PWM_MODE_RANDOM\n");
		mt_set_pwm_disable(conf->pwm_no);
		mt_set_pwm_con_oldmode(conf->pwm_no, OLDMODE_DISABLE);
		mt_set_pwm_con_datasrc(conf->pwm_no, MEMORY);
		mt_set_pwm_con_mode (conf->pwm_no, RAND);
		mt_set_pwm_con_idleval(conf->pwm_no, conf->PWM_MODE_RANDOM_REGS.IDLE_VALUE);
		mt_set_pwm_con_guardval (conf->pwm_no, conf->PWM_MODE_RANDOM_REGS.GUARD_VALUE);
		mt_set_pwm_HiDur (conf->pwm_no, conf->PWM_MODE_RANDOM_REGS.HDURATION);
		mt_set_pwm_LowDur (conf->pwm_no, conf->PWM_MODE_RANDOM_REGS.LDURATION);
		mt_set_pwm_GuardDur (conf->pwm_no, conf->PWM_MODE_RANDOM_REGS.GDURATION);
		mt_set_pwm_buf0_addr(conf->pwm_no, (u32 )conf->PWM_MODE_RANDOM_REGS.BUF0_BASE_ADDR);
		mt_set_pwm_buf0_size (conf->pwm_no, conf->PWM_MODE_RANDOM_REGS.BUF0_SIZE);
		mt_set_pwm_buf1_addr(conf->pwm_no, (u32 )conf->PWM_MODE_RANDOM_REGS.BUF1_BASE_ADDR);
		mt_set_pwm_buf1_size (conf->pwm_no, conf->PWM_MODE_RANDOM_REGS.BUF1_SIZE);
		mt_set_pwm_wave_num(conf->pwm_no, conf->PWM_MODE_RANDOM_REGS.WAVE_NUM);
		mt_set_pwm_con_stpbit(conf->pwm_no, conf->PWM_MODE_RANDOM_REGS.STOP_BITPOS_VALUE, MEMORY);
		mt_set_pwm_valid(conf->pwm_no, BUF0_EN_VALID);
		mt_set_pwm_valid(conf->pwm_no, BUF1_EN_VALID);
		break;

	case PWM_MODE_DELAY:
		PWMDBG("PWM_MODE_DELAY\n");
		mt_set_pwm_con_oldmode(conf->pwm_no, OLDMODE_DISABLE);
		mt_set_pwm_enable_seqmode();
		mt_set_pwm_disable(PWM2);
		mt_set_pwm_disable(PWM3);
		mt_set_pwm_disable(PWM4);
		mt_set_pwm_disable(PWM5);
		if ( conf->PWM_MODE_DELAY_REGS.PWM3_DELAY_DUR <0 ||
				conf->PWM_MODE_DELAY_REGS.PWM3_DELAY_DUR >= (1<<17) ||
				conf->PWM_MODE_DELAY_REGS.PWM4_DELAY_DUR < 0||
				conf->PWM_MODE_DELAY_REGS.PWM4_DELAY_DUR >= (1<<17) ||
				conf->PWM_MODE_DELAY_REGS.PWM5_DELAY_DUR <0 ||
				conf->PWM_MODE_DELAY_REGS.PWM5_DELAY_DUR >=(1<<17) ) {
			PWMDBG("Delay value invalid\n");
			return -EINVALID;
		}
		mt_set_pwm_delay_duration(PWM3_DELAY, conf->PWM_MODE_DELAY_REGS.PWM3_DELAY_DUR );
		mt_set_pwm_delay_clock(PWM3_DELAY, conf->PWM_MODE_DELAY_REGS.PWM3_DELAY_CLK);
		mt_set_pwm_delay_duration(PWM4_DELAY, conf->PWM_MODE_DELAY_REGS.PWM4_DELAY_DUR);
		mt_set_pwm_delay_clock(PWM4_DELAY, conf->PWM_MODE_DELAY_REGS.PWM4_DELAY_CLK);
		mt_set_pwm_delay_duration(PWM5_DELAY, conf->PWM_MODE_DELAY_REGS.PWM5_DELAY_DUR);
		mt_set_pwm_delay_clock(PWM5_DELAY, conf->PWM_MODE_DELAY_REGS.PWM5_DELAY_CLK);

		mt_set_pwm_enable(PWM2);
		mt_set_pwm_enable(PWM3);
		mt_set_pwm_enable(PWM4);
		mt_set_pwm_enable(PWM5);
			break;
*/
	default:
		break;
		}

	switch (conf->clk_src) {
	case PWM_CLK_OLD_MODE_BLOCK:
		mt_set_pwm_clk(conf->pwm_no, CLK_BLOCK, conf->clk_div);
		PWMDBG("Enable oldmode and set clock block\n");
		break;
	case PWM_CLK_OLD_MODE_32K:
		mt_set_pwm_clk(conf->pwm_no, 0x80000000|CLK_BLOCK_BY_1625_OR_32K, conf->clk_div);
		PWMDBG("Enable oldmode and set clock 32K\n");
		break;
	case PWM_CLK_NEW_MODE_BLOCK:
		mt_set_pwm_clk(conf->pwm_no, CLK_BLOCK, conf->clk_div);
		PWMDBG("Enable newmode and set clock block\n");
		break;
	case PWM_CLK_NEW_MODE_BLOCK_DIV_BY_1625:
		mt_set_pwm_clk(conf->pwm_no, CLK_BLOCK_BY_1625_OR_32K, conf->clk_div);
		PWMDBG("Enable newmode and set clock 32K\n");
		break;
	default:
		break;
	}

	mb();/* For memory barrier */
	mt_set_pwm_enable(conf->pwm_no);
	PWMDBG("mt_set_pwm_enable\n");

	return RSUCCESS;

}
EXPORT_SYMBOL(pwm_set_spec_config);

void mt_pwm_dump_regs(void)
{
	mt_pwm_dump_regs_hal();
	PWMDBG("pwm power_flag: 0x%x\n", (unsigned int)pwm_dev->power_flag);
}
EXPORT_SYMBOL(mt_pwm_dump_regs);

struct platform_device pwm_plat_dev = {
	.name = "mt-pwm",
};

static ssize_t pwm_debug_store(struct device *dev, struct device_attribute *attr, const char *buf,
				   size_t count)
{
	PWMDBG("pwm power_flag: 0x%x\n", (unsigned int)pwm_dev->power_flag);

	pwm_debug_store_hal();
/* PWM LDVT Hight Test Case */
#if 0
	PWMDBG("Enter into pwm_debug_store\n");
	int cmd, sub_cmd, pwm_no, n;

	n = sscanf(buf, "%d %d %d", &cmd, &sub_cmd, &pwm_no);
	if (!n)
		pr_err("pwm_debug_store nothing\n");
	/* set gpio mode */
	/* pwm0 */
	if (pwm_no == 0) {
		mt_set_gpio_mode(GPIO44, GPIO_MODE_06);
		mt_set_gpio_mode(GPIO78, GPIO_MODE_05);
		mt_set_gpio_mode(GPIO201, GPIO_MODE_03);
	} else if (pwm_no == 1) {
		mt_set_gpio_mode(GPIO10, GPIO_MODE_01);
		mt_set_gpio_mode(GPIO69, GPIO_MODE_02);
	} else if (pwm_no == 2) {
		mt_set_gpio_mode(GPIO1, GPIO_MODE_01);
		mt_set_gpio_mode(GPIO21, GPIO_MODE_02);
		mt_set_gpio_mode(GPIO55, GPIO_MODE_02);
	} else if (pwm_no == 3) {
		mt_set_gpio_mode(GPIO0, GPIO_MODE_05);
		mt_set_gpio_mode(GPIO59, GPIO_MODE_05);
		mt_set_gpio_mode(GPIO79, GPIO_MODE_05);
	} else if (pwm_no == 4) {
		mt_set_gpio_mode(GPIO60, GPIO_MODE_05);
		mt_set_gpio_mode(GPIO80, GPIO_MODE_05);
	} else {
		PWMDBG("Invalid PWM Number!\n");
	}

	if (cmd == 0) {
		PWMDBG("********** HELP **********\n");
		PWMDBG(" \t1 -> clk source select: 26M or Others source clock\n");
		PWMDBG("\t\t1.1 -> sub cmd 1 : 26M\n");
		PWMDBG("\t\t1.1 -> sub cmd 2 : 66M or Others\n");
		PWMDBG(" \t2 -> FIFO stop bit test: PWM0~PWM4 63, 62, 31\n");
		PWMDBG("\t\t2.1 -> sub cmd 1 : stop bit is 63\n");
		PWMDBG("\t\t2.2 -> sub cmd 2 : stop bit is 62\n");
		PWMDBG("\t\t2.3 -> sub cmd 3 : stop bit is 31\n");
		PWMDBG(" \t3 -> FIFO wavenum test: PWM0~PWM4 num=1/num=0\n");
		PWMDBG(" \t\t3.1 -> sub cmd 1 : PWM0~PWM4 num=0\n");
		PWMDBG(" \t\t3.2 -> sub cmd 2 : PWM0~PWM4 num=1\n");
		PWMDBG(" \t4 -> 32K select or use internal frequency individal\n");
		PWMDBG("\t\t4.1 -> sub cmd 1 : 32KHz selected\n");
		PWMDBG("\t\t4.2 -> sub cmd 2 : 26MHz 1625 setting selected\n");
		PWMDBG("\t\t4.3 -> sub cmd 3 : 26MHz selected\n");
		PWMDBG(" \t5 -> 3D LCM\n");
		PWMDBG(" \t6 -> Test all gpio, This coverd by above test case\n");
		PWMDBG(" \t7 -> MEMO Mode test\n");
	} else if (cmd == 1) {
		if (sub_cmd == 1) {
			struct pwm_spec_config conf;

			pr_debug("=============clk source select test : 26M===============\n");
			conf.pwm_no = pwm_no;
			conf.mode = PWM_MODE_FIFO;
			conf.clk_div = CLK_DIV6;
			conf.clk_src = PWM_CLK_NEW_MODE_BLOCK;
			conf.PWM_MODE_FIFO_REGS.IDLE_VALUE = IDLE_FALSE;
			conf.PWM_MODE_FIFO_REGS.GUARD_VALUE = GUARD_FALSE;
			conf.PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE = 63;
			conf.PWM_MODE_FIFO_REGS.HDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.LDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.GDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.SEND_DATA0 = 0xffffffff;
			conf.PWM_MODE_FIFO_REGS.SEND_DATA1 = 0x00000000;
			conf.PWM_MODE_FIFO_REGS.WAVE_NUM = 0;
			mt_pwm_26M_clk_enable_hal(1);
			pwm_set_spec_config(&conf);
		} else if (sub_cmd == 2) {
			struct pwm_spec_config conf;

			pr_debug("=============clk source select test: Others===============\n");
			conf.pwm_no = pwm_no;
			conf.mode = PWM_MODE_FIFO;
			conf.clk_div = CLK_DIV1;
			conf.clk_src = PWM_CLK_NEW_MODE_BLOCK;
			conf.PWM_MODE_FIFO_REGS.IDLE_VALUE = IDLE_FALSE;
			conf.PWM_MODE_FIFO_REGS.GUARD_VALUE = GUARD_FALSE;
			conf.PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE = 63;
			conf.PWM_MODE_FIFO_REGS.HDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.LDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.GDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.SEND_DATA0 = 0xf0f0f0f0;
			conf.PWM_MODE_FIFO_REGS.SEND_DATA1 = 0xf0f0f0f0;
			conf.PWM_MODE_FIFO_REGS.WAVE_NUM = 0;
			mt_pwm_26M_clk_enable_hal(0);
			pwm_set_spec_config(&conf);
		} /* end sub cmd */
	} else if (cmd == 2) {
		if (sub_cmd == 1) {
			struct pwm_spec_config conf;

			pr_debug("=============Stop bit test : 63===============\n");
			conf.pwm_no = pwm_no;
			conf.mode = PWM_MODE_FIFO;
			conf.clk_div = CLK_DIV1;
			conf.clk_src = PWM_CLK_NEW_MODE_BLOCK;
			conf.PWM_MODE_FIFO_REGS.IDLE_VALUE = IDLE_FALSE;
			conf.PWM_MODE_FIFO_REGS.GUARD_VALUE = GUARD_FALSE;
			conf.PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE = 63;
			conf.PWM_MODE_FIFO_REGS.HDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.LDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.GDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.SEND_DATA0 = 0x0000ff11;
			conf.PWM_MODE_FIFO_REGS.SEND_DATA1 = 0xffffffff;
			conf.PWM_MODE_FIFO_REGS.WAVE_NUM = 0;
			mt_pwm_26M_clk_enable_hal(1);
			pwm_set_spec_config(&conf);
		} else if (sub_cmd == 2) {
			struct pwm_spec_config conf;

			pr_debug("=============Stop bit test: 62===============\n");
			conf.pwm_no = pwm_no;
			conf.mode = PWM_MODE_FIFO;
			conf.clk_div = CLK_DIV1;
			conf.clk_src = PWM_CLK_NEW_MODE_BLOCK;
			conf.PWM_MODE_FIFO_REGS.IDLE_VALUE = IDLE_FALSE;
			conf.PWM_MODE_FIFO_REGS.GUARD_VALUE = GUARD_FALSE;
			conf.PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE = 62;
			conf.PWM_MODE_FIFO_REGS.HDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.LDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.GDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.SEND_DATA0 = 0x0000ff11;
			conf.PWM_MODE_FIFO_REGS.SEND_DATA1 = 0xffffffff;
			conf.PWM_MODE_FIFO_REGS.WAVE_NUM = 0;
			mt_pwm_26M_clk_enable_hal(1);
			pwm_set_spec_config(&conf);
		} else if (sub_cmd == 3) {
			struct pwm_spec_config conf;

			pr_debug("=============Stop bit test: 31===============\n");
			conf.pwm_no = pwm_no;
			conf.mode = PWM_MODE_FIFO;
			conf.clk_div = CLK_DIV1;
			conf.clk_src = PWM_CLK_NEW_MODE_BLOCK;
			conf.PWM_MODE_FIFO_REGS.IDLE_VALUE = IDLE_FALSE;
			conf.PWM_MODE_FIFO_REGS.GUARD_VALUE = GUARD_FALSE;
			conf.PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE = 31;
			conf.PWM_MODE_FIFO_REGS.HDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.LDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.GDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.SEND_DATA0 = 0x0000ff11;
			conf.PWM_MODE_FIFO_REGS.SEND_DATA1 = 0xffffffff;
			conf.PWM_MODE_FIFO_REGS.WAVE_NUM = 0;
			mt_pwm_26M_clk_enable_hal(1);
			pwm_set_spec_config(&conf);
		} /* end sub cmd */
	} else if (cmd == 3) {
		if (sub_cmd == 1) {
			struct pwm_spec_config conf;

			pr_debug("=============Wave number test : 0===============\n");
			conf.pwm_no = pwm_no;
			conf.mode = PWM_MODE_FIFO;
			conf.clk_div = CLK_DIV1;
			conf.clk_src = PWM_CLK_NEW_MODE_BLOCK;
			conf.PWM_MODE_FIFO_REGS.IDLE_VALUE = IDLE_FALSE;
			conf.PWM_MODE_FIFO_REGS.GUARD_VALUE = GUARD_FALSE;
			conf.PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE = 63;
			conf.PWM_MODE_FIFO_REGS.HDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.LDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.GDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.SEND_DATA0 = 0xf0f0f0f0;
			conf.PWM_MODE_FIFO_REGS.SEND_DATA1 = 0xf0f0f0f0;
			conf.PWM_MODE_FIFO_REGS.WAVE_NUM = 0;
			mt_pwm_26M_clk_enable_hal(1);
			pwm_set_spec_config(&conf);
		} else if (sub_cmd == 2) {
			struct pwm_spec_config conf;

			mt_set_intr_enable_hal(0);
			pr_debug("=============Wave Number test: 1===============\n");
			conf.pwm_no = pwm_no;
			conf.mode = PWM_MODE_FIFO;
			conf.clk_div = CLK_DIV1;
			conf.clk_src = PWM_CLK_NEW_MODE_BLOCK;
			conf.PWM_MODE_FIFO_REGS.IDLE_VALUE = IDLE_FALSE;
			conf.PWM_MODE_FIFO_REGS.GUARD_VALUE = GUARD_FALSE;
			conf.PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE = 63;
			conf.PWM_MODE_FIFO_REGS.HDURATION = 119;
			conf.PWM_MODE_FIFO_REGS.LDURATION = 119;
			conf.PWM_MODE_FIFO_REGS.GDURATION = 0;
			conf.PWM_MODE_FIFO_REGS.SEND_DATA0 = 0x0000ff11;
			conf.PWM_MODE_FIFO_REGS.SEND_DATA1 = 0xffffffff;
			conf.PWM_MODE_FIFO_REGS.WAVE_NUM = 2;
			mt_pwm_26M_clk_enable_hal(1);
			pwm_set_spec_config(&conf);

			mt_set_intr_ack_hal(0);

		} /* end sub cmd */
	} else if (cmd == 4) {
		if (sub_cmd == 1) {
			struct pwm_spec_config conf;

			pr_debug("=============Clk select test : 32KHz===============\n");
			conf.pwm_no = pwm_no;
			conf.mode = PWM_MODE_OLD;
			conf.clk_div = CLK_DIV1;
			conf.clk_src = PWM_CLK_OLD_MODE_32K; /* 16KHz */
			conf.PWM_MODE_OLD_REGS.IDLE_VALUE = IDLE_FALSE;
			conf.PWM_MODE_OLD_REGS.GUARD_VALUE = GUARD_FALSE;
			conf.PWM_MODE_OLD_REGS.GDURATION = 0;
			conf.PWM_MODE_OLD_REGS.WAVE_NUM = 0;
			conf.PWM_MODE_OLD_REGS.DATA_WIDTH = 10;
			conf.PWM_MODE_OLD_REGS.THRESH = 5;
			pwm_set_spec_config(&conf);
		} else if (sub_cmd == 2) {
			struct pwm_spec_config conf;

			pr_debug("=============Clk select test : 26MHz/1625===============\n");
			conf.pwm_no = pwm_no;
			conf.mode = PWM_MODE_OLD;
			conf.clk_div = CLK_DIV1;
			conf.clk_src = PWM_CLK_OLD_MODE_32K;  /* 16KHz */
			conf.PWM_MODE_OLD_REGS.IDLE_VALUE = IDLE_FALSE;
			conf.PWM_MODE_OLD_REGS.GUARD_VALUE = GUARD_FALSE;
			conf.PWM_MODE_OLD_REGS.GDURATION = 0;
			conf.PWM_MODE_OLD_REGS.WAVE_NUM = 0;
			conf.PWM_MODE_OLD_REGS.DATA_WIDTH = 10;
			conf.PWM_MODE_OLD_REGS.THRESH = 5;
			pwm_set_spec_config(&conf);
		} else if (sub_cmd == 3) {
			struct pwm_spec_config conf;

			pr_debug("=============Clk select test : 26MHz===============\n");
			conf.pwm_no = pwm_no;
			conf.mode = PWM_MODE_OLD;
			conf.clk_div = CLK_DIV1;
			conf.clk_src = PWM_CLK_OLD_MODE_BLOCK; /* 26MHz */
			conf.PWM_MODE_OLD_REGS.IDLE_VALUE = IDLE_FALSE;
			conf.PWM_MODE_OLD_REGS.GUARD_VALUE = GUARD_FALSE;
			conf.PWM_MODE_OLD_REGS.GDURATION = 0;
			conf.PWM_MODE_OLD_REGS.WAVE_NUM = 0;
			conf.PWM_MODE_OLD_REGS.DATA_WIDTH = 10;
			conf.PWM_MODE_OLD_REGS.THRESH = 5;
			pwm_set_spec_config(&conf);
		} /* end sub cmd */
	} else if (cmd == 5) {
		struct pwm_spec_config conf;

		PWMDBG("=============3DLCM test===============\n");
		conf.mode = PWM_MODE_3DLCM;
		conf.pwm_no = pwm_no;
		conf.clk_div = CLK_DIV1;
		conf.clk_src = PWM_CLK_NEW_MODE_BLOCK;
		conf.PWM_MODE_FIFO_REGS.IDLE_VALUE = IDLE_FALSE;
		conf.PWM_MODE_FIFO_REGS.GUARD_VALUE = GUARD_FALSE;
		conf.PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE = 63;
		conf.PWM_MODE_FIFO_REGS.HDURATION = 0;
		conf.PWM_MODE_FIFO_REGS.LDURATION = 0;
		conf.PWM_MODE_FIFO_REGS.GDURATION = 0;
		conf.PWM_MODE_FIFO_REGS.SEND_DATA0 = 0xf0f0f0f0;
		conf.PWM_MODE_FIFO_REGS.SEND_DATA1 = 0xf0f0f0f0;
		conf.PWM_MODE_FIFO_REGS.WAVE_NUM = 0;
		mt_pwm_26M_clk_enable_hal(1);
		pwm_set_spec_config(&conf);
	} else if (cmd == 6) {
		PWMDBG(" \tTest all gpio, This coverd by above test case!\n");
	} else if (cmd == 7) {
		struct pwm_spec_config conf;

		PWMDBG("=============MEMO test===============\n");
		conf.mode = PWM_MODE_MEMORY;
		conf.pwm_no = pwm_no;
		conf.clk_div = CLK_DIV1;
		conf.clk_src = PWM_CLK_NEW_MODE_BLOCK;
		conf.PWM_MODE_MEMORY_REGS.IDLE_VALUE = IDLE_FALSE;
		conf.PWM_MODE_MEMORY_REGS.GUARD_VALUE = GUARD_FALSE;
		conf.PWM_MODE_MEMORY_REGS.HDURATION = 119;
		conf.PWM_MODE_MEMORY_REGS.LDURATION = 119;
		conf.PWM_MODE_MEMORY_REGS.GDURATION = 0;
		conf.PWM_MODE_MEMORY_REGS.WAVE_NUM = 0;
		conf.PWM_MODE_MEMORY_REGS.STOP_BITPOS_VALUE = 32;

		mt_pwm_26M_clk_enable_hal(1);
		unsigned int *phys;
		unsigned int *virt;

		virt = dma_alloc_coherent(NULL, 8, &phys, GFP_KERNEL);
		/* virt = (unsigned int*)malloc(sizeof(unsigned int) * 128); */
		unsigned int *membuff = virt;
		/* static unsigned int data = {0xaaaaaaaa, 0xaaaaaaaa}; */
		membuff[0] = 0xaaaaaaaa;
		membuff[1] = 0xffff0000;
		/* conf.PWM_MODE_MEMORY_REGS.BUF0_SIZE = sizeof(data)/sizeof(data[0])-1; */
		conf.PWM_MODE_MEMORY_REGS.BUF0_SIZE = 8;
		conf.PWM_MODE_MEMORY_REGS.BUF0_BASE_ADDR = phys;
		pwm_set_spec_config(&conf);
	} else {
		PWMDBG(" \tInvalid Command!\n");
	}
#endif
	return count;
}

static ssize_t pwm_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	pwm_debug_show_hal();
	return snprintf(buf, 32, "pwm power_flag = 0x%08x\n", (unsigned int)pwm_dev->power_flag);
}

static DEVICE_ATTR(pwm_debug, 0644, pwm_debug_show, pwm_debug_store);

static int mt_pwm_probe(struct platform_device *pdev)
{
	int ret;

#ifdef CONFIG_OF
		pwm_base = of_iomap(pdev->dev.of_node, 0);
		if (!pwm_base) {
			PWMDBG("PWM iomap failed\n");
			return -ENODEV;
		};

#if 0
		pwm_irqnr = irq_of_parse_and_map(pdev->dev.of_node, 0);
		if (!pwm_irqnr) {
			PWMDBG("PWM get irqnr failed\n");
			return -ENODEV;
		}
		PWMDBG("pwm base: 0x%p	pwm irq: %d\n", pwm_base, pwm_irqnr);
#endif
PWMDBG("pwm base: 0x%p\n", pwm_base);

#endif

#if !defined(CONFIG_MTK_CLKMGR)
	ret = mt_get_pwm_clk_src(pdev);
	if (ret != 0)
		PWMDBG("[%s]: Fail :%d\n", __func__, ret);
#endif	/* !defined(CONFIG_MTK_CLKMGR) */

	platform_set_drvdata(pdev, pwm_dev);

	ret = device_create_file(&pdev->dev, &dev_attr_pwm_debug);
	if (ret)
		PWMDBG("error creating sysfs files: pwm_debug\n");

#ifdef CONFIG_OF
/* r = request_irq(pwm_irqnr, mt_pwm_irq, IRQF_TRIGGER_LOW, PWM_DEVICE, NULL); */
#else
/* request_irq(69, mt_pwm_irq, IRQF_TRIGGER_LOW, "mt6589_pwm", NULL); */
#endif

#if 0 /* for support gpio pinctrl standardization */
	struct pinctrl *pinctrl;

	pinctrl = devm_pinctrl_get_select(&pdev->dev, "state_pwm2");
	if (IS_ERR(pinctrl)) {
		ret = PTR_ERR(pinctrl);
		dev_err(&pdev->dev, "Cannot find pwm pinctrl!\n");
		return -1;
	}
#endif

	return RSUCCESS;
}

static int  mt_pwm_remove(struct platform_device *pdev)
{
	if (!pdev) {
		PWMDBG("The plaform device is not exist\n");
		return -EBADADDR;
	}
	device_remove_file(&pdev->dev, &dev_attr_pwm_debug);

	PWMDBG("mt_pwm_remove\n");
	return RSUCCESS;
}

static void mt_pwm_shutdown(struct platform_device *pdev)
{
	PWMDBG("mt_pwm_shutdown\n");
}

#ifdef CONFIG_OF
static const struct of_device_id pwm_of_match[] = {
	{.compatible = "mediatek,pwm",},
	{.compatible = "mediatek,mt8163-pwm",},
	{.compatible = "mediatek,mt8173-pwm",},
	{.compatible = "mediatek,mt8127-pwm",},
	{.compatible = "mediatek,mt2701-pwm",},
	{},
};
#endif

struct platform_driver pwm_plat_driver = {
	.probe = mt_pwm_probe,
	.remove = mt_pwm_remove,
	.shutdown = mt_pwm_shutdown,
	.driver = {
		.name = "mt-pwm",
#ifdef CONFIG_OF
		.of_match_table = pwm_of_match,
#endif
	},
};

static int __init mt_pwm_init(void)
{
	int ret;
#ifndef CONFIG_OF
	ret = platform_device_register(&pwm_plat_dev);
	if (ret < 0) {
		PWMDBG("platform_device_register error\n");
		goto out;
	}
#endif
	ret = platform_driver_register(&pwm_plat_driver);
	if (ret < 0) {
		PWMDBG("platform_driver_register error\n");
		goto out;
	}

out:
	mt_pwm_init_power_flag(&(pwm_dev->power_flag));
	return ret;
}

static void __exit mt_pwm_exit(void)
{
#ifndef CONFIG_OF
	platform_device_unregister(&pwm_plat_dev);
#endif
	platform_driver_unregister(&pwm_plat_driver);
}

module_init(mt_pwm_init);
module_exit(mt_pwm_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MTK");
MODULE_DESCRIPTION(" This module is for mtk chip of mediatek");

