/**
 * Copyright (C) 2006 NXP N.V., All Rights Reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of NXP N.V. and is confidential in nature. Under no circumstances
 * is this software to be  exposed to or placed under an Open Source License of
 * any type without the expressed written permission of NXP N.V.
 *
 * \file          tmdlHdmiTx_cfg.h
 *
 * \version       $Revision: 1 $
 *
 * \date          $Date: 08/08/07 11:00 $
 *
 * \brief         devlib driver component API for the TDA998x HDMI Transmitters
 *
 * \section refs  Reference Documents
 * HDMI Tx Driver - FRS.doc,
 *
 * \section info  Change Information
 *
 * \verbatim

   $History: tmbslHdmiTx_cfg.h $
 *
 * *****************  Version 1  *****************
 * User: J. Lamotte Date: 08/08/07  Time: 11:00
 * initial version
 *

   \endverbatim
 *
*/

/*****************************************************************************/
/*****************************************************************************/
/*                THIS FILE MUST NOT BE MODIFIED BY CUSTOMER                 */
/*      Customer specific configuration is set in tmdlHdmiTx_cfg.c file      */
/*****************************************************************************/
/*****************************************************************************/

#ifndef TMDLHDMITX_CFG_H
#define TMDLHDMITX_CFG_H

#include "tmNxTypes.h"
#include "tmbslHdmiTx_types.h"
#include "tmdlHdmiTx_Types.h"


#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/*                          TYPES DECLARATIONS                                */
/*============================================================================*/

/**
 * \brief Video signals that can be input to video ports in RGB/YUV 4:4:4 mode
 */
	typedef enum {
		TMDL_HDMITX_VID444_GY_4_TO_7 = 0x00,
						/**< Video signal G/Y, bits 4 to 7 */
		TMDL_HDMITX_VID444_GY_0_TO_3 = 0x01,
						/**< Video signal G/Y, bits 0 to 3 */
		TMDL_HDMITX_VID444_BU_4_TO_7 = 0x02,
						/**< Video signal B/U, bits 4 to 7 */
		TMDL_HDMITX_VID444_BU_0_TO_3 = 0x03,
						/**< Video signal B/U, bits 0 to 3 */
		TMDL_HDMITX_VID444_VR_4_TO_7 = 0x04,
						/**< Video signal V/R, bits 4 to 7 */
		TMDL_HDMITX_VID444_VR_0_TO_3 = 0x05,
						/**< Video signal V/R, bits 0 to 3 */
		TMDL_HDMITX_VID444_GY_7_TO_4 = 0x80,
						/**< Video signal G/Y, bits 7 to 4 (mirrored) */
		TMDL_HDMITX_VID444_GY_3_TO_0 = 0x81,
						/**< Video signal G/Y, bits 3 to 0 (mirrored) */
		TMDL_HDMITX_VID444_BU_7_TO_4 = 0x82,
						/**< Video signal B/U, bits 7 to 4 (mirrored) */
		TMDL_HDMITX_VID444_BU_3_TO_0 = 0x83,
						/**< Video signal B/U, bits 3 to 0 (mirrored) */
		TMDL_HDMITX_VID444_VR_7_TO_4 = 0x84,
						/**< Video signal V/R, bits 7 to 4 (mirrored) */
		TMDL_HDMITX_VID444_VR_3_TO_0 = 0x85,
						/**< Video signal V/R, bits 3 to 0 (mirrored) */
		TMDL_HDMITX_VID444_NOT_CONNECTED = 0x100
						/**< No signal connected */
	} tmdlHdmiTxCfgVideoSignal444;

/**
 * \brief Video signals that can be input to video ports in semi-planar YUV 4:2:2 mode
 */
	typedef enum {
		TMDL_HDMITX_VID422_Y_8_TO_11 = 0x00,
						/**< Video signal G/Y, bits 8 to 11 */
		TMDL_HDMITX_VID422_Y_4_TO_7 = 0x01,
						/**< Video signal G/Y, bits 4 to 7 */
		TMDL_HDMITX_VID422_Y_0_TO_3 = 0x02,
						/**< Video signal G/Y, bits 0 to 3 */
		TMDL_HDMITX_VID422_UV_8_TO_11 = 0x03,
						/**< Video signal B/U, bits 8 to 11 */
		TMDL_HDMITX_VID422_UV_4_TO_7 = 0x04,
						/**< Video signal B/U, bits 4 to 7 */
		TMDL_HDMITX_VID422_UV_0_TO_3 = 0x05,
						/**< Video signal B/U, bits 0 to 3 */
		TMDL_HDMITX_VID422_Y_11_TO_8 = 0x80,
						/**< Video signal G/Y, bits 11 to 8 (mirrored) */
		TMDL_HDMITX_VID422_Y_7_TO_4 = 0x81,
						/**< Video signal G/Y, bits 7 to 4 (mirrored) */
		TMDL_HDMITX_VID422_Y_3_TO_0 = 0x82,
						/**< Video signal G/Y, bits 3 to 0 (mirrored) */
		TMDL_HDMITX_VID422_UV_11_TO_8 = 0x83,
						/**< Video signal B/U, bits 11 to 8 (mirrored) */
		TMDL_HDMITX_VID422_UV_7_TO_4 = 0x84,
						/**< Video signal B/U, bits 7 to 4 (mirrored) */
		TMDL_HDMITX_VID422_UV_3_TO_0 = 0x85,
						/**< Video signal B/U, bits 3 to 0 (mirrored) */
		TMDL_HDMITX_VID422_NOT_CONNECTED = 0x100
						/**< No signal connected */
	} tmdlHdmiTxCfgVideoSignal422;

/**
 * \brief Video signals that can be input to video ports in semi-planar CCIR 656 mode
 */
	typedef enum {
		TMDL_HDMITX_VIDCCIR_8_TO_11 = 0x00,
						/**< Video signal CCIR, bits 8 to 11 */
		TMDL_HDMITX_VIDCCIR_4_TO_7 = 0x01,
						/**< Video signal CCIR, bits 4 to 7 */
		TMDL_HDMITX_VIDCCIR_0_TO_3 = 0x02,
						/**< Video signal CCIR, bits 0 to 3 */
		TMDL_HDMITX_VIDCCIR_11_TO_8 = 0x80,
						/**< Video signal CCIR, bits 11 to 8 (mirrored) */
		TMDL_HDMITX_VIDCCIR_7_TO_4 = 0x81,
						/**< Video signal CCIR, bits 7 to 4 (mirrored) */
		TMDL_HDMITX_VIDCCIR_3_TO_0 = 0x82,
						/**< Video signal CCIR, bits 3 to 0 (mirrored) */
		TMDL_HDMITX_VIDCCIR_NOT_CONNECTED = 0x100
						/**< No signal connected */
	} tmdlHdmiTxCfgVideoSignalCCIR656;

#ifdef TMFL_RGB_DDR_12BITS
/**
 * \brief Video signals that can be input to video ports in semi-planar CCIR 656 mode
 */
	typedef enum {
		TMDL_HDMITX_VID_B_0_3_G_4_7 = 0x00,
						 /**< Video signal blue 0 to 3 then green 4 to 7 */
		TMDL_HDMITX_VID_B_4_7_R_0_3 = 0x01,
						 /**< Video signal blue 4 to 7 then red 0 to 3 */
		TMDL_HDMITX_VID_G_0_3_R_4_7 = 0x02,
						 /**< Video signal green 0 to 3 then red 4 to 7 */
		TMDL_HDMITX_VID_DDR_NOT_CONNECTED = 0x100
						 /**< No signal connected */
	} tmdlHdmiTxCfgVideoSignal_RGB_DDR_12bits;

/**
 * \brief Video signals can be mux each others using register 0x27 (MUX_VP_MIX_OUT)
 * Whatch out that VIP output shall be GBR: green on [23:16], blue on [15:8] and red in [7:0]
 * this is done by default for all video mode but RGB_DDR_12bits where some extra mux is needed
 */
	typedef enum {
		VIP_MUX_R_B = 0x00,		 /**< internal vp_r = vp blue */
		VIP_MUX_R_G = 0x10,		 /**< internal vp_r = vp green */
		VIP_MUX_R_R = 0x20,		 /**< internal vp_r = vp red */
		VIP_MUX_G_B = 0x00,		 /**< internal vp_g = vp blue */
		VIP_MUX_G_G = 0x04,		 /**< internal vp_g = vp green */
		VIP_MUX_G_R = 0x08,		 /**< internal vp_g = vp red */
		VIP_MUX_B_B = 0x00,		 /**< internal vp_b = vp blue */
		VIP_MUX_B_G = 0x01,		 /**< internal vp_b = vp green */
		VIP_MUX_B_R = 0x02,		 /**< internal vp_b = vp red */
	} tmdlHdmiTxCfgVideoSignal_VIP_OUTPUT_MUX;
#endif

/* Audio port configuration, bitn = 1 to enable port n, = 0 to disable port n */
#define ENABLE_ALL_AUDIO_PORT       0xFF
/* Audio clock port configuration */
#define ENABLE_AUDIO_CLOCK_PORT     1
#define DISABLE_AUDIO_CLOCK_PORT    0
/* Audio port configuration, bitn = 1 to pulldown port n*/
#define DISABLE_ALL_AUDIO_PORT_PULLDOWN 0x00
/* Audio clock port pulldown configuration */
#define ENABLE_AUDIO_CLOCK_PORT_PULLDOWN    1
#define DISABLE_AUDIO_CLOCK_PORT_PULLDOWN   0

/**
 * \brief Structure defining a video mode
 */
	typedef struct _tmdlHdmiTxCfgResolution_t {
		tmdlHdmiTxVidFmt_t resolutionID;
		UInt16 width;
		UInt16 height;
		Bool interlaced;
		tmdlHdmiTxVfreq_t vfrequency;
		tmdlHdmiTxPictAspectRatio_t aspectRatio;
	} tmdlHdmiTxCfgResolution_t, *ptmdlHdmiTxCfgResolution_t;

/**
 * \brief Structure gathering all configuration parameters
 */
	typedef struct {
		UInt8 commandTaskPriority;
		UInt8 commandTaskStackSize;
		UInt8 commandTaskQueueSize;
		UInt8 hdcpTaskPriority;
		UInt8 hdcpTaskStackSize;
		UInt8 i2cAddress;
		ptmbslHdmiTxSysFunc_t i2cReadFunction;
		ptmbslHdmiTxSysFunc_t i2cWriteFunction;
		ptmdlHdmiTxCfgResolution_t pResolutionInfo;
		UInt8 *pMirrorTableCCIR656;
		UInt8 *pSwapTableCCIR656;
		UInt8 *pEnableVideoPortCCIR656;
		UInt8 *pGroundVideoPortCCIR656;
		UInt8 *pMirrorTableYUV422;
		UInt8 *pSwapTableYUV422;
		UInt8 *pEnableVideoPortYUV422;
		UInt8 *pGroundVideoPortYUV422;
		UInt8 *pMirrorTableYUV444;
		UInt8 *pSwapTableYUV444;
		UInt8 *pEnableVideoPortYUV444;
		UInt8 *pGroundVideoPortYUV444;
		UInt8 *pMirrorTableRGB444;
		UInt8 *pSwapTableRGB444;
		UInt8 *pEnableVideoPortRGB444;
		UInt8 *pGroundVideoPortRGB444;
#ifdef TMFL_RGB_DDR_12BITS
		UInt8 *pMirrorTableRGB_DDR_12bits;
		UInt8 *pSwapTableRGB_DDR_12bits;
		UInt8 *pNoMux;
		UInt8 *pMux_RGB_DDR_12bits;
		UInt8 *pEnableVideoPortRGB_DDR_12bits;
		UInt8 *pGroundVideoPortRGB_DDR_12bits;
#endif
		UInt8 *pEnableAudioPortSPDIF;
		UInt8 *pGroundAudioPortSPDIF;
		UInt8 *pEnableAudioClockPortSPDIF;
		UInt8 *pGroundAudioClockPortSPDIF;
		UInt8 *pEnableAudioPortI2S;
		UInt8 *pGroundAudioPortI2S;
		UInt8 *pEnableAudioPortI2S8C;
		UInt8 *pGroundAudioPortI2S8C;
		UInt8 *pEnableAudioClockPortI2S;
		UInt8 *pGroundAudioClockPortI2S;
		UInt8 *pEnableAudioPortOBA;
		UInt8 *pGroundAudioPortOBA;
		UInt8 *pEnableAudioClockPortOBA;
		UInt8 *pGroundAudioClockPortOBA;
		UInt8 *pEnableAudioPortDST;
		UInt8 *pGroundAudioPortDST;
		UInt8 *pEnableAudioClockPortDST;
		UInt8 *pGroundAudioClockPortDST;
		UInt8 *pEnableAudioPortHBR;
		UInt8 *pGroundAudioPortHBR;
		UInt8 *pEnableAudioClockPortHBR;
		UInt8 *pGroundAudioClockPortHBR;
		UInt16 keySeed;
		tmdlHdmiTxTestPattern_t pattern;
		UInt8 dataEnableSignalAvailable;	/* 0 DE is NOT available, 1 DE is there */
	} tmdlHdmiTxDriverConfigTable_t;

/*============================================================================*/
/*                       CONSTANTS DECLARATIONS                               */
/*============================================================================*/

/* Number of HW units supported by SW driver */
#define MAX_UNITS 1

#ifdef TMFL_OS_WINDOWS		/* OS Windows */
	extern tmdlHdmiTxCfgVideoSignalCCIR656 videoPortMapping_CCIR656[MAX_UNITS][6];
	extern tmdlHdmiTxCfgVideoSignal422 videoPortMapping_YUV422[MAX_UNITS][6];
	extern tmdlHdmiTxCfgVideoSignal444 videoPortMapping_YUV444[MAX_UNITS][6];
	extern tmdlHdmiTxCfgVideoSignal444 videoPortMapping_RGB444[MAX_UNITS][6];
#ifdef TMFL_RGB_DDR_12BITS
	extern tmdlHdmiTxCfgVideoSignal_RGB_DDR_12bits
	    VideoPortMapping_RGB_DDR_12bits[MAX_UNITS][6];
	extern UInt8 VideoPortMux_RGB_DDR_12bits[MAX_UNITS];
	extern UInt8 VideoPortNoMux[MAX_UNITS];
#endif
#else				/* TMFL_OS_WINDOWS */
	extern const tmdlHdmiTxCfgVideoSignalCCIR656 videoPortMapping_CCIR656[MAX_UNITS][6];
	extern const tmdlHdmiTxCfgVideoSignal422 videoPortMapping_YUV422[MAX_UNITS][6];
	extern const tmdlHdmiTxCfgVideoSignal444 videoPortMapping_YUV444[MAX_UNITS][6];
	extern const tmdlHdmiTxCfgVideoSignal444 videoPortMapping_RGB444[MAX_UNITS][6];
#ifdef TMFL_RGB_DDR_12BITS
	extern const tmdlHdmiTxCfgVideoSignal_RGB_DDR_12bits
	    VideoPortMapping_RGB_DDR_12bits[MAX_UNITS][6];
	extern const UInt8 VideoPortMux_RGB_DDR_12bits[MAX_UNITS];
	extern const UInt8 VideoPortNoMux[MAX_UNITS];
#endif
#endif				/* TMFL_OS_WINDOWS */

/*============================================================================*/
/*                          VARIABLES DECLARATIONS                            */
/*============================================================================*/
	extern tmdlHdmiTxDriverConfigTable_t driverConfigTableTx[MAX_UNITS];


/*============================================================================*/
/*                       FUNCTIONS DECLARATIONS                               */
/*============================================================================*/
#ifdef TMFL_CEC_AVAILABLE

#include "tmdlHdmiCEC_Types.h"

	typedef struct {
		UInt8 commandTaskPriority;
		UInt8 commandTaskStackSize;
		UInt8 commandTaskQueueSize;
		UInt8 i2cAddress;
		ptmdlHdmiCecSysFunc_t i2cReadFunction;
		ptmdlHdmiCecSysFunc_t i2cWriteFunction;
		tmdlHdmiCecCapabilities_t *pCapabilitiesList;
	} tmdlHdmiCecDriverConfigTable_t;

	tmErrorCode_t tmdlHdmiCecCfgGetConfig
	    (tmUnitSelect_t unit, tmdlHdmiCecDriverConfigTable_t *pConfig);
#endif

#ifdef __cplusplus
}
#endif
#endif				/* TMDLHDMITX_CFG_H */
/*============================================================================*//*                               END OF FILE                                  *//*============================================================================*/
