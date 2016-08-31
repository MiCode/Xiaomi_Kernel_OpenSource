/*
** =========================================================================
** File:
**     ImmVibeSPI.c
**
** Description:
**     Device-dependent functions called by Immersion TSP API
**     to control PWM duty cycle, amp enable/disable, save IVT file, etc...
**
** Portions Copyright (c) 2012 Immersion Corporation. All Rights Reserved.
** Copyright (C) 2016 XiaoMi, Inc.
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
#warning ********* Compiling SPI for DRV2604 using ERM actuator ************

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
#include <linux/delay.h>

/*
** Address of our device
*/
#define DEVICE_ADDR 0x5A

/*
** i2c bus that it sits on
*/
#define DEVICE_BUS  2

/*
** This SPI supports only one actuator.
*/
#define NUM_ACTUATORS 1

/*
** Name of the DRV2604 board
*/
#define DRV2604_BOARD_NAME   "DRV2604"

/*
** Go
*/
#define GO_REG 0x0C
#define GO     0x01
#define STOP   0x00

/*
** Status
*/
#define STATUS_REG          0x00
#define STATUS_DEFAULT      0x00

#define DIAG_RESULT_MASK    (1 << 3)
#define AUTO_CAL_PASSED     (0 << 3)
#define AUTO_CAL_FAILED     (1 << 3)
#define DIAG_GOOD           (0 << 3)
#define DIAG_BAD            (1 << 3)

#define DEV_ID_MASK (7 << 5)
#define DRV2605 (5 << 5)
#define DRV2604 (4 << 5)

/*
** Mode
*/
#define MODE_REG            0x01
#define MODE_STANDBY        0x40

#define DRV2604_MODE_MASK           0x07
#define MODE_INTERNAL_TRIGGER       0
#define MODE_REAL_TIME_PLAYBACK     5
#define MODE_DIAGNOSTICS            6
#define AUTO_CALIBRATION            7

#define MODE_STANDBY_MASK           0x40
#define MODE_READY                  1  /* default */
#define MODE_SOFT_STANDBY           0

#define MODE_RESET                  0x80

/*
** Real Time Playback
*/
#define REAL_TIME_PLAYBACK_REG      0x02

/*
** Library Selection
*/
#define LIBRARY_SELECTION_REG       0x03
#define LIBRARY_SELECTION_DEFAULT   0x00

/*
** Waveform Sequencer
*/
#define WAVEFORM_SEQUENCER_REG      0x04
#define WAVEFORM_SEQUENCER_REG2     0x05
#define WAVEFORM_SEQUENCER_REG3     0x06
#define WAVEFORM_SEQUENCER_REG4     0x07
#define WAVEFORM_SEQUENCER_REG5     0x08
#define WAVEFORM_SEQUENCER_REG6     0x09
#define WAVEFORM_SEQUENCER_REG7     0x0A
#define WAVEFORM_SEQUENCER_REG8     0x0B
#define WAVEFORM_SEQUENCER_MAX      8
#define WAVEFORM_SEQUENCER_DEFAULT  0x00

/*
** OverDrive Time Offset
*/
#define OVERDRIVE_TIME_OFFSET_REG  0x0D

/*
** Sustain Time Offset, postive
*/
#define SUSTAIN_TIME_OFFSET_POS_REG 0x0E

/*
** Sustain Time Offset, negative
*/
#define SUSTAIN_TIME_OFFSET_NEG_REG 0x0F

/*
** Brake Time Offset
*/
#define BRAKE_TIME_OFFSET_REG       0x10

/*
** Audio to Haptics Control
*/
#define AUDIO_HAPTICS_CONTROL_REG   0x11

#define AUDIO_HAPTICS_RECT_10MS     (0 << 2)
#define AUDIO_HAPTICS_RECT_20MS     (1 << 2)
#define AUDIO_HAPTICS_RECT_30MS     (2 << 2)
#define AUDIO_HAPTICS_RECT_40MS     (3 << 2)

#define AUDIO_HAPTICS_FILTER_100HZ  0
#define AUDIO_HAPTICS_FILTER_125HZ  1
#define AUDIO_HAPTICS_FILTER_150HZ  2
#define AUDIO_HAPTICS_FILTER_200HZ  3

/*
** Audio to Haptics Minimum Input Level
*/
#define AUDIO_HAPTICS_MIN_INPUT_REG 0x12

/*
** Audio to Haptics Maximum Input Level
*/
#define AUDIO_HAPTICS_MAX_INPUT_REG 0x13

/*
** Audio to Haptics Minimum Output Drive
*/
#define AUDIO_HAPTICS_MIN_OUTPUT_REG 0x14

/*
** Audio to Haptics Maximum Output Drive
*/
#define AUDIO_HAPTICS_MAX_OUTPUT_REG 0x15

/*
** Rated Voltage
*/
#define RATED_VOLTAGE_REG           0x16

/*
** Overdrive Clamp Voltage
*/
#define OVERDRIVE_CLAMP_VOLTAGE_REG 0x17

/*
** Auto Calibrationi Compensation Result
*/
#define AUTO_CALI_RESULT_REG        0x18

/*
** Auto Calibration Back-EMF Result
*/
#define AUTO_CALI_BACK_EMF_RESULT_REG 0x19

/*
** Feedback Control
*/
#define FEEDBACK_CONTROL_REG        0x1A

#define FEEDBACK_CONTROL_BEMF_ERM_GAIN0 0 /* 0.33x */
#define FEEDBACK_CONTROL_BEMF_ERM_GAIN1 1 /* 1.0x */
#define FEEDBACK_CONTROL_BEMF_ERM_GAIN2 2 /* 1.8x */
#define FEEDBACK_CONTROL_BEMF_ERM_GAIN3 3 /* 4.0x */

#define LOOP_RESPONSE_SLOW      (0 << 2)
#define LOOP_RESPONSE_MEDIUM    (1 << 2) /* default */
#define LOOP_RESPONSE_FAST      (2 << 2)
#define LOOP_RESPONSE_VERY_FAST (3 << 2)

#define FB_BRAKE_FACTOR_1X   (0 << 4) /* 1x */
#define FB_BRAKE_FACTOR_2X   (1 << 4) /* 2x */
#define FB_BRAKE_FACTOR_3X   (2 << 4) /* 3x (default) */
#define FB_BRAKE_FACTOR_4X   (3 << 4) /* 4x */
#define FB_BRAKE_FACTOR_6X   (4 << 4) /* 6x */
#define FB_BRAKE_FACTOR_8X   (5 << 4) /* 8x */
#define FB_BRAKE_FACTOR_16X  (6 << 4) /* 16x */
#define FB_BRAKE_DISABLED    (7 << 4)

#define FEEDBACK_CONTROL_MODE_ERM 0 /* default */

/*
** Control1
*/
#define Control1_REG            0x1B

#define STARTUP_BOOST_ENABLED   (1 << 7)
#define STARTUP_BOOST_DISABLED  (0 << 7) /* default */

/*
** Control2
*/
#define Control2_REG            0x1C

#define IDISS_TIME_MASK         0x03
#define IDISS_TIME_VERY_SHORT   0
#define IDISS_TIME_SHORT        1
#define IDISS_TIME_MEDIUM       2 /* default */
#define IDISS_TIME_LONG         3

#define BLANKING_TIME_MASK          0x0C
#define BLANKING_TIME_VERY_SHORT    (0 << 2)
#define BLANKING_TIME_SHORT         (1 << 2)
#define BLANKING_TIME_MEDIUM        (2 << 2) /* default */
#define BLANKING_TIME_VERY_LONG     (3 << 2)

#define AUTO_RES_GAIN_MASK         0x30
#define AUTO_RES_GAIN_VERY_LOW     (0 << 4)
#define AUTO_RES_GAIN_LOW          (1 << 4)
#define AUTO_RES_GAIN_MEDIUM       (2 << 4) /* default */
#define AUTO_RES_GAIN_HIGH         (3 << 4)

#define SOFT_BRAKE_MASK            0x40

#define BIDIR_INPUT_MASK           0x80
#define UNIDIRECT_INPUT            (0 << 7)
#define BIDIRECT_INPUT             (1 << 7) /* default */

/*
** Control3
*/
#define Control3_REG 0x1D

#define ERM_OpenLoop_Enabled (1 << 5)
#define NG_Thresh_1 (1 << 6)
#define NG_Thresh_2 (2 << 6)
#define NG_Thresh_3 (3 << 6)

/*
** Auto Calibration Memory Interface
*/
#define AUTOCAL_MEM_INTERFACE_REG   0x1E

#define AUTOCAL_TIME_150MS          (0 << 4)
#define AUTOCAL_TIME_250MS          (1 << 4)
#define AUTOCAL_TIME_500MS          (2 << 4)
#define AUTOCAL_TIME_1000MS         (3 << 4)

#define SILICON_REVISION_REG        0x3B
#define SILICON_REVISION_MASK       0x07

#define AUDIO_HAPTICS_MIN_INPUT_VOLTAGE     0x19
#define AUDIO_HAPTICS_MAX_INPUT_VOLTAGE     0x64
#define AUDIO_HAPTICS_MIN_OUTPUT_VOLTAGE    0x19
#define AUDIO_HAPTICS_MAX_OUTPUT_VOLTAGE    0xFF

#define DEFAULT_ERM_AUTOCAL_COMPENSATION    0x14
#define DEFAULT_ERM_AUTOCAL_BACKEMF         0x72

#define DEFAULT_DRIVE_TIME      0x17
#define MAX_AUTOCALIBRATION_ATTEMPT 2

/*
** Rated Voltage:
** Calculated using the formula r = v * 255 / 5.6
** where r is what will be written to the register
** and v is the rated voltage of the actuator.

** Overdrive Clamp Voltage:
** Calculated using the formula o = oc * 255 / 5.6
** where o is what will be written to the register
** and oc is the overdrive clamp voltage of the actuator.
*/
#define GO_BIT_POLL_INTERVAL    15

#define REAL_TIME_PLAYBACK_CALIBRATION_STRENGTH 0x38 /* ~44% of overdrive voltage (open loop) */
#define ERM_RATED_VOLTAGE                       0x3E
#define ERM_OVERDRIVE_CLAMP_VOLTAGE             0x90

static int g_nDeviceID = -1;
static struct i2c_client *g_pTheClient = NULL;
static bool g_bAmpEnabled = false;
static bool g_bNeedToRestartPlayBack = false;

static const unsigned char ERM_autocal_sequence[] = {
	MODE_REG,                       AUTO_CALIBRATION,
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
	OVERDRIVE_TIME_OFFSET_REG,      0x00,
	SUSTAIN_TIME_OFFSET_POS_REG,    0x00,
	SUSTAIN_TIME_OFFSET_NEG_REG,    0x00,
	BRAKE_TIME_OFFSET_REG,          0x00,
	AUDIO_HAPTICS_CONTROL_REG,      AUDIO_HAPTICS_RECT_20MS | AUDIO_HAPTICS_FILTER_125HZ,
	AUDIO_HAPTICS_MIN_INPUT_REG,    AUDIO_HAPTICS_MIN_INPUT_VOLTAGE,
	AUDIO_HAPTICS_MAX_INPUT_REG,    AUDIO_HAPTICS_MAX_INPUT_VOLTAGE,
	AUDIO_HAPTICS_MIN_OUTPUT_REG,   AUDIO_HAPTICS_MIN_OUTPUT_VOLTAGE,
	AUDIO_HAPTICS_MAX_OUTPUT_REG,   AUDIO_HAPTICS_MAX_OUTPUT_VOLTAGE,
	RATED_VOLTAGE_REG,              ERM_RATED_VOLTAGE,
	OVERDRIVE_CLAMP_VOLTAGE_REG,    ERM_OVERDRIVE_CLAMP_VOLTAGE,
	AUTO_CALI_RESULT_REG,           DEFAULT_ERM_AUTOCAL_COMPENSATION,
	AUTO_CALI_BACK_EMF_RESULT_REG,  DEFAULT_ERM_AUTOCAL_BACKEMF,
	FEEDBACK_CONTROL_REG,           FB_BRAKE_FACTOR_3X | LOOP_RESPONSE_MEDIUM | FEEDBACK_CONTROL_BEMF_ERM_GAIN2,
	Control1_REG,                   STARTUP_BOOST_ENABLED | DEFAULT_DRIVE_TIME,
	Control2_REG,                   BIDIRECT_INPUT | AUTO_RES_GAIN_MEDIUM | BLANKING_TIME_SHORT | IDISS_TIME_SHORT,
	Control3_REG,                   ERM_OpenLoop_Enabled | NG_Thresh_2,
	AUTOCAL_MEM_INTERFACE_REG,      AUTOCAL_TIME_500MS,
	GO_REG,                         GO,
};

static void drv2604_write_reg_val(const unsigned char *data, unsigned int size)
{
	int i = 0;

	if (size % 2 != 0)
		return;

	while (i < size) {
		i2c_smbus_write_byte_data(g_pTheClient, data[i], data[i+1]);
		i += 2;
	}
}

static void drv2604_set_go_bit(char val)
{
	char go[] = {
		GO_REG, val
	};
	drv2604_write_reg_val(go, sizeof(go));
}

static unsigned char drv2604_read_reg(unsigned char reg)
{
	return i2c_smbus_read_byte_data(g_pTheClient, reg);
}

static void drv2604_poll_go_bit(void)
{
	while (drv2604_read_reg(GO_REG) == GO)
		schedule_timeout_interruptible(msecs_to_jiffies(GO_BIT_POLL_INTERVAL));
}

static void drv2604_set_rtp_val(char value)
{
	char rtp_val[] = {
		REAL_TIME_PLAYBACK_REG, value
	};
	drv2604_write_reg_val(rtp_val, sizeof(rtp_val));
}

static void drv2604_change_mode(char mode)
{
	unsigned char tmp[] = {
		MODE_REG, mode
	};
	drv2604_write_reg_val(tmp, sizeof(tmp));
}

static int drv2604_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int drv2604_remove(struct i2c_client *client);
static const struct i2c_device_id drv2604_id[] = {
	{DRV2604_BOARD_NAME, 0},
	{}
};

static struct i2c_board_info info = {
	I2C_BOARD_INFO(DRV2604_BOARD_NAME, DEVICE_ADDR),
};

static struct i2c_driver drv2604_driver = {
	.probe = drv2604_probe,
	.remove = drv2604_remove,
	.id_table = drv2604_id,
	.driver = {
		.name = DRV2604_BOARD_NAME,
	},
};

static int drv2604_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	char status;
	int nCalibrationCount = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		DbgOut((DBL_ERROR, "drv2605 probe failed"));
		return -ENODEV;
	}

	/* Wait 30 us */
	udelay(30);
	g_pTheClient = client;

	/* Run auto-calibration */
	do {
		drv2604_write_reg_val(ERM_autocal_sequence, sizeof(ERM_autocal_sequence));

		/* Wait until the procedure is done */
		drv2604_poll_go_bit();

		/* Read status */
		status = drv2604_read_reg(STATUS_REG);

		nCalibrationCount++;

	} while (((status & DIAG_RESULT_MASK) == AUTO_CAL_FAILED) && (nCalibrationCount < MAX_AUTOCALIBRATION_ATTEMPT));

	/* Check result */
	if ((status & DIAG_RESULT_MASK) == AUTO_CAL_FAILED) {
		DbgOut((DBL_ERROR, "drv2604 auto-calibration failed after %d attempts.\n", nCalibrationCount));
	} else {
		/* Read calibration results */
		drv2604_read_reg(AUTO_CALI_RESULT_REG);
		drv2604_read_reg(AUTO_CALI_BACK_EMF_RESULT_REG);
		drv2604_read_reg(FEEDBACK_CONTROL_REG);
	}

	/* Read device ID */
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

	/* Put hardware in standby */
	drv2604_change_mode(MODE_STANDBY);

	DbgOut((DBL_INFO, "drv2604 probe succeeded"));

	return 0;
}

static int drv2604_remove(struct i2c_client *client)
{
	DbgOut((DBL_VERBOSE, "drv2604_remove.\n"));
	return 0;
}

/*
** Called to disable amp (disable output force)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpDisable(VibeUInt8 nActuatorIndex)
{
	if (g_bAmpEnabled) {
		DbgOut((DBL_VERBOSE, "ImmVibeSPI_ForceOut_AmpDisable.\n"));

		/* Set the force to 0 */
		drv2604_set_rtp_val(0);

		/* Put hardware in standby */
		drv2604_change_mode(MODE_STANDBY);

		g_bAmpEnabled = false;
	}
	return VIBE_S_SUCCESS;
}

/*
** Called to enable amp (enable output force)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpEnable(VibeUInt8 nActuatorIndex)
{
	if (!g_bAmpEnabled) {
		DbgOut((DBL_VERBOSE, "ImmVibeSPI_ForceOut_AmpEnable.\n"));
		drv2604_change_mode(MODE_REAL_TIME_PLAYBACK);
		g_bAmpEnabled = true;
		g_bNeedToRestartPlayBack = true;
	}
	return VIBE_S_SUCCESS;
}

/*
** Called at initialization time.
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Initialize(void)
{
	struct i2c_adapter *adapter;
	struct i2c_client *client;

	DbgOut((DBL_VERBOSE, "ImmVibeSPI_ForceOut_Initialize.\n"));
	g_bAmpEnabled = true;   /* to force ImmVibeSPI_ForceOut_AmpDisable disabling the amp */
	adapter = i2c_get_adapter(DEVICE_BUS);

	if (adapter) {
		client = i2c_new_device(adapter, &info);

		if (client) {
			int retVal = i2c_add_driver(&drv2604_driver);

			if (retVal)
				return VIBE_E_FAIL;

		} else {
			DbgOut((DBL_VERBOSE, "drv2605: Cannot create new device.\n"));
			return VIBE_E_FAIL;
		}

	} else {
		DbgOut((DBL_VERBOSE, "ImmVibeSPI_ForceOut_AmpDisable.\n"));

		return VIBE_E_FAIL;
	}

	ImmVibeSPI_ForceOut_AmpDisable(0);
	return VIBE_S_SUCCESS;
}

/*
** Called at termination time to disable amp, etc...
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Terminate(void)
{
	DbgOut((DBL_VERBOSE, "ImmVibeSPI_ForceOut_Terminate.\n"));
	ImmVibeSPI_ForceOut_AmpDisable(0);

	/* Remove TS5000 driver */
	i2c_del_driver(&drv2604_driver);

	/* Reverse i2c_new_device */
	i2c_unregister_device(g_pTheClient);
	return VIBE_S_SUCCESS;
}

/*
** Called by the real-time loop to set the force
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetSamples(VibeUInt8 nActuatorIndex, VibeUInt16 nOutputSignalBitDepth, VibeUInt16 nBufferSizeInBytes, VibeInt8 *pForceOutputBuffer)
{
	drv2604_set_rtp_val(pForceOutputBuffer[0]);

	if (g_bNeedToRestartPlayBack)
		drv2604_set_go_bit(GO);

	g_bNeedToRestartPlayBack = false;
	return VIBE_S_SUCCESS;
}

/*
** Called to set force output frequency parameters
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetFrequency(VibeUInt8 nActuatorIndex, VibeUInt16 nFrequencyParameterID, VibeUInt32 nFrequencyParameterValue)
{
	if (nActuatorIndex != 0)
		return VIBE_S_SUCCESS;

	switch (nFrequencyParameterID) {
	case VIBE_KP_CFG_FREQUENCY_PARAM1:
		/* Update frequency parameter 1 */
		break;

	case VIBE_KP_CFG_FREQUENCY_PARAM2:
		/* Update frequency parameter 2 */
		break;

	case VIBE_KP_CFG_FREQUENCY_PARAM3:
		/* Update frequency parameter 3 */
		break;

	case VIBE_KP_CFG_FREQUENCY_PARAM4:
		/* Update frequency parameter 4 */
		break;

	case VIBE_KP_CFG_FREQUENCY_PARAM5:
		/* Update frequency parameter 5 */
		break;

	case VIBE_KP_CFG_FREQUENCY_PARAM6:
		/* Update frequency parameter 6 */
		break;
	}
	return VIBE_S_SUCCESS;
}

/*
** Called to get the device name (device name must be returned as ANSI char)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_Device_GetName(VibeUInt8 nActuatorIndex, char *szDevName, int nSize)
{
	if ((!szDevName) || (nSize < 1))
		return VIBE_E_FAIL;

	DbgOut((DBL_VERBOSE, "ImmVibeSPI_Device_GetName.\n"));

	switch (g_nDeviceID) {
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

	szDevName[nSize - 1] = '\0'; /* make sure the string is NULL terminated */
	return VIBE_S_SUCCESS;
}
