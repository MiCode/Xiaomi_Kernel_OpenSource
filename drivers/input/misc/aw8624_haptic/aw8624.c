/*
 * aw8624.c   aw8624 haptic module
 *
 * Version: v1.0.0
 *
 * Copyright (c) 2019 AWINIC Technology CO., LTD
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 *  Author: Joseph <zhangzetao@awinic.com.cn>
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
#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include <linux/syscalls.h>
#include <linux/power_supply.h>
#include <linux/pm_qos.h>
#include "aw8624_reg.h"
#include "aw8624.h"

/******************************************************
 *
 * Marco
 *
 ******************************************************/
#define AW8624_I2C_NAME "aw8624_haptic"
#define AW8624_HAPTIC_NAME "aw8624_haptic"

#define AW8624_VERSION "v1.0.0"

#define AWINIC_RAM_UPDATE_DELAY

#define AW_I2C_RETRIES 2
#define AW_I2C_RETRY_DELAY 2
#define AW_READ_CHIPID_RETRIES 5
#define AW_READ_CHIPID_RETRY_DELAY 2

#define AW8624_MAX_DSP_START_TRY_COUNT    10

#define AW8624_MAX_FIRMWARE_LOAD_CNT 20

#define OSC_CALIBRATION_T_LENGTH 5100000
#define PM_QOS_VALUE_VB 400

struct pm_qos_request pm_qos_req_vb;
/******************************************************
 *
 * variable
 *
 ******************************************************/
#define AW8624_RTP_NAME_MAX        64
static char *aw8624_ram_name = "aw8624_haptic.bin";

static char aw8624_rtp_name[][AW8624_RTP_NAME_MAX] = {
	{"osc_rtp_24K_5s.bin"},
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
	{"aw8624_rtp.bin"},	//99
	{"aw8624_rtp.bin"},	//100
	{"offline_countdown_RTP.bin"},
	{"scene_bomb_injury_RTP.bin"},
	{"scene_bomb_RTP.bin"},	//103
	{"door_open_RTP.bin"},
	{"aw8624_rtp.bin"},
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
	{"aw8624_rtp.bin"},
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
	{"aw8624_rtp.bin"},
	{"firearms_awm_RTP.bin"},	//129
	{"firearms_mini14_RTP.bin"},	//130
	{"firearms_vss_RTP.bin"},	//131
	{"firearms_qbz_RTP.bin"},	//132
	{"firearms_ump9_RTP.bin"},	//133
	{"firearms_dp28_RTP.bin"},	//134
	{"firearms_s1897_RTP.bin"},	//135
	{"aw8624_rtp.bin"},
	{"firearms_p18c_RTP.bin"},	//137
	{"aw8624_rtp.bin"},
	{"aw8624_rtp.bin"},
	{"aw8624_rtp.bin"},
	{"aw8624_rtp.bin"},	//141
	{"aw8624_rtp.bin"},
	{"aw8624_rtp.bin"},
	{"aw8624_rtp.bin"},
	{"aw8624_rtp.bin"},
	{"aw8624_rtp.bin"},
	{"aw8624_rtp.bin"},
	{"aw8624_rtp.bin"},
	{"aw8624_rtp.bin"},
	{"aw8624_rtp.bin"},
	{"aw8624_rtp.bin"},
	{"aw8624_rtp.bin"},
	{"aw8624_rtp.bin"},	//153
	{"Atlantis_RTP.bin"},
	{"DigitalUniverse_RTP.bin"},
	{"Reveries_RTP.bin"},
};

struct aw8624_container *aw8624_rtp;
struct aw8624 *g_aw8624;

/******************************************************
 *
 * functions
 *
 ******************************************************/
static void aw8624_interrupt_clear(struct aw8624 *aw8624);
static int aw8624_haptic_get_vbat(struct aw8624 *aw8624);
//static int aw8624_haptic_trig_enable_config(struct aw8624 *aw8624);

 /******************************************************
 *
 * aw8624 i2c write/read
 *
 ******************************************************/
static int aw8624_i2c_write(struct aw8624 *aw8624,
			    unsigned char reg_addr, unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret =
		    i2c_smbus_write_byte_data(aw8624->i2c, reg_addr, reg_data);
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

static int aw8624_i2c_read(struct aw8624 *aw8624,
			   unsigned char reg_addr, unsigned char *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_smbus_read_byte_data(aw8624->i2c, reg_addr);
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

static int aw8624_i2c_write_bits(struct aw8624 *aw8624,
				 unsigned char reg_addr, unsigned int mask,
				 unsigned char reg_data)
{
	unsigned char reg_val = 0;

	aw8624_i2c_read(aw8624, reg_addr, &reg_val);
	reg_val &= mask;
	reg_val |= reg_data;
	aw8624_i2c_write(aw8624, reg_addr, reg_val);

	return 0;
}

static int aw8624_i2c_writes(struct aw8624 *aw8624,
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

	ret = i2c_master_send(aw8624->i2c, data, len + 1);
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
static void aw8624_rtp_loaded(const struct firmware *cont, void *context)
{
	struct aw8624 *aw8624 = context;
	pr_info("%s: enter\n", __func__);

	if (!cont) {
		pr_err("%s: failed to read %s\n", __func__,
		       aw8624_rtp_name[aw8624->rtp_file_num]);
		release_firmware(cont);
		return;
	}

	pr_info("%s: loaded %s - size: %zu\n", __func__,
		aw8624_rtp_name[aw8624->rtp_file_num], cont ? cont->size : 0);

	/* aw8624 rtp update */
	aw8624_rtp = vmalloc(cont->size + sizeof(int));
	if (!aw8624_rtp) {
		release_firmware(cont);
		pr_err("%s: Error allocating memory\n", __func__);
		return;
	}
	aw8624_rtp->len = cont->size;
	pr_info("%s: rtp size = %d\n", __func__, aw8624_rtp->len);
	memcpy(aw8624_rtp->data, cont->data, cont->size);
	release_firmware(cont);

	aw8624->rtp_init = 1;
	pr_info("%s: rtp update complete\n", __func__);
}

static int aw8624_rtp_update(struct aw8624 *aw8624)
{
	pr_info("%s: enter\n", __func__);

	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				       aw8624_rtp_name[aw8624->rtp_file_num],
				       aw8624->dev, GFP_KERNEL, aw8624,
				       aw8624_rtp_loaded);
}

static void aw8624_container_update(struct aw8624 *aw8624,
				    struct aw8624_container *aw8624_cont)
{
	int i = 0;
	unsigned int shift = 0;

	pr_info("%s: enter\n", __func__);

	mutex_lock(&aw8624->lock);

	aw8624->ram.baseaddr_shift = 2;
	aw8624->ram.ram_shift = 4;

	/* RAMINIT Enable */
	aw8624_i2c_write_bits(aw8624, AW8624_REG_SYSCTRL,
			      AW8624_BIT_SYSCTRL_RAMINIT_MASK,
			      AW8624_BIT_SYSCTRL_RAMINIT_EN);

	/* base addr */
	shift = aw8624->ram.baseaddr_shift;
	aw8624->ram.base_addr =
	    (unsigned int)((aw8624_cont->data[0 + shift] << 8) |
			   (aw8624_cont->data[1 + shift]));
	pr_info("%s: base_addr=0x%4x\n", __func__, aw8624->ram.base_addr);

	aw8624_i2c_write(aw8624, AW8624_REG_BASE_ADDRH,
			 aw8624_cont->data[0 + shift]);
	aw8624_i2c_write(aw8624, AW8624_REG_BASE_ADDRL,
			 aw8624_cont->data[1 + shift]);

	aw8624_i2c_write(aw8624, AW8624_REG_FIFO_AEH,
			 (unsigned char)((aw8624->ram.base_addr >> 1) >> 8));
	aw8624_i2c_write(aw8624, AW8624_REG_FIFO_AEL,
			 (unsigned char)((aw8624->ram.base_addr >> 1) &
					 0x00FF));
	aw8624_i2c_write(aw8624, AW8624_REG_FIFO_AFH,
			 (unsigned
			  char)((aw8624->ram.base_addr -
				 (aw8624->ram.base_addr >> 2)) >> 8));
	aw8624_i2c_write(aw8624, AW8624_REG_FIFO_AFL,
			 (unsigned
			  char)((aw8624->ram.base_addr -
				 (aw8624->ram.base_addr >> 2)) & 0x00FF));

	/* ram */
	shift = aw8624->ram.baseaddr_shift;
	aw8624_i2c_write(aw8624, AW8624_REG_RAMADDRH,
			 aw8624_cont->data[0 + shift]);
	aw8624_i2c_write(aw8624, AW8624_REG_RAMADDRL,
			 aw8624_cont->data[1 + shift]);
	shift = aw8624->ram.ram_shift;
	for (i = shift; i < aw8624_cont->len; i++) {
		aw8624_i2c_write(aw8624, AW8624_REG_RAMDATA,
				 aw8624_cont->data[i]);
	}

	/* RAMINIT Disable */
	aw8624_i2c_write_bits(aw8624, AW8624_REG_SYSCTRL,
			      AW8624_BIT_SYSCTRL_RAMINIT_MASK,
			      AW8624_BIT_SYSCTRL_RAMINIT_OFF);

	mutex_unlock(&aw8624->lock);

	pr_info("%s: exit\n", __func__);
}

static void aw8624_ram_loaded(const struct firmware *cont, void *context)
{
	struct aw8624 *aw8624 = context;
	struct aw8624_container *aw8624_fw;
	int i = 0;
	unsigned short check_sum = 0;

	pr_info("%s: enter\n", __func__);

	if (!cont) {
		pr_err("%s: failed to read %s\n", __func__, aw8624_ram_name);
		release_firmware(cont);
		return;
	}

	pr_info("%s: loaded %s - size: %zu\n", __func__, aw8624_ram_name,
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
		aw8624->ram.check_sum = check_sum;
	}

	/* aw8624 ram update */
	aw8624_fw = kzalloc(cont->size + sizeof(int), GFP_KERNEL);
	if (!aw8624_fw) {
		release_firmware(cont);
		pr_err("%s: Error allocating memory\n", __func__);
		return;
	}
	aw8624_fw->len = cont->size;
	memcpy(aw8624_fw->data, cont->data, cont->size);
	release_firmware(cont);

	aw8624_container_update(aw8624, aw8624_fw);

	aw8624->ram.len = aw8624_fw->len;

	kfree(aw8624_fw);

	aw8624->ram_init = 1;
	pr_info("%s: fw update complete\n", __func__);

	//aw8624_haptic_trig_enable_config(aw8624);

	aw8624_rtp_update(aw8624);
}

static int aw8624_ram_update(struct aw8624 *aw8624)
{
	aw8624->ram_init = 0;
	aw8624->rtp_init = 0;
	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				       aw8624_ram_name, aw8624->dev, GFP_KERNEL,
				       aw8624, aw8624_ram_loaded);
}

#ifdef AWINIC_RAM_UPDATE_DELAY
static void aw8624_ram_work_routine(struct work_struct *work)
{
	struct aw8624 *aw8624 =
	    container_of(work, struct aw8624, ram_work.work);

	pr_info("%s: enter\n", __func__);

	aw8624_ram_update(aw8624);

}
#endif

static int aw8624_ram_init(struct aw8624 *aw8624)
{
#ifdef AWINIC_RAM_UPDATE_DELAY
	int ram_timer_val = 5000;
	INIT_DELAYED_WORK(&aw8624->ram_work, aw8624_ram_work_routine);
	//schedule_delayed_work(&aw8624->ram_work,
	//msecs_to_jiffies(ram_timer_val));
	queue_delayed_work(aw8624->work_queue, &aw8624->ram_work,
			   msecs_to_jiffies(ram_timer_val));
#else
	aw8624_ram_update(aw8624);
#endif
	return 0;
}

/*****************************************************
 *
 * haptic control
 *
 *****************************************************/
static int aw8624_haptic_softreset(struct aw8624 *aw8624)
{
	pr_debug("%s: enter\n", __func__);

	aw8624_i2c_write(aw8624, AW8624_REG_ID, 0xAA);
	msleep(1);
	return 0;
}

static int aw8624_haptic_active(struct aw8624 *aw8624)
{
	pr_debug("%s: enter\n", __func__);

	aw8624_i2c_write_bits(aw8624, AW8624_REG_SYSCTRL,
			      AW8624_BIT_SYSCTRL_WORK_MODE_MASK,
			      AW8624_BIT_SYSCTRL_ACTIVE);
	aw8624_interrupt_clear(aw8624);
	aw8624_i2c_write_bits(aw8624, AW8624_REG_SYSINTM,
			      AW8624_BIT_SYSINTM_UVLO_MASK,
			      AW8624_BIT_SYSINTM_UVLO_EN);
	return 0;
}

static int
aw8624_haptic_play_mode(struct aw8624 *aw8624, unsigned char play_mode)
{
	switch (play_mode) {
	case AW8624_HAPTIC_STANDBY_MODE:
		aw8624->play_mode = AW8624_HAPTIC_STANDBY_MODE;
		aw8624_i2c_write_bits(aw8624,
				      AW8624_REG_SYSCTRL,
				      AW8624_BIT_SYSCTRL_WORK_MODE_MASK,
				      AW8624_BIT_SYSCTRL_STANDBY);
		break;
	case AW8624_HAPTIC_RAM_MODE:
		aw8624->play_mode = AW8624_HAPTIC_RAM_MODE;
		aw8624_i2c_write_bits(aw8624,
				      AW8624_REG_SYSCTRL,
				      AW8624_BIT_SYSCTRL_PLAY_MODE_MASK,
				      AW8624_BIT_SYSCTRL_PLAY_MODE_RAM);
		aw8624_haptic_active(aw8624);
		break;
	case AW8624_HAPTIC_RAM_LOOP_MODE:
		aw8624->play_mode = AW8624_HAPTIC_RAM_LOOP_MODE;
		aw8624_i2c_write_bits(aw8624,
				      AW8624_REG_SYSCTRL,
				      AW8624_BIT_SYSCTRL_PLAY_MODE_MASK,
				      AW8624_BIT_SYSCTRL_PLAY_MODE_RAM);
		aw8624_haptic_active(aw8624);
		break;
	case AW8624_HAPTIC_RTP_MODE:
		aw8624->play_mode = AW8624_HAPTIC_RTP_MODE;
		aw8624_i2c_write_bits(aw8624,
				      AW8624_REG_SYSCTRL,
				      AW8624_BIT_SYSCTRL_PLAY_MODE_MASK,
				      AW8624_BIT_SYSCTRL_PLAY_MODE_RTP);
		aw8624_haptic_active(aw8624);
		break;
	case AW8624_HAPTIC_TRIG_MODE:
		aw8624->play_mode = AW8624_HAPTIC_TRIG_MODE;
		aw8624_i2c_write_bits(aw8624,
				      AW8624_REG_SYSCTRL,
				      AW8624_BIT_SYSCTRL_PLAY_MODE_MASK,
				      AW8624_BIT_SYSCTRL_PLAY_MODE_RAM);
		aw8624_haptic_active(aw8624);
		break;
	case AW8624_HAPTIC_CONT_MODE:
		aw8624->play_mode = AW8624_HAPTIC_CONT_MODE;
		aw8624_i2c_write_bits(aw8624,
				      AW8624_REG_SYSCTRL,
				      AW8624_BIT_SYSCTRL_PLAY_MODE_MASK,
				      AW8624_BIT_SYSCTRL_PLAY_MODE_CONT);
		aw8624_haptic_active(aw8624);
		break;
	default:
		dev_err(aw8624->dev, "%s: play mode %d err",
			__func__, play_mode);
		break;
	}
	return 0;
}

static int aw8624_haptic_play_go(struct aw8624 *aw8624, bool flag)
{
	pr_info("%s: enter\n", __func__);

	if (flag == true) {
		aw8624_i2c_write_bits(aw8624, AW8624_REG_GO,
				      AW8624_BIT_GO_MASK, AW8624_BIT_GO_ENABLE);
	} else {
		aw8624_i2c_write_bits(aw8624, AW8624_REG_GO,
				      AW8624_BIT_GO_MASK,
				      AW8624_BIT_GO_DISABLE);
	}
	return 0;
}

static int aw8624_haptic_stop_delay(struct aw8624 *aw8624)
{
	unsigned char reg_val = 0;
	unsigned int cnt = 100;

	while (cnt--) {
		aw8624_i2c_read(aw8624, AW8624_REG_GLB_STATE, &reg_val);
		if ((reg_val & 0x0f) == 0x00) {
			return 0;

			msleep(2);
		}
		pr_debug("%s: wait for standby, reg glb_state=0x%02x\n",
			 __func__, reg_val);
	}
	pr_err("%s: do not enter standby automatically\n", __func__);

	return 0;
}

static int aw8624_haptic_stop(struct aw8624 *aw8624)
{
	pr_debug("%s: enter\n", __func__);

	aw8624_haptic_play_go(aw8624, false);
	aw8624_haptic_stop_delay(aw8624);
	aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_STANDBY_MODE);

	return 0;
}

static int aw8624_haptic_start(struct aw8624 *aw8624)
{
	pr_debug("%s: enter\n", __func__);
	aw8624_haptic_active(aw8624);
	aw8624_haptic_play_go(aw8624, true);

	return 0;
}

static int aw8624_haptic_set_wav_seq(struct aw8624 *aw8624,
				     unsigned char wav, unsigned char seq)
{
	aw8624_i2c_write(aw8624, AW8624_REG_WAVSEQ1 + wav, seq);
	return 0;
}

static int aw8624_haptic_set_wav_loop(struct aw8624 *aw8624,
				      unsigned char wav, unsigned char loop)
{
	unsigned char tmp = 0;

	if (wav % 2) {
		tmp = loop << 0;
		aw8624_i2c_write_bits(aw8624, AW8624_REG_WAVLOOP1 + (wav / 2),
				      AW8624_BIT_WAVLOOP_SEQNP1_MASK, tmp);
	} else {
		tmp = loop << 4;
		aw8624_i2c_write_bits(aw8624, AW8624_REG_WAVLOOP1 + (wav / 2),
				      AW8624_BIT_WAVLOOP_SEQN_MASK, tmp);
	}

	return 0;
}

static int aw8624_haptic_set_repeat_wav_seq(struct aw8624 *aw8624,
					    unsigned char seq)
{
	aw8624_haptic_set_wav_seq(aw8624, 0x00, seq);
	aw8624_haptic_set_wav_loop(aw8624, 0x00,
				   AW8624_BIT_WAVLOOP_INIFINITELY);

	return 0;
}

static int aw8624_haptic_set_gain(struct aw8624 *aw8624, unsigned char gain)
{
	unsigned char comp_gain = 0;
	if (aw8624->ram_vbat_comp == AW8624_HAPTIC_RAM_VBAT_COMP_ENABLE)
	{
		aw8624_haptic_get_vbat(aw8624);
		pr_debug("%s: ref %d vbat %d ", __func__, AW8624_VBAT_REFER,
				aw8624->vbat);
		comp_gain = gain * AW8624_VBAT_REFER / aw8624->vbat;
		if (comp_gain > (128 * AW8624_VBAT_REFER / AW8624_VBAT_MIN)) {
			comp_gain = 128 * AW8624_VBAT_REFER / AW8624_VBAT_MIN;
			pr_debug("%s: comp gain limit is %d ", comp_gain);
		}
		pr_info("%s: enable vbat comp, level = %x comp level = %x", __func__,
			   gain, comp_gain);
		aw8624_i2c_write(aw8624, AW8624_REG_DATDBG, comp_gain);
	} else {
		pr_debug("%s: disable compsensation, vbat=%d, vbat_min=%d, vbat_ref=%d",
				__func__, aw8624->vbat, AW8624_VBAT_MIN, AW8624_VBAT_REFER);
		aw8624_i2c_write(aw8624, AW8624_REG_DATDBG, gain);
	}
	return 0;
}

static int aw8624_haptic_set_pwm(struct aw8624 *aw8624, unsigned char mode)
{
	switch (mode) {
	case AW8624_PWM_48K:
		aw8624_i2c_write_bits(aw8624, AW8624_REG_PWMDBG,
				      AW8624_BIT_PWMDBG_PWM_MODE_MASK,
				      AW8624_BIT_PWMDBG_PWM_48K);
		break;
	case AW8624_PWM_24K:
		aw8624_i2c_write_bits(aw8624, AW8624_REG_PWMDBG,
				      AW8624_BIT_PWMDBG_PWM_MODE_MASK,
				      AW8624_BIT_PWMDBG_PWM_24K);
		break;
	case AW8624_PWM_12K:
		aw8624_i2c_write_bits(aw8624, AW8624_REG_PWMDBG,
				      AW8624_BIT_PWMDBG_PWM_MODE_MASK,
				      AW8624_BIT_PWMDBG_PWM_12K);
		break;
	default:
		break;
	}
	return 0;
}

static int aw8624_haptic_play_repeat_seq(struct aw8624 *aw8624,
					 unsigned char flag)
{
	pr_debug("%s: enter\n", __func__);

	if (flag) {
		aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_RAM_LOOP_MODE);
		aw8624_haptic_start(aw8624);
	}

	return 0;
}

/*****************************************************
 *
 * motor protect
 *
 *****************************************************/
static int aw8624_haptic_swicth_motorprotect_config(struct aw8624 *aw8624,
						    unsigned char addr,
						    unsigned char val)
{
	if (addr == 1) {
		aw8624_i2c_write_bits(aw8624,
				      AW8624_REG_DETCTRL,
				      AW8624_BIT_DETCTRL_PROTECT_MASK,
				      AW8624_BIT_DETCTRL_PROTECT_SHUTDOWN);
		aw8624_i2c_write_bits(aw8624,
				      AW8624_REG_PWMPRC,
				      AW8624_BIT_PWMPRC_PRC_EN_MASK,
				      AW8624_BIT_PWMPRC_PRC_ENABLE);
		aw8624_i2c_write_bits(aw8624,
				      AW8624_REG_PRLVL,
				      AW8624_BIT_PRLVL_PR_EN_MASK,
				      AW8624_BIT_PRLVL_PR_ENABLE);
	} else if (addr == 0) {
		aw8624_i2c_write_bits(aw8624,
				      AW8624_REG_DETCTRL,
				      AW8624_BIT_DETCTRL_PROTECT_MASK,
				      AW8624_BIT_DETCTRL_PROTECT_NO_ACTION);
		aw8624_i2c_write_bits(aw8624,
				      AW8624_REG_PWMPRC,
				      AW8624_BIT_PWMPRC_PRC_EN_MASK,
				      AW8624_BIT_PWMPRC_PRC_DISABLE);
		aw8624_i2c_write_bits(aw8624,
				      AW8624_REG_PRLVL,
				      AW8624_BIT_PRLVL_PR_EN_MASK,
				      AW8624_BIT_PRLVL_PR_DISABLE);
	} else if (addr == 0x2d) {
		aw8624_i2c_write_bits(aw8624, AW8624_REG_PWMPRC,
				      AW8624_BIT_PWMPRC_PRCTIME_MASK, val);
	} else if (addr == 0x3e) {
		aw8624_i2c_write_bits(aw8624, AW8624_REG_PRLVL,
				      AW8624_BIT_PRLVL_PRLVL_MASK, val);
	} else if (addr == 0x3f) {
		aw8624_i2c_write_bits(aw8624, AW8624_REG_PRTIME,
				      AW8624_BIT_PRTIME_PRTIME_MASK, val);
	} else {
		/*nothing to do; */
	}

	return 0;
}

static int aw8624_haptic_cont_vbat_mode(struct aw8624 *aw8624,
					unsigned char flag)
{
	if (flag == AW8624_HAPTIC_CONT_VBAT_HW_COMP_MODE) {
		aw8624_i2c_write_bits(aw8624,
				      AW8624_REG_ADCTEST,
				      AW8624_BIT_DETCTRL_VBAT_MODE_MASK,
				      AW8624_BIT_DETCTRL_VBAT_HW_COMP);
	} else {
		aw8624_i2c_write_bits(aw8624,
				      AW8624_REG_ADCTEST,
				      AW8624_BIT_DETCTRL_VBAT_MODE_MASK,
				      AW8624_BIT_DETCTRL_VBAT_SW_COMP);
	}
	return 0;
}

static int aw8624_haptic_set_f0_preset(struct aw8624 *aw8624)
{
	unsigned int f0_reg = 0;

	pr_info("%s: enter\n", __func__);

	f0_reg = 1000000000 / (aw8624->info.f0_pre * aw8624->info.f0_coeff);
	aw8624_i2c_write(aw8624, AW8624_REG_F_PRE_H,
			 (unsigned char)((f0_reg >> 8) & 0xff));
	aw8624_i2c_write(aw8624, AW8624_REG_F_PRE_L,
			 (unsigned char)((f0_reg >> 0) & 0xff));

	return 0;
}

static int aw8624_haptic_read_f0(struct aw8624 *aw8624)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	pr_info("%s: enter\n", __func__);

	ret = aw8624_i2c_read(aw8624, AW8624_REG_F_LRA_F0_H, &reg_val);
	f0_reg = (reg_val << 8);
	ret = aw8624_i2c_read(aw8624, AW8624_REG_F_LRA_F0_L, &reg_val);
	f0_reg |= (reg_val << 0);
	if (!f0_reg) {
		pr_info("%s: not get f0_reg value is 0!\n", __func__);
		return 0;
	}
	f0_tmp = 1000000000 / (f0_reg * aw8624->info.f0_coeff);
	aw8624->f0 = (unsigned int)f0_tmp;
	pr_info("%s: f0=%d\n", __func__, aw8624->f0);

	return 0;
}

static int aw8624_haptic_read_cont_f0(struct aw8624 *aw8624)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	pr_debug("%s: enter\n", __func__);

	ret = aw8624_i2c_read(aw8624, AW8624_REG_F_LRA_CONT_H, &reg_val);
	f0_reg = (reg_val << 8);
	ret = aw8624_i2c_read(aw8624, AW8624_REG_F_LRA_CONT_L, &reg_val);
	f0_reg |= (reg_val << 0);
	if (!f0_reg) {
		pr_info("%s: not get f0_reg value is 0!\n", __func__);
		return 0;
	}
	f0_tmp = 1000000000 / (f0_reg * aw8624->info.f0_coeff);
	aw8624->cont_f0 = (unsigned int)f0_tmp;
	pr_info("%s: f0=%d\n", __func__, aw8624->cont_f0);

	return 0;
}

static int aw8624_haptic_read_beme(struct aw8624 *aw8624)
{
	int ret = 0;
	unsigned char reg_val = 0;
	pr_info("%s: enter\n", __func__);
	ret = aw8624_i2c_read(aw8624, AW8624_REG_WAIT_VOL_MP, &reg_val);
	aw8624->max_pos_beme = (reg_val << 0);
	ret = aw8624_i2c_read(aw8624, AW8624_REG_WAIT_VOL_MN, &reg_val);
	aw8624->max_neg_beme = (reg_val << 0);

	pr_info("%s: max_pos_beme=%d\n", __func__, aw8624->max_pos_beme);
	pr_info("%s: max_neg_beme=%d\n", __func__, aw8624->max_neg_beme);

	return 0;
}

static int aw8624_haptic_get_vbat(struct aw8624 *aw8624)
{
	unsigned char reg_val = 0;
	unsigned char reg_val_sysctrl = 0;
	unsigned char reg_val_detctrl = 0;

	aw8624_haptic_stop(aw8624);
	aw8624_i2c_read(aw8624, AW8624_REG_SYSCTRL, &reg_val_sysctrl);
	aw8624_i2c_read(aw8624, AW8624_REG_DETCTRL, &reg_val_detctrl);
	/*step 1:EN_RAMINIT*/
	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_SYSCTRL,
				AW8624_BIT_SYSCTRL_RAMINIT_MASK,
				AW8624_BIT_SYSCTRL_RAMINIT_EN);

	/*step 2 :launch offset cali */
	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_DETCTRL,
				AW8624_BIT_DETCTRL_DIAG_GO_MASK,
				AW8624_BIT_DETCTRL_DIAG_GO_ENABLE);
	/*step 3 :delay */
	usleep_range(2000, 2500);

	/*step 4 :launch power supply testing */
	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_DETCTRL,
				AW8624_BIT_DETCTRL_VBAT_GO_MASK,
				AW8624_BIT_DETCTRL_VABT_GO_ENABLE);
	usleep_range(2000, 2500);

	aw8624_i2c_read(aw8624, AW8624_REG_VBATDET, &reg_val);
	aw8624->vbat = 6100 * reg_val / 256;

	/*step 5: return val*/
	aw8624_i2c_write(aw8624, AW8624_REG_SYSCTRL, reg_val_sysctrl);

	return 0;
}

/*****************************************************
 *
 * vbat mode
 *
 *****************************************************/
static int aw8624_lra_resistance_detector(struct aw8624 *aw8624)
{
	unsigned char reg_val = 0;
	unsigned char reg_val_sysctrl = 0;
	unsigned char reg_val_anactrl = 0;
	unsigned char reg_val_d2scfg = 0;
	unsigned int r_lra = 0;

	mutex_lock(&aw8624->lock);
	aw8624_i2c_read(aw8624, AW8624_REG_SYSCTRL, &reg_val_sysctrl);
	aw8624_i2c_read(aw8624, AW8624_REG_ANACTRL, &reg_val_anactrl);
	aw8624_i2c_read(aw8624, AW8624_REG_D2SCFG, &reg_val_d2scfg);
	aw8624_haptic_stop(aw8624);
	aw8624_i2c_write_bits(aw8624,
			      AW8624_REG_SYSCTRL,
			      AW8624_BIT_SYSCTRL_RAMINIT_MASK,
			      AW8624_BIT_SYSCTRL_RAMINIT_EN);

	aw8624_i2c_write_bits(aw8624,
			      AW8624_REG_ANACTRL,
			      AW8624_BIT_ANACTRL_EN_IO_PD1_MASK,
			      AW8624_BIT_ANACTRL_EN_IO_PD1_HIGH);

	aw8624_i2c_write_bits(aw8624,
			      AW8624_REG_D2SCFG,
			      AW8624_BIT_D2SCFG_CLK_ADC_MASK,
			      AW8624_BIT_D2SCFG_CLK_ASC_1P5MHZ);

	aw8624_i2c_write_bits(aw8624,
			      AW8624_REG_DETCTRL,
			      AW8624_BIT_DETCTRL_RL_OS_MASK,
			      AW8624_BIT_DETCTRL_RL_DETECT);
	aw8624_i2c_write_bits(aw8624,
			      AW8624_REG_DETCTRL,
			      AW8624_BIT_DETCTRL_DIAG_GO_MASK,
			      AW8624_BIT_DETCTRL_DIAG_GO_ENABLE);
	mdelay(3);
	aw8624_i2c_read(aw8624, AW8624_REG_RLDET, &reg_val);
	r_lra = 298 * reg_val;
	/*len += snprintf(buf+len, PAGE_SIZE-len, "r_lra=%dmohm\n", r_lra); */

	aw8624_i2c_write(aw8624, AW8624_REG_D2SCFG, reg_val_d2scfg);
	aw8624_i2c_write(aw8624, AW8624_REG_ANACTRL, reg_val_anactrl);
	aw8624_i2c_write(aw8624, AW8624_REG_SYSCTRL, reg_val_sysctrl);

	mutex_unlock(&aw8624->lock);

	return r_lra;
}

static int aw8624_haptic_ram_vbat_comp(struct aw8624 *aw8624, bool flag)
{
	pr_debug("%s: enter\n", __func__);
	if (flag)
		aw8624->ram_vbat_comp = AW8624_HAPTIC_RAM_VBAT_COMP_ENABLE;
	else
		aw8624->ram_vbat_comp = AW8624_HAPTIC_RAM_VBAT_COMP_DISABLE;
	return 0;
}

static void aw8624_haptic_set_rtp_aei(struct aw8624 *aw8624, bool flag)
{
	if (flag) {
		aw8624_i2c_write_bits(aw8624, AW8624_REG_SYSINTM,
				      AW8624_BIT_SYSINTM_FF_AE_MASK,
				      AW8624_BIT_SYSINTM_FF_AE_EN);
	} else {
		aw8624_i2c_write_bits(aw8624, AW8624_REG_SYSINTM,
				      AW8624_BIT_SYSINTM_FF_AE_MASK,
				      AW8624_BIT_SYSINTM_FF_AE_OFF);
	}
}

static unsigned char aw8624_haptic_rtp_get_fifo_afi(struct aw8624 *aw8624)
{
	unsigned char ret = 0;
	unsigned char reg_val = 0;

	pr_debug("%s: enter\n", __func__);

	if (aw8624->osc_cali_flag == 1) {
		aw8624_i2c_read(aw8624, AW8624_REG_SYSST, &reg_val);
		reg_val &= AW8624_BIT_SYSST_FF_AFS;
		ret = reg_val >> 3;
	} else {
		aw8624_i2c_read(aw8624, AW8624_REG_SYSINT, &reg_val);
		reg_val &= AW8624_BIT_SYSINT_FF_AFI;
		ret = reg_val >> 3;
	}

	return ret;
}

/*****************************************************
 *
 * rtp
 *
 *****************************************************/
static int aw8624_haptic_rtp_init(struct aw8624 *aw8624)
{
#if 0				//awinic code
	unsigned int buf_len = 0;

	aw8624->rtp_cnt = 0;

	while ((!aw8624_haptic_rtp_get_fifo_afi(aw8624)) &&
	       (aw8624->play_mode == AW8624_HAPTIC_RTP_MODE)) {
		pr_info("%s: rtp cnt = %d\n", __func__, aw8624->rtp_cnt);
		if ((aw8624_rtp->len - aw8624->rtp_cnt) <
		    (aw8624->ram.base_addr >> 3)) {
			buf_len = aw8624_rtp->len - aw8624->rtp_cnt;
		} else {
			buf_len = (aw8624->ram.base_addr >> 3);
		}
		aw8624_i2c_writes(aw8624, AW8624_REG_RTP_DATA,
				  &aw8624_rtp->data[aw8624->rtp_cnt], buf_len);
		aw8624->rtp_cnt += buf_len;
		if (aw8624->rtp_cnt == aw8624_rtp->len) {
			pr_info("%s: rtp update complete\n", __func__);
			aw8624->rtp_cnt = 0;
			return 0;
		}
	}

	if (aw8624->play_mode == AW8624_HAPTIC_RTP_MODE)
		aw8624_haptic_set_rtp_aei(aw8624, true);

	pr_info("%s: exit\n", __func__);

	return 0;
#else //xiaomi code
	unsigned int buf_len = 0;
	bool rtp_start = true;

	pr_debug("%s: enter\n", __func__);
	pm_qos_add_request(&pm_qos_req_vb, PM_QOS_CPU_DMA_LATENCY,
			   PM_QOS_VALUE_VB);
	aw8624->rtp_cnt = 0;
	disable_irq(gpio_to_irq(aw8624->irq_gpio));
	while ((!aw8624_haptic_rtp_get_fifo_afi(aw8624)) &&
	       (aw8624->play_mode == AW8624_HAPTIC_RTP_MODE) &&
	       !atomic_read(&aw8624->exit_in_rtp_loop)) {
		if (rtp_start) {
			if ((aw8624_rtp->len - aw8624->rtp_cnt) <
			    aw8624->ram.base_addr)
				buf_len = aw8624_rtp->len - aw8624->rtp_cnt;
			else
				buf_len = (aw8624->ram.base_addr);
			aw8624_i2c_writes(aw8624, AW8624_REG_RTP_DATA,
					  &aw8624_rtp->data[aw8624->rtp_cnt],
					  buf_len);
			rtp_start = false;
		} else {
			if ((aw8624_rtp->len - aw8624->rtp_cnt) <
			    (aw8624->ram.base_addr >> 2))
				buf_len = aw8624_rtp->len - aw8624->rtp_cnt;
			else
				buf_len = aw8624->ram.base_addr >> 2;
			aw8624_i2c_writes(aw8624, AW8624_REG_RTP_DATA,
					  &aw8624_rtp->data[aw8624->rtp_cnt],
					  buf_len);
		}
		aw8624->rtp_cnt += buf_len;
		pr_debug("%s: update rtp_cnt = %d \n", __func__,
			 aw8624->rtp_cnt);
		if (aw8624->rtp_cnt == aw8624_rtp->len) {
			pr_info("%s: rtp update complete\n", __func__);
			aw8624->rtp_cnt = 0;
			break;
		}
	}
	enable_irq(gpio_to_irq(aw8624->irq_gpio));
	if (aw8624->play_mode == AW8624_HAPTIC_RTP_MODE
	    && !atomic_read(&aw8624->exit_in_rtp_loop)) {
		aw8624_haptic_set_rtp_aei(aw8624, true);
	}

	pr_debug("%s: exit\n", __func__);
	pm_qos_remove_request(&pm_qos_req_vb);
	return 0;
#endif
}

static int16_t aw8624_haptic_effect_strength(struct aw8624 *aw8624)
{
	pr_debug("%s: enter\n", __func__);
	pr_debug("%s: aw8624->play.vmax_mv =0x%x\n", __func__,
		 aw8624->play.vmax_mv);
#if 0
	switch (aw8624->play.vmax_mv) {
	case AW8624_LIGHT_MAGNITUDE:
		aw8624->level = 0x30;
		break;
	case AW8624_MEDIUM_MAGNITUDE:
		aw8624->level = 0x50;
		break;
	case AW8624_STRONG_MAGNITUDE:
		aw8624->level = 0x80;
		break;
	default:
		break;
	}
#else
	if (aw8624->play.vmax_mv >= 0x7FFF)
		aw8624->level = 0x80;	/*128 */
	else if (aw8624->play.vmax_mv <= 0x3FFF)
		aw8624->level = 0x1E;	/*30 */
	else
		aw8624->level = (aw8624->play.vmax_mv - 16383) / 128;
	if (aw8624->level < 0x1E)
		aw8624->level = 0x1E;	/*30 */
#endif

	pr_info("%s: aw8624->level =0x%x\n", __func__, aw8624->level);
	return 0;
}

static int aw8624_haptic_play_effect_seq(struct aw8624 *aw8624,
					 unsigned char flag)
{
	pr_debug("%s: enter  \n", __func__);

	if (aw8624->effect_id > aw8624->info.effect_id_boundary)
		return 0;
	pr_debug("%s:aw8624->effect_id =%d\n", __func__, aw8624->effect_id);
	pr_debug("%s:aw8624->activate_mode =%d\n", __func__,
		 aw8624->activate_mode);
	if (flag) {
		if (aw8624->activate_mode == AW8624_HAPTIC_ACTIVATE_RAM_MODE) {
			aw8624_haptic_set_wav_seq(aw8624, 0x00,
						  (char)aw8624->effect_id + 1);
			aw8624_haptic_set_wav_seq(aw8624, 0x01, 0x00);
			aw8624_haptic_set_wav_loop(aw8624, 0x00, 0x00);
			aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_RAM_MODE);
			aw8624_haptic_effect_strength(aw8624);
			aw8624_haptic_set_gain(aw8624, aw8624->level);
			aw8624_haptic_start(aw8624);
		}
		if (aw8624->activate_mode ==
		    AW8624_HAPTIC_ACTIVATE_RAM_LOOP_MODE) {
			aw8624_haptic_set_repeat_wav_seq(aw8624,
							 (aw8624->info.
							  effect_id_boundary +
							  1));
			aw8624_haptic_effect_strength(aw8624);
			aw8624_haptic_set_gain(aw8624, aw8624->level);
			aw8624_haptic_play_repeat_seq(aw8624, true);
		}
	}
	pr_debug("%s: exit\n", __func__);
	return 0;
}

static void aw8624_haptic_upload_lra(struct aw8624 *aw8624, unsigned char flag)
{
	switch (flag) {
	case NORMAL_CALI:
		pr_debug("%s: set normal value=\n", __func__);
		aw8624_i2c_write(aw8624, AW8624_REG_TRIM_LRA, 0x00);
		break;
	case F0_CALI:
		pr_debug("%s: f0_cali_lra=%d\n", __func__,
			 aw8624->f0_calib_data);
		aw8624_i2c_write(aw8624, AW8624_REG_TRIM_LRA,
				 (char)aw8624->f0_calib_data);
		break;
	case OSC_CALI:
		pr_debug("%s: rtp_cali_lra=%d\n", __func__,
			 aw8624->lra_calib_data);
		aw8624_i2c_write(aw8624, AW8624_REG_TRIM_LRA,
				 (char)aw8624->lra_calib_data);
		break;
	default:
		break;
	}
}

static int aw8624_clock_OSC_trim_calibration(unsigned long int theory_time,
					     unsigned long int real_time)
{
	unsigned int real_code = 0;
	unsigned int LRA_TRIM_CODE = 0;
	unsigned int DFT_LRA_TRIM_CODE = 0;
	unsigned int Not_need_cali_threshold = 10;	/*0.1 percent not need calibrate */

	if (theory_time == real_time) {
		pr_info
		    ("aw_osctheory_time == real_time:%ld  theory_time = %ld not need to cali\n",
		     real_time, theory_time);
		return 0;
	} else if (theory_time < real_time) {
		if ((real_time - theory_time) > (theory_time / 50)) {
			pr_info
			    ("aw_osc(real_time - theory_time) > (theory_time/50) not to cali\n");
			return DFT_LRA_TRIM_CODE;
		}

		if ((real_time - theory_time) <
		    (Not_need_cali_threshold * theory_time / 10000)) {
			pr_info
			    ("aw_oscmicrosecond:%ld  theory_time = %ld not need to cali\n",
			     real_time, theory_time);
			return DFT_LRA_TRIM_CODE;
		}

		real_code = ((real_time - theory_time) * 4000) / theory_time;
		real_code = ((real_code % 10 < 5) ? 0 : 1) + real_code / 10;
		real_code = 32 + real_code;
	} else if (theory_time > real_time) {
		if ((theory_time - real_time) > (theory_time / 50)) {
			pr_info
			    ("aw_osc((theory_time - real_time) > (theory_time / 50)) not to cali\n");
			return DFT_LRA_TRIM_CODE;
		}
		if ((theory_time - real_time) <
		    (Not_need_cali_threshold * theory_time / 10000)) {
			pr_info
			    ("aw_oscmicrosecond:%ld  theory_time = %ld not need to cali\n",
			     real_time, theory_time);
			return DFT_LRA_TRIM_CODE;
		}
		real_code = ((theory_time - real_time) * 4000) / theory_time;
		real_code = ((real_code % 10 < 5) ? 0 : 1) + real_code / 10;
		real_code = 32 - real_code;
	}
	if (real_code > 31)
		LRA_TRIM_CODE = real_code - 32;
	else
		LRA_TRIM_CODE = real_code + 32;
	pr_info
	    ("aw_oscmicrosecond:%ld  theory_time = %ld real_code =0X%02X LRA_TRIM_CODE 0X%02X\n",
	     real_time, theory_time, real_code, LRA_TRIM_CODE);

	return LRA_TRIM_CODE;
}

static int aw8624_rtp_trim_lra_calibration(struct aw8624 *aw8624)
{
	unsigned char reg_val = 0;
	unsigned int fre_val = 0;
	unsigned int theory_time = 0;
	unsigned int lra_rtim_code = 0;

	aw8624_i2c_read(aw8624, AW8624_REG_PWMDBG, &reg_val);
	fre_val = (reg_val & 0x006f) >> 5;

	if (fre_val == 3)
		theory_time = (aw8624->rtp_len / 12000) * 1000000;	/*12K */
	if (fre_val == 2)
		theory_time = (aw8624->rtp_len / 24000) * 1000000;	/*24K */
	if (fre_val == 1 || fre_val == 0)
		theory_time = (aw8624->rtp_len / 48000) * 1000000;	/*48K */

	printk("microsecond:%ld  theory_time = %d\n", aw8624->microsecond,
	       theory_time);

	lra_rtim_code =
	    aw8624_clock_OSC_trim_calibration(theory_time, aw8624->microsecond);
	if (lra_rtim_code > 0) {
		aw8624->lra_calib_data = lra_rtim_code;
		aw8624_i2c_write(aw8624, AW8624_REG_TRIM_LRA,
				 (char)lra_rtim_code);
	}
	pr_info("%s: osc calibration lra_calib_data = %d \n", __func__,
		aw8624->lra_calib_data);
	return 0;
}

static unsigned char aw8624_haptic_osc_read_int(struct aw8624 *aw8624)
{
	unsigned char reg_val = 0;

	aw8624_i2c_read(aw8624, AW8624_REG_DBGSTAT, &reg_val);
	return reg_val;
}

static int aw8624_rtp_osc_calibration(struct aw8624 *aw8624)
{
	const struct firmware *rtp_file;
	int ret = -1;
	unsigned int buf_len = 0;
	unsigned char osc_int_state = 0;
	aw8624->rtp_cnt = 0;
	aw8624->timeval_flags = 1;
	aw8624->osc_cali_flag = 1;

	pr_info("%s: enter\n", __func__);
	/* fw loaded */
	ret = request_firmware(&rtp_file,
			       aw8624_rtp_name[0],
			       aw8624->dev);
	if (ret < 0) {
		pr_err("%s: failed to read %s\n", __func__,
		       aw8624_rtp_name[0]);
		return ret;
	}
	/*awinic add stop,for irq interrupt during calibrate */
	aw8624_haptic_stop(aw8624);
	aw8624->rtp_init = 0;
	mutex_lock(&aw8624->rtp_lock);
	vfree(aw8624_rtp);
	aw8624_rtp = vmalloc(rtp_file->size + sizeof(int));
	if (!aw8624_rtp) {
		release_firmware(rtp_file);
		mutex_unlock(&aw8624->rtp_lock);
		pr_err("%s: error allocating memory\n", __func__);
		return -ENOMEM;
	}
	aw8624_rtp->len = rtp_file->size;
	aw8624->rtp_len = rtp_file->size;
	pr_info("%s: rtp file [%s] size = %d\n", __func__,
		aw8624_rtp_name[0], aw8624_rtp->len);
	memcpy(aw8624_rtp->data, rtp_file->data, rtp_file->size);
	release_firmware(rtp_file);
	mutex_unlock(&aw8624->rtp_lock);

	/* gain */
	aw8624_haptic_ram_vbat_comp(aw8624, false);

	/* rtp mode config */
	aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_RTP_MODE);
	aw8624_i2c_write_bits(aw8624, AW8624_REG_DBGCTRL,
			      AW8624_BIT_DBGCTRL_INTN_TRG_SEL_MASK,
			      AW8624_BIT_DBGCTRL_INTN_SEL_ENABLE);

	disable_irq(gpio_to_irq(aw8624->irq_gpio));
	/* haptic start */
	aw8624_haptic_start(aw8624);
	pm_qos_add_request(&pm_qos_req_vb, PM_QOS_CPU_DMA_LATENCY,
			   PM_QOS_VALUE_VB);
	while (1) {
		if (!aw8624_haptic_rtp_get_fifo_afi(aw8624)) {
			pr_info
			    ("%s !aw8624_haptic_rtp_get_fifo_afi done aw8624->rtp_cnt= %d \n",
			     __func__, aw8624->rtp_cnt);
			mutex_lock(&aw8624->rtp_lock);
			if ((aw8624_rtp->len - aw8624->rtp_cnt) <
			    (aw8624->ram.base_addr >> 2))
				buf_len = aw8624_rtp->len - aw8624->rtp_cnt;
			else
				buf_len = (aw8624->ram.base_addr >> 2);
			if (aw8624->rtp_cnt != aw8624_rtp->len) {
				if (aw8624->timeval_flags == 1) {
					do_gettimeofday(&aw8624->start);
					aw8624->timeval_flags = 0;
				}
				aw8624_i2c_writes(aw8624, AW8624_REG_RTP_DATA,
						  &aw8624_rtp->data[aw8624->
								    rtp_cnt],
						  buf_len);
				aw8624->rtp_cnt += buf_len;
			}
			mutex_unlock(&aw8624->rtp_lock);
		}
		osc_int_state = aw8624_haptic_osc_read_int(aw8624);
		if (osc_int_state & AW8624_BIT_SYSINT_DONEI) {
			do_gettimeofday(&aw8624->end);
			pr_info
			    ("%s vincent playback done aw8624->rtp_cnt= %d \n",
			     __func__, aw8624->rtp_cnt);
			break;
		}

		do_gettimeofday(&aw8624->end);
		aw8624->microsecond =
		    (aw8624->end.tv_sec - aw8624->start.tv_sec) * 1000000 +
		    (aw8624->end.tv_usec - aw8624->start.tv_usec);
		if (aw8624->microsecond > OSC_CALIBRATION_T_LENGTH) {
			pr_info
			    ("%s vincent time out aw8624->rtp_cnt %d osc_int_state %02x\n",
			     __func__, aw8624->rtp_cnt, osc_int_state);
			break;
		}
	}
	pm_qos_remove_request(&pm_qos_req_vb);
	enable_irq(gpio_to_irq(aw8624->irq_gpio));

	aw8624->osc_cali_flag = 0;
	aw8624->microsecond =
	    (aw8624->end.tv_sec - aw8624->start.tv_sec) * 1000000 +
	    (aw8624->end.tv_usec - aw8624->start.tv_usec);
	/*calibration osc */
	pr_info("%s: 2018_microsecond:%ld \n", __func__, aw8624->microsecond);
	pr_info("%s: exit\n", __func__);
	return 0;
}

static void aw8624_rtp_work_routine(struct work_struct *work)
{
	const struct firmware *rtp_file;
	int ret = -1;
	struct aw8624 *aw8624 = container_of(work, struct aw8624, rtp_work);

	pr_debug("%s: enter\n", __func__);

	if ((aw8624->effect_id < aw8624->info.effect_id_boundary) &&
	    (aw8624->effect_id > aw8624->info.effect_max))
		return;
	pr_info("%s: effect_id =%d state = %d\n", __func__, aw8624->effect_id,
		aw8624->state);
	mutex_lock(&aw8624->lock);
	aw8624_haptic_upload_lra(aw8624, OSC_CALI);
	aw8624_haptic_set_rtp_aei(aw8624, false);
	aw8624_interrupt_clear(aw8624);
	//wait for irq to exit
	atomic_set(&aw8624->exit_in_rtp_loop, 1);
	while (atomic_read(&aw8624->is_in_rtp_loop)) {
		pr_info("%s:  goint to waiting irq exit\n", __func__);
		ret =
		    wait_event_interruptible(aw8624->wait_q,
					     atomic_read(&aw8624->
							 is_in_rtp_loop) == 0);
		pr_info("%s:  wakeup \n", __func__);
		if (ret == -ERESTARTSYS) {
			atomic_set(&aw8624->exit_in_rtp_loop, 0);
			wake_up_interruptible(&aw8624->stop_wait_q);
			mutex_unlock(&aw8624->lock);
			pr_err("%s: wake up by signal return erro\n", __func__);
			return;
		}
	}
	atomic_set(&aw8624->exit_in_rtp_loop, 0);
	wake_up_interruptible(&aw8624->stop_wait_q);
	aw8624_haptic_stop(aw8624);

	if (aw8624->state) {
		aw8624_haptic_effect_strength(aw8624);
		aw8624->rtp_file_num = aw8624->effect_id - 20;
		pr_info("%s:   aw8624->rtp_file_num =%d\n", __func__,
			aw8624->rtp_file_num);
		if (aw8624->rtp_file_num < 0)
			aw8624->rtp_file_num = 0;
		if (aw8624->rtp_file_num >
		    ((sizeof(aw8624_rtp_name) / AW8624_RTP_NAME_MAX) - 1))
			aw8624->rtp_file_num =
			    (sizeof(aw8624_rtp_name) / AW8624_RTP_NAME_MAX) - 1;

		/* fw loaded */
		ret = request_firmware(&rtp_file,
				       aw8624_rtp_name[aw8624->rtp_file_num],
				       aw8624->dev);
		if (ret < 0) {
			pr_err("%s: failed to read %s\n", __func__,
			       aw8624_rtp_name[aw8624->rtp_file_num]);
			mutex_unlock(&aw8624->lock);
			return;
		}
		aw8624->rtp_init = 0;
		vfree(aw8624_rtp);
		aw8624_rtp = vmalloc(rtp_file->size + sizeof(int));
		if (!aw8624_rtp) {
			release_firmware(rtp_file);
			pr_err("%s: error allocating memory\n", __func__);
			mutex_unlock(&aw8624->lock);
			return;
		}
		aw8624_rtp->len = rtp_file->size;
		pr_info("%s: rtp file [%s] size = %d\n", __func__,
			aw8624_rtp_name[aw8624->rtp_file_num], aw8624_rtp->len);
		memcpy(aw8624_rtp->data, rtp_file->data, rtp_file->size);
		release_firmware(rtp_file);

		aw8624->rtp_init = 1;

		aw8624_i2c_write_bits(aw8624,
				      AW8624_REG_DBGCTRL,
				      AW8624_BIT_DBGCTRL_INTN_TRG_SEL_MASK,
				      AW8624_BIT_DBGCTRL_INTN_SEL_ENABLE);

		/* gain */
		aw8624_haptic_ram_vbat_comp(aw8624, false);
		aw8624_haptic_set_gain(aw8624, aw8624->level);

		/* rtp mode config */
		aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_RTP_MODE);

		/* haptic start */
		aw8624_haptic_start(aw8624);

		aw8624_haptic_rtp_init(aw8624);
	} else {
		aw8624->rtp_cnt = 0;
		aw8624->rtp_init = 0;
	}
	mutex_unlock(&aw8624->lock);
}

static enum hrtimer_restart aw8624_haptic_audio_timer_func(struct hrtimer
							   *timer)
{
	struct aw8624 *aw8624 =
	    container_of(timer, struct aw8624, haptic_audio.timer);

	pr_debug("%s: enter\n", __func__);
	//schedule_work(&aw8624->haptic_audio.work);
	queue_work(aw8624->work_queue, &aw8624->haptic_audio.work);

	hrtimer_start(&aw8624->haptic_audio.timer,
		      ktime_set(aw8624->haptic_audio.timer_val / 1000000,
				(aw8624->haptic_audio.timer_val % 1000000) *
				1000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

static void aw8624_haptic_audio_work_routine(struct work_struct *work)
{
	struct aw8624 *aw8624 =
	    container_of(work, struct aw8624, haptic_audio.work);

	pr_info("%s: enter\n", __func__);

	mutex_lock(&aw8624->haptic_audio.lock);
	memcpy(&aw8624->haptic_audio.ctr,
	       &aw8624->haptic_audio.data[aw8624->haptic_audio.cnt],
	       sizeof(struct haptic_ctr));
	pr_debug("%s: cnt=%d, cmd=%d, play=%d, wavseq=%d, loop=%d, gain=%d\n",
		 __func__,
		 aw8624->haptic_audio.cnt,
		 aw8624->haptic_audio.ctr.cmd,
		 aw8624->haptic_audio.ctr.play,
		 aw8624->haptic_audio.ctr.wavseq,
		 aw8624->haptic_audio.ctr.loop, aw8624->haptic_audio.ctr.gain);
	mutex_unlock(&aw8624->haptic_audio.lock);
	if (AW8624_HAPTIC_CMD_ENABLE == aw8624->haptic_audio.ctr.cmd) {
		if (AW8624_HAPTIC_PLAY_ENABLE == aw8624->haptic_audio.ctr.play) {
			pr_info("%s: haptic_audio_play_start\n", __func__);
			mutex_lock(&aw8624->lock);
			aw8624_haptic_stop(aw8624);
			aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_RAM_MODE);

			aw8624_haptic_set_wav_seq(aw8624, 0x00,
						  aw8624->haptic_audio.ctr.
						  wavseq);
			aw8624_haptic_set_wav_seq(aw8624, 0x01, 0x00);

			aw8624_haptic_set_wav_loop(aw8624, 0x00,
						   aw8624->haptic_audio.ctr.
						   loop);

			aw8624_haptic_set_gain(aw8624,
					       aw8624->haptic_audio.ctr.gain);

			aw8624_haptic_start(aw8624);
			mutex_unlock(&aw8624->lock);
		} else if (AW8624_HAPTIC_PLAY_STOP ==
			   aw8624->haptic_audio.ctr.play) {
			mutex_lock(&aw8624->lock);
			aw8624_haptic_stop(aw8624);
			mutex_unlock(&aw8624->lock);
		} else if (AW8624_HAPTIC_PLAY_GAIN ==
			   aw8624->haptic_audio.ctr.play) {
			mutex_lock(&aw8624->lock);
			aw8624_haptic_set_gain(aw8624,
					       aw8624->haptic_audio.ctr.gain);
			mutex_unlock(&aw8624->lock);
		}
		mutex_lock(&aw8624->haptic_audio.lock);
		memset(&aw8624->haptic_audio.data[aw8624->haptic_audio.cnt],
		       0, sizeof(struct haptic_ctr));
		mutex_unlock(&aw8624->haptic_audio.lock);
	}

	mutex_lock(&aw8624->haptic_audio.lock);
	aw8624->haptic_audio.cnt++;
	if (aw8624->haptic_audio.data[aw8624->haptic_audio.cnt].cmd == 0) {
		aw8624->haptic_audio.cnt = 0;
		pr_debug("%s: haptic play buffer restart\n", __func__);
	}
	mutex_unlock(&aw8624->haptic_audio.lock);

}

/*****************************************************
 *
 * haptic cont
 *
 *****************************************************/
static int aw8624_haptic_cont(struct aw8624 *aw8624)
{
	pr_info("%s: enter\n", __func__);

	/* work mode */
	aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_CONT_MODE);

	/* preset f0 */
	aw8624_haptic_set_f0_preset(aw8624);

	/* lpf */
	aw8624_i2c_write_bits(aw8624, AW8624_REG_DATCTRL,
			      AW8624_BIT_DATCTRL_FC_MASK,
			      AW8624_BIT_DATCTRL_FC_1000HZ);
	aw8624_i2c_write_bits(aw8624, AW8624_REG_DATCTRL,
			      AW8624_BIT_DATCTRL_LPF_ENABLE_MASK,
			      AW8624_BIT_DATCTRL_LPF_ENABLE);

	/* cont config */
	aw8624_i2c_write_bits(aw8624, AW8624_REG_CONT_CTRL,
			      AW8624_BIT_CONT_CTRL_ZC_DETEC_MASK,
			      AW8624_BIT_CONT_CTRL_ZC_DETEC_ENABLE);
	aw8624_i2c_write_bits(aw8624, AW8624_REG_CONT_CTRL,
			      AW8624_BIT_CONT_CTRL_WAIT_PERIOD_MASK,
			      AW8624_BIT_CONT_CTRL_WAIT_1PERIOD);
	aw8624_i2c_write_bits(aw8624, AW8624_REG_CONT_CTRL,
			      AW8624_BIT_CONT_CTRL_MODE_MASK,
			      AW8624_BIT_CONT_CTRL_BY_GO_SIGNAL);
	aw8624_i2c_write_bits(aw8624, AW8624_REG_CONT_CTRL,
			      AW8624_BIT_CONT_CTRL_EN_CLOSE_MASK,
			      AW8624_CONT_PLAYBACK_MODE);
	aw8624_i2c_write_bits(aw8624, AW8624_REG_CONT_CTRL,
			      AW8624_BIT_CONT_CTRL_F0_DETECT_MASK,
			      AW8624_BIT_CONT_CTRL_F0_DETECT_DISABLE);
	aw8624_i2c_write_bits(aw8624, AW8624_REG_CONT_CTRL,
			      AW8624_BIT_CONT_CTRL_O2C_MASK,
			      AW8624_BIT_CONT_CTRL_O2C_DISABLE);
	aw8624_i2c_write_bits(aw8624, AW8624_REG_CONT_CTRL,
			      AW8624_BIT_CONT_CTRL_AUTO_BRK_MASK,
			      AW8624_BIT_CONT_CTRL_AUTO_BRK_ENABLE);

	/* TD time */
	aw8624_i2c_write(aw8624, AW8624_REG_TD_H,
			 (unsigned char)(aw8624->info.cont_td >> 8));
	aw8624_i2c_write(aw8624, AW8624_REG_TD_L,
			 (unsigned char)(aw8624->info.cont_td >> 0));
	aw8624_i2c_write(aw8624, AW8624_REG_TSET, aw8624->info.tset);

	/* zero cross */
	aw8624_i2c_write(aw8624, AW8624_REG_ZC_THRSH_H,
			 (unsigned char)(aw8624->info.cont_zc_thr >> 8));
	aw8624_i2c_write(aw8624, AW8624_REG_ZC_THRSH_L,
			 (unsigned char)(aw8624->info.cont_zc_thr >> 0));

	aw8624_i2c_write_bits(aw8624, AW8624_REG_BEMF_NUM,
			      AW8624_BIT_BEMF_NUM_BRK_MASK,
			      aw8624->info.cont_num_brk);
	aw8624_i2c_write(aw8624, AW8624_REG_TIME_NZC, 0x23);	// 35*171us=5.985ms

	/* f0 driver level */
	aw8624_i2c_write(aw8624, AW8624_REG_DRV_LVL, aw8624->info.cont_drv_lvl);
	aw8624_i2c_write(aw8624, AW8624_REG_DRV_LVL_OV,
			 aw8624->info.cont_drv_lvl_ov);

	/* cont play go */
	aw8624_haptic_play_go(aw8624, true);

	return 0;
}

/*****************************************************
 *
 * haptic cont mode f0 cali
 *
 *****************************************************/
static int aw8624_haptic_get_f0(struct aw8624 *aw8624)
{
	int ret = 0;
	int i = 0;
	unsigned char reg_val = 0;
	unsigned char f0_pre_num = 0;
	unsigned char f0_wait_num = 0;
	unsigned int f0_repeat_num = 0;
	unsigned char f0_trace_num = 0;
	unsigned int t_f0_trace_ms = 0;
	unsigned int t_f0_ms = 0;
	unsigned char d2scfg_val = 0;
	unsigned int f0_cali_cnt = 50;

	pr_info("%s: enter\n", __func__);

	aw8624->f0 = aw8624->info.f0_pre;

	/* f0 calibrate work mode */
	aw8624_haptic_stop(aw8624);
	aw8624_i2c_write(aw8624, AW8624_REG_TRIM_LRA, 0x00);
	aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_CONT_MODE);

	/* set d2s gain to 40X */
	aw8624_i2c_read(aw8624, AW8624_REG_D2SCFG, &d2scfg_val);
	aw8624_i2c_write_bits(aw8624,
			      AW8624_REG_D2SCFG,
			      AW8624_BIT_D2SCFG_DS_GAIN_MASK,
			      AW8624_BIT_D2SCFG_DS_GAIN_40);

	aw8624_i2c_write_bits(aw8624,
			      AW8624_REG_CONT_CTRL,
			      AW8624_BIT_CONT_CTRL_EN_CLOSE_MASK,
			      AW8624_BIT_CONT_CTRL_OPEN_PLAYBACK);
	aw8624_i2c_write_bits(aw8624,
			      AW8624_REG_CONT_CTRL,
			      AW8624_BIT_CONT_CTRL_F0_DETECT_MASK,
			      AW8624_BIT_CONT_CTRL_F0_DETECT_ENABLE);

	/* LPF */
	aw8624_i2c_write_bits(aw8624,
			      AW8624_REG_DATCTRL,
			      AW8624_BIT_DATCTRL_FC_MASK,
			      AW8624_BIT_DATCTRL_FC_1000HZ);
	aw8624_i2c_write_bits(aw8624,
			      AW8624_REG_DATCTRL,
			      AW8624_BIT_DATCTRL_LPF_ENABLE_MASK,
			      AW8624_BIT_DATCTRL_LPF_ENABLE);

	/* LRA OSC Source */
	if (aw8624->f0_cali_flag == AW8624_HAPTIC_CALI_F0) {
		aw8624_i2c_write_bits(aw8624,
				      AW8624_REG_ANACTRL,
				      AW8624_BIT_ANACTRL_LRA_SRC_MASK,
				      AW8624_BIT_ANACTRL_LRA_SRC_REG);
	} else {
		aw8624_i2c_write_bits(aw8624,
				      AW8624_REG_ANACTRL,
				      AW8624_BIT_ANACTRL_LRA_SRC_MASK,
				      AW8624_BIT_ANACTRL_LRA_SRC_EFUSE);
	}

	/* preset f0 */
	aw8624_haptic_set_f0_preset(aw8624);

	/* f0 driver level */
	aw8624_i2c_write(aw8624, AW8624_REG_DRV_LVL, aw8624->info.cont_drv_lvl);

	/* f0 trace parameter */
	t_f0_ms = 1000 * 10 / aw8624->info.f0_pre;
	f0_pre_num = aw8624->info.f0_trace_parameter[0];
	f0_wait_num = aw8624->info.f0_trace_parameter[1];
	f0_trace_num = aw8624->info.f0_trace_parameter[3];
	f0_repeat_num = aw8624->info.f0_trace_parameter[2];
	/*
	   ((aw8624->info.parameter1 * aw8624->info.f0_pre * 100) / 5000 -
	   100 * (f0_pre_num + f0_wait_num + 2)) / (f0_trace_num+f0_wait_num + 2);

	   if (f0_repeat_num % 100 >= 50)
	   f0_repeat_num = f0_repeat_num / 100 + 1;
	   else
	   f0_repeat_num = f0_repeat_num / 100;
	 */
	aw8624_i2c_write(aw8624,
			 AW8624_REG_NUM_F0_1,
			 (f0_pre_num << 4) | (f0_wait_num << 0));
	aw8624_i2c_write(aw8624,
			 AW8624_REG_NUM_F0_2, (char)(f0_repeat_num << 0));
	aw8624_i2c_write(aw8624, AW8624_REG_NUM_F0_3, (f0_trace_num << 0));

	/* clear aw8624 interrupt */
	ret = aw8624_i2c_read(aw8624, AW8624_REG_SYSINT, &reg_val);

	/* play go and start f0 calibration */
	aw8624_haptic_play_go(aw8624, true);

	/* f0 trace time */
	t_f0_trace_ms =
	    t_f0_ms * (f0_pre_num + f0_wait_num +
		       (f0_trace_num + f0_wait_num) * f0_repeat_num);
	mdelay(t_f0_trace_ms + 10);

	for (i = 0; i < f0_cali_cnt; i++) {
		ret = aw8624_i2c_read(aw8624, AW8624_REG_GLB_STATE, &reg_val);
		/* f0 calibrate done */
		if ((reg_val & 0x0f) == 0x00) {
			aw8624_haptic_read_f0(aw8624);
			aw8624_haptic_read_beme(aw8624);
			break;
		}
		usleep_range(10000, 10500);
		pr_info("%s: f0 cali sleep 10ms,glb_state=0x%x\n", __func__,
			reg_val);
	}

	aw8624_i2c_write_bits(aw8624,
			      AW8624_REG_CONT_CTRL,
			      AW8624_BIT_CONT_CTRL_EN_CLOSE_MASK,
			      AW8624_BIT_CONT_CTRL_CLOSE_PLAYBACK);
	aw8624_i2c_write_bits(aw8624,
			      AW8624_REG_CONT_CTRL,
			      AW8624_BIT_CONT_CTRL_F0_DETECT_MASK,
			      AW8624_BIT_CONT_CTRL_F0_DETECT_DISABLE);
	/* set d2scfg to default */
	aw8624_i2c_write(aw8624, AW8624_REG_D2SCFG, d2scfg_val);

	return ret;
}

static int aw8624_haptic_f0_calibration(struct aw8624 *aw8624)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_limit = 0;
	char f0_cali_lra = 0;
	int f0_cali_step = 0;

	pr_info("%s: enter\n", __func__);

	aw8624->f0_cali_flag = AW8624_HAPTIC_CALI_F0;

	aw8624_i2c_write(aw8624, AW8624_REG_TRIM_LRA, 0x00);
	if (aw8624_haptic_get_f0(aw8624)) {
		pr_err("%s: get f0 error, user defafult f0\n", __func__);
	} else {
		/* max and min limit */

		f0_limit = aw8624->f0;
		if (aw8624->f0 * 100 <
		    aw8624->info.f0_pre * (100 - aw8624->info.f0_cali_percen)) {
			f0_limit = aw8624->info.f0_pre;
		}
		if (aw8624->f0 * 100 >
		    aw8624->info.f0_pre * (100 + aw8624->info.f0_cali_percen)) {
			f0_limit = aw8624->info.f0_pre;
		}

		/* calculate cali step */
		f0_cali_step =
		    100000 * ((int)f0_limit -
			      (int)aw8624->info.f0_pre) / ((int)f0_limit * 25);
		pr_info("%s:  f0_cali_step=%d\n", __func__, f0_cali_step);
		pr_info("%s:  f0_limit=%d\n", __func__, (int)f0_limit);
		pr_info("%s:  aw8624->info.f0_pre=%d\n", __func__,
			(int)aw8624->info.f0_pre);

		if (f0_cali_step >= 0) {	/*f0_cali_step >= 0 */
			if (f0_cali_step % 10 >= 5) {
				f0_cali_step = f0_cali_step / 10 + 1 +
				    (aw8624->chipid_flag == 1 ? 32 : 16);
			} else {
				f0_cali_step = f0_cali_step / 10 +
				    (aw8624->chipid_flag == 1 ? 32 : 16);
			}
		} else {	/*f0_cali_step < 0 */
			if (f0_cali_step % 10 <= -5) {
				f0_cali_step =
				    (aw8624->chipid_flag == 1 ? 32 : 16) +
				    (f0_cali_step / 10 - 1);
			} else {
				f0_cali_step =
				    (aw8624->chipid_flag == 1 ? 32 : 16) +
				    f0_cali_step / 10;
			}
		}
		if (aw8624->chipid_flag == 1) {
			printk("%s  %d aw8624->chipid_flag = 1 \n", __func__,
			       __LINE__);
			if (f0_cali_step > 31)
				f0_cali_lra = (char)f0_cali_step - 32;
			else
				f0_cali_lra = (char)f0_cali_step + 32;
		} else {
			if (f0_cali_step < 16 ||
			    (f0_cali_step > 31 && f0_cali_step < 48)) {
				f0_cali_lra = (char)f0_cali_step + 16;
			} else {
				f0_cali_lra = (char)f0_cali_step - 16;
			}
		}
		aw8624->f0_calib_data = (int)f0_cali_lra;
		pr_info("%s: f0_cali_lra=%d\n", __func__, (int)f0_cali_lra);

		/* update cali step */
		aw8624_i2c_write(aw8624, AW8624_REG_TRIM_LRA,
				 (char)f0_cali_lra);
		aw8624_i2c_read(aw8624, AW8624_REG_TRIM_LRA, &reg_val);
		pr_info("%s: final trim_lra=0x%02x\n", __func__, reg_val);
	}

	/* if (aw8624_haptic_get_f0(aw8624)) { */
	/* pr_err("%s: get f0 error, user defafult f0\n", __func__); */
	/* } */

	mdelay(1);

	aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_STANDBY_MODE);
	aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_RAM_MODE);
	aw8624_haptic_stop(aw8624);

	return ret;
}

/*****************************************************
 *
 * haptic fops
 *
 *****************************************************/
static int aw8624_file_open(struct inode *inode, struct file *file)
{
	if (!try_module_get(THIS_MODULE))
		return -ENODEV;
	pr_info("%s: enter\n", __func__);
	file->private_data = (void *)g_aw8624;

	return 0;
}

static int aw8624_file_release(struct inode *inode, struct file *file)
{
	file->private_data = (void *)NULL;
	pr_info("%s: enter\n", __func__);
	module_put(THIS_MODULE);

	return 0;
}

static long aw8624_file_unlocked_ioctl(struct file *file, unsigned int cmd,
				       unsigned long arg)
{
	struct aw8624 *aw8624 = (struct aw8624 *)file->private_data;

	int ret = 0;
	pr_info("%s: enter\n", __func__);
	dev_info(aw8624->dev, "%s: cmd=0x%x, arg=0x%lx\n", __func__, cmd, arg);

	mutex_lock(&aw8624->lock);

	if (_IOC_TYPE(cmd) != AW8624_HAPTIC_IOCTL_MAGIC) {
		dev_err(aw8624->dev, "%s: cmd magic err\n", __func__);
		return -EINVAL;
	}

	switch (cmd) {
	default:
		dev_err(aw8624->dev, "%s, unknown cmd\n", __func__);
		break;
	}

	mutex_unlock(&aw8624->lock);

	return ret;
}

static ssize_t aw8624_file_read(struct file *filp, char *buff, size_t len,
				loff_t *offset)
{
	struct aw8624 *aw8624 = (struct aw8624 *)filp->private_data;
	int ret = 0;
	int i = 0;
	unsigned char reg_val = 0;
	unsigned char *pbuff = NULL;
	pr_info("%s: enter\n", __func__);
	mutex_lock(&aw8624->lock);

	dev_info(aw8624->dev, "%s: len=%zu\n", __func__, len);

	switch (aw8624->fileops.cmd) {
	case AW8624_HAPTIC_CMD_READ_REG:
		pbuff = (unsigned char *)kzalloc(len, GFP_KERNEL);
		if (pbuff != NULL) {
			for (i = 0; i < len; i++) {
				aw8624_i2c_read(aw8624, aw8624->fileops.reg + i,
						&reg_val);
				pbuff[i] = reg_val;
			}
			for (i = 0; i < len; i++) {
				dev_info(aw8624->dev, "%s: pbuff[%d]=0x%02x\n",
					 __func__, i, pbuff[i]);
			}
			ret = copy_to_user(buff, pbuff, len);
			if (ret) {
				dev_err(aw8624->dev, "%s: copy to user fail\n",
					__func__);
			}
			kfree(pbuff);
		} else {
			dev_err(aw8624->dev, "%s: alloc memory fail\n",
				__func__);
		}
		break;
	default:
		dev_err(aw8624->dev, "%s, unknown cmd %d \n", __func__,
			aw8624->fileops.cmd);
		break;
	}

	mutex_unlock(&aw8624->lock);

	return len;
}

static ssize_t aw8624_file_write(struct file *filp, const char *buff,
				 size_t len, loff_t *off)
{
	struct aw8624 *aw8624 = (struct aw8624 *)filp->private_data;
	int i = 0;
	int ret = 0;
	unsigned char *pbuff = NULL;
	pr_info("%s: enter\n", __func__);
	pbuff = (unsigned char *)kzalloc(len, GFP_KERNEL);
	if (pbuff == NULL) {
		dev_err(aw8624->dev, "%s: alloc memory fail\n", __func__);
		return len;
	}
	ret = copy_from_user(pbuff, buff, len);
	if (ret) {
		dev_err(aw8624->dev, "%s: copy from user fail\n", __func__);
		return len;
	}

	for (i = 0; i < len; i++) {
		dev_info(aw8624->dev, "%s: pbuff[%d]=0x%02x\n",
			 __func__, i, pbuff[i]);
	}

	mutex_lock(&aw8624->lock);

	aw8624->fileops.cmd = pbuff[0];

	switch (aw8624->fileops.cmd) {
	case AW8624_HAPTIC_CMD_READ_REG:
		if (len == 2) {
			aw8624->fileops.reg = pbuff[1];
		} else {
			dev_err(aw8624->dev, "%s: read cmd len %zu err\n",
				__func__, len);
		}
		break;
	case AW8624_HAPTIC_CMD_WRITE_REG:
		if (len > 2) {
			for (i = 0; i < len - 2; i++) {
				dev_info(aw8624->dev,
					 "%s: write reg0x%02x=0x%02x\n",
					 __func__, pbuff[1] + i, pbuff[i + 2]);
				aw8624_i2c_write(aw8624, pbuff[1] + i,
						 pbuff[2 + i]);
			}
		} else {
			dev_err(aw8624->dev, "%s: write cmd len %zu err\n",
				__func__, len);
		}
		break;
	default:
		dev_err(aw8624->dev, "%s, unknown cmd %d \n", __func__,
			aw8624->fileops.cmd);
		break;
	}

	mutex_unlock(&aw8624->lock);

	if (pbuff != NULL) {
		kfree(pbuff);
	}
	return len;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = aw8624_file_read,
	.write = aw8624_file_write,
	.unlocked_ioctl = aw8624_file_unlocked_ioctl,
	.open = aw8624_file_open,
	.release = aw8624_file_release,
};

static struct miscdevice aw8624_haptic_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = AW8624_HAPTIC_NAME,
	.fops = &fops,
};

static int aw8624_haptic_init(struct aw8624 *aw8624)
{
	int ret = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0, reg_flag = 0;
	unsigned char bemf_config = 0;

	pr_info("%s: enter\n", __func__);
	ret = misc_register(&aw8624_haptic_misc);
	if (ret) {
		dev_err(aw8624->dev, "%s: misc fail: %d\n", __func__, ret);
		return ret;
	}

	/* haptic audio */
	aw8624->haptic_audio.delay_val = 1;
	aw8624->haptic_audio.timer_val = 21318;

	hrtimer_init(&aw8624->haptic_audio.timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	aw8624->haptic_audio.timer.function = aw8624_haptic_audio_timer_func;
	INIT_WORK(&aw8624->haptic_audio.work, aw8624_haptic_audio_work_routine);

	mutex_init(&aw8624->haptic_audio.lock);

	/* haptic init */
	mutex_lock(&aw8624->lock);
	ret = aw8624_i2c_read(aw8624, AW8624_REG_EF_RDATAH, &reg_flag);
	if ((ret >= 0) && ((reg_flag & 0x1) == 1)) {
		aw8624->chipid_flag = 1;
	} else {
		dev_err(aw8624->dev,
			"%s: to read register AW8624_REG_EF_RDATAH: %d\n",
			__func__, ret);
	}

	aw8624->activate_mode = aw8624->info.mode;
	aw8624->osc_cali_run = 0;
	ret = aw8624_i2c_read(aw8624, AW8624_REG_WAVSEQ1, &reg_val);
	aw8624->index = reg_val & 0x7F;
	ret = aw8624_i2c_read(aw8624, AW8624_REG_DATDBG, &reg_val);
	aw8624->gain = reg_val & 0xFF;
	for (i = 0; i < AW8624_SEQUENCER_SIZE; i++) {
		ret = aw8624_i2c_read(aw8624, AW8624_REG_WAVSEQ1 + i, &reg_val);
		aw8624->seq[i] = reg_val;
	}

	aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_STANDBY_MODE);
	aw8624_haptic_set_pwm(aw8624, AW8624_PWM_24K);

	aw8624_haptic_swicth_motorprotect_config(aw8624, 0x00, 0x00);

	//aw8624_haptic_offset_calibration(aw8624);

	/* vbat compensation */
	aw8624_haptic_cont_vbat_mode(aw8624,
				     AW8624_HAPTIC_CONT_VBAT_HW_COMP_MODE);
	//aw8624->ram_vbat_comp = AW8624_HAPTIC_RAM_VBAT_COMP_ENABLE;
	mutex_unlock(&aw8624->lock);

	/* f0 calibration */

	mutex_lock(&aw8624->lock);
	aw8624_i2c_write_bits(aw8624, AW8624_REG_R_SPARE,
			      AW8624_BIT_R_SPARE_MASK,
			      AW8624_BIT_R_SPARE_ENABLE);
	aw8624_haptic_f0_calibration(aw8624);
	mutex_unlock(&aw8624->lock);

	/*brake */
	mutex_lock(&aw8624->lock);
	aw8624_i2c_write(aw8624, AW8624_REG_SW_BRAKE, aw8624->info.sw_brake);
	aw8624_i2c_write(aw8624, AW8624_REG_THRS_BRA_END, 0x00);
	aw8624_i2c_write_bits(aw8624,
			      AW8624_REG_WAVECTRL,
			      AW8624_BIT_WAVECTRL_NUM_OV_DRIVER_MASK,
			      AW8624_BIT_WAVECTRL_NUM_OV_DRIVER);
	aw8624->f0_value = 20000 / aw8624->info.f0_pre + 1;
	/* zero cross */
	aw8624_i2c_write(aw8624,
			 AW8624_REG_ZC_THRSH_H,
			 (unsigned char)(aw8624->info.cont_zc_thr >> 8));
	aw8624_i2c_write(aw8624,
			 AW8624_REG_ZC_THRSH_L,
			 (unsigned char)(aw8624->info.cont_zc_thr >> 0));
	aw8624_i2c_write(aw8624, AW8624_REG_TSET, aw8624->info.tset);

	/* beme config */
	bemf_config = aw8624->info.bemf_config[0];
	aw8624_i2c_write(aw8624, AW8624_REG_BEMF_VTHH_H, bemf_config);
	bemf_config = aw8624->info.bemf_config[1];
	aw8624_i2c_write(aw8624, AW8624_REG_BEMF_VTHH_L, bemf_config);
	bemf_config = aw8624->info.bemf_config[2];
	aw8624_i2c_write(aw8624, AW8624_REG_BEMF_VTHL_H, bemf_config);
	bemf_config = aw8624->info.bemf_config[3];
	aw8624_i2c_write(aw8624, AW8624_REG_BEMF_VTHL_L, bemf_config);
	mutex_unlock(&aw8624->lock);
	return ret;
}

/*****************************************************
 *
 * vibrator
 *
 *****************************************************/
static enum hrtimer_restart qti_hap_stop_timer(struct hrtimer *timer)
{
	struct aw8624 *aw8624 = container_of(timer, struct aw8624,
					     stop_timer);
	int rc;

	pr_info("%s: enter\n", __func__);
	aw8624->play.length_us = 0;
	rc = aw8624_haptic_play_go(aw8624, false);	// qti_haptics_play(aw8624, false);
	if (rc < 0)
		dev_err(aw8624->dev, "Stop playing failed, rc=%d\n", rc);

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart qti_hap_disable_timer(struct hrtimer *timer)
{
	struct aw8624 *aw8624 = container_of(timer, struct aw8624,
					     hap_disable_timer);
	int rc;

	pr_info("%s: enter\n", __func__);
	rc = aw8624_haptic_play_go(aw8624, false);	//qti_haptics_module_en(aw8624, false);
	if (rc < 0)
		dev_err(aw8624->dev, "Disable haptics module failed, rc=%d\n",
			rc);

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart aw8624_vibrator_timer_func(struct hrtimer *timer)
{
	struct aw8624 *aw8624 = container_of(timer, struct aw8624, timer);

	pr_info("%s: enter\n", __func__);

	aw8624->state = 0;
	//schedule_work(&aw8624->vibrator_work);
	queue_work(aw8624->work_queue, &aw8624->vibrator_work);

	return HRTIMER_NORESTART;
}

static void aw8624_vibrator_work_routine(struct work_struct *work)
{
	struct aw8624 *aw8624 =
	    container_of(work, struct aw8624, vibrator_work);

	pr_debug("%s: enter\n", __func__);
	pr_info("%s: state=%d activate_mode = %d duration = %d\n", __func__,
		aw8624->state, aw8624->activate_mode, aw8624->duration);
	mutex_lock(&aw8624->lock);
	aw8624_haptic_upload_lra(aw8624, F0_CALI);
	aw8624_haptic_stop(aw8624);
	if (aw8624->state) {
		if (aw8624->activate_mode == AW8624_HAPTIC_ACTIVATE_RAM_MODE) {
			aw8624_haptic_ram_vbat_comp(aw8624, false);
			/* enable donei */
			aw8624_haptic_play_effect_seq(aw8624, true);
		} else if (aw8624->activate_mode ==
			   AW8624_HAPTIC_ACTIVATE_RAM_LOOP_MODE) {
			aw8624_haptic_ram_vbat_comp(aw8624, true);
			aw8624_haptic_play_effect_seq(aw8624, true);
			hrtimer_start(&aw8624->timer,
				      ktime_set(aw8624->duration / 1000,
						(aw8624->duration % 1000) *
						1000000), HRTIMER_MODE_REL);
			pm_stay_awake(aw8624->dev);
			aw8624->wk_lock_flag = 1;
		} else if (aw8624->activate_mode ==
			   AW8624_HAPTIC_ACTIVATE_CONT_MODE) {
			aw8624_haptic_cont(aw8624);
			hrtimer_start(&aw8624->timer,
				      ktime_set(aw8624->duration / 1000,
						(aw8624->duration % 1000) *
						1000000), HRTIMER_MODE_REL);
		} else {
			/*other mode */
		}
	} else {
		if (aw8624->wk_lock_flag == 1) {
			pm_relax(aw8624->dev);
			aw8624->wk_lock_flag = 0;
		}
	}
	pr_debug("%s: exit \n", __func__);
	mutex_unlock(&aw8624->lock);
}

static int aw8624_vibrator_init(struct aw8624 *aw8624)
{
	pr_info("%s: enter\n", __func__);

	hrtimer_init(&aw8624->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw8624->timer.function = aw8624_vibrator_timer_func;
	INIT_WORK(&aw8624->vibrator_work, aw8624_vibrator_work_routine);
	INIT_WORK(&aw8624->rtp_work, aw8624_rtp_work_routine);

	mutex_init(&aw8624->lock);
	atomic_set(&aw8624->is_in_rtp_loop, 0);
	atomic_set(&aw8624->exit_in_rtp_loop, 0);
	init_waitqueue_head(&aw8624->wait_q);
	init_waitqueue_head(&aw8624->stop_wait_q);

	return 0;
}

/******************************************************
 *
 * irq
 *
 ******************************************************/
static void aw8624_interrupt_clear(struct aw8624 *aw8624)
{
	unsigned char reg_val = 0;
	pr_debug("%s: enter\n", __func__);
	aw8624_i2c_read(aw8624, AW8624_REG_SYSINT, &reg_val);
	pr_debug("%s: reg SYSINT=0x%x\n", __func__, reg_val);
}

static void aw8624_interrupt_setup(struct aw8624 *aw8624)
{
	unsigned char reg_val = 0;

	aw8624_i2c_read(aw8624, AW8624_REG_SYSINT, &reg_val);
	pr_info("%s: reg SYSINT=0x%x\n", __func__, reg_val);

	aw8624_i2c_write_bits(aw8624, AW8624_REG_SYSINTM,
			      AW8624_BIT_SYSINTM_UVLO_MASK,
			      AW8624_BIT_SYSINTM_UVLO_EN);
	aw8624_i2c_write_bits(aw8624, AW8624_REG_SYSINTM,
			      AW8624_BIT_SYSINTM_OCD_MASK,
			      AW8624_BIT_SYSINTM_OCD_EN);
	aw8624_i2c_write_bits(aw8624, AW8624_REG_SYSINTM,
			      AW8624_BIT_SYSINTM_OT_MASK,
			      AW8624_BIT_SYSINTM_OT_EN);
}

static irqreturn_t aw8624_irq(int irq, void *data)
{
	struct aw8624 *aw8624 = data;
	unsigned char reg_val = 0;
	unsigned char dbg_val = 0;
	unsigned int buf_len = 0;

	pr_debug("%s: enter\n", __func__);
	atomic_set(&aw8624->is_in_rtp_loop, 1);
	aw8624_i2c_read(aw8624, AW8624_REG_SYSINT, &reg_val);
	pr_debug("%s: reg SYSINT=0x%x\n", __func__, reg_val);
	aw8624_i2c_read(aw8624, AW8624_REG_DBGSTAT, &dbg_val);
	pr_debug("%s: reg DBGSTAT=0x%x\n", __func__, dbg_val);

	if (reg_val & AW8624_BIT_SYSINT_OVI) {
		pr_err("%s: chip ov int error\n", __func__);
	}
	if (reg_val & AW8624_BIT_SYSINT_UVLI) {
		pr_err("%s: chip uvlo int error\n", __func__);
	}
	if (reg_val & AW8624_BIT_SYSINT_OCDI) {
		pr_err("%s: chip over current int error\n", __func__);
	}
	if (reg_val & AW8624_BIT_SYSINT_OTI) {
		pr_err("%s: chip over temperature int error\n", __func__);
	}
	if (reg_val & AW8624_BIT_SYSINT_FF_AEI) {
		pr_debug("%s: aw8624 rtp fifo almost empty int\n", __func__);
		if (aw8624->rtp_init) {
			while ((!aw8624_haptic_rtp_get_fifo_afi(aw8624)) &&
			       (aw8624->play_mode == AW8624_HAPTIC_RTP_MODE)
			       && !atomic_read(&aw8624->exit_in_rtp_loop)) {
				mutex_lock(&aw8624->rtp_lock);
				pr_debug
				    ("%s: aw8624 rtp mode fifo update, cnt=%d\n",
				     __func__, aw8624->rtp_cnt);
				if (!aw8624_rtp) {
					pr_info("%s:aw8624_rtp is null break\n",
						__func__);
					mutex_unlock(&aw8624->rtp_lock);
					break;
				}
				if ((aw8624_rtp->len - aw8624->rtp_cnt) <
				    (aw8624->ram.base_addr >> 2)) {
					buf_len =
					    aw8624_rtp->len - aw8624->rtp_cnt;
				} else {
					buf_len = (aw8624->ram.base_addr >> 2);
				}
				aw8624_i2c_writes(aw8624, AW8624_REG_RTP_DATA,
						  &aw8624_rtp->data[aw8624->
								    rtp_cnt],
						  buf_len);
				aw8624->rtp_cnt += buf_len;
				if (aw8624->rtp_cnt == aw8624_rtp->len) {
					pr_info("%s: rtp update complete\n",
						__func__);
					aw8624_haptic_set_rtp_aei(aw8624,
								  false);
					aw8624->rtp_cnt = 0;
					aw8624->rtp_init = 0;
					mutex_unlock(&aw8624->rtp_lock);
					break;
				}
				mutex_unlock(&aw8624->rtp_lock);
			}
		} else {
			pr_debug("%s: aw8624 rtp init = %d, init error\n",
				 __func__, aw8624->rtp_init);
		}
	}

	if (reg_val & AW8624_BIT_SYSINT_FF_AFI) {
		pr_debug("%s: aw8624 rtp mode fifo full empty\n", __func__);
	}

	if (aw8624->play_mode != AW8624_HAPTIC_RTP_MODE
	    || atomic_read(&aw8624->exit_in_rtp_loop)) {
		aw8624_haptic_set_rtp_aei(aw8624, false);
	}

	aw8624_i2c_read(aw8624, AW8624_REG_SYSINT, &reg_val);
	pr_debug("%s: reg SYSINT=0x%x\n", __func__, reg_val);
	aw8624_i2c_read(aw8624, AW8624_REG_SYSST, &reg_val);
	pr_debug("%s: reg SYSST=0x%x\n", __func__, reg_val);
	atomic_set(&aw8624->is_in_rtp_loop, 0);
	wake_up_interruptible(&aw8624->wait_q);
	pr_debug("%s: exit\n", __func__);
	return IRQ_HANDLED;
}

/*****************************************************
 *
 * device tree
 *
 *****************************************************/
static int aw8624_parse_dt(struct device *dev, struct aw8624 *aw8624,
			   struct device_node *np)
{
	unsigned int val = 0;
	unsigned int f0_trace_parameter[4];
	unsigned int bemf_config[4];
	unsigned int rtp_time[175];
	//unsigned int trig_config[15];
	struct qti_hap_config *config = &aw8624->config;
	struct device_node *child_node;
	struct qti_hap_effect *effect;
	int rc = 0, tmp, i = 0, j, m;

	pr_info("%s:  enter\n", __func__);
	aw8624->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (aw8624->reset_gpio < 0) {
		dev_err(dev,
			"%s: no reset gpio provided, will not HW reset device\n",
			__func__);
		return -EINVAL;
	} else {
		dev_info(dev, "%s: reset gpio provided ok\n", __func__);
	}
	aw8624->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (aw8624->irq_gpio < 0) {
		dev_err(dev, "%s: no irq gpio provided.\n", __func__);
	} else {
		dev_info(dev, "%s: irq gpio provided ok.\n", __func__);
	}

	val = of_property_read_u32(np, "vib_mode", &aw8624->info.mode);
	if (val != 0)
		pr_err("%s: vib_mode not found\n", __func__);
	val = of_property_read_u32(np, "vib_f0_pre", &aw8624->info.f0_pre);
	if (val != 0)
		pr_err("%s: vib_f0_pre not found\n", __func__);
	val =
	    of_property_read_u32(np, "vib_f0_cali_percen",
				 &aw8624->info.f0_cali_percen);
	if (val != 0)
		pr_err("%s: vib_f0_cali_percen not found\n", __func__);
	val =
	    of_property_read_u32(np, "vib_cont_drv_lev",
				 &aw8624->info.cont_drv_lvl);
	if (val != 0)
		pr_err("%s: vib_cont_drv_lev not found\n", __func__);
	val =
	    of_property_read_u32(np, "vib_cont_drv_lvl_ov",
				 &aw8624->info.cont_drv_lvl_ov);
	if (val != 0)
		pr_err("%s: vib_cont_drv_lvl_ov not found\n", __func__);
	val = of_property_read_u32(np, "vib_cont_td", &aw8624->info.cont_td);
	if (val != 0)
		pr_err("%s: vib_cont_td not found\n", __func__);
	val =
	    of_property_read_u32(np, "vib_cont_zc_thr",
				 &aw8624->info.cont_zc_thr);
	if (val != 0)
		pr_err("%s: vib_cont_zc_thr not found\n", __func__);
	val =
	    of_property_read_u32(np, "vib_cont_num_brk",
				 &aw8624->info.cont_num_brk);
	if (val != 0)
		pr_err("%s: vib_cont_num_brk not found\n", __func__);
	val = of_property_read_u32(np, "vib_f0_coeff", &aw8624->info.f0_coeff);
	if (val != 0)
		pr_err("%s: vib_f0_coeff not found\n", __func__);

	val = of_property_read_u32(np, "vib_tset", &aw8624->info.tset);
	if (val != 0)
		pr_err("%s: vib_tset not found\n", __func__);

	val = of_property_read_u32(np, "vib_r_spare", &aw8624->info.r_spare);
	if (val != 0)
		pr_err("%s: vib_r_spare not found\n", __func__);

	val = of_property_read_u32_array(np, "vib_f0_trace_parameter",
					 f0_trace_parameter,
					 ARRAY_SIZE(f0_trace_parameter));
	if (val != 0)
		pr_err("%s: vib_f0_trace_parameter not found\n", __func__);
	memcpy(aw8624->info.f0_trace_parameter, f0_trace_parameter,
	       sizeof(f0_trace_parameter));
	val =
	    of_property_read_u32_array(np, "vib_bemf_config", bemf_config,
				       ARRAY_SIZE(bemf_config));
	if (val != 0)
		pr_err("%s: vib_bemf_config not found\n", __func__);
	memcpy(aw8624->info.bemf_config, bemf_config, sizeof(bemf_config));

	//val =
	//    of_property_read_u32_array(np, "vib_trig_config", trig_config,
	//                             ARRAY_SIZE(trig_config));
	//if (val != 0)
	//      printk("%s vib_trig_config not found\n", __func__);
	//memcpy(aw8624->info.trig_config, trig_config, sizeof(trig_config));

	val =
	    of_property_read_u32_array(np, "vib_rtp_time", rtp_time,
				       ARRAY_SIZE(rtp_time));
	if (val != 0)
		pr_err("%s: vib_rtp_time not found\n", __func__);
	memcpy(aw8624->info.rtp_time, rtp_time, sizeof(rtp_time));

	val =
	    of_property_read_u32(np, "vib_effect_id_boundary",
				 &aw8624->info.effect_id_boundary);
	if (val != 0)
		pr_err("%s: vib_effect_id_boundary not found\n", __func__);
	val =
	    of_property_read_u32(np, "vib_gain_flag", &aw8624->info.gain_flag);
	if (val != 0)
		pr_err("%s: vib_gain_flag not found\n", __func__);
	val =
	    of_property_read_u32(np, "vib_effect_max",
				 &aw8624->info.effect_max);
	if (val != 0)
		pr_err("%s: vib_effect_max not found\n", __func__);
	val = of_property_read_u32(np, "vib_func_parameter1",
				   &aw8624->info.parameter1);
	if (val != 0)
		pr_err("%s: vib_func_parameter1 not found\n", __func__);

	config->play_rate_us = HAP_PLAY_RATE_US_DEFAULT;
	rc = of_property_read_u32(np, "qcom,play-rate-us", &tmp);
	if (!rc)
		config->play_rate_us = (tmp >= HAP_PLAY_RATE_US_MAX) ?
		    HAP_PLAY_RATE_US_MAX : tmp;

	aw8624->constant.pattern = devm_kcalloc(aw8624->dev,
						HAP_WAVEFORM_BUFFER_MAX,
						sizeof(u8), GFP_KERNEL);
	if (!aw8624->constant.pattern)
		return -ENOMEM;

	tmp = of_get_available_child_count(np);
	aw8624->predefined = devm_kcalloc(aw8624->dev, tmp,
					  sizeof(*aw8624->predefined),
					  GFP_KERNEL);
	if (!aw8624->predefined)
		return -ENOMEM;

	aw8624->effects_count = tmp;
	pr_info("%s: ---%d aw8624->effects_count=%d\n", __func__, __LINE__,
	       aw8624->effects_count);
	for_each_available_child_of_node(np, child_node) {
		effect = &aw8624->predefined[i++];
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
		effect->pattern = devm_kcalloc(aw8624->dev,
					       effect->pattern_length,
					       sizeof(u8), GFP_KERNEL);

		rc = of_property_read_u8_array(child_node, "qcom,wf-pattern",
					       effect->pattern,
					       effect->pattern_length);
		if (rc < 0) {
			printk("%s: Read qcom,wf-pattern property failed !\n",
			       __func__);
		}
		printk
		    ("%s: %d  effect->pattern_length=%d  effect->pattern=%d \n",
		     __func__, __LINE__, effect->pattern_length,
		     (int)effect->pattern);

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


		rc = of_property_read_u32(child_node, "qcom,wf-s-repeat-count",
					  &tmp);
		if (rc < 0) {
			printk("%s: Read  qcom,wf-s-repeat-count failed !\n",
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

	for (j = 0; j < i; j++) {
		printk("%s:       effect_id: %d\n", __func__,
		       aw8624->predefined[j].id);
		printk("%s:       vmax: %d mv\n", __func__,
		       aw8624->predefined[j].vmax_mv);
		printk("%s:        play_rate: %d us\n", __func__,
		       aw8624->predefined[j].play_rate_us);
		for (m = 0; m < aw8624->predefined[j].pattern_length; m++)
			printk("%s:     pattern[%d]: 0x%x\n", __func__, m,
			       aw8624->predefined[j].pattern[m]);
		for (m = 0; m < aw8624->predefined[j].brake_pattern_length; m++)
			printk("%s:     brake_pattern[%d]: 0x%x\n", __func__, m,
			       aw8624->predefined[j].brake[m]);
		printk("%s:         brake_en: %d\n", __func__,
		       aw8624->predefined[j].brake_en);
		printk("%s:        wf_repeat_n: %d\n", __func__,
		       aw8624->predefined[j].wf_repeat_n);
		printk("%s:         wf_s_repeat_n: %d\n", __func__,
		       aw8624->predefined[j].wf_s_repeat_n);
		printk("%s:         lra_auto_res_disable: %d\n", __func__,
		       aw8624->predefined[j].lra_auto_res_disable);
	}
	printk("%s:       aw8624->effects_count: %d\n", __func__,
	       aw8624->effects_count);
	printk("%s:       aw8624->effect_id_boundary: %d\n", __func__,
	       aw8624->info.effect_id_boundary);
	printk("%s:       aw8624->effect_max: %d\n", __func__,
	       aw8624->info.effect_max);
	printk("%s:       aw8624->info.cont_drv_lvl: %d\n", __func__,
	       aw8624->info.cont_drv_lvl);
	printk("%s:       aw8624->info.cont_drv_lvl_ov: %d\n", __func__,
	       aw8624->info.cont_drv_lvl_ov);
	printk("%s:       aw8624->info.gain_flag: %d\n", __func__,
	       aw8624->info.gain_flag);
	for (i = 0; i < 3; i++)
		for (j = 0; j < 5; j++)
			printk
			    ("%s:       aw8624->info.trig_config[%d][%d]: %d\n", __func__,
			     i, j, aw8624->info.trig_config[i][j]);
	for (i = 0; i < 175; i++)
		printk("%s:       aw8624->info.rtp_time[%d]: %d\n", __func__,
		       i, aw8624->info.rtp_time[i]);
	printk("%s:       aw8624->info.parameter1: 0x%x\n", __func__,
	       aw8624->info.parameter1);
	return 0;
}

static inline void get_play_length(struct qti_hap_play_info *play,
				   int *length_us)
{
	struct qti_hap_effect *effect = play->effect;
	int tmp;
	printk("%s  %d enter\n", __func__, __LINE__);

	tmp = effect->pattern_length * effect->play_rate_us;
	tmp *= wf_s_repeat[effect->wf_s_repeat_n];
	tmp *= wf_repeat[effect->wf_repeat_n];
	if (effect->brake_en)
		tmp += effect->play_rate_us * effect->brake_pattern_length;

	*length_us = tmp;
}

static int aw8624_haptics_upload_effect(struct input_dev *dev,
					struct ff_effect *effect,
					struct ff_effect *old)
{
	struct aw8624 *aw8624 = input_get_drvdata(dev);
	struct qti_hap_play_info *play = &aw8624->play;
	s16 data[CUSTOM_DATA_LEN];
	ktime_t rem;
	s64 time_us;
	int ret;

	pr_debug("%s: enter\n", __func__);

	/*for osc calibration */
	if (aw8624->osc_cali_run != 0)
		return 0;

	if (hrtimer_active(&aw8624->timer)) {
		rem = hrtimer_get_remaining(&aw8624->timer);
		time_us = ktime_to_us(rem);
		printk("waiting for playing clear sequence: %lld us\n",
		       time_us);
		usleep_range(time_us, time_us + 100);
	}

	pr_info("%s: effect->type=0x%x,FF_CONSTANT=0x%x,FF_PERIODIC=0x%x\n",
		__func__, effect->type, FF_CONSTANT, FF_PERIODIC);
	aw8624->effect_type = effect->type;
	mutex_lock(&aw8624->lock);
	while (atomic_read(&aw8624->exit_in_rtp_loop)) {
		pr_info("%s:  goint to waiting rtp  exit\n", __func__);
		mutex_unlock(&aw8624->lock);
		ret =
		    wait_event_interruptible(aw8624->stop_wait_q,
					     atomic_read(&aw8624->
							 exit_in_rtp_loop) ==
					     0);
		pr_info("%s:  wakeup \n", __func__);
		if (ret == -ERESTARTSYS) {
			mutex_unlock(&aw8624->lock);
			pr_err("%s: wake up by signal return erro\n", __func__);
			return ret;
		}
		mutex_lock(&aw8624->lock);
	}
	if (aw8624->effect_type == FF_CONSTANT) {
		pr_debug("%s:  effect_type is  FF_CONSTANT! \n", __func__);
		/*cont mode set duration */
		aw8624->duration = effect->replay.length;
		aw8624->activate_mode = AW8624_HAPTIC_ACTIVATE_RAM_LOOP_MODE;
		aw8624->effect_id = aw8624->info.effect_id_boundary;

	} else if (aw8624->effect_type == FF_PERIODIC) {
		if (aw8624->effects_count == 0) {
			mutex_unlock(&aw8624->lock);
			return -EINVAL;
		}

		pr_debug("%s:  effect_type is  FF_PERIODIC! \n", __func__);
		if (copy_from_user(data, effect->u.periodic.custom_data,
				   sizeof(s16) * CUSTOM_DATA_LEN)) {
			mutex_unlock(&aw8624->lock);
			return -EFAULT;
		}

		aw8624->effect_id = data[0];
		pr_debug("%s: aw8624->effect_id =%d \n", __func__,
			 aw8624->effect_id);
		play->vmax_mv = effect->u.periodic.magnitude;	/*vmax level */
		//if (aw8624->info.gain_flag == 1)
		//      play->vmax_mv = AW8624_LIGHT_MAGNITUDE;
		//printk("%s  %d  aw8624->play.vmax_mv = 0x%x\n", __func__, __LINE__, aw8624->play.vmax_mv);

		if (aw8624->effect_id < 0 ||
		    aw8624->effect_id > aw8624->info.effect_max) {
			mutex_unlock(&aw8624->lock);
			return 0;
		}

		if (aw8624->effect_id < aw8624->info.effect_id_boundary) {
			aw8624->activate_mode = AW8624_HAPTIC_ACTIVATE_RAM_MODE;
			pr_info
			    ("%s: aw8624->effect_id=%d , aw8624->activate_mode = %d\n",
			     __func__, aw8624->effect_id,
			     aw8624->activate_mode);
			data[1] = aw8624->predefined[aw8624->effect_id].play_rate_us / 1000000;	/*second data */
			data[2] = aw8624->predefined[aw8624->effect_id].play_rate_us / 1000;	/*millisecond data */
			pr_debug
			    ("%s: aw8624->predefined[aw8624->effect_id].play_rate_us/1000 = %d\n",
			     __func__,
			     aw8624->predefined[aw8624->effect_id].
			     play_rate_us / 1000);
		}
		if (aw8624->effect_id >= aw8624->info.effect_id_boundary) {
			aw8624->activate_mode = AW8624_HAPTIC_ACTIVATE_RTP_MODE;
			pr_info
			    ("%s: aw8624->effect_id=%d , aw8624->activate_mode = %d\n",
			     __func__, aw8624->effect_id,
			     aw8624->activate_mode);
			data[1] = aw8624->info.rtp_time[aw8624->effect_id] / 1000;	/*second data */
			data[2] = aw8624->info.rtp_time[aw8624->effect_id];	/*millisecond data */
			pr_debug("%s: data[1] = %d data[2] = %d\n", __func__,
				 data[1], data[2]);
		}

		if (copy_to_user(effect->u.periodic.custom_data, data,
				 sizeof(s16) * CUSTOM_DATA_LEN)) {
			mutex_unlock(&aw8624->lock);
			return -EFAULT;
		}

	} else {
		pr_err("%s: Unsupported effect type: %d\n", __func__,
		       effect->type);
	}
	mutex_unlock(&aw8624->lock);
	pr_debug("%s	%d	aw8624->effect_type= 0x%x\n", __func__,
		 __LINE__, aw8624->effect_type);
	return 0;
}

static int aw8624_haptics_playback(struct input_dev *dev, int effect_id,
				   int val)
{
	struct aw8624 *aw8624 = input_get_drvdata(dev);
	int rc = 0;
	pr_debug("%s:  %d enter\n", __func__, __LINE__);

	pr_debug("%s: effect_id=%d , val = %d\n", __func__, effect_id, val);
	pr_info("%s: aw8624->effect_id=%d , aw8624->activate_mode = %d\n",
		__func__, aw8624->effect_id, aw8624->activate_mode);

	/*for osc calibration */
	if (aw8624->osc_cali_run != 0)
		return 0;

	if (val > 0)
		aw8624->state = 1;
	if (val <= 0)
		aw8624->state = 0;
	hrtimer_cancel(&aw8624->timer);

	if (aw8624->effect_type == FF_CONSTANT &&
	    aw8624->activate_mode == AW8624_HAPTIC_ACTIVATE_RAM_LOOP_MODE) {
		pr_info("%s: enter cont_mode \n", __func__);
		//schedule_work(&aw8624->vibrator_work);
		queue_work(aw8624->work_queue, &aw8624->vibrator_work);
	} else if (aw8624->effect_type == FF_PERIODIC &&
		   aw8624->activate_mode == AW8624_HAPTIC_ACTIVATE_RAM_MODE) {
		pr_info("%s: enter  ram_mode\n", __func__);
		//schedule_work(&aw8624->vibrator_work)
		queue_work(aw8624->work_queue, &aw8624->vibrator_work);;
	} else if (aw8624->effect_type == FF_PERIODIC &&
		   aw8624->activate_mode == AW8624_HAPTIC_ACTIVATE_RTP_MODE) {
		pr_info("%s: enter  rtp_mode\n", __func__);
		//schedule_work(&aw8624->rtp_work);
		queue_work(aw8624->work_queue, &aw8624->rtp_work);
		//if we are in the play mode, force to exit
		if (val == 0) {
			atomic_set(&aw8624->exit_in_rtp_loop, 1);
		}
	} else {
		/*other mode */
	}

	return rc;
}

static int aw8624_haptics_erase(struct input_dev *dev, int effect_id)
{
	struct aw8624 *aw8624 = input_get_drvdata(dev);
	int rc = 0;

	/*for osc calibration */
	if (aw8624->osc_cali_run != 0)
		return 0;

	pr_debug("%s: enter\n", __func__);
	aw8624->effect_type = 0;
	aw8624->duration = 0;
	return rc;
}

static void aw8624_haptics_set_gain_work_routine(struct work_struct *work)
{
	unsigned char comp_level = 0;
	struct aw8624 *aw8624 =
	    container_of(work, struct aw8624, set_gain_work);

	if (aw8624->new_gain >= 0x7FFF)
		aw8624->level = 0x80;	/*128 */
	else if (aw8624->new_gain <= 0x3FFF)
		aw8624->level = 0x1E;	/*30 */
	else
		aw8624->level = (aw8624->new_gain - 16383) / 128;

	if (aw8624->level < 0x1E)
		aw8624->level = 0x1E;	/*30 */
	pr_info("%s: set_gain queue work, new_gain = %x level = %x \n", __func__,
		aw8624->new_gain, aw8624->level);

	if (aw8624->ram_vbat_comp == AW8624_HAPTIC_RAM_VBAT_COMP_ENABLE
		&& aw8624->vbat)
	{
		pr_debug("%s: ref %d vbat %d ", __func__, AW8624_VBAT_REFER,
				aw8624->vbat);
		comp_level = aw8624->level * AW8624_VBAT_REFER / aw8624->vbat;
		if (comp_level > (128 * AW8624_VBAT_REFER / AW8624_VBAT_MIN)) {
			comp_level = 128 * AW8624_VBAT_REFER / AW8624_VBAT_MIN;
			pr_debug("%s: comp level limit is %d ", comp_level);
		}
		pr_info("%s: enable vbat comp, level = %x comp level = %x", __func__,
			   aw8624->level, comp_level);
		aw8624_i2c_write(aw8624, AW8624_REG_DATDBG, comp_level);
	} else {
		pr_debug("%s: disable compsensation, vbat=%d, vbat_min=%d, vbat_ref=%d",
				__func__, aw8624->vbat, AW8624_VBAT_MIN, AW8624_VBAT_REFER);
		aw8624_i2c_write(aw8624, AW8624_REG_DATDBG, aw8624->level);
	}
}

static void aw8624_haptics_set_gain(struct input_dev *dev, u16 gain)
{
	struct aw8624 *aw8624 = input_get_drvdata(dev);
	pr_debug("%s: enter\n", __func__);
	aw8624->new_gain = gain;
	queue_work(aw8624->work_queue, &aw8624->set_gain_work);
}

static ssize_t aw8624_activate_test_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);

	/* For now nothing to show */
	return snprintf(buf, PAGE_SIZE, "%d\n", aw8624->test_val);
}

static ssize_t aw8624_activate_test_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	aw8624->test_val = val;
	pr_debug("%s: aw8624->test_val=%d\n", __FUNCTION__, aw8624->test_val);

	if (aw8624->test_val == 1) {
		printk("%s  %d  \n", __func__, __LINE__);
		aw8624->duration = 3000;

		aw8624->state = 1;
		aw8624->activate_mode = AW8624_HAPTIC_ACTIVATE_CONT_MODE;
		hrtimer_cancel(&aw8624->timer);
		//schedule_work(&aw8624->vibrator_work);
		queue_work(aw8624->work_queue, &aw8624->vibrator_work);
	}
	if (aw8624->test_val == 2) {
		printk("%s  %d  \n", __func__, __LINE__);
		mutex_lock(&aw8624->lock);
		aw8624_haptic_set_wav_seq(aw8624, 0x00, 0x01);
		aw8624_haptic_set_wav_seq(aw8624, 0x01, 0x01);

		/*step 1:  choose  loop */
		aw8624_haptic_set_wav_loop(aw8624, 0x01, 0x01);
		mutex_unlock(&aw8624->lock);

		aw8624->state = 1;
		aw8624->activate_mode = AW8624_HAPTIC_ACTIVATE_RAM_MODE;
		hrtimer_cancel(&aw8624->timer);
		//schedule_work(&aw8624->vibrator_work);
		queue_work(aw8624->work_queue, &aw8624->vibrator_work);
	}

	if (aw8624->test_val == 3) {	/*Ram instead of Cont */
		aw8624->duration = 10000;

		aw8624->state = 1;
		aw8624->activate_mode = AW8624_HAPTIC_ACTIVATE_CONT_MODE;
		hrtimer_cancel(&aw8624->timer);
		//schedule_work(&aw8624->vibrator_work);
		queue_work(aw8624->work_queue, &aw8624->vibrator_work);
	}

	if (aw8624->test_val == 4) {
		mutex_lock(&aw8624->lock);
		aw8624_haptic_stop(aw8624);
		aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_RAM_MODE);

		aw8624_haptic_set_wav_seq(aw8624, 0x00, 0x01);
		aw8624_haptic_set_wav_seq(aw8624, 0x01, 0x00);

		aw8624_haptic_set_wav_loop(aw8624, 0x01, 0x01);

		aw8624->activate_mode = AW8624_HAPTIC_ACTIVATE_RAM_MODE;
		aw8624->state = 1;
		mutex_unlock(&aw8624->lock);
		hrtimer_cancel(&aw8624->timer);
		//schedule_work(&aw8624->vibrator_work);
		queue_work(aw8624->work_queue, &aw8624->vibrator_work);
	}

	return count;
}

#ifdef ENABLE_PIN_CONTROL
static int select_pin_ctl(struct aw8624 *aw8624, const char *name)
{
	size_t i;
	int rc;

	for (i = 0; i < ARRAY_SIZE(aw8624->pinctrl_state); i++) {
		const char *n = pctl_names[i];

		if (!strncmp(n, name, strlen(n))) {
			rc = pinctrl_select_state(aw8624->aw8624_pinctrl,
						  aw8624->pinctrl_state[i]);
			if (rc)
				pr_info("%s: cannot select '%s'\n", __func__, name);
			else
				pr_info("%s: Selected '%s'\n", __func__, name);
			goto exit;
		}
	}

	rc = -EINVAL;
	pr_info("%s: '%s' not found\n", __func__, name);

exit:
	return rc;
}

static int aw8624_set_interrupt(struct aw8624 *aw8624)
{
	int rc = select_pin_ctl(aw8624, "aw8624_interrupt_active");
	return rc;
}
#endif

static int aw8624_hw_reset(struct aw8624 *aw8624)
{
#ifdef ENABLE_PIN_CONTROL
	int rc = select_pin_ctl(aw8624, "aw8624_reset_active");
	msleep(5);
	rc = select_pin_ctl(aw8624, "aw8624_reset_reset");
	msleep(5);
	rc = select_pin_ctl(aw8624, "aw8624_reset_active");
#endif
	if (!aw8624->enable_pin_control) {
		if (aw8624 && gpio_is_valid(aw8624->reset_gpio)) {
			gpio_set_value_cansleep(aw8624->reset_gpio, 0);
			printk("%s pull down1\n", __func__);
			msleep(5);
			gpio_set_value_cansleep(aw8624->reset_gpio, 1);
			printk("%s pull up1\n", __func__);
			msleep(5);
		} else {
			dev_err(aw8624->dev, "%s:  failed\n", __func__);
		}
	}

	return 0;
}

/*****************************************************
 *
 * check chip id
 *
 *****************************************************/
static int aw8624_read_chipid(struct aw8624 *aw8624)
{
	int ret = -1;
	unsigned char cnt = 0;
	unsigned char reg = 0;

	while (cnt < AW_READ_CHIPID_RETRIES) {
		/* hardware reset */
		aw8624_hw_reset(aw8624);
		ret = aw8624_i2c_read(aw8624, AW8624_REG_ID, &reg);
		if (ret < 0) {
			dev_err(aw8624->dev,
				"%s: failed to read register AW8624_REG_ID: %d\n",
				__func__, ret);
		}
		switch (reg) {
		case 0x24:
			pr_info("%s: aw8624 detected\n", __func__);
			aw8624->chipid = AW8624_ID;
			aw8624_haptic_softreset(aw8624);
			return 0;
		default:
			pr_info("%s: unsupported device revision (0x%x)\n",
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
static ssize_t aw8624_i2c_reg_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);

	unsigned int databuf[2] = { 0, 0 };

	if (2 == sscanf(buf, "%x %x", &databuf[0], &databuf[1])) {
		aw8624_i2c_write(aw8624, (unsigned char)databuf[0],
				 (unsigned char)databuf[1]);
	}

	return count;
}

static ssize_t aw8624_i2c_reg_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	ssize_t len = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;
	for (i = 0; i < AW8624_REG_MAX; i++) {
		if (!(aw8624_reg_access[i] & REG_RD_ACCESS))
			continue;
		aw8624_i2c_read(aw8624, i, &reg_val);
		len +=
		    snprintf(buf + len, PAGE_SIZE - len, "reg:0x%02x=0x%02x \n",
			     i, reg_val);
	}
	return len;
}

static ssize_t aw8624_i2c_ram_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);

	unsigned int databuf[1] = { 0 };

	if (1 == sscanf(buf, "%x", &databuf[0])) {
		if (1 == databuf[0]) {
			aw8624_ram_update(aw8624);
		}
	}

	return count;
}

static ssize_t aw8624_i2c_ram_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	ssize_t len = 0;
	unsigned int i = 0;
	unsigned char reg_val = 0;

	aw8624_haptic_stop(aw8624);
	/* RAMINIT Enable */
	aw8624_i2c_write_bits(aw8624, AW8624_REG_SYSCTRL,
			      AW8624_BIT_SYSCTRL_RAMINIT_MASK,
			      AW8624_BIT_SYSCTRL_RAMINIT_EN);

	aw8624_i2c_write(aw8624, AW8624_REG_RAMADDRH,
			 (unsigned char)(aw8624->ram.base_addr >> 8));
	aw8624_i2c_write(aw8624, AW8624_REG_RAMADDRL,
			 (unsigned char)(aw8624->ram.base_addr & 0x00ff));
	len += snprintf(buf + len, PAGE_SIZE - len, "aw8624_haptic_ram:\n");
	for (i = 0; i < aw8624->ram.len; i++) {
		aw8624_i2c_read(aw8624, AW8624_REG_RAMDATA, &reg_val);
		len += snprintf(buf + len, PAGE_SIZE - len, "0x%02x,", reg_val);
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	/* RAMINIT Disable */
	aw8624_i2c_write_bits(aw8624, AW8624_REG_SYSCTRL,
			      AW8624_BIT_SYSCTRL_RAMINIT_MASK,
			      AW8624_BIT_SYSCTRL_RAMINIT_OFF);

	return len;
}

static ssize_t aw8624_duration_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	ktime_t time_rem;
	s64 time_ms = 0;

	if (hrtimer_active(&aw8624->timer)) {
		time_rem = hrtimer_get_remaining(&aw8624->timer);
		time_ms = ktime_to_ms(time_rem);
	}

	return snprintf(buf, PAGE_SIZE, "%lld\n", time_ms);
}

static ssize_t aw8624_duration_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	/* setting 0 on duration is NOP for now */
	if (val <= 0)
		return count;

	aw8624->duration = val;

	return count;
}

static ssize_t aw8624_activate_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);

	/* For now nothing to show */
	return snprintf(buf, PAGE_SIZE, "%d\n", aw8624->state);
}

static ssize_t aw8624_activate_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val != 0 && val != 1)
		return count;

	pr_debug("%s: value=%d\n", __FUNCTION__, val);

	mutex_lock(&aw8624->lock);
	hrtimer_cancel(&aw8624->timer);

	aw8624->state = val;

	mutex_unlock(&aw8624->lock);
	//schedule_work(&aw8624->vibrator_work);
	queue_work(aw8624->work_queue, &aw8624->vibrator_work);

	return count;
}

static ssize_t aw8624_activate_mode_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "activate_mode=%d\n",
			aw8624->activate_mode);
}

static ssize_t aw8624_activate_mode_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw8624->lock);
	aw8624->activate_mode = val;
	mutex_unlock(&aw8624->lock);
	return count;
}

static ssize_t aw8624_index_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	unsigned char reg_val = 0;
	aw8624_i2c_read(aw8624, AW8624_REG_WAVSEQ1, &reg_val);
	aw8624->index = reg_val;

	return snprintf(buf, PAGE_SIZE, "%d\n", aw8624->index);
}

static ssize_t aw8624_index_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	pr_debug("%s: value=%d\n", __FUNCTION__, val);

	mutex_lock(&aw8624->lock);
	aw8624->index = val;
	aw8624_haptic_set_repeat_wav_seq(aw8624, aw8624->index);
	mutex_unlock(&aw8624->lock);
	return count;
}

static ssize_t aw8624_gain_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw8624->level);
}

static ssize_t aw8624_gain_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	pr_debug("%s: value=%d\n", __FUNCTION__, val);

	mutex_lock(&aw8624->lock);
	aw8624->level = val;
	aw8624_haptic_set_gain(aw8624, aw8624->level);
	mutex_unlock(&aw8624->lock);
	return count;
}

static ssize_t aw8624_seq_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	size_t count = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < AW8624_SEQUENCER_SIZE; i++) {
		aw8624_i2c_read(aw8624, AW8624_REG_WAVSEQ1 + i, &reg_val);
		count += snprintf(buf + count, PAGE_SIZE - count,
				  "seq%d: 0x%02x\n", i + 1, reg_val);
		aw8624->seq[i] |= reg_val;
	}
	return count;
}

static ssize_t aw8624_seq_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	unsigned int databuf[2] = { 0, 0 };

	if (2 == sscanf(buf, "%x %x", &databuf[0], &databuf[1])) {
		pr_debug("%s: seq%d=0x%x\n", __FUNCTION__, databuf[0],
			 databuf[1]);
		mutex_lock(&aw8624->lock);
		aw8624->seq[databuf[0]] = (unsigned char)databuf[1];
		aw8624_haptic_set_wav_seq(aw8624, (unsigned char)databuf[0],
					  aw8624->seq[databuf[0]]);
		mutex_unlock(&aw8624->lock);
	}
	return count;
}

static ssize_t aw8624_loop_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	size_t count = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < AW8624_SEQUENCER_LOOP_SIZE; i++) {
		aw8624_i2c_read(aw8624, AW8624_REG_WAVLOOP1 + i, &reg_val);
		aw8624->loop[i * 2 + 0] = (reg_val >> 4) & 0x0F;
		aw8624->loop[i * 2 + 1] = (reg_val >> 0) & 0x0F;

		count += snprintf(buf + count, PAGE_SIZE - count,
				  "seq%d loop: 0x%02x\n", i * 2 + 1,
				  aw8624->loop[i * 2 + 0]);
		count +=
		    snprintf(buf + count, PAGE_SIZE - count,
			     "seq%d loop: 0x%02x\n", i * 2 + 2,
			     aw8624->loop[i * 2 + 1]);
	}
	return count;
}

static ssize_t aw8624_loop_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	unsigned int databuf[2] = { 0, 0 };

	if (2 == sscanf(buf, "%x %x", &databuf[0], &databuf[1])) {
		pr_debug("%s: seq%d loop=0x%x\n", __FUNCTION__, databuf[0],
			 databuf[1]);
		mutex_lock(&aw8624->lock);
		aw8624->loop[databuf[0]] = (unsigned char)databuf[1];
		aw8624_haptic_set_wav_loop(aw8624, (unsigned char)databuf[0],
					   aw8624->loop[databuf[0]]);
		mutex_unlock(&aw8624->lock);
	}

	return count;
}

static ssize_t aw8624_rtp_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	ssize_t len = 0;
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "rtp play: %d\n",
		     aw8624->rtp_cnt);

	return len;
}

static ssize_t aw8624_rtp_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	aw8624_haptic_stop(aw8624);
	aw8624_haptic_set_rtp_aei(aw8624, false);
	aw8624_interrupt_clear(aw8624);
	if (val < (sizeof(aw8624_rtp_name) / AW8624_RTP_NAME_MAX)) {
		aw8624->rtp_file_num = val;
		if (val) {
			//schedule_work(&aw8624->rtp_work);
			queue_work(aw8624->work_queue, &aw8624->rtp_work);
		}
	} else {
		pr_err("%s: rtp_file_num 0x%02x over max value \n", __func__,
		       aw8624->rtp_file_num);
	}

	return count;
}

static ssize_t aw8624_ram_update_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	//struct aw8624 *aw8624 = dev_get_drvdata(dev);
	ssize_t len = 0;
	len += snprintf(buf + len, PAGE_SIZE - len, "sram update mode\n");
	return len;
}

static ssize_t aw8624_ram_update_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val) {
		aw8624_ram_update(aw8624);
	}
	return count;
}

static ssize_t aw8624_f0_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	ssize_t len = 0;

	mutex_lock(&aw8624->lock);
	aw8624->f0_cali_flag = AW8624_HAPTIC_LRA_F0;
	aw8624_haptic_get_f0(aw8624);
	mutex_unlock(&aw8624->lock);
	len +=
	    //snprintf(buf + len, PAGE_SIZE - len, "aw8624 lra f0 = %d\n",
	    snprintf(buf + len, PAGE_SIZE - len, "%d\n", aw8624->f0);
	return len;
}

static ssize_t aw8624_f0_store(struct device *dev,
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

static ssize_t aw8624_cali_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	ssize_t len = 0;
	mutex_lock(&aw8624->lock);
	aw8624->f0_cali_flag = AW8624_HAPTIC_CALI_F0;
	aw8624_haptic_get_f0(aw8624);
	mutex_unlock(&aw8624->lock);
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "aw8624 cali f0 = %d\n",
		     aw8624->f0);
	return len;
}

static ssize_t aw8624_cali_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	printk("%s %d \n", __func__, __LINE__);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val) {
		mutex_lock(&aw8624->lock);
		aw8624_haptic_f0_calibration(aw8624);
		mutex_unlock(&aw8624->lock);
	}

	return count;
}

static ssize_t aw8624_cont_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	ssize_t len = 0;
	aw8624_haptic_read_cont_f0(aw8624);
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "aw8624 cont f0 = %d\n",
		     aw8624->cont_f0);
	return len;
}

static ssize_t aw8624_cont_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	aw8624_haptic_stop(aw8624);
	if (val) {
		aw8624_haptic_cont(aw8624);
	}
	return count;
}

static ssize_t aw8624_cont_td_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	ssize_t len = 0;
	len +=
	    snprintf(buf + len, PAGE_SIZE - len,
		     "aw8624 cont delay time = 0x%04x\n", aw8624->info.cont_td);
	return len;
}

static ssize_t aw8624_cont_td_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	unsigned int databuf[1] = { 0 };
	if (1 == sscanf(buf, "%x", &databuf[0])) {
		aw8624->info.cont_td = databuf[0];
		aw8624_i2c_write(aw8624, AW8624_REG_TD_H,
				 (unsigned char)(databuf[0] >> 8));
		aw8624_i2c_write(aw8624, AW8624_REG_TD_L,
				 (unsigned char)(databuf[0] >> 0));
	}
	return count;
}

static ssize_t aw8624_cont_drv_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	ssize_t len = 0;
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "aw8624 cont drv level = %d\n",
		     aw8624->info.cont_drv_lvl);
	len +=
	    snprintf(buf + len, PAGE_SIZE - len,
		     "aw8624 cont drv level overdrive= %d\n",
		     aw8624->info.cont_drv_lvl_ov);
	return len;
}

static ssize_t aw8624_cont_drv_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	unsigned int databuf[2] = { 0, 0 };
	if (2 == sscanf(buf, "%d %d", &databuf[0], &databuf[1])) {
		aw8624->info.cont_drv_lvl = databuf[0];
		aw8624_i2c_write(aw8624, AW8624_REG_DRV_LVL,
				 aw8624->info.cont_drv_lvl);
		aw8624->info.cont_drv_lvl_ov = databuf[1];
		aw8624_i2c_write(aw8624, AW8624_REG_DRV_LVL_OV,
				 aw8624->info.cont_drv_lvl_ov);
	}
	return count;
}

static ssize_t aw8624_cont_num_brk_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	ssize_t len = 0;
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "aw8624 cont break num = %d\n",
		     aw8624->info.cont_num_brk);
	return len;
}

static ssize_t aw8624_cont_num_brk_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	unsigned int databuf[1] = { 0 };
	if (1 == sscanf(buf, "%d", &databuf[0])) {
		aw8624->info.cont_num_brk = databuf[0];
		if (aw8624->info.cont_num_brk > 7) {
			aw8624->info.cont_num_brk = 7;
		}
		aw8624_i2c_write_bits(aw8624, AW8624_REG_BEMF_NUM,
				      AW8624_BIT_BEMF_NUM_BRK_MASK,
				      aw8624->info.cont_num_brk);
	}
	return count;
}

static ssize_t aw8624_cont_zc_thr_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	ssize_t len = 0;
	len +=
	    snprintf(buf + len, PAGE_SIZE - len,
		     "aw8624 cont zero cross thr = 0x%04x\n",
		     aw8624->info.cont_zc_thr);
	return len;
}

static ssize_t aw8624_cont_zc_thr_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	unsigned int databuf[1] = { 0 };
	if (1 == sscanf(buf, "%x", &databuf[0])) {
		aw8624->info.cont_zc_thr = databuf[0];
		aw8624_i2c_write(aw8624, AW8624_REG_ZC_THRSH_H,
				 (unsigned char)(databuf[0] >> 8));
		aw8624_i2c_write(aw8624, AW8624_REG_ZC_THRSH_L,
				 (unsigned char)(databuf[0] >> 0));
	}
	return count;
}

static ssize_t aw8624_vbat_monitor_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	ssize_t len = 0;

	mutex_lock(&aw8624->lock);
	aw8624_haptic_stop(aw8624);
	aw8624_haptic_get_vbat(aw8624);
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "vbat=%dmV\n", aw8624->vbat);
	mutex_unlock(&aw8624->lock);

	return len;
}

static ssize_t aw8624_vbat_monitor_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	return count;
}

static ssize_t
aw8624_lra_resistance_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	ssize_t len = 0;
	unsigned int r_lra = 0;

	r_lra = aw8624_lra_resistance_detector(aw8624);

	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", r_lra);
	return len;
}

static ssize_t aw8624_lra_resistance_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	return count;
}

static ssize_t aw8624_prctmode_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	ssize_t len = 0;
	unsigned char reg_val = 0;

	aw8624_i2c_read(aw8624, AW8624_REG_RLDET, &reg_val);

	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "prctmode=%d\n",
		     reg_val & 0x20);
	return len;
}

static ssize_t aw8624_prctmode_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	unsigned int databuf[2] = { 0, 0 };
	unsigned int addr = 0;
	unsigned int val = 0;
	if (2 == sscanf(buf, "%x %x", &databuf[0], &databuf[1])) {
		addr = databuf[0];
		val = databuf[1];
		mutex_lock(&aw8624->lock);
		aw8624_haptic_swicth_motorprotect_config(aw8624, addr, val);
		mutex_unlock(&aw8624->lock);
	}
	return count;
}

static ssize_t aw8624_trig_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	ssize_t len = 0;
	unsigned char i = 0;
	for (i = 0; i < AW8624_TRIG_NUM; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"trig%d: enable=%d, default_level=%d, dual_edge=%d, frist_seq=%d, second_seq=%d\n",
				i + 1, aw8624->trig[i].enable,
				aw8624->trig[i].default_level,
				aw8624->trig[i].dual_edge,
				aw8624->trig[i].frist_seq,
				aw8624->trig[i].second_seq);
	}

	return len;
}

static ssize_t aw8624_trig_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
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
		aw8624->trig[databuf[0]].enable = databuf[1];
		aw8624->trig[databuf[0]].default_level = databuf[2];
		aw8624->trig[databuf[0]].dual_edge = databuf[3];
		aw8624->trig[databuf[0]].frist_seq = databuf[4];
		aw8624->trig[databuf[0]].second_seq = databuf[5];
		mutex_lock(&aw8624->lock);
		//aw8624_haptic_trig_param_config(aw8624);
		//aw8624_haptic_trig_enable_config(aw8624);
		mutex_unlock(&aw8624->lock);
	}
	return count;
}

static ssize_t aw8624_ram_vbat_comp_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	ssize_t len = 0;

	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "ram_vbat_comp=%d\n",
		     aw8624->ram_vbat_comp);

	return len;
}

static ssize_t aw8624_ram_vbat_comp_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw8624->lock);
	if (val) {
		aw8624->ram_vbat_comp = AW8624_HAPTIC_RAM_VBAT_COMP_ENABLE;
	} else {
		aw8624->ram_vbat_comp = AW8624_HAPTIC_RAM_VBAT_COMP_DISABLE;
	}
	mutex_unlock(&aw8624->lock);

	return count;
}

static ssize_t aw8624_osc_cali_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	ssize_t len = 0;

	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "%d\n",
		     aw8624->lra_calib_data);

	return len;
}

static ssize_t aw8624_osc_cali_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	unsigned int val = 0;

	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw8624->lock);
	/*osc calibration flag start,Other behaviors are forbidden */
	aw8624->osc_cali_run = 1;
	if (val == 3) {
		aw8624_rtp_osc_calibration(aw8624);
		aw8624_rtp_trim_lra_calibration(aw8624);
	}
	if (val == 1)
		aw8624_rtp_osc_calibration(aw8624);

	aw8624->osc_cali_run = 0;
	/*osc calibration flag end,Other behaviors are permitted */
	mutex_unlock(&aw8624->lock);

	return count;
}

static ssize_t aw8624_osc_save_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	pr_info("%s: enter\n", __func__);
	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw8624->lra_calib_data = val;
	pr_info("%s: load osa cal: %d\n", __func__, val);

	return count;
}

static ssize_t aw8624_f0_save_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	ssize_t len = 0;

	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "%d\n", aw8624->f0_calib_data);

	return len;
}

static ssize_t aw8624_f0_save_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	pr_info("%s: enter\n", __func__);
	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw8624->f0_calib_data = val;
	pr_info("%s: load f0 cal: %d\n", __func__, val);

	return count;
}

static ssize_t aw8624_effect_id_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "effect_id =%d\n", aw8624->effect_id);
}

static ssize_t aw8624_effect_id_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw8624->lock);
	aw8624->effect_id = val;
	aw8624->play.vmax_mv = AW8624_MEDIUM_MAGNITUDE;
	mutex_unlock(&aw8624->lock);
	return count;
}

static DEVICE_ATTR(effect_id, S_IWUSR | S_IRUGO, aw8624_effect_id_show,
		   aw8624_effect_id_store);
static DEVICE_ATTR(activate_test, S_IWUSR | S_IRUGO, aw8624_activate_test_show,
		   aw8624_activate_test_store);
static DEVICE_ATTR(reg, S_IWUSR | S_IRUGO, aw8624_i2c_reg_show,
		   aw8624_i2c_reg_store);
static DEVICE_ATTR(ram, S_IWUSR | S_IRUGO, aw8624_i2c_ram_show,
		   aw8624_i2c_ram_store);
static DEVICE_ATTR(duration, S_IWUSR | S_IRUGO, aw8624_duration_show,
		   aw8624_duration_store);
static DEVICE_ATTR(activate, S_IWUSR | S_IRUGO, aw8624_activate_show,
		   aw8624_activate_store);
static DEVICE_ATTR(activate_mode, S_IWUSR | S_IRUGO, aw8624_activate_mode_show,
		   aw8624_activate_mode_store);
static DEVICE_ATTR(index, S_IWUSR | S_IRUGO, aw8624_index_show,
		   aw8624_index_store);
static DEVICE_ATTR(gain, S_IWUSR | S_IRUGO, aw8624_gain_show,
		   aw8624_gain_store);
static DEVICE_ATTR(seq, S_IWUSR | S_IRUGO, aw8624_seq_show, aw8624_seq_store);
static DEVICE_ATTR(loop, S_IWUSR | S_IRUGO, aw8624_loop_show,
		   aw8624_loop_store);
static DEVICE_ATTR(rtp, S_IWUSR | S_IRUGO, aw8624_rtp_show, aw8624_rtp_store);
static DEVICE_ATTR(ram_update, S_IWUSR | S_IRUGO, aw8624_ram_update_show,
		   aw8624_ram_update_store);
static DEVICE_ATTR(f0, S_IWUSR | S_IRUGO, aw8624_f0_show, aw8624_f0_store);
static DEVICE_ATTR(cali, S_IWUSR | S_IRUGO, aw8624_cali_show,
		   aw8624_cali_store);
static DEVICE_ATTR(cont, S_IWUSR | S_IRUGO, aw8624_cont_show,
		   aw8624_cont_store);
static DEVICE_ATTR(cont_td, S_IWUSR | S_IRUGO, aw8624_cont_td_show,
		   aw8624_cont_td_store);
static DEVICE_ATTR(cont_drv, S_IWUSR | S_IRUGO, aw8624_cont_drv_show,
		   aw8624_cont_drv_store);
static DEVICE_ATTR(cont_num_brk, S_IWUSR | S_IRUGO, aw8624_cont_num_brk_show,
		   aw8624_cont_num_brk_store);
static DEVICE_ATTR(cont_zc_thr, S_IWUSR | S_IRUGO, aw8624_cont_zc_thr_show,
		   aw8624_cont_zc_thr_store);
static DEVICE_ATTR(vbat_monitor, S_IWUSR | S_IRUGO, aw8624_vbat_monitor_show,
		   aw8624_vbat_monitor_store);
static DEVICE_ATTR(lra_resistance, S_IWUSR | S_IRUGO,
		   aw8624_lra_resistance_show, aw8624_lra_resistance_store);
static DEVICE_ATTR(prctmode, S_IWUSR | S_IRUGO, aw8624_prctmode_show,
		   aw8624_prctmode_store);
static DEVICE_ATTR(trig, S_IWUSR | S_IRUGO, aw8624_trig_show,
		   aw8624_trig_store);
static DEVICE_ATTR(ram_vbat_comp, S_IWUSR | S_IRUGO, aw8624_ram_vbat_comp_show,
		   aw8624_ram_vbat_comp_store);
static DEVICE_ATTR(osc_cali, S_IWUSR | S_IRUGO, aw8624_osc_cali_show,
		   aw8624_osc_cali_store);
static DEVICE_ATTR(f0_save, S_IWUSR | S_IRUGO, aw8624_f0_save_show,
		   aw8624_f0_save_store);
static DEVICE_ATTR(osc_save, S_IWUSR | S_IRUGO, aw8624_osc_cali_show,
		   aw8624_osc_save_store);

static struct attribute *aw8624_vibrator_attributes[] = {
	&dev_attr_effect_id.attr,
	&dev_attr_reg.attr,
	&dev_attr_ram.attr,
	&dev_attr_activate_test.attr,
	&dev_attr_duration.attr,
	&dev_attr_activate.attr,
	&dev_attr_activate_mode.attr,
	&dev_attr_index.attr,
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
	&dev_attr_prctmode.attr,
	&dev_attr_trig.attr,
	&dev_attr_ram_vbat_comp.attr,
	&dev_attr_osc_cali.attr,
	&dev_attr_osc_save.attr,
	&dev_attr_f0_save.attr,
	NULL
};

static struct attribute_group aw8624_vibrator_attribute_group = {
	.attrs = aw8624_vibrator_attributes
};

/******************************************************
 *
 * i2c driver
 *
 ******************************************************/
static int
aw8624_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct aw8624 *aw8624;
	struct input_dev *input_dev;
	int rc = 0, effect_count_max;
	struct ff_device *ff;
	struct device_node *np = i2c->dev.of_node;
	int irq_flags = 0;
	int ret = -1;
#ifdef ENABLE_PIN_CONTROL
	int i;
#endif

	pr_info("%s:      enter\n", __func__);
	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "check_functionality failed\n");
		return -EIO;
	}

	aw8624 = devm_kzalloc(&i2c->dev, sizeof(struct aw8624), GFP_KERNEL);
	if (aw8624 == NULL)
		return -ENOMEM;

	input_dev = devm_input_allocate_device(&i2c->dev);
	if (!input_dev)
		return -ENOMEM;

	aw8624->dev = &i2c->dev;
	aw8624->i2c = i2c;
	device_init_wakeup(aw8624->dev, true);
	i2c_set_clientdata(i2c, aw8624);

	/* aw8624 rst & int */
	if (np) {
		ret = aw8624_parse_dt(&i2c->dev, aw8624, np);
		if (ret) {
			dev_err(&i2c->dev,
				"%s: failed to parse device tree node\n",
				__func__);
			goto err_parse_dt;
		}
	} else {
		aw8624->reset_gpio = -1;
		aw8624->irq_gpio = -1;
	}
	aw8624->enable_pin_control = 0;
#ifdef ENABLE_PIN_CONTROL
	aw8624->aw8624_pinctrl = devm_pinctrl_get(&i2c->dev);
	if (IS_ERR(aw8624->aw8624_pinctrl)) {
		if (PTR_ERR(aw8624->aw8624_pinctrl) == -EPROBE_DEFER) {
			printk("pinctrl not ready\n");
			rc = -EPROBE_DEFER;
			return rc;
		}
		printk("Target does not use pinctrl\n");
		aw8624->aw8624_pinctrl = NULL;
		rc = -EINVAL;
		return rc;
	}
	for (i = 0; i < ARRAY_SIZE(aw8624->pinctrl_state); i++) {
		const char *n = pctl_names[i];
		struct pinctrl_state *state =
		    pinctrl_lookup_state(aw8624->aw8624_pinctrl, n);
		if (IS_ERR(state)) {
			printk("cannot find '%s'\n", n);
			rc = -EINVAL;
			//goto exit;
		}
		pr_info("%s: found pin control %s\n", __func__, n);
		aw8624->pinctrl_state[i] = state;
		aw8624->enable_pin_control = 1;
		aw8624_set_interrupt(aw8624);
	}
#endif
	if (!aw8624->enable_pin_control) {
		if (gpio_is_valid(aw8624->reset_gpio)) {
			ret =
			    devm_gpio_request_one(&i2c->dev, aw8624->reset_gpio,
						  GPIOF_OUT_INIT_LOW,
						  "aw8624_rst");
			if (ret) {
				dev_err(&i2c->dev, "%s: rst request failed\n",
					__func__);
				goto err_reset_gpio_request;
			}
		}
	}

	if (gpio_is_valid(aw8624->irq_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, aw8624->irq_gpio,
					    GPIOF_DIR_IN, "aw8624_int");
		if (ret) {
			dev_err(&i2c->dev, "%s: int request failed\n",
				__func__);
			goto err_irq_gpio_request;
		}
	}

	ret = aw8624_read_chipid(aw8624);
	if (ret != 0) {
		dev_err(&i2c->dev, "%s: aw8624_read_chipid failed ret=%d\n",
			__func__, ret);
		goto err_id;
	}

	if (gpio_is_valid(aw8624->irq_gpio) &&
	    !(aw8624->flags & AW8624_FLAG_SKIP_INTERRUPTS)) {
		/* register irq handler */
		aw8624_interrupt_setup(aw8624);
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
		ret = devm_request_threaded_irq(&i2c->dev,
						gpio_to_irq(aw8624->irq_gpio),
						NULL, aw8624_irq, irq_flags,
						"aw8624", aw8624);
		pr_info("%s: aw8624_irq success.\n", __func__);
		if (ret != 0) {
			dev_err(&i2c->dev, "%s: failed to request IRQ %d: %d\n",
				__func__, gpio_to_irq(aw8624->irq_gpio), ret);
			goto err_irq;
		}
	} else {
		dev_info(&i2c->dev, "%s skipping IRQ registration\n", __func__);
		/* disable feature support if gpio was invalid */
		aw8624->flags |= AW8624_FLAG_SKIP_INTERRUPTS;
		pr_err("%s: aw8624_irq failed.\n", __func__);
	}

	hrtimer_init(&aw8624->stop_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw8624->stop_timer.function = qti_hap_stop_timer;
	hrtimer_init(&aw8624->hap_disable_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	aw8624->hap_disable_timer.function = qti_hap_disable_timer;

	input_dev->name = "aw8624_haptic";
	input_set_drvdata(input_dev, aw8624);
	aw8624->input_dev = input_dev;
	input_set_capability(input_dev, EV_FF, FF_CONSTANT);
	input_set_capability(input_dev, EV_FF, FF_GAIN);
	if (aw8624->effects_count != 0) {
		input_set_capability(input_dev, EV_FF, FF_PERIODIC);
		input_set_capability(input_dev, EV_FF, FF_CUSTOM);
	}

	if (aw8624->effects_count + 1 > FF_EFFECT_COUNT_MAX)
		effect_count_max = aw8624->effects_count + 1;
	else
		effect_count_max = FF_EFFECT_COUNT_MAX;
	rc = input_ff_create(input_dev, effect_count_max);
	if (rc < 0) {
		dev_err(aw8624->dev, "create FF input device failed, rc=%d\n",
			rc);
		return rc;
	}
	aw8624->work_queue =
	    create_singlethread_workqueue("aw8624_vibrator_work_queue");
	if (!aw8624->work_queue) {
		dev_err(&i2c->dev,
			"%s: Error creating aw8624_vibrator_work_queue\n",
			__func__);
		goto err_sysfs;
	}
	INIT_WORK(&aw8624->set_gain_work, aw8624_haptics_set_gain_work_routine);
	aw8624_vibrator_init(aw8624);
	aw8624_haptic_init(aw8624);
	aw8624_ram_init(aw8624);

	ff = input_dev->ff;
	ff->upload = aw8624_haptics_upload_effect;
	ff->playback = aw8624_haptics_playback;
	ff->erase = aw8624_haptics_erase;
	ff->set_gain = aw8624_haptics_set_gain;
	rc = input_register_device(input_dev);
	if (rc < 0) {
		dev_err(aw8624->dev, "register input device failed, rc=%d\n",
			rc);
		goto destroy_ff;
	}

	dev_set_drvdata(&i2c->dev, aw8624);
	ret =
	    sysfs_create_group(&i2c->dev.kobj,
			       &aw8624_vibrator_attribute_group);
	if (ret < 0) {
		dev_info(&i2c->dev, "%s error creating sysfs attr files\n",
			 __func__);
		goto err_sysfs;
	}

	g_aw8624 = aw8624;

	pr_info("%s: probe completed successfully!\n", __func__);

	return 0;

err_sysfs:
	devm_free_irq(&i2c->dev, gpio_to_irq(aw8624->irq_gpio), aw8624);
destroy_ff:
	input_ff_destroy(aw8624->input_dev);
err_irq:
err_id:
	if (gpio_is_valid(aw8624->irq_gpio))
		devm_gpio_free(&i2c->dev, aw8624->irq_gpio);
err_irq_gpio_request:
	if (gpio_is_valid(aw8624->reset_gpio))
		devm_gpio_free(&i2c->dev, aw8624->reset_gpio);
err_reset_gpio_request:
err_parse_dt:
	device_init_wakeup(aw8624->dev, false);
	devm_kfree(&i2c->dev, aw8624);
	aw8624 = NULL;
	return ret;
}

static int aw8624_i2c_remove(struct i2c_client *i2c)
{
	struct aw8624 *aw8624 = i2c_get_clientdata(i2c);

	pr_info("%s: enter\n", __func__);

	sysfs_remove_group(&i2c->dev.kobj, &aw8624_vibrator_attribute_group);

	devm_free_irq(&i2c->dev, gpio_to_irq(aw8624->irq_gpio), aw8624);

	if (gpio_is_valid(aw8624->irq_gpio))
		devm_gpio_free(&i2c->dev, aw8624->irq_gpio);
	if (gpio_is_valid(aw8624->reset_gpio))
		devm_gpio_free(&i2c->dev, aw8624->reset_gpio);
	if (aw8624 != NULL) {
		flush_workqueue(aw8624->work_queue);
		destroy_workqueue(aw8624->work_queue);
	}
	device_init_wakeup(aw8624->dev, false);
	devm_kfree(&i2c->dev, aw8624);
	aw8624 = NULL;

	return 0;
}

static const struct i2c_device_id aw8624_i2c_id[] = {
	{AW8624_I2C_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, aw8624_i2c_id);

static struct of_device_id aw8624_dt_match[] = {
	{.compatible = "awinic,aw8624_haptic"},
	{},
};

static struct i2c_driver aw8624_i2c_driver = {
	.driver = {
		   .name = AW8624_I2C_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(aw8624_dt_match),
		   },
	.probe = aw8624_i2c_probe,
	.remove = aw8624_i2c_remove,
	.id_table = aw8624_i2c_id,
};

static int __init aw8624_i2c_init(void)
{
	int ret = 0;

	pr_info("%s: aw8624 driver version %s\n", __func__, AW8624_VERSION);

	ret = i2c_add_driver(&aw8624_i2c_driver);
	if (ret) {
		pr_err("%s: fail to add aw8624 device into i2c\n");
		return ret;
	}

	return 0;
}

module_init(aw8624_i2c_init);

static void __exit aw8624_i2c_exit(void)
{
	i2c_del_driver(&aw8624_i2c_driver);
}

module_exit(aw8624_i2c_exit);

MODULE_DESCRIPTION("AW8624 Haptic Driver");
MODULE_LICENSE("GPL v2");
