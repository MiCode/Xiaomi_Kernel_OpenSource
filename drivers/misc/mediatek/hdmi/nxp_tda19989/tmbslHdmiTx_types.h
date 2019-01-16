/**
 * Copyright (C) 2007 NXP N.V., All Rights Reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of NXP N.V. and is confidential in nature. Under no circumstances
 * is this software to be  exposed to or placed under an Open Source License of
 * any type without the expressed written permission of NXP N.V.
 *
 * \file          tmbslHdmiTx_types.h
 *
 * \version       $Revision: 18 $
 *
 * \date          $Date: 17/03/08 $
 *
 * \brief         HDMI Transmitter common types
 *
 * \section refs  Reference Documents
 *
 * \section info  Change Information
 *
 * \verbatim

   $History: tmbslHdmiTx_types.h $
 *
 *
 * **************** Version 18  ******************
 * User: G.Burnouf     Date: 01/04/08
 * Updated in $/Source/tmbslHdmiTx/inc
 * PR1468 : add new function tmbslTDA9984GetSinkCategory
 *
 *
 * **************** Version 17  ******************
 * User: G.Burnouf     Date: 17/03/08
 * Updated in $/Source/tmbslHdmiTx/inc
 * PR1430 : Increase size of table for
 *          Short Audio Descriptor
 *
 * **************** Version 16  ******************
 * User: G.Burnouf     Date: 06/03/08
 * Updated in $/Source/tmbslHdmiTx/inc
 * PR1406 : new reset audio fifo sequence
 *
 * **************** Version 15  ******************
 * User: G.Burnouf     Date: 05/02/08
 * Updated in $/Source/tmbslHdmiTx/inc
 * PR1251 : add new type for function
 *          tmbslTDA9984EdidGetBasicDisplayParam
 *
 ******************  Version 14  ******************
 * User: G.Burnouf     Date: 14/01/08
 * Updated in $/Source/tmbslHdmiTx/inc
 * PR580 - Change BSL error base address
 *
 ******************  Version 13  ******************
 * User: G.Burnouf     Date: 10/01/08
 * Updated in $/Source/tmbslHdmiTx/inc
 * PR606 - Apply audio port config in function
 *         of audio format
 *
 * **************** Version 12  ******************
 * User: G.Burnouf     Date: 10/12/07   Time: 08:30
 * Updated in $/Source/tmbslHdmiTx/inc
 * PR1145 : return DTD and monitor description
 *
 * *****************  Version 11  *****************
 * User: G.Burnouf     Date: 04/12/07
 * Updated in $/Source/tmbslHdmiTx/inc
 * PR948 : add new formats, 1080p24/25/30
 *
 * *****************  Version 10 *****************
 * User: C. Diehl	  Date: 27/11/07
 * Updated in $/Source/tmbslHdmiTx/inc
 * PR1030 : - Align with the common interface
 *            reworked for the LIPP4200
 *
 * *****************  Version 9  *****************
 * User: J.Lamotte	  Date: 23/11/07   Time: 09:35
 * Updated in $/Source/tmbslHdmiTx/src
 * PR1078 : - update HDMI_TX_SVD_MAX_CNT from 30
 *            to 113
 *
 * *****************  Version 8  *****************
 * User: G.Burnouf	  Date: 13/11/07   Time: 09:29
 * Updated in $/Source/tmbslHdmiTx/src
 * PR1008 : - update type tmbslHdmiTxHwFeature_t
 *
 * *****************  Version 7  *****************
 * User: G.Burnouf	  Date: 16/10/07   Time: 14:32
 * Updated in $/Source/tmbslHdmiTx/src
 * PR882 : - add type tmbslHdmiTxPowerState_t
 *         - add type tmbslHdmiTxPktGmt_t for gamut
 *         - add new interrupt callback for VS
 *
 * *****************  Version 6  *****************
 * User: G.Burnouf	  Date: 05/10/07   Time: 14:32
 * Updated in $/Source/tmbslHdmiTx/src
 * PR824 : add type for enum _tmbslHdmiTxCallbackInt
 *
 * *****************  Version 5  *****************
 * User: J.Turpin	  Date: 13/09/07   Time: 14:32
 * Updated in $/Source/tmbslHdmiTx/src
 * PR693 : add black pattern functionality
 *		 - add HDMITX_PATTERN_BLACK in
 *			enum tmbslHdmiTxTestPattern_t
 *
 * *****************  Version 4  *****************
 * User: G.Burnouf   Date: 06/09/07   Time: 17:22
 * Updated in $/Source/tmbslTDA9984/Inc
 * PR656 : - add HBR format
 *         - add format I2s Philips left and right justified
 *
 * *****************  Version 3  *****************
 * User: G. Burnouf      Date: 07/08/07   Time: 10:30
 * Updated in $/Source/tmbslTDA9984/Inc
 * PR572 - change type name of tmbslTDA9984_ to tmbslHdmiTx_
 *
 * *****************  Version 2  *****************
 * User: B.Vereecke      Date: 07/08/07   Time: 10:30
 * Updated in $/Source/tmbslTDA9984/Inc
 * PR551 - Add a new Pattern type in tmbslHdmiTxTestPattern_t
 *			it is used for set the bluescreen
 *
 * *****************  Version 1  *****************
 * User: G. Burnouf    Date: 05/07/07   Time: 17:00
 * Updated in $/Source/tmbslTDA9984/Inc
 * PR 414 : Add new edid management
 *
   \endverbatim
 *
*/

#ifndef TMBSLHDMITX_TYPES_H
#define TMBSLHDMITX_TYPES_H

/*============================================================================*/
/*                       INCLUDE FILES                                        */
/*============================================================================*/

#include "tmNxCompId.h"



#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/*                       MACRO DEFINITIONS                                    */
/*============================================================================*/

/**
 * The maximum number of supported HDMI Transmitter units
 */
#define HDMITX_UNITS_MAX       2

/** \name Errors
 *  The group of error codes returned by all API and internal functions
 */
/*@{*/
/** The base offset for all error codes.
 *  This needs defining as non-zero if this component is integrated with others
 *  and all component error ranges are to be kept separate.
 */
#define TMBSL_ERR_HDMI_BASE	CID_BSL_HDMITX

/** Define the OK code if not defined already */
#ifndef TM_OK
#define TM_OK   0
#endif

/** SW interface compatibility error */
#define TMBSL_ERR_HDMI_COMPATIBILITY            (TMBSL_ERR_HDMI_BASE + 0x001U)

/** SW major version error */
#define TMBSL_ERR_HDMI_MAJOR_VERSION            (TMBSL_ERR_HDMI_BASE + 0x002U)

/** SW component version error */
#define TMBSL_ERR_HDMI_COMP_VERSION             (TMBSL_ERR_HDMI_BASE + 0x003U)

/** Invalid device unit number */
#define TMBSL_ERR_HDMI_BAD_UNIT_NUMBER          (TMBSL_ERR_HDMI_BASE + 0x005U)

/** Invalid input parameter other than unit number */
#define TMBSL_ERR_HDMI_BAD_PARAMETER            (TMBSL_ERR_HDMI_BASE + 0x009U)

/* Ressource not available */
#define TMBSL_ERR_HDMI_RESOURCE_NOT_AVAILABLE   (TMBSL_ERR_HDMI_BASE + 0x00CU)

/** Inconsistent input parameters */
#define TMBSL_ERR_HDMI_INCONSISTENT_PARAMS      (TMBSL_ERR_HDMI_BASE + 0x010U)

/** Component is not initialized */
#define TMBSL_ERR_HDMI_NOT_INITIALIZED          (TMBSL_ERR_HDMI_BASE + 0x011U)

/** Command not supported for current device */
#define TMBSL_ERR_HDMI_NOT_SUPPORTED            (TMBSL_ERR_HDMI_BASE + 0x013U)

/** Initialization failed */
#define TMBSL_ERR_HDMI_INIT_FAILED              (TMBSL_ERR_HDMI_BASE + 0x014U)

/** Component is busy and cannot do a new operation */
#define TMBSL_ERR_HDMI_BUSY                     (TMBSL_ERR_HDMI_BASE + 0x015U)

/** I2C read error */
#define TMBSL_ERR_HDMI_I2C_READ                 (TMBSL_ERR_HDMI_BASE + 0x017U)

/** I2C write error */
#define TMBSL_ERR_HDMI_I2C_WRITE                (TMBSL_ERR_HDMI_BASE + 0x018U)

/** Assertion failure */
#define TMBSL_ERR_HDMI_ASSERTION                (TMBSL_ERR_HDMI_BASE + 0x049U)

/** Bad EDID block checksum */
#define TMBSL_ERR_HDMI_INVALID_STATE            (TMBSL_ERR_HDMI_BASE + 0x066U)
#define TMBSL_ERR_HDMI_INVALID_CHECKSUM         TMBSL_ERR_HDMI_INVALID_STATE

/** No connection to HPD pin */
#define TMBSL_ERR_HDMI_NULL_CONNECTION          (TMBSL_ERR_HDMI_BASE + 0x067U)

/** Not allowed in DVI mode */
#define TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED  (TMBSL_ERR_HDMI_BASE + 0x068U)

/** Maximum error code defined */
#define TMBSL_ERR_HDMI_MAX              TMBSL_ERR_HDMI_OPERATION_NOT_PERMITTED

/*============================================================================*/

#define HDMITX_ENABLE_VP_TABLE_LEN	3
#define HDMITX_GROUND_VP_TABLE_LEN	3

/** EDID block size */
#define EDID_BLOCK_SIZE		    128

/** size descriptor block of monitor descriptor */
#define EDID_MONITOR_DESCRIPTOR_SIZE   13

/*@}*/


/*============================================================================*/
/*                       ENUM OR TYPE DEFINITIONS                             */
/*============================================================================*/

/**
* \brief TX IP/IC versions
*/
	typedef enum {
		BSLHDMITX_UNKNOWN = 0x00,
			       /**< IC/IP is not recognized */
		BSLHDMITX_TDA9984,
			       /**< IC is a TDA9984         */
		BSLHDMITX_TDA9989,
			       /**< IC is a TDA9989 (TDA9989N2 64 balls)         */
		BSLHDMITX_TDA9981,
			       /**< IC is a TDA9981         */
		BSLHDMITX_TDA9983,
			       /**< IC is a TDA9983         */
		BSLHDMITX_TDA19989,
			       /**< IC is a TDA19989        */
		BSLHDMITX_TDA19988,
			       /**< ok, u got it, it's a 19988 :p  */
	} tmbslHdmiTxVersion_t;


/**
 * \brief System function pointer type, to call user I2C read/write functions
 * \param slaveAddr     The I2C slave address
 * \param firstRegister The first device register address to read or write
 * \param lenData       Length of data to read or write (i.e. no. of registers)
 * \param pData         Pointer to data to write, or to buffer to receive data
 * \return              The call result:
 *                      - TM_OK: the call was successful
 *                      - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing
 *                      - TMBSL_ERR_HDMI_I2C_READ:  failed when reading
 */
	typedef struct _tmbslHdmiTxSysArgs_t {
		UInt8 slaveAddr;
		UInt8 firstRegister;
		UInt8 lenData;
		UInt8 *pData;
	} tmbslHdmiTxSysArgs_t;
	typedef tmErrorCode_t(*ptmbslHdmiTxSysFunc_t) (tmbslHdmiTxSysArgs_t *pSysArgs);

/**
 * \brief System function pointer type, to call user I2C EDID read function
 * \param segPtrAddr    The EDID segment pointer address 0 to 7Fh
 * \param segPtr        The EDID segment pointer 0 to 7Fh
 * \param dataRegAddr   The EDID data register address 0 to 7Fh
 * \param wordOffset    The first word offset 0 to FFh to read
 * \param lenData       Length of data to read (i.e. number of registers),
			1 to max starting at wordOffset
 * \param pData         Pointer to buffer to receive lenData data bytes
 * \return              The call result:
 *                      - TM_OK: the call was successful
 *                      - TMBSL_ERR_HDMI_I2C_WRITE: failed when writing
 *                      - TMBSL_ERR_HDMI_I2C_READ:  failed when reading
 */
	typedef struct _tmbslHdmiTxSysArgsEdid_t {
		UInt8 segPtrAddr;
		UInt8 segPtr;
		UInt8 dataRegAddr;
		UInt8 wordOffset;
		UInt8 lenData;
		UInt8 *pData;
	} tmbslHdmiTxSysArgsEdid_t;


/**
 * \brief EDID function pointer type, to call application EDID read function
 * \param pSysArgs pointer to the structure containing necessary information to read EDID
 */
	typedef tmErrorCode_t(*ptmbslHdmiTxSysFuncEdid_t)
	 (tmbslHdmiTxSysArgsEdid_t *pSysArgs);

/**
 * \brief Timer function pointer type, to call an application timer
 * \param ms delay in milliseconds
 */
	typedef Void(*ptmbslHdmiTxSysFuncTimer_t) (UInt16 ms);

/*============================================================================*/
/**
 * \brief Callback function pointer type, to call a user interrupt handler
 * \param txUnit: The transmitter unit that interrupted, 0 to max
 */
	typedef Void(*ptmbslHdmiTxCallback_t) (tmUnitSelect_t txUnit);

/*============================================================================*/
/**
 * EIA/CEA-861B video format type
 */
	typedef enum {
		HDMITX_VFMT_NULL = 0,	    /**< Not a valid format...        */
		HDMITX_VFMT_NO_CHANGE = 0,  /**< ...or no change required     */
		HDMITX_VFMT_TV_MIN = 1,	    /**< Lowest valid TV format       */
		HDMITX_VFMT_01_640x480p_60Hz = 1,
					    /**< Format 01 640  x 480p  60Hz  */
		HDMITX_VFMT_02_720x480p_60Hz = 2,
					    /**< Format 02 720  x 480p  60Hz  */
		HDMITX_VFMT_03_720x480p_60Hz = 3,
					    /**< Format 03 720  x 480p  60Hz  */
		HDMITX_VFMT_04_1280x720p_60Hz = 4,
					    /**< Format 04 1280 x 720p  60Hz  */
		HDMITX_VFMT_05_1920x1080i_60Hz = 5,
					    /**< Format 05 1920 x 1080i 60Hz  */
		HDMITX_VFMT_06_720x480i_60Hz = 6,
					    /**< Format 06 720  x 480i  60Hz  */
		HDMITX_VFMT_07_720x480i_60Hz = 7,
					    /**< Format 07 720  x 480i  60Hz  */
		HDMITX_VFMT_08_720x240p_60Hz = 8,
					    /**< Format 08 720  x 240p  60Hz  */
		HDMITX_VFMT_09_720x240p_60Hz = 9,
					    /**< Format 09 720  x 240p  60Hz  */
		HDMITX_VFMT_10_720x480i_60Hz = 10,
					    /**< Format 10 720  x 480i  60Hz  */
		HDMITX_VFMT_11_720x480i_60Hz = 11,
					    /**< Format 11 720  x 480i  60Hz  */
		HDMITX_VFMT_12_720x240p_60Hz = 12,
					    /**< Format 12 720  x 240p  60Hz  */
		HDMITX_VFMT_13_720x240p_60Hz = 13,
					    /**< Format 13 720  x 240p  60Hz  */
		HDMITX_VFMT_14_1440x480p_60Hz = 14,
					    /**< Format 14 1440 x 480p  60Hz  */
		HDMITX_VFMT_15_1440x480p_60Hz = 15,
					    /**< Format 15 1440 x 480p  60Hz  */
		HDMITX_VFMT_16_1920x1080p_60Hz = 16,
					    /**< Format 16 1920 x 1080p 60Hz  */
		HDMITX_VFMT_17_720x576p_50Hz = 17,
					    /**< Format 17 720  x 576p  50Hz  */
		HDMITX_VFMT_18_720x576p_50Hz = 18,
					    /**< Format 18 720  x 576p  50Hz  */
		HDMITX_VFMT_19_1280x720p_50Hz = 19,
					    /**< Format 19 1280 x 720p  50Hz  */
		HDMITX_VFMT_20_1920x1080i_50Hz = 20,
					    /**< Format 20 1920 x 1080i 50Hz  */
		HDMITX_VFMT_21_720x576i_50Hz = 21,
					    /**< Format 21 720  x 576i  50Hz  */
		HDMITX_VFMT_22_720x576i_50Hz = 22,
					    /**< Format 22 720  x 576i  50Hz  */
		HDMITX_VFMT_23_720x288p_50Hz = 23,
					    /**< Format 23 720  x 288p  50Hz  */
		HDMITX_VFMT_24_720x288p_50Hz = 24,
					    /**< Format 24 720  x 288p  50Hz  */
		HDMITX_VFMT_25_720x576i_50Hz = 25,
					    /**< Format 25 720  x 576i  50Hz  */
		HDMITX_VFMT_26_720x576i_50Hz = 26,
					    /**< Format 26 720  x 576i  50Hz  */
		HDMITX_VFMT_27_720x288p_50Hz = 27,
					    /**< Format 27 720  x 288p  50Hz  */
		HDMITX_VFMT_28_720x288p_50Hz = 28,
					    /**< Format 28 720  x 288p  50Hz  */
		HDMITX_VFMT_29_1440x576p_50Hz = 29,
					    /**< Format 29 1440 x 576p  50Hz  */
		HDMITX_VFMT_30_1440x576p_50Hz = 30,
					    /**< Format 30 1440 x 576p  50Hz  */
		HDMITX_VFMT_31_1920x1080p_50Hz = 31,
					    /**< Format 31 1920 x 1080p 50Hz  */
		HDMITX_VFMT_32_1920x1080p_24Hz = 32,
					    /**< Format 32 1920 x 1080p 24Hz  */

		HDMITX_VFMT_TV_NO_REG_MIN = 32,
					    /**< Lowest TV format without prefetched table */

		HDMITX_VFMT_33_1920x1080p_25Hz = 33,
					    /**< Format 33 1920 x 1080p 25Hz  */
		HDMITX_VFMT_34_1920x1080p_30Hz = 34,
					    /**< Format 34 1920 x 1080p 30Hz  */
		HDMITX_VFMT_35_2880x480p_60Hz = 35,
					   /**< Format 35 2880 x 480p  60Hz 4:3  */
		HDMITX_VFMT_36_2880x480p_60Hz = 36,
					   /**< Format 36 2880 x 480p  60Hz 16:9 */
		HDMITX_VFMT_37_2880x576p_50Hz = 37,
					   /**< Format 37 2880 x 576p  50Hz 4:3  */
		HDMITX_VFMT_38_2880x576p_50Hz = 38,
					   /**< Format 38 2880 x 576p  50Hz 16:9 */

		HDMITX_VFMT_INDEX_60_1280x720p_24Hz = 39,
					     /**< Index of HDMITX_VFMT_60_1280x720p_24Hz */
		HDMITX_VFMT_60_1280x720p_24Hz = 60,
					   /**< Format 60 1280 x 720p  23.97/24Hz 16:9 */
		HDMITX_VFMT_61_1280x720p_25Hz = 61,
					   /**< Format 61 1280 x 720p  25Hz 16:9 */
		HDMITX_VFMT_62_1280x720p_30Hz = 62,
					   /**< Format 60 1280 x 720p  29.97/30Hz 16:9 */

		HDMITX_VFMT_TV_MAX = 62,    /**< Highest valid TV format value      */
		HDMITX_VFMT_TV_NUM = 42,    /**< Number of TV formats + null, it is also the Index of PC_MIN */

		HDMITX_VFMT_PC_MIN = 128,   /**< Lowest valid PC format       */
		HDMITX_VFMT_PC_640x480p_60Hz = 128,
					    /**< PC format 128                */
		HDMITX_VFMT_PC_800x600p_60Hz = 129,
					    /**< PC format 129                */
		HDMITX_VFMT_PC_1152x960p_60Hz = 130,
					    /**< PC format 130                */
		HDMITX_VFMT_PC_1024x768p_60Hz = 131,
					    /**< PC format 131                */
		HDMITX_VFMT_PC_1280x768p_60Hz = 132,
					    /**< PC format 132                */
		HDMITX_VFMT_PC_1280x1024p_60Hz = 133,
					    /**< PC format 133                */
		HDMITX_VFMT_PC_1360x768p_60Hz = 134,
					    /**< PC format 134                */
		HDMITX_VFMT_PC_1400x1050p_60Hz = 135,
					    /**< PC format 135                */
		HDMITX_VFMT_PC_1600x1200p_60Hz = 136,
					    /**< PC format 136                */
		HDMITX_VFMT_PC_1024x768p_70Hz = 137,
					    /**< PC format 137                */
		HDMITX_VFMT_PC_640x480p_72Hz = 138,
					    /**< PC format 138                */
		HDMITX_VFMT_PC_800x600p_72Hz = 139,
					    /**< PC format 139                */
		HDMITX_VFMT_PC_640x480p_75Hz = 140,
					    /**< PC format 140                */
		HDMITX_VFMT_PC_1024x768p_75Hz = 141,
					    /**< PC format 141                */
		HDMITX_VFMT_PC_800x600p_75Hz = 142,
					    /**< PC format 142                */
		HDMITX_VFMT_PC_1024x864p_75Hz = 143,
					    /**< PC format 143                */
		HDMITX_VFMT_PC_1280x1024p_75Hz = 144,
					    /**< PC format 144                */
		HDMITX_VFMT_PC_640x350p_85Hz = 145,
					    /**< PC format 145                */
		HDMITX_VFMT_PC_640x400p_85Hz = 146,
					    /**< PC format 146                */
		HDMITX_VFMT_PC_720x400p_85Hz = 147,
					    /**< PC format 147                */
		HDMITX_VFMT_PC_640x480p_85Hz = 148,
					    /**< PC format 148                */
		HDMITX_VFMT_PC_800x600p_85Hz = 149,
					    /**< PC format 149                */
		HDMITX_VFMT_PC_1024x768p_85Hz = 150,
					    /**< PC format 150                */
		HDMITX_VFMT_PC_1152x864p_85Hz = 151,
					    /**< PC format 151                */
		HDMITX_VFMT_PC_1280x960p_85Hz = 152,
					    /**< PC format 152                */
		HDMITX_VFMT_PC_1280x1024p_85Hz = 153,
					    /**< PC format 153                */
		HDMITX_VFMT_PC_1024x768i_87Hz = 154,
					    /**< PC format 154                */
		HDMITX_VFMT_PC_MAX = 154,   /**< Highest valid PC format      */
		HDMITX_VFMT_PC_NUM = (1 + 154 - 128)
						/**< Number of PC formats         */
	} tmbslHdmiTxVidFmt_t;

/*============================================================================*/
/**
 * tmbslTDA9984AudioInSetConfig() parameter types
 */
/** Audio input formats */
	typedef enum {
		HDMITX_AFMT_SPDIF = 0,	/**< SPDIF               */
		HDMITX_AFMT_I2S = 1,	/**< I2S                 */
		HDMITX_AFMT_OBA = 2,	/**< One bit audio / DSD */
		HDMITX_AFMT_DST = 3,	/**< DST                 */
		HDMITX_AFMT_HBR = 4,	/**< HBR                 */
		HDMITX_AFMT_NUM = 5,	/**< Number of Audio input format */
		HDMITX_AFMT_INVALID = 5	/**< Invalid format      */
	} tmbslHdmiTxaFmt_t;

/** I2s formats */
	typedef enum {
		HDMITX_I2SFOR_PHILIPS_L = 0,
					/**< Philips like format */
		HDMITX_I2SFOR_OTH_L = 2,/**< Other non Philips left justified */
		HDMITX_I2SFOR_OTH_R = 3,/**< Other non Philips right justified */
		HDMITX_I2SFOR_INVALID = 4
					/**< Invalid format*/
	} tmbslHdmiTxI2sFor_t;

/** DSD clock polarities */
	typedef enum {
		HDMITX_CLKPOLDSD_ACLK = 0,
					/**< Same as ACLK */
		HDMITX_CLKPOLDSD_NACLK = 1,
					/**< Not ACLK, i.e. inverted */
		HDMITX_CLKPOLDSD_NO_CHANGE = 2,
					/**< No change    */
		HDMITX_CLKPOLDSD_INVALID = 3
					/**< Invalid      */
	} tmbslHdmiTxClkPolDsd_t;

/** DSD data swap values */
	typedef enum {
		HDMITX_SWAPDSD_OFF = 0,	/**< No swap   */
		HDMITX_SWAPDSD_ON = 1,	/**< Swap      */
		HDMITX_SWAPDSD_NO_CHANGE = 2,
					/**< No change */
		HDMITX_SWAPDSD_INVALID = 3
					/**< Invalid   */
	} tmbslHdmiTxSwapDsd_t;

/** DST data transfer rates */
	typedef enum {
		HDMITX_DSTRATE_SINGLE = 0,
					/**< Single transfer rate */
		HDMITX_DSTRATE_DOUBLE = 1,
					/**< Double data rate */
		HDMITX_DSTRATE_NO_CHANGE = 2,
					/**< No change */
		HDMITX_DSTRATE_INVALID = 3
					/**< Invalid   */
	} tmbslHdmiTxDstRate_t;

/** I2S, SPDIF and DSD channel allocation values */
	enum _tmbslHdmiTxChan {
		HDMITX_CHAN_MIN = 0,
		HDMITX_CHAN_MAX = 31,
		HDMITX_CHAN_NO_CHANGE = 32,
		HDMITX_CHAN_INVALID = 33
	};

/** Audio layout values */
	enum _tmbslHdmiTxLayout {
		HDMITX_LAYOUT_MIN = 0,
		HDMITX_LAYOUT_MAX = 1,
		HDMITX_LAYOUT_NO_CHANGE = 2,
		HDMITX_LAYOUT_INVALID = 3
	};

/** Audio FIFO read latency values */
	enum _tmbslHdmiTxlatency_rd {
		HDMITX_LATENCY_MIN = 0x000,
		HDMITX_LATENCY_CURRENT = 0x080,
		HDMITX_LATENCY_MAX = 0x0FF,
		HDMITX_LATENCY_NO_CHANGE = 0x100,
		HDMITX_LATENCY_INVALID = 0x101
	};

/*============================================================================*/
/**
 * tmbslTDA9984AudioInSetCts() parameter types
 */
/** Clock Time Stamp reference source */
	typedef enum {
		HDMITX_CTSREF_ACLK = 0,
				    /**< Clock input pin for I2S       */
		HDMITX_CTSREF_MCLK = 1,
				    /**< Clock input pin for EXTREF    */
		HDMITX_CTSREF_FS64SPDIF = 2,
				    /**< 64xsample rate, for SPDIF     */
		HDMITX_CTSREF_INVALID = 3
				    /**< Invalid value                 */
	} tmbslHdmiTxctsRef_t;

/** Audio sample rate kHz indexes */
	typedef enum {
		HDMITX_AFS_32k = 0,   /**< 32kHz    */
		HDMITX_AFS_44_1k = 1, /**< 44.1kHz  */
		HDMITX_AFS_48K = 2,   /**< 48kHz    */
		HDMITX_AFS_88_2K = 3, /**< 88.2kHz  */
		HDMITX_AFS_96K = 4,   /**< 96kHz    */
		HDMITX_AFS_176_4K = 5,/**< 176.4kHz */
		HDMITX_AFS_192K = 6,  /**< 192kHz   */
		HDMITX_AFS_768K = 7,  /**< 768kHz   */
		HDMITX_AFS_NOT_INDICATED = 8,
				      /**< Not Indicated (Channel Status) */
		HDMITX_AFS_INVALID = 8,
				      /**< Invalid  */
		HDMITX_AFS_NUM = 8    /**< # rates  */
	} tmbslHdmiTxafs_t;

/** Vertical output frequencies */
	typedef enum {
		HDMITX_VFREQ_24Hz = 0,
				    /**< 24Hz          */
		HDMITX_VFREQ_25Hz = 1,	/**< 25Hz          */
		HDMITX_VFREQ_30Hz = 2,	/**< 30Hz          */
		HDMITX_VFREQ_50Hz = 3,
				    /**< 50Hz          */
		HDMITX_VFREQ_59Hz = 4,
				    /**< 59.94Hz       */
		HDMITX_VFREQ_60Hz = 5,
				    /**< 60Hz          */
#ifndef FORMAT_PC
		HDMITX_VFREQ_INVALID = 6,
				    /**< Invalid       */
		HDMITX_VFREQ_NUM = 6/**< No. of values */
#else				/* FORMAT_PC */
		HDMITX_VFREQ_70Hz = 6,
				    /**< 70Hz          */
		HDMITX_VFREQ_72Hz = 7,
				    /**< 72Hz          */
		HDMITX_VFREQ_75Hz = 8,
				    /**< 75Hz          */
		HDMITX_VFREQ_85Hz = 9,
				    /**< 85Hz          */
		HDMITX_VFREQ_87Hz = 10,
				    /**< 87Hz          */
		HDMITX_VFREQ_INVALID = 11,
				    /**< Invalid       */
		HDMITX_VFREQ_NUM = 11
				    /**< No. of values */
#endif				/* FORMAT_PC */
	} tmbslHdmiTxVfreq_t;

/** Clock Time Stamp predivider - scales N */
	typedef enum {
		HDMITX_CTSK1 = 0,   /**< k=1 */
		HDMITX_CTSK2 = 1,   /**< k=2 */
		HDMITX_CTSK3 = 2,   /**< k=3 */
		HDMITX_CTSK4 = 3,   /**< k=4 */
		HDMITX_CTSK8 = 4,   /**< k=8 */
		HDMITX_CTSK_USE_CTSX = 5,
				    /**< Calculate from ctsX factor */
		HDMITX_CTSK_INVALID = 6
				    /**< Invalid */
	} tmbslHdmiTxctsK_t;

/** Clock Time Stamp postdivider measured time stamp */
	typedef enum {
		HDMITX_CTSMTS = 0,  /**< =mts   */
		HDMITX_CTSMTS2 = 1, /**< =mts%2 */
		HDMITX_CTSMTS4 = 2, /**< =mts%4 */
		HDMITX_CTSMTS8 = 3, /**< =mts%8 */
		HDMITX_CTSMTS_USE_CTSX = 4,
				    /**< Calculate from ctsX factor */
		HDMITX_CTSMTS_INVALID = 5
				    /**< Invalid */
	} tmbslHdmiTxctsM_t;

/** Cycle Time Stamp values */
	enum _tmbslHdmiTxCts {
		HDMITX_CTS_AUTO = 0,
		HDMITX_CTS_MIN = 0x000001
	};

/** Cycle Time Stamp X factors */
	enum _tmbslHdmiTxCtsX {
		HDMITX_CTSX_16 = 0,
		HDMITX_CTSX_32 = 1,
		HDMITX_CTSX_48 = 2,
		HDMITX_CTSX_64 = 3,
		HDMITX_CTSX_128 = 4,
		HDMITX_CTSX_NUM = 5,
		HDMITX_CTSX_UNUSED = 5,
				    /**< CTX value unused when K and Mts used */
		HDMITX_CTSX_INVALID = 6
	};


/*============================================================================*/
/**
 * tmbslTDA9984AudioOutSetChanStatus() parameter types
 */

	typedef enum {
		HDMITX_AUDIO_DATA_PCM = 0,
				  /**< Main data field represents linear PCM samples.    */
		HDMITX_AUDIO_DATA_OTHER = 1,
				  /**< Main data field used for purposes other purposes. */
		HDMITX_AUDIO_DATA_INVALID = 2
				  /**< Invalid value */
	} tmbslHdmiTxAudioData_t;

/** BYTE 0: Channel Status Format information */
	typedef enum {
		HDMITX_CSFI_PCM_2CHAN_NO_PRE = 0,
					  /**< PCM 2 channels without pre-emphasis           */
		HDMITX_CSFI_PCM_2CHAN_PRE = 1,
					  /**< PCM 2 channels with 50us/15us pre-emphasis    */
		HDMITX_CSFI_PCM_2CHAN_PRE_RSVD1 = 2,
					  /**< PCM Reserved for 2 channels with pre-emphasis */
		HDMITX_CSFI_PCM_2CHAN_PRE_RSVD2 = 3,
					  /**< PCM Reserved for 2 channels with pre-emphasis */
		HDMITX_CSFI_NOTPCM_DEFAULT = 4,
					  /**< Non-PCM Default state                         */
		HDMITX_CSFI_INVALID = 5	  /**< Invalid value                                 */
	} tmbslHdmiTxCSformatInfo_t;

/** BYTE 0: Channel Status Copyright assertion */
	typedef enum {
		HDMITX_CSCOPYRIGHT_PROTECTED = 0,
					 /**< Copyright protected     */
		HDMITX_CSCOPYRIGHT_UNPROTECTED = 1,
					 /**< Not copyright protected */
		HDMITX_CSCOPYRIGHT_INVALID = 2
					 /**< Invalid value           */
	} tmbslHdmiTxCScopyright_t;

/** BYTE 3: Channel Status Clock Accuracy */
	typedef enum {
		HDMITX_CSCLK_LEVEL_II = 0,
				      /**< Level II                     */
		HDMITX_CSCLK_LEVEL_I = 1,
				      /**< Level I                      */
		HDMITX_CSCLK_LEVEL_III = 2,
				      /**< Level III                    */
		HDMITX_CSCLK_NOT_MATCHED = 3,
				      /**< Not matched to sample freq.  */
		HDMITX_CSCLK_INVALID = 4
				      /**< Invalid                      */
	} tmbslHdmiTxCSclkAcc_t;

/** BYTE 4: Channel Status Maximum sample word length */
	typedef enum {
		HDMITX_CSMAX_LENGTH_20 = 0,
				   /**< Max word length is 20 bits   */
		HDMITX_CSMAX_LENGTH_24 = 1,
				   /**< Max word length is 24 bits   */
		HDMITX_CSMAX_INVALID = 2
				   /**< Invalid value                */
	} tmbslHdmiTxCSmaxWordLength_t;


/** BYTE 4: Channel Status Sample word length */
	typedef enum {
		HDMITX_CSWORD_DEFAULT = 0,  /**< Word length is not indicated                    */
		HDMITX_CSWORD_20_OF_24 = 1, /**< Sample length is 20 bits out of max 24 possible */
		HDMITX_CSWORD_16_OF_20 = 1, /**< Sample length is 16 bits out of max 20 possible */
		HDMITX_CSWORD_22_OF_24 = 2, /**< Sample length is 22 bits out of max 24 possible */
		HDMITX_CSWORD_18_OF_20 = 2, /**< Sample length is 18 bits out of max 20 possible */
		HDMITX_CSWORD_RESVD = 3,    /**< Reserved - shall not be used */
		HDMITX_CSWORD_23_OF_24 = 4, /**< Sample length is 23 bits out of max 24 possible */
		HDMITX_CSWORD_19_OF_20 = 4, /**< Sample length is 19 bits out of max 20 possible */
		HDMITX_CSWORD_24_OF_24 = 5, /**< Sample length is 24 bits out of max 24 possible */
		HDMITX_CSWORD_20_OF_20 = 5, /**< Sample length is 20 bits out of max 20 possible */
		HDMITX_CSWORD_21_OF_24 = 6, /**< Sample length is 21 bits out of max 24 possible */
		HDMITX_CSWORD_17_OF_20 = 6, /**< Sample length is 17 bits out of max 20 possible */
		HDMITX_CSWORD_INVALID = 7   /**< Invalid */
	} tmbslHdmiTxCSwordLength_t;

/** BYTE 4: Channel Status Original sample frequency */
	typedef enum {
		HDMITX_CSOFREQ_NOT_INDICATED = 0,
					/**< Not Indicated */
		HDMITX_CSOFREQ_192k = 1,/**< 192kHz        */
		HDMITX_CSOFREQ_12k = 2,	/**< 12kHz         */
		HDMITX_CSOFREQ_176_4k = 3,
					/**< 176.4kHz      */
		HDMITX_CSOFREQ_RSVD1 = 4,
					/**< Reserved      */
		HDMITX_CSOFREQ_96k = 5,	/**< 96kHz         */
		HDMITX_CSOFREQ_8k = 6,	/**< 8kHz          */
		HDMITX_CSOFREQ_88_2k = 7,
					/**< 88.2kHz       */
		HDMITX_CSOFREQ_16k = 8,	/**< 16kHz         */
		HDMITX_CSOFREQ_24k = 9,	/**< 24kHz         */
		HDMITX_CSOFREQ_11_025k = 10,
					/**< 11.025kHz     */
		HDMITX_CSOFREQ_22_05k = 11,
					/**< 22.05kHz      */
		HDMITX_CSOFREQ_32k = 12,/**< 32kHz         */
		HDMITX_CSOFREQ_48k = 13,/**< 48kHz         */
		HDMITX_CSOFREQ_RSVD2 = 14,
					/**< Reserved      */
		HDMITX_CSOFREQ_44_1k = 15,
					/**< 44.1kHz       */
		HDMITX_CSAFS_INVALID = 16
					/**< Invalid value */
	} tmbslHdmiTxCSorigAfs_t;

/*============================================================================*/
/**
 * tmbslTDA9984AudioOutSetChanStatusMapping() parameter types
 */
/** Channel Status source/channel number limits */
	enum _tmbslHdmiTxChanStatusChanLimits {
		HDMITX_CS_CHANNELS_MAX = 0x0F,
		HDMITX_CS_SOURCES_MAX = 0x0F
	};

/*============================================================================*/
/**
 * tmbslTDA9984AudioOutSetMute() parameter type
 */
/** Audio mute state */
	typedef enum {
		HDMITX_AMUTE_OFF = 0,
				   /**< Mute off */
		HDMITX_AMUTE_ON = 1,
				   /**< Mute on  */
		HDMITX_AMUTE_INVALID = 2
				   /**< Invalid  */
	} tmbslHdmiTxaMute_t;

/** Number of 3 byte Short Audio Descriptors stored in pEdidAFmts */
#define HDMI_TX_SAD_MAX_CNT     30

/*============================================================================*/
/**
 * tmbslTDA9984EdidGetBlockData() parameter types
 */
/** An enum to represent the current EDID status */
	enum _tmbslHdmiTxEdidSta_t {
		HDMITX_EDID_READ = 0,	/* All blocks read OK */
		HDMITX_EDID_READ_INCOMPLETE = 1,	/* All blocks read OK but buffer too
							   small to return all of them */
		HDMITX_EDID_ERROR_CHK_BLOCK_0 = 2,	/* Block 0 checksum error */

		HDMITX_EDID_ERROR_CHK = 3,	/* Block 0 OK, checksum error in one
						   or more other blocks */
		HDMITX_EDID_NOT_READ = 4,	/* EDID not read */

		HDMITX_EDID_STATUS_INVALID = 5
					    /**< Invalid   */
	};

/*============================================================================*/
/**
 * tmbslTDA9984EdidGetSinkType() parameter types
 */
/** Sink device type */
	typedef enum {
		HDMITX_SINK_DVI = 0,	   /**< DVI  */
		HDMITX_SINK_HDMI = 1,	   /**< HDMI */
		HDMITX_SINK_EDID = 2,	   /**< As currently defined in EDID */
		HDMITX_SINK_INVALID = 3	   /**< Invalid   */
	} tmbslHdmiTxSinkType_t;

/*============================================================================*/
/**
 * \brief The tmbslTDA9984EdidGetVideoPreferred() parameter type
 * Detailed timining description structure
 */
	typedef struct _tmbslHdmiTxEdidDtd_t {
		UInt16 uPixelClock;
				/**< Pixel Clock/10,000         */
		UInt16 uHActivePixels;
				/**< Horizontal Active Pixels   */
		UInt16 uHBlankPixels;
				/**< Horizontal Blanking Pixels */
		UInt16 uVActiveLines;
				/**< Vertical Active Lines      */
		UInt16 uVBlankLines;
				/**< Vertical Blanking Lines    */
		UInt16 uHSyncOffset;
				/**< Horizontal Sync Offset     */
		UInt16 uHSyncWidth;
				/**< Horiz. Sync Pulse Width    */
		UInt16 uVSyncOffset;
				/**< Vertical Sync Offset       */
		UInt16 uVSyncWidth;
				/**< Vertical Sync Pulse Width  */
		UInt16 uHImageSize;
				/**< Horizontal Image Size      */
		UInt16 uVImageSize;
				/**< Vertical Image Size        */
		UInt16 uHBorderPixels;
				/**< Horizontal Border          */
		UInt16 uVBorderPixels;
				/**< Vertical Border            */
		UInt8 Flags;	/**< Interlace/sync info        */
	} tmbslHdmiTxEdidDtd_t;


/*============================================================================*/
/**
 * First monitor descriptor structure
 */
	typedef struct _tmbslHdmiTxEdidFirstMD_t {
		Bool bDescRecord;			    /**< True when parameters of struct are available   */
		UInt8 uMonitorName[EDID_MONITOR_DESCRIPTOR_SIZE];
							    /**< Monitor Name                                   */
	} tmbslHdmiTxEdidFirstMD_t;

/*============================================================================*/
/**
 * Second monitor descriptor structure
 */
	typedef struct _tmbslHdmiTxEdidSecondMD_t {
		Bool bDescRecord;			    /**< True when parameters of struct are available   */
		UInt8 uMinVerticalRate;			    /**< Min vertical rate in Hz                        */
		UInt8 uMaxVerticalRate;			    /**< Max vertical rate in Hz                        */
		UInt8 uMinHorizontalRate;		    /**< Min horizontal rate in Hz                      */
		UInt8 uMaxHorizontalRate;		    /**< Max horizontal rate in Hz                      */
		UInt8 uMaxSupportedPixelClk;		    /**< Max suuported pixel clock rate in MHz          */
	} tmbslHdmiTxEdidSecondMD_t;

/*============================================================================*/
/**
 * Other monitor descriptor structure
 */
	typedef struct _tmbslHdmiTxEdidOtherMD_t {
		Bool bDescRecord;			    /**< True when parameters of struct are available   */
		UInt8 uOtherDescriptor[EDID_MONITOR_DESCRIPTOR_SIZE];
							    /**< Other monitor Descriptor                       */
	} tmbslHdmiTxEdidOtherMD_t;

/*============================================================================*/
/**
 * basic display parameters structure
 */
	typedef struct _tmbslHdmiTxEdidBDParam_t {
		UInt8 uVideoInputDef;
				 /**< Video Input Definition                       */
		UInt8 uMaxHorizontalSize;
				 /**< Max. Horizontal Image Size in cm             */
		UInt8 uMaxVerticalSize;
				 /**< Max. Vertical Image Size in cm               */
		UInt8 uGamma;	 /**< Gamma                                        */
		UInt8 uFeatureSupport;
				 /**< Feature support                              */
	} tmbslHdmiTxEdidBDParam_t;

/*============================================================================*/
/**
 * \brief The tmbslTDA9984EdidGetAudioCapabilities() parameter type
 */
	typedef struct _tmbslHdmiTxEdidSad_t {
		UInt8 ModeChans;	/* Bits[6:3]: EIA/CEA861 mode; Bits[2:0]: channels */
		UInt8 Freqs;	/* Bits for each supported frequency */
		UInt8 Byte3;	/* EIA/CEA861B p83: data depending on audio mode */
	} tmbslHdmiTxEdidSad_t;

/*============================================================================*/
/**
 * \brief struc to store parameter provide by function tmbslTDA9984EdidRequestBlockData()
 */
	typedef struct _tmbslHdmiTxEdidToApp_t {
		UInt8 *pRawEdid;	/* pointer on a tab to store edid requested by application */
		Int numBlocks;	/* number of edid block requested by application */
	} tmbslHdmiTxEdidToApp_t;

/*============================================================================*/
/**
 *  tmbslTDA9984EdidGetVideoCapabilities() parameter types
 */
/** Number of 1 byte Short Video Descriptors stored in pEdidVFmts */
#define HDMI_TX_SVD_MAX_CNT     113

/** number of detailed timing descriptor stored in BSL */
#define NUMBER_DTD_STORED       10



/** Flag set in Short Video Descriptor to indicate native format */
#define HDMI_TX_SVD_NATIVE_MASK 0x80
#define HDMI_TX_SVD_NATIVE_NOT  0x7F

/** Video capability flags */
	enum _tmbslHdmiTxVidCap_t {
		HDMITX_VIDCAP_UNDERSCAN = 0x80,
					/**< Underscan supported */
		HDMITX_VIDCAP_YUV444 = 0x40,
					/**< YCbCr 4:4:4 supported */
		HDMITX_VIDCAP_YUV422 = 0x20,
					/**< YCbCr 4:2:2 supported */
		HDMITX_VIDCAP_UNUSED = 0x1F
					/**< Unused flags */
	};

/*============================================================================*/
/**
 * tmbslTDA9984HdcpCheck() parameter type
 */
/** HDCP check result */
	typedef enum {
		HDMITX_HDCP_CHECK_NOT_STARTED = 0,
						/**< Check not started */
		HDMITX_HDCP_CHECK_IN_PROGRESS = 1,
						/**< No failures, more to do */
		HDMITX_HDCP_CHECK_PASS = 2,	/**< Final check has passed */
		HDMITX_HDCP_CHECK_FAIL_FIRST = 3,
						/**< First check failure code */
		HDMITX_HDCP_CHECK_FAIL_DRIVER_STATE = 3,
						/**< Driver not AUTHENTICATED */
		HDMITX_HDCP_CHECK_FAIL_DEVICE_T0 = 4,
						/**< A T0 interrupt occurred */
		HDMITX_HDCP_CHECK_FAIL_DEVICE_RI = 5,
						/**< Device RI changed */
		HDMITX_HDCP_CHECK_FAIL_DEVICE_FSM = 6,
						/**< Device FSM not 10h */
		HDMITX_HDCP_CHECK_NUM = 7	/**< Number of check results */
	} tmbslHdmiTxHdcpCheck_t;

/*============================================================================*/
/**
 * tmbslTDA9984HdcpConfigure() parameter type
 */
/** HDCP DDC slave addresses */
	enum _tmbslHdmiTxHdcpSlaveAddress {
		HDMITX_HDCP_SLAVE_PRIMARY = 0x74,
		HDMITX_HDCP_SLAVE_SECONDARY = 0x76
	};

/** HDCP transmitter modes */
	typedef enum {
		HDMITX_HDCP_TXMODE_NOT_SET = 0,
		HDMITX_HDCP_TXMODE_REPEATER = 1,
		HDMITX_HDCP_TXMODE_TOP_LEVEL = 2,
		HDMITX_HDCP_TXMODE_MAX = 2
	} tmbslHdmiTxHdcpTxMode_t;

/** HDCP option flags */
	typedef enum {
		HDMITX_HDCP_OPTION_FORCE_PJ_IGNORED = 0x01,	/* Not set: obey PJ result     */
		HDMITX_HDCP_OPTION_FORCE_SLOW_DDC = 0x02,	/* Not set: obey BCAPS setting */
		HDMITX_HDCP_OPTION_FORCE_NO_1_1 = 0x04,	/* Not set: obey BCAPS setting */
		HDMITX_HDCP_OPTION_FORCE_REPEATER = 0x08,	/* Not set: obey BCAPS setting */
		HDMITX_HDCP_OPTION_FORCE_NO_REPEATER = 0x10,	/* Not set: obey BCAPS setting */
		HDMITX_HDCP_OPTION_FORCE_V_EQU_VBAR = 0x20,	/* Not set: obey V=V' result   */
		HDMITX_HDCP_OPTION_FORCE_VSLOW_DDC = 0x40,	/* Set: 50kHz DDC */
		HDMITX_HDCP_OPTION_DEFAULT = 0x00,	/* All the above Not Set vals */
		HDMITX_HDCP_OPTION_MASK = 0x7F,	/* Only these bits are allowed */
		HDMITX_HDCP_OPTION_MASK_BAD = 0x80	/* These bits are not allowed  */
	} tmbslHdmiTxHdcpOptions_t;

/*============================================================================*/
/**
 * tmbslTDA9984HdcpDownloadKeys() parameter type
 */
/** HDCP decryption mode */
	typedef enum {
		HDMITX_HDCP_DECRYPT_DISABLE = 0,
		HDMITX_HDCP_DECRYPT_ENABLE = 1,
		HDMITX_HDCP_DECRYPT_MAX = 1
	} tmbslHdmiTxDecrypt_t;

/*============================================================================*/
/**
 * tmbslTDA9984HdcpHandleBSTATUS() parameter type
 */
/** BSTATUS bit fields */
	enum _tmbslHdmiTxHdcpHandleBSTATUS {
		HDMITX_HDCP_BSTATUS_HDMI_MODE = 0x1000,
		HDMITX_HDCP_BSTATUS_MAX_CASCADE_EXCEEDED = 0x0800,
		HDMITX_HDCP_BSTATUS_CASCADE_DEPTH = 0x0700,
		HDMITX_HDCP_BSTATUS_MAX_DEVS_EXCEEDED = 0x0080,
		HDMITX_HDCP_BSTATUS_DEVICE_COUNT = 0x007F
	};

/*============================================================================*/
/**
 * tmbslTDA9984HdcpHandleSHA_1() parameter types
 */
/** KSV list sizes */
	enum _tmbslHdmiTxHdcpHandleSHA_1 {
		HDMITX_KSV_LIST_MAX_DEVICES = 128,
		HDMITX_KSV_BYTES_PER_DEVICE = 5
	};

/*============================================================================*/
/**
 * tmbslTDA9984HotPlugGetStatus() parameter type
 */
/** Current hotplug status */
	typedef enum {
		HDMITX_HOTPLUG_INACTIVE = 0,
				       /**< Hotplug inactive */
		HDMITX_HOTPLUG_ACTIVE = 1,
				       /**< Hotplug active   */
		HDMITX_HOTPLUG_INVALID = 2
				       /**< Invalid Hotplug  */
	} tmbslHdmiTxHotPlug_t;

/*============================================================================*/
/**
 * tmbslTDA9984RxSenseGetStatus() parameter type
 */
/** Current RX Sense status */
	typedef enum {
		HDMITX_RX_SENSE_INACTIVE = 0,
					/**< RxSense inactive */
		HDMITX_RX_SENSE_ACTIVE = 1,
					/**< RxSense active   */
		HDMITX_RX_SENSE_INVALID = 2
					/**< Invalid RxSense  */
	} tmbslHdmiTxRxSense_t;

/*============================================================================*/
/**
 * tmbslTDA9984HwGetCapabilities() parameter type
 */
/** List of HW features that may be supported by HW */
	typedef enum {
		HDMITX_FEATURE_HW_HDCP = 0,  /**< HDCP   feature */
		HDMITX_FEATURE_HW_SCALER = 1,/**< Scaler feature */
		HDMITX_FEATURE_HW_AUDIO_OBA = 2,
					     /**< One Bit Audio feature */
		HDMITX_FEATURE_HW_AUDIO_DST = 3,
					     /**< DST Audio feature */
		HDMITX_FEATURE_HW_AUDIO_HBR = 4,
					     /**< HBR Audio feature */
		HDMITX_FEATURE_HW_HDMI_1_1 = 5,
					     /**< HDMI 1.1 feature */
		HDMITX_FEATURE_HW_HDMI_1_2A = 6,
					     /**< HDMI 1.2a feature */
		HDMITX_FEATURE_HW_HDMI_1_3A = 7,
					     /**< HDMI 1.3a feature */
		HDMITX_FEATURE_HW_DEEP_COLOR_30 = 8,
					     /**< 30 bits deep color support */
		HDMITX_FEATURE_HW_DEEP_COLOR_36 = 9,
					     /**< 36 bits deep color support */
		HDMITX_FEATURE_HW_DEEP_COLOR_48 = 11,
					     /**< 48 bits deep color support */
		HDMITX_FEATURE_HW_UPSAMPLER = 12,
					     /**< Up sampler feature */
		HDMITX_FEATURE_HW_DOWNSAMPLER = 13,
					     /**< Down sampler feature */
		HDMITX_FEATURE_HW_COLOR_CONVERSION = 14
					     /**< Color conversion matrix */
	} tmbslHdmiTxHwFeature_t;

/*============================================================================*/
/**
 * tmbslTDA9984Init() parameter types
 */
/** Supported range of I2C slave addresses */
	enum _tmbslHdmiTxSlaveAddress {
		HDMITX_SLAVE_ADDRESS_MIN = 1,
		HDMITX_SLAVE_ADDRESS_MAX = 127
	};

/**
 * Indexes into the funcCallback[] array of interrupt callback function pointers
 */
	typedef enum _tmbslHdmiTxCallbackInt {
		HDMITX_CALLBACK_INT_SECURITY = 0,
					      /**< HDCP encryption switched off     */
		HDMITX_CALLBACK_INT_ENCRYPT = 0,
					      /**< HDCP encrypt as above (Obsolete) */
		HDMITX_CALLBACK_INT_HPD = 1,  /**< Transition on HPD input          */
		HDMITX_CALLBACK_INT_T0 = 2,   /**< HDCP state machine in state T0   */
		HDMITX_CALLBACK_INT_BCAPS = 3,/**< BCAPS available                  */
		HDMITX_CALLBACK_INT_BSTATUS = 4,
					      /**< BSTATUS available                */
		HDMITX_CALLBACK_INT_SHA_1 = 5,/**< sha-1(ksv,bstatus,m0)=V'         */
		HDMITX_CALLBACK_INT_PJ = 6,   /**< pj=pj' check fails               */
		HDMITX_CALLBACK_INT_R0 = 7,   /**< R0 interrupt                     */
		HDMITX_CALLBACK_INT_SW_INT = 8,
					      /**< SW DEBUG interrupt               */
		HDMITX_CALLBACK_INT_RX_SENSE = 9,
					      /**< RX SENSE interrupt               */
		HDMITX_CALLBACK_INT_EDID_BLK_READ = 10,
					      /**< EDID BLK READ interrupt          */
		HDMITX_CALLBACK_INT_PLL_LOCK = 11,
					      /**< Pll Lock (Serial or Formatter)   */
		HDMITX_CALLBACK_INT_VS_RPT = 12,
					      /**< VS Interrupt for Gamut packets   */
		HDMITX_CALLBACK_INT_NUM = 13  /**< Number of callbacks              */
	} tmbslHdmiTxCallbackInt_t;

/** Pixel rate */
	typedef enum {
		HDMITX_PIXRATE_DOUBLE = 0,	    /**< Double pixel rate */
		HDMITX_PIXRATE_SINGLE = 1,	    /**< Single pixel rate */
		HDMITX_PIXRATE_SINGLE_REPEATED = 2, /**< Single pixel repeated */
		HDMITX_PIXRATE_NO_CHANGE = 3,	    /**< No Change */
		HDMITX_PIXRATE_INVALID = 4	    /**< Invalid   */
	} tmbslHdmiTxPixRate_t;

/**
 * \brief The tmbslTDA9984Init() parameter structure
 */
	typedef struct _tmbslHdmiTxCallbackList_t {
    /** Interrupt callback function pointers (each ptr if null = not used) */
		ptmbslHdmiTxCallback_t funcCallback[HDMITX_CALLBACK_INT_NUM];

	} tmbslHdmiTxCallbackList_t;

/*============================================================================*/
/**
 * tmbslTDA9984MatrixSetCoeffs() parameter type
 */
/** Parameter structure array size */
	enum _tmbslHdmiTxMatCoeff {
		HDMITX_MAT_COEFF_NUM = 9
	};


/** \brief The tmbslTDA9984MatrixSetCoeffs() parameter structure */
/** Array of coefficients (values -1024 to +1023) */
	typedef struct _tmbslHdmiTxMatCoeff_t {
    /** Array of coefficients (values -1024 to +1023) */
		Int16 Coeff[HDMITX_MAT_COEFF_NUM];
	} tmbslHdmiTxMatCoeff_t;

/*============================================================================*/
/**
 * tmbslTDA9984MatrixSetConversion() parameter type
 */
/** Video input mode */
	typedef enum {
		HDMITX_VINMODE_CCIR656 = 0,
				       /**< ccir656  */
		HDMITX_VINMODE_RGB444 = 1,
				       /**< RGB444    */
		HDMITX_VINMODE_YUV444 = 2,
				       /**< YUV444    */
		HDMITX_VINMODE_YUV422 = 3,
				       /**< YUV422    */
		HDMITX_VINMODE_NO_CHANGE = 4,
				       /**< No change */
		HDMITX_VINMODE_INVALID = 5
				       /**< Invalid   */
	} tmbslHdmiTxVinMode_t;

/** Video output mode */
	typedef enum {
		HDMITX_VOUTMODE_RGB444 = 0,
					/**< RGB444    */
		HDMITX_VOUTMODE_YUV422 = 1,
					/**< YUV422    */
		HDMITX_VOUTMODE_YUV444 = 2,
					/**< YUV444    */
		HDMITX_VOUTMODE_NO_CHANGE = 3,
					/**< No change */
		HDMITX_VOUTMODE_INVALID = 4
					/**< Invalid   */
	} tmbslHdmiTxVoutMode_t;

/*============================================================================*/
/**
 * tmbslTDA9984MatrixSetMode() parameter types
 */
/** Matrix  control values */
	typedef enum {
		HDMITX_MCNTRL_ON = 0,
				   /**< Matrix on  */
		HDMITX_MCNTRL_OFF = 1,
				   /**< Matrix off */
		HDMITX_MCNTRL_NO_CHANGE = 2,
				   /**< Matrix unchanged */
		HDMITX_MCNTRL_MAX = 2,
				   /**< Max value  */
		HDMITX_MCNTRL_INVALID = 3
				   /**< Invalid    */
	} tmbslHdmiTxmCntrl_t;

/** Matrix  scale values */
	typedef enum {
		HDMITX_MSCALE_256 = 0,
				   /**< Factor 1/256  */
		HDMITX_MSCALE_512 = 1,
				   /**< Factor 1/512  */
		HDMITX_MSCALE_1024 = 2,
				   /**< Factor 1/1024 */
		HDMITX_MSCALE_NO_CHANGE = 3,
				   /**< Factor unchanged */
		HDMITX_MSCALE_MAX = 3,
				   /**< Max value     */
		HDMITX_MSCALE_INVALID = 4
				   /**< Invalid value */
	} tmbslHdmiTxmScale_t;

/*============================================================================*/
/**
 * Data Island Packet structure
 */
/** Parameter structure array sizes */
	enum _tmbslHdmiTxPkt {
		HDMITX_PKT_DATA_BYTE_CNT = 28
	};

/** \brief The parameter structure for tmbslTDA9984Pkt*() APIs */
	typedef struct _tmbslHdmiTxPkt_t {
		UInt8 dataByte[HDMITX_PKT_DATA_BYTE_CNT];
						    /**< Packet Data   */
	} tmbslHdmiTxPkt_t;

/*============================================================================*/
/**
 * \brief The Audio Infoframe Parameter structure
 */
	typedef struct _tmbslHdmiTxPktAif_t {
		UInt8 CodingType;
			    /**< Coding Type 0 to 0Fh */
		UInt8 ChannelCount;
			    /**< Channel Count 0 to 07h */
		UInt8 SampleFreq;
			    /**< Sample Frequency 0 to 07h */
		UInt8 SampleSize;
			    /**< Sample Size 0 to 03h */
		UInt8 ChannelAlloc;
			    /**< Channel Allocation 0 to FFh */
		Bool DownMixInhibit;
			    /**< Downmix inhibit flag 0/1 */
		UInt8 LevelShift;
			    /**< Level Shift 0 to 0Fh */
	} tmbslHdmiTxPktAif_t;

/*============================================================================*/
/**
 * tmbslTDA9984PktSetMpegInfoframe() parameter types
 */
/** MPEG frame types */
	typedef enum {
		HDMITX_MPEG_FRAME_UNKNOWN = 0,
					/**< Unknown  */
		HDMITX_MPEG_FRAME_I = 1,/**< i-frame   */
		HDMITX_MPEG_FRAME_B = 2,/**< b-frame */
		HDMITX_MPEG_FRAME_P = 3,/**< p-frame */
		HDMITX_MPEG_FRAME_INVALID = 4
					/**< Invalid   */
	} tmbslHdmiTxMpegFrame_t;

/** \brief The MPEG Infoframe Parameter structure */
	typedef struct _tmbslHdmiTxPktMpeg_t {
		UInt32 bitRate;		    /**< MPEG bit rate in Hz */
		tmbslHdmiTxMpegFrame_t frameType;
					    /**< MPEG frame type */
		Bool bFieldRepeat;	    /**< 0: new field, 1:repeated field */
	} tmbslHdmiTxPktMpeg_t;

/*============================================================================*/
/**
  * Source Product Description Infoframe Parameter types
  */
/** SDI frame types */
	typedef enum {
		HDMITX_SPD_INFO_UNKNOWN = 0,
		HDMITX_SPD_INFO_DIGITAL_STB = 1,
		HDMITX_SPD_INFO_DVD = 2,
		HDMITX_SPD_INFO_DVHS = 3,
		HDMITX_SPD_INFO_HDD_VIDEO = 4,
		HDMITX_SPD_INFO_DVC = 5,
		HDMITX_SPD_INFO_DSC = 6,
		HDMITX_SPD_INFO_VIDEO_CD = 7,
		HDMITX_SPD_INFO_GAME = 8,
		HDMITX_SPD_INFO_PC = 9,
		HDMITX_SPD_INFO_INVALID = 10
	} tmbslHdmiTxSourceDev_t;

#define HDMI_TX_SPD_VENDOR_SIZE 8
#define HDMI_TX_SPD_DESCR_SIZE  16
#define HDMI_TX_SPD_LENGTH      25

/** \brief The Source Product Description Infoframe Parameter structure */
	typedef struct _tmbslHdmiTxPktSpd_t {
		UInt8 VendorName[HDMI_TX_SPD_VENDOR_SIZE];	 /**< Vendor name */
		UInt8 ProdDescr[HDMI_TX_SPD_DESCR_SIZE];       /**< Product Description */
		tmbslHdmiTxSourceDev_t SourceDevInfo;	       /**< Source Device Info */
	} tmbslHdmiTxPktSpd_t;

/*============================================================================*/
/**
  * \brief The Video Infoframe Parameter structure
  */
	typedef struct _tmbslHdmiTxPktVif_t {
		UInt8 Colour;	 /**< 0 to 03h */
		Bool ActiveInfo; /**< 0/1 */
		UInt8 BarInfo;	 /**< 0 to 03h */
		UInt8 ScanInfo;	 /**< 0 to 03h */
		UInt8 Colorimetry;
				 /**< 0 to 03h */
		UInt8 PictureAspectRatio;
				 /**< 0 to 03h */
		UInt8 ActiveFormatRatio;
				 /**< 0 to 0Fh */
		UInt8 Scaling;	 /**< 0 to 03h */
		UInt8 VidFormat; /**< 0 to 7Fh */
		UInt8 PixelRepeat;
				 /**< 0 to 0Fh */
		UInt16 EndTopBarLine;
		UInt16 StartBottomBarLine;
		UInt16 EndLeftBarPixel;
		UInt16 StartRightBarPixel;
	} tmbslHdmiTxPktVif_t;

/*============================================================================*/
/**
 * tmbslTDA9984ScalerGetMode() parameter types
 */
/** Scaler modes */
	typedef enum {
		HDMITX_SCAMODE_OFF = 0,
				     /**< Off  */
		HDMITX_SCAMODE_ON = 1,
				     /**< On   */
		HDMITX_SCAMODE_AUTO = 2,
				     /**< Auto */
		HDMITX_SCAMODE_NO_CHANGE = 3,
				     /**< No change */
		HDMITX_SCAMODE_INVALID = 4
				     /**< Invalid   */
	} tmbslHdmiTxScaMode_t;

/*============================================================================*/
/**
 * \brief The tmbslTDA9984ScalerGet() parameter type
 */
	typedef struct _tmbslHdmiTxScalerDiag_t {
		UInt16 maxBuffill_p;
			     /**< Filling primary video buffer           */
		UInt16 maxBuffill_d;
			     /**< Filling video deinterlaced buffer      */
		UInt8 maxFifofill_pi;
			     /**< Filling primary video input FIFO       */
		UInt8 minFifofill_po1;
			     /**< Filling primary video output FIFO #1   */
		UInt8 minFifofill_po2;
			     /**< Filling primary video output FIFO #2   */
		UInt8 minFifofill_po3;
			     /**< Filling primary video output FIFO #3   */
		UInt8 minFifofill_po4;
			     /**< Filling primary video output FIFO #4   */
		UInt8 maxFifofill_di;
			     /**< Filling deinterlaced video input FIFO  */
		UInt8 maxFifofill_do;
			     /**< Filling deinterlaced video output FIFO */
	} tmbslHdmiTxScalerDiag_t;

/*============================================================================*/
/**
 * tmbslTDA9984ScalerSetCoeffs() parameter types
 */
/** Scaler lookup table selection */
	typedef enum {
		HDMITX_SCALUT_DEFAULT_TAB1 = 0,
					   /**< Use default table 1 */
		HDMITX_SCALUT_DEFAULT_TAB2 = 1,
					   /**< Use default table 2 */
		HDMITX_SCALUT_USE_VSLUT = 2,
					   /**< Use vsLut parameter */
		HDMITX_SCALUT_INVALID = 3  /**< Invalid value       */
	} tmbslHdmiTxScaLut_t;

/** Scaler control parameter structure array size */
	enum _tmbslHdmiTxvsLut {
		HDMITX_VSLUT_COEFF_NUM = 45
	};
/*============================================================================*/
/**
 * tmbslTDA9984ScalerSetFieldOrder() parameter types
 */
/** IntExt values */
	typedef enum {
		HDMITX_INTEXT_INTERNAL = 0,/**< Internal  */
		HDMITX_INTEXT_EXTERNAL = 1,/**< External  */
		HDMITX_INTEXT_NO_CHANGE = 2,
					   /**< No change */
		HDMITX_INTEXT_INVALID = 3  /**< Invalid   */
	} tmbslHdmiTxIntExt_t;

/** TopSel values */
	typedef enum {
		HDMITX_TOPSEL_INTERNAL = 0,/**< Internal  */
		HDMITX_TOPSEL_VRF = 1,	   /**< VRF       */
		HDMITX_TOPSEL_NO_CHANGE = 2,
					   /**< No change */
		HDMITX_TOPSEL_INVALID = 3  /**< Invalid   */
	} tmbslHdmiTxTopSel_t;

/** TopTgl values */
	typedef enum {
		HDMITX_TOPTGL_NO_ACTION = 0,
					   /**< NO action */
		HDMITX_TOPTGL_TOGGLE = 1,  /**< Toggle    */
		HDMITX_TOPTGL_NO_CHANGE = 2,
					   /**< No change */
		HDMITX_TOPTGL_INVALID = 3  /**< Invalid   */
	} tmbslHdmiTxTopTgl_t;

/*============================================================================*/
/**
 * tmbslTDA9984ScalerSetPhase() parameter types
 */
/** Phases_h values */
	typedef enum {
		HDMITX_H_PHASES_16 = 0,
				      /**< 15 horizontal phases */
		HDMITX_H_PHASES_15 = 1,
				      /**< 16 horizontal phases */
		HDMITX_H_PHASES_INVALID = 2
				      /**< Invalid   */
	} tmbslHdmiTxHPhases_t;

/*============================================================================*/
/**
 * tmbslTDA9984ScalerSetFine() parameter types
 */
/** Reference pixel values */
	enum _tmbslHdmiTxScalerFinePixelLimits {
		HDMITX_SCALER_FINE_PIXEL_MIN = 0x0000,
		HDMITX_SCALER_FINE_PIXEL_MAX = 0x1FFF,
		HDMITX_SCALER_FINE_PIXEL_NO_CHANGE = 0x2000,
		HDMITX_SCALER_FINE_PIXEL_INVALID = 0x2001
	};

/** Reference line values */
	enum _tmbslHdmiTxScalerFineLineLimits {
		HDMITX_SCALER_FINE_LINE_MIN = 0x0000,
		HDMITX_SCALER_FINE_LINE_MAX = 0x07FF,
		HDMITX_SCALER_FINE_LINE_NO_CHANGE = 0x0800,
		HDMITX_SCALER_FINE_LINE_INVALID = 0x0801
	};
/*============================================================================*/
/**
 * tmbslTDA9984ScalerSetSync() parameter types
 */
/** Video sync method */
	typedef enum {
		HDMITX_VSMETH_V_H = 0, /**< V and H    */
		HDMITX_VSMETH_V_XDE = 1,
				       /**< V and X-DE */
		HDMITX_VSMETH_NO_CHANGE = 2,
				       /**< No change  */
		HDMITX_VSMETH_INVALID = 3
				       /**< Invalid    */
	} tmbslHdmiTxVsMeth_t;

/** Line/pixel counters sync */
	typedef enum {
		HDMITX_VSONCE_EACH_FRAME = 0,
				       /**< Sync on each frame */
		HDMITX_VSONCE_ONCE = 1,/**< Sync once only     */
		HDMITX_VSONCE_NO_CHANGE = 2,
				       /**< No change  */
		HDMITX_VSONCE_INVALID = 3
				       /**< Invalid    */
	} tmbslHdmiTxVsOnce_t;

/*============================================================================*/
/**
 * tmbslTDA9984TmdsSetOutputs() parameter types
 */
/** TMDS output mode */
	typedef enum {
		HDMITX_TMDSOUT_NORMAL = 0,
				       /**< Normal outputs   */
		HDMITX_TMDSOUT_NORMAL1 = 1,
				       /**< Normal outputs, same as 0  */
		HDMITX_TMDSOUT_FORCED0 = 2,
				       /**< Forced 0 outputs */
		HDMITX_TMDSOUT_FORCED1 = 3,
				       /**< Forced 1 outputs */
		HDMITX_TMDSOUT_INVALID = 4
				       /**< Invalid          */
	} tmbslHdmiTxTmdsOut_t;

/*============================================================================*/
/**
 * tmbslTDA9984TmdsSetSerializer() parameter types
 */
/** Serializer phase limits */
	enum _tmbslHdmiTxTmdsPhase {
		HDMITX_TMDSPHASE_MIN = 0,
		HDMITX_TMDSPHASE_MAX = 15,
		HDMITX_TMDSPHASE_INVALID = 16
	};

/*============================================================================*/
/**
 * tmbslTDA9984TestSetPattern() parameter types
 */
/** Test pattern types */
	typedef enum {
		HDMITX_PATTERN_OFF = 0,
				/**< Insert test pattern     */
		HDMITX_PATTERN_CBAR4 = 1,
				/**< Insert 4-bar colour bar */
		HDMITX_PATTERN_CBAR8 = 2,
				/**< Insert 8-bar colour bar */
		HDMITX_PATTERN_BLUE = 3,/**< Insert Blue screen		 */
		HDMITX_PATTERN_BLACK = 4,
				    /**< Insert Blue screen		 */
		HDMITX_PATTERN_INVALID = 5
				/**< Invalid pattern		 */
	} tmbslHdmiTxTestPattern_t;

/*============================================================================*/
/**
 * tmbslTDA9984TestSetMode() parameter types
 */
/** Test modes */
	typedef enum {
		HDMITX_TESTMODE_PAT = 0,
				/**< Insert test pattern                    */
		HDMITX_TESTMODE_656 = 1,
				/**< Inject CCIR-656 video via audio port   */
		HDMITX_TESTMODE_SERPHOE = 2,
				/**< Activate srl_tst_ph2_o & srl_tst_ph3_o */
		HDMITX_TESTMODE_NOSC = 3,
				/**< Input nosc predivider = PLL-ref input  */
		HDMITX_TESTMODE_HVP = 4,
				/**< Test high voltage protection cells     */
		HDMITX_TESTMODE_PWD = 5,
				/**< Test PLLs in sleep mode                */
		HDMITX_TESTMODE_DIVOE = 6,
				/**< Enable scaler PLL divider test output  */
		HDMITX_TESTMODE_INVALID = 7
				/**< Invalid test */
	} tmbslHdmiTxTestMode_t;

/** Test states */
	typedef enum {
		HDMITX_TESTSTATE_OFF = 0,
				   /**< Disable the selected test */
		HDMITX_TESTSTATE_ON = 1,
				   /**< Enable the selected test  */
		HDMITX_TESTSTATE_INVALID = 2
				   /**< Invalid value */
	} tmbslHdmiTxTestState_t;

/*============================================================================*/
/**
 * tmbslTDA9984VideoInSetBlanking() parameter types
 */
/** Blankit Source */
	typedef enum {
		HDMITX_BLNKSRC_NOT_DE = 0, /**< Source=Not DE        */
		HDMITX_BLNKSRC_VS_HS = 1,  /**< Source=VS And HS     */
		HDMITX_BLNKSRC_VS_NOT_HS = 2,
					   /**< Source=VS And Not HS */
		HDMITX_BLNKSRC_VS_HEMB_VEMB = 3,
					   /**< Source=Hemb And Vemb */
		HDMITX_BLNKSRC_NO_CHANGE = 4,
					   /**< No change */
		HDMITX_BLNKSRC_INVALID = 5 /**< Invalid   */
	} tmbslHdmiTxBlnkSrc_t;

/** Blanking Codes */
	typedef enum {
		HDMITX_BLNKCODE_ALL_0 = 0, /**< Code=All Zero */
		HDMITX_BLNKCODE_RGB444 = 1,/**< Code=RGB444   */
		HDMITX_BLNKCODE_YUV444 = 2,/**< Code=YUV444   */
		HDMITX_BLNKCODE_YUV422 = 3,/**< Code=YUV422   */
		HDMITX_BLNKCODE_NO_CHANGE = 4,
					   /**< No change */
		HDMITX_BLNKCODE_INVALID = 5/**< Invalid   */
	} tmbslHdmiTxBlnkCode_t;

/*============================================================================*/
/**
 * tmbslTDA9984VideoInSetConfig() parameter types
 */
/** Sample edge */
	typedef enum {
		HDMITX_PIXEDGE_CLK_POS = 0, /**< Pixel Clock Positive Edge */
		HDMITX_PIXEDGE_CLK_NEG = 1, /**< Pixel Clock Negative Edge */
		HDMITX_PIXEDGE_NO_CHANGE = 2,
					    /**< No Change */
		HDMITX_PIXEDGE_INVALID = 3  /**< Invalid   */
	} tmbslHdmiTxPixEdge_t;

/** Upsample modes */
	typedef enum {
		HDMITX_UPSAMPLE_BYPASS = 0, /**< Bypass */
		HDMITX_UPSAMPLE_COPY = 1,   /**< Copy */
		HDMITX_UPSAMPLE_INTERPOLATE = 2,
					    /**< Interpolate */
		HDMITX_UPSAMPLE_AUTO = 3,   /**< Auto: driver chooses best value */
		HDMITX_UPSAMPLE_NO_CHANGE = 4,
					    /**< No Change */
		HDMITX_UPSAMPLE_INVALID = 5 /**< Invalid   */
	} tmbslHdmiTxUpsampleMode_t;

/*============================================================================*/
/**
 * tmbslTDA9984VideoInSetFine() parameter types
 */
/** Subpacket count */
	typedef enum {
		HDMITX_PIXSUBPKT_FIX_0 = 0, /**< Fix At 0 */
		HDMITX_PIXSUBPKT_FIX_1 = 1, /**< Fix At 1 */
		HDMITX_PIXSUBPKT_FIX_2 = 2, /**< Fix At 2 */
		HDMITX_PIXSUBPKT_FIX_3 = 3, /**< Fix At 3 */
		HDMITX_PIXSUBPKT_SYNC_FIRST = 4,
					    /**< First Sync value */
		HDMITX_PIXSUBPKT_SYNC_HEMB = 4,
					    /**< Sync By Hemb */
		HDMITX_PIXSUBPKT_SYNC_DE = 5,
					    /**< Sync By Rising Edge DE */
		HDMITX_PIXSUBPKT_SYNC_HS = 6,
					    /**< Sync By Rising Edge HS */
		HDMITX_PIXSUBPKT_NO_CHANGE = 7,
					    /**< No Change */
		HDMITX_PIXSUBPKT_INVALID = 8,
					    /**< Invalid   */
		HDMITX_PIXSUBPKT_SYNC_FIXED = 3
					    /**< Not used as a parameter value,
					     *  but used internally when
					     *  Fix at 0/1/2/3 values are set */
	} tmbslHdmiTxPixSubpkt_t;

/** Toggling */
	typedef enum {
		HDMITX_PIXTOGL_NO_ACTION = 0,
					    /**< No Action  */
		HDMITX_PIXTOGL_ENABLE = 1,  /**< Toggle     */
		HDMITX_PIXTOGL_NO_CHANGE = 2,
					    /**< No Change  */
		HDMITX_PIXTOGL_INVALID = 3  /**< Invalid    */
	} tmbslHdmiTxPixTogl_t;

/*============================================================================*/
/**
 * tmbslTDA9984VideoInSetMapping() parameter types
 */
/** Video input port parameter structure array size and limits */
	enum _tmbslHdmiTxVinPortMap {
		HDMITX_VIN_PORT_MAP_TABLE_LEN = 6,

		HDMITX_VIN_PORT_SWAP_NO_CHANGE = 6,
		HDMITX_VIN_PORT_SWAP_INVALID = 7,

		HDMITX_VIN_PORT_MIRROR_NO_CHANGE = 2,
		HDMITX_VIN_PORT_MIRROR_INVALID = 3
	};


/*============================================================================*/
/**
 * tmbslTDA9984VideoInSetSyncAuto() parameter types
 */
/** Sync source - was Embedded sync HDMITX_PIXEMBSYNC_ */
	typedef enum {
		HDMITX_SYNCSRC_EMBEDDED = 0,
					 /**< Embedded sync */
		HDMITX_SYNCSRC_EXT_VREF = 1,
					 /**< External sync Vref, Href, Fref */
		HDMITX_SYNCSRC_EXT_VS = 2,
					 /**< External sync Vs, Hs */
		HDMITX_SYNCSRC_NO_CHANGE = 3,
					 /**< No Change     */
		HDMITX_SYNCSRC_INVALID = 4
					 /**< Invalid       */
	} tmbslHdmiTxSyncSource_t;

/*============================================================================*/
/**
 * tmbslTDA9984VideoInSetSyncManual() parameter types
 */
/** Video output frame pixel values */
	enum _tmbslHdmiTxVoutFinePixelLimits {
		HDMITX_VOUT_FINE_PIXEL_MIN = 0x0000,
		HDMITX_VOUT_FINE_PIXEL_MAX = 0x1FFF,
		HDMITX_VOUT_FINE_PIXEL_NO_CHANGE = 0x2000,
		HDMITX_VOUT_FINE_PIXEL_INVALID = 0x2001
	};

/** Video output frame line values */
	enum _tmbslHdmiTxVoutFineLineLimits {
		HDMITX_VOUT_FINE_LINE_MIN = 0x0000,
		HDMITX_VOUT_FINE_LINE_MAX = 0x07FF,
		HDMITX_VOUT_FINE_LINE_NO_CHANGE = 0x0800,
		HDMITX_VOUT_FINE_LINE_INVALID = 0x0801
	};

/*============================================================================*/
/**
 * tmbslTDA9984VideoOutSetConfig() parameter types
 */
/** Prefilter */
	typedef enum {
		HDMITX_VOUT_PREFIL_OFF = 0,/**< Off */
		HDMITX_VOUT_PREFIL_121 = 1,/**< 121 */
		HDMITX_VOUT_PREFIL_109 = 2,/**< 109 */
		HDMITX_VOUT_PREFIL_CCIR601 = 3,
					   /**< CCIR601   */
		HDMITX_VOUT_PREFIL_NO_CHANGE = 4,
					   /**< No Change */
		HDMITX_VOUT_PREFIL_INVALID = 5
					   /**< Invalid   */
	} tmbslHdmiTxVoutPrefil_t;

/** YUV blanking */
	typedef enum {
		HDMITX_VOUT_YUV_BLNK_16 = 0,
					   /**< 16 */
		HDMITX_VOUT_YUV_BLNK_0 = 1,/**< 0  */
		HDMITX_VOUT_YUV_BLNK_NO_CHANGE = 2,
					   /**< No Change */
		HDMITX_VOUT_YUV_BLNK_INVALID = 3
					   /**< Invalid   */
	} tmbslHdmiTxVoutYuvBlnk_t;

/** Video quantization range */
	typedef enum {
		HDMITX_VOUT_QRANGE_FS = 0, /**< Full Scale */
		HDMITX_VOUT_QRANGE_RGB_YUV = 1,
					   /**< RGB Or YUV */
		HDMITX_VOUT_QRANGE_YUV = 2,/**< YUV        */
		HDMITX_VOUT_QRANGE_NO_CHANGE = 3,
					   /**< No Change  */
		HDMITX_VOUT_QRANGE_INVALID = 4
					   /**< Invalid    */
	} tmbslHdmiTxVoutQrange_t;

/*============================================================================*/
/**
 * tmbslTDA9984VideoOutSetSync() parameter types
 */
/** Video sync source */
	typedef enum {
		HDMITX_VSSRC_INTERNAL = 0,
				       /**< Internal  */
		HDMITX_VSSRC_EXTERNAL = 1,
				       /**< External  */
		HDMITX_VSSRC_NO_CHANGE = 2,
				       /**< No change */
		HDMITX_VSSRC_INVALID = 3
				       /**< Invalid   */
	} tmbslHdmiTxVsSrc_t;

/** Video sync toggle */
	typedef enum {
		HDMITX_VSTGL_TABLE = 0,/**< Vs/Hs polarity from table */
		HDMITX_VSTGL_UNUSED_1 = 1,
				       /**< Unused          */
		HDMITX_VSTGL_UNUSED_2 = 2,
				       /**< Unused          */
		HDMITX_VSTGL_UNUSED_3 = 3,
				       /**< Unused          */
		HDMITX_VSTGL_NO_ACTION = 4,
				       /**< No toggle       */
		HDMITX_VSTGL_HS = 5,   /**< Toggle Hs       */
		HDMITX_VSTGL_VS = 6,   /**< Toggle Vs       */
		HDMITX_VSTGL_HS_VS = 7,/**< Toggle Hs & Vs  */
		HDMITX_VSTGL_NO_CHANGE = 8,
				       /**< No change       */
		HDMITX_VSTGL_INVALID = 9
				       /**< Invalid         */
	} tmbslHdmiTxVsTgl_t;

/*============================================================================*/
/**
 * tmbslTDA9984VideoSetInOut() parameter types
 */
/** Pixel repetition values */
	enum _tmbslHdmiTxPixRepeat {
		HDMITX_PIXREP_NONE = 0,
				     /**< No repetition  */
		HDMITX_PIXREP_MIN = 0,
				     /**< 1 repetition   */

		HDMITX_PIXREP_0 = 0,
		HDMITX_PIXREP_1 = 1,
		HDMITX_PIXREP_2 = 2,
		HDMITX_PIXREP_3 = 3,
		HDMITX_PIXREP_4 = 4,
		HDMITX_PIXREP_5 = 5,
		HDMITX_PIXREP_6 = 6,
		HDMITX_PIXREP_7 = 7,
		HDMITX_PIXREP_8 = 8,
		HDMITX_PIXREP_9 = 9,

		HDMITX_PIXREP_MAX = 9,
				     /**< 10 repetitions */
		HDMITX_PIXREP_DEFAULT = 10,
				     /**< Default repetitions for output format */
		HDMITX_PIXREP_NO_CHANGE = 11,
				     /**< No change */
		HDMITX_PIXREP_INVALID = 12
				     /**< Invalid   */
	};

/** Matrix modes */
	typedef enum {
		HDMITX_MATMODE_OFF = 0,
				     /**< Off  */
		HDMITX_MATMODE_AUTO = 1,
				     /**< Auto */
		HDMITX_MATMODE_NO_CHANGE = 2,
				     /**< No change */
		HDMITX_MATMODE_INVALID = 3
				     /**< Invalid   */
	} tmbslHdmiTxMatMode_t;

/** Datapath bitwidth */
	typedef enum {
		HDMITX_VOUT_DBITS_12 = 0,  /**< 12 bits */
		HDMITX_VOUT_DBITS_8 = 1,   /**< 8 bits  */
		HDMITX_VOUT_DBITS_10 = 2,  /**< 10 bits */
		HDMITX_VOUT_DBITS_NO_CHANGE = 3,
					   /**< No change */
		HDMITX_VOUT_DBITS_INVALID = 4
					   /**< Invalid   */
	} tmbslHdmiTxVoutDbits_t;

/** Color depth */
	typedef enum {
		HDMITX_COLORDEPTH_24 = 0,  /**< 24 bits per pixel */
		HDMITX_COLORDEPTH_30 = 1,  /**< 30 bits per pixel */
		HDMITX_COLORDEPTH_36 = 2,  /**< 36 bits per pixel */
		HDMITX_COLORDEPTH_48 = 3,  /**< 48 bits per pixel */
		HDMITX_COLORDEPTH_NO_CHANGE = 4,
					   /**< No change */
		HDMITX_COLORDEPTH_INVALID = 5
					   /**< Invalid   */
	} tmbslHdmiTxColorDepth;

/** the supported transmission formats of 3D video data */
	typedef enum {
		HDMITX_3D_NONE = 0,	   /**< 3D video data not present */
		HDMITX_3D_FRAME_PACKING = 1,
					   /**< 3D video data Frame Packing structure */
		HDMITX_3D_TOP_AND_BOTTOM = 2,
					   /**< 3D video data Top and Bottom structure */
		HDMITX_3D_SIDE_BY_SIDE_HALF = 3,
					   /**< 3D video data Side by Side Half structure */
		HDMITX_3D_INVALID = 4	   /**< Invalid */
	} tmbslHdmiTx3DStructure_t;

/*============================================================================*/
/**
 * tmbslTDA9984MatrixSetInputOffset() parameter type
 */
/** Parameter structure array size */
	enum _tmbslHdmiTxMatOffset {
		HDMITX_MAT_OFFSET_NUM = 3
	};

/** \brief The tmbslTDA9984MatrixSetInputOffset() parameter structure */
	typedef struct _tmbslHdmiTxMatOffset_t {
    /** Offset array  (values -1024 to +1023) */
		Int16 Offset[HDMITX_MAT_OFFSET_NUM];
	} tmbslHdmiTxMatOffset_t;

/** Matrix numeric limits */
	enum _tmbslHdmiTxMatLimits {
		HDMITX_MAT_OFFSET_MIN = -1024,
		HDMITX_MAT_OFFSET_MAX = 1023
	};

/*============================================================================*/
/**
 * tmbslTDA9989PowerSetState() and tmbslTDA9989PowerGetState() parameter types
 */
	typedef enum {
		HDMITX_POWER_STATE_STAND_BY = 0,   /**< Stand by mode        */
		HDMITX_POWER_STATE_SLEEP_MODE = 1, /**< Sleep mode           */
		HDMITX_POWER_STATE_ON = 2,	   /**< On mode      */
		HDMITX_POWER_STATE_INVALID = 3	   /**< Invalid format       */
	} tmbslHdmiTxPowerState_t;

/**
 * \brief Structure describing gamut metadata packet (P0 or P1 profiles)
 */
	typedef struct {
		UInt8 HB[3];
		   /**< Header bytes (HB0, HB1 & HB2) */
		UInt8 PB[28];
		   /**< Payload bytes 0..27 */
	} tmbslHdmiTxPktGamut_t;


/**
 * \brief Structure describing RAW AVI infoframe
 */
	typedef struct {
		UInt8 HB[3];
		   /**< Header bytes (HB0, HB1 & HB2) */
		UInt8 PB[28];
		   /**< Payload bytes 0..27 */
	} tmbslHdmiTxPktRawAvi_t;


/** Sink category */
	typedef enum {
		HDMITX_SINK_CAT_NOT_REPEATER = 0,
					 /**< Not repeater  */
		HDMITX_SINK_CAT_REPEATER = 1,
					 /**< repeater      */
		HDMITX_SINK_CAT_INVALID = 3
					 /**< Invalid       */
	} tmbslHdmiTxSinkCategory_t;


	typedef struct {
		Bool latency_available;
		Bool Ilatency_available;
		UInt8 Edidvideo_latency;
		UInt8 Edidaudio_latency;
		UInt8 EdidIvideo_latency;
		UInt8 EdidIaudio_latency;

	} tmbslHdmiTxEdidLatency_t;

/**
 * \brief Structure defining additional VSDB data
 */
	typedef struct {
		UInt8 maxTmdsClock;	/* maximum supported TMDS clock */
		UInt8 cnc0;	/* content type Graphics (text) */
		UInt8 cnc1;	/* content type Photo */
		UInt8 cnc2;	/* content type Cinema */
		UInt8 cnc3;	/* content type Game */
		UInt8 hdmiVideoPresent;	/* additional video format */
		UInt8 h3DPresent;	/* 3D support by the HDMI Sink */
		UInt8 h3DMultiPresent;	/* 3D multi strctures present */
		UInt8 imageSize;	/* additional info for the values in the image size area */
		UInt8 hdmi3DLen;	/* total length of 3D video formats */
		UInt8 hdmiVicLen;	/* total length of extended video formats */
		UInt8 ext3DData[21];	/* max_len-10, ie: 31-10=21 */
	} tmbslHdmiTxEdidExtraVsdbData_t;

/**
 * \brief Enum defining possible quantization range
 */
	typedef enum {
		HDMITX_VQR_DEFAULT = 0,	/* Follow HDMI spec. */
		HDMITX_RGB_FULL = 1,	/* Force RGB FULL , DVI only */
		HDMITX_RGB_LIMITED = 2	/* Force RGB LIMITED , DVI only */
	} tmbslHdmiTxVQR_t;




#ifdef __cplusplus
}
#endif
#endif				/* TMBSLHDMITX_TYPES_H */
/*============================================================================*//*                            END OF FILE                                     *//*============================================================================*/
