/*
 * linux/sound/soc/codecs/tlv320aic326x_minidsp_config.c
 *
 * Copyright (C) 2012 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * The TLV320AIC3262 is a flexible, low-power, low-voltage stereo audio
 * codec with digital microphone inputs and programmable outputs.
 *
 * History:
 *
 * Rev 0.1   Added the multiconfig support           17-08-2011
 *
 * Rev 0.2   Migrated for aic3262 nVidia
 *           21-10-2011
 */

/*
 *****************************************************************************
 * INCLUDES
 *****************************************************************************
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <sound/soc.h>
#include <sound/core.h>
#include <sound/soc-dapm.h>
#include <sound/control.h>
#include <linux/time.h>		/* For timing computations */
#include "tlv320aic326x.h"
#include "tlv320aic326x_mini-dsp.h"

#if 0
#include "Patch_base_jazz_Rate48_pps_driver.h"
#include "Patch_base_main_Rate48_pps_driver.h"
#include "Patch_base_pop_Rate48_pps_driver.h"
#include "Patch_base_rock_Rate48_pps_driver.h"
#endif

#ifdef CONFIG_MINI_DSP

#define MAX_CONFIG_D_ARRAYS 4
#define MAX_CONFIG_A_ARRAYS 0
#define MAX_CONFIG_ARRAYS   4
#define MINIDSP_DMODE 0
#define MINIDSP_AMODE 1

/*
 *****************************************************************************
 * LOCAL STATIC DECLARATIONS
 *****************************************************************************
 */
static int multibyte_coeff_change(struct snd_soc_codec *codec, int);

static int m_control_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);
static int m_control_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);

/* k-control macros used for miniDSP related Kcontrols */
#define SOC_SINGLE_VALUE_M(xmax, xinvert) \
	((unsigned long)&(struct soc_mixer_control) \
	{.max = xmax, \
	.invert = xinvert})
#define SOC_SINGLE_M(xname, max, invert) \
{\
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = m_control_info, .get = m_control_get,\
	.put = m_control_put, \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
	.private_value = SOC_SINGLE_VALUE_M(max, invert) }
#define SOC_SINGLE_AIC3262_M(xname) \
{\
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = m_control_info, .get = m_control_get,\
	.put = m_control_put, \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
}


/* The Multi-Configurations generated through PPS GDE has been
 * named as Rock, Pop, Jazz and Main. These were the Configurations
 * that were used while testing this AUdio Driver. If the user
 * creates Process-flow with different names, it is advised to
 * modify the names present in the below array.
 */
static const char *multi_config_support_DAC[] = {
	"ROCK",
	"POP",
	"JAZZ",
	"MAIN",
};

/* SOC_ENUM Declaration and kControl for switching Configurations
 * at run-time.
 */
static const struct soc_enum aic3262_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(multi_config_support_DAC),
			multi_config_support_DAC);

static const struct snd_kcontrol_new aic3262_minidsp_controls1[] = {

	SOC_ENUM_EXT("Multiconfig support for DAC",
		aic3262_enum, m_control_get, m_control_put),

};


/*
 * multibyte_config
 *
 * This structure has been devised to maintain information about each
 * configuration provided with the PPS GDE Processflow. For each
 * configuration, the Driver needs to know where the starting offset
 * of the Coefficient change, Total Size of Coefficients being affected,
 * and the Instruction Sizes.
 * This has to be replicated for both miniDSP_A and miniDSP_D
 */

#if 0
struct multibyte_config {
	reg_value *regs;
	unsigned int d_coeff_start;
	unsigned int d_coeff_size;
	unsigned int d_inst_start;
	unsigned int d_inst_size;
	unsigned int a_coeff_start;
	unsigned int a_coeff_size;
	unsigned int a_inst_start;
	unsigned int a_inst_size;
} config_array[][2][MAX_CONFIG_ARRAYS] = {
	/* Process flow 1 */
	{
		{
			/* DAC */
			{rock_D_reg_values, 0, 67, 67, 0, 0, 0, 0, 0},
			{pop_D_reg_values, 0, 67, 67, 0, 0, 0, 0, 0},
			{jazz_D_reg_values, 0, 67, 67, 0, 0, 0, 0, 0},
			{main_D_reg_values, 0, 67, 67, 0, 0, 0, 0, 0},
		},
		/* ADC */
		{},
	},

	/* Process flow 2 */
	{
#if 0
		{
			{main, 0, 0, 0, 0, 0, 0, 0, 0},
			{pop, 0, 0, 0, 0, 0, 0, 0, 0},
			{jazz, 0, 0, 0, 0, 0, 0, 0, 0},
			{rock, 0, 0, 0, 0, 0, 0, 0, 0},
		},
		/* ADC */
		{},
#endif
	},
};

#else
struct multibyte_config {
	reg_value *regs;
	unsigned int d_coeff_start;
	unsigned int d_coeff_size;
	unsigned int d_inst_start;
	unsigned int d_inst_size;
	unsigned int a_coeff_start;
	unsigned int a_coeff_size;
	unsigned int a_inst_start;
	unsigned int a_inst_size;
} config_array[][2][MAX_CONFIG_ARRAYS] = {
	/* Process flow 1 */
	{
		{
			/* DAC */
				{},
		},
		/* ADC */
		{},
	},

	/* Process flow 2 */
	{
#if 0
		{
			{main, 0, 0, 0, 0, 0, 0, 0, 0},
			{pop, 0, 0, 0, 0, 0, 0, 0, 0},
			{jazz, 0, 0, 0, 0, 0, 0, 0, 0},
			{rock, 0, 0, 0, 0, 0, 0, 0, 0},
		},
		/* ADC */
		{},
#endif
	},
};
#endif

/*
 *----------------------------------------------------------------------------
 * Function : m_control_get
 * Purpose  : This function is to read data of new control for
 *            program the AIC3262 registers.
 *
 *----------------------------------------------------------------------------
 */
static int m_control_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{

	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct aic3262_priv *aic326x = snd_soc_codec_get_drvdata(codec);
	u32 val = 0;
	u32 mode = aic326x->process_flow;


	if (!strcmp(kcontrol->id.name, "Multiconfig support for DAC"))
		val = aic326x->current_dac_config[mode];
	else if (!strcmp(kcontrol->id.name, "Multiconfig support for ADC"))
		val = aic326x->current_adc_config[mode];


	ucontrol->value.integer.value[0] = val;
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : m_new_control_put
 * Purpose  : new_control_put is called to pass data from user/application to
 *            the driver.
 *
 *----------------------------------------------------------------------------
 */
static int m_control_put(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct aic3262_priv *aic326x = snd_soc_codec_get_drvdata(codec);
	u32 val;
	u8 value;
	int pf = aic326x->process_flow;
	struct multibyte_config *array;

	val = ucontrol->value.integer.value[0];
	if (!strcmp(kcontrol->id.name, "Multiconfig support for DAC")) {
		if (aic326x->process_flow == MINIDSP_DMODE) {
			if (val > MAX_CONFIG_D_ARRAYS) {
				dev_err(codec->dev, "Value not in range\n");
				return -1;
			}

			value = aic3262_read(codec, ADC_CHANNEL_POW);
			value = aic3262_read(codec, PASI_DAC_DP_SETUP);

			array = &config_array[pf][MINIDSP_DMODE][val];
			minidsp_i2c_multibyte_transfer(codec,
				array->regs,
				(array->d_inst_size + array->d_coeff_size));


			/*coefficent buffer change*/
			multibyte_coeff_change(codec, 0x50);

			minidsp_i2c_multibyte_transfer(codec,
				array->regs,
				(array->d_inst_size + array->d_coeff_size));



			value = aic3262_read(codec, ADC_CHANNEL_POW);
			value = aic3262_read(codec, PASI_DAC_DP_SETUP);
		}
		aic326x->current_dac_config[pf] = val;

	} else {
		if (aic326x->process_flow == MINIDSP_AMODE) {
			if (val > MAX_CONFIG_A_ARRAYS) {
				dev_err(codec->dev, "Value not in range\n");
				return -1;
			}
			array = &config_array[pf][MINIDSP_AMODE][val];
			minidsp_i2c_multibyte_transfer(codec,
				array->regs,
				(array->a_inst_size + array->a_coeff_size));
		}
		aic326x->current_adc_config[pf] = val;
	}
	return val;
}


/*
 *--------------------------------------------------------------------------
 * Function : aic3262_add_multiconfig_controls
 * Purpose :  Configures the AMIXER Control Interfaces that can be exercised by
 *            the user at run-time. Utilizes the  the snd_adaptive_controls[]
 *            array to specify two run-time controls.
 *---------------------------------------------------------------------------
 */
int aic3262_add_multiconfig_controls(struct snd_soc_codec *codec)
{
	int i, err;

	DBG(KERN_INFO
		"#%s: Invoked to add controls for Multi-Configuration\n",
		__func__);

	/* add mode k control */
	for (i = 0; i < ARRAY_SIZE(aic3262_minidsp_controls1); i++) {
		err = snd_ctl_add(codec->card->snd_card,
				snd_ctl_new1(&aic3262_minidsp_controls1[i],
				codec));
		if (err < 0) {
			printk(KERN_ERR
			"Cannot add controls for mulibyte configuration\n");
			return err;
		}
	}
	DBG(KERN_INFO "#%s: Completed control addition.\n", __func__);
	return 0;
}

/*
 *--------------------------------------------------------------------------
 * Function : minidsp_multiconfig
 * Purpose :  Function which is invoked when user changes the configuration
 *            at run-time. Internally configures/switches both
 *            miniDSP_D and miniDSP_A Coefficient arrays.
 *---------------------------------------------------------------------------
 */
void minidsp_multiconfig(struct snd_soc_codec *codec,
	reg_value *a_patch, int a_size,	reg_value *d_patch, int d_size)
{
	int adc_status,dac_status;
	int (*ptransfer)(struct snd_soc_codec *codec,
			reg_value *program_ptr, int size);
	printk(KERN_INFO "======in the config_multiconfiguration function====\n");
#ifndef MULTIBYTE_I2C
		ptransfer = byte_i2c_array_transfer;
#else
		ptransfer = minidsp_i2c_multibyte_transfer;
#endif
	//DBG(KERN_INFO "#%s: Invoked for miniDSP Mode %d\n", __func__, mode);

	adc_status=aic3262_read(codec,ADC_FLAG_R1);
	printk("ADC STATUS = %x", adc_status);

	dac_status=aic3262_read(codec,DAC_FLAG_R1);
	printk("DAC STATUS = %x", dac_status);

	#if 0
	/* Switching off the adaptive filtering for loading the patch file*/
	aic3262_change_book(codec, 40);
	val1 = i2c_smbus_read_byte_data(codec->control_data, 1);
	aic3262_write(codec, 1, (val1&0xfb));

	aic3262_change_book(codec, 80);
	val2 = i2c_smbus_read_byte_data(codec->control_data, 1);
	aic3262_write(codec, 1, (val2&0xfb));

	#endif

	/*apply patches to both pages (mirrored coeffs). this works when DSP are stopped*/
	/* to apply patches when DSPs are runnig, the 'buffer swap' has to be executed, which is
	    done in the buffer swap section below*/

	if (a_size)
		ptransfer	(codec, a_patch, a_size);
	if (d_size)
		ptransfer	(codec, d_patch, d_size);


		/*swap buffers for both a&d only if DSPs are running*/
              if((dac_status & 0x80) || (dac_status & 0x8))
              {
  		   multibyte_coeff_change(codec, 0x50);/*swap DSP D Buffer*/
		   if (d_size)
		   ptransfer(codec, d_patch, d_size); /*apply patch after swapping buffer*/
              }

              if((adc_status & 0x40) || (adc_status & 0x4))
              {
		  multibyte_coeff_change(codec, 0x28);/*swap DSP A Buffer*/
		  if (a_size)
		  ptransfer(codec, a_patch, a_size); /*apply patch after swapping buffer*/
              }

	#if 0
	if (a_size)
		ptransfer	(codec, a_patch, a_size);
	if (d_size)
		ptransfer	(codec, d_patch, d_size);
	#endif


	return ;
}

/*
 *--------------------------------------------------------------------------
 * Function : config_multibyte_for_mode
 * Purpose :  Function which is invoked when user changes the configuration
 *            at run-time. Internally configures/switches both
 *            miniDSP_D and miniDSP_A Coefficient arrays.
 *---------------------------------------------------------------------------
 */
static int multibyte_coeff_change(struct snd_soc_codec *codec, int bk)
{

	u8 value[2], swap_reg_pre, swap_reg_post;
	struct i2c_client *i2c;
	i2c = codec->control_data;

	aic3262_change_book(codec, bk);
	aic3262_change_page(codec, 0);

	value[0] = 1;

	if (i2c_master_send(i2c, value, 1) != 1)
		printk(KERN_ERR "Can not write register address\n");
	else {
		/* Read the Value of the Page 8 Register 1 which controls the
		   Adaptive Switching Mode */
		if (i2c_master_recv(i2c, value, 1) != 1) {
			printk(KERN_ERR "Can not read codec registers\n");
			goto err;
		}
		swap_reg_pre = value[0];

		/* Write the Register bit updates */
		value[1] = value[0] | 1;
		value[0] = 1;

		if (i2c_master_send(i2c, value, 2) != 2) {
			printk(KERN_ERR "Can not write register address\n");
			goto err;
		}
              /*before verifying for buffer_swap, make sure we give one
              frame delay, because the buffer swap happens at the end of the frame */
              mdelay(5);
		value[0] = 1;
		/* verify buffer swap */
		if (i2c_master_send(i2c, value, 1) != 1)
			printk(KERN_ERR "Can not write register address\n");

		/* Read the Value of the Page 8 Register 1 which controls the
		   Adaptive Switching Mode */
		if (i2c_master_recv(i2c, &swap_reg_post, 1) != 1)
			printk(KERN_ERR "Can not read codec registers\n");

		if ((swap_reg_pre == 4 && swap_reg_post == 6)
			|| (swap_reg_pre == 6 && swap_reg_post == 4))
			printk(KERN_INFO "Buffer swap success\n");
		else
			printk(KERN_ERR
			"Buffer swap...FAILED\nswap_reg_pre=%x, swap_reg_post=%x\n",
			swap_reg_pre, swap_reg_post);

		aic3262_change_book(codec, 0x00);
	}

err:
	return 0;
}



#endif

MODULE_DESCRIPTION("ASoC AIC3262 miniDSP multi-configuration");
MODULE_AUTHOR("Barani Prashanth <gvbarani@mistralsolutions.com>");
MODULE_LICENSE("GPL");
