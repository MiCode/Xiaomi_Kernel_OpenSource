/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/mfd/msm-adie-codec.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/wakelock.h>
#include <linux/pmic8058-othc.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/moduleparam.h>
#include <linux/pm_qos.h>

#include <asm/uaccess.h>
#include <mach/qdsp6v2/audio_dev_ctl.h>
#include <mach/qdsp6v2/audio_acdb.h>
#include <mach/vreg.h>
#include <mach/pmic.h>
#include <mach/debug_mm.h>
#include <mach/cpuidle.h>

#include <sound/q6afe.h>
#include <sound/apr_audio.h>
#include "snddev_icodec.h"

#define SNDDEV_ICODEC_PCM_SZ 32 /* 16 bit / sample stereo mode */
#define SNDDEV_ICODEC_MUL_FACTOR 3 /* Multi by 8 Shift by 3  */
#define SNDDEV_ICODEC_CLK_RATE(freq) \
	(((freq) * (SNDDEV_ICODEC_PCM_SZ)) << (SNDDEV_ICODEC_MUL_FACTOR))
#define SNDDEV_LOW_POWER_MODE 0
#define SNDDEV_HIGH_POWER_MODE 1
/* Voltage required for S4 in microVolts, 2.2V or 2200000microvolts */
#define SNDDEV_VREG_8058_S4_VOLTAGE (2200000)
/* Load Current required for S4 in microAmps,
   36mA - 56mA */
#define SNDDEV_VREG_LOW_POWER_LOAD (36000)
#define SNDDEV_VREG_HIGH_POWER_LOAD (56000)

bool msm_codec_i2s_slave_mode;

/* Context for each internal codec sound device */
struct snddev_icodec_state {
	struct snddev_icodec_data *data;
	struct adie_codec_path *adie_path;
	u32 sample_rate;
	u32 enabled;
};

/* Global state for the driver */
struct snddev_icodec_drv_state {
	struct mutex rx_lock;
	struct mutex lb_lock;
	struct mutex tx_lock;
	u32 rx_active; /* ensure one rx device at a time */
	u32 tx_active; /* ensure one tx device at a time */
	struct clk *rx_osrclk;
	struct clk *rx_bitclk;
	struct clk *tx_osrclk;
	struct clk *tx_bitclk;

	struct pm_qos_request rx_pm_qos_req;
	struct pm_qos_request tx_pm_qos_req;

	/* handle to pmic8058 regulator smps4 */
	struct regulator *snddev_vreg;
};

static struct snddev_icodec_drv_state snddev_icodec_drv;

struct regulator *vreg_init(void)
{
	int rc;
	struct regulator *vreg_ptr;

	vreg_ptr = regulator_get(NULL, "8058_s4");
	if (IS_ERR(vreg_ptr)) {
		pr_err("%s: regulator_get 8058_s4 failed\n", __func__);
		return NULL;
	}

	rc = regulator_set_voltage(vreg_ptr, SNDDEV_VREG_8058_S4_VOLTAGE,
				SNDDEV_VREG_8058_S4_VOLTAGE);
	if (rc == 0)
		return vreg_ptr;
	else
		return NULL;
}

static void vreg_deinit(struct regulator *vreg)
{
	regulator_put(vreg);
}

static void vreg_mode_vote(struct regulator *vreg, int enable, int mode)
{
	int rc;
	if (enable) {
		rc = regulator_enable(vreg);
		if (rc != 0)
			pr_err("%s:Enabling regulator failed\n", __func__);
		else {
			if (mode)
				regulator_set_optimum_mode(vreg,
						SNDDEV_VREG_HIGH_POWER_LOAD);
			else
				regulator_set_optimum_mode(vreg,
						SNDDEV_VREG_LOW_POWER_LOAD);
		}
	} else {
		rc = regulator_disable(vreg);
		if (rc != 0)
			pr_err("%s:Disabling regulator failed\n", __func__);
	}
}

struct msm_cdcclk_ctl_state {
	unsigned int rx_mclk;
	unsigned int rx_mclk_requested;
	unsigned int tx_mclk;
	unsigned int tx_mclk_requested;
};

static struct msm_cdcclk_ctl_state the_msm_cdcclk_ctl_state;

static int msm_snddev_rx_mclk_request(void)
{
	int rc = 0;

	rc = gpio_request(the_msm_cdcclk_ctl_state.rx_mclk,
		"MSM_SNDDEV_RX_MCLK");
	if (rc < 0) {
		pr_err("%s: GPIO request for MSM SNDDEV RX failed\n", __func__);
		return rc;
	}
	the_msm_cdcclk_ctl_state.rx_mclk_requested = 1;
	return rc;
}
static int msm_snddev_tx_mclk_request(void)
{
	int rc = 0;

	rc = gpio_request(the_msm_cdcclk_ctl_state.tx_mclk,
		"MSM_SNDDEV_TX_MCLK");
	if (rc < 0) {
		pr_err("%s: GPIO request for MSM SNDDEV TX failed\n", __func__);
		return rc;
	}
	the_msm_cdcclk_ctl_state.tx_mclk_requested = 1;
	return rc;
}
static void msm_snddev_rx_mclk_free(void)
{
	if (the_msm_cdcclk_ctl_state.rx_mclk_requested) {
		gpio_free(the_msm_cdcclk_ctl_state.rx_mclk);
		the_msm_cdcclk_ctl_state.rx_mclk_requested = 0;
	}
}
static void msm_snddev_tx_mclk_free(void)
{
	if (the_msm_cdcclk_ctl_state.tx_mclk_requested) {
		gpio_free(the_msm_cdcclk_ctl_state.tx_mclk);
		the_msm_cdcclk_ctl_state.tx_mclk_requested = 0;
	}
}
static int get_msm_cdcclk_ctl_gpios(struct platform_device *pdev)
{
	int rc = 0;
	struct resource *res;

	/* Claim all of the GPIOs. */
	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
			"msm_snddev_rx_mclk");
	if (!res) {
		pr_err("%s: failed to get gpio MSM SNDDEV RX\n", __func__);
		return -ENODEV;
	}
	the_msm_cdcclk_ctl_state.rx_mclk = res->start;
	the_msm_cdcclk_ctl_state.rx_mclk_requested = 0;

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
			"msm_snddev_tx_mclk");
	if (!res) {
		pr_err("%s: failed to get gpio MSM SNDDEV TX\n", __func__);
		return -ENODEV;
	}
	the_msm_cdcclk_ctl_state.tx_mclk = res->start;
	the_msm_cdcclk_ctl_state.tx_mclk_requested = 0;

	return rc;
}
static int msm_cdcclk_ctl_probe(struct platform_device *pdev)
{
	int rc = 0;

	rc = get_msm_cdcclk_ctl_gpios(pdev);
	if (rc < 0) {
		pr_err("%s: GPIO configuration failed\n", __func__);
		return -ENODEV;
	}
	return rc;
}
static struct platform_driver msm_cdcclk_ctl_driver = {
	.probe = msm_cdcclk_ctl_probe,
	.driver = { .name = "msm_cdcclk_ctl"}
};

static int snddev_icodec_open_lb(struct snddev_icodec_state *icodec)
{
	int trc;
	struct snddev_icodec_drv_state *drv = &snddev_icodec_drv;

	/* Voting for low power is ok here as all use cases are
	 * supported in low power mode.
	 */
	if (drv->snddev_vreg)
		vreg_mode_vote(drv->snddev_vreg, 1,
					SNDDEV_LOW_POWER_MODE);

	if (icodec->data->voltage_on)
		icodec->data->voltage_on();

	trc = adie_codec_open(icodec->data->profile, &icodec->adie_path);
	if (IS_ERR_VALUE(trc))
		pr_err("%s: adie codec open failed\n", __func__);
	else
		adie_codec_setpath(icodec->adie_path,
					icodec->sample_rate, 256);

	if (icodec->adie_path)
		adie_codec_proceed_stage(icodec->adie_path,
					ADIE_CODEC_DIGITAL_ANALOG_READY);

	if (icodec->data->pamp_on)
		icodec->data->pamp_on();

	icodec->enabled = 1;

	return 0;
}
static int initialize_msm_icodec_gpios(struct platform_device *pdev)
{
	int rc = 0;
	struct resource *res;
	int i = 0;
	int *reg_defaults = pdev->dev.platform_data;

	while ((res = platform_get_resource(pdev, IORESOURCE_IO, i))) {
		rc = gpio_request(res->start, res->name);
		if (rc) {
			pr_err("%s: icodec gpio %d request failed\n", __func__,
				res->start);
			goto err;
		} else {
			/* This platform data structure only works if all gpio
			 * resources are to be used only in output mode.
			 * If gpio resources are added which are to be used in
			 * input mode, then the platform data structure will
			 * have to be changed.
			 */

			gpio_direction_output(res->start, reg_defaults[i]);
			gpio_free(res->start);
		}
		i++;
	}
err:
	return rc;
}
static int msm_icodec_gpio_probe(struct platform_device *pdev)
{
	int rc = 0;

	rc = initialize_msm_icodec_gpios(pdev);
	if (rc < 0) {
		pr_err("%s: GPIO configuration failed\n", __func__);
		return -ENODEV;
	}
	return rc;
}
static struct platform_driver msm_icodec_gpio_driver = {
	.probe = msm_icodec_gpio_probe,
	.driver = { .name = "msm_icodec_gpio"}
};

static int snddev_icodec_open_rx(struct snddev_icodec_state *icodec)
{
	int trc;
	int afe_channel_mode;
	union afe_port_config afe_config;
	struct snddev_icodec_drv_state *drv = &snddev_icodec_drv;

	pm_qos_update_request(&drv->rx_pm_qos_req,
			      msm_cpuidle_get_deep_idle_latency());

	if (drv->snddev_vreg) {
		if (!strcmp(icodec->data->name, "headset_stereo_rx"))
			vreg_mode_vote(drv->snddev_vreg, 1,
					SNDDEV_LOW_POWER_MODE);
		else
			vreg_mode_vote(drv->snddev_vreg, 1,
					SNDDEV_HIGH_POWER_MODE);
	}
	msm_snddev_rx_mclk_request();

	drv->rx_osrclk = clk_get_sys(NULL, "i2s_spkr_osr_clk");
	if (IS_ERR(drv->rx_osrclk))
		pr_err("%s master clock Error\n", __func__);

	trc =  clk_set_rate(drv->rx_osrclk,
			SNDDEV_ICODEC_CLK_RATE(icodec->sample_rate));
	if (IS_ERR_VALUE(trc)) {
		pr_err("ERROR setting m clock1\n");
		goto error_invalid_freq;
	}

	clk_prepare_enable(drv->rx_osrclk);
	drv->rx_bitclk = clk_get_sys(NULL, "i2s_spkr_bit_clk");
	if (IS_ERR(drv->rx_bitclk))
		pr_err("%s clock Error\n", __func__);

	/* Master clock = Sample Rate * OSR rate bit clock
	 * OSR Rate bit clock = bit/sample * channel master
	 * clock / bit clock = divider value = 8
	 */
	if (msm_codec_i2s_slave_mode) {
		pr_info("%s: configuring bit clock for slave mode\n",
				__func__);
		trc =  clk_set_rate(drv->rx_bitclk, 0);
	} else
		trc =  clk_set_rate(drv->rx_bitclk, 8);

	if (IS_ERR_VALUE(trc)) {
		pr_err("ERROR setting m clock1\n");
		goto error_adie;
	}
	clk_prepare_enable(drv->rx_bitclk);

	if (icodec->data->voltage_on)
		icodec->data->voltage_on();

	/* Configure ADIE */
	trc = adie_codec_open(icodec->data->profile, &icodec->adie_path);
	if (IS_ERR_VALUE(trc))
		pr_err("%s: adie codec open failed\n", __func__);
	else
		adie_codec_setpath(icodec->adie_path,
					icodec->sample_rate, 256);
	/* OSR default to 256, can be changed for power optimization
	 * If OSR is to be changed, need clock API for setting the divider
	 */

	switch (icodec->data->channel_mode) {
	case 2:
		afe_channel_mode = MSM_AFE_STEREO;
		break;
	case 1:
	default:
		afe_channel_mode = MSM_AFE_MONO;
		break;
	}
	afe_config.mi2s.channel = afe_channel_mode;
	afe_config.mi2s.bitwidth = 16;
	afe_config.mi2s.line = 1;
	afe_config.mi2s.format = MSM_AFE_I2S_FORMAT_LPCM;
	if (msm_codec_i2s_slave_mode)
		afe_config.mi2s.ws = 0;
	else
		afe_config.mi2s.ws = 1;

	trc = afe_open(icodec->data->copp_id, &afe_config, icodec->sample_rate);

	if (trc < 0)
		pr_err("%s: afe open failed, trc = %d\n", __func__, trc);

	/* Enable ADIE */
	if (icodec->adie_path) {
		adie_codec_proceed_stage(icodec->adie_path,
					ADIE_CODEC_DIGITAL_READY);
		adie_codec_proceed_stage(icodec->adie_path,
					ADIE_CODEC_DIGITAL_ANALOG_READY);
	}

	if (msm_codec_i2s_slave_mode)
		adie_codec_set_master_mode(icodec->adie_path, 1);
	else
		adie_codec_set_master_mode(icodec->adie_path, 0);

	/* Enable power amplifier */
	if (icodec->data->pamp_on) {
		if (icodec->data->pamp_on()) {
			pr_err("%s: Error turning on rx power\n", __func__);
			goto error_pamp;
		}
	}

	icodec->enabled = 1;

	pm_qos_update_request(&drv->rx_pm_qos_req, PM_QOS_DEFAULT_VALUE);
	return 0;

error_pamp:
error_adie:
	clk_disable_unprepare(drv->rx_osrclk);
error_invalid_freq:

	pr_err("%s: encounter error\n", __func__);

	pm_qos_update_request(&drv->rx_pm_qos_req, PM_QOS_DEFAULT_VALUE);
	return -ENODEV;
}

static int snddev_icodec_open_tx(struct snddev_icodec_state *icodec)
{
	int trc;
	int afe_channel_mode;
	union afe_port_config afe_config;
	struct snddev_icodec_drv_state *drv = &snddev_icodec_drv;;

	pm_qos_update_request(&drv->tx_pm_qos_req,
			      msm_cpuidle_get_deep_idle_latency());

	if (drv->snddev_vreg)
		vreg_mode_vote(drv->snddev_vreg, 1, SNDDEV_HIGH_POWER_MODE);

	/* Reuse pamp_on for TX platform-specific setup  */
	if (icodec->data->pamp_on) {
		if (icodec->data->pamp_on()) {
			pr_err("%s: Error turning on tx power\n", __func__);
			goto error_pamp;
		}
	}

	msm_snddev_tx_mclk_request();

	drv->tx_osrclk = clk_get_sys(NULL, "i2s_mic_osr_clk");
	if (IS_ERR(drv->tx_osrclk))
		pr_err("%s master clock Error\n", __func__);

	trc =  clk_set_rate(drv->tx_osrclk,
			SNDDEV_ICODEC_CLK_RATE(icodec->sample_rate));
	if (IS_ERR_VALUE(trc)) {
		pr_err("ERROR setting m clock1\n");
		goto error_invalid_freq;
	}

	clk_prepare_enable(drv->tx_osrclk);
	drv->tx_bitclk = clk_get_sys(NULL, "i2s_mic_bit_clk");
	if (IS_ERR(drv->tx_bitclk))
		pr_err("%s clock Error\n", __func__);

	/* Master clock = Sample Rate * OSR rate bit clock
	 * OSR Rate bit clock = bit/sample * channel master
	 * clock / bit clock = divider value = 8
	 */
	if (msm_codec_i2s_slave_mode) {
		pr_info("%s: configuring bit clock for slave mode\n",
				__func__);
		trc =  clk_set_rate(drv->tx_bitclk, 0);
	} else
		trc =  clk_set_rate(drv->tx_bitclk, 8);

	clk_prepare_enable(drv->tx_bitclk);

	/* Enable ADIE */
	trc = adie_codec_open(icodec->data->profile, &icodec->adie_path);
	if (IS_ERR_VALUE(trc))
		pr_err("%s: adie codec open failed\n", __func__);
	else
		adie_codec_setpath(icodec->adie_path,
					icodec->sample_rate, 256);

	switch (icodec->data->channel_mode) {
	case 2:
		afe_channel_mode = MSM_AFE_STEREO;
		break;
	case 1:
	default:
		afe_channel_mode = MSM_AFE_MONO;
		break;
	}
	afe_config.mi2s.channel = afe_channel_mode;
	afe_config.mi2s.bitwidth = 16;
	afe_config.mi2s.line = 1;
	afe_config.mi2s.format = MSM_AFE_I2S_FORMAT_LPCM;
	if (msm_codec_i2s_slave_mode)
		afe_config.mi2s.ws = 0;
	else
		afe_config.mi2s.ws = 1;

	trc = afe_open(icodec->data->copp_id, &afe_config, icodec->sample_rate);

	if (icodec->adie_path) {
		adie_codec_proceed_stage(icodec->adie_path,
					ADIE_CODEC_DIGITAL_READY);
		adie_codec_proceed_stage(icodec->adie_path,
					ADIE_CODEC_DIGITAL_ANALOG_READY);
	}

	if (msm_codec_i2s_slave_mode)
		adie_codec_set_master_mode(icodec->adie_path, 1);
	else
		adie_codec_set_master_mode(icodec->adie_path, 0);

	icodec->enabled = 1;

	pm_qos_update_request(&drv->tx_pm_qos_req, PM_QOS_DEFAULT_VALUE);
	return 0;

error_invalid_freq:

	if (icodec->data->pamp_off)
		icodec->data->pamp_off();

	pr_err("%s: encounter error\n", __func__);
error_pamp:
	pm_qos_update_request(&drv->tx_pm_qos_req, PM_QOS_DEFAULT_VALUE);
	return -ENODEV;
}

static int snddev_icodec_close_lb(struct snddev_icodec_state *icodec)
{
	struct snddev_icodec_drv_state *drv = &snddev_icodec_drv;

	/* Disable power amplifier */
	if (icodec->data->pamp_off)
		icodec->data->pamp_off();

	if (drv->snddev_vreg)
		vreg_mode_vote(drv->snddev_vreg, 0, SNDDEV_LOW_POWER_MODE);

	if (icodec->adie_path) {
		adie_codec_proceed_stage(icodec->adie_path,
			ADIE_CODEC_DIGITAL_OFF);
		adie_codec_close(icodec->adie_path);
		icodec->adie_path = NULL;
	}

	if (icodec->data->voltage_off)
		icodec->data->voltage_off();

	return 0;
}

static int snddev_icodec_close_rx(struct snddev_icodec_state *icodec)
{
	struct snddev_icodec_drv_state *drv = &snddev_icodec_drv;

	pm_qos_update_request(&drv->rx_pm_qos_req,
			      msm_cpuidle_get_deep_idle_latency());

	if (drv->snddev_vreg)
		vreg_mode_vote(drv->snddev_vreg, 0, SNDDEV_HIGH_POWER_MODE);

	/* Disable power amplifier */
	if (icodec->data->pamp_off)
		icodec->data->pamp_off();

	/* Disable ADIE */
	if (icodec->adie_path) {
		adie_codec_proceed_stage(icodec->adie_path,
			ADIE_CODEC_DIGITAL_OFF);
		adie_codec_close(icodec->adie_path);
		icodec->adie_path = NULL;
	}

	afe_close(icodec->data->copp_id);

	if (icodec->data->voltage_off)
		icodec->data->voltage_off();

	clk_disable_unprepare(drv->rx_bitclk);
	clk_disable_unprepare(drv->rx_osrclk);

	msm_snddev_rx_mclk_free();

	icodec->enabled = 0;

	pm_qos_update_request(&drv->rx_pm_qos_req, PM_QOS_DEFAULT_VALUE);
	return 0;
}

static int snddev_icodec_close_tx(struct snddev_icodec_state *icodec)
{
	struct snddev_icodec_drv_state *drv = &snddev_icodec_drv;

	pm_qos_update_request(&drv->tx_pm_qos_req,
			      msm_cpuidle_get_deep_idle_latency());

	if (drv->snddev_vreg)
		vreg_mode_vote(drv->snddev_vreg, 0, SNDDEV_HIGH_POWER_MODE);

	/* Disable ADIE */
	if (icodec->adie_path) {
		adie_codec_proceed_stage(icodec->adie_path,
					ADIE_CODEC_DIGITAL_OFF);
		adie_codec_close(icodec->adie_path);
		icodec->adie_path = NULL;
	}

	afe_close(icodec->data->copp_id);

	clk_disable_unprepare(drv->tx_bitclk);
	clk_disable_unprepare(drv->tx_osrclk);

	msm_snddev_tx_mclk_free();

	/* Reuse pamp_off for TX platform-specific setup  */
	if (icodec->data->pamp_off)
		icodec->data->pamp_off();

	icodec->enabled = 0;

	pm_qos_update_request(&drv->tx_pm_qos_req, PM_QOS_DEFAULT_VALUE);
	return 0;
}

static int snddev_icodec_set_device_volume_impl(
		struct msm_snddev_info *dev_info, u32 volume)
{
	struct snddev_icodec_state *icodec;

	int rc = 0;

	icodec = dev_info->private_data;

	if (icodec->data->dev_vol_type & SNDDEV_DEV_VOL_DIGITAL) {

		rc = adie_codec_set_device_digital_volume(icodec->adie_path,
				icodec->data->channel_mode, volume);
		if (rc < 0) {
			pr_err("%s: unable to set_device_digital_volume for"
				"%s volume in percentage = %u\n",
				__func__, dev_info->name, volume);
			return rc;
		}

	} else if (icodec->data->dev_vol_type & SNDDEV_DEV_VOL_ANALOG) {
		rc = adie_codec_set_device_analog_volume(icodec->adie_path,
				icodec->data->channel_mode, volume);
		if (rc < 0) {
			pr_err("%s: unable to set_device_analog_volume for"
				"%s volume in percentage = %u\n",
				__func__, dev_info->name, volume);
			return rc;
		}
	} else {
		pr_err("%s: Invalid device volume control\n", __func__);
		return -EPERM;
	}
	return rc;
}

static int snddev_icodec_open(struct msm_snddev_info *dev_info)
{
	int rc = 0;
	struct snddev_icodec_state *icodec;
	struct snddev_icodec_drv_state *drv = &snddev_icodec_drv;

	if (!dev_info) {
		rc = -EINVAL;
		goto error;
	}

	icodec = dev_info->private_data;

	if (icodec->data->capability & SNDDEV_CAP_RX) {
		mutex_lock(&drv->rx_lock);
		if (drv->rx_active) {
			mutex_unlock(&drv->rx_lock);
			pr_err("%s: rx_active is set, return EBUSY\n",
				__func__);
			rc = -EBUSY;
			goto error;
		}
		rc = snddev_icodec_open_rx(icodec);

		if (!IS_ERR_VALUE(rc)) {
			if ((icodec->data->dev_vol_type & (
				SNDDEV_DEV_VOL_DIGITAL |
				SNDDEV_DEV_VOL_ANALOG)))
				rc = snddev_icodec_set_device_volume_impl(
						dev_info, dev_info->dev_volume);
			if (!IS_ERR_VALUE(rc))
				drv->rx_active = 1;
			else
				pr_err("%s: set_device_volume_impl"
					" error(rx) = %d\n", __func__, rc);
		}
		mutex_unlock(&drv->rx_lock);
	} else if (icodec->data->capability & SNDDEV_CAP_LB) {
		mutex_lock(&drv->lb_lock);
		rc = snddev_icodec_open_lb(icodec);

		if (!IS_ERR_VALUE(rc)) {
			if ((icodec->data->dev_vol_type & (
				SNDDEV_DEV_VOL_DIGITAL |
				SNDDEV_DEV_VOL_ANALOG)))
				rc = snddev_icodec_set_device_volume_impl(
						dev_info, dev_info->dev_volume);
		}

		mutex_unlock(&drv->lb_lock);
	} else {
		mutex_lock(&drv->tx_lock);
		if (drv->tx_active) {
			mutex_unlock(&drv->tx_lock);
			pr_err("%s: tx_active is set, return EBUSY\n",
				__func__);
			rc = -EBUSY;
			goto error;
		}
		rc = snddev_icodec_open_tx(icodec);

		if (!IS_ERR_VALUE(rc)) {
			if ((icodec->data->dev_vol_type & (
				SNDDEV_DEV_VOL_DIGITAL |
				SNDDEV_DEV_VOL_ANALOG)))
				rc = snddev_icodec_set_device_volume_impl(
						dev_info, dev_info->dev_volume);
			if (!IS_ERR_VALUE(rc))
				drv->tx_active = 1;
			else
				pr_err("%s: set_device_volume_impl"
					" error(tx) = %d\n", __func__, rc);
		}
		mutex_unlock(&drv->tx_lock);
	}
error:
	return rc;
}

static int snddev_icodec_close(struct msm_snddev_info *dev_info)
{
	int rc = 0;
	struct snddev_icodec_state *icodec;
	struct snddev_icodec_drv_state *drv = &snddev_icodec_drv;
	if (!dev_info) {
		rc = -EINVAL;
		goto error;
	}

	icodec = dev_info->private_data;

	if (icodec->data->capability & SNDDEV_CAP_RX) {
		mutex_lock(&drv->rx_lock);
		if (!drv->rx_active) {
			mutex_unlock(&drv->rx_lock);
			pr_err("%s: rx_active not set, return\n", __func__);
			rc = -EPERM;
			goto error;
		}
		rc = snddev_icodec_close_rx(icodec);
		if (!IS_ERR_VALUE(rc))
			drv->rx_active = 0;
		else
			pr_err("%s: close rx failed, rc = %d\n", __func__, rc);
		mutex_unlock(&drv->rx_lock);
	} else if (icodec->data->capability & SNDDEV_CAP_LB) {
		mutex_lock(&drv->lb_lock);
		rc = snddev_icodec_close_lb(icodec);
		mutex_unlock(&drv->lb_lock);
	} else {
		mutex_lock(&drv->tx_lock);
		if (!drv->tx_active) {
			mutex_unlock(&drv->tx_lock);
			pr_err("%s: tx_active not set, return\n", __func__);
			rc = -EPERM;
			goto error;
		}
		rc = snddev_icodec_close_tx(icodec);
		if (!IS_ERR_VALUE(rc))
			drv->tx_active = 0;
		else
			pr_err("%s: close tx failed, rc = %d\n", __func__, rc);
		mutex_unlock(&drv->tx_lock);
	}

error:
	return rc;
}

static int snddev_icodec_check_freq(u32 req_freq)
{
	int rc = -EINVAL;

	if ((req_freq != 0) && (req_freq >= 8000) && (req_freq <= 48000)) {
		if ((req_freq == 8000) || (req_freq == 11025) ||
			(req_freq == 12000) || (req_freq == 16000) ||
			(req_freq == 22050) || (req_freq == 24000) ||
			(req_freq == 32000) || (req_freq == 44100) ||
			(req_freq == 48000)) {
				rc = 0;
		} else
			pr_info("%s: Unsupported Frequency:%d\n", __func__,
								req_freq);
	}
	return rc;
}

static int snddev_icodec_set_freq(struct msm_snddev_info *dev_info, u32 rate)
{
	int rc;
	struct snddev_icodec_state *icodec;

	if (!dev_info) {
		rc = -EINVAL;
		goto error;
	}

	icodec = dev_info->private_data;
	if (adie_codec_freq_supported(icodec->data->profile, rate) != 0) {
		pr_err("%s: adie_codec_freq_supported() failed\n", __func__);
		rc = -EINVAL;
		goto error;
	} else {
		if (snddev_icodec_check_freq(rate) != 0) {
			pr_err("%s: check_freq failed\n", __func__);
			rc = -EINVAL;
			goto error;
		} else
			icodec->sample_rate = rate;
	}

	if (icodec->enabled) {
		snddev_icodec_close(dev_info);
		snddev_icodec_open(dev_info);
	}

	return icodec->sample_rate;

error:
	return rc;
}

static int snddev_icodec_enable_sidetone(struct msm_snddev_info *dev_info,
	u32 enable, uint16_t gain)
{
	int rc = 0;
	struct snddev_icodec_state *icodec;
	struct snddev_icodec_drv_state *drv = &snddev_icodec_drv;

	if (!dev_info) {
		pr_err("invalid dev_info\n");
		rc = -EINVAL;
		goto error;
	}

	icodec = dev_info->private_data;

	if (icodec->data->capability & SNDDEV_CAP_RX) {
		mutex_lock(&drv->rx_lock);
		if (!drv->rx_active || !dev_info->opened) {
			pr_err("dev not active\n");
			rc = -EPERM;
			mutex_unlock(&drv->rx_lock);
			goto error;
		}
		rc = afe_sidetone(PRIMARY_I2S_TX, PRIMARY_I2S_RX, enable, gain);
		if (rc < 0)
			pr_err("%s: AFE command sidetone failed\n", __func__);
		mutex_unlock(&drv->rx_lock);
	} else {
		rc = -EINVAL;
		pr_err("rx device only\n");
	}

error:
	return rc;

}
static int snddev_icodec_enable_anc(struct msm_snddev_info *dev_info,
	u32 enable)
{
	int rc = 0;
	struct adie_codec_anc_data *reg_writes;
	struct acdb_cal_block cal_block;
	struct snddev_icodec_state *icodec;
	struct snddev_icodec_drv_state *drv = &snddev_icodec_drv;

	pr_info("%s: enable=%d\n", __func__, enable);

	if (!dev_info) {
		pr_err("invalid dev_info\n");
		rc = -EINVAL;
		goto error;
	}
	icodec = dev_info->private_data;

	if ((icodec->data->capability & SNDDEV_CAP_RX) &&
		(icodec->data->capability & SNDDEV_CAP_ANC)) {
		mutex_lock(&drv->rx_lock);

		if (!drv->rx_active || !dev_info->opened) {
			pr_err("dev not active\n");
			rc = -EPERM;
			mutex_unlock(&drv->rx_lock);
			goto error;
		}
		if (enable) {
			get_anc_cal(&cal_block);
			reg_writes = (struct adie_codec_anc_data *)
				cal_block.cal_kvaddr;

			if (reg_writes == NULL) {
				pr_err("error, no calibration data\n");
				rc = -1;
				mutex_unlock(&drv->rx_lock);
				goto error;
			}

			rc = adie_codec_enable_anc(icodec->adie_path,
			1, reg_writes);
		} else {
			rc = adie_codec_enable_anc(icodec->adie_path,
			0, NULL);
		}
		mutex_unlock(&drv->rx_lock);
	} else {
		rc = -EINVAL;
		pr_err("rx and ANC device only\n");
	}

error:
	return rc;

}

int snddev_icodec_set_device_volume(struct msm_snddev_info *dev_info,
		u32 volume)
{
	struct snddev_icodec_state *icodec;
	struct mutex *lock;
	struct snddev_icodec_drv_state *drv = &snddev_icodec_drv;
	int rc = -EPERM;

	if (!dev_info) {
		pr_info("%s : device not intilized.\n", __func__);
		return  -EINVAL;
	}

	icodec = dev_info->private_data;

	if (!(icodec->data->dev_vol_type & (SNDDEV_DEV_VOL_DIGITAL
				| SNDDEV_DEV_VOL_ANALOG))) {

		pr_info("%s : device %s does not support device volume "
				"control.", __func__, dev_info->name);
		return -EPERM;
	}
	dev_info->dev_volume =  volume;

	if (icodec->data->capability & SNDDEV_CAP_RX)
		lock = &drv->rx_lock;
	else if (icodec->data->capability & SNDDEV_CAP_LB)
		lock = &drv->lb_lock;
	else
		lock = &drv->tx_lock;

	mutex_lock(lock);

	rc = snddev_icodec_set_device_volume_impl(dev_info,
			dev_info->dev_volume);
	mutex_unlock(lock);
	return rc;
}

static int snddev_icodec_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct snddev_icodec_data *pdata;
	struct msm_snddev_info *dev_info;
	struct snddev_icodec_state *icodec;

	if (!pdev || !pdev->dev.platform_data) {
		printk(KERN_ALERT "Invalid caller\n");
		rc = -1;
		goto error;
	}
	pdata = pdev->dev.platform_data;
	if ((pdata->capability & SNDDEV_CAP_RX) &&
	   (pdata->capability & SNDDEV_CAP_TX)) {
		pr_err("%s: invalid device data either RX or TX\n", __func__);
		goto error;
	}
	icodec = kzalloc(sizeof(struct snddev_icodec_state), GFP_KERNEL);
	if (!icodec) {
		rc = -ENOMEM;
		goto error;
	}
	dev_info = kmalloc(sizeof(struct msm_snddev_info), GFP_KERNEL);
	if (!dev_info) {
		kfree(icodec);
		rc = -ENOMEM;
		goto error;
	}

	dev_info->name = pdata->name;
	dev_info->copp_id = pdata->copp_id;
	dev_info->private_data = (void *) icodec;
	dev_info->dev_ops.open = snddev_icodec_open;
	dev_info->dev_ops.close = snddev_icodec_close;
	dev_info->dev_ops.set_freq = snddev_icodec_set_freq;
	dev_info->dev_ops.set_device_volume = snddev_icodec_set_device_volume;
	dev_info->capability = pdata->capability;
	dev_info->opened = 0;
	msm_snddev_register(dev_info);
	icodec->data = pdata;
	icodec->sample_rate = pdata->default_sample_rate;
	dev_info->sample_rate = pdata->default_sample_rate;
	dev_info->channel_mode = pdata->channel_mode;
	if (pdata->capability & SNDDEV_CAP_RX)
		dev_info->dev_ops.enable_sidetone =
			snddev_icodec_enable_sidetone;
	else
		dev_info->dev_ops.enable_sidetone = NULL;

	if (pdata->capability & SNDDEV_CAP_ANC) {
		dev_info->dev_ops.enable_anc =
		snddev_icodec_enable_anc;
	} else {
		dev_info->dev_ops.enable_anc = NULL;
	}
error:
	return rc;
}

static int snddev_icodec_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver snddev_icodec_driver = {
  .probe = snddev_icodec_probe,
  .remove = snddev_icodec_remove,
  .driver = { .name = "snddev_icodec" }
};

module_param(msm_codec_i2s_slave_mode, bool, 0);
MODULE_PARM_DESC(msm_codec_i2s_slave_mode, "Set MSM to I2S slave clock mode");

static int __init snddev_icodec_init(void)
{
	s32 rc;
	struct snddev_icodec_drv_state *icodec_drv = &snddev_icodec_drv;

	rc = platform_driver_register(&snddev_icodec_driver);
	if (IS_ERR_VALUE(rc)) {
		pr_err("%s: platform_driver_register for snddev icodec failed\n",
					__func__);
		goto error_snddev_icodec_driver;
	}

	rc = platform_driver_register(&msm_cdcclk_ctl_driver);
	if (IS_ERR_VALUE(rc)) {
		pr_err("%s: platform_driver_register for msm snddev failed\n",
					__func__);
		goto error_msm_cdcclk_ctl_driver;
	}

	rc = platform_driver_register(&msm_icodec_gpio_driver);
	if (IS_ERR_VALUE(rc)) {
		pr_err("%s: platform_driver_register for msm snddev gpio failed\n",
					__func__);
		goto error_msm_icodec_gpio_driver;
	}

	mutex_init(&icodec_drv->rx_lock);
	mutex_init(&icodec_drv->lb_lock);
	mutex_init(&icodec_drv->tx_lock);
	icodec_drv->rx_active = 0;
	icodec_drv->tx_active = 0;
	icodec_drv->snddev_vreg = vreg_init();

	pm_qos_add_request(&icodec_drv->tx_pm_qos_req, PM_QOS_CPU_DMA_LATENCY,
				PM_QOS_DEFAULT_VALUE);
	pm_qos_add_request(&icodec_drv->rx_pm_qos_req, PM_QOS_CPU_DMA_LATENCY,
				PM_QOS_DEFAULT_VALUE);
	return 0;
error_msm_icodec_gpio_driver:
	platform_driver_unregister(&msm_cdcclk_ctl_driver);
error_msm_cdcclk_ctl_driver:
	platform_driver_unregister(&snddev_icodec_driver);
error_snddev_icodec_driver:
	return -ENODEV;
}

static void __exit snddev_icodec_exit(void)
{
	struct snddev_icodec_drv_state *icodec_drv = &snddev_icodec_drv;

	platform_driver_unregister(&snddev_icodec_driver);
	platform_driver_unregister(&msm_cdcclk_ctl_driver);
	platform_driver_unregister(&msm_icodec_gpio_driver);

	clk_put(icodec_drv->rx_osrclk);
	clk_put(icodec_drv->tx_osrclk);
	if (icodec_drv->snddev_vreg) {
		vreg_deinit(icodec_drv->snddev_vreg);
		icodec_drv->snddev_vreg = NULL;
	}
	return;
}

module_init(snddev_icodec_init);
module_exit(snddev_icodec_exit);

MODULE_DESCRIPTION("ICodec Sound Device driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
