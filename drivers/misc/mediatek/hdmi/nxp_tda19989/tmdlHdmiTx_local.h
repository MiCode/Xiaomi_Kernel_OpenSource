/**
 * Copyright (C) 2006 NXP N.V., All Rights Reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of NXP N.V. and is confidential in nature. Under no circumstances
 * is this software to be  exposed to or placed under an Open Source License of
 * any type without the expressed written permission of NXP N.V.
 *
 * \file          tmdlHdmiTx_local.h
 *
 * \version       $Revision: 1 $
 *
 * \date          $Date: 02/08/07 08:32 $
 *
 * \brief         devlib driver component API for the TDA998x HDMI Transmitters
 *
 * \section refs  Reference Documents
 * HDMI Tx Driver - FRS.doc,
 *
 * \section info  Change Information
 *
 * \verbatim

   $History: tmdlHdmiTx_local.h $
 *
  * *****************  Version 13  *****************
 * User: J. Lamotte Date: 02/08/07   Time: 08:32
 * Updated in $/Source/tmdlHdmiTx/inc
 * initial version
 *

   \endverbatim
 *
*/

#ifndef TMDLHDMITX_LOCAL_H
#define TMDLHDMITX_LOCAL_H

#ifdef TMFL_LINUX_OS_KERNEL_DRIVER
#include <linux/kernel.h>
#endif

#include "tmdlHdmiTx_IW.h"
#include "tmNxTypes.h"
#include "tmdlHdmiTx_Types.h"
#include "tmdlHdmiTx_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/*                            MACRO DEFINITIONS                               */
/*============================================================================*/

/* Version of the SW driver */
#define VERSION_COMPATIBILITY 0
#define VERSION_MAJOR         5
#define VERSION_MINOR         3

/* Invalid HDCP seed */
#define HDCP_SEED_NULL      0

/* A default seed value may be defined here, or set to HDCP_SEED_NULL.
 * If HDCP_SEED_NULL, a table of seeds may instead be programmed separately
 * into flash at the location of kSeedTable, below */
#define HDCP_SEED_DEFAULT   HDCP_SEED_NULL

/* Default SHA-1 test handling */
#define HDCP_OPT_DEFAULT    (TMDL_HDMITX_HDCP_OPTION_FORCE_PJ_IGNORED \
				| TMDL_HDMITX_HDCP_OPTION_FORCE_VSLOW_DDC \
				| TMDL_HDMITX_HDCP_OPTION_FORCE_NO_1_1)

/**
 * A macro to check a condition and if true return a result
 */
#ifdef TMFL_LINUX_OS_KERNEL_DRIVER
#define RETIF(cond, rslt)       if ((cond)) { \
      pr_debug("%s %d\n", __func__, __LINE__); \
      return (rslt); }
#else
#define RETIF(cond, rslt)       if ((cond)) {return (rslt); }
#endif

/**
 * A macro to check a condition and if true return
 * TMDL_ERR_DLHDMITX_BAD_PARAMETER.
 * To save code space, it can be compiled out by defining NO_RETIF_BADPARAM on
 * the compiler command line.
 */
#ifdef NO_RETIF_BADPARAM
#define RETIF_BADPARAM(cond)
#else
#ifdef TMFL_LINUX_OS_KERNEL_DRIVER
#define RETIF_BADPARAM(cond)  if ((cond)) { \
      pr_debug("%s %d\n", __func__, __LINE__);    \
      return TMDL_ERR_DLHDMITX_BAD_PARAMETER; }
#else
#define RETIF_BADPARAM(cond)  if ((cond)) {return TMDL_ERR_DLHDMITX_BAD_PARAMETER; }
#endif
#endif

/**
 * A macro to check a condition and if true, release the semaphore describe by handle and return a result
 */
#ifdef TMFL_LINUX_OS_KERNEL_DRIVER
#define RETIF_SEM(handle, cond, rslt)       if ((cond)) { \
      tmdlHdmiTxIWSemaphoreV(handle);                    \
      pr_debug("%s %d\n", __func__, __LINE__);     \
      return (rslt); }
#else
#define RETIF_SEM(handle, cond, rslt)       if ((cond)) {tmdlHdmiTxIWSemaphoreV(handle); return (rslt); }
#endif

	/* Resolution supported */
#ifndef FORMAT_PC
#define RESOLUTION_NB   41
#else
#define RESOLUTION_NB   68
#endif				/* FORMAT_PC */

/* Instance number */
#define INSTANCE_0  0
#define INSTANCE_1  1

#ifdef HDMI_TX_REPEATER_ISR_MODE
/* Number of event */
#define EVENT_NB    10
#else				/* HDMI_TX_REPEATER_ISR_MODE */
/* Number of event */
#define EVENT_NB    9
#endif				/* HDMI_TX_REPEATER_ISR_MODE */

/* Size of a KSV is five bytes */
#define KSV_SIZE  5

/* Arbitrary short TV format values */
#define TV_INVALID      0
#define TV_VGA_60Hz     1
#define TV_240p_60Hz    2
#define TV_480p_60Hz    3
#define TV_480i_60Hz    4
#define TV_720p_60Hz    5
#define TV_1080p_60Hz   6
#define TV_1080i_60Hz   7
#define TV_288p_50Hz    8
#define TV_576p_50Hz    9
#define TV_576i_50Hz    10
#define TV_720p_50Hz    11
#define TV_1080p_50Hz   12
#define TV_1080i_50Hz   13

/* Shorthands for vinMode values in tmbslTDA9984.h */
#define iINVALID TMDL_HDMITX_VINMODE_INVALID
#define iCCIR656 TMDL_HDMITX_VINMODE_CCIR656
#define iRGB444  TMDL_HDMITX_VINMODE_RGB444
#define iYUV444  TMDL_HDMITX_VINMODE_YUV444
#define iYUV422  TMDL_HDMITX_VINMODE_YUV422

/* Shorthands for input sync */
#define EMB      1
#define EXT      0

/* Shorthands for single/double pixel rate in tmbslTDA9984.h */
#define SINGLE   TMDL_HDMITX_PIXRATE_SINGLE
#define DOUBLE   TMDL_HDMITX_PIXRATE_DOUBLE

/* Shorthands for sampling frequency in tmdlHdmiTxSetAudioInput API */
#define AIF_SF_REFER_TO_STREAM_HEADER   0
#define AIF_SF_32K                      1
#define AIF_SF_44K                      2
#define AIF_SF_48K                      3
#define AIF_SF_88K                      4
#define AIF_SF_96K                      5
#define AIF_SF_176K                     6
#define AIF_SF_192K                     7

/* HDCP check interval in milliseconds */
#define HDCP_CHECK_INTERVAL_MS  2500

/* Number of HDCP checks to carry out after HDCP is started */
#define HDCP_NUM_CHECKS         5

#define TMDL_HDMITX_CHANNELALLOC_LUT_SIZE 32


	static UInt8 kChanAllocChanNum[TMDL_HDMITX_CHANNELALLOC_LUT_SIZE] __maybe_unused =
	    { 2, 3, 3, 4, 3, 4, 4, 5, 4, 5, 5, 6, 5, 6, 6, 7, 6, 7, 7, 8, 4, 5, 5, 6, 5, 6, 6, 7, 6,
7, 7, 8 };


/**
 * Lookup table to convert from EIA/CEA TV video format to
 * aspect ratio used in video infoframe:
 * Aspect ratio 1=4:3, 2=16:9
 */
#ifndef FORMAT_PC
	static UInt8 kVfmtToAspect_TV[TMDL_HDMITX_VFMT_TV_NUM] __maybe_unused =
#else				/* FORMAT_PC */
	static UInt8 kVfmtToAspect_TV[TMDL_HDMITX_VFMT_TV_NUM + TMDL_HDMITX_VFMT_PC_NUM] =
#endif				/* FORMAT_PC */
	{
		0,		/* HDMITX_VFMT_NULL                */
		1,		/* HDMITX_VFMT_01_640x480p_60Hz    */
		1,		/* HDMITX_VFMT_02_720x480p_60Hz    */
		2,		/* HDMITX_VFMT_03_720x480p_60Hz    */
		2,		/* HDMITX_VFMT_04_1280x720p_60Hz   */
		2,		/* HDMITX_VFMT_05_1920x1080i_60Hz  */
		1,		/* HDMITX_VFMT_06_720x480i_60Hz    */
		2,		/* HDMITX_VFMT_07_720x480i_60Hz    */
		1,		/* HDMITX_VFMT_08_720x240p_60Hz    */
		2,		/* HDMITX_VFMT_09_720x240p_60Hz    */
		1,		/* HDMITX_VFMT_10_720x480i_60Hz    */
		2,		/* HDMITX_VFMT_11_720x480i_60Hz    */
		1,		/* HDMITX_VFMT_12_720x240p_60Hz    */
		2,		/* HDMITX_VFMT_13_720x240p_60Hz    */
		1,		/* HDMITX_VFMT_14_1440x480p_60Hz   */
		2,		/* HDMITX_VFMT_15_1440x480p_60Hz   */
		2,		/* HDMITX_VFMT_16_1920x1080p_60Hz  */
		1,		/* HDMITX_VFMT_17_720x576p_50Hz    */
		2,		/* HDMITX_VFMT_18_720x576p_50Hz    */
		2,		/* HDMITX_VFMT_19_1280x720p_50Hz   */
		2,		/* HDMITX_VFMT_20_1920x1080i_50Hz  */
		1,		/* HDMITX_VFMT_21_720x576i_50Hz    */
		2,		/* HDMITX_VFMT_22_720x576i_50Hz    */
		1,		/* HDMITX_VFMT_23_720x288p_50Hz    */
		2,		/* HDMITX_VFMT_24_720x288p_50Hz    */
		1,		/* HDMITX_VFMT_25_720x576i_50Hz    */
		2,		/* HDMITX_VFMT_26_720x576i_50Hz    */
		1,		/* HDMITX_VFMT_27_720x288p_50Hz    */
		2,		/* HDMITX_VFMT_28_720x288p_50Hz    */
		1,		/* HDMITX_VFMT_29_1440x576p_50Hz   */
		2,		/* HDMITX_VFMT_30_1440x576p_50Hz   */
		2,		/* HDMITX_VFMT_31_1920x1080p_50Hz  */
		2,		/* HDMITX_VFMT_32_1920x1080p_24Hz  */
		2,		/* HDMITX_VFMT_33_1920x1080p_25Hz  */
		2,		/* HDMITX_VFMT_34_1920x1080p_30Hz  */

		1,		/* TMDL_HDMITX_VFMT_35_2880x480p_60Hz */
		2,		/* TMDL_HDMITX_VFMT_36_2880x480p_60Hz */
		1,		/* TMDL_HDMITX_VFMT_37_2880x576p_50Hz */
		2,		/* TMDL_HDMITX_VFMT_38_2880x576p_50Hz */

		2,		/* TMDL_HDMITX_VFMT_60_1280x720p_24HZ */
		2,		/* TMDL_HDMITX_VFMT_61_1280_720p_25HZ */
		2		/* TMDL_HDMITX_VFMT_62_1280_720p_30HZ */
#ifdef FORMAT_PC
		    , 1,	/* HDMITX_VFMT_PC_640x480p_60Hz    */
		1,		/* HDMITX_VFMT_PC_800x600p_60Hz    */
		1,		/* HDMITX_VFMT_PC_1152x960p_60Hz   */
		1,		/* HDMITX_VFMT_PC_1024x768p_60Hz   */
		1,		/* HDMITX_VFMT_PC_1280x768p_60Hz   */
		1,		/* HDMITX_VFMT_PC_1280x1024p_60Hz  */
		1,		/* HDMITX_VFMT_PC_1360x768p_60Hz   */
		1,		/* HDMITX_VFMT_PC_1400x1050p_60Hz  */
		1,		/* HDMITX_VFMT_PC_1600x1200p_60Hz  */
		1,		/* HDMITX_VFMT_PC_1024x768p_70Hz   */
		1,		/* HDMITX_VFMT_PC_640x480p_72Hz    */
		1,		/* HDMITX_VFMT_PC_800x600p_72Hz    */
		1,		/* HDMITX_VFMT_PC_640x480p_75Hz    */
		1,		/* HDMITX_VFMT_PC_1024x768p_75Hz   */
		1,		/* HDMITX_VFMT_PC_800x600p_75Hz    */
		1,		/* HDMITX_VFMT_PC_1024x864p_75Hz   */
		1,		/* HDMITX_VFMT_PC_1280x1024p_75Hz  */
		1,		/* HDMITX_VFMT_PC_640x350p_85Hz    */
		1,		/* HDMITX_VFMT_PC_640x400p_85Hz    */
		1,		/* HDMITX_VFMT_PC_720x400p_85Hz    */
		1,		/* HDMITX_VFMT_PC_640x480p_85Hz    */
		1,		/* HDMITX_VFMT_PC_800x600p_85Hz    */
		1,		/* HDMITX_VFMT_PC_1024x768p_85Hz   */
		1,		/* HDMITX_VFMT_PC_1152x864p_85Hz   */
		1,		/* HDMITX_VFMT_PC_1280x960p_85Hz   */
		1,		/* HDMITX_VFMT_PC_1280x1024p_85Hz  */
		1		/* HDMITX_VFMT_PC_1024x768i_87Hz   */
#endif				/* FORMAT_PC */
	};

/**
 * Lookup table to convert from EIA/CEA TV video format to
 * the short format of resolution/interlace/frequency
 */
	static UInt8 kVfmtToShortFmt_TV[TMDL_HDMITX_VFMT_TV_NUM] __maybe_unused = {
		TV_INVALID,	/* HDMITX_VFMT_NULL               */
		TV_VGA_60Hz,	/* HDMITX_VFMT_01_640x480p_60Hz   */
		TV_480p_60Hz,	/* HDMITX_VFMT_02_720x480p_60Hz   */
		TV_480p_60Hz,	/* HDMITX_VFMT_03_720x480p_60Hz   */
		TV_720p_60Hz,	/* HDMITX_VFMT_04_1280x720p_60Hz  */
		TV_1080i_60Hz,	/* HDMITX_VFMT_05_1920x1080i_60Hz */
		TV_480i_60Hz,	/* HDMITX_VFMT_06_720x480i_60Hz   */
		TV_480i_60Hz,	/* HDMITX_VFMT_07_720x480i_60Hz   */
		TV_240p_60Hz,	/* HDMITX_VFMT_08_720x240p_60Hz   */
		TV_240p_60Hz,	/* HDMITX_VFMT_09_720x240p_60Hz   */
		TV_480i_60Hz,	/* HDMITX_VFMT_10_720x480i_60Hz   */
		TV_480i_60Hz,	/* HDMITX_VFMT_11_720x480i_60Hz   */
		TV_240p_60Hz,	/* HDMITX_VFMT_12_720x240p_60Hz   */
		TV_240p_60Hz,	/* HDMITX_VFMT_13_720x240p_60Hz   */
		TV_480p_60Hz,	/* HDMITX_VFMT_14_1440x480p_60Hz  */
		TV_480p_60Hz,	/* HDMITX_VFMT_15_1440x480p_60Hz  */
		TV_1080p_60Hz,	/* HDMITX_VFMT_16_1920x1080p_60Hz */
		TV_576p_50Hz,	/* HDMITX_VFMT_17_720x576p_50Hz   */
		TV_576p_50Hz,	/* HDMITX_VFMT_18_720x576p_50Hz   */
		TV_720p_50Hz,	/* HDMITX_VFMT_19_1280x720p_50Hz  */
		TV_1080i_50Hz,	/* HDMITX_VFMT_20_1920x1080i_50Hz */
		TV_576i_50Hz,	/* HDMITX_VFMT_21_720x576i_50Hz   */
		TV_576i_50Hz,	/* HDMITX_VFMT_22_720x576i_50Hz   */
		TV_288p_50Hz,	/* HDMITX_VFMT_23_720x288p_50Hz   */
		TV_288p_50Hz,	/* HDMITX_VFMT_24_720x288p_50Hz   */
		TV_576i_50Hz,	/* HDMITX_VFMT_25_720x576i_50Hz   */
		TV_576i_50Hz,	/* HDMITX_VFMT_26_720x576i_50Hz   */
		TV_288p_50Hz,	/* HDMITX_VFMT_27_720x288p_50Hz   */
		TV_288p_50Hz,	/* HDMITX_VFMT_28_720x288p_50Hz   */
		TV_576p_50Hz,	/* HDMITX_VFMT_29_1440x576p_50Hz  */
		TV_576p_50Hz,	/* HDMITX_VFMT_30_1440x576p_50Hz  */
		TV_1080p_50Hz,	/* HDMITX_VFMT_31_1920x1080p_50Hz */
		TV_INVALID,	/* HDMITX_VFMT_NULL               */
		TV_INVALID,	/* HDMITX_VFMT_NULL               */
		TV_INVALID,	/* HDMITX_VFMT_NULL               */
		TV_480p_60Hz,	/* HDMITX_VFMT_35_2880x480p_60Hz  */
		TV_480p_60Hz,	/* HDMITX_VFMT_36_2880x480p_60Hz  */
		TV_576p_50Hz,	/* HDMITX_VFMT_37_2880x576p_50Hz  */
		TV_576p_50Hz,	/* HDMITX_VFMT_38_2880x576p_50Hz  */
		TV_INVALID,	/* HDMITX_VFMT_NULL               */
		TV_INVALID,	/* HDMITX_VFMT_NULL               */
		TV_INVALID	/* HDMITX_VFMT_NULL               */
	};

/**
 * Macro to pack vinMode(0-5), pixRate(0-1), syncIn(0-1) and bVerified(0-1)
 * into a byte
 */
#define PKBYTE(mode, rate, sync, verf) (((rate)<<7)|((sync)<<6)|((verf)<<5)|((mode)&0x1F))

/**
 * Macros to unpack vinMode(0-5), pixRate(0-1), syncIn(0-1) and bVerified(0-1)
 * from a byte
 */
#define UNPKRATE(byte) (((byte)>>7)&1)
#define UNPKSYNC(byte) (((byte)>>6)&1)
#define UNPKVERF(byte) (((byte)>>5)&1)
#define UNPKMODE(byte) ((byte)&0x1F)

/**
 * Lookup table to match main video settings and look up sets of
 * Refpix and Refline values
 */
	static struct {
		/* Values to match */
		UInt8 modeRateSyncVerf;	/* Packed vinMode, pixRate, syncIn, bVerified */
		UInt8 shortVinFmt;
		UInt8 shortVoutFmt;
		/* Values to look up */
		UInt16 refPix;	/* Output values */
		UInt16 refLine;
		UInt16 scRefPix;	/* Scaler values */
		UInt16 scRefLine;
	} kRefpixRefline[] __maybe_unused = {
  /*************************************************************/
  /** Rows formatted in "Refpix_Refline.xls" and pasted here  **/
  /** DO NOT DELETE ANY ROWS, to keep all scaler combinations **/
  /*************************************************************/
		/*        mode_____Rate___Sync_Verf  shortVinFmt     shortVoutFmt    refPix  refLine  scRefPix  scRefLine  Test ID  */
		{
		PKBYTE(iCCIR656, SINGLE, EMB, 1), TV_480i_60Hz, TV_480p_60Hz, 0x08b, 0x024, 0x078, 0x017},	/* VID_F_04 */
		{
		PKBYTE(iCCIR656, SINGLE, EMB, 1), TV_480i_60Hz, TV_720p_60Hz, 0x08b, 0x012, 0x078, 0x017},	/* VID_F_04 */
		{
		PKBYTE(iCCIR656, SINGLE, EMB, 1), TV_480i_60Hz, TV_1080i_60Hz, 0x08b, 0x00e, 0x078, 0x017},	/* VID_F_04 */
		{
		PKBYTE(iCCIR656, SINGLE, EMB, 1), TV_480i_60Hz, TV_1080p_60Hz, 0x08b, 0x021, 0x078, 0x017},	/*  */
		{
		PKBYTE(iCCIR656, SINGLE, EMB, 1), TV_480p_60Hz, TV_720p_60Hz, 0x08b, 0x017, 0x078, 0x02c},	/* VID_F_01 */
		{
		PKBYTE(iCCIR656, SINGLE, EMB, 1), TV_480p_60Hz, TV_1080i_60Hz, 0x08b, 0x013, 0x078, 0x02c},	/* VID_F_01 */
		{
		PKBYTE(iCCIR656, SINGLE, EMB, 1), TV_480p_60Hz, TV_1080p_60Hz, 0x08b, 0x027, 0x07A, 0x02c},	/*  */
		{
		PKBYTE(iCCIR656, SINGLE, EMB, 1), TV_576i_50Hz, TV_576p_50Hz, 0x091, 0x026, 0x085, 0x018},	/* VID_F_09 */
		{
		PKBYTE(iCCIR656, SINGLE, EMB, 1), TV_576i_50Hz, TV_720p_50Hz, 0x091, 0x013, 0x085, 0x018},	/* VID_F_09 */
		{
		PKBYTE(iCCIR656, SINGLE, EMB, 1), TV_576i_50Hz, TV_1080i_50Hz, 0x091, 0x00f, 0x085, 0x018},	/* VID_F_09 */
		{
		PKBYTE(iCCIR656, SINGLE, EMB, 1), TV_576i_50Hz, TV_1080p_50Hz, 0x091, 0x022, 0x085, 0x018},	/*  */
		{
		PKBYTE(iCCIR656, SINGLE, EMB, 1), TV_576p_50Hz, TV_720p_50Hz, 0x091, 0x019, 0x085, 0x02e},	/* VID_F_06 */
		{
		PKBYTE(iCCIR656, SINGLE, EMB, 1), TV_576p_50Hz, TV_1080i_50Hz, 0x091, 0x014, 0x085, 0x02e},	/* VID_F_06 */
		{
		PKBYTE(iCCIR656, SINGLE, EMB, 1), TV_576p_50Hz, TV_1080p_50Hz, 0x091, 0x028, 0x087, 0x02e},	/*  */
		{
		PKBYTE(iCCIR656, SINGLE, EXT, 1), TV_480i_60Hz, TV_480p_60Hz, 0x014, 0x20d, 0x359, 0x004},	/* VID_F_04 */
		{
		PKBYTE(iCCIR656, SINGLE, EXT, 1), TV_480i_60Hz, TV_720p_60Hz, 0x014, 0x2cb, 0x359, 0x004},	/* VID_F_04 */
		{
		PKBYTE(iCCIR656, SINGLE, EXT, 1), TV_480i_60Hz, TV_1080i_60Hz, 0x014, 0x44c, 0x359, 0x004},	/* VID_F_04 */
		{
		PKBYTE(iCCIR656, SINGLE, EXT, 1), TV_480i_60Hz, TV_1080p_60Hz, 0x014, 0x436, 0x359, 0x004},	/*  */
		{
		PKBYTE(iCCIR656, SINGLE, EXT, 1), TV_480p_60Hz, TV_720p_60Hz, 0x011, 0x2d3, 0x358, 0x007},	/* VID_F_01 */
		{
		PKBYTE(iCCIR656, SINGLE, EXT, 1), TV_480p_60Hz, TV_1080i_60Hz, 0x011, 0x452, 0x358, 0x007},	/* VID_F_01 */
		{
		PKBYTE(iCCIR656, SINGLE, EXT, 1), TV_480p_60Hz, TV_1080p_60Hz, 0x011, 0x43e, 0x358, 0x007},	/*  */
		{
		PKBYTE(iCCIR656, SINGLE, EXT, 1), TV_576i_50Hz, TV_576p_50Hz, 0x00d, 0x26b, 0x35f, 0x001},	/* VID_F_09 */
		{
		PKBYTE(iCCIR656, SINGLE, EXT, 1), TV_576i_50Hz, TV_720p_50Hz, 0x00d, 0x2cb, 0x35f, 0x001},	/* VID_F_09 */
		{
		PKBYTE(iCCIR656, SINGLE, EXT, 1), TV_576i_50Hz, TV_1080i_50Hz, 0x00d, 0x44b, 0x35f, 0x001},	/* VID_F_09 */
		{
		PKBYTE(iCCIR656, SINGLE, EXT, 1), TV_576i_50Hz, TV_1080p_50Hz, 0x00d, 0x435, 0x35f, 0x001},	/*  */
		{
		PKBYTE(iCCIR656, SINGLE, EXT, 1), TV_576p_50Hz, TV_720p_50Hz, 0x00d, 0x2d1, 0x35f, 0x001},	/* VID_F_06 */
		{
		PKBYTE(iCCIR656, SINGLE, EXT, 1), TV_576p_50Hz, TV_1080i_50Hz, 0x00d, 0x451, 0x35f, 0x001},	/* VID_F_06 */
		{
		PKBYTE(iCCIR656, SINGLE, EXT, 1), TV_576p_50Hz, TV_1080p_50Hz, 0x00d, 0x43d, 0x35f, 0x001},	/*  */
		{
		PKBYTE(iCCIR656, DOUBLE, EMB, 1), TV_480i_60Hz, TV_480p_60Hz, 0x08b, 0x024, 0x078, 0x017},	/* VID_F_04 */
		{
		PKBYTE(iCCIR656, DOUBLE, EMB, 1), TV_480i_60Hz, TV_720p_60Hz, 0x08b, 0x012, 0x078, 0x017},	/* VID_F_04 */
		{
		PKBYTE(iCCIR656, DOUBLE, EMB, 1), TV_480i_60Hz, TV_1080i_60Hz, 0x08b, 0x00e, 0x078, 0x017},	/* VID_F_04 */
		{
		PKBYTE(iCCIR656, DOUBLE, EMB, 1), TV_480i_60Hz, TV_1080p_60Hz, 0x08b, 0x021, 0x078, 0x017},	/*  */
		{
		PKBYTE(iCCIR656, DOUBLE, EMB, 1), TV_480p_60Hz, TV_720p_60Hz, 0x08b, 0x017, 0x078, 0x02c},	/* VID_F_01 */
		{
		PKBYTE(iCCIR656, DOUBLE, EMB, 1), TV_480p_60Hz, TV_1080i_60Hz, 0x08b, 0x013, 0x078, 0x02c},	/* VID_F_01 */
		{
		PKBYTE(iCCIR656, DOUBLE, EMB, 1), TV_480p_60Hz, TV_1080p_60Hz, 0x08b, 0x027, 0x07A, 0x02c},	/*  */
		{
		PKBYTE(iCCIR656, DOUBLE, EMB, 1), TV_576i_50Hz, TV_576p_50Hz, 0x091, 0x026, 0x085, 0x018},	/* VID_F_09 */
		{
		PKBYTE(iCCIR656, DOUBLE, EMB, 1), TV_576i_50Hz, TV_720p_50Hz, 0x091, 0x013, 0x085, 0x018},	/* VID_F_09 */
		{
		PKBYTE(iCCIR656, DOUBLE, EMB, 1), TV_576i_50Hz, TV_1080i_50Hz, 0x091, 0x00f, 0x085, 0x018},	/* VID_F_09 */
		{
		PKBYTE(iCCIR656, DOUBLE, EMB, 1), TV_576i_50Hz, TV_1080p_50Hz, 0x091, 0x022, 0x085, 0x018},	/*  */
		{
		PKBYTE(iCCIR656, DOUBLE, EMB, 1), TV_576p_50Hz, TV_720p_50Hz, 0x091, 0x019, 0x085, 0x02e},	/* VID_F_06 */
		{
		PKBYTE(iCCIR656, DOUBLE, EMB, 1), TV_576p_50Hz, TV_1080i_50Hz, 0x091, 0x014, 0x085, 0x02e},	/* VID_F_06 */
		{
		PKBYTE(iCCIR656, DOUBLE, EMB, 1), TV_576p_50Hz, TV_1080p_50Hz, 0x091, 0x028, 0x087, 0x02e},	/*  */
		{
		PKBYTE(iCCIR656, DOUBLE, EXT, 1), TV_480i_60Hz, TV_480p_60Hz, 0x014, 0x20d, 0x359, 0x004},	/* VID_F_04 */
		{
		PKBYTE(iCCIR656, DOUBLE, EXT, 1), TV_480i_60Hz, TV_720p_60Hz, 0x014, 0x2cb, 0x359, 0x004},	/* VID_F_04 */
		{
		PKBYTE(iCCIR656, DOUBLE, EXT, 1), TV_480i_60Hz, TV_1080i_60Hz, 0x014, 0x44c, 0x359, 0x004},	/* VID_F_04 */
		{
		PKBYTE(iCCIR656, DOUBLE, EXT, 1), TV_480i_60Hz, TV_1080p_60Hz, 0x014, 0x436, 0x359, 0x004},	/*  */
		{
		PKBYTE(iCCIR656, DOUBLE, EXT, 1), TV_480p_60Hz, TV_720p_60Hz, 0x011, 0x2d3, 0x358, 0x007},	/* VID_F_01 */
		{
		PKBYTE(iCCIR656, DOUBLE, EXT, 1), TV_480p_60Hz, TV_1080i_60Hz, 0x011, 0x452, 0x358, 0x007},	/* VID_F_01 */
		{
		PKBYTE(iCCIR656, DOUBLE, EXT, 1), TV_480p_60Hz, TV_1080p_60Hz, 0x011, 0x43e, 0x358, 0x007},	/*  */
		{
		PKBYTE(iCCIR656, DOUBLE, EXT, 1), TV_576i_50Hz, TV_576p_50Hz, 0x00d, 0x26b, 0x35f, 0x001},	/* VID_F_09 */
		{
		PKBYTE(iCCIR656, DOUBLE, EXT, 1), TV_576i_50Hz, TV_720p_50Hz, 0x00d, 0x2cb, 0x35f, 0x001},	/* VID_F_09 */
		{
		PKBYTE(iCCIR656, DOUBLE, EXT, 1), TV_576i_50Hz, TV_1080i_50Hz, 0x00d, 0x44b, 0x35f, 0x001},	/* VID_F_09 */
		{
		PKBYTE(iCCIR656, DOUBLE, EXT, 1), TV_576i_50Hz, TV_1080p_50Hz, 0x00d, 0x435, 0x35f, 0x001},	/*  */
		{
		PKBYTE(iCCIR656, DOUBLE, EXT, 1), TV_576p_50Hz, TV_720p_50Hz, 0x00d, 0x2d1, 0x35f, 0x001},	/* VID_F_06 */
		{
		PKBYTE(iCCIR656, DOUBLE, EXT, 1), TV_576p_50Hz, TV_1080i_50Hz, 0x00d, 0x451, 0x35f, 0x001},	/* VID_F_06 */
		{
		PKBYTE(iCCIR656, DOUBLE, EXT, 1), TV_576p_50Hz, TV_1080p_50Hz, 0x00d, 0x43d, 0x35f, 0x001},	/*  */
		{
		PKBYTE(iYUV422, SINGLE, EMB, 1), TV_480i_60Hz, TV_480p_60Hz, 0x08d, 0x028, 0x078, 0x017},	/* VID_F_04 */
		{
		PKBYTE(iYUV422, SINGLE, EMB, 1), TV_480i_60Hz, TV_720p_60Hz, 0x08d, 0x014, 0x078, 0x017},	/* VID_F_04 */
		{
		PKBYTE(iYUV422, SINGLE, EMB, 1), TV_480i_60Hz, TV_1080i_60Hz, 0x08d, 0x010, 0x078, 0x017},	/* VID_F_04 */
		{
		PKBYTE(iYUV422, SINGLE, EMB, 1), TV_480i_60Hz, TV_1080p_60Hz, 0x08d, 0x021, 0x078, 0x017},	/*  */
		{
		PKBYTE(iYUV422, SINGLE, EMB, 1), TV_480p_60Hz, TV_720p_60Hz, 0x08d, 0x017, 0x078, 0x02c},	/* VID_F_01 */
		{
		PKBYTE(iYUV422, SINGLE, EMB, 1), TV_480p_60Hz, TV_1080i_60Hz, 0x08d, 0x014, 0x078, 0x02c},	/* VID_F_01 */
		{
		PKBYTE(iYUV422, SINGLE, EMB, 1), TV_480p_60Hz, TV_1080p_60Hz, 0x08d, 0x027, 0x07C, 0x02c},	/*  */
		{
		PKBYTE(iYUV422, SINGLE, EMB, 1), TV_576i_50Hz, TV_576p_50Hz, 0x093, 0x02a, 0x085, 0x018},	/* VID_F_09 */
		{
		PKBYTE(iYUV422, SINGLE, EMB, 1), TV_576i_50Hz, TV_720p_50Hz, 0x093, 0x013, 0x085, 0x018},	/* VID_F_09 */
		{
		PKBYTE(iYUV422, SINGLE, EMB, 1), TV_576i_50Hz, TV_1080i_50Hz, 0x093, 0x00e, 0x085, 0x018},	/* VID_F_09 */
		{
		PKBYTE(iYUV422, SINGLE, EMB, 1), TV_576i_50Hz, TV_1080p_50Hz, 0x093, 0x022, 0x085, 0x018},	/*  */
		{
		PKBYTE(iYUV422, SINGLE, EMB, 1), TV_576p_50Hz, TV_720p_50Hz, 0x093, 0x019, 0x085, 0x02e},	/* VID_F_06 */
		{
		PKBYTE(iYUV422, SINGLE, EMB, 1), TV_576p_50Hz, TV_1080i_50Hz, 0x093, 0x014, 0x085, 0x02e},	/* VID_F_06 */
		{
		PKBYTE(iYUV422, SINGLE, EMB, 1), TV_576p_50Hz, TV_1080p_50Hz, 0x093, 0x028, 0x089, 0x02e},	/*  */
		{
		PKBYTE(iYUV422, SINGLE, EMB, 1), TV_720p_50Hz, TV_1080p_50Hz, 0x2bf, 0x024, 0x105, 0x019},	/*  */
		{
		PKBYTE(iYUV422, SINGLE, EMB, 1), TV_720p_60Hz, TV_1080p_60Hz, 0x175, 0x024, 0x105, 0x019},	/*  */
		{
		PKBYTE(iYUV422, SINGLE, EMB, 1), TV_1080i_50Hz, TV_1080p_50Hz, 0x2d3, 0x023, 0x0c3, 0x014},	/*  */
		{
		PKBYTE(iYUV422, SINGLE, EMB, 1), TV_1080i_60Hz, TV_1080p_60Hz, 0x11b, 0x023, 0x0c3, 0x014},	/*  */
		{
		PKBYTE(iYUV422, SINGLE, EXT, 1), TV_480i_60Hz, TV_480p_60Hz, 0x016, 0x20d, 0x359, 0x004},	/* VID_F_04 */
		{
		PKBYTE(iYUV422, SINGLE, EXT, 1), TV_480i_60Hz, TV_720p_60Hz, 0x016, 0x2cb, 0x359, 0x004},	/* VID_F_04 */
		{
		PKBYTE(iYUV422, SINGLE, EXT, 1), TV_480i_60Hz, TV_1080i_60Hz, 0x016, 0x44c, 0x359, 0x004},	/* VID_F_04 */
		{
		PKBYTE(iYUV422, SINGLE, EXT, 1), TV_480i_60Hz, TV_1080p_60Hz, 0x016, 0x436, 0x359, 0x004},	/*  */
		{
		PKBYTE(iYUV422, SINGLE, EXT, 1), TV_480p_60Hz, TV_720p_60Hz, 0x013, 0x2d3, 0x358, 0x007},	/* VID_F_01 */
		{
		PKBYTE(iYUV422, SINGLE, EXT, 1), TV_480p_60Hz, TV_1080i_60Hz, 0x013, 0x452, 0x358, 0x007},	/* VID_F_01 */
		{
		PKBYTE(iYUV422, SINGLE, EXT, 1), TV_480p_60Hz, TV_1080p_60Hz, 0x013, 0x43e, 0x358, 0x007},	/*  */
		{
		PKBYTE(iYUV422, SINGLE, EXT, 1), TV_576i_50Hz, TV_576p_50Hz, 0x00f, 0x26b, 0x35f, 0x001},	/* VID_F_09 */
		{
		PKBYTE(iYUV422, SINGLE, EXT, 1), TV_576i_50Hz, TV_720p_50Hz, 0x00f, 0x2cb, 0x35f, 0x001},	/* VID_F_09 */
		{
		PKBYTE(iYUV422, SINGLE, EXT, 1), TV_576i_50Hz, TV_1080i_50Hz, 0x00f, 0x44b, 0x35f, 0x001},	/* VID_F_09 */
		{
		PKBYTE(iYUV422, SINGLE, EXT, 1), TV_576i_50Hz, TV_1080p_50Hz, 0x00f, 0x435, 0x35f, 0x001},	/*  */
		{
		PKBYTE(iYUV422, SINGLE, EXT, 1), TV_576p_50Hz, TV_720p_50Hz, 0x00f, 0x2d1, 0x35f, 0x001},	/* VID_F_06 */
		{
		PKBYTE(iYUV422, SINGLE, EXT, 1), TV_576p_50Hz, TV_1080i_50Hz, 0x00f, 0x451, 0x35f, 0x001},	/* VID_F_06 */
		{
		PKBYTE(iYUV422, SINGLE, EXT, 1), TV_576p_50Hz, TV_1080p_50Hz, 0x00f, 0x43d, 0x35f, 0x001},	/*  */
		{
		PKBYTE(iYUV422, SINGLE, EXT, 1), TV_720p_50Hz, TV_1080p_50Hz, 0x1bb, 0x463, 0x7bb, 0x000},	/*  */
		{
		PKBYTE(iYUV422, SINGLE, EXT, 1), TV_720p_60Hz, TV_1080p_60Hz, 0x071, 0x463, 0x671, 0x000},	/*  */
		{
		PKBYTE(iYUV422, SINGLE, EXT, 1), TV_1080i_50Hz, TV_1080p_50Hz, 0x213, 0x460, 0xa4f, 0x000},	/*  */
		{
		PKBYTE(iYUV422, SINGLE, EXT, 1), TV_1080i_60Hz, TV_1080p_60Hz, 0x05b, 0x460, 0x897, 0x000},	/*  */
		{
		PKBYTE(iYUV422, DOUBLE, EMB, 0), TV_480i_60Hz, TV_480p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iYUV422, DOUBLE, EMB, 0), TV_480i_60Hz, TV_720p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iYUV422, DOUBLE, EMB, 0), TV_480i_60Hz, TV_1080i_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iYUV422, DOUBLE, EMB, 0), TV_480p_60Hz, TV_720p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_01 */
		{
		PKBYTE(iYUV422, DOUBLE, EMB, 0), TV_480p_60Hz, TV_1080i_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_01 */
		{
		PKBYTE(iYUV422, DOUBLE, EMB, 0), TV_576i_50Hz, TV_576p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iYUV422, DOUBLE, EMB, 0), TV_576i_50Hz, TV_720p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iYUV422, DOUBLE, EMB, 0), TV_576i_50Hz, TV_1080i_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iYUV422, DOUBLE, EMB, 0), TV_576p_50Hz, TV_720p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_06 */
		{
		PKBYTE(iYUV422, DOUBLE, EMB, 0), TV_576p_50Hz, TV_1080i_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_06 */
		{
		PKBYTE(iYUV422, DOUBLE, EXT, 0), TV_480i_60Hz, TV_480p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iYUV422, DOUBLE, EXT, 0), TV_480i_60Hz, TV_720p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iYUV422, DOUBLE, EXT, 0), TV_480i_60Hz, TV_1080i_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iYUV422, DOUBLE, EXT, 0), TV_480p_60Hz, TV_720p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_01 */
		{
		PKBYTE(iYUV422, DOUBLE, EXT, 0), TV_480p_60Hz, TV_1080i_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_01 */
		{
		PKBYTE(iYUV422, DOUBLE, EXT, 0), TV_576i_50Hz, TV_576p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iYUV422, DOUBLE, EXT, 0), TV_576i_50Hz, TV_720p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iYUV422, DOUBLE, EXT, 0), TV_576i_50Hz, TV_1080i_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iYUV422, DOUBLE, EXT, 0), TV_576p_50Hz, TV_720p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_06 */
		{
		PKBYTE(iYUV422, DOUBLE, EXT, 0), TV_576p_50Hz, TV_1080i_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_06 */
		{
		PKBYTE(iYUV444, SINGLE, EMB, 0), TV_480i_60Hz, TV_480p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iYUV444, SINGLE, EMB, 0), TV_480i_60Hz, TV_720p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iYUV444, SINGLE, EMB, 0), TV_480i_60Hz, TV_1080i_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iYUV444, SINGLE, EMB, 0), TV_480p_60Hz, TV_720p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_01 */
		{
		PKBYTE(iYUV444, SINGLE, EMB, 0), TV_480p_60Hz, TV_1080i_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_01 */
		{
		PKBYTE(iYUV444, SINGLE, EMB, 0), TV_576i_50Hz, TV_576p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iYUV444, SINGLE, EMB, 0), TV_576i_50Hz, TV_720p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iYUV444, SINGLE, EMB, 0), TV_576i_50Hz, TV_1080i_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iYUV444, SINGLE, EMB, 0), TV_576p_50Hz, TV_720p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_06 */
		{
		PKBYTE(iYUV444, SINGLE, EMB, 0), TV_576p_50Hz, TV_1080i_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_06 */
		{
		PKBYTE(iYUV444, SINGLE, EXT, 0), TV_480i_60Hz, TV_480p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iYUV444, SINGLE, EXT, 0), TV_480i_60Hz, TV_720p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iYUV444, SINGLE, EXT, 0), TV_480i_60Hz, TV_1080i_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iYUV444, SINGLE, EXT, 0), TV_480p_60Hz, TV_720p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_01 */
		{
		PKBYTE(iYUV444, SINGLE, EXT, 0), TV_480p_60Hz, TV_1080i_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_01 */
		{
		PKBYTE(iYUV444, SINGLE, EXT, 0), TV_576i_50Hz, TV_576p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iYUV444, SINGLE, EXT, 0), TV_576i_50Hz, TV_720p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iYUV444, SINGLE, EXT, 0), TV_576i_50Hz, TV_1080i_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iYUV444, SINGLE, EXT, 0), TV_576p_50Hz, TV_720p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_06 */
		{
		PKBYTE(iYUV444, SINGLE, EXT, 0), TV_576p_50Hz, TV_1080i_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_06 */
		{
		PKBYTE(iYUV444, DOUBLE, EMB, 0), TV_480i_60Hz, TV_480p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iYUV444, DOUBLE, EMB, 0), TV_480i_60Hz, TV_720p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iYUV444, DOUBLE, EMB, 0), TV_480i_60Hz, TV_1080i_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iYUV444, DOUBLE, EMB, 0), TV_480p_60Hz, TV_720p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_01 */
		{
		PKBYTE(iYUV444, DOUBLE, EMB, 0), TV_480p_60Hz, TV_1080i_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_01 */
		{
		PKBYTE(iYUV444, DOUBLE, EMB, 0), TV_576i_50Hz, TV_576p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iYUV444, DOUBLE, EMB, 0), TV_576i_50Hz, TV_720p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iYUV444, DOUBLE, EMB, 0), TV_576i_50Hz, TV_1080i_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iYUV444, DOUBLE, EMB, 0), TV_576p_50Hz, TV_720p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_06 */
		{
		PKBYTE(iYUV444, DOUBLE, EMB, 0), TV_576p_50Hz, TV_1080i_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_06 */
		{
		PKBYTE(iYUV444, DOUBLE, EXT, 0), TV_480i_60Hz, TV_480p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iYUV444, DOUBLE, EXT, 0), TV_480i_60Hz, TV_720p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iYUV444, DOUBLE, EXT, 0), TV_480i_60Hz, TV_1080i_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iYUV444, DOUBLE, EXT, 0), TV_480p_60Hz, TV_720p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_01 */
		{
		PKBYTE(iYUV444, DOUBLE, EXT, 0), TV_480p_60Hz, TV_1080i_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_01 */
		{
		PKBYTE(iYUV444, DOUBLE, EXT, 0), TV_576i_50Hz, TV_576p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iYUV444, DOUBLE, EXT, 0), TV_576i_50Hz, TV_720p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iYUV444, DOUBLE, EXT, 0), TV_576i_50Hz, TV_1080i_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iYUV444, DOUBLE, EXT, 0), TV_576p_50Hz, TV_720p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_06 */
		{
		PKBYTE(iYUV444, DOUBLE, EXT, 0), TV_576p_50Hz, TV_1080i_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_06 */
		{
		PKBYTE(iRGB444, SINGLE, EMB, 0), TV_480i_60Hz, TV_480p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iRGB444, SINGLE, EMB, 0), TV_480i_60Hz, TV_720p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iRGB444, SINGLE, EMB, 0), TV_480i_60Hz, TV_1080i_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iRGB444, SINGLE, EMB, 0), TV_480p_60Hz, TV_720p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_01 */
		{
		PKBYTE(iRGB444, SINGLE, EMB, 0), TV_480p_60Hz, TV_1080i_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_01 */
		{
		PKBYTE(iRGB444, SINGLE, EMB, 0), TV_576i_50Hz, TV_576p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iRGB444, SINGLE, EMB, 0), TV_576i_50Hz, TV_720p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iRGB444, SINGLE, EMB, 0), TV_576i_50Hz, TV_1080i_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iRGB444, SINGLE, EMB, 0), TV_576p_50Hz, TV_720p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_06 */
		{
		PKBYTE(iRGB444, SINGLE, EMB, 0), TV_576p_50Hz, TV_1080i_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_06 */
		{
		PKBYTE(iRGB444, SINGLE, EXT, 0), TV_480i_60Hz, TV_480p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iRGB444, SINGLE, EXT, 0), TV_480i_60Hz, TV_720p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iRGB444, SINGLE, EXT, 0), TV_480i_60Hz, TV_1080i_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iRGB444, SINGLE, EXT, 0), TV_480p_60Hz, TV_720p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_01 */
		{
		PKBYTE(iRGB444, SINGLE, EXT, 0), TV_480p_60Hz, TV_1080i_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_01 */
		{
		PKBYTE(iRGB444, SINGLE, EXT, 0), TV_480p_60Hz, TV_VGA_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_01 */
		{
		PKBYTE(iRGB444, SINGLE, EXT, 0), TV_576i_50Hz, TV_576p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iRGB444, SINGLE, EXT, 0), TV_576i_50Hz, TV_720p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iRGB444, SINGLE, EXT, 0), TV_576i_50Hz, TV_1080i_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iRGB444, SINGLE, EXT, 0), TV_576p_50Hz, TV_720p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_06 */
		{
		PKBYTE(iRGB444, SINGLE, EXT, 0), TV_576p_50Hz, TV_1080i_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_06 */
		{
		PKBYTE(iRGB444, DOUBLE, EMB, 0), TV_480i_60Hz, TV_480p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iRGB444, DOUBLE, EMB, 0), TV_480i_60Hz, TV_720p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iRGB444, DOUBLE, EMB, 0), TV_480i_60Hz, TV_1080i_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iRGB444, DOUBLE, EMB, 0), TV_480p_60Hz, TV_720p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_01 */
		{
		PKBYTE(iRGB444, DOUBLE, EMB, 0), TV_480p_60Hz, TV_1080i_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_01 */
		{
		PKBYTE(iRGB444, DOUBLE, EMB, 0), TV_576i_50Hz, TV_576p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iRGB444, DOUBLE, EMB, 0), TV_576i_50Hz, TV_720p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iRGB444, DOUBLE, EMB, 0), TV_576i_50Hz, TV_1080i_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iRGB444, DOUBLE, EMB, 0), TV_576p_50Hz, TV_720p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_06 */
		{
		PKBYTE(iRGB444, DOUBLE, EMB, 0), TV_576p_50Hz, TV_1080i_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_06 */
		{
		PKBYTE(iRGB444, DOUBLE, EXT, 0), TV_480i_60Hz, TV_480p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iRGB444, DOUBLE, EXT, 0), TV_480i_60Hz, TV_720p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iRGB444, DOUBLE, EXT, 0), TV_480i_60Hz, TV_1080i_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_04 */
		{
		PKBYTE(iRGB444, DOUBLE, EXT, 0), TV_480p_60Hz, TV_720p_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_01 */
		{
		PKBYTE(iRGB444, DOUBLE, EXT, 0), TV_480p_60Hz, TV_1080i_60Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_01 */
		{
		PKBYTE(iRGB444, DOUBLE, EXT, 0), TV_576i_50Hz, TV_576p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iRGB444, DOUBLE, EXT, 0), TV_576i_50Hz, TV_720p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iRGB444, DOUBLE, EXT, 0), TV_576i_50Hz, TV_1080i_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_09 */
		{
		PKBYTE(iRGB444, DOUBLE, EXT, 0), TV_576p_50Hz, TV_720p_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_06 */
		{
		PKBYTE(iRGB444, DOUBLE, EXT, 0), TV_576p_50Hz, TV_1080i_50Hz, 0x000, 0x000, 0x000, 0x000},	/* VID_F_06 */
		{
		PKBYTE(iINVALID, DOUBLE, EMB, 0), TV_INVALID, TV_INVALID, 0x000, 0x000, 0x000, 0x000}	/* EndTable */
	};


/*============================================================================*/
/*                         ENUM OR TYPE DEFINITIONS                           */
/*============================================================================*/
/* Enum listing all the type of colorimetry */
	typedef enum {
		TMDL_HDMITX_COLORIMETRY_NO_DATA = 0,
		TMDL_HDMITX_COLORIMETRY_ITU601 = 1,
		TMDL_HDMITX_COLORIMETRY_ITU709 = 2,
		TMDL_HDMITX_COLORIMETRY_EXTENDED = 3
	} tmdlHdmiTxColorimetry_t;

/* Possible states of the state machine */
	typedef enum {
		STATE_NOT_INITIALIZED,
			    /**< Driver is not initialized */
		STATE_INITIALIZED,
			    /**< Driver is initialized */
		STATE_UNPLUGGED,
			    /**< Receiver device not connected */
		STATE_PLUGGED,
			    /**< Receiver device connected, clock lock */
		STATE_EDID_AVAILABLE
			    /**< Managed to read receiver's EDID */
	} tmdlHdmiTxDriverState_t;

/* revocation list structure */
	typedef struct {
		UInt8 *pList;
		UInt32 length;
	} revocationList_t;


/* unit configuration structure */
	typedef struct {
		Bool opened;						/**< Is unit instanciated ? */
		Bool hdcpEnable;					/**< Is HDCP enabled ? */
		tmdlHdmiTxHdcpOptions_t hdcpOptions;		/**< HDCP options */
		Bool repeaterEnable;				/**< Is repeater enabled ? */
		Bool simplayHd;						/**< Enable simplayHD support */
		tmdlHdmiTxDeviceVersion_t deviceVersion;	/**< Version of the HW device */
		UInt8 *pEdidBuffer;				/**< Pointer to raw EDID data */
		UInt32 edidBufferSize;				/**< Size of buffer for raw EDID data */
		tmdlHdmiTxIWTaskHandle_t commandTaskHandle;	/**< Handle of the command task associated to this unit */
		tmdlHdmiTxIWQueueHandle_t queueHandle;		/**< Handle of the message queue associated to this unit */
		tmdlHdmiTxIWTaskHandle_t hdcpTaskHandle;		/**< Handle of the hdcp check task associated to this unit */
		tmdlHdmiTxDriverState_t state;				/**< Current state of the driver */
		ptmdlHdmiTxCallback_t pCallback;			/**< Data callback */
		revocationList_t revocationList;	/**< Revolation List */
	} unitConfig_t;


/* Instance status */

/* Video information structure */
	typedef struct _tmdlHdmiTxVideoInfo_t {
		Bool videoMuteState;	/* Video mute state: on/off */
		tmdlHdmiTxVideoInConfig_t videoInConfig;	/* Video input configuration */
		tmdlHdmiTxVideoOutConfig_t videoOutConfig;	/* Video output configuration */
	} tmdlHdmiTxVideoInfo_t, *ptmdlHdmiTxVideoInfo_t;

/* Audio information structure */
	typedef struct _tmdlHdmiTxAudioInfo_t {
		Bool audioMuteState;	/* Audio mute state: on/off */
		tmdlHdmiTxAudioInConfig_t audioInCfg;	/* Audio input configuration */
	} tmdlHdmiTxAudioInfo_t, *ptmdlHdmiTxAudioInfo_t;

/* Event state structure */
	typedef struct _tmdlHdmiTxEventState_t {
		tmdlHdmiTxEvent_t event;	/* Event */
		tmdlHdmiTxEventStatus_t status;	/* Event status: enabled or disabled */
	} tmdlHdmiTxEventState_t, *ptmdlHdmiTxEventState_t;

/* Color bars state structure */
	typedef struct _tmdlHdmiTxColBarState_t {
		Bool disableColorBarOnR0;	/* To be able to disable colorBar on R0 */
		Bool hdcpColbarChange;	/* Used to auto-reset colour bars */
		Bool hdcpEncryptOrT0;	/* True when ENCRYPT or T0 interrupt */
		Bool hdcpSecureOrT0;	/* True when BKSV secure or T0 */
		Bool inOutFirstSetDone;	/* API tmdlHdmiTxSetInputOutput call at least one time */
		Bool colorBarOn;
		Bool changeColorBarNow;
	} tmdlHdmiTxColBarState_t, *ptmdlHdmiTxColBarState_t;

/* Gamut state structure */
	typedef struct _tmdlHdmiTxGamutState_t {
		Bool gamutOn;	/* Gamut status : able or disable */
		UInt8 gamutBufNum;	/* Numero of the buffer used for Gamut metadata (0 or 1) */
		tmdlHdmiTxExtColorimetry_t wideGamutColorSpace;	/* Store extended colorimetry */
		Bool extColOn;	/* extended colorimetry status : enabled or disabled */
		tmdlHdmiTxYCCQR_t yccQR;	/* Store YCC quantisation range */
	} tmdlHdmiTxGamutState_t, *ptmdlHdmiTxGamutState_t;


/* instance status structure */
	typedef struct {
		ptmdlHdmiTxVideoInfo_t pVideoInfo;	/* Video information: current mode and format... */
		ptmdlHdmiTxAudioInfo_t pAudioInfo;	/* Audio information: current mode and format... */
		ptmdlHdmiTxEventState_t pEventState;	/* Event state: enabled or disabled */
		ptmdlHdmiTxColBarState_t pColBarState;	/* Color bars state */
		ptmdlHdmiTxGamutState_t pGamutState;	/* Gamut state */
	} instanceStatus_t;

/*============================================================================*/
/*                         FUNCTION PROTOTYPES                                */
/*============================================================================*/


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
	tmErrorCode_t dlHdmiTxGetConfig
	    (tmUnitSelect_t unit, tmdlHdmiTxDriverConfigTable_t *pConfig);


#ifdef __cplusplus
}
#endif

#endif				/* TMDLHDMITX_LOCAL_H */

/*============================================================================*/
/*                               END OF FILE                                  */
/*============================================================================*/
