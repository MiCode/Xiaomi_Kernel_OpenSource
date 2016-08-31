/*
** =========================================================================
** File:
**     ImmVibeSPI.c
**
** Description:
**     Device-dependent functions called by Immersion TSP API
**     to control PWM duty cycle, amp enable/disable, save IVT file, etc...
**
** Portions Copyright (c) 2011-2012 Immersion Corporation. All Rights Reserved.
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

#warning ************ Compiling ADUX1001 SPI ************

#include <linux/i2c.h>
#include <linux/wakelock.h>
#include <linux/io.h>
#include <linux/pwm.h>
#include <linux/gpio.h>
#include <mach/gpio-herring.h>

#ifdef IMMVIBESPIAPI
#undef IMMVIBESPIAPI
#endif
#define IMMVIBESPIAPI static

/*
** Specify polarity used for SHUTDOWN (amplifier enable) pin in HW
** Use 0 if SHUTDOWN is active low, and 1 if active high
*/
#define SHUTDOWN_POL    0

/*
** Run calibration command
**
** Note: the ADUX1001 requires that calibration data be stored in
** non-volatile memory, as the calibration values will be lost when
** the ADUX1001 is put in low power mode. The ADUX1001 does NOT
** include a means to save these values.
*/
#define CALIBRATE 1

/*
** Name of the ADUX1001 board
*/
#define ADUX1001_BOARD_NAME     "adux1001"
#define ADUX1001_DEVICE_ADDR    0x16
#define ADUX1001_DEVICE_BUS     2

/*
** This SPI supports only one actuator.
*/
#define NUM_ACTUATORS 1

#define NUM_EXTRA_BUFFERS 0

/*
** Init data size
*/
#define INITDATA1_SIZE 15
#define INITDATA2_SIZE 12

/*
** LRA PWM and GPIO
*/
#define PWM_PERIOD			  44642   /* 89284 / 2 - 22.4kHz */
#define PWM_DUTY_50			 22321   /* 50% duty cycle */
#define PWM_ZERO_MAGNITUDE      0
#define PWM_BRAKING_MAGNITUDE   ((3*PWM_PERIOD) >> 7) /* 2% duty cycle (127*0.02) */
#define GPD0_TOUT_1			 2 << 4

/*
** Global variables
*/
static bool g_bAmpEnabled = false;
static struct i2c_client *g_pTheClient = NULL;
static struct pwm_device *g_pPWMDev;
static char g_szFWVersion[VIBE_MAX_DEVICE_NAME_LENGTH];
static VibeInt8 g_i2cBuf[VIBE_OUTPUT_SAMPLE_SIZE+1];
static struct workqueue_struct *g_workqueueStruct = 0;
struct semaphore g_hSemaphoreAmpDisableEnable;
static VibeInt8 g_nTrailingNegativeForce = 0;

static void AmpDisableWorkQueueHandler(struct work_struct *w);
DECLARE_DELAYED_WORK(g_AmpDisableHandler, AmpDisableWorkQueueHandler);

/* Wait time should be at least 3x LRA resonance cycles */
#define WAIT_TIME_BEFORE_DISABLE_AMP_MS 20
#define AMP_ENABLE_DISABLE_TIMEOUT_MS   20

static void autotune_init(void);
static int adux1001_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int adux1001_remove(struct i2c_client *client);

static const struct i2c_device_id adux1001_id[] = {
	{ADUX1001_BOARD_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, adux1001_id);

static struct i2c_driver adux1001_driver = {
	.probe = adux1001_probe,
	.remove = adux1001_remove,
	.id_table = adux1001_id,
	.driver = {
		.name = ADUX1001_BOARD_NAME,
	},
};

static struct i2c_board_info adux1001_info = {
	I2C_BOARD_INFO(ADUX1001_BOARD_NAME, ADUX1001_DEVICE_ADDR),
};

#define INIT_AAC_NISUX			  0
#define INIT_BLUECOM_BVM1030        1
#define INIT_SEMCO_DMJBRN1030       2
#define INIT_SEMCO_DMJBRN1036       3

#define INIT_DATA   INIT_SEMCO_DMJBRN1036

#if (INIT_DATA == INIT_AAC_NISUX)
static VibeInt8 g_initData1[INITDATA1_SIZE] = {
	0x02,
	0x68, /* ADUX1001_CONFIG - default to PWM mode */
	0x34, /* ADUX1001_OUTPUT_CONFIG */
	0x01, /* ADUX1001_OUTPUT_RATE */
	0x0A, /* ADUX1001_ARB_SEL */
	0x00, /* ADUX1001_ARB0 */
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00 /* ADUX1001_ARB9 */
};
#define ADUX1001_DEFAULT_CAL0   0x00
#define ADUX1001_DEFAULT_CAL1   0x40
#define ADUX1001_DEFAULT_CAL2   0x03
#define ADUX1001_DEFAULT_CAL3   0xFA
static VibeInt8 g_initData2[INITDATA2_SIZE] = {
	0x11,
	0x00, /* ADUX1001_ACTIVE */
	0x48, /* ADUX1001_CALIBRATE */
	ADUX1001_DEFAULT_CAL0, /* ADUX1001_SLRA_CAL0 */
	ADUX1001_DEFAULT_CAL1,
	ADUX1001_DEFAULT_CAL2,
	ADUX1001_DEFAULT_CAL3, /* ADUX1001_SLRA_CAL3 */
	0x63, /* ADUX1001_RESERVED0 */
	0x90,
	0x19,
	0xE8,
	0x00 /* ADUX1001_RESERVED4 */
};
#elif (INIT_DATA == INIT_BLUECOM_BVM1030)
static VibeInt8 g_initData1[INITDATA1_SIZE] = {
	0x02,
	0x68, /* ADUX1001_CONFIG - default to PWM mode */
	0x34, /* ADUX1001_OUTPUT_CONFIG */
	0x01, /* ADUX1001_OUTPUT_RATE */
	0x0A, /* ADUX1001_ARB_SEL */
	0x00, /* ADUX1001_ARB0 */
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00 /* ADUX1001_ARB9 */
};
#define ADUX1001_DEFAULT_CAL0   0x00
#define ADUX1001_DEFAULT_CAL1   0x28
#define ADUX1001_DEFAULT_CAL2   0x02
#define ADUX1001_DEFAULT_CAL3   0xEF
static VibeInt8 g_initData2[INITDATA2_SIZE] = {
	0x11,
	0x00, /* ADUX1001_ACTIVE */
	0x48, /* ADUX1001_CALIBRATE */
	ADUX1001_DEFAULT_CAL0, /* ADUX1001_SLRA_CAL0 */
	ADUX1001_DEFAULT_CAL1,
	ADUX1001_DEFAULT_CAL2,
	ADUX1001_DEFAULT_CAL3, /* ADUX1001_SLRA_CAL3 */
	0x64, /* ADUX1001_RESERVED0 */
	0x50,
	0x15,
	0xF8,
	0x00 /* ADUX1001_RESERVED4 */
};
#elif (INIT_DATA == INIT_SEMCO_DMJBRN1030)
static VibeInt8 g_initData1[INITDATA1_SIZE] = {
	0x02,
	0x68, /* ADUX1001_CONFIG - default to PWM mode */
	0x34, /* ADUX1001_OUTPUT_CONFIG */
	0x01, /* ADUX1001_OUTPUT_RATE */
	0x0A, /* ADUX1001_ARB_SEL */
	0x00, /* ADUX1001_ARB0 */
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00 /* ADUX1001_ARB9 */
};
#define ADUX1001_DEFAULT_CAL0   0x00
#define ADUX1001_DEFAULT_CAL1   0x40
#define ADUX1001_DEFAULT_CAL2   0x03
#define ADUX1001_DEFAULT_CAL3   0x64
static VibeInt8 g_initData2[INITDATA2_SIZE] = {
	0x11,
	0x00, /* ADUX1001_ACTIVE  */
	0x48, /* ADUX1001_CALIBRATE */
	ADUX1001_DEFAULT_CAL0, /* ADUX1001_SLRA_CAL0 */
	ADUX1001_DEFAULT_CAL1,
	ADUX1001_DEFAULT_CAL2,
	ADUX1001_DEFAULT_CAL3, /* ADUX1001_SLRA_CAL3 */
	0x64, /* ADUX1001_RESERVED0 */
	0x90,
	0x55,
	0xF8,
	0x00 /* ADUX1001_RESERVED4 */
};
#elif (INIT_DATA == INIT_SEMCO_DMJBRN1036)
static VibeInt8 g_initData1[INITDATA1_SIZE] = {
	0x02,
	0x68, /* ADUX1001_CONFIG - default to PWM mode */
	0x34, /* ADUX1001_OUTPUT_CONFIG */
	0x01, /* ADUX1001_OUTPUT_RATE */
	0x0A, /* ADUX1001_ARB_SEL */
	0x00, /* ADUX1001_ARB0 */
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00 /* ADUX1001_ARB9 */
};
#define ADUX1001_DEFAULT_CAL0   0x00
#define ADUX1001_DEFAULT_CAL1   0x40
#define ADUX1001_DEFAULT_CAL2   0x03
#define ADUX1001_DEFAULT_CAL3   0x64
static VibeInt8 g_initData2[INITDATA2_SIZE] = {
	0x11,
	0x00, /* ADUX1001_ACTIVE */
	0x48, /* ADUX1001_CALIBRATE */
	ADUX1001_DEFAULT_CAL0, /* ADUX1001_SLRA_CAL0 */
	ADUX1001_DEFAULT_CAL1,
	ADUX1001_DEFAULT_CAL2,
	ADUX1001_DEFAULT_CAL3, /* ADUX1001_SLRA_CAL3 */
	0x64, /* ADUX1001_RESERVED0 */
	0x90,
	0x55,
	0xF8,
	0x00 /* ADUX1001_RESERVED4 */
};
#endif

#ifdef CALIBRATE
#define NUM_CALIBRATE_RETRIES   5
/*
** ADUX1001 calibration data struct (copied from linux/input/adux1001.h).
*/
struct adux1001_calib_data {
	unsigned char slra_cal0;
	unsigned char slra_cal1;
	unsigned char slra_cal2;
	unsigned char slra_cal3;
} __packed;

static int calibrate_adux1001(struct adux1001_calib_data *calib_data);
static int get_calibdata(struct adux1001_calib_data *calib_data);
static int store_calibdata(struct adux1001_calib_data *calib_data);
static struct adux1001_calib_data g_calib_data = {
	.slra_cal0 = ADUX1001_DEFAULT_CAL0,
	.slra_cal1 = ADUX1001_DEFAULT_CAL1,
	.slra_cal2 = ADUX1001_DEFAULT_CAL2,
	.slra_cal3 = ADUX1001_DEFAULT_CAL3
};
#endif /* CALIBRATE */

/*
** Send the initialization sequence. Per ADI's specifications,
** this is required after every time the ADUX1001 is powered down.
** Initialization takes ~1.2ms @400kHz, and ~4ms @100kHz.
** Re-initialization is not required if the ADUX1001 is left powered on.
*/
static void autotune_init(void)
{
	struct i2c_msg msg;
	msg.addr = g_pTheClient->addr;
	msg.flags = 0;
	msg.buf = g_i2cBuf;

	/* send reset message */
	g_i2cBuf[0] = 0x01; /* ADUX1001_CONTROL */
	g_i2cBuf[1] = 0x02;
	msg.len = 2;
	i2c_transfer(g_pTheClient->adapter, &msg, 1);

	/* init sequence 1 of 2 */
	 memcpy(g_i2cBuf, g_initData1, INITDATA1_SIZE);
	msg.len = 15;
	if (i2c_transfer(g_pTheClient->adapter, &msg, 1) < 0) {
		schedule_timeout_interruptible(msecs_to_jiffies(5));
		i2c_transfer(g_pTheClient->adapter, &msg, 1);
	}

	/* init sequence 2 of 2 */
	memcpy(g_i2cBuf, g_initData2, INITDATA2_SIZE);
	msg.len = 12;
	if (i2c_transfer(g_pTheClient->adapter, &msg, 1) < 0) {
		schedule_timeout_interruptible(msecs_to_jiffies(5));
		i2c_transfer(g_pTheClient->adapter, &msg, 1);
	}
}

static void AmpDisableWorkQueueHandler(struct work_struct *w)
{
	if (0 != down_timeout(&g_hSemaphoreAmpDisableEnable, msecs_to_jiffies(AMP_ENABLE_DISABLE_TIMEOUT_MS))) {
		DbgOut((DBL_ERROR, "AmpDisableWorkQueueHandler: down_timeout timed out.\n"));
		return;
	}

	if (!g_bAmpEnabled) {
		/* Don't shut down if a new AmpEnable call was received during the sleep time above */
#if (SHUTDOWN_POL == 1)
		gpio_direction_output(GPIO_VIBTONE_EN1, GPIO_LEVEL_HIGH);
#else
		gpio_direction_output(GPIO_VIBTONE_EN1, GPIO_LEVEL_LOW);
#endif
		/* Disable LRA PWM */
		pwm_disable(g_pPWMDev);

		/* For extra safety in case amp enable/disable circuity somehow not present */
		pwm_config(g_pPWMDev, PWM_ZERO_MAGNITUDE, PWM_PERIOD);
		/*
		** ADUX1001 power consumption is ~2mA while on, and ~2uA while off.
		** If the chip is powered off, it must be re-initialized.
		** ADI recommends not powering down the ADUX1001 for best performance.
		*/
	}
	up(&g_hSemaphoreAmpDisableEnable);
}

/*
** Called to disable amp (disable output force)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpDisable(VibeUInt8 nActuatorIndex)
{
	if (g_bAmpEnabled) {
		DbgOut((DBL_INFO, "ImmVibeSPI_ForceOut_AmpDisable.\n"));

	if (g_nTrailingNegativeForce == 0 || g_nTrailingNegativeForce > 3) {
			/*
			** Last bit of effect was not braking, so we should let it ring
			** or there was already sufficient brake time from the effect
			*/
#if (SHUTDOWN_POL == 1)
			gpio_direction_output(GPIO_VIBTONE_EN1, GPIO_LEVEL_HIGH);
#else
			gpio_direction_output(GPIO_VIBTONE_EN1, GPIO_LEVEL_LOW);
#endif
			pwm_disable(g_pPWMDev);

			/* For extra safety in case amp enable/disable circuity somehow not present */
			pwm_config(g_pPWMDev, PWM_ZERO_MAGNITUDE, PWM_PERIOD);
			/*
			** ADUX1001 power consumption is ~2mA while on, and ~2uA while off.
			** If the chip is powered off, it must be re-initialized.
			** ADI recommends not powering down the ADUX1001 for best performance.
			*/
		} else {
			if (g_workqueueStruct) {
				/*Delay the amp disable only by however many resonance cycles remain for braking */
				queue_delayed_work(g_workqueueStruct,
						&g_AmpDisableHandler,
						msecs_to_jiffies(WAIT_TIME_BEFORE_DISABLE_AMP_MS - (5*g_nTrailingNegativeForce)));
			}
		}
		g_nTrailingNegativeForce = 0;
		g_bAmpEnabled = false;
	}
	up(&g_hSemaphoreAmpDisableEnable);

	return VIBE_S_SUCCESS;
}

/*
** Called to enable amp (enable output force)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpEnable(VibeUInt8 nActuatorIndex)
{
	/*
	** This is called from tspdrv.c. We'll ignore that and use non-zero force
	** output only to enable the amp.
	*/
	return VIBE_S_SUCCESS;
}

/* This is the real implementation called only from this file for LRA when output force != 0 */
static void AmpEnable(VibeUInt8 nActuatorIndex)
{
	cancel_delayed_work_sync(&g_AmpDisableHandler);

	if (0 != down_timeout(&g_hSemaphoreAmpDisableEnable, msecs_to_jiffies(AMP_ENABLE_DISABLE_TIMEOUT_MS)))
		DbgOut((DBL_ERROR, "ImmVibeSPI_ForceOut_AmpEnable: down_timeout timed out.\n"));

	if (!g_bAmpEnabled) {
		DbgOut((DBL_INFO, "AmpEnable %d.\n", nActuatorIndex));
		g_bAmpEnabled = true;

		/* turn on amp (for adux1001 chip)*/
#if (SHUTDOWN_POL == 1)
		gpio_direction_output(GPIO_VIBTONE_EN1, GPIO_LEVEL_LOW);
#else
		gpio_direction_output(GPIO_VIBTONE_EN1, GPIO_LEVEL_HIGH);
#endif
		/* wait 200usecs for chip to power up */
		schedule_timeout_interruptible(usecs_to_jiffies(200));

		/*
		** ADUX1001 power consumption is ~2mA while on, and ~2uA while off.
		** If the chip is powered off, it must be re-initialized.
		*/
		autotune_init();

		/* enable LRA drive signal pin */
		pwm_enable(g_pPWMDev);
		pwm_config(g_pPWMDev, PWM_ZERO_MAGNITUDE, PWM_PERIOD);
	}
	up(&g_hSemaphoreAmpDisableEnable);
}

#ifdef CALIBRATE
/*
** Execute calibration command and update the calibration data struct
*/
static int calibrate_adux1001(struct adux1001_calib_data *calib_data)
{
	struct i2c_msg msg[2];  /* initialized below */
	int calibrationTimeoutCycles = NUM_CALIBRATE_RETRIES;

	/*
	** Make sure the amplifier is on
	** AmpEnable also sends the initialization sequence
	*/
	g_bAmpEnabled = false;
	g_initData1[1] = 0x64; /* initialize to I2C mode for calibration */
	AmpEnable(0);
	g_initData1[1] = 0x68; /* initialize to PWM for effect play */

	/* set up dedicated I2C read message */
	msg[1].addr = g_pTheClient->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = g_i2cBuf;

	/* get status */
	g_i2cBuf[0] = 0x1C;
	msg[0].addr = g_pTheClient->addr;
	msg[0].flags = 0;
	msg[0].buf = g_i2cBuf;
	msg[0].len = 1;
	i2c_transfer(g_pTheClient->adapter, msg, 2);

	/* send calibrate command, wait 90ms minimum */
	g_i2cBuf[0] = 0x12;
	g_i2cBuf[1] = g_initData2[2] | 0x01;
	msg[0].len = 2;
	i2c_transfer(g_pTheClient->adapter, msg, 1);
	schedule_timeout_interruptible(msecs_to_jiffies(90));

	/* get status again */
	msg[0].len = 1;
	g_i2cBuf[0] = 0x1C;
	i2c_transfer(g_pTheClient->adapter, msg, 2);
	while ((calibrationTimeoutCycles--) > 0 && ((g_i2cBuf[0] & 0x01) == 0x01)) {
		/* Just in case calibration is still not done, wait a bit longer */
		schedule_timeout_interruptible(msecs_to_jiffies(15));
		g_i2cBuf[0] = 0x1C;
		i2c_transfer(g_pTheClient->adapter, msg, 2);
	}

	if ((g_i2cBuf[0] & 0x08) != 0x08) {
		/* status okay, retrieve and store calibration values */
		g_i2cBuf[0] = 0x13;
		msg[1].len = 4;
		i2c_transfer(g_pTheClient->adapter, msg, 2);

		/* keep amplitude values */
		calib_data->slra_cal2 = g_i2cBuf[2];
		calib_data->slra_cal3 = g_i2cBuf[3];
		g_initData2[5] = calib_data->slra_cal2;
		g_initData2[6] = calib_data->slra_cal3;
		ImmVibeSPI_ForceOut_AmpDisable(0);
		return 0;
	} else {
		/* status not okay, clear the error */
		g_i2cBuf[0] = 0x01;
		g_i2cBuf[1] = 0x01;
		i2c_transfer(g_pTheClient->adapter, msg, 1);
		ImmVibeSPI_ForceOut_AmpDisable(0);
		return -EPERM;
	}
}

/*
** Read calibration data from a file in non-volatile memory.
** If the file does not exist or could not be opened, return -EPERM.
*/
static int get_calibdata(struct adux1001_calib_data *calib_data)
{
	/* Add implementation here if necessary */
	return -EPERM;
}

/*
** Store calibration data to a file in non-volatile memory.
*/
static int store_calibdata(struct adux1001_calib_data *calib_data)
{
	/* Add implementation here if necessary */
	return 0;
}
#endif

/*
** Called at initialization time to set PWM freq, disable amp, etc...
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Initialize(void)
{
#ifdef CALIBRATE
	int nRetries = NUM_CALIBRATE_RETRIES;
#endif
	int retval = -ENOMEM;
	struct i2c_adapter *adapter;
	struct i2c_client *client;

	DbgOut((DBL_INFO, "ImmVibeSPI_ForceOut_Initialize.\n"));

	sema_init(&g_hSemaphoreAmpDisableEnable, 1); /* initialize semaphore that synchronize Amp disanling/enabling functions */
	g_workqueueStruct = create_workqueue("tspdrv_disable_amp_workqueue"); /* create workqueue to handle amp disabling thread */

	/* Add ADUX1001 driver */
	adapter = i2c_get_adapter(ADUX1001_DEVICE_BUS);
	if (!adapter) {
		DbgOut((DBL_ERROR, "ADUX1001: Cannot get adapter\n"));
		return VIBE_E_FAIL;
	}

	client = i2c_new_device(adapter, &adux1001_info);
	if (!client) {
		DbgOut((DBL_ERROR, "ADUX1001: Cannot create new device \n"));
		return VIBE_E_FAIL;
	}

	retval = i2c_add_driver(&adux1001_driver);
	if (retval) {
		DbgOut((DBL_ERROR, "ADUX1001 driver initialization error \n"));
		i2c_unregister_device(g_pTheClient);
		return VIBE_E_FAIL;
	}

	/* Init LRA controller */
	if (gpio_request(GPIO_VIBTONE_EN1, "vibrator-en") >= 0) {
		s3c_gpio_cfgpin(GPIO_VIBTONE_PWM, GPD0_TOUT_1);

		g_pPWMDev = pwm_request(1, "vibrator-pwm");
		if (IS_ERR(g_pPWMDev)) {
			gpio_free(GPIO_VIBTONE_EN1);
		}
	}

#ifdef CALIBRATE
/*
** Note: the ADUX1001 requires that calibration data be stored in
** non-volatile memory, as the calibration values will be lost when
** the ADUX1001 is put in low power mode. The ADUX1001 does NOT
** include a means to save these values.
*/
if (0 > get_calibdata(&g_calib_data)) {
		while (calibrate_adux1001(&g_calib_data) < 0) {
			if ((nRetries--) == 0) {
				DbgOut((DBL_ERROR, "ADUX1001 calibration failed.\n"));
				return VIBE_E_FAIL;
			}
		}

		/* status okay, store calibration values */
		if (store_calibdata(&g_calib_data) < 0)
			DbgOut((DBL_ERROR, "ADUX1001 storing calibration data failed.\n"));
	}
#endif

	g_bAmpEnabled = true;   /* to force ImmVibeSPI_ForceOut_AmpDisable disabling the amp */
	ImmVibeSPI_ForceOut_AmpDisable(0);

	return VIBE_S_SUCCESS;
}

/*
** Called at termination time to set PWM freq, disable amp, etc...
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Terminate(void)
{
	DbgOut((DBL_INFO, "ImmVibeSPI_ForceOut_Terminate.\n"));

	/*
	** Disable amp.
	** If multiple actuators are supported, please make sure to call
	** ImmVibeSPI_ForceOut_AmpDisable for each actuator (provide the actuator index as
	** input argument).
	*/
	ImmVibeSPI_ForceOut_AmpDisable(0);

	/* Remove ADUX1001 driver */
	i2c_del_driver(&adux1001_driver);
	pwm_free(g_pPWMDev);
	gpio_free(GPIO_VIBTONE_EN1);

	if (g_workqueueStruct) {
		destroy_workqueue(g_workqueueStruct);
		g_workqueueStruct = 0;
	}

	return VIBE_S_SUCCESS;
}

/*
** Called by the real-time loop to set PWM duty cycle
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetSamples(VibeUInt8 nActuatorIndex, VibeUInt16 nOutputSignalBitDepth, VibeUInt16 nBufferSizeInBytes, VibeInt8 *pForceOutputBuffer)
{
	/* Empty buffer is okay */
	if (0 == nBufferSizeInBytes)
		return VIBE_S_SUCCESS;

	if ((0 == nActuatorIndex) && (8 == nOutputSignalBitDepth) && (1 == nBufferSizeInBytes)) {
	VibeInt8 force = pForceOutputBuffer[0];

		if (force == 0) {
			/* turn off the amp to disable autotune braking */
			ImmVibeSPI_ForceOut_AmpDisable(0);
		} else {
			AmpEnable(0);
			if (force >= 10) { /* 127*.08, to ensure we stay out of the unstable zone */
				/* ignore negative brake pulse */
				unsigned int duty = ((unsigned int)force * (unsigned int)PWM_PERIOD) >> 7;
				pwm_config(g_pPWMDev, (PWM_PERIOD - duty), PWM_PERIOD);
			} else { /* 0 < nForce < 10 */
				/*
				** don't use zero magnitude for braking
				** because the ADUX1001 will go to sleep
				*/
				pwm_config(g_pPWMDev, PWM_PERIOD - PWM_BRAKING_MAGNITUDE, PWM_PERIOD);
			}
		}
	} else
		return VIBE_E_FAIL;
	return VIBE_S_SUCCESS;
}

/*
** Called to set force output frequency parameters
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetFrequency(VibeUInt8 nActuatorIndex, VibeUInt16 nFrequencyParameterID, VibeUInt32 nFrequencyParameterValue)
{
	/* Ignore fequency parameters from the player */
	return VIBE_S_SUCCESS;
}

/*
** Called to get the device name (device name must be returned as ANSI char)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_Device_GetName(VibeUInt8 nActuatorIndex, char *szDevName, int nSize)
{
	char szDeviceName[VIBE_MAX_DEVICE_NAME_LENGTH] = "TS3000 Device";

	if ((strlen(szDeviceName) + 6 + strlen(g_szFWVersion)) >= nSize)
		return VIBE_E_FAIL;

	sprintf(szDevName, "%s %d %s", szDeviceName, VIBE_EDITION_LEVEL, g_szFWVersion);

	return VIBE_S_SUCCESS;
}

/*
** ADUX1001 callback functions
*/
static int adux1001_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int nRet = 0;
	struct i2c_msg msg;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		nRet = -ENODEV;
	else {
		/* make sure the amplifier is on */
		if (gpio_request(GPIO_VIBTONE_EN1, "vibrator-en") >= 0) {
		/*
		** Note: the ADUX1001 *must* be toggled on for 500us to ensure
		** correct operation; the toggling code here is intended as a
		** safety measure. Ideally this toggling should happen immediately
		** after power is supplied to the ADUX1001.
		*/
#if (SHUTDOWN_POL == 1)
			gpio_direction_output(GPIO_VIBTONE_EN1, GPIO_LEVEL_HIGH);
			schedule_timeout_interruptible(usecs_to_jiffies(200));
			gpio_direction_output(GPIO_VIBTONE_EN1, GPIO_LEVEL_LOW);
			schedule_timeout_interruptible(usecs_to_jiffies(500));
			gpio_direction_output(GPIO_VIBTONE_EN1, GPIO_LEVEL_HIGH);
			schedule_timeout_interruptible(usecs_to_jiffies(200));
			gpio_direction_output(GPIO_VIBTONE_EN1, GPIO_LEVEL_LOW);
#else
			gpio_direction_output(GPIO_VIBTONE_EN1, GPIO_LEVEL_LOW);
			schedule_timeout_interruptible(usecs_to_jiffies(200));
			gpio_direction_output(GPIO_VIBTONE_EN1, GPIO_LEVEL_HIGH);
			schedule_timeout_interruptible(usecs_to_jiffies(500));
			gpio_direction_output(GPIO_VIBTONE_EN1, GPIO_LEVEL_LOW);
			schedule_timeout_interruptible(usecs_to_jiffies(200));
			gpio_direction_output(GPIO_VIBTONE_EN1, GPIO_LEVEL_HIGH);
#endif /* SHUTDOWN_POL */
			schedule_timeout_interruptible(usecs_to_jiffies(200));
			g_pTheClient = client;
			msg.addr = g_pTheClient->addr;
			msg.flags = 0;
			msg.buf = g_i2cBuf;
			msg.len = 1;
			g_i2cBuf[0] = 0x1D;

			i2c_transfer(g_pTheClient->adapter, &msg, 1);
			msg.flags = I2C_M_RD;
			i2c_transfer(g_pTheClient->adapter, &msg, 1);

			g_i2cBuf[0] &= 0x07;

			if (g_i2cBuf[0] < 2)
				sprintf(g_szFWVersion, "[ADUX1001: ES1]");
			else if (g_i2cBuf[0] < 5)
				sprintf(g_szFWVersion, "[ADUX1001: ES%d]", g_i2cBuf[0]);
			else if (g_i2cBuf[0] == 5)
				sprintf(g_szFWVersion, "[ADUX1001i]");
			else
				sprintf(g_szFWVersion, "[ADUX1001: %d]", g_i2cBuf[0]);

			gpio_free(GPIO_VIBTONE_EN1);
		}
	}

	return nRet;
}

static int adux1001_remove(struct i2c_client *client)
{
	g_pTheClient = NULL;
	return 0;
}
