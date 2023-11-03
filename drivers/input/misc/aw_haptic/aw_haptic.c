/*
 * aw_haptic.c
 *
 *
 *
 * Copyright (c) 2021 AWINIC Technology CO., LTD
 *
 * Author: <chelvming@awinic.com.cn>
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
#include  "ringbuffer.h"
#include "aw_haptic.h"
#include "aw8697.h"
#include "aw86927.h"

#ifdef AAC_RICHTAP_SUPPORT
#include <linux/mman.h>
struct aw86927 *g_aw_haptic = NULL;
#endif

#define AW_DRIVER_VERSION		"v0.5.1.1"

/******************************************************
 *
 * Value
 *
 ******************************************************/
#ifdef	ENABLE_PIN_CONTROL
static const char *const pctl_names[] = {
	"awinic_reset_reset",
	"awinic_reset_active",
	"awinic_interrupt_active",
};
#endif

char *awinic_ram_name = "aw8697_haptic.bin";

#ifdef TEST_RTP
char awinic_rtp_name[][AWINIC_RTP_NAME_MAX] = {
	{"aw86927_rtp_1.bin"},
};
#else
char awinic_rtp_name[][AWINIC_RTP_NAME_MAX] = {
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
#endif
int awinic_rtp_name_len = sizeof(awinic_rtp_name) / AWINIC_RTP_NAME_MAX;
int CUSTOME_WAVE_ID;

/******************************************************
 *
 * i2c read/write
 *
 ******************************************************/
static int awinic_i2c_read(struct awinic *awinic,
		unsigned char reg_addr, unsigned char *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_smbus_read_byte_data(awinic->i2c, reg_addr);
		if (ret < 0) {
			aw_err("%s: i2c_read cnt=%d error=%d\n",
				__func__, cnt, ret);
		} else {
			*reg_data = ret;
			break;
		}
		cnt++;
		msleep(AW_I2C_RETRY_DELAY);
	}

	return ret;
}

static int awinic_i2c_write(struct awinic *awinic,
		 unsigned char reg_addr, unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret =
		i2c_smbus_write_byte_data(awinic->i2c, reg_addr, reg_data);
		if (ret < 0) {
			aw_err("%s: i2c_write cnt=%d error=%d\n",
				__func__, cnt, ret);
		} else {
			break;
		}
		cnt++;
		msleep(AW_I2C_RETRY_DELAY);
	}

	return ret;
}

#ifdef ENABLE_PIN_CONTROL
static int awinic_select_pin_ctl(struct awinic *awinic, const char *name)
{
	size_t i;
	int rc;

	for (i = 0; i < ARRAY_SIZE(awinic->pinctrl_state); i++) {
		const char *n = pctl_names[i];

		if (!strncmp(n, name, strlen(n))) {
			rc = pinctrl_select_state(awinic->awinic_pinctrl,
						  awinic->pinctrl_state[i]);
			if (rc)
				aw_err("%s: cannot select '%s'\n", __func__,
				       name);
			else
				aw_err("%s: Selected '%s'\n", __func__, name);
			goto exit;
		}
	}

	rc = -EINVAL;
	aw_info("%s: '%s' not found\n", __func__, name);

exit:
	return rc;
}

static int awinic_set_interrupt(struct awinic *awinic)
{
	int rc = awinic_select_pin_ctl(awinic, "awinic_interrupt_active");
	return rc;
}
#endif

static int awinic_sw_reset(struct awinic *awinic)
{
	aw_info("%s enter\n", __func__);
	awinic_i2c_write(awinic, 0x00, 0xAA);
	usleep_range(2000, 2500);
	return 0;
}

static int awinic_control_rest_pin(struct awinic *awinic)
{
	int ret = 0;

	ret = awinic_select_pin_ctl(awinic, "awinic_reset_active");
	if (ret < 0) {
		aw_err("%s select reset failed!\n", __func__);
		return ret;
	}
	usleep_range(5000, 5500);
	ret = awinic_select_pin_ctl(awinic, "awinic_reset_reset");
	if (ret < 0) {
		aw_err("%s select reset failed!\n", __func__);
		return ret;
	}
	usleep_range(5000, 5500);
	ret = awinic_select_pin_ctl(awinic, "awinic_reset_active");
	usleep_range(8000, 8500);
	if (ret < 0) {
		aw_err("%s select reset failed!\n", __func__);
		return ret;
	}
	return 0;
}

static int awinic_hw_reset(struct awinic *awinic)
{
	int rc = 0;

	if (!awinic->enable_pin_control) {
		if (awinic && gpio_is_valid(awinic->reset_gpio)) {
			gpio_set_value_cansleep(awinic->reset_gpio, 0);
			aw_info("%s pull down1\n", __func__);
			usleep_range(5000, 5500);
			gpio_set_value_cansleep(awinic->reset_gpio, 1);
			aw_info("%s pull up1\n", __func__);
			usleep_range(8000, 8500);
		} else {
			aw_err("%s:  failed\n", __func__);
		}
	} else {
		rc = awinic_control_rest_pin(awinic);
		if (rc < 0)
			return rc;
	}

	return 0;
}

/******************************************************
 *
 * check chip id
 *
 ******************************************************/
static int awinic_read_chipid(struct awinic *awinic, unsigned int *reg_val)
{
	unsigned char value[2] = {0};
	unsigned char chipid_addr[2] = {AW_REG_IDH, AW_REG_IDL};
	unsigned char i = 0;
	int ret = -1;

	aw_info("%s enter!\n", __func__);

	/* try to read aw8697 chip id */
	ret = awinic_i2c_read(awinic, 0x00, &value[0]);
	if (ret < 0)
		return ret;
	if (value[0] == AW8697_CHIP_ID) {
		*reg_val = value[0];
		return 0;
	}
	/* try to read aw86927 chip id */
	for (i = 0; i < 2; i++) {
		ret = awinic_i2c_read(awinic, chipid_addr[i], &value[i]);
		if (ret < 0)
			return ret;
	}
	*reg_val = value[0] << 8 | value[1];
	return 0;

}

static int awinic_parse_chipid(struct awinic *awinic)
{
	int ret = -1;
	unsigned char cnt = 0;
	unsigned int reg_val = 0;


	while (cnt < AW_READ_CHIPID_RETRIES) {
		/* hardware reset */
		ret = awinic_hw_reset(awinic);
		if (ret < 0) {
			aw_err("%s: hardware reset failed!\n", __func__);
			break;
		}

		ret = awinic_read_chipid(awinic, &reg_val);
		if (ret < 0) {
			aw_err("%s: failed to read AW_REG_ID: %d\n",
				__func__, ret);
			break;
		}

		switch (reg_val) {
		case AW8697_CHIP_ID:
			aw_info("%s aw8697 detected\n", __func__);
			awinic->name = AW8697;
			awinic_sw_reset(awinic);
			return 0;
		case AW86927_CHIP_ID:
			aw_info("%s aw86927 detected\n", __func__);
			awinic->name = AW86927;
			awinic_sw_reset(awinic);
			return 0;
		default:
			aw_info("%s unsupported device revision (0x%x)\n",
				__func__, reg_val);
			break;
		}
		cnt++;

		usleep_range(2000, 3000);
	}

	return -EINVAL;
}

/******************************************************
 *
 * parse dts
 *
 ******************************************************/
static int awinic_parse_dt(struct awinic *awinic, struct device *dev,
		       struct device_node *np)
{
	awinic->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (awinic->reset_gpio >= 0) {
		aw_err("%s: reset gpio provided ok\n", __func__);
	} else {
		awinic->reset_gpio = -1;
		aw_err("%s: no reset gpio provided, will not HW reset device\n",
			__func__);
		return -ERANGE;
	}

	awinic->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (awinic->irq_gpio < 0) {
		aw_err("%s: no irq gpio provided.\n", __func__);
		awinic->IsUsedIRQ = false;
	} else {
		aw_err("%s: irq gpio provided ok.\n", __func__);
		awinic->IsUsedIRQ = true;
	}

	return 0;
}

/******************************************************
 *
 * Richtap2.0
 *
 ******************************************************/

#ifdef AAC_RICHTAP_SUPPORT
void richtap_clean_buf(struct aw86927 *aw86927, int status)
{
    struct mmap_buf_format *opbuf = aw86927->start_buf;
    int i = 0;

    for(i = 0; i < RICHTAP_MMAP_BUF_SUM; i++)
    {
	    memset(opbuf->data, 0, RICHTAP_MMAP_BUF_SIZE);
        opbuf->status = status;
        opbuf = opbuf->kernel_next;
    }
}

void richtap_update_fifo_data(struct aw86927 *aw86927, uint32_t fifo_len)
{
        int32_t samples_left = 0, pos = 0, retry = 3;

        do
        {
            if(aw86927->curr_buf->status == MMAP_BUF_DATA_VALID) 
            {
                samples_left = aw86927->curr_buf->length - aw86927->pos;
                if(samples_left < fifo_len)
                {   
                    memcpy(&aw86927->rtp_ptr[pos], &aw86927->curr_buf->data[aw86927->pos], samples_left);
                    pos += samples_left;
                    fifo_len -= samples_left;
                    aw86927->curr_buf->status = MMAP_BUF_DATA_INVALID;
                    aw86927->curr_buf->length = 0;
                    aw86927->curr_buf = aw86927->curr_buf->kernel_next;
                    aw86927->pos = 0;
                }
                else
                {
                    memcpy(&aw86927->rtp_ptr[pos], &aw86927->curr_buf->data[aw86927->pos], fifo_len);
                    aw86927->pos += fifo_len;
                    pos += fifo_len;
                    fifo_len = 0;
                }
            }
            else if(aw86927->curr_buf->status == MMAP_BUF_DATA_FINISHED)
            {
                break;
            }
            else
            {
                if(retry-- <= 0)
                {
                    pr_info("invalid data\n");
                    break;
                }
                else
                {
                    usleep_range(1000,1000);
                }
            }
        } while(fifo_len > 0 && atomic_read(&aw86927->richtap_rtp_mode));

		pr_debug("update fifo len %d\n", pos);
		aw86927_write_rtp_data(aw86927, aw86927->rtp_ptr, pos);
}

bool richtap_rtp_start(struct aw86927 *aw86927)
{
    int cnt = 200;
    bool rtp_work_flag = false;
    uint8_t reg_val = 0;
	unsigned char glb_state = 0;

    mutex_lock(&aw86927->lock);
	aw86927_haptic_play_mode(aw86927,AW86927_RTP_MODE);
	aw86927_haptic_play_go(aw86927);
    usleep_range(2000, 2000);

    while (cnt) {
		aw86927_haptic_get_glb_state(aw86927,&glb_state);
        if ((glb_state & AW_GLBRD_STATE_MASK) == AW_STATE_RTP) 
        {
            cnt = 0;
            rtp_work_flag = true;
            pr_info("%s: RTP_GO! glb_state=0x08\n", __func__);
        } 
        else 
        {
            cnt--;
            pr_debug("%s: wait for RTP_GO, glb_state=0x%02X\n", __func__, reg_val);
        }
        usleep_range(2000, 2500);
    }

    if(rtp_work_flag == false)
    {
         aw86927_haptic_stop(aw86927);
    }

    mutex_unlock(&aw86927->lock);

    return rtp_work_flag;
}

void richtap_rtp_work(struct work_struct *work)
{
	struct aw86927 *aw86927 = container_of(work, struct aw86927, richtap_rtp_work);
	uint32_t retry = 0, tmp_len = 0;

    aw86927->curr_buf = aw86927->start_buf;

    do {
        if(aw86927->curr_buf->status == MMAP_BUF_DATA_VALID)
        {
            if((tmp_len + aw86927->curr_buf->length) > aw86927->ram.base_addr)
            {
                memcpy(&aw86927->rtp_ptr[tmp_len], aw86927->curr_buf->data, (aw86927->ram.base_addr - tmp_len));
                aw86927->pos = aw86927->ram.base_addr - tmp_len;
                tmp_len = aw86927->ram.base_addr;
            }
            else
            {
                memcpy(&aw86927->rtp_ptr[tmp_len], aw86927->curr_buf->data, aw86927->curr_buf->length);
                tmp_len += aw86927->curr_buf->length;
                aw86927->curr_buf->status = MMAP_BUF_DATA_INVALID;
                aw86927->curr_buf->length = 0;
                aw86927->pos = 0;
                aw86927->curr_buf = aw86927->curr_buf->kernel_next;
            }
        }
        else if(aw86927->curr_buf->status == MMAP_BUF_DATA_FINISHED)
        {
            break;
        }
        else
        {
            msleep(1);
        }
    } while(tmp_len < aw86927->ram.base_addr && retry++ < 30);

	pr_debug("rtp tm_len = %d, retry = %d, aw_haptic->ram.base_addr = %d\n", tmp_len, retry, aw86927->ram.base_addr);

    if(richtap_rtp_start(aw86927))
    {  
         aw86927_write_rtp_data(aw86927, aw86927->rtp_ptr, tmp_len);
        aw86927_haptic_set_rtp_aei(aw86927, true);
		aw86927_interrupt_clear(aw86927);
        atomic_set(&aw86927->richtap_rtp_mode, true);
    }
}
#ifdef AW_DOUBLE
int haptic_left_flag(int unuse)
{
	(void)unuse;
	return LEFT_FOPS;
}
int haptic_right_flag(int unuse)
{
	(void)unuse;
	return RIGHT_FOPS;
}
#endif

int richtap_file_open(struct inode *inode, struct file *file)
{
	if (!try_module_get(THIS_MODULE))
		return -ENODEV;
#ifdef AW_DOUBLE
	if(file->f_op->check_flags(1) == LEFT_FOPS){
		file->private_data = (void *)left;
	}else if(file->f_op->check_flags(1) == RIGHT_FOPS){
		file->private_data = (void *)right;
	}else{
		file->private_data = (void *)NULL;
		pr_err("%s: file private_data err!", __func__);
	}
#else
    file->private_data = (void *)g_aw_haptic;
#endif
	return 0;
}

int richtap_file_release(struct inode *inode, struct file *file)
{
    module_put(THIS_MODULE);

	return 0;
}

long richtap_file_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct aw86927 *aw86927 = (struct aw86927 *)filp->private_data;
	int ret = 0, tmp;

	pr_info("%s: cmd=0x%x, arg=0x%lx\n", __func__, cmd, arg);

    aw86927->ram.base_addr = 0x800;   /* don't merge this line for oem */
	switch(cmd)
	{
	    case RICHTAP_GET_HWINFO:
            tmp = RICHTAP_AW_8697;
            if(copy_to_user((void __user *)arg, &tmp, sizeof(int)))
                ret = -EFAULT;
            break;
	    case RICHTAP_RTP_MODE:
            mutex_lock(&aw86927->lock);
            aw86927_haptic_stop(aw86927);
            mutex_unlock(&aw86927->lock);
            if(copy_from_user(aw86927->rtp_ptr, (void __user *)arg, RICHTAP_MMAP_BUF_SIZE * RICHTAP_MMAP_BUF_SUM))
            {
                ret = -EFAULT;
                break;
            }
            tmp = *((int*)aw86927->rtp_ptr);
            if(tmp > (RICHTAP_MMAP_BUF_SIZE * RICHTAP_MMAP_BUF_SUM - 4))
            {
                dev_err(aw86927->dev, "rtp mode date len error %d\n", tmp);
                ret = -EINVAL;
                break;
            }
			aw86927_haptic_upload_lra(aw86927,AW86927_OSC_CALI);
            aw86927_haptic_set_bst_vol(aw86927, aw86927->vmax);
            if(richtap_rtp_start(aw86927))
            {
                aw86927_write_rtp_data(aw86927, &aw86927->rtp_ptr[4], tmp);
            }
            break;
	    case RICHTAP_OFF_MODE:
		    break;
	    case RICHTAP_GET_F0:
		    tmp = aw86927->f0;
		    if(copy_to_user((void __user *)arg, &tmp, sizeof(uint32_t)))
			    ret = -EFAULT;
		    break;
	    case RICHTAP_SETTING_GAIN:
		    if(arg > 0x80)
				arg = 0x80;
            aw86927_haptic_set_gain(aw86927, (uint8_t)arg);
		    break;
	    case RICHTAP_STREAM_MODE:
            richtap_clean_buf(aw86927, MMAP_BUF_DATA_INVALID);
            mutex_lock(&aw86927->lock);
			aw86927_interrupt_clear(aw86927);
            aw86927_haptic_stop(aw86927);
            mutex_unlock(&aw86927->lock);
			aw86927_haptic_set_rtp_aei(aw86927, false);
			atomic_set(&aw86927->richtap_rtp_mode, false);
            aw86927_haptic_upload_lra(aw86927, AW86927_OSC_CALI);
            aw86927_haptic_set_bst_vol(aw86927, aw86927->vmax);
            schedule_work(&aw86927->richtap_rtp_work);
            break;
        case RICHTAP_STOP_MODE:
            richtap_clean_buf(aw86927, MMAP_BUF_DATA_FINISHED);
            mutex_lock(&aw86927->lock);
			aw86927_interrupt_clear(aw86927);
            aw86927_haptic_stop(aw86927);
            mutex_unlock(&aw86927->lock);
            aw86927_haptic_set_rtp_aei(aw86927, false);
            atomic_set(&aw86927->richtap_rtp_mode, false);  
            break;
	    default:
		    dev_err(aw86927->dev, "%s, unknown cmd\n", __func__);
		    break;
	}

	return ret;
}

int richtap_file_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long phys;
	struct aw86927 *aw86927 = (struct aw86927 *)filp->private_data;
	int ret = 0;

#if LINUX_VERSION_CODE > KERNEL_VERSION(4,7,0)
	//only accept PROT_READ, PROT_WRITE and MAP_SHARED from the API of mmap
	vm_flags_t vm_flags = calc_vm_prot_bits(PROT_READ|PROT_WRITE, 0) | calc_vm_flag_bits(MAP_SHARED);
	vm_flags |= current->mm->def_flags | VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC| VM_SHARED | VM_MAYSHARE;
	if(vma && (pgprot_val(vma->vm_page_prot) != pgprot_val(vm_get_page_prot(vm_flags))))
	    return -EPERM;

	if(vma && ((vma->vm_end - vma->vm_start) != (PAGE_SIZE << RICHTAP_MMAP_PAGE_ORDER)))
	    return -ENOMEM;
#endif 
	phys = virt_to_phys(aw86927->start_buf);

	ret = remap_pfn_range(vma, vma->vm_start, (phys >> PAGE_SHIFT), (vma->vm_end - vma->vm_start), vma->vm_page_prot);
	if(ret)
	{
	    dev_err(aw86927->dev, "Error mmap failed\n");
	    return ret;
	}

	return ret;
}

ssize_t richtap_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	return 0;
}

ssize_t richtap_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	return count;
}

struct file_operations left_fops = {
	.owner = THIS_MODULE,
	.read = richtap_read,
	.write = richtap_write,
	.mmap = richtap_file_mmap,
	.unlocked_ioctl = richtap_file_unlocked_ioctl,
	.open = richtap_file_open,
	.release = richtap_file_release,
#ifdef AW_DOUBLE
	.check_flags = haptic_left_flag,
#endif
};

#ifdef AW_DOUBLE
struct file_operations right_fops = {
	.owner = THIS_MODULE,
	.read = richtap_read,
	.write = richtap_write,
	.mmap = richtap_file_mmap,
	.unlocked_ioctl = richtap_file_unlocked_ioctl,
	.open = richtap_file_open,
	.release = richtap_file_release,
	.check_flags = haptic_right_flag,
};
#endif

struct miscdevice richtap_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "aw8697_haptic",
    .fops = &left_fops,
};

#ifdef AW_DOUBLE
struct miscdevice richtap_misc_x = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "aw8697x_haptic",
    .fops = &right_fops,
};
#endif
#endif
/******************************************************
 *
 * i2c driver
 *
 ******************************************************/
static int awinic_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct awinic *awinic;
	struct input_dev *input_dev;
	struct ff_device *ff;
	struct device_node *np = i2c->dev.of_node;
	int effect_count_max;
	int irq_flags = 0;
	int ret = -1;
	int rc = 0;

#ifdef ENABLE_PIN_CONTROL
	int i;
#endif

	aw_info("%s enter\n", __func__);
	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		aw_err("check_functionality failed\n");
		return -EIO;
	}

	awinic = devm_kzalloc(&i2c->dev, sizeof(struct awinic), GFP_KERNEL);
	if (awinic == NULL)
		return -ENOMEM;
	input_dev = devm_input_allocate_device(&i2c->dev);
	if (!input_dev)
		return -ENOMEM;
	input_dev->name = "awinic_haptic";
	awinic->dev = &i2c->dev;
	awinic->i2c = i2c;
	device_init_wakeup(awinic->dev, true);
	i2c_set_clientdata(i2c, awinic);

	/* awinic rst & int */
	if (np) {
		ret = awinic_parse_dt(awinic, &i2c->dev, np);
		if (ret) {
			aw_err("%s: failed to parse device tree node\n",
				__func__);
			goto err_parse_dt;
		}
	} else {
		awinic->reset_gpio = -1;
		awinic->irq_gpio = -1;
	}

	awinic->enable_pin_control = 0;

#ifdef ENABLE_PIN_CONTROL
	awinic->awinic_pinctrl = devm_pinctrl_get(&i2c->dev);
	if (IS_ERR(awinic->awinic_pinctrl)) {
		if (PTR_ERR(awinic->awinic_pinctrl) == -EPROBE_DEFER) {
			aw_info("%s pinctrl not ready\n", __func__);
			rc = -EPROBE_DEFER;
			return rc;
		}
		aw_info("%s Target does not use pinctrl\n", __func__);
		awinic->awinic_pinctrl = NULL;
		rc = -EINVAL;
		return rc;
	}
	for (i = 0; i < ARRAY_SIZE(awinic->pinctrl_state); i++) {
		const char *n = pctl_names[i];
		struct pinctrl_state *state =
			pinctrl_lookup_state(awinic->awinic_pinctrl, n);
		if (!IS_ERR(state)) {
			aw_info("%s: found pin control %s\n", __func__, n);
			awinic->pinctrl_state[i] = state;
			awinic->enable_pin_control = 1;
			awinic_set_interrupt(awinic);
			continue;
		}
		aw_info("%s cannot find '%s'\n", __func__, n);

	}
#endif
	if (!awinic->enable_pin_control) {
		if (gpio_is_valid(awinic->reset_gpio)) {
			ret = devm_gpio_request_one(&i2c->dev,
						    awinic->reset_gpio,
						    GPIOF_OUT_INIT_LOW,
						    "awinic_rst");
			if (ret) {
				aw_err("%s: rst request failed\n", __func__);
				goto err_reset_gpio_request;
			}
		}
	}

	if (gpio_is_valid(awinic->irq_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, awinic->irq_gpio,
					    GPIOF_DIR_IN, "awinic_int");
		if (ret) {
			aw_err("%s: int request failed\n", __func__);
			goto err_irq_gpio_request;
		}
	}

	ret = awinic_parse_chipid(awinic);
	if (ret < 0) {
		aw_err("%s: read_chipid failed ret=%d\n", __func__, ret);
		goto err_id;
	}

	/* aw8697 */
	if (awinic->name == AW8697) {
		awinic->aw8697 = devm_kzalloc(&i2c->dev,
					sizeof(struct aw8697), GFP_KERNEL);
		if (awinic->aw8697 == NULL) {
			if (gpio_is_valid(awinic->irq_gpio))
				devm_gpio_free(&i2c->dev, awinic->irq_gpio);
			if (gpio_is_valid(awinic->reset_gpio))
				devm_gpio_free(&i2c->dev, awinic->reset_gpio);
			devm_kfree(&i2c->dev, awinic);
			awinic = NULL;
			return -ENOMEM;
		}

		awinic->aw8697->dev = awinic->dev;
		awinic->aw8697->i2c = awinic->i2c;
		awinic->aw8697->reset_gpio = awinic->reset_gpio;
		awinic->aw8697->irq_gpio = awinic->irq_gpio;

		/* aw8697 rst & int */
		if (np) {
			ret = aw8697_parse_dt(&i2c->dev, awinic->aw8697, np);
			if (ret) {
				aw_err("%s: failed to parse device tree node\n",
					__func__);
				goto err_aw8697_parse_dt;
			}
		}

		/* aw8697 irq */
		if (gpio_is_valid(awinic->aw8697->irq_gpio) &&
		    !(awinic->aw8697->flags & AW8697_FLAG_SKIP_INTERRUPTS)) {
			/* register irq handler */
			aw8697_interrupt_setup(awinic->aw8697);
			irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
			ret = devm_request_threaded_irq(&i2c->dev,
					gpio_to_irq(awinic->aw8697->irq_gpio),
					NULL, aw8697_irq, irq_flags,
					"aw8697", awinic->aw8697);
			if (ret != 0) {
				aw_err("%s: failed to request IRQ %d: %d\n",
					__func__,
					gpio_to_irq(awinic->aw8697->irq_gpio),
					ret);
				goto err_aw8697_irq;
			}
		} else {
			aw_info("%s skipping IRQ registration\n", __func__);
			/* disable feature support if gpio was invalid */
			awinic->aw8697->flags |= AW8697_FLAG_SKIP_INTERRUPTS;
			aw_info("%s: aw8697_irq failed.\n", __func__);
		}

		awinic->aw8697->work_queue = create_singlethread_workqueue("aw8697_vibrator_work_queue");
		if (!awinic->aw8697->work_queue) {
			aw_err("%s: Error creating aw8697_vibrator_work_queue\n",
				__func__);
			goto err_aw8697_sysfs;
		}

		aw8697_vibrator_init(awinic->aw8697);
		aw8697_haptic_init(awinic->aw8697);
		aw8697_ram_init(awinic->aw8697);

		CUSTOME_WAVE_ID = awinic->aw8697->info.effect_max;

		/*aw8697 input config*/
		input_set_drvdata(input_dev, awinic->aw8697);
		awinic->aw8697->input_dev = input_dev;
		input_set_capability(input_dev, EV_FF, FF_CONSTANT);
		input_set_capability(input_dev, EV_FF, FF_GAIN);
		if (awinic->aw8697->effects_count != 0) {
			input_set_capability(input_dev, EV_FF, FF_PERIODIC);
			input_set_capability(input_dev, EV_FF, FF_CUSTOM);
		}
		if (awinic->aw8697->effects_count + 1 > FF_EFFECT_COUNT_MAX)
			effect_count_max = awinic->aw8697->effects_count + 1;
		else
			effect_count_max = FF_EFFECT_COUNT_MAX;
		rc = input_ff_create(input_dev, effect_count_max);
		if (rc < 0) {
			aw_err("%s create FF input device failed, rc=%d\n",
				__func__, rc);
			goto err_aw8697_input_ff;
		}
		INIT_WORK(&awinic->aw8697->set_gain_work,
			  aw8697_haptics_set_gain_work_routine);
		ff = input_dev->ff;
		ff->upload = aw8697_haptics_upload_effect;
		ff->playback = aw8697_haptics_playback;
		ff->erase = aw8697_haptics_erase;
		ff->set_gain = aw8697_haptics_set_gain;
		rc = input_register_device(input_dev);
		if (rc < 0) {
			aw_err("%s register input device failed, rc=%d\n",
				__func__, rc);
			goto aw8697_destroy_ff;
		}
	} else if (awinic->name == AW86927) {
		awinic->aw86927 = devm_kzalloc(&i2c->dev,
					sizeof(struct aw86927), GFP_KERNEL);
		if (awinic->aw86927 == NULL) {
			if (gpio_is_valid(awinic->irq_gpio))
				devm_gpio_free(&i2c->dev, awinic->irq_gpio);
			if (gpio_is_valid(awinic->reset_gpio))
				devm_gpio_free(&i2c->dev, awinic->reset_gpio);
			devm_kfree(&i2c->dev, awinic);
			awinic = NULL;
			return -ENOMEM;
		}

		awinic->aw86927->dev = awinic->dev;
		awinic->aw86927->i2c = awinic->i2c;
		awinic->aw86927->reset_gpio = awinic->reset_gpio;
		awinic->aw86927->irq_gpio = awinic->irq_gpio;
		/* chip qualify */
#ifdef AW_CHECK_QUAL
		if (aw86927_check_qualify(awinic->aw86927)) {
			aw_err("%s:unqualified chip!\n", __func__);
			goto err_aw86927_check_qualify;
		}
#endif
		/* aw86927 rst & int */
		if (np) {
			ret = aw86927_parse_dt(awinic->aw86927, &i2c->dev, np);
			if (ret) {
				aw_err("%s: failed to parse device tree node\n",
					__func__);
				goto err_aw86927_parse_dt;
			}
		}

		/* aw86927 irq */
		if (gpio_is_valid(awinic->aw86927->irq_gpio) &&
		    !(awinic->aw86927->flags & AW86927_FLAG_SKIP_INTERRUPTS)) {
			/* register irq handler */
			aw86927_interrupt_setup(awinic->aw86927);
			irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
			ret = devm_request_threaded_irq(&i2c->dev,
					gpio_to_irq(awinic->aw86927->irq_gpio),
					NULL, aw86927_irq, irq_flags,
					"aw86927", awinic->aw86927);
			if (ret != 0) {
				aw_err("%s: failed to request IRQ %d: %d\n",
					__func__,
					gpio_to_irq(awinic->aw86927->irq_gpio),
					ret);
				goto err_aw86927_irq;
			}
		} else {
			aw_info("%s skipping IRQ registration\n", __func__);
			/* disable feature support if gpio was invalid */
			awinic->aw86927->flags |= AW86927_FLAG_SKIP_INTERRUPTS;
			aw_info("%s: aw86927_irq failed.\n", __func__);
		}

		awinic->aw86927->work_queue = create_singlethread_workqueue("aw86927_vibrator_work_queue");
		if (!awinic->aw86927->work_queue) {
			aw_err("%s: Error creating aw86927_vibrator_work_queue\n",
				__func__);
			goto err_aw86927_sysfs;
		}

		aw86927_vibrator_init(awinic->aw86927);
		aw86927_haptic_init(awinic->aw86927);
		aw86927_ram_init(awinic->aw86927);

		CUSTOME_WAVE_ID = awinic->aw86927->info.effect_max;

		/*aw86927 input config*/
		input_set_drvdata(input_dev, awinic->aw86927);
		awinic->aw86927->input_dev = input_dev;
		input_set_capability(input_dev, EV_FF, FF_CONSTANT);
		input_set_capability(input_dev, EV_FF, FF_GAIN);
		if (awinic->aw86927->effects_count != 0) {
			input_set_capability(input_dev, EV_FF, FF_PERIODIC);
			input_set_capability(input_dev, EV_FF, FF_CUSTOM);
		}
		if (awinic->aw86927->effects_count + 1 > FF_EFFECT_COUNT_MAX)
			effect_count_max = awinic->aw86927->effects_count + 1;
		else
			effect_count_max = FF_EFFECT_COUNT_MAX;
		rc = input_ff_create(input_dev, effect_count_max);
		if (rc < 0) {
			aw_err("%s create FF input device failed, rc=%d\n",
				__func__, rc);
			goto err_aw86927_input_ff;
		}
		INIT_WORK(&awinic->aw86927->set_gain_work,
			  aw86927_haptics_set_gain_work_routine);
		ff = input_dev->ff;
		ff->upload = aw86927_haptics_upload_effect;
		ff->playback = aw86927_haptics_playback;
		ff->erase = aw86927_haptics_erase;
		ff->set_gain = aw86927_haptics_set_gain;
		rc = input_register_device(input_dev);
		if (rc < 0) {
			aw_err("%s register input device failed, rc=%d\n",
				__func__, rc);
			goto aw86927_destroy_ff;
		}
#ifdef AAC_RICHTAP_SUPPORT
		awinic->aw86927->rtp_ptr = kmalloc(RICHTAP_MMAP_BUF_SIZE * RICHTAP_MMAP_BUF_SUM, GFP_KERNEL);
		if(awinic->aw86927->rtp_ptr == NULL)
		{
			dev_err(&i2c->dev, "malloc rtp memory failed\n");
			goto richtap_err1;
		}

		awinic->aw86927->start_buf = (struct mmap_buf_format *)__get_free_pages(GFP_KERNEL, RICHTAP_MMAP_PAGE_ORDER);
		if(awinic->aw86927->start_buf == NULL)    {
			dev_err(&i2c->dev, "Error __get_free_pages failed\n");
			goto richtap_err2;
		}
		SetPageReserved(virt_to_page(awinic->aw86927->start_buf));
		{
			struct mmap_buf_format *temp;
			uint32_t i = 0;
			temp = awinic->aw86927->start_buf;
			for( i = 1; i < RICHTAP_MMAP_BUF_SUM; i++)
			{
					temp->kernel_next = (awinic->aw86927->start_buf + i);
					temp = temp->kernel_next;
			}
			temp->kernel_next = awinic->aw86927->start_buf;
		}
		INIT_WORK(&awinic->aw86927->richtap_rtp_work, richtap_rtp_work);
		atomic_set(&awinic->aw86927->richtap_rtp_mode, false);
#ifdef AW_DOUBLE
		if(of_device_is_compatible(np, "awinic,haptic_hv_l"))
		{
			misc_register(&richtap_misc);
		}

		if(of_device_is_compatible(np, "awinic,haptic_hv_r"))
		{
			misc_register(&richtap_misc_x);
		}
#else
		misc_register(&richtap_misc);
		//dev_set_drvdata(richtap_misc.this_device, aw_haptic);
		g_aw_haptic = awinic->aw86927;
#endif
#endif
	} else {
		goto err_parse_dt;
	}

	dev_set_drvdata(&i2c->dev, awinic);
	ret =  create_rb();
	if (ret < 0) {
		aw_info("%s error creating ringbuffer\n", __func__);
		goto err_rb;
	}
	aw_info("%s probe completed successfully!\n", __func__);
	return 0;

#ifdef AAC_RICHTAP_SUPPORT
richtap_err2:
	kfree(awinic->aw86927->rtp_ptr);
richtap_err1:
	devm_free_irq(&i2c->dev, gpio_to_irq(awinic->aw86927->irq_gpio), awinic->aw86927);
#endif

err_rb:
aw86927_destroy_ff:
	if (awinic->name == AW86927)
		input_ff_destroy(awinic->aw86927->input_dev);
err_aw86927_input_ff:
err_aw86927_sysfs:
	if (awinic->name == AW86927)
		devm_free_irq(&i2c->dev, gpio_to_irq(awinic->aw86927->irq_gpio),
			      awinic->aw86927);
err_aw86927_irq:
err_aw86927_parse_dt:
#ifdef AW_CHECK_QUAL
err_aw86927_check_qualify:
#endif
	if (awinic->name == AW86927) {
		devm_kfree(&i2c->dev, awinic->aw86927);
		awinic->aw86927 = NULL;
	}
aw8697_destroy_ff:
	if (awinic->name == AW8697)
		input_ff_destroy(awinic->aw8697->input_dev);
err_aw8697_input_ff:
err_aw8697_sysfs:
	if (awinic->name == AW8697)
		devm_free_irq(&i2c->dev, gpio_to_irq(awinic->aw8697->irq_gpio),
			      awinic->aw8697);
err_aw8697_irq:
err_aw8697_parse_dt:
	if (awinic->name == AW8697) {
		devm_kfree(&i2c->dev, awinic->aw8697);
		awinic->aw8697 = NULL;
	}
err_id:
	if (gpio_is_valid(awinic->irq_gpio))
		devm_gpio_free(&i2c->dev, awinic->irq_gpio);
err_irq_gpio_request:
	if (gpio_is_valid(awinic->reset_gpio))
		devm_gpio_free(&i2c->dev, awinic->reset_gpio);
err_reset_gpio_request:
err_parse_dt:
	device_init_wakeup(awinic->dev, false);
	devm_kfree(&i2c->dev, awinic);
	awinic = NULL;
	return ret;
}


static int awinic_i2c_remove(struct i2c_client *i2c)
{
	struct awinic *awinic = i2c_get_clientdata(i2c);

	aw_info("%s enter\n", __func__);

	if (awinic->name == AW8697) {
		aw_err("%s remove aw8697\n", __func__);
		misc_deregister(&aw8697_haptic_misc);
		sysfs_remove_group(&i2c->dev.kobj,
				   &aw8697_vibrator_attribute_group);

		devm_free_irq(&i2c->dev, gpio_to_irq(awinic->aw8697->irq_gpio),
			      awinic->aw8697);
		if (gpio_is_valid(awinic->aw8697->irq_gpio))
			devm_gpio_free(&i2c->dev, awinic->aw8697->irq_gpio);
		if (gpio_is_valid(awinic->aw8697->reset_gpio))
			devm_gpio_free(&i2c->dev, awinic->aw8697->reset_gpio);
		if (awinic->aw8697 != NULL) {
			flush_workqueue(awinic->aw8697->work_queue);
			destroy_workqueue(awinic->aw8697->work_queue);
		}
		devm_kfree(&i2c->dev, awinic->aw8697);
		awinic->aw8697 = NULL;
	} else if (awinic->name == AW86927) {
		aw_err("%s remove aw86927\n", __func__);
		sysfs_remove_group(&i2c->dev.kobj,
				   &aw86927_vibrator_attribute_group);

		devm_free_irq(&i2c->dev, gpio_to_irq(awinic->aw86927->irq_gpio),
			      awinic->aw86927);

#ifdef AAC_RICHTAP_SUPPORT
		cancel_work_sync(&awinic->aw86927->richtap_rtp_work);
		kfree(awinic->aw86927->rtp_ptr);
		ClearPageReserved(virt_to_page(awinic->aw86927->start_buf));
		free_pages((unsigned long)awinic->aw86927->start_buf, RICHTAP_MMAP_PAGE_ORDER);
#ifdef AW_DOUBLE
		if(of_device_is_compatible(i2c->dev.of_node, "awinic,haptic_hv_l"))
		{
			misc_deregister(&richtap_misc);
		}
		if(of_device_is_compatible(i2c->dev.of_node, "awinic,haptic_hv_r"))
		{
			misc_deregister(&richtap_misc_x);
		}
#else
		misc_deregister(&richtap_misc);
#endif
#endif
		if (gpio_is_valid(awinic->aw86927->irq_gpio))
			devm_gpio_free(&i2c->dev, awinic->aw86927->irq_gpio);
		if (gpio_is_valid(awinic->aw86927->reset_gpio))
			devm_gpio_free(&i2c->dev, awinic->aw86927->reset_gpio);
		if (awinic->aw86927 != NULL) {
			flush_workqueue(awinic->aw86927->work_queue);
			destroy_workqueue(awinic->aw86927->work_queue);
		}
		devm_kfree(&i2c->dev, awinic->aw86927);
		awinic->aw86927 = NULL;
	} else {
		aw_err("%s no chip\n", __func__);
		return -ERANGE;
	}
	release_rb();
	device_init_wakeup(awinic->dev, false);

	aw_info("%s exit\n", __func__);
	return 0;
}

static const struct i2c_device_id awinic_i2c_id[] = {
	{AW_I2C_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, awinic_i2c_id);

static const struct of_device_id aw_dt_match[] = {
	{.compatible = "awinic,awinic_haptic"},
	{},
};

static struct i2c_driver awinic_i2c_driver = {
	.driver = {
		   .name = AW_I2C_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(aw_dt_match),
		  },
	.probe = awinic_i2c_probe,
	.remove = awinic_i2c_remove,
	.id_table = awinic_i2c_id,
};

static int __init awinic_i2c_init(void)
{
	int ret = 0;

	aw_info("%s awinic driver version %s\n", __func__, AW_DRIVER_VERSION);

	ret = i2c_add_driver(&awinic_i2c_driver);
	if (ret) {
		aw_err("%s fail to add awinic device into i2c\n", __func__);
		return ret;
	}
	aw_info("%s aw_i2c_init success", __func__);
	return 0;
}

module_init(awinic_i2c_init);

static void __exit awinic_i2c_exit(void)
{
	i2c_del_driver(&awinic_i2c_driver);
}

module_exit(awinic_i2c_exit);

MODULE_DESCRIPTION("AWinic Haptic Driver");
MODULE_LICENSE("GPL v2");
