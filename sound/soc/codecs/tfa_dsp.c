/*
 * Copyright (C) NXP Semiconductors (PLMA)
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "%s(%s): " fmt, __func__, tfa98xx->fw.name
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/crc32.h>

#include "tfa98xx-core.h"
#include "tfa98xx-regs.h"
#include "tfa_container.h"
#include "tfa_dsp.h"



/* size of the data buffer used for I2C transfer */
#define TFA98XX_MAX_I2C_SIZE	252


#define TFA98XX_XMEM_CALIBRATION_DONE 231 /* 0xe7 */
#define TFA98XX_XMEM_IMPEDANCE        232
#define TFA98XX_XMEM_COUNT_BOOT       161 /* 0xa1 */

/*
 * Maximum number of retries for DSP result
 * Keep this value low!
 * If certain calls require longer wait conditions, the
 * application should poll, not the API
 * The total wait time depends on device settings. Those
 * are application specific.
 */
#define TFA98XX_WAITRESULT_NTRIES	50
#define TFA98XX_WAITRESULT_NTRIES_WAIT	200
#define TFA98XX_WAITRESULT_NTRIES_LONG	2000


/* DSP module IDs */
#define MODULE_FRAMEWORK        0
#define MODULE_SPEAKERBOOST     1
#define MODULE_BIQUADFILTERBANK 2
#define MODULE_SETRE		9


/* RPC commands IDs */
/* Load a full model into SpeakerBoost. */
#define SB_PARAM_SET_LSMODEL	0x06
#define SB_PARAM_SET_EQ		0x0A /* 2 Equaliser Filters */
#define SB_PARAM_SET_PRESET	0x0D /* Load a preset */
#define SB_PARAM_SET_CONFIG	0x0E /* Load a config */
#define SB_PARAM_SET_DRC	0x0F
#define SB_PARAM_SET_AGCINS	0x10


/* gets the speaker calibration impedance (@25 degrees celsius) */
#define SB_PARAM_GET_RE0		0x85
#define SB_PARAM_GET_LSMODEL		0x86 /* Gets LoudSpeaker Model */
#define SB_PARAM_GET_CONFIG_PRESET	0x80
#define SB_PARAM_GET_STATE		0xC0
#define SB_PARAM_GET_XMODEL		0xC1 /* Gets Excursion Model */

#define SPKRBST_TEMPERATURE_EXP		9

/* Framework params */
#define FW_PARAM_SET_CURRENT_DELAY	0x03
#define FW_PARAM_SET_CURFRAC_DELAY	0x06
#define FW_PARAM_GET_STATE		0x84
#define FW_PARAM_GET_FEATURE_BITS	0x85


/*
 * write a bit field
 */
int tfaRunWriteBitfield(struct tfa98xx *tfa98xx,  struct nxpTfaBitfield bf)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 value, oldvalue, msk, tmp;
	union {
		u16 field;
		struct nxpTfaBfEnum Enum;
	} bfUni;

	value = bf.value;
	bfUni.field = bf.field;
	/* bfUni.field  &= 0x7fff; */ /* mask of high bit, done before */

	pr_debug("bitfield: %s=%d (0x%x[%d..%d]=0x%x)\n",
		 tfaContBfName(bfUni.field), value, bfUni.Enum.address,
		 bfUni.Enum.pos, bfUni.Enum.pos+bfUni.Enum.len, value);
	if (((struct nxpTfaBfEnum *)&bf.field)->address & 0x80)
		pr_err("WARNING:not a persistant write of MTP\n");

	oldvalue = (u16)snd_soc_read(codec, bfUni.Enum.address);
	tmp = oldvalue;

	msk = ((1 << (bfUni.Enum.len + 1)) - 1) << bfUni.Enum.pos;
	oldvalue &= ~msk;
	oldvalue |= value << bfUni.Enum.pos;
	pr_debug("bitfield: %s=%d (0x%x -> 0x%x)\n", tfaContBfName(bfUni.field),
		 value, tmp, oldvalue);
	snd_soc_write(codec, bfUni.Enum.address, oldvalue);

	return 0;
}

/*
 * write the register based on the input address, value and mask
 * only the part that is masked will be updated
 */
int tfaRunWriteRegister(struct tfa98xx *tfa98xx, struct nxpTfaRegpatch *reg)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 value, newvalue;

	pr_debug("register: 0x%02x=0x%04x (msk=0x%04x)\n", reg->address,
		 reg->value, reg->mask);

	value = (u16)snd_soc_read(codec, reg->address);
	value &= ~reg->mask;
	newvalue = reg->value & reg->mask;
	value |= newvalue;
	snd_soc_write(codec, reg->address, value);

	return 0;
}

/*
 * tfa98xx_dsp_system_stable will compensate for the wrong behavior of CLKS
 * to determine if the DSP subsystem is ready for patch and config loading.
 *
 * A MTP calibration register is checked for non-zero.
 *
 * Note: This only works after i2c reset as this will clear the MTP contents.
 * When we are configured then the DSP communication will synchronize access.
 */
int tfa98xx_dsp_system_stable(struct tfa98xx *tfa98xx, int *ready)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	int ret = 0;
	u16 status, mtp0, sysctrl;
	int tries;

	*ready = 0;

	/* check the contents of the STATUS register */
	status = (u16)snd_soc_read(codec, TFA98XX_STATUSREG);
	sysctrl = (u16)snd_soc_read(codec, TFA98XX_SYS_CTRL);

	pr_debug("statusreg = 0x%04x, sysctrl=0x%04x\n", status, sysctrl);

	/*
	 * if AMPS is set then we were already configured and running
	 * no need to check further
	 */
	*ready = (status & TFA98XX_STATUSREG_AMPS_MSK) ==
		 (TFA98XX_STATUSREG_AMPS_MSK);

	pr_debug("AMPS %d\n", *ready);
	if (*ready)
		return 0;

	/* check AREFS and CLKS: not ready if either is clear */
	*ready = (status &
		  (TFA98XX_STATUSREG_AREFS_MSK | TFA98XX_STATUSREG_CLKS_MSK)) ==
		  (TFA98XX_STATUSREG_AREFS_MSK | TFA98XX_STATUSREG_CLKS_MSK);
	pr_debug("AREFS | CLKS %d\n", *ready);
	if (!*ready)		/* if not ready go back */
		return 0;

	if (tfa98xx->rev != REV_TFA9890) {
		*ready = 1;
		return 0;
	}

	/*
	 * check MTPB
	 *   mtpbusy will be active when the subsys copies MTP to I2C
	 *   2 times retry avoids catching this short mtpbusy active period
	 */
	for (tries = 2; tries > 0; tries--) {
		status = (u16)snd_soc_read(codec, TFA98XX_STATUSREG);
		/* check the contents of the STATUS register */
		*ready = (status & TFA98XX_STATUSREG_MTPB_MSK) == 0;
		if (*ready)	/* if ready go on */
			break;
	}
	pr_debug("MTPB %d\n", *ready);
	if (tries == 0) { /* ready will be 0 if retries exausted */
		pr_debug("Not ready %d\n", !*ready);
		return 0;
	}

	/*
	 * check the contents of  MTP register for non-zero,
	 * this indicates that the subsys is ready
	 */
	mtp0 = (u16)snd_soc_read(codec, 0x84);

	*ready = (mtp0 != 0);	/* The MTP register written? */
	pr_debug("MTP0 %d\n", *ready);

	return ret;
}

/*
 * Disable clock gating
 */
static int tfa98xx_clockgating(struct tfa98xx *tfa98xx, int on)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 value;

	pr_debug("%d\n", on);

	/* The clockgating hack is used only for the tfa9890 */
	if (tfa98xx->rev != REV_TFA9890)
		return 0;

	/* TFA9890: temporarily disable clock gating when dsp reset is used */
	value = snd_soc_read(codec, TFA98XX_CURRENTSENSE4);

	if (on)	/* clock gating on - clear the bit */
		value &= ~TFA98XX_CURRENTSENSE4_CTRL_CLKGATECFOFF;
	else	/* clock gating off - set the bit */
		value |= TFA98XX_CURRENTSENSE4_CTRL_CLKGATECFOFF;

	return snd_soc_write(codec, TFA98XX_CURRENTSENSE4, value);
}

/*
 * tfa98xx_dsp_reset will deal with clock gating control in order
 * to reset the DSP for warm state restart
 */
static int tfa98xx_dsp_reset(struct tfa98xx *tfa98xx, int state)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 value;

	pr_debug("\n");

	/* TFA9890: temporarily disable clock gating when dsp reset is used */
	tfa98xx_clockgating(tfa98xx, 0);

	value = snd_soc_read(codec, TFA98XX_CF_CONTROLS);

	/* set requested the DSP reset signal state */
	value = state ? (value | TFA98XX_CF_CONTROLS_RST_MSK) :
			(value & ~TFA98XX_CF_CONTROLS_RST_MSK);

	snd_soc_write(codec, TFA98XX_CF_CONTROLS, value);

	/* clock gating restore */
	return tfa98xx_clockgating(tfa98xx, 1);
}

int tfa98xx_powerdown(struct tfa98xx *tfa98xx, int powerdown)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 value;

	pr_debug("%d\n", powerdown);

	/* read the SystemControl register, modify the bit and write again */
	value = snd_soc_read(codec, TFA98XX_SYS_CTRL);

	switch (powerdown) {
	case 1:
		value |= TFA98XX_SYS_CTRL_PWDN_MSK;
		break;
	case 0:
		value &= ~(TFA98XX_SYS_CTRL_PWDN_MSK);
		break;
	default:
		return -EINVAL;
	}

	return snd_soc_write(codec, TFA98XX_SYS_CTRL, value);
}


static int tfa98xx_read_data(struct tfa98xx *tfa98xx, u8 address, int len,
			     u8 *data)
{
	/* pr_debug("@%02x, #%d\n", address, len); */

	if (tfa98xx_i2c_read(tfa98xx->i2c, address, data, len)) {
		pr_err("Error during I2C read\n");
		return -EIO;
	}

	return 0;
}

void tfa98xx_convert_data2bytes(int num_data, const int *data, u8 *bytes)
{
	int i, k, d;
	/*
	 * note: cannot just take the lowest 3 bytes from the 32 bit
	 * integer, because also need to take care of clipping any
	 * value > 2&23
	 */
	for (i = 0, k = 0; i < num_data; ++i, k += 3) {
		if (data[i] >= 0)
			d = MIN(data[i], (1 << 23) - 1);
		else {
			/* 2's complement */
			d = (1 << 24) - MIN(-data[i], 1 << 23);
		}
		bytes[k] = (d >> 16) & 0xFF;	/* MSB */
		bytes[k + 1] = (d >> 8) & 0xFF;
		bytes[k + 2] = (d) & 0xFF;	/* LSB */
	}
}


/*
 * convert DSP memory bytes to signed 24 bit integers
 * data contains "len/3" elements
 * bytes contains "len" elements
 */
void tfa98xx_convert_bytes2data(int len, const u8 *bytes, int *data)
{
	int i, k, d;
	int num_data = len / 3;

	for (i = 0, k = 0; i < num_data; ++i, k += 3) {
		d = (bytes[k] << 16) | (bytes[k + 1] << 8) | (bytes[k + 2]);
		if (bytes[k] & 0x80)	/* sign bit was set */
			d = -((1 << 24) - d);

		data[i] = d;
	}
}


int tfa98xx_dsp_read_mem(struct tfa98xx *tfa98xx, u16 start_offset,
			 int num_words, int *values)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	int ret = 0;
	u16 cf_ctrl;	/* to sent to the CF_CONTROLS register */
	u8 bytes[MAX_PARAM_SIZE];
	int burst_size;		/* number of words per burst size */
	int bytes_per_word = 3;
	int len;
	int *p;

	/* first set DMEM and AIF, leaving other bits intact */
	cf_ctrl = snd_soc_read(codec, TFA98XX_CF_CONTROLS);
	cf_ctrl &= ~0x000E;	/* clear AIF & DMEM */
	/* set DMEM, leave AIF cleared for autoincrement */
	cf_ctrl |= (Tfa98xx_DMEM_XMEM << 1);

	snd_soc_write(codec, TFA98XX_CF_CONTROLS, cf_ctrl);

	snd_soc_write(codec, TFA98XX_CF_MAD, start_offset);

	len = num_words * bytes_per_word;
	p = values;
	for (; len > 0;) {
		burst_size = ROUND_DOWN(16, bytes_per_word);
		if (len < burst_size)
			burst_size = len;

		ret = tfa98xx_read_data(tfa98xx, TFA98XX_CF_MEM, burst_size,
					bytes);
		if (ret)
			return ret;

		tfa98xx_convert_bytes2data(burst_size, bytes, p);
		/* pr_debug("0x%06x\n", *p); */
		len -= burst_size;
		p += burst_size / bytes_per_word;
	}

	return 0;
}

/*
 * Write all the bytes specified by len and data
 */
static int tfa98xx_write_data(struct tfa98xx *tfa98xx, u8 subaddress, int len,
		       const u8 *data)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u8 write_data[MAX_PARAM_SIZE];
	/* subaddress followed by data */
	int count = len + 1;

	/* pr_debug("%d\n", len); */

	if (count > MAX_PARAM_SIZE) {
		pr_err("Error param size too big %d\n", len);
		return -EINVAL;
	}

	write_data[0] = subaddress;
	memcpy(write_data + 1, data, len);

	return tfa98xx_bulk_write_raw(codec, write_data, count);
}


static int tfa98xx_dsp_write_mem(struct tfa98xx *tfa98xx, unsigned int address,
				 int value, int memtype)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	int ret = 0;
	u16 cf_ctrl; /* the value to sent to the CF_CONTROLS register */
	u8 bytes[3];

	pr_debug("@0x%04x=%d\n", address, value);

	/* first set DMEM and AIF, leaving other bits intact */
	cf_ctrl = snd_soc_read(codec, TFA98XX_CF_CONTROLS);
	cf_ctrl &= ~0x000E;     /* clear AIF & DMEM */

	switch (memtype) {
	case Tfa98xx_DMEM_PMEM:
		cf_ctrl |= (Tfa98xx_DMEM_PMEM << 1);
		break;
	case Tfa98xx_DMEM_XMEM:
		cf_ctrl |= (Tfa98xx_DMEM_XMEM << 1);
		break;
	case Tfa98xx_DMEM_YMEM:
		cf_ctrl |= (Tfa98xx_DMEM_YMEM << 1);
		break;
	case Tfa98xx_DMEM_IOMEM:
		cf_ctrl |= (Tfa98xx_DMEM_IOMEM << 1);
		break;
	}
	snd_soc_write(codec, TFA98XX_CF_CONTROLS, cf_ctrl);
	snd_soc_write(codec, TFA98XX_CF_MAD, address & 0xffff);

	tfa98xx_convert_data2bytes(1, &value, bytes);
	ret = tfa98xx_write_data(tfa98xx, TFA98XX_CF_MEM, 3, bytes);
	if (ret)
		return ret;

	return 0;
}

/* tfaRunWriteDspMem */
int tfa98xx_write_dsp_mem(struct tfa98xx *tfa98xx, struct nxpTfaDspMem *cfmem)
{
	int ret = 0;
	int i;

	for (i = 0; (i < cfmem->size) && (ret == 0); i++) {
		ret = tfa98xx_dsp_write_mem(tfa98xx, cfmem->address++,
					    cfmem->words[i], cfmem->type);
	}

	return ret;
}


static int tfa98xx_is87(struct tfa98xx *tfa98xx);

/*
	return the target address for the filter on this device

filter_index:
	[0..9] reserved for EQ (not deployed, calc. is available)
	[10..12] anti-alias filter
	[13]  integrator filter

 */
static int tfa98xx_filter_mem(struct tfa98xx *tfa98xx,
				int filter_index, u16 *address)
{
	int dmem = -1;
	int idx;
	const u16 bq_table[4][4] = {
	/* index: 10, 11, 12, 13 */
		{346, 351, 356, 288}, /* 87 BRA_MAX_MRA4-2_7.00 */
		{346, 351, 356, 288}, /* 90 BRA_MAX_MRA6_9.02   */
		{467, 472, 477, 409}, /* 95 BRA_MAX_MRA7_10.02  */
		{406, 411, 416, 348}  /* 97 BRA_MAX_MRA9_12.01  */
	};


	if ((10 <= filter_index) && (filter_index <= 13)) {
		dmem = Tfa98xx_DMEM_YMEM; /* for all devices */
		idx = filter_index-10;

		switch (tfa98xx->rev) {
		case 0x12:
			if (tfa98xx_is87(tfa98xx))
				*address = bq_table[0][idx];
			else
				*address = bq_table[2][idx];
			break;
		case 0x97:
			*address = bq_table[3][idx];
			break;
		case 0x80:
		case 0x81: /* for the RAM version */
		case 0x91:
			*address = bq_table[1][idx];
			break;
		default:
			pr_err("Unsupported device: rev=0x%x, subrev=0x%x\n",
				tfa98xx->rev,  tfa98xx->subrev);
			return -EINVAL;
		}
	}

	return dmem;
}

/* tfaRunWriteFilter */
int tfa98xx_write_filter(struct tfa98xx *tfa98xx,
			 struct nxpTfaBiquadSettings *bq)
{
	int dmem;
	u16 address;
	u8 data[3*3 + sizeof(bq->bytes)];

	/* get the target address for the filter on this device */
	dmem = tfa98xx_filter_mem(tfa98xx, bq->index, &address);
	if (dmem < 0)
		return dmem;

	/*
	 * send a DSP memory message that targets the devices specific memory
	 * for the filter msg params: which_mem, start_offset, num_words msg
	*/
	memset(data, 0, 3*3);
	data[2] = dmem;                /* output[0] = which_mem */
	data[4] = address >> 8;        /* output[1] = start_offset */
	data[5] = address & 0xff;
	data[8] = sizeof(bq->bytes)/3; /* output[2] = num_words */
	memcpy(&data[9], bq->bytes, sizeof(bq->bytes)); /* payload */

	return tfa98xx_dsp_set_param(tfa98xx, /*framework*/0, /*param*/4,
			sizeof(data), data);
}

int tfa98xx_dsp_reset_count(struct tfa98xx *tfa98xx)
{
	int count;

	tfa98xx_dsp_read_mem(tfa98xx, TFA98XX_XMEM_COUNT_BOOT, 1, &count);

	return count;
}

/*
 * wait for calibrate done
 */
static int tfa98xx_wait_calibration(struct tfa98xx *tfa98xx, int *done)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	int ret = 0;
	int tries = 0;
	u16 mtp;

	*done = 0;

	mtp = snd_soc_read(codec, TFA98XX_MTP);

	pr_debug("TFA98XX_MTP 0x%04x\n", mtp);

	/* in case of calibrate once wait for MTPEX */
	if (mtp & TFA98XX_MTP_MTPOTC) {
		pr_debug("calibrate once wait for MTPEX\n");
		while ((*done == 0) && (tries < TFA98XX_WAITRESULT_NTRIES_WAIT)) {
			msleep_interruptible(5);
			mtp = snd_soc_read(codec, TFA98XX_MTP);
			/* check MTP bit1 (MTPEX) */
			*done = (mtp & TFA98XX_MTP_MTPEX);
			tries++;
		}
	} else { /* poll xmem for calibrate always */
		pr_debug("poll xmem for calibrate always\n");
		while ((*done == 0) && (tries < TFA98XX_WAITRESULT_NTRIES_WAIT)) {
			msleep_interruptible(5);
			ret = tfa98xx_dsp_read_mem(tfa98xx,
						   TFA98XX_XMEM_CALIBRATION_DONE,
						   1, done);
			tries++;
		}
	}

	if (tries == TFA98XX_WAITRESULT_NTRIES_WAIT) {
		pr_err("Calibrate Done timedout\n");
		return -ETIMEDOUT;
	}

	return ret;
}

static int tfa9887_specific(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 value;

	pr_debug("\n");

	value = snd_soc_read(codec, TFA98XX_SYS_CTRL);
	/* DSP must be in control of the amplifier to avoid plops */
	value |= TFA98XX_SYS_CTRL_AMPE_MSK;
	snd_soc_write(codec, TFA98XX_SYS_CTRL, value);

	/* some other registers must be set for optimal amplifier behaviour */
	snd_soc_write(codec, TFA98XX_BAT_PROT, 0x13AB);
	snd_soc_write(codec, TFA98XX_AUDIO_CTR, 0x001F);
	/* peak voltage protection is always on, but may be written */
	snd_soc_write(codec, 0x08, 0x3C4E);
	/* TFA98XX_SYSCTRL_DCA = 0 */
	snd_soc_write(codec, TFA98XX_SYS_CTRL, 0x024D);
	snd_soc_write(codec, 0x0A, 0x3EC3);
	snd_soc_write(codec, 0x41, 0x0308);
	snd_soc_write(codec, 0x49, 0x0E82);

	return 0;
}

static int tfa9887B_specific(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 value;

	pr_debug("\n");

	/* all i2C registers are already set to default */
	value = snd_soc_read(codec, TFA98XX_SYS_CTRL);
	/* DSP must be in control of the amplifier to avoid plops */
	value |= TFA98XX_SYS_CTRL_AMPE_MSK;
	snd_soc_write(codec, TFA98XX_SYS_CTRL, value);

	/* some other registers must be set for optimal amplifier behaviour */
	snd_soc_write(codec, TFA98XX_BAT_PROT, 0x13AB);
	snd_soc_write(codec, TFA98XX_AUDIO_CTR, 0x001F);
	/* peak voltage protection is always on, but may be written */
	snd_soc_write(codec, 0x08, 0x3C4E);
	/* TFA98XX_SYSCTRL_DCA = 0 */
	snd_soc_write(codec, TFA98XX_SYS_CTRL, 0x024D);
	snd_soc_write(codec, 0x41, 0x0308);
	snd_soc_write(codec, 0x49, 0x0E82);

	return 0;
}

static int tfa9890_specific(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 reg;

	pr_debug("\n");

	/* all i2C registers are already set to default for N1C2 */

	/* some PLL registers must be set optimal for amplifier behaviour */
	snd_soc_write(codec, 0x40, 0x5a6b);
	reg = snd_soc_read(codec, 0x59);

	reg |= 0x3;

	snd_soc_write(codec, 0x59, reg);
	snd_soc_write(codec, 0x40, 0x0000);

	return 0;
}

static int tfa9897_specific(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 reg;

	pr_debug("\n");

	/* all i2C registers must already set to default POR value */

	/* $48:[3] - 1 ==> 0; iddqtestbst - default value changed. */
	snd_soc_write(codec, 0x48, 0x0300); /* POR value = 0x308 */

	/* $49:[0] - 1 ==> 0; CLIP - default value changed. 0 means CLIPPER on*/
	reg = snd_soc_read(codec, TFA98XX_CURRENTSENSE4);
	reg &= ~(TFA98XX_CURRENTSENSE4_CLIP_MSK);
	snd_soc_write(codec, TFA98XX_CURRENTSENSE4, reg);

	return 0;
}

/*
 * clockless way to determine if this is the tfa9887 or tfa9895
 * by testing if the PVP bit is writable
 */
static int tfa98xx_is87(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 save_value, check_value;

	pr_debug("\n");

	save_value = snd_soc_read(codec, 0x08);

	/* if clear it's 87 */
	if ((save_value & 0x0400) == 0)
		return 1;

	/* try to clear pvp bit */
	snd_soc_write(codec, 0x08, (save_value & ~0x0400));
	check_value = snd_soc_read(codec, 0x08);

	/* restore */
	snd_soc_write(codec, 0x08, save_value);
	/* could we write the bit */

	/* if changed it's the 87 */
	return (check_value != save_value) ? 1 : 0;
}

/*
 * I2C register init should be done at probe/recover time (TBC)
 */
static int tfa98xx_init(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	int ret;

	pr_debug("Reset all i2c registers\n");

	/* reset all i2c registers to default */
	snd_soc_write(codec, TFA98XX_SYS_CTRL, TFA98XX_SYS_CTRL_I2CR_MSK);

	switch (tfa98xx->rev) {
	case 0x12:
		if (tfa98xx_is87(tfa98xx))
			ret = tfa9887_specific(tfa98xx);
		else
			ret = tfa9887B_specific(tfa98xx);
		break;
	case 0x80:
		ret = tfa9890_specific(tfa98xx);
		break;
	case 0x91:
		break;
	case 0x97:
		ret = tfa9897_specific(tfa98xx);
		break;
	case 0x81:
		/* for the RAM version disable clock-gating */
		ret = tfa9890_specific(tfa98xx);
		tfa98xx_clockgating(tfa98xx, 0);
		break;
	default:
		pr_err("Unsupported device: rev=0x%x, subrev=0x%x\n",
		       tfa98xx->rev,  tfa98xx->subrev);
		return -EINVAL;
	}

	return ret;
}

/* NXP: Only restore I2C registers */
int tfa98xx_restore_i2cmtp(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 readValue;

	pr_err("%s\n", __func__);

	snd_soc_write(codec, 0x40, 0x5a6b);
	readValue = (u16)snd_soc_read(codec, 0x8b);

	readValue ^= 0x5a;

	snd_soc_write(codec, 0x60, readValue);
	snd_soc_write(codec, 0x0b, 0x5A);

	snd_soc_write(codec, 0x81, 0xF800);
	snd_soc_write(codec, 0x82, 0x414);
	snd_soc_write(codec, 0x84, 0x3F40);
	snd_soc_write(codec, 0x85, 0x0);
	snd_soc_write(codec, 0x86, 0x0);
	snd_soc_write(codec, 0x87, 0x0);
	snd_soc_write(codec, 0x8a, 0x5555);
	snd_soc_write(codec, 0x8b, 0xAAAA);
	snd_soc_write(codec, 0x8c, 0x5555);
	snd_soc_write(codec, 0x8d, 0xAAAA);

	snd_soc_write(codec, 0x60, 0x0);
	snd_soc_write(codec, 0x40, 0x0);

	return 0;
}


/*
 * start the clocks and wait until the AMP is switching
 * on return the DSP sub system will be ready for loading
 */
static int tfa98xx_startup(struct tfa98xx *tfa98xx)
{
	int tries, status, ret;
	u16 mtp0;

	pr_debug("\n");

	/* load the optimal TFA98XX in HW settings */
	ret = tfa98xx_init(tfa98xx);

	/*
	 * I2S settings to define the audio input properties
	 * these must be set before the subsys is up
	 * this will run the list until a non-register item is encountered
	 */
	ret = tfaContWriteRegsDev(tfa98xx); /* write device register settings */

	/*
	 * also write register the settings from the default profile
	 * NOTE we may still have ACS=1 so we can switch sample rate here
	 */
	ret = tfaContWriteRegsProf(tfa98xx, tfa98xx->profile_current);

	/* power on the sub system */
	ret = tfa98xx_powerdown(tfa98xx, 0);

	/* NXP: Added the check of AREF before reset DSP */
	/*  powered on
	 *    - now check if it is allowed to access DSP specifics
	 */
	for (tries = 1; tries < CFSTABLE_TRIES; tries++) {
		status = (u16)snd_soc_read(tfa98xx->codec, TFA98XX_STATUSREG);
		if (status & TFA98XX_STATUSREG_AREFS_MSK)
			break;
		else
			msleep_interruptible(1);
	}

	/* NXP: Added putting DSP to reset mode to start TFA in proper sequence */
	/*
	 * Reset Coolflux
	 */
	ret = tfa98xx_dsp_reset(tfa98xx, 1);
	if (ret)
		pr_debug("TFA set DSP to reset failed\n");

	/*  powered on
	 *    - now it is allowed to access DSP specifics
	 */

	/*
	 * wait until the DSP subsystem hardware is ready
	 *    note that the DSP CPU is not running (RST=1)
	 */
	pr_debug("Waiting for DSP system stable...()\n");
	for (tries = 1; tries < CFSTABLE_TRIES; tries++) {
		ret = tfa98xx_dsp_system_stable(tfa98xx, &status);
		if (status)
			break;
	}

	if (tries == CFSTABLE_TRIES) {
		 pr_err("tfa98xx_dsp_system_stable Time out\n");
		/*
		* check the contents of  MTP register for non-zero,
		* this indicates that the subsys is ready
		*/
		mtp0 = (u16)snd_soc_read(tfa98xx->codec, 0x84);

		/* NXP: if 0x84 is wrong, restore the correct mtp settings */
		if (!mtp0) {
			pr_err("nxp: mtp0 incident before calibration.\n");
			tfa98xx_restore_i2cmtp(tfa98xx);
			return 0;
		}
		return -ETIMEDOUT;
	}  else {
		pr_debug("OK (tries=%d)\n", tries);
	}

	/* the CF subsystem is enabled */
	pr_debug("reset count:0x%x\n", tfa98xx_dsp_reset_count(tfa98xx));

	return 0;
}

static int tfa98xx_is_coldboot(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 status;

	/* check status ACS bit to set */
	status = snd_soc_read(codec, TFA98XX_STATUSREG);

	pr_debug("ACS %d\n", (status & TFA98XX_STATUSREG_ACS) != 0);

	return (status & TFA98XX_STATUSREG_ACS) != 0;
}

/*
 * report if we are in powerdown state
 * use AREFS from the status register iso the actual PWDN bit
 * return true if powered down
 */
int tfa98xx_is_pwdn(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 status;

	/* check if PWDN bit is clear by looking at AREFS */
	status = snd_soc_read(codec, TFA98XX_STATUSREG);

	pr_debug("AREFS %d\n", (status & TFA98XX_STATUSREG_AREFS) != 0);

	return (status & TFA98XX_STATUSREG_AREFS) == 0;
}

/*
 * report if device has been calibrated
 */
int tfaIsCalibrated(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 status;

	status = snd_soc_read(codec, TFA98XX_KEY2_PROTECTED_SPKR_CAL_MTP);

	pr_debug("MTPEX %d\n", (status & TFA98XX_KEY2_PROTECTED_SPKR_CAL_MTP_MTPEX) != 0);

	return ((status & TFA98XX_KEY2_PROTECTED_SPKR_CAL_MTP_MTPEX) != 0);
}

int tfa98xx_is_amp_running(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 status;

	/* check status SWS bit to set */
	status = snd_soc_read(codec, TFA98XX_STATUSREG);

	pr_debug("SWS %d\n", (status & TFA98XX_STATUSREG_SWS_MSK) != 0);

	return (status & TFA98XX_STATUSREG_SWS_MSK) != 0;
}

#define CF_CONTROL 0x8100

int tfa98xx_coldboot(struct tfa98xx *tfa98xx, int state)
{
	int ret = 0;
	int tries = 10;

	pr_debug("\n");

	/* repeat set ACS bit until set as requested */
	while (state == !tfa98xx_is_coldboot(tfa98xx)) {
		/* set coldstarted in CF_CONTROL to force ACS */
		ret = tfa98xx_dsp_write_mem(tfa98xx, CF_CONTROL, state,
					    Tfa98xx_DMEM_IOMEM);

		if (tries-- == 0) {
			pr_debug("coldboot (ACS) did not %s\n",
				  state ? "set" : "clear");
			return -EINVAL;
		}
	}

	return ret;
}

/*
 * powerup the coolflux subsystem and wait for it
 */
int tfa98xx_dsp_power_up(struct tfa98xx *tfa98xx)
{
	int ret = 0;
	int tries, status;

	pr_debug("\n");

	/* power on the sub system */
	ret = tfa98xx_powerdown(tfa98xx, 0);

	pr_debug("Waiting for DSP system stable...\n");

	/* wait until everything is stable, in case clock has been off */
	for (tries = CFSTABLE_TRIES; tries > 0; tries--) {
		ret = tfa98xx_dsp_system_stable(tfa98xx, &status);
		if (status)
			break;
	}

	if (tries == 0) {
		/* timedout */
		pr_err("DSP subsystem start timed out\n");
		return -ETIMEDOUT;
	}

	return ret;
}

/*
 * the patch contains a header with the following
 * IC revision register: 1 byte, 0xFF means don't care
 * XMEM address to check: 2 bytes, big endian, 0xFFFF means don't care
 * XMEM value to expect: 3 bytes, big endian
 */
int tfa98xx_check_ic_rom_version(struct tfa98xx *tfa98xx,
				 const u8 patchheader[])
{
	int ret = 0;
	u16 checkrev;
	u16 checkaddress;
	int checkvalue;
	int value = 0;
	int status;

	pr_debug("FW rev: %x, IC rev %x\n", patchheader[0], tfa98xx->rev);

	checkrev = patchheader[0];
	if ((checkrev != 0xff) && (checkrev != tfa98xx->rev))
		return -EINVAL;

	checkaddress = (patchheader[1] << 8) + patchheader[2];
	checkvalue = (patchheader[3] << 16) + (patchheader[4] << 8) +
		     patchheader[5];

	if (checkaddress != 0xffff) {
		pr_debug("checkvalue: 0x%04x, checkvalue 0x%08x\n",
			  checkvalue, checkvalue);
		/* before reading XMEM, check if we can access the DSP */
		ret = tfa98xx_dsp_system_stable(tfa98xx, &status);
		if (!ret) {
			if (!status) {
				/* DSP subsys not running */
				ret = -EBUSY;
			}
		}

		/* read register to check the correct ROM version */
		if (!ret) {
			ret = tfa98xx_dsp_read_mem(tfa98xx, checkaddress, 1,
						   &value);
			pr_debug("checkvalue: 0x%08x, DSP 0x%08x\n", checkvalue,
				  value);
		}

		if (!ret) {
			if (value != checkvalue)
				ret = -EINVAL;
		}
	}

	return ret;
}


int tfa98xx_process_patch_file(struct tfa98xx *tfa98xx, int len, const u8 *data)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 size;
	int index;
	int ret = 0;
	u8 chunk_buf[TFA98XX_MAX_I2C_SIZE + 1];

	pr_debug("len %d\n", len);

	/*
	 * expect following format in patchBytes:
	 * 2 bytes len of I2C transaction in little endian, then the bytes,
	 * excluding the slave address which is added from the handle
	 * This repeats for the whole file
	 */
	index = 0;
	while (index < len) {
		/* extract little endian length */
		size = data[index] + data[index + 1] * 256;
		if (size > TFA98XX_MAX_I2C_SIZE)
			pr_err("Patch chunk size %d > %d\n", size,
				TFA98XX_MAX_I2C_SIZE);

		index += 2;

		if ((index + size) > len) {
			/* outside the buffer, error in the input data */
			return -EINVAL;
		}

		/*
		 * Need to copy data from the fw into local memory to avoid
		 * trouble with some i2c controller
		 */
		memcpy(chunk_buf, data + index, size);

		ret = tfa98xx_bulk_write_raw(codec, chunk_buf, size);
		if (ret) {
			pr_err("writing dsp patch failed %d\n", ret);
			break;
		}

		index += size;
	}

	return ret;
}

#define PATCH_HEADER_LENGTH 6
int tfa98xx_dsp_patch(struct tfa98xx *tfa98xx, int patchLength,
		      const u8 *patchBytes)
{
	int ret = 0;

	pr_debug("\n");

	if (patchLength < PATCH_HEADER_LENGTH)
		return -EINVAL;

	ret = tfa98xx_check_ic_rom_version(tfa98xx, patchBytes);
	if (ret) {
		pr_err("Error, patch in container file does not match device!\n");
		return ret;

	}

	ret = tfa98xx_process_patch_file(tfa98xx,
					 patchLength - PATCH_HEADER_LENGTH,
					 patchBytes + PATCH_HEADER_LENGTH);
	return ret;
}


int tfa98xx_set_configured(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 value;

	pr_debug("\n");

	/* read the SystemControl register, modify the bit and write again */
	value = snd_soc_read(codec, TFA98XX_SYS_CTRL);
	value |= TFA98XX_SYS_CTRL_SBSL_MSK;
	snd_soc_write(codec, TFA98XX_SYS_CTRL, value);

	return 0;
}

#define TO_LONG_LONG(x)	((s64)(x)<<32)
#define TO_INT(x)	((x)>>32)
#define TO_FIXED(e)	e

int float_to_int(u32 x)
{
	unsigned e = (0x7F + 31) - ((*(unsigned *) &x & 0x7F800000) >> 23);
	unsigned m = 0x80000000 | (*(unsigned *) &x << 8);
	return (int)((m >> e) & -(e < 32));
}

int tfa98xx_set_volume(struct tfa98xx *tfa98xx, u32 voldB)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 value;
	int volume_value;

	value = snd_soc_read(codec, TFA98XX_AUDIO_CTR);

	/*
	 * 0x00 ->  0.0 dB
	 * 0x01 -> -0.5 dB
	 * ...
	 * 0xFE -> -127dB
	 * 0xFF -> muted
	 */
	volume_value = 2 * float_to_int(voldB);
	if (volume_value > 255)
		volume_value = 255;

	pr_debug("%d, attenuation -%d dB\n", volume_value, float_to_int(voldB));

	/* volume value is in the top 8 bits of the register */
	value = (value & 0x00FF) | (u16)(volume_value << 8);
	snd_soc_write(codec, TFA98XX_AUDIO_CTR, value);

	return 0;
}


int tfa98xx_get_volume(struct tfa98xx *tfa98xx, s64 *pVoldB)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	int ret = 0;
	u16 value;

	pr_debug("\n");

	value = snd_soc_read(codec, TFA98XX_AUDIO_CTR);
	value >>= 8;
	*pVoldB = TO_FIXED(value) / -2;

	return ret;
}


static int tfa98xx_set_mute(struct tfa98xx *tfa98xx, enum Tfa98xx_Mute mute)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	int ret = 0;
	u16 audioctrl_value;
	u16 sysctrl_value;

	pr_debug("\n");

	audioctrl_value = snd_soc_read(codec, TFA98XX_AUDIO_CTR);
	sysctrl_value = snd_soc_read(codec, TFA98XX_SYS_CTRL);

	switch (mute) {
	case Tfa98xx_Mute_Off:
		/*
		 * previous state can be digital or amplifier mute,
		 * clear the cf_mute and set the enbl_amplifier bits
		 *
		 * To reduce PLOP at power on it is needed to switch the
		 * amplifier on with the DCDC in follower mode
		 * (enbl_boost = 0 ?).
		 * This workaround is also needed when toggling the
		 * powerdown bit!
		 */
		audioctrl_value &= ~(TFA98XX_AUDIO_CTR_CFSM_MSK);
		sysctrl_value |= (TFA98XX_SYS_CTRL_AMPE_MSK |
				  TFA98XX_SYS_CTRL_DCA_MSK);
		break;
	case Tfa98xx_Mute_Digital:
		/* set the cf_mute bit */
		audioctrl_value |= TFA98XX_AUDIO_CTR_CFSM_MSK;
		/* set the enbl_amplifier bit */
		sysctrl_value |= (TFA98XX_SYS_CTRL_AMPE_MSK);
		/* clear active mode */
		sysctrl_value &= ~(TFA98XX_SYS_CTRL_DCA_MSK);
		break;
	case Tfa98xx_Mute_Amplifier:
		/* clear the cf_mute bit */
		audioctrl_value &= ~TFA98XX_AUDIO_CTR_CFSM_MSK;
		/* clear the enbl_amplifier bit and active mode */
		sysctrl_value &= ~(TFA98XX_SYS_CTRL_AMPE_MSK |
				   TFA98XX_SYS_CTRL_DCA_MSK);
		break;
	default:
		return -EINVAL;
	}

	ret = snd_soc_write(codec, TFA98XX_AUDIO_CTR, audioctrl_value);
	if (ret)
		return ret;

	ret = snd_soc_write(codec, TFA98XX_SYS_CTRL, sysctrl_value);
	return ret;
}


int tfa98xx_mute(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	int ret = 0;
	u16 status;
	int tries = 0;

	pr_debug("\n");

	/* signal the TFA98XX to mute plop free and turn off the amplifier */
	ret = tfa98xx_set_mute(tfa98xx, Tfa98xx_Mute_Amplifier);
	if (ret)
		return ret;

	/* now wait for the amplifier to turn off */
	status = snd_soc_read(codec, TFA98XX_STATUSREG);
	while (((status & TFA98XX_STATUSREG_SWS) == TFA98XX_STATUSREG_SWS)
			&& (tries < TFA98XX_WAITRESULT_NTRIES)) {
		usleep_range(10000, 11000);
		status = snd_soc_read(codec, TFA98XX_STATUSREG);
		tries++;
	}

	/* The amplifier is always switching */
	if (tries == TFA98XX_WAITRESULT_NTRIES)
		return -ETIMEDOUT;

	pr_debug("-------------------- muted --------------------\n");

	return 0;
}


int tfa98xx_unmute(struct tfa98xx *tfa98xx)
{
	int ret = 0;

	/* signal the TFA98XX to mute  */
	ret = tfa98xx_set_mute(tfa98xx, Tfa98xx_Mute_Off);

	pr_debug("-------------------unmuted ------------------\n");

	return ret;
}


/* check that num_byte matches the memory type selected */
int tfa98xx_check_size(enum Tfa98xx_DMEM which_mem, int len)
{
	int ret = 0;
	int modulo_size = 1;

	switch (which_mem) {
	case Tfa98xx_DMEM_PMEM:
		/* 32 bit PMEM */
		modulo_size = 4;
		break;
	case Tfa98xx_DMEM_XMEM:
	case Tfa98xx_DMEM_YMEM:
	case Tfa98xx_DMEM_IOMEM:
		/* 24 bit MEM */
		modulo_size = 3;
		break;
	default:
		return -EINVAL;
	}

	if ((len % modulo_size) != 0)
		return -EINVAL;

	return ret;
}


int tfa98xx_execute_param(struct tfa98xx *tfa98xx)
{
	struct snd_soc_codec *codec = tfa98xx->codec;

	/* the value to be sent to the CF_CONTROLS register: cf_req=00000000,
	 * cf_int=0, cf_aif=0, cf_dmem=XMEM=01, cf_rst_dsp=0 */
	u16 cf_ctrl = 0x0002;
	cf_ctrl |= (1 << 8) | (1 << 4);	/* set the cf_req1 and cf_int bit */
	return snd_soc_write(codec, TFA98XX_CF_CONTROLS, cf_ctrl);
}

int tfa98xx_wait_result(struct tfa98xx *tfa98xx, int waitRetryCount)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	int ret = 0;
	u16 cf_status; /* the contents of the CF_STATUS register */
	int tries = 0;

	/* don't wait forever, DSP is pretty quick to respond (< 1ms) */
	do {
		cf_status = snd_soc_read(codec, TFA98XX_CF_STATUS);
		tries++;
	} while ((!ret) && ((cf_status & 0x0100) == 0)
			  && (tries < waitRetryCount));
	if (tries >= waitRetryCount) {
		/* something wrong with communication with DSP */
		pr_err("Error DSP not running\n");
		return -ETIMEDOUT;
	}

	return 0;
}


/* read the return code for the RPC call */
int tfa98xx_check_rpc_status(struct tfa98xx *tfa98xx, int *status)
{
	int ret = 0;
	/* the value to sent to the * CF_CONTROLS register: cf_req=00000000,
	 * cf_int=0, cf_aif=0, cf_dmem=XMEM=01, cf_rst_dsp=0 */
	u16 cf_ctrl = 0x0002;
	/* memory address to be accessed (0: Status, 1: ID, 2: parameters) */
	u16 cf_mad = 0x0000;
	u8 mem[3];	/* for the status read from DSP memory */
	u8 buffer[4];

	/* minimize the number of I2C transactions by making use
	 * of the autoincrement in I2C */
	/* first the data for CF_CONTROLS */
	buffer[0] = (u8)((cf_ctrl >> 8) & 0xFF);
	buffer[1] = (u8)(cf_ctrl & 0xFF);
	/* write the contents of CF_MAD which is the subaddress
	 * following CF_CONTROLS */
	buffer[2] = (u8)((cf_mad >> 8) & 0xFF);
	buffer[3] = (u8)(cf_mad & 0xFF);

	ret = tfa98xx_write_data(tfa98xx, TFA98XX_CF_CONTROLS, sizeof(buffer),
				 buffer);
	if (ret)
		return ret;

	/* read 1 word (24 bit) from XMEM */
	ret = tfa98xx_read_data(tfa98xx, TFA98XX_CF_MEM, 3, mem);
	if (ret)
		return ret;

	*status = mem[0] << 16 | mem[1] << 8 | mem[2];

	return 0;
}

int tfa98xx_write_parameter(struct tfa98xx *tfa98xx,
			    u8 module_id,
			    u8 param_id,
			    int len, const u8 data[])
{
	int  ret;
	/*
	 * the value to be sent to the CF_CONTROLS register: cf_req=00000000,
	 * cf_int=0, cf_aif=0, cf_dmem=XMEM=01, cf_rst_dsp=0
	 */
	u16 cf_ctrl = 0x0002;
	/* memory address to be accessed (0 : Status, 1 : ID, 2 : parameters)*/
	u16 cf_mad = 0x0001;
	u8 buffer[7];
	int offset = 0;
	int chunk_size = ROUND_DOWN(TFA98XX_MAX_I2C_SIZE, 3);
	int remaining_bytes = len;

	/* pr_debug("%d\n", len); */

	ret = tfa98xx_check_size(Tfa98xx_DMEM_XMEM, len);
	if (!ret) {
		if ((len <= 0) || (len > MAX_PARAM_SIZE)) {
			pr_err("Error in parameters size\n");
			return -EINVAL;
		}
	}

	/*
	 * minimize the number of I2C transactions by making use of
	 * the autoincrement in I2C
	 */

	/* first the data for CF_CONTROLS */
	buffer[0] = (u8)((cf_ctrl >> 8) & 0xFF);
	buffer[1] = (u8)(cf_ctrl & 0xFF);
	/*
	 * write the contents of CF_MAD which is the subaddress
	 * following CF_CONTROLS
	 */
	buffer[2] = (u8)((cf_mad >> 8) & 0xFF);
	buffer[3] = (u8)(cf_mad & 0xFF);
	/*
	 * write the module and RPC id into CF_MEM, which
	 * follows CF_MAD
	 */
	buffer[4] = 0;
	buffer[5] = module_id + 128;
	buffer[6] = param_id;

	ret = tfa98xx_write_data(tfa98xx, TFA98XX_CF_CONTROLS, sizeof(buffer),
				 buffer);
	if (ret)
		return ret;

	/*
	 * Thanks to autoincrement in cf_ctrl, next write will
	 * happen at the next address
	 */
	while ((!ret) && (remaining_bytes > 0)) {
		if (remaining_bytes < chunk_size)
			chunk_size = remaining_bytes;

		ret = tfa98xx_write_data(tfa98xx, TFA98XX_CF_MEM, chunk_size,
					 data + offset);

		remaining_bytes -= chunk_size;
		offset += chunk_size;
	}

	return ret;
}


/* Execute RPC protocol to write something to the DSP */
int tfa98xx_dsp_set_param_var_wait(struct tfa98xx *tfa98xx,
				   u8 module_id,
				   u8 param_id, int len,
				   const u8 data[], int waitRetryCount)
{
	int ret = 0;
	int status = 0;

	/* pr_debug("\n"); */

	/* 1) write the id and data to the DSP XMEM */
	ret = tfa98xx_write_parameter(tfa98xx, module_id, param_id, len, data);
	if (ret)
		return ret;

	/* 2) wake up the DSP and let it process the data */
	ret = tfa98xx_execute_param(tfa98xx);
	if (ret)
		return ret;

	/* 3) wait for the ack */
	ret = tfa98xx_wait_result(tfa98xx, waitRetryCount);
	if (ret)
		return ret;

	/* 4) check the RPC return value */
	ret = tfa98xx_check_rpc_status(tfa98xx, &status);
	if (ret)
		return ret;

	if (status) {
		/* DSP RPC call returned an error */
		pr_err("DSP RPC error %d\n", status + ERROR_RPC_BASE);
		return -EIO;
	}

	return ret;
}

/* Execute RPC protocol to write something to the DSP */
int tfa98xx_dsp_set_param(struct tfa98xx *tfa98xx, u8 module_id,
			  u8 param_id, int len,
			  const u8 *data)
{
	/* Use small WaitResult retry count */
	return tfa98xx_dsp_set_param_var_wait(tfa98xx, module_id, param_id,
					      len, data,
					      TFA98XX_WAITRESULT_NTRIES);
}

#ifdef CONFIG_SND_SOC_TFA98XX_DEBUG
/*
 *  write/read raw msg functions :
 *  the buffer is provided in little endian format, each word occupying 3 bytes,
 *  length is in bytes.
 */
int tfa98xx_dsp_msg_write(struct tfa98xx *tfa98xx, int length, const u8 *data)
{
	int offset = 0;
	int chunk_size = ROUND_DOWN(TFA98XX_MAX_I2C_SIZE, 3);
	int remaining_bytes = length;
	u8 buffer[4];
	int ret = 0;
	int status = 0;
	/*
	 * the value to be sent to the CF_CONTROLS register: cf_req=00000000,
	 * cf_int=0, cf_aif=0, cf_dmem=XMEM=01, cf_rst_dsp=0
	 */
	u16 cf_ctrl = 0x0002;
	/* memory address to be accessed (0 : Status, 1 : ID, 2 : parameters)*/
	u16 cf_mad = 0x0001;

	/* first the data for CF_CONTROLS */
	buffer[0] = (u8)((cf_ctrl >> 8) & 0xFF);
	buffer[1] = (u8)(cf_ctrl & 0xFF);
	/*
	 * write the contents of CF_MAD which is the subaddress
	 * following CF_CONTROLS
	 */
	buffer[2] = (u8)((cf_mad >> 8) & 0xFF);
	buffer[3] = (u8)(cf_mad & 0xFF);

	ret = tfa98xx_write_data(tfa98xx, TFA98XX_CF_CONTROLS, sizeof(buffer),
				 buffer);
	if (ret)
		return ret;

	/*
	 * Thanks to autoincrement in cf_ctrl, next write will
	 * happen at the next address
	 */
	while ((!ret) && (remaining_bytes > 0)) {
		if (remaining_bytes < chunk_size)
			chunk_size = remaining_bytes;

		ret = tfa98xx_write_data(tfa98xx, TFA98XX_CF_MEM, chunk_size,
					 data + offset);

		remaining_bytes -= chunk_size;
		offset += chunk_size;
	}

	/* 2) wake up the DSP and let it process the data */
	ret = tfa98xx_execute_param(tfa98xx);
	if (ret)
		return ret;

	/* 3) wait for the ack */
	ret = tfa98xx_wait_result(tfa98xx, tfa98xx->dsp_msg_retries);
	if (ret)
		return ret;

	/* 4) check the RPC return value */
	ret = tfa98xx_check_rpc_status(tfa98xx, &status);
	if (ret)
		return ret;

	if (status) {
		/* DSP RPC call returned an error */
		pr_err("DSP RPC error %d\n", status + ERROR_RPC_BASE);
		return -EIO;
	}

	return ret;
}

int tfa98xx_dsp_msg_read(struct tfa98xx *tfa98xx, int length, u8 *data)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	unsigned short cf_ctrl;	/* to sent to the CF_CONTROLS register */
	int burst_size;		/* number of words per burst size */
	int num_bytes;
	int offset = 0;
	int ret = 0;
	unsigned short start_offset = 2; /* msg starts @xmem[2] ,[1]=cmd */

	/* first set DMEM and AIF, leaving other bits intact */
	cf_ctrl = snd_soc_read(codec, TFA98XX_CF_CONTROLS);
	cf_ctrl &= ~0x000E;	/* clear AIF & DMEM */
	/* set DMEM, leave AIF cleared for autoincrement */
	cf_ctrl |= (Tfa98xx_DMEM_XMEM << 1);

	snd_soc_write(codec, TFA98XX_CF_CONTROLS, cf_ctrl);
	snd_soc_write(codec, TFA98XX_CF_MAD, start_offset);

	num_bytes = length; /* input param */
	while (num_bytes > 0) {
		burst_size = ROUND_DOWN(16, 3);
		if (num_bytes < burst_size)
			burst_size = num_bytes;

		ret = tfa98xx_read_data(tfa98xx, TFA98XX_CF_MEM, burst_size,
					data + offset);
		if (ret)
			return ret;

		num_bytes -= burst_size;
		offset += burst_size;
	}

	return ret;
}
#endif

int tfa98xx_dsp_write_config(struct tfa98xx *tfa98xx, int len, const u8 *data)
{
	int ret = 0;

	pr_debug("\n");

	ret = tfa98xx_dsp_set_param(tfa98xx, MODULE_SPEAKERBOOST,
				      SB_PARAM_SET_CONFIG, len, data);

	return ret;
}


/* the number of biquads supported */
#define TFA98XX_BIQUAD_NUM	10
#define BIQUAD_COEFF_SIZE	6

int tfa98xx_dsp_biquad_disable(struct tfa98xx *tfa98xx, int biquad_index)
{
	int coeff_buffer[BIQUAD_COEFF_SIZE];
	u8 data[BIQUAD_COEFF_SIZE * 3];

	if (biquad_index > TFA98XX_BIQUAD_NUM)
		return -EINVAL;

	if (biquad_index < 1)
		return -EINVAL;

	/* set in correct order and format for the DSP */
	coeff_buffer[0] = (int)-8388608; /* -1.0f */
	coeff_buffer[1] = 0;
	coeff_buffer[2] = 0;
	coeff_buffer[3] = 0;
	coeff_buffer[4] = 0;
	coeff_buffer[5] = 0;

	/*
	 * convert to fixed point and then bytes suitable for
	 * transmission over I2C
	 */
	tfa98xx_convert_data2bytes(BIQUAD_COEFF_SIZE, coeff_buffer, data);
	return tfa98xx_dsp_set_param(tfa98xx, MODULE_BIQUADFILTERBANK,
				     (u8)biquad_index,
				     (u8)(BIQUAD_COEFF_SIZE * 3),
				     data);
}

int tfa98xx_dsp_biquad_set_coeff(struct tfa98xx *tfa98xx, int biquad_index,
				 int len, u8 *data)
{
	return tfa98xx_dsp_set_param(tfa98xx, MODULE_BIQUADFILTERBANK,
				     biquad_index, len, data);
}

/*
 * The AgcGainInsert functions are static because they are not public:
 * only allowed mode is PRE.
 * The functions are nevertheless needed because the mode is forced to
 * POST by each SetLSmodel and each SetConfig => it should be reset to
 * PRE afterwards.
 */
int tfa98xx_dsp_set_agc_gain_insert(struct tfa98xx *tfa98xx,
				    enum Tfa98xx_AgcGainInsert
				    agcGainInsert)
{
	int ret = 0;
	unsigned char bytes[3];

	pr_debug("\n");

	tfa98xx_convert_data2bytes(1, (int *) &agcGainInsert, bytes);

	ret = tfa98xx_dsp_set_param(tfa98xx, MODULE_SPEAKERBOOST,
				      SB_PARAM_SET_AGCINS, 3, bytes);

	return ret;
}


int tfa98xx_dsp_write_speaker_parameters(struct tfa98xx *tfa98xx, int len,
					 const u8 *data)
{
	int ret = 0;

	if (!data)
		return -EINVAL;

	pr_debug("%d\n", len);

	ret = tfa98xx_dsp_set_param_var_wait(tfa98xx, MODULE_SPEAKERBOOST,
					       SB_PARAM_SET_LSMODEL, len,
					       data,
					       TFA98XX_WAITRESULT_NTRIES_LONG);

	return ret;
}


int tfa98xx_dsp_write_preset(struct tfa98xx *tfa98xx, int len, const u8 *data)
{
	if (!data)
		return -EINVAL;

	pr_debug("\n");

	return tfa98xx_dsp_set_param(tfa98xx, MODULE_SPEAKERBOOST,
			SB_PARAM_SET_PRESET, len, data);
}

/* load all the parameters for the DRC settings from a file */
int tfa98xx_dsp_write_drc(struct tfa98xx *tfa98xx, int len, const u8 *data)
{
	if (!data)
		return -EINVAL;

	pr_debug("\n");

	return tfa98xx_dsp_set_param(tfa98xx, MODULE_SPEAKERBOOST,
			SB_PARAM_SET_DRC, len, data);
}


/* Execute RPC protocol to read something from the DSP */
int tfa98xx_dsp_get_param(struct tfa98xx *tfa98xx, u8 module_id,
			  u8 param_id, int len, u8 *data)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	int ret = 0;
	u16 cf_mad;
	int status = 0;
	int offset = 0;
	int chunk_size = ROUND_DOWN(TFA98XX_MAX_I2C_SIZE, 3);
	int remaining_bytes = len;

	pr_debug("\n");

	ret = tfa98xx_check_size(Tfa98xx_DMEM_XMEM, len);
	if (!ret) {
		if ((len <= 0) || (len > MAX_PARAM_SIZE))
			return -EINVAL;
	}

	/* 1) write the id and data to the DSP XMEM */
	ret = tfa98xx_write_parameter(tfa98xx, module_id, param_id, len, data);
	if (ret)
		return ret;

	/* 2) wake up the DSP and let it process the data */
	ret = tfa98xx_execute_param(tfa98xx);
	if (ret)
		return ret;

	/* 3) wait for the ack */
	ret = tfa98xx_wait_result(tfa98xx, TFA98XX_WAITRESULT_NTRIES);
	if (ret)
		return ret;

	/* 4) check the RPC return value */
	ret = tfa98xx_check_rpc_status(tfa98xx, &status);
	if (ret)
		return ret;

	if (status) {
		/* DSP RPC call returned an error
		 * Note: when checking for features and a features
		 * is not supported on a device, it returns ERROR_RPC_PARAMID
		 */
		pr_warn("DSP RPC error %d\n", status + ERROR_RPC_BASE);
		return -EIO;
	}

	/* 5) read the resulting data */
	/* memory address to be accessed (0: Status,
	 * 1: ID, 2: parameters) */
	cf_mad = 0x0002;
	snd_soc_write(codec, TFA98XX_CF_MAD, cf_mad);

	/* due to autoincrement in cf_ctrl, next write will happen at
	 * the next address */
	while ((!ret) && (remaining_bytes > 0)) {
		if (remaining_bytes < TFA98XX_MAX_I2C_SIZE)
			chunk_size = remaining_bytes;

		/* else chunk_size remains at initialize value above */
		ret = tfa98xx_read_data(tfa98xx, TFA98XX_CF_MEM, chunk_size,
					  data + offset);
		remaining_bytes -= chunk_size;
		offset += chunk_size;
	}

	return ret;
}


int tfa98xx_dsp_get_sw_feature_bits(struct tfa98xx *tfa98xx, int features[2])
{
	int ret = 0;
	unsigned char bytes[3 * 2];

	pr_debug("\n");

	ret = tfa98xx_dsp_get_param(tfa98xx, MODULE_FRAMEWORK,
					FW_PARAM_GET_FEATURE_BITS,
					sizeof(bytes), bytes);
	/* old ROM code may respond with ERROR_RPC_PARAMID -> -EIO */
	if (ret)
		return ret;

	tfa98xx_convert_bytes2data(sizeof(bytes), bytes, features);

	return ret;
}


int tfa98xx_dsp_support_drc(struct tfa98xx *tfa98xx, int *has_drc)
{
	int ret = 0;
	*has_drc = 0;

	pr_debug("\n");

	if (tfa98xx->has_drc) {
		*has_drc = tfa98xx->has_drc;
	} else {
		int features[2];

		ret = tfa98xx_dsp_get_sw_feature_bits(tfa98xx, features);
		if (!ret) {
			/* easy case: new API available */
			/* bit=0 means DRC enabled */
			*has_drc = (features[0] & FEATURE1_DRC) == 0;
		} else if (ret == -EIO) {
			/* older ROM code, doesn't support it */
			*has_drc = 0;
			ret = 0;
		}
		/* else some other ret, return transparently */

		if (!ret)
			tfa98xx->has_drc = *has_drc;
	}
	return ret;
}

int tfa98xx_resolve_incident(struct tfa98xx *tfa98xx)
{
	if (tfa98xx->rev == REV_TFA9897) {
		pr_warn("OCDS TFA9897 trigger\n");
		/* TFA9897 need to reset the DSP to take the newly set re0 */
		tfa98xx_dsp_reset(tfa98xx, 1);
		tfa98xx_dsp_reset(tfa98xx, 0);
	} else {
		/* TFA98xx need power cycle */
		tfa98xx_powerdown(tfa98xx, 1);
		tfa98xx_powerdown(tfa98xx, 0);
	}

	return 0;
}

int tfa98xx_dsp_get_calibration_impedance(struct tfa98xx *tfa98xx, u32 *re25)
{
	int ret = 0;
	u8 bytes[3];
	int data[1];
	int done;

	pr_debug("\n");

	ret = tfa98xx_dsp_read_mem(tfa98xx, TFA98XX_XMEM_CALIBRATION_DONE, 1,
				   &done);
	if (ret)
		return ret;

	if (!done) {
		pr_err("Calibration not done %d\n", done);
		return -EINVAL;
	}

	ret = tfa98xx_dsp_get_param(tfa98xx, MODULE_SPEAKERBOOST,
				      SB_PARAM_GET_RE0, 3, bytes);

	tfa98xx_convert_bytes2data(3, bytes, data);

	/* /2^23*2^(def.SPKRBST_TEMPERATURE_EXP) */
	*re25 = TO_FIXED(data[0]) / (1 << (23 - SPKRBST_TEMPERATURE_EXP));

	return ret;
}

static int tfa98xx_aec_output(struct tfa98xx *tfa98xx, int enable)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 value;
	int ret = 0;

	if (tfa98xx->rev == REV_TFA9897) {
		/*
		 * 97 powerdown already handled this internally by disabling
		 * TDM interface
		*/
		return ret;
	}

	value = snd_soc_read(codec, TFA98XX_I2SREG);
	if (enable == 0)
		value &= ~TFA98XX_I2SREG_I2SDOE;
	else
		value |= TFA98XX_I2SREG_I2SDOE;
	ret = snd_soc_write(codec, TFA98XX_I2SREG, value);

	return ret;
}


int tfa98xx_select_mode(struct tfa98xx *tfa98xx, enum Tfa98xx_Mode mode)
{
	struct snd_soc_codec *codec = tfa98xx->codec;
	u16 i2s_value, sysctrl_value, temp_value;
	u16 bat_volt;
	int ret = 0;
	int timeoutloop = 100;

	pr_debug("%d\n", mode);

	if (tfa98xx->rev != REV_TFA9897)
		return -EINVAL;

	i2s_value = snd_soc_read(codec, TFA98XX_I2SREG);
	sysctrl_value = snd_soc_read(codec, TFA98XX_SYS_CTRL);

	switch (mode) {
	case Tfa98xx_Mode_Normal:
		/* clear the 2 bits RCV */
		i2s_value &= ~(TFA98XX_AUDIOREG_RCV_MSK);
		sysctrl_value |= (TFA98XX_SYS_CTRL_DCA_MSK);

		ret = snd_soc_write(codec, TFA98XX_I2SREG, i2s_value);
		if (ret == 0) {
			ret = snd_soc_write(codec, TFA98XX_SYS_CTRL,
					    sysctrl_value);
		}
		break;

	case Tfa98xx_Mode_RCV:
		do {
			temp_value = snd_soc_read(codec,
						  TFA98XX_TEMPERATURE);
			if (temp_value == -1)
				ret = -EIO;
			/* wait until th ADC's are up, 0x100 means not
			  ready yet */
		} while ((--timeoutloop) && (temp_value >= 0x100) &&
			 (ret == 0));

		if ((timeoutloop == 0) && (temp_value >= 0x100)) {
			pr_err("ADC's startup timed out");
			ret = -ETIMEDOUT;
		}

		if (ret == 0) {
			bat_volt = snd_soc_read(codec,
						TFA98XX_BATTERYVOLTAGE);
			if (bat_volt < 838) {
				i2s_value |= TFA98XX_AUDIOREG_RCV_MSK;
				sysctrl_value &= ~TFA98XX_SYS_CTRL_DCA_MSK;

				ret = snd_soc_write(codec,
						    TFA98XX_I2SREG, i2s_value);
				if (ret == 0) {
					ret = snd_soc_write(codec,
							    TFA98XX_SYS_CTRL,
							    sysctrl_value);
				}
			} else
				pr_err("Battery voltage too high");
		} else
			pr_err("Failed to read ADC's");
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

/*
 * the int24 values for the vsfw delay table
 */
static u8 vsfwdelay_table[] = {
	0, 0, 2, /* Index 0 - Current/Volt Fractional Delay for 8KHz  */
	0, 0, 0, /* Index 1 - Current/Volt Fractional Delay for 11KHz */
	0, 0, 0, /* Index 2 - Current/Volt Fractional Delay for 12KHz */
	0, 0, 2, /* Index 3 - Current/Volt Fractional Delay for 16KHz */
	0, 0, 2, /* Index 4 - Current/Volt Fractional Delay for 22KHz */
	0, 0, 2, /* Index 5 - Current/Volt Fractional Delay for 24KHz */
	0, 0, 2, /* Index 6 - Current/Volt Fractional Delay for 32KHz */
	0, 0, 2, /* Index 7 - Current/Volt Fractional Delay for 44KHz */
	0, 0, 3  /* Index 8 - Current/Volt Fractional Delay for 48KHz */
};

static int tfa9897_dsp_write_vsfwdelay_table(struct tfa98xx *tfa98xx)
{
	return tfa98xx_dsp_set_param(tfa98xx, MODULE_FRAMEWORK,
				FW_PARAM_SET_CURRENT_DELAY,
				sizeof(vsfwdelay_table),
				vsfwdelay_table);
}

/*
 * The int24 values for the fracdelay table
 * For now applicable only for 8 and 48 kHz
 */
static u8 cvfracdelay_table[] = {
	0, 0, 51, /* Index 0 - Current/Volt Fractional Delay for 8KHz  */
	0, 0,  0, /* Index 1 - Current/Volt Fractional Delay for 11KHz */
	0, 0,  0, /* Index 2 - Current/Volt Fractional Delay for 12KHz */
	0, 0, 38, /* Index 3 - Current/Volt Fractional Delay for 16KHz */
	0, 0, 34, /* Index 4 - Current/Volt Fractional Delay for 22KHz */
	0, 0, 33, /* Index 5 - Current/Volt Fractional Delay for 24KHz */
	0, 0, 11, /* Index 6 - Current/Volt Fractional Delay for 32KHz */
	0, 0,  2, /* Index 7 - Current/Volt Fractional Delay for 44KHz */
	0, 0, 62  /* Index 8 - Current/Volt Fractional Delay for 48KHz */
};

static int tfa9897_dsp_write_cvfracdelay_table(struct tfa98xx *tfa98xx)
{
	return tfa98xx_dsp_set_param(tfa98xx, MODULE_FRAMEWORK,
				FW_PARAM_SET_CURFRAC_DELAY,
				sizeof(cvfracdelay_table),
				cvfracdelay_table);
}

/*
 * load the tables to the DSP, called after patch load is done
 */
int tfa98xx_dsp_write_tables(struct tfa98xx *tfa98xx)
{
	int ret = 0;

	if (tfa98xx->rev == REV_TFA9897) {
		ret = tfa9897_dsp_write_vsfwdelay_table(tfa98xx);
		pr_debug("tfa9897_dsp_write_vsfwdelay_table %d\n", ret);
		if (!ret) {
			ret = tfa9897_dsp_write_cvfracdelay_table(tfa98xx);
			pr_debug("tfa9897_dsp_write_cvfracdelay_table %d\n",
				 ret);
		}
	}

	return ret;
}

/*
 *  this will load the patch witch will implicitly start the DSP
 *   if no patch is available the DPS is started immediately
 */
static int tfaRunStartDSP(struct tfa98xx *tfa98xx)
{
	int ret;

	ret = tfaContWritePatch(tfa98xx);
	if (ret) {
		pr_err("Error, writing patch failed (%d)\n", ret);
		return ret;
	}

	ret = tfa98xx_dsp_write_tables(tfa98xx);

	return ret;
}

/*
 * Run the startup/init sequence and set ACS bit
 */
int tfaRunColdStartup(struct tfa98xx *tfa98xx)
{
	int ret;

	pr_debug("\n");

	ret = tfa98xx_startup(tfa98xx);
	pr_debug("tfa98xx_startup %d\n", ret);
	if (ret)
		return ret;

	/* force cold boot */
	ret = tfa98xx_coldboot(tfa98xx, 1); /* set ACS */
	pr_debug("tfa98xx_coldboot %d\n", ret);
	if (ret)
		return ret;

	ret = tfaRunStartDSP(tfa98xx);

	return ret;
}

/* write this fuction to keep PA working when it never be done calibration */
static int tfaRunSetCalibrateOnce(struct tfa98xx *tfa98xx)
{
    struct snd_soc_codec *codec = tfa98xx->codec;
    u16 mtp, status;
	int tries = 0;
	int err = 0;

	/* Read MTP Register */
	mtp = snd_soc_read(codec, TFA98XX_MTP);
	if ((mtp & TFA98XX_MTP_MTPOTC) == 0) {
		/* Setting sequence for MTPOTC one time calibration */
		snd_soc_write(codec, 0x0b, 0x5A);
		snd_soc_write(codec, TFA98XX_MTP, 0x01);
		snd_soc_write(codec, 0x62, 0x800);

		for (tries = 0; tries < TFA98XX_WAITRESULT_NTRIES; tries++) {
			msleep_interruptible(10);
			/* Check if MTP is still busy */
			status = snd_soc_read(codec, TFA98XX_STATUSREG);
			if (!(status & TFA98XX_STATUSREG_MTPB_MSK))
				break;
		}
		snd_soc_write(codec, 0x0b, 0x0);
		if (tries == TFA98XX_WAITRESULT_NTRIES) {
			pr_err("Wait for MTPB timedout\n");
			return -ETIMEDOUT;
		}
	}
	return err;
}


static int coldboot;
module_param(coldboot, int, S_IRUGO | S_IWUSR);

/*
 * Start the maximus speakerboost algorithm this implies a full system
 * startup when the system was not already started.
 */
int tfaRunSpeakerBoost(struct tfa98xx *tfa98xx, int force)
{
	int ret = 0;

	pr_debug("force: %d\n", force);

	if (force) {
		ret = tfaRunColdStartup(tfa98xx);
		if (ret)
			return ret;
		/* DSP is running now */
	}

	if (force || tfa98xx_is_coldboot(tfa98xx)) {
		int done;

		pr_debug("coldstart%s\n", force ? " (forced)" : "");

		/* in case of force CF already runnning */
		if (!force) {
			ret = tfa98xx_startup(tfa98xx);
			if (ret) {
				pr_err("tfa98xx_startup %d\n", ret);
				return ret;
			}

			/* load patch and start the DSP */
			ret = tfaRunStartDSP(tfa98xx);
		}

		/*
		 * DSP is running now
		 *   NOTE that ACS may be active
		 *   no DSP reset/sample rate may be done until configured
		 *   (SBSL)
		 */

		/* Set calibrate MTPOTC to 1 if it's not calibrated */
		ret = tfaRunSetCalibrateOnce(tfa98xx);

		/* soft mute */
		ret = tfa98xx_set_mute(tfa98xx, Tfa98xx_Mute_Digital);
		if (ret) {
			pr_err("Tfa98xx_SetMute error: %d\n", ret);
			return ret;
		}

		/*
		 * For the first configuration the DSP expects at least
		 * the speaker, config and a preset.
		 * Therefore all files from the device list as well as the file
		 * from the default profile are loaded before SBSL is set.
		 *
		 * Note that the register settings were already done before
		 * loading the patch
		 *
		 * write all the files from the device list (typically spk and
		 * config)
		 */
		ret = tfaContWriteFiles(tfa98xx);
		if (ret) {
			pr_err("tfaContWriteFiles error: %d\n", ret);
			return ret;
		}

		/*
		 * write all the files from the profile list (typically preset)
		 * use volumestep 0
		 */
		tfaContWriteFilesProf(tfa98xx, tfa98xx->profile_current, 0);

		/* tell DSP it's loaded SBSL = 1 */
		ret = tfa98xx_set_configured(tfa98xx);
		if (ret) {
			pr_err("tfa98xx_set_configured error: %d\n", ret);
			return ret;
		}

		/* await calibration, this should return ok */
		tfa98xx_wait_calibration(tfa98xx, &done);
		if (!done) {
			pr_err("Calibration not done!\n");
			/*
			 * Don't return on timeout, otherwise we cannot power
			 * down the DSP
			 */
		} else {
			pr_debug("Calibration done\n");
		}

	} else {
		/* already warm, so just pwr on */
		ret = tfa98xx_dsp_power_up(tfa98xx);
	}



	if (!tfaIsCalibrated(tfa98xx)) {
		pr_err("Not calibrated, power down devcie\n");
		ret = tfa98xx_powerdown(tfa98xx, 1);
		if (ret)
			return ret;

		return -EINVAL;
	}


	tfa98xx_unmute(tfa98xx);

	return ret;
}

int tfa98xx_dsp_start(struct tfa98xx *tfa98xx, int next_profile, int vstep)
{
	int forcecoldboot = coldboot;
	int active_profile;
	int active_vstep;
	int ret = 0;

	if (!tfa98xx->profile_count || !tfa98xx->profiles)
		return -EINVAL;


	if (tfa98xx->dsp_init == TFA98XX_DSP_INIT_RECOVER) {
		pr_warn("Restart for recovery\n");
		forcecoldboot = 1;
	}

	/*
	 * set the current profile and vsteps
	 * in case they get written during cold start
	 */

	active_profile = tfa98xx->profile_current;
	tfa98xx->profile_current = next_profile;

	active_vstep = tfa98xx->vstep_current;
	tfa98xx->vstep_current = vstep;

	pr_debug("Starting device\n");
	if (forcecoldboot || tfa98xx_is_coldboot(tfa98xx)) {
		/* enable I2S output */
		ret = tfa98xx_aec_output(tfa98xx, 1);
		if (ret)
			return ret;
		/* cold start up without unmute*/
		if (tfaRunSpeakerBoost(tfa98xx, forcecoldboot))
			return -EINVAL;
	} else {
		/* was it not done already */
		if (next_profile != active_profile) {
			ret = tfaContWriteProfile(tfa98xx, next_profile, vstep);
			if (ret) /* if error, set to original profile*/
				tfa98xx->profile_current = active_profile;
		} else {
			if (tfa98xx_is_pwdn(tfa98xx)) {
				/* enable I2S output */
				ret = tfa98xx_aec_output(tfa98xx, 1);
				if (ret)
					return ret;
				ret = tfaRunSpeakerBoost(tfa98xx, 0);
			}
			if (ret)
				return -EINVAL;

			if (vstep != active_vstep) {
				ret = tfaContWriteFilesVstep(tfa98xx,
							     next_profile,
							     vstep);
				if (ret)
					tfa98xx->vstep_current = active_vstep;
			}
		}
	}

	tfa98xx_unmute(tfa98xx);

	return ret;
}

int tfa98xx_dsp_stop(struct tfa98xx *tfa98xx)
{
	int ret = 0;
	u16 val = 0;

	pr_debug("Stopping device [%s]\n", tfa98xx->fw.name);

	/* tfaRunSpeakerBoost implies unmute */
	/* mute + SWS wait */
	ret = tfa98xx_mute(tfa98xx);
	if (ret)
		return ret;

	val = snd_soc_read(tfa98xx->codec, TFA98XX_STATUSREG);
	pr_debug("tfa98xx_dsp_stop status = 0x%02x.\n", val);

	/* powerdown CF */
	ret = tfa98xx_powerdown(tfa98xx, 1);
	if (ret)
		return ret;

	/* disable I2S output */
	ret = tfa98xx_aec_output(tfa98xx, 0);

	return ret;
}
