
#ifndef __DRV2624_H__
#define __DRV2624_H__
/*
** =============================================================================
** Copyright (c)2016  Texas Instruments Inc.
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
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
** File:
**     drv2624.h
**
** Description:
**     Header file for drv2624.c
**
** =============================================================================
*/

#include <linux/regmap.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/firmware.h>
//#define ANDROID
//#define ANDROID_TIMED_OUTPUT
#ifdef ANDROID
#include <../../../drivers/staging/android/timed_output.h>
#include <linux/wakelock.h>
#elif defined ANDROID_TIMED_OUTPUT
#include "timed_output.h"
#endif /*  */

#define HAPTICS_DEVICE_NAME "drv2624"
#define NEED_RELOAD_FIRMWARE	0
#define	DRV2624_REG_ID				0x00
#define DRV2624_ID_MASK                         0xf0
#define	DRV2624_ID				(0x02&DRV2624_ID_MASK)
#define	DRV2624_SW_VERSION		"1.0.2020.4.9" //specific some effectID as square wave

#define	DRV2624_REG_STATUS			0x01
#define	DIAG_MASK				0x80
#define	DIAG_SUCCESS				0x00
#define	DIAG_SHIFT				0x07
#define	INT_MASK				0x1f
#define	PRG_ERR_MASK				0x10
#define	PROCESS_DONE_MASK			0x08
#define	ULVO_MASK				0x04
#define	OVERTEMPRATURE_MASK			0x02
#define	OVERCURRENT_MASK			0x01

#define	DRV2624_REG_INT_ENABLE			0x02
#define	INT_MASK_ALL				0x1f
#define	INT_ENABLE_ALL				0x00
#define	INT_ENABLE_CRITICAL			0x08

#define	DRV2624_REG_DIAG_Z			0x03

#define	DRV2624_REG_MODE			0x07
#define	WORKMODE_MASK				0x03
#define	MODE_RTP				0x00
#define	MODE_WAVEFORM_SEQUENCER			0x01
#define	MODE_DIAGNOSTIC				0x02
#define	MODE_CALIBRATION			0x03
#define	PINFUNC_MASK				0x0c
#define	PINFUNC_INT				0x02
#define	PINFUNC_SHIFT				0x02
#define DRV2624_CALIBRATION_MODE_CFG            0x4B

#define	DRV2624_REG_CONTROL1			0x08
#define DRV2624_AUTO_BRK_INTO_STBY_MASK         (0x01 << 3)
#define DRV2624_STBY_MODE_WITH_AUTO_BRAKE       (0x01 << 3)
#define DRV2624_STBY_MODE_WITHOUT_AUTO_BRAKE    0x00
#define DRV2624_REMOVE_STBY_MODE                0x00

#define	ACTUATOR_MASK				0x80
#define	ACTUATOR_SHIFT				7
#define	LOOP_MASK				0x40
#define	LOOP_SHIFT				6
#define	AUTOBRK_OK_MASK				0x10
#define	AUTOBRK_OK_ENABLE			0x10

#define	DRV2624_REG_GO				0x0c
#define DRV2624_GO_BIT_MASK                     0x01

#define	DRV2624_REG_CONTROL2			0x0d
#define	LIB_LRA					0x00
#define	LIB_ERM					0x01
#define	LIB_MASK				0x80
#define	LIB_SHIFT				0x07
#define	SCALE_MASK				0x03
#define	INTERVAL_MASK				0x20
#define	INTERVAL_SHIFT				0x05

#define	DRV2624_REG_RTP_INPUT		0x0e
#define	DRV2624_REG_SEQUENCER_1		0x0f
#define	DRV2624_REG_SEQ_LOOP_1		0x17
#define	DRV2624_REG_SEQ_LOOP_2		0x18
#define	DRV2624_REG_MAIN_LOOP		0x19
#define	DRV2624_REG_RATED_VOLTAGE	0x1f
#define	DRV2624_REG_OVERDRIVE_CLAMP	0x20
#define	DRV2624_REG_CAL_COMP		0x21
#define	DRV2624_REG_CAL_BEMF		0x22
#define	DRV2624_REG_LOOP_CONTROL	0x23
#define	BEMFGAIN_MASK			0x03
#define DRV2624_CONSTANT_GAIN 		0x44

#define	DRV2624_REG_DRIVE_TIME		0x27
#define	DRIVE_TIME_MASK				0x1f
#define	MINFREQ_SEL_45HZ			0x01
#define	MINFREQ_SEL_MASK			0x80
#define	MINFREQ_SEL_SHIFT			0x07

#define	DRV2624_REG_OL_PERIOD_H			0x2e
#define	DRV2624_REG_OL_PERIOD_L			0x2f
#define	DRV2624_REG_RUNNING_PERIOD_H		0x05
#define	DRV2624_REG_RUNNING_PERIOD_L		0x06
#define	DRV2624_REG_DIAG_K			0x30

#define	GO_BIT_POLL_INTERVAL		15
#define	STANDBY_WAKE_DELAY		1
#define	WAKE_STANDBY_DELAY		3

#define	DRV2624_REG_RAM_ADDR_UPPER	0xfd
#define	DRV2624_REG_RAM_ADDR_LOWER	0xfe
#define	DRV2624_REG_RAM_DATA		0xff

#define	DRV2624_REG_LRA_SHAPE 		0x2c
#define	DRV2624_REG_LRA_SHAPE_MASK 	0x01
#define	DRV2624_REG_LRA_SHAPE_SINE 	0x01
#define	DRV2624_REG_LRA_SHAPE_SQUARE 	0x00

/* Commands */
#define	HAPTIC_CMDID_PLAY_SINGLE_EFFECT		0x01
#define	HAPTIC_CMDID_PLAY_EFFECT_SEQUENCE	0x02
#define	HAPTIC_CMDID_PLAY_TIMED_EFFECT		0x03
#define	HAPTIC_CMDID_GET_DEV_ID			0x04
#define	HAPTIC_CMDID_RUN_DIAG			0x05
#define	HAPTIC_CMDID_AUDIOHAPTIC_ENABLE		0x06
#define	HAPTIC_CMDID_AUDIOHAPTIC_DISABLE	0x07
#define	HAPTIC_CMDID_AUDIOHAPTIC_GETSTATUS	0x08
#define	HAPTIC_CMDID_REG_WRITE			0x09
#define	HAPTIC_CMDID_REG_READ			0x0a
#define	HAPTIC_CMDID_REG_SETBIT			0x0b
#define	HAPTIC_CMDID_PATTERN_RTP		0x0c
#define	HAPTIC_CMDID_RTP_SEQUENCE		0x0d
#define	HAPTIC_CMDID_GET_EFFECT_COUNT		0x10
#define	HAPTIC_CMDID_UPDATE_FIRMWARE		0x11
#define	HAPTIC_CMDID_READ_FIRMWARE		0x12
#define	HAPTIC_CMDID_RUN_CALIBRATION		0x13
#define	HAPTIC_CMDID_CONFIG_WAVEFORM		0x14
#define	HAPTIC_CMDID_SET_SEQUENCER		0x15
#define	HAPTIC_CMDID_REGLOG_ENABLE		0x16
#define HAPTIC_SET_CALIBRATION_RESULT           0x17

#define	HAPTIC_CMDID_STOP			0xFF

#define	MAX_TIMEOUT				10000	/* 10s */
#define	MAX_READ_BYTES				0xff
#define	DRV2624_SEQUENCER_SIZE			8

#define	WORK_IDLE				0
#define	WORK_VIBRATOR				0x01
#define	WORK_IRQ				0x02
#define	WORK_EFFECTSEQUENCER			0x04
#define	WORK_CALIBRATION			0x08
#define	WORK_DIAGNOSTIC				0x10

#define	YES		1
#define	NO		0
#define	GO		1
#define	STOP		0

#define	GO_BIT_CHECK_INTERVAL           5	/* 5 ms */
#define	GO_BIT_MAX_RETRY_CNT		20	/* 50 times */

#define DRV2624_MAGIC		0x2624
#define STRONG_MAGNITUDE        0x7fff
#define MEDIUM_MAGNITUDE        0x5fff
#define LIGHT_MAGNITUDE         0x3fff
#define DRV2624_RAM_SIZE        1024
#define EFFECT_MAX_NUM          32

/* auto calibration */
#define AUTO_CAL_TIME_REG             0x2A
#define AUTO_CAL_TIME_MASK            0x03
#define AUTO_CAL_TIME_250MS           0x00
#define AUTO_CAL_TIME_500MS           0x01
#define AUTO_CAL_TIME_1000MS          0x10
#define AUTO_CAL_TIME_AUTO_TRIGGER    0x11

/* boot calibration build config*/
#define DRV_BOOT_CALIB

typedef enum { DRV2624_RTP_MODE =
	    0x00, DRV2624_RAM_MODE, DRV2624_WAVE_SEQ_MODE =
	    DRV2624_RAM_MODE, DRV2624_DIAG_MODE, DRV2624_CALIBRATION_MODE,
	    DRV2624_NEW_RTP_MODE,
} drv2624_mode_t;
enum actuator_type { ERM = 0, LRA
};
enum loop_type { CLOSE_LOOP = 0x00, OPEN_LOOP
};
struct actuator_data {
	unsigned char mnActuatorType;
	unsigned char mnRatedVoltage;
	unsigned char mnOverDriveClampVoltage;
	unsigned char mnLRAFreq;
};
enum wave_seq_loop { SEQ_NO_LOOP, SEQ_LOOP_ONCE, SEQ_LOOP_TWICE,
	SEQ_LOOP_TRIPPLE
};
enum wave_main_loop { MAIN_NO_LOOP, MAIN_LOOP_ONCE, MAIN_LOOP_TWICE,
	MAIN_LOOP_3_TIMES, MAIN_LOOP_4_TIMES, MAIN_LOOP_5_TIMES,
	MAIN_LOOP_6_TIMES, MAIN_LOOP_INFINITELY
};
enum wave_main_scale { PERCENTAGE_100, PERCENTAGE_75, PERCENTAGE_50,
	PERCENTAGE_25
};
enum wave_main_interval { INTERVAL_5MS, INTERVAL_1MS
};
struct drv2624_waveform {
	unsigned char mnEffect;
	unsigned char mnLoop;
};
struct drv2624_waveform_sequencer {
	struct drv2624_waveform msWaveform[DRV2624_SEQUENCER_SIZE];
};
struct drv2624_wave_setting {
	unsigned char mnLoop;
	unsigned char mnInterval;
	unsigned char mnScale;
};
struct drv2624_autocal_result {
	int mnFinished;
	unsigned char mnResult;
	unsigned char mnCalComp;
	unsigned char mnCalBemf;
	unsigned char mnCalGain;
};
struct drv2624_diag_result {
	int mnFinished;
	unsigned char mnResult;
	unsigned char mnDiagZ;
	unsigned char mnDiagK;
};
struct drv2624_platform_data {
	int mnGpioNRST;
	int mnGpioINT;
	unsigned char mnLoop;
	struct actuator_data msActuator;
};
struct drv2624_fw_header {
	int fw_magic;
	int fw_size;
	int fw_date;
	int fw_chksum;
	int fw_effCount;
};
struct drv2624_constant_playinfo {
	int effect_count;
	int effect_id;
	int length;
	int magnitude;
	unsigned char rtp_input;
};
enum haptics_custom_effect_param { CUSTOM_DATA_EFFECT_IDX,
	CUSTOM_DATA_TIMEOUT_SEC_IDX, CUSTOM_DATA_TIMEOUT_MSEC_IDX,
	CUSTOM_DATA_LEN,
};
struct drv2624_data {
	struct input_dev *input_dev;
	struct i2c_client *client;
	struct drv2624_platform_data msPlatData;
	struct mutex lock;
	struct mutex dev_lock;
	unsigned char mnDeviceID;
	struct device *dev;
	struct regmap *mpRegmap;
	unsigned int mnIRQ;
	unsigned char mnIntStatus;
	bool mbIRQEnabled;
	bool mbIRQUsed;
	struct drv2624_wave_setting msWaveformSetting;
	struct drv2624_waveform_sequencer msWaveformSequencer;
	unsigned char mnFileCmd;
	volatile int mnVibratorPlaying;
	volatile char mnWorkMode;
	unsigned char mnCurrentReg;
	struct hrtimer haptics_timer;
	/** add count pEffDuration**/
	int effects_count;
	//u32 *pEffDuration;
	//struct hrtimer stop_timer;
	struct drv2624_autocal_result mAutoCalResult;
	struct drv2624_diag_result mDiagResult;
	struct drv2624_fw_header fw_header;
	unsigned char mRAMLSB;
	unsigned char mRAMMSB;
	int mnEffectType;
	unsigned char mnFwRam[DRV2624_RAM_SIZE];
	unsigned int mnEffectTimems[EFFECT_MAX_NUM];
	struct drv2624_constant_playinfo play;
	struct work_struct vibrator_work;

#ifdef	ANDROID_TIMED_OUTPUT
	struct timed_output_dev to_dev;

#endif				/*  */
	struct work_struct upload_periodic_work;
	struct work_struct haptics_playback_work;
	//struct workqueue_struct *cali_write_workqueue;
	//struct delayed_work cali_write_work;
	//struct work_struct haptics_set_gain_work;
#ifdef DRV_BOOT_CALIB
#define DELAY_BOOT_CALIB
#ifdef DELAY_BOOT_CALIB
#define BOOT_CALIB_TIMER (1*1000) //ms delayed after tas2624 init
	struct workqueue_struct *boot_calib_workqueue;
	struct delayed_work deblay_boot_calib_work;
#else
	struct work_struct boot_calib_work;
#endif
#endif
};

#define DRV2624_MAGIC_NUMBER	0x32363234	/* '2624' */

#define DRV2624_WAVSEQ_PLAY		 			_IOWR(DRV2624_MAGIC_NUMBER, 4, unsigned long)
#define DRV2624_STOP			 			_IOWR(DRV2624_MAGIC_NUMBER, 5, unsigned long)
#define DRV2624_RUN_DIAGNOSTIC			 	_IOWR(DRV2624_MAGIC_NUMBER, 6, unsigned long)
#define DRV2624_GET_DIAGRESULT			 	_IOWR(DRV2624_MAGIC_NUMBER, 7, struct drv2624_diag_result *)
#define DRV2624_RUN_AUTOCAL				 	_IOWR(DRV2624_MAGIC_NUMBER, 8, unsigned long)
#define DRV2624_GET_CALRESULT			 	_IOWR(DRV2624_MAGIC_NUMBER, 9, struct drv2624_autocal_result *)

#endif /*  */
