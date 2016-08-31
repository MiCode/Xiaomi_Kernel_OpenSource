#ifndef __MFD_AIC3256_REGISTERS_H__
#define __MFD_AIC3256_REGISTERS_H__

#define AIC3256_CODEC_SUPPORT 1

/* Moved these registers from tlv320aic325x.h as part of the MFD changes*/
#define MAKE_REG(page, offset)	((unsigned int)((page<<8) | offset)\
								& 0x0000ffff)
/* ****************** Page 0 Registers **************************************/
/* Page select register */
#define	AIC3256_PAGE_SELECT			MAKE_REG(0, 0)
/* Software reset register */
#define	AIC3256_RESET				MAKE_REG(0, 1)
/*Clock setting register 1, Multiplexers*/
#define	AIC3256_CLK_REG_1			MAKE_REG(0, 4)
/*Clock setting register 2, PLL*/
#define	AIC3256_CLK_REG_2			MAKE_REG(0, 5)
/*Clock setting register 3, PLL*/
#define	AIC3256_CLK_REG_3			MAKE_REG(0, 6)
/*Clock setting register 4, PLL*/
#define	AIC3256_CLK_REG_4			MAKE_REG(0, 7)
/*Clock setting register 5, PLL*/
#define	AIC3256_CLK_REG_5			MAKE_REG(0, 8)
/*Clock setting register 6, PLL*/
#define	AIC3256_NDAC_CLK_REG_6		        MAKE_REG(0, 11)
/*Clock setting register 7, PLL*/
#define	AIC3256_MDAC_CLK_REG_7		        MAKE_REG(0, 12)
/*DAC OSR setting register1,MSB value*/
#define AIC3256_DAC_OSR_MSB			MAKE_REG(0, 13)
/*DAC OSR setting register 2,LSB value*/
#define AIC3256_DAC_OSR_LSB			MAKE_REG(0, 14)
/*Clock setting register 8, PLL*/
#define	AIC3256_NADC_CLK_REG_8			MAKE_REG(0, 18)
/*Clock setting register 9, PLL*/
#define	AIC3256_MADC_CLK_REG_9			MAKE_REG(0, 19)
/*ADC Oversampling (AOSR) Register*/
#define AIC3256_ADC_OSR_REG			MAKE_REG(0, 20)
/*Clock setting register 9, Multiplexers*/
#define AIC3256_CLK_MUX_REG_9		        MAKE_REG(0, 25)
/*Clock setting register 10, CLOCKOUT M divider value*/
#define AIC3256_CLK_REG_10			MAKE_REG(0, 26)
/*Audio Interface Setting Register 1*/
#define AIC3256_INTERFACE_SET_REG_1	        MAKE_REG(0, 27)
/*Audio Interface Setting Register 2*/
#define AIC3256_AIS_REG_2			MAKE_REG(0, 28)
#define AIC3256_INTERFACE_SET_REG_2		MAKE_REG(0, 29)
/*Audio Interface Setting Register 3*/
#define AIC3256_INTERFACE_SET_REG_3		MAKE_REG(0, 29)
#define AIC3256_AIS_REG_3			MAKE_REG(0, 29)
/*Clock setting register 11,BCLK N Divider*/
#define AIC3256_CLK_REG_11			MAKE_REG(0, 30)
/*Audio Interface Setting Register 4,Secondary Audio Interface*/
#define AIC3256_AIS_REG_4			MAKE_REG(0, 31)
/*Audio Interface Setting Register 5*/
#define AIC3256_AIS_REG_5			MAKE_REG(0, 32)
/*Audio Interface Setting Register 6*/
#define AIC3256_AIS_REG_6			MAKE_REG(0, 33)
/*ADC Flag Register */
#define AIC3256_ADC_FLAG			MAKE_REG(0, 36)
/*DAC Flag Register 1*/
#define AIC3256_DAC_FLAG_1			MAKE_REG(0, 37)
/*DAC Flag Register 2*/
#define AIC3256_DAC_FLAG_2			MAKE_REG(0, 38)
/* Sticky flag for Headset Insertion/ removal */
#define AIC3256_STICKY_FLAG1			MAKE_REG(0, 44)
/* Status flag holds the Headst detection status */
#define AIC3256_STATUS_FLAG1			MAKE_REG(0, 46)

#define AIC3256_INT1_CTRL			MAKE_REG(0, 48)
#define AIC3256_GPIO_CTRL			MAKE_REG(0, 52)
#define AIC3256_GPIO_D6_D2			(0b01111100)
#define AIC3256_GPIO_D2_SHIFT			(2)

/*DOUT Function Control*/
#define AIC3256_DOUT_CTRL			MAKE_REG(0, 53)
/*DIN Function Control*/
#define AIC3256_DIN_CTL				MAKE_REG(0, 54)
/*MISO Function Control*/
#define AIC3256_DAC_PRB				MAKE_REG(0, 60)
/*ADC Signal Processing Block*/
#define AIC3256_ADC_PRB				MAKE_REG(0, 61)
/*DAC channel setup register*/
#define AIC3256_DAC_CHN_REG                     MAKE_REG(0, 63)
/*DAC Mute and volume control register*/
#define AIC3256_DAC_MUTE_CTRL_REG	        MAKE_REG(0, 64)
/*Left DAC channel digital volume control*/
#define AIC3256_LDAC_VOL			MAKE_REG(0, 65)
/*Right DAC channel digital volume control*/
#define AIC3256_RDAC_VOL			MAKE_REG(0, 66)

/*Headset Detection Configuration Register*/
#define AIC3256_HEADSET_DETECT                  MAKE_REG(0, 67)
#define AIC3256_HEADSET_ENABLE			0x80
#define AIC3256_HEADSET_DISABLE			0x00
#define AIC3256_HEADSET_DEBOUNCE_MASK		0x1c
#define AIC3256_HEADSET_DEBOUNCE_32		0x04
#define AIC3256_HEADSET_DEBOUNCE_64		0x08
#define AIC3256_HEADSET_DEBOUNCE_128		0x0c
#define AIC3256_HEADSET_DEBOUNCE_256		0x10
#define AIC3256_HEADSET_DEBOUNCE_512		0x14
#define AIC3256_HEADSET_IN_MASK			0x80

/* DRC control Registers */
#define AIC3256_DRC_CTRL_REG1                   MAKE_REG(0, 68)
#define AIC3256_DRC_CTRL_REG2                   MAKE_REG(0, 69)
#define AIC3256_DRC_CTRL_REG3                   MAKE_REG(0, 70)

/* Beep Generator Control Registers */
#define AIC3256_BEEP_CTRL_REG1                  MAKE_REG(0, 71)
#define AIC3256_BEEP_CTRL_REG2                  MAKE_REG(0, 72)
#define AIC3256_BEEP_CTRL_REG3                  MAKE_REG(0, 73)
#define AIC3256_BEEP_CTRL_REG4                  MAKE_REG(0, 74)
#define AIC3256_BEEP_CTRL_REG5                  MAKE_REG(0, 75)
#define AIC3256_BEEP_CTRL_REG6                  MAKE_REG(0, 76)
#define AIC3256_BEEP_CTRL_REG7                  MAKE_REG(0, 77)
#define AIC3256_BEEP_CTRL_REG8                  MAKE_REG(0, 78)
#define AIC3256_BEEP_CTRL_REG9                  MAKE_REG(0, 79)

/*ADC Register 1*/
#define AIC3256_ADC_CHN_REG			MAKE_REG(0, 81)
/*ADC Fine Gain Adjust*/
#define	AIC3256_ADC_FGA				MAKE_REG(0, 82)
/*Left ADC Channel Volume Control*/
#define AIC3256_LADC_VOL			MAKE_REG(0, 83)
/*Right ADC Channel Volume Control*/
#define AIC3256_RADC_VOL			MAKE_REG(0, 84)

/*Left Channel AGC Control Register 1*/
#define AIC3256_LEFT_AGC_REG1		        MAKE_REG(0, 86)
/*Left Channel AGC Control Register 2*/
#define AIC3256_LEFT_AGC_REG2		        MAKE_REG(0, 87)
/*Left Channel AGC Control Register 3 */
#define AIC3256_LEFT_AGC_REG3		        MAKE_REG(0, 88)
/*Left Channel AGC Control Register 4 */
#define AIC3256_LEFT_AGC_REG4		        MAKE_REG(0, 89)
/*Left Channel AGC Control Register 5 */
#define AIC3256_LEFT_AGC_REG5		        MAKE_REG(0, 90)
/*Left Channel AGC Control Register 6 */
#define AIC3256_LEFT_AGC_REG6		        MAKE_REG(0, 91)
/*Left Channel AGC Control Register 7 */
#define AIC3256_LEFT_AGC_REG7		        MAKE_REG(0, 92)
/*Right Channel AGC Control Register 1*/
#define AIC3256_RIGHT_AGC_REG1		        MAKE_REG(0, 94)
/*Right Channel AGC Control Register 2*/
#define AIC3256_RIGHT_AGC_REG2			MAKE_REG(0, 95)
/*Right Channel AGC Control Register 3 */
#define AIC3256_RIGHT_AGC_REG3			MAKE_REG(0, 96)
/*Right Channel AGC Control Register 4 */
#define AIC3256_RIGHT_AGC_REG4		        MAKE_REG(0, 97)
/*Right Channel AGC Control Register 5 */
#define AIC3256_RIGHT_AGC_REG5		        MAKE_REG(0, 98)
/*Right Channel AGC Control Register 6 */
#define AIC3256_RIGHT_AGC_REG6		        MAKE_REG(0, 99)
/*Right Channel AGC Control Register 7 */
#define AIC3256_RIGHT_AGC_REG7		        MAKE_REG(0, 100)

#define AIC3256_DEVICE_ID		        MAKE_REG(0, 125)
/******************** Page 1 Registers **************************************/
/*Power Conguration*/
#define AIC3256_POW_CFG				MAKE_REG(1, 1)
/*LDO Control*/
#define AIC3256_LDO_CTRL			MAKE_REG(1, 2)

#if defined(AIC3256_CODEC_SUPPORT) || defined(AIC3206_CODEC_SUPPORT)
/*power control register 2 */
#define AIC3256_PWR_CTRL_REG                    MAKE_REG(1, 2)
#endif


/*Output Driver Power Control*/
#define AIC3256_OUT_PWR_CTRL			MAKE_REG(1, 9)

/* Full Chip Common Mode register */
#ifdef AIC3256_CODEC_SUPPORT
#define AIC3256_CM_CTRL_REG			MAKE_REG(1, 10)
#endif
/*HPL Routing Selection*/
#define AIC3256_HPL_ROUTE_CTRL		        MAKE_REG(1, 12)
/*HPR Routing Selection*/
#define AIC3256_HPR_ROUTE_CTRL		        MAKE_REG(1, 13)

#ifndef AIC3253_CODEC_SUPPORT
/*LOL Routing Selection*/
#define AIC3256_LOL_ROUTE_CTRL		        MAKE_REG(1, 14)
/*LOR Routing Selection*/
#define AIC3256_LOR_ROUTE_CTRL		        MAKE_REG(1, 15)
#endif

/*HPL Driver Gain*/
#define	AIC3256_HPL_GAIN			MAKE_REG(1, 16)
/*HPR Driver Gain*/
#define	AIC3256_HPR_GAIN			MAKE_REG(1, 17)

/*HPL Driver Mute Control*/
#define AIC3256_HPL_MUTE_CTRL			MAKE_REG(1, 16)
/*HPR Driver Mute Control*/
#define AIC3256_HPR_MUTE_CTRL			MAKE_REG(1, 17)
/*HP Driver Mute Mask*/
#define AIC3256_HP_MUTE_MASK			(1 << 6)
/*HP Driver Mute Enable*/
#define AIC3256_HP_MUTE_ENABLE			(1 << 6)
/*HP Driver Mute Disable*/
#define AIC3256_HP_MUTE_DISABLE			(0 << 6)

#ifndef AIC3253_CODEC_SUPPORT
/*LOL Driver Gain*/
#define	AIC3256_LOL_GAIN			MAKE_REG(1, 18)
/*LOR Driver Gain*/
#define	AIC3256_LOR_GAIN			MAKE_REG(1, 19)
#endif

/*Headphone Driver Startup Control Register*/
#define AIC3256_HPHONE_STARTUP_CTRL	        MAKE_REG(1, 20)
/*IN1L to HPL Volume Control Register */
#define AIC3256_IN1L_HPL_CTRL                   MAKE_REG(1, 22)
/*IN1R to HPR Volume Control Register */
#define AIC3256_IN1R_HPR_CTRL                   MAKE_REG(1, 23)

#ifndef AIC3253_CODEC_SUPPORT
/*MAL Volume Control Register*/
#define AIC3256_MAL_CTRL_REG                    MAKE_REG(1, 24)
/*MAR Volume Control Register*/
#define AIC3256_MAR_CTRL_REG                    MAKE_REG(1, 25)
#endif

/*MICBIAS Configuration Register*/
#define AIC3256_MICBIAS_CTRL		        MAKE_REG(1, 51)

#ifndef AIC3253_CODEC_SUPPORT
/*Left MICPGA Positive Terminal Input Routing Configuration Register*/
#define AIC3256_LMICPGA_PIN_CFG		        MAKE_REG(1, 52)
/*Left MICPGA Negative Terminal Input Routing Configuration Register*/
#define AIC3256_LMICPGA_NIN_CFG		        MAKE_REG(1, 54)
/*Right MICPGA Positive Terminal Input Routing Configuration Register*/
#define AIC3256_RMICPGA_PIN_CFG		        MAKE_REG(1, 55)
/*Right MICPGA Negative Terminal Input Routing Configuration Register*/
#define AIC3256_RMICPGA_NIN_CFG		        MAKE_REG(1, 57)
#endif

/*Floating Input Configuration Register*/
#define AIC3256_INPUT_CFG_REG		        MAKE_REG(1, 58)

#ifndef AIC3253_CODEC_SUPPORT
/*Left MICPGA Volume Control Register*/
#define AIC3256_LMICPGA_VOL_CTRL	        MAKE_REG(1, 59)
/*Right MICPGA Volume Control Register*/
#define AIC3256_RMICPGA_VOL_CTRL	        MAKE_REG(1, 60)
#endif

#define AIC3256_ANALOG_INPUT_CHARGING_CFG	MAKE_REG(1, 71)

#define AIC3256_REF_PWR_UP_CONF_REG		MAKE_REG(1, 123)

#if defined(AIC3256_CODEC_SUPPORT) || defined(AIC3206_CODEC_SUPPORT)
/*charge control register*/
#define AIC3256_CHRG_CTRL_REG                   MAKE_REG(1, 124)
/*headphone driver configuration register*/
#define AIC3256_HP_DRIVER_CONF_REG              MAKE_REG(1, 125)
#endif

#define AIC3256_ADC_DATAPATH_SETUP		MAKE_REG(0, 81)
#define AIC3256_DAC_DATAPATH_SETUP	MAKE_REG(0, 63)

#define AIC3256_ADC_FLAG		MAKE_REG(0, 36)
#define AIC3256_ADC_POWER_MASK		0x44

#define AIC3256_DAC_FLAG		MAKE_REG(0, 37)
#define AIC3256_DAC_POWER_MASK		0x88

#define AIC3256_LADC_POWER_MASK		0x40
#define AIC3256_RADC_POWER_MASK		0x04

#define AIC3256_LDAC_POWER_MASK		0x80
#define AIC3256_RDAC_POWER_MASK		0x08

#define AIC3256_DAC_PRB                 MAKE_REG(0, 60)
/**************************************************************/
#define AIC3256_JACK_TYPE_MASK		(0b01100000)
#define AIC3256_JACK_WITHOUT_MIC	(0b00100000)
#define AIC3256_JACK_WITH_MIC		(0b01100000)
/*****************************************************************/
#define AIC3256_ADC_ADAPTIVE_CRAM_REG	MAKE_REG(8, 1)
#define AIC3256_DAC_ADAPTIVE_CRAM_REG	MAKE_REG(44, 1)
#define AIC3256_LDAC_POWER_STATUS_MASK  0x80
#define AIC3256_RDAC_POWER_STATUS_MASK  0x08
#define AIC3256_LADC_POWER_STATUS_MASK  0x40
#define AIC3256_RADC_POWER_STATUS_MASK  0x04
#define AIC3256_HPL_POWER_STATUS_MASK   0X20
#define AIC3256_HPR_POWER_STATUS_MASK   0X02

#define AIC3256_TIME_DELAY              5
#define AIC3256_DELAY_COUNTER           100
#if 0	/*Need to map the following registers with 3256 codec dstasheet*/

#define AIC3256_GPIO1_IO_CNTL	86
#define AIC3256_GPIO2_IO_CNTL	87
#define AIC3256_GPI1_EN		91
#define AIC3256_GPI2_EN		92
#define AIC3256_GPO1_OUT_CNTL	96
#define AIC3256_RESET_REG	1
#define AIC3256_REV_PG_ID	2
#define AIC3256_REV_MASK	(0b01110000)
#define AIC3256_REV_SHIFT	4
#define AIC3256_PG_MASK		(0b00000111)
#define AIC3256_PG_SHIFT	0

#define AIC3256_GPIO_D6_D2	(0b01111100)
#define AIC3256_GPIO_D2_SHIFT	(2)
#define AIC3256_GPIO_D6_D2	(0b01111100)
#define AIC3256_GPI1_D2_D1	(0b00000110)
#define AIC3256_GPIO_D1_SHIFT	(1)
#define AIC3256_GPI2_D5_D4	(0b00110000)
#define AIC3256_GPIO_D4_SHIFT	(4)
#define AIC3256_GPO1_D4_D1	(0b00011110)

#endif
#define AIC3256_FREQ_12000000 12000000
#define	AIC3256_FREQ_12288000 12288000
#define AIC3256_FREQ_24000000 24000000
#define AIC3256_FREQ_19200000 19200000
#define AIC3256_FREQ_38400000 38400000

/* Audio data word length = 16-bits (default setting) */
#define AIC3256_WORD_LEN_16BITS		0x00
#define AIC3256_WORD_LEN_20BITS		0x01
#define AIC3256_WORD_LEN_24BITS		0x02
#define AIC3256_WORD_LEN_32BITS		0x03

/****************************************************************************/
#define AIC3256_BIT7				(0x01 << 7)
#define AIC3256_CODEC_CLKIN_MASK		0x03
#define AIC3256_MCLK_2_CODEC_CLKIN		0x00
#define AIC3256_PLLCLK_2_CODEC_CLKIN	        0x03
/*Bclk_in selection*/
#define AIC3256_BDIV_CLKIN_MASK			0x03
#define	AIC3256_DAC_MOD_CLK_2_BDIV_CLKIN	0x01
#define AIC3256_SOFT_RESET			0x01
#define AIC3256_PAGE0				0x00
#define AIC3256_PAGE1				0x01
#define AIC3256_BIT_CLK_MASTER			0x08
#define AIC3256_WORD_CLK_MASTER			0x04
#define	AIC3256_HIGH_PLL			(0x01 << 6)
#define AIC3256_ENABLE_PLL			BIT7
#define AIC3256_ENABLE_NDAC			BIT7
#define AIC3256_ENABLE_MDAC			BIT7
#define AIC3256_ENABLE_NADC			BIT7
#define AIC3256_ENABLE_MADC			BIT7
#define AIC3256_ENABLE_BCLK			BIT7
#define AIC3256_ENABLE_DAC			(0x03 << 6)
#define AIC3256_LDAC_2_LCHN			(0x01 << 4)
#define AIC3256_RDAC_2_RCHN			(0x01 << 2)
#define AIC3256_LDAC_CHNL_2_HPL			(0x01 << 3)
#define AIC3256_RDAC_CHNL_2_HPR			(0x01 << 3)
#define AIC3256_SOFT_STEP_2WCLK			(0x01)
#define AIC3256_DAC_MUTE_ON			0x0C
#define AIC3256_ADC_MUTE_ON			0x88
#define AIC3256_DEFAULT_VOL			0x0
#define AIC3256_DISABLE_ANALOG			(0x01 << 3)
#define AIC3256_LDAC_2_HPL_ROUTEON		0x08
#define AIC3256_RDAC_2_HPR_ROUTEON		0x08

#define AIC3256_HP_DRIVER_BUSY_MASK		0x04
/* Headphone driver Configuration Register Page 1, Register 125 */
#define AIC3256_GCHP_ENABLE			0x10
#define AIC3256_DC_OC_FOR_ALL_COMB		0x03
#define AIC3256_DC_OC_FOR_PROG_COMB		0x02

/* Reference Power-Up configuration register */
#define AIC3256_REF_PWR_UP_MASK			0x4
#define AIC3256_AUTO_REF_PWR_UP			0x0
#define AIC3256_FORCED_REF_PWR_UP		0x4

/* Power Configuration register 1 */
#define AIC3256_WEAK_AVDD_TO_DVDD_DIS		0x8

/* Power Configuration register 1 */
#define AIC3256_ANALOG_BLOCK_POWER_CONTROL_MASK	0x08
#define AIC3256_ENABLE_ANALOG_BLOCK		0x0
#define AIC3256_DISABLE_ANALOG_BLOCK		0x8

/* Floating input Configuration register P1_R58 */
#define AIC3256_WEAK_BIAS_INPUTS_MASK		0xFC

/* Common Mode Control Register */
#define AIC3256_GCHP_HPL_STATUS			0x4

/* Audio Interface Register 3 P0_R29 */
#define AIC3256_BCLK_WCLK_BUFFER_POWER_CONTROL_MASK	0x4
#define AIC3256_BCLK_WCLK_BUFFER_ON			0x4

/* Power Configuration Register */
#define AIC3256_AVDD_CONNECTED_TO_DVDD_MASK	0x8
#define AIC3256_DISABLE_AVDD_TO_DVDD		0x8
#define AIC3256_ENABLE_AVDD_TO_DVDD		0x0


/* Masks used for updating register bits */
#define AIC3256_PLL_P_DIV_MASK			0x7F
#define AIC3256_PLL_J_DIV_MASK			0x7F
#define AIC3256_PLL_NDAC_DIV_MASK		0x7F
#define AIC3256_PLL_MDAC_DIV_MASK		0x7F
#define AIC3256_PLL_NADC_DIV_MASK		0x7F
#define AIC3256_PLL_MADC_DIV_MASK		0x7F
#define AIC3256_PLL_BCLK_DIV_MASK		0x7F
#define AIC3256_INTERFACE_REG_MASK		0x7F

#define AIC3256_PLL_D_MSB_DIV_MASK		0x3F
#define AIC3256_PLL_D_LSB_DIV_MASK		0xFF
#define AIC3256_PLL_DOSR_MSB_MASK		0x03
#define AIC3256_PLL_DOSR_LSB_MASK		0xFF
#define AIC3256_PLL_AOSR_DIV_MASK		0xFF

#define AIC3256_DAC_MUTE_MASK			0x0C
#define AIC3256_ADC_MUTE_MASK			0x88
#define AIC3256_CODEC_RESET_MASK		0x01
#define AIC3256_DAC_SOFT_STEPPING_MASK		0x02
#define AIC3256_ADC_SOFT_STEPPING_MASK		0x02

#define AIC3256_TIME_DELAY			5
#define AIC3256_DELAY_COUNTER			100

/* Serial data bus uses I2S mode (Default mode) */
#define AIC3256_I2S_MODE		0x00
#define AIC3256_DSP_MODE		0x01
#define AIC3256_RIGHT_JUSTIFIED_MODE	0x02
#define AIC3256_LEFT_JUSTIFIED_MODE	0x03

/* shift value for CLK_REG_3 register */
#define AIC3256_CLK_REG_3_SHIFT			6

/* AIC3256 register space */
#define	AIC3256_CACHEREGNUM		256

#endif /* __MFD_AIC3256_REGISTERS_H__ */

