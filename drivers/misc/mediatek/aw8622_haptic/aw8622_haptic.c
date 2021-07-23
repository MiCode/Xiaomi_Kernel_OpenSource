/*
 *  PWM haptic driver
 *
 *  Copyright (C) 2017 Collabora Ltd.
 *
 *
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 */

#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/of_gpio.h>
#include <linux/dma-mapping.h>
#include <linux/time64.h>

#include <mt-plat/mtk_pwm_hal_pub.h>
#include <mt-plat/mtk_pwm.h>
#include <mach/mtk_pwm_hal.h>
#include <linux/workqueue.h>
#include "aw8622_haptic.h"

#define AW8622_WAVEFORM_NAME_MAX		(64)

#define AW_GPIO_MODE_LED_DEFAULT		(0)
#define HAPTIC_GPIO_AW8622_DEFAULT		(0)
#define HAPTIC_GPIO_AW8622_SET			(1)
#define HAPTIC_PWM_MEMORY_MODE_CLOCK	(26000000) // 26 Mhz
#define HAPTIC_PWM_OLD_MODE_CLOCK 		(26000000) // 812500 hz


#define LOW_F0_FREQ				(203)
#define MID_F0_FREQ				(208)
#define HIGH_F0_FREQ			(213)


#define HAPTIC_PMW_SHOCKING_OLD_MODE_CLOCK		(812500) // 812500 hz CLK_DIV32


#define WAVE_SAMPLE_FREQ_26K				(26000) //26K
#define WAVE_SAMPLE_FREQ_13K				(13000) //13K

char *aw8622_pwm_gpio_cfg[] = {"haptic_gpio_aw8622_default", "haptic_gpio_aw8622_set"};
static u64 aw8622_dma_mask = DMA_BIT_MASK(32);

struct pwm_spec_config aw8622_pwm_memory_mode_config = {
       .pwm_no = 0,  /* pwm number */
       .mode = PWM_MODE_MEMORY,
       .clk_div = CLK_DIV1, /* default */
       .clk_src = PWM_CLK_NEW_MODE_BLOCK, /* default */
       .pmic_pad = 0,
       .PWM_MODE_MEMORY_REGS.IDLE_VALUE = IDLE_FALSE,
       .PWM_MODE_MEMORY_REGS.GUARD_VALUE = GUARD_FALSE,
       .PWM_MODE_MEMORY_REGS.STOP_BITPOS_VALUE = 31,
       /* 1 microseconds, assume clock source is 26M */
       .PWM_MODE_MEMORY_REGS.HDURATION = 8,
       .PWM_MODE_MEMORY_REGS.LDURATION = 8,
       .PWM_MODE_MEMORY_REGS.GDURATION = 0,
       .PWM_MODE_MEMORY_REGS.WAVE_NUM = 1,		/* max = 0xffff */
};

/* 	base clock 812500hz  
	24K : DATA_WIDTH = 34 DATA_WIDTH = 17, 
	208Hz DATA_WIDTH = 3906 DATA_WIDTH = 1953
*/
struct pwm_spec_config aw8622_pwm_old_mode_config = {
	.pwm_no = 0,
	.mode = PWM_MODE_OLD,
	.clk_div = CLK_DIV1,
	.clk_src = PWM_CLK_OLD_MODE_BLOCK,  // 26Mhz
	.pmic_pad = 0,

	.PWM_MODE_OLD_REGS.IDLE_VALUE = IDLE_FALSE,
	.PWM_MODE_OLD_REGS.WAVE_NUM = 0,
	.PWM_MODE_OLD_REGS.DATA_WIDTH = 1000,
	.PWM_MODE_OLD_REGS.GDURATION = 0,
	.PWM_MODE_OLD_REGS.THRESH = 500,
	
	.PWM_MODE_FIFO_REGS.IDLE_VALUE = 0,
	.PWM_MODE_FIFO_REGS.GUARD_VALUE = 0,
	.PWM_MODE_FIFO_REGS.GDURATION = 0,
	.PWM_MODE_FIFO_REGS.WAVE_NUM = 0,
};


#define NUMS_WAVEFORM_USED					(10)

#define LONG_SHOCK_EFFECT_IDX				(0)
#define LOW_SHORT_SHOCK_EFFECT_IDX			(1)
#define MID_SHORT_SHOCK_EFFECT_IDX			(2)
#define HIGH_SHORT_SHOCK_EFFECT_IDX			(3)
#define SHORT_SHOCK_NUMS					(3)

#define LOW_F0_TEST_EFFECT_IDX				(7)
#define MID_F0_TEST_EFFECT_IDX				(8)
#define HIGH_F0_TEST_EFFECT_IDX				(9)


#define  LOW_F0_LOAD_WAVEFORM_OFFSET		(0)
#define  MID_F0_LOAD_WAVEFORM_OFFSET		(1)
#define  HIGH_F0_LOAD_WAVEFORM_OFFSET		(2)


/* Customer's f0 standard is 208hz, with a deviation of +-7hz min f0 = 201 max f0 = 215 */
static char aw8622_waveform_file_name[][AW8622_WAVEFORM_NAME_MAX] = {
#if 0
/* Actual waveform file used */
	{"LONG_cfg.bin"},
	/* Called when the chip has been enabled */
	{"Q_cfg.bin"},
	{"M_cfg.bin"},
	{"Z_cfg.bin"},
	
	/* Called when the chip is disable */
	{"need_setup_Q_cfg.bin"},
	{"need_setup_M_cfg.bin"},
	{"need_setup_Z_cfg.bin"},

/* f0 calibrate wave file */
	{"low_f0_cali_cfg.bin"}, /* 204 hz */
	{"mid_f0_cali_cfg.bin"}, /* 208 hz */
	{"high_f0_cali_cfg.bin"}, /* 212 hz */
#endif
#if 1
/* 204 hz */
/* low f0  waveform file used */
	{"aw8622_low_f0_LONG_cfg.bin"},
	{"aw8622_low_f0_Q_cfg.bin"},
	{"aw8622_low_f0_M_cfg.bin"},
	{"aw8622_low_f0_Z_cfg.bin"},
	/* Called when the chip is disable */
	{"aw8622_low_f0_need_setup_Q_cfg.bin"},
	{"aw8622_low_f0_need_setup_M_cfg.bin"},
	{"aw8622_low_f0_need_setup_Z_cfg.bin"},
/* f0 calibrate wave file */
	{"aw8622_low_f0_cali_cfg.bin"}, /* 204 hz */
	{"aw8622_mid_f0_cali_cfg.bin"}, /* 208 hz */
	{"aw8622_high_f0_cali_cfg.bin"}, /* 212 hz */

/* 208 hz */
/* mid f0  waveform file used */
	{"aw8622_mid_f0_LONG_cfg.bin"},
	{"aw8622_mid_f0_Q_cfg.bin"},
	{"aw8622_mid_f0_M_cfg.bin"},
	{"aw8622_mid_f0_Z_cfg.bin"},
	/* Called when the chip is disable */
	{"aw8622_mid_f0_need_setup_Q_cfg.bin"},
	{"aw8622_mid_f0_need_setup_M_cfg.bin"},
	{"aw8622_mid_f0_need_setup_Z_cfg.bin"},
/* f0 calibrate wave file */
	{"aw8622_low_f0_cali_cfg.bin"}, /* 204 hz */
	{"aw8622_mid_f0_cali_cfg.bin"}, /* 208 hz */
	{"aw8622_high_f0_cali_cfg.bin"}, /* 212 hz */

/* 212hz */
/* high f0  waveform file used */
	{"aw8622_high_f0_LONG_cfg.bin"},
	{"aw8622_high_f0_Q_cfg.bin"},
	{"aw8622_high_f0_M_cfg.bin"},
	{"aw8622_high_f0_Z_cfg.bin"},
	/* Called when the chip is disable */
	{"aw8622_high_f0_need_setup_Q_cfg.bin"},
	{"aw8622_high_f0_need_setup_M_cfg.bin"},
	{"aw8622_high_f0_need_setup_Z_cfg.bin"},
/* f0 calibrate wave file */
	{"aw8622_low_f0_cali_cfg.bin"}, /* 204 hz */
	{"aw8622_mid_f0_cali_cfg.bin"}, /* 208 hz */
	{"aw8622_high_f0_cali_cfg.bin"}, /* 212 hz */
#endif
};

/* wave file data parse and save */
static void aw8622_wavefrom_data_load(const struct firmware *cont, void *context)
{
	struct aw8622_haptic *haptic = context;
	struct waveform_data_info *p_waveform_data = NULL;
	int data_offset = 0;
	unsigned int total_time = 0;
	pr_info("%s enter\n", __func__);

	if (!cont) {
		pr_err("%s: failed to read %s\n", __func__, aw8622_waveform_file_name[haptic->cur_load_idx + haptic->load_idx_offset]);
		release_firmware(cont);
		return;
	}

	p_waveform_data = &haptic->p_waveform_data[haptic->cur_load_idx];
	if (p_waveform_data == NULL) {
		pr_err("%s: p_waveform_data cat't used \n", __func__);
	}
	pr_info("%s: loaded %s - size: %zu\n", __func__,
		aw8622_waveform_file_name[haptic->cur_load_idx + haptic->load_idx_offset], cont ? cont->size : 0);

	if (p_waveform_data->is_loaded) { /* Free up old space */
		vfree(p_waveform_data->data);
		p_waveform_data->is_loaded = false;
	}
	p_waveform_data->data = vmalloc(cont->size);
	if (!p_waveform_data->data) {
		release_firmware(cont);
		pr_err("%s: Error allocating memory\n", __func__);
		return;
	}
	
	p_waveform_data->sample_freq = (cont->data[0]<<24) | (cont->data[1]<<16) | (cont->data[2]<<8) | (cont->data[3]<<0);
	p_waveform_data->sample_nums = (cont->data[4]<<24) | (cont->data[5]<<16) | (cont->data[6]<<8) | (cont->data[7]<<0);
	p_waveform_data->us_time_len = (cont->data[8]<<24) | (cont->data[9]<<16) | (cont->data[10]<<8) | (cont->data[11]<<0);
	data_offset = WAVEFORM_DATA_OFFSET;

	p_waveform_data->len = cont->size - data_offset;
	
	pr_info("%s: %s  file size = %ld\n", __func__, aw8622_waveform_file_name[haptic->cur_load_idx + haptic->load_idx_offset], cont->size);
	
	memcpy(p_waveform_data->data, cont->data + data_offset, p_waveform_data->len);
	if (p_waveform_data->us_time_len == 0) {
		haptic->h_l_period = HAPTIC_PWM_MEMORY_MODE_CLOCK/BIT_NUMS_PER_SAMPLED_VALE/p_waveform_data->sample_freq;
		total_time = (p_waveform_data->len) * BIT_NUMS_PER_BYTE * haptic->h_l_period;
		total_time = total_time * USEC_PER_SEC / HAPTIC_PWM_MEMORY_MODE_CLOCK;
		p_waveform_data->us_time_len = total_time;
	}
	p_waveform_data->is_loaded = true;
	release_firmware(cont);

	pr_info("%s idx = %d, sample_freq = %u, sample_nums = %u, len = %u time len = %u\n", __func__, 
			haptic->cur_load_idx, p_waveform_data->sample_freq, p_waveform_data->sample_nums, p_waveform_data->len, p_waveform_data->us_time_len);
	pr_info("%s load %s success\n", __func__, aw8622_waveform_file_name[haptic->cur_load_idx + haptic->load_idx_offset]);
}


static int aw8622_get_f0_info(struct aw8622_haptic *haptic)
{
	const struct firmware *firmware;

	if (request_firmware(&firmware, "aw8622_f0_info.bin", haptic->dev) < 0) {
		dev_err(haptic->dev, "%s request_firmware failed\n", __func__);
		return -EIO;
	}
	pr_info("%s data %d size = %zu\n", __func__, firmware->data[0], firmware->size);
	if (firmware->size > 0) {
		haptic->load_idx_offset = firmware->data[0] & 0xff;
		pr_info("%s haptic->load_idx_offset 1 = %u\n", __func__, haptic->load_idx_offset);
	}

	if (haptic->load_idx_offset != LOW_F0_LOAD_WAVEFORM_OFFSET &&
			haptic->load_idx_offset != MID_F0_LOAD_WAVEFORM_OFFSET &&
			haptic->load_idx_offset != HIGH_F0_LOAD_WAVEFORM_OFFSET) {
		haptic->load_idx_offset = MID_F0_LOAD_WAVEFORM_OFFSET;
		pr_info("%s haptic->load_idx_offset 2 = %u\n", __func__, haptic->load_idx_offset);
	} 
	haptic->load_idx_offset *= NUMS_WAVEFORM_USED;
    release_firmware(firmware);
	pr_info("%s haptic->load_idx_offset 3 = %u\n", __func__, haptic->load_idx_offset);
	return 0;
	
}


static void aw8622_waveform_data_delay_work(struct work_struct *delay_work) {
	int i = 0;
	int cunt = 0;
	int ret = 0;
	unsigned int wave_max_len = 0;
	//int buf_size = 0;

	struct aw8622_haptic *haptic = NULL;
	struct delayed_work *p_delayed_work = NULL;
	
	//haptic->is_wavefrom_ready = false;
	p_delayed_work = container_of(delay_work,
					struct delayed_work, work);
	
	haptic = container_of(p_delayed_work,
					struct aw8622_haptic, load_waveform_work);

	pr_info("%s load wavefile func enter\n", __func__);

	/* get aw8622 f0 info */

	aw8622_get_f0_info(haptic);
	haptic->waveform_data_nums= NUMS_WAVEFORM_USED;

	if ( !haptic->is_malloc_wavedata_info ) {
		haptic->p_waveform_data = vmalloc(haptic->waveform_data_nums * sizeof(struct waveform_data_info));
		if (!haptic->p_waveform_data) {
			pr_err("%s: Error allocating memory\n", __func__);
			return;
		}
		haptic->is_malloc_wavedata_info = true;
	}
	memset(haptic->p_waveform_data, 0, haptic->waveform_data_nums * sizeof(struct waveform_data_info));

	pr_info("%s waveform file offset idx = %d\n", __func__, haptic->load_idx_offset);
	for (i = 0; i < haptic->waveform_data_nums; i++) {
		haptic->cur_load_idx = i;
		ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
		        aw8622_waveform_file_name[haptic->cur_load_idx + haptic->load_idx_offset], haptic->dev, GFP_KERNEL,
		        haptic, aw8622_wavefrom_data_load);
		if (ret < 0) {
			dev_err(haptic->dev, "request_firmware_nowait faied effect id = %d, ret = %d", haptic->cur_load_idx, ret);
		}
		#if 1
		cunt = 0;
		while ( !haptic->p_waveform_data[haptic->cur_load_idx].is_loaded ) {
			cunt++;
			msleep(100);
			if (cunt > 200) {
				pr_err("%s: load waveform file %s faied\n", __func__, aw8622_waveform_file_name[haptic->cur_load_idx + haptic->load_idx_offset]);
				haptic->p_waveform_data[haptic->cur_load_idx].is_loaded = false;
				return;
			}
		}
		#endif
	}


	/* alloc DMA memory */
	for (i = 0; i < haptic->waveform_data_nums; i++) {
		if (haptic->p_waveform_data[i].is_loaded) {
			if (haptic->p_waveform_data[i].len > wave_max_len) {
				wave_max_len = haptic->p_waveform_data[i ].len;
			}
		}
	}

	if (haptic->is_wavefrom_ready) {
		pr_info("%s dma is malloc need release old memeory\n", __func__);
		dma_free_coherent(haptic->dev, haptic->wave_max_len, haptic->wave_vir,
					haptic->wave_phy);
		haptic->is_wavefrom_ready = false;
	}
	#if 0
	if (haptic->load_idx_offset == (LOW_F0_LOAD_WAVEFORM_OFFSET * NUMS_WAVEFORM_USED)) {
		haptic->center_freq = LOW_F0_FREQ;
	} else if (haptic->load_idx_offset == (MID_F0_LOAD_WAVEFORM_OFFSET * NUMS_WAVEFORM_USED)) {
		haptic->center_freq = MID_F0_FREQ;
	} else if (haptic->load_idx_offset == (HIGH_F0_LOAD_WAVEFORM_OFFSET * NUMS_WAVEFORM_USED)) {
		haptic->center_freq = HIGH_F0_FREQ;
	} else {
		haptic->center_freq = MID_F0_FREQ;
	}
	#endif
#if 1
	haptic->wave_max_len = wave_max_len + 4;
	pr_info("%s wavefile max len = %u\n", __func__, haptic->wave_max_len);
	//buf_size = (wave_max_len + 4) / 4 ;
	haptic->wave_vir = dma_alloc_coherent(haptic->dev, haptic->wave_max_len,
		  &haptic->wave_phy, GFP_KERNEL | GFP_DMA);
	if (!haptic->wave_vir) {
		pr_err("%s()  alloc memory fail\n", __func__);
		ret = -ENOMEM;
	}
	memset(haptic->wave_vir, 0, haptic->wave_max_len);
#endif
	haptic->is_wavefrom_ready = true;
}


static void aw8622_hw_off_work(struct work_struct *delay_work) {
	struct aw8622_haptic *haptic = NULL;
	struct delayed_work *p_delayed_work = NULL;
	
	p_delayed_work = container_of(delay_work,
					struct delayed_work, work);
	
	haptic = container_of(p_delayed_work,
					struct aw8622_haptic, hw_off_work);

	pr_info("%s\n",__func__);
	
	if (haptic->is_actived) {
		pr_info("%s is active hw off failed \n",__func__);
		return;
	}
	mutex_unlock(&haptic->mutex_lock);
	if (!haptic->is_actived)
	{
		pr_info("%s hw off success \n",__func__);
		gpio_set_value(haptic->hwen_gpio, 0); //hw disable
		udelay(1000);
		pr_info("%s pwm call  mt_pwm_disable", __func__);
		mt_pwm_disable(haptic->pwm_ch, aw8622_pwm_memory_mode_config.pmic_pad);
		haptic->is_power_on = false;
	}
	mutex_unlock(&haptic->mutex_lock);
}


void aw8622_switch_pwm_gpio_mode(struct aw8622_haptic *haptic, int mode)
{
	struct pinctrl_state *pins_state = NULL;
	
	if (mode >= (ARRAY_SIZE(aw8622_pwm_gpio_cfg))) {
			pr_err("%s() [PinC](%d) fail!! - invalid parameter!\n",
					__func__, mode);
			return;
	}
	
	if (IS_ERR(haptic->ppinctrl_pwm)) {
		pr_err("%s() [PinC] ppinctrl_haptic:%p Error! err:%ld\n",
				__func__, haptic->ppinctrl_pwm, PTR_ERR(haptic->ppinctrl_pwm));
		return;
	}

	pins_state = pinctrl_lookup_state(haptic->ppinctrl_pwm, aw8622_pwm_gpio_cfg[mode]);
	if (IS_ERR(pins_state)) {
		pr_notice("%s() [PinC] pinctrl_lockup(%p, %s) fail!\n",
				__func__, haptic->ppinctrl_pwm, aw8622_pwm_gpio_cfg[mode]);
		pr_notice("%s() [PinC] ppinctrl:%p, err:%ld\n",
				__func__, pins_state, PTR_ERR(pins_state));
		return;
	}
	
	pinctrl_select_state(haptic->ppinctrl_pwm, pins_state);
	pr_info("%s() [PinC] to mode:%d done.\n", __func__, mode);

}


static int aw8622_state_init(struct aw8622_haptic *haptic) {
	aw8622_switch_pwm_gpio_mode(haptic, HAPTIC_GPIO_AW8622_SET);
	haptic->is_power_on = false;
	haptic->is_actived = false;
	haptic->is_hwen = false;
	haptic->is_malloc_wavedata_info = false;
	haptic->duration = 10;
	haptic->is_wavefrom_ready = false;
	return 0;
}


static int aw8622_set_pwm_defalut_state(struct aw8622_haptic *haptic)
{
	int err = 0;
	pr_info("%s\n",__func__);
	//spin_lock(&haptic->spin_lock);
	//mt_set_pwm_disable(haptic->pwm_ch);
	pr_info("%s pwm call  mt_pwm_disable", __func__);
	mt_pwm_disable(aw8622_pwm_old_mode_config.pwm_no, aw8622_pwm_old_mode_config.pmic_pad);
	mt_pwm_clk_sel_hal(aw8622_pwm_old_mode_config.pwm_no, CLK_26M);
	aw8622_pwm_old_mode_config.clk_div = CLK_DIV1;
	aw8622_pwm_old_mode_config.PWM_MODE_OLD_REGS.DATA_WIDTH = HAPTIC_PWM_OLD_MODE_CLOCK/haptic->default_pwm_freq; //period 
	aw8622_pwm_old_mode_config.PWM_MODE_OLD_REGS.THRESH = HAPTIC_PWM_OLD_MODE_CLOCK/(haptic->default_pwm_freq * 2);
	pr_info("%s pwm call pwm_set_spec_config", __func__);
	err = pwm_set_spec_config(&aw8622_pwm_old_mode_config);
	//spin_unlock(&haptic->spin_lock);
	if (err < 0) {
		dev_err(haptic->dev, "%s pwm_set_spec_config \n", __func__);
	}
	return 0;
}

static void aw8622_haptic_stop(struct aw8622_haptic *haptic)
{
	aw8622_set_pwm_defalut_state(haptic);
	#if 0
	if (!haptic->wave_vir) {
		dma_free_coherent(haptic->dev, haptic->dma_len, haptic->wave_vir,
						haptic->wave_phy);
		haptic->wave_vir = NULL;
	}
	#endif
	haptic->is_actived = false;
}

static int aw8622_play_wave(struct aw8622_haptic *haptic)
{

	int ret = 0;
	int buf_size = 0;
	int pwm_wave_num = 0;
	
	struct aw8622_effect_state *p_effect_state = &haptic->effect_state;
	//struct waveform_data_info *p_waveform_info = &haptic->p_waveform_data[p_effect_state->effect_idx];
	struct waveform_data_info *p_waveform_info = NULL;
	
	pr_info("%s entern\n", __func__);


	if (!haptic->is_power_on && (p_effect_state->effect_idx > LONG_SHOCK_EFFECT_IDX && p_effect_state->effect_idx <= HIGH_SHORT_SHOCK_EFFECT_IDX)) { /* 1 2 3 */
		p_effect_state->effect_idx = p_effect_state->effect_idx + SHORT_SHOCK_NUMS;
	}

	p_waveform_info = &haptic->p_waveform_data[p_effect_state->effect_idx];
	if ( !(p_waveform_info->is_loaded) ) {
		pr_err("%s effect_id = %d wave data is not available\n", __func__, p_effect_state->effect_idx);
		ret = EINVAL;
		goto exit_1;
	}

	buf_size = (p_waveform_info->len + 3) / 4;
	#if 0
	haptic->wave_vir = dma_alloc_coherent(haptic->dev, p_waveform_info->len,
		   &haptic->wave_phy, GFP_KERNEL | GFP_DMA);
	if (!haptic->wave_vir) {
		pr_err("%s() dma alloc memory fail\n", __func__);
		ret = -ENOMEM;
		goto exit_1;
	}
	haptic->dma_len = p_waveform_info->len;
	#endif
	memcpy(haptic->wave_vir, p_waveform_info->data, p_waveform_info->len);
#if 0
	membuff = p_effect_state->wave_vir;

	for (i = 0; i < buf_size; i++) {
		pr_info("%s idx = %d, 0x%08x\n", __func__, i, membuff[i]);
	}
#endif
	
	if (haptic->is_power_on) {
		//pr_info("%s pwm call  mt_pwm_disable", __func__);
		mt_pwm_disable(aw8622_pwm_old_mode_config.pwm_no, aw8622_pwm_old_mode_config.pmic_pad);
	}
	
	if (haptic->effect_idx > LONG_SHOCK_EFFECT_IDX && haptic->effect_idx < LOW_F0_TEST_EFFECT_IDX) { /* only effect 1 2 3 4 5 6 is useful short shock */
		pwm_wave_num = 1;
		p_effect_state->secs =  p_waveform_info->us_time_len / USEC_PER_SEC;
		p_effect_state->nsces = (p_waveform_info->us_time_len % USEC_PER_SEC) * NSEC_PER_USEC;
		haptic->h_l_period = HAPTIC_PWM_MEMORY_MODE_CLOCK/BIT_NUMS_PER_SAMPLED_VALE/p_waveform_info->sample_freq;
	} else {
		pwm_wave_num = 0;
		haptic->h_l_period = HAPTIC_PWM_MEMORY_MODE_CLOCK/LONG_SHOCK_BIT_NUMS_PER_SAMPLED_VALE/p_waveform_info->sample_freq;
	}
	//pr_info("%s effect idx = %d, WAVE_NUM = %d, h_l_period = %u\n", __func__, haptic->effect_idx, pwm_wave_num, haptic->h_l_period);
	aw8622_pwm_memory_mode_config.PWM_MODE_MEMORY_REGS.HDURATION = haptic->h_l_period - 1;
	aw8622_pwm_memory_mode_config.PWM_MODE_MEMORY_REGS.LDURATION = haptic->h_l_period - 1;
	aw8622_pwm_memory_mode_config.PWM_MODE_MEMORY_REGS.WAVE_NUM = pwm_wave_num;
	aw8622_pwm_memory_mode_config.PWM_MODE_MEMORY_REGS.BUF0_BASE_ADDR = haptic->wave_phy;
	aw8622_pwm_memory_mode_config.PWM_MODE_MEMORY_REGS.BUF0_SIZE = buf_size - 1;
		/* old mode switch to memory mode don't need to disable */
	//mt_set_pwm_disable(aw8622_pwm_old_mode_config.pwm_no);
		//pr_info("%s pwm call pwm_set_spec_config", __func__);
	ret = pwm_set_spec_config(&aw8622_pwm_memory_mode_config);
#if 0
	else {
		//pwm_wave_num = p_effect_state->duration / (p_waveform_info->us_time_len / USEC_PER_MSEC);
		//if (pwm_wave_num > 0xffff) {
		//	pr_err("%s pwm_wave_num out of range \r\n", __func__);
		//	pwm_wave_num = 0xffff;
		//}
		mt_pwm_clk_sel_hal(aw8622_pwm_old_mode_config.pwm_no, CLK_26M);
		aw8622_pwm_old_mode_config.clk_div = CLK_DIV32;
		aw8622_pwm_old_mode_config.PWM_MODE_OLD_REGS.DATA_WIDTH = HAPTIC_PMW_SHOCKING_OLD_MODE_CLOCK/haptic->center_freq; //period 
		aw8622_pwm_old_mode_config.PWM_MODE_OLD_REGS.THRESH = HAPTIC_PMW_SHOCKING_OLD_MODE_CLOCK/(haptic->center_freq * 2);
		ret =  pwm_set_spec_config(&aw8622_pwm_old_mode_config);
	}
#endif
	if (ret < 0) {
		dev_err(haptic->dev, "%s pwm_set_spec_config failed ret = %d\n", __func__, ret);
	}
	if (!haptic->is_power_on) {
		gpio_set_value(haptic->hwen_gpio, 1); //hw enable
		haptic->is_power_on = true;
	}
	pr_info("%s effect idx = %d, play_time secs = %d, ms= %lu wave nums = %d\n", __func__,
				p_effect_state->effect_idx, p_effect_state->secs, p_effect_state->nsces/NSEC_PER_MSEC, pwm_wave_num);
	hrtimer_start(&haptic->timer, ktime_set(p_effect_state->secs,p_effect_state->nsces), HRTIMER_MODE_REL);
	return 0;
exit_1:
	   return ret;
}


/* param period : unit is ns 
 * parma duty: unit is ns
 */
 #if 0
static void aw8622_long_shock(struct aw8622_haptic *haptic) 
{
	int err;
	pr_info("%s()\n",__func__);
	mt_pwm_clk_sel_hal(aw8622_pwm_old_mode_config.pwm_no, CLK_26M);
	aw8622_pwm_old_mode_config.PWM_MODE_OLD_REGS.DATA_WIDTH = HAPTIC_PWM_OLD_MODE_CLOCK/haptic->center_freq;
	aw8622_pwm_old_mode_config.PWM_MODE_OLD_REGS.THRESH = HAPTIC_PWM_OLD_MODE_CLOCK/(haptic->center_freq*2);
	err = pwm_set_spec_config(&aw8622_pwm_old_mode_config);
	if (err < 0) {
		dev_err(haptic->dev, "%s pwm_set_spec_config \n", __func__);
	}
	msleep(haptic->duration);
	if (haptic->is_actived) {
		aw8622_set_pwm_defalut_state(haptic);
	}
}
#endif


/* functon : Periodic update PWM duty cycle */
static enum hrtimer_restart aw8622_haptic_timer_func(struct hrtimer *timer)
{
	struct aw8622_haptic *haptic = container_of(timer, struct aw8622_haptic,
					     timer);
	//struct aw8622_effect_state *p_effect_state = &haptic->effect_state;
	pr_info("%s enter\n", __func__);
	//p_effect_state->is_shock_stop = true;
	queue_work(haptic->aw8622_wq,&haptic->stop_play_work);

	return HRTIMER_NORESTART;
}

static void aw8622_init_effect_state(struct aw8622_haptic *haptic) {
	struct aw8622_effect_state *p_effect_state = &haptic->effect_state;

	if (haptic->effect_idx == LONG_SHOCK_EFFECT_IDX) {
		if (haptic->duration < 200) {
			haptic->duration -= 15;
		} else if (haptic->duration < 1000) {
			haptic->duration -= 50;
		} else {
			haptic->duration -= 100;
		}
	}
	p_effect_state->effect_idx = haptic->effect_idx;
	p_effect_state->secs = haptic->duration / MSEC_PER_SEC;
	p_effect_state->nsces = (haptic->duration % MSEC_PER_SEC) * NSEC_PER_MSEC;
	p_effect_state->duration = haptic->duration;
	//p_effect_state->is_shock_stop = false;
	pr_info("%s, duration = %u, secs = %d", __func__, haptic->duration, p_effect_state->secs);
}


static void aw8622_haptic_play_work(struct work_struct *work)
{
	struct aw8622_haptic *haptic = container_of(work,
					struct aw8622_haptic, play_work);
	int ret = 0;

	//struct aw8622_effect_state *p_effect_state = &haptic->effect_state;
	//struct waveform_data_info *p_waveform_info = &haptic->p_waveform_data[p_effect_state->effect_idx];
	pr_info("%s entern \n", __func__);
	//mutex_lock(&haptic->mutex_lock);
	if ( haptic->is_actived ) {
		aw8622_init_effect_state(haptic);
		ret = aw8622_play_wave(haptic);
		if (ret < 0) {
			pr_err("%s aw8622_play_wave failed\n", __func__);
			/* clear state*/
			queue_work(haptic->aw8622_wq,&haptic->stop_play_work);
		}
		haptic->is_real_play = true;
	}
	//mutex_unlock(&haptic->mutex_lock);
}

static void aw8622_haptic_stop_play_work(struct work_struct *work)
{
	struct aw8622_haptic *haptic = container_of(work,
					struct aw8622_haptic, stop_play_work);
	//struct aw8622_effect_state *p_effect_state = &haptic->effect_state;
	//struct waveform_data_info *p_waveform_info = &haptic->p_waveform_data[p_effect_state->effect_idx];
	pr_info("%s entern \n", __func__);

	if ( !haptic->is_actived ) {
		dev_err(haptic->dev, "%s logic error \n", __func__);
	}
	mutex_lock(&haptic->mutex_lock);
	cancel_delayed_work_sync(&haptic->hw_off_work);
	//schedule_delayed_work(&haptic->hw_off_work, 60 * HZ); //delay 1 mins
	queue_delayed_work(haptic->aw8622_wq, &haptic->hw_off_work, 30 * HZ);
	
	aw8622_haptic_stop(haptic);
	haptic->is_real_play = false;
	mutex_unlock(&haptic->mutex_lock);
}


static void aw8622_haptic_test_work(struct work_struct *work)
{
	struct aw8622_haptic *haptic = container_of(work,
					struct aw8622_haptic, test_work);
	struct waveform_data_info *p_waveform_info = NULL;
	int cnt_idx = 0;
	int ret = 0;
	int buf_size = 0;
	int pwm_wave_num = 0;
	int err;
	
	cancel_delayed_work_sync(&haptic->hw_off_work);
	haptic->is_actived = true;
	for (cnt_idx = 0; cnt_idx < haptic->test_cnt; cnt_idx++) {
		/* start */
		p_waveform_info = &haptic->p_waveform_data[0];

		buf_size = (p_waveform_info->len + 3) / 4;
		memcpy(haptic->wave_vir, p_waveform_info->data, p_waveform_info->len);
		haptic->h_l_period = HAPTIC_PWM_MEMORY_MODE_CLOCK/BIT_NUMS_PER_SAMPLED_VALE/p_waveform_info->sample_freq;
		pwm_wave_num = 0;
		aw8622_pwm_old_mode_config.clk_div = CLK_DIV32;
		aw8622_pwm_old_mode_config.PWM_MODE_OLD_REGS.DATA_WIDTH = 812500/320; //period 
		aw8622_pwm_old_mode_config.PWM_MODE_OLD_REGS.THRESH = 812500/(320 * 2);

		//aw8622_pwm_memory_mode_config.PWM_MODE_MEMORY_REGS.WAVE_NUM = pwm_wave_num;
		//aw8622_pwm_memory_mode_config.PWM_MODE_MEMORY_REGS.BUF0_BASE_ADDR = haptic->wave_phy;
		//aw8622_pwm_memory_mode_config.PWM_MODE_MEMORY_REGS.BUF0_SIZE = buf_size - 1;
		/* old mode switch to memory mode don't need to disable */
		//mt_set_pwm_disable(aw8622_pwm_old_mode_config.pwm_no);
		mt_pwm_disable(aw8622_pwm_old_mode_config.pwm_no, aw8622_pwm_old_mode_config.pmic_pad);
		ret =  pwm_set_spec_config(&aw8622_pwm_old_mode_config);
		if (ret < 0) {
			dev_err(haptic->dev, "%s pwm_set_spec_config failed ret = %d\n", __func__, ret);
		}
		if (!haptic->is_power_on) {
			gpio_set_value(haptic->hwen_gpio, 1); //hw enable
			haptic->is_power_on = true;
		}
		pr_info("%s start play_time secs = %d, enter memory mode\n", __func__, 5);	
		
		msleep(5000);
		
		/* end */
		//mt_set_pwm_disable(haptic->pwm_ch);
		mt_pwm_disable(aw8622_pwm_old_mode_config.pwm_no, aw8622_pwm_old_mode_config.pmic_pad);
		mt_pwm_clk_sel_hal(aw8622_pwm_old_mode_config.pwm_no, CLK_26M);
		aw8622_pwm_old_mode_config.clk_div = CLK_DIV1;
		aw8622_pwm_old_mode_config.PWM_MODE_OLD_REGS.DATA_WIDTH = HAPTIC_PWM_OLD_MODE_CLOCK/haptic->default_pwm_freq; //period 
		aw8622_pwm_old_mode_config.PWM_MODE_OLD_REGS.THRESH = HAPTIC_PWM_OLD_MODE_CLOCK/(haptic->default_pwm_freq * 2);
		err = pwm_set_spec_config(&aw8622_pwm_old_mode_config);
		pr_info("%s end play enter OLD mode\n", __func__);		
	}
	haptic->is_actived = false;
	queue_delayed_work(haptic->aw8622_wq, &haptic->hw_off_work, 30 * HZ);
}
static int aw8622_hwen_init(struct aw8622_haptic *haptic)
{
	int ret = 0;
	struct device_node *node = haptic->dev->of_node;
	haptic->hwen_gpio = of_get_named_gpio(node, "hwen-gpio", 0);
	if ((!gpio_is_valid(haptic->hwen_gpio))) {
		dev_err(haptic->dev, "%s: dts don't provide hwen-gpio\n",__func__);
		return -EINVAL;
	}

	ret = gpio_request(haptic->hwen_gpio, "aw8622-hwen");
	if (ret) {
		dev_err(haptic->dev, "%s: unable to request gpio [%d]\n",__func__, haptic->hwen_gpio);
		return ret;
	}

	ret = gpio_direction_output(haptic->hwen_gpio, 0);
	if (ret) {
		gpio_free(haptic->hwen_gpio);
		dev_err(haptic->dev, "%s: unable to set direction of gpio\n",__func__);
		return ret;
	}
	return ret;
}


static int aw8622_parse_devicetree_info(struct aw8622_haptic *haptic){
	int err = 0;

	/* get sample preiod */
	err = of_property_read_u32(haptic->dev->of_node, "waveform_sample_period", &haptic->wave_sample_period);
	if ( err < 0 ) {
		dev_err(haptic->dev, "get waveform_sample_period %d failed", err);
		return -EINVAL;
	}

	err = of_property_read_u32(haptic->dev->of_node, "center_freq", &haptic->center_freq);
	if ( err < 0 ) {
		dev_err(haptic->dev, "get center_freq %d failed", err);
		return -EINVAL;
	}

	err = of_property_read_u32(haptic->dev->of_node, "default_pwm_freq", &haptic->default_pwm_freq);
	if ( err < 0 ) {
		dev_err(haptic->dev, "get default_pwm_freq %d failed", err);
		return -EINVAL;
	}


	err = of_property_read_u32(haptic->dev->of_node, "pwm_ch",
		   &haptic->pwm_ch);
	if ( err < 0 ) {
		dev_err(haptic->dev, "get pwm_ch %d failed", err);
		return -EINVAL;
	}

	/* get ctrl */
	haptic->ppinctrl_pwm = devm_pinctrl_get(haptic->dev);
	if (IS_ERR(haptic->ppinctrl_pwm)) {
		   pr_notice("%s() [PinC]cannot find pinctrl! ptr_err:%ld.\n",
				  __func__, PTR_ERR(haptic->ppinctrl_pwm));
		   err = PTR_ERR(haptic->ppinctrl_pwm);
	}

	pr_info("%s dt info def_pwm_freq = %uHz center_freq = %u \n", __func__, haptic->default_pwm_freq, haptic->center_freq);
	pr_info("%s dt info pwmc_ch = %u\n",__func__, haptic->pwm_ch);

	return err;
}


static ssize_t aw8622_activate_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aw8622_haptic *haptic = dev_get_drvdata(dev);

	/* For now nothing to show */
	return snprintf(buf, PAGE_SIZE, "%d\n", haptic->is_actived);
}

static ssize_t aw8622_activate_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aw8622_haptic *haptic = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;
	struct aw8622_effect_state *p_effect_state = &haptic->effect_state;
	//struct waveform_data_info *p_waveform_info = &haptic->p_waveform_data[p_effect_state->effect_idx];

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	pr_info("%s: value=%d\n", __FUNCTION__, val);
	mutex_lock(&haptic->mutex_lock);
	if (val == 1) {
		if ( !haptic->is_actived && haptic->is_wavefrom_ready) {
			haptic->is_actived = true;
			queue_work(haptic->aw8622_wq,&haptic->play_work);
			//schedule_work(&haptic->play_work);
		} else {
			if ( !haptic->is_wavefrom_ready ) {
				pr_info("%s haptic waveform not ready\n", __func__);
			} else {
				pr_info("%s haptic is active, wait and try again\n", __func__);
			}
		}
	} else {
		//dev_info(haptic->dev, "%s Manually stop haptic\n", __func__);
		if ( p_effect_state->effect_idx == LONG_SHOCK_EFFECT_IDX && haptic->is_real_play) { //only long shock can stop 
			if (hrtimer_try_to_cancel(&haptic->timer) > 0 ) { // timer is active and canel success
				/* cancel success */
				dev_info(haptic->dev, "%s Manually stop haptic success \n", __func__);
				queue_work(haptic->aw8622_wq,&haptic->stop_play_work);
			}
		} else if (p_effect_state->effect_idx >= LOW_F0_TEST_EFFECT_IDX) { //cali tese 
			if (hrtimer_try_to_cancel(&haptic->timer) > 0 ) { // timer is active and canel success
				/* cancel success */
				dev_info(haptic->dev, "%s Manually stop haptic success \n", __func__);
				queue_work(haptic->aw8622_wq,&haptic->stop_play_work);
			}
		}
	}
	mutex_unlock(&haptic->mutex_lock);
	return count;
}


static ssize_t aw8622_index_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aw8622_haptic *haptic = dev_get_drvdata(dev);

	/* For now nothing to show */
	return snprintf(buf, PAGE_SIZE, "haptic->effect_idx = %d, \n", haptic->effect_idx);
}


static ssize_t aw8622_index_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aw8622_haptic *haptic = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val >= haptic->waveform_data_nums) {
		val = haptic->waveform_data_nums - 1;
	}
	
	haptic->effect_idx = val;
	pr_info("%s: index = %d\n", __FUNCTION__, val);
	return count;
}


/* sys fs */
static ssize_t aw8622_duration_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aw8622_haptic *haptic = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", haptic->duration);
}

/* duration unit ms */
static ssize_t aw8622_duration_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aw8622_haptic *haptic = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	pr_info("%s: duration = %d\n", __FUNCTION__, val);
	
	/* setting 0 on duration is NOP for now */
	if (val <= 0)
		return count;
	
	mutex_lock(&haptic->mutex_lock);
	if (val < 20) { //20ms
		haptic->effect_idx = 1; //short haptic 1
	} else if (val >= 20 && val < 40) {
		haptic->effect_idx = 2; //short haptic 2
	}
	else if (val >= 40 && val < 60) {
		haptic->effect_idx = 3; //short haptic 3
	} else {
		haptic->effect_idx = 0;
	}
	haptic->duration = val;
	mutex_unlock(&haptic->mutex_lock);
	return count;
}

static ssize_t aw8622_hwen_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aw8622_haptic *haptic = dev_get_drvdata(dev);

	/* For now nothing to show */
	return snprintf(buf, PAGE_SIZE, "%s\n", gpio_get_value(haptic->hwen_gpio) ? "enable" : "disable");
}

static ssize_t aw8622_hwen_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aw8622_haptic *haptic = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	pr_info("%s: value=%d\n", __FUNCTION__, val);

	if (val == 1) {
		gpio_set_value(haptic->hwen_gpio, 1); //hw enable
		msleep(50);
	} else {
		gpio_set_value(haptic->hwen_gpio, 0); //hw disable
	}
	return count;
}

static ssize_t aw8622_load_wavefile_ctrl_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aw8622_haptic *haptic = dev_get_drvdata(dev);

	/* For now nothing to show */
	return snprintf(buf, PAGE_SIZE, "%d\n", haptic->load_idx_offset);
}

/* wave load offset */
static ssize_t aw8622_load_wavefile_ctrl_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aw8622_haptic *haptic = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	pr_info("%s: load_idx_offset=%d\n", __FUNCTION__, val);
	haptic->load_idx_offset = val;

	schedule_delayed_work(&haptic->load_waveform_work, 0); //delay 10s	
	return count;
}

/* debug sys node */
static ssize_t aw8622_debug_val_ctrl_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aw8622_haptic *haptic = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "haptic->center_freq %u, haptic->h_l+period = %u default_pwm_freq = %u interval = %u \n", 
			haptic->center_freq, haptic->h_l_period, haptic->default_pwm_freq, haptic->interval);
}

/* duration unit ms */
static ssize_t aw8622_debug_val_ctrl_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aw8622_haptic *haptic = dev_get_drvdata(dev);
	int databuf[4] = {0};
	
	sscanf(buf,"%d %d %d %d",&databuf[0], &databuf[1], &databuf[2], &databuf[3]);

	
	haptic->center_freq = databuf[0];
	haptic->h_l_period = databuf[1];
	haptic->default_pwm_freq = databuf[2];
	haptic->interval = databuf[3];

	pr_info("%s haptic->center_freq %u, haptic->h_l+period = %u\n, default_pwm_freq = %u, interval = %u", 
			__func__, haptic->center_freq, haptic->h_l_period, haptic->default_pwm_freq, haptic->interval);
	return count;
}


static ssize_t aw8622_debug_pwm_ctrl_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "old mode pwm period = %d duty = %d\n", 
			aw8622_pwm_old_mode_config.PWM_MODE_OLD_REGS.DATA_WIDTH,
			aw8622_pwm_old_mode_config.PWM_MODE_OLD_REGS.THRESH);
}


/* ehco period duty is_enable > pwm_ctrl */
static ssize_t aw8622_debug_pwm_ctrl_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aw8622_haptic *haptic = dev_get_drvdata(dev);
	int databuf[4] = {0};
	int err = 0;
	sscanf(buf,"%d %d %d",&databuf[0], &databuf[1], &databuf[2]);
	if (databuf[3] != 0) {
		mt_pwm_clk_sel_hal(aw8622_pwm_old_mode_config.pwm_no, CLK_26M);
		aw8622_pwm_old_mode_config.clk_div = databuf[0];
		aw8622_pwm_old_mode_config.PWM_MODE_OLD_REGS.DATA_WIDTH = databuf[1]; //period 
		aw8622_pwm_old_mode_config.PWM_MODE_OLD_REGS.THRESH = databuf[2];
		//aw8622_switch_pwm_gpio_mode(haptic, HAPTIC_GPIO_AW8622_SET);
		pr_info("%s pwm call pwm_set_spec_config", __func__);
		err = pwm_set_spec_config(&aw8622_pwm_old_mode_config);
		if (err < 0) {
			dev_err(haptic->dev, "%s pwm_set_spec_config \n", __func__);
		}
	} else if (databuf[3] == 0) {
		aw8622_pwm_old_mode_config.clk_div = databuf[0];
		aw8622_pwm_old_mode_config.PWM_MODE_OLD_REGS.DATA_WIDTH = databuf[1]; //period 
		aw8622_pwm_old_mode_config.PWM_MODE_OLD_REGS.THRESH = databuf[2];
		pr_info("%s pwm call  mt_pwm_disable", __func__);
		mt_pwm_disable(haptic->pwm_ch, aw8622_pwm_old_mode_config.pmic_pad);
		//aw8622_switch_pwm_gpio_mode(haptic, HAPTIC_GPIO_AW8622_DEFAULT);
	}
	return count;
}

static ssize_t aw8622_debug_test_cnt_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aw8622_haptic *haptic = dev_get_drvdata(dev);
	int databuf[1] = {0};
	
	sscanf(buf,"%d=",&databuf[0]);

	
	haptic->test_cnt = databuf[0];


	pr_info("%s haptic->test_cnt =  %u", __func__, haptic->test_cnt);
	queue_work(haptic->aw8622_wq, &haptic->test_work);
	
	return count;
}

static DEVICE_ATTR(activate, S_IWUSR | S_IRUGO, aw8622_activate_show,
		   aw8622_activate_store);
static DEVICE_ATTR(index, S_IWUSR | S_IRUGO, aw8622_index_show,
		   aw8622_index_store);
static DEVICE_ATTR(duration, S_IWUSR | S_IRUGO, aw8622_duration_show,
		   aw8622_duration_store);

static DEVICE_ATTR(hwen, S_IWUSR | S_IRUGO, aw8622_hwen_show,
		   aw8622_hwen_store);

static DEVICE_ATTR(load_wavefile_ctrl, S_IWUSR | S_IRUGO, aw8622_load_wavefile_ctrl_show,
		   aw8622_load_wavefile_ctrl_store);

static DEVICE_ATTR(debug_val_ctrl, S_IWUSR | S_IRUGO, aw8622_debug_val_ctrl_show,
		   aw8622_debug_val_ctrl_store);

/* sys debug node */
static DEVICE_ATTR(pwm_ctrl, S_IWUSR | S_IRUGO, aw8622_debug_pwm_ctrl_show,
		   aw8622_debug_pwm_ctrl_store);
static DEVICE_ATTR(test_cnt, S_IWUSR | S_IRUGO, NULL,
		   aw8622_debug_test_cnt_store);


static struct attribute *aw8622_vibrator_attributes[] = {
	&dev_attr_duration.attr,
	&dev_attr_activate.attr,
	&dev_attr_index.attr,
	&dev_attr_hwen.attr,
	&dev_attr_load_wavefile_ctrl.attr,

	&dev_attr_debug_val_ctrl.attr,
	&dev_attr_pwm_ctrl.attr,
	&dev_attr_test_cnt.attr,
	NULL,
};


static struct attribute_group aw8622_vibrator_attribute_group = {
	.attrs = aw8622_vibrator_attributes
};

static int aw8622_haptic_probe(struct platform_device *pdev)
{
	struct aw8622_haptic *haptic;
	int err;

	pr_info("%s enter \r\n", __func__);
	haptic = devm_kzalloc(&pdev->dev, sizeof(*haptic), GFP_KERNEL);
	if (!haptic)
		return -ENOMEM;

	haptic->dev = &pdev->dev;
	platform_set_drvdata(pdev, haptic);


	err = aw8622_parse_devicetree_info(haptic);
	if ( err < 0 ) {
		dev_err(haptic->dev, "%s aw8622 parse devicetree info failed\n", __func__);
		return err;
	}

	err = aw8622_hwen_init(haptic);
	if (err ) {
		dev_err(haptic->dev, "%s aw8622 hwen error\n", __func__);
		return err;
	}

	haptic->aw8622_wq = create_singlethread_workqueue("aw8622 vibrator work queue");
	if ( !haptic->aw8622_wq ) {
		dev_err(haptic->dev, "%s create workqueue error\n", __func__);
	}

	INIT_WORK(&haptic->play_work, aw8622_haptic_play_work);
	INIT_WORK(&haptic->stop_play_work, aw8622_haptic_stop_play_work);
	INIT_WORK(&haptic->test_work, aw8622_haptic_test_work);
	INIT_DELAYED_WORK(&haptic->load_waveform_work, aw8622_waveform_data_delay_work);
	INIT_DELAYED_WORK(&haptic->hw_off_work, aw8622_hw_off_work);

	//spin_lock_init(&haptic->spin_lock);

	hrtimer_init(&haptic->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	haptic->timer.function = aw8622_haptic_timer_func;
	mutex_init(&haptic->mutex_lock);

	haptic->dev->dma_mask = &aw8622_dma_mask;
	haptic->dev->coherent_dma_mask = aw8622_dma_mask;
	

	aw8622_pwm_memory_mode_config.pwm_no = haptic->pwm_ch;
	aw8622_pwm_old_mode_config.pwm_no = haptic->pwm_ch;

	pr_info("%s waveform_sample_period = %d\n", __func__, haptic->wave_sample_period);

	aw8622_state_init(haptic);

	haptic->interval = 75;
	
	err = sysfs_create_group(&pdev->dev.kobj,
			       &aw8622_vibrator_attribute_group);
	if (err < 0) {
		dev_info(&pdev->dev, "%s error creating sysfs attr files\n",
			 __func__);
		return err;
	}

	haptic->load_idx_offset = MID_F0_LOAD_WAVEFORM_OFFSET * NUMS_WAVEFORM_USED;
	schedule_delayed_work(&haptic->load_waveform_work, 10 * HZ); //delay 10s

	pr_info("%s probe success \r\n", __func__);
	return 0;
}

static int __maybe_unused aw8622_haptic_suspend(struct device *dev)
{
	struct aw8622_haptic *haptic = dev_get_drvdata(dev);

	cancel_delayed_work_sync(&haptic->hw_off_work);
	mutex_lock(&haptic->mutex_lock);
	if (haptic->is_power_on) {
		gpio_set_value(haptic->hwen_gpio, 0);
		udelay(500);
		pr_info("%s pwm call  mt_pwm_disable", __func__);
		mt_pwm_disable(aw8622_pwm_old_mode_config.pwm_no, aw8622_pwm_old_mode_config.pmic_pad);
		haptic->is_power_on = false;
	}
	mutex_unlock(&haptic->mutex_lock);
	return 0;
}

static int __maybe_unused aw8622_haptic_resume(struct device *dev)
{
	//struct aw8622_haptic *haptic = dev_get_drvdata(dev);
	//int err;

	pr_info("%s\n", __func__);
	/* Output default period 50% duty cycle PWM */
	//aw8622_pwm_init(haptic);

	/* Stabilize the signal */
	//gpio_set_value(haptic->hwen_gpio, 1);
	return 0;
}

static SIMPLE_DEV_PM_OPS(aw8622_haptic_pm_ops,
			 aw8622_haptic_suspend, aw8622_haptic_resume);

#ifdef CONFIG_OF
static const struct of_device_id pwm_vibra_dt_match_table[] = {
	{ .compatible = "awinic,aw8622" },
	{},
};
MODULE_DEVICE_TABLE(of, pwm_vibra_dt_match_table);
#endif

static struct platform_driver aw8622_haptic_driver = {
	.probe	= aw8622_haptic_probe,
	.driver	= {
		.name	= "awinic,aw8622-haptic",
		.pm	= &aw8622_haptic_pm_ops,
		.of_match_table = of_match_ptr(pwm_vibra_dt_match_table),
	},
};
module_platform_driver(aw8622_haptic_driver);

MODULE_AUTHOR("luofuhong@awinic.com");
MODULE_DESCRIPTION("aw8622 haptic driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("aw8622 haptic");
