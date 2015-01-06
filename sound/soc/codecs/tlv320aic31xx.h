/* revert
 * linux/sound/soc/codecs/tlv320aic31xx.h
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */
#ifndef _TLV320AIC31XX_H
#define _TLV320AIC31XX_H

#define AUDIO_NAME "aic31xx"
/* AIC31XX supported sample rate are 8k to 192k */
#define AIC31XX_RATES	SNDRV_PCM_RATE_8000_192000

/* AIC31XX supports the word formats 16bits, 20bits, 24bits and 32 bits */
#define AIC31XX_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE \
			 | SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

#define AIC31XX_REQ_TIMER_FREQ		1000000
#define AIC31XX_FREQ_25000000		25000000
#define AIC31XX_JD_FREQ			1000000

#define AIC31XX_INTERNALCLOCK		0
#define AIC31XX_MCLK			1
/* Audio data word length = 16-bits (default setting) */
#define AIC31XX_WORD_LEN_16BITS		0x00
#define AIC31XX_WORD_LEN_20BITS		0x01
#define AIC31XX_WORD_LEN_24BITS		0x02
#define AIC31XX_WORD_LEN_32BITS		0x03


/* D15..D8 aic31xx register offset */
#define AIC31XX_REG_OFFSET_INDEX		0
/* D7...D0 register data */
#define AIC31XX_REG_DATA_INDEX		1


/* 8 bit mask value */
#define AIC31XX_8BITS_MASK		0xFF

/* shift value for CLK_REG_3 register */
#define CLK_REG_3_SHIFT			6
/* shift value for DAC_OSR_MSB register */
#define DAC_OSR_MSB_SHIFT		4

/* Masks used for updating register bits */
#define AIC31XX_IFACE1_DATALEN_MASK	0x30
#define AIC31XX_IFACE1_DATALEN_SHIFT	(4)
#define AIC31XX_IFACE1_DATATYPE_MASK	0xC0
#define AIC31XX_IFACE1_DATATYPE_SHIFT	(6)
/* Serial data bus uses I2S mode (Default mode) */
#define AIC31XX_I2S_MODE		0x00
#define AIC31XX_DSP_MODE		0x01
#define AIC31XX_RIGHT_JUSTIFIED_MODE	0x02
#define AIC31XX_LEFT_JUSTIFIED_MODE	0x03

#define AIC31XX_IFACE1_MASTER_MASK	0x0C
#define AIC31XX_IFACE1_MASTER_SHIFT	(2)
#define AIC31XX_BCLK_MASTER	0x80
#define AIC31XX_WCLK_MASTER	0x40


#define AIC31XX_DATA_OFFSET_MASK	0xFF
#define AIC31XX_BCLKINV_MASK		0x08
#define AIC31XX_BDIVCLK_MASK		0x03

#define AIC31XX_DAC2BCLK		0x00
#define AIC31XX_DACMOD2BCLK		0x01
#define AIC31XX_ADC2BCLK		0x02
#define AIC31XX_ADCMOD2BCLK		0x03


struct aic31xx_jack_data {
	struct snd_soc_jack *jack;
	int report;
};

struct aic31xx_gpio_setup {
	unsigned int reg;
	u8 value;
};

/*
 *----------------------------------------------------------------------------
 * @struct  aic31xx_rate_divs |
 *          Setting up the values to get different freqencies
 *
 * @field   u32 | mclk |
 *          Master clock
 * @field   u32 | rate |
 *          sample rate
 * @field   u8 | p_val |
 *          value of p in PLL
 * @field   u32 | pll_j |
 *          value for pll_j
 * @field   u32 | pll_d |
 *          value for pll_d
 * @field   u32 | dosr |
 *          value to store dosr
 * @field   u32 | ndac |
 *          value for ndac
 * @field   u32 | mdac |
 *          value for mdac
 * @field   u32 | aosr |
 *          value for aosr
 * @field   u32 | nadc |
 *          value for nadc
 * @field   u32 | madc |
 *          value for madc
 * @field   u32 | blck_N |
 *          value for block N
 * @field   u32 | aic31xx_configs |
 *          configurations for aic31xx register value
 *----------------------------------------------------------------------------
 */
struct aic31xx_rate_divs {
	u32 mclk;
	u32 rate;
	u8 p_val;
	u8 pll_j;
	u16 pll_d;
	u16 dosr;
	u8 ndac;
	u8 mdac;
	u8 aosr;
	u8 nadc;
	u8 madc;
	u8 bclk_n;
};


enum aic31xx_type {
	AIC311X = 0,
	AIC310X = 1,
};

struct aic31xx_pdata {
	enum aic31xx_type codec_type;
	unsigned int audio_mclk1;
	unsigned int gpio_irq; /* whether AIC31XX interrupts the host AP
				on a GPIO pin of AP */
	unsigned int gpio_reset; /* is the codec being reset by a gpio
				[host] pin, if yes provide the number. */
	int num_gpios;
	struct aic31xx_gpio_setup *gpio_defaults;/* all gpio configuration */
	int naudint_irq; /* audio interrupt */
	int headset_detect;
	int button_press_detect;
};

struct aic31xx_priv {
	struct aic31xx_jack_data hs_jack;
	struct workqueue_struct *workqueue;
	struct delayed_work delayed_work;
	struct snd_soc_codec *codec;
	u8 i2c_regs_status;
	struct device *dev;
	struct regmap *regmap;
	struct mutex mutex;
	struct aic31xx_pdata pdata;
	unsigned int irq;
};

/* Driver data to differentiate between apci and i2c device
 * platform data extraction is different for ACPI and i2c device
 */
struct aic31xx_driver_data {
	u32 acpi_device;
};

/*
 *----------------------------------------------------------------------------
int aic31xx_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
 * @struct  snd_soc_codec_device |
 *          This structure is soc audio codec device sturecute which pointer
 *          to basic functions aic31xx_probe(), aic31xx_remove(),
 *			aic31xx_suspend() and aic31xx_resume()
 *
 */
extern struct snd_soc_codec_device soc_codec_dev_aic31xx;


/* Device I/O API */
int aic31xx_device_init(struct aic31xx_priv *aic31xx);
void aic31xx_device_exit(struct aic31xx_priv *aic31xx);
void aic31xx_enable_mic_bias(struct snd_soc_codec *codec, int enable);
int aic31xx_query_jack_status(struct snd_soc_codec *codec);
int aic31xx_query_btn_press(struct snd_soc_codec *codec);
void aic31xx_btn_press_intr_enable(struct snd_soc_codec *codec,
		int enable);


/* ****************** Book 0 Registers **************************************/
/* ****************** AIC31XX has one book only *****************************/

/* ****************** Page 0 Registers **************************************/
/* Software reset register */
#define AIC31XX_RESET				0x81
/* OT FLAG register */
#define AIC31XX_OT_FLAG				0x83
/* Revision and PG-ID */
#define AIC31XX_REV_PG_ID				0x03
#define AIC31XX_REV_MASK				0x70 /* (0b01110000) */
#define AIC31XX_REV_SHIFT				4
#define AIC31XX_PG_MASK					0x1  /* (0b00000001) */
#define AIC31XX_PG_SHIFT				0

/* Clock clock Gen muxing, Multiplexers*/
#define AIC31XX_CLKMUX					0x84
#define AIC31XX_PLL_CLKIN_MASK				0xC  /* (0b00001100) */
#define AIC31XX_PLL_CLKIN_SHIFT				2
#define AIC31XX_PLL_CLKIN_MCLK				0
#define AIC31XX_CODEC_CLKIN_MASK			0x3  /* (0b00000011) */
#define AIC31XX_CODEC_CLKIN_SHIFT			0
#define AIC31XX_CODEC_CLKIN_PLL				0x3
/* PLL P and R-VAL register*/
#define AIC31XX_PLLPR					0x85
#define AIC31XX_PLL_MASK				0x7f
/* PLL J-VAL register*/
#define AIC31XX_PLLJ					0x86
/* PLL D-VAL MSB register */
#define AIC31XX_PLLDMSB					0x87
/* PLL D-VAL LSB register */
#define AIC31XX_PLLDLSB					0x88
/* DAC NDAC_VAL register*/
#define AIC31XX_NDAC				0x8B
/* DAC MDAC_VAL register */
#define AIC31XX_MDAC				0x8C
/*DAC OSR setting register1, MSB value*/
#define AIC31XX_DOSRMSB				0x8D
/*DAC OSR setting register 2, LSB value*/
#define AIC31XX_DOSRLSB				0x8E
#define AIC31XX_MINI_DSP_INPOL				0x90
/*Clock setting register 8, PLL*/
#define AIC31XX_NADC				0x92
/*Clock setting register 9, PLL*/
#define AIC31XX_MADC				0x93
/*ADC Oversampling (AOSR) Register*/
#define AIC31XX_AOSR				0x94
/*Clock setting register 9, Multiplexers*/
#define AIC31XX_CLKOUTMUX				0x99
/*Clock setting register 10, CLOCKOUT M divider value*/
#define AIC31XX_CLKOUTMVAL					0x9A
/*Audio Interface Setting Register 1*/
#define AIC31XX_IFACE1			0x9B
/*Audio Data Slot Offset Programming*/
#define AIC31XX_DATA_OFFSET			0x9C
/*Audio Interface Setting Register 2*/
#define AIC31XX_IFACE2			0x9D
/*Clock setting register 11, BCLK N Divider*/
#define AIC31XX_BCLKN					0x9E
/*Audio Interface Setting Register 3, Secondary Audio Interface*/
#define AIC31XX_IFACESEC1			0x9F
/*Audio Interface Setting Register 4*/
#define AIC31XX_IFACESEC2			0xA0
/*Audio Interface Setting Register 5*/
#define AIC31XX_IFACESEC3			0xA1
/* I2C Bus Condition */
#define AIC31XX_I2C				0xA2
/* ADC FLAG */
#define AIC31XX_ADCFLAG				0xA4
/* DAC Flag Registers */
#define AIC31XX_DACFLAG1				0xA5
#define AIC31XX_DACFLAG2				0xA6
#define AIC31XX_HPL_MASK			0x20
#define AIC31XX_HPR_MASK			0x02
#define AIC31XX_SPL_MASK			0x10
#define AIC31XX_SPR_MASK			0x01

/* Sticky Interrupt flag (overflow) */
#define AIC31XX_OFFLAG			0xA7

/* Sticky Interrupt flags 1 and 2 registers (DAC) */
#define AIC31XX_INTRDACFLAG				0xAC
#define AIC31XX_HPSCDETECT_MASK				0x80
#define AIC31XX_BUTTONPRESS_MASK			0x20
#define AIC31XX_HSPLUG_MASK				0x10
#define AIC31XX_LDRCTHRES_MASK				0x08
#define AIC31XX_RDRCTHRES_MASK				0x04
#define AIC31XX_DACSINT_MASK				0x02
#define AIC31XX_DACAINT_MASK				0x01

/* Sticky Interrupt flags 1 and 2 registers (ADC) */
#define AIC31XX_INTRADCFLAG				0xAD

/* Interrupt flags register */
#define AIC31XX_INTRFLAG				0xAE
#define AIC31XX_BTNPRESS_STATUS_MASK			0x20
#define AIC31XX_HEADSET_STATUS_MASK			0x10
#define AIC31XX_BTN_HS_STATUS_MASK (AIC31XX_BTNPRESS_STATUS_MASK \
					|AIC31XX_HEADSET_STATUS_MASK)

/* INT1 interrupt control */
#define AIC31XX_INT1CTRL				0xB0
#define AIC31XX_HSPLUGDET_MASK				0x80
#define AIC31XX_BUTTONPRESSDET_MASK			0x40
#define AIC31XX_DRCTHRES_MASK				0x20
#define AIC31XX_AGCNOISE_MASK				0x10
#define AIC31XX_OC_MASK					0x08
#define AIC31XX_ENGINE_MASK				0x04
#define AIC31XX_MULTIPLE_PULSES				0x01

/* INT2 interrupt control */
#define AIC31XX_INT2CTRL				0xB1

/* GPIO1 control */
#define AIC31XX_GPIO1					0xB3
#define AIC31XX_GPIO_D5_D2				0x3C /* (0b00111100) */
#define AIC31XX_GPIO_D2_SHIFT				2

#define AIC31XX_DACPRB					0xBC
/*ADC Instruction Set Register*/
#define AIC31XX_ADCPRB					0xBD
/*DAC channel setup register*/
#define AIC31XX_DACSETUP				0xBF
#define AIC31XX_SOFTSTEP_MASK				0x03
/*DAC Mute and volume control register*/
#define AIC31XX_DACMUTE					0xC0
#define AIC31XX_DACMUTE_MASK				0x0C
/*Left DAC channel digital volume control*/
#define AIC31XX_LDACVOL				0xC1
/*Right DAC channel digital volume control*/
#define AIC31XX_RDACVOL				0xC2
/* Headset detection */
#define AIC31XX_HSDETECT				0xC3
#define AIC31XX_HS_MASK					0x60 /* (0b01100000) */
#define AIC31XX_HP_MASK					0x20 /* (0b00100000) */
#define AIC31XX_JACK_DEBOUCE_MASK			0x1C /* (0b00011100) */
#define AIC31XX_BTN_DEBOUCE_MASK			0x3  /* (0b00000011) */
#define AIC31XX_ADCSETUP				0xD1
#define AIC31XX_ADCFGA				0xD2
#define AIC31XX_ADCMUTE_MASK			0x80
#define AIC31XX_ADCVOL				0xD3


/******************** Page 1 Registers **************************************/
/* Headphone drivers */
#define AIC31XX_HPDRIVER				0x11F
/* Class-D Speakear Amplifier */
#define AIC31XX_SPKAMP				0x120
/* HP Output Drivers POP Removal Settings */
#define AIC31XX_HPPOP				0x121
/* Output Driver PGA Ramp-Down Period Control */
#define AIC31XX_SPPGARAMP				0x122
/* DAC_L and DAC_R Output Mixer Routing */
#define AIC31XX_DACMIXERROUTE			0x123
/*Left Analog Vol to HPL */
#define AIC31XX_LANALOGHPL				0x124
/* Right Analog Vol to HPR */
#define AIC31XX_RANALOGHPR			0x125
/* Left Analog Vol to SPL */
#define AIC31XX_LANALOGSPL				0x126
/* Right Analog Vol to SPR */
#define AIC31XX_RANALOGSPR			0x127
/* HPL Driver */
#define AIC31XX_HPLGAIN				0x128
/* HPR Driver */
#define AIC31XX_HPRGAIN				0x129
/* SPL Driver */
#define AIC31XX_SPLGAIN				0x12A
/* SPR Driver */
#define AIC31XX_SPRGAIN				0x12B
/* HP Driver Control */
#define AIC31XX_HPCONTROL			0x12C
/*MICBIAS Configuration Register*/
#define AIC31XX_MICBIAS			0x12E
/* MIC PGA*/
#define AIC31XX_MICPGA				0x12F
/* Delta-Sigma Mono ADC Channel Fine-Gain Input Selection for P-Terminal */
#define AIC31XX_MICPGAPI				0x130
/* ADC Input Selection for M-Terminal */
#define AIC31XX_MICPGAMI				0x131
/* Input CM Settings */
#define AIC31XX_MICPGACM				0x132

/* Timer Clock MCLK Divider */
#define AIC31XX_TIMERCLOCK				0x210
#define AIC31XX_CLKSEL_MASK				0x80
#define AIC31XX_DIVIDER_MASK				0x7F

/* ****************** Page 3 Registers **************************************/

/* Timer Clock MCLK Divider */
#define AIC31XX_MCLKDIV				0x310

/* ****************** Page 8 Registers **************************************/
#define AIC31XX_DACADAPTIVE			0x801
#define AIC31XX_TIME_DELAY			5000
#define AIC31XX_LDACPWRSTATUS_MASK		0x80
#define AIC31XX_RDACPWRSTATUS_MASK		0x08
#define AIC31XX_ADCPWRSTATUS_MASK		0x40
#define AIC31XX_DELAY_COUNTER			100


#endif				/* _TLV320AIC31XX_H */
