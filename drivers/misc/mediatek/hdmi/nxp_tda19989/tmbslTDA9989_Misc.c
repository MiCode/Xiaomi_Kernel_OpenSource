/**
 * Copyright (C) 2009 NXP N.V., All Rights Reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of NXP N.V. and is confidential in nature. Under no circumstances
 * is this software to be  exposed to or placed under an Open Source License of
 * any type without the expressed written permission of NXP N.V.
 *
 * \file          tmbslTDA9989_misc.c
 *
 * \version        %version: 3 %
 *
 *
*/

/*============================================================================*/
/*                       INCLUDE FILES                                        */
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


#ifdef TMFL_LINUX_OS_KERNEL_DRIVER
#include <linux/kernel.h>
#endif

#include "tmbslHdmiTx_types.h"
#include "tmbslTDA9989_Functions.h"
#include "tmbslTDA9989_local.h"
#include "tmbslTDA9989_HDCP_l.h"
#include "tmbslTDA9989_State_l.h"
#include "tmbslTDA9989_InOut_l.h"
#include "tmbslTDA9989_Edid_l.h"
#include "tmbslTDA9989_Misc_l.h"

/*============================================================================*/
/*                     TYPES DECLARATIONS                                     */
/*============================================================================*/

/*============================================================================*/
/*                       CONSTANTS DECLARATIONS EXPORTED                      */
/*============================================================================*/

/*============================================================================*/
/*                       CONSTANTS DECLARATIONS                               */
/*============================================================================*/

/** Preset default values for an object instance */
static tmHdmiTxobject_t kHdmiTxInstanceDefault = {
	ST_UNINITIALIZED,	/* state */
	0,			/* nIgnoredEvents */
	tmUnit0,		/* txUnit */
	0,			/* uHwAddress */
	(ptmbslHdmiTxSysFunc_t) 0,	/* sysFuncWrite */
	(ptmbslHdmiTxSysFunc_t) 0,	/* sysFuncRead */
	(ptmbslHdmiTxSysFuncEdid_t) 0,	/* sysFuncEdidRead */
	(ptmbslHdmiTxSysFuncTimer_t) 0,	/* sysFuncTimer */
	{			/* funcIntCallbacks[] */
	 (ptmbslHdmiTxCallback_t) 0},
	0,			/* InterruptsEnable */
	{			/* uSupportedVersions[] */
	 E_DEV_VERSION_N2,
	 E_DEV_VERSION_TDA19989,
	 E_DEV_VERSION_TDA19989_N2,
	 E_DEV_VERSION_TDA19988,
	 E_DEV_VERSION_LIST_END},
	E_DEV_VERSION_LIST_END,	/* uDeviceVersion */
	E_DEV_VERSION_LIST_END,	/* uDeviceFeatures */
	(tmbslHdmiTxPowerState_t) tmPowerOff,	/* ePowerState */
	False,			/* EdidAlternateAddr */
	HDMITX_SINK_DVI,	/* sinkType */
	HDMITX_SINK_DVI,	/* EdidSinkType */
	False,			/* EdidSinkAi */
	0,			/* EdidCeaFlags */

	0,			/* EdidCeaXVYCCFlags */
	{
	 False,			/* latency_available */
	 False,			/* Ilatency_available */
	 0,			/* Edidvideo_latency */
	 0,			/* Edidaudio_latency */
	 0,			/* EdidIvideo_latency */
	 0},			/* EdidIaudio_latency */

	{
	 0,			/* maximum supported TMDS clock */
	 0,			/* content type Graphics (text) */
	 0,			/* content type Photo */
	 0,			/* content type Cinema */
	 0,			/* content type Game */
	 0,			/* additional video format */
	 0,			/* 3D support by the HDMI Sink */
	 0,			/* 3D multi strctures present */
	 0,			/* additional info for the values in the image size area */
	 0,			/* total length of 3D video formats */
	 0,			/* total length of extended video formats */
	 {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}	/* max_len-10, ie: 31-10=21 */
	 },

	HDMITX_EDID_NOT_READ,	/* EdidStatus */
	0,			/* NbDTDStored */
	{			/* EdidDTD: *//* * NUMBER_DTD_STORED */
	 {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/*1 */
	 {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/*2 */
	 {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/*3 */
	 {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/*4 */
	 {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/*5 */
	 {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/*6 */
	 {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/*7 */
	 {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/*8 */
	 {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/*9 */
	 {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}	/*10 */
	 },
	{			/* EdidMonitorDescriptor */
	 False,			/* bDescRecord */
	 {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}	/* uMonitorName[EDID_MONITOR_DESCRIPTOR_SIZE]       */
	 },
	{
	 False,			/* bDescRecord */
	 0,			/* uMinVerticalRate                                 */
	 0,			/* uMaxVerticalRate                                 */
	 0,			/* uMinHorizontalRate                               */
	 0,			/* uMaxHorizontalRate                               */
	 0			/* uMaxSupportedPixelClk                            */
	 },
	{
	 False,			/* bDescRecord */
	 {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}	/* uOtherDescriptor[EDID_MONITOR_DESCRIPTOR_SIZE]   */
	 },
	{			/* EdidVFmts[] */
	 HDMITX_VFMT_NULL},
	0,			/* EdidSvdCnt */
	{			/* EdidAFmts[]. */
	 {0, 0, 0}		/* {ModeChans, Freqs, Byte3} */
	 },
	0,			/* EdidSadCnt */
	{
	 0			/* EdidBlock[ ] */
	 },
	0,			/* EdidBlockCnt */
	0,			/* EdidSourceAddress */
	0,			/* EdidBlockRequested */
	False,			/* EdidReadStarted */
	{			/* EdidToApp */
	 0,			/* pRawEdid */
	 0			/* numBlocks */
	 },
	{			/* EDIDBasicDisplayParam */
	 0,			/* uVideoInputDef */
	 0,			/* uMaxHorizontalSize */
	 0,			/* uMaxVerticalSize */
	 0,			/* uGamma */
	 0,			/* uFeatureSupport */
	 },
#ifdef TMFL_HDCP_SUPPORT
	False,			/* HDCPIgnoreEncrypt */
	0,			/* HdcpPortAddress */
	HDMITX_HDCP_TXMODE_NOT_SET,	/* HdcpTxMode */
	HDMITX_HDCP_OPTION_DEFAULT,	/* HdcpOptions */
	0,			/* HdcpBcaps */
	0,			/* HdcpBstatus */
	0,			/* HdcpRi */
	0,			/* HdcpFsmState */
	0,			/* HdcpT0FailState */
	0,			/* HdcpSeed */
	{0, 0, 0, 0, 0},	/* HdcpAksv */
	(ptmHdmiTxFunc_t) 0,	/* HdcpFuncScheduled */
	0,			/* HdcpFuncRemainingMs */
	0,			/* HdcpCheckIntervalMs */
	0,			/* HdcpCheckRemainingMs */
	0,			/* HdcpCheckNum */
	0,			/* HdcpChecksToDo */
#endif				/* TMFL_HDCP_SUPPORT */
	HDMITX_VFMT_NULL,	/* vinFmt */
	HDMITX_VFMT_NULL,	/* voutFmt */
	HDMITX_PIXRATE_DOUBLE,	/* pixRate */
	HDMITX_VINMODE_RGB444,	/* vinMode */
	HDMITX_VOUTMODE_RGB444,	/* voutMode */
	HDMITX_VFREQ_INVALID,	/* voutFreq */
	HDMITX_SCAMODE_OFF,	/* scaMode */
	HDMITX_UPSAMPLE_AUTO,	/* upsampleMode */
	HDMITX_PIXREP_MIN,	/* pixelRepeatCount */
	HDMITX_HOTPLUG_INVALID,	/* hotPlugStatus */
	HDMITX_RX_SENSE_INVALID,	/* rxSenseStatus */
	E_PAGE_INVALID,		/* curRegPage */
	{
	 /* These match power-up defaults.  shadowReg[]: */
	 0x00,			/* E_SP00_INT_FLAGS_0 */
	 0x00,			/* E_SP00_INT_FLAGS_1 */
	 0x00,			/* E_SP00_INT_FLAGS_2 */
	 0x01,			/* E_SP00_VIP_CNTRL_0 */
	 0x24,			/* E_SP00_VIP_CNTRL_1 */
	 0x56,			/* E_SP00_VIP_CNTRL_2 */
	 0x17,			/* E_SP00_VIP_CNTRL_3 */
	 0x01,			/* E_SP00_VIP_CNTRL_4 */
	 0x00,			/* E_SP00_VIP_CNTRL_5 */
	 0x05,			/* E_SP00_MAT_CONTRL  */
	 0x00,			/* E_SP00_TBG_CNTRL_0 */
	 0x00,			/* E_SP00_TBG_CNTRL_1 */
	 0x00,			/* E_SP00_HVF_CNTRL_0 */
	 0x00,			/* E_SP00_HVF_CNTRL_1 */
	 0x00,			/* E_SP00_TIMER_H     */
	 0x00,			/* E_SP00_DEBUG_PROBE */
	 0x00			/* E_SP00_AIP_CLKSEL  */
	 , 0x00			/* E_SP01_SC_VIDFORMAT */
	 , 0x00			/* E_SP01_SC_CNTRL    */
	 , 0x00			/* E_SP01_TBG_CNTRL_0 */
#ifdef TMFL_HDCP_SUPPORT
	 , 0x00			/* E_SP12_CTRL   */
	 , 0x00			/* E_SP12_BCAPS  */
#endif				/* TMFL_HDCP_SUPPORT */
	 },
	False,			/* Init prevFilterPattern to false */
	False,			/* Init prevPattern to false */
	False,			/* bInitialized */
	HDMITX_VQR_DEFAULT
};


/**
 * Table of shadow registers, as packed Shad/Page/Addr codes.
 * This allows shadow index values to be searched for using register page
 * and address values.
 */
static UInt16 kShadowReg[E_SNUM] = {	/* Shadow Index                  Packed Shad/Page/Addr */
	E_REG_P00_INT_FLAGS_0_RW,	/* E_SP00_INT_FLAGS_0  */
	E_REG_P00_INT_FLAGS_1_RW,	/* E_SP00_INT_FLAGS_1  */
	E_REG_P00_INT_FLAGS_2_RW,	/* E_SP00_INT_FLAGS_2  */
	E_REG_P00_VIP_CNTRL_0_W,	/* E_SP00_VIP_CNTRL_0  */
	E_REG_P00_VIP_CNTRL_1_W,	/* E_SP00_VIP_CNTRL_1  */
	E_REG_P00_VIP_CNTRL_2_W,	/* E_SP00_VIP_CNTRL_2  */
	E_REG_P00_VIP_CNTRL_3_W,	/* E_SP00_VIP_CNTRL_3  */
	E_REG_P00_VIP_CNTRL_4_W,	/* E_SP00_VIP_CNTRL_4  */
	E_REG_P00_VIP_CNTRL_5_W,	/* E_SP00_VIP_CNTRL_5  */
	E_REG_P00_MAT_CONTRL_W,	/* E_SP00_MAT_CONTRL   */
	E_REG_P00_TBG_CNTRL_0_W,	/* E_SP00_TBG_CNTRL_0  */
	E_REG_P00_TBG_CNTRL_1_W,	/* E_SP00_TBG_CNTRL_1  */
	E_REG_P00_HVF_CNTRL_0_W,	/* E_SP00_HVF_CNTRL_0  */
	E_REG_P00_HVF_CNTRL_1_W,	/* E_SP00_HVF_CNTRL_1  */
	E_REG_P00_TIMER_H_W,	/* E_SP00_TIMER_H      */
	E_REG_P00_DEBUG_PROBE_W,	/* E_SP00_DEBUG_PROBE  */
	E_REG_P00_AIP_CLKSEL_W,	/* E_SP00_AIP_CLKSEL   */
	E_REG_P01_SC_VIDFORMAT_W,	/* E_SP01_SC_VIDFORMAT */
	E_REG_P01_SC_CNTRL_W,	/* E_SP01_SC_CNTRL     */
	E_REG_P01_TBG_CNTRL_0_W	/* E_SP01_TBG_CNTRL_0  */
#ifdef TMFL_HDCP_SUPPORT
	    , E_REG_P12_CTRL_W	/* E_SP12_CTRL    */
	    , E_REG_P12_BCAPS_W	/* E_SP12_BCAPS   */
#endif				/* TMFL_HDCP_SUPPORT */
};


/**
 * Table of registers to switch to low power (standby)

static tmHdmiTxRegMaskVal_t kPowerOff[] =
{
    {E_REG_P02_TEST2_RW,        E_MASKREG_P02_TEST2_pwd1v8,         1},
    {E_REG_P02_PLL_SCG1_RW,     E_MASKREG_P02_PLL_SCG1_scg_fdn,     1},
    {E_REG_P02_PLL_SERIAL_1_RW, E_MASKREG_P02_PLL_SERIAL_1_srl_fdn, 1},
    {E_REG_P02_PLL_DE_RW,       E_MASKREG_P02_PLL_DE_pllde_fdn,     1},
    {E_REG_P02_BUFFER_OUT_RW,   E_MASKREG_P02_BUFFER_OUT_srl_force, 2},
    {E_REG_P02_SEL_CLK_RW,      E_MASKREG_P02_SEL_CLK_ena_sc_clk,   0},
    {E_REG_P00_CCLK_ON_RW,      E_MASKREG_P00_CCLK_ON_cclk_on,      0},
    {0,0,0}
};
*/
/**
 * Table of registers to switch to normal power (resume)

static tmHdmiTxRegMaskVal_t kPowerOn[] =
{
    {E_REG_P02_TEST2_RW,        E_MASKREG_P02_TEST2_pwd1v8,         0},
    {E_REG_P02_PLL_SERIAL_1_RW, E_MASKREG_P02_PLL_SERIAL_1_srl_fdn, 0},
    {E_REG_P02_PLL_DE_RW,       E_MASKREG_P02_PLL_DE_pllde_fdn,     0},
    {E_REG_P02_PLL_SCG1_RW,     E_MASKREG_P02_PLL_SCG1_scg_fdn,     0},
    {E_REG_P02_SEL_CLK_RW,      E_MASKREG_P02_SEL_CLK_ena_sc_clk,   1},
    {E_REG_P02_BUFFER_OUT_RW,   E_MASKREG_P02_BUFFER_OUT_srl_force, 0},
    {E_REG_P00_TBG_CNTRL_0_W,   E_MASKREG_P00_TBG_CNTRL_0_sync_once,0},
    {E_REG_P00_CCLK_ON_RW,      E_MASKREG_P00_CCLK_ON_cclk_on,      1},
    {0,0,0}
};
*/

static tmbslHdmiTxCallbackInt_t kITCallbackPriority[HDMITX_CALLBACK_INT_NUM] = {
	HDMITX_CALLBACK_INT_R0,		/**< R0 interrupt                     */
	HDMITX_CALLBACK_INT_ENCRYPT,	/**< HDCP encryption switched off     */
	HDMITX_CALLBACK_INT_HPD,	/**< Transition on HPD input          */
	HDMITX_CALLBACK_INT_T0,		/**< HDCP state machine in state T0   */
	HDMITX_CALLBACK_INT_BCAPS,	/**< BCAPS available                  */
	HDMITX_CALLBACK_INT_BSTATUS,	/**< BSTATUS available                */
	HDMITX_CALLBACK_INT_SHA_1,	/**< sha-1(ksv,bstatus,m0)=V'         */
	HDMITX_CALLBACK_INT_PJ,		/**< pj=pj' check fails               */
	HDMITX_CALLBACK_INT_SW_INT,	/**< SW DEBUG interrupt               */
	HDMITX_CALLBACK_INT_RX_SENSE,	/**< RX SENSE interrupt               */
	HDMITX_CALLBACK_INT_EDID_BLK_READ,
					/**< EDID BLK READ interrupt          */
	HDMITX_CALLBACK_INT_VS_RPT,	/**< VS interrupt                     */
	HDMITX_CALLBACK_INT_PLL_LOCK	/** PLL LOCK not present on TDA9984   */
};



#ifdef TMFL_TDA9989_PIXEL_CLOCK_ON_DDC

UInt8 kndiv_im[] = {
	0,			/* HDMITX_VFMT_NO_CHANGE */
	4,			/* HDMITX_VFMT_01_640x480p_60Hz */
	4,			/* HDMITX_VFMT_02_720x480p_60Hz */
	4,			/* HDMITX_VFMT_03_720x480p_60Hz */
	12,			/* HDMITX_VFMT_04_1280x720p_60Hz */
	12,			/* HDMITX_VFMT_05_1920x1080i_60Hz */
	4,			/* HDMITX_VFMT_06_720x480i_60Hz */
	4,			/* HDMITX_VFMT_07_720x480i_60Hz */
	4,			/* HDMITX_VFMT_08_720x240p_60Hz */
	4,			/* HDMITX_VFMT_09_720x240p_60Hz */
	4,			/* HDMITX_VFMT_10_720x480i_60Hz */
	4,			/* HDMITX_VFMT_11_720x480i_60Hz */
	4,			/* HDMITX_VFMT_12_720x240p_60Hz */
	4,			/* HDMITX_VFMT_13_720x240p_60Hz */
	4,			/* HDMITX_VFMT_14_1440x480p_60Hz */
	4,			/* HDMITX_VFMT_15_1440x480p_60Hz */
	12,			/* HDMITX_VFMT_16_1920x1080p_60Hz */
	4,			/* HDMITX_VFMT_17_720x576p_50Hz */
	4,			/* HDMITX_VFMT_18_720x576p_50Hz */
	12,			/* HDMITX_VFMT_19_1280x720p_50Hz */
	12,			/* HDMITX_VFMT_20_1920x1080i_50Hz */
	4,			/* HDMITX_VFMT_21_720x576i_50Hz */
	4,			/* HDMITX_VFMT_22_720x576i_50Hz */
	4,			/* HDMITX_VFMT_23_720x288p_50Hz */
	4,			/* HDMITX_VFMT_24_720x288p_50Hz */
	4,			/* HDMITX_VFMT_25_720x576i_50Hz */
	4,			/* HDMITX_VFMT_26_720x576i_50Hz */
	4,			/* HDMITX_VFMT_27_720x288p_50Hz */
	4,			/* HDMITX_VFMT_28_720x288p_50Hz */
	4,			/* HDMITX_VFMT_29_1440x576p_50Hz */
	4,			/* HDMITX_VFMT_30_1440x576p_50Hz */
	12,			/* HDMITX_VFMT_31_1920x1080p_50Hz */
	12,			/* HDMITX_VFMT_32_1920x1080p_24Hz */
	12,			/* HDMITX_VFMT_33_1920x1080p_25Hz */
	12,			/* HDMITX_VFMT_34_1920x1080p_30Hz */

};

UInt8 kclk_div[] = {
	0,			/* HDMITX_VFMT_NO_CHANGE */
	44,			/* HDMITX_VFMT_01_640x480p_60Hz */
	44,			/* HDMITX_VFMT_02_720x480p_60Hz */
	44,			/* HDMITX_VFMT_03_720x480p_60Hz */
	44,			/* HDMITX_VFMT_04_1280x720p_60Hz */
	44,			/* HDMITX_VFMT_05_1920x1080i_60Hz */
	44,			/* HDMITX_VFMT_06_720x480i_60Hz */
	44,			/* HDMITX_VFMT_07_720x480i_60Hz */
	44,			/* HDMITX_VFMT_08_720x240p_60Hz */
	44,			/* HDMITX_VFMT_09_720x240p_60Hz */
	44,			/* HDMITX_VFMT_10_720x480i_60Hz */
	44,			/* HDMITX_VFMT_11_720x480i_60Hz */
	44,			/* HDMITX_VFMT_12_720x240p_60Hz */
	44,			/* HDMITX_VFMT_13_720x240p_60Hz */
	44,			/* HDMITX_VFMT_14_1440x480p_60Hz */
	44,			/* HDMITX_VFMT_15_1440x480p_60Hz */
	44,			/* HDMITX_VFMT_16_1920x1080p_60Hz */
	44,			/* HDMITX_VFMT_17_720x576p_50Hz */
	44,			/* HDMITX_VFMT_18_720x576p_50Hz */
	44,			/* HDMITX_VFMT_19_1280x720p_50Hz */
	44,			/* HDMITX_VFMT_20_1920x1080i_50Hz */
	44,			/* HDMITX_VFMT_21_720x576i_50Hz */
	44,			/* HDMITX_VFMT_22_720x576i_50Hz */
	44,			/* HDMITX_VFMT_23_720x288p_50Hz */
	44,			/* HDMITX_VFMT_24_720x288p_50Hz */
	44,			/* HDMITX_VFMT_25_720x576i_50Hz */
	44,			/* HDMITX_VFMT_26_720x576i_50Hz */
	44,			/* HDMITX_VFMT_27_720x288p_50Hz */
	44,			/* HDMITX_VFMT_28_720x288p_50Hz */
	44,			/* HDMITX_VFMT_29_1440x576p_50Hz */
	44,			/* HDMITX_VFMT_30_1440x576p_50Hz */
	44,			/* HDMITX_VFMT_31_1920x1080p_50Hz */
	44,			/* HDMITX_VFMT_32_1920x1080p_24Hz */
	44,			/* HDMITX_VFMT_33_1920x1080p_25Hz */
	44,			/* HDMITX_VFMT_34_1920x1080p_30Hz */
};

#endif				/* TMFL_TDA9989_PIXEL_CLOCK_ON_DDC */


/*============================================================================*/
/*                       FUNCTIONS DECLARATIONS                               */
/*============================================================================*/

/*============================================================================*/
/*                       VARIABLES DECLARATIONS                               */
/*============================================================================*/

#ifdef TMFL_HDCP_SUPPORT
static UInt32 sgBcapsCounter;
#endif				/* TMFL_HDCP_SUPPORT */

#define TDA19989_DDC_SPEED_FACTOR 39

static Bool gMiscInterruptHpdRxEnable = False;	/* Enable HPD and RX sense IT after */
					       /* first call done by init function */
static UInt8 int_level = 0xFF;
/*============================================================================*/
/*                       FUNCTION PROTOTYPES                                  */
/*============================================================================*/

/*============================================================================*/
/* tmbslTDA9989Deinit                                                          */
/*============================================================================*/
tmErrorCode_t tmbslTDA9989Deinit(tmUnitSelect_t txUnit) {
	tmHdmiTxobject_t *pDis;	/* Pointer to Device Instance Structure */
	tmErrorCode_t err;	/* Error code */
	UInt8 EnableModeMask = 0;	/* Local Variable */

	/* Check unit parameter and point to its object */
	err = checkUnitSetDis(txUnit, &pDis);
	RETIF(err != TM_OK, err)

	    /* patch to get successfull soft reset even if powerstate has been set to standby mode */
	    /*Write data in ENAMODS CEC Register */
	    EnableModeMask = 0x40;
	EnableModeMask |= E_MASKREG_CEC_ENAMODS_ena_hdmi;	/*Enable HDMI Mode */
	EnableModeMask &= ~E_MASKREG_CEC_ENAMODS_dis_fro;	/* Enable FRO  */
	err = setCECHwRegister(pDis, E_REG_CEC_ENAMODS_RW, EnableModeMask);
	RETIF_REG_FAIL(err)

	    /* Hold the device in reset to disable it */
	    err = setHwRegisterField(pDis, E_REG_P00_MAIN_CNTRL0_RW,
				     E_MASKREG_P00_MAIN_CNTRL0_sr, 1);
	RETIF_REG_FAIL(err)

	    /* patch to get successfull soft reset even if powerstate has been set to standby mode */
	    EnableModeMask &= ~E_MASKREG_CEC_ENAMODS_ena_hdmi;	/* Disable HDMI Mode */
	EnableModeMask &= ~E_MASKREG_CEC_ENAMODS_ena_rxs;	/* Reset RxSense Mode */
	EnableModeMask |= E_MASKREG_CEC_ENAMODS_dis_fro;	/* Disable FRO  */
	err = setCECHwRegister(pDis, E_REG_CEC_ENAMODS_RW, EnableModeMask);
	RETIF_REG_FAIL(err)

	    /* Clear the Initialized flag to destroy the device instance */
	    pDis->bInitialized = False;

	setState(pDis, EV_DEINIT);
	return TM_OK;
}

/*============================================================================*/
/* tmbslTDA9989HotPlugGetStatus                                                */
/*============================================================================*/
tmErrorCode_t tmbslTDA9989HotPlugGetStatus(tmUnitSelect_t txUnit, tmbslHdmiTxHotPlug_t *pHotPlugStatus, Bool client	/* Used to determine whether the request comes from the application */
    ) {
	tmHdmiTxobject_t *pDis;	/* Pointer to Device Instance Structure */
	tmErrorCode_t err;	/* Error code */
	UInt8 regVal;		/* Register value */

	/* Check unit parameter and point to its object */
	err = checkUnitSetDis(txUnit, &pDis);
	RETIF(err != TM_OK, err)

	    /* Check remaining parameters */
	    RETIF_BADPARAM(pHotPlugStatus == (tmbslHdmiTxHotPlug_t *) 0)

	    /* Read HPD RXS level */
	    err = getCECHwRegister(pDis, E_REG_CEC_RXSHPDLEV_R, &regVal);
	RETIF(err != TM_OK, err)

	    /* Read Hot Plug input status to know the actual level that caused the interrupt */
	    if (client) {
		*pHotPlugStatus = (regVal & E_MASKREG_CEC_RXSHPDLEV_hpd_level) ?
		    HDMITX_HOTPLUG_ACTIVE : HDMITX_HOTPLUG_INACTIVE;
	} else {

		*pHotPlugStatus = pDis->hotPlugStatus;

	}
	return TM_OK;
}

/*============================================================================*/
/* tmbslTDA9989RxSenseGetStatus                                                */
/*============================================================================*/
tmErrorCode_t tmbslTDA9989RxSenseGetStatus(tmUnitSelect_t txUnit, tmbslHdmiTxRxSense_t *pRxSenseStatus, Bool client	/* Used to determine whether the request comes from the application */
    ) {
	tmHdmiTxobject_t *pDis;	/* Pointer to Device Instance Structure */
	tmErrorCode_t err;	/* Error code */
	UInt8 regVal;		/* Register value */

	/* Check unit parameter and point to its object */
	err = checkUnitSetDis(txUnit, &pDis);
	RETIF(err != TM_OK, err)

	    /* Check remaining parameters */
	    RETIF_BADPARAM(pRxSenseStatus == (tmbslHdmiTxRxSense_t *) 0)


	    /* Read HPD RXS level */
	    err = getCECHwRegister(pDis, E_REG_CEC_RXSHPDLEV_R, &regVal);
	RETIF(err != TM_OK, err)


	    /*Read RXS_FIL status to know the actual level that caused the interrupt */
	    if (client) {
		*pRxSenseStatus = (regVal & E_MASKREG_CEC_RXSHPDLEV_rxs_level) ?
		    HDMITX_RX_SENSE_ACTIVE : HDMITX_RX_SENSE_INACTIVE;
	} else {
		*pRxSenseStatus = pDis->rxSenseStatus;
	}

	return TM_OK;
}

/*============================================================================*/
/* tmbslTDA9989HwGetRegisters                                                  */
/*============================================================================*/
tmErrorCode_t
    tmbslTDA9989HwGetRegisters
    (tmUnitSelect_t txUnit, Int regPage, Int regAddr, UInt8 *pRegBuf, Int nRegs) {
	tmHdmiTxobject_t *pDis;	/* Pointer to Device Instance Structure */
	tmErrorCode_t err;	/* Error code */
	Int i;			/* Loop index */
	UInt8 newRegPage;	/* The register's new page number */
	UInt8 regShad;		/* Index to the register's shadow copy */
	UInt16 regShadPageAddr;	/* Packed shadowindex/page/address */
	tmbslHdmiTxSysArgs_t sysArgs;	/* Arguments passed to system function     */

	/* Check unit parameter and point to its object */
	err = checkUnitSetDis(txUnit, &pDis);
	RETIF(err != TM_OK, err)

	    /* Check remaining parameters */
	    RETIF_BADPARAM((regPage < kPageIndexToPage[E_PAGE_00])
			   || ((regPage > kPageIndexToPage[E_PAGE_02])
			       && (regPage < kPageIndexToPage[E_PAGE_09]))
			   || ((regPage > kPageIndexToPage[E_PAGE_09])
			       && (regPage < kPageIndexToPage[E_PAGE_11]))
			   || (regPage > kPageIndexToPage[E_PAGE_12]))
	    RETIF_BADPARAM((regAddr < E_REG_MIN_ADR) || (regAddr >= E_REG_CURPAGE_ADR_W))
	    RETIF_BADPARAM(pRegBuf == (pUInt8) 0)
	    RETIF_BADPARAM((nRegs < 1) || ((nRegs + regAddr) > E_REG_CURPAGE_ADR_W))

	    /* Set page register if required */
	    newRegPage = (UInt8) regPage;
	if (pDis->curRegPage != newRegPage) {
		/* All non-OK results are errors */
		sysArgs.slaveAddr = pDis->uHwAddress;
		sysArgs.firstRegister = E_REG_CURPAGE_ADR_W;
		sysArgs.lenData = 1;
		sysArgs.pData = &newRegPage;
		err = pDis->sysFuncWrite(&sysArgs);
		RETIF(err != TM_OK, TMBSL_ERR_HDMI_I2C_WRITE)
		    pDis->curRegPage = newRegPage;
	}

	/* Read each register in the range. nRegs must start at 1 or more */
	for (; nRegs > 0; pRegBuf++, regAddr++, nRegs--) {
		/* Find shadow register index.
		 * This loop is not very efficient, but it is assumed that this API
		 * will not be used often. The alternative is to use a huge sparse
		 * array indexed by page and address and containing the shadow index.
		 */
		regShad = E_SNONE;
		for (i = 0; i < E_SNUM; i++) {
			/* Check lookup table for match with page and address */
			regShadPageAddr = kShadowReg[i];
			if ((SPA2PAGE(regShadPageAddr) == newRegPage)
			    && (SPA2ADDR(regShadPageAddr) == regAddr)) {
				/* Found page and address - look up the shadow index */
				regShad = SPA2SHAD(regShadPageAddr);
				break;
			}
		}
		/* Read the shadow register if available, as device registers that
		 * are shadowed cannot be read directly */
		if (regShad != E_SNONE) {
			*pRegBuf = pDis->shadowReg[regShad];
		} else {
			/* Read the device register - all non-OK results are errors.
			 * Note that some non-shadowed registers are also write-only and
			 * cannot be read. */
			sysArgs.slaveAddr = pDis->uHwAddress;
			sysArgs.firstRegister = (UInt8) regAddr;
			sysArgs.lenData = 1;
			sysArgs.pData = pRegBuf;
			err = pDis->sysFuncRead(&sysArgs);
			RETIF(err != TM_OK, TMBSL_ERR_HDMI_I2C_READ)
		}
	}

	return TM_OK;
}

/*============================================================================*/
/* tmbslTDA9989HwGetVersion                                                    */
/*============================================================================*/

tmErrorCode_t tmbslTDA9989HwGetVersion(tmUnitSelect_t txUnit, pUInt8 pHwVersion) {
	tmHdmiTxobject_t *pDis;	/* Pointer to Device Instance Structure */
	tmErrorCode_t err;	/* Error code */
	UInt8 regVal;

	/* Check unit parameter and point to its object */
	err = checkUnitSetDis(txUnit, &pDis);
	RETIF(err != TM_OK, err)

	    /* Check remaining parameters */
	    RETIF_BADPARAM(pHwVersion == (pUInt8) 0)

	    /* Get MSB version Value */
	    err = getHwRegister(pDis, E_REG_P00_VERSION_MSB_RW, &regVal);
	RETIF(err != TM_OK, err)

	    switch (regVal) {
	case 0x01:
		*pHwVersion = (UInt8) (BSLHDMITX_TDA9989);
		break;
	case 0x02:
		*pHwVersion = (UInt8) (BSLHDMITX_TDA19989);
		break;
	case 0x03:
		*pHwVersion = (UInt8) (BSLHDMITX_TDA19988);
		break;
	default:
		*pHwVersion = (UInt8) (BSLHDMITX_UNKNOWN);
		break;
	}

	return TM_OK;
}


/*============================================================================*/
/* tmbslTDA9989HwHandleInterrupt                                               */
/* RETIF_REG_FAIL NOT USED HERE AS ALL ERRORS SHOULD BE TRAPPED IN ALL BUILDS */
/*============================================================================*/
tmErrorCode_t tmbslTDA9989HwHandleInterrupt(tmUnitSelect_t txUnit) {
	tmHdmiTxobject_t *pDis;	/* Pointer to Device Instance Structure */
	tmErrorCode_t err;	/* Error code */
	UInt8 regVal;		/* Register value */
	UInt8 regVal1;		/* Register value */
	UInt16 fInterruptStatus;	/* Interrupt flags */
	UInt16 fInterruptMask;	/* Mask to test each interrupt bit */
	tmbslHdmiTxRxSense_t newRxs_fil;	/* Latest copy of rx_sense */
	Int i;			/* Loop counter */
	tmbslHdmiTxHotPlug_t newHpdIn;	/* Latest copy of hpd input */
	Bool sendEdidCallback;
	Bool hpdOrRxsLevelHasChanged = False;

	/* Check unit parameter and point to its object */
	err = checkUnitSetDis(txUnit, &pDis);
	RETIF(err != TM_OK, err)

	    fInterruptStatus = 0;
	sendEdidCallback = False;



	/* Read HPD RXS int status */
	err = getCECHwRegister(pDis, E_REG_CEC_RXSHPDINT_R, &regVal);
	RETIF(err != TM_OK, err);

	/* Read HPD RXS level */
	err = getCECHwRegister(pDis, E_REG_CEC_RXSHPDLEV_R, &regVal1);
	RETIF(err != TM_OK, err);

	if (int_level != 0xFF) {	/* init should be done */
		/* check multi-transition */
		if ((regVal == 0) && (int_level != regVal1)) {
#ifdef TMFL_LINUX_OS_KERNEL_DRIVER
			pr_debug("HDMI Int multi-transition\n");
#endif
			err = setCECHwRegister(pDis, E_REG_CEC_RXSHPDINTENA_RW, 0x00);
			err += setCECHwRegister(pDis, E_REG_CEC_RXSHPDINTENA_RW,
						E_MASKREG_CEC_RXSHPDINTENA_ena_rxs_int |
						E_MASKREG_CEC_RXSHPDINTENA_ena_hpd_int);
			err += getCECHwRegister(pDis, E_REG_CEC_RXSHPDLEV_R, &regVal1);
			RETIF(err != TM_OK, err)
		}
	}
	int_level = regVal1;

	/* Read Hot Plug input status to know the actual level that caused the interrupt */
	newHpdIn = (regVal1 & E_MASKREG_CEC_RXSHPDLEV_hpd_level) ?
	    HDMITX_HOTPLUG_ACTIVE : HDMITX_HOTPLUG_INACTIVE;

	/*Read RXS_FIL status to know the actual level that caused the interrupt */
	newRxs_fil = (regVal1 & E_MASKREG_CEC_RXSHPDLEV_rxs_level) ?
	    HDMITX_RX_SENSE_ACTIVE : HDMITX_RX_SENSE_INACTIVE;

	/*Fill fInterruptStatus with HPD Interrupt flag */

	if (newHpdIn != pDis->hotPlugStatus) {
		fInterruptStatus = fInterruptStatus | (1 << HDMITX_CALLBACK_INT_HPD);
		/* Yes: save new HPD level */
		pDis->hotPlugStatus = newHpdIn;
		hpdOrRxsLevelHasChanged = True;
	}

	/*Fill fInterruptStatus with RX Sense Interrupt flag */
	if (newRxs_fil != pDis->rxSenseStatus) {
		fInterruptStatus = fInterruptStatus | (1 << HDMITX_CALLBACK_INT_RX_SENSE);
		/* Yes: save new rxSense level */
		pDis->rxSenseStatus = newRxs_fil;
		hpdOrRxsLevelHasChanged = True;
	}



	/* is it  HDMI interrupt ? */
	err = getCECHwRegister(pDis, E_REG_CEC_INTERRUPTSTATUS_R, &regVal);
	RETIF(err != TM_OK, err)

	    /* there is no HDMI int to handle, give up */
	    if ((regVal & E_MASKREG_CEC_INTERRUPTSTATUS_hdmi_int) == 0x00) {

		if (hpdOrRxsLevelHasChanged == True) {
		} else {
			return TM_OK;
		}
	}



    /************************************************************************************************/
    /***********************************End of Temporary code****************************************/
    /************************************************************************************************/

	/* Do only if HDMI is On */
	if (pDis->ePowerState == tmPowerOn) {
		/* Read the main interrupt flags register to determine the source(s)
		 * of the interrupt. (The device resets these register flags after they
		 * have been read.)
		 */
		err = getHwRegister(pDis, E_REG_P00_INT_FLAGS_0_RW, &regVal);
		RETIF(err != TM_OK, err)
#ifdef TMFL_HDCP_SUPPORT
		    /* encrypt */
		    if ((regVal & E_MASKREG_P00_INT_FLAGS_0_encrypt) != 0) {
			fInterruptStatus = fInterruptStatus | (1 << HDMITX_CALLBACK_INT_ENCRYPT);
		}
#endif				/* TMFL_HDCP_SUPPORT */

		/* get TO interrupt Flag */
		if ((regVal & E_MASKREG_P00_INT_FLAGS_0_t0) != 0) {
			fInterruptStatus = fInterruptStatus | (1 << HDMITX_CALLBACK_INT_T0);
		}
#ifdef TMFL_HDCP_SUPPORT
		/* bcaps */
		if ((regVal & E_MASKREG_P00_INT_FLAGS_0_bcaps) != 0) {
			fInterruptStatus = fInterruptStatus | (1 << HDMITX_CALLBACK_INT_BCAPS);

			/* TDA19989 N1 only */
			if (pDis->uDeviceVersion == E_DEV_VERSION_TDA19989) {

				/* WA: HDCP ATC Test 1B_03 */

				sgBcapsCounter++;

				if (sgBcapsCounter == 49) {
					sgBcapsCounter = 0;
					/* force a T0 interrupt */
					fInterruptStatus =
					    fInterruptStatus | (1 << HDMITX_CALLBACK_INT_T0);
				}

			}
			/* TDA19989 N1 only */
		}

		/* bstatus */
		if ((regVal & E_MASKREG_P00_INT_FLAGS_0_bstatus) != 0) {
			fInterruptStatus = fInterruptStatus | (1 << HDMITX_CALLBACK_INT_BSTATUS);

			/* TDA19989 N1 only */
			if (pDis->uDeviceVersion == E_DEV_VERSION_TDA19989) {

				/* WA: HDCP ATC Test 1B_03 */
				sgBcapsCounter = 0;

			}
			/* TDA19989 N1 only */
		}

		/* sha_1 */
		if ((regVal & E_MASKREG_P00_INT_FLAGS_0_sha_1) != 0) {
			fInterruptStatus = fInterruptStatus | (1 << HDMITX_CALLBACK_INT_SHA_1);
		}

		/* pj */
		if ((regVal & E_MASKREG_P00_INT_FLAGS_0_pj) != 0) {
			fInterruptStatus = fInterruptStatus | (1 << HDMITX_CALLBACK_INT_PJ);
		}

		/* r0 */
		if ((regVal & E_MASKREG_P00_INT_FLAGS_0_r0) != 0) {
			fInterruptStatus = fInterruptStatus | (1 << HDMITX_CALLBACK_INT_R0);

			/* TDA19989 N1 only */
			if (pDis->uDeviceVersion == E_DEV_VERSION_TDA19989) {

				/* WA: HDCP ATC Test 1B_03 */
				sgBcapsCounter = 0;

			}
			/* TDA19989 N1 only */
		}
#endif				/* TMFL_HDCP_SUPPORT */


		err = getHwRegister(pDis, E_REG_P00_INT_FLAGS_1_RW, &regVal);
		RETIF(err != TM_OK, err)


		    /* Read the software interrupt flag */
		    if ((regVal & E_MASKREG_P00_INT_FLAGS_1_sw_int) != 0) {
			fInterruptStatus = fInterruptStatus | (1 << HDMITX_CALLBACK_INT_SW_INT);
		}

		/* Read the VS_rpt interrupt flag */
		if (((pDis->InterruptsEnable & E_MASKREG_P00_INT_FLAGS_1_vs_rpt) != 0) &&
		    ((regVal & E_MASKREG_P00_INT_FLAGS_1_vs_rpt) != 0)
		    ) {
			fInterruptStatus = fInterruptStatus | (1 << HDMITX_CALLBACK_INT_VS_RPT);
		}

		/* Read INT_FLAGS_2 interrupt flag register.
		 *(The device resets these register flags after they
		 * have been read.) */
		err = getHwRegister(pDis, E_REG_P00_INT_FLAGS_2_RW, &regVal);
		RETIF(err != TM_OK, err)

		    /* Has the EDID_blk_rd interrupt occurs */
		    if ((regVal & E_MASKREG_P00_INT_FLAGS_2_edid_blk_rd) != 0) {
			fInterruptStatus =
			    fInterruptStatus | (1 << HDMITX_CALLBACK_INT_EDID_BLK_READ);
		}
	}


	/* Handle the HPD Interrupt */
	if ((fInterruptStatus & (1 << HDMITX_CALLBACK_INT_HPD)) != 0) {
		/* Callback disable on first tmbslTDA9989HwHandleInterrupt call */
		if (gMiscInterruptHpdRxEnable) {
			/* Reset EDID status */
			err = ClearEdidRequest(txUnit);

			/* Reset all simultaneous HDCP interrupts on hot plug,
			 * preserving only the high-priority hpd interrupt rx_sense and sw interrupt for debug*/
			fInterruptStatus &= (1 << HDMITX_CALLBACK_INT_HPD) |
			    (1 << HDMITX_CALLBACK_INT_RX_SENSE) | (1 << HDMITX_CALLBACK_INT_SW_INT);

			if (pDis->ePowerState == tmPowerOn) {
				if ((pDis->hotPlugStatus == HDMITX_HOTPLUG_ACTIVE)) {

					/* TDA19989 N1 only */
					if (pDis->uDeviceVersion == E_DEV_VERSION_TDA19989) {

						err = tmbslTDA9989Reset(txUnit);
						RETIF(err != TM_OK, err)

						    err = hotPlugRestore(txUnit);
						RETIF(err != TM_OK, err)

					}
					/* TDA19989 N1 only */
					else {	/* TDA19989 N2 */

						HDCP_F2;
					}

#ifdef TMFL_TDA9989_PIXEL_CLOCK_ON_DDC

					err = tmbslTDA9989Reset(txUnit);
					RETIF(err != TM_OK, err)

					    err = hotPlugRestore(txUnit);
					RETIF(err != TM_OK, err)
#endif				/* TMFL_TDA9989_PIXEL_CLOCK_ON_DDC */
					    setState(pDis, EV_PLUGGEDIN);
				} else {
					setState(pDis, EV_UNPLUGGED);
				}
			}
		}
	} else {
		/* Clear HPD status if level has not changed */
		fInterruptStatus &= ~(1 << HDMITX_CALLBACK_INT_HPD);

		if (fInterruptStatus & (1 << HDMITX_CALLBACK_INT_EDID_BLK_READ)) {
			err = EdidBlockAvailable(txUnit, &sendEdidCallback);
			RETIF(err != TM_OK, err)
			    if (sendEdidCallback == False) {
				/* Read EDID not finished clear callback */
				fInterruptStatus &= ~(1 << HDMITX_CALLBACK_INT_EDID_BLK_READ);
			} else {
#ifdef TMFL_TDA9989_PIXEL_CLOCK_ON_DDC

				if ((pDis->vinFmt == HDMITX_VFMT_16_1920x1080p_60Hz)
				    || (pDis->vinFmt == HDMITX_VFMT_31_1920x1080p_50Hz)) {

					err = setHwRegisterField(pDis,
								 E_REG_P02_PLL_SERIAL_3_RW,
								 E_MASKREG_P02_PLL_SERIAL_3_srl_ccir,
								 0x00);
					RETIF_REG_FAIL(err)
				}
#endif				/* TMFL_TDA9989_PIXEL_CLOCK_ON_DDC */
			}


		}
	}

	/*Handle RxSense Interrupt */
	if ((fInterruptStatus & (1 << HDMITX_CALLBACK_INT_RX_SENSE)) != 0) {
		/* Callback disable on first tmbslTDA9989HwHandleInterrupt call */
		if (gMiscInterruptHpdRxEnable) {


			fInterruptStatus &= (1 << HDMITX_CALLBACK_INT_HPD) |
			    (1 << HDMITX_CALLBACK_INT_RX_SENSE) | (1 << HDMITX_CALLBACK_INT_SW_INT);

			if (pDis->rxSenseStatus == HDMITX_RX_SENSE_ACTIVE) {
				setState(pDis, EV_SINKON);
			} else {
				setState(pDis, EV_SINKOFF);
			}
		}
	} else {
		/* Clear RX_sense IT if level has not changed */
		fInterruptStatus &= ~(1 << HDMITX_CALLBACK_INT_RX_SENSE);
	}

	/* Ignore other simultaneous HDCP interrupts if T0 interrupt,
	 * preserving any hpd interrupt */

	if (fInterruptStatus & (1 << HDMITX_CALLBACK_INT_T0)) {
		if (pDis->EdidReadStarted) {

#ifdef TMFL_HDCP_SUPPORT
			err = getHwRegister(pDis, E_REG_P12_TX0_RW, &regVal);
			RETIF(err != TM_OK, err)

			    /* EDID read failure */
			    if ((regVal & E_MASKREG_P12_TX0_sr_hdcp) != 0) {

#endif				/* TMFL_HDCP_SUPPORT */


				/* Reset EDID status */
				err = ClearEdidRequest(txUnit);
				RETIF(err != TM_OK, err)

				    /* enable EDID callback */
				    fInterruptStatus =
				    (UInt16) (fInterruptStatus & (~(1 << HDMITX_CALLBACK_INT_T0)));
				fInterruptStatus =
				    fInterruptStatus | (1 << HDMITX_CALLBACK_INT_EDID_BLK_READ);

#ifdef TMFL_HDCP_SUPPORT
			}
#endif				/* TMFL_HDCP_SUPPORT */

		} else {
			fInterruptStatus &= ((1 << HDMITX_CALLBACK_INT_HPD)
					     | (1 << HDMITX_CALLBACK_INT_T0)
					     | (1 << HDMITX_CALLBACK_INT_RX_SENSE)
					     | (1 << HDMITX_CALLBACK_INT_SW_INT)
			    );
		}
	}

	HDCP_F3;

	/* For each interrupt flag that is set, check the corresponding registered
	 * callback function pointer in the Device Instance Structure
	 * funcIntCallbacks array.
	 */
	fInterruptMask = 1;
	for (i = 0; i < HDMITX_CALLBACK_INT_NUM; i++) {
		if (i != HDMITX_CALLBACK_INT_PLL_LOCK) {	/* PLL LOCK not present on TDA9989 */

			fInterruptMask = 1;
			fInterruptMask = fInterruptMask << ((UInt16) kITCallbackPriority[i]);

			if (fInterruptStatus & fInterruptMask) {
				/* IF a registered callback pointer is non-null THEN call it. */
				if (pDis->funcIntCallbacks[kITCallbackPriority[i]] !=
				    (ptmbslHdmiTxCallback_t) 0) {
					pDis->funcIntCallbacks[kITCallbackPriority[i]] (txUnit);
				}
			}

		}
	}
	return TM_OK;
}

/*============================================================================*/
/* tmbslTDA9989FlagSwInt                                                       */
/* Use only for debug to flag the software debug interrupt                    */
/*============================================================================*/
tmErrorCode_t tmbslTDA9989FlagSwInt(tmUnitSelect_t txUnit, UInt32 uSwInt) {
	tmHdmiTxobject_t *pDis;	/* Pointer to Device Instance Structure */
	tmErrorCode_t err;	/* Error code */

	DUMMY_ACCESS(uSwInt);

	/* Check unit parameter and point to its object */
	err = checkUnitSetDis(txUnit, &pDis);
	RETIF(err != TM_OK, err)

	    err = setHwRegister(pDis, E_REG_P00_SW_INT_W, E_MASKREG_P00_SW_INT_sw_int);


	return err;
}

/*============================================================================*/
/* tmbslTDA9989HwSetRegisters                                                  */
/*============================================================================*/

tmErrorCode_t
    tmbslTDA9989HwSetRegisters
    (tmUnitSelect_t txUnit, Int regPage, Int regAddr, UInt8 *pRegBuf, Int nRegs) {
	tmHdmiTxobject_t *pDis;	/* Pointer to Device Instance Structure */
	tmErrorCode_t err;	/* Error code */
	Int i;			/* Loop index */
	UInt8 newRegPage;	/* The register's new page number */
	UInt8 regShad;		/* Index to the register's shadow copy */
	UInt16 regShadPageAddr;	/* Packed shadowindex/page/address */
	tmbslHdmiTxSysArgs_t sysArgs;	/* Arguments passed to system function */

	/* Check unit parameter and point to its object */
	err = checkUnitSetDis(txUnit, &pDis);
	RETIF(err != TM_OK, err)

	    /* Check remaining parameters */
	    RETIF_BADPARAM((regPage < kPageIndexToPage[E_PAGE_00])
			   || ((regPage > kPageIndexToPage[E_PAGE_02])
			       && (regPage < kPageIndexToPage[E_PAGE_11]))
			   || (regPage > kPageIndexToPage[E_PAGE_12]))
	    RETIF_BADPARAM((regAddr < E_REG_MIN_ADR) || (regAddr >= E_REG_CURPAGE_ADR_W))
	    RETIF_BADPARAM(pRegBuf == (pUInt8) 0)
	    RETIF_BADPARAM((nRegs < 0) || ((nRegs + regAddr) > E_REG_CURPAGE_ADR_W))

	    /* Set page register if required */
	    newRegPage = (UInt8) regPage;
	if (pDis->curRegPage != newRegPage) {
		/* All non-OK results are errors */
		sysArgs.slaveAddr = pDis->uHwAddress;
		sysArgs.firstRegister = E_REG_CURPAGE_ADR_W;
		sysArgs.lenData = 1;
		sysArgs.pData = &newRegPage;
		err = pDis->sysFuncWrite(&sysArgs);
		RETIF(err != TM_OK, TMBSL_ERR_HDMI_I2C_WRITE)
		    pDis->curRegPage = newRegPage;
	}

	/* Write each register in the range. nRegs = 0 is ok, to allow only
	 * the page register to be written if required (above)
	 */
	for (; nRegs > 0; pRegBuf++, regAddr++, nRegs--) {
		/* Find shadow register index.
		 * This loop is not very efficient, but it is assumed that this API
		 * will not be used often. The alternative is to use a huge sparse
		 * array indexed by page and address and containing the shadow index.
		 */
		for (i = 0; i < E_SNUM; i++) {
			/* Check lookup table for match with page and address */
			regShadPageAddr = kShadowReg[i];
			if ((SPA2PAGE(regShadPageAddr) == newRegPage)
			    && (SPA2ADDR(regShadPageAddr) == regAddr)) {
				/* Found index - write the shadow register */
				regShad = SPA2SHAD(regShadPageAddr);
				pDis->shadowReg[regShad] = *pRegBuf;
				break;
			}
		}
		/* Write the device register - all non-OK results are errors */
		sysArgs.slaveAddr = pDis->uHwAddress;
		sysArgs.firstRegister = (UInt8) regAddr;
		sysArgs.lenData = 1;
		sysArgs.pData = pRegBuf;
		err = pDis->sysFuncWrite(&sysArgs);
		RETIF(err != TM_OK, TMBSL_ERR_HDMI_I2C_WRITE)
	}

	return TM_OK;
}


/*============================================================================*/
/* tmbslTDA9989HwStartup                                                       */
/*============================================================================*/
void
 tmbslTDA9989HwStartup(void
    ) {
	pr_debug("%s\n", __func__);
	/* Reset device instance data for when compiler doesn't do it */
	lmemset(&gHdmiTxInstance, 0, sizeof(gHdmiTxInstance));
}

/*============================================================================*/
/* tmbslTDA9989Init                                                            */
/* RETIF_REG_FAIL NOT USED HERE AS ALL ERRORS SHOULD BE TRAPPED IN ALL BUILDS */
/*============================================================================*/
tmErrorCode_t
    tmbslTDA9989Init
    (tmUnitSelect_t txUnit,
     UInt8 uHwAddress,
     ptmbslHdmiTxSysFunc_t sysFuncWrite,
     ptmbslHdmiTxSysFunc_t sysFuncRead,
     ptmbslHdmiTxSysFuncEdid_t sysFuncEdidRead,
     ptmbslHdmiTxSysFuncTimer_t sysFuncTimer,
     tmbslHdmiTxCallbackList_t *funcIntCallbacks,
     Bool bEdidAltAddr, tmbslHdmiTxVidFmt_t vinFmt, tmbslHdmiTxPixRate_t pixRate) {
	tmHdmiTxobject_t *pDis;	/* Pointer to Device Instance Structure */
	tmErrorCode_t err;	/* Error code */
	Int i;			/* Loop index */
	Bool bFound;		/* T=found, F=not found */
	UInt8 EnableIntMask = 0;	/* Mask used to enable HPD and RX Sense interrupt */
	UInt8 EnableModeMask;	/* Mask used to Set HDMI and RxSense modes */
	UInt16 val16Bits;	/* Value on 16 bit */
	UInt8 regVal;		/* Register value */

	pr_debug("%s\n", __func__);
	/* Check unit parameter and point to its object */
	RETIF(txUnit < tmUnit0, TMBSL_ERR_HDMI_BAD_UNIT_NUMBER)
	    RETIF(txUnit >= HDMITX_UNITS_MAX, TMBSL_ERR_HDMI_BAD_UNIT_NUMBER)
	    pDis = &gHdmiTxInstance[txUnit];

	/* IF the bInitialized flag is set THEN return (only Init does this) */
	RETIF(pDis->bInitialized, TMBSL_ERR_HDMI_INIT_FAILED)

	    /* Check remaining parameters */
	    RETIF_BADPARAM(uHwAddress < HDMITX_SLAVE_ADDRESS_MIN)
	    RETIF_BADPARAM(uHwAddress > HDMITX_SLAVE_ADDRESS_MAX)
	    RETIF_BADPARAM(sysFuncWrite == (ptmbslHdmiTxSysFunc_t) 0)
	    RETIF_BADPARAM(sysFuncRead == (ptmbslHdmiTxSysFunc_t) 0)
	    /*RETIF_BADPARAM(sysFuncEdidRead == (ptmbslHdmiTxSysFuncEdid_t)0) *//*Previously on TDA9983 */
	    /*RETIF_BADPARAM(sysFuncTimer == (ptmbslHdmiTxSysFuncTimer_t)0) */
	    RETIF_BADPARAM((bEdidAltAddr != True) && (bEdidAltAddr != False))
	    RETIF_BADPARAM(!IS_VALID_FMT(vinFmt))
	    RETIF_BADPARAM(pixRate >= HDMITX_PIXRATE_INVALID)

	    /* Set all Device Instance Structure members to default values */
	    lmemcpy(pDis, &kHdmiTxInstanceDefault, sizeof(*pDis));

	/* Copy txUnit, uHwAddress, sysFuncWrite and sysFuncRead values to
	 * the defaulted Device Instance Structure BEFORE FIRST DEVICE ACCESS.
	 */
	pDis->txUnit = txUnit;
#ifdef UNIT_TEST
	/* Unit test build can't support 127 device sets of dummy registers, so use
	 * smaller range instead, indexed by unit number not I2C address */
	pDis->uHwAddress = (UInt8) txUnit;
#else
	/* Store actual I2C address */
	pDis->uHwAddress = uHwAddress;
#endif
	pDis->sysFuncWrite = sysFuncWrite;
	pDis->sysFuncRead = sysFuncRead;
	pDis->sysFuncEdidRead = sysFuncEdidRead;
	pDis->sysFuncTimer = sysFuncTimer;

	/* IF the funcIntCallbacks array pointer is defined
	 * THEN for each funcIntCallbacks pointer that is not null:
	 * - Copy the pointer to the Device Instance Structure
	 *   funcIntCallbacks array.
	 */

	for (i = 0; i < HDMITX_CALLBACK_INT_NUM; i++) {
		if ((funcIntCallbacks != (tmbslHdmiTxCallbackList_t *) 0)
		    && (funcIntCallbacks->funcCallback[i] != (ptmbslHdmiTxCallback_t) 0)) {
			pDis->funcIntCallbacks[i] = funcIntCallbacks->funcCallback[i];
		} else {
			pDis->funcIntCallbacks[i] = (ptmbslHdmiTxCallback_t) 0;
		}
	}

	/* Set the EDID alternate address flag if needed */
	pDis->bEdidAlternateAddr = bEdidAltAddr;

/* *****************************************************************************************// */
/* *****************************************************************************************// */
/* **********************Enable HDMI and RxSense************************/// */

	/* reset ENAMODS */
	err = setCECHwRegister(pDis, E_REG_CEC_ENAMODS_RW, 0x40);
	RETIF_REG_FAIL(err)





	    /*Read data out of ENAMODS CEC Register */
	    err = getCECHwRegister(pDis, E_REG_CEC_ENAMODS_RW, &EnableModeMask);

	/*Enable required modes */
	EnableModeMask |= E_MASKREG_CEC_ENAMODS_ena_hdmi;	/*Enable HDMI Mode */
	EnableModeMask |= E_MASKREG_CEC_ENAMODS_ena_rxs;	/*Enable RxSense Mode */
	EnableModeMask &= ~E_MASKREG_CEC_ENAMODS_dis_fro;	/* Enable FRO  */

	/*Write data in ENAMODS CEC Register */
	err = setCECHwRegister(pDis, E_REG_CEC_ENAMODS_RW, EnableModeMask);

	pDis->ePowerState = (tmbslHdmiTxPowerState_t) tmPowerOn;

	/* Set the bInitialized flag to enable other APIs */
	pDis->bInitialized = True;

	/* Reset the device */
	err = tmbslTDA9989Reset(txUnit);
	RETIF(err != TM_OK, err)
/* ***************************************************************************************// */
/* ****************Get Device Version and Capabilities************************************// */
	    /* Read the device version register to uDeviceVersion in the
	     * Device Instance Structure
	     */
	    err = getHwRegister(pDis, E_REG_P00_VERSION_R, &regVal);
	RETIF(err != TM_OK, err)

	    /* Copy N4 features bits to DIS */
	    pDis->uDeviceFeatures = regVal &
	    (E_MASKREG_P00_VERSION_not_h | E_MASKREG_P00_VERSION_not_s);

	pDis->uDeviceVersion = regVal;

	/* Get MSB version Value */
	err = getHwRegister(pDis, E_REG_P00_VERSION_MSB_RW, &regVal);
	RETIF(err != TM_OK, err)

	    /* Build Device Version Info */
	    val16Bits = regVal;
	pDis->uDeviceVersion = pDis->uDeviceVersion | (val16Bits << 8);
	val16Bits = pDis->uDeviceFeatures;
	pDis->uDeviceVersion &= ~val16Bits;

	if (pDis->uDeviceVersion != E_DEV_VERSION_LIST_END) {
		/* Search for the device version in the Supported Version
		 * List in the Device Instance Structure.
		 */
		for (i = 0, bFound = False; i < E_DEV_VERSION_LIST_NUM; i++) {
			if (pDis->uDeviceVersion == pDis->uSupportedVersions[i]) {
				bFound = True;
			}
		}
		if (bFound == False) {
			/* IF the device version is not found in the Supported Version List THEN
			 * this driver component is not compatible with the device.*/
			err = tmbslTDA9989Deinit(txUnit);
			RETIF(err != TM_OK, err)
			    return TMBSL_ERR_HDMI_COMPATIBILITY;
		}
	} else {
		/* Quit if version reads zero */
		err = tmbslTDA9989Deinit(txUnit);
		RETIF(err != TM_OK, err)
		    return TMBSL_ERR_HDMI_COMPATIBILITY;
	}

/***************************************************************************************/
/************Set the BIAS_tmds Value (general control for Analogu module)***************/
	regVal = HDMI_TX_VSWING_VALUE;

	err = setHwRegister(pDis, E_REG_P02_ANA_GENERAL_RW, regVal);
	RETIF(err != TM_OK, err)

/*****************************************************************************************/
/*****************************************************************************************/
	    /* Set the PLL before resetting the device */
	    /* PLL registers common configuration */
	    err = setHwRegisterFieldTable(pDis, &kCommonPllCfg[0]);
	RETIF_REG_FAIL(err)

	    /*Reset 656_Alt bit in VIP_CONTROL_4 Register */
	    err =
	    setHwRegisterField(pDis, E_REG_P00_VIP_CNTRL_4_W, E_MASKREG_P00_VIP_CNTRL_4_656_alt, 0);

	switch (vinFmt) {
		/* 480i or 576i video input format */
	case HDMITX_VFMT_06_720x480i_60Hz:
	case HDMITX_VFMT_07_720x480i_60Hz:
	case HDMITX_VFMT_21_720x576i_50Hz:
	case HDMITX_VFMT_22_720x576i_50Hz:
		err = setHwRegisterFieldTable(pDis, &kVfmt480i576iPllCfg[0]);
		RETIF_REG_FAIL(err)

		    switch (pixRate) {
		case HDMITX_PIXRATE_SINGLE:
			/* Single edge mode, vinFmt 480i or 576i */
			err = setHwRegisterFieldTable(pDis, &kSinglePrateVfmt480i576iPllCfg[0]);
			RETIF_REG_FAIL(err)

			    break;
		case HDMITX_PIXRATE_SINGLE_REPEATED:
			/* Single repeated edge mode, vinFmt 480i or 576i */
			err = setHwRegisterFieldTable(pDis, &kSrepeatedPrateVfmt480i576iPllCfg[0]);
			RETIF_REG_FAIL(err)

			    break;
		default:
			/* Double edge mode doesn't exist for vinFmt 480i or 576i */
			return (TMBSL_ERR_HDMI_INCONSISTENT_PARAMS);
		}

		break;


		/* Others video input format */
	default:
		err = setHwRegisterFieldTable(pDis, &kVfmtOtherPllCfg[0]);
		RETIF_REG_FAIL(err)

		    switch (pixRate) {
		case HDMITX_PIXRATE_SINGLE:
			/* Single edge mode, vinFmt other than 480i or 576i */
			err = setHwRegisterFieldTable(pDis, &kSinglePrateVfmtOtherPllCfg[0]);
			RETIF_REG_FAIL(err)
			    break;
		case HDMITX_PIXRATE_DOUBLE:
			/* Double edge mode, vinFmt other than 480i or 576i */
			err = setHwRegisterFieldTable(pDis, &kDoublePrateVfmtOtherPllCfg[0]);
			RETIF_REG_FAIL(err)
			    break;
		default:
			/* Single repeated edge mode doesn't exist for other vinFmt */
			return (TMBSL_ERR_HDMI_INCONSISTENT_PARAMS);
		}
		break;

	}

	/* DDC interface is disable for TDA9989 after reset, enable it */
	err = setHwRegister(pDis, E_REG_P00_DDC_DISABLE_RW, 0x00);
	RETIF(err != TM_OK, err)

	    /* Set clock speed of the DDC channel */
	    err = setHwRegister(pDis, E_REG_P12_TX3_RW, TDA19989_DDC_SPEED_FACTOR);
	RETIF(err != TM_OK, err)

	    /* TDA19989 N1 only */
	    if (pDis->uDeviceVersion == E_DEV_VERSION_TDA19989) {

		err = setHwRegisterField(pDis, E_REG_P00_I2C_MASTER_RW, E_MASKREG_P00_I2C_MASTER_dis_mm, 0);	/* 0: enable multi master mode */
		RETIF_REG_FAIL(err)

	}

	/* TDA19989 N1 only */
	err =
	    setCECHwRegister(pDis, E_REG_CEC_FRO_IM_CLK_CTRL_RW,
			     E_MASKREG_CEC_FRO_IM_CLK_CTRL_ghost_dis |
			     E_MASKREG_CEC_FRO_IM_CLK_CTRL_imclk_sel);
	RETIF_REG_FAIL(err)

	    /* The DIS hotplug status is HDMITX_HOTPLUG_INVALID, so call the main
	     * interrupt handler to read the current Hot Plug status and run any
	     * registered HPD callback before interrupts are enabled below */
	    /* err = tmbslTDA9989HwHandleInterrupt(txUnit); */
	    RETIF(err != TM_OK, err)

	    /* enable  sw _interrupt and  VS_interrupt for debug */
	    err = setHwRegister(pDis, E_REG_P00_INT_FLAGS_1_RW, E_MASKREG_P00_INT_FLAGS_1_sw_int);

	/* enable edid read */
	err = setHwRegister(pDis, E_REG_P00_INT_FLAGS_2_RW, E_MASKREG_P00_INT_FLAGS_2_edid_blk_rd);


	/* Read HPD RXS level */
	err = getCECHwRegister(pDis, E_REG_CEC_RXSHPDLEV_R, &regVal);
	RETIF(err != TM_OK, err)

	    /* Read Hot Plug input status to know the actual level that caused the interrupt */
	    pDis->hotPlugStatus = (regVal & E_MASKREG_CEC_RXSHPDLEV_hpd_level) ?
	    HDMITX_HOTPLUG_ACTIVE : HDMITX_HOTPLUG_INACTIVE;

	/*Read RXS_FIL status to know the actual level that caused the interrupt */
	pDis->rxSenseStatus = (regVal & E_MASKREG_CEC_RXSHPDLEV_rxs_level) ?
	    HDMITX_RX_SENSE_ACTIVE : HDMITX_RX_SENSE_INACTIVE;

	/*Disable required Interrupts */
	err = getCECHwRegister(pDis, E_REG_CEC_RXSHPDINTENA_RW, &EnableIntMask);
	EnableIntMask |= E_MASKREG_CEC_RXSHPDINTENA_ena_rxs_int;	/* Enable RxSense Interrupt */
	EnableIntMask |= E_MASKREG_CEC_RXSHPDINTENA_ena_hpd_int;	/* Enable HPD Interrupt */

	/* Switch BSL State machine into UNINITIALIZED State */
	setState(pDis, EV_INIT);

	/*Write data in RXSHPD Register */
	err += setCECHwRegister(pDis, E_REG_CEC_RXSHPDINTENA_RW, EnableIntMask);
	err += getCECHwRegister(pDis, E_REG_CEC_RXSHPDLEV_R, &int_level);

	/* Enable HPD and RX sense IT after first call done by init function */
	gMiscInterruptHpdRxEnable = True;

	return err;
}

/*============================================================================*/
/* tmbslTDA9989PowerGetState                                                   */
/*============================================================================*/

tmErrorCode_t tmbslTDA9989PowerGetState(tmUnitSelect_t txUnit, tmPowerState_t *pePowerState) {
	tmHdmiTxobject_t *pDis;	/* Pointer to Device Instance Structure */
	tmErrorCode_t err;	/* Error code */

	/* Check unit parameter and point to its object */
	err = checkUnitSetDis(txUnit, &pDis);
	RETIF(err != TM_OK, err)

	    /* Check remaining parameters */
	    RETIF_BADPARAM(pePowerState == (tmPowerState_t) 0)

	    /*return parameter */
	    * pePowerState = (tmPowerState_t) pDis->ePowerState;

	return TM_OK;
}


/*============================================================================*/
/* tmbslTDA9989PowerSetState                                                   */
/*============================================================================*/
tmErrorCode_t tmbslTDA9989PowerSetState(tmUnitSelect_t txUnit, tmPowerState_t ePowerState) {
	tmHdmiTxobject_t *pDis;	/* Pointer to Device Instance Structure */
	tmErrorCode_t err;	/* Error code */
	UInt8 RegVal = 0;	/* Local Variable */

	/* Check unit parameter and point to its object */
	err = checkUnitSetDis(txUnit, &pDis);
	RETIF(err != TM_OK, err)

	    if (ePowerState == tmPowerOff) {
		ePowerState = tmPowerStandby;
	}


	/* Check remaining parameters */
	RETIF_BADPARAM((ePowerState != tmPowerStandby)
		       && (ePowerState != tmPowerSuspend)
		       && (ePowerState != tmPowerOn)
	    )

	    if ((ePowerState == tmPowerStandby) && (pDis->ePowerState != tmPowerStandby)) {
		/*Disable HPD and RxSense Interrupts */
		err = getCECHwRegister(pDis, E_REG_CEC_RXSHPDINTENA_RW, &RegVal);
		RegVal &= ~E_MASKREG_CEC_RXSHPDINTENA_ena_hpd_int;
		RegVal &= ~E_MASKREG_CEC_RXSHPDINTENA_ena_rxs_int;
		err += setCECHwRegister(pDis, E_REG_CEC_RXSHPDINTENA_RW, RegVal);
		err += getCECHwRegister(pDis, E_REG_CEC_RXSHPDLEV_R, &int_level);
		RETIF_REG_FAIL(err)

		    /* Disable if coming from ACTIVE */
		    if (pDis->ePowerState == tmPowerOn) {

			/* Disable audio and video ports */
			err = setHwRegister(pDis, E_REG_P00_ENA_AP_RW, 0x00);	/* 0: disable */
			RETIF_REG_FAIL(err)


			    err = setHwRegister(pDis, E_REG_P00_ENA_VP_0_RW, 0x00);	/* 0: disable */
			RETIF_REG_FAIL(err)

			    err = setHwRegister(pDis, E_REG_P00_ENA_VP_1_RW, 0x00);	/* 0: disable */
			RETIF_REG_FAIL(err)

			    err = setHwRegister(pDis, E_REG_P00_ENA_VP_2_RW, 0x00);	/* 0: disable */
			RETIF_REG_FAIL(err)

			    /* Disable DDC */
			    err = setHwRegisterField(pDis, E_REG_P00_DDC_DISABLE_RW, E_MASKREG_P00_DDC_DISABLE_ddc_dis, 1);	/* 1: disable */
			RETIF_REG_FAIL(err);

#ifdef TMFL_HDCP_OPTIMIZED_POWER
			/* power down clocks */
			tmbslTDA9989HdcpPowerDown(txUnit, True);
			err = setHwRegisterField(pDis, E_REG_FEAT_POWER_DOWN,
						 E_MASKREG_FEAT_POWER_DOWN_all, 0x0F);
#endif
		}

		/*Disable HDMI and RxSense Modes AND FRO if required */
		err = getCECHwRegister(pDis, E_REG_CEC_ENAMODS_RW, &RegVal);
		RegVal &= ~E_MASKREG_CEC_ENAMODS_ena_hdmi;	/* Reset HDMI Mode */

		err = setCECHwRegister(pDis, E_REG_CEC_ENAMODS_RW, RegVal);
		RETIF_REG_FAIL(err)

		    err = getCECHwRegister(pDis, E_REG_CEC_ENAMODS_RW, &RegVal);
		RETIF_REG_FAIL(err)
		    RegVal |= E_MASKREG_CEC_ENAMODS_ena_hdmi;	/* Set HDMI Mode */
		err = setCECHwRegister(pDis, E_REG_CEC_ENAMODS_RW, RegVal);
		RETIF_REG_FAIL(err)

		    err = getCECHwRegister(pDis, E_REG_CEC_ENAMODS_RW, &RegVal);
		RegVal &= ~E_MASKREG_CEC_ENAMODS_ena_hdmi;	/* Reset HDMI Mode */

		RegVal &= ~E_MASKREG_CEC_ENAMODS_ena_rxs;	/* Reset RxSense Mode */


		/* disable FRO */
		RegVal |= E_MASKREG_CEC_ENAMODS_dis_fro;
		err = setCECHwRegister(pDis, E_REG_CEC_ENAMODS_RW, RegVal);
		RETIF_REG_FAIL(err)


		    /*Send STANDBY event to the BSL State Machine */
		    setState(pDis, EV_STANDBY);
	} else if ((ePowerState == tmPowerSuspend) && (pDis->ePowerState != tmPowerSuspend)) {

		/* Disable if coming from ACTIVE */
		if (pDis->ePowerState == tmPowerOn) {
			/* Disable audio and video ports */
			err = setHwRegister(pDis, E_REG_P00_ENA_AP_RW, 0x00);	/* 0: disable */
			RETIF_REG_FAIL(err)


			    err = setHwRegister(pDis, E_REG_P00_ENA_VP_0_RW, 0x00);	/* 0: disable */
			RETIF_REG_FAIL(err)

			    err = setHwRegister(pDis, E_REG_P00_ENA_VP_1_RW, 0x00);	/* 0: disable */
			RETIF_REG_FAIL(err)

			    err = setHwRegister(pDis, E_REG_P00_ENA_VP_2_RW, 0x00);	/* 0: disable */
			RETIF_REG_FAIL(err);

			/* Disable DDC */
			err = setHwRegisterField(pDis, E_REG_P00_DDC_DISABLE_RW, E_MASKREG_P00_DDC_DISABLE_ddc_dis, 1);	/* 1: disable */
			RETIF_REG_FAIL(err);

#ifdef TMFL_HDCP_OPTIMIZED_POWER
			/* power down clocks */
			tmbslTDA9989HdcpPowerDown(txUnit, True);
			err = setHwRegisterField(pDis, E_REG_FEAT_POWER_DOWN,
						 E_MASKREG_FEAT_POWER_DOWN_all, 0x0F);
#endif
		}

		/*Enable RxSense Mode and Disable HDMI Mode */
		err = getCECHwRegister(pDis, E_REG_CEC_ENAMODS_RW, &RegVal);
		RegVal &= ~E_MASKREG_CEC_ENAMODS_ena_hdmi;	/* Reset HDMI Mode */
		RegVal |= E_MASKREG_CEC_ENAMODS_ena_rxs;	/* Set RxSense Mode */
		err = setCECHwRegister(pDis, E_REG_CEC_ENAMODS_RW, RegVal);
		RETIF_REG_FAIL(err)

		    /*Enable HPD and RxS Interupt in case of the current Device Power States is STANDBY */
		    /*In other cases, those interrupts have already been enabled */
		    if (pDis->ePowerState == tmPowerStandby) {
			/* Enable FRO if coming from STANDBY */
			err = getCECHwRegister(pDis, E_REG_CEC_ENAMODS_RW, &RegVal);
			RegVal &= ~E_MASKREG_CEC_ENAMODS_dis_fro;	/* Enable FRO  */
			err = setCECHwRegister(pDis, E_REG_CEC_ENAMODS_RW, RegVal);
			RETIF_REG_FAIL(err)

			    /*Enable HPD and RxS Interupt */
			    err = getCECHwRegister(pDis, E_REG_CEC_RXSHPDINTENA_RW, &RegVal);
			RegVal |= E_MASKREG_CEC_RXSHPDINTENA_ena_hpd_int;	/* Enable HPD Interrupt */
			RegVal |= E_MASKREG_CEC_RXSHPDINTENA_ena_rxs_int;	/* Enable RxSense Interrupt */
			err += setCECHwRegister(pDis, E_REG_CEC_RXSHPDINTENA_RW, RegVal);
			err += getCECHwRegister(pDis, E_REG_CEC_RXSHPDLEV_R, &int_level);
			RETIF_REG_FAIL(err)

			    /* force interrupt HPD and RXS level reading */
			    err = tmbslTDA9989HwHandleInterrupt(txUnit);
			RETIF(err != TM_OK, err)


		}

		/*Send the SLEEP event to the BSL State Machine */
		setState(pDis, EV_SLEEP);

	}

	else if ((ePowerState == tmPowerOn) && (pDis->ePowerState != tmPowerOn)) {

		/* Enable RxSense HDMI Modes */
		err = getCECHwRegister(pDis, E_REG_CEC_ENAMODS_RW, &RegVal);
		RegVal |= E_MASKREG_CEC_ENAMODS_ena_hdmi;	/* Set HDMI Mode */
		RegVal |= E_MASKREG_CEC_ENAMODS_ena_rxs;	/* Set RxSense Mode */
		err = setCECHwRegister(pDis, E_REG_CEC_ENAMODS_RW, RegVal);
		RETIF_REG_FAIL(err)

		    /*Enable HPD and RxS Interupt in case of the current Device Power States is STANDBY */
		    /*In other cases, those interrupts have already been enabled */
		    if (pDis->ePowerState == tmPowerStandby) {
			/* Enable FRO if coming from STANDBY */
			err = getCECHwRegister(pDis, E_REG_CEC_ENAMODS_RW, &RegVal);
			RegVal &= ~E_MASKREG_CEC_ENAMODS_dis_fro;	/* Enable FRO  */
			err = setCECHwRegister(pDis, E_REG_CEC_ENAMODS_RW, RegVal);
			RETIF_REG_FAIL(err)


			    /*Apply the required mode, Reset RxS and HDMI bits */
			    err = getCECHwRegister(pDis, E_REG_CEC_RXSHPDINTENA_RW, &RegVal);
			RegVal |= E_MASKREG_CEC_RXSHPDINTENA_ena_hpd_int;	/* Enable HPD Interrupt */
			RegVal |= E_MASKREG_CEC_RXSHPDINTENA_ena_rxs_int;	/* Enable RxSense Interrupt */
			err += setCECHwRegister(pDis, E_REG_CEC_RXSHPDINTENA_RW, RegVal);
			err += getCECHwRegister(pDis, E_REG_CEC_RXSHPDLEV_R, &int_level);
			RETIF_REG_FAIL(err)
		}


		/* Restore BIAS TMDS */
		RegVal = HDMI_TX_VSWING_VALUE;
		err = setHwRegister(pDis, E_REG_P02_ANA_GENERAL_RW, RegVal);
		RETIF(err != TM_OK, err)


		    err = tmbslTDA9989Reset(txUnit);
		RETIF(err != TM_OK, err)


		    err =
		    setHwRegisterField(pDis, E_REG_P00_VIP_CNTRL_4_W,
				       E_MASKREG_P00_VIP_CNTRL_4_656_alt, 0);

		err = setHwRegister(pDis, E_REG_P12_TX3_RW, TDA19989_DDC_SPEED_FACTOR);
		RETIF(err != TM_OK, err)

		    /* TDA19989 N1 only */
		    if (pDis->uDeviceVersion == E_DEV_VERSION_TDA19989) {

			err = setHwRegisterField(pDis,
						 E_REG_P00_I2C_MASTER_RW,
						 E_MASKREG_P00_I2C_MASTER_dis_mm, 0);
			RETIF_REG_FAIL(err)

		}
		/* TDA19989 N1 only */
		err =
		    setCECHwRegister(pDis, E_REG_CEC_FRO_IM_CLK_CTRL_RW,
				     E_MASKREG_CEC_FRO_IM_CLK_CTRL_ghost_dis |
				     E_MASKREG_CEC_FRO_IM_CLK_CTRL_imclk_sel);
		RETIF_REG_FAIL(err)
#ifdef TMFL_HDCP_SUPPORT
		    if (pDis->HdcpSeed) {
			err =
			    tmbslTDA9989HdcpDownloadKeys(txUnit, pDis->HdcpSeed,
							 HDMITX_HDCP_DECRYPT_ENABLE);
		}
#endif				/* TMFL_HDCP_SUPPORT */


		/* Enable DDC */
		err = setHwRegisterField(pDis, E_REG_P00_DDC_DISABLE_RW, E_MASKREG_P00_DDC_DISABLE_ddc_dis, 0);	/* 0: enable */
		RETIF_REG_FAIL(err)

		    /* Enable audio and video ports */
		    err = setHwRegister(pDis, E_REG_P00_ENA_AP_RW, 0xFF);	/* 1: enable */
		RETIF_REG_FAIL(err)


		    err = setHwRegister(pDis, E_REG_P00_ENA_VP_0_RW, 0xFF);	/* 1: enable */
		RETIF_REG_FAIL(err)

		    err = setHwRegister(pDis, E_REG_P00_ENA_VP_1_RW, 0xFF);	/* 1: enable */
		RETIF_REG_FAIL(err)

		    err = setHwRegister(pDis, E_REG_P00_ENA_VP_2_RW, 0xFF);	/* 1: enable */
		RETIF_REG_FAIL(err)


		    /*Send the Hot Plug detection status event to the BSL State Machine */
		    if (pDis->hotPlugStatus == HDMITX_HOTPLUG_ACTIVE) {
			setState(pDis, EV_PLUGGEDIN);
		} else {
			setState(pDis, EV_UNPLUGGED);
		}

	}

	/* Set the current Device Power status to the required Power Status */
	pDis->ePowerState = (tmbslHdmiTxPowerState_t) ePowerState;


	return TM_OK;
}

/*============================================================================*/
/* tmbslTDA9989Reset                                                           */
/*============================================================================*/
tmErrorCode_t tmbslTDA9989Reset(tmUnitSelect_t txUnit) {
	tmHdmiTxobject_t *pDis;	/* Pointer to Device Instance Structure */
	tmErrorCode_t err;	/* Error code */

	/* Check unit parameter and point to its object */
	err = checkUnitSetDis(txUnit, &pDis);
	RETIF(err != TM_OK, err)

	    /* Reset I2C master and Audio */
	    (void)setHwRegisterField(pDis, E_REG_P00_SR_REG_W,
				     E_MASKREG_P00_SR_REG_sr_i2c_ms |
				     E_MASKREG_P00_SR_REG_sr_audio, 1);

	pDis->sysFuncTimer(50);	/* ms */

	(void)setHwRegisterField(pDis, E_REG_P00_SR_REG_W,
				 E_MASKREG_P00_SR_REG_sr_i2c_ms | E_MASKREG_P00_SR_REG_sr_audio, 0);

	pDis->sysFuncTimer(50);	/* ms */

	/* Write to the transmitter to do a soft reset. Don't abort after any
	 * error here, to ensure full reset.
	 */
	(void)setHwRegisterField(pDis, E_REG_P00_MAIN_CNTRL0_RW, E_MASKREG_P00_MAIN_CNTRL0_sr, 1);
	/* pDis->sysFuncTimer(50); *//* ms */
	(void)setHwRegisterField(pDis, E_REG_P00_MAIN_CNTRL0_RW, E_MASKREG_P00_MAIN_CNTRL0_sr, 0);
	/* pDis->sysFuncTimer(50); *//* ms */
	/* Clear any colourbars */
	(void)setHwRegisterField(pDis, E_REG_P00_HVF_CNTRL_0_W, E_MASKREG_P00_HVF_CNTRL_0_sm, 0);

#ifdef TMFL_HDCP_SUPPORT
	/* Disable any scheduled function and HDCP check timer */
	pDis->HdcpFuncRemainingMs = 0;
	pDis->HdcpCheckNum = 0;
#endif				/* TMFL_HDCP_SUPPORT */

	/* Switch BSL State machine into UNINITIALIZED State */
	setState(pDis, EV_DEINIT);
	/* Switch Power State into STAND_BY State */
	/* pDis->ePowerState = tmPowerStandby; */
	return TM_OK;
}

/*============================================================================*/
/* tmbslTDA9989SwGetVersion                                                    */
/*============================================================================*/
tmErrorCode_t tmbslTDA9989SwGetVersion(ptmSWVersion_t pSWVersion) {
	/* Check parameters */
	RETIF_BADPARAM(pSWVersion == (ptmSWVersion_t) 0)

	    /* Get the version details of the component. */
	    pSWVersion->compatibilityNr = HDMITX_BSL_COMP_NUM;
	pSWVersion->majorVersionNr = HDMITX_BSL_MAJOR_VER;
	pSWVersion->minorVersionNr = HDMITX_BSL_MINOR_VER;

	return TM_OK;
}

/*============================================================================*/
/* tmbslTDA9989SysTimerWait                                                    */
/*============================================================================*/
tmErrorCode_t tmbslTDA9989SysTimerWait(tmUnitSelect_t txUnit, UInt16 waitMs) {
	tmHdmiTxobject_t *pDis;	/* Pointer to Device Instance Structure */
	tmErrorCode_t err;	/* Error code */

	/* Check unit parameter and point to TX unit object */
	err = checkUnitSetDis(txUnit, &pDis);
	RETIF(err != TM_OK, err)

	    /* Return if this device timer is not set up */
	    RETIF(!pDis->sysFuncTimer, TMBSL_ERR_HDMI_NOT_INITIALIZED)

	    /* Wait for the requested time */
	    pDis->sysFuncTimer(waitMs);

	return TM_OK;
}

/*============================================================================*/
/* tmbslTDA9989TestSetMode                                                     */
/*============================================================================*/

tmErrorCode_t
    tmbslTDA9989TestSetMode
    (tmUnitSelect_t txUnit, tmbslHdmiTxTestMode_t testMode, tmbslHdmiTxTestState_t testState) {
	tmHdmiTxobject_t *pDis;	/* Pointer to Device Instance Structure */
	tmErrorCode_t err;	/* Error code */
	/* Register used to activate a test */
	UInt16 testReg = E_REG_P00_VIP_CNTRL_4_W;
	/* Register bitfield mask used */
	UInt8 testMask = E_MASKREG_P00_VIP_CNTRL_4_tst_pat;

	/* Check unit parameter and point to TX unit object */
	err = checkUnitSetDis(txUnit, &pDis);
	RETIF(err != TM_OK, err)

	    /* Check parameters */
	    RETIF_BADPARAM(testMode >= HDMITX_TESTMODE_INVALID)
	    RETIF_BADPARAM(testState >= HDMITX_TESTSTATE_INVALID)

	    /* Set the mode selected by testMode to the state indicated by testState */
	    switch (testMode) {
	case HDMITX_TESTMODE_PAT:
		testReg = E_REG_P00_VIP_CNTRL_4_W;
		testMask = E_MASKREG_P00_VIP_CNTRL_4_tst_pat;
		break;
	case HDMITX_TESTMODE_656:
		testReg = E_REG_P00_VIP_CNTRL_4_W;
		testMask = E_MASKREG_P00_VIP_CNTRL_4_tst_656;
		break;
	case HDMITX_TESTMODE_SERPHOE:
		/*testReg  = E_REG_P02_TEST1_RW;
		   testMask = E_MASKREG_P02_TEST1_tstserphoe; */
		break;
	case HDMITX_TESTMODE_NOSC:
		testReg = E_REG_P02_TEST1_RW;
		testMask = E_MASKREG_P02_TEST1_tst_nosc;
		break;
	case HDMITX_TESTMODE_HVP:
		/*testReg  = E_REG_P02_TEST1_RW;
		   testMask = E_MASKREG_P02_TEST1_tst_hvp; */
		break;
	case HDMITX_TESTMODE_PWD:
		/*testReg  = E_REG_P02_TEST2_RW;
		   testMask = E_MASKREG_P02_TEST2_pwd1v8; */
		break;
	case HDMITX_TESTMODE_DIVOE:
		/*testReg  = E_REG_P02_TEST2_RW;
		   testMask = E_MASKREG_P02_TEST2_divtestoe; */
		break;
	case HDMITX_TESTMODE_INVALID:
		break;
	}
	err = setHwRegisterField(pDis, testReg, testMask, (UInt8) testState);
	return err;
}

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
	    - TMBSL_ERR_BSLHDMIRX_BAD_UNIT_NUMBER: the unit number is wrong or
	      the receiver instance is not initialised
	    - TMBSL_ERR_BSLHDMIRX_BAD_PARAMETER: a parameter is invalid or out
	      of range
	    - TMBSL_ERR_BSLHDMIRX_I2C_WRITE: failed when writing to the I2C
	      bus

 ******************************************************************************/
tmErrorCode_t tmbslTDA9989PktFillGamut
    (tmUnitSelect_t txUnit, tmbslHdmiTxPktGamut_t *pPkt, UInt8 bufSel) {
	tmHdmiTxobject_t *pDis;	/* Pointer to Device Instance Structure */
	tmErrorCode_t err;	/* Error code */

	/* Check unit parameter and point to TX unit object */

	err = checkUnitSetDis(txUnit, &pDis);
	RETIF(err != TM_OK, err)

	    /* Check parameters */
	    RETIF_BADPARAM((bufSel != 0) && (bufSel != 1))

	    /* Fill Gamut registers */
	    /*Fill buffer 0 */
	    if (bufSel == 0) {
		/*Write Header */
		err = setHwRegisters(pDis, E_REG_P13_GMD_0_HB0_RW, &pPkt->HB[0], 3);
		RETIF(err != TM_OK, err)
		    /*Write Payload */
		    err = setHwRegisters(pDis, E_REG_P13_GMD_0_PB0_RW, &pPkt->PB[0], 28);
		RETIF(err != TM_OK, err)
	}

	/*Fill buffer 1 */
	else {
		/*Write Header */
		err = setHwRegisters(pDis, E_REG_P13_GMD_1_HB0_RW, &pPkt->HB[0], 3);
		RETIF(err != TM_OK, err)
		    /*Write Payload */
		    err = setHwRegisters(pDis, E_REG_P13_GMD_1_PB0_RW, &pPkt->PB[0], 28);
		RETIF(err != TM_OK, err)
	}

	return err;
}

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
	    - TMBSL_ERR_BSLHDMIRX_BAD_UNIT_NUMBER: the unit number is wrong or
	      the receiver instance is not initialised
	    - TMBSL_ERR_BSLHDMIRX_BAD_PARAMETER: a parameter is invalid or out
	      of range
	    - TMBSL_ERR_BSLHDMIRX_I2C_WRITE: failed when writing to the I2C
	      bus

 ******************************************************************************/
tmErrorCode_t tmbslTDA9989PktSendGamut(tmUnitSelect_t txUnit, UInt8 bufSel, Bool bEnable) {
	tmHdmiTxobject_t *pDis;	/* Pointer to Device Instance Structure */
	tmErrorCode_t err;	/* Error code */
	UInt8 GMD_Ctrl_Val;	/*GMD control Value */

	/* Check unit parameter and point to TX unit object */
	err = checkUnitSetDis(txUnit, &pDis);
	RETIF(err != TM_OK, err)

	    /* Check parameters */
	    RETIF_BADPARAM((bufSel != 0) && (bufSel != 1))

	    /*Init Value */
	    GMD_Ctrl_Val = 0x00;

	/*Enable Send of Gamut MetaData */
	if (bEnable) {
		/*Send Buffer 0 */
		if (bufSel == 0) {
			GMD_Ctrl_Val |= E_MASKREG_P13_GMD_CONTROL_enable;
			GMD_Ctrl_Val &= ~E_MASKREG_P13_GMD_CONTROL_buf_sel;
			err = setHwRegister(pDis, E_REG_P13_GMD_CONTROL_RW, GMD_Ctrl_Val);
			RETIF(err != TM_OK, err)
		}
		/*Send Buffer 1 */
		else {
			GMD_Ctrl_Val |= E_MASKREG_P13_GMD_CONTROL_enable;
			GMD_Ctrl_Val |= E_MASKREG_P13_GMD_CONTROL_buf_sel;
			err = setHwRegister(pDis, E_REG_P13_GMD_CONTROL_RW, GMD_Ctrl_Val);
			RETIF(err != TM_OK, err)
		}
	}
	/*Disable Send of Gamut MetaData */
	else {
		GMD_Ctrl_Val &= ~E_MASKREG_P13_GMD_CONTROL_enable;
		GMD_Ctrl_Val &= ~E_MASKREG_P13_GMD_CONTROL_buf_sel;
		err = setHwRegister(pDis, E_REG_P13_GMD_CONTROL_RW, GMD_Ctrl_Val);
		RETIF(err != TM_OK, err)
	}

	return err;
}




/*============================================================================*/
/* tmbslTDA9989EnableCallback                                                 */
/*============================================================================*/
tmErrorCode_t
    tmbslTDA9989EnableCallback
    (tmUnitSelect_t txUnit, tmbslHdmiTxCallbackInt_t callbackSource, Bool enable) {
	tmHdmiTxobject_t *pDis;	/* Pointer to Device Instance Structure */
	tmErrorCode_t err = TM_OK;	/* Error code */

	/* Check unit parameter and point to TX unit object */
	err = checkUnitSetDis(txUnit, &pDis);
	RETIF(err != TM_OK, err)

	    /* Check parameters */
	    RETIF_BADPARAM(callbackSource >= HDMITX_CALLBACK_INT_NUM)

	    switch (callbackSource) {
	case HDMITX_CALLBACK_INT_VS_RPT:
		/* Enable or disable VS Interrupt */
		err = setHwRegisterField(pDis,
					 E_REG_P00_INT_FLAGS_1_RW,
					 E_MASKREG_P00_INT_FLAGS_1_vs_rpt, (UInt8) enable);
		if (enable) {
			pDis->InterruptsEnable |= (1 << callbackSource);
		} else {
			pDis->InterruptsEnable &= ~(1 << callbackSource);
		}
		break;
	default:
		err = TMBSL_ERR_HDMI_NOT_SUPPORTED;
		break;
	}

	return err;
}

/*============================================================================*/
/* tmbslTDA9989SetColorDepth                                                  */
/*============================================================================*/
tmErrorCode_t
    tmbslTDA9989SetColorDepth
    (tmUnitSelect_t txUnit, tmbslHdmiTxColorDepth colorDepth, Bool termEnable) {
	tmHdmiTxobject_t *pDis;	/* Pointer to Device Instance Structure */
	tmErrorCode_t err = TM_OK;	/* Error code */

	DUMMY_ACCESS(termEnable);

	/* Check unit parameter and point to TX unit object */
	err = checkUnitSetDis(txUnit, &pDis);
	RETIF(err != TM_OK, err)

	    /* Check parameters */
	    RETIF_BADPARAM(colorDepth >= HDMITX_COLORDEPTH_INVALID)

	    switch (colorDepth) {
	case HDMITX_COLORDEPTH_NO_CHANGE:
		break;

	case HDMITX_COLORDEPTH_24:

		break;

	default:
		err = TMBSL_ERR_HDMI_NOT_SUPPORTED;
		break;
	}

	return err;

}


/*============================================================================*/
/* tmbslTDA9989Set5vpower                                                     */
/*============================================================================*/
tmErrorCode_t tmbslTDA9989Set5vpower(tmUnitSelect_t txUnit, Bool pwrEnable) {
	tmHdmiTxobject_t *pDis;	/* Pointer to Device Instance Structure */
	tmErrorCode_t err;	/* Error code */

	DUMMY_ACCESS(pwrEnable);

	/* Check unit parameter and point to TX unit object */
	err = checkUnitSetDis(txUnit, &pDis);
	RETIF(err != TM_OK, err)


	    return TMBSL_ERR_HDMI_NOT_SUPPORTED;
}

/*============================================================================*/
/* tmbslTDA9989SetDefaultPhase                                                */
/*============================================================================*/
tmErrorCode_t
    tmbslTDA9989SetDefaultPhase
    (tmUnitSelect_t txUnit, Bool bEnable, tmbslHdmiTxColorDepth colorDepth, UInt8 videoFormat) {
	tmHdmiTxobject_t *pDis;	/* Pointer to Device Instance Structure */
	tmErrorCode_t err;	/* Error code */

	DUMMY_ACCESS(bEnable);
	DUMMY_ACCESS(colorDepth);
	DUMMY_ACCESS(videoFormat);

	/* Check unit parameter and point to TX unit object */
	err = checkUnitSetDis(txUnit, &pDis);
	RETIF(err != TM_OK, err)

	    return TMBSL_ERR_HDMI_NOT_SUPPORTED;
}



/*============================================================================*/
/* tmbslDebugWriteFakeRegPage                                                 */
/*============================================================================*/
tmErrorCode_t tmbslDebugWriteFakeRegPage(tmUnitSelect_t txUnit)
{
	tmHdmiTxobject_t *pDis;	/* Pointer to Device Instance Structure */
	tmErrorCode_t err;	/* Error code */

	err = checkUnitSetDis(txUnit, &pDis);

	pDis->curRegPage = 0x20;

	return err;
}


/*============================================================================*/
/* hotPlugRestore                                                             */
/*============================================================================*/
tmErrorCode_t hotPlugRestore(tmUnitSelect_t txUnit)
{
	tmHdmiTxobject_t *pDis;	/* Pointer to Device Instance Structure */
	tmErrorCode_t err;	/* Error code */
	UInt8 regVal;
	UInt8 EnableIntMask = 0;	/* Mask used to enable HPD and RX Sense interrupt */

	/* Check unit parameter and point to TX unit object */
	err = checkUnitSetDis(txUnit, &pDis);
	RETIF(err != TM_OK, err)

	    /* Set the BIAS_tmds Value */
	    regVal = HDMI_TX_VSWING_VALUE;
	err = setHwRegister(pDis, E_REG_P02_ANA_GENERAL_RW, regVal);
	RETIF(err != TM_OK, err)

	    /* PLL registers common configuration */
	    err = setHwRegisterFieldTable(pDis, &kCommonPllCfg[0]);
	RETIF_REG_FAIL(err)

	    /*Reset 656_Alt bit in VIP_CONTROL_4 Register */
	    err =
	    setHwRegisterField(pDis, E_REG_P00_VIP_CNTRL_4_W, E_MASKREG_P00_VIP_CNTRL_4_656_alt, 0);

	/* DDC interface is disable for TDA9989 after reset, enable it */
	err = setHwRegister(pDis, E_REG_P00_DDC_DISABLE_RW, 0x00);
	RETIF(err != TM_OK, err)

	    err = setHwRegister(pDis, E_REG_P12_TX3_RW, TDA19989_DDC_SPEED_FACTOR);
	RETIF(err != TM_OK, err)

	    /* TDA19989 N1 only */
	    if (pDis->uDeviceVersion == E_DEV_VERSION_TDA19989) {

		err = setHwRegisterField(pDis, E_REG_P00_I2C_MASTER_RW, E_MASKREG_P00_I2C_MASTER_dis_mm, 0);	/* 0: enable multi master mode */
		RETIF_REG_FAIL(err)

	}

	/* TDA19989 N1 only */
	err =
	    setCECHwRegister(pDis, E_REG_CEC_FRO_IM_CLK_CTRL_RW,
			     E_MASKREG_CEC_FRO_IM_CLK_CTRL_ghost_dis |
			     E_MASKREG_CEC_FRO_IM_CLK_CTRL_imclk_sel);
	RETIF_REG_FAIL(err)




	    /* enable sw_interrupt for debug */
	    err = setHwRegister(pDis, E_REG_P00_INT_FLAGS_1_RW, E_MASKREG_P00_INT_FLAGS_1_sw_int);

	/* enable edid read */
	err = setHwRegister(pDis, E_REG_P00_INT_FLAGS_2_RW, E_MASKREG_P00_INT_FLAGS_2_edid_blk_rd);

	err = getCECHwRegister(pDis, E_REG_CEC_RXSHPDINTENA_RW, &EnableIntMask);
	EnableIntMask |= E_MASKREG_CEC_RXSHPDINTENA_ena_rxs_int;	/* Enable RxSense Interrupt */
	EnableIntMask |= E_MASKREG_CEC_RXSHPDINTENA_ena_hpd_int;	/* Enable HPD Interrupt */
	err += setCECHwRegister(pDis, E_REG_CEC_RXSHPDINTENA_RW, EnableIntMask);
	err += getCECHwRegister(pDis, E_REG_CEC_RXSHPDLEV_R, &int_level);

#ifdef TMFL_HDCP_SUPPORT

	if (pDis->HdcpSeed) {
		err =
		    tmbslTDA9989HdcpDownloadKeys(txUnit, pDis->HdcpSeed,
						 HDMITX_HDCP_DECRYPT_ENABLE);
	}
#endif				/* TMFL_HDCP_SUPPORT */

	setState(pDis, EV_INIT);

	return err;
}

#ifdef TMFL_TDA9989_PIXEL_CLOCK_ON_DDC

tmErrorCode_t hotPlugRestore(tmUnitSelect_t txUnit)
{
	tmHdmiTxobject_t *pDis;	/* Pointer to Device Instance Structure */
	tmErrorCode_t err;	/* Error code */
	UInt8 regVal;
	UInt8 EnableIntMask = 0;	/* Mask used to enable HPD and RX Sense interrupt */

	/* Check unit parameter and point to TX unit object */
	err = checkUnitSetDis(txUnit, &pDis);
	RETIF(err != TM_OK, err)

	    /* Set the BIAS_tmds Value */
	    regVal = HDMI_TX_VSWING_VALUE;
	err = setHwRegister(pDis, E_REG_P02_ANA_GENERAL_RW, regVal);
	RETIF(err != TM_OK, err)

	    /* PLL registers common configuration */
	    err = setHwRegisterFieldTable(pDis, &kCommonPllCfg[0]);
	RETIF_REG_FAIL(err)

	    /*Reset 656_Alt bit in VIP_CONTROL_4 Register */
	    err =
	    setHwRegisterField(pDis, E_REG_P00_VIP_CNTRL_4_W, E_MASKREG_P00_VIP_CNTRL_4_656_alt, 0);

	/* DDC interface is disable for TDA9989 after reset, enable it */
	err = setHwRegister(pDis, E_REG_P00_DDC_DISABLE_RW, 0x00);
	RETIF(err != TM_OK, err)



	    if ((pDis->vinFmt != HDMITX_VFMT_NO_CHANGE) && (pDis->vinFmt <= HDMITX_VFMT_TV_MAX)) {

		err = setHwRegister(pDis, E_REG_P00_TIMER_H_W, 0);
		RETIF(err != TM_OK, err)

		    err = setHwRegister(pDis, E_REG_P00_NDIV_IM_W, kndiv_im[pDis->vinFmt]);
		RETIF(err != TM_OK, err)

		    err = setHwRegister(pDis, E_REG_P12_TX3_RW, kclk_div[pDis->vinFmt]);
		RETIF(err != TM_OK, err)

	} else if (pDis->vinFmt > HDMITX_VFMT_TV_MAX) {

		err = setHwRegister(pDis, E_REG_P00_TIMER_H_W, E_MASKREG_P00_TIMER_H_im_clksel);
		RETIF(err != TM_OK, err)
		    err = setHwRegister(pDis, E_REG_P12_TX3_RW, 17);
		RETIF(err != TM_OK, err)
	}





	/* enable sw_interrupt for debug */
	err = setHwRegister(pDis, E_REG_P00_INT_FLAGS_1_RW, E_MASKREG_P00_INT_FLAGS_1_sw_int);

	/* enable edid read */
	err = setHwRegister(pDis, E_REG_P00_INT_FLAGS_2_RW, E_MASKREG_P00_INT_FLAGS_2_edid_blk_rd);

	err = getCECHwRegister(pDis, E_REG_CEC_RXSHPDINTENA_RW, &EnableIntMask);
	EnableIntMask |= E_MASKREG_CEC_RXSHPDINTENA_ena_rxs_int;	/* Enable RxSense Interrupt */
	EnableIntMask |= E_MASKREG_CEC_RXSHPDINTENA_ena_hpd_int;	/* Enable HPD Interrupt */
	err += setCECHwRegister(pDis, E_REG_CEC_RXSHPDINTENA_RW, EnableIntMask);
	err += getCECHwRegister(pDis, E_REG_CEC_RXSHPDLEV_R, &int_level);

	return err;
}

#endif				/* TMFL_TDA9989_PIXEL_CLOCK_ON_DDC */

#ifdef TMFL_HDCP_OPTIMIZED_POWER
/*============================================================================*/
/* tmbslTDA9989HdcpPowerDown                                                  */
/* RETIF_REG_FAIL NOT USED HERE AS ALL ERRORS SHOULD BE TRAPPED IN ALL BUILDS */
/*============================================================================*/
tmErrorCode_t tmbslTDA9989HdcpPowerDown(tmUnitSelect_t txUnit, Bool requested) {
	tmHdmiTxobject_t *pDis;	/* Pointer to Device Instance Structure */
	tmErrorCode_t err;	/* Error code */

	/* Check unit parameter and point to TX unit object */
	err = checkUnitSetDis(txUnit, &pDis);
	RETIF(err != TM_OK, err);

	err = setHwRegisterField(pDis, E_REG_P12_TX4_RW,
				 E_MASKREG_P12_TX4_pd_all, (requested ? 0x07 : 0x00));
	return err;

}
#endif

/*============================================================================*/
/*                            END OF FILE                                     */
/*============================================================================*/
