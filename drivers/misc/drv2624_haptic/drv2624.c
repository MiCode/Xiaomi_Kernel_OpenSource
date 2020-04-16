/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
** Copyright (C) 2020 XiaoMi, Inc.
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** File:
**     drv2624.c
**
** Description:
**     DRV2624 chip driver
**
** =============================================================================
*/
#define DEBUG
#include <linux/init.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/semaphore.h>
#include <linux/device.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/spinlock_types.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/jiffies.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include "drv2624.h"
#include "parse_rtp.h"
static struct drv2624_data *g_DRV2624data = NULL;
static struct regmap_config drv2624_i2c_regmap = {.reg_bits = 8,.val_bits =
	    8,.cache_type = REGCACHE_NONE,
};

static int dev_run_diagnostics(struct drv2624_data *pDRV2624);
static inline int drv2624_calculate_voltage(unsigned int voltage);
static int drv2624_reg_write(struct drv2624_data *pDRV2624,
			     unsigned char reg, unsigned char val);
static int drv2624_set_go_bit(struct drv2624_data *pDRV2624, unsigned char val);
static int drv2624_stop(struct drv2624_data *pDRV2624);
unsigned char rtp_data[MAX_RTP_FILE_SIZE] = { 0 };

static void get_command_str(struct drv2624_data *pDRV2624)
{
	char command = 0;
	int nResult;
	int pop_result = 1;
	int data;
	//int previous_sleeping_time = 0;
	dev_err(pDRV2624->dev, "%s: enter\n", __func__);
	nResult = drv2624_reg_write(pDRV2624, DRV2624_REG_RTP_INPUT, 0x0);	//clear RTP data
	if (nResult < 0) {
		dev_err(pDRV2624->dev, "%s %u %d\n", __func__, __LINE__,
			nResult);
	}

	nResult = drv2624_set_go_bit(pDRV2624, GO);
	if (nResult < 0) {
		dev_err(pDRV2624->dev, "%s write go_bit_value filed %d\n",
			__func__, nResult);
	}

	pDRV2624->mnVibratorPlaying = YES;
	while (pop_result >= 0 && pDRV2624->mnVibratorPlaying == YES) {
		pop_result = pop_running_effect_command(&command, &data);
		dev_err(pDRV2624->dev, "%s: command = %c, data = %d mnVibratorPlaying:%d \n",
			__func__, command, data, pDRV2624->mnVibratorPlaying);

		if (command == 'w') {
			nResult =
			    drv2624_reg_write(pDRV2624, DRV2624_REG_RTP_INPUT,
					      data);
			if (nResult < 0) {
				dev_err(pDRV2624->dev,
					"%s write rtp_input_value filed %d\n",
					__func__, nResult);
			}
		} else if (command == 'd') {
			//nResult = drv2624_reg_write(pDRV2624, DRV2624_REG_RTP_INPUT, data);
			msleep(data);
		} else {
			dev_err(pDRV2624->dev, "%s command error\n", __func__);
			break;
		}
	}
	drv2624_stop(pDRV2624);

}

/**
 * RW Functions for DRV2624 registers through I2C
 * drv2624_reg_read, drv2624_reg_write,
 * drv2624_bulk_read, drv2624_bulk_write
 **/
static int drv2624_reg_read(struct drv2624_data *pDRV2624, unsigned char reg)
{
	unsigned int val;
	int nResult;
	mutex_lock(&pDRV2624->dev_lock);
	nResult = regmap_read(pDRV2624->mpRegmap, reg, &val);
	mutex_unlock(&pDRV2624->dev_lock);
	if (nResult < 0) {
		dev_err(pDRV2624->dev, "%s I2C error %d\n", __func__, nResult);
		return nResult;
	} else {
		dev_dbg(pDRV2624->dev, "%s, Reg[0x%x]=0x%x\n", __func__, reg,
			val);
		return val;
	}
}

static int drv2624_reg_write(struct drv2624_data *pDRV2624,
			     unsigned char reg, unsigned char val)
{
	int nResult;
	mutex_lock(&pDRV2624->dev_lock);
	nResult = regmap_write(pDRV2624->mpRegmap, reg, val);
	mutex_unlock(&pDRV2624->dev_lock);
	if (nResult < 0)
		dev_err(pDRV2624->dev, "%s reg=0x%x, value=0%x error %d\n",
			__func__, reg, val, nResult);
	dev_dbg(pDRV2624->dev, "%s, Reg[0x%x]=0x%x\n", __func__, reg, val);
	return nResult;
}

static int drv2624_bulk_read(struct drv2624_data *pDRV2624,
			     unsigned char reg, unsigned char *buf,
			     unsigned int count)
{
	int nResult;
	mutex_lock(&pDRV2624->dev_lock);
	nResult = regmap_bulk_read(pDRV2624->mpRegmap, reg, buf, count);
	mutex_unlock(&pDRV2624->dev_lock);
	if (nResult < 0)
		dev_err(pDRV2624->dev, "%s reg=0%x, count=%d error %d\n",
			__func__, reg, count, nResult);
	return nResult;
}

static int drv2624_bulk_write(struct drv2624_data *pDRV2624,
			      unsigned char reg, const u8 *buf,
			      unsigned int count)
{
	int nResult, i;
	mutex_lock(&pDRV2624->dev_lock);
	nResult = regmap_bulk_write(pDRV2624->mpRegmap, reg, buf, count);
	mutex_unlock(&pDRV2624->dev_lock);
	if (nResult < 0)
		dev_err(pDRV2624->dev, "%s reg=0%x, count=%d error %d\n",
			__func__, reg, count, nResult);
	for (i = 0; i < count; i++)
		dev_dbg(pDRV2624->dev, "%s, Reg[0x%x]=0x%x\n", __func__,
			reg + i, buf[i]);
	return nResult;
}

static int drv2624_set_bits(struct drv2624_data *pDRV2624,
			    unsigned char reg, unsigned char mask,
			    unsigned char val)
{
	int nResult;
	mutex_lock(&pDRV2624->dev_lock);
	nResult = regmap_update_bits(pDRV2624->mpRegmap, reg, mask, val);
	mutex_unlock(&pDRV2624->dev_lock);
	if (nResult < 0)
		dev_err(pDRV2624->dev,
			"%s reg=%x, mask=0x%x, value=0x%x error %d\n",
			__func__, reg, mask, val, nResult);
	dev_dbg(pDRV2624->dev, "%s, Reg[0x%x]:M=0x%x, V=0x%x\n", __func__,
		reg, mask, val);
	return nResult;
}

/**
 *
 * bRTP = NO == 0; Enable all interrupt of DRV2624
 * bRTP = 1 == 1; Only Enable critical interrupt,  PROCESS_DONE and PRG_ERROR
 *
 **/
static int drv2624_enableIRQ(struct drv2624_data *pDRV2624, unsigned char bRTP)
{
	int nResult = 0;
	unsigned char mask = INT_ENABLE_CRITICAL;
	if (!pDRV2624->mbIRQUsed)
		goto end;
	if (pDRV2624->mbIRQEnabled)
		goto end;
	if (bRTP == 0)
		mask = INT_ENABLE_ALL;
	nResult = drv2624_reg_read(pDRV2624, DRV2624_REG_STATUS);
	if (nResult < 0)
		goto end;
	nResult = drv2624_reg_write(pDRV2624, DRV2624_REG_INT_ENABLE, mask);
	if (nResult < 0)
		goto end;
	enable_irq(pDRV2624->mnIRQ);
	pDRV2624->mbIRQEnabled = true;
end:	return nResult;
}

static void drv2624_disableIRQ(struct drv2624_data *pDRV2624)
{
	dev_err(pDRV2624->dev, "%s:entter\n", __func__);
	if (pDRV2624->mbIRQUsed) {
		if (pDRV2624->mbIRQEnabled) {
			disable_irq_nosync(pDRV2624->mnIRQ);
			drv2624_reg_write(pDRV2624, DRV2624_REG_INT_ENABLE,
					  INT_MASK_ALL);
			pDRV2624->mbIRQEnabled = false;
		}
	}
}

static int drv2624_set_go_bit(struct drv2624_data *pDRV2624, unsigned char val)
{
	int nResult = 0, value = 0;
	int retry = GO_BIT_MAX_RETRY_CNT;
	val &= DRV2624_GO_BIT_MASK;
	nResult = drv2624_reg_write(pDRV2624, DRV2624_REG_GO, val);
	if (nResult < 0)
		goto end;
	mdelay(GO_BIT_CHECK_INTERVAL);
	value = drv2624_reg_read(pDRV2624, DRV2624_REG_GO);
	if (value < 0) {
		nResult = value;
		goto end;
	}
	dev_dbg(pDRV2624->dev, "%s, go value = %d\n", __func__, value);
	while (((value & DRV2624_GO_BIT_MASK) != val) && (retry > 0)) {
		value = drv2624_reg_read(pDRV2624, DRV2624_REG_GO);
		dev_dbg(pDRV2624->dev, "%s, GO bit %d\n", __func__, value);
		mdelay(GO_BIT_CHECK_INTERVAL);
		retry--;
	}
	dev_dbg(pDRV2624->dev, "%s: pull go bit success!\n", __func__);
end:	return nResult;
}

static inline int drv2624_change_mode(struct drv2624_data *pDRV2624,
				      drv2624_mode_t work_mode)
{
	pDRV2624->mnWorkMode = work_mode;
	return drv2624_set_bits(pDRV2624, DRV2624_REG_MODE, WORKMODE_MASK,
				work_mode);
}

static inline void drv2624_set_stopflag(struct drv2624_data *pDRV2624)
{
	pDRV2624->mnVibratorPlaying = NO;
	dev_dbg(pDRV2624->dev, "%s: mnVibratorPlaying=%d\n", __func__,
			 pDRV2624->mnVibratorPlaying);
//      pDRV2624->mnEffectType = 0;
}

static int drv2624_get_diag_result(struct drv2624_data *pDRV2624,
				   unsigned char nStatus)
{
	int Re = 0, nResult = 0;
	pDRV2624->mDiagResult.mnResult = nStatus;
	if ((nStatus & DIAG_MASK) != DIAG_SUCCESS)
		dev_err(pDRV2624->dev, "%s: Diagnostic fail\n", __func__);

	else {
		nResult = drv2624_reg_read(pDRV2624, DRV2624_REG_DIAG_Z);
		if (nResult < 0)
			goto end;
		pDRV2624->mDiagResult.mnDiagZ = nResult;
		nResult = drv2624_reg_read(pDRV2624, DRV2624_REG_DIAG_K);
		if (nResult < 0)
			goto end;
		pDRV2624->mDiagResult.mnDiagK = nResult;
		Re = 478.43 * (pDRV2624->mDiagResult.mnDiagZ /
			       (4 * pDRV2624->mDiagResult.mnDiagK + 719));
		dev_dbg(pDRV2624->dev,
			"%s: ZResult=0x%x, CurrentK=0x%x, Re = %d ohm\n",
			__func__, pDRV2624->mDiagResult.mnDiagZ,
			pDRV2624->mDiagResult.mnDiagK, Re);
	}
end:	return nResult;
}

/**
 * No need to stop in Waveform Sequencer Mode.
 * 1. Disable irq
 * 2. Cancel hrimer
 * 3. Set GO bit as STOP
 * 4. Set stop flag in drv2624_data struct
 *
 **/
#if 1
static int drv2624_stop(struct drv2624_data *pDRV2624)
{
	int nResult = 0, mode = 0;
	dev_dbg(pDRV2624->dev, "%s enter!\n", __func__);
	nResult = drv2624_reg_read(pDRV2624, DRV2624_REG_MODE);
	if (nResult < 0) {
		dev_err(pDRV2624->dev, "%s: drv2624_reg_read failed\n", __func__);
		return nResult;
	}
	mode = nResult & WORKMODE_MASK;
	if (mode == MODE_WAVEFORM_SEQUENCER) {
		dev_err(pDRV2624->dev, "%s: In sequence play, ignore stop\n", __func__);
		return 0;
	}
	if (pDRV2624->mnVibratorPlaying == YES) {
		if (pDRV2624->mbIRQUsed)
			drv2624_disableIRQ(pDRV2624);
		if (hrtimer_active(&pDRV2624->haptics_timer))
			hrtimer_cancel(&pDRV2624->haptics_timer);
		nResult = drv2624_set_go_bit(pDRV2624, STOP);
		drv2624_set_stopflag(pDRV2624);
	}
	dev_dbg(pDRV2624->dev, "%s: exit, mnVibratorPlaying=%d\n", __func__, pDRV2624->mnVibratorPlaying);
	return nResult;
}
#else
static int drv2624_stop(struct drv2624_data *pDRV2624)
{
	int nResult = 0;
	//int nResult = 0, mode = 0;
	dev_dbg(pDRV2624->dev, "%s enter!\n", __func__);
#if 0
	nResult = drv2624_reg_read(pDRV2624, DRV2624_REG_MODE);
	if (nResult < 0)
		return nResult;
	mode = nResult & WORKMODE_MASK;
	if (mode == MODE_WAVEFORM_SEQUENCER) {
		dev_dbg(pDRV2624->dev, "In sequence play, ignore stop\n");
		return 0;
	}
#endif
	if (pDRV2624->mnVibratorPlaying == YES) {
		if (pDRV2624->mbIRQUsed)
			drv2624_disableIRQ(pDRV2624);
		if (hrtimer_active(&pDRV2624->haptics_timer))
			hrtimer_cancel(&pDRV2624->haptics_timer);
		nResult = drv2624_set_go_bit(pDRV2624, STOP);
		drv2624_set_stopflag(pDRV2624);
	}
	//debugging reset mode, play ram mode continuious, cause problems
	drv2624_change_mode(pDRV2624, DRV2624_RTP_MODE);
	return nResult;
}
#endif

#ifdef ANDROID_TIMED_OUTPUT
static int vibrator_get_time(struct timed_output_dev *dev)
{
	struct drv2624_data *pDRV2624 =
	    container_of(dev, struct drv2624_data, to_dev);
	if (hrtimer_active(&pDRV2624->haptics_timer)) {
		ktime_t r = hrtimer_get_remaining(&pDRV2624->haptics_timer);
		return ktime_to_ms(r);
	}
	return 0;
}

static void vibrator_enable(struct timed_output_dev *dev, int value)
{
	int nResult = 0;
	struct drv2624_data *pDRV2624 =
	    container_of(dev, struct drv2624_data, to_dev);
	dev_dbg(pDRV2624->dev, "%s, value=%d\n", __func__, value);
	mutex_lock(&pDRV2624->lock);
	dev_dbg(pDRV2624->dev, "%s, afer mnWorkMode=0x%x\n", __func__,
		pDRV2624->mnWorkMode);
	drv2624_stop(pDRV2624);
	if (value > 0) {
		nResult = drv2624_change_mode(pDRV2624, DRV2624_RTP_MODE);
		if (nResult < 0)
			goto end;
		nResult = drv2624_set_go_bit(pDRV2624, GO);
		if (nResult >= 0) {
			value = (value > MAX_TIMEOUT) ? MAX_TIMEOUT : value;
			hrtimer_start(&pDRV2624->haptics_timer,
				      ns_to_ktime((u64) value *
						  NSEC_PER_MSEC),
				      HRTIMER_MODE_REL);
			pDRV2624->mnVibratorPlaying = YES;
			if (pDRV2624->mbIRQUsed)
				nResult = drv2624_enableIRQ(pDRV2624, YES);
		}
	}
end:	mutex_unlock(&pDRV2624->lock);
}

#endif /* ANDROID_TIMED_OUTPUT */
static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	struct drv2624_data *pDRV2624 =
	    container_of(timer, struct drv2624_data, haptics_timer);
	dev_dbg(pDRV2624->dev, "%s\n", __func__);
	schedule_work(&pDRV2624->vibrator_work);
	return HRTIMER_NORESTART;
}

/**
 * 1. Do work due to pDRV2624->mnWorkMode set before.
 * 2. For WORK_EFFECTSEQUENCER, WORK_CALIBRATION and WORK_DIAGNOSTIC
 *    check the GO bit until the process in DRV2624 has completed.
 * 3. For WORK_VIBRATOR, Stop DRV2624 directly.
 **/
static void vibrator_work_routine(struct work_struct *work)
{
	struct drv2624_data *pDRV2624 =
	    container_of(work, struct drv2624_data, vibrator_work);
	unsigned char status;
	int nResult = 0;
	mutex_lock(&pDRV2624->lock);
	dev_dbg(pDRV2624->dev, "%s, afer mnWorkMode=0x%x\n", __func__,
		pDRV2624->mnWorkMode);
	if (pDRV2624->mbIRQUsed) {
		pDRV2624->mnIntStatus =
		    drv2624_reg_read(pDRV2624, DRV2624_REG_STATUS);
		if (nResult < 0)
			goto err;
		drv2624_disableIRQ(pDRV2624);
		status = pDRV2624->mnIntStatus;
		dev_dbg(pDRV2624->dev, "%s, status=0x%x\n", __func__,
			pDRV2624->mnIntStatus);
		if (status & OVERCURRENT_MASK)
			dev_err(pDRV2624->dev,
				"ERROR, Over Current detected!!\n");
		if (status & OVERTEMPRATURE_MASK)
			dev_err(pDRV2624->dev,
				"ERROR, Over Temperature detected!!\n");
		if (status & ULVO_MASK)
			dev_err(pDRV2624->dev, "ERROR, VDD drop observed!!\n");
		if (status & PRG_ERR_MASK)
			dev_err(pDRV2624->dev, "ERROR, PRG error!!\n");
	}
	if (pDRV2624->mnWorkMode == DRV2624_RTP_MODE) {
		drv2624_stop(pDRV2624);
	} else if (pDRV2624->mnWorkMode == DRV2624_RAM_MODE) {
		dev_err(pDRV2624->dev, "%s: read go bit\n", __func__);
		status = drv2624_reg_read(pDRV2624, DRV2624_REG_GO);
		if ((status < 0) || (status == STOP)
		    || !pDRV2624->mnVibratorPlaying) {
			dev_err(pDRV2624->dev, "%s: status error = %d\n",
				__func__, status);
			drv2624_stop(pDRV2624);
		} else {
			if (!hrtimer_active(&pDRV2624->haptics_timer)) {
				dev_dbg(pDRV2624->dev,
					"will check GO bit after %d ms\n",
					GO_BIT_CHECK_INTERVAL);
				hrtimer_start(&pDRV2624->haptics_timer,
					      ns_to_ktime((u64)
							  GO_BIT_CHECK_INTERVAL
							  * NSEC_PER_MSEC),
					      HRTIMER_MODE_REL);
			}
		}
	}
err:	mutex_unlock(&pDRV2624->lock);
}

static int dev_auto_calibrate(struct drv2624_data *pDRV2624)
{
	int nResult = 0;
	dev_info(pDRV2624->dev, "%s enter!\n", __func__);

	/* Set MODE register to Auto Level Calibration Routine
	 * and choose Trigger Function Internal */
	nResult =
	    drv2624_reg_write(pDRV2624, DRV2624_REG_MODE,
			      DRV2624_CALIBRATION_MODE_CFG);
	if (nResult < 0)
		goto end;
	nResult = drv2624_change_mode(pDRV2624, MODE_CALIBRATION);
	if (nResult < 0) {
		dev_err(pDRV2624->dev, "%s: change mode  Done nResult = %d\n",
			__func__, nResult);
		goto end;
	}
	nResult =
	    drv2624_set_bits(pDRV2624, AUTO_CAL_TIME_REG, AUTO_CAL_TIME_MASK,
			     AUTO_CAL_TIME_AUTO_TRIGGER);
	if (nResult < 0) {
		dev_err(pDRV2624->dev, "%s: set bits Done nResult = %d\n",
			__func__, nResult);
		goto end;
	}
	nResult = drv2624_set_go_bit(pDRV2624, GO);
	if (nResult < 0) {
		dev_err(pDRV2624->dev,
			"%s: calibrate go bit Done nResult = %d\n", __func__,
			nResult);
		goto end;
	}
	msleep(1000);		/* waiting auto calibration finished */
	pDRV2624->mnVibratorPlaying = YES;
	return nResult;
end:	dev_err(pDRV2624->dev, "%s: Calibtion Done nResult = %d\n", __func__,
		nResult);
	return nResult;
}

static int drv2624_calibration_fetch(struct drv2624_data *pDRV2624)
{
	int nResult;
	dev_dbg(pDRV2624->dev, "%s: enter!\n", __func__);
	nResult = drv2624_reg_read(pDRV2624, DRV2624_REG_STATUS);
	if (nResult < 0)
		goto end;
	pDRV2624->mnIntStatus = nResult;
	drv2624_reg_write(pDRV2624, DRV2624_REG_CAL_COMP,
			  pDRV2624->mAutoCalResult.mnCalComp);
	drv2624_reg_write(pDRV2624, DRV2624_REG_CAL_BEMF,
			  pDRV2624->mAutoCalResult.mnCalBemf);
	drv2624_set_bits(pDRV2624, DRV2624_REG_LOOP_CONTROL, BEMFGAIN_MASK,
			 pDRV2624->mAutoCalResult.mnCalGain);

end:	dev_dbg(pDRV2624->dev, "%s: fetched calib values = %d\n", __func__,
		nResult);
	return nResult;
}

#if 0
//char *drv2624_w_path = "/mnt/vendor/persist/Drv2624Date.ini";
//char *drv2624_r_path = "/vendor/firmware/Drv2624Date.ini";
char *drv2624_path = "/vendor/firmware/Drv2624Date.ini";
//char *drv2624_r_path = "/vendor/firmware/audio/Drv2624Date.ini";

//int vibrator_save_cali_data(char *path, unsigned char val)
int vibrator_save_cali_data(char *path, struct drv2624_data *pDRV2624)
{
	unsigned char buf[4] = { 0 };

	struct file *fp = NULL;
	mm_segment_t fs;
	loff_t pos;

	buf[0] = pDRV2624->mnIntStatus;
	buf[1] = pDRV2624->mAutoCalResult.mnCalComp;
	buf[2] = pDRV2624->mAutoCalResult.mnCalBemf;
	buf[3] = pDRV2624->mAutoCalResult.mnCalGain;

	pr_info("drv2624 %s 0x%x 0x%x 0x%x 0x%x\n", *path, buf[0], buf[1],
	       buf[2], buf[3]);

	if (NULL == fp) {
		fp = filp_open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
	}
	if (IS_ERR(fp)) {
		pr_err("drv2624 write file error\n");
		return -EIO;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(fp, buf, sizeof(buf), &pos);
	filp_close(fp, NULL);
	set_fs(fs);

	return 0;
}

//int vibrator_read_cali_data(char *path)
int vibrator_read_cali_data(char *path, struct drv2624_data *pDRV2624)
{
	unsigned char buf[4] = { 0 };

	struct file *fp = NULL;
	mm_segment_t fs;
	loff_t pos;

	pr_info("%s\n", *path);
	//pDRV2624->mnIntStatus = buf[0];

	if (NULL == fp) {
		fp = filp_open(path, O_RDONLY, 0);
	}
	if (IS_ERR(fp)) {
		pr_err("drv2624 read file error\n");
		return -EIO;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_read(fp, buf, sizeof(buf), &pos);
	pr_info("drv2624 read: 0x%x 0x%x 0x%x 0x%x\n", buf[0], buf[1], buf[2],
	       buf[3]);
	filp_close(fp, NULL);
	set_fs(fs);

	//if(buf[0] != 0){
	pDRV2624->mnIntStatus = buf[0];
	pDRV2624->mAutoCalResult.mnCalComp = buf[1];
	pDRV2624->mAutoCalResult.mnCalBemf = buf[2];
	pDRV2624->mAutoCalResult.mnCalGain = buf[3];
	//}

	return 0;
}

static void drv2624_init_write_cali_data(struct work_struct *work)
{
	//struct drv2624_data *pDRV2624 = container_of(work, struct drv2624_data, haptics_playback_work);

	vibrator_read_cali_data(drv2624_path, g_DRV2624data);
	//mdelay(2);
	//vibrator_read_cali_data(drv2624_r_path, g_DRV2624data);

	if (g_DRV2624data->mnIntStatus != 0x08) {
		dev_dbg(g_DRV2624data->dev, "%s\n", __func__);
		queue_delayed_work(g_DRV2624data->cali_write_workqueue,
				   &g_DRV2624data->cali_write_work,
				   msecs_to_jiffies(2 * 1000));
	} else {
		drv2624_calibration_fetch(g_DRV2624data);
		dev_err(g_DRV2624data->dev,
			"%s: Cal_Result: 0x%x, CalComp: 0x%x, CalBemf: 0x%x, CalGain: 0x%x\n",
			__func__, g_DRV2624data->mnIntStatus,
			g_DRV2624data->mAutoCalResult.mnCalComp,
			g_DRV2624data->mAutoCalResult.mnCalBemf,
			g_DRV2624data->mAutoCalResult.mnCalGain);
		mdelay(5);
		cancel_delayed_work_sync(&g_DRV2624data->cali_write_work);
	}
}
#endif

static int drv2624_get_calibration_result(struct drv2624_data *pDRV2624)
{
	int nResult, cal_bemf, cal_comp, cal_gain;
	dev_dbg(pDRV2624->dev, "%s: enter!\n", __func__);
	nResult = drv2624_reg_read(pDRV2624, DRV2624_REG_STATUS);
	if (nResult < 0)
		goto end;
	pDRV2624->mnIntStatus = nResult;
	cal_comp = drv2624_reg_read(pDRV2624, DRV2624_REG_CAL_COMP);
	if (cal_comp < 0)
		goto end;
	pDRV2624->mAutoCalResult.mnCalComp = cal_comp;
	cal_bemf = drv2624_reg_read(pDRV2624, DRV2624_REG_CAL_BEMF);
	if (cal_bemf < 0)
		goto end;
	pDRV2624->mAutoCalResult.mnCalBemf = cal_bemf;
	cal_gain =
	    drv2624_reg_read(pDRV2624,
			     DRV2624_REG_LOOP_CONTROL) & BEMFGAIN_MASK;
	if (cal_gain < 0)
		goto end;
	pDRV2624->mAutoCalResult.mnCalGain = cal_gain;
end:	dev_dbg(pDRV2624->dev, "%s: nResult = %d\n", __func__, nResult);
	return nResult;
}

/*show calibrtion*/
static ssize_t
drv2624_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int nResult = 0;
	struct drv2624_data *pDRV2624 = dev_get_drvdata(dev);
	nResult = drv2624_get_calibration_result(pDRV2624);
	if (nResult < 0)
		goto end;

	dev_dbg(pDRV2624->dev,
		"%s: Cal_Result: 0x%x, CalComp: 0x%x, CalBemf: 0x%x, CalGain: 0x%x\n",
		__func__, pDRV2624->mnIntStatus,
		pDRV2624->mAutoCalResult.mnCalComp,
		pDRV2624->mAutoCalResult.mnCalBemf,
		pDRV2624->mAutoCalResult.mnCalGain);

	return snprintf(buf, 100, "%x %x %x %x\n",
			pDRV2624->mnIntStatus,
			pDRV2624->mAutoCalResult.mnCalComp,
			pDRV2624->mAutoCalResult.mnCalBemf,
			pDRV2624->mAutoCalResult.mnCalGain);
end:
	return nResult;
}

/* store calibration*/
static ssize_t drv2624_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t size)
{
	int nResult = 0;
	struct drv2624_data *tdev = dev_get_drvdata(dev);
	if ((buf[0] == 'C') && (buf[1] == 'A') &&
	    (buf[2] == 'L') && (buf[3] == 'I') && (buf[4] == 'B')) {
		nResult = dev_auto_calibrate(tdev);
	} else if ((buf[0] == 'D') && (buf[1] == 'I')
		   && (buf[2] == 'A') && (buf[3] == 'G')) {
		nResult = dev_run_diagnostics(tdev);
	}

#if 0
	nResult = drv2624_get_calibration_result(tdev);

	dev_err(tdev->dev,
		"%s: Cal_Result: 0x%x, CalComp: 0x%x, CalBemf: 0x%x, CalGain: 0x%x\n",
		__func__, tdev->mnIntStatus,
		tdev->mAutoCalResult.mnCalComp,
		tdev->mAutoCalResult.mnCalBemf, tdev->mAutoCalResult.mnCalGain);

	if (tdev->mnIntStatus == 0x08) {
		vibrator_save_cali_data(drv2624_path, tdev);
		//mdelay(2);
		//vibrator_save_cali_data(drv2624_r_path, tdev);
	}
#endif
	return size;
}

/*show name*/
static ssize_t
NAME_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct drv2624_data *pDRV2624 = dev_get_drvdata(dev);
	int nResult = 0;
	nResult = drv2624_reg_read(pDRV2624, DRV2624_REG_ID);
	if (nResult == 0x3) {
		//nResult =
		 //   drv2624_reg_write(pDRV2624, DRV2624_REG_RTP_INPUT, 0x7f);
		return snprintf(buf, 100, "TI-DRV2624\n");
	} else {
		return snprintf(buf, 100, "i2c-error\n");
	}
}

/* store name*/
static ssize_t NAME_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t size)
{
	struct drv2624_data *pDRV2624 = dev_get_drvdata(dev);
	int rc = 0;
	int nResult = 0;
	int val = 0;
	rc = kstrtouint(buf, 0, &val);
	if (val == 1) {
		nResult =
		    drv2624_reg_write(pDRV2624, DRV2624_REG_RTP_INPUT, 0x7f);
	}

	return size;
}

int reg;
/*
*cat registe value
*
*/
static ssize_t
reg_val_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct drv2624_data *pDRV2624 = dev_get_drvdata(dev);
	int nResult = 0;
	nResult = drv2624_reg_read(pDRV2624, (unsigned char)reg);
	return snprintf(buf, 30, "reg val = 0x%x\n", nResult);
}

/**
*write register value
*
*/
static ssize_t reg_val_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t size)
{
	struct drv2624_data *pDRV2624 = dev_get_drvdata(dev);
	int nResult = 0;
	int reg_value;
	sscanf(buf, "%d %d", &reg, &reg_value);
	dev_err(pDRV2624->dev, "reg = 0x%x , value = 0x%x\n", reg, reg_value);
	nResult = drv2624_reg_write(pDRV2624, (unsigned char)reg, reg_value);
	return size;
}

static ssize_t drv2624_vibrator_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	int nResult = 0;
	struct drv2624_data *pDRV2624 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;;
	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val == 0) {
		nResult = drv2624_set_go_bit(pDRV2624, STOP);
		if (hrtimer_active(&pDRV2624->haptics_timer))
			hrtimer_cancel(&pDRV2624->haptics_timer);
		goto end;
	}
	dev_info(pDRV2624->dev, "val = %d\n", val);
	pDRV2624->play.length = val;
	drv2624_stop(pDRV2624);
	nResult = drv2624_change_mode(pDRV2624, DRV2624_RTP_MODE);
	if (nResult < 0) {
		dev_dbg(pDRV2624->dev, "%s: change_mode nResult = %d\n",
			__func__, nResult);
		goto end;
	}
	nResult = drv2624_reg_write(pDRV2624, DRV2624_REG_RTP_INPUT, 0x7f);
	if (nResult < 0)
		goto end;
	nResult = drv2624_set_go_bit(pDRV2624, GO);
	if (nResult < 0)
		goto end;
	if (pDRV2624->play.length != 0) {
		hrtimer_start(&pDRV2624->haptics_timer,
			      ns_to_ktime((u64) val * NSEC_PER_MSEC),
			      HRTIMER_MODE_REL);
	}
	pDRV2624->mnVibratorPlaying = YES;
end:	return size;
}

static ssize_t
drv2624_vibrator_show(struct device *dev, struct device_attribute *attr,
		      char *buf)
{
	return 1;
}

static ssize_t cali_save_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	int nResult = 0;
	//int rc = 0;
	struct drv2624_data *pDRV2624 = dev_get_drvdata(dev);
	unsigned int cali_mnIntStatus, cali_mnCalComp;
	unsigned int cali_mnCalBemf, cali_mnCalGain;

	nResult =
	    sscanf(buf, "%x %x %x %x", &cali_mnIntStatus, &cali_mnCalComp,
		   &cali_mnCalBemf, &cali_mnCalGain);

	dev_dbg(pDRV2624->dev, "%s: 0x%x 0x%x 0x%x 0x%x\n",
		__func__, cali_mnIntStatus, cali_mnCalComp, cali_mnCalBemf,
		cali_mnCalGain);

	//rc = kstrtouint(buf, 0, &val);

	pDRV2624->mnIntStatus = cali_mnIntStatus;
	pDRV2624->mAutoCalResult.mnCalComp = cali_mnCalComp;
	pDRV2624->mAutoCalResult.mnCalBemf = cali_mnCalBemf;
	pDRV2624->mAutoCalResult.mnCalGain = cali_mnCalGain;

	drv2624_calibration_fetch(pDRV2624);

	return size;
}

static ssize_t
cali_save_show(struct device *dev, struct device_attribute *attr,
		      char *buf)
{
	int nResult = 0;
	struct drv2624_data *pDRV2624 = dev_get_drvdata(dev);

	return snprintf(buf, 100,
			"Cal_Result: 0x%x, CalComp: 0x%x, CalBemf: 0x%x, CalGain: 0x%x\n",
			pDRV2624->mnIntStatus,
			pDRV2624->mAutoCalResult.mnCalComp,
			pDRV2624->mAutoCalResult.mnCalBemf,
			pDRV2624->mAutoCalResult.mnCalGain);
	return nResult;
}

static DEVICE_ATTR(enable, 0664, drv2624_vibrator_show, drv2624_vibrator_store);
static DEVICE_ATTR(cali, 0664, drv2624_show, drv2624_store);
static DEVICE_ATTR(NAME, 0664, NAME_show, NAME_store);
static DEVICE_ATTR(Reg_wr, 0664, reg_val_show, reg_val_store);
static DEVICE_ATTR(cali_save, 0664, cali_save_show, cali_save_store);
static struct attribute *drv2624_vibrator_attributes[] = {
	&dev_attr_cali.attr,
	&dev_attr_NAME.attr,
	&dev_attr_Reg_wr.attr,
	&dev_attr_enable.attr,
	&dev_attr_cali_save.attr,
	NULL
};

static struct attribute_group drv2624_vibrator_attribute_group = {
	.attrs = drv2624_vibrator_attributes
};

static int dev_run_diagnostics(struct drv2624_data *pDRV2624)
{
	int nResult = 0, value = 0;
	dev_info(pDRV2624->dev, "%s\n", __func__);
	nResult = drv2624_change_mode(pDRV2624, DRV2624_DIAG_MODE);
	if (nResult < 0)
		goto end;
	nResult = drv2624_set_go_bit(pDRV2624, GO);
	if (nResult < 0)
		goto end;
	dev_dbg(pDRV2624->dev, "%s: Diag start\n", __func__);
	pDRV2624->mnVibratorPlaying = YES;
	value = drv2624_reg_read(pDRV2624, DRV2624_REG_STATUS);
	if (value < 0) {
		return value;
	}
	drv2624_get_diag_result(pDRV2624, value);
end:	return nResult;
}

/**
 * Play Waveform sequence stored in DRV2624_REG_SEQUENCER_1
 *
 **/
static int drv2624_playEffect(struct drv2624_data *pDRV2624)
{
	int nResult = 0;
	dev_err(pDRV2624->dev, "%s;enter\n", __func__);
	nResult = drv2624_change_mode(pDRV2624, DRV2624_WAVE_SEQ_MODE);
	if (nResult < 0)
		goto end;
	nResult = drv2624_set_go_bit(pDRV2624, GO);
	if (nResult < 0) {
		dev_err(pDRV2624->dev, "%s:enter go bit fail\n", __func__);
		goto end;
	}
	dev_dbg(pDRV2624->dev, "effects start\n");
	pDRV2624->mnVibratorPlaying = YES;
	if (!pDRV2624->mbIRQUsed) {
		schedule_work(&pDRV2624->vibrator_work);
	}
end:	return nResult;
}

static void drv2624_set_ram_mode_reg(struct drv2624_data *pDRV2624)
{
	//drv2624_reg_write(pDRV2624, 0x07, 0x53);
	drv2624_reg_write(pDRV2624, 0x09, 0x07);
	drv2624_reg_write(pDRV2624, 0xa, 0x92);
	drv2624_reg_write(pDRV2624, 0xb, 0x8d);
	drv2624_reg_write(pDRV2624, 0xd, 0x20);
	drv2624_reg_write(pDRV2624, 0x1a, 0x00);
	drv2624_reg_write(pDRV2624, 0x28, 0x11);
	drv2624_reg_write(pDRV2624, 0x29, 0x08);
	drv2624_reg_write(pDRV2624, 0x2b, 0x00);
	drv2624_reg_write(pDRV2624, 0x2c, 0x01);
	//drv2624_reg_write(pDRV2624, 0x2e, 0x00);
	//drv2624_reg_write(pDRV2624, 0x2f, 0xc6);
	drv2624_reg_write(pDRV2624, 0x27, 0x13);
}

static int drv2624_set_waveform(struct drv2624_data *pDRV2624, struct drv2624_waveform_sequencer
				*pSequencer)
{
	int nResult = 0;
	int i = 0;
	unsigned char loop[2] = { 0 };
	unsigned char effects[DRV2624_SEQUENCER_SIZE] = { 0 };
	unsigned char len = 0;
	dev_err(pDRV2624->dev, "%s:enter\n", __func__);
	for (i = 0; i < DRV2624_SEQUENCER_SIZE; i++) {
		if (pSequencer->msWaveform[i].mnEffect != 0) {
			len++;
			if (i < 4)
				loop[0] |= (pSequencer->msWaveform[i].mnLoop << (2 * i));
			else
				loop[1] |= (pSequencer->msWaveform[i].mnLoop << (2 * (i - 4)));
			effects[i] = pSequencer->msWaveform[i].mnEffect;
		} else
			break;
	}
	dev_err(pDRV2624->dev, "%s:len =%d, effects[0] = %d\n", __func__, len,
		effects[0]);
	if (len == 1) {
		nResult =
		    drv2624_reg_write(pDRV2624, DRV2624_REG_SEQUENCER_1, /*0 */
				      effects[0]);
		drv2624_reg_write(pDRV2624, DRV2624_REG_SEQ_LOOP_1, loop[0]);
		nResult = drv2624_reg_write(pDRV2624, 0x10, /*0 */ 0);
	} else
		nResult =
		    drv2624_bulk_write(pDRV2624, DRV2624_REG_SEQUENCER_1,
				       effects, len);
	if (nResult < 0) {
		dev_err(pDRV2624->dev, "sequence error\n");
		goto end;
	}
	if (len > 1) {
		if ((len - 1) <= 4)
			drv2624_reg_write(pDRV2624, DRV2624_REG_SEQ_LOOP_1,
					  loop[0]);

		else
			drv2624_bulk_write(pDRV2624, DRV2624_REG_SEQ_LOOP_1,
					   loop, 2);
	}
end:	return nResult;
}

static int fw_chksum(const struct firmware *fw)
{
	int sum = 0;
	int i = 0;
	int size = fw->size;
	const unsigned char *pBuf = fw->data;
	for (i = 0; i < size; i++) {
		if ((i > 11) && (i < 16)) {
		} else
			sum += pBuf[i];
	}
	return sum;
}

static int drv2624_get_effect_timems(struct drv2624_data *pDRV2624,
				     unsigned char effect)
{
	unsigned char *fw = &pDRV2624->mnFwRam[0];
	u16 header_address, tmp;
	u16 address = 0;
	unsigned char effect_repeats = 0;
	unsigned int effect_size = 0;
	int i = 0;
	unsigned int ticks = 0;
	unsigned int playback_interval = 0;
	effect++;
	header_address = (effect - 1) * 3 + 1;
	tmp = fw[header_address];
	address = tmp << 8 | fw[header_address + 1];
	effect_repeats = (fw[header_address + 2] & 0xe0) >> 5;
	effect_size = fw[header_address + 2] & 0x1f;
	for (i = 0; i < effect_size / 2; i++) {
		ticks += fw[address + (i * 2) + 1];
	}
	playback_interval =
	    (pDRV2624->msWaveformSetting.mnInterval == INTERVAL_5MS) ? 5 : 1;
	return ticks * (effect_repeats + 1) * playback_interval;
}

/* drv2624_firmware_load:
* This function is called by the
* request_firmware_nowait function as soon
* as the firmware has been loaded from the file.
* The firmware structure contains the data and$
* the size of the firmware loaded.
*
* @fw: pointer to firmware file to be dowloaded
* @context: pointer variable to drv2624 data
*/
static void drv2624_firmware_load(const struct firmware *fw, void *context)
{
	struct drv2624_data *pDRV2624 = context;
	int size = 0, fwsize = 0, i = 0;
	const unsigned char *pBuf = NULL;
	//unsigned char *pTmp = NULL;
	mutex_lock(&pDRV2624->lock);
	if (fw != NULL) {
		pBuf = fw->data;
		size = fw->size;
		if (size > 1024) {
			dev_err(pDRV2624->dev,
				"%s, ERROR!! firmware size %d too big\n",
				__func__, size);
		} else {
			memcpy(&(pDRV2624->fw_header), pBuf,
			       sizeof(struct drv2624_fw_header));
			if ((pDRV2624->fw_header.fw_magic !=
			     DRV2624_MAGIC) || (pDRV2624->fw_header.fw_size !=
						size)
			    || (pDRV2624->fw_header.fw_chksum !=
				fw_chksum(fw))) {
				dev_err(pDRV2624->dev,
					"%s, ERROR!! firmware not right:Magic=0x%x, Size=%d, chksum=0x%x\n",
					__func__, pDRV2624->fw_header.fw_magic,
					pDRV2624->fw_header.fw_size,
					pDRV2624->fw_header.fw_chksum);
			} else {
				dev_info(pDRV2624->dev, "%s, firmware good\n",
					 __func__);
				pDRV2624->effects_count =
				    pDRV2624->fw_header.fw_effCount;
				//pDRV2624->play.effect_count = pDRV2624->fw_header.fw_effCount;
				pBuf += sizeof(struct drv2624_fw_header);
				dev_err(pDRV2624->dev,
					"%s: pDRV2624->fw_header.fw_effCount =%d\n",
					__func__,
					pDRV2624->fw_header.fw_effCount);
				//pDRV2624->pEffDuration = kmalloc(pDRV2624->effects_count * sizeof(u32), GFP_KERNEL);

				//pTmp = (unsigned char *)(pBuf) + sizeof(struct drv2624_fw_header);

				//memcpy(pDRV2624->pEffDuration, pTmp, pDRV2624->effects_count * sizeof(u32));

				//pTmp += pDRV2624->effects_count * sizeof(u32);

				drv2624_reg_write(pDRV2624,
						  DRV2624_REG_RAM_ADDR_UPPER,
						  0);
				drv2624_reg_write(pDRV2624,
						  DRV2624_REG_RAM_ADDR_LOWER,
						  0);
				fwsize =
				    size - sizeof(struct drv2624_fw_header);
				//    size - sizeof(struct drv2624_fw_header) - pDRV2624->effects_count * sizeof(u32);
				dev_err(pDRV2624->dev,
					"%s, firmwar fwsize = %d\n", __func__,
					fwsize);
				for (i = 0; i < fwsize; i++) {
					dev_err(pDRV2624->dev,
						"%s, firmware bytes pBuf[%03d]=0x%02x\n",
						__func__, i, pBuf[i]);
					drv2624_reg_write(pDRV2624,
							  DRV2624_REG_RAM_DATA,
							  pBuf[i]);
				}
				memset(&pDRV2624->mnFwRam[0], 0,
				       DRV2624_RAM_SIZE);
				memcpy(&pDRV2624->mnFwRam[0], pBuf, fwsize);
				for (i = 0;
				     i < pDRV2624->fw_header.fw_effCount; i++)
					pDRV2624->mnEffectTimems[i] =
					    drv2624_get_effect_timems(pDRV2624,
								      i);
			}
		}
	} else
		dev_err(pDRV2624->dev, "%s, ERROR!! firmware not found\n",
			__func__);
	release_firmware(fw);
	mutex_unlock(&pDRV2624->lock);
}

/**
* add load_rtp
*/

static void drv2624_load_rtp_1(const struct firmware *fw, void *context)
{
	struct drv2624_data *pDRV2624 = context;
	unsigned int size = 0;
	unsigned char *pBuf = NULL;
	int nResult = 0;
	mutex_lock(&pDRV2624->lock);
	if (fw != NULL) {
		size = fw->size;
		if (fw->size > MAX_RTP_FILE_SIZE) {
			//      pBuf = kzalloc(fw->size, GFP_KERNEL);
			dev_err(pDRV2624->dev,
				"%s, %u Error allocating memory\n", __func__,
				__LINE__);
			goto EXIT;
		}
		pBuf = rtp_data;
		memset(rtp_data, 0, sizeof(rtp_data));
		memcpy(pBuf, fw->data, fw->size);
		nResult = rtp_parse(pBuf, size);
		if (nResult < 0) {
			dev_err(pDRV2624->dev, "%s, rtp parse failed\n",
				__func__);
		}
		dev_err(pDRV2624->dev, "%s, size = %d\n", __func__, size);
	} else
		dev_err(pDRV2624->dev, "%s, ERROR!! firmware not found\n",
			__func__);
EXIT:
	mutex_unlock(&pDRV2624->lock);
	release_firmware(fw);
}

static int drv2624_file_open(struct inode *inode, struct file *file)
{
	if (!try_module_get(THIS_MODULE))
		return -ENODEV;
	file->private_data = (void *)g_DRV2624data;
	return 0;
}

static int drv2624_file_release(struct inode *inode, struct file *file)
{
	file->private_data = (void *)NULL;
	module_put(THIS_MODULE);
	return 0;
}

static long drv2624_file_unlocked_ioctl(struct file *file, unsigned int cmd,
					unsigned long arg)
{
	struct drv2624_data *pDRV2624 = file->private_data;

	int nResult = 0;
	mutex_lock(&pDRV2624->lock);
	dev_dbg(pDRV2624->dev, "ioctl 0x%x\n", cmd);
	switch (cmd) {
	}
	mutex_unlock(&pDRV2624->lock);
	return nResult;
}

static ssize_t drv2624_file_read(struct file *filp, char *buff,
				 size_t length, loff_t *offset)
{
	struct drv2624_data *pDRV2624 =
	    (struct drv2624_data *)filp->private_data;
	int nResult = 0;
	unsigned char value = 0;
	unsigned char *p_kBuf = NULL;
	mutex_lock(&pDRV2624->lock);
	switch (pDRV2624->mnFileCmd) {
	case HAPTIC_CMDID_REG_READ:
		if (length == 1) {
			nResult =
			    drv2624_reg_read(pDRV2624, pDRV2624->mnCurrentReg);
			if (nResult >= 0) {
				value = nResult;
				nResult = copy_to_user(buff, &value, 1);
				if (0 != nResult) {

					/* Failed to copy all the data, exit */
					dev_err(pDRV2624->dev,
						"copy to user fail %d\n",
						nResult);
				}
			}
		} else if (length > 1) {
			p_kBuf = (unsigned char *)kzalloc(length, GFP_KERNEL);
			if (p_kBuf != NULL) {
				nResult =
				    drv2624_bulk_read(pDRV2624,
						      pDRV2624->mnCurrentReg,
						      p_kBuf, length);
				if (nResult >= 0) {
					nResult =
					    copy_to_user(buff, p_kBuf, length);
					if (0 != nResult) {

						/* Failed to copy all the data, exit */
						dev_err(pDRV2624->dev,
							"copy to user fail %d\n",
							nResult);
					}
				}
				kfree(p_kBuf);
			} else {
				dev_err(pDRV2624->dev, "read no mem\n");
				nResult = -ENOMEM;
			}
		}
		break;
	case HAPTIC_CMDID_RUN_DIAG:
		if (pDRV2624->mnVibratorPlaying)
			length = 0;

		else {
			unsigned char buf[3];
			buf[0] = pDRV2624->mDiagResult.mnResult;
			buf[1] = pDRV2624->mDiagResult.mnDiagZ;
			buf[2] = pDRV2624->mDiagResult.mnDiagK;
			nResult = copy_to_user(buff, buf, 3);
			if (0 != nResult) {
				/* Failed to copy all the data, exit */
				dev_err(pDRV2624->dev,
					"copy to user fail %d\n", nResult);
			}
		}
		break;
	case HAPTIC_CMDID_RUN_CALIBRATION:
		if (pDRV2624->mnVibratorPlaying)
			length = 0;

		else {
			unsigned char buf[4];
			buf[0] = pDRV2624->mAutoCalResult.mnResult;
			buf[1] = pDRV2624->mAutoCalResult.mnCalComp;
			buf[2] = pDRV2624->mAutoCalResult.mnCalBemf;
			buf[3] = pDRV2624->mAutoCalResult.mnCalGain;
			nResult = copy_to_user(buff, buf, 4);
			if (0 != nResult) {

				/* Failed to copy all the data, exit */
				dev_err(pDRV2624->dev,
					"copy to user fail %d\n", nResult);
			}
		}
		break;
	default:
		pDRV2624->mnFileCmd = 0;
		break;
	}
	mutex_unlock(&pDRV2624->lock);
	return length;
}

static ssize_t drv2624_file_write(struct file *filp, const char *buff,
				  size_t len, loff_t *off)
{
	struct drv2624_data *pDRV2624 =
	    (struct drv2624_data *)filp->private_data;
	unsigned char *p_kBuf = NULL;
	int nResult = 0;
	mutex_lock(&pDRV2624->lock);
	p_kBuf = (unsigned char *)kzalloc(len, GFP_KERNEL);
	if (p_kBuf == NULL) {
		dev_err(pDRV2624->dev, "write no mem\n");
		goto err;
	}
	nResult = copy_from_user(p_kBuf, buff, len);
	if (0 != nResult) {
		dev_err(pDRV2624->dev, "copy_from_user failed.\n");
		goto err;
	}
	pDRV2624->mnFileCmd = p_kBuf[0];
	switch (pDRV2624->mnFileCmd) {
	case HAPTIC_CMDID_REG_READ:
		if (len == 2)
			pDRV2624->mnCurrentReg = p_kBuf[1];

		else
			dev_err(pDRV2624->dev, " read cmd len %lu err\n",
				(unsigned long)len);
		break;
	case HAPTIC_CMDID_REG_WRITE:
		if ((len - 1) == 2)
			nResult =
			    drv2624_reg_write(pDRV2624, p_kBuf[1], p_kBuf[2]);

		else if ((len - 1) > 2)
			nResult =
			    drv2624_bulk_write(pDRV2624, p_kBuf[1], &p_kBuf[2],
					       len - 2);

		else
			dev_err(pDRV2624->dev, "%s, reg_write len %lu error\n",
				__func__, (unsigned long)len);
		break;
	case HAPTIC_CMDID_RUN_DIAG:
		nResult = drv2624_stop(pDRV2624);
		if (nResult < 0)
			break;
		nResult = dev_run_diagnostics(pDRV2624);
		if ((nResult >= 0) && pDRV2624->mbIRQUsed)
			drv2624_enableIRQ(pDRV2624, NO);
		break;
	case HAPTIC_CMDID_RUN_CALIBRATION:
		nResult = drv2624_stop(pDRV2624);
		if (nResult < 0)
			break;
		nResult = dev_auto_calibrate(pDRV2624);
		if ((nResult >= 0) && pDRV2624->mbIRQUsed)
			drv2624_enableIRQ(pDRV2624, NO);
		break;
	case HAPTIC_SET_CALIBRATION_RESULT:
		drv2624_reg_write(pDRV2624, DRV2624_REG_CAL_COMP, p_kBuf[1]);
		drv2624_reg_write(pDRV2624, DRV2624_REG_CAL_BEMF, p_kBuf[2]);
		drv2624_reg_write(pDRV2624, DRV2624_REG_LOOP_CONTROL,
				  p_kBuf[3]);
		break;
	default:
		dev_err(pDRV2624->dev, "%s, unknown cmd\n", __func__);
		break;
	}
err:	if (p_kBuf != NULL)
		kfree(p_kBuf);
	mutex_unlock(&pDRV2624->lock);
	return len;
}

static void upload_periodic_work_routine(struct work_struct *work)
{
	struct drv2624_data *pDRV2624 =
	    container_of(work, struct drv2624_data, upload_periodic_work);
	int nResult = 0;
	dev_err(pDRV2624->dev, "%s:enter\n", __func__);
	mutex_lock(&pDRV2624->lock);
	//nResult = drv2624_stop(pDRV2624);
	if (nResult < 0) {
		return;
	}
	if ((pDRV2624->mnEffectType == FF_PERIODIC) &&
	    (pDRV2624->mnWorkMode == DRV2624_RAM_MODE)) {
		/**set ram mode register **/
		drv2624_set_ram_mode_reg(pDRV2624);
		//drv2624_calibration_fetch(pDRV2624);
		nResult =
		    drv2624_set_waveform(pDRV2624,
					 &pDRV2624->msWaveformSequencer);
		if (nResult < 0) {
			dev_err(pDRV2624->dev,
				"Configurate predefined effect %d failed, nResult=%d\n",
				pDRV2624->msWaveformSequencer.msWaveform[0].mnEffect, nResult);
			goto End;
		}
	} else if ((pDRV2624->mnEffectType == FF_PERIODIC) &&
		   (pDRV2624->mnWorkMode == DRV2624_NEW_RTP_MODE)) {
		//nResult = set_running_effect_id(22);
		if (nResult < 0) {
			dev_err(pDRV2624->dev, "%s:set effect_id failed\n",
				__func__);
			goto End;
		}

	} else {
		dev_err(pDRV2624->dev, "%s: effect_type(%d) not supported!\n",
			__func__, pDRV2624->mnEffectType);
	}
	/**set ram mode register
	drv2624_set_ram_mode_reg(pDRV2624);
	nResult =
	    drv2624_set_waveform(pDRV2624, &pDRV2624->msWaveformSequencer);
	if (nResult < 0)
		dev_err(pDRV2624->dev,
			 "Configurate predefined effect %d failed, nResult=%d\n",
			 pDRV2624->msWaveformSequencer.msWaveform[0].mnEffect,
			 nResult);
	nResult = set_running_effect_id(22);
	if (nResult < 0) {
		dev_err(pDRV2624->dev, "%s:set effect_id failed\n", __func__);
		goto End;
	}
	*/
	dev_dbg(pDRV2624->dev,
		"Configurate predefined effect success, effect=%d\n",
		pDRV2624->msWaveformSequencer.msWaveform[0].mnEffect);
End:
	mutex_unlock(&pDRV2624->lock);
}

static int drv2624_haptics_upload_effect(struct input_dev *dev,
					 struct ff_effect *effect,
					 struct ff_effect *old)
{
	struct drv2624_data *pDRV2624 = input_get_drvdata(dev);
	struct drv2624_constant_playinfo *play = &pDRV2624->play;
	u16 data[CUSTOM_DATA_LEN];
	int nResult = 0;
	ktime_t rem;
	s64 time_us;
	uint time_ms = 0;
	dev_err(pDRV2624->dev, "%s enter function \n", __func__);
	mutex_lock(&pDRV2624->lock);
	/* waiting last vibration to end */
	if (hrtimer_active(&pDRV2624->haptics_timer)) {
		rem = hrtimer_get_remaining(&pDRV2624->haptics_timer);
		time_us = ktime_to_us(rem);
		dev_info(pDRV2624->dev,
			 "%s: waiting for playing finished: %lld us\n",
			 __func__, time_us);
		usleep_range(time_us, time_us + 100);
	}
	pDRV2624->mnEffectType = effect->type;
	dev_err(pDRV2624->dev, "%s: mnEffectType: %d\n", __func__,
		pDRV2624->mnEffectType);
	switch (pDRV2624->mnEffectType) {
	case FF_CONSTANT:
		play->length = effect->replay.length;
		play->rtp_input = effect->u.constant.level;
		pDRV2624->mnWorkMode = DRV2624_RTP_MODE;
	/***1-5-1**/
		play->effect_id = 127;
		dev_err(pDRV2624->dev, "%s: length(%d), level(%d)\n",
			__func__, play->length, play->rtp_input);
		break;
	case FF_PERIODIC:

		if (effect->u.periodic.waveform != FF_CUSTOM) {
			dev_err(pDRV2624->dev,
				"Only accept custom waveforms\n");
			nResult = -EINVAL;
			break;
		}
		if (copy_from_user
		    (data, effect->u.periodic.custom_data,
		     sizeof(u16) * CUSTOM_DATA_LEN)) {
			nResult = -EFAULT;
			break;
		}
		/**1-5-2**/
		play->effect_id = data[CUSTOM_DATA_EFFECT_IDX] + 1;
		play->magnitude = effect->u.periodic.magnitude;
		dev_err(pDRV2624->dev, "%s: effect_id = %d, fw_effCount = %d\n",
			__func__, play->effect_id,
			pDRV2624->fw_header.fw_effCount);
		if (play->effect_id > 0
		    && play->effect_id <= pDRV2624->fw_header.fw_effCount) {
			pDRV2624->mnWorkMode = DRV2624_RAM_MODE;
			dev_err(pDRV2624->dev,
				"%s: effect_id = %d, magnitude = %d\n",
				__func__, play->effect_id, play->magnitude);
			if ((play->effect_id < 0)
			    || (play->effect_id >
				pDRV2624->fw_header.fw_effCount)) {
				dev_err(pDRV2624->dev,
					"%s: overflow effect_id = %d, max_effect_id = %d\n",
					__func__, play->effect_id,
					pDRV2624->fw_header.fw_effCount);
				nResult = -EINVAL;
				break;
			}
			dev_err(pDRV2624->dev,
				"%s upload to use effect,fw_effCount = %d\n",
				__func__, pDRV2624->fw_header.fw_effCount);
			memset(&pDRV2624->msWaveformSequencer, 0,
			       sizeof(struct drv2624_waveform_sequencer));
		/** +1 **/
			pDRV2624->msWaveformSequencer.msWaveform[0].mnEffect =
			    data[CUSTOM_DATA_EFFECT_IDX] + 1;
			pDRV2624->msWaveformSequencer.msWaveform[0].mnLoop = 0;
			pDRV2624->msWaveformSequencer.msWaveform[1].mnEffect =
			    0;
			pDRV2624->msWaveformSequencer.msWaveform[1].mnLoop = 0;
		/** +1 **/
			dev_err(pDRV2624->dev, "upload to use effect %d\n",
				data[CUSTOM_DATA_EFFECT_IDX] + 1);
		/**1-5-2**/
			time_ms = pDRV2624->mnEffectTimems[data[CUSTOM_DATA_EFFECT_IDX]];

			dev_err(pDRV2624->dev, "effect playing time_ms %d\n",
				time_ms);
			data[CUSTOM_DATA_TIMEOUT_SEC_IDX] =
			    time_ms / MSEC_PER_SEC;
			dev_err(pDRV2624->dev, " ram data[1] time_ms =%d\n",
				data[CUSTOM_DATA_TIMEOUT_SEC_IDX]);
			data[CUSTOM_DATA_TIMEOUT_MSEC_IDX] =
			    time_ms % MSEC_PER_SEC;;
			dev_err(pDRV2624->dev, " ram data[2] time_ms =%d\n",
				data[CUSTOM_DATA_TIMEOUT_MSEC_IDX]);

			/*
			 * Copy the custom data contains the play length back to
			 * userspace so that the userspace client can wait and
			 * send stop playing command after it's done.
			 */
			if (copy_to_user
			    (effect->u.periodic.custom_data, data,
			     sizeof(u16) * CUSTOM_DATA_LEN)) {
				pr_info("%s copy to user failed \n", __func__);
				nResult = -EFAULT;
				break;
			}
			schedule_work(&pDRV2624->upload_periodic_work);
			break;
		} else if (play->effect_id > pDRV2624->fw_header.fw_effCount
			   && play->effect_id < 127) {
			pDRV2624->mnWorkMode = DRV2624_NEW_RTP_MODE;
			time_ms = set_running_effect_id(play->effect_id - 1);	//test
			data[CUSTOM_DATA_TIMEOUT_SEC_IDX] = time_ms / MSEC_PER_SEC;	//300 / 1000;
			dev_err(pDRV2624->dev, " rtp data[1] time_ms =%d\n",
				data[CUSTOM_DATA_TIMEOUT_SEC_IDX]);
			data[CUSTOM_DATA_TIMEOUT_MSEC_IDX] = time_ms % MSEC_PER_SEC;	//300;
			dev_err(pDRV2624->dev, " rtp data[2] time_ms =%d\n",
				data[CUSTOM_DATA_TIMEOUT_MSEC_IDX]);
			if (copy_to_user
			    (effect->u.periodic.custom_data, data,
			     sizeof(u16) * CUSTOM_DATA_LEN)) {
				pr_info("%s copy to user failed \n", __func__);
				nResult = -EFAULT;
				break;
			}
			schedule_work(&pDRV2624->upload_periodic_work);
			break;
		}
		break;
	default:
		dev_err(pDRV2624->dev, "Unsupported effect type: %d\n",
			effect->type);
		break;
	}
	mutex_unlock(&pDRV2624->lock);
	dev_info(pDRV2624->dev, "%s exit \n", __func__);
	return nResult;
}

static void haptics_playback_work_routine(struct work_struct *work)
{
	int nResult = 0;
	struct drv2624_data *pDRV2624 =
	    container_of(work, struct drv2624_data, haptics_playback_work);
	mutex_lock(&pDRV2624->lock);

	nResult = drv2624_stop(pDRV2624);
	if (nResult < 0) {
		dev_err(pDRV2624->dev, "%s: stop failed!\n", __func__);
		goto end;
	}
	dev_err(pDRV2624->dev, "%s:pDRV2624->type = %d\n", __func__,
		pDRV2624->mnEffectType);
	switch (pDRV2624->mnEffectType) {
	case FF_PERIODIC:
		dev_err(pDRV2624->dev, "%s:FF_PERIODIC enter\n", __func__);
		if (pDRV2624->mnWorkMode == DRV2624_RAM_MODE) {
			nResult = drv2624_playEffect(pDRV2624);
			if ((nResult >= 0) && pDRV2624->mbIRQUsed) {
				drv2624_enableIRQ(pDRV2624, NO);
				break;
			}
		} else if (pDRV2624->mnWorkMode == DRV2624_NEW_RTP_MODE) {
			pDRV2624->play.length = 0;
			pDRV2624->mnWorkMode = DRV2624_RTP_MODE;
			nResult =
			    drv2624_change_mode(pDRV2624, DRV2624_RTP_MODE);
			dev_err(pDRV2624->dev, "%s:FF_PERIODIC mode = 0x%x\n",
				__func__, pDRV2624->mnWorkMode);
			//logtime vibration unlock to allow stopping
			mutex_unlock(&pDRV2624->lock);
			get_command_str(pDRV2624);
			goto exit;
		}
	case FF_CONSTANT:
		dev_err(pDRV2624->dev, "%s enter effect.length(%d) \n",
			__func__, pDRV2624->play.length);
		if (pDRV2624->play.length <= 0) {
			goto end;
		}
		nResult = drv2624_change_mode(pDRV2624, DRV2624_RTP_MODE);
		if (nResult < 0) {
			dev_dbg(pDRV2624->dev,
				"%s: change_mode nResult = %d\n", __func__,
				nResult);
			break;
		}
		nResult =
		    drv2624_reg_write(pDRV2624, DRV2624_REG_RTP_INPUT, 0x7f);
		if (nResult < 0)
			break;
		nResult = drv2624_set_go_bit(pDRV2624, GO);
		if (nResult < 0)
			break;
		pDRV2624->mnVibratorPlaying = YES;
		if (pDRV2624->mbIRQUsed)
			nResult = drv2624_enableIRQ(pDRV2624, YES);
		break;
	default:
		dev_err(pDRV2624->dev, "Unsupported effect type: %d\n",
			pDRV2624->mnEffectType);
		break;
	}
	if (pDRV2624->play.length != 0) {
		hrtimer_start(&pDRV2624->haptics_timer,
			      ns_to_ktime((u64) pDRV2624->play.length *
					  NSEC_PER_MSEC), HRTIMER_MODE_REL);
	}
end:	mutex_unlock(&pDRV2624->lock);
exit: 	dev_dbg(pDRV2624->dev, "%s: exit\n", __func__);
}

static int drv2624_haptics_playback(struct input_dev *dev, int effect_id,
				    int val)
{
	struct drv2624_data *pDRV2624 = input_get_drvdata(dev);
	int nResult = 0;
	dev_err(pDRV2624->dev, "%s: mnEffectType(%d) WorkMode(%d)\n",
		__func__, pDRV2624->mnEffectType, pDRV2624->mnWorkMode);
	if ((pDRV2624->mnEffectType == FF_CONSTANT)
	    && (pDRV2624->mnWorkMode == DRV2624_RTP_MODE)) {
		pDRV2624->mnWorkMode = DRV2624_RTP_MODE;
		if (hrtimer_active(&pDRV2624->haptics_timer))
			hrtimer_cancel(&pDRV2624->haptics_timer);
		schedule_work(&pDRV2624->haptics_playback_work);
	} else if ((pDRV2624->mnEffectType == FF_PERIODIC) &&
		   (pDRV2624->mnWorkMode == DRV2624_RAM_MODE)) {
		schedule_work(&pDRV2624->haptics_playback_work);
	} else if ((pDRV2624->mnEffectType == FF_PERIODIC) &&
		   (pDRV2624->mnWorkMode == DRV2624_NEW_RTP_MODE)) {
		schedule_work(&pDRV2624->haptics_playback_work);
	} else {
		dev_err(pDRV2624->dev, "%s: effect_type(%d) not supported!\n",
			__func__, pDRV2624->mnEffectType);
	}
	return nResult;
}

static int drv2624_haptics_erase(struct input_dev *dev, int effect_id)
{
	struct drv2624_data *pDRV2624 = input_get_drvdata(dev);
	struct drv2624_constant_playinfo *play = &pDRV2624->play;
	int nResult = 0;
	dev_err(pDRV2624->dev, "%s enter\n", __func__);
	mutex_lock(&pDRV2624->lock);
	play->length = 0;
	pDRV2624->mnEffectType = 0;
	nResult = drv2624_stop(pDRV2624);
	if (nResult < 0)
		dev_err(pDRV2624->dev, "failed to stop vibrator: %d\n",
			nResult);
	mutex_unlock(&pDRV2624->lock);
	return nResult;
}

/* NOTE: gain range: 0 ~ 0x7f*/
static void drv2624_haptics_set_gain(struct input_dev *dev, u16 gain)
{
	struct drv2624_data *pDRV2624 = input_get_drvdata(dev);
#if 0
	struct drv2624_constant_playinfo *play = &pDRV2624->play;
	if (gain == 0)
		return;

	else if (gain > 0x7f)
		gain = 0x7f;
	/*
	   play->rtp_input =
	   (gain - LIGHT_MAGNITUDE) * 255 / (STRONG_MAGNITUDE -
	   LIGHT_MAGNITUDE);
	 */
	play->rtp_input = gain;
	dev_dbg(pDRV2624->dev, "upload constant effect, rtp_input=%d\n",
		play->rtp_input);
	dev_dbg(pDRV2624->dev, "enter drv264_reg_write rtp_input\n");
	//schedule_work(&pDRV2624->haptics_set_gain_work);
#endif
	dev_err(pDRV2624->dev, "%s: enter\n", __func__);

}

static struct file_operations fops = {
	.owner = THIS_MODULE,.read = drv2624_file_read,.write =
	    drv2624_file_write,.unlocked_ioctl =
	    drv2624_file_unlocked_ioctl,.open = drv2624_file_open,.release =
	    drv2624_file_release,
};

static struct miscdevice drv2624_misc = {
	.minor = MISC_DYNAMIC_MINOR,.name = HAPTICS_DEVICE_NAME,.fops = &fops,
};

static int Haptics_init(struct drv2624_data *pDRV2624)
{
	int nResult = 0;

#ifdef ANDROID_TIMED_OUTPUT
	pDRV2624->to_dev.name = "vibrator";
	pDRV2624->to_dev.get_time = vibrator_get_time;
	pDRV2624->to_dev.enable = vibrator_enable;
	nResult = timed_output_dev_register(&(pDRV2624->to_dev));
	if (nResult < 0) {
		dev_err(pDRV2624->dev,
			"drv2624: fail to create timed output dev\n");
		return nResult;
	}
#endif /* ANDROID_TIMED_OUTPUT */
	nResult = misc_register(&drv2624_misc);
	if (nResult) {
		dev_err(pDRV2624->dev, "drv2624 misc fail: %d\n", nResult);
		return nResult;
	}
	/*
	   nResult = device_create_file(pDRV2624->dev, &dev_attr_cali);
	   if (nResult < 0) {
	   dev_info(pDRV2624->dev,
	   "sys file creation failed nResult: %d\n", nResult);
	   return nResult;
	   }
	 */

	hrtimer_init(&pDRV2624->haptics_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	pDRV2624->haptics_timer.function = vibrator_timer_func;
#if 0
	hrtimer_init(&pDRV2624->stop_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pDRV2624->stop_timer.function = vibrator_rtp_stop_timer;
#endif
	INIT_WORK(&pDRV2624->vibrator_work, vibrator_work_routine);
	INIT_WORK(&pDRV2624->upload_periodic_work,
		  upload_periodic_work_routine);
	INIT_WORK(&pDRV2624->haptics_playback_work,
		  haptics_playback_work_routine);
	//INIT_WORK(&pDRV2624->haptics_set_gain_work,
	//         haptics_set_gain_work_routine);
	mutex_init(&pDRV2624->lock);
	return 0;
}

static void drv2624_init(struct drv2624_data *pDRV2624)
{
	struct drv2624_platform_data *pDrv2624Platdata = &pDRV2624->msPlatData;
	struct actuator_data actuator = pDrv2624Platdata->msActuator;
	unsigned char value_temp = 0;
	unsigned char mask_temp = 0;
	struct drv2624_wave_setting wavesetting;
	unsigned char value = 0;
	drv2624_set_bits(pDRV2624, DRV2624_REG_MODE, PINFUNC_MASK,
			 (PINFUNC_INT << PINFUNC_SHIFT));
	if ((actuator.mnActuatorType == ERM) || (actuator.mnActuatorType ==
						 LRA)) {
		mask_temp |= ACTUATOR_MASK;
		value_temp |= (actuator.mnActuatorType << ACTUATOR_SHIFT);
	}
	if ((pDrv2624Platdata->mnLoop == CLOSE_LOOP)
	    || (pDrv2624Platdata->mnLoop == OPEN_LOOP)) {
		mask_temp |= LOOP_MASK;
		value_temp |= (pDrv2624Platdata->mnLoop << LOOP_SHIFT);
	}
	drv2624_set_bits(pDRV2624, DRV2624_REG_CONTROL1,
			 mask_temp | AUTOBRK_OK_MASK,
			 value_temp | AUTOBRK_OK_ENABLE);
	value_temp = 0;
	if (actuator.mnActuatorType == ERM)
		value_temp = LIB_ERM;

	else if (actuator.mnActuatorType == LRA)
		value_temp = LIB_LRA;
	if (value_temp != 0)
		drv2624_set_bits(pDRV2624, DRV2624_REG_CONTROL2, LIB_MASK,
				 value_temp << LIB_SHIFT);
	if (actuator.mnRatedVoltage != 0)
		drv2624_reg_write(pDRV2624, DRV2624_REG_RATED_VOLTAGE,
				  actuator.mnRatedVoltage);

	else
		dev_err(pDRV2624->dev, "%s, ERROR Rated ZERO\n", __func__);
	if (actuator.mnOverDriveClampVoltage != 0)
		drv2624_reg_write(pDRV2624, DRV2624_REG_OVERDRIVE_CLAMP,
				  actuator.mnOverDriveClampVoltage);

	else
		dev_err(pDRV2624->dev, "%s, ERROR OverDriveVol ZERO\n",
			__func__);
	if (actuator.mnActuatorType == LRA) {
		unsigned char DriveTime =
		    5 * (1000 - actuator.mnLRAFreq) / actuator.mnLRAFreq;
		unsigned short openLoopPeriod =
		    (unsigned short)((unsigned int)1000000000 /
				     (24619 * actuator.mnLRAFreq));
		if (actuator.mnLRAFreq < 125)
			DriveTime |= (MINFREQ_SEL_45HZ << MINFREQ_SEL_SHIFT);
		drv2624_set_bits(pDRV2624, DRV2624_REG_DRIVE_TIME,
				 DRIVE_TIME_MASK | MINFREQ_SEL_MASK, DriveTime);
		drv2624_set_bits(pDRV2624, DRV2624_REG_OL_PERIOD_H, 0x03,
				 (openLoopPeriod & 0x0300) >> 8);
		drv2624_reg_write(pDRV2624, DRV2624_REG_OL_PERIOD_L,
				  (openLoopPeriod & 0x00ff));
		dev_info(pDRV2624->dev, "%s, LRA = %d, DriveTime=0x%x\n",
			 __func__, actuator.mnLRAFreq, DriveTime);
	}
	value = drv2624_reg_read(pDRV2624, DRV2624_REG_CONTROL2);
	wavesetting.mnLoop =
	    drv2624_reg_read(pDRV2624, DRV2624_REG_MAIN_LOOP) & 0x07;
	wavesetting.mnInterval = ((value & INTERVAL_MASK) >> INTERVAL_SHIFT);
	wavesetting.mnScale = (value & SCALE_MASK);
	memcpy(&pDRV2624->msWaveformSetting, &wavesetting,
	       sizeof(struct drv2624_wave_setting));
}

static irqreturn_t drv2624_irq_handler(int irq, void *dev_id)
{
	struct drv2624_data *pDRV2624 = (struct drv2624_data *)dev_id;
	dev_err(pDRV2624->dev, "%s: enter\n", __func__);
	pDRV2624->mnWorkMode |= WORK_IRQ;
	schedule_work(&pDRV2624->vibrator_work);
	return IRQ_HANDLED;
}

static int drv2624_parse_dt(struct device *dev, struct drv2624_data *pDRV2624)
{
	struct device_node *np = dev->of_node;
	struct drv2624_platform_data *pPlatData = &pDRV2624->msPlatData;
	int rc = 0, nResult = 0;
	unsigned int value;
	pPlatData->mnGpioNRST = of_get_named_gpio(np, "ti,reset-gpio", 0);
	if (pPlatData->mnGpioNRST < 0) {
		dev_err(pDRV2624->dev,
			"Looking up %s property in node %s failed %d\n",
			"ti,reset-gpio", np->full_name, pPlatData->mnGpioNRST);
		return -EINVAL;
	} else
		dev_dbg(pDRV2624->dev, "ti,reset-gpio=%d\n",
			pPlatData->mnGpioNRST);
	rc = of_property_read_u32(np, "ti,smart-loop", &value);
	if (rc) {
		dev_err(pDRV2624->dev,
			"Looking up %s property in node %s failed %d\n",
			"ti,smart-loop", np->full_name, rc);
		return -EINVAL;
	} else {
		pPlatData->mnLoop = value & 0x01;
		dev_dbg(pDRV2624->dev, "ti,smart-loop=%d\n", pPlatData->mnLoop);
	}
	rc = of_property_read_u32(np, "ti,actuator", &value);
	if (rc) {
		dev_err(pDRV2624->dev,
			"Looking up %s property in node %s failed %d\n",
			"ti,actuator", np->full_name, rc);
		return -EINVAL;
	} else {
		pPlatData->msActuator.mnActuatorType = value & 0x01;
		dev_dbg(pDRV2624->dev, "ti,actuator=%d\n",
			pPlatData->msActuator.mnActuatorType);
	}
	rc = of_property_read_u32(np, "ti,rated-voltage", &value);
	if (rc) {
		dev_err(pDRV2624->dev,
			"Looking up %s property in node %s failed %d\n",
			"ti,rated-voltage", np->full_name, rc);
		return -EINVAL;
	} else {
		pPlatData->msActuator.mnRatedVoltage =
		    drv2624_calculate_voltage(value);
		dev_dbg(pDRV2624->dev, "ti,rated-voltage=0x%x\n",
			pPlatData->msActuator.mnRatedVoltage);
	}
	rc = of_property_read_u32(np, "ti,odclamp-voltage", &value);
	if (rc) {
		dev_err(pDRV2624->dev,
			"Looking up %s property in node %s failed %d\n",
			"ti,odclamp-voltage", np->full_name, rc);
		return -EINVAL;
	} else {
		pPlatData->msActuator.mnOverDriveClampVoltage =
		    drv2624_calculate_voltage(value);
		dev_dbg(pDRV2624->dev, "ti,odclamp-voltage=0x%x\n",
			pPlatData->msActuator.mnOverDriveClampVoltage);
	}
	if (pPlatData->msActuator.mnActuatorType == LRA) {
		rc = of_property_read_u32(np, "ti,lra-frequency", &value);
		if (rc) {
			dev_err(pDRV2624->dev,
				"Looking up %s property in node %s failed %d\n",
				"ti,lra-frequency", np->full_name, rc);
			return -EINVAL;
		} else {
			if ((value >= 45) && (value <= 300)) {
				pPlatData->msActuator.mnLRAFreq = value;
				dev_dbg(pDRV2624->dev, "ti,lra-frequency=%d\n",
					pPlatData->msActuator.mnLRAFreq);
			} else {
				dev_err(pDRV2624->dev,
					"ERROR, ti,lra-frequency=%d, out of range\n",
					pPlatData->msActuator.mnLRAFreq);
				return -EINVAL;
			}
		}
	}
	pPlatData->mnGpioINT = of_get_named_gpio(np, "ti,irq-gpio", 0);
	if (pPlatData->mnGpioINT < 0) {
		dev_err(pDRV2624->dev,
			"Looking up %s property in node %s failed %d\n",
			"ti,irq-gpio", np->full_name, pPlatData->mnGpioINT);
		return -EINVAL;
	} else
		dev_dbg(pDRV2624->dev, "ti,irq-gpio=%d\n",
			pPlatData->mnGpioINT);
	return nResult;
}

static void drv2624_close(struct input_dev *input)
{
	struct drv2624_data *pDRV2624 = input_get_drvdata(input);
	dev_dbg(pDRV2624->dev, "%s\n", __func__);
	mutex_lock(&pDRV2624->lock);
	if (hrtimer_active(&pDRV2624->haptics_timer)
	    || pDRV2624->mnVibratorPlaying)
		drv2624_stop(pDRV2624);
	mutex_unlock(&pDRV2624->lock);
	return;
}

/**
 * Rated and Overdriver Voltages:
 * Calculated using the formula r = voltage(V) * 255 / 5.6
 * where r is what will be written to the register
 * and v is the rated or overdriver voltage of the actuator
 **/
static inline int drv2624_calculate_voltage(unsigned int voltage)
{
	return (voltage * 255 / 5600);
}

static int drv2624_hw_reset(struct drv2624_data *pDRV2624)
{
	int nResult = 0;
	dev_dbg(pDRV2624->dev, "%s: enter! \n", __func__);
	gpio_direction_output(pDRV2624->msPlatData.mnGpioNRST, 0);
	mdelay(5);
	gpio_direction_output(pDRV2624->msPlatData.mnGpioNRST, 1);
	mdelay(2);
	return nResult;
}

static int drv2624_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	int nResult = 0;
	struct drv2624_data *pDRV2624;
	struct ff_device *ff;
	dev_info(&client->dev, "%s enter\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "%s:I2C check failed\n", __func__);
		return -ENODEV;
	}
	pDRV2624 =
	    devm_kzalloc(&client->dev, sizeof(struct drv2624_data), GFP_KERNEL);
	if (pDRV2624 == NULL) {
		dev_err(&client->dev, "%s:no memory\n", __func__);
		return -ENOMEM;
	}
	pDRV2624->dev = &client->dev;
	pDRV2624->client = client;
	i2c_set_clientdata(client, pDRV2624);
	dev_set_drvdata(&client->dev, pDRV2624);
	pDRV2624->mpRegmap = devm_regmap_init_i2c(client, &drv2624_i2c_regmap);
	if (IS_ERR(pDRV2624->mpRegmap)) {
		nResult = PTR_ERR(pDRV2624->mpRegmap);
		dev_err(pDRV2624->dev,
			"%s:Failed to allocate register map: %d\n", __func__,
			nResult);
		goto free_mem;
	}
	if (client->dev.of_node) {
		dev_dbg(pDRV2624->dev, "of node parse\n");
		nResult = drv2624_parse_dt(&client->dev, pDRV2624);
		if (nResult < 0) {
			dev_err(pDRV2624->dev, "%s: parse_dt failed %d\n",
				__func__, nResult);
			goto free_gpio;
		}
	} else if (client->dev.platform_data) {
		dev_dbg(pDRV2624->dev, "platform data parse\n");
		memcpy(&pDRV2624->msPlatData, client->dev.platform_data,
		       sizeof(struct drv2624_platform_data));
	} else {
		dev_err(pDRV2624->dev, "%s: ERROR no platform data\n",
			__func__);
		goto free_gpio;
	}
	if (gpio_is_valid(pDRV2624->msPlatData.mnGpioNRST)) {
		nResult =
		    gpio_request(pDRV2624->msPlatData.mnGpioNRST,
				 "DRV2624-NRST");
		if (nResult < 0) {
			dev_err(pDRV2624->dev,
				"%s: GPIO %d request NRST error\n", __func__,
				pDRV2624->msPlatData.mnGpioNRST);
			goto free_gpio;
		}
		drv2624_hw_reset(pDRV2624);
	}
	mutex_init(&pDRV2624->dev_lock);
	nResult = drv2624_reg_read(pDRV2624, DRV2624_REG_ID);
	if (nResult != 0x3)
		goto exit_gpio_request_failed1;

	else {
		dev_info(pDRV2624->dev, "%s, ID status (0x%x)\n", __func__,
			 nResult);
		pDRV2624->mnDeviceID = nResult;
	}
	if ((pDRV2624->mnDeviceID & 0xf0) != DRV2624_ID) {
		dev_err(pDRV2624->dev, "%s, device_id(0x%x) fail\n", __func__,
			pDRV2624->mnDeviceID);
		goto destroy_mutex;
	}
	drv2624_init(pDRV2624);
	if (gpio_is_valid(pDRV2624->msPlatData.mnGpioINT)) {
		nResult =
		    gpio_request(pDRV2624->msPlatData.mnGpioINT, "DRV2624-IRQ");
		if (nResult < 0) {
			dev_err(pDRV2624->dev,
				"%s: GPIO %d request INT error\n", __func__,
				pDRV2624->msPlatData.mnGpioINT);
			goto destroy_mutex;
		}
		gpio_direction_input(pDRV2624->msPlatData.mnGpioINT);
		pDRV2624->mnIRQ = gpio_to_irq(pDRV2624->msPlatData.mnGpioINT);
		dev_dbg(pDRV2624->dev, "irq = %d \n", pDRV2624->mnIRQ);
		nResult =
		    request_threaded_irq(pDRV2624->mnIRQ, drv2624_irq_handler,
					 NULL,
					 IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					 client->name, pDRV2624);
		if (nResult < 0) {
			dev_err(pDRV2624->dev, "request_irq failed, %d\n",
				nResult);
			goto destroy_mutex;
		}
		disable_irq_nosync(pDRV2624->mnIRQ);
		pDRV2624->mbIRQEnabled = false;
		pDRV2624->mbIRQUsed = true;
	} else
		pDRV2624->mbIRQUsed = false;
	pDRV2624->input_dev = devm_input_allocate_device(&client->dev);
	if (!pDRV2624->input_dev) {
		dev_err(&client->dev, "Failed to allocate input device_node\n");
		goto destroy_mutex;
	}
	pDRV2624->input_dev->name = "drv2624:haptics";
	pDRV2624->input_dev->close = drv2624_close;
	input_set_drvdata(pDRV2624->input_dev, pDRV2624);
	input_set_capability(pDRV2624->input_dev, EV_FF, FF_RUMBLE);
	input_set_capability(pDRV2624->input_dev, EV_FF, FF_CONSTANT);
	input_set_capability(pDRV2624->input_dev, EV_FF, FF_GAIN);
	input_set_capability(pDRV2624->input_dev, EV_FF, FF_PERIODIC);
	input_set_capability(pDRV2624->input_dev, EV_FF, FF_CUSTOM);
	nResult = input_ff_create(pDRV2624->input_dev, EFFECT_MAX_NUM);
	/*
	   nResult = device_create_file(pDRV2624->dev, &dev_attr_enable);
	   nResult = device_create_file(pDRV2624->dev, &dev_attr_NAME);
	   nResult = device_create_file(pDRV2624->dev, &dev_attr_Reg_wr);
	 */
	if (nResult) {
		dev_err(&client->dev, "input_ff_create() failed: %d\n",
			nResult);
		goto destroy_mutex;
	}
	sysfs_create_group(&pDRV2624->dev->kobj,
			   &drv2624_vibrator_attribute_group);
	ff = pDRV2624->input_dev->ff;
	ff->upload = drv2624_haptics_upload_effect;
	ff->playback = drv2624_haptics_playback;
	ff->erase = drv2624_haptics_erase;
	ff->set_gain = drv2624_haptics_set_gain;
	nResult = input_register_device(pDRV2624->input_dev);
	if (nResult) {
		dev_err(&client->dev, "couldn't register input device: %d\n",
			nResult);
		goto destroy_ff;
	}
	Haptics_init(pDRV2624);
	g_DRV2624data = pDRV2624;
	nResult = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
					  "drv2624.bin", &(client->dev),
					  GFP_KERNEL, pDRV2624,
					  drv2624_firmware_load);
	if (nResult != 0) {
		dev_err(&client->dev,
			"%s: %u: nResult = %d: %s request drv2624_firmware_laod!\n",
			__func__, __LINE__, nResult);
	}
	/** add **/
	nResult = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
					  RTP_BIN_NAME, &(client->dev),
					  GFP_KERNEL, pDRV2624,
					  drv2624_load_rtp_1);
	if (nResult != 0) {
		dev_err(&client->dev,
			"%s: %u: nResult = %d: %s request drv2624_load_rtp_1!\n",
			__func__, __LINE__, nResult, RTP_BIN_NAME);
	}

#if 0
	INIT_DELAYED_WORK(&pDRV2624->cali_write_work,
			  drv2624_init_write_cali_data);
	pDRV2624->cali_write_workqueue =
	    create_workqueue("drv2624_probe_cali_data");
	if (pDRV2624->cali_write_workqueue == NULL)
		dev_err(&client->dev,
			"can't create pDRV2624 cali_write_workqueue\n");

	queue_delayed_work(pDRV2624->cali_write_workqueue,
			   &pDRV2624->cali_write_work,
			   msecs_to_jiffies(25 * 1000));
#endif

	dev_info(pDRV2624->dev, "drv2624 probe succeeded\n");
	return 0;
destroy_ff:input_ff_destroy(pDRV2624->input_dev);
destroy_mutex:mutex_destroy(&pDRV2624->dev_lock);
free_gpio:
	if (gpio_is_valid(pDRV2624->msPlatData.mnGpioINT))
		gpio_free(pDRV2624->msPlatData.mnGpioINT);
	if (gpio_is_valid(pDRV2624->msPlatData.mnGpioNRST))
		gpio_free(pDRV2624->msPlatData.mnGpioNRST);
free_mem:
	if (NULL != pDRV2624)
		kfree(pDRV2624);
exit_gpio_request_failed1:if (gpio_is_valid
	    (pDRV2624->msPlatData.mnGpioNRST))
		gpio_free(pDRV2624->msPlatData.mnGpioNRST);
	mutex_destroy(&pDRV2624->dev_lock);
	return nResult;
}

static int drv2624_i2c_remove(struct i2c_client *client)
{
	struct drv2624_data *pDRV2624 = i2c_get_clientdata(client);

	//cancel_delayed_work_sync(&pDRV2624->cali_write_work);

	if (pDRV2624->msPlatData.mnGpioNRST)
		gpio_free(pDRV2624->msPlatData.mnGpioNRST);
	if (pDRV2624->msPlatData.mnGpioINT)
		gpio_free(pDRV2624->msPlatData.mnGpioINT);
	misc_deregister(&drv2624_misc);
	input_ff_destroy(pDRV2624->input_dev);
	mutex_destroy(&pDRV2624->lock);
	mutex_destroy(&pDRV2624->dev_lock);
	return 0;
}
static const struct i2c_device_id drv2624_i2c_id[] = { {"drv2624", 0}, {}
};

#ifdef CONFIG_PM_SLEEP
static int __maybe_unused drv2624_suspend(struct device *dev)
{
	struct drv2624_data *pDRV2624 = dev_get_drvdata(dev);
	dev_dbg(pDRV2624->dev, "%s enter!\n", __func__);
	mutex_lock(&pDRV2624->lock);
	if (hrtimer_active(&pDRV2624->haptics_timer)
	    || pDRV2624->mnVibratorPlaying) {
		drv2624_stop(pDRV2624);
	}

	/* set device to standby mode */
	drv2624_set_bits(pDRV2624, DRV2624_REG_CONTROL1,
			 DRV2624_AUTO_BRK_INTO_STBY_MASK,
			 DRV2624_REMOVE_STBY_MODE
			 /*DRV2624_STBY_MODE_WITH_AUTO_BRAKE */ );
	mutex_unlock(&pDRV2624->lock);
	return 0;
}

static int __maybe_unused drv2624_resume(struct device *dev)
{
	struct drv2624_data *pDRV2624 = dev_get_drvdata(dev);
	dev_err(pDRV2624->dev, "%s enter!\n", __func__);
	mutex_lock(&pDRV2624->lock);

	/*** 1-7-1 set device to active mode
	    drv2624_reg_write(pDRV2624, DRV2624_REG_CONTROL1,
			      DRV2624_REMOVE_STBY_MODE);
	*/
#if 0
	drv2624_reg_write(pDRV2624, DRV2624_REG_CONTROL1, 0x98);
	mutex_unlock(&pDRV2624->lock);
	return 0;
#endif
	drv2624_set_bits(pDRV2624, DRV2624_REG_CONTROL1,
			 DRV2624_AUTO_BRK_INTO_STBY_MASK,
			 DRV2624_STBY_MODE_WITH_AUTO_BRAKE
			 /*DRV2624_REMOVE_STBY_MODE */ );
	drv2624_calibration_fetch(pDRV2624);
	mutex_unlock(&pDRV2624->lock);
	return 0;
}

static SIMPLE_DEV_PM_OPS(drv2624_pm_ops, drv2624_suspend, drv2624_resume);

#endif /*  */
MODULE_DEVICE_TABLE(i2c, drv2624_i2c_id);

#if defined(CONFIG_OF)
static const struct of_device_id drv2624_of_match[] = {
	{.compatible = "ti,drv2624"}, {},
};

MODULE_DEVICE_TABLE(of, drv2624_of_match);

#endif /*  */
static struct i2c_driver drv2624_i2c_driver = {
	.driver = {
		   .name = "drv2624",
		   .owner = THIS_MODULE,
#ifdef CONFIG_PM_SLEEP
		   .pm = &drv2624_pm_ops,
#endif /*  */
#if defined(CONFIG_OF)
		   .of_match_table = of_match_ptr(drv2624_of_match),

#endif /*  */
		   },
	.probe = drv2624_i2c_probe,
	.remove = drv2624_i2c_remove,
	.id_table = drv2624_i2c_id,
};

module_i2c_driver(drv2624_i2c_driver);
MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("DRV2624 I2C Smart Haptics driver");
MODULE_LICENSE("GPL");
