/*
** =========================================================================
** File:
**     ImmVibeSPI.c
**
** Description:
**     Device-dependent functions called by Immersion TSP API
**     to control PWM duty cycle, amp enable/disable, save IVT file, etc...
**
** Portions Copyright (c) 2008-2012 Immersion Corporation. All Rights Reserved.
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

#warning ************ Compiling DRV2603 SPI ************

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
** This SPI supports only one actuator.
*/
#define NUM_ACTUATORS 1

/*
** LRA PWM and GPIO
*/
#define PWM_PERIOD	44642	/* 89284 / 2 - 22.4kHz */
#define PWM_DUTY_50	22321	/* 50% duty cycle */
#define GPD0_TOUT_1	2 << 4

/*
** Set the PWM signal for zero magnitude
*/
#define PWM_BRAKING_MAGNITUDE  PWM_PERIOD - 446
#define PWM_ZERO_MAGNITUDE  PWM_DUTY_50

/*
** Global variables
*/
static bool g_bAmpEnabled = false;
static struct pwm_device *g_pPWMDev;

/*
** Called to disable amp (disable output force)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpDisable(VibeUInt8 nActuatorIndex)
{
	if (g_bAmpEnabled) {

		g_bAmpEnabled = false;

		/* Disable LRA PWM */
		gpio_direction_output(GPIO_VIBTONE_EN1, GPIO_LEVEL_LOW);
		pwm_disable(g_pPWMDev);

		/* For extra safety in case amp enable/disable circuity somehow not present */
		pwm_config(g_pPWMDev, PWM_ZERO_MAGNITUDE, PWM_PERIOD);
	}

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
	if (!g_bAmpEnabled) {
		g_bAmpEnabled = true;

		/* Enable PWM for LRA */
		gpio_direction_output(GPIO_VIBTONE_EN1, GPIO_LEVEL_HIGH);
		pwm_config(g_pPWMDev, PWM_ZERO_MAGNITUDE, PWM_PERIOD);
		pwm_enable(g_pPWMDev);

		schedule_timeout_interruptible(msecs_to_jiffies(1));
	}
}

/*
** Called at initialization time to set PWM freq, disable amp, etc...
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Initialize(void)
{

	/* Init LRA controller */
	if (gpio_request(GPIO_VIBTONE_EN1, "vibrator-en") >= 0) {
		s3c_gpio_cfgpin(GPIO_VIBTONE_PWM, GPD0_TOUT_1);

		g_pPWMDev = pwm_request(1, "vibrator-pwm");
		if (IS_ERR(g_pPWMDev)) {
			gpio_free(GPIO_VIBTONE_EN1);
			return VIBE_E_FAIL;
		}
	} else
		return VIBE_E_FAIL;

	/* Disable amp */
	g_bAmpEnabled = true;   /* to force ImmVibeSPI_ForceOut_AmpDisable disabling the amp */
	ImmVibeSPI_ForceOut_AmpDisable(0);

	return VIBE_S_SUCCESS;
}

/*
** Called at termination time to set PWM freq, disable amp, etc...
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Terminate(void)
{

	/* Disable amp */
	ImmVibeSPI_ForceOut_AmpDisable(0);
	gpio_direction_output(GPIO_VIBTONE_EN1, GPIO_LEVEL_LOW);
	pwm_free(g_pPWMDev);
	gpio_free(GPIO_VIBTONE_EN1);

	return VIBE_S_SUCCESS;
}

/*
** Called to set force output frequency parameters
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetFrequency(VibeUInt8 nActuatorIndex,
	VibeUInt16 nFrequencyParameterID, VibeUInt32 nFrequencyParameterValue)
{
	return VIBE_S_SUCCESS;
}

/*
** Called by the real-time loop to set force output, and enable amp if required
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetSamples(VibeUInt8 nActuatorIndex,
	VibeUInt16 nOutputSignalBitDepth, VibeUInt16 nBufferSizeInBytes, VibeInt8 *pForceOutputBuffer)
{
	/* Empty buffer is okay */
	if (0 == nBufferSizeInBytes)
		return VIBE_S_SUCCESS;

	if ((0 == nActuatorIndex) && (8 == nOutputSignalBitDepth) && (1 == nBufferSizeInBytes)) {
		VibeInt8 force = pForceOutputBuffer[0];

		if (force == 0) {

			ImmVibeSPI_ForceOut_AmpDisable(0);
		} else {
			AmpEnable(0);
			if (force > 0) {

				unsigned int duty = ((unsigned int)(128 - force) * (unsigned int)PWM_PERIOD) >> 8;

				pwm_config(g_pPWMDev, duty, PWM_PERIOD);
			} else
				pwm_config(g_pPWMDev, PWM_BRAKING_MAGNITUDE, PWM_PERIOD);
		}
	} else
		return VIBE_E_FAIL;

	return VIBE_S_SUCCESS;
}

/*
** Called to get the device name (device name must be returned as ANSI char)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_Device_GetName(VibeUInt8 nActuatorIndex, char *szDevName, int nSize)
{
	char szDeviceName[VIBE_MAX_DEVICE_NAME_LENGTH] = "TS3000 Device";

	if ((strlen(szDeviceName) + 8) >= nSize)
		return VIBE_E_FAIL;

	sprintf(szDevName, "%s DRV2603", szDeviceName);

	return VIBE_S_SUCCESS;
}
