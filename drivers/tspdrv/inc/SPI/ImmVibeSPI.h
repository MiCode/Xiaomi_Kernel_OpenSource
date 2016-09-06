/*
** =========================================================================
** Copyright (c) 2004-2011  Immersion Corporation.  All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
**                          Immersion Corporation Confidential and Proprietary
**
** File:
**     ImmVibeSPI.h
**
** Description:
**     Device-dependent functions called by Immersion TSP API
**     to control PWM duty cycle, amp enable/disable, save IVT file, etc...
**
** =========================================================================
*/
#ifndef _IMMVIBESPI_H
#define _IMMVIBESPI_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef IMMVIBESPIAPI
    #if defined(WIN32)
        #define IMMVIBESPIAPI __declspec(dllimport)
    #else
        #define IMMVIBESPIAPI extern
    #endif
#endif

/*
** Frequency constant parameters to control force output values and signals.
*/
#define VIBE_KP_CFG_FREQUENCY_PARAM1        85
#define VIBE_KP_CFG_FREQUENCY_PARAM2        86
#define VIBE_KP_CFG_FREQUENCY_PARAM3        87
#define VIBE_KP_CFG_FREQUENCY_PARAM4        88
#define VIBE_KP_CFG_FREQUENCY_PARAM5        89
#define VIBE_KP_CFG_FREQUENCY_PARAM6        90

/*
** Called at initialization time to set PWM freq, disable amp, etc...
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Initialize(void);

/*
** Called at termination time to set PWM freq, disable amp, etc...
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Terminate(void);

/*
** Called to disable amp (disable output force)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpDisable(VibeUInt8 nActuatorIndex);

/*
** Called to enable amp (enable output force)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpEnable(VibeUInt8 nActuatorIndex);

/*
** Called to save an IVT data file (pIVT) to a file (szPathName)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_IVTFile_Save(const VibeUInt8 *pIVT, VibeUInt32 nIVTSize, const char *szPathname);

/*
** Called to delete an IVT file
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_IVTFile_Delete(const char *szPathname);

/*
** Called to get the device name (device name must be returned as ANSI char)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_Device_GetName(VibeUInt8 nActuatorIndex, char *szDevName, int nSize);

/*
** Called to send output force samples
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetSamples(VibeUInt8 nActuatorIndex, VibeUInt16 nOutputSignalBitDepth, VibeUInt16 nBufferSizeInBytes, VibeInt8 *pForceOutputBuffer);

/*
** Called to set output frequency parameters
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetFrequency(VibeUInt8 nActuatorIndex, VibeUInt16 nFrequencyParameterID, VibeUInt32 nFrequencyParameterValue);

/*
** Called at initialization time to get the number of actuators
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_Device_GetNum(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _IMMVIBESPI_H */
