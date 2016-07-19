/*
** =========================================================================
** Copyright (c) 2007-2013  Immersion Corporation.  All rights reserved.
** Copyright (C) 2016 XiaoMi, Inc.
**						  Immersion Corporation Confidential and Proprietary
**
** File:
**	 ImmVibeSPI.c
**
** Description:
**	 Device-dependent functions called by Immersion TSP API
**	 to control PWM duty cycle, amp enable/disable, save IVT file, etc...
**
**	 This file is provided for Generic Project
**
** =========================================================================
*/
#include <linux/string.h>	 /* for strncpy */

/*
** This SPI supports only one actuator.
*/
#define NUM_ACTUATORS 1

extern void isa1000_vib_set_level(int level);

/************ ISA1000 specific section end **********/

/*
** This function is necessary for the TSP Designer Bridge.
** Called to allocate the specified amount of space in the DIAG output buffer.
** OEM must review this function and verify if the proposed function is used in their software package.
*/
void* ImmVibeSPI_Diag_BufPktAlloc(int nLength)
{
	/* No need to be implemented */

	/* return allocate_a_buffer(nLength); */
	return NULL;
}

/*
** Called to disable amp (disable output force)
*/
VibeStatus ImmVibeSPI_ForceOut_AmpDisable(VibeUInt8 nActuatorIndex)
{
	/* Disable amp */
	/* Disable the ISA1000 output */
	/* Xiaomi TODO: This is only an example.
	**			  Please change to the GPIO procedures for
	**			  the Hong Mi 2A platform
	*/
	isa1000_vib_set_level(0);


	return VIBE_S_SUCCESS;
}

/*
** Called to enable amp (enable output force)
*/
VibeStatus ImmVibeSPI_ForceOut_AmpEnable(VibeUInt8 nActuatorIndex)
{
	/* Reset PWM frequency (only if necessary on Hong Mi 2A)*/
	/* To be implemented with appropriate hardware access macros */

	/* Set duty cycle to 50% (which correspond to the output level 0) */
	isa1000_vib_set_level(0);

	/* Enable amp */
	/* Enable the ISA1000 output */
	/* Xiaomi TODO: This is only an example.
	**			  Please change to the GPIO procedures for
	**			  the Hong Mi 2A platform
	*/


	return VIBE_S_SUCCESS;
}

/*
** Called at initialization time to set PWM freq, disable amp, etc...
*/
VibeStatus ImmVibeSPI_ForceOut_Initialize(void)
{
	/* Disable amp */
	ImmVibeSPI_ForceOut_AmpDisable(0);

	return VIBE_S_SUCCESS;
}

/*
** Called at termination time to set PWM freq, disable amp, etc...
*/
VibeStatus ImmVibeSPI_ForceOut_Terminate(void)
{
	/* Disable amp */
	ImmVibeSPI_ForceOut_AmpDisable(0);

	/* Set PWM frequency (only if necessary on Hong Mi 2A) */
	/* To be implemented with appropriate hardware access macros */

	/* Set duty cycle to 50% */
	/* i.e. output level to 0 */


	return VIBE_S_SUCCESS;
}

/*
** Called by the real-time loop to set force output, and enable amp if required
*/
VibeStatus ImmVibeSPI_ForceOut_SetSamples(VibeUInt8 nActuatorIndex, VibeUInt16 nOutputSignalBitDepth, VibeUInt16 nBufferSizeInBytes, VibeInt8* pForceOutputBuffer)
{
	/*
	** For ERM:
	**	  nBufferSizeInBytes should be equal to 1 if nOutputSignalBitDepth is equal to 8
	**	  nBufferSizeInBytes should be equal to 2 if nOutputSignalBitDepth is equal to 16
	*/

	/* Below based on assumed 8 bit PWM, other implementation are possible */

	/* M = 1, N = 256, 1 <= nDutyCycle <= (N-M) */

	/* Output force: nForce is mapped from [-127, 127] to [1, 255] */
	int level;

	if (nOutputSignalBitDepth == 8) {
		if (nBufferSizeInBytes != 1) {
			printk("%s: Only support single sample for ERM\n", __func__);
			return VIBE_E_FAIL;
		} else
			level = (signed char)(*pForceOutputBuffer);

	} else if (nOutputSignalBitDepth == 16) {
		if (nBufferSizeInBytes != 2) {
			printk("%s: Only support single sample for ERM\n", __func__);
			return VIBE_E_FAIL;
		} else {
			level = (signed short)(*((signed short*)(pForceOutputBuffer)));
			/* Quantize it to 8-bit value as ISA1200 only support 8-bit levels */
			level >>= 8;
		}
	} else {
		printk("%s: Invalid Output Force Bit Depth\n", __func__);
		return VIBE_E_FAIL;
	}

	printk(KERN_DEBUG "%s: level = %d\n", __func__, level);

	isa1000_vib_set_level(level);

	return VIBE_S_SUCCESS;
}

/*
** Called to set force output frequency parameters
*/
VibeStatus ImmVibeSPI_ForceOut_SetFrequency(VibeUInt8 nActuatorIndex, VibeUInt16 nFrequencyParameterID, VibeUInt32 nFrequencyParameterValue)
{
	/* Don't needed to be implemented on Hong Mi 2A*/

	return VIBE_S_SUCCESS;
}

/*
** Called to save an IVT data file (pIVT) to a file (szPathName)
*/
VibeStatus ImmVibeSPI_IVTFile_Save(const VibeUInt8 *pIVT, VibeUInt32 nIVTSize, const char *szPathname)
{
	/* Don't needed to be implemented on Hong Mi 2A*/

	return VIBE_S_SUCCESS;
}

/*
** Called to delete an IVT file
*/
VibeStatus ImmVibeSPI_IVTFile_Delete(const char *szPathname)
{
	/* Don't needed to be implemented on Hong Mi 2A*/

	return VIBE_S_SUCCESS;
}

/*
** Called to get the device name (device name must be returned as ANSI char)
*/
VibeStatus ImmVibeSPI_Device_GetName(VibeUInt8 nActuatorIndex, char *szDevName, int nSize)
{
	if ((!szDevName) || (nSize < 1))
		return VIBE_E_FAIL;

	strncpy(szDevName, "ISA1000LCT", nSize-1);
	szDevName[nSize - 1] = '\0';

	return VIBE_S_SUCCESS;
}

/*
** Called at initialization time to get the number of actuators
*/
VibeStatus ImmVibeSPI_Device_GetNum(void)
{
	return NUM_ACTUATORS;
}
