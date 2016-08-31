/*
 * tfa98xx.c  --  codec driver for TFA98XX
 *
 * Copyright (C) 2014 Xiaomi Corporation
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * Author: Xiang Xiao <xiaoxiang@xiaomi.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#include "tfa98xx.h"

#define TFA98XX_STATUSREG_UP		(TFA98XX_STATUSREG_PLLS | \
					 TFA98XX_STATUSREG_CLKS | \
					 TFA98XX_STATUSREG_AREFS)
#define TFA98XX_STATUSREG_UP_MSK	(TFA98XX_STATUSREG_PLLS_MSK | \
					 TFA98XX_STATUSREG_MTPB_MSK | \
					 TFA98XX_STATUSREG_CLKS_MSK | \
					 TFA98XX_STATUSREG_AREFS_MSK)

#define TFA98XX_STATUSREG_ERR1		(TFA98XX_STATUSREG_OCDS)
#define TFA98XX_STATUSREG_ERR1_MSK	(TFA98XX_STATUSREG_OCDS_MSK)

#define TFA98XX_STATUSREG_ERR2		(TFA98XX_STATUSREG_ACS | \
					 TFA98XX_STATUSREG_WDS)
#define TFA98XX_STATUSREG_ERR2_MSK	(TFA98XX_STATUSREG_ACS_MSK | \
					 TFA98XX_STATUSREG_WDS_MSK)

#define TFA98XX_I2SCTRL_MSB_J		(2 << TFA98XX_I2SREG_I2SF_POS)
#define TFA98XX_I2SCTRL_PHILIPS		(3 << TFA98XX_I2SREG_I2SF_POS)
#define TFA98XX_I2SCTRL_LSB_J_16	(4 << TFA98XX_I2SREG_I2SF_POS)
#define TFA98XX_I2SCTRL_LSB_J_18	(5 << TFA98XX_I2SREG_I2SF_POS)
#define TFA98XX_I2SCTRL_LSB_J_20	(6 << TFA98XX_I2SREG_I2SF_POS)
#define TFA98XX_I2SCTRL_LSB_J_24	(7 << TFA98XX_I2SREG_I2SF_POS)

#define TFA98XX_I2SCTRL_RATE_08000	(0 << TFA98XX_I2SREG_I2SSR_POS)
#define TFA98XX_I2SCTRL_RATE_11025	(1 << TFA98XX_I2SREG_I2SSR_POS)
#define TFA98XX_I2SCTRL_RATE_12000	(2 << TFA98XX_I2SREG_I2SSR_POS)
#define TFA98XX_I2SCTRL_RATE_16000	(3 << TFA98XX_I2SREG_I2SSR_POS)
#define TFA98XX_I2SCTRL_RATE_22050	(4 << TFA98XX_I2SREG_I2SSR_POS)
#define TFA98XX_I2SCTRL_RATE_24000	(5 << TFA98XX_I2SREG_I2SSR_POS)
#define TFA98XX_I2SCTRL_RATE_32000	(6 << TFA98XX_I2SREG_I2SSR_POS)
#define TFA98XX_I2SCTRL_RATE_44100	(7 << TFA98XX_I2SREG_I2SSR_POS)
#define TFA98XX_I2SCTRL_RATE_48000	(8 << TFA98XX_I2SREG_I2SSR_POS)

#define TFA98XX_MUTE_OFF		0
#define TFA98XX_MUTE_DIGITAL		1
#define TFA98XX_MUTE_AMPLIFIER		2

#define TFA98XX_MODULE_SPEAKERBOOST	(128 + 1)
#define TFA98XX_MODULE_BIQUADFILTERBANK	(128 + 2)

#define TFA98XX_PARAM_SET_LSMODEL	0x06
#define TFA98XX_PARAM_SET_PRESET	0x0D
#define TFA98XX_PARAM_SET_CONFIG	0x0E

#define TFA98XX_FW_BOOT			0
#define TFA98XX_FW_ROM			1
#define TFA98XX_FW_SPEAKER		2
#define TFA98XX_FW_CONFIG		3
#define TFA98XX_FW_PRESET		4
#define TFA98XX_FW_EQUALIZER		5
#define TFA98XX_FW_NUMBER		6

struct tfa98xx_priv {
	struct mutex fw_lock;
	bool fw_chg[TFA98XX_FW_NUMBER];
	char fw_name[TFA98XX_FW_NUMBER][512];
	const struct firmware *fw[TFA98XX_FW_NUMBER];
	struct workqueue_struct *workqueue;
	struct delayed_work monitor_work;
	struct delayed_work download_work;
	struct snd_soc_codec *codec;
	unsigned int pilot_tone;
	unsigned int reg_addr;
	unsigned int fmt;
	bool dsp_crash;
	bool recalib;
};

static int tfa98xx_bulk_read(struct snd_soc_codec *codec,
			     unsigned int reg_,
			     void *data, size_t len)
{
	struct i2c_client *client = to_i2c_client(codec->dev);
	struct i2c_msg msgs[2];
	u8 reg = reg_;
	int ret;

	msgs[0].addr  = client->addr;
	msgs[0].flags = client->flags;
	msgs[0].len   = 1;
	msgs[0].buf   = &reg;
	msgs[1].addr  = client->addr;
	msgs[1].flags = client->flags | I2C_M_RD;
	msgs[1].len   = len;
	msgs[1].buf   = data;

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret > 0)
		ret = 0;

	return ret;
}

static int tfa98xx_bulk_write(struct snd_soc_codec *codec,
			      unsigned int reg,
			      const void *data, size_t len)
{
	struct i2c_client *client = to_i2c_client(codec->dev);
	struct i2c_msg msg;
	u8 buf[len + 1];
	int ret;

	buf[0] = reg;
	memcpy(buf + 1, data, len);

	msg.addr  = client->addr;
	msg.flags = client->flags;
	msg.len   = len + 1;
	msg.buf   = buf;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

static unsigned int tfa98xx_read(struct snd_soc_codec *codec, unsigned int reg)
{
	unsigned int val;
	u8 buf[3];
	int ret;

	if (reg >= codec->driver->reg_cache_size ||
	    snd_soc_codec_volatile_register(codec, reg) ||
	    codec->cache_bypass) {
		if (codec->cache_only)
			return -EINVAL;

		ret = tfa98xx_bulk_read(codec, reg, buf, reg == TFA98XX_CF_MEM ? 3 : 2);
		if (ret < 0)
			return ret;
		else if (reg == TFA98XX_CF_MEM)
			return (buf[0] << 16) | (buf[1] << 8) | buf[2];
		else
			return (buf[0] << 8) | buf[1];
	}

	ret = snd_soc_cache_read(codec, reg, &val);
	if (ret < 0)
		return ret;
	return val;
}

static int tfa98xx_write(struct snd_soc_codec *codec, unsigned int reg,
			 unsigned int value)
{
	u8 buf[3];
	size_t sz;
	int ret;

	if (!snd_soc_codec_volatile_register(codec, reg) &&
	    reg < codec->driver->reg_cache_size &&
	    !codec->cache_bypass) {
		ret = snd_soc_cache_write(codec, reg, value);
		if (ret < 0)
			return ret;
	}

	if (codec->cache_only) {
		codec->cache_sync = 1;
		return 0;
	}

	if (reg == TFA98XX_CF_MEM) {
		buf[0] = value >> 16;
		buf[1] = value >> 8;
		buf[2] = value >> 0;
		sz     = 3;
	} else {
		buf[0] = value >> 8;
		buf[1] = value >> 0;
		sz     = 2;
	}

	return tfa98xx_bulk_write(codec, reg, buf, sz);
}

static unsigned int tfa98xx_read_dsp(struct snd_soc_codec *codec, unsigned int reg)
{
	int ret;

	ret = snd_soc_write(codec, TFA98XX_CF_CONTROLS, (reg >> 15) & 0x0E);
	if (ret < 0)
		return ret;

	ret = snd_soc_write(codec, TFA98XX_CF_MAD, reg & 0xFFFF);
	if (ret < 0)
		return ret;

	return snd_soc_read(codec, TFA98XX_CF_MEM);
}

static int tfa98xx_reset(struct snd_soc_codec *codec)
{
	struct i2c_client *client = to_i2c_client(codec->dev);
	int ret;

	/* use the low level function to bypass the codec cache */
	ret = i2c_smbus_write_word_swapped(client, TFA98XX_SYS_CTRL, TFA98XX_SYS_CTRL_I2CR);
	if (ret < 0)
		return ret;

	/* apply the current setting after reset */
	codec->cache_sync = 1;
	ret = snd_soc_cache_sync(codec);
	if (ret < 0)
		return ret;

	return ret;
}

static int tfa98xx_reset_dsp(struct snd_soc_codec *codec)
{
	unsigned int sense4;
	int ret;

	/* temporarily disable clock gating when dsp reset */
	sense4 = snd_soc_read(codec, TFA98XX_CURRENTSENSE4);
	ret = snd_soc_write(codec, TFA98XX_CURRENTSENSE4,
			sense4 | TFA9897_CURRENTSENSE4_2);
	if (ret < 0)
		return ret;

	ret = snd_soc_update_bits(codec, TFA98XX_CF_CONTROLS,
			TFA98XX_CF_CONTROLS_RST_MSK, TFA98XX_CF_CONTROLS_RST);
	/* clock gating restore */
	snd_soc_write(codec, TFA98XX_CURRENTSENSE4, sense4);

	return ret;
}

static int tfa98xx_wait_clock(struct snd_soc_codec *codec)
{
	unsigned int status;
	int tries;

	for (tries = 10; tries > 0; tries--) {
		status = snd_soc_read(codec, TFA98XX_STATUSREG);
		if ((status & TFA98XX_STATUSREG_UP_MSK) == TFA98XX_STATUSREG_UP)
			break;
		mdelay(1);
	}
	if (tries == 0) {
		dev_err(codec->dev, "Fail to sync i2s clock on time(%x)\n", status);
		return -EBUSY;
	}

	return 0;
}

static int tfa98xx_power(struct snd_soc_codec *codec, bool on)
{
	int ret;

	ret = snd_soc_update_bits_locked(codec, TFA98XX_SYS_CTRL,
			TFA98XX_SYS_CTRL_PWDN_MSK, on ? 0 : TFA98XX_SYS_CTRL_PWDN);
	if (ret < 0)
		return ret;
	if (on)
		ret = tfa98xx_wait_clock(codec);
	return ret;
}

static int tfa98xx_mute(struct snd_soc_codec *codec, int mute)
{
	unsigned int status;
	int ret = 0, tries;

	switch (mute) {
	case TFA98XX_MUTE_OFF:
		ret = snd_soc_update_bits_locked(codec,
				TFA98XX_AUDIO_CTR, TFA98XX_AUDIO_CTR_CFSM_MSK, 0);
		if (ret < 0)
			return ret;
		ret = snd_soc_update_bits_locked(codec, TFA98XX_SYS_CTRL,
				TFA98XX_SYS_CTRL_DCA_MSK | TFA98XX_SYS_CTRL_AMPE_MSK,
				TFA98XX_SYS_CTRL_DCA | TFA98XX_SYS_CTRL_AMPE);
		if (ret < 0)
			return ret;
		break;
	case TFA98XX_MUTE_DIGITAL:
		ret = snd_soc_update_bits_locked(codec, TFA98XX_AUDIO_CTR,
				TFA98XX_AUDIO_CTR_CFSM_MSK, TFA98XX_AUDIO_CTR_CFSM);
		if (ret < 0)
			return ret;
		ret = snd_soc_update_bits_locked(codec, TFA98XX_SYS_CTRL,
				TFA98XX_SYS_CTRL_DCA_MSK | TFA98XX_SYS_CTRL_AMPE_MSK,
				TFA98XX_SYS_CTRL_AMPE);
		if (ret < 0)
			return ret;
		break;
	case TFA98XX_MUTE_AMPLIFIER:
		ret = snd_soc_update_bits_locked(codec,
				TFA98XX_AUDIO_CTR, TFA98XX_AUDIO_CTR_CFSM_MSK, 0);
		if (ret < 0)
			return ret;
		ret = snd_soc_update_bits_locked(codec, TFA98XX_SYS_CTRL,
				TFA98XX_SYS_CTRL_DCA_MSK | TFA98XX_SYS_CTRL_AMPE_MSK,
				0);
		if (ret < 0)
			return ret;

		/* wait for amplifier to stop switching */
		for (tries = 10; tries > 0; tries--) {
			status = snd_soc_read(codec, TFA98XX_STATUSREG);
			if (!(status & TFA98XX_STATUSREG_SWS_MSK))
				break;
			msleep(10);
		}
		if (tries == 0)
			dev_warn(codec->dev, "Fail to stop amplifier on time\n");
		break;
	}

	return ret;
}

static int tfa98xx_enable_otc(struct snd_soc_codec *codec, bool recalib)
{
	unsigned int mtp, status;
	int ret = 0, tries;

	mtp = snd_soc_read(codec, TFA98XX_MTP_SPKR_CAL);
	if (recalib || !(mtp & TFA98XX_MTP_SPKR_CAL_MTPOTC_MSK)) {
		ret = snd_soc_write(codec, TFA98XX_MTPKEY2_REG, 0x5A);
		if (ret < 0) {
			dev_err(codec->dev, "Fail to unlock key(%d)\n", ret);
			return ret;
		}

		mtp &= ~TFA98XX_MTP_SPKR_CAL_MTPEX_MSK;
		mtp |= TFA98XX_MTP_SPKR_CAL_MTPOTC_MSK;
		ret = snd_soc_write(codec, TFA98XX_MTP_SPKR_CAL, mtp);
		if (ret < 0) {
			dev_err(codec->dev, "Fail to update mtp(%d)\n", ret);
			return ret;
		}

		ret = snd_soc_update_bits(codec, TFA98XX_MTP_CTRL_REG3,
				TFA98XX_MTP_CTRL_REG3_CIMTP_MSK, TFA98XX_MTP_CTRL_REG3_CIMTP);
		if (ret < 0) {
			dev_err(codec->dev, "Fail to enable mtp copy(%d)\n", ret);
			return ret;
		}

		for (tries = 10; tries > 0; tries--) {
			status = snd_soc_read(codec, TFA98XX_STATUSREG);
			if (!(status & TFA98XX_STATUSREG_MTPB_MSK))
				break;
			msleep(100);
		}
		if (tries == 0) {
			dev_err(codec->dev, "Fail to copy mtp on time\n");
			return -EBUSY;
		}
	}

	return ret;
}

static int tfa98xx_download_patch(struct snd_soc_codec *codec,
				  const struct firmware *fw)
{
	int ret = -EINVAL;
	size_t i, sz;

	/* start from 6 to skip the patch header */
	for (i = 6; i < fw->size; i += sz) {
		sz  = fw->data[i++];
		sz += fw->data[i++] << 8;

		if (i + sz > fw->size) {
			dev_err(codec->dev,
				"Invalid patch format(%x, %x)\n", i, sz);
			return -EINVAL;
		}

		dev_dbg(codec->dev,
			"Download patch offset = %x, size = %x\n", i, sz);

		ret = snd_soc_bulk_write_raw(codec,
				fw->data[i], &fw->data[i + 1], sz - 1);
		if (ret < 0) {
			dev_err(codec->dev,
				"Fail to download patch(%x, %x, %d)\n", i, sz, ret);
			return ret;
		}
	}

	return ret;
}

static int tfa98xx_upload_file(struct snd_soc_codec *codec,
				int module_id, int param_id,
				void *data, size_t size)
{
	unsigned int status;
	int ret, tries;
	u8 buf[7];

	/* step 1: write the header */
	buf[0] = 0x00; /* CF_CONTROLS: req=0, int=0, aif=0, dmem=1(XMEM), rst_dsp=0 */
	buf[1] = 0x02;
	buf[2] = 0x00; /* CF_MAD: addr=1(ID) */
	buf[3] = 0x01;
	buf[4] = 0x00; /* CF_MEM */
	buf[5] = module_id;
	buf[6] = param_id;

	ret = snd_soc_bulk_write_raw(codec, TFA98XX_CF_CONTROLS, buf, 7);
	if (ret < 0) {
		dev_err(codec->dev, "Fail to write data header(%d)\n", ret);
		return ret;
	}

	/* step 2: wake up dsp and wait done */
	/* CF_CONTROLS: req=1, int=1, aif=0, dmem=1(XMEM), rst_dsp=0 */
	ret = snd_soc_write(codec, TFA98XX_CF_CONTROLS, 0x0112);
	if (ret < 0) {
		dev_err(codec->dev, "Fail to wake up dsp(%d)\n", ret);
		return ret;
	}

	for (tries = 10; tries > 0; tries--) {
		status = snd_soc_read(codec, TFA98XX_CF_STATUS);
		if ((status & TFA98XX_CF_STATUS_ACK_MSK) == 0x0100)
			break;
		mdelay(1);
	}
	if (tries == 0) {
		dev_err(codec->dev, "Fail to response on time\n");
		return -EBUSY;
	}

	/* step 3: check dsp result */
	buf[0] = 0x00; /* CF_CONTROLS: req=0, int=0, aif=0, dmem=1(XMEM), rst_dsp=0 */
	buf[1] = 0x02;
	buf[2] = 0x00; /* CF_MAD: addr=0(STATUS) */
	buf[3] = 0x00;

	ret = snd_soc_bulk_write_raw(codec, TFA98XX_CF_CONTROLS, buf, 4);
	if (ret < 0) {
		dev_err(codec->dev, "Fail to write status header(%d)\n", ret);
		return ret;
	}

	status = snd_soc_read(codec, TFA98XX_CF_MEM);
	if (status != 0) {
		dev_err(codec->dev, "Fail to upload file(%d)\n", status);
		return -EINVAL;
	}

	/* step 4: read the data */
	ret = snd_soc_write(codec, TFA98XX_CF_MAD, 0x0002);
	if (ret < 0) {
		dev_err(codec->dev, "Fail to write memory address(%d)\n", ret);
		return ret;
	}

	ret = tfa98xx_bulk_read(codec, TFA98XX_CF_MEM, data, size);
	if (ret < 0) {
		dev_err(codec->dev, "Fail to read data(%d)\n", ret);
		return ret;
	}

	return ret;
}

static int tfa98xx_download_file(struct snd_soc_codec *codec,
				 int module_id, int param_id,
				 const void *data, size_t size)
{
	unsigned int status;
	int ret, tries;
	u8 buf[7];

	/* step 1: write the header */
	buf[0] = 0x00; /* CF_CONTROLS: req=0, int=0, aif=0, dmem=1(XMEM), rst_dsp=0 */
	buf[1] = 0x02;
	buf[2] = 0x00; /* CF_MAD: addr=1(ID) */
	buf[3] = 0x01;
	buf[4] = 0x00; /* CF_MEM */
	buf[5] = module_id;
	buf[6] = param_id;

	ret = snd_soc_bulk_write_raw(codec, TFA98XX_CF_CONTROLS, buf, 7);
	if (ret < 0) {
		dev_err(codec->dev, "Fail to write data header(%d)\n", ret);
		return ret;
	}

	/* step 2: write the data */
	ret = snd_soc_bulk_write_raw(codec, TFA98XX_CF_MEM, data, size);
	if (ret < 0) {
		dev_err(codec->dev, "Fail to write data(%d)\n", ret);
		return ret;
	}

	/* step 3: wake up dsp and wait done */
	/* CF_CONTROLS: req=1, int=1, aif=0, dmem=1(XMEM), rst_dsp=0 */
	ret = snd_soc_write(codec, TFA98XX_CF_CONTROLS, 0x0112);
	if (ret < 0) {
		dev_err(codec->dev, "Fail to wake up dsp(%d)\n", ret);
		return ret;
	}

	for (tries = 10; tries > 0; tries--) {
		status = snd_soc_read(codec, TFA98XX_CF_STATUS);
		if ((status & TFA98XX_CF_STATUS_ACK_MSK) == 0x0100)
			break;
		mdelay(1);
	}
	if (tries == 0) {
		dev_err(codec->dev, "Fail to response on time\n");
		return -EBUSY;
	}

	/* step 4: check dsp result */
	buf[0] = 0x00; /* CF_CONTROLS: req=0, int=0, aif=0, dmem=1(XMEM), rst_dsp=0 */
	buf[1] = 0x02;
	buf[2] = 0x00; /* CF_MAD: addr=0(STATUS) */
	buf[3] = 0x00;

	ret = snd_soc_bulk_write_raw(codec, TFA98XX_CF_CONTROLS, buf, 4);
	if (ret < 0) {
		dev_err(codec->dev, "Fail to write status header(%d)\n", ret);
		return ret;
	}

	status = snd_soc_read(codec, TFA98XX_CF_MEM);
	if (status != 0) {
		dev_err(codec->dev, "Fail to download file(%d)\n", status);
		return -EINVAL;
	}

	return ret;
}

static int tfa98xx_download_and_verify_file(struct snd_soc_codec *codec,
					    int module_id, int param_id,
					    const void *data, size_t size)
{
	u8 buf[size];
	int ret;

	ret = tfa98xx_download_file(codec, module_id, param_id, data, size);
	if (ret < 0)
		return ret;
	ret = tfa98xx_upload_file(codec, module_id, param_id, buf, size);
	if (ret < 0)
		return ret;
	if (memcmp(data, buf, size) != 0) {
		dev_err(codec->dev, "Write then read mismatch:\n");
		print_hex_dump(KERN_ERR, "WR ",
			DUMP_PREFIX_ADDRESS, 16, 1, data, size, true);
		print_hex_dump(KERN_ERR, "RD ",
			DUMP_PREFIX_ADDRESS, 16, 1, buf, size, true);
		return -EINVAL;
	}

	return ret;
}

static void tfa98xx_download(struct work_struct *work)
{
	struct tfa98xx_priv *tfa98xx;
	struct snd_soc_codec *codec;
	struct delayed_work *dwork;
	unsigned int mtp;
	int ret = 0, id, tries;

	dwork = to_delayed_work(work);
	tfa98xx = container_of(dwork, struct tfa98xx_priv, download_work);
	codec = tfa98xx->codec;

	mutex_lock(&tfa98xx->fw_lock);

	/* wait the clock stable */
	if (codec->dapm.bias_level == SND_SOC_BIAS_OFF)
		goto unlock; /* impossible to get the clock */

	ret = tfa98xx_wait_clock(codec);
	if (ret < 0)
		goto unlock;

	ret = tfa98xx_enable_otc(codec, tfa98xx->recalib);
	if (ret < 0)
		goto unlock;
	tfa98xx->recalib = false;

	ret = tfa98xx_mute(codec, TFA98XX_MUTE_DIGITAL);
	if (ret < 0)
		goto unlock;

	for (id = 0; id < TFA98XX_FW_NUMBER; id++) {
		if (tfa98xx->fw[id] == NULL)
			goto unmute;
		if (!tfa98xx->fw_chg[id])
			continue;

		dev_info(codec->dev,
			"Download firmware %s\n", tfa98xx->fw_name[id]);

		switch (id) {
		case TFA98XX_FW_BOOT:
			ret = tfa98xx_reset_dsp(codec);
			if (ret < 0)
				goto unmute;
			ret = tfa98xx_wait_clock(codec);
			if (ret < 0)
				goto unmute;
			ret = tfa98xx_download_patch(codec, tfa98xx->fw[id]);
			if (ret < 0)
				goto unmute;
			/* reload rom patch */
			tfa98xx->fw_chg[TFA98XX_FW_ROM] = true;
			break;
		case TFA98XX_FW_ROM:
			ret = tfa98xx_download_patch(codec, tfa98xx->fw[id]);
			if (ret < 0)
				goto unmute;
			/* reload all setting */
			tfa98xx->fw_chg[TFA98XX_FW_SPEAKER]   = true;
			tfa98xx->fw_chg[TFA98XX_FW_CONFIG]    = true;
			tfa98xx->fw_chg[TFA98XX_FW_PRESET]    = true;
			tfa98xx->fw_chg[TFA98XX_FW_EQUALIZER] = true;
			break;
		case TFA98XX_FW_SPEAKER:
			ret = tfa98xx_download_and_verify_file(codec,
						    TFA98XX_MODULE_SPEAKERBOOST,
						    TFA98XX_PARAM_SET_LSMODEL,
						    tfa98xx->fw[id]->data,
						    tfa98xx->fw[id]->size);
			if (ret < 0)
				goto unmute;
			break;
		case TFA98XX_FW_CONFIG:
			ret = tfa98xx_download_and_verify_file(codec,
						    TFA98XX_MODULE_SPEAKERBOOST,
						    TFA98XX_PARAM_SET_CONFIG,
						    tfa98xx->fw[id]->data,
						    tfa98xx->fw[id]->size);
			if (ret < 0)
				goto unmute;
			break;
		case TFA98XX_FW_PRESET:
			ret = tfa98xx_download_and_verify_file(codec,
						    TFA98XX_MODULE_SPEAKERBOOST,
						    TFA98XX_PARAM_SET_PRESET,
						    tfa98xx->fw[id]->data,
						    tfa98xx->fw[id]->size);
			if (ret < 0)
				goto unmute;
			break;
		case TFA98XX_FW_EQUALIZER:
			ret = tfa98xx_download_and_verify_file(codec,
						    TFA98XX_MODULE_BIQUADFILTERBANK,
						    0,
						    tfa98xx->fw[id]->data,
						    tfa98xx->fw[id]->size);
			if (ret < 0)
				goto unmute;
			break;
		}

		/* done, clear dirty flag */
		tfa98xx->fw_chg[id] = false;
	}

	/* signal dsp to load the setting */
	mtp = snd_soc_read(codec, TFA98XX_MTP_SPKR_CAL);
	if (!(mtp & TFA98XX_MTP_SPKR_CAL_MTPEX_MSK))
		dev_info(codec->dev, "Start one time calibration\n");
	else
		dev_info(codec->dev, "Load the calibration value from mtp\n");

	snd_soc_update_bits_locked(codec, TFA98XX_SYS_CTRL,
		TFA98XX_SYS_CTRL_SBSL_MSK, TFA98XX_SYS_CTRL_SBSL);

	for (tries = 10; tries > 0; tries--) {
		mtp = snd_soc_read(codec, TFA98XX_MTP_SPKR_CAL);
		if (mtp & TFA98XX_MTP_SPKR_CAL_MTPEX_MSK)
			break;
		msleep(100);
	}

	if (tries == 0)
		dev_warn(codec->dev, "Fail to calibrate on time\n");
	else
		dev_info(codec->dev, "Finish one time calibration\n");

unmute:
	tfa98xx_mute(codec, TFA98XX_MUTE_OFF);
unlock:
	mutex_unlock(&tfa98xx->fw_lock);

	/* retry in a late time if fail */
	if (ret < 0) {
		queue_delayed_work(tfa98xx->workqueue,
			&tfa98xx->download_work, msecs_to_jiffies(10));
	}
}

static bool tfa98xx_start_download(struct tfa98xx_priv *tfa98xx, bool force)
{
	bool sched = force;
	int id;

	mutex_lock(&tfa98xx->fw_lock);
	if (force) { /* re-download all firmware */
		for (id = 0; id < TFA98XX_FW_NUMBER; id++)
			tfa98xx->fw_chg[id] = true;
	} else { /* check the pending change exist */
		for (id = 0; id < TFA98XX_FW_NUMBER; id++) {
			if (tfa98xx->fw_chg[id]) {
				sched = true;
				break;
			}
		}
	}
	mutex_unlock(&tfa98xx->fw_lock);

	if (sched)
		queue_delayed_work(tfa98xx->workqueue, &tfa98xx->download_work, 0);
	return sched;
}

static void tfa98xx_stop_download(struct tfa98xx_priv *tfa98xx)
{
	cancel_delayed_work_sync(&tfa98xx->download_work);
}

static void tfa98xx_start_monitor(struct tfa98xx_priv *tfa98xx)
{
	queue_delayed_work(tfa98xx->workqueue,
		&tfa98xx->monitor_work, msecs_to_jiffies(5000));
}

static void tfa98xx_stop_monitor(struct tfa98xx_priv *tfa98xx)
{
	cancel_delayed_work_sync(&tfa98xx->monitor_work);
}

static int tfa98xx_check_error(struct snd_soc_codec *codec)
{
	unsigned int status;

	status = snd_soc_read(codec, TFA98XX_STATUSREG);
	if (status & TFA98XX_STATUSREG_ERR2_MSK)
		return 2;
	else if (status & TFA98XX_STATUSREG_ERR1_MSK)
		return 1;

	status = tfa98xx_read_dsp(codec, 0x666);
	if (status == 0x7FFFFF)
		return 2;

	return 0;
}

static void tfa98xx_monitor(struct work_struct *work)
{
	struct tfa98xx_priv *tfa98xx;
	struct snd_soc_codec *codec;
	struct delayed_work *dwork;

	dwork = to_delayed_work(work);
	tfa98xx = container_of(dwork, struct tfa98xx_priv, monitor_work);
	codec = tfa98xx->codec;

	switch (tfa98xx_check_error(codec)) {
	case 2:
		dev_err(codec->dev, "Restart due to dsp crash\n");
		tfa98xx->dsp_crash = true; /* save crash info */
		tfa98xx->pilot_tone = tfa98xx_read_dsp(codec, 0x1029A);
		tfa98xx_reset(codec);
		tfa98xx_start_download(tfa98xx, true);
		break;
	case 1:
		dev_err(codec->dev, "Repower due to over condition\n");
		tfa98xx_power(codec, false);
		usleep_range(5000, 5000);
		tfa98xx_power(codec, true);
		break;
	}

	tfa98xx_start_monitor(tfa98xx);
}

static ssize_t tfa98xx_dsp_crash_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tfa98xx_priv *tfa98xx = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", tfa98xx->dsp_crash);
}

static ssize_t tfa98xx_crash_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tfa98xx_priv *tfa98xx = i2c_get_clientdata(client);
	unsigned long val;
	int ret;

	ret = strict_strtoul(buf, 0, &val);
	if (ret < 0)
		return ret;
	tfa98xx->dsp_crash = (val != 0);

	return count;
}
static DEVICE_ATTR(dsp_crash, 0664, tfa98xx_dsp_crash_show, tfa98xx_crash_store);


static ssize_t tfa98xx_pilot_tone_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tfa98xx_priv *tfa98xx = i2c_get_clientdata(client);

	return sprintf(buf, "0x%06X\n", tfa98xx->pilot_tone);
}
static DEVICE_ATTR(pilot_tone, 0444, tfa98xx_pilot_tone_show, NULL);

static int tfa98xx_probe(struct snd_soc_codec *codec)
{
	struct tfa98xx_priv *tfa98xx;
	int ret;

	tfa98xx = kzalloc(sizeof(struct tfa98xx_priv), GFP_KERNEL);
	if (tfa98xx == NULL) {
		dev_err(codec->dev, "Failed to alloc tfa98xx_priv\n");
		return -ENOMEM;
	}

	tfa98xx->codec = codec;
	mutex_init(&tfa98xx->fw_lock);

	INIT_DELAYED_WORK(&tfa98xx->monitor_work, tfa98xx_monitor);
	INIT_DELAYED_WORK(&tfa98xx->download_work, tfa98xx_download);

	codec->bulk_write_raw = tfa98xx_bulk_write;
	snd_soc_codec_set_drvdata(codec, tfa98xx);

	tfa98xx->workqueue = create_singlethread_workqueue(dev_name(codec->dev));
	if (tfa98xx->workqueue == NULL) {
		dev_err(codec->dev, "Failed to create workqueue\n");
		ret = -ENOMEM;
		goto wq_fail;
	}

	ret = tfa98xx_reset(codec);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to reset tf98xx(%d)\n", ret);
		goto reset_fail;
	}

	device_create_file(codec->dev, &dev_attr_dsp_crash);
	device_create_file(codec->dev, &dev_attr_pilot_tone);

	return ret;

reset_fail:
	destroy_workqueue(tfa98xx->workqueue);
wq_fail:
	kfree(tfa98xx);
	return ret;
}

static int tfa98xx_remove(struct snd_soc_codec *codec)
{
	struct tfa98xx_priv *tfa98xx = snd_soc_codec_get_drvdata(codec);
	int id;

	tfa98xx_stop_monitor(tfa98xx);
	tfa98xx_stop_download(tfa98xx);
	destroy_workqueue(tfa98xx->workqueue);

	for (id = 0; id < TFA98XX_FW_NUMBER; id++)
		release_firmware(tfa98xx->fw[id]);
	kfree(tfa98xx);

	return 0;
}

static int tfa98xx_reg_addr_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa98xx_priv *tfa98xx = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tfa98xx->reg_addr;
	return 0;
}

static int tfa98xx_reg_addr_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa98xx_priv *tfa98xx = snd_soc_codec_get_drvdata(codec);

	tfa98xx->reg_addr = ucontrol->value.integer.value[0];
	return 0;
}

static int tfa98xx_reg_value_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa98xx_priv *tfa98xx = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = snd_soc_read(codec, tfa98xx->reg_addr);
	return 0;
}

static int tfa98xx_reg_value_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa98xx_priv *tfa98xx = snd_soc_codec_get_drvdata(codec);

	return snd_soc_write(codec, tfa98xx->reg_addr, ucontrol->value.integer.value[0]);
}

static int tfa98xx_chsa_put(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	bool bypass = ucontrol->value.enumerated.item[0] < 2;
	int ret;

	ret = snd_soc_update_bits_locked(codec, TFA98XX_SYS_CTRL,
		TFA98XX_SYS_CTRL_CFE_MSK, bypass ? 0 : TFA98XX_SYS_CTRL_CFE);
	if (ret < 0)
		return ret;

	return snd_soc_put_enum_double(kcontrol, ucontrol);
}

static int tfa98xx_bsst_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int bsss, bsst;

	bsss   = snd_soc_read(codec, TFA98XX_AUDIO_CTR);
	bsss  &= TFA989X_AUDIO_CTR_BSSS_MSK;
	bsss >>= TFA989X_AUDIO_CTR_BSSS_POS;

	bsst   = snd_soc_read(codec, TFA98XX_BAT_PROT);
	bsst  &= TFA989X_BAT_PROT_BSST_MSK;
	bsst >>= TFA989X_BAT_PROT_BSST_POS;

	ucontrol->value.enumerated.item[0] = (bsst << 1) | bsss;
	return 0;
}

static int tfa98xx_bsst_put(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int bsss = ucontrol->value.enumerated.item[0] & 1;
	unsigned int bsst = ucontrol->value.enumerated.item[0] >> 1;
	int ret;

	ret = snd_soc_update_bits_locked(codec, TFA98XX_AUDIO_CTR,
			TFA989X_AUDIO_CTR_BSSS_MSK,
			bsss << TFA989X_AUDIO_CTR_BSSS_POS);
	if (ret < 0)
		return ret;

	ret = snd_soc_update_bits(codec, TFA98XX_BAT_PROT,
			TFA989X_BAT_PROT_BSST_MSK,
			bsst << TFA989X_BAT_PROT_BSST_POS);
	if (ret < 0)
		return ret;

	return ret;
}

static int tfa98xx_recalib_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa98xx_priv *tfa98xx = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tfa98xx->recalib;
	return 0;
}

static int tfa98xx_recalib_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa98xx_priv *tfa98xx = snd_soc_codec_get_drvdata(codec);

	tfa98xx->recalib = !!ucontrol->value.integer.value[0];
	if (tfa98xx->recalib)
		tfa98xx_start_download(tfa98xx, true);

	return 0;
}

static int tfa98xx_firmware_info(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = 512;
	return 0;
}

static int tfa98xx_firmware_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa98xx_priv *tfa98xx = snd_soc_codec_get_drvdata(codec);
	unsigned long id = kcontrol->private_value;

	strcpy(ucontrol->value.bytes.data, tfa98xx->fw_name[id]);
	return 0;
}

static int tfa98xx_firmware_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa98xx_priv *tfa98xx = snd_soc_codec_get_drvdata(codec);
	unsigned long id = kcontrol->private_value;
	const char *name = ucontrol->value.bytes.data;
	const struct firmware *fw;
	int ret;

	ret = request_firmware(&fw, name, codec->dev);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to request %s(%d)\n", name, ret);
		return ret;
	}

	mutex_lock(&tfa98xx->fw_lock);
	strcpy(tfa98xx->fw_name[id], name);
	if (tfa98xx->fw[id] == NULL || /* no firmware yet */
	    tfa98xx->fw[id]->size != fw->size || /* or change */
	    memcmp(tfa98xx->fw[id]->data, fw->data, fw->size)) {
		tfa98xx->fw_chg[id] = true;
		swap(tfa98xx->fw[id], fw);
	}
	mutex_unlock(&tfa98xx->fw_lock);

	/* download in the background to unblock
	   the caller to turn on the clock */
	tfa98xx_start_download(tfa98xx, false);

	release_firmware(fw);
	return ret;
}

#define TFA98XX_FIRMWARE(xname, id) \
	{ \
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
		.name = xname, \
		.info = tfa98xx_firmware_info, \
		.get = tfa98xx_firmware_get, \
		.put = tfa98xx_firmware_put, \
		.private_value = id, \
	}

static const char * const tfa98xx_chs12_text[] = {
	"Left", "Right", "Mono",
};
static const unsigned int tfa98xx_chs12_value[] = {
	1, 2, 3,
};
static const SOC_VALUE_ENUM_SINGLE_DECL(
	tfa98xx_chs12_enum, TFA98XX_I2SREG,
	TFA98XX_I2SREG_CHS12_POS, TFA98XX_I2SREG_CHS12_MAX,
	tfa98xx_chs12_text, tfa98xx_chs12_value);

static const char * const tfa98xx_chs3_text[] = {
	"Left", "Right",
};
static const SOC_ENUM_SINGLE_DECL(
	tfa98xx_chs3_enum, TFA98XX_I2SREG,
	TFA98XX_I2SREG_CHS3_POS, tfa98xx_chs3_text);

static const char * const tfa98xx_chsa_text[] = {
	"Left", "Right", "DSP",
};
static const SOC_ENUM_SINGLE_DECL(
	tfa98xx_chsa_enum, TFA98XX_I2SREG,
	TFA98XX_I2SREG_CHSA_POS, tfa98xx_chsa_text);

static const char * const tfa98xx_i2sdoc_text[] = {
	"DSP", "DATAI1", "DATAI2", "DATAI3",
};
static const SOC_ENUM_SINGLE_DECL(
	tfa98xx_i2sdoc_enum, TFA98XX_I2SREG,
	TFA9890_I2SREG_I2SDOC_POS, tfa98xx_i2sdoc_text);

static const char * const tfa98xx_bsst_text[] = {
	"2.73V", "2.99V", "2.83V", "3.09V", "2.93V", "3.19V", "3.03V", "3.29V",
	"3.13V", "3.39V", "3.23V", "3.49V", "3.33V", "3.59V", "3.43V", "3.69V",
	"3.53V", "3.79V", "3.63V", "3.89V", "3.73V", "3.99V", "3.83V", "4.09V",
	"3.93V", "4.19V", "4.03V", "4.29V", "4.13V", "4.39V", "4.23V", "4.49V",
};
static const SOC_ENUM_SINGLE_EXT_DECL(
	tfa98xx_bsst_enum, tfa98xx_bsst_text);

static const DECLARE_TLV_DB_SCALE(
	tfa98xx_vol_tlv, -12750, 50, 0);

static const char * const tfa98xx_dcvo_text[] = {
	"6.0V", "6.5V", "7.0V", "7.5V", "8.0V", "8.5V", "9.0V", "9.5V",
};
static const SOC_ENUM_SINGLE_DECL(
	tfa98xx_dcvo_enum, TFA98XX_DCDCBOOST,
	TFA98XX_DCDCBOOST_DCVO_POS, tfa98xx_dcvo_text);

static const char * const tfa98xx_dcmcc_text[] = {
	"0.5A", "1.0A", "1.4A", "1.9A", "2.4A", "2.9A", "3.3A", "3.8A",
};
static const SOC_ENUM_SINGLE_DECL(
	tfa98xx_dcmcc_enum, TFA98XX_DCDCBOOST,
	TFA98XX_DCDCBOOST_DCMCC_POS, tfa98xx_dcmcc_text);

static const char * const tfa98xx_isel_text[] = {
	"Input1", "Input2",
};
static const SOC_ENUM_SINGLE_DECL(
	tfa98xx_isel_enum, TFA98XX_SYS_CTRL,
	TFA98XX_SYS_CTRL_ISEL_POS, tfa98xx_isel_text);

static const char * const tfa98xx_dccv_text[] = {
	"0.7uH", "1uH", "1.5uH", "2.2uH",
};
static const SOC_ENUM_SINGLE_DECL(
	tfa98xx_dccv_enum, TFA98XX_SYS_CTRL,
	TFA98XX_SYS_CTRL_DCCV_POS, tfa98xx_dccv_text);

static const char * const tfa98xx_spkr_text[] = {
	"Auto", "4Omh", "6Omh", "8Omh",
};
static const SOC_ENUM_SINGLE_DECL(
	tfa98xx_spkr_enum, TFA98XX_I2S_SEL_REG,
	TFA98XX_I2S_SEL_REG_SPKR_POS, tfa98xx_spkr_text);

static const char * const tfa98xx_spkl_text[] = {
	"22uH", "27uH", "33uH", "39uH", "47uH", "56uH", "68uH", "82uH",
};
static const SOC_ENUM_SINGLE_DECL(
	tfa98xx_spkl_enum, TFA98XX_I2S_SEL_REG,
	TFA98XX_I2S_SEL_REG_SPKL_POS, tfa98xx_spkl_text);

static const char * const tfa98xx_dos_text[] = {
	"Current", "Gain", "AEC", "Voltage", "DATAI3 Right", "DATAI3 Left",
};
static const SOC_ENUM_DOUBLE_DECL(
	tfa98xx_dos_enum, TFA98XX_I2S_SEL_REG,
	TFA98XX_I2S_SEL_REG_DOLS_POS, TFA98XX_I2S_SEL_REG_DORS_POS,
	tfa98xx_dos_text);

static const struct snd_kcontrol_new tfa98xx_controls[] = {
	SOC_SINGLE_EXT("Reg Addr", SND_SOC_NOPM, 0, 0x8F, 0,
		tfa98xx_reg_addr_get, tfa98xx_reg_addr_put),
	SOC_SINGLE_EXT("Reg Value", SND_SOC_NOPM, 0, 0xFFFFFF, 0,
		tfa98xx_reg_value_get, tfa98xx_reg_value_put),
	SOC_SINGLE("Battery Voltage", TFA98XX_BATTERYVOLTAGE,
		TFA98XX_BATTERYVOLTAGE_BATS_POS, TFA98XX_BATTERYVOLTAGE_BATS_MAX, 0),
	SOC_SINGLE("Temperature", TFA98XX_TEMPERATURE,
		TFA98XX_TEMPERATURE_TEMPS_POS, TFA98XX_TEMPERATURE_TEMPS_MAX, 0),
	SOC_VALUE_ENUM("Input Channel Mux", tfa98xx_chs12_enum),
	SOC_ENUM("Gain Channel Mux", tfa98xx_chs3_enum),
	SOC_ENUM_EXT("Amplifier Channel Mux", tfa98xx_chsa_enum,
		snd_soc_get_enum_double, tfa98xx_chsa_put),
	SOC_ENUM("Output Interface Mux", tfa98xx_i2sdoc_enum),
	SOC_ENUM_EXT("Safeguard Threshold", tfa98xx_bsst_enum,
		tfa98xx_bsst_get, tfa98xx_bsst_put),
	SOC_SINGLE("Safeguard Bypass", TFA98XX_BAT_PROT,
		TFA989X_BAT_PROT_BSSBY_POS, TFA989X_BAT_PROT_BSSBY_MAX, 0),
	SOC_SINGLE_TLV("Digital Volume", TFA98XX_AUDIO_CTR,
		TFA98XX_AUDIO_CTR_VOL_POS, TFA98XX_AUDIO_CTR_VOL_MAX,
		1, tfa98xx_vol_tlv),
	SOC_ENUM("Output Voltage", tfa98xx_dcvo_enum),
	SOC_ENUM("Max Coil Current", tfa98xx_dcmcc_enum),
	SOC_SINGLE("Use External Temperature", TFA98XX_SPKR_CALIBRATION,
		TFA98XX_SPKR_CALIBRATION_TROS_POS, TFA98XX_SPKR_CALIBRATION_TROS_MAX, 0),
	SOC_SINGLE("External Temperature", TFA98XX_SPKR_CALIBRATION,
		TFA98XX_SPKR_CALIBRATION_EXTTS_POS, TFA98XX_SPKR_CALIBRATION_EXTTS_MAX, 0),
	SOC_ENUM("Input Interface Mux", tfa98xx_isel_enum),
	SOC_ENUM("Coil Value", tfa98xx_dccv_enum),
	SOC_ENUM("Resistance", tfa98xx_spkr_enum),
	SOC_ENUM("Inductance", tfa98xx_spkl_enum),
	SOC_ENUM("Output Channel Mux", tfa98xx_dos_enum),
	SOC_SINGLE_BOOL_EXT("Recalibrate", 0,
		tfa98xx_recalib_get, tfa98xx_recalib_put),
	TFA98XX_FIRMWARE("Boot Patch", TFA98XX_FW_BOOT),
	TFA98XX_FIRMWARE("ROM Patch", TFA98XX_FW_ROM),
	TFA98XX_FIRMWARE("Speaker File", TFA98XX_FW_SPEAKER),
	TFA98XX_FIRMWARE("Config File", TFA98XX_FW_CONFIG),
	TFA98XX_FIRMWARE("Preset File", TFA98XX_FW_PRESET),
	TFA98XX_FIRMWARE("Equalizer File", TFA98XX_FW_EQUALIZER),
};

static const struct snd_soc_dapm_route tfa98xx_routes[] = {
	{ "Capture", NULL, "Playback" },
};

static const u16 tfa98xx_reg[0x90] = {
	[TFA98XX_I2SREG] =             0x888B,
	[TFA98XX_BAT_PROT] =           0x9392,
	[TFA98XX_AUDIO_CTR] =          0x000F,
	[TFA98XX_DCDCBOOST] =          0x8FFF,
	[TFA98XX_SPKR_CALIBRATION] =   0x3800,
	[TFA98XX_SYS_CTRL] =           0x824D,
	[TFA98XX_I2S_SEL_REG] =        0x3EC3,


	[TFA98XX_INTERRUPT_REG] =      0x0040,


	[TFA98XX_CURRENTSENSE4] =      0xAD93,
};

static int tfa98xx_volatile_register(struct snd_soc_codec *codec, unsigned int reg)
{
	switch (reg) {
	case TFA98XX_I2SREG:
	case TFA98XX_BAT_PROT:
	case TFA98XX_AUDIO_CTR:
	case TFA98XX_DCDCBOOST:
	case TFA98XX_SPKR_CALIBRATION:
	case TFA98XX_SYS_CTRL:
	case TFA98XX_I2S_SEL_REG:
	case TFA98XX_INTERRUPT_REG:
	case TFA98XX_CURRENTSENSE4:
		return 0;
	default:
		return 1;
	}
}

static int tfa98xx_writable_register(struct snd_soc_codec *codec, unsigned int reg)
{
	switch (reg) {
	case TFA98XX_STATUSREG:
	case TFA98XX_BATTERYVOLTAGE:
	case TFA98XX_TEMPERATURE:
	case TFA98XX_REVISIONNUMBER:
	case TFA9890_MTPF:
		return 0;
	default:
		return 1;
	}
}

static const struct snd_soc_codec_driver tfa98xx_drv = {
	.probe = tfa98xx_probe,
	.remove = tfa98xx_remove,
	.controls = tfa98xx_controls,
	.num_controls = ARRAY_SIZE(tfa98xx_controls),
	.dapm_routes = tfa98xx_routes,
	.num_dapm_routes = ARRAY_SIZE(tfa98xx_routes),
	.read = tfa98xx_read,
	.write = tfa98xx_write,
	.volatile_register = tfa98xx_volatile_register,
	.writable_register = tfa98xx_writable_register,
	.reg_cache_size = ARRAY_SIZE(tfa98xx_reg),
	.reg_word_size = sizeof(tfa98xx_reg[0]),
	.reg_cache_default = tfa98xx_reg,
	.idle_bias_off = 1,
};

#define TFA98XX_FORMATS			(SNDRV_PCM_FMTBIT_S16_LE  | \
					 SNDRV_PCM_FMTBIT_S18_3LE | \
					 SNDRV_PCM_FMTBIT_S20_3LE | \
					 SNDRV_PCM_FMTBIT_S24_LE)

#define TFA98XX_RATES			SNDRV_PCM_RATE_8000_48000

static int tfa98xx_set_format(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct tfa98xx_priv *tfa98xx = snd_soc_codec_get_drvdata(codec);

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
	case SND_SOC_DAIFMT_RIGHT_J:
	case SND_SOC_DAIFMT_LEFT_J:
		break;
	default:
		dev_err(codec->dev, "Invalid interface format\n");
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	default:
		dev_err(codec->dev, "Invalid clock inversion\n");
		return -EINVAL;
	}

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		dev_err(codec->dev, "Invalid master/slave setting\n");
		return -EINVAL;
	}

	/* save for later use */
	tfa98xx->fmt = fmt;
	return 0;
}

static int tfa98xx_digital_mute(struct snd_soc_dai *codec_dai, int mute)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct tfa98xx_priv *tfa98xx = snd_soc_codec_get_drvdata(codec);

	if (mute) {
		tfa98xx_stop_monitor(tfa98xx);
		tfa98xx_stop_download(tfa98xx);
		tfa98xx_mute(codec, TFA98XX_MUTE_AMPLIFIER);
		tfa98xx_power(codec, false);
		usleep_range(5000, 5000);
	} else {
		usleep_range(5000, 5000);
		tfa98xx_power(codec, true);
		if (tfa98xx_start_download(tfa98xx, false))
			; /* will turn off the mute after download */
		else
			tfa98xx_mute(codec, TFA98XX_MUTE_OFF);
		tfa98xx_start_monitor(tfa98xx);
	}

	return 0;
}

static int tfa98xx_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tfa98xx_priv *tfa98xx = snd_soc_codec_get_drvdata(codec);
	unsigned int value = 0;

	switch (tfa98xx->fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		value |= TFA98XX_I2SCTRL_PHILIPS;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S16_LE:
			value |= TFA98XX_I2SCTRL_LSB_J_16;
			break;
		case SNDRV_PCM_FORMAT_S18_3LE:
			value |= TFA98XX_I2SCTRL_LSB_J_18;
			break;
		case SNDRV_PCM_FORMAT_S20_3LE:
			value |= TFA98XX_I2SCTRL_LSB_J_20;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			value |= TFA98XX_I2SCTRL_LSB_J_24;
			break;
		default:
			return -EINVAL;
		}
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		value |= TFA98XX_I2SCTRL_MSB_J;
		break;
	default:
		dev_err(codec->dev, "Invalid dai format = %d\n", tfa98xx->fmt);
		return -EINVAL;
	}

	switch (params_rate(params)) {
	case 48000:
		value |= TFA98XX_I2SCTRL_RATE_48000;
		break;
	case 44100:
		value |= TFA98XX_I2SCTRL_RATE_44100;
		break;
	case 32000:
		value |= TFA98XX_I2SCTRL_RATE_32000;
		break;
	case 24000:
		value |= TFA98XX_I2SCTRL_RATE_24000;
		break;
	case 22050:
		value |= TFA98XX_I2SCTRL_RATE_22050;
		break;
	case 16000:
		value |= TFA98XX_I2SCTRL_RATE_16000;
		break;
	case 12000:
		value |= TFA98XX_I2SCTRL_RATE_12000;
		break;
	case 11025:
		value |= TFA98XX_I2SCTRL_RATE_11025;
		break;
	case 8000:
		value |= TFA98XX_I2SCTRL_RATE_08000;
		break;
	default:
		return -EINVAL;
	}

	return snd_soc_update_bits_locked(codec, TFA98XX_I2SREG,
			TFA98XX_I2SREG_I2SSR_MSK | TFA98XX_I2SREG_I2SF_MSK, value);
}

static const struct snd_soc_dai_ops tfa98xx_dai_ops = {
	.set_fmt = tfa98xx_set_format,
	.digital_mute = tfa98xx_digital_mute,
	.hw_params = tfa98xx_hw_params,
};

static struct snd_soc_dai_driver tfa98xx_dai = {
	.name = "tfa98xx-dai",
	.ops = &tfa98xx_dai_ops,
	.capture = {
		.stream_name = "Capture",
		.formats = TFA98XX_FORMATS,
		.rates = TFA98XX_RATES,
		.channels_min = 2,
		.channels_max = 2,
	},
	.playback = {
		.stream_name = "Playback",
		.formats = TFA98XX_FORMATS,
		.rates = TFA98XX_RATES,
		.channels_min = 2,
		.channels_max = 2,
	},
	.symmetric_rates = 1,
};

static int tfa98xx_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	return snd_soc_register_codec(&client->dev,
			&tfa98xx_drv, &tfa98xx_dai, 1);
}

static int tfa98xx_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

static void tfa98xx_i2c_shutdown(struct i2c_client *client)
{
	struct tfa98xx_priv *tfa98xx = i2c_get_clientdata(client);

	if (tfa98xx)
		tfa98xx_power(tfa98xx->codec, false);
}

static const struct i2c_device_id tfa98xx_i2c_id[] = {
	{ "tfa98xx", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tfa98xx_i2c_id);

static struct i2c_driver tfa98xx_i2c_driver = {
	.driver = {
		.name = "tfa98xx",
		.owner = THIS_MODULE,
	},
	.probe = tfa98xx_i2c_probe,
	.remove = tfa98xx_i2c_remove,
	.shutdown = tfa98xx_i2c_shutdown,
	.id_table = tfa98xx_i2c_id,
};
module_i2c_driver(tfa98xx_i2c_driver);

MODULE_AUTHOR("Xiang Xiao <xiaoxiang@xiaomi.com>");
MODULE_DESCRIPTION("ASoC TFA98XX codec driver");
MODULE_LICENSE("GPL");
