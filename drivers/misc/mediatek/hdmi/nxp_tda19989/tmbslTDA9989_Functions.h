/**
 * Copyright (C) 2009 NXP N.V., All Rights Reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of NXP N.V. and is confidential in nature. Under no circumstances
 * is this software to be  exposed to or placed under an Open Source License of
 * any type without the expressed written permission of NXP N.V.
 *
 * \file          tmbslTDA9989_functions.h
 *
 * \version       $Revision: 2 $
 *
 *
*/

#ifndef TMBSLTDA9989_FUNCTIONS_H
#define TMBSLTDA9989_FUNCTIONS_H

/*============================================================================*/
/*                       INCLUDE FILES                                        */
/*============================================================================*/

#include "tmNxCompId.h"
#include "tmbslHdmiTx_types.h"


#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/*                       EXTERN FUNCTION PROTOTYPES                           */
/*============================================================================*/

/*============================================================================*/
/**
    \brief      Reset the Clock Time Stamp generator in HDMI mode only

    \param[in]  txUnit      Transmitter unit number

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing the I2C bus
		  - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: in DVI mode
 */
	tmErrorCode_t tmbslTDA9989AudioInResetCts(tmUnitSelect_t txUnit);

/**
    \brief      Set audio input configuration in HDMI mode only

    \param[in]  txUnit      Transmitter unit number
    \param[in]  aFmt        Audio input format
    \param[in]  i2sFormat   I2s format type
    \param[in]  chanI2s     I2S channel allocation
    \param[in]  chanDsd     DSD channel allocation
    \param[in]  clkPolDsd   DSD clock polarity
    \param[in]  swapDsd     DSD data swap
    \param[in]  layout      Sample layout
    \param[in]  latency_rd  Audio FIFO read latency
    \param[in]  dstRate     Dst rate (not used)

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: in DVI mode
 */
	tmErrorCode_t
	    tmbslTDA9989AudioInSetConfig
	    (tmUnitSelect_t txUnit,
	     tmbslHdmiTxaFmt_t aFmt,
	     tmbslHdmiTxI2sFor_t i2sFormat,
	     UInt8 chanI2s,
	     UInt8 chanDsd,
	     tmbslHdmiTxClkPolDsd_t clkPolDsd,
	     tmbslHdmiTxSwapDsd_t swapDsd,
	     UInt8 layout, UInt16 latency_rd, tmbslHdmiTxDstRate_t dstRate);


/**
    \brief      Set the Clock Time Stamp generator in HDMI mode only

    \param[in]  txUnit      Transmitter unit number
    \param[in]  ctsRef      Clock Time Stamp reference source
    \param[in]  afs         Audio input sample frequency
    \param[in]  voutFmt     Video output format
    \param[in]  voutFreq    Vertical output frequency
    \param[in]  uCts        Manual Cycle Time Stamp
    \param[in]  uCtsX       Clock Time Stamp factor x
    \param[in]  ctsK        Clock Time Stamp predivider k
    \param[in]  ctsM        Clock Time Stamp postdivider m
    \param[in]  dstRate     Dst rate (not used)

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: in DVI mode
 */
	tmErrorCode_t
	    tmbslTDA9989AudioInSetCts
	    (tmUnitSelect_t txUnit,
	     tmbslHdmiTxctsRef_t ctsRef,
	     tmbslHdmiTxafs_t afs,
	     tmbslHdmiTxVidFmt_t voutFmt,
	     tmbslHdmiTxVfreq_t voutFreq,
	     UInt32 uCts,
	     UInt16 uCtsX,
	     tmbslHdmiTxctsK_t ctsK, tmbslHdmiTxctsM_t ctsM, tmbslHdmiTxDstRate_t dstRate);

/**
    \brief      Set the Channel Status Bytes 0,1,3 & 4

    \param[in]  txUnit              Transmitter unit number
    \param[in]  copyright           Byte 0 Copyright bit (bit2)
    \param[in]  formatInfo          Byte 0 Audio sample format (bit1) and additional info (bit345)
    \param[in]  categoryCode        Byte 1 Category code (bits8-15)
    \param[in]  sampleFreq          Byte 3 Sample Frequency (bits24-27)
    \param[in]  clockAccuracy       Byte 3 Clock Accuracy (bits38-31)
    \param[in]  maxWordLength       Byte 4 Maximum word length (bit32)
    \param[in]  wordLength          Byte 4 Word length (bits33-35)
    \param[in]  origSampleFreq      Byte 4 Original Sample Frequency (bits36-39)

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: in DVI mode

    \note       The consumer use bit (bit0) and Mode bits (bits6-7) are forced to zero.
		Use tmbslTDA9989AudioOutSetChanStatusMapping to set CS Byte 2.

 */
	tmErrorCode_t
	    tmbslTDA9989AudioOutSetChanStatus
	    (tmUnitSelect_t txUnit,
	     tmbslHdmiTxAudioData_t pcmIdentification,
	     tmbslHdmiTxCSformatInfo_t formatInfo,
	     tmbslHdmiTxCScopyright_t copyright,
	     UInt8 categoryCode,
	     tmbslHdmiTxafs_t sampleFreq,
	     tmbslHdmiTxCSclkAcc_t clockAccuracy,
	     tmbslHdmiTxCSmaxWordLength_t maxWordLength,
	     tmbslHdmiTxCSwordLength_t wordLength, tmbslHdmiTxCSorigAfs_t origSampleFreq);

/**
    \brief      Set the Channel Status Byte2 for Audio Port 0

    \param[in]  txUnit              Transmitter unit number
    \param[in]  sourceLeft          L Source Number: 0 don't take into account, 1-15
    \param[in]  channelLeft         L Channel Number: 0 don't take into account, 1-15
    \param[in]  sourceRight         R Source Number: 0 don't take into account, 1-15
    \param[in]  channelRight        R Channel Number: 0 don't take into account, 1-15
    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: in DVI mode

    \note       Use tmbslTDA9989AudioOutSetChanStatus to set all other CS bytes
		This function only sets the mapping for Audio Port 0.

 */
	tmErrorCode_t
	    tmbslTDA9989AudioOutSetChanStatusMapping
	    (tmUnitSelect_t txUnit,
	     UInt8 sourceLeft[4], UInt8 channelLeft[4], UInt8 sourceRight[4], UInt8 channelRight[4]
	    );

/**
    \brief      Mute or un-mute the audio output by controlling the audio FIFO,
		in HDMI mode only

    \param[in]  txUnit      Transmitter unit number
    \param[in]  aMute       Audio mute: On, Off

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: in DVI mode

    \note       tmbslTDA9989PktSetGeneralCntrl must be used to control the audio
		mute in outgoing data island packets

 */
	tmErrorCode_t tmbslTDA9989AudioOutSetMute(tmUnitSelect_t txUnit, tmbslHdmiTxaMute_t aMute);


/*============================================================================*/
/**
    \brief      Disable an HDMI Transmitter output and destroy its driver
		instance

    \param[in]  txUnit Transmitter unit number

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: the unit is not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
 */
	tmErrorCode_t tmbslTDA9989Deinit(tmUnitSelect_t txUnit);


/**
    \brief      Get supported audio format(s) from previously read EDID

    \param[in]  txUnit      Transmitter unit number
    \param[out] pEdidAFmts  Pointer to the array of structures to receive the
			    supported Short Audio Descriptors
    \param[in]  aFmtLength  Number of SADs supported in buffer pEdidAFmts,
			    up to HDMI_TX_SAD_MAX_CNT
    \param[out] pAFmtsAvail Pointer to receive the number of SADs available
    \param[out] pAudioFlags Pointer to the byte to receive the Audio Capability
			    Flags

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus
		  - TMBSL_ERR_HDMI_RESOURCE_NOT_AVAILABLE : EDID not read
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized

    \note \verbatim
		Supported Short Audio Descriptors array:
		EdidAFmts[0].ModeChans      SAD 1  - Mode byte
		EdidAFmts[0].Freqs	        SAD 1  - Frequencies byte
		EdidAFmts[0].Byte3          SAD 1  - Byte 3
		...
		EdidAFmts[n-1].ModeChans    SAD n  - Mode byte
		EdidAFmts[n-1].Freqs	    SAD n  - Frequencies byte
		EdidAFmts[n-1].Byte3        SAD n  - Byte 3
		(Where n is the smaller of aFmtLength and pAFmtAvail)
    \endverbatim
 */
	tmErrorCode_t
	    tmbslTDA9989EdidGetAudioCapabilities
	    (tmUnitSelect_t txUnit,
	     tmbslHdmiTxEdidSad_t *pEdidAFmts,
	     UInt aFmtLength, UInt *pAFmtsAvail, UInt8 *pAudioFlags);

/*============================================================================*/
/**
    \brief      Get the EDID block count

    \param[in]  txUnit              Transmitter unit number
    \param[out] puEdidBlockCount    Pointer to data byte in which to return the
				    block count

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus
		  - TMBSL_ERR_HDMI_RESOURCE_NOT_AVAILABLE : EDID not read
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
 */
	tmErrorCode_t
	    tmbslTDA9989EdidGetBlockCount(tmUnitSelect_t txUnit, UInt8 *puEdidBlockCount);

/*============================================================================*/
/**
    \brief      Get the EDID status

    \param[in]  txUnit              Transmitter unit number
    \param[out] puEdidBlockCount    Pointer to data byte in which to return the
				    edid status

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
 */
	tmErrorCode_t tmbslTDA9989EdidGetStatus(tmUnitSelect_t txUnit, UInt8 *puEdidStatus);

/**
    \brief      Request read of EDID blocks from the sink device via DDC
		function not synchronous edid will available on
		EDID_BLK_READ callback


    \param[in]  txUnit      Transmitter unit number
    \param[out] pRawEdid    Pointer to a buffer supplied by the caller to accept
			    the raw EDID data.
    \param[in]  numBlocks   Number of blocks to read
    \param[in]  lenRawEdid  Length in bytes of the supplied buffer
    \param[out] pEdidStatus Pointer to status value E_EDID_READ or E_EDID_ERROR
			    valid only when the return value is TM_OK

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_RESOURCE_NOT_AVAILABLE: EDID not read

    \note       NA

 */

	tmErrorCode_t tmbslTDA9989EdidRequestBlockData(tmUnitSelect_t txUnit, UInt8 *pRawEdid, Int numBlocks,	/* Only relevant if pRawEdid valid */
						       Int lenRawEdid	/* Only relevant if pRawEdid valid */
	    );


/**
    \brief      Get Sink Type by analysis of EDID content

    \param[in]  txUnit      Transmitter unit number
    \param[out] pSinkType   Pointer to returned Sink Type: DVI or HDMI

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus
		  - TMBSL_ERR_HDMI_RESOURCE_NOT_AVAILABLE : edid not read
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_NULL_CONNECTION: HPD pin is inactive

    \sa         tmbslTDA9989EdidGetBlockData
 */
	tmErrorCode_t
	    tmbslTDA9989EdidGetSinkType(tmUnitSelect_t txUnit, tmbslHdmiTxSinkType_t *pSinkType);

/*============================================================================*/
/**
    \brief      Get Source Physical Address by analysis of EDID content

    \param[in]  txUnit          Transmitter unit number
    \param[out] pSourceAddress  Pointer to returned Source Physical Address (ABCDh)

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus
		  - TMBSL_ERR_HDMI_RESOURCE_NOT_AVAILABLE : EDID not read
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized

    \sa         tmbslTDA9989EdidGetBlockData
 */
	tmErrorCode_t
	    tmbslTDA9989EdidGetSourceAddress(tmUnitSelect_t txUnit, UInt16 *pSourceAddress);

/**
    \brief      Get supported video format(s) from previously read EDID

    \param[in]  txUnit      Transmitter unit number
    \param[out] pEdidVFmts  Pointer to the array to receive the supported Short
			    Video Descriptors
    \param[in]  vFmtLength  Number of SVDs supported in buffer pEdidVFmts,
			    up to HDMI_TX_SVD_MAX_CNT
    \param[out] pVFmtsAvail Pointer to receive the number of SVDs available
    \param[out] pVidFlags   Ptr to the byte to receive Video Capability Flags
			    b7: underscan supported
			    b6: YCbCr 4:4:4 supported
			    b5: YCbCr 4:2:2 supported

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus
		  - TMBSL_ERR_HDMI_RESOURCE_NOT_AVAILABLE : EDID not read
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_NULL_CONNECTION: HPD pin is inactive

    \note \verbatim
		Supported Short Video Descriptors array:
		    (HDMI_TX_SVD_NATIVE_MASK bit set to indicate native format)
		EdidVFmts[0]   EIA/CEA Short Video Descriptor 1, or 0
		...
		EdidVFmts[n-1]  EIA/CEA Short Video Descriptor 32, or 0
		(Where n is the smaller of vFmtLength and pVFmtAvail)
    \endverbatim
    \sa         tmbslTDA9989EdidGetBlockData
 */
	tmErrorCode_t
	    tmbslTDA9989EdidGetVideoCapabilities
	    (tmUnitSelect_t txUnit,
	     UInt8 *pEdidVFmts, UInt vFmtLength, UInt *pVFmtsAvail, UInt8 *pVidFlags);
/**
    \brief      Get detailed timing descriptor from previously read EDID

    \param[in]  txUnit      Transmitter unit number
    \param[out] pEdidDTD    Pointer to the array to receive the Detailed timing descriptor

    \param[in]  nb_size     Number of DTD supported in buffer pEdidDTD

    \param[out] pDTDAvail Pointer to receive the number of DTD available

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus
		  - TMBSL_ERR_HDMI_RESOURCE_NOT_AVAILABLE : EDID not read
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
*/
	tmErrorCode_t
	    tmbslTDA9989EdidGetDetailedTimingDescriptors
	    (tmUnitSelect_t txUnit,
	     tmbslHdmiTxEdidDtd_t *pEdidDTD, UInt8 nb_size, UInt8 *pDTDAvail);

/**
    \brief      Get detailed Monitor descriptor from previously read EDID

    \param[in]  txUnit          Transmitter unit number
    \param[out] pEdidFirstMD    Pointer to the First Monitor descriptor
    \param[out] pEdidSecondMD   Pointer to the Second Monitor descriptor
    \param[out] pEdidOtherMD    Pointer to the Other Monitor descriptor
    \param[in]  sizeOtherMD     Not used
    \param[out] pOtherMDAvail   Not used

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus
		  - TMBSL_ERR_HDMI_RESOURCE_NOT_AVAILABLE : EDID not read
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
*/
	tmErrorCode_t
	    tmbslTDA9989EdidGetMonitorDescriptors
	    (tmUnitSelect_t txUnit,
	     tmbslHdmiTxEdidFirstMD_t *pEdidFirstMD,
	     tmbslHdmiTxEdidSecondMD_t *pEdidSecondMD,
	     tmbslHdmiTxEdidOtherMD_t *pEdidOtherMD, UInt8 sizeOtherMD, UInt8 *pOtherMDAvail);

/**
    \brief      Get basic display parameters from previously read EDID

    \param[in]  txUnit          Transmitter unit number
    \param[out] pEdidBDParam    Pointer to the Basic Display Parameters

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_RESOURCE_NOT_AVAILABLE : EDID not read
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
*/
	tmErrorCode_t
	    tmbslTDA9989EdidGetBasicDisplayParam
	    (tmUnitSelect_t txUnit, tmbslHdmiTxEdidBDParam_t *pEdidBDParam);


/**
    \brief      Get preferred video format from previously read EDID

    \param[in]  txUnit      Transmitter unit number
    \param[out] pEdidDTD    Pointer to the structure to receive the Detailed
			    Timing Descriptor parameters of the preferred video
			    format

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus
		  - TMBSL_ERR_HDMI_RESOURCE_NOT_AVAILABLE : EDID not read
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_NULL_CONNECTION: HPD pin is inactive

    \note \verbatim
		Detailed Timing Descriptor parameters output structure:
		UInt16 uPixelClock      Pixel Clock (MHz/10,000)
		UInt16 uHActivePixels   Horizontal Active Pixels
		UInt16 uHBlankPixels    Horizontal Blanking Pixels
		UInt16 uVActiveLines    Vertical Active Lines
		UInt16 uVBlankLines     Vertical Blanking Lines
		UInt16 uHSyncOffset     Horizontal Sync Offset (Pixels)
		UInt16 uHSyncWidth      Horizontal Sync Pulse Width (Pixels)
		UInt16 uVSyncOffset     Vertical Sync Offset (Lines)
		UInt16 uVSyncWidth      Vertical Sync Pulse Width (Lines)
		UInt16 uHImageSize      Horizontal Image Size (mm)
		UInt16 uVImageSize      Vertical Image Size (mm)
		UInt16 uHBorderPixels   Horizontal Border (Pixels)
		UInt16 uVborderPixels   Vertical Border (Pixels)
		UInt8 Flags             Interlace/sync info
    \endverbatim
    \sa         tmbslTDA9989EdidGetBlockData
 */
	tmErrorCode_t
	    tmbslTDA9989EdidGetVideoPreferred
	    (tmUnitSelect_t txUnit, tmbslHdmiTxEdidDtd_t *pEdidDTD);

/**
    \brief      Check the result of an HDCP encryption attempt, called at
		intervals (set by uTimeSinceLastCallMs) after tmbslTDA9989HdcpRun
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED

    \param[in]  txUnit                  Transmitter unit number
    \param[in]  uTimeSinceLastCallMs    Time in ms since this was last called
    \param[out] pResult                 The outcome of the check

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing the I2C bus
		  - TMBSL_ERR_HDMI_NOT_SUPPORTED: device does not support HDCP
 */
	tmErrorCode_t
	    tmbslTDA9989HdcpCheck
	    (tmUnitSelect_t txUnit, UInt16 uTimeSinceLastCallMs, tmbslHdmiTxHdcpCheck_t *pResult);

/**
    \brief      Configure various HDCP parameters
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED

    \param[in]  txUnit          Transmitter unit number
    \param[in]  slaveAddress    DDC I2C slave address
    \param[in]  txMode          Mode of our transmitter device
    \param[in]  options         Options flags to control behaviour of HDCP
    \param[in]  uCheckIntervalMs HDCP check interval in milliseconds
    \param[in]  uChecksToDo     Number of HDCP checks to do after HDCP starts
				A value of 2 or more is valid for checking
				May be set to 0 to disabling checking

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing the I2C bus
		  - TMBSL_ERR_HDMI_NOT_SUPPORTED: device does not support HDCP

    \note       Must be called before all other HDCP APIs
 */
	tmErrorCode_t
	    tmbslTDA9989HdcpConfigure
	    (tmUnitSelect_t txUnit,
	     UInt8 slaveAddress,
	     tmbslHdmiTxHdcpTxMode_t txMode,
	     tmbslHdmiTxHdcpOptions_t options, UInt16 uCheckIntervalMs, UInt8 uChecksToDo);

/**
    \brief      Download keys and AKSV data from OTP memory to the device
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED


    \param[in]  txUnit          Transmitter unit number
    \param[in]  seed            Seed value
    \param[in]  keyDecryption   State of key decryption 0 to 1 (disabled, enabled)

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing the I2C bus
		  - TMBSL_ERR_HDMI_NOT_SUPPORTED: device does not support HDCP
 */
	tmErrorCode_t
	    tmbslTDA9989HdcpDownloadKeys
	    (tmUnitSelect_t txUnit, UInt16 seed, tmbslHdmiTxDecrypt_t keyDecryption);

/*============================================================================*/
/**
    \brief      Switch HDCP encryption on or off without disturbing Infoframes
		(Not normally used)
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED

    \param[in]  txUnit  Transmitter unit number
    \param[in]  bOn     Encryption state: 1=on, 0=off

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing the I2C bus
		  - TMBSL_ERR_HDMI_NOT_SUPPORTED: device does not support HDCP
*/
	tmErrorCode_t tmbslTDA9989HdcpEncryptionOn(tmUnitSelect_t txUnit, Bool bOn);

/*============================================================================*/
/**
    \brief      Get HDCP OTP registers
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED

    \param[in]  txUnit      Transmitter unit number
    \param[in]  otpAddress  OTP start address 0-FF
    \param[out] pOtpData    Ptr to a three-byte array to hold the data read:
			    [0] = OTP_DATA_MSB
			    [1] = OTP_DATA_ISB
			    [2] = OTP_DATA_LSB

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing the I2C bus
		  - TMBSL_ERR_HDMI_NOT_SUPPORTED: device does not support HDCP
*/
	tmErrorCode_t
	    tmbslTDA9989HdcpGetOtp(tmUnitSelect_t txUnit, UInt8 otpAddress, UInt8 *pOtpData);

/*============================================================================*/
/**
    \brief      Return the failure state that caused the last T0 interrupt
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED

    \param[in]  txUnit      Transmitter unit number
    \param[out] pFailState  Ptr to the unit's last T0 fail state

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_NOT_SUPPORTED: device does not support HDCP
*/
	tmErrorCode_t tmbslTDA9989HdcpGetT0FailState(tmUnitSelect_t txUnit, UInt8 *pFailState);

/*============================================================================*/
/**
    \brief      Handle BCAPS interrupt
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED


    \param[in]  txUnit  Transmitter unit number

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing the I2C bus

    \note       The user BCAPS interrupt handler (registered with
		tmbslTDA9989Init) calls this API before calling
		tmbslTDA9989HdcpHandleBKSV
*/
	tmErrorCode_t tmbslTDA9989HdcpHandleBCAPS(tmUnitSelect_t txUnit);

/*============================================================================*/
/**
    \brief      Read BKSV registers
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED

    \param[in]  txUnit          Transmitter unit number
    \param[out] pBksv           Pointer to 5-byte BKSV array returned to caller
				(1st byte is MSB)
    \param[out] pbCheckRequired Pointer to a result variable to tell the caller
				whether to check for BKSV in a revocation list:
				0 or 1 (check not required, check required)

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing the I2C bus

    \note       The user BCAPS interrupt handler (registered with
		tmbslTDA9989Init) calls this API after calling
		tmbslTDA9989HdcpHandleBCAPS
*/
	tmErrorCode_t
	    tmbslTDA9989HdcpHandleBKSV
	    (tmUnitSelect_t txUnit, UInt8 *pBksv, Bool *pbCheckRequired);

/*============================================================================*/
/**
    \brief      Declare BKSV result to be secure or not secure
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED


    \param[in]  txUnit          Transmitter unit number
    \param[in]  bSecure         Result of user's check of BKSV against a
				revocation list:
				0 (not secure: BKSV found in revocation list)
				1 (secure: BKSV not found in revocation list)

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing the I2C bus

    \note       The user BCAPS interrupt handler (registered with
		tmbslTDA9989Init) calls this API after calling
		tmbslTDA9989HdcpHandleBKSV
*/
	tmErrorCode_t tmbslTDA9989HdcpHandleBKSVResult(tmUnitSelect_t txUnit, Bool bSecure);

/**
    \brief      Handle BSTATUS interrupt
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED


    \param[in]  txUnit      Transmitter unit number
    \param[out] pBstatus    Pointer to 16-bit BSTATUS value returned to caller

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing the I2C bus

    \note       Called by user's BSTATUS interrupt handler registered with
		tmbslTDA9989Init
*/
	tmErrorCode_t tmbslTDA9989HdcpHandleBSTATUS(tmUnitSelect_t txUnit, UInt16 *pBstatus);

/*============================================================================*/
/**
    \brief      Handle ENCRYPT interrupt
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED

    \param[in]  txUnit  Transmitter unit number

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing the I2C bus

    \note       Called by user's ENCRYPT interrupt handler registered with
		tmbslTDA9989Init
*/
	tmErrorCode_t tmbslTDA9989HdcpHandleENCRYPT(tmUnitSelect_t txUnit);

/*============================================================================*/
/**
    \brief      Handle PJ interrupt
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED


    \param[in]  txUnit  Transmitter unit number

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing the I2C bus

    \note       Called by user's PJ interrupt handler registered with
		tmbslTDA9989Init
*/
	tmErrorCode_t tmbslTDA9989HdcpHandlePJ(tmUnitSelect_t txUnit);

/**
    \brief      Handle SHA-1 interrupt
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED


    \param[in]  txUnit          Transmitter unit number
    \param[in]  maxKsvDevices   Maximum number of 5-byte devices that will fit
				in *pKsvList: 0 to 128 devices
				If 0, no KSV read is done and it is treated as
				secure
    \param[out] pKsvList        Pointer to KSV list array supplied by caller:
				Sets of 5-byte KSVs, 1 per device, 1st byte is
				LSB of 1st device
				May be null if maxKsvDevices is 0
    \param[out] pnKsvDevices    Pointer to number of KSV devices copied to
				*pKsvList: 0 to 128
				If 0, no KSV check is needed and it is treated
				as secure
				May be null if maxKsvDevices is 0
    \param[out] pDepth          Connection tree depth

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_INCONSISTENT_PARAMS: two parameters disagree
		  - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing the I2C bus

    \note       Called by user's SHA-1 interrupt handler registered with
		tmbslTDA9989Init
*/
	tmErrorCode_t
	    tmbslTDA9989HdcpHandleSHA_1
	    (tmUnitSelect_t txUnit,
	     UInt8 maxKsvDevices, UInt8 *pKsvList, UInt8 *pnKsvDevices, UInt8 *pDepth);

/*============================================================================*/
/**
    \brief      Declare KSV list result to be secure or not secure
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED


    \param[in]  txUnit          Transmitter unit number
    \param[in]  bSecure         Result of user's check of KSV list against a
				revocation list:
				0 (not secure: one or more KSVs are in r.list)
				1 (secure: no KSV found in revocation list)

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing the I2C bus

    \note       The user SHA_1 interrupt handler (registered with
		tmbslTDA9989Init) calls this API after calling
		tmbslTDA9989HdcpHandleSHA_1
*/
	tmErrorCode_t tmbslTDA9989HdcpHandleSHA_1Result(tmUnitSelect_t txUnit, Bool bSecure);

/*============================================================================*/
/**
    \brief      Handle T0 interrupt
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED

    \param[in]  txUnit      Transmitter unit number

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing the I2C bus

    \note       Called by user's T0 interrupt handler registered with
		tmbslTDA9989Init
*/
	tmErrorCode_t tmbslTDA9989HdcpHandleT0(tmUnitSelect_t txUnit);

/*============================================================================*/
/**
    \brief      Prepare for HDCP operation
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED

    \param[in]  txUnit      Transmitter unit number
    \param[in]  voutFmt     Video output format
    \param[in]  voutFreq    Vertical output frequency

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing the I2C bus
		  - TMBSL_ERR_HDMI_NOT_SUPPORTED: device does not support HDCP

    \note       Must be called before tmbslTDA9989HdcpRun
*/
	tmErrorCode_t
	    tmbslTDA9989HdcpInit
	    (tmUnitSelect_t txUnit, tmbslHdmiTxVidFmt_t voutFmt, tmbslHdmiTxVfreq_t voutFreq);

/*============================================================================*/
/**
    \brief      Start HDCP operation
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED

    \param[in]  txUnit  Transmitter unit number

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing the I2C bus
		  - TMBSL_ERR_HDMI_NOT_SUPPORTED: device does not support HDCP

    \note       Must be called after tmbslTDA9989HdcpInit
*/
	tmErrorCode_t tmbslTDA9989HdcpRun(tmUnitSelect_t txUnit);

/*============================================================================*/
/**
    \brief      Stop HDCP operation, and cease encrypting the output
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED

    \param[in]  txUnit  Transmitter unit number

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing the I2C bus
		  - TMBSL_ERR_HDMI_NOT_SUPPORTED: device does not support HDCP

    \note       This will trigger an Encrypt interrupt
*/
	tmErrorCode_t tmbslTDA9989HdcpStop(tmUnitSelect_t txUnit);
/**
    \brief      Get the hot plug input status last read by tmbslTDA9989Init
		or tmbslTDA9989HwHandleInterrupt

    \param[in]  txUnit          Transmitter unit number
    \param[out] pHotPlugStatus  Pointer to returned Hot Plug Detect status

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
 */
	tmErrorCode_t tmbslTDA9989HotPlugGetStatus(tmUnitSelect_t txUnit, tmbslHdmiTxHotPlug_t *pHotPlugStatus, Bool client	/* Used to determine whether the request comes from the application */
	    );

/**
    \brief      Get the rx sense input status last read by tmbslTDA9989Init
		or tmbslTDA9989HwHandleInterrupt

    \param[in]  txUnit          Transmitter unit number
    \param[out] pRxSenseStatus  Pointer to returned Rx Sense Detect status

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
 */
	tmErrorCode_t tmbslTDA9989RxSenseGetStatus(tmUnitSelect_t txUnit, tmbslHdmiTxRxSense_t *pRxSenseStatus, Bool client	/* Used to determine whether the request comes from the application */
	    );

/*============================================================================*/
/**
    \brief      Get one or more hardware I2C register values

    \param[in]  txUnit      Transmitter unit number
    \param[in]  regPage     The device register's page: 00h, 01h, 02h, 11h, 12h
    \param[in]  regAddr     The starting register address on the page: 0 to FFh
    \param[out] pRegBuf     Pointer to buffer to receive the register data
    \param[in]  nRegs       Number of contiguous registers to read: 1 to 254

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing the I2C bus
 */
	tmErrorCode_t
	    tmbslTDA9989HwGetRegisters
	    (tmUnitSelect_t txUnit, Int regPage, Int regAddr, UInt8 *pRegBuf, Int nRegs);

/*============================================================================*/
/**
    \brief      Get the transmitter device version read at initialization

    \param[in]  txUnit          Transmitter unit number
    \param[out] puDeviceVersion Pointer to returned hardware version

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
 */
	tmErrorCode_t tmbslTDA9989HwGetVersion(tmUnitSelect_t txUnit, UInt8 *puDeviceVersion);


/**
    \brief      Get the transmitter device feature read at initialization

    \param[in]  txUnit            Transmitter unit number
    \param[in]  deviceFeature     Hardware feature to check
    \param[out]  pFeatureSupported Hardware feature supported or not

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus
 */
	tmErrorCode_t
	    tmbslTDA9989HwGetCapabilities
	    (tmUnitSelect_t txUnit,
	     tmbslHdmiTxHwFeature_t deviceCapability, Bool *pFeatureSupported);


/*============================================================================*/
/**
    \brief      Handle all hardware interrupts from a transmitter unit

    \param[in]  txUnit      Transmitter unit number

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus
    \note       This function must be called at task level not interrupt level,
		as I2C access is required
 */
	tmErrorCode_t tmbslTDA9989HwHandleInterrupt(tmUnitSelect_t txUnit);


/*============================================================================*/
/**
    \brief      Set one or more hardware I2C registers

    \param[in]  txUnit      Transmitter unit number
    \param[in]  regPage     The device register's page: 00h, 01h, 02h, 11h, 12h
    \param[in]  regAddr     The starting register address on the page: 0 to FFh
    \param[in]  pRegBuf     Ptr to buffer from which to write the register data
    \param[in]  nRegs       Number of contiguous registers to write: 0 to 254.
			    The page register (255) may not be written - it is
			    written to automatically here. If nRegs is 0, the
			    page register is the only register written.

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
 */
	tmErrorCode_t
	    tmbslTDA9989HwSetRegisters
	    (tmUnitSelect_t txUnit, Int regPage, Int regAddr, UInt8 *pRegBuf, Int nRegs);


/*============================================================================*/
/**
    \brief      Handle hardware startup by resetting Device Instance Data
 */
	void
	 tmbslTDA9989HwStartup(void
	    );

/**
    \brief      Create an instance of an HDMI Transmitter: initialize the
		driver, reset the transmitter device and get the current
		Hot Plug state

    \param[in]  txUnit           Transmitter unit number
    \param[in]  uHwAddress       Device I2C slave address
    \param[in]  sysFuncWrite     System function to write I2C
    \param[in]  sysFuncRead      System function to read I2C
    \param[in]  sysFuncEdidRead  System function to read EDID blocks via I2C
    \param[in]  sysFuncTimer     System function to run a timer
    \param[in]  funcIntCallbacks Pointer to interrupt callback function list
				 The list pointer is null for no callbacks;
				 each pointer in the list may also be null.
    \param[in]  bEdidAltAddr     Use alternative i2c address for EDID data
				 register between Driver and TDA9983/2:
				 0 - use default address (A0)
				 1 - use alternative address (A2)
    \param[in]  vinFmt           EIA/CEA Video input format: 1 to 31, 0 = No Change
    \param[in]  pixRate          Single data (repeated or not) or double data rate

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: the unit number is wrong or
		    the transmitter instance is already initialised
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter is invalid or out
		    of range
		  - TMBSL_ERR_HDMI_INIT_FAILED: the unit instance is already
		    initialised
		  - TMBSL_ERR_HDMI_COMPATIBILITY: the driver is not compatiable
		    with the internal device version code
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus
 */
	 tmErrorCode_t
	    tmbslTDA9989Init
	    (tmUnitSelect_t txUnit,
	     UInt8 uHwAddress,
	     ptmbslHdmiTxSysFunc_t sysFuncWrite,
	     ptmbslHdmiTxSysFunc_t sysFuncRead,
	     ptmbslHdmiTxSysFuncEdid_t sysFuncEdidRead,
	     ptmbslHdmiTxSysFuncTimer_t sysFuncTimer,
	     tmbslHdmiTxCallbackList_t *funcIntCallbacks,
	     Bool bEdidAltAddr, tmbslHdmiTxVidFmt_t vinFmt, tmbslHdmiTxPixRate_t pixRate);

/**
    \brief      Set colour space converter matrix coefficients

    \param[in]  txUnit      Transmitter unit number
    \param[in]  pMatCoeff   Pointer to Matrix Coefficient structure

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus

    \note       Matrix Coefficient parameter structure:
		Int16 Coeff[9]: Array of coefficients (values -1024 to +1023)
 */
	 tmErrorCode_t
	    tmbslTDA9989MatrixSetCoeffs(tmUnitSelect_t txUnit, tmbslHdmiTxMatCoeff_t *pMatCoeff);

/**
    \brief      Set colour space conversion using preset values

    \param[in]  txUnit          Transmitter unit number
    \param[in]  vinFmt          Input video format
    \param[in]  vinMode         Input video mode
    \param[in]  voutFmt         Output video format
    \param[in]  voutMode        Output video mode

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
 */
	 tmErrorCode_t
	    tmbslTDA9989MatrixSetConversion
	    (tmUnitSelect_t txUnit,
	     tmbslHdmiTxVidFmt_t vinFmt,
	     tmbslHdmiTxVinMode_t vinMode,
	     tmbslHdmiTxVidFmt_t voutFmt, tmbslHdmiTxVoutMode_t voutMode, tmbslHdmiTxVQR_t dviVqr);

/**
    \brief      Set colour space converter matrix offset at input

    \param[in]  txUnit      Transmitter unit number
    \param[in]  pMatOffset  Pointer to Matrix Offset structure

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized

    \note       Matrix Offset structure parameter structure:
		Int16 Offset[3]: Offset array (values -1024 to +1023)
 */
	 tmErrorCode_t
	    tmbslTDA9989MatrixSetInputOffset
	    (tmUnitSelect_t txUnit, tmbslHdmiTxMatOffset_t *pMatOffset);

/**
    \brief      Set colour space converter matrix mode

    \param[in]  txUnit      Transmitter unit number
    \param[in]  mControl    Matrix Control: On, Off, No change
    \param[in]  mScale      Matrix Scale Factor: 1/256, 1/512, 1/1024, No change

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized

    \note       NA

    \sa         NA
 */
	 tmErrorCode_t
	    tmbslTDA9989MatrixSetMode
	    (tmUnitSelect_t txUnit, tmbslHdmiTxmCntrl_t mControl, tmbslHdmiTxmScale_t mScale);

/*============================================================================*/
/**
    \brief      Set colour space converter matrix offset at output

    \param[in]  txUnit      Transmitter unit number
    \param[in]  pMatOffset  Pointer to Matrix Offset structure

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized

    \note       Matrix Offset parameter structure:
		nt16 Offset[3]: Offset array (values -1024 to +1023)
 */
	 tmErrorCode_t
	    tmbslTDA9989MatrixSetOutputOffset
	    (tmUnitSelect_t txUnit, tmbslHdmiTxMatOffset_t *pMatOffset);

/*============================================================================*/
/**
    \brief      Enable audio clock recovery packet insertion

    \param[in]  txUnit      Transmitter unit number
    \param[in]  bEnable     Enable or disable packet insertion

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: when in DVI mode

    \note       tmbslTDA9989AudioInSetCts sets CTS and N values
 */
	 tmErrorCode_t tmbslTDA9989PktSetAclkRecovery(tmUnitSelect_t txUnit, Bool bEnable);

/**
    \brief      Set audio content protection packet & enable/disable packet
		insertion

    \param[in]  txUnit      Transmitter unit number
    \param[in]  pPkt        Pointer to Data Island Packet structure
    \param[in]  byteCnt     Packet buffer byte count
    \param[in]  uAcpType    Content protection type
    \param[in]  bEnable     Enable or disable packet insertion

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_INCONSISTENT_PARAMS: pointer suppied with byte count of zero
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_NOT_SUPPORTED: not possible with this device
		  - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

    \note       Data Island Packet parameter structure:
		UInt8 dataByte[28]      Packet Data

    \sa         NA
 */
	 tmErrorCode_t
	    tmbslTDA9989PktSetAcp
	    (tmUnitSelect_t txUnit,
	     tmbslHdmiTxPkt_t *pPkt, UInt byteCnt, UInt8 uAcpType, Bool bEnable);

/**
    \brief      Set audio info frame packet & enable/disable packet insertion

    \param[in]  txUnit  Transmitter unit number
    \param[in]  pPkt    Pointer to Audio Infoframe structure
    \param[in]  bEnable Enable or disable packet insertion

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

    \note       Audio Infoframe structure:
		UInt8 CodingType
		UInt8 ChannelCount
		UInt8 SampleFreq
		UInt8 SampleSize
		UInt8 ChannelAlloc
		Bool DownMixInhibit
		UInt8 LevelShift
 */
	 tmErrorCode_t
	    tmbslTDA9989PktSetAudioInfoframe
	    (tmUnitSelect_t txUnit, tmbslHdmiTxPktAif_t *pPkt, Bool bEnable);


/*============================================================================*/
/**
    \brief      Set contents of general control packet & enable/disable
		packet insertion

    \param[in]  txUnit      Transmitter unit number
    \param[in]  paMute      Pointer to Audio Mute; if Null, no change to packet
			    contents is made
    \param[in]  bEnable     Enable or disable packet insertion

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

    \note       tmbslTDA9989AudioOutSetMute must be used to mute the audio output
 */
	 tmErrorCode_t
	    tmbslTDA9989PktSetGeneralCntrl
	    (tmUnitSelect_t txUnit, tmbslHdmiTxaMute_t *paMute, Bool bEnable);

/*============================================================================*/
/**
    \brief      Set ISRC1 packet & enable/disable packet insertion

    \param[in]  txUnit      Transmitter unit number
    \param[in]  pPkt        Pointer to Data Island Packet structure
    \param[in]  byteCnt     Packet buffer byte count
    \param[in]  bIsrcCont   ISRC continuation flag
    \param[in]  bIsrcValid  ISRC valid flag
    \param[in]  uIsrcStatus ISRC Status
    \param[in]  bEnable     Enable or disable packet insertion

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_INCONSISTENT_PARAMS: pointer suppied with byte count of zero
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_NOT_SUPPORTED: not possible with this device
		  - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

    \note       Data Island Packet parameter structure:
		UInt8 dataByte[28]  Packet Data

    \sa         NA
 */
	 tmErrorCode_t
	    tmbslTDA9989PktSetIsrc1
	    (tmUnitSelect_t txUnit,
	     tmbslHdmiTxPkt_t *pPkt,
	     UInt byteCnt, Bool bIsrcCont, Bool bIsrcValid, UInt8 uIsrcStatus, Bool bEnable);

/*============================================================================*/
/**
    \brief      Set ISRC2 packet & enable/disable packet insertion

    \param[in]  txUnit      Transmitter unit number
    \param[in]  pPkt        Pointer to Data Island Packet structure
    \param[in]  byteCnt     Packet buffer byte count
    \param[in]  bEnable     Enable or disable packet insertion

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_INCONSISTENT_PARAMS: pointer suppied with byte count of zero
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_NOT_SUPPORTED: not possible with this device
		  - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

    \note       Data Island Packet parameter structure:
		UInt8 dataByte[28]      Packet Data

    \sa         NA
 */
	 tmErrorCode_t
	    tmbslTDA9989PktSetIsrc2
	    (tmUnitSelect_t txUnit, tmbslHdmiTxPkt_t *pPkt, UInt byteCnt, Bool bEnable);

/**
    \brief      Set MPEG infoframe packet & enable/disable packet insertion

    \param[in]  txUnit          Transmitter unit number
    \param[in]  pPkt            Pointer to MPEG Infoframe structure
    \param[in]  bEnable         Enable or disable packet insertion

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_NOT_SUPPORTED: not possible with this device
		  - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

    \note       MPEG Infoframe structure:
		UInt32                  bitRate
		tmbslHdmiTxMpegFrame_t  frameType
		Bool                    bFieldRepeat

    \sa         NA
 */
	 tmErrorCode_t
	    tmbslTDA9989PktSetMpegInfoframe
	    (tmUnitSelect_t txUnit, tmbslHdmiTxPktMpeg_t *pPkt, Bool bEnable);

/*============================================================================*/
/**
    \brief      Enable NULL packet insertion

    \param[in]  txUnit      Transmitter unit number
    \param[in]  bEnable     Enable or disable packet insertion

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode
 */
	 tmErrorCode_t tmbslTDA9989PktSetNullInsert(tmUnitSelect_t txUnit, Bool bEnable);

/*============================================================================*/
/**
    \brief      Set single Null packet insertion (flag auto-resets after
		transmission)

    \param[in]  txUnit      Transmitter unit number

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

    \note       Operation resets after single transmission
 */
	 tmErrorCode_t tmbslTDA9989PktSetNullSingle(tmUnitSelect_t txUnit);

/**
    \brief      Set audio info frame packet & enable/disable packet insertion

    \param[in]  txUnit  Transmitter unit number
    \param[in]  pPkt    Pointer to Audio Infoframe structure
    \param[in]  bEnable Enable or disable packet insertion

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_NOT_SUPPORTED: not possible with this device
		  - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

    \note       Audio Infoframe structure:
		UInt8                   VendorName[8]
		UInt8                   ProdDescr[16]
		tmbslHdmiTxSourceDev_t  SourceDevInfo
 */
	 tmErrorCode_t
	    tmbslTDA9989PktSetSpdInfoframe
	    (tmUnitSelect_t txUnit, tmbslHdmiTxPktSpd_t *pPkt, Bool bEnable);

/**
    \brief      Set video infoframe packet & enable/disable packet insertion

    \param[in]  txUnit  Transmitter unit number
    \param[in]  pPkt    Pointer to Video Infoframe structure
    \param[in]  bEnable Enable or disable packet insertion

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

    \note       Video Infoframe structure:
		UInt8 Colour
		Bool ActiveInfo
		UInt8 BarInfo
		UInt8 ScanInfo
		UInt8 Colorimetry
		UInt8 PictureAspectRatio
		UInt8 ActiveFormatRatio
		UInt8 Scaling
		UInt8 VidFormat
		UInt8 PixelRepeat
		UInt16 EndTopBarLine
		UInt16 StartBottomBarLine
		UInt16 EndLeftBarPixel
		UInt16 StartRightBarPixel   (incorrectly named in [HDMI1.2])
 */
	 tmErrorCode_t
	    tmbslTDA9989PktSetVideoInfoframe
	    (tmUnitSelect_t txUnit, tmbslHdmiTxPktVif_t *pPkt, Bool bEnable);

/*============================================================================*/
/**
    \brief      Set Vendor Specific Infoframe packet & enable/disable packet insertion

    \param[in]  txUnit      Transmitter unit number
    \param[in]  pPkt        Pointer to Data Island Packet structure
    \param[in]  byteCnt     Packet buffer byte count
    \param[in]  uVersion    Version number for packet header
    \param[in]  bEnable     Enable or disable packet insertion

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_INCONSISTENT_PARAMS: pointer suppied with byte count of zero
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_NOT_SUPPORTED: not possible with this device
		  - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

    \note       Data Island Packet parameter structure:
		UInt8 dataByte[28]      Packet Data (only use 27 bytes max)

    \sa         NA
 */
	 tmErrorCode_t
	    tmbslTDA9989PktSetVsInfoframe
	    (tmUnitSelect_t txUnit,
	     tmbslHdmiTxPkt_t *pPkt, UInt byteCnt, UInt8 uVersion, Bool bEnable);

/*============================================================================*/
/**
    \brief      Set raw video Infoframe packet & enable/disable packet insertion

    \param[in]  txUnit      Transmitter unit number
    \param[in]  pPkt        Pointer to raw Packet structure
    \param[in]  bEnable     Enable or disable packet insertion

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED: not allowed in DVI mode

    \note       Data Island Packet parameter structure:
		UInt8 dataByte[28]      Packet Data

    \sa         NA
 */
	tmErrorCode_t tmbslTDA9989PktSetRawVideoInfoframe
	    (tmUnitSelect_t txUnit, tmbslHdmiTxPktRawAvi_t *pPkt, Bool bEnable);


/*============================================================================*/
/**
    \brief      Get the power state of the transmitter

    \param[in]  txUnit       Transmitter unit number
    \param[out] pePowerState Pointer to the power state of the device now

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized

    \note       Power states:
		- tmPowerOn
		- tmPowerStandby
 */
	 tmErrorCode_t
	    tmbslTDA9989PowerGetState(tmUnitSelect_t txUnit, tmPowerState_t *pePowerState);

/*============================================================================*/
/**
    \brief      Set the power state of the transmitter

    \param[in]  txUnit      Transmitter unit number
    \param[in]  ePowerState Power state to set

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus

    \note       Power states (Off and Suspend are treated the same as Standby):
		- tmPowerOn
		- tmPowerStandby
		- tmPowerSuspend
		- tmPowerOff
 */
	 tmErrorCode_t tmbslTDA9989PowerSetState(tmUnitSelect_t txUnit, tmPowerState_t ePowerState);


/*============================================================================*/
/**
    \brief      Reset the HDMI transmitter

    \param[in]  txUnit      Transmitter unit number

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus

    \note       NA

    \sa         tmbslTDA9989Init
 */
	 tmErrorCode_t tmbslTDA9989Reset(tmUnitSelect_t txUnit);

/**
    \brief      Get diagnostic counters from the scaler
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED

    \param[in]  txUnit      Transmitter unit number
    \param[out] pScalerDiag Pointer to structure to receive scaler diagnostic
			    registers

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_READ: failed when reading the I2C bus

    \note       scaler diagnostic registers structure:
		UInt16 maxBuffill_p     Filling primary video buffer
		UInt16 maxBuffill_d     Filling video deinterlaced buffer
		UInt8  maxFifofill_pi   Filling primary video input FIFO
		UInt8  minFifofill_po1  Filling primary video output FIFO #1
		UInt8  minFifofill_po2  Filling primary video output FIFO #2
		UInt8  minFifofill_po3  Filling primary video output FIFO #3
		UInt8  minFifofill_po4  Filling primary video output FIFO #4
		UInt8  maxFifofill_di   Filling deinterlaced video input FIFO
		UInt8  maxFifofill_do   Filling deinterlaced video output FIFO
 */
	 tmErrorCode_t
	    tmbslTDA9989ScalerGet(tmUnitSelect_t txUnit, tmbslHdmiTxScalerDiag_t *pScalerDiag);



/**
    \brief      Get the current scaler mode
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED


    \param[in]  txUnit      Transmitter unit number
    \param[out] pScalerMode Pointer to variable to receive scaler mode

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
*/
	 tmErrorCode_t
	    tmbslTDA9989ScalerGetMode(tmUnitSelect_t txUnit, tmbslHdmiTxScaMode_t *pScalerMode);


/*============================================================================*/
/**
    \brief      Enable or disable scaler input frame
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED


    \param[in]  txUnit      Transmitter unit number
    \param[in]  bDisable    Enable or disable scaler input

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
 */
	 tmErrorCode_t tmbslTDA9989ScalerInDisable(tmUnitSelect_t txUnit, Bool bDisable);

/**
    \brief      Set the active coefficient lookup table for the vertical scaler
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED

    \param[in]  txUnit      Transmitter unit number
    \param[in]  lutSel      Coefficient lookup table selection
    \param[in]  pVsLut      Table of HDMITX_VSLUT_COEFF_NUM coefficient values
			    (may be null if lutSel not HDMITX_SCALUT_USE_VSLUT)

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_INCONSISTENT_PARAMS: two parameters disagree
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
 */
	 tmErrorCode_t
	    tmbslTDA9989ScalerSetCoeffs
	    (tmUnitSelect_t txUnit, tmbslHdmiTxScaLut_t lutSel, UInt8 *pVsLut);

/**
    \brief      Set scaler field positions
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED

    \param[in]  txUnit      Transmitter unit number
    \param[in]  topExt      Internal, External, No Change
    \param[in]  deExt       Internal, External, No Change
    \param[in]  topSel      Internal, VRF, No Change
    \param[in]  topTgl      No Action, Toggle, No Change

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
 */
	 tmErrorCode_t
	    tmbslTDA9989ScalerSetFieldOrder
	    (tmUnitSelect_t txUnit,
	     tmbslHdmiTxIntExt_t topExt,
	     tmbslHdmiTxIntExt_t deExt, tmbslHdmiTxTopSel_t topSel, tmbslHdmiTxTopTgl_t topTgl);

/**
    \brief      Set scaler fine adjustment options
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED

    \param[in]  txUnit       Transmitter unit number
    \param[in]  uRefPix      Ref. pixel preset 0 to 1FFFh (2000h = No Change)
    \param[in]  uRefLine     Ref. line preset 0 to 7FFh (800h = No Change)

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
 */
	 tmErrorCode_t
	    tmbslTDA9989ScalerSetFine(tmUnitSelect_t txUnit, UInt16 uRefPix, UInt16 uRefLine);

/**
    \brief      Set scaler phase for scaling 1080p
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED

    \param[in]  txUnit                  Transmitter unit number
    \param[in]  tmbslHdmiTxHPhases_t   Ref. 0 to 15_horizontal_phases 1 to 16_horizontal_phases

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
 */

	 tmErrorCode_t
	    tmbslTDA9989ScalerSetPhase
	    (tmUnitSelect_t txUnit, tmbslHdmiTxHPhases_t horizontalPhases);

/**
    \brief      configure scaler latency to set run in run out
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED

    \param[in]  txUnit      Transmitter unit number
    \param[in]  UInt8       Ref. 0 to 255

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
 */
	 tmErrorCode_t tmbslTDA9989ScalerSetLatency(tmUnitSelect_t txUnit, UInt8 scaler_latency);

/**
    \brief      Set scaler synchronization options
		On a TDA9989 this function is not supported and
		return result TMBSL_ERR_HDMI_NOT_SUPPORTED

    \param[in]  txUnit       Transmitter unit number
    \param[in]  method       Sync. combination method
    \param[in]  once         Line/pixel counters sync once or each frame

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
 */
	 tmErrorCode_t
	    tmbslTDA9989ScalerSetSync
	    (tmUnitSelect_t txUnit, tmbslHdmiTxVsMeth_t method, tmbslHdmiTxVsOnce_t once);


/*============================================================================*/
/**
    \brief      Get the driver software version and compatibility numbers

    \param[out] pSWVersion   Pointer to the software version structure returned

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
 */
	 tmErrorCode_t tmbslTDA9989SwGetVersion(ptmSWVersion_t pSWVersion);


/*============================================================================*/
/**
    \brief      Get the driver software version and compatibility numbers

    \param[in]  txUnit       Transmitter unit number
    \param[in]  waitMs       Period in milliseconds to wait

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
 */
	 tmErrorCode_t tmbslTDA9989SysTimerWait(tmUnitSelect_t txUnit, UInt16 waitMs);

/**
    \brief      Set the TMDS outputs to normal active operation or to a forced
		state

    \param[in]  txUnit      Transmitter unit number
    \param[in]  tmdsOut     TMDS output mode

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
 */
	 tmErrorCode_t
	    tmbslTDA9989TmdsSetOutputs(tmUnitSelect_t txUnit, tmbslHdmiTxTmdsOut_t tmdsOut);

/**
    \brief      Fine-tune the TMDS serializer

    \param[in]  txUnit      Transmitter unit number
    \param[in]  uPhase2     Serializer phase 2
    \param[in]  uPhase3     Serializer phase 3

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
 */
	 tmErrorCode_t
	    tmbslTDA9989TmdsSetSerializer(tmUnitSelect_t txUnit, UInt8 uPhase2, UInt8 uPhase3);

/**
    \brief      Set a colour bar test pattern

    \param[in]  txUnit     Transmitter unit number
    \param[in]  pattern    Test pattern

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
 */
	 tmErrorCode_t
	    tmbslTDA9989TestSetPattern(tmUnitSelect_t txUnit, tmbslHdmiTxTestPattern_t pattern);

/**
    \brief      Set or clear one or more simultaneous test modes

    \param[in]  txUnit      Transmitter unit number
    \param[in]  testMode    Mode: tst_pat, tst_656, tst_serphoe, tst_nosc,
			    tst_hvp, tst_pwd, tst_divoe
    \param[in]  testState   State: 1=On, 0=Off

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
 */
	 tmErrorCode_t
	    tmbslTDA9989TestSetMode
	    (tmUnitSelect_t txUnit,
	     tmbslHdmiTxTestMode_t testMode, tmbslHdmiTxTestState_t testState);

/**
    \brief      Enable blanking between active data

    \param[in]  txUnit          Transmitter unit number
    \param[in]  blankitSource   Blankit Source: Not DE, VS And HS,
				VS And Not HS, Hemb And Vemb, No Change
    \param[in]  blankingCodes   Blanking Codes: All Zero, RGB444, YUV444,
				YUV422, No Change

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus

    \note       NA

    \sa         NA
 */
	 tmErrorCode_t
	    tmbslTDA9989VideoInSetBlanking
	    (tmUnitSelect_t txUnit,
	     tmbslHdmiTxBlnkSrc_t blankitSource, tmbslHdmiTxBlnkCode_t blankingCodes);

/**
    \brief      Configure video input options and control the upsampler

    \param[in]  txUnit          Transmitter unit number
    \param[in]  vinMode         Video input mode
    \param[in]  voutFmt         EIA/CEA Video output format: 1 to 31, 0 = No Change
    \param[in]  sampleEdge      Sample edge:
				Pixel Clock Positive Edge,
				Pixel Clock Negative Edge, No Change
    \param[in]  pixRate         Single data or double data rate
    \param[in]  upsampleMode    Upsample mode

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
 */
	 tmErrorCode_t
	    tmbslTDA9989VideoInSetConfig
	    (tmUnitSelect_t txUnit,
	     tmbslHdmiTxVinMode_t vinMode,
	     tmbslHdmiTxVidFmt_t voutFmt,
	     tmbslHdmiTx3DStructure_t structure3D,
	     tmbslHdmiTxPixEdge_t sampleEdge,
	     tmbslHdmiTxPixRate_t pixRate, tmbslHdmiTxUpsampleMode_t upsampleMode);

/**
    \brief      Set fine image position

    \param[in]  txUnit          Transmitter unit number
    \param[in]  subpacketCount  Subpacket Count fixed values and sync options
    \param[in]  toggleClk1      Toggle clock 1 phase w.r.t. clock 2

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus

    \note       NA

    \sa         NA
 */
	 tmErrorCode_t
	    tmbslTDA9989VideoInSetFine
	    (tmUnitSelect_t txUnit,
	     tmbslHdmiTxPixSubpkt_t subpacketCount, tmbslHdmiTxPixTogl_t toggleClk1);

/**
    \brief      Set video input port swapping and mirroring

    \param[in]  txUnit          Transmitter unit number
    \param[in]  pSwapTable      Pointer to 6-byte port swap table
    \param[in]  pMirrorTable    Pointer to 6-byte port mirror table

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus

    \note       UInt8 pSwapTable[6]

		Each table position 0 to 5 represents a group of 4 port bits:
		[0]=23:20, [1]=16:19, [2]=15:12, [3]=11:8, [4]=4:7, [5]=0:3
		Table position values are 0 to 6, denoting the group of 4 port
		bits to swap to: 0=23:20, 1=16:19, 2=15:12, 3=11:8, 4=4:7, 5=0:3.
		For example, to swap port bits 15:12 to bits 4:7: pSwapTable[2]=4

		UInt8 pMirrorTable[6]

		Each table position 0 to 5 represents a group of 4 port bits:
		[0]=23:20, [1]=16:19, [2]=15:12, [3]=11:8, [4]=4:7, [5]=0:3.
		Cell values are 0 to 2 (Not Mirrored, Mirrored, No Change).
		For example, to mirror port bits 11:8 to bits 8:11:
		pMirrorTable[3]=1.
 */
	 tmErrorCode_t tmbslTDA9989VideoInSetMapping
#ifdef TMFL_RGB_DDR_12BITS
	 (tmUnitSelect_t txUnit, UInt8 *pSwapTable, UInt8 *pMirrorTable, UInt8 *pMux);
#else
	 (tmUnitSelect_t txUnit, UInt8 *pSwapTable, UInt8 *pMirrorTable);
#endif
/**
    \brief      Set video input port (enable, ground)

    \param[in]  txUnit                Transmitter unit number
    \param[in]  pEnaVideoPortTable    Pointer to 3-byte video port enable table
    \param[in]  pGndVideoPortTable    Pointer to 3-byte video port ground table

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus

    \note       UInt8 pEnaVideoPortTable[3]

		Each table position 0 to 2 represents a group of 8 port bits:
		[0]=7:0, [1]=15:8, [2]=23:16
		bitn = '1' means enable port n
				bitn = '0' means disable port n
				For example, to enable port 0 to 7 only : pEnaVideoPortTable[0]= 0xFF
		pEnaVideoPortTable[1]= 0x00, pEnaVideoPortTable[2]= 0x00

				UInt8 pGndVideoPortTable[3]

		Each table position 0 to 2 represents a group of 8 port bits:
		[0]=7:0, [1]=15:8, [2]=23:16
		bitn = '1' means pulldown port n
				bitn = '0' means not pulldown port n
				For example, to pulldown port 8 to 15 only : pEnaVideoPortTable[0]= 0x00
		pEnaVideoPortTable[1]= 0xFF, pEnaVideoPortTable[2]= 0x00
 */
	 tmErrorCode_t
	    tmbslTDA9989SetVideoPortConfig
	    (tmUnitSelect_t txUnit, UInt8 *pEnaVideoPortTable, UInt8 *pGndVideoPortTable);

/*============================================================================*/
/**
    \brief      Set audio input port (enable, ground)

    \param[in]  txUnit                Transmitter unit number
    \param[in]  pEnaAudioPortTable    Pointer to 1-byte audio port enable configuration
    \param[in]  pGndAudioPortTable    Pointer to 1-byte audio port ground configuration

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus

    \note       UInt8 pEnaAudioPortTable[1]
		bitn = '1' means enable port n
				bitn = '0' means disable port n
				For example, to enable all audio port (0:7) : pEnaAudioPortTable[0]= 0xFF

				UInt8 pGndAudioPortTable[1]
		bitn = '1' means pulldown port n
				bitn = '0' means not pulldown port n
				For example, to pulldown audio port (0:7) : pEnaAudioPortTable[0]= 0xFF
*/
	 tmErrorCode_t
	    tmbslTDA9989SetAudioPortConfig
	    (tmUnitSelect_t txUnit, UInt8 *pEnaAudioPortTable, UInt8 *pGndAudioPortTable);

/*============================================================================*/
/**
    \brief      Set audio input Clock port (enable, ground)

    \param[in]  txUnit                Transmitter unit number
    \param[in]  pEnaAudioClockPortTable    Pointer to 1-byte audio Clock port enable configuration
    \param[in]  pGndAudioClockPortTable    Pointer to 1-byte audio Clock port ground configuration

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus

    \note       UInt8 pEnaAudioClockPortTable[1]
		bitn = '1' means enable port n
				bitn = '0' means disable port n
				For example, to enable all audio Clock port (0) : pEnaAudioPortTable[0]= 0x01

				UInt8 pGndAudioClockPortTable[1]
		bitn = '1' means pulldown port n
				bitn = '0' means not pulldown port n
				For example, to pulldown audio Clock port (0:7) : pEnaAudioPortTable[0]= 0x01
*/
	 tmErrorCode_t
	    tmbslTDA9989SetAudioClockPortConfig
	    (tmUnitSelect_t txUnit,
	     UInt8 *pEnaAudioClockPortTable, UInt8 *pGndAudioClockPortTable);

/**
    \brief      Configure video input sync automatically

    \param[in]  txUnit       Transmitter unit number
    \param[in]  syncSource   Sync Source:
			     Embedded, External Vref, External Vs
			     No Change
    \param[in]  vinFmt       EIA/CEA Video input format: 1 to 31, 0 = No Change
    \param[in]  vinMode      Input video mode

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
 */
	 tmErrorCode_t
	    tmbslTDA9989VideoInSetSyncAuto
	    (tmUnitSelect_t txUnit,
	     tmbslHdmiTxSyncSource_t syncSource,
	     tmbslHdmiTxVidFmt_t vinFmt,
	     tmbslHdmiTxVinMode_t vinMode, tmbslHdmiTx3DStructure_t structure3D);

/**
    \brief      Configure video input sync with manual parameters

    \param[in]  txUnit       Transmitter unit number
    \param[in]  syncSource   Sync Source:
			     Embedded, External Vref, External Vs
			     No Change
    \param[in]  syncMethod   Sync method: V And H, V And X-DE, No Change
    \param[in]  toggleV      VS Toggle:
			     No Action, Toggle VS/Vref, No Change
    \param[in]  toggleH      HS Toggle:
			     No Action, Toggle HS/Href, No Change
    \param[in]  toggleX      DE/FREF Toggle:
			     No Action, Toggle DE/Fref, No Change
    \param[in]  uRefPix      Ref. pixel preset 0 to 1FFFh (2000h = No Change)
    \param[in]  uRefLine     Ref. line preset 0 to 7FFh (800h = No Change)

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
 */
	 tmErrorCode_t
	    tmbslTDA9989VideoInSetSyncManual
	    (tmUnitSelect_t txUnit,
	     tmbslHdmiTxSyncSource_t syncSource,
	     tmbslHdmiTxVsMeth_t syncMethod,
	     tmbslHdmiTxPixTogl_t toggleV,
	     tmbslHdmiTxPixTogl_t toggleH,
	     tmbslHdmiTxPixTogl_t toggleX, UInt16 uRefPix, UInt16 uRefLine);


/*============================================================================*/
/**
    \brief      Enable or disable output video frame

    \param[in]  txUnit      Transmitter unit number
    \param[in]  bDisable    Enable or disable scaler input

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
 */
	 tmErrorCode_t tmbslTDA9989VideoOutDisable(tmUnitSelect_t txUnit, Bool bDisable);

/**
    \brief      Configure sink type, configure video output colour and
		quantization, control the downsampler, and force RGB output
		and mute audio in DVI mode

    \param[in]  txUnit          Transmitter unit number:
    \param[in]  sinkType        Sink device type: DVI or HDMI or copy from EDID
    \param[in]  voutMode        Video output mode
    \param[in]  preFilter       Prefilter: Off, 121, 109, CCIR601, No Change
    \param[in]  yuvBlank        YUV blanking: 16, 0, No Change
    \param[in]  quantization    Video quantization range:
				Full Scale, RGB Or YUV, YUV, No Change


    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
 */
	 tmErrorCode_t
	    tmbslTDA9989VideoOutSetConfig
	    (tmUnitSelect_t txUnit,
	     tmbslHdmiTxSinkType_t sinkType,
	     tmbslHdmiTxVoutMode_t voutMode,
	     tmbslHdmiTxVoutPrefil_t preFilter,
	     tmbslHdmiTxVoutYuvBlnk_t yuvBlank, tmbslHdmiTxVoutQrange_t quantization);

/**
    \brief      Set video synchronization

    \param[in]  txUnit      Transmitter unit number
    \param[in]  srcH        Horizontal sync source: Internal, Exter'l, No Change
    \param[in]  srcV        Vertical sync source: Internal, Exter'l, No Change
    \param[in]  srcX        X sync source: Internal, Exter'l, No Change
    \param[in]  toggle      Sync toggle: Hs, Vs, Off, No Change
    \param[in]  once        Line/pixel counters sync once or each frame

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
 */
	 tmErrorCode_t
	    tmbslTDA9989VideoOutSetSync
	    (tmUnitSelect_t txUnit,
	     tmbslHdmiTxVsSrc_t srcH,
	     tmbslHdmiTxVsSrc_t srcV,
	     tmbslHdmiTxVsSrc_t srcX, tmbslHdmiTxVsTgl_t toggle, tmbslHdmiTxVsOnce_t once);

/**
    \brief      Set main video input and output parameters

    \param[in]  txUnit       Transmitter unit number
    \param[in]  vinFmt       EIA/CEA Video input format: 1 to 31, 0 = No Change
    \param[in]  scaMode      Scaler mode: Off, On, Auto, No Change
			     On TDA9989, only scaler mode off is possible
    \param[in]  voutFmt      EIA/CEA Video output format: 1 to 31, 0 = No Change
    \param[in]  uPixelRepeat Pixel repetition factor: 0 to 9, 10 = default,
			     11 = no change
    \param[in]  matMode      Matrix mode: 0 = off, 1 = auto
    \param[in]  datapathBits Datapath bitwidth: 0 to 3 (8, 10, 12, No Change)
    \param[in]  Desired VQR in dvi mode

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_INCONSISTENT_PARAMS: params are inconsistent
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
 */
	 tmErrorCode_t
	    tmbslTDA9989VideoSetInOut
	    (tmUnitSelect_t txUnit,
	     tmbslHdmiTxVidFmt_t vinFmt,
	     tmbslHdmiTx3DStructure_t structure3D,
	     tmbslHdmiTxScaMode_t scaMode,
	     tmbslHdmiTxVidFmt_t voutFmt,
	     UInt8 uPixelRepeat,
	     tmbslHdmiTxMatMode_t matMode,
	     tmbslHdmiTxVoutDbits_t datapathBits, tmbslHdmiTxVQR_t dviVqr);

/**
    \brief      Use only for debug to flag the software debug interrupt

    \param[in]  txUnit       Transmitter unit number
    \param[in]  uSwInt       Interrupt to be generated (not relevant)

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
 */
	 tmErrorCode_t tmbslTDA9989FlagSwInt(tmUnitSelect_t txUnit, UInt32 uSwInt);


/**
    \brief      Enable or disable 5v power

    \param[in]  txUnit    Transmitter unit number
    \param[in]  pwrEnable 5v Power enable(True)/disable(False)

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_NOT_SUPPORTED: functionnality not supported by this device
 */
	 tmErrorCode_t tmbslTDA9989Set5vpower(tmUnitSelect_t txUnit, Bool pwrEnable);

/**
    \brief      Enable or disable a callback source

    \param[in]  txUnit         Transmitter unit number
    \param[in]  callbackSource Callback source
    \param[in]  enable         Callback source enable(True)/disable(False)

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_NOT_SUPPORTED: impossible to disable this interrupt
 */
	 tmErrorCode_t
	    tmbslTDA9989EnableCallback
	    (tmUnitSelect_t txUnit, tmbslHdmiTxCallbackInt_t callbackSource, Bool enable);

/**
    \brief      Configure the deep color mode

    \param[in]  txUnit     Transmitter unit number
    \param[in]  colorDepth Number of bits per pixel to be processed
    \param[in]  termEnable Enable transmitter termination

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_NOT_SUPPORTED: mode not supported
 */
	 tmErrorCode_t
	    tmbslTDA9989SetColorDepth
	    (tmUnitSelect_t txUnit, tmbslHdmiTxColorDepth colorDepth, Bool termEnable);

/**
    \brief      Configure the default phase for a specific deep color mode

    \param[in]  txUnit      Transmitter unit number
    \param[in]  bEnable     Enable(true)/disable(False) default phase
    \param[in]  colorDepth  Concerned deepcolor mode
    \param[in]  videoFormat Number of bits per pixel to be processed

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
		  - TMBSL_ERR_HDMI_NOT_SUPPORTED: functionnality not supported by this device
 */
	 tmErrorCode_t
	    tmbslTDA9989SetDefaultPhase
	    (tmUnitSelect_t txUnit,
	     Bool bEnable, tmbslHdmiTxColorDepth colorDepth, UInt8 videoFormat);



/**
    \brief      Control (Enable/Disable) VS interrupt

    \param[in]  txUnit       Transmitter unit number
    \param[in]  uIntFlag     Enable/Disable VS interrupt

    \return     The call result:
		- TM_OK: the call was successful
		- Else a problem has been detected:
		  - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: bad transmitter unit number
		  - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter was out of range
		  - TMBSL_ERR_HDMI_NOT_INITIALIZED: transmitter not initialized
		  - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C bus
 */

	 tmErrorCode_t tmbslTDA9989CtlVsInterrupt(tmUnitSelect_t txUnit, Bool uIntFlag);

/*============================================================================*/
/**
    \brief Fill Gamut metadata packet into one of the gamut HW buffer. this
	   function is not sending any gamut metadata into the HDMI stream,
	   it is only loading data into the HW.

    \param txUnit Transmitter unit number
    \param pPkt   pointer to the gamut packet structure
    \param bufSel number of the gamut buffer to fill

    \return The call result:
	    - TM_OK: the call was successful
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: the unit number is wrong or
	      the receiver instance is not initialised
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter is invalid or out
	      of range
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C
	      bus

 ******************************************************************************/
	tmErrorCode_t tmbslTDA9989PktFillGamut
	    (tmUnitSelect_t txUnit, tmbslHdmiTxPktGamut_t *pPkt, UInt8 bufSel);

/*============================================================================*/
/**
    \brief Enable transmission of gamut metadata packet. Calling this function
	   tells HW which gamut buffer to send into the HDMI stream. HW will
	   only take into account this command at the next VS, not during the
	   current one.

    \param txUnit Transmitter unit number
    \param bufSel Number of the gamut buffer to be sent
    \param enable Enable/disable gamut packet transmission

    \return The call result:
	    - TM_OK: the call was successful
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: the unit number is wrong or
	      the receiver instance is not initialised
	    - TMBSL_ERR_HDMI_BAD_PARAMETER: a parameter is invalid or out
	      of range
	    - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing to the I2C
	      bus

 ******************************************************************************/
	tmErrorCode_t tmbslTDA9989PktSendGamut(tmUnitSelect_t txUnit, UInt8 bufSel, Bool bEnable);


/**
    \brief Return the category of equipement connected

    \param txUnit   Transmitter unit number
    \param category return category type

    \return The call result:
	    - TM_OK: the call was successful
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: the unit number is wrong or
	      the receiver instance is not initialised
	    - TMBSL_ERR_HDMI_INCONSISTENT_PARAMS: params are inconsistent
	    - TMBSL_ERR_HDMI_RESOURCE_NOT_AVAILABLE hdcp not started

*/
	tmErrorCode_t tmbslTDA9989HdcpGetSinkCategory
	    (tmUnitSelect_t txUnit, tmbslHdmiTxSinkCategory_t *category);


/**
    \brief Return the sink latency information if any

    \param txUnit         Transmitter unit number
    \param pEdidLatency   latency data structure to return

    \return The call result:
	    - TM_OK: the call was successful
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: the unit number is wrong or
	      the receiver instance is not initialised
	    - TMBSL_ERR_HDMI_INCONSISTENT_PARAMS: params are inconsistent
	    - TMBSL_ERR_HDMI_RESOURCE_NOT_AVAILABLE hdcp not started

*/
	tmErrorCode_t tmbslTDA9989EdidGetLatencyInfo
	    (tmUnitSelect_t txUnit, tmbslHdmiTxEdidLatency_t *pEdidLatency);


/**
    \brief Return the sink additional VSDB data information if any

    \param txUnit         Transmitter unit number
    \param p3Ddata        3D data structure to return

    \return The call result:
	    - TM_OK: the call was successful
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: the unit number is wrong or
	      the receiver instance is not initialised
	    - TMBSL_ERR_HDMI_INCONSISTENT_PARAMS: params are inconsistent
	    - TMBSL_ERR_HDMI_RESOURCE_NOT_AVAILABLE hdcp not started

*/
	tmErrorCode_t tmbslTDA9989EdidGetExtraVsdbData
	    (tmUnitSelect_t txUnit, tmbslHdmiTxEdidExtraVsdbData_t **pExtraVsdbData);


#ifdef TMFL_HDCP_OPTIMIZED_POWER
/**
    \brief Optimized power by frozing useless clocks related to HDCP

    \param txUnit         Transmitter unit number
    \param request        power down request

    \return The call result:
	    - TM_OK: the call was successful
	    - TMBSL_ERR_HDMI_BAD_UNIT_NUMBER: the unit number is wrong or
	      the receiver instance is not initialised
	    - TMBSL_ERR_HDMI_INCONSISTENT_PARAMS: params are inconsistent

*/
	 tmErrorCode_t tmbslTDA9989HdcpPowerDown(tmUnitSelect_t txUnit, Bool requested);
#endif

#ifdef __cplusplus
}
#endif
#endif				/* TMBSLTDA9989_FUNCTIONS_H */
/*============================================================================*//*                            END OF FILE                                     *//*============================================================================*/
