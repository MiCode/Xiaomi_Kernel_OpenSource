/*
** =============================================================================
** Copyright (c) 2014  Texas Instruments Inc.
** Copyright (C) 2016 XiaoMi, Inc.
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
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
** File:
**     drv2604.c
**
** Description:
**     DRV2604 chip driver
**
** =============================================================================
*/
#include <linux/init.h>
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
#include <linux/jiffies.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/firmware.h>
#include <drv2604.h>

static struct drv2604_platform_data  drv2604_plat_data = {
	.GpioEnable = 0,
	.GpioTrigger = 0,

	.loop = CLOSE_LOOP,
	.RTPFormat = Signed,
	.BIDIRInput = BiDirectional,
	.actuator = {
		.device_type = LRA,
		.rated_vol = 0x30,/*0x30 is 1.2Vrms,but rated_vol <=over_drive_vol=0.9Vrms.*/
		.over_drive_vol = 0x40,/*decrease  the overdrive voltage to 0.9Vrms*/
		.LRAFreq = 240,
	},
};



static const unsigned char effect[] = {
	0x00,
	0x00, 0x0a, 0x04, 0x00, 0x0e, 0x04, 0x00, 0x12, 0x04,
	0x1a, 0x03, 0x4f, 0x07,
	0x3f, 0x03, 0x4f, 0x07,
	0x2f, 0x02, 0x4f, 0x04
};


static struct drv2604_data *pDRV2604data;

static int drv2604_reg_read(struct drv2604_data *pDrv2604data, unsigned int reg)
{
	unsigned int val;
	int ret;

	ret = regmap_read(pDrv2604data->regmap, reg, &val);

	if (ret < 0)
		return ret;
	else
		return val;
}

static ssize_t waveform_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct drv2604_data *pDrv2604data = container_of(timed_dev, struct drv2604_data,
					 to_dev);
	short arry[8] = {};
	int save_count = 0;
	int i;
	pr_debug("%s,%d \n", buf, (int)count);
	save_count = sscanf(buf, "%hd %hd %hd %hd %hd %hd %hd %hd",
													arry, arry+1, arry+2, arry+3, arry+4, arry+5, arry+6, arry+7);
	for (i = 0; i < 8; i++)
		pDrv2604data->sequence[i] = (unsigned char)arry[i];
	return count;
}

static ssize_t waveform_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *timed_dev = dev_get_drvdata(dev);
	struct drv2604_data *pDrv2604data = container_of(timed_dev, struct drv2604_data,
					 to_dev);
	int count = 0, i;

	for (i = 0; i < 8 ; i++) {
		count += snprintf(buf + count, PAGE_SIZE - count,
				"%hd  ", (short)pDrv2604data->sequence[i]);

		if (count >= PAGE_SIZE)
			return PAGE_SIZE - 1;
	}
	count += snprintf(buf + count, PAGE_SIZE - count,
			"  0x16 = 0x%x  \n", drv2604_reg_read(pDrv2604data, 0x16));
	if (count >= PAGE_SIZE)
		return PAGE_SIZE - 1;
	return count;
}

static struct device_attribute waveform_attrs[] = {
	__ATTR(waveform, (S_IRUGO | S_IWUSR | S_IWGRP),
			waveform_show,
			waveform_store),
};

static int drv2604_reg_write(struct drv2604_data *pDrv2604data, unsigned char reg, char val)
{
	return regmap_write(pDrv2604data->regmap, reg, val);
}

static int drv2604_bulk_read(struct drv2604_data *pDrv2604data, unsigned char reg, unsigned int count, u8 *buf)
{
	return regmap_bulk_read(pDrv2604data->regmap, reg, buf, count);
}

static int drv2604_bulk_write(struct drv2604_data *pDrv2604data, unsigned char reg, unsigned int count, const u8 *buf)
{
	return regmap_bulk_write(pDrv2604data->regmap, reg, buf, count);
}

static int drv2604_set_bits(struct drv2604_data *pDrv2604data, unsigned char reg, unsigned char mask, unsigned char val)
{
	return regmap_update_bits(pDrv2604data->regmap, reg, mask, val);
}

static int drv2604_set_go_bit(struct drv2604_data *pDrv2604data, unsigned char val)
{
	return drv2604_reg_write(pDrv2604data, GO_REG, (val&0x01));
}

static void drv2604_poll_go_bit(struct drv2604_data *pDrv2604data)
{
	while (drv2604_reg_read(pDrv2604data, GO_REG) == GO)
		schedule_timeout_interruptible(msecs_to_jiffies(GO_BIT_POLL_INTERVAL));
}

static int drv2604_set_rtp_val(struct drv2604_data *pDrv2604data, char value)
{

	return drv2604_reg_write(pDrv2604data, REAL_TIME_PLAYBACK_REG, value);
}

static int drv2604_set_waveform_sequence(struct drv2604_data *pDrv2604data, unsigned char *seq, unsigned int size)
{
	return drv2604_bulk_write(pDrv2604data, WAVEFORM_SEQUENCER_REG, (size > WAVEFORM_SEQUENCER_MAX)?WAVEFORM_SEQUENCER_MAX:size, seq);
}

static void drv2604_change_mode(struct drv2604_data *pDrv2604data, char work_mode, char dev_mode)
{
	/* please be noted : LRA open loop cannot be used with analog input mode */
	if (dev_mode == DEV_IDLE) {
		pDrv2604data->dev_mode = dev_mode;
		pDrv2604data->work_mode = work_mode;
	} else if (dev_mode == DEV_STANDBY) {
		if (pDrv2604data->dev_mode != DEV_STANDBY) {
			pDrv2604data->dev_mode = DEV_STANDBY;
			drv2604_set_rtp_val(pDrv2604data, 0);
			mdelay(25);
			drv2604_reg_write(pDrv2604data, MODE_REG, MODE_STANDBY);
			schedule_timeout_interruptible(msecs_to_jiffies(WAKE_STANDBY_DELAY));
		}
		pDrv2604data->work_mode = WORK_IDLE;
	} else if (dev_mode == DEV_READY) {
		if ((work_mode != pDrv2604data->work_mode)
			|| (dev_mode != pDrv2604data->dev_mode)) {
			pDrv2604data->work_mode = work_mode;
			pDrv2604data->dev_mode = dev_mode;
			if ((pDrv2604data->work_mode == WORK_VIBRATOR)
				|| (pDrv2604data->work_mode == WORK_PATTERN_RTP_ON)
				|| (pDrv2604data->work_mode == WORK_SEQ_RTP_ON)
				|| (pDrv2604data->work_mode == WORK_RTP)) {
					drv2604_reg_write(pDrv2604data, MODE_REG, MODE_REAL_TIME_PLAYBACK);
			} else if (pDrv2604data->work_mode == WORK_CALIBRATION) {
				drv2604_reg_write(pDrv2604data, MODE_REG, AUTO_CALIBRATION);
			} else {
				drv2604_reg_write(pDrv2604data, MODE_REG, MODE_INTERNAL_TRIGGER);
			}

			schedule_timeout_interruptible(msecs_to_jiffies(STANDBY_WAKE_DELAY));
		}
	}
}

static void play_effect(struct drv2604_data *pDrv2604data)
{
	switch_set_state(&pDrv2604data->sw_dev, SW_STATE_SEQUENCE_PLAYBACK);
	drv2604_change_mode(pDrv2604data, WORK_SEQ_PLAYBACK, DEV_READY);
	drv2604_set_waveform_sequence(pDrv2604data, pDrv2604data->sequence, WAVEFORM_SEQUENCER_MAX);
	pDrv2604data->vibrator_is_playing = YES;
	drv2604_set_go_bit(pDrv2604data, GO);

	pr_debug("play_effect  begin  \n");
	while ((drv2604_reg_read(pDrv2604data, GO_REG) == GO) && (pDrv2604data->should_stop == NO)) {
		schedule_timeout_interruptible(msecs_to_jiffies(GO_BIT_POLL_INTERVAL));
	}

	if (pDrv2604data->should_stop == YES) {
		drv2604_set_go_bit(pDrv2604data, STOP);
	}

	drv2604_change_mode(pDrv2604data, WORK_IDLE, DEV_STANDBY);
	switch_set_state(&pDrv2604data->sw_dev, SW_STATE_IDLE);
	pDrv2604data->vibrator_is_playing = NO;
	wake_unlock(&pDrv2604data->wklock);
}

static void vibrator_off(struct drv2604_data *pDrv2604data)
{
	char mode;
	int iTimeout = 10;

	if (pDrv2604data->vibrator_is_playing) {
		pDrv2604data->vibrator_is_playing = NO;
		drv2604_set_go_bit(pDrv2604data, STOP);
		do {
		drv2604_change_mode(pDrv2604data, WORK_IDLE, DEV_STANDBY);
		mode = drv2604_reg_read(pDrv2604data, MODE_REG) & DRV2604_MODE_MASK;
		} while ((MODE_REAL_TIME_PLAYBACK == mode) && (--iTimeout > 0));

		if (iTimeout <= 0) {
			pDrv2604data->vibrator_is_playing = YES;
			pr_err("drv2604 vibrator_off failed\n");
		}
		switch_set_state(&pDrv2604data->sw_dev, SW_STATE_IDLE);
		wake_unlock(&pDrv2604data->wklock);
	}
}

static void drv2604_stop(struct drv2604_data *pDrv2604data)
{
	if (pDrv2604data->vibrator_is_playing) {
		if ((pDrv2604data->work_mode == WORK_VIBRATOR)
				|| (pDrv2604data->work_mode == WORK_PATTERN_RTP_ON)
				|| (pDrv2604data->work_mode == WORK_PATTERN_RTP_OFF)
				|| (pDrv2604data->work_mode == WORK_SEQ_RTP_ON)
				|| (pDrv2604data->work_mode == WORK_SEQ_RTP_OFF)
				|| (pDrv2604data->work_mode == WORK_RTP)) {
			vibrator_off(pDrv2604data);
		} else if (pDrv2604data->work_mode == WORK_SEQ_PLAYBACK) {
		} else {
			printk("%s, err mode=%d \n", __FUNCTION__, pDrv2604data->work_mode);
		}
	}
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	struct drv2604_data *pDrv2604data = container_of(dev, struct drv2604_data, to_dev);

	if (hrtimer_active(&pDrv2604data->timer)) {
		ktime_t r = hrtimer_get_remaining(&pDrv2604data->timer);
		return ktime_to_ms(r);
	}

	return 0;
}
static void dev_init_platform_data(struct drv2604_data *pDrv2604data);

static void vibrator_enable(struct timed_output_dev *dev, int value)
{
	struct drv2604_data *pDrv2604data = container_of(dev, struct drv2604_data, to_dev);
	struct drv2604_platform_data *pDrv2604Platdata = &pDrv2604data->PlatData;
	struct actuator_data actuator = pDrv2604Platdata->actuator;
	pDrv2604data->should_stop = YES;
	hrtimer_cancel(&pDrv2604data->timer);
	cancel_work_sync(&pDrv2604data->vibrator_work);

	 mutex_lock(&pDrv2604data->lock);

	drv2604_stop(pDrv2604data);

	if (value > 0) {
		wake_lock(&pDrv2604data->wklock);

		if (drv2604_reg_read(pDrv2604data, RATED_VOLTAGE_REG) != actuator.rated_vol) {
			printk(KERN_INFO "drv2604: Register values reset.\n");
			drv2604_change_mode(pDrv2604data, WORK_IDLE, DEV_READY);
			schedule_timeout_interruptible(msecs_to_jiffies(STANDBY_WAKE_DELAY));
			pDrv2604data->OTP = drv2604_reg_read(pDrv2604data, AUTOCAL_MEM_INTERFACE_REG) & AUTOCAL_MEM_INTERFACE_REG_OTP_MASK;
			dev_init_platform_data(pDrv2604data);
		}

		drv2604_set_rtp_val(pDrv2604data, 0x7f);
		drv2604_change_mode(pDrv2604data, WORK_VIBRATOR, DEV_READY);
		pDrv2604data->vibrator_is_playing = YES;
		switch_set_state(&pDrv2604data->sw_dev, SW_STATE_RTP_PLAYBACK);

		value = (value > MAX_TIMEOUT)?MAX_TIMEOUT:value;
		hrtimer_start(&pDrv2604data->timer, ns_to_ktime((u64)value * NSEC_PER_MSEC), HRTIMER_MODE_REL);
	} else if (value < 0 && value >= -3) {

	if (value == -1)
		pDrv2604data->sequence[0] = 1;
	else if (value == -2)
		pDrv2604data->sequence[0] = 3;
	else
		pDrv2604data->sequence[0] = 2;
		wake_lock(&pDrv2604data->wklock);
		pDrv2604data->should_stop = NO;
		drv2604_change_mode(pDrv2604data, WORK_SEQ_PLAYBACK, DEV_IDLE);
		schedule_work(&pDrv2604data->vibrator_work);
	}

	mutex_unlock(&pDrv2604data->lock);
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	struct drv2604_data *pDrv2604data = container_of(timer, struct drv2604_data, timer);

	schedule_work(&pDrv2604data->vibrator_work);

	return HRTIMER_NORESTART;
}

static void vibrator_work_routine(struct work_struct *work)
{
	struct drv2604_data *pDrv2604data = container_of(work, struct drv2604_data, vibrator_work);

	mutex_lock(&pDrv2604data->lock);

	if ((pDrv2604data->work_mode == WORK_VIBRATOR)
		|| (pDrv2604data->work_mode == WORK_RTP)) {
		vibrator_off(pDrv2604data);
	} else if (pDrv2604data->work_mode == WORK_SEQ_PLAYBACK) {
		play_effect(pDrv2604data);

	}
	mutex_unlock(&pDrv2604data->lock);
}

static int fw_chksum(const struct firmware *fw)
{
	int sum = 0;
	int i = 0;
	int size = fw->size;
	const unsigned char *pBuf = fw->data;

	for (i = 0; i < size; i++) {
		if ((i > 11) && (i < 16)) {

		} else {
			sum += pBuf[i];
		}
	}

	return sum;
}

/* drv2604_firmware_load:   This function is called by the
 *		request_firmware_nowait function as soon
 *		as the firmware has been loaded from the file.
 *		The firmware structure contains the data and$
 *		the size of the firmware loaded.
 * @fw: pointer to firmware file to be dowloaded
 * @context: pointer variable to drv2604_data
 *
 *
 */

static void drv2604_firmware_load(const struct firmware *fw, void *context)
{
	struct drv2604_data *pDrv2604data = context;
	int size = 0, fwsize = 0, i = 0;
	const unsigned char *pBuf = NULL;

	if (fw != NULL) {
		pBuf = fw->data;
		size = fw->size;

		memcpy(&(pDrv2604data->fw_header), pBuf, sizeof(struct drv2604_fw_header));
		if ((pDrv2604data->fw_header.fw_magic != DRV2604_MAGIC)
			|| (pDrv2604data->fw_header.fw_size != size)
			|| (pDrv2604data->fw_header.fw_chksum != fw_chksum(fw))) {
			printk("%s, ERROR!! firmware not right:Magic=0x%x,Size=%d,chksum=0x%x\n",
				__FUNCTION__, pDrv2604data->fw_header.fw_magic,
				pDrv2604data->fw_header.fw_size, pDrv2604data->fw_header.fw_chksum);
		} else {
			printk("%s, firmware good\n", __FUNCTION__);

			drv2604_change_mode(pDrv2604data, WORK_IDLE, DEV_READY);

			pBuf += sizeof(struct drv2604_fw_header);

			drv2604_reg_write(pDrv2604data, DRV2604_REG_RAM_ADDR_UPPER_BYTE, 0);
			drv2604_reg_write(pDrv2604data, DRV2604_REG_RAM_ADDR_LOWER_BYTE, 0);

			fwsize = size - sizeof(struct drv2604_fw_header);
			for (i = 0; i < fwsize; i++) {
				drv2604_reg_write(pDrv2604data, DRV2604_REG_RAM_DATA, pBuf[i]);
			}
			drv2604_change_mode(pDrv2604data, WORK_IDLE, DEV_STANDBY);
		}
	} else {
			printk("%s : firmware not found,use default settings.\n", __FUNCTION__);
			drv2604_change_mode(pDrv2604data, WORK_IDLE, DEV_READY);
			drv2604_reg_write(pDrv2604data, DRV2604_REG_RAM_ADDR_UPPER_BYTE, 0);
			drv2604_reg_write(pDrv2604data, DRV2604_REG_RAM_ADDR_LOWER_BYTE, 0);
			for (i = 0; i < sizeof(effect); i++) {
				drv2604_reg_write(pDrv2604data, DRV2604_REG_RAM_DATA, effect[i]);
			}
			drv2604_change_mode(pDrv2604data, WORK_IDLE, DEV_STANDBY);
	}
}

static int dev2604_open (struct inode *i_node, struct file *filp)
{
	if (pDRV2604data == NULL)
		return -ENODEV;

	filp->private_data = pDRV2604data;
	return 0;
}

static ssize_t dev2604_read(struct file *filp, char *buff, size_t length, loff_t *offset)
{
	struct drv2604_data *pDrv2604data = (struct drv2604_data *)filp->private_data;
	int ret = 0;

	if (pDrv2604data->ReadLen > 0) {
		ret = copy_to_user(buff, pDrv2604data->ReadBuff, pDrv2604data->ReadLen);
		if (ret != 0)
			pr_debug("%s, copy_to_user err=%d \n", __FUNCTION__, ret);
		else
			ret = pDrv2604data->ReadLen;
		pDrv2604data->ReadLen = 0;
	} else
		pr_debug("%s, nothing to read\n", __FUNCTION__);

	return ret;
}

static bool isforDebug(int cmd)
{
	return ((cmd == HAPTIC_CMDID_REG_WRITE)
		|| (cmd == HAPTIC_CMDID_REG_READ)
		|| (cmd == HAPTIC_CMDID_REG_SETBIT));
}

static ssize_t dev2604_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
	struct drv2604_data *pDrv2604data = (struct drv2604_data *)filp->private_data;

	if (isforDebug(buff[0])) {
	} else {
		pDrv2604data->should_stop = YES;
		hrtimer_cancel(&pDrv2604data->timer);
		cancel_work_sync(&pDrv2604data->vibrator_work);
	}

	mutex_lock(&pDrv2604data->lock);

	if (isforDebug(buff[0])) {
	} else {
		drv2604_stop(pDrv2604data);
	}

	pr_debug("buff[0] = %c \n", buff[0]);
	switch (buff[0]) {

	case HAPTIC_CMDID_PLAY_SINGLE_EFFECT:
	case HAPTIC_CMDID_PLAY_EFFECT_SEQUENCE:
	{
		memset(&pDrv2604data->sequence, 0, WAVEFORM_SEQUENCER_MAX);
		if (!copy_from_user(&pDrv2604data->sequence, &buff[1], len - 1)) {
			wake_lock(&pDrv2604data->wklock);

			pDrv2604data->should_stop = NO;
			drv2604_change_mode(pDrv2604data, WORK_SEQ_PLAYBACK, DEV_IDLE);
			schedule_work(&pDrv2604data->vibrator_work);
		}
		break;
	}
	case HAPTIC_CMDID_STOP:
	{
		break;
	}

	case HAPTIC_CMDID_UPDATE_FIRMWARE:
	{
		struct firmware fw;
		unsigned char *fw_buffer = (unsigned char *)kzalloc(len-1, GFP_KERNEL);
		int result = -1;

		if (fw_buffer != NULL) {
			fw.size = len - 1;

			wake_lock(&pDrv2604data->wklock);
			result = copy_from_user(fw_buffer, &buff[1], fw.size);
			if (result == 0) {
				pr_debug("%s, fwsize=%lu, f:%x, l:%x\n", __FUNCTION__, fw.size, buff[1], buff[len-1]);
					fw.data = (const unsigned char *)fw_buffer;
					drv2604_firmware_load(&fw, (void *)pDrv2604data);
			}
			wake_unlock(&pDrv2604data->wklock);

			kfree(fw_buffer);
		}
		break;
	}

	case HAPTIC_CMDID_READ_FIRMWARE:
	{
		int i;
		if (len == 3) {
			pDrv2604data->ReadLen = 1;
			drv2604_reg_write(pDrv2604data, DRV2604_REG_RAM_ADDR_UPPER_BYTE, buff[2]);
			drv2604_reg_write(pDrv2604data, DRV2604_REG_RAM_ADDR_LOWER_BYTE, buff[1]);
			pDrv2604data->ReadBuff[0] = drv2604_reg_read(pDrv2604data, DRV2604_REG_RAM_DATA);
		} else if (len == 4) {
			drv2604_reg_write(pDrv2604data, DRV2604_REG_RAM_ADDR_UPPER_BYTE, buff[2]);
			drv2604_reg_write(pDrv2604data, DRV2604_REG_RAM_ADDR_LOWER_BYTE, buff[1]);
			pDrv2604data->ReadLen = (buff[3] > MAX_READ_BYTES)?MAX_READ_BYTES:buff[3];
			for (i = 0; i < pDrv2604data->ReadLen; i++) {
				pDrv2604data->ReadBuff[i] = drv2604_reg_read(pDrv2604data, DRV2604_REG_RAM_DATA);
			}
		} else
			printk("%s, read fw len error\n", __FUNCTION__);
		break;
	}

	case HAPTIC_CMDID_REG_READ:
	{
		if (len == 2) {
			pDrv2604data->ReadLen = 1;
			pDrv2604data->ReadBuff[0] = drv2604_reg_read(pDrv2604data, buff[1]);
		} else if (len == 3) {
			pDrv2604data->ReadLen = (buff[2] > MAX_READ_BYTES)?MAX_READ_BYTES:buff[2];
			drv2604_bulk_read(pDrv2604data, buff[1], pDrv2604data->ReadLen, pDrv2604data->ReadBuff);
		} else
			pr_debug("%s, reg_read len error\n", __FUNCTION__);
		break;
	}

	case HAPTIC_CMDID_REG_WRITE:
	{
		if ((len-1) == 2)
			drv2604_reg_write(pDrv2604data, buff[1], buff[2]);
		else if ((len-1) > 2) {
			unsigned char *data = (unsigned char *)kzalloc(len-2, GFP_KERNEL);
			if (data != NULL) {
				if (copy_from_user(data, &buff[2], len-2) != 0)
					pr_debug("%s, reg copy err\n", __FUNCTION__);
				else
					drv2604_bulk_write(pDrv2604data, buff[1], len-2, data);

				kfree(data);
			}
		} else
			pr_debug("%s, reg_write len error\n", __FUNCTION__);

		break;
	}

	case HAPTIC_CMDID_REG_SETBIT:
	{
		int i = 1;
		for (i = 1; i < len;) {
			drv2604_set_bits(pDrv2604data, buff[i], buff[i+1], buff[i+2]);
			i += 3;
		}
		break;
	}
	default:
		pr_debug("%s, unknown HAPTIC cmd\n", __FUNCTION__);
	break;
	}

	mutex_unlock(&pDrv2604data->lock);

	return len;
}


static struct file_operations fops = {
	.open = dev2604_open,
	.read = dev2604_read,
	.write = dev2604_write,
};
static int Haptics_init(struct drv2604_data *pDrv2604data)
{
	int reval = -ENOMEM;

	pDrv2604data->version = MKDEV(0, 0);
	reval = alloc_chrdev_region(&pDrv2604data->version, 0, 1, HAPTICS_DEVICE_NAME);
	if (reval < 0) {
		printk(KERN_ALERT"drv2604: error getting major number %d\n", reval);
		goto fail0;
	}

	pDrv2604data->class = class_create(THIS_MODULE, HAPTICS_DEVICE_NAME);
	if (!pDrv2604data->class) {
		printk(KERN_ALERT"drv2604: error creating class\n");
		goto fail1;
	}

	pDrv2604data->device = device_create(pDrv2604data->class, NULL, pDrv2604data->version, NULL, HAPTICS_DEVICE_NAME);
	if (!pDrv2604data->device) {
		printk(KERN_ALERT"drv2604: error creating device 2604\n");
		goto fail2;
	}

	cdev_init(&pDrv2604data->cdev, &fops);
	pDrv2604data->cdev.owner = THIS_MODULE;
	pDrv2604data->cdev.ops = &fops;
	reval = cdev_add(&pDrv2604data->cdev, pDrv2604data->version, 1);
	if (reval) {
		printk(KERN_ALERT"drv2604: fail to add cdev\n");
		goto fail3;
	}

	pDrv2604data->sw_dev.name = "haptics";
	reval = switch_dev_register(&pDrv2604data->sw_dev);
	if (reval < 0) {
		printk(KERN_ALERT"drv2604: fail to register switch\n");
		goto fail4;
	}

	pDrv2604data->to_dev.name = "vibrator";
	pDrv2604data->to_dev.get_time = vibrator_get_time;
	pDrv2604data->to_dev.enable = vibrator_enable;

	if (timed_output_dev_register(&(pDrv2604data->to_dev)) < 0) {
		printk(KERN_ALERT"drv2604: fail to create timed output dev\n");
		goto fail3;
	}
	 if (sysfs_create_file(&pDrv2604data->to_dev.dev->kobj,
					&waveform_attrs[0].attr) < 0) {
		 printk(KERN_ALERT"drv2604: fail to create sysfs\n");
		 goto fail3;
	 }


	hrtimer_init(&pDrv2604data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pDrv2604data->timer.function = vibrator_timer_func;
	INIT_WORK(&pDrv2604data->vibrator_work, vibrator_work_routine);

	wake_lock_init(&pDrv2604data->wklock, WAKE_LOCK_SUSPEND, "vibrator");
	mutex_init(&pDrv2604data->lock);

	return 0;

fail4:
	switch_dev_unregister(&pDrv2604data->sw_dev);
fail3:
	device_destroy(pDrv2604data->class, pDrv2604data->version);
fail2:
	class_destroy(pDrv2604data->class);
fail1:
	unregister_chrdev_region(pDrv2604data->version, 1);
fail0:
	return reval;
}

static void dev_init_platform_data(struct drv2604_data *pDrv2604data)
{
	struct drv2604_platform_data *pDrv2604Platdata = &pDrv2604data->PlatData;
	struct actuator_data actuator = pDrv2604Platdata->actuator;
	unsigned char temp = 0;


	if (pDrv2604data->OTP == 0) {
		if (actuator.rated_vol != 0) {
			drv2604_reg_write(pDrv2604data, RATED_VOLTAGE_REG, actuator.rated_vol);
		} else {
			printk("%s, ERROR Rated ZERO\n", __FUNCTION__);
		}

		if (actuator.over_drive_vol != 0) {
			drv2604_reg_write(pDrv2604data, OVERDRIVE_CLAMP_VOLTAGE_REG, actuator.over_drive_vol);
		} else {
			printk("%s, ERROR OverDriveVol ZERO\n", __FUNCTION__);
		}

		drv2604_set_bits(pDrv2604data,
						FEEDBACK_CONTROL_REG,
						FEEDBACK_CONTROL_DEVICE_TYPE_MASK,
						(actuator.device_type == LRA)?FEEDBACK_CONTROL_MODE_LRA:FEEDBACK_CONTROL_MODE_ERM);
	} else {
		printk("%s, OTP programmed\n", __FUNCTION__);
	}

	if (pDrv2604Platdata->loop == OPEN_LOOP) {
		temp = BIDIR_INPUT_BIDIRECTIONAL;
	} else {
		if (pDrv2604Platdata->BIDIRInput == UniDirectional) {
			temp = BIDIR_INPUT_UNIDIRECTIONAL;
		} else {
			temp = BIDIR_INPUT_BIDIRECTIONAL;
		}
	}

	if (actuator.device_type == LRA) {
		unsigned char DriveTime = 5*(1000 - actuator.LRAFreq)/actuator.LRAFreq;
		drv2604_set_bits(pDrv2604data,
				Control1_REG,
				Control1_REG_DRIVE_TIME_MASK,
				DriveTime);
		printk("%s, LRA = %d, DriveTime=0x%x\n", __FUNCTION__, actuator.LRAFreq, DriveTime);
	}

	drv2604_set_bits(pDrv2604data,
				Control2_REG,
				Control2_REG_BIDIR_INPUT_MASK,
				temp);

	if ((pDrv2604Platdata->loop == OPEN_LOOP) && (actuator.device_type == LRA)) {
		temp = LRA_OpenLoop_Enabled;
	} else if ((pDrv2604Platdata->loop == OPEN_LOOP) && (actuator.device_type == ERM)) {
		temp = ERM_OpenLoop_Enabled;
	} else {
		temp = ERM_OpenLoop_Disable|LRA_OpenLoop_Disable;
	}

	if ((pDrv2604Platdata->loop == CLOSE_LOOP) && (pDrv2604Platdata->BIDIRInput == UniDirectional)) {
		temp |= RTP_FORMAT_UNSIGNED;
		drv2604_reg_write(pDrv2604data, REAL_TIME_PLAYBACK_REG, 0xff);
	} else {
		if (pDrv2604Platdata->RTPFormat == Signed) {
			temp |= RTP_FORMAT_SIGNED;
			drv2604_reg_write(pDrv2604data, REAL_TIME_PLAYBACK_REG, 0x7f);
		} else {
			temp |= RTP_FORMAT_UNSIGNED;
			drv2604_reg_write(pDrv2604data, REAL_TIME_PLAYBACK_REG, 0xff);
		}
	}

	drv2604_set_bits(pDrv2604data,
					Control3_REG,
					Control3_REG_LOOP_MASK|Control3_REG_FORMAT_MASK,
					temp);
	drv2604_reg_write(pDrv2604data, AUTO_CALI_RESULT_REG, 0x08);
	drv2604_reg_write(pDrv2604data, AUTO_CALI_BACK_EMF_RESULT_REG, 0x8e);
	drv2604_set_bits(pDrv2604data, FEEDBACK_CONTROL_REG, 0x03, 0x01);

}

static int dev_auto_calibrate(struct drv2604_data *pDrv2604data)
{
	int err = 0, status = 0;

	drv2604_change_mode(pDrv2604data, WORK_CALIBRATION, DEV_READY);
	drv2604_set_go_bit(pDrv2604data, GO);


	drv2604_poll_go_bit(pDrv2604data);

	status = drv2604_reg_read(pDrv2604data, STATUS_REG);

	printk("%s, calibration status =0x%x\n", __FUNCTION__, status);

	drv2604_reg_read(pDrv2604data, AUTO_CALI_RESULT_REG);
	drv2604_reg_read(pDrv2604data, AUTO_CALI_BACK_EMF_RESULT_REG);
	drv2604_reg_read(pDrv2604data, FEEDBACK_CONTROL_REG);

	return err;
}

static struct regmap_config drv2604_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
};

static void HapticsFirmwareLoad(const struct firmware *fw, void *context)
{
	drv2604_firmware_load(fw, context);
	release_firmware(fw);
}

static int drv2604_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct drv2604_data *pDrv2604data;


	int err = 0;
	int status = 0;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR"%s:I2C check failed\n", __FUNCTION__);
		return -ENODEV;
	}

	pDrv2604data = devm_kzalloc(&client->dev, sizeof(struct drv2604_data), GFP_KERNEL);
	if (pDrv2604data == NULL) {
		printk(KERN_ERR"%s:no memory\n", __FUNCTION__);
		return -ENOMEM;
	}

	pDrv2604data->regmap = devm_regmap_init_i2c(client, &drv2604_i2c_regmap);
	if (IS_ERR(pDrv2604data->regmap)) {
		err = PTR_ERR(pDrv2604data->regmap);
		printk(KERN_ERR"%s:Failed to allocate register map: %d\n", __FUNCTION__, err);
		return err;
	}

	memcpy(&pDrv2604data->PlatData, &drv2604_plat_data, sizeof(struct drv2604_platform_data));
	i2c_set_clientdata(client, pDrv2604data);

	if (pDrv2604data->PlatData.GpioTrigger) {
		err = gpio_request(pDrv2604data->PlatData.GpioTrigger, HAPTICS_DEVICE_NAME"Trigger");
		if (err < 0) {
			printk(KERN_ERR"%s: GPIO request Trigger error\n", __FUNCTION__);
			goto exit_gpio_request_failed;
		}
	}


	if (gpio_request(93, "vibrator-en") < 0) {
		printk(KERN_ALERT"drv2604: error requesting gpio\n");
		return -EPERM;
	}

	gpio_direction_output(93, 1);

	mdelay(1);
	err = drv2604_reg_read(pDrv2604data, STATUS_REG);
	if (err < 0) {
		printk("%s, i2c bus fail (%d)\n", __FUNCTION__, err);
		goto exit_gpio_request_failed;
	} else {
		printk("%s, i2c status (0x%x)\n", __FUNCTION__, err);
		status = err;
	}
	/* Read device ID */
	pDrv2604data->device_id = (status & DEV_ID_MASK);
	switch (pDrv2604data->device_id) {
	case DRV2605_VER_1DOT1:
		printk("drv2604 driver found: drv2605 v1.1.\n");
		break;
	case DRV2605_VER_1DOT0:
		printk("drv2604 driver found: drv2605 v1.0.\n");
		break;
	case DRV2604:
		printk(KERN_ALERT"drv2604 driver found: drv2604.\n");
		break;
	case DRV2604L:
		printk(KERN_ALERT"drv2604 driver found: drv2604L.\n");
		break;
	case DRV2605L:
		printk(KERN_ALERT"drv2604 driver found: drv2605L.\n");
		break;
	default:
		printk(KERN_ERR"drv2604 driver found: unknown.\n");
		break;
	}

	if (pDrv2604data->device_id != DRV2604 && pDrv2604data->device_id != DRV2604L) {
		printk("%s, status(0x%x),device_id(%d) fail\n",
			__FUNCTION__, status, pDrv2604data->device_id);
		goto exit_gpio_request_failed;
	} else {
		err = request_firmware_nowait(THIS_MODULE,
						FW_ACTION_HOTPLUG,
						"drv2604.bin",
						&(client->dev),
						GFP_KERNEL,
						pDrv2604data,
						HapticsFirmwareLoad);

	}

	drv2604_change_mode(pDrv2604data, WORK_IDLE, DEV_READY);
	schedule_timeout_interruptible(msecs_to_jiffies(STANDBY_WAKE_DELAY));

	pDrv2604data->OTP = drv2604_reg_read(pDrv2604data, AUTOCAL_MEM_INTERFACE_REG) & AUTOCAL_MEM_INTERFACE_REG_OTP_MASK;

	dev_init_platform_data(pDrv2604data);

	if (0/*pDrv2604data->OTP == 0*/) {
		err = dev_auto_calibrate(pDrv2604data);
		if (err < 0) {
			printk("%s, ERROR, calibration fail\n",	__FUNCTION__);
		}
	}

	/* Put hardware in standby */
	drv2604_change_mode(pDrv2604data, WORK_IDLE, DEV_STANDBY);

	Haptics_init(pDrv2604data);

	pDRV2604data = pDrv2604data;
	printk("drv2604 probe succeeded\n");

	return 0;

exit_gpio_request_failed:
	if (pDrv2604data->PlatData.GpioTrigger) {
		gpio_free(pDrv2604data->PlatData.GpioTrigger);
	}

	if (pDrv2604data->PlatData.GpioEnable) {
		gpio_free(pDrv2604data->PlatData.GpioEnable);
	}

	printk(KERN_ERR"%s failed, err=%d\n", __FUNCTION__, err);
	return err;
}

static int drv2604_remove(struct i2c_client *client)
{
	struct drv2604_data *pDrv2604data = i2c_get_clientdata(client);

	device_destroy(pDrv2604data->class, pDrv2604data->version);
	class_destroy(pDrv2604data->class);
	unregister_chrdev_region(pDrv2604data->version, 1);

	if (pDrv2604data->PlatData.GpioTrigger)
		gpio_free(pDrv2604data->PlatData.GpioTrigger);

	if (pDrv2604data->PlatData.GpioEnable)
		gpio_free(pDrv2604data->PlatData.GpioEnable);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&pDrv2604data->early_suspend);
#endif

	printk(KERN_ALERT"drv2604 remove");

	return 0;
}

static struct i2c_device_id drv2604_id_table[] = {
	{ HAPTICS_DEVICE_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, drv2604_id_table);

static struct i2c_driver drv2604_driver = {
	.driver = {
		.name = HAPTICS_DEVICE_NAME,
		.owner = THIS_MODULE,
	},
	.id_table = drv2604_id_table,
	.probe = drv2604_probe,
	.remove = drv2604_remove,
};

static int __init drv2604_init(void)
{
	return i2c_add_driver(&drv2604_driver);
}

static void __exit drv2604_exit(void)
{
	i2c_del_driver(&drv2604_driver);
}

module_init(drv2604_init);
module_exit(drv2604_exit);

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("Driver for "HAPTICS_DEVICE_NAME);
