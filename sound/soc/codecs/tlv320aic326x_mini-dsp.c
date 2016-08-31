/*
 * linux/sound/soc/codecs/tlv320aic326x_mini-dsp.c
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
 * Rev 0.1   Added the miniDSP Support	 01-03-2011
 *
 * Rev 0.2   Updated the code-base for miniDSP switching and
 *	 mux control update.	21-03-2011
 *
 * Rev 0.3   Updated the code-base to support Multi-Configuration feature
 *		   of PPS GDE
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

#include "base_main_Rate48_pps_driver.h"
#include "second_rate_pps_driver.h"
//#include "one_mic_aec_nc_latest.h"
#ifdef CONFIG_MINI_DSP

#ifdef REG_DUMP_MINIDSP
static void aic3262_dump_page(struct i2c_client *i2c, u8 page);
#endif

/*
 *****************************************************************************
 * LOCAL STATIC DECLARATIONS
 *****************************************************************************
 */
static int m_control_info(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_info *uinfo);
static int m_control_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol);
static int m_control_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol);

/*
 *****************************************************************************
 * MINIDSP RELATED GLOBALS
 *****************************************************************************
 */
/* The below variable is used to maintain the I2C Transactions
 * to be carried out during miniDSP switching.
 */
 #if 1
minidsp_parser_data dsp_parse_data[MINIDSP_PARSER_ARRAY_SIZE*2];

struct i2c_msg i2c_transaction[MINIDSP_PARSER_ARRAY_SIZE * 2];
/* Total count of I2C Messages are stored in the i2c_count */
int i2c_count;

/* The below array is used to store the burst array for I2C Multibyte
 * Operations
 */
minidsp_i2c_page i2c_page_array[MINIDSP_PARSER_ARRAY_SIZE];
int i2c_page_count;
#else
minidsp_parser_data dsp_parse_data;

struct i2c_msg i2c_transaction;
/* Total count of I2C Messages are stored in the i2c_count */
int i2c_count;

/* The below array is used to store the burst array for I2C Multibyte
 * Operations
 */
minidsp_i2c_page i2c_page_array;
int i2c_page_count;
#endif

/* kcontrol structure used to register with ALSA Core layer */
static struct snd_kcontrol_new snd_mux_controls[MAX_MUX_CONTROLS];

/* mode variables */
static int amode;
static int dmode;

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

/*
 * aic3262_minidsp_controls
 *
 * Contains the list of the Kcontrol macros required for modifying the
 * miniDSP behavior at run-time.
 */
static const struct snd_kcontrol_new aic3262_minidsp_controls[] = {
	SOC_SINGLE_AIC3262_M("Minidsp mode") ,
	SOC_SINGLE_AIC3262_M("ADC Adaptive mode Enable") ,
	SOC_SINGLE_AIC3262_M("DAC Adaptive mode Enable") ,
	SOC_SINGLE_AIC3262_M("Dump Regs Book0") ,
	SOC_SINGLE_AIC3262_M("Verify minidsp program") ,
};

#ifdef REG_DUMP_MINIDSP
/*
 *----------------------------------------------------------------------------
 * Function : aic3262_dump_page
 * Purpose  : Read and display one codec register page, for debugging purpose
 *----------------------------------------------------------------------------
 */
static void aic3262_dump_page(struct i2c_client *i2c, u8 page)
{
	int i;
	u8 data;
	u8 test_page_array[256];

	aic3262_change_page(codec, page);

	data = 0x0;

	i2c_master_send(i2c, data, 1);
	i2c_master_recv(i2c, test_page_array, 128);

	DBG("\n------- MINI_DSP PAGE %d DUMP --------\n", page);
	for (i = 0; i < 128; i++)
		DBG(KERN_INFO " [ %d ] = 0x%x\n", i, test_page_array[i]);

}
#endif

/*
 *----------------------------------------------------------------------------
 * Function : update_kcontrols
 * Purpose  : Given the miniDSP process flow, this function reads the
 *			corresponding Page Numbers and then performs I2C Read for those
 *			Pages.
 *----------------------------------------------------------------------------
 */
void update_kcontrols(struct snd_soc_codec *codec, int process_flow)
{
	int i, val1, array_size;
	char **knames;
	control *cntl;

#if 0
	if (process_flow == 1) {
		knames = Second_Rate_MUX_control_names;
		cntl = Second_Rate_MUX_controls;
		array_size = ARRAY_SIZE(Second_Rate_MUX_controls);
	} else {
#endif
		knames = main44_MUX_control_names;
		cntl = main44_MUX_controls;
		array_size = ARRAY_SIZE(main44_MUX_controls);
//	}

	DBG(KERN_INFO "%s: ARRAY_SIZE = %d\tmode=%d\n", __func__,
			array_size, process_flow);
	for (i = 0; i < array_size; i++) {
		aic3262_change_book(codec, cntl[i].control_book);
		aic3262_change_page(codec, cntl[i].control_page);
		val1 = i2c_smbus_read_byte_data(codec->control_data,
				cntl[i].control_base);
		snd_mux_controls[i].private_value = 0;
	}
}

/*
 *----------------------------------------------------------------------------
 * Function : byte_i2c_array_transfer
 * Purpose  : Function used only for debugging purpose. This function will
 *			be used while switching miniDSP Modes register by register.
 *			This needs to be used only during development.
 *-----------------------------------------------------------------------------
 */
 #if 1
int byte_i2c_array_transfer(struct snd_soc_codec *codec,
				reg_value *program_ptr,
				int size)
{
	int j;
	u8 buf[3];

	for (j = 0; j < size; j++) {
		/* Check if current Reg offset is zero */
		if (program_ptr[j].reg_off == 0) {
			/* Check for the Book Change Request */
			if ((j < (size - 1)) &&
				(program_ptr[j+1].reg_off == 127)) {
				aic3262_change_book(codec,
					program_ptr[j+1].reg_val);
			/* Increment for loop counter across Book Change */
				j++;
				continue;
		}
		/* Check for the Page Change Request in Current book */
		aic3262_change_page(codec, program_ptr[j].reg_val);
		continue;
		}

		buf[AIC3262_REG_OFFSET_INDEX] = program_ptr[j].reg_off % 128;
		buf[AIC3262_REG_DATA_INDEX] =
				program_ptr[j].reg_val & AIC3262_8BITS_MASK;

		if (codec->hw_write(codec->control_data, buf, 2) != 2) {
			printk(KERN_ERR "Error in i2c write\n");
			return -EIO;
		}
	}
	aic3262_change_book(codec, 0);
	return 0;
}

#else
int byte_i2c_array_transfer(struct snd_soc_codec *codec,
				reg_value *program_ptr,
				int size)
{
	int j;
	u8 buf[3];
	printk(KERN_INFO "%s: started with array size %d\n", __func__, size);
	for (j = 0; j < size; j++) {
		/* Check if current Reg offset is zero */
		buf[AIC3262_REG_OFFSET_INDEX] = program_ptr[j].reg_off % 128;
		buf[AIC3262_REG_DATA_INDEX] =
				program_ptr[j].reg_val & AIC3262_8BITS_MASK;

		if (codec->hw_write(codec->control_data, buf, 2) != 2) {
			printk(KERN_ERR "Error in i2c write\n");
			return -EIO;
		}
	}
	printk(KERN_INFO "%s: ended\n", __func__);
	return 0;
}
#endif
/*
 *----------------------------------------------------------------------------
 * Function : byte_i2c_array_read
 * Purpose  : This function is used to perform Byte I2C Read. This is used
 *			only for debugging purposes to read back the Codec Page
 *			Registers after miniDSP Configuration.
 *----------------------------------------------------------------------------
 */
int byte_i2c_array_read(struct snd_soc_codec *codec,
			reg_value *program_ptr, int size)
{
	int j;
	u8 val1;
	u8 cur_page = 0;
	u8 cur_book = 0;
	for (j = 0; j < size; j++) {
		/* Check if current Reg offset is zero */
		if (program_ptr[j].reg_off == 0) {
			/* Check for the Book Change Request */
			if ((j < (size - 1)) &&
				(program_ptr[j+1].reg_off == 127)) {
				aic3262_change_book(codec,
					program_ptr[j+1].reg_val);
				cur_book = program_ptr[j+1].reg_val;
			/* Increment for loop counter across Book Change */
				j++;
				continue;
			}
			/* Check for the Page Change Request in Current book */
			aic3262_change_page(codec, program_ptr[j].reg_val);
			cur_page = program_ptr[j].reg_val;
			continue;
		}

		val1 = i2c_smbus_read_byte_data(codec->control_data,
				program_ptr[j].reg_off);
		if (val1 < 0)
			printk(KERN_ERR "Error in smbus read\n");

		if(val1 != program_ptr[j].reg_val)
			/*printk(KERN_INFO "mismatch [%d][%d][%d] = %x %x\n",
			cur_book, cur_page, program_ptr[j].reg_off, val1, program_ptr[j].reg_val);*/
		DBG(KERN_INFO "[%d][%d][%d]= %x\n",
			cur_book, cur_page, program_ptr[j].reg_off, val1);
	}
	aic3262_change_book(codec, 0);
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : minidsp_get_burst
 * Purpose  : Format one I2C burst for transfer from mini dsp program array.
 *			This function will parse the program array and get next burst
 *			data for doing an I2C bulk transfer.
 *----------------------------------------------------------------------------
 */
static void
minidsp_get_burst(reg_value *program_ptr,
				int program_size,
				minidsp_parser_data *parse_data)
{
	int index = parse_data->current_loc;
	int burst_write_count = 0;

	/*DBG("GET_BURST: start\n");*/
	/* check if first location is page register, and populate page addr */
	if (program_ptr[index].reg_off == 0) {
		if ((index < (program_size - 1)) &&
			(program_ptr[index+1].reg_off == 127)) {
			parse_data->book_change = 1;
			parse_data->book_no = program_ptr[index+1].reg_val;
			index += 2;
			goto finish_out;

		}
		parse_data->page_num = program_ptr[index].reg_val;
		parse_data->burst_array[burst_write_count++] =
			program_ptr[index].reg_off;
		parse_data->burst_array[burst_write_count++] =
			program_ptr[index].reg_val;
		index++;
		goto finish_out;
	}

	parse_data->burst_array[burst_write_count++] =
			program_ptr[index].reg_off;
	parse_data->burst_array[burst_write_count++] =
			program_ptr[index].reg_val;
	index++;

	for (; index < program_size; index++) {
		if (program_ptr[index].reg_off !=
				(program_ptr[index - 1].reg_off + 1))
			break;
		else
			parse_data->burst_array[burst_write_count++] =
				program_ptr[index].reg_val;

	}
finish_out:
	parse_data->burst_size = burst_write_count;
	if (index == program_size)
		/* parsing completed */
		parse_data->current_loc = MINIDSP_PARSING_END;
	else
		parse_data->current_loc = index;
	/*DBG("GET_BURST: end\n");*/
}
/*
 *----------------------------------------------------------------------------
 * Function : minidsp_i2c_multibyte_transfer
 * Purpose  : Function used to perform multi-byte I2C Writes. Used to configure
 *			the miniDSP Pages.
 *----------------------------------------------------------------------------
 */
 #if 1
int
minidsp_i2c_multibyte_transfer(struct snd_soc_codec *codec,
					reg_value *program_ptr,
					int program_size)
{
	struct i2c_client *client = codec->control_data;

	minidsp_parser_data parse_data;
	int count = 0;

#ifdef DEBUG_MINIDSP_LOADING
	int i = 0, j = 0;
#endif
	/* point the current location to start of program array */
	parse_data.current_loc = 0;
	parse_data.page_num = 0;
	parse_data.book_change = 0;
	parse_data.book_no = 0;

	DBG(KERN_INFO "size is : %d", program_size);
	do {
		do {
			/* Get first burst data */
			minidsp_get_burst(program_ptr, program_size,
					&parse_data);
			if (parse_data.book_change == 1)
				break;
			dsp_parse_data[count] = parse_data;

			i2c_transaction[count].addr = client->addr;
			i2c_transaction[count].flags =
				client->flags & I2C_M_TEN;
			i2c_transaction[count].len =
				dsp_parse_data[count].burst_size;
			i2c_transaction[count].buf =
				dsp_parse_data[count].burst_array;

#ifdef DEBUG_MINIDSP_LOADING
			DBG(KERN_INFO
			"i: %d\taddr: %d\tflags: %d\tlen: %d\tbuf:",
			i, client->addr, client->flags & I2C_M_TEN,
			dsp_parse_data[count].burst_size);

			for (j = 0; j <= dsp_parse_data[count].burst_size; j++)
				DBG(KERN_INFO "%x ",
					dsp_parse_data[i].burst_array[j]);

			DBG(KERN_INFO "\n\n");
			i++;
#endif

			count++;
			/* Proceed to the next burst reg_addr_incruence */
		} while (parse_data.current_loc != MINIDSP_PARSING_END);

		if (count > 0) {
			if (i2c_transfer(client->adapter,
				i2c_transaction, count) != count) {
				printk(KERN_ERR "Write burst i2c data error!\n");
			}
		}
		if (parse_data.book_change == 1) {
			aic3262_change_book(codec, parse_data.book_no);
			parse_data.book_change = 0;
		}
	} while (parse_data.current_loc != MINIDSP_PARSING_END);
	aic3262_change_book(codec, 0);
	return 0;
}
#else
int
minidsp_i2c_multibyte_transfer(struct snd_soc_codec *codec,
					reg_value *program_ptr,
					int program_size)
{
	struct i2c_client *client = codec->control_data;

	minidsp_parser_data parse_data;
	int count = 1;

#ifdef DEBUG_MINIDSP_LOADING
	int i = 0, j = 0;
#endif
	/* point the current location to start of program array */
	parse_data.current_loc = 0;
	parse_data.page_num = 0;
	parse_data.book_change = 0;
	parse_data.book_no = 0;

	DBG(KERN_INFO "size is : %d", program_size);

	do {
		/* Get first burst data */
		minidsp_get_burst(program_ptr, program_size,
				&parse_data);

		dsp_parse_data = parse_data;

		i2c_transaction.addr = client->addr;
		i2c_transaction.flags =
			client->flags & I2C_M_TEN;
		i2c_transaction.len =
			dsp_parse_data.burst_size;
		i2c_transaction.buf =
			dsp_parse_data.burst_array;

#ifdef DEBUG_MINIDSP_LOADING
			DBG(KERN_INFO
			"i: %d\taddr: %d\tflags: %d\tlen: %d\tbuf:",
			i, client->addr, client->flags & I2C_M_TEN,
			dsp_parse_data.burst_size);

			for (j = 0; j <= dsp_parse_data.burst_size; j++)
				printk( "%x ",
					dsp_parse_data.burst_array[j]);

			DBG(KERN_INFO "\n\n");
			i++;
#endif

		if (i2c_transfer(client->adapter,
			&i2c_transaction, count) != count) {
			printk(KERN_ERR "Write burst i2c data error!\n");
		}
		if (parse_data.book_change == 1) {
			aic3262_change_book(codec, parse_data.book_no);
			parse_data.book_change = 0;
		}
		/* Proceed to the next burst reg_addr_incruence */
	} while (parse_data.current_loc != MINIDSP_PARSING_END);

	return 0;
}
#endif
/*
* Process_Flow Structure
* Structure used to maintain the mapping of each PFW like the miniDSP_A
* miniDSP_D array values and sizes. It also contains information about
* the patches required for each patch.
*/
struct process_flow{
	int init_size;
	reg_value *miniDSP_init;
	int A_size;
	reg_value *miniDSP_A_values;
	int D_size;
	reg_value *miniDSP_D_values;
	int post_size;
	reg_value *miniDSP_post;
	struct minidsp_config {
		int a_patch_size;
		reg_value *a_patch;
		int d_patch_size;
		reg_value *d_patch;
	} configs[MAXCONFIG];

} miniDSP_programs[]  = {
  	{
	ARRAY_SIZE(main44_REG_Section_init_program), main44_REG_Section_init_program,
  	ARRAY_SIZE(main44_miniDSP_A_reg_values),main44_miniDSP_A_reg_values,
  	ARRAY_SIZE(main44_miniDSP_D_reg_values),main44_miniDSP_D_reg_values,
  	ARRAY_SIZE(main44_REG_Section_post_program),main44_REG_Section_post_program,
  	{

		{ 0, 0, 0, 0},
		{ 0, 0, 0, 0},
		{ 0, 0, 0, 0},
		{ 0, 0, 0, 0},


	},
},
	{
	ARRAY_SIZE(base_speaker_SRS_REG_init_Section_program),base_speaker_SRS_REG_init_Section_program,
  	ARRAY_SIZE(base_speaker_SRS_miniDSP_A_reg_values),base_speaker_SRS_miniDSP_A_reg_values,
  	ARRAY_SIZE(base_speaker_SRS_miniDSP_D_reg_values),base_speaker_SRS_miniDSP_D_reg_values,
  	ARRAY_SIZE(base_speaker_SRS_REG_post_Section_program),base_speaker_SRS_REG_post_Section_program,

	{
			{0, 0,	ARRAY_SIZE(SRS_ON_miniDSP_D_reg_values), SRS_ON_miniDSP_D_reg_values},
			{0, 0,	ARRAY_SIZE(SRS_OFF_miniDSP_D_reg_values),SRS_OFF_miniDSP_D_reg_values},
			{0, 0, 0, 0},
			{0, 0, 0, 0},
		},
},
#if 0
	{ARRAY_SIZE(spkr_srs_REG_Section_init_program),spkr_srs_REG_Section_init_program,
  	ARRAY_SIZE(spkr_srs_miniDSP_A_reg_values),spkr_srs_miniDSP_A_reg_values,
  	ARRAY_SIZE(spkr_srs_miniDSP_D_reg_values),spkr_srs_miniDSP_D_reg_values,
  	ARRAY_SIZE(spkr_srs_REG_Section_post_program),spkr_srs_REG_Section_post_program,
	{
  		{ 0, 0, 0, 0},
		{ 0, 0, 0, 0},
		{ 0, 0, 0, 0},
		{ 0, 0, 0, 0},

	},
},
#endif
};

int
set_minidsp_mode(struct snd_soc_codec *codec, int new_mode, int new_config)
{
	struct aic3262_priv *aic326x;
	struct snd_soc_dapm_context *dapm;
	struct process_flow *  pflows = &miniDSP_programs[new_mode];
	u8 pll_pow, ndac_pow, mdac_pow, nadc_pow;
	u8 adc_status,dac_status;

	int (*ptransfer)(struct snd_soc_codec *codec, reg_value *program_ptr,
								int size);

	printk("%s:New Switch mode = %d New Config= %d\n", __func__, new_mode,new_config);

	if (codec == NULL) {
		printk(KERN_INFO "%s codec is NULL\n", __func__);
		return 0;
	}
	aic326x = snd_soc_codec_get_drvdata(codec);
	dapm = &codec->dapm;

	printk(KERN_INFO "%s:New Switch mode = %d New Config= %d\n", __func__,
							new_mode, new_config);

	if (new_mode >= ARRAY_SIZE(miniDSP_programs))
		return 0; //  error condition
		if (new_config > MAXCONFIG)
			return 0;
#ifndef MULTIBYTE_I2C
		ptransfer = byte_i2c_array_transfer;
#else
		ptransfer = minidsp_i2c_multibyte_transfer;
#endif
	if (new_mode !=  aic326x->process_flow) {

		printk("== From PFW %d to PFW %d==\n", aic326x->process_flow , new_mode);

		/* Change to book 0 page 0 and turn off the DAC and snd_soc_dapm_disable_piADC,
		* while turning them down, poll for the power down completion.
		*/
		   aic3262_change_page(codec, 0);
		   aic3262_change_book(codec, 0);

#if 0
		reg63 = aic3262_read(codec, PASI_DAC_DP_SETUP);
		aic3262_write(codec, PASI_DAC_DP_SETUP, (reg63 & ~0xC0));/*dac power down*/
		mdelay (5);
		counter = 0;
		reg = DAC_FLAG_R1;

   		  dac_status = aic3262_read(codec, reg);

		do {
			dac_status = snd_soc_read(codec, reg);
			counter++;struct snd_soc_dapm_context *dapm
			mdelay(5);
		} while ((counter < 200) && ((dac_status & 0x88) == 1));
		printk (KERN_INFO  "#%s: Polled Register %d Bits set 0x%X counter %d\n",
				__func__, reg, dac_status, counter);snd_soc_dapm_disable_pi
		struct snd_soc_dapm_context *dapm
		   reg81= aic3262_read(codec, ADC_CHANNEL_POW);
		aic3262_write(codec, ADC_CHANNEL_POW, (reg81 & ~0xC0));/*adc power down*/
		mdelay (5);

		adc_status=aic3262_read(codec,ADC_FLAG_R1);
		counter = 0;
		reg = ADC_FLAG_R1;
		do {
			adc_status = snd_soc_read(codec, reg);
			counter++;
			mdelay(5);
		} while ((counter < 200) && ((adc_status & 0x44) == 1));

		printk (KERN_INFO  "#%s: Polled Register %d Bits set 0x%X counter %d\n",
				__func__, reg, adc_status, counter);

		dac_status = snd_soc_read(codec, DAC_FLAG_R1);
		adc_status = snd_soc_read (codec, ADC_FLAG_R1);

		printk (KERN_INFO "#%s: Initial DAC_STATUS 0x%x ADC_STATUS 0x%X\n",
			__func__, dac_status, adc_status);

#endif
		/* Instead of hard-coding the switching off DAC and ADC, we will use the DAPM
		* to switch off the Playback Paths and the ADC
		*/
		snd_soc_dapm_disable_pin( dapm, "Headphone Jack");
		snd_soc_dapm_disable_pin( dapm, "EarPiece");
		snd_soc_dapm_disable_pin( dapm, "Int Spk");
		snd_soc_dapm_disable_pin( dapm, "SPK out");
		snd_soc_dapm_disable_pin( dapm, "Line Out");

		snd_soc_dapm_disable_pin( dapm, "Mic Jack");
		snd_soc_dapm_disable_pin( dapm, "Linein");
		snd_soc_dapm_disable_pin( dapm, "Int Mic");

		//snd_soc_dapm_disable_pin (codec, "Left DAC");
		//snd_soc_dapm_disable_pin (codec, "Right DAC");
		//snd_soc_dapm_disable_pin (codec, "Left ADC");
		//snd_soc_dapm_disable_pin (codec, "Right ADC");
		snd_soc_dapm_sync(dapm);
		mdelay(10);

		mdac_pow = aic3262_read(codec, MDAC_DIV_POW_REG);
		aic3262_write(codec, MDAC_DIV_POW_REG, (mdac_pow & ~0x80));/*mdac power down*/
		mdelay(5);
		nadc_pow = aic3262_read(codec, MADC_DIV_POW_REG);
		aic3262_write(codec, MADC_DIV_POW_REG, (nadc_pow & ~0x80));/*madc power down*/
		mdelay(5);
		pll_pow = aic3262_read(codec, PLL_PR_POW_REG);
		aic3262_write(codec, PLL_PR_POW_REG, (pll_pow & ~0x80));/*pll power down*/
		mdelay(5);
		ndac_pow = aic3262_read(codec, NDAC_DIV_POW_REG);
		aic3262_write(codec, NDAC_DIV_POW_REG, (ndac_pow & ~0x80)); /*ndac power down*/
		mdelay(5);

		dac_status = snd_soc_read(codec, DAC_FLAG_R1);
		adc_status = snd_soc_read (codec, ADC_FLAG_R1);

		printk (KERN_INFO "#%s: Before Switching DAC_STATUS 0x%x ADC_STATUS 0x%X\n",
			__func__, dac_status, adc_status);

		mdelay (10);
	 	ptransfer(codec, pflows->miniDSP_init,		   pflows->init_size);
		   ptransfer(codec, pflows->miniDSP_A_values,  pflows->A_size);
		   ptransfer(codec, pflows->miniDSP_D_values,  pflows->D_size);
		   ptransfer(codec, pflows->miniDSP_post,		 pflows->post_size);


		aic326x->process_flow = new_mode;

		aic3262_change_page(codec, 0);
		   	aic3262_change_book(codec, 0);
#if 0

		/* After the miniDSP Programming is completed, power up the DAC and ADC
		* and poll for its power up operation.
		*/

		aic3262_write(codec, PASI_DAC_DP_SETUP, reg63);/*reverting the old DAC values */
		mdelay(5);

		/* Poll for DAC Power-up first */
		/* For DAC Power-up and Power-down event, we will poll for
		* Book0 Page0 Register 37
		*/
		reg = DAC_FLAG_R1;
		counter = 0;
		do {
			dac_status = snd_soc_read(codec, reg);
			counter++;
			mdelay(5);
		} while ((counter < 200) && ((dac_status & 0x88) == 0));

		printk (KERN_INFO  "#%s: Polled Register %d Bits set 0x%X counter %d\n",
				__func__, reg, dac_status, counter);

		aic3262_write(codec, ADC_CHANNEL_POW, reg81);/*reverting the old ADC values*/
		mdelay (5);
		/* For ADC Power-up and Power-down event, we will poll for
		* Book0 Page0 Register 36
		*/
		reg = ADC_FLAG_R1;
		counter = 0;
		do {
			adc_status = snd_soc_read(codec, reg);
			counter++;
			mdelay(5);
		} while ((counter < 200) && ((adc_status & 0x44) == 0));

		printk (KERN_INFO  "#%s: Polled Register %d Bits set 0x%X counter %d\n",
				__func__, reg, adc_status, counter);
		aic3262_write(codec, PLL_PR_POW_REG, pll_pow);/*reverting the old pll values*/
		mdelay(10);

		aic3262_write(codec, MDAC_DIV_POW_REG, mdac_pow);/*reverting the old mdac values*/
		mdelay(5);
		aic3262_write(codec, MADC_DIV_POW_REG, madc_pow);/*reverting the old madc values*/
		mdelay(5);
		aic3262_write(codec, NDAC_DIV_POW_REG, ndac_pow);/*reverting the old ndac values*/
		mdelay(5);

		/*if (new_config == 0) {
			aic326x->current_config = 0;
			return 0;
		}
		aic326x->current_config =  -1;*/

		//aic3262_change_book(codec, 0);
		//aic3262_change_page(codec, 0);
#endif
	}

#ifdef MULTICONFIG_SUPPORT
	if (new_config < 0 )
		return 0; // No configs supported in this pfw
	if (new_config == aic326x->current_config)
		return 0;
	   if (pflows->configs[new_config].a_patch_size || pflows->configs[new_config].d_patch_size)
		minidsp_multiconfig(codec,
			pflows->configs[new_config].a_patch, pflows->configs[new_config].a_patch_size,
			pflows->configs[new_config].d_patch,  pflows->configs[new_config].d_patch_size);
#endif

	aic326x->current_config = new_config;
	aic3262_change_book( codec, 0);

	DBG(KERN_INFO "%s: switch mode finished\n", __func__);
	return 0;
}

/*
 * i2c_verify
 *
 * Function used to validate the contents written into the miniDSP
 * pages after miniDSP Configuration.
*/
int i2c_verify(struct snd_soc_codec *codec)
{

	DBG(KERN_INFO "#%s: Invoked.. Resetting to page 0\n", __func__);

	aic3262_change_book(codec, 0);
	DBG(KERN_INFO "#Reading reg_section_init_program\n");

	byte_i2c_array_read(codec, main44_REG_Section_init_program,
		ARRAY_SIZE(main44_REG_Section_init_program));

	DBG(KERN_INFO "#Reading minidsp_A_reg_values\n");
	byte_i2c_array_read(codec, main44_miniDSP_A_reg_values,
		(main44_miniDSP_A_reg_values_COEFF_SIZE +
		 main44_miniDSP_A_reg_values_INST_SIZE));

	DBG(KERN_INFO "#Reading minidsp_D_reg_values\n");
	byte_i2c_array_read(codec, main44_miniDSP_D_reg_values,
		(main44_miniDSP_D_reg_values_COEFF_SIZE +
		 main44_miniDSP_D_reg_values_INST_SIZE));

	DBG(KERN_INFO "#Reading reg_section_post_program\n");
	byte_i2c_array_read(codec, main44_REG_Section_post_program,
		ARRAY_SIZE(main44_REG_Section_post_program));

	aic3262_change_book(codec, 0);

	DBG(KERN_INFO "i2c_verify completed\n");
	return 0;
}


int change_codec_power_status(struct snd_soc_codec * codec, int off_restore, int power_mask)
{
	int minidsp_power_mask;
	u8 dac_status;
	u8 adc_status;

	minidsp_power_mask = 0;

	aic3262_change_page (codec, 0);
	aic3262_change_book (codec, 0);


	switch (off_restore) {

		case 0: /* Power-off the Codec */
			dac_status = snd_soc_read (codec, DAC_FLAG_R1);

			if(dac_status & 0x88) {
				minidsp_power_mask |= 0x1;
				snd_soc_update_bits(codec, PASI_DAC_DP_SETUP, 0xC0, 0x0);

				poll_dac(codec, 0x0, 0x0);
				poll_dac(codec, 0x1, 0x0);
			}

			adc_status = snd_soc_read (codec, ADC_FLAG_R1);

			if(adc_status & 0x44) {
				minidsp_power_mask |= 0x2;
				snd_soc_update_bits(codec, ADC_CHANNEL_POW, 0xC0, 0x0);

				poll_adc(codec, 0x0, 0x0);
				poll_adc(codec, 0x1, 0x0);
			}
		break;
		case 1: /* For Restoring Codec to Previous Power State */

			if(power_mask & 0x1) {

				snd_soc_update_bits(codec, PASI_DAC_DP_SETUP, 0xC0, 0xC0);

				poll_dac(codec, 0x0, 0x1);
				poll_dac(codec, 0x1, 0x1);
			}

			if(power_mask & 0x2) {

				snd_soc_update_bits(codec, ADC_CHANNEL_POW, 0xC0, 0xC0);

				poll_adc(codec, 0x0, 0x1);
				poll_adc(codec, 0x1, 0x1);
			}
		break;
		default:
			printk(KERN_ERR "#%s: Unknown Power State Requested..\n",
				__func__);
	}

	return minidsp_power_mask;

}

/*
 *----------------------------------------------------------------------------
 * Function  : boot_minidsp
 * Purpose  : for laoding the default minidsp mode for the first time .
 *----------------------------------------------------------------------------
 */
int
boot_minidsp(struct snd_soc_codec *codec, int new_mode)
{
	struct aic3262_priv *aic326x = snd_soc_codec_get_drvdata(codec);
	struct process_flow *  pflows = &miniDSP_programs[new_mode];
	int minidsp_stat;

	int (*ptransfer)(struct snd_soc_codec *codec,
				reg_value *program_ptr,
				int size);

	DBG("%s: switch  mode start\n", __func__);
	if (new_mode >= ARRAY_SIZE(miniDSP_programs))
		return 0; //  error condition
	if (new_mode == aic326x->process_flow)
		return 0;


#ifndef MULTIBYTE_I2C
	ptransfer = byte_i2c_array_transfer;
#else
	ptransfer = minidsp_i2c_multibyte_transfer;
#endif

	minidsp_stat = change_codec_power_status (codec, 0x0, 0x3);

	ptransfer(codec, pflows->miniDSP_init,		   pflows->init_size);
	ptransfer(codec, pflows->miniDSP_A_values,  pflows->A_size);
	ptransfer(codec, pflows->miniDSP_D_values,  pflows->D_size);
	ptransfer(codec, pflows->miniDSP_post,		 pflows->post_size);

	aic326x->process_flow = new_mode;

	change_codec_power_status(codec, 1, minidsp_stat);

	aic3262_change_page( codec,0);
	aic3262_change_book( codec,0);

	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic3262_minidsp_program
 * Purpose  : Program mini dsp for AIC3262 codec chip. This routine is
 *		called from the aic3262 codec driver, if mini dsp programming
 *		is enabled.
 *----------------------------------------------------------------------------
 */
int aic3262_minidsp_program(struct snd_soc_codec *codec)
{
	struct aic3262_priv *aic326x = snd_soc_codec_get_drvdata(codec);
	DBG(KERN_INFO "#AIC3262: programming mini dsp\n");

#if defined(PROGRAM_MINI_DSP_first)
	#ifdef DEBUG
	DBG("#Verifying book 0\n");
	i2c_verify_book0(codec);
#endif
	aic3262_change_book(codec, 0);
	boot_minidsp(codec, 1);
	aic326x->process_flow = 0;
	aic3262_change_book(codec, 0);
#ifdef DEBUG
	DBG("#verifying book 0\n");
	i2c_verify_book0(codec);
#endif
#endif
#if defined(PROGRAM_MINI_DSP_second)
#ifdef DEBUG
	DBG("#Verifying book 0\n");
	aic3262_change_book(codec, 0);
#endif
	boot_minidsp(codec, 0);
	aic326x->process_flow = 1;
#ifdef DEBUG
	DBG("#verifying book 0\n");
	aic3262_change_book(codec, 0);
#endif
#endif
	return 0;
}
/*
 *----------------------------------------------------------------------------
 * Function : m_control_info
 * Purpose  : This function is to initialize data for new control required to
 *			program the AIC3262 registers.
 *
 *----------------------------------------------------------------------------
 */
static int m_control_info(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_info *uinfo)
{
	uinfo->count = 1;
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : m_control_get
 * Purpose  : This function is to read data of new control for
 *			program the AIC3262 registers.
 *
 *----------------------------------------------------------------------------
 */
static int m_control_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct aic3262_priv *aic3262 = snd_soc_codec_get_drvdata(codec);
	u32 val;
	u8 val1;

	if (!strcmp(kcontrol->id.name, "Minidsp mode")) {
		val = aic3262->process_flow;
		ucontrol->value.integer.value[0] = val;
		DBG(KERN_INFO "control get : mode=%d\n", aic3262->process_flow);
	}
	if (!strcmp(kcontrol->id.name, "DAC Adaptive mode Enable")) {
		aic3262_change_book(codec, 80);
		val1 = i2c_smbus_read_byte_data(codec->control_data, 1);
		ucontrol->value.integer.value[0] = ((val1>>1)&0x01);
		DBG(KERN_INFO "control get : mode=%d\n", aic3262->process_flow);
		aic3262_change_book(codec,0);
	}
	if (!strcmp(kcontrol->id.name, "ADC Adaptive mode Enable")) {
		aic3262_change_book(codec, 40);
		val1 = i2c_smbus_read_byte_data(codec->control_data, 1);
		ucontrol->value.integer.value[0] = ((val1>>1)&0x01);
		DBG(KERN_INFO "control get : mode=%d\n", dmode);
		aic3262_change_book(codec,0);
	}

	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : m_new_control_put
 * Purpose  : new_control_put is called to pass data from user/application to
 *			the driver.
 *
 *----------------------------------------------------------------------------
 */
static int m_control_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct aic3262_priv *aic3262 = snd_soc_codec_get_drvdata(codec);

	u32 val;
	u8 val1;
	int mode = aic3262->process_flow;

	DBG("n_control_put\n");
	val = ucontrol->value.integer.value[0];
	if (!strcmp(kcontrol->id.name, "Minidsp mode")) {
		DBG(KERN_INFO "\nMini dsp put\n mode = %d, val=%d\n",
			aic3262->process_flow, val);
		if (val != mode) {
			if (aic3262->mute_codec == 1) {
				i2c_verify_book0(codec);
				aic3262_change_book(codec, 0);
				boot_minidsp(codec, val);

				aic3262_change_book(codec, 0);
				i2c_verify_book0(codec);
	/*			update_kcontrols(codec, val);*/
			} else {
				printk(KERN_ERR
			" Cant Switch Processflows, Playback in progress");
			}
		}
	}

	if (!strcmp(kcontrol->id.name, "DAC Adaptive mode Enable")) {
		DBG(KERN_INFO "\nMini dsp put\n mode = %d, val=%d\n",
			aic3262->process_flow, val);
		if (val != amode) {
			aic3262_change_book(codec, 80);
			val1 = i2c_smbus_read_byte_data(codec->control_data, 1);
			aic3262_write(codec, 1, (val1&0xfb)|(val<<1));
			aic3262_change_book(codec,0);
		}
		amode = val;
	}

	if (!strcmp(kcontrol->id.name, "ADC Adaptive mode Enable")) {
		DBG(KERN_INFO "\nMini dsp put\n mode = %d, val=%d\n",
			aic3262->process_flow, val);
		if (val != dmode) {
			aic3262_change_book(codec, 40);
			val1 = i2c_smbus_read_byte_data(codec->control_data, 1);
			aic3262_write(codec, 1, (val1&0xfb)|(val<<1));
			aic3262_change_book(codec,0);
		}
		dmode = val;
	}

	if (!strcmp(kcontrol->id.name, "Dump Regs Book0"))
		i2c_verify_book0(codec);

#if 0
	if (!strcmp(kcontrol->id.name, "Verify minidsp program")) {

		if (mode == 0) {
			DBG("Current mod=%d\nVerifying minidsp_D_regs", mode);
			byte_i2c_array_read(codec,  main44_miniDSP_D_reg_values,
				(main44_miniDSP_D_reg_values_COEFF_SIZE +
				main44_miniDSP_D_reg_values_INST_SIZE));
		} else {
			byte_i2c_array_read(codec,
				Second_Rate_miniDSP_A_reg_values,
				(Second_Rate_miniDSP_A_reg_values_COEFF_SIZE +
				Second_Rate_miniDSP_A_reg_values_INST_SIZE));
			byte_i2c_array_read(codec,
				Second_Rate_miniDSP_D_reg_values,
				(Second_Rate_miniDSP_D_reg_values_COEFF_SIZE +
				Second_Rate_miniDSP_D_reg_values_INST_SIZE));
		}
	}
#endif
	DBG("\nmode = %d\n", mode);
	return mode;
}

/************************** MUX CONTROL section *****************************/
/*
 *----------------------------------------------------------------------------
 * Function : __new_control_info_minidsp_mux
 * Purpose  : info routine for mini dsp mux control amixer kcontrols
 *----------------------------------------------------------------------------
 */
static int __new_control_info_minidsp_mux(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_info *uinfo)
{
	int index,index2;
	int ret_val = -1;


	for (index = 0; index < ARRAY_SIZE(main44_MUX_controls); index++) {
		if (strstr(kcontrol->id.name, main44_MUX_control_names[index]))
			break;
	}
	if (index < ARRAY_SIZE(main44_MUX_controls))
		{
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = MIN_MUX_CTRL;
		uinfo->value.integer.max = MAX_MUX_CTRL;
		ret_val = 0;
	}

	#if 1
	else{
		printk(" The second rate kcontrol id name is====== %s\n",kcontrol->id.name);


	for (index2 = 0; index < ARRAY_SIZE(base_speaker_SRS_MUX_controls); index2++) {
		if (strstr(kcontrol->id.name, base_speaker_SRS_MUX_control_names[index2]))
			break;
		}
		if (index < ARRAY_SIZE(base_speaker_SRS_MUX_controls))
		{
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = MIN_MUX_CTRL;
		uinfo->value.integer.max = MAX_MUX_CTRL;
		ret_val = 0;
		}
	}

	#endif

	return ret_val;
}

/*
 *----------------------------------------------------------------------------
 * Function : __new_control_get_minidsp_mux
 *
 * Purpose  : get routine for  mux control amixer kcontrols,
 *   read current register values to user.
 *   Used for for mini dsp 'MUX control' amixer controls.
 *----------------------------------------------------------------------------
 */
static int __new_control_get_minidsp_mux(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{

	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : __new_control_put_minidsp_mux
 *
 * Purpose  : put routine for amixer kcontrols, write user values to registers
 *			values. Used for for mini dsp 'MUX control' amixer controls.
 *----------------------------------------------------------------------------
 */
static int __new_control_put_minidsp_mux(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	u8 data[MUX_CTRL_REG_SIZE + 1];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int index = 1;
	int user_value = ucontrol->value.integer.value[0];
	struct i2c_client *i2c;
	u8 value[2], swap_reg_pre, swap_reg_post;
	u8 page;
	int ret_val = -1, array_size;
	control *array;
	char **array_names;
	char *control_name, *control_name1;
	struct aic3262_priv *aic326x = snd_soc_codec_get_drvdata(codec);
	i2c = codec->control_data;



	if (aic326x->process_flow == 0) {
		DBG("#the current process flow is %d", aic326x->process_flow);
		array = main44_MUX_controls;
		array_size = ARRAY_SIZE(main44_MUX_controls);
		array_names = main44_MUX_control_names;
		control_name = "Stereo_Mux_TwoToOne_1";
		control_name1 = "Mono_Mux_1_1";
		}

#if 0

		/* Configure only for process flow  1 controls */
		if (strcmp(kcontrol->id.name, control_name) &&
			strcmp(kcontrol->id.name, control_name1))
			return 0;
	} else {
		array = Second_Rate_MUX_controls;
		array_size = ARRAY_SIZE(Second_Rate_MUX_controls);
		array_names = Second_Rate_MUX_control_names;
		control_name = "Stereo_Mux_TwoToOne_1_Second";
		control_name1 = "Mono_Mux_1_Second";
		control_name2 = "Mono_Mux_4_Second";

		/* Configure only for process flow 2 controls */
		if (strcmp(kcontrol->id.name, control_name1) &&
			strcmp(kcontrol->id.name, control_name2))
			return 0;
	}

#endif

	page = array[index].control_page;

	DBG("#user value = 0x%x\n", user_value);
	for (index = 0; index < array_size; index++) {
		if (strstr(kcontrol->id.name, array_names[index]))
			break;
	}
	if (index < array_size) {
		DBG(KERN_INFO "#Index %d Changing to Page %d\n", index,
			array[index].control_page);

		aic3262_change_book(codec,
					array[index].control_book);
		aic3262_change_page(codec,
					array[index].control_page);

		if (!strcmp(array_names[index], control_name)) {
			if (user_value > 0) {
				data[1] = 0x00;
				data[2] = 0x00;
				data[3] = 0x00;
			} else {
				data[1] = 0xFF;
				data[2] = 0xFf;
				data[3] = 0xFF;
			}
		} else {
			if (user_value > 0) {
				data[1] =
					(u8) ((user_value >> 16) &
						  AIC3262_8BITS_MASK);
				data[2] =
					(u8) ((user_value >> 8) &
						  AIC3262_8BITS_MASK);
				data[3] =
					(u8)((user_value) & AIC3262_8BITS_MASK);
			}
		}
		/* start register address */
		data[0] = array[index].control_base;

		DBG(KERN_INFO
		"#Writing %d %d %d \r\n", data[0], data[1], data[2]);

		ret_val = i2c_master_send(i2c, data, MUX_CTRL_REG_SIZE + 1);

		if (ret_val != MUX_CTRL_REG_SIZE + 1)
			printk(KERN_ERR "i2c_master_send transfer failed\n");
		else {
			/* store the current level */
			kcontrol->private_value = user_value;
			ret_val = 0;
			/* Enable adaptive filtering for ADC/DAC */
		}

		/* Perform a BUFFER SWAP Command. Check if we are currently not
		 * in Page 8, if so, swap to Page 8 first
		 */

		value[0] = 1;

		if (i2c_master_send(i2c, value, 1) != 1)
			printk(KERN_ERR "Can not write register address\n");

		/* Read the Value of the Page 8 Register 1 which controls the
		   Adaptive Switching Mode */
		if (i2c_master_recv(i2c, value, 1) != 1)
			printk(KERN_ERR "Can not read codec registers\n");

		swap_reg_pre = value[0];
		/* Write the Register bit updates */
		value[1] = value[0] | 1;
		value[0] = 1;

		if (i2c_master_send(i2c, value, 2) != 2)
			printk(KERN_ERR "Can not write register address\n");

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
			DBG("Buffer swap success\n");
		else
			printk(KERN_ERR
			"Buffer swap...FAILED\nswap_reg_pre=%x, \
			swap_reg_post=%x\n", swap_reg_pre, swap_reg_post);

	}
	/* update the new buffer value in the old, just swapped out buffer */
	aic3262_change_book(codec, array[index].control_book);
	aic3262_change_page(codec, array[index].control_page);
	ret_val = i2c_master_send(i2c, data, MUX_CTRL_REG_SIZE + 1);
	ret_val = 0;

	aic3262_change_book(codec, 0);
	return ret_val;
}

/*
 *----------------------------------------------------------------------------
 * Function : minidsp_mux_ctrl_mixer_controls
 *
 * Purpose  : Add amixer kcontrols for mini dsp mux controls,
 *----------------------------------------------------------------------------
 */
static int minidsp_mux_ctrl_mixer_controls(struct snd_soc_codec *codec,
						int size, control *cntl,
						char **name)
{
	int i, err;
	int val1;

	printk("%d mixer controls for mini dsp MUX\n", size);
	if (size) {
		for (i = 0; i < size; i++) {

			snd_mux_controls[i].name = name[i];
			snd_mux_controls[i].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
			snd_mux_controls[i].access =
				SNDRV_CTL_ELEM_ACCESS_READWRITE;
			snd_mux_controls[i].info =
				__new_control_info_minidsp_mux;
			snd_mux_controls[i].get = __new_control_get_minidsp_mux;
			snd_mux_controls[i].put = __new_control_put_minidsp_mux;
			/*
			 *  TBD: read volume reg and update the index number
			 */
			aic3262_change_book(codec, cntl[i].control_book);
			aic3262_change_page(codec, cntl[i].control_page);
			val1 = i2c_smbus_read_byte_data(codec->control_data,
					cntl[i].control_base);
			DBG(KERN_INFO "Control data %x\n", val1);
			/*
			if( val1 >= 0 )
				snd_mux_controls[i].private_value = val1;
			else
				snd_mux_controls[i].private_value = 0;
			*/
			DBG(KERN_INFO
				"the value of amixer control mux=%d", val1);
			if (val1 >= 0 && val1 != 255)
				snd_mux_controls[i].private_value = val1;
			else
				snd_mux_controls[i].private_value = 0;

			snd_mux_controls[i].count = 0;

			err = snd_ctl_add(codec->card->snd_card,
				snd_ctl_new1(&snd_mux_controls[i],
						   codec));
			if (err < 0)
				printk(KERN_ERR
					"%s:Invalid control %s\n", __FILE__,
					snd_mux_controls[i].name);
		}
	}
	return 0;
}

/*-------------------------  Volume Controls  -----------------------*/
static int volume_lite_table[] = {

	0x00000D, 0x00000E, 0x00000E, 0x00000F,
	0x000010, 0x000011, 0x000012, 0x000013,
	0x000015, 0x000016, 0x000017, 0x000018,
	0x00001A, 0x00001C, 0x00001D, 0x00001F,
	0x000021, 0x000023, 0x000025, 0x000027,
	0x000029, 0x00002C, 0x00002F, 0x000031,
	0x000034, 0x000037, 0x00003B, 0x00003E,
	0x000042, 0x000046, 0x00004A, 0x00004F,
	0x000053, 0x000058, 0x00005D, 0x000063,
	0x000069, 0x00006F, 0x000076, 0x00007D,
	0x000084, 0x00008C, 0x000094, 0x00009D,
	0x0000A6, 0x0000B0, 0x0000BB, 0x0000C6,
	0x0000D2, 0x0000DE, 0x0000EB, 0x0000F9,
	0x000108, 0x000118, 0x000128, 0x00013A,
	0x00014D, 0x000160, 0x000175, 0x00018B,
	0x0001A3, 0x0001BC, 0x0001D6, 0x0001F2,
	0x000210, 0x00022F, 0x000250, 0x000273,
	0x000298, 0x0002C0, 0x0002E9, 0x000316,
	0x000344, 0x000376, 0x0003AA, 0x0003E2,
	0x00041D, 0x00045B, 0x00049E, 0x0004E4,
	0x00052E, 0x00057C, 0x0005D0, 0x000628,
	0x000685, 0x0006E8, 0x000751, 0x0007C0,
	0x000836, 0x0008B2, 0x000936, 0x0009C2,
	0x000A56, 0x000AF3, 0x000B99, 0x000C49,
	0x000D03, 0x000DC9, 0x000E9A, 0x000F77,
	0x001062, 0x00115A, 0x001262, 0x001378,
	0x0014A0, 0x0015D9, 0x001724, 0x001883,
	0x0019F7, 0x001B81, 0x001D22, 0x001EDC,
	0x0020B0, 0x0022A0, 0x0024AD, 0x0026DA,
	0x002927, 0x002B97, 0x002E2D, 0x0030E9,
	0x0033CF, 0x0036E1, 0x003A21, 0x003D93,
	0x004139, 0x004517, 0x00492F, 0x004D85,
	0x00521D, 0x0056FA, 0x005C22, 0x006197,
	0x006760, 0x006D80, 0x0073FD, 0x007ADC,
	0x008224, 0x0089DA, 0x009205, 0x009AAC,
	0x00A3D7, 0x00B7D4, 0x00AD8C, 0x00C2B9,
	0x00CE43, 0x00DA7B, 0x00E76E, 0x00F524,
	0x0103AB, 0x01130E, 0x01235A, 0x01349D,
	0x0146E7, 0x015A46, 0x016ECA, 0x018486,
	0x019B8C, 0x01B3EE, 0x01CDC3, 0x01E920,
	0x02061B, 0x0224CE, 0x024553, 0x0267C5,
	0x028C42, 0x02B2E8, 0x02DBD8, 0x030736,
	0x033525, 0x0365CD, 0x039957, 0x03CFEE,
	0x0409C2, 0x044703, 0x0487E5, 0x04CCA0,
	0x05156D, 0x05628A, 0x05B439, 0x060ABF,
	0x066666, 0x06C77B, 0x072E50, 0x079B3D,
	0x080E9F, 0x0888D7, 0x090A4D, 0x09936E,
	0x0A24B0, 0x0ABE8D, 0x0B6188, 0x0C0E2B,
	0x0CC509, 0x0D86BD, 0x0E53EB, 0x0F2D42,
	0x101379, 0x110754, 0x1209A3, 0x131B40,
	0x143D13, 0x157012, 0x16B543, 0x180DB8,
	0x197A96, 0x1AFD13, 0x1C9676, 0x1E481C,
	0x201373, 0x21FA02, 0x23FD66, 0x261F54,
	0x28619A, 0x2AC625, 0x2D4EFB, 0x2FFE44,
	0x32D646, 0x35D96B, 0x390A41, 0x3C6B7E,
	0x400000, 0x43CAD0, 0x47CF26, 0x4C106B,
	0x50923B, 0x55586A, 0x5A6703, 0x5FC253,
	0x656EE3, 0x6B7186, 0x71CF54, 0x788DB4,
	0x7FB260,
};

static struct snd_kcontrol_new snd_vol_controls[MAX_VOLUME_CONTROLS];
/*
 *----------------------------------------------------------------------------
 * Function : __new_control_info_main44_minidsp_volume
 * Purpose  : info routine for volumeLite amixer kcontrols
 *----------------------------------------------------------------------------
 */

static int
__new_control_info_minidsp_volume(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_info *uinfo)
{
	int index, index8;
	int ret_val = -1;

	for (index = 0; index < ARRAY_SIZE(main44_VOLUME_controls); index++) {
		if (strstr
			(kcontrol->id.name, main44_VOLUME_control_names[index]))
			break;
	}

	for (index8 = 0; index8 < ARRAY_SIZE(base_speaker_SRS_VOLUME_controls);
			index8++) {
		if (strstr
			(kcontrol->id.name,
			base_speaker_SRS_VOLUME_control_names[index]))
			break;
	}

	if ((index < ARRAY_SIZE(main44_VOLUME_controls))

		|| (index8 < ARRAY_SIZE(base_speaker_SRS_VOLUME_controls))) {
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = MIN_VOLUME;
		uinfo->value.integer.max = MAX_VOLUME;
		ret_val = 0;
	}
	return ret_val;
}

/*
 *----------------------------------------------------------------------------
 * Function : __new_control_get_main44_minidsp_vol
 * Purpose  : get routine for amixer kcontrols, read current register
 *		 values. Used for for mini dsp 'VolumeLite' amixer controls.
 *----------------------------------------------------------------------------
 */
static int
__new_control_get_minidsp_volume(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : __new_control_put_main44_minidsp_volume
 * Purpose  : put routine for amixer kcontrols, write user values to registers
 *		values. Used for for mini dsp 'VolumeLite' amixer controls.
 *----------------------------------------------------------------------------
 */
static int
__new_control_put_minidsp_volume(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	u8 data[4];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int user_value = ucontrol->value.integer.value[0];
	struct i2c_client *i2c = codec->control_data;
	int ret_val = -1;
	int coeff;
	u8 value[2], swap_reg_pre, swap_reg_post;
	struct aic3262_priv *aic3262 = snd_soc_codec_get_drvdata(codec);

	control *volume_controls = NULL;
	printk(KERN_INFO "user value = 0x%x\n", user_value);

	if (aic3262->process_flow == 0)
		volume_controls = main44_VOLUME_controls;

	else
		volume_controls = base_speaker_SRS_VOLUME_controls;


	aic3262_change_book(codec, volume_controls->control_book);
	aic3262_change_page(codec, volume_controls->control_page);

	coeff = volume_lite_table[user_value << 1];

	data[1] = (u8) ((coeff >> 16) & AIC3262_8BITS_MASK);
	data[2] = (u8) ((coeff >> 8) & AIC3262_8BITS_MASK);
	data[3] = (u8) ((coeff) & AIC3262_8BITS_MASK);

	/* Start register address */
	data[0] = volume_controls->control_base;
	ret_val = i2c_master_send(i2c, data, VOLUME_REG_SIZE + 1);
	if (ret_val != VOLUME_REG_SIZE + 1)
		printk(KERN_ERR "i2c_master_send transfer failed\n");
	else {
		/* store the current level */
		kcontrol->private_value = user_value;
		ret_val = 0;
	}
	/* Initiate buffer swap */
	value[0] = 1;

	if (i2c_master_send(i2c, value, 1) != 1)
		printk(KERN_ERR "Can not write register address\n");

	/* Read the Value of the Page 8 Register 1 which controls the
	   Adaptive Switching Mode */
	if (i2c_master_recv(i2c, value, 1) != 1)
		printk(KERN_ERR "Can not read codec registers\n");

	swap_reg_pre = value[0];
	/* Write the Register bit updates */
	value[1] = value[0] | 1;
	value[0] = 1;
	if (i2c_master_send(i2c, value, 2) != 2)
		printk(KERN_ERR "Can not write register address\n");

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
		DBG("Buffer swap success\n");
	else
		DBG("Buffer swap...FAILED\nswap_reg_pre=%x, swap_reg_post=%x\n",
			 swap_reg_pre, swap_reg_post);

	/* update the new buffer value in the old, just swapped out buffer */
	aic3262_change_book(codec, volume_controls->control_book);
	aic3262_change_page(codec, volume_controls->control_page);
	i2c_master_send(i2c, data, MUX_CTRL_REG_SIZE + 1);

	aic3262_change_book(codec, 0);

	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : minidsp_volume_main44_mixer_controls
 * Purpose  : Add amixer kcontrols for mini dsp volume Lite controls,
 *----------------------------------------------------------------------------
 */
static int minidsp_volume_mixer_controls(struct snd_soc_codec *codec)
{
	int i, err, no_volume_controls;
	static char volume_control_name[MAX_VOLUME_CONTROLS][40];

	/*	  ADD first process volume controls	   */
	no_volume_controls = ARRAY_SIZE(main44_VOLUME_controls);

	printk(KERN_INFO " %d mixer controls for mini dsp 'volumeLite'\n",
		no_volume_controls);

	if (no_volume_controls) {

		for (i = 0; i < no_volume_controls; i++) {
			strcpy(volume_control_name[i],
				   main44_VOLUME_control_names[i]);
			strcat(volume_control_name[i], VOLUME_KCONTROL_NAME);

			printk(KERN_ERR "Volume controls: %s\n",
				volume_control_name[i]);

			snd_vol_controls[i].name = volume_control_name[i];
			snd_vol_controls[i].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
			snd_vol_controls[i].access =
				SNDRV_CTL_ELEM_ACCESS_READWRITE;
			snd_vol_controls[i].info =
				__new_control_info_minidsp_volume;
			snd_vol_controls[i].get =
				__new_control_get_minidsp_volume;
			snd_vol_controls[i].put =
				__new_control_put_minidsp_volume;
			/*
			 *	  TBD: read volume reg and update the index number
			 */
			snd_vol_controls[i].private_value = 0;
			snd_vol_controls[i].count = 0;

			err = snd_ctl_add(codec->card->snd_card,
					  snd_ctl_new1(&snd_vol_controls[i],
							   codec));
			if (err < 0) {
				printk(KERN_ERR
					"%s:Invalid control %s\n", __FILE__,
					snd_vol_controls[i].name);
			}
		}
	}


	/*	  ADD second process volume controls	  */
	no_volume_controls = ARRAY_SIZE(base_speaker_SRS_VOLUME_controls);

	printk(KERN_ERR " %d mixer controls for mini dsp 'volumeLite'\n",
		no_volume_controls);

	if (no_volume_controls) {

		for (i = 0; i < no_volume_controls; i++) {
			strcpy(volume_control_name[i],
				   base_speaker_SRS_VOLUME_control_names[i]);
			strcat(volume_control_name[i], VOLUME_KCONTROL_NAME);

			printk(KERN_ERR "Volume controls: %s\n",
				volume_control_name[i]);

			snd_vol_controls[i].name = volume_control_name[i];
			snd_vol_controls[i].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
			snd_vol_controls[i].access =
				SNDRV_CTL_ELEM_ACCESS_READWRITE;
			snd_vol_controls[i].info =
				__new_control_info_minidsp_volume;
			snd_vol_controls[i].get =
				__new_control_get_minidsp_volume;
			snd_vol_controls[i].put =
				__new_control_put_minidsp_volume;
			/*
			 *	  TBD: read volume reg and update the index number
			 */
			snd_vol_controls[i].private_value = 0;
			snd_vol_controls[i].count = 0;

			err = snd_ctl_add(codec->card->snd_card,
					  snd_ctl_new1(&snd_vol_controls[i],
							   codec));
			if (err < 0) {
				printk(KERN_ERR
					"%s:Invalid control %s\n", __FILE__,
					   snd_vol_controls[i].name);
			}
		}
	}

	return 0;
}

/*
 *--------------------------------------------------------------------------
 * Function : aic3262_add_minidsp_controls
 * Purpose :  Configures the AMIXER Control Interfaces that can be exercised by
 *			the user at run-time. Utilizes the  the snd_adaptive_controls[]
 *			array to specify two run-time controls.
 *---------------------------------------------------------------------------
 */
int aic3262_add_minidsp_controls(struct snd_soc_codec *codec)
{
#ifdef ADD_MINI_DSP_CONTROLS
	int i, err, no_mux_controls,no_mux_controls1;
	/* add mode k control */
	for (i = 0; i < ARRAY_SIZE(aic3262_minidsp_controls); i++) {
		err = snd_ctl_add(codec->card->snd_card,
		snd_ctl_new1(&aic3262_minidsp_controls[i], codec));
		if (err < 0) {
			printk(KERN_ERR "Invalid control\n");
			return err;
		}
	}


	/* add mux controls */
	no_mux_controls = ARRAY_SIZE(main44_MUX_controls);
	minidsp_mux_ctrl_mixer_controls(codec, no_mux_controls,
		main44_MUX_controls, main44_MUX_control_names);


	no_mux_controls1 = ARRAY_SIZE(base_speaker_SRS_MUX_controls);
	minidsp_mux_ctrl_mixer_controls(codec, no_mux_controls1,
		base_speaker_SRS_MUX_controls, base_speaker_SRS_MUX_control_names);


	/* add volume controls*/
	minidsp_volume_mixer_controls(codec);
#endif /* ADD_MINI_DSP_CONTROLS */
	return 0;
}

MODULE_DESCRIPTION("ASoC TLV320AIC3262 miniDSP driver");
MODULE_AUTHOR("Y Preetam Sashank Reddy <preetam@mistralsolutions.com>");
MODULE_LICENSE("GPL");
#endif /* End of CONFIG_MINI_DSP */
