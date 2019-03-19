/*
 * Copyright 2014-2017 NXP Semiconductors
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "dbgprint.h"
#include "tfa_container.h"
#include "tfa.h"
#include "tfa98xx_tfafieldnames.h"
#include "tfa_internal.h"

/* handle macro for bitfield */
#define TFA_MK_BF(reg, pos, len) ((reg<<8)|(pos<<4)|(len-1))

/* abstract family for register */
#define FAM_TFA98XX_CF_CONTROLS (TFA_FAM(tfa, RST) >> 8)
#define FAM_TFA98XX_CF_MEM      (TFA_FAM(tfa, MEMA)>> 8)
#define FAM_TFA98XX_MTP0        (TFA_FAM(tfa, MTPOTC) >> 8)
#define FAM_TFA98xx_INT_EN      (TFA_FAM(tfa, INTENVDDS) >> 8)

#define CF_STATUS_I2C_CMD_ACK 0x01

/* Defines below are used for irq function (this removed the genregs include) */
#define TFA98XX_INTERRUPT_ENABLE_REG1		0x48
#define TFA98XX_INTERRUPT_IN_REG1		0x44
#define TFA98XX_INTERRUPT_OUT_REG1		0x40
#define TFA98XX_STATUS_POLARITY_REG1		0x4c
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_MSK	0x2
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_MSK	0x1
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_POS	1
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_POS	0

void tfanone_ops(struct tfa_device_ops *ops);
void tfa9872_ops(struct tfa_device_ops *ops);
void tfa9874_ops(struct tfa_device_ops *ops);
void tfa9912_ops(struct tfa_device_ops *ops);
void tfa9888_ops(struct tfa_device_ops *ops);
void tfa9891_ops(struct tfa_device_ops *ops);
void tfa9897_ops(struct tfa_device_ops *ops);
void tfa9896_ops(struct tfa_device_ops *ops);
void tfa9890_ops(struct tfa_device_ops *ops);
void tfa9895_ops(struct tfa_device_ops *ops);
void tfa9894_ops(struct tfa_device_ops *ops);

#ifndef MIN
#define MIN(A, B) (A<B?A:B)
#endif

/* retry values */
#define CFSTABLE_TRIES		10
#define AMPOFFWAIT_TRIES	50
#define MTPBWAIT_TRIES		50
#define MTPEX_WAIT_NTRIES	50

/* calibration done executed */
#define TFA_MTPEX_POS           TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_POS /**/

int tfa_get_calibration_info(struct tfa_device *tfa, int channel)
{
	return tfa->mohm[channel];
}

/* return sign extended tap pattern */
int tfa_get_tap_pattern(struct tfa_device *tfa)
{
	int value = tfa_get_bf(tfa, TFA9912_BF_CFTAPPAT );
	int bitshift;
	uint8_t field_len = 1 + (TFA9912_BF_CFTAPPAT & 0x0f); /* length of bitfield */

	bitshift = 8*sizeof(int)-field_len;
	/* signextend */
	value = (value << bitshift) >> bitshift;

	return value;
}
/*
 * interrupt bit function to clear
 */
int tfa_irq_clear(struct tfa_device *tfa, enum tfa9912_irq bit)
{
	unsigned char reg;

	/* make bitfield enum */
	if ( bit == tfa9912_irq_all) {
		/* operate on all bits */
		for(reg=TFA98XX_INTERRUPT_IN_REG1; reg<TFA98XX_INTERRUPT_IN_REG1+3; reg++)
			reg_write(tfa, reg, 0xffff); /* all bits */
	} else if (bit < tfa9912_irq_max) {
		reg = (unsigned char)(TFA98XX_INTERRUPT_IN_REG1 + (bit>>4));
		reg_write(tfa, reg,  1<<(bit & 0x0f)); /* only this bit */
	} else
		return -1;

	return 0;
}
/*
 * return state of irq or -1 if illegal bit
 */
int tfa_irq_get(struct tfa_device *tfa, enum tfa9912_irq bit)
{
	uint16_t value;
	int reg, mask;

	if (bit < tfa9912_irq_max) {
		/* only this bit */
		reg = TFA98XX_INTERRUPT_OUT_REG1 + (bit>>4);
		mask = 1<<(bit & 0x0f);
		reg_read(tfa, (unsigned char)reg, &value);
	} else
		return -1;

	return (value & mask) !=0 ;
}
/*
 * interrupt bit function that operates on the shadow regs in the handle
 */

int tfa_irq_ena(struct tfa_device *tfa, enum tfa9912_irq bit, int state)
{
	uint16_t value, new_value;
	int reg=0, mask;
	/* */
	if ( bit == tfa9912_irq_all) {
		/* operate on all bits */
		for(reg=TFA98XX_INTERRUPT_ENABLE_REG1; reg<=TFA98XX_INTERRUPT_ENABLE_REG1+tfa9912_irq_max/16; reg++) {
			reg_write(tfa, (unsigned char)reg, state ? 0xffff : 0); /* all bits */
			tfa->interrupt_enable[reg-TFA98XX_INTERRUPT_ENABLE_REG1] = state ? 0xffff : 0; /* all bits */
		}
	} else if (bit < tfa9912_irq_max) {
		 /* only this bit */
			reg = TFA98XX_INTERRUPT_ENABLE_REG1 + (bit>>4);
			mask = 1<<(bit & 0x0f);
			reg_read(tfa, (unsigned char)reg, &value);
			if (state)
				new_value = (uint16_t)(value | mask);
			else
				new_value = value & ~mask;
			if ( new_value != value) {
				reg_write(tfa, (unsigned char)reg,  new_value); /* only this bit */
				tfa->interrupt_enable[reg-TFA98XX_INTERRUPT_ENABLE_REG1] = new_value;
			}
	} else
		return -1;

	return 0;
}

/*
 * mask interrupts by disabling them
 */
int tfa_irq_mask(struct tfa_device *tfa)
{
	int reg;

	/* operate on all bits */
	for (reg=TFA98XX_INTERRUPT_ENABLE_REG1; reg<=TFA98XX_INTERRUPT_ENABLE_REG1+tfa9912_irq_max/16; reg++)
		reg_write(tfa, (unsigned char)reg, 0);

	return 0;
}

/*
 * unmask interrupts by enabling them again
 */
int tfa_irq_unmask(struct tfa_device *tfa)
{
	int reg;

	/* operate on all bits */
	for (reg=TFA98XX_INTERRUPT_ENABLE_REG1; reg<=TFA98XX_INTERRUPT_ENABLE_REG1+tfa9912_irq_max/16; reg++)
		reg_write(tfa, (unsigned char)reg, tfa->interrupt_enable[reg-TFA98XX_INTERRUPT_ENABLE_REG1]);

	return 0;
}

/*
 * interrupt bit function that sets the polarity
 */

int tfa_irq_set_pol(struct tfa_device *tfa, enum tfa9912_irq bit, int state)
{
	uint16_t value, new_value;
	int reg=0, mask;

	if (bit == tfa9912_irq_all) {
		/* operate on all bits */
		for(reg=TFA98XX_STATUS_POLARITY_REG1; reg<=TFA98XX_STATUS_POLARITY_REG1+tfa9912_irq_max/16; reg++) {
			reg_write(tfa, (unsigned char)reg, state ? 0xffff : 0); /* all bits */
		}
	} else if (bit < tfa9912_irq_max) {
		 /* only this bit */
		reg = TFA98XX_STATUS_POLARITY_REG1 + (bit>>4);
		mask = 1<<(bit & 0x0f);
		reg_read(tfa, (unsigned char)reg, &value);
		if (state) /* Active High */
			new_value = (uint16_t)(value | mask);
		else       /* Active Low */
			new_value = value & ~mask;
		if ( new_value != value) {
			reg_write(tfa, (unsigned char)reg,  new_value); /* only this bit */
		}
	} else
		return -1;

	return 0;
}

/*
 *  set device info and register device ops
 */
void tfa_set_query_info(struct tfa_device *tfa)
{
	/* invalidate device struct cached values */
	tfa->hw_feature_bits = -1;
	tfa->sw_feature_bits[0] = -1;
	tfa->sw_feature_bits[1] = -1;
	tfa->profile = -1;
	tfa->vstep = -1;
	/* defaults */
	tfa->is_probus_device = 0;
	tfa->tfa_family = 1;
	tfa->daimap = Tfa98xx_DAI_I2S;		/* all others */
	tfa->spkr_count = 1;
	tfa->spkr_select = 0;
	tfa->support_tcoef = supportYes;
	tfa->supportDrc = supportNotSet;
	tfa->support_saam = supportNotSet;
	tfa->ext_dsp = -1;	/* respond to external DSP: -1:none, 0:no_dsp, 1:cold, 2:warm */
	tfa->bus=0;
	tfa->partial_enable = 0;
	tfa->convert_dsp32 = 0;
	tfa->sync_iv_delay = 0;

	/* TODO use the getfeatures() for retrieving the features [artf103523]
	tfa->supportDrc = supportNotSet;*/

	switch (tfa->rev & 0xff) {
	case 0: /* tfanone : non-i2c external DSP device */
		/* e.g. qc adsp */
		tfa->supportDrc = supportYes;
		tfa->tfa_family = 0;
		tfa->spkr_count = 0;
		tfa->daimap = 0;
		tfanone_ops(&tfa->dev_ops); /* register device operations via tfa hal*/
		tfa->bus=1;
		break;
	case 0x72:
		/* tfa9872 */
		tfa->supportDrc = supportYes;
		tfa->tfa_family = 2;
		tfa->spkr_count = 1;
		tfa->is_probus_device = 1;
		tfa->daimap = Tfa98xx_DAI_TDM;
		tfa9872_ops(&tfa->dev_ops); /* register device operations */
		break;
	case 0x74:
		/* tfa9874 */
		tfa->supportDrc = supportYes;
		tfa->tfa_family = 2;
		tfa->spkr_count = 1;
		tfa->is_probus_device = 1;
		tfa->daimap = Tfa98xx_DAI_TDM;
		tfa9874_ops(&tfa->dev_ops); /* register device operations */
		break;
	case 0x88:
		/* tfa9888 */
		tfa->tfa_family = 2;
		tfa->spkr_count = 2;
		tfa->daimap = Tfa98xx_DAI_TDM;
		tfa9888_ops(&tfa->dev_ops); /* register device operations */
		break;
	case 0x97:
		/* tfa9897 */
		tfa->supportDrc = supportNo;
		tfa->spkr_count = 1;
		tfa->daimap = Tfa98xx_DAI_TDM;
		tfa9897_ops(&tfa->dev_ops); /* register device operations */
		break;
	case 0x96:
		/* tfa9896 */
		tfa->supportDrc = supportNo;
		tfa->spkr_count = 1;
		tfa->daimap = Tfa98xx_DAI_TDM;
		tfa9896_ops(&tfa->dev_ops); /* register device operations */
		break;
	case 0x92:
		/* tfa9891 */
		tfa->spkr_count = 1;
		tfa->daimap = ( Tfa98xx_DAI_PDM | Tfa98xx_DAI_I2S );
		tfa9891_ops(&tfa->dev_ops); /* register device operations */
		break;
	case 0x91:
		/* tfa9890B */
		tfa->spkr_count = 1;
		tfa->daimap = ( Tfa98xx_DAI_PDM | Tfa98xx_DAI_I2S );
		break;
	case 0x80:
	case 0x81:
		/* tfa9890 */
		tfa->spkr_count = 1;
		tfa->daimap = Tfa98xx_DAI_I2S;
		tfa->supportDrc = supportNo;
		tfa->supportFramework = supportNo;
		tfa9890_ops(&tfa->dev_ops); /* register device operations */
		break;
	case 0x12:
		/* tfa9895 */
		tfa->spkr_count = 1;
		tfa->daimap = Tfa98xx_DAI_I2S;
		tfa9895_ops(&tfa->dev_ops); /* register device operations */
		break;
	case 0x13:
		/* tfa9912 */
		tfa->tfa_family = 2;
		tfa->spkr_count = 1;
		tfa->daimap = Tfa98xx_DAI_TDM;
		tfa9912_ops(&tfa->dev_ops); /* register device operations */
		break;
	case 0x94:
		/* tfa9894 */
		tfa->tfa_family = 2;
		tfa->spkr_count = 1;
		tfa->daimap = Tfa98xx_DAI_TDM;
		tfa9894_ops(&tfa->dev_ops); /* register device operations */
		break;

	default:
		pr_err("unknown device type : 0x%02x\n", tfa->rev);
		_ASSERT(0);
		break;
	}
}

/*
 * lookup the device type and return the family type
 */
int tfa98xx_dev2family(int dev_type)
{
	/* only look at the die ID part (lsb byte) */
	switch(dev_type & 0xff) {
	case 0x12:
	case 0x80:
	case 0x81:
	case 0x91:
	case 0x92:
	case 0x97:
	case 0x96:
		return 1;
	case 0x88:
	case 0x72:
	case 0x13:
	case 0x74:
	case 0x94:
		return 2;
	case 0x50:
		return 3;
	default:
		return 0;
	}
}

/*
 * 	return the target address for the filter on this device

  filter_index:
	[0..9] reserved for EQ (not deployed, calc. is available)
	[10..12] anti-alias filter
	[13]  integrator filter

 */
enum Tfa98xx_DMEM tfa98xx_filter_mem(struct tfa_device *tfa, int filter_index, unsigned short *address, int channel)
{
	enum Tfa98xx_DMEM dmem=-1;
	int idx;
	unsigned short bq_table[7][4] ={
	/* index: 10, 11, 12, 13 */
			{346, 351, 356, 288},
			{346, 351, 356, 288},
			{467, 472, 477, 409},
			{406, 411, 416, 348},
			{467, 472, 477, 409},
			{8832, 8837, 8842, 8847},
			{8853, 8858, 8863, 8868}
			/* Since the 88 is stereo we have 2 parts.
			 * Every index has 5 values except index 13 this one has 6 values
			 */
	};

	if ( (10 <= filter_index) && (filter_index <= 13) ) {
		dmem = Tfa98xx_DMEM_YMEM; /* for all devices */
		idx = filter_index-10;

		switch (tfa->rev & 0xff ) {
		case 0x12:
			*address = bq_table[2][idx];
			break;
		case 0x97:
			*address = bq_table[3][idx];
			break;
		case 0x96:
			*address = bq_table[3][idx];
			break;
		case 0x80:
		case 0x81:
		case 0x91:
			*address = bq_table[1][idx];
			break;
		case 0x92:
			*address = bq_table[4][idx];
			break;
		case 0x88:
			/* Channel 1 = primary, 2 = secondary */
			if(channel == 1)
				*address = bq_table[5][idx];
			else
				*address = bq_table[6][idx];
			break;
		case 0x72:
		case 0x74:
		case 0x13:
		default:
			/* unsupported case, possibly intermediate version */
			return -1;
			_ASSERT(0);
		}
	}
	return dmem;
}

/************************ query functions ********************************************************/
/**
* return revision
* Used by the LTT
*/
void tfa98xx_rev(int *major, int *minor, int *revision)
{
	char version_str[] = TFA98XX_API_REV_STR;
	sscanf(version_str, "v%d.%d.%d", major, minor, revision);
}

/**
 * tfa_supported_speakers
 *  returns the number of the supported speaker count
 */
enum Tfa98xx_Error tfa_supported_speakers(struct tfa_device *tfa, int* spkr_count)
{
	if (tfa->in_use == 0)
		return Tfa98xx_Error_NotOpen;
	else
		*spkr_count = tfa->spkr_count;

	return Tfa98xx_Error_Ok;
}

/*
 * tfa98xx_supported_saam
 *  returns the supportedspeaker as microphone feature
 */
enum Tfa98xx_Error tfa98xx_supported_saam(struct tfa_device *tfa, enum Tfa98xx_saam *saam)
{
	int features;
	enum Tfa98xx_Error error;

	if (tfa->support_saam == supportNotSet) {
		error = tfa98xx_dsp_get_hw_feature_bits(tfa, &features);
		if (error!=Tfa98xx_Error_Ok)
			return error;
		tfa->support_saam =
				(features & 0x8000)? supportYes : supportNo; /* SAAM is bit15 */
	}
	*saam = tfa->support_saam == supportYes ? Tfa98xx_saam : Tfa98xx_saam_none ;

	return Tfa98xx_Error_Ok;
}

/*
 * tfa98xx_compare_features
 *  Obtains features_from_MTP and features_from_cnt
 */
enum Tfa98xx_Error tfa98xx_compare_features(struct tfa_device *tfa, int features_from_MTP[3], int features_from_cnt[3])
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	uint32_t value;
	uint16_t mtpbf;
	unsigned char bytes[3 * 2];
	int status;

	tfa98xx_dsp_system_stable(tfa, &status);
	if (!status)
		return Tfa98xx_Error_NoClock;

	/* Set proper MTP location per device: */
	if (tfa->tfa_family == 1) {
		mtpbf=0x850f;  /* MTP5 for tfa1,16 bits */
	} else {
		mtpbf=0xf907;  /* MTP9 for tfa2, 8 bits */
	}

	/* Read HW features from MTP: */
	value = tfa_read_reg(tfa, mtpbf) & 0xffff;
	features_from_MTP[0] = tfa->hw_feature_bits = value;

    /* Read SW features: */
    error = tfa_dsp_cmd_id_write_read(tfa, MODULE_FRAMEWORK, FW_PAR_ID_GET_FEATURE_INFO, sizeof(bytes), bytes);
	if (error != Tfa98xx_Error_Ok)
	        return error; /* old ROM code may respond with Tfa98xx_Error_RpcParamId */

	tfa98xx_convert_bytes2data(sizeof(bytes), bytes, &features_from_MTP[1]);

	/* check if feature bits from MTP match feature bits from cnt file: */
	get_hw_features_from_cnt(tfa, &features_from_cnt[0]);
	get_sw_features_from_cnt(tfa, &features_from_cnt[1]);

	return error;
}

/********************************* device specific ops ************************************************/
/* the wrapper for DspReset, in case of full */
enum Tfa98xx_Error tfa98xx_dsp_reset(struct tfa_device *tfa, int state)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	error = (tfa->dev_ops.dsp_reset)(tfa, state);

	return error;
}

/* the ops wrapper for tfa98xx_dsp_SystemStable */
enum Tfa98xx_Error tfa98xx_dsp_system_stable(struct tfa_device *tfa, int *ready)
{
	return (tfa->dev_ops.dsp_system_stable)(tfa, ready);
}

/* the ops wrapper for tfa98xx_dsp_system_stable */
enum Tfa98xx_Error tfa98xx_auto_copy_mtp_to_iic(struct tfa_device *tfa)
{
	return (tfa->dev_ops.auto_copy_mtp_to_iic)(tfa);
}

/* the ops wrapper for tfa98xx_faim_protect */
enum Tfa98xx_Error tfa98xx_faim_protect(struct tfa_device *tfa, int state)
{
	return (tfa->dev_ops.faim_protect)(tfa, state);
}

/*
 * bring the device into a state similar to reset
 */
enum Tfa98xx_Error tfa98xx_init(struct tfa_device *tfa)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	uint16_t value=0;

	/* reset all i2C registers to default
	 *  Write the register directly to avoid the read in the bitfield function.
	 *  The I2CR bit may overwrite the full register because it is reset anyway.
	 *  This will save a reg read transaction.
	 */
	TFA_SET_BF_VALUE(tfa, I2CR, 1, &value );
	TFA_WRITE_REG(tfa, I2CR, value);

	/* Put DSP in reset */
	tfa98xx_dsp_reset(tfa, 1); /* in pair of tfaRunStartDSP() */

	/* some other registers must be set for optimal amplifier behaviour
	 * This is implemented in a file specific for the type number
	 */
	if (tfa->dev_ops.tfa_init)
		error = (tfa->dev_ops.tfa_init)(tfa);

	return error;
}

enum Tfa98xx_Error tfa98xx_dsp_write_tables(struct tfa_device *tfa, int sample_rate)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	error = (tfa->dev_ops.dsp_write_tables)(tfa, sample_rate);

	return error;
}

/** Set internal oscillator into power down mode.
*
*  @param[in] tfa device description structure
*  @param[in] state new state 0 - oscillator is on, 1 oscillator is off.
*
*  @return Tfa98xx_Error_Ok when successfull, error otherwise.
*/
enum Tfa98xx_Error tfa98xx_set_osc_powerdown(struct tfa_device *tfa, int state)
{
	if (tfa->dev_ops.set_osc_powerdown) {
		return tfa->dev_ops.set_osc_powerdown(tfa, state);
	}

	return Tfa98xx_Error_Not_Implemented;
}

/** Check presence of powerswitch=1 in configuration and optimal setting.
*
*  @param[in] tfa device description structure
*
*  @return -1 when error, 0 or 1 depends on switch settings.
*/
int tfa98xx_powerswitch_is_enabled(struct tfa_device *tfa)
{
	uint16_t value;
	enum Tfa98xx_Error ret;

	if (((tfa->rev & 0xff) == 0x13) || ((tfa->rev & 0xff) == 0x88)) {
		ret = reg_read(tfa, 0xc6, &value);
		if (ret != Tfa98xx_Error_Ok) {
			return -1;
		}
		/* PLMA5539: Check actual value of powerswitch. TODO: regmap v1.40 should make this bit public. */

		return (int)(value & (1u << 6));
	}

	return 1;
}

/********************* new tfa2 *********************************************************************/
/* newly added messaging for tfa2 tfa1? */
enum Tfa98xx_Error tfa98xx_dsp_get_memory(struct tfa_device *tfa, int memoryType,
		int offset, int length, unsigned char bytes[])
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	char msg[4*3];
	int nr = 0;

	msg[nr++] = 8;
	msg[nr++] = MODULE_FRAMEWORK + 128;
	msg[nr++] = FW_PAR_ID_GET_MEMORY;

	msg[nr++] = 0;
	msg[nr++] = 0;
	msg[nr++] = (char)memoryType;

	msg[nr++] = 0;
	msg[nr++] = (offset>>8) & 0xff;
	msg[nr++] = offset & 0xff;

	msg[nr++] = 0;
	msg[nr++] = (length>>8) & 0xff;
	msg[nr++] = length & 0xff;

	/* send msg */
	error = dsp_msg(tfa, nr, (char *)msg);

	if (error != Tfa98xx_Error_Ok)
		return error;

	/* read the data from the device (length * 3) */
	error = dsp_msg_read(tfa, length * 3, bytes);

	return error;
}

enum Tfa98xx_Error tfa98xx_dsp_set_memory(struct tfa_device *tfa, int memoryType,
		int offset, int length, int value)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	int nr = 0;
	char msg[5*3];

	msg[nr++] = 8;
	msg[nr++] = MODULE_FRAMEWORK + 128;
	msg[nr++] = FW_PAR_ID_SET_MEMORY;

	msg[nr++] = 0;
	msg[nr++] = 0;
	msg[nr++] = (char)memoryType;

	msg[nr++] = 0;
	msg[nr++] = (offset>>8) & 0xff;
	msg[nr++] = offset & 0xff;

	msg[nr++] = 0;
	msg[nr++] = (length>>8) & 0xff;
	msg[nr++] = length & 0xff;

	msg[nr++] = (value>>16) & 0xff;
	msg[nr++] = (value>>8) & 0xff;
	msg[nr++] = value & 0xff;

	/* send msg */
	error = dsp_msg(tfa, nr, (char *)msg);

	return error;
}
/****************************** calibration support **************************/
/*
 * get/set the mtp with user controllable values
 *
 *  check if the relevant clocks are available
 */
enum Tfa98xx_Error tfa98xx_get_mtp(struct tfa_device *tfa, uint16_t *value)
{
	int status;
	int result;

	/* not possible if PLL in powerdown */
	if ( TFA_GET_BF(tfa, PWDN) ) {
		pr_debug("PLL in powerdown\n");
		return Tfa98xx_Error_NoClock;
	}

	tfa98xx_dsp_system_stable(tfa, &status);
	if (status==0) {
		pr_debug("PLL not running\n");
		return Tfa98xx_Error_NoClock;
	}

	result = TFA_READ_REG(tfa, MTP0);
	if (result <  0) {
		return -result;
	}
	*value = (uint16_t)result;

	return Tfa98xx_Error_Ok;
}

/*
 * lock or unlock KEY2
 *  lock = 1 will lock
 *  lock = 0 will unlock
 *
 *  note that on return all the hidden key will be off
 */
void tfa98xx_key2(struct tfa_device *tfa, int lock)
{
	/* unhide lock registers */
	reg_write(tfa, (tfa->tfa_family == 1) ? 0x40 :0x0F, 0x5A6B);
	/* lock/unlock key2 MTPK */
	TFA_WRITE_REG(tfa, MTPKEY2, lock? 0 :0x5A );
	/* unhide lock registers */
	reg_write(tfa, (tfa->tfa_family == 1) ? 0x40 :0x0F, 0);
}

enum Tfa98xx_Error tfa98xx_set_mtp(struct tfa_device *tfa, uint16_t value, uint16_t mask)
{
	unsigned short mtp_old, mtp_new;
	int loop, status;
	enum Tfa98xx_Error error;

	error = tfa98xx_get_mtp(tfa, &mtp_old);

	if (error != Tfa98xx_Error_Ok)
		return error;

	mtp_new = (value & mask) | (mtp_old & ~mask);

	if ( mtp_old == mtp_new) /* no change */ {
		if (tfa->verbose)
			pr_info("No change in MTP. Value not written! \n");
		return Tfa98xx_Error_Ok;
	}

	/* Assure FAIM is enabled (enable it when neccesery) */
	error = tfa98xx_faim_protect(tfa, 1);
	if (error) {
		return error;
	}
	if (tfa->verbose) {
		pr_debug("MTP clock enabled.\n");
	}

	/* assure that the clock is up, else we can't write MTP */
	error = tfa98xx_dsp_system_stable(tfa, &status);
	if (error){
		return error;
	}
	if (status==0){
		return Tfa98xx_Error_NoClock;
	}

	tfa98xx_key2(tfa, 0); /* unlock */
	TFA_WRITE_REG(tfa, MTP0, mtp_new); 	/* write to i2c shadow reg */
	/* CIMTP=1 start copying all the data from i2c regs_mtp to mtp*/
	TFA_SET_BF(tfa, CIMTP, 1);
	/* no check for MTPBUSY here, i2c delay assumed to be enough */
	tfa98xx_key2(tfa, 1); /* lock */

	/* wait until MTP write is done */
	error = Tfa98xx_Error_StateTimedOut;
	for(loop=0; loop<100 /*x10ms*/ ;loop++) {
		msleep_interruptible(10); 			/* wait 10ms to avoid busload */
		if (tfa_dev_get_mtpb(tfa) == 0) {
			error = Tfa98xx_Error_Ok;
			break;
		}
	}
	/* MTP setting failed due to timeout ?*/
	if (error) {
		return error;
	}

	/* Disable the FAIM, if this is neccessary */
	error = tfa98xx_faim_protect(tfa, 0);
	if (error) {
		return error;
	}
	if (tfa->verbose) {
		pr_debug("MTP clock disabled.\n");
	}

	return error;
}
/*
 * clear mtpex
 * set ACS
 * start tfa
 */
int tfa_calibrate(struct tfa_device *tfa)
{
	enum Tfa98xx_Error error;

	/* clear mtpex */
	error = tfa98xx_set_mtp(tfa, 0, TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_MSK);
	if (error)
		return error ;

	/* set RST=1 to put the DSP in Reset */
	TFA_SET_BF(tfa, RST, 1);

	/* set ACS/coldboot state */
	error = tfaRunColdboot(tfa, 1);

	/* start tfa by playing */
	return error;
}

static short twos(short x)
{
	 return (x<0)? x+512 : x;
}

void tfa98xx_set_exttemp(struct tfa_device *tfa, short ext_temp)
{
	if ((-256 <= ext_temp) && (ext_temp <= 255)) {
		/* make twos complement */
		pr_debug("Using ext temp %d C\n", twos(ext_temp));
		TFA_SET_BF(tfa, TROS, 1);
		TFA_SET_BF(tfa, EXTTS, twos(ext_temp));
	} else {
		pr_debug("Clearing ext temp settings\n");
		TFA_SET_BF(tfa, TROS, 0);
	}
}
short tfa98xx_get_exttemp(struct tfa_device *tfa)
{
	short ext_temp = (short)TFA_GET_BF(tfa, EXTTS);
	return (twos(ext_temp));
}

/************************** tfa simple bitfield interfacing ***************************************/
/* convenience functions */
enum Tfa98xx_Error tfa98xx_set_volume_level(struct tfa_device *tfa, unsigned short vol)
{
	if(tfa->in_use == 0)
		return Tfa98xx_Error_NotOpen;

	if (vol > 255)	/* restricted to 8 bits */
		vol = 255;

	/* 0x00 -> 0.0 dB
	 * 0x01 -> -0.5 dB
	 * ...
	 * 0xFE -> -127dB
	 * 0xFF -> muted
	 */

	/* volume value is in the top 8 bits of the register */
	return -TFA_SET_BF(tfa, VOL, (uint16_t)vol);
}

static enum Tfa98xx_Error
tfa98xx_set_mute_tfa2(struct tfa_device *tfa, enum Tfa98xx_Mute mute)
{
	enum Tfa98xx_Error error;

	if (tfa->dev_ops.set_mute == NULL)
		return Tfa98xx_Error_Not_Supported;

	switch (mute) {
	case Tfa98xx_Mute_Off:
		error = tfa->dev_ops.set_mute(tfa, 0);
        TFA_SET_BF(tfa, AMPE, 1);
		break;
	case Tfa98xx_Mute_Amplifier:
	case Tfa98xx_Mute_Digital:
		error = tfa->dev_ops.set_mute(tfa, 1);
        TFA_SET_BF(tfa, AMPE, 0);
		break;
	default:
		return Tfa98xx_Error_Bad_Parameter;
	}

	return error;
}

static enum Tfa98xx_Error
tfa98xx_set_mute_tfa1(struct tfa_device *tfa, enum Tfa98xx_Mute mute)
{
	enum Tfa98xx_Error error;
	unsigned short audioctrl_value;
	unsigned short sysctrl_value;
	int value;

	value = TFA_READ_REG(tfa, CFSM); /* audio control register */
	if (value < 0)
		return -value;
	audioctrl_value = (unsigned short)value;
	value = TFA_READ_REG(tfa, AMPE); /* system control register */
	if (value < 0)
		return -value;
	sysctrl_value = (unsigned short)value;

	switch (mute) {
	case Tfa98xx_Mute_Off:
		/* previous state can be digital or amplifier mute,
		 * clear the cf_mute and set the enbl_amplifier bits
		 *
		 * To reduce PLOP at power on it is needed to switch the
		 * amplifier on with the DCDC in follower mode
		 * (enbl_boost = 0 ?).
		 * This workaround is also needed when toggling the
		 * powerdown bit!
		 */
		TFA_SET_BF_VALUE(tfa, CFSM, 0, &audioctrl_value);
		TFA_SET_BF_VALUE(tfa, AMPE, 1, &sysctrl_value);
		TFA_SET_BF_VALUE(tfa, DCA, 1, &sysctrl_value);
		break;
	case Tfa98xx_Mute_Digital:
		/* expect the amplifier to run */
		/* set the cf_mute bit */
		TFA_SET_BF_VALUE(tfa, CFSM, 1, &audioctrl_value);
		/* set the enbl_amplifier bit */
		TFA_SET_BF_VALUE(tfa, AMPE, 1, &sysctrl_value);
		/* clear active mode */
		TFA_SET_BF_VALUE(tfa, DCA, 0, &sysctrl_value);
		break;
	case Tfa98xx_Mute_Amplifier:
		/* clear the cf_mute bit */
		TFA_SET_BF_VALUE(tfa, CFSM, 0, &audioctrl_value);
		/* clear the enbl_amplifier bit and active mode */
		TFA_SET_BF_VALUE(tfa, AMPE, 0, &sysctrl_value);
		TFA_SET_BF_VALUE(tfa, DCA, 0, &sysctrl_value);
		break;
	default:
		return Tfa98xx_Error_Bad_Parameter;
	}

	error = -TFA_WRITE_REG(tfa, CFSM, audioctrl_value);
	if (error)
		return error;
	error = -TFA_WRITE_REG(tfa, AMPE, sysctrl_value);
	return error;
}

enum Tfa98xx_Error
tfa98xx_set_mute(struct tfa_device *tfa, enum Tfa98xx_Mute mute)
{
	if (tfa->in_use == 0) {
		pr_err("device is not opened \n");
		return Tfa98xx_Error_NotOpen;
	}

	if (tfa->tfa_family == 1)
		return tfa98xx_set_mute_tfa1(tfa, mute);
	else
		return tfa98xx_set_mute_tfa2(tfa, mute);
}

/****************** patching **********************************************************/
static enum Tfa98xx_Error
tfa98xx_process_patch_file(struct tfa_device *tfa, int length,
		 const unsigned char *bytes)
{
	unsigned short size;
	int index = 0;
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	while (index < length) {
		size = bytes[index] + bytes[index + 1] * 256;
		index += 2;
		if ((index + size) > length) {
			/* outside the buffer, error in the input data */
			return Tfa98xx_Error_Bad_Parameter;
		}

		if (size > tfa->buffer_size) {
			/* too big, must fit buffer */
			return Tfa98xx_Error_Bad_Parameter;
		}

		error = tfa98xx_write_raw(tfa, size, &bytes[index]);
		if (error != Tfa98xx_Error_Ok)
			break;
		index += size;
	}
	return  error;
}



/* the patch contains a header with the following
 * IC revision register: 1 byte, 0xFF means don't care
 * XMEM address to check: 2 bytes, big endian, 0xFFFF means don't care
 * XMEM value to expect: 3 bytes, big endian
 */
static enum Tfa98xx_Error
tfa98xx_check_ic_rom_version(struct tfa_device *tfa, const unsigned char patchheader[])
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	unsigned short checkrev, revid;
	unsigned char lsb_revid;
	unsigned short checkaddress;
	int checkvalue;
	int value = 0;
	int status;
	checkrev = patchheader[0];
	lsb_revid = tfa->rev & 0xff; /* only compare lower byte */

	if ((checkrev != 0xFF) && (checkrev != lsb_revid))
		return Tfa98xx_Error_Not_Supported;

	checkaddress = (patchheader[1] << 8) + patchheader[2];
	checkvalue =
	    (patchheader[3] << 16) + (patchheader[4] << 8) + patchheader[5];
	if (checkaddress != 0xFFFF) {
		/* before reading XMEM, check if we can access the DSP */
		error = tfa98xx_dsp_system_stable(tfa, &status);
		if (error == Tfa98xx_Error_Ok) {
			if (!status) {
				/* DSP subsys not running */
				error = Tfa98xx_Error_DSP_not_running;
			}
		}
		/* read register to check the correct ROM version */
		if (error == Tfa98xx_Error_Ok) {
			error = mem_read(tfa, checkaddress, 1, &value);
		}
		if (error == Tfa98xx_Error_Ok) {
			if (value != checkvalue) {
				pr_err("patch file romid type check failed [0x%04x]: expected 0x%02x, actual 0x%02x\n",
						checkaddress, value, checkvalue);
				error = Tfa98xx_Error_Not_Supported;
			}
		}
	} else { /* == 0xffff */
		/* check if the revid subtype is in there */
		if ( checkvalue != 0xFFFFFF && checkvalue != 0) {
			revid = patchheader[5]<<8 | patchheader[0]; /* full revid */
			if ( revid != tfa->rev) {
				pr_err("patch file device type check failed: expected 0x%02x, actual 0x%02x\n",
						tfa->rev, revid);
				return Tfa98xx_Error_Not_Supported;
			}
		}
	}

	return error;
}


#define PATCH_HEADER_LENGTH 6
enum Tfa98xx_Error
tfa_dsp_patch(struct tfa_device *tfa, int patchLength,
		 const unsigned char *patchBytes)
{
	enum Tfa98xx_Error error;
	if(tfa->in_use == 0)
		return Tfa98xx_Error_NotOpen;

	if (patchLength < PATCH_HEADER_LENGTH)
		return Tfa98xx_Error_Bad_Parameter;

	error = tfa98xx_check_ic_rom_version(tfa, patchBytes);
	if (Tfa98xx_Error_Ok != error) {
		return error;
	}
	error =
	    tfa98xx_process_patch_file(tfa, patchLength - PATCH_HEADER_LENGTH,
			     patchBytes + PATCH_HEADER_LENGTH);

	return error;
}

/******************  end patching **********************************************************/

TFA_INTERNAL enum Tfa98xx_Error
tfa98xx_wait_result(struct tfa_device *tfa, int wait_retry_count)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	int cf_status; /* the contents of the CF_STATUS register */
	int tries = 0;
	do {
		cf_status = TFA_GET_BF(tfa, ACK);
		if (cf_status < 0)
			error = -cf_status;
		tries++;
	}

	/* don't wait forever, DSP is pretty quick to respond (< 1ms) */
	while ((error == Tfa98xx_Error_Ok) && ((cf_status & CF_STATUS_I2C_CMD_ACK) == 0)
			&& (tries < wait_retry_count));
	if (tries >= wait_retry_count) {
		/* something wrong with communication with DSP */
		error = Tfa98xx_Error_DSP_not_running;
	}
	return error;
}

/*
 * *  support functions for data conversion
 */
/**
 convert memory bytes to signed 24 bit integers
   input:  bytes contains "num_bytes" byte elements
   output: data contains "num_bytes/3" int24 elements
*/
void tfa98xx_convert_bytes2data(int num_bytes, const unsigned char bytes[],
			       int data[])
{
	int i;			/* index for data */
	int k;			/* index for bytes */
	int d;
	int num_data = num_bytes / 3;
	_ASSERT((num_bytes % 3) == 0);
	for (i = 0, k = 0; i < num_data; ++i, k += 3) {
		d = (bytes[k] << 16) | (bytes[k + 1] << 8) | (bytes[k + 2]);
		_ASSERT(d >= 0);
		_ASSERT(d < (1 << 24));	/* max 24 bits in use */
		if (bytes[k] & 0x80)	/* sign bit was set */
			d = -((1 << 24) - d);

		data[i] = d;
	}
}


/**
 convert signed 32 bit integers to 24 bit aligned bytes
   input:   data contains "num_data" int elements
   output:  bytes contains "3 * num_data" byte elements
*/
void tfa98xx_convert_data2bytes(int num_data, const int data[],
			        unsigned char bytes[])
{
	int i;			/* index for data */
	int k;			/* index for bytes */
	int d;
	/* note: cannot just take the lowest 3 bytes from the 32 bit
	 * integer, because also need to take care of clipping any
	 * value > 2&23 */
	for (i = 0, k = 0; i < num_data; ++i, k += 3) {
		if (data[i] >= 0)
			d = MIN(data[i], (1 << 23) - 1);
		else {
			/* 2's complement */
			d = (1 << 24) - MIN(-data[i], 1 << 23);
		}
		_ASSERT(d >= 0);
		_ASSERT(d < (1 << 24));	/* max 24 bits in use */
		bytes[k] = (d >> 16) & 0xFF;	/* MSB */
		bytes[k + 1] = (d >> 8) & 0xFF;
		bytes[k + 2] = (d) & 0xFF;	/* LSB */
	}
}

/*
 *  DSP RPC message support functions
 *   depending on framework to be up and running
 *   need base i2c of memaccess (tfa1=0x70/tfa2=0x90)
 */


/* write dsp messages in function tfa_dsp_msg() */
/*  note the 'old' write_parameter() was more efficient because all i2c was in one burst transaction */


enum Tfa98xx_Error tfa_dsp_msg_write(struct tfa_device *tfa, int length, const char *buffer)
{
	int offset = 0;
	int chunk_size = ROUND_DOWN(tfa->buffer_size, 3);  /* XMEM word size */
	int remaining_bytes = length;
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	uint16_t cfctl;
	int value;

	value = TFA_READ_REG(tfa, DMEM);
	if (value < 0) {
		error = -value;
		return error;
	}
	cfctl = (uint16_t)value;
	/* assume no I2C errors from here */

	TFA_SET_BF_VALUE(tfa, DMEM, (uint16_t)Tfa98xx_DMEM_XMEM, &cfctl); /* set cf ctl to DMEM  */
	TFA_SET_BF_VALUE(tfa, AIF, 0, &cfctl ); /* set to autoincrement */
	TFA_WRITE_REG(tfa, DMEM, cfctl);

	/* xmem[1] is start of message
		*  direct write to register to save cycles avoiding read-modify-write
		*/
	TFA_WRITE_REG(tfa, MADD, 1);

	/* due to autoincrement in cf_ctrl, next write will happen at
	 * the next address */
	while ((error == Tfa98xx_Error_Ok) && (remaining_bytes > 0)) {
		if (remaining_bytes < chunk_size)
			chunk_size = remaining_bytes;
		/* else chunk_size remains at initialize value above */
		error = tfa98xx_write_data(tfa, FAM_TFA98XX_CF_MEM,
				      chunk_size, (const unsigned char *)buffer + offset);
		remaining_bytes -= chunk_size;
		offset += chunk_size;
	}

	/* notify the DSP */
	if (error == Tfa98xx_Error_Ok) {
		/* cf_int=0, cf_aif=0, cf_dmem=XMEM=01, cf_rst_dsp=0 */
		/* set the cf_req1 and cf_int bit */
		TFA_SET_BF_VALUE(tfa, REQCMD, 0x01, &cfctl ); /* bit 0 */
		TFA_SET_BF_VALUE(tfa, CFINT, 1, &cfctl );
		error = -TFA_WRITE_REG(tfa, CFINT, cfctl);
	}

	return error;
}

enum Tfa98xx_Error tfa_dsp_msg_write_id(struct tfa_device *tfa, int length, const char *buffer, uint8_t cmdid[3])
{
        int offset = 0;
	int chunk_size = ROUND_DOWN(tfa->buffer_size, 3);  /* XMEM word size */
	int remaining_bytes = length;
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	uint16_t cfctl;
	int value;

	value = TFA_READ_REG(tfa, DMEM);
	if (value < 0) {
		error = -value;
		return error;
	}
	cfctl = (uint16_t)value;
	/* assume no I2C errors from here */

	TFA_SET_BF_VALUE(tfa, DMEM, (uint16_t)Tfa98xx_DMEM_XMEM, &cfctl); /* set cf ctl to DMEM  */
	TFA_SET_BF_VALUE(tfa, AIF, 0, &cfctl ); /* set to autoincrement */
	TFA_WRITE_REG(tfa, DMEM, cfctl);

	/* xmem[1] is start of message
	 *  direct write to register to save cycles avoiding read-modify-write
	 */
	TFA_WRITE_REG(tfa, MADD, 1);

	/* write cmd-id */
	error = tfa98xx_write_data(tfa, FAM_TFA98XX_CF_MEM, 3, (const unsigned char *)cmdid);

	/* due to autoincrement in cf_ctrl, next write will happen at
	 * the next address */
	while ((error == Tfa98xx_Error_Ok) && (remaining_bytes > 0)) {
		if (remaining_bytes < chunk_size)
			chunk_size = remaining_bytes;
		/* else chunk_size remains at initialize value above */
		error = tfa98xx_write_data(tfa, FAM_TFA98XX_CF_MEM,
				      chunk_size, (const unsigned char *)buffer + offset);
		remaining_bytes -= chunk_size;
		offset += chunk_size;
	}

	/* notify the DSP */
	if (error == Tfa98xx_Error_Ok) {
		/* cf_int=0, cf_aif=0, cf_dmem=XMEM=01, cf_rst_dsp=0 */
		/* set the cf_req1 and cf_int bit */
		TFA_SET_BF_VALUE(tfa, REQCMD, 0x01, &cfctl ); /* bit 0 */
		TFA_SET_BF_VALUE(tfa, CFINT, 1, &cfctl );
		error = -TFA_WRITE_REG(tfa, CFINT, cfctl);
	}

	return error;
}

/*
* status function used by tfa_dsp_msg() to retrieve command/msg status:
* return a <0 status of the DSP did not ACK.
*/
enum Tfa98xx_Error tfa_dsp_msg_status(struct tfa_device *tfa, int *pRpcStatus)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	error = tfa98xx_wait_result(tfa, 2); /* 2 is only one try */
	if (error == Tfa98xx_Error_DSP_not_running) {
		*pRpcStatus = -1;
		return Tfa98xx_Error_Ok;
	}
	else if (error != Tfa98xx_Error_Ok)
		return error;

	error = tfa98xx_check_rpc_status(tfa, pRpcStatus);

	return error;
}

const char* tfa98xx_get_i2c_status_id_string(int status)
{
	const char* p_id_str;

	switch (status) {
		case Tfa98xx_DSP_Not_Running:
			p_id_str = "No response from DSP";
			break;
		case Tfa98xx_I2C_Req_Done:
			p_id_str = "Ok";
			break;
		case Tfa98xx_I2C_Req_Busy:
			p_id_str = "Request is being processed";
			break;
		case Tfa98xx_I2C_Req_Invalid_M_ID:
			p_id_str = "Provided M-ID does not fit in valid rang [0..2]";
			break;
		case Tfa98xx_I2C_Req_Invalid_P_ID:
			p_id_str = "Provided P-ID is not valid in the given M-ID context";
			break;
		case Tfa98xx_I2C_Req_Invalid_CC:
			p_id_str = "Invalid channel configuration bits (SC|DS|DP|DC) combination";
			break;
		case Tfa98xx_I2C_Req_Invalid_Seq:
			p_id_str = "Invalid sequence of commands, in case the DSP expects some commands in a specific order";
			break;
		case Tfa98xx_I2C_Req_Invalid_Param:
			p_id_str = "Generic error, invalid parameter";
			break;
		case Tfa98xx_I2C_Req_Buffer_Overflow:
			p_id_str = "I2C buffer has overflowed: host has sent too many parameters, memory integrity is not guaranteed";
			break;
		case Tfa98xx_I2C_Req_Calib_Busy:
			p_id_str = "Calibration not completed";
			break;
		case Tfa98xx_I2C_Req_Calib_Failed:
			p_id_str = "Calibration failed";
			break;

		default:
			p_id_str = "Unspecified error";
	}

	return p_id_str;
}

enum Tfa98xx_Error tfa_dsp_msg_read(struct tfa_device *tfa, int length, unsigned char *bytes)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	int burst_size;		/* number of words per burst size */
	int bytes_per_word = 3;
	int num_bytes;
	int offset = 0;
	unsigned short start_offset=2; /* msg starts @xmem[2] ,[1]=cmd */

	if ( length > TFA2_MAX_PARAM_SIZE)
		return Tfa98xx_Error_Bad_Parameter;

	TFA_SET_BF(tfa, DMEM, (uint16_t)Tfa98xx_DMEM_XMEM);
	error = -TFA_WRITE_REG(tfa, MADD, start_offset);
	if (error != Tfa98xx_Error_Ok)
		return error;

	num_bytes = length; /* input param */
	while (num_bytes > 0) {
		burst_size = ROUND_DOWN(tfa->buffer_size, bytes_per_word);
		if (num_bytes < burst_size)
			burst_size = num_bytes;
		error = tfa98xx_read_data(tfa, FAM_TFA98XX_CF_MEM, burst_size, bytes + offset);
		if (error != Tfa98xx_Error_Ok)
			return error;

		num_bytes -= burst_size;
		offset += burst_size;
	}

	return error;
}

enum Tfa98xx_Error dsp_msg(struct tfa_device *tfa, int length24, const char *buf24)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	int lastmessage=0;
	uint8_t *blob;
	int i;
	int *intbuf = NULL;
	char* buf = (char *)buf24;
	int length = length24;

	if (tfa->convert_dsp32) {
		int idx = 0;

		length = 4 * length24 / 3;
		intbuf = kmem_cache_alloc(tfa->cachep, GFP_KERNEL);
		buf = (char *)intbuf;

		/* convert 24 bit DSP messages to a 32 bit integer */
		for (i=0; i<length24; i+=3) {
			int tmp = (buf24[i] << 16) + (buf24[i+1] << 8) + buf24[i+2];
			/* Sign extend to 32-bit from 24-bit */
			intbuf[idx++] = ((int32_t)tmp << 8) >> 8;
		}
	}

	/* Only create multi-msg when the dsp is cold */
	if(tfa->ext_dsp == 1) {
		/* Creating the multi-msg */
		error = tfa_tib_dsp_msgmulti(tfa, length, buf);
		if(error == Tfa98xx_Error_Fail)
			return Tfa98xx_Error_Fail;

		/* if the buffer is full we need to send the existing message and add the current message */
		if(error == Tfa98xx_Error_Buffer_too_small) {
			int len;

			/* (a) send the existing (full) message */
			blob = kmalloc(64*1024, GFP_KERNEL);
			len = tfa_tib_dsp_msgmulti(tfa, -1, (const char*)blob);
			if (tfa->verbose) {
				pr_debug("Multi-message buffer full. Sending multi-message, length=%d \n", len);
			}
			if (tfa->has_msg==0 ) /* via i2c */ {
				/* Send tot the target selected */
				error = (tfa->dev_ops.dsp_msg)(tfa, len, (const char*)blob);
			} else { /* via msg hal */
				error = tfa98xx_write_dsp(tfa, len, (const char*)blob);
			}
			kfree(blob);

			/* (b) add the current DSP message to a new multi-message */
			error = tfa_tib_dsp_msgmulti(tfa, length, buf);
			if(error == Tfa98xx_Error_Fail) {
				return Tfa98xx_Error_Fail;
			}
		}

		lastmessage = error;

		/* When the lastmessage is done we can send the multi-msg to the target */
		if(lastmessage == 1) {

			/* Get the full multi-msg data */
			blob = kmalloc(64*1024, GFP_KERNEL);
			length = tfa_tib_dsp_msgmulti(tfa, -1, (const char*)blob);

			if (tfa->verbose)
				pr_debug("Last message for the multi-message received. Multi-message length=%d \n", length);

			if (tfa->has_msg==0 ) /* via i2c */ {
				/* Send tot the target selected */
				error = (tfa->dev_ops.dsp_msg)(tfa, length, (const char*)blob);
			} else { /* via msg hal */
				error = tfa98xx_write_dsp(tfa, length, (const char*)blob);
			}

			kfree(blob); /* Free the kmalloc blob */
			lastmessage = 0; /* reset to be able to re-start */
		}
	} else {
		if (tfa->has_msg==0 ) /* via i2c */ {
			error = (tfa->dev_ops.dsp_msg)(tfa, length, buf);
		} else { /* via msg hal */
			error = tfa98xx_write_dsp(tfa, length, (const char*)buf);
		}
	}

	if(error != Tfa98xx_Error_Ok)
		error = (enum Tfa98xx_Error) (error + Tfa98xx_Error_RpcBase); /* Get actual error code from softDSP */

	/* DSP verbose has argument 0x04 */
	if((tfa->verbose & 0x04)!=0) {
		pr_debug("DSP w [%d]: ", length);
		for(i=0; i<length; i++)
			pr_debug("0x%02x ", (uint8_t)buf[i]);
		pr_debug("\n");
	}

	if (tfa->convert_dsp32) {
		kmem_cache_free(tfa->cachep, intbuf);
	}

	return error;
}

enum Tfa98xx_Error dsp_msg_read(struct tfa_device *tfa, int length24, unsigned char *bytes24)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	int i;
	int length = length24;
	unsigned char *bytes = bytes24;

	if (tfa->convert_dsp32) {
		length = 4 * length24 / 3;
		bytes = kmem_cache_alloc(tfa->cachep, GFP_KERNEL);
	}

	if (tfa->has_msg==0) /* via i2c */ {
		error = (tfa->dev_ops.dsp_msg_read)(tfa, length, bytes);
	} else { /* via msg hal */
		error = tfa98xx_read_dsp(tfa, length, bytes);
	}

	if(error != Tfa98xx_Error_Ok)
		error = (enum Tfa98xx_Error) (error + Tfa98xx_Error_RpcBase); /* Get actual error code from softDSP */

	/* DSP verbose has argument 0x04 */
	if((tfa->verbose & 0x04)!=0) {
		pr_debug("DSP R [%d]: ", length);
		for(i=0; i<length; i++)
			pr_debug("0x%02x ", (uint8_t)bytes[i]);
		pr_debug("\n");
	}

	if (tfa->convert_dsp32) {
		int idx = 0;

		/* convert 32 bit LE to 24 bit BE */
		for (i=0; i<length; i+=4) {
			bytes24[idx++] = bytes[i + 2];
			bytes24[idx++] = bytes[i + 1];
			bytes24[idx++] = bytes[i + 0];
		}

		kmem_cache_free(tfa->cachep, bytes);
	}

	return error;
}

enum Tfa98xx_Error reg_read(struct tfa_device *tfa, unsigned char subaddress, unsigned short *value)
{
	enum Tfa98xx_Error error;

	error = (tfa->dev_ops.reg_read)(tfa, subaddress, value);
	if(error != Tfa98xx_Error_Ok)
		error = (enum Tfa98xx_Error) (error + Tfa98xx_Error_RpcBase); /* Get actual error code from softDSP */

	return error;
}

enum Tfa98xx_Error reg_write(struct tfa_device *tfa, unsigned char subaddress, unsigned short value)
{
	enum Tfa98xx_Error error;

	error = (tfa->dev_ops.reg_write)(tfa, subaddress, value);
	if(error != Tfa98xx_Error_Ok)
		error = (enum Tfa98xx_Error) (error + Tfa98xx_Error_RpcBase); /* Get actual error code from softDSP */

	return error;
}

enum Tfa98xx_Error mem_read(struct tfa_device *tfa, unsigned int start_offset, int num_words, int *pValues)
{
	enum Tfa98xx_Error error;

	error = (tfa->dev_ops.mem_read)(tfa, start_offset, num_words, pValues);
	if(error != Tfa98xx_Error_Ok)
		error = (enum Tfa98xx_Error) (error + Tfa98xx_Error_RpcBase); /* Get actual error code from softDSP */

	return error;
}

enum Tfa98xx_Error mem_write(struct tfa_device *tfa, unsigned short address, int value, int memtype)
{
	enum Tfa98xx_Error error;

	error = (tfa->dev_ops.mem_write)(tfa, address, value, memtype);
	if(error != Tfa98xx_Error_Ok)
		error = (enum Tfa98xx_Error) (error + Tfa98xx_Error_RpcBase); /* Get actual error code from softDSP */

	return error;
}


/*
 *  write/read raw msg functions :
 *  the buffer is provided in little endian format, each word occupying 3 bytes, length is in bytes.
 *  The functions will return immediately and do not not wait for DSP reponse.
 */
#define MAX_WORDS (300)
enum Tfa98xx_Error tfa_dsp_msg(struct tfa_device *tfa, int length, const char *buf)
{
	enum Tfa98xx_Error error;
	int tries, rpc_status = Tfa98xx_I2C_Req_Done;

	/* write the message and notify the DSP */
	error = tfa_dsp_msg_write(tfa, length, buf);
	if( error != Tfa98xx_Error_Ok)
		return error;

	/* get the result from the DSP (polling) */
	for(tries=TFA98XX_WAITRESULT_NTRIES; tries>0;tries--) {
		error = tfa_dsp_msg_status(tfa, &rpc_status);
                if (error == Tfa98xx_Error_Ok && rpc_status == Tfa98xx_I2C_Req_Done)
			break;
		/* If the rpc status is a specific error we want to know it.
		 * If it is busy or not running it should retry
		 */
		if(rpc_status != Tfa98xx_I2C_Req_Busy && rpc_status != Tfa98xx_DSP_Not_Running)
			break;
	}

	if (rpc_status != Tfa98xx_I2C_Req_Done) {
		/* DSP RPC call returned an error */
		error = (enum Tfa98xx_Error) (rpc_status + Tfa98xx_Error_RpcBase);
                pr_debug("DSP msg status: %d (%s)\n", rpc_status, tfa98xx_get_i2c_status_id_string(rpc_status));
	}
	return error;
}

/**
 *  write/read raw msg functions:
 *  the buffer is provided in little endian format, each word occupying 3 bytes, length is in bytes.
 *  The functions will return immediately and do not not wait for DSP reponse.
 *  An ID is added to modify the command-ID
 */
enum Tfa98xx_Error tfa_dsp_msg_id(struct tfa_device *tfa, int length, const char *buf, uint8_t cmdid[3])
{
	enum Tfa98xx_Error error;
	int tries, rpc_status = Tfa98xx_I2C_Req_Done;

	/* write the message and notify the DSP */
	error = tfa_dsp_msg_write_id(tfa, length, buf, cmdid);
	if( error != Tfa98xx_Error_Ok)
		return error;

	/* get the result from the DSP (polling) */
	for(tries=TFA98XX_WAITRESULT_NTRIES; tries>0;tries--) {
		error = tfa_dsp_msg_status(tfa, &rpc_status);
                if (error == Tfa98xx_Error_Ok && rpc_status == Tfa98xx_I2C_Req_Done)
			break;
	}

	if (rpc_status != Tfa98xx_I2C_Req_Done) {
		/* DSP RPC call returned an error */
		error = (enum Tfa98xx_Error) (rpc_status + Tfa98xx_Error_RpcBase);
                pr_debug("DSP msg status: %d (%s)\n", rpc_status, tfa98xx_get_i2c_status_id_string(rpc_status));
	}
	return error;
}

/* read the return code for the RPC call */
TFA_INTERNAL enum Tfa98xx_Error
tfa98xx_check_rpc_status(struct tfa_device *tfa, int *pRpcStatus)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	/* the value to sent to the * CF_CONTROLS register: cf_req=00000000,
	 * cf_int=0, cf_aif=0, cf_dmem=XMEM=01, cf_rst_dsp=0 */
	unsigned short cf_ctrl = 0x0002;
	/* memory address to be accessed (0: Status, 1: ID, 2: parameters) */
	unsigned short cf_mad = 0x0000;

	if(tfa->in_use == 0)
		return Tfa98xx_Error_NotOpen;
	if (pRpcStatus == NULL)
		return Tfa98xx_Error_Bad_Parameter;

	/* 1) write DMEM=XMEM to the DSP XMEM */
	{
		/* minimize the number of I2C transactions by making use of the autoincrement in I2C */
		unsigned char buffer[4];
		/* first the data for CF_CONTROLS */
		buffer[0] = (unsigned char)((cf_ctrl >> 8) & 0xFF);
		buffer[1] = (unsigned char)(cf_ctrl & 0xFF);
		/* write the contents of CF_MAD which is the subaddress following CF_CONTROLS */
		buffer[2] = (unsigned char)((cf_mad >> 8) & 0xFF);
		buffer[3] = (unsigned char)(cf_mad & 0xFF);
		error = tfa98xx_write_data(tfa, FAM_TFA98XX_CF_CONTROLS, sizeof(buffer), buffer);
	}
	if (error == Tfa98xx_Error_Ok) {
		/* read 1 word (24 bit) from XMEM */
		error = tfa98xx_dsp_read_mem(tfa, 0, 1, pRpcStatus);
	}

	return error;
}

/***************************** xmem only **********************************/
enum Tfa98xx_Error
tfa98xx_dsp_read_mem(struct tfa_device *tfa,
		unsigned int start_offset, int num_words, int *pValues)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	unsigned char *bytes;
	int burst_size;		/* number of words per burst size */
	const int bytes_per_word = 3;
	int dmem;
	int num_bytes;
	int *p;

	bytes = (unsigned char *)kmem_cache_alloc(tfa->cachep, GFP_KERNEL);
	if (bytes == NULL)
		return Tfa98xx_Error_Fail;

	/* If no offset is given, assume XMEM! */
	if(((start_offset>>16) & 0xf) > 0 )
		dmem = (start_offset>>16) & 0xf;
	else
		dmem = Tfa98xx_DMEM_XMEM;

	/* Remove offset from adress */
	start_offset = start_offset & 0xffff;
	num_bytes = num_words * bytes_per_word;
	p = pValues;

	TFA_SET_BF(tfa, DMEM, (uint16_t)dmem);
	error = -TFA_WRITE_REG(tfa, MADD, (unsigned short)start_offset);
	if (error != Tfa98xx_Error_Ok)
		goto tfa98xx_dsp_read_mem_exit;

	for (; num_bytes > 0;) {
		burst_size = ROUND_DOWN(tfa->buffer_size, bytes_per_word);
		if (num_bytes < burst_size)
			burst_size = num_bytes;

		_ASSERT(burst_size <= sizeof(bytes));
		error = tfa98xx_read_data(tfa, FAM_TFA98XX_CF_MEM, burst_size, bytes);
		if (error != Tfa98xx_Error_Ok)
			goto tfa98xx_dsp_read_mem_exit;

		tfa98xx_convert_bytes2data(burst_size, bytes, p);

		num_bytes -= burst_size;
		p += burst_size / bytes_per_word;
	}

tfa98xx_dsp_read_mem_exit:
	kmem_cache_free(tfa->cachep, bytes);

	return error;
}


enum Tfa98xx_Error
tfa98xx_dsp_write_mem_word(struct tfa_device *tfa, unsigned short address, int value, int memtype)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	unsigned char bytes[3];

	TFA_SET_BF(tfa, DMEM, (uint16_t)memtype);

	error = -TFA_WRITE_REG(tfa, MADD, address);
	if (error != Tfa98xx_Error_Ok)
		return error;

	tfa98xx_convert_data2bytes(1, &value, bytes);
	error = tfa98xx_write_data(tfa, FAM_TFA98XX_CF_MEM, 3, bytes);

	return error;
}

enum Tfa98xx_Error tfa_cont_write_filterbank(struct tfa_device *tfa, nxpTfaFilter_t *filter)
{
	unsigned char biquad_index;
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	for(biquad_index=0;biquad_index<10;biquad_index++) {
		if (filter[biquad_index].enabled ) {
			error = tfa_dsp_cmd_id_write(tfa, MODULE_BIQUADFILTERBANK,
					biquad_index+1,
					sizeof(filter[biquad_index].biquad.bytes),
						filter[biquad_index].biquad.bytes);
		} else {
			error = Tfa98xx_DspBiquad_Disable(tfa, biquad_index+1);
		}
		if (error) return error;

	}

	return error;
}

enum Tfa98xx_Error
Tfa98xx_DspBiquad_Disable(struct tfa_device *tfa, int biquad_index)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	int coeff_buffer[BIQUAD_COEFF_SIZE];
	unsigned char bytes[3 + BIQUAD_COEFF_SIZE * 3];
	int nr = 0;

	if (biquad_index > TFA98XX_BIQUAD_NUM)
		return Tfa98xx_Error_Bad_Parameter;
	if (biquad_index < 1)
		return Tfa98xx_Error_Bad_Parameter;

	/* make opcode */
	bytes[nr++] = 0;
	bytes[nr++] = MODULE_BIQUADFILTERBANK+128;
	bytes[nr++] = (unsigned char)biquad_index;


	/* set in correct order and format for the DSP */
	coeff_buffer[0] = (int) - 8388608;	/* -1.0f */
	coeff_buffer[1] = 0;
	coeff_buffer[2] = 0;
	coeff_buffer[3] = 0;
	coeff_buffer[4] = 0;
	coeff_buffer[5] = 0;

	/* convert to packed 24 */
	tfa98xx_convert_data2bytes(BIQUAD_COEFF_SIZE, coeff_buffer, &bytes[nr]);
	nr += BIQUAD_COEFF_SIZE * 3;

	error = dsp_msg(tfa, nr, (char *)bytes);

	return error;
}

/* wrapper for dsp_msg that adds opcode */
enum Tfa98xx_Error tfa_dsp_cmd_id_write(struct tfa_device *tfa,
			   unsigned char module_id,
			   unsigned char param_id, int num_bytes,
                           const unsigned char data[])
{
	enum Tfa98xx_Error error;
	unsigned char *buffer;
	int nr = 0;

	buffer = kmem_cache_alloc(tfa->cachep, GFP_KERNEL);
	if (buffer == NULL)
		return Tfa98xx_Error_Fail;

	buffer[nr++] = tfa->spkr_select;
	buffer[nr++] = module_id + 128;
	buffer[nr++] = param_id;

	memcpy(&buffer[nr], data, num_bytes);
	nr += num_bytes;

	error = dsp_msg(tfa, nr, (char *)buffer);

	kmem_cache_free(tfa->cachep, buffer);

	return error;
}

/* wrapper for dsp_msg that adds opcode */
/* this is as the former tfa98xx_dsp_get_param() */
enum Tfa98xx_Error tfa_dsp_cmd_id_write_read(struct tfa_device *tfa,
			   unsigned char module_id,
			   unsigned char param_id, int num_bytes,
                           unsigned char data[])
{
	enum Tfa98xx_Error error;
	unsigned char buffer[3];
	int nr = 0;

	if (num_bytes <= 0) {
		pr_debug("Error: The number of READ bytes is smaller or equal to 0! \n");
		return Tfa98xx_Error_Fail;
	}

	if ((tfa->is_probus_device) && (tfa->cnt->ndev == 1) &&
		(param_id == SB_PARAM_GET_RE25C ||
		 param_id == SB_PARAM_GET_LSMODEL ||
		 param_id == SB_PARAM_GET_ALGO_PARAMS)) {
		/* Modifying the ID for GetRe25C */
		buffer[nr++] = 4;
	} else {
		buffer[nr++] = tfa->spkr_select;
	}
	buffer[nr++] = module_id + 128;
	buffer[nr++] = param_id;

	error = dsp_msg(tfa, nr, (char *)buffer);
	if (error != Tfa98xx_Error_Ok)
		return error;

	/* read the data from the dsp */
	error = dsp_msg_read(tfa, num_bytes, data);
	return error;
}

/* wrapper for dsp_msg that adds opcode and 3 bytes required for coefs */
enum Tfa98xx_Error tfa_dsp_cmd_id_coefs(struct tfa_device *tfa,
			   unsigned char module_id,
			   unsigned char param_id, int num_bytes,
			   unsigned char data[])
{
	enum Tfa98xx_Error error;
	unsigned char buffer[2*3];
	int nr = 0;

	buffer[nr++] = tfa->spkr_select;
	buffer[nr++] = module_id + 128;
	buffer[nr++] = param_id;

        buffer[nr++] = 0;
        buffer[nr++] = 0;
        buffer[nr++] = 0;

	error = dsp_msg(tfa, nr, (char *)buffer);
	if (error != Tfa98xx_Error_Ok)
		return error;

	/* read the data from the dsp */
	error = dsp_msg_read(tfa, num_bytes, data);

	return error;
}

/* wrapper for dsp_msg that adds opcode and 3 bytes required for MBDrcDynamics */
enum Tfa98xx_Error tfa_dsp_cmd_id_MBDrc_dynamics(struct tfa_device *tfa,
			   unsigned char module_id,
			   unsigned char param_id, int index_subband,
			   int num_bytes, unsigned char data[])
{
	enum Tfa98xx_Error error;
	unsigned char buffer[2 * 3];
	int nr = 0;

	buffer[nr++] = tfa->spkr_select;
	buffer[nr++] = module_id + 128;
	buffer[nr++] = param_id;

        buffer[nr++] = 0;
        buffer[nr++] = 0;
        buffer[nr++] = (unsigned char)index_subband;

	error = dsp_msg(tfa, nr, (char *)buffer);
	if(error != Tfa98xx_Error_Ok)
		return error;

	/* read the data from the dsp */
	error = dsp_msg_read(tfa, num_bytes, data);

	return error;
}

enum Tfa98xx_Error
tfa98xx_dsp_write_preset(struct tfa_device *tfa, int length,
		       const unsigned char *p_preset_bytes)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	if (p_preset_bytes != NULL) {
		/* by design: keep the data opaque and no
		 * interpreting/calculation */
		error = tfa_dsp_cmd_id_write(tfa, MODULE_SPEAKERBOOST,
					SB_PARAM_SET_PRESET, length,
					p_preset_bytes);
	} else {
		error = Tfa98xx_Error_Bad_Parameter;
	}
	return error;
}

/*
 * get features from MTP
 */
enum Tfa98xx_Error
tfa98xx_dsp_get_hw_feature_bits(struct tfa_device *tfa, int *features)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	uint32_t value;
	uint16_t mtpbf;

	/* return the cache data if it's valid */
	if (tfa->hw_feature_bits != -1) {
		*features = tfa->hw_feature_bits;
	} else {
		/* for tfa1 check if we have clock */
		if (tfa->tfa_family == 1) {
			int status;
			tfa98xx_dsp_system_stable(tfa, &status);
			if (!status) {
				get_hw_features_from_cnt(tfa, features);
				/* skip reading MTP: */
				return (*features == -1) ? Tfa98xx_Error_Fail : Tfa98xx_Error_Ok;
			}
			mtpbf=0x850f;  /* MTP5 for tfa1,16 bits */
		} else
			mtpbf=0xf907;  /* MTP9 for tfa2, 8 bits */
		value = tfa_read_reg(tfa, mtpbf) & 0xffff;
		*features = tfa->hw_feature_bits = value;
	}

	return error;
}

enum Tfa98xx_Error
tfa98xx_dsp_get_sw_feature_bits(struct tfa_device *tfa, int features[2])
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	const int byte_size = 2 * 3;
	unsigned char bytes[2 * 3];

	/* return the cache data if it's valid */
	if (tfa->sw_feature_bits[0] != -1) {
		features[0] = tfa->sw_feature_bits[0];
		features[1] = tfa->sw_feature_bits[1];
	} else {
		/* for tfa1 check if we have clock */
		if (tfa->tfa_family == 1) {
			int status;
			tfa98xx_dsp_system_stable(tfa, &status);
			if (!status) {
				get_sw_features_from_cnt(tfa, features);
				/* skip reading MTP: */
				return (features[0] == -1) ? Tfa98xx_Error_Fail : Tfa98xx_Error_Ok;
			}
		}
		error = tfa_dsp_cmd_id_write_read(tfa, MODULE_FRAMEWORK,
				FW_PAR_ID_GET_FEATURE_INFO, byte_size, bytes);

		if (error != Tfa98xx_Error_Ok) {
			/* old ROM code may respond with Tfa98xx_Error_RpcParamId */
			return error;
		}

		tfa98xx_convert_bytes2data(byte_size, bytes, features);
	}
	return error;
}

enum Tfa98xx_Error tfa98xx_dsp_get_state_info(struct tfa_device *tfa, unsigned char bytes[], unsigned int *statesize)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	int bSupportFramework = 0;
	unsigned int stateSize = 9;

	err = tfa98xx_dsp_support_framework(tfa, &bSupportFramework);
	if (err == Tfa98xx_Error_Ok) {
		if (bSupportFramework) {
			err = tfa_dsp_cmd_id_write_read(tfa, MODULE_FRAMEWORK,
				FW_PARAM_GET_STATE, 3 * stateSize, bytes);
		} else {
			/* old ROM code, ask SpeakerBoost and only do first portion */
			stateSize = 8;
			err = tfa_dsp_cmd_id_write_read(tfa, MODULE_SPEAKERBOOST,
				SB_PARAM_GET_STATE, 3 * stateSize, bytes);
		}
	}

	*statesize = stateSize;

	return err;
}

enum Tfa98xx_Error tfa98xx_dsp_support_drc(struct tfa_device *tfa, int *pbSupportDrc)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	*pbSupportDrc = 0;

	if(tfa->in_use == 0)
		return Tfa98xx_Error_NotOpen;
	if (tfa->supportDrc != supportNotSet) {
		*pbSupportDrc = (tfa->supportDrc == supportYes);
	} else {
		int featureBits[2];

		error = tfa98xx_dsp_get_sw_feature_bits(tfa, featureBits);
		if (error == Tfa98xx_Error_Ok) {
			/* easy case: new API available */
			/* bit=0 means DRC enabled */
			*pbSupportDrc = (featureBits[0] & FEATURE1_DRC) == 0;
		} else if (error == Tfa98xx_Error_RpcParamId) {
			/* older ROM code, doesn't support it */
			*pbSupportDrc = 0;
			error = Tfa98xx_Error_Ok;
		}
		/* else some other error, return transparently */
		/* pbSupportDrc only changed when error == Tfa98xx_Error_Ok */

		if (error == Tfa98xx_Error_Ok) {
			tfa->supportDrc = *pbSupportDrc ? supportYes : supportNo;
		}
	}
	return error;
}

enum Tfa98xx_Error
tfa98xx_dsp_support_framework(struct tfa_device *tfa, int *pbSupportFramework)
{
	int featureBits[2] = { 0, 0 };
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	_ASSERT(pbSupportFramework != 0);

	if(tfa->in_use == 0)
		return Tfa98xx_Error_NotOpen;

	if (tfa->supportFramework != supportNotSet) {
		if(tfa->supportFramework == supportNo)
			*pbSupportFramework = 0;
		else
			*pbSupportFramework = 1;
	} else {
		error = tfa98xx_dsp_get_sw_feature_bits(tfa, featureBits);
		if (error == Tfa98xx_Error_Ok) {
			*pbSupportFramework = 1;
			tfa->supportFramework = supportYes;
		} else {
			*pbSupportFramework = 0;
			tfa->supportFramework = supportNo;
			error = Tfa98xx_Error_Ok;
		}
	}

	/* *pbSupportFramework only changed when error == Tfa98xx_Error_Ok */
	return error;
}

enum Tfa98xx_Error
tfa98xx_dsp_write_speaker_parameters(struct tfa_device *tfa,
		int length, const unsigned char *p_speaker_bytes)
{
	enum Tfa98xx_Error error;
	int bSupportDrc;

	if (p_speaker_bytes != NULL) {
		/* by design: keep the data opaque and no
		 * interpreting/calculation */
		/* Use long WaitResult retry count */
		error = tfa_dsp_cmd_id_write(
					tfa,
					MODULE_SPEAKERBOOST,
					SB_PARAM_SET_LSMODEL, length,
					p_speaker_bytes);
	} else {
		error = Tfa98xx_Error_Bad_Parameter;
	}

	if (error != Tfa98xx_Error_Ok)
		return error;

	error = tfa98xx_dsp_support_drc(tfa, &bSupportDrc);
	if (error != Tfa98xx_Error_Ok)
		return error;

	if (bSupportDrc) {
		/* Need to set AgcGainInsert back to PRE,
		 * as the SetConfig forces it to POST */
		uint8_t bytes[3] = {0, 0, 0};

		error = tfa_dsp_cmd_id_write(tfa,
					     MODULE_SPEAKERBOOST,
					     SB_PARAM_SET_AGCINS,
					     3,
					     bytes);
	}

	return error;
}

enum Tfa98xx_Error
tfa98xx_dsp_write_config(struct tfa_device *tfa, int length,
		   const unsigned char *p_config_bytes)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	int bSupportDrc;

	error = tfa_dsp_cmd_id_write(tfa,
				      MODULE_SPEAKERBOOST,
				      SB_PARAM_SET_CONFIG, length,
				      p_config_bytes);
	if (error != Tfa98xx_Error_Ok)
		return error;

	error = tfa98xx_dsp_support_drc(tfa, &bSupportDrc);
	if (error != Tfa98xx_Error_Ok)
		return error;

	if (bSupportDrc) {
		/* Need to set AgcGainInsert back to PRE,
		 * as the SetConfig forces it to POST */
		uint8_t bytes[3] = {0, 0, 0};

		error = tfa_dsp_cmd_id_write(tfa,
					     MODULE_SPEAKERBOOST,
					     SB_PARAM_SET_AGCINS,
					     3,
					     bytes);
	}

	return error;
}

/* load all the parameters for the DRC settings from a file */
enum Tfa98xx_Error tfa98xx_dsp_write_drc(struct tfa_device *tfa,
				         int length, const unsigned char *p_drc_bytes)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	if (p_drc_bytes != NULL) {
		error = tfa_dsp_cmd_id_write(tfa,
				        MODULE_SPEAKERBOOST,
				        SB_PARAM_SET_DRC, length,
				        p_drc_bytes);

	} else {
		error = Tfa98xx_Error_Bad_Parameter;
	}
	return error;
}

enum Tfa98xx_Error tfa98xx_powerdown(struct tfa_device *tfa, int powerdown)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	if (tfa->in_use == 0)
		return Tfa98xx_Error_NotOpen;

	error = TFA_SET_BF(tfa, PWDN, (uint16_t)powerdown);

	if (powerdown) {
		/* Workaround for ticket PLMA5337 */
		if (tfa->tfa_family == 2) {
			TFA_SET_BF_VOLATILE(tfa, AMPE, 0);
		}
	}

	return error;
}

enum Tfa98xx_Error
tfa98xx_select_mode(struct tfa_device *tfa, enum Tfa98xx_Mode mode)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	if(tfa->in_use == 0)
		return Tfa98xx_Error_NotOpen;

	if (error == Tfa98xx_Error_Ok) {
		switch (mode) {

		default:
			error = Tfa98xx_Error_Bad_Parameter;
		}
	}

	return error;
}

int tfa_set_bf(struct tfa_device *tfa, const uint16_t bf, const uint16_t value)
{
	enum Tfa98xx_Error err;
	uint16_t regvalue, msk, oldvalue;

	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;
	uint8_t address = (bf >> 8) & 0xff;

	err = reg_read(tfa, address, &regvalue);
	if (err) {
		pr_err("Error getting bf :%d \n", -err);
		return -err;
	}

	oldvalue = regvalue;
	msk = ((1<<(len+1))-1)<<pos;
	regvalue &= ~msk;
	regvalue |= value<<pos;

	/* Only write when the current register value is not the same as the new value */
	if(oldvalue != regvalue) {
		err = reg_write(tfa, address, regvalue);
		if (err) {
			pr_err("Error setting bf :%d \n", -err);
			return -err;
		}
	}

	return 0;
}

int tfa_set_bf_volatile(struct tfa_device *tfa, const uint16_t bf, const uint16_t value)
{
	enum Tfa98xx_Error err;
	uint16_t regvalue, msk;

	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;
	uint8_t address = (bf >> 8) & 0xff;

	err = reg_read(tfa, address, &regvalue);
	if (err) {
		pr_err("Error getting bf :%d \n", -err);
		return -err;
	}

	msk = ((1<<(len+1))-1)<<pos;
	regvalue &= ~msk;
	regvalue |= value<<pos;

	err = reg_write(tfa, address, regvalue);
	if (err) {
		pr_err("Error setting bf :%d \n", -err);
		return -err;
	}

	return 0;
}

int tfa_get_bf(struct tfa_device *tfa, const uint16_t bf)
{
	enum Tfa98xx_Error err;
	uint16_t regvalue, msk;
	uint16_t value;

	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;
	uint8_t address = (bf >> 8) & 0xff;

	err = reg_read(tfa, address, &regvalue);
	if (err) {
		pr_err("Error getting bf :%d \n", -err);
		return -err;
	}

	msk = ((1<<(len+1))-1)<<pos;
	regvalue &= msk;
	value = regvalue>>pos;

	return value;
}

int tfa_set_bf_value(const uint16_t bf, const uint16_t bf_value, uint16_t *p_reg_value)
{
	uint16_t regvalue, msk;

	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;

	regvalue = *p_reg_value;

	msk = ((1<<(len+1))-1)<<pos;
	regvalue &= ~msk;
	regvalue |= bf_value<<pos;

	*p_reg_value = regvalue;

	return 0;
}

uint16_t tfa_get_bf_value(const uint16_t bf, const uint16_t reg_value)
{
	uint16_t msk, value;

	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;

	msk = ((1<<(len+1))-1)<<pos;
	value = (reg_value & msk) >> pos;

	return value;
}


int tfa_write_reg(struct tfa_device *tfa, const uint16_t bf, const uint16_t reg_value)
{
	enum Tfa98xx_Error err;

	/* bitfield enum - 8..15 : address */
	uint8_t address = (bf >> 8) & 0xff;

	err = reg_write(tfa, address, reg_value);
	if (err)
		return -err;

	return 0;
}

int tfa_read_reg(struct tfa_device *tfa, const uint16_t bf)
{
	enum Tfa98xx_Error err;
	uint16_t regvalue;

	/* bitfield enum - 8..15 : address */
	uint8_t address = (bf >> 8) & 0xff;

	err = reg_read(tfa, address, &regvalue);
	if (err)
		return -err;

	return regvalue;
}

/*
 * powerup the coolflux subsystem and wait for it
 */
enum Tfa98xx_Error tfa_cf_powerup(struct tfa_device *tfa)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	int tries, status;

	/* power on the sub system */
	TFA_SET_BF_VOLATILE(tfa, PWDN, 0);


	if (tfa->verbose)
		pr_info("Waiting for DSP system stable...\n");
	for ( tries=CFSTABLE_TRIES; tries > 0; tries-- ) {
		err = tfa98xx_dsp_system_stable(tfa, &status);
		_ASSERT(err == Tfa98xx_Error_Ok);
		if ( status )
			break;
		else
			msleep_interruptible(10); /* wait 10ms to avoid busload */
	}
	if (tries==0) {
		pr_err("DSP subsystem start timed out\n");
		return Tfa98xx_Error_StateTimedOut;
	}

	return err;
}

/*
 * Enable/Disable the I2S output for TFA1 devices
 * without TDM interface
 */
static enum Tfa98xx_Error tfa98xx_aec_output(struct tfa_device *tfa, int enable)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;

	if ((tfa->daimap & Tfa98xx_DAI_TDM) == Tfa98xx_DAI_TDM)
		return err;

	if (tfa->tfa_family == 1)
		err = -tfa_set_bf(tfa, TFA1_BF_I2SDOE, (enable!=0));
	else {
		pr_err("I2SDOE on unsupported family\n");
		err = Tfa98xx_Error_Not_Supported;
	}

	return err;
}

/*
 * Print the current state of the hardware manager
 * Device manager status information, man_state from TFA9888_N1B_I2C_regmap_V12
 */
enum Tfa98xx_Error show_current_state(struct tfa_device *tfa)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	int manstate = -1;

	if (tfa->tfa_family == 2 && tfa->verbose) {
		manstate = TFA_GET_BF(tfa, MANSTATE);
		if (manstate < 0)
			return -manstate;

		pr_debug("Current HW manager state: ");

		switch(manstate) {
			case 0: pr_debug("power_down_state \n");
				break;
			case 1: pr_debug("wait_for_source_settings_state \n");
				break;
			case 2: pr_debug("connnect_pll_input_state \n");
				break;
			case 3: pr_debug("disconnect_pll_input_state \n");
				break;
			case 4: pr_debug("enable_pll_state \n");
				break;
			case 5: pr_debug("enable_cgu_state \n");
				break;
			case 6: pr_debug("init_cf_state \n");
				break;
			case 7: pr_debug("enable_amplifier_state \n");
				break;
			case 8: pr_debug("alarm_state \n");
				break;
			case 9: pr_debug("operating_state \n");
				break;
			case 10: pr_debug("mute_audio_state \n");
				break;
			case 11: pr_debug("disable_cgu_pll_state \n");
				break;
			default:
				pr_debug("Unable to find current state \n");
				break;
		}
	}

	return err;
}

/*
 *  start the speakerboost algorithm
 *  this implies a full system startup when the system was not already started
 *
 */
enum Tfa98xx_Error tfaRunSpeakerBoost(struct tfa_device *tfa, int force, int profile)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	int value;

	if (force) {
		err= tfaRunColdStartup(tfa, profile);
		if ( err ) return err;
	}

	/* Returns 1 when device is "cold" and 0 when device is warm */
	value = tfa_is_cold(tfa);

	pr_debug("Startup of device [%s] is a %sstart\n", tfaContDeviceName(tfa->cnt, tfa->dev_idx), value ? "cold" : "warm");

	/* cold start and not tap profile */
	if (value) {
		/* Run startup and write all files */
		err = tfaRunSpeakerStartup(tfa, force, profile);
		if ( err ) return err;

		/* Save the current profile and set the vstep to 0 */
		/* This needs to be overwriten even in CF bypass */
		tfa_dev_set_swprof(tfa, (unsigned short)profile);
		tfa_dev_set_swvstep(tfa, 0);

		/* Synchonize I/V delay on 96/97 at cold start */
		if ((tfa->tfa_family == 1) && (tfa->daimap == Tfa98xx_DAI_TDM))
			tfa->sync_iv_delay = 1;
	}

	return err;
}

enum Tfa98xx_Error tfaRunSpeakerStartup(struct tfa_device *tfa, int force, int profile)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;

	if ( !force ) {
		err = tfaRunStartup(tfa, profile);
		PRINT_ASSERT(err);
		if ( err )
			return err;

		/* Startup with CF in bypass then return here */
		if (tfa_cf_enabled(tfa) == 0)
			return err;

		/* respond to external DSP: -1:none, 0:no_dsp, 1:cold, 2:warm */
		if(tfa->ext_dsp == -1) {
			err = tfaRunStartDSP(tfa);
			if ( err )
				return err;
		}
	}

	/* Set auto_copy_mtp_to_iic (bit 5 of A3) to 1 */
	tfa98xx_auto_copy_mtp_to_iic(tfa);

	/* write all the files from the device list */
	err = tfaContWriteFiles(tfa);
	if (err) {
		pr_debug("[%s] tfaContWriteFiles error = %d \n", __FUNCTION__, err);
		return err;
	}

	/* write all the files from the profile list (use volumstep 0) */
	err = tfaContWriteFilesProf(tfa, profile, 0);
	if (err) {
		pr_debug("[%s] tfaContWriteFilesProf error = %d \n", __FUNCTION__, err);
		return err;
	}

	return err;
}

/*
 * Run calibration
 */
enum Tfa98xx_Error tfaRunSpeakerCalibration(struct tfa_device *tfa)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	int calibrateDone;

	/* return if there is no audio running */
	if ((tfa->tfa_family == 2) && TFA_GET_BF(tfa, NOCLK))
		return Tfa98xx_Error_NoClock;

	/* When MTPOTC is set (cal=once) unlock key2 */
	if (TFA_GET_BF(tfa, MTPOTC) == 1) {
		tfa98xx_key2(tfa, 0);
	}

	/* await calibration, this should return ok */
	err = tfaRunWaitCalibration(tfa, &calibrateDone);
	if (err == Tfa98xx_Error_Ok) {
		err = tfa_dsp_get_calibration_impedance(tfa);
		PRINT_ASSERT(err);
	}

	/* When MTPOTC is set (cal=once) re-lock key2 */
	if (TFA_GET_BF(tfa, MTPOTC) == 1) {
		tfa98xx_key2(tfa, 1);
	}

	return err;
}

enum Tfa98xx_Error tfaRunColdboot(struct tfa_device *tfa, int state)
{
#define CF_CONTROL 0x8100
	enum Tfa98xx_Error err=Tfa98xx_Error_Ok;
	int tries = 10;

	/* repeat set ACS bit until set as requested */
	while ( state != TFA_GET_BF(tfa, ACS)) {
		/* set colstarted in CF_CONTROL to force ACS */
		err = mem_write(tfa, CF_CONTROL, state, Tfa98xx_DMEM_IOMEM);
		PRINT_ASSERT(err);

		if (tries-- == 0) {
			pr_debug("coldboot (ACS) did not %s\n", state ? "set":"clear");
			return Tfa98xx_Error_Other;
		}
	}

	return err;
}



/*
 * load the patch if any
 *   else tell no loaded
 */
static enum Tfa98xx_Error tfa_run_load_patch(struct tfa_device *tfa)
{
	return tfaContWritePatch(tfa);
}

/*
 *  this will load the patch witch will implicitly start the DSP
 *   if no patch is available the DPS is started immediately
 */
enum Tfa98xx_Error tfaRunStartDSP(struct tfa_device *tfa)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;

	err = tfa_run_load_patch(tfa);
	if (err) { /* patch load is fatal so return immediately*/
		return err;
	}

	/* Clear count_boot, should be reset to 0 before the DSP reset is released */
	err = mem_write(tfa, 512, 0, Tfa98xx_DMEM_XMEM);
	PRINT_ASSERT(err);

	/* Reset DSP once for sure after initializing */
	if ( err == Tfa98xx_Error_Ok) {
		err = tfa98xx_dsp_reset(tfa, 0);
		PRINT_ASSERT(err);
	}

	/* Sample rate is needed to set the correct tables */
	err = tfa98xx_dsp_write_tables(tfa, TFA_GET_BF(tfa, AUDFS));
	PRINT_ASSERT(err);

	return err;
}

/*
 * start the clocks and wait until the AMP is switching
 *  on return the DSP sub system will be ready for loading
 */
enum Tfa98xx_Error tfaRunStartup(struct tfa_device *tfa, int profile)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	nxpTfaDeviceList_t *dev = tfaContDevice(tfa->cnt, tfa->dev_idx);
	int i, noinit=0;

	if(dev == NULL)
		return Tfa98xx_Error_Fail;

	if(dev->bus) /* no i2c device, do nothing */
		return Tfa98xx_Error_Ok;

	/* process the device list to see if the user implemented the noinit */
	for(i=0;i<dev->length;i++) {
		if (dev->list[i].type == dscNoInit) {
			noinit=1;
			break;
		}
	}

	if(!noinit) {
		/* load the optimal TFA98XX in HW settings */
		err = tfa98xx_init(tfa);
		PRINT_ASSERT(err);
	} else {
		pr_debug("\nWarning: No init keyword found in the cnt file. Init is skipped! \n");
	}

	/* I2S settings to define the audio input properties
	 *  these must be set before the subsys is up */

	err = tfaContWriteRegsDev(tfa);
	PRINT_ASSERT(err);


	err = tfaContWriteRegsProf(tfa, profile);
	PRINT_ASSERT(err);

	/* Factory trimming for the Boost converter */
	tfa98xx_factory_trimmer(tfa);

	/* Go to the initCF state */
	tfa_dev_set_state(tfa, TFA_STATE_INIT_CF);

	err = show_current_state(tfa);

	return err;
}

/*
 * run the startup/init sequence and set ACS bit
 */
enum Tfa98xx_Error tfaRunColdStartup(struct tfa_device *tfa, int profile)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;

	err = tfaRunStartup(tfa, profile);
	PRINT_ASSERT(err);
	if (err)
		return err;

	if(!tfa->is_probus_device) {
		/* force cold boot */
		err = tfaRunColdboot(tfa, 1);
		PRINT_ASSERT(err);
		if (err)
			return err;
	}

	/* start */
	err = tfaRunStartDSP(tfa);
	PRINT_ASSERT(err);

	return err;
}

/*
 *
 */
enum Tfa98xx_Error tfaRunMute(struct tfa_device *tfa)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	int status;
	int tries = 0;

	/* signal the TFA98XX to mute  */
	if (tfa->tfa_family == 1) {
		err = tfa98xx_set_mute(tfa, Tfa98xx_Mute_Amplifier);

		if(err == Tfa98xx_Error_Ok) {
			/* now wait for the amplifier to turn off */
			do {
				status = TFA_GET_BF(tfa, SWS);
				if (status != 0)
					msleep_interruptible(10); /* wait 10ms to avoid busload */
				else
					break;
				tries++;
			}  while (tries < AMPOFFWAIT_TRIES);


			if (tfa->verbose)
				pr_debug("-------------------- muted --------------------\n");

			/*The amplifier is always switching*/
			if (tries == AMPOFFWAIT_TRIES)
				return Tfa98xx_Error_Other;
		}
	}

	return err;
}
/*
 *
 */
enum Tfa98xx_Error tfaRunUnmute(struct tfa_device *tfa)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;

	/* signal the TFA98XX to mute  */
	err = tfa98xx_set_mute(tfa, Tfa98xx_Mute_Off);

	if (tfa->verbose)
	    pr_debug("-------------------unmuted ------------------\n");

	return err;
}

static void individual_calibration_results(struct tfa_device *tfa)
{
	int value_P, value_S;

	/* Read the calibration result in xmem (529=primary channel) (530=secondary channel) */
	mem_read(tfa, 529, 1, &value_P);
	mem_read(tfa, 530, 1, &value_S);

	if(value_P != 1 && value_S != 1)
		pr_debug("Calibration failed on both channels! \n");
	else if(value_P != 1) {
		pr_debug("Calibration failed on Primary (Left) channel! \n");
		TFA_SET_BF_VOLATILE(tfa, SSLEFTE, 0); /* Disable the sound for the left speaker */
	}
	else if(value_S != 1) {
		pr_debug("Calibration failed on Secondary (Right) channel! \n");
		TFA_SET_BF_VOLATILE(tfa, SSRIGHTE, 0); /* Disable the sound for the right speaker */
	}

	TFA_SET_BF_VOLATILE(tfa, AMPINSEL, 0); /* Set amplifier input to TDM */
	TFA_SET_BF_VOLATILE(tfa, SBSL, 1);
}

/*
 * wait for calibrateDone
 */
enum Tfa98xx_Error tfaRunWaitCalibration(struct tfa_device *tfa, int *calibrateDone)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	int tries = 0, mtp_busy = 1, tries_mtp_busy = 0;

	*calibrateDone = 0;

	/* in case of calibrate once wait for MTPEX */
	if (TFA_GET_BF(tfa, MTPOTC)) {

		while (tries_mtp_busy < MTPBWAIT_TRIES)
		{
			mtp_busy = tfa_dev_get_mtpb(tfa);
			if (mtp_busy == 1)
				msleep_interruptible(10); /* wait 10ms to avoid busload */
			else
				break;
			tries_mtp_busy++;
		}

		if (tries_mtp_busy < MTPBWAIT_TRIES) {
			/* Because of the msleep TFA98XX_API_WAITRESULT_NTRIES is way to long!
				* Setting this to 25 will take it atleast 25*50ms = 1.25 sec
				*/
			while ((*calibrateDone == 0) && (tries < MTPEX_WAIT_NTRIES)) {
				*calibrateDone = TFA_GET_BF(tfa, MTPEX);
				if (*calibrateDone == 1)
					break;
				msleep_interruptible(50); /* wait 50ms to avoid busload */
				tries++;
			}

			if (tries >= MTPEX_WAIT_NTRIES) {
				tries = TFA98XX_API_WAITRESULT_NTRIES;
			}
		} else {
			pr_err("MTP bussy after %d tries\n", MTPBWAIT_TRIES);
		}
	}

	/* poll xmem for calibrate always
		* calibrateDone = 0 means "calibrating",
		* calibrateDone = -1 (or 0xFFFFFF) means "fails"
		* calibrateDone = 1 means calibration done
		*/
	while ((*calibrateDone != 1) && (tries<TFA98XX_API_WAITRESULT_NTRIES)) {
		err = mem_read(tfa, TFA_FW_XMEM_CALIBRATION_DONE, 1, calibrateDone);
		if (*calibrateDone == -1)
			break;
		tries++;
	}

	if (*calibrateDone != 1) {
		pr_err("Calibration failed! \n");
		err = Tfa98xx_Error_Bad_Parameter;
	} else if (tries==TFA98XX_API_WAITRESULT_NTRIES) {
		pr_debug("Calibration has timedout! \n");
		err = Tfa98xx_Error_StateTimedOut;
	} else if(tries_mtp_busy == 1000) {
		pr_err("Calibrate Failed: MTP_busy stays high! \n");
		err = Tfa98xx_Error_StateTimedOut;
	}

	/* Give reason why calibration failed! */
	if (err != Tfa98xx_Error_Ok) {
		if ((tfa->tfa_family == 2) && (TFA_GET_BF(tfa, REFCKSEL) == 1)) {
			pr_err("Unable to calibrate the device with the internal clock! \n");
		}
	}

	/* Check which speaker calibration failed. Only for 88C */
	if ((err != Tfa98xx_Error_Ok) && ((tfa->rev & 0x0FFF) == 0xc88)) {
		individual_calibration_results(tfa);
	}

	return err;
}

/*
 * tfa_dev_start will only do the basics: Going from powerdown to operating or a profile switch.
 * for calibrating or akoustic shock handling use the tfa98xxCalibration function.
 */
enum tfa_error tfa_dev_start(struct tfa_device *tfa, int next_profile, int vstep)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	int active_profile = -1;

	/* Get currentprofile */
	active_profile = tfa_dev_get_swprof(tfa);
	if (active_profile == 0xff)
		active_profile = -1;

	/* TfaRun_SpeakerBoost implies un-mute */
	pr_debug("Active_profile:%s, next_profile:%s\n",
			tfaContProfileName(tfa->cnt, tfa->dev_idx, active_profile),
			tfaContProfileName(tfa->cnt, tfa->dev_idx, next_profile));

	err = show_current_state(tfa);

	if ( tfa->tfa_family == 1 ) { /* TODO move this to ini file */
		/* Enable I2S output on TFA1 devices without TDM */
		err = tfa98xx_aec_output(tfa, 1);
		if ( err != Tfa98xx_Error_Ok)
			goto error_exit;
	}

	if ( tfa->bus != 0 )  { /* non i2c  */
#ifndef __KERNEL__
		tfadsp_fw_start(tfa, next_profile, vstep);
#endif /* __KERNEL__ */
	} else {
		/* Check if we need coldstart or ACS is set */
		err = tfaRunSpeakerBoost(tfa, 0, next_profile);
		if ( err != Tfa98xx_Error_Ok)
			goto error_exit;

		/* Make sure internal oscillator is running for DSP devices (non-dsp and max1 this is no-op) */
		tfa98xx_set_osc_powerdown(tfa, 0);

		/* Go to the Operating state */
		tfa_dev_set_state(tfa, TFA_STATE_OPERATING | TFA_STATE_MUTE);
		}
		active_profile = tfa_dev_get_swprof(tfa);

		/* Profile switching */
		if ((next_profile != active_profile && active_profile >= 0)) {
			err = tfaContWriteProfile(tfa, next_profile, vstep);
			if (err!=Tfa98xx_Error_Ok)
				goto error_exit;
		}

		/* If the profile contains the .standby suffix go to powerdown
		 * else we should be in operating state
		 */
		if(strstr(tfaContProfileName(tfa->cnt, tfa->dev_idx, next_profile), ".standby") != NULL) {
			tfa_dev_set_swprof(tfa, (unsigned short)next_profile);
			tfa_dev_set_swvstep(tfa, (unsigned short)tfa->vstep);
			goto error_exit;
		}

		err = show_current_state(tfa);

		/* get current vstep*/
		tfa->vstep = tfa_dev_get_swvstep(tfa);
		if (vstep != tfa->vstep && vstep != -1) {
			err = tfaContWriteFilesVstep(tfa, next_profile, vstep);
			if ( err != Tfa98xx_Error_Ok)
				goto error_exit;
		}

		/* Always search and apply filters after a startup */
		err = tfa_set_filters(tfa, next_profile);
		if (err!=Tfa98xx_Error_Ok)
			goto error_exit;

		tfa_dev_set_swprof(tfa, (unsigned short)next_profile);
		tfa_dev_set_swvstep(tfa, (unsigned short)tfa->vstep);

		/* PLMA5539: Gives information about current setting of powerswitch */
		if (tfa->verbose) {
			if (!tfa98xx_powerswitch_is_enabled(tfa))
				pr_info("Device start without powerswitch enabled!\n");
		}

error_exit:
	show_current_state(tfa);

	return err;
}

enum tfa_error tfa_dev_stop(struct tfa_device *tfa)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;

	/* mute */
	tfaRunMute(tfa);

	/* Make sure internal oscillator is not running for DSP devices (non-dsp and max1 this is no-op) */
	tfa98xx_set_osc_powerdown(tfa, 1);

	/* powerdown CF */
	err = tfa98xx_powerdown(tfa, 1 );
	if ( err != Tfa98xx_Error_Ok)
		return err;

	/* disable I2S output on TFA1 devices without TDM */
	err = tfa98xx_aec_output(tfa, 0);

	return err;
}

/*
 *  int registers and coldboot dsp
 */
int tfa_reset(struct tfa_device *tfa)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;

	err = -TFA_SET_BF_VOLATILE(tfa, I2CR, 1);
	if(err) return err;

	if (tfa->tfa_family == 2) {
		/* restore MANSCONF to POR state */
		TFA_SET_BF_VOLATILE(tfa, MANSCONF, 0);
	}

	if( !tfa->is_probus_device ) {
		if (tfa->tfa_family == 2) {
			/* restore MANCOLD to POR state */
			TFA_SET_BF_VOLATILE(tfa, MANCOLD, 1);
		}

		/* powerup CF to access CF io */
		tfa98xx_powerdown(tfa, 0 );
		/* for clock */
		err = tfa_cf_powerup(tfa);
		PRINT_ASSERT(err);

		/* force cold boot */
		err = tfaRunColdboot(tfa, 1);
		PRINT_ASSERT(err);

		/* reset all i2C registers to default */
		err = -TFA_SET_BF(tfa, I2CR, 1);
		PRINT_ASSERT(err);
	} else {
		if (tfa->ext_dsp > 0)
			tfa98xx_init_dsp(tfa);
	}

	return err;
}

/*
 * Write all the bytes specified by num_bytes and data
 */
enum Tfa98xx_Error
tfa98xx_write_data(struct tfa_device *tfa,
		  unsigned char subaddress, int num_bytes,
		  const unsigned char data[])
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	/* subaddress followed by data */
	const int bytes2write = num_bytes + 1;
	unsigned char *write_data;

	if (num_bytes > TFA2_MAX_PARAM_SIZE)
		return Tfa98xx_Error_Bad_Parameter;

	write_data = (unsigned char *)kmem_cache_alloc(tfa->cachep, GFP_KERNEL);
	if (write_data == NULL)
		return Tfa98xx_Error_Fail;

	write_data[0] = subaddress;
	memcpy(&write_data[1], data, num_bytes);

	error = tfa98xx_write_raw(tfa, bytes2write, write_data);

	kmem_cache_free(tfa->cachep, write_data);
	return error;
}

/*
 * fill the calibration value as milli ohms in the struct
 *
 *  assume that the device has been calibrated
 */
enum Tfa98xx_Error tfa_dsp_get_calibration_impedance(struct tfa_device *tfa)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	unsigned char bytes[3*2] = {0};
	int nr_bytes, i, data[2], calibrateDone, spkr_count=0, cal_idx=0;
	unsigned int scaled_data;
	int tries=0;

	error = tfa_supported_speakers(tfa, &spkr_count);

	if (tfa_dev_mtp_get(tfa, TFA_MTP_OTC)) {
		pr_debug("Getting calibration values from MTP\n");

		if((tfa->rev & 0xFF) == 0x88) {
			for(i=0; i<spkr_count; i++) {
				if(i==0)
					tfa->mohm[i] = tfa_dev_mtp_get(tfa, TFA_MTP_RE25_PRIM);
				else
					tfa->mohm[i] = tfa_dev_mtp_get(tfa, TFA_MTP_RE25_SEC);
			}
		} else {
			tfa->mohm[0] = tfa_dev_mtp_get(tfa, TFA_MTP_RE25);
		}
	} else {
		pr_debug("Getting calibration values from Speakerboost\n");

		/* Make sure the calibrateDone bit is set before getting the values from speakerboost!
		 * This does not work for 72 (because the dsp cannot set this bit)
		 */
		if (!tfa->is_probus_device) {
			/* poll xmem for calibrate always
				* calibrateDone = 0 means "calibrating",
				* calibrateDone = -1 (or 0xFFFFFF) means "fails"
				* calibrateDone = 1 means calibration done
				*/
			calibrateDone = 0;
			while ((calibrateDone != 1) && (tries < TFA98XX_API_WAITRESULT_NTRIES)) {
				error = mem_read(tfa, TFA_FW_XMEM_CALIBRATION_DONE, 1, &calibrateDone);
				if (calibrateDone == 1)
					break;
				tries++;
			}

			if (calibrateDone != 1) {
				pr_err("Calibration failed! \n");
				error = Tfa98xx_Error_Bad_Parameter;
			}
			else if (tries == TFA98XX_API_WAITRESULT_NTRIES) {
				pr_debug("Calibration has timedout! \n");
				error = Tfa98xx_Error_StateTimedOut;
			}
		}
		/* SoftDSP interface differs from hw-dsp interfaces */
		if(tfa->is_probus_device && tfa->cnt->ndev > 1) {
			spkr_count = tfa->cnt->ndev;
		}

		nr_bytes = spkr_count * 3;
		error = tfa_dsp_cmd_id_write_read(tfa, MODULE_SPEAKERBOOST, SB_PARAM_GET_RE25C, nr_bytes, bytes);
		if (error == Tfa98xx_Error_Ok) {
			tfa98xx_convert_bytes2data(nr_bytes, bytes, data);

			for(i=0; i<spkr_count; i++) {

				/* for probus devices, calibration values coming from soft-dsp speakerboost,
				   are ordered in a different way. Re-align to standard representation. */
				cal_idx=i;
				if((tfa->is_probus_device && tfa->dev_idx >= 1) ) {
					cal_idx=0;
				}

				/* signed data has a limit of 30 Ohm */
				scaled_data = data[i];

				if(tfa->tfa_family == 2)
					tfa->mohm[cal_idx] = (scaled_data*1000)/TFA2_FW_ReZ_SCALE;
				else
					tfa->mohm[cal_idx] = (scaled_data*1000)/TFA1_FW_ReZ_SCALE;
			}
		}
	}

	return error;
}

/* start count from 1, 0 is invalid */
int tfa_dev_get_swprof(struct tfa_device *tfa)
{
	return (tfa->dev_ops.get_swprof)(tfa);
}

int tfa_dev_set_swprof(struct tfa_device *tfa, unsigned short new_value)
{
	return (tfa->dev_ops.set_swprof)(tfa, new_value+1);
}

/*   same value for all channels
 * start count from 1, 0 is invalid */
int tfa_dev_get_swvstep(struct tfa_device *tfa)
{
	return (tfa->dev_ops.get_swvstep)(tfa);
}

int tfa_dev_set_swvstep(struct tfa_device *tfa, unsigned short new_value)
{
	return (tfa->dev_ops.set_swvstep)(tfa, new_value + 1);
}

/*
	function overload for MTPB
 */
int tfa_dev_get_mtpb(struct tfa_device *tfa)
{
	return (tfa->dev_ops.get_mtpb)(tfa);
}

int tfa_is_cold(struct tfa_device *tfa)
{
	int value;

	/*
	 * check for cold boot status
	 */
	if (tfa->is_probus_device) {
		if (tfa->ext_dsp > 0) {
			if (tfa->ext_dsp == 2)
				value = 0;
			else /* no dsp or cold */
				value = 1;
		} else {
			value = (TFA_GET_BF(tfa, MANSCONF) == 0);
		}
	} else {
		value = TFA_GET_BF(tfa, ACS);
	}

	return value;
}

int tfa_needs_reset(struct tfa_device *tfa)
{
	int value;

	/* checks if the DSP commands SetAlgoParams and SetMBDrc
	 * need a DSP reset (now: at coldstart or during calibration)
	 */
	if (tfa_is_cold(tfa) == 1 || tfa->needs_reset == 1)
		value = 1;
	else
		value = 0;

	return value;
}

int tfa_cf_enabled(struct tfa_device *tfa)
{
	int value;

	/* For 72 there is no CF */
	if(tfa->is_probus_device) {
		value = (tfa->ext_dsp != 0);
	} else {
		value = TFA_GET_BF(tfa, CFE);
	}

	return value;
}

#define NR_COEFFS 6
#define NR_BIQUADS 28
#define BQ_SIZE (3 * NR_COEFFS)
#define DSP_MSG_OVERHEAD 27

#pragma pack (push, 1)
struct dsp_msg_all_coeff {
	uint8_t select_eq[3];
	uint8_t biquad[NR_BIQUADS][NR_COEFFS][3];
};
#pragma pack (pop)

/* number of biquads for each equalizer */
static const int eq_biquads[] = {
	10, 10, 2, 2, 2, 2
};

#define NR_EQ (int)(sizeof(eq_biquads) / sizeof(int))

enum Tfa98xx_Error dsp_partial_coefficients(struct tfa_device *tfa, uint8_t *prev, uint8_t *next)
{
	uint8_t bq, eq;
	int eq_offset;
	int new_cost, old_cost;
	uint32_t eq_biquad_mask[NR_EQ];
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	struct dsp_msg_all_coeff *data1 = (struct dsp_msg_all_coeff *)prev;
	struct dsp_msg_all_coeff *data2 = (struct dsp_msg_all_coeff *)next;

	old_cost = DSP_MSG_OVERHEAD + 3 + sizeof(struct dsp_msg_all_coeff);
	new_cost = 0;

	eq_offset = 0;
	for (eq=0; eq<NR_EQ; eq++) {
		uint8_t *eq1 = &data1->biquad[eq_offset][0][0];
		uint8_t *eq2 = &data2->biquad[eq_offset][0][0];

		eq_biquad_mask[eq] = 0;

		if (memcmp(eq1, eq2, BQ_SIZE*eq_biquads[eq]) != 0) {
			int nr_bq = 0;
			int bq_sz, eq_sz;

			for (bq=0; bq < eq_biquads[eq]; bq++) {
				uint8_t *bq1 = &eq1[bq*BQ_SIZE];
				uint8_t *bq2 = &eq2[bq*BQ_SIZE];

				if (memcmp(bq1, bq2, BQ_SIZE) != 0) {
					eq_biquad_mask[eq] |= (1<<bq);
					nr_bq++;
				}
			}

			bq_sz = (2 * 3 + BQ_SIZE) * nr_bq;
			eq_sz = 2 * 3 + BQ_SIZE * eq_biquads[eq];

			/* dsp message i2c transaction overhead */
			bq_sz += DSP_MSG_OVERHEAD * nr_bq;
			eq_sz += DSP_MSG_OVERHEAD;

			if (bq_sz >= eq_sz) {
				eq_biquad_mask[eq] = 0xffffffff;

				new_cost += eq_sz;

			} else {
				new_cost += bq_sz;
			}
		}
		pr_debug("eq_biquad_mask[%d] = 0x%.8x\n", eq, eq_biquad_mask[eq]);

		eq_offset += eq_biquads[eq];
	}

	pr_debug("cost for writing all coefficients     = %d\n", old_cost);
	pr_debug("cost for writing changed coefficients = %d\n", new_cost);

	if (new_cost >= old_cost) {
		const int buffer_sz = 3 + sizeof(struct dsp_msg_all_coeff);
		uint8_t *buffer;

		buffer = kmalloc(buffer_sz, GFP_KERNEL);
		if (buffer == NULL)
			return Tfa98xx_Error_Fail;

		/* cmd id */
		buffer[0] = 0x00;
		buffer[1] = 0x82;
		buffer[2] = 0x00;

		/* parameters */
		memcpy(&buffer[3], data2, sizeof(struct dsp_msg_all_coeff));

		err = dsp_msg(tfa, buffer_sz, (const char *)buffer);

		kfree(buffer);
		if (err)
			return err;

	} else {
		eq_offset = 0;
		for (eq=0; eq<NR_EQ; eq++) {
			uint8_t *eq2 = &data2->biquad[eq_offset][0][0];

			if (eq_biquad_mask[eq] == 0xffffffff) {
				const int msg_sz = 6 + BQ_SIZE * eq_biquads[eq];
				uint8_t *msg;

				msg = kmalloc(msg_sz, GFP_KERNEL);
				if (msg == NULL)
					return Tfa98xx_Error_Fail;

				/* cmd id */
				msg[0] = 0x00;
				msg[1] = 0x82;
				msg[2] = 0x00;

				/* select eq and bq */
				msg[3] = 0x00;
				msg[4] = eq+1;
				msg[5] = 0x00; /* all biquads */

				/* biquad parameters */
				memcpy(&msg[6], eq2, BQ_SIZE * eq_biquads[eq]);

				err = dsp_msg(tfa, msg_sz, (const char *)msg);

				kfree(msg);
				if (err)
					return err;

			} else if (eq_biquad_mask[eq] != 0) {
				for(bq=0; bq < eq_biquads[eq]; bq++) {

					if (eq_biquad_mask[eq] & (1<<bq)) {
						uint8_t *bq2 = &eq2[bq*BQ_SIZE];
						const int msg_sz = 6 + BQ_SIZE;
						uint8_t *msg;

						msg = kmem_cache_alloc(tfa->cachep, GFP_KERNEL);
						if (msg == NULL)
							return Tfa98xx_Error_Fail;

						/* cmd id */
						msg[0] = 0x00;
						msg[1] = 0x82;
						msg[2] = 0x00;

						/* select eq and bq*/
						msg[3] = 0x00;
						msg[4] = eq+1;
						msg[5] = bq+1;

						/* biquad parameters */
						memcpy(&msg[6], bq2, BQ_SIZE);

						err = dsp_msg(tfa, msg_sz, (const char *)msg);

						kmem_cache_free(tfa->cachep, msg);
						if (err)
							return err;
					}
				}
			}
			eq_offset += eq_biquads[eq];
		}
	}

	return err;
}

/* fill context info */
int tfa_dev_probe(int slave, struct tfa_device *tfa)
{
	uint16_t rev;

	tfa->slave_address = (unsigned char)slave;

	/* read revid via low level hal, register 3 */
	if (tfa98xx_read_register16(tfa, 3, &rev) != Tfa98xx_Error_Ok) {
		PRINT("\nError: Unable to read revid from slave:0x%02x \n", slave);
		return -1;
	}

	tfa->rev = rev;
	tfa->dev_idx = -1;
	tfa->state = TFA_STATE_UNKNOWN;
	tfa->p_regInfo = NULL;

	tfa_set_query_info(tfa);

	tfa->in_use = 1;

	return 0;
}

enum tfa_error tfa_dev_set_state(struct tfa_device *tfa, enum tfa_state state)
{
	enum tfa_error err = tfa_error_ok;
	int loop = 50, ready = 0;
	int count;

	/* Base states */
	/* Do not change the order of setting bits as this is important! */
	switch (state & 0x0f) {
	case TFA_STATE_POWERDOWN:    /* PLL in powerdown, Algo up */
		break;
	case TFA_STATE_INIT_HW:      /* load I2C/PLL hardware setting (~wait2srcsettings) */
		break;
	case TFA_STATE_INIT_CF:      /* coolflux HW access possible (~initcf) */
								 /* Start with SBSL=0 to stay in initCF state */
		TFA_SET_BF(tfa, SBSL, 0);

		/* We want to leave Wait4SrcSettings state for max2 */
		if (tfa->tfa_family == 2)
			TFA_SET_BF(tfa, MANSCONF, 1);

		/* And finally set PWDN to 0 to leave powerdown state */
		TFA_SET_BF(tfa, PWDN, 0);

		/* Make sure the DSP is running! */
		do {
			err = tfa98xx_dsp_system_stable(tfa, &ready);
			if (err != tfa_error_ok)
				return err;
			if (ready)
				break;
		} while (loop--);
		/* Enable FAIM when clock is stable, to avoid MTP corruption */
		err = tfa98xx_faim_protect(tfa, 1);
		if (tfa->verbose) {
			pr_debug("FAIM enabled (err:%d).\n", err);
		}
		break;
	case TFA_STATE_INIT_FW:      /* DSP framework active (~patch loaded) */
		break;
	case TFA_STATE_OPERATING:    /* Amp and Algo running */
								 /* Depending on our previous state we need to set 3 bits */
		TFA_SET_BF(tfa, PWDN, 0);	/* Coming from state 0 */
		TFA_SET_BF(tfa, MANSCONF, 1);	/* Coming from state 1 */
		TFA_SET_BF(tfa, SBSL, 1);	/* Coming from state 6 */

									/*
									* Disable MTP clock to protect memory.
									* However in case of calibration wait for DSP! (This should be case only during calibration).
									*/
		if (TFA_GET_BF(tfa, MTPOTC) == 1) {
			count = MTPEX_WAIT_NTRIES * 4; /* Calibration takes a lot of time */
			while ((TFA_GET_BF(tfa, MTPEX) != 1) && count) {
				msleep_interruptible(10);
				count--;
			}
		}
		err = tfa98xx_faim_protect(tfa, 0);
		if (tfa->verbose) {
			pr_debug("FAIM disabled (err:%d).\n", err);
		}

		/* Synchonize I/V delay on 96/97 at cold start */
		if (tfa->sync_iv_delay) {
			if (tfa->verbose)
				pr_debug("syncing I/V delay for %x\n",
				(tfa->rev & 0xff));

			/* wait for ACS to be cleared */
			count = 10;
			while ((TFA_GET_BF(tfa, ACS) == 1) &&
				(count-- > 0)) {
				msleep_interruptible(1);
			}

			tfa98xx_dsp_reset(tfa, 1);
			tfa98xx_dsp_reset(tfa, 0);
			tfa->sync_iv_delay = 0;
		}
		break;
	case TFA_STATE_FAULT:        /* An alarm or error occurred */
		break;
	case TFA_STATE_RESET:        /* I2C reset and ACS set */
		tfa98xx_init(tfa);
		break;
	default:
		if (state & 0x0f)
			return tfa_error_bad_param;
	}

	/* state modifiers */

	if (state & TFA_STATE_MUTE)
		tfa98xx_set_mute(tfa, Tfa98xx_Mute_Amplifier);

	if (state & TFA_STATE_UNMUTE)
		tfa98xx_set_mute(tfa, Tfa98xx_Mute_Off);

	tfa->state = state;

	return tfa_error_ok;
}

enum tfa_state tfa_dev_get_state(struct tfa_device *tfa)
{
	int cold = TFA_GET_BF(tfa, ACS);
	int manstate;

	/* different per family type */
	if ( tfa->tfa_family == 1 ) {
		if (  cold && TFA_GET_BF(tfa, PWDN) )
			tfa->state = TFA_STATE_RESET;
		else if ( !cold && TFA_GET_BF(tfa, SWS))
			tfa->state = TFA_STATE_OPERATING;
	} else /* family 2 */ {
		manstate = TFA_GET_BF(tfa, MANSTATE);
		switch(manstate) {
			case 0:
				tfa->state = cold ?  TFA_STATE_RESET : TFA_STATE_POWERDOWN;
				break;
			case 8: /* if dsp reset if off assume framework is running */
				tfa->state = TFA_GET_BF(tfa, RST) ? TFA_STATE_INIT_CF : TFA_STATE_INIT_FW;
				break;
			case 9:
				tfa->state = TFA_STATE_OPERATING;
				break;
			default:
				break;
		}
	}

	return tfa->state;
}

int tfa_dev_mtp_get(struct tfa_device *tfa, enum tfa_mtp item)
{
	int value = 0;

	switch (item) {
		case TFA_MTP_OTC:
			value = TFA_GET_BF(tfa, MTPOTC);
			break;
		case TFA_MTP_EX:
			value = TFA_GET_BF(tfa, MTPEX);
			break;
		case TFA_MTP_RE25:
		case TFA_MTP_RE25_PRIM:
			if(tfa->tfa_family == 2) {
				if((tfa->rev & 0xFF) == 0x88)
					value = TFA_GET_BF(tfa, R25CL);
				else if((tfa->rev & 0xFF) == 0x13)
					value = tfa_get_bf(tfa, TFA9912_BF_R25C);
				else
					value = TFA_GET_BF(tfa, R25C);
			} else {
				reg_read(tfa, 0x83, (unsigned short*)&value);
			}
			break;
		case TFA_MTP_RE25_SEC:
			if((tfa->rev & 0xFF) == 0x88) {
				value = TFA_GET_BF(tfa, R25CR);
			} else {
				pr_debug("Error: Current device has no secondary Re25 channel \n");
			}
			break;
		case TFA_MTP_LOCK:
			break;
	}

	return value;
}

enum tfa_error tfa_dev_mtp_set(struct tfa_device *tfa, enum tfa_mtp item, int value)
{
	enum tfa_error err = tfa_error_ok;

	switch (item) {
		case TFA_MTP_OTC:
			err = tfa98xx_set_mtp(tfa, (uint16_t)value, TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_MSK);
			break;
		case TFA_MTP_EX:
			err = tfa98xx_set_mtp(tfa, (uint16_t)value, TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_MSK);
			break;
		case TFA_MTP_RE25:
		case TFA_MTP_RE25_PRIM:
			if(tfa->tfa_family == 2) {
				if((tfa->rev & 0xFF) == 0x88)
					TFA_SET_BF(tfa, R25CL, (uint16_t)value);
				else
					TFA_SET_BF(tfa, R25C, (uint16_t)value);
			}
			break;
		case TFA_MTP_RE25_SEC:
			if((tfa->rev & 0xFF) == 0x88) {
				TFA_SET_BF(tfa, R25CR, (uint16_t)value);
			} else {
				pr_debug("Error: Current device has no secondary Re25 channel \n");
				err = tfa_error_bad_param;
			}
			break;
		case TFA_MTP_LOCK:
			break;
	}

	return err;
}

int tfa_get_pga_gain(struct tfa_device *tfa)
{
	return TFA_GET_BF(tfa, SAAMGAIN);
}

int tfa_set_pga_gain(struct tfa_device *tfa, uint16_t value)
{

	return TFA_SET_BF(tfa, SAAMGAIN, value);
}

int tfa_get_noclk(struct tfa_device *tfa)
{
	return TFA_GET_BF(tfa, NOCLK);
}


enum Tfa98xx_Error tfa_status(struct tfa_device *tfa)
{
	int value;
	uint16_t val;

	/*
	 * check IC status bits: cold start
	 * and DSP watch dog bit to re init
	 */
	value = TFA_READ_REG(tfa, VDDS); /* STATUSREG */
	if (value < 0)
		return -value;
	val = (uint16_t)value;

	/* pr_debug("SYS_STATUS0: 0x%04x\n", val); */
	if (TFA_GET_BF_VALUE(tfa, ACS, val) ||
	    TFA_GET_BF_VALUE(tfa, WDS, val)) {

		if (TFA_GET_BF_VALUE(tfa, ACS, val))
			pr_err("ERROR: ACS\n");
		if (TFA_GET_BF_VALUE(tfa, WDS, val))
			pr_err("ERROR: WDS\n");

		return Tfa98xx_Error_DSP_not_running;
	}

	if (TFA_GET_BF_VALUE(tfa, SPKS, val))
		pr_err("ERROR: SPKS\n");
	if (!TFA_GET_BF_VALUE(tfa, SWS, val))
		pr_err("ERROR: SWS\n");

	/* Check secondary errors */
	if (!TFA_GET_BF_VALUE(tfa, CLKS, val) ||
	    !TFA_GET_BF_VALUE(tfa, UVDS, val) ||
	    !TFA_GET_BF_VALUE(tfa, OVDS, val) ||
	    !TFA_GET_BF_VALUE(tfa, OTDS, val) ||
	    !TFA_GET_BF_VALUE(tfa, PLLS, val) ||
		(!(tfa->daimap & Tfa98xx_DAI_TDM) &&
		 !TFA_GET_BF_VALUE(tfa, VDDS, val)) )
		pr_err("Misc errors detected: STATUS_FLAG0 = 0x%x\n", val);

	if ((tfa->daimap & Tfa98xx_DAI_TDM) && (tfa->tfa_family == 2)) {
		value = TFA_READ_REG(tfa, TDMERR); /* STATUS_FLAGS1 */
		if (value < 0)
			return -value;
		val = (uint16_t)value;
		if (TFA_GET_BF_VALUE(tfa, TDMERR, val) ||
		    TFA_GET_BF_VALUE(tfa, TDMLUTER, val))
			pr_err("TDM related errors: STATUS_FLAG1 = 0x%x\n", val);
	}

	return Tfa98xx_Error_Ok;
}

int tfa_plop_noise_interrupt(struct tfa_device *tfa, int profile, int vstep)
{
	enum Tfa98xx_Error err;
	int no_clk=0;

	/* Remove sticky bit by reading it once */
	TFA_GET_BF(tfa, NOCLK);

	/* No clock detected */
	if (tfa_irq_get(tfa, tfa9912_irq_stnoclk)) {
		no_clk = TFA_GET_BF(tfa, NOCLK);

		/* Detect for clock is lost! (clock is not stable) */
		if (no_clk == 1) {
			/* Clock is lost. Set I2CR to remove POP noise */
			pr_info("No clock detected. Resetting the I2CR to avoid pop on 72! \n");
			err = tfa_dev_start(tfa, profile, vstep);
			if (err != Tfa98xx_Error_Ok) {
				pr_err("Error loading i2c registers (tfa_dev_start), err=%d\n", err);
			} else {
				pr_info("Setting i2c registers after I2CR succesfull\n");
				tfa_dev_set_state(tfa, TFA_STATE_UNMUTE);
			}

			/* Remove sticky bit by reading it once */
			tfa_get_noclk(tfa);

			/* This is only for SAAM on the 72.
			   Since the NOCLK interrupt is only enabled for 72 this is the place
			   However: Not tested yet! But also does not harm normal flow!
			*/
			if (strstr(tfaContProfileName(tfa->cnt, tfa->dev_idx, profile), ".saam")) {
				pr_info("Powering down from a SAAM profile, workaround PLMA4766 used! \n");
				TFA_SET_BF(tfa, PWDN, 1);
				TFA_SET_BF(tfa, AMPE, 0);
				TFA_SET_BF(tfa, SAMMODE, 0);
			}
		}

		/* If clk is stable set polarity to check for LOW (no clock)*/
		tfa_irq_set_pol(tfa, tfa9912_irq_stnoclk, (no_clk == 0));

		/* clear interrupt */
		tfa_irq_clear(tfa, tfa9912_irq_stnoclk);
	}

	/* return no_clk to know we called tfa_dev_start */
	return no_clk;
}

void tfa_lp_mode_interrupt(struct tfa_device *tfa)
{
	const int irq_stclp0 = 36; /* FIXME: this 72 interrupt does not excist for 9912 */
	int lp0, lp1;

	if (tfa_irq_get(tfa, irq_stclp0)) {
		lp0 = TFA_GET_BF(tfa, LP0);
		if (lp0 > 0) {
			pr_info("lowpower mode 0 detected\n");
		} else {
			pr_info("lowpower mode 0 not detected\n");
		}

		tfa_irq_set_pol(tfa, irq_stclp0, (lp0 == 0));

		/* clear interrupt */
		tfa_irq_clear(tfa, irq_stclp0);
	}

	if (tfa_irq_get(tfa, tfa9912_irq_stclpr)) {
		lp1 = TFA_GET_BF(tfa, LP1);
		if (lp1 > 0) {
			pr_info("lowpower mode 1 detected\n");
		} else {
			pr_info("lowpower mode 1 not detected\n");
		}

		tfa_irq_set_pol(tfa, tfa9912_irq_stclpr, (lp1 == 0));

		/* clear interrupt */
		tfa_irq_clear(tfa, tfa9912_irq_stclpr);
	}
}
