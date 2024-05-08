/*
 *  Silicon Integrated Co., Ltd haptic sih688x haptic driver file
 *
 *  Copyright (c) 2021 kugua <canzhen.peng@si-in.com>
 *  Copyright (c) 2021 tianchi <tianchi.zheng@si-in.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation
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
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/power_supply.h>
#include <linux/vmalloc.h>
#include <linux/pm_qos.h>
#include <linux/mm.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <linux/errno.h>
#include <linux/mman.h>
#include "ringbuffer.h"
#include "haptic_mid.h"
#include "haptic_regmap.h"
#include "haptic.h"
#include "haptic_misc.h"
#include "sih688x.h"
#include "sih688x_reg.h"
#include "sih688x_func_config.h"
#include "xm-haptic.h"

/*****************************************************
 *
 * variable
 *
 *****************************************************/
char awinic_ram_name[][SIH_RTP_NAME_MAX] = {
	{"aw8697_haptic.bin"},
};

char awinic_rtp_name[][SIH_RTP_NAME_MAX] = {
	{"aw8697_rtp_1.bin"}, /*8*/
	{"aw8697_rtp_1.bin"},
	{"aw8697_rtp_1.bin"},
	{"aw8697_rtp_1.bin"},
	{"aw8697_rtp_1.bin"},
	{"aw8697_rtp_1.bin"},
	{"aw8697_rtp_1.bin"},
	{"aw8697_rtp_1.bin"},
	{"aw8697_rtp_1.bin"},
	{"aw8697_rtp_1.bin"},
	{"aw8697_rtp_1.bin"},
	/*{"aw8697_rtp_1.bin"},*/
	/*{"aw8697_rtp_1.bin"},*/
	{"AcousticGuitar_RTP.bin"}, /*21*/
	{"Blues_RTP.bin"},
	{"Candy_RTP.bin"},
	{"Carousel_RTP.bin"},
	{"Celesta_RTP.bin"},
	{"Childhood_RTP.bin"},
	{"Country_RTP.bin"},
	{"Cowboy_RTP.bin"},
	{"Echo_RTP.bin"},
	{"Fairyland_RTP.bin"},
	{"Fantasy_RTP.bin"},
	{"Field_Trip_RTP.bin"},
	{"Glee_RTP.bin"},
	{"Glockenspiel_RTP.bin"},
	{"Ice_Latte_RTP.bin"},
	{"Kung_Fu_RTP.bin"},
	{"Leisure_RTP.bin"},
	{"Lollipop_RTP.bin"},
	{"MiMix2_RTP.bin"},
	{"Mi_RTP.bin"},
	{"MiHouse_RTP.bin"},
	{"MiJazz_RTP.bin"},
	{"MiRemix_RTP.bin"},
	{"Mountain_Spring_RTP.bin"},
	{"Orange_RTP.bin"},
	{"WindChime_RTP.bin"},
	{"Space_Age_RTP.bin"},
	{"ToyRobot_RTP.bin"},
	{"Vigor_RTP.bin"},
	{"Bottle_RTP.bin"},
	{"Bubble_RTP.bin"},
	{"Bullfrog_RTP.bin"},
	{"Burst_RTP.bin"},
	{"Chirp_RTP.bin"},
	{"Clank_RTP.bin"},
	{"Crystal_RTP.bin"},
	{"FadeIn_RTP.bin"},
	{"FadeOut_RTP.bin"},
	{"Flute_RTP.bin"},
	{"Fresh_RTP.bin"},
	{"Frog_RTP.bin"},
	{"Guitar_RTP.bin"},
	{"Harp_RTP.bin"},
	{"IncomingMessage_RTP.bin"},
	{"MessageSent_RTP.bin"},
	{"Moment_RTP.bin"},
	{"NotificationXylophone_RTP.bin"},
	{"Potion_RTP.bin"},
	{"Radar_RTP.bin"},
	{"Spring_RTP.bin"},
	{"Swoosh_RTP.bin"}, /*71*/
	{"Gesture_UpSlide_RTP.bin"},
	{"FOD_Motion_Planet_RTP.bin"},
	{"Charge_Wire_RTP.bin"},
	{"Charge_Wireless_RTP.bin"},
	{"Unlock_Failed_RTP.bin"},
	{"FOD_Motion1_RTP.bin"},
	{"FOD_Motion2_RTP.bin"},
	{"FOD_Motion3_RTP.bin"},
	{"FOD_Motion4_RTP.bin"},
	{"FOD_Motion_Aurora_RTP.bin"},
	{"FaceID_Wrong2_RTP.bin"}, /*82*/
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
	{"aw8697_rtp_1.bin"}, /*99*/
	{"aw8697_rtp_1.bin"}, /*100*/
	{"offline_countdown_RTP.bin"},
	{"scene_bomb_injury_RTP.bin"},
	{"scene_bomb_RTP.bin"}, /*103*/
	{"door_open_RTP.bin"},
	{"aw8697_rtp_1.bin"},
	{"scene_step_RTP.bin"}, /*106*/
	{"crawl_RTP.bin"},
	{"scope_on_RTP.bin"},
	{"scope_off_RTP.bin"},
	{"magazine_quick_RTP.bin"},
	{"grenade_RTP.bin"},
	{"scene_getshot_RTP.bin"}, /*112*/
	{"grenade_explosion_RTP.bin"},
	{"punch_RTP.bin"},
	{"pan_RTP.bin"},
	{"bandage_RTP.bin"},
	{"aw8697_rtp_1.bin"},
	{"scene_jump_RTP.bin"},
	{"vehicle_plane_RTP.bin"}, /*119*/
	{"scene_openparachute_RTP.bin"}, /*120*/
	{"scene_closeparachute_RTP.bin"}, /*121*/
	{"vehicle_collision_RTP.bin"},
	{"vehicle_buggy_RTP.bin"}, /*123*/
	{"vehicle_dacia_RTP.bin"}, /*124*/
	{"vehicle_moto_RTP.bin"}, /*125*/
	{"firearms_akm_RTP.bin"}, /*126*/
	{"firearms_m16a4_RTP.bin"}, /*127*/
	{"aw8697_rtp_1.bin"},
	{"firearms_awm_RTP.bin"}, /*129*/
	{"firearms_mini14_RTP.bin"}, /*130*/
	{"firearms_vss_RTP.bin"}, /*131*/
	{"firearms_qbz_RTP.bin"}, /*132*/
	{"firearms_ump9_RTP.bin"}, /*133*/
	{"firearms_dp28_RTP.bin"}, /*134*/
	{"firearms_s1897_RTP.bin"}, /*135*/
	{"aw8697_rtp_1.bin"},
	{"firearms_p18c_RTP.bin"}, /*137*/
	{"aw8697_rtp_1.bin"},
	{"aw8697_rtp_1.bin"},
	{"CFM_KillOne_RTP.bin"},
	{"CFM_Headshot_RTP.bin"}, /*141*/
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
	{"CFM_Weapon_BLT_Shoot_RTP.bin"}, /*153*/
	{"Atlantis_RTP.bin"},
	{"DigitalUniverse_RTP.bin"},
	{"Reveries_RTP.bin"},
	{"FOD_Motion_Triang_RTP.bin"},
	{"FOD_Motion_Flare_RTP.bin"},
	{"FOD_Motion_Ripple_RTP.bin"},
	{"FOD_Motion_Spiral_RTP.bin"},
	{"gamebox_launch_rtp.bin"}, /*161*/
	{"Gesture_Back_Pull_RTP.bin"}, /*162*/
	{"Gesture_Back_Release_RTP.bin"}, /*163*/
	{"alert_rtp.bin"}, /*164*/
	{"feedback_negative_light_rtp.bin"}, /*165*/
	{"feedback_neutral_rtp.bin"}, /*166*/
	{"feedback_positive_rtp.bin"}, /*167*/
	{"fingerprint_record_rtp.bin"}, /*168*/
	{"lockdown_rtp.bin"}, /*169*/
	{"sliding_damping_rtp.bin"}, /*170*/
	{"todo_alldone_rtp.bin"}, /*171*/
	{"uninstall_animation_icon_rtp.bin"}, /*172*/
	{"signal_button_highlight_rtp.bin"}, /*173*/
	{"signal_button_negative_rtp.bin"},
	{"signal_button_rtp.bin"},
	{"signal_clock_high_rtp.bin"}, /*176*/
	{"signal_clock_rtp.bin"},
	{"signal_clock_unit_rtp.bin"},
	{"signal_inputbox_rtp.bin"},
	{"signal_key_high_rtp.bin"},
	{"signal_key_unit_rtp.bin"}, /*181*/
	{"signal_list_highlight_rtp.bin"},
	{"signal_list_rtp.bin"},
	{"signal_picker_rtp.bin"},
	{"signal_popup_rtp.bin"},
	{"signal_seekbar_rtp.bin"}, /*186*/
	{"signal_switch_rtp.bin"},
	{"signal_tab_rtp.bin"},
	{"signal_text_rtp.bin"},
	{"signal_transition_light_rtp.bin"},
	{"signal_transition_rtp.bin"}, /*191*/
	{"haptics_video_rtp.bin"}, /*192*/
	{"keyboard_clicky_down_rtp.bin"},
	{"keyboard_clicky_up_rtp.bin"},
	{"keyboard_linear_down_rtp.bin"},
	{"keyboard_linear_up_rtp.bin"}, /*196*/
};
int CUSTOME_WAVE_ID;
int awinic_rtp_name_len = sizeof(awinic_rtp_name) / SIH_RTP_NAME_MAX;
static sih_haptic_ptr_t g_haptic_t;
/*****************************************************
 *
 * parse dts
 *
 *****************************************************/
static int sih_parse_hw_dts(struct device *dev, sih_haptic_t *sih_haptic,
	struct device_node *np)
{
	struct device_node *sih_node = np;

	if (sih_node == NULL) {
		hp_err("%s:haptic device node acquire failed\n", __func__);
		return -EINVAL;
	}

	/* acquire reset gpio */
	sih_haptic->chip_attr.reset_gpio =
		of_get_named_gpio(sih_node, "reset-gpio", 0);
	if (sih_haptic->chip_attr.reset_gpio < 0) {
		hp_err("%s:reset gpio acquire failed\n", __func__);
		return -EIO;
	}

	/* acquire irq gpio */
	sih_haptic->chip_attr.irq_gpio =
		of_get_named_gpio(sih_node, "irq-gpio", 0);
	if (sih_haptic->chip_attr.irq_gpio < 0) {
		hp_err("%s:irq gpio acquire failed\n", __func__);
		return -EIO;
	}

	hp_info("%s:reset_gpio = %d, irq_gpio = %d\n", __func__,
		sih_haptic->chip_attr.reset_gpio, sih_haptic->chip_attr.irq_gpio);

	return 0;
}

static int sih_parse_lra_dts(struct device *dev, sih_haptic_t *sih_haptic,
	struct device_node *np)
{
	struct device_node *sih_node = np;
	const char *str = NULL;
	int ret = -1;
	unsigned int val = 0;

	if (sih_node == NULL) {
		hp_err("%s:haptic device node acquire failed\n", __func__);
		return -EINVAL;
	}

	/* acquire lra msg */
	ret = of_property_read_string(sih_node, "lra_name", &str);
	if (ret) {
		hp_err("%s:lra name acquire failed\n", __func__);
		return -EIO;
	}
	strlcpy(sih_haptic->chip_attr.lra_name, str, SIH_LRA_NAME_LEN);
	hp_info("%s:lra_name = %s\n", __func__, sih_haptic->chip_attr.lra_name);

	val = of_property_read_u32(np, "vib_f0_pre",
				   &sih_haptic->detect.cali_target_value);
	if (val != 0) {
		hp_info("%s: vib_f0_pre not found\n", __func__);
		XM_HAP_F0_PROTECT_EXCEPTION(0, "SIH：haptics_get_closeloop_lra_period");
	}

	val = of_property_read_u32(np, "vib_effect_id_boundary",
				 &sih_haptic->chip_ipara.effect_id_boundary);
	if (val != 0)
		hp_info("%s: vib_effect_id_boundary not found\n", __func__);

	val = of_property_read_u32(np, "vib_effect_max",
				 &sih_haptic->chip_ipara.effect_max);
	if (val != 0)
		hp_info("%s: vib_effect_max not found\n", __func__);

	val = of_property_read_u32(np, "vib_bst_vol_default",
				   &sih_haptic->chip_ipara.drv_vboost);
	if (val != 0)
		hp_info("%s: vib_bst_vol_default not found\n", __func__);

	hp_info("%s: default drv bst = %d\n", __func__, sih_haptic->chip_ipara.drv_vboost);
	return 0;
}

static int sih_parse_dts(struct device *dev, sih_haptic_t *sih_haptic,
	struct device_node *np)
{
	int ret = -1;

	/* Obtain DTS information and data */
	if (np) {
		ret = sih_parse_hw_dts(dev, sih_haptic, np);
		if (ret) {
			hp_err("%s:dts acquire failed hw\n", __func__);
			return ret;
		}
		ret = sih_parse_lra_dts(dev, sih_haptic, np);
		if (ret) {
			hp_err("%s:dts acquire failed dts\n", __func__);
			XM_HAP_REGISTER_EXCEPTION("DT", "haptics_parse_dt");
			return ret;
		}
	} else {
		sih_haptic->chip_attr.reset_gpio = -1;
		sih_haptic->chip_attr.irq_gpio = -1;
	}

	return 0;
}

static void sih_hardware_reset(sih_haptic_t *sih_haptic)
{
	if (gpio_is_valid(sih_haptic->chip_attr.reset_gpio)) {
		gpio_set_value(sih_haptic->chip_attr.reset_gpio, SIH_RESET_GPIO_RESET);
		usleep_range(1000, 2000);
		gpio_set_value(sih_haptic->chip_attr.reset_gpio, SIH_RESET_GPIO_SET);
		usleep_range(1000, 2000);
	}
}

static int sih_acquire_prepare_res(struct device *dev,
	sih_haptic_t *sih_haptic)
{
	int ret = -1;

	if (gpio_is_valid(sih_haptic->chip_attr.irq_gpio)) {
		ret = devm_gpio_request_one(dev, sih_haptic->chip_attr.irq_gpio,
			GPIOF_DIR_IN, "sih_haptic_irq");
		if (ret) {
			hp_err("%s:irq gpio request failed\n", __func__);
			return ret;
		}
	}

	if (gpio_is_valid(sih_haptic->chip_attr.reset_gpio)) {
		ret = devm_gpio_request_one(dev, sih_haptic->chip_attr.reset_gpio,
			GPIOF_OUT_INIT_LOW, "sih_haptic_rst");
		if (ret) {
			hp_err("%s:reset gpio request failed\n", __func__);
			return ret;
		}
	}

	sih_hardware_reset(sih_haptic);

	return ret;
}

static void sih_chip_state_recovery(sih_haptic_t *sih_haptic)
{
	sih_haptic->chip_ipara.state = SIH_STANDBY_MODE;
	sih_haptic->chip_ipara.play_mode = SIH_IDLE_MODE;
}

/*****************************************************
 *
 * rtp
 *
 *****************************************************/
static bool sih_irq_rtp_local_file_handle(sih_haptic_t *sih_haptic,
	haptic_container_t *rtp_cont)
{
	uint32_t buf_len = 0;
	uint32_t cont_len = 0;
	uint32_t inject_data_cnt;
	int ret = -1;

	/* inject 1/4 fifo size data once max */
	inject_data_cnt = sih_haptic->ram.base_addr >> 2;
	mutex_lock(&sih_haptic->rtp.rtp_lock);
	if (!rtp_cont) {
		hp_err("%s:rtp_container is null, break!\n", __func__);
		sih_haptic->hp_func->set_rtp_aei(sih_haptic, false);
		sih_haptic->rtp.rtp_init = false;
		sih_haptic->rtp.rtp_cnt = 0;
		pm_relax(sih_haptic->dev);
		sih_chip_state_recovery(sih_haptic);
		mutex_unlock(&sih_haptic->rtp.rtp_lock);
		return false;
	}
	if (sih_haptic->chip_ipara.is_custom_wave == 0) {
		cont_len = rtp_cont->len;
		if ((cont_len - sih_haptic->rtp.rtp_cnt) < inject_data_cnt)
			buf_len = cont_len - sih_haptic->rtp.rtp_cnt;
		else
			buf_len = inject_data_cnt;
		hp_err("%s: cont_len = %d,sih_haptic->rtp.rtp_cnt = %d,inject_data_cnt = %d\n", __func__, cont_len, sih_haptic->rtp.rtp_cnt, inject_data_cnt);
		if(buf_len > 0){
			ret = sih_haptic->hp_func->write_rtp_data(sih_haptic,
			&rtp_cont->data[sih_haptic->rtp.rtp_cnt], buf_len);
			if (ret < 0) {
				sih_haptic->hp_func->stop(sih_haptic);
				sih_haptic->hp_func->set_rtp_aei(sih_haptic, false);
				sih_haptic->rtp.rtp_init = false;
				sih_haptic->rtp.rtp_cnt = 0;
				pm_relax(sih_haptic->dev);
				mutex_unlock(&sih_haptic->rtp.rtp_lock);
				hp_err("%s:i2c write rtp data failed,buf_len = %d \n", __func__,buf_len);
				return false;
			}
		}
		sih_haptic->rtp.rtp_cnt += buf_len;
		hp_info("%s: write %d, total write %d, file length %d\n",
				__func__, buf_len, sih_haptic->rtp.rtp_cnt, cont_len);
		if ((sih_haptic->rtp.rtp_cnt == cont_len) ||
			sih_haptic->hp_func->if_chip_is_mode(sih_haptic, SIH_IDLE_MODE)) {
			if (sih_haptic->rtp.rtp_cnt != cont_len)
				hp_err("%s:rtp play error suspend!\n", __func__);
			else
				hp_info("%s:rtp update complete!\n", __func__);
			sih_haptic->hp_func->set_rtp_aei(sih_haptic, false);
			sih_haptic->rtp.rtp_init = false;
			sih_haptic->rtp.rtp_cnt = 0;
			pm_relax(sih_haptic->dev);
			sih_chip_state_recovery(sih_haptic);
			mutex_unlock(&sih_haptic->rtp.rtp_lock);
			return false;
		}
	} else if (sih_haptic->chip_ipara.is_custom_wave == 1) {
		buf_len = read_rb(rtp_cont->data, inject_data_cnt);
		if(buf_len >0){
			ret = sih_haptic->hp_func->write_rtp_data(sih_haptic,
				rtp_cont->data, buf_len);
			if (ret < 0) {
				sih_haptic->hp_func->stop(sih_haptic);
				sih_haptic->hp_func->set_rtp_aei(sih_haptic, false);
				sih_haptic->rtp.rtp_init = false;
				sih_haptic->rtp.rtp_cnt = 0;
				pm_relax(sih_haptic->dev);
				mutex_unlock(&sih_haptic->rtp.rtp_lock);
				hp_err("%s:i2c write rtp data failed,buf_len = %d\n", __func__,buf_len);
				return false;
			}
			if (buf_len < inject_data_cnt) {
				hp_info("%s: rtp update complete\n", __func__);
				sih_haptic->hp_func->set_rtp_aei(sih_haptic, false);
				sih_haptic->rtp.rtp_init = false;
				sih_haptic->rtp.rtp_cnt = 0;
				pm_relax(sih_haptic->dev);
				sih_chip_state_recovery(sih_haptic);
				mutex_unlock(&sih_haptic->rtp.rtp_lock);
				return false;
			}
		}
	}
	mutex_unlock(&sih_haptic->rtp.rtp_lock);

	return true;
}

static irqreturn_t sih_irq_isr(int irq, void *data)
{
	sih_haptic_t *sih_haptic = data;
	haptic_container_t *rtp_cont = sih_haptic->rtp.rtp_cont;

	hp_info("%s:enter! interrupt code number is %d\n", __func__, irq);

	if (sih_haptic->stream_func->is_stream_mode(sih_haptic))
		return IRQ_HANDLED;

	atomic_set(&sih_haptic->rtp.is_in_rtp_loop, 1);
	if (sih_haptic->hp_func->get_rtp_fifo_empty_state(sih_haptic)) {
		if (sih_haptic->rtp.rtp_init) {
			while ((!sih_haptic->hp_func->get_rtp_fifo_full_state(sih_haptic)) &&
					(sih_haptic->chip_ipara.play_mode == SIH_RTP_MODE) &&
					!atomic_read(&sih_haptic->rtp.exit_in_rtp_loop)) {
				if (!sih_irq_rtp_local_file_handle(sih_haptic, rtp_cont))
					break;
			}
		} else {
			hp_err("%s: rtp init false\n", __func__);
		}
	}

	if (sih_haptic->chip_ipara.play_mode != SIH_RTP_MODE ||
		atomic_read(&sih_haptic->rtp.exit_in_rtp_loop))
		sih_haptic->hp_func->set_rtp_aei(sih_haptic, false);

	/* detect */
	if ((sih_haptic->detect.trig_detect_en | sih_haptic->detect.ram_detect_en |
		sih_haptic->detect.rtp_detect_en | sih_haptic->detect.cont_detect_en) &&
		(sih_haptic->hp_func->if_chip_is_detect_done(sih_haptic))) {
		hp_info("%s:if chip is detect done\n", __func__);
		sih_haptic->hp_func->ram_init(sih_haptic, true);
		sih_haptic->hp_func->read_detect_fifo(sih_haptic);
		sih_haptic->hp_func->ram_init(sih_haptic, false);
		sih_haptic->hp_func->detect_fifo_ctrl(sih_haptic, false);
		sih_haptic->detect.detect_f0_read_done = true;
	}
	atomic_set(&sih_haptic->rtp.is_in_rtp_loop, 0);
	wake_up_interruptible(&sih_haptic->rtp.wait_q);
	hp_info("%s:exit\n", __func__);
	return IRQ_HANDLED;
}

static int sih_acquire_irq_res(struct device *dev, sih_haptic_t *sih_haptic)
{
	int ret = -1;
	int irq_flags;

	sih_haptic->hp_func->interrupt_state_init(sih_haptic);

	irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;

	ret = devm_request_threaded_irq(dev,
		gpio_to_irq(sih_haptic->chip_attr.irq_gpio), NULL, sih_irq_isr,
		irq_flags, sih_haptic->soft_frame.vib_dev.name, sih_haptic);

	if (ret != 0)
		hp_err("%s: irq gpio interrupt request failed\n", __func__);

	return ret;
}

static void sih_vfree_container(sih_haptic_t *sih_haptic,
	haptic_container_t *cont)
{
	vfree(cont);
}

static void sih_rtp_play_func(sih_haptic_t *sih_haptic, uint8_t mode)
{
	uint32_t buf_len = 0;
	uint32_t cont_len = 0;
	unsigned int period_size = sih_haptic->ram.base_addr >> 2;
	haptic_container_t *rtp_cont = sih_haptic->rtp.rtp_cont;

	if (!rtp_cont) {
		hp_err("%s:cont is null\n", __func__);
		sih_chip_state_recovery(sih_haptic);
		return;
	}
	hp_info("%s:the rtp cont len is %d\n", __func__, rtp_cont->len);
	cpu_latency_qos_add_request(&sih_haptic->pm_qos, SIH_PM_QOS_VALUE_VB);
	sih_haptic->rtp.rtp_cnt = 0;
	disable_irq(gpio_to_irq(sih_haptic->chip_attr.irq_gpio));
	while (!sih_haptic->hp_func->get_rtp_fifo_full_state(sih_haptic) &&
			(sih_haptic->chip_ipara.play_mode == SIH_RTP_MODE) &&
			!atomic_read(&sih_haptic->rtp.exit_in_rtp_loop)) {
		if (sih_haptic->chip_ipara.is_custom_wave == 0) {
			cont_len = rtp_cont->len;
			if (sih_haptic->rtp.rtp_cnt < sih_haptic->ram.base_addr) {
				if ((cont_len - sih_haptic->rtp.rtp_cnt) <
					sih_haptic->ram.base_addr)
					buf_len = cont_len - sih_haptic->rtp.rtp_cnt;
				else
					buf_len = sih_haptic->ram.base_addr;
			} else if ((cont_len - sih_haptic->rtp.rtp_cnt) <
				(sih_haptic->ram.base_addr >> 2)) {
				buf_len = cont_len - sih_haptic->rtp.rtp_cnt;
			} else {
				buf_len = sih_haptic->ram.base_addr >> 2;
			}
			if (sih_haptic->rtp.rtp_cnt != cont_len) {
				if (mode == SIH_RTP_OSC_PLAY) {
					if (sih_haptic->osc_para.start_flag) {
						sih_haptic->osc_para.kstart = ktime_get();
						sih_haptic->osc_para.start_flag = false;
					}
				}
                         	if(buf_len > 0){
					sih_haptic->hp_func->write_rtp_data(sih_haptic,
						&rtp_cont->data[sih_haptic->rtp.rtp_cnt], buf_len);
					sih_haptic->rtp.rtp_cnt += buf_len;
					hp_info("%s: write %d, total write %d, file len %d\n",
							__func__, buf_len, sih_haptic->rtp.rtp_cnt, cont_len);
                                }
			}
			if ((sih_haptic->rtp.rtp_cnt == cont_len) &&
				sih_haptic->hp_func->if_chip_is_mode(sih_haptic, SIH_IDLE_MODE)) {
				if (sih_haptic->rtp.rtp_cnt != cont_len)
					hp_err("%s:rtp suspend!\n", __func__);
				else
					hp_info("%s:rtp complete!\n", __func__);
				if (mode == SIH_RTP_OSC_PLAY)
					sih_haptic->osc_para.kend = ktime_get();
				sih_chip_state_recovery(sih_haptic);
				break;
			}
		} else {
			buf_len = read_rb(rtp_cont->data, period_size);
			sih_haptic->hp_func->write_rtp_data(sih_haptic,
				rtp_cont->data, buf_len);
			if (buf_len < period_size) {
				hp_info("%s: custom rtp update complete\n",
					__func__);
				sih_haptic->rtp.rtp_cnt = 0;
				sih_haptic->hp_func->set_rtp_aei(sih_haptic, false);
				break;
			}
		}
	}
	enable_irq(gpio_to_irq(sih_haptic->chip_attr.irq_gpio));
	if (mode == SIH_RTP_NORMAL_PLAY &&
		sih_haptic->chip_ipara.play_mode == SIH_RTP_MODE &&
		!atomic_read(&sih_haptic->rtp.exit_in_rtp_loop)) {
		sih_haptic->hp_func->set_rtp_aei(sih_haptic, true);
	}
	cpu_latency_qos_remove_request(&sih_haptic->pm_qos);
	hp_info("%s: exit\n", __func__);
}

static void sih_rtp_play(sih_haptic_t *sih_haptic, uint8_t mode)
{
	hp_info("%s:rtp mode:%d\n", __func__, mode);
	if (mode == SIH_RTP_NORMAL_PLAY) {
		sih_rtp_play_func(sih_haptic, mode);
	} else if (mode == SIH_RTP_OSC_PLAY) {
		sih_haptic->osc_para.start_flag = true;
		sih_rtp_play_func(sih_haptic, mode);
		sih_haptic->osc_para.actual_time =
			ktime_to_us(ktime_sub(sih_haptic->osc_para.kend,
			sih_haptic->osc_para.kstart));
		hp_info("%s:actual time:%d\n", __func__,
			sih_haptic->osc_para.actual_time);
	} else if (mode == SIH_RTP_POLAR_PLAY) {
		sih_rtp_play_func(sih_haptic, mode);
	} else {
		hp_err("%s:err mode %d\n", __func__, mode);
	}
}

static void sih_rtp_local_work(sih_haptic_t *sih_haptic, uint8_t mode)
{
	bool rtp_work_flag = false;
	int cnt = SIH_ENTER_RTP_MODE_MAX_TRY;
	int ret = -1;
	const struct firmware *rtp_file;
	uint8_t rtp_file_index = 0;

	hp_info("%s:enter!\n", __func__);

	if (mode == SIH_RTP_OSC_PLAY)
		rtp_file_index = 0;
	else if (mode == SIH_RTP_POLAR_PLAY)
		rtp_file_index = 0;
	else
		hp_err("%s:err mode:%d\n", __func__, mode);

	mutex_lock(&sih_haptic->lock);

	sih_haptic->rtp.rtp_init = false;
	sih_vfree_container(sih_haptic, sih_haptic->rtp.rtp_cont);

	ret = request_firmware(&rtp_file, awinic_rtp_name[rtp_file_index],
		sih_haptic->dev);
	if (ret < 0) {
		hp_err("%s:fail to read %s\n", __func__, awinic_rtp_name[rtp_file_index]);
		sih_chip_state_recovery(sih_haptic);
		mutex_unlock(&sih_haptic->rtp.rtp_lock);
		return;
	}

	sih_haptic->rtp.rtp_cont = vmalloc(rtp_file->size + sizeof(int));
	if (!sih_haptic->rtp.rtp_cont) {
		release_firmware(rtp_file);
		hp_err("%s:error allocating memory\n", __func__);
		sih_chip_state_recovery(sih_haptic);
		mutex_unlock(&sih_haptic->rtp.rtp_lock);
		return;
	}
	sih_haptic->rtp.rtp_cont->len = rtp_file->size;
	if (mode == SIH_RTP_OSC_PLAY)
		sih_haptic->osc_para.osc_rtp_len = rtp_file->size;

	mutex_unlock(&sih_haptic->lock);

	memcpy(sih_haptic->rtp.rtp_cont->data, rtp_file->data, rtp_file->size);
	release_firmware(rtp_file);

	hp_info("%s:rtp len is %d\n", __func__, sih_haptic->rtp.rtp_cont->len);

	mutex_lock(&sih_haptic->lock);
	sih_haptic->rtp.rtp_init = true;
	sih_haptic->chip_ipara.state = SIH_ACTIVE_MODE;
	sih_haptic->hp_func->stop(sih_haptic);
	sih_haptic->hp_func->set_rtp_aei(sih_haptic, false);
	sih_haptic->hp_func->set_play_mode(sih_haptic, SIH_RTP_MODE);
	/* osc rtp cali set trim to zero */
	if (mode == SIH_RTP_OSC_PLAY)
		sih_haptic->hp_func->upload_f0(sih_haptic, SIH_WRITE_ZERO);
	if (mode != SIH_RTP_NORMAL_PLAY) {
		disable_irq(gpio_to_irq(sih_haptic->chip_attr.irq_gpio));
		sih_haptic->chip_ipara.is_custom_wave = 0;
	}

	sih_haptic->hp_func->play_go(sih_haptic, true);
	usleep_range(2000, 2500);
	while (cnt--) {
		if (sih_haptic->hp_func->if_chip_is_mode(sih_haptic, SIH_RTP_MODE)) {
			rtp_work_flag = true;
			hp_info("%s:rtp go!\n", __func__);
			break;
		}

		hp_info("%s:wait for rtp go!\n", __func__);
		usleep_range(2000, 2500);
	}
	if (rtp_work_flag) {
		sih_rtp_play(sih_haptic, mode);
	} else {
		/* enter standby mode */
		sih_haptic->hp_func->stop(sih_haptic);
		sih_haptic->chip_ipara.state = SIH_STANDBY_MODE;
		hp_err("%s:failed to enter rtp_go status!\n", __func__);
	}
	/* enable irq */
	if (mode != SIH_RTP_NORMAL_PLAY)
		enable_irq(gpio_to_irq(sih_haptic->chip_attr.irq_gpio));

	mutex_unlock(&sih_haptic->lock);
}

/*****************************************************
 *
 * ram
 *
 *****************************************************/
static void get_ram_num(sih_haptic_t *sih_haptic)
{
	uint8_t wave_addr[2] = {0};
	uint32_t first_wave_addr = 0;

	hp_info("%s:enter\n", __func__);
	if (!sih_haptic->ram.ram_init) {
		hp_err("%s:ram init failed, wave_num = 0!\n", __func__);
		return;
	}

	mutex_lock(&sih_haptic->lock);
	/* RAMINIT Enable */
	sih_haptic->hp_func->ram_init(sih_haptic, true);
	sih_haptic->hp_func->stop(sih_haptic);
	sih_haptic->hp_func->set_ram_addr(sih_haptic);
	sih_haptic->hp_func->get_first_wave_addr(sih_haptic, wave_addr);
	first_wave_addr = (wave_addr[0] << 8 | wave_addr[1]);
	sih_haptic->ram.wave_num = (first_wave_addr -
		sih_haptic->ram.base_addr - 1) / 4;

	hp_info("%s:first wave addr = 0x%04x, wave_num = %d\n", __func__,
		first_wave_addr, sih_haptic->ram.wave_num);

	/* RAMINIT Disable */
	sih_haptic->hp_func->ram_init(sih_haptic, false);
	mutex_unlock(&sih_haptic->lock);
}

static void sih_ram_load(const struct firmware *cont, void *context)
{
	int i;
	int ret = -1;
	uint16_t check_sum = 0;
	sih_haptic_t *sih_haptic = context;
	haptic_container_t *sih_haptic_fw;

	hp_info("%s:enter\n", __func__);

	if (!cont) {
		hp_err("%s:failed to read %s\n", __func__,
			awinic_ram_name[sih_haptic->ram.lib_index]);
		release_firmware(cont);
		return;
	}

	hp_info("%s:loaded %s - size: %zu\n", __func__,
		awinic_ram_name[sih_haptic->ram.lib_index], cont ? cont->size : 0);

	/* check sum */
	for (i = 2; i < cont->size; i++)
		check_sum += cont->data[i];
	if (check_sum != (uint16_t)((cont->data[0] << 8) | (cont->data[1]))) {
		hp_err("%s:check sum err: check_sum=0x%04x\n", __func__, check_sum);
		release_firmware(cont);
		return;
	}

	hp_info("%s:check sum pass : 0x%04x\n", __func__, check_sum);

	sih_haptic->ram.check_sum = check_sum;

	/*ram update */
	sih_haptic_fw = kzalloc(cont->size + sizeof(int), GFP_KERNEL);
	if (!sih_haptic_fw) {
		release_firmware(cont);
		hp_err("%s:error allocating memory\n", __func__);
		return;
	}

	sih_haptic_fw->len = cont->size;
	memcpy(sih_haptic_fw->data, cont->data, cont->size);
	release_firmware(cont);
	ret = sih_haptic->hp_func->update_ram_config(sih_haptic, sih_haptic_fw);
	if (ret) {
		hp_err("%s:ram firmware update failed!\n", __func__);
	} else {
		sih_haptic->ram.ram_init = true;
		sih_haptic->ram.len = sih_haptic_fw->len - sih_haptic->ram.ram_shift;

		hp_info("%s:ram firmware update complete!\n", __func__);
		get_ram_num(sih_haptic);
	}
	kfree(sih_haptic_fw);
}

static void sih_ram_play(sih_haptic_t *sih_haptic, uint8_t mode)
{
	hp_info("%s:enter\n", __func__);
	sih_haptic->hp_func->set_wav_seq(sih_haptic, 0, sih_haptic->chip_ipara.effect_id+1);
	sih_haptic->hp_func->set_wav_loop(sih_haptic, 0, 0);
	sih_haptic->hp_func->set_play_mode(sih_haptic, mode);
	sih_haptic->hp_func->play_go(sih_haptic, true);
}

sih_haptic_t *get_global_haptic_ptr(void)
{
	return g_haptic_t.g_haptic[SIH_HAPTIC_MMAP_DEV_INDEX];
}

int pointer_prehandle(struct device *dev, const char *buf,
	sih_haptic_t **sih_haptic)
{
	hp_info("%s:enter\n", __func__);
	null_pointer_err_check(dev);
	null_pointer_err_check(buf);
	*sih_haptic = dev_get_drvdata(dev);
	null_pointer_err_check(*sih_haptic);
	return 0;
}

/*****************************************************
 *
 * input ff
 *
 *****************************************************/
void sih_set_gain_work_routine(struct work_struct *work)
{
	sih_haptic_t *sih_haptic = container_of(work, sih_haptic_t, ram.gain_work);

	if (sih_haptic->chip_ipara.new_gain >= 0x7FFF)
		sih_haptic->chip_ipara.gain = 0x80;	/*128 */
	else if (sih_haptic->chip_ipara.new_gain <= 0x3FFF)
		sih_haptic->chip_ipara.gain = 0x1E;	/*30 */
	else
		sih_haptic->chip_ipara.gain = (sih_haptic->chip_ipara.new_gain - 16383) / 128;

	if (sih_haptic->chip_ipara.gain < 0x1E)
		sih_haptic->chip_ipara.gain = 0x1E;	/*30 */
	hp_info("%s:new_gain = %x level = %x\n",
		__func__, sih_haptic->chip_ipara.new_gain, sih_haptic->chip_ipara.gain);
	sih_haptic->hp_func->vbat_comp(sih_haptic);
}

void sih_effect_strength_gain_adapter(sih_haptic_t *sih_haptic)
{
	if (sih_haptic->chip_ipara.new_gain >= 0x7FFF)
		sih_haptic->chip_ipara.gain = 0x80;	/*128 */
	else if (sih_haptic->chip_ipara.new_gain <= 0x3FFF)
		sih_haptic->chip_ipara.gain = 0x1E;	/*30 */
	else
		sih_haptic->chip_ipara.gain = (sih_haptic->chip_ipara.new_gain - 16383) / 128;

	if (sih_haptic->chip_ipara.gain < 0x1E)
		sih_haptic->chip_ipara.gain = 0x1E;	/*30 */
	hp_info("%s:new_gain = %x level = %x\n",
		__func__, sih_haptic->chip_ipara.new_gain, sih_haptic->chip_ipara.gain);
}

void sih_set_gain(struct input_dev *dev, u16 gain)
{
	sih_haptic_t *sih_haptic = input_get_drvdata(dev);

	hp_info("%s enter\n", __func__);
	sih_haptic->chip_ipara.new_gain = gain;
	sih_effect_strength_gain_adapter(sih_haptic);
}

int sih_upload_effect(struct input_dev *dev,
					struct ff_effect *effect,
					struct ff_effect *old)
{
	sih_haptic_t *sih_haptic = input_get_drvdata(dev);
	s16 data[CUSTOM_DATA_LEN];
	ktime_t rem;
	s64 time_us;
	int ret;

	if (hrtimer_active(&sih_haptic->timer)) {
		rem = hrtimer_get_remaining(&sih_haptic->timer);
		time_us = ktime_to_us(rem);
		hp_info("waiting for playing clear sequence: %lld us\n",
			time_us);
		usleep_range(time_us, time_us + 100);
	}
	hp_info("%s: effect->type=0x%x,FF_CONSTANT=0x%x,FF_PERIODIC=0x%x\n",
		__func__, effect->type, FF_CONSTANT, FF_PERIODIC);
	sih_haptic->chip_ipara.effect_type = effect->type;
	mutex_lock(&sih_haptic->lock);
	while (atomic_read(&sih_haptic->rtp.exit_in_rtp_loop)) {
		hp_info("%s goint to waiting rtp exit\n", __func__);
		mutex_unlock(&sih_haptic->lock);
		ret = wait_event_interruptible(sih_haptic->rtp.stop_wait_q,
				atomic_read(&sih_haptic->rtp.exit_in_rtp_loop) == 0);
		hp_info("%s wakeup\n", __func__);
		if (ret == -ERESTARTSYS) {
			mutex_unlock(&sih_haptic->lock);
			hp_err("%s wake up by signal return erro\n", __func__);
			return ret;
		}
		mutex_lock(&sih_haptic->lock);
	}

	if (sih_haptic->chip_ipara.effect_type == FF_CONSTANT) {
		hp_info("%s: effect_type is  FF_CONSTANT!\n", __func__);
		/*cont mode set duration */
		sih_haptic->chip_ipara.duration = effect->replay.length;
		sih_haptic->ram.action_mode = SIH_RAM_LOOP_MODE;
		sih_haptic->chip_ipara.effect_id = sih_haptic->chip_ipara.effect_id_boundary;

	} else if (sih_haptic->chip_ipara.effect_type == FF_PERIODIC) {
		if (sih_haptic->chip_ipara.effects_count == 0) {
			mutex_unlock(&sih_haptic->lock);
			return -EINVAL;
		}

		hp_info("%s: effect_type is  FF_PERIODIC!\n", __func__);
		if (copy_from_user(data, effect->u.periodic.custom_data,
				   sizeof(s16) * CUSTOM_DATA_LEN)) {
			mutex_unlock(&sih_haptic->lock);
			hp_err("%s: get custom data fail\n", __func__);
			return -EFAULT;
		}
		sih_haptic->chip_ipara.effect_id = data[0];
		sih_haptic->chip_ipara.new_gain = effect->u.periodic.magnitude;
		hp_info("%s: effect_id =%d\n",
			 __func__, sih_haptic->chip_ipara.effect_id);

		if (sih_haptic->chip_ipara.effect_id < 0 ||
			sih_haptic->chip_ipara.effect_id > sih_haptic->chip_ipara.effect_max) {
			mutex_unlock(&sih_haptic->lock);
			return 0;
		}
		sih_haptic->chip_ipara.is_custom_wave = 0;

		if (sih_haptic->chip_ipara.effect_id < sih_haptic->chip_ipara.effect_id_boundary) {
			sih_haptic->ram.action_mode = SIH_RAM_MODE;
			hp_info("%s: effect_id=%d , play_mode = %d\n",
				__func__, sih_haptic->chip_ipara.effect_id,
				sih_haptic->ram.action_mode);
			if(sih_haptic->chip_ipara.effect_id == 4){
				/*second data*/
				data[1] = 0;
				/*millisecond data*/
				data[2] = 28;
			}else{
				/*second data*/
				data[1] = 0;
				/*millisecond data*/
				data[2] = 20;
			}
		}
		if (sih_haptic->chip_ipara.effect_id >= sih_haptic->chip_ipara.effect_id_boundary) {
			sih_haptic->ram.action_mode = SIH_RTP_MODE;
			hp_info("%s: effect_id=%d , play_mode = %d\n",
				__func__, sih_haptic->chip_ipara.effect_id,
				sih_haptic->ram.action_mode);
			/*second data*/
			data[1] = 30;
			/*millisecond data*/
			data[2] = 0;
		}
		if (sih_haptic->chip_ipara.effect_id == CUSTOME_WAVE_ID) {
			sih_haptic->ram.action_mode = SIH_RTP_MODE;
			hp_info("%s: effect_id=%d , play_mode = %d\n",
				__func__, sih_haptic->chip_ipara.effect_id,
				sih_haptic->ram.action_mode);
			/*second data*/
			data[1] = 30;
			/*millisecond data*/
			data[2] = 0;
			sih_haptic->chip_ipara.is_custom_wave = 1;
			rb_init();
		}

		if (copy_to_user(effect->u.periodic.custom_data, data,
			sizeof(s16) * CUSTOM_DATA_LEN)) {
			mutex_unlock(&sih_haptic->lock);
			return -EFAULT;
		}

	} else {
		hp_err("%s Unsupported effect type: %d\n", __func__,
		       effect->type);
	}
	mutex_unlock(&sih_haptic->lock);
	return 0;
}

int sih_playback(struct input_dev *dev, int effect_id,
				   int val)
{
	sih_haptic_t *sih_haptic = input_get_drvdata(dev);
	int rc = 0;

	hp_info("%s: effect_id=%d , action_mode = %d val = %d\n",
		__func__, sih_haptic->chip_ipara.effect_id, sih_haptic->ram.action_mode, val);

	if (val > 0)
		sih_haptic->chip_ipara.state = SIH_ACTIVE_MODE;
	if (val <= 0)
		sih_haptic->chip_ipara.state = SIH_STANDBY_MODE;
	hrtimer_cancel(&sih_haptic->timer);

	if (sih_haptic->chip_ipara.effect_type == FF_CONSTANT &&
		sih_haptic->ram.action_mode ==
			SIH_RAM_LOOP_MODE) {
		hp_info("%s: enter cont_mode\n", __func__);
		queue_work(sih_haptic->work_queue, &sih_haptic->ram.ram_work);
	} else if (sih_haptic->chip_ipara.effect_type == FF_PERIODIC &&
		sih_haptic->ram.action_mode == SIH_RAM_MODE) {
		hp_info("%s: enter ram_mode\n", __func__);
		queue_work(sih_haptic->work_queue, &sih_haptic->ram.ram_work);
	} else if ((sih_haptic->chip_ipara.effect_type == FF_PERIODIC) &&
		sih_haptic->ram.action_mode == SIH_RTP_MODE) {
		hp_info("%s: enter rtp_mode\n", __func__);
		queue_work(sih_haptic->work_queue, &sih_haptic->rtp.rtp_work);
		/*if we are in the play mode, force to exit*/
		if (val == 0) {
			atomic_set(&sih_haptic->rtp.exit_in_rtp_loop, 1);
			rb_force_exit();
			wake_up_interruptible(&sih_haptic->rtp.stop_wait_q);
			pm_relax(sih_haptic->dev);
		}
	} else {
		/*other mode */
	}
	return rc;
}

int sih_erase(struct input_dev *dev, int effect_id)
{
	sih_haptic_t *sih_haptic = input_get_drvdata(dev);
	int rc = 0;

	hp_info("%s: enter\n", __func__);
	sih_haptic->chip_ipara.effect_type = 0;
	sih_haptic->chip_ipara.is_custom_wave = 0;
	sih_haptic->chip_ipara.duration = 0;
	return rc;
}

/*****************************************************
 *
 * vibrator sysfs node
 *
 *****************************************************/

static ssize_t seq_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	uint8_t i;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	sih_haptic->hp_func->get_wav_seq(sih_haptic, SIH_HAPTIC_SEQUENCER_SIZE);

	for (i = 0; i < SIH_HAPTIC_SEQUENCER_SIZE; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len,
			"seq%d = %d\n", i, sih_haptic->ram.seq[i]);
	}

	return len;
}

static ssize_t seq_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	unsigned int databuf[2] = {0, 0};
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		if (databuf[0] >= SIH_HAPTIC_SEQUENCER_SIZE) {
			hp_err("%s:input value out of range!\n", __func__);
			return count;
		}
		mutex_lock(&sih_haptic->lock);
		sih_haptic->ram.seq[(uint8_t)databuf[0]] = (uint8_t)databuf[1];
		sih_haptic->hp_func->set_wav_seq(sih_haptic,
			(uint8_t)databuf[0], (uint8_t)databuf[1]);
		mutex_unlock(&sih_haptic->lock);
	}

	return count;
}

static ssize_t reg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	uint32_t i;
	ssize_t len = 0;
	uint8_t reg_val = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	for (i = 0; i <= SIH688X_REG_MAX; i++) {
		haptic_regmap_read(sih_haptic->regmapp.regmapping, i,
			SIH_I2C_OPERA_BYTE_ONE, &reg_val);
		len += snprintf(buf + len, PAGE_SIZE - len,
			"0x%02x = 0x%02x\n", i, reg_val);
	}

	return len;
}

static ssize_t reg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	unsigned int databuf[2] = {0, 0};
	uint8_t val = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		val = (uint8_t)databuf[1];
		haptic_regmap_write(sih_haptic->regmapp.regmapping,
			(uint8_t)databuf[0], SIH_I2C_OPERA_BYTE_ONE, &val);
	}

	return count;
}

static ssize_t state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	sih_haptic->hp_func->update_chip_state(sih_haptic);

	len += snprintf(buf + len, PAGE_SIZE - len, "state = %d, play_mode = %d\n",
		sih_haptic->chip_ipara.state, sih_haptic->chip_ipara.play_mode);

	return len;
}

static ssize_t state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	uint32_t val = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val != 0 && val != 1)
	{
		hp_err("%s: error val %d\n", __func__, val);
		return count;
	}
	
	if (val == SIH_STANDBY_MODE) {
		sih_haptic->hp_func->stop(sih_haptic);
		sih_haptic->chip_ipara.state = SIH_STANDBY_MODE;
	} else {
		sih_haptic->chip_ipara.state = SIH_ACTIVE_MODE;
	}

	return count;
}

static ssize_t gain_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len = snprintf(buf, PAGE_SIZE, "gain = 0x%02x\n", sih_haptic->chip_ipara.gain);

	return len;
}

static ssize_t gain_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	uint32_t val = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val > SIH_HAPTIC_MAX_GAIN) {
		hp_err("%s:gain out of range!\n", __func__);
		return count;
	}

	hp_info("%s:gain = 0x%02x\n", __func__, val);

	mutex_lock(&sih_haptic->lock);
	sih_haptic->chip_ipara.gain = val;
	sih_haptic->hp_func->set_gain(sih_haptic, sih_haptic->chip_ipara.gain);
	mutex_unlock(&sih_haptic->lock);

	return count;
}

static ssize_t rtp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len += snprintf(buf + len, PAGE_SIZE - len, "rtp_cnt = %d\n",
		sih_haptic->rtp.rtp_cnt);

	return len;
}

static ssize_t rtp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	uint32_t val = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val != 0 && val != 1)
	{
		hp_err("%s: error val %d\n", __func__, val);
		return count;
	}

	hp_info("%s: rtp start: %d\n", __func__, val);
	if (val == 1) {
		mutex_lock(&sih_haptic->lock);
		sih_haptic->hp_func->stop(sih_haptic);
		sih_haptic->chip_ipara.state = SIH_ACTIVE_MODE;
		sih_haptic->chip_ipara.play_mode = SIH_RTP_MODE;
		mutex_unlock(&sih_haptic->lock);
		schedule_work(&sih_haptic->rtp.rtp_work);
	} else {
		mutex_lock(&sih_haptic->lock);
		sih_haptic->hp_func->stop(sih_haptic);
		sih_haptic->chip_ipara.state = SIH_STANDBY_MODE;
		sih_haptic->chip_ipara.play_mode = SIH_IDLE_MODE;
		mutex_unlock(&sih_haptic->lock);
	}
	return count;
}

static ssize_t cali_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len += snprintf(buf + len, PAGE_SIZE - len, "cali_target_value = %d\n",
		sih_haptic->detect.cali_target_value);

	return len;
}

static ssize_t cali_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	uint32_t val = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val <= 0)
		return count;

	sih_haptic->detect.cali_target_value = val;
	sih_haptic->hp_func->get_tracking_f0(sih_haptic);
	sih_haptic->hp_func->upload_f0(sih_haptic, SIH_F0_CALI_LRA);
	msleep(200);

	return count;
}

static ssize_t auto_pvdd_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len += snprintf(buf + len, PAGE_SIZE - len, "auto_pvdd = %d\n",
		sih_haptic->chip_ipara.auto_pvdd_en);

	return len;
}

static ssize_t auto_pvdd_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	bool val = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	rc = strtobool(buf, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&sih_haptic->lock);
	sih_haptic->hp_func->stop(sih_haptic);
	sih_haptic->hp_func->set_auto_pvdd(sih_haptic, val);
	mutex_unlock(&sih_haptic->lock);

	return count;
}

static ssize_t duration_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	ktime_t time_remain;
	s64 time_ms = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	if (hrtimer_active(&sih_haptic->timer)) {
		time_remain = hrtimer_get_remaining(&sih_haptic->timer);
		time_ms = ktime_to_ms(time_remain);
	}

	len = snprintf(buf, PAGE_SIZE, "duration = %lldms\n", time_ms);

	return len;
}

static ssize_t duration_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	uint32_t val = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val <= 0) {
		hp_err("%s:duration out of range!\n", __func__);
		return count;
	}
	sih_haptic->chip_ipara.duration = val;

	return count;
}

static ssize_t osc_cali_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len += snprintf(buf + len, PAGE_SIZE - len, "actual = %d theory = %d\n",
		sih_haptic->osc_para.actual_time, sih_haptic->osc_para.theory_time);

	return len;
}

static ssize_t osc_cali_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	uint32_t val = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val <= 0)
		return count;

	sih_haptic->chip_ipara.state = SIH_ACTIVE_MODE;
	sih_rtp_local_work(sih_haptic, SIH_RTP_OSC_PLAY);
	sih_haptic->hp_func->osc_cali(sih_haptic);

	return count;
}

static ssize_t effect_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len += snprintf(buf, PAGE_SIZE, "index = %d\n", sih_haptic->chip_ipara.effect_id);

	return len;
}

static ssize_t effect_id_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	uint32_t val = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val > sih_haptic->chip_ipara.effect_max) {
		hp_err("%s:input value out of range!\n", __func__);
		return count;
	}

	hp_info("%s:effect_id = %d\n", __func__, val);

	mutex_lock(&sih_haptic->lock);
	sih_haptic->chip_ipara.effect_id = val;
	mutex_unlock(&sih_haptic->lock);

	return count;
}

static ssize_t ram_num_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len += snprintf(buf + len, PAGE_SIZE - len, "wave_num = %d\n",
		sih_haptic->ram.wave_num);

	return len;
}

static ssize_t loop_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	uint8_t i;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	sih_haptic->hp_func->get_wav_loop(sih_haptic);

	for (i = 0; i < SIH_HAPTIC_SEQUENCER_SIZE; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len, "seq%d = loop:%d\n",
			i, sih_haptic->ram.loop[i]);
	}

	return len;
}

static ssize_t loop_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	unsigned int databuf[2] = {0, 0};
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		if ((databuf[0] >= SIH_HAPTIC_SEQUENCER_SIZE) ||
			(databuf[1] > SIH_HAPTIC_REG_SEQLOOP_MAX)) {
			hp_err("%s:input value out of range!\n", __func__);
			return count;
		}
		mutex_lock(&sih_haptic->lock);
		sih_haptic->ram.loop[(uint8_t)databuf[0]] = (uint8_t)databuf[1];
		sih_haptic->hp_func->set_wav_loop(sih_haptic,
			(uint8_t)databuf[0], (uint8_t)databuf[1]);
		mutex_unlock(&sih_haptic->lock);
	}

	return count;
}

static ssize_t ram_update_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	/* RAMINIT Enable */
	sih_haptic->hp_func->stop(sih_haptic);
	sih_haptic->hp_func->ram_init(sih_haptic, true);
	sih_haptic->hp_func->set_ram_addr(sih_haptic);
	len += snprintf(buf + len, PAGE_SIZE - len, "sih_haptic_ram:\n");
	len += sih_haptic->hp_func->get_ram_data(sih_haptic, buf);

	/* RANINIT Disable */
	sih_haptic->hp_func->ram_init(sih_haptic, false);

	return len;
}

static ssize_t ram_update_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	uint32_t val = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	hp_info("%s:ram update input is %d\n", __func__, val);

	if (val) {
		if (val > SIH_RAMWAVEFORM_MAX_NUM)
			return count;

		sih_haptic->ram.lib_index = val - 1;
		sih_haptic->ram.ram_init = false;
		request_firmware_nowait(THIS_MODULE, FW_ACTION_UEVENT,
			awinic_ram_name[sih_haptic->ram.lib_index],
			sih_haptic->dev, GFP_KERNEL, sih_haptic, sih_ram_load);
	}

	return count;
}

static ssize_t ram_vbat_comp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len += snprintf(buf + len, PAGE_SIZE - len, "ram_vbat_comp = %d\n",
		sih_haptic->ram.ram_vbat_comp);

	return len;
}

static ssize_t ram_vbat_comp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	uint32_t val = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&sih_haptic->lock);

	if (val)
		sih_haptic->ram.ram_vbat_comp = SIH_RAM_VBAT_COMP_ENABLE;
	else
		sih_haptic->ram.ram_vbat_comp = SIH_RAM_VBAT_COMP_DISABLE;

	mutex_unlock(&sih_haptic->lock);

	return count;
}

static ssize_t cont_go_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	uint32_t val = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val <= 0)
		return count;

	mutex_lock(&sih_haptic->lock);
	sih_haptic->hp_func->check_detect_state(sih_haptic, SIH_CONT_MODE);
	sih_haptic->hp_func->set_play_mode(sih_haptic, SIH_CONT_MODE);
	sih_haptic->hp_func->play_go(sih_haptic, true);
	mutex_unlock(&sih_haptic->lock);

	return count;
}

static ssize_t cont_seq0_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len = sih_haptic->hp_func->get_cont_para(sih_haptic,
		SIH688X_CONT_PARA_SEQ0, buf);

	return len;
}

static ssize_t cont_seq0_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	unsigned int databuf[3] = {0};
	uint8_t param[3] = {0};
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	if (sscanf(buf, "%x %x %x", &databuf[0], &databuf[1], &databuf[2]) == 3) {
		param[0] = (uint8_t)databuf[0];
		param[1] = (uint8_t)databuf[1];
		param[2] = (uint8_t)databuf[2];
		mutex_lock(&sih_haptic->lock);
		sih_haptic->hp_func->set_cont_para(sih_haptic,
			SIH688X_CONT_PARA_SEQ0, param);
		mutex_unlock(&sih_haptic->lock);
	}

	return count;
}

static ssize_t cont_seq1_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len = sih_haptic->hp_func->get_cont_para(sih_haptic,
		SIH688X_CONT_PARA_SEQ1, buf);

	return len;
}

static ssize_t cont_seq1_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	unsigned int databuf[3] = {0};
	uint8_t param[3] = {0};
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	if (sscanf(buf, "%x %x %x", &databuf[0], &databuf[1], &databuf[2]) == 3) {
		param[0] = (uint8_t)databuf[0];
		param[1] = (uint8_t)databuf[1];
		param[2] = (uint8_t)databuf[2];
		mutex_lock(&sih_haptic->lock);
		sih_haptic->hp_func->set_cont_para(sih_haptic,
			SIH688X_CONT_PARA_SEQ1, param);
		mutex_unlock(&sih_haptic->lock);
	}

	return count;
}

static ssize_t cont_seq2_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len = sih_haptic->hp_func->get_cont_para(sih_haptic,
		SIH688X_CONT_PARA_SEQ2, buf);

	return len;
}

static ssize_t cont_seq2_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	unsigned int databuf[3] = {0};
	uint8_t param[3] = {0};
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	if (sscanf(buf, "%x %x %x", &databuf[0], &databuf[1], &databuf[2]) == 3) {
		param[0] = (uint8_t)databuf[0];
		param[1] = (uint8_t)databuf[1];
		param[2] = (uint8_t)databuf[2];
		mutex_lock(&sih_haptic->lock);
		sih_haptic->hp_func->set_cont_para(sih_haptic,
			SIH688X_CONT_PARA_SEQ2, param);
		mutex_unlock(&sih_haptic->lock);
	}

	return count;
}

static ssize_t cont_asmooth_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len = sih_haptic->hp_func->get_cont_para(sih_haptic,
		SIH688X_CONT_PARA_ASMOOTH, buf);

	return len;
}

static ssize_t cont_asmooth_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	unsigned int databuf[3] = {0};
	uint8_t param[3] = {0};
	int rc = 0;
	hp_err("%s:enter!\n", __func__);
	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	if (sscanf(buf, "%x %x %x", &databuf[0], &databuf[1], &databuf[2]) == 3) {
		param[0] = (uint8_t)databuf[0];
		param[1] = (uint8_t)databuf[1];
		param[2] = (uint8_t)databuf[2];
		mutex_lock(&sih_haptic->lock);
		sih_haptic->hp_func->set_cont_para(sih_haptic,
			SIH688X_CONT_PARA_ASMOOTH, param);
		mutex_unlock(&sih_haptic->lock);
	}

	return count;
}

static ssize_t cont_th_len_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len = sih_haptic->hp_func->get_cont_para(sih_haptic,
		SIH688X_CONT_PARA_TH_LEN, buf);

	return len;
}

static ssize_t cont_th_len_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	unsigned int databuf[3] = {0};
	uint8_t param[3] = {0};
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	if (sscanf(buf, "%x %x %x", &databuf[0], &databuf[1], &databuf[2]) == 3) {
		param[0] = (uint8_t)databuf[0];
		param[1] = (uint8_t)databuf[1];
		param[2] = (uint8_t)databuf[2];
		mutex_lock(&sih_haptic->lock);
		sih_haptic->hp_func->set_cont_para(sih_haptic,
			SIH688X_CONT_PARA_TH_LEN, param);
		mutex_unlock(&sih_haptic->lock);
	}

	return count;
}

static ssize_t cont_th_num_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len = sih_haptic->hp_func->get_cont_para(sih_haptic,
		SIH688X_CONT_PARA_TH_NUM, buf);

	return len;
}

static ssize_t cont_th_num_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	unsigned int databuf[3] = {0};
	uint8_t param[3] = {0};
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	if (sscanf(buf, "%x %x %x", &databuf[0], &databuf[1], &databuf[2]) == 3) {
		param[0] = (uint8_t)databuf[0];
		param[1] = (uint8_t)databuf[1];
		param[2] = (uint8_t)databuf[2];
		mutex_lock(&sih_haptic->lock);
		sih_haptic->hp_func->set_cont_para(sih_haptic,
			SIH688X_CONT_PARA_TH_NUM, param);
		mutex_unlock(&sih_haptic->lock);
	}

	return count;
}

static ssize_t cont_ampli_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len = sih_haptic->hp_func->get_cont_para(sih_haptic,
		SIH688X_CONT_PARA_AMPLI, buf);

	return len;
}

static ssize_t cont_ampli_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	unsigned int databuf[3] = {0};
	uint8_t param[3] = {0};
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	if (sscanf(buf, "%x %x %x", &databuf[0], &databuf[1], &databuf[2]) == 3) {
		param[0] = (uint8_t)databuf[0];
		param[1] = (uint8_t)databuf[1];
		param[2] = (uint8_t)databuf[2];
		mutex_lock(&sih_haptic->lock);
		sih_haptic->hp_func->set_cont_para(sih_haptic,
			SIH688X_CONT_PARA_AMPLI, param);
		mutex_unlock(&sih_haptic->lock);
	}

	return count;
}

static ssize_t ram_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	uint32_t databuf[2] = {0, 0};
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) != 2) {
		hp_err("%s:input parameter error\n", __func__);
		return count;
	}

	if (!sih_haptic->ram.ram_init) {
		hp_err("%s:ram init failed, not allow to play!\n", __func__);
		return count;
	}

	mutex_lock(&sih_haptic->lock);

	/* RAM MODE */
	if (databuf[0] == 1) {
		sih_haptic->hp_func->stop(sih_haptic);
		sih_haptic->ram.action_mode = SIH_RAM_MODE;
	/* LOOPRAM MODE */
	} else if (databuf[0] == 2) {
		sih_haptic->hp_func->stop(sih_haptic);
		sih_haptic->ram.action_mode = SIH_RAM_LOOP_MODE;
	} else {
		mutex_unlock(&sih_haptic->lock);
		hp_err("%s:mode parameter error\n", __func__);
		return count;
	}

	if (databuf[1] == 1) {
		sih_haptic->chip_ipara.state = SIH_ACTIVE_MODE;
	} else {
		sih_haptic->hp_func->stop(sih_haptic);
		sih_haptic->chip_ipara.state = SIH_STANDBY_MODE;
		mutex_unlock(&sih_haptic->lock);
		return count;
	}

	if (hrtimer_active(&sih_haptic->timer))
		hrtimer_cancel(&sih_haptic->timer);

	mutex_unlock(&sih_haptic->lock);
	sih_haptic->hp_func->check_detect_state(sih_haptic, SIH_RAM_MODE);
	schedule_work(&sih_haptic->ram.ram_work);

	return count;
}

static ssize_t lra_resistance_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	mutex_lock(&sih_haptic->lock);
	sih_haptic->hp_func->get_lra_resistance(sih_haptic);
	mutex_unlock(&sih_haptic->lock);

	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n",
				(uint32_t)sih_haptic->detect.resistance);
	return len;
}

static ssize_t lra_resistance_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	uint32_t val = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	rc = kstrtoint(buf, 0 , &val);
	if (rc < 0)
		return rc;

	sih_haptic->detect.rl_offset = val;

	return count;
}


static ssize_t osc_save_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	sih_haptic_t *sih_haptic = NULL;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n",
		sih_haptic->osc_para.osc_data);

	return len;
}

static ssize_t osc_save_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	uint32_t val = 0;
	sih_haptic_t *sih_haptic = NULL;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	rc = kstrtoint(buf, 0 , &val);
	if (rc < 0)
		return rc;

	sih_haptic->osc_para.osc_data = val;

	return count;
}

static ssize_t f0_save_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	sih_haptic_t *sih_haptic = NULL;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len += snprintf(buf + len, PAGE_SIZE - len, "f0_data = %d\n",
		sih_haptic->detect.tracking_f0);
	
	hp_info("%s:sih_haptic->detect.tracking_f0 val:%d\n", __func__,sih_haptic->detect.tracking_f0);
	return len;
}

static ssize_t f0_save_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	uint32_t val = 0;
	sih_haptic_t *sih_haptic = NULL;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	sih_haptic->detect.tracking_f0 = val;
	hp_info("%s:sih_haptic->detect.tracking_f0 val:%d\n", __func__,sih_haptic->detect.tracking_f0);
	return count;
}

static ssize_t f0_value_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	sih_haptic_t *sih_haptic = NULL;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n",
		sih_haptic->detect.tracking_f0);

	hp_info("%s:sih_haptic cur f0 val:%d\n", __func__,sih_haptic->detect.tracking_f0);
	return len;
}

static ssize_t trig_para_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len += sih_haptic->hp_func->get_trig_para(sih_haptic, buf);

	return len;
}

static ssize_t trig_para_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	uint32_t databuf[7] = {0, 0, 0, 0, 0, 0, 0};
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	if (sscanf(buf, "%x %x %x %x %x %x %x",
		&databuf[0], &databuf[1], &databuf[2], &databuf[3],
		&databuf[4], &databuf[5], &databuf[6]) == 7) {
		mutex_lock(&sih_haptic->lock);
		sih_haptic->hp_func->set_trig_para(sih_haptic, databuf);
		mutex_unlock(&sih_haptic->lock);
	}

	return count;
}

static ssize_t low_power_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len += snprintf(buf + len, PAGE_SIZE - len, "low_power_mode = %d\n",
		(uint8_t)sih_haptic->chip_ipara.low_power);

	return len;
}

static ssize_t low_power_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	uint32_t val = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&sih_haptic->lock);
	sih_haptic->hp_func->set_low_power_mode(sih_haptic, val);
	mutex_unlock(&sih_haptic->lock);

	return count;
}

static ssize_t activate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len += snprintf(buf, PAGE_SIZE, "activate = %d\n",
		sih_haptic->chip_ipara.state);

	return len;
}

static ssize_t activate_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	uint32_t val = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val <= 0)
		return count;

	hp_info("%s:value = %d\n", __func__, val);

	if (!sih_haptic->ram.ram_init) {
		hp_err("%s:ram init failed\n", __func__);
		return count;
	}
	mutex_lock(&sih_haptic->lock);
	sih_haptic->hp_func->stop(sih_haptic);
	sih_haptic->chip_ipara.state = SIH_ACTIVE_MODE;
	sih_haptic->ram.action_mode = SIH_RAM_MODE;
	if (hrtimer_active(&sih_haptic->timer))
		hrtimer_cancel(&sih_haptic->timer);

	mutex_unlock(&sih_haptic->lock);
	sih_haptic->hp_func->check_detect_state(sih_haptic, SIH_RAM_MODE);
	schedule_work(&sih_haptic->ram.ram_work);

	return count;
}

static ssize_t drv_vboost_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len += snprintf(buf, PAGE_SIZE, "drv_vboost = %d\n",
		sih_haptic->chip_ipara.drv_vboost);

	return len;
}

static ssize_t drv_vboost_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	uint32_t val = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	hp_info("%s:value=%d\n", __func__, val);

	mutex_lock(&sih_haptic->lock);
	sih_haptic->chip_ipara.drv_vboost = val;
	if (val < SIH688X_DRV_VBOOST_MIN * SIH688X_DRV_VBOOST_COEFFICIENT) {
		hp_info("%s:drv_vboost is too low,set to 60:%d", __func__, val);
		sih_haptic->chip_ipara.drv_vboost = SIH688X_DRV_VBOOST_MIN
			* SIH688X_DRV_VBOOST_COEFFICIENT;
	} else if (val > SIH688X_DRV_VBOOST_MAX * SIH688X_DRV_VBOOST_COEFFICIENT) {
		hp_info("%s:drv_vboost is too high,set to 110:%d", __func__, val);
		sih_haptic->chip_ipara.drv_vboost = SIH688X_DRV_VBOOST_MAX
			* SIH688X_DRV_VBOOST_COEFFICIENT;
	}
	sih_haptic->hp_func->set_drv_bst_vol(sih_haptic,
		sih_haptic->chip_ipara.drv_vboost);
	mutex_unlock(&sih_haptic->lock);

	return count;
}

static ssize_t burst_rw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int i = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	if (sih_haptic->chip_reg.rw_type != SIH_BURST_READ) {
		hp_err("%s:not read mode\n", __func__);
		return -ERANGE;
	}
	if (sih_haptic->chip_reg.reg_addr == NULL) {
		hp_err("%s:no reg_addr parameter\n", __func__);
		return -ERANGE;
	}
	for (i = 0; i < sih_haptic->chip_reg.reg_num; i++) {
		len += snprintf(buf + len, PAGE_SIZE, "0x%02x,",
			sih_haptic->chip_reg.reg_addr[i]);
	}
	len += snprintf(buf + len - 1, PAGE_SIZE, "\n");

	return len;
}

static ssize_t burst_rw_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	uint8_t reg_val = 0;
	uint32_t rw_type = 0;
	uint32_t reg_num = 0;
	uint32_t reg_addr = 0;
	int i = 0;
	int rc = 0;
	char tmp[5] = {0};
	sih_haptic_t *sih_haptic = NULL;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	if (sscanf(buf, "%x %x %x", &rw_type, &reg_num, &reg_addr) == 3) {
		if (!reg_num) {
			hp_err("%s:param error\n", __func__);
			return -ERANGE;
		}
		sih_haptic->chip_reg.rw_type = rw_type;
		sih_haptic->chip_reg.reg_num = reg_num;
		if (sih_haptic->chip_reg.reg_addr != NULL)
			kfree(sih_haptic->chip_reg.reg_addr);

		sih_haptic->chip_reg.reg_addr = kmalloc(reg_num, GFP_KERNEL);
		if (rw_type == SIH_BURST_WRITE) {
			if ((reg_num * 5) != (strlen(buf) - 15)) {
				hp_err("%s:param err, reg_num = %d, strlen = %d\n",
					__func__, reg_num, (int)(strlen(buf) - 15));
				return -ERANGE;
			}
			for (i = 0; i < reg_num; i++) {
				memcpy(tmp, &buf[15 + i * 5], 4);
				tmp[4] = '\0';
				rc = kstrtou8(tmp, 0, &reg_val);
				if (rc < 0) {
					hp_err("%s:reg_val err\n", __func__);
					return -ERANGE;
				}
				sih_haptic->chip_reg.reg_addr[i] = reg_val;
			}
			for (i = 0; i < reg_num; i++) {
				haptic_regmap_write(sih_haptic->regmapp.regmapping,
					(uint8_t)reg_addr + i, SIH_I2C_OPERA_BYTE_ONE,
					&sih_haptic->chip_reg.reg_addr[i]);
			}
		} else if (rw_type == SIH_BURST_READ) {
			for (i = 0; i < reg_num; i++) {
				haptic_regmap_read(sih_haptic->regmapp.regmapping,
					(uint8_t)reg_addr + i, SIH_I2C_OPERA_BYTE_ONE,
					&sih_haptic->chip_reg.reg_addr[i]);
			}
		}
	} else {
		hp_err("%s:param error rw_type err\n", __func__);
		return -ERANGE;
	}

	return count;
}

static ssize_t detect_vbat_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	mutex_lock(&sih_haptic->lock);
	sih_haptic->hp_func->get_vbat(sih_haptic);
	len += snprintf(buf + len, PAGE_SIZE - len, "detect_vbat = %d\n",
		sih_haptic->detect.vbat);
	mutex_unlock(&sih_haptic->lock);

	return len;
}

static ssize_t rtp_file_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len += snprintf(buf + len, PAGE_SIZE - len, "rtp_cnt = %d\n",
		sih_haptic->rtp.rtp_cnt);

	return len;
}

static ssize_t rtp_file_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	uint32_t rtp_num_max = sizeof(awinic_rtp_name) / SIH_RTP_NAME_MAX;
	char databuf[SIH_RTP_NAME_MAX] = {0};
	int buf_len = 0;
	int i = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	if (sscanf(buf, "%s %d", databuf, &buf_len) != 2) {
		hp_err("%s:input parameter error\n", __func__);
		return count;
	}

	if (strlen(databuf) != buf_len) {
		hp_err("%s:input parameter not match\n", __func__);
		return count;
	}

	mutex_lock(&sih_haptic->lock);

	sih_haptic->hp_func->stop(sih_haptic);
	sih_haptic->hp_func->set_rtp_aei(sih_haptic, false);
	sih_haptic->hp_func->clear_interrupt_state(sih_haptic);

	for (i = 0; i < rtp_num_max; i++) {
		if (strncmp(&(awinic_rtp_name[i][0]), databuf, buf_len) == 0) {
			sih_haptic->rtp.rtp_file_num = i;
			sih_haptic->chip_ipara.state = SIH_ACTIVE_MODE;
			schedule_work(&sih_haptic->rtp.rtp_work);
			mutex_unlock(&sih_haptic->lock);
			break;
		}
	}

	if (i == rtp_num_max) {
		hp_err("%s:file name %s or length %d err\n",
			__func__, databuf, buf_len);
		mutex_unlock(&sih_haptic->lock);
		return -ERANGE;
	}

	mutex_unlock(&sih_haptic->lock);

	return count;
}

static ssize_t main_loop_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	sih_haptic->hp_func->get_wav_main_loop(sih_haptic);

	len += snprintf(buf + len, PAGE_SIZE - len, "main loop:%d\n",
		sih_haptic->ram.main_loop);

	return len;
}

static ssize_t main_loop_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	uint8_t val = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	rc = kstrtou8(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val > SIH_RAM_MAIN_LOOP_MAX_TIME) {
		hp_err("%s:input value out of range!\n", __func__);
		return count;
	}

	hp_info("%s:main_loop = %d\n", __func__, val);

	mutex_lock(&sih_haptic->lock);
	sih_haptic->ram.main_loop = val;
	sih_haptic->hp_func->set_wav_main_loop(sih_haptic, val);
	mutex_unlock(&sih_haptic->lock);

	return count;
}

static ssize_t seq_gain_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	sih_haptic_t *sih_haptic = NULL;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;
	len = sih_haptic->hp_func->get_ram_seq_gain(sih_haptic, buf);

	return len;
}

static ssize_t seq_gain_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	unsigned int databuf[2] = {0, 0};
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		if (databuf[0] >= SIH_HAPTIC_SEQUENCER_SIZE) {
			hp_err("%s:input value out of range!\n", __func__);
			return count;
		}

		if (databuf[1] > SIH_HAPTIC_GAIN_LIMIT) {
			hp_err("%s:gain out of limit!\n", __func__);
			databuf[1] = SIH_HAPTIC_GAIN_LIMIT;
		}

		mutex_lock(&sih_haptic->lock);
		sih_haptic->ram.gain[(uint8_t)databuf[0]] = (uint8_t)databuf[1];
		sih_haptic->hp_func->set_ram_seq_gain(sih_haptic,
			(uint8_t)databuf[0], sih_haptic->ram.gain[(uint8_t)databuf[0]]);
		mutex_unlock(&sih_haptic->lock);
	}

	return count;
}

static ssize_t pwm_rate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	sih_haptic_t *sih_haptic = NULL;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len += sih_haptic->hp_func->get_pwm_rate(sih_haptic, buf);

	return len;
}

static ssize_t pwm_rate_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	uint32_t databuf[2] = {0, 0};
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) != 2) {
		hp_err("%s:input parameter error\n", __func__);
		return count;
	}
	mutex_lock(&sih_haptic->lock);
	sih_haptic->hp_func->set_pwm_rate(sih_haptic,
		(uint8_t)databuf[0], (uint8_t)databuf[1]);
	mutex_unlock(&sih_haptic->lock);

	return count;
}

static ssize_t brake_ctrl_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len += sih_haptic->hp_func->get_brk_state(sih_haptic, buf);

	return len;
}

static ssize_t brake_ctrl_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	unsigned int databuf[2] = {0, 0};
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	if (sscanf(buf, "%x %x\n", &databuf[0], &databuf[1]) != 2) {
		hp_err("%s:input parameter error\n", __func__);
		return count;
	}

	mutex_lock(&sih_haptic->lock);
	sih_haptic->hp_func->set_brk_state(sih_haptic, (uint8_t)databuf[0],
		(uint8_t)databuf[1]);
	mutex_unlock(&sih_haptic->lock);

	return count;
}

static ssize_t detect_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len += sih_haptic->hp_func->get_detect_state(sih_haptic, buf);

	return len;
}

static ssize_t detect_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	uint32_t databuf[2] = {0, 0};
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	if (sscanf(buf, "%x %x\n", &databuf[0], &databuf[1]) != 2) {
		hp_err("%s:input parameter error\n", __func__);
		return count;
	}

	mutex_lock(&sih_haptic->lock);
	sih_haptic->hp_func->set_detect_state(sih_haptic, databuf[0], databuf[1]);
	mutex_unlock(&sih_haptic->lock);

	return count;
}

static ssize_t tracking_f0_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = dev_get_drvdata(dev);
	ssize_t len = 0;
	uint8_t i;

	mutex_lock(&sih_haptic->lock);
	for (i = 0; i < SIH_F0_DETECT_TRY; i++) {
		sih_haptic->hp_func->get_tracking_f0(sih_haptic);
		if (sih_haptic->detect.tracking_f0 <= SIH_F0_MAX_THRESHOLD &&
			sih_haptic->detect.tracking_f0 >= SIH_F0_MIN_THRESHOLD) {
			sih_haptic->f0_cali_status = true;
			break;
		}
		msleep(200);
	}
	mutex_unlock(&sih_haptic->lock);

	len += snprintf(buf, PAGE_SIZE, "%d\n",
		sih_haptic->detect.tracking_f0);
	hp_info("%s:sih_haptic->detect.tracking_f0 val:%d\n", __func__,sih_haptic->detect.tracking_f0);
	return len;
}

static ssize_t sihaptic_f0_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = dev_get_drvdata(dev);
	ssize_t len = 0;

  	if (sih_haptic->f0_cali_status == true)
 		len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", 1);
  	if (sih_haptic->f0_cali_status == false)
  		len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", 0);

	return len;
}

static ssize_t tracking_f0_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	uint32_t val = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val <= 0)
		return count;

	mutex_lock(&sih_haptic->lock);
	sih_haptic->detect.tracking_f0 = val;
	hp_info("%s:sih_haptic->detect.tracking_f0 val:%d\n", __func__,sih_haptic->detect.tracking_f0);
	mutex_unlock(&sih_haptic->lock);

	return count;
}

static ssize_t detect_f0_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len += snprintf(buf + len, PAGE_SIZE - len, "detect_f0 = %d\n",
		sih_haptic->detect.detect_f0);

	return len;
}

/* return buffer size and availbe size */
static ssize_t custom_wave_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	sih_haptic_t *sih_haptic = NULL;
	ssize_t len = 0;
	int rc = 0;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	len +=
		snprintf(buf + len, PAGE_SIZE - len, "period_size=%d;",
		sih_haptic->ram.base_addr >> 2);
	len +=
		snprintf(buf + len, PAGE_SIZE - len,
		"max_size=%d;free_size=%d;",
		get_rb_max_size(), get_rb_free_size());
	len +=
		snprintf(buf + len, PAGE_SIZE - len,
		"custom_wave_id=%d;", CUSTOME_WAVE_ID);
	return len;
}

static ssize_t custom_wave_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	sih_haptic_t *sih_haptic = NULL;
	int rc = 0;
	unsigned long  buf_len, period_size, offset;
	int ret;

	rc = pointer_prehandle(dev, buf, &sih_haptic);
	if (rc < 0)
		return rc;

	period_size = (sih_haptic->ram.base_addr >> 2);
	offset = 0;

	hp_info("%s: write szie %zd, period size %lu", __func__, count,
		 period_size);
	if (count % period_size || count < period_size)
		rb_end();
	atomic_set(&sih_haptic->rtp.is_in_write_loop, 1);

	while (count > 0) {
		buf_len = MIN(count, period_size);
		ret = write_rb(buf + offset,  buf_len);
		if (ret < 0)
			goto exit;
		count -= buf_len;
		offset += buf_len;
	}
	ret = offset;
exit:
	atomic_set(&sih_haptic->rtp.is_in_write_loop, 0);
	wake_up_interruptible(&sih_haptic->rtp.stop_wait_q);
	hp_info("%s: return size %d", __func__, ret);
	return ret;
}

static DEVICE_ATTR(f0, S_IWUSR | S_IRUGO,
	tracking_f0_show, tracking_f0_store);
static DEVICE_ATTR(seq, S_IWUSR | S_IRUGO,
	seq_show, seq_store);
static DEVICE_ATTR(reg, S_IWUSR | S_IRUGO,
	reg_show, reg_store);
static DEVICE_ATTR(gain, S_IWUSR | S_IRUGO,
	gain_show, gain_store);
static DEVICE_ATTR(state, S_IWUSR | S_IRUGO,
	state_show, state_store);
static DEVICE_ATTR(loop, S_IWUSR | S_IRUGO,
	loop_show, loop_store);
static DEVICE_ATTR(rtp, S_IWUSR | S_IRUGO,
	rtp_show, rtp_store);
static DEVICE_ATTR(cali, S_IWUSR | S_IRUGO,
	cali_show, cali_store);
static DEVICE_ATTR(cont_go, S_IWUSR | S_IRUGO,
	NULL, cont_go_store);
static DEVICE_ATTR(cont_seq0, S_IWUSR | S_IRUGO,
	cont_seq0_show, cont_seq0_store);
static DEVICE_ATTR(cont_seq1, S_IWUSR | S_IRUGO,
	cont_seq1_show, cont_seq1_store);
static DEVICE_ATTR(cont_seq2, S_IWUSR | S_IRUGO,
	cont_seq2_show, cont_seq2_store);
static DEVICE_ATTR(cont_asmooth, S_IWUSR | S_IRUGO,
	cont_asmooth_show, cont_asmooth_store);
static DEVICE_ATTR(cont_th_len, S_IWUSR | S_IRUGO,
	cont_th_len_show, cont_th_len_store);
static DEVICE_ATTR(cont_th_num, S_IWUSR | S_IRUGO,
	cont_th_num_show, cont_th_num_store);
static DEVICE_ATTR(cont_ampli, S_IWUSR | S_IRUGO,
	cont_ampli_show, cont_ampli_store);
static DEVICE_ATTR(ram, S_IWUSR | S_IRUGO,
	NULL, ram_store);
static DEVICE_ATTR(effect_id, S_IWUSR | S_IRUGO,
	effect_id_show, effect_id_store);
static DEVICE_ATTR(ram_num, S_IWUSR | S_IRUGO,
	ram_num_show, NULL);
static DEVICE_ATTR(detect_vbat, S_IWUSR | S_IRUGO,
	detect_vbat_show, NULL);
static DEVICE_ATTR(f0_save, S_IWUSR | S_IRUGO,
	f0_save_show, f0_save_store);
static DEVICE_ATTR(f0_value, S_IRUGO,
	f0_value_show,NULL);
static DEVICE_ATTR(tracking_f0, S_IWUSR | S_IRUGO,
	tracking_f0_show, NULL);
static DEVICE_ATTR(detect_f0, S_IWUSR | S_IRUGO,
	detect_f0_show, NULL);
static DEVICE_ATTR(duration, S_IWUSR | S_IRUGO,
	duration_show, duration_store);
static DEVICE_ATTR(osc_cali, S_IWUSR | S_IRUGO,
	osc_cali_show, osc_cali_store);
static DEVICE_ATTR(auto_pvdd, S_IWUSR | S_IRUGO,
	auto_pvdd_show, auto_pvdd_store);
static DEVICE_ATTR(ram_update, S_IWUSR | S_IRUGO,
	ram_update_show, ram_update_store);
static DEVICE_ATTR(ram_vbat_comp, S_IWUSR | S_IRUGO,
	ram_vbat_comp_show, ram_vbat_comp_store);
static DEVICE_ATTR(lra_resistance, S_IWUSR | S_IRUGO,
	lra_resistance_show, lra_resistance_store);
static DEVICE_ATTR(osc_save, S_IWUSR | S_IRUGO,
	osc_save_show, osc_save_store);
static DEVICE_ATTR(trig_para, S_IWUSR | S_IRUGO,
	trig_para_show, trig_para_store);
static DEVICE_ATTR(low_power, S_IWUSR | S_IRUGO,
	low_power_show, low_power_store);
static DEVICE_ATTR(activate, S_IWUSR | S_IRUGO,
	activate_show, activate_store);
static DEVICE_ATTR(drv_vboost, S_IWUSR | S_IRUGO,
	drv_vboost_show, drv_vboost_store);
static DEVICE_ATTR(burst_rw, S_IWUSR | S_IRUGO,
	burst_rw_show, burst_rw_store);
static DEVICE_ATTR(rtp_file, S_IWUSR | S_IRUGO,
	rtp_file_show, rtp_file_store);
static DEVICE_ATTR(main_loop, S_IWUSR | S_IRUGO,
	main_loop_show, main_loop_store);
static DEVICE_ATTR(seq_gain, S_IWUSR | S_IRUGO,
	seq_gain_show, seq_gain_store);
static DEVICE_ATTR(pwm_rate, S_IWUSR | S_IRUGO,
	pwm_rate_show, pwm_rate_store);
static DEVICE_ATTR(brake_ctrl, S_IWUSR | S_IRUGO,
	brake_ctrl_show, brake_ctrl_store);
static DEVICE_ATTR(detect_state, S_IWUSR | S_IRUGO,
	detect_state_show, detect_state_store);
static DEVICE_ATTR(custom_wave, S_IWUSR | S_IRUGO,
	custom_wave_show, custom_wave_store);
static DEVICE_ATTR(f0_check, S_IRUGO,sihaptic_f0_check_show,NULL);

static struct attribute *sih_vibra_attribute[] = {
		&dev_attr_f0.attr,
		&dev_attr_seq.attr,
		&dev_attr_reg.attr,
		&dev_attr_gain.attr,
		&dev_attr_state.attr,
		&dev_attr_loop.attr,
		&dev_attr_rtp.attr,
		&dev_attr_cali.attr,
		&dev_attr_cont_go.attr,
		&dev_attr_cont_seq0.attr,
		&dev_attr_cont_seq1.attr,
		&dev_attr_cont_seq2.attr,
		&dev_attr_cont_asmooth.attr,
		&dev_attr_cont_th_len.attr,
		&dev_attr_cont_th_num.attr,
		&dev_attr_cont_ampli.attr,
		&dev_attr_ram.attr,
		&dev_attr_effect_id.attr,
		&dev_attr_ram_num.attr,
		&dev_attr_duration.attr,
		&dev_attr_osc_cali.attr,
		&dev_attr_auto_pvdd.attr,
		&dev_attr_ram_update.attr,
		&dev_attr_ram_vbat_comp.attr,
		&dev_attr_lra_resistance.attr,
		&dev_attr_osc_save.attr,
		&dev_attr_f0_save.attr,
		&dev_attr_trig_para.attr,
		&dev_attr_low_power.attr,
		&dev_attr_activate.attr,
		&dev_attr_drv_vboost.attr,
		&dev_attr_burst_rw.attr,
		&dev_attr_detect_vbat.attr,
		&dev_attr_rtp_file.attr,
		&dev_attr_main_loop.attr,
		&dev_attr_seq_gain.attr,
		&dev_attr_pwm_rate.attr,
		&dev_attr_brake_ctrl.attr,
		&dev_attr_detect_state.attr,
		&dev_attr_tracking_f0.attr,
		&dev_attr_detect_f0.attr,
		&dev_attr_custom_wave.attr,
		&dev_attr_f0_check.attr,
		&dev_attr_f0_value.attr,
		NULL,
};

static struct attribute_group sih_vibra_attribute_group = {
		.attrs = sih_vibra_attribute,
};

static enum hrtimer_restart haptic_timer_func(struct hrtimer *timer)
{
	sih_haptic_t *sih_haptic = container_of(timer, sih_haptic_t, timer);

	hp_info("%s:enter!\n", __func__);
	sih_chip_state_recovery(sih_haptic);
	schedule_work(&sih_haptic->ram.ram_work);
	return HRTIMER_NORESTART;
}

static void ram_work_func(struct work_struct *work)
{
	sih_haptic_t *sih_haptic = container_of(work, sih_haptic_t, ram.ram_work);

	hp_info("%s:enter!\n", __func__);
	hp_info("%s:play_mode = %d, effect_id = %d, state = %d, duration = %d\n", __func__,
		sih_haptic->ram.action_mode, sih_haptic->chip_ipara.effect_id,
		sih_haptic->chip_ipara.state, sih_haptic->chip_ipara.duration);
	if (sih_haptic->chip_ipara.effect_id > sih_haptic->chip_ipara.effect_id_boundary) {
		hp_err("%s: effect id = %d, return.", __func__, sih_haptic->chip_ipara.effect_id);
		return;
	}
	mutex_lock(&sih_haptic->lock);
	/* Enter standby mode */
	sih_haptic->hp_func->stop(sih_haptic);
	if (sih_haptic->chip_ipara.state == SIH_ACTIVE_MODE) {
		sih_effect_strength_gain_adapter(sih_haptic);
		switch (sih_haptic->ram.action_mode) {
		case SIH_RAM_MODE:
			sih_haptic->hp_func->set_gain(sih_haptic,sih_haptic->chip_ipara.gain);
			sih_ram_play(sih_haptic, SIH_RAM_MODE);
			break;
		case SIH_RAM_LOOP_MODE:
			sih_haptic->hp_func->vbat_comp(sih_haptic);
			sih_ram_play(sih_haptic, SIH_RAM_LOOP_MODE);
			/* run ms timer */
			hrtimer_start(&sih_haptic->timer,
				ktime_set(sih_haptic->chip_ipara.duration / 1000,
				(sih_haptic->chip_ipara.duration % 1000) * 1000000),
				HRTIMER_MODE_REL);
			pm_stay_awake(sih_haptic->dev);
			sih_haptic->ram.ram_loop_lock = 1;
			break;
		default:
			hp_err("%s:err state = %d\n", __func__, sih_haptic->chip_ipara.state);
			break;
		}
	} else {
		if (sih_haptic->ram.ram_loop_lock) {
			pm_relax(sih_haptic->dev);
			sih_haptic->ram.ram_loop_lock = 0;
		}
	}
	mutex_unlock(&sih_haptic->lock);
}

static void rtp_work_func(struct work_struct *work)
{
	bool rtp_work_flag = false;
	int cnt = SIH_ENTER_RTP_MODE_MAX_TRY;
	int ret = -1;
	const struct firmware *rtp_file;
	sih_haptic_t *sih_haptic = container_of(work, sih_haptic_t, rtp.rtp_work);

	hp_info("%s:enter!\n", __func__);
	if ((sih_haptic->chip_ipara.effect_id < sih_haptic->chip_ipara.effect_id_boundary) &&
	    (sih_haptic->chip_ipara.effect_id > sih_haptic->chip_ipara.effect_max))
		return;
	hp_info("%s:action_mode = %d, effect_id = %d, state = %d\n", __func__,
		sih_haptic->ram.action_mode, sih_haptic->chip_ipara.effect_id,
		sih_haptic->chip_ipara.state);
	mutex_lock(&sih_haptic->lock);
	sih_haptic->hp_func->set_rtp_aei(sih_haptic, false);

	/* wait for irq to exit */
	atomic_set(&sih_haptic->rtp.exit_in_rtp_loop, 1);
	while (atomic_read(&sih_haptic->rtp.is_in_rtp_loop)) {
		hp_info("%s:goint to waiting irq exit\n", __func__);
		mutex_unlock(&sih_haptic->lock);
		ret = wait_event_interruptible(sih_haptic->rtp.wait_q,
				atomic_read(&sih_haptic->rtp.is_in_rtp_loop) == 0);
		hp_info("%s:wakeup\n", __func__);
		mutex_lock(&sih_haptic->lock);
		if (ret == -ERESTARTSYS) {
			atomic_set(&sih_haptic->rtp.exit_in_rtp_loop, 0);
			wake_up_interruptible(&sih_haptic->rtp.stop_wait_q);
			mutex_unlock(&sih_haptic->lock);
			hp_err("%s:wake up by signal return error\n", __func__);
			return;
		}
	}
	atomic_set(&sih_haptic->rtp.exit_in_rtp_loop, 0);
	wake_up_interruptible(&sih_haptic->rtp.stop_wait_q);
	/* how to force exit this call */
	if (sih_haptic->chip_ipara.is_custom_wave == 1 && sih_haptic->chip_ipara.state) {
		hp_info("%s:buffer size %d, availbe size %d\n",
		       __func__, sih_haptic->ram.base_addr >> 2,
		       get_rb_avalible_size());
		while (get_rb_avalible_size() < sih_haptic->ram.base_addr &&
		       !rb_shoule_exit()) {
			mutex_unlock(&sih_haptic->lock);
			ret = wait_event_interruptible(sih_haptic->rtp.stop_wait_q,
							(get_rb_avalible_size() >= sih_haptic->ram.base_addr) ||
							rb_shoule_exit());
			hp_info("%s:wakeup\n", __func__);
			hp_info("%s:after wakeup sbuffer size %d, availbe size %d\n",
			       __func__, sih_haptic->ram.base_addr >> 2,
			       get_rb_avalible_size());
			if (ret == -ERESTARTSYS) {
				hp_err("%s wake up by signal return erro\n",
				       __func__);
					return;
			}
			mutex_lock(&sih_haptic->lock);
		}
	}

	sih_haptic->hp_func->stop(sih_haptic);
	if (sih_haptic->chip_ipara.state) {
		pm_stay_awake(sih_haptic->dev);
		sih_effect_strength_gain_adapter(sih_haptic);
		sih_haptic->hp_func->set_gain(sih_haptic,sih_haptic->chip_ipara.gain);
		sih_haptic->rtp.rtp_init = false;
		if (sih_haptic->chip_ipara.is_custom_wave == 0) {
			sih_haptic->rtp.rtp_file_num = sih_haptic->chip_ipara.effect_id - 
					sih_haptic->chip_ipara.effect_id_boundary;
			hp_info("%s:rtp_file_num = %d\n", __func__,
			       sih_haptic->rtp.rtp_file_num);
			if (sih_haptic->rtp.rtp_file_num < 0)
				sih_haptic->rtp.rtp_file_num = 0;
			if (sih_haptic->rtp.rtp_file_num > (awinic_rtp_name_len - 1))
				sih_haptic->rtp.rtp_file_num = awinic_rtp_name_len - 1;
			ret = request_firmware(&rtp_file,
				awinic_rtp_name[sih_haptic->rtp.rtp_file_num],	sih_haptic->dev);
			if (ret < 0) {
				hp_err("%s:failed to read %s\n", __func__,
					awinic_rtp_name[sih_haptic->rtp.rtp_file_num]);
				sih_chip_state_recovery(sih_haptic);
				pm_relax(sih_haptic->dev);
				mutex_unlock(&sih_haptic->lock);
				return;
			}
			sih_vfree_container(sih_haptic, sih_haptic->rtp.rtp_cont);
			sih_haptic->rtp.rtp_cont = vmalloc(rtp_file->size + sizeof(int));
			if (!sih_haptic->rtp.rtp_cont) {
				release_firmware(rtp_file);
				hp_err("%s:error allocating memory\n", __func__);
				sih_chip_state_recovery(sih_haptic);
				pm_relax(sih_haptic->dev);
				mutex_unlock(&sih_haptic->lock);
				return;
			}
			sih_haptic->rtp.rtp_cont->len = rtp_file->size;
			memcpy(sih_haptic->rtp.rtp_cont->data, rtp_file->data, rtp_file->size);
			release_firmware(rtp_file);
			hp_info("%s:rtp len is %d\n", __func__, sih_haptic->rtp.rtp_cont->len);
		} else {
			sih_vfree_container(sih_haptic, sih_haptic->rtp.rtp_cont);
			sih_haptic->rtp.rtp_cont = NULL;
			if(sih_haptic->ram.base_addr != 0) {
				sih_haptic->rtp.rtp_cont = vmalloc((sih_haptic->ram.base_addr >> 2) + sizeof(int));
			} else {
				hp_err("%s:ram update not done yet, return!", __func__);
			}
			if (!sih_haptic->rtp.rtp_cont) {
				hp_err("%s:error allocating memory\n",
				       __func__);
				pm_relax(sih_haptic->dev);
				mutex_unlock(&sih_haptic->lock);
				return;
			}
		}
		sih_haptic->rtp.rtp_init = true;	
		sih_haptic->hp_func->check_detect_state(sih_haptic, SIH_RTP_MODE);
		sih_haptic->hp_func->set_play_mode(sih_haptic, SIH_RTP_MODE);
		sih_haptic->hp_func->play_go(sih_haptic, true);
		usleep_range(2000, 2500);
		while (cnt--) {
			if (sih_haptic->hp_func->if_chip_is_mode(sih_haptic, SIH_RTP_MODE)) {
				rtp_work_flag = true;
				hp_info("%s:rtp go!\n", __func__);
				break;
			}
			hp_info("%s:wait for rtp go!\n", __func__);
			usleep_range(2000, 2500);
		}
		if (rtp_work_flag) {
			sih_rtp_play(sih_haptic, SIH_RTP_NORMAL_PLAY);
		} else {
			sih_haptic->hp_func->stop(sih_haptic);
			sih_haptic->chip_ipara.state = SIH_STANDBY_MODE;
			pm_relax(sih_haptic->dev);
			hp_err("%s:rtp go failed! not enter rtp status!\n", __func__);
		}
	} else {
		sih_haptic->rtp.rtp_cnt = 0;
		sih_haptic->rtp.rtp_init = false;	
		pm_relax(sih_haptic->dev);
	}
	mutex_unlock(&sih_haptic->lock);
}

static int vibrator_chip_init(sih_haptic_t *sih_haptic)
{
	int ret = -1;

	ret = sih_haptic->hp_func->efuse_check(sih_haptic);
	if (ret < 0)
		return ret;

	sih_haptic->hp_func->init(sih_haptic);
	sih_haptic->hp_func->stop(sih_haptic);
	/* load lra reg config */
	ret = sih_lra_config_load(sih_haptic);
	if (ret < 0)
		return ret;
	hp_info("%s:end\n", __func__);

	return ret;
}

static void vibrator_init(sih_haptic_t *sih_haptic)
{
	/* timer init */
	hrtimer_init(&sih_haptic->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	sih_haptic->timer.function = haptic_timer_func;
	/* work func init */
	INIT_WORK(&sih_haptic->ram.ram_work, ram_work_func);
	INIT_WORK(&sih_haptic->rtp.rtp_work, rtp_work_func);
	/* mutex init */
	mutex_init(&sih_haptic->lock);
	mutex_init(&sih_haptic->rtp.rtp_lock);
	atomic_set(&sih_haptic->rtp.is_in_rtp_loop, 0);
	atomic_set(&sih_haptic->rtp.exit_in_rtp_loop, 0);
	atomic_set(&sih_haptic->rtp.is_in_write_loop, 0);
	init_waitqueue_head(&sih_haptic->rtp.wait_q);
	init_waitqueue_head(&sih_haptic->rtp.stop_wait_q);
}

static int ram_work_init(sih_haptic_t *sih_haptic)
{
	int ret = -1;

	hp_info("%s:enter\n", __func__);

	sih_haptic->ram.ram_init = false;

	ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_UEVENT,
		awinic_ram_name[sih_haptic->ram.lib_index], sih_haptic->dev,
		GFP_KERNEL, sih_haptic, sih_ram_load);

	return ret;
}

static int sih_i2c_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id)
{
	sih_haptic_t *sih_haptic;
	struct input_dev *input_dev;
	struct ff_device *ff;
	struct device_node *np = i2c->dev.of_node;
	int effect_count_max;
	int ret = -1;

	/* I2C Adapter capability detection */
	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		hp_err("%s:i2c algorithm ability detect failed\n", __func__);
		return -EIO;
	}

	/* Allocate cache space for haptic device */
	sih_haptic = devm_kzalloc(&i2c->dev, sizeof(sih_haptic_t), GFP_KERNEL);
	if (sih_haptic == NULL)
		return -ENOMEM;

	input_dev = devm_input_allocate_device(&i2c->dev);
	if (!input_dev)
		return -ENOMEM;
	input_dev->name = "si_haptic";
	sih_haptic->dev = &i2c->dev;
	sih_haptic->i2c = i2c;
	device_init_wakeup(sih_haptic->dev, true);
	i2c_set_clientdata(i2c, sih_haptic);

	xm_hap_driver_init(true);

	/* matching dts */
	ret = sih_parse_dts(&i2c->dev, sih_haptic, np);
	if (ret)
		goto err_parse_dts;

	/* acquire gpio resources and read id code */
	ret = sih_acquire_prepare_res(&i2c->dev, sih_haptic);
	if (ret)
		goto err_prepare_res;

	/* Registers chip manipulation functions */
	ret = sih_register_func(sih_haptic);
	if (ret)
		goto err_id;

	/* registers regmap */
	sih_haptic->regmapp.regmapping = haptic_regmap_init(i2c,
		sih_haptic->regmapp.config);
	if (sih_haptic->regmapp.regmapping == NULL)
		goto err_regmap;

	/* handle gpio irq */
	ret = sih_acquire_irq_res(&i2c->dev, sih_haptic);
	if (ret)
		goto err_irq;

	CUSTOME_WAVE_ID = sih_haptic->chip_ipara.effect_max;
	/* input device config */
	input_set_drvdata(input_dev, sih_haptic);
	sih_haptic->soft_frame.vib_dev = *input_dev;
	input_set_capability(input_dev, EV_FF, FF_CONSTANT);
	input_set_capability(input_dev, EV_FF, FF_GAIN);
	input_set_capability(input_dev, EV_FF, FF_PERIODIC);
	input_set_capability(input_dev, EV_FF, FF_CUSTOM);
	sih_haptic->chip_ipara.effects_count = 10;
	if (sih_haptic->chip_ipara.effects_count + 1 > FF_EFFECT_COUNT_MAX)
		effect_count_max = sih_haptic->chip_ipara.effects_count  + 1;
	else
		effect_count_max = FF_EFFECT_COUNT_MAX;
	ret = input_ff_create(input_dev, effect_count_max);
	if (ret < 0) {
		hp_err("%s create FF input device failed, ret=%d\n",
			__func__, ret);
		goto err_input_ff;
	}

	INIT_WORK(&sih_haptic->ram.gain_work, sih_set_gain_work_routine);
	ff = input_dev->ff;
	ff->upload = sih_upload_effect;
	ff->playback = sih_playback;
	ff->erase = sih_erase;
	ff->set_gain = sih_set_gain;
	ret = input_register_device(input_dev);
	if (ret < 0) {
		hp_err("%s register input device failed, ret=%d\n",
			__func__, ret);
		XM_HAP_REGISTER_EXCEPTION("Device register", "input_register_device");
		goto err_destroy_ff;
	}

	ret = create_rb();
	if (ret < 0) {
		XM_HAP_REGISTER_EXCEPTION("Hardware", "haptics_hw_init");
		hp_info("%s: error creating ringbuffer\n", __func__);
		goto err_rb;
	}

    /* vibrator sysfs node create */
	ret = sysfs_create_group(&sih_haptic->i2c->dev.kobj,
		&sih_vibra_attribute_group);
	if (ret < 0) {
		hp_err("%s:sysfs node create failed = %d\n ", __func__, ret);
		return ret;
	}
	/* vibrator globle ptr init */
	g_haptic_t.sih_num = SIH_HAPTIC_DEV_NUM;
	g_haptic_t.g_haptic[SIH_HAPTIC_MMAP_DEV_INDEX] = sih_haptic;

	/* vibrator init */
	vibrator_init(sih_haptic);

	sih_haptic->work_queue = create_singlethread_workqueue("sih_haptic_work_queue");
	if (!sih_haptic->work_queue) {
		hp_err("%s: Error creating sih_haptic_work_queue\n",
			__func__);
		goto err_dev_sysfs;
	}
	/* vibrator chip init */
	ret = vibrator_chip_init(sih_haptic);
	if (ret)
		goto err_dev_sysfs;

	sih_haptic->hp_func->get_tracking_f0(sih_haptic);
	/* ram work init */
	ret = ram_work_init(sih_haptic);
	if (ret)
		goto err_dev_sysfs;

	ret = sih_haptic->stream_func->stream_rtp_work_init(sih_haptic);
	if (ret)
		goto err_dev_sysfs;

	hp_info("%s:end\n", __func__);
	return ret;

err_rb:
err_destroy_ff:
	input_ff_destroy(&sih_haptic->soft_frame.vib_dev);
err_input_ff:
err_dev_sysfs:
	sih_haptic->stream_func->stream_rtp_work_release(sih_haptic);
	devm_free_irq(&i2c->dev, gpio_to_irq(sih_haptic->chip_attr.irq_gpio), sih_haptic);
err_irq:
err_regmap:
err_id:
err_prepare_res:
err_parse_dts:
	devm_kfree(&i2c->dev, sih_haptic);

	sih_haptic = NULL;
	hp_info("%s:error, ret = %d\n", __func__,ret);
	return ret;
}

static void sih_i2c_remove(struct i2c_client *i2c)
{
	sih_haptic_t *sih_haptic = i2c_get_clientdata(i2c);
	hp_info("%s:enter\n", __func__);

	/* work_struct release */
	cancel_work_sync(&sih_haptic->ram.ram_work);
	cancel_work_sync(&sih_haptic->rtp.rtp_work);

	/* hrtimer release */
	hrtimer_cancel(&sih_haptic->timer);

	/* mutex release */
	mutex_destroy(&sih_haptic->lock);
	mutex_destroy(&sih_haptic->rtp.rtp_lock);

	/* input device release */
	input_unregister_device(&sih_haptic->soft_frame.vib_dev);

	/* irq release */
	devm_free_irq(&i2c->dev, gpio_to_irq(sih_haptic->chip_attr.irq_gpio),
		sih_haptic);

	/* gpio release */
	/* GPIOs requested with devm_gpio_request will be automatically freed on driver detach */

	/* regmap exit */
	haptic_regmap_remove(sih_haptic->regmapp.regmapping);

	/* container release */
	sih_vfree_container(sih_haptic, sih_haptic->rtp.rtp_cont);
	/* reg addr release */
	if (sih_haptic->chip_reg.reg_addr != NULL)
		kfree(sih_haptic->chip_reg.reg_addr);

	sih_haptic->stream_func->stream_rtp_work_release(sih_haptic);
}

static int sih_suspend(struct device *dev)
{
	int ret = 0;
	sih_haptic_t *sih_haptic = dev_get_drvdata(dev);

	mutex_lock(&sih_haptic->lock);
	/* chip stop */
	mutex_unlock(&sih_haptic->lock);

	return ret;
}

static int sih_resume(struct device *dev)
{
	int ret = 0;

	hp_info("%s:resume\n", __func__);

	return ret;
}

static SIMPLE_DEV_PM_OPS(sih_pm_ops, sih_suspend, sih_resume);

static const struct i2c_device_id sih_i2c_id[] = {
	{SIH_HAPTIC_NAME_688X, 0},
	{}
};

static struct of_device_id sih_dt_match[] = {
	{.compatible = "silicon,sih_haptic_688X"},
	{},
};

static struct i2c_driver sih_i2c_driver = {
	.driver = {
		.name = SIH_HAPTIC_NAME_688X,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sih_dt_match),
		.pm = &sih_pm_ops,
	},
	.probe = sih_i2c_probe,
	.remove = sih_i2c_remove,
	.id_table = sih_i2c_id,
};

static int __init sih_i2c_init(void)
{
	int ret = -1;

	ret = i2c_add_driver(&sih_i2c_driver);

	hp_info("%s:i2c_add_driver,ret = %d\n", __func__, ret);

	if (ret) {
		hp_err("%s:fail to add haptic device,ret = %d\n", __func__, ret);
		return ret;
	}
	ret = sih_add_misc_dev();
	if (ret) {
		hp_err("%s:misc fail:%d\n", __func__, ret);
		return ret;
	}
	return 0;
}

static void __exit sih_i2c_exit(void)
{
	i2c_del_driver(&sih_i2c_driver);
}

module_init(sih_i2c_init);
module_exit(sih_i2c_exit);

MODULE_AUTHOR("tianchi zheng <tianchi.zheng@si-in.com>");
MODULE_DESCRIPTION("Haptic Driver V1.0.3.691");
MODULE_LICENSE("GPL v2");
MODULE_SOFTDEP("pre: aw8697-haptic");