/**
 * Copyright (C) 2007 NXP N.V., All Rights Reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of NXP N.V. and is confidential in nature. Under no circumstances
 * is this software to be  exposed to or placed under an Open Source License of
 * any type without the expressed written permission of NXP N.V.
 *
 * \file          tmdlHdmiTx_Functions.h
 *
 * \version       $Revision: 1 $
 *
 * \date          $Date: 02/08/07 8:32 $
 *
 * \brief         devlib driver component API for the TDA998x HDMI Transmitters
 *
 * \section refs  Reference Documents
 * HDMI Tx Driver - FRS.doc,
 * HDMI Tx Driver - tmdlHdmiTx - SCS.doc
 *
 * \section info  Change Information
 *
 * \verbatim

   $History: tmdlHdmiTx_Functions.h $
 *
 * *****************  Version 1  *****************
 * User: Demoment     Date: 02/08/07   Time: 8:32
 * Updated in $/Source/tmdlHdmiTx/inc
 * initial version
 *

   \endverbatim
 *
*/

#ifndef TMDLHDMITX_FUNCTIONS_H
#define TMDLHDMITX_FUNCTIONS_H

/*============================================================================*/
/*                       INCLUDE FILES                                        */
/*============================================================================*/

#include "tmNxTypes.h"
#include "tmdlHdmiTx_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/*                       EXTERN FUNCTION PROTOTYPES                           */
/*============================================================================*/

/*****************************************************************************/
/**
    \brief Get the software version of the driver.

    \param pSWVersion Pointer to the version structure.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxGetSWVersion(tmSWVersion_t *pSWVersion);

/*****************************************************************************/
/**
    \brief Get the number of available HDMI transmitters devices in the system.
	   A unit directly represents a physical device.

    \param pUnitCount Pointer to the number of available units.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxGetNumberOfUnits(UInt32 *pUnitCount);

/*****************************************************************************/
/**
    \brief Get the capabilities of unit 0. Capabilities are stored into a
	   dedicated structure and are directly read from the HW device.

    \param pCapabilities Pointer to the capabilities structure.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_UNIT_NUMBER: the unit number is wrong or
	      the receiver instance is not initialised
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMDL_ERR_DLHDMITX_BAD_PARAMETER: a parameter is invalid or out
	      of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxGetCapabilities(tmdlHdmiTxCapabilities_t *pCapabilities);

/*****************************************************************************/
/**
    \brief Get the capabilities of a specific unit. Capabilities are stored
	   into a dedicated structure and are directly read from the HW
	   device.

    \param unit          Unit to be probed.
    \param pCapabilities Pointer to the capabilities structure.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_UNIT_NUMBER: the unit number is wrong or
	      the receiver instance is not initialised
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMDL_ERR_DLHDMITX_BAD_PARAMETER: a parameter is invalid or out
	      of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxGetCapabilitiesM
	    (tmUnitSelect_t unit, tmdlHdmiTxCapabilities_t *pCapabilities);

/*****************************************************************************/
/**
    \brief Open unit 0 of HdmiTx driver and provides the instance number to
	   the caller. Note that one unit of HdmiTx represents one physical
	   HDMI transmitter and that only one instance per unit can be opened.

    \param pInstance Pointer to the variable that will receive the instance
		     identifier.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_UNIT_NUMBER: the unit number is wrong or
	      the transmitter instance is not initialised
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMDL_ERR_DLHDMITX_RESOURCE_OWNED: the resource is already in use
	    - TMDL_ERR_DLHDMITX_INVALID_STATE: the state is invalid for
	      the function
	    - TMDL_ERR_DLHDMITX_INIT_FAILED: the unit instance is already
	      initialised or something wrong happened at lower level.
	    - TMDL_ERR_DLHDMITX_NO_RESOURCES: the resource is not available
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: the unit is not initialized
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter is invalid or out
	      of range
	    - TMBSL_ERR_HDMI_INIT_FAILED: the unit instance is already
	      initialised
	    - TMBSL_ERR_HDMI_COMPATIBILITY: the driver is not compatiable
	      with the internal device version code
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
	    - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxOpen(tmInstance_t *pInstance);

/*****************************************************************************/
/**
    \brief Open a specific unit of HdmiTx driver and provides the instance
	   number to the caller. Note that one unit of HdmiTx represents one
	   physical HDMI transmitter and that only one instance per unit can be
	   opened. This function switches driver's state machine to
	   "initialized" state.

    \param pInstance Pointer to the structure that will receive the instance
		     identifier.
    \param unit      Unit number to be opened.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_UNIT_NUMBER: the unit number is wrong or
	      the transmitter instance is not initialised
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMDL_ERR_DLHDMITX_RESOURCE_OWNED: the resource is already in use
	    - TMDL_ERR_DLHDMITX_INVALID_STATE: the state is invalid for
	      the function
	    - TMDL_ERR_DLHDMITX_INIT_FAILED: the unit instance is already
	      initialised or something wrong happened at lower level.
	    - TMDL_ERR_DLHDMITX_NO_RESOURCES: the resource is not available
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: the unit is not initialized
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter is invalid or out
	      of range
	    - TMBSL_ERR_HDMI_INIT_FAILED: the unit instance is already
	      initialised
	    - TMBSL_ERR_HDMI_COMPATIBILITY: the driver is not compatiable
	      with the internal device version code
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
	    - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxOpenM(tmInstance_t *pInstance, tmUnitSelect_t unit);

/*****************************************************************************/
/**
    \brief Close an instance of HdmiTx driver.

    \param instance Instance identifier.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_RESOURCE_NOT_OWNED: the caller does not own
	      the resource
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxClose(tmInstance_t instance);

/*****************************************************************************/
/**
    \brief Set the power state of an instance of the HDMI transmitter.

    \param instance   Instance identifier.
    \param powerState Power state to set.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxSetPowerState(tmInstance_t instance, tmPowerState_t powerState);

/*****************************************************************************/
/**
    \brief Get the power state of an instance of the HDMI transmitter.

    \param instance    Instance identifier.
    \param pPowerState Pointer to the power state.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxGetPowerState(tmInstance_t instance, tmPowerState_t *pPowerState);

/*****************************************************************************/
/**
    \brief Set the configuration of instance attributes. This function is
	   required by DVP architecture rules but actually does nothing in this
	   driver

    \param instance    Instance identifier.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxInstanceConfig(tmInstance_t instance);

/*****************************************************************************/
/**
    \brief Setup the instance with its configuration parameters. This function
	   allows basic instance configuration like enabling HDCP, choosing
	   HDCP encryption mode or enabling HDCP repeater mode.

    \param instance   Instance identifier.
    \param pSetupInfo Pointer to the structure containing all setup parameters.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMDL_ERR_DLHDMITX_RESOURCE_NOT_OWNED: the caller does not own
	      the resource
	    - TMDL_ERR_DLHDMITX_INVALID_STATE: the state is invalid for
	      the function

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxInstanceSetup
	    (tmInstance_t instance, tmdlHdmiTxInstanceSetupInfo_t *pSetupInfo);

/*****************************************************************************/
/**
    \brief Get instance setup parameters.

    \param instance   Instance identifier.
    \param pSetupInfo Pointer to the structure that will receive setup
		      parameters.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMDL_ERR_DLHDMITX_RESOURCE_NOT_OWNED: the caller does not own
	      the resource

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxGetInstanceSetup
	    (tmInstance_t instance, tmdlHdmiTxInstanceSetupInfo_t *pSetupInfo);

/*****************************************************************************/
/**
    \brief Make device library handle an incoming interrupt. This function is
	   used by application to tell the device library that the hardware
	   sent an interrupt.

    \param instance   Instance identifier.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_RESOURCE_NOT_OWNED: the caller does not own
	      the resource
	    - TMDL_ERR_DLHDMITX_FULL: the queue is full

 ******************************************************************************/
	tmErrorCode_t tmdlHdmiTxHandleInterrupt(tmInstance_t instance);

/*****************************************************************************/
/**
    \brief Register event callbacks. Only one callback is registered through
	   this API. This callback will received the type of event that
	   occured throug a dedicated parameter and will be called as many
	   times as there is pending events.
	   This function is synchronous.
	   This function is ISR friendly.

    \param instance   Instance identifier.
    \param pCallback  Pointer to the callback function that will handle events
		      from the devlib.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_RESOURCE_NOT_OWNED: the caller does not own
	      the resource
	    - TMDL_ERR_DLHDMITX_INVALID_STATE: the state is invalid for
	      the function

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxRegisterCallbacks
	    (tmInstance_t instance, ptmdlHdmiTxCallback_t pCallback);

/*****************************************************************************/
/**
    \brief This function allows enabling a specific event. By default, all
	   events are disabled, except input lock.

    \param instance Instance identifier.
    \param event    Event to enable.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxEnableEvent(tmInstance_t instance, tmdlHdmiTxEvent_t event);

/*****************************************************************************/
/**
    \brief This function allows disabling a specific event. By default, all
	   events are disabled, except input lock.

    \param instance Instance identifier.
    \param event    Event to disable.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxDisableEvent(tmInstance_t instance, tmdlHdmiTxEvent_t event);

/*****************************************************************************/
/**
    \brief Get specifications of a given video format. Application can use
	   this function to retreives all specifications (frequencies,
	   resolution, etc.) of a given IA/CEA 861-D video format.
	   This function is synchronous.
	   This function is ISR friendly.

    \param instance         Instance identifier.
    \param resolutionID     ID of the resolution to retrieve specs from.
    \param pResolutionSpecs Pointer to the structure receiving specs.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMDL_ERR_DLHDMITX_RESOLUTION_UNKNOWN: the resolution is unknown

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxGetVideoFormatSpecs
	    (tmInstance_t instance,
	     tmdlHdmiTxVidFmt_t resolutionID, tmdlHdmiTxVidFmtSpecs_t *pResolutionSpecs);

/*****************************************************************************/
/**
    \brief Configures all input and output parameters : format, modes, rates,
	   etc. This is the main configuration function of the driver. Here
	   are transmitted all crucial input and output parameters of the
	   device.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance          Instance identifier.
    \param videoInputConfig  Configuration of the input video.
    \param videoOutputConfig Configuration of the output video.
    \param audioInputConfig  Configuration of the input audio.
    \param sinkType          Type of sink connected to the output of the Tx.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
	    - TMBSL_ERR_HDMI_I2C_READ: failed when reading to the I2C bus
	    - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxSetInputOutput
	    (tmInstance_t instance,
	     tmdlHdmiTxVideoInConfig_t videoInputConfig,
	     tmdlHdmiTxVideoOutConfig_t videoOutputConfig,
	     tmdlHdmiTxAudioInConfig_t audioInputConfig, tmdlHdmiTxSinkType_t sinkType);

/*****************************************************************************/
/**
    \brief Configures audio input parameters : format, rate, etc.
	   This function is similar to tmdlHdmiTxSetInputOutput except that
	   video is not reconfigured.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance          Instance identifier.
    \param audioInputConfig  Configuration of the input audio.
    \param sinkType          Type of sink connected to the output of the Tx.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
	    - TMBSL_ERR_HDMI_I2C_READ: failed when reading to the I2C bus
	    - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxSetAudioInput
	    (tmInstance_t instance,
	     tmdlHdmiTxAudioInConfig_t audioInputConfig, tmdlHdmiTxSinkType_t sinkType);

/*****************************************************************************/
/**
    \brief Defines the content of AVI infoframe to be sent by Tx device.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance     Instance identifier.
    \param enable       Enable/disable infoframe insertion.
    \param pAviIfData   Pointer to the structure containing AVI infoframe
			parameters.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
	    - TMBSL_ERR_HDMI_I2C_READ: failed when reading to the I2C bus
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
	    - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxSetVideoInfoframe
	    (tmInstance_t instance, Bool enable, tmdlHdmiTxAviIfData_t *pAviIfData);

/*****************************************************************************/
/**
    \brief Defines the content of AUD infoframe to be sent by Tx device.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance     Instance identifier.
    \param enable       Enable/disable infoframe insertion.
    \param pAudIfData   Pointer to the structure containing AUD infoframe
			parameters.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
	    - TMBSL_ERR_HDMI_I2C_READ: failed when reading to the I2C bus
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
	    - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxSetAudioInfoframe
	    (tmInstance_t instance, Bool enable, tmdlHdmiTxAudIfData_t *pAudIfData);

/*****************************************************************************/
/**
    \brief Defines the content of the audio content protection packet to be
	   sent by Tx device.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance     Instance identifier.
    \param enable       Enable/disable infoframe insertion.
    \param pAcpPktData  Pointer to the structure containing ACP infoframe
			parameters.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
	    - TMBSL_ERR_HDMI_I2C_READ: failed when reading to the I2C bus
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
	    - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxSetACPPacket
	    (tmInstance_t instance, Bool enable, tmdlHdmiTxAcpPktData_t *pAcpPktData);

/*****************************************************************************/
/**
    \brief Defines the content of the General Control packet to be sent by Tx
	   device.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance     Instance identifier.
    \param enable       Enable/disable infoframe insertion.
    \param pGcpPktData  Pointer to the structure containing GCP packet parameters.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
	    - TMBSL_ERR_HDMI_I2C_READ: failed when reading to the I2C bus
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
	    - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxSetGeneralControlPacket
	    (tmInstance_t instance, Bool enable, tmdlHdmiTxGcpPktData_t *pGcpPktData);

/*****************************************************************************/
/**
    \brief Defines the content of ISRC1 packet to be sent by Tx device.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance         Instance identifier.
    \param enable           Enable/disable infoframe insertion.
    \param pIsrc1PktData    Pointer to the structure containing GCP packet parameters.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
	    - TMBSL_ERR_HDMI_I2C_READ: failed when reading to the I2C bus
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
	    - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxSetISRC1Packet
	    (tmInstance_t instance, Bool enable, tmdlHdmiTxIsrc1PktData_t *pIsrc1PktData);

/*****************************************************************************/
/**
    \brief Defines the content of ISRC2 packet to be sent by Tx device.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance         Instance identifier.
    \param enable           Enable/disable infoframe insertion.
    \param pIsrc2PktData    Pointer to the structure containing GCP packet parameters.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
	    - TMBSL_ERR_HDMI_I2C_READ: failed when reading to the I2C bus
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
	    - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxSetISRC2Packet
	    (tmInstance_t instance, Bool enable, tmdlHdmiTxIsrc2PktData_t *pIsrc2PktData);

/*****************************************************************************/
/**
    \brief Defines the content of MPS infoframe to be sent by Tx device.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance     Instance identifier.
    \param enable       Enable/disable infoframe insertion.
    \param pMpsIfData   Pointer to the structure containing MPS infoframe
			parameters.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
	    - TMBSL_ERR_HDMI_I2C_READ: failed when reading to the I2C bus
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
	    - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxSetMPSInfoframe
	    (tmInstance_t instance, Bool enable, tmdlHdmiTxMpsIfData_t *pMpsIfData);

/*****************************************************************************/
/**
    \brief Defines the content of SPD infoframe to be sent by Tx device.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance     Instance identifier.
    \param enable       Enable/disable infoframe insertion.
    \param pSpdIfData   Pointer to the structure containing SPD infoframe
			parameters.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
	    - TMBSL_ERR_HDMI_I2C_READ: failed when reading to the I2C bus
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
	    - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxSetSpdInfoframe
	    (tmInstance_t instance, Bool enable, tmdlHdmiTxSpdIfData_t *pSpdIfData);

/*****************************************************************************/
/**
    \brief Defines the content of VS infoframe to be sent by Tx device.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance     Instance identifier.
    \param enable       Enable/disable infoframe insertion.
    \param pVsIfData    Pointer to the structure containing VS infoframe
			parameters.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
	    - TMBSL_ERR_HDMI_I2C_READ: failed when reading to the I2C bus
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
	    - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxSetVsInfoframe
	    (tmInstance_t instance, Bool enable, tmdlHdmiTxVsPktData_t *pVsIfData);

/*****************************************************************************/
/**
    \brief Enables/disables NULL packet sending (only used for debug purpose).
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance  Instance identifier.
    \param enable    Enable/disable packet insertion.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
	    - TMBSL_ERR_HDMI_I2C_READ: failed when reading to the I2C bus
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
	    - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxDebugSetNullPacket(tmInstance_t instance, Bool enable);

/*****************************************************************************/
/**
    \brief Send one single NULL packet (only used for debug purpose).
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance  Instance identifier.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
	    - TMBSL_ERR_HDMI_I2C_READ: failed when reading to the I2C bus
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
	    - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxDebugSetSingleNullPacket(tmInstance_t instance);

/*****************************************************************************/
/**
    \brief Set the audio output mute status. This function can be used to mute
	   audio output, without muting video. This can be typically used when
	   reconfiguring the audio HW after a sample rate change.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance   Instance identifier.
    \param muteStatus Mute status (True/False).

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
	    - TMBSL_ERR_HDMI_I2C_READ: failed when reading to the I2C bus
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
	    - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxSetAudioMute(tmInstance_t instance, Bool audioMute);

/*****************************************************************************/
/**
    \brief Reset audio CTS.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance   Instance identifier.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
	    - TMBSL_ERR_HDMI_I2C_READ: failed when reading to the I2C bus
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
	    - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode


******************************************************************************/
	tmErrorCode_t tmdlHdmiTxResetAudioCts(tmInstance_t instance);

/*****************************************************************************/
/**
    \brief Retrieve EDID Status from driver.
	   This function is synchronous.
	   This function is ISR friendly.

    \param instance         Instance identifier.
    \param pEdidStatus      Pointer to the array that will receive the EDID Status.
    \param pEdidBlkCount    Pointer to the integer that will receive the number of
			    read EDID block.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxGetEdidStatus
	    (tmInstance_t instance, tmdlHdmiTxEdidStatus_t *pEdidStatus, UInt8 *pEdidBlkCount);

/*****************************************************************************/
/**
    \brief Retrieves audio descriptors from receiver's EDID. This function
	   parses the EDID of Tx device to get the relevant data.
	   This function is synchronous.
	   This function is not ISR friendly.


    \param instance             Instance identifier.
    \param pAudioDescs          Pointer to the array that will receive audio
				descriptors.
    \param maxAudioDescs        Size of the array.
    \param pWrittenAudioDescs   Pointer to the integer that will receive the actual
				number of written descriptors.
    \param pAudioFlags          Pointer to the byte to receive Audio Capabilities Flags.
				This byte is filled as such:
				b7    is the Basic audio bit from CEA Extension Version 3.
				If this bit is set to 1 this means that the sink handles "Basic audio" i.e.
				two channel L-PCM audio at sample rates of 32kHz, 44.1kHz, and 48kHz.
				b6    is the Supports_AI bit from the VSDB
				This bit set to 1 if the sink supports at least one function that uses
				information carried by the ACP, ISRC1, or ISRC2 packets.
				b5-b0    0

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMDL_ERR_DLHDMITX_INVALID_STATE: the state is invalid for
	      the function
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_RESOURCE_NOT_AVAILABLE : EDID not read
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxGetEdidAudioCaps
	    (tmInstance_t instance,
	     tmdlHdmiTxEdidAudioDesc_t *pAudioDescs,
	     UInt maxAudioDescs, UInt *pWrittenAudioDescs, UInt8 *pAudioFlags);

/*****************************************************************************/
/**
    \brief Retrieves supported video formats (short descriptors) from
	   receiver's EDID. This function parses the EDID of Rx device to get
	   the relevant data.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance             Instance identifier.
    \param pVideoDesc           Pointer to the structure that will receive short
				video descriptors.
    \param maxVideoFormats      Size of the array.
    \param pWrittenVideoFormats Pointer to the integer that will receive the actual
				number of written descriptors.
    \param pVideoFlags          Pointer to the byte to receive Video Capability Flags.
				b7: underscan supported
				b6: YCbCr 4:4:4 supported
				b5: YCbCr 4:2:2 supported

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMDL_ERR_DLHDMITX_INVALID_STATE: the state is invalid for
	      the function
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_RESOURCE_NOT_AVAILABLE : EDID not read
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxGetEdidVideoCaps
	    (tmInstance_t instance,
	     tmdlHdmiTxShortVidDesc_t *pVideoDesc,
	     UInt maxVideoFormats, UInt *pWrittenVideoFormats, UInt8 *pVideoFlags);

/*****************************************************************************/
/**
    \brief Retrieves supported video formats (short descriptors) from
	   receiver's EDID. This function parses the EDID of Rx device to get
	   the relevant data.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance                 Instance identifier.
    \param pNativeVideoFormat    Pointer to the array that will receive video
				    timing descriptor.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMDL_ERR_DLHDMITX_INVALID_STATE: the state is invalid for
	      the function
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_RESOURCE_NOT_AVAILABLE : EDID not read
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxGetEdidVideoPreferred
	    (tmInstance_t instance, tmdlHdmiTxEdidVideoTimings_t *pNativeVideoFormat);

/*****************************************************************************/
/**
    \brief Retrieves the sink type from receiver's EDID (HDMI or DVI). This
	   function parses the EDID of Rx device to get the relevant data.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance  Instance identifier.
    \param pSinkType Pointer to the array that will receive sink type.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_BAD_PARAMETER: a parameter is invalid or out
	      of range
	    - TMDL_ERR_DLHDMITX_NO_RESOURCES: the resource is not available
	    - TMDL_ERR_DLHDMITX_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMDL_ERR_DLHDMITX_NOT_INITIALIZED: the transmitter is not initialized
	    - TMDL_ERR_DLHDMITX_INVALID_STATE: the state is invalid for
	      the function

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxGetEdidSinkType
	    (tmInstance_t instance, tmdlHdmiTxSinkType_t *pSinkType);

/*****************************************************************************/
/**
    \brief Retrieves source address from receivers's EDID. This
	   function parses the EDID of Rx device to get the relevant data.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance         Instance identifier.
    \param pSourceAddress   Pointer to the integer that will receive the EDID source
			    address.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMDL_ERR_DLHDMITX_INVALID_STATE: the state is invalid for
	      the function
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_RESOURCE_NOT_AVAILABLE : EDID not read
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxGetEdidSourceAddress
	    (tmInstance_t instance, UInt16 *pSourceAddress);



/*****************************************************************************/
/**
    \brief Retreives KSV list received by Tx device.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance     Instance identifier.
    \param pKsv         Pointer to the array that will receive the KSV list.
    \param maxKsv       Maximum number of KSV that the array can store.
    \param pWrittenKsv  Actual number of KSV written into the array.
    \param pDepth       Connection tree depth.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMDL_ERR_DLHDMITX_INVALID_STATE: the state is invalid for
	      the function
	    - TMDL_ERR_DLHDMITX_NOT_SUPPORTED: device does not support HDCP

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxGetKsvList
	    (tmInstance_t instance,
	     UInt8 *pKsv,
	     UInt8 maxKsv,
	     UInt8 *pWrittenKsv, UInt8 *pDepth, Bool *pMaxCascExd, Bool *pMaxDevsExd);
#ifdef HDMI_TX_REPEATER_ISR_MODE
/******************************************************************************
    \brief Retreives HDCP depth received by Tx device.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance     Instance identifier.
    \param pDepth       Connection tree depth returned with KSV list.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMDL_ERR_DLHDMITX_INVALID_STATE: the state is invalid for
	      the function
	    - TMDL_ERR_DLHDMITX_NOT_SUPPORTED: device does not support HDCP

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxGetDepth(tmInstance_t instance, UInt8 *pDepth);

/******************************************************************************
    \brief Generate SHA_1 interrupt if not occured.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance     Instance identifier.


    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMDL_ERR_DLHDMITX_INVALID_STATE: the state is invalid for
	      the function
	    - TMDL_ERR_DLHDMITX_NOT_SUPPORTED: device does not support HDCP

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxGeneSHA_1_IT(tmInstance_t instance);
#endif				/* HDMI_TX_REPEATER_ISR_MODE */
/*****************************************************************************/
/**
    \brief Enable/Disable HDCP encryption.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance   Instance identifier.
    \param hdcpEnable HDCP On/Off (true = On, False = Off).

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INVALID_STATE: the state is invalid for
	      the function
	    - TMDL_ERR_DLHDMITX_RESOLUTION_UNKNOWN: the resolution is unknown
	    - TMDL_ERR_DLHDMITX_NOT_SUPPORTED: device does not support HDCP
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
	    - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_NOT_SUPPORTED: device does not support HDCP

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxSetHdcp(tmInstance_t instance, Bool hdcpEnable);

/*****************************************************************************/
/**
    \brief Get the driver HDCP state.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance          Instance identifier.
    \param pHdcpCheckState   Pointer to the integer that will receive the HDCP check state.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMDL_ERR_DLHDMITX_NOT_SUPPORTED: device does not support HDCP

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxGetHdcpState
	    (tmInstance_t instance, tmdlHdmiTxHdcpCheck_t *pHdcpCheckState);

/*****************************************************************************/
/**
    \brief Check the result of an HDCP encryption attempt, called at
	   intervals (set by timeSinceLastCall) after tmdlHdmiTxSetHdcp(true).
	   This API must be used only in case of No Operating System. if OS,
	   this is manage internally of this device library.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance             Instance identifier.
    \param timeSinceLastCall    Time passed in milliseconds since last call,
				must be shorter than 600 ms.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_NOT_SUPPORTED: device does not support HDCP
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
	    - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing the I2C bus
	    - TMBSL_ERR_HDMI_NOT_SUPPORTED: device does not support HDCP

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxHdcpCheck(tmInstance_t instance, UInt16 timeSinceLastCall);

/*****************************************************************************/
/**
    \brief This function loads a gamut metadata packet into the HW. HW will
	   actually send it at the beginning of next VS, during the vertical
	   blanking.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance     Instance identifier.
    \param enable       Enable/disable gamut metadata packet insertion.
    \param pGamutData   Pointer to the structure containing gamut metadata
			parameters.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INVALID_STATE: the state is invalid for
	      the function
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
	    - TMBSL_ERR_HDMI_I2C_READ: failed when reading to the I2C bus
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
	    - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxSetGamutPacket
	    (tmInstance_t instance, Bool enable, tmdlHdmiTxGamutData_t *pGamutData);

/*****************************************************************************/
/**
    \brief This function set the extended colorimetry with one of the following
	   extended colorimetries(bits EC2-0): xvYCC601, xvYCC709, sYCC601,
	   AdobeYCC601, AdobeRGB. When the parameter extendedColorimetry is
	   xvYCC601 or xvYCC70, this function calls the API tmdlHdmiTxSetGamutPacket
	   to send Gamut Packet Data that does not exist for all other types of
	   extended colorimetries for which pointer pGamutData can be set to NULL.
	   This function also allows to set YCC Quantization Range (YQ1-0)

	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance              Instance identifier.
    \param enable                Enable/Disable extended colorimetry.
    \param extendedColorimetry   value of the extended colorimetry (bits EC2 EC1 EC0).
    \param yccQR                 YCC quantisation range
    \param pGamutData            Pointer to the structure containing gamut metadata
				 parameters.

    \return The call result:
	   - TM_OK: the call was successful
	   - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong
	   - TMDL_ERR_DLHDMITX_BAD_PARAMETER: a parameter was out of range
	   - TMDL_ERR_DLHDMITX_INVALID_STATE: the state is invalid for the function
	   - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	   - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	   - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
	   - TMBSL_ERR_HDMI_I2C_READ: failed when reading to the I2C bus
	   - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
	   - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxSetExtendedColorimetry
	    (tmInstance_t instance,
	     Bool enable,
	     tmdlHdmiTxExtColorimetry_t extendedColorimetry,
	     tmdlHdmiTxYCCQR_t yccQR, tmdlHdmiTxGamutData_t *pGamutData);

/*****************************************************************************/
/**
    \brief Retrieves supported detailled video descriptors from
	   receiver's EDID. This function parses the EDID of Rx device to get
	   the relevant data.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance         Instance identifier.
    \param pDTDescriptors   Pointer to the array that will receive detailled
			    timing descriptors.
    \param maxDTDesc        Size of the array.
    \param pWrittenDesc     Pointer to the integer that will receive the actual
			    number of written descriptors.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMDL_ERR_DLHDMITX_INVALID_STATE: the state is invalid for
	      the function
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_RESOURCE_NOT_AVAILABLE : EDID not read
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxGetEdidDetailledTimingDescriptors
	    (tmInstance_t instance,
	     tmdlHdmiTxEdidVideoTimings_t *pDTDescriptors,
	     UInt8 maxDTDesc, UInt8 *pWrittenDTDesc);

/*****************************************************************************/
/**
    \brief Retrieves supported monitor descriptor from receiver's EDID.
	   This function parses the EDID of Rx device to get
	   the relevant data.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance         Instance identifier.
    \param pEdidFirstMD     Pointer to the array that will receive the first monitor
			    descriptors.
    \param pEdidSecondMD    Pointer to the array that will receive the second monitor
			    descriptors.
    \param pEdidOtherMD     Pointer to the array that will receive the other monitor
			    descriptors.
    \param maxOtherMD       Size of the array.
    \param pWrittenOtherMD  Pointer to the integer that will receive the actual
			    number of written descriptors.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMDL_ERR_DLHDMITX_INVALID_STATE: the state is invalid for
	      the function
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_RESOURCE_NOT_AVAILABLE : EDID not read
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxGetEdidMonitorDescriptors
	    (tmInstance_t instance,
	     tmdlHdmiTxEdidFirstMD_t *pEdidFirstMD,
	     tmdlHdmiTxEdidSecondMD_t *pEdidSecondMD,
	     tmdlHdmiTxEdidOtherMD_t *pEdidOtherMD, UInt8 maxOtherMD, UInt8 *pWrittenOtherMD);

/*****************************************************************************/
/**
    \brief Retrieves TV picture ratio from receiver's EDID.
	   This function parses the EDID of Rx device to get
	   the relevant data.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance             Instance identifier.
    \param pEdidTvPictureRatio  Pointer to the variable that will receive TV picture ratio
				value.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMDL_ERR_DLHDMITX_INVALID_STATE: the state is invalid for
	      the function
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_RESOURCE_NOT_AVAILABLE : EDID not read
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxGetEdidTVPictureRatio
	    (tmInstance_t instance, tmdlHdmiTxPictAspectRatio_t *pEdidTvPictureRatio);

/******************************************************************************
    \brief This function set the revocation list use for HDCP
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance     Instance identifier.
    \param listPtr      Pointer on revocation list provide by application.
    \param length       length of revocation list.

    \return The call result:
	    - TM_OK: the call was successful, however RX keys have
		     not been checked with provided revocation list
		     because they are not available.
	    - TMDL_DLHDMITX_HDCP_SECURE: the call was successful, RX keys are secure
	    - TMDL_DLHDMITX_HDCP_NOT_SECURE: the call was successful, RX keys are NOT secure
	    - TMDL_ERR_DLHDMITX_INVALID_STATE: we are a repeater
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent


******************************************************************************/

	tmErrorCode_t tmdlHdmiTxSetHDCPRevocationList(tmInstance_t instance,
						      void *listPtr, UInt32 length);


/*****************************************************************************/
/**
    \brief Retreives current HDCP link status. This function is typically used
	   when an "HDCP INACTIVE" event is received to know why HDCP
	   is INACTIVE.

    \param instance    Instance identifier.
    \param pHdcpStatus Pointer to the enum describing the status.
    \param pRawStatus  Pointer to the byte with the raw error code from HW.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong
	    - TMDL_ERR_DLHDMITX_BAD_PARAMETER: a parameter is invalid or out
	      of range
	    - TMDL_ERR_DLHDMITX_NOT_INITIALIZED: the transmitter is not initialized

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxGetHdcpFailStatus
	    (tmInstance_t instance, tmdlHdmiTxHdcpStatus_t *pHdcpStatus, UInt8 *pRawStatus);


/*****************************************************************************/
/**
    \brief Retrieves latency information from receiver's EDID.
	   This function is synchronous.
	   This function is not ISR friendly.


    \param instance             Instance identifier.
    \param pLatency             Pointer to the data structure that receive latency
				information.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMDL_ERR_DLHDMITX_INVALID_STATE: the state is invalid for
	      the function
	    - TMBSL_ERR_HDMI_RESOURCE_NOT_AVAILABLE : EDID not read
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxGetEdidLatencyInfo
	    (tmInstance_t instance, tmdlHdmiTxEdidLatency_t *pLatency);


/******************************************************************************
    \brief Retrieves additional data from receiver's EDID VSDB. This function
	   parses the EDID of Rx device to get the relevant data.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance             Instance identifier.
    \param pExtraVsdbData       Pointer to the structure of additional VSDB data

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMDL_ERR_DLHDMITX_INVALID_STATE: the state is invalid for
	      the function
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_RESOURCE_NOT_AVAILABLE : EDID not read
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxGetEdidExtraVsdbData
	    (tmInstance_t instance, tmdlHdmiTxEdidExtraVsdbData_t **pExtraVsdbData);


/******************************************************************************
    \brief This function set the B... screen
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance     Instance identifier.

    \return The call result:
	    - TM_OK: the call was successful, however RX keys have
		     not been checked with provided revocation list
		     because they are not available.
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range


******************************************************************************/

	tmErrorCode_t tmdlHdmiTxSetBScreen(tmInstance_t instance, tmdlHdmiTxTestPattern_t pattern);

/******************************************************************************
    \brief This function set the Remove B.... screen
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance     Instance identifier.

    \return The call result:
	    - TM_OK: the call was successful, however RX keys have
		     not been checked with provided revocation list
		     because they are not available.
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range


******************************************************************************/

	tmErrorCode_t tmdlHdmiTxRemoveBScreen(tmInstance_t instance);

/******************************************************************************
    \brief This function Convert DTD to CEA
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance         Instance identifier.
    \param pDTDescriptors   Pointer on one DTD
    \return The call result:
	    - CEA code


******************************************************************************/
	tmdlHdmiTxVidFmt_t tmdlHdmiTxConvertDTDtoCEA
	    (tmInstance_t instance, tmdlHdmiTxEdidVideoTimings_t *pDTDescriptors);


/*****************************************************************************/
/**
    \brief Retrieve HPD status from driver.
	   This function is synchronous.
	   This function is ISR friendly.

    \param instance         Instance identifier.
    \param pHPDStatus       Pointer to the variable that will receive the HPD Status.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxGetHPDStatus
	    (tmInstance_t instance, tmdlHdmiTxHotPlug_t *pHPDStatus);


/*****************************************************************************/
/**
    \brief Retrieve RXSense status from driver.
	   This function is synchronous.
	   This function is ISR friendly.

    \param instance         Instance identifier.
    \param pRXSenseStatus   Pointer to the variable that will receive the RXSense Status.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxGetRXSenseStatus
	    (tmInstance_t instance, tmdlHdmiTxRxSense_t *pRXSenseStatus);


/******************************************************************************
    \brief Mute or unmute the TMDS outputs.

    \param instance         Instance identifier.
    \param muteTmdsOut      Mute or unmute indication.

    \return NA.

******************************************************************************/
	tmErrorCode_t tmdlHdmiTxTmdsSetOutputsMute(tmInstance_t instance, Bool muteTmdsOut);

#ifdef __cplusplus
}
#endif
#endif				/* TMDLHDMITX_FUNCTIONS_H */
/*============================================================================*//*                            END OF FILE                                     *//*============================================================================*/
