/*
** =========================================================================
** Copyright (c) 2007-2011  Immersion Corporation.  All rights reserved.
**                          Immersion Corporation Confidential and Proprietary
** Copyright (C) 2015 XiaoMi, Inc.
**
** File:
**     ImmVibeSPI.c
**
** Description:
**     Device-dependent functions called by Immersion TSP API
**     to control PWM duty cycle, amp enable/disable, save IVT file, etc...
**
**     This file is provided for Generic Project
**
** =========================================================================
*/
#include <ImmVibeCore.h>
#include <ImmVibeSPI.h>
#include <string.h>     /* for strncpy */

/*
** This function is necessary for the TSP Designer Bridge.
** Called to allocate the specified amount of space in the DIAG output buffer.
** OEM must review this function and verify if the proposed function is used in their software package.
*/
void* ImmVibeSPI_Diag_BufPktAlloc(int nLength)
{
#error Please implement

	/* return allocate_a_buffer(nLength); */
	return NULL;
}

/*
** Called to disable amp (disable output force)
*/
VibeStatus ImmVibeSPI_ForceOut_AmpDisable(VibeUInt8 nActuatorIndex)
{
#error Please implement

	/* Disable amp */
	/* To be implemented with appropriate hardware access macros */

	return VIBE_S_SUCCESS;
}

/*
** Called to enable amp (enable output force)
*/
VibeStatus ImmVibeSPI_ForceOut_AmpEnable(VibeUInt8 nActuatorIndex)
{
#error Please implement

	/* Reset PWM frequency */
	/* To be implemented with appropriate hardware access macros */

	/* Set duty cycle to 50% */
	/* To be implemented with appropriate hardware access macros */

	/* Enable amp */
	/* To be implemented with appropriate hardware access macros */

	return VIBE_S_SUCCESS;
}

/*
** Called at initialization time to set PWM freq, disable amp, etc...
*/
VibeStatus ImmVibeSPI_ForceOut_Initialize(void)
{
	/* Disable amp */
	ImmVibeSPI_ForceOut_AmpDisable(0);

#error Please implement

	/* Set PWM frequency */
	/* To be implemented with appropriate hardware access macros */

	/* Set duty cycle to 50% */
	/* To be implemented with appropriate hardware access macros */

	return VIBE_S_SUCCESS;
}

/*
** Called at termination time to set PWM freq, disable amp, etc...
*/
VibeStatus ImmVibeSPI_ForceOut_Terminate(void)
{
#error Please implement

	/* Disable amp */
	ImmVibeSPI_ForceOut_AmpDisable(0);

	/* Set PWM frequency */
	/* To be implemented with appropriate hardware access macros */

	/* Set duty cycle to 50% */
	/* To be implemented with appropriate hardware access macros */

	return VIBE_S_SUCCESS;
}

/*
** Called by the real-time loop to set force output, and enable amp if required
*/
VibeStatus ImmVibeSPI_ForceOut_SetSamples(VibeUInt8 nActuatorIndex, VibeUInt16 nOutputSignalBitDepth, VibeUInt16 nBufferSizeInBytes, VibeInt8* pForceOutputBuffer)
{
#error Please implement

	/*
	** For ERM:
	** nBufferSizeInBytes should be equal to 1 if nOutputSignalBitDepth is equal to 8
	** nBufferSizeInBytes should be equal to 2 if nOutputSignalBitDepth is equal to 16
	*/

	/* Below based on assumed 8 bit PWM, other implementation are possible */

	/* M = 1, N = 256, 1 <= nDutyCycle <= (N-M) */

	/* Output force: nForce is mapped from [-127, 127] to [1, 255] */
	/* To be implemented with appropriate hardware access macros */

	return VIBE_S_SUCCESS;
}

/*
** Called to set force output frequency parameters
*/
VibeStatus ImmVibeSPI_ForceOut_SetFrequency(VibeUInt8 nActuatorIndex, VibeUInt16 nFrequencyParameterID, VibeUInt32 nFrequencyParameterValue)
{
#error Please review the code between the #if and #endif

#if 0
	/*
	** The following code is provided as sample. If enabled, it will allow device
	** frequency parameters tuning via the ImmVibeSetDeviceKernelParameter API.
	** Please modify as required.
	*/
	switch (nFrequencyParameterID)
	{
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
#endif

	return VIBE_S_SUCCESS;
}

/*
** Called to save an IVT data file (pIVT) to a file (szPathName)
*/
VibeStatus ImmVibeSPI_IVTFile_Save(const VibeUInt8 *pIVT, VibeUInt32 nIVTSize, const char *szPathname)
{
#error Please implement

	/* To be implemented */

	return VIBE_S_SUCCESS;
}

/*
** Called to delete an IVT file
*/
VibeStatus ImmVibeSPI_IVTFile_Delete(const char *szPathname)
{
#error Please implement

	/* To be implemented */

	return VIBE_S_SUCCESS;
}

/*
** Called to get the device name (device name must be returned as ANSI char)
*/
VibeStatus ImmVibeSPI_Device_GetName(VibeUInt8 nActuatorIndex, char *szDevName, int nSize)
{
#error Please review the code between the #if and #endif

#if 0   /* The following code is provided as sample. Please modify as required. */
	if ((!szDevName) || (nSize < 1)) return VIBE_E_FAIL;

	strncpy(szDevName, "Generic", nSize-1);
	szDevName[nSize - 1] = '\0';	/* make sure the string is NULL terminated */
#endif

	return VIBE_S_SUCCESS;
}

/*
** Called at initialization time to get the number of actuators
*/
VibeStatus ImmVibeSPI_Device_GetNum(void)
{
	return 1;
}
