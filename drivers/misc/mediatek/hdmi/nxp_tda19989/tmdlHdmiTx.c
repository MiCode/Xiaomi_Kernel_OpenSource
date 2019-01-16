/**
 * Copyright (C) 2006 NXP N.V., All Rights Reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of NXP N.V. and is confidential in nature. Under no circumstances
 * is this software to be  exposed to or placed under an Open Source License of
 * any type without the expressed written permission of NXP N.V.
 *
 * \file          tmdlHdmiTx.c
 *
 * \version       Revision: 1
 *
 * \date          Date: 10/08/07 10:00
 *
 * \brief         devlib driver component API for the TDA998x HDMI Transmitters
 *
 * \section refs  Reference Documents
 * HDMI Tx Driver - FRS.doc,
 *
 * \section info  Change Information
 *
 * \verbatim

   History:       tmdlHdmiTx.c
 *
 * *****************  Version 1  *****************
 * User: J. Lamotte Date: 10/08/07   Time: 10:00
 * Updated in $/Source/tmdlHdmiTx/inc
 * initial version

   \endverbatim
 *
*/

/*============================================================================*/
/*                             INCLUDE FILES                                  */
/*============================================================================*/
#ifndef TMFL_TDA19989
#define TMFL_TDA19989
#endif

#ifndef TMFL_NO_RTOS
#define TMFL_NO_RTOS
#endif

#ifndef TMFL_LINUX_OS_KERNEL_DRIVER
#define TMFL_LINUX_OS_KERNEL_DRIVER
#endif


#include "tmdlHdmiTx_IW.h"
#include "tmdlHdmiTx.h"
#include "tmdlHdmiTx_local.h"
#include "tmdlHdmiTx_cfg.h"
#include "tmbslHdmiTx_funcMapping.h"

/*============================================================================*/
/*                          TYPES DECLARATIONS                                */
/*============================================================================*/

/* Macro to avoid compilation warnings */
#ifdef TMFL_OS_WINDOWS
#define DUMMY_ACCESS(x) x
#else
#define DUMMY_ACCESS(x)
#endif

/*============================================================================*/
/*                       CONSTANTS DECLARATIONS                               */
/*============================================================================*/



/*============================================================================*/
/*                         FUNCTION PROTOTYPES                                */
/*============================================================================*/

/* Prototypes of internal functions */
/* Task functions */
#ifndef TMFL_NO_RTOS
static void CommandTaskUnit0(void);
static void HdcpTaskUnit0(void);
#endif				/* TMFL_NO_RTOS */

/* Interrupt callback functions */
static void dlHdmiTxHandleENCRYPT(tmInstance_t instance);
static void dlHdmiTxHandleHPD(tmInstance_t instance);
static void dlHdmiTxHandleT0(tmInstance_t instance);
static void dlHdmiTxHandleBCAPS(tmInstance_t instance);
static void dlHdmiTxHandleBSTATUS(tmInstance_t instance);
static void dlHdmiTxHandleSHA_1(tmInstance_t instance);
static void dlHdmiTxHandlePJ(tmInstance_t instance);
static void dlHdmiTxHandleR0(tmInstance_t instance);
static void dlHdmiTxHandleSW_INT(tmInstance_t instance);
static void dlHdmiTxHandleRX_SENSE(tmInstance_t instance);
static void dlHdmiTxHandleEDID_READ(tmInstance_t instance);
static void dlHdmiTxHandleVS_RPT(tmInstance_t instance);

/* Devlib internal color bar management functions */
#ifndef NO_HDCP
static void dlHdmiTxCheckColorBar(tmInstance_t instance);
static void dlHdmiTxCheckHdcpColorBar(tmInstance_t instance);
#endif

#ifndef NO_HDCP
static void dlHdmiTxFindHdcpSeed(tmInstance_t instance);
#endif				/* NO_HDCP */

/* Set the state machine of device library */
static void dlHdmiTxSetState(tmInstance_t instance, tmdlHdmiTxDriverState_t state);

/* Get the event status (enable or disable) in order to known
   if event should be signaled */
static tmdlHdmiTxEventStatus_t dlHdmiTxGetEventStatus
    (tmInstance_t instance, tmdlHdmiTxEvent_t event);

/* Use by tmdlHdmiTxSetInputOutput in scaler mode */
static Bool dlHdmiTxGetReflineRefpix
    (tmdlHdmiTxVidFmt_t vinFmt,
     tmdlHdmiTxVinMode_t vinMode,
     tmdlHdmiTxVidFmt_t voutFmt,
     UInt8 syncIn,
     tmdlHdmiTxPixRate_t pixRate,
     UInt16 *pRefPix,
     UInt16 *pRefLine, UInt16 *pScRefPix, UInt16 *pScRefLine, Bool *pbVerified);

/* Use by tmdlHdmiTxSetInputOutput to set AVI infoframe */
static tmErrorCode_t dlHdmiTxSetVideoInfoframe
    (tmInstance_t instance, tmdlHdmiTxVidFmt_t voutFmt, tmdlHdmiTxVoutMode_t voutMode);

/* Use to set AVI infoframe with raw data */
static tmErrorCode_t dlHdmiTxSetRawVideoInfoframe
    (tmInstance_t instance, tmdlHdmiTxAviIfData_t *pContentVif, Bool enable);

/* Calculate Checksum for info frame */
static UInt8 dlHdmiTxcalculateCheksumIF(tmbslHdmiTxPktRawAvi_t *pData	/* Pointer to checksum data */
    );

/* IMPORTANT: The 3 functions define below should not be declared in static
   in order to allow applicative API to call them. Those functions are not
   in tmdlHdmiTx_Functions.h but are in tmdlHdmiTxCore.def */

/* Get the device library state */
tmdlHdmiTxDriverState_t dlHdmiTxGetState(tmInstance_t instance);

/* Set pattern ON (Blue screen or color bar) */
tmErrorCode_t dlHdmiTxSetTestPatternOn
    (tmInstance_t instance,
     tmdlHdmiTxVidFmt_t voutFmt, tmdlHdmiTxVoutMode_t voutMode, tmdlHdmiTxTestPattern_t pattern);

/* Set pattern OFF */
tmErrorCode_t dlHdmiTxSetTestPatternOff
    (tmInstance_t instance, tmdlHdmiTxVidFmt_t voutFmt, tmdlHdmiTxVoutMode_t voutMode);

/* Get DTD from BSL */
static tmErrorCode_t dlHdmiTxEdidGetDTD
    (tmInstance_t instance,
     tmdlHdmiTxEdidVideoTimings_t *pDTDescriptors, UInt8 maxDTDesc, UInt8 *pWrittenDTDesc);

static tmdlHdmiTxVidFmt_t dlHdmiTxConvertDTDtoCEA_640HAP
    (tmdlHdmiTxEdidVideoTimings_t *pDTDescriptors);

static tmdlHdmiTxVidFmt_t dlHdmiTxConvertDTDtoCEA_720HAP
    (tmdlHdmiTxEdidVideoTimings_t *pDTDescriptors, tmdlHdmiTxPictAspectRatio_t pictureAspectRatio);

static tmdlHdmiTxVidFmt_t dlHdmiTxConvertDTDtoCEA_1280HAP
    (tmdlHdmiTxEdidVideoTimings_t *pDTDescriptors);

static tmdlHdmiTxVidFmt_t dlHdmiTxConvertDTDtoCEA_1920HAP
    (tmdlHdmiTxEdidVideoTimings_t *pDTDescriptors, Bool formatInterlaced);

static tmdlHdmiTxVidFmt_t dlHdmiTxConvertDTDtoCEA_1440HAP
    (tmdlHdmiTxEdidVideoTimings_t *pDTDescriptors,
     tmdlHdmiTxPictAspectRatio_t pictureAspectRatio, Bool formatInterlaced);

static tmdlHdmiTxVidFmt_t dlHdmiTxConvertDTDtoCEA_2880HAP
    (tmdlHdmiTxEdidVideoTimings_t *pDTDescriptors,
     tmdlHdmiTxPictAspectRatio_t pictureAspectRatio, Bool formatInterlaced);

static tmdlHdmiTxPictAspectRatio_t dlHdmiTxCalcAspectRatio(UInt16 HImageSize, UInt16 VImageSize);

#ifndef NO_HDCP
static void dlHdmiTxCheckHdcpBksv
    (tmInstance_t instance, UInt8 *pHdcpBksvTested, Bool *pbBksvSecure, Bool bBigEndian);
#endif

/* Calculate table index according to video format value */
static tmdlHdmiTxVidFmt_t dlHdmiTxCalcVidFmtIndex(tmdlHdmiTxVidFmt_t vidFmt);

extern tmErrorCode_t tmbslDebugWriteFakeRegPage(tmUnitSelect_t txUnit);

/*============================================================================*/
/*                       VARIABLES DECLARATIONS                               */
/*============================================================================*/

tmdlHdmiTxIWSemHandle_t dlHdmiTxItSemaphore[MAX_UNITS];

/* Unit configuration structure (device library system configuration) */
unitConfig_t unitTableTx[MAX_UNITS] = {
	{
	 False,
	 False,
	 (tmdlHdmiTxHdcpOptions_t) HDCP_OPT_DEFAULT,
	 False,
	 False,
	 TMDL_HDMITX_DEVICE_UNKNOWN,
	 0,
	 0,
	 (tmdlHdmiTxIWTaskHandle_t) 0,
	 (tmdlHdmiTxIWQueueHandle_t) 0,
	 (tmdlHdmiTxIWTaskHandle_t) 0,
	 STATE_NOT_INITIALIZED,
	 (ptmdlHdmiTxCallback_t) 0,
	 {Null, 0,},
	 }
};

#ifndef TMFL_NO_RTOS

tmdlHdmiTxIWFuncPtr_t commandTaskTableTx[MAX_UNITS] = {
	CommandTaskUnit0
};

tmdlHdmiTxIWFuncPtr_t hdcpTaskTableTx[MAX_UNITS] = {
	HdcpTaskUnit0
};

#endif				/* TMFL_NO_RTOS */

tmbslHdmiTxCallbackList_t callbackFuncTableTx;

/* Device library configuration structure completed by dlHdmiTxGetConfig with
   informations contained in config file */
tmdlHdmiTxDriverConfigTable_t gtmdlHdmiTxDriverConfigTable[MAX_UNITS] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, TMDL_HDMITX_PATTERN_OFF, 0},
};

/* Video info (see instanceStatusInfoTx) */
tmdlHdmiTxVideoInfo_t videoInfoListTx = {
	False,
	{TMDL_HDMITX_VFMT_03_720x480p_60Hz, TMDL_HDMITX_VINMODE_YUV422, TMDL_HDMITX_SYNCSRC_EXT_VS,
	 TMDL_HDMITX_PIXRATE_SINGLE, TMDL_HDMITX_3D_NONE},
	{TMDL_HDMITX_VFMT_03_720x480p_60Hz, TMDL_HDMITX_VOUTMODE_YUV422, TMDL_HDMITX_COLORDEPTH_24,
	 TMDL_HDMITX_VQR_DEFAULT}
};

/* Audio info (see instanceStatusInfoTx) */
tmdlHdmiTxAudioInfo_t audioInfoListTx = {
	False,
	{TMDL_HDMITX_AFMT_SPDIF, TMDL_HDMITX_AFS_48K, TMDL_HDMITX_I2SFOR_PHILIPS_L,
	 TMDL_HDMITX_I2SQ_16BITS, TMDL_HDMITX_DSTRATE_SINGLE, 0x00}
};

/* Event state (see instanceStatusInfoTx) */
tmdlHdmiTxEventState_t eventStateListTx[EVENT_NB] = {
	{TMDL_HDMITX_HDCP_ACTIVE, TMDL_HDMITX_EVENT_DISABLED},
	{TMDL_HDMITX_HDCP_INACTIVE, TMDL_HDMITX_EVENT_DISABLED},
	{TMDL_HDMITX_HPD_ACTIVE, TMDL_HDMITX_EVENT_DISABLED},
	{TMDL_HDMITX_HPD_INACTIVE, TMDL_HDMITX_EVENT_DISABLED},
	{TMDL_HDMITX_RX_KEYS_RECEIVED, TMDL_HDMITX_EVENT_DISABLED},
	{TMDL_HDMITX_RX_DEVICE_ACTIVE, TMDL_HDMITX_EVENT_DISABLED},
	{TMDL_HDMITX_RX_DEVICE_INACTIVE, TMDL_HDMITX_EVENT_DISABLED},
	{TMDL_HDMITX_EDID_RECEIVED, TMDL_HDMITX_EVENT_DISABLED},
	{TMDL_HDMITX_VS_RPT_RECEIVED, TMDL_HDMITX_EVENT_DISABLED}
#ifdef HDMI_TX_REPEATER_ISR_MODE
	, {TMDL_HDMITX_B_STATUS, TMDL_HDMITX_EVENT_DISABLED}
#endif				/* HDMI_TX_REPEATER_ISR_MODE */
};

/* Color bars state (see instanceStatusInfoTx) */
tmdlHdmiTxColBarState_t colorBarStateTx = {
	False,
	True,
	True,
	False,
	False,
	True,
	False
};

tmdlHdmiTxGamutState_t gamutStateTx = {
	False,
	0,
	TMDL_HDMITX_EXT_COLORIMETRY_XVYCC601,
	False,
	TMDL_HDMITX_YQR_LIMITED
};


/* Instance status (save the actual configuration) */
instanceStatus_t instanceStatusInfoTx[MAX_UNITS] = {
	{(ptmdlHdmiTxVideoInfo_t) &videoInfoListTx,
	 (ptmdlHdmiTxAudioInfo_t) &audioInfoListTx,
	 (ptmdlHdmiTxEventState_t) eventStateListTx,
	 (ptmdlHdmiTxColBarState_t) &colorBarStateTx,
	 (ptmdlHdmiTxGamutState_t) &gamutStateTx}
};

/* HDCP seed table, arranged as pairs of 16-bit integers: lookup value, seed value.
 * If no table is programmed and if KEY_SEED in config file is null, HDCP will be disabled */
#define SEED_TABLE_LEN 10
static const UInt16 kSeedTable[SEED_TABLE_LEN][2] = {
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0}
};

#ifndef NO_HDCP
tmdlHdmiTxHdcpInfo_t hdcpInfoListTx[MAX_UNITS];
#endif				/* NO_HDCP */


static Bool gI2CDebugAccessesEnabled = True;	/* For debug purpose only, used to manage underlying I2C accessed */

#ifdef HDMI_TX_REPEATER_ISR_MODE
static Bool gIgnoreNextSha1 = False;
#endif				/*HDMI_TX_REPEATER_ISR_MODE */

/*============================================================================*/
/*                              FUNCTIONS                                     */
/*============================================================================*/

/******************************************************************************
    \brief Get the software version of the driver.

    \param pSWVersion Pointer to the version structure.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent

******************************************************************************/
tmErrorCode_t tmdlHdmiTxGetSWVersion(tmSWVersion_t *pSWVersion) {
	/* Check if SWVersion pointer is Null */
	RETIF(pSWVersion == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

	    /* Copy SW version */
	    pSWVersion->compatibilityNr = VERSION_COMPATIBILITY;
	pSWVersion->majorVersionNr = VERSION_MAJOR;
	pSWVersion->minorVersionNr = VERSION_MINOR;

	return TM_OK;
}

/******************************************************************************
    \brief Get the number of available HDMI transmitters devices in the system.
	   A unit directly represents a physical device.

    \param pUnitCount Pointer to the number of available units.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent

******************************************************************************/
tmErrorCode_t tmdlHdmiTxGetNumberOfUnits(UInt32 *pUnitCount) {
	/* Check if UnitCount pointer is Null */
	RETIF(pUnitCount == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

	    /* Copy the maximum number of units */
	    * pUnitCount = MAX_UNITS;

	return TM_OK;
}

/******************************************************************************
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
tmErrorCode_t tmdlHdmiTxGetCapabilities(tmdlHdmiTxCapabilities_t *pCapabilities) {
	/* Directly call GetCapabilitiesM function for unit 0 and return the result */
	return (tmdlHdmiTxGetCapabilitiesM((tmUnitSelect_t) 0, pCapabilities));
}

/******************************************************************************
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
    (tmUnitSelect_t unit, tmdlHdmiTxCapabilities_t *pCapabilities) {
	tmErrorCode_t errCode = TM_OK;
	Bool featureSupported;

	/* Check if unit number is in range */
	RETIF((unit < 0) || (unit >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_UNIT_NUMBER)

	    /* Check if Capalities pointer is Null */
	    RETIF(pCapabilities == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

	    /* Device version */
	    pCapabilities->deviceVersion = unitTableTx[unit].deviceVersion;

	/* Retrieve the capabilities from the BSL layer */

	/* HDCP support */
	RETIF((errCode = tmbslHdmiTxHwGetCapabilities(unit,
						      HDMITX_FEATURE_HW_HDCP,
						      &featureSupported)) != TM_OK, errCode)

	    pCapabilities->hdcp = featureSupported;

	/* Scaler support */
	RETIF((errCode = tmbslHdmiTxHwGetCapabilities(unit,
						      HDMITX_FEATURE_HW_SCALER,
						      &featureSupported)) != TM_OK, errCode)

	    pCapabilities->scaler = featureSupported;

	/* Audio HBR support */
	RETIF((errCode = tmbslHdmiTxHwGetCapabilities(unit,
						      HDMITX_FEATURE_HW_AUDIO_HBR,
						      &featureSupported)) != TM_OK, errCode)

	    pCapabilities->audioPacket.HBR = featureSupported;

	/* Audio OBA support */
	RETIF((errCode = tmbslHdmiTxHwGetCapabilities(unit,
						      HDMITX_FEATURE_HW_AUDIO_OBA,
						      &featureSupported)) != TM_OK, errCode)

	    pCapabilities->audioPacket.oneBitAudio = featureSupported;

	/* Audio DST support */
	RETIF((errCode = tmbslHdmiTxHwGetCapabilities(unit,
						      HDMITX_FEATURE_HW_AUDIO_DST,
						      &featureSupported)) != TM_OK, errCode)

	    pCapabilities->audioPacket.DST = featureSupported;

	/* HDMI version 1.1 support */
	RETIF((errCode = tmbslHdmiTxHwGetCapabilities(unit,
						      HDMITX_FEATURE_HW_HDMI_1_1,
						      &featureSupported)) != TM_OK, errCode)

	    if (featureSupported) {
		pCapabilities->hdmiVersion = TMDL_HDMITX_HDMI_VERSION_1_1;
	}

	/* HDMI version 1.2A support */
	RETIF((errCode = tmbslHdmiTxHwGetCapabilities(unit,
						      HDMITX_FEATURE_HW_HDMI_1_2A,
						      &featureSupported)) != TM_OK, errCode)

	    if (featureSupported) {
		pCapabilities->hdmiVersion = TMDL_HDMITX_HDMI_VERSION_1_2a;
	}

	/* HDMI version 1.3 support */
	RETIF((errCode = tmbslHdmiTxHwGetCapabilities(unit,
						      HDMITX_FEATURE_HW_HDMI_1_3A,
						      &featureSupported)) != TM_OK, errCode)

	    if (featureSupported) {
		pCapabilities->hdmiVersion = TMDL_HDMITX_HDMI_VERSION_1_3a;
	}

	/* Deep Color support */
	/* By default */
	pCapabilities->colorDepth = TMDL_HDMITX_COLORDEPTH_24;

	RETIF((errCode = tmbslHdmiTxHwGetCapabilities(unit,
						      HDMITX_FEATURE_HW_DEEP_COLOR_30,
						      &featureSupported)) != TM_OK, errCode)

	    if (featureSupported) {
		pCapabilities->colorDepth = TMDL_HDMITX_COLORDEPTH_30;
	}

	RETIF((errCode = tmbslHdmiTxHwGetCapabilities(unit,
						      HDMITX_FEATURE_HW_DEEP_COLOR_36,
						      &featureSupported)) != TM_OK, errCode)

	    if (featureSupported) {
		pCapabilities->colorDepth = TMDL_HDMITX_COLORDEPTH_36;
	}

	RETIF((errCode = tmbslHdmiTxHwGetCapabilities(unit,
						      HDMITX_FEATURE_HW_DEEP_COLOR_48,
						      &featureSupported)) != TM_OK, errCode)

	    if (featureSupported) {
		pCapabilities->colorDepth = TMDL_HDMITX_COLORDEPTH_48;
	}

	return errCode;
}

/******************************************************************************
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
tmErrorCode_t tmdlHdmiTxOpen(tmInstance_t *pInstance) {
	/* Directly call OpenM function for unit 0 and return the result */
	return (tmdlHdmiTxOpenM(pInstance, (tmUnitSelect_t) 0));
}

/******************************************************************************
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

#include <linux/semaphore.h>
DEFINE_SEMAPHORE(hdmitx_mutex);

tmErrorCode_t tmdlHdmiTxOpenM(tmInstance_t *pInstance, tmUnitSelect_t unit) {
	tmErrorCode_t errCode;
	tmErrorCode_t errCodeSem;
	UInt16 i;
	UInt8 deviceVersion;
	Bool featureSupported;

	/* Check if unit number is in range */
	RETIF((unit < 0) || (unit >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_UNIT_NUMBER)

	    /* Check if Instance pointer is Null */
	    RETIF(pInstance == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

	    /* Create the semaphore to protect variables modified under interruption */
	    /* RETIF( (errCode = tmdlHdmiTxIWSemaphoreCreate(&dlHdmiTxItSemaphore[unit]) ) != TM_OK, errCode) */
	    dlHdmiTxItSemaphore[unit] = (tmdlHdmiTxIWSemHandle_t) (&hdmitx_mutex);

	/* Take the sempahore */
	RETIF((errCodeSem = tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[unit])) != TM_OK, errCodeSem)

	    /* Check if unit is already instanciated */
	    RETIF_SEM(dlHdmiTxItSemaphore[unit],
		      unitTableTx[unit].opened == True, TMDL_ERR_DLHDMITX_RESOURCE_OWNED)

	    /* Check the state */
	    RETIF_SEM(dlHdmiTxItSemaphore[unit],
		      dlHdmiTxGetState(unit) != STATE_NOT_INITIALIZED,
		      TMDL_ERR_DLHDMITX_INVALID_STATE)

	    /* Instanciate unit and return corresponding instance number */
	    /* Since HW unit are only instanciable once, instance = unit */
	    unitTableTx[unit].opened = True;
	unitTableTx[unit].hdcpEnable = False;
	unitTableTx[unit].repeaterEnable = False;
	unitTableTx[unit].deviceVersion = TMDL_HDMITX_DEVICE_UNKNOWN;
	unitTableTx[unit].simplayHd = False;
	unitTableTx[unit].pCallback = Null;
	unitTableTx[unit].revocationList.pList = Null;
	unitTableTx[unit].revocationList.length = 0;

	/* Recover the configuration of the device library */
	RETIF_SEM(dlHdmiTxItSemaphore[unit],
		  dlHdmiTxGetConfig(unit, &gtmdlHdmiTxDriverConfigTable[unit]) != TM_OK,
		  TMDL_ERR_DLHDMITX_INIT_FAILED)
#ifndef TMFL_NO_RTOS
	    /* Create message queue associated to this instance/unit */
	    RETIF_SEM(dlHdmiTxItSemaphore[unit],
		      tmdlHdmiTxIWQueueCreate(gtmdlHdmiTxDriverConfigTable[unit].
					      commandTaskQueueSize,
					      &(unitTableTx[unit].queueHandle)) != TM_OK,
		      TMDL_ERR_DLHDMITX_NO_RESOURCES)

	    /* Create the command task associated to this instance/unit */
	    RETIF_SEM(dlHdmiTxItSemaphore[unit],
		      tmdlHdmiTxIWTaskCreate(commandTaskTableTx[unit],
					     gtmdlHdmiTxDriverConfigTable[unit].commandTaskPriority,
					     gtmdlHdmiTxDriverConfigTable[unit].
					     commandTaskStackSize,
					     &(unitTableTx[unit].commandTaskHandle)) != TM_OK,
		      TMDL_ERR_DLHDMITX_NO_RESOURCES)

	    RETIF_SEM(dlHdmiTxItSemaphore[unit],
		      tmdlHdmiTxIWTaskStart(unitTableTx[unit].commandTaskHandle) != TM_OK,
		      TMDL_ERR_DLHDMITX_NO_RESOURCES)

	    /* Create the hdcp check task associated to this instance/unit */
#ifndef NO_HDCP
	    RETIF_SEM(dlHdmiTxItSemaphore[unit],
		      tmdlHdmiTxIWTaskCreate(hdcpTaskTableTx[unit],
					     gtmdlHdmiTxDriverConfigTable[unit].hdcpTaskPriority,
					     gtmdlHdmiTxDriverConfigTable[unit].hdcpTaskStackSize,
					     &(unitTableTx[unit].hdcpTaskHandle)) != TM_OK,
		      TMDL_ERR_DLHDMITX_NO_RESOURCES)
#endif				/* NO_HDCP */
#endif				/* TMFL_NO_RTOS */
	    * pInstance = (tmInstance_t) unit;

#ifndef NO_HDCP
	hdcpInfoListTx[unit].bKsvSecure = False;
	hdcpInfoListTx[unit].hdcpKsvDevices = 0;
	for (i = 0; i < TMDL_HDMITX_KSV_BYTES_PER_DEVICE; i++) {
		hdcpInfoListTx[unit].hdcpBksv[i] = 0;
	}
	hdcpInfoListTx[unit].hdcpDeviceDepth = 0;
#endif				/* NO_HDCP */

	/* Init the BSL */
	/* Make sure all events are disabled */
	instanceStatusInfoTx[unit].pEventState[TMDL_HDMITX_HDCP_ACTIVE].status =
	    TMDL_HDMITX_EVENT_DISABLED;
	instanceStatusInfoTx[unit].pEventState[TMDL_HDMITX_HDCP_INACTIVE].status =
	    TMDL_HDMITX_EVENT_DISABLED;
	instanceStatusInfoTx[unit].pEventState[TMDL_HDMITX_HPD_ACTIVE].status =
	    TMDL_HDMITX_EVENT_DISABLED;
	instanceStatusInfoTx[unit].pEventState[TMDL_HDMITX_HPD_INACTIVE].status =
	    TMDL_HDMITX_EVENT_DISABLED;
	instanceStatusInfoTx[unit].pEventState[TMDL_HDMITX_RX_KEYS_RECEIVED].status =
	    TMDL_HDMITX_EVENT_DISABLED;
	instanceStatusInfoTx[unit].pEventState[TMDL_HDMITX_RX_DEVICE_ACTIVE].status =
	    TMDL_HDMITX_EVENT_DISABLED;
	instanceStatusInfoTx[unit].pEventState[TMDL_HDMITX_RX_DEVICE_INACTIVE].status =
	    TMDL_HDMITX_EVENT_DISABLED;
	instanceStatusInfoTx[unit].pEventState[TMDL_HDMITX_EDID_RECEIVED].status =
	    TMDL_HDMITX_EVENT_DISABLED;
	instanceStatusInfoTx[unit].pEventState[TMDL_HDMITX_VS_RPT_RECEIVED].status =
	    TMDL_HDMITX_EVENT_DISABLED;
#ifdef HDMI_TX_REPEATER_ISR_MODE
	instanceStatusInfoTx[unit].pEventState[TMDL_HDMITX_B_STATUS].status =
	    TMDL_HDMITX_EVENT_DISABLED;
#endif				/* HDMI_TX_REPEATER_ISR_MODE */
	instanceStatusInfoTx[unit].pColBarState->disableColorBarOnR0 = False;
	instanceStatusInfoTx[unit].pColBarState->hdcpColbarChange = False;
	instanceStatusInfoTx[unit].pColBarState->hdcpEncryptOrT0 = True;
	instanceStatusInfoTx[unit].pColBarState->hdcpSecureOrT0 = False;
	instanceStatusInfoTx[unit].pColBarState->inOutFirstSetDone = False;
	instanceStatusInfoTx[unit].pColBarState->colorBarOn = False;
	instanceStatusInfoTx[unit].pColBarState->changeColorBarNow = False;

	instanceStatusInfoTx[unit].pGamutState->gamutOn = False;
	instanceStatusInfoTx[unit].pGamutState->gamutBufNum = 0;	/* use buffer 0 by default */
	instanceStatusInfoTx[unit].pGamutState->wideGamutColorSpace =
	    TMDL_HDMITX_EXT_COLORIMETRY_XVYCC601;
	instanceStatusInfoTx[unit].pGamutState->extColOn = False;
	instanceStatusInfoTx[unit].pGamutState->yccQR = TMDL_HDMITX_YQR_LIMITED;


	instanceStatusInfoTx[unit].pAudioInfo->audioMuteState = False;	/* Initially audio is not muted */



	/* The funcCallback is not the same between BSL, so fill it dynamically */
	for (i = 0; i < HDMITX_CALLBACK_INT_NUM; i++) {
		callbackFuncTableTx.funcCallback[i] = Null;
	}
	callbackFuncTableTx.funcCallback[HDMITX_CALLBACK_INT_ENCRYPT] = dlHdmiTxHandleENCRYPT;
	callbackFuncTableTx.funcCallback[HDMITX_CALLBACK_INT_HPD] = dlHdmiTxHandleHPD;
	callbackFuncTableTx.funcCallback[HDMITX_CALLBACK_INT_T0] = dlHdmiTxHandleT0;
	callbackFuncTableTx.funcCallback[HDMITX_CALLBACK_INT_BCAPS] = dlHdmiTxHandleBCAPS;
	callbackFuncTableTx.funcCallback[HDMITX_CALLBACK_INT_BSTATUS] = dlHdmiTxHandleBSTATUS;
	callbackFuncTableTx.funcCallback[HDMITX_CALLBACK_INT_SHA_1] = dlHdmiTxHandleSHA_1;
	callbackFuncTableTx.funcCallback[HDMITX_CALLBACK_INT_PJ] = dlHdmiTxHandlePJ;
	callbackFuncTableTx.funcCallback[HDMITX_CALLBACK_INT_R0] = dlHdmiTxHandleR0;
	callbackFuncTableTx.funcCallback[HDMITX_CALLBACK_INT_SW_INT] = dlHdmiTxHandleSW_INT;
	callbackFuncTableTx.funcCallback[HDMITX_CALLBACK_INT_RX_SENSE] = dlHdmiTxHandleRX_SENSE;
	callbackFuncTableTx.funcCallback[HDMITX_CALLBACK_INT_EDID_BLK_READ] =
	    dlHdmiTxHandleEDID_READ;
	callbackFuncTableTx.funcCallback[HDMITX_CALLBACK_INT_VS_RPT] = dlHdmiTxHandleVS_RPT;

	/* Prepare static TDA9984 driver data as the compiler doesn't seem to */

	tmbslHdmiTxHwStartup();
	errCode = tmbslHdmiTxInit(*pInstance, gtmdlHdmiTxDriverConfigTable[unit].i2cAddress, gtmdlHdmiTxDriverConfigTable[unit].i2cWriteFunction, gtmdlHdmiTxDriverConfigTable[unit].i2cReadFunction, (ptmbslHdmiTxSysFuncEdid_t) 0,	/* Not used for TDA9984 */
				  (ptmbslHdmiTxSysFuncTimer_t) tmdlHdmiTxIWWait, &callbackFuncTableTx, False,	/* Alternate EDID address not used */
				  (tmbslHdmiTxVidFmt_t) instanceStatusInfoTx[unit].pVideoInfo->
				  videoInConfig.format,
				  (tmbslHdmiTxPixRate_t) instanceStatusInfoTx[unit].pVideoInfo->
				  videoInConfig.pixelRate);
	if (errCode != TM_OK) {
		/* Init failed */
		tmbslHdmiTxDeinit(unit);

		/* Release the sempahore */
		RETIF((errCodeSem =
		       tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[unit])) != TM_OK, errCodeSem)

		    return errCode;
	} else {
		/* Init passed, continue */

		/* Start by forcing the TMDS ouputs off */
		errCode = tmbslHdmiTxTmdsSetOutputs(unit, HDMITX_TMDSOUT_FORCED0);
		RETIF_SEM(dlHdmiTxItSemaphore[unit], (errCode != TM_OK)
			  && (errCode != TMBSL_ERR_HDMI_NOT_SUPPORTED), errCode)

		    RETIF_SEM(dlHdmiTxItSemaphore[unit],
			      (errCode = tmbslHdmiTxHwGetCapabilities(unit,
								      HDMITX_FEATURE_HW_HDCP,
								      &featureSupported)) != TM_OK,
			      errCode)
#ifndef NO_HDCP
		    if (featureSupported == True) {
			dlHdmiTxFindHdcpSeed(unit);
		}
#endif				/* NO_HDCP */

#ifdef TMFL_HDCP_OPTIMIZED_POWER
		tmbslHdmiTxHdcpPowerDown(unit, True);
#endif
		/* Retrieve the hardware device version from the BSL layer */
		RETIF_SEM(dlHdmiTxItSemaphore[unit],
			  (errCode = tmbslHdmiTxHwGetVersion(unit, &deviceVersion))
			  != TM_OK, errCode);

		/* Store the hardware device version in the global variable */
		switch (deviceVersion) {
		case BSLHDMITX_TDA9984:
			unitTableTx[unit].deviceVersion = TMDL_HDMITX_DEVICE_TDA9984;
			break;

		case BSLHDMITX_TDA9989:
			unitTableTx[unit].deviceVersion = TMDL_HDMITX_DEVICE_TDA9989;
			break;

		case BSLHDMITX_TDA9981:
			unitTableTx[unit].deviceVersion = TMDL_HDMITX_DEVICE_TDA9981;
			break;

		case BSLHDMITX_TDA9983:
			unitTableTx[unit].deviceVersion = TMDL_HDMITX_DEVICE_TDA9983;
			break;

		case BSLHDMITX_TDA19989:
			unitTableTx[unit].deviceVersion = TMDL_HDMITX_DEVICE_TDA19989;
			break;

		case BSLHDMITX_TDA19988:
			unitTableTx[unit].deviceVersion = TMDL_HDMITX_DEVICE_TDA19988;
			break;

		default:
			unitTableTx[unit].deviceVersion = TMDL_HDMITX_DEVICE_UNKNOWN;
			break;
		}
	}


#ifndef TMFL_NO_RTOS
	/* Start HDCP check task */

#ifndef NO_HDCP
	RETIF_SEM(dlHdmiTxItSemaphore[unit],
		  tmdlHdmiTxIWTaskStart(unitTableTx[unit].hdcpTaskHandle) != TM_OK,
		  TMDL_ERR_DLHDMITX_NO_RESOURCES)
#endif				/* NO_HDCP */
#endif				/* TMFL_NO_RTOS */
	    /* Set the state machine to initialized */
	    dlHdmiTxSetState(unit, STATE_INITIALIZED);

	/* Release the sempahore */
	RETIF((errCodeSem = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[unit])) != TM_OK, errCodeSem)

	    return TM_OK;
}

/******************************************************************************
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
tmErrorCode_t tmdlHdmiTxClose(tmInstance_t instance) {
	tmErrorCode_t errCode = TM_OK;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Check if unit corresponding to instance is opened */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      unitTableTx[instance].opened == False, TMDL_ERR_DLHDMITX_RESOURCE_NOT_OWNED)

	    /* Close instance */
	    unitTableTx[instance].opened = False;

	/* Set the state machine */
	dlHdmiTxSetState(instance, STATE_NOT_INITIALIZED);

	/* Destroy resources allocated for this instance/unit */

#ifndef TMFL_NO_RTOS

#ifndef NO_HDCP
	tmdlHdmiTxIWTaskDestroy(unitTableTx[instance].hdcpTaskHandle);
#endif				/* NO_HDCP */

	tmdlHdmiTxIWTaskDestroy(unitTableTx[instance].commandTaskHandle);
	tmdlHdmiTxIWQueueDestroy(unitTableTx[instance].queueHandle);

#endif				/* TMFL_NO_RTOS */

	/* Reset an instance of an HDMI transmitter */
	tmbslHdmiTxDeinit(instance);

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Close the handle to the semaphore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreDestroy(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
	    - TMBSL_ERR_HDMI_I2C_READ: failed when reading to the I2C bus

******************************************************************************/
tmErrorCode_t tmdlHdmiTxSetPowerState(tmInstance_t instance, tmPowerState_t powerState) {
	tmErrorCode_t errCode;
	tmbslHdmiTxHotPlug_t hpdStatus;	/* HPD status */

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    if (unitTableTx[instance].deviceVersion == TMDL_HDMITX_DEVICE_TDA9984) {
		if (powerState == tmPowerSuspend) {
			return TMDL_ERR_DLHDMITX_NOT_SUPPORTED;
		}
	}




	/* Switch off HDCP */
	if (((powerState == tmPowerOff) && (unitTableTx[instance].hdcpEnable == True))
	    || ((powerState == tmPowerStandby) && (unitTableTx[instance].hdcpEnable == True))
	    || ((powerState == tmPowerSuspend) && (unitTableTx[instance].hdcpEnable == True))
	    ) {
		/* Release the sempahore */
		RETIF((errCode =
		       tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)
		    /* Switch off HDCP */
		    RETIF((errCode = tmdlHdmiTxSetHdcp(instance, False)) != TM_OK, errCode)
		    /* Take the sempahore */
		    RETIF((errCode =
			   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)
	}


	/* TDA9989, TDA19989 and TDA19988 only */
	if ((unitTableTx[instance].deviceVersion == TMDL_HDMITX_DEVICE_TDA9989)
	    || (unitTableTx[instance].deviceVersion == TMDL_HDMITX_DEVICE_TDA19989)
	    || (unitTableTx[instance].deviceVersion == TMDL_HDMITX_DEVICE_TDA19988))
	{
		if ((powerState != tmPowerOn) && (powerState != tmPowerSuspend)) {
			dlHdmiTxSetState(instance, STATE_INITIALIZED);
		}

		if ((powerState == tmPowerOn) && (unitTableTx[instance].simplayHd == True)) {

			instanceStatusInfoTx[0].pColBarState->disableColorBarOnR0 = False;
			instanceStatusInfoTx[0].pColBarState->hdcpColbarChange = False;
			instanceStatusInfoTx[0].pColBarState->hdcpEncryptOrT0 = True;
			instanceStatusInfoTx[0].pColBarState->hdcpSecureOrT0 = False;
			instanceStatusInfoTx[0].pColBarState->inOutFirstSetDone = False;
			instanceStatusInfoTx[0].pColBarState->colorBarOn = True;
			instanceStatusInfoTx[0].pColBarState->changeColorBarNow = True;

		}

	}

	/* Set the power state of the transmitter */
	RETIF_SEM(dlHdmiTxItSemaphore[instance],
		  (errCode = tmbslHdmiTxPowerSetState(instance, powerState)) != TM_OK, errCode)

	    /* Get Hot Plug status */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode = tmbslHdmiTxHotPlugGetStatus(instance,
							     &hpdStatus, False)) != TM_OK, errCode)

	    if (powerState == tmPowerOn) {
		if ((hpdStatus == HDMITX_HOTPLUG_ACTIVE)
		    && (dlHdmiTxGetState(instance) != STATE_EDID_AVAILABLE)) {
			/* Yes: Wait for DDC line to settle before reading EDID */
			tmbslHdmiTxSysTimerWait(instance, 500);	/* ms */

			/* Request EDID read */
			RETIF_SEM(dlHdmiTxItSemaphore[instance],
				  (errCode = tmbslHdmiTxEdidRequestBlockData(instance,
									     unitTableTx[instance].
									     pEdidBuffer,
									     (Int) ((unitTableTx
										     [instance].
										     edidBufferSize)
										    >> 7),
									     (Int) (unitTableTx
										    [instance].
										    edidBufferSize)))
				  != TM_OK, errCode)
		}
	}

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
tmErrorCode_t tmdlHdmiTxGetPowerState(tmInstance_t instance, tmPowerState_t *pPowerState) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Check if PowerState pointer is Null */
	    RETIF(pPowerState == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Get the power state of the transmitter */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode = tmbslHdmiTxPowerGetState(instance, pPowerState)) != TM_OK, errCode)

	    /* Release the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
    \brief Set the configuration of instance attributes. This function is
	   required by DVP architecture rules but actually does nothing in this
	   driver.

    \param instance    Instance identifier.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range

******************************************************************************/
tmErrorCode_t tmdlHdmiTxInstanceConfig(tmInstance_t instance) {
	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    return TM_OK;
}

/******************************************************************************
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
    (tmInstance_t instance, tmdlHdmiTxInstanceSetupInfo_t *pSetupInfo) {
	tmErrorCode_t errCode;
#ifndef NO_HDCP
	UInt16 i;
#endif

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Check if SetupInfo pointer is NULL */
	    RETIF(pSetupInfo == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Check if unit corresponding to instance is opened */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      unitTableTx[instance].opened == False, TMDL_ERR_DLHDMITX_RESOURCE_NOT_OWNED)

	    /* Check the state */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      dlHdmiTxGetState(instance) != STATE_INITIALIZED,
		      TMDL_ERR_DLHDMITX_INVALID_STATE)

	    unitTableTx[instance].repeaterEnable = pSetupInfo->repeaterEnable;
	unitTableTx[instance].simplayHd = pSetupInfo->simplayHd;
	unitTableTx[instance].pEdidBuffer = pSetupInfo->pEdidBuffer;
	unitTableTx[instance].edidBufferSize = pSetupInfo->edidBufferSize;

#ifndef NO_HDCP
	/* Reset HDCP DevLib data */
	hdcpInfoListTx[instance].hdcpCheckState = TMDL_HDMITX_HDCP_CHECK_NOT_STARTED;
	hdcpInfoListTx[instance].hdcpErrorState = 0;
	hdcpInfoListTx[instance].hdcpKsvDevices = 0;
	hdcpInfoListTx[instance].bKsvSecure = False;
	for (i = 0; i < TMDL_HDMITX_KSV_BYTES_PER_DEVICE; i++) {
		hdcpInfoListTx[instance].hdcpBksv[i] = 0;
	}
	hdcpInfoListTx[instance].hdcpDeviceDepth = 0;

	hdcpInfoListTx[instance].hdcpMaxCascExceeded = False;
	hdcpInfoListTx[instance].hdcpMaxDevsExceeded = False;
#endif				/* NO_HDCP */

	/* Set state machine to Unplugged */
	dlHdmiTxSetState(instance, STATE_UNPLUGGED);

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
    (tmInstance_t instance, tmdlHdmiTxInstanceSetupInfo_t *pSetupInfo) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Check if SetupInfo pointer is NULL */
	    RETIF(pSetupInfo == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Check if unit corresponding to instance is opened */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      unitTableTx[instance].opened == False, TMDL_ERR_DLHDMITX_RESOURCE_NOT_OWNED)

	    pSetupInfo->simplayHd = unitTableTx[instance].simplayHd;
	pSetupInfo->repeaterEnable = unitTableTx[instance].repeaterEnable;
	/* JL, TODO */

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
tmErrorCode_t tmdlHdmiTxHandleInterrupt(tmInstance_t instance) {
#ifndef TMFL_NO_RTOS
	tmErrorCode_t errCode;
	UInt8 message = 0;
#else
	tmErrorCode_t err = TM_OK;
#endif				/* TMFL_NO_RTOS */

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)
#ifndef TMFL_NO_RTOS
	    RETIF((errCode =
		   tmdlHdmiTxIWQueueSend(unitTableTx[instance].queueHandle, message)) != TM_OK,
		  errCode)

	    /* Disable interrupts for Tx until the callbacks have been done by the command task */
	    switch (instance) {
	case INSTANCE_0:
		tmdlHdmiTxIWDisableInterrupts(TMDL_HDMI_IW_TX_1);
		break;
	case INSTANCE_1:
		tmdlHdmiTxIWDisableInterrupts(TMDL_HDMI_IW_TX_2);
		break;
	default:
		return TMDL_ERR_DLHDMITX_BAD_INSTANCE;
	}
#else
	    /* Clear T0 flag before polling for interrupts */
	    instanceStatusInfoTx[0].pColBarState->hdcpSecureOrT0 = False;

	if (gI2CDebugAccessesEnabled == True) {

		err = tmbslHdmiTxHwHandleInterrupt(0);

		if ((err == TMBSL_ERR_HDMI_I2C_WRITE) || (err == TMBSL_ERR_HDMI_I2C_READ)) {

			unitTableTx[0].pCallback(TMDL_HDMITX_DEBUG_EVENT_1);
		}

	}

	/* (gI2CDebugAccessesEnabled == True) */
#endif				/* TMFL_NO_RTOS */

	return TM_OK;
}

/******************************************************************************
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
tmErrorCode_t tmdlHdmiTxRegisterCallbacks(tmInstance_t instance, ptmdlHdmiTxCallback_t pCallback) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Check if unit corresponding to instance is opened */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      unitTableTx[instance].opened == False, TMDL_ERR_DLHDMITX_RESOURCE_NOT_OWNED)

	    /* Check if instance state is correct */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      dlHdmiTxGetState(instance) != STATE_INITIALIZED,
		      TMDL_ERR_DLHDMITX_INVALID_STATE)

	    /* Store callback pointers */
	    unitTableTx[instance].pCallback = pCallback;

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
tmErrorCode_t tmdlHdmiTxEnableEvent(tmInstance_t instance, tmdlHdmiTxEvent_t event) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Check if the event exists */
	    RETIF_BADPARAM(event >= EVENT_NB)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Protect the access to this ressource */
	    instanceStatusInfoTx[instance].pEventState[event].status = TMDL_HDMITX_EVENT_ENABLED;

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
tmErrorCode_t tmdlHdmiTxDisableEvent(tmInstance_t instance, tmdlHdmiTxEvent_t event) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Check if the event exists */
	    RETIF_BADPARAM(event >= EVENT_NB)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Protect the access to this ressource */
	    instanceStatusInfoTx[instance].pEventState[event].status = TMDL_HDMITX_EVENT_DISABLED;

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
     tmdlHdmiTxVidFmt_t resolutionID, tmdlHdmiTxVidFmtSpecs_t *pResolutionSpecs) {
	UInt8 i;
	Bool find = False;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Check if ResolutionSpecs pointer is Null */
	    RETIF(pResolutionSpecs == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

	    for (i = 0; i < RESOLUTION_NB; i++) {
		if (resolutionID ==
		    gtmdlHdmiTxDriverConfigTable[instance].pResolutionInfo[i].resolutionID) {
			find = True;
			pResolutionSpecs->height =
			    gtmdlHdmiTxDriverConfigTable[instance].pResolutionInfo[i].height;
			pResolutionSpecs->width =
			    gtmdlHdmiTxDriverConfigTable[instance].pResolutionInfo[i].width;
			pResolutionSpecs->interlaced =
			    gtmdlHdmiTxDriverConfigTable[instance].pResolutionInfo[i].interlaced;
			pResolutionSpecs->vfrequency =
			    gtmdlHdmiTxDriverConfigTable[instance].pResolutionInfo[i].vfrequency;
			pResolutionSpecs->aspectRatio =
			    gtmdlHdmiTxDriverConfigTable[instance].pResolutionInfo[i].aspectRatio;

			/* Transformation of 2D-interlaced formats into 3DFP-progressif formats */
			if ((instanceStatusInfoTx[instance].pVideoInfo->videoInConfig.structure3D ==
			     TMDL_HDMITX_3D_FRAME_PACKING)
			    && pResolutionSpecs->interlaced
			    && ((resolutionID == TMDL_HDMITX_VFMT_20_1920x1080i_50Hz)
				|| (resolutionID == TMDL_HDMITX_VFMT_05_1920x1080i_60Hz))) {
				pResolutionSpecs->interlaced = False;
				if (pResolutionSpecs->vfrequency == TMDL_HDMITX_VFREQ_50Hz) {
					pResolutionSpecs->vfrequency = TMDL_HDMITX_VFREQ_25Hz;
				} else if ((pResolutionSpecs->vfrequency == TMDL_HDMITX_VFREQ_60Hz)
					   || (pResolutionSpecs->vfrequency ==
					       TMDL_HDMITX_VFREQ_59Hz)) {
					pResolutionSpecs->vfrequency = TMDL_HDMITX_VFREQ_30Hz;
				}
			}

			break;
		}
	}

	/* Resolution not found in table */
	RETIF(find == False, TMDL_ERR_DLHDMITX_RESOLUTION_UNKNOWN)

	    return TM_OK;
}

/******************************************************************************
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
     tmdlHdmiTxAudioInConfig_t audioInputConfig, tmdlHdmiTxSinkType_t sinkType) {
	tmErrorCode_t errCode;
	UInt8 pixRepeat;	/* Pixel repetition */
	tmbslHdmiTxVoutDbits_t pathBits;	/* Data path bit width */
	tmbslHdmiTxPixEdge_t pixelEdge;	/* Pixel sampling edge */
	tmbslHdmiTxVsMeth_t syncMethod;	/* Sync method */
	tmbslHdmiTxPixTogl_t toggle;	/* Toggling */
	UInt8 syncIn;		/* Embedded or external */
	tmbslHdmiTxPixSubpkt_t spSync;	/* Subpacket sync */
	tmbslHdmiTxBlnkSrc_t blankit;	/* Blanking */
	tmbslHdmiTxPixRate_t pixRateSingleDouble;	/* HDMITX_PIXRATE_SINGLE */
	UInt16 uRefPix;		/* REFPIX for output */
	UInt16 uRefLine;	/* REFLINE for output */
	UInt16 uScRefPix = 0;	/* REFPIX for scaler */
	UInt16 uScRefLine = 0;	/* REFLINE for scaler */
	Bool bVerified;		/* Scaler setting verified */
	tmbslHdmiTxTopSel_t topSel;	/* Adjustment for interlaced output */
	tmbslHdmiTxHPhases_t phasesH;	/* Horizontal phase */
	tmbslHdmiTxVsOnce_t once;	/* Line/pixel counters sync */
	tmbslHdmiTxScaMode_t scalerMode;	/* Current scaler mode */
	Bool OBASupported;	/* OBA supported or not */
	Bool DSTSupported;	/* DST supported or not */
	Bool HBRSupported;	/* HBR supporeted or not */

	UInt8 *pSwapTable = Null;	/* Initialized after (depend on video mode used) */
	UInt8 *pMirrorTable = Null;	/* Initialized after (depend on video mode used) */
#ifdef TMFL_RGB_DDR_12BITS
	UInt8 *pMux = Null;	/* Initialized after (depend on video mode used) */
#endif
	UInt8 *pEnaVideoPortTable = Null;	/* Initialized after (depend on video mode used) */
	UInt8 *pGndVideoPortTable = Null;	/* Initialized after (depend on video mode used) */
	tmdlHdmiTxVidFmt_t vinFmtIndex;	/* index in table kVfmtToShortFmt_TV */

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Update the instance status information */
	    instanceStatusInfoTx[instance].pVideoInfo->videoInConfig.format =
	    videoInputConfig.format;
	instanceStatusInfoTx[instance].pVideoInfo->videoInConfig.mode = videoInputConfig.mode;
	instanceStatusInfoTx[instance].pVideoInfo->videoInConfig.syncSource =
	    videoInputConfig.syncSource;
	instanceStatusInfoTx[instance].pVideoInfo->videoInConfig.pixelRate =
	    videoInputConfig.pixelRate;
	instanceStatusInfoTx[instance].pVideoInfo->videoInConfig.structure3D =
	    videoInputConfig.structure3D;

	instanceStatusInfoTx[instance].pVideoInfo->videoOutConfig.format = videoOutputConfig.format;
	instanceStatusInfoTx[instance].pVideoInfo->videoOutConfig.mode = videoOutputConfig.mode;
	instanceStatusInfoTx[instance].pVideoInfo->videoOutConfig.colorDepth =
	    videoOutputConfig.colorDepth;

	/* TODO */
	/*instanceStatusInfoTx[instance].pVideoInfo->videoMuteState            = */

	/* Audio OBA support */
	RETIF_SEM(dlHdmiTxItSemaphore[instance],
		  (errCode = tmbslHdmiTxHwGetCapabilities(instance,
							  HDMITX_FEATURE_HW_AUDIO_OBA,
							  &OBASupported)) != TM_OK, errCode)

	    /* Audio DST support */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode = tmbslHdmiTxHwGetCapabilities(instance,
							      HDMITX_FEATURE_HW_AUDIO_DST,
							      &DSTSupported)) != TM_OK, errCode)

	    /* Audio HBR support */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode = tmbslHdmiTxHwGetCapabilities(instance,
							      HDMITX_FEATURE_HW_AUDIO_HBR,
							      &HBRSupported)) != TM_OK, errCode)

	    /* Test if audio input format is supported */
	    if (((audioInputConfig.format == TMDL_HDMITX_AFMT_OBA) && (OBASupported == False)) ||
		((audioInputConfig.format == TMDL_HDMITX_AFMT_DST) && (DSTSupported == False)) ||
		((audioInputConfig.format == TMDL_HDMITX_AFMT_HBR) && (HBRSupported == False))) {
		/* Release the sempahore */
		RETIF((errCode =
		       tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

		    return TMDL_ERR_DLHDMITX_NOT_SUPPORTED;
	}

	instanceStatusInfoTx[instance].pAudioInfo->audioInCfg.format = audioInputConfig.format;
	instanceStatusInfoTx[instance].pAudioInfo->audioInCfg.i2sFormat =
	    audioInputConfig.i2sFormat;
	instanceStatusInfoTx[instance].pAudioInfo->audioInCfg.i2sQualifier =
	    audioInputConfig.i2sQualifier;
	instanceStatusInfoTx[instance].pAudioInfo->audioInCfg.rate = audioInputConfig.rate;
	instanceStatusInfoTx[instance].pAudioInfo->audioInCfg.channelAllocation =
	    audioInputConfig.channelAllocation;


	if (sinkType == TMDL_HDMITX_SINK_EDID) {
		/* Change sink type with the currently defined in EDID */
		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxEdidGetSinkType(instance,
								(tmbslHdmiTxSinkType_t *) &
								sinkType)) != TM_OK, errCode)
	}

	/* forbid format with pixel repetition in DVI */
	if (sinkType == TMDL_HDMITX_SINK_DVI) {
		if (((videoOutputConfig.format >= TMDL_HDMITX_VFMT_06_720x480i_60Hz)
		     && (videoOutputConfig.format <= TMDL_HDMITX_VFMT_15_1440x480p_60Hz))
		    || ((videoOutputConfig.format >= TMDL_HDMITX_VFMT_21_720x576i_50Hz)
			&& (videoOutputConfig.format <= TMDL_HDMITX_VFMT_30_1440x576p_50Hz))
		    || ((videoOutputConfig.format >= TMDL_HDMITX_VFMT_35_2880x480p_60Hz)
			&& (videoOutputConfig.format <= TMDL_HDMITX_VFMT_38_2880x576p_50Hz))
		    ) {
			/* Release the sempahore */
			RETIF((errCode =
			       tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK,
			      errCode)

			    return TMDL_ERR_DLHDMITX_BAD_PARAMETER;
		}
	}

	/* Set color depth according to output config, transmitter termination is disable */
	RETIF_SEM(dlHdmiTxItSemaphore[instance],
		  (errCode = tmbslHdmiTxSetColorDepth(instance,
						      (tmbslHdmiTxColorDepth) (videoOutputConfig.
									       colorDepth),
						      False)) != TM_OK, errCode)

	    /* Set the TMDS outputs to a forced state */
	    errCode = tmbslHdmiTxTmdsSetOutputs(instance, HDMITX_TMDSOUT_FORCED0);
	RETIF_SEM(dlHdmiTxItSemaphore[instance], (errCode != TM_OK)
		  && (errCode != TMBSL_ERR_HDMI_NOT_SUPPORTED), errCode)

	    /* Fine-tune the TMDS serializer */
	    errCode = tmbslHdmiTxTmdsSetSerializer(instance, 4, 8);
	RETIF_SEM(dlHdmiTxItSemaphore[instance], (errCode != TM_OK)
		  && (errCode != TMBSL_ERR_HDMI_NOT_SUPPORTED), errCode)

	    /* Set video output configuration */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode = tmbslHdmiTxVideoOutSetConfig(instance,
							      (tmbslHdmiTxSinkType_t) sinkType,
							      (tmbslHdmiTxVoutMode_t)
							      videoOutputConfig.mode,
							      HDMITX_VOUT_PREFIL_OFF,
							      HDMITX_VOUT_YUV_BLNK_16,
							      HDMITX_VOUT_QRANGE_FS)) != TM_OK,
		      errCode)

	    /* Set default config */
	    pixRepeat = HDMITX_PIXREP_DEFAULT;
	pathBits = HDMITX_VOUT_DBITS_12;
	pixelEdge = HDMITX_PIXEDGE_CLK_POS;
	syncMethod = HDMITX_VSMETH_V_H;
	toggle = HDMITX_PIXTOGL_ENABLE;

	/* Set sync details */
	if (videoInputConfig.syncSource == TMDL_HDMITX_SYNCSRC_EMBEDDED) {
		/* Embedded sync */
		syncIn = EMB;
		spSync = HDMITX_PIXSUBPKT_SYNC_HEMB;
		blankit = HDMITX_BLNKSRC_VS_HEMB_VEMB;
		syncMethod = HDMITX_VSMETH_V_XDE;
	} else {
		/* External sync */
		syncIn = EXT;


		if (gtmdlHdmiTxDriverConfigTable[instance].dataEnableSignalAvailable == 1) {
			/* DE is available */
			spSync = HDMITX_PIXSUBPKT_SYNC_DE;
		} else {
			/* DE is NOT available */
			spSync = HDMITX_PIXSUBPKT_SYNC_HS;
		}



		blankit = HDMITX_BLNKSRC_NOT_DE;
	}


#ifdef TMFL_RGB_DDR_12BITS
	/* by default, mux is not used */
	pMux = &gtmdlHdmiTxDriverConfigTable[instance].pNoMux[0];
#endif

	/* Port swap table */
	switch (videoInputConfig.mode) {
	case TMDL_HDMITX_VINMODE_CCIR656:
		pathBits = HDMITX_VOUT_DBITS_8;
		pixelEdge = HDMITX_PIXEDGE_CLK_NEG;
		pSwapTable = gtmdlHdmiTxDriverConfigTable[instance].pSwapTableCCIR656;
		pMirrorTable = gtmdlHdmiTxDriverConfigTable[instance].pMirrorTableCCIR656;
#ifdef TMFL_RGB_DDR_12BITS
		pMux = &gtmdlHdmiTxDriverConfigTable[instance].pMux_RGB_DDR_12bits[0];
#endif
		pEnaVideoPortTable = gtmdlHdmiTxDriverConfigTable[instance].pEnableVideoPortCCIR656;
		pGndVideoPortTable = gtmdlHdmiTxDriverConfigTable[instance].pGroundVideoPortCCIR656;
		break;

	case TMDL_HDMITX_VINMODE_RGB444:
		pSwapTable = gtmdlHdmiTxDriverConfigTable[instance].pSwapTableRGB444;
		pMirrorTable = gtmdlHdmiTxDriverConfigTable[instance].pMirrorTableRGB444;
		pEnaVideoPortTable = gtmdlHdmiTxDriverConfigTable[instance].pEnableVideoPortRGB444;
		pGndVideoPortTable = gtmdlHdmiTxDriverConfigTable[instance].pGroundVideoPortRGB444;
		break;

	case TMDL_HDMITX_VINMODE_YUV444:
		pSwapTable = gtmdlHdmiTxDriverConfigTable[instance].pSwapTableYUV444;
		pMirrorTable = gtmdlHdmiTxDriverConfigTable[instance].pMirrorTableYUV444;
		pEnaVideoPortTable = gtmdlHdmiTxDriverConfigTable[instance].pEnableVideoPortYUV444;
		pGndVideoPortTable = gtmdlHdmiTxDriverConfigTable[instance].pGroundVideoPortYUV444;
		break;

	case TMDL_HDMITX_VINMODE_YUV422:
		pSwapTable = gtmdlHdmiTxDriverConfigTable[instance].pSwapTableYUV422;
		pMirrorTable = gtmdlHdmiTxDriverConfigTable[instance].pMirrorTableYUV422;
#ifdef TMFL_RGB_DDR_12BITS
		pMux = &gtmdlHdmiTxDriverConfigTable[instance].pMux_RGB_DDR_12bits[0];
#endif
		pEnaVideoPortTable = gtmdlHdmiTxDriverConfigTable[instance].pEnableVideoPortYUV422;
		pGndVideoPortTable = gtmdlHdmiTxDriverConfigTable[instance].pGroundVideoPortYUV422;
		break;

#ifdef TMFL_RGB_DDR_12BITS
	case TMDL_HDMITX_VINMODE_RGB_DDR_12BITS:
		pSwapTable = gtmdlHdmiTxDriverConfigTable[instance].pSwapTableRGB_DDR_12bits;
		pMirrorTable = gtmdlHdmiTxDriverConfigTable[instance].pMirrorTableRGB_DDR_12bits;
		pMux = &gtmdlHdmiTxDriverConfigTable[instance].pMux_RGB_DDR_12bits[0];
		pEnaVideoPortTable =
		    gtmdlHdmiTxDriverConfigTable[instance].pEnableVideoPortRGB_DDR_12bits;
		pGndVideoPortTable =
		    gtmdlHdmiTxDriverConfigTable[instance].pGroundVideoPortRGB_DDR_12bits;
		break;
#endif
	default:
		break;
	}

	/* Set the audio and video input port configuration */
	errCode = tmbslHdmiTxSetVideoPortConfig(instance, pEnaVideoPortTable, pGndVideoPortTable);
	RETIF_SEM(dlHdmiTxItSemaphore[instance], (errCode != TM_OK)
		  && (errCode != TMBSL_ERR_HDMI_NOT_SUPPORTED), errCode);

#ifdef TMFL_RGB_DDR_12BITS
	errCode = tmbslHdmiTxVideoInSetMapping(instance, pSwapTable, pMirrorTable, pMux);
#else
	errCode = tmbslHdmiTxVideoInSetMapping(instance, pSwapTable, pMirrorTable);
#endif
	RETIF_SEM(dlHdmiTxItSemaphore[instance], (errCode != TM_OK)
		  && (errCode != TMBSL_ERR_HDMI_NOT_SUPPORTED), errCode);

	/* Set fine image position */
	RETIF_SEM(dlHdmiTxItSemaphore[instance],
		  (errCode =
		   tmbslHdmiTxVideoInSetFine(instance, spSync, HDMITX_PIXTOGL_NO_ACTION)) != TM_OK,
		  errCode);

	/* Set input blanking */
	errCode = tmbslHdmiTxVideoInSetBlanking(instance, blankit, HDMITX_BLNKCODE_ALL_0);
	RETIF_SEM(dlHdmiTxItSemaphore[instance], (errCode != TM_OK)
		  && (errCode != TMBSL_ERR_HDMI_NOT_SUPPORTED), errCode);

	/* Configure video input options and control the upsampler */
	RETIF_SEM(dlHdmiTxItSemaphore[instance],
		  (errCode = tmbslHdmiTxVideoInSetConfig(instance,
							 (tmbslHdmiTxVinMode_t) videoInputConfig.
							 mode,
							 (tmbslHdmiTxVidFmt_t) videoOutputConfig.
							 format,
							 (tmbslHdmiTx3DStructure_t)
							 videoInputConfig.structure3D, pixelEdge,
							 (tmbslHdmiTxPixRate_t) videoInputConfig.
							 pixelRate, HDMITX_UPSAMPLE_AUTO)) != TM_OK,
		  errCode);


	/* Set input ouput - may give NOT_SUPPORTED error */
	RETIF_SEM(dlHdmiTxItSemaphore[instance],
		  (errCode =
		   tmbslHdmiTxVideoSetInOut(instance, (tmbslHdmiTxVidFmt_t) videoInputConfig.format,
					    (tmbslHdmiTx3DStructure_t) videoInputConfig.structure3D,
					    HDMITX_SCAMODE_AUTO,
					    (tmbslHdmiTxVidFmt_t) videoOutputConfig.format,
					    pixRepeat, HDMITX_MATMODE_AUTO, pathBits,
					    (tmbslHdmiTxVQR_t) videoOutputConfig.dviVqr)) != TM_OK,
		  errCode);


	/* Only set audio for HDMI, not DVI */
	if (sinkType == TMDL_HDMITX_SINK_HDMI) {
		/* Release the sempahore */
		RETIF((errCode =
		       tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)
		    /* Set audio parameters */
		    RETIF((errCode =
			   tmdlHdmiTxSetAudioInput(instance, audioInputConfig, sinkType)) != TM_OK,
			  errCode)
		    /* Take the sempahore */
		    RETIF((errCode =
			   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)
	}

	/* Output fine adjustment */
	pixRateSingleDouble = (tmbslHdmiTxPixRate_t) videoInputConfig.pixelRate;
	if (videoInputConfig.pixelRate == HDMITX_PIXRATE_SINGLE_REPEATED) {
		pixRateSingleDouble = HDMITX_PIXRATE_SINGLE;
	}


	if ((videoInputConfig.structure3D != HDMITX_3D_FRAME_PACKING) &&
	    dlHdmiTxGetReflineRefpix(videoInputConfig.format, videoInputConfig.mode,
				     videoOutputConfig.format, syncIn,
				     (tmdlHdmiTxPixRate_t) pixRateSingleDouble, &uRefPix, &uRefLine,
				     &uScRefPix, &uScRefLine, &bVerified) > 0) {
		/* From 720p50/60 or 1080i50/60 up-scaling to 1080p50/60, when external sync,
		   toggleV, toggleH and toggleX need to be set to 0 */
		if (syncIn == EXT) {
			switch (videoInputConfig.format) {
			case TMDL_HDMITX_VFMT_04_1280x720p_60Hz:
			case TMDL_HDMITX_VFMT_19_1280x720p_50Hz:
			case TMDL_HDMITX_VFMT_05_1920x1080i_60Hz:
			case TMDL_HDMITX_VFMT_20_1920x1080i_50Hz:
				if ((videoOutputConfig.format ==
				     TMDL_HDMITX_VFMT_16_1920x1080p_60Hz)
				    || (videoOutputConfig.format ==
					TMDL_HDMITX_VFMT_31_1920x1080p_50Hz)) {
					toggle = HDMITX_PIXTOGL_NO_ACTION;
				}
				break;
			default:
				toggle = HDMITX_PIXTOGL_ENABLE;
				break;
			}
		}

		/* Combination found in table for scaler: configure input manually */
		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxVideoInSetSyncManual(instance,
								     (tmbslHdmiTxSyncSource_t)
								     videoInputConfig.syncSource,
								     syncMethod, toggle, toggle,
								     toggle, uRefPix,
								     uRefLine)) != TM_OK, errCode)
	} else {
		/* Not found so assume non-scaler and auto-configure input */
		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxVideoInSetSyncAuto(instance,
								   (tmbslHdmiTxSyncSource_t)
								   videoInputConfig.syncSource,
								   (tmbslHdmiTxVidFmt_t)
								   videoInputConfig.format,
								   (tmbslHdmiTxVinMode_t)
								   videoInputConfig.mode,
								   (tmbslHdmiTx3DStructure_t)
								   videoInputConfig.structure3D)) !=
			  TM_OK, errCode)
	}

	/* Only set infoframes for HDMI, not DVI */
	if (sinkType == TMDL_HDMITX_SINK_HDMI) {
		/* Set avi infoframe */
		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode =
			   dlHdmiTxSetVideoInfoframe(instance, videoOutputConfig.format,
						     videoOutputConfig.mode)) != TM_OK, errCode)
	}

	errCode = tmbslHdmiTxScalerGetMode(instance, &scalerMode);

	/* Ignore scaler TMBSL_ERR_HDMI_NOT_SUPPORTED error */
	if ((errCode == TM_OK) && (scalerMode == HDMITX_SCAMODE_ON)) {
		/* Enable scaler mode */
		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxScalerInDisable(instance, False)) != TM_OK, errCode)

		    /* Correction to interlace */
		    topSel = HDMITX_TOPSEL_INTERNAL;
		if ((videoOutputConfig.format == TMDL_HDMITX_VFMT_05_1920x1080i_60Hz)
		    || (videoOutputConfig.format == TMDL_HDMITX_VFMT_20_1920x1080i_50Hz)) {
			/* video input format is range-checked by tmbslHdmiTxVideoSetInOut above */
			vinFmtIndex = dlHdmiTxCalcVidFmtIndex(videoInputConfig.format);
			if ((kVfmtToShortFmt_TV[vinFmtIndex] == TV_480p_60Hz)
			    || (kVfmtToShortFmt_TV[vinFmtIndex] == TV_576p_50Hz)) {
				/* Correct for 1080i output for p->i conversion only */
				topSel = HDMITX_TOPSEL_VRF;
			}
		}

		/* Set scaler field positions */
		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxScalerSetFieldOrder(instance,
								    HDMITX_INTEXT_NO_CHANGE,
								    HDMITX_INTEXT_NO_CHANGE, topSel,
								    HDMITX_TOPTGL_NO_CHANGE)) !=
			  TM_OK, errCode)

		    /* Scaler fine adjustment */
		    RETIF_SEM(dlHdmiTxItSemaphore[instance],
			      (errCode = tmbslHdmiTxScalerSetFine(instance,
								  uScRefPix, uScRefLine)) != TM_OK,
			      errCode)

		    if ((videoOutputConfig.format == TMDL_HDMITX_VFMT_16_1920x1080p_60Hz)
			|| (videoOutputConfig.format == TMDL_HDMITX_VFMT_31_1920x1080p_50Hz)) {
			phasesH = HDMITX_H_PHASES_16;
		} else {
			phasesH = HDMITX_H_PHASES_15;
		}

		/* Set scaler phase */
		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxScalerSetPhase(instance,
							       phasesH)) != TM_OK, errCode)

		    /* Set scaler latency */
		    RETIF_SEM(dlHdmiTxItSemaphore[instance],
			      (errCode = tmbslHdmiTxScalerSetLatency(instance,
								     0x22)) != TM_OK, errCode)

		    /* Set scaler synchronisation option */
		    RETIF_SEM(dlHdmiTxItSemaphore[instance],
			      (errCode = tmbslHdmiTxScalerSetSync(instance,
								  syncMethod,
								  HDMITX_VSONCE_EACH_FRAME)) !=
			      TM_OK, errCode)

		    /* With scaler, use Only Once setting for tmbslHdmiTxVideoOutSetSync */
		    once = HDMITX_VSONCE_ONCE;
	} else {
		once = HDMITX_VSONCE_EACH_FRAME;
	}

	/* Set video synchronisation */
	RETIF_SEM(dlHdmiTxItSemaphore[instance],
		  (errCode = tmbslHdmiTxVideoOutSetSync(instance,
							HDMITX_VSSRC_INTERNAL,
							HDMITX_VSSRC_INTERNAL,
							HDMITX_VSSRC_INTERNAL, HDMITX_VSTGL_TABLE,
							once)) != TM_OK, errCode)

	    /* Release the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    instanceStatusInfoTx[instance].pColBarState->inOutFirstSetDone = True;

	/* Test if pattern is already on */
	if (instanceStatusInfoTx[instance].pColBarState->colorBarOn == True) {
		/* If pattern is On, apply new settings */
		instanceStatusInfoTx[instance].pColBarState->changeColorBarNow = True;
	}

	return TM_OK;
}

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
     tmdlHdmiTxAudioInConfig_t audioInputConfig, tmdlHdmiTxSinkType_t sinkType) {
	tmErrorCode_t errCode;
	tmdlHdmiTxVidFmtSpecs_t resolutionSpecs;	/* Used to convert video format to video frequency */
	UInt8 layout;		/* 0 or 1 */
	UInt8 aifChannelCountCode = 0;	/* audio info frame channels */
	tmbslHdmiTxVfreq_t vOutFreq;	/* Vertical output frequency */
	tmbslHdmiTxctsRef_t ctsRef;	/* CTS ref source */
	UInt16 uCtsX;		/* CtsX value */
	tmbslHdmiTxPktAif_t pktAif;	/* Audio infoframe packet */
	Bool OBASupported;	/* OBA supported or not */
	Bool DSTSupported;	/* DST supported or not */
	Bool HBRSupported;	/* HBR supporeted or not */
	UInt8 *pEnaAudioPortCfg;
	UInt8 *pGndAudioPortCfg;
	UInt8 *pEnaAudioClockPortCfg;
	UInt8 *pGndAudioClockPortCfg;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Take the semaphore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Audio OBA support */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode = tmbslHdmiTxHwGetCapabilities(instance,
							      HDMITX_FEATURE_HW_AUDIO_OBA,
							      &OBASupported)) != TM_OK, errCode)

	    /* Audio DST support */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode = tmbslHdmiTxHwGetCapabilities(instance,
							      HDMITX_FEATURE_HW_AUDIO_DST,
							      &DSTSupported)) != TM_OK, errCode)

	    /* Audio HBR support */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode = tmbslHdmiTxHwGetCapabilities(instance,
							      HDMITX_FEATURE_HW_AUDIO_HBR,
							      &HBRSupported)) != TM_OK, errCode)

	    /* Test if audio input format is supported */
	    if (((audioInputConfig.format == TMDL_HDMITX_AFMT_OBA) && (OBASupported == False)) ||
		((audioInputConfig.format == TMDL_HDMITX_AFMT_DST) && (DSTSupported == False)) ||
		((audioInputConfig.format == TMDL_HDMITX_AFMT_HBR) && (HBRSupported == False))) {
		/* Release the semaphore */
		RETIF((errCode =
		       tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

		    return TMDL_ERR_DLHDMITX_NOT_SUPPORTED;
	}

	if (sinkType == TMDL_HDMITX_SINK_EDID) {
		/* Change sink type with the currently defined in EDID */
		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxEdidGetSinkType(instance,
								(tmbslHdmiTxSinkType_t *) &
								sinkType)) != TM_OK, errCode)
	}

	if (sinkType == TMDL_HDMITX_SINK_HDMI) {
		/* Set audio layout */
		layout = 1;
		if (audioInputConfig.channelAllocation == 0x00) {
			layout = 0;
		}
		aifChannelCountCode = kChanAllocChanNum[audioInputConfig.channelAllocation] - 1;

		/* Port audio configuration */
		switch (audioInputConfig.format) {
		case TMDL_HDMITX_AFMT_SPDIF:
			pEnaAudioPortCfg =
			    gtmdlHdmiTxDriverConfigTable[instance].pEnableAudioPortSPDIF;
			pGndAudioPortCfg =
			    gtmdlHdmiTxDriverConfigTable[instance].pGroundAudioPortSPDIF;
			pEnaAudioClockPortCfg =
			    gtmdlHdmiTxDriverConfigTable[instance].pEnableAudioClockPortSPDIF;
			pGndAudioClockPortCfg =
			    gtmdlHdmiTxDriverConfigTable[instance].pGroundAudioClockPortSPDIF;
			break;

		case TMDL_HDMITX_AFMT_I2S:
			pEnaAudioClockPortCfg =
			    gtmdlHdmiTxDriverConfigTable[instance].pEnableAudioClockPortI2S;
			pGndAudioClockPortCfg =
			    gtmdlHdmiTxDriverConfigTable[instance].pGroundAudioClockPortI2S;

			if (audioInputConfig.channelAllocation >= 1) {	/* For Multi channels */
				pEnaAudioPortCfg =
				    gtmdlHdmiTxDriverConfigTable[instance].pEnableAudioPortI2S8C;
				pGndAudioPortCfg =
				    gtmdlHdmiTxDriverConfigTable[instance].pGroundAudioPortI2S8C;
			} else {
				pEnaAudioPortCfg =
				    gtmdlHdmiTxDriverConfigTable[instance].pEnableAudioPortI2S;
				pGndAudioPortCfg =
				    gtmdlHdmiTxDriverConfigTable[instance].pGroundAudioPortI2S;
			}
			break;

		case TMDL_HDMITX_AFMT_OBA:
			pEnaAudioPortCfg =
			    gtmdlHdmiTxDriverConfigTable[instance].pEnableAudioPortOBA;
			pGndAudioPortCfg =
			    gtmdlHdmiTxDriverConfigTable[instance].pGroundAudioPortOBA;
			pEnaAudioClockPortCfg =
			    gtmdlHdmiTxDriverConfigTable[instance].pEnableAudioClockPortOBA;
			pGndAudioClockPortCfg =
			    gtmdlHdmiTxDriverConfigTable[instance].pGroundAudioClockPortOBA;
			break;

		case TMDL_HDMITX_AFMT_DST:
			pEnaAudioPortCfg =
			    gtmdlHdmiTxDriverConfigTable[instance].pEnableAudioPortDST;
			pGndAudioPortCfg =
			    gtmdlHdmiTxDriverConfigTable[instance].pGroundAudioPortDST;
			pEnaAudioClockPortCfg =
			    gtmdlHdmiTxDriverConfigTable[instance].pEnableAudioClockPortDST;
			pGndAudioClockPortCfg =
			    gtmdlHdmiTxDriverConfigTable[instance].pGroundAudioClockPortDST;
			break;

		case TMDL_HDMITX_AFMT_HBR:
			pEnaAudioPortCfg =
			    gtmdlHdmiTxDriverConfigTable[instance].pEnableAudioPortHBR;
			pGndAudioPortCfg =
			    gtmdlHdmiTxDriverConfigTable[instance].pGroundAudioPortHBR;
			pEnaAudioClockPortCfg =
			    gtmdlHdmiTxDriverConfigTable[instance].pEnableAudioClockPortHBR;
			pGndAudioClockPortCfg =
			    gtmdlHdmiTxDriverConfigTable[instance].pGroundAudioClockPortHBR;
			break;

		default:
			/* Release the sempahore */
			RETIF((errCode =
			       tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK,
			      errCode)

			    return TMDL_ERR_DLHDMITX_BAD_PARAMETER;
		}

		errCode = tmbslHdmiTxSetAudioPortConfig(instance,
							pEnaAudioPortCfg, pGndAudioPortCfg);
		RETIF_SEM(dlHdmiTxItSemaphore[instance], (errCode != TM_OK)
			  && (errCode != TMBSL_ERR_HDMI_NOT_SUPPORTED), errCode)

		    errCode = tmbslHdmiTxSetAudioClockPortConfig(instance,
								 pEnaAudioClockPortCfg,
								 pGndAudioClockPortCfg);
		RETIF_SEM(dlHdmiTxItSemaphore[instance], (errCode != TM_OK)
			  && (errCode != TMBSL_ERR_HDMI_NOT_SUPPORTED), errCode)

		    RETIF_SEM(dlHdmiTxItSemaphore[instance],
			      (errCode = tmbslHdmiTxAudioInSetConfig(instance,
								     (tmbslHdmiTxaFmt_t)
								     audioInputConfig.format,
								     (tmbslHdmiTxI2sFor_t)
								     audioInputConfig.i2sFormat,
								     audioInputConfig.
								     channelAllocation,
								     HDMITX_CHAN_NO_CHANGE,
								     HDMITX_CLKPOLDSD_NO_CHANGE,
								     HDMITX_SWAPDSD_NO_CHANGE,
								     layout, HDMITX_LATENCY_CURRENT,
								     (tmbslHdmiTxDstRate_t)
								     audioInputConfig.dstRate)) !=
			      TM_OK, errCode)

		    /* Find output vertical frequency from output format */
		    RETIF_SEM(dlHdmiTxItSemaphore[instance],
			      (errCode =
			       tmdlHdmiTxGetVideoFormatSpecs(instance,
							     instanceStatusInfoTx[instance].
							     pVideoInfo->videoOutConfig.format,
							     &resolutionSpecs)) != TM_OK, errCode)
		    vOutFreq = (tmbslHdmiTxVfreq_t) resolutionSpecs.vfrequency;

		if ((audioInputConfig.format == TMDL_HDMITX_AFMT_SPDIF)
		    || (audioInputConfig.format == TMDL_HDMITX_AFMT_OBA)) {
			ctsRef = HDMITX_CTSREF_FS64SPDIF;
			uCtsX = HDMITX_CTSX_64;
		} else {	/* I2S */

			ctsRef = HDMITX_CTSREF_ACLK;
			if (audioInputConfig.i2sQualifier == TMDL_HDMITX_I2SQ_32BITS) {
				uCtsX = HDMITX_CTSX_64;
			} else {
				uCtsX = HDMITX_CTSX_32;
			}
		}

		/* Set the Clock Time Stamp generator in HDMI mode only */
		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxAudioInSetCts(instance,
							      ctsRef,
							      (tmbslHdmiTxafs_t) audioInputConfig.
							      rate,
							      (tmbslHdmiTxVidFmt_t)
							      instanceStatusInfoTx[instance].
							      pVideoInfo->videoOutConfig.format,
							      vOutFreq, HDMITX_CTS_AUTO, uCtsX,
							      HDMITX_CTSK_USE_CTSX,
							      HDMITX_CTSMTS_USE_CTSX,
							      (tmbslHdmiTxDstRate_t)
							      audioInputConfig.dstRate)) != TM_OK,
			  errCode)

		    /* Set Channel Status registers
		       No need to call tmbslTDA9984AudioOutSetChanStatusMapping, since default Byte 2
		       values of "Do not take into account" are adequate */
		    if (audioInputConfig.format != TMDL_HDMITX_AFMT_SPDIF) {	/* channel status automatically copied from SPDIF */

			if (audioInputConfig.format != TMDL_HDMITX_AFMT_HBR) {

				RETIF_SEM(dlHdmiTxItSemaphore[instance],
					  (errCode = tmbslHdmiTxAudioOutSetChanStatus(instance,
										      (tmbslHdmiTxAudioData_t)
										      audioInputConfig.
										      channelStatus.
										      PcmIdentification,
										      (tmbslHdmiTxCSformatInfo_t)
										      audioInputConfig.
										      channelStatus.
										      FormatInfo,
										      (tmbslHdmiTxCScopyright_t)
										      audioInputConfig.
										      channelStatus.
										      CopyrightInfo,
										      audioInputConfig.
										      channelStatus.
										      categoryCode,
										      (tmbslHdmiTxafs_t)
										      audioInputConfig.
										      rate,
										      (tmbslHdmiTxCSclkAcc_t)
										      audioInputConfig.
										      channelStatus.
										      clockAccuracy,
										      (tmbslHdmiTxCSmaxWordLength_t)
										      audioInputConfig.
										      channelStatus.
										      maxWordLength,
										      (tmbslHdmiTxCSwordLength_t)
										      audioInputConfig.
										      channelStatus.
										      wordLength,
										      (tmbslHdmiTxCSorigAfs_t)
										      audioInputConfig.
										      channelStatus.
										      origSampleFreq))
					  != TM_OK, errCode)
			} else {


				RETIF_SEM(dlHdmiTxItSemaphore[instance],
					  (errCode = tmbslHdmiTxAudioOutSetChanStatus(instance,
										      (tmbslHdmiTxAudioData_t)
										      audioInputConfig.
										      channelStatus.
										      PcmIdentification,
										      (tmbslHdmiTxCSformatInfo_t)
										      audioInputConfig.
										      channelStatus.
										      FormatInfo,
										      (tmbslHdmiTxCScopyright_t)
										      audioInputConfig.
										      channelStatus.
										      CopyrightInfo,
										      audioInputConfig.
										      channelStatus.
										      categoryCode,
										      HDMITX_AFS_768K,
										      (tmbslHdmiTxCSclkAcc_t)
										      audioInputConfig.
										      channelStatus.
										      clockAccuracy,
										      (tmbslHdmiTxCSmaxWordLength_t)
										      audioInputConfig.
										      channelStatus.
										      maxWordLength,
										      (tmbslHdmiTxCSwordLength_t)
										      audioInputConfig.
										      channelStatus.
										      wordLength,
										      (tmbslHdmiTxCSorigAfs_t)
										      audioInputConfig.
										      channelStatus.
										      origSampleFreq))
					  != TM_OK, errCode)


			}

		}


		/* Set reset_fifo to 1 */
		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxAudioOutSetMute(instance,
								HDMITX_AMUTE_ON)) != TM_OK, errCode)

		    /* UN Mute audio only if previously not muted */
		    if (instanceStatusInfoTx[instance].pAudioInfo->audioMuteState == False) {

			/* Wait for 20 ms */
			RETIF_SEM(dlHdmiTxItSemaphore[instance],
				  (errCode = tmdlHdmiTxIWWait(20)) != TM_OK, errCode)
			    /* Set reset_fifo to 0 */
			    RETIF_SEM(dlHdmiTxItSemaphore[instance],
				      (errCode = tmbslHdmiTxAudioOutSetMute(instance,
									    HDMITX_AMUTE_OFF)) !=
				      TM_OK, errCode)

		}


		/* Set audio infoframe */
		pktAif.ChannelCount = aifChannelCountCode;
		pktAif.CodingType = 0;	/* refer to stream header */
		pktAif.SampleSize = 0;	/* refer to stream header */
		pktAif.ChannelAlloc = audioInputConfig.channelAllocation;
		pktAif.LevelShift = 0;	/* 0dB level shift */
		pktAif.DownMixInhibit = 0;	/* down-mix stereo permitted */
		pktAif.SampleFreq = AIF_SF_REFER_TO_STREAM_HEADER;	/* refer to stream header */

		/* SampleFreq parameter need to be set for OBA and DST audio stream */
		if ((audioInputConfig.format == TMDL_HDMITX_AFMT_OBA) ||
		    (audioInputConfig.format == TMDL_HDMITX_AFMT_DST)) {
			switch (audioInputConfig.rate) {
			case TMDL_HDMITX_AFS_32K:
				pktAif.SampleFreq = AIF_SF_32K;	/* see table 18 of CEA-861 */
				break;
			case TMDL_HDMITX_AFS_44K:
				pktAif.SampleFreq = AIF_SF_44K;
				break;
			case TMDL_HDMITX_AFS_48K:
				pktAif.SampleFreq = AIF_SF_48K;
				break;
			case TMDL_HDMITX_AFS_88K:
				pktAif.SampleFreq = AIF_SF_88K;
				break;
			case TMDL_HDMITX_AFS_96K:
				pktAif.SampleFreq = AIF_SF_96K;
				break;
			case TMDL_HDMITX_AFS_176K:
				pktAif.SampleFreq = AIF_SF_176K;
				break;
			case TMDL_HDMITX_AFS_192K:
				pktAif.SampleFreq = AIF_SF_192K;
				break;
			default:
				pktAif.SampleFreq = AIF_SF_REFER_TO_STREAM_HEADER;	/* refer to stream header */
				break;
			}
		}

		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxPktSetAudioInfoframe(instance,
								     &pktAif, True)) != TM_OK,
			  errCode)
	}

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
    (tmInstance_t instance, Bool enable, tmdlHdmiTxAviIfData_t *pAviIfData) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    if (enable == True) {
		/* Check if AviIfData pointer is Null */
		RETIF_SEM(dlHdmiTxItSemaphore[instance], pAviIfData == Null,
			  TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

		    errCode = dlHdmiTxSetRawVideoInfoframe(instance, pAviIfData, enable);
	} else {
		errCode = dlHdmiTxSetRawVideoInfoframe(instance, Null, enable);
	}

	/* Ignore infoframe interlock in DVI mode */
	if (errCode == TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED) {
		errCode = TM_OK;
	}

	RETIF_SEM(dlHdmiTxItSemaphore[instance], errCode != TM_OK, errCode)

	    /* Release the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
    (tmInstance_t instance, Bool enable, tmdlHdmiTxAudIfData_t *pAudIfData) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    if (enable == True) {
		/* Check if AudIfData pointer is Null */
		RETIF_SEM(dlHdmiTxItSemaphore[instance], pAudIfData == Null,
			  TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

		    RETIF_SEM(dlHdmiTxItSemaphore[instance],
			      (errCode = tmbslHdmiTxPktSetAudioInfoframe(instance,
									 (tmbslHdmiTxPktAif_t *)
									 pAudIfData,
									 enable)) != TM_OK, errCode)
	} else {
		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxPktSetAudioInfoframe(instance,
								     Null, enable)) != TM_OK,
			  errCode)
	}

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
    (tmInstance_t instance, Bool enable, tmdlHdmiTxAcpPktData_t *pAcpPktData) {
	tmErrorCode_t errCode;
	tmbslHdmiTxPkt_t pkt;
	UInt8 i;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    if (enable == True) {
		/* Check if AcpPktData pointer is Null */
		RETIF_SEM(dlHdmiTxItSemaphore[instance], pAcpPktData == Null,
			  TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

		    switch (pAcpPktData->acpType) {
			/* Make sure bytes reserved are 0 */
		case 0:	/* Generic audio */
			for (i = 0; i < 28; i++) {
				pkt.dataByte[i] = 0;
			}
			break;

		case 1:	/* IEC 60958 identified audio */
			for (i = 0; i < 28; i++) {
				pkt.dataByte[i] = 0;
			}
			break;

		case 2:	/* DVD Audio */
			for (i = 0; i < 2; i++) {
				pkt.dataByte[i] = pAcpPktData->acpData[i];
			}
			for (i = 2; i < 28; i++) {
				pkt.dataByte[i] = 0;
			}
			break;

		case 3:	/* SuperAudio CD */
			for (i = 0; i < 17; i++) {
				pkt.dataByte[i] = pAcpPktData->acpData[i];
			}
			for (i = 17; i < 28; i++) {
				pkt.dataByte[i] = 0;
			}
			break;

		default:
			/* Release the sempahore */
			RETIF((errCode =
			       tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK,
			      errCode)
			    return TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS;
		}

		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxPktSetAcp(instance,
							  &pkt, 28, pAcpPktData->acpType,
							  enable)) != TM_OK, errCode)
	} else {
		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxPktSetAcp(instance,
							  Null, 0, 0, enable)) != TM_OK, errCode)
	}

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
    (tmInstance_t instance, Bool enable, tmdlHdmiTxGcpPktData_t *pGcpPktData) {
	tmErrorCode_t errCode;
	tmbslHdmiTxaMute_t aMute;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    if (enable == True) {
		/* Check if GcpPktData pointer is Null */
		RETIF_SEM(dlHdmiTxItSemaphore[instance], pGcpPktData == Null,
			  TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

		    if (pGcpPktData->avMute == False) {
			aMute = HDMITX_AMUTE_OFF;
		} else {
			aMute = HDMITX_AMUTE_ON;
		}

		/* Set contents of general control packet & enable/disable packet insertion */
		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxPktSetGeneralCntrl(instance,
								   &aMute, enable)) != TM_OK,
			  errCode)
	} else {
		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxPktSetGeneralCntrl(instance,
								   Null, enable)) != TM_OK, errCode)
	}

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
    (tmInstance_t instance, Bool enable, tmdlHdmiTxIsrc1PktData_t *pIsrc1PktData) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    if (enable == True) {
		/* Check if Isrc1PktData pointer is Null */
		RETIF_SEM(dlHdmiTxItSemaphore[instance], pIsrc1PktData == Null,
			  TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

		    RETIF_SEM(dlHdmiTxItSemaphore[instance],
			      (errCode = tmbslHdmiTxPktSetIsrc1(instance,
								(tmbslHdmiTxPkt_t *) pIsrc1PktData->
								UPC_EAN_ISRC, 16,
								pIsrc1PktData->isrcCont,
								pIsrc1PktData->isrcValid,
								pIsrc1PktData->isrcStatus,
								enable)) != TM_OK, errCode)
	} else {
		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxPktSetIsrc1(instance,
							    Null, 0, 0, 0, 0, enable)) != TM_OK,
			  errCode)
	}

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
    (tmInstance_t instance, Bool enable, tmdlHdmiTxIsrc2PktData_t *pIsrc2PktData) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    if (enable == True) {
		/* Check if Isrc1PktData pointer is Null */
		RETIF_SEM(dlHdmiTxItSemaphore[instance], pIsrc2PktData == Null,
			  TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

		    RETIF_SEM(dlHdmiTxItSemaphore[instance],
			      (errCode = tmbslHdmiTxPktSetIsrc2(instance,
								(tmbslHdmiTxPkt_t *) pIsrc2PktData->
								UPC_EAN_ISRC, 16, enable)) != TM_OK,
			      errCode)
	} else {
		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxPktSetIsrc2(instance,
							    Null, 0, enable)) != TM_OK, errCode)
	}

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
    (tmInstance_t instance, Bool enable, tmdlHdmiTxMpsIfData_t *pMpsIfData) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    if (enable == True) {
		/* Check if MpsIfData pointer is Null */
		RETIF_SEM(dlHdmiTxItSemaphore[instance], pMpsIfData == Null,
			  TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

		    RETIF_SEM(dlHdmiTxItSemaphore[instance],
			      (errCode = tmbslHdmiTxPktSetMpegInfoframe(instance,
									(tmbslHdmiTxPktMpeg_t *)
									pMpsIfData,
									enable)) != TM_OK, errCode)
	} else {
		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxPktSetMpegInfoframe(instance,
								    Null, enable)) != TM_OK,
			  errCode)
	}

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
    (tmInstance_t instance, Bool enable, tmdlHdmiTxSpdIfData_t *pSpdIfData) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    if (enable == True) {
		/* Check if SpdIfData pointer is Null */
		RETIF_SEM(dlHdmiTxItSemaphore[instance], pSpdIfData == Null,
			  TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

		    RETIF_SEM(dlHdmiTxItSemaphore[instance],
			      (errCode = tmbslHdmiTxPktSetSpdInfoframe(instance,
								       (tmbslHdmiTxPktSpd_t *)
								       pSpdIfData,
								       enable)) != TM_OK, errCode)
	} else {
		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxPktSetSpdInfoframe(instance,
								   Null, enable)) != TM_OK, errCode)
	}

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
    (tmInstance_t instance, Bool enable, tmdlHdmiTxVsPktData_t *pVsIfData) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    if (enable == True) {
		/* Check if VsIfData pointer is Null */
		RETIF_SEM(dlHdmiTxItSemaphore[instance], pVsIfData == Null,
			  TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

		    RETIF_SEM(dlHdmiTxItSemaphore[instance],
			      (errCode = tmbslHdmiTxPktSetVsInfoframe(instance,
								      (tmbslHdmiTxPkt_t *)
								      pVsIfData->vsData,
								      HDMITX_PKT_DATA_BYTE_CNT - 1,
								      pVsIfData->version,
								      enable)) != TM_OK, errCode)
	} else {
		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxPktSetVsInfoframe(instance,
								  Null, 0, pVsIfData->version,
								  enable)) != TM_OK, errCode)
	}

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
tmErrorCode_t tmdlHdmiTxDebugSetNullPacket(tmInstance_t instance, Bool enable) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode = tmbslHdmiTxPktSetNullInsert(instance, enable)) != TM_OK, errCode)


	    /* Release the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
tmErrorCode_t tmdlHdmiTxDebugSetSingleNullPacket(tmInstance_t instance) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode = tmbslHdmiTxPktSetNullSingle(instance))
		      != TM_OK, errCode)

	    /* Release the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
tmErrorCode_t tmdlHdmiTxSetAudioMute(tmInstance_t instance, Bool audioMute) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Mute or Un-mute the audio output */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode = tmbslHdmiTxAudioOutSetMute(instance,
							    (tmbslHdmiTxaMute_t) audioMute)) !=
		      TM_OK, errCode)

	    /* Store current audio mute status */
	    instanceStatusInfoTx[instance].pAudioInfo->audioMuteState = audioMute;


	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
tmErrorCode_t tmdlHdmiTxResetAudioCts(tmInstance_t instance) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Reset the audio Clock Time Stamp generator */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance], (errCode = tmbslHdmiTxAudioInResetCts(instance)
		      ) != TM_OK, errCode)

	    /* Release the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
    (tmInstance_t instance, tmdlHdmiTxEdidStatus_t *pEdidStatus, UInt8 *pEdidBlkCount) {
	tmErrorCode_t errCode;
	UInt8 edidStatus;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Check if EdidStatus and pReadBytesNumber pointers are Null */
	    RETIF(pEdidStatus == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)
	    RETIF(pEdidBlkCount == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Get the EDID status from BSL driver */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode = tmbslHdmiTxEdidGetStatus(instance,
							  (UInt8 *) &edidStatus)) != TM_OK,
		      errCode)

	    if (edidStatus >= TMDL_HDMITX_EDID_STATUS_INVALID) {
		*pEdidStatus = TMDL_HDMITX_EDID_STATUS_INVALID;
	} else {
		*pEdidStatus = (tmdlHdmiTxEdidStatus_t) edidStatus;
	}

	if ((*pEdidStatus == TMDL_HDMITX_EDID_READ) || (*pEdidStatus == TMDL_HDMITX_EDID_ERROR_CHK)) {
		/* Get the read EDID block number from BSL driver */
		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxEdidGetBlockCount(instance,
								  pEdidBlkCount)) != TM_OK, errCode)
	}

	if (errCode != TM_OK) {
		/* Error during read EDID, number of read block is 0 */
		*pEdidBlkCount = 0;

		/* Release the sempahore */
		(void)tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance]);

		return errCode;
	}

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
     UInt maxAudioDescs, UInt *pWrittenAudioDescs, UInt8 *pAudioFlags) {
	tmErrorCode_t errCode;
	tmbslHdmiTxEdidSad_t edidSad[HDMI_TX_SAD_MAX_CNT];
	UInt i;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Check if AudioDescs, WrittenAudioDescs and AudioFlags pointers are Null */
	    RETIF(pAudioDescs == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)
	    RETIF(pWrittenAudioDescs == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)
	    RETIF(pAudioFlags == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Check the current state */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      dlHdmiTxGetState(instance) != STATE_EDID_AVAILABLE,
		      TMDL_ERR_DLHDMITX_INVALID_STATE)

	    /* Get video capabilities from EDID, return TMDL_ERR_DLHDMITX_NO_RESOURCES if EDID are not read */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode = tmbslHdmiTxEdidGetAudioCapabilities(instance,
								     edidSad, maxAudioDescs,
								     pWrittenAudioDescs,
								     pAudioFlags)) != TM_OK,
		      errCode)

	    for (i = 0; i < *pWrittenAudioDescs; i++) {
		pAudioDescs[i].format = (edidSad[i].ModeChans & 0x78) >> 3;	/* Bits[6:3]: EIA/CEA861 mode */
		pAudioDescs[i].channels = edidSad[i].ModeChans & 0x07;	/* Bits[2:0]: channels */
		pAudioDescs[i].supportedFreqs = edidSad[i].Freqs;	/* Supported frequencies */

		if (pAudioDescs[i].format == 1) {	/* LPCM format */
			pAudioDescs[i].supportedRes = edidSad[i].Byte3 & 0x07;
			pAudioDescs[i].maxBitrate = 0x00;
		} else if ((pAudioDescs[i].format >= 2) &&	/* Compressed format */
			   (pAudioDescs[i].format <= 8)) {
			pAudioDescs[i].supportedRes = 0x00;
			pAudioDescs[i].maxBitrate = edidSad[i].Byte3;
		} else {
			pAudioDescs[i].supportedRes = 0x00;
			pAudioDescs[i].maxBitrate = 0x00;
		}
	}

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
     UInt maxVideoFormats, UInt *pWrittenVideoFormats, UInt8 *pVideoFlags) {
	tmErrorCode_t errCode;
	UInt8 edidVFmtsBuffer[HDMI_TX_SVD_MAX_CNT];
	tmdlHdmiTxEdidVideoTimings_t edidDTDBuffer[NUMBER_DTD_STORED];
	UInt8 i;
	UInt8 writtenDTD = 0;
	UInt8 dtdCounter = 0;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Check if Videoformats, WrittenVideoFormats and VideoFlags pointers are Null */
	    RETIF(pVideoDesc == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)
	    RETIF(pWrittenVideoFormats == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)
	    RETIF(pVideoFlags == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)
	    RETIF(maxVideoFormats == 0, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Check the current state */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      dlHdmiTxGetState(instance) != STATE_EDID_AVAILABLE,
		      TMDL_ERR_DLHDMITX_INVALID_STATE)

	    /* Get video capabilities from EDID, return TMDL_ERR_DLHDMITX_NO_RESOURCES if EDID are not read */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode = tmbslHdmiTxEdidGetVideoCapabilities(instance,
								     edidVFmtsBuffer,
								     HDMI_TX_SVD_MAX_CNT,
								     pWrittenVideoFormats,
								     pVideoFlags)) != TM_OK,
		      errCode)

	    /* Get detailled descriptors from EDID, return TMDL_ERR_DLHDMITX_NO_RESOURCES if EDID are not read */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode =
		       dlHdmiTxEdidGetDTD(instance, edidDTDBuffer, NUMBER_DTD_STORED,
					  &writtenDTD)) != TM_OK, errCode)

	    dtdCounter = 0;
	if (writtenDTD > 0) {
		/* Release the sempahore */
		RETIF((errCode =
		       tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

		    /* Write first DTD in first position of table video desc  */
		    pVideoDesc[0].videoFormat =
		    tmdlHdmiTxConvertDTDtoCEA(instance, &(edidDTDBuffer[dtdCounter]));

		/* Take the sempahore */
		RETIF((errCode =
		       tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

		    dtdCounter++;

		pVideoDesc[0].nativeVideoFormat = False;
	}

	/* Start with i = 1 keep the first position for the first DTD */
	for (i = dtdCounter; i < maxVideoFormats; i++) {
		if ((i < (HDMI_TX_SVD_MAX_CNT + dtdCounter))
		    && (i < ((*pWrittenVideoFormats) + dtdCounter))) {
			/* Store SVD */
			pVideoDesc[i].videoFormat =
			    (tmdlHdmiTxVidFmt_t) ((Int) edidVFmtsBuffer[i - dtdCounter] & 0x7F);
			/* if bit 7 is true, it means that is a preferred video format */
			if ((edidVFmtsBuffer[i - dtdCounter] & 0x80) == 0x80) {
				pVideoDesc[i].nativeVideoFormat = True;
			} else {
				pVideoDesc[i].nativeVideoFormat = False;
			}
		} else {
			if ((dtdCounter < NUMBER_DTD_STORED) && (dtdCounter < writtenDTD)) {
				/* Release the sempahore */
				RETIF((errCode =
				       tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) !=
				      TM_OK, errCode)
				    /* Store DTD except first DTD */
				    pVideoDesc[i].videoFormat =
				    tmdlHdmiTxConvertDTDtoCEA(instance,
							      &(edidDTDBuffer[dtdCounter]));
				/* Take the sempahore */
				RETIF((errCode =
				       tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) !=
				      TM_OK, errCode)

				    dtdCounter++;

				pVideoDesc[i].nativeVideoFormat = False;
			} else {
				/* VGA is always supported */
				pVideoDesc[i].videoFormat = TMDL_HDMITX_VFMT_01_640x480p_60Hz;
				pVideoDesc[i].nativeVideoFormat = False;
				/* Last format supported exit from loop for */
				break;
			}
		}
	}

	*pWrittenVideoFormats = *pWrittenVideoFormats + dtdCounter + 1;	/* + 1 for VGA format */

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
    (tmInstance_t instance, tmdlHdmiTxEdidVideoTimings_t *pNativeVideoFormat) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Check if NativeVideoFormat pointer is Null */
	    RETIF(pNativeVideoFormat == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Check the current state */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      dlHdmiTxGetState(instance) != STATE_EDID_AVAILABLE,
		      TMDL_ERR_DLHDMITX_INVALID_STATE)

	    /* Get preferred video format from EDID, return TMDL_ERR_DLHDMITX_NO_RESOURCES if EDID are not read */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode = tmbslHdmiTxEdidGetVideoPreferred(instance,
								  (tmbslHdmiTxEdidDtd_t *)
								  pNativeVideoFormat)) != TM_OK,
		      errCode)

	    /* Release the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

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
     tmdlHdmiTxEdidVideoTimings_t *pDTDescriptors, UInt8 maxDTDesc, UInt8 *pWrittenDTDesc) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Check if DTDescriptors, WrittenDTDesc pointers are Null */
	    RETIF(pDTDescriptors == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)
	    RETIF(pWrittenDTDesc == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Get detailled descriptors from EDID, return TMDL_ERR_DLHDMITX_NO_RESOURCES if EDID are not read */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode =
		       dlHdmiTxEdidGetDTD(instance, pDTDescriptors, maxDTDesc,
					  pWrittenDTDesc)) != TM_OK, errCode)

	    /* Release the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

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
     tmdlHdmiTxEdidOtherMD_t *pEdidOtherMD, UInt8 maxOtherMD, UInt8 *pWrittenOtherMD) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Check if DTDescriptors, WrittenDTDesc pointers are Null */
	    RETIF(pEdidFirstMD == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)
	    RETIF(pEdidSecondMD == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)
	    RETIF(pEdidOtherMD == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Check the current state */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      dlHdmiTxGetState(instance) != STATE_EDID_AVAILABLE,
		      TMDL_ERR_DLHDMITX_INVALID_STATE)

	    /* Get monitor descriptors from EDID, return TMDL_ERR_DLHDMITX_NO_RESOURCES if EDID are not read */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode = tmbslHdmiTxEdidGetMonitorDescriptors(instance,
								      (tmbslHdmiTxEdidFirstMD_t *)
								      pEdidFirstMD,
								      (tmbslHdmiTxEdidSecondMD_t *)
								      pEdidSecondMD,
								      (tmbslHdmiTxEdidOtherMD_t *)
								      pEdidOtherMD, maxOtherMD,
								      pWrittenOtherMD)) != TM_OK,
		      errCode)

	    /* Release the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/*****************************************************************************/
/**
    \brief Retrieves TV picture ratio from receiver's EDID.
	   This function parses the EDID of Rx device to get
	   the relevant data.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance             Instance identifier.
    \param pEdidTvPictureRatio  Pointer to the array that will receive the TV picture
				ratio.

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
    (tmInstance_t instance, tmdlHdmiTxPictAspectRatio_t *pEdidTvPictureRatio) {
	tmErrorCode_t errCode;
	tmbslHdmiTxEdidBDParam_t edidBDParam;
	UInt16 horizontalSize;
	UInt16 verticalSize;
	tmbslHdmiTxEdidDtd_t edidDTDBuffer;
	UInt8 writtenDTD = 0;
	Bool bDataAvailable = False;	/* Data available in EDID for calcul TV picture ratio */

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Check if DTDescriptors, WrittenDTDesc pointers are Null */
	    RETIF(pEdidTvPictureRatio == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Check the current state */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      dlHdmiTxGetState(instance) != STATE_EDID_AVAILABLE,
		      TMDL_ERR_DLHDMITX_INVALID_STATE)

	    /* Get Basic Display Parameter from EDID, return TMDL_ERR_DLHDMITX_NO_RESOURCES if EDID are not read */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode = tmbslHdmiTxEdidGetBasicDisplayParam(instance,
								     &edidBDParam)) != TM_OK,
		      errCode)

	    horizontalSize = (UInt16) edidBDParam.uMaxHorizontalSize;
	verticalSize = (UInt16) edidBDParam.uMaxVerticalSize;

	if ((horizontalSize == 0) && (verticalSize == 0)) {
		/* Get Basic Display Parameter from EDID, return TMDL_ERR_DLHDMITX_NO_RESOURCES if EDID are not read */
		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxEdidGetDetailedTimingDescriptors
			   (instance, &edidDTDBuffer, 1, &writtenDTD)) != TM_OK, errCode);

		if (writtenDTD == 1) {
			horizontalSize = edidDTDBuffer.uHImageSize;
			verticalSize = edidDTDBuffer.uVImageSize;
			bDataAvailable = True;
		} else {
			*pEdidTvPictureRatio = TMDL_HDMITX_P_ASPECT_RATIO_UNDEFINED;
		}
	} else {
		bDataAvailable = True;
	}

	if (bDataAvailable == True) {
		*pEdidTvPictureRatio = dlHdmiTxCalcAspectRatio(horizontalSize, verticalSize);
	}

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;


}

/******************************************************************************
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
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent
	    - TMDL_ERR_DLHDMITX_INVALID_STATE: the state is invalid for
	      the function
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_RESOURCE_NOT_AVAILABLE : EDID not read
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized

******************************************************************************/
tmErrorCode_t tmdlHdmiTxGetEdidSinkType(tmInstance_t instance, tmdlHdmiTxSinkType_t *pSinkType) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Check if SinkType pointer is Null */
	    RETIF(pSinkType == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Check the current state */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      dlHdmiTxGetState(instance) != STATE_EDID_AVAILABLE,
		      TMDL_ERR_DLHDMITX_INVALID_STATE)

	    /* Read the source address from EDID, return TMDL_ERR_DLHDMITX_NO_RESOURCES if EDID are not read */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode = tmbslHdmiTxEdidGetSinkType(instance,
							    (tmbslHdmiTxSinkType_t *) pSinkType)) !=
		      TM_OK, errCode)

	    /* Release the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
tmErrorCode_t tmdlHdmiTxGetEdidSourceAddress(tmInstance_t instance, UInt16 *pSourceAddress) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Check if SourceAddress pointer is Null */
	    RETIF(pSourceAddress == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Check the current state */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      dlHdmiTxGetState(instance) != STATE_EDID_AVAILABLE,
		      TMDL_ERR_DLHDMITX_INVALID_STATE)

	    /* Read the source address from EDID, return TMDL_ERR_DLHDMITX_NO_RESOURCES if EDID are not read */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode = tmbslHdmiTxEdidGetSourceAddress(instance,
								 pSourceAddress)) != TM_OK, errCode)

	    /* Release the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
    \brief Retreives KSV list received by Tx device.
	   This function is synchronous.
	   This function is not ISR friendly.

    \param instance     Instance identifier.
    \param pKsv         Pointer to the array that will receive the KSV list.
    \param maxKsv       Maximum number of KSV that the array can store.
    \param pWrittenKsv  Actual number of KSV written into the array.
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
tmErrorCode_t tmdlHdmiTxGetKsvList
    (tmInstance_t instance,
     UInt8 *pKsv,
     UInt8 maxKsv, UInt8 *pWrittenKsv, UInt8 *pDepth, Bool *pMaxCascExd, Bool *pMaxDevsExd) {
	tmErrorCode_t errCode;
#ifndef NO_HDCP
	UInt16 i, j;
#endif

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Check if pDepth, Ksv and WrittenKsv pointers are Null */
	    RETIF(pKsv == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)
	    RETIF(pWrittenKsv == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)
	    RETIF(pDepth == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)
	    RETIF(pMaxCascExd == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)
	    RETIF(pMaxDevsExd == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

	    /* Maximum Ksv is HDMITX_KSV_LIST_MAX_DEVICES, 128 devices */
	    RETIF_BADPARAM(maxKsv > HDMITX_KSV_LIST_MAX_DEVICES)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Check the current state */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      dlHdmiTxGetState(instance) != STATE_EDID_AVAILABLE,
		      TMDL_ERR_DLHDMITX_INVALID_STATE)

	    /* Make sure that *pWrittenKsv is 0 */
	    * pWrittenKsv = 0;

	/* Make sure that *pDepth is 0 */
	*pDepth = 0;

#ifndef NO_HDCP

	*pMaxCascExd = hdcpInfoListTx[instance].hdcpMaxCascExceeded;
	*pMaxDevsExd = hdcpInfoListTx[instance].hdcpMaxDevsExceeded;


	/* Copy the bKsv */
	if (maxKsv) {

		for (j = 0; j < 5; j++) {
			pKsv[j] = hdcpInfoListTx[instance].hdcpBksv[4 - j];
		}
		*pWrittenKsv = *pWrittenKsv + 1;

	}


	/* maxKsv */
	/* Copy the Ksv list */
	for (i = 1; i <= hdcpInfoListTx[instance].hdcpKsvDevices; i++) {
		if (i < maxKsv) {
			for (j = 0; j < 5; j++) {
				pKsv[(5 * i) + j] =
				    hdcpInfoListTx[instance].hdcpKsvList[(5 * (i - 1)) + j];
			}
			*pWrittenKsv = *pWrittenKsv + 1;
		}
	}

	*pDepth = hdcpInfoListTx[instance].hdcpDeviceDepth;

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
#else
	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TMDL_ERR_DLHDMITX_NOT_SUPPORTED;
#endif				/* NO_HDCP */
}

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
tmErrorCode_t tmdlHdmiTxGetDepth(tmInstance_t instance, UInt8 *pDepth) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Check if pDepth, is Null */
	    RETIF(pDepth == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Make sure that *pDepth is 0 */
	    * pDepth = 0;

#ifndef NO_HDCP


	*pDepth = hdcpInfoListTx[instance].hdcpDeviceDepth;

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
#else
	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TMDL_ERR_DLHDMITX_NOT_SUPPORTED;
#endif				/* NO_HDCP */
}

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
tmErrorCode_t tmdlHdmiTxGeneSHA_1_IT(tmInstance_t instance) {
	tmErrorCode_t errCode;


	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    dlHdmiTxHandleSHA_1(instance);

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}
#endif				/* HDMI_TX_REPEATER_ISR_MODE */
/******************************************************************************
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
tmErrorCode_t tmdlHdmiTxSetHdcp(tmInstance_t instance, Bool hdcpEnable) {
	tmErrorCode_t errCode;
#ifndef NO_HDCP
	tmdlHdmiTxVidFmtSpecs_t resolutionSpecs;
	tmbslHdmiTxVfreq_t voutFreq;
	tmbslHdmiTxVidFmt_t voutFmt;
	tmbslHdmiTxHdcpTxMode_t txMode;
	tmbslHdmiTxHdcpOptions_t options;
	UInt8 slaveAddress;
	UInt16 i;
#endif
	tmbslHdmiTxRxSense_t rxSenseStatus;	/* Rx Sense status */
	tmbslHdmiTxHotPlug_t hpdStatus;	/* HPD status */

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Check the current state */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      dlHdmiTxGetState(instance) != STATE_EDID_AVAILABLE,
		      TMDL_ERR_DLHDMITX_INVALID_STATE)

	    /* Hdcp is not supported if keySeed is null */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      gtmdlHdmiTxDriverConfigTable[instance].keySeed == HDCP_SEED_NULL,
		      TMDL_ERR_DLHDMITX_NOT_SUPPORTED)

	    /* Read rxSenseStatus and hpdStatus to authorize HDCP only if active */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode = tmbslHdmiTxRxSenseGetStatus(instance,
							     &rxSenseStatus, False)) != TM_OK,
		      errCode)

	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode = tmbslHdmiTxHotPlugGetStatus(instance,
							     &hpdStatus, False)) != TM_OK, errCode)
#ifndef NO_HDCP
	    if (hdcpEnable == True) {	/* HDCP ON */
		if ((rxSenseStatus == HDMITX_RX_SENSE_ACTIVE)
		    && (hpdStatus == HDMITX_HOTPLUG_ACTIVE)) {

#ifdef TMFL_HDCP_OPTIMIZED_POWER
			tmbslHdmiTxHdcpPowerDown(instance, False);
#endif
			/* Reset HDCP DevLib data to ensure that new values are used */
			hdcpInfoListTx[instance].hdcpCheckState =
			    TMDL_HDMITX_HDCP_CHECK_IN_PROGRESS;
			hdcpInfoListTx[instance].hdcpErrorState = 0;
			hdcpInfoListTx[instance].hdcpKsvDevices = 0;
			hdcpInfoListTx[instance].bKsvSecure = True;
			for (i = 0; i < TMDL_HDMITX_KSV_BYTES_PER_DEVICE; i++) {
				hdcpInfoListTx[instance].hdcpBksv[i] = 0;
			}
			hdcpInfoListTx[instance].hdcpDeviceDepth = 0;

			hdcpInfoListTx[instance].hdcpMaxCascExceeded = False;
			hdcpInfoListTx[instance].hdcpMaxDevsExceeded = False;

			/* Current used video output format */
			voutFmt =
			    (tmbslHdmiTxVidFmt_t) instanceStatusInfoTx[instance].pVideoInfo->
			    videoOutConfig.format;

			/* Find output vertical frequency from output format */
			RETIF_SEM(dlHdmiTxItSemaphore[instance],
				  (errCode =
				   tmdlHdmiTxGetVideoFormatSpecs(instance,
								 (tmdlHdmiTxVidFmt_t) voutFmt,
								 &resolutionSpecs)) != TM_OK,
				  errCode)
			    voutFreq = (tmbslHdmiTxVfreq_t) resolutionSpecs.vfrequency;

			/* Configure HDCP */

			/* HDCP DDC Slave address */
			slaveAddress = HDMITX_HDCP_SLAVE_PRIMARY;

			/* Top level or repeater HDCP mode */
			if (unitTableTx[instance].repeaterEnable == True) {
				txMode = HDMITX_HDCP_TXMODE_REPEATER;
			} else {
				txMode = HDMITX_HDCP_TXMODE_TOP_LEVEL;
			}

			instanceStatusInfoTx[instance].pColBarState->changeColorBarNow = True;
			instanceStatusInfoTx[instance].pColBarState->colorBarOn = True;
			dlHdmiTxCheckColorBar(instance);

			/* HDCP options */
			options = (tmbslHdmiTxHdcpOptions_t) unitTableTx[instance].hdcpOptions;

			RETIF_SEM(dlHdmiTxItSemaphore[instance],
				  (errCode = tmbslHdmiTxHdcpConfigure(instance,
								      slaveAddress, txMode, options,
								      HDCP_CHECK_INTERVAL_MS,
								      HDCP_NUM_CHECKS)) != TM_OK,
				  errCode)

			    /* Start HDCP */
			    RETIF_SEM(dlHdmiTxItSemaphore[instance],
				      (errCode = tmbslHdmiTxHdcpInit(instance,
								     voutFmt, voutFreq)) != TM_OK,
				      errCode)

			    RETIF_SEM(dlHdmiTxItSemaphore[instance],
				      (errCode = tmbslHdmiTxHdcpRun(instance)
				      ) != TM_OK, errCode)

			    unitTableTx[instance].hdcpEnable = True;

			/* Release the sempahore */
			RETIF((errCode =
			       tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK,
			      errCode)

			    return errCode;
		}

		/* Release the sempahore */
		RETIF((errCode =
		       tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

		    return TMDL_ERR_DLHDMITX_INVALID_STATE;
	} else {		/* HDCP OFF */

		RETIF_SEM(dlHdmiTxItSemaphore[instance], (errCode = tmbslHdmiTxHdcpStop(instance)
			  ) != TM_OK, errCode)

		    unitTableTx[instance].hdcpEnable = False;

		if (dlHdmiTxGetEventStatus(instance, TMDL_HDMITX_HDCP_INACTIVE) ==
		    TMDL_HDMITX_EVENT_ENABLED) {
			/* Release the sempahore */
			(void)tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance]);
			unitTableTx[instance].pCallback(TMDL_HDMITX_HDCP_INACTIVE);
			/* Take the sempahore */
			(void)tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance]);
		}

		/* Release the sempahore */
		RETIF((errCode =
		       tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode);

#ifdef TMFL_HDCP_OPTIMIZED_POWER
		tmbslHdmiTxHdcpPowerDown(instance, True);
#endif
		return TM_OK;
	}
#else
	    /* Release the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TMDL_ERR_DLHDMITX_NOT_SUPPORTED;
#endif				/* NO_HDCP */
}

/******************************************************************************
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
tmErrorCode_t tmdlHdmiTxGetHdcpState(tmInstance_t instance, tmdlHdmiTxHdcpCheck_t *pHdcpCheckState) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Check if HdcpCheckState pointer is Null */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance], pHdcpCheckState == Null,
		      TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)
#ifndef NO_HDCP
	    /* Result of tmbslHdmiTxHdcpCheck */
	    * pHdcpCheckState = hdcpInfoListTx[instance].hdcpCheckState;

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
#else
	    * pHdcpCheckState = TMDL_HDMITX_HDCP_CHECK_NOT_STARTED;

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TMDL_ERR_DLHDMITX_NOT_SUPPORTED;
#endif				/* NO_HDCP */
}

/******************************************************************************
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
tmErrorCode_t tmdlHdmiTxHdcpCheck(tmInstance_t instance, UInt16 timeSinceLastCall) {
	tmErrorCode_t errCode;
	Bool featureSupported;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode = tmbslHdmiTxHwGetCapabilities(instance,
							      HDMITX_FEATURE_HW_HDCP,
							      &featureSupported)) != TM_OK, errCode)
#ifndef NO_HDCP
	    dlHdmiTxCheckColorBar(instance);
	dlHdmiTxCheckHdcpColorBar(instance);

	if (featureSupported == True) {
		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxHdcpCheck(instance,
							  timeSinceLastCall,
							  (tmbslHdmiTxHdcpCheck_t *) &
							  (hdcpInfoListTx[instance].
							   hdcpCheckState))) != TM_OK, errCode)
	}

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
#else
	    /* Release the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TMDL_ERR_DLHDMITX_NOT_SUPPORTED;
#endif				/* NO_HDCP */
}

/******************************************************************************
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
    (tmInstance_t instance, Bool enable, tmdlHdmiTxGamutData_t *pGamutData) {
	tmErrorCode_t errCode;
	tmbslHdmiTxPktGamut_t pkt;
	UInt8 i;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Check the current state */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      dlHdmiTxGetState(instance) != STATE_EDID_AVAILABLE,
		      TMDL_ERR_DLHDMITX_INVALID_STATE)

	    if (enable == True) {
		/* Check if GamutData pointer is Null */
		RETIF_SEM(dlHdmiTxItSemaphore[instance], pGamutData == Null,
			  TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

		    /* Fill data */
		    pkt.HB[0] = 0x0A;

		pkt.HB[1] = 0x00;
		pkt.HB[1] = (UInt8) (pkt.HB[1] | ((pGamutData->nextField & 0x01) << 7));
		pkt.HB[1] |= (pGamutData->GBD_Profile & 0x07) << 4;
		pkt.HB[1] |= (pGamutData->affectedGamutSeqNum & 0x0F);

		pkt.HB[2] = 0x00;
		pkt.HB[2] = (UInt8) (pkt.HB[2] | ((pGamutData->noCurrentGBD & 0x01) << 7));
		pkt.HB[2] |= (pGamutData->packetSequence & 0x03) << 4;
		pkt.HB[2] |= (pGamutData->currentGamutSeqNum & 0x0F);

		for (i = 0; i < 28; i++) {
			pkt.PB[i] = pGamutData->payload[i];
		}

		/* Store GBD color space */
		if (((pGamutData->payload[0]) & 0x03) == 2) {
			instanceStatusInfoTx[instance].pGamutState->wideGamutColorSpace =
			    TMDL_HDMITX_EXT_COLORIMETRY_XVYCC709;
		} else {
			instanceStatusInfoTx[instance].pGamutState->wideGamutColorSpace =
			    TMDL_HDMITX_EXT_COLORIMETRY_XVYCC601;
		}

		/* Fill Gamut metadata packet */
		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxPktFillGamut(instance,
							     &pkt,
							     instanceStatusInfoTx[instance].
							     pGamutState->gamutBufNum)) != TM_OK,
			  errCode)

		    /* Enable Gamut metadata transmission */
		    RETIF_SEM(dlHdmiTxItSemaphore[instance],
			      (errCode = tmbslHdmiTxPktSendGamut(instance,
								 instanceStatusInfoTx[instance].
								 pGamutState->gamutBufNum,
								 enable)) != TM_OK, errCode)

		    /* Use next buffer for next time */
		    if (instanceStatusInfoTx[instance].pGamutState->gamutBufNum == 0) {
			instanceStatusInfoTx[instance].pGamutState->gamutBufNum = 1;
		} else {
			instanceStatusInfoTx[instance].pGamutState->gamutBufNum = 0;
		}
	} else {
		/* Disable Gamut metadata transmission */
		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxPktSendGamut(instance,
							     0, enable)) != TM_OK, errCode)
	}

	/* Store gamut status */
	instanceStatusInfoTx[instance].pGamutState->gamutOn = enable;
	if (enable)
		instanceStatusInfoTx[instance].pGamutState->extColOn = False;

	/* Set avi infoframe */
	RETIF_SEM(dlHdmiTxItSemaphore[instance],
		  (errCode = dlHdmiTxSetVideoInfoframe(instance,
						       instanceStatusInfoTx[instance].pVideoInfo->
						       videoOutConfig.format,
						       instanceStatusInfoTx[instance].pVideoInfo->
						       videoOutConfig.mode)) != TM_OK, errCode)

	    /* Release the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
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
     tmdlHdmiTxYCCQR_t yccQR, tmdlHdmiTxGamutData_t *pGamutData) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Check if extendedColorimetry & yccQR values are in the correct range */
	    if (enable)		/* no need to check them for disable handling */
	{
		RETIF(extendedColorimetry >= TMDL_HDMITX_EXT_COLORIMETRY_INVALID,
		      TMDL_ERR_DLHDMITX_BAD_PARAMETER)
		    RETIF(yccQR >= TMDL_HDMITX_YQR_INVALID, TMDL_ERR_DLHDMITX_BAD_PARAMETER)
	}

	/* Take the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Check the current state */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      dlHdmiTxGetState(instance) != STATE_EDID_AVAILABLE,
		      TMDL_ERR_DLHDMITX_INVALID_STATE)

	    /* Store YCC quantisation range value for later AVI InfoFrame insertion */
	    if (enable)
		instanceStatusInfoTx[instance].pGamutState->yccQR = yccQR;

	/* Extended colorimetries that need to send Gamut Packet Data */
	if (((enable == True) && ((extendedColorimetry == TMDL_HDMITX_EXT_COLORIMETRY_XVYCC601) ||
				  (extendedColorimetry == TMDL_HDMITX_EXT_COLORIMETRY_XVYCC709)))
	    || ((enable == False) && (instanceStatusInfoTx[instance].pGamutState->gamutOn == True))) {
		/* can not have two different types of extended colorimeties enabled in the same time */
		if (enable)
			instanceStatusInfoTx[instance].pGamutState->extColOn = False;

		/* Release the sempahore */
		RETIF((errCode =
		       tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

		    /* Call the API that handles Gamut MetaData */
		    RETIF((errCode =
			   tmdlHdmiTxSetGamutPacket(instance, enable, pGamutData)) != TM_OK,
			  errCode)

		    return TM_OK;
	}

	/* Extended colorimetries that do not need to send Gamut Packet Data */
	if (instanceStatusInfoTx[instance].pGamutState->gamutOn == True) {
		/* Disable Gamut metadata transmission */
		RETIF_SEM(dlHdmiTxItSemaphore[instance],
			  (errCode = tmbslHdmiTxPktSendGamut(instance, 0, False)) != TM_OK, errCode)

		    instanceStatusInfoTx[instance].pGamutState->gamutOn = False;
	}

	/* Store the extended colorimetry that does not need sending Gamut Packet Data */
	if (enable)
		instanceStatusInfoTx[instance].pGamutState->wideGamutColorSpace =
		    extendedColorimetry;
	instanceStatusInfoTx[instance].pGamutState->extColOn = enable;

	/* Set avi infoframe */
	RETIF_SEM(dlHdmiTxItSemaphore[instance],
		  (errCode = dlHdmiTxSetVideoInfoframe(instance,
						       instanceStatusInfoTx[instance].pVideoInfo->
						       videoOutConfig.format,
						       instanceStatusInfoTx[instance].pVideoInfo->
						       videoOutConfig.mode)) != TM_OK, errCode)

	    /* Release the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}

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

tmErrorCode_t tmdlHdmiTxSetHDCPRevocationList(tmInstance_t instance, void *listPtr, UInt32 length)
{
	tmErrorCode_t errCode = TM_OK;
#ifndef NO_HDCP
	tmErrorCode_t errCodeSem = TM_OK;
	UInt8 aCounter = 0;
	UInt8 indexKSVList = 0;
	UInt8 i;
	Bool bIsSecure = True;
#endif

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Check parameters */
	    RETIF((listPtr == Null) || (length == 0), TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)
#ifndef NO_HDCP
	    /* --------------------- */
	    /* Take the semaphore    */
	    /* --------------------- */
	    RETIF((errCodeSem =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCodeSem)

	    /* Register revocation list */
	    unitTableTx[instance].revocationList.pList = (UInt8 *) listPtr;
	unitTableTx[instance].revocationList.length = length;

	/* Look if hdcpBksv is filled in */
	for (i = 0; i < TMDL_HDMITX_KSV_BYTES_PER_DEVICE; i++) {
		if (hdcpInfoListTx[instance].hdcpBksv[i] == 0)
			aCounter++;
	}

	/* If it the case ,check bksv */
	if (aCounter != TMDL_HDMITX_KSV_BYTES_PER_DEVICE) {

		dlHdmiTxCheckHdcpBksv(instance, hdcpInfoListTx[instance].hdcpBksv, &bIsSecure,
				      True);

		/* bksv is secure */
		if (bIsSecure == True) {

			/* if HDMI TX is at top level */
			if (unitTableTx[instance].repeaterEnable == False) {

				/* if present, check ksv list */
				if (hdcpInfoListTx[instance].hdcpKsvDevices) {

					while ((indexKSVList < TMDL_HDMITX_KSV_LIST_MAX_DEVICES) &&
					       (indexKSVList <
						hdcpInfoListTx[instance].hdcpKsvDevices)
					       && (bIsSecure == True)) {

						dlHdmiTxCheckHdcpBksv(instance,
								      &(hdcpInfoListTx[instance].
									hdcpKsvList[indexKSVList *
										    TMDL_HDMITX_KSV_BYTES_PER_DEVICE]),
								      &bIsSecure, False);
						indexKSVList++;
					}

					if (bIsSecure == True) {
						errCode = TMDL_DLHDMITX_HDCP_SECURE;
					} else {
						errCode = TMDL_DLHDMITX_HDCP_NOT_SECURE;
					}
				} else {	/* ksv list does NOT exist */

					/* we suppose that application calls the API after RX_KEYS_RECEIVED */
					errCode = TMDL_DLHDMITX_HDCP_SECURE;
				}

			} else {	/* we are a repeater */
				errCode = TMDL_ERR_DLHDMITX_INVALID_STATE;
			}

		} else {	/* bksv NOT secure */

			errCode = TMDL_DLHDMITX_HDCP_NOT_SECURE;
		}

	} else {
		/* bksv is not read, could not be tested */
		errCode = TM_OK;
	}

	/* --------------------------- */
	/* Release the sempahore */
	/* --------------------------- */
	RETIF((errCodeSem =
	       tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCodeSem)
#else
	    (void)instance;	/* Remove compiler warning */
#endif				/* NO_HDCP */
	return errCode;
}

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

tmErrorCode_t tmdlHdmiTxSetBScreen(tmInstance_t instance, tmdlHdmiTxTestPattern_t pattern)
{

	tmErrorCode_t errCodeSem = TM_OK;
	tmErrorCode_t errCode = TM_OK;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* --------------------- */
	    /* Take the semaphore    */
	    /* --------------------- */
	    RETIF((errCodeSem =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCodeSem)

	    gtmdlHdmiTxDriverConfigTable[instance].pattern = pattern;

	/* Set service mode colour bar on/off (also used as HDCP logo pattern) */
	(void)dlHdmiTxSetTestPatternOn(instance,
				       instanceStatusInfoTx[instance].pVideoInfo->videoOutConfig.
				       format,
				       instanceStatusInfoTx[instance].pVideoInfo->videoOutConfig.
				       mode, gtmdlHdmiTxDriverConfigTable[instance].pattern);

	/* --------------------------- */
	/* Release the sempahore */
	/* --------------------------- */
	RETIF((errCodeSem =
	       tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCodeSem)

	    return errCode;

}

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

tmErrorCode_t tmdlHdmiTxRemoveBScreen(tmInstance_t instance)
{

	tmErrorCode_t errCodeSem = TM_OK;
	tmErrorCode_t errCode = TM_OK;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* --------------------- */
	    /* Take the semaphore    */
	    /* --------------------- */
	    RETIF((errCodeSem =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCodeSem)

	    /* Restore last output format and mode */
	    (void)dlHdmiTxSetTestPatternOff(instance,
					    instanceStatusInfoTx[instance].pVideoInfo->
					    videoOutConfig.format,
					    instanceStatusInfoTx[instance].pVideoInfo->
					    videoOutConfig.mode);

	/* --------------------------- */
	/* Release the sempahore */
	/* --------------------------- */
	RETIF((errCodeSem =
	       tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCodeSem)

	    return errCode;

}

/******************************************************************************
    \brief tmdlHdmiTxConvertDTDtoCEA .

    \param DTDescriptors     DTD to convert.

    \return NA.

******************************************************************************/
tmdlHdmiTxVidFmt_t tmdlHdmiTxConvertDTDtoCEA
    (tmInstance_t instance, tmdlHdmiTxEdidVideoTimings_t *pDTDescriptors) {

	tmdlHdmiTxVidFmt_t codeCEA;
	tmdlHdmiTxPictAspectRatio_t pictureAspectRatio;
	Bool formatInterlaced;

	/* --------------------- */
	/* Take the semaphore    */
	/* --------------------- */
	tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance]);

	formatInterlaced = False;

	if ((pDTDescriptors->flags) & 0x80) {
		formatInterlaced = True;
	}

	pictureAspectRatio =
	    dlHdmiTxCalcAspectRatio(pDTDescriptors->hImageSize, pDTDescriptors->vImageSize);

	switch (pDTDescriptors->hActivePixels) {
	case 640:
		codeCEA = dlHdmiTxConvertDTDtoCEA_640HAP(pDTDescriptors);
		break;

	case 720:
		codeCEA = dlHdmiTxConvertDTDtoCEA_720HAP(pDTDescriptors, pictureAspectRatio);
		break;

	case 1280:
		codeCEA = dlHdmiTxConvertDTDtoCEA_1280HAP(pDTDescriptors);
		break;

	case 1920:
		codeCEA = dlHdmiTxConvertDTDtoCEA_1920HAP(pDTDescriptors, formatInterlaced);
		break;

	case 1440:
		codeCEA =
		    dlHdmiTxConvertDTDtoCEA_1440HAP(pDTDescriptors, pictureAspectRatio,
						    formatInterlaced);
		break;

	case 2880:
		codeCEA =
		    dlHdmiTxConvertDTDtoCEA_2880HAP(pDTDescriptors, pictureAspectRatio,
						    formatInterlaced);
		break;

	default:
		/* Not a valid format */
		codeCEA = TMDL_HDMITX_VFMT_NULL;
		break;
	}

	/* --------------------------- */
	/* Release the sempahore */
	/* --------------------------- */
	tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance]);

	return codeCEA;

}

/*============================================================================*/
/*                           INTERNAL FUNCTION                                */
/*============================================================================*/

/******************************************************************************
    \brief Get the REFPIX and REFLINE for output and scaler
	    for the current settings.

    \param vinFmt       Video input format.
    \param vinMode      Video input mode.
    \param voutFmt      Video output format.
    \param syncIn       Type of synchro (ext or emb).
    \param pixRate      Video pixel rate.
    \param pRefPix      RefPix for output.
    \param pRefLine     RefLine for output.
    \param pScRefPix    RefPix for scaler.
    \param pScRefLine   RefLine for scaler.
    \param pbVerified   Pointer to the boolean that will receive the fact that
			this scaler setting was verified.

    \return True (Found) or False (Not found).

******************************************************************************/
static Bool dlHdmiTxGetReflineRefpix
    (tmdlHdmiTxVidFmt_t vinFmt,
     tmdlHdmiTxVinMode_t vinMode,
     tmdlHdmiTxVidFmt_t voutFmt,
     UInt8 syncIn,
     tmdlHdmiTxPixRate_t pixRate,
     UInt16 *pRefPix,
     UInt16 *pRefLine, UInt16 *pScRefPix, UInt16 *pScRefLine, Bool *pbVerified) {
	UInt8 shortVinFmt;
	UInt8 shortVoutFmt;
	int i;
	Bool bFound;
	tmdlHdmiTxVidFmt_t vinFmtIndex, voutFmtIndex;

	/* Search for all values to match in table, until table end is reached
	 * when both refPix values are zero */
	*pRefPix = 0;
	*pRefLine = 0;
	*pScRefPix = 0;
	*pScRefLine = 0;

	/* If match is not found in table, we can assume a verified non-scaler
	 * combination */
	*pbVerified = 1;
	bFound = False;

	if ((voutFmt < TMDL_HDMITX_VFMT_TV_NO_REG_MIN)
	    || ((voutFmt >= HDMITX_VFMT_35_2880x480p_60Hz)
		&& (voutFmt <= HDMITX_VFMT_38_2880x576p_50Hz))) {
		vinFmtIndex = dlHdmiTxCalcVidFmtIndex(vinFmt);
		voutFmtIndex = dlHdmiTxCalcVidFmtIndex(voutFmt);
		shortVinFmt = kVfmtToShortFmt_TV[vinFmtIndex];
		shortVoutFmt = kVfmtToShortFmt_TV[voutFmtIndex];

		for (i = 0; kRefpixRefline[i].shortVinFmt != TV_INVALID; i++) {
			if ((kRefpixRefline[i].shortVinFmt == shortVinFmt)
			    && (UNPKMODE(kRefpixRefline[i].modeRateSyncVerf) == vinMode)
			    && (kRefpixRefline[i].shortVoutFmt == shortVoutFmt)
			    && (UNPKRATE(kRefpixRefline[i].modeRateSyncVerf) == pixRate)
			    && (UNPKSYNC(kRefpixRefline[i].modeRateSyncVerf) == syncIn)) {
				*pRefPix = kRefpixRefline[i].refPix;
				*pRefLine = kRefpixRefline[i].refLine;
				*pScRefPix = kRefpixRefline[i].scRefPix;
				*pScRefLine = kRefpixRefline[i].scRefLine;
				*pbVerified = UNPKVERF(kRefpixRefline[i].modeRateSyncVerf);
				bFound = True;
				break;
			}
		}
	}

	return bFound;
}

/******************************************************************************
    \brief Set the video infoframe.

    \param instance     Instance identifier.
    \param voutFmt      Video output format.
    \param voutMode     Video output mode.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
	    - TMBSL_ERR_HDMI_I2C_READ: failed when reading to the I2C bus
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized

******************************************************************************/
static tmErrorCode_t dlHdmiTxSetVideoInfoframe
    (tmInstance_t instance, tmdlHdmiTxVidFmt_t voutFmt, tmdlHdmiTxVoutMode_t voutMode) {
	tmErrorCode_t errCode;
	tmdlHdmiTxAviIfData_t contentVif;
	tmdlHdmiTxVidFmt_t voutFmtIndex;


	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    contentVif.colorIndicator = voutMode;	/* 3rd api_set_avi_infoframe param */
	contentVif.activeInfoPresent = 0;
	contentVif.barInformationDataValid = 0;
	contentVif.scanInformation = 0;

	voutFmtIndex = dlHdmiTxCalcVidFmtIndex(voutFmt);
	contentVif.pictureAspectRatio = kVfmtToAspect_TV[voutFmtIndex];

	contentVif.activeFormatAspectRatio = 8;
	contentVif.nonUniformPictureScaling = 0;

#ifdef FORMAT_PC
	if (voutFmt >= TMDL_HDMITX_VFMT_PC_MIN) {
		if (voutFmt == TMDL_HDMITX_VFMT_PC_640x480p_60Hz) {
			contentVif.videoFormatIdentificationCode =
			    (tmbslHdmiTxVidFmt_t) TMDL_HDMITX_VFMT_01_640x480p_60Hz;
		} else {
			/* Format PC not Valid in EIA861b */
			contentVif.videoFormatIdentificationCode =
			    (tmbslHdmiTxVidFmt_t) TMDL_HDMITX_VFMT_NULL;
		}
	} else {
#endif				/* FORMAT_PC */

		contentVif.videoFormatIdentificationCode = (tmbslHdmiTxVidFmt_t) voutFmt;

#ifdef FORMAT_PC
	}
#endif				/* FORMAT_PC */


	if (((voutFmt >= TMDL_HDMITX_VFMT_06_720x480i_60Hz)
	     && (voutFmt <= TMDL_HDMITX_VFMT_09_720x240p_60Hz))
	    || ((voutFmt >= TMDL_HDMITX_VFMT_21_720x576i_50Hz)
		&& (voutFmt <= TMDL_HDMITX_VFMT_24_720x288p_50Hz))) {
		/* Force pixel repeat for formats where it's mandatory (Pixel Frequency < 20 Mpix/s) */
		contentVif.pixelRepetitionFactor = 1;
	} else if ((voutFmt == TMDL_HDMITX_VFMT_10_720x480i_60Hz)
		   || (voutFmt == TMDL_HDMITX_VFMT_11_720x480i_60Hz)
		   || (voutFmt == TMDL_HDMITX_VFMT_25_720x576i_50Hz)
		   || (voutFmt == TMDL_HDMITX_VFMT_26_720x576i_50Hz)) {
		contentVif.pixelRepetitionFactor = HDMITX_PIXREP_3;	/* pixel sent 1 or 10 times, here 4 times */
	} else if ((voutFmt == TMDL_HDMITX_VFMT_14_1440x480p_60Hz)
		   || (voutFmt == TMDL_HDMITX_VFMT_15_1440x480p_60Hz)
		   || (voutFmt == TMDL_HDMITX_VFMT_29_1440x576p_50Hz)
		   || (voutFmt == TMDL_HDMITX_VFMT_30_1440x576p_50Hz)) {
		contentVif.pixelRepetitionFactor = HDMITX_PIXREP_1;	/* pixel sent 1 or 2 times, here 2 times */
	} else if ((voutFmt >= TMDL_HDMITX_VFMT_35_2880x480p_60Hz)
		   && (voutFmt <= TMDL_HDMITX_VFMT_38_2880x576p_50Hz)) {
		contentVif.pixelRepetitionFactor = HDMITX_PIXREP_3;
	} else {		/* Default to no repeat for all other formats */

		contentVif.pixelRepetitionFactor = HDMITX_PIXREP_NONE;
	}

	if ((instanceStatusInfoTx[instance].pGamutState->gamutOn == True) ||
	    (instanceStatusInfoTx[instance].pGamutState->extColOn == True)) {
		contentVif.colorimetry = (UInt8) TMDL_HDMITX_COLORIMETRY_EXTENDED;
	} else {
		switch (voutFmt) {
		case TMDL_HDMITX_VFMT_04_1280x720p_60Hz:
		case TMDL_HDMITX_VFMT_05_1920x1080i_60Hz:
		case TMDL_HDMITX_VFMT_16_1920x1080p_60Hz:
		case TMDL_HDMITX_VFMT_19_1280x720p_50Hz:
		case TMDL_HDMITX_VFMT_20_1920x1080i_50Hz:
		case TMDL_HDMITX_VFMT_31_1920x1080p_50Hz:
			contentVif.colorimetry = (UInt8) TMDL_HDMITX_COLORIMETRY_ITU709;
			break;

		default:
			contentVif.colorimetry = (UInt8) TMDL_HDMITX_COLORIMETRY_ITU601;
			break;
		}
	}

	contentVif.lineNumberEndTopBar = 0;
	contentVif.lineNumberStartBottomBar = 0;
	contentVif.lineNumberEndLeftBar = 0;
	contentVif.lineNumberStartRightBar = 0;

	errCode = dlHdmiTxSetRawVideoInfoframe(instance, &contentVif, True);

	/* Ignore infoframe interlock in DVI mode */
	if (errCode == TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED) {
		errCode = TM_OK;
	}

	return errCode;
}

/******************************************************************************
    \brief Set the video infoframe.

    \param instance     Instance identifier.
    \param voutFmt      Video output format.
    \param voutMode     Video output mode.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
	    - TMBSL_ERR_HDMI_I2C_READ: failed when reading to the I2C bus
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
	    - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

******************************************************************************/
static tmErrorCode_t dlHdmiTxSetRawVideoInfoframe
    (tmInstance_t instance, tmdlHdmiTxAviIfData_t *pContentVif, Bool enable) {

	tmErrorCode_t errCode;
	tmbslHdmiTxPktRawAvi_t PktInfoFrame;
	UInt8 i;

	if (pContentVif != Null) {

		for (i = 0; i < sizeof(PktInfoFrame.PB); i++) {
			PktInfoFrame.PB[i] = 0;
		}

		/* Prepare VIF header */
		PktInfoFrame.HB[0] = 0x82;	/* Video InfoFrame */
		PktInfoFrame.HB[1] = 0x02;	/* Version 2 [HDMI 1.2] */
		PktInfoFrame.HB[2] = 0x0D;	/* Length [HDMI 1.2] */

		/* Prepare VIF packet (byte numbers offset by 3) */
		PktInfoFrame.PB[0] = 0;	/* Preset checksum to zero so calculation works! */
		PktInfoFrame.PB[1] = ((pContentVif->colorIndicator & 0x03) << 5) |	/* Y1-0, B1-0,S1-0 */
		    ((pContentVif->barInformationDataValid & 0x03) << 2) |
		    (pContentVif->scanInformation & 0x03);
		if (pContentVif->activeInfoPresent == True) {
			PktInfoFrame.PB[1] += 0x10;	/* A0 bit */
		}

		PktInfoFrame.PB[2] = ((pContentVif->colorimetry & 0x03) << 6) |	/* C1-0, M1-0, R3-0 */
		    ((pContentVif->pictureAspectRatio & 0x03) << 4) |
		    (pContentVif->activeFormatAspectRatio & 0x0F);

		PktInfoFrame.PB[3] = (pContentVif->nonUniformPictureScaling & 0x03);	/* SC1-0 *//* [HDMI 1.2] */

		/* Q1-0 = 00 => RGB Quantization Range depends on video format (CEA-861) */
		/* Limited Range for all video formats except PC formats which requires Full Range */

		if (pContentVif->colorimetry == TMDL_HDMITX_COLORIMETRY_EXTENDED) {
			PktInfoFrame.PB[3] =
			    ((((UInt8) instanceStatusInfoTx[instance].pGamutState->
			       wideGamutColorSpace) & 0x07) << 4)
			    | PktInfoFrame.PB[3];
		}

		/* Bit ITC = 0 => No Content Type ; Bit ITC = 1 => Content Type (see CN1-0) */
		/* Today ITC = 0 => No Content Type */

		PktInfoFrame.PB[4] = (pContentVif->videoFormatIdentificationCode & 0x7F);	/* VIC6-0 */

		PktInfoFrame.PB[5] = (pContentVif->pixelRepetitionFactor & 0x0F);	/* PR3-0 */

		/* CN1-0 => Content Type */
		/* Today CN1-0 = 00 => No Data */

		/* YQ1-0 => YCC Quantization Range, only managed for those extended colorimetries */
		if (pContentVif->colorimetry == TMDL_HDMITX_COLORIMETRY_EXTENDED) {
			PktInfoFrame.PB[5] |=
			    (((UInt8) instanceStatusInfoTx[instance].pGamutState->
			      yccQR) & 0x03) << 6;
		}
		PktInfoFrame.PB[6] = (UInt8) (pContentVif->lineNumberEndTopBar & 0x00FF);
		PktInfoFrame.PB[7] = (UInt8) ((pContentVif->lineNumberEndTopBar & 0xFF00) >> 8);
		PktInfoFrame.PB[8] = (UInt8) (pContentVif->lineNumberStartBottomBar & 0x00FF);
		PktInfoFrame.PB[9] =
		    (UInt8) ((pContentVif->lineNumberStartBottomBar & 0xFF00) >> 8);
		PktInfoFrame.PB[10] = (UInt8) (pContentVif->lineNumberEndLeftBar & 0x00FF);
		PktInfoFrame.PB[11] = (UInt8) ((pContentVif->lineNumberEndLeftBar & 0xFF00) >> 8);
		PktInfoFrame.PB[12] = (UInt8) (pContentVif->lineNumberStartRightBar & 0x00FF);
		PktInfoFrame.PB[13] =
		    (UInt8) ((pContentVif->lineNumberStartRightBar & 0xFF00) >> 8);

		/* Calculate checksum - this is worked out on "Length" bytes of the
		 * packet, the checksum (which we've preset to zero), and the three
		 * header bytes.
		 */
		PktInfoFrame.PB[0] = dlHdmiTxcalculateCheksumIF(&PktInfoFrame);

		errCode = tmbslHdmiTxPktSetRawVideoInfoframe(instance, &PktInfoFrame, enable);
	} else {
		errCode = tmbslHdmiTxPktSetVideoInfoframe(instance, Null, enable);
	}

	return errCode;

}

/*============================================================================*/
/* calculateChecksum - returns the byte needed to yield a checksum of zero    */
/*============================================================================*/
static UInt8 dlHdmiTxcalculateCheksumIF(tmbslHdmiTxPktRawAvi_t *pData	/* Pointer to checksum data */
    ) {
	UInt8 checksum = 0;	/* Working checksum calculation */
	UInt8 result = 0;	/* Value to be returned */
	UInt8 numBytes = 0;
	Int i;

	if (pData != Null) {

		numBytes = sizeof(pData->HB);

		for (i = 0; i < numBytes; i++) {
			checksum = checksum + pData->HB[i];
		}

		numBytes = sizeof(pData->PB);

		for (i = 0; i < numBytes; i++) {
			checksum = checksum + pData->PB[i];
		}

		result = (UInt8) ((255 - checksum) + 1);
	}
	return result;		/* returns 0 in the case of null ptr or 0 bytes */
}

/******************************************************************************
    \brief Set colourbar test pattern on with RGB infoframe

    \param instance     Instance identifier.
    \param voutFmt      Video output format.
    \param voutMode     Video output mode.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus

******************************************************************************/
tmErrorCode_t dlHdmiTxSetTestPatternOn
    (tmInstance_t instance,
     tmdlHdmiTxVidFmt_t voutFmt, tmdlHdmiTxVoutMode_t voutMode, tmdlHdmiTxTestPattern_t pattern) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    RETIF((errCode = tmbslHdmiTxTestSetPattern(instance,
						       (tmbslHdmiTxTestPattern_t) pattern)) !=
		  TM_OK, errCode)

	    if (pattern > TMDL_HDMITX_PATTERN_CBAR8) {
		RETIF((errCode =
		       dlHdmiTxSetVideoInfoframe(instance, voutFmt, voutMode)) != TM_OK, errCode)
	} else {
		/* For TMDL_HDMITX_PATTERN_CBAR8 and TMDL_HDMITX_PATTERN_CBAR4, video mode in infoframe should be RGB */
		RETIF((errCode =
		       dlHdmiTxSetVideoInfoframe(instance, voutFmt,
						 TMDL_HDMITX_VOUTMODE_RGB444)) != TM_OK, errCode)
	}
	return TM_OK;
}

/******************************************************************************
    \brief Set colourbar test pattern off with previous infoframe

    \param instance     Instance identifier.
    \param voutFmt      Video output format.
    \param voutMode     Video output mode.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_INSTANCE: the instance number is wrong or
	      out of range
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
	    - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus

******************************************************************************/
tmErrorCode_t dlHdmiTxSetTestPatternOff
    (tmInstance_t instance, tmdlHdmiTxVidFmt_t voutFmt, tmdlHdmiTxVoutMode_t voutMode) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    RETIF((errCode = tmbslHdmiTxTestSetPattern(instance,
						       (tmbslHdmiTxTestPattern_t)
						       TMDL_HDMITX_PATTERN_OFF)) != TM_OK, errCode)

	    /* Restore video infoframe */
	    RETIF((errCode =
		   dlHdmiTxSetVideoInfoframe(instance, voutFmt, voutMode)) != TM_OK, errCode)

	    return TM_OK;
}

/******************************************************************************
    \brief HDCP ENCRYPT interrupt callback.

    \param instance     Instance identifier.

    \return NA.

******************************************************************************/
static void dlHdmiTxHandleENCRYPT(tmInstance_t instance) {
#ifndef NO_HDCP

	tmbslHdmiTxHdcpHandleENCRYPT(instance);

	if (instanceStatusInfoTx[instance].pColBarState->disableColorBarOnR0 == False) {
		instanceStatusInfoTx[instance].pColBarState->hdcpColbarChange = False;
		instanceStatusInfoTx[instance].pColBarState->hdcpEncryptOrT0 = True;
	}
	instanceStatusInfoTx[instance].pColBarState->disableColorBarOnR0 = False;
#else
	(void)instance;		/* Remove compiler warning */
#endif				/* NO_HDCP */
}

/******************************************************************************
    \brief HPD interrupt callback.

    \param instance     Instance identifier.

    \return NA.

******************************************************************************/
static void dlHdmiTxHandleHPD(tmInstance_t instance) {
	tmErrorCode_t errCode;
	tmbslHdmiTxHotPlug_t hpdStatus;	/* HPD status */
	tmPowerState_t powerState;	/* Power state of transmitter */

	hpdStatus = HDMITX_HOTPLUG_INVALID;

	/* Get Hot Plug status */
	errCode = tmbslHdmiTxHotPlugGetStatus(instance, &hpdStatus, False);

	if (errCode != TM_OK)
		return;

	/* Get the power state of the transmitter */
	errCode = tmbslHdmiTxPowerGetState(instance, &powerState);

	if (errCode != TM_OK)
		return;

	/* Has hot plug changed to Active? */
	if (hpdStatus == HDMITX_HOTPLUG_ACTIVE) {
		/* Set state machine to Plugged */
		dlHdmiTxSetState(instance, STATE_PLUGGED);

		if (dlHdmiTxGetEventStatus(instance, TMDL_HDMITX_HPD_ACTIVE) ==
		    TMDL_HDMITX_EVENT_ENABLED) {
			/* Release the sempahore */
			(void)tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance]);
			unitTableTx[instance].pCallback(TMDL_HDMITX_HPD_ACTIVE);
			/* Take the sempahore */
			(void)tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance]);
		}

		if (powerState == tmPowerOn) {
			/* Yes: Wait for DDC line to settle before reading EDID */
			tmbslHdmiTxSysTimerWait(instance, 500);	/* ms */

			/* Request EDID read */
			errCode = tmbslHdmiTxEdidRequestBlockData(instance,
								  unitTableTx[instance].pEdidBuffer,
								  (Int) ((unitTableTx[instance].
									  edidBufferSize) >> 7),
								  (Int) (unitTableTx[instance].
									 edidBufferSize));

			if (errCode != TM_OK)
				return;
		}
	} else {
#ifndef NO_HDCP
		if (unitTableTx[instance].hdcpEnable == True) {
			/* Release the sempahore */
			(void)tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance]);
			/* Switch off HDCP */
			(void)tmdlHdmiTxSetHdcp(instance, False);
			/* Take the sempahore */
			(void)tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance]);
		}
#endif				/* NO_HDCP */

		/* Set state machine to Unplugged */
		dlHdmiTxSetState(instance, STATE_UNPLUGGED);

		if (dlHdmiTxGetEventStatus(instance, TMDL_HDMITX_HPD_INACTIVE) ==
		    TMDL_HDMITX_EVENT_ENABLED) {
			/* Release the sempahore */
			(void)tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance]);
			unitTableTx[instance].pCallback(TMDL_HDMITX_HPD_INACTIVE);
			/* Take the sempahore */
			(void)tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance]);
		}
	}
}

/******************************************************************************
    \brief T0 interrupt callback.

    \param instance     Instance identifier.

    \return NA.

******************************************************************************/
static void dlHdmiTxHandleT0(tmInstance_t instance) {
#ifndef NO_HDCP
	tmErrorCode_t errCode;

	errCode = tmbslHdmiTxHdcpHandleT0(instance);

	if (errCode != TM_OK)
		return;

	tmbslHdmiTxHdcpGetT0FailState(instance, &(hdcpInfoListTx[instance].hdcpErrorState));

	if (dlHdmiTxGetEventStatus(instance, TMDL_HDMITX_HDCP_INACTIVE) ==
	    TMDL_HDMITX_EVENT_ENABLED) {
		/* Release the sempahore */
		(void)tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance]);
		unitTableTx[instance].pCallback(TMDL_HDMITX_HDCP_INACTIVE);
		/* Take the sempahore */
		(void)tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance]);
	}

	instanceStatusInfoTx[instance].pColBarState->hdcpColbarChange = False;
	instanceStatusInfoTx[instance].pColBarState->hdcpEncryptOrT0 = True;
	instanceStatusInfoTx[instance].pColBarState->hdcpSecureOrT0 = True;
#else
	(void)instance;
#endif				/* NO_HDCP */
}

/******************************************************************************
    \brief BCAPS interrupt callback.

    \param instance     Instance identifier.

    \return NA.

******************************************************************************/
static void dlHdmiTxHandleBCAPS(tmInstance_t instance) {
#ifndef NO_HDCP
	Bool bCheckRequired;
	tmErrorCode_t errCode;

	/* Handle BCAPS interrupt immediately */
	errCode = tmbslHdmiTxHdcpHandleBCAPS(instance);

	if (errCode != TM_OK)
		return;

	/* Wait for TDA9984 to read BKSV from B device */
	tmbslHdmiTxSysTimerWait(instance, 10);

	/* Handle BKSV read */
	errCode = tmbslHdmiTxHdcpHandleBKSV(instance,
					    hdcpInfoListTx[instance].hdcpBksv, &bCheckRequired);

	if (errCode != TM_OK)
		return;

	if (bCheckRequired) {
		/* check HdcpBksv against a revocation list */
		dlHdmiTxCheckHdcpBksv(instance, hdcpInfoListTx[instance].hdcpBksv,
				      &(hdcpInfoListTx[instance].bKsvSecure), True);
	} else {
		/* Result is always secure if no check required */
		hdcpInfoListTx[instance].bKsvSecure = True;
	}

	/* Handle BKSV result */
	errCode = tmbslHdmiTxHdcpHandleBKSVResult(instance, hdcpInfoListTx[instance].bKsvSecure);

	if (errCode != TM_OK)
		return;


#else
	(void)instance;
#endif				/* NO_HDCP */
}

/******************************************************************************
    \brief BSTATUS interrupt callback.

    \param instance     Instance identifier.

    \return NA.

******************************************************************************/
static void dlHdmiTxHandleBSTATUS(tmInstance_t instance) {
#ifndef NO_HDCP
	UInt16 bstatus = 0;

	tmbslHdmiTxHdcpHandleBSTATUS(instance, &bstatus);

#ifdef HDMI_TX_REPEATER_ISR_MODE
	gIgnoreNextSha1 = False;
#endif				/*HDMI_TX_REPEATER_ISR_MODE */

	if (((bstatus & HDMITX_HDCP_BSTATUS_MAX_CASCADE_EXCEEDED) > 0)
	    || ((bstatus & HDMITX_HDCP_BSTATUS_MAX_DEVS_EXCEEDED) > 0)) {

		hdcpInfoListTx[instance].hdcpDeviceDepth =
		    (UInt8) ((bstatus & HDMITX_HDCP_BSTATUS_CASCADE_DEPTH) >> 8);

		/* The KsvList length is limited by the smaller of the list array
		 * length and the number of devices returned in BSTATUS */
		hdcpInfoListTx[instance].hdcpKsvDevices =
		    (UInt8) (bstatus & HDMITX_HDCP_BSTATUS_DEVICE_COUNT);

		if (HDMITX_KSV_LIST_MAX_DEVICES < hdcpInfoListTx[instance].hdcpKsvDevices) {
			hdcpInfoListTx[instance].hdcpKsvDevices = HDMITX_KSV_LIST_MAX_DEVICES;
		}

		if ((bstatus & HDMITX_HDCP_BSTATUS_MAX_CASCADE_EXCEEDED) > 0) {
			hdcpInfoListTx[instance].hdcpMaxCascExceeded = True;
		}

		if ((bstatus & HDMITX_HDCP_BSTATUS_MAX_DEVS_EXCEEDED) > 0) {
			hdcpInfoListTx[instance].hdcpMaxDevsExceeded = True;
		}

		if (dlHdmiTxGetEventStatus(instance, TMDL_HDMITX_RX_KEYS_RECEIVED) ==
		    TMDL_HDMITX_EVENT_ENABLED) {
			/* Release the sempahore */
			(void)tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance]);
			unitTableTx[instance].pCallback(TMDL_HDMITX_RX_KEYS_RECEIVED);
			/* Take the sempahore */
			(void)tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance]);
		}
	} else {

#ifdef HDMI_TX_REPEATER_ISR_MODE
		/* Call SHA_1 otherwise this ISR is missed */
		hdcpInfoListTx[instance].hdcpDeviceDepth =
		    (UInt8) ((bstatus & HDMITX_HDCP_BSTATUS_CASCADE_DEPTH) >> 8);

		if (dlHdmiTxGetEventStatus(instance, TMDL_HDMITX_B_STATUS) ==
		    TMDL_HDMITX_EVENT_ENABLED) {
			/* Release the sempahore */
			(void)tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance]);
			unitTableTx[instance].pCallback(TMDL_HDMITX_B_STATUS);
			/* Take the sempahore */
			(void)tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance]);
		}

#endif				/* HDMI_TX_REPEATER_ISR_MODE */
	}


#else
	(void)instance;
#endif				/* NO_HDCP */
}

/******************************************************************************
    \brief SHA_1 interrupt callback.

    \param instance     Instance identifier.

    \return NA.

******************************************************************************/
static void dlHdmiTxHandleSHA_1(tmInstance_t instance) {
#ifndef NO_HDCP
	tmErrorCode_t errCode;
	UInt8 indexKSVList;

#ifdef HDMI_TX_REPEATER_ISR_MODE
	if (gIgnoreNextSha1 == False) {
		gIgnoreNextSha1 = True;
#endif				/*HDMI_TX_REPEATER_ISR_MODE */

		errCode = tmbslHdmiTxHdcpHandleSHA_1(instance,
						     HDMITX_KSV_LIST_MAX_DEVICES,
						     hdcpInfoListTx[instance].hdcpKsvList,
						     &(hdcpInfoListTx[instance].hdcpKsvDevices),
						     &(hdcpInfoListTx[instance].hdcpDeviceDepth));
		if (errCode != TM_OK)
			return;

		/* Top level or repeater HDCP mode */
		if (unitTableTx[instance].repeaterEnable == False) {
			/* check HdcpKsvList against revocation list */

			indexKSVList = 0;
			while ((indexKSVList < TMDL_HDMITX_KSV_LIST_MAX_DEVICES) &&
			       (indexKSVList < hdcpInfoListTx[instance].hdcpKsvDevices) &&
			       (hdcpInfoListTx[instance].bKsvSecure == True)
			    ) {
				dlHdmiTxCheckHdcpBksv(instance,
						      &(hdcpInfoListTx[instance].
							hdcpKsvList[indexKSVList *
								    TMDL_HDMITX_KSV_BYTES_PER_DEVICE]),
						      &(hdcpInfoListTx[instance].bKsvSecure),
						      False);
				indexKSVList++;
			}
		} else {
			hdcpInfoListTx[instance].bKsvSecure = True;
		}

		/* Handle SHA_1 result */
		errCode = tmbslHdmiTxHdcpHandleSHA_1Result(instance,
							   hdcpInfoListTx[instance].bKsvSecure);

		if (dlHdmiTxGetEventStatus(instance, TMDL_HDMITX_RX_KEYS_RECEIVED) ==
		    TMDL_HDMITX_EVENT_ENABLED) {
			/* Release the sempahore */
			(void)tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance]);
			unitTableTx[instance].pCallback(TMDL_HDMITX_RX_KEYS_RECEIVED);
			/* Take the sempahore */
			(void)tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance]);
		}

		if (!hdcpInfoListTx[instance].bKsvSecure) {
			instanceStatusInfoTx[instance].pColBarState->changeColorBarNow = True;
			instanceStatusInfoTx[instance].pColBarState->colorBarOn = True;
			dlHdmiTxCheckColorBar(instance);
		}
#ifdef HDMI_TX_REPEATER_ISR_MODE
	}
#endif				/*HDMI_TX_REPEATER_ISR_MODE */


#else
	(void)instance;
#endif				/* NO_HDCP */
}

/******************************************************************************
    \brief PJ interrupt callback.

    \param instance     Instance identifier.

    \return NA.

******************************************************************************/
static void dlHdmiTxHandlePJ(tmInstance_t instance) {
#ifndef NO_HDCP
	tmbslHdmiTxHdcpHandlePJ(instance);
#else
	(void)instance;
#endif				/* NO_HDCP */
}

/******************************************************************************
    \brief R0 interrupt callback.

    \param instance     Instance identifier.

    \return NA.

******************************************************************************/
static void dlHdmiTxHandleR0(tmInstance_t instance) {
#ifndef NO_HDCP
	tmErrorCode_t errCode;
	tmbslHdmiTxSinkCategory_t category;



	if (hdcpInfoListTx[instance].bKsvSecure == True) {

		if (dlHdmiTxGetEventStatus(instance, TMDL_HDMITX_HDCP_ACTIVE) ==
		    TMDL_HDMITX_EVENT_ENABLED) {
			/* Release the sempahore */
			(void)tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance]);
			unitTableTx[instance].pCallback(TMDL_HDMITX_HDCP_ACTIVE);
			/* Take the sempahore */
			(void)tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance]);
		}

		instanceStatusInfoTx[instance].pColBarState->hdcpSecureOrT0 = False;
	}


	errCode = tmbslHdmiTxHdcpGetSinkCategory(instance, &category);
	if (errCode != TM_OK)
		return;

	if (category == HDMITX_SINK_CAT_NOT_REPEATER) {
		if (dlHdmiTxGetEventStatus(instance, TMDL_HDMITX_RX_KEYS_RECEIVED) ==
		    TMDL_HDMITX_EVENT_ENABLED) {
			/* Release the sempahore */
			(void)tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance]);
			unitTableTx[instance].pCallback(TMDL_HDMITX_RX_KEYS_RECEIVED);
			/* Take the sempahore */
			(void)tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance]);
		}
	}

	instanceStatusInfoTx[instance].pColBarState->disableColorBarOnR0 = True;
	instanceStatusInfoTx[instance].pColBarState->hdcpColbarChange = True;
#else
	(void)instance;
#endif				/* NO_HDCP */
}

/******************************************************************************
    \brief SW_INT interrupt callback.

    \param instance     Instance identifier.

    \return NA.

******************************************************************************/
static void dlHdmiTxHandleSW_INT(tmInstance_t instance) {
	DUMMY_ACCESS(instance);
}

/******************************************************************************
    \brief RX_SENSE interrupt callback.

    \param instance     Instance identifier.

    \return NA.

******************************************************************************/
static void dlHdmiTxHandleRX_SENSE(tmInstance_t instance) {
	tmErrorCode_t errCode;
	tmbslHdmiTxRxSense_t rxSenseStatus;	/* Rx Sense status */
	tmbslHdmiTxHotPlug_t hpdStatus;	/* HPD status */

	errCode = tmbslHdmiTxRxSenseGetStatus(instance, &rxSenseStatus, False);

	if (errCode != TM_OK)
		return;

	errCode = tmbslHdmiTxHotPlugGetStatus(instance, &hpdStatus, False);

	if (errCode != TM_OK)
		return;

/* if (hpdStatus == HDMITX_HOTPLUG_ACTIVE) */
/* { */
	if (rxSenseStatus == HDMITX_RX_SENSE_ACTIVE) {
		if (dlHdmiTxGetEventStatus(instance, TMDL_HDMITX_RX_DEVICE_ACTIVE) ==
		    TMDL_HDMITX_EVENT_ENABLED) {
			/* Release the sempahore */
			(void)tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance]);
			unitTableTx[instance].pCallback(TMDL_HDMITX_RX_DEVICE_ACTIVE);
			/* Take the sempahore */
			(void)tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance]);
		}
	} else if (rxSenseStatus == HDMITX_RX_SENSE_INACTIVE) {

		if (dlHdmiTxGetEventStatus(instance, TMDL_HDMITX_RX_DEVICE_INACTIVE) ==
		    TMDL_HDMITX_EVENT_ENABLED) {
			/* Release the sempahore */
			(void)tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance]);
			unitTableTx[instance].pCallback(TMDL_HDMITX_RX_DEVICE_INACTIVE);
			/* Take the sempahore */
			(void)tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance]);
		}
#ifndef NO_HDCP
		if (unitTableTx[instance].hdcpEnable == True) {
			/* Release the sempahore */
			(void)tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance]);
			/* Switch off HDCP */
			(void)tmdlHdmiTxSetHdcp(instance, False);
			/* Take the sempahore */
			(void)tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance]);
		}
#endif				/* NO_HDCP */
	}
/* } */
}

/******************************************************************************
    \brief EDID_READ interrupt callback.

    \param instance     Instance identifier.

    \return NA.

******************************************************************************/
static void dlHdmiTxHandleEDID_READ(tmInstance_t instance) {
	tmErrorCode_t errCode;
	UInt8 edidStatus = TMDL_HDMITX_EDID_NOT_READ;

	/* Get the edid status and read the connected device's EDID */

	/* Get Edid status */
	errCode = tmbslHdmiTxEdidGetStatus(instance, &edidStatus);

	if (errCode != TM_OK) {
		/* Set state machine to Plugged */
		dlHdmiTxSetState(instance, STATE_PLUGGED);
		return;
	}

	/* Has hot plug changed to Active? */
	if ((edidStatus == TMDL_HDMITX_EDID_READ) || (edidStatus == TMDL_HDMITX_EDID_ERROR_CHK)) {
		/* Set state machine to EDID available */
		dlHdmiTxSetState(instance, STATE_EDID_AVAILABLE);
	} else {
		/* Set state machine to Plugged */
		dlHdmiTxSetState(instance, STATE_PLUGGED);
	}


	if (dlHdmiTxGetEventStatus(instance, TMDL_HDMITX_EDID_RECEIVED) ==
	    TMDL_HDMITX_EVENT_ENABLED) {
		/* Release the sempahore */
		(void)tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance]);
		unitTableTx[instance].pCallback(TMDL_HDMITX_EDID_RECEIVED);
		/* Take the sempahore */
		(void)tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance]);
	}


}

/******************************************************************************
    \brief VS_RPT interrupt callback.

    \param instance     Instance identifier.

    \return NA.

******************************************************************************/
static void dlHdmiTxHandleVS_RPT(tmInstance_t instance) {
	if (dlHdmiTxGetEventStatus(instance, TMDL_HDMITX_VS_RPT_RECEIVED) ==
	    TMDL_HDMITX_EVENT_ENABLED) {
		/* Release the sempahore */
		(void)tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance]);
		unitTableTx[instance].pCallback(TMDL_HDMITX_VS_RPT_RECEIVED);
		/* Take the sempahore */
		(void)tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance]);
	}
}

/******************************************************************************
    \brief dlHdmiTxConvertDTDtoCEA_640HAP .

    \param  pDTDescriptors      DTD to convert.
	    pictureAspectRatio  aspect ratio of DTD
	    formatInterlaced    DTD Interlaced or progressif

    \return NA.

******************************************************************************/
static tmdlHdmiTxVidFmt_t dlHdmiTxConvertDTDtoCEA_640HAP
    (tmdlHdmiTxEdidVideoTimings_t *pDTDescriptors) {
	tmdlHdmiTxVidFmt_t codeCEA;

	switch (pDTDescriptors->vActiveLines) {
	case 480:
		codeCEA = TMDL_HDMITX_VFMT_01_640x480p_60Hz;
		break;

	default:
		/* Not a valid format */
		codeCEA = TMDL_HDMITX_VFMT_NULL;
		break;
	}

	return codeCEA;

}

/******************************************************************************
    \brief dlHdmiTxConvertDTDtoCEA_720HAP .

    \param  pDTDescriptors      DTD to convert.
	    pictureAspectRatio  aspect ratio of DTD
	    formatInterlaced    DTD Interlaced or progressif

    \return NA.

******************************************************************************/
static tmdlHdmiTxVidFmt_t dlHdmiTxConvertDTDtoCEA_720HAP
    (tmdlHdmiTxEdidVideoTimings_t *pDTDescriptors,
     tmdlHdmiTxPictAspectRatio_t pictureAspectRatio) {
	tmdlHdmiTxVidFmt_t codeCEA;

	switch (pDTDescriptors->vActiveLines) {
	case 480:
		if (pictureAspectRatio == TMDL_HDMITX_P_ASPECT_RATIO_4_3) {
			codeCEA = TMDL_HDMITX_VFMT_02_720x480p_60Hz;
		} else {
			codeCEA = TMDL_HDMITX_VFMT_03_720x480p_60Hz;
		}
		break;

	case 576:
		if (pictureAspectRatio == TMDL_HDMITX_P_ASPECT_RATIO_4_3) {
			codeCEA = TMDL_HDMITX_VFMT_17_720x576p_50Hz;
		} else {
			codeCEA = TMDL_HDMITX_VFMT_18_720x576p_50Hz;
		}
		break;

	default:
		/* Not a valid format */
		codeCEA = TMDL_HDMITX_VFMT_NULL;
		break;
	}

	return codeCEA;

}

/******************************************************************************
    \brief dlHdmiTxConvertDTDtoCEA_1280HAP .

    \param  pDTDescriptors      DTD to convert.
	    pictureAspectRatio  aspect ratio of DTD
	    formatInterlaced    DTD Interlaced or progressif

    \return NA.

******************************************************************************/
static tmdlHdmiTxVidFmt_t dlHdmiTxConvertDTDtoCEA_1280HAP
    (tmdlHdmiTxEdidVideoTimings_t *pDTDescriptors) {
	tmdlHdmiTxVidFmt_t codeCEA;

	switch (pDTDescriptors->vActiveLines) {
	case 720:
		switch (pDTDescriptors->hBlankPixels) {
		case 370:
			codeCEA = TMDL_HDMITX_VFMT_04_1280x720p_60Hz;
			break;

		case 700:
			codeCEA = TMDL_HDMITX_VFMT_19_1280x720p_50Hz;
			break;

		default:
			/* Not a valid format */
			codeCEA = TMDL_HDMITX_VFMT_NULL;
			break;
		}
		break;

	default:
		/* Not a valid format */
		codeCEA = TMDL_HDMITX_VFMT_NULL;
		break;
	}


	return codeCEA;
}

/******************************************************************************
    \brief dlHdmiTxConvertDTDtoCEA_1920HAP .

    \param  pDTDescriptors      DTD to convert.
	    pictureAspectRatio  aspect ratio of DTD
	    formatInterlaced    DTD Interlaced or progressif

    \return NA.

******************************************************************************/
static tmdlHdmiTxVidFmt_t dlHdmiTxConvertDTDtoCEA_1920HAP
    (tmdlHdmiTxEdidVideoTimings_t *pDTDescriptors, Bool formatInterlaced) {
	tmdlHdmiTxVidFmt_t codeCEA;

	switch (pDTDescriptors->hBlankPixels) {
	case 280:
		if (formatInterlaced) {
			codeCEA = TMDL_HDMITX_VFMT_05_1920x1080i_60Hz;
		} else {
			if (pDTDescriptors->pixelClock == 14850) {
				codeCEA = TMDL_HDMITX_VFMT_16_1920x1080p_60Hz;
			} else {
				codeCEA = TMDL_HDMITX_VFMT_34_1920x1080p_30Hz;
			}
		}
		break;

	case 720:
		if (formatInterlaced) {
			codeCEA = TMDL_HDMITX_VFMT_20_1920x1080i_50Hz;
		} else {
			switch (pDTDescriptors->pixelClock) {
			case 14850:
				codeCEA = TMDL_HDMITX_VFMT_31_1920x1080p_50Hz;
				break;

			case 7425:
				codeCEA = TMDL_HDMITX_VFMT_33_1920x1080p_25Hz;
				break;
			default:
				/* Not a valid format */
				codeCEA = TMDL_HDMITX_VFMT_NULL;
				break;
			}
		}
		break;

	case 830:
		codeCEA = TMDL_HDMITX_VFMT_32_1920x1080p_24Hz;
		break;

	default:
		/* Not a valid format */
		codeCEA = TMDL_HDMITX_VFMT_NULL;
		break;
	}


	return codeCEA;
}

/******************************************************************************
    \brief dlHdmiTxConvertDTDtoCEA_1440HAP .

    \param  pDTDescriptors      DTD to convert.
	    pictureAspectRatio  aspect ratio of DTD
	    formatInterlaced    DTD Interlaced or progressif

    \return NA.

******************************************************************************/
static tmdlHdmiTxVidFmt_t dlHdmiTxConvertDTDtoCEA_1440HAP
    (tmdlHdmiTxEdidVideoTimings_t *pDTDescriptors,
     tmdlHdmiTxPictAspectRatio_t pictureAspectRatio, Bool formatInterlaced) {
	tmdlHdmiTxVidFmt_t codeCEA;

	switch (pDTDescriptors->vActiveLines) {
	case 240:
		if (formatInterlaced) {
			if (pictureAspectRatio == TMDL_HDMITX_P_ASPECT_RATIO_4_3) {
				codeCEA = TMDL_HDMITX_VFMT_06_720x480i_60Hz;
			} else {
				codeCEA = TMDL_HDMITX_VFMT_07_720x480i_60Hz;
			}
		} else {
			if (pictureAspectRatio == TMDL_HDMITX_P_ASPECT_RATIO_4_3) {
				codeCEA = TMDL_HDMITX_VFMT_08_720x240p_60Hz;
			} else {
				codeCEA = TMDL_HDMITX_VFMT_09_720x240p_60Hz;
			}
		}
		break;

	case 288:
		if (formatInterlaced) {
			if (pictureAspectRatio == TMDL_HDMITX_P_ASPECT_RATIO_4_3) {
				codeCEA = TMDL_HDMITX_VFMT_21_720x576i_50Hz;
			} else {
				codeCEA = TMDL_HDMITX_VFMT_22_720x576i_50Hz;
			}
		} else {
			if (pictureAspectRatio == TMDL_HDMITX_P_ASPECT_RATIO_4_3) {
				codeCEA = TMDL_HDMITX_VFMT_23_720x288p_50Hz;
			} else {
				codeCEA = TMDL_HDMITX_VFMT_24_720x288p_50Hz;
			}
		}
		break;

	case 480:
		if (pictureAspectRatio == TMDL_HDMITX_P_ASPECT_RATIO_4_3) {
			codeCEA = TMDL_HDMITX_VFMT_14_1440x480p_60Hz;
		} else {
			codeCEA = TMDL_HDMITX_VFMT_15_1440x480p_60Hz;
		}
		break;

	case 576:
		if (pictureAspectRatio == TMDL_HDMITX_P_ASPECT_RATIO_4_3) {
			codeCEA = TMDL_HDMITX_VFMT_29_1440x576p_50Hz;
		} else {
			codeCEA = TMDL_HDMITX_VFMT_30_1440x576p_50Hz;
		}
		break;

	default:
		/* Not a valid format */
		codeCEA = TMDL_HDMITX_VFMT_NULL;
		break;
	}

	return codeCEA;
}

/******************************************************************************
    \brief dlHdmiTxConvertDTDtoCEA_2880HAP .

    \param  pDTDescriptors      DTD to convert.
	    pictureAspectRatio  aspect ratio of DTD
	    formatInterlaced    DTD Interlaced or progressif

    \return NA.

******************************************************************************/
static tmdlHdmiTxVidFmt_t dlHdmiTxConvertDTDtoCEA_2880HAP
    (tmdlHdmiTxEdidVideoTimings_t *pDTDescriptors,
     tmdlHdmiTxPictAspectRatio_t pictureAspectRatio, Bool formatInterlaced) {
	tmdlHdmiTxVidFmt_t codeCEA;

	switch (pDTDescriptors->vActiveLines) {
	case 240:
		if (formatInterlaced) {
			if (pictureAspectRatio == TMDL_HDMITX_P_ASPECT_RATIO_4_3) {
				codeCEA = TMDL_HDMITX_VFMT_10_720x480i_60Hz;
			} else {
				codeCEA = TMDL_HDMITX_VFMT_11_720x480i_60Hz;
			}
		} else {
			if (pictureAspectRatio == TMDL_HDMITX_P_ASPECT_RATIO_4_3) {
				codeCEA = TMDL_HDMITX_VFMT_12_720x240p_60Hz;
			} else {
				codeCEA = TMDL_HDMITX_VFMT_13_720x240p_60Hz;
			}
		}
		break;

	case 288:
		if (formatInterlaced) {
			if (pictureAspectRatio == TMDL_HDMITX_P_ASPECT_RATIO_4_3) {
				codeCEA = TMDL_HDMITX_VFMT_25_720x576i_50Hz;
			} else {
				codeCEA = TMDL_HDMITX_VFMT_26_720x576i_50Hz;
			}
		} else {
			if (pictureAspectRatio == TMDL_HDMITX_P_ASPECT_RATIO_4_3) {
				codeCEA = TMDL_HDMITX_VFMT_27_720x288p_50Hz;
			} else {
				codeCEA = TMDL_HDMITX_VFMT_28_720x288p_50Hz;
			}
		}
		break;

	default:
		/* Not a valid format */
		codeCEA = TMDL_HDMITX_VFMT_NULL;
		break;
	}

	return codeCEA;
}

/******************************************************************************
    \brief EdidGetDTD .

    \param .

    \return NA.

******************************************************************************/
tmErrorCode_t dlHdmiTxEdidGetDTD
    (tmInstance_t instance,
     tmdlHdmiTxEdidVideoTimings_t *pDTDescriptors, UInt8 maxDTDesc, UInt8 *pWrittenDTDesc) {
	tmErrorCode_t errCode;

	/* Check the current state */
	RETIF(dlHdmiTxGetState(instance) != STATE_EDID_AVAILABLE, TMDL_ERR_DLHDMITX_INVALID_STATE)

	    /* Get detailled descriptors from EDID, return TMDL_ERR_DLHDMITX_NO_RESOURCES if EDID are not read */
	    RETIF((errCode =
		   tmbslHdmiTxEdidGetDetailedTimingDescriptors(instance,
							       (tmbslHdmiTxEdidDtd_t *)
							       pDTDescriptors, maxDTDesc,
							       pWrittenDTDesc)) != TM_OK, errCode);

	return TM_OK;
}


/******************************************************************************
    \brief Command processing task, dedicated to unit/instance 0.

    \param NA.

    \return NA.

******************************************************************************/

#ifndef TMFL_NO_RTOS
static void CommandTaskUnit0(void)
{
	UInt8 command;
	Bool loop = True;	/* Just to avoid compiler warning */
	tmErrorCode_t err = TM_OK;

	while (loop) {
		tmdlHdmiTxIWQueueReceive(unitTableTx[0].queueHandle, &command);

		/* Take the sempahore */
		(void)tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[0]);

		/* Clear T0 flag before polling for interrupts */
		instanceStatusInfoTx[0].pColBarState->hdcpSecureOrT0 = False;

		if (gI2CDebugAccessesEnabled == True) {

			err = tmbslHdmiTxHwHandleInterrupt(0);

			if ((err == TMBSL_ERR_HDMI_I2C_WRITE) || (err == TMBSL_ERR_HDMI_I2C_READ)) {

				unitTableTx[0].pCallback(TMDL_HDMITX_DEBUG_EVENT_1);
			}

		}

		/* (gI2CDebugAccessesEnabled == True) */
		/* Enable interrupts for Tx (interrupts are disabled in the HandleInterrupt function) */
		tmdlHdmiTxIWEnableInterrupts(TMDL_HDMI_IW_TX_1);

		/* Release the sempahore */
		(void)tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[0]);
	};
}
#endif				/* TMFL_NO_RTOS */

/******************************************************************************
    \brief Hdcp check task, dedicated to unit/instance 0.

    \param NA.

    \return NA.

******************************************************************************/
#ifndef TMFL_NO_RTOS
static void HdcpTaskUnit0(void)
{
	Bool loop = True;	/* Just to avoid compiler warning */
	Bool featureSupported;

	tmbslHdmiTxHwGetCapabilities(0, HDMITX_FEATURE_HW_HDCP, &featureSupported);

#ifndef NO_HDCP
	while (loop) {
		(void)tmdlHdmiTxIWWait(35);

		/* Take the sempahore */
		(void)tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[0]);

		if (gI2CDebugAccessesEnabled == True) {

			dlHdmiTxCheckColorBar(0);
			dlHdmiTxCheckHdcpColorBar(0);

			if (featureSupported == True) {
				tmbslHdmiTxHdcpCheck(0, 35,
						     (tmbslHdmiTxHdcpCheck_t *) &
						     (hdcpInfoListTx[0].hdcpCheckState));
			}

		}

		/*  gI2CDebugAccessesEnabled == True */
		/* Release the sempahore */
		(void)tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[0]);
	};
#else
	(void)loop;
#endif				/* NO_HDCP */
}
#endif				/* TMFL_NO_RTOS */

#ifndef NO_HDCP
/******************************************************************************
    \brief Check hdcp state to manage color bar.

    \param instance Instance identifier.

    \return NA.

******************************************************************************/
static void dlHdmiTxCheckHdcpColorBar(tmInstance_t instance) {
	/* Use HDCP check result to control HDCP colour bars */
	if ((instanceStatusInfoTx[instance].pColBarState->disableColorBarOnR0 == True)
	    && (instanceStatusInfoTx[instance].pColBarState->hdcpColbarChange == True)
	    && (instanceStatusInfoTx[instance].pColBarState->hdcpSecureOrT0 == False)) {
		/* Remove test pattern once if Authenticated with no error interrupts */
		if (instanceStatusInfoTx[instance].pColBarState->colorBarOn != False) {
			instanceStatusInfoTx[instance].pColBarState->colorBarOn = False;
			instanceStatusInfoTx[instance].pColBarState->changeColorBarNow = True;

			if (unitTableTx[instance].simplayHd == True) {

				/* Mute or Un-mute the audio output */
				tmbslHdmiTxAudioOutSetMute(instance,
							   (tmbslHdmiTxaMute_t) HDMITX_AMUTE_OFF);

				/* Store current audio mute status */
				instanceStatusInfoTx[instance].pAudioInfo->audioMuteState = False;
			}


		}
		/* Reset state flags */
		instanceStatusInfoTx[instance].pColBarState->hdcpColbarChange = False;
		instanceStatusInfoTx[instance].pColBarState->hdcpSecureOrT0 = True;

#ifdef TMFL_TDA19989
		instanceStatusInfoTx[instance].pColBarState->disableColorBarOnR0 = False;
#endif				/* TMFL_TDA19989 */



	}

	if ((instanceStatusInfoTx[instance].pColBarState->hdcpEncryptOrT0 == True)
	    && (instanceStatusInfoTx[instance].pColBarState->inOutFirstSetDone == True)) {
		/* Set test pattern once if not Authenticated, to mask HDCP failure */
		if (instanceStatusInfoTx[instance].pColBarState->colorBarOn != True) {
			instanceStatusInfoTx[instance].pColBarState->colorBarOn = True;
			instanceStatusInfoTx[instance].pColBarState->changeColorBarNow = True;

			if (unitTableTx[instance].simplayHd == True) {

				/* Mute or Un-mute the audio output */
				tmbslHdmiTxAudioOutSetMute(instance,
							   (tmbslHdmiTxaMute_t) HDMITX_AMUTE_ON);

				/* Store current audio mute status */
				instanceStatusInfoTx[instance].pAudioInfo->audioMuteState = True;
			}

		}
		/* Reset state flag */
		instanceStatusInfoTx[instance].pColBarState->hdcpEncryptOrT0 = False;
	}
}
#endif

#ifndef NO_HDCP
/******************************************************************************
    \brief Show color bars or restore the last video format.

    \param instance Instance identifier.

    \return NA.

******************************************************************************/
static void dlHdmiTxCheckColorBar(tmInstance_t instance) {
	if ((instanceStatusInfoTx[instance].pColBarState->inOutFirstSetDone == True)
	    && (instanceStatusInfoTx[instance].pColBarState->changeColorBarNow == True)) {
		instanceStatusInfoTx[instance].pColBarState->changeColorBarNow = False;

		if (unitTableTx[instance].simplayHd == True) {
			if (instanceStatusInfoTx[instance].pColBarState->colorBarOn == True) {
				/* Set service mode colour bar on/off (also used as HDCP logo pattern) */
				(void)dlHdmiTxSetTestPatternOn(instance,
							       instanceStatusInfoTx[instance].
							       pVideoInfo->videoOutConfig.format,
							       instanceStatusInfoTx[instance].
							       pVideoInfo->videoOutConfig.mode,
							       gtmdlHdmiTxDriverConfigTable
							       [instance].pattern);
			} else {
				/* Restore last output format and mode */
				(void)dlHdmiTxSetTestPatternOff(instance,
								instanceStatusInfoTx[instance].
								pVideoInfo->videoOutConfig.format,
								instanceStatusInfoTx[instance].
								pVideoInfo->videoOutConfig.mode);
			}
		}
	}
}
#endif

#ifndef NO_HDCP
/******************************************************************************
    \brief Get hdcp seed.

    \param instance Instance identifier.

    \return NA.

******************************************************************************/
static void dlHdmiTxFindHdcpSeed(tmInstance_t instance) {
#if HDCP_SEED_DEFAULT == HDCP_SEED_NULL
	UInt8 otp[3];
#endif

	/* If no seed is coded in this file then find it somewhere else */
#if HDCP_SEED_DEFAULT == HDCP_SEED_NULL
	/* See if a seed table has been programmed in flash */
	if (kSeedTable[0][0] != 0xFFFF) {
		/* Read OTP LSB at address 0x00 and try to match in flash table */
		if ((tmbslHdmiTxHdcpGetOtp(instance, 0x00, otp)) == TM_OK) {
			int i;
			for (i = 0; i < SEED_TABLE_LEN; i++) {
				if (kSeedTable[i][0] == otp[2]) {	/* OTP_DATA_LSB */
					/* Found seed! */
					gtmdlHdmiTxDriverConfigTable[instance].keySeed =
					    kSeedTable[i][1];
					break;
				}
			}
		}
	}
#endif				/* HDCP_SEED_DEFAULT != HDCP_SEED_NULL */

	/* Initialise the TDA9984 HDCP keys */
	if (gtmdlHdmiTxDriverConfigTable[instance].keySeed != HDCP_SEED_NULL) {
		/* Initialise the HDMI Transmitter HDCP keys */
		tmbslHdmiTxHdcpDownloadKeys(instance,
					    gtmdlHdmiTxDriverConfigTable[instance].keySeed,
					    HDMITX_HDCP_DECRYPT_ENABLE);
	}
}
#endif				/* NO_HDCP */

/******************************************************************************
    \brief Set the state of the state machine.

    \param instance Instance identifier.
    \param state    State of the state machine.

    \return NA.

******************************************************************************/
static void dlHdmiTxSetState(tmInstance_t instance, tmdlHdmiTxDriverState_t state) {
	/* Set the state */
	unitTableTx[instance].state = state;
}

/******************************************************************************
    \brief Get the state of the state machine.

    \param instance Instance identifier.

    \return tmdlHdmiTxDriverState_t Current State of the state machine.

******************************************************************************/
tmdlHdmiTxDriverState_t dlHdmiTxGetState(tmInstance_t instance) {
	tmdlHdmiTxDriverState_t state;

	/* Get the state */
	state = unitTableTx[instance].state;

	return (state);
}

/******************************************************************************
    \brief Get the state of the event (enabled or disabled).

    \param instance Instance identifier.
    \param event    Event to give the state.

    \return NA.

******************************************************************************/
static tmdlHdmiTxEventStatus_t dlHdmiTxGetEventStatus
    (tmInstance_t instance, tmdlHdmiTxEvent_t event) {
	tmdlHdmiTxEventStatus_t eventStatus;

	/* Get the event status */
	eventStatus = instanceStatusInfoTx[instance].pEventState[event].status;

	return (eventStatus);
}

/******************************************************************************
    \brief Caculation of aspect ratio.

    \param HImageSize Horizontal image size.
    \param VImageSize Vertical image size.

    \return NA.

******************************************************************************/
static tmdlHdmiTxPictAspectRatio_t dlHdmiTxCalcAspectRatio(UInt16 HImageSize, UInt16 VImageSize)
{
	tmdlHdmiTxPictAspectRatio_t pictureAspectRatio;
	UInt16 calcPictureAspectRatio;

	/* Define picture Aspect Ratio                                          */
	/* 16/9 = 1.77777 so the result approach is 2                           */
	/* 4/3 = 1.33333 so the result approach is 1                            */
	/*  operation :                                                         */
	/* ImageSize + (vImageSize/2)                                           */
	/* -------------------------- > vImageSize     ->True 16/9 False 4/3    */
	/*           2                                                          */

	calcPictureAspectRatio = ((UInt16) (HImageSize + ((VImageSize) >> 1))) >> 1;

	if (calcPictureAspectRatio > VImageSize) {
		pictureAspectRatio = TMDL_HDMITX_P_ASPECT_RATIO_16_9;
	} else {
		pictureAspectRatio = TMDL_HDMITX_P_ASPECT_RATIO_4_3;
	}

	return pictureAspectRatio;

}

#ifndef NO_HDCP
/******************************************************************************
    \brief dlHdmiTxCheckHdcpBksv .

    \param pHdcpBksvTested  ksv To test.
    \param pbBksvSecure     Test result.
    \param bBigEndian       ksv provide by hardware are in little or big endian.

    \return NA.

******************************************************************************/
static void dlHdmiTxCheckHdcpBksv
    (tmInstance_t instance, UInt8 *pHdcpBksvTested, Bool *pbBksvSecure, Bool bBigEndian) {

	UInt32 NbInRevocationList;

	NbInRevocationList = 0;

	/* CBE: force secure, otherwise we will not look at anything */
	*pbBksvSecure = True;

	if ((unitTableTx[instance].revocationList.pList != Null)
	    && (unitTableTx[instance].revocationList.length > 0)) {
		while ((*pbBksvSecure == True)
		       && (NbInRevocationList < unitTableTx[instance].revocationList.length)) {
			if (bBigEndian) {
				if ((pHdcpBksvTested[0] ==
				     unitTableTx[instance].revocationList.pList[NbInRevocationList *
										HDMITX_KSV_BYTES_PER_DEVICE])
				    && (pHdcpBksvTested[1] ==
					unitTableTx[instance].revocationList.pList[1 +
										   (NbInRevocationList
										    *
										    HDMITX_KSV_BYTES_PER_DEVICE)])
				    && (pHdcpBksvTested[2] ==
					unitTableTx[instance].revocationList.pList[2 +
										   (NbInRevocationList
										    *
										    HDMITX_KSV_BYTES_PER_DEVICE)])
				    && (pHdcpBksvTested[3] ==
					unitTableTx[instance].revocationList.pList[3 +
										   (NbInRevocationList
										    *
										    HDMITX_KSV_BYTES_PER_DEVICE)])
				    && (pHdcpBksvTested[4] ==
					unitTableTx[instance].revocationList.pList[4 +
										   (NbInRevocationList
										    *
										    HDMITX_KSV_BYTES_PER_DEVICE)])
				    ) {
					*pbBksvSecure = False;
				}
			} else {
				if ((pHdcpBksvTested[4] ==
				     unitTableTx[instance].revocationList.pList[NbInRevocationList *
										HDMITX_KSV_BYTES_PER_DEVICE])
				    && (pHdcpBksvTested[3] ==
					unitTableTx[instance].revocationList.pList[1 +
										   (NbInRevocationList
										    *
										    HDMITX_KSV_BYTES_PER_DEVICE)])
				    && (pHdcpBksvTested[2] ==
					unitTableTx[instance].revocationList.pList[2 +
										   (NbInRevocationList
										    *
										    HDMITX_KSV_BYTES_PER_DEVICE)])
				    && (pHdcpBksvTested[1] ==
					unitTableTx[instance].revocationList.pList[3 +
										   (NbInRevocationList
										    *
										    HDMITX_KSV_BYTES_PER_DEVICE)])
				    && (pHdcpBksvTested[0] ==
					unitTableTx[instance].revocationList.pList[4 +
										   (NbInRevocationList
										    *
										    HDMITX_KSV_BYTES_PER_DEVICE)])
				    ) {
					*pbBksvSecure = False;
				}
			}
			NbInRevocationList++;
		}

	}


}
#endif

/******************************************************************************
    \brief dlHdmiTxCalcVidFmtIndex.

    \param vidFmt  video format.

    \return table index.

******************************************************************************/
static tmdlHdmiTxVidFmt_t dlHdmiTxCalcVidFmtIndex(tmdlHdmiTxVidFmt_t vidFmt)
{
	tmdlHdmiTxVidFmt_t vidFmtIndex = vidFmt;

	/* Hanlde VIC or table index discontinuity */
	if ((vidFmt >= TMDL_HDMITX_VFMT_60_1280x720p_24Hz)
	    && (vidFmt <= TMDL_HDMITX_VFMT_62_1280x720p_30Hz)) {
		vidFmtIndex =
		    (tmdlHdmiTxVidFmt_t) (TMDL_HDMITX_VFMT_INDEX_60_1280x720p_24Hz +
					  (vidFmt - TMDL_HDMITX_VFMT_60_1280x720p_24Hz));
	}
#ifdef FORMAT_PC
	else if (vidFmt >= TMDL_HDMITX_VFMT_PC_MIN) {
		vidFmtIndex =
		    (tmdlHdmiTxVidFmt_t) (TMDL_HDMITX_VFMT_TV_NUM +
					  (vidFmt - TMDL_HDMITX_VFMT_PC_MIN));
	}
#endif				/* FORMAT_PC */
	return (vidFmtIndex);
}


tmErrorCode_t tmdlHdmiTxDebugEnableI2CAccesses(tmInstance_t instance, Bool enableI2C)
{
	tmErrorCode_t errCode = TM_OK;

	/* Check if instance number is in range */
	if ((instance < 0) || (instance >= MAX_UNITS)) {
		errCode = TMDL_ERR_DLHDMITX_BAD_INSTANCE;
		return errCode;
	}

	if (enableI2C == True) {
		errCode = tmbslDebugWriteFakeRegPage(instance);
		gI2CDebugAccessesEnabled = True;
	} else {
		gI2CDebugAccessesEnabled = False;
	}


	return errCode;

}				/* tmdlHdmiTxDebugManageI2CAccesses */

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
    (tmInstance_t instance, tmdlHdmiTxHdcpStatus_t *pHdcpStatus, UInt8 *pRawStatus) {
	tmErrorCode_t errCode = TM_OK;

#ifndef NO_HDCP
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)
	    RETIF(pHdcpStatus == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)
	    RETIF(pRawStatus == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    * pRawStatus = hdcpInfoListTx[instance].hdcpErrorState;

	switch (hdcpInfoListTx[instance].hdcpErrorState) {

	case 0:
		*pHdcpStatus = TMDL_HDMITX_HDCP_OK;
		break;

	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
		*pHdcpStatus = TMDL_HDMITX_HDCP_BKSV_RCV_FAIL;
		break;

	case 0x08:
		*pHdcpStatus = TMDL_HDMITX_HDCP_BKSV_CHECK_FAIL;
		break;

	case 0x0C:
		*pHdcpStatus = TMDL_HDMITX_HDCP_BCAPS_RCV_FAIL;
		break;

	case 0x0F:
	case 0x10:
	case 0x11:
		*pHdcpStatus = TMDL_HDMITX_HDCP_AKSV_SEND_FAIL;
		break;

	case 0x23:
	case 0x24:
	case 0x25:
		*pHdcpStatus = TMDL_HDMITX_HDCP_R0_RCV_FAIL;
		break;

	case 0x26:
		*pHdcpStatus = TMDL_HDMITX_HDCP_R0_CHECK_FAIL;
		break;

	case 0x27:
		*pHdcpStatus = TMDL_HDMITX_HDCP_BKSV_NOT_SECURE;
		break;

	case 0x2B:
	case 0x2C:
	case 0x2D:
		*pHdcpStatus = TMDL_HDMITX_HDCP_RI_RCV_FAIL;
		break;

	case 0x77:
	case 0x78:
	case 0x79:
		*pHdcpStatus = TMDL_HDMITX_HDCP_RPT_RI_RCV_FAIL;
		break;

	case 0x2E:
		*pHdcpStatus = TMDL_HDMITX_HDCP_RI_CHECK_FAIL;
		break;

	case 0x7A:
		*pHdcpStatus = TMDL_HDMITX_HDCP_RPT_RI_CHECK_FAIL;
		break;

	case 0x66:
		*pHdcpStatus = TMDL_HDMITX_HDCP_RPT_BCAPS_RCV_FAIL;
		break;

	case 0x67:
		*pHdcpStatus = TMDL_HDMITX_HDCP_RPT_BCAPS_READY_TIMEOUT;
		break;

	case 0x6A:
		*pHdcpStatus = TMDL_HDMITX_HDCP_RPT_V_RCV_FAIL;
		break;

	case 0x6C:
	case 0x6D:
		*pHdcpStatus = TMDL_HDMITX_HDCP_RPT_BSTATUS_RCV_FAIL;
		break;

	case 0x6F:
		*pHdcpStatus = TMDL_HDMITX_HDCP_RPT_KSVLIST_RCV_FAIL;
		break;

	case 0x74:
		*pHdcpStatus = TMDL_HDMITX_HDCP_RPT_KSVLIST_NOT_SECURE;
		break;

	default:
		*pHdcpStatus = TMDL_HDMITX_HDCP_UNKNOWN_STATUS;
		break;
	}

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)
#else
	(void)instance;		/* Remove compiler warning */
#endif				/* NO_HDCP */

	return errCode;
}


tmErrorCode_t tmdlHdmiTxGetEdidLatencyInfo
    (tmInstance_t instance, tmdlHdmiTxEdidLatency_t *pLatency) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Check pointer is Null */
	    RETIF(pLatency == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Check the current state */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      dlHdmiTxGetState(instance) != STATE_EDID_AVAILABLE,
		      TMDL_ERR_DLHDMITX_INVALID_STATE)

	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode =
		       tmbslHdmiTxEdidGetLatencyInfo(instance,
						     (tmbslHdmiTxEdidLatency_t *) pLatency)) !=
		      TM_OK, errCode)

	    /* Release the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}				/* tmdlHdmiTxGetEdidLatencyInfo */

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
    (tmInstance_t instance, tmdlHdmiTxEdidExtraVsdbData_t **pExtraVsdbData) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    /* Check pointer is Null */
	    RETIF(pExtraVsdbData == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Check the current state */
	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      dlHdmiTxGetState(instance) != STATE_EDID_AVAILABLE,
		      TMDL_ERR_DLHDMITX_INVALID_STATE)

	    RETIF_SEM(dlHdmiTxItSemaphore[instance],
		      (errCode =
		       tmbslHdmiTxEdidGetExtraVsdbData(instance,
						       (tmbslHdmiTxEdidExtraVsdbData_t **)
						       pExtraVsdbData)) != TM_OK, errCode)

	    /* Release the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;
}				/* tmdlHdmiTxGetEdidExtraVsdbData */


tmErrorCode_t tmdlHdmiTxGetHPDStatus(tmInstance_t instance, tmdlHdmiTxHotPlug_t *pHPDStatus) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    RETIF(pHPDStatus == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)


	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Get the HPD status from BSL driver */
	    errCode =
	    tmbslHdmiTxHotPlugGetStatus(instance, (tmbslHdmiTxHotPlug_t *) pHPDStatus, True);

	if (errCode == TM_OK) {
		/* do nothing */
	} else {
		*pHPDStatus = TMDL_HDMITX_HOTPLUG_INVALID;
	}

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;

}				/* tmdlHdmiTxGetHPDStatus */



tmErrorCode_t tmdlHdmiTxGetRXSenseStatus
    (tmInstance_t instance, tmdlHdmiTxRxSense_t *pRXSenseStatus) {
	tmErrorCode_t errCode;

	/* Check if instance number is in range */
	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    RETIF(pRXSenseStatus == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)


	    /* Take the sempahore */
	    RETIF((errCode =
		   tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    /* Get the RXS sense status from BSL driver */
	    errCode =
	    tmbslHdmiTxRxSenseGetStatus(instance, (tmbslHdmiTxRxSense_t *) pRXSenseStatus, True);

	if (errCode == TM_OK) {
		/* do nothing */
	} else {
		*pRXSenseStatus = TMDL_HDMITX_RX_SENSE_INVALID;
	}

	/* Release the sempahore */
	RETIF((errCode = tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCode)

	    return TM_OK;

}				/* tmdlHdmiTxGetRXSenseStatus */


/******************************************************************************
    \brief Mute or unmute the TMDS outputs.

    \param instance         Instance identifier.
    \param muteTmdsOut      Mute or unmute indication.

    \return NA.

******************************************************************************/
tmErrorCode_t tmdlHdmiTxTmdsSetOutputsMute(tmInstance_t instance, Bool muteTmdsOut) {
	tmErrorCode_t errCode;
	tmErrorCode_t errCodeSem;
	tmbslHdmiTxTmdsOut_t tmdsOut;

	RETIF((instance < 0) || (instance >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_INSTANCE)

	    if (muteTmdsOut)
		tmdsOut = HDMITX_TMDSOUT_FORCED0;	/* forced 0 outputs */
	else
		tmdsOut = HDMITX_TMDSOUT_NORMAL;	/* normal outputs */

	/* Take the sempahore */
	RETIF((errCodeSem =
	       tmdlHdmiTxIWSemaphoreP(dlHdmiTxItSemaphore[instance])) != TM_OK, errCodeSem)

	    errCode = tmbslHdmiTxTmdsSetOutputs(instance, tmdsOut);

	/* Release the sempahore */
	RETIF((errCodeSem =
	       tmdlHdmiTxIWSemaphoreV(dlHdmiTxItSemaphore[instance])) != TM_OK, errCodeSem)


	    return errCode;
}

/*============================================================================*/
/*                            END OF FILE                                     */
/*============================================================================*/
