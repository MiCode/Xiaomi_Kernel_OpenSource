/**
 * Copyright (C) 2006 NXP N.V., All Rights Reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of NXP N.V. and is confidential in nature. Under no circumstances
 * is this software to be  exposed to or placed under an Open Source License of
 * any type without the expressed written permission of NXP N.V.
 *
 * \file          tmdlHdmiTx_local.c
 *
 * \version       Revision: 1
 *
 * \date          Date: 21/02/08
 *
 * \brief         devlib driver component API for the TDA998x HDMI Transmitters
 *
 * \section refs  Reference Documents
 * HDMI Tx Driver - FRS.doc,
 *
 * \section info  Change Information
 *
 * \verbatim

   History:       tmdlHdmiTx_local.c
 *
 * *****************  Version 1  *****************
 * User: G. Burnouf Date: 21/02/08
 * Updated in $/Source/tmdlHdmiTx/src
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


#include "tmdlHdmiTx_local.h"
#include "tmdlHdmiTx_cfg.h"
#include "tmdlHdmiTx.h"

/*============================================================================*/
/*                          TYPES DECLARATIONS                                */
/*============================================================================*/

typedef struct _dlHdmiTxResolution_t {
	tmdlHdmiTxVidFmt_t resolutionID;
	UInt16 width;
	UInt16 height;
	Bool interlaced;
	tmdlHdmiTxVfreq_t vfrequency;
	tmdlHdmiTxPictAspectRatio_t aspectRatio;
} dlHdmiTxResolution_t, *pdlHdmiTxResolution_t;


/*============================================================================*/
/*                       CONSTANTS DECLARATIONS                               */
/*============================================================================*/
/* macro for quick error handling */
#ifndef RETIF
#define RETIF(cond, rslt) if ((cond)) {return (rslt); }
#endif

/*============================================================================*/
/*                         FUNCTION PROTOTYPES                                */
/*============================================================================*/

/******************************************************************************/
/* DO NOT MODIFY                                                              */
/******************************************************************************/

static void dlHdmiTxGenerateVideoPortTables
    (tmUnitSelect_t unit, tmdlHdmiTxDriverConfigTable_t *pConfig);


/*============================================================================*/
/*                       VARIABLES DECLARATIONS                               */
/*============================================================================*/

/**
 * \brief List of the resolution to be detected by the device library
 */

#ifdef TMFL_OS_WINDOWS		/* OS Windows */
dlHdmiTxResolution_t resolutionInfoTx[RESOLUTION_NB] = {
#else				/* OS ARM7 */
const dlHdmiTxResolution_t resolutionInfoTx[RESOLUTION_NB] = {
#endif				/* endif TMFL_OS_WINDOWS */
	/* TV Formats */
	/* 60 HZ */
	{TMDL_HDMITX_VFMT_01_640x480p_60Hz, 640, 480, False, TMDL_HDMITX_VFREQ_59Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_02_720x480p_60Hz, 720, 480, False, TMDL_HDMITX_VFREQ_59Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_03_720x480p_60Hz, 720, 480, False, TMDL_HDMITX_VFREQ_59Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},
	{TMDL_HDMITX_VFMT_04_1280x720p_60Hz, 1280, 720, False, TMDL_HDMITX_VFREQ_60Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},
	{TMDL_HDMITX_VFMT_05_1920x1080i_60Hz, 1920, 1080, True, TMDL_HDMITX_VFREQ_60Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},
	{TMDL_HDMITX_VFMT_06_720x480i_60Hz, 720, 480, True, TMDL_HDMITX_VFREQ_59Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_07_720x480i_60Hz, 720, 480, True, TMDL_HDMITX_VFREQ_59Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},
	{TMDL_HDMITX_VFMT_08_720x240p_60Hz, 720, 240, False, TMDL_HDMITX_VFREQ_59Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_09_720x240p_60Hz, 720, 240, False, TMDL_HDMITX_VFREQ_59Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},
	{TMDL_HDMITX_VFMT_10_720x480i_60Hz, 720, 480, True, TMDL_HDMITX_VFREQ_59Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_11_720x480i_60Hz, 720, 480, True, TMDL_HDMITX_VFREQ_59Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},
	{TMDL_HDMITX_VFMT_12_720x240p_60Hz, 720, 240, False, TMDL_HDMITX_VFREQ_59Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_13_720x240p_60Hz, 720, 240, False, TMDL_HDMITX_VFREQ_59Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},
	{TMDL_HDMITX_VFMT_14_1440x480p_60Hz, 1440, 480, False, TMDL_HDMITX_VFREQ_59Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_15_1440x480p_60Hz, 1440, 480, False, TMDL_HDMITX_VFREQ_59Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},
	{TMDL_HDMITX_VFMT_16_1920x1080p_60Hz, 1920, 1080, False, TMDL_HDMITX_VFREQ_60Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},
	{TMDL_HDMITX_VFMT_35_2880x480p_60Hz, 2880, 480, False, TMDL_HDMITX_VFREQ_60Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_36_2880x480p_60Hz, 2880, 480, False, TMDL_HDMITX_VFREQ_60Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},

	/* 50 HZ */
	{TMDL_HDMITX_VFMT_17_720x576p_50Hz, 720, 576, False, TMDL_HDMITX_VFREQ_50Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_18_720x576p_50Hz, 720, 576, False, TMDL_HDMITX_VFREQ_50Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},
	{TMDL_HDMITX_VFMT_19_1280x720p_50Hz, 1280, 720, False, TMDL_HDMITX_VFREQ_50Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},
	{TMDL_HDMITX_VFMT_20_1920x1080i_50Hz, 1920, 1080, True, TMDL_HDMITX_VFREQ_50Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},
	{TMDL_HDMITX_VFMT_21_720x576i_50Hz, 720, 576, True, TMDL_HDMITX_VFREQ_50Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_22_720x576i_50Hz, 720, 576, True, TMDL_HDMITX_VFREQ_50Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},
	{TMDL_HDMITX_VFMT_23_720x288p_50Hz, 720, 288, False, TMDL_HDMITX_VFREQ_50Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_24_720x288p_50Hz, 720, 288, False, TMDL_HDMITX_VFREQ_50Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},
	{TMDL_HDMITX_VFMT_25_720x576i_50Hz, 720, 576, True, TMDL_HDMITX_VFREQ_50Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_26_720x576i_50Hz, 720, 576, True, TMDL_HDMITX_VFREQ_50Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},
	{TMDL_HDMITX_VFMT_27_720x288p_50Hz, 720, 288, False, TMDL_HDMITX_VFREQ_50Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_28_720x288p_50Hz, 720, 288, False, TMDL_HDMITX_VFREQ_50Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},
	{TMDL_HDMITX_VFMT_29_1440x576p_50Hz, 1440, 576, False, TMDL_HDMITX_VFREQ_50Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_30_1440x576p_50Hz, 1440, 576, False, TMDL_HDMITX_VFREQ_50Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},
	{TMDL_HDMITX_VFMT_31_1920x1080p_50Hz, 1920, 1080, False, TMDL_HDMITX_VFREQ_50Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},
	{TMDL_HDMITX_VFMT_37_2880x576p_50Hz, 2880, 576, False, TMDL_HDMITX_VFREQ_50Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_38_2880x576p_50Hz, 2880, 576, False, TMDL_HDMITX_VFREQ_50Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},

	/* Low Tv */
	{TMDL_HDMITX_VFMT_32_1920x1080p_24Hz, 1920, 1080, False, TMDL_HDMITX_VFREQ_24Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},
	{TMDL_HDMITX_VFMT_33_1920x1080p_25Hz, 1920, 1080, False, TMDL_HDMITX_VFREQ_25Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},
	{TMDL_HDMITX_VFMT_34_1920x1080p_30Hz, 1920, 1080, False, TMDL_HDMITX_VFREQ_30Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},
	{TMDL_HDMITX_VFMT_60_1280x720p_24Hz, 1280, 720, False, TMDL_HDMITX_VFREQ_24Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},
	{TMDL_HDMITX_VFMT_61_1280x720p_25Hz, 1280, 720, False, TMDL_HDMITX_VFREQ_25Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},
	{TMDL_HDMITX_VFMT_62_1280x720p_30Hz, 1280, 720, False, TMDL_HDMITX_VFREQ_30Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9}

#ifdef FORMAT_PC
	/* PC Formats */
	/* 60 HZ */
	, {TMDL_HDMITX_VFMT_PC_640x480p_60Hz, 640, 480, False, TMDL_HDMITX_VFREQ_60Hz,
	   TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_PC_800x600p_60Hz, 800, 600, False, TMDL_HDMITX_VFREQ_60Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_PC_1152x960p_60Hz, 1152, 960, False, TMDL_HDMITX_VFREQ_60Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_6_5},
	{TMDL_HDMITX_VFMT_PC_1024x768p_60Hz, 1024, 768, False, TMDL_HDMITX_VFREQ_60Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_PC_1280x768p_60Hz, 1280, 768, False, TMDL_HDMITX_VFREQ_60Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_5_3},
	{TMDL_HDMITX_VFMT_PC_1280x1024p_60Hz, 1280, 1024, False, TMDL_HDMITX_VFREQ_60Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_5_4},
	{TMDL_HDMITX_VFMT_PC_1360x768p_60Hz, 1360, 768, False, TMDL_HDMITX_VFREQ_60Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_9},
	{TMDL_HDMITX_VFMT_PC_1400x1050p_60Hz, 1400, 1050, False, TMDL_HDMITX_VFREQ_60Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_PC_1600x1200p_60Hz, 1600, 1200, False, TMDL_HDMITX_VFREQ_60Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	/* 70 HZ */
	{TMDL_HDMITX_VFMT_PC_1024x768p_70Hz, 1024, 768, False, TMDL_HDMITX_VFREQ_70Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	/* 72 HZ */
	{TMDL_HDMITX_VFMT_PC_640x480p_72Hz, 640, 480, False, TMDL_HDMITX_VFREQ_72Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_PC_800x600p_72Hz, 800, 600, False, TMDL_HDMITX_VFREQ_72Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	/* 75 HZ */
	{TMDL_HDMITX_VFMT_PC_640x480p_75Hz, 640, 480, False, TMDL_HDMITX_VFREQ_75Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_PC_1024x768p_75Hz, 1024, 768, False, TMDL_HDMITX_VFREQ_75Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_PC_800x600p_75Hz, 800, 600, False, TMDL_HDMITX_VFREQ_75Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_PC_1024x864p_75Hz, 1024, 864, False, TMDL_HDMITX_VFREQ_75Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_UNDEFINED},
	{TMDL_HDMITX_VFMT_PC_1280x1024p_75Hz, 1280, 1024, False, TMDL_HDMITX_VFREQ_75Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_5_4},
	/* 85 HZ */
	{TMDL_HDMITX_VFMT_PC_640x350p_85Hz, 640, 350, False, TMDL_HDMITX_VFREQ_85Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_UNDEFINED},
	{TMDL_HDMITX_VFMT_PC_640x400p_85Hz, 640, 400, False, TMDL_HDMITX_VFREQ_85Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_16_10},
	{TMDL_HDMITX_VFMT_PC_720x400p_85Hz, 720, 400, False, TMDL_HDMITX_VFREQ_85Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_9_5},
	{TMDL_HDMITX_VFMT_PC_640x480p_85Hz, 640, 480, False, TMDL_HDMITX_VFREQ_85Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_PC_800x600p_85Hz, 800, 600, False, TMDL_HDMITX_VFREQ_85Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_PC_1024x768p_85Hz, 1024, 768, False, TMDL_HDMITX_VFREQ_85Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_PC_1152x864p_85Hz, 1152, 864, False, TMDL_HDMITX_VFREQ_85Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_PC_1280x960p_85Hz, 1280, 960, False, TMDL_HDMITX_VFREQ_85Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3},
	{TMDL_HDMITX_VFMT_PC_1280x1024p_85Hz, 1280, 1024, False, TMDL_HDMITX_VFREQ_85Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_5_4},
	/* 87 HZ */
	{TMDL_HDMITX_VFMT_PC_1024x768i_87Hz, 1024, 768, True, TMDL_HDMITX_VFREQ_87Hz,
	 TMDL_HDMITX_P_ASPECT_RATIO_4_3}
#endif				/* FORMAT_PC */
};


/*============================================================================*/
/*                         FUNCTION                                           */
/*============================================================================*/

/******************************************************************************/
/* DO NOT MODIFY                                                              */
/******************************************************************************
    \brief This function allows to the main driver to retrieve its
	   configuration parameters.

    \param pConfig Pointer to the config structure

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_UNIT_NUMBER: the unit number is wrong or
	      the receiver instance is not initialised
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent

******************************************************************************/
tmErrorCode_t dlHdmiTxGetConfig(tmUnitSelect_t unit, tmdlHdmiTxDriverConfigTable_t *pConfig) {
	/* Check if unit number is in range */
	RETIF((unit < 0) || (unit >= MAX_UNITS), TMDL_ERR_DLHDMITX_BAD_UNIT_NUMBER)

	    /* Check if pointer is Null */
	    RETIF(pConfig == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

	    * pConfig = driverConfigTableTx[unit];

	/* Done here because of const declaration of tables in ARM7 case */
	pConfig->pResolutionInfo = (ptmdlHdmiTxCfgResolution_t) resolutionInfoTx;

	/* Generate swap and mirror tables in function of video port mapping tables */
	dlHdmiTxGenerateVideoPortTables(unit, pConfig);

	return TM_OK;
}


/*============================================================================*/
/*                           INTERNAL FUNCTION                                */
/*============================================================================*/
/******************************************************************************/
/* DO NOT MODIFY                                                              */
/******************************************************************************/
static void dlHdmiTxGenerateVideoPortTables
    (tmUnitSelect_t unit, tmdlHdmiTxDriverConfigTable_t *pConfig) {
	UInt8 i;

	for (i = 0; i < 6; i++) {
		/* CCIR656 */
		if (videoPortMapping_CCIR656[unit][i] != TMDL_HDMITX_VIDCCIR_NOT_CONNECTED) {
			pConfig->pSwapTableCCIR656[videoPortMapping_CCIR656[unit][i] & 0x07F] =
			    5 - i;
			pConfig->pMirrorTableCCIR656[videoPortMapping_CCIR656[unit][i] & 0x07F] =
			    (UInt8) (videoPortMapping_CCIR656[unit][i] >> 7);
			/* Enable port and disable ground port */
			if (((5 - i) % 2) == 0) {
				pConfig->pEnableVideoPortCCIR656[i / 2] |= 0x0F;
				pConfig->pGroundVideoPortCCIR656[i / 2] =
				    (UInt8) (pConfig->pGroundVideoPortCCIR656[i / 2] & 0xF0);
			} else {
				pConfig->pEnableVideoPortCCIR656[i / 2] =
				    (UInt8) (pConfig->pEnableVideoPortCCIR656[i / 2] | 0xF0);
				pConfig->pGroundVideoPortCCIR656[i / 2] &= 0x0F;
			}
		}

		/* YUV422 */
		if (videoPortMapping_YUV422[unit][i] != TMDL_HDMITX_VID422_NOT_CONNECTED) {
			pConfig->pSwapTableYUV422[videoPortMapping_YUV422[unit][i] & 0x07F] = 5 - i;
			pConfig->pMirrorTableYUV422[videoPortMapping_YUV422[unit][i] & 0x07F] =
			    (UInt8) (videoPortMapping_YUV422[unit][i] >> 7);
			/* Enable port and disable ground port */
			if (((5 - i) % 2) == 0) {
				pConfig->pEnableVideoPortYUV422[i / 2] |= 0x0F;
				pConfig->pGroundVideoPortYUV422[i / 2] =
				    (UInt8) (pConfig->pGroundVideoPortYUV422[i / 2] & 0xF0);
			} else {
				pConfig->pEnableVideoPortYUV422[i / 2] =
				    (UInt8) (pConfig->pEnableVideoPortYUV422[i / 2] | 0xF0);
				pConfig->pGroundVideoPortYUV422[i / 2] &= 0x0F;
			}
		}

		/* YUV444 */
		if (videoPortMapping_YUV444[unit][i] != TMDL_HDMITX_VID444_NOT_CONNECTED) {
			pConfig->pSwapTableYUV444[videoPortMapping_YUV444[unit][i] & 0x07F] = 5 - i;
			pConfig->pMirrorTableYUV444[videoPortMapping_YUV444[unit][i] & 0x07F] =
			    (UInt8) (videoPortMapping_YUV444[unit][i] >> 7);
			/* Enable port and disable ground port */
			if (((5 - i) % 2) == 0) {
				pConfig->pEnableVideoPortYUV444[i / 2] |= 0x0F;
				pConfig->pGroundVideoPortYUV444[i / 2] =
				    (UInt8) (pConfig->pGroundVideoPortYUV444[i / 2] & 0xF0);
			} else {
				pConfig->pEnableVideoPortYUV444[i / 2] =
				    (UInt8) (pConfig->pEnableVideoPortYUV444[i / 2] | 0xF0);
				pConfig->pGroundVideoPortYUV444[i / 2] &= 0x0F;
			}
		}

		pr_debug("videoPortMapping_RGB444[%d][%d] = %d\n", unit, i,
			 videoPortMapping_RGB444[unit][i]);
		/* RGB444 */
		if (videoPortMapping_RGB444[unit][i] != TMDL_HDMITX_VID444_NOT_CONNECTED) {
			pConfig->pSwapTableRGB444[videoPortMapping_RGB444[unit][i] & 0x07F] = 5 - i;
			pConfig->pMirrorTableRGB444[videoPortMapping_RGB444[unit][i] & 0x07F] =
			    (UInt8) (videoPortMapping_RGB444[unit][i] >> 7);
			/* Enable port and disable ground port */
			if (((5 - i) % 2) == 0) {
				pConfig->pEnableVideoPortRGB444[i / 2] |= 0x0F;
				pConfig->pGroundVideoPortRGB444[i / 2] =
				    (UInt8) (pConfig->pGroundVideoPortRGB444[i / 2] & 0xF0);
			} else {
				pConfig->pEnableVideoPortRGB444[i / 2] =
				    (UInt8) (pConfig->pEnableVideoPortRGB444[i / 2] | 0xF0);
				pConfig->pGroundVideoPortRGB444[i / 2] &= 0x0F;
			}
		}

#ifdef TMFL_RGB_DDR_12BITS
		/* RGB DDR 12bits */
		if (VideoPortMapping_RGB_DDR_12bits[unit][i] != TMDL_HDMITX_VID_DDR_NOT_CONNECTED) {
			pConfig->
			    pSwapTableRGB_DDR_12bits[VideoPortMapping_RGB_DDR_12bits[unit][i] &
						     0x07F] = 5 - i;
			pConfig->
			    pMirrorTableRGB_DDR_12bits[VideoPortMapping_RGB_DDR_12bits[unit][i] &
						       0x07F] =
			    (UInt8) (VideoPortMapping_RGB_DDR_12bits[unit][i] >> 7);
			/* Enable port and disable ground port */
			if (((5 - i) % 2) == 0) {
				pConfig->pEnableVideoPortRGB_DDR_12bits[i / 2] |= 0x0F;
				pConfig->pGroundVideoPortRGB_DDR_12bits[i / 2] =
				    (UInt8) (pConfig->pGroundVideoPortRGB_DDR_12bits[i / 2] & 0xF0);
			} else {
				pConfig->pEnableVideoPortRGB_DDR_12bits[i / 2] =
				    (UInt8) (pConfig->pEnableVideoPortRGB_DDR_12bits[i / 2] | 0xF0);
				pConfig->pGroundVideoPortRGB_DDR_12bits[i / 2] &= 0x0F;
			}
		}
#endif
	}

#ifdef TMFL_RGB_DDR_12BITS
	/* VIP internal mux for RGB DDR  */
	pConfig->pNoMux = (UInt8 *) &VideoPortNoMux[unit];
	pConfig->pMux_RGB_DDR_12bits = (UInt8 *) &VideoPortMux_RGB_DDR_12bits[unit];
#endif

}

/*============================================================================*/
/*                            END OF FILE                                     */
/*============================================================================*/
