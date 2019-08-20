/*
 * aw8896.c   aw8896 codec module
 *
 * Version: v1.0.7
 *
 * Copyright (c) 2017 AWINIC Technology CO., LTD
 *
 * Author: Nick Li <liweilei@awinic.com.cn>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#include <linux/input.h>
#include <sound/tlv.h>
#include "aw8896.h"
#include "aw8896_reg.h"

#define AW8896_I2C_NAME "aw889x_smartpa"
#define AW8896_VERSION "v1.0.7"
#define AW8896_RATES SNDRV_PCM_RATE_8000_48000
#define AW8896_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
				SNDRV_PCM_FMTBIT_S24_LE | \
				SNDRV_PCM_FMTBIT_S32_LE)

#define AW_I2C_RETRIES 5
#define AW_I2C_RETRY_DELAY 5	/* 5ms */
#define AW_READ_CHIPID_RETRIES 5
#define AW_READ_CHIPID_RETRY_DELAY 5
#define AW8896_MAX_DSP_START_TRY_COUNT    10
#define DT_MAX_PROP_SIZE 80

static int aw8896_spk_control;
static int aw8896_rcv_control;

#define AW8896_FW_NAME_MAX     64
#define AW8896_MAX_FIRMWARE_LOAD_CNT 20
static char *aw8896_reg_name = "aw8896_reg.bin";
static char aw8896_fw_name[][AW8896_FW_NAME_MAX] = {
	{"aw8896_fw_d.bin"},
	{"aw8896_fw_e.bin"},
};
static char *aw8896_cfg_name = "aw8896_cfg.bin";

static int aw8896_i2c_write(struct aw8896 *aw8896,
	unsigned char reg_addr, unsigned int reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = regmap_write(aw8896->regmap, reg_addr, reg_data);
		if (ret < 0)
			pr_err("%s: regmap_write cnt=%d error=%d\n", __func__,
					cnt, ret);
		else
			break;
		cnt++;
	}

	return ret;
}

static int aw8896_i2c_read(struct aw8896 *aw8896,
		unsigned char reg_addr, unsigned int *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = regmap_read(aw8896->regmap, reg_addr, reg_data);
		if (ret < 0)
			pr_err("%s: regmap_read cnt=%d error=%d\n", __func__,
					cnt, ret);
		else
			break;
		cnt++;
	}

	return ret;
}

static int aw8896_i2c_writes(struct aw8896 *aw8896,
		unsigned char reg_addr, unsigned char *buf, unsigned int len)
{
	int ret = -1;
	unsigned char *data = NULL;

	data = kmalloc(len + 1, GFP_KERNEL);
	if (data == NULL)
		return  -ENOMEM;

	data[0] = reg_addr;
	memcpy(&data[1], buf, len);

	ret = i2c_master_send(aw8896->i2c, data, len + 1);
	if (ret < 0)
		pr_err("%s: i2c master send error\n", __func__);

	kfree(data);

	return ret;
}

static void aw8896_run_mute(struct aw8896 *aw8896, bool mute)
{
	unsigned int reg_val = 0;

	aw8896_i2c_read(aw8896, AW8896_REG_PWMCTRL, &reg_val);
	if (mute)
		reg_val |= AW8896_BIT_PWMCTRL_HMUTE;
	else
		reg_val &= (~AW8896_BIT_PWMCTRL_HMUTE);

	aw8896_i2c_write(aw8896, AW8896_REG_PWMCTRL, reg_val);
}

static void aw8896_run_pwd(struct aw8896 *aw8896, bool pwd)
{
	unsigned int reg_val = 0;

	aw8896_i2c_read(aw8896, AW8896_REG_SYSCTRL, &reg_val);
	if (pwd)
		reg_val |= AW8896_BIT_SYSCTRL_PWDN;
	else
		reg_val &= (~AW8896_BIT_SYSCTRL_PWDN);

	aw8896_i2c_write(aw8896, AW8896_REG_SYSCTRL, reg_val);
}

static void aw8896_dsp_enable(struct aw8896 *aw8896, bool dsp)
{
	unsigned int reg_val = 0;

	aw8896_i2c_read(aw8896, AW8896_REG_SYSCTRL, &reg_val);
	if (dsp)
		reg_val &= (~AW8896_BIT_SYSCTRL_DSPBY);
	else
		reg_val |= AW8896_BIT_SYSCTRL_DSPBY;

	aw8896_i2c_write(aw8896, AW8896_REG_SYSCTRL, reg_val);
}

static int aw8896_get_iis_status(struct aw8896 *aw8896)
{
	int ret = -1;
	unsigned int reg_val = 0;

	aw8896_i2c_read(aw8896, AW8896_REG_SYSST, &reg_val);
	if (reg_val & AW8896_BIT_SYSST_PLLS)
		ret = 0;

	return ret;
}

static int aw8896_get_dsp_status(struct aw8896 *aw8896)
{
	int ret = -1;
	unsigned int reg_val = 0;

	aw8896_i2c_read(aw8896, AW8896_REG_WDT, &reg_val);
	if (reg_val)
		ret = 0;

	return ret;
}

static void aw8896_spk_rcv_mode(struct aw8896 *aw8896)
{
	unsigned int reg_val = 0;

	pr_debug("%s spk_rcv=%d\n", __func__, aw8896->spk_rcv_mode);

	if (aw8896->spk_rcv_mode == AW8896_SPEAKER_MODE) {
		/* RFB IDAC = 6V */
		aw8896_i2c_read(aw8896, AW8896_REG_AMPDBG1, &reg_val);
		reg_val |= AW8896_BIT_AMPDBG1_OPD;
		reg_val &= (~AW8896_BIT_AMPDBG1_IPWM_20UA);
		reg_val &= (~AW8896_BIT_AMPDBG1_RFB_MASK);
		reg_val |= 0x0002;
		aw8896_i2c_write(aw8896, AW8896_REG_AMPDBG1, reg_val);

		/* Speaker Mode */
		aw8896_i2c_read(aw8896, AW8896_REG_SYSCTRL, &reg_val);
		reg_val &= (~AW8896_BIT_SYSCTRL_RCV_MODE);
		aw8896_i2c_write(aw8896, AW8896_REG_SYSCTRL, reg_val);

	} else if (aw8896->spk_rcv_mode == AW8896_RECEIVER_MODE) {
		/* RFB IDAC = 4V */
		aw8896_i2c_read(aw8896, AW8896_REG_AMPDBG1, &reg_val);
		reg_val |= AW8896_BIT_AMPDBG1_OPD;
		reg_val |= (AW8896_BIT_AMPDBG1_IPWM_20UA);
		reg_val &= (~AW8896_BIT_AMPDBG1_RFB_MASK);
		reg_val |= 0x000B;
		aw8896_i2c_write(aw8896, AW8896_REG_AMPDBG1, reg_val);

		/* Receiver Mode */
		aw8896_i2c_read(aw8896, AW8896_REG_SYSCTRL, &reg_val);
		reg_val |= AW8896_BIT_SYSCTRL_RCV_MODE;
		aw8896_i2c_write(aw8896, AW8896_REG_SYSCTRL, reg_val);
	}
}

static void aw8896_tx_cfg(struct aw8896 *aw8896)
{
	unsigned int reg_val = 0;

	aw8896_i2c_read(aw8896, AW8896_REG_I2STXCFG, &reg_val);
	reg_val |= AW8896_BIT_I2STXCFG_TXEN;
	aw8896_i2c_write(aw8896, AW8896_REG_I2STXCFG, reg_val);

	if (aw8896->dsp_fw_ver == AW8896_DSP_FW_VER_D) {
		aw8896_i2c_read(aw8896, AW8896_REG_DBGCTRL, &reg_val);
		reg_val |= AW8896_BIT_DBGCTRL_LPBK_NEARE;
		aw8896_i2c_write(aw8896, AW8896_REG_DBGCTRL, reg_val);
	}
}

static void aw8896_start(struct aw8896 *aw8896)
{
	aw8896_run_pwd(aw8896, false);
	aw8896_run_mute(aw8896, false);
}

static void aw8896_stop(struct aw8896 *aw8896)
{
	aw8896_run_mute(aw8896, true);
	aw8896_run_pwd(aw8896, true);
}

static void aw8896_dsp_container_update(struct aw8896 *aw8896,
		struct aw8896_container *aw8896_cont, int base)
{
	int i = 0;
	unsigned char tmp_val = 0;
	unsigned int tmp_len = 0;

	aw8896_i2c_write(aw8896, AW8896_REG_DSPMADD, base);
	for (i = 0; i < aw8896_cont->len; i += 2) {
		tmp_val = aw8896_cont->data[i + 0];
		aw8896_cont->data[i + 0] = aw8896_cont->data[i + 1];
		aw8896_cont->data[i + 1] = tmp_val;
	}

	for (i = 0; i < aw8896_cont->len; i += MAX_RAM_WRITE_BYTE_SIZE) {

		if ((aw8896_cont->len - i) < MAX_RAM_WRITE_BYTE_SIZE)
			tmp_len = aw8896_cont->len - i;
		else
			tmp_len = MAX_RAM_WRITE_BYTE_SIZE;

		aw8896_i2c_writes(aw8896, AW8896_REG_DSPMDAT,
			&aw8896_cont->data[i], tmp_len);
	}

	pr_debug("%s exit\n", __func__);
}

static void aw8896_cfg_loaded(const struct firmware *cont, void *context)
{
	struct aw8896 *aw8896 = context;
	struct aw8896_container *aw8896_cfg;
	int ret = -1;

	if (!aw8896) {
		pr_err("%s: aw8896 is null\n", __func__);
		release_firmware(cont);
		return;
	}
	aw8896->dsp_cfg_state = AW8896_DSP_CFG_FAIL;

	if (!cont) {
		pr_err("%s: failed to read %s\n", __func__, aw8896_cfg_name);
		release_firmware(cont);
		return;
	}

	pr_debug("%s: loaded %s - size: %zu\n", __func__, aw8896_cfg_name,
					cont ? cont->size : 0);

	aw8896_cfg = kzalloc(cont->size + sizeof(int), GFP_KERNEL);
	if (!aw8896_cfg) {
		release_firmware(cont);
		pr_err("%s: error allocating memory\n", __func__);
		return;
	}
	aw8896_cfg->len = cont->size;
	memcpy(aw8896_cfg->data, cont->data, cont->size);
	release_firmware(cont);

	mutex_lock(&aw8896->lock);
	aw8896_dsp_container_update(aw8896, aw8896_cfg, AW8896_DSP_CFG_BASE);

	aw8896->dsp_cfg_len = aw8896_cfg->len;

	kfree(aw8896_cfg);
	pr_debug("%s: cfg update complete\n", __func__);

	aw8896_dsp_enable(aw8896, true);

	/* delay 1ms wait for dsp start run */
	udelay(1000);

	ret = aw8896_get_dsp_status(aw8896);
	if (ret) {
		aw8896_dsp_enable(aw8896, false);
		aw8896_run_mute(aw8896, true);
		pr_err("%s: dsp working wdt, dsp fw&cfg update failed\n",
			__func__);
	} else {
		aw8896_tx_cfg(aw8896);
		aw8896_spk_rcv_mode(aw8896);
		aw8896_start(aw8896);
		if (!(aw8896->flags & AW8896_FLAG_SKIP_INTERRUPTS)) {
			aw8896_interrupt_clear(aw8896);
			aw8896_interrupt_setup(aw8896);
		}
		aw8896->init = 1;
		aw8896->dsp_cfg_state = AW8896_DSP_CFG_OK;
		aw8896->dsp_fw_state = AW8896_DSP_FW_OK;
		pr_debug("%s: init ok\n", __func__);
	}
	mutex_unlock(&aw8896->lock);
}

static int aw8896_load_cfg(struct aw8896 *aw8896)
{
	if (aw8896->dsp_cfg_state == AW8896_DSP_CFG_OK) {
		pr_debug("%s: dsp cfg ok\n", __func__);
		return 0;
	}

	aw8896->dsp_cfg_state = AW8896_DSP_CFG_PENDING;

	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				aw8896_cfg_name, aw8896->dev, GFP_KERNEL,
				aw8896, aw8896_cfg_loaded);
}

static void aw8896_fw_loaded(const struct firmware *cont, void *context)
{
	struct aw8896 *aw8896 = context;
	struct aw8896_container *aw8896_fw = NULL;
	int ret = -1;

	if (!aw8896) {
		pr_err("%s: aw8896 is null\n", __func__);
		release_firmware(cont);
		return;
	}
	aw8896->dsp_fw_state = AW8896_DSP_FW_FAIL;

	if (!cont) {
		pr_err("%s: failed to read %s\n", __func__,
			aw8896_fw_name[aw8896->dsp_fw_ver]);
		release_firmware(cont);
		return;
	}

	pr_debug("%s: loaded %s - size: %zu\n", __func__,
		aw8896_fw_name[aw8896->dsp_fw_ver],
		cont ? cont->size : 0);

	aw8896_fw = kzalloc(cont->size + sizeof(int), GFP_KERNEL);
	if (!aw8896_fw) {
		release_firmware(cont);
		pr_err("%s: Error allocating memory\n", __func__);
		return;
	}
	aw8896_fw->len = cont->size;
	memcpy(aw8896_fw->data, cont->data, cont->size);
	release_firmware(cont);

	mutex_lock(&aw8896->lock);
	aw8896_dsp_container_update(aw8896, aw8896_fw, AW8896_DSP_FW_BASE);
	mutex_unlock(&aw8896->lock);

	aw8896->dsp_fw_len = aw8896_fw->len;
	kfree(aw8896_fw);
	pr_debug("%s: fw update complete\n", __func__);

	ret = aw8896_load_cfg(aw8896);
	if (ret)
		pr_err("%s: cfg loading requested failed: %d\n", __func__, ret);
}

static int aw8896_load_fw(struct aw8896 *aw8896)
{
	unsigned int reg_val = 0;
	unsigned int tmp_val = 0;

	if (!aw8896) {
		pr_err("%s: aw8896 is null\n", __func__);
		return -EINVAL;
	}

	if (aw8896->dsp_fw_state == AW8896_DSP_FW_OK) {
		pr_debug("%s: dsp fw ok\n", __func__);
		return 0;
	}

	aw8896->dsp_fw_state = AW8896_DSP_FW_PENDING;

	aw8896_i2c_write(aw8896, AW8896_REG_DSPMADD, AW8896_DSP_FW_VER_BASE);
	aw8896_i2c_read(aw8896, AW8896_REG_DSPMDAT, &reg_val);
	tmp_val |= reg_val;
	aw8896_i2c_write(aw8896, AW8896_REG_DSPMADD,
		AW8896_DSP_FW_VER_BASE + 1);
	aw8896_i2c_read(aw8896, AW8896_REG_DSPMDAT, &reg_val);
	tmp_val |= (reg_val << 16);
	if (tmp_val == 0xdeac97d6) {
		aw8896->dsp_fw_ver = AW8896_DSP_FW_VER_E;
		pr_debug("%s: dsp fw read version: AW8896_DSP_VER_FW_E: 0x%x\n",
				__func__, tmp_val);
	} else if (tmp_val == 0) {
		aw8896->dsp_fw_ver = AW8896_DSP_FW_VER_D;
		pr_debug("%s: dsp fw read version: AW8896_DSP_FW_VER_D: 0x%x\n",
				__func__, tmp_val);
	} else {
		pr_err("%s: dsp fw read version err: 0x%x\n", __func__,
			tmp_val);
		return -EINVAL;
	}

	aw8896_run_mute(aw8896, true);
	aw8896_dsp_enable(aw8896, false);
	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
			aw8896_fw_name[aw8896->dsp_fw_ver],
			aw8896->dev, GFP_KERNEL, aw8896,
			aw8896_fw_loaded);
}

static void aw8896_reg_container_update(struct aw8896 *aw8896,
		struct aw8896_container *aw8896_cont)
{
	int i = 0;
	int reg_addr = 0;
	int reg_val = 0;

	mutex_lock(&aw8896->lock);
	for (i = 0; i < aw8896_cont->len; i += 4) {
		reg_addr = (aw8896_cont->data[i + 1] << 8)
			 + aw8896_cont->data[i + 0];
		reg_val = (aw8896_cont->data[i + 3] << 8)
			 + aw8896_cont->data[i + 2];
		pr_debug("%s: reg=0x%04x, val = 0x%04x\n",
				__func__, reg_addr, reg_val);
		aw8896_i2c_write(aw8896, (unsigned char)reg_addr,
				(unsigned int)reg_val);
	}
	mutex_unlock(&aw8896->lock);

	pr_debug("%s exit\n", __func__);
}

static void aw8896_reg_loaded(const struct firmware *cont, void *context)
{
	struct aw8896 *aw8896 = context;
	struct aw8896_container *aw8896_reg = NULL;
	int ret = -1;

	if (!aw8896) {
		pr_err("%s: aw8896 is null\n", __func__);
		release_firmware(cont);
		return;
	}

	if (!cont) {
		pr_err("%s: failed to read %s\n", __func__, aw8896_reg_name);
		release_firmware(cont);
		return;
	}

	pr_debug("%s: loaded %s - size: %zu\n", __func__, aw8896_reg_name,
			cont ? cont->size : 0);

	aw8896_reg = kzalloc(cont->size+sizeof(int), GFP_KERNEL);
	if (!aw8896_reg) {
		release_firmware(cont);
		pr_err("%s: error allocating memory\n", __func__);
		return;
	}
	aw8896_reg->len = cont->size;
	memcpy(aw8896_reg->data, cont->data, cont->size);
	release_firmware(cont);

	aw8896_reg_container_update(aw8896, aw8896_reg);

	kfree(aw8896_reg);

	ret = aw8896_get_iis_status(aw8896);
	if (ret < 0) {
		pr_err("%s: get no iis signal, ret=%d\n", __func__, ret);
	} else {
		ret = aw8896_load_fw(aw8896);
		if (ret)
			pr_err("%s: cfg loading requested failed: %d\n",
				__func__, ret);
	}
}

static int aw8896_load_reg(struct aw8896 *aw8896)
{
	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
			aw8896_reg_name, aw8896->dev, GFP_KERNEL,
			aw8896, aw8896_reg_loaded);
}

static void aw8896_cold_start(struct aw8896 *aw8896)
{
	int ret = -1;

	ret = aw8896_load_reg(aw8896);
	if (ret)
		pr_err("%s: cfg loading requested failed: %d\n", __func__, ret);
}

static void aw8896_smartpa_cfg(struct aw8896 *aw8896, bool flag)
{
	pr_debug("%s, flag = %d\n", __func__, flag);

	if (flag == true) {
		if (aw8896->init == 0) {
			pr_debug("%s, init = %d\n", __func__, aw8896->init);
			aw8896_cold_start(aw8896);
		} else {
			aw8896_spk_rcv_mode(aw8896);
			aw8896_start(aw8896);
		}
	} else {
		aw8896_stop(aw8896);
	}
}

static const char *const spk_function[] = { "Off", "On" };
static const char *const rcv_function[] = { "Off", "On" };
static const DECLARE_TLV_DB_SCALE(digital_gain, 0, 50, 0);

struct soc_mixer_control aw8896_mixer = {
	.reg    = AW8896_REG_DSPCFG,
	.shift  = AW8896_VOL_REG_SHIFT,
	.max    = AW8896_VOLUME_MAX,
	.min    = AW8896_VOLUME_MIN,
};

static int aw8896_volume_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	struct soc_mixer_control *mc = (struct soc_mixer_control *)
					kcontrol->private_value;

	/* set kcontrol info */
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = mc->max - mc->min;
	return 0;
}

static int aw8896_volume_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct aw8896 *aw8896 = snd_soc_codec_get_drvdata(codec);
	int value;
	struct soc_mixer_control *mc = (struct soc_mixer_control *)
					kcontrol->private_value;

	aw8896_i2c_read(aw8896, AW8896_REG_DSPCFG, &value);
	ucontrol->value.integer.value[0] = (value >> mc->shift)
				 & (~AW8896_BIT_DSPCFG_VOL_MASK);
	return 0;
}

static int aw8896_volume_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc = (struct soc_mixer_control *)
					kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct aw8896 *aw8896 = snd_soc_codec_get_drvdata(codec);
	unsigned int value = 0;
	unsigned int reg_value = 0;

	value = ucontrol->value.integer.value[0];
	if (value > (mc->max-mc->min) || value < 0) {
		pr_err("%s:value over range\n", __func__);
		return -EINVAL;
	}

	aw8896_i2c_read(aw8896, AW8896_REG_SYSST, &reg_value);
	if (!(reg_value & AW8896_BIT_SYSST_PLLS)) {
		pr_err("%s: NO I2S CLK ,can not write reg\n", __func__);
		return 0;
	}
	value = value << mc->shift & AW8896_BIT_DSPCFG_VOL_MASK;
	aw8896_i2c_read(aw8896, AW8896_REG_DSPCFG, &reg_value);
	value = value | (reg_value & 0x00ff);

	aw8896_i2c_read(aw8896, AW8896_REG_SYSCTRL, &reg_value);
	if (reg_value & AW8896_BIT_SYSCTRL_DSPBY) {
		aw8896_i2c_write(aw8896, AW8896_REG_DSPCFG, value);
	} else {
		aw8896_dsp_enable(aw8896, false);
		aw8896_i2c_write(aw8896, AW8896_REG_DSPCFG, value);
		aw8896_dsp_enable(aw8896, true);
	}

	return 0;
}

static struct snd_kcontrol_new aw8896_volume = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name  = "aw8896_rx_volume",
	.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ
		 | SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.tlv.p  = (digital_gain),
	.info = aw8896_volume_info,
	.get =  aw8896_volume_get,
	.put =  aw8896_volume_put,
	.private_value = (unsigned long)&aw8896_mixer,
};

static int aw8896_spk_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: aw8896_spk_control=%d\n", __func__, aw8896_spk_control);
	ucontrol->value.integer.value[0] = aw8896_spk_control;
	return 0;
}

static int aw8896_spk_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct aw8896 *aw8896 = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: ucontrol->value.integer.value[0]=%ld\n ",
			__func__, ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0] == aw8896_spk_control)
		return 1;

	aw8896_spk_control = ucontrol->value.integer.value[0];
	aw8896->spk_rcv_mode = AW8896_SPEAKER_MODE;

	return 0;
}

static int aw8896_rcv_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: aw8896_rcv_control=%d\n", __func__, aw8896_rcv_control);
	ucontrol->value.integer.value[0] = aw8896_rcv_control;
	return 0;
}

static int aw8896_rcv_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct aw8896 *aw8896 = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: ucontrol->value.integer.value[0]=%ld\n ",
			__func__, ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0] == aw8896_rcv_control)
		return 1;

	aw8896_rcv_control = ucontrol->value.integer.value[0];

	aw8896->spk_rcv_mode = AW8896_RECEIVER_MODE;

	return 0;
}

static const struct soc_enum aw8896_snd_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(spk_function), spk_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(rcv_function), rcv_function),
};

static struct snd_kcontrol_new aw8896_controls[] = {
	SOC_ENUM_EXT("aw8896_speaker_switch", aw8896_snd_enum[0],
			aw8896_spk_get, aw8896_spk_set),
	SOC_ENUM_EXT("aw8896_receiver_switch", aw8896_snd_enum[1],
			aw8896_rcv_get, aw8896_rcv_set),
};

static void aw8896_add_codec_controls(struct aw8896 *aw8896)
{
	snd_soc_add_codec_controls(aw8896->codec, aw8896_controls,
				ARRAY_SIZE(aw8896_controls));

	snd_soc_add_codec_controls(aw8896->codec, &aw8896_volume, 1);
}

/*
 * Digital Audio Interface
 */
static int aw8896_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct aw8896 *aw8896 = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: enter\n", __func__);
	mutex_lock(&aw8896->lock);
	aw8896_run_pwd(aw8896, false);
	mutex_unlock(&aw8896->lock);

	return 0;
}

static int aw8896_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;

	pr_debug("%s: fmt=0x%x\n", __func__, fmt);

	/* Supported mode: regular I2S, slave, or PDM */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		if ((fmt & SND_SOC_DAIFMT_MASTER_MASK)
			 != SND_SOC_DAIFMT_CBS_CFS) {
			dev_err(codec->dev, "%s: invalid codec master mode\n",
				__func__);
			return -EINVAL;
		}
		break;
	default:
		dev_err(codec->dev, "%s: unsupported DAI format %d\n", __func__,
			fmt & SND_SOC_DAIFMT_FORMAT_MASK);
		return -EINVAL;
	}
	return 0;
}

static int aw8896_set_dai_sysclk(struct snd_soc_dai *codec_dai,
	int clk_id, unsigned int freq, int dir)
{
	struct aw8896 *aw8896 = snd_soc_codec_get_drvdata(codec_dai->codec);

	pr_debug("%s: freq=%d\n", __func__, freq);

	aw8896->sysclk = freq;
	return 0;
}

static int aw8896_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct aw8896 *aw8896 = snd_soc_codec_get_drvdata(codec);
	int reg_value = 0;
	int tmp_value = 0;
	int width = 0;

	mutex_lock(&aw8896->lock);
	/* get rate param */
	aw8896->rate = params_rate(params);
	pr_debug("%s: requested rate: %d, sample size: %d\n", __func__,
			aw8896->rate,
			snd_pcm_format_width(params_format(params)));

	switch (aw8896->rate) {
	case 8000:
		reg_value = AW8896_BIT_I2SCTRL_SR_8K;
		break;
	case 16000:
		reg_value = AW8896_BIT_I2SCTRL_SR_16K;
		break;
	case 32000:
		reg_value = AW8896_BIT_I2SCTRL_SR_32K;
		break;
	case 44100:
		reg_value = AW8896_BIT_I2SCTRL_SR_44K;
		break;
	case 48000:
		reg_value = AW8896_BIT_I2SCTRL_SR_48K;
		break;
	default:
		reg_value = AW8896_BIT_I2SCTRL_SR_48K;
		pr_err("%s: rate can not support, set to default\n", __func__);
		break;
	}
	/* set chip rate */
	if (reg_value != -1) {
		aw8896_i2c_read(aw8896, AW8896_REG_I2SCTRL, &tmp_value);
		reg_value |= (tmp_value & (~AW8896_BIT_I2SCTRL_SR_MASK));
		aw8896_i2c_write(aw8896, AW8896_REG_I2SCTRL, reg_value);
	}

	/* get bit width */
	width = params_width(params);
	pr_debug("%s: width = %d\n", __func__, width);
	switch (width) {
	case 16:
		reg_value = AW8896_BIT_I2SCTRL_FMS_16BIT;
		break;
	case 20:
		reg_value = AW8896_BIT_I2SCTRL_FMS_20BIT;
		break;
	case 24:
		reg_value = AW8896_BIT_I2SCTRL_FMS_24BIT;
		break;
	case 32:
		reg_value = AW8896_BIT_I2SCTRL_FMS_32BIT;
		break;
	default:
		reg_value = AW8896_BIT_I2SCTRL_FMS_16BIT;
		pr_err("%s: width can not support\n", __func__);
		break;
	}
	/* set width */
	if (reg_value != -1) {
		aw8896_i2c_read(aw8896, AW8896_REG_I2SCTRL, &tmp_value);
		reg_value |= (tmp_value & (~AW8896_BIT_I2SCTRL_FMS_MASK));
		aw8896_i2c_write(aw8896, AW8896_REG_I2SCTRL, reg_value);
	}
	mutex_unlock(&aw8896->lock);

	return 0;
}

static int aw8896_mute(struct snd_soc_dai *dai, int mute, int stream)
{
	struct snd_soc_codec *codec = dai->codec;
	struct aw8896 *aw8896 = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: mute state=%d\n", __func__, mute);

	if (!(aw8896->flags & AW8896_FLAG_DSP_START_ON_MUTE))
		return 0;

	if (mute) {
		/* stop DSP only when both playback and capture streams
		 * are deactivated
		 */
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			aw8896->pstream = 0;
		else
			aw8896->cstream = 0;
		if (aw8896->pstream != 0 || aw8896->cstream != 0)
			return 0;

		/* Stop DSP */
		mutex_lock(&aw8896->lock);
		aw8896_smartpa_cfg(aw8896, false);
		mutex_unlock(&aw8896->lock);
	} else {
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			aw8896->pstream = 1;
		else
			aw8896->cstream = 1;

		/* Start DSP */
		mutex_lock(&aw8896->lock);
		aw8896_smartpa_cfg(aw8896, true);
		mutex_unlock(&aw8896->lock);
	}

	return 0;
}

static void aw8896_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct aw8896 *aw8896 = snd_soc_codec_get_drvdata(codec);

	aw8896->rate = 0;
	aw8896_run_pwd(aw8896, true);
}

static const struct snd_soc_dai_ops aw8896_dai_ops = {
	.startup = aw8896_startup,
	.set_fmt = aw8896_set_fmt,
	.set_sysclk = aw8896_set_dai_sysclk,
	.hw_params = aw8896_hw_params,
	.mute_stream = aw8896_mute,
	.shutdown = aw8896_shutdown,
};

static struct snd_soc_dai_driver aw8896_dai[] = {
	{
		.name = "aw8896-aif",
		.id = 1,
		.playback = {
			.stream_name = "Speaker_Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = AW8896_RATES,
			.formats = AW8896_FORMATS,
		},
		.capture = {
			 .stream_name = "Speaker_Capture",
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = AW8896_RATES,
			 .formats = AW8896_FORMATS,
		 },
		.ops = &aw8896_dai_ops,
		.symmetric_rates = 1,
		.symmetric_channels = 1,
		.symmetric_samplebits = 1,
	},
};

static int aw8896_probe(struct snd_soc_codec *codec)
{
	struct aw8896 *aw8896 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	aw8896->codec = codec;
	aw8896_add_codec_controls(aw8896);

	return ret;
}

static int aw8896_remove(struct snd_soc_codec *codec)
{
	return 0;
}

struct regmap *aw8896_get_regmap(struct device *dev)
{
	struct aw8896 *aw8896 = dev_get_drvdata(dev);

	return aw8896->regmap;
}

static unsigned int aw8896_codec_read(struct snd_soc_codec *codec,
		unsigned int reg)
{
	struct aw8896 *aw8896 = snd_soc_codec_get_drvdata(codec);
	unsigned int value = 0;
	int ret = 0;

	pr_debug("%s:enter\n", __func__);

	if (aw8896_reg_access[reg] & REG_RD_ACCESS) {
		ret = aw8896_i2c_read(aw8896, reg, &value);
		if (ret < 0) {
			pr_debug("%s: read register failed\n", __func__);
			return ret;
		}
	} else {
		pr_debug("%s:Register 0x%x NO read access\n", __func__, reg);
		return -EINVAL;
	}
	return value;
}

static int aw8896_codec_write(struct snd_soc_codec *codec, unsigned int reg,
		unsigned int value)
{
	int ret = 0;
	struct aw8896 *aw8896 = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s:enter ,reg is 0x%x value is 0x%x\n", __func__, reg, value);

	if (aw8896_reg_access[reg] & REG_WR_ACCESS) {
		ret = aw8896_i2c_write(aw8896, reg, value);
		return ret;
	}
	pr_debug("%s: Register 0x%x NO write access\n", __func__, reg);

	return -EINVAL;
}

#ifdef CONFIG_PM
static int aw8896_suspend(struct snd_soc_codec *codec)
{
	int ret = 0;
	struct aw8896 *aw8896 = snd_soc_codec_get_drvdata(codec);

	/* clear FW/DSP status flags so the DSP can get into
	 * correct status after resume
	 */
	aw8896->init = 0;
	aw8896->dsp_fw_state = AW8896_DSP_FW_FAIL;
	aw8896->dsp_cfg_state = AW8896_DSP_CFG_FAIL;

	ret = regulator_disable(aw8896->supply.regulator);
	if (ret < 0)
		dev_err(codec->dev, "%s: fail to disable regulator, err:%d\n",
			__func__, ret);

	return 0;
}

static int aw8896_resume(struct snd_soc_codec *codec)
{
	int ret = 0;
	struct aw8896 *aw8896 = snd_soc_codec_get_drvdata(codec);

	ret = regulator_enable(aw8896->supply.regulator);
	if (ret < 0)
		dev_err(codec->dev, "%s: fail to enable regulator, err:%d\n",
			__func__, ret);

	return 0;
}
#else
#define aw8896_suspend NULL
#define aw8896_resume NULL
#endif

static struct snd_soc_codec_driver soc_codec_dev_aw8896 = {
	.probe = aw8896_probe,
	.remove = aw8896_remove,
	.suspend = aw8896_suspend,
	.resume = aw8896_resume,
	.read = aw8896_codec_read,
	.write = aw8896_codec_write,
	.reg_cache_size = AW8896_REG_MAX,
	.reg_word_size = 2,
};

bool aw8896_writeable_register(struct device *dev, unsigned int reg)
{
	/* enable write access for all registers */
	return 1;
}

bool aw8896_readable_register(struct device *dev, unsigned int reg)
{
	/* enable read access for all registers */
	return 1;
}

bool aw8896_volatile_register(struct device *dev, unsigned int reg)
{
	return 1;
}

static const struct regmap_config aw8896_regmap = {
	.reg_bits = 8,
	.val_bits = 16,

	.max_register = AW8896_MAX_REGISTER,
	.writeable_reg = aw8896_writeable_register,
	.readable_reg = aw8896_readable_register,
	.volatile_reg = aw8896_volatile_register,
	.cache_type = REGCACHE_RBTREE,
};

static void aw8896_interrupt_setup(struct aw8896 *aw8896)
{
	unsigned int reg_val = 0;

	aw8896_i2c_read(aw8896, AW8896_REG_SYSINTM, &reg_val);

	/* i2s status interrupt unmask */
	reg_val &= (~AW8896_BIT_SYSINTM_PLLM);
	reg_val &= (~AW8896_BIT_SYSINTM_CLKM);
	reg_val &= (~AW8896_BIT_SYSINTM_NOCLKM);

	/*  uvlo interrupt unmask */
	reg_val &= (~AW8896_BIT_SYSINTM_UVLOM);

	/* over temperature interrupt unmask */
	reg_val &= (~AW8896_BIT_SYSINTM_OTHM);

	/* dsp watchdog status interrupt unmask */
	reg_val &= (~AW8896_BIT_SYSINTM_WDM);

	aw8896_i2c_write(aw8896, AW8896_REG_SYSINTM, reg_val);
}

static void aw8896_interrupt_clear(struct aw8896 *aw8896)
{
	unsigned int reg_val;

	aw8896_i2c_read(aw8896, AW8896_REG_SYSST, &reg_val);
	pr_debug("%s: reg SYSST=0x%x\n", __func__, reg_val);

	aw8896_i2c_read(aw8896, AW8896_REG_SYSINT, &reg_val);
	pr_debug("%s: reg SYSINT=0x%x\n", __func__, reg_val);

	aw8896_i2c_read(aw8896, AW8896_REG_SYSINTM, &reg_val);
	pr_debug("%s: reg SYSINTM=0x%x\n", __func__, reg_val);
}

static irqreturn_t aw8896_irq(int irq, void *data)
{
	struct aw8896 *aw8896 = data;

	aw8896_interrupt_clear(aw8896);

	pr_debug("%s exit\n", __func__);

	return IRQ_HANDLED;
}

static int aw8896_parse_dt(struct device *dev, struct aw8896 *aw8896,
		struct device_node *np) {
	int prop_val = 0;
	int ret = 0;
	int len = 0;
	const __be32 *prop = NULL;
	struct device_node *regnode = NULL;
	char *dvdd_supply = "dvdd";
	char prop_name[DT_MAX_PROP_SIZE] = {0};

	aw8896->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (aw8896->reset_gpio < 0) {
		dev_err(dev,
			"%s: no reset gpio provided, will not HW reset device\n",
			__func__);
		return -EINVAL;
	}

	dev_dbg(dev, "%s: reset gpio provided ok\n", __func__);

	aw8896->irq_gpio =  of_get_named_gpio(np, "irq-gpio", 0);
	if (aw8896->irq_gpio < 0)
		dev_info(dev, "%s: no irq gpio provided.\n", __func__);

	snprintf(prop_name, DT_MAX_PROP_SIZE, "%s-supply", dvdd_supply);
	regnode = of_parse_phandle(np, prop_name, 0);
	if (!regnode) {
		dev_err(dev, "%s: no %s provided\n", __func__, prop_name);
		goto err_get_regulator;
	}

	aw8896->supply.regulator = devm_regulator_get(dev, dvdd_supply);
	if (IS_ERR(aw8896->supply.regulator)) {
		dev_err(dev, "%s: failed to get supply for %s\n", __func__,
			dvdd_supply);
		goto err_get_regulator;
	}

	snprintf(prop_name, DT_MAX_PROP_SIZE, "%s-voltage", dvdd_supply);
	prop = of_get_property(np, prop_name, &len);
	if (!prop || (len != (2 * sizeof(__be32)))) {
		dev_err(dev, "%s: no %s provided or format invalid\n",
			__func__, prop_name);
		goto err_get_voltage;
	}

	aw8896->supply.min_uv = be32_to_cpup(&prop[0]);
	aw8896->supply.max_uv = be32_to_cpup(&prop[1]);

	snprintf(prop_name, DT_MAX_PROP_SIZE, "%s-current", dvdd_supply);
	ret = of_property_read_u32(np, prop_name, &prop_val);
	if (ret) {
		dev_err(dev, "%s: no %s provided\n", __func__, prop_name);
		goto err_get_current;
	}
	aw8896->supply.ua = prop_val;

	return 0;

err_get_current:
err_get_voltage:
	devm_regulator_put(aw8896->supply.regulator);
	aw8896->supply.regulator = NULL;
err_get_regulator:
	return -EINVAL;
}

int aw8896_hw_reset(struct aw8896 *aw8896)
{
	if (aw8896 && gpio_is_valid(aw8896->reset_gpio)) {
		gpio_set_value_cansleep(aw8896->reset_gpio, 1);
		udelay(1000);   /* delay 1ms for aw8896 hardware reset */
		gpio_set_value_cansleep(aw8896->reset_gpio, 0);
		udelay(1000);	/* delay 1ms for aw8896 hardware reset */
	} else {
		dev_err(aw8896->dev, "%s:  failed\n", __func__);
		return -EINVAL;
	}
	return 0;
}

int aw8896_read_chipid(struct aw8896 *aw8896)
{
	int ret = -1;
	unsigned int cnt = 0;
	unsigned int reg = 0;

	while (cnt < AW_READ_CHIPID_RETRIES) {
		ret = aw8896_i2c_read(aw8896, AW8896_REG_ID, &reg);
		if (ret < 0) {
			dev_err(aw8896->dev,
				"%s: failed to read register AW8896_REG_ID: %d\n",
				__func__, ret);
			return -EIO;
		}
		switch (reg) {
		case 0x0310:
			pr_debug("%s aw8896 detected\n", __func__);
			aw8896->flags |= AW8896_FLAG_SKIP_INTERRUPTS;
			aw8896->flags |= AW8896_FLAG_DSP_START_ON_MUTE;
			aw8896->flags |= AW8896_FLAG_DSP_START_ON;
			aw8896->chipid = AW8990_ID;
			pr_debug("%s aw8896->flags=0x%x\n", __func__,
				aw8896->flags);
			return 0;
		default:
			pr_err("%s unsupported device revision (0x%x)\n",
				__func__, reg);
			break;
		}
		cnt++;

		msleep(AW_READ_CHIPID_RETRY_DELAY);
	}

	return -EINVAL;
}

static ssize_t aw8896_reg_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct aw8896 *aw8896 = dev_get_drvdata(dev);
	unsigned int databuf[2] = {0};

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2)
		aw8896_i2c_write(aw8896, databuf[0], databuf[1]);

	return count;
}

static ssize_t aw8896_reg_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct aw8896 *aw8896 = dev_get_drvdata(dev);
	ssize_t len = 0;
	unsigned char i = 0;
	unsigned int reg_val = 0;

	for (i = 0; i < AW8896_REG_MAX; i++) {
		if (!(aw8896_reg_access[i] & REG_RD_ACCESS))
			continue;
		aw8896_i2c_read(aw8896, i, &reg_val);
		len += snprintf(buf + len, PAGE_SIZE - len,
			"reg:0x%02x=0x%04x\n", i, reg_val);
	}
	return len;
}

static ssize_t aw8896_dsp_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct aw8896 *aw8896 = dev_get_drvdata(dev);

	unsigned int databuf[16] = {0};
	unsigned int reg_val = 0;

	if (kstrtouint(buf, 10, &databuf[0]) == 0) {
		if (databuf[0] == 1) {
			aw8896_i2c_read(aw8896, AW8896_REG_SYSST, &reg_val);
			if (reg_val & AW8896_BIT_SYSST_PLLS) {
				aw8896->init = 0;
				aw8896->dsp_fw_state = AW8896_DSP_FW_FAIL;
				aw8896->dsp_cfg_state = AW8896_DSP_CFG_FAIL;
				aw8896_smartpa_cfg(aw8896, false);
				aw8896_smartpa_cfg(aw8896, true);
			} else {
				count += snprintf((char *)(buf + count),
					PAGE_SIZE - count,
					"aw8896 plls=%d, no iis signal\n",
					reg_val & AW8896_BIT_SYSST_PLLS);
			}
		} else {
			aw8896_dsp_enable(aw8896, false);
		}
	}

	return count;
}

static ssize_t aw8896_dsp_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct aw8896 *aw8896 = dev_get_drvdata(dev);
	ssize_t len = 0;
	unsigned int i = 0;
	unsigned int addr = 0;
	unsigned int reg_val = 0;

	aw8896_i2c_read(aw8896, AW8896_REG_SYSCTRL, &reg_val);

	if (reg_val & AW8896_BIT_SYSCTRL_DSPBY) {
		len += snprintf(buf + len, PAGE_SIZE - len,
			"aw8896 dsp bypass\n");
	} else {
		aw8896_i2c_read(aw8896, AW8896_REG_SYSST, &reg_val);
		if (reg_val & AW8896_BIT_SYSST_PLLS) {
			len += snprintf(buf + len, PAGE_SIZE - len,
				"aw8896 dsp working\n");

			aw8896_dsp_enable(aw8896, false);

			len += snprintf(buf + len, PAGE_SIZE - len,
				"aw8896 dsp firmware:\n");
			addr = 0;
			for (i = 0; i < aw8896->dsp_fw_len; i += 2) {
				aw8896_i2c_write(aw8896, AW8896_REG_DSPMADD,
					AW8896_DSP_FW_BASE + addr);
				aw8896_i2c_read(aw8896, AW8896_REG_DSPMDAT,
					&reg_val);

				len += snprintf(buf + len, PAGE_SIZE - len,
					"0x%02x,0x%02x,", reg_val & 0x00FF,
					(reg_val >> 8));

				if ((i / 2 + 1) % 4 == 0)
					len += snprintf(buf + len,
						PAGE_SIZE - len, "\n");
				addr++;
			}
			len += snprintf(buf + len, PAGE_SIZE - len, "\n");
			len += snprintf(buf + len, PAGE_SIZE - len,
				"aw8896 dsp config:\n");
			addr = 0;
			for (i = 0; i < aw8896->dsp_cfg_len; i += 2) {
				aw8896_i2c_write(aw8896, AW8896_REG_DSPMADD,
					AW8896_DSP_CFG_BASE + addr);
				aw8896_i2c_read(aw8896, AW8896_REG_DSPMDAT,
					&reg_val);

				len += snprintf(buf + len, PAGE_SIZE - len,
					"0x%02x,0x%02x,", reg_val & 0x00FF,
					(reg_val >> 8));

				if ((i / 2 + 1) % 4 == 0)
					len += snprintf(buf + len,
						PAGE_SIZE - len, "\n");
				addr++;
			}
			len += snprintf(buf + len, PAGE_SIZE - len, "\n");

			aw8896_dsp_enable(aw8896, true);
		} else {
			len += snprintf((char *)(buf+len), PAGE_SIZE - len,
				"aw8896 plls=%d, no iis signal\n",
				reg_val & AW8896_BIT_SYSST_PLLS);
		}
	}

	return len;
}

static ssize_t aw8896_spk_rcv_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct aw8896 *aw8896 = dev_get_drvdata(dev);
	unsigned int databuf[2] = {0};

	if (kstrtouint(buf, 10, &databuf[0]) == 0)
		aw8896->spk_rcv_mode = databuf[0];

	return count;
}

static ssize_t aw8896_spk_rcv_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct aw8896 *aw8896 = dev_get_drvdata(dev);
	ssize_t len = 0;

	if (aw8896->spk_rcv_mode == AW8896_SPEAKER_MODE)
		len += snprintf(buf+len, PAGE_SIZE-len,
			"aw8896 spk_rcv: %d, speaker mode\n",
			aw8896->spk_rcv_mode);
	else if (aw8896->spk_rcv_mode == AW8896_RECEIVER_MODE)
		len += snprintf(buf+len, PAGE_SIZE-len,
			"aw8896 spk_rcv: %d, receiver mode\n",
			aw8896->spk_rcv_mode);
	else
		len += snprintf(buf+len, PAGE_SIZE-len,
			"aw8896 spk_rcv: %d, unknown mode\n",
			aw8896->spk_rcv_mode);

	return len;
}

static DEVICE_ATTR(reg, 0644, aw8896_reg_show, aw8896_reg_store);
static DEVICE_ATTR(dsp, 0644, aw8896_dsp_show, aw8896_dsp_store);
static DEVICE_ATTR(spk_rcv, 0644, aw8896_spk_rcv_show, aw8896_spk_rcv_store);

static struct attribute *aw8896_attributes[] = {
	&dev_attr_reg.attr,
	&dev_attr_dsp.attr,
	&dev_attr_spk_rcv.attr,
	NULL
};

static struct attribute_group aw8896_attribute_group = {
	.attrs = aw8896_attributes
};

static int aw8896_i2c_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	struct snd_soc_dai_driver *dai;
	struct aw8896 *aw8896;
	struct device_node *np = i2c->dev.of_node;
	int irq_flags = 0;
	int ret = -1;

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "check_functionality failed\n");
		return -EIO;
	}

	aw8896 = devm_kzalloc(&i2c->dev, sizeof(struct aw8896), GFP_KERNEL);
	if (aw8896 == NULL)
		return -ENOMEM;

	aw8896->dev = &i2c->dev;
	aw8896->i2c = i2c;

	aw8896->regmap = devm_regmap_init_i2c(i2c, &aw8896_regmap);
	if (IS_ERR(aw8896->regmap)) {
		ret = PTR_ERR(aw8896->regmap);
		dev_err(&i2c->dev, "%s: failed to allocate register map: %d\n",
			__func__, ret);
		goto err_regmap;
	}

	i2c_set_clientdata(i2c, aw8896);
	mutex_init(&aw8896->lock);

	if (np) {
		ret = aw8896_parse_dt(&i2c->dev, aw8896, np);
		if (ret) {
			dev_err(&i2c->dev,
				"%s: failed to parse device tree node\n",
				__func__);
			goto err_parse_dt;
		}
	} else {
		aw8896->reset_gpio = -1;
		aw8896->irq_gpio = -1;
	}

	if (gpio_is_valid(aw8896->reset_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, aw8896->reset_gpio,
			GPIOF_OUT_INIT_LOW, "aw8896_rst");
		if (ret) {
			dev_err(&i2c->dev, "%s: rst request failed\n",
					__func__);
			goto err_reset_gpio_request;
		}
	}

	if (gpio_is_valid(aw8896->irq_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, aw8896->irq_gpio,
			GPIOF_DIR_IN, "aw8896_int");
		if (ret) {
			dev_err(&i2c->dev, "%s: int request failed\n",
				__func__);
			goto err_irq_gpio_request;
		}
	}

	ret = regulator_set_voltage(aw8896->supply.regulator,
				    aw8896->supply.max_uv,
				    aw8896->supply.min_uv);
	if (ret) {
		dev_err(&i2c->dev, "%s: set voltage %d ~ %d failed\n",
				__func__,
				aw8896->supply.min_uv,
				aw8896->supply.max_uv);
		goto err_supply_set;
	}

	ret = regulator_set_load(aw8896->supply.regulator, aw8896->supply.ua);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: set current %d failed\n", __func__,
				aw8896->supply.ua);
		goto err_supply_set;
	}

	ret = regulator_enable(aw8896->supply.regulator);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: regulator enable failed\n", __func__);
		goto err_supply_set;
	}

	ret = aw8896_hw_reset(aw8896);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: aw8896_hw_reset failed\n", __func__);
		goto err_hw_rst;
	}

	ret = aw8896_read_chipid(aw8896);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: aw8896_read_chipid failed ret=%d\n",
				   __func__, ret);
		goto err_id;
	}

	/* register codec */
	dai = devm_kzalloc(&i2c->dev, sizeof(aw8896_dai), GFP_KERNEL);
	if (!dai)
		goto err_dai_kzalloc;

	memcpy(dai, aw8896_dai, sizeof(aw8896_dai));
	pr_debug("%s dai->name(%s)\n", __func__, dai->name);

	ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_aw8896,
				dai, ARRAY_SIZE(aw8896_dai));
	if (ret < 0) {
		dev_err(&i2c->dev, "%s failed to register aw8896: %d\n",
				   __func__, ret);
		goto err_register_codec;
	}

	/* aw8896 irq */
	if (gpio_is_valid(aw8896->irq_gpio) &&
		!(aw8896->flags & AW8896_FLAG_SKIP_INTERRUPTS)) {

		irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
		ret = devm_request_threaded_irq(&i2c->dev,
				gpio_to_irq(aw8896->irq_gpio),
				NULL, aw8896_irq, irq_flags,
				"aw8896", aw8896);
		if (ret != 0) {
			dev_err(&i2c->dev, "failed to request IRQ %d: %d\n",
					gpio_to_irq(aw8896->irq_gpio), ret);
			goto err_irq;
		}
	} else {
		dev_info(&i2c->dev, "%s skipping IRQ registration\n", __func__);
		/* disable feature support if gpio was invalid */
		aw8896->flags |= AW8896_FLAG_SKIP_INTERRUPTS;
	}

	dev_set_drvdata(&i2c->dev, aw8896);
	ret = sysfs_create_group(&i2c->dev.kobj, &aw8896_attribute_group);
	if (ret < 0) {
		dev_info(&i2c->dev, "%s error creating sysfs attr files\n",
			__func__);
		goto err_sysfs;
	}

	pr_debug("%s probe completed successfully!\n", __func__);

	return 0;

err_sysfs:
	devm_free_irq(&i2c->dev, gpio_to_irq(aw8896->irq_gpio), aw8896);
err_irq:
	snd_soc_unregister_codec(&i2c->dev);
err_register_codec:
	devm_kfree(&i2c->dev, dai);
	dai = NULL;
err_dai_kzalloc:
err_id:
	if (gpio_is_valid(aw8896->irq_gpio))
		devm_gpio_free(&i2c->dev, aw8896->irq_gpio);
err_hw_rst:
	if (aw8896->supply.regulator)
		regulator_disable(aw8896->supply.regulator);
err_supply_set:
	if (aw8896->supply.regulator)
		devm_regulator_put(aw8896->supply.regulator);
err_irq_gpio_request:
	if (gpio_is_valid(aw8896->reset_gpio))
		devm_gpio_free(&i2c->dev, aw8896->reset_gpio);
err_reset_gpio_request:
err_parse_dt:
err_regmap:
	devm_kfree(&i2c->dev, aw8896);
	aw8896 = NULL;
	return ret;
}

static int aw8896_i2c_remove(struct i2c_client *i2c)
{
	struct aw8896 *aw8896 = i2c_get_clientdata(i2c);

	if (gpio_to_irq(aw8896->irq_gpio))
		devm_free_irq(&i2c->dev, gpio_to_irq(aw8896->irq_gpio), aw8896);

	snd_soc_unregister_codec(&i2c->dev);

	if (gpio_is_valid(aw8896->irq_gpio))
		devm_gpio_free(&i2c->dev, aw8896->irq_gpio);
	if (gpio_is_valid(aw8896->reset_gpio))
		devm_gpio_free(&i2c->dev, aw8896->reset_gpio);

	if (aw8896->supply.regulator) {
		regulator_disable(aw8896->supply.regulator);
		devm_regulator_put(aw8896->supply.regulator);
	}

	devm_kfree(&i2c->dev, aw8896);
	aw8896 = NULL;

	return 0;
}

static const struct i2c_device_id aw8896_i2c_id[] = {
	{ AW8896_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, aw8896_i2c_id);

static const struct of_device_id aw8896_dt_match[] = {
	{ .compatible = "awinic,aw8896_smartpa" },
	{ },
};

static struct i2c_driver aw8896_i2c_driver = {
	.driver = {
		.name = AW8896_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(aw8896_dt_match),
	},
	.probe = aw8896_i2c_probe,
	.remove = aw8896_i2c_remove,
	.id_table = aw8896_i2c_id,
};

static int __init aw8896_i2c_init(void)
{
	int ret = 0;

	pr_info("%s: aw8896 driver version %s\n", __func__, AW8896_VERSION);

	ret = i2c_add_driver(&aw8896_i2c_driver);
	if (ret) {
		pr_err("%s: fail to add aw8896 device into i2c\n", __func__);
		return ret;
	}

	return 0;
}
module_init(aw8896_i2c_init);

static void __exit aw8896_i2c_exit(void)
{
	i2c_del_driver(&aw8896_i2c_driver);
}
module_exit(aw8896_i2c_exit);

MODULE_DESCRIPTION("ASoC AW8896 Smart PA Driver");
MODULE_LICENSE("GPL v2");
