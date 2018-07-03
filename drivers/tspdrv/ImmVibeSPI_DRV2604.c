/*
 ** =========================================================================
 ** File:
 **	ImmVibeSPI.c
 **
 ** Description:
 **	Device-dependent functions called by Immersion TSP API
 **	to control PWM duty cycle, amp enable/disable, save IVT file, etc...
 **
 ** Portions Copyright (c) 2012 Immersion Corporation. All Rights Reserved.
 ** Copyright (C) 2018 XiaoMi, Inc.
 **
 ** This file contains Original Code and/or Modifications of Original Code
 ** as defined in and that are subject to the GNU Public License v2 -
 ** (the 'License'). You may not use this file except in compliance with the
 ** License. You should have received a copy of the GNU General Public License
 ** along with this program; if not, write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or contact
 ** TouchSenseSales@immersion.com.
 **
 ** The Original Code and all software distributed under the License are
 ** distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 ** EXPRESS OR IMPLIED, AND IMMERSION HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 ** INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
 ** FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see
 ** the License for the specific language governing rights and limitations
 ** under the License.
 ** =========================================================================
 */



#ifdef IMMVIBESPIAPI
#undef IMMVIBESPIAPI
#endif
#define IMMVIBESPIAPI static

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/slab.h>
#include <linux/types.h>

#include <linux/fs.h>
#include <linux/cdev.h>

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
#include <linux/timer.h>

#include <linux/workqueue.h>
#include <../../../drivers/staging/android/timed_output.h>
#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/wakelock.h>
#include <linux/debugfs.h>

#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <asm/bootinfo.h>

#define SUPPORT_TIMED_OUTPUT 1
#define SUPPORT_WRITE_PAT    0
#define DRV2604_USE_PWM_MODE 0
#define DRV2604_USE_RTP_MODE (1-DRV2604_USE_PWM_MODE)


#define USE_DRV2604_EN_PIN 0

#define GPIO_AMP_EN 0x00

#define USE_DRV2604_STANDBY 1

#define DEVICE_ADDR 0x5A

#define DEVICE_BUS 10

#define NUM_ACTUATORS 1

#define DRV2604_BOARD_NAME "DRV2604"

#define GO_REG	0x0C
#define GO	0x01
#define STOP	0x00

#define STATUS_REG	0x00
#define STATUS_DEFAULT	0x00

#define DIAG_RESULT_MASK	(1 << 3)
#define AUTO_CAL_PASSED		(0 << 3)
#define AUTO_CAL_FAILED		(1 << 3)
#define DIAG_GOOD		(0 << 3)
#define DIAG_BAD		(1 << 3)

#define DEV_ID_MASK (7 << 5)
#define DRV2605 (3 << 5)
#define DRV2604 (4 << 5)

#define MODE_REG	0x01
#define MODE_STANDBY	0x40

#define DRV2604_MODE_MASK		0x07
#define MODE_INTERNAL_TRIGGER		0
#define MODE_PWM_OR_ANALOG_INPUT	3
#define MODE_REAL_TIME_PLAYBACK		5
#define MODE_DIAGNOSTICS		6
#define AUTO_CALIBRATION		7

#define MODE_STANDBY_MASK		0x40
#define MODE_READY			1
#define MODE_SOFT_STANDBY		0

#define MODE_RESET		0x80

#define REAL_TIME_PLAYBACK_REG	0x02

#define LIBRARY_SELECTION_REG	0x03
#define LIBRARY_SELECTION_DEFAULT	0x00

#define WAVEFORM_SEQUENCER_REG  0x04
#define WAVEFORM_SEQUENCER_REG2	0x05
#define WAVEFORM_SEQUENCER_REG3	0x06
#define WAVEFORM_SEQUENCER_REG4	0x07
#define WAVEFORM_SEQUENCER_REG5	0x08
#define WAVEFORM_SEQUENCER_REG6	0x09
#define WAVEFORM_SEQUENCER_REG7	0x0A
#define WAVEFORM_SEQUENCER_REG8	0x0B
#define WAVEFORM_SEQUENCER_MAX  8
#define WAVEFORM_SEQUENCER_DEFAULT 0x00

#define OVERDRIVE_TIME_OFFSET_REG 0x0D

#define SUSTAIN_TIME_OFFSET_POS_REG 0x0E

#define SUSTAIN_TIME_OFFSET_NEG_REG 0x0F

#define BRAKE_TIME_OFFSET_REG 0x10

#define AUDIO_HAPTICS_CONTROL_REG 0x11

#define AUDIO_HAPTICS_RECT_10MS	(0 << 2)
#define AUDIO_HAPTICS_RECT_20MS	(1 << 2)
#define AUDIO_HAPTICS_RECT_30MS	(2 << 2)
#define AUDIO_HAPTICS_RECT_40MS	(3 << 2)

#define AUDIO_HAPTICS_FILTER_100HZ  0
#define AUDIO_HAPTICS_FILTER_125HZ  1
#define AUDIO_HAPTICS_FILTER_150HZ  2
#define AUDIO_HAPTICS_FILTER_200HZ  3

#define AUDIO_HAPTICS_MIN_INPUT_REG 0x12

#define AUDIO_HAPTICS_MAX_INPUT_REG 0x13

#define AUDIO_HAPTICS_MIN_OUTPUT_REG 0x14

#define AUDIO_HAPTICS_MAX_OUTPUT_REG 0x15

#define RATED_VOLTAGE_REG	0x16

#define OVERDRIVE_CLAMP_VOLTAGE_REG 0x17

#define AUTO_CALI_RESULT_REG	0x18

#define AUTO_CALI_BACK_EMF_RESULT_REG 0x19

#define FEEDBACK_CONTROL_REG	0x1A

#define FEEDBACK_CONTROL_BEMF_LRA_GAIN0 0
#define FEEDBACK_CONTROL_BEMF_LRA_GAIN1 1
#define FEEDBACK_CONTROL_BEMF_LRA_GAIN2 2
#define FEEDBACK_CONTROL_BEMF_LRA_GAIN3 3

#define LOOP_RESPONSE_SLOW	(0 << 2)
#define LOOP_RESPONSE_MEDIUM	(1 << 2)
#define LOOP_RESPONSE_FAST	(2 << 2)
#define LOOP_RESPONSE_VERY_FAST (3 << 2)

#define FB_BRAKE_FACTOR_1X	(0 << 4)
#define FB_BRAKE_FACTOR_2X	(1 << 4)
#define FB_BRAKE_FACTOR_3X	(2 << 4)
#define FB_BRAKE_FACTOR_4X	(3 << 4)
#define FB_BRAKE_FACTOR_6X	(4 << 4)
#define FB_BRAKE_FACTOR_8X	(5 << 4)
#define FB_BRAKE_FACTOR_16X	(6 << 4)
#define FB_BRAKE_DISABLED	(7 << 4)

#define FEEDBACK_CONTROL_MODE_ERM 0 /* default */
#define FEEDBACK_CONTROL_MODE_LRA (1 << 7)

#define Control1_REG		0x1B

#define STARTUP_BOOST_ENABLED	(1 << 7)
#define STARTUP_BOOST_DISABLED  (0 << 7)
#define AC_COUPLE_ENABLED	(1 << 5)
#define AC_COUPLE_DISABLED	(0 << 5)


#define Control2_REG		0x1C

#define IDISS_TIME_MASK		0x03
#define IDISS_TIME_VERY_SHORT	0
#define IDISS_TIME_SHORT	1
#define IDISS_TIME_MEDIUM	2
#define IDISS_TIME_LONG		3

#define BLANKING_TIME_MASK		0x0C
#define BLANKING_TIME_VERY_SHORT	(0 << 2)
#define BLANKING_TIME_SHORT		(1 << 2)
#define BLANKING_TIME_MEDIUM		(2 << 2)
#define BLANKING_TIME_VERY_LONG		(3 << 2)

#define AUTO_RES_GAIN_MASK	0x30
#define AUTO_RES_GAIN_VERY_LOW	(0 << 4)
#define AUTO_RES_GAIN_LOW	(1 << 4)
#define AUTO_RES_GAIN_MEDIUM	(2 << 4)
#define AUTO_RES_GAIN_HIGH	(3 << 4)

#define SOFT_BRAKE_MASK		0x40
#define SOFT_BRAKE		(1 << 6)

#define BIDIR_INPUT_MASK	0x80
#define UNIDIRECT_INPUT		(0 << 7)
#define BIDIRECT_INPUT		(1 << 7)

#define Control3_REG 0x1D
#define INPUT_PWM		(0 << 1)
#define INPUT_ANALOG		(1 << 1)
#define ERM_OpenLoop_Enabled	(1 << 5)
#define NG_Thresh_1		(1 << 6)
#define NG_Thresh_2		(2 << 6)
#define NG_Thresh_3		(3 << 6)

#define AUTOCAL_MEM_INTERFACE_REG 0x1E

#define AUTOCAL_TIME_150MS	(0 << 4)
#define AUTOCAL_TIME_250MS	(1 << 4)
#define AUTOCAL_TIME_500MS	(2 << 4)
#define AUTOCAL_TIME_1000MS	(3 << 4)

#define Control5_REG 0x1F

#define LRA_OPEN_LOOP_PERIOD 0x20

#define SILICON_REVISION_REG	0x3B
#define SILICON_REVISION_MASK	0x07

#define AUDIO_HAPTICS_MIN_INPUT_VOLTAGE		0x19
#define AUDIO_HAPTICS_MAX_INPUT_VOLTAGE		0x64
#define AUDIO_HAPTICS_MIN_OUTPUT_VOLTAGE	0x19
#define AUDIO_HAPTICS_MAX_OUTPUT_VOLTAGE	0xFF

#define DEFAULT_LRA_AUTOCAL_COMPENSATION	0x06
#define DEFAULT_LRA_AUTOCAL_BACKEMF		0xFB

#define DEFAULT_DRIVE_TIME	0x13
#define MAX_AUTOCALIBRATION_ATTEMPT 2

#if 0
#define GPIO_VIBTONE_EN1 86
#define GPIO_PORT GPIO_VIBTONE_EN1
#endif
#define GPIO_LEVEL_LOW 0
#define GPIO_LEVEL_HIGH 1

#define SKIP_LRA_AUTOCAL	1
#define GO_BIT_POLL_INTERVAL	15

#define LRA_SEMCO1036	0
#define LRA_SEMCO0934	1
#define LRA_BLUECOM	2
#define LRA_SELECTION	LRA_BLUECOM

#if (LRA_SELECTION == LRA_SEMCO1036)
#define LRA_RATED_VOLTAGE		0x4E
#define LRA_OVERDRIVE_CLAMP_VOLTAGE	0xA4
#define LRA_RTP_STRENGTH		0x7F

#elif (LRA_SELECTION == LRA_SEMCO0934)
#define LRA_RATED_VOLTAGE		0x51
#define LRA_OVERDRIVE_CLAMP_VOLTAGE	0x72
#define LRA_RTP_STRENGTH		0x7F

#elif (LRA_SELECTION == LRA_BLUECOM)
#define LRA_RATED_VOLTAGE		0x53
#define LRA_OVERDRIVE_CLAMP_VOLTAGE	0xA4
#define LRA_RTP_STRENGTH		0x7F

#else
#define LRA_RATED_VOLTAGE			0x60
#define LRA_OVERDRIVE_CLAMP_VOLTAGE		0x9E
#define LRA_RTP_STRENGTH		0x7F

#endif


#define ERM_RATED_VOLTAGE			0x7F
#define ERM_OVERDRIVE_CLAMP_VOLTAGE		0x9B
#define DEFAULT_ERM_AUTOCAL_COMPENSATION	0x04
#define DEFAULT_ERM_AUTOCAL_BACKEMF		0xA7


#define X5LRA_RATED_VOLTAGE                     0x18
#define X5LRA_OVERDRIVE_CLAMP_VOLTAGE           0x2B


#define STANDBY_WAKE_DELAY	1

#define REAL_TIME_PLAYBACK_CALIBRATION_STRENGTH 0x7F

#define MAX_REVISION_STRING_SIZE 10

static int g_nDeviceID = -1;
static struct i2c_client* g_pTheClient = NULL;
static bool g_bAmpEnabled = false;
static bool g_brake = false;
static bool g_autotune_brake_enabled = false;
static bool g_bNeedToRestartPlayBack = false;
static unsigned GPIO_VIBTONE_EN1 = 899;

#define MAX_TIMEOUT 10000
#define PAT_MAX_LEN 256

#define PWM_CH_ID 3

#define IMMVIBESPI_MULTIPARAM_SUPPORT
IMMVIBESPIAPI VibeStatus ImmVibeSPI_Device_GetParamFileId(void) {
	return 0;
}

static struct vibrator {
	struct wake_lock wklock;
	struct pwm_device *pwm_dev;
	struct hrtimer timer;
	struct mutex lock;
	struct work_struct work;
	struct work_struct work_play_eff;
	unsigned char sequence[8];
	volatile int should_stop;

#if SUPPORT_WRITE_PAT
	struct work_struct pat_work;
	struct workqueue_struct *hap_wq;
	signed char* pat;
	int pat_len;
	int pat_i;
	int pat_mode;
#endif
} vibdata;

#if SUPPORT_WRITE_PAT
static signed char pattern[PAT_MAX_LEN];
#endif

static const unsigned char LRA_autocal_sequence[] = {
	MODE_REG,			AUTO_CALIBRATION,
	RATED_VOLTAGE_REG,		LRA_RATED_VOLTAGE,
	OVERDRIVE_CLAMP_VOLTAGE_REG,	LRA_OVERDRIVE_CLAMP_VOLTAGE,
	FEEDBACK_CONTROL_REG,		FEEDBACK_CONTROL_MODE_LRA | FB_BRAKE_FACTOR_4X | LOOP_RESPONSE_FAST,
	Control2_REG,			0xF5,
	Control3_REG,			NG_Thresh_2,
	GO_REG,			 GO,
};


static const unsigned char LRA_autocal_done_seq[] = {
	FEEDBACK_CONTROL_REG,		0xb7,
};

#if SKIP_LRA_AUTOCAL == 1

static const unsigned char LRA_init_sequence[] = {
	MODE_REG,			MODE_INTERNAL_TRIGGER,
	REAL_TIME_PLAYBACK_REG,		REAL_TIME_PLAYBACK_CALIBRATION_STRENGTH,
	LIBRARY_SELECTION_REG,		0x00,
	WAVEFORM_SEQUENCER_REG,		WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG2,	WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG3,	WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG4,	WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG5,	WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG6,	WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG7,	WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG8,	WAVEFORM_SEQUENCER_DEFAULT,
	GO_REG,				STOP,
	OVERDRIVE_TIME_OFFSET_REG,	0x00,
	SUSTAIN_TIME_OFFSET_POS_REG,	0x00,
	SUSTAIN_TIME_OFFSET_NEG_REG,	0x00,
	BRAKE_TIME_OFFSET_REG,		0x06,
	AUDIO_HAPTICS_CONTROL_REG,	AUDIO_HAPTICS_RECT_20MS | AUDIO_HAPTICS_FILTER_125HZ,
	AUDIO_HAPTICS_MIN_INPUT_REG,	AUDIO_HAPTICS_MIN_INPUT_VOLTAGE,
	AUDIO_HAPTICS_MAX_INPUT_REG,	AUDIO_HAPTICS_MAX_INPUT_VOLTAGE,
	AUDIO_HAPTICS_MIN_OUTPUT_REG,	AUDIO_HAPTICS_MIN_OUTPUT_VOLTAGE,
	AUDIO_HAPTICS_MAX_OUTPUT_REG,	AUDIO_HAPTICS_MAX_OUTPUT_VOLTAGE,
	RATED_VOLTAGE_REG,		LRA_RATED_VOLTAGE,
	OVERDRIVE_CLAMP_VOLTAGE_REG,	LRA_OVERDRIVE_CLAMP_VOLTAGE,
	AUTO_CALI_RESULT_REG,		DEFAULT_LRA_AUTOCAL_COMPENSATION,
	AUTO_CALI_BACK_EMF_RESULT_REG,	DEFAULT_LRA_AUTOCAL_BACKEMF,
	FEEDBACK_CONTROL_REG,		FEEDBACK_CONTROL_MODE_LRA | FB_BRAKE_FACTOR_4X | LOOP_RESPONSE_MEDIUM | FEEDBACK_CONTROL_BEMF_LRA_GAIN3,
	Control1_REG,			STARTUP_BOOST_ENABLED | DEFAULT_DRIVE_TIME,
	Control2_REG,			BIDIRECT_INPUT | SOFT_BRAKE | AUTO_RES_GAIN_HIGH | BLANKING_TIME_MEDIUM | IDISS_TIME_MEDIUM,
	Control3_REG,			NG_Thresh_2 ,
	AUTOCAL_MEM_INTERFACE_REG,	AUTOCAL_TIME_500MS,
};

static const unsigned char X5LRA_init_sequence[] = {
	MODE_REG,                       MODE_INTERNAL_TRIGGER,
	REAL_TIME_PLAYBACK_REG,         REAL_TIME_PLAYBACK_CALIBRATION_STRENGTH,
	LIBRARY_SELECTION_REG,          0x00,
	WAVEFORM_SEQUENCER_REG,         WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG2,        WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG3,        WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG4,        WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG5,        WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG6,        WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG7,        WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG8,        WAVEFORM_SEQUENCER_DEFAULT,
	GO_REG,                         STOP,
	OVERDRIVE_TIME_OFFSET_REG,      0x00,
	SUSTAIN_TIME_OFFSET_POS_REG,    0x00,
	SUSTAIN_TIME_OFFSET_NEG_REG,    0x00,
	BRAKE_TIME_OFFSET_REG,          0x00,
	AUDIO_HAPTICS_CONTROL_REG,      AUDIO_HAPTICS_RECT_20MS | AUDIO_HAPTICS_FILTER_125HZ,
	AUDIO_HAPTICS_MIN_INPUT_REG,    AUDIO_HAPTICS_MIN_INPUT_VOLTAGE,
	AUDIO_HAPTICS_MAX_INPUT_REG,    AUDIO_HAPTICS_MAX_INPUT_VOLTAGE,
	AUDIO_HAPTICS_MIN_OUTPUT_REG,   AUDIO_HAPTICS_MIN_OUTPUT_VOLTAGE,
	AUDIO_HAPTICS_MAX_OUTPUT_REG,   AUDIO_HAPTICS_MAX_OUTPUT_VOLTAGE,
	RATED_VOLTAGE_REG,              X5LRA_RATED_VOLTAGE,
	OVERDRIVE_CLAMP_VOLTAGE_REG,    X5LRA_OVERDRIVE_CLAMP_VOLTAGE,
	AUTO_CALI_RESULT_REG,           0x08,
	AUTO_CALI_BACK_EMF_RESULT_REG,  0x8E,
	FEEDBACK_CONTROL_REG,           0xA9,
	Control1_REG,                   0x8F,
	Control2_REG,                   0xF5,
	Control3_REG,                   NG_Thresh_2 ,
	Control5_REG,                   0x80,
	LRA_OPEN_LOOP_PERIOD,           0x2a,
	AUTOCAL_MEM_INTERFACE_REG,      0x30,
};


static const unsigned char ERM_init_sequence[] = {
	MODE_REG,			MODE_INTERNAL_TRIGGER,
	REAL_TIME_PLAYBACK_REG,		REAL_TIME_PLAYBACK_CALIBRATION_STRENGTH,
	LIBRARY_SELECTION_REG,		0x00,
	WAVEFORM_SEQUENCER_REG,		WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG2,	WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG3,	WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG4,	WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG5,	WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG6,	WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG7,	WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG8,	WAVEFORM_SEQUENCER_DEFAULT,
	GO_REG,				STOP,
	OVERDRIVE_TIME_OFFSET_REG,	0x00,
	SUSTAIN_TIME_OFFSET_POS_REG,	0x00,
	SUSTAIN_TIME_OFFSET_NEG_REG,	0x00,
	BRAKE_TIME_OFFSET_REG,		0x06,
	AUDIO_HAPTICS_CONTROL_REG,	AUDIO_HAPTICS_RECT_20MS | AUDIO_HAPTICS_FILTER_125HZ,
	AUDIO_HAPTICS_MIN_INPUT_REG,	AUDIO_HAPTICS_MIN_INPUT_VOLTAGE,
	AUDIO_HAPTICS_MAX_INPUT_REG,	AUDIO_HAPTICS_MAX_INPUT_VOLTAGE,
	AUDIO_HAPTICS_MIN_OUTPUT_REG,	AUDIO_HAPTICS_MIN_OUTPUT_VOLTAGE,
	AUDIO_HAPTICS_MAX_OUTPUT_REG,	AUDIO_HAPTICS_MAX_OUTPUT_VOLTAGE,
	RATED_VOLTAGE_REG,		ERM_RATED_VOLTAGE,
	OVERDRIVE_CLAMP_VOLTAGE_REG,	ERM_OVERDRIVE_CLAMP_VOLTAGE,
	AUTO_CALI_RESULT_REG,		0x05,
	AUTO_CALI_BACK_EMF_RESULT_REG,	0x9F,
	FEEDBACK_CONTROL_REG,		FB_BRAKE_FACTOR_4X | LOOP_RESPONSE_MEDIUM | FEEDBACK_CONTROL_BEMF_LRA_GAIN2,
	Control1_REG,			STARTUP_BOOST_ENABLED | DEFAULT_DRIVE_TIME,
	Control2_REG,			BIDIRECT_INPUT | SOFT_BRAKE | AUTO_RES_GAIN_HIGH | BLANKING_TIME_MEDIUM | IDISS_TIME_MEDIUM,
	Control3_REG,			ERM_OpenLoop_Enabled | NG_Thresh_2 ,
	AUTOCAL_MEM_INTERFACE_REG,	AUTOCAL_TIME_500MS,
};

static size_t seq_size;
static const unsigned char *DRV_init_sequence;
static unsigned int g_hw_version = 6;

#endif

static void drv2604_write_reg_val(const unsigned char* data, unsigned int size)
{
	int i = 0;

	if (size % 2 != 0)
		return;

	if(g_hw_version == 5) {
		while (i < size) {
			pr_debug("drv2604 x5 write 0x%02x, 0x%02x", data[i], data[i + 1]);
			if(data[i] == 0x02)
				i2c_smbus_write_byte_data(g_pTheClient, data[i], 0xFF);
			else
				i2c_smbus_write_byte_data(g_pTheClient, data[i], data[i + 1]);
			i += 2;
		}
	} else {
		while (i < size) {
			pr_debug("drv2604 write 0x%02x, 0x%02x", data[i], data[i + 1]);
			i2c_smbus_write_byte_data(g_pTheClient, data[i], data[i + 1]);
			i += 2;
		}
	}
}
static unsigned char drv2604_read_reg(unsigned char reg)
{
	unsigned char data;
	struct i2c_msg msgs[2];
	struct i2c_adapter *i2c_adap = g_pTheClient->adapter;
	unsigned char address = g_pTheClient->addr;
	int res;

	if (!i2c_adap)
		return -EINVAL;

	msgs[0].addr = address;
	msgs[0].flags = 0;
	msgs[0].buf = &reg;
	msgs[0].len = 1;

	msgs[1].addr = address;
	msgs[1].flags = I2C_M_RD;
	msgs[1].buf = &data;
	msgs[1].len = 1;

	res = i2c_transfer(i2c_adap, msgs, 2);
	pr_debug("drv2604 read addr:0x%x reg:0x%x data:0x%x res:%d",
			address, reg, data, res);

	return data;
}

#if SKIP_LRA_AUTOCAL == 0
static void drv2604_poll_go_bit(void)
{
	while (drv2604_read_reg(GO_REG) == GO)
		schedule_timeout_interruptible(msecs_to_jiffies(GO_BIT_POLL_INTERVAL));
}
#endif

static void drv2604_set_rtp_val(char value)
{
	char rtp_val[] = {REAL_TIME_PLAYBACK_REG, value};

	drv2604_write_reg_val(rtp_val, sizeof(rtp_val));
}

static void drv2604_change_mode(char mode)
{
	unsigned char tmp[] = {MODE_REG, mode};

	drv2604_write_reg_val(tmp, sizeof(tmp));;

}

#define YES 1
#define NO  0



static int vibrator_is_playing = NO;

static int vibrator_get_time(struct timed_output_dev *dev)
{
	if (hrtimer_active(&vibdata.timer)) {
		ktime_t r = hrtimer_get_remaining(&vibdata.timer);
		return ktime_to_ms(r);
	}

	return 0;
}

static void vibrator_off(void)
{
#if SUPPORT_TIMED_OUTPUT
	if (vibrator_is_playing) {
		vibrator_is_playing = NO;
		drv2604_change_mode(MODE_STANDBY);
		g_bAmpEnabled = false;
	}

	wake_unlock(&vibdata.wklock);
#endif
}

static void vibrator_enable(struct timed_output_dev *dev, int value)
{
#if SUPPORT_TIMED_OUTPUT
	char mode;
	hrtimer_cancel(&vibdata.timer);
	cancel_work_sync(&vibdata.work);

#if SUPPORT_WRITE_PAT
	cancel_work_sync(&vibdata.pat_work);
#endif
	mutex_lock(&vibdata.lock);

	if (value) {
		wake_lock(&vibdata.wklock);
		drv2604_read_reg(STATUS_REG);

		if (!g_bAmpEnabled) {
			mode = drv2604_read_reg(MODE_REG) & DRV2604_MODE_MASK;
#if DRV2604_USE_RTP_MODE
			if (mode != MODE_REAL_TIME_PLAYBACK) {
				drv2604_change_mode(MODE_REAL_TIME_PLAYBACK);
				drv2604_set_rtp_val(REAL_TIME_PLAYBACK_CALIBRATION_STRENGTH);
				vibrator_is_playing = YES;
				g_bAmpEnabled = true;
			}
#endif
#if DRV2604_USE_PWM_MODE
			if (mode != MODE_PWM_OR_ANALOG_INPUT) {
				pwm_duty_enable(vibdata.pwm_dev, 0);
				drv2604_change_mode(MODE_PWM_OR_ANALOG_INPUT);
				vibrator_is_playing = YES;
				g_bAmpEnabled = true;
			}
#endif
		}

		if (value > 0) {
			if (value > MAX_TIMEOUT)
				value = MAX_TIMEOUT;
			hrtimer_start(&vibdata.timer, ns_to_ktime((u64)value * NSEC_PER_MSEC), HRTIMER_MODE_REL);
		}
	} else
		vibrator_off();

	mutex_unlock(&vibdata.lock);
#endif
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	schedule_work(&vibdata.work);
	return HRTIMER_NORESTART;
}

static void vibrator_work(struct work_struct *work)
{
	mutex_lock(&vibdata.lock);
	vibrator_off();
	mutex_unlock(&vibdata.lock);
}

static unsigned char g_dbg_reg = 0;
static int drv2604_dbg_get(void *data, u64 *val)
{
	*val = g_nDeviceID;
	drv2604_read_reg(g_dbg_reg);
	return 0;
}

static int drv2604_dbg_set(void *data, u64 val)
{
	unsigned char v = val & 0xFF;
	g_dbg_reg = (val & 0xFF00) >> 8;

	if(v == 0xFF) return 0;

	i2c_smbus_write_byte_data(g_pTheClient, g_dbg_reg, val);
	pr_info("drv2604 dbg write 0x%02x 0x%02x", g_dbg_reg, v);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(drv2604_dbg, drv2604_dbg_get, drv2604_dbg_set, "%llu\n");

#if SUPPORT_WRITE_PAT
static void drv2604_pat_work(struct work_struct *work)
{
	int i;
	u32 value = 0;
	u32 time  = 0;

	for (i = 1; i < vibdata.pat_len; i += 2) {
		time = (u8)vibdata.pat[i + 1];
		if (vibdata.pat[i] != 0) {
			value = (vibdata.pat[i] > 0)?(vibdata.pat[i]):0;
			if(value > 126)
				value = 256;
			else
				value += 128;
			pwm_duty_enable(vibdata.pwm_dev, value);
			msleep(time);
		} else {
			if ((time == 0) || (i + 2 >= vibdata.pat_len )) {
				pwm_disable(vibdata.pwm_dev);
				drv2604_change_mode(MODE_STANDBY);
				pr_debug("drv2604 vib len:%d time:%d", vibdata.pat_len, time);
				break;
			} else {
				pwm_duty_enable(vibdata.pwm_dev, 0);
				msleep(time);
			}
		}
		pr_debug("%s: %d vib:%d time:%d value:%u",__func__, i, vibdata.pat[i], time, value);
	}
	wake_unlock(&vibdata.wklock);
}
#endif

static ssize_t drv2604_write_pattern(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr,
		char *buffer, loff_t offset, size_t count)
{
#if SUPPORT_TIMED_OUTPUT
#if SUPPORT_WRITE_PAT
	mutex_lock(&vibdata.lock);
	wake_lock(&vibdata.wklock);
	pr_debug("%s count:%d [%d %d %d %d %d %d %d %d %d ]", __func__,
			count, buffer[0], buffer[1], buffer[2], buffer[3],
			buffer[4], buffer[5], buffer[6], buffer[7], buffer[8]);

	vibdata.pat_len = 0;
	cancel_work_sync(&vibdata.pat_work);

	memcpy(pattern, buffer, count);
	pattern[count] = 0;
	pattern[count + 1] = 0;
	vibdata.pat_mode = pattern[0];
	vibdata.pat_len = count + 2;
	vibdata.pat_i = 1;

	drv2604_change_mode(MODE_PWM_OR_ANALOG_INPUT);
	queue_work(vibdata.hap_wq, &vibdata.pat_work);

	mutex_unlock(&vibdata.lock);
#endif
#endif
	return 0;
}

static struct timed_output_dev to_dev =
{
	.name		= "vibrator",
	.get_time	= vibrator_get_time,
	.enable	 = vibrator_enable,
};

static struct bin_attribute drv2604_bin_attrs = {
	.attr	= {
		.name	= "pattern",
		.mode	= 0644
	},
	.write  = drv2604_write_pattern,
	.size	= PAT_MAX_LEN + 1,
};

static int drv2604_probe(struct i2c_client* client, const struct i2c_device_id* id)
{
	char status;
#if SKIP_LRA_AUTOCAL == 0
	int nCalibrationCount = 0;
#endif
	unsigned char tmp[] = { MODE_REG, MODE_STANDBY };

	struct pinctrl_state *set_state =NULL;

	client->dev.pins = kmalloc(sizeof(struct dev_pin_info),GFP_KERNEL);
	client->dev.pins->p = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(client->dev.pins->p)) {
		pr_err("vibrator_probe: return error in lines%d\n",__LINE__);
		return -ENODEV;
	}
	set_state = pinctrl_lookup_state(client->dev.pins->p, "vibrator_active");
	if (IS_ERR_OR_NULL(set_state)) {
		pr_err("vibrator_probe: return error in lines%d\n",__LINE__);
		return -ENODEV;
	}
	pinctrl_select_state(client->dev.pins->p, set_state);
	set_state = pinctrl_lookup_state(client->dev.pins->p, "vibrator_suspend");
	if (IS_ERR_OR_NULL(set_state)) {
		pr_err("vibrator_probe: return error in lines%d\n",__LINE__);
		return -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		DbgOut((DBL_ERROR, "drv2604 on M3 probe failed"));
		return -ENODEV;
	}
	gpio_direction_output(GPIO_VIBTONE_EN1, GPIO_LEVEL_HIGH);

	udelay(30);
	g_pTheClient = client;
	DRV_init_sequence = X5LRA_init_sequence;
	seq_size = sizeof(X5LRA_init_sequence);
	g_autotune_brake_enabled = true;

#if SKIP_LRA_AUTOCAL == 1
	drv2604_write_reg_val(DRV_init_sequence, seq_size);
	status = drv2604_read_reg(STATUS_REG);
#else
	do {
		drv2604_write_reg_val(LRA_autocal_sequence, sizeof(LRA_autocal_sequence));
		drv2604_poll_go_bit();

		status = drv2604_read_reg(STATUS_REG);

		nCalibrationCount++;
	} while (((status & DIAG_RESULT_MASK) == AUTO_CAL_FAILED) && (nCalibrationCount < MAX_AUTOCALIBRATION_ATTEMPT));

	if ((status & DIAG_RESULT_MASK) == AUTO_CAL_FAILED) {
		DbgOut((DBL_ERROR, "drv2604 auto-calibration failed after %d attempts.\n", nCalibrationCount));
	} else {
		drv2604_write_reg_val(LRA_autocal_done_seq, sizeof(LRA_autocal_done_seq));
		drv2604_read_reg(AUTO_CALI_RESULT_REG);
		drv2604_read_reg(AUTO_CALI_BACK_EMF_RESULT_REG);
		drv2604_read_reg(FEEDBACK_CONTROL_REG);
	}
#endif

	g_nDeviceID = (status & DEV_ID_MASK);
	switch (g_nDeviceID) {
	case DRV2605:
		DbgOut((DBL_INFO, "drv2604 driver found: drv2605.\n"));
		break;
	case DRV2604:
		DbgOut((DBL_INFO, "drv2604 driver found: drv2604.\n"));
		break;
	default:
		DbgOut((DBL_INFO, "drv2604 driver found: unknown.\n"));
		break;
	}

#if USE_DRV2604_STANDBY
	drv2604_write_reg_val(tmp, sizeof(tmp));
#elif USE_DRV2604_EN_PIN
	drv2604_change_mode(MODE_REAL_TIME_PLAYBACK);
#endif

#if USE_DRV2604_EN_PIN
	drv2604_set_en(false);
#endif

	debugfs_create_file("drv2604", 0644, NULL, NULL, &drv2604_dbg);

	DbgOut((DBL_INFO, "drv2604 on M3 probe succeeded"));

#if SUPPORT_WRITE_PAT
	INIT_WORK(&vibdata.pat_work, drv2604_pat_work);
	vibdata.hap_wq = alloc_workqueue("haptic_wq", WQ_HIGHPRI, 0);
	if (vibdata.hap_wq  == NULL) {
		printk("drv2604 alloc workqueue failed");
	}
	vibdata.pat = pattern;
#endif

	printk(KERN_ALERT"drv2604 probe succeeded");

	return 0;
}

static int drv2604_remove(struct i2c_client* client)
{
	DbgOut((DBL_VERBOSE, "drv2604 on M3 removed.\n"));
	return 0;
}

static const struct i2c_device_id drv2604_id[] = {
	{ "drv2604", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, drv2604_id);
#ifdef CONFIG_OF
static struct of_device_id drv2604_match_table[] = {
	{ .compatible = "vibrator,drv2604",},
	{ },
};
#else
#define drv2604_match_table NULL
#endif

static struct i2c_driver drv2604_driver = {
	.driver	= {
		.name	= "drv2604",
		.of_match_table = drv2604_match_table,
	},
	.probe		= drv2604_probe,
	.remove		= drv2604_remove,
	.id_table	= drv2604_id,
};


#define AUTOTUNE_BRAKE_TIME 35

static VibeInt8 g_lastForce = 0;

static void autotune_brake_complete(struct work_struct *work)
{
	if (g_lastForce > 0)
		return;

#if USE_DRV2604_STANDBY
	drv2604_change_mode(MODE_STANDBY);
#endif

#if USE_DRV2604_EN_PIN
	drv2604_set_en(false);
#endif
}

DECLARE_DELAYED_WORK(g_brake_complete, autotune_brake_complete);

static struct workqueue_struct *g_workqueue;

IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpDisable(VibeUInt8 nActuatorIndex)
{
	if (g_bAmpEnabled)
	{
		DbgOut((DBL_VERBOSE, "ImmVibeSPI_ForceOut_AmpDisable.\n"));

#if DRV2604_USE_RTP_MODE
		drv2604_set_rtp_val(0);
#endif
#if DRV2604_USE_PWM_MODE
		pwm_duty_enable(vibdata.pwm_dev, 0);
#endif

		if (g_autotune_brake_enabled && g_brake && g_workqueue) {
			queue_delayed_work(g_workqueue,
					&g_brake_complete,
					msecs_to_jiffies(AUTOTUNE_BRAKE_TIME));
		} else
		{
#if USE_DRV2604_STANDBY
			drv2604_change_mode(MODE_STANDBY);
#endif
#if USE_DRV2604_EN_PIN
			drv2604_set_en(false);
#endif
		}
		g_bAmpEnabled = false;
	}
	return VIBE_S_SUCCESS;
}

IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpEnable(VibeUInt8 nActuatorIndex)
{
	if (!g_bAmpEnabled) {
		DbgOut((DBL_VERBOSE, "ImmVibeSPI_ForceOut_AmpEnable.\n"));
		if(g_autotune_brake_enabled)
			cancel_delayed_work_sync(&g_brake_complete);

#if USE_DRV2604_EN_PIN
		drv2604_set_en(true);
#endif

#if USE_DRV2604_STANDBY
#if DRV2604_USE_RTP_MODE
		drv2604_change_mode(MODE_REAL_TIME_PLAYBACK);
#endif
#if DRV2604_USE_PWM_MODE
		pwm_duty_enable(vibdata.pwm_dev, 0);
		drv2604_change_mode(MODE_PWM_OR_ANALOG_INPUT);
#endif
#endif




		if(g_hw_version == 6) {

		} else {
			usleep_range(1000, 1000);

#if SKIP_LRA_AUTOCAL == 1
			if (drv2604_read_reg(RATED_VOLTAGE_REG) != DRV_init_sequence[43]) {
				printk(KERN_INFO "ImmVibeSPI_ForceOut_AmpEnable: Register values resent.\n");
				drv2604_write_reg_val(DRV_init_sequence, seq_size);
			}
#endif
		}
		g_bAmpEnabled = true;
		g_bNeedToRestartPlayBack = true;
	}
	return VIBE_S_SUCCESS;
}

IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Initialize(void)
{
	struct i2c_adapter* adapter;
	struct i2c_client* client;

	DbgOut((DBL_VERBOSE, "ImmVibeSPI_ForceOut_Initialize.\n"));

	g_bAmpEnabled = true;

	GPIO_VIBTONE_EN1 = 899;

	if (gpio_request(GPIO_VIBTONE_EN1, "vibrator-en") < 0) {
		printk(KERN_ALERT"drv2604: error requesting gpio\n");
		return VIBE_E_FAIL;
	}

	adapter = i2c_get_adapter(DEVICE_BUS);

	if (adapter) {
		int retVal = i2c_add_driver(&drv2604_driver);

		if (retVal)
			return VIBE_E_FAIL;
	} else {
		DbgOut((DBL_VERBOSE, "ImmVibeSPI_ForceOut_AmpDisable.\n"));
		return VIBE_E_FAIL;
	}

	if(g_autotune_brake_enabled)
		g_workqueue = create_workqueue("tspdrv_workqueue");

	if (timed_output_dev_register(&to_dev) < 0) {
		printk(KERN_ALERT"drv2604: fail to create timed output dev: enable\n");
		gpio_direction_output(GPIO_VIBTONE_EN1, GPIO_LEVEL_LOW);
		gpio_free(GPIO_VIBTONE_EN1);
		i2c_del_driver(&drv2604_driver);
		i2c_unregister_device(client);
		return VIBE_E_FAIL;
	}

	if ( device_create_bin_file(to_dev.dev, &drv2604_bin_attrs)) {
		printk(KERN_ALERT"drv2604: fail to create timed output dev: pattern\n");
		return VIBE_E_FAIL;
	}

	hrtimer_init(&vibdata.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vibdata.timer.function = vibrator_timer_func;
	INIT_WORK(&vibdata.work, vibrator_work);


	wake_lock_init(&vibdata.wklock, WAKE_LOCK_SUSPEND, "vibrator");
	mutex_init(&vibdata.lock);

	printk(KERN_ALERT"drv2604: initialized on M3\n");

	ImmVibeSPI_ForceOut_AmpDisable(0);

	return VIBE_S_SUCCESS;
}

IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Terminate(void)
{
	DbgOut((DBL_VERBOSE, "ImmVibeSPI_ForceOut_Terminate.\n"));

	if (g_autotune_brake_enabled && g_workqueue)
	{
		destroy_workqueue(g_workqueue);
		g_workqueue = 0;
	}

	ImmVibeSPI_ForceOut_AmpDisable(0);

	gpio_direction_output(GPIO_VIBTONE_EN1, GPIO_LEVEL_LOW);
	gpio_free(GPIO_VIBTONE_EN1);

	i2c_del_driver(&drv2604_driver);

	i2c_unregister_device(g_pTheClient);

	return VIBE_S_SUCCESS;
}

IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetSamples(VibeUInt8 nActuatorIndex,
		VibeUInt16 nOutputSignalBitDepth, VibeUInt16 nBufferSizeInBytes,
		VibeInt8* pForceOutputBuffer)
{
	if(g_autotune_brake_enabled) {
		VibeInt8 force = pForceOutputBuffer[0];
		if (force > 0 && g_lastForce <= 0) {
			g_brake = false;

			ImmVibeSPI_ForceOut_AmpEnable(nActuatorIndex);
		} else if (force <= 0 && g_lastForce > 0) {
			g_brake = force < 0;

			ImmVibeSPI_ForceOut_AmpDisable(nActuatorIndex);
		}

		if (g_lastForce != force) {
#if DRV2604_USE_RTP_MODE
			if (force > 0)
				drv2604_set_rtp_val(pForceOutputBuffer[0]);
#endif
#if DRV2604_USE_PWM_MODE
			u32 uForce;
			if (pForceOutputBuffer[0] != 0) {
				uForce = (pForceOutputBuffer[0] > 0)?(pForceOutputBuffer[0]):0;


				DbgOut((DBL_VERBOSE, "ImmVibeSPI_ForceOut_SetSamples(%d)\n", uForce));

				if (uForce > 126)
					uForce = 256;
				else
					uForce += 128;
				pwm_duty_enable(vibdata.pwm_dev, uForce);
			} else {

				DbgOut((DBL_VERBOSE, "ImmVibeSPI_ForceOut_SetSamples(0)\n"));

				pwm_duty_enable(vibdata.pwm_dev, 0);
			}
#endif
		}
		g_lastForce = force;
	} else {
#if DRV2604_USE_RTP_MODE
		drv2604_set_rtp_val(pForceOutputBuffer[0]);
#endif
#if DRV2604_USE_PWM_MODE
		u32 uForce;
		if (pForceOutputBuffer[0] != 0) {
			uForce = (pForceOutputBuffer[0] > 0) ? (pForceOutputBuffer[0]) : 0;


			DbgOut((DBL_VERBOSE, "SetSamples(%d)\n", uForce));

			if(uForce > 126)
				uForce = 256;
			else
				uForce += 128;
			pwm_duty_enable(vibdata.pwm_dev, uForce);
		} else {


			DbgOut((DBL_VERBOSE, "SetSamples(0)\n"));

			pwm_duty_enable(vibdata.pwm_dev, 0);
		}
#endif
	}

	g_bNeedToRestartPlayBack = false;

	return VIBE_S_SUCCESS;
}

IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetFrequency(VibeUInt8 nActuatorIndex,
		VibeUInt16 nFrequencyParameterID,
		VibeUInt32 nFrequencyParameterValue)
{
	if (nActuatorIndex != 0) return VIBE_S_SUCCESS;

	switch (nFrequencyParameterID)
	{
	case VIBE_KP_CFG_FREQUENCY_PARAM1:
		break;

	case VIBE_KP_CFG_FREQUENCY_PARAM2:
		break;

	case VIBE_KP_CFG_FREQUENCY_PARAM3:
		break;

	case VIBE_KP_CFG_FREQUENCY_PARAM4:
		break;

	case VIBE_KP_CFG_FREQUENCY_PARAM5:
		break;

	case VIBE_KP_CFG_FREQUENCY_PARAM6:
		break;
	}
	return VIBE_S_SUCCESS;
}

IMMVIBESPIAPI VibeStatus ImmVibeSPI_Device_GetName(VibeUInt8 nActuatorIndex, char *szDevName, int nSize)
{
	char szRevision[MAX_REVISION_STRING_SIZE];

	if ((!szDevName) || (nSize < 1)) return VIBE_E_FAIL;

	DbgOut((DBL_VERBOSE, "ImmVibeSPI_Device_GetName.\n"));

	switch (g_nDeviceID)
	{
	case DRV2605:
		strncpy(szDevName, "DRV2605", nSize-1);
		break;
	case DRV2604:
		strncpy(szDevName, "DRV2604", nSize-1);
		break;
	default:
		strncpy(szDevName, "Unknown", nSize-1);
		break;
	}

	sprintf(szRevision, " Rev:%d", (drv2604_read_reg(SILICON_REVISION_REG) & SILICON_REVISION_MASK));
	if ((strlen(szRevision) + strlen(szDevName)) < nSize - 1)
		strcat(szDevName, szRevision);

	szDevName[nSize - 1] = '\0';

	return VIBE_S_SUCCESS;
}
