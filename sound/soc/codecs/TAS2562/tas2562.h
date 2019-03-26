
#ifndef __TAS2562_
#define __TAS2562_


/* Book Control Register (available in page0 of each book) */
#define TAS2562_BOOKCTL_PAGE            0
#define TAS2562_BOOKCTL_REG         127

#define TAS2562_REG(page, reg)        ((page * 128) + reg)

    /* Page */
#define TAS2562_Page  TAS2562_REG(0X0, 0x00)
#define TAS2562_Page_Page_Mask  (0xff << 0)


#define TAS2562_BOOK_ID(reg)			(reg / (256 * 128))

#define TAS2562_PAGE_ID(reg)			((reg % (256 * 128)) / 128)

#define TAS2562_BOOK_REG(reg)			(reg % (256 * 128))

#define TAS2562_PAGE_REG(reg)			((reg % (256 * 128)) % 128)


    /* Software Reset */
#define TAS2562_SoftwareReset  TAS2562_REG(0X0, 0x01)
#define TAS2562_SoftwareReset_SoftwareReset_Mask  (0x1 << 0),
#define TAS2562_SoftwareReset_SoftwareReset_DontReset  (0x0 << 0)
#define TAS2562_SoftwareReset_SoftwareReset_Reset  (0x1 << 0)

    /* Power Control */
#define TAS2562_PowerControl  TAS2562_REG(0X0, 0x02)
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
#define TAS2562_PlaybackConfigurationReg0  TAS2562_REG(0X0, 0x03)
#define TAS2562_PlaybackConfigurationReg0_PDMPinMapping_Mask  (0x1 << 7)
#define TAS2562_PlaybackConfigurationReg0_PDMPinMapping_Pdm0  (0x0 << 7)
#define TAS2562_PlaybackConfigurationReg0_PDMPinMapping_Pdm1  (0x1 << 7)
#define TAS2562_PlaybackConfigurationReg0_PlaybackPDMSource_Mask  (0x1 << 6)
#define TAS2562_PlaybackConfigurationReg0_PlaybackSource_Mask  (0x1 << 5)
#define TAS2562_PlaybackConfigurationReg0_PlaybackSource_Pcm  (0x0 << 5)
#define TAS2562_PlaybackConfigurationReg0_PlaybackSource_Pdm  (0x1 << 5)
#define TAS2562_PlaybackConfigurationReg0_AmplifierLevel40_Mask  (0x1f << 0)

    /* Playback Configuration Reg1 */
#define TAS2562_PlaybackConfigurationReg1  TAS2562_REG(0X0, 0x04)
#define TAS2562_PlaybackConfigurationReg1_HPFFrequency20_Mask  (0x7 << 0)
#define TAS2562_PlaybackConfigurationReg1_HPFFrequency20_2  (0x1 << 0)
#define TAS2562_PlaybackConfigurationReg1_HPFFrequency20_50  (0x2 << 0)
#define TAS2562_PlaybackConfigurationReg1_HPFFrequency20_100  (0x3 << 0)
#define TAS2562_PlaybackConfigurationReg1_HPFFrequency20_200  (0x4 << 0)
#define TAS2562_PlaybackConfigurationReg1_HPFFrequency20_400  (0x5 << 0)
#define TAS2562_PlaybackConfigurationReg1_HPFFrequency20_800  (0x6 << 0)
#define TAS2562_PlaybackConfigurationReg1_HPFFrequency20_Bypass  (0x0 << 0)

    /* Playback Configuration Reg2 */
#define TAS2562_PlaybackConfigurationReg2  TAS2562_REG(0X0, 0x05)
#define TAS2562_PlaybackConfigurationReg2_DVCPCM70_Mask  (0xff << 0)

    /* Playback Configuration Reg3 */
#define TAS2562_PlaybackConfigurationReg3  TAS2562_REG(0X0, 0x06)
#define TAS2562_PlaybackConfigurationReg3_DVCPDM70_Mask  (0xff << 0)

    /* Misc Configuration Reg0 */
#define TAS2562_MiscConfigurationReg0  TAS2562_REG(0X0, 0x04)
#define TAS2562_MiscConfigurationReg0_DVCRampRate76_Mask  (0x3 << 6)
#define TAS2562_MiscConfigurationReg0_DVCRampRate76_0_5dbPer1Sample  (0x0 << 6)
#define TAS2562_MiscConfigurationReg0_DVCRampRate76_0_5dbPer4Sample  (0x1 << 6)
#define TAS2562_MiscConfigurationReg0_DVCRampRate76_0_5dbPer8Sample  (0x2 << 6)
#define TAS2562_MiscConfigurationReg0_DVCRampRate76_VolRampDisabled  (0x3 << 6)
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

    /* PDM Input Reg0 */
#define TAS2562_PDMInputReg0  TAS2562_REG(0X0, 0x08)
#define TAS2562_PDMInputReg0_ClassDSYNC_Mask  (0x1 << 6)
#define TAS2562_PDMInputReg0_ClassDSYNC_AsyncMode  (0x0 << 6)
#define TAS2562_PDMInputReg0_ClassDSYNC_Retry  (0x1 << 6),
#define TAS2562_PDMInputReg0_PDMRATESW54_Mask  (0x3 << 4)
#define TAS2562_PDMInputReg0_PDMRATED132_Mask  (0x3 << 2)
#define TAS2562_PDMInputReg0_PDMRATED132_2_543_38MHz  (0x0 << 2)
#define TAS2562_PDMInputReg0_PDMRATED132_5_086_76MHz  (0x1 << 2)
#define TAS2562_PDMInputReg0_PDMRATED010_Mask  (0x3 << 0)
#define TAS2562_PDMInputReg0_PDMRATED010_2_543_38MHz  (0x0 << 0)
#define TAS2562_PDMInputReg0_PDMRATED010_5_086_76MHz  (0x1 << 0)

    /* PDM Configuration Reg1 */
#define TAS2562_PDMConfigurationReg1  TAS2562_REG(0X0, 0x09)
#define TAS2562_PDMConfigurationReg1_PDMEDGED1_Mask  (0x1 << 7)
#define TAS2562_PDMConfigurationReg1_PDMEDGED1_Rising  (0x0 << 7)
#define TAS2562_PDMConfigurationReg1_PDMEDGED1_Falling  (0x1 << 7)
#define TAS2562_PDMConfigurationReg1_PDMEDGED0_Mask  (0x1 << 6)
#define TAS2562_PDMConfigurationReg1_PDMEDGED0_Rising  (0x0 << 6)
#define TAS2562_PDMConfigurationReg1_PDMEDGED0_Falling  (0x1 << 6)
#define TAS2562_PDMConfigurationReg1_PDMSLVD1_Mask  (0x1 << 5)
#define TAS2562_PDMConfigurationReg1_PDMSLVD1_Slave  (0x0 << 5)
#define TAS2562_PDMConfigurationReg1_PDMSLVD1_Master  (0x1 << 5)
#define TAS2562_PDMConfigurationReg1_PDMSLVD0_Mask  (0x1 << 4)
#define TAS2562_PDMConfigurationReg1_PDMSLVD0_Slave  (0x0 << 4)
#define TAS2562_PDMConfigurationReg1_PDMSLVD0_Master  (0x1 << 4)
#define TAS2562_PDMConfigurationReg1_PDMCLKD1_Mask  (0x1 << 3)
#define TAS2562_PDMConfigurationReg1_PDMCLKD1_Pdmck0  (0x0 << 3)
#define TAS2562_PDMConfigurationReg1_PDMCLKD1_Pdmck1  (0x1 << 3)
#define TAS2562_PDMConfigurationReg1_PDMCLKD0_Mask  (0x1 << 2)
#define TAS2562_PDMConfigurationReg1_PDMCLKD0_Pdmck0  (0x0 << 2)
#define TAS2562_PDMConfigurationReg1_PDMCLKD0_Pdmck1  (0x1 << 2)
#define TAS2562_PDMConfigurationReg1_PDMGATED1_Mask  (0x1 << 1)
#define TAS2562_PDMConfigurationReg1_PDMGATED1_GatedOff  (0x0 << 1)
#define TAS2562_PDMConfigurationReg1_PDMGATED1_Active  (0x1 << 1)
#define TAS2562_PDMConfigurationReg1_PDMGATED0_Mask  (0x1 << 0)
#define TAS2562_PDMConfigurationReg1_PDMGATED0_GatedOff  (0x0 << 0)
#define TAS2562_PDMConfigurationReg1_PDMGATED0_Active  (0x1 << 0)

    /* TDM Configuration Reg0 */
#define TAS2562_TDMConfigurationReg0  TAS2562_REG(0X0, 0x06)
#define TAS2562_TDMConfigurationReg0_SAMPRATERAMP_Mask  (0x1 << 5)
#define TAS2562_TDMConfigurationReg0_SAMPRATERAMP_48KHz  (0x0 << 5)
#define TAS2562_TDMConfigurationReg0_SAMPRATERAMP_44_1KHz  (0x1 << 5)
#define TAS2562_TDMConfigurationReg0_DETECTSAMPRATE_Mask  (0x1 << 4)
#define TAS2562_TDMConfigurationReg0_DETECTSAMPRATE_Disabled  (0x1 << 4)
#define TAS2562_TDMConfigurationReg0_DETECTSAMPRATE_Enabled  (0x0 << 4)
#define TAS2562_TDMConfigurationReg0_SAMPRATE31_Mask  (0x7 << 1)
#define TAS2562_TDMConfigurationReg0_SAMPRATE31_44_1_48kHz  (0x4 << 1)
#define TAS2562_TDMConfigurationReg0_SAMPRATE31_88_2_96kHz  (0x5 << 1)
#define TAS2562_TDMConfigurationReg0_SAMPRATE31_176_4_192kHz  (0x6 << 1)
#define TAS2562_TDMConfigurationReg0_FRAMESTART_Mask  (0x1 << 0)
#define TAS2562_TDMConfigurationReg0_FRAMESTART_LowToHigh  (0x0 << 0)
#define TAS2562_TDMConfigurationReg0_FRAMESTART_HighToLow  (0x1 << 0)

    /* TDM Configuration Reg1 */
#define TAS2562_TDMConfigurationReg1  TAS2562_REG(0X0, 0x07)
#define TAS2562_TDMConfigurationReg1_RXJUSTIFY_Mask  (0x1 << 6)
#define TAS2562_TDMConfigurationReg1_RXJUSTIFY_Left  (0x0 << 6)
#define TAS2562_TDMConfigurationReg1_RXJUSTIFY_Right  (0x1 << 6)
#define TAS2562_TDMConfigurationReg1_RXOFFSET51_Mask  (0x1f << 1)
#define TAS2562_TDMConfigurationReg1_RXOFFSET51_Shift (1)
#define TAS2562_TDMConfigurationReg1_RXEDGE_Mask  (0x1 << 0)
#define TAS2562_TDMConfigurationReg1_RXEDGE_Rising  (0x0 << 0)
#define TAS2562_TDMConfigurationReg1_RXEDGE_Falling  (0x1 << 0)

    /* TDM Configuration Reg2 */
#define TAS2562_TDMConfigurationReg2  TAS2562_REG(0X0, 0x08)
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
#define TAS2562_TDMConfigurationReg3  TAS2562_REG(0X0, 0x09)
#define TAS2562_TDMConfigurationReg3_RXSLOTRight74_Mask  (0xf << 4)
#define TAS2562_TDMConfigurationReg3_RXSLOTLeft30_Mask  (0xf << 0)

    /* TDM Configuration Reg4 */
#define TAS2562_TDMConfigurationReg4  TAS2562_REG(0X0, 0x0A)
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
#define TAS2562_TDMConfigurationReg5  TAS2562_REG(0X0, 0x0B)
#define TAS2562_TDMConfigurationReg5_VSNSTX_Mask  (0x1 << 6)
#define TAS2562_TDMConfigurationReg5_VSNSTX_Disable  (0x0 << 6),
#define TAS2562_TDMConfigurationReg5_VSNSTX_Enable  (0x1 << 6),
#define TAS2562_TDMConfigurationReg5_VSNSSLOT50_Mask  (0x3f << 0)

    /* TDM Configuration Reg6 */
#define TAS2562_TDMConfigurationReg6  TAS2562_REG(0X0, 0x0C)
#define TAS2562_TDMConfigurationReg6_ISNSTX_Mask  (0x1 << 6)
#define TAS2562_TDMConfigurationReg6_ISNSTX_Disable  (0x0 << 6),
#define TAS2562_TDMConfigurationReg6_ISNSTX_Enable  (0x1 << 6),
#define TAS2562_TDMConfigurationReg6_ISNSSLOT50_Mask  (0x3f << 0)

    /* TDM Configuration Reg7 */
#define TAS2562_TDMConfigurationReg7  TAS2562_REG(0X0, 0x11)
#define TAS2562_TDMConfigurationReg7_PDMTX_Mask  (0x1 << 6)
#define TAS2562_TDMConfigurationReg7_PDMTX_Disable  (0x0 << 6)
#define TAS2562_TDMConfigurationReg7_PDMTX_Enable  (0x1 << 6)
#define TAS2562_TDMConfigurationReg7_PDMSLOT50_Mask  (0x3f << 0)

    /* TDM Configuration Reg8 */
#define TAS2562_TDMConfigurationReg8  TAS2562_REG(0X0, 0x12)
#define TAS2562_TDMConfigurationReg8_VBATSLEN_Mask  (0x1 << 7)
#define TAS2562_TDMConfigurationReg8_VBATSLEN_8Bits  (0x0 << 7)
#define TAS2562_TDMConfigurationReg8_VBATSLEN_16Bits  (0x1 << 7)
#define TAS2562_TDMConfigurationReg8_VBATTX_Mask  (0x1 << 6)
#define TAS2562_TDMConfigurationReg8_VBATTX_Disable  (0x0 << 6)
#define TAS2562_TDMConfigurationReg8_VBATTX_Enable  (0x1 << 6)
#define TAS2562_TDMConfigurationReg8_VBATSLOT50_Mask  (0x3f << 0)

    /* TDM Configuration Reg9 */
#define TAS2562_TDMConfigurationReg9  TAS2562_REG(0X0, 0x13)
#define TAS2562_TDMConfigurationReg9_TEMPTX_Mask  (0x1 << 6)
#define TAS2562_TDMConfigurationReg9_TEMPTX_Disable  (0x0 << 6)
#define TAS2562_TDMConfigurationReg9_TEMPTX_Enable  (0x1 << 6)
#define TAS2562_TDMConfigurationReg9_TEMPSLOT50_Mask  (0x3f << 0)

    /* TDM Configuration Reg10 */
#define TAS2562_TDMConfigurationReg10  TAS2562_REG(0X0, 0x14)
#define TAS2562_TDMConfigurationReg10_GAINTX_Mask  (0x1 << 6)
#define TAS2562_TDMConfigurationReg10_GAINTX_Disable  (0x0 << 6)
#define TAS2562_TDMConfigurationReg10_GAINTX_Enable  (0x1 << 6)
#define TAS2562_TDMConfigurationReg10_GAINSLOT50_Mask  (0x3f << 0)

    /* Limiter Configuration Reg0 */
#define TAS2562_LimiterConfigurationReg0  TAS2562_REG(0X0, 0x15)
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
#define TAS2562_LimiterConfigurationReg1  TAS2562_REG(0X0, 0x16)
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

    /* Limiter Configuration Reg2 */
#define TAS2562_LimiterConfigurationReg2  TAS2562_REG(0X0, 0x17)
#define TAS2562_LimiterConfigurationReg2_LIMMAXATN40_Mask  (0x1f << 0)

    /* Limiter Configuration Reg3 */
#define TAS2562_LimiterConfigurationReg3  TAS2562_REG(0X0, 0x18)
#define TAS2562_LimiterConfigurationReg3_LIMTHMAX60_Mask  (0x7f << 0)

    /* Limiter Configuration Reg4 */
#define TAS2562_LimiterConfigurationReg4  TAS2562_REG(0X0, 0x19)
#define TAS2562_LimiterConfigurationReg4_LIMTHMIN60_Mask  (0x7f << 0)

    /* Limiter Configuration Reg5 */
#define TAS2562_LimiterConfigurationReg5  TAS2562_REG(0X0, 0x1A)
#define TAS2562_LimiterConfigurationReg5_LIMINFPOINT_Mask  (0x7f << 0)

    /* Brown Out Prevention Reg0 */
#define TAS2562_BrownOutPreventionReg0  TAS2562_REG(0X0, 0x1B)
#define TAS2562_BrownOutPreventionReg0_LIMSLOPE54_Mask  (0x3 << 4)
#define TAS2562_BrownOutPreventionReg0_LIMSLOPE54_1  (0x0 << 4)
#define TAS2562_BrownOutPreventionReg0_LIMSLOPE54_2  (0x2 << 4)
#define TAS2562_BrownOutPreventionReg0_LIMSLOPE54_4  (0x3 << 4)
#define TAS2562_BrownOutPreventionReg0_LIMSLOPE54_1_5  (0x1 << 4)
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
#define TAS2562_BrownOutPreventionReg1  TAS2562_REG(0X0, 0x1C)
#define TAS2562_BrownOutPreventionReg1_BOPTH70_Mask  (0xff << 0)

    /* Brown Out Prevention Reg2 */
#define TAS2562_BrownOutPreventionReg2  TAS2562_REG(0X0, 0x1D)
#define TAS2562_BrownOutPreventionReg2_BOPATKRT75_Mask  (0x7 << 5)
#define TAS2562_BrownOutPreventionReg2_BOPATKRT75_5  (0x0 << 5)
#define TAS2562_BrownOutPreventionReg2_BOPATKRT75_10  (0x1 << 5)
#define TAS2562_BrownOutPreventionReg2_BOPATKRT75_20  (0x2 << 5)
#define TAS2562_BrownOutPreventionReg2_BOPATKRT75_40  (0x3 << 5)
#define TAS2562_BrownOutPreventionReg2_BOPATKRT75_80  (0x4 << 5)
#define TAS2562_BrownOutPreventionReg2_BOPATKRT75_160  (0x5 << 5)
#define TAS2562_BrownOutPreventionReg2_BOPATKRT75_320  (0x6 << 5)
#define TAS2562_BrownOutPreventionReg2_BOPATKRT75_640  (0x7 << 5)
#define TAS2562_BrownOutPreventionReg2_BOPATKST43_Mask  (0x3 << 3)
#define TAS2562_BrownOutPreventionReg2_BOPATKST43_1  (0x1 << 3)
#define TAS2562_BrownOutPreventionReg2_BOPATKST43_2  (0x3 << 3)
#define TAS2562_BrownOutPreventionReg2_BOPATKST43_0_5  (0x0 << 3)
#define TAS2562_BrownOutPreventionReg2_BOPATKST43_1_5  (0x2 << 3)
#define TAS2562_BrownOutPreventionReg2_BOPHLDTM20_Mask  (0x7 << 0)
#define TAS2562_BrownOutPreventionReg2_BOPHLDTM20_0  (0x0 << 0)
#define TAS2562_BrownOutPreventionReg2_BOPHLDTM20_10  (0x1 << 0)
#define TAS2562_BrownOutPreventionReg2_BOPHLDTM20_25  (0x2 << 0)
#define TAS2562_BrownOutPreventionReg2_BOPHLDTM20_50  (0x3 << 0)
#define TAS2562_BrownOutPreventionReg2_BOPHLDTM20_100  (0x4 << 0)
#define TAS2562_BrownOutPreventionReg2_BOPHLDTM20_250  (0x5 << 0)
#define TAS2562_BrownOutPreventionReg2_BOPHLDTM20_500  (0x6 << 0)
#define TAS2562_BrownOutPreventionReg2_BOPHLDTM20_1000  (0x7 << 0)

    /* ICLA Reg0 */
#define TAS2562_ICLAReg0  TAS2562_REG(0X0, 0x1E)
#define TAS2562_ICLAReg0_ICLAUSEMAX_Mask  (0x1 << 7)
#define TAS2562_ICLAReg0_ICLAUSEMAX_Min  (0x0 << 7)
#define TAS2562_ICLAReg0_ICLAUSEMAX_Max  (0x1 << 7)
#define TAS2562_ICLAReg0_ICLASLOT61_Mask  (0x3f << 1)
#define TAS2562_ICLAReg0_ICLAEN_Mask  (0x1 << 0)
#define TAS2562_ICLAReg0_ICLAEN_Disabled  (0x0 << 0)
#define TAS2562_ICLAReg0_ICLAEN_Enabled  (0x1 << 0)

    /* ICLA Reg1 */
#define TAS2562_ICLAReg1  TAS2562_REG(0X0, 0x1F)
#define TAS2562_ICLAReg1_ICLASEN_Mask  (0xff << 0)
#define TAS2562_ICLAReg1_ICLASLOT_7_Disable  (0x0 << 7)
#define TAS2562_ICLAReg1_ICLASLOT_7_Enable  (0x1 << 7)
#define TAS2562_ICLAReg1_ICLASLOT_6_Disable  (0x0 << 6)
#define TAS2562_ICLAReg1_ICLASLOT_6_Enable  (0x1 << 6)
#define TAS2562_ICLAReg1_ICLASLOT_5_Disable  (0x0 << 5)
#define TAS2562_ICLAReg1_ICLASLOT_5_Enable  (0x1 << 5)
#define TAS2562_ICLAReg1_ICLASLOT_4_Disable  (0x0 << 4)
#define TAS2562_ICLAReg1_ICLASLOT_4_Enable  (0x1 << 4)
#define TAS2562_ICLAReg1_ICLASLOT_3_Disable  (0x0 << 3)
#define TAS2562_ICLAReg1_ICLASLOT_3_Enable  (0x1 << 3)
#define TAS2562_ICLAReg1_ICLASLOT_2_Disable  (0x0 << 2)
#define TAS2562_ICLAReg1_ICLASLOT_2_Enable  (0x1 << 2)
#define TAS2562_ICLAReg1_ICLASLOT_1_Disable  (0x0 << 1)
#define TAS2562_ICLAReg1_ICLASLOT_1_Enable  (0x1 << 1)
#define TAS2562_ICLAReg1_ICLASLOT_0_Disable  (0x0 << 0)
#define TAS2562_ICLAReg1_ICLASLOT_0_Enable  (0x1 << 0)

    /* Interrupt Mask Reg0 */
#define TAS2562_InterruptMaskReg0  TAS2562_REG(0X0, 0x20)
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

    /* Interrupt Mask Reg1 */
#define TAS2562_InterruptMaskReg1  TAS2562_REG(0X0, 0x21)
#define TAS2562_InterruptMaskReg1_PDMAUDDATAINVALIDINTMASK_Mask  (0x1 << 7)
#define TAS2562_InterruptMaskReg1_PDMAUDDATAINVALIDINTMASK_Unmask  (0x0 << 7)
#define TAS2562_InterruptMaskReg1_PDMAUDDATAINVALIDINTMASK_Disable  (0x1 << 7)
#define TAS2562_InterruptMaskReg1_VBATOVLOINTMASK_Mask  (0x1 << 3)
#define TAS2562_InterruptMaskReg1_VBATOVLOINTMASK_Unmask  (0x0 << 3)
#define TAS2562_InterruptMaskReg1_VBATOVLOINTMASK_Disable  (0x1 << 3)
#define TAS2562_InterruptMaskReg1_VBATUVLOINTMASK_Mask  (0x1 << 2)
#define TAS2562_InterruptMaskReg1_VBATUVLOINTMASK_Unmask  (0x0 << 2)
#define TAS2562_InterruptMaskReg1_VBATUVLOINTMASK_Disable  (0x1 << 2)
#define TAS2562_InterruptMaskReg1_BrownOutFlagINTMASK_Mask  (0x1 << 1)
#define TAS2562_InterruptMaskReg1_BrownOutFlagINTMASK_Unmask  (0x0 << 1)
#define TAS2562_InterruptMaskReg1_BrownOutFlagINTMASK_Disable  (0x1 << 1)
#define TAS2562_InterruptMaskReg1_PDMClockErrorINTMASK_Mask  (0x1 << 0)
#define TAS2562_InterruptMaskReg1_PDMClockErrorINTMASK_Unmask  (0x0 << 0)
#define TAS2562_InterruptMaskReg1_PDMClockErrorINTMASK_Disable  (0x1 << 0)

    /* Live-Interrupt Reg0 */
#define TAS2562_LiveInterruptReg0  TAS2562_REG(0X0, 0x22)
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
#define TAS2562_LiveInterruptReg1  TAS2562_REG(0X0, 0x23)
#define TAS2562_LiveInterruptReg1_PDMAUDDATAINVALID_Mask  (0x1 << 7)
#define TAS2562_LiveInterruptReg1_PDMAUDDATAINVALID_NoInterrupt  (0x0 << 7)
#define TAS2562_LiveInterruptReg1_PDMAUDDATAINVALID_Interrupt  (0x1 << 7)
#define TAS2562_LiveInterruptReg1_VBATOVLO_Mask  (0x1 << 3)
#define TAS2562_LiveInterruptReg1_VBATOVLO_NoInterrupt  (0x0 << 3)
#define TAS2562_LiveInterruptReg1_VBATOVLO_Interrupt  (0x1 << 3)
#define TAS2562_LiveInterruptReg1_VBATUVLO_Mask  (0x1 << 2)
#define TAS2562_LiveInterruptReg1_VBATUVLO_NoInterrupt  (0x0 << 2)
#define TAS2562_LiveInterruptReg1_VBATUVLO_Interrupt  (0x1 << 2)
#define TAS2562_LiveInterruptReg1_BrownOutFlag_Mask  (0x1 << 1)
#define TAS2562_LiveInterruptReg1_BrownOutFlag_NoInterrupt  (0x0 << 1)
#define TAS2562_LiveInterruptReg1_BrownOutFlag_Interrupt  (0x1 << 1)
#define TAS2562_LiveInterruptReg1_PDMClockError_Mask  (0x1 << 0)
#define TAS2562_LiveInterruptReg1_PDMClockError_NoInterrupt  (0x0 << 0)
#define TAS2562_LiveInterruptReg1_PDMClockError_Interrupt  (0x1 << 0)

    /* Latched-Interrupt Reg0 */
#define TAS2562_LatchedInterruptReg0  TAS2562_REG(0X0, 0x24)
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
#define TAS2562_LatchedInterruptReg1  TAS2562_REG(0X0, 0x25)
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

    /* VBAT MSB */
#define TAS2562_VBATMSB  TAS2562_REG(0X0, 0x27)
#define TAS2562_VBATMSB_VBATMSB70_Mask  (0xff << 0)

    /* VBAT LSB */
#define TAS2562_VBATLSB  TAS2562_REG(0X0, 0x28)
#define TAS2562_VBATLSB_VBATLSB74_Mask  (0xf << 4)

    /* TEMP MSB */
#define TAS2562_TEMPMSB  TAS2562_REG(0X0, 0x29)
#define TAS2562_TEMPMSB_TEMPMSB70_Mask  (0xff << 0)

    /* TEMP LSB */
#define TAS2562_TEMPLSB  TAS2562_REG(0X0, 0x2A)
#define TAS2562_TEMPLSB_TEMPLSB74_Mask  (0xf << 4)

	/* SDZ Config */
#define TAS2562_SDZCONFIG  TAS2562_REG(0X0, 0x2F)
#define TAS2562_SDZCONFIG_ICLANONZEROMIN_Mask  (0x1 << 4)
#define TAS2562_SDZCONFIG_ICLANONZEROMIN_Disable  (0x0 << 4)
#define TAS2562_SDZCONFIG_ICLANONZEROMIN_Enable  (0x1 << 4)
#define TAS2562_SDZCONFIG_SDZMODECONF_Mask  (0x3 << 2)
#define TAS2562_SDZCONFIG_SDZMODECONF_ForcedShutdownAfterTimeout  (0x0 << 2)
#define TAS2562_SDZCONFIG_SDZMODECONF_ForceShutdown  (0x1 << 2)
#define TAS2562_SDZCONFIG_SDZMODECONF_NormalShutdown  (0x2 << 2)
#define TAS2562_SDZCONFIG_SDZMODETIMEOUT_Mask  (0x3 << 0)
#define TAS2562_SDZCONFIG_SDZMODETIMEOUT_2ms  (0x0 << 0)
#define TAS2562_SDZCONFIG_SDZMODETIMEOUT_4ms  (0x1 << 0)
#define TAS2562_SDZCONFIG_SDZMODETIMEOUT_6ms  (0x2 << 0)
#define TAS2562_SDZCONFIG_SDZMODETIMEOUT_23p8ms  (0x3 << 0)


    /* Interrupt Configuration */
#define TAS2562_InterruptConfiguration  TAS2562_REG(0X0, 0x30)
#define TAS2562_InterruptConfiguration_INTTHRUSW_Mask  (0x1 << 2),
#define TAS2562_InterruptConfiguration_INTTHRUSW_IntOnIRQZ  (0x0 << 2)
#define TAS2562_InterruptConfiguration_INTTHRUSW_IntFor2ms  (0x1 << 2)
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
#define TAS2562_DigitalInputPinPullDown  TAS2562_REG(0X0, 0x31)
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
#define TAS2562_MiscIRQ  TAS2562_REG(0X0, 0x32)
#define TAS2562_MiscIRQ_IRQZREQD_Mask  (0x1 << 7)
#define TAS2562_MiscIRQ_IRQZREQD_ActiveHigh  (0x0 << 7)
#define TAS2562_MiscIRQ_IRQZREQD_ActiveLow  (0x1 << 7)
#define TAS2562_MiscIRQ_IRQZBITBANG_Mask  (0x1 << 0)
#define TAS2562_MiscIRQ_IRQZBITBANG_IRQZInputBuf0  (0x0 << 0)
#define TAS2562_MiscIRQ_IRQZBITBANG_IRQZInputBuf1  (0x1 << 0)


    /* Clock Configuration */
#define TAS2562_ClockConfiguration  TAS2562_REG(0X0, 0x38)
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


#define TAS2562_BDIVSelection_BCLKMaster  TAS2562_REG(0X0, 0x3D)
#define TAS2562_BDIVSelection_BCLKMaster_ClockSource10_Mask  (0x3 << 0)
#define TAS2562_BDIVSelection_BCLKMaster_ClockSource10_NDIV2Output  (0x0 << 0)
#define TAS2562_BDIVSelection_BCLKMaster_ClockSource10_NDIV1Output  (0x1 << 0)
#define TAS2562_BDIVSelection_BCLKMaster_ClockSource10_MCLKOutput  (0x2 << 0)
#define TAS2562_BDIVSelection_BCLKMaster_ClockSource10_PDMCLK1PAD  (0x3 << 0)

#define TAS2562_BDIVSelection_HOLDSARUPDATE  TAS2562_REG(0X0, 0x41)
#define TAS2562_BDIVSelection_HOLDSARUPDATE10_Mask  (0x1 << 0)
#define TAS2562_BDIVSelection_HOLDSARUPDATE10_Disabled  (0x0 << 0)
#define TAS2562_BDIVSelection_HOLDSARUPDATE10_Enabled  (0x1 << 0)


    /* TDM Clock detection monitor */
#define TAS2562_TDMClockdetectionmonitor  TAS2562_REG(0X0, 0x77)
#define TAS2562_TDMClockdetectionmonitor_SBCLKtoFSYNC63_Mask  (0xf << 3)
#define TAS2562_TDMClockdetectionmonitor_SBCLKtoFSYNC63_16  (0x0 << 3)
#define TAS2562_TDMClockdetectionmonitor_SBCLKtoFSYNC63_24  (0x1 << 3)
#define TAS2562_TDMClockdetectionmonitor_SBCLKtoFSYNC63_32  (0x2 << 3)
#define TAS2562_TDMClockdetectionmonitor_SBCLKtoFSYNC63_48  (0x3 << 3)
#define TAS2562_TDMClockdetectionmonitor_SBCLKtoFSYNC63_64  (0x4 << 3)
#define TAS2562_TDMClockdetectionmonitor_SBCLKtoFSYNC63_96  (0x5 << 3)
#define TAS2562_TDMClockdetectionmonitor_SBCLKtoFSYNC63_128  (0x6 << 3)
#define TAS2562_TDMClockdetectionmonitor_SBCLKtoFSYNC63_192  (0x7 << 3)
#define TAS2562_TDMClockdetectionmonitor_SBCLKtoFSYNC63_256  (0x8 << 3)
#define TAS2562_TDMClockdetectionmonitor_SBCLKtoFSYNC63_384  (0x9 << 3)
#define TAS2562_TDMClockdetectionmonitor_SBCLKtoFSYNC63_512  (0xf << 3)
#define TAS2562_TDMClockdetectionmonitor_DetectedSampleRate20_Mask  (0x7 << 0),
#define TAS2562_TDMClockdetectionmonitor_DetectedSampleRate20_8khz  (0x0 << 0)
#define TAS2562_TDMClockdetectionmonitor_DetectedSampleRate20_16khz  (0x1 << 0)
#define TAS2562_TDMClockdetectionmonitor_DetectedSampleRate20_32khz  (0x2 << 0)
#define TAS2562_TDMClockdetectionmonitor_DetectedSampleRate20_48khz  (0x3 << 0)
#define TAS2562_TDMClockdetectionmonitor_DetectedSampleRate20_96khz  (0x4 << 0)
#define TAS2562_TDMClockdetectionmonitor_DetectedSampleRate20_192khz  (0x5 << 0)

    /* Revision and PG ID */
#define TAS2562_RevisionandPGID  TAS2562_REG(0X0, 0x7D)
#define TAS2562_RevisionandPGID_RevisionID74_Mask  (0xf << 4)
#define TAS2562_RevisionandPGID_PGID30_Mask  (0xf << 0)

    /* I2C Checksum */
#define TAS2562_I2CChecksum  TAS2562_REG(0X0, 0x7E)
#define TAS2562_I2CChecksum_I2CChecksum70_Mask  (0xff << 0)

    /* Book */
#define TAS2562_Book  TAS2562_REG(0X0, 0x7F)
#define TAS2562_Book_Book70_Mask  (0xff << 0)


#define TAS2562_RegisterCount  55

#define ERROR_NONE          0x0000000
#define ERROR_PLL_ABSENT    0x0000000
#define ERROR_DEVA_I2C_COMM 0x0000000
#define ERROR_DEVB_I2C_COMM 0x0000000
#define ERROR_PRAM_CRCCHK   0x0000000
#define ERROR_YRAM_CRCCHK   0x0000001
#define ERROR_CLK_DET2      0x0000002
#define ERROR_CLK_DET1      0x0000004
#define ERROR_CLK_LOST      0x0000008
#define ERROR_BROWNOUT      0x0000010
#define ERROR_DIE_OVERTEMP  0x0000020
#define ERROR_CLK_HALT      0x0000040
#define ERROR_UNDER_VOLTAGE 0x0000080
#define ERROR_OVER_CURRENT  0x0000100
#define ERROR_CLASSD_PWR    0x0000200
#define ERROR_FAILSAFE      0x4000000

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
struct hrtimer mtimer;
int mnClkin;
int mnClkid;
bool mbPowerUp;
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
//Added 060356-PP
int mnSlot_width;
int ch_size;
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