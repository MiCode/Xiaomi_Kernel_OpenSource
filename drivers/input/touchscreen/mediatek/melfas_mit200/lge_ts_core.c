/* lge_ts_core.c
 *
 * Copyright (C) 2011 LGE.
 *
 * Author: yehan.ahn@lge.com, hyesung.shin@lge.com
 * Modified : WX-BSP-TS@lge.com
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define LGE_TS_CORE_C

#include <asm/atomic.h>

#include <linux/version.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/moduleparam.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/firmware.h>
//#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
//#endif
#if defined(CONFIG_FB)
#include <linux/fb.h>
#include <linux/notifier.h>
#endif
#if defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif
#include <linux/completion.h>
#include <linux/wakelock.h>
#include <linux/pinctrl/consumer.h>
//#include <mach/board_lge.h>


#include "lge_ts_core.h"

#ifdef USE_DMA
#include <linux/dma-mapping.h>
#endif

#ifndef IS_MTK
#define LGE_TOUCH_NAME		"lge_touch"
#else
#include <linux/kthread.h>
#include <cust_eint.h>
#include <mach/wd_api.h>
#include <mach/mt_wdt.h>
#include "L0M45P1_00_02_bin.h"

#define LGE_TOUCH_NAME		"mtk_tpd"
#define TPD_DEV_NAME		"lge_touch_melfas"

static u8* I2CDMABuf_va = NULL;
static u32 I2CDMABuf_pa = NULL;

static bool download_status = 1;		/* avoid power off during F/W upgrade */
struct lge_touch_data *g_ts = NULL;


extern int mtk_wdt_enable(enum wk_wdt_en en);
static DECLARE_WAIT_QUEUE_HEAD(tpd_waiter);
static DEFINE_MUTEX(i2c_access);
#endif


enum lge_boot_mode_type {
	LGE_BOOT_MODE_NORMAL = 0,
	LGE_BOOT_MODE_CHARGER,
	LGE_BOOT_MODE_CHARGERLOGO,
	LGE_BOOT_MODE_QEM_56K,
	LGE_BOOT_MODE_QEM_130K,
	LGE_BOOT_MODE_QEM_910K,
	LGE_BOOT_MODE_PIF_56K,
	LGE_BOOT_MODE_PIF_130K,
	LGE_BOOT_MODE_PIF_910K,
	LGE_BOOT_MODE_MINIOS    /* LGE_UPDATE for MINIOS2.0 */
};

enum lge_boot_mode_type lge_get_boot_mode(void)
{
	return LGE_BOOT_MODE_NORMAL;
}

struct lge_touch_data
{
	void				*h_touch;
	atomic_t			device_init;
	atomic_t			crack_test_state;
	u8				work_sync_err_cnt;
	u8				ic_init_err_cnt;
	u8				ic_error_cnt;
	volatile bool is_probed;
	struct mutex			thread_lock;
	struct i2c_client 		*client;
	struct input_dev 		*input_dev;
	struct hrtimer 			timer;
	struct hrtimer 			trigger_timer;
	struct work_struct  		work;
	struct delayed_work		work_init;
	struct delayed_work		work_touch_lock;
	struct work_struct  		work_fw_upgrade;
	struct delayed_work		work_crack;
#if defined(CONFIG_FB)
	struct notifier_block		fb_notifier_block;
#endif
#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend		early_suspend;
#endif
	struct touch_platform_data 	*pdata;
	struct touch_data		ts_data;
	struct touch_fw_info		fw_info;
	struct section_info		st_info;
	struct kobject 			lge_touch_kobj;
	struct ghost_finger_ctrl	gf_ctrl;
	bool sd_status;
	struct completion irq_completion;
	struct completion fw_upgrade_completion;
	struct pinctrl      *ts_pinctrl;
	struct pinctrl_state	*ts_pinset_state_active;
	struct pinctrl_state	*ts_pinset_state_suspend;

};

struct lge_touch_attribute {
	struct attribute	attr;
	ssize_t (*show)(struct lge_touch_data *ts, char *buf);
	ssize_t (*store)(struct lge_touch_data *ts, const char *buf, size_t count);
};
#define LGE_TOUCH_ATTR(_name, _mode, _show, _store)	\
struct lge_touch_attribute lge_touch_attr_##_name = __ATTR(_name, _mode, _show, _store)

#define ts_pdata	((ts)->pdata)
#define ts_caps		(ts_pdata->caps)
#define ts_role		(ts_pdata->role)
#define ts_pwr		(ts_pdata->pwr)
#define ts_limit	(ts_pdata->limit)
#define ts_lpwg		(ts_pdata->tci_info)


struct touch_device_driver	*touch_drv;
static struct workqueue_struct	*touch_wq = NULL;

struct wake_lock touch_wake_lock;
static DEFINE_MUTEX(i2c_suspend_lock);
static DEFINE_MUTEX(irq_lock);

static bool touch_irq_mask = 1;
static bool touch_irq_wake_mask = 0;

#define get_time_interval(a,b) 	(a >= b ? a-b : 1000000+a-b)
#define jitter_abs(x)		(x >= 0 ? x : -x)
#define jitter_sub(x, y)	(x >= y ? x - y : y - x)

#ifdef LGE_TOUCH_GHOST_DETECTION
static unsigned int ta_debouncing_count = 0;
static unsigned int button_press_count =0;
static unsigned int ts_rebase_count =0;
struct timeval t_ex_debug[TIME_EX_PROFILE_MAX];
struct t_data	 ts_prev_finger_press_data;
int long_press_check_count = 0;
int force_continuous_mode = 0;
int long_press_check = 0;
int finger_subtraction_check_count = 0;
bool ghost_detection = 0;
int ghost_detection_count = 0;
int trigger_baseline = 0;
int ts_charger_plug = 0;
#endif

int power_block = 0;
enum window_status window_crack_check = NO_CRACK;
static char *window_crack[2] = { "TOUCH_WINDOW_STATE=CRACK", NULL };

static void safety_reset(struct lge_touch_data *ts);
static int touch_ic_init(struct lge_touch_data *ts);
static void touch_work_func_a(struct lge_touch_data *ts);
static int touch_local_init ( void );
atomic_t			dev_state;
static int tpd_flag = 0;
/* Debug mask value
 * usage: echo [debug_mask] > /sys/module/lge_ts_core/parameters/debug_mask
 */
u32 touch_debug_mask_ = 0
			| DEBUG_BASE_INFO
			/* | DEBUG_ABS */
			| DEBUG_CONFIG
			| DEBUG_TRACE 
			| DEBUG_POWER
			| DEBUG_FW_UPGRADE
			| DEBUG_ABS_POINT
			;
module_param_named(debug_mask, touch_debug_mask_, int, S_IRUGO|S_IWUSR|S_IWGRP);

#if defined(CONFIG_LGE_LCD_ESD)
void mdss_lcd_esd_reset(void);
#endif

static void release_all_ts_event(struct lge_touch_data *ts);

/* set_touch_handle_
 *
 * Developer can save their object using 'set_touch_handle_'.
 * Also, they can restore that using 'get_touch_handle_'.
 */

void set_touch_handle_(struct i2c_client *client, void* h_touch)
{
	struct lge_touch_data *ts = i2c_get_clientdata(client);
	ts->h_touch = h_touch;
}

void* get_touch_handle_(struct i2c_client *client)
{
	struct lge_touch_data *ts = i2c_get_clientdata(client);
	return ts->h_touch;
}

static struct bus_type touch_subsys = {
	.name = LGE_TOUCH_NAME,
	.dev_name = "lge_touch",
};

static struct device device_touch = {
	.id    = 0,
	.bus   = &touch_subsys,
};

/* send_uevent
*
* It will be used to send u-event to Android-framework.
*/
void send_uevent(char* string[2])
{
	kobject_uevent_env(&device_touch.kobj, KOBJ_CHANGE, string);
	TOUCH_INFO_MSG("uevent[%s]\n", string[0]);
}


void power_lock(int value)
{
	power_block |= value;
}

void power_unlock(int value)
{
	power_block &= ~(value);
}

#ifdef LGE_TOUCH_GHOST_DETECTION
static enum hrtimer_restart touch_trigger_timer_handler(struct hrtimer *timer)
{
	struct lge_touch_data *ts =
			container_of(timer, struct lge_touch_data, trigger_timer);

	if (ts_role->ghost_detection_enable) {
		if (trigger_baseline==1 && atomic_read(&ts->device_init) == 1)
		{
			trigger_baseline = 2;
			atomic_inc(&ts->next_work);
			queue_work(touch_wq, &ts->work);
		}
	}
	return HRTIMER_NORESTART;
}

void trigger_baseline_state_machine(int usb_type)
{
	u8 buf=0;

	if (touch_test_dev && touch_test_dev->pdata->role->ghost_detection_enable) {
		if (touch_test_dev->curr_pwr_state == POWER_ON) {
			if (usb_type ==0) {
				touch_i2c_read(touch_test_dev->client, 0x50, 1, &buf);
				buf = buf & 0xDF;
				touch_i2c_write_byte(touch_test_dev->client, 0x50, buf);
			} else {
				touch_i2c_read(touch_test_dev->client, 0x50, 1, &buf);
				buf = buf | 0x20;
				touch_i2c_write_byte(touch_test_dev->client, 0x50, buf);
			}
		}
		TOUCH_INFO_MSG(" trigger_baseline_state_machine = %d \n", usb_type);

		ts_charger_plug = (usb_type == 0) ? 0 : 1;
		if ( trigger_baseline == 0) {
			trigger_baseline = 1;
			hrtimer_start(&ts->trigger_timer, ktime_set(0, MS_TO_NS(1000)), HRTIMER_MODE_REL);
		}
	}
}


/* Ghost Detection Solution */
static u8 resume_flag = 0;
int ghost_detect_solution(struct lge_touch_data *ts)
{
	int first_int_detection = 0;
	int cnt = 0, id =0;

	if (ts->gf_ctrl.incoming_call && (ts->ts_data.total_num > 1)) {
		TOUCH_INFO_MSG("call state rebase\n");
		goto out_need_to_rebase;
	}

	if (trigger_baseline==2)
		goto out_need_to_rebase;

	if (resume_flag) {
		resume_flag = 0;
		do_gettimeofday(&t_ex_debug[TIME_EX_FIRST_INT_TIME]);

		if ( t_ex_debug[TIME_EX_FIRST_INT_TIME].tv_sec - t_ex_debug[TIME_EX_INIT_TIME].tv_sec == 0 ) {
			if ((get_time_interval(t_ex_debug[TIME_EX_FIRST_INT_TIME].tv_usec,
			                   t_ex_debug[TIME_EX_INIT_TIME].tv_usec)) <= 200000) first_int_detection= 1;
		} else if ( t_ex_debug[TIME_EX_FIRST_INT_TIME].tv_sec - t_ex_debug[TIME_EX_INIT_TIME].tv_sec == 1 ) {
			if ( t_ex_debug[TIME_EX_FIRST_INT_TIME].tv_usec + 1000000
				- t_ex_debug[TIME_EX_INIT_TIME].tv_usec <= 200000) {
				first_int_detection = 1;
			}
		}
	}

	if (first_int_detection) {
		for (cnt = 0; cnt < ts_caps->max_id; cnt++) {
			if (ts->ts_data.curr_data[cnt].status == FINGER_PRESSED) {
					TOUCH_INFO_MSG("ghost detected within first input time 200ms\n");
					ghost_detection = true;
			}
		}
	}

	if (ts_charger_plug) {
		if ( (ts_role->ta_debouncing_finger_num  <= ts->ts_data.total_num) && ( ta_debouncing_count < ts_role->ta_debouncing_count)) {
			ts_role->ta_debouncing_count++;
			memset(&ts->ts_data.curr_data, 0x0, sizeof(ts->ts_data.curr_data));
			goto out_need_to_debounce;
		} else if (ts->ts_data.total_num < ts_role->ta_debouncing_finger_num) {
			ts_role->ta_debouncing_count = 0;
		} else ;
	}

	if ((ts->ts_data.state != TOUCH_ABS_LOCK) &&(ts->ts_data.total_num)) {

		if (ts->ts_data.prev_total_num != ts->ts_data.total_num)
		{
			if (ts->ts_data.prev_total_num <= ts->ts_data.total_num)
			{
			       if (ts->gf_ctrl.stage == GHOST_STAGE_CLEAR || (ts->gf_ctrl.stage | GHOST_STAGE_1) || ts->gf_ctrl.stage == GHOST_STAGE_4)
					ts->ts_data.state = TOUCH_BUTTON_LOCK;

				for(id=0; id < ts_caps->max_id; id++) {
					if (ts->ts_data.curr_data[id].status == FINGER_PRESSED
							&& ts->ts_data.prev_data[id].status == FINGER_RELEASED) {
						break;
					}
				}

				if ( id < 10)
				{
					memcpy(&t_ex_debug[TIME_EX_PREV_PRESS_TIME], &t_ex_debug[TIME_EX_CURR_PRESS_TIME], sizeof(struct timeval));
					do_gettimeofday(&t_ex_debug[TIME_EX_CURR_PRESS_TIME]);

					if ( 1<= ts->ts_data.prev_total_num && 1<= ts->ts_data.total_num && jitter_sub(ts_prev_finger_press_data.x_position,ts->ts_data.curr_data[id].x_position)<=10 && jitter_sub(ts_prev_finger_press_data.y_position,ts->ts_data.curr_data[id].y_position)<=10 )
					{
					       // if time_interval between prev fingger pressed and curr finger pressed is less than 50ms, we need to rebase touch ic.
						if (((t_ex_debug[TIME_EX_CURR_PRESS_TIME].tv_sec - t_ex_debug[TIME_EX_PREV_PRESS_TIME].tv_sec)==1) &&
							(( get_time_interval(t_ex_debug[TIME_EX_CURR_PRESS_TIME].tv_usec+1000000, t_ex_debug[TIME_EX_PREV_PRESS_TIME].tv_usec)) <= 50*1000))
						{
							ghost_detection = true;
							ghost_detection_count++;
						}
						else if (((t_ex_debug[TIME_EX_CURR_PRESS_TIME].tv_sec - t_ex_debug[TIME_EX_PREV_PRESS_TIME].tv_sec)==0) &&
							(( get_time_interval(t_ex_debug[TIME_EX_CURR_PRESS_TIME].tv_usec, t_ex_debug[TIME_EX_PREV_PRESS_TIME].tv_usec)) <= 50*1000))
						{
							ghost_detection = true;
							ghost_detection_count++;
						}
						else	; // do not anything
					}
					else if (ts->ts_data.prev_total_num==0 && ts->ts_data.total_num==1 && jitter_sub(ts_prev_finger_press_data.x_position,ts->ts_data.curr_data[id].x_position)<=10 && jitter_sub(ts_prev_finger_press_data.y_position,ts->ts_data.curr_data[id].y_position)<=10 )
					{
					       // if time_interval between prev fingger pressed and curr finger pressed is less than 50ms, we need to rebase touch ic.
						if (((t_ex_debug[TIME_EX_CURR_PRESS_TIME].tv_sec - t_ex_debug[TIME_EX_PREV_PRESS_TIME].tv_sec)==1) &&
							(( get_time_interval(t_ex_debug[TIME_EX_CURR_PRESS_TIME].tv_usec+1000000, t_ex_debug[TIME_EX_PREV_PRESS_TIME].tv_usec)) <= 50*1000))
						{
							ghost_detection = true;
						}
						else if (((t_ex_debug[TIME_EX_CURR_PRESS_TIME].tv_sec - t_ex_debug[TIME_EX_PREV_PRESS_TIME].tv_sec)==0) &&
							(( get_time_interval(t_ex_debug[TIME_EX_CURR_PRESS_TIME].tv_usec, t_ex_debug[TIME_EX_PREV_PRESS_TIME].tv_usec)) <= 50*1000))
						{
							ghost_detection = true;
						}
						else	; // do not anything
					}
					else if ( 5 < jitter_sub(ts->ts_data.prev_total_num,ts->ts_data.total_num) )
					{
						 ghost_detection = true;
					}
					else; //do not anything

					memcpy(&ts_prev_finger_press_data, &ts->ts_data.curr_data[id], sizeof(ts_prev_finger_press_data));
				}
			}else{
					memcpy(&t_ex_debug[TIME_EX_PREV_PRESS_TIME], &t_ex_debug[TIME_EX_CURR_PRESS_TIME], sizeof(struct timeval));
					do_gettimeofday(&t_ex_debug[TIME_EX_CURR_INT_TIME]);

				       // if finger subtraction time is less than 10ms, we need to check ghost state.
					if (((t_ex_debug[TIME_EX_CURR_INT_TIME].tv_sec - t_ex_debug[TIME_EX_PREV_PRESS_TIME].tv_sec)==1) &&
						(( get_time_interval(t_ex_debug[TIME_EX_CURR_INT_TIME].tv_usec+1000000, t_ex_debug[TIME_EX_PREV_PRESS_TIME].tv_usec)) < 11*1000))
						finger_subtraction_check_count++;
					else if (((t_ex_debug[TIME_EX_CURR_INT_TIME].tv_sec - t_ex_debug[TIME_EX_PREV_PRESS_TIME].tv_sec)==0) &&
						(( get_time_interval(t_ex_debug[TIME_EX_CURR_INT_TIME].tv_usec, t_ex_debug[TIME_EX_PREV_PRESS_TIME].tv_usec)) < 11*1000))
						finger_subtraction_check_count++;
					else
						finger_subtraction_check_count = 0;

					if (4<finger_subtraction_check_count) {
						finger_subtraction_check_count = 0;
						TOUCH_INFO_MSG("need_to_rebase finger_subtraction!!! \n");
						goto out_need_to_rebase;
					}
			}
		}

	}else if (!ts->ts_data.total_num) {
			long_press_check_count = 0;
			finger_subtraction_check_count = 0;
	}

	if (ts->ts_data.state != TOUCH_BUTTON_LOCK) {
		if (ts->work_sync_err_cnt > 0
				&& ts->ts_data.prev_button.state == BUTTON_RELEASED) {
			/* Do nothing */
		} else {

				if (button_press_count ==0)
					do_gettimeofday(&t_ex_debug[TIME_EX_BUTTON_PRESS_START_TIME]);
				else
					do_gettimeofday(&t_ex_debug[TIME_EX_BUTTON_PRESS_END_TIME]);

				button_press_count++;

				if (6 <= button_press_count)
				{
				     if (((t_ex_debug[TIME_EX_BUTTON_PRESS_END_TIME].tv_sec - t_ex_debug[TIME_EX_BUTTON_PRESS_START_TIME].tv_sec)==1) &&
						(( get_time_interval(t_ex_debug[TIME_EX_BUTTON_PRESS_END_TIME].tv_usec+1000000, t_ex_debug[TIME_EX_BUTTON_PRESS_START_TIME].tv_usec)) <= 100*1000)) {
							TOUCH_INFO_MSG("need_to_rebase button zero\n");
							goto out_need_to_rebase;
					} else if (((t_ex_debug[TIME_EX_BUTTON_PRESS_END_TIME].tv_sec - t_ex_debug[TIME_EX_BUTTON_PRESS_START_TIME].tv_sec)==0) &&
						(( get_time_interval(t_ex_debug[TIME_EX_BUTTON_PRESS_END_TIME].tv_usec, t_ex_debug[TIME_EX_BUTTON_PRESS_START_TIME].tv_usec)) <= 100*1000)) {
							TOUCH_INFO_MSG("need_to_rebase button zero\n");
							goto out_need_to_rebase;
					} else; //do not anything

					button_press_count = 0;
				} else {
					if ((t_ex_debug[TIME_EX_BUTTON_PRESS_END_TIME].tv_sec -
						t_ex_debug[TIME_EX_BUTTON_PRESS_START_TIME].tv_sec) > 1)
							button_press_count = 0;
					else if (((t_ex_debug[TIME_EX_BUTTON_PRESS_END_TIME].tv_sec - t_ex_debug[TIME_EX_BUTTON_PRESS_START_TIME].tv_sec)==1) &&
						(( get_time_interval(t_ex_debug[TIME_EX_BUTTON_PRESS_END_TIME].tv_usec+1000000, t_ex_debug[TIME_EX_BUTTON_PRESS_START_TIME].tv_usec)) >= 100*1000)) {
							button_press_count = 0;
					} else if (((t_ex_debug[TIME_EX_BUTTON_PRESS_END_TIME].tv_sec - t_ex_debug[TIME_EX_BUTTON_PRESS_START_TIME].tv_sec)==0) &&
						(( get_time_interval(t_ex_debug[TIME_EX_BUTTON_PRESS_END_TIME].tv_usec, t_ex_debug[TIME_EX_BUTTON_PRESS_START_TIME].tv_usec)) >= 100*1000)) {
							button_press_count = 0;
					} else; //do not anything
				}
		}
	}

	if (ghost_detection == true && ts->ts_data.total_num == 0 && ts->ts_data.palm == 0) {
		TOUCH_INFO_MSG("need_to_rebase zero\n");

		goto out_need_to_rebase;
	}
	else if (ghost_detection == true && 3 <= ghost_detection_count && ts->ts_data.palm == 0) {
		TOUCH_INFO_MSG("need_to_rebase zero 3\n");
		goto out_need_to_rebase;
	}

	return 0;

out_need_to_debounce:
	return NEED_TO_OUT;

out_need_to_rebase:
	{
		ghost_detection = false;
		ghost_detection_count = 0;
		memset(&ts_prev_finger_press_data, 0x0, sizeof(ts_prev_finger_press_data));
		button_press_count = 0;
		ts_rebase_count++;

		if (ts_rebase_count == 1) {
			do_gettimeofday(&t_ex_debug[TIME_EX_FIRST_GHOST_DETECT_TIME]);

			if ((t_ex_debug[TIME_EX_FIRST_GHOST_DETECT_TIME].tv_sec - t_ex_debug[TIME_EX_INIT_TIME].tv_sec) <= 3) {
				ts_rebase_count = 0;
				TOUCH_INFO_MSG("need_to_init in 3 sec\n");
				goto out_need_to_init;
			}
		} else {
			do_gettimeofday(&t_ex_debug[TIME_EX_SECOND_GHOST_DETECT_TIME]);

			if (((t_ex_debug[TIME_EX_SECOND_GHOST_DETECT_TIME].tv_sec - t_ex_debug[TIME_EX_FIRST_GHOST_DETECT_TIME].tv_sec) <= 5))
			{
				ts_rebase_count = 0;
				TOUCH_INFO_MSG("need_to_init\n");
				goto out_need_to_init;
			} else {
				ts_rebase_count = 1;
				memcpy(&t_ex_debug[TIME_EX_FIRST_GHOST_DETECT_TIME], &t_ex_debug[TIME_EX_SECOND_GHOST_DETECT_TIME], sizeof(struct timeval));
			}
		}
		release_all_ts_event(ts);
		memset(&ts->ts_data, 0, sizeof(ts->ts_data));
		if (touch_drv->ic_ctrl) {
			if (touch_drv->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_REBASE) < 0) {
				TOUCH_ERR_MSG("IC_CTRL_REBASE(2) handling fail\n");
			}
		}
		TOUCH_INFO_MSG("need_to_rebase\n");
	}
	return NEED_TO_OUT;

out_need_to_init:
	return NEED_TO_INIT;
}
#endif

void touch_enable(unsigned int irq) {

#ifndef IS_MTK
	mutex_lock(&irq_lock);

	if (!touch_irq_mask) {
		touch_irq_mask = 1;
		enable_irq(irq);		
	}

	mutex_unlock(&irq_lock);
#else
	mt_eint_unmask (CUST_EINT_TOUCH_PANEL_NUM);
#endif
}
void touch_disable(unsigned int irq) {

#ifndef IS_MTK
	mutex_lock(&irq_lock);

	if (touch_irq_mask) {
		touch_irq_mask = 0;
		disable_irq_nosync(irq);
	}
	mutex_unlock(&irq_lock);
#else
	mt_eint_mask (CUST_EINT_TOUCH_PANEL_NUM);
#endif
}
void touch_enable_wake(unsigned int irq) {
#ifdef IS_MTK
	return ;
#else	mutex_lock(&irq_lock);

	if (!touch_irq_wake_mask) {
		touch_irq_wake_mask = 1;
		enable_irq_wake(irq);
	}

	mutex_unlock(&irq_lock);
#endif
}
void touch_disable_wake(unsigned int irq) {
#ifdef IS_MTK
	return ;
#else
	mutex_lock(&irq_lock);

	if (touch_irq_wake_mask) {
		touch_irq_wake_mask = 0;
		disable_irq_wake(irq);
	}

	mutex_unlock(&irq_lock);
#endif
}

#if 0
int touch_i2c_read(struct i2c_client *client, u8 reg, int len, u8 *buf)
{
	struct i2c_msg msgs[] = {
		{
			.addr = ->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		{
			.addr = ->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};

	if (i2c_transfer(->adapter, msgs, 2) < 0) {
		if (printk_ratelimit())
			TOUCH_ERR_MSG("transfer error\n");
		return -EIO;
	} else
		return 0;
}

int touch_i2c_write(struct i2c_client *client, u8 reg, int len, u8 * buf)
{
	unsigned char send_buf[len + 1];
		struct i2c_msg msgs[] = {
			{
				.addr = ->addr,
				.flags = ->flags,
				.len = len+1,
			.buf = send_buf,
		},
	};

	send_buf[0] = (unsigned char)reg;
	memcpy(&send_buf[1], buf, len);

	if (i2c_transfer(->adapter, msgs, 1) < 0) {
		if (printk_ratelimit())
			TOUCH_ERR_MSG("transfer error\n");
		return -EIO;
	} else
		return 0;
}
#endif

#ifdef USE_DMA
/****************************************************************************
* with DMA, I2C  Read / Write Funtions
****************************************************************************/
int i2c_dma_write ( struct i2c_client *client, const uint8_t *buf, int len )
{
	int i = 0;
//	TOUCH_TRACE_FUNC();

	for ( i = 0 ; i < len ; i++ )
	{
		I2CDMABuf_va[i] = buf[i];
	}

	if ( len < 8 )
	{
		client->addr = client->addr & I2C_MASK_FLAG | I2C_ENEXT_FLAG;
		if(download_status == 1)
		{
			client->timing = 400;
		}
		else
		{
			client->timing = 100;
		}
		return i2c_master_send(client, buf, len);
	}
	else
	{
		client->addr = client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG | I2C_ENEXT_FLAG;
		if(download_status == 1)
		{
			client->timing = 400;
		}
		else
		{
			client->timing = 100;
		}
		return i2c_master_send(client, I2CDMABuf_pa, len);
	}
}

EXPORT_SYMBOL(i2c_dma_write );

int i2c_dma_read ( struct i2c_client *client, uint8_t *buf, int len )
{
	int i = 0, ret = 0;

//	TOUCH_TRACE_FUNC();

	if ( len < 8 )
	{
		client->addr = client->addr & I2C_MASK_FLAG | I2C_ENEXT_FLAG;
		client->timing = 400;
		return i2c_master_recv(client, buf, len);
	}
	else
	{
		client->addr = client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG | I2C_ENEXT_FLAG;
		client->timing = 400;
		ret = i2c_master_recv(client, I2CDMABuf_pa, len);
		if ( ret < 0 )
		{
			return ret;
		}

		for ( i = 0 ; i < len ; i++ )
		{
			buf[i] = I2CDMABuf_va[i];
		}
	}

	return ret;
}
EXPORT_SYMBOL(i2c_dma_read );

int i2c_msg_transfer ( struct i2c_client *client, struct i2c_msg *msgs, int count )
{
	int i = 0, ret = 0;

//	TOUCH_TRACE_FUNC();
	
	for ( i = 0 ; i < count ; i++ )
	{
		if ( msgs[i].flags & I2C_M_RD )
		{
			ret = i2c_dma_read(client, msgs[i].buf, msgs[i].len);
		}
		else
		{
			ret = i2c_dma_write(client, msgs[i].buf, msgs[i].len);
		}

		if ( ret < 0 )
		{
			return ret;
		}
	}

	return 0;
}

EXPORT_SYMBOL( i2c_msg_transfer );
#endif //USE_DMA
/* touch_asb_input_report
 *
 * finger status report
 */
static int touch_asb_input_report(struct lge_touch_data *ts, int status)
{
	int id = 0;
	u8 total = 0;

	struct input_dev *input_dev = ts->input_dev;

	if (status == FINGER_PRESSED) {
		for (id = 0; id < ts_caps->max_id; id++) {
			if ((ts_role->key_type == TOUCH_SOFT_KEY)
					&& (ts->ts_data.curr_data[id].y_position >= ts_caps->y_button_boundary))
				continue;

			if (ts->ts_data.curr_data[id].status == FINGER_PRESSED) {
				input_mt_slot(input_dev, id);
				input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, true);
				input_report_abs(input_dev, ABS_MT_POSITION_X,
						 ts->ts_data.curr_data[id].x_position);

				/* When a user's finger cross the boundary (from key to LCD),
				   a ABS-event will change its y-position to edge of LCD, automatically.*/

				if (ts_role->key_type == TOUCH_SOFT_KEY
						&& ts->ts_data.curr_data[id].y_position < ts_caps->y_button_boundary
						&& ts->ts_data.prev_data[id].y_position > ts_caps->y_button_boundary
						&& ts->ts_data.prev_button.key_code != KEY_NULL)
					input_report_abs(input_dev, ABS_MT_POSITION_Y, ts_caps->y_button_boundary);
				else
					input_report_abs(input_dev, ABS_MT_POSITION_Y, ts->ts_data.curr_data[id].y_position);

				if (ts_caps->is_pressure_supported)
					input_report_abs(input_dev, ABS_MT_PRESSURE,
							ts->ts_data.curr_data[id].pressure);

				if (ts_caps->is_width_supported) {
					input_report_abs(input_dev, ABS_MT_WIDTH_MAJOR,
							ts->ts_data.curr_data[id].width_major);
					input_report_abs(input_dev, ABS_MT_WIDTH_MINOR,
							ts->ts_data.curr_data[id].width_minor);
					input_report_abs(input_dev, ABS_MT_ORIENTATION,
							ts->ts_data.curr_data[id].width_orientation);
				}

				if (ts_caps->is_id_supported)
					input_report_abs(input_dev, ABS_MT_TRACKING_ID, id);

				ts->ts_data.curr_data[id].status = FINGER_HOLD;
				total++;

				if (unlikely(touch_debug_mask_ & DEBUG_ABS))
					TOUCH_INFO_MSG("hyj-press[%d] pos[%4d,%4d] w_m[%2d] w_n[%2d] w_o[%2d] p[%3d]\n",
							ts_caps->is_id_supported ? id : 0,
							ts->ts_data.curr_data[id].x_position,
							ts->ts_data.curr_data[id].y_position,
							ts_caps->is_width_supported ? ts->ts_data.curr_data[id].width_major: 0,
							ts_caps->is_width_supported ? ts->ts_data.curr_data[id].width_minor: 0,
							ts_caps->is_width_supported ? ts->ts_data.curr_data[id].width_orientation: 0,
							ts_caps->is_pressure_supported ? ts->ts_data.curr_data[id].pressure : 0);
			} else {
				/* release handling */
				if (ts_role->key_type != TOUCH_SOFT_KEY && ts->ts_data.curr_data[id].status == FINGER_RELEASED) {
					if (unlikely(touch_debug_mask_ & DEBUG_ABS))
						TOUCH_INFO_MSG("hyj-release [%d]\n", id);
					ts->ts_data.curr_data[id].status = FINGER_UNUSED;
					memset(&ts->ts_data.prev_data[id], 0x0, sizeof(ts->ts_data.prev_data[id]));

					input_mt_slot(input_dev, id);
					input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
				}
			}
		}
	} else if (status == FINGER_RELEASED) {
		for (id = 0; id < ts_caps->max_id; id++) {
			if (ts->ts_data.curr_data[id].status == FINGER_RELEASED) {
					if (unlikely(touch_debug_mask_ & DEBUG_ABS))
						TOUCH_INFO_MSG("hyj-release [%d]R\n", id);
				ts->ts_data.curr_data[id].status = FINGER_UNUSED;

				memset(&ts->ts_data.prev_data[id], 0x0, sizeof(ts->ts_data.prev_data[id]));

				input_mt_slot(input_dev, id);
				input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
			}
		}
	} else if (status == ALL_FINGER_RELEASED){
		for (id = 0; id < ts_caps->max_id; id++) {
			if (ts->ts_data.curr_data[id].status >= FINGER_PRESSED) {
				TOUCH_INFO_MSG("touch_release[%s] : <%d> x[%3d] y[%3d] M:%d\n",
					ts->ts_data.palm?"Palm":" ", id,
					ts->ts_data.curr_data[id].x_position,
					ts->ts_data.curr_data[id].y_position,
					ts->ts_data.curr_data[id].touch_conut);
			}

			ts->ts_data.curr_data[id].status = FINGER_UNUSED;
			ts->ts_data.curr_data[id].point_log_state = 0;
			memset(&ts->ts_data.prev_data[id], 0x0, sizeof(ts->ts_data.prev_data[id]));

			input_mt_slot(input_dev, id);
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
		}
	}

	return total;
}

static char *get_touch_button_string(u16 key_code)
{
	static char str[16] = {0};

	switch(key_code) {
		case KEY_BACK : /*158 0x9E*/
			sprintf(str, "BACK");
			break;
		case KEY_HOMEPAGE : /*172 0xAC*/
			sprintf(str, "HOME");
			break;
		case KEY_MENU : /* 139 0x8B*/
			sprintf(str, "MENU");
			break;
#if 0
		case KEY_SIMSWITCH : /*249 0xF9*/
			sprintf(str, "SIM_SWITCH");
			break;
#endif
		default :
			sprintf(str, "Unknown");
			break;
	}

	return str;
}

/* release_all_ts_event
 *
 * When system enters suspend-state,
 * if user press touch-panel, release them automatically.
 */
static void release_all_ts_event(struct lge_touch_data *ts)
{
	if (ts->input_dev == NULL) {
		//TOUCH_DEBUG_MSG("Input device is NOT allocated!!\n");
		return;
	}

	if (ts_role->key_type == TOUCH_HARD_KEY) {
		touch_asb_input_report(ts, ALL_FINGER_RELEASED);

		if (likely(touch_debug_mask_ & (DEBUG_ABS)))
			TOUCH_INFO_MSG("touch finger position released\n");

		if (ts->ts_data.prev_button.state == BUTTON_PRESSED) {
			input_report_key(ts->input_dev, ts->ts_data.prev_button.key_code, BUTTON_RELEASED);

			if (likely(touch_debug_mask_ & (DEBUG_BUTTON | DEBUG_BASE_INFO)))
				TOUCH_INFO_MSG("KEY[%s:%3d] is released\n",
					get_touch_button_string(ts->ts_data.prev_button.key_code), ts->ts_data.prev_button.key_code);
		}
	} else if (ts_role->key_type == VIRTUAL_KEY) {
		touch_asb_input_report(ts, ALL_FINGER_RELEASED);

		if (likely(touch_debug_mask_ & (DEBUG_ABS | DEBUG_BASE_INFO)))
			TOUCH_INFO_MSG("touch finger position released\n");
	} else if (ts_role->key_type == TOUCH_SOFT_KEY) {
		if (ts->ts_data.state == ABS_PRESS) {
			touch_asb_input_report(ts, FINGER_RELEASED);

			if (likely(touch_debug_mask_ & (DEBUG_ABS | DEBUG_BASE_INFO)))
				TOUCH_INFO_MSG("touch finger position released\n");
		} else if (ts->ts_data.state == BUTTON_PRESS) {
			input_report_key(ts->input_dev, ts->ts_data.prev_button.key_code, BUTTON_RELEASED);

			if (likely(touch_debug_mask_ & (DEBUG_BUTTON | DEBUG_BASE_INFO)))
				TOUCH_INFO_MSG("KEY[%d] is released\n", ts->ts_data.prev_button.key_code);
		}
	}

	ts->ts_data.prev_total_num = 0;
	ts->ts_data.touch_count_num = 0;
	input_sync(ts->input_dev);
}

/* touch_power_cntl
 *
 * 1. POWER_ON
 * 2. POWER_OFF
 * 3. POWER_SLEEP
 * 4. POWER_WAKE
 */
static int touch_power_cntl(struct lge_touch_data *ts, int onoff)
{
	int ret = 0;

	TOUCH_INFO_MSG("%s \n", __func__);
	
	if (touch_drv->power == NULL) {
		TOUCH_INFO_MSG("There is no specific power control function\n");
		return -1;
	}

	if (!ts->fw_info.is_downloading && power_block) {
		TOUCH_INFO_MSG("Power control blocked \n");
		return 0;
	}

	switch (onoff) {
	case POWER_ON:
		ret = touch_drv->power(ts->client, POWER_ON);
		if (ret < 0) {
			TOUCH_ERR_MSG("Power On failed\n");
		}
		break;

	case POWER_OFF:
		ret = touch_drv->power(ts->client, POWER_OFF);
		if (ret < 0) {
			TOUCH_ERR_MSG("Power Off failed\n");
		}
		atomic_set(&ts->device_init, 0);
		break;

	case POWER_SLEEP:
		ret = touch_drv->power(ts->client, POWER_SLEEP);
		if (ret < 0) {
			TOUCH_ERR_MSG("power Sleep failed\n");
		}
		break;

	case POWER_WAKE:
		ret = touch_drv->power(ts->client, POWER_WAKE);
		if (ret < 0) {
			TOUCH_ERR_MSG("Power Wake failed\n");
		}
		break;

	default:
		break;
	}

	if (ret >= 0)
		TOUCH_POWER_MSG("power_state[%d]\n", ts->pdata->curr_pwr_state);

	return ret;
}


/* safety_reset
 *
 * 1. disable irq/timer.
 * 2. turn off the power.
 * 3. turn on the power.
 * 4. sleep (booting_delay)ms, usually 400ms(synaptics).
 * 5. enable irq/timer.
 *
 * After 'safety_reset', we should call 'touch_init'.
 */
static void safety_reset(struct lge_touch_data *ts)
{
	TOUCH_TRACE_FUNC();
	
	if(ts->is_probed)
		touch_disable(ts->client->irq);

#ifdef LGE_TOUCH_GHOST_DETECTION
	if (ts_role->ghost_detection_enable) {
		hrtimer_cancel(&ts->trigger_timer);
	}
#endif

	release_all_ts_event(ts);

	touch_power_cntl(ts, POWER_OFF);
	touch_power_cntl(ts, POWER_ON);

	if(ts->is_probed)
		touch_enable(ts->client->irq);

	return;
}

/* touch_ic_init
 *
 * initialize the device_IC and variables.
 */
static int touch_ic_init(struct lge_touch_data *ts)
{
	int pinstate[3] = {0}; // SDA, SCL, INT

	TOUCH_TRACE_FUNC();

	if (unlikely(ts->ic_init_err_cnt >= MAX_RETRY_COUNT)) {
		TOUCH_ERR_MSG("Init Failed: Irq-pin has some unknown problems\n");
#if defined(CONFIG_PRE_SELF_DIAGNOSIS)
		lge_pre_self_diagnosis("i2c", 5, (char *)ts->client->name, (char *)ts->client->driver->driver.name, 5);
#endif
		goto err_out_critical;
	}

	atomic_set(&ts->device_init, 1);
	atomic_set(&dev_state,DEV_RESUME);

	if (touch_drv->init == NULL) {
		TOUCH_INFO_MSG("There is no specific IC init function\n");
		goto err_out_critical;
	}

#ifndef IS_MTK
	if (gpio_is_valid(ts_pdata->sda_pin))
		pinstate[0] = gpio_get_value(ts_pdata->sda_pin);

	if (gpio_is_valid(ts_pdata->scl_pin))
		pinstate[1] = gpio_get_value(ts_pdata->scl_pin);

	if (gpio_is_valid(ts_pdata->int_pin))
		pinstate[2] = gpio_get_value(ts_pdata->int_pin);
#else
//	if (mt_get_gpio_dir(ts_pdata->sda_pin))
		pinstate[0] = mt_get_gpio_pull_select(ts_pdata->sda_pin);

//	if (mt_get_gpio_dir(ts_pdata->scl_pin))
		pinstate[1] = mt_get_gpio_pull_select(ts_pdata->scl_pin);

//	if (mt_get_gpio_dir(ts_pdata->int_pin))
		pinstate[2] = mt_get_gpio_pull_select(ts_pdata->int_pin);

#endif

	if (pinstate[0] == 0 || pinstate[1] == 0 || pinstate[2] == 0)
		TOUCH_INFO_MSG("pin state [sda:%d, scl:%d, int:%d]\n", pinstate[0], pinstate[1], pinstate[2] );

#if 0
	if (touch_drv->init(ts->client, &ts->fw_info) < 0) {
		TOUCH_ERR_MSG("specific device initialization fail\n");
		goto err_out_retry;
	}
#endif

#ifdef LGE_TOUCH_GHOST_DETECTION
	if (ts_role->ghost_detection_enable) {
		/* force continuous mode after IC init  */
		if (touch_drv->ic_ctrl) {
			TOUCH_INFO_MSG("force continuous mode !!!\n");
			if (touch_drv->ic_ctrl(ts->client, IC_CTRL_REPORT_MODE, 0) < 0) {
				TOUCH_ERR_MSG("IC_CTRL_BASELINE handling fail\n");
				goto err_out_retry;
			}
			force_continuous_mode = 1;
		}
		trigger_baseline = 0;
		ghost_detection = 0;
		ghost_detection_count = 0;
		do_gettimeofday(&t_ex_debug[TIME_EX_INIT_TIME]);

		ts->gf_ctrl.count = 0;
		ts->gf_ctrl.ghost_check_count = 0;
		ts->gf_ctrl.saved_x = -1;
		ts->gf_ctrl.saved_y = -1;

		if (ts->gf_ctrl.probe) {
			ts->gf_ctrl.stage = GHOST_STAGE_1;
			if (touch_drv->ic_ctrl) {
				if (touch_drv->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_OPEN) < 0) {
					TOUCH_ERR_MSG("IC_CTRL_BASELINE handling fail\n");
					goto err_out_retry;
				}
			}
		} else {
			if (ts->gf_ctrl.stage & GHOST_STAGE_2) {
				ts->gf_ctrl.stage = GHOST_STAGE_1 | GHOST_STAGE_2 | GHOST_STAGE_4;
				ts->gf_ctrl.ghost_check_count = MAX_GHOST_CHECK_COUNT - 1;
				if (touch_drv->ic_ctrl) {
					if (touch_drv->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_OPEN) < 0) {
						TOUCH_ERR_MSG("IC_CTRL_BASELINE handling fail\n");
						goto err_out_retry;
					}
				}
			}
			else {
				ts->gf_ctrl.stage = GHOST_STAGE_3;
				if (touch_drv->ic_ctrl) {
					if (touch_drv->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_FIX) < 0) {
						TOUCH_ERR_MSG("IC_CTRL_BASELINE handling fail\n");
						goto err_out_retry;
					}
				}
			}
		}
	}
#endif

	if (unlikely(touch_debug_mask_ & (DEBUG_BASE_INFO))) {
		touch_drv->ic_ctrl(ts->client, IC_CTRL_TESTMODE_VERSION_SHOW, 0);
	}

	ts->gf_ctrl.probe = 0;

	memset(&ts->ts_data, 0, sizeof(ts->ts_data));
	ts->fw_info.is_downloading = 0;
	ts->ic_init_err_cnt = 0;
	return 0;

#ifdef LGE_TOUCH_GHOST_DETECTION
err_out_retry:
	safety_reset(ts);
	ts->ic_init_err_cnt++;
	queue_delayed_work(touch_wq, &ts->work_init, msecs_to_jiffies(20));
	return 0;
#endif

err_out_critical:
	ts->ic_init_err_cnt = 0;

	return -1;
}

/* ghost_finger_solution
 *
 * GHOST_STAGE_1
 * - melt_mode.
 * - If user press and release their finger in 1 sec, STAGE_1 will be cleared. --> STAGE_2
 * - If there is no key-guard, ghost_finger_solution is finished.
 *
 * GHOST_STAGE_2
 * - no_melt_mode
 * - if user use multi-finger, stage will be changed to STAGE_1
 *   (We assume that ghost-finger occured)
 * - if key-guard is unlocked, STAGE_2 is cleared. --> STAGE_3
 *
 * GHOST_STAGE_3
 * - when user release their finger, device re-scan the baseline.
 * - Then, GHOST_STAGE3 is cleared and ghost_finger_solution is finished.
 */
#define ghost_sub(x, y)	(x > y ? x - y : y - x)

int ghost_finger_solution(struct lge_touch_data *ts)
{
	u8	id = 0;

	for (id = 0; id < ts_caps->max_id; id++) {
		if (ts->ts_data.curr_data[id].status == FINGER_PRESSED) {
			break;
		}
	}

	if (ts->gf_ctrl.stage & GHOST_STAGE_1) {
		if (ts->ts_data.total_num == 0 && ts->ts_data.curr_button.state == 0 && ts->ts_data.palm == 0) {
			if (ts->gf_ctrl.count < ts->gf_ctrl.min_count || ts->gf_ctrl.count >= ts->gf_ctrl.max_count) {
				if (ts->gf_ctrl.stage & GHOST_STAGE_2)
					ts->gf_ctrl.ghost_check_count = MAX_GHOST_CHECK_COUNT - 1;
				else
					ts->gf_ctrl.ghost_check_count = 0;
			}
			else{
				if (ghost_sub(ts->gf_ctrl.saved_x, ts->gf_ctrl.saved_last_x) > ts->gf_ctrl.max_moved ||
				   ghost_sub(ts->gf_ctrl.saved_y, ts->gf_ctrl.saved_last_y) > ts->gf_ctrl.max_moved)
					ts->gf_ctrl.ghost_check_count = MAX_GHOST_CHECK_COUNT;
				else
					ts->gf_ctrl.ghost_check_count++;

				if (unlikely(touch_debug_mask_ & DEBUG_GHOST))
					TOUCH_INFO_MSG("ghost_stage_1: delta[%d/%d/%d]\n",
						ghost_sub(ts->gf_ctrl.saved_x, ts->gf_ctrl.saved_last_x),
						ghost_sub(ts->gf_ctrl.saved_y, ts->gf_ctrl.saved_last_y),
						ts->gf_ctrl.max_moved);
			}

			if (unlikely(touch_debug_mask_ & DEBUG_GHOST || touch_debug_mask_ & DEBUG_BASE_INFO))
				TOUCH_INFO_MSG("ghost_stage_1: ghost_check_count+[0x%x]\n", ts->gf_ctrl.ghost_check_count);

			if (ts->gf_ctrl.ghost_check_count >= MAX_GHOST_CHECK_COUNT) {
				ts->gf_ctrl.ghost_check_count = 0;
				if (touch_drv->ic_ctrl) {
					if (touch_drv->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_FIX) < 0)
						return -1;
				}
				ts->gf_ctrl.stage &= ~GHOST_STAGE_1;
				if (unlikely(touch_debug_mask_ & DEBUG_GHOST|| touch_debug_mask_ & DEBUG_BASE_INFO))
					TOUCH_INFO_MSG("ghost_stage_1: cleared[0x%x]\n", ts->gf_ctrl.stage);
				if (!ts->gf_ctrl.stage) {
					if (unlikely(touch_debug_mask_ & DEBUG_GHOST))
						TOUCH_INFO_MSG("ghost_stage_finished. (NON-KEYGUARD)\n");
				}
			}
			ts->gf_ctrl.count = 0;
			ts->gf_ctrl.saved_x = -1;
			ts->gf_ctrl.saved_y = -1;
		}
		else if (ts->ts_data.total_num == 1 && ts->ts_data.curr_button.state == 0
				&& id == 0 && ts->ts_data.palm == 0) {
			if (ts->gf_ctrl.saved_x == -1 && ts->gf_ctrl.saved_x == -1) {
				ts->gf_ctrl.saved_x = ts->ts_data.curr_data[id].x_position;
				ts->gf_ctrl.saved_y = ts->ts_data.curr_data[id].y_position;
			}
			ts->gf_ctrl.count++;
			if (unlikely(touch_debug_mask_ & DEBUG_GHOST))
				TOUCH_INFO_MSG("ghost_stage_1: int_count[%d/%d]\n", ts->gf_ctrl.count, ts->gf_ctrl.max_count);
		}
		else {
			if (unlikely(touch_debug_mask_ & DEBUG_GHOST) && ts->gf_ctrl.count != ts->gf_ctrl.max_count)
				TOUCH_INFO_MSG("ghost_stage_1: Not good condition. total[%d] button[%d] id[%d] palm[%d]\n",
						ts->ts_data.total_num, ts->ts_data.curr_button.state,
						id, ts->ts_data.palm);
			ts->gf_ctrl.count = ts->gf_ctrl.max_count;
		}
	}
	else if (ts->gf_ctrl.stage & GHOST_STAGE_2) {
		if (ts->ts_data.total_num > 1 || (ts->ts_data.total_num == 1 && ts->ts_data.curr_button.state)) {
			ts->gf_ctrl.stage |= GHOST_STAGE_1;
			ts->gf_ctrl.ghost_check_count = MAX_GHOST_CHECK_COUNT - 1;
			ts->gf_ctrl.count = 0;
			ts->gf_ctrl.saved_x = -1;
			ts->gf_ctrl.saved_y = -1;
			if (touch_drv->ic_ctrl) {
				if (touch_drv->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_OPEN) < 0)
					return -1;
#ifdef CUST_G_TOUCH
//do nothing
#else
				if (touch_drv->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_REBASE) < 0)
					return -1;
#endif
			}
			if (unlikely(touch_debug_mask_ & DEBUG_GHOST))
				TOUCH_INFO_MSG("ghost_stage_2: multi_finger. return to ghost_stage_1[0x%x]\n", ts->gf_ctrl.stage);
		}
	}
	else if (ts->gf_ctrl.stage & GHOST_STAGE_3) {
		if (ts->ts_data.total_num == 0 && ts->ts_data.curr_button.state == 0 && ts->ts_data.palm == 0) {
			ts->gf_ctrl.ghost_check_count++;

			if (unlikely(touch_debug_mask_ & DEBUG_GHOST || touch_debug_mask_ & DEBUG_BASE_INFO))
				TOUCH_INFO_MSG("ghost_stage_3: ghost_check_count+[0x%x]\n", ts->gf_ctrl.ghost_check_count);

			if (ts->gf_ctrl.ghost_check_count >= MAX_GHOST_CHECK_COUNT) {
				ts->gf_ctrl.stage &= ~GHOST_STAGE_3;
				if (unlikely(touch_debug_mask_ & DEBUG_GHOST || touch_debug_mask_ & DEBUG_BASE_INFO))
					TOUCH_INFO_MSG("ghost_stage_3: cleared[0x%x]\n", ts->gf_ctrl.stage);
				if (!ts->gf_ctrl.stage) {
					if (unlikely(touch_debug_mask_ & DEBUG_GHOST))
						TOUCH_INFO_MSG("ghost_stage_finished. (NON-KEYGUARD)\n");
				}
			}
		}
		else if (ts->ts_data.total_num == 1 && ts->ts_data.curr_button.state == 0
				&& id == 0 && ts->ts_data.palm == 0);
		else{
			ts->gf_ctrl.stage &= ~GHOST_STAGE_3;
			ts->gf_ctrl.stage |= GHOST_STAGE_1;
			ts->gf_ctrl.ghost_check_count = 0;
			ts->gf_ctrl.count = 0;
			ts->gf_ctrl.saved_x = -1;
			ts->gf_ctrl.saved_y = -1;
			if (touch_drv->ic_ctrl) {
				if (touch_drv->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_OPEN) < 0)
					return -1;
			}

			if (unlikely(touch_debug_mask_ & DEBUG_GHOST)) {
				TOUCH_INFO_MSG("ghost_stage_3: Not good condition. total[%d] button[%d] id[%d] palm[%d]\n",
						ts->ts_data.total_num, ts->ts_data.curr_button.state,
						id, ts->ts_data.palm);
				TOUCH_INFO_MSG("ghost_stage_3: return to ghost_stage_1[0x%x]\n", ts->gf_ctrl.stage);
			}
		}
	}
	else if (ts->gf_ctrl.stage & GHOST_STAGE_4) {
		if (ts->ts_data.total_num == 0 && ts->ts_data.curr_button.state == 0 && ts->ts_data.palm == 0) {
#ifdef CUST_G_TOUCH
//do nothing
#else
			if (touch_drv->ic_ctrl) {
				if (touch_drv->ic_ctrl(ts->client, IC_CTRL_BASELINE, BASELINE_REBASE) < 0)
					return -1;
			}
#endif
			ts->gf_ctrl.stage = GHOST_STAGE_CLEAR;
			if (unlikely(touch_debug_mask_ & DEBUG_GHOST || touch_debug_mask_ & DEBUG_BASE_INFO))
				TOUCH_INFO_MSG("ghost_stage_4: cleared[0x%x]\n", ts->gf_ctrl.stage);
			if (unlikely(touch_debug_mask_ & DEBUG_GHOST))
				TOUCH_INFO_MSG("ghost_stage_finished. (KEYGUARD)\n");
		}

		if (unlikely(touch_debug_mask_ & DEBUG_GHOST) && ts->ts_data.palm != 0)
			TOUCH_INFO_MSG("ghost_stage_4: palm[%d]\n", ts->ts_data.palm);
	}

	ts->gf_ctrl.saved_last_x = ts->ts_data.curr_data[id].x_position;
	ts->gf_ctrl.saved_last_y = ts->ts_data.curr_data[id].y_position;

	return 0;
}


/* touch_init_func
 *
 * In order to reduce the booting-time,
 * we used delayed_work_queue instead of msleep or mdelay.
 */
static void touch_init_func(struct work_struct *work_init)
{
	struct lge_touch_data *ts =
			container_of(to_delayed_work(work_init), struct lge_touch_data, work_init);

	if (unlikely(touch_debug_mask_ & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	/* Specific device initialization */
	touch_ic_init(ts);
}


/* send_uevent_lpwg
 *
 * It uses wake-lock in order to prevent entering the sleep-state,
 * during recognition or verification.
 */
#define VALID_LPWG_UEVENT_SIZE 3
static char *lpwg_uevent[VALID_LPWG_UEVENT_SIZE][2] = {
	{"TOUCH_GESTURE_WAKEUP=WAKEUP", NULL},
	{"TOUCH_GESTURE_WAKEUP=PASSWORD", NULL},
	{"TOUCH_GESTURE_WAKEUP=SIGNATURE", NULL}
};

void touch_send_wakeup(void *info)
{
	struct lge_touch_data *ts = (struct lge_touch_data *)info;

	wake_lock_timeout(&touch_wake_lock, msecs_to_jiffies(3000));

	TOUCH_INFO_MSG("Send Wake-up \n");
	if (ts->pdata->send_lpwg == LPWG_DOUBLE_TAP) {
		kobject_uevent_env(&device_touch.kobj, KOBJ_CHANGE, lpwg_uevent[0]);
		TOUCH_DEBUG_MSG( "uevent[%s]\n", lpwg_uevent[0][0]);
	} else if (ts->pdata->send_lpwg == LPWG_MULTI_TAP) {
	     kobject_uevent_env(&device_touch.kobj, KOBJ_CHANGE, lpwg_uevent[1]);
		TOUCH_DEBUG_MSG( "uevent[%s]\n", lpwg_uevent[1][0]);
	} else {
		TOUCH_DEBUG_MSG("error send mode = 0x%x\n", ts->pdata->send_lpwg);
	}
}

static void touch_gesture_wakeup_func(struct lge_touch_data *ts)
{
	TOUCH_INFO_MSG("%s \n", __func__);

	if (ts->fw_info.is_downloading == UNDER_DOWNLOADING){
		TOUCH_INFO_MSG("%s is not executed \n", __func__);
		return;
	}

	mutex_lock(&i2c_suspend_lock);

	if (touch_drv->data(ts->client, &ts->ts_data) < 0) {
		TOUCH_INFO_MSG("%s get data fail \n", __func__);
		goto ignore_interrupt;
	}

	if (ts->pdata->send_lpwg) {
		touch_send_wakeup(ts);
		ts->pdata->send_lpwg = 0;
	}

ignore_interrupt:
	mutex_unlock(&i2c_suspend_lock);
	return;
}

static void touch_lock_func(struct work_struct *work_touch_lock)
{
	struct lge_touch_data *ts =
			container_of(to_delayed_work(work_touch_lock), struct lge_touch_data, work_touch_lock);

	if (unlikely(touch_debug_mask_ & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	ts->ts_data.state = DO_NOT_ANYTHING;
}

/* check_log_finger_changed
 *
 * Check finger state change for Debug
 */
static void check_log_finger_changed(struct lge_touch_data *ts, u8 total_num)
{
	u16 tmp_p = 0;
	u16 tmp_r = 0;
	u16 id = 0;

#if 0
	TOUCH_INFO_MSG("%s %d ts->ts_data.prev_total_num=%d total_num=%d\n", __func__, __LINE__, ts->ts_data.prev_total_num, total_num);


	if (ts->ts_data.prev_total_num != total_num) {
		/* Finger added */
		if (ts->ts_data.prev_total_num <= total_num) {
			for(id=0; id < ts_caps->max_id; id++) {
				if (ts->ts_data.curr_data[id].status == FINGER_PRESSED
						&& ts->ts_data.prev_data[id].status == FINGER_RELEASED) {
					break;
				}
			}
			TOUCH_INFO_MSG("%d finger pressed : <%d> x[%4d] y[%4d] z[%3d]\n",
					total_num, id,
					ts->ts_data.curr_data[id].x_position,
					ts->ts_data.curr_data[id].y_position,
					ts->ts_data.curr_data[id].pressure);
		} else {
		/* Finger subtracted */
			TOUCH_INFO_MSG("%d finger pressed\n", total_num);
		}
	}
#endif

	if (ts->ts_data.prev_total_num == total_num && total_num == 1) {
		/* Finger changed at one finger status - IC bug check */
		for (id = 0, tmp_p = 0; id < ts_caps->max_id; id++) {
			/* find pressed */
			if (ts->ts_data.curr_data[id].status == FINGER_PRESSED
					&& ts->ts_data.prev_data[id].status == FINGER_RELEASED) {
				tmp_p = id;
			}
			/* find released */
			if (ts->ts_data.curr_data[id].status == FINGER_RELEASED
					&& ts->ts_data.prev_data[id].status == FINGER_PRESSED) {
				tmp_r = id;
			}
		}

		if (tmp_p != tmp_r
				&& (ts->ts_data.curr_data[tmp_p].status
						!= ts->ts_data.prev_data[tmp_p].status)) {
			TOUCH_INFO_MSG("%d finger changed : <%d -> %d> x[%4d] y[%4d] z[%3d]\n",
						total_num, tmp_r, tmp_p,
						ts->ts_data.curr_data[id].x_position,
						ts->ts_data.curr_data[id].y_position,
						ts->ts_data.curr_data[id].pressure);
		}
	}
}

/* check_log_finger_released
 *
 * Check finger state change for Debug
 */
#if 0
static void check_log_finger_released(struct lge_touch_data *ts)
{
	u16 id = 0;

	for (id = 0; id < ts_caps->max_id; id++) {
		if (ts->ts_data.prev_data[id].status == FINGER_PRESSED) {
			break;
		}
	}

	TOUCH_INFO_MSG("touch_release[%s] : <%d> x[%4d] y[%4d]\n",
			ts->ts_data.palm?"Palm":"", id,
			ts->ts_data.prev_data[id].x_position,
			ts->ts_data.prev_data[id].y_position);
}
#endif

/* touch_work_pre_proc
 *
 * Pre-process work at touch_work
 */
static int touch_work_pre_proc(struct lge_touch_data *ts)
{
	int result = 0;

	ts->ts_data.total_num = 0;

	if (unlikely(ts->work_sync_err_cnt >= MAX_RETRY_COUNT)) {
		TOUCH_ERR_MSG("Work Sync Failed: Irq-pin has some unknown problems\n");
		return -EIO;
	}

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_WORKQUEUE_START]);
#endif
	TOUCH_TRACE_FUNC();

	result = touch_drv->data(ts->client, &ts->ts_data);

	if (result == -EIO) {
		TOUCH_ERR_MSG("get data fail\n");
		goto ignore_interrupt;
	}
	else if (result == -ENXIO) {
		if (++ts->ic_error_cnt > MAX_RETRY_COUNT) {
			TOUCH_INFO_MSG("Error cannot recover \n");
			touch_disable(ts->client->irq);
			release_all_ts_event(ts);
			touch_power_cntl(ts, POWER_OFF);
			return -ENXIO;
		}
		TOUCH_INFO_MSG("Error(%d) detected. Retry(%d/%d) \n", ts->ts_data.state, ts->ic_error_cnt, MAX_RETRY_COUNT);
#if defined(CONFIG_LGE_LCD_ESD)
		if (ts->pdata->panel_on && ts->ts_data.state == 0) {
			TOUCH_INFO_MSG("LCD Reset \n");
			mdss_lcd_esd_reset();
		}
#endif
		return -EIO;
	} else {
		ts->ic_error_cnt = 0;
	}

	/* Ghost finger solution */
	if (likely(ts_role->ghost_finger_solution_enable) && unlikely(ts->gf_ctrl.stage)) {
		if (ghost_finger_solution(ts)) {
			TOUCH_ERR_MSG("ghost_finger_solution was failed\n");
			return -EIO;
		}
	}

	return 0;

ignore_interrupt:
	return -EIO;

}

/* touch_work_post_proc
 *
 * Post-process work at touch_work
 */
static void touch_work_post_proc(struct lge_touch_data *ts, int post_proc)
{

	if (post_proc >= WORK_POST_MAX)
		return;

	switch (post_proc) {
	case WORK_POST_OUT:
#ifdef LGE_TOUCH_TIME_DEBUG
		do_gettimeofday(&t_debug[TIME_WORKQUEUE_END]);
		time_profile_result(ts);
#endif

		ts->work_sync_err_cnt = 0;
		post_proc = WORK_POST_COMPLATE;
		break;

	case WORK_POST_ERR_RETRY:
		ts->work_sync_err_cnt++;
		safety_reset(ts);
		touch_ic_init(ts);
		post_proc = WORK_POST_COMPLATE;
		break;

	case WORK_POST_ERR_CIRTICAL:
		ts->work_sync_err_cnt = 0;
		safety_reset(ts);
		touch_ic_init(ts);
		post_proc = WORK_POST_COMPLATE;
		break;

	default:
		post_proc = WORK_POST_COMPLATE;
		break;
	}

	if (post_proc != WORK_POST_COMPLATE)
		touch_work_post_proc(ts, post_proc);
}

/* touch_work_func_a
 *
 * HARD_TOUCH_KEY
 */
static void touch_work_func_a(struct lge_touch_data *ts)
{
	u8 report_enable = 0;
	int ret = 0;

	ret = touch_work_pre_proc(ts);
	if (ret == -EIO)
		goto err_out_critical;
	else if (ret == -EAGAIN)
		goto out;
	else if(ret == -ENXIO)
		goto power_off;

#ifdef LGE_TOUCH_GHOST_DETECTION
	/* Ghost detection solution */
	if (ts_role->ghost_detection_enable) {
		ret = ghost_detect_solution(ts);
		if (ret == NEED_TO_OUT)
			goto out;
		else if (ret == NEED_TO_INIT)
			goto err_out_init;
	}
#endif

	/* Palm handle */
	if (ts->ts_data.palm) {
		release_all_ts_event(ts);
		cancel_delayed_work_sync(&ts->work_touch_lock);
		goto out;
	}

	/* Finger handle */
	if (!ts->ts_data.total_num) {
		touch_asb_input_report(ts, FINGER_RELEASED);
		report_enable = 1;

		queue_delayed_work(touch_wq, &ts->work_touch_lock, msecs_to_jiffies(200));
		ts->ts_data.prev_total_num = 0;
	} else if (ts->ts_data.total_num <= ts_caps->max_id) {
		cancel_delayed_work_sync(&ts->work_touch_lock);

		if (ts->gf_ctrl.stage == GHOST_STAGE_CLEAR
				|| (ts->gf_ctrl.stage | GHOST_STAGE_1)
				|| ts->gf_ctrl.stage == GHOST_STAGE_4) {
			ts->ts_data.state = TOUCH_BUTTON_LOCK;
		}

		/* key button cancel */
		if (ts->ts_data.prev_button.state == BUTTON_PRESSED && ts->ts_data.state == TOUCH_BUTTON_LOCK) {
			input_report_key(ts->input_dev, ts->ts_data.prev_button.key_code, BUTTON_CANCELED);

			if (likely(touch_debug_mask_ & (DEBUG_BUTTON | DEBUG_BASE_INFO)))
				TOUCH_INFO_MSG("KEY[%s:%3d] is canceled\n",
						get_touch_button_string(ts->ts_data.prev_button.key_code),
						ts->ts_data.prev_button.key_code);

			memset(&ts->ts_data.prev_button, 0x0, sizeof(ts->ts_data.prev_button));
		}

		if (likely(touch_debug_mask_ & (DEBUG_BASE_INFO | DEBUG_ABS)))
			check_log_finger_changed(ts, ts->ts_data.total_num);

		ts->ts_data.prev_total_num = ts->ts_data.total_num;

		touch_asb_input_report(ts, FINGER_PRESSED);
		report_enable = 1;

		memcpy(ts->ts_data.prev_data, ts->ts_data.curr_data, sizeof(ts->ts_data.curr_data));
	}

	if (report_enable)
		input_sync(ts->input_dev);

	/* Button handle */
	if (ts->ts_data.state != TOUCH_BUTTON_LOCK) {
		/* do not check when there is no pressed button at error case
		 * 	- if you check it, sometimes touch is locked becuase button pressed via IC error.
		 */

		if (ts->work_sync_err_cnt > 0
				&& ts->ts_data.prev_button.state == BUTTON_RELEASED) {
			/* Do nothing */
		} else {
			report_enable = 0;

			if (unlikely(touch_debug_mask_ & DEBUG_BUTTON))
				TOUCH_INFO_MSG("Cur. button -code: %d state: %d, Prev. button -code: %d state: %d\n",
						ts->ts_data.curr_button.key_code,
						ts->ts_data.curr_button.state,
						ts->ts_data.prev_button.key_code,
						ts->ts_data.prev_button.state);

			if (ts->ts_data.curr_button.state == BUTTON_PRESSED
					&& ts->ts_data.prev_button.state == BUTTON_RELEASED) {
				/* button pressed */
				cancel_delayed_work_sync(&ts->work_touch_lock);

				input_report_key(ts->input_dev, ts->ts_data.curr_button.key_code, BUTTON_PRESSED);

				if (likely(touch_debug_mask_ & (DEBUG_BUTTON | DEBUG_BASE_INFO)))
					TOUCH_INFO_MSG("KEY[%s:%3d] is pressed\n",
							get_touch_button_string(ts->ts_data.curr_button.key_code),
							ts->ts_data.curr_button.key_code);

				memcpy(&ts->ts_data.prev_button, &ts->ts_data.curr_button,
						sizeof(ts->ts_data.curr_button));

				report_enable = 1;
			}else if (ts->ts_data.curr_button.state == BUTTON_PRESSED
					&& ts->ts_data.prev_button.state == BUTTON_PRESSED
					&& ts->ts_data.prev_button.key_code != ts->ts_data.curr_button.key_code) {
				/* exception case - multi press button handle */
				queue_delayed_work(touch_wq, &ts->work_touch_lock, msecs_to_jiffies(200));

				/* release previous pressed button */
				input_report_key(ts->input_dev, ts->ts_data.prev_button.key_code, BUTTON_RELEASED);

				ts->ts_data.prev_button.state = BUTTON_RELEASED;

				if (likely(touch_debug_mask_ & (DEBUG_BUTTON | DEBUG_BASE_INFO)))
					TOUCH_INFO_MSG("KEY[%s:%3d] is released\n",
							get_touch_button_string(ts->ts_data.prev_button.key_code),
							ts->ts_data.prev_button.key_code);

				report_enable = 1;
			} else if (ts->ts_data.curr_button.state == BUTTON_RELEASED /* button released */
					&& ts->ts_data.prev_button.state == BUTTON_PRESSED
					&& ts->ts_data.prev_button.key_code == ts->ts_data.curr_button.key_code) {
				/* button release */
				input_report_key(ts->input_dev, ts->ts_data.prev_button.key_code, BUTTON_RELEASED);

				if (likely(touch_debug_mask_ & (DEBUG_BUTTON | DEBUG_BASE_INFO)))
					TOUCH_INFO_MSG("KEY[%s:%3d] is released\n",
							get_touch_button_string(ts->ts_data.prev_button.key_code),
							ts->ts_data.prev_button.key_code);

				memset(&ts->ts_data.prev_button, 0x0, sizeof(ts->ts_data.prev_button));
				memset(&ts->ts_data.curr_button, 0x0, sizeof(ts->ts_data.curr_button));

				report_enable = 1;
			}

			if (report_enable)
				input_sync(ts->input_dev);
		}
	}

out:
	touch_work_post_proc(ts, WORK_POST_OUT);
	return;

power_off:
	return;

err_out_critical:
	touch_work_post_proc(ts, WORK_POST_ERR_CIRTICAL);
	return;

#ifdef LGE_TOUCH_GHOST_DETECTION
err_out_init:
	touch_work_post_proc(ts, WORK_POST_ERR_CIRTICAL);
	return;
#endif
}

static int touch_request_firmware(struct lge_touch_data *ts)
{
	int ret = 0;
	char *fw_name = ts_pdata->fw_image;

	TOUCH_TRACE_FUNC();

	if (ts->fw_info.path[0])
		fw_name = ts->fw_info.path;

#ifndef IS_MTK
	TOUCH_INFO_MSG("request_firmware() %s \n", fw_name);
//	TOUCH_INFO_MSG("%s %d] fw_upgrade_completion : %d\n",__func__, __LINE__, completion_done(&ts->fw_upgrade_completion));

	if ((ret = request_firmware_mod((const struct firmware **) (&ts->fw_info.fw), fw_name, &ts->client->dev)) != 0) {
		ts->fw_info.fw = NULL;
		TOUCH_ERR_MSG("request_firmware() failed %d\n", ret);
	}
//	TOUCH_INFO_MSG("%s e] fw_upgrade_completion : %d\n",__func__, completion_done(&ts->fw_upgrade_completion));
#else
	if ( MELFAS_binary == NULL || (size_t)sizeof(MELFAS_binary) < 0)
	{
		TOUCH_ERR_MSG("firmware load failed \n");
		goto out;
	}
	else
	{
		TOUCH_INFO_MSG("MELFAS_binary = 0x%X, (size_t)sizeof(MELFAS_binary) = 0x%X. the data present hex unit", (int)MELFAS_binary, (int)sizeof(MELFAS_binary));

		ts->fw_info.fw =  kzalloc(sizeof(struct firmware), GFP_KERNEL);
		if (!ts->fw_info.fw) {
			TOUCH_ERR_MSG("%s: kmalloc(struct firmware) failed\n", __func__);
			return ERR_PTR(-ENOMEM);
		}

		ts->fw_info.fw->data = (u8*) &MELFAS_binary[0];
		ts->fw_info.fw->size = (size_t)sizeof(MELFAS_binary);

		if ( (u16)ts->fw_info.fw->size != MELFAS_binary_nLength )
		{
			TOUCH_ERR_MSG("%s: MELFAS_binary_nLength incorrect \n", __func__);		
			ret = 1;
			goto out;
		}
		else{
			TOUCH_INFO_MSG(" ts->fw_info.fw = 0x%X, ts->fw_info.fw->size = 0x%X. the data present hex unit", (int)ts->fw_info.fw->data, (int)ts->fw_info.fw->size);
		}

	}
out:
#endif //IS_MTK
	return ret;
}

static int touch_release_firmware(struct lge_touch_data *ts)
{
	TOUCH_TRACE_FUNC();

	if(ts->fw_info.fw) {
		release_firmware((const struct firmware *) ts->fw_info.fw);
	}

	ts->fw_info.fw = NULL;

	return 0;
}

/* touch_fw_upgrade_func
 *
 * it used to upgrade the firmware of touch IC.
 */
static void touch_fw_upgrade_func(struct work_struct *work_fw_upgrade)
{
	struct lge_touch_data *ts =
			container_of(work_fw_upgrade, struct lge_touch_data, work_fw_upgrade);
	u8	saved_state = ts->pdata->curr_pwr_state;

	TOUCH_TRACE_FUNC();

	if (touch_drv->fw_upgrade == NULL) {
		TOUCH_INFO_MSG("There is no specific firmware upgrade function\n");
		goto out;
	}

	ts->fw_info.is_downloading = UNDER_DOWNLOADING;
	power_lock(POWER_FW_UP_LOCK);

	if (touch_request_firmware(ts)) {
		TOUCH_INFO_MSG("FW-upgrade is not excuted\n");
		goto out;
	}

#ifdef LGE_TOUCH_GHOST_DETECTION
	if (ts_role->ghost_detection_enable) {
		hrtimer_cancel(&ts->trigger_timer);
	}
#endif

	if (ts->pdata->curr_pwr_state == POWER_OFF) {
		touch_power_cntl(ts, POWER_ON);
	}

	if (likely(touch_debug_mask_ & (DEBUG_FW_UPGRADE | DEBUG_BASE_INFO)))
		TOUCH_INFO_MSG("F/W Upgrade - Start \n");

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_FW_UPGRADE_START]);
#endif
	if (touch_drv->fw_upgrade(ts->client, &ts->fw_info) < 0) { //start touch f/w update
		TOUCH_INFO_MSG("F/W Upgrade - Failed \n");
		goto err_out;
	}

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_FW_UPGRADE_END]);
#endif

	safety_reset(ts);
	touch_ic_init(ts);

	touch_drv->ic_ctrl(ts->client, IC_CTRL_SAVE_IC_INFO, 0);
	if (saved_state != POWER_ON)
		touch_power_cntl(ts, saved_state);

	if (likely(touch_debug_mask_ & (DEBUG_FW_UPGRADE |DEBUG_BASE_INFO)))
		TOUCH_INFO_MSG("F/W Upgrade - Finish \n");

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_FW_UPGRADE_END]);

	if (touch_time_debug_mask & DEBUG_TIME_FW_UPGRADE
			|| touch_time_debug_mask & DEBUG_TIME_PROFILE_ALL) {
		TOUCH_INFO_MSG("FW upgrade time is under %3lu.%06lusec\n",
				get_time_interval(t_debug[TIME_FW_UPGRADE_END].tv_sec, t_debug[TIME_FW_UPGRADE_START].tv_sec),
				get_time_interval(t_debug[TIME_FW_UPGRADE_END].tv_usec, t_debug[TIME_FW_UPGRADE_START].tv_usec));
	}
#endif

	goto out;

err_out:
	power_unlock(POWER_FW_UP_LOCK);
	safety_reset(ts);
	touch_ic_init(ts);

out:
#ifndef IS_MTK
	/* Specific device resolution */
	touch_release_firmware(ts);
#endif

	TOUCH_TRACE_FUNC();
	if (touch_drv->resolution) {
		if (touch_drv->resolution(ts->client) < 0) {
			TOUCH_ERR_MSG("specific device resolution fail\n");
		}
	}
	ts->fw_info.is_downloading = DOWNLOAD_COMPLETE;
	complete(&ts->fw_upgrade_completion);
	power_unlock(POWER_FW_UP_LOCK);
	return;
}


static void inspection_crack_func(struct work_struct *work_crack)
{
	struct lge_touch_data *ts =
			container_of(to_delayed_work(work_crack), struct lge_touch_data, work_crack);
	int iterator = 0;
	int retry_count = 3;
	char *buf = NULL;

	TOUCH_TRACE_FUNC();
	atomic_set(&ts->crack_test_state, CRACK_TEST_START);

	do {
		iterator++;
		if (ts->fw_info.is_downloading != UNDER_DOWNLOADING) {
			window_crack_check = touch_drv->inspection_crack(ts->client);
			TOUCH_INFO_MSG("%s window_crack_check = %d, count = %d\n", __func__, window_crack_check, iterator);
		}
	} while ( (window_crack_check == CRACK) && (iterator < retry_count) );

	safety_reset(ts);
	touch_ic_init(ts);

	if(ts->pdata->panel_on == POWER_ON || ts->pdata->panel_on == POWER_WAKE) {
		buf = kzalloc(PAGE_SIZE,GFP_KERNEL);
		TOUCH_INFO_MSG("%s Check Rawdata\n", __func__);
		touch_drv->sysfs(ts->client, buf, 0, SYSFS_RAWDATA_SHOW);
		kfree(buf);
	} else {
		TOUCH_INFO_MSG("If you want to check raw data, please turn on the LCD.\n");
	}

	atomic_set(&ts->crack_test_state, CRACK_TEST_FINISH);
}

#ifndef IS_MTK
/* touch_irq_handler
 *
 * When Interrupt occurs, it will be called before touch_thread_irq_handler.
 *
 * return
 * IRQ_HANDLED: touch_thread_irq_handler will not be called.
 * IRQ_WAKE_THREAD: touch_thread_irq_handler will be called.
 */
static irqreturn_t touch_irq_handler(int irq, void *dev_id)
{
	struct lge_touch_data *ts = (struct lge_touch_data *)dev_id;

	if (unlikely(atomic_read(&ts->device_init) != 1) && atomic_read(&dev_state) == DEV_RESUME) {
		return IRQ_HANDLED;
	}

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_ISR_START]);
#endif

	return IRQ_WAKE_THREAD;
}

/* touch_thread_irq_handler
 *
 * 1. disable irq.
 * 2. enqueue the new work.
 * 3. enalbe irq.
 */
static irqreturn_t touch_thread_irq_handler(int irq, void *dev_id)
{
	struct lge_touch_data *ts = (struct lge_touch_data *)dev_id;

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_THREAD_ISR_START]);
#endif

	INIT_COMPLETION(ts->irq_completion);

	if (ts->pdata->panel_on == 0 && ts->pdata->lpwg_mode) {
		touch_gesture_wakeup_func(ts);
	} else {
		touch_work_func_a(ts);
	}

	complete_all(&ts->irq_completion);

	return IRQ_HANDLED;
}


#else //////IS_MTK


/* touch_irq_handler
 *
 * When Interrupt occurs, it will be called before touch_thread_irq_handler.
 *
 * return
 * IRQ_HANDLED: touch_thread_irq_handler will not be called.
 * IRQ_WAKE_THREAD: touch_thread_irq_handler will be called.
 */
 #if 0
static irqreturn_t touch_irq_handler(int irq, void *dev_id) 
{
	struct lge_touch_data *ts = (struct lge_touch_data *)dev_id;

	TOUCH_TRACE_FUNC();
	touch_disable (CUST_EINT_TOUCH_PANEL_NUM );
	if (unlikely(atomic_read(&ts->device_init) != 1) && atomic_read(&dev_state) == DEV_RESUME) {
		TOUCH_ERR_MSG("zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz\n");
		return IRQ_HANDLED;
	}

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_ISR_START]);
#endif
	tpd_flag = 1;
	wake_up_interruptible ( &tpd_waiter );
	return IRQ_HANDLED;
}
 #else
 static void touch_irq_handler ( void )
{
	TOUCH_TRACE_FUNC();
	touch_disable (CUST_EINT_TOUCH_PANEL_NUM );
//	TPD_DEBUG_PRINT_INT;

	tpd_flag = 1;
	wake_up_interruptible ( &tpd_waiter );
}
#endif

/* touch_thread_irq_handler
 *
 * 1. disable irq.
 * 2. enqueue the new work.
 * 3. enalbe irq.
 */
static int touch_thread_irq_handler ( void * dev_id )
{
	struct lge_touch_data *ts = (struct lge_touch_data *)dev_id;
	struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_THREAD_ISR_START]);
#endif

	sched_setscheduler ( current, SCHED_RR, &param );
	TOUCH_TRACE_FUNC();
	do
	{
		set_current_state ( TASK_INTERRUPTIBLE );
		wait_event_interruptible ( tpd_waiter, tpd_flag != 0 );

		tpd_flag = 0;
		set_current_state ( TASK_RUNNING );

		mutex_lock ( &i2c_access );
		TOUCH_TRACE_FUNC();
		
		if ( ts == NULL ) 
			TOUCH_ERR_MSG("ts is NULL\n");
		else if ( ts  != dev_id ) 
			TOUCH_ERR_MSG("it is different between ts and dev_id\n");

		if (unlikely(atomic_read(&ts->device_init) != 1) && atomic_read(&dev_state) == DEV_RESUME) {
			TOUCH_ERR_MSG("return IRQ_HANDLED becasue of the value that ts->device_init:[%d] dev_state:[%d] \n", atomic_read(&ts->device_init), atomic_read(&dev_state));
//			return IRQ_HANDLED;
		}

		if (ts->pdata->panel_on == 0 && ts->pdata->lpwg_mode) {
				touch_gesture_wakeup_func(ts);
			} else {
				touch_work_func_a(ts);
			TOUCH_TRACE_FUNC();
		}
exit_work:
		mutex_unlock ( &i2c_access );
		touch_enable ( ts->client->irq );
	} while ( !kthread_should_stop () );

	return 0;
}



#endif //IS_MTK


/* check_platform_data
 *
 * check-list
 * 1. Null Pointer
 * 2. lcd, touch screen size
 * 3. button support
 * 4. operation mode
 * 5. power module
 * 6. report period
 */
static int check_platform_data(struct i2c_client *client)
{
	int i;
	struct lge_touch_data *ts = i2c_get_clientdata(client);

	TOUCH_INFO_MSG("%s \n", __func__);

	if (!ts_pdata)
		return -1;

	if (!ts_caps || !ts_role)
		return -1;

	if (!ts_caps->lcd_x || !ts_caps->lcd_y || !ts_caps->x_max || !ts_caps->y_max) {
		TOUCH_ERR_MSG("lcd_x, lcd_y, x_max, y_max are should be defined\n");
		return -1;
	}

	if (ts_caps->button_support) {
		if (!ts_role->key_type) {
			TOUCH_ERR_MSG("button_support = 1, but key_type is not defined\n");
			return -1;
		}

		if (!ts_caps->y_button_boundary) {
			if (ts_role->key_type == TOUCH_HARD_KEY)
				ts_caps->y_button_boundary = ts_caps->y_max;
			else
				ts_caps->y_button_boundary =
					(ts_caps->lcd_y * ts_caps->x_max) / ts_caps->lcd_x;
		}

		if (ts_caps->button_margin < 0 || ts_caps->button_margin > 49) {
			ts_caps->button_margin = 10;
			TOUCH_ERR_MSG("0 < button_margin < 49, button_margin is set 10 by force\n");
		}
	}
#ifndef IS_MTK
		if (!gpio_is_valid(ts_pdata->int_pin)) {
			TOUCH_ERR_MSG("gpio must be set for interrupt mode\n");
			return -1;
		}

		if (client->irq != gpio_to_irq(ts_pdata->int_pin)) {
			TOUCH_ERR_MSG("warning!! irq[%d] and client->irq[%d] are different\n",
					gpio_to_irq(ts_pdata->int_pin), client->irq);
			client->irq = gpio_to_irq(ts_pdata->int_pin);
		}
#endif //IS_MTK

	if (ts_role->suspend_pwr == POWER_OFF)
		ts_role->resume_pwr = POWER_ON;
	else if (ts_role->suspend_pwr == POWER_SLEEP)
		ts_role->resume_pwr = POWER_WAKE;
	else
		TOUCH_ERR_MSG("suspend_pwr = POWER_OFF or POWER_SLEEP\n");

#ifndef IS_MTK
	for (i = 0; i < TOUCH_PWR_NUM; ++i) {
		if (ts_pwr[i].type == 1 && !gpio_is_valid(ts_pwr[i].value)) {
			TOUCH_ERR_MSG("you should assign gpio for pwr[%d]\n", i);
			return -1;
		} else if (ts_pwr[i].type == 2) {
			if (!ts_pwr[i].name[0]) {
				TOUCH_ERR_MSG("you should assign the supply name for regulator[%d]\n", i);
				return -1;
			}
		}
	}
#endif

	if (ts_caps->max_id > MAX_FINGER)
		ts_caps->max_id = MAX_FINGER;

	return 0;
}


enum touch_dt_type {
	DT_U8,
	DT_U16,
	DT_U32,
	DT_GPIO,
	DT_STRING,
};

struct touch_dt_map {
	const char		*name;
	void			*data;
	enum touch_dt_type	type;
	int			def_value;
};

static char *touch_print_dt_type(enum touch_dt_type type)
{
	static char *str[10] ={
		"DT_U8",
		"DT_U16",
		"DT_U32",
		"DT_GPIO",
		"DT_STRING",
	};

	return str[type];
}
#ifndef IS_MTK
static int touch_parse_dt(struct device *dev, struct touch_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	struct property *prop;

	struct touch_device_caps *caps = pdata->caps;
	struct touch_operation_role *role = pdata->role;
	struct touch_power_info *pwr = pdata->pwr;
	struct touch_limit_value *limit = pdata->limit;

	int i = 0;
	int ret = 0;
	char *tmp = NULL;
	u32 val = 0;
	const __be32 *value = NULL;
	const char *str_index = NULL;
	char power_gpio[16] = {0,};
	struct touch_dt_map *itr = NULL;
	struct touch_dt_map map[] = {
		{ "lge,rst-gpio", &pdata->reset_pin, DT_GPIO, 0xffffffff },
		{ "lge,int-gpio", &pdata->int_pin, DT_GPIO, 0xffffffff },
		{ "lge,sda-gpio", &pdata->sda_pin, DT_GPIO, 0xffffffff },
		{ "lge,scl-gpio", &pdata->scl_pin, DT_GPIO, 0xffffffff },
		{ "lge,id-gpio", &pdata->id_pin, DT_GPIO, 0 },
		{ "lge,id2-gpio", &pdata->id2_pin, DT_GPIO, 0 },

		{ "lge,ic_type", &pdata->ic_type, DT_U8, 0 },
		{ "lge,maker", &pdata->maker, DT_STRING, 0 },
		{ "lge,product", &pdata->fw_product, DT_STRING, 0 },
		{ "lge,fw_image", &pdata->fw_image, DT_STRING, 0 },
		{ "lge,auto_fw_update", &pdata->auto_fw_update, DT_U8, 0 },
		{ "active_area_gap", &pdata->active_area_gap, DT_U8, 0 },

		/* panel status check */
		{ "openshort_enable", &pdata->check_openshort, DT_U8, 0 },

		/*  limitation value */
		{ "raw_data_max", &limit->raw_data_max, DT_U32, 0 },
		{ "raw_data_min", &limit->raw_data_min, DT_U32, 0 },
		{ "raw_data_margin", &limit->raw_data_margin, DT_U32, 0 },
		{ "raw_data_otp_min", &limit->raw_data_otp_min, DT_U32, 0 },
		{ "raw_data_otp_max", &limit->raw_data_otp_max, DT_U32, 0 },
		{ "open_short_min", &limit->open_short_min, DT_U32, 0 },
		{ "slope_max", &limit->slope_max, DT_U32, 0 },
		{ "slope_min", &limit->slope_min, DT_U32, 0 },

		/* caps */
		{ "button_support", &caps->button_support, DT_U8, 0 },
		{ "is_width_supported", &caps->is_width_supported, DT_U8, 0 },
		{ "is_pressure_supported", &caps->is_pressure_supported, DT_U8, 0 },
		{ "is_id_supported", &caps->is_id_supported, DT_U8, 0 },
		{ "max_width", &caps->max_width, DT_U32, 0 },
		{ "max_pressure", &caps->max_pressure, DT_U32, 0 },
		{ "max_id", &caps->max_id, DT_U32, 0 },
		{ "x_max", &caps->x_max, DT_U32, 0 },
		{ "y_max", &caps->y_max, DT_U32, 0 },
		{ "lcd_x", &caps->lcd_x, DT_U32, 0 },
		{ "lcd_y", &caps->lcd_y, DT_U32, 0 },

		/* role */
		{ "key_type", &role->key_type, DT_U8, 0 },
		{ "booting_delay", &role->booting_delay, DT_U32, 0 },
		{ "reset_delay", &role->reset_delay, DT_U32, 0 },
		{ "suspend_pwr", &role->suspend_pwr, DT_U8, 0 },
		{ "resume_pwr", &role->resume_pwr, DT_U8, 0 },
		{ "ghost_detection_enable", &role->ghost_finger_solution_enable, DT_U32, 0 },
		{ "use_crack_mode", &role->use_crack_mode, DT_U32, 0 },

		/* power_info */
		{ "vdd_type0", &pwr[0].type, DT_U8, 0 },
		{ "vdd_name0", &pwr[0].name, DT_STRING, 0 },
		{ "vdd_value0", &pwr[0].value, DT_U32, 0xffffffff },
		{ "vdd_type1", &pwr[1].type, DT_U8, 0 },
		{ "vdd_name1", &pwr[1].name, DT_STRING, 0 },
		{ "vdd_value1", &pwr[1].value, DT_U32, 0xffffffff },
		{ "vdd_type2", &pwr[2].type, DT_U8, 0 },
		{ "vdd_name2", &pwr[2].name, DT_STRING, 0 },
		{ "vdd_value2", &pwr[2].value, DT_U32, 0xffffffff },
		{ NULL, NULL, 0, 0},
	};

	TOUCH_TRACE_FUNC();

	/* reset, irq gpio info */
	if (np == NULL)
		return -ENODEV;

	for (itr = map; itr->name; ++itr) {
		val = 0;
		ret = 0;

		switch (itr->type) {
		case DT_U8:
			if ((ret = of_property_read_u32(np, itr->name, &val)) == 0) {
				*((u8 *) itr->data) = (u8) val;
				if (unlikely(touch_debug_mask_ & DEBUG_CONFIG))
					TOUCH_INFO_MSG("DT entry: %s = %d (%s)\n", itr->name, val, touch_print_dt_type(itr->type));
			} else {
				*((u8 *) itr->data) = (u8) itr->def_value;
			}

			break;

		case DT_U16:
			if ((ret = of_property_read_u32(np, itr->name, &val)) == 0) {
				*((u16 *) itr->data) = (u16) val;
				if (unlikely(touch_debug_mask_ & DEBUG_CONFIG))
					TOUCH_INFO_MSG("DT entry: %s = %d (%s)\n", itr->name, val, touch_print_dt_type(itr->type));
			} else {
				*((u16 *) itr->data) = (u16) itr->def_value;
			}
			break;

		case DT_U32:
			if ((ret = of_property_read_u32(np, itr->name, &val)) == 0) {
				*((u32 *) itr->data) = val;
				if (unlikely(touch_debug_mask_ & DEBUG_CONFIG))
					TOUCH_INFO_MSG("DT entry: %s = %d (%s)\n", itr->name, val, touch_print_dt_type(itr->type));
			} else {
				*((u32 *) itr->data) = (u32) itr->def_value;
			}
			break;

		case DT_STRING:
			tmp = NULL;
			if ((ret = of_property_read_string(np, itr->name, (const char **) &tmp)) == 0) {
				strcpy(itr->data, tmp);
				if (unlikely(touch_debug_mask_ & DEBUG_CONFIG))
					TOUCH_INFO_MSG("DT entry: %s = %s (%s)\n", itr->name, (char *) itr->data, touch_print_dt_type(itr->type));
			} else {
				*((u32 *) itr->data) = 0;
			}
			break;

		case DT_GPIO:
			if ((ret = of_get_named_gpio(np, itr->name, 0)) >= 0) {
				if (unlikely(touch_debug_mask_ & DEBUG_CONFIG))
					TOUCH_INFO_MSG("DT entry: %s = %d (%s)\n", itr->name, ret, touch_print_dt_type(itr->type));
				*((int *) itr->data) = (int) ret;
				ret = 0;
			} else {
				*((int *) itr->data) = (int) -1;
			}

			break;

		default:
			ret = -EBADE;
		}

		if (ret) {
			if (unlikely(touch_debug_mask_ & DEBUG_CONFIG))
				TOUCH_INFO_MSG("Missing DT entry: %s, ret:%d\n", itr->name, ret);
		}
	}

	if (role->key_type == 0)
		role->key_type = TOUCH_HARD_KEY;

	if (caps->button_support) {
		prop = of_find_property(np, "button_name", &i);
		if (prop == NULL) {
			if (unlikely(touch_debug_mask_ & DEBUG_CONFIG))
				TOUCH_INFO_MSG("Missing DT entry: %s, ret:%d\n", itr->name, ret);
			return 0;
		}

		caps->number_of_button = (i >> 2);

		if (caps->number_of_button > MAX_BUTTON) {
			if (unlikely(touch_debug_mask_ & DEBUG_CONFIG))
				TOUCH_DEBUG_MSG("too many buttons : %d !!\n", caps->number_of_button);
			caps->number_of_button = MAX_BUTTON;
		}

		if (unlikely(touch_debug_mask_ & DEBUG_CONFIG))
			TOUCH_DEBUG_MSG("DT entry: number_of_button = %d\n", caps->number_of_button);

		value = prop->value;

		for (i = 0; i < caps->number_of_button; i++) {
			caps->button_name[i] = be32_to_cpup(value++);

			if (unlikely(touch_debug_mask_ & DEBUG_CONFIG))
				TOUCH_INFO_MSG("DT entry: button_name[%d] : %s:%d\n", i,
					get_touch_button_string((u16)caps->button_name[i]), caps->button_name[i]);
		}
	}

	ret = of_property_count_strings(np, "panel_id");
	if (ret) {
		for (i = 0; i < ret; i++) {
			of_property_read_string_index(np, "panel_id", i, &str_index);
			pdata->panel_type[i] = str_index[0];
		}
	}

	// In case of vdd type is GPIO, vdd_value should be reparsed to GPIO value
	for (i = 0; i < TOUCH_PWR_NUM; ++i) {
		if (pwr[i].type == 1) {
			memset(power_gpio, 0x0, sizeof(power_gpio));
			sprintf(power_gpio, "vdd_value%d", i);
			if ((ret = of_get_named_gpio(np, power_gpio, 0)) >= 0) {
				if (unlikely(touch_debug_mask_ & DEBUG_CONFIG))
					TOUCH_INFO_MSG("DT entry[Reparse]: %s = %d (%s)\n", power_gpio, ret, touch_print_dt_type(DT_GPIO));
				pwr[i].value = (int) ret;
			} else {
				pwr[i].value = (int) -1;
			}
		}
	}

	return 0;
}
#else
//IS_MTK
static int touch_parse_dt(struct device *dev, struct touch_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	struct property *prop;

	struct touch_device_caps *caps = pdata->caps;
	struct touch_operation_role *role = pdata->role;
	struct touch_power_info *pwr = pdata->pwr;
	struct touch_limit_value *limit = pdata->limit;

	int i = 0;
	int ret = 0;
	char *tmp = NULL;
	u32 val = 0;
	const __be32 *value = NULL;
	const char *str_index = NULL;
	char power_gpio[16] = {0,};
	struct touch_dt_map *itr = NULL;
	struct touch_dt_map map[] = {
		{ "lge,rst-gpio", &pdata->reset_pin, DT_GPIO, GPIO_CTP_RST_PIN },
		{ "lge,int-gpio", &pdata->int_pin, DT_GPIO, GPIO_CTP_EINT_PIN },
		{ "lge,sda-gpio", &pdata->sda_pin, DT_GPIO, GPIO_I2C0_SDA_PIN },
		{ "lge,scl-gpio", &pdata->scl_pin, DT_GPIO, GPIO_I2C0_SCA_PIN },
		{ "lge,id-gpio", &pdata->id_pin, DT_GPIO, 0 },
		{ "lge,id2-gpio", &pdata->id2_pin, DT_GPIO, 0 },

		{ "lge,ic_type", &pdata->ic_type, DT_U8, 2 },
		{ "lge,maker", &pdata->maker, DT_STRING, 0 },
		{ "lge,product", &pdata->fw_product, DT_STRING, 0 },
		{ "lge,fw_image", &pdata->fw_image, DT_STRING, 0 },
		{ "lge,auto_fw_update", &pdata->auto_fw_update, DT_U8, 1 },
		{ "active_area_gap", &pdata->active_area_gap, DT_U8, 24 },

		/* panel status check */
		{ "openshort_enable", &pdata->check_openshort, DT_U8, 1 },

		/*  limitation value */
		{ "raw_data_max", &limit->raw_data_max, DT_U32, 42125 },
		{ "raw_data_min", &limit->raw_data_min, DT_U32, 11394 },
		{ "raw_data_margin", &limit->raw_data_margin, DT_U32, 1000 },
		{ "raw_data_otp_min", &limit->raw_data_otp_min, DT_U32, 15000 },
		{ "raw_data_otp_max", &limit->raw_data_otp_max, DT_U32, 39000 },
		{ "open_short_min", &limit->open_short_min, DT_U32, 10 },
		{ "slope_max", &limit->slope_max, DT_U32, 110 },
		{ "slope_min", &limit->slope_min, DT_U32, 90 },

		/* caps */
		{ "button_support", &caps->button_support, DT_U8, 0 },
		{ "is_width_supported", &caps->is_width_supported, DT_U8, 1 },
		{ "is_pressure_supported", &caps->is_pressure_supported, DT_U8, 1 },
		{ "is_id_supported", &caps->is_id_supported, DT_U8, 1 },
		{ "max_width", &caps->max_width, DT_U32, 30 },
		{ "max_pressure", &caps->max_pressure, DT_U32, 0xff },
		{ "max_id", &caps->max_id, DT_U32, 10 },
		{ "x_max", &caps->x_max, DT_U32, 480 },
		{ "y_max", &caps->y_max, DT_U32, 854 },
		{ "lcd_x", &caps->lcd_x, DT_U32, 480 },
		{ "lcd_y", &caps->lcd_y, DT_U32, 854 },

		/* role */
		{ "key_type", &role->key_type, DT_U8, 0 },
		{ "booting_delay", &role->booting_delay, DT_U32, 10 },
		{ "reset_delay", &role->reset_delay, DT_U32, 5 },
		{ "suspend_pwr", &role->suspend_pwr, DT_U8, 0 },
		{ "resume_pwr", &role->resume_pwr, DT_U8, 1 },
		{ "ghost_detection_enable", &role->ghost_finger_solution_enable, DT_U32, 0 },
		{ "use_crack_mode", &role->use_crack_mode, DT_U32, 0 },

#ifdef IS_MTK
		/* power_info */
		{ "vdd_type0", &pwr[0].type, DT_U8, 0 },
		{ "vdd_name0", &pwr[0].name, DT_STRING, 0 },
		{ "vdd_value0", &pwr[0].value, DT_U32, 0xffffffff },
		{ "vdd_type1", &pwr[1].type, DT_U8, 0 },
		{ "vdd_name1", &pwr[1].name, DT_STRING, 0 },
		{ "vdd_value1", &pwr[1].value, DT_U32, 0xffffffff },
		{ "vdd_type2", &pwr[2].type, DT_U8, 0 },
		{ "vdd_name2", &pwr[2].name, DT_STRING, 0 },
		{ "vdd_value2", &pwr[2].value, DT_U32, 0xffffffff },
#endif //IS_MTK
		{ NULL, NULL, 0, 0},
	};

	TOUCH_TRACE_FUNC();
	
	/* reset, irq gpio info */
#ifndef IS_MTK
	if (np == NULL)
		return -ENODEV;
#endif

	for (itr = map; itr->name; ++itr) {
		val = 0;
		ret = 0;
		switch (itr->type) {
		case DT_U8:
#ifndef IS_MTK
			if ((ret = of_property_read_u32(np, itr->name, &val)) == 0) {
				*((u8 *) itr->data) = (u8) val;
				if (unlikely(touch_debug_mask_ & DEBUG_CONFIG))
					TOUCH_INFO_MSG("DT entry: %s = %d (%s)\n", itr->name, val, touch_print_dt_type(itr->type));
			} else {
				*((u8 *) itr->data) = (u8) itr->def_value;
			}
#else
				*((u8 *) itr->data) = (u8) itr->def_value;
				if (unlikely(touch_debug_mask_ & DEBUG_CONFIG))
					TOUCH_INFO_MSG("DT entry: %s = %d (%s)\n", itr->name, *((u8 *) itr->data), touch_print_dt_type(itr->type));
#endif
			break;

		case DT_U16:
#ifndef IS_MTK
			if ((ret = of_property_read_u32(np, itr->name, &val)) == 0) {
				*((u16 *) itr->data) = (u16) val;
				if (unlikely(touch_debug_mask_ & DEBUG_CONFIG))
					TOUCH_INFO_MSG("DT entry: %s = %d (%s)\n", itr->name, val, touch_print_dt_type(itr->type));
			} else {
				*((u16 *) itr->data) = (u16) itr->def_value;
			}
#else
				*((u16 *) itr->data) = (u16) itr->def_value;

				if (unlikely(touch_debug_mask_ & DEBUG_CONFIG))
					TOUCH_INFO_MSG("DT entry: %s = %d (%s)\n", itr->name,  (u16) itr->def_value , touch_print_dt_type(itr->type));
#endif
			break;

		case DT_U32:
#ifndef IS_MTK
			if ((ret = of_property_read_u32(np, itr->name, &val)) == 0) {
				*((u32 *) itr->data) = val;
				if (unlikely(touch_debug_mask_ & DEBUG_CONFIG))
					TOUCH_INFO_MSG("DT entry: %s = %d (%s)\n", itr->name, val, touch_print_dt_type(itr->type));
			} else {
				*((u32 *) itr->data) = (u32) itr->def_value;
			}
#else
			*((u32 *) itr->data) = (u32) itr->def_value;
			if (unlikely(touch_debug_mask_ & DEBUG_CONFIG))
					TOUCH_INFO_MSG("DT entry: %s = %d (%s)\n", itr->name, (u32) itr->def_value, touch_print_dt_type(itr->type));
#endif
			break;
#ifndef IS_MTK
		case DT_STRING:

			tmp = NULL;
			if ((ret = of_property_read_string(np, itr->name, (const char **) &tmp)) == 0) {
				strcpy(itr->data, tmp);
				if (unlikely(touch_debug_mask_ & DEBUG_CONFIG))
					TOUCH_INFO_MSG("DT entry: %s = %s (%s)\n", itr->name, (char *) itr->data, touch_print_dt_type(itr->type));
			} else {
				*((u32 *) itr->data) = 0;
			}
#endif
			break;

		case DT_GPIO:
#ifndef IS_MTK
			if ((ret = of_get_named_gpio(np, itr->name, 0)) >= 0) {
				if (unlikely(touch_debug_mask_ & DEBUG_CONFIG))
					TOUCH_INFO_MSG("DT entry: %s = %d (%s)\n", itr->name, ret, touch_print_dt_type(itr->type));
				*((int *) itr->data) = (int) ret;
				ret = 0;
			} else {
				*((int *) itr->data) = (int) -1;
			}
#else
			*((unsigned long *) itr->data) = (unsigned long)itr->def_value;
			if (unlikely(touch_debug_mask_ & DEBUG_CONFIG)) 	{
				TOUCH_INFO_MSG("DT entry: %s = %Xu (%s)\n", itr->name, itr->def_value, touch_print_dt_type(itr->type));
				TOUCH_INFO_MSG("DT entry: %s = %Xu (%s)\n", itr->name, (itr->def_value & 0x0fffffff), touch_print_dt_type(itr->type));
			}
#endif
			break;

		default:
			ret = -EBADE;
		}

		if (ret) {
			if (unlikely(touch_debug_mask_ & DEBUG_CONFIG))
				TOUCH_INFO_MSG("Missing DT entry: %s, ret:%d\n", itr->name, ret);
		}
	}
#ifdef IS_MTK
//case DT_STRING:
		strcpy(pdata->maker , "Melfas");
		strcpy(pdata->fw_product , "L0M45P1");
		strcpy(pdata->fw_image , "melfas/mit200/L0M45P1_00_02.fw");

		TOUCH_INFO_MSG("DT entry: pdata->maker = %s \n", pdata->maker);
		TOUCH_INFO_MSG("DT entry: pdata->fw_product = %s \n", pdata->fw_product);
		TOUCH_INFO_MSG("DT entry: pdata->fw_image = %s \n", pdata->fw_image);
#endif //IS_MTK
	if (role->key_type == 0)
		role->key_type = TOUCH_HARD_KEY;

	if (caps->button_support) {
		prop = of_find_property(np, "button_name", &i);
		if (prop == NULL) {
			if (unlikely(touch_debug_mask_ & DEBUG_CONFIG))
				TOUCH_INFO_MSG("Missing DT entry: %s, ret:%d\n", itr->name, ret);
			return 0;
		}
		caps->number_of_button = (i >> 2);

		if (caps->number_of_button > MAX_BUTTON) {
			if (unlikely(touch_debug_mask_ & DEBUG_CONFIG))
				TOUCH_DEBUG_MSG("too many buttons : %d !!\n", caps->number_of_button);
			caps->number_of_button = MAX_BUTTON;
		}

		if (unlikely(touch_debug_mask_ & DEBUG_CONFIG))
			TOUCH_DEBUG_MSG("DT entry: number_of_button = %d\n", caps->number_of_button);

		value = prop->value;

		for (i = 0; i < caps->number_of_button; i++) {
			caps->button_name[i] = be32_to_cpup(value++);

			if (unlikely(touch_debug_mask_ & DEBUG_CONFIG))
				TOUCH_INFO_MSG("DT entry: button_name[%d] : %s:%d\n", i,
					get_touch_button_string((u16)caps->button_name[i]), caps->button_name[i]);
		}
	}

	ret = of_property_count_strings(np, "panel_id");
	if (ret) {
		for (i = 0; i < ret; i++) {
			of_property_read_string_index(np, "panel_id", i, &str_index);
			pdata->panel_type[i] = str_index[0];
		}
	}
#if 0
	// In case of vdd type is GPIO, vdd_value should be reparsed to GPIO value
	for (i = 0; i < TOUCH_PWR_NUM; ++i) {
		if (pwr[i].type == 1) {
			memset(power_gpio, 0x0, sizeof(power_gpio));
			sprintf(power_gpio, "vdd_value%d", i);
			if ((ret = of_get_named_gpio(np, power_gpio, 0)) >= 0) {
				if (unlikely(touch_debug_mask_ & DEBUG_CONFIG))
					TOUCH_INFO_MSG("DT entry[Reparse]: %s = %d (%s)\n", power_gpio, ret, touch_print_dt_type(DT_GPIO));
				pwr[i].value = (int) ret;
			} else {
				pwr[i].value = (int) -1;
			}
		}
	}
#endif
	return 0;
}

#endif//0
static int touch_init_platform_data(struct i2c_client *client)
{
	struct lge_touch_data *ts = i2c_get_clientdata(client);
	struct section_info *sc = &ts->st_info;

	int one_sec = 0;
	int i;
	TOUCH_INFO_MSG("%s \n", __func__);
	
#ifndef IS_MTK
	if (client->dev.of_node)
#else
	if ( true )
#endif
	{
		ts_pdata = devm_kzalloc(&client->dev,
				sizeof(struct touch_platform_data), GFP_KERNEL);
		if (!ts_pdata) {
			dev_err(&client->dev, "Failed to allocate memory : pdata\n");
			return -ENOMEM;
		}

		ts_caps = devm_kzalloc(&client->dev,
				sizeof(struct touch_device_caps), GFP_KERNEL);
		if (!ts_caps) {
			dev_err(&client->dev, "Failed to allocate memory : caps\n");
			return -ENOMEM;
		}

		ts_role = devm_kzalloc(&client->dev,
				sizeof(struct touch_operation_role), GFP_KERNEL);
		if (!ts_role) {
			dev_err(&client->dev, "Failed to allocate memory : role\n");
			return -ENOMEM;
		}

		ts_limit = devm_kzalloc(&client->dev,
				sizeof(struct touch_limit_value), GFP_KERNEL);
		if (!ts_limit) {
			dev_err(&client->dev, "Failed to allocate memory : limit\n");
			return -ENOMEM;
		}

		ts_lpwg = devm_kzalloc(&client->dev,
			sizeof(struct touch_tci_info), GFP_KERNEL);
		if (!ts_lpwg) {
			dev_err(&client->dev, "Failed to allocate memory : lpwg\n");
			return -ENOMEM;
		}

		if (touch_parse_dt(&client->dev, ts_pdata) < 0)
			return -EINVAL;
	} else {
		ts_pdata = client->dev.platform_data;
		if (!ts_pdata) {
			TOUCH_ERR_MSG("touch_parse_dt failed\n");
			return -EINVAL;
		}
	}

	if (check_platform_data(client) < 0) {
		TOUCH_ERR_MSG("platform data check failed\n");
		return -EINVAL;
	}
	one_sec = 1000000 / (12500000/*ts_role->report_period*//1000);
	ts->ic_init_err_cnt = 0;
	ts->work_sync_err_cnt = 0;

	/* Ghost-Finger solution
	 * max_count : if melt-time > 7-sec, max_count should be changed to 'one_sec * 3'
	 */
	ts->gf_ctrl.min_count = one_sec / 12;
	ts->gf_ctrl.max_count = one_sec * 3;
	ts->gf_ctrl.saved_x = -1;
	ts->gf_ctrl.saved_y = -1;
	ts->gf_ctrl.saved_last_x = 0;
	ts->gf_ctrl.saved_last_y = 0;
	ts->gf_ctrl.max_moved = ts_caps->x_max / 4;
	ts->gf_ctrl.max_pressure = 255;

	sc->panel.left = 0;
	sc->panel.right = ts_caps->x_max;
	sc->panel.top = 0;
	sc->panel.bottom = ts_caps->y_button_boundary;

	if (ts_caps->number_of_button)
		sc->b_width  = ts_caps->x_max / ts_caps->number_of_button;
	else
		sc->b_width  = ts_caps->x_max;

	sc->b_margin = sc->b_width * ts_caps->button_margin / 100;
	sc->b_inner_width = sc->b_width - (2*sc->b_margin);
	sc->b_height = ts_caps->y_max - ts_caps->y_button_boundary;
	sc->b_num = ts_caps->number_of_button;

	for (i = 0; i < sc->b_num; i++) {
		if (ts_caps->number_of_button)
			sc->button[i].left   = i * (ts_caps->x_max / ts_caps->number_of_button) + sc->b_margin;
		else
			sc->button[i].left   = i * ts_caps->x_max + sc->b_margin;

		sc->button[i].right  = sc->button[i].left + sc->b_inner_width;
		sc->button[i].top    = ts_caps->y_button_boundary + 1;
		sc->button[i].bottom = ts_caps->y_max;

		sc->button_cancel[i].left = sc->button[i].left - (2*sc->b_margin) >= 0 ?
			sc->button[i].left - (2*sc->b_margin) : 0;
		sc->button_cancel[i].right = sc->button[i].right + (2*sc->b_margin) <= ts_caps->x_max ?
			sc->button[i].right + (2*sc->b_margin) : ts_caps->x_max;
		sc->button_cancel[i].top = sc->button[i].top;
		sc->button_cancel[i].bottom = sc->button[i].bottom;

		sc->b_name[i] = ts_caps->button_name[i];
	}
	return 0;
}


static int touch_input_init(struct lge_touch_data *ts)
{
	struct input_dev *dev;
	int i;
	int max;

	TOUCH_TRACE_FUNC();

//#ifndef IS_MTK	
	dev = input_allocate_device();


	if (dev == NULL) {
		TOUCH_ERR_MSG("Failed to allocate input device\n");
		return -ENOMEM;
	}
//#endif
	dev->name = "touch_dev";

	set_bit(EV_SYN, dev->evbit);
	set_bit(EV_ABS, dev->evbit);
	set_bit(INPUT_PROP_DIRECT, dev->propbit);

	if (ts_caps->button_support && ts_role->key_type != VIRTUAL_KEY) {
		set_bit(EV_KEY, dev->evbit);
		for(i = 0; i < ts_caps->number_of_button; i++) {
			set_bit(ts_caps->button_name[i], ts->input_dev->keybit);
		}
	}

	input_set_abs_params(dev, ABS_MT_POSITION_X, 0, ts_caps->x_max, 0, 0);
	max = (ts_caps->y_button_boundary ? ts_caps->y_button_boundary : ts_caps->y_max);
	input_set_abs_params(dev, ABS_MT_POSITION_Y, 0, max, 0, 0);

	if (ts_caps->is_pressure_supported)
		input_set_abs_params(dev, ABS_MT_PRESSURE, 0, ts_caps->max_pressure, 0, 0);

	if (ts_caps->is_width_supported) {
		input_set_abs_params(dev, ABS_MT_WIDTH_MAJOR, 0, ts_caps->max_width, 0, 0);
		input_set_abs_params(dev, ABS_MT_WIDTH_MINOR, 0, ts_caps->max_width, 0, 0);
		input_set_abs_params(dev, ABS_MT_ORIENTATION, 0, 1, 0, 0);
	}

	input_mt_init_slots(dev, ts_caps->max_id, 0);

	if (input_register_device(dev) < 0) {
		TOUCH_ERR_MSG("Unable to register %s input device\n",
				dev->name);
		input_free_device(dev);
		return -EINVAL;
	}

	input_set_drvdata(dev, ts);
	ts->input_dev = dev;

	return 0;
}

#if defined(TOUCH_USE_DSV)
struct lge_touch_data *g_ts = NULL;
static void dsv_ctrl(int onoff)
{
	TOUCH_INFO_MSG("apds interrupt onoff: %d\n", onoff);
	g_ts->pdata->sensor_value = onoff;

	if(g_ts->pdata->enable_sensor_interlock) {
		if(onoff) {
			if (atomic_read(&dev_state) == DEV_RESUME_ENABLE) {
				g_ts->pdata->use_dsv = 1;
				touch_drv->dsv_control(g_ts->client);
			} else {
				g_ts->pdata->use_dsv = 0;
				TOUCH_INFO_MSG("%s Don't turn on DSV, LPWG not ready\n", __func__);
			}
		} else {
			g_ts->pdata->use_dsv = 0;
			touch_drv->dsv_control(g_ts->client);
		}
	}
}
#endif
static int touch_fb_suspend(struct device *device)
{
	struct lge_touch_data *ts =  dev_get_drvdata(device);
	long ret = 0;

	TOUCH_INFO_MSG("%s \n", __func__);

	ts->pdata->panel_on = 0;
	if (ts->pdata->curr_pwr_state == POWER_OFF)
		return 0;

	if(ts->pdata->role->use_crack_mode) {
		if (atomic_read(&ts->crack_test_state) != CRACK_TEST_FINISH) {
			TOUCH_INFO_MSG("touch_fb_suspend is not executed - crack check not finished\n");
			return 0;
		}
	}

	if(power_block){
		TOUCH_INFO_MSG("touch_suspend is not executed\n");
		return 0;
	}

	touch_disable(ts->client->irq);

	if (!ts->pdata->lpwg_mode)
		cancel_delayed_work_sync(&ts->work_init);

	if (ts_role->key_type == TOUCH_HARD_KEY)
		cancel_delayed_work_sync(&ts->work_touch_lock);

	ret = wait_for_completion_interruptible_timeout(&ts->irq_completion, msecs_to_jiffies(1000));
	if (ret == 0) {
		TOUCH_INFO_MSG("Completion timeout \n");
	} else if (ret < 0) {
		TOUCH_INFO_MSG("Completion ret = %ld \n", ret);
	}

	release_all_ts_event(ts);

	if (ts->ts_data.palm) {
		TOUCH_INFO_MSG("Palm released \n");
	}

	memset(&ts->ts_data, 0x0, sizeof(ts->ts_data));

	if (ts->pdata->lpwg_mode) {
		TOUCH_INFO_MSG("ts->pdata->lpwg_mode : %d, \n", (u32)ts->pdata->lpwg_mode);
		touch_drv->ic_ctrl(ts->client, IC_CTRL_LPWG, (u32)&(ts->pdata->lpwg_mode));
		atomic_set(&dev_state,DEV_RESUME_ENABLE);
		touch_enable(ts->client->irq);
		touch_enable_wake(ts->client->irq);
	} else {
		touch_power_cntl(ts, ts_role->suspend_pwr);
		atomic_set(&dev_state,DEV_SUSPEND);
		TOUCH_INFO_MSG("SUSPEND AND SET power off\n");

#if defined(TOUCH_USE_DSV)
		if(ts_pdata->enable_sensor_interlock) {
			ts->pdata->use_dsv = 0;
			touch_drv->dsv_control(g_ts->client);
		}
#endif
	}

	return 0;
}

static int touch_fb_resume(struct device *device)
{
	struct lge_touch_data *ts =  dev_get_drvdata(device);

	mutex_lock(&ts->thread_lock);

	TOUCH_INFO_MSG("%s \n", __func__);

	ts->pdata->panel_on = 1;

#if defined(TOUCH_USE_DSV)
	ts->pdata->sensor_value = 0;
	if(ts->pdata->enable_sensor_interlock) {
		ts->pdata->use_dsv = 0;
	}
#endif

	if(ts->pdata->role->use_crack_mode) {
		if (atomic_read(&ts->crack_test_state) == CRACK_TEST_FINISH) {
			if (window_crack_check == CRACK) {
				TOUCH_INFO_MSG("%s : window cracked - send uevent\n", __func__);
				send_uevent(window_crack);
			}
		} else {
			TOUCH_INFO_MSG("touch_resume is not executed - crack check not finished\n");
			goto exit;
		}
	}

	if (power_block) {
		TOUCH_INFO_MSG("touch_resume is not executed\n");
		goto exit;
	}

	if (ts->pdata->lpwg_mode) {
		touch_disable_wake(ts->client->irq);
		safety_reset(ts);
	} else {
		if (ts->pdata->curr_pwr_state == POWER_ON) {
			touch_disable_wake(ts->client->irq);
			safety_reset(ts);
		} else {
			touch_power_cntl(ts, ts_role->resume_pwr);
			touch_enable(ts->client->irq);
		}
	}

	ts->ic_error_cnt =0;

	if (ts_pdata->role->resume_pwr == POWER_ON)
		queue_delayed_work(touch_wq, &ts->work_init,
				msecs_to_jiffies(ts_role->booting_delay));
	else
		queue_delayed_work(touch_wq, &ts->work_init, 0);

exit:
	mutex_unlock(&ts->thread_lock);

	return 0;
}

static int touch_suspend(struct device *device)
{
	TOUCH_INFO_MSG("%s \n", __func__);



	return 0;
}

static int touch_resume(struct device *device)
{
	TOUCH_INFO_MSG("%s \n", __func__);


	return 0;
}

#if defined (CONFIG_HAS_EARLYSUSPEND)
static void touch_early_suspend(struct early_suspend *h)
{
	struct lge_touch_data *ts =
		container_of(h, struct lge_touch_data, early_suspend);

	TOUCH_TRACE_FUNC();

	touch_fb_suspend(&ts->client->dev);
}

static void touch_late_resume(struct early_suspend *h)
{
	struct lge_touch_data *ts =
		container_of(h, struct lge_touch_data, early_suspend);

	TOUCH_TRACE_FUNC();

	touch_fb_resume(&ts->client->dev);
}
#endif


#if defined(CONFIG_FB)
static int touch_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct lge_touch_data *ts = container_of(self, struct lge_touch_data, fb_notifier_block);

	if (evdata && evdata->data && event == FB_EVENT_BLANK && ts && ts->client) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK) {
			touch_fb_resume(&ts->input_dev->dev);
		} else if (*blank == FB_BLANK_POWERDOWN) {
			touch_fb_suspend(&ts->input_dev->dev);
		}
	}

	return 0;
}
#endif

/* sysfs */
static ssize_t fw_upgrade_show(struct lge_touch_data *ts, char *buf, bool forceupgrade)
{
	int ret = 0;

	ret += sprintf(buf + ret, "\n");
	if (forceupgrade) {
		ret += sprintf(buf + ret, "help for fw_force_upgrade\n");
		ret += sprintf(buf + ret, "   echo [option] > fw_force_upgrade\n");
	} else {
		ret += sprintf(buf + ret, "help for fw_upgrade\n");
		ret += sprintf(buf + ret, "   echo [option] > fw_upgrade\n");
	}
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "option:\n");

	ret += sprintf(buf + ret, "  erase : Erase the firmware at the Touch IC.\n");
	ret += sprintf(buf + ret, "  [F/W name].fw : Upgrade with the written name.\n");
	ret += sprintf(buf + ret, "  1 : Upgrade with the given name in Device Tree [%s]\n", ts_pdata->fw_image);
	ret += sprintf(buf + ret, "  9 : Upgrade with %s%s\n", EXTERNAL_FW_PATH, EXTERNAL_FW_NAME);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "cat fw_upgrade\n");
	ret += sprintf(buf + ret, "    : show this message\n");

	return ret;
}

static ssize_t fw_upgrade_store(struct lge_touch_data *ts, const char *buf, size_t count, bool forceupgrade)
{
	char cmd[NAME_BUFFER_SIZE] ={0};

	ts->fw_info.eraseonly = 0;

	if (sscanf(buf, "%s", cmd) != 1)
		return -EINVAL;

	if (!strncmp(cmd, "erase", 5)) {
		TOUCH_INFO_MSG("Touch Firmware erase start...\n");
		ts->fw_info.eraseonly = 1;
	} else if (strncmp(&cmd[strlen(cmd) - 3], ".fw", 3) == 0 || strncmp(&cmd[strlen(cmd) - 3], ".FW", 3) == 0) {
		TOUCH_INFO_MSG("F/W File : %s\n",cmd);
		snprintf(ts->fw_info.path, NAME_BUFFER_SIZE, "%s", cmd);
	} else if(!strncmp(cmd,"1",1)){
		strncpy(ts->fw_info.path, ts_pdata->fw_image, NAME_BUFFER_SIZE);
	} else if(!strncmp(cmd,"9",1)){
		snprintf(ts->fw_info.path, NAME_BUFFER_SIZE, "%s", EXTERNAL_FW_NAME);
	} else {
		ts->fw_info.path[0] = '\0';
		return count;
	}

	while (ts->fw_info.is_downloading);

	ts->fw_info.is_downloading = 1;

	if (forceupgrade)
		ts->fw_info.force_upgrade = 1;
	else
		ts->fw_info.force_upgrade = 0;

	queue_work(touch_wq, &ts->work_fw_upgrade);

	while (ts->fw_info.is_downloading);

	return count;
}

static ssize_t fw_force_upgrade_show(struct lge_touch_data *ts, char *buf)
{
	return  fw_upgrade_show(ts, buf, true);
}

static ssize_t fw_force_upgrade_store(struct lge_touch_data *ts, const char *buf, size_t count)
{
	return fw_upgrade_store(ts, buf, count, true);
}

static ssize_t fw_normal_upgrade_show(struct lge_touch_data *ts, char *buf)
{
	return  fw_upgrade_show(ts, buf, false);
}

static ssize_t fw_normal_upgrade_store(struct lge_touch_data *ts, const char *buf, size_t count)
{
	return fw_upgrade_store(ts, buf, count, false);
}

static ssize_t fw_dump_show(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;

	ret = touch_drv->sysfs(ts->client, buf, 0, SYSFS_FW_DUMP);

	return ret;
}

static ssize_t version_show(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;

	ret = touch_drv->sysfs(ts->client, buf, 0, SYSFS_VERSION_SHOW);
	return ret;
}

static ssize_t testmode_version_show(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;

	ret = touch_drv->sysfs(ts->client, buf, 0, SYSFS_TESTMODE_VERSION_SHOW);
	return ret;
}

static ssize_t power_control_show(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;

	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "help for power_control\n");
	ret += sprintf(buf + ret, "   echo [option] > power_control\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "option:\n");
	ret += sprintf(buf + ret, "  0 : power off\n");
	ret += sprintf(buf + ret, "  1 : power on\n");
	ret += sprintf(buf + ret, "  2 : power reset\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "cat power_control\n");
	ret += sprintf(buf + ret, "    : show this message\n");

	return ret;
}

static ssize_t power_control_store(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int cmd = 0;

	if (sscanf(buf, "%d", &cmd) != 1)
		return -EINVAL;
	switch (cmd){
		case 0:
			if (ts->pdata->curr_pwr_state == POWER_ON){
				touch_disable(ts->client->irq);
				release_all_ts_event(ts);
				touch_power_cntl(ts, POWER_OFF);
			}
			break;
		case 1:
			if (ts->pdata->curr_pwr_state == POWER_OFF){
				touch_power_cntl(ts, POWER_ON);
				touch_enable(ts->client->irq);
				touch_ic_init(ts);
			}
			break;
		case 2:
			if (ts->pdata->curr_pwr_state == POWER_ON){
				safety_reset(ts);
				touch_ic_init(ts);
			}
			break;
		default:
			TOUCH_INFO_MSG("usage: echo [0|1|2] > control\n");
			TOUCH_INFO_MSG("  - 0: power off\n");
			TOUCH_INFO_MSG("  - 1: power on\n");
			TOUCH_INFO_MSG("  - 2: power reset\n");
			break;
	}
	return count;
}

static ssize_t chstatus_show(struct lge_touch_data *ts, char *buf)
{
	ssize_t ret;
	if(ts->pdata->panel_on == POWER_ON || ts->pdata->panel_on == POWER_WAKE) {
		ret = touch_drv->sysfs(ts->client, buf, 0, SYSFS_CHSTATUS_SHOW);
	} else {
		ret = snprintf(buf,PAGE_SIZE,"If you want to check open short, please turn on the LCD.\n");
		TOUCH_INFO_MSG("If you want to check open short, please turn on the LCD.\n");
	}
	return ret;
}

static ssize_t openshort_show(struct lge_touch_data *ts, char *buf)
{
	ssize_t ret;
	int value = 0;
	if(ts->pdata->panel_on == POWER_ON || ts->pdata->panel_on == POWER_WAKE) {

		value = ts->pdata->check_openshort;
		ts->pdata->check_openshort = 1;

		ret = touch_drv->sysfs(ts->client, buf, 0, SYSFS_CHSTATUS_SHOW);

		ts->pdata->check_openshort = value;
	} else {
		ret = snprintf(buf,PAGE_SIZE,"If you want to check open short, please turn on the LCD.\n");
		TOUCH_INFO_MSG("If you want to check open short, please turn on the LCD.\n");
	}
	return ret;
}

static ssize_t openshort_store(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int ret = 0;
	int value = 0;
	char cmd[NAME_BUFFER_SIZE] = {0};
	if (ts->pdata->panel_on == POWER_ON || ts->pdata->panel_on == POWER_WAKE) {
		if (sscanf(buf, "%s", cmd) != 1)
			return -EINVAL;

		value = ts->pdata->check_openshort;
		ts->pdata->check_openshort = 1;

		ret = touch_drv->sysfs(ts->client, 0, buf, SYSFS_CHSTATUS_STORE);

		ts->pdata->check_openshort = value;
	} else {
		TOUCH_INFO_MSG("If you want to check open short, please turn on the LCD.\n");
	}

	return count;
}


static ssize_t rawdata_show(struct lge_touch_data *ts, char *buf)
{
	ssize_t ret;
	if(ts->pdata->panel_on == POWER_ON || ts->pdata->panel_on == POWER_WAKE || ts->pdata->lpwg_debug_enable != 0) {
	ret = touch_drv->sysfs(ts->client, buf, 0, SYSFS_RAWDATA_SHOW);
	} else {
		ret = snprintf(buf,PAGE_SIZE,"If you want to check raw data, please turn on the LCD.\n");
		TOUCH_INFO_MSG("If you want to check raw data, please turn on the LCD.\n");
	}
	return ret;

}

static ssize_t rawdata_store(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int ret = 0;
	char cmd[NAME_BUFFER_SIZE] = {0};
	if (ts->pdata->panel_on == POWER_ON || ts->pdata->panel_on == POWER_WAKE || ts->pdata->lpwg_debug_enable != 0) {
		if (sscanf(buf, "%s", cmd) != 1)
			return -EINVAL;

		ret = touch_drv->sysfs(ts->client, 0, buf, SYSFS_RAWDATA_STORE);
	} else {
		TOUCH_INFO_MSG("If you want to check raw data, please turn on the LCD.\n");
	}

	return count;

}

static ssize_t delta_show(struct lge_touch_data *ts, char *buf)
{
	ssize_t ret;
	if (ts->pdata->panel_on == POWER_ON || ts->pdata->panel_on == POWER_WAKE || ts->pdata->lpwg_debug_enable != 0) {
		ret = touch_drv->sysfs(ts->client, buf, 0, SYSFS_DELTA_SHOW);
	} else {
		ret = snprintf(buf,PAGE_SIZE,"If you want to check delta, please turn on the LCD.\n");
		TOUCH_INFO_MSG("If you want to check delta, please turn on the LCD.\n");
	}
	return ret;
}

static ssize_t self_diagnostic_show(struct lge_touch_data *ts, char *buf)
{
	ssize_t ret;
	if(ts->pdata->panel_on == POWER_ON || ts->pdata->panel_on == POWER_WAKE || ts->pdata->lpwg_debug_enable != 0) {
		ts->sd_status = 1;
		ret = touch_drv->sysfs(ts->client, buf, 0, SYSFS_SELF_DIAGNOSTIC_SHOW);
		ts->sd_status = 0;
	 } else {
		ret = snprintf(buf,PAGE_SIZE,"If you want to check self diagnostic, please turn on the LCD.\n");
		TOUCH_INFO_MSG("If you want to check self diagnostic, please turn on the LCD.\n");
	}
	return ret;
}

static ssize_t sensing_on_show(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;
	char onoff = '1';
	ret = touch_drv->sysfs(ts->client, &onoff, buf, SYSFS_SENSING_ALL_BLOCK_CONTROL);

	return ret;
}

static ssize_t sensing_off_show(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;
	char onoff = '0';

	ret = touch_drv->sysfs(ts->client, &onoff, buf, SYSFS_SENSING_ALL_BLOCK_CONTROL);

	return ret;
}

static ssize_t sensing_on_store(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int ret = 0;
	char onoff = '1';
	int cmd = 0;
	if (sscanf(buf, "%d", &cmd) != 1)
		return -EINVAL;

	ret = touch_drv->sysfs(ts->client, &onoff, buf, SYSFS_SENSING_BLOCK_CONTROL);

	return count;
}

static ssize_t sensing_off_store(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int ret = 0;
	char onoff = '0';
	int cmd = 0;
	if (sscanf(buf, "%d", &cmd) != 1)
		return -EINVAL;

	ret = touch_drv->sysfs(ts->client, &onoff, buf, SYSFS_SENSING_BLOCK_CONTROL);

	return count;
}

static ssize_t lpwg_show(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;

	ret = touch_drv->sysfs(ts->client, buf, 0, SYSFS_LPWG_SHOW);

	return ret;
}

static ssize_t lpwg_store(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int ret = 0;
	char onoff = '1';
	int cmd = 0;
	if (sscanf(buf, "%d", &cmd) != 1)
		return -EINVAL;

	ret = touch_drv->sysfs(ts->client, &onoff, buf, SYSFS_LPWG_STORE);

	return count;
}

static ssize_t window_crack_status_show(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;
	ret = sprintf(buf, "window crack status [%d]\n", window_crack_check);
	return ret;
}

static ssize_t window_crack_status_store(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int cmd = 0;
	sscanf(buf, "%d", &cmd);
	if(cmd == 1)
		window_crack_check = CRACK;
	else
		window_crack_check = NO_CRACK;
	return count;
}

static ssize_t lpwg_notify_store(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int type = 0;
	int cmd[4] = {0,};
	int ret = 0;

	TOUCH_TRACE_FUNC();
	
	sscanf(buf, "%d %d %d %d %d", &type, &cmd[0], &cmd[1], &cmd[2], &cmd[3]);
	TOUCH_INFO_MSG(" type = %d value = %d %d %d %d\n", type, cmd[0], cmd[1], cmd[2], cmd[3]);

//for a while bring-up
	mutex_lock(&ts->thread_lock);
	switch(type) {
		case CMD_LPWG_ENABLE:
			break;
		case CMD_LPWG_LCD:
			break;
		case CMD_LPWG_ACTIVE_AREA:
			ret = touch_drv->sysfs(ts->client, "area", buf + 1, SYSFS_LPWG_STORE);
			break;
		case CMD_LPWG_TAP_COUNT:
			ret = touch_drv->sysfs(ts->client, "tap_count", buf + 1, SYSFS_LPWG_STORE);
			break;
		case CMD_LPWG_LCD_RESUME_SUSPEND:
			if (cmd[0] == 0)
				ts->pdata->lpwg_panel_on = LCD_OFF;
			else
				ts->pdata->lpwg_panel_on = LCD_ON;
			break;
		case CMD_LPWG_PROX:
			break;
		case CMD_LPWG_DOUBLE_TAP_CHECK:
			ret = touch_drv->sysfs(ts->client, "tap_check", buf + 1, SYSFS_LPWG_STORE);
			break;
		case CMD_LPWG_TOTAL_STATUS:
			ts->pdata->lpwg_panel_on = cmd[1];
			ts->pdata->lpwg_prox = cmd[2];
			ret = touch_drv->sysfs(ts->client, "update_all", buf + 1, SYSFS_LPWG_STORE);
			break;
	}
	mutex_unlock(&ts->thread_lock);
	return count;
}

static ssize_t lpwg_data_show(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;
	int i = 0;

	TOUCH_INFO_MSG("Called lpwg_data_show  size = %d\n", ts->pdata->lpwg_size);

	for(i = 0; i < ts->pdata->lpwg_size; i++) {
		ret += sprintf(buf+ret, "%d %d\n", ts->pdata->lpwg_x[i], ts->pdata->lpwg_y[i]);
	}

	memset(ts->pdata->lpwg_x, 0, sizeof(int)*10);
	memset(ts->pdata->lpwg_y, 0, sizeof(int)*10);
	memset(&ts->pdata->lpwg_size, 0, sizeof(int));
	return ret;
}

static ssize_t lpwg_data_store(struct lge_touch_data *ts, const char *buf, size_t count)
{
	uint8_t reply = 0;

	sscanf(buf, "%c", &reply);
	TOUCH_INFO_MSG("Called lpwg_data_store reply = %c\n", reply);

	return count;
}

static ssize_t keyguard_info_store(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int ret = 0;

	ret = touch_drv->sysfs(ts->client, 0, buf, SYSFS_KEYGUARD_STORE);

	return count;
}

static ssize_t reg_control_store(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int ret = 0;

	TOUCH_INFO_MSG("reg_control_store\n");
	ret = touch_drv->sysfs(ts->client, 0, buf, SYSFS_REG_CONTROL_STORE);

	return count;
}

static ssize_t tci_show(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;

	ret += sprintf(buf+ret, "\n<Select Type>\n\n1.IDLE_REPORTRATE_CTRL\n2.ACTIVE_REPORTRATE_CTRL\n3.SENSITIVITY_CTRL\n11.TCI_ENABLE_CTRL\n12.TOUCH_SLOP_CTRL\n13.TAP_MIN_DISTANCE_CTRL\n"
							"14.TAP_MAX_DISTANCE_CTRL\n15.MIN_INTERTAP_CTRL\n16.MAX_INTERTAP_CTRL\n"
							"17.TAP_COUNT_CTRL\n18.INTERRUPT_DELAY_CTRL\n21.TCI_ENABLE_CTRL2\n");
	ret += sprintf(buf+ret, "22.TOUCH_SLOP_CTRL2\n23.TAP_MIN_DISTANCE_CTRL2\n24.TAP_MAX_DISTANCE_CTRL2\n"
							"25.MIN_INTERTAP_CTRL2\n26.MAX_INTERTAP_CTRL2\n27.TAP_COUNT_CTRL2\n28.INTERRUPT_DELAY_CTRL2\n"
							"31.LPWG_STORE_INFO_CTRL\n32.LPWG_START_CTRL\n\n");
	return ret;
}

static ssize_t tci_store(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int ret = 0;

	TOUCH_INFO_MSG("tci_store!\n");
	ret = touch_drv->sysfs(ts->client, 0, buf, SYSFS_LPWG_TCI_STORE);

	return count;
}
#if defined(TOUCH_USE_DSV)
static ssize_t touch_dsv_show(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;
	char tmp[128] = {0};

	sprintf(tmp, "use_dsv : %d \n", ts_pdata->use_dsv);

	TOUCH_INFO_MSG("%s", tmp);

	ret += sprintf(buf+ret, "%s", tmp);

	return ret;
}

static ssize_t touch_dsv_store(struct lge_touch_data *ts, const char *buf, size_t count)
{
	if (!ts_pdata->panel_on) {
		TOUCH_INFO_MSG("LCD is off. Try after LCD On \n");
		return count;
	}
	sscanf(buf, "%d", &ts_pdata->use_dsv);
	TOUCH_INFO_MSG("use_dsv : %d \n", ts_pdata->use_dsv);

	return count;
}

static ssize_t sensor_value_show(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;
	char tmp[128] = {0};

	sprintf(tmp, "sensor_value : %d \n", ts_pdata->sensor_value);

	TOUCH_INFO_MSG("%s", tmp);

	ret += sprintf(buf+ret, "%s", tmp);

	return ret;
}

static ssize_t sensor_value_store(struct lge_touch_data *ts, const char *buf, size_t count)
{
	if (!ts_pdata->panel_on) {
		TOUCH_INFO_MSG("LCD is off. Try after LCD On \n");
		return count;
	}
	sscanf(buf, "%d", &ts_pdata->sensor_value);
	TOUCH_INFO_MSG("sensor_value store: %d \n", ts_pdata->sensor_value);

	return count;
}

static ssize_t enable_sensor_interlock_show(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;
	char tmp[128] = {0};

	sprintf(tmp, "enable_sensor_interlock : %d \n", ts_pdata->enable_sensor_interlock);

	TOUCH_INFO_MSG("%s", tmp);

	ret += sprintf(buf+ret, "%s", tmp);

	return ret;
}

static ssize_t enable_sensor_interlock_store(struct lge_touch_data *ts, const char *buf, size_t count)
{
	if (!ts_pdata->panel_on) {
		TOUCH_INFO_MSG("LCD is off. Try after LCD On \n");
		return count;
	}
	sscanf(buf, "%d", &ts_pdata->enable_sensor_interlock);
	TOUCH_INFO_MSG("enable_sensor_interlock store: %d \n", ts_pdata->enable_sensor_interlock);

	return count;
}
#endif

static ssize_t lpwg_debug_show(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;
	ret = sprintf(buf, "ts->pdata->lpwg_debug_enable : [%d]\n", ts->pdata->lpwg_debug_enable);
	return ret;
}

static ssize_t lpwg_debug_store(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int cmd = 0;
	int ret = 0;

	sscanf(buf, "%d", &cmd);

	ts->pdata->lpwg_debug_enable = cmd;
	ret = touch_drv->sysfs(ts->client, 0, buf, SYSFS_LPWG_DEBUG_STORE);

	return count;
}

static ssize_t lpwg_reason_show(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;
	ret = sprintf(buf, "ts->pdata->lpwg_fail_reason : [%d]\n", ts->pdata->lpwg_fail_reason);
	return ret;
}

static ssize_t lpwg_reason_store(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int cmd = 0;
	int ret = 0;

	sscanf(buf, "%d", &cmd);

	ts->pdata->lpwg_fail_reason = cmd;
	ret = touch_drv->sysfs(ts->client, 0, buf, SYSFS_LPWG_REASON_STORE);

	return count;
}

static LGE_TOUCH_ATTR(fw_force_upgrade, S_IRUGO | S_IWUSR, fw_force_upgrade_show, fw_force_upgrade_store);
static LGE_TOUCH_ATTR(fw_upgrade, S_IRUGO | S_IWUSR, fw_normal_upgrade_show, fw_normal_upgrade_store);
static LGE_TOUCH_ATTR(fw_dump, S_IRUGO | S_IWUSR, fw_dump_show, NULL);
static LGE_TOUCH_ATTR(version, S_IRUGO | S_IWUSR, version_show, NULL);
static LGE_TOUCH_ATTR(testmode_ver, S_IRUGO | S_IWUSR, testmode_version_show, NULL);
static LGE_TOUCH_ATTR(power_control, S_IRUGO | S_IWUSR, power_control_show, power_control_store);
static LGE_TOUCH_ATTR(chstatus, S_IRUGO | S_IWUSR, chstatus_show, NULL);
static LGE_TOUCH_ATTR(openshort, S_IRUGO | S_IWUSR, openshort_show, openshort_store);
static LGE_TOUCH_ATTR(rawdata, S_IRUGO | S_IWUSR, rawdata_show, rawdata_store);
static LGE_TOUCH_ATTR(delta, S_IRUGO | S_IWUSR, delta_show, NULL);
static LGE_TOUCH_ATTR(sd, S_IRUGO | S_IWUSR, self_diagnostic_show, NULL);
static LGE_TOUCH_ATTR(sensing_block_on, S_IRUGO | S_IWUSR, sensing_on_show, sensing_on_store);
static LGE_TOUCH_ATTR(sensing_block_off, S_IRUGO | S_IWUSR, sensing_off_show, sensing_off_store);
static LGE_TOUCH_ATTR(lpwg, S_IRUGO | S_IWUSR, lpwg_show, lpwg_store);
static LGE_TOUCH_ATTR(crack_status, S_IRUGO | S_IWUSR, window_crack_status_show, window_crack_status_store);
static LGE_TOUCH_ATTR(lpwg_notify, S_IRUGO | S_IWUSR, NULL, lpwg_notify_store);
static LGE_TOUCH_ATTR(lpwg_data, S_IRUGO | S_IWUSR, lpwg_data_show, lpwg_data_store);
static LGE_TOUCH_ATTR(keyguard, S_IRUGO | S_IWUSR, NULL, keyguard_info_store);
static LGE_TOUCH_ATTR(reg_control, S_IRUGO | S_IWUSR, NULL, reg_control_store);
static LGE_TOUCH_ATTR(tci, S_IRUGO | S_IWUSR, tci_show, tci_store);
static LGE_TOUCH_ATTR(lpwg_debug, S_IRUGO | S_IWUSR, lpwg_debug_show, lpwg_debug_store);
static LGE_TOUCH_ATTR(lpwg_fail_reason, S_IRUGO | S_IWUSR, lpwg_reason_show, lpwg_reason_store);
#if defined(TOUCH_USE_DSV)
static LGE_TOUCH_ATTR(dsv, S_IRUGO | S_IWUSR, touch_dsv_show, touch_dsv_store);
static LGE_TOUCH_ATTR(sensor_value, S_IRUGO | S_IWUSR, sensor_value_show, sensor_value_store);
static LGE_TOUCH_ATTR(enable_sensor_interlock, S_IRUGO | S_IWUSR, enable_sensor_interlock_show, enable_sensor_interlock_store);
#endif


static struct attribute *lge_touch_attribute_list[] = {
	&lge_touch_attr_fw_force_upgrade.attr,
	&lge_touch_attr_fw_upgrade.attr,
	&lge_touch_attr_fw_dump.attr,
	&lge_touch_attr_version.attr,
	&lge_touch_attr_testmode_ver.attr,
	&lge_touch_attr_power_control.attr,
	&lge_touch_attr_chstatus.attr,
	&lge_touch_attr_openshort.attr,
	&lge_touch_attr_rawdata.attr,
	&lge_touch_attr_delta.attr,
	&lge_touch_attr_sd.attr,
	&lge_touch_attr_sensing_block_on.attr,
	&lge_touch_attr_sensing_block_off.attr,
	&lge_touch_attr_lpwg.attr,
	&lge_touch_attr_lpwg_data.attr,
	&lge_touch_attr_lpwg_notify.attr,
	&lge_touch_attr_crack_status.attr,
	&lge_touch_attr_keyguard.attr,
	&lge_touch_attr_reg_control.attr,
	&lge_touch_attr_tci.attr,
	&lge_touch_attr_lpwg_debug.attr,
	&lge_touch_attr_lpwg_fail_reason.attr,
#if defined(TOUCH_USE_DSV)
	&lge_touch_attr_dsv.attr,
	&lge_touch_attr_sensor_value.attr,
	&lge_touch_attr_enable_sensor_interlock.attr,
#endif

	NULL,
};

static ssize_t lge_touch_attr_show(struct kobject *lge_touch_kobj, struct attribute *attr,
                             char *buf)
{
        struct lge_touch_data *ts =
                        container_of(lge_touch_kobj, struct lge_touch_data, lge_touch_kobj);
        struct lge_touch_attribute *lge_touch_priv =
                container_of(attr, struct lge_touch_attribute, attr);
        ssize_t ret = 0;

        if (lge_touch_priv->show)
                ret = lge_touch_priv->show(ts, buf);

        return ret;
}

static ssize_t lge_touch_attr_store(struct kobject *lge_touch_kobj, struct attribute *attr,
                              const char *buf, size_t count)
{
        struct lge_touch_data *ts =
                container_of(lge_touch_kobj, struct lge_touch_data, lge_touch_kobj);
        struct lge_touch_attribute *lge_touch_priv =
                container_of(attr, struct lge_touch_attribute, attr);
        ssize_t ret = 0;

        if (lge_touch_priv->store)
                ret = lge_touch_priv->store(ts, buf, count);

        return ret;
}

static const struct sysfs_ops lge_touch_sysfs_ops = {
        .show   = lge_touch_attr_show,
        .store  = lge_touch_attr_store,
};

static struct kobj_type lge_touch_kobj_type = {
        .sysfs_ops              = &lge_touch_sysfs_ops,
        .default_attrs  = lge_touch_attribute_list,
};

static int touch_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct lge_touch_data *ts = NULL;
	int ret = 0;
#ifdef IS_MTK
	struct task_struct *thread = NULL;
	int err=0;
#endif

	TOUCH_INFO_MSG("%s \n", __func__);

	/* Init  Lock */
	wake_lock_init(&touch_wake_lock, WAKE_LOCK_SUSPEND, "touch_irq");

	/* I2C Check & Register */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		TOUCH_ERR_MSG("i2c functionality check error\n");
		ret = -EPERM;
		goto err_check_functionality_failed;
	}

	ts = devm_kzalloc(&client->dev, sizeof(struct lge_touch_data), GFP_KERNEL);
	if (ts == NULL) {
		TOUCH_ERR_MSG("Can not allocate memory\n");
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	ts->client = client;
	i2c_set_clientdata(client, ts);
	g_ts = ts;

	/* Get pinctrl if target uses pinctrl */

	if ((ret = touch_init_platform_data(client)) < 0)
	{
		TOUCH_ERR_MSG("touch_init_platform_data faild\n");
		goto err_assign_platform_data;
	}

	/* Get pinctrl if target uses pinctrl */
#ifndef IS_MTK
	TOUCH_INFO_MSG("Start pinctrl \n");
	ts->ts_pinctrl = devm_pinctrl_get(&(client->dev));
	if (IS_ERR(ts->ts_pinctrl)) {
		if (PTR_ERR(ts->ts_pinctrl) == -EPROBE_DEFER) {
			TOUCH_INFO_MSG("ts_pinctrl ==  -EPROBE_DEFER\n");
			return -EPROBE_DEFER;
		}
		TOUCH_INFO_MSG("Target does not use pinctrl(ts->ts_pinctrl == NULL) \n");
		ts->ts_pinctrl = NULL;
	}

	if (ts->ts_pinctrl) {
		ts->ts_pinset_state_active = pinctrl_lookup_state(ts->ts_pinctrl, "pmx_ts_active");
		if (IS_ERR(ts->ts_pinset_state_active))
			TOUCH_ERR_MSG("cannot get ts pinctrl active state\n");

		ts->ts_pinset_state_suspend = pinctrl_lookup_state(ts->ts_pinctrl, "pmx_ts_suspend");
		if (IS_ERR(ts->ts_pinset_state_suspend))
			TOUCH_ERR_MSG("cannot get ts pinctrl active state\n");

		if (ts->ts_pinset_state_active) {
			ret = pinctrl_select_state(ts->ts_pinctrl, ts->ts_pinset_state_active);
			if (ret)
				TOUCH_INFO_MSG("cannot set ts pinctrl active state \n");
		} else {
			TOUCH_INFO_MSG("pinctrl active == NULL \n");
		}
	}
	TOUCH_INFO_MSG("End pinctrl \n");
#endif //0

	/* Specific device probe */
	if (touch_drv->probe) {
		ts->pdata->panel_on = 1;
		ret = touch_drv->probe(client, ts_pdata);
		if (ret < 0) {
			TOUCH_ERR_MSG("specific device probe fail\n");
			goto err_assign_platform_data;
		}
	}

	TOUCH_INFO_MSG("%s-%d], mt_get_gpio_in(ts_pdata->int_pin) : %d\n", __func__, __LINE__, mt_get_gpio_in(ts_pdata->int_pin));
	TOUCH_INFO_MSG("%s-%d],  mt_get_gpio_out(ts_pdata->reset_pin) : %d\n", __func__, __LINE__, mt_get_gpio_out(ts_pdata->reset_pin));
	/* reset pin setting */
#ifndef IS_MTK
	if (gpio_is_valid(ts_pdata->reset_pin)) {
		ret = gpio_request(ts_pdata->reset_pin, "touch_reset");
		if (ret < 0) {
			TOUCH_ERR_MSG("FAIL: touch_reset gpio_request\n");
			goto err_assign_platform_data;
		}
		gpio_direction_output(ts_pdata->reset_pin, 1);
	} else {
		TOUCH_INFO_MSG("reset pin valid fail : %d\n",gpio_is_valid(ts_pdata->reset_pin));
	}
#else
	mt_set_gpio_mode ( ts_pdata->reset_pin, GPIO_MODE_00 );
	mt_set_gpio_dir ( ts_pdata->reset_pin, GPIO_DIR_OUT );

	if (ret = mt_get_gpio_dir(ts_pdata->reset_pin)) {				
		if (ret < 0) {
			TOUCH_ERR_MSG("FAIL: touch_reset gpio_get\n");
			goto err_assign_platform_data;
		}
		mt_set_gpio_out(ts_pdata->reset_pin, 0);		
		mt_set_gpio_out(ts_pdata->reset_pin, 1);		
	} else {
		TOUCH_INFO_MSG("reset pin valid fail : %d\n",ret);
	}

#endif


	atomic_set(&ts->device_init, 0);
	atomic_set(&dev_state,DEV_RESUME_ENABLE);
	/* Power on */
	if (touch_power_cntl(ts, POWER_ON) < 0)
		goto err_power_failed;

	if (touch_drv->init(ts->client, &ts->fw_info) < 0) {
		TOUCH_ERR_MSG("specific device initialization fail\n");
	}

	/* init work_queue */
	INIT_DELAYED_WORK(&ts->work_touch_lock, touch_lock_func);
	INIT_DELAYED_WORK(&ts->work_init, touch_init_func);
	INIT_WORK(&ts->work_fw_upgrade, touch_fw_upgrade_func);
	INIT_DELAYED_WORK(&ts->work_crack, inspection_crack_func);
	init_completion(&ts->irq_completion);
	init_completion(&ts->fw_upgrade_completion);

	/* Specific device initialization */
	touch_ic_init(ts);
	mutex_init(&ts->thread_lock);
	touch_drv->ic_ctrl(ts->client, IC_CTRL_INFO_SHOW, 0);

	msleep(100);

	/* Firmware Upgrade Check - use thread for booting time reduction */
	if (ts_pdata->auto_fw_update && touch_drv->fw_upgrade) {
		if (likely(touch_debug_mask_ & (DEBUG_FW_UPGRADE | DEBUG_BASE_INFO)))
			TOUCH_INFO_MSG("Auto F/W upgrade \n");
		safety_reset(ts);
		queue_work(touch_wq, &ts->work_fw_upgrade);

		ret = wait_for_completion_interruptible_timeout(&ts->fw_upgrade_completion, msecs_to_jiffies(30 * MSEC_PER_SEC));

		if (ret == 0) {
			TOUCH_INFO_MSG("fw_upgrade_completion timeout \n");
		}
	}

	if(ts->pdata->role->use_crack_mode)
	{
		TOUCH_INFO_MSG("Crack Check \n");
		queue_delayed_work(touch_wq, &ts->work_crack, msecs_to_jiffies(100));
	}

	ret = touch_input_init(ts);
	if (ret < 0)
		goto err_lge_touch_input_init;


#ifndef IS_MTK	
	/* interrupt mode */
	ret = gpio_request(ts_pdata->int_pin, "touch_int");
	if (ret < 0) {
		TOUCH_ERR_MSG("FAIL: touch_int gpio_request\n");
		goto err_interrupt_failed;
	}
	gpio_direction_input(ts_pdata->int_pin);

	ret = request_threaded_irq(client->irq, touch_irq_handler, touch_thread_irq_handler,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT | IRQF_NO_SUSPEND, client->name, ts);

	if (ret < 0) {
		TOUCH_ERR_MSG("request_irq failed. use polling mode\n");
		goto err_interrupt_failed;
	}

#else

	thread = kthread_run ( touch_thread_irq_handler, ts, client->name );
	if ( IS_ERR ( thread ) )
	{
		err = PTR_ERR ( thread );
		TOUCH_ERR_MSG ( "failed to create kernel thread: %d\n", err );
	}

	TOUCH_INFO_MSG("Interrupt pin setting \n");
	mt_set_gpio_mode(ts_pdata->int_pin, GPIO_CTP_EINT_PIN_M_EINT);
	mt_set_gpio_dir(ts_pdata->int_pin, GPIO_DIR_IN);

	mt_set_gpio_pull_enable(ts_pdata->int_pin, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(ts_pdata->int_pin, GPIO_PULL_UP);

	mt_eint_registration( CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_TYPE, touch_irq_handler, 1 );

//	TOUCH_ERR_MSG("mt_eint_get_mask(CUST_EINT_TOUCH_PANEL_NUM) : %d\n" , mt_eint_get_mask(CUST_EINT_TOUCH_PANEL_NUM));
	touch_enable(client->irq);
	TOUCH_INFO_MSG("client->irq : %d // CUST_EINT_TOUCH_PANEL_NUM : %d\n", client->irq, CUST_EINT_TOUCH_PANEL_NUM);

#endif //IS_MTK

#ifdef LGE_TOUCH_GHOST_DETECTION
	if (ts_role->ghost_detection_enable) {
		hrtimer_init(&ts->trigger_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->trigger_timer.function = touch_trigger_timer_handler;
	}
#endif
	/* ghost-finger solution */
	ts->gf_ctrl.probe = 1;

#if defined(CONFIG_FB)
	ts->fb_notifier_block.notifier_call = touch_notifier_callback;
	if ((ret = fb_register_client(&ts->fb_notifier_block))) {
		TOUCH_ERR_MSG("unable to register fb_notifier callback: %d\n", ret);
		goto err_lge_touch_fb_register;
	}
#endif

#if defined(CONFIG_HAS_EARLYSUSPEND)
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = touch_early_suspend;
	ts->early_suspend.resume = touch_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	ret = subsys_system_register(&touch_subsys, NULL);
	if (ret < 0)
		TOUCH_ERR_MSG("%s, bus is not registered, ret : %d\n", __func__, ret);
	ret = device_register(&device_touch);
	if (ret < 0)
		TOUCH_ERR_MSG("%s, device is not registered, ret : %d\n", __func__, ret);

	ret = kobject_init_and_add(&ts->lge_touch_kobj, &lge_touch_kobj_type,
			ts->input_dev->dev.kobj.parent,
			"%s", LGE_TOUCH_NAME);
	if (ret < 0) {
		TOUCH_ERR_MSG("kobject_init_and_add is failed\n");
		goto err_lge_touch_sysfs_init_and_add;
	}

#if defined(TOUCH_USE_DSV)
	g_ts = ts;
	ts_pdata->use_dsv = 0;
	ts_pdata->sensor_value = 0;
	ts_pdata->enable_sensor_interlock = 1;
	apds9930_register_lux_change_callback(dsv_ctrl);
#endif

	if (likely(touch_debug_mask_ & DEBUG_BASE_INFO))
		TOUCH_INFO_MSG("Touch driver is initialized\n");
	complete_all(&ts->irq_completion);
	ts->is_probed = true;
	TOUCH_INFO_MSG("probe done\n");

	tpd_load_status = 1;

	TOUCH_INFO_MSG("%s-%d], mt_get_gpio_in(ts_pdata->int_pin) : %d\n", __func__, __LINE__, mt_get_gpio_in(ts_pdata->int_pin));
	TOUCH_INFO_MSG("%s-%d],  mt_get_gpio_out(ts_pdata->reset_pin) : %d\n", __func__, __LINE__, mt_get_gpio_out(ts_pdata->reset_pin));

	
	return 0;

err_lge_touch_sysfs_init_and_add:
	kobject_del(&ts->lge_touch_kobj);
#if defined(CONFIG_FB)
	fb_unregister_client(&ts->fb_notifier_block);
err_lge_touch_fb_register:
#endif
#if defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ts->early_suspend);
#endif
	free_irq(client->irq, ts);

err_interrupt_failed:
err_lge_touch_input_init:
	release_all_ts_event(ts);
	touch_power_cntl(ts, POWER_OFF);
err_power_failed:
err_assign_platform_data:
err_alloc_data_failed:
err_check_functionality_failed:
	return ret;
}

static int touch_remove(struct i2c_client *client)
{
	struct lge_touch_data *ts = i2c_get_clientdata(client);

	TOUCH_TRACE_FUNC();

#if defined(CONFIG_FB)
	fb_unregister_client(&ts->fb_notifier_block);
#endif
#if defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ts->early_suspend);
#endif

	/* Specific device remove */
	if (touch_drv->remove)
		touch_drv->remove(client);

	release_all_ts_event(ts);

	/* Power off */
	touch_power_cntl(ts, POWER_OFF);

	if (gpio_is_valid(ts_pdata->reset_pin)) {
		gpio_free(ts_pdata->reset_pin);
	}

	if (gpio_is_valid(ts_pdata->int_pin)) {
		gpio_free(ts_pdata->int_pin);
	}

	kobject_del(&ts->lge_touch_kobj);

#if defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ts->early_suspend);
#endif

	free_irq(client->irq, ts);

#ifdef LGE_TOUCH_GHOST_DETECTION
	if (ts_role->ghost_detection_enable) {
		hrtimer_cancel(&ts->trigger_timer);
	}
#endif
	input_unregister_device(ts->input_dev);

	mutex_destroy(&irq_lock);
	mutex_destroy(&i2c_suspend_lock);
	mutex_destroy(&ts->thread_lock);
	wake_lock_destroy(&touch_wake_lock);

	return 0;
}

#if defined(CONFIG_PM)
static struct dev_pm_ops touch_pm_ops = {
	.suspend 	= touch_suspend,
	.resume 	= touch_resume,
};
#endif

static struct of_device_id touch_match_table[] = {
	{ .compatible = "lge,touch_core", },
	{ },
};

static struct i2c_device_id touch_id_table[] = {
	{ TPD_DEV_NAME, 0 },
};

static struct i2c_driver lge_touch_driver = {
	.probe = touch_probe,
	.remove = touch_remove,
	.id_table = touch_id_table,
	.driver = {
		.name = LGE_TOUCH_NAME,
//		.owner = THIS_MODULE,
//#if defined(CONFIG_PM)
//		.pm = &touch_pm_ops,
//#endif
//		.of_match_table = touch_match_table,
	},
};

#ifdef CONFIG_LGE_PM_CHARGING_CHARGERLOGO
extern int lge_boot_mode_for_touch;
#endif

#ifdef IS_MTK

static struct i2c_board_info __initdata i2c_tpd = {	I2C_BOARD_INFO ( TPD_DEV_NAME, 0x34 ) };

static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = TPD_DEV_NAME,
	.tpd_local_init = touch_local_init,
#ifdef CONFIG_HAS_EARLYSUSPEND
	.suspend = touch_suspend,
	.resume = touch_resume,
#endif
#ifdef TPD_HAVE_BUTTON
	.tpd_have_button = 1,
#else
	.tpd_have_button = 0,
#endif
};


static int touch_local_init ( void )
{
	TOUCH_INFO_MSG("%s \n", __func__);

	if ( i2c_add_driver ( &lge_touch_driver ) != 0 )
	{
		TOUCH_ERR_MSG("FAIL: i2c_add_driver\n");
		destroy_workqueue(touch_wq);
		return -1;
	}

	if ( tpd_load_status == 0 )
	{
		TOUCH_ERR_MSG ( "touch driver probing failed\n" );
		i2c_del_driver ( &lge_touch_driver );
		return -1;
	}

	tpd_type_cap = 1;

	return 0;
}

#endif //IS_MTK

int touch_driver_register_(struct touch_device_driver *driver)
{
	int ret = 0;

	TOUCH_TRACE_FUNC();
	TOUCH_INFO_MSG("%s \n", __func__);

	if(lge_get_boot_mode() == LGE_BOOT_MODE_CHARGERLOGO) {
		ret = -EMLINK;
		TOUCH_INFO_MSG("CHARGERLOGO_MODE, not register touch-driver\n");
		goto chargerlogo_mode;
	}

	if (touch_drv != NULL) {
		TOUCH_ERR_MSG("CANNOT add new touch-driver\n");
		ret = -EMLINK;
		goto err_touch_driver_register;
	}

	touch_drv = driver; //jumper for ic ctrl func
	touch_wq = create_singlethread_workqueue("touch_wq");
	if (!touch_wq) {
		TOUCH_ERR_MSG("CANNOT create new workqueue\n");
		ret = -ENOMEM;
		goto err_work_queue;
	}
#ifdef USE_DMA
	I2CDMABuf_va = (u8 *) dma_alloc_coherent ( NULL, 4096, &I2CDMABuf_pa, GFP_KERNEL );
	if ( !I2CDMABuf_va )
	{
		TOUCH_ERR_MSG ( "Allocate Touch DMA I2C Buffer failed!\n" );
		goto err_i2c_add_buffer;
	}
#endif

#ifndef IS_MTK
	ret = i2c_add_driver(&lge_touch_driver);
	if (ret < 0) {
		TOUCH_ERR_MSG("FAIL: i2c_add_driver\n");
		goto err_i2c_add_driver;
	}
#else
	i2c_register_board_info ( 0, &i2c_tpd, 1 );
	if ( tpd_driver_add ( &tpd_device_driver ) < 0 ) //confirm touch driver using only dev's name
	{
		TOUCH_ERR_MSG ( "tpd_driver_add failed\n" );
		goto err_i2c_add_driver;
	}
#endif

	return 0;

err_i2c_add_driver:
	destroy_workqueue(touch_wq);
err_i2c_add_buffer:
err_work_queue:
err_touch_driver_register:
chargerlogo_mode:
	return ret;
}

void touch_driver_unregister_(void)
{
	TOUCH_TRACE_FUNC();

	if(lge_get_boot_mode() == LGE_BOOT_MODE_CHARGERLOGO) {
		TOUCH_INFO_MSG("CHARGERLOGO_MODE, not unregister touch-driver\n");
		return;
	}
	
	if ( I2CDMABuf_va )
	{
		dma_free_coherent ( NULL, 4096, I2CDMABuf_va, I2CDMABuf_pa );
		I2CDMABuf_va = NULL;
		I2CDMABuf_pa = 0;
	}

	i2c_del_driver(&lge_touch_driver);
	touch_drv = NULL;

	if (touch_wq)
		destroy_workqueue(touch_wq);
}
