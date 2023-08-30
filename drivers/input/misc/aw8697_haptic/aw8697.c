/*
 * aw8697.c   aw8697 haptic module
 *
 * Version: v1.1.3
 *
 * Copyright (c) 2019 AWINIC Technology CO., LTD
 *
 *  Author: Nick Li <zhangzetao@awinic.com.cn>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#define DEBUG
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
#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include <linux/syscalls.h>
#include <linux/power_supply.h>
#include <linux/pm_qos.h>
#include "aw8697_config.h"
#include "aw8697_reg.h"
#include "aw8697.h"
#include  "ringbuffer.h"


/******************************************************
 *
 * Marco
 *
 ******************************************************/
#define AW8697_I2C_NAME "aw8697_haptic"
#define AW8697_HAPTIC_NAME "aw8697_haptic"

#define AW8697_VERSION "v1.1.3"

#define AWINIC_RAM_UPDATE_DELAY

#define AW_I2C_RETRIES 2
#define AW_I2C_RETRY_DELAY 2
#define AW_READ_CHIPID_RETRIES 5
#define AW_READ_CHIPID_RETRY_DELAY 2

#define AW8697_MAX_DSP_START_TRY_COUNT    10

#define AW8697_MAX_FIRMWARE_LOAD_CNT 20

#define AW8697_MAX_BST_VO 0x1f

#define OSC_CALIBRATION_T_LENGTH 5100000
#define CPU_LATENCY_QOC_VALUE (0)
static struct pm_qos_request pm_qos_req;
/******************************************************
 *
 * variable
 *
 ******************************************************/
#define AW8697_RTP_NAME_MAX        64
static char *aw8697_ram_name = "aw8697_haptic.bin";
#ifdef TEST_RTP
static char aw8697_rtp_name[][AW8697_RTP_NAME_MAX] = {
	{"aw8697_rtp_1.bin"},
};
#else
static char aw8697_rtp_name[][AW8697_RTP_NAME_MAX] = {
	{"aw8697_rtp_1.bin"}, //8
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
	//{"aw8697_rtp_1.bin"},
	//{"aw8697_rtp_1.bin"},
	{"AcousticGuitar_RTP.bin"}, //21
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
	{"Raindrops_RTP.bin"},
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
	{"Swoosh_RTP.bin"}, //71
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
	{"FaceID_Wrong2_RTP.bin"}, //82
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
	{"aw8697_rtp_1.bin"}, //99
	{"aw8697_rtp_1.bin"}, //100
	{"offline_countdown_RTP.bin"},
	{"scene_bomb_injury_RTP.bin"},
	{"scene_bomb_RTP.bin"}, //103
	{"door_open_RTP.bin"},
	{"aw8697_rtp_1.bin"},
	{"scene_step_RTP.bin"}, //106
	{"crawl_RTP.bin"},
	{"scope_on_RTP.bin"},
	{"scope_off_RTP.bin"},
	{"magazine_quick_RTP.bin"},
	{"grenade_RTP.bin"},
	{"scene_getshot_RTP.bin"}, //112
	{"grenade_explosion_RTP.bin"},
	{"punch_RTP.bin"},
	{"pan_RTP.bin"},
	{"bandage_RTP.bin"},
	{"aw8697_rtp_1.bin"},
	{"scene_jump_RTP.bin"},
	{"vehicle_plane_RTP.bin"}, //119
	{"scene_openparachute_RTP.bin"}, //120
	{"scene_closeparachute_RTP.bin"}, //121
	{"vehicle_collision_RTP.bin"},
	{"vehicle_buggy_RTP.bin"}, //123
	{"vehicle_dacia_RTP.bin"}, //124
	{"vehicle_moto_RTP.bin"}, //125
	{"firearms_akm_RTP.bin"}, //126
	{"firearms_m16a4_RTP.bin"}, //127
	{"aw8697_rtp_1.bin"},
	{"firearms_awm_RTP.bin"}, //129
	{"firearms_mini14_RTP.bin"}, //130
	{"firearms_vss_RTP.bin"}, //131
	{"firearms_qbz_RTP.bin"}, //132
	{"firearms_ump9_RTP.bin"}, //133
	{"firearms_dp28_RTP.bin"}, //134
	{"firearms_s1897_RTP.bin"}, //135
	{"aw8697_rtp_1.bin"},
	{"firearms_p18c_RTP.bin"}, //137
	{"aw8697_rtp_1.bin"},
	{"aw8697_rtp_1.bin"},
	{"CFM_KillOne_RTP.bin"},
	{"CFM_Headshot_RTP.bin"}, //141
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
	{"CFM_Weapon_BLT_Shoot_RTP.bin"}, //153
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
#endif
static int CUSTOME_WAVE_ID;
struct aw8697_container *aw8697_rtp;
struct aw8697 *g_aw8697;

/******************************************************
 *
 * functions
 *
 ******************************************************/
static void aw8697_interrupt_clear(struct aw8697 *aw8697);
static int aw8697_haptic_trig_enable_config(struct aw8697 *aw8697);
static int aw8697_haptic_get_vbat(struct aw8697 *aw8697);

 /******************************************************
 *
 * aw8697 i2c write/read
 *
 ******************************************************/
static int aw8697_i2c_write(struct aw8697 *aw8697,
			    unsigned char reg_addr, unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret =
		    i2c_smbus_write_byte_data(aw8697->i2c, reg_addr, reg_data);
		if (ret < 0) {
			pr_err("%s: i2c_write cnt=%d error=%d\n", __func__, cnt,
			       ret);
		} else {
			break;
		}
		cnt++;
		msleep(AW_I2C_RETRY_DELAY);
	}

	return ret;
}

static int aw8697_i2c_read(struct aw8697 *aw8697,
			   unsigned char reg_addr, unsigned char *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_smbus_read_byte_data(aw8697->i2c, reg_addr);
		if (ret < 0) {
			pr_err("%s: i2c_read cnt=%d error=%d\n", __func__, cnt,
			       ret);
		} else {
			*reg_data = ret;
			break;
		}
		cnt++;
		msleep(AW_I2C_RETRY_DELAY);
	}

	return ret;
}

static int aw8697_i2c_write_bits(struct aw8697 *aw8697,
				 unsigned char reg_addr, unsigned int mask,
				 unsigned char reg_data)
{
	unsigned char reg_val = 0;

	aw8697_i2c_read(aw8697, reg_addr, &reg_val);
	reg_val &= mask;
	reg_val |= reg_data;
	aw8697_i2c_write(aw8697, reg_addr, reg_val);

	return 0;
}

static int aw8697_i2c_writes(struct aw8697 *aw8697,
			     unsigned char reg_addr, unsigned char *buf,
			     unsigned int len)
{
	int ret = -1;
	unsigned char *data;

	data = kmalloc(len + 1, GFP_KERNEL);
	if (data == NULL) {
		pr_err("%s: can not allocate memory\n", __func__);
		return -ENOMEM;
	}

	data[0] = reg_addr;
	memcpy(&data[1], buf, len);

	ret = i2c_master_send(aw8697->i2c, data, len + 1);
	if (ret < 0) {
		pr_err("%s: i2c master send error\n", __func__);
	}

	kfree(data);

	return ret;
}

/*****************************************************
 *
 * ram update
 *
 *****************************************************/
static void aw8697_rtp_loaded(const struct firmware *cont, void *context)
{
	struct aw8697 *aw8697 = context;
	pr_info("%s enter\n", __func__);

	if (!cont) {
		pr_err("%s: failed to read %s\n", __func__,
		       aw8697_rtp_name[aw8697->rtp_file_num]);
		release_firmware(cont);
		return;
	}

	pr_info("%s: loaded %s - size: %zu\n", __func__,
		aw8697_rtp_name[aw8697->rtp_file_num], cont ? cont->size : 0);

	/* aw8697 rtp update */
	aw8697_rtp = vmalloc(cont->size + sizeof(int));
	if (!aw8697_rtp) {
		release_firmware(cont);
		pr_err("%s: Error allocating memory\n", __func__);
		return;
	}
	aw8697_rtp->len = cont->size;
	pr_info("%s: rtp size = %d\n", __func__, aw8697_rtp->len);
	memcpy(aw8697_rtp->data, cont->data, cont->size);
	release_firmware(cont);

	aw8697->rtp_init = 1;
	pr_info("%s: rtp update complete\n", __func__);
}

static int aw8697_rtp_update(struct aw8697 *aw8697)
{
	pr_info("%s enter\n", __func__);

	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				       aw8697_rtp_name[aw8697->rtp_file_num],
				       aw8697->dev, GFP_KERNEL, aw8697,
				       aw8697_rtp_loaded);
}

static void aw8697_container_update(struct aw8697 *aw8697,
				    struct aw8697_container *aw8697_cont)
{
	int i = 0;
	unsigned int shift = 0;

	pr_info("%s enter\n", __func__);

	mutex_lock(&aw8697->lock);

	aw8697->ram.baseaddr_shift = 2;
	aw8697->ram.ram_shift = 4;

	/* RAMINIT Enable */
	aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSCTRL,
			      AW8697_BIT_SYSCTRL_RAMINIT_MASK,
			      AW8697_BIT_SYSCTRL_RAMINIT_EN);

	/* base addr */
	shift = aw8697->ram.baseaddr_shift;
	aw8697->ram.base_addr =
	    (unsigned int)((aw8697_cont->data[0 + shift] << 8) |
			   (aw8697_cont->data[1 + shift]));
	pr_info("%s: base_addr=0x%4x\n", __func__, aw8697->ram.base_addr);

	aw8697_i2c_write(aw8697, AW8697_REG_BASE_ADDRH,
			 aw8697_cont->data[0 + shift]);
	aw8697_i2c_write(aw8697, AW8697_REG_BASE_ADDRL,
			 aw8697_cont->data[1 + shift]);

	aw8697_i2c_write(aw8697, AW8697_REG_FIFO_AEH,
			 (unsigned char)((aw8697->ram.base_addr >> 2) >> 8));
	aw8697_i2c_write(aw8697, AW8697_REG_FIFO_AEL,
			 (unsigned char)((aw8697->ram.base_addr >> 2) &
					 0x00FF));
	aw8697_i2c_write(aw8697, AW8697_REG_FIFO_AFH,
			 (unsigned
			  char)((aw8697->ram.base_addr -
				 (aw8697->ram.base_addr >> 2)) >> 8));
	aw8697_i2c_write(aw8697, AW8697_REG_FIFO_AFL,
			 (unsigned
			  char)((aw8697->ram.base_addr -
				 (aw8697->ram.base_addr >> 2)) & 0x00FF));

	/* ram */
	shift = aw8697->ram.baseaddr_shift;
	aw8697_i2c_write(aw8697, AW8697_REG_RAMADDRH,
			 aw8697_cont->data[0 + shift]);
	aw8697_i2c_write(aw8697, AW8697_REG_RAMADDRL,
			 aw8697_cont->data[1 + shift]);
	shift = aw8697->ram.ram_shift;
	for (i = shift; i < aw8697_cont->len; i++) {
		aw8697_i2c_write(aw8697, AW8697_REG_RAMDATA,
				 aw8697_cont->data[i]);
	}

	/* RAMINIT Disable */
	aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSCTRL,
			      AW8697_BIT_SYSCTRL_RAMINIT_MASK,
			      AW8697_BIT_SYSCTRL_RAMINIT_OFF);

	mutex_unlock(&aw8697->lock);

	pr_info("%s exit\n", __func__);
}

static void aw8697_ram_loaded(const struct firmware *cont, void *context)
{
	struct aw8697 *aw8697 = context;
	struct aw8697_container *aw8697_fw;
	int i = 0;
	unsigned short check_sum = 0;

	pr_info("%s enter\n", __func__);

	if (!cont) {
		pr_err("%s: failed to read %s\n", __func__, aw8697_ram_name);
		release_firmware(cont);
		return;
	}

	pr_info("%s: loaded %s - size: %zu\n", __func__, aw8697_ram_name,
		cont ? cont->size : 0);
	/*
	for(i=0; i<cont->size; i++) {
		pr_info("%s: addr:0x%04x, data:0x%02x\n", __func__, i, *(cont->data+i));
	}
	*/

	/* check sum */
	for (i = 2; i < cont->size; i++) {
		check_sum += cont->data[i];
	}
	if (check_sum !=
	    (unsigned short)((cont->data[0] << 8) | (cont->data[1]))) {
		pr_err("%s: check sum err: check_sum=0x%04x\n", __func__,
		       check_sum);
		return;
	} else {
		pr_info("%s: check sum pass : 0x%04x\n", __func__, check_sum);
		aw8697->ram.check_sum = check_sum;
	}

	/* aw8697 ram update */
	aw8697_fw = kzalloc(cont->size + sizeof(int), GFP_KERNEL);
	if (!aw8697_fw) {
		release_firmware(cont);
		pr_err("%s: Error allocating memory\n", __func__);
		return;
	}
	aw8697_fw->len = cont->size;
	memcpy(aw8697_fw->data, cont->data, cont->size);
	release_firmware(cont);

	aw8697_container_update(aw8697, aw8697_fw);

	aw8697->ram.len = aw8697_fw->len;

	kfree(aw8697_fw);

	aw8697->ram_init = 1;
	pr_info("%s: fw update complete\n", __func__);

	aw8697_haptic_trig_enable_config(aw8697);

	aw8697_rtp_update(aw8697);
}

static int aw8697_ram_update(struct aw8697 *aw8697)
{
	aw8697->ram_init = 0;
	aw8697->rtp_init = 0;
	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				       aw8697_ram_name, aw8697->dev, GFP_KERNEL,
				       aw8697, aw8697_ram_loaded);
}

#ifdef AWINIC_RAM_UPDATE_DELAY
static void aw8697_ram_work_routine(struct work_struct *work)
{
	struct aw8697 *aw8697 =
	    container_of(work, struct aw8697, ram_work.work);

	pr_info("%s enter\n", __func__);

	aw8697_ram_update(aw8697);

}
#endif

static int aw8697_ram_init(struct aw8697 *aw8697)
{
#ifdef AWINIC_RAM_UPDATE_DELAY
	int ram_timer_val = 5000;
	INIT_DELAYED_WORK(&aw8697->ram_work, aw8697_ram_work_routine);
	//schedule_delayed_work(&aw8697->ram_work,
			      //msecs_to_jiffies(ram_timer_val));
	queue_delayed_work(aw8697->work_queue, &aw8697->ram_work,  msecs_to_jiffies(ram_timer_val));
#else
	aw8697_ram_update(aw8697);
#endif
	return 0;
}

/*****************************************************
 *
 * haptic control
 *
 *****************************************************/
static int aw8697_haptic_softreset(struct aw8697 *aw8697)
{
	pr_debug("%s enter\n", __func__);

	aw8697_i2c_write(aw8697, AW8697_REG_ID, 0xAA);
	msleep(1);
	return 0;
}

static int aw8697_haptic_active(struct aw8697 *aw8697)
{
	pr_debug("%s enter\n", __func__);

	aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSCTRL,
			      AW8697_BIT_SYSCTRL_WORK_MODE_MASK,
			      AW8697_BIT_SYSCTRL_ACTIVE);
	aw8697_interrupt_clear(aw8697);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSINTM,
			      AW8697_BIT_SYSINTM_UVLO_MASK,
			      AW8697_BIT_SYSINTM_UVLO_EN);
	return 0;
}

static int aw8697_haptic_play_mode(struct aw8697 *aw8697,
				   unsigned char play_mode)
{
	pr_debug("%s enter\n", __func__);

	switch (play_mode) {
	case AW8697_HAPTIC_STANDBY_MODE:
		aw8697->play_mode = AW8697_HAPTIC_STANDBY_MODE;
		aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSINTM,
				      AW8697_BIT_SYSINTM_UVLO_MASK,
				      AW8697_BIT_SYSINTM_UVLO_OFF);
		aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSCTRL,
				      AW8697_BIT_SYSCTRL_WORK_MODE_MASK,
				      AW8697_BIT_SYSCTRL_STANDBY);
		break;
	case AW8697_HAPTIC_RAM_MODE:
		aw8697->play_mode = AW8697_HAPTIC_RAM_MODE;
		aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSCTRL,
				      AW8697_BIT_SYSCTRL_PLAY_MODE_MASK,
				      AW8697_BIT_SYSCTRL_PLAY_MODE_RAM);
		aw8697_haptic_active(aw8697);
		aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSCTRL,
				      AW8697_BIT_SYSCTRL_BST_MODE_MASK,
				      AW8697_BIT_SYSCTRL_BST_MODE_BOOST);
		break;
	case AW8697_HAPTIC_RAM_LOOP_MODE:
		aw8697->play_mode = AW8697_HAPTIC_RAM_LOOP_MODE;
		aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSCTRL,
				      AW8697_BIT_SYSCTRL_PLAY_MODE_MASK,
				      AW8697_BIT_SYSCTRL_PLAY_MODE_RAM);
		aw8697_haptic_active(aw8697);
		aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSCTRL,
				      AW8697_BIT_SYSCTRL_BST_MODE_MASK,
				      AW8697_BIT_SYSCTRL_BST_MODE_BYPASS);
		break;
	case AW8697_HAPTIC_RTP_MODE:
		aw8697->play_mode = AW8697_HAPTIC_RTP_MODE;
		aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSCTRL,
				      AW8697_BIT_SYSCTRL_PLAY_MODE_MASK,
				      AW8697_BIT_SYSCTRL_PLAY_MODE_RTP);
		aw8697_haptic_active(aw8697);
		aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSCTRL,
				      AW8697_BIT_SYSCTRL_BST_MODE_MASK,
				      AW8697_BIT_SYSCTRL_BST_MODE_BOOST);
		break;
	case AW8697_HAPTIC_TRIG_MODE:
		aw8697->play_mode = AW8697_HAPTIC_TRIG_MODE;
		aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSCTRL,
				      AW8697_BIT_SYSCTRL_PLAY_MODE_MASK,
				      AW8697_BIT_SYSCTRL_PLAY_MODE_RAM);
		aw8697_haptic_active(aw8697);
		break;
	case AW8697_HAPTIC_CONT_MODE:
		aw8697->play_mode = AW8697_HAPTIC_CONT_MODE;
		aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSCTRL,
				      AW8697_BIT_SYSCTRL_PLAY_MODE_MASK,
				      AW8697_BIT_SYSCTRL_PLAY_MODE_CONT);
		aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSCTRL,
				      AW8697_BIT_SYSCTRL_BST_MODE_MASK,
				      AW8697_BIT_SYSCTRL_BST_MODE_BYPASS);
		aw8697_haptic_active(aw8697);
		break;
	default:
		dev_err(aw8697->dev, "%s: play mode %d err",
			__func__, play_mode);
		break;
	}
	return 0;
}

static int aw8697_haptic_play_go(struct aw8697 *aw8697, bool flag)
{
	if (flag == true) {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_GO,
				      AW8697_BIT_GO_MASK, AW8697_BIT_GO_ENABLE);
	} else {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_GO,
				      AW8697_BIT_GO_MASK,
				      AW8697_BIT_GO_DISABLE);
	}
	return 0;
}

static int aw8697_haptic_stop_delay(struct aw8697 *aw8697)
{
	unsigned char reg_val = 0;
	unsigned int cnt = 100;

	while (cnt--) {
		aw8697_i2c_read(aw8697, AW8697_REG_GLB_STATE, &reg_val);
		if ((reg_val & 0x0f) == 0x00) {
			return 0;
		}
		msleep(2);
		pr_debug("%s wait for standby, reg glb_state=0x%02x\n",
			 __func__, reg_val);
	}
	pr_err("%s do not enter standby automatically\n", __func__);

	return 0;
}

static int aw8697_haptic_stop(struct aw8697 *aw8697)
{
	pr_debug("%s enter\n", __func__);

	aw8697_haptic_play_go(aw8697, false);
	aw8697_haptic_stop_delay(aw8697);
	aw8697_haptic_play_mode(aw8697, AW8697_HAPTIC_STANDBY_MODE);

	return 0;
}

static int aw8697_haptic_start(struct aw8697 *aw8697)
{
	pr_debug("%s enter\n", __func__);

	aw8697_haptic_play_go(aw8697, true);

	return 0;
}

static int aw8697_haptic_set_wav_seq(struct aw8697 *aw8697,
				     unsigned char wav, unsigned char seq)
{
	aw8697_i2c_write(aw8697, AW8697_REG_WAVSEQ1 + wav, seq);
	return 0;
}

static int aw8697_haptic_set_wav_loop(struct aw8697 *aw8697,
				      unsigned char wav, unsigned char loop)
{
	unsigned char tmp = 0;

	if (wav % 2) {
		tmp = loop << 0;
		aw8697_i2c_write_bits(aw8697, AW8697_REG_WAVLOOP1 + (wav / 2),
				      AW8697_BIT_WAVLOOP_SEQNP1_MASK, tmp);
	} else {
		tmp = loop << 4;
		aw8697_i2c_write_bits(aw8697, AW8697_REG_WAVLOOP1 + (wav / 2),
				      AW8697_BIT_WAVLOOP_SEQN_MASK, tmp);
	}

	return 0;
}

static int aw8697_haptic_set_repeat_wav_seq(struct aw8697 *aw8697,
					    unsigned char seq)
{
	aw8697_haptic_set_wav_seq(aw8697, 0x00, seq);
	aw8697_haptic_set_wav_loop(aw8697, 0x00,
				   AW8697_BIT_WAVLOOP_INIFINITELY);

	return 0;
}

static int aw8697_haptic_set_bst_vol(struct aw8697 *aw8697,
				     unsigned char bst_vol)
{
	if (bst_vol & 0xe0) {
		bst_vol = 0x1f;
	}
	//printk("%s %d --\n", __func__, __LINE__);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_BSTDBG4,
			      AW8697_BIT_BSTDBG4_BSTVOL_MASK, (bst_vol << 1));
	return 0;
}

static int aw8697_haptic_set_bst_peak_cur(struct aw8697 *aw8697,
					  unsigned char peak_cur)
{
	peak_cur &= AW8697_BSTCFG_PEAKCUR_LIMIT;
	pr_info("%s  %d enter\n", __func__, __LINE__);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_BSTCFG,
			      AW8697_BIT_BSTCFG_PEAKCUR_MASK, peak_cur);
	return 0;
}

static int aw8697_haptic_set_gain(struct aw8697 *aw8697, unsigned char gain)
{
	int comp_gain = 0;

	if (aw8697->ram_vbat_comp == AW8697_HAPTIC_RAM_VBAT_COMP_ENABLE) {
		aw8697_haptic_get_vbat(aw8697);
		pr_debug("%s: ref %d vbat %d ", __func__, AW8697_VBAT_REFER,
			aw8697->vbat);
		comp_gain = gain * AW8697_VBAT_REFER / aw8697->vbat;
		if (comp_gain > (128 * AW8697_VBAT_REFER / AW8697_VBAT_MIN)) {
			comp_gain = 128 * AW8697_VBAT_REFER / AW8697_VBAT_MIN;
			pr_debug("%s comp gain limit=%d\n", __func__, comp_gain);
		}
		pr_info("%s: enable vbat comp, level = %x comp level = %x",
			__func__, gain, comp_gain);
		aw8697_i2c_write(aw8697, AW8697_REG_DATDBG, comp_gain);
	} else {
		pr_debug("%s: disable compsensation, vbat=%d, vbat_min=%d, vbat_ref=%d",
			__func__, aw8697->vbat, AW8697_VBAT_MIN, AW8697_VBAT_REFER);
		aw8697_i2c_write(aw8697, AW8697_REG_DATDBG, gain);
	}
	return 0;
}

static int aw8697_haptic_set_pwm(struct aw8697 *aw8697, unsigned char mode)
{
	switch (mode) {
	case AW8697_PWM_48K:
		aw8697_i2c_write_bits(aw8697, AW8697_REG_PWMDBG,
				      AW8697_BIT_PWMDBG_PWM_MODE_MASK,
				      AW8697_BIT_PWMDBG_PWM_48K);
		break;
	case AW8697_PWM_24K:
		aw8697_i2c_write_bits(aw8697, AW8697_REG_PWMDBG,
				      AW8697_BIT_PWMDBG_PWM_MODE_MASK,
				      AW8697_BIT_PWMDBG_PWM_24K);
		break;
	case AW8697_PWM_12K:
		aw8697_i2c_write_bits(aw8697, AW8697_REG_PWMDBG,
				      AW8697_BIT_PWMDBG_PWM_MODE_MASK,
				      AW8697_BIT_PWMDBG_PWM_12K);
		break;
	default:
		break;
	}
	return 0;
}

static int aw8697_haptic_play_repeat_seq(struct aw8697 *aw8697,
					 unsigned char flag)
{
	pr_debug("%s enter\n", __func__);

	if (flag) {
		aw8697_haptic_play_mode(aw8697, AW8697_HAPTIC_RAM_LOOP_MODE);
		aw8697_haptic_start(aw8697);
	}

	return 0;
}

/*****************************************************
 *
 * motor protect
 *
 *****************************************************/
static int aw8697_haptic_swicth_motorprotect_config(struct aw8697 *aw8697,
						    unsigned char addr,
						    unsigned char val)
{
	pr_info("%s enter\n", __func__);

	if (addr == 1) {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_DETCTRL,
				      AW8697_BIT_DETCTRL_PROTECT_MASK,
				      AW8697_BIT_DETCTRL_PROTECT_SHUTDOWN);
		aw8697_i2c_write_bits(aw8697, AW8697_REG_PWMPRC,
				      AW8697_BIT_PWMPRC_PRC_MASK,
				      AW8697_BIT_PWMPRC_PRC_ENABLE);
		aw8697_i2c_write_bits(aw8697, AW8697_REG_PRLVL,
				      AW8697_BIT_PRLVL_PR_MASK,
				      AW8697_BIT_PRLVL_PR_ENABLE);
	} else if (addr == 0) {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_DETCTRL,
				      AW8697_BIT_DETCTRL_PROTECT_MASK,
				      AW8697_BIT_DETCTRL_PROTECT_NO_ACTION);
		aw8697_i2c_write_bits(aw8697, AW8697_REG_PWMPRC,
				      AW8697_BIT_PWMPRC_PRC_MASK,
				      AW8697_BIT_PWMPRC_PRC_DISABLE);
		aw8697_i2c_write_bits(aw8697, AW8697_REG_PRLVL,
				      AW8697_BIT_PRLVL_PR_MASK,
				      AW8697_BIT_PRLVL_PR_DISABLE);
	} else if (addr == 0x2d) {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_PWMPRC,
				      AW8697_BIT_PWMPRC_PRCTIME_MASK, val);
	} else if (addr == 0x3e) {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_PRLVL,
				      AW8697_BIT_PRLVL_PRLVL_MASK, val);
	} else if (addr == 0x3f) {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_PRTIME,
				      AW8697_BIT_PRTIME_PRTIME_MASK, val);
	} else {
		/*nothing to do; */
	}
	return 0;
}

/*****************************************************
 *
 * offset calibration
 *
 *****************************************************/
static int aw8697_haptic_offset_calibration(struct aw8697 *aw8697)
{
	unsigned int cont = 2000;
	unsigned char reg_val = 0;

	pr_info("%s enter\n", __func__);

	aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSCTRL,
			      AW8697_BIT_SYSCTRL_RAMINIT_MASK,
			      AW8697_BIT_SYSCTRL_RAMINIT_EN);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_DETCTRL,
			      AW8697_BIT_DETCTRL_DIAG_GO_MASK,
			      AW8697_BIT_DETCTRL_DIAG_GO_ENABLE);
	while (1) {
		aw8697_i2c_read(aw8697, AW8697_REG_DETCTRL, &reg_val);
		if ((reg_val & 0x01) == 0 || cont == 0)
			break;
		cont--;
	}
	aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSCTRL,
			      AW8697_BIT_SYSCTRL_RAMINIT_MASK,
			      AW8697_BIT_SYSCTRL_RAMINIT_OFF);

	return 0;
}

/*****************************************************
 *
 * trig config
 *
 *****************************************************/

static int aw8697_haptic_trig_param_init(struct aw8697 *aw8697)
{
	pr_info("%s enter\n", __func__);

	aw8697->trig[0].enable = aw8697->info.trig_config[0][0];
	aw8697->trig[0].default_level = aw8697->info.trig_config[0][1];
	aw8697->trig[0].dual_edge = aw8697->info.trig_config[0][2];
	aw8697->trig[0].frist_seq = aw8697->info.trig_config[0][3];
	aw8697->trig[0].second_seq = aw8697->info.trig_config[0][4];

	aw8697->trig[1].enable = aw8697->info.trig_config[1][0];;
	aw8697->trig[1].default_level = aw8697->info.trig_config[1][1];
	aw8697->trig[1].dual_edge = aw8697->info.trig_config[1][2];
	aw8697->trig[1].frist_seq = aw8697->info.trig_config[1][3];
	aw8697->trig[1].second_seq = aw8697->info.trig_config[1][4];

	aw8697->trig[2].enable = aw8697->info.trig_config[2][0];;
	aw8697->trig[2].default_level = aw8697->info.trig_config[2][1];
	aw8697->trig[2].dual_edge = aw8697->info.trig_config[2][2];
	aw8697->trig[2].frist_seq = aw8697->info.trig_config[2][3];
	aw8697->trig[2].second_seq = aw8697->info.trig_config[2][4];

	return 0;
}

static int aw8697_haptic_trig_param_config(struct aw8697 *aw8697)
{
	pr_info("%s enter\n", __func__);

	if (aw8697->trig[0].default_level) {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_TRG_CFG1,
				      AW8697_BIT_TRGCFG1_TRG1_POLAR_MASK,
				      AW8697_BIT_TRGCFG1_TRG1_POLAR_NEG);
	} else {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_TRG_CFG1,
				      AW8697_BIT_TRGCFG1_TRG1_POLAR_MASK,
				      AW8697_BIT_TRGCFG1_TRG1_POLAR_POS);
	}
	if (aw8697->trig[1].default_level) {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_TRG_CFG1,
				      AW8697_BIT_TRGCFG1_TRG2_POLAR_MASK,
				      AW8697_BIT_TRGCFG1_TRG2_POLAR_NEG);
	} else {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_TRG_CFG1,
				      AW8697_BIT_TRGCFG1_TRG2_POLAR_MASK,
				      AW8697_BIT_TRGCFG1_TRG2_POLAR_POS);
	}
	if (aw8697->trig[2].default_level) {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_TRG_CFG1,
				      AW8697_BIT_TRGCFG1_TRG3_POLAR_MASK,
				      AW8697_BIT_TRGCFG1_TRG3_POLAR_NEG);
	} else {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_TRG_CFG1,
				      AW8697_BIT_TRGCFG1_TRG3_POLAR_MASK,
				      AW8697_BIT_TRGCFG1_TRG3_POLAR_POS);
	}

	if (aw8697->trig[0].dual_edge) {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_TRG_CFG1,
				      AW8697_BIT_TRGCFG1_TRG1_EDGE_MASK,
				      AW8697_BIT_TRGCFG1_TRG1_EDGE_POS_NEG);
	} else {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_TRG_CFG1,
				      AW8697_BIT_TRGCFG1_TRG1_EDGE_MASK,
				      AW8697_BIT_TRGCFG1_TRG1_EDGE_POS);
	}
	if (aw8697->trig[1].dual_edge) {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_TRG_CFG1,
				      AW8697_BIT_TRGCFG1_TRG2_EDGE_MASK,
				      AW8697_BIT_TRGCFG1_TRG2_EDGE_POS_NEG);
	} else {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_TRG_CFG1,
				      AW8697_BIT_TRGCFG1_TRG2_EDGE_MASK,
				      AW8697_BIT_TRGCFG1_TRG2_EDGE_POS);
	}
	if (aw8697->trig[2].dual_edge) {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_TRG_CFG1,
				      AW8697_BIT_TRGCFG1_TRG3_EDGE_MASK,
				      AW8697_BIT_TRGCFG1_TRG3_EDGE_POS_NEG);
	} else {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_TRG_CFG1,
				      AW8697_BIT_TRGCFG1_TRG3_EDGE_MASK,
				      AW8697_BIT_TRGCFG1_TRG3_EDGE_POS);
	}

	if (aw8697->trig[0].frist_seq) {
		aw8697_i2c_write(aw8697, AW8697_REG_TRG1_WAV_P,
				 aw8697->trig[0].frist_seq);
	}
	if (aw8697->trig[0].second_seq && aw8697->trig[0].dual_edge) {
		aw8697_i2c_write(aw8697, AW8697_REG_TRG1_WAV_N,
				 aw8697->trig[0].second_seq);
	}
	if (aw8697->trig[1].frist_seq) {
		aw8697_i2c_write(aw8697, AW8697_REG_TRG2_WAV_P,
				 aw8697->trig[1].frist_seq);
	}
	if (aw8697->trig[1].second_seq && aw8697->trig[1].dual_edge) {
		aw8697_i2c_write(aw8697, AW8697_REG_TRG2_WAV_N,
				 aw8697->trig[1].second_seq);
	}
	if (aw8697->trig[2].frist_seq) {
		aw8697_i2c_write(aw8697, AW8697_REG_TRG3_WAV_P,
				 aw8697->trig[1].frist_seq);
	}
	if (aw8697->trig[2].second_seq && aw8697->trig[2].dual_edge) {
		aw8697_i2c_write(aw8697, AW8697_REG_TRG3_WAV_N,
				 aw8697->trig[1].second_seq);
	}

	return 0;
}

static int aw8697_haptic_trig_enable_config(struct aw8697 *aw8697)
{
	pr_info("%s enter\n", __func__);

	aw8697_i2c_write_bits(aw8697, AW8697_REG_TRG_CFG2,
			      AW8697_BIT_TRGCFG2_TRG1_ENABLE_MASK,
			      aw8697->trig[0].enable);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_TRG_CFG2,
			      AW8697_BIT_TRGCFG2_TRG2_ENABLE_MASK,
			      aw8697->trig[1].enable);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_TRG_CFG2,
			      AW8697_BIT_TRGCFG2_TRG3_ENABLE_MASK,
			      aw8697->trig[2].enable);

	return 0;
}

static int aw8697_haptic_auto_boost_config(struct aw8697 *aw8697,
					   unsigned char flag)
{
	aw8697->auto_boost = flag;
	if (flag) {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_BST_AUTO,
					AW8697_BIT_BST_AUTO_BST_AUTOSW_MASK,
					AW8697_BIT_BST_AUTO_BST_AUTOMATIC_BOOST);
	} else {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_BST_AUTO,
					AW8697_BIT_BST_AUTO_BST_AUTOSW_MASK,
					AW8697_BIT_BST_AUTO_BST_MANUAL_BOOST);
	}
	return 0;
}

/*****************************************************
 *
 * vbat mode
 *
 *****************************************************/
static int aw8697_haptic_cont_vbat_mode(struct aw8697 *aw8697,
					unsigned char flag)
{
	if (flag == AW8697_HAPTIC_CONT_VBAT_HW_COMP_MODE) {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_ADCTEST,
				      AW8697_BIT_ADCTEST_VBAT_MODE_MASK,
				      AW8697_BIT_ADCTEST_VBAT_HW_COMP);
	} else {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_ADCTEST,
				      AW8697_BIT_ADCTEST_VBAT_MODE_MASK,
				      AW8697_BIT_ADCTEST_VBAT_SW_COMP);
	}
	return 0;
}

static int aw8697_haptic_get_vbat(struct aw8697 *aw8697)
{
	unsigned char reg_val = 0;
	unsigned int cont = 2000;

	aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSCTRL,
			      AW8697_BIT_SYSCTRL_RAMINIT_MASK,
			      AW8697_BIT_SYSCTRL_RAMINIT_EN);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_DETCTRL,
			      AW8697_BIT_DETCTRL_VBAT_GO_MASK,
			      AW8697_BIT_DETCTRL_VABT_GO_ENABLE);

	while (1) {
		aw8697_i2c_read(aw8697, AW8697_REG_DETCTRL, &reg_val);
		if ((reg_val & 0x02) == 0 || cont == 0)
			break;
		cont--;
	}

	aw8697_i2c_read(aw8697, AW8697_REG_VBATDET, &reg_val);
	aw8697->vbat = 6100 * reg_val / 256;
	if (aw8697->vbat > AW8697_VBAT_MAX) {
		//aw8697->vbat = AW8697_VBAT_MAX;
		pr_debug("%s vbat max limit = %dmV\n", __func__, aw8697->vbat);
	}
	if (aw8697->vbat < AW8697_VBAT_MIN) {
		//aw8697->vbat = AW8697_VBAT_MIN;
		pr_debug("%s vbat min limit = %dmV\n", __func__, aw8697->vbat);
	}

	aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSCTRL,
			      AW8697_BIT_SYSCTRL_RAMINIT_MASK,
			      AW8697_BIT_SYSCTRL_RAMINIT_OFF);

	return 0;
}

static int aw8697_haptic_ram_vbat_comp(struct aw8697 *aw8697, bool flag)
{
	if (flag) {
		aw8697->ram_vbat_comp = AW8697_HAPTIC_RAM_VBAT_COMP_ENABLE;
	} else {
		aw8697->ram_vbat_comp = AW8697_HAPTIC_RAM_VBAT_COMP_DISABLE;
	}

	return 0;
}

/*****************************************************
 *
 * f0
 *
 *****************************************************/
static int aw8697_haptic_set_f0_preset(struct aw8697 *aw8697)
{
	unsigned int f0_reg = 0;

	pr_info("%s enter\n", __func__);

	f0_reg = 1000000000 / (aw8697->info.f0_pre * aw8697->info.f0_coeff);
	aw8697_i2c_write(aw8697, AW8697_REG_F_PRE_H,
			 (unsigned char)((f0_reg >> 8) & 0xff));
	aw8697_i2c_write(aw8697, AW8697_REG_F_PRE_L,
			 (unsigned char)((f0_reg >> 0) & 0xff));

	return 0;
}

#ifndef USE_CONT_F0_CALI
static int aw8697_haptic_read_f0(struct aw8697 *aw8697)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	pr_info("%s enter\n", __func__);

	ret = aw8697_i2c_read(aw8697, AW8697_REG_F_LRA_F0_H, &reg_val);
	f0_reg = (reg_val << 8);
	ret = aw8697_i2c_read(aw8697, AW8697_REG_F_LRA_F0_L, &reg_val);
	f0_reg |= (reg_val << 0);
	if (!f0_reg) {
		pr_info("%s: not get f0_reg value is 0!\n", __func__);
		aw8697->f0 = 0;
		aw8697->f0_cali_status = false;
		return 0;
	}
	aw8697->f0_cali_status = true;
	f0_tmp = 1000000000 / (f0_reg * aw8697->info.f0_coeff);
	aw8697->f0 = (unsigned int)f0_tmp;
	pr_info("%s f0=%d\n", __func__, aw8697->f0);

	return 0;
}
#endif

#ifndef USE_CONT_F0_CALI
static int aw8697_haptic_read_cont_f0(struct aw8697 *aw8697)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	pr_debug("%s enter\n", __func__);

	ret = aw8697_i2c_read(aw8697, AW8697_REG_F_LRA_CONT_H, &reg_val);
	f0_reg = (reg_val << 8);
	ret = aw8697_i2c_read(aw8697, AW8697_REG_F_LRA_CONT_L, &reg_val);
	f0_reg |= (reg_val << 0);
	if (!f0_reg) {
		pr_info("%s: not get f0_reg value is 0!\n", __func__);
		return 0;
	}
	f0_tmp = 1000000000 / (f0_reg * aw8697->info.f0_coeff);
	aw8697->cont_f0 = (unsigned int)f0_tmp;
	pr_info("%s f0=%d\n", __func__, aw8697->cont_f0);

	return 0;
}
#else
static int aw8697_haptic_read_cont_f0(struct aw8697 *aw8697)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	pr_debug("%s enter\n", __func__);

	ret = aw8697_i2c_read(aw8697, AW8697_REG_F_LRA_CONT_H, &reg_val);
	f0_reg = (reg_val << 8);
	ret = aw8697_i2c_read(aw8697, AW8697_REG_F_LRA_CONT_L, &reg_val);
	f0_reg |= (reg_val << 0);
	if (!f0_reg) {
		pr_info("%s: not get f0_reg value is 0!\n", __func__);
		return 0;
	}
	f0_tmp = 1000000000 / (f0_reg * aw8697->info.f0_coeff);
	aw8697->cont_f0 = (unsigned int)f0_tmp;
	aw8697->cont_f0 -= 12;
	aw8697->f0 = aw8697->cont_f0;
	pr_info("%s f0=%d\n", __func__, aw8697->cont_f0);

	return 0;
}
#endif

#ifndef USE_CONT_F0_CALI
static int aw8697_haptic_read_beme(struct aw8697 *aw8697)
{
	int ret = 0;
	unsigned char reg_val = 0;
	pr_info("%s  %d enter\n", __func__, __LINE__);
	ret = aw8697_i2c_read(aw8697, AW8697_REG_WAIT_VOL_MP, &reg_val);
	aw8697->max_pos_beme = (reg_val << 0);
	ret = aw8697_i2c_read(aw8697, AW8697_REG_WAIT_VOL_MN, &reg_val);
	aw8697->max_neg_beme = (reg_val << 0);

	pr_info("%s max_pos_beme=%d\n", __func__, aw8697->max_pos_beme);
	pr_info("%s max_neg_beme=%d\n", __func__, aw8697->max_neg_beme);

	return 0;
}
#else
static int aw8697_haptic_read_cont_bemf(struct aw8697 *aw8697)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int bemf = 0;

	ret = aw8697_i2c_read(aw8697, AW8697_REG_BEMF_VOL_H, &reg_val);
	bemf |= (reg_val<<8);
	ret = aw8697_i2c_read(aw8697, AW8697_REG_BEMF_VOL_L, &reg_val);
	bemf |= (reg_val<<0);

	pr_info("%s bemf=%d\n", __func__, bemf);

	return 0;
}
#endif

/*****************************************************
 *
 * rtp
 *
 *****************************************************/
static void aw8697_haptic_set_rtp_aei(struct aw8697 *aw8697, bool flag)
{
	if (flag) {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSINTM,
				      AW8697_BIT_SYSINTM_FF_AE_MASK,
				      AW8697_BIT_SYSINTM_FF_AE_EN);
	} else {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSINTM,
				      AW8697_BIT_SYSINTM_FF_AE_MASK,
				      AW8697_BIT_SYSINTM_FF_AE_OFF);
	}
}

static unsigned char aw8697_haptic_rtp_get_fifo_afi(struct aw8697 *aw8697)
{
	unsigned char ret = 0;
	unsigned char reg_val = 0;

	if (aw8697->osc_cali_flag == 1) {
		aw8697_i2c_read(aw8697, AW8697_REG_SYSST, &reg_val);
		reg_val &= AW8697_BIT_SYSST_FF_AFS;
		ret = reg_val >> 3;
	} else {
		aw8697_i2c_read(aw8697, AW8697_REG_SYSINT, &reg_val);
		reg_val &= AW8697_BIT_SYSINT_FF_AFI;
		ret = reg_val >> 3;
	}

	return ret;
}

/*****************************************************
 *
 * rtp
 *
 *****************************************************/
static int aw8697_haptic_rtp_init(struct aw8697 *aw8697)
{
	unsigned int buf_len = 0;
	unsigned int period_size = aw8697->ram.base_addr >> 2 ;

	cpu_latency_qos_add_request(&pm_qos_req, CPU_LATENCY_QOC_VALUE);
	aw8697->rtp_cnt = 0;
	mutex_lock(&aw8697->rtp_lock);
	while ((!aw8697_haptic_rtp_get_fifo_afi(aw8697)) &&
	       (aw8697->play_mode == AW8697_HAPTIC_RTP_MODE) &&
	       !atomic_read(&aw8697->exit_in_rtp_loop)) {
		if (aw8697->is_custom_wave == 0) {
			if ((aw8697_rtp->len - aw8697->rtp_cnt) <
			(aw8697->ram.base_addr >> 2)) {
				buf_len = aw8697_rtp->len - aw8697->rtp_cnt;
			} else {
				buf_len = (aw8697->ram.base_addr >> 2);
			}
			aw8697_i2c_writes(aw8697, AW8697_REG_RTP_DATA,
					  &aw8697_rtp->data[aw8697->rtp_cnt], buf_len);
			aw8697->rtp_cnt += buf_len;
			if (aw8697->rtp_cnt == aw8697_rtp->len) {
				aw8697->rtp_cnt = 0;
				aw8697_haptic_set_rtp_aei(aw8697, false);
				mutex_unlock(&aw8697->rtp_lock);
				cpu_latency_qos_remove_request(&pm_qos_req);
				return 0;
			}
		} else {
			buf_len = read_rb(aw8697_rtp->data,  period_size);
			aw8697_i2c_writes(aw8697, AW8697_REG_RTP_DATA, aw8697_rtp->data, buf_len);
			if (buf_len < period_size) {
				pr_info("%s: custom rtp update complete\n", __func__);
				aw8697->rtp_cnt = 0;
				aw8697_haptic_set_rtp_aei(aw8697, false);
				mutex_unlock(&aw8697->rtp_lock);
				cpu_latency_qos_remove_request(&pm_qos_req);
				return 0;
			}
		}
	}
	if (aw8697->play_mode == AW8697_HAPTIC_RTP_MODE && !atomic_read(&aw8697->exit_in_rtp_loop)) {
		aw8697_haptic_set_rtp_aei(aw8697, true);
	}
	pr_info("%s: exit\n", __func__);
	mutex_unlock(&aw8697->rtp_lock);
	cpu_latency_qos_remove_request(&pm_qos_req);
	return 0;
}

static int16_t aw8697_haptic_effect_strength(struct aw8697 *aw8697)
{
	pr_debug("%s enter\n", __func__);
	pr_debug("%s: aw8697->play.vmax_mv =0x%x\n", __func__, aw8697->play.vmax_mv);
#if 0
	switch (aw8697->play.vmax_mv) {
	case AW8697_LIGHT_MAGNITUDE:
		aw8697->level = 0x80;
		break;
	case AW8697_MEDIUM_MAGNITUDE:
		aw8697->level = 0x50;
		break;
	case AW8697_STRONG_MAGNITUDE:
		aw8697->level = 0x30;
		break;
	default:
		break;
	}
#else
	if (aw8697->play.vmax_mv >= 0x7FFF)
		aw8697->level = 0x80; /*128*/
	else if (aw8697->play.vmax_mv <= 0x3FFF)
		aw8697->level = 0x1E; /*30*/
    else
		aw8697->level = (aw8697->play.vmax_mv - 16383) / 128;
	if( aw8697->level < 0x1E)
		aw8697->level = 0x1E; /*30*/
#endif

	pr_info("%s: aw8697->level =0x%x\n", __func__, aw8697->level);
	return 0;
}

static int aw8697_haptic_play_effect_seq(struct aw8697 *aw8697,
					 unsigned char flag)
{
	if (aw8697->effect_id > aw8697->info.effect_id_boundary)
		return 0;
	//printk("%s:aw8697->effect_id =%d\n", __func__, aw8697->effect_id);
	//printk("%s:aw8697->activate_mode =%d\n", __func__, aw8697->activate_mode);

	if (flag) {
		if (aw8697->activate_mode == AW8697_HAPTIC_ACTIVATE_RAM_MODE) {
			aw8697_haptic_set_wav_seq(aw8697, 0x00,
						(char)aw8697->effect_id + 1);
			aw8697_haptic_set_wav_seq(aw8697, 0x01, 0x00);
			aw8697_haptic_set_wav_loop(aw8697, 0x00, 0x00);
			aw8697_haptic_play_mode(aw8697, AW8697_HAPTIC_RAM_MODE);
			if (aw8697->info.bst_vol_ram <= AW8697_MAX_BST_VO)
				aw8697_haptic_set_bst_vol(aw8697, aw8697->info.bst_vol_ram);
			else
				aw8697_haptic_set_bst_vol(aw8697, aw8697->vmax);
			aw8697_haptic_effect_strength(aw8697);
			aw8697_haptic_set_gain(aw8697, aw8697->level);
			aw8697_haptic_start(aw8697);
		}
		if (aw8697->activate_mode == AW8697_HAPTIC_ACTIVATE_RAM_LOOP_MODE) {
			aw8697_haptic_set_repeat_wav_seq(aw8697, (aw8697->info.effect_id_boundary + 1));
			aw8697_haptic_effect_strength(aw8697);
			aw8697_haptic_set_gain(aw8697, aw8697->level);
			aw8697_haptic_play_repeat_seq(aw8697, true);
		}
	}

	return 0;
}

static void aw8697_haptic_upload_lra(struct aw8697 *aw8697, unsigned char flag)
{
	if (flag) {
		//printk("%s f0_cali_lra=%d\n", __func__, aw8697->f0_calib_data);
		aw8697_i2c_write(aw8697, AW8697_REG_TRIM_LRA,
				 (char)aw8697->f0_calib_data);
	} else {
		//printk("%s rtp_cali_lra=%d\n", __func__, aw8697->lra_calib_data);
		aw8697_i2c_write(aw8697, AW8697_REG_TRIM_LRA,
				 (char)aw8697->lra_calib_data);
	}

}

static int aw8697_clock_OSC_trim_calibration(unsigned long int theory_time, unsigned long int real_time)
{
	unsigned int real_code = 0;
	unsigned int LRA_TRIM_CODE = 0;
	unsigned int DFT_LRA_TRIM_CODE = 0;
	unsigned int Not_need_cali_threshold = 10;/*0.1 percent not need calibrate*/

	if (theory_time == real_time) {
		pr_info("aw_osctheory_time == real_time:%ld  theory_time = %ld not need to cali\n", real_time, theory_time);
		return 0;
	} else if (theory_time < real_time) {
		if ((real_time - theory_time) > (theory_time / 50)) {
			pr_info("aw_osc(real_time - theory_time) > (theory_time/50) not to cali\n");
			return DFT_LRA_TRIM_CODE;
		}

		if ((real_time - theory_time) < (Not_need_cali_threshold*theory_time/10000)) {
			pr_info("aw_oscmicrosecond:%ld  theory_time = %ld not need to cali\n", real_time, theory_time);
			return DFT_LRA_TRIM_CODE;
		}

		real_code = ((real_time - theory_time) * 4000) / theory_time;
		real_code = ((real_code%10 < 5) ? 0 : 1) + real_code/10;
		real_code = 32 + real_code;
	} else if (theory_time > real_time) {
		if ((theory_time - real_time) > (theory_time / 50)) {
			pr_info("aw_osc((theory_time - real_time) > (theory_time / 50)) not to cali\n");
			return DFT_LRA_TRIM_CODE;
		}
		if ((theory_time - real_time) < (Not_need_cali_threshold * theory_time/10000)) {
			pr_info("aw_oscmicrosecond:%ld  theory_time = %ld not need to cali\n", real_time, theory_time);
			return DFT_LRA_TRIM_CODE;
		}
		real_code = ((theory_time - real_time) * 4000) / theory_time;
		real_code = ((real_code%10 < 5) ? 0 : 1) + real_code/10;
		real_code = 32 - real_code;
	}
	if (real_code > 31)
		LRA_TRIM_CODE = real_code - 32;
	else
		LRA_TRIM_CODE = real_code + 32;
	pr_info("aw_oscmicrosecond:%ld  theory_time = %ld real_code =0X%02X LRA_TRIM_CODE 0X%02X\n", real_time, theory_time, real_code, LRA_TRIM_CODE);

	return LRA_TRIM_CODE;
}

static int aw8697_rtp_trim_lra_calibration(struct aw8697 *aw8697)
{
	unsigned char reg_val = 0;
	unsigned int fre_val = 0;
	unsigned int theory_time = 0;
	unsigned int lra_rtim_code = 0;

	aw8697_i2c_read(aw8697, AW8697_REG_PWMDBG, &reg_val);
	fre_val = (reg_val & 0x006f) >> 5;

	if (fre_val == 3)
		theory_time = (aw8697->rtp_len / 12000) * 1000000; /*12K */
	if (fre_val == 2)
		theory_time = (aw8697->rtp_len / 24000) * 1000000; /*24K */
	if (fre_val == 1 || fre_val == 0)
		theory_time = (aw8697->rtp_len / 48000) * 1000000; /*48K */

	printk("microsecond:%ld  theory_time = %d\n", aw8697->microsecond, theory_time);

	lra_rtim_code = aw8697_clock_OSC_trim_calibration(theory_time, aw8697->microsecond);
	if (lra_rtim_code > 0) {
		aw8697->lra_calib_data = lra_rtim_code;
		aw8697_i2c_write(aw8697, AW8697_REG_TRIM_LRA, (char)lra_rtim_code);
	}
	return 0;
}
static unsigned char aw8697_haptic_osc_read_int(struct aw8697 *aw8697)
{
	unsigned char reg_val = 0;

	aw8697_i2c_read(aw8697, AW8697_REG_DBGSTAT, &reg_val);
	return reg_val;
}

static int aw8697_rtp_osc_calibration(struct aw8697 *aw8697)
{
	const struct firmware *rtp_file;
	int ret = -1;
	unsigned int buf_len = 0;
	unsigned char osc_int_state = 0;
	aw8697->rtp_cnt = 0;
	aw8697->timeval_flags = 1;
	aw8697->osc_cali_flag = 1;

	pr_info("%s enter\n", __func__);
	/* fw loaded */
	ret = request_firmware(&rtp_file,
		aw8697_rtp_name[/*aw8697->rtp_file_num*/ 0],
		aw8697->dev);
	if (ret < 0) {
		pr_err("%s: failed to read %s\n", __func__,
			aw8697_rtp_name[/*aw8697->rtp_file_num*/ 0]);
		return ret;
	}
	/*awinic add stop,for irq interrupt during calibrate*/
	aw8697_haptic_stop(aw8697);
	aw8697->rtp_init = 0;
	mutex_lock(&aw8697->rtp_lock);
	vfree(aw8697_rtp);
	aw8697_rtp = vmalloc(rtp_file->size+sizeof(int));
	if (!aw8697_rtp) {
		release_firmware(rtp_file);
		mutex_unlock(&aw8697->rtp_lock);
		pr_err("%s: error allocating memory\n", __func__);
		return -ENOMEM;
	}
	aw8697_rtp->len = rtp_file->size;
	aw8697->rtp_len = rtp_file->size;
	pr_info("%s: rtp file [%s] size = %d\n", __func__,
		aw8697_rtp_name[/*aw8697->rtp_file_num*/ 0], aw8697_rtp->len);
	memcpy(aw8697_rtp->data, rtp_file->data, rtp_file->size);
	release_firmware(rtp_file);
	mutex_unlock(&aw8697->rtp_lock);

	/* gain */
	aw8697_haptic_ram_vbat_comp(aw8697, false);

	/* rtp mode config */
	aw8697_haptic_play_mode(aw8697, AW8697_HAPTIC_RTP_MODE);

	aw8697_i2c_write_bits(aw8697, AW8697_REG_DBGCTRL,
		AW8697_BIT_DBGCTRL_INT_MODE_MASK, AW8697_BIT_DBGCTRL_INT_MODE_EDGE);
	disable_irq(gpio_to_irq(aw8697->irq_gpio));
	/* haptic start */
	aw8697_haptic_start(aw8697);
	cpu_latency_qos_add_request(&pm_qos_req, CPU_LATENCY_QOC_VALUE);
	while (1) {
		if (!aw8697_haptic_rtp_get_fifo_afi(aw8697)) {
			mutex_lock(&aw8697->rtp_lock);
			if ((aw8697_rtp->len - aw8697->rtp_cnt) < (aw8697->ram.base_addr>>2))
				buf_len = aw8697_rtp->len-aw8697->rtp_cnt;
			else
				buf_len = (aw8697->ram.base_addr >> 2);
			if (aw8697->rtp_cnt != aw8697_rtp->len) {
				if (aw8697->timeval_flags == 1) {
					ktime_get_real_ts64(&aw8697->start);
					aw8697->timeval_flags = 0;
				}
				aw8697_i2c_writes(aw8697, AW8697_REG_RTP_DATA, &aw8697_rtp->data[aw8697->rtp_cnt], buf_len);
				aw8697->rtp_cnt += buf_len;
			}
			mutex_unlock(&aw8697->rtp_lock);
		}
		osc_int_state = aw8697_haptic_osc_read_int(aw8697);
		if (osc_int_state&AW8697_BIT_SYSINT_DONEI) {
			ktime_get_real_ts64(&aw8697->end);
			pr_info("%s vincent playback done aw8697->rtp_cnt= %d \n", __func__, aw8697->rtp_cnt);
			break;
		}

		ktime_get_real_ts64(&aw8697->end);
		aw8697->microsecond = (aw8697->end.tv_sec - aw8697->start.tv_sec)*1000000 +
					(aw8697->end.tv_nsec - aw8697->start.tv_nsec) / 1000000;
		if (aw8697->microsecond > OSC_CALIBRATION_T_LENGTH) {
			pr_info("%s vincent time out aw8697->rtp_cnt %d osc_int_state %02x\n", __func__, aw8697->rtp_cnt, osc_int_state);
			break;
		}
	}
	cpu_latency_qos_remove_request(&pm_qos_req);
	enable_irq(gpio_to_irq(aw8697->irq_gpio));

	aw8697->osc_cali_flag = 0;
	aw8697->microsecond = (aw8697->end.tv_sec - aw8697->start.tv_sec)*1000000 +
				(aw8697->end.tv_nsec - aw8697->start.tv_nsec) / 1000000;
	/*calibration osc*/
	pr_info("%s 2018_microsecond:%ld \n", __func__, aw8697->microsecond);
	pr_info("%s exit\n", __func__);
	return 0;
}


static void aw8697_rtp_work_routine(struct work_struct *work)
{
	const struct firmware *rtp_file;
	int ret = -1;
	struct aw8697 *aw8697 = container_of(work, struct aw8697, rtp_work);

	if ((aw8697->effect_id < aw8697->info.effect_id_boundary) &&
	    (aw8697->effect_id > aw8697->info.effect_max))
		return;

	pr_info("%s: effect_id = %d state=%d activate_mode = %d\n", __func__,
		aw8697->effect_id, aw8697->state, aw8697->activate_mode);
	mutex_lock(&aw8697->lock);
	aw8697_haptic_upload_lra(aw8697, false);
	aw8697_haptic_set_rtp_aei(aw8697, false);
	aw8697_interrupt_clear(aw8697);
	//wait for irq to exit


	atomic_set(&aw8697->exit_in_rtp_loop, 1);
	while (atomic_read(&aw8697->is_in_rtp_loop)) {
		pr_info("%s  goint to waiting irq exit\n", __func__);
		mutex_unlock(&aw8697->lock);
		ret = wait_event_interruptible(aw8697->wait_q, atomic_read(&aw8697->is_in_rtp_loop) == 0);
		pr_info("%s  wakeup \n", __func__);
		mutex_lock(&aw8697->lock);
		if (ret == -ERESTARTSYS) {
			atomic_set(&aw8697->exit_in_rtp_loop, 0);
			wake_up_interruptible(&aw8697->stop_wait_q);
			mutex_unlock(&aw8697->lock);
			pr_err("%s wake up by signal return erro\n", __func__);
			return;
		}
	}

	atomic_set(&aw8697->exit_in_rtp_loop, 0);
	wake_up_interruptible(&aw8697->stop_wait_q);


		//how to force exit this call
		if (aw8697->is_custom_wave == 1 && aw8697->state) {
			pr_err("%s buffer size %d, availbe size %d \n", __func__, aw8697->ram.base_addr >> 2, get_rb_avalible_size());
			while (get_rb_avalible_size() < aw8697->ram.base_addr && !rb_shoule_exit()) {
			mutex_unlock(&aw8697->lock);
			ret = wait_event_interruptible(aw8697->stop_wait_q, (get_rb_avalible_size() >= aw8697->ram.base_addr) || rb_shoule_exit());
			pr_info("%s  wakeup  \n", __func__);
			pr_err("%s after wakeup sbuffer size %d, availbe size %d \n", __func__, aw8697->ram.base_addr >> 2, get_rb_avalible_size());
			if (ret == -ERESTARTSYS) {
			pr_err("%s wake up by signal return erro\n", __func__);
			return ;
		}
			mutex_lock(&aw8697->lock);

		}
	}

	aw8697_haptic_stop(aw8697);

	if (aw8697->state) {
		pm_stay_awake(aw8697->dev);
		if (aw8697->info.bst_vol_ram <= AW8697_MAX_BST_VO)
			aw8697_haptic_set_bst_vol(aw8697, aw8697->info.bst_vol_rtp);
		else
			aw8697_haptic_set_bst_vol(aw8697, aw8697->vmax);
		aw8697_haptic_effect_strength(aw8697);
		aw8697->rtp_init = 0;
		if (aw8697->is_custom_wave == 0) {
			aw8697->rtp_file_num =
			    aw8697->effect_id - aw8697->info.effect_id_boundary;
			printk("%s: aw8697->rtp_file_num =%d\n", __func__,
			       aw8697->rtp_file_num);
			if (aw8697->rtp_file_num < 0)
				aw8697->rtp_file_num = 0;
			if (aw8697->rtp_file_num > ((sizeof(aw8697_rtp_name) / AW8697_RTP_NAME_MAX) - 1))
				aw8697->rtp_file_num = (sizeof(aw8697_rtp_name) / AW8697_RTP_NAME_MAX) - 1;

			/* fw loaded */
			ret = request_firmware(&rtp_file,
					       aw8697_rtp_name[aw8697->rtp_file_num],
					       aw8697->dev);
			if (ret < 0) {
				pr_err("%s: failed to read %s\n", __func__,
				       aw8697_rtp_name[aw8697->rtp_file_num]);
				pm_relax(aw8697->dev);
				mutex_unlock(&aw8697->lock);
				return;
			}

			vfree(aw8697_rtp);
			aw8697_rtp = vmalloc(rtp_file->size + sizeof(int));
			if (!aw8697_rtp) {
				release_firmware(rtp_file);
				pr_err("%s: error allocating memory\n", __func__);
				pm_relax(aw8697->dev);
				mutex_unlock(&aw8697->lock);
				return;
			}
			aw8697_rtp->len = rtp_file->size;
			pr_info("%s: rtp file [%s] size = %d\n", __func__,
				aw8697_rtp_name[aw8697->rtp_file_num], aw8697_rtp->len);
			memcpy(aw8697_rtp->data, rtp_file->data, rtp_file->size);
			release_firmware(rtp_file);
		} else  {
			vfree(aw8697_rtp);
			aw8697_rtp = NULL;
			if(aw8697->ram.base_addr != 0) {
				aw8697_rtp = vmalloc((aw8697->ram.base_addr >> 2) + sizeof(int));
			} else {
				pr_err("ram update not done yet, return !");
			}
			if (!aw8697_rtp) {
				pr_err("%s: error allocating memory\n", __func__);
				pm_relax(aw8697->dev);
				mutex_unlock(&aw8697->lock);
				return;
			}
		}
		aw8697->rtp_init = 1;

		/* gain */
		aw8697_haptic_ram_vbat_comp(aw8697, false);
		aw8697_haptic_set_gain(aw8697, aw8697->level);

		/* rtp mode config */
		aw8697_haptic_play_mode(aw8697, AW8697_HAPTIC_RTP_MODE);

		/* haptic start */
		aw8697_haptic_start(aw8697);

		aw8697_haptic_rtp_init(aw8697);
	} else {
		aw8697->rtp_cnt = 0;
		aw8697->rtp_init = 0;
		pm_relax(aw8697->dev);
	}
	mutex_unlock(&aw8697->lock);
}

static enum hrtimer_restart aw8697_haptic_audio_timer_func(struct hrtimer
							   *timer)
{
	struct aw8697 *aw8697 =
	    container_of(timer, struct aw8697, haptic_audio.timer);

	pr_debug("%s enter\n", __func__);
	//schedule_work(&aw8697->haptic_audio.work);
	queue_work(aw8697->work_queue, &aw8697->haptic_audio.work);

	hrtimer_start(&aw8697->haptic_audio.timer,
		      ktime_set(aw8697->haptic_audio.timer_val / 1000000,
				(aw8697->haptic_audio.timer_val % 1000000) *
				1000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

static void aw8697_haptic_audio_work_routine(struct work_struct *work)
{
	struct aw8697 *aw8697 =
	    container_of(work, struct aw8697, haptic_audio.work);

	pr_info("%s enter\n", __func__);

	mutex_lock(&aw8697->haptic_audio.lock);
	memcpy(&aw8697->haptic_audio.ctr,
	       &aw8697->haptic_audio.data[aw8697->haptic_audio.cnt],
	       sizeof(struct haptic_ctr));
	pr_debug("%s: cnt=%d, cmd=%d, play=%d, wavseq=%d, loop=%d, gain=%d\n",
		 __func__,
		 aw8697->haptic_audio.cnt,
		 aw8697->haptic_audio.ctr.cmd,
		 aw8697->haptic_audio.ctr.play,
		 aw8697->haptic_audio.ctr.wavseq,
		 aw8697->haptic_audio.ctr.loop, aw8697->haptic_audio.ctr.gain);
	mutex_unlock(&aw8697->haptic_audio.lock);
	if (AW8697_HAPTIC_CMD_ENABLE == aw8697->haptic_audio.ctr.cmd) {
		if (AW8697_HAPTIC_PLAY_ENABLE == aw8697->haptic_audio.ctr.play) {
			pr_info("%s: haptic_audio_play_start\n", __func__);
			mutex_lock(&aw8697->lock);
			aw8697_haptic_stop(aw8697);
			aw8697_haptic_play_mode(aw8697, AW8697_HAPTIC_RAM_MODE);

			aw8697_haptic_set_wav_seq(aw8697, 0x00,
						  aw8697->haptic_audio.
						  ctr.wavseq);
			aw8697_haptic_set_wav_seq(aw8697, 0x01, 0x00);

			aw8697_haptic_set_wav_loop(aw8697, 0x00,
						   aw8697->haptic_audio.
						   ctr.loop);

			aw8697_haptic_set_gain(aw8697,
					       aw8697->haptic_audio.ctr.gain);

			aw8697_haptic_start(aw8697);
			mutex_unlock(&aw8697->lock);
		} else if (AW8697_HAPTIC_PLAY_STOP ==
			   aw8697->haptic_audio.ctr.play) {
			mutex_lock(&aw8697->lock);
			aw8697_haptic_stop(aw8697);
			mutex_unlock(&aw8697->lock);
		} else if (AW8697_HAPTIC_PLAY_GAIN ==
			   aw8697->haptic_audio.ctr.play) {
			mutex_lock(&aw8697->lock);
			aw8697_haptic_set_gain(aw8697,
					       aw8697->haptic_audio.ctr.gain);
			mutex_unlock(&aw8697->lock);
		}
		mutex_lock(&aw8697->haptic_audio.lock);
		memset(&aw8697->haptic_audio.data[aw8697->haptic_audio.cnt],
		       0, sizeof(struct haptic_ctr));
		mutex_unlock(&aw8697->haptic_audio.lock);
	}

	mutex_lock(&aw8697->haptic_audio.lock);
	aw8697->haptic_audio.cnt++;
	if (aw8697->haptic_audio.data[aw8697->haptic_audio.cnt].cmd == 0) {
		aw8697->haptic_audio.cnt = 0;
		pr_debug("%s: haptic play buffer restart\n", __func__);
	}
	mutex_unlock(&aw8697->haptic_audio.lock);

}

/*****************************************************
 *
 * haptic cont
 *
 *****************************************************/
static int aw8697_haptic_cont(struct aw8697 *aw8697)
{
	pr_info("%s enter\n", __func__);

	/* work mode */
	aw8697_haptic_play_mode(aw8697, AW8697_HAPTIC_CONT_MODE);

	/* preset f0 */
	aw8697_haptic_set_f0_preset(aw8697);

	/* lpf */
	aw8697_i2c_write_bits(aw8697, AW8697_REG_DATCTRL,
			      AW8697_BIT_DATCTRL_FC_MASK,
			      AW8697_BIT_DATCTRL_FC_1000HZ);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_DATCTRL,
			      AW8697_BIT_DATCTRL_LPF_ENABLE_MASK,
			      AW8697_BIT_DATCTRL_LPF_ENABLE);

	/* cont config */
	aw8697_i2c_write_bits(aw8697, AW8697_REG_CONT_CTRL,
			      AW8697_BIT_CONT_CTRL_ZC_DETEC_MASK,
			      AW8697_BIT_CONT_CTRL_ZC_DETEC_ENABLE);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_CONT_CTRL,
			      AW8697_BIT_CONT_CTRL_WAIT_PERIOD_MASK,
			      AW8697_BIT_CONT_CTRL_WAIT_1PERIOD);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_CONT_CTRL,
			      AW8697_BIT_CONT_CTRL_MODE_MASK,
			      AW8697_BIT_CONT_CTRL_BY_GO_SIGNAL);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_CONT_CTRL,
			      AW8697_BIT_CONT_CTRL_EN_CLOSE_MASK,
			      AW8697_CONT_PLAYBACK_MODE);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_CONT_CTRL,
			      AW8697_BIT_CONT_CTRL_F0_DETECT_MASK,
			      AW8697_BIT_CONT_CTRL_F0_DETECT_DISABLE);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_CONT_CTRL,
			      AW8697_BIT_CONT_CTRL_O2C_MASK,
			      AW8697_BIT_CONT_CTRL_O2C_DISABLE);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_CONT_CTRL,
			      AW8697_BIT_CONT_CTRL_AUTO_BRK_MASK,
			      AW8697_BIT_CONT_CTRL_AUTO_BRK_ENABLE);

	/* TD time */
	aw8697_i2c_write(aw8697, AW8697_REG_TD_H,
			 (unsigned char)(aw8697->info.cont_td >> 8));
	aw8697_i2c_write(aw8697, AW8697_REG_TD_L,
			 (unsigned char)(aw8697->info.cont_td >> 0));
	aw8697_i2c_write(aw8697, AW8697_REG_TSET, aw8697->info.tset);

	/* zero cross */
	aw8697_i2c_write(aw8697, AW8697_REG_ZC_THRSH_H,
			 (unsigned char)(aw8697->info.cont_zc_thr >> 8));
	aw8697_i2c_write(aw8697, AW8697_REG_ZC_THRSH_L,
			 (unsigned char)(aw8697->info.cont_zc_thr >> 0));

	aw8697_i2c_write_bits(aw8697, AW8697_REG_BEMF_NUM,
			      AW8697_BIT_BEMF_NUM_BRK_MASK,
			      aw8697->info.cont_num_brk);
	aw8697_i2c_write(aw8697, AW8697_REG_TIME_NZC, 0x23);	// 35*171us=5.985ms

	/* f0 driver level */
	aw8697_i2c_write(aw8697, AW8697_REG_DRV_LVL, aw8697->info.cont_drv_lvl);
	aw8697_i2c_write(aw8697, AW8697_REG_DRV_LVL_OV,
			 aw8697->info.cont_drv_lvl_ov);

	/* cont play go */
	aw8697_haptic_play_go(aw8697, true);

	return 0;
}

#ifndef USE_CONT_F0_CALI
static int aw8697_get_glb_state(struct aw8697 *aw8697)
{
	unsigned char glb_state_val = 0;

	aw8697_i2c_read(aw8697, AW8697_REG_GLB_STATE, &glb_state_val);
	return glb_state_val;
}
#endif

#ifndef USE_CONT_F0_CALI
/*****************************************************
 *
 * haptic f0 cali
 *
 *****************************************************/
static int aw8697_haptic_get_f0(struct aw8697 *aw8697)
{
	int ret = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;
	unsigned char f0_pre_num = 0;
	unsigned char f0_wait_num = 0;
	unsigned char f0_repeat_num = 0;
	unsigned char f0_trace_num = 0;
	unsigned int t_f0_ms = 0;
	unsigned int t_f0_trace_ms = 0;
	unsigned int f0_cali_cnt = 50;

	pr_info("%s enter\n", __func__);

	aw8697->f0 = aw8697->info.f0_pre;

	/* f0 calibrate work mode */
	aw8697_haptic_stop(aw8697);
	aw8697_i2c_write(aw8697, AW8697_REG_TRIM_LRA, 0x00);
	aw8697_haptic_play_mode(aw8697, AW8697_HAPTIC_CONT_MODE);

	aw8697_i2c_write_bits(aw8697, AW8697_REG_CONT_CTRL,
			      AW8697_BIT_CONT_CTRL_EN_CLOSE_MASK,
			      AW8697_BIT_CONT_CTRL_OPEN_PLAYBACK);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_CONT_CTRL,
			      AW8697_BIT_CONT_CTRL_F0_DETECT_MASK,
			      AW8697_BIT_CONT_CTRL_F0_DETECT_ENABLE);

	/* LPF */
	aw8697_i2c_write_bits(aw8697, AW8697_REG_DATCTRL,
			      AW8697_BIT_DATCTRL_FC_MASK,
			      AW8697_BIT_DATCTRL_FC_1000HZ);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_DATCTRL,
			      AW8697_BIT_DATCTRL_LPF_ENABLE_MASK,
			      AW8697_BIT_DATCTRL_LPF_ENABLE);

	/* LRA OSC Source */
	if (aw8697->f0_cali_flag == AW8697_HAPTIC_CALI_F0) {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_ANACTRL,
				      AW8697_BIT_ANACTRL_LRA_SRC_MASK,
				      AW8697_BIT_ANACTRL_LRA_SRC_REG);
	} else {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_ANACTRL,
				      AW8697_BIT_ANACTRL_LRA_SRC_MASK,
				      AW8697_BIT_ANACTRL_LRA_SRC_EFUSE);
	}

	/* preset f0 */
	aw8697_haptic_set_f0_preset(aw8697);

	/* f0 driver level */
	aw8697_i2c_write(aw8697, AW8697_REG_DRV_LVL, aw8697->info.cont_drv_lvl);

	/* f0 trace parameter */
	f0_pre_num = aw8697->info.f0_trace_parameter[0];
	f0_wait_num = aw8697->info.f0_trace_parameter[1];
	f0_repeat_num = aw8697->info.f0_trace_parameter[2];
	f0_trace_num = aw8697->info.f0_trace_parameter[3];
	aw8697_i2c_write(aw8697, AW8697_REG_NUM_F0_1,
			 (f0_pre_num << 4) | (f0_wait_num << 0));
	aw8697_i2c_write(aw8697, AW8697_REG_NUM_F0_2, (f0_repeat_num << 0));
	aw8697_i2c_write(aw8697, AW8697_REG_NUM_F0_3, (f0_trace_num << 0));

	/* clear aw8697 interrupt */
	ret = aw8697_i2c_read(aw8697, AW8697_REG_SYSINT, &reg_val);

	/* play go and start f0 calibration */
	aw8697_haptic_play_go(aw8697, true);

	/* f0 trace time */
	t_f0_ms = 1000 * 10 / aw8697->info.f0_pre;
	t_f0_trace_ms =
	    t_f0_ms * (f0_pre_num + f0_wait_num +
		       (f0_trace_num + f0_wait_num) * (f0_repeat_num - 1)) + 50;
	usleep_range(t_f0_trace_ms * 1000, t_f0_trace_ms * 1000 + 500);


	for (i = 0; i < f0_cali_cnt; i++) {
		reg_val = aw8697_get_glb_state(aw8697);
		/* f0 calibrate done */
		if ((reg_val & 0x0f) == 0x00) {
			aw8697_haptic_read_f0(aw8697);
			aw8697_haptic_read_beme(aw8697);
			break;
		}
		usleep_range(10000, 10500);
		pr_info("%s: f0 cali sleep 10ms\n", __func__);
	}

	if ((i == f0_cali_cnt) ||(aw8697->f0 == 0)){
		ret = -1;
	} else {
		ret = 0;
	}

	/* restore default config */
	aw8697_i2c_write_bits(aw8697, AW8697_REG_CONT_CTRL,
			      AW8697_BIT_CONT_CTRL_EN_CLOSE_MASK,
			      AW8697_CONT_PLAYBACK_MODE);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_CONT_CTRL,
			      AW8697_BIT_CONT_CTRL_F0_DETECT_MASK,
			      AW8697_BIT_CONT_CTRL_F0_DETECT_DISABLE);

	return ret;
}
#else
/*****************************************************
 *
 * haptic cont mode f0 cali
 *
 *****************************************************/
static int aw8697_haptic_get_f0(struct aw8697 *aw8697)
{
	int ret = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;
	unsigned int t_f0_trace_ms = 0;
	unsigned int f0_cali_cnt = 50;

	pr_info("%s enter\n", __func__);

	aw8697->f0 = aw8697->info.f0_pre;

	/* f0 calibrate work mode */
	aw8697_haptic_stop(aw8697);
	aw8697_haptic_play_mode(aw8697, AW8697_HAPTIC_CONT_MODE);

	aw8697_i2c_write_bits(aw8697, AW8697_REG_CONT_CTRL,
		AW8697_BIT_CONT_CTRL_MODE_MASK, AW8697_BIT_CONT_CTRL_BY_DRV_TIME);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_CONT_CTRL,
		AW8697_BIT_CONT_CTRL_EN_CLOSE_MASK, AW8697_BIT_CONT_CTRL_OPEN_PLAYBACK);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_CONT_CTRL,
		AW8697_BIT_CONT_CTRL_EN_CLOSE_MASK, AW8697_BIT_CONT_CTRL_CLOSE_PLAYBACK);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_CONT_CTRL,
		AW8697_BIT_CONT_CTRL_F0_DETECT_MASK, AW8697_BIT_CONT_CTRL_F0_DETECT_DISABLE);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_CONT_CTRL,
		AW8697_BIT_CONT_CTRL_AUTO_BRK_MASK, AW8697_BIT_CONT_CTRL_AUTO_BRK_DISABLE);

	/* LPF */
	//aw8697_i2c_write_bits(aw8697, AW8697_REG_DATCTRL,
	//        AW8697_BIT_DATCTRL_FC_MASK, AW8697_BIT_DATCTRL_FC_1000HZ);
	//aw8697_i2c_write_bits(aw8697, AW8697_REG_DATCTRL,
	//        AW8697_BIT_DATCTRL_LPF_ENABLE_MASK, AW8697_BIT_DATCTRL_LPF_ENABLE);

	/* LRA OSC Source */
	if(aw8697->f0_cali_flag == AW8697_HAPTIC_CALI_F0) {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_ANACTRL,
			AW8697_BIT_ANACTRL_LRA_SRC_MASK, AW8697_BIT_ANACTRL_LRA_SRC_REG);
	} else {
		aw8697_i2c_write_bits(aw8697, AW8697_REG_ANACTRL,
			AW8697_BIT_ANACTRL_LRA_SRC_MASK, AW8697_BIT_ANACTRL_LRA_SRC_EFUSE);
	}

	/* f0 driver level */
	aw8697_i2c_write(aw8697, AW8697_REG_DRV_LVL, aw8697->info.cont_drv_lvl);
	aw8697_i2c_write(aw8697, AW8697_REG_DRV_LVL_OV, aw8697->info.cont_drv_lvl_ov);

	/* TD time */
	aw8697_i2c_write(aw8697, AW8697_REG_TD_H, aw8697->info.cont_td >> 8);
	aw8697_i2c_write(aw8697, AW8697_REG_TD_L, aw8697->info.cont_td);
	aw8697_i2c_write(aw8697, AW8697_REG_TSET, aw8697->info.tset);

	/* drive time  */
	aw8697_i2c_write(aw8697, AW8697_REG_DRV_TIME, 0x75);

	/* preset f0 */
	aw8697_haptic_set_f0_preset(aw8697);

	/* clear aw8697 interrupt */
	ret = aw8697_i2c_read(aw8697, AW8697_REG_SYSINT, &reg_val);

	/* play go and start f0 calibration */
	aw8697_haptic_play_go(aw8697, true);

	/* f0 trace time */
	t_f0_trace_ms = 0xfe * 684 / 1000;
	msleep(t_f0_trace_ms);

	for(i=0; i<f0_cali_cnt; i++) {
		ret = aw8697_i2c_read(aw8697, AW8697_REG_SYSINT, &reg_val);
		/* f0 calibrate done */
		if(reg_val & 0x01) {
			aw8697_haptic_read_cont_f0(aw8697);
			aw8697_haptic_read_cont_bemf(aw8697);
			break;
		}
		msleep(10);
		pr_info("%s f0 cali sleep 10ms\n", __func__);
	}

	if(i == f0_cali_cnt) {
		ret = -1;
	} else {
		ret = 0;
	}

	/* restore default config */
	aw8697_i2c_write_bits(aw8697, AW8697_REG_CONT_CTRL,
		AW8697_BIT_CONT_CTRL_EN_CLOSE_MASK, AW8697_CONT_PLAYBACK_MODE);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_CONT_CTRL,
		AW8697_BIT_CONT_CTRL_F0_DETECT_MASK, AW8697_BIT_CONT_CTRL_F0_DETECT_DISABLE);

	return ret;
}
#endif

static int aw8697_haptic_f0_calibration(struct aw8697 *aw8697)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_limit = 0;
	char f0_cali_lra = 0;
	int f0_cali_step = 0;

	pr_info("%s enter\n", __func__);

	aw8697->f0_cali_flag = AW8697_HAPTIC_CALI_F0;

	aw8697_i2c_write(aw8697, AW8697_REG_TRIM_LRA, 0x00);
	if (aw8697_haptic_get_f0(aw8697)) {
		pr_err("%s get f0 error, user defafult f0\n", __func__);
	} else {
		/* max and min limit */
		f0_limit = aw8697->f0;
		if(((f0_limit*100) > (1700*107))&& ((f0_limit*100) < (1700*109))){//107~109% max 181
			aw8697->f0_calib_data = 0x1f;
			printk("%s aw8697->f0_calib_data = %d\n",__func__, aw8697->f0_calib_data);
			aw8697_i2c_write(aw8697, AW8697_REG_TRIM_LRA, aw8697->f0_calib_data);
			return 0;
		} else if(((f0_limit*100) > (1700 * 91)) && ((f0_limit*100) < (1700*93))){//91~93% min 161
			aw8697->f0_calib_data = 0x20;
			printk("%s aw8697->f0_calib_data = %d\n",__func__, aw8697->f0_calib_data);
			aw8697_i2c_write(aw8697, AW8697_REG_TRIM_LRA, aw8697->f0_calib_data);
			return 0;
		}

		if (aw8697->f0 * 100 <
			aw8697->info.f0_pre * (100 - aw8697->info.f0_cali_percen)) {
			    f0_limit = aw8697->info.f0_pre;
		}
		if (aw8697->f0 * 100 >
			aw8697->info.f0_pre * (100 + aw8697->info.f0_cali_percen)) {
			    f0_limit = aw8697->info.f0_pre;
		}

		/* calculate cali step */
		f0_cali_step =
		    100000 * ((int)f0_limit -
			      (int)aw8697->info.f0_pre) / ((int)f0_limit * 25);
		pr_info("%s  line=%d f0_cali_step=%d\n", __func__, __LINE__,
		       f0_cali_step);
		pr_info("%s line=%d  f0_limit=%d\n", __func__, __LINE__,
		       (int)f0_limit);
		pr_info("%s line=%d  aw8697->info.f0_pre=%d\n", __func__,
		       __LINE__, (int)aw8697->info.f0_pre);

		if (f0_cali_step >= 0) {	/*f0_cali_step >= 0 */
			if (f0_cali_step % 10 >= 5) {
				f0_cali_step = f0_cali_step / 10 + 1 + 32;
			} else {
				f0_cali_step = f0_cali_step / 10 + 32;
			}
		} else {	/*f0_cali_step < 0 */
			if (f0_cali_step % 10 <= -5) {
				f0_cali_step = 32 + (f0_cali_step / 10 - 1);
			} else {
				f0_cali_step = 32 + f0_cali_step / 10;
			}
		}

		if (f0_cali_step > 31) {
			f0_cali_lra = (char)f0_cali_step - 32;
		} else {
			f0_cali_lra = (char)f0_cali_step + 32;
		}
		aw8697->f0_calib_data = (int)f0_cali_lra;
		printk("%s f0_cali_lra=%d\n", __func__, (int)f0_cali_lra);

		/* update cali step */
		aw8697_i2c_write(aw8697, AW8697_REG_TRIM_LRA,
				 (char)f0_cali_lra);
		aw8697_i2c_read(aw8697, AW8697_REG_TRIM_LRA, &reg_val);
		pr_info("%s final trim_lra=0x%02x\n", __func__, reg_val);
	}

	/* restore default work mode */
	aw8697_haptic_play_mode(aw8697, AW8697_HAPTIC_STANDBY_MODE);
	aw8697->play_mode = AW8697_HAPTIC_RAM_MODE;
	aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSCTRL,
			      AW8697_BIT_SYSCTRL_PLAY_MODE_MASK,
			      AW8697_BIT_SYSCTRL_PLAY_MODE_RAM);
	aw8697_haptic_stop(aw8697);

	return ret;
}

/*****************************************************
 *
 * haptic fops
 *
 *****************************************************/
static int aw8697_file_open(struct inode *inode, struct file *file)
{
	if (!try_module_get(THIS_MODULE))
		return -ENODEV;
	pr_info("%s enter\n", __func__);
	file->private_data = (void *)g_aw8697;

	return 0;
}

static int aw8697_file_release(struct inode *inode, struct file *file)
{
	file->private_data = (void *)NULL;
	pr_info("%s enter\n", __func__);
	module_put(THIS_MODULE);

	return 0;
}

static long aw8697_file_unlocked_ioctl(struct file *file, unsigned int cmd,
				       unsigned long arg)
{
	struct aw8697 *aw8697 = (struct aw8697 *)file->private_data;

	int ret = 0;
	pr_info("%s enter\n", __func__);
	dev_info(aw8697->dev, "%s: cmd=0x%x, arg=0x%lx\n", __func__, cmd, arg);

	mutex_lock(&aw8697->lock);

	if (_IOC_TYPE(cmd) != AW8697_HAPTIC_IOCTL_MAGIC) {
		dev_err(aw8697->dev, "%s: cmd magic err\n", __func__);
		return -EINVAL;
	}

	switch (cmd) {
	default:
		dev_err(aw8697->dev, "%s, unknown cmd\n", __func__);
		break;
	}

	mutex_unlock(&aw8697->lock);

	return ret;
}

static ssize_t aw8697_file_read(struct file *filp, char *buff, size_t len,
				loff_t *offset)
{
	struct aw8697 *aw8697 = (struct aw8697 *)filp->private_data;
	int ret = 0;
	int i = 0;
	unsigned char reg_val = 0;
	unsigned char *pbuff = NULL;
	pr_info("%s enter\n", __func__);
	mutex_lock(&aw8697->lock);

	dev_info(aw8697->dev, "%s: len=%zu\n", __func__, len);

	switch (aw8697->fileops.cmd) {
	case AW8697_HAPTIC_CMD_READ_REG:
		pbuff = (unsigned char *)kzalloc(len, GFP_KERNEL);
		if (pbuff != NULL) {
			for (i = 0; i < len; i++) {
				aw8697_i2c_read(aw8697, aw8697->fileops.reg + i,
						&reg_val);
				pbuff[i] = reg_val;
			}
			for (i = 0; i < len; i++) {
				dev_info(aw8697->dev, "%s: pbuff[%d]=0x%02x\n",
					 __func__, i, pbuff[i]);
			}
			ret = copy_to_user(buff, pbuff, len);
			if (ret) {
				dev_err(aw8697->dev, "%s: copy to user fail\n",
					__func__);
			}
			kfree(pbuff);
		} else {
			dev_err(aw8697->dev, "%s: alloc memory fail\n",
				__func__);
		}
		break;
	default:
		dev_err(aw8697->dev, "%s, unknown cmd %d \n", __func__,
			aw8697->fileops.cmd);
		break;
	}

	mutex_unlock(&aw8697->lock);

	return len;
}

static ssize_t aw8697_file_write(struct file *filp, const char *buff,
				 size_t len, loff_t *off)
{
	struct aw8697 *aw8697 = (struct aw8697 *)filp->private_data;
	int i = 0;
	int ret = 0;
	unsigned char *pbuff = NULL;
	pr_info("%s enter\n", __func__);
	pbuff = (unsigned char *)kzalloc(len, GFP_KERNEL);
	if (pbuff == NULL) {
		dev_err(aw8697->dev, "%s: alloc memory fail\n", __func__);
		return len;
	}
	ret = copy_from_user(pbuff, buff, len);
	if (ret) {
		dev_err(aw8697->dev, "%s: copy from user fail\n", __func__);
		return len;
	}

	for (i = 0; i < len; i++) {
		dev_info(aw8697->dev, "%s: pbuff[%d]=0x%02x\n",
			 __func__, i, pbuff[i]);
	}

	mutex_lock(&aw8697->lock);

	aw8697->fileops.cmd = pbuff[0];

	switch (aw8697->fileops.cmd) {
	case AW8697_HAPTIC_CMD_READ_REG:
		if (len == 2) {
			aw8697->fileops.reg = pbuff[1];
		} else {
			dev_err(aw8697->dev, "%s: read cmd len %zu err\n",
				__func__, len);
		}
		break;
	case AW8697_HAPTIC_CMD_WRITE_REG:
		if (len > 2) {
			for (i = 0; i < len - 2; i++) {
				dev_info(aw8697->dev,
					 "%s: write reg0x%02x=0x%02x\n",
					 __func__, pbuff[1] + i, pbuff[i + 2]);
				aw8697_i2c_write(aw8697, pbuff[1] + i,
						 pbuff[2 + i]);
			}
		} else {
			dev_err(aw8697->dev, "%s: write cmd len %zu err\n",
				__func__, len);
		}
		break;
	default:
		dev_err(aw8697->dev, "%s, unknown cmd %d \n", __func__,
			aw8697->fileops.cmd);
		break;
	}

	mutex_unlock(&aw8697->lock);

	if (pbuff != NULL) {
		kfree(pbuff);
	}
	return len;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = aw8697_file_read,
	.write = aw8697_file_write,
	.unlocked_ioctl = aw8697_file_unlocked_ioctl,
	.open = aw8697_file_open,
	.release = aw8697_file_release,
};

static struct miscdevice aw8697_haptic_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = AW8697_HAPTIC_NAME,
	.fops = &fops,
};

static int aw8697_haptic_init(struct aw8697 *aw8697)
{
	int ret = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;
	unsigned char bemf_config = 0;

	pr_info("%s enter\n", __func__);
	ret = misc_register(&aw8697_haptic_misc);
	if (ret) {
		dev_err(aw8697->dev, "%s: misc fail: %d\n", __func__, ret);
		return ret;
	}

	/* haptic audio */
	aw8697->haptic_audio.delay_val = 1;
	aw8697->haptic_audio.timer_val = 21318;
	aw8697->f0_cali_status = true;

	hrtimer_init(&aw8697->haptic_audio.timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	aw8697->haptic_audio.timer.function = aw8697_haptic_audio_timer_func;
	INIT_WORK(&aw8697->haptic_audio.work, aw8697_haptic_audio_work_routine);

	mutex_init(&aw8697->haptic_audio.lock);

	/* haptic init */
	mutex_lock(&aw8697->lock);

	aw8697->activate_mode = aw8697->info.mode;
	aw8697->osc_cali_run = 0;
	ret = aw8697_i2c_read(aw8697, AW8697_REG_WAVSEQ1, &reg_val);
	aw8697->index = reg_val & 0x7F;
	ret = aw8697_i2c_read(aw8697, AW8697_REG_DATDBG, &reg_val);
	aw8697->gain = reg_val & 0xFF;
	ret = aw8697_i2c_read(aw8697, AW8697_REG_BSTDBG4, &reg_val);
	aw8697->vmax = (reg_val >> 1) & 0x1F;
	for (i = 0; i < AW8697_SEQUENCER_SIZE; i++) {
		ret = aw8697_i2c_read(aw8697, AW8697_REG_WAVSEQ1 + i, &reg_val);
		aw8697->seq[i] = reg_val;
	}

	aw8697_haptic_play_mode(aw8697, AW8697_HAPTIC_STANDBY_MODE);

	aw8697_haptic_set_pwm(aw8697, AW8697_PWM_24K);

	aw8697_i2c_write(aw8697, AW8697_REG_BSTDBG1, aw8697->info.bstdbg[0]);
	aw8697_i2c_write(aw8697, AW8697_REG_BSTDBG2, aw8697->info.bstdbg[1]);
	aw8697_i2c_write(aw8697, AW8697_REG_BSTDBG3, aw8697->info.bstdbg[2]);
	aw8697_i2c_write(aw8697, AW8697_REG_TSET, aw8697->info.tset);
	aw8697_i2c_write(aw8697, AW8697_REG_R_SPARE, aw8697->info.r_spare);

	aw8697_i2c_write_bits(aw8697, AW8697_REG_ANADBG,
			      AW8697_BIT_ANADBG_IOC_MASK,
			      AW8697_BIT_ANADBG_IOC_4P65A);

	aw8697_haptic_set_bst_peak_cur(aw8697, AW8697_DEFAULT_PEAKCUR);

	aw8697_haptic_swicth_motorprotect_config(aw8697, 0x00, 0x00);

	aw8697_haptic_auto_boost_config(aw8697, false);

	if ((aw8697->info.trig_config[0][0] == 1) ||
		(aw8697->info.trig_config[1][0] == 1) ||
		(aw8697->info.trig_config[2][0] == 1)) {
		aw8697_haptic_trig_param_init(aw8697);
		aw8697_haptic_trig_param_config(aw8697);
	}

	aw8697_haptic_offset_calibration(aw8697);

	/* vbat compensation */
	aw8697_haptic_cont_vbat_mode(aw8697,
				     AW8697_HAPTIC_CONT_VBAT_HW_COMP_MODE);
	aw8697->ram_vbat_comp = AW8697_HAPTIC_RAM_VBAT_COMP_ENABLE;

	mutex_unlock(&aw8697->lock);

	/* f0 calibration */
	mutex_lock(&aw8697->lock);
#ifndef USE_CONT_F0_CALI
	aw8697_haptic_f0_calibration(aw8697);
#endif
	mutex_unlock(&aw8697->lock);
	/* beme config */
	bemf_config = aw8697->info.bemf_config[0];
	aw8697_i2c_write(aw8697, AW8697_REG_BEMF_VTHH_H, bemf_config);
	bemf_config = aw8697->info.bemf_config[1];
	aw8697_i2c_write(aw8697, AW8697_REG_BEMF_VTHH_L, bemf_config);
	bemf_config = aw8697->info.bemf_config[2];
	aw8697_i2c_write(aw8697, AW8697_REG_BEMF_VTHL_H, bemf_config);
	bemf_config = aw8697->info.bemf_config[3];
	aw8697_i2c_write(aw8697, AW8697_REG_BEMF_VTHL_L, bemf_config);
	return ret;
}

/*****************************************************
 *
 * vibrator
 *
 *****************************************************/
static enum hrtimer_restart qti_hap_stop_timer(struct hrtimer *timer)
{
	struct aw8697 *aw8697 = container_of(timer, struct aw8697,
					     stop_timer);
	int rc;

	pr_info("%s enter\n", __func__);
	aw8697->play.length_us = 0;
	rc = aw8697_haptic_play_go(aw8697, false);	// qti_haptics_play(aw8697, false);
	if (rc < 0)
		dev_err(aw8697->dev, "Stop playing failed, rc=%d\n", rc);

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart qti_hap_disable_timer(struct hrtimer *timer)
{
	struct aw8697 *aw8697 = container_of(timer, struct aw8697,
					     hap_disable_timer);
	int rc;

	pr_info("%s enter\n", __func__);
	rc = aw8697_haptic_play_go(aw8697, false);	//qti_haptics_module_en(aw8697, false);
	if (rc < 0)
		dev_err(aw8697->dev, "Disable haptics module failed, rc=%d\n",
			rc);

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart aw8697_vibrator_timer_func(struct hrtimer *timer)
{
	struct aw8697 *aw8697 = container_of(timer, struct aw8697, timer);

	pr_info("%s enter\n", __func__);

	aw8697->state = 0;
	//schedule_work(&aw8697->vibrator_work);
	queue_work(aw8697->work_queue, &aw8697->vibrator_work);

	return HRTIMER_NORESTART;
}

static void aw8697_vibrator_work_routine(struct work_struct *work)
{
	struct aw8697 *aw8697 =
	    container_of(work, struct aw8697, vibrator_work);

	pr_debug("%s enter\n", __func__);
	pr_info("%s: effect_id = %d state=%d activate_mode = %d duration = %d\n", __func__,
		aw8697->effect_id, aw8697->state, aw8697->activate_mode, aw8697->duration);
	mutex_lock(&aw8697->lock);
	aw8697_haptic_upload_lra(aw8697, true);
	aw8697_haptic_stop(aw8697);
	if (aw8697->state) {
		if (aw8697->activate_mode == AW8697_HAPTIC_ACTIVATE_RAM_MODE) {
			aw8697_haptic_ram_vbat_comp(aw8697, false);
			aw8697_haptic_play_effect_seq(aw8697, true);
		} else if (aw8697->activate_mode ==
			   AW8697_HAPTIC_ACTIVATE_RAM_LOOP_MODE) {
			aw8697_haptic_ram_vbat_comp(aw8697, true);
			aw8697_haptic_play_effect_seq(aw8697, true);
			hrtimer_start(&aw8697->timer,
				      ktime_set(aw8697->duration / 1000,
						(aw8697->duration % 1000) *
						1000000), HRTIMER_MODE_REL);
		} else if (aw8697->activate_mode ==
			   AW8697_HAPTIC_ACTIVATE_CONT_MODE) {
			aw8697_haptic_cont(aw8697);
			hrtimer_start(&aw8697->timer,
				      ktime_set(aw8697->duration / 1000,
						(aw8697->duration % 1000) *
						1000000), HRTIMER_MODE_REL);
		} else {
			/*other mode */
		}
	}
	mutex_unlock(&aw8697->lock);
}

static int aw8697_vibrator_init(struct aw8697 *aw8697)
{
	pr_info("%s enter\n", __func__);

	hrtimer_init(&aw8697->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw8697->timer.function = aw8697_vibrator_timer_func;
	INIT_WORK(&aw8697->vibrator_work, aw8697_vibrator_work_routine);
	INIT_WORK(&aw8697->rtp_work, aw8697_rtp_work_routine);

	mutex_init(&aw8697->lock);
	mutex_init(&aw8697->rtp_lock);
	atomic_set(&aw8697->is_in_rtp_loop, 0);
	atomic_set(&aw8697->exit_in_rtp_loop, 0);
	atomic_set(&aw8697->is_in_write_loop, 0);
	init_waitqueue_head(&aw8697->wait_q);
	init_waitqueue_head(&aw8697->stop_wait_q);

	return 0;
}

/******************************************************
 *
 * irq
 *
 ******************************************************/
static void aw8697_interrupt_clear(struct aw8697 *aw8697)
{
	unsigned char reg_val = 0;
	aw8697_i2c_read(aw8697, AW8697_REG_SYSINT, &reg_val);
}

static void aw8697_interrupt_setup(struct aw8697 *aw8697)
{
	unsigned char reg_val = 0;


	aw8697_i2c_read(aw8697, AW8697_REG_SYSINT, &reg_val);

	/* edge int mode */
	aw8697_i2c_write_bits(aw8697, AW8697_REG_DBGCTRL,
			      AW8697_BIT_DBGCTRL_INT_MODE_MASK,
			      AW8697_BIT_DBGCTRL_INT_MODE_EDGE);

	/* int enable */
	aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSINTM,
			      AW8697_BIT_SYSINTM_BSTERR_MASK,
			      AW8697_BIT_SYSINTM_BSTERR_OFF);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSINTM,
			      AW8697_BIT_SYSINTM_OV_MASK,
			      AW8697_BIT_SYSINTM_OV_EN);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSINTM,
			      AW8697_BIT_SYSINTM_UVLO_MASK,
			      AW8697_BIT_SYSINTM_UVLO_EN);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSINTM,
			      AW8697_BIT_SYSINTM_OCD_MASK,
			      AW8697_BIT_SYSINTM_OCD_EN);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSINTM,
			      AW8697_BIT_SYSINTM_OT_MASK,
			      AW8697_BIT_SYSINTM_OT_EN);
}

static irqreturn_t aw8697_irq(int irq, void *data)
{
	struct aw8697 *aw8697 = data;
	unsigned char reg_val = 0;
	unsigned char dbg_val = 0;
	unsigned int buf_len = 0;
	unsigned period_size =  aw8697->ram.base_addr >> 2;


	atomic_set(&aw8697->is_in_rtp_loop, 1);
	aw8697_i2c_read(aw8697, AW8697_REG_SYSINT, &reg_val);
	//pr_info("%s: reg SYSINT=0x%x\n", __func__, reg_val);
	aw8697_i2c_read(aw8697, AW8697_REG_DBGSTAT, &dbg_val);
	//pr_info("%s: reg DBGSTAT=0x%x\n", __func__, dbg_val);

	if (reg_val & AW8697_BIT_SYSINT_OVI) {
		pr_err("%s chip ov int error\n", __func__);
	}
	if (reg_val & AW8697_BIT_SYSINT_UVLI) {
		pr_err("%s chip uvlo int error\n", __func__);
	}
	if (reg_val & AW8697_BIT_SYSINT_OCDI) {
		pr_err("%s chip over current int error\n", __func__);
	}
	if (reg_val & AW8697_BIT_SYSINT_OTI) {
		pr_err("%s chip over temperature int error\n", __func__);
	}
	if (reg_val & AW8697_BIT_SYSINT_DONEI) {
		pr_info("%s chip playback done\n", __func__);
	}

	if (reg_val & AW8697_BIT_SYSINT_FF_AEI) {
		pr_debug("%s: aw8697 rtp fifo almost empty int\n", __func__);
		if (aw8697->rtp_init) {
			while ((!aw8697_haptic_rtp_get_fifo_afi(aw8697)) &&
			       (aw8697->play_mode == AW8697_HAPTIC_RTP_MODE) && !atomic_read(&aw8697->exit_in_rtp_loop)) {
				mutex_lock(&aw8697->rtp_lock);
				if (!aw8697_rtp) {
					pr_info("%s:aw8697_rtp is null break\n",
					__func__);
					mutex_unlock(&aw8697->rtp_lock);
					break;
				}

				if (aw8697->is_custom_wave == 1) {
					buf_len = read_rb(aw8697_rtp->data,  period_size);
					aw8697_i2c_writes(aw8697, AW8697_REG_RTP_DATA, aw8697_rtp->data, buf_len);
					if (buf_len < period_size) {
						pr_info("%s: rtp update complete\n",
							__func__);
						aw8697_haptic_set_rtp_aei(aw8697,
									  false);
						aw8697->rtp_cnt = 0;
						aw8697->rtp_init = 0;
						mutex_unlock(&aw8697->rtp_lock);
						break;
					}
				} else {
					if ((aw8697_rtp->len - aw8697->rtp_cnt) < period_size) {
						buf_len =
						aw8697_rtp->len - aw8697->rtp_cnt;
					} else {
						buf_len = period_size;
					}
					aw8697_i2c_writes(aw8697, AW8697_REG_RTP_DATA,
								&aw8697_rtp->
								data[aw8697->rtp_cnt],
								buf_len);
					aw8697->rtp_cnt += buf_len;
					if (aw8697->rtp_cnt == aw8697_rtp->len) {
						pr_info("%s: rtp update complete\n",
							__func__);
						aw8697_haptic_set_rtp_aei(aw8697,
									  false);
						aw8697->rtp_cnt = 0;
						aw8697->rtp_init = 0;
						mutex_unlock(&aw8697->rtp_lock);
						break;
					}
				}
				mutex_unlock(&aw8697->rtp_lock);
			}
		} else {
			pr_err("%s: aw8697 rtp init = %d, init error\n",
				__func__, aw8697->rtp_init);
		}
	}

	if (reg_val & AW8697_BIT_SYSINT_FF_AFI) {
		pr_debug("%s: aw8697 rtp mode fifo full empty\n", __func__);
	}

	if (aw8697->play_mode != AW8697_HAPTIC_RTP_MODE || atomic_read(&aw8697->exit_in_rtp_loop)) {
		aw8697_haptic_set_rtp_aei(aw8697, false);
	}

	aw8697_i2c_read(aw8697, AW8697_REG_SYSINT, &reg_val);
	pr_debug("%s: reg SYSINT=0x%x\n", __func__, reg_val);
	aw8697_i2c_read(aw8697, AW8697_REG_SYSST, &reg_val);
	pr_debug("%s: reg SYSST=0x%x\n", __func__, reg_val);
	atomic_set(&aw8697->is_in_rtp_loop, 0);
	wake_up_interruptible(&aw8697->wait_q);
	pr_debug("%s exit\n", __func__);
	return IRQ_HANDLED;
}

/*****************************************************
 *
 * device tree
 *
 *****************************************************/
static int aw8697_parse_dt(struct device *dev, struct aw8697 *aw8697,
			   struct device_node *np)
{
	unsigned int val = 0;
	unsigned int bstdbg[6];
	unsigned int f0_trace_parameter[4];
	unsigned int bemf_config[4];
	unsigned int rtp_time[175];
	unsigned int trig_config[15];
	struct qti_hap_config *config = &aw8697->config;
	struct device_node *child_node;
	struct qti_hap_effect *effect;
	int rc = 0, tmp, i = 0, j;

	aw8697->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (aw8697->reset_gpio < 0) {
		dev_err(dev,
			"%s: no reset gpio provided, will not HW reset device\n",
			__func__);
		return -EINVAL;
	} else {
		dev_info(dev, "%s: reset gpio provided ok\n", __func__);
	}
	aw8697->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (aw8697->irq_gpio < 0) {
		dev_err(dev, "%s: no irq gpio provided.\n", __func__);
	} else {
		dev_info(dev, "%s: irq gpio provided ok.\n", __func__);
	}

	val = of_property_read_u32(np, "vib_mode", &aw8697->info.mode);
	if (val != 0)
		printk("vib_mode not found\n");
	val = of_property_read_u32(np, "vib_f0_pre", &aw8697->info.f0_pre);
	if (val != 0)
		printk("vib_f0_pre not found\n");
	val =
	    of_property_read_u32(np, "vib_f0_cali_percen",
				 &aw8697->info.f0_cali_percen);
	if (val != 0)
		printk("vib_f0_cali_percen not found\n");
	val =
	    of_property_read_u32(np, "vib_cont_drv_lev",
				 &aw8697->info.cont_drv_lvl);
	if (val != 0)
		printk("vib_cont_drv_lev not found\n");
	val =
	    of_property_read_u32(np, "vib_cont_drv_lvl_ov",
				 &aw8697->info.cont_drv_lvl_ov);
	if (val != 0)
		printk("vib_cont_drv_lvl_ov not found\n");
	val = of_property_read_u32(np, "vib_cont_td", &aw8697->info.cont_td);
	if (val != 0)
		printk("vib_cont_td not found\n");
	val =
	    of_property_read_u32(np, "vib_cont_zc_thr",
				 &aw8697->info.cont_zc_thr);
	if (val != 0)
		printk("vib_cont_zc_thr not found\n");
	val =
	    of_property_read_u32(np, "vib_cont_num_brk",
				 &aw8697->info.cont_num_brk);
	if (val != 0)
		printk("vib_cont_num_brk not found\n");
	val = of_property_read_u32(np, "vib_f0_coeff", &aw8697->info.f0_coeff);
	if (val != 0)
		printk("vib_f0_coeff not found\n");

	val = of_property_read_u32(np, "vib_tset", &aw8697->info.tset);
	if (val != 0)
		printk("%s vib_tset not found\n", __func__);
	val = of_property_read_u32(np, "vib_r_spare", &aw8697->info.r_spare);
	if (val != 0)
		printk("%s vib_r_spare not found\n", __func__);
	val = of_property_read_u32_array(np, "vib_bstdbg",
					 bstdbg, ARRAY_SIZE(bstdbg));
	if (val != 0)
		printk("%s vib_bstdbg not found\n", __func__);
	memcpy(aw8697->info.bstdbg, bstdbg, sizeof(bstdbg));

	val = of_property_read_u32_array(np, "vib_f0_trace_parameter",
					 f0_trace_parameter,
					 ARRAY_SIZE(f0_trace_parameter));
	if (val != 0)
		printk("%s vib_f0_trace_parameter not found\n", __func__);
	memcpy(aw8697->info.f0_trace_parameter, f0_trace_parameter,
	       sizeof(f0_trace_parameter));
	val =
	    of_property_read_u32_array(np, "vib_bemf_config", bemf_config,
				       ARRAY_SIZE(bemf_config));
	if (val != 0)
		printk("%s vib_bemf_config not found\n", __func__);
	memcpy(aw8697->info.bemf_config, bemf_config, sizeof(bemf_config));

	val =
	    of_property_read_u32_array(np, "vib_trig_config", trig_config,
				       ARRAY_SIZE(trig_config));
	if (val != 0)
		printk("%s vib_trig_config not found\n", __func__);
	memcpy(aw8697->info.trig_config, trig_config, sizeof(trig_config));

	val =
	    of_property_read_u32_array(np, "vib_rtp_time", rtp_time,
				       ARRAY_SIZE(rtp_time));
	if (val != 0)
		printk("%s vib_rtp_time not found\n", __func__);
	memcpy(aw8697->info.rtp_time, rtp_time, sizeof(rtp_time));

	val =
	    of_property_read_u32(np, "vib_effect_id_boundary",
				 &aw8697->info.effect_id_boundary);
	if (val != 0)
		printk("%s vib_effect_id_boundary not found\n", __func__);
	val =
	    of_property_read_u32(np, "vib_effect_max",
				 &aw8697->info.effect_max);
	if (val != 0)
		printk("%s vib_effect_max not found\n", __func__);
	val =
	    of_property_read_u32(np, "vib_bst_vol_default",
				 &aw8697->info.bst_vol_default);
	if (val != 0)
		printk("%s vib_bst_vol_default not found\n", __func__);
	val =
	    of_property_read_u32(np, "vib_bst_vol_ram",
				 &aw8697->info.bst_vol_ram);
	if (val != 0)
		printk("%s vib_bst_vol_ram not found\n", __func__);
	val =
	    of_property_read_u32(np, "vib_bst_vol_rtp",
				 &aw8697->info.bst_vol_rtp);
	if (val != 0)
		printk("%s vib_bst_vol_rtp not found\n", __func__);

	config->play_rate_us = HAP_PLAY_RATE_US_DEFAULT;
	rc = of_property_read_u32(np, "qcom,play-rate-us", &tmp);
	if (!rc)
	config->play_rate_us = (tmp >= HAP_PLAY_RATE_US_MAX) ?
		HAP_PLAY_RATE_US_MAX : tmp;

	aw8697->constant.pattern = devm_kcalloc(aw8697->dev,
						HAP_WAVEFORM_BUFFER_MAX,
						sizeof(u8), GFP_KERNEL);
	if (!aw8697->constant.pattern)
		return -ENOMEM;

	tmp = of_get_available_child_count(np);
	aw8697->predefined = devm_kcalloc(aw8697->dev, tmp,
					  sizeof(*aw8697->predefined),
					  GFP_KERNEL);
	if (!aw8697->predefined)
		return -ENOMEM;

	aw8697->effects_count = tmp;
	for_each_available_child_of_node(np, child_node) {
		effect = &aw8697->predefined[i++];
		rc = of_property_read_u32(child_node, "qcom,effect-id",
					  &effect->id);
		if (rc != 0) {
			printk("%s Read qcom,effect-id failed\n", __func__);
		}

		effect->vmax_mv = config->vmax_mv;
		rc = of_property_read_u32(child_node, "qcom,wf-vmax-mv", &tmp);
		if (rc != 0)
			printk("%s  Read qcom,wf-vmax-mv failed !\n", __func__);
		else
			effect->vmax_mv = tmp;

		printk("%s ---%d effect->vmax_mv =%d \n", __func__, __LINE__, effect->vmax_mv);
		rc = of_property_count_elems_of_size(child_node,
						     "qcom,wf-pattern",
						     sizeof(u8));
		if (rc < 0) {
			printk("%s Count qcom,wf-pattern property failed !\n",
			       __func__);
		} else if (rc == 0) {
			printk("%s qcom,wf-pattern has no data\n", __func__);
		}
		printk("%s ---%d \n", __func__, __LINE__);

		effect->pattern_length = rc;
		effect->pattern = devm_kcalloc(aw8697->dev,
					       effect->pattern_length,
					       sizeof(u8), GFP_KERNEL);

		rc = of_property_read_u8_array(child_node, "qcom,wf-pattern",
					       effect->pattern,
					       effect->pattern_length);
		if (rc < 0) {
			printk("%s Read qcom,wf-pattern property failed !\n",
			       __func__);
		}

		effect->play_rate_us = config->play_rate_us;
		rc = of_property_read_u32(child_node, "qcom,wf-play-rate-us",
					  &tmp);
		if (rc < 0)
			printk("%s Read qcom,wf-play-rate-us failed !\n",
			       __func__);
		else
			effect->play_rate_us = tmp;

		rc = of_property_read_u32(child_node, "qcom,wf-repeat-count",
					  &tmp);
		if (rc < 0) {
			printk("%s Read  qcom,wf-repeat-count failed !\n",
			       __func__);
		} else {
			for (j = 0; j < ARRAY_SIZE(wf_repeat); j++)
				if (tmp <= wf_repeat[j])
					break;

			effect->wf_repeat_n = j;
		}

		rc = of_property_read_u32(child_node, "qcom,wf-s-repeat-count",
					  &tmp);
		if (rc < 0) {
			printk("%s Read  qcom,wf-s-repeat-count failed !\n",
			       __func__);
		} else {
			for (j = 0; j < ARRAY_SIZE(wf_s_repeat); j++)
				if (tmp <= wf_s_repeat[j])
					break;

			effect->wf_s_repeat_n = j;
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
			    ("%s wf-brake-pattern shouldn't be more than %d bytes\n",
			     __func__, HAP_BRAKE_PATTERN_MAX);
		}

		rc = of_property_read_u8_array(child_node,
					       "qcom,wf-brake-pattern",
					       effect->brake, tmp);
		if (rc < 0) {
			printk("%s Failed to get wf-brake-pattern !\n",
			       __func__);
		}

		effect->brake_pattern_length = tmp;
	}

	printk(" 20190420_dt       aw8697->info.bst_vol_default: 0x%x\n",
	       aw8697->info.bst_vol_default);
	printk(" 20190420_dt       aw8697->info.bst_vol_ram: 0x%x\n",
	       aw8697->info.bst_vol_ram);
	printk(" 20190420_dt       aw8697->info.bst_vol_rtp: 0x%x\n",
	       aw8697->info.bst_vol_rtp);
	return 0;
}

static inline void get_play_length(struct qti_hap_play_info *play,
				   int *length_us)
{
	struct qti_hap_effect *effect = play->effect;
	int tmp;
	//printk("%s  %d enter\n", __func__, __LINE__);

	tmp = effect->pattern_length * effect->play_rate_us;
	tmp *= wf_s_repeat[effect->wf_s_repeat_n];
	tmp *= wf_repeat[effect->wf_repeat_n];
	if (effect->brake_en)
		tmp += effect->play_rate_us * effect->brake_pattern_length;

	*length_us = tmp;
}

static int aw8697_haptics_upload_effect(struct input_dev *dev,
					struct ff_effect *effect,
					struct ff_effect *old)
{
	struct aw8697 *aw8697 = input_get_drvdata(dev);
	struct qti_hap_play_info *play = &aw8697->play;
	s16 data[CUSTOM_DATA_LEN];
	ktime_t rem;
	s64 time_us;
	int ret;

	/*for osc calibration*/
	if (aw8697->osc_cali_run != 0)
		return 0;

	if (hrtimer_active(&aw8697->timer)) {
		rem = hrtimer_get_remaining(&aw8697->timer);
		time_us = ktime_to_us(rem);
		printk("waiting for playing clear sequence: %lld us\n",
			time_us);
		usleep_range(time_us, time_us + 100);
	}
	pr_debug("%s: effect->type=0x%x,FF_CONSTANT=0x%x,FF_PERIODIC=0x%x\n",
		__func__, effect->type, FF_CONSTANT, FF_PERIODIC);
	aw8697->effect_type = effect->type;
	 mutex_lock(&aw8697->lock);
	 while (atomic_read(&aw8697->exit_in_rtp_loop)) {
		 pr_info("%s  goint to waiting rtp  exit\n", __func__);
		 mutex_unlock(&aw8697->lock);
		 ret = wait_event_interruptible(aw8697->stop_wait_q, atomic_read(&aw8697->exit_in_rtp_loop) == 0);
		 pr_info("%s  wakeup \n", __func__);
		 if (ret == -ERESTARTSYS) {
			 mutex_unlock(&aw8697->lock);
			 pr_err("%s wake up by signal return erro\n", __func__);
			 return ret;
		 }
		 mutex_lock(&aw8697->lock);
	 }

	if (aw8697->effect_type == FF_CONSTANT) {
		pr_debug("%s: effect_type is  FF_CONSTANT! \n", __func__);
		/*cont mode set duration */
		aw8697->duration = effect->replay.length;
		aw8697->activate_mode = AW8697_HAPTIC_ACTIVATE_RAM_LOOP_MODE;
		aw8697->effect_id = aw8697->info.effect_id_boundary;

	} else if (aw8697->effect_type == FF_PERIODIC) {
		if (aw8697->effects_count == 0) {
			mutex_unlock(&aw8697->lock);
			return -EINVAL;
		}

		pr_debug("%s: effect_type is  FF_PERIODIC! \n", __func__);
		if (copy_from_user(data, effect->u.periodic.custom_data,
				   sizeof(s16) * CUSTOM_DATA_LEN)) {
			mutex_unlock(&aw8697->lock);
			return -EFAULT;
		}

		aw8697->effect_id = data[0];
		pr_debug("%s: aw8697->effect_id =%d \n", __func__, aw8697->effect_id);
		play->vmax_mv = effect->u.periodic.magnitude; /*vmax level*/

		if (aw8697->effect_id < 0 ||
			aw8697->effect_id > aw8697->info.effect_max) {
			mutex_unlock(&aw8697->lock);
			return 0;
		}
		aw8697->is_custom_wave = 0;

		if (aw8697->effect_id < aw8697->info.effect_id_boundary) {
			aw8697->activate_mode = AW8697_HAPTIC_ACTIVATE_RAM_MODE;
			pr_debug("%s: aw8697->effect_id=%d , aw8697->activate_mode = %d\n",
				__func__, aw8697->effect_id, aw8697->activate_mode);
			data[1] = aw8697->predefined[aw8697->effect_id].play_rate_us/1000000; /*second data*/
			data[2] = aw8697->predefined[aw8697->effect_id].play_rate_us/1000;  /*millisecond data*/
		}
		if (aw8697->effect_id >= aw8697->info.effect_id_boundary) {
			aw8697->activate_mode = AW8697_HAPTIC_ACTIVATE_RTP_MODE;
			pr_debug("%s: aw8697->effect_id=%d , aw8697->activate_mode = %d\n",
				__func__, aw8697->effect_id, aw8697->activate_mode);
			data[1] = 30; /*second data*/
			data[2] = 0;  /*millisecond data*/
		}
		if (aw8697->effect_id == CUSTOME_WAVE_ID) {
			aw8697->activate_mode = AW8697_HAPTIC_ACTIVATE_RTP_MODE;
			pr_debug("%s: aw8697->effect_id=%d , aw8697->activate_mode = %d\n",
				__func__, aw8697->effect_id, aw8697->activate_mode);
			data[1] = 30; /*second data*/
			data[2] = 0;  /*millisecond data*/
			aw8697->is_custom_wave = 1;
			rb_init();
		}


		if (copy_to_user(effect->u.periodic.custom_data, data,
			sizeof(s16) * CUSTOM_DATA_LEN)) {
			mutex_unlock(&aw8697->lock);
			return -EFAULT;
		}

	} else {
		pr_err("%s Unsupported effect type: %d\n", __func__,
		       effect->type);
	}
	mutex_unlock(&aw8697->lock);
	return 0;
}

static int aw8697_haptics_playback(struct input_dev *dev, int effect_id,
				   int val)
{
	struct aw8697 *aw8697 = input_get_drvdata(dev);
	int rc = 0;

	//printk("%s effect_id=%d , val = %d\n", __func__, effect_id, val);
	//printk("%s aw8697->effect_id=%d , aw8697->activate_mode = %d\n",
	//     __func__, aw8697->effect_id, aw8697->activate_mode);

	pr_debug("%s: effect_id=%d , activate_mode = %d val = %d\n",
		__func__, aw8697->effect_id, aw8697->activate_mode, val);
	/*for osc calibration*/
	if (aw8697->osc_cali_run != 0)
		return 0;

	if (val > 0)
		aw8697->state = 1;
	if (val <= 0)
		aw8697->state = 0;
	hrtimer_cancel(&aw8697->timer);

	if (aw8697->effect_type == FF_CONSTANT &&
		aw8697->activate_mode == AW8697_HAPTIC_ACTIVATE_RAM_LOOP_MODE) {
		pr_debug("%s: enter cont_mode \n", __func__);
		//schedule_work(&aw8697->vibrator_work);
		queue_work(aw8697->work_queue, &aw8697->vibrator_work);
	} else if (aw8697->effect_type == FF_PERIODIC &&
		aw8697->activate_mode == AW8697_HAPTIC_ACTIVATE_RAM_MODE) {
		pr_debug("%s: enter  ram_mode\n", __func__);
		//schedule_work(&aw8697->vibrator_work)
		queue_work(aw8697->work_queue, &aw8697->vibrator_work);;
	} else if ((aw8697->effect_type == FF_PERIODIC) &&
		aw8697->activate_mode == AW8697_HAPTIC_ACTIVATE_RTP_MODE) {
		pr_debug("%s: enter  rtp_mode\n", __func__);
		//schedule_work(&aw8697->rtp_work);
		queue_work(aw8697->work_queue, &aw8697->rtp_work);
		//if we are in the play mode, force to exit
		if (val == 0) {
			atomic_set(&aw8697->exit_in_rtp_loop, 1);
			rb_force_exit();
			wake_up_interruptible(&aw8697->stop_wait_q);
		}
	} else {
		/*other mode */
	}

	return rc;
}

static int aw8697_haptics_erase(struct input_dev *dev, int effect_id)
{
	struct aw8697 *aw8697 = input_get_drvdata(dev);
	int rc = 0;

	/*for osc calibration*/
	if (aw8697->osc_cali_run != 0)
		return 0;

	pr_debug("%s: enter\n", __func__);
	aw8697->effect_type = 0;
	aw8697->is_custom_wave = 0;
	aw8697->duration = 0;
	return rc;
}

static void set_gain(struct work_struct * work)
{
	struct aw8697 *aw8697 = container_of(work, struct aw8697, set_gain_work);
	int comp_gain = 0;
	pr_debug("%s enter set_gain queue work\n", __func__);

	if (aw8697->new_gain >= 0x7FFF)
		aw8697->level = 0x80; /*128*/
	else if (aw8697->new_gain <= 0x3FFF)
		aw8697->level = 0x1E; /*30*/
	else
		aw8697->level = (aw8697->new_gain - 16383) / 128;

	if( aw8697->level < 0x1E)
		aw8697->level = 0x1E; /*30*/

	if ((aw8697->activate_mode == AW8697_HAPTIC_ACTIVATE_RAM_LOOP_MODE) &&
		(aw8697->ram_vbat_comp == AW8697_HAPTIC_RAM_VBAT_COMP_ENABLE) &&
		aw8697->vbat) {
		comp_gain = aw8697->level * AW8697_VBAT_REFER / aw8697->vbat;
		if (comp_gain > (128 * AW8697_VBAT_REFER / AW8697_VBAT_MIN)) {
			comp_gain = 128 * AW8697_VBAT_REFER / AW8697_VBAT_MIN;
			pr_debug("%s gain limit=%d\n", __func__, comp_gain);
		}
		pr_info("%s: enable vbat comp, level = %x comp_level = %x", __func__,
			aw8697->level, comp_gain);
		aw8697_i2c_write(aw8697, AW8697_REG_DATDBG, comp_gain);
	} else {
		pr_debug("%s: disable compsensation, vbat=%d, vbat_min=%d, vbat_ref=%d",
			__func__, aw8697->vbat, AW8697_VBAT_MIN, AW8697_VBAT_REFER);
		aw8697_i2c_write(aw8697, AW8697_REG_DATDBG, aw8697->level);
	}
}

static void aw8697_haptics_set_gain(struct input_dev *dev, u16 gain)
{
	struct aw8697 *aw8697 = input_get_drvdata(dev);
	pr_debug("%s enter\n", __func__);
	aw8697->new_gain = gain;
	queue_work(aw8697->work_queue, &aw8697->set_gain_work);
}



static ssize_t aw8697_activate_test_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);

	/* For now nothing to show */
	return snprintf(buf, PAGE_SIZE, "%d\n", aw8697->test_val);
}

static ssize_t aw8697_activate_test_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	aw8697->test_val = val;
	pr_debug("%s: aw8697->test_val=%d\n", __FUNCTION__, aw8697->test_val);

	if (aw8697->test_val == 1) {
		printk("%s  %d  \n", __func__, __LINE__);
		aw8697->duration = 3000;

		aw8697->state = 1;
		aw8697->activate_mode = AW8697_HAPTIC_ACTIVATE_CONT_MODE;
		hrtimer_cancel(&aw8697->timer);
		//schedule_work(&aw8697->vibrator_work);
		queue_work(aw8697->work_queue, &aw8697->vibrator_work);
	}
	if (aw8697->test_val == 2) {
		printk("%s  %d  \n", __func__, __LINE__);
		mutex_lock(&aw8697->lock);
		aw8697_haptic_set_wav_seq(aw8697, 0x00, 0x01);
		aw8697_haptic_set_wav_seq(aw8697, 0x01, 0x01);

		/*step 1:  choose  loop */
		aw8697_haptic_set_wav_loop(aw8697, 0x01, 0x01);
		mutex_unlock(&aw8697->lock);

		aw8697->state = 1;
		aw8697->activate_mode = AW8697_HAPTIC_ACTIVATE_RAM_MODE;
		hrtimer_cancel(&aw8697->timer);
		//schedule_work(&aw8697->vibrator_work);
		queue_work(aw8697->work_queue, &aw8697->vibrator_work);
	}

	if (aw8697->test_val == 3) {   /*Ram instead of Cont */
		aw8697->duration = 10000;

		aw8697->state = 1;
		aw8697->activate_mode = AW8697_HAPTIC_ACTIVATE_CONT_MODE;
		hrtimer_cancel(&aw8697->timer);
		//schedule_work(&aw8697->vibrator_work);
		queue_work(aw8697->work_queue, &aw8697->vibrator_work);
	}

	if (aw8697->test_val == 4) {
		mutex_lock(&aw8697->lock);
		aw8697_haptic_stop(aw8697);
		aw8697_haptic_play_mode(aw8697, AW8697_HAPTIC_RAM_MODE);

		aw8697_haptic_set_wav_seq(aw8697, 0x00, 0x01);
		aw8697_haptic_set_wav_seq(aw8697, 0x01, 0x00);

		aw8697_haptic_set_wav_loop(aw8697, 0x01, 0x01);

		if (aw8697->info.bst_vol_ram <= AW8697_MAX_BST_VO)
			aw8697_haptic_set_bst_vol(aw8697, aw8697->info.bst_vol_ram);
		else
			aw8697_haptic_set_bst_vol(aw8697, aw8697->vmax);

		aw8697->activate_mode = AW8697_HAPTIC_ACTIVATE_RAM_MODE;
		aw8697->state = 1;
		mutex_unlock(&aw8697->lock);
		hrtimer_cancel(&aw8697->timer);
		//schedule_work(&aw8697->vibrator_work);
		queue_work(aw8697->work_queue, &aw8697->vibrator_work);
	}

	return count;
}
#ifdef ENABLE_PIN_CONTROL
static int select_pin_ctl(struct aw8697 *aw8697, const char *name)
{
	size_t i;
	int rc;

	for (i = 0; i < ARRAY_SIZE(aw8697->pinctrl_state); i++) {
		const char *n = pctl_names[i];

		if (!strncmp(n, name, strlen(n))) {
			rc = pinctrl_select_state(aw8697->aw8697_pinctrl,
					aw8697->pinctrl_state[i]);
			if (rc)
				printk("cannot select '%s'\n", name);
			else
				printk("Selected '%s'\n", name);
			goto exit;
		}
	}

	rc = -EINVAL;
	printk("%s:'%s' not found\n", __func__, name);

exit:
	return rc;
}
static int aw8697_set_interrupt (struct aw8697 *aw8697)
{
	int rc = select_pin_ctl(aw8697, "aw8697_interrupt_active");
	return rc;
}
#endif

static int aw8697_hw_reset(struct aw8697 *aw8697)
{
#ifdef ENABLE_PIN_CONTROL
	int rc = select_pin_ctl(aw8697, "aw8697_reset_active");
	msleep(5);
	rc = select_pin_ctl(aw8697, "aw8697_reset_reset");
	msleep(5);
	rc = select_pin_ctl(aw8697, "aw8697_reset_active");
#endif
	if (!aw8697->enable_pin_control) {
		if (aw8697 && gpio_is_valid(aw8697->reset_gpio)) {
			gpio_set_value_cansleep(aw8697->reset_gpio, 0);
			printk("%s pull down1\n", __func__);
			msleep(5);
			gpio_set_value_cansleep(aw8697->reset_gpio, 1);
			printk("%s pull up1\n", __func__);
			msleep(5);
		} else {
			dev_err(aw8697->dev, "%s:  failed\n", __func__);
		}
	}

	return 0;
}

/*****************************************************
 *
 * check chip id
 *
 *****************************************************/
static int aw8697_read_chipid(struct aw8697 *aw8697)
{
	int ret = -1;
	unsigned char cnt = 0;
	unsigned char reg = 0;

	while (cnt < AW_READ_CHIPID_RETRIES) {
		/* hardware reset */
		aw8697_hw_reset(aw8697);

		ret = aw8697_i2c_read(aw8697, AW8697_REG_ID, &reg);
		if (ret < 0) {
			dev_err(aw8697->dev,
				"%s: failed to read register AW8697_REG_ID: %d\n",
				__func__, ret);
		}
		switch (reg) {
		case AW8697_CHIPID:
			pr_info("%s aw8697 detected\n", __func__);
			aw8697->chipid = AW8697_CHIPID;
			aw8697_haptic_softreset(aw8697);
			return 0;
		default:
			pr_info("%s unsupported device revision (0x%x)\n",
				__func__, reg);
			break;
		}
		cnt++;

		msleep(AW_READ_CHIPID_RETRY_DELAY);
	}

	return -EINVAL;
}

/******************************************************
 *
 * sys group attribute: reg
 *
 ******************************************************/
static ssize_t aw8697_i2c_reg_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);

	unsigned int databuf[2] = { 0, 0 };

	if (2 == sscanf(buf, "%x %x", &databuf[0], &databuf[1])) {
		aw8697_i2c_write(aw8697, (unsigned char)databuf[0],
				 (unsigned char)databuf[1]);
	}

	return count;
}

static ssize_t aw8697_i2c_reg_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	ssize_t len = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;
	for (i = 0; i < AW8697_REG_MAX; i++) {
		if (!(aw8697_reg_access[i] & REG_RD_ACCESS))
			continue;
		aw8697_i2c_read(aw8697, i, &reg_val);
		len +=
		    snprintf(buf + len, PAGE_SIZE - len, "reg:0x%02x=0x%02x \n",
			     i, reg_val);
	}
	return len;
}

static ssize_t aw8697_i2c_ram_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);

	unsigned int databuf[1] = { 0 };

	if (1 == sscanf(buf, "%x", &databuf[0])) {
		if (1 == databuf[0]) {
			aw8697_ram_update(aw8697);
		}
	}

	return count;
}

static ssize_t aw8697_i2c_ram_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	ssize_t len = 0;
	unsigned int i = 0;
	unsigned char reg_val = 0;

	aw8697_haptic_stop(aw8697);
	/* RAMINIT Enable */
	aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSCTRL,
			      AW8697_BIT_SYSCTRL_RAMINIT_MASK,
			      AW8697_BIT_SYSCTRL_RAMINIT_EN);

	aw8697_i2c_write(aw8697, AW8697_REG_RAMADDRH,
			 (unsigned char)(aw8697->ram.base_addr >> 8));
	aw8697_i2c_write(aw8697, AW8697_REG_RAMADDRL,
			 (unsigned char)(aw8697->ram.base_addr & 0x00ff));
	len += snprintf(buf + len, PAGE_SIZE - len, "aw8697_haptic_ram:\n");
	for (i = 0; i < aw8697->ram.len; i++) {
		aw8697_i2c_read(aw8697, AW8697_REG_RAMDATA, &reg_val);
		len += snprintf(buf + len, PAGE_SIZE - len, "0x%02x,", reg_val);
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	/* RAMINIT Disable */
	aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSCTRL,
			      AW8697_BIT_SYSCTRL_RAMINIT_MASK,
			      AW8697_BIT_SYSCTRL_RAMINIT_OFF);

	return len;
}

static ssize_t aw8697_duration_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	ktime_t time_rem;
	s64 time_ms = 0;

	if (hrtimer_active(&aw8697->timer)) {
		time_rem = hrtimer_get_remaining(&aw8697->timer);
		time_ms = ktime_to_ms(time_rem);
	}

	return snprintf(buf, PAGE_SIZE, "%lld\n", time_ms);
}

static ssize_t aw8697_duration_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	/* setting 0 on duration is NOP for now */
	if (val <= 0)
		return count;

	aw8697->duration = val;

	return count;
}

static ssize_t aw8697_activate_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);

	/* For now nothing to show */
	return snprintf(buf, PAGE_SIZE, "%d\n", aw8697->state);
}

static ssize_t aw8697_activate_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val != 0 && val != 1)
		return count;

	pr_debug("%s: value=%d\n", __FUNCTION__, val);

	mutex_lock(&aw8697->lock);
	hrtimer_cancel(&aw8697->timer);

	aw8697->state = val;

	mutex_unlock(&aw8697->lock);
	//schedule_work(&aw8697->vibrator_work);
	queue_work(aw8697->work_queue, &aw8697->vibrator_work);

	return count;
}

static ssize_t aw8697_activate_mode_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "activate_mode=%d\n",
			aw8697->activate_mode);
}

static ssize_t aw8697_activate_mode_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw8697->lock);
	aw8697->activate_mode = val;
	mutex_unlock(&aw8697->lock);
	return count;
}

static ssize_t aw8697_index_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned char reg_val = 0;
	aw8697_i2c_read(aw8697, AW8697_REG_WAVSEQ1, &reg_val);
	aw8697->index = reg_val;

	return snprintf(buf, PAGE_SIZE, "%d\n", aw8697->index);
}

static ssize_t aw8697_index_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	pr_debug("%s: value=%d\n", __FUNCTION__, val);

	mutex_lock(&aw8697->lock);
	aw8697->index = val;
	aw8697_haptic_set_repeat_wav_seq(aw8697, aw8697->index);
	mutex_unlock(&aw8697->lock);
	return count;
}

static ssize_t aw8697_vmax_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw8697->vmax);
}

static ssize_t aw8697_vmax_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	pr_debug("%s: value=%d\n", __FUNCTION__, val);

	mutex_lock(&aw8697->lock);
	aw8697->vmax = val;
	aw8697_haptic_set_bst_vol(aw8697, aw8697->vmax);
	mutex_unlock(&aw8697->lock);
	return count;
}

static ssize_t aw8697_gain_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw8697->level);
}

static ssize_t aw8697_gain_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	pr_debug("%s: value=%d\n", __FUNCTION__, val);

	mutex_lock(&aw8697->lock);
	aw8697->level = val;
	aw8697_haptic_set_gain(aw8697, aw8697->level);
	mutex_unlock(&aw8697->lock);
	return count;
}

static ssize_t aw8697_seq_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	size_t count = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < AW8697_SEQUENCER_SIZE; i++) {
		aw8697_i2c_read(aw8697, AW8697_REG_WAVSEQ1 + i, &reg_val);
		count += snprintf(buf + count, PAGE_SIZE - count,
				  "seq%d: 0x%02x\n", i + 1, reg_val);
		aw8697->seq[i] |= reg_val;
	}
	return count;
}

static ssize_t aw8697_seq_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned int databuf[2] = { 0, 0 };

	if (2 == sscanf(buf, "%x %x", &databuf[0], &databuf[1])) {
		pr_debug("%s: seq%d=0x%x\n", __FUNCTION__, databuf[0],
			 databuf[1]);
		mutex_lock(&aw8697->lock);
		aw8697->seq[databuf[0]] = (unsigned char)databuf[1];
		aw8697_haptic_set_wav_seq(aw8697, (unsigned char)databuf[0],
					  aw8697->seq[databuf[0]]);
		mutex_unlock(&aw8697->lock);
	}
	return count;
}

static ssize_t aw8697_loop_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	size_t count = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < AW8697_SEQUENCER_LOOP_SIZE; i++) {
		aw8697_i2c_read(aw8697, AW8697_REG_WAVLOOP1 + i, &reg_val);
		aw8697->loop[i * 2 + 0] = (reg_val >> 4) & 0x0F;
		aw8697->loop[i * 2 + 1] = (reg_val >> 0) & 0x0F;

		count += snprintf(buf + count, PAGE_SIZE - count,
				  "seq%d loop: 0x%02x\n", i * 2 + 1,
				  aw8697->loop[i * 2 + 0]);
		count +=
		    snprintf(buf + count, PAGE_SIZE - count,
			     "seq%d loop: 0x%02x\n", i * 2 + 2,
			     aw8697->loop[i * 2 + 1]);
	}
	return count;
}

static ssize_t aw8697_loop_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned int databuf[2] = { 0, 0 };

	if (2 == sscanf(buf, "%x %x", &databuf[0], &databuf[1])) {
		pr_debug("%s: seq%d loop=0x%x\n", __FUNCTION__, databuf[0],
			 databuf[1]);
		mutex_lock(&aw8697->lock);
		aw8697->loop[databuf[0]] = (unsigned char)databuf[1];
		aw8697_haptic_set_wav_loop(aw8697, (unsigned char)databuf[0],
					   aw8697->loop[databuf[0]]);
		mutex_unlock(&aw8697->lock);
	}

	return count;
}

static ssize_t aw8697_rtp_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	ssize_t len = 0;
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "rtp play: %d\n",
		     aw8697->rtp_cnt);

	return len;
}

static ssize_t aw8697_rtp_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	aw8697_haptic_stop(aw8697);
	aw8697_haptic_set_rtp_aei(aw8697, false);
	aw8697_interrupt_clear(aw8697);
	if (val < (sizeof(aw8697_rtp_name) / AW8697_RTP_NAME_MAX)) {
		aw8697->rtp_file_num = val;
		if (val) {
			//schedule_work(&aw8697->rtp_work);
			queue_work(aw8697->work_queue, &aw8697->rtp_work);
		}
	} else {
		pr_err("%s: rtp_file_num 0x%02x over max value \n", __func__,
		       aw8697->rtp_file_num);
	}

	return count;
}

static ssize_t aw8697_ram_update_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	//struct aw8697 *aw8697 = dev_get_drvdata(dev);
	ssize_t len = 0;
	len += snprintf(buf + len, PAGE_SIZE - len, "sram update mode\n");
	return len;
}

static ssize_t aw8697_ram_update_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val) {
		aw8697_ram_update(aw8697);
	}
	return count;
}

static ssize_t aw8697_f0_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	ssize_t len = 0;
	unsigned int f0_cnt_time = 0;

	mutex_lock(&aw8697->lock);
	aw8697->f0_cali_flag = AW8697_HAPTIC_LRA_F0;
	aw8697_haptic_get_f0(aw8697);
        f0_cnt_time = 1;
	if((f0_cnt_time == 1) && (aw8697->f0 == 0)){
		aw8697_haptic_get_f0(aw8697);
		f0_cnt_time = 0;
	}
	mutex_unlock(&aw8697->lock);
	len +=
	    //snprintf(buf + len, PAGE_SIZE - len, "aw8697 lra f0 = %d\n",
	    snprintf(buf + len, PAGE_SIZE - len, "%d\n", aw8697->f0);
	return len;
}

static ssize_t aw8697_f0_store(struct device *dev,
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

static ssize_t aw8697_cali_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	ssize_t len = 0;
	mutex_lock(&aw8697->lock);
	aw8697->f0_cali_flag = AW8697_HAPTIC_CALI_F0;
	aw8697_haptic_get_f0(aw8697);
	mutex_unlock(&aw8697->lock);
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "aw8697 cali f0 = %d\n",
		     aw8697->f0);
	return len;
}

static ssize_t aw8697_cali_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val) {
		mutex_lock(&aw8697->lock);
		aw8697_haptic_f0_calibration(aw8697);
		mutex_unlock(&aw8697->lock);
	}
	return count;
}

static ssize_t aw8697_cont_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	ssize_t len = 0;
	aw8697_haptic_read_cont_f0(aw8697);
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "aw8697 cont f0 = %d\n",
		     aw8697->cont_f0);
	return len;
}

static ssize_t aw8697_cont_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	aw8697_haptic_stop(aw8697);
	if (val) {
		aw8697_haptic_cont(aw8697);
	}
	return count;
}

static ssize_t aw8697_cont_td_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	ssize_t len = 0;
	len +=
	    snprintf(buf + len, PAGE_SIZE - len,
		     "aw8697 cont delay time = 0x%04x\n", aw8697->info.cont_td);
	return len;
}

static ssize_t aw8697_cont_td_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned int databuf[1] = { 0 };
	if (1 == sscanf(buf, "%x", &databuf[0])) {
		aw8697->info.cont_td = databuf[0];
		aw8697_i2c_write(aw8697, AW8697_REG_TD_H,
				 (unsigned char)(databuf[0] >> 8));
		aw8697_i2c_write(aw8697, AW8697_REG_TD_L,
				 (unsigned char)(databuf[0] >> 0));
	}
	return count;
}

static ssize_t aw8697_cont_drv_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	ssize_t len = 0;
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "aw8697 cont drv level = %d\n",
		     aw8697->info.cont_drv_lvl);
	len +=
	    snprintf(buf + len, PAGE_SIZE - len,
		     "aw8697 cont drv level overdrive= %d\n",
		     aw8697->info.cont_drv_lvl_ov);
	return len;
}

static ssize_t aw8697_cont_drv_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned int databuf[2] = { 0, 0 };
	if (2 == sscanf(buf, "%d %d", &databuf[0], &databuf[1])) {
		aw8697->info.cont_drv_lvl = databuf[0];
		aw8697_i2c_write(aw8697, AW8697_REG_DRV_LVL,
				 aw8697->info.cont_drv_lvl);
		aw8697->info.cont_drv_lvl_ov = databuf[1];
		aw8697_i2c_write(aw8697, AW8697_REG_DRV_LVL_OV,
				 aw8697->info.cont_drv_lvl_ov);
	}
	return count;
}

//return buffer size and availbe size
static ssize_t aw8697_custom_wave_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	ssize_t len = 0;
	len +=
		snprintf(buf + len, PAGE_SIZE - len, "period_size=%d;",
		aw8697->ram.base_addr >> 2);
	len +=
		snprintf(buf + len, PAGE_SIZE - len,
		"max_size=%d;free_size=%d;",
		get_rb_max_size(), get_rb_free_size());
	len +=
		snprintf(buf + len, PAGE_SIZE - len,
		"custom_wave_id=%d;", CUSTOME_WAVE_ID);
	return len;
}

static ssize_t aw8697_custom_wave_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned long  buf_len, period_size, offset;
	int ret;
	period_size = (aw8697->ram.base_addr >> 2);
	offset = 0;
	pr_debug(" write szie %d, period size %d", count, period_size);
	if (count % period_size || count < period_size)
		rb_end();
	atomic_set(&aw8697->is_in_write_loop, 1);

	while (count > 0) {
		buf_len = min(count, period_size);
		ret = write_rb(buf + offset,  buf_len);
		if (ret < 0)
			goto exit;
		count -= buf_len;
		offset += buf_len;
	}
	ret = offset;
exit:
	atomic_set(&aw8697->is_in_write_loop, 0);
	wake_up_interruptible(&aw8697->stop_wait_q);
	pr_debug(" return size %d", ret);
	return ret;
}

static ssize_t aw8697_cont_num_brk_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	ssize_t len = 0;
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "aw8697 cont break num = %d\n",
		     aw8697->info.cont_num_brk);
	return len;
}

static ssize_t aw8697_cont_num_brk_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned int databuf[1] = { 0 };
	if (1 == sscanf(buf, "%d", &databuf[0])) {
		aw8697->info.cont_num_brk = databuf[0];
		if (aw8697->info.cont_num_brk > 7) {
			aw8697->info.cont_num_brk = 7;
		}
		aw8697_i2c_write_bits(aw8697, AW8697_REG_BEMF_NUM,
				      AW8697_BIT_BEMF_NUM_BRK_MASK,
				      aw8697->info.cont_num_brk);
	}
	return count;
}

static ssize_t aw8697_cont_zc_thr_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	ssize_t len = 0;
	len +=
	    snprintf(buf + len, PAGE_SIZE - len,
		     "aw8697 cont zero cross thr = 0x%04x\n",
		     aw8697->info.cont_zc_thr);
	return len;
}

static ssize_t aw8697_cont_zc_thr_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned int databuf[1] = { 0 };
	if (1 == sscanf(buf, "%x", &databuf[0])) {
		aw8697->info.cont_zc_thr = databuf[0];
		aw8697_i2c_write(aw8697, AW8697_REG_ZC_THRSH_H,
				 (unsigned char)(databuf[0] >> 8));
		aw8697_i2c_write(aw8697, AW8697_REG_ZC_THRSH_L,
				 (unsigned char)(databuf[0] >> 0));
	}
	return count;
}

static ssize_t aw8697_vbat_monitor_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	ssize_t len = 0;

	mutex_lock(&aw8697->lock);
	aw8697_haptic_stop(aw8697);
	aw8697_haptic_get_vbat(aw8697);
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "vbat=%dmV\n", aw8697->vbat);
	mutex_unlock(&aw8697->lock);

	return len;
}

static ssize_t aw8697_vbat_monitor_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	return count;
}

static ssize_t aw8697_lra_resistance_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	ssize_t len = 0;
	unsigned char reg_val = 0;

	mutex_lock(&aw8697->lock);
	aw8697_haptic_stop(aw8697);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSCTRL,
			      AW8697_BIT_SYSCTRL_RAMINIT_MASK,
			      AW8697_BIT_SYSCTRL_RAMINIT_EN);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSCTRL,
			      AW8697_BIT_SYSCTRL_BST_MODE_MASK,
			      AW8697_BIT_SYSCTRL_BST_MODE_BYPASS);

	aw8697_i2c_write_bits(aw8697, AW8697_REG_ANACTRL,
			      AW8697_BIT_ANACTRL_HD_PD_MASK,
			      AW8697_BIT_ANACTRL_HD_HZ_EN);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_D2SCFG,
			      AW8697_BIT_D2SCFG_CLK_ADC_MASK,
			      AW8697_BIT_D2SCFG_CLK_ASC_1P5MHZ);

	aw8697_i2c_write_bits(aw8697, AW8697_REG_DETCTRL,
			      AW8697_BIT_DETCTRL_RL_OS_MASK,
			      AW8697_BIT_DETCTRL_RL_DETECT);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_DETCTRL,
			      AW8697_BIT_DETCTRL_DIAG_GO_MASK,
			      AW8697_BIT_DETCTRL_DIAG_GO_ENABLE);
	msleep(3);
	aw8697_i2c_read(aw8697, AW8697_REG_RLDET, &reg_val);
	aw8697->lra = 298 * reg_val;
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "%d\n", aw8697->lra);
	    //snprintf(buf + len, PAGE_SIZE - len, "r_lra=%dmohm\n", aw8697->lra);

	aw8697_i2c_write_bits(aw8697, AW8697_REG_ANACTRL,
			      AW8697_BIT_ANACTRL_HD_PD_MASK,
			      AW8697_BIT_ANACTRL_HD_PD_EN);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_D2SCFG,
			      AW8697_BIT_D2SCFG_CLK_ADC_MASK,
			      AW8697_BIT_D2SCFG_CLK_ASC_6MHZ);

	aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSCTRL,
			      AW8697_BIT_SYSCTRL_RAMINIT_MASK,
			      AW8697_BIT_SYSCTRL_RAMINIT_OFF);
	aw8697_i2c_write_bits(aw8697, AW8697_REG_SYSCTRL,
			      AW8697_BIT_SYSCTRL_BST_MODE_MASK,
			      AW8697_BIT_SYSCTRL_BST_MODE_BOOST);
	mutex_unlock(&aw8697->lock);

	return len;
}

static ssize_t aw8697_lra_resistance_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	return count;
}

static ssize_t aw8697_auto_boost_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	ssize_t len = 0;

	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "auto_boost=%d\n",
		     aw8697->auto_boost);

	return len;
}

static ssize_t aw8697_auto_boost_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw8697->lock);
	aw8697_haptic_stop(aw8697);
	aw8697_haptic_auto_boost_config(aw8697, val);
	mutex_unlock(&aw8697->lock);

	return count;
}

static ssize_t aw8697_prctmode_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	ssize_t len = 0;
	unsigned char reg_val = 0;

	aw8697_i2c_read(aw8697, AW8697_REG_RLDET, &reg_val);

	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "prctmode=%d\n",
		     reg_val & 0x20);
	return len;
}

static ssize_t aw8697_prctmode_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned int databuf[2] = { 0, 0 };
	unsigned int addr = 0;
	unsigned int val = 0;
	if (2 == sscanf(buf, "%x %x", &databuf[0], &databuf[1])) {
		addr = databuf[0];
		val = databuf[1];
		mutex_lock(&aw8697->lock);
		aw8697_haptic_swicth_motorprotect_config(aw8697, addr, val);
		mutex_unlock(&aw8697->lock);
	}
	return count;
}

static ssize_t aw8697_trig_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	ssize_t len = 0;
	unsigned char i = 0;
	for (i = 0; i < AW8697_TRIG_NUM; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"trig%d: enable=%d, default_level=%d, dual_edge=%d, frist_seq=%d, second_seq=%d\n",
				i + 1, aw8697->trig[i].enable,
				aw8697->trig[i].default_level,
				aw8697->trig[i].dual_edge,
				aw8697->trig[i].frist_seq,
				aw8697->trig[i].second_seq);
	}

	return len;
}

static ssize_t aw8697_trig_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned int databuf[6] = { 0 };
	if (sscanf(buf, "%d %d %d %d %d %d",
		   &databuf[0], &databuf[1], &databuf[2], &databuf[3],
		   &databuf[4], &databuf[5])) {
		pr_debug("%s: %d, %d, %d, %d, %d, %d\n", __func__, databuf[0],
			 databuf[1], databuf[2], databuf[3], databuf[4],
			 databuf[5]);
		if (databuf[0] > 3) {
			databuf[0] = 3;
		}
		if (databuf[0] > 0) {
			databuf[0] -= 1;
		}
		aw8697->trig[databuf[0]].enable = databuf[1];
		aw8697->trig[databuf[0]].default_level = databuf[2];
		aw8697->trig[databuf[0]].dual_edge = databuf[3];
		aw8697->trig[databuf[0]].frist_seq = databuf[4];
		aw8697->trig[databuf[0]].second_seq = databuf[5];
		mutex_lock(&aw8697->lock);
		aw8697_haptic_trig_param_config(aw8697);
		aw8697_haptic_trig_enable_config(aw8697);
		mutex_unlock(&aw8697->lock);
	}
	return count;
}

static ssize_t aw8697_ram_vbat_comp_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	ssize_t len = 0;

	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "ram_vbat_comp=%d\n",
		     aw8697->ram_vbat_comp);

	return len;
}

static ssize_t aw8697_ram_vbat_comp_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw8697->lock);
	if (val) {
		aw8697->ram_vbat_comp = AW8697_HAPTIC_RAM_VBAT_COMP_ENABLE;
	} else {
		aw8697->ram_vbat_comp = AW8697_HAPTIC_RAM_VBAT_COMP_DISABLE;
	}
	mutex_unlock(&aw8697->lock);

	return count;
}

static ssize_t aw8697_osc_cali_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	ssize_t len = 0;

	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "%d\n",
		     aw8697->lra_calib_data);

	return len;
}

static ssize_t aw8697_osc_cali_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned int val = 0;

	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw8697->lock);
	/*osc calibration flag start,Other behaviors are forbidden*/
	aw8697->osc_cali_run = 1;
	if (val == 3) {
		aw8697_rtp_osc_calibration(aw8697);
		aw8697_rtp_trim_lra_calibration(aw8697);
	}
	if (val == 1)
		aw8697_rtp_osc_calibration(aw8697);

	aw8697->osc_cali_run = 0;
	/*osc calibration flag end,Other behaviors are permitted*/
	mutex_unlock(&aw8697->lock);

	return count;
}

static ssize_t aw8697_osc_save_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	pr_info("%s enter\n", __func__);
	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw8697->lra_calib_data = val;
	pr_info("%s load osa cal: %d\n", __func__, val);

	return count;
}

static ssize_t aw8697_f0_save_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	ssize_t len = 0;

	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "%d\n",
		     aw8697->f0_calib_data);

	return len;
}

static ssize_t aw8697_f0_save_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	pr_info("%s enter\n", __func__);
	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw8697->f0_calib_data = val;
	pr_info("%s load f0 cal: %d\n", __func__, val);

	return count;
}

static ssize_t aw8697_f0_value_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", aw8697->f0);
}

static ssize_t aw8697_f0_check_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct aw8697 *aw8697 = dev_get_drvdata(dev);
	ssize_t len = 0;

	if (aw8697->f0_cali_status == true)
		len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", 1);
	if (aw8697->f0_cali_status == false)
		len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", 0);

	return len;
}

static DEVICE_ATTR(activate_test, S_IWUSR | S_IRUGO, aw8697_activate_test_show,
		   aw8697_activate_test_store);

static DEVICE_ATTR(reg, S_IWUSR | S_IRUGO, aw8697_i2c_reg_show,
		   aw8697_i2c_reg_store);
static DEVICE_ATTR(ram, S_IWUSR | S_IRUGO, aw8697_i2c_ram_show,
		   aw8697_i2c_ram_store);
static DEVICE_ATTR(duration, S_IWUSR | S_IRUGO, aw8697_duration_show,
		   aw8697_duration_store);
static DEVICE_ATTR(activate, S_IWUSR | S_IRUGO, aw8697_activate_show,
		   aw8697_activate_store);
static DEVICE_ATTR(activate_mode, S_IWUSR | S_IRUGO, aw8697_activate_mode_show,
		   aw8697_activate_mode_store);
static DEVICE_ATTR(index, S_IWUSR | S_IRUGO, aw8697_index_show,
		   aw8697_index_store);
static DEVICE_ATTR(vmax, S_IWUSR | S_IRUGO, aw8697_vmax_show,
		   aw8697_vmax_store);
static DEVICE_ATTR(gain, S_IWUSR | S_IRUGO, aw8697_gain_show,
		   aw8697_gain_store);
static DEVICE_ATTR(seq, S_IWUSR | S_IRUGO, aw8697_seq_show, aw8697_seq_store);
static DEVICE_ATTR(loop, S_IWUSR | S_IRUGO, aw8697_loop_show,
		   aw8697_loop_store);
static DEVICE_ATTR(rtp, S_IWUSR | S_IRUGO, aw8697_rtp_show, aw8697_rtp_store);
static DEVICE_ATTR(ram_update, S_IWUSR | S_IRUGO, aw8697_ram_update_show,
		   aw8697_ram_update_store);
static DEVICE_ATTR(f0, S_IWUSR | S_IRUGO, aw8697_f0_show, aw8697_f0_store);
static DEVICE_ATTR(cali, S_IWUSR | S_IRUGO, aw8697_cali_show,
		   aw8697_cali_store);
static DEVICE_ATTR(cont, S_IWUSR | S_IRUGO, aw8697_cont_show,
		   aw8697_cont_store);
static DEVICE_ATTR(cont_td, S_IWUSR | S_IRUGO, aw8697_cont_td_show,
		   aw8697_cont_td_store);
static DEVICE_ATTR(cont_drv, S_IWUSR | S_IRUGO, aw8697_cont_drv_show,
		   aw8697_cont_drv_store);
static DEVICE_ATTR(cont_num_brk, S_IWUSR | S_IRUGO, aw8697_cont_num_brk_show,
		   aw8697_cont_num_brk_store);
static DEVICE_ATTR(cont_zc_thr, S_IWUSR | S_IRUGO, aw8697_cont_zc_thr_show,
		   aw8697_cont_zc_thr_store);
static DEVICE_ATTR(vbat_monitor, S_IWUSR | S_IRUGO, aw8697_vbat_monitor_show,
		   aw8697_vbat_monitor_store);
static DEVICE_ATTR(lra_resistance, S_IWUSR | S_IRUGO,
		   aw8697_lra_resistance_show, aw8697_lra_resistance_store);
static DEVICE_ATTR(auto_boost, S_IWUSR | S_IRUGO, aw8697_auto_boost_show,
		   aw8697_auto_boost_store);
static DEVICE_ATTR(prctmode, S_IWUSR | S_IRUGO, aw8697_prctmode_show,
		   aw8697_prctmode_store);
static DEVICE_ATTR(trig, S_IWUSR | S_IRUGO, aw8697_trig_show,
		   aw8697_trig_store);
static DEVICE_ATTR(ram_vbat_comp, S_IWUSR | S_IRUGO, aw8697_ram_vbat_comp_show,
		   aw8697_ram_vbat_comp_store);
static DEVICE_ATTR(osc_cali, S_IWUSR | S_IRUGO, aw8697_osc_cali_show, aw8697_osc_cali_store);
static DEVICE_ATTR(f0_check, S_IWUSR | S_IRUGO, aw8697_f0_check_show, NULL);
static DEVICE_ATTR(f0_save, S_IWUSR | S_IRUGO, aw8697_f0_save_show, aw8697_f0_save_store);
static DEVICE_ATTR(osc_save, S_IWUSR | S_IRUGO, aw8697_osc_cali_show, aw8697_osc_save_store);
static DEVICE_ATTR(f0_value, S_IRUGO, aw8697_f0_value_show, NULL);
static DEVICE_ATTR(custom_wave, S_IWUSR | S_IRUGO, aw8697_custom_wave_show,
		   aw8697_custom_wave_store);

static struct attribute *aw8697_vibrator_attributes[] = {
	&dev_attr_reg.attr,
	&dev_attr_ram.attr,
	&dev_attr_activate_test.attr,
	&dev_attr_duration.attr,
	&dev_attr_activate.attr,
	&dev_attr_activate_mode.attr,
	&dev_attr_index.attr,
	&dev_attr_vmax.attr,
	&dev_attr_gain.attr,
	&dev_attr_seq.attr,
	&dev_attr_loop.attr,
	&dev_attr_rtp.attr,
	&dev_attr_ram_update.attr,
	&dev_attr_f0.attr,
	&dev_attr_cali.attr,
	&dev_attr_cont.attr,
	&dev_attr_cont_td.attr,
	&dev_attr_cont_drv.attr,
	&dev_attr_cont_num_brk.attr,
	&dev_attr_cont_zc_thr.attr,
	&dev_attr_vbat_monitor.attr,
	&dev_attr_lra_resistance.attr,
	&dev_attr_auto_boost.attr,
	&dev_attr_prctmode.attr,
	&dev_attr_trig.attr,
	&dev_attr_ram_vbat_comp.attr,
	&dev_attr_osc_cali.attr,
	&dev_attr_f0_check.attr,
	&dev_attr_osc_save.attr,
	&dev_attr_f0_save.attr,
        &dev_attr_f0_value.attr,
	&dev_attr_custom_wave.attr,
	NULL
};

static struct attribute_group aw8697_vibrator_attribute_group = {
	.attrs = aw8697_vibrator_attributes
};

/******************************************************
 *
 * i2c driver
 *
 ******************************************************/
static int aw8697_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct aw8697 *aw8697;
	struct input_dev *input_dev;
	int rc = 0, effect_count_max;
	struct ff_device *ff;
	struct device_node *np = i2c->dev.of_node;
	int irq_flags = 0;
	int ret = -1;
#ifdef ENABLE_PIN_CONTROL
	int i;
#endif

	pr_info("%s enter\n", __func__);
	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "check_functionality failed\n");
		return -EIO;
	}

	aw8697 = devm_kzalloc(&i2c->dev, sizeof(struct aw8697), GFP_KERNEL);
	if (aw8697 == NULL)
		return -ENOMEM;
	input_dev = devm_input_allocate_device(&i2c->dev);
	if (!input_dev)
		return -ENOMEM;
	aw8697->dev = &i2c->dev;
	aw8697->i2c = i2c;
	device_init_wakeup(aw8697->dev, true);
	i2c_set_clientdata(i2c, aw8697);

	/* aw8697 rst & int */
	if (np) {
		ret = aw8697_parse_dt(&i2c->dev, aw8697, np);
		printk("%s ---%d \n", __func__, __LINE__);
		if (ret) {
			dev_err(&i2c->dev,
				"%s: failed to parse device tree node\n",
				__func__);
			goto err_parse_dt;
		}
	} else {
		aw8697->reset_gpio = -1;
		aw8697->irq_gpio = -1;
	}
	aw8697->enable_pin_control = 0;
#ifdef ENABLE_PIN_CONTROL
	aw8697->aw8697_pinctrl = devm_pinctrl_get(&i2c->dev);
	if (IS_ERR(aw8697->aw8697_pinctrl)) {
		if (PTR_ERR(aw8697->aw8697_pinctrl) == -EPROBE_DEFER) {
			printk("pinctrl not ready\n");
			rc = -EPROBE_DEFER;
			return rc;
		}
		printk("Target does not use pinctrl\n");
		aw8697->aw8697_pinctrl = NULL;
		rc = -EINVAL;
		return rc;
	}
	for (i = 0; i < ARRAY_SIZE(aw8697->pinctrl_state); i++) {
		const char *n = pctl_names[i];
		struct pinctrl_state *state =
			pinctrl_lookup_state(aw8697->aw8697_pinctrl, n);
		if (IS_ERR(state)) {
			printk("cannot find '%s'\n", n);
			rc = -EINVAL;
			//goto exit;
		}
		printk("found pin control %s\n", n);
		aw8697->pinctrl_state[i] = state;
		aw8697->enable_pin_control = 1;
		aw8697_set_interrupt(aw8697);
	}
#endif
	printk("%s ---%d \n", __func__, __LINE__);
	if (!aw8697->enable_pin_control) {
		if (gpio_is_valid(aw8697->reset_gpio)) {
			ret = devm_gpio_request_one(&i2c->dev, aw8697->reset_gpio,
					    GPIOF_OUT_INIT_LOW, "aw8697_rst");
			if (ret) {
				dev_err(&i2c->dev, "%s: rst request failed\n",
					__func__);
				goto err_reset_gpio_request;
			}
		}
	}

	if (gpio_is_valid(aw8697->irq_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, aw8697->irq_gpio,
					    GPIOF_DIR_IN, "aw8697_int");
		if (ret) {
			dev_err(&i2c->dev, "%s: int request failed\n",
				__func__);
			goto err_irq_gpio_request;
		}
	}

	ret = aw8697_read_chipid(aw8697);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: aw8697_read_chipid failed ret=%d\n",
			__func__, ret);
		goto err_id;
	}

	if (gpio_is_valid(aw8697->irq_gpio) &&
	    !(aw8697->flags & AW8697_FLAG_SKIP_INTERRUPTS)) {
		/* register irq handler */
		aw8697_interrupt_setup(aw8697);
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
		ret = devm_request_threaded_irq(&i2c->dev,
						gpio_to_irq(aw8697->irq_gpio),
						NULL, aw8697_irq, irq_flags,
						"aw8697", aw8697);
		if (ret != 0) {
			dev_err(&i2c->dev, "%s: failed to request IRQ %d: %d\n",
				__func__, gpio_to_irq(aw8697->irq_gpio), ret);
			goto err_irq;
		}
	} else {
		dev_info(&i2c->dev, "%s skipping IRQ registration\n", __func__);
		/* disable feature support if gpio was invalid */
		aw8697->flags |= AW8697_FLAG_SKIP_INTERRUPTS;
	}

	hrtimer_init(&aw8697->stop_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw8697->stop_timer.function = qti_hap_stop_timer;
	hrtimer_init(&aw8697->hap_disable_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	aw8697->hap_disable_timer.function = qti_hap_disable_timer;

	input_dev->name = "aw8697_haptic";	//  "qti-haptics";
	input_set_drvdata(input_dev, aw8697);
	aw8697->input_dev = input_dev;
	input_set_capability(input_dev, EV_FF, FF_CONSTANT);
	input_set_capability(input_dev, EV_FF, FF_GAIN);
	if (aw8697->effects_count != 0) {
		input_set_capability(input_dev, EV_FF, FF_PERIODIC);
		input_set_capability(input_dev, EV_FF, FF_CUSTOM);
	}

	if (aw8697->effects_count + 1 > FF_EFFECT_COUNT_MAX)
		effect_count_max = aw8697->effects_count + 1;
	else
		effect_count_max = FF_EFFECT_COUNT_MAX;
	rc = input_ff_create(input_dev, effect_count_max);
	if (rc < 0) {
		dev_err(aw8697->dev, "create FF input device failed, rc=%d\n",
			rc);
		return rc;
	}
	aw8697->work_queue = create_singlethread_workqueue("aw8976_vibrator_work_queue");
	if (!aw8697->work_queue) {
		dev_err(&i2c->dev, "%s: Error creating aw8976_vibrator_work_queue\n",
			__func__);
		goto err_sysfs;
	}
	INIT_WORK(&aw8697->set_gain_work, set_gain);
	aw8697_vibrator_init(aw8697);
	aw8697_haptic_init(aw8697);
	aw8697_ram_init(aw8697);

	ff = input_dev->ff;
	ff->upload = aw8697_haptics_upload_effect;
	ff->playback = aw8697_haptics_playback;
	ff->erase = aw8697_haptics_erase;
	ff->set_gain = aw8697_haptics_set_gain;
	rc = input_register_device(input_dev);
	if (rc < 0) {
		dev_err(aw8697->dev, "register input device failed, rc=%d\n",
			rc);
		goto destroy_ff;
	}

	dev_set_drvdata(&i2c->dev, aw8697);
	ret =
	    sysfs_create_group(&i2c->dev.kobj,
			       &aw8697_vibrator_attribute_group);
	if (ret < 0) {
		dev_info(&i2c->dev, "%s error creating sysfs attr files\n",
			 __func__);
		goto err_sysfs;
	}

	g_aw8697 = aw8697;

	ret =  create_rb();
	if (ret < 0) {
		dev_info(&i2c->dev, "%s error creating ringbuffer\n",
			 __func__);
		goto err_rb;
	}

	CUSTOME_WAVE_ID = aw8697->info.effect_max;

	pr_info("%s probe completed successfully!\n", __func__);

	return 0;

	err_rb:
	sysfs_remove_group(&i2c->dev.kobj, &aw8697_vibrator_attribute_group);
      err_sysfs:
	devm_free_irq(&i2c->dev, gpio_to_irq(aw8697->irq_gpio), aw8697);
      destroy_ff:
	input_ff_destroy(aw8697->input_dev);
      err_irq:
      err_id:
	if (gpio_is_valid(aw8697->irq_gpio))
		devm_gpio_free(&i2c->dev, aw8697->irq_gpio);
      err_irq_gpio_request:
	if (gpio_is_valid(aw8697->reset_gpio))
		devm_gpio_free(&i2c->dev, aw8697->reset_gpio);
      err_reset_gpio_request:
      err_parse_dt:
	device_init_wakeup(aw8697->dev, false);
	devm_kfree(&i2c->dev, aw8697);
	aw8697 = NULL;
	return ret;
}

static int aw8697_i2c_remove(struct i2c_client *i2c)
{
	struct aw8697 *aw8697 = i2c_get_clientdata(i2c);

	pr_info("%s enter\n", __func__);

	sysfs_remove_group(&i2c->dev.kobj, &aw8697_vibrator_attribute_group);

	devm_free_irq(&i2c->dev, gpio_to_irq(aw8697->irq_gpio), aw8697);

	if (gpio_is_valid(aw8697->irq_gpio))
		devm_gpio_free(&i2c->dev, aw8697->irq_gpio);
	if (gpio_is_valid(aw8697->reset_gpio))
		devm_gpio_free(&i2c->dev, aw8697->reset_gpio);
	if (aw8697 != NULL) {
		flush_workqueue(aw8697->work_queue);
		destroy_workqueue(aw8697->work_queue);
	}
	device_init_wakeup(aw8697->dev, false);
	release_rb();
	devm_kfree(&i2c->dev, aw8697);
	aw8697 = NULL;

	return 0;
}

static const struct i2c_device_id aw8697_i2c_id[] = {
	{AW8697_I2C_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, aw8697_i2c_id);

static struct of_device_id aw8697_dt_match[] = {
	{.compatible = "awinic,aw8697_haptic"},
	{},
};

static struct i2c_driver aw8697_i2c_driver = {
	.driver = {
		   .name = AW8697_I2C_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(aw8697_dt_match),
		   },
	.probe = aw8697_i2c_probe,
	.remove = aw8697_i2c_remove,
	.id_table = aw8697_i2c_id,
};

static int __init aw8697_i2c_init(void)
{
	int ret = 0;

	pr_info("aw8697 driver version %s\n", AW8697_VERSION);

	ret = i2c_add_driver(&aw8697_i2c_driver);
	if (ret) {
		pr_err("fail to add aw8697 device into i2c\n");
		return ret;
	}
    printk("aw8697_i2c_init success");
	return 0;
}

module_init(aw8697_i2c_init);

static void __exit aw8697_i2c_exit(void)
{
	i2c_del_driver(&aw8697_i2c_driver);
}

module_exit(aw8697_i2c_exit);

MODULE_DESCRIPTION("AW8697 Haptic Driver");
MODULE_LICENSE("GPL v2");
