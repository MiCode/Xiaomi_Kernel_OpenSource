/*  drivers/media/radio/rtc6226/radio-rtc6226-common.c
 *
 *  Driver for Richwave RTC6226 FM Tuner
 *
 *  Copyright (c) 2009 Tobias Lorenz <tobias.lorenz@gmx.net>
 *  Copyright (c) 2012 Hans de Goede <hdegoede@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * 2008-01-12	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Version 1.0.0
 *		- First working version
 * 2008-01-13	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Version 1.0.1
 *		- Improved error handling, every function now returns errno
 *		- Improved multi user access (start/mute/stop)
 *		- Channel doesn't get lost anymore after start/mute/stop
 *		- RDS support added (polling mode via interrupt EP 1)
 *		- marked default module parameters with *value*
 *		- switched from bit structs to bit masks
 *		- header file cleaned and integrated
 * 2008-01-14	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Version 1.0.2
 *		- hex values are now lower case
 *		- commented USB ID for ADS/Tech moved on todo list
 *		- blacklisted in hid-quirks.c
 *		- rds buffer handling functions integrated into *_work, *_read
 *		- rds_command exchanged against simple retval
 *		- check for firmware version 15
 *		- code order and prototypes still remain the same
 *		- spacing and bottom of band codes remain the same
 * 2008-01-16	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Version 1.0.3
 *		- code reordered to avoid function prototypes
 *		- switch/case defaults are now more user-friendly
 *		- unified comment style
 *		- applied all checkpatch.pl v1.12 suggestions
 *		  except the warning about the too long lines with bit comments
 *		- renamed FMRADIO to RADIO to cut line length (checkpatch.pl)
 * 2008-01-22	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Version 1.0.4
 *		- avoid poss. locking when doing copy_to_user which may sleep
 *		- RDS is automatically activated on read now
 *		- code cleaned of unnecessary rds_commands
 *		- USB Vendor/Product ID for ADS/Tech FM Radio Receiver verified
 *		  (thanks to Guillaume RAMOUSSE)
 * 2008-01-27	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Version 1.0.5
 *		- number of seek_retries changed to tune_timeout
 *		- fixed problem with incomplete tune operations by own buffers
 *		- optimization of variables and printf types
 *		- improved error logging
 * 2008-01-31	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Oliver Neukum <oliver@neukum.org>
 *		Version 1.0.6
 *		- fixed coverity checker warnings in *_usb_driver_disconnect
 *		- probe()/open() race by correct ordering in probe()
 *		- DMA coherency rules by separate allocation of all buffers
 *		- use of endianness macros
 *		- abuse of spinlock, replaced by mutex
 *		- racy handling of timer in disconnect,
 *		  replaced by delayed_work
 *		- racy interruptible_sleep_on(),
 *		  replaced with wait_event_interruptible()
 *		- handle signals in read()
 * 2008-02-08	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Oliver Neukum <oliver@neukum.org>
 *		Version 1.0.7
 *		- usb autosuspend support
 *		- unplugging fixed
 * 2008-05-07	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Version 1.0.8
 *		- hardware frequency seek support
 *		- afc indication
 *		- more safety checks, get_freq return errno
 *		- vidioc behavior corrected according to v4l2 spec
 * 2008-10-20	Alexey Klimov <klimov.linux@gmail.com>
 *		- add support for KWorld USB FM Radio FM700
 *		- blacklisted KWorld radio in hid-core.c and hid-ids.h
 * 2008-12-03	Mark Lord <mlord@pobox.com>
 *		- add support for DealExtreme USB Radio
 * 2009-01-31	Bob Ross <pigiron@gmx.com>
 *		- correction of stereo detection/setting
 *		- correction of signal strength indicator scaling
 * 2009-01-31	Rick Bronson <rick@efn.org>
 *		Tobias Lorenz <tobias.lorenz@gmx.net>
 *		- add LED status output
 *		- get HW/SW version from scratchpad
 * 2009-06-16   Edouard Lafargue <edouard@lafargue.name>
 *		Version 1.0.10
 *		- add support for interrupt mode for RDS endpoint,
 *                instead of polling.
 *                Improves RDS reception significantly
 * 2018-02-01	LG Electronics, Inc.
 * 2018-08-19   Richwave Technology Co.Ltd
 */

/* kernel includes */
#include <linux/delay.h>
#include <linux/i2c.h>
#include "radio-rtc6226.h"
/**************************************************************************
 * Module Parameters
 **************************************************************************/

/* Bottom of Band (MHz) */
/* 0: 87.5 - 108 MHz (USA, Europe)*/
/* 1: 76   - 108 MHz (Japan wide band) */
/* 2: 76   -  90 MHz (Japan) */

/* De-emphasis */
/* 0: 75 us (USA) */
/* 1: 50 us (Europe, Australia, Japan) */
static unsigned short de;

wait_queue_head_t rtc6226_wq;
int rtc6226_wq_flag = NO_WAIT;
#ifdef New_VolumeControl
unsigned short global_volume;
#endif

void rtc6226_q_event(struct rtc6226_device *radio,
		enum rtc6226_evt_t event)
{

	struct kfifo *data_b;
	unsigned char evt = event;

	data_b = &radio->data_buf[RTC6226_FM_BUF_EVENTS];

	pr_info("%s updating event_q with event %x\n", __func__, event);
	if (kfifo_in_locked(data_b,
				&evt,
				1,
				&radio->buf_lock[RTC6226_FM_BUF_EVENTS]))
		wake_up_interruptible(&radio->event_queue);
}

/*
 * rtc6226_set_chan - set the channel
 */
static int rtc6226_set_chan(struct rtc6226_device *radio, unsigned short chan)
{
	int retval;
	unsigned short current_chan =
		radio->registers[CHANNEL] & CHANNEL_CSR0_CH;

	pr_info("%s CHAN=%d chan=%d\n", __func__, radio->registers[CHANNEL],
						chan);

	/* start tuning */
	radio->registers[CHANNEL] &= ~CHANNEL_CSR0_CH;
	radio->registers[CHANNEL] |= CHANNEL_CSR0_TUNE | chan;
	retval = rtc6226_set_register(radio, CHANNEL);
	if (retval < 0)	{
		radio->registers[CHANNEL] = current_chan;
		goto done;
	}

done:
	pr_info("%s exit %d\n", __func__, retval);
	return retval;
}

/*
 * rtc6226_get_freq - get the frequency
 */
static int rtc6226_get_freq(struct rtc6226_device *radio, unsigned int *freq)
{
	unsigned short chan;
	unsigned short rssi = 0;
	int retval;

	pr_info("%s enter\n", __func__);

	/* read channel */
	retval = rtc6226_get_register(radio, CHANNEL1);
	if (retval < 0) {
		pr_info("%s fail to get register\n", __func__);
		goto end;
	}
	chan = radio->registers[CHANNEL1] & STATUS_READCH;
	retval = rtc6226_get_register(radio, RSSI);
	rssi = radio->registers[RSSI] & RSSI_RSSI;
	pr_info("%s chan %d\n", __func__, chan);
	*freq = chan * TUNE_STEP_SIZE;
	pr_info("FMRICHWAVE, freq= %d, rssi= %d dBuV\n", *freq, rssi);

	if (rssi < radio->rssi_th)
		rtc6226_q_event(radio, RTC6226_EVT_BELOW_TH);
	else
		rtc6226_q_event(radio, RTC6226_EVT_ABOVE_TH);

end:
	return retval;
}


/*
 * rtc6226_set_freq - set the frequency
 */
int rtc6226_set_freq(struct rtc6226_device *radio, unsigned int freq)
{
	unsigned int band_bottom;
	unsigned short chan;
	unsigned char i;
	int retval = 0;

	pr_info("%s enter freq:%d\n", __func__, freq);

	band_bottom = (radio->registers[RADIOSEEKCFG2] &
		CHANNEL_CSR0_FREQ_BOT) * TUNE_STEP_SIZE;

	if (freq < band_bottom)
		freq = band_bottom;

	/* Chan = Freq (Mhz) / 10 */
	chan = (u16)(freq / TUNE_STEP_SIZE);

	pr_info("%s chan:%d freq:%d  band_bottom:%d\n", __func__,
			chan, freq, band_bottom);
	retval = rtc6226_set_chan(radio, chan);
	if (retval < 0) {
		pr_info("%s fail to set chan\n", __func__);
		goto end;
	}

	for (i = 0x12; i < RADIO_REGISTER_NUM; i++) {
		retval = rtc6226_get_register(radio, i);
		if (retval < 0) {
			pr_info("%s fail to get register\n", __func__);
			goto end;
		}
	}

end:
	return retval;
}


/*
 * rtc6226_set_seek - set seek
 */
static int rtc6226_set_seek(struct rtc6226_device *radio,
	 unsigned int seek_up, unsigned int seek_wrap)
{
	int retval = 0;
	unsigned short seekcfg1_val = radio->registers[SEEKCFG1];

	pr_info("%s enter up:%d wrap:%d, th:%d\n", __func__, seek_up, seek_wrap,
						seekcfg1_val);
	if (seek_wrap)
		radio->registers[SEEKCFG1] &= ~SEEKCFG1_CSR0_SKMODE;
	else
		radio->registers[SEEKCFG1] |= SEEKCFG1_CSR0_SKMODE;

	if (seek_up)
		radio->registers[SEEKCFG1] |= SEEKCFG1_CSR0_SEEKUP;
	else
		radio->registers[SEEKCFG1] &= ~SEEKCFG1_CSR0_SEEKUP;

	radio->registers[SEEKCFG1] &= ~SEEKCFG1_CSR0_SEEK;
	retval = rtc6226_set_register(radio, SEEKCFG1);
	if (retval < 0) {
		radio->registers[SEEKCFG1] = seekcfg1_val;
		goto done;
	}

	/* start seeking */
	radio->registers[SEEKCFG1] |= SEEKCFG1_CSR0_SEEK;
	retval = rtc6226_set_register(radio, SEEKCFG1);
	if (retval < 0) {
		radio->registers[SEEKCFG1] = seekcfg1_val;
		goto done;
	}

done:
	pr_info("%s exit %d\n", __func__, retval);
	return retval;
}

static void rtc6226_update_search_list(struct rtc6226_device *radio, int freq)
{
	int temp_freq = freq;
	int index = radio->srch_list.num_stations_found;

	temp_freq = temp_freq -
		(radio->recv_conf.band_low_limit * TUNE_STEP_SIZE);
	temp_freq = temp_freq / 50;
	radio->srch_list.rel_freq[index].rel_freq_lsb = GET_LSB(temp_freq);
	radio->srch_list.rel_freq[index].rel_freq_msb = GET_MSB(temp_freq);
	radio->srch_list.num_stations_found++;
}

void rtc6226_scan(struct work_struct *work)
{
	struct rtc6226_device *radio;
	int current_freq_khz = 0;
	struct kfifo *data_b;
	int len = 0;
	u32 next_freq_khz;
	int retval = 0;
	int i;

	pr_info("%s enter\n", __func__);

	radio = container_of(work, struct rtc6226_device, work_scan.work);

	retval = rtc6226_get_freq(radio, &current_freq_khz);
	if (retval < 0) {
		pr_err("%s fail to get freq\n", __func__);
		goto seek_tune_fail;
	}
	pr_info("%s cuurent freq %d\n", __func__, current_freq_khz);
		/* tune to lowest freq of the band */
	radio->seek_tune_status = SCAN_PENDING;
	retval = rtc6226_set_freq(radio,
		radio->recv_conf.band_low_limit * TUNE_STEP_SIZE);
	if (retval < 0)
		goto seek_tune_fail;
	/* wait for tune to complete. */
	if (!wait_for_completion_timeout(&radio->completion,
				msecs_to_jiffies(WAIT_TIMEOUT_MSEC)))
		pr_err("In %s, didn't receive STC for tune\n", __func__);

	while (1) {
		if (radio->is_search_cancelled) {
			pr_err("%s: scan cancelled\n", __func__);
			if (radio->g_search_mode == SCAN_FOR_STRONG)
				goto seek_tune_fail;
			else
				goto seek_cancelled;
			goto seek_cancelled;
		} else if (radio->mode != FM_RECV) {
			pr_err("%s: FM is not in proper state\n", __func__);
			return;
		}

		retval = rtc6226_set_seek(radio, SRCH_UP, WRAP_DISABLE);
		if (retval < 0) {
			pr_err("%s seek fail %d\n", __func__, retval);
			goto seek_tune_fail;
		}
			/* wait for seek to complete */
		if (!wait_for_completion_timeout(&radio->completion,
					msecs_to_jiffies(WAIT_TIMEOUT_MSEC))) {
			pr_err("%s:timeout didn't receive STC for seek\n",
						__func__);
			rtc6226_get_all_registers(radio);
			for (i = 0; i < 16; i++)
				pr_info("%s registers[%d]:%x\n", __func__, i,
					radio->registers[i]);
			/* FM is not correct state or scan is cancelled */
			continue;
		} else
			pr_err("%s: received STC for seek\n", __func__);

		retval = rtc6226_get_freq(radio, &next_freq_khz);
		if (retval < 0) {
			pr_err("%s fail to get freq\n", __func__);
			goto seek_tune_fail;
		}
		pr_info("%s next freq %d\n", __func__, next_freq_khz);

		retval = rtc6226_get_register(radio, RSSI);
		if (retval < 0) {
			pr_err("%s read fail to RSSI\n", __func__);
			goto seek_tune_fail;
		}

		pr_info("%s valid channel %d, rssi %d\n", __func__,
			next_freq_khz, radio->registers[RSSI] & RSSI_RSSI);

		if (radio->registers[STATUS] & STATUS_SF) {
			pr_err("%s band limit reached. Seek one more.\n",
					__func__);
			break;
		}
		if (radio->g_search_mode == SCAN)
			rtc6226_q_event(radio, RTC6226_EVT_TUNE_SUCC);
		/*
		 * If scan is cancelled or FM is not ON, break ASAP so that we
		 * don't need to sleep for dwell time.
		 */
		if (radio->is_search_cancelled) {
			pr_err("%s: scan cancelled\n", __func__);
			if (radio->g_search_mode == SCAN_FOR_STRONG)
				goto seek_tune_fail;
			else
				goto seek_cancelled;
			goto seek_cancelled;
		} else if (radio->mode != FM_RECV) {
			pr_err("%s: FM is not in proper state\n", __func__);
			return;
		}
		pr_info("%s update search list %d\n", __func__, next_freq_khz);
		if (radio->g_search_mode == SCAN) {
			/* sleep for dwell period */
			msleep(radio->dwell_time_sec * 1000);
			/* need to queue the event when the seek completes */
			pr_info("%s frequency update list %d\n", __func__,
				next_freq_khz);
			rtc6226_q_event(radio, RTC6226_EVT_SCAN_NEXT);
		} else if (radio->g_search_mode == SCAN_FOR_STRONG) {
			rtc6226_update_search_list(radio, next_freq_khz);
		}

	}

seek_tune_fail:
	if (radio->g_search_mode == SCAN_FOR_STRONG) {
		len = radio->srch_list.num_stations_found * 2 +
			sizeof(radio->srch_list.num_stations_found);
		data_b = &radio->data_buf[RTC6226_FM_BUF_SRCH_LIST];
		kfifo_in_locked(data_b, &radio->srch_list, len,
				&radio->buf_lock[RTC6226_FM_BUF_SRCH_LIST]);
		rtc6226_q_event(radio, RTC6226_EVT_NEW_SRCH_LIST);
	}
	/* tune to original frequency */
	retval = rtc6226_set_freq(radio, current_freq_khz);
	if (retval < 0)
		pr_err("%s: Tune to orig freq failed with error %d\n",
				__func__, retval);
	else {
		if (!wait_for_completion_timeout(&radio->completion,
			msecs_to_jiffies(WAIT_TIMEOUT_MSEC)))
			pr_err("%s: didn't receive STD for tune\n", __func__);
		else
			pr_err("%s: received STD for tune\n", __func__);
	}
seek_cancelled:
	rtc6226_q_event(radio, RTC6226_EVT_SEEK_COMPLETE);
	radio->seek_tune_status = NO_SEEK_TUNE_PENDING;
	pr_err("%s seek cancelled %d\n", __func__, retval);
	return;

}

int rtc6226_cancel_seek(struct rtc6226_device *radio)
{
	int retval = 0;

	pr_info("%s enter\n", __func__);
	mutex_lock(&radio->lock);

	/* stop seeking */
	radio->registers[SEEKCFG1] &= ~SEEKCFG1_CSR0_SEEK;
	retval = rtc6226_set_register(radio, SEEKCFG1);
	complete(&radio->completion);

	mutex_unlock(&radio->lock);
	radio->is_search_cancelled = true;

	return retval;

}

void rtc6226_search(struct rtc6226_device *radio, bool on)
{
	int current_freq_khz;

	current_freq_khz = radio->tuned_freq_khz;

	if (on) {
		pr_info("%s: Queuing the work onto scan work q\n", __func__);
		queue_delayed_work(radio->wqueue_scan, &radio->work_scan,
					msecs_to_jiffies(10));
	} else {
		rtc6226_cancel_seek(radio);
		rtc6226_q_event(radio, RTC6226_EVT_SEEK_COMPLETE);
	}
}

/*
 * rtc6226_start - switch on radio
 */
int rtc6226_start(struct rtc6226_device *radio)
{
	int retval;
	u8 i2c_error;
	u16 initbuf[] = {0x0000};

	radio->registers[BANKCFG] = 0x0000;
	i2c_error = 0;
	/* Keep in case of any unpredicted control */
	/* Set 0x16AA */
	radio->registers[DEVICEID] = 0x16AA;
	/* released the I2C from unexpected I2C start condition */
	retval = rtc6226_set_register(radio, DEVICEID);
	/* recheck TH : 10 */
	while ((retval < 0) && (i2c_error < 10)) {
		retval = rtc6226_set_register(radio, DEVICEID);
		i2c_error++;
	}

	if (retval < 0)	{
		pr_err("%s set to fail retval = %d\n", __func__, retval);
		/* goto done;*/
	}
	msleep(30);

	/* Don't read all between writing 0x16AA and 0x96AA */
	i2c_error = 0;
	radio->registers[DEVICEID] = 0x96AA;
	retval = rtc6226_set_register(radio, DEVICEID);
	/* recheck TH : 10 */
	while ((retval < 0) && (i2c_error < 10)) {
		retval = rtc6226_set_register(radio, DEVICEID);
		i2c_error++;
	}

	if (retval < 0)
		pr_err("%s set to fail 0x96AA %d\n", __func__, retval);
	msleep(30);

	/* get device and chip versions */
	rtc6226_get_register(radio, DEVICEID);
	rtc6226_get_register(radio, CHIPID);
	pr_info("%s DeviceID=0x%x ChipID=0x%x Addr=0x%x\n", __func__,
		radio->registers[DEVICEID], radio->registers[CHIPID],
		radio->client->addr);

	/* Have to update shadow buf from all register */
	retval = rtc6226_get_all_registers(radio);
	if (retval < 0)
		goto done;

	pr_info("%s rtc6226_power_up1: DeviceID=0x%4.4hx ChipID=0x%4.4hx\n",
		__func__,
		radio->registers[DEVICEID], radio->registers[CHIPID]);
	pr_info("%s rtc6226_power_up2: Reg2=0x%4.4hx Reg3=0x%4.4hx\n", __func__,
		radio->registers[MPXCFG], radio->registers[CHANNEL]);
	pr_info("%s rtc6226_power_up3: Reg4=0x%4.4hx Reg5=0x%4.4hx\n", __func__,
		radio->registers[SYSCFG], radio->registers[SEEKCFG1]);
	pr_info("%s rtc6226_power_up4: Reg6=0x%4.4hx Reg7=0x%4.4hx\n", __func__,
		radio->registers[POWERCFG], radio->registers[PADCFG]);
	pr_info("%s rtc6226_power_up5: Reg8=0x%4.4hx Reg9=0x%4.4hx\n", __func__,
		radio->registers[8], radio->registers[9]);
	pr_info("%s rtc6226_power_up6: regA=0x%4.4hx RegB=0x%4.4hx\n", __func__,
		radio->registers[10], radio->registers[11]);
	pr_info("%s rtc6226_power_up7: regC=0x%4.4hx RegD=0x%4.4hx\n", __func__,
		radio->registers[12], radio->registers[13]);
	pr_info("%s rtc6226_power_up8: regE=0x%4.4hx RegF=0x%4.4hx\n", __func__,
		radio->registers[14], radio->registers[15]);


	pr_info("%s DeviceID=0x%x ChipID=0x%x Addr=0x%x\n", __func__,
		radio->registers[DEVICEID], radio->registers[CHIPID],
		radio->client->addr);

	/* initial patch 01 */
	initbuf[0] = 0x0038;
	retval = rtc6226_set_serial_registers(radio, initbuf, 0x40);
	if (retval < 0)
		goto done;

	/* initial patch 02 */
	initbuf[0] = 0xC100;
	retval = rtc6226_set_serial_registers(radio, initbuf, 0x8E);
	if (retval < 0)
		goto done;

done:
	return retval;
}

/*
 * rtc6226_stop - switch off radio
 */
int rtc6226_stop(struct rtc6226_device *radio)
{
	int retval;

	/* sysconfig */
	radio->registers[SYSCFG] &= ~SYSCFG_CSR0_RDS_EN;
	radio->registers[SYSCFG] &= ~SYSCFG_CSR0_RDSIRQEN;
	radio->registers[SYSCFG] &= ~SYSCFG_CSR0_STDIRQEN;
	retval = rtc6226_set_register(radio, SYSCFG);
	if (retval < 0)
		goto done;

	/* powerconfig */
	radio->registers[MPXCFG] &= ~MPXCFG_CSR0_DIS_MUTE;
	retval = rtc6226_set_register(radio, MPXCFG);

	/* POWERCFG_ENABLE has to automatically go low */
	radio->registers[POWERCFG] |= POWERCFG_CSR0_DISABLE;
	radio->registers[POWERCFG] &= ~POWERCFG_CSR0_ENABLE;
	retval = rtc6226_set_register(radio, POWERCFG);

	/* Set 0x16AA */
	radio->registers[DEVICEID] = 0x16AA;
	retval = rtc6226_set_register(radio, DEVICEID);

done:
	return retval;
}

static void rtc6226_get_rds(struct rtc6226_device *radio)
{
	int retval = 0;
	int i;

	mutex_lock(&radio->lock);
	retval = rtc6226_get_all_registers(radio);

	if (retval < 0) {
		pr_err("%s read fail%d\n", __func__, retval);
		mutex_unlock(&radio->lock);
		return;
	}
	radio->block[0] = radio->registers[BA_DATA];
	radio->block[1] = radio->registers[BB_DATA];
	radio->block[2] = radio->registers[BC_DATA];
	radio->block[3] = radio->registers[BD_DATA];

	for (i = 0; i < 4; i++)
		pr_info("%s block[%d] %x\n", __func__, i, radio->block[i]);

	radio->bler[0] = (radio->registers[RSSI] & RSSI_RDS_BA_ERRS) >> 14;
	radio->bler[1] = (radio->registers[RSSI] & RSSI_RDS_BB_ERRS) >> 12;
	radio->bler[2] = (radio->registers[RSSI] & RSSI_RDS_BC_ERRS) >> 10;
	radio->bler[3] = (radio->registers[RSSI] & RSSI_RDS_BD_ERRS) >> 8;
	mutex_unlock(&radio->lock);
}

static void rtc6226_pi_check(struct rtc6226_device *radio, u16 current_pi)
{
	if (radio->pi != current_pi) {
		pr_info("%s current_pi %x , radio->pi %x\n"
				, __func__, current_pi, radio->pi);
		radio->pi = current_pi;
	} else {
		pr_info("%s Received same PI code\n", __func__);
	}
}

static void rtc6226_pty_check(struct rtc6226_device *radio, u8 current_pty)
{
	if (radio->pty != current_pty) {
		pr_info("%s PTY code of radio->block[1] = %x\n",
			__func__, current_pty);
		radio->pty = current_pty;
	} else {
		pr_info("%s PTY repeated\n", __func__);
	}
}

static bool is_new_freq(struct rtc6226_device *radio, u32 freq)
{
	u8 i = 0;

	for (i = 0; i < radio->af_info2.size; i++) {
		if (freq == radio->af_info2.af_list[i])
			return false;
	}

	return true;
}

static bool is_different_af_list(struct rtc6226_device *radio)
{
	u8 i = 0, j = 0;
	u32 freq;

	if (radio->af_info1.orig_freq_khz != radio->af_info2.orig_freq_khz)
		return true;

	/* freq is same, check if the AFs are same. */
	for (i = 0; i < radio->af_info1.size; i++) {
		freq = radio->af_info1.af_list[i];
		for (j = 0; j < radio->af_info2.size; j++) {
			if (freq == radio->af_info2.af_list[j])
				break;
		}

		/* freq is not there in list2 i.e list1, list2 are different.*/
		if (j == radio->af_info2.size)
			return true;
	}

	return false;
}

static bool is_valid_freq(struct rtc6226_device *radio, u32 freq)
{
	u32 band_low_limit;
	u32 band_high_limit;
	u8 spacing = 0;

	band_low_limit = radio->recv_conf.band_low_limit * TUNE_STEP_SIZE;
	band_high_limit = radio->recv_conf.band_high_limit * TUNE_STEP_SIZE;

	if (radio->space == 0)
		spacing = CH_SPACING_200;
	else if (radio->space == 1)
		spacing = CH_SPACING_100;
	else if (radio->space == 2)
		spacing = CH_SPACING_50;
	else
		return false;

	if ((freq >= band_low_limit) &&
			(freq <= band_high_limit) &&
			((freq - band_low_limit) % spacing == 0))
		return true;

	return false;
}

static void rtc6226_update_af_list(struct rtc6226_device *radio)
{

	bool retval;
	u8 i = 0;
	u8 af_data = radio->block[2] >> 8;
	u32 af_freq_khz;
	u32 tuned_freq_khz;
	struct kfifo *buff;
	struct af_list_ev ev;
	spinlock_t lock = radio->buf_lock[RTC6226_FM_BUF_AF_LIST];

	rtc6226_get_freq(radio, &tuned_freq_khz);

	for (; i < NO_OF_AF_IN_GRP; i++, af_data = radio->block[2] & 0xFF) {

		if (af_data >= MIN_AF_CNT_CODE && af_data <= MAX_AF_CNT_CODE) {

			pr_info("%s: resetting af info, freq %u, pi %u\n",
					__func__, tuned_freq_khz, radio->pi);
			radio->af_info2.inval_freq_cnt = 0;
			radio->af_info2.cnt = 0;
			radio->af_info2.orig_freq_khz = 0;

			/* AF count. */
			radio->af_info2.cnt = af_data - NO_AF_CNT_CODE;
			radio->af_info2.orig_freq_khz = tuned_freq_khz;
			radio->af_info2.pi = radio->pi;

			pr_info("%s: current freq is %u, AF cnt is %u\n",
				__func__, tuned_freq_khz, radio->af_info2.cnt);
		} else if (af_data >= MIN_AF_FREQ_CODE &&
				af_data <= MAX_AF_FREQ_CODE &&
				radio->af_info2.orig_freq_khz != 0 &&
				radio->af_info2.size < MAX_NO_OF_AF) {

			af_freq_khz = SCALE_AF_CODE_TO_FREQ_KHZ(af_data);
			retval = is_valid_freq(radio, af_freq_khz);
			if (!retval) {
				pr_info("%s: Invalid AF\n", __func__);
				radio->af_info2.inval_freq_cnt++;
				continue;
			}

			retval = is_new_freq(radio, af_freq_khz);
			if (!retval) {
				pr_info("%s: Duplicate AF\n", __func__);
				radio->af_info2.inval_freq_cnt++;
				continue;
			}

			/* update the AF list */
			radio->af_info2.af_list[radio->af_info2.size++] =
				af_freq_khz;
			pr_info("%s: AF is %u\n", __func__, af_freq_khz);
			if ((radio->af_info2.size +
					radio->af_info2.inval_freq_cnt ==
					radio->af_info2.cnt) &&
					is_different_af_list(radio)) {

				/* Copy the list to af_info1. */
				radio->af_info1.cnt = radio->af_info2.cnt;
				radio->af_info1.size = radio->af_info2.size;
				radio->af_info1.pi = radio->af_info2.pi;
				radio->af_info1.orig_freq_khz =
					radio->af_info2.orig_freq_khz;
				memset(radio->af_info1.af_list, 0,
					sizeof(radio->af_info1.af_list));

				memcpy(radio->af_info1.af_list,
					radio->af_info2.af_list,
					sizeof(radio->af_info2.af_list));

				/* AF list changed, post it to user space */
				memset(&ev, 0, sizeof(struct af_list_ev));

				ev.tune_freq_khz =
					radio->af_info1.orig_freq_khz;
				ev.pi_code = radio->pi;
				ev.af_size = radio->af_info1.size;

				memcpy(&ev.af_list[0],
						radio->af_info1.af_list,
						GET_AF_LIST_LEN(ev.af_size));

				buff = &radio->data_buf[RTC6226_FM_BUF_AF_LIST];
				kfifo_in_locked(buff,
						(u8 *)&ev,
						GET_AF_EVT_LEN(ev.af_size),
						&lock);

				pr_info("%s: posting AF list evt,currfreq %u\n",
						__func__, ev.tune_freq_khz);

				rtc6226_q_event(radio,
						RTC6226_EVT_NEW_AF_LIST);
			}
		}
	}
}

static void rtc6226_update_ps(struct rtc6226_device *radio, u8 addr, u8 ps)
{
	u8 i;
	bool ps_txt_chg = false;
	bool ps_cmplt = true;
	u8 *data;
	struct kfifo *data_b;

	pr_info("%s enter addr:%x ps:%x\n", __func__, addr, ps);

	if (radio->ps_tmp0[addr] == ps) {
		if (radio->ps_cnt[addr] < PS_VALIDATE_LIMIT) {
			radio->ps_cnt[addr]++;
		} else {
			radio->ps_cnt[addr] = PS_VALIDATE_LIMIT;
			radio->ps_tmp1[addr] = ps;
		}
	} else if (radio->ps_tmp1[addr] == ps) {
		if (radio->ps_cnt[addr] >= PS_VALIDATE_LIMIT) {
			ps_txt_chg = true;
			radio->ps_cnt[addr] = PS_VALIDATE_LIMIT + 1;
		} else {
			radio->ps_cnt[addr] = PS_VALIDATE_LIMIT;
		}
		radio->ps_tmp1[addr] = radio->ps_tmp0[addr];
		radio->ps_tmp0[addr] = ps;
	} else if (!radio->ps_cnt[addr]) {
		radio->ps_tmp0[addr] = ps;
		radio->ps_cnt[addr] = 1;
	} else {
		radio->ps_tmp1[addr] = ps;
	}

	if (ps_txt_chg) {
		for (i = 0; i < MAX_PS_LEN; i++) {
			if (radio->ps_cnt[i] > 1)
				radio->ps_cnt[i]--;
		}
	}

	for (i = 0; i < MAX_PS_LEN; i++) {
		if (radio->ps_cnt[i] < PS_VALIDATE_LIMIT) {
			pr_info("%s ps_cnt[%d] %d\n", __func__, i,
				radio->ps_cnt[i]);
			ps_cmplt = false;
			return;
		}
	}

	if (ps_cmplt) {
		for (i = 0; (i < MAX_PS_LEN) &&
			(radio->ps_display[i] == radio->ps_tmp0[i]); i++)
			;
		if (i == MAX_PS_LEN) {
			pr_info("%s Same PS string repeated\n", __func__);
			return;
		}

		for (i = 0; i < MAX_PS_LEN; i++)
			radio->ps_display[i] = radio->ps_tmp0[i];

		data = kmalloc(PS_EVT_DATA_LEN, GFP_ATOMIC);
		if (data != NULL) {
			data[0] = NO_OF_PS;
			data[1] = radio->pty;
			data[2] = (radio->pi >> 8) & 0xFF;
			data[3] = (radio->pi & 0xFF);
			data[4] = 0;
			memcpy(data + OFFSET_OF_PS,
					radio->ps_tmp0, MAX_PS_LEN);
			data_b = &radio->data_buf[RTC6226_FM_BUF_PS_RDS];
			kfifo_in_locked(data_b, data, PS_EVT_DATA_LEN,
				&radio->buf_lock[RTC6226_FM_BUF_PS_RDS]);
			pr_info("%s Q the PS event\n", __func__);
			rtc6226_q_event(radio, RTC6226_EVT_NEW_PS_RDS);
			kfree(data);
		} else {
			pr_err("%s Memory allocation failed for PTY\n",
				__func__);
		}
	}
}

static void display_rt(struct rtc6226_device *radio)
{
	u8 len = 0, i = 0;
	u8 *data;
	struct kfifo *data_b;
	bool rt_cmplt = true;

	pr_info("%s enter\n", __func__);

	for (i = 0; i < MAX_RT_LEN; i++) {
		if (radio->rt_cnt[i] < RT_VALIDATE_LIMIT) {
			pr_info("%s rt_cnt %d\n", __func__, radio->rt_cnt[i]);
			rt_cmplt = false;
			return;
		}
		if (radio->rt_tmp0[i] == END_OF_RT)
			break;
	}
	if (rt_cmplt) {
		while ((len < MAX_RT_LEN) && (radio->rt_tmp0[len] != END_OF_RT))
			len++;

		for (i = 0; (i < len) &&
		(radio->rt_display[i] == radio->rt_tmp0[i]); i++)
			;
		if (i == len) {
			pr_info("%s Same RT string repeated\n", __func__);
			return;
		}
		for (i = 0; i < len; i++)
			radio->rt_display[i] = radio->rt_tmp0[i];
		data = kmalloc(len + OFFSET_OF_RT, GFP_ATOMIC);
		if (data != NULL) {
			data[0] = len; /* len of RT */
			data[1] = radio->pty;
			data[2] = (radio->pi >> 8) & 0xFF;
			data[3] = (radio->pi & 0xFF);
			data[4] = radio->rt_flag;
			memcpy(data + OFFSET_OF_RT, radio->rt_display, len);
			data_b = &radio->data_buf[RTC6226_FM_BUF_RT_RDS];
			kfifo_in_locked(data_b, data, OFFSET_OF_RT + len,
				&radio->buf_lock[RTC6226_FM_BUF_RT_RDS]);
			pr_info("%s Q the RT event\n", __func__);
			rtc6226_q_event(radio, RTC6226_EVT_NEW_RT_RDS);
			kfree(data);
		} else {
			pr_err("%s Memory allocation failed for PTY\n",
				__func__);
		}
	}
}

static void rt_handler(struct rtc6226_device *radio, u8 ab_flg,
		u8 cnt, u8 addr, u8 *rt)
{
	u8 i, errcnt, blermax;
	bool rt_txt_chg = false;

	pr_info("%s enter\n", __func__);

	if (ab_flg != radio->rt_flag && radio->valid_rt_flg) {
		for (i = 0; i < sizeof(radio->rt_cnt); i++) {
			if (!radio->rt_tmp0[i]) {
				radio->rt_tmp0[i] = ' ';
				radio->rt_cnt[i]++;
			}
		}
		memset(radio->rt_cnt, 0, sizeof(radio->rt_cnt));
		memset(radio->rt_tmp0, 0, sizeof(radio->rt_tmp0));
		memset(radio->rt_tmp1, 0, sizeof(radio->rt_tmp1));
	}

	radio->rt_flag = ab_flg;
	radio->valid_rt_flg = true;

	for (i = 0; i < cnt; i++) {
		if ((i < 2) && (cnt > 2)) {
			errcnt = radio->bler[2];
			blermax = CORRECTED_THREE_TO_FIVE;
		} else {
			errcnt = radio->bler[3];
			blermax = CORRECTED_THREE_TO_FIVE;
		}
		if (errcnt <= blermax) {
			if (!rt[i])
				rt[i] = ' ';
			if (radio->rt_tmp0[addr+i] == rt[i]) {
				if (radio->rt_cnt[addr+i] < RT_VALIDATE_LIMIT) {
					radio->rt_cnt[addr+i]++;
				} else {
					radio->rt_cnt[addr+i] =
							RT_VALIDATE_LIMIT;
					radio->rt_tmp1[addr+i] = rt[i];
				}
			} else if (radio->rt_tmp1[addr+i] == rt[i]) {
				if (radio->rt_cnt[addr+i] >=
						RT_VALIDATE_LIMIT) {
					rt_txt_chg = true;
					radio->rt_cnt[addr+i] =
						RT_VALIDATE_LIMIT + 1;
				} else {
					radio->rt_cnt[addr+i] =
						RT_VALIDATE_LIMIT;
				}
				radio->rt_tmp1[addr+i] = radio->rt_tmp0[addr+i];
				radio->rt_tmp0[addr+i] = rt[i];
			} else if (!radio->rt_cnt[addr+i]) {
				radio->rt_tmp0[addr+i] = rt[i];
				radio->rt_cnt[addr+i] = 1;
			} else {
				radio->rt_tmp1[addr+i] = rt[i];
			}
		}
	}

	if (rt_txt_chg) {
		for (i = 0; i < MAX_RT_LEN; i++) {
			if (radio->rt_cnt[i] > 1)
				radio->rt_cnt[i]--;
		}
	}
	display_rt(radio);
}

static void rtc6226_raw_rds(struct rtc6226_device *radio)
{
	u16 aid, app_grp_typ;

	aid = radio->block[3];
	app_grp_typ = radio->block[1] & APP_GRP_typ_MASK;
	pr_info("%s app_grp_typ = %x\n", __func__, app_grp_typ);
	pr_info("%s AID = %x\n", __func__, aid);

	switch (aid) {
	case ERT_AID:
		radio->utf_8_flag = (radio->block[2] & 1);
		radio->formatting_dir = EXTRACT_BIT(radio->block[2],
			ERT_FORMAT_DIR_BIT);
		if (radio->ert_carrier != app_grp_typ) {
			rtc6226_q_event(radio, RTC6226_EVT_NEW_ODA);
			radio->ert_carrier = app_grp_typ;
		}
		break;
	case RT_PLUS_AID:
		/*Extract 5th bit of MSB (b7b6b5b4b3b2b1b0)*/
		radio->rt_ert_flag = EXTRACT_BIT(radio->block[2],
				RT_ERT_FLAG_BIT);
		if (radio->rt_plus_carrier != app_grp_typ) {
			rtc6226_q_event(radio, RTC6226_EVT_NEW_ODA);
			radio->rt_plus_carrier = app_grp_typ;
		}
		break;
	default:
		pr_info("%s Not handling the AID of  %x\n", __func__, aid);
		break;
	}
}

static void rtc6226_ev_ert(struct rtc6226_device *radio)
{
	u8 *data = NULL;
	struct kfifo *data_b;

	if (radio->ert_len <= 0)
		return;

	pr_info("%s enter\n", __func__);
	data = kmalloc((radio->ert_len + ERT_OFFSET), GFP_ATOMIC);
	if (data != NULL) {
		data[0] = radio->ert_len;
		data[1] = radio->utf_8_flag;
		data[2] = radio->formatting_dir;
		memcpy((data + ERT_OFFSET), radio->ert_buf, radio->ert_len);
		data_b = &radio->data_buf[RTC6226_FM_BUF_ERT];
		kfifo_in_locked(data_b, data, (radio->ert_len + ERT_OFFSET),
				&radio->buf_lock[RTC6226_FM_BUF_ERT]);
		rtc6226_q_event(radio, RTC6226_EVT_NEW_ERT);
		kfree(data);
	}
}

static void rtc6226_buff_ert(struct rtc6226_device *radio)
{
	int i;
	u16 info_byte = 0;
	u8 byte_pair_index;

	byte_pair_index = radio->block[1] & APP_GRP_typ_MASK;
	if (byte_pair_index == 0) {
		radio->c_byt_pair_index = 0;
		radio->ert_len = 0;
	}
	if (radio->c_byt_pair_index == byte_pair_index) {
		for (i = 2; i <= 3; i++) {
			info_byte = radio->block[i];
			pr_info("%s info_byte = %x\n", __func__, info_byte);
			pr_info("%s ert_len = %x\n", __func__, radio->ert_len);
			if (radio->ert_len > (MAX_ERT_LEN - 2))
				return;
			radio->ert_buf[radio->ert_len] = radio->block[i] >> 8;
			radio->ert_buf[radio->ert_len + 1] =
				radio->block[i] & 0xFF;
			radio->ert_len += ERT_CNT_PER_BLK;
			pr_info("%s utf_8_flag = %d\n", __func__,
				radio->utf_8_flag);
			if ((radio->utf_8_flag == 0) &&
					(info_byte == END_OF_RT)) {
				radio->ert_len -= ERT_CNT_PER_BLK;
				break;
			} else if ((radio->utf_8_flag == 1) &&
					(radio->block[i] >> 8 == END_OF_RT)) {
				info_byte = END_OF_RT;
				radio->ert_len -= ERT_CNT_PER_BLK;
				break;
			} else if ((radio->utf_8_flag == 1) &&
					((radio->block[i] & 0xFF)
					 == END_OF_RT)) {
				info_byte = END_OF_RT;
				radio->ert_len--;
				break;
			}
		}
		if ((byte_pair_index == MAX_ERT_SEGMENT) ||
				(info_byte == END_OF_RT)) {
			rtc6226_ev_ert(radio);
			radio->c_byt_pair_index = 0;
			radio->ert_len = 0;
		}
		radio->c_byt_pair_index++;
	} else {
		radio->ert_len = 0;
		radio->c_byt_pair_index = 0;
	}
}

static void rtc6226_rt_plus(struct rtc6226_device *radio)
{
	u8 tag_type1, tag_type2;
	u8 *data = NULL;
	int len = 0;
	u16 grp_typ;
	struct kfifo *data_b;

	grp_typ = radio->block[1] & APP_GRP_typ_MASK;
	/*
	 *right most 3 bits of Lsb of block 2
	 * and left most 3 bits of Msb of block 3
	 */
	tag_type1 = (((grp_typ & TAG1_MSB_MASK) << TAG1_MSB_OFFSET) |
			(radio->block[2] >> TAG1_LSB_OFFSET));
	/*
	 *right most 1 bit of lsb of 3rd block
	 * and left most 5 bits of Msb of 4th block
	 */
	tag_type2 = (((radio->block[2] & TAG2_MSB_MASK)
				<< TAG2_MSB_OFFSET) |
			(radio->block[2] >> TAG2_LSB_OFFSET));

	if (tag_type1 != DUMMY_CLASS)
		len += RT_PLUS_LEN_1_TAG;
	if (tag_type2 != DUMMY_CLASS)
		len += RT_PLUS_LEN_1_TAG;

	if (len != 0) {
		len += RT_PLUS_OFFSET;
		data = kmalloc(len, GFP_ATOMIC);
	} else {
		pr_err("%s:Len is zero\n", __func__);
		return;
	}
	if (data != NULL) {
		data[0] = len;
		len = RT_ERT_FLAG_OFFSET;
		data[len++] = radio->rt_ert_flag;
		if (tag_type1 != DUMMY_CLASS) {
			data[len++] = tag_type1;
			/*
			 *start position of tag1
			 *right most 5 bits of msb of 3rd block
			 *and left most bit of lsb of 3rd block
			 */
			data[len++] = (radio->block[2] >> TAG1_POS_LSB_OFFSET)
				& TAG1_POS_MSB_MASK;
			/*
			 *length of tag1
			 *left most 6 bits of lsb of 3rd block
			 */
			data[len++] = (radio->block[2] >> TAG1_LEN_OFFSET) &
				TAG1_LEN_MASK;
		}
		if (tag_type2 != DUMMY_CLASS) {
			data[len++] = tag_type2;
			/*
			 *start position of tag2
			 *right most 3 bit of msb of 4th block
			 *and left most 3 bits of lsb of 4th block
			 */
			data[len++] = (radio->block[3] >> TAG2_POS_LSB_OFFSET) &
				TAG2_POS_MSB_MASK;
			/*
			 *length of tag2
			 *right most 5 bits of lsb of 4th block
			 */
			data[len++] = radio->block[3] & TAG2_LEN_MASK;
		}
		data_b = &radio->data_buf[RTC6226_FM_BUF_RT_PLUS];
		kfifo_in_locked(data_b, data, len,
				&radio->buf_lock[RTC6226_FM_BUF_RT_PLUS]);
		rtc6226_q_event(radio, RTC6226_EVT_NEW_RT_PLUS);
		kfree(data);
	} else {
		pr_err("%s:memory allocation failed\n", __func__);
	}
}

void rtc6226_rds_handler(struct work_struct *worker)
{
	struct rtc6226_device *radio;
	u8 rt_blks[NO_OF_RDS_BLKS];
	u8 grp_type, addr, ab_flg;

	radio = container_of(worker, struct rtc6226_device, rds_worker);

	if (!radio) {
		pr_err("%s:radio is null\n", __func__);
		return;
	}

	pr_info("%s enter\n", __func__);

	rtc6226_get_rds(radio);

	if (radio->bler[0] < CORRECTED_THREE_TO_FIVE)
		rtc6226_pi_check(radio, radio->block[0]);

	if (radio->bler[1] < CORRECTED_ONE_TO_TWO) {
		grp_type = radio->block[1] >> OFFSET_OF_GRP_TYP;
		pr_info("%s grp_type = %d\n", __func__, grp_type);
	} else {
		pr_err("%s invalid data %d\n", __func__, radio->bler[1]);
		return;
	}
	if (grp_type & 0x01)
		rtc6226_pi_check(radio, radio->block[2]);

	rtc6226_pty_check(radio, (radio->block[1] >> OFFSET_OF_PTY) & PTY_MASK);

	switch (grp_type) {
	case RDS_TYPE_0A:
		if (radio->bler[2] <= CORRECTED_THREE_TO_FIVE)
			rtc6226_update_af_list(radio);
		/*  fall through */
	case RDS_TYPE_0B:
		addr = (radio->block[1] & PS_MASK) * NO_OF_CHARS_IN_EACH_ADD;
		pr_info("%s RDS is PS\n", __func__);
		if (radio->bler[3] <= CORRECTED_THREE_TO_FIVE) {
			rtc6226_update_ps(radio, addr+0, radio->block[3] >> 8);
			rtc6226_update_ps(radio, addr+1,
				radio->block[3] & 0xff);
		}
		break;
	case RDS_TYPE_2A:
		pr_info("%s RDS is RT 2A group\n", __func__);
		rt_blks[0] = (u8)(radio->block[2] >> 8);
		rt_blks[1] = (u8)(radio->block[2] & 0xFF);
		rt_blks[2] = (u8)(radio->block[3] >> 8);
		rt_blks[3] = (u8)(radio->block[3] & 0xFF);
		addr = (radio->block[1] & 0xf) * 4;
		ab_flg = (radio->block[1] & 0x0010) >> 4;
		rt_handler(radio, ab_flg, CNT_FOR_2A_GRP_RT, addr, rt_blks);
		break;
	case RDS_TYPE_2B:
		pr_info("%s RDS is RT 2B group\n", __func__);
		rt_blks[0] = (u8)(radio->block[3] >> 8);
		rt_blks[1] = (u8)(radio->block[3] & 0xFF);
		rt_blks[2] = 0;
		rt_blks[3] = 0;
		addr = (radio->block[1] & 0xf) * 2;
		ab_flg = (radio->block[1] & 0x0010) >> 4;
		radio->rt_tmp0[MAX_LEN_2B_GRP_RT] = END_OF_RT;
		radio->rt_tmp1[MAX_LEN_2B_GRP_RT] = END_OF_RT;
		radio->rt_cnt[MAX_LEN_2B_GRP_RT] = RT_VALIDATE_LIMIT;
		rt_handler(radio, ab_flg, CNT_FOR_2B_GRP_RT, addr, rt_blks);
		break;
	case RDS_TYPE_3A:
		pr_info("%s RDS is 3A group\n", __func__);
		rtc6226_raw_rds(radio);
		break;
	default:
		pr_err("%s Not handling the group type %d\n", __func__,
			grp_type);
		break;
	}
	pr_info("%s rt_plus_carrier = %x\n", __func__, radio->rt_plus_carrier);
	pr_info("%s ert_carrier = %x\n", __func__, radio->ert_carrier);
	if (radio->rt_plus_carrier && (grp_type == radio->rt_plus_carrier))
		rtc6226_rt_plus(radio);
	else if (radio->ert_carrier && (grp_type == radio->ert_carrier))
		rtc6226_buff_ert(radio);
}

/*
 * rtc6226_rds_on - switch on rds reception
 */
static int rtc6226_rds_on(struct rtc6226_device *radio)
{
	int retval;

	pr_info("%s enter\n", __func__);
	/* sysconfig */
	radio->registers[SYSCFG] |= SYSCFG_CSR0_RDS_EN;
	retval = rtc6226_set_register(radio, SYSCFG);

	if (retval < 0)
		radio->registers[SYSCFG] &= ~SYSCFG_CSR0_RDS_EN;

	return retval;
}

int rtc6226_reset_rds_data(struct rtc6226_device *radio)
{
	mutex_lock(&radio->lock);
	radio->pi = 0;
	/* reset PS bufferes */
	memset(radio->ps_display, 0, sizeof(radio->ps_display));
	memset(radio->ps_tmp0, 0, sizeof(radio->ps_tmp0));
	memset(radio->ps_tmp1, 0, sizeof(radio->ps_tmp1));
	memset(radio->ps_cnt, 0, sizeof(radio->ps_cnt));

	memset(radio->rt_display, 0, sizeof(radio->rt_display));
	memset(radio->rt_tmp0, 0, sizeof(radio->rt_tmp0));
	memset(radio->rt_tmp1, 0, sizeof(radio->rt_tmp1));
	memset(radio->rt_cnt, 0, sizeof(radio->rt_cnt));
	radio->wr_index = 0;
	radio->rd_index = 0;
	memset(radio->buffer, 0, radio->buf_size);
	mutex_unlock(&radio->lock);

	return 0;
}

int rtc6226_set_rssi_threshold(struct rtc6226_device *radio, u16 rssi)
{
	int retval = 0;

	/*csr_rssi_low_th = RSSI_threshold/4*/
	rssi = rssi/4;
	if ((rssi < MIN_RSSI) && (rssi > MAX_RSSI))
		return -EINVAL;
	radio->registers[SEEKCFG1] &= ~SEEKCFG1_CSR0_RSSI_LOW_TH;
	radio->registers[SEEKCFG1] |= rssi << 8;
	retval = rtc6226_set_register(radio, SEEKCFG1);
	radio->rssi_th = (u8)(rssi*4);
	return retval;
}

int rtc6226_power_down(struct rtc6226_device *radio)
{
	int retval = 0;

	pr_info("%s enter\n", __func__);

	mutex_lock(&radio->lock);
		/* stop radio */
	retval = rtc6226_stop(radio);

	//rtc6226_disable_irq(radio);
	mutex_unlock(&radio->lock);
	pr_info("%s exit %d\n", __func__, retval);

	return retval;
}

int rtc6226_power_up(struct rtc6226_device *radio)
{
	int retval = 0;

	mutex_lock(&radio->lock);

	pr_info("%s enter\n", __func__);

	/* start radio */
	retval = rtc6226_start(radio);
	if (retval < 0)
		goto done;
	pr_info("%s : after initialization\n", __func__);

	/* mpxconfig */
	/* Disable Softmute / Disable Mute / De-emphasis / Volume 8 */
	radio->registers[MPXCFG] = 0x0008 |
		MPXCFG_CSR0_DIS_SMUTE | MPXCFG_CSR0_DIS_MUTE |
		((de << 12) & MPXCFG_CSR0_DEEM);
	retval = rtc6226_set_register(radio, MPXCFG);
	if (retval < 0)
		goto done;

	/* enable RDS / STC interrupt */
	radio->registers[SYSCFG] |= SYSCFG_CSR0_RDSIRQEN;
	radio->registers[SYSCFG] |= SYSCFG_CSR0_STDIRQEN;
	/*radio->registers[SYSCFG] |= SYSCFG_CSR0_RDS_EN;*/
	retval = rtc6226_set_register(radio, SYSCFG);
	if (retval < 0)
		goto done;

	radio->registers[PADCFG] &= ~PADCFG_CSR0_GPIO;
	radio->registers[PADCFG] |= 0x1 << 2;
	retval = rtc6226_set_register(radio, PADCFG);
	if (retval < 0)
		goto done;

	/* channel */
	/* Top Frequency 108.0MHZ */
	radio->registers[RADIOSEEKCFG1] = 0x2a30;
	retval = rtc6226_set_register(radio, RADIOSEEKCFG1);
	if (retval < 0)
		goto done;

	/* Bottom Frequency 87.5MHz*/
	radio->registers[RADIOSEEKCFG2] = 0x222e;
	retval = rtc6226_set_register(radio, RADIOSEEKCFG2);
	if (retval < 0)
		goto done;

	/* Space 100KHz */
	/* radio->registers[RADIOCFG] &= ~CHANNEL_CSR0_CHSPACE; */
	radio->registers[RADIOCFG] = 0x0a00;
	retval = rtc6226_set_register(radio, RADIOCFG);
	if (retval < 0)
		goto done;

	/* Default channel 90.1Mhz */
	radio->registers[CHANNEL] = 0x2232;
	retval = rtc6226_set_register(radio, CHANNEL);
	if (retval < 0)
		goto done;

	/* I2S salve */
	radio->registers[I2SCFG] = 0x2480;
	retval = rtc6226_set_register(radio, I2SCFG);
	if (retval < 0)
		goto done;

	/*set default rssi threshold*/
	retval = rtc6226_set_rssi_threshold(radio, DEFAULT_RSSI_TH);
	if (retval < 0)
		pr_err("%s fail to set rssi threshold\n", __func__);

	/* powerconfig */
	/* Enable FM */
	radio->registers[POWERCFG] = POWERCFG_CSR0_ENABLE;
	retval = rtc6226_set_register(radio, POWERCFG);
	if (retval < 0)
		goto done;
	/*wait for radio enable to complete*/
	usleep_range(50, 30000);
	retval = rtc6226_get_all_registers(radio);
	if (retval < 0)
		goto done;

	pr_info("%s : DeviceID=0x%4.4hx ChipID=0x%4.4hx\n", __func__,
		radio->registers[DEVICEID], radio->registers[CHIPID]);
	pr_info("%s : Reg2=0x%4.4hx Reg3=0x%4.4hx\n", __func__,
		radio->registers[MPXCFG], radio->registers[CHANNEL]);
	pr_info("%s : Reg4=0x%4.4hx Reg5=0x%4.4hx\n", __func__,
		radio->registers[SYSCFG], radio->registers[SEEKCFG1]);
	pr_info("%s : Reg6=0x%4.4hx Reg7=0x%4.4hx\n", __func__,
		radio->registers[POWERCFG], radio->registers[PADCFG]);
	pr_info("%s : Reg8=0x%4.4hx Reg9=0x%4.4hx\n", __func__,
		radio->registers[8], radio->registers[9]);
	pr_info("%s : regA=0x%4.4hx RegB=0x%4.4hx\n", __func__,
		radio->registers[10], radio->registers[11]);
	pr_info("%s : regC=0x%4.4hx RegD=0x%4.4hx\n", __func__,
		radio->registers[12], radio->registers[13]);
	pr_info("%s : regE=0x%4.4hx RegF=0x%4.4hx\n", __func__,
		radio->registers[14], radio->registers[15]);

done:
	pr_info("%s exit %d\n", __func__, retval);
	mutex_unlock(&radio->lock);
	return retval;
}

/**************************************************************************
 * File Operations Interface
 **************************************************************************/

/*
 * rtc6226_fops_read - read RDS data
 */
static ssize_t rtc6226_fops_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct rtc6226_device *radio = video_drvdata(file);
	int retval = 0;
	unsigned int block_count = 0;

	/* switch on rds reception */
	mutex_lock(&radio->lock);
	/* if RDS is not on, then turn on RDS */
	if ((radio->registers[SYSCFG] & SYSCFG_CSR0_RDS_EN) == 0)
		rtc6226_rds_on(radio);

	/* block if no new data available */
	while (radio->wr_index == radio->rd_index) {
		if (file->f_flags & O_NONBLOCK) {
			retval = -EWOULDBLOCK;
			goto done;
		}
		if (wait_event_interruptible(radio->read_queue,
				radio->wr_index != radio->rd_index) < 0) {
			retval = -EINTR;
			goto done;
		}
	}

	/* calculate block count from byte count */
	count /= 3;
	#ifdef _RDSDEBUG
	pr_info("%s : count = %zu\n", __func__, count);
	#endif

	/* copy RDS block out of internal buffer and to user buffer */
	while (block_count < count) {
		if (radio->rd_index == radio->wr_index)
			break;
		/* always transfer rds complete blocks */
		if (copy_to_user(buf, &radio->buffer[radio->rd_index], 3))
			/* retval = -EFAULT; */
			break;
		/* increment and wrap read pointer */
		radio->rd_index += 3;
		if (radio->rd_index >= radio->buf_size)
			radio->rd_index = 0;
		/* increment counters */
		block_count++;
		buf += 3;
		retval += 3;
		#ifdef _RDSDEBUG
		pr_info("%s : block_count = %d, count = %zu\n", __func__,
			block_count, count);
		#endif
	}

done:
	mutex_unlock(&radio->lock);
	return retval;
}

/*
 * rtc6226_fops_poll - poll RDS data
 */
static unsigned int rtc6226_fops_poll(struct file *file,
		struct poll_table_struct *pts)
{
	struct rtc6226_device *radio = video_drvdata(file);
	int retval = 0;

	/* switch on rds reception */
	mutex_lock(&radio->lock);
	if ((radio->registers[SYSCFG] & SYSCFG_CSR0_RDS_EN) == 0)
		rtc6226_rds_on(radio);
	mutex_unlock(&radio->lock);

	poll_wait(file, &radio->read_queue, pts);

	if (radio->rd_index != radio->wr_index)
		retval = POLLIN | POLLRDNORM;

	return retval;
}

/* static */
int rtc6226_vidioc_g_ctrl(struct file *file, void *priv,
	struct v4l2_control *ctrl)
{
	struct rtc6226_device *radio = video_drvdata(file);
	int retval = 0;

	pr_info("%s enter, ctrl->id: %x, value:%d\n", __func__,
		ctrl->id, ctrl->value);

	mutex_lock(&radio->lock);

	switch (ctrl->id) {
	case V4L2_CID_PRIVATE_CSR0_ENABLE:
		pr_info("V4L2_CID_PRIVATE_CSR0_ENABLE val=%d\n", ctrl->value);
		break;
	case V4L2_CID_PRIVATE_CSR0_DISABLE:
		pr_info("V4L2_CID_PRIVATE_CSR0_DISABLE val=%d\n", ctrl->value);
		break;
	case V4L2_CID_PRIVATE_CSR0_VOLUME:
	case V4L2_CID_AUDIO_VOLUME:
		ctrl->value = radio->registers[MPXCFG] & MPXCFG_CSR0_VOLUME;
		break;
	case V4L2_CID_PRIVATE_CSR0_DIS_MUTE:
	case V4L2_CID_AUDIO_MUTE:
		ctrl->value = ((radio->registers[MPXCFG] &
			MPXCFG_CSR0_DIS_MUTE) == 0) ? 1 : 0;
		break;
	case V4L2_CID_PRIVATE_CSR0_DIS_SMUTE:
		ctrl->value = ((radio->registers[MPXCFG] &
			MPXCFG_CSR0_DIS_SMUTE) == 0) ? 1 : 0;
		break;
	case V4L2_CID_PRIVATE_CSR0_BAND:
		ctrl->value = radio->band;
		break;
	case V4L2_CID_PRIVATE_CSR0_SEEKRSSITH:
		ctrl->value = radio->registers[SEEKCFG1] &
			SEEKCFG1_CSR0_RSSI_LOW_TH;
		break;
	case V4L2_CID_PRIVATE_RSSI:
		rtc6226_get_all_registers(radio);
		ctrl->value = radio->registers[RSSI] & RSSI_RSSI;
		pr_info("Get V4L2_CONTROL V4L2_CID_PRIVATE_RSSI: STATUS=0x%4.4hx RSSI = %d\n",
			radio->registers[STATUS],
			radio->registers[RSSI] & RSSI_RSSI);
		pr_info("Get V4L2_CONTROL V4L2_CID_PRIVATE_RSSI: regC=0x%4.4hx RegD=0x%4.4hx\n",
			radio->registers[BA_DATA], radio->registers[BB_DATA]);
		pr_info("Get V4L2_CONTROL V4L2_CID_PRIVATE_RSSI: regE=0x%4.4hx RegF=0x%4.4hx\n",
			radio->registers[BC_DATA], radio->registers[BD_DATA]);
		break;
	case V4L2_CID_PRIVATE_DEVICEID:
		ctrl->value = radio->registers[DEVICEID] & DEVICE_ID;
		pr_info("Get V4L2_CONTROL V4L2_CID_PRIVATE_DEVICEID: DEVICEID=0x%4.4hx\n",
			radio->registers[DEVICEID]);
		break;
	case V4L2_CID_PRIVATE_RTC6226_RDSGROUP_PROC:
		break;
	case V4L2_CID_PRIVATE_RTC6226_SIGNAL_TH:
	/* intentional fallthrough */
	case V4L2_CID_PRIVATE_RTC6226_RSSI_TH:
		ctrl->value = radio->rssi_th;
		break;
	default:
		pr_info("%s in default id:%d\n", __func__, ctrl->id);
		retval = -EINVAL;
	}

	mutex_unlock(&radio->lock);
	return retval;
}

static int rtc6226_vidioc_dqbuf(struct file *file, void *priv,
		struct v4l2_buffer *buffer)
{

	struct rtc6226_device *radio = video_get_drvdata(video_devdata(file));
	enum rtc6226_buf_t buf_type = -1;
	u8 buf_fifo[STD_BUF_SIZE] = {0};
	struct kfifo *data_fifo = NULL;
	u8 *buf = NULL;
	int len = 0, retval = -1;

	if ((radio == NULL) || (buffer == NULL)) {
		pr_err("%s radio/buffer is NULL\n", __func__);
		return -ENXIO;
	}

	buf_type = buffer->index;
	buf = (u8 *)buffer->m.userptr;
	len = buffer->length;
	pr_info("%s: requesting buffer %d\n", __func__, buf_type);

	if ((buf_type < RTC6226_FM_BUF_MAX) && (buf_type >= 0)) {
		data_fifo = &radio->data_buf[buf_type];
		if (buf_type == RTC6226_FM_BUF_EVENTS) {
			if (wait_event_interruptible(radio->event_queue,
						kfifo_len(data_fifo)) < 0) {
				return -EINTR;
			}
		}
	} else {
		pr_err("%s invalid buffer type\n", __func__);
		return -EINVAL;
	}
	if (len <= STD_BUF_SIZE) {
		buffer->bytesused = kfifo_out_locked(data_fifo, &buf_fifo[0],
				len, &radio->buf_lock[buf_type]);
	} else {
		pr_err("%s kfifo_out_locked can not use len more than 128\n",
			__func__);
		return -EINVAL;
	}
	retval = copy_to_user(buf, &buf_fifo[0], buffer->bytesused);
	if (retval > 0) {
		pr_err("%s Failed to copy %d bytes data\n", __func__, retval);
		return -EAGAIN;
	}

	return retval;
}

static bool check_mode(struct rtc6226_device *radio)
{
	bool retval = true;

	if (radio->mode == FM_OFF || radio->mode == FM_RECV)
		retval = false;

	return retval;
}



static int rtc6226_disable(struct rtc6226_device *radio)
{
	int retval = 0;

	/* disable RDS/STC interrupt */
	radio->registers[SYSCFG] &= ~SYSCFG_CSR0_RDS_EN;
	radio->registers[SYSCFG] &= ~SYSCFG_CSR0_RDSIRQEN;
	radio->registers[SYSCFG] &= ~SYSCFG_CSR0_STDIRQEN;
	retval = rtc6226_set_register(radio, SYSCFG);
	if (retval < 0) {
		pr_err("%s fail to disable RDS/SCT interrupt\n", __func__);
		goto done;
	}
	retval = rtc6226_power_down(radio);
	if (retval < 0) {
		pr_err("%s fail to turn off fmradio\n", __func__);
		goto done;
	}

	if (radio->mode == FM_TURNING_OFF || radio->mode == FM_RECV) {
		pr_info("%s: posting RTC6226_EVT_RADIO_DISABLED event\n",
				__func__);
		rtc6226_q_event(radio, RTC6226_EVT_RADIO_DISABLED);
		radio->mode = FM_OFF;
	}
	/* flush_workqueue(radio->wqueue); */

done:
	return retval;
}

static int rtc6226_enable(struct rtc6226_device *radio)
{
	int retval = 0;

	rtc6226_get_register(radio, POWERCFG);
	retval = rtc6226_power_up(radio);
	if (retval < 0)
		goto done;

	if ((radio->registers[SYSCFG] &  SYSCFG_CSR0_STDIRQEN) == 0) {
		radio->registers[SYSCFG] |= SYSCFG_CSR0_RDSIRQEN;
		radio->registers[SYSCFG] |= SYSCFG_CSR0_STDIRQEN;
		retval = rtc6226_set_register(radio, SYSCFG);
		if (retval < 0) {
			pr_err("%s set register fail\n", __func__);
			goto done;
		} else {
			rtc6226_q_event(radio, RTC6226_EVT_RADIO_READY);
			radio->mode = FM_RECV;
		}
	} else {
		rtc6226_q_event(radio, RTC6226_EVT_RADIO_READY);
		radio->mode = FM_RECV;
	}
done:
	return retval;

}

bool rtc6226_is_valid_srch_mode(int srch_mode)
{
	if ((srch_mode >= RTC6226_MIN_SRCH_MODE) &&
			(srch_mode <= RTC6226_MAX_SRCH_MODE))
		return true;
	else
		return false;
}

/*
 * rtc6226_vidioc_s_ctrl - set the value of a control
 */
int rtc6226_vidioc_s_ctrl(struct file *file, void *priv,
	struct v4l2_control *ctrl)
{
	struct rtc6226_device *radio = video_drvdata(file);
	int retval = 0;
	int space_s = 0;

	pr_info("%s enter, ctrl->id: %x, value:%d\n", __func__,
		ctrl->id, ctrl->value);

	switch (ctrl->id) {
	case V4L2_CID_PRIVATE_RTC6226_STATE:
		if (ctrl->value == FM_RECV) {
			if (check_mode(radio)) {
				pr_err("%s:fm is not in proper state\n",
						__func__);
				retval = -EINVAL;
				goto end;
			}
			radio->mode = FM_RECV_TURNING_ON;
			retval = rtc6226_enable(radio);
			if (retval < 0) {
				pr_err(
				"%s Error while enabling RECV FM %d\n",
					__func__, retval);
				radio->mode = FM_OFF;
				goto end;
			}
		} else if (ctrl->value == FM_OFF) {
			radio->mode = FM_TURNING_OFF;
			retval = rtc6226_disable(radio);
			if (retval < 0) {
				pr_err("Err on disable recv FM %d\n", retval);
				radio->mode = FM_RECV;
				goto end;
			}
		}
		break;
	case V4L2_CID_PRIVATE_RTC6226_SET_AUDIO_PATH:
	case V4L2_CID_PRIVATE_RTC6226_SRCH_ALGORITHM:
	case V4L2_CID_PRIVATE_RTC6226_REGION:
		retval = 0;
		break;
	case V4L2_CID_PRIVATE_RTC6226_EMPHASIS:
		if (ctrl->value == 1)
			radio->registers[MPXCFG] |= MPXCFG_CSR0_DEEM;
		else
			radio->registers[MPXCFG] &= ~MPXCFG_CSR0_DEEM;
		retval = rtc6226_set_register(radio, MPXCFG);
		break;
	case V4L2_CID_PRIVATE_RTC6226_RDS_STD:
		/* enable RDS / STC interrupt */
		radio->registers[SYSCFG] |= SYSCFG_CSR0_RDSIRQEN;
		radio->registers[SYSCFG] |= SYSCFG_CSR0_STDIRQEN;
		/*radio->registers[SYSCFG] |= SYSCFG_CSR0_RDS_EN;*/
		retval = rtc6226_set_register(radio, SYSCFG);
		break;
	case V4L2_CID_PRIVATE_RTC6226_SPACING:
		space_s = ctrl->value;
		radio->space = ctrl->value;
		radio->registers[CHANNEL] &= ~CHANNEL_CSR0_CHSPACE;
		radio->registers[CHANNEL] |= (space_s << 10);
		retval = rtc6226_set_register(radio, CHANNEL);
		break;
	case V4L2_CID_PRIVATE_RTC6226_SRCHON:
		rtc6226_search(radio, (bool)ctrl->value);
		break;
	case V4L2_CID_PRIVATE_RTC6226_LP_MODE:
		if (ctrl->value) {
			/* disable RDS interrupts */
			radio->registers[SYSCFG] &= ~SYSCFG_CSR0_RDSIRQEN;
			retval = rtc6226_set_register(radio, SYSCFG);
		} else {
			/* enable RDS interrupts */
			radio->registers[SYSCFG] |= SYSCFG_CSR0_RDSIRQEN;
			retval = rtc6226_set_register(radio, SYSCFG);
		}
		break;
	case V4L2_CID_PRIVATE_RTC6226_ANTENNA:
	case V4L2_CID_PRIVATE_RTC6226_AF_JUMP:
	case V4L2_CID_PRIVATE_RTC6226_SRCH_CNT:
	case V4L2_CID_PRIVATE_RTC6226_RXREPEATCOUNT:
	case V4L2_CID_PRIVATE_RTC6226_SINR_THRESHOLD:
		retval = 0;
		break;
	case V4L2_CID_PRIVATE_RTC6226_SIGNAL_TH:
		retval = rtc6226_set_rssi_threshold(radio, ctrl->value);
		if (retval < 0)
			pr_err("%s fail to set rssi threshold\n", __func__);
		rtc6226_get_register(radio, SEEKCFG1);
		pr_info("FMRICHWAVE RSSI_TH: Dec = %d , Hexa = %x\n",
			radio->registers[SEEKCFG1] & 0xFF,
			radio->registers[SEEKCFG1] & 0xFF);
		break;
	/* case V4L2_CID_PRIVATE_RTC6226_OFS_THRESHOLD: */
	case V4L2_CID_PRIVATE_RTC6226_SPUR_FREQ_RMSSI:
		break;
	case V4L2_CID_PRIVATE_RTC6226_RDSD_BUF:
	case V4L2_CID_PRIVATE_RTC6226_RDSGROUP_MASK:
	case V4L2_CID_PRIVATE_RTC6226_RDSGROUP_PROC:
		if ((radio->registers[SYSCFG] & SYSCFG_CSR0_RDS_EN) == 0)
			rtc6226_rds_on(radio);
		retval = 0;
		break;
	case V4L2_CID_PRIVATE_RTC6226_SRCHMODE:
		if (rtc6226_is_valid_srch_mode(ctrl->value)) {
			radio->g_search_mode = ctrl->value;
		} else {
			pr_err("%s:srch mode is not valid\n", __func__);
			retval = -EINVAL;
			goto end;
		}
		break;
	case V4L2_CID_PRIVATE_RTC6226_PSALL:
		break;
	case V4L2_CID_PRIVATE_RTC6226_SCANDWELL:
		if ((ctrl->value >= MIN_DWELL_TIME) &&
				(ctrl->value <= MAX_DWELL_TIME)) {
			radio->dwell_time_sec = ctrl->value;
		} else {
			pr_err(
			"%s:scandwell period is not valid\n", __func__);
			retval = -EINVAL;
		}
		break;
	case V4L2_CID_PRIVATE_CSR0_ENABLE:
		pr_info("V4L2_CID_PRIVATE_CSR0_ENABLE val=%d\n",
			ctrl->value);
		retval = rtc6226_power_up(radio);
		/* must keep below line */
		ctrl->value = 0;
		break;
	case V4L2_CID_PRIVATE_CSR0_DISABLE:
		pr_info("V4L2_CID_PRIVATE_CSR0_DISABLE val=%d\n",
			ctrl->value);
		retval = rtc6226_power_down(radio);
		/* must keep below line */
		ctrl->value = 0;
		break;
	case V4L2_CID_PRIVATE_DEVICEID:
		pr_info("V4L2_CID_PRIVATE_DEVICEID val=%d\n", ctrl->value);
		break;
	case V4L2_CID_PRIVATE_CSR0_VOLUME:
	case V4L2_CID_AUDIO_VOLUME:
		pr_info("MPXCFG=0x%4.4hx POWERCFG=0x%4.4hx\n",
		radio->registers[MPXCFG], radio->registers[POWERCFG]);
		radio->registers[MPXCFG] &= ~MPXCFG_CSR0_VOLUME;
		radio->registers[MPXCFG] |=
			(ctrl->value > 15) ? 8 : ctrl->value;
		pr_info("MPXCFG=0x%4.4hx POWERCFG=0x%4.4hx\n",
		radio->registers[MPXCFG], radio->registers[POWERCFG]);
		retval = rtc6226_set_register(radio, MPXCFG);
		break;
	case V4L2_CID_PRIVATE_CSR0_DIS_MUTE:
	case V4L2_CID_AUDIO_MUTE:
		if (ctrl->value == 1)
			radio->registers[MPXCFG] &= ~MPXCFG_CSR0_DIS_MUTE;
		else
			radio->registers[MPXCFG] |= MPXCFG_CSR0_DIS_MUTE;
		retval = rtc6226_set_register(radio, MPXCFG);
		break;
	case V4L2_CID_PRIVATE_RTC6226_SOFT_MUTE:
		pr_info("V4L2_CID_PRIVATE_RTC6226_SOFT_MUTE\n");
		if (ctrl->value == 1)
			radio->registers[MPXCFG] &= ~MPXCFG_CSR0_DIS_SMUTE;
		else
			radio->registers[MPXCFG] |= MPXCFG_CSR0_DIS_SMUTE;
		retval = rtc6226_set_register(radio, MPXCFG);
		break;
	case V4L2_CID_PRIVATE_CSR0_DEEM:
		pr_info("V4L2_CID_PRIVATE_CSR0_DEEM\n");
		if (ctrl->value == 1)
			radio->registers[MPXCFG] |= MPXCFG_CSR0_DEEM;
		else
			radio->registers[MPXCFG] &= ~MPXCFG_CSR0_DEEM;
		retval = rtc6226_set_register(radio, MPXCFG);
		break;
	case V4L2_CID_PRIVATE_CSR0_BLNDADJUST:
		pr_info("V4L2_CID_PRIVATE_CSR0_BLNDADJUST val=%d\n",
				ctrl->value);
		break;
	case V4L2_CID_PRIVATE_CSR0_BAND:
		pr_info(
		"V4L2_CID_PRIVATE_CSR0_BAND : FREQ_TOP=%d FREQ_BOT=%d %d\n",
			radio->registers[RADIOSEEKCFG1],
			radio->registers[RADIOSEEKCFG2], ctrl->value);
		switch (ctrl->value) {
		case FMBAND_87_108_MHZ:
			radio->registers[RADIOSEEKCFG1] = 10800;
			radio->registers[RADIOSEEKCFG2] = 8750;
			break;
		case FMBAND_76_108_MHZ:
			radio->registers[RADIOSEEKCFG1] = 10800;
			radio->registers[RADIOSEEKCFG2] = 7600;
			break;
		case FMBAND_76_91_MHZ:
			radio->registers[RADIOSEEKCFG1] = 9100;
			radio->registers[RADIOSEEKCFG2] = 7600;
			break;
		case FMBAND_64_76_MHZ:
			radio->registers[RADIOSEEKCFG1] = 7600;
			radio->registers[RADIOSEEKCFG2] = 6400;
			break;
		default:
			retval = -EINVAL;
			break;
		}
		pr_info(
		"V4L2_CID_PRIVATE_CSR0_BAND : FREQ_TOP=%d FREQ_BOT=%d %d\n",
			radio->registers[RADIOSEEKCFG1],
			radio->registers[RADIOSEEKCFG2], ctrl->value);
		radio->band = ctrl->value;
		retval = rtc6226_set_register(radio, RADIOSEEKCFG1);
		retval = rtc6226_set_register(radio, RADIOSEEKCFG2);
		break;
	case V4L2_CID_PRIVATE_CSR0_CHSPACE:
		pr_info("V4L2_CID_PRIVATE_CSR0_CHSPACE : FM_SPACE=%d %d\n",
			radio->registers[RADIOCFG], ctrl->value);
		switch (ctrl->value) {
		case FMSPACE_200_KHZ:
			radio->registers[RADIOCFG] = 20;
			break;
		case FMSPACE_100_KHZ:
			radio->registers[RADIOCFG] = 10;
			break;
		case FMSPACE_50_KHZ:
			radio->registers[RADIOCFG] = 5;
			break;
		default:
			retval = -EINVAL;
			break;
		}
		radio->space = ctrl->value;
		pr_info("V4L2_CID_PRIVATE_CSR0_CHSPACE : FM_SPACE=%d %d\n",
			radio->registers[RADIOCFG], ctrl->value);
		retval = rtc6226_set_register(radio, RADIOCFG);
		break;
	case V4L2_CID_PRIVATE_CSR0_DIS_AGC:
		pr_info("V4L2_CID_PRIVATE_CSR0_DIS_AGC val=%d\n",
			ctrl->value);
		break;
	case V4L2_CID_PRIVATE_RTC6226_RDSON:
		pr_info(
		"V4L2_CSR0_RDS_EN:CHANNEL=0x%4.4hx SYSCFG=0x%4.4hx\n",
			radio->registers[CHANNEL],
			radio->registers[SYSCFG]);
		rtc6226_reset_rds_data(radio);
		radio->registers[SYSCFG] &= ~SYSCFG_CSR0_RDS_EN;
		radio->registers[SYSCFG] &= ~SYSCFG_CSR0_RDSIRQEN;
		radio->registers[SYSCFG] |= (ctrl->value << 15);
		radio->registers[SYSCFG] |= (ctrl->value << 12);
		pr_info
		("V4L2_CSR0_RDS_EN : CHANNEL=0x%4.4hx SYSCFG=0x%4.4hx\n",
			radio->registers[CHANNEL],
			radio->registers[SYSCFG]);
		retval = rtc6226_set_register(radio, SYSCFG);
		break;
	case V4L2_CID_PRIVATE_SEEK_CANCEL:
		rtc6226_search(radio, (bool)ctrl->value);
		break;
	case V4L2_CID_PRIVATE_CSR0_SEEKRSSITH:
		radio->registers[SEEKCFG1] &= ~SEEKCFG1_CSR0_RSSI_LOW_TH;
		radio->registers[SEEKCFG1] |= ctrl->value;
		retval = rtc6226_set_register(radio, SEEKCFG1);
		break;
	default:
		pr_info("%s id: %x in default\n", __func__, ctrl->id);
		retval = -EINVAL;
		break;
	}

end:
	pr_info("%s exit id: %x , ret: %d\n", __func__, ctrl->id, retval);

	return retval;
}

/*
 * rtc6226_vidioc_g_audio - get audio attributes
 */
static int rtc6226_vidioc_g_audio(struct file *file, void *priv,
	struct v4l2_audio *audio)
{
	/* driver constants */
	audio->index = 0;
	strlcpy(audio->name, "Radio", sizeof(audio->name));
	audio->capability = V4L2_AUDCAP_STEREO;
	audio->mode = 0;

	return 0;
}


/*
 * rtc6226_vidioc_g_tuner - get tuner attributes
 */
static int rtc6226_vidioc_g_tuner(struct file *file, void *priv,
	struct v4l2_tuner *tuner)
{
	struct rtc6226_device *radio = video_drvdata(file);
	int retval = 0;

	pr_info("%s enter\n", __func__);

	if (tuner->index != 0) {
		retval = -EINVAL;
		goto done;
	}

	retval = rtc6226_get_register(radio, RSSI);
	if (retval < 0)
		goto done;

	/* driver constants */
	strlcpy(tuner->name, "FM", sizeof(tuner->name));
	tuner->type = V4L2_TUNER_RADIO;
	tuner->capability = V4L2_TUNER_CAP_LOW | V4L2_TUNER_CAP_STEREO |
		V4L2_TUNER_CAP_RDS | V4L2_TUNER_CAP_RDS_BLOCK_IO;

	tuner->rangehigh = (radio->registers[RADIOSEEKCFG1] &
		CHANNEL_CSR0_FREQ_TOP) * TUNE_STEP_SIZE * TUNE_PARAM;
	tuner->rangelow = (radio->registers[RADIOSEEKCFG2] &
		CHANNEL_CSR0_FREQ_BOT) * TUNE_STEP_SIZE * TUNE_PARAM;

	pr_debug("%s low:%d high:%d\n", __func__,
		tuner->rangelow, tuner->rangehigh);
	/* stereo indicator == stereo (instead of mono) */
	if ((radio->registers[STATUS] & STATUS_SI) == 0)
		tuner->rxsubchans = V4L2_TUNER_SUB_MONO;
	else
		tuner->rxsubchans = V4L2_TUNER_SUB_MONO | V4L2_TUNER_SUB_STEREO;
	/* If there is a reliable method of detecting an RDS channel,
	 * then this code should check for that before setting this
	 * RDS subchannel.
	 */
	tuner->rxsubchans |= V4L2_TUNER_SUB_RDS;

	/* mono/stereo selector */
	if ((radio->registers[MPXCFG] & MPXCFG_CSR0_MONO) == 0) {
		tuner->audmode = V4L2_TUNER_MODE_STEREO;
		rtc6226_q_event(radio, RTC6226_EVT_STEREO);
	} else {
		tuner->audmode = V4L2_TUNER_MODE_MONO;
		rtc6226_q_event(radio, RTC6226_EVT_MONO);
	}

	/* min is worst, max is best; rssi: 0..0xff */
	tuner->signal = (radio->registers[RSSI] & RSSI_RSSI);

done:
	pr_info("%s exit %d\n",	__func__, retval);

	return retval;
}


/*
 * rtc6226_vidioc_s_tuner - set tuner attributes
 */
static int rtc6226_vidioc_s_tuner(struct file *file, void *priv,
	const struct v4l2_tuner *tuner)
{
	struct rtc6226_device *radio = video_drvdata(file);
	int retval = 0;
	u16 bottom_freq;
	u16 top_freq;

	pr_info("%s entry\n", __func__);

	if (tuner->index != 0) {
		pr_info("%s index :%d\n", __func__, tuner->index);
		goto done;
	}

	/* mono/stereo selector */
	switch (tuner->audmode) {
	case V4L2_TUNER_MODE_MONO:
		radio->registers[MPXCFG] |= MPXCFG_CSR0_MONO;  /* force mono */
		break;
	case V4L2_TUNER_MODE_STEREO:
		radio->registers[MPXCFG] &= ~MPXCFG_CSR0_MONO; /* try stereo */
		break;
	default:
		pr_debug("%s audmode is not set\n", __func__);
	}

	retval = rtc6226_set_register(radio, MPXCFG);

	/*  unit is 10kHz */
	top_freq = (u16)((tuner->rangehigh / TUNE_PARAM) / TUNE_STEP_SIZE);
	bottom_freq = (u16)((tuner->rangelow / TUNE_PARAM) / TUNE_STEP_SIZE);

	pr_debug("%s low:%d high:%d\n", __func__,
		bottom_freq, top_freq);

	radio->registers[RADIOSEEKCFG1] = top_freq;
	radio->registers[RADIOSEEKCFG2] = bottom_freq;

	retval = rtc6226_set_register(radio, RADIOSEEKCFG1);
	if (retval < 0)
		pr_err("In %s, error %d setting higher limit freq\n",
			__func__, retval);
	else
		radio->recv_conf.band_high_limit = top_freq;

	retval = rtc6226_set_register(radio, RADIOSEEKCFG2);
	if (retval < 0)
		pr_err("In %s, error %d setting lower limit freq\n",
			__func__, retval);
	else
		radio->recv_conf.band_low_limit = bottom_freq;
done:
	pr_info("%s exit %d\n", __func__, retval);
	return retval;
}


/*
 * rtc6226_vidioc_g_frequency - get tuner or modulator radio frequency
 */
static int rtc6226_vidioc_g_frequency(struct file *file, void *priv,
	struct v4l2_frequency *freq)
{
	struct rtc6226_device *radio = video_drvdata(file);
	int retval = 0;
	unsigned int frq;

	pr_info("%s enter freq %d\n", __func__, freq->frequency);

	freq->type = V4L2_TUNER_RADIO;
	retval = rtc6226_get_freq(radio, &frq);
	freq->frequency = frq * TUNE_PARAM;
	radio->tuned_freq_khz = frq * TUNE_STEP_SIZE;
	pr_info(" %s *freq=%d, ret %d\n", __func__, freq->frequency, retval);

	if (retval < 0)
		pr_err(" %s get frequency failed with %d\n", __func__, retval);

	return retval;
}


/*
 * rtc6226_vidioc_s_frequency - set tuner or modulator radio frequency
 */
static int rtc6226_vidioc_s_frequency(struct file *file, void *priv,
	const struct v4l2_frequency *freq)
{
	struct rtc6226_device *radio = video_drvdata(file);
	int retval = 0;
	u32 f = 0;

	pr_info("%s enter freq = %d\n", __func__, freq->frequency);
	if (unlikely(freq == NULL)) {
		pr_err("%s:freq is null\n", __func__);
		return -EINVAL;
	}
	if (freq->type != V4L2_TUNER_RADIO)
		return -EINVAL;
	f = (freq->frequency)/TUNE_PARAM;

	radio->seek_tune_status = TUNE_PENDING;
	retval = rtc6226_set_freq(radio, f);
	if (retval < 0)
		pr_err("%s set frequency failed with %d\n", __func__, retval);
	else
		radio->tuned_freq_khz = f;

	return retval;
}


/*
 * rtc6226_vidioc_s_hw_freq_seek - set hardware frequency seek
 */
static int rtc6226_vidioc_s_hw_freq_seek(struct file *file, void *priv,
	const struct v4l2_hw_freq_seek *seek)
{
	struct rtc6226_device *radio = video_drvdata(file);
	int retval = 0;

	pr_info("%s enter\n", __func__);

	if (file->f_flags & O_NONBLOCK)
		return -EWOULDBLOCK;

	radio->is_search_cancelled = false;

	if (radio->g_search_mode == SEEK) {
		/* seek */
		pr_info("%s starting seek\n", __func__);
		radio->seek_tune_status = SEEK_PENDING;
		retval = rtc6226_set_seek(radio, seek->seek_upward,
				WRAP_ENABLE);
	} else if ((radio->g_search_mode == SCAN) ||
			(radio->g_search_mode == SCAN_FOR_STRONG)) {
		/* scan */
		if (radio->g_search_mode == SCAN_FOR_STRONG) {
			pr_info("%s starting search list\n", __func__);
			memset(&radio->srch_list, 0,
					sizeof(struct rtc6226_srch_list_compl));
		} else {
			pr_info("%s starting scan\n", __func__);
		}
		rtc6226_search(radio, START_SCAN);
	} else {
		retval = -EINVAL;
		pr_err("In %s, invalid search mode %d\n",
				__func__, radio->g_search_mode);
	}
	pr_info("%s exit %d\n", __func__, retval);
	return retval;
}

static int rtc6226_vidioc_g_fmt_type_private(struct file *file, void *priv,
						struct v4l2_format *f)
{
	return 0;
}

static const struct v4l2_file_operations rtc6226_fops = {
	.owner			= THIS_MODULE,
	.unlocked_ioctl	= video_ioctl2,
#ifdef CONFIG_COMPAT
	.compat_ioctl32	= v4l2_compat_ioctl32,
#endif
	.read			= rtc6226_fops_read,
	.poll			= rtc6226_fops_poll,
	.open			= rtc6226_fops_open,
	.release		= rtc6226_fops_release,
};

/*
 * rtc6226_ioctl_ops - video device ioctl operations
 */
/* static */
const struct v4l2_ioctl_ops rtc6226_ioctl_ops = {
	.vidioc_querycap            =   rtc6226_vidioc_querycap,
	.vidioc_g_audio             =   rtc6226_vidioc_g_audio,
	.vidioc_g_tuner             =   rtc6226_vidioc_g_tuner,
	.vidioc_s_tuner             =   rtc6226_vidioc_s_tuner,
	.vidioc_g_ctrl              =   rtc6226_vidioc_g_ctrl,
	.vidioc_s_ctrl              =   rtc6226_vidioc_s_ctrl,
	.vidioc_g_frequency         =   rtc6226_vidioc_g_frequency,
	.vidioc_s_frequency         =   rtc6226_vidioc_s_frequency,
	.vidioc_s_hw_freq_seek      =   rtc6226_vidioc_s_hw_freq_seek,
	.vidioc_dqbuf               =   rtc6226_vidioc_dqbuf,
	.vidioc_g_fmt_type_private  =	rtc6226_vidioc_g_fmt_type_private,
};

/*
 * rtc6226_viddev_template - video device interface
 */
struct video_device rtc6226_viddev_template = {
	.fops           =   &rtc6226_fops,
	.name           =   DRIVER_NAME,
	.release        =   video_device_release_empty,
	.ioctl_ops      =   &rtc6226_ioctl_ops,
};

/**************************************************************************
 * Module Interface
 **************************************************************************/

/*
 * rtc6226_i2c_init - module init
 */
static __init int rtc6226_init(void)
{
	pr_info(DRIVER_DESC ", Version " DRIVER_VERSION "\n");
	return rtc6226_i2c_init();
}

/*
 * rtc6226_i2c_exit - module exit
 */
static void __exit rtc6226_exit(void)
{
	i2c_del_driver(&rtc6226_i2c_driver);
}

module_init(rtc6226_init);
module_exit(rtc6226_exit);
