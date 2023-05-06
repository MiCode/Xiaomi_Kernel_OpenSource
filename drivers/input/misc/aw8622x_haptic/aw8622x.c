/*
 * aw8622x.c
 *
 * Copyright (c) 2020 AWINIC Technology CO., LTD
 *
 * vun Author: Ray <yulei@awinic.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/syscalls.h>
#include <linux/power_supply.h>
#include <linux/pm_qos.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <linux/vmalloc.h>
#include "aw8622x_reg.h"
#include "aw8622x.h"
#include "haptic.h"
/******************************************************
 *
 * Value
 *
 ******************************************************/
static char *aw8622x_ram_name = "aw8622x_haptic.bin";
static char aw8622x_rtp_name[][AW8622X_RTP_NAME_MAX] = {
	{"aw8622x_osc_rtp_24K_5s.bin"},
	{"AcousticGuitar_RTP.bin"},	//21
	{"Blues_RTP.bin"},
	{"Candy_RTP.bin"},
	{"Carousel_RTP.bin"},
	{"Celesta_RTP.bin"},
	{"Childhood_RTP.bin"},
	{"Country_RTP.bin"},
	{"Cowboy_RTP.bin"},
	{"Echo_RTP.bin"},
	{"Fairyland_RTP.bin"},
	{"Fantasy_RTP.bin"},	//31
	{"Field_Trip_RTP.bin"},
	{"Glee_RTP.bin"},
	{"Glockenspiel_RTP.bin"},
	{"Ice_Latte_RTP.bin"},
	{"Kung_Fu_RTP.bin"},
	{"Leisure_RTP.bin"},
	{"Lollipop_RTP.bin"},
	{"MiMix2_RTP.bin"},
	{"Mi_RTP.bin"},
	{"MiHouse_RTP.bin"},	//41
	{"MiJazz_RTP.bin"},
	{"MiRemix_RTP.bin"},
	{"Mountain_Spring_RTP.bin"},
	{"Orange_RTP.bin"},
	{"WindChime_RTP.bin"},	//46
	{"Space_Age_RTP.bin"},
	{"ToyRobot_RTP.bin"},
	{"Vigor_RTP.bin"},
	{"Bottle_RTP.bin"},
	{"Bubble_RTP.bin"},	//51
	{"Bullfrog_RTP.bin"},
	{"Burst_RTP.bin"},
	{"Chirp_RTP.bin"},
	{"Clank_RTP.bin"},
	{"Crystal_RTP.bin"},
	{"FadeIn_RTP.bin"},
	{"FadeOut_RTP.bin"},
	{"Flute_RTP.bin"},
	{"Fresh_RTP.bin"},
	{"Frog_RTP.bin"},	//61
	{"Guitar_RTP.bin"},
	{"Harp_RTP.bin"},
	{"IncomingMessage_RTP.bin"},
	{"MessageSent_RTP.bin"},
	{"Moment_RTP.bin"},
	{"NotificationXylophone_RTP.bin"},
	{"Potion_RTP.bin"},
	{"Radar_RTP.bin"},
	{"Spring_RTP.bin"},
	{"Swoosh_RTP.bin"},	//71
	{"Gesture_UpSlide_RTP.bin"},
	{"Gesture_UpHold_RTP.bin"},
	{"Charge_Wire_RTP.bin"},
	{"Charge_Wireless_RTP.bin"},
	{"Unlock_Failed_RTP.bin"},
	{"FOD_Motion1_RTP.bin"},
	{"FOD_Motion2_RTP.bin"},
	{"FOD_Motion3_RTP.bin"},
	{"FOD_Motion4_RTP.bin"},
	{"FaceID_Wrong1_RTP.bin"},
	{"FaceID_Wrong2_RTP.bin"},	//82
	{"uninstall_animation_rtp.bin"},
	{"uninstall_dialog_rtp.bin"},
	{"screenshot_rtp.bin"},
	{"lockscreen_camera_entry_rtp.bin"},
	{"launcher_edit_rtp.bin"},
	{"launcher_icon_selection_rtp.bin"},
	{"taskcard_remove_rtp.bin"},
	{"task_cleanall_rtp.bin"},
	{"new_iconfolder_rtp.bin"},
	{"notification_remove_rtp.bin"},
	{"notification_cleanall_rtp.bin"},
	{"notification_setting_rtp.bin"},
	{"game_turbo_rtp.bin"},
	{"NFC_card_rtp.bin"},
	{"wakeup_voice_assistant_rtp.bin"},
	{"NFC_card_slow_rtp.bin"},
	{"POCO_RTP.bin"},	//99
	{"aw8622x_rtp.bin"},	//100
	{"offline_countdown_RTP.bin"},
	{"scene_bomb_injury_RTP.bin"},
	{"scene_bomb_RTP.bin"},	//103
	{"door_open_RTP.bin"},
	{"aw8622x_rtp.bin"},
	{"scene_step_RTP.bin"},	//106
	{"crawl_RTP.bin"},
	{"scope_on_RTP.bin"},
	{"scope_off_RTP.bin"},
	{"magazine_quick_RTP.bin"},
	{"grenade_RTP.bin"},
	{"scene_getshot_RTP.bin"},	//112
	{"grenade_explosion_RTP.bin"},
	{"punch_RTP.bin"},
	{"pan_RTP.bin"},
	{"bandage_RTP.bin"},
	{"aw8622x_rtp.bin"},
	{"scene_jump_RTP.bin"},
	{"vehicle_plane_RTP.bin"},	//119
	{"scene_openparachute_RTP.bin"},	//120
	{"scene_closeparachute_RTP.bin"},	//121
	{"vehicle_collision_RTP.bin"},
	{"vehicle_buggy_RTP.bin"},	//123
	{"vehicle_dacia_RTP.bin"},	//124
	{"vehicle_moto_RTP.bin"},	//125
	{"firearms_akm_RTP.bin"},	//126
	{"firearms_m16a4_RTP.bin"},	//127
	{"aw8622x_rtp.bin"},
	{"firearms_awm_RTP.bin"},	//129
	{"firearms_mini14_RTP.bin"},	//130
	{"firearms_vss_RTP.bin"},	//131
	{"firearms_qbz_RTP.bin"},	//132
	{"firearms_ump9_RTP.bin"},	//133
	{"firearms_dp28_RTP.bin"},	//134
	{"firearms_s1897_RTP.bin"},	//135
	{"aw8622x_rtp.bin"},
	{"firearms_p18c_RTP.bin"},	//137
	{"aw8622x_rtp.bin"},
	{"aw8622x_rtp.bin"},
	{"CFM_KillOne_RTP.bin"},
	{"CFM_Headshot_RTP.bin"},	//141
	{"CFM_MultiKill_RTP.bin"},
	{"CFM_KillOne_Strong_RTP.bin"},
	{"CFM_Headshot_Strong_RTP.bin"},
	{"CFM_MultiKill_Strong_RTP.bin"},
	{"CFM_Weapon_Grenade_Explode_RTP.bin"},
	{"CFM_Weapon_Grenade_KillOne_RTP.bin"},
	{"CFM_ImpactFlesh_Normal_RTP.bin"},
	{"CFM_Weapon_C4_Installed_RTP.bin"},
	{"CFM_Hero_Appear_RTP.bin"},
	{"CFM_UI_Reward_OpenBox_RTP.bin"},
	{"CFM_UI_Reward_Task_RTP.bin"},
	{"CFM_Weapon_BLT_Shoot_RTP.bin"},	//153
	{"Atlantis_RTP.bin"},
	{"DigitalUniverse_RTP.bin"},
	{"Reveries_RTP.bin"},
	{"FOD_Motion_Triang_RTP.bin"},
	{"FOD_Motion_Flare_RTP.bin"},
	{"FOD_Motion_Ripple_RTP.bin"},
	{"FOD_Motion_Spiral_RTP.bin"},
	{"gamebox_launch_rtp.bin"}, // 161
	{"Gesture_Back_Pull_RTP.bin"},// 162
	{"Gesture_Back_Release_RTP.bin"},// 163
	{"alert_rtp.bin"},// 164
	{"feedback_negative_light_rtp.bin"},// 165
	{"feedback_neutral_rtp.bin"},// 166
	{"feedback_positive_rtp.bin"},// 167
	{"fingerprint_record_rtp.bin"},// 168
	{"lockdown_rtp.bin"},// 169
	{"sliding_damping_rtp.bin"},// 170
	{"todo_alldone_rtp.bin"},// 171
	{"uninstall_animation_icon_rtp.bin"},// 172
	{"signal_button_highlight_rtp.bin"},//173
	{"signal_button_negative_rtp.bin"},
	{"signal_button_rtp.bin"},
	{"signal_clock_high_rtp.bin"},//176
	{"signal_clock_rtp.bin"},
	{"signal_clock_unit_rtp.bin"},
	{"signal_inputbox_rtp.bin"},
	{"signal_key_high_rtp.bin"},
	{"signal_key_unit_rtp.bin"},//181
	{"signal_list_highlight_rtp.bin"},
	{"signal_list_rtp.bin"},
	{"signal_picker_rtp.bin"},
	{"signal_popup_rtp.bin"},
	{"signal_seekbar_rtp.bin"},//186
	{"signal_switch_rtp.bin"},
	{"signal_tab_rtp.bin"},
	{"signal_text_rtp.bin"},
	{"signal_transition_light_rtp.bin"},
	{"signal_transition_rtp.bin"},//191
	{"haptics_video_rtp.bin"},//192
};

struct pm_qos_request aw8622x_pm_qos_req_vb;

static int wf_repeat[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };
 /******************************************************
 *
 * aw8622x i2c write/read
 *
 ******************************************************/
static int aw8622x_i2c_write(struct aw8622x *aw8622x,
			     unsigned char reg_addr, unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW8622X_I2C_RETRIES) {
		ret =
		    i2c_smbus_write_byte_data(aw8622x->i2c, reg_addr, reg_data);
		if (ret < 0) {
			aw_dev_err(aw8622x->dev, "%s: i2c_write addr=0x%02X, data=0x%02X, cnt=%d, error=%d\n",
				__func__, reg_addr, reg_data, cnt, ret);
		} else {
			break;
		}
		cnt++;
		usleep_range(AW8622X_I2C_RETRY_DELAY * 1000,
			     AW8622X_I2C_RETRY_DELAY * 1000 + 500);
	}
	return ret;
}

int aw8622x_i2c_read(struct aw8622x *aw8622x,
			    unsigned char reg_addr, unsigned char *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW8622X_I2C_RETRIES) {
		ret = i2c_smbus_read_byte_data(aw8622x->i2c, reg_addr);
		if (ret < 0) {
			aw_dev_err(aw8622x->dev,
				"%s: i2c_read addr=0x%02X, cnt=%d error=%d\n",
				   __func__, reg_addr, cnt, ret);
		} else {
			*reg_data = ret;
			break;
		}
		cnt++;
		usleep_range(AW8622X_I2C_RETRY_DELAY * 1000,
			     AW8622X_I2C_RETRY_DELAY * 1000 + 500);
	}
	return ret;
}

int aw8622x_i2c_writes(struct aw8622x *aw8622x,
			unsigned char reg_addr, unsigned char *buf,
			unsigned int len)
{
	int ret = -1;
	unsigned char *data = NULL;

	data = kmalloc(len + 1, GFP_KERNEL);
	if (data == NULL) {
		aw_dev_err(aw8622x->dev,
			"%s: can not allocate memory\n", __func__);
		return -ENOMEM;
	}
	data[0] = reg_addr;
	memcpy(&data[1], buf, len);
	ret = i2c_master_send(aw8622x->i2c, data, len + 1);
	if (ret < 0)
		aw_dev_err(aw8622x->dev,
			"%s: i2c master send error\n", __func__);
	kfree(data);
	return ret;
}

static int aw8622x_i2c_write_bits(struct aw8622x *aw8622x,
				  unsigned char reg_addr, unsigned int mask,
				  unsigned char reg_data)
{
	int ret = -1;
	unsigned char reg_val = 0;

	ret = aw8622x_i2c_read(aw8622x, reg_addr, &reg_val);
	if (ret < 0) {
		aw_dev_err(aw8622x->dev,
			"%s: i2c read error, ret=%d\n", __func__, ret);
		return ret;
	}
	reg_val &= mask;
	reg_val |= reg_data;
	ret = aw8622x_i2c_write(aw8622x, reg_addr, reg_val);
	if (ret < 0) {
		aw_dev_err(aw8622x->dev,
			"%s: i2c write error, ret=%d\n", __func__, ret);
		return ret;
	}
	return 0;
}

unsigned char aw8622x_haptic_rtp_get_fifo_afs(struct aw8622x *aw8622x)
{
	unsigned char ret = 0;
	unsigned char reg_val = 0;

	aw8622x_i2c_read(aw8622x, AW8622X_REG_SYSST, &reg_val);
	reg_val &= AW8622X_BIT_SYSST_FF_AFS;
	ret = reg_val >> 3;
	return ret;
}

/*****************************************************
 *
 * rtp
 *
 *****************************************************/
void aw8622x_haptic_set_rtp_aei(struct aw8622x *aw8622x, bool flag)
{
	if (flag) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSINTM,
				       AW8622X_BIT_SYSINTM_FF_AEM_MASK,
				       AW8622X_BIT_SYSINTM_FF_AEM_ON);
	} else {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSINTM,
				       AW8622X_BIT_SYSINTM_FF_AEM_MASK,
				       AW8622X_BIT_SYSINTM_FF_AEM_OFF);
	}
}


/*****************************************************
 *
 * device tree
 *
 *****************************************************/
int aw8622x_parse_dt(struct device *dev, struct aw8622x *aw8622x,
			    struct device_node *np)
{
	unsigned int val = 0;
	unsigned int prctmode_temp[3];
	unsigned int sine_array_temp[4];
	unsigned int rtp_time[194];
	struct qti_hap_config *config = &aw8622x->config;
	struct device_node *child_node;
	struct qti_hap_effect *effect;
	int rc = 0, tmp, i = 0, j;

	val = of_property_read_u32(np,
			"aw8622x_vib_mode",
			&aw8622x->dts_info.mode);
	if (val != 0)
		aw_dev_err(aw8622x->dev,
			"%s aw8622x_vib_mode not found\n",
			__func__);
	val = of_property_read_u32(np,
			"aw8622x_vib_f0_pre",
			&aw8622x->dts_info.f0_ref);
	if (val != 0)
		aw_dev_err(aw8622x->dev, "%s vib_f0_ref not found\n",
			    __func__);
	val =
	    of_property_read_u32(np, "aw8622x_vib_f0_cali_percen",
				 &aw8622x->dts_info.f0_cali_percent);
	if (val != 0)
		aw_dev_err(aw8622x->dev, "%s vib_f0_cali_percent not found\n",
			    __func__);

	val = of_property_read_u32(np, "aw8622x_vib_cont_drv1_lvl",
				   &aw8622x->dts_info.cont_drv1_lvl_dt);
	if (val != 0)
		aw_dev_err(aw8622x->dev, "%s vib_cont_drv1_lvl not found\n",
			    __func__);
	val =
	    of_property_read_u32(np, "aw8622x_vib_cont_drv2_lvl",
				 &aw8622x->dts_info.cont_drv2_lvl_dt);
	if (val != 0)
		aw_dev_err(aw8622x->dev, "%s vib_cont_drv2_lvl not found\n",
			    __func__);
	val =
	    of_property_read_u32(np, "aw8622x_vib_cont_drv1_time",
				 &aw8622x->dts_info.cont_drv1_time_dt);
	if (val != 0)
		aw_dev_err(aw8622x->dev, "%s vib_cont_drv1_time not found\n",
			    __func__);
	val =
	    of_property_read_u32(np, "aw8622x_vib_cont_drv2_time",
				 &aw8622x->dts_info.cont_drv2_time_dt);
	if (val != 0)
		aw_dev_err(aw8622x->dev, "%s vib_cont_drv2_time not found\n",
			    __func__);
	val =
	    of_property_read_u32(np, "aw8622x_vib_cont_drv_width",
				 &aw8622x->dts_info.cont_drv_width);
	if (val != 0)
		aw_dev_err(aw8622x->dev, "%s vib_cont_drv_width not found\n",
			    __func__);
	val =
	    of_property_read_u32(np, "aw8622x_vib_cont_wait_num",
				 &aw8622x->dts_info.cont_wait_num_dt);
	if (val != 0)
		aw_dev_err(aw8622x->dev, "%s vib_cont_wait_num not found\n",
			    __func__);
	val =
	    of_property_read_u32(np, "aw8622x_vib_cont_brk_gain",
				 &aw8622x->dts_info.cont_brk_gain);
	if (val != 0)
		aw_dev_err(aw8622x->dev, "%s vib_cont_brk_gain not found\n",
			    __func__);
	val =
	    of_property_read_u32(np, "aw8622x_vib_cont_tset",
				 &aw8622x->dts_info.cont_tset);
	if (val != 0)
		aw_dev_err(aw8622x->dev, "%s vib_cont_tset not found\n",
			    __func__);
	val =
	    of_property_read_u32(np, "aw8622x_vib_cont_bemf_set",
				 &aw8622x->dts_info.cont_bemf_set);
	if (val != 0)
		aw_dev_err(aw8622x->dev, "%s vib_cont_bemf_set not found\n",
			    __func__);
	val =
	    of_property_read_u32(np, "aw8622x_vib_d2s_gain",
				 &aw8622x->dts_info.d2s_gain);
	if (val != 0)
		aw_dev_err(aw8622x->dev, "%s vib_d2s_gain not found\n",
			    __func__);
	val =
	    of_property_read_u32(np, "aw8622x_vib_cont_brk_time",
				 &aw8622x->dts_info.cont_brk_time_dt);
	if (val != 0)
		aw_dev_err(aw8622x->dev, "%s vib_cont_brk_time not found\n",
			    __func__);
	val =
	    of_property_read_u32(np, "aw8622x_vib_cont_track_margin",
				 &aw8622x->dts_info.cont_track_margin);
	if (val != 0)
		aw_dev_err(aw8622x->dev,
			"%s vib_cont_track_margin not found\n", __func__);

	val = of_property_read_u32_array(np, "aw8622x_vib_prctmode",
					 prctmode_temp,
					 ARRAY_SIZE(prctmode_temp));
	if (val != 0)
		aw_dev_err(aw8622x->dev, "%s vib_prctmode not found\n",
			    __func__);
	memcpy(aw8622x->dts_info.prctmode, prctmode_temp,
					sizeof(prctmode_temp));
	val = of_property_read_u32_array(np,
				"aw8622x_vib_sine_array",
				sine_array_temp,
				ARRAY_SIZE(sine_array_temp));
	if (val != 0)
		aw_dev_err(aw8622x->dev, "%s vib_sine_array not found\n",
			    __func__);
	memcpy(aw8622x->dts_info.sine_array, sine_array_temp,
		sizeof(sine_array_temp));
	aw8622x->dts_info.is_enabled_auto_bst =
			of_property_read_bool(np,
					"aw8622x_vib_is_enabled_auto_bst");
	aw_dev_err(aw8622x->dev,
		    "%s aw8622x->info.is_enabled_auto_bst = %d\n", __func__,
		    aw8622x->dts_info.is_enabled_auto_bst);
//yundi add
	val =
	    of_property_read_u32_array(np, "vib_rtp_time", rtp_time,
				       ARRAY_SIZE(rtp_time));
	if (val != 0)
		aw_dev_err(aw8622x->dev,"%s: vib_rtp_time not found\n", __func__);
	memcpy(aw8622x->dts_info.rtp_time, rtp_time, sizeof(rtp_time));

	val =
	    of_property_read_u32(np, "vib_effect_id_boundary",
				 &aw8622x->dts_info.effect_id_boundary);
	if (val != 0)
		aw_dev_err(aw8622x->dev,"%s: vib_effect_id_boundary not found\n", __func__);

	val =
	    of_property_read_u32(np, "vib_effect_max",
				 &aw8622x->dts_info.effect_max);
	if (val != 0)
		aw_dev_err(aw8622x->dev,"%s: vib_effect_max not found\n", __func__);

		config->play_rate_us = HAP_PLAY_RATE_US_DEFAULT;
	rc = of_property_read_u32(np, "qcom,play-rate-us", &tmp);
	if (!rc)
		config->play_rate_us = (tmp >= HAP_PLAY_RATE_US_MAX) ?
		    HAP_PLAY_RATE_US_MAX : tmp;

	aw8622x->constant.pattern = devm_kcalloc(aw8622x->dev,
						HAP_WAVEFORM_BUFFER_MAX,
						sizeof(u8), GFP_KERNEL);
	if (!aw8622x->constant.pattern)
		return -ENOMEM;

	tmp = of_get_available_child_count(np);
	aw8622x->predefined = devm_kcalloc(aw8622x->dev, tmp,
					  sizeof(*aw8622x->predefined),
					  GFP_KERNEL);
	if (!aw8622x->predefined)
		return -ENOMEM;

	aw8622x->effects_count = tmp;
	pr_info("%s: ---%d aw8622x->effects_count=%d\n", __func__, __LINE__,
	       aw8622x->effects_count);
	for_each_available_child_of_node(np, child_node) {
		effect = &aw8622x->predefined[i++];
		rc = of_property_read_u32(child_node, "qcom,effect-id",
					  &effect->id);
		if (rc != 0) {
			printk("%s: Read qcom,effect-id failed\n", __func__);
		}
		printk("%s: effect_id: %d\n", __func__, effect->id);

		effect->vmax_mv = config->vmax_mv;
		rc = of_property_read_u32(child_node, "qcom,wf-vmax-mv", &tmp);
		if (rc != 0)
			printk("%s:  Read qcom,wf-vmax-mv failed !\n", __func__);
		else
			effect->vmax_mv = tmp;

		printk("%s: ---%d effect->vmax_mv =%d \n", __func__, __LINE__,
		       effect->vmax_mv);
		rc = of_property_count_elems_of_size(child_node,
						     "qcom,wf-pattern",
						     sizeof(u8));
		if (rc < 0) {
			printk("%s: Count qcom,wf-pattern property failed !\n",
			       __func__);
		} else if (rc == 0) {
			printk("%s: qcom,wf-pattern has no data\n", __func__);
		}

		effect->pattern_length = rc;
		effect->pattern = devm_kcalloc(aw8622x->dev,
					       effect->pattern_length,
					       sizeof(u8), GFP_KERNEL);

		rc = of_property_read_u8_array(child_node, "qcom,wf-pattern",
					       effect->pattern,
					       effect->pattern_length);
		if (rc < 0) {
			printk("%s: Read qcom,wf-pattern property failed !\n",
			       __func__);
		}
		//printk
		//    ("%s: %d  effect->pattern_length=%d  effect->pattern=%s \n",
		//     __func__, __LINE__, effect->pattern_length,
		//     effect->pattern);

		effect->play_rate_us = config->play_rate_us;
		rc = of_property_read_u32(child_node, "qcom,wf-play-rate-us",
					  &tmp);
		if (rc < 0)
			printk("%s: Read qcom,wf-play-rate-us failed !\n",
			       __func__);
		else
			effect->play_rate_us = tmp;
		printk("%s: ---%d effect->play_rate_us=%d \n", __func__,
		       __LINE__, effect->play_rate_us);

		rc = of_property_read_u32(child_node, "qcom,wf-repeat-count",
					  &tmp);
		if (rc < 0) {
			printk("%s: Read  qcom,wf-repeat-count failed !\n",
			       __func__);
		} else {
			for (j = 0; j < ARRAY_SIZE(wf_repeat); j++)
				if (tmp <= wf_repeat[j])
					break;

			effect->wf_repeat_n = j;
		}
				effect->lra_auto_res_disable = of_property_read_bool(child_node,
								     "qcom,lra-auto-resonance-disable");

		tmp = of_property_count_elems_of_size(child_node,
						      "qcom,wf-brake-pattern",
						      sizeof(u8));
		if (tmp <= 0)
			continue;

		if (tmp > HAP_BRAKE_PATTERN_MAX) {
			printk
			    ("%s: wf-brake-pattern shouldn't be more than %d bytes\n",
			     __func__, HAP_BRAKE_PATTERN_MAX);
		}

		rc = of_property_read_u8_array(child_node,
					       "qcom,wf-brake-pattern",
					       effect->brake, tmp);
		if (rc < 0) {
			printk("%s: Failed to get wf-brake-pattern !\n",
			       __func__);
		}

		effect->brake_pattern_length = tmp;
	}
	return 0;
}


static void aw8622x_haptic_upload_lra(struct aw8622x *aw8622x,
				      unsigned int flag)
{
	switch (flag) {
	case AW8622X_WRITE_ZERO:
		aw_dev_err(aw8622x->dev, "%s write zero to trim_lra!\n",
			    __func__);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_TRIMCFG3,
				       AW8622X_BIT_TRIMCFG3_TRIM_LRA_MASK,
				       0x00);
		break;
	case AW8622X_F0_CALI:
		aw_dev_err(aw8622x->dev, "%s write f0_cali_data to trim_lra = 0x%02X\n",
			    __func__, aw8622x->f0_cali_data);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_TRIMCFG3,
				       AW8622X_BIT_TRIMCFG3_TRIM_LRA_MASK,
				       (char)aw8622x->f0_cali_data);
		break;
	case AW8622X_OSC_CALI:
		aw_dev_err(aw8622x->dev, "%s write osc_cali_data to trim_lra = 0x%02X\n",
			    __func__, aw8622x->osc_cali_data);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_TRIMCFG3,
				       AW8622X_BIT_TRIMCFG3_TRIM_LRA_MASK,
				       (char)aw8622x->osc_cali_data);
		break;
	default:
		break;
	}
}



/*****************************************************
 *
 * sram size, normally 3k(2k fifo, 1k ram)
 *
 *****************************************************/
static int aw8622x_sram_size(struct aw8622x *aw8622x, int size_flag)
{
	if (size_flag == AW8622X_HAPTIC_SRAM_2K) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RTPCFG1,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_2K_MASK,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_2K_EN);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RTPCFG1,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_1K_MASK,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_1K_DIS);
	} else if (size_flag == AW8622X_HAPTIC_SRAM_1K) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RTPCFG1,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_2K_MASK,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_2K_DIS);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RTPCFG1,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_1K_MASK,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_1K_EN);
	} else if (size_flag == AW8622X_HAPTIC_SRAM_3K) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RTPCFG1,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_1K_MASK,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_1K_EN);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RTPCFG1,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_2K_MASK,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_2K_EN);
	}
	return 0;
}

static int aw8622x_haptic_stop(struct aw8622x *aw8622x)
{
	unsigned char cnt = 40;
	unsigned char reg_val = 0;
	bool force_flag = true;

	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);
	aw8622x->play_mode = AW8622X_HAPTIC_STANDBY_MODE;
	aw8622x_i2c_write(aw8622x, AW8622X_REG_PLAYCFG4, 0x02);
	while (cnt) {
		aw8622x_i2c_read(aw8622x, AW8622X_REG_GLBRD5, &reg_val);
		if ((reg_val & 0x0f) == 0x00
		    || (reg_val & 0x0f) == 0x0A) {
			cnt = 0;
			force_flag = false;
			aw_dev_err(aw8622x->dev, "%s entered standby! glb_state=0x%02X\n",
				    __func__, reg_val);
		} else {
			cnt--;
			aw_dev_err(aw8622x->dev, "%s wait for standby, glb_state=0x%02X\n",
			     __func__, reg_val);
		}
		usleep_range(2000, 2500);
	}

	if (force_flag) {
		aw_dev_err(aw8622x->dev, "%s force to enter standby mode!\n",
			   __func__);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL2,
				       AW8622X_BIT_SYSCTRL2_STANDBY_MASK,
				       AW8622X_BIT_SYSCTRL2_STANDBY_ON);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL2,
				       AW8622X_BIT_SYSCTRL2_STANDBY_MASK,
				       AW8622X_BIT_SYSCTRL2_STANDBY_OFF);
	}
	return 0;
}

static void aw8622x_haptic_raminit(struct aw8622x *aw8622x, bool flag)
{
	if (flag) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL1,
				       AW8622X_BIT_SYSCTRL1_RAMINIT_MASK,
				       AW8622X_BIT_SYSCTRL1_RAMINIT_ON);
	} else {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL1,
				       AW8622X_BIT_SYSCTRL1_RAMINIT_MASK,
				       AW8622X_BIT_SYSCTRL1_RAMINIT_OFF);
	}
}

static int aw8622x_haptic_get_vbat(struct aw8622x *aw8622x)
{
	unsigned char reg_val = 0;
	unsigned int vbat_code = 0;
	/*unsigned int cont = 2000;*/

	aw8622x_haptic_stop(aw8622x);
	aw8622x_haptic_raminit(aw8622x, true);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_DETCFG2,
			       AW8622X_BIT_DETCFG2_VBAT_GO_MASK,
			       AW8622X_BIT_DETCFG2_VABT_GO_ON);
	usleep_range(20000, 25000);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_DET_VBAT, &reg_val);
	vbat_code = (vbat_code | reg_val) << 2;
	aw8622x_i2c_read(aw8622x, AW8622X_REG_DET_LO, &reg_val);
	vbat_code = vbat_code | ((reg_val & 0x30) >> 4);
	aw8622x->vbat = 6100 * vbat_code / 1024;
	if (aw8622x->vbat > AW8622X_VBAT_MAX) {
		aw8622x->vbat = AW8622X_VBAT_MAX;
		aw_dev_err(aw8622x->dev, "%s vbat max limit = %dmV\n",
			__func__, aw8622x->vbat);
	}
	if (aw8622x->vbat < AW8622X_VBAT_MIN) {
		aw8622x->vbat = AW8622X_VBAT_MIN;
		aw_dev_err(aw8622x->dev, "%s vbat min limit = %dmV\n",
			    __func__, aw8622x->vbat);
	}
	aw_dev_err(aw8622x->dev, "%s aw8622x->vbat=%dmV, vbat_code=0x%02X\n",
		    __func__, aw8622x->vbat, vbat_code);
	aw8622x_haptic_raminit(aw8622x, false);
	return 0;
}

/*****************************************************
 *
 * rtp brk
 *
 *****************************************************/
/*
*static int aw8622x_rtp_brake_set(struct aw8622x *aw8622x) {
*     aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG1,
*			AW8622X_BIT_CONTCFG1_MBRK_MASK,
*			AW8622X_BIT_CONTCFG1_MBRK_ENABLE);
*
*     aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL7,
*			AW8622X_BIT_SYSCTRL7_D2S_GAIN_MASK,
*			0x05);
*     return 0;
*}
*/

static void aw8622x_interrupt_clear(struct aw8622x *aw8622x)
{
	unsigned char reg_val = 0;

	aw_dev_dbg(aw8622x->dev, "%s enter\n", __func__);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_SYSINT, &reg_val);
	aw_dev_dbg(aw8622x->dev, "%s: reg SYSINT=0x%02X\n", __func__, reg_val);
}

//Daniel 20210716 modify start
static int aw8622x_haptic_set_gain(struct aw8622x *aw8622x, unsigned char gain)
{
	unsigned char comp_gain = 0;
	if (aw8622x->ram_vbat_compensate == AW8622X_HAPTIC_RAM_VBAT_COMP_ENABLE)
	{
		aw8622x_haptic_get_vbat(aw8622x);
		pr_debug("%s: ref %d vbat %d ", __func__, AW8622X_VBAT_REFER,
				aw8622x->vbat);
		comp_gain =
			    aw8622x->gain * AW8622X_VBAT_REFER / aw8622x->vbat;
			if (comp_gain >
			    (128 * AW8622X_VBAT_REFER / AW8622X_VBAT_MIN)) {
				comp_gain =
				    128 * AW8622X_VBAT_REFER / AW8622X_VBAT_MIN;
				aw_dev_dbg(aw8622x->dev, "%s gain limit=%d\n",
					   __func__, comp_gain);
			}
		pr_info("%s: enable vbat comp, level = %x comp level = %x", __func__,
			   gain, comp_gain);
		aw8622x_i2c_write(aw8622x, AW8622X_REG_PLAYCFG2, comp_gain);
	} else {
		pr_debug("%s: disable compsensation, vbat=%d, vbat_min=%d, vbat_ref=%d",
		__func__, aw8622x->vbat, AW8622X_VBAT_MIN, AW8622X_VBAT_REFER);
		aw8622x_i2c_write(aw8622x, AW8622X_REG_PLAYCFG2, gain);
	}
	return 0;
}


static int aw8622x_haptic_ram_vbat_compensate(struct aw8622x *aw8622x,
					      bool flag)
{
	if (flag)
		aw8622x->ram_vbat_compensate = AW8622X_HAPTIC_RAM_VBAT_COMP_ENABLE;
	else
		aw8622x->ram_vbat_compensate = AW8622X_HAPTIC_RAM_VBAT_COMP_DISABLE;
	return 0;

}//Daniel 20210716 modify end

static int aw8622x_haptic_play_mode(struct aw8622x *aw8622x,
				    unsigned char play_mode)
{
	aw_dev_dbg(aw8622x->dev, "%s enter\n", __func__);

	switch (play_mode) {
	case AW8622X_HAPTIC_STANDBY_MODE:
		aw_dev_err(aw8622x->dev, "%s: enter standby mode\n", __func__);
		aw8622x->play_mode = AW8622X_HAPTIC_STANDBY_MODE;
		aw8622x_haptic_stop(aw8622x);
		break;
	case AW8622X_HAPTIC_RAM_MODE:
		aw_dev_err(aw8622x->dev, "%s: enter ram mode\n", __func__);
		aw8622x->play_mode = AW8622X_HAPTIC_RAM_MODE;
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_RAM);
		break;
	case AW8622X_HAPTIC_RAM_LOOP_MODE:
		aw_dev_err(aw8622x->dev, "%s: enter ram loop mode\n",
			    __func__);
		aw8622x->play_mode = AW8622X_HAPTIC_RAM_LOOP_MODE;
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_RAM);
		break;
	case AW8622X_HAPTIC_RTP_MODE:
		aw_dev_err(aw8622x->dev, "%s: enter rtp mode\n", __func__);
		aw8622x->play_mode = AW8622X_HAPTIC_RTP_MODE;
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_RTP);
		break;
	case AW8622X_HAPTIC_TRIG_MODE:
		aw_dev_err(aw8622x->dev, "%s: enter trig mode\n", __func__);
		aw8622x->play_mode = AW8622X_HAPTIC_TRIG_MODE;
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_RAM);
		break;
	case AW8622X_HAPTIC_CONT_MODE:
		aw_dev_err(aw8622x->dev, "%s: enter cont mode\n", __func__);
		aw8622x->play_mode = AW8622X_HAPTIC_CONT_MODE;
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_CONT);
		break;
	default:
		aw_dev_err(aw8622x->dev, "%s: play mode %d error",
			   __func__, play_mode);
		break;
	}
	return 0;
}

static int aw8622x_haptic_play_go(struct aw8622x *aw8622x, bool flag)
{
	unsigned char reg_val = 0;

	aw_dev_dbg(aw8622x->dev, "%s enter\n", __func__);
	if (flag == true) {
		aw8622x_i2c_write(aw8622x, AW8622X_REG_PLAYCFG4, 0x01);
		mdelay(2);
	} else {
		aw8622x_i2c_write(aw8622x, AW8622X_REG_PLAYCFG4, 0x02);
	}

	aw8622x_i2c_read(aw8622x, AW8622X_REG_GLBRD5, &reg_val);
	return 0;
}

static int aw8622x_haptic_set_wav_seq(struct aw8622x *aw8622x,
				      unsigned char wav, unsigned char seq)
{
	aw_dev_info(aw8622x->dev, "%s enter!\n", __func__);
	aw8622x_i2c_write(aw8622x, AW8622X_REG_WAVCFG1 + wav, seq);
	return 0;
}

static int aw8622x_haptic_set_wav_loop(struct aw8622x *aw8622x,
				       unsigned char wav, unsigned char loop)
{
	unsigned char tmp = 0;

	aw_dev_info(aw8622x->dev, "%s enter!\n", __func__);
	if (wav % 2) {
		tmp = loop << 0;
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_WAVCFG9 + (wav / 2),
				       AW8622X_BIT_WAVLOOP_SEQ_EVEN_MASK, tmp);
	} else {
		tmp = loop << 4;
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_WAVCFG9 + (wav / 2),
				       AW8622X_BIT_WAVLOOP_SEQ_ODD_MASK, tmp);
	}
	return 0;
}

/*****************************************************
 *
 * haptic f0 cali
 *
 *****************************************************/
static int aw8622x_haptic_read_lra_f0(struct aw8622x *aw8622x)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);
	/* F_LRA_F0_H */
	ret = aw8622x_i2c_read(aw8622x, AW8622X_REG_CONTRD14, &reg_val);
	f0_reg = (f0_reg | reg_val) << 8;
	/* F_LRA_F0_L */
	ret = aw8622x_i2c_read(aw8622x, AW8622X_REG_CONTRD15, &reg_val);
	f0_reg |= (reg_val << 0);
	if (!f0_reg) {
		aw_dev_err(aw8622x->dev, "%s didn't get lra f0 because f0_reg value is 0!\n",
			   __func__);
		aw8622x->f0 = aw8622x->dts_info.f0_ref;
		return -1;
	} else {
		f0_tmp = 384000 * 10 / f0_reg;
		aw8622x->f0 = (unsigned int)f0_tmp;
		aw_dev_err(aw8622x->dev, "%s lra_f0=%d\n", __func__,
			    aw8622x->f0);
	}

	return 0;
}

static int aw8622x_haptic_read_cont_f0(struct aw8622x *aw8622x)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);
	ret = aw8622x_i2c_read(aw8622x, AW8622X_REG_CONTRD16, &reg_val);
	f0_reg = (f0_reg | reg_val) << 8;
	ret = aw8622x_i2c_read(aw8622x, AW8622X_REG_CONTRD17, &reg_val);
	f0_reg |= (reg_val << 0);
	if (!f0_reg) {
		aw_dev_err(aw8622x->dev, "%s didn't get cont f0 because f0_reg value is 0!\n",
			   __func__);
		aw8622x->cont_f0 = aw8622x->dts_info.f0_ref;
		return -1;
	} else {
		f0_tmp = 384000 * 10 / f0_reg;
		aw8622x->cont_f0 = (unsigned int)f0_tmp;
		aw_dev_err(aw8622x->dev, "%s cont_f0=%d\n", __func__,
			    aw8622x->cont_f0);
	}
	return 0;
}

static int aw8622x_haptic_cont_get_f0(struct aw8622x *aw8622x)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int cnt = 200;
	bool get_f0_flag = false;
	unsigned char brk_en_temp = 0;

	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);
	aw8622x->f0 = aw8622x->dts_info.f0_ref;
	/* enter standby mode */
	aw8622x_haptic_stop(aw8622x);
	/* f0 calibrate work mode */
	aw8622x_haptic_play_mode(aw8622x, AW8622X_HAPTIC_CONT_MODE);
	/* enable f0 detect */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG1,
			       AW8622X_BIT_CONTCFG1_EN_F0_DET_MASK,
			       AW8622X_BIT_CONTCFG1_F0_DET_ENABLE);
	/* cont config */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG6,
			       AW8622X_BIT_CONTCFG6_TRACK_EN_MASK,
			       AW8622X_BIT_CONTCFG6_TRACK_ENABLE);
	/* enable auto brake */
	aw8622x_i2c_read(aw8622x, AW8622X_REG_PLAYCFG3, &reg_val);
	brk_en_temp = 0x04 & reg_val;
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
			       AW8622X_BIT_PLAYCFG3_BRK_EN_MASK,
			       AW8622X_BIT_PLAYCFG3_BRK_ENABLE);
	/* f0 driver level */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG6,
			       AW8622X_BIT_CONTCFG6_DRV1_LVL_MASK,
			       aw8622x->dts_info.cont_drv1_lvl_dt);
	aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG7,
			  aw8622x->dts_info.cont_drv2_lvl_dt);
	/* DRV1_TIME */
	aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG8,
			  aw8622x->dts_info.cont_drv1_time_dt);
	/* DRV2_TIME */
	aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG9,
			  aw8622x->dts_info.cont_drv2_time_dt);
	/* TRACK_MARGIN */
	if (!aw8622x->dts_info.cont_track_margin) {
		aw_dev_err(aw8622x->dev, "%s aw8622x->dts_info.cont_track_margin = 0!\n",
			   __func__);
	} else {
		aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG11,
				  (unsigned char)aw8622x->dts_info.
				  cont_track_margin);
	}
	/* DRV_WIDTH */
	/*
	 * aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG3,
	 *                aw8622x->dts_info.cont_drv_width);
	 */
	/* cont play go */
	aw8622x_haptic_play_go(aw8622x, true);
	/* 300ms */
	while (cnt) {
		aw8622x_i2c_read(aw8622x, AW8622X_REG_GLBRD5, &reg_val);
		if ((reg_val & 0x0f) == 0x00) {
			cnt = 0;
			get_f0_flag = true;
			aw_dev_err(aw8622x->dev, "%s entered standby mode! glb_state=0x%02X\n",
				    __func__, reg_val);
		} else {
			cnt--;
			aw_dev_err(aw8622x->dev, "%s waitting for standby, glb_state=0x%02X\n",
				    __func__, reg_val);
		}
		usleep_range(10000, 10500);
	}
	if (get_f0_flag) {
		aw8622x_haptic_read_lra_f0(aw8622x);
		aw8622x_haptic_read_cont_f0(aw8622x);
	} else {
		aw_dev_err(aw8622x->dev, "%s enter standby mode failed, stop reading f0!\n",
			   __func__);
	}
	/* restore default config */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG1,
			       AW8622X_BIT_CONTCFG1_EN_F0_DET_MASK,
			       AW8622X_BIT_CONTCFG1_F0_DET_DISABLE);
	/* recover auto break config */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
			       AW8622X_BIT_PLAYCFG3_BRK_EN_MASK,
			       brk_en_temp);
	return ret;
}

static int aw8622x_haptic_rtp_init(struct aw8622x *aw8622x)
{
	unsigned int buf_len = 0;
	unsigned char glb_state_val = 0;

	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);
	cpu_latency_qos_add_request(&aw8622x_pm_qos_req_vb, AW8622X_PM_QOS_VALUE_VB);
	aw8622x->rtp_cnt = 0;
	mutex_lock(&aw8622x->rtp_lock);
	while ((!aw8622x_haptic_rtp_get_fifo_afs(aw8622x))
	       && (aw8622x->play_mode == AW8622X_HAPTIC_RTP_MODE)
		   &&  !atomic_read(&aw8622x->exit_in_rtp_loop)) {
		aw_dev_err(aw8622x->dev, "%s rtp cnt = %d\n", __func__,
			    aw8622x->rtp_cnt);
		if (!aw8622x->rtp_container) {
			aw_dev_err(aw8622x->dev, "%s:aw8622x->rtp_container is null, break!\n",
				    __func__);
			break;
		}
		if (aw8622x->rtp_cnt < (aw8622x->ram.base_addr)) {
			if ((aw8622x->rtp_container->len - aw8622x->rtp_cnt) <
			    (aw8622x->ram.base_addr)) {
				buf_len = aw8622x->rtp_container->len - aw8622x->rtp_cnt;
			} else {
				buf_len = aw8622x->ram.base_addr;
			}
		} else if ((aw8622x->rtp_container->len - aw8622x->rtp_cnt) <
			   (aw8622x->ram.base_addr >> 2)) {
			buf_len = aw8622x->rtp_container->len - aw8622x->rtp_cnt;
		} else {
			buf_len = aw8622x->ram.base_addr >> 2;
		}
		aw_dev_err(aw8622x->dev, "%s buf_len = %d\n", __func__,
			    buf_len);
		aw8622x_i2c_writes(aw8622x, AW8622X_REG_RTPDATA,
				   &aw8622x->rtp_container->data[aw8622x->rtp_cnt],
				   buf_len);
		aw8622x->rtp_cnt += buf_len;
		aw8622x_i2c_read(aw8622x, AW8622X_REG_GLBRD5, &glb_state_val);
		if ((aw8622x->rtp_cnt == aw8622x->rtp_container->len)
		    || ((glb_state_val & 0x0f) == 0x00)) {
			if (aw8622x->rtp_cnt == aw8622x->rtp_container->len)
				aw_dev_err(aw8622x->dev,
					"%s: rtp load completely! glb_state_val=0x%02x aw8622x->rtp_cnt=%d\n",
					__func__, glb_state_val,
					aw8622x->rtp_cnt);
			else
				aw_dev_err(aw8622x->dev,
					"%s rtp load failed!! glb_state_val=0x%02x aw8622x->rtp_cnt=%d\n",
					__func__, glb_state_val,
					aw8622x->rtp_cnt);
			aw8622x->rtp_cnt = 0;
			cpu_latency_qos_remove_request(&aw8622x_pm_qos_req_vb);
			mutex_unlock(&aw8622x->rtp_lock);
			return 0;
		}
	}
	mutex_unlock(&aw8622x->rtp_lock);

	if (aw8622x->play_mode == AW8622X_HAPTIC_RTP_MODE
	    && !atomic_read(&aw8622x->exit_in_rtp_loop))
		aw8622x_haptic_set_rtp_aei(aw8622x, true);

	aw_dev_err(aw8622x->dev, "%s exit\n", __func__);
	cpu_latency_qos_remove_request(&aw8622x_pm_qos_req_vb);
	return 0;
}

static unsigned char aw8622x_haptic_osc_read_status(struct aw8622x *aw8622x)
{
	unsigned char reg_val = 0;

	aw8622x_i2c_read(aw8622x, AW8622X_REG_SYSST2, &reg_val);
	return reg_val;
}

static int aw8622x_haptic_set_repeat_wav_seq(struct aw8622x *aw8622x,
					     unsigned char seq)
{
	aw_dev_info(aw8622x->dev, "%s repeat wav seq %d\n", __func__, seq);
	aw8622x_haptic_set_wav_seq(aw8622x, 0x00, seq);
	aw8622x_haptic_set_wav_loop(aw8622x, 0x00,
				    AW8622X_BIT_WAVLOOP_INIFINITELY);
	return 0;
}

static int aw8622x_haptic_set_pwm(struct aw8622x *aw8622x, unsigned char mode)
{
	switch (mode) {
	case AW8622X_PWM_48K:
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL2,
				       AW8622X_BIT_SYSCTRL2_WAVDAT_MODE_MASK,
				       AW8622X_BIT_SYSCTRL2_RATE_48K);
		break;
	case AW8622X_PWM_24K:
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL2,
				       AW8622X_BIT_SYSCTRL2_WAVDAT_MODE_MASK,
				       AW8622X_BIT_SYSCTRL2_RATE_24K);
		break;
	case AW8622X_PWM_12K:
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL2,
				       AW8622X_BIT_SYSCTRL2_WAVDAT_MODE_MASK,
				       AW8622X_BIT_SYSCTRL2_RATE_12K);
		break;
	default:
		break;
	}
	return 0;
}

static int16_t aw8622x_haptic_effect_strength(struct aw8622x *aw8622x)
{
	pr_debug("%s: enter\n", __func__);
	pr_debug("%s: aw8622x->play.vmax_mv =0x%x\n", __func__,
		 aw8622x->play.vmax_mv);
#if 0
	switch (aw8622x->play.vmax_mv) {
	case AW8622X_LIGHT_MAGNITUDE:
		aw8622x->level = 0x30;
		break;
	case AW8622X_MEDIUM_MAGNITUDE:
		aw8622x->level = 0x50;
		break;
	case AW8622X_STRONG_MAGNITUDE:
		aw8622x->level = 0x80;
		break;
	default:
		break;
	}
#else
	if (aw8622x->play.vmax_mv >= 0x7FFF)
		aw8622x->level = 0x80;	/*128 */
	else if (aw8622x->play.vmax_mv <= 0x3FFF)
		aw8622x->level = 0x1E;	/*30 */
	else
		aw8622x->level = (aw8622x->play.vmax_mv - 16383) / 128;
	if (aw8622x->level < 0x1E)
		aw8622x->level = 0x1E;	/*30 */
#endif

	pr_info("%s: aw8622x->level =0x%x\n", __func__, aw8622x->level);
	return 0;
}

static void aw8622x_rtp_work_routine(struct work_struct *work)
{
	const struct firmware *rtp_file;
	int ret = -1;
	unsigned int cnt = 200;
	unsigned char reg_val = 0;
	bool rtp_work_flag = false;
	struct aw8622x *aw8622x = container_of(work, struct aw8622x, rtp_work);

	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);

	if ((aw8622x->effect_id < aw8622x->dts_info.effect_id_boundary) &&
	    (aw8622x->effect_id > aw8622x->dts_info.effect_max))
		return;
	pr_info("%s: effect_id =%d state = %d\n", __func__, aw8622x->effect_id,
		aw8622x->state);
	mutex_lock(&aw8622x->lock);
	aw8622x_haptic_upload_lra(aw8622x, AW8622X_OSC_CALI);
	aw8622x_haptic_set_rtp_aei(aw8622x, false);
	aw8622x_interrupt_clear(aw8622x);
	//wait for irq to exit
	atomic_set(&aw8622x->exit_in_rtp_loop, 1);
	while (atomic_read(&aw8622x->is_in_rtp_loop)) {
		pr_info("%s:  goint to waiting irq exit\n", __func__);
		ret =
		    wait_event_interruptible(aw8622x->wait_q,
					     atomic_read(&aw8622x->
							 is_in_rtp_loop) == 0);
		pr_info("%s:  wakeup \n", __func__);
		if (ret == -ERESTARTSYS) {
			atomic_set(&aw8622x->exit_in_rtp_loop, 0);
			wake_up_interruptible(&aw8622x->stop_wait_q);
			mutex_unlock(&aw8622x->lock);
			pr_err("%s: wake up by signal return erro\n", __func__);
			return;
		}
	}
	atomic_set(&aw8622x->exit_in_rtp_loop, 0);
	wake_up_interruptible(&aw8622x->stop_wait_q);
	aw8622x_haptic_stop(aw8622x);

	if (aw8622x->state) {
		pm_stay_awake(aw8622x->dev);
		aw8622x->wk_lock_flag = 1;
		aw8622x_haptic_effect_strength(aw8622x);
		aw8622x->rtp_file_num = aw8622x->effect_id - 20;
		pr_info("%s:   aw8622x->rtp_file_num =%d\n", __func__,
			aw8622x->rtp_file_num);
		if (aw8622x->rtp_file_num < 0)
			aw8622x->rtp_file_num = 0;
		if (aw8622x->rtp_file_num >
		    ((sizeof(aw8622x_rtp_name) / AW8622X_RTP_NAME_MAX) - 1))
			aw8622x->rtp_file_num =
			    (sizeof(aw8622x_rtp_name) / AW8622X_RTP_NAME_MAX) - 1;


		/* fw loaded */
		ret = request_firmware(&rtp_file,
				       aw8622x_rtp_name[aw8622x->rtp_file_num],
				       aw8622x->dev);
		if (ret < 0) {
			aw_dev_err(aw8622x->dev, "%s: failed to read %s\n", __func__,
				   aw8622x_rtp_name[aw8622x->rtp_file_num]);
			if (aw8622x->wk_lock_flag == 1) {
					pm_relax(aw8622x->dev);
					aw8622x->wk_lock_flag = 0;
				}
			mutex_unlock(&aw8622x->lock);
			return;
		}
		aw8622x->rtp_init = 0;
		vfree(aw8622x->rtp_container);
		aw8622x->rtp_container = vmalloc(rtp_file->size + sizeof(int));
		if (!aw8622x->rtp_container) {
			release_firmware(rtp_file);
			aw_dev_err(aw8622x->dev, "%s: error allocating memory\n",
				   __func__);
		    if (aw8622x->wk_lock_flag == 1) {
					pm_relax(aw8622x->dev);
					aw8622x->wk_lock_flag = 0;
				}
			mutex_unlock(&aw8622x->lock);
			return;
		}
		aw8622x->rtp_container->len = rtp_file->size;
		aw_dev_err(aw8622x->dev, "%s: rtp file:[%s] size = %dbytes\n",
			    __func__, aw8622x_rtp_name[aw8622x->rtp_file_num],
			    aw8622x->rtp_container->len);
		memcpy(aw8622x->rtp_container->data, rtp_file->data, rtp_file->size);
		release_firmware(rtp_file);
		aw8622x->rtp_init = 1;
		aw8622x_haptic_upload_lra(aw8622x, AW8622X_OSC_CALI);
		aw8622x_haptic_set_pwm(aw8622x, AW8622X_PWM_24K);//Daniel 20210601
		/* gain */
		aw8622x_haptic_ram_vbat_compensate(aw8622x, false);
		aw8622x_haptic_set_gain(aw8622x, aw8622x->level);//Daniel 20210716 modify
		/* rtp mode config */
		aw8622x_haptic_play_mode(aw8622x, AW8622X_HAPTIC_RTP_MODE);
		/* haptic go */
		aw8622x_haptic_play_go(aw8622x, true);
		mutex_unlock(&aw8622x->lock);
		usleep_range(2000, 2500);
		while (cnt) {
			aw8622x_i2c_read(aw8622x, AW8622X_REG_GLBRD5, &reg_val);
			if ((reg_val & 0x0f) == 0x08) {
				cnt = 0;
				rtp_work_flag = true;
				aw_dev_err(aw8622x->dev, "%s RTP_GO! glb_state=0x08\n",
					    __func__);
			} else {
				cnt--;
				aw_dev_dbg(aw8622x->dev, "%s wait for RTP_GO, glb_state=0x%02X\n",
					   __func__, reg_val);
			}
			usleep_range(2000, 2500);
		}
		if (rtp_work_flag) {
			aw8622x_haptic_rtp_init(aw8622x);
		} else {
			/* enter standby mode */
			aw8622x_haptic_stop(aw8622x);
			aw_dev_err(aw8622x->dev, "%s failed to enter RTP_GO status!\n",
				   __func__);
		}
	} else {
		if (aw8622x->wk_lock_flag == 1) {
			pm_relax(aw8622x->dev);
			aw8622x->wk_lock_flag = 0;
		}
		aw8622x->rtp_cnt = 0;
		aw8622x->rtp_init = 0;
                mutex_unlock(&aw8622x->lock);
	}

}

static int aw8622x_rtp_osc_calibration(struct aw8622x *aw8622x)
{
	const struct firmware *rtp_file;
	int ret = -1;
	unsigned int buf_len = 0;
	unsigned char osc_int_state = 0;

	aw8622x->rtp_cnt = 0;
	aw8622x->timeval_flags = 1;

	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);
	/* fw loaded */
	ret = request_firmware(&rtp_file, aw8622x_rtp_name[0], aw8622x->dev);
	if (ret < 0) {
		aw_dev_err(aw8622x->dev, "%s: failed to read %s\n", __func__,
			   aw8622x_rtp_name[0]);
		return ret;
	}
	/*awinic add stop,for irq interrupt during calibrate */
	aw8622x_haptic_stop(aw8622x);
	aw8622x->rtp_init = 0;
	mutex_lock(&aw8622x->rtp_lock);
	vfree(aw8622x->rtp_container);
	aw8622x->rtp_container = vmalloc(rtp_file->size + sizeof(int));
	if (!aw8622x->rtp_container) {
		release_firmware(rtp_file);
		mutex_unlock(&aw8622x->rtp_lock);
		aw_dev_err(aw8622x->dev, "%s: error allocating memory\n",
			   __func__);
		return -ENOMEM;
	}
	aw8622x->rtp_container->len = rtp_file->size;
	aw8622x->rtp_len = rtp_file->size;
	aw_dev_err(aw8622x->dev, "%s: rtp file:[%s] size = %dbytes\n",
		    __func__, aw8622x_rtp_name[0], aw8622x->rtp_container->len);

	memcpy(aw8622x->rtp_container->data, rtp_file->data, rtp_file->size);
	release_firmware(rtp_file);
	mutex_unlock(&aw8622x->rtp_lock);
	/* gain */
	aw8622x_haptic_ram_vbat_compensate(aw8622x, false);
	/* rtp mode config */
	aw8622x_haptic_play_mode(aw8622x, AW8622X_HAPTIC_RTP_MODE);

	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL7,
			       AW8622X_BIT_SYSCTRL7_INT_MODE_MASK,
			       AW8622X_BIT_SYSCTRL7_INT_MODE_EDGE);
	disable_irq(gpio_to_irq(aw8622x->irq_gpio));
	/* haptic go */
	aw8622x_haptic_play_go(aw8622x, true);
	/* require latency of CPU & DMA not more then PM_QOS_VALUE_VB us */
	cpu_latency_qos_add_request(&aw8622x_pm_qos_req_vb, AW8622X_PM_QOS_VALUE_VB);
	while (1) {
		if (!aw8622x_haptic_rtp_get_fifo_afs(aw8622x)) {
			mutex_lock(&aw8622x->rtp_lock);
			if ((aw8622x->rtp_container->len - aw8622x->rtp_cnt) <
			    (aw8622x->ram.base_addr >> 2))
				buf_len = aw8622x->rtp_container->len - aw8622x->rtp_cnt;
			else
				buf_len = (aw8622x->ram.base_addr >> 2);

			if (aw8622x->rtp_cnt != aw8622x->rtp_container->len) {
				if (aw8622x->timeval_flags == 1) {
					ktime_get_real_ts64(&aw8622x->start);
					aw8622x->timeval_flags = 0;
				}
				aw8622x->rtp_update_flag =
				    aw8622x_i2c_writes(aw8622x,
							AW8622X_REG_RTPDATA,
							&aw8622x->rtp_container->data
							[aw8622x->rtp_cnt],
							buf_len);
				aw8622x->rtp_cnt += buf_len;
			}
			mutex_unlock(&aw8622x->rtp_lock);
		}
		osc_int_state = aw8622x_haptic_osc_read_status(aw8622x);
		if (osc_int_state & AW8622X_BIT_SYSST2_FF_EMPTY) {
			ktime_get_real_ts64(&aw8622x->end);
			pr_info
			    ("%s osc trim playback done aw8622x->rtp_cnt= %d\n",
			     __func__, aw8622x->rtp_cnt);
			break;
		}
		ktime_get_real_ts64(&aw8622x->end);
		aw8622x->microsecond =
		    (aw8622x->end.tv_sec - aw8622x->start.tv_sec) * 1000000 +
		    (aw8622x->end.tv_nsec - aw8622x->start.tv_nsec) / 1000000;
		if (aw8622x->microsecond > AW8622X_OSC_CALI_MAX_LENGTH) {
			aw_dev_err(aw8622x->dev,
				"%s osc trim time out! aw8622x->rtp_cnt %d osc_int_state %02x\n",
				__func__, aw8622x->rtp_cnt, osc_int_state);
			break;
		}
	}
	cpu_latency_qos_remove_request(&aw8622x_pm_qos_req_vb);
	enable_irq(gpio_to_irq(aw8622x->irq_gpio));

	aw8622x->microsecond =
	    (aw8622x->end.tv_sec - aw8622x->start.tv_sec) * 1000000 +
	    (aw8622x->end.tv_nsec - aw8622x->start.tv_nsec)  / 1000000;
	/*calibration osc */
	aw_dev_err(aw8622x->dev, "%s awinic_microsecond: %ld\n", __func__,
		    aw8622x->microsecond);
	aw_dev_err(aw8622x->dev, "%s exit\n", __func__);
	return 0;
}

static int aw8622x_osc_trim_calculation(unsigned long int theory_time,
					unsigned long int real_time)
{
	unsigned int real_code = 0;
	unsigned int lra_code = 0;
	unsigned int DFT_LRA_TRIM_CODE = 0;
	/*0.1 percent below no need to calibrate */
	unsigned int osc_cali_threshold = 10;

	pr_err("%s enter\n", __func__);
	if (theory_time == real_time) {
		pr_err("%s theory_time == real_time: %ld, no need to calibrate!\n",
			__func__, real_time);
		return 0;
	} else if (theory_time < real_time) {
		if ((real_time - theory_time) > (theory_time / 50)) {
			pr_err("%s (real_time - theory_time) > (theory_time/50), can't calibrate!\n",
				__func__);
			return DFT_LRA_TRIM_CODE;
		}

		if ((real_time - theory_time) <
		    (osc_cali_threshold * theory_time / 10000)) {
			pr_err("%s real_time: %ld, theory_time: %ld, no need to calibrate!\n",
				__func__,
				real_time, theory_time);
			return DFT_LRA_TRIM_CODE;
		}

		real_code = ((real_time - theory_time) * 4000) / theory_time;
		real_code = ((real_code % 10 < 5) ? 0 : 1) + real_code / 10;
		real_code = 32 + real_code;
	} else if (theory_time > real_time) {
		if ((theory_time - real_time) > (theory_time / 50)) {
			pr_err("%s (theory_time - real_time) > (theory_time / 50), can't calibrate!\n",
				__func__);
			return DFT_LRA_TRIM_CODE;
		}
		if ((theory_time - real_time) <
		    (osc_cali_threshold * theory_time / 10000)) {
			pr_err("%s real_time: %ld, theory_time: %ld, no need to calibrate!\n",
				__func__,
				real_time, theory_time);
			return DFT_LRA_TRIM_CODE;
		}
		real_code = ((theory_time - real_time) * 4000) / theory_time;
		real_code = ((real_code % 10 < 5) ? 0 : 1) + real_code / 10;
		real_code = 32 - real_code;
	}
	if (real_code > 31)
		lra_code = real_code - 32;
	else
		lra_code = real_code + 32;
	pr_err("%s real_time: %ld, theory_time: %ld\n", __func__, real_time,
		theory_time);
	pr_err("%s real_code: %02X, trim_lra: 0x%02X\n", __func__, real_code,
		lra_code);
	return lra_code;
}

static int aw8622x_haptic_get_lra_resistance(struct aw8622x *aw8622x)
{
	unsigned char reg_val = 0;
	unsigned char d2s_gain_temp = 0;
	unsigned int lra_code = 0;
	unsigned int lra = 0;

	mutex_lock(&aw8622x->lock);
	aw8622x_haptic_stop(aw8622x);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_SYSCTRL7, &reg_val);
	d2s_gain_temp = 0x07 & reg_val;
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL7,
			       AW8622X_BIT_SYSCTRL7_D2S_GAIN_MASK,
			       aw8622x->dts_info.d2s_gain);
	aw8622x_haptic_raminit(aw8622x, true);
	/* enter standby mode */
	aw8622x_haptic_stop(aw8622x);
	usleep_range(2000, 2500);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL2,
			       AW8622X_BIT_SYSCTRL2_STANDBY_MASK,
			       AW8622X_BIT_SYSCTRL2_STANDBY_OFF);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_DETCFG1,
			       AW8622X_BIT_DETCFG1_RL_OS_MASK,
			       AW8622X_BIT_DETCFG1_RL);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_DETCFG2,
			       AW8622X_BIT_DETCFG2_DIAG_GO_MASK,
			       AW8622X_BIT_DETCFG2_DIAG_GO_ON);
	usleep_range(30000, 35000);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_DET_RL, &reg_val);
	lra_code = (lra_code | reg_val) << 2;
	aw8622x_i2c_read(aw8622x, AW8622X_REG_DET_LO, &reg_val);
	lra_code = lra_code | (reg_val & 0x03);
	/* 2num */
	lra = (lra_code * 678 * 100) / (1024 * 10);
	/* Keep up with aw8624 driver */
	aw8622x->lra = lra * 10;
	aw8622x_haptic_raminit(aw8622x, false);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL7,
			       AW8622X_BIT_SYSCTRL7_D2S_GAIN_MASK,
			       d2s_gain_temp);
	mutex_unlock(&aw8622x->lock);
	return 0;
}

static int aw8622x_haptic_juge_RTP_is_going_on(struct aw8622x *aw8622x)
{
	unsigned char rtp_state = 0;
	unsigned char mode = 0;
	unsigned char glb_st = 0;
	aw8622x_i2c_read(aw8622x, AW8622X_REG_PLAYCFG3, &mode);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_GLBRD5, &glb_st);
	if ((mode & AW8622X_BIT_PLAYCFG3_PLAY_MODE_RTP) &&
		(glb_st == AW8622X_BIT_GLBRD5_STATE_RTP_GO)) {
		rtp_state = 1;
	}
	return rtp_state;
}

static int aw8622x_container_update(struct aw8622x *aw8622x,
				     struct aw8622x_container *aw8622x_cont)
{
	unsigned char reg_val = 0;
	unsigned int shift = 0;
	unsigned int temp = 0;
	int i = 0;
	int ret = 0;
#ifdef AW_CHECK_RAM_DATA
	unsigned short check_sum = 0;
#endif

	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);
	mutex_lock(&aw8622x->lock);
	aw8622x->ram.baseaddr_shift = 2;
	aw8622x->ram.ram_shift = 4;
	/* RAMINIT Enable */
	aw8622x_haptic_raminit(aw8622x, true);
	/* Enter standby mode */
	aw8622x_haptic_stop(aw8622x);
	/* base addr */
	shift = aw8622x->ram.baseaddr_shift;
	aw8622x->ram.base_addr =
	    (unsigned int)((aw8622x_cont->data[0 + shift] << 8) |
			   (aw8622x_cont->data[1 + shift]));

	/* default 3k SRAM */
	aw8622x_sram_size(aw8622x, AW8622X_HAPTIC_SRAM_3K);

	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RTPCFG1, /*ADDRH*/
			AW8622X_BIT_RTPCFG1_ADDRH_MASK,
			aw8622x_cont->data[0 + shift]);

	aw8622x_i2c_write(aw8622x, AW8622X_REG_RTPCFG2, /*ADDRL*/
			aw8622x_cont->data[1 + shift]);

	/* FIFO_AEH */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RTPCFG3,
			AW8622X_BIT_RTPCFG3_FIFO_AEH_MASK,
			(unsigned char)
				(((aw8622x->ram.base_addr >> 1) >> 4) & 0xF0));
	/* FIFO AEL */
	aw8622x_i2c_write(aw8622x, AW8622X_REG_RTPCFG4,
			(unsigned char)
				(((aw8622x->ram.base_addr >> 1) & 0x00FF)));
	/* FIFO_AFH */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RTPCFG3,
				AW8622X_BIT_RTPCFG3_FIFO_AFH_MASK,
				(unsigned char)(((aw8622x->ram.base_addr -
				(aw8622x->ram.base_addr >> 2)) >> 8) & 0x0F));
	/* FIFO_AFL */
	aw8622x_i2c_write(aw8622x, AW8622X_REG_RTPCFG5,
			(unsigned char)(((aw8622x->ram.base_addr -
				(aw8622x->ram.base_addr >> 2)) & 0x00FF)));
/*
*	unsigned int temp
*	HIGH<byte4 byte3 byte2 byte1>LOW
*	|_ _ _ _AF-12BIT_ _ _ _AE-12BIT|
*/
	aw8622x_i2c_read(aw8622x, AW8622X_REG_RTPCFG3, &reg_val);
	temp = ((reg_val & 0x0f) << 24) | ((reg_val & 0xf0) << 4);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_RTPCFG4, &reg_val);
	temp = temp | reg_val;
	aw_dev_err(aw8622x->dev, "%s: almost_empty_threshold = %d\n", __func__,
		    (unsigned short)temp);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_RTPCFG5, &reg_val);
	temp = temp | (reg_val << 16);
	aw_dev_err(aw8622x->dev, "%s: almost_full_threshold = %d\n", __func__,
		    temp >> 16);
	/* ram */
	shift = aw8622x->ram.baseaddr_shift;

	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RAMADDRH,
			       AW8622X_BIT_RAMADDRH_MASK,
			       aw8622x_cont->data[0 + shift]);
	aw8622x_i2c_write(aw8622x, AW8622X_REG_RAMADDRL,
			  aw8622x_cont->data[1 + shift]);
	shift = aw8622x->ram.ram_shift;
	aw_dev_err(aw8622x->dev, "%s: ram_len = %d\n", __func__,
		    aw8622x_cont->len - shift);
	for (i = shift; i < aw8622x_cont->len; i++) {
		aw8622x->ram_update_flag = aw8622x_i2c_write(aw8622x,
							AW8622X_REG_RAMDATA,
							aw8622x_cont->data
							[i]);
	}
#ifdef	AW_CHECK_RAM_DATA
	shift = aw8622x->ram.baseaddr_shift;
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RAMADDRH,
			       AW8622X_BIT_RAMADDRH_MASK,
			       aw8622x_cont->data[0 + shift]);
	aw8622x_i2c_write(aw8622x, AW8622X_REG_RAMADDRL,
			  aw8622x_cont->data[1 + shift]);
	shift = aw8622x->ram.ram_shift;
	for (i = shift; i < aw8622x_cont->len; i++) {
		aw8622x_i2c_read(aw8622x, AW8622X_REG_RAMDATA, &reg_val);
		/*
		*   aw_dev_info(aw8622x->dev,
		*	"%s aw8622x_cont->data=0x%02X, ramdata=0x%02X\n",
		*	__func__,aw8622x_cont->data[i],reg_val);
		*/
		if (reg_val != aw8622x_cont->data[i]) {
			aw_dev_err(aw8622x->dev,
				"%s: ram check error addr=0x%04x, file_data=0x%02X, ram_data=0x%02X\n",
				__func__, i, aw8622x_cont->data[i], reg_val);
			ret = -1;
			break;
		}
		check_sum += reg_val;
	}
	if (!ret) {
		aw8622x_i2c_read(aw8622x, AW8622X_REG_RTPCFG1, &reg_val);
		check_sum += reg_val & 0x0f;
		aw8622x_i2c_read(aw8622x, AW8622X_REG_RTPCFG2, &reg_val);
		check_sum += reg_val;

		if (check_sum != aw8622x->ram.check_sum) {
			aw_dev_err(aw8622x->dev, "%s: ram data check sum error, check_sum=0x%04x\n",
				__func__, check_sum);
			ret = -1;
		} else {
			aw_dev_err(aw8622x->dev, "%s: ram data check sum pass, check_sum=0x%04x\n",
				 __func__, check_sum);
		}
	}

#endif
	/* RAMINIT Disable */
	aw8622x_haptic_raminit(aw8622x, false);
	mutex_unlock(&aw8622x->lock);
	aw_dev_err(aw8622x->dev, "%s exit\n", __func__);
	return ret;
}

static void aw8622x_ram_loaded(const struct firmware *cont, void *context)
{
	struct aw8622x *aw8622x = context;
	struct aw8622x_container *aw8622x_fw;
	unsigned short check_sum = 0;
	int i = 0;
	int ret = 0;
#ifdef AW_READ_BIN_FLEXBALLY
	static unsigned char load_cont;
	int ram_timer_val = 1000;

	load_cont++;
#endif
	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);
	if (!cont) {
		aw_dev_err(aw8622x->dev, "%s: failed to read %s\n", __func__,
			   aw8622x_ram_name);
		release_firmware(cont);
#ifdef AW_READ_BIN_FLEXBALLY
		if (load_cont <= 20) {
			schedule_delayed_work(&aw8622x->ram_work,
						msecs_to_jiffies(ram_timer_val));
			aw_dev_err(aw8622x->dev, "%s:start hrtimer: load_cont=%d\n",
					__func__, load_cont);
		}
#endif
		return;
	}
	aw_dev_err(aw8622x->dev, "%s: loaded %s - size: %zu bytes\n", __func__,
		    aw8622x_ram_name, cont ? cont->size : 0);
/*
*	for(i=0; i < cont->size; i++) {
*		aw_dev_info(aw8622x->dev, "%s: addr: 0x%04x, data: 0x%02X\n",
*			__func__, i, *(cont->data+i));
*	}
*/
	/* check sum */
	for (i = 2; i < cont->size; i++)
		check_sum += cont->data[i];
	if (check_sum !=
	    (unsigned short)((cont->data[0] << 8) | (cont->data[1]))) {
		aw_dev_err(aw8622x->dev,
			"%s: check sum err: check_sum=0x%04x\n", __func__,
			check_sum);
		return;
	} else {
		aw_dev_err(aw8622x->dev, "%s: check sum pass: 0x%04x\n",
			    __func__, check_sum);
		aw8622x->ram.check_sum = check_sum;
	}

	/* aw8622x ram update less then 128kB */
	aw8622x_fw = kzalloc(cont->size + sizeof(int), GFP_KERNEL);
	if (!aw8622x_fw) {
		release_firmware(cont);
		aw_dev_err(aw8622x->dev, "%s: Error allocating memory\n",
			   __func__);
		return;
	}
	aw8622x_fw->len = cont->size;
	memcpy(aw8622x_fw->data, cont->data, cont->size);
	release_firmware(cont);
	ret = aw8622x_container_update(aw8622x, aw8622x_fw);
	if (ret) {
		kfree(aw8622x_fw);
		aw8622x->ram.len = 0;
		aw_dev_err(aw8622x->dev, "%s: ram firmware update failed!\n",
			__func__);
	} else {
		aw8622x->ram_init = 1;
		aw8622x->ram.len = aw8622x_fw->len;
		kfree(aw8622x_fw);
		aw_dev_err(aw8622x->dev, "%s: ram firmware update complete!\n",
		    __func__);
	}

}

static int aw8622x_ram_update(struct aw8622x *aw8622x)
{
	aw8622x->ram_init = 0;
	aw8622x->rtp_init = 0;

	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				       aw8622x_ram_name, aw8622x->dev,
				       GFP_KERNEL, aw8622x, aw8622x_ram_loaded);
}

static int aw8622x_rtp_trim_lra_calibration(struct aw8622x *aw8622x)
{
	unsigned char reg_val = 0;
	unsigned int fre_val = 0;
	unsigned int theory_time = 0;
	unsigned int lra_trim_code = 0;

	aw8622x_i2c_read(aw8622x, AW8622X_REG_SYSCTRL2, &reg_val);
	fre_val = (reg_val & 0x03) >> 0;

	if (fre_val == 2 || fre_val == 3)
		theory_time = (aw8622x->rtp_len / 12000) * 1000000;	/*12K */
	if (fre_val == 0)
		theory_time = (aw8622x->rtp_len / 24000) * 1000000;	/*24K */
	if (fre_val == 1)
		theory_time = (aw8622x->rtp_len / 48000) * 1000000;	/*48K */

	aw_dev_err(aw8622x->dev, "%s microsecond:%ld  theory_time = %d\n",
		    __func__, aw8622x->microsecond, theory_time);

	lra_trim_code = aw8622x_osc_trim_calculation(theory_time,
						     aw8622x->microsecond);
	if (lra_trim_code >= 0) {
		aw8622x->osc_cali_data = lra_trim_code;
		aw8622x_haptic_upload_lra(aw8622x, AW8622X_OSC_CALI);
	}
	return 0;
}

static enum hrtimer_restart aw8622x_vibrator_timer_func(struct hrtimer *timer)
{
	struct aw8622x *aw8622x = container_of(timer, struct aw8622x, timer);

	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);
	aw8622x->state = 0;
	schedule_work(&aw8622x->long_vibrate_work);

	return HRTIMER_NORESTART;
}

static int aw8622x_haptic_play_repeat_seq(struct aw8622x *aw8622x,
					  unsigned char flag)
{
	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);

	if (flag) {
		aw8622x_haptic_play_mode(aw8622x, AW8622X_HAPTIC_RAM_LOOP_MODE);
		aw8622x_haptic_play_go(aw8622x, true);
	}
	return 0;
}

static int aw8622x_haptic_trig_config(struct aw8622x *aw8622x)
{

	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);

	if (aw8622x->isUsedIntn == false) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL2,
				AW8622X_BIT_SYSCTRL2_INTN_PIN_MASK,
				AW8622X_BIT_SYSCTRL2_TRIG1);
	}

	return 0;
}

/*****************************************************
 *
 * motor protect
 *
 *****************************************************/
static int aw8622x_haptic_swicth_motor_protect_config(struct aw8622x *aw8622x,
						      unsigned char addr,
						      unsigned char val)
{
	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);
	if (addr == 1) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_DETCFG1,
				       AW8622X_BIT_DETCFG1_PRCT_MODE_MASK,
				       AW8622X_BIT_DETCFG1_PRCT_MODE_VALID);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PWMCFG1,
				       AW8622X_BIT_PWMCFG1_PRC_EN_MASK,
				       AW8622X_BIT_PWMCFG1_PRC_ENABLE);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PWMCFG3,
				       AW8622X_BIT_PWMCFG3_PR_EN_MASK,
				       AW8622X_BIT_PWMCFG3_PR_ENABLE);
	} else if (addr == 0) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_DETCFG1,
				       AW8622X_BIT_DETCFG1_PRCT_MODE_MASK,
				       AW8622X_BIT_DETCFG1_PRCT_MODE_INVALID);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PWMCFG1,
				       AW8622X_BIT_PWMCFG1_PRC_EN_MASK,
				       AW8622X_BIT_PWMCFG1_PRC_DISABLE);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PWMCFG3,
				       AW8622X_BIT_PWMCFG3_PR_EN_MASK,
				       AW8622X_BIT_PWMCFG3_PR_DISABLE);
	} else if (addr == 0x2d) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PWMCFG1,
				       AW8622X_BIT_PWMCFG1_PRCTIME_MASK, val);
	} else if (addr == 0x3e) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PWMCFG3,
				       AW8622X_BIT_PWMCFG3_PRLVL_MASK, val);
	} else if (addr == 0x3f) {
		aw8622x_i2c_write(aw8622x, AW8622X_REG_PWMCFG4, val);
	}
	return 0;
}

static int aw8622x_haptic_f0_calibration(struct aw8622x *aw8622x)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_limit = 0;
	char f0_cali_lra = 0;
	int f0_cali_step = 0;
	unsigned int f0_cali_min = aw8622x->dts_info.f0_ref *
				(100 - aw8622x->dts_info.f0_cali_percent) / 100;
	unsigned int f0_cali_max =  aw8622x->dts_info.f0_ref *
				(100 + aw8622x->dts_info.f0_cali_percent) / 100;

	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);
	/*
	 * aw8622x_haptic_upload_lra(aw8622x, AW8622X_WRITE_ZERO);
	 */
	if (aw8622x_haptic_cont_get_f0(aw8622x)) {
		aw_dev_err(aw8622x->dev, "%s get f0 error, user defafult f0\n",
			   __func__);
	} else {
		/* max and min limit */
		f0_limit = aw8622x->f0;
		aw_dev_err(aw8622x->dev, "%s f0_ref = %d, f0_cali_min = %d, f0_cali_max = %d, f0 = %d\n",
			    __func__, aw8622x->dts_info.f0_ref,
			    f0_cali_min, f0_cali_max, aw8622x->f0);

		if ((aw8622x->f0 < f0_cali_min) || aw8622x->f0 > f0_cali_max) {
			aw_dev_err(aw8622x->dev, "%s f0 calibration out of range = %d!\n",
				   __func__, aw8622x->f0);
			f0_limit = aw8622x->dts_info.f0_ref;
			return -ERANGE;
		}
		aw_dev_err(aw8622x->dev, "%s f0_limit = %d\n", __func__,
			    (int)f0_limit);
		/* calculate cali step */
		f0_cali_step = 100000 * ((int)f0_limit -
					 (int)aw8622x->dts_info.f0_ref) /
		    ((int)f0_limit * 24);
		aw_dev_err(aw8622x->dev, "%s f0_cali_step = %d\n", __func__,
			    f0_cali_step);
		if (f0_cali_step >= 0) {	/*f0_cali_step >= 0 */
			if (f0_cali_step % 10 >= 5)
				f0_cali_step = 32 + (f0_cali_step / 10 + 1);
			else
				f0_cali_step = 32 + f0_cali_step / 10;
		} else {	/* f0_cali_step < 0 */
			if (f0_cali_step % 10 <= -5)
				f0_cali_step = 32 + (f0_cali_step / 10 - 1);
			else
				f0_cali_step = 32 + f0_cali_step / 10;
		}
		if (f0_cali_step > 31)
			f0_cali_lra = (char)f0_cali_step - 32;
		else
			f0_cali_lra = (char)f0_cali_step + 32;
		/* update cali step */
		aw8622x->f0_cali_data = (int)f0_cali_lra;
		aw8622x_haptic_upload_lra(aw8622x, AW8622X_F0_CALI);

		aw8622x_i2c_read(aw8622x, AW8622X_REG_TRIMCFG3, &reg_val);

		aw_dev_err(aw8622x->dev, "%s final trim_lra=0x%02x\n",
			__func__, reg_val);
	}
	/* restore standby work mode */
	aw8622x_haptic_stop(aw8622x);
	return ret;
}

/*****************************************************
 *
 * haptic cont
 *
 *****************************************************/
static int aw8622x_haptic_cont_config(struct aw8622x *aw8622x)
{
	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);

	/* work mode */
	aw8622x_haptic_play_mode(aw8622x, AW8622X_HAPTIC_CONT_MODE);
	/* cont config */
	/* aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG1,
	 **                     AW8622X_BIT_CONTCFG1_EN_F0_DET_MASK,
	 **                     AW8622X_BIT_CONTCFG1_F0_DET_ENABLE);
	 */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG6,
			       AW8622X_BIT_CONTCFG6_TRACK_EN_MASK,
			       AW8622X_BIT_CONTCFG6_TRACK_ENABLE);
	/* f0 driver level */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG6,
			       AW8622X_BIT_CONTCFG6_DRV1_LVL_MASK,
			       aw8622x->cont_drv1_lvl);
	aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG7,
			  aw8622x->cont_drv2_lvl);
	/* DRV1_TIME */
	/* aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG8, 0xFF); */
	/* DRV2_TIME */
	aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG9, 0xFF);
	/* cont play go */
	aw8622x_haptic_play_go(aw8622x, true);
	return 0;
}

static int aw8622x_haptic_play_wav_seq(struct aw8622x *aw8622x,
				       unsigned char flag)
{
	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);
	if (flag) {
		aw8622x_haptic_play_mode(aw8622x, AW8622X_HAPTIC_RAM_MODE);
		aw8622x_haptic_play_go(aw8622x, true);
	}
	return 0;
}

#ifdef TIMED_OUTPUT
static int aw8622x_vibrator_get_time(struct timed_output_dev *dev)
{
	struct aw8622x *aw8622x = container_of(dev, struct aw8622x, vib_dev);

	if (hrtimer_active(&aw8622x->timer)) {
		ktime_t r = hrtimer_get_remaining(&aw8622x->timer);

		return ktime_to_ms(r);
	}
	return 0;
}

static void aw8622x_vibrator_enable(struct timed_output_dev *dev, int value)
{
	struct aw8622x *aw8622x = container_of(dev, struct aw8622x, vib_dev);

	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);
	mutex_lock(&aw8622x->lock);
	aw8622x_haptic_stop(aw8622x);
	if (value > 0) {
		aw8622x_haptic_ram_vbat_compensate(aw8622x, false);
		aw8622x_haptic_play_wav_seq(aw8622x, value);
	}
	mutex_unlock(&aw8622x->lock);
	aw_dev_err(aw8622x->dev, "%s exit\n", __func__);
}
#else
static enum led_brightness aw8622x_haptic_brightness_get(struct led_classdev
							 *cdev)
{
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	return aw8622x->amplitude;
}

static void aw8622x_haptic_brightness_set(struct led_classdev *cdev,
					  enum led_brightness level)
{
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);
	if (!aw8622x->ram_init) {
		aw_dev_err(aw8622x->dev, "%s: ram init failed, not allow to play!\n",
		       __func__);
		return;
	}
	if (aw8622x->ram_update_flag < 0)
		return;
	aw8622x->amplitude = level;
	mutex_lock(&aw8622x->lock);
	aw8622x_haptic_stop(aw8622x);
	if (aw8622x->amplitude > 0) {
		aw8622x_haptic_upload_lra(aw8622x, AW8622X_F0_CALI);
		aw8622x_haptic_ram_vbat_compensate(aw8622x, false);
		aw8622x_haptic_play_wav_seq(aw8622x, aw8622x->amplitude);
	}
	mutex_unlock(&aw8622x->lock);
}
#endif

static int
aw8622x_haptic_audio_ctr_list_insert(struct haptic_audio *haptic_audio,
						struct haptic_ctr *haptic_ctr,
						struct device *dev)
{
	struct haptic_ctr *p_new = NULL;

	p_new = (struct haptic_ctr *)kzalloc(
		sizeof(struct haptic_ctr), GFP_KERNEL);
	if (p_new == NULL) {
		aw_dev_err(dev, "%s: kzalloc memory fail\n", __func__);
		return -1;
	}
	/* update new list info */
	p_new->cnt = haptic_ctr->cnt;
	p_new->cmd = haptic_ctr->cmd;
	p_new->play = haptic_ctr->play;
	p_new->wavseq = haptic_ctr->wavseq;
	p_new->loop = haptic_ctr->loop;
	p_new->gain = haptic_ctr->gain;

	INIT_LIST_HEAD(&(p_new->list));
	list_add(&(p_new->list), &(haptic_audio->ctr_list));
	return 0;
}

static int
aw8622x_haptic_audio_ctr_list_clear(struct haptic_audio *haptic_audio)
{
	struct haptic_ctr *p_ctr = NULL;
	struct haptic_ctr *p_ctr_bak = NULL;

	list_for_each_entry_safe_reverse(p_ctr, p_ctr_bak,
					&(haptic_audio->ctr_list),
					list) {
		list_del(&p_ctr->list);
		kfree(p_ctr);
	}

	return 0;
}

static int aw8622x_haptic_audio_off(struct aw8622x *aw8622x)
{
	aw_dev_dbg(aw8622x->dev, "%s: enter\n", __func__);
	mutex_lock(&aw8622x->lock);
	aw8622x_haptic_set_gain(aw8622x, 0x80);
	aw8622x_haptic_stop(aw8622x);
	aw8622x->gun_type = 0xff;
	aw8622x->bullet_nr = 0;
	aw8622x_haptic_audio_ctr_list_clear(&aw8622x->haptic_audio);
	mutex_unlock(&aw8622x->lock);
	return 0;
}

static int aw8622x_haptic_audio_init(struct aw8622x *aw8622x)
{

	aw_dev_dbg(aw8622x->dev, "%s enter\n", __func__);
	aw8622x_haptic_set_wav_seq(aw8622x, 0x01, 0x00);

	return 0;
}

static int aw8622x_haptic_activate(struct aw8622x *aw8622x)
{
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL2,
			       AW8622X_BIT_SYSCTRL2_STANDBY_MASK,
			       AW8622X_BIT_SYSCTRL2_STANDBY_OFF);
	aw8622x_interrupt_clear(aw8622x);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSINTM,
			       AW8622X_BIT_SYSINTM_UVLM_MASK,
			       AW8622X_BIT_SYSINTM_UVLM_ON);
	return 0;
}

static int aw8622x_haptic_start(struct aw8622x *aw8622x)
{
	aw8622x_haptic_activate(aw8622x);
	aw8622x_haptic_play_go(aw8622x, true);
	return 0;
}


static void aw8622x_haptic_audio_work_routine(struct work_struct *work)
{
	struct aw8622x *aw8622x = container_of(work,
					struct aw8622x,
					haptic_audio.work);
	struct haptic_audio *haptic_audio = NULL;
	struct haptic_ctr *p_ctr = NULL;
	struct haptic_ctr *p_ctr_bak = NULL;
	unsigned int ctr_list_flag = 0;
	unsigned int ctr_list_input_cnt = 0;
	unsigned int ctr_list_output_cnt = 0;
	unsigned int ctr_list_diff_cnt = 0;
	unsigned int ctr_list_del_cnt = 0;
	int rtp_is_going_on = 0;

	aw_dev_dbg(aw8622x->dev, "%s enter\n", __func__);

	haptic_audio = &(aw8622x->haptic_audio);
	mutex_lock(&aw8622x->haptic_audio.lock);
	memset(&aw8622x->haptic_audio.ctr, 0,
	       sizeof(struct haptic_ctr));
	ctr_list_flag = 0;
	list_for_each_entry_safe_reverse(p_ctr, p_ctr_bak,
			&(haptic_audio->ctr_list), list) {
		ctr_list_flag = 1;
		break;
	}
	if (ctr_list_flag == 0)
		aw_dev_err(aw8622x->dev, "%s: ctr list empty\n", __func__);

	if (ctr_list_flag == 1) {
		list_for_each_entry_safe(p_ctr, p_ctr_bak,
					&(haptic_audio->ctr_list), list) {
			ctr_list_input_cnt =  p_ctr->cnt;
			break;
		}
		list_for_each_entry_safe_reverse(p_ctr, p_ctr_bak,
					&(haptic_audio->ctr_list), list) {
			ctr_list_output_cnt =  p_ctr->cnt;
			break;
		}
		if (ctr_list_input_cnt > ctr_list_output_cnt)
			ctr_list_diff_cnt = ctr_list_input_cnt - ctr_list_output_cnt;

		if (ctr_list_input_cnt < ctr_list_output_cnt)
			ctr_list_diff_cnt = 32 + ctr_list_input_cnt - ctr_list_output_cnt;

		if (ctr_list_diff_cnt > 2) {
			list_for_each_entry_safe_reverse(p_ctr, p_ctr_bak,
					&(haptic_audio->ctr_list), list) {
				if ((p_ctr->play == 0) &&
				(AW8622X_HAPTIC_CMD_ENABLE ==
					(AW8622X_HAPTIC_CMD_HAPTIC & p_ctr->cmd))) {
					list_del(&p_ctr->list);
					kfree(p_ctr);
					ctr_list_del_cnt++;
				}
				if (ctr_list_del_cnt == ctr_list_diff_cnt)
					break;
			}
		}
	}

	/* get the last data from list */
	list_for_each_entry_safe_reverse(p_ctr, p_ctr_bak,
				&(haptic_audio->ctr_list), list) {
		aw8622x->haptic_audio.ctr.cnt = p_ctr->cnt;
		aw8622x->haptic_audio.ctr.cmd = p_ctr->cmd;
		aw8622x->haptic_audio.ctr.play = p_ctr->play;
		aw8622x->haptic_audio.ctr.wavseq = p_ctr->wavseq;
		aw8622x->haptic_audio.ctr.loop = p_ctr->loop;
		aw8622x->haptic_audio.ctr.gain = p_ctr->gain;
		list_del(&p_ctr->list);
		kfree(p_ctr);
		break;
	}

	if (aw8622x->haptic_audio.ctr.play) {
		aw_dev_err(aw8622x->dev, "%s: cnt=%d, cmd=%d, play=%d, wavseq=%d, loop=%d, gain=%d\n",
			__func__,
			aw8622x->haptic_audio.ctr.cnt,
			aw8622x->haptic_audio.ctr.cmd,
			aw8622x->haptic_audio.ctr.play,
			aw8622x->haptic_audio.ctr.wavseq,
			aw8622x->haptic_audio.ctr.loop,
			aw8622x->haptic_audio.ctr.gain);
	}

	/* rtp mode jump */
	rtp_is_going_on = aw8622x_haptic_juge_RTP_is_going_on(aw8622x);
	if (rtp_is_going_on) {
		mutex_unlock(&aw8622x->haptic_audio.lock);
		return;
	}
	mutex_unlock(&aw8622x->haptic_audio.lock);

	/*haptic play control*/
	if (AW8622X_HAPTIC_CMD_ENABLE ==
	   (AW8622X_HAPTIC_CMD_HAPTIC & aw8622x->haptic_audio.ctr.cmd)) {
		if (aw8622x->haptic_audio.ctr.play ==
			AW8622X_HAPTIC_PLAY_ENABLE) {
			aw_dev_err(aw8622x->dev,
				"%s: haptic_audio_play_start\n", __func__);
			aw_dev_err(aw8622x->dev,
				"%s: normal haptic start\n", __func__);
			mutex_lock(&aw8622x->lock);
			aw8622x_haptic_stop(aw8622x);
			aw8622x_haptic_play_mode(aw8622x,
				AW8622X_HAPTIC_RAM_MODE);
			aw8622x_haptic_set_wav_seq(aw8622x, 0x00,
				aw8622x->haptic_audio.ctr.wavseq);
			aw8622x_haptic_set_wav_loop(aw8622x, 0x00,
				aw8622x->haptic_audio.ctr.loop);
			aw8622x_haptic_set_gain(aw8622x,
				aw8622x->haptic_audio.ctr.gain);
			aw8622x_haptic_start(aw8622x);
			mutex_unlock(&aw8622x->lock);
		} else if (AW8622X_HAPTIC_PLAY_STOP ==
			   aw8622x->haptic_audio.ctr.play) {
			mutex_lock(&aw8622x->lock);
			aw8622x_haptic_stop(aw8622x);
			mutex_unlock(&aw8622x->lock);
		} else if (AW8622X_HAPTIC_PLAY_GAIN ==
			   aw8622x->haptic_audio.ctr.play) {
			mutex_lock(&aw8622x->lock);
			aw8622x_haptic_set_gain(aw8622x,
					       aw8622x->haptic_audio.ctr.gain);
			mutex_unlock(&aw8622x->lock);
		}
	}


}

static ssize_t aw8622x_cont_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	ssize_t len = 0;

	aw8622x_haptic_read_cont_f0(aw8622x);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"%d\n", aw8622x->cont_f0);
	return len;
}

static ssize_t aw8622x_cont_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	aw8622x_haptic_stop(aw8622x);
	if (val)
		aw8622x_haptic_cont_config(aw8622x);
	return count;
}

static ssize_t aw8622x_f0_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	ssize_t len = 0;
	unsigned char reg = 0;

	mutex_lock(&aw8622x->lock);

	/* set d2s_gain to max to get better performance when cat f0 .*/
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL7,
				AW8622X_BIT_SYSCTRL7_D2S_GAIN_MASK,
				AW8622X_BIT_SYSCTRL7_D2S_GAIN_40);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_TRIMCFG3, &reg);
	aw8622x_haptic_upload_lra(aw8622x, AW8622X_WRITE_ZERO);
	aw8622x_haptic_cont_get_f0(aw8622x);
	aw8622x_i2c_write(aw8622x, AW8622X_REG_TRIMCFG3, reg);
	/* set d2s_gain to default when cat f0 is finished.*/
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL7,
				AW8622X_BIT_SYSCTRL7_D2S_GAIN_MASK,
				aw8622x->dts_info.d2s_gain);
	mutex_unlock(&aw8622x->lock);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"%d\n", aw8622x->f0);
	return len;
}

static ssize_t aw8622x_f0_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t aw8622x_reg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	ssize_t len = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < AW8622X_REG_MAX; i++) {
		if (!(aw8622x_reg_access[i] & REG_RD_ACCESS))
			continue;
		aw8622x_i2c_read(aw8622x, i, &reg_val);
		len += snprintf(buf + len, PAGE_SIZE - len,
				"reg:0x%02X=0x%02X\n", i, reg_val);
	}
	return len;
}

static ssize_t aw8622x_reg_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw8622x_i2c_write(aw8622x, (unsigned char)databuf[0],
				  (unsigned char)databuf[1]);
	}

	return count;
}

static ssize_t aw8622x_duration_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	ktime_t time_rem;
	s64 time_ms = 0;

	if (hrtimer_active(&aw8622x->timer)) {
		time_rem = hrtimer_get_remaining(&aw8622x->timer);
		time_ms = ktime_to_ms(time_rem);
	}
	return snprintf(buf, PAGE_SIZE, "%lld\n", time_ms);
}

static ssize_t aw8622x_duration_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	/* setting 0 on duration is NOP for now */
	if (val <= 0)
		return count;
	aw8622x->duration = val;
	return count;
}

static ssize_t aw8622x_activate_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;

	/* For now nothing to show */
	return snprintf(buf, PAGE_SIZE, "activate = %d\n", aw8622x->state);
}

static ssize_t aw8622x_activate_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned int val = 0;
	int rc = 0;

	if (!aw8622x->ram_init) {
		aw_dev_err(aw8622x->dev, "%s: ram init failed, not allow to play!\n",
		       __func__);
		return count;
	}
	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val != 0 && val != 1)
		return count;
	aw_dev_err(aw8622x->dev, "%s: value=%d\n", __func__, val);
	mutex_lock(&aw8622x->lock);
	hrtimer_cancel(&aw8622x->timer);
	aw8622x->state = val;
	mutex_unlock(&aw8622x->lock);
	schedule_work(&aw8622x->long_vibrate_work);
	return count;
}

static ssize_t aw8622x_seq_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	size_t count = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < AW8622X_SEQUENCER_SIZE; i++) {
		aw8622x_i2c_read(aw8622x, AW8622X_REG_WAVCFG1 + i, &reg_val);
		count += snprintf(buf + count, PAGE_SIZE - count,
				  "seq%d: 0x%02x\n", i + 1, reg_val);
		aw8622x->seq[i] |= reg_val;
	}
	return count;
}

static ssize_t aw8622x_seq_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		if (databuf[0] > AW8622X_SEQUENCER_SIZE) {
			aw_dev_err(aw8622x->dev, "%s input value out of range\n",
				__func__);
			return count;
		}
		aw_dev_err(aw8622x->dev, "%s: seq%d=0x%02X\n", __func__,
			    databuf[0], databuf[1]);
		mutex_lock(&aw8622x->lock);
		aw8622x->seq[databuf[0]] = (unsigned char)databuf[1];
		aw8622x_haptic_set_wav_seq(aw8622x, (unsigned char)databuf[0],
					   aw8622x->seq[databuf[0]]);
		mutex_unlock(&aw8622x->lock);
	}
	return count;
}

static ssize_t aw8622x_loop_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	size_t count = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < AW8622X_SEQUENCER_LOOP_SIZE; i++) {
		aw8622x_i2c_read(aw8622x, AW8622X_REG_WAVCFG9 + i, &reg_val);
		aw8622x->loop[i * 2 + 0] = (reg_val >> 4) & 0x0F;
		aw8622x->loop[i * 2 + 1] = (reg_val >> 0) & 0x0F;

		count += snprintf(buf + count, PAGE_SIZE - count,
				  "seq%d_loop = 0x%02x\n", i * 2 + 1,
				  aw8622x->loop[i * 2 + 0]);
		count += snprintf(buf + count, PAGE_SIZE - count,
				  "seq%d_loop = 0x%02x\n", i * 2 + 2,
				  aw8622x->loop[i * 2 + 1]);
	}
	return count;
}

static ssize_t aw8622x_loop_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw_dev_err(aw8622x->dev, "%s: seq%d loop=0x%02X\n", __func__,
			    databuf[0], databuf[1]);
		mutex_lock(&aw8622x->lock);
		aw8622x->loop[databuf[0]] = (unsigned char)databuf[1];
		aw8622x_haptic_set_wav_loop(aw8622x, (unsigned char)databuf[0],
					    aw8622x->loop[databuf[0]]);
		mutex_unlock(&aw8622x->lock);
	}
	return count;
}

static ssize_t aw8622x_rtp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"rtp_cnt = %d\n",
			aw8622x->rtp_cnt);
	return len;
}

static ssize_t aw8622x_rtp_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0) {
		aw_dev_err(aw8622x->dev,
			"%s: kstrtouint fail\n", __func__);
		return rc;
	}
	mutex_lock(&aw8622x->lock);
	aw8622x_haptic_stop(aw8622x);
	aw8622x_haptic_set_rtp_aei(aw8622x, false);
	aw8622x_interrupt_clear(aw8622x);
	/* aw8622x_rtp_brake_set(aw8622x); */
	if (val < (sizeof(aw8622x_rtp_name) / AW8622X_RTP_NAME_MAX)) {
		aw8622x->rtp_file_num = val;
		if (val) {
			aw_dev_err(aw8622x->dev,
				"%s: aw8622x_rtp_name[%d]: %s\n", __func__,
				val, aw8622x_rtp_name[val]);

			schedule_work(&aw8622x->rtp_work);
		} else {
			aw_dev_err(aw8622x->dev,
				"%s: rtp_file_num 0x%02X over max value\n",
				__func__, aw8622x->rtp_file_num);
		}
	}
	mutex_unlock(&aw8622x->lock);
	return count;
}

static ssize_t aw8622x_state_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;

	return snprintf(buf, PAGE_SIZE, "%d\n", aw8622x->state);
}

static ssize_t aw8622x_state_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{

	return count;
}

static ssize_t aw8622x_activate_mode_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;

	return snprintf(buf, PAGE_SIZE, "%d\n",
			aw8622x->activate_mode);
}

static ssize_t aw8622x_activate_mode_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	mutex_lock(&aw8622x->lock);
	aw8622x->activate_mode = val;
	mutex_unlock(&aw8622x->lock);
	return count;
}

static ssize_t aw8622x_index_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned char reg_val = 0;

	aw8622x_i2c_read(aw8622x, AW8622X_REG_WAVCFG1, &reg_val);
	aw8622x->index = reg_val;
	return snprintf(buf, PAGE_SIZE, "index = %d\n", aw8622x->index);
}

static ssize_t aw8622x_index_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw_dev_err(aw8622x->dev, "%s: value=%d\n", __func__, val);
	mutex_lock(&aw8622x->lock);
	aw8622x->index = val;
	aw8622x_haptic_set_repeat_wav_seq(aw8622x, aw8622x->index);
	mutex_unlock(&aw8622x->lock);
	return count;
}

static ssize_t aw8622x_sram_size_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned char reg_val = 0;

	aw8622x_i2c_read(aw8622x, AW8622X_REG_RTPCFG1, &reg_val);
	if ((reg_val & 0x30) == 0x20)
		return snprintf(buf, PAGE_SIZE, "sram_size = 2K\n");
	else if ((reg_val & 0x30) == 0x10)
		return snprintf(buf, PAGE_SIZE, "sram_size = 1K\n");
	else if ((reg_val & 0x30) == 0x30)
		return snprintf(buf, PAGE_SIZE, "sram_size = 3K\n");
	return snprintf(buf, PAGE_SIZE,
			"sram_size = 0x%02x error, plz check reg.\n",
			reg_val & 0x30);
}

static ssize_t aw8622x_sram_size_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw_dev_err(aw8622x->dev, "%s: value=%d\n", __func__, val);
	mutex_lock(&aw8622x->lock);
	if (val == AW8622X_HAPTIC_SRAM_2K)
		aw8622x_sram_size(aw8622x, AW8622X_HAPTIC_SRAM_2K);
	else if (val == AW8622X_HAPTIC_SRAM_1K)
		aw8622x_sram_size(aw8622x, AW8622X_HAPTIC_SRAM_1K);
	else if (val == AW8622X_HAPTIC_SRAM_3K)
		aw8622x_sram_size(aw8622x, AW8622X_HAPTIC_SRAM_3K);
	mutex_unlock(&aw8622x->lock);
	return count;
}

static ssize_t aw8622x_osc_cali_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "osc_cali_data = 0x%02X\n",
			aw8622x->osc_cali_data);

	return len;
}

static ssize_t aw8622x_osc_cali_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	mutex_lock(&aw8622x->lock);
	aw8622x->osc_cali_run = 1;
	if (val == 1) {
		aw8622x_haptic_upload_lra(aw8622x, AW8622X_WRITE_ZERO);
		aw8622x_rtp_osc_calibration(aw8622x);
		aw8622x_rtp_trim_lra_calibration(aw8622x);
	} else if (val == 2) {
		aw8622x_haptic_upload_lra(aw8622x, AW8622X_OSC_CALI);
		aw8622x_rtp_osc_calibration(aw8622x);
	} else {
		aw_dev_err(aw8622x->dev, "%s input value out of range\n", __func__);
	}
	aw8622x->osc_cali_run = 0;
	/* osc calibration flag end, other behaviors are permitted */
	mutex_unlock(&aw8622x->lock);

	return count;
}

static ssize_t aw8622x_gain_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	unsigned char reg = 0;
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;

	aw8622x_i2c_read(aw8622x, AW8622X_REG_PLAYCFG2, &reg);

	return snprintf(buf, PAGE_SIZE, "0x%02X\n", reg);
}

static ssize_t aw8622x_gain_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	aw_dev_err(aw8622x->dev, "%s: value=%d\n", __func__, val);
	if (val >= 0x80)
		val = 0x80;
	mutex_lock(&aw8622x->lock);
	aw8622x->gain = val;
	aw8622x_haptic_set_gain(aw8622x, aw8622x->gain);
	mutex_unlock(&aw8622x->lock);
	return count;
}

static ssize_t aw8622x_ram_update_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	ssize_t len = 0;
	unsigned int i = 0;
	unsigned char reg_val = 0;

	/* RAMINIT Enable */
	aw8622x_haptic_raminit(aw8622x, true);
	aw8622x_haptic_stop(aw8622x);
	aw8622x_i2c_write(aw8622x, AW8622X_REG_RAMADDRH,
			  (unsigned char)(aw8622x->ram.base_addr >> 8));
	aw8622x_i2c_write(aw8622x, AW8622X_REG_RAMADDRL,
			  (unsigned char)(aw8622x->ram.base_addr & 0x00ff));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"haptic_ram len = %d\n", aw8622x->ram.len);
	for (i = 0; i < aw8622x->ram.len; i++) {
		aw8622x_i2c_read(aw8622x, AW8622X_REG_RAMDATA, &reg_val);
		if (i % 5 == 0)
			len += snprintf(buf + len,
				PAGE_SIZE - len, "0x%02X\n", reg_val);
		else
			len += snprintf(buf + len,
				PAGE_SIZE - len, "0x%02X,", reg_val);
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	/* RAMINIT Disable */
	aw8622x_haptic_raminit(aw8622x, false);
	return len;
}

static ssize_t aw8622x_ram_update_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val)
		aw8622x_ram_update(aw8622x);
	return count;
}

static ssize_t aw8622x_f0_save_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "f0_cali_data = 0x%02X\n",
			aw8622x->f0_cali_data);

	return len;
}

static ssize_t aw8622x_f0_save_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw8622x->f0_cali_data = val;
	return count;
}

static ssize_t aw8622x_osc_save_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "osc_cali_data = 0x%02X\n",
			aw8622x->osc_cali_data);

	return len;
}

static ssize_t aw8622x_osc_save_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw8622x->osc_cali_data = val;
	return count;
}


static ssize_t aw8622x_ram_vbat_compensate_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	ssize_t len = 0;

	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "ram_vbat_comp = %d\n",
		     aw8622x->ram_vbat_compensate);

	return len;
}

static ssize_t aw8622x_ram_vbat_compensate_store(struct device *dev,
						 struct device_attribute *attr,
						 const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw8622x->lock);
	if (val)
		aw8622x->ram_vbat_compensate =
		    AW8622X_HAPTIC_RAM_VBAT_COMP_ENABLE;
	else
		aw8622x->ram_vbat_compensate =
		    AW8622X_HAPTIC_RAM_VBAT_COMP_DISABLE;
	mutex_unlock(&aw8622x->lock);

	return count;
}

static ssize_t aw8622x_cali_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	ssize_t len = 0;
	unsigned char reg = 0;

	mutex_lock(&aw8622x->lock);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_TRIMCFG3, &reg);
	aw8622x_haptic_upload_lra(aw8622x, AW8622X_F0_CALI);
	aw8622x_haptic_cont_get_f0(aw8622x);
	aw8622x_i2c_write(aw8622x, AW8622X_REG_TRIMCFG3, reg);
	mutex_unlock(&aw8622x->lock);
	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", aw8622x->f0);
	return len;
}

static ssize_t aw8622x_cali_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val) {
		mutex_lock(&aw8622x->lock);
		aw8622x_haptic_upload_lra(aw8622x, AW8622X_WRITE_ZERO);
		aw8622x_haptic_f0_calibration(aw8622x);
		mutex_unlock(&aw8622x->lock);
	}
	return count;
}

static ssize_t aw8622x_cont_wait_num_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"cont_wait_num = 0x%02X\n", aw8622x->cont_wait_num);
	return len;
}

static ssize_t aw8622x_cont_wait_num_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned int databuf[1] = { 0 };

	if (sscanf(buf, "%x", &databuf[0]) == 1) {
		aw8622x->cont_wait_num = databuf[0];
		aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG4, databuf[0]);
	}
	return count;
}

static ssize_t aw8622x_cont_drv_lvl_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"cont_drv1_lvl = 0x%02X\n",
			aw8622x->cont_drv1_lvl);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"cont_drv2_lvl = 0x%02X\n",
			aw8622x->cont_drv2_lvl);
	return len;
}

static ssize_t aw8622x_cont_drv_lvl_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw8622x->cont_drv1_lvl = databuf[0];
		aw8622x->cont_drv2_lvl = databuf[1];
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG6,
				       AW8622X_BIT_CONTCFG6_DRV1_LVL_MASK,
				       aw8622x->cont_drv1_lvl);
		aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG7,
				  aw8622x->cont_drv2_lvl);
	}
	return count;
}

static ssize_t aw8622x_cont_drv_time_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"cont_drv1_time = 0x%02X\n",
			aw8622x->cont_drv1_time);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"cont_drv2_time = 0x%02X\n",
			aw8622x->cont_drv2_time);
	return len;
}

static ssize_t aw8622x_cont_drv_time_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw8622x->cont_drv1_time = databuf[0];
		aw8622x->cont_drv2_time = databuf[1];
		aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG8,
				  aw8622x->cont_drv1_time);
		aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG9,
				  aw8622x->cont_drv2_time);
	}
	return count;
}

static ssize_t aw8622x_cont_brk_time_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "cont_brk_time = 0x%02X\n",
			aw8622x->cont_brk_time);
	return len;
}

static ssize_t aw8622x_cont_brk_time_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned int databuf[1] = { 0 };

	if (sscanf(buf, "%x", &databuf[0]) == 1) {
		aw8622x->cont_brk_time = databuf[0];
		aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG10,
				  aw8622x->cont_brk_time);
	}
	return count;
}

static ssize_t aw8622x_vbat_monitor_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	ssize_t len = 0;

	mutex_lock(&aw8622x->lock);
	aw8622x_haptic_get_vbat(aw8622x);
	len += snprintf(buf + len, PAGE_SIZE - len, "vbat_monitor = %d\n",
			aw8622x->vbat);
	mutex_unlock(&aw8622x->lock);

	return len;
}

static ssize_t aw8622x_vbat_monitor_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	return count;
}

static ssize_t aw8622x_lra_resistance_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	ssize_t len = 0;

	aw8622x_haptic_get_lra_resistance(aw8622x);
	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n",
			aw8622x->lra);
	return len;
}

static ssize_t aw8622x_lra_resistance_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	return count;
}

static ssize_t aw8622x_prctmode_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	ssize_t len = 0;
	unsigned char reg_val = 0;

	aw8622x_i2c_read(aw8622x, AW8622X_REG_DETCFG1, &reg_val);

	len += snprintf(buf + len, PAGE_SIZE - len, "prctmode = %d\n",
			reg_val & 0x08);
	return len;
}

static ssize_t aw8622x_prctmode_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned int databuf[2] = { 0, 0 };
	unsigned int addr = 0;
	unsigned int val = 0;

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		addr = databuf[0];
		val = databuf[1];
		mutex_lock(&aw8622x->lock);
		aw8622x_haptic_swicth_motor_protect_config(aw8622x, addr, val);
		mutex_unlock(&aw8622x->lock);
	}
	return count;
}

static ssize_t aw8622x_gun_type_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw8622x->gun_type);

}

static ssize_t aw8622x_gun_type_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw_dev_err(aw8622x->dev, "%s: value=%d\n", __func__, val);
	mutex_lock(&aw8622x->lock);
	aw8622x->gun_type = val;
	mutex_unlock(&aw8622x->lock);
	return count;
}

static ssize_t aw8622x_bullet_nr_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw8622x->bullet_nr);
}

static ssize_t aw8622x_bullet_nr_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw_dev_err(aw8622x->dev, "%s: value=%d\n", __func__, val);
	mutex_lock(&aw8622x->lock);
	aw8622x->bullet_nr = val;
	mutex_unlock(&aw8622x->lock);
	return count;
}

static ssize_t aw8622x_haptic_audio_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len,
			"%d\n", aw8622x->haptic_audio.ctr.cnt);
	return len;
}

static ssize_t aw8622x_haptic_audio_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = awinic->aw8622x;
	unsigned int databuf[6] = {0};
	int rtp_is_going_on = 0;
	struct haptic_ctr *hap_ctr = NULL;

	rtp_is_going_on = aw8622x_haptic_juge_RTP_is_going_on(aw8622x);
	if (rtp_is_going_on) {
		aw_dev_err(aw8622x->dev,
			"%s: RTP is runing, stop audio haptic\n", __func__);
		return count;
	}
	if (!aw8622x->ram_init)
		return count;

	if (sscanf(buf, "%d %d %d %d %d %d",
		&databuf[0], &databuf[1], &databuf[2],
		&databuf[3], &databuf[4], &databuf[5]) == 6) {
		if (databuf[2]) {
			aw_dev_err(aw8622x->dev, "%s: cnt=%d, cmd=%d, play=%d, wavseq=%d, loop=%d, gain=%d\n",
				__func__,
				databuf[0], databuf[1], databuf[2],
				databuf[3], databuf[4], databuf[5]);
			hap_ctr = (struct haptic_ctr *)kzalloc(sizeof(struct haptic_ctr),
								GFP_KERNEL);
			if (hap_ctr == NULL) {
				aw_dev_err(aw8622x->dev, "%s: kzalloc memory fail\n",
					   __func__);
				return count;
			}
			mutex_lock(&aw8622x->haptic_audio.lock);
			hap_ctr->cnt = (unsigned char)databuf[0];
			hap_ctr->cmd = (unsigned char)databuf[1];
			hap_ctr->play = (unsigned char)databuf[2];
			hap_ctr->wavseq = (unsigned char)databuf[3];
			hap_ctr->loop = (unsigned char)databuf[4];
			hap_ctr->gain = (unsigned char)databuf[5];
			aw8622x_haptic_audio_ctr_list_insert(&aw8622x->haptic_audio,
							hap_ctr, aw8622x->dev);
			if (hap_ctr->cmd == 0xff) {
				aw_dev_err(aw8622x->dev,
					"%s: haptic_audio stop\n", __func__);
				if (hrtimer_active(&aw8622x->haptic_audio.timer)) {
					aw_dev_err(aw8622x->dev, "%s: cancel haptic_audio_timer\n",
						__func__);
					hrtimer_cancel(&aw8622x->haptic_audio.timer);
					aw8622x->haptic_audio.ctr.cnt = 0;
					aw8622x_haptic_audio_off(aw8622x);
				}
			} else {
				if (hrtimer_active(&aw8622x->haptic_audio.timer)) {
				} else {
					aw_dev_err(aw8622x->dev, "%s: start haptic_audio_timer\n",
						__func__);
					aw8622x_haptic_audio_init(aw8622x);
					hrtimer_start(&aw8622x->haptic_audio.timer,
					ktime_set(aw8622x->haptic_audio.delay_val/1000000,
						(aw8622x->haptic_audio.delay_val%1000000)*1000),
					HRTIMER_MODE_REL);
				}
			}

		}
		mutex_unlock(&aw8622x->haptic_audio.lock);
		kfree(hap_ctr);
	}
	return count;
}

static DEVICE_ATTR(f0, 0644, aw8622x_f0_show, aw8622x_f0_store);
static DEVICE_ATTR(cont, 0644, aw8622x_cont_show, aw8622x_cont_store);
static DEVICE_ATTR(register, 0644, aw8622x_reg_show, aw8622x_reg_store);
static DEVICE_ATTR(duration, 0644, aw8622x_duration_show,
		   aw8622x_duration_store);
static DEVICE_ATTR(index, 0644, aw8622x_index_show, aw8622x_index_store);
static DEVICE_ATTR(activate, 0644, aw8622x_activate_show,
		   aw8622x_activate_store);
static DEVICE_ATTR(activate_mode, 0644, aw8622x_activate_mode_show,
		   aw8622x_activate_mode_store);
static DEVICE_ATTR(seq, 0644, aw8622x_seq_show, aw8622x_seq_store);
static DEVICE_ATTR(loop, 0644, aw8622x_loop_show, aw8622x_loop_store);
static DEVICE_ATTR(rtp, 0644, aw8622x_rtp_show, aw8622x_rtp_store);
static DEVICE_ATTR(state, 0644, aw8622x_state_show, aw8622x_state_store);
static DEVICE_ATTR(sram_size, 0644, aw8622x_sram_size_show,
		   aw8622x_sram_size_store);
static DEVICE_ATTR(osc_cali, 0644, aw8622x_osc_cali_show,
		   aw8622x_osc_cali_store);
static DEVICE_ATTR(gain, 0644, aw8622x_gain_show, aw8622x_gain_store);
static DEVICE_ATTR(ram_update, 0644, aw8622x_ram_update_show,
		   aw8622x_ram_update_store);
static DEVICE_ATTR(f0_save, 0644, aw8622x_f0_save_show, aw8622x_f0_save_store);
static DEVICE_ATTR(osc_save, 0644, aw8622x_osc_save_show,
		   aw8622x_osc_save_store);
static DEVICE_ATTR(ram_vbat_comp, 0644, aw8622x_ram_vbat_compensate_show,
		   aw8622x_ram_vbat_compensate_store);
static DEVICE_ATTR(cali, 0644, aw8622x_cali_show, aw8622x_cali_store);
static DEVICE_ATTR(cont_wait_num, 0644, aw8622x_cont_wait_num_show,
		   aw8622x_cont_wait_num_store);
static DEVICE_ATTR(cont_drv_lvl, 0644, aw8622x_cont_drv_lvl_show,
		   aw8622x_cont_drv_lvl_store);
static DEVICE_ATTR(cont_drv_time, 0644, aw8622x_cont_drv_time_show,
		   aw8622x_cont_drv_time_store);
static DEVICE_ATTR(cont_brk_time, 0644, aw8622x_cont_brk_time_show,
		   aw8622x_cont_brk_time_store);
static DEVICE_ATTR(vbat_monitor, 0644, aw8622x_vbat_monitor_show,
		   aw8622x_vbat_monitor_store);
static DEVICE_ATTR(lra_resistance, 0644, aw8622x_lra_resistance_show,
		   aw8622x_lra_resistance_store);
static DEVICE_ATTR(prctmode, 0644, aw8622x_prctmode_show,
		   aw8622x_prctmode_store);
static DEVICE_ATTR(gun_type, 0644, aw8622x_gun_type_show,
		   aw8622x_gun_type_store);
static DEVICE_ATTR(bullet_nr, 0644, aw8622x_bullet_nr_show,
		   aw8622x_bullet_nr_store);
static DEVICE_ATTR(haptic_audio, 0644, aw8622x_haptic_audio_show,
		   aw8622x_haptic_audio_store);
static struct attribute *aw8622x_vibrator_attributes[] = {
	&dev_attr_state.attr,
	&dev_attr_duration.attr,
	&dev_attr_activate.attr,
	&dev_attr_activate_mode.attr,
	&dev_attr_index.attr,
	&dev_attr_gain.attr,
	&dev_attr_seq.attr,
	&dev_attr_loop.attr,
	&dev_attr_register.attr,
	&dev_attr_rtp.attr,
	&dev_attr_ram_update.attr,
	&dev_attr_f0.attr,
	&dev_attr_cali.attr,
	&dev_attr_f0_save.attr,
	&dev_attr_osc_save.attr,
	&dev_attr_cont.attr,
	&dev_attr_cont_wait_num.attr,
	&dev_attr_cont_drv_lvl.attr,
	&dev_attr_cont_drv_time.attr,
	&dev_attr_cont_brk_time.attr,
	&dev_attr_vbat_monitor.attr,
	&dev_attr_lra_resistance.attr,
	&dev_attr_prctmode.attr,
	&dev_attr_sram_size.attr,
	&dev_attr_ram_vbat_comp.attr,
	&dev_attr_osc_cali.attr,
	&dev_attr_gun_type.attr,
	&dev_attr_bullet_nr.attr,
	&dev_attr_haptic_audio.attr,
	NULL
};

struct attribute_group aw8622x_vibrator_attribute_group = {
	.attrs = aw8622x_vibrator_attributes
};



static int aw8622x_haptic_play_effect_seq(struct aw8622x *aw8622x,
					 unsigned char flag)
{
	pr_debug("%s: enter  \n", __func__);

	if (aw8622x->effect_id > aw8622x->dts_info.effect_id_boundary)
		return 0;
	pr_debug("%s:aw8622x->effect_id =%d\n", __func__, aw8622x->effect_id);
	pr_debug("%s:aw8622x->activate_mode =%d\n", __func__,
		 aw8622x->activate_mode);
	if (flag) {
		if (aw8622x->activate_mode == AW8622X_HAPTIC_ACTIVATE_RAM_MODE) {
			//Daniel 20210719 modify start
			//this mapping only used for K9B
			if(aw8622x->effect_id == 5 || aw8622x->effect_id == 6){
				aw8622x->effect_id = 6;
			}else if(aw8622x->effect_id == 1){
				aw8622x->effect_id = 3;
			}else if((aw8622x->effect_id >= 2)&&(aw8622x->effect_id <= 9)){
				aw8622x->effect_id = 8;
			}
			//Daniel 20210719 modify end
			aw8622x_haptic_set_wav_seq(aw8622x, 0x00,
						  (char)aw8622x->effect_id + 1);
			aw8622x_haptic_set_pwm(aw8622x,  AW8622X_PWM_12K);//Daniel 20210601 modify
			aw8622x_haptic_set_wav_seq(aw8622x, 0x01, 0x00);
			aw8622x_haptic_set_wav_loop(aw8622x, 0x00, 0x00);
			aw8622x_haptic_play_mode(aw8622x, AW8622X_HAPTIC_RAM_MODE);
			aw8622x_haptic_effect_strength(aw8622x);
			aw8622x_haptic_set_gain(aw8622x, aw8622x->level);
			aw8622x_haptic_start(aw8622x);
		}
		if (aw8622x->activate_mode ==
		    AW8622X_HAPTIC_ACTIVATE_RAM_LOOP_MODE) {
			aw8622x_haptic_set_repeat_wav_seq(aw8622x,
							 (aw8622x->dts_info.
							  effect_id_boundary +
							  1));
			//Daniel 20210716 modify start
			aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL7,
			AW8622X_BIT_SYSCTRL7_GAIN_BYPASS_MASK,
			AW8622X_BIT_SYSCTRL7_GAIN_CHANGEABLE);
			//Daniel 20210716 modify end
			aw8622x_haptic_set_pwm(aw8622x,  AW8622X_PWM_12K);//Daniel 20210601 modify
			aw8622x_haptic_set_gain(aw8622x, aw8622x->level);
			aw8622x_haptic_play_repeat_seq(aw8622x, true);
		}
	}
	pr_debug("%s: exit\n", __func__);
	return 0;
}

static void aw8622x_long_vibrate_work_routine(struct work_struct *work)
{
	struct aw8622x *aw8622x = container_of(work, struct aw8622x,
					       long_vibrate_work);

	aw_dev_dbg(aw8622x->dev, "%s enter\n", __func__);

	printk("%s: effect_id = %d state=%d activate_mode = %d duration = %d\n", __func__,
		aw8622x->effect_id, aw8622x->state, aw8622x->activate_mode, aw8622x->duration);

	mutex_lock(&aw8622x->lock);
	/* Enter standby mode */
	aw8622x_haptic_stop(aw8622x);
	aw8622x_haptic_upload_lra(aw8622x, AW8622X_F0_CALI);
	if (aw8622x->state) {
		if (aw8622x->activate_mode ==
			AW8622X_HAPTIC_ACTIVATE_RAM_MODE) {
			    //Daniel 20210716 midify start
			    aw8622x_haptic_ram_vbat_compensate(aw8622x, false);
			    //aw8622x_haptic_play_repeat_seq(aw8622x, true);
				aw8622x_haptic_play_effect_seq(aw8622x, true);
		} else if (aw8622x->activate_mode ==
			   AW8622X_HAPTIC_ACTIVATE_RAM_LOOP_MODE) {
			        aw8622x_haptic_ram_vbat_compensate(aw8622x, true);
					aw8622x_haptic_play_effect_seq(aw8622x, true);
			       //aw8622x_haptic_play_repeat_seq(aw8622x, true);
			/* run ms timer */
		        hrtimer_start(&aw8622x->timer,
			              ktime_set(aw8622x->duration / 1000,
					       (aw8622x->duration % 1000) * 1000000),
			      HRTIMER_MODE_REL);
				pm_stay_awake(aw8622x->dev);
			    aw8622x->wk_lock_flag = 1;
		} else if (aw8622x->activate_mode ==
			   AW8622X_HAPTIC_ACTIVATE_CONT_MODE) {
			aw_dev_err(aw8622x->dev, "%s mode:%s\n", __func__,
				    "AW8622X_HAPTIC_ACTIVATE_CONT_MODE");
			aw8622x_haptic_cont_config(aw8622x);
					/* run ms timer */
		             hrtimer_start(&aw8622x->timer,
			          ktime_set(aw8622x->duration / 1000,
					   (aw8622x->duration % 1000) * 1000000),
			      HRTIMER_MODE_REL);
		} else {
			aw_dev_err(aw8622x->dev, "%s: activate_mode error\n",
				   __func__);
		}
	}else {
		if (aw8622x->wk_lock_flag == 1) {
			       pm_relax(aw8622x->dev);
			       aw8622x->wk_lock_flag = 0;
	            }
	}
	mutex_unlock(&aw8622x->lock);
}

int aw8622x_vibrator_init(struct aw8622x *aw8622x)
{
	int ret = 0;

	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);

#ifdef TIMED_OUTPUT
	aw_dev_err(aw8622x->dev, "%s: TIMED_OUT FRAMEWORK!\n", __func__);
	aw8622x->vib_dev.name = "awinic_vibrator";
	aw8622x->vib_dev.get_time = aw8622x_vibrator_get_time;
	aw8622x->vib_dev.enable = aw8622x_vibrator_enable;

	ret = sysfs_create_group(&aw8622x->i2c->dev.kobj, &aw8622x_vibrator_attribute_group);
	if (ret < 0) {
		aw_dev_err(aw8622x->dev, "%s error creating bus sysfs attr files\n",
			   __func__);
		return ret;
	}
	ret = timed_output_dev_register(&(aw8622x->vib_dev));
	if (ret < 0) {
		aw_dev_err(aw8622x->dev,
			"%s: fail to create timed output dev\n", __func__);
		return ret;
	}
#else
	aw_dev_err(aw8622x->dev, "%s: loaded in leds_cdev framework!\n",
		    __func__);
	aw8622x->vib_dev.name = "awinic_vibrator";
	aw8622x->vib_dev.brightness_get = aw8622x_haptic_brightness_get;
	aw8622x->vib_dev.brightness_set = aw8622x_haptic_brightness_set;

	ret = sysfs_create_group(&aw8622x->i2c->dev.kobj, &aw8622x_vibrator_attribute_group);
	if (ret < 0) {
		aw_dev_err(aw8622x->dev, "%s error creating bus sysfs attr files\n",
			   __func__);
		return ret;
	}

	ret = devm_led_classdev_register(&aw8622x->i2c->dev, &aw8622x->vib_dev);
	if (ret < 0) {
		aw_dev_err(aw8622x->dev, "%s: fail to create led dev\n",
			   __func__);
		return ret;
	}

	ret = sysfs_create_link(&aw8622x->vib_dev.dev->kobj,
					&aw8622x->i2c->dev.kobj, "vibrator");
	if (ret < 0) {
		aw_dev_err(aw8622x->dev, "%s error creating class sysfs link attr files\n",
			   __func__);
		return ret;
	}
#endif
	hrtimer_init(&aw8622x->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw8622x->timer.function = aw8622x_vibrator_timer_func;
	INIT_WORK(&aw8622x->long_vibrate_work,
		  aw8622x_long_vibrate_work_routine);
	INIT_WORK(&aw8622x->rtp_work, aw8622x_rtp_work_routine);
	mutex_init(&aw8622x->lock);
	mutex_init(&aw8622x->rtp_lock);
	atomic_set(&aw8622x->is_in_rtp_loop, 0);
	atomic_set(&aw8622x->exit_in_rtp_loop, 0);
	init_waitqueue_head(&aw8622x->wait_q);
	init_waitqueue_head(&aw8622x->stop_wait_q);

	return 0;
}



static void aw8622x_haptic_misc_para_init(struct aw8622x *aw8622x)
{

	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);
	aw8622x->cont_drv1_lvl = aw8622x->dts_info.cont_drv1_lvl_dt;
	aw8622x->cont_drv2_lvl = aw8622x->dts_info.cont_drv2_lvl_dt;
	aw8622x->cont_drv1_time = aw8622x->dts_info.cont_drv1_time_dt;
	aw8622x->cont_drv2_time = aw8622x->dts_info.cont_drv2_time_dt;
	aw8622x->cont_brk_time = aw8622x->dts_info.cont_brk_time_dt;
	aw8622x->cont_wait_num = aw8622x->dts_info.cont_wait_num_dt;
	/* SIN_H */
	aw8622x_i2c_write(aw8622x, AW8622X_REG_SYSCTRL3,
			  aw8622x->dts_info.sine_array[0]);
	/* SIN_L */
	aw8622x_i2c_write(aw8622x, AW8622X_REG_SYSCTRL4,
			  aw8622x->dts_info.sine_array[1]);
	/* COS_H */
	aw8622x_i2c_write(aw8622x, AW8622X_REG_SYSCTRL5,
			  aw8622x->dts_info.sine_array[2]);
	/* COS_L */
	aw8622x_i2c_write(aw8622x, AW8622X_REG_SYSCTRL6,
			  aw8622x->dts_info.sine_array[3]);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_TRGCFG8,
				       AW8622X_BIT_TRGCFG8_TRG_TRIG1_MODE_MASK,
				       AW8622X_BIT_TRGCFG8_TRIG1);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_ANACFG8,
					AW8622X_BIT_ANACFG8_TRTF_CTRL_HDRV_MASK,
					AW8622X_BIT_ANACFG8_TRTF_CTRL_HDRV);

	/* d2s_gain */
	if (!aw8622x->dts_info.d2s_gain) {
		aw_dev_err(aw8622x->dev, "%s aw8622x->dts_info.d2s_gain = 0!\n",
			   __func__);
	} else {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL7,
				       AW8622X_BIT_SYSCTRL7_D2S_GAIN_MASK,
				       aw8622x->dts_info.d2s_gain);
	}

	/* cont_tset */
	if (!aw8622x->dts_info.cont_tset) {
		aw_dev_err(aw8622x->dev,
			"%s aw8622x->dts_info.cont_tset = 0!\n", __func__);
	} else {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG13,
				       AW8622X_BIT_CONTCFG13_TSET_MASK,
				       aw8622x->dts_info.cont_tset << 4);
	}

	/* cont_bemf_set */
	if (!aw8622x->dts_info.cont_bemf_set) {
		aw_dev_err(aw8622x->dev, "%s aw8622x->dts_info.cont_bemf_set = 0!\n",
			   __func__);
	} else {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG13,
				       AW8622X_BIT_CONTCFG13_BEME_SET_MASK,
				       aw8622x->dts_info.cont_bemf_set);
	}

	/* cont_brk_time */
	if (!aw8622x->cont_brk_time) {
		aw_dev_err(aw8622x->dev, "%s aw8622x->cont_brk_time = 0!\n",
			   __func__);
	} else {
		aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG10,
				  aw8622x->cont_brk_time);
	}

	/* cont_bst_brk_gain */
	/*
	** if (!aw8622x->dts_info.cont_bst_brk_gain) {
	**	aw_dev_err(aw8622x->dev,
	**		"%s aw8622x->dts_info.cont_bst_brk_gain = 0!\n",
	**		   __func__);
	** } else {
	**	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG5,
	**			       AW8622X_BIT_CONTCFG5_BST_BRK_GAIN_MASK,
	**			       aw8622x->dts_info.cont_bst_brk_gain);
	** }
	*/

	/* cont_brk_gain */
	if (!aw8622x->dts_info.cont_brk_gain) {
		aw_dev_err(aw8622x->dev, "%s aw8622x->dts_info.cont_brk_gain = 0!\n",
			   __func__);
	} else {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG5,
				       AW8622X_BIT_CONTCFG5_BRK_GAIN_MASK,
				       aw8622x->dts_info.cont_brk_gain);
	}
}

/*****************************************************
 *
 * offset calibration
 *
 *****************************************************/
static int aw8622x_haptic_offset_calibration(struct aw8622x *aw8622x)
{
	unsigned int cont = 2000;
	unsigned char reg_val = 0;

	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);

	aw8622x_haptic_raminit(aw8622x, true);

	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_DETCFG2,
			       AW8622X_BIT_DETCFG2_DIAG_GO_MASK,
			       AW8622X_BIT_DETCFG2_DIAG_GO_ON);
	while (1) {
		aw8622x_i2c_read(aw8622x, AW8622X_REG_DETCFG2, &reg_val);
		if ((reg_val & 0x01) == 0 || cont == 0)
			break;
		cont--;
	}
	if (cont == 0)
		aw_dev_err(aw8622x->dev, "%s calibration offset failed!\n",
			   __func__);
	aw8622x_haptic_raminit(aw8622x, false);
	return 0;
}

/*****************************************************
 *
 * vbat mode
 *
 *****************************************************/
static int aw8622x_haptic_vbat_mode_config(struct aw8622x *aw8622x,
					   unsigned char flag)
{
	if (flag == AW8622X_HAPTIC_CONT_VBAT_HW_ADJUST_MODE) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL1,
				       AW8622X_BIT_SYSCTRL1_VBAT_MODE_MASK,
				       AW8622X_BIT_SYSCTRL1_VBAT_MODE_HW);
	} else {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL1,
				       AW8622X_BIT_SYSCTRL1_VBAT_MODE_MASK,
				       AW8622X_BIT_SYSCTRL1_VBAT_MODE_SW);
	}
	return 0;
}

static void aw8622x_ram_work_routine(struct work_struct *work)
{
	struct aw8622x *aw8622x = container_of(work, struct aw8622x,
						ram_work.work);

	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);
	aw8622x_ram_update(aw8622x);
}

int aw8622x_ram_work_init(struct aw8622x *aw8622x)
{
	int ram_timer_val = 8000;

	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);
	INIT_DELAYED_WORK(&aw8622x->ram_work, aw8622x_ram_work_routine);
	schedule_delayed_work(&aw8622x->ram_work,
				msecs_to_jiffies(ram_timer_val));
	return 0;
}
static enum hrtimer_restart
aw8622x_haptic_audio_timer_func(struct hrtimer *timer)
{
	struct aw8622x *aw8622x = container_of(timer,
					struct aw8622x, haptic_audio.timer);

	aw_dev_dbg(aw8622x->dev, "%s enter\n", __func__);
	schedule_work(&aw8622x->haptic_audio.work);

	hrtimer_start(&aw8622x->haptic_audio.timer,
		ktime_set(aw8622x->haptic_audio.timer_val/1000000,
			(aw8622x->haptic_audio.timer_val%1000000)*1000),
		HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

static void
aw8622x_haptic_auto_bst_enable(struct aw8622x *aw8622x, unsigned char flag)
{
	if (flag) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
				AW8622X_BIT_PLAYCFG3_BRK_EN_MASK,
				AW8622X_BIT_PLAYCFG3_BRK_ENABLE);
	} else {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
				AW8622X_BIT_PLAYCFG3_BRK_EN_MASK,
				AW8622X_BIT_PLAYCFG3_BRK_DISABLE);
	}
}
int aw8622x_haptic_init(struct aw8622x *aw8622x)
{
	int ret = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);
	/* haptic audio */
	aw8622x->haptic_audio.delay_val = 1;
	aw8622x->haptic_audio.timer_val = 21318;
	INIT_LIST_HEAD(&(aw8622x->haptic_audio.ctr_list));
	hrtimer_init(&aw8622x->haptic_audio.timer,
		CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw8622x->haptic_audio.timer.function = aw8622x_haptic_audio_timer_func;
	INIT_WORK(&aw8622x->haptic_audio.work,
		aw8622x_haptic_audio_work_routine);
	mutex_init(&aw8622x->haptic_audio.lock);
	aw8622x->gun_type = 0xff;
	aw8622x->bullet_nr = 0x00;

	mutex_lock(&aw8622x->lock);
	/* haptic init */
	aw8622x->ram_state = 0;
	aw8622x->activate_mode = aw8622x->dts_info.mode;
	ret = aw8622x_i2c_read(aw8622x, AW8622X_REG_WAVCFG1, &reg_val);
	aw8622x->index = reg_val & 0x7F;
	ret = aw8622x_i2c_read(aw8622x, AW8622X_REG_PLAYCFG2, &reg_val);
	aw8622x->gain = reg_val & 0xFF;
	aw_dev_err(aw8622x->dev, "%s aw8622x->gain =0x%02X\n", __func__,
		    aw8622x->gain);
	for (i = 0; i < AW8622X_SEQUENCER_SIZE; i++) {
		ret = aw8622x_i2c_read(aw8622x, AW8622X_REG_WAVCFG1 + i,
				       &reg_val);
		aw8622x->seq[i] = reg_val;
	}
	aw8622x_haptic_play_mode(aw8622x, AW8622X_HAPTIC_STANDBY_MODE);
	aw8622x_haptic_set_pwm(aw8622x, AW8622X_PWM_12K);
	/* misc value init */
	aw8622x_haptic_misc_para_init(aw8622x);
	/* set motor protect */
	aw8622x_haptic_swicth_motor_protect_config(aw8622x, 0x00, 0x00);
	aw8622x_haptic_trig_config(aw8622x);
	aw8622x_haptic_offset_calibration(aw8622x);
	/*config auto_brake*/
	aw8622x_haptic_auto_bst_enable(aw8622x,
				       aw8622x->dts_info.is_enabled_auto_bst);
	/* vbat compensation */
	aw8622x_haptic_vbat_mode_config(aw8622x,
				AW8622X_HAPTIC_CONT_VBAT_HW_ADJUST_MODE);
	aw8622x->ram_vbat_compensate = AW8622X_HAPTIC_RAM_VBAT_COMP_ENABLE;

	/* f0 calibration */
	/*LRA trim source select register*/
	aw8622x_i2c_write_bits(aw8622x,
				AW8622X_REG_TRIMCFG1,
				AW8622X_BIT_TRIMCFG1_RL_TRIM_SRC_MASK,
				AW8622X_BIT_TRIMCFG1_RL_TRIM_SRC_REG);
	aw8622x_haptic_upload_lra(aw8622x, AW8622X_WRITE_ZERO);
	aw8622x_haptic_f0_calibration(aw8622x);
	mutex_unlock(&aw8622x->lock);
	return ret;
}

void aw8622x_interrupt_setup(struct aw8622x *aw8622x)
{
	unsigned char reg_val = 0;

	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);

	aw8622x_i2c_read(aw8622x, AW8622X_REG_SYSINT, &reg_val);

	aw_dev_err(aw8622x->dev, "%s: reg SYSINT=0x%02X\n", __func__, reg_val);

	/* edge int mode */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL7,
			       AW8622X_BIT_SYSCTRL7_INT_MODE_MASK,
			       AW8622X_BIT_SYSCTRL7_INT_MODE_EDGE);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL7,
			       AW8622X_BIT_SYSCTRL7_INT_EDGE_MODE_MASK,
			       AW8622X_BIT_SYSCTRL7_INT_EDGE_MODE_POS);
	/* int enable */
	/*
	*aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSINTM,
	*			AW8622X_BIT_SYSINTM_BST_SCPM_MASK,
	*			AW8622X_BIT_SYSINTM_BST_SCPM_OFF);
	*aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSINTM,
	*			AW8622X_BIT_SYSINTM_BST_OVPM_MASK,
	*		AW8622X_BIT_SYSINTM_BST_OVPM_ON);
	*/
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSINTM,
			       AW8622X_BIT_SYSINTM_UVLM_MASK,
			       AW8622X_BIT_SYSINTM_UVLM_ON);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSINTM,
			       AW8622X_BIT_SYSINTM_OCDM_MASK,
			       AW8622X_BIT_SYSINTM_OCDM_ON);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSINTM,
			       AW8622X_BIT_SYSINTM_OTM_MASK,
			       AW8622X_BIT_SYSINTM_OTM_ON);
}

irqreturn_t aw8622x_irq(int irq, void *data)
{
	struct aw8622x *aw8622x = data;
	unsigned char reg_val = 0;
	unsigned int buf_len = 0;
	unsigned char glb_state_val = 0;

	aw_dev_err(aw8622x->dev, "%s enter\n", __func__);
	atomic_set(&aw8622x->is_in_rtp_loop, 1);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_SYSINT, &reg_val);
	aw_dev_err(aw8622x->dev, "%s: reg SYSINT=0x%02X\n", __func__, reg_val);
	if (reg_val & AW8622X_BIT_SYSINT_UVLI)
		aw_dev_err(aw8622x->dev, "%s chip uvlo int error\n", __func__);
	if (reg_val & AW8622X_BIT_SYSINT_OCDI)
		aw_dev_err(aw8622x->dev, "%s chip over current int error\n",
			   __func__);
	if (reg_val & AW8622X_BIT_SYSINT_OTI)
		aw_dev_err(aw8622x->dev, "%s chip over temperature int error\n",
			   __func__);
	if (reg_val & AW8622X_BIT_SYSINT_DONEI)
		aw_dev_err(aw8622x->dev, "%s chip playback done\n", __func__);

	if (reg_val & AW8622X_BIT_SYSINT_FF_AEI) {
		aw_dev_err(aw8622x->dev, "%s: aw8622x rtp fifo almost empty\n",
			    __func__);
		if (aw8622x->rtp_init) {
			while ((!aw8622x_haptic_rtp_get_fifo_afs(aw8622x)) &&
			       (aw8622x->play_mode ==AW8622X_HAPTIC_RTP_MODE)
				  && !atomic_read(&aw8622x->exit_in_rtp_loop)) {
				mutex_lock(&aw8622x->rtp_lock);
				aw_dev_err(aw8622x->dev, "%s: aw8622x rtp mode fifo update, cnt=%d\n",
					    __func__, aw8622x->rtp_cnt);
				if (!aw8622x->rtp_container) {
					aw_dev_err(aw8622x->dev,
						"%s:aw8622x->rtp_container is null, break!\n",
						__func__);
					mutex_unlock(&aw8622x->rtp_lock);
					break;
				}
				if ((aw8622x->rtp_container->len - aw8622x->rtp_cnt) <
				    (aw8622x->ram.base_addr >> 2)) {
					buf_len =
					    aw8622x->rtp_container->len - aw8622x->rtp_cnt;
				} else {
					buf_len = (aw8622x->ram.base_addr >> 2);
				}
				aw8622x->rtp_update_flag =
				    aw8622x_i2c_writes(aw8622x,
						AW8622X_REG_RTPDATA,
						&aw8622x->rtp_container->data
						[aw8622x->rtp_cnt],
						buf_len);
				aw8622x->rtp_cnt += buf_len;
				aw8622x_i2c_read(aw8622x, AW8622X_REG_GLBRD5,
						 &glb_state_val);
				if ((aw8622x->rtp_cnt == aw8622x->rtp_container->len)
				    || ((glb_state_val & 0x0f) == 0)) {
					if (aw8622x->rtp_cnt ==
						aw8622x->rtp_container->len)
						aw_dev_err(aw8622x->dev,
							"%s: rtp load completely! glb_state_val=%02x aw8622x->rtp_cnt=%d\n",
							__func__, glb_state_val,
							aw8622x->rtp_cnt);
					else
						aw_dev_err(aw8622x->dev,
							"%s rtp load failed!! glb_state_val=%02x aw8622x->rtp_cnt=%d\n",
							__func__, glb_state_val,
							aw8622x->rtp_cnt);

					aw8622x_haptic_set_rtp_aei(aw8622x,
								false);
					aw8622x->rtp_cnt = 0;
					aw8622x->rtp_init = 0;
					mutex_unlock(&aw8622x->rtp_lock);
					break;
				}
				mutex_unlock(&aw8622x->rtp_lock);
			}
		} else {
			aw_dev_err(aw8622x->dev, "%s: aw8622x rtp init = %d, init error\n",
				    __func__, aw8622x->rtp_init);
		}
	}

	if (reg_val & AW8622X_BIT_SYSINT_FF_AFI)
		aw_dev_err(aw8622x->dev, "%s: aw8622x rtp mode fifo almost full!\n",
			    __func__);

	if (aw8622x->play_mode != AW8622X_HAPTIC_RTP_MODE
	    || atomic_read(&aw8622x->exit_in_rtp_loop)){
		aw8622x_haptic_set_rtp_aei(aw8622x, false);}

	atomic_set(&aw8622x->is_in_rtp_loop, 0);
	wake_up_interruptible(&aw8622x->wait_q);
	aw_dev_err(aw8622x->dev, "%s exit\n", __func__);

	return IRQ_HANDLED;
}

char aw8622x_check_qualify(struct aw8622x *aw8622x)
{
	unsigned char reg = 0;
	int ret = 0;

	ret = aw8622x_i2c_read(aw8622x, AW8622X_REG_EFRD9, &reg);
	if (ret < 0) {
		aw_dev_err(aw8622x->dev, "%s: failed to read register 0x64: %d\n",
			   __func__, ret);
		return ret;
	}
	if ((reg & 0x80) == 0x80)
		return 1;
	aw_dev_err(aw8622x->dev, "%s: register 0x64 error: 0x%02x\n",
			__func__, reg);
	return 0;
}



int aw8622x_haptics_upload_effect (struct input_dev *dev,
				   struct ff_effect *effect,
				   struct ff_effect *old)
{
       struct aw8622x *aw8622x = input_get_drvdata(dev);
       struct qti_hap_play_info *play = &aw8622x->play;
	s16 data[CUSTOM_DATA_LEN];
	ktime_t rem;
	s64 time_us;
	int ret;

       aw_dev_dbg(aw8622x->dev, "%s: enter\n", __func__);

       if (aw8622x->osc_cali_run != 0)
		return 0;

	if (hrtimer_active(&aw8622x->timer)) {
		rem = hrtimer_get_remaining(&aw8622x->timer);
		time_us = ktime_to_us(rem);
		printk("waiting for playing clear sequence: %lld us\n",
		       time_us);
		usleep_range(time_us, time_us + 100);
	}

	aw_dev_dbg(aw8622x->dev, "%s: effect->type=0x%x,FF_CONSTANT=0x%x,FF_PERIODIC=0x%x\n",
		__func__, effect->type, FF_CONSTANT, FF_PERIODIC);
	aw8622x->effect_type = effect->type;
	mutex_lock(&aw8622x->lock);
	while (atomic_read(&aw8622x->exit_in_rtp_loop)) {
		pr_info("%s:  goint to waiting rtp  exit\n", __func__);
		mutex_unlock(&aw8622x->lock);
		ret =
		    wait_event_interruptible(aw8622x->stop_wait_q,
					     atomic_read(&aw8622x->
							 exit_in_rtp_loop) ==
					     0);
		pr_info("%s:  wakeup \n", __func__);
		if (ret == -ERESTARTSYS) {
			mutex_unlock(&aw8622x->lock);
			pr_err("%s: wake up by signal return erro\n", __func__);
			return ret;
		}
		mutex_lock(&aw8622x->lock);
	}
	if (aw8622x->effect_type == FF_CONSTANT) {
		pr_debug("%s:  effect_type is  FF_CONSTANT! \n", __func__);
		/*cont mode set duration */
		aw8622x->duration = effect->replay.length;
		aw8622x->activate_mode = AW8622X_HAPTIC_ACTIVATE_RAM_LOOP_MODE;
		aw8622x->effect_id = aw8622x->dts_info.effect_id_boundary;

	} else if (aw8622x->effect_type == FF_PERIODIC) {
		if (aw8622x->effects_count == 0) {
			mutex_unlock(&aw8622x->lock);
			return -EINVAL;
		}

		pr_debug("%s:  effect_type is  FF_PERIODIC! \n", __func__);
		if (copy_from_user(data, effect->u.periodic.custom_data,
				   sizeof(s16) * CUSTOM_DATA_LEN)) {
			mutex_unlock(&aw8622x->lock);
			return -EFAULT;
		}

		aw8622x->effect_id = data[0];
		//this mapping for aw8624 effect_id 21
		if (aw8622x->effect_id == 521) {
			aw8622x->effect_id = 21;
		}
		pr_debug("%s: aw8622x->effect_id =%d \n", __func__,
			 aw8622x->effect_id);
		play->vmax_mv = effect->u.periodic.magnitude;	/*vmax level */
		//if (aw8624->info.gain_flag == 1)
		//      play->vmax_mv = AW8624_LIGHT_MAGNITUDE;
		//printk("%s  %d  aw8624->play.vmax_mv = 0x%x\n", __func__, __LINE__, aw8624->play.vmax_mv);

		if (aw8622x->effect_id < 0 ||
		    aw8622x->effect_id > aw8622x->dts_info.effect_max) {
			mutex_unlock(&aw8622x->lock);
			return 0;
		}

		if (aw8622x->effect_id < aw8622x->dts_info.effect_id_boundary) {
			aw8622x->activate_mode = AW8622X_HAPTIC_ACTIVATE_RAM_MODE;
			aw_dev_dbg
			    (aw8622x->dev, "%s: aw8622x->effect_id=%d , aw8622x->activate_mode = %d\n",
			     __func__, aw8622x->effect_id,
			     aw8622x->activate_mode);
			data[1] = aw8622x->predefined[aw8622x->effect_id].play_rate_us / 1000000;	/*second data */
			data[2] = aw8622x->predefined[aw8622x->effect_id].play_rate_us / 1000;	/*millisecond data */
			pr_debug
			    ("%s: aw8622x->predefined[aw8622x->effect_id].play_rate_us/1000 = %d\n",
			     __func__,
			     aw8622x->predefined[aw8622x->effect_id].
			     play_rate_us / 1000);
		}
		if (aw8622x->effect_id >= aw8622x->dts_info.effect_id_boundary) {
			aw8622x->activate_mode = AW8622X_HAPTIC_ACTIVATE_RTP_MODE;
			pr_info
			    ("%s: aw8622x->effect_id=%d , aw8622x->activate_mode = %d\n",
			     __func__, aw8622x->effect_id,
			     aw8622x->activate_mode);
			data[1] = aw8622x->dts_info.rtp_time[aw8622x->effect_id] / 1000;	/*second data */
			data[2] = aw8622x->dts_info.rtp_time[aw8622x->effect_id] % 1000;	/*millisecond data */
			pr_debug("%s: data[1] = %d data[2] = %d, rtp_time %d\n", __func__,
				 data[1], data[2], aw8622x->dts_info.rtp_time[aw8622x->effect_id]);
		}

		if (copy_to_user(effect->u.periodic.custom_data, data,
				 sizeof(s16) * CUSTOM_DATA_LEN)) {
			mutex_unlock(&aw8622x->lock);
			return -EFAULT;
		}

	} else {
		pr_err("%s: Unsupported effect type: %d\n", __func__,
		       effect->type);
	}
	mutex_unlock(&aw8622x->lock);
	pr_debug("%s	%d	aw8622x->effect_type= 0x%x\n", __func__,
		 __LINE__, aw8622x->effect_type);
	return 0;
}

int aw8622x_haptics_playback(struct input_dev *dev, int effect_id,
			     int val)
{
	struct aw8622x *aw8622x = input_get_drvdata(dev);
	int rc = 0;
	pr_debug("%s:  %d enter\n", __func__, __LINE__);

	aw_dev_dbg(aw8622x->dev, "%s: effect_id=%d , val = %d\n", __func__, effect_id, val);
	aw_dev_dbg(aw8622x->dev, "%s: aw8622x->effect_id=%d , aw8622x->activate_mode = %d\n",
		__func__, aw8622x->effect_id, aw8622x->activate_mode);

	/*for osc calibration */
	if (aw8622x->osc_cali_run != 0)
		return 0;

	if (val > 0)
		aw8622x->state = 1;
	if (val <= 0)
		aw8622x->state = 0;
	hrtimer_cancel(&aw8622x->timer);

	if (aw8622x->effect_type == FF_CONSTANT &&
	    aw8622x->activate_mode == AW8622X_HAPTIC_ACTIVATE_RAM_LOOP_MODE) {
		pr_info("%s: enter ram_loop_mode \n", __func__);
		//schedule_work(&aw8624->long_vibrate_work);
		queue_work(aw8622x->work_queue, &aw8622x->long_vibrate_work);
	} else if (aw8622x->effect_type == FF_PERIODIC &&
		   aw8622x->activate_mode == AW8622X_HAPTIC_ACTIVATE_RAM_MODE) {
		aw_dev_dbg(aw8622x->dev, "%s: enter  ram_mode\n", __func__);
		//schedule_work(&aw8624->long_vibrate_work)
		queue_work(aw8622x->work_queue, &aw8622x->long_vibrate_work);;
	} else if (aw8622x->effect_type == FF_PERIODIC &&
		   aw8622x->activate_mode == AW8622X_HAPTIC_ACTIVATE_RTP_MODE) {
		pr_info("%s: enter  rtp_mode\n", __func__);
		//schedule_work(&aw8624->rtp_work);
		queue_work(aw8622x->work_queue, &aw8622x->rtp_work);
		//if we are in the play mode, force to exit
		if (val == 0) {
			atomic_set(&aw8622x->exit_in_rtp_loop, 1);
		}
	} else {
		/*other mode */
	}

	return rc;
}

int aw8622x_haptics_erase(struct input_dev *dev, int effect_id)
{
	struct aw8622x *aw8622x = input_get_drvdata(dev);
	int rc = 0;

	/*for osc calibration */
	if (aw8622x->osc_cali_run != 0)
		return 0;

	pr_debug("%s: enter\n", __func__);
	aw8622x->effect_type = 0;
	aw8622x->duration = 0;
	return rc;
}

void aw8622x_haptics_set_gain_work_routine(struct work_struct *work)
{
	unsigned char comp_level = 0;
	struct aw8622x *aw8622x =
	    container_of(work, struct aw8622x, set_gain_work);

	if (aw8622x->new_gain >= 0x7FFF)
		aw8622x->level = 0x80;	/*128 */
	else if (aw8622x->new_gain <= 0x3FFF)
		aw8622x->level = 0x1E;	/*30 */
	else
		aw8622x->level = (aw8622x->new_gain - 16383) / 128;

	if (aw8622x->level < 0x1E)
		aw8622x->level = 0x1E;	/*30 */
	pr_info("%s: set_gain queue work, new_gain = %x level = %x \n", __func__,
		aw8622x->new_gain, aw8622x->level);

	if (aw8622x->ram_vbat_compensate == AW8622X_HAPTIC_RAM_VBAT_COMP_ENABLE
		&& aw8622x->vbat)
	{
		pr_debug("%s: ref %d vbat %d ", __func__, AW8622X_VBAT_REFER,
				aw8622x->vbat);
		comp_level = aw8622x->level * AW8622X_VBAT_REFER / aw8622x->vbat;
		if (comp_level > (128 * AW8622X_VBAT_REFER / AW8622X_VBAT_MIN)) {
			comp_level = 128 * AW8622X_VBAT_REFER / AW8622X_VBAT_MIN;
			pr_debug("%s: comp level limit is %d ", __func__, comp_level);
		}
		pr_info("%s: enable vbat comp, level = %x comp level = %x", __func__,
			   aw8622x->level, comp_level);
		aw8622x_i2c_write(aw8622x, AW8622X_REG_PLAYCFG2, comp_level);//Daniel 20210716
	} else {
		pr_debug("%s: disable compsensation, vbat=%d, vbat_min=%d, vbat_ref=%d",
				__func__, aw8622x->vbat, AW8622X_VBAT_MIN, AW8622X_VBAT_REFER);
		aw8622x_i2c_write(aw8622x, AW8622X_REG_PLAYCFG2, aw8622x->level);//Daniel 20210716
	}
}

void aw8622x_haptics_set_gain(struct input_dev *dev, u16 gain)
{
	struct aw8622x *aw8622x = input_get_drvdata(dev);
	pr_debug("%s: enter\n", __func__);
	aw8622x->new_gain = gain;
	queue_work(aw8622x->work_queue, &aw8622x->set_gain_work);
}
