/*
 * max98927.c -- ALSA SoC Stereo MAX98927 driver
 * Copyright 2013-15 Maxim Integrated Products
 * Copyright (C) 2018 XiaoMi, Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <sound/tlv.h>
#include <linux/debugfs.h>
#include "max98927.h"

#if 0
#undef pr_info
#define pr_info pr_err

#undef pr_debug
#define pr_debug pr_err
#endif

/* #define USE_PA_MAX98928 1 */

struct max98927_priv *g_max98927 = NULL;
EXPORT_SYMBOL_GPL(g_max98927);


#define USE_DSM_MISC_DEV 1
#ifdef USE_PA_MAX98928
#define MAX_BOOST_VOLT		(0xc)
#else
#define MAX_BOOST_VOLT		(0x1c)
#endif

#ifdef USE_DSM_MISC_DEV
extern int afe_dsm_rx_set_params(uint8_t *payload, int size);
extern int afe_dsm_rx_get_params(uint8_t *payload, int size);
extern int afe_dsm_set_calib(uint8_t *payload);
extern int afe_dsm_pre_calib(uint8_t *payload);
extern int afe_dsm_post_calib(uint8_t *payload);
extern int afe_dsm_get_calib(uint8_t *payload);
extern int afe_dsm_get_average_calib(uint8_t *payload);
extern int afe_dsm_ramp_dn_cfg(uint8_t *payload, int delay_in_ms);
extern int afe_dsm_get_libary_info(uint32_t *payload, int size);
static DEFINE_MUTEX(dsm_lock);
#endif

#define Q_DSM_ADAPTIVE_FC 9
#define Q_DSM_ADAPTIVE_DC_RES 27

static unsigned int i2c_states;
static int delay_array_msec[] = {10, 20, 30, 40, 50};

int reg_common_map[][2] = {
	{MAX98927_Brownout_level_infinite_hold,  0x00},
	{MAX98927_Brownout_level_hold,  0x00},
	{MAX98927_Brownout__level_1_current_limit,  0x14},
	{MAX98927_Brownout__level_1_amp_1_control_1,  0x00},
	{MAX98927_Brownout__level_1_amp_1_control_2,  0x0c},
	{MAX98927_Brownout__level_1_amp_1_control_3,  0x00},
	{MAX98927_Brownout__level_2_current_limit,  0x10},
	{MAX98927_Brownout__level_2_amp_1_control_1,  0x00},
	{MAX98927_Brownout__level_2_amp_1_control_2,  0x0c},
	{MAX98927_Brownout__level_2_amp_1_control_3,  0x00},
	{MAX98927_Brownout__level_3_current_limit,  0x0c},
	{MAX98927_Brownout__level_3_amp_1_control_1,  0x06},
	{MAX98927_Brownout__level_3_amp_1_control_2,  0x18},
	{MAX98927_Brownout__level_3_amp_1_control_3,  0x0c},
	{MAX98927_Brownout__level_4_current_limit,  0x08},
	{MAX98927_Brownout__level_4_amp_1_control_1,  0x0e},
	{MAX98927_Brownout__level_4_amp_1_control_2,  0x80},
	{MAX98927_Brownout__level_4_amp_1_control_3,  0x00},
	{MAX98927_Brownout_threshold_hysterysis,  0x00},
	{MAX98927_Brownout_AMP_limiter_attack_release,  0x00},
	{MAX98927_Brownout_AMP_gain_attack_release,  0x00},
	{MAX98927_Brownout_AMP1_clip_mode,  0x00},
	{MAX98927_Meas_ADC_Config, 0x07},
	{MAX98927_Meas_ADC_Thermal_Warning_Threshhold, 0x78},
	{MAX98927_Meas_ADC_Thermal_Shutdown_Threshhold, 0xFF},
	{MAX98927_Pin_Config,  0x55},
	{MAX98927_Measurement_DSP_Config, 0xF7},
	{MAX98927_PCM_Tx_Enables_B, 0x00},
	{MAX98927_PCM_Rx_Enables_B, 0x00},
	{MAX98927_PCM_Tx_Channel_Sources_B, 0x00},
	{MAX98927_PCM_Tx_HiZ_Control_B, 0xFF},
	{MAX98927_Measurement_enables, 0x03},
	{MAX98927_PDM_Rx_Enable,  0x00},
	{MAX98927_AMP_volume_control,  0x38},
	{MAX98927_AMP_DSP_Config,  0x33},
	{MAX98927_DRE_Control, 0x01},
	{MAX98927_Speaker_Gain,  0x05},
	{MAX98927_SSM_Configuration,  0x85},
	{MAX98927_Boost_Control_0, MAX_BOOST_VOLT},
	{MAX98927_Boost_Control_1, 0x3e},
	{MAX98927_Meas_ADC_Base_Divide_MSByte, 0x00},
	{MAX98927_Meas_ADC_Base_Divide_LSByte, 0x00},
	{MAX98927_Meas_ADC_Thermal_Hysteresis, 0x00},
	{MAX98927_Env_Tracker_Vout_Headroom, 0x0C},
	{MAX98927_Env_Tracker_Control,  0x01},
	{MAX98927_Brownout_enables,  0x00},
};

int reg_channel_map[][7][2] = {
	{
		{MAX98927_Boost_Control_3, 0x01},
		{MAX98927_PCM_Tx_Channel_Sources_A, 0x01},
		{MAX98927_PCM_Rx_Enables_A, 0x03},
		{MAX98927_PCM_Tx_Enables_A, 0x03},
		{MAX98927_PCM_Tx_HiZ_Control_A, 0xFC},
		{MAX98927_PCM_to_speaker_monomix_A, 0x80},
		{MAX98927_PCM_to_speaker_monomix_B, 0x00},
	},
	{
		{MAX98927_Boost_Control_3, 0x01},
		{MAX98927_PCM_Tx_Channel_Sources_A, 0x00},
		{MAX98927_PCM_Rx_Enables_A, 0x01},
		{MAX98927_PCM_Tx_Enables_A, 0x01},
		{MAX98927_PCM_Tx_HiZ_Control_A, 0xFE},
		{MAX98927_PCM_to_speaker_monomix_A, 0x80},
		{MAX98927_PCM_to_speaker_monomix_B, 0x00},
	},
	{
		{MAX98927_Boost_Control_3, 0x09},
		{MAX98927_PCM_Tx_Channel_Sources_A, 0x11},
		{MAX98927_PCM_Rx_Enables_A, 0x02},
		{MAX98927_PCM_Tx_Enables_A, 0x02},
		{MAX98927_PCM_Tx_HiZ_Control_A, 0xFD},
		{MAX98927_PCM_to_speaker_monomix_A, 0x81},
		{MAX98927_PCM_to_speaker_monomix_B, 0x01},
	},

};

static bool max98927_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
		case MAX98927_Interrupt_Raw_1:
		case MAX98927_Interrupt_Raw_2:
		case MAX98927_Interrupt_Raw_3:
		case MAX98927_Interrupt_State_1:
		case MAX98927_Interrupt_State_2:
		case MAX98927_Interrupt_State_3:
		case MAX98927_Interrupt_Flag_1:
		case MAX98927_Interrupt_Flag_2:
		case MAX98927_Interrupt_Flag_3:
		case MAX98927_Interrupt_Enable_1:
		case MAX98927_Interrupt_Enable_2:
		case MAX98927_Interrupt_Enable_3:
		case MAX98927_IRQ_Control:
		case MAX98927_Clock_monitor_enable:
		case MAX98927_Watchdog_Control:
		case MAX98927_Meas_ADC_Thermal_Warning_Threshhold:
		case MAX98927_Meas_ADC_Thermal_Shutdown_Threshhold:
		case MAX98927_Meas_ADC_Thermal_Hysteresis:
		case MAX98927_Pin_Config:
		case MAX98927_PCM_Rx_Enables_A:
		case MAX98927_PCM_Rx_Enables_B:
		case MAX98927_PCM_Tx_Enables_A:
		case MAX98927_PCM_Tx_Enables_B:
		case MAX98927_PCM_Tx_HiZ_Control_A:
		case MAX98927_PCM_Tx_HiZ_Control_B:
		case MAX98927_PCM_Tx_Channel_Sources_A:
		case MAX98927_PCM_Tx_Channel_Sources_B:
		case MAX98927_PCM_Mode_Config:
		case MAX98927_PCM_Master_Mode:
		case MAX98927_PCM_Clock_setup:
		case MAX98927_PCM_Sample_rate_setup_1:
		case MAX98927_PCM_Sample_rate_setup_2:
		case MAX98927_PCM_to_speaker_monomix_A:
		case MAX98927_PCM_to_speaker_monomix_B:
		case MAX98927_ICC_RX_Enables_A:
		case MAX98927_ICC_RX_Enables_B:
		case MAX98927_ICC_TX_Enables_A:
		case MAX98927_ICC_TX_Enables_B:
		case MAX98927_ICC_Data_Order_Select:
		case MAX98927_ICC_HiZ_Manual_Mode:
		case MAX98927_ICC_TX_HiZ_Enables_A:
		case MAX98927_ICC_TX_HiZ_Enables_B:
		case MAX98927_ICC_Link_Enables:
		case MAX98927_PDM_Tx_Enables:
		case MAX98927_PDM_Tx_HiZ_Control:
		case MAX98927_PDM_Tx_Control:
		case MAX98927_PDM_Rx_Enable:
		case MAX98927_AMP_volume_control:
		case MAX98927_AMP_DSP_Config:
		case MAX98927_Tone_Generator_and_DC_Config:
		case MAX98927_DRE_Control:
		case MAX98927_AMP_enables:
		case MAX98927_Speaker_source_select:
		case MAX98927_Speaker_Gain:
		case MAX98927_SSM_Configuration:
		case MAX98927_Measurement_enables:
		case MAX98927_Measurement_DSP_Config:
		case MAX98927_Boost_Control_0:
		case MAX98927_Boost_Control_3:
		case MAX98927_Boost_Control_1:
		case MAX98927_Meas_ADC_Config:
		case MAX98927_Meas_ADC_Base_Divide_MSByte:
		case MAX98927_Meas_ADC_Base_Divide_LSByte:
		case MAX98927_Meas_ADC_Chan_0_Divide:
		case MAX98927_Meas_ADC_Chan_1_Divide:
		case MAX98927_Meas_ADC_Chan_2_Divide:
		case MAX98927_Meas_ADC_Chan_0_Filt_Config:
		case MAX98927_Meas_ADC_Chan_1_Filt_Config:
		case MAX98927_Meas_ADC_Chan_2_Filt_Config:
		case MAX98927_Meas_ADC_Chan_0_Readback:
		case MAX98927_Meas_ADC_Chan_1_Readback:
		case MAX98927_Meas_ADC_Chan_2_Readback:
		case MAX98927_Brownout_status:
		case MAX98927_Brownout_enables:
		case MAX98927_Brownout_level_infinite_hold:
		case MAX98927_Brownout_level_hold:
		case MAX98927_Brownout__level_1_threshold:
		case MAX98927_Brownout__level_2_threshold:
		case MAX98927_Brownout__level_3_threshold:
		case MAX98927_Brownout__level_4_threshold:
		case MAX98927_Brownout_threshold_hysterysis:
		case MAX98927_Brownout_AMP_limiter_attack_release:
		case MAX98927_Brownout_AMP_gain_attack_release:
		case MAX98927_Brownout_AMP1_clip_mode:
		case MAX98927_Brownout__level_1_current_limit:
		case MAX98927_Brownout__level_1_amp_1_control_1:
		case MAX98927_Brownout__level_1_amp_1_control_2:
		case MAX98927_Brownout__level_1_amp_1_control_3:
		case MAX98927_Brownout__level_2_current_limit:
		case MAX98927_Brownout__level_2_amp_1_control_1:
		case MAX98927_Brownout__level_2_amp_1_control_2:
		case MAX98927_Brownout__level_2_amp_1_control_3:
		case MAX98927_Brownout__level_3_current_limit:
		case MAX98927_Brownout__level_3_amp_1_control_1:
		case MAX98927_Brownout__level_3_amp_1_control_2:
		case MAX98927_Brownout__level_3_amp_1_control_3:
		case MAX98927_Brownout__level_4_current_limit:
		case MAX98927_Brownout__level_4_amp_1_control_1:
		case MAX98927_Brownout__level_4_amp_1_control_2:
		case MAX98927_Brownout__level_4_amp_1_control_3:
		case MAX98927_Env_Tracker_Vout_Headroom:
		case MAX98927_Env_Tracker_Boost_Vout_Delay:
		case MAX98927_Env_Tracker_Release_Rate:
		case MAX98927_Env_Tracker_Hold_Rate:
		case MAX98927_Env_Tracker_Control:
		case MAX98927_Env_Tracker__Boost_Vout_ReadBack:
		case MAX98927_Global_Enable:
		case MAX98927_REV_ID:
			return true;
		default:
			return false;
	}
}

static bool max98927_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
		case MAX98927_Interrupt_Raw_1:
		case MAX98927_Interrupt_Raw_2:
		case MAX98927_Interrupt_Raw_3:
		case MAX98927_Interrupt_State_1:
		case MAX98927_Interrupt_State_2:
		case MAX98927_Interrupt_State_3:
		case MAX98927_Interrupt_Flag_1:
		case MAX98927_Interrupt_Flag_2:
		case MAX98927_Interrupt_Flag_3:
		case MAX98927_Meas_ADC_Chan_0_Readback:
		case MAX98927_Meas_ADC_Chan_1_Readback:
		case MAX98927_Meas_ADC_Chan_2_Readback:
		case MAX98927_Brownout_status:
		case MAX98927_Env_Tracker__Boost_Vout_ReadBack:
			return true;
		default:
			return false;
	}
}

#ifdef USE_DSM_MISC_DEV
#define PKG_HEADER (48)
#define PAYLOAD_COUNT (110)

#if 1
typedef enum {
	DSM_API_MONO_SPKER                  = 0x00000000,
	DSM_API_STEREO_SPKER                = 0x03000000,

	DSM_API_L_CHAN                      = 0x01000000,
	DSM_API_R_CHAN                      = 0x02000000,

	DSM_API_CHANNEL_1                   = 0x01000000,
	DSM_API_CHANNEL_2                   = 0x02000000,
	DSM_API_CHANNEL_3                   = 0x04000000,
	DSM_API_CHANNEL_4                   = 0x08000000,
	DSM_API_CHANNEL_5                   = 0x10000000,
	DSM_API_CHANNEL_6                   = 0x20000000,
	DSM_API_CHANNEL_7                   = 0x40000000,
	DSM_API_CHANNEL_8                   = 0x80000000,

	DSM_MAX_SUPPORTED_CHANNELS          = 8
} DSM_API_CHANNEL_ID;

#define DSM_SET_MONO_PARAM(cmdId)       ((cmdId&0x00FFFFFF)|DSM_API_MONO_SPKER)
#define DSM_SET_STEREO_PARAM(cmdId)     ((cmdId&0x00FFFFFF)|DSM_API_STEREO_SPKER)
#define DSM_SET_LEFT_PARAM(cmdId)       ((cmdId&0x00FFFFFF)|DSM_API_L_CHAN)
#define DSM_SET_RIGHT_PARAM(cmdId)      ((cmdId&0x00FFFFFF)|DSM_API_R_CHAN)

enum working_mode {
    DSM_MODE_NONE = 0,
    DSM_MODE_LEFT_ONLY,
    DSM_MODE_RIGHT_ONLY,
    DSM_MODE_LEFT_RIGHT,
    DSM_MODE_RIGHT_LEFT,
    DSM_MODE_CALIB_START,
    DSM_MODE_CALIB_ING,
    DSM_MODE_RDC,
    DSM_MODE_CALIB_DONE,
};

typedef struct dsm_params {
    uint32_t mode;
    uint32_t pcount;
    uint32_t pdata[PAYLOAD_COUNT];
} dsm_param_t;

struct param_info {
    int pid;
    char name[80];
    int q_val;
};



#define RDC_MIN  (241833636)
#define RDC_MAX  (403056239)

#endif

static uint32_t gParam[PKG_HEADER+PAYLOAD_COUNT];

static int maxdsm_open(struct inode *inode, struct file *filep)
{
	return 0;
}

#define ADAPTIVE_FC (16)
#define ADAPTIVE_DC_RES (18)

static ssize_t maxdsm_read(struct file *filep, char __user *buf,
		size_t count, loff_t *ppos)
{
	int rc;
	uint8_t *payload = (uint8_t *)&gParam[PKG_HEADER];

	if (count > sizeof(uint32_t)*PAYLOAD_COUNT)
		count = sizeof(uint32_t)*PAYLOAD_COUNT;

	mutex_lock(&dsm_lock);
	rc = afe_dsm_rx_get_params(payload, sizeof(uint32_t)*PAYLOAD_COUNT);

	if (rc != 0) {
		pr_err("%s: afe_dsm_rx_get_params failed - %d\n", __func__, rc);
	}
	rc = copy_to_user(buf, payload, count);
	if (rc != 0) {
		pr_err("%s: copy_to_user failed - %d\n", __func__, rc);
	}
	mutex_unlock(&dsm_lock);

	return rc;
}

static ssize_t maxdsm_write(struct file *filep, const char __user *buf,
		size_t count, loff_t *ppos)
{
	int rc;
	uint8_t *payload = (uint8_t *)&gParam[PKG_HEADER];

	if (count > sizeof(uint32_t)*PAYLOAD_COUNT)
		count = sizeof(uint32_t)*PAYLOAD_COUNT;

	mutex_lock(&dsm_lock);
	rc = copy_from_user(payload, buf, count);
	if (rc != 0) {
		pr_err("%s: copy_from_user failed - %d\n", __func__, rc);
		goto exit;
	}

	afe_dsm_rx_set_params(payload, count);
exit:
	mutex_unlock(&dsm_lock);

	return rc;
}

static const struct file_operations dsm_ctrl_fops = {
	.owner		= THIS_MODULE,
	.open		= maxdsm_open,
	.release	= NULL,
	.read		= maxdsm_read,
	.write		= maxdsm_write,
	.mmap		= NULL,
	.poll		= NULL,
	.fasync		= NULL,
	.llseek		= NULL,
};

static struct miscdevice dsm_ctrl_miscdev = {
	.minor =	MISC_DYNAMIC_MINOR,
	.name =		"dsm_ctrl_dev",
	.fops =		&dsm_ctrl_fops
};
#endif

/* max. length of a alsa mixer control name */
#define MAX_CONTROL_NAME        48
#define CALIBRATE_FILE   "/persist/spkr_calibration.bin"
#define CALIBRATE_FILE_R   "/persist/spkr_calibration_r.bin"

struct dsm_info_t {
    int32_t averageTemp[2];
	int32_t	bits_per_sample;
	int32_t	sampling_rate;
	int32_t	Q_factor;
	int32_t	working_mode;
	int32_t	enable_flag;
	int32_t	num_channels;
 	int32_t maxTempQ19[2];
	int32_t minTempQ19[2];
	int32_t maxFcQ9[2];
	int32_t minFcQ9[2];
	int32_t maxRdcQ27[2];
	int32_t minRdcQ27[2];
};


#if 0
static int max989xx_create_calibfile(void)
{
    struct file *pfile = NULL;
    mm_segment_t old_fs;
    int ret = 0;

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    pfile = filp_open(CALIBRATE_FILE, O_RDWR | O_CREAT, 0666);
    if (!IS_ERR(pfile)) {
        pr_info("%s: %s create success! \n", __func__, CALIBRATE_FILE);
        filp_close(pfile, NULL);
    } else {
        pr_info("%s: %s create failed! \n", __func__, CALIBRATE_FILE);
        ret = -1;
    }

    set_fs(old_fs);

    return ret;

}
#endif

static int max989xx_calib_save(uint32_t calib_value)
{
    struct file *pfile = NULL;
    mm_segment_t old_fs;
    int ret = 0;
    loff_t pos = 0;

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    pfile = filp_open(CALIBRATE_FILE, O_RDWR | O_CREAT, 0666);
    if (!IS_ERR(pfile)) {
        pr_info("%s: save calib_value=%d \n", __func__, calib_value);
        vfs_write(pfile, (char *)&calib_value, sizeof(uint32_t), &pos);
        filp_close(pfile, NULL);
    } else {
        pr_info("%s: %s open failed! \n", __func__, CALIBRATE_FILE);
        ret = -1;
    }

    set_fs(old_fs);

    return ret;
}

static int max989xx_calib_save_right(uint32_t calib_value)
{
	struct file *pfile = NULL;
	mm_segment_t old_fs;
	int ret = 0;
	loff_t pos = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	pfile = filp_open(CALIBRATE_FILE_R, O_RDWR | O_CREAT, 0666);
	if (!IS_ERR(pfile)) {
		pr_info("%s: save calib_value=%d \n", __func__, calib_value);
		vfs_write(pfile, (char *)&calib_value, sizeof(uint32_t), &pos);
		filp_close(pfile, NULL);
	} else {
		pr_info("%s: %s open failed! \n", __func__, CALIBRATE_FILE_R);
		ret = -1;
	}

	set_fs(old_fs);

	return ret;
}

static bool rdc_check_valid(uint32_t rdc)
{
    if (rdc > RDC_MIN && rdc < RDC_MAX) {
        return true;
    }

    pr_info("%s: rdc=%d invalid, [%d, %d] \n", __func__, rdc, RDC_MIN, RDC_MAX);
    return false;
}

#ifdef CONFIG_DEBUG_FS
static ssize_t max989xx_dbgfs_calibrate_read(struct file *file,
                     char __user *user_buf, size_t count,
                     loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct max98927_priv *max989xx = i2c_get_clientdata(i2c);
    uint32_t *payload = (uint32_t *)&gParam[PKG_HEADER];
    int ret = 0;
    uint32_t impedance_l, impedance_r;
    char *str;

    pr_info("%s: enter... \n", __func__);
    mutex_lock(&dsm_lock);
    ret = afe_dsm_pre_calib((uint8_t*)payload);
	ret |= afe_dsm_get_calib((uint8_t*)payload);
	if (ret == 0) {
        impedance_l = *payload;
        impedance_r = *(payload+1);
        if (!rdc_check_valid(impedance_l)) {
            impedance_l = 0xCACACACA;
            *payload = 0xCACACACA;
        }
		max989xx->ref_RDC[MAX98927L] = impedance_l;
		max989xx_calib_save(impedance_l);

		if (max989xx->mono_stereo == 3) {
			if (!rdc_check_valid(impedance_r)) {
				impedance_r = 0xCACACACA;
				*(payload+1) = 0xCACACACA;
			}
			max989xx->ref_RDC[MAX98927R] = impedance_r;
			max989xx_calib_save_right(impedance_r);
		}

		afe_dsm_set_calib((uint8_t *)(payload));
    } else {
        pr_info("%s failed to calibrate \n", __func__);
        ret = -EIO;
        goto exit;
    }

    str = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (!str) {
        pr_info("%s failed to kmalloc \n", __func__);
        ret = -ENOMEM;
        goto exit;
    }

    pr_info("%s: calibrate [impedance]=%d \n", __func__, impedance_l);
    ret = snprintf(str, PAGE_SIZE, "%d\n", impedance_l);
	if (max989xx->mono_stereo == 3) {
		pr_info("%s: calibrate [impedance_r]=%d \n", __func__, impedance_r);
		ret += snprintf(str+ret, PAGE_SIZE, "%d\n", impedance_r);
	}
    ret = simple_read_from_buffer(user_buf, count, ppos, str, ret);
    kfree(str);

exit:
    afe_dsm_post_calib((uint8_t*)payload);
    mutex_unlock(&dsm_lock);
    return ret;
}

static ssize_t max989xx_dbgfs_impedance_read(struct file *file,
                     char __user *user_buf, size_t count,
                     loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct max98927_priv *max989xx = i2c_get_clientdata(i2c);
    uint32_t *payload = (uint32_t *)&gParam[PKG_HEADER];
    int ret = 0;
	uint32_t impedance = 0, impedance_r = 0;
	char *str;

	afe_dsm_get_calib((uint8_t *)payload);
    impedance = *payload;
    if (!rdc_check_valid(impedance)) {
        pr_info("%s failed to read impedance. \n", __func__);
        ret = -EIO;
        goto exit;
	}

	if (max989xx->mono_stereo == 3) {
		impedance_r = *(payload + 1);
		if (!rdc_check_valid(impedance_r)) {
			pr_info("%s failed to read impedance_r. \n", __func__);
			ret = -EIO;
			goto exit;
		}
    }

    str = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (!str) {
        pr_info("%s failed to kmalloc \n", __func__);
        ret = -ENOMEM;
        goto exit;
    }

    pr_info("%s: [impedance] = %d \n", __func__, impedance);
    ret = snprintf(str, PAGE_SIZE, "%d\n", impedance);
	if (max989xx->mono_stereo == 3) {
		pr_info("%s: [impedance_r] = %d \n", __func__, impedance_r);
		ret += snprintf(str+ret, PAGE_SIZE, "%d\n", impedance_r);
	}
    ret = simple_read_from_buffer(user_buf, count, ppos, str, ret);
    kfree(str);

exit:

    return ret;
}

static ssize_t max989xx_dbgfs_info_read(struct file *file,
                     char __user *user_buf, size_t count,
                     loff_t *ppos)
{


    uint32_t *payload = (uint32_t *)&gParam[PKG_HEADER];
    int ret = 0;
    struct dsm_info_t *pInfo;
    char *str;

	afe_dsm_get_libary_info(payload, 100);
    pInfo = (struct dsm_info_t*) payload;

    str = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (!str) {
        pr_info("%s failed to kmalloc \n", __func__);
        ret = -ENOMEM;
        goto exit;
    }

    pr_info("%s: [average temp] = %d \n", __func__, pInfo->averageTemp[0]);
    ret = snprintf(str, PAGE_SIZE, "%d\n", pInfo->averageTemp[0]);
    ret = simple_read_from_buffer(user_buf, count, ppos, str, ret);
    kfree(str);

exit:

    return ret;
}

static ssize_t max989xx_dbgfs_f0_read(struct file *file,
                     char __user *user_buf, size_t count,
                     loff_t *ppos)
{


    uint32_t *payload = (uint32_t *)&gParam[PKG_HEADER];
    int ret = 0;
    uint32_t f0 = 0;
    char *str;

	afe_dsm_get_calib((uint8_t *)payload);
    f0 = *(payload+2);
	#if 0
    if (!rdc_check_valid(f0)) {
        pr_info("%s failed to read f0. \n", __func__);
        ret = -EIO;
        goto exit;
    }
    #endif
    str = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (!str) {
        pr_info("%s failed to kmalloc \n", __func__);
        ret = -ENOMEM;
        goto exit;
    }

    pr_info("%s: [f0] = %d \n", __func__, f0);
    ret = snprintf(str, PAGE_SIZE, "%d\n", f0);
    ret = simple_read_from_buffer(user_buf, count, ppos, str, ret);
    kfree(str);

exit:

    return ret;
}


static ssize_t max989xx_dbgfs_temperature_read(struct file *file,
                     char __user *user_buf, size_t count,
                     loff_t *ppos)
{


    uint32_t *payload = (uint32_t *)&gParam[PKG_HEADER];
    int ret = 0;
    uint32_t coiltemp = 0;
    char *str;


	afe_dsm_get_calib((uint8_t *)payload);
	coiltemp = *(payload + 4);
    pr_info("%s: [coiltemp] = %d \n", __func__, coiltemp);

    str = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (!str) {
        pr_info("%s failed to kmalloc \n", __func__);
        ret = -ENOMEM;
        goto exit;
    }

    ret = snprintf(str, PAGE_SIZE, "%d\n", coiltemp);
    ret = simple_read_from_buffer(user_buf, count, ppos, str, ret);
    kfree(str);

exit:

    return ret;
}

static const struct file_operations max989xx_dbgfs_calibrate_fops = {
    .open = simple_open,
    .read = max989xx_dbgfs_calibrate_read,
    .llseek = default_llseek,
};

static const struct file_operations max989xx_dbgfs_impedance_fops = {
    .open = simple_open,
    .read = max989xx_dbgfs_impedance_read,
    .llseek = default_llseek,
};

static const struct file_operations max989xx_dbgfs_f0_fops = {
    .open = simple_open,
    .read = max989xx_dbgfs_f0_read,
    .llseek = default_llseek,
};
static const struct file_operations max989xx_dbgfs_temperature_fops = {
    .open = simple_open,
    .read = max989xx_dbgfs_temperature_read,
    .llseek = default_llseek,
};

static const struct file_operations max989xx_dbgfs_info_fops = {
    .open = simple_open,
    .read = max989xx_dbgfs_info_read,
    .llseek = default_llseek,
};

static void max989xx_debug_init(struct max98927_priv *max989xx, struct i2c_client *i2c)
{
    char name[60];

    scnprintf(name, MAX_CONTROL_NAME, "%s", i2c->name);
    max989xx->dbg_dir = debugfs_create_dir(name, NULL);
    debugfs_create_file("calibrate", S_IRUGO|S_IWUGO, max989xx->dbg_dir,
                        i2c, &max989xx_dbgfs_calibrate_fops);
    debugfs_create_file("impedance", S_IRUGO|S_IWUGO, max989xx->dbg_dir,
                        i2c, &max989xx_dbgfs_impedance_fops);
    debugfs_create_file("f0detect", S_IRUGO|S_IWUGO, max989xx->dbg_dir,
                        i2c, &max989xx_dbgfs_f0_fops);
    debugfs_create_file("temperature", S_IRUGO|S_IWUGO, max989xx->dbg_dir,
                        i2c, &max989xx_dbgfs_temperature_fops);
    debugfs_create_file("infomation", S_IRUGO|S_IWUGO, max989xx->dbg_dir,
                        i2c, &max989xx_dbgfs_info_fops);

}

static void max989xx_debug_remove(struct max98927_priv *max989xx)
{
    if (max989xx->dbg_dir)
        debugfs_remove_recursive(max989xx->dbg_dir);
}
#endif

static ssize_t max98927_info_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
    pr_err("%s",__func__);

	return 0;
}
static ssize_t max98927_info_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{

    uint32_t *payload = (uint32_t *)&gParam[PKG_HEADER];
	int ret = 0;
	struct dsm_info_t *pInfo;

	afe_dsm_get_libary_info(payload, 100);
	pInfo = (struct dsm_info_t*) payload;

	pr_info("%s: [average temp] = %d \n", __func__, pInfo->averageTemp[0]);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", pInfo->averageTemp[0]);

	return ret;

}

static ssize_t max98927_QFactor_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
    pr_err("%s",__func__);

	return 0;
}
static ssize_t max98927_QFactor_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{

    uint32_t *payload = (uint32_t *)&gParam[PKG_HEADER];
	int ret = 0;
	uint32_t QFactor = 0;

	afe_dsm_get_calib((uint8_t *)payload);
	QFactor = *(payload + 6);
	pr_info("%s: [QFactor] = %d \n", __func__, QFactor);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", QFactor);

	return ret;
}


static ssize_t max98927_temperature_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
    pr_err("%s",__func__);

	return 0;
}
static ssize_t max98927_temperature_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{

    uint32_t *payload = (uint32_t *)&gParam[PKG_HEADER];
	int ret = 0;
	uint32_t coiltemp = 0;

	afe_dsm_get_calib((uint8_t *)payload);
	coiltemp = *(payload + 4);
	pr_info("%s: [coiltemp] = %d \n", __func__, coiltemp);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", coiltemp);

	return ret;

}


static ssize_t max98927_f0_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
    pr_err("%s",__func__);

	return 0;
}
static ssize_t max98927_f0_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{

	uint32_t *payload = (uint32_t *)&gParam[PKG_HEADER];
	int ret = 0;
	uint32_t f0 = 0;

	afe_dsm_get_calib((uint8_t *)payload);
	f0 = *(payload+2);

	pr_info("%s: [f0] = %d \n", __func__, f0);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", f0);

	return ret;

}


static ssize_t max98927_impedance_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
    pr_err("%s",__func__);

	return 0;
}
static ssize_t max98927_impedance_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{

    uint32_t *payload = (uint32_t *)&gParam[PKG_HEADER];
	int ret = 0;
	uint32_t impedance = 0;

	afe_dsm_get_calib((uint8_t *)payload);
	impedance = *payload;
	if (!rdc_check_valid(impedance)) {
		pr_info("%s failed to read impedance. \n", __func__);
		impedance = 0xCACACACA;
	}

	pr_info("%s: [impedance] = %d \n", __func__, impedance);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", impedance);

	return ret;

}


static ssize_t max98927_calibrate_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
    pr_err("%s",__func__);

	return 0;
}
static ssize_t max98927_calibrate_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{

    struct max98927_priv *max989xx = g_max98927;
    uint32_t *payload = (uint32_t *)&gParam[PKG_HEADER];
    int ret = 0;
    uint32_t impedance_l, impedance_r;




    pr_info("%s: enter... \n", __func__);
    mutex_lock(&dsm_lock);
    ret = afe_dsm_pre_calib((uint8_t*)payload);
	ret |= afe_dsm_get_calib((uint8_t*)payload);
	if (ret == 0) {
        impedance_l = *payload;
        impedance_r = *(payload+1);
        if (!rdc_check_valid(impedance_l)) {
            impedance_l = 0xCACACACA;
            *payload = 0xCACACACA;
        }
		max989xx->ref_RDC[MAX98927L] = impedance_l;

		max989xx_calib_save(impedance_l);

		if (max989xx->mono_stereo == 3) {
			if (!rdc_check_valid(impedance_r)) {
				impedance_r = 0xCACACACA;
				*(payload+1) = 0xCACACACA;
			}
			max989xx->ref_RDC[MAX98927R] = impedance_r;
			max989xx_calib_save_right(impedance_r);
		}

		afe_dsm_set_calib((uint8_t *)(payload));
    } else {
        pr_err("%s failed to calibrate \n", __func__);
		impedance_l = 0xCACACACA;
		*payload = 0xCACACACA;
    }
#if 0
    str = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (!str) {
        pr_info("%s failed to kmalloc \n", __func__);
        ret = -ENOMEM;
        goto exit;
    }

    pr_info("%s: calibrate [impedance]=%d \n", __func__, impedance_l);
    ret = snprintf(str, PAGE_SIZE, "%d\n", impedance_l);
		if (max989xx->mono_stereo == 3) {
			pr_info("%s: calibrate [impedance_r]=%d \n", __func__, impedance_r);
			ret += snprintf(str+ret, PAGE_SIZE, "%d\n", impedance_r);
		}
#endif
	ret = sprintf(buf,"%d\n",impedance_l);


    afe_dsm_post_calib((uint8_t*)payload);

    mutex_unlock(&dsm_lock);
    return ret;
}


static struct device_attribute max98927_calibrate_attr =
     __ATTR(calibrate, 0444, max98927_calibrate_show, max98927_calibrate_store);

static struct device_attribute max98927_impedance_attr =
     __ATTR(impedance, 0444, max98927_impedance_show, max98927_impedance_store);

static struct device_attribute max98927_f0_attr =
     __ATTR(f0, 0444, max98927_f0_show, max98927_f0_store);

static struct device_attribute max98927_temperature_attr =
     __ATTR(temperature, 0444, max98927_temperature_show, max98927_temperature_store);

static struct device_attribute max98927_QFactor_attr =
     __ATTR(QFactor, 0444, max98927_QFactor_show, max98927_QFactor_store);


static struct device_attribute max98927_info_attr =
     __ATTR(info, 0444, max98927_info_show, max98927_info_store);


static void max989xx_sys_init(struct i2c_client *i2c)
{
	int ret = 0;
	ret = sysfs_create_file(&i2c->dev.kobj, &max98927_calibrate_attr.attr);
	if (ret < 0)
	   pr_err("%s sysfs_create_file max98927_calibrate_attr err.",__func__);

    ret = sysfs_create_file(&i2c->dev.kobj, &max98927_impedance_attr.attr);
	if (ret < 0)
	   pr_err("%s sysfs_create_file max98927_impedance_attr err.",__func__);

    ret = sysfs_create_file(&i2c->dev.kobj, &max98927_f0_attr.attr);
	if (ret < 0)
	   pr_err("%s sysfs_create_file max98927_f0_attr err.",__func__);

	ret = sysfs_create_file(&i2c->dev.kobj, &max98927_temperature_attr.attr);
	if (ret < 0)
	   pr_err("%s sysfs_create_file max98927_temperature_attr err.",__func__);

	ret = sysfs_create_file(&i2c->dev.kobj, &max98927_QFactor_attr.attr);
	if (ret < 0)
	   pr_err("%s sysfs_create_file max98927_QFactor_attr err.",__func__);

	ret = sysfs_create_file(&i2c->dev.kobj, &max98927_info_attr.attr);
	if (ret < 0)
	   pr_err("%s sysfs_create_file max98927_info_attr err.",__func__);
    return;
}

static void max989xx_sys_remove(struct i2c_client *i2c)
{
	sysfs_remove_file(&i2c->dev.kobj, &max98927_calibrate_attr.attr);

	sysfs_remove_file(&i2c->dev.kobj, &max98927_impedance_attr.attr);

	sysfs_remove_file(&i2c->dev.kobj, &max98927_f0_attr.attr);

	sysfs_remove_file(&i2c->dev.kobj, &max98927_temperature_attr.attr);

	sysfs_remove_file(&i2c->dev.kobj, &max98927_QFactor_attr.attr);

	sysfs_remove_file(&i2c->dev.kobj, &max98927_info_attr.attr);

	return;
}


static int max989xx_calib_get(uint32_t *calib_value)
{
	struct file *pfile = NULL;
	mm_segment_t old_fs;
	int found = 0;
	loff_t pos = 0;

	*calib_value = 0;
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	pfile = filp_open(CALIBRATE_FILE, O_RDONLY, 0);
	if (!IS_ERR_OR_NULL(pfile)) {
		found = 1;
		vfs_read(pfile, (char *)calib_value, sizeof(uint32_t), &pos);
		pr_info("%s get calib_value %d  \n", __func__, *calib_value);
		filp_close(pfile, NULL);
	} else {
		pr_info("%s No found\n", __func__);
		found = 0;
	}

	set_fs(old_fs);

	return found;
}

static int max989xx_calib_get_r(uint32_t *calib_value)
{
	struct file *pfile = NULL;
	mm_segment_t old_fs;
	int found = 0;
	loff_t pos = 0;

	*calib_value = 0;
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	pfile = filp_open(CALIBRATE_FILE_R, O_RDONLY, 0);
	if (!IS_ERR_OR_NULL(pfile)) {
		found = 1;
		vfs_read(pfile, (char *)calib_value, sizeof(uint32_t), &pos);
		pr_info("%s get calib_value %d  \n", __func__, *calib_value);
		filp_close(pfile, NULL);
	} else {
		pr_info("%s No found\n", __func__);
		found = 0;
	}

	set_fs(old_fs);

	return found;
}


int max98927_wrapper_read(struct max98927_priv *max98927, bool speaker,
		unsigned int reg, unsigned int *val)
{
	int ret = -1;
	if (i2c_states & (1 << speaker)){
		ret = regmap_read(max98927->regmap[speaker], reg, val);
	}
	return ret;
}

void max98927_wrapper_write(struct max98927_priv *max98927,
		unsigned int reg, unsigned int val)
{
	int i;
	for (i = 0; i < MAX_CHANNEL_NUM; i++){
        if (i2c_states & (1 << i)){
			regmap_write(max98927->regmap[i], reg, val);
        }
	}
}

void max98927_wrap_update_bits(struct max98927_priv *max98927,
		unsigned int reg, unsigned int mask, unsigned int val)
{
	int i;
 	for (i = 0; i < MAX_CHANNEL_NUM; i++){
        if (i2c_states & (1 << i)){
			regmap_update_bits(max98927->regmap[i], reg, mask, val);
        }
	}
}

static int max98927_reg_get_w(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
  	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int reg = mc->reg;
	unsigned int shift = mc->shift;
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	unsigned int val;

	max98927_wrapper_read(max98927, 0, reg, &val);

	val = (val >> shift) & mask;

 	if (invert)
		ucontrol->value.integer.value[0] = max - val;
	else
		ucontrol->value.integer.value[0] = val;

	return 0;
}

static int max98927_reg_put_w(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
 	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int reg = mc->reg;
	unsigned int shift = mc->shift;
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;

	unsigned int val = (ucontrol->value.integer.value[0] & mask);
 	if (invert)
		val = max - val;
	mask = mask << shift;
	val = val << shift;

	max98927_wrap_update_bits(max98927, reg, mask, val);
	pr_info("%s: register 0x%02X, value 0x%02X\n",
			__func__, reg, val);
	return 0;
}
static int max98927_reg_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol, unsigned int reg,
		unsigned int mask, unsigned int shift)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	int data;

	max98927_wrapper_read(max98927, 0, reg, &data);
	ucontrol->value.integer.value[0] =
		(data & mask) >> shift;
	return 0;
}

static int max98927_reg_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol, unsigned int reg,
		unsigned int mask, unsigned int shift)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);

	unsigned int sel = ucontrol->value.integer.value[0];
	max98927_wrap_update_bits(max98927, reg, mask, sel << shift);
	pr_info("%s: register 0x%02X, value 0x%02X\n",
			__func__, reg, sel);
	return 0;
}

static int max98927_dai_set_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);

	pr_info("%s: fmt 0x%08X\n", __func__, fmt);
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
		case SND_SOC_DAIFMT_CBS_CFS:
			max98927_wrap_update_bits(max98927, MAX98927_PCM_Master_Mode,
					MAX98927_PCM_Master_Mode_PCM_MSTR_MODE_Mask,
					MAX98927_PCM_Master_Mode_PCM_MSTR_MODE_SLAVE);
			break;
		case SND_SOC_DAIFMT_CBM_CFM:
			max98927->master = true;
			max98927_wrap_update_bits(max98927, MAX98927_PCM_Master_Mode,
					MAX98927_PCM_Master_Mode_PCM_MSTR_MODE_Mask,
					MAX98927_PCM_Master_Mode_PCM_MSTR_MODE_MASTER);
			break;
		case SND_SOC_DAIFMT_CBS_CFM:
			max98927_wrap_update_bits(max98927, MAX98927_PCM_Master_Mode,
					MAX98927_PCM_Master_Mode_PCM_MSTR_MODE_Mask,
					MAX98927_PCM_Master_Mode_PCM_MSTR_MODE_HYBRID);
			break;
		default:
			pr_info("DAI clock mode unsupported");
			return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
			max98927_wrap_update_bits(max98927, MAX98927_PCM_Mode_Config,
					MAX98927_PCM_Mode_Config_PCM_BCLKEDGE,
					0);
			break;
		case SND_SOC_DAIFMT_IB_NF:
			max98927_wrap_update_bits(max98927, MAX98927_PCM_Mode_Config,
					MAX98927_PCM_Mode_Config_PCM_BCLKEDGE,
					MAX98927_PCM_Mode_Config_PCM_BCLKEDGE);
			break;
		default:
			pr_info("DAI invert mode unsupported");
			return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_I2S:
			max98927->iface |= SND_SOC_DAIFMT_I2S;
			max98927_wrap_update_bits(max98927, MAX98927_PCM_Mode_Config,
					MAX98927_PCM_Mode_Config_PCM_FORMAT_Mask,
					MAX98927_PCM_Mode_Config_PCM_FORMAT_I2S);
			break;
		case SND_SOC_DAIFMT_LEFT_J:
			max98927->iface |= SND_SOC_DAIFMT_LEFT_J;
			max98927_wrap_update_bits(max98927, MAX98927_PCM_Mode_Config,
					MAX98927_PCM_Mode_Config_PCM_FORMAT_Mask,
					MAX98927_PCM_Mode_Config_PCM_FORMAT_LEFT);
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

/* codec MCLK rate in master mode */
static const int rate_table[] = {
	5644800, 6000000, 6144000, 6500000,
	9600000, 11289600, 12000000, 12288000,
	13000000, 19200000,
};

static int max98927_set_clock(struct max98927_priv *max98927,
		struct snd_pcm_hw_params *params)
{
	/* BCLK/LRCLK ratio calculation */
	int blr_clk_ratio = 2 * max98927->ch_size;
	int reg = MAX98927_PCM_Clock_setup;
	int mask = MAX98927_PCM_Clock_setup_PCM_BSEL_Mask;
	int value;

	if (max98927->master) {
		int i;
		/* match rate to closest value */
		for (i = 0; i < ARRAY_SIZE(rate_table); i++) {
			if (rate_table[i] >= max98927->sysclk)
				break;
		}
		if (i == ARRAY_SIZE(rate_table)) {
			pr_err("%s couldn't get the MCLK to match codec\n", __func__);
			return -EINVAL;
		}
		max98927_wrap_update_bits(max98927, MAX98927_PCM_Master_Mode,
				MAX98927_PCM_Master_Mode_PCM_MCLK_RATE_Mask,
				i << MAX98927_PCM_Master_Mode_PCM_MCLK_RATE_SHIFT);
	}

	switch (blr_clk_ratio) {
		case 32:
			value = 2;
			break;
		case 48:
			value = 3;
			break;
		case 64:
			value = 4;
			break;
		default:
			return -EINVAL;
	}

	pr_info("%s: BLCK fix to %d\n", __func__, blr_clk_ratio);
	max98927_wrap_update_bits(max98927,
			reg, mask, value);
	return 0;
}

static int max98927_dai_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	int sampling_rate = 0;

	switch (snd_pcm_format_width(params_format(params))) {
		case 16:
			max98927_wrap_update_bits(max98927,
					MAX98927_PCM_Mode_Config,
					MAX98927_PCM_Mode_Config_PCM_CHANSZ_Mask,
					MAX98927_PCM_Mode_Config_PCM_CHANSZ_16);
			max98927->ch_size = 16;
			break;
		case 24:






		case 32:
			max98927_wrap_update_bits(max98927,
					MAX98927_PCM_Mode_Config,
					MAX98927_PCM_Mode_Config_PCM_CHANSZ_Mask,
					MAX98927_PCM_Mode_Config_PCM_CHANSZ_32);
			max98927->ch_size = 32;
			break;
		default:
			pr_err("%s: format unsupported %d",
					__func__, params_format(params));
			goto err;
	}
	pr_info("%s: format supported %d",
			__func__, max98927->ch_size);

	switch (params_rate(params)) {
		case 8000:
			sampling_rate |=
				MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_8000;
			break;
		case 11025:
			sampling_rate |=
				MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_11025;
			break;
		case 12000:
			sampling_rate |=
				MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_12000;
			break;
		case 16000:
			sampling_rate |=
				MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_16000;
			break;
		case 22050:
			sampling_rate |=
				MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_22050;
			break;
		case 24000:
			sampling_rate |=
				MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_24000;
			break;
		case 32000:
			sampling_rate |=
				MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_32000;
			break;
		case 44100:
			sampling_rate |=
				MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_44100;
			break;
		case 48000:
			sampling_rate |=
				MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_48000;
			break;
		default:
			pr_err("%s rate %d not supported\n", __func__, params_rate(params));
			goto err;
	}
	/* set DAI_SR to correct LRCLK frequency */
	max98927_wrap_update_bits(max98927, MAX98927_PCM_Sample_rate_setup_1,
			MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_Mask, sampling_rate);
	max98927_wrap_update_bits(max98927, MAX98927_PCM_Sample_rate_setup_2,
			MAX98927_PCM_Sample_rate_setup_2_SPK_SR_Mask, sampling_rate<<4);
	if (max98927->interleave_mode){
		max98927_wrap_update_bits(max98927, MAX98927_PCM_Sample_rate_setup_2,
				MAX98927_PCM_Sample_rate_setup_2_IVADC_SR_Mask, (sampling_rate-3));
	} else {
 		max98927_wrap_update_bits(max98927, MAX98927_PCM_Sample_rate_setup_2,
				MAX98927_PCM_Sample_rate_setup_2_IVADC_SR_Mask, sampling_rate);
	}

	return max98927_set_clock(max98927, params);
err:
	return -EINVAL;
}

#define MAX98927_RATES SNDRV_PCM_RATE_8000_48000

#define MAX98927_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
		SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static int max98927_dai_set_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	pr_info("%s: clk_id %d, freq %d, dir %d\n", __func__, clk_id, freq, dir);

	max98927->sysclk = freq;
	return 0;
}

static int max98927_stream_mute(struct snd_soc_dai *codec_dai, int mute, int stream)
{
    struct snd_soc_codec *codec = codec_dai->codec;
    struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
    uint32_t *payload = (uint32_t *)&gParam[PKG_HEADER];
	int rc = 0;
	uint32_t impedance = 0;

	pr_info("%s--- stream %d, mute %d \n", __func__, stream, mute);

    if (!max98927) {
        pr_err("%s ------ priv data null pointer\n", __func__);
        return 0;
    }

    if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
        if (mute) {
			if (max98927->path_status)
				afe_dsm_ramp_dn_cfg((uint8_t*)payload, 25);
			max98927_wrap_update_bits(max98927, MAX98927_Global_Enable, MAX98927_Global_Enable_EN, 0);
			max98927_wrap_update_bits(max98927, MAX98927_AMP_enables, MAX98927_AMP_enables, 0);
			max98927->spk_mode = 0;
			pr_info("%s ------ disable max98927 playback \n", __func__);
		} else {
#if 0

			if (max98927->path_status) {
				max98927_wrap_update_bits(max98927, MAX98927_Speaker_Gain,
						MAX98927_Speaker_Gain_SPK_PCM_GAIN_Mask, 5);
				max98927_wrap_update_bits(max98927, MAX98927_AMP_volume_control,
						MAX98927_AMP_volume_control_AMP_VOL_Mask, 56);
				max98927_wrap_update_bits(max98927, MAX98927_Boost_Control_0,
						MAX98927_Boost_Control_0_BST_VOUT_Mask, MAX_BOOST_VOLT);
			} else {
				max98927_wrap_update_bits(max98927, MAX98927_Speaker_Gain,
						MAX98927_Speaker_Gain_SPK_PCM_GAIN_Mask, 1);
				max98927_wrap_update_bits(max98927, MAX98927_AMP_volume_control,
						MAX98927_AMP_volume_control_AMP_VOL_Mask, 44);
				max98927_wrap_update_bits(max98927, MAX98927_Boost_Control_0,
						MAX98927_Boost_Control_0_BST_VOUT_Mask, 0x00);
			}
#endif
			if (max98927->mono_stereo == 0x3) {
				if (max98927->spk_mode & (0x1 << MAX98927L)) {
					regmap_update_bits(max98927->regmap[MAX98927L], MAX98927_AMP_enables,
							MAX98927_AMP_enables_SPK_EN, 1);
					regmap_update_bits(max98927->regmap[MAX98927L], MAX98927_Global_Enable,
							MAX98927_Global_Enable_EN, 1);
				} else if (max98927->spk_mode & (0x1 << MAX98927R)) {
					regmap_update_bits(max98927->regmap[MAX98927R], MAX98927_AMP_enables,
							MAX98927_AMP_enables_SPK_EN, 1);
					regmap_update_bits(max98927->regmap[MAX98927R], MAX98927_Global_Enable,
							MAX98927_Global_Enable_EN, 1);
				}
			} else {
				max98927_wrap_update_bits(max98927, MAX98927_AMP_enables, MAX98927_AMP_enables_SPK_EN, 1);
				max98927_wrap_update_bits(max98927, MAX98927_Global_Enable, MAX98927_Global_Enable_EN, 1);
			}

			pr_info("%s ------ enable max98927 playback \n", __func__);
		}
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (mute) {
			/* max98927_wrapper_write(max98927, MAX98927_Measurement_enables, 0x0); */
			max98927->path_status = 0;
			pr_info("%s ------ disable max98927 capture\n", __func__);
		} else {
			max98927_wrapper_write(max98927, MAX98927_Measurement_enables, 0x3);
			max98927->path_status |= i2c_states;
			if (max98927->mono_stereo == 0x0 || (max98927->mono_stereo & 0x1)) {
				if (!rdc_check_valid(max98927->ref_RDC[MAX98927L]) && max98927->ref_RDC[MAX98927L] != 0xCACACACA){
					rc = max989xx_calib_get(&impedance);
					if (rdc_check_valid(impedance) || impedance == 0xCACACACA) {
						max98927->ref_RDC[MAX98927L] = impedance;
						pr_info("%s: ref_RDC left =%d \n", __func__,  max98927->ref_RDC[MAX98927L]);
					}
				}
			}
			if (max98927->mono_stereo & 0x2) {
				if (!rdc_check_valid(max98927->ref_RDC[MAX98927R]) && max98927->ref_RDC[MAX98927R] != 0xCACACACA){
					impedance = 0;
					rc = max989xx_calib_get_r(&impedance);
					if (rdc_check_valid(impedance) || impedance == 0xCACACACA) {
						max98927->ref_RDC[MAX98927R] = impedance;
						pr_info("%s: ref_RDC right=%d \n", __func__, max98927->ref_RDC[MAX98927R]);
					}
				}
			}
			mutex_lock(&dsm_lock);
			*payload = max98927->ref_RDC[MAX98927L];
			*(payload+1) = max98927->ref_RDC[MAX98927R];
			*(payload+2) = max98927->adsp_mode;
			afe_dsm_set_calib((uint8_t *)payload);

			mutex_unlock(&dsm_lock);
			pr_info("%s ------ enable max98927 capture\n", __func__);
		}
	}

	return 0;
}

static const struct snd_soc_dai_ops max98927_dai_ops = {
	.set_sysclk = max98927_dai_set_sysclk,
	.set_fmt = max98927_dai_set_fmt,
	.hw_params = max98927_dai_hw_params,
	.mute_stream =  max98927_stream_mute,
};

static int max98927_feedforward_event(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	u32  ret = 0;

 	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct max98927_priv  *max98927 = snd_soc_codec_get_drvdata(codec);
    /* uint32_t* payload = (uint32_t *)&gParam[PKG_HEADER]; */

	if (!max98927){
		pr_err("%s------priv data null pointer\n", __func__);
		return ret;
	}
	pr_info("%s---feedforward event %d\n", __func__, event);
	switch(event){
		case SND_SOC_DAPM_POST_PMU:
			break;
		case SND_SOC_DAPM_POST_PMD:
			break;
		case SND_SOC_DAPM_PRE_PMU:
			break;
		case SND_SOC_DAPM_PRE_PMD:
			break;
		default:
			break;
	}
	return ret;
}
static int max98927_feedback_event(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	u32  ret = 0;

 	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct max98927_priv  *max98927 = snd_soc_codec_get_drvdata(codec);
	if (!max98927){
		pr_err("%s------priv data null pointer\n", __func__);
		return ret;
	}
	pr_info("%s---feedback event %d\n", __func__, event);
	switch(event){
		case SND_SOC_DAPM_POST_PMU:
			/* max98927_wrapper_write(max98927, MAX98927_Measurement_enables, 0x3); */
   		break;
		case SND_SOC_DAPM_POST_PMD:
			/* max98927_wrapper_write(max98927, MAX98927_Measurement_enables, 0x0); */
			break;
		default:
			break;
	}
	return ret;
}

static const struct snd_soc_dapm_widget max98927_dapm_widgets[] = {
	SND_SOC_DAPM_DAC_E("DACs", "HiFi Playback", SND_SOC_NOPM, 0, 0,
			max98927_feedforward_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
 	SND_SOC_DAPM_ADC_E("ADCs", "HiFi Capture", SND_SOC_NOPM, 0, 0,
			max98927_feedback_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_INPUT("MAX98927_IN"),
	SND_SOC_DAPM_OUTPUT("MAX98927_OUT"),
};

static DECLARE_TLV_DB_SCALE(max98927_spk_tlv, 300, 300, 0);
static DECLARE_TLV_DB_SCALE(max98927_digital_tlv, -1600, 25, 0);

static int max98927_spk_gain_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = max98927->spk_gain;
	pr_info("max98927_spk_gain_get: spk_gain setting returned %d\n",
			(int) ucontrol->value.integer.value[0]);

	return 0;
}

static int max98927_spk_gain_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];
	pr_info("max98927_spk_gain_put: %d\n",sel);

	if (sel < ((1 << MAX98927_Speaker_Gain_Width) - 1)) {
		/* max98927_wrap_update_bits(max98927, MAX98927_Speaker_Gain, */
				/* MAX98927_Speaker_Gain_SPK_PCM_GAIN_Mask, sel); */
		max98927->spk_gain = sel;
	}
	return 0;
}

static int max98927_digital_gain_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = max98927->digital_gain;
	pr_info("%s: spk_gain setting returned %d\n", __func__,
			(int) ucontrol->value.integer.value[0]);
	return 0;
}

static int max98927_digital_gain_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];
	pr_info("max98927_digital_gain_put: %d\n",sel);

	if (sel <= ((1 << MAX98927_AMP_VOL_WIDTH) - 1)) {
		/* max98927_wrap_update_bits(max98927, MAX98927_AMP_volume_control, */
				/* MAX98927_AMP_volume_control_AMP_VOL_Mask, sel); */
		max98927->digital_gain = sel;
	}
	return 0;
}

static int max98927_boost_voltage_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_get(kcontrol, ucontrol, MAX98927_Boost_Control_0,
			MAX98927_Boost_Control_0_BST_VOUT_Mask, 0);
}

static int max98927_boost_voltage_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_put(kcontrol, ucontrol, MAX98927_Boost_Control_0,
			MAX98927_Boost_Control_0_BST_VOUT_Mask, 0);
}

static int max98927_boost_input_limit_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_get(kcontrol, ucontrol, MAX98927_Boost_Control_1,
			MAX98927_Boost_Control_1_BST_ILIM_Mask, MAX98927_BST_ILIM_SHIFT);
}

static int max98927_boost_input_limit_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_put(kcontrol, ucontrol, MAX98927_Boost_Control_1,
			MAX98927_Boost_Control_1_BST_ILIM_Mask, MAX98927_BST_ILIM_SHIFT);
}

static int max98927_spk_src_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_get(kcontrol, ucontrol, MAX98927_Speaker_source_select,
			MAX98927_Speaker_source_select_SPK_SOURCE_Mask, 0);
}

static int max98927_spk_src_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_put(kcontrol, ucontrol, MAX98927_Speaker_source_select,
			MAX98927_Speaker_source_select_SPK_SOURCE_Mask, 0);
}

static int max98927_mono_out_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_get(kcontrol, ucontrol, MAX98927_PCM_to_speaker_monomix_A,
			MAX98927_PCM_to_speaker_monomix_A_DMONOMIX_CH0_SOURCE_Mask, 0);
}

static int max98927_mono_out_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_put(kcontrol, ucontrol, MAX98927_PCM_to_speaker_monomix_A,
			MAX98927_PCM_to_speaker_monomix_A_DMONOMIX_CH0_SOURCE_Mask, 0);
}

static int max98927_mono_out_get_l(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	int data = 0;
    if (i2c_states & MAX98927_CH0){
		regmap_read(max98927->regmap[MAX98927L], MAX98927_PCM_to_speaker_monomix_A, &data);
		ucontrol->value.integer.value[0] =
			(data & MAX98927_PCM_to_speaker_monomix_A_DMONOMIX_CH0_SOURCE_Mask);
		pr_info("%s: value:%d", __func__, data);
    }

	return 0;
}

static int max98927_mono_out_put_l(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];

    if (i2c_states & MAX98927_CH0){
		regmap_update_bits(max98927->regmap[MAX98927L], MAX98927_PCM_to_speaker_monomix_A,
				MAX98927_PCM_to_speaker_monomix_A_DMONOMIX_CH0_SOURCE_Mask, sel);
		regmap_update_bits(max98927->regmap[MAX98927L], MAX98927_PCM_Rx_Enables_A,
				0xf, sel+1);
		pr_info("%s: register 0x%02X, value 0x%02X\n",
				__func__, MAX98927_PCM_to_speaker_monomix_A, sel);
	}

	return 0;
}

static int max98927_mono_out_get_r(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	int data = 0;

    if (i2c_states & MAX98927_CH1){
		regmap_read(max98927->regmap[MAX98927R], MAX98927_PCM_to_speaker_monomix_A, &data);
		ucontrol->value.integer.value[0] =
			(data & MAX98927_PCM_to_speaker_monomix_A_DMONOMIX_CH0_SOURCE_Mask);
	}
	pr_info("%s: value:%d", __func__, data);
	return 0;
}

static int max98927_mono_out_put_r(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];
    if (i2c_states & MAX98927_CH1){
		regmap_update_bits(max98927->regmap[MAX98927R], MAX98927_PCM_to_speaker_monomix_A,
				MAX98927_PCM_to_speaker_monomix_A_DMONOMIX_CH0_SOURCE_Mask, sel);
		regmap_update_bits(max98927->regmap[MAX98927R], MAX98927_PCM_Rx_Enables_A, 0xf, sel+1);
		pr_info("%s: register 0x%02X, value 0x%02X\n",
				__func__, MAX98927_PCM_to_speaker_monomix_A, sel);
	} else {
		pr_info("%s: mono mode not support!!\n", __func__);
	}
	return 0;
}

static int max98927_feedback_en_get_l(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	int data = 0;

	if (i2c_states & MAX98927_CH0){
		regmap_read(max98927->regmap[MAX98927L], MAX98927_Measurement_enables, &data);
		ucontrol->value.integer.value[0] = data;
		pr_info("%s: value:%d", __func__, data);
	}

	return 0;
}

static int max98927_feedback_en_put_l(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];

	if (i2c_states & MAX98927_CH0){
		regmap_write(max98927->regmap[MAX98927L], MAX98927_Measurement_enables, sel);
		pr_info("%s: register 0x%02X, value 0x%02X\n",
			__func__, MAX98927_Measurement_enables, sel);
	}
	return 0;
}

static int max98927_feedback_en_get_r(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	int data = 0;

	if (i2c_states & MAX98927_CH1){
		regmap_read(max98927->regmap[MAX98927R], MAX98927_Measurement_enables, &data);
		ucontrol->value.integer.value[0] = data;
	}
	pr_info("%s: value:%d", __func__, data);
	return 0;
}

static int max98927_feedback_en_put_r(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];
	if (i2c_states & MAX98927_CH1){
		regmap_write(max98927->regmap[MAX98927R], MAX98927_Measurement_enables, sel);
		pr_info("%s: register 0x%02X, value 0x%02X\n",
				__func__, MAX98927_Measurement_enables, sel);
	} else {
		pr_info("%s: mono mode not support!!\n", __func__);
	}
	return 0;
}

static int max98927_adsp_mode_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.integer.value[0] = max98927->adsp_mode;
	pr_info("%s: value:%d", __func__, (int)ucontrol->value.integer.value[0]);
	return 0;
}

static int max98927_adsp_mode_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];
	uint32_t *payload = (uint32_t *)&gParam[PKG_HEADER];

    max98927->adsp_mode = sel;

	mutex_lock(&dsm_lock);
	*payload = max98927->ref_RDC[MAX98927L];
	*(payload+1) = max98927->ref_RDC[MAX98927R];
	*(payload+2) = max98927->adsp_mode;
	afe_dsm_set_calib((uint8_t *)payload);

	mutex_unlock(&dsm_lock);

    pr_info("%s: value 0x%02X\n", __func__, sel);
	return 0;
}

static int max98927_left_channel_enable_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	int data_global = 0;
	int data_amp = 0;


	if (i2c_states & MAX98927_CH0){
		regmap_read(max98927->regmap[MAX98927L], MAX98927_Global_Enable, &data_global);
		regmap_read(max98927->regmap[MAX98927L], MAX98927_AMP_enables, &data_amp);
		ucontrol->value.integer.value[0] = (data_global & MAX98927_Global_Enable_EN)
			& (data_amp & MAX98927_AMP_enables_SPK_EN);
	}

	pr_info("%s: value:%d", __func__, (int)ucontrol->value.integer.value[0]);
	return 0;
}

static int max98927_left_channel_enable_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];
	max98927->spk_mode &= ~0x1;
	max98927->spk_mode |= sel;

	pr_info("%s: register 0x%02X, value 0x%02X\n",
			__func__, MAX98927_Global_Enable, sel);
	return 0;
}

static int max98927_right_channel_enable_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	int data_global = 0;
	int data_amp = 0;

	if (i2c_states & MAX98927_CH1){
		regmap_read(max98927->regmap[MAX98927R], MAX98927_Global_Enable, &data_global);
		regmap_read(max98927->regmap[MAX98927R], MAX98927_AMP_enables, &data_amp);
		ucontrol->value.integer.value[0] = (data_global & MAX98927_Global_Enable_EN)
			& (data_amp & MAX98927_AMP_enables_SPK_EN);
	}

	pr_info("%s: value:%d", __func__, (int)ucontrol->value.integer.value[0]);
	return 0;
}

static int max98927_right_channel_enable_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];
	max98927->spk_mode &= ~0x2;
	max98927->spk_mode |= sel<<0x1;
	pr_info("%s: register 0x%02X, value 0x%02X\n",
				__func__, MAX98927_Global_Enable, sel);
	return 0;
}

static const char * const max98927_boost_voltage_text[] = {
	"6.5V", "6.625V", "6.75V", "6.875V", "7V", "7.125V", "7.25V", "7.375V",
	"7.5V", "7.625V", "7.75V", "7.875V", "8V", "8.125V", "8.25V", "8.375V",
	"8.5V", "8.625V", "8.75V", "8.875V", "9V", "9.125V", "9.25V", "9.375V",
	"9.5V", "9.625V", "9.75V", "9.875V", "10V"
};

static const char * const max98927_boost_current_limit_text[] = {
    "1.0A", "1.1A", "1.2A", "1.3A", "1.4A", "1.5A", "1.6A", "1.7A", "1.8A", "1.9A",
    "2.0A", "2.1A", "2.2A", "2.3A", "2.4A", "2.5A", "2.6A", "2.7A", "2.8A", "2.9A",
    "3.0A", "3.1A", "3.2A", "3.3A", "3.4A", "3.5A", "3.6A", "3.7A", "3.8A", "3.9A",
    "4.0A", "4.1A"
};

static const char * const max98927_speaker_source_text[] = {
	"i2s", "reserved", "tone", "pdm"
};

static const char * const max98927_monomix_output_text[] = {
	"ch_0", "ch_1", "ch_1_2_div"
};
static const char * const max98927_feedback_switch_text[] = {
	"OFF", "V_EN", "I_EN", "ON"
};

static const char * const max98927_adsp_working_mode_text[] = {
	"full", "feedback", "playback", "bypass"
};

static const struct soc_enum max98927_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(max98927_monomix_output_text), max98927_monomix_output_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(max98927_speaker_source_text), max98927_speaker_source_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(max98927_boost_voltage_text), max98927_boost_voltage_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(max98927_feedback_switch_text), max98927_feedback_switch_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(max98927_boost_current_limit_text), max98927_boost_current_limit_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(max98927_adsp_working_mode_text), max98927_adsp_working_mode_text),
};

static const struct snd_kcontrol_new max98927_snd_controls[] = {
	SOC_SINGLE_EXT_TLV("Speaker Volume", MAX98927_Speaker_Gain,
			0, (1<<MAX98927_Speaker_Gain_Width)-1, 0,
			max98927_spk_gain_get, max98927_spk_gain_put, max98927_spk_tlv),

	SOC_SINGLE_EXT_TLV("Digital Gain", MAX98927_AMP_volume_control,
			0, (1<<MAX98927_AMP_VOL_WIDTH)-1, 0,
			max98927_digital_gain_get, max98927_digital_gain_put, max98927_digital_tlv),

    SOC_SINGLE_EXT("BDE Enable", MAX98927_Brownout_enables,
			0, 1, 0, max98927_reg_get_w, max98927_reg_put_w),

	SOC_SINGLE_EXT("Amp DSP Enable", MAX98927_Brownout_enables,
			MAX98927_BDE_DSP_SHIFT, 1, 0, max98927_reg_get_w, max98927_reg_put_w),

	SOC_SINGLE_EXT("BDE AMP Enable", MAX98927_Brownout_enables,
			1, 1, 0, max98927_reg_get_w, max98927_reg_put_w),

	SOC_SINGLE_EXT("Ramp Switch", MAX98927_AMP_DSP_Config,
			MAX98927_SPK_RMP_EN_SHIFT, 1, 1, max98927_reg_get_w, max98927_reg_put_w),

	SOC_SINGLE_EXT("DRE EN", MAX98927_DRE_Control,
			0, 1, 0, max98927_reg_get_w, max98927_reg_put_w),

	SOC_SINGLE_EXT("Amp Volume Location", MAX98927_AMP_volume_control,
			MAX98927_AMP_VOL_LOCATION_SHIFT, 1, 0, max98927_reg_get_w, max98927_reg_put_w),

    SOC_SINGLE_EXT("Level1 Threshold", MAX98927_Brownout__level_1_threshold,
            0, 255, 0, max98927_reg_get_w, max98927_reg_put_w),

    SOC_SINGLE_EXT("Level2 Threshold", MAX98927_Brownout__level_2_threshold,
            0, 255, 0, max98927_reg_get_w, max98927_reg_put_w),

    SOC_SINGLE_EXT("Level3 Threshold", MAX98927_Brownout__level_3_threshold,
            0, 255, 0, max98927_reg_get_w, max98927_reg_put_w),

    SOC_SINGLE_EXT("Level4 Threshold", MAX98927_Brownout__level_4_threshold,
            0, 255, 0, max98927_reg_get_w, max98927_reg_put_w),

    SOC_SINGLE_EXT("Level1 Current Limit", MAX98927_Brownout__level_1_current_limit,
            0, 63, 0, max98927_reg_get_w, max98927_reg_put_w),

    SOC_SINGLE_EXT("Level2 Current Limit", MAX98927_Brownout__level_2_current_limit,
            0, 63, 0, max98927_reg_get_w, max98927_reg_put_w),

    SOC_SINGLE_EXT("Level3 Current Limit", MAX98927_Brownout__level_3_current_limit,
            0, 63, 0, max98927_reg_get_w, max98927_reg_put_w),

    SOC_SINGLE_EXT("Level4 Current Limit", MAX98927_Brownout__level_4_current_limit,
            0, 63, 0, max98927_reg_get_w, max98927_reg_put_w),

	SOC_ENUM_EXT("Boost Output Voltage", max98927_enum[2],
			max98927_boost_voltage_get, max98927_boost_voltage_put),

	SOC_ENUM_EXT("Boost Current Limit", max98927_enum[4],
			max98927_boost_input_limit_get, max98927_boost_input_limit_put),

	SOC_ENUM_EXT("Speaker Source", max98927_enum[1],
			max98927_spk_src_get, max98927_spk_src_put),

	SOC_ENUM_EXT("Monomix Output", max98927_enum[0],
			max98927_mono_out_get, max98927_mono_out_put),


	SOC_ENUM_EXT("Left Monomix Output", max98927_enum[0],
			max98927_mono_out_get_l, max98927_mono_out_put_l),


	SOC_ENUM_EXT("Right Monomix Output", max98927_enum[0],
			max98927_mono_out_get_r, max98927_mono_out_put_r),
	SOC_ENUM_EXT("Left Feedback Enable", max98927_enum[3],
			max98927_feedback_en_get_l, max98927_feedback_en_put_l),


	SOC_ENUM_EXT("Right Feedback Enable", max98927_enum[3],
			max98927_feedback_en_get_r, max98927_feedback_en_put_r),

 	SOC_ENUM_EXT("Adsp Working Mode", max98927_enum[5],
			max98927_adsp_mode_get, max98927_adsp_mode_put),


	SOC_SINGLE_EXT("Left Channel Enable", MAX98927_Global_Enable,
			0, 1, 0, max98927_left_channel_enable_get, max98927_left_channel_enable_set),

	SOC_SINGLE_EXT("Right Channel Enable", MAX98927_Global_Enable,
			0, 1, 0, max98927_right_channel_enable_get, max98927_right_channel_enable_set),
};

static const struct snd_soc_dapm_route max98927_audio_map[] = {
	{"MAX98927_OUT", NULL, "DACs"},
	{"ADCs", NULL, "MAX98927_IN"},
};

static struct snd_soc_dai_driver max98927_dai[] = {
	{
		.name = "max98927-aif1",
		.playback = {
			.stream_name = "HiFi Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MAX98927_RATES,
			.formats = MAX98927_FORMATS,
		},
		.capture = {
			.stream_name = "HiFi Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MAX98927_RATES,
			.formats = MAX98927_FORMATS,
		},
		.ops = &max98927_dai_ops,
	}
};

static int max98927_probe(struct snd_soc_codec *codec)
{
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	struct snd_soc_dapm_context *dapm = &codec->dapm;


	pr_info("%s: enter\n", __func__);

	max98927->codec = codec;
	snd_soc_dapm_ignore_suspend(dapm, "MAX98927_OUT");
	snd_soc_dapm_ignore_suspend(dapm, "MAX98927_IN");
    snd_soc_dapm_ignore_suspend(dapm, "HiFi Playback");
    snd_soc_dapm_ignore_suspend(dapm, "HiFi Capture");


	snd_soc_dapm_sync(dapm);
	g_max98927 = max98927;

	return 0;
}

static const struct snd_soc_codec_driver soc_codec_dev_max98927 = {
	.probe			  = max98927_probe,
	.component_driver	  = {
		.dapm_routes	  = max98927_audio_map,
		.num_dapm_routes  = ARRAY_SIZE(max98927_audio_map),
		.dapm_widgets     = max98927_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(max98927_dapm_widgets),
		.controls         = max98927_snd_controls,
		.num_controls     = ARRAY_SIZE(max98927_snd_controls),
	},
};

static const struct regmap_config max98927_regmap = {
	.reg_bits		  = 16,
	.val_bits		  = 8,
	.max_register	  = MAX98927_REV_ID,
	.readable_reg	  = max98927_readable_register,
	.volatile_reg	  = max98927_volatile_register,
	.cache_type		  = REGCACHE_RBTREE,
};

int max98927_get_i2c_states(void)
{
	return i2c_states;
}
EXPORT_SYMBOL(max98927_get_i2c_states);

static int max98927_reset(struct i2c_client *i2c, struct max98927_priv *max98927)
{
    int ret = 0;
 	max98927->reset_gpio_l = of_get_named_gpio(i2c->dev.of_node, "maxim,98927-reset-gpio", 0);
	pr_info("max98927_reset:%d------\n", max98927->reset_gpio_l);

    if (max98927->reset_gpio_l > 0){
		ret = gpio_request(max98927->reset_gpio_l, "max_98927_reset");
		if (ret) {
			pr_err("max98927_i2c_probe : failed to request rest gpio %d error:%d\n",
					max98927->reset_gpio_l, ret);
			gpio_free(max98927->reset_gpio_l);
			return ret;
		}
		gpio_direction_output(max98927->reset_gpio_l, 0);
 		msleep(10);
		gpio_direction_output(max98927->reset_gpio_l, 1);
		msleep(5);
    }
	return ret;
}

static bool check_max98927_presence(struct regmap *regmap)
{
	int rc = 0, reg = 0, i;

	rc = regmap_read(regmap, MAX98927_REV_ID, &reg);
	for (i = 0; rc && i < ARRAY_SIZE(delay_array_msec); i++) {
		pr_err("Failed reading version=%u - retry(%d)\n", reg, i);
		/* retry after delay of increasing order */
		msleep(delay_array_msec[i]);
		rc = regmap_read(regmap, MAX98927_REV_ID, &reg);
	}
	if (rc) {
		pr_err("Failed reading version=%u rc=%d\n", reg, rc);
		return false;
	}else{
		pr_info("max98927 device version 0x%02X\n", reg);
		return true;
	}
}
static int max98927_i2c_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	static struct max98927_priv *max98927;
	int value, i, ret = 0;
	unsigned int presence = 0;


    pr_err("****** %s id.name =%s id.driver_data= %d \n",__func__,id->name,(int)(id->driver_data));






	if (!max98927) {
		max98927 = devm_kzalloc(&i2c->dev,
				sizeof(*max98927), GFP_KERNEL);
		if (!max98927) {
			pr_info("------%s devm_kzalloc error!!\n", __func__);
			return -ENOMEM;
		}
	}
 	if (!of_property_read_u32(i2c->dev.of_node, "mono_stereo_mode", &value)) {
		if (value > 3) {
			pr_err("only support max to 2 channel!\n");
			value  = 0;
		}
		max98927->mono_stereo = value;
	}

	if (!of_property_read_u32(i2c->dev.of_node, "interleave_mode", &value)) {
		if (value > 1) {
			pr_info("interleave number is wrong:\n");
		}
		max98927->interleave_mode = value;
	}
	max98927->spk_gain = 5;
	max98927->digital_gain = 56;

    max98927->adsp_mode = 0;
#if 0
	if (!max98927->i2c_pull) {
		max98927->i2c_pull = devm_regulator_get(&i2c->dev, "i2c-pull");
		if (IS_ERR(max98927->i2c_pull)) {
			pr_err("regulator i2c_pull get failed\n ");
			devm_kfree(&i2c->dev, max98927);
			return PTR_ERR(max98927->i2c_pull);
		}

		ret = regulator_enable(max98927->i2c_pull);
		if (ret) {
			pr_err("regulator_enable i2c_pull failed! \n");
			devm_kfree(&i2c->dev, max98927);
			return ret;
		}
	}
    max98927->max989xx_vdd = regulator_get(&i2c->dev, "max989xx_vdd");
    if (IS_ERR(max98927->max989xx_vdd)) {
        pr_err("regulator max989xx_vdd get failed\n ");
        devm_kfree(&i2c->dev, max98927);
        return PTR_ERR(max98927->max989xx_vdd);
    } else {
        if (regulator_count_voltages(max98927->max989xx_vdd) > 0) {
            ret = regulator_set_voltage(max98927->max989xx_vdd, 1800000, 1800000);
            if (ret) {
                pr_err("%s Regulator set vdd failed ret=%d\n", __func__, ret);
                return ret;
            }

            ret = regulator_set_load(max98927->max989xx_vdd, 200000);
            if (ret) {
                pr_err("%s failed to set load, ret=%d\n", __func__, ret);
                return ret;
            }
        }
    }

    ret = regulator_enable(max98927->max989xx_vdd);
    if (ret) {
        pr_err("regulator_enable max989xx_vdd failed! \n");
        devm_kfree(&i2c->dev, max98927);
        return ret;
    }
#endif

	ret = max98927_reset(i2c, max98927);

	i2c_set_clientdata(i2c, max98927);

	max98927->regmap[id->driver_data] =
		devm_regmap_init_i2c(i2c, &max98927_regmap);
	if (IS_ERR(max98927->regmap[id->driver_data])){
  		ret = PTR_ERR(max98927->regmap[id->driver_data]);
		dev_err(&i2c->dev,
				"Failed to allocate chennel %lu regmap : %d\n", id->driver_data,  ret);
	}else{
		if (check_max98927_presence(max98927->regmap[id->driver_data])){
			presence = (1 << id->driver_data);
			if (max98927->mono_stereo == 0){
				i2c_states |= presence;
				for (i = 0; i < sizeof(reg_channel_map[0])/sizeof(reg_channel_map[0][0]); i++)
					regmap_write(max98927->regmap[id->driver_data], reg_channel_map[0][i][0], reg_channel_map[0][i][1]);
			}else if (max98927->mono_stereo & presence){
				i2c_states |= presence;
				for (i = 0; i < sizeof(reg_channel_map[id->driver_data+1])/sizeof(reg_channel_map[id->driver_data+1][0]); i++)
					regmap_write(max98927->regmap[id->driver_data],
							reg_channel_map[id->driver_data+1][i][0], reg_channel_map[id->driver_data+1][i][1]);
			}
    		for (i = 0; i < sizeof(reg_common_map)/sizeof(reg_common_map[0]); i++)
				regmap_write(max98927->regmap[id->driver_data], reg_common_map[i][0], reg_common_map[i][1]);

            if (max98927->interleave_mode)
				regmap_write(max98927->regmap[id->driver_data], MAX98927_PCM_Tx_Channel_Sources_B, 0x20);

		}
	}
	if (presence){
		if (max98927->dev == NULL){
			dev_set_name(&i2c->dev, "%s", "max98927");
			ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_max98927,
					max98927_dai, ARRAY_SIZE(max98927_dai));
            pr_err("****** %s snd_soc_register_codec id =%d\n",__func__,(int)(id->driver_data));
			if (ret < 0) {
				pr_err("max98927 Failed to register codec: %d\n", ret);
				i2c_states = 0;
				return ret;
			}
			max98927->dev = &i2c->dev;
			pr_info("max98927 register codec ok.\n");
#ifdef USE_DSM_MISC_DEV
			ret = misc_register(&dsm_ctrl_miscdev);
			if (ret != 0)
				pr_err("max98927 misc_register error:%d\n", ret);
#endif

            #ifdef CONFIG_DEBUG_FS
            max989xx_debug_init(max98927, i2c);
            #endif
			max989xx_sys_init(i2c);
		}
	}else
		pr_err("max98927 detection failed at %s - %x. \n", i2c->name, i2c->addr);

	return ret;
}

static int max98927_i2c_remove(struct i2c_client *client)
{
	struct max98927_priv *max98927 = i2c_get_clientdata(client);
	if (max98927) {
		if (max98927->dev == &client->dev) {
			snd_soc_unregister_codec(&client->dev);
			i2c_set_clientdata(client, NULL);
#ifdef CONFIG_DEBUG_FS
			max989xx_debug_remove(max98927);
#endif
			max989xx_sys_remove(client);
			kfree(max98927);
#ifdef USE_DSM_MISC_DEV
			misc_deregister(&dsm_ctrl_miscdev);
#endif
		}
	}

	return 0;
}

static const struct i2c_device_id max98927_i2c_id[] = {
	{ "max98927L", MAX98927L },
	{ "max98927R", MAX98927R },
	{ },
};

MODULE_DEVICE_TABLE(i2c, max98927_i2c_id);

static const struct of_device_id max98927_of_match[] = {
	{ .compatible = "maxim,max98927L", },
	{ .compatible = "maxim,max98927R", },
	{ }
};
MODULE_DEVICE_TABLE(of, max98927_of_match);

static struct i2c_driver max98927_i2c_driver = {
	.driver = {
		.name = "max98927",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(max98927_of_match),
		.pm = NULL,
	},
	.probe	= max98927_i2c_probe,
	.remove = max98927_i2c_remove,
	.id_table = max98927_i2c_id,
};

module_i2c_driver(max98927_i2c_driver)

MODULE_DESCRIPTION("ALSA SoC MAX98927 driver");
MODULE_AUTHOR("Maxim Integrated Inc.");
MODULE_LICENSE("GPL");
