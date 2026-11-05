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
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <linux/thermal.h>
#include <linux/mman.h>
#include <linux/minmax.h>

#ifndef MIN
#define MIN(a, b) min(a, b)
#endif

#include "fs_haptic.h"
#include "ringbuffer.h"
#include "fs3002.h"

#define FS3002_BROADCAST_ADDR			(0x00)
#define FS3002_LEFT_CHIP_ADDR			(0x5A)
#define FS3002_RIGHT_CHIP_ADDR			(0x5B)
#define FS3002_RTPSTART_SIZE            (0x80)

static unsigned char ERROR_CODE = 0xFF;
static char* FSERROR = "FSERROR";
static char* FSREAD = "FSREAD";
static char* FSWRITE = "FSWRITE";
static char* FSWRITEBULK = "FSWRITEBULK";
struct foursemi *g_foursemi = NULL;
struct pm_qos_request fs3002_pm_qos_req_vb;
char *fs3002_ram_name[] = {"fs3002_haptic.bin"};//zzzz            change char *fs3002_ram_name[]->char *fs3002_ram_name   ?

static char fs3002_rtp_name[][FS3002_RTP_NAME_MAX] = 
	{
		{"0_osc_rtp_12K_10s.bin"}, //0
		{"1_IncomingMessage_RTP.bin"}, //1
		{"2_Guitar_RTP.bin"},
		{"3_Charge_Wire_RTP.bin"},
		{"4_Candy_RTP.bin"},
		{"5_Harp_RTP.bin"},
		{"fs3002_rtp_1.bin"},
		{"fs3002_rtp_1.bin"},
		{"fs3002_rtp_1.bin"},
		{"fs3002_rtp_1.bin"},
		{"fs3002_rtp_1.bin"},
		{"fs3002_rtp_1.bin"},//11
		{"fs3002_rtp_1.bin"},
		{"fs3002_rtp_1.bin"},
		{"fs3002_rtp_1.bin"},
		{"fs3002_rtp_1.bin"},
		{"fs3002_rtp_1.bin"},
		{"fs3002_rtp_1.bin"},
		{"fs3002_rtp_1.bin"},
		{"fs3002_rtp_1.bin"},
		{"fs3002_rtp_1.bin"},		
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
		{"Fantasy_RTP.bin"},//31
		{"Field_Trip_RTP.bin"},
		{"Glee_RTP.bin"},
		{"Glockenspiel_RTP.bin"},
		{"Ice_Latte_RTP.bin"},
		{"Kung_Fu_RTP.bin"},
		{"Leisure_RTP.bin"},
		{"Lollipop_RTP.bin"},
		{"MiMix2_RTP.bin"},
		{"Mi_RTP.bin"},
		{"MiHouse_RTP.bin"},//41
		{"MiJazz_RTP.bin"},
		{"MiRemix_RTP.bin"},
		{"Mountain_Spring_RTP.bin"},
		{"Orange_RTP.bin"},
		{"WindChime_RTP.bin"},
		{"Space_Age_RTP.bin"},
		{"ToyRobot_RTP.bin"},
		{"Vigor_RTP.bin"},
		{"Bottle_RTP.bin"},
		{"Bubble_RTP.bin"},//51
		{"Bullfrog_RTP.bin"},
		{"Burst_RTP.bin"},
		{"Chirp_RTP.bin"},
		{"Clank_RTP.bin"},
		{"Crystal_RTP.bin"},
		{"FadeIn_RTP.bin"},
		{"FadeOut_RTP.bin"},
		{"Flute_RTP.bin"},
		{"Fresh_RTP.bin"},
		{"Frog_RTP.bin"},//61
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
		{"fs3002_rtp_1.bin"}, /*99*/
		{"fs3002_rtp_1.bin"}, /*100*/
		{"offline_countdown_RTP.bin"},
		{"scene_bomb_injury_RTP.bin"},
		{"scene_bomb_RTP.bin"}, /*103*/
		{"door_open_RTP.bin"},
		{"fs3002_rtp_1.bin"},
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
		{"fs3002_rtp_1.bin"},
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
		{"fs3002_rtp_1.bin"},
		{"firearms_awm_RTP.bin"}, /*129*/
		{"firearms_mini14_RTP.bin"}, /*130*/
		{"firearms_vss_RTP.bin"}, /*131*/
		{"firearms_qbz_RTP.bin"}, /*132*/
		{"firearms_ump9_RTP.bin"}, /*133*/
		{"firearms_dp28_RTP.bin"}, /*134*/
		{"firearms_s1897_RTP.bin"}, /*135*/
		{"fs3002_rtp_1.bin"},
		{"firearms_p18c_RTP.bin"}, /*137*/
		{"fs3002_rtp_1.bin"},
		{"fs3002_rtp_1.bin"},
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
int fs3002_rtp_name_len = sizeof(fs3002_rtp_name) / FS3002_RTP_NAME_MAX;
static int wf_repeat[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };


char str[300];




//-----------------------------------------------------------------------------------------

//zzzz

static int fs3002_ram_update(struct fs3002 *fs3002);
static int fs3002_haptic_set_mode(struct fs3002 *fs3002, unsigned char play_mode);
static void fs3002_haptic_play_go(struct fs3002 *fs3002);
static int fs3002_set_base_addr(struct fs3002 *fs3002);
static int fs3002_haptic_stop(struct fs3002 *fs3002);
static void fs3002_vibrate_work_routine(struct work_struct *work);
static void fs3002_rtp_work_routine(struct work_struct *work);
static void fs3002_haptic_audio_init(struct fs3002 *fs3002);
void fs3002_f0_cali_setting_init(struct fs3002 *fs3002);
static void fs3002_haptic_set_f0_ref(struct fs3002 *fs3002);
int fs3002_ram_init(struct fs3002 *fs3002);
static void fs3002_ram_loaded(const struct firmware *cont, void *context);
static int fs3002_haptic_get_vbat(struct fs3002 *fs3002);
static void fs3002_haptic_raminit(struct fs3002 *fs3002, bool flag);
static void fs3002_interrupt_clear(struct fs3002 *fs3002);
void fs3002_haptic_set_rtp_aei(struct fs3002 *fs3002, bool flag);
static int fs3002_haptic_rtp_init(struct fs3002 *fs3002);
static int fs3002_haptic_set_pwm(struct fs3002 *fs3002, unsigned char mode);
static void fs3002_haptic_set_gain(struct fs3002 *fs3002, unsigned char gain);
static unsigned char fs3002_get_midv(struct fs3002 *fs3002);
static unsigned char fs3002_get_highv(struct fs3002 *fs3002);
static unsigned int get_theory_time(struct fs3002 *fs3002,unsigned char fre_val);
static void fs3002_haptic_upload_lra(struct fs3002 *fs3002, unsigned int flag);
static void fs3002_haptic_diagnostic_sequence(struct fs3002 *fs3002);
static void fs3002_haptic_set_diagnostic_reg_to_default_value(struct fs3002 *fs3002);
static uint8_t fs3002_get_bin_ram_num(const uint8_t* buffer);

//zzzz static void fs3002_haptic_duration_ram_play_config(struct fs3002 *fs3002,int duration);

//-----------------------------------------------------------------------------------------













void fs3002_debug_message(struct fs3002 *fs3002, const char* p_char)
{
	if(fs3002->fs3002_debug_enable)
	{
		pr_err("%s",p_char);
	}
}

static unsigned char pow2(unsigned char value)
{
	unsigned char ret = 1;
	int i=0;

	for(i=0;i<value;i++)
	{
		ret = ret*2;
	}	
	
	return ret;
}

static unsigned char util_get_bit(unsigned char value, int i_pos)
{
	return ((value >> i_pos) & 1);
}

static unsigned char util_get_bits(unsigned char value, int i_start, int i_stop)
{
	if(i_start<i_stop || (i_start-i_stop+1)>8)
	{
		pr_err("%s:i_start=%d,i_stop=%d\n",FSERROR,i_start,i_stop);
		return ERROR_CODE;
	}
	return ((value >> i_stop) & (pow2(i_start - i_stop + 1) - 1));
}



static int fs3002_i2c_write(struct fs3002 *fs3002, unsigned char reg_addr, unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < FS_I2C_RETRIES) 
	{
		ret = i2c_smbus_write_byte_data(fs3002->i2c, reg_addr, reg_data);
		if (ret < 0) 
		{
			pr_err("%s:i2c_write addr=0x%02X, data=0x%02X, cnt=%d, error=%d\n", FSERROR,reg_addr, reg_data, cnt, ret);
		} 
		else 
		{
			if(fs3002->fs3002_debug_enable)
			{
				pr_info("%s,addr=0x%02X, data=0x%02X\n",FSWRITE,reg_addr, reg_data);
			}
			break;
		}
		cnt++;

		msleep(FS_I2C_RETRY_DELAY);
	}

	return ret;
}

static int fs3002_i2c_read(struct fs3002 *fs3002, unsigned char reg_addr, unsigned char *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < FS_I2C_RETRIES) 
	{
		ret = i2c_smbus_read_byte_data(fs3002->i2c, reg_addr);
		if (ret < 0) 
		{
			pr_err("%s:i2c_read addr=0x%02X, cnt=%d error=%d\n", FSERROR,reg_addr, cnt, ret);
		} 
		else 
		{
			*reg_data = ret;
			if(fs3002->fs3002_debug_enable)
			{
				pr_info("%s,addr=0x%02X, data=0x%02X\n",FSREAD,reg_addr, ret);
			}
			break;
		}
		cnt++;

		msleep(FS_I2C_RETRY_DELAY);
	}

	return ret;
}

static int fs3002_i2c_writes(struct fs3002 *fs3002,unsigned char reg_addr, unsigned char *buf, unsigned int len)
{
	int ret = -1;
	unsigned char *data = NULL;

	data = kmalloc(len + 1, GFP_KERNEL);
	if (data == NULL) 
	{
		pr_err("%s:can not allocate memory\n",FSERROR);
		return -ENOMEM;
	}
	data[0] = reg_addr;
	memcpy(&data[1], buf, len);
	ret = i2c_master_send(fs3002->i2c, data, len + 1);
	if (ret < 0)
		pr_err("%s:i2c master send error\n",FSERROR);
	else
	{
		sprintf(str,"%s,%s,addr=0x%02X, data=[%d]\n", __func__, FSWRITEBULK, reg_addr, len);
		fs3002_debug_message(fs3002, str);
	}
	kfree(data);
	return ret;
}

int fs3002_i2c_reads(struct fs3002 *fs3002, unsigned char reg_addr, unsigned char *buf, unsigned int len)
{
	int ret;
	struct i2c_msg msg[] = 
	{
		[0] = {
			.addr = fs3002->i2c->addr,
			.flags = 0,
			.len = sizeof(uint8_t),
			.buf = &reg_addr,
			},
		[1] = {
			.addr = fs3002->i2c->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
			},
	};

	ret = i2c_transfer(fs3002->i2c->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0) 
	{
		pr_err("%s: transfer failed.", FSERROR);
		return ret;
	} 
	else if (ret != 2) 
	{
		pr_err("%s: transfer failed(size error).", FSERROR);
		return -ENXIO;
	}

	return ret;
}

static int fs3002_i2c_write_bits(struct fs3002 *fs3002,unsigned char reg_addr,unsigned char reg_data, unsigned char i_start, unsigned char i_stop)
{
	int ret = -1;
	unsigned char reg_val = 0;
	unsigned char t1 = 0, t2 = 0;

	if(i_start<i_stop || (i_start-i_stop+1)>8)
	{
		pr_err("%s:i_start=%d,i_stop=%d\n",FSERROR,i_start,i_stop);
		return ret;
	}

	ret = fs3002_i2c_read(fs3002, reg_addr, &reg_val);
	if (ret < 0) 
	{
		pr_err("%s:ret=%d\n", FSERROR,ret);
		return ret;
	}
	t1 = (pow2(i_start - i_stop + 1) - 1)<<i_stop;
	t2 = ~t1;
	reg_val = (reg_val & t2) | (t1 & (reg_data << i_stop));
	ret = fs3002_i2c_write(fs3002, reg_addr, reg_val);
	if (ret < 0) 
	{
		pr_err("%s:ret=%d\n", FSERROR,ret);
		return ret;
	}
	return 0;
}

static unsigned char fs3002_i2c_read_bits(struct fs3002 *fs3002,unsigned char reg_addr, int i_start, int i_stop)
{
	int ret = -1;
	unsigned char reg_val = 0;

	if(i_start<i_stop || (i_start-i_stop+1)>8)
	{
		pr_err("%s:i_start=%d,i_stop=%d\n",FSERROR,i_start,i_stop);
		return ERROR_CODE;
	}

	ret = fs3002_i2c_read(fs3002, reg_addr, &reg_val);
	if (ret < 0) 
	{
		return ERROR_CODE;
	}

	return util_get_bits(reg_val,i_start,i_stop);
}

static void pm_qos_enable(struct fs3002 *fs3002,bool b_enable)
{
	if(fs3002->dts_info.fs3002_play_rtp_srate == 0)
	{
#ifdef FS_KERNEL_VER_OVER_5_10
		sprintf(str,"%s,OVER_5_10 enable=%d, Qos_time=%d\n",__func__,b_enable,fs3002->Qos_time);
		fs3002_debug_message(fs3002, str);
		if(b_enable)
		{
			cpu_latency_qos_add_request(&fs3002_pm_qos_req_vb, fs3002->Qos_time);
		}		
		else
		{
			cpu_latency_qos_remove_request(&fs3002_pm_qos_req_vb);		
		}
#else
		sprintf(str,"%s,NOT OVER_5_10 enable=%d, Qos_time=%d\n",__func__,b_enable,fs3002->Qos_time);
		fs3002_debug_message(fs3002, str);
		if(b_enable)
		{
			pm_qos_add_request(&fs3002_pm_qos_req_vb, PM_QOS_CPU_DMA_LATENCY,fs3002->Qos_time);
		}
		else
		{
			pm_qos_remove_request(&fs3002_pm_qos_req_vb);	
		}
#endif
	}
}


void fs3002_haptic_rtp_set_auto_env(struct fs3002 *fs3002)
{
	sprintf(str, "%s, enter,fs3002_rtp_auto_env=%d\n", __func__,fs3002->dts_info.fs3002_rtp_auto_env);
	fs3002_debug_message(fs3002, str);

	if(fs3002->dts_info.fs3002_rtp_auto_env)
	{
		fs3002_i2c_write_bits(fs3002, FS3002_RTPCTRL,1,3,3);
		if(fs3002->dts_info.fs3002_rtp_auto_size)
		{
			fs3002->dts_info.fs3002_rtp_auto_env_mid = 127*fs3002->dts_info.fs3002_env_lowv/fs3002_get_highv(fs3002);
			fs3002->dts_info.fs3002_rtp_auto_env_high = 127*fs3002_get_midv(fs3002)/fs3002_get_highv(fs3002);
		}
			
		fs3002_i2c_write(fs3002, FS3002_ADPBST4,fs3002->dts_info.fs3002_rtp_auto_env_mid);
		fs3002_i2c_write(fs3002, FS3002_ADPBST3,fs3002->dts_info.fs3002_rtp_auto_env_high);
	}
	else
	{
		fs3002_i2c_write_bits(fs3002, FS3002_RTPCTRL,0,3,3);
	}
}


//okok
unsigned char fs3002_haptic_rtp_get_fifo_afs_0xAF(struct fs3002 *fs3002)
{
	unsigned char reg_val = 0;
	
	sprintf(str,"%s,enter\n",__func__);
	fs3002_debug_message(fs3002,str);
	reg_val = fs3002_i2c_read_bits(fs3002, FS3002_INTSTATR2, 1,1);
	return reg_val;
}


//okok
static void fs3002_haptic_enable_key(struct fs3002 *fs3002, bool flag)
{
	sprintf(str, "%s,enter flag = %d\n",__func__,flag);
	fs3002_debug_message(fs3002, str);

	if (flag) 
	{
		fs3002_i2c_write(fs3002, FS3002_ACCKEY,0x91);
	} 
	else 
	{
		fs3002_i2c_write(fs3002, FS3002_ACCKEY,0);
	}
}

#ifdef FS_HAPSTREAM

static int fs3002_hapstream_i2c_writes(struct fs3002 *fs3002, struct mmap_buf_format *hapstream_buf)
{
	int ret = -1, i = 0, len = 0;
	char str[200] = { 0 };

	sprintf(str,"fs3002_hapstream_i2c_writes enter,hapstream_buf->reg_addr=0x%02X, hapstream_buf->length=%d\n",hapstream_buf->reg_addr,hapstream_buf->length);
	fs3002_debug_message(fs3002, str);

	ret = i2c_master_send(fs3002->i2c, &(hapstream_buf->reg_addr), hapstream_buf->length + 1);

	//xxxx for debug
	if(fs3002->fs3002_debug_enable)
	{
		for (i = 0; i < hapstream_buf->length; i++)
		{
			if ((i + 1) % 16 == 0)
			{
				snprintf(str + len, 200 - len, "0x%02X \n", (uint8_t)hapstream_buf->data[i]);
				pr_info("%s", str);
				len = 0;
			}
			else
			{
				len += snprintf(str + len, 200 - len, "0x%02X ", (uint8_t)hapstream_buf->data[i]);
				if(i == hapstream_buf->length -1)
				{
					pr_info("%s", str);
				}
			}
		}
	}
	
	if (ret < 0)
		pr_err("%s, i2c master send error\n", FSERROR);
	return ret;
}

static inline unsigned int fs3002_get_sys_msecs(struct fs3002 *fs3002)//static inline unsigned int fs3002_get_sys_msecs() -- transsion platform
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	struct timespec64 ts64;
	ktime_get_coarse_real_ts64(&ts64);
#else
	struct timespec64 ts64 = current_kernel_time64();
#endif

	sprintf(str,"fs3002_get_sys_msecs enter\n");
	fs3002_debug_message(fs3002, str);


	return jiffies_to_msecs(timespec64_to_jiffies(&ts64));
}

static void fs3002_hapstream_clean_buf(struct fs3002 *fs3002, int status)
{
	struct mmap_buf_format *hapstream_buf = fs3002->start_buf;
	int i = 0;

	pr_info("enter\n");

	for (i = 0; i < HAPSTREAM_MMAP_BUF_SUM; i++)
	{
		hapstream_buf->status = status;
		hapstream_buf = hapstream_buf->kernel_next;
	}
}

static void fs3002_rtp_work_hapstream(struct work_struct *work)
{
	//struct fs3002 *fs3002 = container_of(work,struct fs3002,rtp_hapstream);
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	struct mmap_buf_format *hapstream_buf = fs3002->start_buf;
	int count = 100;
	unsigned char reg_val_AF = 0x01;
	unsigned char reg_val_11 = 0;
	unsigned char reg_val_00 = 0;
	unsigned int write_start;
	unsigned int buf_cnt = 0;

	pr_info("enter\n");

	mutex_lock(&fs3002->lock);
	fs3002->hapstream_stop_flag = false;
	while (true && count--)
	{
		if (hapstream_buf->status == MMAP_BUF_DATA_VALID) 
		{
			fs3002_haptic_set_mode(fs3002, FS3002_HAPTIC_RTP_MODE);
			fs3002_haptic_upload_lra(fs3002, FS3002_WRITE_ZERO);
			fs3002_haptic_play_go(fs3002);
			break;
		} 
		else if (fs3002->hapstream_stop_flag == true) 
		{
			mutex_unlock(&fs3002->lock);
			pr_info("fs3002->hapstream_stop_flag == true, return\n");
			return;
		} 
		else 
		{
			mdelay(1);//mdelay(10);//xxxx for debug, otherwise too much debug message     
		}
	}
	if (count <= 0) 
	{
		pr_err( "%s, error, start_buf->status is not valid\n", FSERROR);
		fs3002->hapstream_stop_flag = true;
		mutex_unlock(&fs3002->lock);
		return;
	}
	mutex_unlock(&fs3002->lock);

	mutex_lock(&fs3002->rtp_lock);
	pm_qos_enable(fs3002, true);
	fs3002_haptic_raminit(fs3002, true);
	write_start = fs3002_get_sys_msecs(fs3002);
	reg_val_AF = 0x01;
	while (true)
	{
		if (fs3002_get_sys_msecs(fs3002) > (write_start + 800)) 
		{
			pr_err( "%s, Failed ! endless loop\n", FSERROR);
			break;
		}
		fs3002_i2c_read(fs3002, FS3002_STATUS, &reg_val_00);
		fs3002_i2c_read(fs3002, FS3002_SYSCTRL, &reg_val_11);
		if ((reg_val_11 & 0x03) != 0x01)
		{
			pr_info("hapstream break_1, reg_11 = 0x%02X, not rtp mode\n", reg_val_11);
			break;
		}
		if (((reg_val_00 & 0x01) != 0x01) || (fs3002->hapstream_stop_flag == true) || (hapstream_buf->status == MMAP_BUF_DATA_FINISHED) || (hapstream_buf->status == MMAP_BUF_DATA_INVALID)) 
		{
			pr_info("hapstream break_2, reg_00 = 0x%02X, hapstream_stop_flag = %d, hapstream_buf->status = 0x%02X\n",reg_val_00, fs3002->hapstream_stop_flag, hapstream_buf->status);
			break;
		}
		else if (hapstream_buf->status == MMAP_BUF_DATA_VALID && (reg_val_AF & 0x01)) 
		{
			pr_info("buf_cnt = %d, bit = %d, length = %d!\n", buf_cnt, hapstream_buf->bit, hapstream_buf->length);
			fs3002_hapstream_i2c_writes(fs3002, hapstream_buf);
			memset(hapstream_buf->data, 0, hapstream_buf->length);
			hapstream_buf->length = 0;
			hapstream_buf->status = MMAP_BUF_DATA_FINISHED;
			hapstream_buf = hapstream_buf->kernel_next;
			write_start = fs3002_get_sys_msecs(fs3002);
			buf_cnt++;
		}
		else 
		{
			if(fs3002->fs3002_debug_enable)
				pr_info("hapstream wait, reg_AF = 0x%02X, hapstream_buf->status = 0x%02X\n", reg_val_AF, hapstream_buf->status);
			mdelay(1);//mdelay(10);//xxxx for debug, otherwise too much debug message     
		}
		fs3002_i2c_read(fs3002, FS3002_INTSTATR2, &reg_val_AF);
	}
	fs3002_haptic_raminit(fs3002, false);
	pm_qos_enable(fs3002, false);

	fs3002->hapstream_stop_flag = true;
	mutex_unlock(&fs3002->rtp_lock);
}

static void fs3002_rtp_irq_work_hapstream(struct work_struct*work)
{
	unsigned int cnt = 200;
	unsigned char reg_val = 0;
	bool rtp_work_flag = false;
	struct fs3002 *fs3002 = g_foursemi->fs3002;

	pr_info("enter\n");

	mutex_lock(&fs3002->lock);
	fs3002_haptic_stop(fs3002);
	fs3002_haptic_set_rtp_aei(fs3002, false);
	fs3002_interrupt_clear(fs3002);
	fs3002_haptic_set_mode(fs3002, FS3002_HAPTIC_RTP_MODE);
	fs3002_haptic_play_go(fs3002);
	//usleep_range (2000, 2500);
	while (cnt) 
	{
		fs3002_i2c_read(fs3002, FS3002_DIGSTAT, &reg_val);
		if ((reg_val >> 4) == FS3002_DIGSTAT_B7_4_OPS_GO) //DIGSTAT_OPS == 0x20  go
		{
			cnt = 0;
			rtp_work_flag = true;
			sprintf(str,"%s,RTP_GO! OPS = 2\n",__func__);
			fs3002_debug_message(fs3002,str);
		} 
		else 
		{
			cnt--;
			sprintf(str,"%s,wait for RTP_GO, OPS=%d\n",__func__,(reg_val>>4));
			fs3002_debug_message(fs3002, str);
		}
		usleep_range(2000, 2500);
	}

	if (!rtp_work_flag) 
	{
		pr_info( "failed to enter RTP_CO status!\n");
		fs3002_haptic_stop(fs3002);
	}
	mutex_unlock(&fs3002->lock);

	if (fs3002->hapstream_stop_flag == false) 
	{
		fs3002_haptic_rtp_init(fs3002);
	}
}


static long fs3002_hapstream_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned char reg_addr = 0;
	unsigned char reg_data = 0;
	int ret = 0;
	unsigned int tmp = 0;

	pr_info("enter,cmd=0x%x,arg=0x%ld\n",cmd,arg);

	switch (cmd) 
	{
		case HAPSTREAM_GET_HWINFO:
			pr_info("cmd = HAPSTREAM_GET_HWINFO!\n");
			tmp = fs3002->chipid;
			if (copy_to_user((void __user *)arg, &tmp, sizeof(unsigned int)))
				ret = -EFAULT;
			break;
		case HAPSTREAM_SETTING_GAIN:
			pr_info("cmd = HAPSTREAM_SETTING_GAIN!, arg = 0x%2lx\n", arg);
			mutex_lock(&fs3002->lock);
			fs3002->gain = arg;
			fs3002_haptic_set_gain(fs3002, fs3002->gain);
			mutex_unlock(&fs3002->lock);
			break;
		case HAPSTREAM_GET_F0:
			pr_info("cmd = HAPSTREAM_GET_F0!\n");
			tmp = fs3002->f0;
			if (copy_to_user((void __user *)arg, &tmp, sizeof(unsigned int)))
				ret = -EFAULT;
			break;
		case HAPSTREAM_WRITE_REG:
			pr_info("cmd = HAPSTREAM_WRITE_REG!\n");
			reg_addr = (arg & 0xFF00) >> 8;
			reg_data = arg & 0x00FF;
			fs3002_i2c_write(fs3002, reg_addr, reg_data);
			break;
		case HAPSTREAM_READ_REG:
			pr_info("cmd = HAPSTREAM_READ_REG!\n");
			if (copy_from_user(&reg_addr, (void __user *)arg, sizeof(unsigned char))) {
				ret = -EFAULT;
				break;
			}
			fs3002_i2c_read(fs3002, reg_addr, &reg_data);
			if (copy_to_user((void __user *)arg, &reg_data, sizeof(unsigned char)))
				ret = -EFAULT;
			break;
		case HAPSTREAM_STOP_MODE:
			pr_info("cmd = HAPSTREAM_STOP_MODE!\n");
			fs3002->hapstream_stop_flag = true;
			mutex_lock(&fs3002->lock);
			fs3002_haptic_stop(fs3002);
			mutex_unlock(&fs3002->lock);
			break;
		case HAPSTREAM_ON_MODE:
			pr_info("cmd = HAPSTREAM_ON_MODE!, arg = %ld\n", arg);
			tmp = arg;
			vfree(fs3002->hapstream_rtp);
			fs3002->hapstream_rtp = NULL;
			fs3002->hapstream_rtp = vmalloc(tmp);
			if (fs3002->hapstream_rtp == NULL) {
				pr_info( "malloc hapstream_rtp memory failed\n");
				return -ENOMEM;
			}
			break;
		case HAPSTREAM_OFF_MODE:
			pr_info("cmd = HAPSTREAM_OFF_MODE!\n");
			fs3002->hapstream_stop_flag = true;
			vfree(fs3002->hapstream_rtp);
			break;
		case HAPSTREAM_RTP_MODE:
			//pr_info("cmd = HAPSTREAM_RTP_MODE!\n");
			fs3002_hapstream_clean_buf(fs3002, MMAP_BUF_DATA_INVALID);
			fs3002->hapstream_stop_flag = true;
			if (fs3002->vib_stop_flag == false) 
			{
				mutex_lock(&fs3002->lock);
				fs3002_haptic_stop(fs3002);
				mutex_unlock(&fs3002->lock);
			}

			schedule_work(&fs3002->rtp_hapstream);
			break;
		case HAPSTREAM_SETTING_SPEED:
			pr_info("cmd = HAPSTREAM_SETTING_SPEED!, arg = 0x%02lx\n", arg);
			fs3002->dts_info.fs3002_play_rtp_srate = arg;
			fs3002_haptic_set_pwm(fs3002, fs3002->dts_info.fs3002_play_rtp_srate);
			break;
		case HAPSTREAM_GET_SPEED:
			pr_info("cmd = HAPSTREAM_GET_SPEED!\n");
			tmp = fs3002->dts_info.fs3002_play_rtp_srate;
			if (copy_to_user((void __user *)arg, &tmp, sizeof(unsigned int)))
				ret = -EFAULT;
			break;
		case HAPSTREAM_GET_FRE_GAP://f0 numerical range of frequency up and down 
			pr_info("cmd = HAPSTREAM_GET_FRE_GAP!\n");
			tmp = fs3002->dts_info.fs3002_hapstream_fre_gap;
			if (copy_to_user((void __user *)arg, &tmp, sizeof(unsigned int)))
				ret = -EFAULT;
			break;
		case HAPSTREAM_GET_TRANS_CYCLES://how many sine wave cycles in one transient
			pr_info("cmd = HAPSTREAM_GET_TRANS_CYCLES!\n");
			tmp = fs3002->dts_info.fs3002_hapstream_trans_cycles;
			if (copy_to_user((void __user *)arg, &tmp, sizeof(unsigned int)))
				ret = -EFAULT;
			break;			
		default:
			pr_info("unknown cmd = %d\n", cmd);
			break;
	}
	return ret;
}


#define WRITE_RTP_MODE 	1
#define WRITE_STOP_MODE	2

static ssize_t fs3002_buf_write_proc(struct file *filp, const char __user *buffer, size_t count, loff_t *off)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;

	pr_info("enter: count=%d, hapstream_stop_flag=%d, vib_stop_flag=%d\n", (int)count, fs3002->hapstream_stop_flag, fs3002->vib_stop_flag);

	switch (count) 
	{
		case WRITE_RTP_MODE:
			fs3002_hapstream_clean_buf(fs3002, MMAP_BUF_DATA_INVALID);
			fs3002->hapstream_stop_flag = true;
			if (fs3002->vib_stop_flag == false) 
			{
				mutex_lock(&fs3002->lock);
				fs3002_haptic_stop(fs3002);
				mutex_unlock(&fs3002->lock);
			}
			schedule_work(&fs3002->rtp_hapstream);
			break;
		case WRITE_STOP_MODE:
			fs3002->hapstream_stop_flag = true;
			mutex_lock(&fs3002->lock);
			fs3002_haptic_stop(fs3002);
			mutex_unlock(&fs3002->lock);
			break;
		default:
			break;
	}

	return count;
}

static int fs3002_file_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long phys;
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	int ret = 0;

#if LINUX_VERSION_CODE > KERNEL_VERSION(4,7,0)
	vm_flags_t vm_flags = calc_vm_prot_bits(PROT_READ | PROT_WRITE, 0);// | calc_vm_flag_bits(MAP_SHARED);

	vm_flags |= current->mm->def_flags | VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC | VM_SHARED | VM_MAYSHARE;

	if (vma && (pgprot_val(vma->vm_page_prot) != pgprot_val(vm_get_page_prot(vm_flags)))) 
	{
		pr_info("vm_page_prot error!\n");
		return -EPERM;
	}

	if (vma && ((vma->vm_end - vma->vm_start) != (PAGE_SIZE << HAPSTREAM_MMAP_PAGE_ORDER))) 
	{
		pr_info("mmap size check err!\n");
		return -EPERM;
	}
#endif

	pr_info("enter\n");

	phys = virt_to_phys(fs3002->start_buf);

	ret = remap_pfn_range(vma, vma->vm_start, (phys >> PAGE_SHIFT), (vma->vm_end - vma->vm_start), vma->vm_page_prot);
	if (ret) {
		pr_info("mmap failed!\n");
		return ret;
	}

	pr_info("success!\n");

	return ret;
}

#ifdef FS_KERNEL_VER_OVER_5_10 
static const struct proc_ops config_proc_ops = 
{
	.proc_write = fs3002_buf_write_proc,
	.proc_ioctl = fs3002_hapstream_unlocked_ioctl,
	.proc_mmap = fs3002_file_mmap,
};
#else
static const struct file_operations config_proc_ops = 
{
	.owner = THIS_MODULE,
	.write = fs3002_buf_write_proc,
	.unlocked_ioctl = fs3002_hapstream_unlocked_ioctl,
	.mmap = fs3002_file_mmap,
};
#endif

#endif


//okok
static unsigned char fs3002_get_trig_v(struct fs3002 *fs3002, unsigned char uc_value)
{
	return FS3002_BASE_LOWV + uc_value * 2;
}

//okok
static unsigned char fs3002_get_midv(struct fs3002 *fs3002)
{
	return FS3002_BASE_MIDV + fs3002->dts_info.fs3002_midv_ref * 2;
}

//okok
static unsigned char fs3002_get_highv(struct fs3002 *fs3002)
{
	return FS3002_BASE_HIGHV + fs3002->dts_info.fs3002_highv_ref;
}


//okok
static int fs3002_haptic_wait_enter_standby(struct fs3002 *fs3002, unsigned int cnt)
{
	int ret = 0;
	unsigned char reg_val_00 = 0;

	pr_info("enter!\n");
	fs3002->vib_stop_flag = true;
	while (cnt) 
	{
		fs3002_i2c_read(fs3002, FS3002_STATUS, &reg_val_00);
		if((reg_val_00 & 0x01) == 0)
		{
			sprintf(str, "standby state, FS3002_STATUS = %d\n",reg_val_00);
			fs3002_debug_message(fs3002, str);
			break;
		}
		cnt = cnt - 1;
		sprintf(str, "not in standby state, FS3002_STATUS = %d\n",reg_val_00);
		fs3002_debug_message(fs3002, str);
		usleep_range(2000,2500);

	}
	if (!cnt)
	{
		ret = -1;
	}
		
	return ret;
}

//okok
static void fs3002_haptic_auto_break_mode(struct fs3002 *fs3002, bool flag)
{
	pr_info("enter, flag = %d\n", flag);
	fs3002_i2c_write_bits(fs3002, FS3002_BRKCTRL,(unsigned char)flag,7,7);
}

static void fs3002_haptic_set_rtpstartl(struct fs3002 *fs3002, unsigned char value)
{
	pr_info("enter, flag = %d\n", value);
	fs3002_i2c_write(fs3002, FS3002_RTPSTART_L, value);
}

static void fs3002_haptic_set_ilmt(struct fs3002 *fs3002, unsigned char value)
{
	pr_info("enter, ilmt = %d\n", value);
	fs3002_i2c_write_bits(fs3002, FS3002_BSTCTRL2, value, 7, 4);
}


//okok
static int fs3002_haptic_read_lra_f0(struct fs3002 *fs3002)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	pr_info("enter\n");
	//F_LRA_F0_L
	ret = fs3002_i2c_read(fs3002, FS3002_F0CALB_L, &reg_val);
	ret = fs3002_i2c_read(fs3002, FS3002_F0CALB_L, &reg_val);
	f0_reg = reg_val;
	//F_LRA_F0_H
	ret = fs3002_i2c_read(fs3002, FS3002_F0CALB_H, &reg_val);
	f0_reg = f0_reg | (reg_val << 8);
	if (!f0_reg) 
	{
		pr_err("%s:didn't get lra f0 because f0_reg value is 0!\n", FSERROR);
		fs3002->f0 = 0;
		fs3002->f0_cali_status = false;
		return -1;
	} 
	else 
	{
		fs3002->f0_cali_status = true;
		f0_tmp = FS3002_BASE_FRE * 10 / f0_reg;
		fs3002->f0 = (unsigned int)f0_tmp;
		pr_info("lra_f0=%d\n",fs3002->f0);
	}

	return 0;

}

//okok
static void fs3002_haptic_trig1_param_init(struct fs3002 *fs3002)
{
	pr_info("enter\n");

	fs3002->trig[0].trig_level = fs3002->dts_info.fs3002_trig_config[0];
	fs3002->trig[0].trig_polar = fs3002->dts_info.fs3002_trig_config[1];
	fs3002->trig[0].pos_enable = fs3002->dts_info.fs3002_trig_config[2];
	fs3002->trig[0].pos_sequence = fs3002->dts_info.fs3002_trig_config[3];
	fs3002->trig[0].neg_enable = fs3002->dts_info.fs3002_trig_config[4];
	fs3002->trig[0].neg_sequence = fs3002->dts_info.fs3002_trig_config[5];
	fs3002->trig[0].trig_brk = fs3002->dts_info.fs3002_trig_config[6];
	fs3002->trig[0].trig_bst = fs3002->dts_info.fs3002_trig_config[7];
}

//okok
static void fs3002_haptic_trig2_param_init(struct fs3002 *fs3002)
{
	pr_info("enter\n");

	fs3002->trig[1].trig_level = fs3002->dts_info.fs3002_trig_config[8 + 0];
	fs3002->trig[1].trig_polar = fs3002->dts_info.fs3002_trig_config[8 + 1];
	fs3002->trig[1].pos_enable = fs3002->dts_info.fs3002_trig_config[8 + 2];
	fs3002->trig[1].pos_sequence = fs3002->dts_info.fs3002_trig_config[8 + 3];
	fs3002->trig[1].neg_enable = fs3002->dts_info.fs3002_trig_config[8 + 4];
	fs3002->trig[1].neg_sequence = fs3002->dts_info.fs3002_trig_config[8 + 5];
	fs3002->trig[1].trig_brk = fs3002->dts_info.fs3002_trig_config[8 + 6];
	fs3002->trig[1].trig_bst = fs3002->dts_info.fs3002_trig_config[8 + 7];
}

//okok
static void fs3002_haptic_trig3_param_init(struct fs3002 *fs3002)
{
	pr_info("enter\n");
	fs3002->trig[2].trig_level = fs3002->dts_info.fs3002_trig_config[16 + 0];
	fs3002->trig[2].trig_polar = fs3002->dts_info.fs3002_trig_config[16 + 1];
	fs3002->trig[2].pos_enable = fs3002->dts_info.fs3002_trig_config[16 + 2];
	fs3002->trig[2].pos_sequence = fs3002->dts_info.fs3002_trig_config[16 + 3];
	fs3002->trig[2].neg_enable = fs3002->dts_info.fs3002_trig_config[16 + 4];
	fs3002->trig[2].neg_sequence = fs3002->dts_info.fs3002_trig_config[16 + 5];
	fs3002->trig[2].trig_brk = fs3002->dts_info.fs3002_trig_config[16 + 6];
	fs3002->trig[2].trig_bst = fs3002->dts_info.fs3002_trig_config[16 + 7];
}

static unsigned char fs3002_get_env(struct fs3002 *fs3002, unsigned char uc_amp, unsigned char uc_v_ref)
{
	unsigned char uc_env = 0;

	unsigned long ampv = uc_amp * fs3002->gain * uc_v_ref;
	unsigned long midv = FS3002_FULL_AMP * FS3002_FULL_GAIN * fs3002_get_midv(fs3002);
	unsigned long lowv = FS3002_FULL_AMP * FS3002_FULL_GAIN * fs3002->dts_info.fs3002_env_lowv;
	pr_info("enter,uc_amp=%d,uc_v_ref=%d,ampv=%lu,lowv=%lu,midv=%lu\n",uc_amp,uc_v_ref,ampv,lowv,midv);
	if(ampv <= lowv)
	{
		uc_env = 0;
	}
	else if (ampv > midv)
	{
		uc_env = 2;
	}
	else
	{
		uc_env = 1;
	}

	pr_info("ampv=%lu,lowv=%lu,midv=%lu,env=%d\n",ampv,midv,lowv,uc_env);
	return uc_env;
}

//okok
static void fs3002_haptic_trig1_param_config(struct fs3002 *fs3002)
{
	unsigned char reg_76 = 0x33, reg_61 = 0, reg_73 = 0;
	unsigned char env = 0, trigv = 0, ampv = 0;

	pr_info("enter\n");
	//polar, level or edge, brake, bst, (1<<0) means bst always enabled
	reg_76 = (fs3002->trig[0].trig_polar<<3) | (fs3002->trig[0].trig_level<<2) | (fs3002->trig[0].trig_brk<<1) | (fs3002->trig[0].trig_bst<<0);
	fs3002_i2c_write_bits(fs3002, FS3002_TRGCFG7,reg_76,7,4);
	//trig1 pos enable, seq
	reg_61 = (fs3002->trig[0].pos_enable<<7) | (fs3002->trig[0].pos_sequence);
	fs3002_i2c_write(fs3002, FS3002_TRGCFG1,reg_61);
	//trig1 neg enable, seq
	reg_73 = (fs3002->trig[0].neg_enable<<7) | (fs3002->trig[0].neg_sequence);	
	fs3002_i2c_write(fs3002, FS3002_TRGCFG4,reg_73);

	
	//trig1 pos and neg env setting
	if(fs3002->ram.b_over_max_num == true)
	{
		fs3002_i2c_write_bits(fs3002, FS3002_BSTENV1,2,1,0);
		fs3002_i2c_write_bits(fs3002, FS3002_BSTENV2,2,1,0);
	}
	else
	{	
		if(fs3002->trig[0].pos_sequence != 0)
		{
			ampv = fs3002->ram.ram_max[fs3002->trig[0].pos_sequence-1];
			trigv = fs3002_get_trig_v(fs3002, fs3002->dts_info.fs3002_trig1_p_ref);
			env = fs3002_get_env(fs3002, ampv, trigv);
			fs3002_i2c_write_bits(fs3002, FS3002_BSTENV1,env,1,0);
		}
		
		if(fs3002->trig[0].neg_sequence != 0)
		{
			ampv = fs3002->ram.ram_max[fs3002->trig[0].neg_sequence-1];
			trigv = fs3002_get_trig_v(fs3002, fs3002->dts_info.fs3002_trig1_n_ref);
			env = fs3002_get_env(fs3002, ampv, trigv);
			fs3002_i2c_write_bits(fs3002, FS3002_BSTENV2,env,1,0);
		}
	}
}

//okok
static void fs3002_haptic_trig2_param_config(struct fs3002 *fs3002)
{
	unsigned char reg_76 = 0x33, reg_62 = 0, reg_74 = 0;
	unsigned char env = 0, trigv = 0, ampv = 0;
	pr_info("enter\n");

	//polar, level or edge, brake, bst, (1<<0) means bst always enabled
	reg_76 = (fs3002->trig[1].trig_polar<<3) | (fs3002->trig[1].trig_level<<2) | (fs3002->trig[1].trig_brk<<1) | (fs3002->trig[1].trig_bst<<0);
	fs3002_i2c_write_bits(fs3002, FS3002_TRGCFG7,reg_76,3,0);
	//trig2 pos enable, seq
	reg_62 = (fs3002->trig[1].pos_enable<<7) | (fs3002->trig[1].pos_sequence);
	fs3002_i2c_write(fs3002, FS3002_TRGCFG2,reg_62);
	//trig2 neg enable, seq
	reg_74 = (fs3002->trig[1].neg_enable<<7) | (fs3002->trig[1].neg_sequence);
	fs3002_i2c_write(fs3002, FS3002_TRGCFG5,reg_74);
	
	//trig2 pos and neg env setting
	if(fs3002->ram.b_over_max_num == true)
	{
		fs3002_i2c_write_bits(fs3002, FS3002_BSTENV1,2,3,2);
		fs3002_i2c_write_bits(fs3002, FS3002_BSTENV2,2,3,2);
	}
	else
	{	
		if(fs3002->trig[1].pos_sequence != 0)
		{
			ampv = fs3002->ram.ram_max[fs3002->trig[1].pos_sequence-1];
			trigv = fs3002_get_trig_v(fs3002, fs3002->dts_info.fs3002_trig2_p_ref);
			env = fs3002_get_env(fs3002, ampv, trigv);
			fs3002_i2c_write_bits(fs3002, FS3002_BSTENV1,env,3,2);
		}
		
		if(fs3002->trig[1].neg_sequence != 0)
		{
			ampv = fs3002->ram.ram_max[fs3002->trig[1].neg_sequence-1];
			trigv = fs3002_get_trig_v(fs3002, fs3002->dts_info.fs3002_trig2_n_ref);
			env = fs3002_get_env(fs3002, ampv, trigv);
			fs3002_i2c_write_bits(fs3002, FS3002_BSTENV2,env,3,2);
		}
	}
}

//okok
static void fs3002_haptic_trig3_param_config(struct fs3002 *fs3002)
{
	unsigned char reg_77 = 0x30, reg_63 = 0, reg_75 = 0;
	unsigned char env = 0, trigv = 0, ampv = 0;
	pr_info("enter\n");

	//polar, level or edge, brake, bst, (1<<0) means bst always enabled
	reg_77 = (fs3002->trig[2].trig_polar<<3) | (fs3002->trig[2].trig_level<<2) | (fs3002->trig[2].trig_brk<<1) | (fs3002->trig[2].trig_bst<<0);
	fs3002_i2c_write_bits(fs3002, FS3002_TRGCFG8,reg_77,7,4);
	//trig3 pos enable, seq
	reg_63 = (fs3002->trig[2].pos_enable<<7) | (fs3002->trig[2].pos_sequence);
	fs3002_i2c_write(fs3002, FS3002_TRGCFG3,reg_63);
	//trig3 neg enable, seq
	reg_75 = (fs3002->trig[2].neg_enable<<7) | (fs3002->trig[2].neg_sequence);
	fs3002_i2c_write(fs3002, FS3002_TRGCFG6,reg_75);
	
	//trig3 pos and neg env setting
	if(fs3002->ram.b_over_max_num == true)
	{
		fs3002_i2c_write_bits(fs3002, FS3002_BSTENV1,2,5,4);
		fs3002_i2c_write_bits(fs3002, FS3002_BSTENV2,2,5,4);
	}
	else
	{	
		if(fs3002->trig[2].pos_sequence != 0)
		{
			ampv = fs3002->ram.ram_max[fs3002->trig[2].pos_sequence-1];
			trigv = fs3002_get_trig_v(fs3002, fs3002->dts_info.fs3002_trig3_p_ref);
			env = fs3002_get_env(fs3002, ampv, trigv);
			fs3002_i2c_write_bits(fs3002, FS3002_BSTENV1,env,5,4);
		}
		
		if(fs3002->trig[2].neg_sequence != 0)
		{
			ampv = fs3002->ram.ram_max[fs3002->trig[2].neg_sequence-1];
			trigv = fs3002_get_trig_v(fs3002, fs3002->dts_info.fs3002_trig3_n_ref);
			env = fs3002_get_env(fs3002, ampv, trigv);
			fs3002_i2c_write_bits(fs3002, FS3002_BSTENV2,env,5,4);
		}
	}	
}

int fs3002_check_qualify(struct fs3002 *fs3002)
{
	int ret = -1;
	unsigned char reg_val = 0;

	ret = fs3002_i2c_read(fs3002, FS3002_CHIPINI, &reg_val);
	if (ret < 0)
		return ret;
	if (util_get_bit(reg_val,1) != 1)
	{
		pr_err("%s: unqualified chip!", FSERROR);
		return -ERANGE;
	}
	return 0;
}

//okok
void fs3002_haptic_set_rtp_aei(struct fs3002 *fs3002, bool flag)
{
	pr_info("enter flag=%d\n",flag);
	if (flag) 
	{
		fs3002_i2c_write_bits(fs3002, FS3002_INTMASK2, 0, 0, 0);
	}
	else 
	{
		fs3002_i2c_write_bits(fs3002, FS3002_INTMASK2, 1, 0, 0);
	}
}

//okok
static int fs3002_set_base_addr(struct fs3002 *fs3002)
{
	int ret = -1;

	pr_info("enter!\n");
	if (!fs3002->ram.base_addr) 
	{
		pr_err("%s: fs3002 ram base addr is error\n", FSERROR);
		return ret;
	}
	fs3002_i2c_write_bits(fs3002, FS3002_RAMADDR_H,(unsigned char)(fs3002->ram.base_addr >> 8),4,0);
	fs3002_i2c_write(fs3002, FS3002_RAMADDR_L,(unsigned char)(fs3002->ram.base_addr & 0x00ff));

	return 0;
}

//okok
static void fs3002_haptic_raminit(struct fs3002 *fs3002, bool flag)
{
	pr_info("enter flag = %d\n",flag);

	if (flag) 
	{
		fs3002_i2c_write_bits(fs3002, FS3002_RAMACC,0x03,1,0);
	} 
	else 
	{
		fs3002_i2c_write_bits(fs3002, FS3002_RAMACC,0x00,1,0);
	}
}

//okok
static int fs3002_haptic_cont_play(struct fs3002 *fs3002)
{
	pr_info("enter\n");

	//work mode
	fs3002_haptic_set_mode(fs3002, FS3002_HAPTIC_CONT_MODE);
	//cont play go
	fs3002_haptic_play_go(fs3002);
	return 0;
}


void fs3002_interrupt_setup(struct fs3002 *fs3002)
{
	unsigned char reg_val = 0;

	pr_info("enter\n");

	//INT clear 0xAC
	fs3002_i2c_read(fs3002, FS3002_INTSTAT1, &reg_val);
	pr_info("reg INTSTAT1=0x%02X\n", reg_val);
	//INT clear 0xAD
	fs3002_i2c_read(fs3002, FS3002_INTSTAT2, &reg_val);
	pr_info("reg INTSTAT2=0x%02X\n", reg_val);


	//INTM7	7	RW	1h	Interrupt source request mask - OC
	//INTM6	6	RW	1h	Interrupt source request mask - UV
	//INTM5	5	RW	1h	Interrupt source request mask - OV
	//INTM4	4	RW	1h	Interrupt source request mask - OTP
	//INTM3	3	RW	1h	Interrupt source request mask - DC
	//INTM2	2	RW	1h	Interrupt source request mask - VCM fault
	//INTM1	1	RW	1h	Interrupt source request mask - BST OVP
	//INTM0	0	RW	1h	Interrupt source request mask - a fault removal
	
	//Since ae and empty are always 1, if there is a non-sticky interrupt source, 
	//then the interruption will occur all the time, and the default is that it is 
	//sticky, so we need to read the above first to clear out ae and empty. The following 
	//sentence opens all interrupt sources and marks them first, which is not useful, 
	//because even if the interrupt is triggered, there is no special 
	//processing (if the chip is not working properly, just read the status register).
	//close all INT source
	//fs3002_i2c_write_bits(fs3002, FS3002_INTMASK1,0b00000010,7,0);//enable OC,UV,OV,OTP,DC,VCM, reserved,fault removal

}

static void fs3002_interrupt_clear(struct fs3002 *fs3002)
{
	unsigned char reg_val = 0;

	sprintf(str, "%s, enter\n", __func__);
	fs3002_debug_message(fs3002, str);
	fs3002_i2c_read(fs3002, FS3002_INTSTAT1, &reg_val);
	sprintf(str,"%s,reg SYSINT=0x%02X\n",__func__, reg_val);
	fs3002_debug_message(fs3002, str);
	fs3002_i2c_read(fs3002, FS3002_INTSTAT2, &reg_val);
	sprintf(str,"%s,reg SYSINT=0x%02X\n",__func__, reg_val);
	fs3002_debug_message(fs3002, str);
}


static int fs3002_haptic_read_cont_f0(struct fs3002 *fs3002)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	pr_info("enter\n");
	ret = fs3002_i2c_read(fs3002, FS3002_F0TRK_L, &reg_val);
	ret = fs3002_i2c_read(fs3002, FS3002_F0TRK_L, &reg_val);
	f0_reg = reg_val;
	ret = fs3002_i2c_read(fs3002, FS3002_F0TRK_H, &reg_val);
	f0_reg = f0_reg | (reg_val << 8);

	if (!f0_reg) 
	{
		pr_err("%s:didn't get cont f0 because f0_reg value is 0!\n", FSERROR);
		fs3002->cont_f0 = fs3002->dts_info.fs3002_f0_ref;
		return -1;
	} 
	else 
	{
		f0_tmp = FS3002_BASE_FRE * 10 / f0_reg;
		fs3002->cont_f0 = (unsigned int)f0_tmp;
		pr_info("cont_f0=%d\n",fs3002->cont_f0);
	}
	return 0;

}

//okok
static int fs3002_haptic_cont_get_f0(struct fs3002 *fs3002)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int cnt = 200;
	bool get_f0_flag = false;

	pr_info("enter\n");
	fs3002->f0 = fs3002->dts_info.fs3002_f0_ref;
	// enter standby mode
	fs3002_haptic_stop(fs3002);
	
	// cont play go
	fs3002_haptic_play_go(fs3002);
	// 300ms
	while (cnt) 
	{
		fs3002_i2c_read(fs3002, FS3002_DIGSTAT, &reg_val);
		if ((reg_val >> 4) == FS3002_DIGSTAT_B7_4_OPS_OFF) 
		{
			get_f0_flag = true;
			pr_info("entered standby mode! state=0x%02X,count=%d\n", reg_val,cnt);
			cnt = 0;
		} 
		else 
		{
			cnt--;
			pr_info("waitting for standby, state=0x%02X\n", reg_val);
		}
		usleep_range(10000, 10500);
	}
	
	if (get_f0_flag) 
	{
		fs3002_haptic_read_lra_f0(fs3002);
		fs3002_haptic_read_cont_f0(fs3002);
	} 
	else 
	{
		pr_err("%s:enter standby mode failed, stop reading f0!\n", FSERROR);
	}
	return ret;
}

//okok
static void fs3002_haptic_upload_lra(struct fs3002 *fs3002, unsigned int flag)
{
	sprintf(str,"%s,enter flag=%d\n",__func__,flag);
	fs3002_debug_message(fs3002,str);
	switch (flag) 
	{
		case FS3002_WRITE_ZERO:
			sprintf(str,"%s,write zero to trim_lra!\n",__func__);
			fs3002_debug_message(fs3002, str);
			fs3002_i2c_write(fs3002, FS3002_FTUNECFG,0x90);
			break;
		case FS3002_F0_CALI:
			sprintf(str,"%s,write f0_cali_data to trim_lra = 0x%02X\n",__func__,fs3002->f0_cali_data);
			fs3002_debug_message(fs3002, str);
			fs3002_i2c_write(fs3002, FS3002_FTUNECFG,(unsigned char)fs3002->f0_cali_data);
			break;
		case FS3002_OSC_CALI:
			sprintf(str, "%s,write osc_cali_data to trim_lra = 0x%02X\n",__func__, fs3002->osc_cali_data);
			fs3002_debug_message(fs3002, str);
			fs3002_i2c_write(fs3002, FS3002_FTUNECFG,(unsigned char)fs3002->osc_cali_data);
			break;
		default:
			break;
	}
}

static unsigned char fs3002_haptic_osc_read_status(struct fs3002 *fs3002)
{
	unsigned char reg_val = 0;
	//pr_info("enter\n");//too much message

	fs3002_i2c_read(fs3002, FS3002_INTSTATR2, &reg_val);
	return reg_val;
}


static int fs3002_haptic_get_vbat(struct fs3002 *fs3002)
{
	unsigned char reg_val = 0;
	unsigned int vbat_code = 0;
	
	sprintf(str, "%s,enter\n", __func__);
	fs3002_debug_message(fs3002, str);

	fs3002_haptic_stop(fs3002);

	fs3002_i2c_read(fs3002, FS3002_BATS_L, &reg_val);
	fs3002_i2c_read(fs3002, FS3002_BATS_L, &reg_val);
	vbat_code = reg_val;
	reg_val = fs3002_i2c_read_bits(fs3002, FS3002_BATS_H, 1,0);
	vbat_code = vbat_code | (reg_val << 8);
	
	fs3002->vbat = 6000 * vbat_code / 1024;
	//zzzz, pay attantion to (remove?)
	if (fs3002->vbat > FS3002_VBAT_MAX) 
	{
		fs3002->vbat = FS3002_VBAT_MAX;
		pr_info("vbat max limit = %dmV\n", fs3002->vbat);
	}
	if (fs3002->vbat < FS3002_VBAT_MIN) 
	{
		fs3002->vbat = FS3002_VBAT_MIN;
		pr_info("vbat min limit = %dmV\n", fs3002->vbat);
	}
	pr_info("fs3002->vbat=%dmV, vbat_code=0x%02X\n", fs3002->vbat, vbat_code);
	return 0;

}

static unsigned int get_d2sgain(unsigned char uc_0x2A)
{
	switch (uc_0x2A)
	{
	case 0:
		return 25;
	case 1:
		return 50;
	case 2:
		return 100;
	case 3:
		return 200;
	case 4:
		return 400;
	case 5:
	case 6:
	case 7:
		return 800;
	case 8:
		return 125;
	case 9:
		return 250;
	case 10:
		return 500;
	case 11:
		return 1000;
	case 12:
		return 2000;
	case 13:
	case 14:
	case 15:
		return 4000;
	default:
		return 800;
	}
}


static int fs3002_haptic_get_lra_resistance(struct fs3002 *fs3002)
{
	unsigned char uc_0xBC = 0;
	unsigned char uc_0xBD = 0;
 	unsigned int ui_BCBD = 0;
	unsigned char uc_0x2A = 0;
	int i = 0;
	
	pr_info("enter\n");

	mutex_lock(&fs3002->lock);
	fs3002_haptic_stop(fs3002);
	fs3002_haptic_diagnostic_sequence(fs3002);

	fs3002_i2c_write(fs3002, FS3002_SWDIAG1, 0xFA);//0xB5 = 0xFA		// Turn of S2F, turn on RL detection current sink
	if(fs3002->lra_test_sleep_time != 0)
	{
		//Wait > 100 us(100); 
		usleep_range(fs3002->lra_test_sleep_time, fs3002->lra_test_sleep_time+10);
	}

	ui_BCBD = 0;
	for(i=0;i<FS3002_OFFSET_RETRIES;i++)
	{
		fs3002_i2c_write(fs3002, FS3002_SARCTRL, 0x84);//0xBB = 0x84		// SAR capture data
		if(fs3002->lra_test_sleep_time != 0)
		{
			//Wait > 10 us
			usleep_range(fs3002->lra_test_sleep_time, fs3002->lra_test_sleep_time+10);
		}
		fs3002_i2c_read(fs3002, FS3002_SARTS_L, &uc_0xBC);//0xBC//low 8 offset value
		fs3002_i2c_read(fs3002, FS3002_SARTS_L, &uc_0xBC);//0xBC//low 8 offset value
		uc_0xBD = fs3002_i2c_read_bits(fs3002, FS3002_SARTS_H, 1,0);//0xBD//high 2 offset value
		ui_BCBD = (uc_0xBD<<8 | uc_0xBC) + ui_BCBD;
	}
	ui_BCBD = ui_BCBD/FS3002_OFFSET_RETRIES;
	uc_0x2A = fs3002_i2c_read_bits(fs3002, FS3002_PGACTRL, 3,0);

	fs3002->lra = abs(fs3002->offset - ui_BCBD) * 428600 / (1024 * get_d2sgain(uc_0x2A));
	fs3002->lra = fs3002->lra *100;//need to * 100 (In get_d2sgain function, gain has been amplified by 100 times )
	pr_info("fs3002->offset=%d,ui_BCBD=%d,uc_0x2A=%d,fs3002->lra=%d\n",fs3002->offset,ui_BCBD,uc_0x2A,fs3002->lra);
	fs3002_haptic_set_diagnostic_reg_to_default_value(fs3002);
	mutex_unlock(&fs3002->lock);
	
	pr_info("exit\n");
	return 0;
}



static int fs3002_trig_config(struct fs3002 *fs3002)
{
	pr_info("enter!\n");

	fs3002_haptic_trig1_param_init(fs3002);
	fs3002_haptic_trig1_param_config(fs3002);
	fs3002_haptic_trig2_param_init(fs3002);
	fs3002_haptic_trig2_param_config(fs3002);
	fs3002_haptic_trig3_param_init(fs3002);
	fs3002_haptic_trig3_param_config(fs3002);
	return 0;
}

//okok
static int fs3002_haptic_set_trig_vref(struct fs3002 *fs3002)
{
	pr_info("enter!\n");
	fs3002_i2c_write_bits(fs3002, FS3002_TRGCFG9, fs3002->dts_info.fs3002_trig1_p_ref, 5, 0);
	fs3002_i2c_write_bits(fs3002, FS3002_TRGCFG10, fs3002->dts_info.fs3002_trig1_n_ref, 5, 0);
	fs3002_i2c_write_bits(fs3002, FS3002_TRGCFG11, fs3002->dts_info.fs3002_trig2_p_ref, 5, 0);
	fs3002_i2c_write_bits(fs3002, FS3002_TRGCFG12, fs3002->dts_info.fs3002_trig2_n_ref, 5, 0);
	fs3002_i2c_write_bits(fs3002, FS3002_TRGCFG13, fs3002->dts_info.fs3002_trig3_p_ref, 5, 0);
	fs3002_i2c_write_bits(fs3002, FS3002_TRGCFG14, fs3002->dts_info.fs3002_trig3_n_ref, 5, 0);
	return 0;
}


//okok
static int fs3002_haptic_set_vref(struct fs3002 *fs3002)
{
	pr_info("enter!\n");
	fs3002_i2c_write_bits(fs3002, FS3002_PWMCTRL, fs3002->dts_info.fs3002_lowv_ref, 7, 4);
	fs3002_i2c_write_bits(fs3002, FS3002_ADPBST1, fs3002->dts_info.fs3002_midv_ref, 5, 4);
	fs3002_i2c_write_bits(fs3002, FS3002_BSTCTRL1, fs3002->dts_info.fs3002_highv_ref, 5, 0);
	return 0;
}

//okok
static int fs3002_haptic_set_pwm(struct fs3002 *fs3002, unsigned char mode)
{
	sprintf(str, "%s, pwm mode=%d\n", __func__,mode);
	fs3002_debug_message(fs3002, str);

	fs3002_i2c_write_bits(fs3002, FS3002_SYSCTRL,mode,5,4);
	return 0;
}


//okok
static int fs3002_haptic_swicth_motor_protect_config(struct fs3002 *fs3002,bool flag)
{
	pr_info("enter\n");
	fs3002_i2c_write_bits(fs3002, FS3002_DCCTRL,flag? 1:0,7,7);
	return 0;
}

//okok
static void fs3002_haptic_set_gain(struct fs3002 *fs3002, unsigned char gain)
{
	pr_info("enter, gain = 0x%x\n", gain);
	fs3002_i2c_write(fs3002, FS3002_GAINCFG, gain);

}

void fs3002_haptic_ff_set_gain_work_routine(struct work_struct *work)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;

	if (fs3002->new_gain >= 0x7FFF)
		fs3002->level = 0x80;	/*128 */
	else if (fs3002->new_gain <= 0x3FFF)
		fs3002->level = 0x1E;	/*30 */
	else
		fs3002->level = (fs3002->new_gain - 16383) / 128;

	if (fs3002->level < 0x1E)
		fs3002->level = 0x1E;	/*30 */
	
	pr_info("enter, set_gain queue work, new_gain = 0x%x, level = 0x%x, vbat=%d, vbat_min=%d, vbat_ref=%d\n", fs3002->new_gain, fs3002->level, fs3002->vbat, FS3002_VBAT_MIN, FS3002_VBAT_REFER);
	fs3002_i2c_write(fs3002, FS3002_GAINCFG, fs3002->level);
}

void fs3002_haptic_ff_set_gain(struct input_dev *dev, u16 gain)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;

	pr_debug("enter, gain=%d\n",gain);
	fs3002->new_gain = gain;
	queue_work(fs3002->work_queue, &fs3002->set_gain_work);
}


static enum hrtimer_restart fs3002_haptic_audio_timer_func(struct hrtimer *timer)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;

	pr_debug("enter\n");
	queue_work(fs3002->work_queue, &fs3002->haptic_audio.work);

	hrtimer_start(&fs3002->haptic_audio.timer, ktime_set(fs3002->haptic_audio.timer_val / 1000000, (fs3002->haptic_audio.timer_val % 1000000) * 1000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

//okok
static int fs3002_haptic_set_wav_seq(struct fs3002 *fs3002,unsigned char wav, unsigned char seq)
{
	sprintf(str, "%s, enter wav=%d  seq=%d\n", __func__,wav,seq);
	fs3002_debug_message(fs3002, str);
	
	fs3002_i2c_write_bits(fs3002, FS3002_WFSCFG1 + wav, seq, 6, 0);
	return 0;
}

//okok
static int fs3002_haptic_set_wav_loop(struct fs3002 *fs3002,unsigned char wav, unsigned char loop)
{

	sprintf(str, "%s, enter  wav=%d   loop=%d\n", __func__,wav,loop);
	fs3002_debug_message(fs3002, str);

	
	if (wav % 2 == 0) 
	{
		fs3002_i2c_write_bits(fs3002, FS3002_WFSLOOP1 + (wav / 2),loop,3,0);
	} 
	else 
	{
		fs3002_i2c_write_bits(fs3002, FS3002_WFSLOOP1 + (wav / 2),loop,7,4);
	}
	return 0;
}


//okok
static void fs3002_haptic_play_go(struct fs3002 *fs3002)
{
	sprintf(str, "%s, enter\n", __func__);
	fs3002_debug_message(fs3002, str);

	fs3002_i2c_write(fs3002, FS3002_OPCTRL, 0x00);//Do a stop action regardless of whether the flag is false
	fs3002_i2c_write(fs3002, FS3002_OPCTRL, 0x01);

	fs3002->vib_stop_flag = false;
}

static int fs3002_haptic_set_repeat_wav_seq(struct fs3002 *fs3002,unsigned char seq)
{
	pr_info("enter repeat wav seq %d\n", seq);
	fs3002_haptic_set_wav_seq(fs3002, 0x00, seq);
	fs3002_haptic_set_wav_loop(fs3002, 0x00,FS3002_WFSLOOP_INIFINITELY);
	return 0;
}


//okok
static int fs3002_haptic_set_mode(struct fs3002 *fs3002, unsigned char play_mode)
{
	pr_info("enter, play_mode=%d\n", play_mode);

	switch (play_mode) 
	{
		case FS3002_HAPTIC_STANDBY_MODE:
			sprintf(str, "%s, enter standby mode\n", __func__);
			fs3002_debug_message(fs3002, str);
			
			fs3002->play_mode = FS3002_HAPTIC_STANDBY_MODE;
			fs3002_i2c_write_bits(fs3002, FS3002_SYSCTRL,0,1,0);
			fs3002_haptic_stop(fs3002);			
			// bst_sel 0
			fs3002_i2c_write_bits(fs3002, FS3002_BSTCTRL2,0,2,2);//BST_SEL=0
			// disable f0 detect
			fs3002_i2c_write_bits(fs3002, FS3002_FDETCTRL,0,7,7);
			// disable f0 self tuning
			fs3002_i2c_write_bits(fs3002, FS3002_FDETCTRL,1,4,4);
			break;
		case FS3002_HAPTIC_RAM_MODE:
			sprintf(str, "%s, enter ram mode\n", __func__);
			fs3002_debug_message(fs3002, str);
			
			fs3002->play_mode = FS3002_HAPTIC_RAM_MODE;
			fs3002_i2c_write_bits(fs3002, FS3002_SYSCTRL,0,1,0);		
			// bst_sel 1
			fs3002_i2c_write_bits(fs3002, FS3002_BSTCTRL2,1,2,2);//BST_SEL=1
			// disable f0 detect
			fs3002_i2c_write_bits(fs3002, FS3002_FDETCTRL,0,7,7);
			// disable f0 self tuning
			fs3002_i2c_write_bits(fs3002, FS3002_FDETCTRL,1,4,4);				
			break;
		case FS3002_HAPTIC_RAM_LOOP_MODE:
			sprintf(str, "%s, enter ram loop mode\n", __func__);
			fs3002_debug_message(fs3002, str);
			
			fs3002->play_mode = FS3002_HAPTIC_RAM_LOOP_MODE;
			fs3002_i2c_write_bits(fs3002, FS3002_SYSCTRL,0,1,0);		
			// bst_sel 0
			fs3002_i2c_write_bits(fs3002, FS3002_BSTCTRL2,0,2,2);//BST_SEL=0
			// all seq env = 1
			fs3002_i2c_write_bits(fs3002, FS3002_BSTENV4,0x33,7,0);
			fs3002_i2c_write_bits(fs3002, FS3002_BSTENV3,0x33,7,0);
			// disable f0 detect
			fs3002_i2c_write_bits(fs3002, FS3002_FDETCTRL,0,7,7);
			// disable f0 self tuning
			fs3002_i2c_write_bits(fs3002, FS3002_FDETCTRL,1,4,4);				
			break;
		case FS3002_HAPTIC_RTP_MODE:
			sprintf(str, "%s, enter rtp mode\n", __func__);
			fs3002_debug_message(fs3002, str);
			
			fs3002->play_mode = FS3002_HAPTIC_RTP_MODE;
			fs3002_i2c_write_bits(fs3002, FS3002_SYSCTRL,1,1,0);		
			// bst_sel
			if(fs3002->dts_info.fs3002_rtp_bst_sel)
			{
				fs3002_i2c_write_bits(fs3002, FS3002_BSTCTRL2,1,2,2);
			}
			else
			{
				fs3002_i2c_write_bits(fs3002, FS3002_BSTCTRL2,0,2,2);
			}	
			// disable f0 detect
			fs3002_i2c_write_bits(fs3002, FS3002_FDETCTRL,0,7,7);
			// disable f0 self tuning
			fs3002_i2c_write_bits(fs3002, FS3002_FDETCTRL,1,4,4);
			// set auto env				
			fs3002_haptic_rtp_set_auto_env(fs3002);
			// set pwm
			fs3002_haptic_set_pwm(fs3002, fs3002->dts_info.fs3002_play_rtp_srate);
			break;
		case FS3002_HAPTIC_TRIG_MODE:
			sprintf(str, "%s, enter trig mode\n", __func__);
			fs3002_debug_message(fs3002, str);
			
			fs3002->play_mode = FS3002_HAPTIC_TRIG_MODE;	
			// bst_sel 1
			fs3002_i2c_write_bits(fs3002, FS3002_BSTCTRL2,1,2,2);//BST_SEL=1
			// disable f0 detect
			fs3002_i2c_write_bits(fs3002, FS3002_FDETCTRL,0,7,7);
			// disable f0 self tuning
			fs3002_i2c_write_bits(fs3002, FS3002_FDETCTRL,1,4,4);

			break;
		case FS3002_HAPTIC_CONT_MODE:
			sprintf(str, "%s, enter cont mode\n", __func__);
			fs3002_debug_message(fs3002, str);
			
			fs3002->play_mode = FS3002_HAPTIC_CONT_MODE;
			fs3002_i2c_write_bits(fs3002, FS3002_SYSCTRL,2,1,0);		
			// bst_sel 0
			fs3002_i2c_write_bits(fs3002, FS3002_BSTCTRL2,0,2,2);//BST_SEL=0
			// cont env = 1
			fs3002_i2c_write_bits(fs3002, FS3002_BSTENV1,1,7,6);
			// disable f0 detect
			fs3002_i2c_write_bits(fs3002, FS3002_FDETCTRL,0,7,7);
			// disable f0 self tuning
			fs3002_i2c_write_bits(fs3002, FS3002_FDETCTRL,1,4,4);					
			break;
		case FS3002_HAPTIC_F0_DETECT_MODE:
			pr_info("enter F0 detect mode\n");
			fs3002->play_mode = FS3002_HAPTIC_F0_DETECT_MODE;
			fs3002_i2c_write_bits(fs3002, FS3002_SYSCTRL,2,1,0);
			// bst_sel 0
			fs3002_i2c_write_bits(fs3002, FS3002_BSTCTRL2,0,2,2);//BST_SEL=0
			// cont env = 1
			fs3002_i2c_write_bits(fs3002, FS3002_BSTENV1,1,7,6);			
			// enable f0 detect
			fs3002_i2c_write_bits(fs3002, FS3002_FDETCTRL,1,7,7);
			// disable f0 self tuning
			fs3002_i2c_write_bits(fs3002, FS3002_FDETCTRL,1,4,4);
			break;
		case FS3002_HAPTIC_F0_CALI_MODE:
			pr_info("enter F0 cali mode\n");
			fs3002->play_mode = FS3002_HAPTIC_F0_CALI_MODE;
			fs3002_i2c_write_bits(fs3002, FS3002_SYSCTRL,3,1,0);
			// bst_sel 0
			fs3002_i2c_write_bits(fs3002, FS3002_BSTCTRL2,0,2,2);//BST_SEL=0
			// cont env = 1
			fs3002_i2c_write_bits(fs3002, FS3002_BSTENV1,1,7,6);			
			// enable f0 detect
			fs3002_i2c_write_bits(fs3002, FS3002_FDETCTRL,1,7,7);
			// enable f0 self tuning
			fs3002_i2c_write_bits(fs3002, FS3002_FDETCTRL,0,4,4);
			break;			
		default:
			pr_err("%s: play mode %d error",FSERROR, play_mode);
			break;
	}
	return 0;
}

//okok
static int fs3002_haptic_stop(struct fs3002 *fs3002)
{
	int ret = 0;

	sprintf(str, "%s, enter, vib_stop_flag = %d\n", __func__, fs3002->vib_stop_flag);
	fs3002_debug_message(fs3002, str);

	if (fs3002->vib_stop_flag == true)
	{
		return 0;
	}
	fs3002->play_mode = FS3002_HAPTIC_STANDBY_MODE;

	fs3002_i2c_write(fs3002, FS3002_OPCTRL, 0);
	ret = fs3002_haptic_wait_enter_standby(fs3002, 40);

	if (ret < 0) 
	{
		pr_err("%s force to enter standby mode gain!\n", FSERROR);
		fs3002_i2c_write(fs3002, FS3002_OPCTRL, 0);
	}
	return 0;
}

//now, this work is done in fs3002_ram_loaded
/*
static int fs3002_haptic_get_ram_num_and_max(struct fs3002 *fs3002)
{
	unsigned char i = 0;
	unsigned char reg_val = 0;
	unsigned char ram_data[3] = {0};
	unsigned int first_wave_addr = 0, ram_start[FS3002_RAM_NUM_MAX] = {0}, ram_end[FS3002_RAM_NUM_MAX] = {0}, j = 0;
	signed char sc_data = 0;
	pr_info("enter!\n");
	if (!fs3002->ram_init) 
	{
		pr_err("ram init faild, ram_num = 0!\n");
		return -EPERM;
	}
	
	mutex_lock(&fs3002->lock);
	//RAMINIT Enable
	fs3002_haptic_raminit(fs3002, true);
	fs3002_haptic_stop(fs3002);
	fs3002_set_base_addr(fs3002);
	for (i = 0; i < 3; i++) 
	{
		fs3002_i2c_read(fs3002, FS3002_RAMRDATA, &reg_val);
		ram_data[i] = reg_val;
	}
	first_wave_addr = (ram_data[1] << 8 | ram_data[2]);
	fs3002->ram.ram_num = (first_wave_addr - fs3002->ram.base_addr - 1) / 4;

	pr_info("ram_version = 0x%02x, first waveform addr = 0x%04x, ram_num = = %d\n", ram_data[0], first_wave_addr, fs3002->ram.ram_num);

	//clear ram.ram_max
	for (i = 0; i < FS3002_RAM_NUM_MAX; i++) 
	{
		fs3002->ram.ram_max[i] = 0;
	}
		
	if(fs3002->ram.ram_num > FS3002_RAM_NUM_MAX)
	{
		pr_err("%s: ram number is over %d\n", FSERROR, FS3002_RAM_NUM_MAX);
		fs3002->ram.b_over_max_num = true;
	}
	else
	{
		if(fs3002->ram.ram_num == 0)
		{
			pr_err("%s: ram number is 0\n", FSERROR);
			fs3002->ram.b_over_max_num = true;
		}
		else
		{
			fs3002->ram.b_over_max_num = false;

			ram_start[j] = first_wave_addr - fs3002->ram.base_addr;
			for (i = 0; i < fs3002->ram.ram_num; i++) 
			{
				fs3002_i2c_read(fs3002, FS3002_RAMRDATA, &ram_data[1]);
				fs3002_i2c_read(fs3002, FS3002_RAMRDATA, &ram_data[2]);
				if(i == 0)
				{
					ram_end[j] = (ram_data[1] << 8 | ram_data[2]) - fs3002->ram.base_addr;
				}
				else
				{
					ram_start[j] = (ram_data[1] << 8 | ram_data[2]) - fs3002->ram.base_addr;
					fs3002_i2c_read(fs3002, FS3002_RAMRDATA, &ram_data[1]);
					fs3002_i2c_read(fs3002, FS3002_RAMRDATA, &ram_data[2]);
					ram_end[j] = (ram_data[1] << 8 | ram_data[2]) - fs3002->ram.base_addr;
				}	
				j = j + 1;
				pr_info("bin start:0x%2x, end:0x%2x\n", ram_start[i], ram_end[i]);
			}

			for (i = 0; i < fs3002->ram.ram_num; i++) 
			{
				for (j = 0; j < ram_end[i] - ram_start[i] + 1 ; j++) 
				{
					fs3002_i2c_read(fs3002, FS3002_RAMRDATA, &reg_val);
					sc_data = (signed char)reg_val;
					if(sc_data > fs3002->ram.ram_max[i])
					{				
						fs3002->ram.ram_max[i] = sc_data;
					}
				}
				pr_info("ram[%d] max = %d\n", i, fs3002->ram.ram_max[i]);
			}		
		}
	}
	
	//RAMINIT Disable
	fs3002_haptic_raminit(fs3002, false);
	mutex_unlock(&fs3002->lock);

	return 0;
}
*/

static int fs3002_rtp_osc_calibration(struct fs3002 *fs3002)
{
	const struct firmware *rtp_file;
	int ret = -1;
	unsigned int buf_len = 0;
	unsigned char osc_int_state = 0;
	unsigned char fre_val = 0;
	unsigned int theory_time = 0;

	fs3002->rtp_cnt = 0;
	fs3002->timeval_flags = 1;

	pr_info("enter\n");
	//fw loaded
	ret = request_firmware(&rtp_file, fs3002_rtp_name[0], fs3002->dev);
	if (ret < 0) 
	{
		pr_err("%s:failed to read %s\n", FSERROR,fs3002_rtp_name[0]);
		return ret;
	}
	//foursemi add stop,for irq interrupt during calibrate
	fs3002_haptic_stop(fs3002);
	mutex_lock(&fs3002->rtp_lock);
	vfree(fs3002->rtp_container);
	fs3002->rtp_container = vmalloc(rtp_file->size + sizeof(int));
	if (!fs3002->rtp_container) 
	{
		release_firmware(rtp_file);
		mutex_unlock(&fs3002->rtp_lock);
		pr_err("%s:error allocating memory\n", FSERROR);
		return -ENOMEM;
	}
	fs3002->rtp_container->len = rtp_file->size;
	fs3002->rtp_len = rtp_file->size;
	pr_info("rtp file:[%s] size = %dbytes\n",fs3002_rtp_name[0], fs3002->rtp_container->len);

	memcpy(fs3002->rtp_container->data, rtp_file->data, rtp_file->size);
	release_firmware(rtp_file);
	mutex_unlock(&fs3002->rtp_lock);
	// rtp mode config
	fs3002_haptic_set_mode(fs3002, FS3002_HAPTIC_RTP_MODE);
	//burst mode enable
	fs3002_haptic_raminit(fs3002,true);

	disable_irq(gpio_to_irq(fs3002->irq_gpio));
	//haptic go
	fs3002_haptic_play_go(fs3002);
	fs3002_interrupt_clear(fs3002);//need to do it immediately, otherwise fs3002_haptic_rtp_get_fifo_afs_0xAF will get wrong result
	//require latency of CPU & DMA not more then PM_QOS_VALUE_VB us
	pm_qos_enable(fs3002, true);
	while (1) 
	{
		//!almost full
		if (!fs3002_haptic_rtp_get_fifo_afs_0xAF(fs3002)) 
		{
			mutex_lock(&fs3002->rtp_lock);
			if ((fs3002->rtp_container->len - fs3002->rtp_cnt) < (fs3002->ram.base_addr >> 2))
				buf_len = fs3002->rtp_container->len - fs3002->rtp_cnt;
			else
				buf_len = (fs3002->ram.base_addr >> 2);

			//transfered file len != total file len
			if (fs3002->rtp_cnt != fs3002->rtp_container->len) 
			{
				if (fs3002->timeval_flags == 1) 
				{
					fre_val = fs3002_i2c_read_bits(fs3002, FS3002_SYSCTRL, 5,4);
					theory_time = get_theory_time(fs3002,fre_val);
					pr_info("Please waiting for about %d theory_time\n",theory_time);
					//start
					ktime_get_real_ts64(&fs3002->start);
					fs3002->timeval_flags = 0;
				}
				fs3002_i2c_writes(fs3002,FS3002_RTPWDATA,&fs3002->rtp_container->data[fs3002->rtp_cnt],buf_len);
				fs3002->rtp_cnt += buf_len;
			}
			mutex_unlock(&fs3002->rtp_lock);
		}
		osc_int_state = fs3002_haptic_osc_read_status(fs3002);
		if (util_get_bit(osc_int_state,2)==1) //bit2-empty happened
		{
			ktime_get_real_ts64(&fs3002->end);
			pr_info("osc trim playback done fs3002->rtp_cnt= %d\n", fs3002->rtp_cnt);
			break;
		}
		ktime_get_real_ts64(&fs3002->end);
		fs3002->microsecond = (fs3002->end.tv_sec - fs3002->start.tv_sec) * 1000000 + (fs3002->end.tv_nsec - fs3002->start.tv_nsec) / 1000000;
		if (fs3002->microsecond > FS3002_OSC_CALI_MAX_LENGTH) 
		{
			pr_err("%s:osc trim time out! fs3002->rtp_cnt %d osc_int_state %02x\n", FSERROR, fs3002->rtp_cnt, osc_int_state);
			break;
		}
	}
	pm_qos_enable(fs3002, false);
	enable_irq(gpio_to_irq(fs3002->irq_gpio));

	//burst mode disable
	fs3002_haptic_raminit(fs3002,false);

	fs3002->microsecond =(fs3002->end.tv_sec - fs3002->start.tv_sec) * 1000000 +(fs3002->end.tv_nsec - fs3002->start.tv_nsec)  / 1000000;
	//calibration osc
	pr_info("foursemi_microsecond: %ld\n",fs3002->microsecond);
	pr_info("exit\n");
	return 0;
}

//okok
//Calculate ftv based on actual duration and file duration
//lra_code = (100 * (real_time - theory_time) + 257064068) / 1776596;   from chip designer
static int fs3002_osc_trim_calculation(unsigned long int theory_time,unsigned long int real_time)
{
	unsigned int lra_code = 0;
	unsigned int DFT_LRA_TRIM_CODE = 0x90;

	pr_info("enter\n");
	if (theory_time == real_time) 
	{
		pr_info("theory_time == real_time: %ld, no need to calibrate!\n", real_time);
		return DFT_LRA_TRIM_CODE;
	}
	else if(abs(real_time - theory_time)> (theory_time / 50))
	{
		pr_err("%s:|real_time - theory_time| > (theory_time/50), can't calibrate!\n",FSERROR);
		return DFT_LRA_TRIM_CODE;
	}
	else
	{
		 lra_code = (100 * (real_time - theory_time) + 257064068) / 1776596;
	}
	
	pr_info("real_time: %ld, theory_time: %ld, lra_code:0x%02X\n", real_time,theory_time,lra_code);
	return lra_code;
}

static unsigned int get_theory_time(struct fs3002 *fs3002,unsigned char fre_val)
{
	unsigned int theory_time = 0;

	if (fre_val == 2 || fre_val == 3)
		theory_time = (fs3002->rtp_len / 48000) * 1000000;	//48K
	if (fre_val == 0)
		theory_time = (fs3002->rtp_len / 12000) * 1000000;	//12K
	if (fre_val == 1)
		theory_time = (fs3002->rtp_len / 24000) * 1000000;	//24K
	return theory_time;
}

//okok
//compare fs3002->microsecond and theory_time, get fs3002->osc_cali_data and set this value to osc
static int fs3002_rtp_trim_lra_calibration(struct fs3002 *fs3002)
{
	unsigned char fre_val = 0;
	unsigned int theory_time = 0;
	unsigned int lra_trim_code = 0;
	pr_info("enter\n");

	fre_val = fs3002_i2c_read_bits(fs3002, FS3002_SYSCTRL, 5,4);

	theory_time = get_theory_time(fs3002,fre_val);

	pr_info("microsecond:%ld  theory_time = %d\n", fs3002->microsecond, theory_time);

	lra_trim_code = fs3002_osc_trim_calculation(theory_time,fs3002->microsecond);
	if (lra_trim_code >= 0) 
	{
		fs3002->osc_cali_data = lra_trim_code;
		fs3002_haptic_upload_lra(fs3002, FS3002_OSC_CALI);
	}
	return 0;
}


//okok
static unsigned int fs3002_haptic_cal_0x7B_value(long long ll_f0,long long ll_f0_ref)
{
	unsigned int ui_0x7B = 0x90;
	long long ll_osc_deviation = 0;

	pr_info("enter: f0=%lld,f0_ref=%lld\n", ll_f0, ll_f0_ref);

	ll_f0 = ll_f0 * 1000000000;

	ll_osc_deviation = (ll_f0 / ll_f0_ref) - 1000000000;
	ui_0x7B = (unsigned int)((ll_osc_deviation + 257064068) / 1776596);

	if (ui_0x7B<0 || ui_0x7B>255)
	{
		pr_err("%s:f0_ref setting is over current haptic range\n",FSERROR);
		return 0x90;
	}
	pr_info("need to set 0x7B=0x%02X\n", ui_0x7B);
	return ui_0x7B;

}


//okok
static int fs3002_haptic_f0_calibration(struct fs3002 *fs3002)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int ui_0x7B = 0x90;

	pr_info("enter\n");

	//set f0 ref
	fs3002_haptic_set_f0_ref(fs3002);
	fs3002_i2c_write(fs3002, FS3002_FTUNECFG, 0x90);//cali will fail if  FTUNECFG == 0 or 0xff

	fs3002_haptic_upload_lra(fs3002,FS3002_WRITE_ZERO);
	if(fs3002->dts_info.fs3002_f0_cali_mode == FS3002_F0_CALI_MODE_AUTO)
	{
		// f0 calibrate work mode
		fs3002_haptic_set_mode(fs3002, FS3002_HAPTIC_F0_CALI_MODE);
		fs3002_haptic_cont_get_f0(fs3002);

		fs3002_haptic_set_mode(fs3002, FS3002_HAPTIC_STANDBY_MODE);

		//save f0_cali_data
		fs3002_i2c_read(fs3002, FS3002_FTUNEDS, &reg_val);
		fs3002->f0_cali_data = reg_val;
		
		fs3002_haptic_upload_lra(fs3002, FS3002_F0_CALI);
		pr_info("f0_cali_data=0x%02X\n",fs3002->f0_cali_data);
	}
	else if(fs3002->dts_info.fs3002_f0_cali_mode == FS3002_F0_CALI_MODE_FORMULA)
	{
		// enter DETECT_MODE
		fs3002_haptic_set_mode(fs3002, FS3002_HAPTIC_F0_DETECT_MODE);
	
		fs3002_haptic_cont_get_f0(fs3002);//get f0

		if((fs3002->f0 > fs3002->dts_info.fs3002_f0_max) || (fs3002->f0 < fs3002->dts_info.fs3002_f0_min))
		{
			ui_0x7B = 0x90;
			pr_err("%s: fs3002->f0=%d is out of range(f0_min=%d, f0_max=%d)\n",FSERROR, fs3002->f0, fs3002->dts_info.fs3002_f0_min, fs3002->dts_info.fs3002_f0_max);
		}
		else
		{
			ui_0x7B = fs3002_haptic_cal_0x7B_value(fs3002->f0, fs3002->dts_info.fs3002_f0_ref);
		}
			
		
		//save f0_cali_data
		fs3002->f0_cali_data = ui_0x7B;
		//set new osc
		fs3002_haptic_upload_lra(fs3002, FS3002_F0_CALI);
		//get new f0
		//fs3002_haptic_cont_get_f0(fs3002);
		fs3002_haptic_set_mode(fs3002, FS3002_HAPTIC_STANDBY_MODE);
	}

	fs3002_haptic_stop(fs3002);
	return ret;

}

static enum hrtimer_restart fs3002_vibrator_timer_func(struct hrtimer *timer)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;

	pr_info("%s enter\n", __func__);

	fs3002->state = 0;
	queue_work(fs3002->work_queue, &fs3002->vibrate_work);

	return HRTIMER_NORESTART;
}

#ifdef TIMED_OUTPUT
static int fs3002_vibrator_get_time(struct timed_output_dev *dev)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	pr_info("enter\n");
	if (hrtimer_active(&fs3002->timer)) 
	{
		ktime_t r = hrtimer_get_remaining(&fs3002->timer);
		return ktime_to_ms(r);
	}
	return 0;
}

static void fs3002_vibrator_enable(struct timed_output_dev *dev, int value)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;

	pr_info( "enter\n");
	mutex_lock(&fs3002->lock);
	fs3002_haptic_stop(fs3002);
	if (value > 0) 
	{
		//fs3002_haptic_ram_vbat_compensate(fs3002, false);
		//fs3002_haptic_play_wav_seq(fs3002, value);
	}
	mutex_unlock(&fs3002->lock);
	pr_info("exit\n");
}
#else

//zzzz, 20250722  No longer need to register as an LED device
/*
static enum led_brightness fs3002_haptic_brightness_get(struct led_classdev*cdev)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	pr_info("enter\n");
	return fs3002->amplitude;
}

static void fs3002_haptic_brightness_set(struct led_classdev *cdev, enum led_brightness level)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;

	pr_info("enter\n");

	if (!fs3002->ram_init) 
	{
		pr_err("%s:ram init failed, not allow to play!\n", FSERROR);
		return;
	}
		
	fs3002->amplitude = level;
	mutex_lock(&fs3002->lock);
	fs3002_haptic_stop(fs3002);
	if (fs3002->amplitude > 0) 
	{
		fs3002_haptic_upload_lra(fs3002, FS3002_F0_CALI);
		//fs3002_haptic_ram_vbat_compensate(fs3002, false);
		//fs3002_haptic_play_wav_seq(fs3002, fs3002->amplitude);
	}
	mutex_unlock(&fs3002->lock);
}*/
#endif


//okok
int fs3002_vibrator_init(struct fs3002 *fs3002)
{
	int ret = 0;

	pr_info("enter\n");

/*
	fs3002->vib_dev.name = "vibrator";
	fs3002->vib_dev.brightness_get = fs3002_haptic_brightness_get;
	fs3002->vib_dev.brightness_set = fs3002_haptic_brightness_set;
	
	
	ret = devm_led_classdev_register(&fs3002->i2c->dev, &fs3002->vib_dev);
	if (ret < 0)
	{
		   pr_err("%s:fail to create led dev\n", FSERROR);
		   return ret;
	}
*/	
	ret = sysfs_create_group(&fs3002->i2c->dev.kobj, &fs3002_vibrator_attribute_group);
	//ret = sysfs_create_group(&fs3002->vib_dev.dev->kobj, &fs3002_vibrator_attribute_group);
	if (ret < 0)
	{
		   pr_err("%s:error creating bus sysfs attr files\n", FSERROR);
		   return ret;
	}


/*	ret = sysfs_create_link(&fs3002->vib_dev.dev->kobj,&fs3002->i2c->dev.kobj, "vibrator");
	if (ret < 0)
	{
		pr_err("%s:error creating class sysfs link attr files\n", FSERROR);
		return ret;
	}
*/

#ifdef FS_HAPSTREAM
	fs3002->fs_config_proc = NULL;
	fs3002->fs_config_proc = proc_create(FS_HAPSTREAM_NAME, 0666, NULL, &config_proc_ops);
	if (fs3002->fs_config_proc == NULL)
	   dev_err(fs3002->dev, "create_proc_entry %s failed\n", FS_HAPSTREAM_NAME);
	else
	   dev_info(fs3002->dev, "create proc entry %s success\n", FS_HAPSTREAM_NAME);

	fs3002->start_buf = (struct mmap_buf_format *)__get_free_pages(GFP_KERNEL, HAPSTREAM_MMAP_PAGE_ORDER);
	if (fs3002->start_buf == NULL)
	{
	   pr_info( "Error _get_free_pages failed\n");
	   return -ENOMEM;
	}

	SetPageReserved(virt_to_page(fs3002->start_buf));
	{
	   struct mmap_buf_format *temp;
	   uint32_t i = 0;
	   temp = fs3002->start_buf;
	   for (i = 1; i < HAPSTREAM_MMAP_BUF_SUM; i++)
	   {
		   temp->kernel_next = (fs3002->start_buf + i);
		   temp = temp->kernel_next;
	   }
	   temp->kernel_next = fs3002->start_buf;

	   temp = fs3002->start_buf;
	   for (i = 0; i < HAPSTREAM_MMAP_BUF_SUM; i++)
	   {
		   temp->bit = i;
		   temp->reg_addr = FS3002_RTPWDATA;
		   temp = temp->kernel_next;
	   }
	}
	fs3002->hapstream_stop_flag = true;

	INIT_WORK(&fs3002->rtp_irq_hapstream, fs3002_rtp_irq_work_hapstream);
	INIT_WORK(&fs3002->rtp_hapstream, fs3002_rtp_work_hapstream);
#endif
	fs3002->vib_stop_flag = false;

	hrtimer_init(&fs3002->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	fs3002->timer.function = fs3002_vibrator_timer_func;
	INIT_WORK(&fs3002->vibrate_work, fs3002_vibrate_work_routine);
	INIT_WORK(&fs3002->rtp_work, fs3002_rtp_work_routine);
	mutex_init(&fs3002->lock);
	mutex_init(&fs3002->rtp_lock);
	atomic_set(&fs3002->is_in_rtp_loop, 0);
	atomic_set(&fs3002->exit_in_rtp_loop, 0);
	atomic_set(&fs3002->is_in_write_loop, 0);
	init_waitqueue_head(&fs3002->wait_q);
	init_waitqueue_head(&fs3002->stop_wait_q);
	return 0;
}

void fs3002_reg_init(struct fs3002 *fs3002)
{
	int i = 0;
	unsigned char uc_reg_addr = 0, uc_reg_val =0, uc_start = 0, uc_end = 0;
	//0x927000 fs3002_i2c_write_bits(fs3002, 0x92,00,7,0);
	//0x934003 fs3002_i2c_write_bits(fs3002, 0x93,3,4,0);

	pr_info("enter\n");

	mutex_lock(&fs3002->lock);
	for (i = 0;i < ARRAY_SIZE(fs3002->dts_info.fs3002_reg_inits);i++)
	{
		if(fs3002->dts_info.fs3002_reg_inits[i] != 0xffffff)
		{
			uc_reg_addr = (fs3002->dts_info.fs3002_reg_inits[i] >> 16) & 0xff;
			uc_reg_val = fs3002->dts_info.fs3002_reg_inits[i]  & 0xff;
			uc_end = (fs3002->dts_info.fs3002_reg_inits[i] >> 8) & 0xf;
			uc_start = ((fs3002->dts_info.fs3002_reg_inits[i] >> 12) & 0xf) + uc_end;
			
			fs3002_i2c_write_bits(fs3002, uc_reg_addr,uc_reg_val,uc_start,uc_end);
		}
	}
	mutex_unlock(&fs3002->lock);
}

//okok
static int fs3002_haptic_judge_RTP_is_going_on(struct fs3002 *fs3002)
{
	unsigned char rtp_state = 0;
	unsigned char mode = 0;
	unsigned char glb_st = 0;
	pr_info("enter\n");
	
	fs3002_i2c_read(fs3002, FS3002_SYSCTRL, &mode);
	fs3002_i2c_read(fs3002, FS3002_DIGSTAT, &glb_st);
	if (fs3002->rtp_routine_on || (((mode & 0x3) == FS3002_SYSCTRL_B1_0_OPMODE_RTP) && ((glb_st >> 4) ==  FS3002_DIGSTAT_B7_4_OPS_GO)))
	{
		rtp_state = 1;
	}
	pr_info("rtp_state=%d\n",rtp_state);
	return rtp_state;

}


static void fs3002_haptic_audio_work_routine(struct work_struct *work)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	int rtp_is_going_on = 0;

	pr_info("enter\n");

	mutex_lock(&fs3002->haptic_audio.lock);
	/* rtp mode jump */
	rtp_is_going_on = fs3002_haptic_judge_RTP_is_going_on(fs3002);
	if (rtp_is_going_on) 
	{
		mutex_unlock(&fs3002->haptic_audio.lock);
		return;
	}
	memcpy(&fs3002->haptic_audio.ctr, &fs3002->haptic_audio.data[fs3002->haptic_audio.cnt], sizeof(struct haptic_ctr));
	pr_info("cnt=%d, cmd=%d, play=%d, wavseq=%d, loop=%d, gain=%d\n", fs3002->haptic_audio.cnt, fs3002->haptic_audio.ctr.cmd, fs3002->haptic_audio.ctr.play, fs3002->haptic_audio.ctr.wavseq, fs3002->haptic_audio.ctr.loop, fs3002->haptic_audio.ctr.gain);
	mutex_unlock(&fs3002->haptic_audio.lock);
	if (fs3002->haptic_audio.ctr.cmd == FS3002_HAPTIC_CMD_ENABLE) 
	{
		if (fs3002->haptic_audio.ctr.play == FS3002_HAPTIC_PLAY_ENABLE) 
		{
			pr_info("haptic_audio_play_start\n");
			mutex_lock(&fs3002->lock);
			fs3002_haptic_stop(fs3002);
			fs3002_haptic_set_mode(fs3002, FS3002_HAPTIC_RAM_MODE);

			fs3002_haptic_set_wav_seq(fs3002, 0x00, fs3002->haptic_audio.ctr.wavseq);
			fs3002_haptic_set_wav_seq(fs3002, 0x01, 0x00);

			fs3002_haptic_set_wav_loop(fs3002, 0x00, fs3002->haptic_audio.ctr.loop);
			fs3002_haptic_set_gain(fs3002, fs3002->haptic_audio.ctr.gain);

			fs3002_haptic_play_go(fs3002);
			mutex_unlock(&fs3002->lock);
		} 
		else if (FS3002_HAPTIC_PLAY_STOP == fs3002->haptic_audio.ctr.play) 
		{
			mutex_lock(&fs3002->lock);
			fs3002_haptic_stop(fs3002);
			mutex_unlock(&fs3002->lock);
		} 
		else if (FS3002_HAPTIC_PLAY_GAIN == fs3002->haptic_audio.ctr.play) 
		{
			mutex_lock(&fs3002->lock);
			fs3002_haptic_set_gain(fs3002, fs3002->haptic_audio.ctr.gain);
			mutex_unlock(&fs3002->lock);
		}
		mutex_lock(&fs3002->haptic_audio.lock);
		memset(&fs3002->haptic_audio.data[fs3002->haptic_audio.cnt], 0, sizeof(struct haptic_ctr));
		mutex_unlock(&fs3002->haptic_audio.lock);
	}
	mutex_lock(&fs3002->haptic_audio.lock);
	fs3002->haptic_audio.cnt++;
	if (fs3002->haptic_audio.data[fs3002->haptic_audio.cnt].cmd == 0) 
	{
		fs3002->haptic_audio.cnt = 0;
		pr_debug("haptic play buffer restart\n");
	}
	mutex_unlock(&fs3002->haptic_audio.lock);
}

//okok
static void fs3002_haptic_audio_init(struct fs3002 *fs3002)
{
	pr_info("enter\n");

	fs3002->haptic_audio.delay_val = 1;
	fs3002->haptic_audio.timer_val = 21318;
	hrtimer_init(&fs3002->haptic_audio.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	fs3002->haptic_audio.timer.function = fs3002_haptic_audio_timer_func;
	INIT_WORK(&fs3002->haptic_audio.work, fs3002_haptic_audio_work_routine);
	mutex_init(&fs3002->haptic_audio.lock);
}

//okok
static void fs3002_haptic_set_f0_ref(struct fs3002 *fs3002)
{
	unsigned int i_temp = FS3002_BASE_FRE * 10 / fs3002->dts_info.fs3002_f0_ref;
	pr_info("enter\n");
	fs3002_i2c_write(fs3002, FS3002_F0SET_L, i_temp & 0xFF);
	fs3002_i2c_write(fs3002, FS3002_F0SET_H, (i_temp>>8) & 0xFF);
}

//okok
static int fs3002_haptic_set_pgagain(struct fs3002 *fs3002,unsigned char pgagain)
{
	pr_info("enter pgagain=0x%2x\n", pgagain);
	fs3002_i2c_write_bits(fs3002, FS3002_PGACTRL,pgagain,3,0);
	return 0;
}


//okok
void fs3002_f0_cali_setting_init(struct fs3002 *fs3002)
{
	pr_info("enter\n");

	mutex_lock(&fs3002->lock);

	//	DRV1_lvl
	fs3002_i2c_write(fs3002, FS3002_DRVCFG1,fs3002->dts_info.fs3002_cont_drv1_lvl);
	//	DRV2_lvl
	fs3002_i2c_write(fs3002, FS3002_DRVCFG2,fs3002->dts_info.fs3002_cont_drv2_lvl);
	//  DRV1_TIME
	fs3002_i2c_write(fs3002, FS3002_DRVCFG3,fs3002->dts_info.fs3002_cont_drv1_time);
	//  DRV2_TIME
	fs3002_i2c_write(fs3002, FS3002_DRVCFG4,fs3002->dts_info.fs3002_cont_drv2_time);
	//  cont_1_period
	fs3002_i2c_write(fs3002, FS3002_TRKCFG2, fs3002->dts_info.fs3002_cont_1_period & 0xff);
	fs3002_i2c_write_bits(fs3002, FS3002_TRKCFG3, fs3002->dts_info.fs3002_cont_1_period>>8,4,0);
	//	brk_slopeth
	fs3002_i2c_write(fs3002, FS3002_BRKCFG1, fs3002->dts_info.fs3002_brk_slopeth & 0xff);
	//	brk_gain
	fs3002_i2c_write(fs3002, FS3002_BRKCFG4, fs3002->dts_info.fs3002_brk_gain & 0xff);
	//	brk_times
	fs3002_i2c_write_bits(fs3002, FS3002_BRKCFG5,fs3002->dts_info.fs3002_brk_times,4,0);
	//	brk_noise_gate
	fs3002_i2c_write(fs3002, FS3002_BEMFDCFG4, fs3002->dts_info.fs3002_brk_noise_gate & 0xff);
	//	brk_1_period
	fs3002_i2c_write(fs3002, FS3002_TRKCFG6, fs3002->dts_info.fs3002_brk_1_period & 0xff);
	fs3002_i2c_write_bits(fs3002, FS3002_TRKCFG7, fs3002->dts_info.fs3002_brk_1_period>>8,4,0);
	//	brk_pgagain
	fs3002_haptic_set_pgagain(fs3002, fs3002->dts_info.fs3002_brk_pgagain);
	//	brk_margin
	fs3002_i2c_write(fs3002, FS3002_TRKCFG4, fs3002->dts_info.fs3002_brk_margin & 0xff);
	//	brk_coefp
	fs3002_i2c_write(fs3002, FS3002_BRKCFG2, (fs3002->dts_info.fs3002_brk_coefp<<4) & 0xf0);

	
	//fs3002_i2c_write_bits(fs3002, FS3002_FDETCTRL, fs3002->dts_info.fs3002_average_number,3,0);
	//fs3002_i2c_write(fs3002, FS3002_FDETCFG1, fs3002->dts_info.fs3002_try_number & 0xff);
	//fs3002_i2c_write(fs3002, FS3002_TRKCFG5, fs3002->dts_info.fs3002_apt_step & 0xff);
	//fs3002_i2c_write(fs3002, FS3002_BEMFDCFG14, fs3002->dts_info.fs3002_zc_timeout_f0 & 0xff);
	//fs3002_i2c_write(fs3002, FS3002_BEMFDCFG9, fs3002->dts_info.fs3002_zc_timeout & 0xff);
	//fs3002_i2c_write(fs3002, FS3002_BEMFDCFG10, fs3002->dts_info.fs3002_tra_margin & 0xff);
	mutex_unlock(&fs3002->lock);
}

//okok
static void fs3002_hack_init(struct fs3002 *fs3002)
{
	pr_info("enter\n");

	//if(fs3002->name== FS3002_A1)
	//{
	//	fs3002_haptic_enable_key(fs3002,true);//enable key
	//	fs3002_i2c_write_bits(fs3002, FS3002_ANACTRL,1,6,6);//Just in case of special circumstances, so enable it
	//	fs3002_i2c_write(fs3002, FS3002_OTPPG0W2B4, 0x00);//for A1 short
	//	fs3002_i2c_write_bits(fs3002, FS3002_ANACTRL,0,6,6);
	//	fs3002_haptic_enable_key(fs3002,false);//disable key
	//}
}

//okok
static int fs3002_container_update(struct fs3002 *fs3002, struct fs3002_container *fs3002_cont)
{
	unsigned char reg_val = 0;
	unsigned int shift = 0;
	unsigned int temp = 0;
	
	int ret = 0;
#ifdef FS_CHECK_RAM_DATA
	unsigned short check_sum = 0;
    int i = 0;
#endif

	pr_info("enter\n");
	mutex_lock(&fs3002->lock);
	fs3002->ram.baseaddr_shift = 2;
	fs3002->ram.ram_shift = 4;
	//RAMINIT Enable
	fs3002_haptic_raminit(fs3002, true);

	//Enter standby mode
	fs3002_haptic_stop(fs3002);
	//base addr
	shift = fs3002->ram.baseaddr_shift;
	fs3002->ram.base_addr = (unsigned int)((fs3002_cont->data[0 + shift] << 8) |(fs3002_cont->data[1 + shift]));

	//BASE_ADDRH  WFSBASE_H 
	fs3002_i2c_write_bits(fs3002, FS3002_WFSBASE_H,fs3002_cont->data[0 + shift],3,0);

	//BASE_ADDRL  WFSBASE_L 
	fs3002_i2c_write(fs3002, FS3002_WFSBASE_L,fs3002_cont->data[1 + shift]);

	//FIFO_AEH	1/2 of fifo
	fs3002_i2c_write_bits(fs3002, FS3002_RTPFIFOAE_H,(unsigned char)(((fs3002->ram.base_addr >> 1) >> 8) & 0x0F),3,0);
	//FIFO AEL
	fs3002_i2c_write(fs3002, FS3002_RTPFIFOAE_L,(unsigned char)(((fs3002->ram.base_addr >> 1) & 0xFF)));

	
	// FIFO_AFH  1/4 of fifo
	fs3002_i2c_write_bits(fs3002, FS3002_RTPFIFOAF_H,(unsigned char)(((fs3002->ram.base_addr -(fs3002->ram.base_addr >> 2)) >> 8) & 0x0F),3,0);
	// FIFO_AFL
	fs3002_i2c_write(fs3002, FS3002_RTPFIFOAF_L,(unsigned char)(((fs3002->ram.base_addr -(fs3002->ram.base_addr >> 2)) & 0xFF)));

	fs3002_i2c_read(fs3002, FS3002_RTPFIFOAE_L, &reg_val);
	temp = fs3002_i2c_read_bits(fs3002, FS3002_RTPFIFOAE_H, 3,0);
	temp = (temp<<8) | reg_val;
	pr_info("almost_empty_threshold = %d\n", temp);

	fs3002_i2c_read(fs3002, FS3002_RTPFIFOAF_L, &reg_val);
	temp = fs3002_i2c_read_bits(fs3002, FS3002_RTPFIFOAF_H, 3,0);
	temp = (temp<<8) | reg_val;
	pr_info("almost_full_threshold = %d\n", temp);
	
	//ram
	shift = fs3002->ram.baseaddr_shift;

	//RAMADDR_H
	fs3002_i2c_write_bits(fs3002, FS3002_RAMADDR_H,fs3002_cont->data[0 + shift],3,0);
	//RAMADDR_L
	fs3002_i2c_write(fs3002, FS3002_RAMADDR_L,fs3002_cont->data[1 + shift]);

	shift = fs3002->ram.ram_shift;
	pr_info("ram_len = %d\n",fs3002_cont->len - shift);

	fs3002_i2c_writes(fs3002,FS3002_RAMWDATA,&fs3002_cont->data[shift],fs3002_cont->len - shift);
	//for (i = shift; i < fs3002_cont->len; i++) 
	//{
	//	fs3002_i2c_write(fs3002,FS3002_RAMWDATA,fs3002_cont->data[i]);
	//}
	
#ifdef	FS_CHECK_RAM_DATA
	shift = fs3002->ram.baseaddr_shift;
	//RAMADDR_H
	fs3002_i2c_write_bits(fs3002, FS3002_RAMADDR_H,fs3002_cont->data[0 + shift],3,0);
	//RAMADDR_H
	fs3002_i2c_write(fs3002, FS3002_RAMADDR_L,fs3002_cont->data[1 + shift]);

	shift = fs3002->ram.ram_shift;
	for (i = shift; i < fs3002_cont->len; i++) 
	{
		fs3002_i2c_read(fs3002, FS3002_RAMRDATA, &reg_val);
		//for debug
		//pr_info("fs3002_cont->data=0x%02X, ramdata=0x%02X\n",fs3002_cont->data[i],reg_val);
		
		if (reg_val != fs3002_cont->data[i]) 
		{
			pr_err("%s:ram check error addr=0x%04x, file_data=0x%02X, ram_data=0x%02X\n", FSERROR, i, fs3002_cont->data[i], reg_val);
			ret = -1;
			break;
		}
		check_sum += reg_val;
	}
	if (!ret) 
	{
		fs3002_i2c_read(fs3002, FS3002_WFSBASE_L, &reg_val);
		check_sum += reg_val;
		fs3002_i2c_read(fs3002, FS3002_WFSBASE_H, &reg_val);
		check_sum += reg_val & 0x0f;

		if (check_sum != fs3002->ram.check_sum) 
		{
			pr_err("%s:ram data check sum error, check_sum=0x%04x\n", FSERROR, check_sum);
			ret = -1;
		} 
		else 
		{
			pr_info("ram data check sum pass, check_sum=0x%04x\n", check_sum);
		}
	}

#endif
	//RAMINIT Disable
	fs3002_haptic_raminit(fs3002, false);
	mutex_unlock(&fs3002->lock);
	pr_info("exit\n");
	return ret;
}


//okok
static void fs3002_ram_loaded(const struct firmware *cont, void *context)
{
	struct fs3002 *fs3002 = context;
	struct fs3002_container *fs3002_fw;
	int i = 0, j = 0;
	int ret = 0;
	unsigned short check_sum = 0;
	uint32_t wave_start[FS3002_RAM_NUM_MAX] = {0}, wave_end[FS3002_RAM_NUM_MAX] = {0};
	uint16_t ui_count = 0;

	pr_info("enter\n");
	if (!cont) 
	{
		pr_err("%s:failed to read %s\n", FSERROR, fs3002_ram_name[0]);
		release_firmware(cont);
		return;
	}
	pr_info("loaded %s - size: %zu bytes\n",fs3002_ram_name[0], cont ? cont->size : 0);

//for debug	xxxx, print bin file content
//	for(i=0; i < cont->size; i++) 
//	{
//		pr_info("addr: 0x%04x, data: 0x%02X\n",i, *(cont->data+i));
//	}

	// check sum
	for (i = 2; i < cont->size; i++)
	{
		check_sum += cont->data[i];
	}

	if (check_sum != (unsigned short)((cont->data[0] << 8) | (cont->data[1]))) 
	{
		pr_err("%s:check sum err: check_sum=0x%04x\n", FSERROR,check_sum);
		return;
	} 
	else 
	{
		pr_info("check sum pass: 0x%04x\n", check_sum);
		fs3002->ram.check_sum = check_sum;
	}

	//fs3002 ram update less then 128kB
	fs3002_fw = kzalloc(cont->size + sizeof(int), GFP_KERNEL);
	if (!fs3002_fw) 
	{
		release_firmware(cont);
		pr_err("%s:error allocating memory\n", FSERROR);
		return;
	}
	fs3002_fw->len = cont->size;
	memcpy(fs3002_fw->data, cont->data, cont->size);
	release_firmware(cont);
	
	ret = fs3002_container_update(fs3002, fs3002_fw);
	if (ret) 
	{
		kfree(fs3002_fw);
		fs3002->ram.len = 0;
		pr_err("%s:ram firmware update failed!\n", FSERROR);
	} 
	else 
	{
		fs3002->ram_init = 1;
		fs3002->ram.len = fs3002_fw->len - fs3002->ram.ram_shift;

		//fs3002_haptic_get_ram_num_and_max(fs3002);

		//get wave num
		fs3002->ram.ram_num = fs3002_get_bin_ram_num(fs3002_fw->data);
		//get data_start_addr
		ui_count = (uint16_t)(((fs3002_fw->data[5] << 8) | (fs3002_fw->data[6])) - ((fs3002_fw->data[2] << 8) | (fs3002_fw->data[3])));
		pr_info("ram_version = 0x%02x, wave_num = %d, data_start_addr = %d\n", fs3002_fw->data[4],fs3002->ram.ram_num, ui_count);
		//clear ram.ram_max
		for (i = 0; i < FS3002_RAM_NUM_MAX; i++) 
		{
			fs3002->ram.ram_max[i] = 0;
		}
			
		if(fs3002->ram.ram_num > FS3002_RAM_NUM_MAX)
		{
			pr_err("%s: ram number is over %d\n", FSERROR, FS3002_RAM_NUM_MAX);
			fs3002->ram.b_over_max_num = true;
		}
		else
		{
			for( i = 0; i < fs3002->ram.ram_num; i++)
			{	
				wave_start[i] = (fs3002_fw->data[5 + i * 4] << 8 | fs3002_fw->data[6 + i * 4]);
				
				wave_end[i] = (fs3002_fw->data[7 + i * 4] << 8 | fs3002_fw->data[8 + i * 4]);
				for(j = 0; j < wave_end[i] - wave_start[i]; j++)
				{
					if(abs((int8_t)fs3002_fw->data[ui_count]) > fs3002->ram.ram_max[i])
					{
						fs3002->ram.ram_max[i] = abs((int8_t)fs3002_fw->data[ui_count]);
						
					}
					ui_count++;
				}
				pr_info("wave_num = %d, wave_max = %d\n", i, fs3002->ram.ram_max[i]);
			}
		}
	
		fs3002_trig_config(fs3002);
		kfree(fs3002_fw);
		pr_info("ram firmware update complete!\n");
	}
}

static uint8_t fs3002_get_bin_ram_num(const uint8_t* buffer)
{
	uint32_t first_wave_addr = 0;
	uint32_t fs_base_addr = 0;
	uint8_t wave_num = 0;
	fs_base_addr = (buffer[2] << 8) | buffer[3];
	first_wave_addr = (buffer[5] << 8) | buffer[6];
	wave_num = (first_wave_addr - fs_base_addr - 1)/4;
	pr_info("wave_num = %d\n", wave_num);
	return wave_num;
}

//okok
static int fs3002_ram_update(struct fs3002 *fs3002)
{
	pr_info("enter\n");

	fs3002->ram_init = 0;
	fs3002->rtp_init = 0;
	return request_firmware_nowait(THIS_MODULE, FW_ACTION_UEVENT, fs3002_ram_name[0], fs3002->dev, GFP_KERNEL, fs3002, fs3002_ram_loaded);
}

//okok
static int fs3002_haptic_effect_strength(struct fs3002 *fs3002)
{
#if 0
	switch (fs3002->play.vmax_mv) {
	case FS3002_LIGHT_MAGNITUDE:
		fs3002->level = 0x80;
		break;
	case FS3002_MEDIUM_MAGNITUDE:
		fs3002->level = 0x50;
		break;
	case FS3002_STRONG_MAGNITUDE:
		fs3002->level = 0x30;
		break;
	default:
		break;
	}
#else
	if (fs3002->play.vmax_mv >= 0x7FFF)
		fs3002->level = 0x80; /*128*/
	else if (fs3002->play.vmax_mv <= 0x3FFF)
		fs3002->level = 0x1E; /*30*/
	else
		fs3002->level = (fs3002->play.vmax_mv - 16383) / 128;
	
	if (fs3002->level < 0x1E)
		fs3002->level = 0x1E; /*30*/
#endif

	pr_info("enter, fs3002->play.vmax_mv =0x%x, fs3002->level =0x%x\n", fs3002->play.vmax_mv, fs3002->level);
	return 0;
}

//okok
static void fs3002_haptic_diagnostic_sequence(struct fs3002 *fs3002)
{
	unsigned char uc_0xBC = 0;
	unsigned char uc_0xBD = 0;
 	unsigned int ui_BCBD = 0;
	
	pr_info( "enter\n");
	fs3002_haptic_enable_key(fs3002,true);//enable key
	fs3002_i2c_write(fs3002, FS3002_ANACTRL, 0x40);//0xC0 = 0x40		// Enable OSC
	fs3002_i2c_write(fs3002, FS3002_SWDIAG1, 0xC0);//0xB5 = 0xC0		// Enable Software diagnose mode and VCM buffer
	if(fs3002->lra_test_sleep_time != 0)
	{
		//Wait > 5 us
		usleep_range(fs3002->lra_test_sleep_time, fs3002->lra_test_sleep_time+10);
	} 
	fs3002_i2c_write(fs3002, FS3002_SWDIAG1, 0xD2);//0xB5 = 0xD2		// Turn on S1FN & S1FP & PGA
	if(fs3002->lra_test_sleep_time != 0)
	{
		//Wait > 100 us
		usleep_range(fs3002->lra_test_sleep_time, fs3002->lra_test_sleep_time+10);
	}
	
	fs3002_i2c_write(fs3002, FS3002_SARCTRL, 0x82);// Enable SAR test mode, SAR ADC input =0.2*OUTP
	fs3002_i2c_write(fs3002, FS3002_SARCTRL, 0x86);// SAR capture data
	fs3002_i2c_write(fs3002, FS3002_SARCTRL, 0x82);
	if(fs3002->lra_test_sleep_time != 0)
	{
		//Wait > 10 us
		usleep_range(fs3002->lra_test_sleep_time, fs3002->lra_test_sleep_time+10);
	}
	//Read SAR data from {0xBD<1:0>, 0xBC<7:0>}, denoted as SAR_OUTP_READING. Check if SAR_OUTP_READING is [[100, 350] (TBD, 0.58V~2.05V). 
	fs3002_i2c_read(fs3002, FS3002_SARTS_L, &uc_0xBC);//0xBC//low 8 offset value
	fs3002_i2c_read(fs3002, FS3002_SARTS_L, &uc_0xBC);//0xBC//low 8 offset value
	uc_0xBD = fs3002_i2c_read_bits(fs3002, FS3002_SARTS_H, 1,0);//0xBD//high 2 offset value
	ui_BCBD = (uc_0xBD<<8 | uc_0xBC) + ui_BCBD;
	if(ui_BCBD >= 100 && ui_BCBD<=350)
	{
		pr_info("SAR_OUTP is 0x%4x\n", ui_BCBD);
	}
	else
	{
		pr_err("%s:SAR_OUTP is out of range: 0x%4x\n",FSERROR, ui_BCBD);
	}

	ui_BCBD = 0;
	fs3002_i2c_write(fs3002, FS3002_SARCTRL, 0x83);
	fs3002_i2c_write(fs3002, FS3002_SARCTRL, 0x87);// SAR capture data
	fs3002_i2c_write(fs3002, FS3002_SARCTRL, 0x83);
	if(fs3002->lra_test_sleep_time != 0)
	{
		//Wait > 10 us
		usleep_range(fs3002->lra_test_sleep_time, fs3002->lra_test_sleep_time+10);
	}
	//Read SAR data from {0xBD<1:0>, 0xBC<7:0>}
	fs3002_i2c_read(fs3002, FS3002_SARTS_L, &uc_0xBC);//0xBC//low 8 offset value
	fs3002_i2c_read(fs3002, FS3002_SARTS_L, &uc_0xBC);//0xBC//low 8 offset value
	uc_0xBD = fs3002_i2c_read_bits(fs3002, FS3002_SARTS_H, 1,0);//0xBD//high 2 offset value
	ui_BCBD = (uc_0xBD<<8 | uc_0xBC) + ui_BCBD;
	if(ui_BCBD >= 100 && ui_BCBD<=350)
	{
		pr_info("SAR_OUTN is 0x%4x\n", ui_BCBD);
	}
	else
	{
		pr_err("%s:SAR_OUTN is out of range: 0x%4x\n",FSERROR, ui_BCBD);
	}
}

//okok
static void fs3002_haptic_set_diagnostic_reg_to_default_value(struct fs3002 *fs3002)
{
	pr_info( "enter\n");
	//reset to default value //0xB5, 0xBB, 0x2A(15), 0xC0, 0xA9(80)
	fs3002_i2c_write(fs3002, FS3002_SWDIAG1, 0x00);//0xB5 = 0x00
	fs3002_i2c_write(fs3002, FS3002_SARCTRL, 0x00);//0xBB = 0x00
	fs3002_i2c_write(fs3002, FS3002_ANACTRL, 0x00);//0xC0 = 0x00
	fs3002_haptic_enable_key(fs3002,false);//disable key
}



//okok
//offset calibration
static int fs3002_haptic_offset_calibration(struct fs3002 *fs3002)
{
	unsigned char uc_0xBC = 0;
	unsigned char uc_0xBD = 0;
 	unsigned int ui_BCBD = 0;
	int i = 0;

	pr_info( "enter\n");
	fs3002_haptic_diagnostic_sequence(fs3002);

	//outp
	fs3002_i2c_write(fs3002, FS3002_SARCTRL, 0x86);//0xBB = 0x86		// SAR capture data
	usleep_range(1000, 2000);//usleep_range(10, 20); //Wait > 5 us
	fs3002_i2c_read(fs3002, FS3002_SARTS_L, &uc_0xBC);//0xBC//low 8 offset value
	fs3002_i2c_read(fs3002, FS3002_SARTS_L, &uc_0xBC);//0xBC//low 8 offset value
	uc_0xBD = fs3002_i2c_read_bits(fs3002, FS3002_SARTS_H, 1,0);//0xBD//high 2 offset value
	ui_BCBD = uc_0xBD<<8 | uc_0xBC;
	if (100 <= ui_BCBD && ui_BCBD <= 350)            //Check if SAR_OUTN_READING is [100, 350] (TBD, 0.58V ~2.05V). 
	{
		pr_info( "outp = %d\n",ui_BCBD);
	}
	else
	{
		pr_err( "%s,outp is out of range =  %d\n",FSERROR,ui_BCBD);
		fs3002_haptic_set_diagnostic_reg_to_default_value(fs3002);
		return ui_BCBD;
	}

	//outn
	fs3002_i2c_write(fs3002, FS3002_SARCTRL, 0x87);//0xBB = 0x87		// SAR capture data
	usleep_range(1000, 2000);//usleep_range(10, 20); //Wait > 5 us
	fs3002_i2c_read(fs3002, FS3002_SARTS_L, &uc_0xBC);//0xBC//low 8 offset value
	fs3002_i2c_read(fs3002, FS3002_SARTS_L, &uc_0xBC);//0xBC//low 8 offset value
	uc_0xBD = fs3002_i2c_read_bits(fs3002, FS3002_SARTS_H, 1,0);//0xBD//high 2 offset value
	ui_BCBD = uc_0xBD<<8 | uc_0xBC;
	if (100 <= ui_BCBD && ui_BCBD <= 350)            //Check if SAR_OUTN_READING is [100, 350] (TBD, 0.58V ~2.05V). 
	{
		pr_info("outn = %d\n",ui_BCBD);
	}
	else
	{
		pr_err("%s,outn is out of range =  %d\n",FSERROR,ui_BCBD);
		fs3002_haptic_set_diagnostic_reg_to_default_value(fs3002);
		return ui_BCBD;
	}

	//offset
	fs3002_i2c_write(fs3002, FS3002_SWDIAG1, 0xDE);//0xB5 = 0xDE		// Turn on S2F
	usleep_range(1000, 2000);//usleep_range(10, 20); //Wait > 5 us
	ui_BCBD = 0;
	for(i=0;i<FS3002_OFFSET_RETRIES;i++)
	{
		fs3002_i2c_write(fs3002, FS3002_SARCTRL, 0x84);//0xBB = 0x84		// SAR capture data
		usleep_range(1000, 2000);//usleep_range(10, 20); //Wait > 5 us
		fs3002_i2c_read(fs3002, FS3002_SARTS_L, &uc_0xBC);//0xBC//low 8 offset value
		fs3002_i2c_read(fs3002, FS3002_SARTS_L, &uc_0xBC);//0xBC//low 8 offset value
		uc_0xBD = fs3002_i2c_read_bits(fs3002, FS3002_SARTS_H, 1,0);//0xBD//high 2 offset value
		ui_BCBD = (uc_0xBD<<8 | uc_0xBC) + ui_BCBD;
	}
	ui_BCBD = ui_BCBD/FS3002_OFFSET_RETRIES;
	if(ui_BCBD>(512+6) || ui_BCBD<(512-6))
	{
		pr_err("%s:offset = %d\n",FSERROR,ui_BCBD);
	}
	else
	{
		fs3002->offset = ui_BCBD;
		pr_info("offset = %d\n",fs3002->offset);
		//Write back SAR_OS_READING to {0x83<1:0>, 0x82<7:0>}
		fs3002_i2c_write(fs3002, FS3002_BEMFDCFG2, (ui_BCBD & 0xff));
		fs3002_i2c_write(fs3002,FS3002_BEMFDCFG3,(ui_BCBD >> 8));
	}

	fs3002_haptic_set_diagnostic_reg_to_default_value(fs3002);

	return 0;
}

static void fs3002_ram_work_routine(struct work_struct *work)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;

	pr_info("enter\n");

	fs3002_ram_update(fs3002);

}

int fs3002_ram_init(struct fs3002 *fs3002)
{
	int ram_timer_val = 8000;

	pr_info("enter\n");
	INIT_DELAYED_WORK(&fs3002->ram_work, fs3002_ram_work_routine);
	schedule_delayed_work(&fs3002->ram_work,msecs_to_jiffies(ram_timer_val));
	return 0;
}

static int fs3002_haptic_play_repeat_seq(struct fs3002 *fs3002,unsigned char flag)
{
	pr_info("enter\n");

	if (flag) 
	{
		fs3002_haptic_set_mode(fs3002, FS3002_HAPTIC_RAM_LOOP_MODE);
		fs3002_haptic_play_go(fs3002);
	}
	return 0;
}


static int fs3002_haptic_activate(struct fs3002 *fs3002)
{
	sprintf(str, "%s, enter\n", __func__);
	fs3002_debug_message(fs3002, str);
	
	fs3002_interrupt_clear(fs3002);
	//xxxx_?? open UV INT
	fs3002_i2c_write_bits(fs3002, FS3002_INTMASK1,0,6,6);
	return 0;
}

static int fs3002_haptic_start(struct fs3002 *fs3002)
{
	sprintf(str, "%s, enter\n", __func__);
	fs3002_debug_message(fs3002, str);
	
	fs3002_haptic_activate(fs3002);
	fs3002_haptic_play_go(fs3002);
	return 0;
}

static void fs3002_haptic_set_ram_env(struct fs3002 *fs3002, unsigned char id)
{
	unsigned char env = 0, highv = 0, ampv = 0;
	
	sprintf(str, "%s, enter,id=%d,fs3002->ram.ram_max[id-1]=%d\n", __func__,id,fs3002->ram.ram_max[id-1]);
	fs3002_debug_message(fs3002, str);
	if(fs3002->ram.b_over_max_num == true)
	{
		fs3002_i2c_write_bits(fs3002, FS3002_BSTENV4,2,1,0);
	}
	else
	{	
		if(id < FS3002_RAM_NUM_MAX)
		{
			ampv = fs3002->ram.ram_max[id-1];
			highv = fs3002_get_highv(fs3002);
			env = fs3002_get_env(fs3002, ampv, highv);
			fs3002_i2c_write_bits(fs3002, FS3002_BSTENV4,env,1,0);
		}
	}
}

static int fs3002_haptic_play_effect_seq(struct fs3002 *fs3002, unsigned char flag)
{
	sprintf(str, "%s, enter flag = %d,fs3002->effect_id =%d,fs3002->dts_info.fs3002_ram_id_boundary = %d,fs3002->activate_mode =%d,fs3002->dts_info.fs3002_bypass_system_gain=%d\n", __func__,flag,fs3002->effect_id,fs3002->dts_info.fs3002_ram_id_boundary,fs3002->activate_mode,fs3002->dts_info.fs3002_bypass_system_gain);
	fs3002_debug_message(fs3002, str);
	
	if (fs3002->effect_id > fs3002->dts_info.fs3002_ram_id_boundary)
	{
		return 0;
	}
	
	if (flag) 
	{
		if (fs3002->activate_mode == FS3002_HAPTIC_ACTIVATE_RAM_MODE) 
		{
			fs3002_haptic_set_ram_env(fs3002, fs3002->effect_id + 1);
			fs3002_haptic_set_wav_seq(fs3002, 0x00,(char)fs3002->effect_id + 1);//set seq0
			fs3002_haptic_set_pwm(fs3002, fs3002->dts_info.fs3002_play_ram_srate);//20210601
			fs3002_haptic_set_wav_seq(fs3002, 0x01, 0x00);//set seq1 stop
			fs3002_haptic_set_wav_loop(fs3002, 0x00, 0x00);
			fs3002_haptic_set_mode(fs3002, FS3002_HAPTIC_RAM_MODE);
			if(fs3002->dts_info.fs3002_bypass_system_gain == 0)
			{
				fs3002_haptic_effect_strength(fs3002);
				fs3002_haptic_set_gain(fs3002, fs3002->level);
			}
			fs3002_haptic_start(fs3002);
		}
		if (fs3002->activate_mode == FS3002_HAPTIC_ACTIVATE_RAM_LOOP_MODE) 
		{
			//zzzz if set in fs3002_duration_store->fs3002_haptic_duration_ram_play_config, need to remove the function "fs3002_haptic_set_repeat_wav_seq"
			fs3002_haptic_set_repeat_wav_seq(fs3002,(fs3002->dts_info.fs3002_ram_id_boundary +1));
			fs3002_haptic_set_pwm(fs3002,  fs3002->dts_info.fs3002_play_ram_srate);//20210601
			if(fs3002->dts_info.fs3002_bypass_system_gain == 0)
			{
				fs3002_haptic_set_gain(fs3002, fs3002->level);
			}
			//there is fs3002_haptic_set_mode(fs3002, FS3002_HAPTIC_RAM_LOOP_MODE) in fs3002_haptic_play_repeat_seq function
			//lowv is 4.1v, so needn't call fs3002_haptic_set_ram_env
			fs3002_haptic_play_repeat_seq(fs3002, true);
		}
	}
	
	sprintf(str, "%s, exit\n", __func__);
	fs3002_debug_message(fs3002, str);
	return 0;
}

int fs3002_haptics_erase(struct input_dev *dev, int effect_id)
{
	struct fs3002 *fs3002 = input_get_drvdata(dev);
	int rc = 0;

	/*for osc calibration*/
	if (fs3002->osc_cali_run != 0)
		return 0;

	pr_debug("%s: enter\n", __func__);
	fs3002->effect_type = 0;
	fs3002->is_custom_wave = 0;
	fs3002->duration = 0;
	return rc;
}

/* zzzz
static void fs3002_haptic_duration_ram_play_config(struct fs3002 *fs3002,int duration)
{
	unsigned char seq = 0;
	unsigned char loop = 0;
	
	pr_info( "enter %d\n",duration);

	if(duration == 200)
	{
		duration = 100;//wrong
	}
	
	if(duration == 12)
	{
		duration = 0;//ring
	}

	if(duration == 65 || duration == 95)
	{
		seq = 1;//short-strong
		loop = 0;
	}
	else if((duration < fs3002->dts_info.fs3002_duration_time[0]) && duration != 12)
	{
		seq = 3;//0-30 short-weak
		loop = 0;
	}
	else if((duration >= fs3002->dts_info.fs3002_duration_time[0]) && (duration < fs3002->dts_info.fs3002_duration_time[1]))
	{
		seq = 1;//30-60 short-strong
		loop = 0;
	}
	else if((duration >= fs3002->dts_info.fs3002_duration_time[1]) && (duration < fs3002->dts_info.fs3002_duration_time[2]) && duration != 65)
	{
		seq = 5;//60-90 long-weak
		loop = 15;
	}
	else if((duration >= fs3002->dts_info.fs3002_duration_time[2]) && duration != 95)
	{
		seq = 4;//over 90 long-strong
		loop = 15;
	}
	else
	{
		seq = 0;
		loop = 0;
	}

	fs3002_haptic_set_wav_seq(fs3002,0,seq);
	fs3002_haptic_set_wav_loop(fs3002,0,loop);
	fs3002_haptic_set_wav_seq(fs3002,1,0);
	fs3002_haptic_set_wav_loop(fs3002,1,0);
}
*/




/******************************************************
 *
 * sysfs attr
 *
 ******************************************************/

//okok
// show lra f0, make a cont go to get f0 and cont f0, but only return f0(cont f0 not returned)
// Why set FTUNECFG (7Bh) to 0x90? Because the OSC corresponding to 0x90 is trimmed and accurate
//vibrate
// cat f0
// 1723
static ssize_t fs3002_f0_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;

	pr_info("enter");

	mutex_lock(&fs3002->lock);

	fs3002_haptic_upload_lra(fs3002, FS3002_WRITE_ZERO);
	// f0 detect mode
	fs3002_haptic_set_mode(fs3002, FS3002_HAPTIC_F0_DETECT_MODE);
	fs3002_haptic_cont_get_f0(fs3002);
	fs3002_haptic_set_mode(fs3002, FS3002_HAPTIC_STANDBY_MODE);
	
	mutex_unlock(&fs3002->lock);
	len += snprintf(buf + len, PAGE_SIZE - len,"%d\n", fs3002->f0);
	return len;
}

//show f0_ref
//cat f0_ref
//f0_ref = 2350
static ssize_t fs3002_f0_ref_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	pr_info("enter\n");

	len += snprintf(buf + len, PAGE_SIZE - len,"f0_ref = %d\n",fs3002->dts_info.fs3002_f0_ref);
	return len;

}

//set f0_ref[generally, after this step, we need to call "echo 1 > cali" to calibrate the motor again]
//cat f0_ref
//f0_ref = 2350
//echo 2222 > f0_ref		[hex is ok:echo 0x8AE > f0_ref ]
//f0_ref = 2222
static ssize_t fs3002_f0_ref_store(struct device *dev,struct device_attribute *attr, const char *buf,size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int val = 0;
	int rc = 0;
	pr_info("enter\n");

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	
	mutex_lock(&fs3002->lock);
	fs3002->dts_info.fs3002_f0_ref = val;
	mutex_unlock(&fs3002->lock);

	return count;
}

//okok
//cat f0_check
//return fs3002->f0_cali_status (only show f0_cali_status, no vibration)
static ssize_t fs3002_f0_check_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;

	if (fs3002->f0_cali_status == true)
		len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", 1);
	if (fs3002->f0_cali_status == false)
		len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", 0);

	return len;
}

//okok
//cat f0_value
//return fs3002->f0 (only show f0, no vibration)
static ssize_t fs3002_f0_value_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	pr_info("enter\n");
	return snprintf(buf, PAGE_SIZE, "%d\n", fs3002->f0);
}


//okok
//Show fs3002->f0_cali_data
//cat f0_save
//f0_cali_data = 0x83
static ssize_t fs3002_f0_save_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	pr_info("enter\n");

	len += snprintf(buf + len, PAGE_SIZE - len, "f0_cali_data = 0x%02X\n",fs3002->f0_cali_data);
	return len;
}

//okok
//set fs3002->f0_cali_data
//cat f0_save
//f0_cali_data = 0x83
//echo 144>f0_save		[hex is ok: echo 0x90 > f0_save		]
//cat f0_save
//f0_cali_data = 0x90
static ssize_t fs3002_f0_save_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int val = 0;
	int rc = 0;
	pr_info("enter\n");

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	fs3002->f0_cali_data = val;
	fs3002_haptic_upload_lra(fs3002, FS3002_F0_CALI);
	return count;
}

//okok
//show fs3002->f0 with f0_cali_data osc
//In this case, F0 is detected again with the new OSC after Cali, and the result obtained at this time should be F0_ref
//cat cali
//2350
static ssize_t fs3002_cali_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	pr_info("enter\n");

	mutex_lock(&fs3002->lock);
	
	fs3002_haptic_upload_lra(fs3002, FS3002_F0_CALI);
	// f0 detect mode
	fs3002_haptic_set_mode(fs3002, FS3002_HAPTIC_F0_DETECT_MODE);
	fs3002_haptic_cont_get_f0(fs3002);
	fs3002_haptic_set_mode(fs3002, FS3002_HAPTIC_STANDBY_MODE);
	mutex_unlock(&fs3002->lock);
	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", fs3002->f0);
	return len;
}

//okok
//Perform F0 calibration
//echo 1 > cali   [hex is ok]
static ssize_t fs3002_cali_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int val = 0;
	int rc = 0;
	pr_info("enter\n");

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val) 
	{
		mutex_lock(&fs3002->lock);
		fs3002_haptic_f0_calibration(fs3002);
		mutex_unlock(&fs3002->lock);
	}
	return count;
}


//okok,same as "cat osc_cali", show fs3002->osc_cali_data
static ssize_t fs3002_osc_save_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;

	pr_info("enter\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "0x%2x\n", fs3002->osc_cali_data);

	return len;
}

//okok
//set fs3002->osc_cali_data
static ssize_t fs3002_osc_save_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int val = 0;
	int rc = 0;
	pr_info("enter\n");

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
	{
		return rc;
	}
		
	fs3002->osc_cali_data = val;
	pr_info("load osa cali data: %d\n", val);
	return count;
}

//okok,same as "cat osc_save"
static ssize_t fs3002_osc_cali_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;

	pr_info("enter\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "0x%2x\n", fs3002->osc_cali_data);

	return len;
}

//okok
//play a 10s rtp file, then get fs3002->microsecond
//compare fs3002->microsecond and theory_time, get fs3002->osc_cali_data and set this value to osc
//cat osc_cali
//osc_cali_data = 0x90
//echo 1 > osc_cali
//cat osc_cali
//osc_cali_data = 0x90
static ssize_t fs3002_osc_cali_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int val = 0;
	int rc = 0;
	pr_info("enter\n");

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	mutex_lock(&fs3002->lock);
	fs3002->osc_cali_run = 1;
	if (val == 3) 
	{
		//set osc to default value(FS3002_FTUNECFG=0x90,osc to 384000)
		fs3002_haptic_upload_lra(fs3002, FS3002_WRITE_ZERO);
		//play a 10s rtp file, then get fs3002->microsecond
		fs3002_rtp_osc_calibration(fs3002);
		//compare fs3002->microsecond and theory_time, get fs3002->osc_cali_data and set this value to osc
		fs3002_rtp_trim_lra_calibration(fs3002);
	}
	//set osc to osc_cali_data, then play a 10s rtp file, get fs3002->microsecond
	//just for observing the debug message
	else if (val == 1) 
	{
		fs3002_haptic_upload_lra(fs3002, FS3002_OSC_CALI);
		fs3002_rtp_osc_calibration(fs3002);
	}
	else 
	{
		pr_err("%s:input value out of range\n", FSERROR);
	}
	fs3002->osc_cali_run = 0;
	//osc calibration flag end, other behaviors are permitted
	mutex_unlock(&fs3002->lock);

	return count;
}


//okok
//show fs3002->dts_info.fs3002_reg_inits contents
//cat reg_inits
//0x927000
//0x934003
//0x5f7080
//0xffffff
//0xffffff
//0xffffff
//0xffffff
//0xffffff
//0xffffff
//0xffffff

//0x5f0001			write 0x01 to 0x5f, start bit=0, len=1(0x1->bit0)
//0x5f3403			write 0x03 to 0x5f, start bit=4, len=4(0x3->bit7-4)
//0x5f7080			write 0x80 to 0x5f, start bit=0, len=8(0x80->bit7-0)
static ssize_t fs3002_reg_inits_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	int i = 0;
	pr_info("enter\n");
	for (i = 0;i < ARRAY_SIZE(fs3002->dts_info.fs3002_reg_inits);i++)
	{
		len += snprintf(buf + len, PAGE_SIZE - len, "0x%x\n", fs3002->dts_info.fs3002_reg_inits[i]);
	}

	return len;
}

//okok
// cat reg
// Display all registers
static ssize_t fs3002_reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	pr_info("enter");
	fs3002_haptic_enable_key(fs3002,true);
	for (i = 0; i <= FS3002_REG_MAX; i++) 
	{
		fs3002_i2c_read(fs3002, i, &reg_val);
		if((i+1) % 8 == 0)
		{
			len += snprintf(buf + len, PAGE_SIZE - len,"0x%02X=0x%02X\n", i, reg_val);
		}
		else
		{
			len += snprintf(buf + len, PAGE_SIZE - len,"0x%02X=0x%02X ", i, reg_val);
		}
	}
	fs3002_haptic_enable_key(fs3002,false);
	pr_info("PAGE_SIZE=%d len=%d\n",(int)PAGE_SIZE,(int)len);
	return len;
}

//okok
//set a register
//echo 9 2 > register  (set 0x2 to address 0x9)
static ssize_t fs3002_reg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int databuf[2] = { 0, 0 };

	pr_info("enter");
	fs3002_haptic_enable_key(fs3002,true);

	if(sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2)
	{
		fs3002_i2c_write(fs3002, (unsigned char)databuf[0],(unsigned char)databuf[1]);
	}
	fs3002_haptic_enable_key(fs3002,false);

	return count;

}

//okok
//set multi registers
//echo 1110 4001 4100 4900 1201 ffff ffff ffff ffff ffff > registers (ram wave one vibrates one time )
//reg count must be 10, ffff is dull commnd
static ssize_t fs3002_regs_store(struct device *dev,struct device_attribute *attr, const char *buf,size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int databuf[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	int i = 0, ret_sscanf = 0;
	
	pr_info("enter");

	ret_sscanf = sscanf(buf, "%x %x %x %x %x %x %x %x %x %x", &databuf[0], &databuf[1], &databuf[2], &databuf[3], &databuf[4], &databuf[5], &databuf[6], &databuf[7], &databuf[8], &databuf[9]);

	if(ret_sscanf > 0 && ret_sscanf <=10)
	{
		for (i = 0;i < ret_sscanf;i++)
		{
			fs3002_i2c_write(fs3002, (unsigned char)((databuf[i]>>8) & 0xff),(unsigned char)(databuf[i] & 0xff));
		}
	}
	else
	{
		pr_info("count should be > 0 and <=10");
	}

	return count;
}


//okok
//show fs3002_cont_drv1_lvl,fs3002_cont_drv2_lvl
//cat cont_drv_lvl
//cont_drv1_lvl = 0x7F
//cont_drv2_lvl = 0x36
static ssize_t fs3002_cont_drv_lvl_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	pr_info("enter\n");

	len += snprintf(buf + len, PAGE_SIZE - len, "cont_drv1_lvl = 0x%02X, cont_drv2_lvl = 0x%02X\n", fs3002->dts_info.fs3002_cont_drv1_lvl, fs3002->dts_info.fs3002_cont_drv2_lvl);
	return len;
}

//okok
//set fs3002_cont_drv1_lvl,fs3002_cont_drv2_lvl
//cat cont_drv_lvl
//cont_drv1_lvl = 0x7F
//cont_drv2_lvl = 0x36
//echo 0x7f 044 > cont_drv_lvl
//cat cont_drv_lvl
//cont_drv1_lvl = 0x7F
//cont_drv2_lvl = 0x44
static ssize_t fs3002_cont_drv_lvl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int databuf[2] = { 0, 0 };
	pr_info("enter\n");

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) 
	{
		fs3002->dts_info.fs3002_cont_drv1_lvl = databuf[0];
		fs3002->dts_info.fs3002_cont_drv2_lvl = databuf[1];
		fs3002_i2c_write(fs3002, FS3002_DRVCFG1,fs3002->dts_info.fs3002_cont_drv1_lvl);
		fs3002_i2c_write(fs3002, FS3002_DRVCFG2,fs3002->dts_info.fs3002_cont_drv2_lvl);
	}
	else
	{
		pr_info("Wrong parameter\n");
	}	
	return count;
}

//okok
//show cont_drv1_time,cont_drv2_time
//cat cont_drv_time
//cont_drv1_lvl = 0x04
//cont_drv2_time = 0x14
static ssize_t fs3002_cont_drv_time_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	pr_info("enter\n");

	len += snprintf(buf + len, PAGE_SIZE - len, "cont_drv1_time = 0x%02X, cont_drv2_time = 0x%02X\n", fs3002->dts_info.fs3002_cont_drv1_time, fs3002->dts_info.fs3002_cont_drv2_time);
	return len;
}

//okok
//show fs3002->dts_info.fs3002_brk_times
//cat cont_brk_times
//cont_brk_times = 0x06
static ssize_t fs3002_cont_drv_time_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int databuf[2] = { 0, 0 };
	pr_info("enter\n");

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) 
	{
		fs3002->dts_info.fs3002_cont_drv1_time = databuf[0];
		fs3002->dts_info.fs3002_cont_drv2_time = databuf[1];
		fs3002_i2c_write(fs3002, FS3002_DRVCFG3,fs3002->dts_info.fs3002_cont_drv1_time);
		fs3002_i2c_write(fs3002, FS3002_DRVCFG4,fs3002->dts_info.fs3002_cont_drv2_time);
	}
	else
	{
		pr_info("Wrong parameter\n");
	}	
	return count;
}


//okok
//read the register and get tracking F0
//cat cont
//1723 (Because can't handle decimals, need to divide the result by 10)

static ssize_t fs3002_cont_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;

	pr_info("enter");

	fs3002_haptic_read_cont_f0(fs3002);
	len += snprintf(buf + len, PAGE_SIZE - len,"%d\n", fs3002->cont_f0);
	return len;
}

//okok
// Make a cont go
//echo 1 > cont
static ssize_t fs3002_cont_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int val = 0;
	int rc = 0;

	pr_info("enter");

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	fs3002_haptic_stop(fs3002);
	if (val)
	{
		fs3002_haptic_cont_play(fs3002);
	}
	return count;
}


//okok
//cat ram_num
//show how many waveforms are there in the currently loaded fs300_2haptic.bin file(fs3002->ram.ram_num)
static ssize_t fs3002_ram_num_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "ram_num = %d\n", fs3002->ram.ram_num);
	return len;
}


//okok
//Display the contents of the RAM bin (read from RAM, not directly read from the bin file)
//cat ram_update
//haptic_ram len = 893
//...
//...
static ssize_t fs3002_ram_update_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	unsigned int i = 0;
	unsigned char reg_val = 0;
	pr_info("enter\n");

	//set to ram mode
	fs3002_haptic_set_mode(fs3002, FS3002_HAPTIC_RAM_MODE);

	//RAMINIT Enable
	fs3002_haptic_raminit(fs3002, true);
	fs3002_haptic_stop(fs3002);
	fs3002_set_base_addr(fs3002);

	len += snprintf(buf + len, PAGE_SIZE - len,"haptic_ram len = %d\n", fs3002->ram.len);
	for (i = 0; i < fs3002->ram.len; i++) 
	{
		fs3002_i2c_read(fs3002, FS3002_RAMRDATA, &reg_val);
		if ((i+1) % 16 == 0)
			len += snprintf(buf + len,PAGE_SIZE - len, "%02X\n", reg_val);
		else
			len += snprintf(buf + len,PAGE_SIZE - len, "%02X,", reg_val);
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	// RAMINIT Disable
	fs3002_haptic_raminit(fs3002, false);
	return len;
}

//okok
// Reload the RAM bin file
// echo 1 > ram_update
static ssize_t fs3002_ram_update_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int val = 0;
	int rc = 0;
	pr_info("enter\n");

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val)
	{
		fs3002_haptic_set_mode(fs3002, FS3002_HAPTIC_RAM_MODE);
		fs3002_ram_update(fs3002);
	}
	return count;
}

//okok
//cat ram_max
//show the maximum value of 32 RAM short shock waveforms in total (fs3002->ram.ram_max)
static ssize_t fs3002_ram_max_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	unsigned int i = 0;
	pr_info("enter\n");
	

	for (i = 0; i < FS3002_RAM_NUM_MAX; i++) 
	{
		if ((i+1) % 16 == 0)
			len += snprintf(buf + len,PAGE_SIZE - len, "%02X\n", fs3002->ram.ram_max[i]);
		else
			len += snprintf(buf + len,PAGE_SIZE - len, "%02X,", fs3002->ram.ram_max[i]);
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	return len;
}

static ssize_t fs3002_ram_max_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	int rc = 0;
	unsigned int i = 0;
	unsigned int val = 0;
	pr_info("enter\n");

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
	{
		pr_err("%s:kstrtouint fail\n", FSERROR);
		return rc;
	}

	pr_info("value=%d\n", val);
	for (i = 0; i < FS3002_RAM_NUM_MAX; i++) 
	{
		fs3002->ram.ram_max[i] = val;
	}

	return count;
}




//okok
//show seq1
//cat index
//index = 1
static ssize_t fs3002_index_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned char reg_val = 0;
	pr_info("enter\n");

	reg_val = fs3002_i2c_read_bits(fs3002, FS3002_WFSCFG1, 6, 0);
	return snprintf(buf, PAGE_SIZE, "index = %d\n", reg_val);
}

//okok
//Set seq1 and set loop1 to 0xf
//cat index
//index = 1
//echo 2 > index [hex is ok:echo 0x02 > index]
//cat index
//index = 2
//cat loop
//seq1_loop = 0x0f
//seq2_loop = 0x00
//seq3_loop = 0x00
//seq4_loop = 0x00
//seq5_loop = 0x00
//seq6_loop = 0x00
//seq7_loop = 0x00
//seq8_loop = 0x00
static ssize_t fs3002_index_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int val = 0;
	int rc = 0;
	pr_info("enter\n");

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
	{
		pr_err("%s:kstrtouint fail\n", FSERROR);
		return rc;
	}

	pr_info("value=%d\n", val);
	mutex_lock(&fs3002->lock);
	fs3002_haptic_set_repeat_wav_seq(fs3002, val);
	mutex_unlock(&fs3002->lock);
	return count;
}


//okok
//show current seq
//cat seq
//seq1: 0x01
//seq2: 0x00
//seq3: 0x00
//seq4: 0x00
//seq5: 0x00
//seq6: 0x00
//seq7: 0x00
//seq8: 0x00
static ssize_t fs3002_seq_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	size_t count = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;
	pr_info("enter\n");

	for (i = 0; i < FS3002_SEQUENCER_SIZE; i++) 
	{
		fs3002_i2c_read(fs3002, FS3002_WFSCFG1 + i, &reg_val);
		count += snprintf(buf + count, PAGE_SIZE - count,"seq%d: 0x%02x\n", i + 1, reg_val & 0x7f);
		fs3002->seq[i] |= reg_val;
	}
	return count;
}


//okok
// Set seq. Note that the first parameter ranges from 0 to 7
//cat seq
//seq1: 0x01
//seq2: 0x00
//seq3: 0x00
//seq4: 0x00
//seq5: 0x00
//seq6: 0x00
//seq7: 0x00
//seq8: 0x00
//echo 1 1f > seq
//cat seq
//seq1: 0x01
//seq2: 0x1f
//seq3: 0x00
//seq4: 0x00
//seq5: 0x00
//seq6: 0x00
//seq7: 0x00
//seq8: 0x00
static ssize_t fs3002_seq_store(struct device *dev, struct device_attribute *attr, const char *buf,size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int databuf[2] = { 0, 0 };
	pr_info("enter\n");

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) 
	{
		if (databuf[0] >= FS3002_SEQUENCER_SIZE) 
		{
			pr_err("%s:input value out of range\n", FSERROR);
			return count;
		}
		pr_info("seq%d=0x%02X\n",databuf[0], databuf[1]);
		mutex_lock(&fs3002->lock);
		fs3002->seq[databuf[0]] = (unsigned char)databuf[1];
		fs3002_haptic_set_wav_seq(fs3002, (unsigned char)databuf[0],fs3002->seq[databuf[0]]);
		mutex_unlock(&fs3002->lock);
	}
	return count;
}



//okok
// Cat loop displays the value of 8 RAM play loops
//seq1_loop = 0x0f
//seq2_loop = 0x00
//seq3_loop = 0x00
//seq4_loop = 0x00
//seq5_loop = 0x00
//seq6_loop = 0x00
//seq7_loop = 0x00
//seq8_loop = 0x00
static ssize_t fs3002_loop_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	size_t count = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;
	pr_info("enter\n");

	for (i = 0; i < FS3002_SEQUENCER_LOOP_SIZE/2; i++) 
	{
		fs3002_i2c_read(fs3002, FS3002_WFSLOOP1 + i, &reg_val);
		fs3002->loop[i*2+0] = (reg_val >> 0) & 0x0F;
		fs3002->loop[i*2+1] = (reg_val >> 4) & 0x0F;

		count += snprintf(buf + count, PAGE_SIZE - count,"seq%d_loop = 0x%02x\n", i * 2 + 1,fs3002->loop[i*2+0]);
		count += snprintf(buf + count, PAGE_SIZE - count,"seq%d_loop = 0x%02x\n", i * 2 + 2,fs3002->loop[i*2+1]);
	}


	return count;
}

//okok
//Set loop. Note that the first parameter ranges from 0 to 7
//Cat loop
//seq1_loop = 0x0f
//seq2_loop = 0x00
//seq3_loop = 0x00
//seq4_loop = 0x00
//seq5_loop = 0x00
//seq6_loop = 0x00
//seq7_loop = 0x00
//seq8_loop = 0x00
//echo 1 e > loop
//cat loop
//seq1_loop = 0x0f
//seq2_loop = 0x0e
//seq3_loop = 0x00
//seq4_loop = 0x00
//seq5_loop = 0x00
//seq6_loop = 0x00
//seq7_loop = 0x00
//seq8_loop = 0x00
static ssize_t fs3002_loop_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int databuf[2] = { 0, 0 };
	pr_info("enter\n");

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) 
	{
		if (databuf[0] >= FS3002_SEQUENCER_SIZE) 
		{
			pr_err("%s:input value out of range\n", FSERROR);
			return count;
		}
		pr_info("seq%d loop=0x%02X\n",databuf[0], databuf[1]);
		mutex_lock(&fs3002->lock);
		fs3002->loop[databuf[0]] = (unsigned char)databuf[1];
		fs3002_haptic_set_wav_loop(fs3002, (unsigned char)databuf[0],fs3002->loop[databuf[0]]);
		mutex_unlock(&fs3002->lock);
	}
	return count;
}



//okok
//show fs3002->timer remaining and duration  
//cat duration
//fs3002->timer remaining = 0, fs3002->duration = 2000
static ssize_t fs3002_duration_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ktime_t time_rem;
	s64 time_ms = 0;
	pr_info("enter\n");

	if (hrtimer_active(&fs3002->timer)) 
	{
		time_rem = hrtimer_get_remaining(&fs3002->timer);
		time_ms = ktime_to_ms(time_rem);
	}
	return snprintf(buf, PAGE_SIZE, "fs3002->timer remaining = %lld, fs3002->duration = %d\n", time_ms, fs3002->duration);
}

//okok
// By setting FS3002 ->duration, you can control the duration of RAM loop playback
//enum fs3002_haptic_activate_mode {
//	FS3002_HAPTIC_ACTIVATE_RAM_MODE = 0,
//	FS3002_HAPTIC_ACTIVATE_CONT_MODE = 1,
//  FS3002_HAPTIC_ACTIVATE_RTP_MODE = 2,
//	FS3002_HAPTIC_ACTIVATE_RAM_LOOP_MODE = 3,
//};
//echo 2000 > duration   set 2000ms [hex is ok:echo 0xff > duration]
//echo 3 > activate_mode
//echo 1 > activate
static ssize_t fs3002_duration_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int val = 0;
	int rc = 0;
	pr_info("enter\n");

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	//setting 0 on duration is NOP for now
	if (val <= 0)
		return count;

	//zzzz need to add this function in some platforms[fs3002_haptic_play_effect_seq-->fs3002_haptic_set_repeat_wav_seq]
	//fs3002_haptic_duration_ram_play_config(fs3002,val);
	fs3002->duration = val;
	return count;
}

//okok
//cat effect_id
static ssize_t fs3002_effect_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	pr_info("enter\n");
	return snprintf(buf, PAGE_SIZE, "effect_id =%d\n", fs3002->effect_id);
}

//okok
//echo 100 > effect_id
//set fs3002->effect_id
static ssize_t fs3002_effect_id_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int databuf[1] = { 0 };
	pr_info("enter\n");

	if (sscanf(buf, "%x", &databuf[0]) == 1) 
	{
		mutex_lock(&fs3002->lock);
		fs3002->effect_id = databuf[0];
		fs3002->play.vmax_mv = FS3002_MEDIUM_MAGNITUDE;
		mutex_unlock(&fs3002->lock);
		pr_info("effect_id = %d\n",fs3002->effect_id);
	}
	else
	{
		pr_info("Wrong parameter\n");
	}
	return count;
}

//okok
//Displays the number of transmitted bytes of the current RTP fs3002-> rtp_cnt
//cat rtp
//rtp_cnt = 0
//echo 172 > rtp
//cat rtp
//rtp_cnt = 37376    //it's playing, so it can show a not zero number
//cat rtp
//rtp_cnt = 0			//The rtp playing is over, so it shows 0
static ssize_t fs3002_rtp_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	pr_info("enter\n");

	len += snprintf(buf + len, PAGE_SIZE - len,"rtp_cnt = %d\n",fs3002->rtp_cnt);
	return len;
}

//okok
//example:echo 41 > rtp [hex is ok: echo 0x29 > rtp]
// Play an RTP
//sizeof(fs3002_rtp_name) / FS3002_RTP_NAME_MAX is total rtp files number 173(0--172) 
static ssize_t fs3002_rtp_store(struct device *dev,struct device_attribute *attr, const char *buf,size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int val = 0;
	int rc = 0;
	pr_info("enter\n");

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0) 
	{
		pr_err("%s:kstrtouint fail\n", FSERROR);
		return rc;
	}
	mutex_lock(&fs3002->lock);
	fs3002_haptic_stop(fs3002);
	//Set the INT of almost emtpy. This is set in the fs3002_rtp_work_routine function, so it is not needed here
	//fs3002_haptic_set_rtp_aei(fs3002, false);
	//fs3002_interrupt_clear(fs3002);
	if (val < (sizeof(fs3002_rtp_name) / FS3002_RTP_NAME_MAX))
	{
		fs3002->effect_id = val;
		pr_info("fs3002_rtp_name[%d]: %s\n",val, fs3002_rtp_name[val]);
		queue_work(fs3002->work_queue, &fs3002->rtp_work);
	}
	else if(val >= (sizeof(fs3002_rtp_name) / FS3002_RTP_NAME_MAX))
	{
		pr_err("%s:rtp_file_num %u >= max value(%zu) \n", FSERROR, val, (sizeof(fs3002_rtp_name) / FS3002_RTP_NAME_MAX));
	}

	mutex_unlock(&fs3002->lock);
	return count;
}

static ssize_t fs3002_buf_size_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	pr_info("enter\n");

	len += snprintf(buf + len, PAGE_SIZE - len,"buf_size = %d\n",fs3002->buf_size);
	return len;
}

static ssize_t fs3002_buf_size_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int val = 0;
	int rc = 0;

	pr_info("enter\n");

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
	{
		pr_err("%s:input value out of range\n", FSERROR);
		return rc;
	}

    fs3002->buf_size = val;
    pr_info("buf_size = %d\n",fs3002->buf_size);

	return count;
}

static ssize_t fs3002_rtp_auto_env_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	pr_info("enter\n");

	return snprintf(buf, PAGE_SIZE, "rtp_auto_env = %d\n", fs3002->dts_info.fs3002_rtp_auto_env);
}

//cat rtp_auto_env
//rtp_auto_env = 0
//echo 1 > rtp_auto_env
//cat rtp_auto_env
//rtp_auto_env = 1
static ssize_t fs3002_rtp_auto_env_store(struct device *dev,struct device_attribute *attr, const char *buf,size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	int databuf[1] = { 0 };
	pr_info("enter\n");
	if (sscanf(buf, "%x", &databuf[0]) == 1) 
	{
		pr_info("rtp_auto_env = %d\n",databuf[0]);
		if (databuf[0] != 0 && databuf[0] != 1)
		{
			pr_info("1:input value_0=%d out of range\n",databuf[0]);
			return count;
		}
		
		fs3002->dts_info.fs3002_rtp_auto_env = databuf[0];
		pr_info("2:rtp_auto_env = %d\n",fs3002->dts_info.fs3002_rtp_auto_env);
	}
	return count;
}



//okok
//cat rtp_auto_env_value
//rtp_auto_env_mid = 0x40
//rtp_auto_env_high = 0x70
static ssize_t fs3002_rtp_auto_env_value_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	pr_info("enter\n");

	len += snprintf(buf + len, PAGE_SIZE - len, "rtp_auto_env_mid = 0x%02X, rtp_auto_env_high = 0x%02X\n", fs3002->dts_info.fs3002_rtp_auto_env_mid, fs3002->dts_info.fs3002_rtp_auto_env_high);
	return len;
}

//okok
//cat rtp_auto_env_value
//rtp_auto_env_mid = 0x40
//rtp_auto_env_high = 0x70
//echo 0x30 060 > rtp_auto_env_value
//cat rtp_auto_env_value
//rtp_auto_env_mid = 0x30
//rtp_auto_env_high = 0x60
static ssize_t fs3002_rtp_auto_env_value_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int databuf[2] = { 0, 0 };
	pr_info("enter\n");

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) 
	{
		fs3002->dts_info.fs3002_rtp_auto_env_mid = databuf[0];
		fs3002->dts_info.fs3002_rtp_auto_env_high = databuf[1];
		fs3002_i2c_write(fs3002, FS3002_ADPBST4,fs3002->dts_info.fs3002_rtp_auto_env_mid);
		fs3002_i2c_write(fs3002, FS3002_ADPBST3,fs3002->dts_info.fs3002_rtp_auto_env_high);
	}
	else
	{
		pr_info("Wrong parameter\n");
	}	
	return count;
}

static ssize_t fs3002_rtp_auto_size_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	pr_info("enter\n");

	len += snprintf(buf + len, PAGE_SIZE - len, "rtp_auto_size = %d\n", fs3002->dts_info.fs3002_rtp_auto_size);
	return len;
}


static ssize_t fs3002_rtp_auto_size_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int databuf[1] = { 0};
	pr_info("enter\n");

	if (sscanf(buf, "%x", &databuf[0]) == 1) 
	{
		if (databuf[0] != 0 && databuf[0] != 1)
		{
			pr_info("1:input value=%d out of range\n",databuf[0]);
			return count;
		}
		fs3002->dts_info.fs3002_rtp_auto_size = databuf[0];
	}
	else
	{
		pr_info("Wrong parameter\n");
	}	
	return count;
}



//okok
//cat custom_wave
//return buffer size, availbe size and fs3002_rtp_max
static ssize_t fs3002_custom_wave_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	pr_info("enter, period_size=%d;max_size=%d;free_size=%d;custom_wave_id=%d;\n",fs3002->ram.base_addr >> 2,get_rb_max_size(), get_rb_free_size(),CUSTOME_WAVE_ID);

	len += snprintf(buf + len, PAGE_SIZE - len, "period_size=%d;", fs3002->ram.base_addr >> 2);
	len += snprintf(buf + len, PAGE_SIZE - len, "max_size=%d;free_size=%d;", get_rb_max_size(), get_rb_free_size());
	len += snprintf(buf + len, PAGE_SIZE - len, "custom_wave_id=%d;", CUSTOME_WAVE_ID);
	return len;
}

//okok
//zzzz   It's not entirely clear what this thing is for. It's likely that data can be written into this ringbuffer, and then used as a trigger for RTP
static ssize_t fs3002_custom_wave_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fs3002 *fs3002;
	unsigned long  buf_len, period_size, offset;
	int ret;
	pr_info("enter\n");

	if (!g_foursemi){
		pr_info("g_foursemi is null, return\n");
		return -ENODEV;
	}
	if (!g_foursemi->fs3002){
		pr_info("g_foursemi->fs3002 is null, return\n");
		return -ENODEV;
	}
	fs3002 = g_foursemi->fs3002;

	period_size = (fs3002->ram.base_addr >> 2);
	offset = 0;
	if (period_size == 0) 
	{
		pr_info("period_size is 0, return\n");
		return -EINVAL;
	}
	pr_info("write size %zd, period size %lu", count, period_size);
	if (count % period_size || count < period_size)
	{
		rb_end();
	}
		
	atomic_set(&fs3002->is_in_write_loop, 1);

	while (count > 0) 
	{
		buf_len = MIN(count, period_size);
		ret = write_rb(buf + offset,  buf_len);
		if (ret < 0)
			goto exit;
		count -= buf_len;
		offset += buf_len;
	}
	ret = offset;
exit:
	atomic_set(&fs3002->is_in_write_loop, 0);
	wake_up_interruptible(&fs3002->stop_wait_q);
	pr_info(" return size %d", ret);
	return ret;
}

//show fs3002->is_custom_wave
static ssize_t fs3002_custom_is_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	pr_info("enter:is_custom_wave = %d\n", fs3002->is_custom_wave);

	len += snprintf(buf + len, PAGE_SIZE - len, "is_custom_wave = %d\n", fs3002->is_custom_wave);
	return len;
}

//set fs3002->is_custom_wave
static ssize_t fs3002_custom_is_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int databuf[1] = { 0};
	pr_info("enter\n");

	if (sscanf(buf, "%x", &databuf[0]) == 1) 
	{
		if (databuf[0] != 0 && databuf[0] != 1)
		{
			pr_info("1:input value=%d out of range\n",databuf[0]);
			return count;
		}
		fs3002->is_custom_wave = databuf[0];
	}
	else
	{
		pr_info("Wrong parameter\n");
	}	
	return count;
}

static ssize_t fs3002_print_rb_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	unsigned char i = 0;
	unsigned int buf_len = 0,count = 0;

	pr_info("enter");

	vfree(fs3002->rtp_container);			
	fs3002->rtp_container = NULL;			
	if(fs3002->ram.base_addr != 0) 			
	{				
		fs3002->rtp_container = vmalloc((fs3002->ram.base_addr >> 2) + sizeof(int));
	}

	while(1)
	{
		buf_len = read_rb(fs3002->rtp_container->data,  fs3002->ram.base_addr >> 2);
		count = count + buf_len;		
		if (buf_len < (fs3002->ram.base_addr >> 2)) 			
		{				
			pr_info("read_rb completed, total bytes = %d\n", count);			
			break;			
		}
	}

	for (i = 0; i <= count; i++) 
	{
		if((i+1) % 8 == 0)
		{
			len += snprintf(buf + len, PAGE_SIZE - len,"0x%02X=0x%02X\n", i, fs3002->rtp_container->data[i]);
		}
		else
		{
			len += snprintf(buf + len, PAGE_SIZE - len,"0x%02X=0x%02X ", i, fs3002->rtp_container->data[i]);
		}
	}
	return len;
}


static ssize_t fs3002_rtp_bst_sel_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	pr_info("enter\n");

	return snprintf(buf, PAGE_SIZE, "rtp_bst_sel = %d\n", fs3002->dts_info.fs3002_rtp_bst_sel);
}

//cat rtp_bst_sel
//rtp_bst_sel = 1
//echo 0 > rtp_bst_sel
//cat rtp_bst_sel
//rtp_bst_sel = 0
static ssize_t fs3002_rtp_bst_sel_store(struct device *dev,struct device_attribute *attr, const char *buf,size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	int databuf[1] = { 0 };
	pr_info("enter\n");
	if (sscanf(buf, "%x", &databuf[0]) == 1) 
	{
		pr_info("rtp_bst_sel = %d\n",databuf[0]);
		if (databuf[0] != 0 && databuf[0] != 1)
		{
			pr_info("1:input value_0=%d out of range\n",databuf[0]);
			return count;
		}
		
		fs3002->dts_info.fs3002_rtp_bst_sel = databuf[0];
		pr_info("2:rtp_bst_sel = %d\n",fs3002->dts_info.fs3002_rtp_bst_sel);
	}
	return count;
}



//okok
//cat trig
//fs3002->trig[i].trig_level			edge mode or level mode
//fs3002->trig[i].trig_polar 			
//fs3002->trig[i].pos_enable,
//fs3002->trig[i].pos_sequence,
//fs3002->trig[i].neg_enable,
//fs3002->trig[i].neg_sequence,
//fs3002->trig[i].trig_brk,
//fs3002->trig[i].trig_bst
static ssize_t fs3002_trig_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	unsigned char i = 0;
	pr_info("enter\n");

	for (i = 0; i < FS3002_TRIG_NUM; i++) 
	{
		len += snprintf(buf + len, PAGE_SIZE - len,
				"trig%d: trig_level=%d, trig_polar=%d, pos_enable=%d, pos_sequence=%d, neg_enable=%d, neg_sequence=%d trig_brk=%d, trig_bst=%d\n",
				i + 1,
				fs3002->trig[i].trig_level,
				fs3002->trig[i].trig_polar,
				fs3002->trig[i].pos_enable,
				fs3002->trig[i].pos_sequence,
				fs3002->trig[i].neg_enable,
				fs3002->trig[i].neg_sequence,
				fs3002->trig[i].trig_brk,
				fs3002->trig[i].trig_bst);
	}

	return len;
}

//okok
//echo 1 0 0 0 0 1 2 0 0 > trig
//trig number = 1
//0:edge mode
//0:polar
//0:positive trig enable
//0:positive trig seq
//1:negtive trig enable
//2:negtive trig seq
//0:trig brake enable
//0:trig boost enable
static ssize_t fs3002_trig_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int databuf[9] = { 0 };
	pr_info("enter\n");

	if (sscanf(buf, "%x %x %x %x %x %x %x %x %x", &databuf[0], &databuf[1], &databuf[2], &databuf[3], &databuf[4], &databuf[5], &databuf[6], &databuf[7], &databuf[8]) == 9) 
	{
		pr_info("%d, %d, %d, %d, %d, %d, %d, %d, %d\n", databuf[0], databuf[1], databuf[2], databuf[3], databuf[4], databuf[5], databuf[6], databuf[7], databuf[8]);
		if (!fs3002->ram_init) 
		{
			pr_err("%s,ram init failed, not allow to play!\n",FSERROR);
			return count;
		}		
		if (databuf[0] < 1 || databuf[0] > 3) 
		{
			pr_err("%s, para_0[0-8]:input trig_num out of range!\n",FSERROR);
			return count;
		}
		if (databuf[1]!= 0 && databuf[1] != 1) 
		{
			pr_err("%s,para_1[0-8]:edge mode should be 0 or 1\n",FSERROR);
			return count;
		}
		if (databuf[2]!= 0 && databuf[2] != 1) 
		{
			pr_err("%s,para_2[0-8]:polar should be 0 or 1\n",FSERROR);
			return count;
		}	
		if (databuf[3]!= 0 && databuf[3] != 1) 
		{
			pr_err("%s,para_3[0-8]:pos enable should be 0 or 1\n",FSERROR);
			return count;
		}			
		if (databuf[4] > fs3002->ram.ram_num) 
		{
			pr_err("%s,para_4[0-8]:pos seq value out of range! (should be < %d)\n",FSERROR,fs3002->ram.ram_num);
			return count;
		}
		if (databuf[5]!= 0 && databuf[5] != 1) 
		{
			pr_err("%s,para_5[0-8]:neg enable should be 0 or 1\n",FSERROR);
			return count;
		}
		if (databuf[6] > fs3002->ram.ram_num) 
		{
			pr_err("%s,para_6[0-8]:neg seq value out of range! (should be < %d)\n",FSERROR,fs3002->ram.ram_num);
			return count;
		}		
		if (databuf[7]!= 0 && databuf[7] != 1) 
		{
			pr_err("%s,para_7[0-8]:brk enable should be 0 or 1\n",FSERROR);
			return count;
		}
		if (databuf[8]!= 0 && databuf[8] != 1) 
		{
			pr_err("%s,para_8[0-8]:boost enable should be 0 or 1\n",FSERROR);
			return count;
		}	

		databuf[0] = databuf[0] - 1;

		fs3002->trig[databuf[0]].trig_level = databuf[1];
		fs3002->trig[databuf[0]].trig_polar = databuf[2];
		fs3002->trig[databuf[0]].pos_enable = databuf[3];
		fs3002->trig[databuf[0]].pos_sequence = databuf[4];
		fs3002->trig[databuf[0]].neg_enable = databuf[5];
		fs3002->trig[databuf[0]].neg_sequence = databuf[6];
		fs3002->trig[databuf[0]].trig_brk = databuf[7];
		fs3002->trig[databuf[0]].trig_bst = databuf[8];
		mutex_lock(&fs3002->lock);
		switch (databuf[0]) 
		{
			case 0:
				fs3002_haptic_trig1_param_config(fs3002);
				break;
			case 1:
				fs3002_haptic_trig2_param_config(fs3002);
				break;
			case 2:
				fs3002_haptic_trig3_param_config(fs3002);
				break;
		}
		mutex_unlock(&fs3002->lock);
	}
	else
	{
		pr_info("Wrong parameter. example:echo 1 0 0 0 0 1 2 0 0 > trig\n");
		//pr_info("echo 1            0               0           0                0         1                2          0                 0                > trig\n");
		pr_info(" [trig number 1-3] [edge mode 0-1] [polar 0-1] [pos enable 0-1] [pos seq] [neg enable 0-1] [pos seq]  0[brk enable 0-1] 0[bst enable 0-1]\n");
	}
	return count;
}


//okok
static ssize_t fs3002_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;

	return snprintf(buf, PAGE_SIZE, "state = %d\n", fs3002->state);
}

//okok
//show fs3002->activate_mode
//cat activate_mode
//fs3002->activate_mode = 2
static ssize_t fs3002_activate_mode_show(struct device *dev,struct device_attribute *attr,char *buf)

{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	pr_info("enter\n");

	return snprintf(buf, PAGE_SIZE, "fs3002->activate_mode = %d\n",fs3002->activate_mode);

}

//okok
//echo 2 > activate_mode			//set fs3002->activate_mode
//cat activate_mode
//fs3002->activate_mode = 2			[hex is ok:echo 0x02 > activate_mode]
//enum fs3002_haptic_activate_mode {
//	FS3002_HAPTIC_ACTIVATE_RAM_MODE = 0,
//	FS3002_HAPTIC_ACTIVATE_CONT_MODE = 1,
//  FS3002_HAPTIC_ACTIVATE_RTP_MODE = 2,
//	FS3002_HAPTIC_ACTIVATE_RAM_LOOP_MODE = 3,
static ssize_t fs3002_activate_mode_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int val = 0;
	int rc = 0;
	pr_info("enter\n");

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
	{
		pr_err("%s:kstrtouint fail\n", FSERROR);
		return rc;
	}
		
	mutex_lock(&fs3002->lock);
	fs3002->activate_mode = val;
	mutex_unlock(&fs3002->lock);
	return count;
}


//okok
//show fs3002->activate_mode and fs3002->state
//cat activate
//fs3002->activate = 3 fs3002->state=0
static ssize_t fs3002_activate_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	
	pr_info("enter");
	return snprintf(buf, PAGE_SIZE, "fs3002->activate_mode = %d fs3002->state=%d\n", fs3002->activate_mode,fs3002->state);

}

//okok
static ssize_t fs3002_activate_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;	
	unsigned int val = 0;
	int rc = 0;

	pr_info("enter");

	if (!fs3002->ram_init) 
	{
		pr_err("%s:ram init failed, not allow to play!\n", FSERROR);
		return count;
	}
	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
	{
		pr_err("%s:kstrtouint fail\n",FSERROR);
		return rc;
	}
	
	pr_info("value=%d\n", val);
	if (val != 0 && val != 1)
	{
		pr_err("%s:fs3002_activate_store value must be 0 or 1\n",FSERROR);
		return count;
	}

	mutex_lock(&fs3002->lock);
	hrtimer_cancel(&fs3002->timer);
	fs3002->state = val;
	mutex_unlock(&fs3002->lock);
	queue_work(fs3002->work_queue, &fs3002->vibrate_work);
	return count;
}


//okok
//show fs3002->dts_info.fs3002_brk_times
//cat cont_brk_times
//cont_brk_times = 0x06
static ssize_t fs3002_brk_times_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	pr_info("enter\n");

	len += snprintf(buf + len, PAGE_SIZE - len, "brk_times = 0x%02X\n", fs3002->dts_info.fs3002_brk_times);
	return len;
}

//okok
//set brk cont brk times
//cat brk_times
//brk_times = 0x06
//echo 5 > brk_times
//brk_times = 0x05
static ssize_t fs3002_brk_times_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int databuf[1] = { 0 };
	pr_info("enter\n");

	if (sscanf(buf, "%x", &databuf[0]) == 1) 
	{
		fs3002->dts_info.fs3002_brk_times = databuf[0];
		fs3002_i2c_write(fs3002, FS3002_BRKCFG5,fs3002->dts_info.fs3002_brk_times);
	}
	else
	{
		pr_info("Wrong parameter\n");
	}	

	return count;
}


//cat auto_brake
//show auto brake setting
static ssize_t fs3002_auto_brake_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	pr_info("enter\n");

	len += snprintf(buf + len, PAGE_SIZE - len,"auto_brake = 0x%02X\n",fs3002->dts_info.fs3002_auto_brake);
	return len;
}

//echo 1 > auto_brake
//set fs3002->dts_info.fs3002_auto_brake
static ssize_t fs3002_auto_brake_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int databuf[1] = { 0 };
	pr_info("enter\n");

	if (sscanf(buf, "%x", &databuf[0]) == 1) 
	{
		pr_info("auto_brake = %d\n",databuf[0]);
		if (databuf[0] != 0 && databuf[0] != 1)
		{
			pr_info("1:input value=%d out of range\n",databuf[0]);
			return count;
		}
		
		fs3002->dts_info.fs3002_auto_brake = databuf[0];
		pr_info("2:fs3002_auto_brake = %d\n",fs3002->dts_info.fs3002_auto_brake);
	}

	return count;
}



//show fs3002->dts_info.fs3002_bypass_system_gain
//cat bypass_system_gain
//bypass_system_gain = 0
static ssize_t fs3002_bypass_system_gain_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	pr_info("enter\n");

	return snprintf(buf, PAGE_SIZE, "bypass_system_gain = %d\n", fs3002->dts_info.fs3002_bypass_system_gain);
}

//set fs3002->dts_info.fs3002_bypass_system_gain
//cat bypass_system_gain
//bypass_system_gain = 0
//echo 1 > bypass_system_gain
//cat bypass_system_gain
//bypass_system_gain = 1
static ssize_t fs3002_bypass_system_gain_store(struct device *dev,struct device_attribute *attr, const char *buf,size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	int databuf[1] = { 0 };
	pr_info("enter\n");
	if (sscanf(buf, "%x", &databuf[0]) == 1) 
	{
		pr_info("bypass_system_gain = %d\n",databuf[0]);
		if (databuf[0] != 0 && databuf[0] != 1)
		{
			pr_info("1:input value_0=%d out of range\n",databuf[0]);
			return count;
		}
		
		fs3002->dts_info.fs3002_bypass_system_gain = databuf[0];
		pr_info("2:bypass_system_gain = %d\n",fs3002->dts_info.fs3002_bypass_system_gain);
	}
	return count;
}


//okok
//Display the Settings of the GAIN (register FS3002 GAINCFG)
//cat gain
//0x80
static ssize_t fs3002_gain_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	unsigned char reg = 0;
	ssize_t len = 0;
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	pr_info("enter\n");

	fs3002_i2c_read(fs3002, FS3002_GAINCFG, &reg);
	len += snprintf(buf + len, PAGE_SIZE - len,"fs3002->gain=0x%2x,reg gain=0x%2x\n",fs3002->gain,reg);
	return len;
}

//okok
//set GAIN to FS3002 -> GAIN, but there is a conversion process (voltage compensation factor)
//cat gain
//0x80
//echo 126 > gain		[hex is ok: echo 0x7E > gain]
//cat gain
//0x7E
static ssize_t fs3002_gain_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int val = 0;
	int rc = 0;
	pr_info("enter\n");

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
	{
		return rc;
	}

	pr_info("value=%d\n", val);
	
	mutex_lock(&fs3002->lock);
	fs3002->gain = val;
	fs3002_haptic_set_gain(fs3002, fs3002->gain);
	mutex_unlock(&fs3002->lock);
	return count;
}

//okok
static ssize_t fs3002_boost_mode_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	unsigned char reg_val = 0;
	pr_info("enter\n");

	reg_val = fs3002_i2c_read_bits(fs3002, FS3002_BSTCTRL2,1,0);
	switch (reg_val) 
	{
		case 0:
		case 3:
			len += snprintf(buf + len, PAGE_SIZE - len,"follow mode\n");
			break;
		case 1:
			len += snprintf(buf + len, PAGE_SIZE - len,"boost mode\n");
			break;
		case 2:
			len += snprintf(buf + len, PAGE_SIZE - len,"adaptive mode\n");
			break;
	}
	
	return len;
}

//okok
static ssize_t fs3002_boost_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int databuf[1] = { 0 };
	pr_info("enter\n");

	if (sscanf(buf, "%x", &databuf[0]) == 1) 
	{
		pr_info("debug_enable = %d\n",databuf[0]);
		if (databuf[0] != 0 && databuf[0] != 1 && databuf[0] != 2)
		{
			pr_info("1:input value=%d out of range\n",databuf[0]);
			return count;
		}
		fs3002_i2c_write_bits(fs3002, FS3002_BSTCTRL2,databuf[0],1,0);
	}

	return count;
}


//okok
//cat vref
//show low_vref, mid_vref, high_vref, trig1_p_vref, trig1_n_vref, trig2_p_vref, trig2_n_vref, trig3_p_vref, trig3_n_vref, 
static ssize_t fs3002_vref_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	pr_info("enter\n");

	len += snprintf(buf + len, PAGE_SIZE - len, "lowv_ref=0x%x, midv_ref=0x%x, highv_ref=0x%x,trig1_p_vref=0x%x, trig1_n_vref=0x%x, trig2_p_vref=0x%x, trig2_n_vref=0x%x, trig3_p_vref=0x%x, trig3_n_vref=0x%x\n", 
												fs3002->dts_info.fs3002_lowv_ref, fs3002->dts_info.fs3002_midv_ref, fs3002->dts_info.fs3002_highv_ref, 
												fs3002->dts_info.fs3002_trig1_p_ref, fs3002->dts_info.fs3002_trig1_n_ref, 
												fs3002->dts_info.fs3002_trig2_p_ref, fs3002->dts_info.fs3002_trig2_n_ref, 
												fs3002->dts_info.fs3002_trig3_p_ref, fs3002->dts_info.fs3002_trig3_n_ref);
	return len;
}

//okok
//echo 6,0,0x23,0x21,0x21,0x21,0x21,0x21,0x21 > vref
//set low_vref, mid_vref, high_vref, trig1_p_vref, trig1_n_vref, trig2_p_vref, trig2_n_vref, trig3_p_vref, trig3_n_vref, 
static ssize_t fs3002_vref_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int databuf[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0};
	int i = 0;
	pr_info("enter\n");

	if (sscanf(buf, "%x %x %x %x %x %x %x %x %x", &databuf[0], &databuf[1], &databuf[2], &databuf[3], &databuf[4], &databuf[5], &databuf[6], &databuf[7], &databuf[8]) == 9) 
	{
		if (databuf[0] > 15)
		{
			databuf[0] = FS3002_DEFAULT_LOWV_REF;
		}
		if (databuf[1] > 3)
		{
			databuf[1] = FS3002_DEFAULT_MIDV_REF;
		}			
		if (databuf[2] > 63)
		{
			databuf[2] = FS3002_DEFAULT_HIGHV_REF;
		}

		for(i=3;i<9;i++)
		{
			if (databuf[i] > 63)
			{
				databuf[i] = FS3002_DEFAULT_TRIG_REF;
			}
		}
			
		fs3002->dts_info.fs3002_lowv_ref = databuf[0];
		fs3002->dts_info.fs3002_midv_ref = databuf[1];
		fs3002->dts_info.fs3002_highv_ref = databuf[2];

		fs3002->dts_info.fs3002_trig1_p_ref = databuf[3];
		fs3002->dts_info.fs3002_trig1_n_ref = databuf[4];
		fs3002->dts_info.fs3002_trig2_p_ref = databuf[5];
		fs3002->dts_info.fs3002_trig2_n_ref = databuf[6];
		fs3002->dts_info.fs3002_trig3_p_ref = databuf[7];
		fs3002->dts_info.fs3002_trig3_n_ref = databuf[8];
		fs3002_haptic_set_vref(fs3002);
		fs3002_haptic_set_trig_vref(fs3002);		
	}
	else
	{
		pr_info("Wrong parameter\n");
	}	
	return count;
}


//okok
static ssize_t fs3002_env_lowv_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	pr_info("enter\n");

	len += snprintf(buf + len, PAGE_SIZE - len,"%d\n",fs3002->dts_info.fs3002_env_lowv);

	return len;
}

//okok
static ssize_t fs3002_env_lowv_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int databuf[1] = { 0 };
	pr_info("enter\n");

	if (sscanf(buf, "%x", &databuf[0]) == 1) 
	{
		fs3002->dts_info.fs3002_env_lowv = databuf[0];
	}
	else
	{
		pr_info("Wrong parameter\n");
	}

	return count;
}


//okok
static ssize_t fs3002_debug_enable_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	pr_info("enter\n");

	len += snprintf(buf + len, PAGE_SIZE - len,"debug_enable = 0x%02X\n",fs3002->fs3002_debug_enable);
	return len;
}

//okok
static ssize_t fs3002_debug_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int databuf[1] = { 0 };
	pr_info("enter\n");

	if (sscanf(buf, "%x", &databuf[0]) == 1) 
	{
		pr_info("debug_enable = %d\n",databuf[0]);
		if (databuf[0] != 0 && databuf[0] != 1)
		{
			pr_info("1:input value=%d out of range\n",databuf[0]);
			return count;
		}
		
		fs3002->fs3002_debug_enable = databuf[0];
		pr_info("2:debug_enable = %d\n",fs3002->fs3002_debug_enable);
	}

	return count;
}


//cat version
//show driver version
static ssize_t fs3002_version_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	pr_info("enter\n");

	return snprintf(buf, PAGE_SIZE, "%s\n", FOURSEMI_DRIVER_VERSION);
}

static ssize_t fs3002_Qos_time_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	pr_info("enter\n");

	len += snprintf(buf + len, PAGE_SIZE - len,"Qos_time = %d\n",fs3002->Qos_time);
	return len;
}

static ssize_t fs3002_Qos_time_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int val = 0;
	int rc = 0;

	pr_info("enter\n");

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
	{
		pr_err("%s:input value out of range\n", FSERROR);
		return rc;
	}

    fs3002->Qos_time = val;
    pr_info("Qos_time = %d\n",fs3002->Qos_time);

	return count;
}

static ssize_t fs3002_lra_time_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	pr_info("enter\n");

	len += snprintf(buf + len, PAGE_SIZE - len,"lra_time = %d\n",fs3002->lra_test_sleep_time);
	return len;
}

static ssize_t fs3002_lra_time_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int val = 0;
	int rc = 0;

	pr_info("enter\n");

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
	{
		pr_err("%s:input value out of range\n", FSERROR);
		return rc;
	}

    fs3002->lra_test_sleep_time = val;
    pr_info("lra_time = %d\n",fs3002->lra_test_sleep_time);

	return count;
}



//reset the chip
//echo 1 > reset
static ssize_t fs3002_reset_store(struct device *dev,struct device_attribute *attr, const char *buf,size_t count)
{
	unsigned int val = 0;
	int rc = 0;

	pr_info("enter");

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val)
	{
		foursemi_sw_reset(g_foursemi);
	}
	return count;
}


//okok
static ssize_t fs3002_vbat_monitor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;

	mutex_lock(&fs3002->lock);
	//make a "cat f0" action
	fs3002_haptic_upload_lra(fs3002, FS3002_WRITE_ZERO);
	fs3002_haptic_set_mode(fs3002, FS3002_HAPTIC_F0_DETECT_MODE);
	fs3002_haptic_cont_get_f0(fs3002);
	fs3002_haptic_set_mode(fs3002, FS3002_HAPTIC_STANDBY_MODE);
	
	fs3002_haptic_get_vbat(fs3002);
	len += snprintf(buf + len, PAGE_SIZE - len, "vbat=%dmv\n", fs3002->vbat);
	mutex_unlock(&fs3002->lock);

	return len;
}

//okok
//show lra_resistance
//cat lra_resistance
//lra_resistance = 226
static ssize_t fs3002_lra_resistance_show(struct device *dev,struct device_attribute *attr,char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	pr_info("enter\n");
	
	if(fs3002->dts_info.fs3002_lr_pgagain != fs3002->dts_info.fs3002_brk_pgagain)
	{
		fs3002_haptic_set_pgagain(fs3002, fs3002->dts_info.fs3002_lr_pgagain);
		fs3002_haptic_offset_calibration(fs3002);
	}
	
	fs3002_haptic_get_lra_resistance(fs3002);
	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n",fs3002->lra);

	if(fs3002->dts_info.fs3002_lr_pgagain != fs3002->dts_info.fs3002_brk_pgagain)
	{
		fs3002_haptic_set_pgagain(fs3002, fs3002->dts_info.fs3002_brk_pgagain);
		fs3002_haptic_offset_calibration(fs3002);
	}
	pr_info("exit\n");
	return len;
}

//okok
//show ram,rtp sample rate
//cat sample_rate
//ram_srate = 1
//rtp_srate = 1
static ssize_t fs3002_sample_rate_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	pr_info("enter\n");

	len += snprintf(buf + len, PAGE_SIZE - len, "ram_srate = 0x%02X, rtp_srate = 0x%02X\n", fs3002->dts_info.fs3002_play_ram_srate, fs3002->dts_info.fs3002_play_rtp_srate);
	return len;
}

//okok
//set ram,rtp sample rate
//cat sample_rate
//ram_srate = 1
//rtp_srate = 1
//echo 0 0 > sample_rate
//cat sample_rate
//ram_srate = 0
//rtp_srate = 0
static ssize_t fs3002_sample_rate_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int databuf[2] = { 0, 0 };
	pr_info("enter\n");

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) 
	{
		if (databuf[0] != 0 && databuf[0] != 1 && databuf[0] != 2)
		{
			pr_info("1:input value=%d out of range[0-2]\n",databuf[0]);
			return count;
		}
		if (databuf[1] != 0 && databuf[1] != 1 && databuf[1] != 2)
		{
			pr_info("1:input value=%d out of range[0-2]\n",databuf[1]);
			return count;
		}
		fs3002->dts_info.fs3002_play_ram_srate = databuf[0];
		fs3002->dts_info.fs3002_play_rtp_srate = databuf[1];
	}
	else
	{
		pr_info("Wrong parameter\n");
	}	
	return count;
}

static ssize_t fs3002_hapstream_fre_gap_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	pr_info("enter\n");

	len += snprintf(buf + len, PAGE_SIZE - len,"hapstream_fre_gap = %d\n",fs3002->dts_info.fs3002_hapstream_fre_gap);
	return len;
}

static ssize_t fs3002_hapstream_fre_gap_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int val = 0;
	int rc = 0;

	pr_info("enter\n");

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
	{
		pr_err("%s:input value out of range\n", FSERROR);
		return rc;
	}

    fs3002->dts_info.fs3002_hapstream_fre_gap = val;
    pr_info("hapstream_fre_gap = %d\n",fs3002->dts_info.fs3002_hapstream_fre_gap);

	return count;
}

static ssize_t fs3002_hapstream_trans_cycles_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	ssize_t len = 0;
	pr_info("enter\n");

	len += snprintf(buf + len, PAGE_SIZE - len,"hapstream_trans_cycles = %d\n",fs3002->dts_info.fs3002_hapstream_trans_cycles);
	return len;
}

static ssize_t fs3002_hapstream_trans_cycles_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	unsigned int val = 0;
	int rc = 0;

	pr_info("enter\n");

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
	{
		pr_err("%s:input value out of range\n", FSERROR);
		return rc;
	}

    fs3002->dts_info.fs3002_hapstream_trans_cycles = val;
    pr_info("fs3002_hapstream_trans_cycles = %d\n",fs3002->dts_info.fs3002_hapstream_trans_cycles);

	return count;
}




//f0
static DEVICE_ATTR(f0, 0644, fs3002_f0_show, NULL);
static DEVICE_ATTR(f0_ref, 0644, fs3002_f0_ref_show, fs3002_f0_ref_store);
static DEVICE_ATTR(f0_value, 0644, fs3002_f0_value_show, NULL);
static DEVICE_ATTR(f0_save, 0644, fs3002_f0_save_show, fs3002_f0_save_store);
static DEVICE_ATTR(f0_check, 0644, fs3002_f0_check_show, NULL);
//cali
static DEVICE_ATTR(cali, 0644, fs3002_cali_show, fs3002_cali_store);
static DEVICE_ATTR(osc_save, 0644, fs3002_osc_save_show, fs3002_osc_save_store);
static DEVICE_ATTR(osc_cali, 0644, fs3002_osc_cali_show, fs3002_osc_cali_store);
//reg
static DEVICE_ATTR(reg_inits, 0644, fs3002_reg_inits_show, NULL);
static DEVICE_ATTR(reg, 0644, fs3002_reg_show, fs3002_reg_store);
static DEVICE_ATTR(registers, 0644, NULL, fs3002_regs_store);
//cont play
static DEVICE_ATTR(cont_drv_lvl, 0644, fs3002_cont_drv_lvl_show, fs3002_cont_drv_lvl_store);
static DEVICE_ATTR(cont_drv_time, 0644, fs3002_cont_drv_time_show,fs3002_cont_drv_time_store);
static DEVICE_ATTR(cont, 0644, fs3002_cont_show, fs3002_cont_store);
//ram play
static DEVICE_ATTR(ram_num, 0644, fs3002_ram_num_show, NULL);
static DEVICE_ATTR(ram_update, 0644, fs3002_ram_update_show, fs3002_ram_update_store);
static DEVICE_ATTR(ram_max, 0644, fs3002_ram_max_show, fs3002_ram_max_store);
static DEVICE_ATTR(index, 0644, fs3002_index_show,fs3002_index_store);
static DEVICE_ATTR(seq, 0644, fs3002_seq_show, fs3002_seq_store);
static DEVICE_ATTR(loop, 0644, fs3002_loop_show,fs3002_loop_store);
static DEVICE_ATTR(duration, 0644, fs3002_duration_show,fs3002_duration_store);
//rtp
static DEVICE_ATTR(effect_id, 0644, fs3002_effect_id_show, fs3002_effect_id_store);
static DEVICE_ATTR(rtp, 0644, fs3002_rtp_show, fs3002_rtp_store);
static DEVICE_ATTR(buf_size, 0644, fs3002_buf_size_show, fs3002_buf_size_store);
static DEVICE_ATTR(rtp_auto_env, 0644, fs3002_rtp_auto_env_show, fs3002_rtp_auto_env_store);
static DEVICE_ATTR(rtp_auto_env_value, 0644, fs3002_rtp_auto_env_value_show, fs3002_rtp_auto_env_value_store);
static DEVICE_ATTR(rtp_auto_size, 0644, fs3002_rtp_auto_size_show, fs3002_rtp_auto_size_store);
static DEVICE_ATTR(custom_wave, 0644, fs3002_custom_wave_show, fs3002_custom_wave_store);
static DEVICE_ATTR(custom_is, 0644, fs3002_custom_is_show, fs3002_custom_is_store);
static DEVICE_ATTR(print_rb, 0644, fs3002_print_rb_show, NULL);
static DEVICE_ATTR(rtp_bst_sel, 0644, fs3002_rtp_bst_sel_show, fs3002_rtp_bst_sel_store);
//trig
static DEVICE_ATTR(trig, 0644, fs3002_trig_show, fs3002_trig_store);
//play
static DEVICE_ATTR(state, 0644, fs3002_state_show,NULL);
static DEVICE_ATTR(activate_mode, 0644, fs3002_activate_mode_show,fs3002_activate_mode_store);
static DEVICE_ATTR(activate, 0644, fs3002_activate_show,fs3002_activate_store);
//brake
static DEVICE_ATTR(brk_times, 0644, fs3002_brk_times_show, fs3002_brk_times_store);
static DEVICE_ATTR(auto_brake, 0644, fs3002_auto_brake_show, fs3002_auto_brake_store);
//gain
static DEVICE_ATTR(bypass_system_gain, 0644, fs3002_bypass_system_gain_show,fs3002_bypass_system_gain_store);
static DEVICE_ATTR(gain, 0644, fs3002_gain_show,fs3002_gain_store);
//boost
static DEVICE_ATTR(boost_mode, 0644, fs3002_boost_mode_show, fs3002_boost_mode_store);
static DEVICE_ATTR(vref, 0644, fs3002_vref_show, fs3002_vref_store);
static DEVICE_ATTR(env_lowv, 0644, fs3002_env_lowv_show, fs3002_env_lowv_store);
//hapstream
static DEVICE_ATTR(fre_gap, 0644, fs3002_hapstream_fre_gap_show, fs3002_hapstream_fre_gap_store);
static DEVICE_ATTR(trans_cycles, 0644, fs3002_hapstream_trans_cycles_show, fs3002_hapstream_trans_cycles_store);
//other
static DEVICE_ATTR(debug_enable, 0644, fs3002_debug_enable_show, fs3002_debug_enable_store);
static DEVICE_ATTR(version, 0644, fs3002_version_show, NULL);
static DEVICE_ATTR(Qos_time, 0644, fs3002_Qos_time_show, fs3002_Qos_time_store);
static DEVICE_ATTR(lra_time, 0644, fs3002_lra_time_show, fs3002_lra_time_store);
//other action
static DEVICE_ATTR(reset, 0644, NULL, fs3002_reset_store);
static DEVICE_ATTR(vbat_monitor, 0644, fs3002_vbat_monitor_show, NULL);
static DEVICE_ATTR(lra_resistance, 0644, fs3002_lra_resistance_show, NULL);
static DEVICE_ATTR(sample_rate, 0644, fs3002_sample_rate_show,fs3002_sample_rate_store);


static struct attribute *fs3002_vibrator_attributes[] = 
{
	//f0
	&dev_attr_f0.attr,
	&dev_attr_f0_ref.attr,
	&dev_attr_f0_value.attr,
	&dev_attr_f0_save.attr,
	&dev_attr_f0_check.attr,
	//cali
	&dev_attr_cali.attr,
	&dev_attr_osc_save.attr,
	&dev_attr_osc_cali.attr,	
	//reg
	&dev_attr_reg_inits.attr,
	&dev_attr_reg.attr,
	&dev_attr_registers.attr,
	//cont play
	&dev_attr_cont_drv_lvl.attr,
	&dev_attr_cont_drv_time.attr,
	&dev_attr_cont.attr,
	//ram play
	&dev_attr_ram_num.attr,	
	&dev_attr_ram_update.attr,
	&dev_attr_ram_max.attr,
	&dev_attr_index.attr,
	&dev_attr_seq.attr,
	&dev_attr_loop.attr,
	&dev_attr_duration.attr,
	//rtp
	&dev_attr_effect_id.attr,
	&dev_attr_rtp.attr,
	&dev_attr_buf_size.attr,
	&dev_attr_rtp_auto_env.attr,
	&dev_attr_rtp_auto_env_value.attr,
	&dev_attr_rtp_auto_size.attr,
	&dev_attr_custom_wave.attr,
	&dev_attr_custom_is.attr,
	&dev_attr_print_rb.attr,
	&dev_attr_rtp_bst_sel.attr,
	//trig
	&dev_attr_trig.attr,
	//play
	&dev_attr_state.attr,
	&dev_attr_activate_mode.attr,
	&dev_attr_activate.attr,
	//brake
	&dev_attr_brk_times.attr,
	&dev_attr_auto_brake.attr,
	//gain
	&dev_attr_gain.attr,
	&dev_attr_bypass_system_gain.attr,
	//boost
	&dev_attr_boost_mode.attr,
	&dev_attr_vref.attr,
	&dev_attr_env_lowv.attr,
	//hapstream
	&dev_attr_fre_gap.attr,
	&dev_attr_trans_cycles.attr,
	//other
	&dev_attr_debug_enable.attr,
	&dev_attr_version.attr,
	&dev_attr_Qos_time.attr,
	&dev_attr_lra_time.attr,
	//other action
	&dev_attr_reset.attr,
	&dev_attr_vbat_monitor.attr,
	&dev_attr_lra_resistance.attr,
	&dev_attr_sample_rate.attr,
	NULL
};

struct attribute_group fs3002_vibrator_attribute_group = 
{
	.attrs = fs3002_vibrator_attributes
};

void fs3002_vibrate_params_init(struct fs3002 *fs3002)
{
	unsigned char i = 0;
	unsigned char reg_val = 0;

	pr_info("enter!\n");
	
	fs3002->rtp_routine_on = 0;
	fs3002->rtp_num_max = fs3002_rtp_name_len;

	// set Waveform select to be square
	fs3002_i2c_write(fs3002, FS3002_BWAVCTRL, FS3002_BWAVCTRL_B1_0_SQUARE);

	
	fs3002->activate_mode = fs3002->dts_info.fs3002_default_vib_mode;
	fs3002_i2c_read(fs3002, FS3002_WFSCFG1, &reg_val);
	fs3002->index = reg_val & 0x7F;
	fs3002_i2c_read(fs3002, FS3002_GAINCFG, &reg_val);
	fs3002->gain = reg_val & 0xFF;
	pr_info("fs3002->gain =0x%02X, fs3002->index =0x%02X\n", fs3002->gain, fs3002->index);

	for (i = 0; i < FS3002_SEQUENCER_SIZE; i++) 
	{
		fs3002_i2c_read_bits(fs3002, FS3002_WFSCFG1 + i, 6, 0);
		fs3002->seq[i] = reg_val;
	}

	fs3002->level = 0x80;
	fs3002->f0_cali_data = 0x90;
	fs3002->osc_cali_data= 0x90;
	fs3002->offset = 512;
	
	//enable trig
	fs3002_i2c_write_bits(fs3002, FS3002_TRGCTRL,1,7,7);

	fs3002->buf_size = 384;
	fs3002->Qos_time = 0;
	fs3002->lra_test_sleep_time = 1000;

	//enable lbm
	fs3002_i2c_write_bits(fs3002, FS3002_VBATLOWCTRL1,1,7,7);

	//init cali status true 
	fs3002->f0_cali_status = true;
}

//okok
int fs3002_haptic_init(struct fs3002 *fs3002)
{
	pr_info("%s enter\n", __func__);

	mutex_lock(&fs3002->lock);
	fs3002_hack_init(fs3002);
	fs3002_haptic_audio_init(fs3002);
	fs3002_vibrate_params_init(fs3002);
	fs3002_haptic_set_mode(fs3002, FS3002_HAPTIC_STANDBY_MODE);
	fs3002_haptic_set_pwm(fs3002, fs3002->dts_info.fs3002_play_ram_srate);
	fs3002_haptic_set_vref(fs3002);
	fs3002_haptic_set_trig_vref(fs3002);	
	fs3002_haptic_swicth_motor_protect_config(fs3002, true);

	//offset_calibration
	fs3002_haptic_offset_calibration(fs3002);
	
	//config auto_brake
	fs3002_haptic_auto_break_mode(fs3002, fs3002->dts_info.fs3002_auto_brake);
	
	//config rtpstartl
	fs3002_haptic_set_rtpstartl(fs3002, FS3002_RTPSTART_SIZE);

	//config ilmt
	fs3002_haptic_set_ilmt(fs3002, fs3002->dts_info.fs3002_ilmt);
	
	//f0 calibration
	if (fs3002->dts_info.fs3002_f0_cali_data_mode == FS3002_F0_CALI_DATA_SELF_MODE)
	{
		fs3002_haptic_f0_calibration(fs3002);
	}
/*zzzz
	else if(fs3002->dts_info.fs3002_f0_cali_data_mode == FS3002_F0_CALI_DATA_CMDLINE_MODE)
	{
		ptr = strstr(saved_command_line, "fs3002_lk_f0_cali=");
		if (ptr != NULL) 
		{
			ptr += strlen("fs3002_lk_f0_cali=");
			fs3002->f0_cali_data = simple_strtol(ptr, NULL, 0);
			pr_info("fs3002->f0_cali_data = 0x%x\n", fs3002->f0_cali_data);
		}
		else
		{
			pr_err("%s: FS3002_F0_CALI_DATA_CMDLINE_MODE fail\n",FSERROR);
			fs3002_haptic_f0_calibration(fs3002);
		}
	}
	*/
	else if(fs3002->dts_info.fs3002_f0_cali_data_mode == FS3002_F0_CALI_DATA_DTS_MODE)
	{
		if (fs3002->dts_info.fs3002_lk_f0_cali != 0) 
		{
			fs3002->f0_cali_data = fs3002->dts_info.fs3002_lk_f0_cali;
			pr_info("fs3002->f0_cali_data = 0x%x\n", fs3002->f0_cali_data);
		}
		else
		{
			pr_err("%s: FS3002_F0_CALI_DATA_DTS_MODE fail\n",FSERROR);
			fs3002_haptic_f0_calibration(fs3002);
		}
	}
	else
	{
		pr_err("%s: fs3002->dts_info.fs3002_f0_cali_data_mode=%d\n",FSERROR, fs3002->dts_info.fs3002_f0_cali_data_mode);
		fs3002_haptic_f0_calibration(fs3002);
	}

	mutex_unlock(&fs3002->lock);
	return 0;
}


int fs3002_parse_dt(struct fs3002 *fs3002, struct device *dev, struct device_node *np)
{
	unsigned int val = 0;
	unsigned int fs3002_trig_config[24] = 
	{
		1, 0, 1, 1, 1, 2, 0, 0,
		1, 0, 0, 1, 0, 2, 0, 0,
		1, 0, 0, 1, 0, 2, 0, 0
	};
	
	int i = 0,tmp = 0,rc = 0,j=0;
	//This value is currently set to 200, which is greater than the number of elements in the fs3002_rtp_name array in dts. 
	//These two arrays are also written first. 
    //fs3002_rtp_max=< 200 >;
    //fs3002_rtp_time=< 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 >;
    
	unsigned int rtp_time[175];
	unsigned int reg_inits[10];
	struct qti_hap_config *config = &fs3002->config;
	struct device_node *child_node;
	struct qti_hap_effect *effect;

	//fs3002_f0_ref
	val = of_property_read_u32(np, "fs3002_f0_ref", &fs3002->dts_info.fs3002_f0_ref);
	if (val != 0)
	{
		pr_err("%s:fs3002_f0_ref not found\n", FSERROR);
		fs3002->dts_info.fs3002_f0_ref = FS3002_DEFAULT_F0_REF;
	}
	else
	{
		sprintf(str, "%s, fs3002_f0_ref=%d\n", __func__,fs3002->dts_info.fs3002_f0_ref);
		fs3002_debug_message(fs3002, str);
	}

	//fs3002_auto_brake
	val = of_property_read_u32(np,"fs3002_auto_brake",&fs3002->dts_info.fs3002_auto_brake);
	if (val != 0)
	{
		pr_err("%s:fs3002_auto_brake not found\n", FSERROR);
		fs3002->dts_info.fs3002_auto_brake = FS3002_AUTO_BRAKE_DISABLE;
	}		
	else
	{
		sprintf(str, "%s, fs3002_auto_brake=%d\n", __func__,fs3002->dts_info.fs3002_auto_brake);
		fs3002_debug_message(fs3002, str);
	}

	//fs3002_f0_cali_mode
	val = of_property_read_u32(np,"fs3002_f0_cali_mode",&fs3002->dts_info.fs3002_f0_cali_mode);
	if (val != 0)
	{
		pr_err("%s:fs3002_f0_cali_mode not found\n", FSERROR);
		fs3002->dts_info.fs3002_f0_cali_mode = FS3002_F0_CALI_MODE_AUTO;
	}		
	else
	{
		sprintf(str, "%s, fs3002_f0_cali_mode=%d\n", __func__,fs3002->dts_info.fs3002_f0_cali_mode);
		fs3002_debug_message(fs3002, str);
	}
	
	//fs3002_cont_drv1_lvl
	val = of_property_read_u32(np, "fs3002_cont_drv1_lvl", &fs3002->dts_info.fs3002_cont_drv1_lvl);
	if (val != 0)
	{
		pr_err("%s:fs3002_cont_drv1_lvl not found\n", FSERROR);
		fs3002->dts_info.fs3002_cont_drv1_lvl = FS3002_DEFAULT_CONT_DRV1_LVL;
	}
	else
	{
		sprintf(str, "%s, fs3002_cont_drv1_lvl=%d\n", __func__,fs3002->dts_info.fs3002_cont_drv1_lvl);
		fs3002_debug_message(fs3002, str);
	}
	
	//fs3002_cont_drv2_lvl
	val = of_property_read_u32(np, "fs3002_cont_drv2_lvl", &fs3002->dts_info.fs3002_cont_drv2_lvl);
	if (val != 0)
	{
		pr_err("%s:fs3002_cont_drv2_lvl not found\n", FSERROR);
		fs3002->dts_info.fs3002_cont_drv2_lvl = FS3002_DEFAULT_CONT_DRV2_LVL;
	}
	else
	{
		sprintf(str, "%s, fs3002_cont_drv2_lvl=%d\n", __func__,fs3002->dts_info.fs3002_cont_drv2_lvl);
		fs3002_debug_message(fs3002, str);
	}
	
	//fs3002_cont_drv1_time
	val = of_property_read_u32(np, "fs3002_cont_drv1_time", &fs3002->dts_info.fs3002_cont_drv1_time);
	if (val != 0)
	{
		pr_err("%s:fs3002_cont_drv1_time not found\n", FSERROR);
		fs3002->dts_info.fs3002_cont_drv1_time = FS3002_DEFAULT_CONT_DRV1_TIME;
	}
	else
	{
		sprintf(str, "%s, fs3002_cont_drv1_time=%d\n", __func__,fs3002->dts_info.fs3002_cont_drv1_time);
		fs3002_debug_message(fs3002, str);
	}
	
	//fs3002_cont_drv2_time
	val = of_property_read_u32(np, "fs3002_cont_drv2_time", &fs3002->dts_info.fs3002_cont_drv2_time);
	if (val != 0)
	{
		pr_err("%s:fs3002_cont_drv2_time not found\n", FSERROR);
		fs3002->dts_info.fs3002_cont_drv2_time = FS3002_DEFAULT_CONT_DRV2_TIME;
	}
	else
	{
		sprintf(str, "%s, fs3002_cont_drv2_time=%d\n", __func__,fs3002->dts_info.fs3002_cont_drv2_time);
		fs3002_debug_message(fs3002, str);
	}

	//fs3002_cont_1_period
	val = of_property_read_u32(np,"fs3002_cont_1_period",&fs3002->dts_info.fs3002_cont_1_period);
	if (val != 0)
	{
		pr_err("%s:fs3002_cont_1_period not found\n", FSERROR);
		fs3002->dts_info.fs3002_cont_1_period = FS3002_DEFAULT_CONT_1_PERIOD;
	}
	else
	{
		sprintf(str, "%s, fs3002_cont_1_period=%d\n", __func__,fs3002->dts_info.fs3002_cont_1_period);
		fs3002_debug_message(fs3002, str);
	}		

	//fs3002_brk_slopeth
	val = of_property_read_u32(np,"fs3002_brk_slopeth",&fs3002->dts_info.fs3002_brk_slopeth);
	if (val != 0)
	{
		pr_err("%s:fs3002_brk_slopeth not found\n", FSERROR);
		fs3002->dts_info.fs3002_brk_slopeth = FS3002_DEFAULT_BRK_SLOPETH;
	}
	else
	{
		sprintf(str, "%s, fs3002_brk_slopeth=%d\n", __func__,fs3002->dts_info.fs3002_brk_slopeth);
		fs3002_debug_message(fs3002, str);
	}			

	//fs3002_brk_gain
	val = of_property_read_u32(np, "fs3002_brk_gain", &fs3002->dts_info.fs3002_brk_gain);
	if (val != 0)
	{
		pr_err("%s:fs3002_brk_gain not found\n", FSERROR);
		fs3002->dts_info.fs3002_brk_gain = FS3002_DEFAULT_BRK_GAIN;
	}
	else
	{
		sprintf(str, "%s, fs3002_brk_gain = %d\n", __func__,fs3002->dts_info.fs3002_brk_gain);
		fs3002_debug_message(fs3002, str);
	}	

	//fs3002_brk_times
	val = of_property_read_u32(np, "fs3002_brk_times", &fs3002->dts_info.fs3002_brk_times);
	if (val != 0)
	{
		pr_err("%s:fs3002_brk_times not found\n", FSERROR);
		fs3002->dts_info.fs3002_brk_times = FS3002_DEFAULT_BRK_TIMES;
	}
	else
	{
		sprintf(str, "%s, fs3002_brk_times=%d\n", __func__,fs3002->dts_info.fs3002_brk_times);
		fs3002_debug_message(fs3002, str);
	}

	//fs3002_brk_noise_gate
	val = of_property_read_u32(np, "fs3002_brk_noise_gate",&fs3002->dts_info.fs3002_brk_noise_gate);
	if (val != 0)
	{
		pr_err("%s:fs3002_brk_noise_gate not found\n", FSERROR);
		fs3002->dts_info.fs3002_brk_noise_gate = FS3002_DEFAULT_BRK_TIMES;
	}		
	else
	{
		sprintf(str, "%s, fs3002_brk_noise_gate = %d\n", __func__,fs3002->dts_info.fs3002_brk_noise_gate);
		fs3002_debug_message(fs3002, str);
	}		

	//fs3002_brk_1_period
	val = of_property_read_u32(np, "fs3002_brk_1_period",&fs3002->dts_info.fs3002_brk_1_period);
	if (val != 0)
	{
		pr_err("%s:fs3002_brk_1_period not found\n", FSERROR);
		fs3002->dts_info.fs3002_brk_1_period = FS3002_DEFAULT_BRK_1_PERIOD;
	}		
	else
	{
		sprintf(str, "%s, fs3002_brk_1_period = %d\n", __func__,fs3002->dts_info.fs3002_brk_1_period);
		fs3002_debug_message(fs3002, str);
	}			

	//fs3002_brk_pgagain
	val =of_property_read_u32(np, "fs3002_brk_pgagain",&fs3002->dts_info.fs3002_brk_pgagain);
	if (val != 0)
	{
		pr_err("%s:fs3002_brk_pgagain not found\n", FSERROR);
		fs3002->dts_info.fs3002_brk_pgagain = FS3002_DEFAULT_BRK_PGAGAIN;
	}	
	else
	{
		sprintf(str, "%s, fs3002_brk_pgagain = %d\n", __func__,fs3002->dts_info.fs3002_brk_pgagain);
		fs3002_debug_message(fs3002, str);
	}	

	//fs3002_brk_coefp
	val = of_property_read_u32(np, "fs3002_brk_coefp",&fs3002->dts_info.fs3002_brk_coefp);
	if (val != 0)
	{
		pr_err("%s:fs3002_brk_coefp not found\n", FSERROR);
		fs3002->dts_info.fs3002_brk_coefp = FS3002_DEFAULT_BRK_COEFP;
	}		
	else
	{
		sprintf(str, "%s, fs3002_brk_coefp = %d\n", __func__,fs3002->dts_info.fs3002_brk_coefp);
		fs3002_debug_message(fs3002, str);
	}	

	//fs3002_brk_margin
	val =of_property_read_u32(np, "fs3002_brk_margin",&fs3002->dts_info.fs3002_brk_margin);
	if (val != 0)
	{
		pr_err("%s:fs3002_brk_margin not found\n", FSERROR);
		fs3002->dts_info.fs3002_brk_margin = FS3002_DEFAULT_BRK_MARGIN;
	}	
	else
	{
		sprintf(str, "%s, fs3002_brk_margin = %d\n", __func__,fs3002->dts_info.fs3002_brk_margin);
		fs3002_debug_message(fs3002, str);
	}

	//fs3002_play_ram_srate
	val =of_property_read_u32(np, "fs3002_play_ram_srate",&fs3002->dts_info.fs3002_play_ram_srate);
	if (val != 0)
	{
		pr_err("%s:fs3002_play_ram_srate not found\n", FSERROR);
		fs3002->dts_info.fs3002_play_ram_srate = FS3002_DEFAULT_PLAY_RAM_SRATE;
	}	
	else
	{
		sprintf(str, "%s, fs3002_play_ram_srate = %d\n", __func__,fs3002->dts_info.fs3002_play_ram_srate);
		fs3002_debug_message(fs3002, str);
	}		

	
	//fs3002_play_rtp_srate
	val =of_property_read_u32(np, "fs3002_play_rtp_srate",&fs3002->dts_info.fs3002_play_rtp_srate);
	if (val != 0)
	{
		pr_err("%s:fs3002_play_rtp_srate not found\n", FSERROR);
		fs3002->dts_info.fs3002_play_rtp_srate = FS3002_DEFAULT_PLAY_RTP_SRATE;
	}	
	else
	{
		sprintf(str, "%s,fs3002_play_rtp_srate = %d\n", __func__,fs3002->dts_info.fs3002_play_rtp_srate);
		fs3002_debug_message(fs3002, str);
	}	

	//	fs3002_default_vib_mode
	val = of_property_read_u32(np,"fs3002_default_vib_mode",&fs3002->dts_info.fs3002_default_vib_mode);
	if (val != 0)
	{
		pr_err("%s:fs3002_default_vib_mode not found\n", FSERROR);
		fs3002->dts_info.fs3002_default_vib_mode = FS3002_DEFAULT_VIB_MODE;
	}
	else
	{
		sprintf(str, "%s,fs3002_default_vib_mode=%d\n", __func__,fs3002->dts_info.fs3002_default_vib_mode);
		fs3002_debug_message(fs3002, str);
	}	
	
	//	fs3002_lr_pgagain
	val = of_property_read_u32(np,"fs3002_lr_pgagain",&fs3002->dts_info.fs3002_lr_pgagain);
	if (val != 0)
	{
		pr_err("%s:fs3002_lr_pgagain not found\n", FSERROR);
		fs3002->dts_info.fs3002_lr_pgagain = FS3002_DEFAULT_LR_PGAGAIN;
	}
	else
	{
		sprintf(str, "%s,fs3002_lr_pgagain=%d\n", __func__,fs3002->dts_info.fs3002_lr_pgagain);
		fs3002_debug_message(fs3002, str);
	}		

	//fs3002_reg_inits = < 0x5f0001 0x5f3411 0x5f7080 >;
	val = of_property_read_u32_array(np, "fs3002_reg_inits",reg_inits,ARRAY_SIZE(reg_inits));
	if (val != 0)
	{
		pr_err("%s:fs3002_reg_inits not found\n", FSERROR);
		for (i = 0;i < ARRAY_SIZE(reg_inits);i++)
		{
			reg_inits[i] = FS3002_DEFAULT_REG_INITS;
		}
	}
	else
	{
		for (i = 0;i < ARRAY_SIZE(reg_inits);i++)
		{
			if(reg_inits[i] != FS3002_DEFAULT_REG_INITS)
			{
				sprintf(str, "%s,reg_inits[%d]=0x%x\n", __func__,i,reg_inits[i]);
				fs3002_debug_message(fs3002, str);			
			}
		}
	}
	memcpy(fs3002->dts_info.fs3002_reg_inits, reg_inits,sizeof(reg_inits));

	//	fs3002_rtp_auto_env
	val = of_property_read_u32(np, "fs3002_rtp_auto_env",&fs3002->dts_info.fs3002_rtp_auto_env);
	if (val != 0)
	{
		pr_err("%s:fs3002_rtp_auto_env not found\n", FSERROR);
		fs3002->dts_info.fs3002_rtp_auto_env = FS3002_DEFAULT_RTP_AUTO_ENV;
	}		
	else
	{
		sprintf(str, "%s,fs3002_rtp_auto_env = %d\n", __func__,fs3002->dts_info.fs3002_rtp_auto_env);
		fs3002_debug_message(fs3002, str);
	}

	//	fs3002_rtp_auto_env_mid
	val = of_property_read_u32(np, "fs3002_rtp_auto_env_mid",&fs3002->dts_info.fs3002_rtp_auto_env_mid);
	if (val != 0)
	{
		pr_err("%s:fs3002_rtp_auto_env_mid not found\n", FSERROR);
		fs3002->dts_info.fs3002_rtp_auto_env_mid = FS3002_DEFAULT_RTP_AUTO_ENV_MID;
	}		
	else
	{
		sprintf(str, "%s,fs3002_rtp_auto_env_mid = %d\n", __func__,fs3002->dts_info.fs3002_rtp_auto_env_mid);
		fs3002_debug_message(fs3002, str);
	}

	//	fs3002_rtp_auto_env_high
	val = of_property_read_u32(np, "fs3002_rtp_auto_env_high",&fs3002->dts_info.fs3002_rtp_auto_env_high);
	if (val != 0)
	{
		pr_err("%s:fs3002_rtp_auto_env_high not found\n", FSERROR);
		fs3002->dts_info.fs3002_rtp_auto_env_high = FS3002_DEFAULT_RTP_AUTO_ENV_HIGH;
	}		
	else
	{
		sprintf(str, "%s,fs3002_rtp_auto_env_high = %d\n", __func__,fs3002->dts_info.fs3002_rtp_auto_env_high);
		fs3002_debug_message(fs3002, str);
	}

	//	fs3002_rtp_auto_size
	val = of_property_read_u32(np, "fs3002_rtp_auto_size",&fs3002->dts_info.fs3002_rtp_auto_size);
	if (val != 0)
	{
		pr_err("%s:fs3002_rtp_auto_size not found\n", FSERROR);
		fs3002->dts_info.fs3002_rtp_auto_size = FS3002_DEFAULT_RTP_AUTO_SIZE;
	}		
	else
	{
		sprintf(str, "%s,fs3002_rtp_auto_size = %d\n", __func__,fs3002->dts_info.fs3002_rtp_auto_size);
		fs3002_debug_message(fs3002, str);
	}

	//	fs3002_ram_id_boundary
	val = of_property_read_u32(np, "fs3002_ram_id_boundary",&fs3002->dts_info.fs3002_ram_id_boundary);
	if (val != 0)
	{
		pr_err("%s:fs3002_ram_id_boundary not found\n", FSERROR);
		fs3002->dts_info.fs3002_ram_id_boundary = FS3002_DEFAULT_RAM_ID_BOUNDARY;
	}
	else
	{
		sprintf(str, "%s,fs3002_ram_id_boundary = %d\n", __func__,fs3002->dts_info.fs3002_ram_id_boundary);
		fs3002_debug_message(fs3002, str);
	}	

	//	fs3002_rtp_max
	val = of_property_read_u32(np, "fs3002_rtp_max",&fs3002->dts_info.fs3002_rtp_max);
	if (val != 0)
	{
		pr_err("%s:fs3002_rtp_max not found\n", FSERROR);
		fs3002->dts_info.fs3002_rtp_max = FS3002_DEFAULT_RTP_MAX;
	}		
	else
	{
		sprintf(str, "%s,fs3002_rtp_max = %d\n", __func__,fs3002->dts_info.fs3002_rtp_max);
		fs3002_debug_message(fs3002, str);
	}		

	//fs3002_rtp_time
	val = of_property_read_u32_array(np, "fs3002_rtp_time", rtp_time,ARRAY_SIZE(rtp_time));
	//val = of_property_read_u32_array(np, "fs3002_trig_config", fs3002_trig_config,ARRAY_SIZE(fs3002_trig_config));
	if (val != 0)
	{
		pr_err("%s:fs3002_rtp_time not found\n", FSERROR);
		for (i = 0;i < ARRAY_SIZE(rtp_time);i++)
		{
			rtp_time[i] = FS3002_DEFAULT_RTP_TIME;
		}
	}		
	else
	{
		sprintf(str, "%s,fs3002_rtp_time number = %d\n", __func__,(int)ARRAY_SIZE(rtp_time));
		fs3002_debug_message(fs3002, str);
	}
	memcpy(fs3002->dts_info.fs3002_rtp_time, rtp_time, sizeof(rtp_time));

	//fs3002_trig_config
	val = of_property_read_u32_array(np, "fs3002_trig_config", fs3002_trig_config,ARRAY_SIZE(fs3002_trig_config));
	if (val != 0)
	{
		pr_err("%s:fs3002_trig_config not found\n", FSERROR);
		for (i = 0;i < ARRAY_SIZE(fs3002_trig_config);i++)
		{
			fs3002_trig_config[i] = FS3002_DEFAULT_TRIG_CONFIG;
		}
	}		
	else
	{
		sprintf(str, "%s,fs3002_trig_config number = %d\n", __func__,(int)ARRAY_SIZE(fs3002_trig_config));
		fs3002_debug_message(fs3002, str);
	}
	memcpy(fs3002->dts_info.fs3002_trig_config, fs3002_trig_config, sizeof(fs3002_trig_config));

	//	fs3002_env_lowv
	val = of_property_read_u32(np, "fs3002_env_lowv",&fs3002->dts_info.fs3002_env_lowv);
	if (val != 0)
	{
		pr_err("%s:fs3002_env_lowv not found\n", FSERROR);
		fs3002->dts_info.fs3002_env_lowv = FS3002_DEFAULT_ENV_LOWV;
	}		
	else
	{
		sprintf(str, "%s,fs3002_env_lowv = %d\n", __func__,fs3002->dts_info.fs3002_env_lowv);
		fs3002_debug_message(fs3002, str);
	}

	//	fs3002_lowv_ref
	val = of_property_read_u32(np, "fs3002_lowv_ref",&fs3002->dts_info.fs3002_lowv_ref);
	if (val != 0)
	{
		pr_err("%s:fs3002_lowv_ref not found\n", FSERROR);
		fs3002->dts_info.fs3002_lowv_ref = FS3002_DEFAULT_LOWV_REF;
	}		
	else
	{
		sprintf(str, "%s,fs3002_lowv_ref = %d\n", __func__,fs3002->dts_info.fs3002_lowv_ref);
		fs3002_debug_message(fs3002, str);
	}
	
	//	fs3002_midv_ref
	val = of_property_read_u32(np, "fs3002_midv_ref",&fs3002->dts_info.fs3002_midv_ref);
	if (val != 0)
	{
		pr_err("%s:fs3002_midv_ref not found\n", FSERROR);
		fs3002->dts_info.fs3002_midv_ref = FS3002_DEFAULT_MIDV_REF;
	}		
	else
	{
		sprintf(str, "%s,fs3002_midv_ref = %d\n", __func__,fs3002->dts_info.fs3002_midv_ref);
		fs3002_debug_message(fs3002, str);
	}

	//	fs3002_highv_ref
	val = of_property_read_u32(np, "fs3002_highv_ref",&fs3002->dts_info.fs3002_highv_ref);
	if (val != 0)
	{
		pr_err("%s:fs3002_highv_ref not found\n", FSERROR);
		fs3002->dts_info.fs3002_highv_ref = FS3002_DEFAULT_HIGHV_REF;
	}		
	else
	{
		sprintf(str, "%s,fs3002_highv_ref = %d\n", __func__,fs3002->dts_info.fs3002_highv_ref);
		fs3002_debug_message(fs3002, str);
	}

	//	fs3002_rtp_bst_sel
	val = of_property_read_u32(np, "fs3002_rtp_bst_sel",&fs3002->dts_info.fs3002_rtp_bst_sel);
	if (val != 0)
	{
		pr_err("%s:fs3002_rtp_bst_sel not found\n", FSERROR);
		fs3002->dts_info.fs3002_rtp_bst_sel = FS3002_DEFAULT_RTP_BST_SEL;
	}		
	else
	{
		sprintf(str, "%s,fs3002_rtp_bst_sel = %d\n", __func__,fs3002->dts_info.fs3002_rtp_bst_sel);
		fs3002_debug_message(fs3002, str);
	}

    //	fs3002_ilmt
	val = of_property_read_u32(np, "fs3002_ilmt",&fs3002->dts_info.fs3002_ilmt);
	if (val != 0)
	{
		pr_err("%s:fs3002_ilmt not found\n", FSERROR);
		fs3002->dts_info.fs3002_ilmt = FS3002_DEFAULT_ILMT;
	}		
	else
	{
		sprintf(str, "%s,fs3002_ilmt = %d\n", __func__,fs3002->dts_info.fs3002_ilmt);
		fs3002_debug_message(fs3002, str);
	}

	//	fs3002_trig1_p_ref
	val = of_property_read_u32(np, "fs3002_trig1_p_ref",&fs3002->dts_info.fs3002_trig1_p_ref);
	if (val != 0)
	{
		pr_err("%s:fs3002_trig1_p_ref not found\n", FSERROR);
		fs3002->dts_info.fs3002_trig1_p_ref = FS3002_DEFAULT_TRIG_REF;
	}		
	else
	{
		sprintf(str, "%s,fs3002_trig1_p_ref = %d\n", __func__,fs3002->dts_info.fs3002_trig1_p_ref);
		fs3002_debug_message(fs3002, str);
	}

	//	fs3002_trig1_n_ref
	val = of_property_read_u32(np, "fs3002_trig1_n_ref",&fs3002->dts_info.fs3002_trig1_n_ref);
	if (val != 0)
	{
		pr_err("%s:fs3002_trig1_n_ref not found\n", FSERROR);
		fs3002->dts_info.fs3002_trig1_n_ref = FS3002_DEFAULT_TRIG_REF;
	}		
	else
	{
		sprintf(str, "%s,fs3002_trig1_n_ref = %d\n", __func__,fs3002->dts_info.fs3002_trig1_n_ref);
		fs3002_debug_message(fs3002, str);
	}

	//	fs3002_trig2_p_ref
	val = of_property_read_u32(np, "fs3002_trig2_p_ref",&fs3002->dts_info.fs3002_trig2_p_ref);
	if (val != 0)
	{
		pr_err("%s:fs3002_trig2_p_ref not found\n", FSERROR);
		fs3002->dts_info.fs3002_trig2_p_ref = FS3002_DEFAULT_TRIG_REF;
	}		
	else
	{
		sprintf(str, "%s,fs3002_trig2_p_ref = %d\n", __func__,fs3002->dts_info.fs3002_trig2_p_ref);
		fs3002_debug_message(fs3002, str);
	}

	//	fs3002_trig2_n_ref
	val = of_property_read_u32(np, "fs3002_trig2_n_ref",&fs3002->dts_info.fs3002_trig2_n_ref);
	if (val != 0)
	{
		pr_err("%s:fs3002_trig2_n_ref not found\n", FSERROR);
		fs3002->dts_info.fs3002_trig2_n_ref = FS3002_DEFAULT_TRIG_REF;
	}		
	else
	{
		sprintf(str, "%s,fs3002_trig2_n_ref = %d\n", __func__,fs3002->dts_info.fs3002_trig2_n_ref);
		fs3002_debug_message(fs3002, str);
	}

	//	fs3002_trig3_p_ref
	val = of_property_read_u32(np, "fs3002_trig3_p_ref",&fs3002->dts_info.fs3002_trig3_p_ref);
	if (val != 0)
	{
		pr_err("%s:fs3002_trig3_p_ref not found\n", FSERROR);
		fs3002->dts_info.fs3002_trig3_p_ref = FS3002_DEFAULT_TRIG_REF;
	}		
	else
	{
		sprintf(str, "%s,fs3002_trig3_p_ref = %d\n", __func__,fs3002->dts_info.fs3002_trig3_p_ref);
		fs3002_debug_message(fs3002, str);
	}

	//	fs3002_trig3_n_ref
	val = of_property_read_u32(np, "fs3002_trig3_n_ref",&fs3002->dts_info.fs3002_trig3_n_ref);
	if (val != 0)
	{
		pr_err("%s:fs3002_trig3_n_ref not found\n", FSERROR);
		fs3002->dts_info.fs3002_trig3_n_ref = FS3002_DEFAULT_TRIG_REF;
	}		
	else
	{
		sprintf(str, "%s,fs3002_trig3_n_ref = %d\n", __func__,fs3002->dts_info.fs3002_trig3_n_ref);
		fs3002_debug_message(fs3002, str);
	}	

	// fs3002_lk_f0_cali
	val = of_property_read_u32(np, "fs3002_lk_f0_cali", &fs3002->dts_info.fs3002_lk_f0_cali);
	if (val != 0)
	{
		pr_err("%s:fs3002_lk_f0_cali not found\n", FSERROR);
		fs3002->dts_info.fs3002_lk_f0_cali = FS3002_F0_CALI_DATA_SELF_MODE;
	}		
	else
	{
		sprintf(str, "%s,fs3002_lk_f0_cali = 0x%02x\n", __func__,fs3002->dts_info.fs3002_lk_f0_cali);
		fs3002_debug_message(fs3002, str);
	}

	//	fs3002_f0_cali_data_mode
	val = of_property_read_u32(np,"fs3002_f0_cali_data_mode",&fs3002->dts_info.fs3002_f0_cali_data_mode);
	if (val != 0)
	{
		pr_err("%s:fs3002_f0_cali_data_mode not found\n", FSERROR);
		fs3002->dts_info.fs3002_f0_cali_data_mode = FS3002_F0_CALI_DATA_SELF_MODE;
	}
	else
	{
		sprintf(str, "%s,fs3002_f0_cali_data_mode=%d\n", __func__,fs3002->dts_info.fs3002_f0_cali_data_mode);
		fs3002_debug_message(fs3002, str);
	}

	//	fs3002_bypass_system_gain
	val = of_property_read_u32(np,"fs3002_bypass_system_gain",&fs3002->dts_info.fs3002_bypass_system_gain);
	if (val != 0)
	{
		pr_err("%s:fs3002_bypass_system_gain not found\n", FSERROR);
		fs3002->dts_info.fs3002_bypass_system_gain = FS3002_BYPASS_SYSTEM_GAIN_DEFAULT;
	}
	else
	{
		pr_info("fs3002_bypass_system_gain=%d\n",fs3002->dts_info.fs3002_bypass_system_gain);
	}	

	//	fs3002_hapstream_fre_gap
	val = of_property_read_u32(np,"fs3002_hapstream_fre_gap",&fs3002->dts_info.fs3002_hapstream_fre_gap);
	if (val != 0)
	{
		pr_err("%s:fs3002_hapstream_fre_gap not found\n", FSERROR);
		fs3002->dts_info.fs3002_hapstream_fre_gap = FS3002_HAPSTREAM_FRE_GAP_DEFAULT;
	}
	else
	{
		pr_info("fs3002_hapstream_fre_gap=%d\n",fs3002->dts_info.fs3002_hapstream_fre_gap);
	}

	//	fs3002_hapstream_trans_cycles
	val = of_property_read_u32(np,"fs3002_hapstream_trans_cycles",&fs3002->dts_info.fs3002_hapstream_trans_cycles);
	if (val != 0)
	{
		pr_err("%s:fs3002_hapstream_trans_cycles not found\n", FSERROR);
		fs3002->dts_info.fs3002_hapstream_trans_cycles = FS3002_HAPSTREAM_TRANS_CYCLES_DEFAULT;
	}
	else
	{
		pr_info("fs3002_hapstream_trans_cycles=%d\n",fs3002->dts_info.fs3002_hapstream_trans_cycles);
	}

	//	fs3002_f0_min
	val = of_property_read_u32(np,"fs3002_f0_min",&fs3002->dts_info.fs3002_f0_min);
	if (val != 0)
	{
		pr_err("%s:fs3002_f0_min not found\n", FSERROR);
		fs3002->dts_info.fs3002_f0_min = fs3002->dts_info.fs3002_f0_ref - FS3002_F0_REF_DIFF_DEFAULT;
	}
	else
	{
		pr_info("fs3002_f0_min=%d\n",fs3002->dts_info.fs3002_f0_min);
	}
	
	//	fs3002_f0_max
	val = of_property_read_u32(np,"fs3002_f0_max",&fs3002->dts_info.fs3002_f0_max);
	if (val != 0)
	{
		pr_err("%s:fs3002_f0_max not found\n", FSERROR);
		fs3002->dts_info.fs3002_f0_max = fs3002->dts_info.fs3002_f0_ref + FS3002_F0_REF_DIFF_DEFAULT;
	}
	else
	{
		pr_info("fs3002_f0_max=%d\n",fs3002->dts_info.fs3002_f0_max);
	}


	//predefined
	tmp = of_get_available_child_count(np);
	//devm_kcalloc alloc tmp size of sizeof(*fs3002->predefined)
	fs3002->predefined = devm_kcalloc(fs3002->dev, tmp,sizeof(*fs3002->predefined),GFP_KERNEL);
	if (!fs3002->predefined)
		return -ENOMEM;

	fs3002->effects_count = tmp;
	pr_info("fs3002->effects_count=%d\n",fs3002->effects_count);
	i = 0;
	for_each_available_child_of_node(np, child_node) 
	{
		effect = &fs3002->predefined[i++];
		//effect-id = <0>;
		rc = of_property_read_u32(child_node, "effect-id",&effect->id);
		if (rc != 0) 
		{
			pr_err("%s:Read effect-id failed\n",FSERROR);
		}
		else
		{
			sprintf(str, "%s,effect_id: %d\n", __func__,effect->id);
			fs3002_debug_message(fs3002, str);		
		}

		//wf-vmax-mv = <3600>;
		effect->vmax_mv = config->vmax_mv;
		rc = of_property_read_u32(child_node, "wf-vmax-mv", &tmp);
		if (rc != 0)
			pr_err("%s:Read wf-vmax-mv failed !\n",FSERROR);
		else
		{
			effect->vmax_mv = tmp;
			sprintf(str, "%s,effect->vmax_mv =%d \n", __func__,effect->vmax_mv);
			fs3002_debug_message(fs3002, str);				
		}

		//get wf-pattern = [3e 3e] number
		rc = of_property_count_elems_of_size(child_node,"wf-pattern",sizeof(u8));
		if (rc < 0) 
		{
			pr_err("%s:Count wf-pattern property failed !\n",FSERROR);
		}
		else if (rc == 0)
		{
			pr_err("%s:wf-pattern has no data\n",FSERROR);
		}
		else
		{
			sprintf(str, "%s,effect_id %d,wf-pattern count =0x%x \n", __func__,effect->id,rc);
			fs3002_debug_message(fs3002, str);		
		}

		//get wf-pattern = [3e 3e]
		effect->pattern_length = rc;
		effect->pattern = devm_kcalloc(fs3002->dev,effect->pattern_length,sizeof(u8), GFP_KERNEL);

		rc = of_property_read_u8_array(child_node, "wf-pattern",effect->pattern,effect->pattern_length);
		if (rc < 0) 
		{
			pr_err("%s:Read wf-pattern property failed !\n",FSERROR);
		}
		else
		{
			for (j = 0; j < effect->pattern_length; j++)
			{
				sprintf(str, "%s,effect->pattern[%d] = =%d \n", __func__,j,effect->pattern[j]);
				fs3002_debug_message(fs3002, str);
			}
		}

		//wf-play-rate-us = <20000>;
		effect->play_rate_us = config->play_rate_us;
		rc = of_property_read_u32(child_node, "wf-play-rate-us",&tmp);
		if (rc < 0)
			pr_err("%s:Read wf-play-rate-us failed !\n",FSERROR);
		else
		{
			effect->play_rate_us = tmp;
			sprintf(str, "%s,effect->play_rate_us=%d \n", __func__,effect->play_rate_us);
			fs3002_debug_message(fs3002, str);
		}

		//not find it in dts
		rc = of_property_read_u32(child_node, "wf-repeat-count",&tmp);
		if (rc < 0) 
		{
			//pr_err("%s,Read  wf-repeat-count failed !\n",FSERROR);
		}
		else 
		{
			for (j = 0; j < ARRAY_SIZE(wf_repeat); j++)
			{
				if (tmp <= wf_repeat[j])
				{
					break;
				}
			}
			effect->wf_repeat_n = j;
		}
		
		//"lra-auto-resonance-disable"
		effect->lra_auto_res_disable = of_property_read_bool(child_node,"lra-auto-resonance-disable");
		sprintf(str, "%s,lra-auto-resonance-disable = %d\n", __func__,effect->lra_auto_res_disable);
		fs3002_debug_message(fs3002, str);

		//wf-brake-pattern = [02 01 00 00]; get number
		tmp = of_property_count_elems_of_size(child_node,"wf-brake-pattern",sizeof(u8));
		if (tmp <= 0)
		{
			continue;
		}

		if (tmp > HAP_BRAKE_PATTERN_MAX) 
		{
			pr_err("%s:wf-brake-pattern shouldn't be more than %d bytes\n",FSERROR, HAP_BRAKE_PATTERN_MAX);
		}
		else
		{
			sprintf(str, "%s,wf-brake-pattern count=%d \n", __func__,tmp);
			fs3002_debug_message(fs3002, str);
		}

		//get wf-brake-pattern = [02 01 00 00] details
		rc = of_property_read_u8_array(child_node,"wf-brake-pattern",effect->brake, tmp);
		if (rc < 0) 
		{
			pr_err("%s:Failed to get wf-brake-pattern !\n",FSERROR);
		}
		else
		{
			for (j = 0; j < tmp; j++)
			{
				sprintf(str, "%s,effect->brake[%d] = =%d \n", __func__,j,effect->brake[j]);
				fs3002_debug_message(fs3002, str);
			}
		}

		effect->brake_pattern_length = tmp;
	}	
	return 0;
}

//okok
//fs3002_haptics_upload_effect
//1:case: fs3002->effect_type == FF_CONSTANT
//fs3002->duration = effect->replay.length;
//fs3002->activate_mode = FS3002_HAPTIC_ACTIVATE_RAM_LOOP_MODE;
//fs3002->effect_id = fs3002->dts_info.fs3002_ram_id_boundary;
//In this case, the vibration can be played for a fixed duration. The duration is effect->replay.length. The mode is recorded as Ram loop, vibration mode, and fs3002_ram_id_boundary (10 in DTS). How do you control the loop time? The essence is to use a timer, the time is up, the RAM play to stop.

//2:fs3002->effect_type == FF_PERIODIC
//Custom_data stores the user-specified play information in this effect-> u.putiodic.custom_data. Use copy_from_user to store this information in the variable data[]
//fs3002->effect_id = data[0]    //get effect_id
//A:If effect_id is between 0 and 9 (i.e., <boundary), then activate_mode is recorded as FS3002_HAPTIC_ACTIVATE_RAM_MODE
//B:If effect_id>=10, then activate_mode is recorded as FS3002_HAPTIC_ACTIVATE_RTP_MODE
//C:Both A and B need to save the duration for effect_id (obtained from DTS) to data[1], data[2]
//Finally, use copy_to_user to return the data[] information to effect->u.periodic.custom_data
int fs3002_haptics_upload_effect (struct input_dev *dev,struct ff_effect *effect,struct ff_effect *old)
{
	struct fs3002 *fs3002 = input_get_drvdata(dev);
	s16 data[CUSTOM_DATA_LEN];
	ktime_t rem;
	s64 time_us;
	int ret;

	sprintf(str,"%s,enter effect->type=0x%x,FF_CONSTANT=0x%x,FF_PERIODIC=0x%x, fs3002->dts_info.fs3002_rtp_max=%d\n",__func__,effect->type, FF_CONSTANT, FF_PERIODIC, fs3002->dts_info.fs3002_rtp_max);
	fs3002_debug_message(fs3002, str);

	if (fs3002->osc_cali_run != 0)
		return 0;

	if (hrtimer_active(&fs3002->timer)) 
	{
		rem = hrtimer_get_remaining(&fs3002->timer);
		time_us = ktime_to_us(rem);
		pr_info("waiting for playing clear sequence: %lld us\n",time_us);
		usleep_range(time_us, time_us + 100);
	}

	//include/uapi/linux/input.h
	//#define FF_RUMBLE 0x50
	//#define FF_PERIODIC	0x51
	//#define FF_CONSTANT	0x52
	//#define FF_SPRING 0x53
	//#define FF_FRICTION	0x54
	//#define FF_DAMPER 0x55
	//#define FF_INERTIA	0x56
	//#define FF_RAMP		0x57

	
	fs3002->effect_type = effect->type;
	
	sprintf(str,"%s,before mutex_lock(&fs3002->lock)\n",__func__);
	fs3002_debug_message(fs3002, str);
	
	mutex_lock(&fs3002->lock);
	
	sprintf(str,"%s,after mutex_lock(&fs3002->lock)\n",__func__);
	fs3002_debug_message(fs3002, str);
	
	while (atomic_read(&fs3002->exit_in_rtp_loop)) 
	{
		sprintf(str,"%s,goint to waiting rtp  exit\n",__func__);
		fs3002_debug_message(fs3002, str);
		mutex_unlock(&fs3002->lock);
		ret = wait_event_interruptible(fs3002->stop_wait_q,atomic_read(&fs3002->exit_in_rtp_loop) == 0);
		
		sprintf(str,"%s,wakeup \n",__func__);
		fs3002_debug_message(fs3002, str);
		
		if (ret == -ERESTARTSYS) 
		{
			mutex_unlock(&fs3002->lock);
			sprintf(str,"%s,%s:wake up by signal return error\n",__func__, FSERROR);
			fs3002_debug_message(fs3002, str);
			return ret;
		}
		mutex_lock(&fs3002->lock);
	}

	if (fs3002->effect_type == FF_CONSTANT) 
	{
		pr_info("effect_type is  FF_CONSTANT! \n");
		//set duration
		fs3002->duration = effect->replay.length;
		fs3002->activate_mode = FS3002_HAPTIC_ACTIVATE_RAM_LOOP_MODE;
		fs3002->effect_id = fs3002->dts_info.fs3002_ram_id_boundary;

	} 
	else if (fs3002->effect_type == FF_PERIODIC) 
	{
		sprintf(str,"%s,effect_type is  FF_PERIODIC! \n",__func__);
		fs3002_debug_message(fs3002, str);

		if (fs3002->effects_count == 0) 
		{
			mutex_unlock(&fs3002->lock);
			return -EINVAL;
		}

		if (copy_from_user(data, effect->u.periodic.custom_data,sizeof(s16) * CUSTOM_DATA_LEN)) 
		{
			mutex_unlock(&fs3002->lock);
			return -EFAULT;
		}

		fs3002->effect_id = data[0];//yyyy_get effect_id from data[0] as effect_type is FF_PERIODIC
		sprintf(str,"%s,fs3002->effect_id =%d \n",__func__,fs3002->effect_id);
		fs3002_debug_message(fs3002, str);

		fs3002->play.vmax_mv = effect->u.periodic.magnitude;	//vmax level

		if (fs3002->effect_id < 0 || fs3002->effect_id > fs3002->dts_info.fs3002_rtp_max) 
		{
			pr_err("%s:effect_id[%d] is out of the range\n", FSERROR, fs3002->effect_id);
			mutex_unlock(&fs3002->lock);
			return 0;
		}

		fs3002->is_custom_wave = 0;
		if (fs3002->effect_id < fs3002->dts_info.fs3002_ram_id_boundary) 
		{
			fs3002->activate_mode = FS3002_HAPTIC_ACTIVATE_RAM_MODE;
			sprintf(str,"%s,fs3002->effect_id=%d , fs3002->activate_mode = %d\n",__func__, fs3002->effect_id,fs3002->activate_mode);
			fs3002_debug_message(fs3002, str);
			data[1] = fs3002->predefined[fs3002->effect_id].play_rate_us / 1000000;	//second data
			data[2] = fs3002->predefined[fs3002->effect_id].play_rate_us / 1000;	//millisecond data
			sprintf(str,"%s,fs3002->predefined[fs3002->effect_id].play_rate_us/1000 = %d\n",__func__,fs3002->predefined[fs3002->effect_id].play_rate_us / 1000);
			fs3002_debug_message(fs3002, str);
		}
		
		if (fs3002->effect_id >= fs3002->dts_info.fs3002_ram_id_boundary) 
		{
			fs3002->activate_mode = FS3002_HAPTIC_ACTIVATE_RTP_MODE;
			sprintf(str,"%s,fs3002->effect_id=%d , fs3002->activate_mode = %d\n",__func__, fs3002->effect_id,fs3002->activate_mode);
			fs3002_debug_message(fs3002, str);
			data[1] = 30;
			data[2] = 0;
			//data[1] = fs3002->dts_info.fs3002_rtp_time[fs3002->effect_id] / 1000;	//second data
			//data[2] = fs3002->dts_info.fs3002_rtp_time[fs3002->effect_id] % 1000;	//millisecond data
			//sprintf(str,"%s,data[1] = %d data[2] = %d\n",__func__,data[1], data[2]);
			//fs3002_debug_message(fs3002, str);
		}

		if (fs3002->effect_id == CUSTOME_WAVE_ID) 
		{
			fs3002->activate_mode = FS3002_HAPTIC_ACTIVATE_RTP_MODE;
			sprintf(str,"%s,CUSTOME_WAVE_ID=%d , fs3002->activate_mode = %d\n",__func__, fs3002->effect_id,fs3002->activate_mode);
			fs3002_debug_message(fs3002, str);
			data[1] = 30;
			data[2] = 0;
			fs3002->is_custom_wave = 1;
			rb_init();
		}

		if (copy_to_user(effect->u.periodic.custom_data, data,sizeof(s16) * CUSTOM_DATA_LEN)) 
		{
			pr_err("%s:copy_to_user failed\n", FSERROR);
			mutex_unlock(&fs3002->lock);
			return -EFAULT;
		}
	} 
	else 
	{
		pr_err("%s:Unsupported effect type: %d\n", FSERROR,effect->type);
	}
	sprintf(str,"%s,before mutex_unlock(&fs3002->lock)     fs3002->effect_type= 0x%x\n",__func__, fs3002->effect_type);
	fs3002_debug_message(fs3002, str);
	
	mutex_unlock(&fs3002->lock);
	sprintf(str,"%s,after unlock,fs3002->effect_type= 0x%x\n",__func__,fs3002->effect_type);
	fs3002_debug_message(fs3002, str);
	return 0;
}

//okok
int fs3002_haptics_playback(struct input_dev *dev, int effect_id, int val)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;
	int rc = 0;

	sprintf(str,"%s,enter: fs3002->effect_id=%d , fs3002->activate_mode = %d, val=%d, duration=%d\n",__func__, fs3002->effect_id, fs3002->activate_mode,val, fs3002->duration);
	fs3002_debug_message(fs3002,str);
		

	//for osc calibration
	if (fs3002->osc_cali_run != 0)
	{
		return 0;
	}

	if (val > 0)
	{
		fs3002->state = 1;
	}
	if (val <= 0)
	{
		fs3002->state = 0;
	}
	hrtimer_cancel(&fs3002->timer);

	sprintf(str,"%s,hrtimer_cancel(&fs3002->timer)\n",__func__);
	fs3002_debug_message(fs3002,str);

	if (fs3002->effect_type == FF_CONSTANT && fs3002->activate_mode == FS3002_HAPTIC_ACTIVATE_RAM_LOOP_MODE) 
	{
		sprintf(str,"%s,enter ram_loop_mode \n",__func__);
		fs3002_debug_message(fs3002,str);
		queue_work(fs3002->work_queue, &fs3002->vibrate_work);
	} 
	else if (fs3002->effect_type == FF_PERIODIC && fs3002->activate_mode == FS3002_HAPTIC_ACTIVATE_RAM_MODE) 
	{
		sprintf(str,"%s,enter  ram_mode\n",__func__);
		fs3002_debug_message(fs3002,str);
		queue_work(fs3002->work_queue, &fs3002->vibrate_work);
	} 
	else if (fs3002->effect_type == FF_PERIODIC && fs3002->activate_mode == FS3002_HAPTIC_ACTIVATE_RTP_MODE) 
	{
		sprintf(str,"%s,enter  rtp_mode\n",__func__);
		fs3002_debug_message(fs3002,str);
		queue_work(fs3002->work_queue, &fs3002->rtp_work);
		//if we are in the play mode, force to exit
		if (val == 0) 
		{
			atomic_set(&fs3002->exit_in_rtp_loop, 1);
			rb_force_exit();
			wake_up_interruptible(&fs3002->stop_wait_q);			
		}
	} 
	else 
	{
		//other mode
	}

	return rc;
}


//okok
static void fs3002_vibrate_work_routine(struct work_struct *work)
{
	struct fs3002 *fs3002 = g_foursemi->fs3002;

	sprintf(str, "%s, enter,effect_id = %d, state=%d activate_mode = %d duration = %d\n", __func__,fs3002->effect_id, fs3002->state, fs3002->activate_mode, fs3002->duration);
	fs3002_debug_message(fs3002, str);

	mutex_lock(&fs3002->lock);
	//Enter standby mode
	fs3002_haptic_stop(fs3002);
	fs3002_haptic_upload_lra(fs3002, FS3002_F0_CALI);
	if (fs3002->state) 
	{
		if (fs3002->activate_mode == FS3002_HAPTIC_ACTIVATE_RAM_MODE) 
		{
			fs3002_haptic_play_effect_seq(fs3002, true);
		}
		else if (fs3002->activate_mode == FS3002_HAPTIC_ACTIVATE_RAM_LOOP_MODE) 
		{
			fs3002_haptic_play_effect_seq(fs3002, true);
			// run ms timer
			hrtimer_start(&fs3002->timer, ktime_set(fs3002->duration / 1000,(fs3002->duration % 1000) * 1000000), HRTIMER_MODE_REL);
		}
		else if (fs3002->activate_mode == FS3002_HAPTIC_ACTIVATE_CONT_MODE) 
		{
			pr_info("mode:%s\n","FS3002_HAPTIC_ACTIVATE_CONT_MODE");
			fs3002_haptic_cont_play(fs3002);
			// run ms timer
			hrtimer_start(&fs3002->timer,ktime_set(fs3002->duration / 1000,(fs3002->duration % 1000) * 1000000),HRTIMER_MODE_REL);
		}
		else if (fs3002->activate_mode == FS3002_HAPTIC_ACTIVATE_RTP_MODE) 
		{
			pr_info("mode:%s, do nothing\n","FS3002_HAPTIC_ACTIVATE_RTP_MODE");
		}
		else
		{
			pr_err("%s:activate_mode error\n", FSERROR);
		}	
	}

	mutex_unlock(&fs3002->lock);
}


static void fs3002_rtp_work_routine(struct work_struct *work)
{
	const struct firmware *rtp_file;
	int ret = -1;
	unsigned int cnt = 200;
	unsigned char reg_val = 0;
	bool rtp_work_flag = false;
	int value_is = 0,value_exit = 0;
	struct fs3002 *fs3002 = g_foursemi->fs3002;

	value_is = atomic_read(&fs3002->is_in_rtp_loop);
	value_exit = atomic_read(&fs3002->exit_in_rtp_loop);
	sprintf(str,"%s,enter fs3002->effect_id=%d,fs3002->dts_info.fs3002_ram_id_boundary=%d, fs3002->dts_info.fs3002_rtp_max=%d,fs3002->dts_info.fs3002_bypass_system_gain=%d,state = %d,is_in_rtp_loop=%d,exit_in_rtp_loop=%d\n",__func__, fs3002->effect_id,fs3002->dts_info.fs3002_ram_id_boundary,fs3002->dts_info.fs3002_rtp_max,fs3002->dts_info.fs3002_bypass_system_gain,fs3002->state,value_is,value_exit);
	fs3002_debug_message(fs3002, str);

	mutex_lock(&fs3002->lock);
	fs3002_haptic_upload_lra(fs3002, FS3002_WRITE_ZERO);
	fs3002_haptic_set_rtp_aei(fs3002, false);
	fs3002_interrupt_clear(fs3002);
	/* wait for irq to exit */
	atomic_set(&fs3002->exit_in_rtp_loop, 1);
	while (atomic_read(&fs3002->is_in_rtp_loop)) 
	{
		sprintf(str,"%s,goint to waiting irq exit\n",__func__);
		fs3002_debug_message(fs3002, str);
		ret = wait_event_interruptible(fs3002->wait_q,atomic_read(&fs3002->is_in_rtp_loop) == 0);
		pr_info("wakeup 1\n");
		if (ret == -ERESTARTSYS) 
		{
			atomic_set(&fs3002->exit_in_rtp_loop, 0);
			wake_up_interruptible(&fs3002->stop_wait_q);
			mutex_unlock(&fs3002->lock);
			pr_err("%s:wake up by signal return error\n",FSERROR);
			return;
		}
	}

	atomic_set(&fs3002->exit_in_rtp_loop, 0);
	wake_up_interruptible(&fs3002->stop_wait_q);

	//how to force exit this call
	if (fs3002->is_custom_wave == 1 && fs3002->state) 
	{
		pr_info("is_custom_wave == 1 and fs3002->state, buffer size %d, availbe size %d\n", fs3002->ram.base_addr >> 2, get_rb_avalible_size());
		while (get_rb_avalible_size() < fs3002->ram.base_addr && !rb_shoule_exit()) 
		{
			mutex_unlock(&fs3002->lock);
			ret = wait_event_interruptible(fs3002->stop_wait_q, (get_rb_avalible_size() >= fs3002->ram.base_addr) || rb_shoule_exit());
			pr_info("wakeup 2, buffer size %d, availbe size %d\n", fs3002->ram.base_addr >> 2, get_rb_avalible_size());
			if (ret == -ERESTARTSYS) 
			{
				pr_err("%s wake up by signal return erro\n", FSERROR);
				return;
			}
			mutex_lock(&fs3002->lock);
		}
	}

	fs3002_haptic_stop(fs3002);
	if (fs3002->state) 
	{
		pm_stay_awake(fs3002->dev);
		if(fs3002->dts_info.fs3002_bypass_system_gain == 0)
		{
			fs3002_haptic_effect_strength(fs3002);
			fs3002_haptic_set_gain(fs3002, fs3002->level);//20210716
		}

		fs3002->rtp_init = 0;
		if (fs3002->is_custom_wave == 0) 
		{
			fs3002->rtp_file_num = fs3002->effect_id;

			//sprintf(str,"%s,fs3002->rtp_file_num =%d\n",__func__,fs3002->rtp_file_num);
			//fs3002_debug_message(fs3002, str);
			
			if (fs3002->rtp_file_num < 0)
			{
				fs3002->rtp_file_num = 0;
			}
			if (fs3002->rtp_file_num > (fs3002_rtp_name_len - 1))
			{
				fs3002->rtp_file_num = fs3002_rtp_name_len - 1;
			}

			sprintf(str,"%s,fs3002->rtp_file_num =%d\n",__func__,fs3002->rtp_file_num);
			fs3002_debug_message(fs3002, str);

			fs3002->rtp_routine_on = 1;
			//fw loaded
			ret = request_firmware(&rtp_file, fs3002_rtp_name[fs3002->rtp_file_num], fs3002->dev);
			if (ret < 0) 
			{
				pr_err("%s: failed to read %s\n", FSERROR, fs3002_rtp_name[fs3002->rtp_file_num]);
				fs3002->rtp_routine_on = 0;
				pm_relax(fs3002->dev);
				mutex_unlock(&fs3002->lock);
				return;
			}
			vfree(fs3002->rtp_container);
			fs3002->rtp_container = vmalloc(rtp_file->size + sizeof(int));
			if (!fs3002->rtp_container) 
			{
				release_firmware(rtp_file);
				pr_err("%s: error allocating memory\n", FSERROR);
				fs3002->rtp_routine_on = 0;
				pm_relax(fs3002->dev);
				mutex_unlock(&fs3002->lock);
				return;
			}
			fs3002->rtp_container->len = rtp_file->size;
			pr_info("rtp file:[%s] size = %dbytes\n", fs3002_rtp_name[fs3002->rtp_file_num], fs3002->rtp_container->len);
			memcpy(fs3002->rtp_container->data, rtp_file->data, rtp_file->size);
			release_firmware(rtp_file);
		} 
		else
		{
			//rtp ringbuffer
			//fs3002_custom_wave_store
			vfree(fs3002->rtp_container);
			fs3002->rtp_container = NULL;
			if(fs3002->ram.base_addr != 0) 
			{
				fs3002->rtp_container = vmalloc((fs3002->ram.base_addr >> 2) + sizeof(int));
			} 
			else 
			{
				pr_err("%s:ram update not done yet, return !",FSERROR);
			}
			if (!fs3002->rtp_container) 
			{
				pr_err("%s: error allocating memory\n", FSERROR);
				pm_relax(fs3002->dev);
				mutex_unlock(&fs3002->lock);
				return;
			}
		}
		fs3002->rtp_init = 1;
		//rtp mode config
		fs3002_haptic_set_mode(fs3002, FS3002_HAPTIC_RTP_MODE);
		//enable ram access and bulk
		fs3002_haptic_raminit(fs3002,true);
		//haptic go
		fs3002_haptic_play_go(fs3002);
		usleep_range(2000, 2500);
		while (cnt) 
		{
			fs3002_i2c_read(fs3002, FS3002_DIGSTAT, &reg_val);
			if ((reg_val >> 4) == FS3002_DIGSTAT_B7_4_OPS_GO) //DIGSTAT_OPS == 0x20  go
			{
				cnt = 0;
				rtp_work_flag = true;
				sprintf(str,"%s,RTP_GO! OPS = 2\n",__func__);
				fs3002_debug_message(fs3002,str);
			} 
			else 
			{
				cnt--;
				sprintf(str,"%s,wait for RTP_GO, OPS=%d\n",__func__,(reg_val>>4));
				fs3002_debug_message(fs3002, str);
			}
			usleep_range(2000, 2500);
		}

		if (rtp_work_flag) 
		{
			fs3002_haptic_rtp_init(fs3002);
		} 
		else
		{
			//enter standby mode
			fs3002_haptic_raminit(fs3002,false);
			fs3002_haptic_stop(fs3002);
			pr_err("%s failed to enter RTP_GO status!\n", FSERROR);
		}
		fs3002->rtp_routine_on = 0;
	} 
	else
	{
		fs3002->rtp_cnt = 0;
		fs3002->rtp_init = 0;
		pm_relax(fs3002->dev);
	}
	mutex_unlock(&fs3002->lock);
}

static int fs3002_haptic_rtp_init(struct fs3002 *fs3002)
{
	unsigned int buf_len = 0;

	pr_info("enter fs3002->play_mode=%d\n",fs3002->play_mode);
	pm_qos_enable(fs3002, true);
	fs3002->rtp_cnt = 0;
	disable_irq(gpio_to_irq(fs3002->irq_gpio));
	while ((!fs3002_haptic_rtp_get_fifo_afs_0xAF(fs3002)) && (fs3002->play_mode == FS3002_HAPTIC_RTP_MODE) &&  !atomic_read(&fs3002->exit_in_rtp_loop)) 
	{
		sprintf(str,"%s,rtp cnt = %d\n",__func__,fs3002->rtp_cnt);
		fs3002_debug_message(fs3002, str);
		if (fs3002->is_custom_wave == 0) 
		{
			if (fs3002->rtp_cnt < (fs3002->ram.base_addr)) 
			{
				if ((fs3002->rtp_container->len - fs3002->rtp_cnt) < (fs3002->ram.base_addr)) 
				{
					buf_len = fs3002->rtp_container->len - fs3002->rtp_cnt;
				}
				else 
				{
					buf_len = fs3002->ram.base_addr;
				}
			} 
			else if ((fs3002->rtp_container->len - fs3002->rtp_cnt) < (fs3002->ram.base_addr >> 2)) 
			{
				buf_len = fs3002->rtp_container->len - fs3002->rtp_cnt;
			}
			else 
			{
				buf_len = fs3002->ram.base_addr >> 2;
			}
			
			sprintf(str,"%s,buf_len = %d\n",__func__,buf_len);
			fs3002_debug_message(fs3002, str);
			
			fs3002_i2c_writes(fs3002,FS3002_RTPWDATA,&fs3002->rtp_container->data[fs3002->rtp_cnt],buf_len);

			fs3002->rtp_cnt += buf_len;
			if (fs3002->rtp_cnt == fs3002->rtp_container->len) 
			{
				sprintf(str,"%s,finished in fs3002_haptic_rtp_init while loop = %d\n",__func__,buf_len);
				fs3002_debug_message(fs3002, str);
				fs3002->rtp_cnt = 0;
				fs3002->rtp_init = 0;
				pm_qos_enable(fs3002, false);
				fs3002_haptic_raminit(fs3002,false);
				fs3002_haptic_set_rtp_aei(fs3002, false);
				break;
			}
		} 
		else 
		{
			buf_len = read_rb(fs3002->rtp_container->data,  fs3002->ram.base_addr >> 2);
			fs3002_i2c_writes(fs3002, FS3002_RTPWDATA, fs3002->rtp_container->data, buf_len);
			if (buf_len < (fs3002->ram.base_addr >> 2)) 
			{
				pr_info(" custom rtp update complete, buf_len = %d\n", buf_len);
				fs3002->rtp_cnt = 0;
				fs3002->rtp_init = 0;
				fs3002_haptic_raminit(fs3002,false);
				fs3002_haptic_set_rtp_aei(fs3002, false);
				break;
			}
		}
	}
	enable_irq(gpio_to_irq(fs3002->irq_gpio));
	if (fs3002->play_mode == FS3002_HAPTIC_RTP_MODE && !atomic_read(&fs3002->exit_in_rtp_loop)) 
	{
		fs3002_haptic_set_rtp_aei(fs3002, true);
	}

	sprintf(str,"%s,exit\n",__func__);
	fs3002_debug_message(fs3002,str);
	pm_qos_enable(fs3002, false);
	return 0;
}


irqreturn_t fs3002_irq(int irq, void *data)
{
	struct fs3002 *fs3002 = data;
	unsigned char DIGSTAT_OPS = 0;
	unsigned char reg_val = 0;
	unsigned int buf_len = 0;
	
#ifdef FS_HAPSTREAM
	sprintf(str,"%s,enter hapstream_stop_flag=%d\n",__func__, fs3002->hapstream_stop_flag);
	fs3002_debug_message(fs3002, str);

	if (fs3002->hapstream_stop_flag == false) 
	{
		return IRQ_HANDLED;
	}
#endif

	atomic_set(&fs3002->is_in_rtp_loop, 1);
	sprintf(str,"%s, enter, rtp_init=%d\n",__func__, fs3002->rtp_init);
	fs3002_debug_message(fs3002, str);
	
	fs3002_i2c_read(fs3002, FS3002_INTSTAT1, &reg_val);
	if(reg_val != 0)
	{
		fs3002->rtp_routine_on = 0;
		pr_err("%s,reg INTSTAT1=0x%02X\n",FSERROR, reg_val);
	}
	
	fs3002_i2c_read(fs3002, FS3002_INTSTAT2, &reg_val);
	sprintf(str,"%s,reg INTSTAT2=0x%02X\n",__func__, reg_val);
	fs3002_debug_message(fs3002, str);

	//almost empty
	if ((reg_val & FS3002_INTSTAT2_MASK_B1_AE) && fs3002->rtp_init) 
	{
		sprintf(str,"%s,fs3002 rtp fifo almost empty\n",__func__);
		fs3002_debug_message(fs3002,str);

		while ((!fs3002_haptic_rtp_get_fifo_afs_0xAF(fs3002)) && (fs3002->play_mode == FS3002_HAPTIC_RTP_MODE) && !atomic_read(&fs3002->exit_in_rtp_loop)) 
		{
			mutex_lock(&fs3002->rtp_lock);
			
			sprintf(str,"%s,fs3002 rtp mode fifo update, cnt=%d\n",__func__, fs3002->rtp_cnt);
			fs3002_debug_message(fs3002, str);
			
			if (!fs3002->rtp_container) 
			{
				pr_err("%s:fs3002->rtp_container is null, break!\n", FSERROR);
				mutex_unlock(&fs3002->rtp_lock);
				break;
			}

			if (fs3002->is_custom_wave == 1) 
			{
				buf_len = read_rb(fs3002->rtp_container->data, (fs3002->ram.base_addr >> 2));
				fs3002_i2c_writes(fs3002, FS3002_RTPWDATA, fs3002->rtp_container->data, buf_len);
				if (buf_len < (fs3002->ram.base_addr >> 2)) 
				{
					pr_info("rtp update complete, buf_len=%d\n", buf_len);
					fs3002_haptic_raminit(fs3002,false);
					fs3002_haptic_set_rtp_aei(fs3002, false);
					fs3002->rtp_cnt = 0;
					fs3002->rtp_init = 0;
					mutex_unlock(&fs3002->rtp_lock);
					break;
				}
			} 
			else
			{
				//total file len - transfered file len < 1/2 file size
				if ((fs3002->rtp_container->len - fs3002->rtp_cnt) < (fs3002->ram.base_addr >> 2)) 
				{
					buf_len = fs3002->rtp_container->len - fs3002->rtp_cnt;
				} 
				else 
				{
					if(fs3002->fs3002_debug_enable)
					{
						buf_len = fs3002->buf_size;
					}
					else
					{
						buf_len = fs3002->ram.base_addr >> 2;
					}
				}
				fs3002_i2c_writes(fs3002,FS3002_RTPWDATA,&fs3002->rtp_container->data[fs3002->rtp_cnt],buf_len);
				fs3002->rtp_cnt += buf_len;

				fs3002_i2c_read(fs3002, FS3002_DIGSTAT, &DIGSTAT_OPS);
				DIGSTAT_OPS = DIGSTAT_OPS >> 4;//get 0x4 high 4 bits(7-4)
				if ((DIGSTAT_OPS & 0x0f) == 0) 
				{
					if (fs3002->rtp_cnt != fs3002->rtp_container->len)
						pr_err("%s: rtp play suspend!\n",FSERROR);
					else
						pr_info("rtp update complete! (rtp_cnt == rtp_container->len=%d)\n",fs3002->rtp_cnt);
					fs3002->rtp_routine_on = 0;
					fs3002_haptic_raminit(fs3002,false);
					fs3002_haptic_set_rtp_aei(fs3002,false);
					fs3002->rtp_cnt = 0;
					fs3002->rtp_init = 0;
					mutex_unlock(&fs3002->rtp_lock);
					break;
				}
			}
			mutex_unlock(&fs3002->rtp_lock);
		}
	}


	if (reg_val & FS3002_INTSTAT2_MASK_B1_AF)
	{
		sprintf(str,"%s,fs3002 rtp mode fifo almost full!\n",__func__);
		fs3002_debug_message(fs3002,str);
	}			
	

	if (fs3002->play_mode != FS3002_HAPTIC_RTP_MODE || atomic_read(&fs3002->exit_in_rtp_loop))
	{
		fs3002_haptic_set_rtp_aei(fs3002, false);
	}

	atomic_set(&fs3002->is_in_rtp_loop, 0);
	wake_up_interruptible(&fs3002->wait_q);

	sprintf(str,"%s,exit\n",__func__);
	fs3002_debug_message(fs3002,str);

	return IRQ_HANDLED;
}
