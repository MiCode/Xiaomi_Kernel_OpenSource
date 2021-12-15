/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/leds.h>
#include <linux/of.h>
#include <linux/workqueue.h>
#include <linux/pm_wakeup.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/sched/clock.h>


#ifdef CONFIG_MTK_AAL_SUPPORT
#include <ddp_aal.h>
/* #include <linux/aee.h> */
#endif

#ifdef CONFIG_MTK_PWM
#include <mt-plat/mtk_pwm.h>
#endif

#ifdef CONFIG_MTK_PMIC_NEW_ARCH
#include <mt-plat/upmu_common.h>
#endif

#ifdef CONFIG_MTK_FB
#include <ddp_gamma.h>
#include "ddp_pwm.h"
#include "mtkfb.h"
#endif

#define MET_USER_EVENT_SUPPORT
#ifdef MET_USER_EVENT_SUPPORT
#include <mt-plat/met_drv.h>
#endif

#include "mtk_leds_sw.h"
#include "mtk_leds_hal.h"
#include "../mtk_leds_drv.h"

/* for LED&Backlight bringup, define the dummy API */
#ifndef CONFIG_MTK_PMIC_NEW_ARCH
u16 pmic_set_register_value(u32 flagname, u32 val)
{
	return 0;
}
#endif

/* #ifndef CONFIG_BACKLIGHT_SUPPORT_LM3697 */

int disp_bls_set_backlight(int level_1024)
{
	return 0;
}

void disp_pq_notify_backlight_changed(int bl_1024);
//{
//}

static DEFINE_MUTEX(leds_mutex);
static DEFINE_MUTEX(leds_pmic_mutex);

/****************************************************************************
 * variables
 ***************************************************************************/
#define CLK_DIV1 0

static unsigned int bl_brightness_hal = 102;
#ifdef CONFIG_MTK_PWM
static unsigned int bl_duty_hal = 21;
#endif
static unsigned int bl_div_hal = CLK_DIV1;

/* for button led don't do ISINK disable first time */
static int button_flag_isink0;
static int button_flag_isink1;
struct wakeup_source leds_suspend_lock;
struct cust_mt65xx_led *pled_dtsi;

char *leds_name[TYPE_TOTAL] = {
	"red",
	"green",
	"blue",
	"jogball-backlight",
	"keyboard-backlight",
	"button-backlight",
	"lcd-backlight",
};

/****************************************************************************
 * DEBUG MACROS
 ***************************************************************************/
static int debug_enable_led_hal = 1;
#define LEDS_DEBUG(format, args...) do { \
	if (debug_enable_led_hal) {	\
		pr_debug("[LED]"format, ##args);\
	} \
} while (0)
/*****************PWM *************************************************/
#define PWM_DIV_NUM 8
#ifdef CONFIG_MTK_PWM
static int time_array_hal[PWM_DIV_NUM] = {
	256, 512, 1024, 2048, 4096, 8192, 16384, 32768 };
static unsigned int backlight_PWM_div_hal = CLK_DIV1;
static unsigned int div_array_hal[PWM_DIV_NUM] = {
	1, 2, 4, 8, 16, 32, 64, 128 };
#endif

/****************************************************************************
 * func:return global variables
 ***************************************************************************/
static unsigned long long current_t, last_time;
static int count;
static char buffer[4096] = "[BL] Set Backlight directly ";

static void backlight_debug_log(int level, int mappingLevel)
{
	unsigned long cur_time_mod = 0;
	unsigned long long cur_time_display = 0;
	int ret = 0;

	current_t = sched_clock();
	cur_time_display = current_t;
	do_div(cur_time_display, 1000000);
	cur_time_mod = do_div(cur_time_display, 1000);

	ret = snprintf(buffer + strlen(buffer),
		4095 - strlen(buffer), "T:%lld.%ld,L:%d map:%d    ",
		cur_time_display, cur_time_mod, level, mappingLevel);

	count++;

	if (ret < 0 || ret >= 4096) {
		pr_info("print log error!");
		count = 5;
	}

	if (level == 0 || count >= 5 || (current_t - last_time) > 1000000000) {
		LEDS_DEBUG("%s", buffer);
		count = 0;
		buffer[strlen("Set Backlight directly ")] = '\0';
	}

	last_time = sched_clock();
}

void mt_leds_wake_lock_init(void)
{
	wakeup_source_init(&leds_suspend_lock, "leds wakelock");
}

struct cust_mt65xx_led *get_cust_led_dtsi(void)
{
	struct device_node *led_node = NULL;
	bool isSupportDTS = false;
	int i, ret;
	int mode, data, led_bits;
	int pwm_config[5] = { 0 };

	if (pled_dtsi)
		goto out;

	pr_info("[LED] %s pled_dtsi is null, load dts file\n", __func__);
	pled_dtsi = kmalloc(TYPE_TOTAL * sizeof(struct cust_mt65xx_led),
			GFP_KERNEL);
	if (pled_dtsi == NULL) {
		LEDS_DEBUG("%s kmalloc fail\n", __func__);
		goto out;
	}

	for (i = 0; i < TYPE_TOTAL; i++) {
		char node_name[32] = "mediatek,";

		if (strlen(node_name) + strlen(leds_name[i]) + 1 >
				sizeof(node_name)) {
			LEDS_DEBUG("buffer for %s%s not enough\n",
			node_name, leds_name[i]);
			pled_dtsi[i].mode = 0;
			pled_dtsi[i].data = -1;
			continue;
		}

		pled_dtsi[i].name = leds_name[i];
		led_node = of_find_compatible_node(NULL, NULL,
			strncat(node_name, leds_name[i],
			sizeof(node_name) - strlen(node_name) - 1));
		if (!led_node) {
			pr_info("[LED]Cannot find LED node from dts\n");
			pled_dtsi[i].mode = 0;
			pled_dtsi[i].data = -1;
			continue;
		}

		isSupportDTS = true;
		ret = of_property_read_u32(led_node, "led_mode", &mode);
		if (!ret) {
			pled_dtsi[i].mode = mode;
			LEDS_DEBUG("The %s's led mode is : %d\n",
				pled_dtsi[i].name, pled_dtsi[i].mode);
		} else {
			LEDS_DEBUG("led dts can not get %s led mode\n",
				pled_dtsi[i].name);
			pled_dtsi[i].mode = 0;
		}

		ret =
		    of_property_read_u32(led_node, "data",
					 &data);
		if (!ret) {
			pled_dtsi[i].data = data;
			LEDS_DEBUG("The %s's led data is : %ld\n",
			     pled_dtsi[i].name, pled_dtsi[i].data);
		} else {
			LEDS_DEBUG("led dts can not get %s led data\n",
			    pled_dtsi[i].name);
			pled_dtsi[i].data = -1;
		}

		ret =
		    of_property_read_u32(led_node, "led_bits",
					 &led_bits);
		if (!ret) {
			pled_dtsi[i].led_bits = led_bits;
			LEDS_DEBUG("The %s's led led_bits is : %d\n",
			     pled_dtsi[i].name, pled_dtsi[i].led_bits);
		} else {
			pled_dtsi[i].led_bits = 8;
			pr_info("[LED]led dts can not get %s led led_bits\n",
			    pled_dtsi[i].name);
		}
		ret = of_property_read_u32_array(led_node, "pwm_config",
				pwm_config, ARRAY_SIZE(pwm_config));
		if (!ret) {
			LEDS_DEBUG("%s's pwm cfg %d %d %d %d %d\n",
			     pled_dtsi[i].name, pwm_config[0], pwm_config[1],
			     pwm_config[2], pwm_config[3], pwm_config[4]);
			pled_dtsi[i].config_data.clock_source = pwm_config[0];
			pled_dtsi[i].config_data.div = pwm_config[1];
			pled_dtsi[i].config_data.low_duration = pwm_config[2];
			pled_dtsi[i].config_data.High_duration = pwm_config[3];
			pled_dtsi[i].config_data.pmic_pad = pwm_config[4];
		} else
			LEDS_DEBUG("led dts can't get pwm config\n");

		switch (pled_dtsi[i].mode) {
		case MT65XX_LED_MODE_CUST_LCM:
#if defined(CONFIG_BACKLIGHT_SUPPORT_LM3697)
			pled_dtsi[i].data =
			   (long)chargepump_set_backlight_level;
			LEDS_DEBUG("BL set by chargepump\n");
#else
			pled_dtsi[i].data = (long)mtkfb_set_backlight_level;
#endif
			LEDS_DEBUG("kernel:the BL hw mode is LCM.\n");
			break;
		case MT65XX_LED_MODE_CUST_BLS_PWM:
			pled_dtsi[i].data = (long)disp_bls_set_backlight;
			LEDS_DEBUG("kernel:the BL hw mode is BLS.\n");
			break;
		default:
			break;
		}
	}

	if (!isSupportDTS) {
		kfree(pled_dtsi);
		pled_dtsi = NULL;
	}

 out:
	return pled_dtsi;
}

struct cust_mt65xx_led *mt_get_cust_led_list(void)
{
	struct cust_mt65xx_led *cust_led_list = get_cust_led_dtsi();
	return cust_led_list;
}

/****************************************************************************
 * internal functions
 ***************************************************************************/
#ifdef CONFIG_MTK_PWM
static int brightness_mapto64(int level)
{
	if (level < 30)
		return (level >> 1) + 7;
	else if (level <= 120)
		return (level >> 2) + 14;
	else if (level <= 160)
		return level / 5 + 20;
	else
		return (level >> 3) + 33;
}

static int find_time_index(int time)
{
	int index = 0;

	while (index < 8) {
		if (time < time_array_hal[index])
			return index;
		index++;
	}
	return PWM_DIV_NUM - 1;
}
#endif

int mt_led_set_pwm(int pwm_num, struct nled_setting *led)
{
#ifdef CONFIG_MTK_PWM
	struct pwm_spec_config pwm_setting;
	int time_index = 0;

	memset(&pwm_setting, 0, sizeof(struct pwm_spec_config));
	pwm_setting.pwm_no = pwm_num;
	pwm_setting.mode = PWM_MODE_OLD;

	LEDS_DEBUG("led_set_pwm: mode=%d,pwm_no=%d\n", led->nled_mode,
		   pwm_num);
	/* We won't choose 32K to be the clock src of old mode because of
	 * system performance. The setting here will be clock src = 26MHz,
	 * CLKSEL = 26M/1625 (i.e. 16K)
	 */
	pwm_setting.clk_src = PWM_CLK_OLD_MODE_32K;
	pwm_setting.pmic_pad = 0;

	switch (led->nled_mode) {
	/* Actually, the setting still can not to turn off NLED. We should
	 * disable PWM to turn off NLED.
	 */
	case NLED_OFF:
		pwm_setting.PWM_MODE_OLD_REGS.THRESH = 0;
		pwm_setting.clk_div = CLK_DIV1;
		pwm_setting.PWM_MODE_OLD_REGS.DATA_WIDTH = 100 / 2;
		break;

	case NLED_ON:
		pwm_setting.PWM_MODE_OLD_REGS.THRESH = 30 / 2;
		pwm_setting.clk_div = CLK_DIV1;
		pwm_setting.PWM_MODE_OLD_REGS.DATA_WIDTH = 100 / 2;
		break;

	case NLED_BLINK:
		LEDS_DEBUG("LED blink on time = %d offtime = %d\n",
			   led->blink_on_time, led->blink_off_time);
		time_index =
		    find_time_index(led->blink_on_time + led->blink_off_time);
		LEDS_DEBUG("LED div is %d\n", time_index);
		pwm_setting.clk_div = time_index;
		pwm_setting.PWM_MODE_OLD_REGS.DATA_WIDTH =
		    (led->blink_on_time +
		     led->blink_off_time) * MIN_FRE_OLD_PWM /
		    div_array_hal[time_index];
		pwm_setting.PWM_MODE_OLD_REGS.THRESH =
		    (led->blink_on_time * 100) / (led->blink_on_time +
						  led->blink_off_time);
		break;
	default:
		LEDS_DEBUG("Invalid nled mode\n");
		return -1;
	}

	pwm_setting.PWM_MODE_FIFO_REGS.IDLE_VALUE = 0;
	pwm_setting.PWM_MODE_FIFO_REGS.GUARD_VALUE = 0;
	pwm_setting.PWM_MODE_FIFO_REGS.GDURATION = 0;
	pwm_setting.PWM_MODE_FIFO_REGS.WAVE_NUM = 0;
	pwm_set_spec_config(&pwm_setting);
#endif
	return 0;
}

/************************ led breath function*****************************/
/*************************************************************************
 * func is to switch to breath mode from PWM mode of ISINK
 * para: enable: 1 : breath mode; 0: PWM mode;
 *************************************************************************/
#if 0
static int led_switch_breath_pmic(enum mt65xx_led_pmic pmic_type,
				  struct nled_setting *led, int enable)
{
	/* int time_index = 0; */
	/* int duty = 0; */
	LEDS_DEBUG("led_blink_pmic: pmic_type=%d\n", pmic_type);

	if ((pmic_type != MT65XX_LED_PMIC_NLED_ISINK0
	     && pmic_type != MT65XX_LED_PMIC_NLED_ISINK1
	     && pmic_type != MT65XX_LED_PMIC_NLED_ISINK2
	     && pmic_type != MT65XX_LED_PMIC_NLED_ISINK3)
	    || led->nled_mode != NLED_BLINK) {
		return -1;
	}
	if (enable == 1) {
		switch (pmic_type) {
		case MT65XX_LED_PMIC_NLED_ISINK0:
			pmic_set_register_value(PMIC_ISINK_CH0_MODE,
						ISINK_BREATH_MODE);
			pmic_set_register_value(PMIC_ISINK_CH0_STEP, ISINK_3);
			pmic_set_register_value(PMIC_ISINK_BREATH0_TR1_SEL,
						0x04);
			pmic_set_register_value(PMIC_ISINK_BREATH0_TR2_SEL,
						0x04);
			pmic_set_register_value(PMIC_ISINK_BREATH0_TF1_SEL,
						0x04);
			pmic_set_register_value(PMIC_ISINK_BREATH0_TF2_SEL,
						0x04);
			pmic_set_register_value(PMIC_ISINK_BREATH0_TON_SEL,
						0x02);
			pmic_set_register_value(PMIC_ISINK_BREATH0_TOFF_SEL,
						0x03);
			pmic_set_register_value(PMIC_ISINK_DIM0_DUTY, 15);
			pmic_set_register_value(PMIC_ISINK_DIM0_FSEL, 11);
			break;
		case MT65XX_LED_PMIC_NLED_ISINK1:
			pmic_set_register_value(PMIC_ISINK_CH1_MODE,
						ISINK_BREATH_MODE);
			pmic_set_register_value(PMIC_ISINK_CH1_STEP, ISINK_3);
			pmic_set_register_value(PMIC_ISINK_BREATH1_TR1_SEL,
						0x04);
			pmic_set_register_value(PMIC_ISINK_BREATH1_TR2_SEL,
						0x04);
			pmic_set_register_value(PMIC_ISINK_BREATH1_TF1_SEL,
						0x04);
			pmic_set_register_value(PMIC_ISINK_BREATH1_TF2_SEL,
						0x04);
			pmic_set_register_value(PMIC_ISINK_BREATH1_TON_SEL,
						0x02);
			pmic_set_register_value(PMIC_ISINK_BREATH1_TOFF_SEL,
						0x03);
			pmic_set_register_value(PMIC_ISINK_DIM1_DUTY, 15);
			pmic_set_register_value(PMIC_ISINK_DIM1_FSEL, 11);
			break;
		case MT65XX_LED_PMIC_NLED_ISINK2:
			pmic_set_register_value(PMIC_ISINK_CH2_MODE,
						ISINK_BREATH_MODE);
			pmic_set_register_value(PMIC_ISINK_CH2_STEP, ISINK_3);
			pmic_set_register_value(PMIC_ISINK_BREATH2_TR1_SEL,
						0x04);
			pmic_set_register_value(PMIC_ISINK_BREATH2_TR2_SEL,
						0x04);
			pmic_set_register_value(PMIC_ISINK_BREATH2_TF1_SEL,
						0x04);
			pmic_set_register_value(PMIC_ISINK_BREATH2_TF2_SEL,
						0x04);
			pmic_set_register_value(PMIC_ISINK_BREATH2_TON_SEL,
						0x02);
			pmic_set_register_value(PMIC_ISINK_BREATH2_TOFF_SEL,
						0x03);
			pmic_set_register_value(PMIC_ISINK_DIM2_DUTY, 15);
			pmic_set_register_value(PMIC_ISINK_DIM2_FSEL, 11);
			break;
		case MT65XX_LED_PMIC_NLED_ISINK3:
			pmic_set_register_value(PMIC_ISINK_CH3_MODE,
						ISINK_BREATH_MODE);
			pmic_set_register_value(PMIC_ISINK_CH3_STEP, ISINK_3);
			pmic_set_register_value(PMIC_ISINK_BREATH3_TR1_SEL,
						0x04);
			pmic_set_register_value(PMIC_ISINK_BREATH3_TR2_SEL,
						0x04);
			pmic_set_register_value(PMIC_ISINK_BREATH3_TF1_SEL,
						0x04);
			pmic_set_register_value(PMIC_ISINK_BREATH3_TF2_SEL,
						0x04);
			pmic_set_register_value(PMIC_ISINK_BREATH3_TON_SEL,
						0x02);
			pmic_set_register_value(PMIC_ISINK_BREATH3_TOFF_SEL,
						0x03);
			pmic_set_register_value(PMIC_ISINK_DIM3_DUTY, 15);
			pmic_set_register_value(PMIC_ISINK_DIM3_FSEL, 11);
			break;
		default:
			break;
		}
	} else {
		switch (pmic_type) {
		case MT65XX_LED_PMIC_NLED_ISINK0:
			pmic_set_register_value(PMIC_ISINK_CH3_MODE,
						ISINK_PWM_MODE);
			break;
		case MT65XX_LED_PMIC_NLED_ISINK0:
			pmic_set_register_value(PMIC_ISINK_CH3_MODE,
						ISINK_PWM_MODE);
			break;
		case MT65XX_LED_PMIC_NLED_ISINK0:
			pmic_set_register_value(PMIC_ISINK_CH3_MODE,
						ISINK_PWM_MODE);
			break;
		case MT65XX_LED_PMIC_NLED_ISINK0:
			pmic_set_register_value(PMIC_ISINK_CH3_MODE,
						ISINK_PWM_MODE);
			break;
		default:
			break;
		}
	}
	return 0;

}
#endif

#ifdef CONFIG_MTK_PMIC_CHIP_MT6357

#define PMIC_PERIOD_NUM 8
int pmic_period_array[] = { 2, 4, 6, 8, 10, 12, 20, 60 };
int pmic_freqsel_array[] = { 0, 1, 2, 3, 4, 5, 9, 28 };

static int find_time_index_pmic(int time_ms)
{
	int i;

	for (i = 0; i < PMIC_PERIOD_NUM; i++) {
		if (time_ms <= pmic_period_array[i])
			return i;
	}
	return PMIC_PERIOD_NUM - 1;
}
#endif

int mt_led_blink_pmic(enum mt65xx_led_pmic pmic_type, struct nled_setting *led)
{
#ifndef CONFIG_MTK_PMIC_CHIP_MT6357
	LEDS_DEBUG("%s pmic_type = %d isink does not support this path\n",
		__func__, pmic_type);
#else
	int time_index = 0;
	int duty = 0;

	if (led->nled_mode != NLED_BLINK)
		return -1;

	LEDS_DEBUG("%s blink on time = %d offtime = %d\n", __func__,
		   led->blink_on_time, led->blink_off_time);
	time_index =
	    find_time_index_pmic(led->blink_on_time + led->blink_off_time);
	LEDS_DEBUG("LED index is %d  freqsel=%d\n", time_index,
		   pmic_freqsel_array[time_index]);

	duty = 256 * led->blink_on_time / (led->blink_on_time +
				       led->blink_off_time);
	if (pmic_type > MT65XX_LED_PMIC_NLED_ISINK_MIN &&
		pmic_type < MT65XX_LED_PMIC_NLED_ISINK_MAX)
		pmic_set_register_value(PMIC_RG_DRV_128K_CK_PDN, 0x0);

	switch (pmic_type) {
	case MT65XX_LED_PMIC_NLED_ISINK0:
		pmic_set_register_value(PMIC_ISINK_CH0_EN, NLED_OFF);
		pmic_set_register_value(PMIC_RG_DRV_ISINK0_CK_PDN, 0);
		pmic_set_register_value(PMIC_ISINK_CH0_MODE, ISINK_PWM_MODE);
		pmic_set_register_value(PMIC_ISINK_CH0_STEP, ISINK_3);
		pmic_set_register_value(PMIC_ISINK_DIM0_DUTY, duty);
		pmic_set_register_value(PMIC_ISINK_DIM0_FSEL,
			pmic_freqsel_array[time_index]);
		pmic_set_register_value(PMIC_ISINK_CH0_BIAS_EN, NLED_ON);
		pmic_set_register_value(PMIC_ISINK_CHOP0_EN, NLED_ON);
		pmic_set_register_value(PMIC_ISINK_CH0_EN, NLED_ON);
		break;
	case MT65XX_LED_PMIC_NLED_ISINK1:
		pmic_set_register_value(PMIC_ISINK_CH1_EN, NLED_OFF);
		pmic_set_register_value(PMIC_RG_DRV_ISINK1_CK_PDN, 0);
		pmic_set_register_value(PMIC_ISINK_CH1_MODE, ISINK_PWM_MODE);
		pmic_set_register_value(PMIC_ISINK_CH1_STEP, ISINK_3);
		pmic_set_register_value(PMIC_ISINK_DIM1_DUTY, duty);
		pmic_set_register_value(PMIC_ISINK_DIM1_FSEL,
			pmic_freqsel_array[time_index]);
		pmic_set_register_value(PMIC_ISINK_CH1_BIAS_EN, NLED_ON);
		pmic_set_register_value(PMIC_ISINK_CHOP1_EN, NLED_ON);
		pmic_set_register_value(PMIC_ISINK_CH1_EN, NLED_ON);
		break;
	default:
		LEDS_DEBUG("pmic_type %d is not handled\n", pmic_type);
		break;
	}
#endif
	return 0;
}

int mt_backlight_set_pwm(int pwm_num, u32 level, u32 div,
			 struct PWM_config *config_data)
{
#ifdef CONFIG_MTK_PWM
	struct pwm_spec_config pwm_setting;
	unsigned int BacklightLevelSupport =
	    Cust_GetBacklightLevelSupport_byPWM();
	pwm_setting.pwm_no = pwm_num;

	if (BacklightLevelSupport == BACKLIGHT_LEVEL_PWM_256_SUPPORT)
		pwm_setting.mode = PWM_MODE_OLD;
	else
		pwm_setting.mode = PWM_MODE_FIFO;

	pwm_setting.pmic_pad = config_data->pmic_pad;

	if (config_data->div) {
		pwm_setting.clk_div = config_data->div;
		backlight_PWM_div_hal = config_data->div;
	} else
		pwm_setting.clk_div = div;

	if (BacklightLevelSupport == BACKLIGHT_LEVEL_PWM_256_SUPPORT) {
	/* PWM_CLK_OLD_MODE_32K is block/1625 = 26M/1625 = 16KHz @ MT6571 */
		if (config_data->clock_source)
			pwm_setting.clk_src = PWM_CLK_OLD_MODE_BLOCK;
		else
			pwm_setting.clk_src = PWM_CLK_OLD_MODE_32K;
		pwm_setting.PWM_MODE_OLD_REGS.IDLE_VALUE = 0;
		pwm_setting.PWM_MODE_OLD_REGS.GUARD_VALUE = 0;
		pwm_setting.PWM_MODE_OLD_REGS.GDURATION = 0;
		pwm_setting.PWM_MODE_OLD_REGS.WAVE_NUM = 0;
		pwm_setting.PWM_MODE_OLD_REGS.DATA_WIDTH = 255;	/* 256 level */
		pwm_setting.PWM_MODE_OLD_REGS.THRESH = level;

		LEDS_DEBUG("[%d]backlight set pwm:duty is %d/%d\n",
			   BacklightLevelSupport, level,
			   pwm_setting.PWM_MODE_OLD_REGS.DATA_WIDTH);
		LEDS_DEBUG("[%d]backlight set pwm:clk_src/div is %d%d\n",
			   BacklightLevelSupport, pwm_setting.clk_src,
			   pwm_setting.clk_div);
		if (level > 0 && level < 256) {
			pwm_set_spec_config(&pwm_setting);
			LEDS_DEBUG
			    ("[%d]old mode: thres/data_width is %d/%d\n",
			     BacklightLevelSupport,
			     pwm_setting.PWM_MODE_OLD_REGS.THRESH,
			     pwm_setting.PWM_MODE_OLD_REGS.DATA_WIDTH);
		} else {
			LEDS_DEBUG("[%d]Error level in backlight\n",
				   BacklightLevelSupport);
			mt_pwm_disable(pwm_setting.pwm_no,
				       config_data->pmic_pad);
		}
	} else {
		if (config_data->clock_source) {
			pwm_setting.clk_src = PWM_CLK_NEW_MODE_BLOCK;
		} else {
			pwm_setting.clk_src =
			    PWM_CLK_NEW_MODE_BLOCK_DIV_BY_1625;
		}

		if (config_data->High_duration && config_data->low_duration) {
			pwm_setting.PWM_MODE_FIFO_REGS.HDURATION =
			    config_data->High_duration;
			pwm_setting.PWM_MODE_FIFO_REGS.LDURATION =
			    pwm_setting.PWM_MODE_FIFO_REGS.HDURATION;
		} else {
			pwm_setting.PWM_MODE_FIFO_REGS.HDURATION = 4;
			pwm_setting.PWM_MODE_FIFO_REGS.LDURATION = 4;
		}

		pwm_setting.PWM_MODE_FIFO_REGS.IDLE_VALUE = 0;
		pwm_setting.PWM_MODE_FIFO_REGS.GUARD_VALUE = 0;
		pwm_setting.PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE = 31;
		pwm_setting.PWM_MODE_FIFO_REGS.GDURATION =
		    (pwm_setting.PWM_MODE_FIFO_REGS.HDURATION + 1) * 32 - 1;
		pwm_setting.PWM_MODE_FIFO_REGS.WAVE_NUM = 0;

		LEDS_DEBUG("%s :duty is %d\n", __func__, level);
		LEDS_DEBUG("clk_src/div/high/low is %d%d%d%d\n",
		     pwm_setting.clk_src, pwm_setting.clk_div,
		     pwm_setting.PWM_MODE_FIFO_REGS.HDURATION,
		     pwm_setting.PWM_MODE_FIFO_REGS.LDURATION);

		if (level > 0 && level <= 32) {
			pwm_setting.PWM_MODE_FIFO_REGS.GUARD_VALUE = 0;
			pwm_setting.PWM_MODE_FIFO_REGS.SEND_DATA0 =
			    (1 << level) - 1;
			pwm_set_spec_config(&pwm_setting);
		} else if (level > 32 && level <= 64) {
			pwm_setting.PWM_MODE_FIFO_REGS.GUARD_VALUE = 1;
			level -= 32;
			pwm_setting.PWM_MODE_FIFO_REGS.SEND_DATA0 =
			    (1 << level) - 1;
			pwm_set_spec_config(&pwm_setting);
		} else {
			LEDS_DEBUG("Error level in backlight\n");
			mt_pwm_disable(pwm_setting.pwm_no,
				       config_data->pmic_pad);
		}
	}
#endif
	return 0;
}

void mt_led_pwm_disable(int pwm_num)
{
#ifdef CONFIG_MTK_PWM
	struct cust_mt65xx_led *cust_led_list = get_cust_led_dtsi();

	mt_pwm_disable(pwm_num, cust_led_list->config_data.pmic_pad);
#endif
}

int mt_brightness_set_pmic(enum mt65xx_led_pmic pmic_type, u32 level, u32 div)
{
#ifndef CONFIG_MTK_PMIC_CHIP_MT6357
	LEDS_DEBUG("%s, pmic_type = %d isink does not support this path\n",
		__func__, pmic_type);
#else
	static bool first_time = true;

	LEDS_DEBUG("%s pmic_type%d level%d\n", __func__, pmic_type, level);
	mutex_lock(&leds_pmic_mutex);
	if (pmic_type == MT65XX_LED_PMIC_NLED_ISINK0) {
		/* button flag ==0, means this ISINK is not for button */
		if ((button_flag_isink0 == 0) && (first_time == true)) {
			/* sw workround for sync leds status */
			if (button_flag_isink1 == 0)
				pmic_set_register_value(PMIC_ISINK_CH1_EN,
					NLED_OFF);
			first_time = false;
		}
		pmic_set_register_value(PMIC_RG_DRV_128K_CK_PDN, 0x0);
		pmic_set_register_value(PMIC_RG_DRV_ISINK0_CK_PDN, 0);
		pmic_set_register_value(PMIC_ISINK_CH0_MODE, ISINK_PWM_MODE);
		/* 16mA */
		pmic_set_register_value(PMIC_ISINK_CH0_STEP, ISINK_3);
		pmic_set_register_value(PMIC_ISINK_DIM0_DUTY, 255);
		/* 1KHz */
		pmic_set_register_value(PMIC_ISINK_DIM0_FSEL,
			ISINK_128K_500HZ);

		pmic_set_register_value(PMIC_ISINK_CH0_BIAS_EN, NLED_ON);
		pmic_set_register_value(PMIC_ISINK_CHOP0_EN, NLED_ON);

		if (level)
			pmic_set_register_value(PMIC_ISINK_CH0_EN, NLED_ON);
		else
			pmic_set_register_value(PMIC_ISINK_CH0_EN, NLED_OFF);
		mutex_unlock(&leds_pmic_mutex);
		return 0;
	} else if (pmic_type == MT65XX_LED_PMIC_NLED_ISINK1) {
		/* button flag ==0, means this ISINK is not for button*/
		if ((button_flag_isink1 == 0) && (first_time == true)) {
			/* sw workround for sync leds status */
			if (button_flag_isink0 == 0)
				pmic_set_register_value(PMIC_ISINK_CH0_EN,
					NLED_OFF);
			first_time = false;
		}
		/* Disable power down */
		pmic_set_register_value(PMIC_RG_DRV_128K_CK_PDN, 0x0);
		pmic_set_register_value(PMIC_RG_DRV_ISINK1_CK_PDN, 0);
		pmic_set_register_value(PMIC_ISINK_CH1_MODE, ISINK_PWM_MODE);
		/* 16mA */
		pmic_set_register_value(PMIC_ISINK_CH1_STEP, ISINK_3);
		pmic_set_register_value(PMIC_ISINK_DIM1_DUTY, 255);
		/* 1KHz */
		pmic_set_register_value(PMIC_ISINK_DIM1_FSEL, ISINK_128K_500HZ);

		pmic_set_register_value(PMIC_ISINK_CH1_BIAS_EN, NLED_ON);
		pmic_set_register_value(PMIC_ISINK_CHOP1_EN, NLED_ON);
		if (level)
			pmic_set_register_value(PMIC_ISINK_CH1_EN, NLED_ON);
		else
			pmic_set_register_value(PMIC_ISINK_CH1_EN, NLED_OFF);
		mutex_unlock(&leds_pmic_mutex);
		return 0;
	}
	mutex_unlock(&leds_pmic_mutex);
#endif
	return -1;
}

int mt_mt65xx_led_set_cust(struct cust_mt65xx_led *cust, int level)
{
#ifdef CONFIG_MTK_PWM
	struct nled_setting led_tmp_setting = { 0, 0, 0 };
	int tmp_level = level;
	unsigned int BacklightLevelSupport =
	    Cust_GetBacklightLevelSupport_byPWM();
#endif
	static bool button_flag;

	switch (cust->mode) {

	case MT65XX_LED_MODE_PWM:
#ifdef CONFIG_MTK_PWM
		if (strcmp(cust->name, "lcd-backlight") == 0) {
			bl_brightness_hal = level;
			if (level == 0) {
				mt_pwm_disable(cust->data,
					cust->config_data.pmic_pad);

			} else {
				if (BacklightLevelSupport ==
				    BACKLIGHT_LEVEL_PWM_256_SUPPORT)
					level = brightness_mapping(tmp_level);
				else
					level = brightness_mapto64(tmp_level);

				mt_backlight_set_pwm(cust->data, level,
					bl_div_hal, &cust->config_data);
			}
			bl_duty_hal = level;
		} else {
			if (level == 0) {
				led_tmp_setting.nled_mode = NLED_OFF;
				mt_led_set_pwm(cust->data, &led_tmp_setting);
				mt_pwm_disable(cust->data,
					       cust->config_data.pmic_pad);
			} else {
				led_tmp_setting.nled_mode = NLED_ON;
				mt_led_set_pwm(cust->data, &led_tmp_setting);
			}
		}
#endif
		return 1;

	case MT65XX_LED_MODE_GPIO:
		LEDS_DEBUG("%s led GPIO mode!!\n", __func__);
		return ((cust_set_brightness) (cust->data)) (level);

	case MT65XX_LED_MODE_PMIC:
		/* for button baclight used SINK channel, when set button ISINK,
		 * don't do disable other ISINK channel
		 */
		if ((strcmp(cust->name, "button-backlight") == 0)) {
			if (button_flag == false) {
				switch (cust->data) {
				case MT65XX_LED_PMIC_NLED_ISINK0:
					button_flag_isink0 = 1;
					break;
				case MT65XX_LED_PMIC_NLED_ISINK1:
					button_flag_isink1 = 1;
					break;
				default:
					break;
				}
				button_flag = true;
			}
		}
		return mt_brightness_set_pmic(cust->data, level, bl_div_hal);

	case MT65XX_LED_MODE_CUST_LCM:
		if (strcmp(cust->name, "lcd-backlight") == 0)
			bl_brightness_hal = level;
		LEDS_DEBUG("%s backlight control by LCM\n", __func__);
		/* warning for this API revork */
		return ((cust_brightness_set) (cust->data)) (level, bl_div_hal);

	case MT65XX_LED_MODE_CUST_BLS_PWM:
		if (strcmp(cust->name, "lcd-backlight") == 0)
			bl_brightness_hal = level;
#ifdef MET_USER_EVENT_SUPPORT
		if (enable_met_backlight_tag())
			output_met_backlight_tag(level);
#endif
		pr_info("[LED] disp_bls_set_backlight: %d", level);
		return ((cust_set_brightness) (cust->data)) (level);

	case MT65XX_LED_MODE_NONE:
	default:
		break;
	}
	return -1;
}

void mt_mt65xx_led_work(struct work_struct *work)
{
	struct mt65xx_led_data *led_data =
	    container_of(work, struct mt65xx_led_data, work);

	LEDS_DEBUG("%s:%d work start\n", led_data->cust.name, led_data->level);
	mutex_lock(&leds_mutex);
	mt_mt65xx_led_set_cust(&led_data->cust, led_data->level);
	mutex_unlock(&leds_mutex);
}

void mt_mt65xx_led_set(struct led_classdev *led_cdev, enum led_brightness level)
{

	int trans_level;
	struct mt65xx_led_data *led_data =
	    container_of(led_cdev, struct mt65xx_led_data, cdev);
	/* unsigned long flags; */
	/* spin_lock_irqsave(&leds_lock, flags); */

	/* do something only when level is changed */
	if (led_data->level == level) {
		LEDS_DEBUG("no level change,do nothing\n");
		return;
	}

	led_data->level = level;
	if (strcmp(led_data->cust.name, "lcd-backlight") != 0) {
		LEDS_DEBUG("Set NLED directly %d at time %lu\n",
			   led_data->level, jiffies);
		schedule_work(&led_data->work);
		return;
	}
	if (level != 0 && level * CONFIG_LIGHTNESS_MAPPING_VALUE < 255)
		level = 1;
	else
		level = (level * CONFIG_LIGHTNESS_MAPPING_VALUE) / 255;

	backlight_debug_log(led_data->level, level);
	trans_level = ((((1 << MT_LED_INTERNAL_LEVEL_BIT_CNT)
				- 1) * level +
				(((1 << led_data->cust.led_bits) - 1) / 2))
				/ ((1 << led_data->cust.led_bits) - 1));

	pr_info("[LED] disp_pq_notify_backlight_changed: %d-%d(%d)",
			level, trans_level, led_data->cust.led_bits);
	disp_pq_notify_backlight_changed(trans_level);
#ifdef CONFIG_MTK_AAL_SUPPORT
	disp_aal_notify_backlight_changed(trans_level);
#else
	if (led_data->cust.mode == MT65XX_LED_MODE_CUST_BLS_PWM)
		mt_mt65xx_led_set_cust(&led_data->cust, trans_level);
	else
		mt_mt65xx_led_set_cust(&led_data->cust, level);
#endif
}

/* spin_unlock_irqrestore(&leds_lock, flags); */
/* if(0!=aee_kernel_Powerkey_is_press()) */
/* aee_kernel_wdt_kick_Powkey_api("mt_mt65xx_led_set",WDT_SETBY_Backlight); */

int mt_mt65xx_blink_set(struct led_classdev *led_cdev,
			unsigned long *delay_on, unsigned long *delay_off)
{
	struct mt65xx_led_data *led_data =
	    container_of(led_cdev, struct mt65xx_led_data, cdev);
	static int got_wake_lock;
	struct nled_setting nled_tmp_setting = { 0, 0, 0 };

	if ((*delay_on == led_data->delay_on)
		&& (*delay_off == led_data->delay_off)) {
		LEDS_DEBUG("blink para not changed,do nothing %ld %ld\n",
			*delay_on, *delay_off);
		return -1;
	}
	/* only allow software blink when delay_on or delay_off changed */
	led_data->delay_on = *delay_on;
	led_data->delay_off = *delay_off;
	if (led_data->delay_on && led_data->delay_off) {
		led_data->level = 255;
		/* AP PWM all support OLD mode */
		if (led_data->cust.mode == MT65XX_LED_MODE_PWM) {
			nled_tmp_setting.nled_mode = NLED_BLINK;
			nled_tmp_setting.blink_off_time =
			    led_data->delay_off;
			nled_tmp_setting.blink_on_time =
			    led_data->delay_on;
			mt_led_set_pwm(led_data->cust.data,
				       &nled_tmp_setting);
		} else if (led_data->cust.mode == MT65XX_LED_MODE_PMIC) {
			nled_tmp_setting.nled_mode = NLED_BLINK;
			nled_tmp_setting.blink_off_time =
			    led_data->delay_off;
			nled_tmp_setting.blink_on_time =
			    led_data->delay_on;
			mt_led_blink_pmic(led_data->cust.data,
					  &nled_tmp_setting);
		} else if (!got_wake_lock) {
			__pm_stay_awake(&leds_suspend_lock);
			got_wake_lock = 1;
		}
	} else if (!led_data->delay_on && led_data->delay_off) {
		/* AP PWM all support OLD mode */
		if (led_data->cust.mode == MT65XX_LED_MODE_PWM) {
			nled_tmp_setting.nled_mode = NLED_OFF;
			mt_led_set_pwm(led_data->cust.data,
				       &nled_tmp_setting);
		} else if (led_data->cust.mode == MT65XX_LED_MODE_PMIC)
			mt_brightness_set_pmic(led_data->cust.data, 0, 0);
		else if (got_wake_lock) {
			__pm_relax(&leds_suspend_lock);
			got_wake_lock = 0;
		}
	} else if (led_data->delay_on && !led_data->delay_off) {
		/* AP PWM all support OLD mode */
		if (led_data->cust.mode == MT65XX_LED_MODE_PWM) {
			nled_tmp_setting.nled_mode = NLED_ON;
			mt_led_set_pwm(led_data->cust.data, &nled_tmp_setting);
		} else if (led_data->cust.mode == MT65XX_LED_MODE_PMIC)
			mt_brightness_set_pmic(led_data->cust.data, 1, 0);
		else if (got_wake_lock) {
			__pm_relax(&leds_suspend_lock);
			got_wake_lock = 0;
		}
	} else
		return -1;

	return 0;
}
