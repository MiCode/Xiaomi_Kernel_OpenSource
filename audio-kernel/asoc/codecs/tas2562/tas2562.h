
#ifndef __TAS2562_
#define __TAS2562_

#define TAS2562_DRIVER_ID           "1.0.1"

/* Page Control Register */
#define TAS2562_PAGECTL_REG			0

/* Book Control Register (available in page0 of each book) */
#define TAS2562_BOOKCTL_PAGE		0
#define TAS2562_BOOKCTL_REG			127

#define TAS2562_REG(book, page, reg)	(((book * 256 * 128) + \
											(page * 128)) + reg)


#define TAS2562_BOOK_ID(reg)			(reg / (256 * 128))

#define TAS2562_PAGE_ID(reg)			((reg % (256 * 128)) / 128)

#define TAS2562_BOOK_REG(reg)			(reg % (256 * 128))

#define TAS2562_PAGE_REG(reg)			((reg % (256 * 128)) % 128)


    /* Software Reset */
#define TAS2562_SoftwareReset  TAS2562_REG(0x0, 0x0, 0x01)
#define TAS2562_SoftwareReset_SoftwareReset_Mask  (0x1 << 0),
#define TAS2562_SoftwareReset_SoftwareReset_DontReset  (0x0 << 0)
#define TAS2562_SoftwareReset_SoftwareReset_Reset  (0x1 << 0)

    /* Power Control */
#define TAS2562_PowerControl  TAS2562_REG(0x0, 0x0, 0x02)
#define TAS2562_PowerControl_ISNSPower_Mask  (0x1 << 3)
#define TAS2562_PowerControl_ISNSPower_Active  (0x0 << 3)
#define TAS2562_PowerControl_ISNSPower_PoweredDown  (0x1 << 3)
#define TAS2562_PowerControl_VSNSPower_Mask  (0x1 << 2)
#define TAS2562_PowerControl_VSNSPower_Active  (0x0 << 2)
#define TAS2562_PowerControl_VSNSPower_PoweredDown  (0x1 << 2)
#define TAS2562_PowerControl_OperationalMode10_Mask  (0x3 << 0)
#define TAS2562_PowerControl_OperationalMode10_Active  (0x0 << 0)
#define TAS2562_PowerControl_OperationalMode10_Mute  (0x1 << 0)
#define TAS2562_PowerControl_OperationalMode10_Shutdown  (0x2 << 0)

	/* data format */
#define TAS2562_DATAFORMAT_SHIFT		2
#define TAS2562_DATAFORMAT_I2S			0x0
#define TAS2562_DATAFORMAT_DSP			0x1
#define TAS2562_DATAFORMAT_RIGHT_J		0x2
#define TAS2562_DATAFORMAT_LEFT_J		0x3

#define TAS2562_DAI_FMT_MASK		(0x7 << TAS2562_DATAFORMAT_SHIFT)

    /* Playback Configuration Reg0 */
#define TAS2562_PlaybackConfigurationReg0  TAS2562_REG(0x0, 0x0, 0x03)
#define TAS2562_PlaybackConfigurationReg0_DCBlocker_Mask  (0x1 << 6)
#define TAS2562_PlaybackConfigurationReg0_DCBlocker_Enabled  (0x0 << 6)
#define TAS2562_PlaybackConfigurationReg0_DCBlocker_Disabled  (0x1 << 6)
#define TAS2562_PlaybackConfigurationReg0_AmplifierLevel51_Mask  (0x1f << 1)

    /* Misc Configuration Reg0 */
#define TAS2562_MiscConfigurationReg0  TAS2562_REG(0x0, 0x0, 0x04)
#define TAS2562_MiscConfigurationReg0_CPPGRetry_Mask  (0x1 << 7)
#define TAS2562_MiscConfigurationReg0_CPPGRetry_DoNotRetry  (0x0 << 7)
#define TAS2562_MiscConfigurationReg0_CPPGRetry_Retry  (0x1 << 7)
#define TAS2562_MiscConfigurationReg0_VBATPRORetry_Mask  (0x1 << 6)
#define TAS2562_MiscConfigurationReg0_VBATPRORetry_DoNotRetry  (0x0 << 6)
#define TAS2562_MiscConfigurationReg0_VBATPRORetry_Retry  (0x1 << 6)
#define TAS2562_MiscConfigurationReg0_OCERetry_Mask  (0x1 << 5)
#define TAS2562_MiscConfigurationReg0_OCERetry_DoNotRetry  (0x0 << 5)
#define TAS2562_MiscConfigurationReg0_OCERetry_Retry  (0x1 << 5)
#define TAS2562_MiscConfigurationReg0_OTERetry_Mask  (0x1 << 4)
#define TAS2562_MiscConfigurationReg0_OTERetry_DoNotRetry  (0x0 << 4)
#define TAS2562_MiscConfigurationReg0_OTERetry_Retry  (0x1 << 4)
#define TAS2562_MiscConfigurationReg0_IRQZPull_Mask  (0x1 << 3)
#define TAS2562_MiscConfigurationReg0_IRQZPull_Disabled  (0x0 << 3)
#define TAS2562_MiscConfigurationReg0_IRQZPull_Enabled  (0x1 << 3)
#define TAS2562_MiscConfigurationReg0_AMPSS_Mask  (0x1 << 2)
#define TAS2562_MiscConfigurationReg0_AMPSS_Disabled  (0x0 << 2)
#define TAS2562_MiscConfigurationReg0_AMPSS_Enabled  (0x1 << 2)

    /* TDM Configuration Reg0 */
#define TAS2562_TDMConfigurationReg0  TAS2562_REG(0x0, 0x0, 0x06)
#define TAS2562_TDMConfigurationReg0_SAMPRATERAMP_Mask  (0x1 << 5)
#define TAS2562_TDMConfigurationReg0_SAMPRATERAMP_48KHz  (0x0 << 5)
#define TAS2562_TDMConfigurationReg0_SAMPRATERAMP_44_1KHz  (0x1 << 5)
#define TAS2562_TDMConfigurationReg0_DETECTSAMPRATE_Mask  (0x1 << 4)
#define TAS2562_TDMConfigurationReg0_DETECTSAMPRATE_Disabled  (0x1 << 4)
#define TAS2562_TDMConfigurationReg0_DETECTSAMPRATE_Enabled  (0x0 << 4)
#define TAS2562_TDMConfigurationReg0_SAMPRATE31_Mask  (0x7 << 1)
#define TAS2562_TDMConfigurationReg0_SAMPRATE31_14_7_16kHz  (0x1 << 1)
#define TAS2562_TDMConfigurationReg0_SAMPRATE31_44_1_48kHz  (0x4 << 1)
#define TAS2562_TDMConfigurationReg0_SAMPRATE31_88_2_96kHz  (0x5 << 1)
#define TAS2562_TDMConfigurationReg0_SAMPRATE31_176_4_192kHz  (0x6 << 1)
#define TAS2562_TDMConfigurationReg0_FRAMESTART_Mask  (0x1 << 0)
#define TAS2562_TDMConfigurationReg0_FRAMESTART_LowToHigh  (0x0 << 0)
#define TAS2562_TDMConfigurationReg0_FRAMESTART_HighToLow  (0x1 << 0)

    /* TDM Configuration Reg1 */
#define TAS2562_TDMConfigurationReg1  TAS2562_REG(0x0, 0x0, 0x07)
#define TAS2562_TDMConfigurationReg1_RXJUSTIFY_Mask  (0x1 << 6)
#define TAS2562_TDMConfigurationReg1_RXJUSTIFY_Left  (0x0 << 6)
#define TAS2562_TDMConfigurationReg1_RXJUSTIFY_Right  (0x1 << 6)
#define TAS2562_TDMConfigurationReg1_RXOFFSET51_Mask  (0x1f << 1)
#define TAS2562_TDMConfigurationReg1_RXOFFSET51_Shift (1)
#define TAS2562_TDMConfigurationReg1_RXEDGE_Mask  (0x1 << 0)
#define TAS2562_TDMConfigurationReg1_RXEDGE_Rising  (0x0 << 0)
#define TAS2562_TDMConfigurationReg1_RXEDGE_Falling  (0x1 << 0)

    /* TDM Configuration Reg2 */
#define TAS2562_TDMConfigurationReg2  TAS2562_REG(0x0, 0x0, 0x08)
#define TAS2562_TDMConfigurationReg2_RXSCFG54_Mask  (0x3 << 4)
#define TAS2562_TDMConfigurationReg2_RXSCFG54_Mono_I2C  (0x0 << 4),
#define TAS2562_TDMConfigurationReg2_RXSCFG54_Mono_Left  (0x1 << 4),
#define TAS2562_TDMConfigurationReg2_RXSCFG54_Mono_Right  (0x2 << 4)
#define TAS2562_TDMConfigurationReg2_RXSCFG54_Stereo_DownMix  (0x3 << 4)
#define TAS2562_TDMConfigurationReg2_RXWLEN32_Mask  (0x3 << 2)
#define TAS2562_TDMConfigurationReg2_RXWLEN32_16Bits  (0x0 << 2)
#define TAS2562_TDMConfigurationReg2_RXWLEN32_20Bits  (0x1 << 2)
#define TAS2562_TDMConfigurationReg2_RXWLEN32_24Bits  (0x2 << 2)
#define TAS2562_TDMConfigurationReg2_RXWLEN32_32Bits  (0x3 << 2)
#define TAS2562_TDMConfigurationReg2_RXSLEN10_Mask  (0x3 << 0)
#define TAS2562_TDMConfigurationReg2_RXSLEN10_16Bits  (0x0 << 0)
#define TAS2562_TDMConfigurationReg2_RXSLEN10_24Bits  (0x1 << 0)
#define TAS2562_TDMConfigurationReg2_RXSLEN10_32Bits  (0x2 << 0)

    /* TDM Configuration Reg3 */
#define TAS2562_TDMConfigurationReg3  TAS2562_REG(0x0, 0x0, 0x09)
#define TAS2562_TDMConfigurationReg3_RXSLOTRight74_Mask  (0xf << 4)
#define TAS2562_TDMConfigurationReg3_RXSLOTLeft30_Mask  (0xf << 0)

    /* TDM Configuration Reg4 */
#define TAS2562_TDMConfigurationReg4  TAS2562_REG(0x0, 0x0, 0x0A)
#define TAS2562_TDMConfigurationReg4_TXKEEPER_Mask  (0x1 << 5)
#define TAS2562_TDMConfigurationReg4_TXKEEPER_Disable  (0x0 << 5)
#define TAS2562_TDMConfigurationReg4_TXKEEPER_Enable  (0x1 << 5)
#define TAS2562_TDMConfigurationReg4_TXFILL_Mask  (0x1 << 4)
#define TAS2562_TDMConfigurationReg4_TXFILL_Transmit0  (0x0 << 4)
#define TAS2562_TDMConfigurationReg4_TXFILL_TransmitHiz  (0x1 << 4)
#define TAS2562_TDMConfigurationReg4_TXOFFSET31_Mask  (0x7 << 1)
#define TAS2562_TDMConfigurationReg4_TXEDGE_Mask  (0x1 << 0)
#define TAS2562_TDMConfigurationReg4_TXEDGE_Rising  (0x0 << 0)
#define TAS2562_TDMConfigurationReg4_TXEDGE_Falling  (0x1 << 0)

    /* TDM Configuration Reg5 */
#define TAS2562_TDMConfigurationReg5  TAS2562_REG(0x0, 0x0, 0x0B)
#define TAS2562_TDMConfigurationReg5_VSNSTX_Mask  (0x1 << 6)
#define TAS2562_TDMConfigurationReg5_VSNSTX_Disable  (0x0 << 6),
#define TAS2562_TDMConfigurationReg5_VSNSTX_Enable  (0x1 << 6),
#define TAS2562_TDMConfigurationReg5_VSNSSLOT50_Mask  (0x3f << 0)

    /* TDM Configuration Reg6 */
#define TAS2562_TDMConfigurationReg6  TAS2562_REG(0x0, 0x0, 0x0C)
#define TAS2562_TDMConfigurationReg6_ISNSTX_Mask  (0x1 << 6)
#define TAS2562_TDMConfigurationReg6_ISNSTX_Disable  (0x0 << 6),
#define TAS2562_TDMConfigurationReg6_ISNSTX_Enable  (0x1 << 6),
#define TAS2562_TDMConfigurationReg6_ISNSSLOT50_Mask  (0x3f << 0)

    /* TDM Configuration Reg7 */
#define TAS2562_TDMConfigurationReg7  TAS2562_REG(0x0, 0x0, 0x0D)
#define TAS2562_TDMConfigurationReg7_VBATSLEN_Mask  (0x1 << 7)
#define TAS2562_TDMConfigurationReg7_VBATSLEN_8Bits  (0x0 << 7)
#define TAS2562_TDMConfigurationReg7_VBATSLEN_16Bits  (0x1 << 7)
#define TAS2562_TDMConfigurationReg7_VBATTX_Mask  (0x1 << 6)
#define TAS2562_TDMConfigurationReg7_VBATTX_Disable  (0x0 << 6)
#define TAS2562_TDMConfigurationReg7_VBATTX_Enable  (0x1 << 6)
#define TAS2562_TDMConfigurationReg7_VBATSLOT50_Mask  (0x3f << 0)

    /* TDM Configuration Reg8 */
#define TAS2562_TDMConfigurationReg8  TAS2562_REG(0x0, 0x0, 0x0E)
#define TAS2562_TDMConfigurationReg8_TEMPTX_Mask  (0x1 << 6)
#define TAS2562_TDMConfigurationReg8_TEMPTX_Disable  (0x0 << 6)
#define TAS2562_TDMConfigurationReg8_TEMPTX_Enable  (0x1 << 6)
#define TAS2562_TDMConfigurationReg8_TEMPSLOT50_Mask  (0x3f << 0)

    /* TDM Configuration Reg9 */
#define TAS2562_TDMConfigurationReg9  TAS2562_REG(0x0, 0x0, 0x0F)
#define TAS2562_TDMConfigurationReg9_GAINTX_Mask  (0x1 << 6)
#define TAS2562_TDMConfigurationReg9_GAINTX_Disable  (0x0 << 6)
#define TAS2562_TDMConfigurationReg9_GAINTX_Enable  (0x1 << 6)
#define TAS2562_TDMConfigurationReg9_GAINSLOT50_Mask  (0x3f << 0)

    /* Limiter Configuration Reg0 */
#define TAS2562_LimiterConfigurationReg0  TAS2562_REG(0x0, 0x0, 0x12)
#define TAS2562_LimiterConfigurationReg0_LIMATKST54_Mask  (0x3 << 4)
#define TAS2562_LimiterConfigurationReg0_LIMATKST54_1  (0x2 << 4)
#define TAS2562_LimiterConfigurationReg0_LIMATKST54_2  (0x3 << 4)
#define TAS2562_LimiterConfigurationReg0_LIMATKST54_0_25  (0x0 << 4)
#define TAS2562_LimiterConfigurationReg0_LIMATKST54_0_5  (0x1 << 4)
#define TAS2562_LimiterConfigurationReg0_LIMATKRT31_Mask  (0x7 << 1)
#define TAS2562_LimiterConfigurationReg0_LIMATKRT31_5  (0x0 << 1)
#define TAS2562_LimiterConfigurationReg0_LIMATKRT31_10  (0x1 << 1)
#define TAS2562_LimiterConfigurationReg0_LIMATKRT31_20  (0x2 << 1)
#define TAS2562_LimiterConfigurationReg0_LIMATKRT31_40  (0x3 << 1)
#define TAS2562_LimiterConfigurationReg0_LIMATKRT31_80  (0x4 << 1)
#define TAS2562_LimiterConfigurationReg0_LIMATKRT31_160  (0x5 << 1)
#define TAS2562_LimiterConfigurationReg0_LIMATKRT31_320  (0x6 << 1)
#define TAS2562_LimiterConfigurationReg0_LIMATKRT31_640  (0x7 << 1)
#define TAS2562_LimiterConfigurationReg0_LIMEN_Mask  (0x1 << 0)
#define TAS2562_LimiterConfigurationReg0_LIMEN_Disabled  (0x0 << 0)
#define TAS2562_LimiterConfigurationReg0_LIMEN_Enabled  (0x1 << 0)

    /* Limiter Configuration Reg1 */
#define TAS2562_LimiterConfigurationReg1  TAS2562_REG(0x0, 0x0, 0x13)
#define TAS2562_LimiterConfigurationReg1_LIMRLSST76_Mask  (0x3 << 6)
#define TAS2562_LimiterConfigurationReg1_LIMRLSST76_1  (0x2 << 6)
#define TAS2562_LimiterConfigurationReg1_LIMRLSST76_2  (0x3 << 6)
#define TAS2562_LimiterConfigurationReg1_LIMRLSST76_0_25  (0x0 << 6)
#define TAS2562_LimiterConfigurationReg1_LIMRLSST76_0_5  (0x1 << 6)
#define TAS2562_LimiterConfigurationReg1_LIMRLSRT53_Mask  (0x7 << 3)
#define TAS2562_LimiterConfigurationReg1_LIMRLSRT53_10  (0x0 << 3)
#define TAS2562_LimiterConfigurationReg1_LIMRLSRT53_50  (0x1 << 3)
#define TAS2562_LimiterConfigurationReg1_LIMRLSRT53_100  (0x2 << 3)
#define TAS2562_LimiterConfigurationReg1_LIMRLSRT53_250  (0x3 << 3)
#define TAS2562_LimiterConfigurationReg1_LIMRLSRT53_500  (0x4 << 3)
#define TAS2562_LimiterConfigurationReg1_LIMRLSRT53_750  (0x5 << 3)
#define TAS2562_LimiterConfigurationReg1_LIMRLSRT53_1000  (0x6 << 3)
#define TAS2562_LimiterConfigurationReg1_LIMRLSRT53_1500  (0x7 << 3)
#define TAS2562_LimiterConfigurationReg1_LIMHLDTM20_Mask  (0x7 << 0)
#define TAS2562_LimiterConfigurationReg1_LIMHLDTM20_0  (0x0 << 0)
#define TAS2562_LimiterConfigurationReg1_LIMHLDTM20_10  (0x1 << 0)
#define TAS2562_LimiterConfigurationReg1_LIMHLDTM20_25  (0x2 << 0)
#define TAS2562_LimiterConfigurationReg1_LIMHLDTM20_50  (0x3 << 0)
#define TAS2562_LimiterConfigurationReg1_LIMHLDTM20_100  (0x4 << 0)
#define TAS2562_LimiterConfigurationReg1_LIMHLDTM20_250  (0x5 << 0)
#define TAS2562_LimiterConfigurationReg1_LIMHLDTM20_500  (0x6 << 0)
#define TAS2562_LimiterConfigurationReg1_LIMHLDTM20_1000  (0x7 << 0)

    /* Brown Out Prevention Reg0 */
#define TAS2562_BrownOutPreventionReg0  TAS2562_REG(0x0, 0x0, 0x14)
#define TAS2562_BrownOutPreventionReg0_BOPSDEN_Mask  (0x1 << 4)
#define TAS2562_BrownOutPreventionReg0_BOPSDEN_Disabled  (0x0 << 4)
#define TAS2562_BrownOutPreventionReg0_BOPSDEN_Enabled  (0x1 << 4)
#define TAS2562_BrownOutPreventionReg0_BOPHLDCLR_Mask  (0x1 << 3)
#define TAS2562_BrownOutPreventionReg0_BOPHLDCLR_DontClear  (0x0 << 3)
#define TAS2562_BrownOutPreventionReg0_BOPHLDCLR_Clear  (0x1 << 3)
#define TAS2562_BrownOutPreventionReg0_BOPINFHLD_Mask  (0x1 << 2)
#define TAS2562_BrownOutPreventionReg0_BOPINFHLD_UseHoldTime  (0x0 << 2)
#define TAS2562_BrownOutPreventionReg0_BOPINFHLD_HoldUntilCleared  (0x1 << 2)
#define TAS2562_BrownOutPreventionReg0_BOPMUTE_Mask  (0x1 << 1)
#define TAS2562_BrownOutPreventionReg0_BOPMUTE_DoNotMute  (0x0 << 1)
#define TAS2562_BrownOutPreventionReg0_BOPMUTE_Mute  (0x1 << 1)
#define TAS2562_BrownOutPreventionReg0_BOPEN_Mask  (0x1 << 0)
#define TAS2562_BrownOutPreventionReg0_BOPEN_Disabled  (0x0 << 0)
#define TAS2562_BrownOutPreventionReg0_BOPEN_Enabled  (0x1 << 0)

    /* Brown Out Prevention Reg1 */
#define TAS2562_BrownOutPreventionReg1  TAS2562_REG(0x0, 0x0, 0x15)
#define TAS2562_BrownOutPreventionReg1_BOPATKRT75_Mask  (0x7 << 5)
#define TAS2562_BrownOutPreventionReg1_BOPATKRT75_1  (0x0 << 5)
#define TAS2562_BrownOutPreventionReg1_BOPATKRT75_2  (0x1 << 5)
#define TAS2562_BrownOutPreventionReg1_BOPATKRT75_4  (0x2 << 5)
#define TAS2562_BrownOutPreventionReg1_BOPATKRT75_8  (0x3 << 5)
#define TAS2562_BrownOutPreventionReg1_BOPATKRT75_16  (0x4 << 5)
#define TAS2562_BrownOutPreventionReg1_BOPATKRT75_32  (0x5 << 5)
#define TAS2562_BrownOutPreventionReg1_BOPATKRT75_64  (0x6 << 5)
#define TAS2562_BrownOutPreventionReg1_BOPATKRT75_128  (0x7 << 5)
#define TAS2562_BrownOutPreventionReg1_BOPATKST43_Mask  (0x3 << 3)
#define TAS2562_BrownOutPreventionReg1_BOPATKST43_1  (0x1 << 3)
#define TAS2562_BrownOutPreventionReg1_BOPATKST43_2  (0x3 << 3)
#define TAS2562_BrownOutPreventionReg1_BOPATKST43_0_5  (0x0 << 3)
#define TAS2562_BrownOutPreventionReg1_BOPATKST43_1_5  (0x2 << 3)
#define TAS2562_BrownOutPreventionReg1_BOPHLDTM20_Mask  (0x7 << 0)
#define TAS2562_BrownOutPreventionReg1_BOPHLDTM20_0  (0x0 << 0)
#define TAS2562_BrownOutPreventionReg1_BOPHLDTM20_10  (0x1 << 0)
#define TAS2562_BrownOutPreventionReg1_BOPHLDTM20_25  (0x2 << 0)
#define TAS2562_BrownOutPreventionReg1_BOPHLDTM20_50  (0x3 << 0)
#define TAS2562_BrownOutPreventionReg1_BOPHLDTM20_100  (0x4 << 0)
#define TAS2562_BrownOutPreventionReg1_BOPHLDTM20_250  (0x5 << 0)
#define TAS2562_BrownOutPreventionReg1_BOPHLDTM20_500  (0x6 << 0)
#define TAS2562_BrownOutPreventionReg1_BOPHLDTM20_1000  (0x7 << 0

    /* Interrupt Mask Reg0 */
#define TAS2562_InterruptMaskReg0  TAS2562_REG(0x0, 0x0, 0x1A)
#define TAS2562_InterruptMaskReg0_LIMMUTEINTMASK_Mask  (0x1 << 7)
#define TAS2562_InterruptMaskReg0_LIMMUTEINTMASK_Unmask  (0x0 << 7)
#define TAS2562_InterruptMaskReg0_LIMMUTEINTMASK_Disable  (0x1 << 7)
#define TAS2562_InterruptMaskReg0_LIMINFHLDINTMASK_Mask  (0x1 << 6)
#define TAS2562_InterruptMaskReg0_LIMINFHLDINTMASK_Unmask  (0x0 << 6)
#define TAS2562_InterruptMaskReg0_LIMINFHLDINTMASK_Disable  (0x1 << 6)
#define TAS2562_InterruptMaskReg0_LIMMAXATNINTMASK_Mask  (0x1 << 5)
#define TAS2562_InterruptMaskReg0_LIMMAXATNINTMASK_Unmask  (0x0 << 5)
#define TAS2562_InterruptMaskReg0_LIMMAXATNINTMASK_Disable  (0x1 << 5)
#define TAS2562_InterruptMaskReg0_VBATLessthanINFINTMASK_Mask  (0x1 << 4)
#define TAS2562_InterruptMaskReg0_VBATLessthanINFINTMASK_Unmask  (0x0 << 4)
#define TAS2562_InterruptMaskReg0_VBATLessthanINFINTMASK_Disable  (0x1 << 4)
#define TAS2562_InterruptMaskReg0_LIMActiveFlagINTMASK_Mask  (0x1 << 3)
#define TAS2562_InterruptMaskReg0_LIMActiveFlagINTMASK_Unmask  (0x0 << 3)
#define TAS2562_InterruptMaskReg0_LIMActiveFlagINTMASK_Disable  (0x1 << 3)
#define TAS2562_InterruptMaskReg0_TDMClockErrorINTMASK_Mask  (0x1 << 2)
#define TAS2562_InterruptMaskReg0_TDMClockErrorINTMASK_Unmask  (0x0 << 2)
#define TAS2562_InterruptMaskReg0_TDMClockErrorINTMASK_Disable  (0x1 << 2)
#define TAS2562_InterruptMaskReg0_OCEINTMASK_Mask  (0x1 << 1)
#define TAS2562_InterruptMaskReg0_OCEINTMASK_Unmask  (0x0 << 1)
#define TAS2562_InterruptMaskReg0_OCEINTMASK_Disable  (0x1 << 1)
#define TAS2562_InterruptMaskReg0_OTEINTMASK_Mask  (0x1 << 0)
#define TAS2562_InterruptMaskReg0_OTEINTMASK_Unmask  (0x0 << 0)
#define TAS2562_InterruptMaskReg0_OTEINTMASK_Disable  (0x1 << 0)
#define TAS2562_InterruptMaskReg0_Disable 0xff

    /* Interrupt Mask Reg1 */
#define TAS2562_InterruptMaskReg1  TAS2562_REG(0x0, 0x0, 0x1B)
#define TAS2562_InterruptMaskReg1_DSPOUTPUTINTMASK_Mask  (0x1 << 7)
#define TAS2562_InterruptMaskReg1_DSPOUTPUTINTMASK_Unmask  (0x0 << 7)
#define TAS2562_InterruptMaskReg1_DSPOUTPUTINTMASK_Disable  (0x1 << 7)
#define TAS2562_InterruptMaskReg1_CRCINTMASK_Mask  (0x1 << 6)
#define TAS2562_InterruptMaskReg1_CRCINTMASK_Unmask  (0x0 << 6)
#define TAS2562_InterruptMaskReg1_CRCINTMASK_Disable  (0x1 << 6)
#define TAS2562_InterruptMaskReg1_VBATOVLOINTMASK_Mask  (0x1 << 2)
#define TAS2562_InterruptMaskReg1_VBATOVLOINTMASK_Unmask  (0x0 << 2)
#define TAS2562_InterruptMaskReg1_VBATOVLOINTMASK_Disable  (0x1 << 2)
#define TAS2562_InterruptMaskReg1_VBATUVLOINTMASK_Mask  (0x1 << 1)
#define TAS2562_InterruptMaskReg1_VBATUVLOINTMASK_Unmask  (0x0 << 1)
#define TAS2562_InterruptMaskReg1_VBATUVLOINTMASK_Disable  (0x1 << 1)
#define TAS2562_InterruptMaskReg1_BrownOutFlagINTMASK_Mask  (0x1 << 0)
#define TAS2562_InterruptMaskReg1_BrownOutFlagINTMASK_Unmask  (0x0 << 0)
#define TAS2562_InterruptMaskReg1_BrownOutFlagINTMASK_Disable  (0x1 << 0)
#define TAS2562_InterruptMaskReg1_Disable 0xff

    /* Interrupt Mask Reg2 */
#define TAS2562_InterruptMaskReg2  TAS2562_REG(0x0, 0x0, 0x1C)
#define TAS2562_InterruptMaskReg2_DACLKINTMASK_Mask  (0x1 << 7)
#define TAS2562_InterruptMaskReg2_DACLKINTMASK_Unmask  (0x0 << 7)
#define TAS2562_InterruptMaskReg2_DACLKINTMASK_Disable  (0x1 << 7)
#define TAS2562_InterruptMaskReg2_BSTCLKINTMASK_Mask  (0x1 << 6)
#define TAS2562_InterruptMaskReg2_BSTCLKINTMASK_Unmask  (0x0 << 6)
#define TAS2562_InterruptMaskReg2_BSTCLKINTMASK_Disable  (0x1 << 6)
#define TAS2562_InterruptMaskReg2_VBATPORCLKINTMASK_Mask  (0x1 << 5)
#define TAS2562_InterruptMaskReg2_VBATPORCLKINTMASK_Unmask  (0x0 << 5)
#define TAS2562_InterruptMaskReg2_VBATPORCLKINTMASK_Disable  (0x1 << 5)
#define TAS2562_InterruptMaskReg2_PLLOCKINTMASK_Mask  (0x1 << 4)
#define TAS2562_InterruptMaskReg2_PLLOCKINTMASK_Unmask  (0x0 << 4)
#define TAS2562_InterruptMaskReg2_PLLOCKINTMASK_Disable  (0x1 << 4)
#define TAS2562_InterruptMaskReg2_DCDETECTINTMASK_Mask  (0x1 << 3)
#define TAS2562_InterruptMaskReg2_DCDETECTINTMASK_Unmask  (0x0 << 3)
#define TAS2562_InterruptMaskReg2_DCDETECTINTMASK_Disable  (0x1 << 3)

    /* Live-Interrupt Reg0 */
#define TAS2562_LiveInterruptReg0  TAS2562_REG(0x0, 0x0, 0x1F)
#define TAS2562_LiveInterruptReg0_LIMMUTE_Mask  (0x1 << 7)
#define TAS2562_LiveInterruptReg0_LIMMUTE_NoInterrupt  (0x0 << 7)
#define TAS2562_LiveInterruptReg0_LIMMUTE_Interrupt  (0x1 << 7)
#define TAS2562_LiveInterruptReg0_LIMINFHLD_Mask  (0x1 << 6)
#define TAS2562_LiveInterruptReg0_LIMINFHLD_NoInterrupt  (0x0 << 6)
#define TAS2562_LiveInterruptReg0_LIMINFHLD_Interrupt  (0x1 << 6)
#define TAS2562_LiveInterruptReg0_LIMMAXATN_Mask  (0x1 << 5)
#define TAS2562_LiveInterruptReg0_LIMMAXATN_NoInterrupt  (0x0 << 5)
#define TAS2562_LiveInterruptReg0_LIMMAXATN_Interrupt  (0x1 << 5)
#define TAS2562_LiveInterruptReg0_VBATLessthanINF_Mask  (0x1 << 4)
#define TAS2562_LiveInterruptReg0_VBATLessthanINF_NoInterrupt  (0x0 << 4)
#define TAS2562_LiveInterruptReg0_VBATLessthanINF_Interrupt  (0x1 << 4)
#define TAS2562_LiveInterruptReg0_LIMActiveFlag_Mask  (0x1 << 3)
#define TAS2562_LiveInterruptReg0_LIMActiveFlag_NoInterrupt  (0x0 << 3)
#define TAS2562_LiveInterruptReg0_LIMActiveFlag_Interrupt  (0x1 << 3)
#define TAS2562_LiveInterruptReg0_TDMClockError_Mask  (0x1 << 2)
#define TAS2562_LiveInterruptReg0_TDMClockError_NoInterrupt  (0x0 << 2)
#define TAS2562_LiveInterruptReg0_TDMClockError_Interrupt  (0x1 << 2)
#define TAS2562_LiveInterruptReg0_OCEFlag_Mask  (0x1 << 1)
#define TAS2562_LiveInterruptReg0_OCEFlag_NoInterrupt  (0x0 << 1)
#define TAS2562_LiveInterruptReg0_OCEFlag_Interrupt  (0x1 << 1)
#define TAS2562_LiveInterruptReg0_OTEFlag_Mask  (0x1 << 0)
#define TAS2562_LiveInterruptReg0_OTEFlag_NoInterrupt  (0x0 << 0)
#define TAS2562_LiveInterruptReg0_OTEFlag_Interrupt  (0x1 << 0)

    /* Live-Interrupt Reg1 */
#define TAS2562_LiveInterruptReg1  TAS2562_REG(0x0, 0x0, 0x20)
#define TAS2562_LiveInterruptReg1_DSPINTOutput_Mask  (0x1 << 7)
#define TAS2562_LiveInterruptReg1_DSPINTOutput_NoInterrupt  (0x0 << 7)
#define TAS2562_LiveInterruptReg1_DSPINTOutput_Interrupt  (0x1 << 7)
#define TAS2562_LiveInterruptReg1_OTPCRC_Mask  (0x1 << 6)
#define TAS2562_LiveInterruptReg1_OTPCRC_NoInterrupt  (0x0 << 6)
#define TAS2562_LiveInterruptReg1_OTPCRC_Interrupt  (0x1 << 6)
#define TAS2562_LiveInterruptReg1_BrownOutFlag_Mask  (0x1 << 1)
#define TAS2562_LiveInterruptReg1_BrownOutFlag_NoInterrupt  (0x0 << 1)
#define TAS2562_LiveInterruptReg1_BrownOutFlag_Interrupt  (0x1 << 1)
#define TAS2562_LiveInterruptReg1_BrownOutDetected_Mask  (0x1 << 1)
#define TAS2562_LiveInterruptReg1_BrownOutDetected_NoInterrupt  (0x0 << 1)
#define TAS2562_LiveInterruptReg1_BrownOutDetected_Interrupt  (0x1 << 1)

    /* Latched-Interrupt Reg0 */
#define TAS2562_LatchedInterruptReg0  TAS2562_REG(0x0, 0x0, 0x24)
#define TAS2562_LatchedInterruptReg0_LIMMUTESticky_Mask  (0x1 << 7)
#define TAS2562_LatchedInterruptReg0_LIMMUTESticky_NoInterrupt  (0x0 << 7)
#define TAS2562_LatchedInterruptReg0_LIMMUTESticky_Interrupt  (0x1 << 7)
#define TAS2562_LatchedInterruptReg0_LIMINFHLDSticky_Mask  (0x1 << 6)
#define TAS2562_LatchedInterruptReg0_LIMINFHLDSticky_NoInterrupt  (0x0 << 6)
#define TAS2562_LatchedInterruptReg0_LIMINFHLDSticky_Interrupt  (0x1 << 6)
#define TAS2562_LatchedInterruptReg0_LIMMAXATNSticky_Mask  (0x1 << 5)
#define TAS2562_LatchedInterruptReg0_LIMMAXATNSticky_NoInterrupt  (0x0 << 5)
#define TAS2562_LatchedInterruptReg0_LIMMAXATNSticky_Interrupt  (0x1 << 5)
#define TAS2562_LatchedInterruptReg0_VBATLessthanINFSticky_Mask  (0x1 << 4)
#define TAS2562_LatchedInterruptReg0_VBATLessthanINFSticky_NoInterrupt \
	(0x0 << 4)
#define TAS2562_LatchedInterruptReg0_VBATLessthanINFSticky_Interrupt  (0x1 << 4)
#define TAS2562_LatchedInterruptReg0_LIMActiveFlagSticky_Mask  (0x1 << 3)
#define TAS2562_LatchedInterruptReg0_LIMActiveFlagSticky_NoInterrupt  (0x0 << 3)
#define TAS2562_LatchedInterruptReg0_LIMActiveFlagSticky_Interrupt  (0x1 << 3)
#define TAS2562_LatchedInterruptReg0_TDMClockErrorSticky_Mask  (0x1 << 2)
#define TAS2562_LatchedInterruptReg0_TDMClockErrorSticky_NoInterrupt  (0x0 << 2)
#define TAS2562_LatchedInterruptReg0_TDMClockErrorSticky_Interrupt  (0x1 << 2)
#define TAS2562_LatchedInterruptReg0_OCEFlagSticky_Mask  (0x1 << 1)
#define TAS2562_LatchedInterruptReg0_OCEFlagSticky_NoInterrupt  (0x0 << 1)
#define TAS2562_LatchedInterruptReg0_OCEFlagSticky_Interrupt  (0x1 << 1)
#define TAS2562_LatchedInterruptReg0_OTEFlagSticky_Mask  (0x1 << 0)
#define TAS2562_LatchedInterruptReg0_OTEFlagSticky_NoInterrupt  (0x0 << 0)
#define TAS2562_LatchedInterruptReg0_OTEFlagSticky_Interrupt  (0x1 << 0)

    /* Latched-Interrupt Reg1 */
#define TAS2562_LatchedInterruptReg1  TAS2562_REG(0x0, 0x0, 0x25)
#define TAS2562_LatchedInterruptReg1_PDMAUDDATAINVALIDSticky_Mask  (0x1 << 7)
#define TAS2562_LatchedInterruptReg1_PDMAUDDATAINVALIDSticky_NoInterrupt \
	(0x0 << 7)
#define TAS2562_LatchedInterruptReg1_PDMAUDDATAINVALIDSticky_Interrupt \
	(0x1 << 7)
#define TAS2562_LatchedInterruptReg1_VBATOVLOSticky_Mask  (0x1 << 3)
#define TAS2562_LatchedInterruptReg1_VBATOVLOSticky_NoInterrupt  (0x0 << 3)
#define TAS2562_LatchedInterruptReg1_VBATOVLOSticky_Interrupt  (0x1 << 3)
#define TAS2562_LatchedInterruptReg1_VBATUVLOSticky_Mask  (0x1 << 2)
#define TAS2562_LatchedInterruptReg1_VBATUVLOSticky_NoInterrupt  (0x0 << 2)
#define TAS2562_LatchedInterruptReg1_VBATUVLOSticky_Interrupt  (0x1 << 2)
#define TAS2562_LatchedInterruptReg1_BrownOutFlagSticky_Mask  (0x1 << 1)
#define TAS2562_LatchedInterruptReg1_BrownOutFlagSticky_NoInterrupt  (0x0 << 1)
#define TAS2562_LatchedInterruptReg1_BrownOutFlagSticky_Interrupt  (0x1 << 1)
#define TAS2562_LatchedInterruptReg1_PDMClockErrorSticky_Mask  (0x1 << 0)
#define TAS2562_LatchedInterruptReg1_PDMClockErrorSticky_NoInterrupt  (0x0 << 0)
#define TAS2562_LatchedInterruptReg1_PDMClockErrorSticky_Interrupt  (0x1 << 0)

#define TAS2562_LatchedInterruptReg2  TAS2562_REG(0x0, 0x0, 0x28)
#define TAS2562_LatchedInterruptReg3  TAS2562_REG(0x0, 0x0, 0x26)
#define TAS2562_LatchedInterruptReg4  TAS2562_REG(0x0, 0x0, 0x27)

    /* VBAT MSB */
#define TAS2562_VBATMSB  TAS2562_REG(0x0, 0x0, 0x2A)
#define TAS2562_VBATMSB_VBATMSB70_Mask  (0xff << 0)

    /* VBAT LSB */
#define TAS2562_VBATLSB  TAS2562_REG(0x0, 0x0, 0x2B)
#define TAS2562_VBATLSB_VBATLSB74_Mask  (0xf << 4)

    /* TEMP */
#define TAS2562_TEMP  TAS2562_REG(0x0, 0x0, 0x2C)
#define TAS2562_TEMP_TEMPMSB70_Mask  (0xff << 0)


    /* Interrupt Configuration */
#define TAS2562_InterruptConfiguration  TAS2562_REG(0x0, 0x0, 0x30)
#define TAS2562_InterruptConfiguration_LTCHINTClear_Mask (0x1 << 2)
#define TAS2562_InterruptConfiguration_LTCHINTClear (0x1 << 2)
#define TAS2562_InterruptConfiguration_PININTConfig10_Mask  (0x3 << 0)
#define TAS2562_InterruptConfiguration_PININTConfig10_AssertOnLiveInterrupts \
	(0x0 << 0)
#define \
TAS2562_InterruptConfiguration_PININTConfig10_AssertOnLatchedInterrupts \
	(0x1 << 0)
#define \
TAS2562_InterruptConfiguration_PININTConfig10_Assert2msOnLiveInterrupts \
	(0x2 << 0)
#define \
TAS2562_InterruptConfiguration_PININTConfig10_Assert2msOnLatchedInterrupts \
	(0x3 << 0)

    /* Digital Input Pin Pull Down */
#define TAS2562_DigitalInputPinPullDown  TAS2562_REG(0x0, 0x0, 0x31)
#define TAS2562_DigitalInputPinPullDown_WKPulldownSDOUT_Mask  (0x1 << 7)
#define TAS2562_DigitalInputPinPullDown_WKPulldownSDOUT_Disabled  (0x0 << 7)
#define TAS2562_DigitalInputPinPullDown_WKPulldownSDOUT_Enabled  (0x1 << 7)
#define TAS2562_DigitalInputPinPullDown_WKPulldownSDIN_Mask  (0x1 << 6)
#define TAS2562_DigitalInputPinPullDown_WKPulldownSDIN_Disabled  (0x0 << 6)
#define TAS2562_DigitalInputPinPullDown_WKPulldownSDIN_Enabled  (0x1 << 6)
#define TAS2562_DigitalInputPinPullDown_WKPulldownFSYNC_Mask  (0x1 << 5)
#define TAS2562_DigitalInputPinPullDown_WKPulldownFSYNC_Disabled  (0x0 << 5)
#define TAS2562_DigitalInputPinPullDown_WKPulldownFSYNC_Enabled  (0x1 << 5)
#define TAS2562_DigitalInputPinPullDown_WKPulldownSBCLK_Mask  (0x1 << 4)
#define TAS2562_DigitalInputPinPullDown_WKPulldownSBCLK_Disabled  (0x0 << 4)
#define TAS2562_DigitalInputPinPullDown_WKPulldownSBCLK_Enabled  (0x1 << 4)
#define TAS2562_DigitalInputPinPullDown_WKPulldownPDMD0_Mask  (0x1 << 3)
#define TAS2562_DigitalInputPinPullDown_WKPulldownPDMD0_Disabled  (0x0 << 3)
#define TAS2562_DigitalInputPinPullDown_WKPulldownPDMD0_Enabled  (0x1 << 3)
#define TAS2562_DigitalInputPinPullDown_WKPulldownPDMD1_Mask  (0x1 << 2)
#define TAS2562_DigitalInputPinPullDown_WKPulldownPDMD1_Disabled  (0x0 << 2)
#define TAS2562_DigitalInputPinPullDown_WKPulldownPDMD1_Enabled  (0x1 << 2)
#define TAS2562_DigitalInputPinPullDown_WKPulldownPDMCK0_Mask  (0x1 << 1)
#define TAS2562_DigitalInputPinPullDown_WKPulldownPDMCK0_Disabled  (0x0 << 1)
#define TAS2562_DigitalInputPinPullDown_WKPulldownPDMCK0_Enabled  (0x1 << 1)
#define TAS2562_DigitalInputPinPullDown_WKPulldownPDMCK1_Mask  (0x1 << 0)
#define TAS2562_DigitalInputPinPullDown_WKPulldownPDMCK1_Disabled  (0x0 << 0)
#define TAS2562_DigitalInputPinPullDown_WKPulldownPDMCK1_Enabled  (0x1 << 0)

    /* Misc IRQ */
#define TAS2562_MiscIRQ  TAS2562_REG(0x0, 0x0, 0x32)
#define TAS2562_MiscIRQ_IRQZREQD_Mask  (0x1 << 7)
#define TAS2562_MiscIRQ_IRQZREQD_ActiveHigh  (0x0 << 7)
#define TAS2562_MiscIRQ_IRQZREQD_ActiveLow  (0x1 << 7)
#define TAS2562_MiscIRQ_IRQZBITBANG_Mask  (0x1 << 0)
#define TAS2562_MiscIRQ_IRQZBITBANG_IRQZInputBuf0  (0x0 << 0)
#define TAS2562_MiscIRQ_IRQZBITBANG_IRQZInputBuf1  (0x1 << 0)

#define TAS2562_BoostConfiguration2 TAS2562_REG(0x0, 0x0, 0x34)
#define TAS2562_BoostConfiguration2_BoostMaxVoltage_Mask  (0x0f << 0)

#define TAS2562_BoostSlope TAS2562_REG(0x0, 0x0, 0x35)
#define TAS2562_BoostSlope_Mask  	(0x3 << 2)
#define TAS2562_BoostSlope_3AV		(0x1 << 2)
#define TAS2562_BoostSlope_2AV		(0x2 << 2)

    /* Clock Configuration */
#define TAS2562_ClockConfiguration  TAS2562_REG(0x0, 0x0, 0x38)
#define TAS2562_ClockConfiguration_SBCLKtoFS52_Mask  (0xf << 2)
#define TAS2562_ClockConfiguration_SBCLKtoFS52_16  (0x0 << 2)
#define TAS2562_ClockConfiguration_SBCLKtoFS52_24  (0x1 << 2)
#define TAS2562_ClockConfiguration_SBCLKtoFS52_32  (0x2 << 2)
#define TAS2562_ClockConfiguration_SBCLKtoFS52_48  (0x3 << 2)
#define TAS2562_ClockConfiguration_SBCLKtoFS52_64  (0x4 << 2)
#define TAS2562_ClockConfiguration_SBCLKtoFS52_96  (0x5 << 2)
#define TAS2562_ClockConfiguration_SBCLKtoFS52_128  (0x6 << 2)
#define TAS2562_ClockConfiguration_SBCLKtoFS52_192  (0x7 << 2)
#define TAS2562_ClockConfiguration_SBCLKtoFS52_256  (0x8 << 2)
#define TAS2562_ClockConfiguration_SBCLKtoFS52_384  (0x9 << 2)
#define TAS2562_ClockConfiguration_SBCLKtoFS52_512  (0xa << 2)
#define TAS2562_ClockConfiguration_DISCLKRateDetect10_Mask  (0x3 << 0)
#define TAS2562_ClockConfiguration_DISCLKRateDetect10_Disabled  (0x1 << 0)
#define TAS2562_ClockConfiguration_DISCLKRateDetect10_Enabled  (0x0 << 0)

#define TAS2562_VBatFilter TAS2562_REG(0x0, 0x0, 0x3b)
#define TAS2562_ClassHReleaseTimer TAS2562_REG(0x0, 0x0, 0x3c)

#define TAS2562_THERMALFOLD TAS2562_REG(0x0, 0x1, 0x8)

#define TAS2562_ICN_REG TAS2562_REG(0x0, 0x2, 0x64)

#define TAS2562_TestPageConfiguration TAS2562_REG(0x0, 0xfd, 0xd)
#define TAS2562_ClassDConfiguration1	TAS2562_REG(0x0, 0xfd, 0x19)
#define TAS2562_ClassDConfiguration2	TAS2562_REG(0x0, 0xfd, 0x32)
#define TAS2562_ClassDConfiguration3	TAS2562_REG(0x0, 0xfd, 0x33)
#define TAS2562_ClassDConfiguration4	TAS2562_REG(0x0, 0xfd, 0x3f)
#define TAS2562_EfficiencyConfiguration	TAS2562_REG(0x0, 0xfd, 0x5f)

#define TAS2562_ClassHHeadroom TAS2562_REG(0x64, 0x7, 0x48)
#define TAS2562_ClassHHysteresis TAS2562_REG(0x64, 0x7, 0x4c)
#define TAS2562_ClassHMtct TAS2562_REG(0x64, 0x5, 0x4c)

    /* Revision and PG ID */
#define TAS2562_RevisionandPGID  TAS2562_REG(0x0, 0x0, 0x7D)
#define TAS2562_RevisionandPGID_RevisionID74_Mask  (0xf << 4)
#define TAS2562_RevisionandPGID_PGID30_Mask  (0xf << 0)

    /* I2C Checksum */
#define TAS2562_I2CChecksum  TAS2562_REG(0x0, 0x0, 0x7E)
#define TAS2562_I2CChecksum_I2CChecksum70_Mask  (0xff << 0)

    /* Book */
#define TAS2562_Book  TAS2562_REG(0x0, 0x0, 0x7F)
#define TAS2562_Book_Book70_Mask  (0xff << 0)

#define TAS2562_POWER_ACTIVE 0
#define TAS2562_POWER_MUTE 1
#define TAS2562_POWER_SHUTDOWN 2

#define ERROR_NONE		0x0000000
#define ERROR_PLL_ABSENT	0x0000000
#define ERROR_PRAM_CRCCHK	0x0000000
#define ERROR_DTMCLK_ERROR	0x0000001
#define ERROR_OVER_CURRENT	0x0000002
#define ERROR_DIE_OVERTEMP	0x0000004
#define ERROR_OVER_VOLTAGE	0x0000008
#define ERROR_UNDER_VOLTAGE	0x0000010
#define ERROR_BROWNOUT		0x0000020
#define ERROR_CLASSD_PWR	0x0000040
#define ERROR_DEVA_I2C_COMM	0x0000080
#define ERROR_FAILSAFE      0x4000000

#define CHECK_PERIOD	5000	/* 5 second */

#define TAS2562_I2C_RETRY_COUNT     8
#define ERROR_I2C_SUSPEND           -1
#define ERROR_I2C_FAILED            -2

struct tas2562_register {
int book;
int page;
int reg;
};

struct tas2562_dai_cfg {
unsigned int dai_fmt;
unsigned int tdm_delay;
};

struct tas2562_priv {
struct device *dev;
struct regmap *regmap;
struct mutex dev_lock;
struct delayed_work irq_work;
struct delayed_work init_work;
struct hrtimer mtimer;
#ifdef CONFIG_TAS2562_CODEC
struct snd_soc_codec *codec;
#endif
int mnClkin;
int mnClkid;
bool mbPowerUp;
int mnPowerState;
int mnCurrentBook;
int mnCurrentPage;
int mnLoad;
int mnASIFormat;
int mnResetGPIO;
int mnIRQGPIO;
int mnIRQ;
bool mbIRQEnable;
int mnSamplingRate;
int mnFrameSize;
int mnPLL;
int mnPPG;
int mnCh_size;
int mnSlot_width;
int mnPCMFormat;
bool mbMute;
bool i2c_suspend;
int (*read)(struct tas2562_priv *pTAS2562,
	unsigned int reg, unsigned int *pValue);
int (*write)(struct tas2562_priv *pTAS2562,
	unsigned int reg, unsigned int Value);
int (*bulk_read)(struct tas2562_priv *pTAS2562,
	unsigned int reg, unsigned char *pData, unsigned int len);
int (*bulk_write)(struct tas2562_priv *pTAS2562,
	unsigned int reg, unsigned char *pData, unsigned int len);
int (*update_bits)(struct tas2562_priv *pTAS2562,
	unsigned int reg, unsigned int mask, unsigned int value);
void (*hw_reset)(struct tas2562_priv *pTAS2562);
void (*clearIRQ)(struct tas2562_priv *pTAS2562);
void (*enableIRQ)(struct tas2562_priv *pTAS2562, bool enable);
    /* device is working, but system is suspended */
int (*runtime_suspend)(struct tas2562_priv *pTAS2562);
int (*runtime_resume)(struct tas2562_priv *pTAS2562);
bool mbRuntimeSuspend;

unsigned int mnErrCode;
#ifdef CONFIG_TAS2562_CODEC
struct mutex codec_lock;
#endif

#ifdef CONFIG_TAS2562_MISC
int mnDBGCmd;
int mnCurrentReg;
struct mutex file_lock;
#endif
};

#endif /* __TAS2562_ */
