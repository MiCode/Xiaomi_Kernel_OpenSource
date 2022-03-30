// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": %s: " fmt, __func__

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/list.h>
#include <linux/delay.h>

#include "mt6336/mt6336.h"
#include "flashlight-core.h"
#include "flashlight-dt.h"

/* device tree should be defined in flashlight-dt.h */
#ifndef MT6336_DTNAME
#define MT6336_DTNAME "mediatek,flashlights_mt6336"
#endif

#define MT6336_NAME "flashlights-mt6336"

/* define channel, level */
#define MT6336_CHANNEL_NUM 2
#define MT6336_CHANNEL_CH1 0
#define MT6336_CHANNEL_CH2 1

#define MT6336_NONE (-1)
#define MT6336_DISABLE 0
#define MT6336_ENABLE 1
#define MT6336_ENABLE_TORCH 1
#define MT6336_ENABLE_FLASH 2

#define MT6336_LEVEL_NUM 34
#define MT6336_LEVEL_TORCH 8
#define MT6336_WDT_TIMEOUT 600 /* ms */
#define MT6336_HW_TIMEOUT 600 /* ms */

/* define mutex, work queue and timer */
static DEFINE_MUTEX(mt6336_mutex);
static struct work_struct mt6336_work_ch1;
static struct work_struct mt6336_work_ch2;
static struct hrtimer mt6336_timer_ch1;
static struct hrtimer mt6336_timer_ch2;
static unsigned int mt6336_timeout_ms[MT6336_CHANNEL_NUM];

/* define usage count */
static int use_count;

/* mt6336 pmic control handler */
static struct mt6336_ctrl *flashlight_ctrl;

/* platform data */
struct mt6336_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};


/******************************************************************************
 * mt6336 operations
 *****************************************************************************/
static const int mt6336_current[MT6336_LEVEL_NUM] = {
	  25,   50,   75,  100,  125,  150,  175,  200,  250,  300,
	 350,  400,  450,  500,  550,  600,  650,  700,  750,  800,
	 850,  900,  950, 1000, 1050, 1100, 1150, 1200, 1250, 1300,
	1350, 1400, 1450, 1500
};

static const unsigned char mt6336_level[MT6336_LEVEL_NUM] = {
	0x01, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x0F, 0x13, 0x17,
	0x1B, 0x1F, 0x23, 0x27, 0x2B, 0x2F, 0x33, 0x37, 0x3B, 0x3F,
	0x43, 0x47, 0x4B, 0x4F, 0x53, 0x57, 0x5B, 0x5F, 0x63, 0x67,
	0x6B, 0x6F, 0x73, 0x77
};

static int is_preenable;
static int mt6336_decouple_mode;
static int mt6336_en_ch1;
static int mt6336_en_ch2;
static int mt6336_level_ch1;
static int mt6336_level_ch2;

static int mt6336_is_torch(int level)
{
	if (level >= MT6336_LEVEL_TORCH)
		return -1;

	return 0;
}

static int mt6336_verify_level(int level)
{
	if (level < 0)
		level = 0;
	else if (level >= MT6336_LEVEL_NUM)
		level = MT6336_LEVEL_NUM - 1;

	return level;
}

/* pre-enable steps before enable flashlight */
static int mt6336_preenable(void)
{
	int ret;

	ret = mt6336_set_register_value(0x0519, 0x0A);
	/* mt6336_set_flag_register_value(
	 * MT6336_RG_A_LOOP_GM_EN, 0x06);
	 * mt6336_set_flag_register_value(
	 * MT6336_RG_A_LOOP_CLAMP_EN, 0x01);
	 */

	ret = mt6336_set_register_value(0x0520, 0x00);
	/* mt6336_set_flag_register_value(
	 * MT6336_RG_A_LOOP_GM_TUNE_SYS_MSB, 0x00);
	 */

	ret = mt6336_set_register_value(0x055A, 0x01);
	/* mt6336_set_flag_register_value(
	 * MT6336_RG_A_SWCHR_RSV_TRIM_MSB, 0x01);
	 */

	ret = mt6336_set_register_value(0x0455, 0x00);
	/* mt6336_set_flag_register_value(
	 * MT6336_RG_EN_HW_GAIN_SET, 0x00);
	 */

	ret = mt6336_set_register_value(0x03C9, 0x00);
	/* mt6336_set_flag_register_value(
	 * MT6336_AUXADC_HWGAIN_EN, 0x00);
	 */

	ret = mt6336_set_register_value(0x0553, 0x54);
	/* mt6336_set_flag_register_value(
	 * MT6336_RG_A_PWR_LG_DTC, 0x01);
	 * mt6336_set_flag_register_value(
	 * MT6336_RG_A_PWR_LG_SRCEH, 0x01);
	 * mt6336_set_flag_register_value(
	 * MT6336_RG_A_PWR_LG_SRC, 0x01);
	 * mt6336_set_flag_register_value(
	 * MT6336_RG_A_PWR_LG_VTHSEL, 0x00);
	 */

	ret = mt6336_set_register_value(0x055F, 0xE0);
	/* mt6336_set_flag_register_value(
	 * MT6336_RG_A_LOOP_FTR_RC, 0x03);
	 * mt6336_set_flag_register_value(
	 * MT6336_RG_A_LOOP_FTR_DROP, 0x04);
	 * mt6336_set_flag_register_value(
	 * MT6336_RG_A_LOOP_FTR_SHOOT_EN, 0x00);
	 * mt6336_set_flag_register_value(
	 * MT6336_RG_A_LOOP_FTR_DROP_EN, 0x00);
	 * mt6336_set_flag_register_value(
	 * MT6336_RG_A_SWCHR_ZCD_TRIM_EN, 0x00);
	 */

	ret = mt6336_set_register_value(0x053D, 0x45);
	/* mt6336_set_flag_register_value(
	 * MT6336_RG_VRAMP_SLP, 0x04);
	 * mt6336_set_flag_register_value(
	 * MT6336_RG_A_VRAMP_DCOS, 0x05);
	 */

	/* only way to verify register is to get again */
	ret = 0;

	return ret;
}

/* post-enable steps after enable flashlight */
static int mt6336_postenable(void)
{
	int ret;

	ret = mt6336_set_register_value(0x052A, 0x88);
	/* mt6336_set_flag_register_value(
	 * MT6336_RG_A_LOOP_GM_RSV_LSB, 0x88);
	 */

	ret = mt6336_set_register_value(0x0553, 0x14);
	/* mt6336_set_flag_register_value(
	 * MT6336_RG_A_PWR_LG_DTC, 0x00);
	 * mt6336_set_flag_register_value(
	 * MT6336_RG_A_PWR_LG_SRCEH, 0x01);
	 * mt6336_set_flag_register_value(
	 * MT6336_RG_A_PWR_LG_SRC, 0x01);
	 * mt6336_set_flag_register_value(
	 * MT6336_RG_A_PWR_LG_VTHSEL, 0x00);
	 */

	ret = mt6336_set_register_value(0x0519, 0x3E);
	/* mt6336_set_flag_register_value(
	 * MT6336_RG_A_LOOP_GM_EN, 0x1F);
	 * mt6336_set_flag_register_value(
	 * MT6336_RG_A_LOOP_CLAMP_EN, 0x01);
	 */

	ret = mt6336_set_register_value(0x051E, 0x02);
	/* mt6336_set_flag_register_value(
	 * MT6336_RG_A_LOOP_GM_TUNE_ICHIN_MSB, 0x02);
	 */

	ret = mt6336_set_register_value(0x0520, 0x04);
	/* mt6336_set_flag_register_value(
	 * MT6336_RG_A_LOOP_GM_TUNE_SYS_MSB, 0x04);
	 */

	ret = mt6336_set_register_value(0x055A, 0x00);
	/* mt6336_set_flag_register_value(
	 * MT6336_RG_A_SWCHR_RSV_TRIM_MSB, 0x00);
	 */

	ret = mt6336_set_register_value(0x0455, 0x01);
	/* mt6336_set_flag_register_value(
	 * MT6336_RG_EN_HW_GAIN_SET, 0x01);
	 */

	ret = mt6336_set_register_value(0x03C9, 0x10);
	/* mt6336_set_flag_register_value(
	 * MT6336_AUXADC_HWGAIN_EN, 0x01);
	 */

	ret = mt6336_set_register_value(0x03CF, 0x03);
	/* mt6336_set_flag_register_value(
	 * MT6336_AUXADC_HWGAIN_DET_PRD_M, 0x03);
	 */

	ret = mt6336_set_register_value(0x0402, 0x03);
	/* mt6336_set_flag_register_value(
	 * MT6336_RG_DIS_REVFET, 0x00);
	 * mt6336_set_flag_register_value(
	 * MT6336_RG_T_TERM_EXT, 0x00);
	 * mt6336_set_flag_register_value(
	 * MT6336_RG_EN_TERM, 0x01);
	 * mt6336_set_flag_register_value(
	 * MT6336_RG_EN_RECHARGE, 0x01);
	 */

	ret = mt6336_set_register_value(0x0529, 0x80);
	/* mt6336_set_flag_register_value(
	 * MT6336_RG_A_LOOP_GM_RSV_MSB, 0x88);
	 */

	ret = mt6336_set_register_value(0x051F, 0x84);
	ret = mt6336_set_register_value(0x053D, 0x47);

	/* only way to verify register is to get again */
	ret = 0;

	return ret;
}

/* flashlight enable function */
static int mt6336_enable(void)
{
	u8 buck_enable_status;

	mt6336_ctrl_enable(flashlight_ctrl);

	if ((mt6336_en_ch1 == MT6336_ENABLE_FLASH)
			|| (mt6336_en_ch2 == MT6336_ENABLE_FLASH)) {
		/* flash mode */
		if (!mt6336_get_flag_register_value(
					MT6336_DA_QI_OTG_MODE_MUX)) {

			/* disable charging */
			/* mt6336_set_register_value(0x0400, 0x00); */
			buck_enable_status = mt6336_get_flag_register_value(
					MT6336_RG_EN_BUCK);
			mt6336_set_flag_register_value(MT6336_RG_EN_BUCK, 0x00);
			/*
			 * mt6336_set_flag_register_value(
			 * MT6336_RG_EN_CHARGE, 0x00);
			 */

			/* pre-enable steps */
			mt6336_preenable();
			is_preenable = 1;

			/* enable flash mode, source current */
			if (mt6336_en_ch1)
				mt6336_set_flag_register_value(
						MT6336_RG_EN_IFLA1, 0x01);
			if (mt6336_en_ch2)
				mt6336_set_flag_register_value(
						MT6336_RG_EN_IFLA2, 0x01);
			mt6336_set_flag_register_value(
					MT6336_RG_EN_LEDCS, 0x01);

			/* enable flash and apply previous buck setting */
			mt6336_set_flag_register_value(MT6336_RG_EN_BUCK,
					buck_enable_status);
			mt6336_set_flag_register_value(MT6336_RG_EN_FLASH,
					0x01);
		} else {
			/*  failed in OTG mode */
			pr_info("Failed to turn on flash mode since in OTG mode.\n");
		}

	} else {
		/* torch mode */

		/* pre-enable steps */
		if (!mt6336_get_flag_register_value(
					MT6336_DA_QI_OTG_MODE_MUX) &&
				!mt6336_get_flag_register_value(
					MT6336_DA_QI_VBUS_PLUGIN_MUX)) {
			mt6336_preenable();
			is_preenable = 1;
		}

		/* enable torch mode, source current and enable torch */
		if (mt6336_en_ch1)
			mt6336_set_flag_register_value(
					MT6336_RG_EN_ITOR1, 0x01);
		if (mt6336_en_ch2)
			mt6336_set_flag_register_value(
					MT6336_RG_EN_ITOR2, 0x01);
		mt6336_set_flag_register_value(MT6336_RG_EN_LEDCS, 0x01);
		mt6336_set_flag_register_value(MT6336_RG_EN_TORCH, 0x01);
	}

	mt6336_ctrl_disable(flashlight_ctrl);

	mt6336_en_ch1 = MT6336_NONE;
	mt6336_en_ch2 = MT6336_NONE;

	return 0;
}

/* flashlight disable function */
static int mt6336_disable(void)
{
	mt6336_ctrl_enable(flashlight_ctrl);

	/* disable torch/flash mode */
	mt6336_set_flag_register_value(MT6336_RG_EN_ITOR1, 0x00);
	mt6336_set_flag_register_value(MT6336_RG_EN_ITOR2, 0x00);
	mt6336_set_flag_register_value(MT6336_RG_EN_IFLA1, 0x00);
	mt6336_set_flag_register_value(MT6336_RG_EN_IFLA2, 0x00);

	/* pre-enable steps */
	if (is_preenable) {
		mt6336_postenable();
		is_preenable = 0;
	}

	/* disable source current and disable torch/flash */
	mt6336_set_flag_register_value(MT6336_RG_EN_LEDCS, 0x00);
	mt6336_set_flag_register_value(MT6336_RG_EN_TORCH, 0x00);
	mt6336_set_flag_register_value(MT6336_RG_EN_FLASH, 0x00);

	mt6336_ctrl_disable(flashlight_ctrl);

	mt6336_en_ch1 = MT6336_NONE;
	mt6336_en_ch2 = MT6336_NONE;

	return 0;
}

/* set flashlight level */
static int mt6336_set_level_ch1(int level)
{
	level = mt6336_verify_level(level);
	mt6336_level_ch1 = level;

	/* set brightness level */
	mt6336_ctrl_enable(flashlight_ctrl);
	mt6336_set_flag_register_value(MT6336_RG_ITOR1, mt6336_level[level]);
	mt6336_set_flag_register_value(MT6336_RG_IFLA1, mt6336_level[level]);
	mt6336_ctrl_disable(flashlight_ctrl);

	return 0;
}

static int mt6336_set_level_ch2(int level)
{
	level = mt6336_verify_level(level);
	mt6336_level_ch2 = level;

	/* set brightness level */
	mt6336_ctrl_enable(flashlight_ctrl);
	mt6336_set_flag_register_value(MT6336_RG_ITOR2, mt6336_level[level]);
	mt6336_set_flag_register_value(MT6336_RG_IFLA2, mt6336_level[level]);
	mt6336_ctrl_disable(flashlight_ctrl);

	return 0;
}

static int mt6336_set_level(int channel, int level)
{
	if (channel == MT6336_CHANNEL_CH1)
		mt6336_set_level_ch1(level);
	else if (channel == MT6336_CHANNEL_CH2)
		mt6336_set_level_ch2(level);
	else {
		pr_info("Error channel\n");
		return -1;
	}

	return 0;
}

static int mt6336_set_scenario(int scenario)
{
	/* set decouple mode */
	mt6336_decouple_mode = scenario & FLASHLIGHT_SCENARIO_DECOUPLE_MASK;

	return 0;
}

/* flashlight init */
int mt6336_init(void)
{
	int ret = 0;

	mt6336_ctrl_enable(flashlight_ctrl);

	/* clear flash/torch mode enable register */
	mt6336_set_flag_register_value(MT6336_RG_EN_IFLA1, 0x00);
	mt6336_set_flag_register_value(MT6336_RG_EN_IFLA2, 0x00);
	mt6336_set_flag_register_value(MT6336_RG_EN_ITOR1, 0x00);
	mt6336_set_flag_register_value(MT6336_RG_EN_ITOR2, 0x00);

	/* clear current source register */
	mt6336_set_flag_register_value(MT6336_RG_EN_LEDCS, 0x00);

	/* clear flash/torch enable register */
	mt6336_set_flag_register_value(MT6336_RG_EN_FLASH, 0x00);
	mt6336_set_flag_register_value(MT6336_RG_EN_TORCH, 0x00);

	/* setup flash mode output regulation voltage (default: 5.0 V) */
	mt6336_set_flag_register_value(MT6336_RG_VFLA, 0x05);

	/* setup flash mode current step up/down timing (default: 10 us) */
	mt6336_set_flag_register_value(MT6336_RG_TSTEP_ILED, 0x01);

	/* setup flash/torch watchdog timeout (default: 600 ms) */
	mt6336_set_flag_register_value(MT6336_RG_FLA_WDT, 0x10);

	/* enable flash watchdog (default) */
	mt6336_set_flag_register_value(MT6336_RG_EN_FLA_WDT, 0x01);

	/* disable torch watchdog (default) */
	mt6336_set_flag_register_value(MT6336_RG_EN_TOR_WDT, 0x00);

	mt6336_ctrl_disable(flashlight_ctrl);

	/* clear flashlight state */
	mt6336_en_ch1 = MT6336_NONE;
	mt6336_en_ch2 = MT6336_NONE;

	/* clear decouple mode */
	mt6336_decouple_mode = FLASHLIGHT_SCENARIO_COUPLE;

	is_preenable = 0;

	return ret;
}

/* flashlight uninit */
int mt6336_uninit(void)
{
	/* clear flashlight state */
	mt6336_en_ch1 = MT6336_NONE;
	mt6336_en_ch2 = MT6336_NONE;

	/* clear decouple mode */
	mt6336_decouple_mode = FLASHLIGHT_SCENARIO_COUPLE;

	return mt6336_disable();
}


/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
void mt6336_isr_short_ch1(void)
{
	schedule_work(&mt6336_work_ch1);
}

void mt6336_isr_short_ch2(void)
{
	schedule_work(&mt6336_work_ch2);
}

/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static void mt6336_work_disable_ch1(struct work_struct *data)
{
	pr_debug("ht work queue callback\n");
	mt6336_disable();
}

static void mt6336_work_disable_ch2(struct work_struct *data)
{
	pr_debug("lt work queue callback\n");
	mt6336_disable();
}

static enum hrtimer_restart mt6336_timer_func_ch1(struct hrtimer *timer)
{
	schedule_work(&mt6336_work_ch1);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart mt6336_timer_func_ch2(struct hrtimer *timer)
{
	schedule_work(&mt6336_work_ch2);
	return HRTIMER_NORESTART;
}

static int mt6336_timer_start(int channel, ktime_t ktime)
{
	if (channel == MT6336_CHANNEL_CH1)
		hrtimer_start(&mt6336_timer_ch1, ktime, HRTIMER_MODE_REL);
	else if (channel == MT6336_CHANNEL_CH2)
		hrtimer_start(&mt6336_timer_ch2, ktime, HRTIMER_MODE_REL);
	else {
		pr_info("Error channel\n");
		return -1;
	}

	return 0;
}

static int mt6336_timer_cancel(int channel)
{
	if (channel == MT6336_CHANNEL_CH1)
		hrtimer_cancel(&mt6336_timer_ch1);
	else if (channel == MT6336_CHANNEL_CH2)
		hrtimer_cancel(&mt6336_timer_ch2);
	else {
		pr_info("Error channel\n");
		return -1;
	}

	return 0;
}

/******************************************************************************
 * Flashlight operation wrapper function
 *****************************************************************************/
static int mt6336_operate(int channel, int enable)
{
	ktime_t ktime;
	unsigned int s;
	unsigned int ns;

	/* setup enable/disable */
	if (channel == MT6336_CHANNEL_CH1) {
		mt6336_en_ch1 = enable;
		if (mt6336_en_ch1)
			if (mt6336_is_torch(mt6336_level_ch1))
				mt6336_en_ch1 = MT6336_ENABLE_FLASH;
	} else if (channel == MT6336_CHANNEL_CH2) {
		mt6336_en_ch2 = enable;
		if (mt6336_en_ch2)
			if (mt6336_is_torch(mt6336_level_ch2))
				mt6336_en_ch2 = MT6336_ENABLE_FLASH;
	} else {
		pr_info("Error channel\n");
		return -1;
	}

	/* decouple mode */
	if (mt6336_decouple_mode) {
		if (channel == MT6336_CHANNEL_CH1)
			mt6336_en_ch2 = MT6336_DISABLE;
		else if (channel == MT6336_CHANNEL_CH2)
			mt6336_en_ch1 = MT6336_DISABLE;
	}

	/* operate flashlight and setup timer */
	if ((mt6336_en_ch1 != MT6336_NONE) && (mt6336_en_ch2 != MT6336_NONE)) {
		if ((mt6336_en_ch1 == MT6336_DISABLE) &&
				(mt6336_en_ch2 == MT6336_DISABLE)) {
			mt6336_disable();
			mt6336_timer_cancel(MT6336_CHANNEL_CH1);
			mt6336_timer_cancel(MT6336_CHANNEL_CH2);
		} else {
			if (mt6336_timeout_ms[MT6336_CHANNEL_CH1]) {
				s = mt6336_timeout_ms[MT6336_CHANNEL_CH1] /
					1000;
				ns = mt6336_timeout_ms[MT6336_CHANNEL_CH1] %
						1000 * 1000000;
				ktime = ktime_set(s, ns);
				mt6336_timer_start(MT6336_CHANNEL_CH1, ktime);
			}
			if (mt6336_timeout_ms[MT6336_CHANNEL_CH2]) {
				s = mt6336_timeout_ms[MT6336_CHANNEL_CH2] /
					1000;
				ns = mt6336_timeout_ms[MT6336_CHANNEL_CH2] %
						1000 * 1000000;
				ktime = ktime_set(s, ns);
				mt6336_timer_start(MT6336_CHANNEL_CH2, ktime);
			}
			mt6336_enable();
		}

		/* clear flashlight state */
		mt6336_en_ch1 = MT6336_NONE;
		mt6336_en_ch2 = MT6336_NONE;
	}

	return 0;
}

/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int mt6336_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	/* verify channel */
	if (channel < 0 || channel >= MT6336_CHANNEL_NUM) {
		pr_info("Failed with error channel\n");
		return -EINVAL;
	}

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		pr_debug("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				channel, (int)fl_arg->arg);
		mt6336_timeout_ms[channel] = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		pr_debug("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		mt6336_set_level(channel, fl_arg->arg);
		break;

	case FLASH_IOC_SET_SCENARIO:
		pr_debug("FLASH_IOC_SET_SCENARIO(%d): %d\n",
				channel, (int)fl_arg->arg);
		mt6336_set_scenario(fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		pr_debug("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		mt6336_operate(channel, fl_arg->arg);
		break;

	case FLASH_IOC_GET_DUTY_NUMBER:
		pr_debug("FLASH_IOC_GET_DUTY_NUMBER(%d)\n", channel);
		fl_arg->arg = MT6336_LEVEL_NUM;
		break;

	case FLASH_IOC_GET_MAX_TORCH_DUTY:
		pr_debug("FLASH_IOC_GET_MAX_TORCH_DUTY(%d)\n", channel);
		fl_arg->arg = MT6336_LEVEL_TORCH - 1;
		break;

	case FLASH_IOC_GET_DUTY_CURRENT:
		fl_arg->arg = mt6336_verify_level(fl_arg->arg);
		pr_debug("FLASH_IOC_GET_DUTY_CURRENT(%d): %d\n",
				channel, (int)fl_arg->arg);
		fl_arg->arg = mt6336_current[fl_arg->arg];
		break;

	case FLASH_IOC_GET_HW_TIMEOUT:
		pr_debug("FLASH_IOC_GET_HW_TIMEOUT(%d)\n", channel);
		fl_arg->arg = MT6336_HW_TIMEOUT;
		break;

	default:
		pr_info("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int mt6336_open(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int mt6336_release(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int mt6336_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	mutex_lock(&mt6336_mutex);
	if (set) {
		if (!use_count)
			ret = mt6336_init();
		use_count++;
		pr_debug("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = mt6336_uninit();
		if (use_count < 0)
			use_count = 0;
		pr_debug("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&mt6336_mutex);

	return ret;
}

static ssize_t mt6336_strobe_store(struct flashlight_arg arg)
{
	mt6336_set_driver(1);
	mt6336_set_scenario(FLASHLIGHT_SCENARIO_COUPLE);
	mt6336_set_level(arg.channel, arg.level);
	mt6336_timeout_ms[arg.channel] = 0;

	if (arg.level < 0)
		mt6336_operate(arg.channel, MT6336_DISABLE);
	else
		mt6336_operate(arg.channel, MT6336_ENABLE);

	msleep(arg.dur);
	mt6336_operate(arg.channel, MT6336_DISABLE);
	mt6336_set_driver(0);

	return 0;
}

static struct flashlight_operations mt6336_ops = {
	mt6336_open,
	mt6336_release,
	mt6336_ioctl,
	mt6336_strobe_store,
	mt6336_set_driver
};


/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int mt6336_parse_dt(struct device *dev,
		struct mt6336_platform_data *pdata)
{
	struct device_node *np, *cnp;
	u32 decouple = 0;
	int i = 0;

	if (!dev || !dev->of_node || !pdata)
		return -ENODEV;

	np = dev->of_node;

	pdata->channel_num = of_get_child_count(np);
	if (!pdata->channel_num) {
		pr_info("Parse no dt, node.\n");
		return 0;
	}
	pr_info("Channel number(%d).\n", pdata->channel_num);

	if (of_property_read_u32(np, "decouple", &decouple))
		pr_info("Parse no dt, decouple.\n");

	pdata->dev_id = devm_kzalloc(dev,
			pdata->channel_num *
			sizeof(struct flashlight_device_id),
			GFP_KERNEL);
	if (!pdata->dev_id)
		return -ENOMEM;

	for_each_child_of_node(np, cnp) {
		if (of_property_read_u32(cnp, "type", &pdata->dev_id[i].type))
			goto err_node_put;
		if (of_property_read_u32(cnp, "ct", &pdata->dev_id[i].ct))
			goto err_node_put;
		if (of_property_read_u32(cnp, "part", &pdata->dev_id[i].part))
			goto err_node_put;
		snprintf(pdata->dev_id[i].name, FLASHLIGHT_NAME_SIZE,
				MT6336_NAME);
		pdata->dev_id[i].channel = i;
		pdata->dev_id[i].decouple = decouple;

		pr_info("Parse dt (type,ct,part,name,channel,decouple)=(%d,%d,%d,%s,%d,%d).\n",
				pdata->dev_id[i].type, pdata->dev_id[i].ct,
				pdata->dev_id[i].part, pdata->dev_id[i].name,
				pdata->dev_id[i].channel,
				pdata->dev_id[i].decouple);
		i++;
	}

	return 0;

err_node_put:
	of_node_put(cnp);
	return -EINVAL;
}


static int mt6336_probe(struct platform_device *pdev)
{
	struct mt6336_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int ret;
	int i;

	pr_debug("Probe start.\n");

	/* parse dt */
	if (!pdata) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
		pdev->dev.platform_data = pdata;
		ret = mt6336_parse_dt(&pdev->dev, pdata);
		if (ret)
			return ret;
	}

	/* init work queue */
	INIT_WORK(&mt6336_work_ch1, mt6336_work_disable_ch1);
	INIT_WORK(&mt6336_work_ch2, mt6336_work_disable_ch2);

	/* init timer */
	hrtimer_init(&mt6336_timer_ch1, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	mt6336_timer_ch1.function = mt6336_timer_func_ch1;
	hrtimer_init(&mt6336_timer_ch2, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	mt6336_timer_ch2.function = mt6336_timer_func_ch2;
	mt6336_timeout_ms[MT6336_CHANNEL_CH1] = 600;
	mt6336_timeout_ms[MT6336_CHANNEL_CH2] = 600;

	/* get mt6336 pmic control handler */
	flashlight_ctrl = mt6336_ctrl_get("mt6336_flashlight");

	/* register and enable mt6336 pmic ISR */
	mt6336_ctrl_enable(flashlight_ctrl);
	mt6336_register_interrupt_callback(
			MT6336_INT_LED1_SHORT, mt6336_isr_short_ch1);
	mt6336_register_interrupt_callback(
			MT6336_INT_LED2_SHORT, mt6336_isr_short_ch2);
	/* mt6336_register_interrupt_callback(MT6336_INT_LED1_OPEN, NULL); */
	/* mt6336_register_interrupt_callback(MT6336_INT_LED2_OPEN, NULL); */
	mt6336_enable_interrupt(MT6336_INT_LED1_SHORT, "flashlight");
	mt6336_enable_interrupt(MT6336_INT_LED2_SHORT, "flashlight");
	/* mt6336_enable_interrupt(MT6336_INT_LED1_OPEN, "flashlight"); */
	/* mt6336_enable_interrupt(MT6336_INT_LED2_OPEN, "flashlight"); */
	mt6336_ctrl_disable(flashlight_ctrl);

	/* clear usage count */
	use_count = 0;

	/* register flashlight device */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			if (flashlight_dev_register_by_device_id(
						&pdata->dev_id[i],
						&mt6336_ops))
				return -EFAULT;
	} else {
		if (flashlight_dev_register(MT6336_NAME, &mt6336_ops))
			return -EFAULT;
	}

	pr_debug("Probe done.\n");

	return 0;
}

static int mt6336_remove(struct platform_device *pdev)
{
	struct mt6336_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int i;

	pr_debug("Remove start.\n");

	pdev->dev.platform_data = NULL;

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(
					&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(MT6336_NAME);

	/* flush work queue */
	flush_work(&mt6336_work_ch1);
	flush_work(&mt6336_work_ch2);

	/* disable mt6336 pmic ISR */
	mt6336_ctrl_enable(flashlight_ctrl);
	mt6336_disable_interrupt(MT6336_INT_LED1_SHORT, "flashlight");
	mt6336_disable_interrupt(MT6336_INT_LED2_SHORT, "flashlight");
	/* mt6336_disable_interrupt(MT6336_INT_LED1_OPEN, "flashlight"); */
	/* mt6336_disable_interrupt(MT6336_INT_LED2_OPEN, "flashlight"); */
	mt6336_ctrl_disable(flashlight_ctrl);

	pr_debug("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt6336_of_match[] = {
	{.compatible = MT6336_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, mt6336_of_match);
#else
static struct platform_device mt6336_platform_device[] = {
	{
		.name = MT6336_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, mt6336_platform_device);
#endif

static struct platform_driver mt6336_platform_driver = {
	.probe = mt6336_probe,
	.remove = mt6336_remove,
	.driver = {
		.name = MT6336_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = mt6336_of_match,
#endif
	},
};

static int __init flashlight_mt6336_init(void)
{
	int ret;

	pr_debug("Init start.\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&mt6336_platform_device);
	if (ret) {
		pr_info("Failed to register platform device\n");
		return ret;
	}
#endif

	ret = platform_driver_register(&mt6336_platform_driver);
	if (ret) {
		pr_info("Failed to register platform driver\n");
		return ret;
	}

	pr_debug("Init done.\n");

	return 0;
}

static void __exit flashlight_mt6336_exit(void)
{
	pr_debug("Exit start.\n");

	platform_driver_unregister(&mt6336_platform_driver);

	pr_debug("Exit done.\n");
}

/* replace module_init() since conflict in kernel init process */
late_initcall(flashlight_mt6336_init);
module_exit(flashlight_mt6336_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Simon Wang <Simon-TCH.Wang@mediatek.com>");
MODULE_DESCRIPTION("MTK Flashlight MT6336 Driver");

