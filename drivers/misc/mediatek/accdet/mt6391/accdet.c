/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "accdet.h"
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/timer.h>
#include <upmu_common.h>

#define DEBUG_THREAD 1
#define ACCDET_MULTI_KEY_FEATURE
#define SW_WORK_AROUND_ACCDET_REMOTE_BUTTON_ISSUE
/*----------------------------------------------------------------------
 * static variable defination
 *----------------------------------------------------------------------
 */

#define REGISTER_VALUE(x) (x - 1)
static int button_press_debounce = 0x400;
int cur_key;
struct head_dts_data accdet_dts_data;
s8 accdet_auxadc_offset;
int accdet_irq;
unsigned int headsetdebounce;
unsigned int accdet_eint_type;
struct headset_mode_settings *cust_headset_settings;
#define ACCDET_DEBUG(format, args...) pr_debug(format, ##args)
#define ACCDET_INFO(format, args...) pr_debug(format, ##args)
#define ACCDET_ERROR(format, args...) pr_debug(format, ##args)
static void send_accdet_status_event(int cable_type, int status);
static struct input_dev *kpd_accdet_dev;
static struct cdev *accdet_cdev;
static struct class *accdet_class;
static struct device *accdet_nor_device;
static dev_t accdet_devno;
static int pre_status;
static int pre_state_swctrl;
static int accdet_status = PLUG_OUT;
static int cable_type;
static int eint_accdet_sync_flag;
static int g_accdet_first = 1;
static bool IRQ_CLR_FLAG;
static int call_status;
static int button_status;
static struct wakeup_source *accdet_suspend_lock;
static struct wakeup_source *accdet_irq_lock;
static struct wakeup_source *accdet_key_lock;
static struct wakeup_source *accdet_timer_lock;
static struct work_struct accdet_work;
static struct workqueue_struct *accdet_workqueue;
static DEFINE_MUTEX(accdet_eint_irq_sync_mutex);
static inline void clear_accdet_interrupt(void);
static inline void clear_accdet_eint_interrupt(void);
static void send_key_event(int keycode, int flag);
#define KEYDOWN_FLAG 1
#define KEYUP_FLAG 0
static int long_press_time = 2000;

#if defined CONFIG_ACCDET_EINT || defined CONFIG_ACCDET_EINT_IRQ
static struct work_struct accdet_eint_work;
static struct workqueue_struct *accdet_eint_workqueue;
static inline void accdet_init(void);
#define MICBIAS_DISABLE_TIMER (3 * HZ) /*3 seconds*/
struct timer_list micbias_timer;
static void disable_micbias(unsigned long a);
/* Used to let accdet know if the pin has been fully plugged-in */
#define EINT_PIN_PLUG_IN (1)
#define EINT_PIN_PLUG_OUT (0)
int cur_eint_state = EINT_PIN_PLUG_OUT;
static struct work_struct accdet_disable_work;
static struct workqueue_struct *accdet_disable_workqueue;
#endif /*end CONFIG_ACCDET_EINT*/

static u32 pmic_pwrap_read(u32 addr);
static u32 pmic_pwrap_write(u32 addr, unsigned int wdata);

#ifdef FOUR_KEY_HEADSET
static int ADC_GE;
static int ADC_OE;
#endif

char *accdet_status_string[5] = {"Plug_out", "Headset_plug_in",
				 /*"Double_check",*/
				 "Hook_switch",
				 /*"Tvout_plug_in",*/
				 "Stand_by"};
char *accdet_report_string[4] = {
	"No_device", "Headset_mic", "Headset_no_mic",
	/*"HEADSET_illegal",*/
	/* "Double_check"*/
};
/****************************************************************/
/***        export function **/
/****************************************************************/

void accdet_detect(void)
{
	int ret = 0;

	ACCDET_DEBUG("[Accdet]accdet_detect\n");

	accdet_status = PLUG_OUT;
	ret = queue_work(accdet_workqueue, &accdet_work);
	if (!ret)
		ACCDET_DEBUG("[Accdet]accdet_detect:accdet_work return:%d!\n",
			     ret);
}
EXPORT_SYMBOL(accdet_detect);

void accdet_state_reset(void)
{

	ACCDET_DEBUG("[Accdet]accdet_state_reset\n");

	accdet_status = PLUG_OUT;
	cable_type = NO_DEVICE;
}
EXPORT_SYMBOL(accdet_state_reset);

int accdet_get_cable_type(void)
{
	return cable_type;
}

void accdet_auxadc_switch(int enable)
{
	if (enable) {
#ifndef ACCDET_28V_MODE
		pmic_pwrap_write(ACCDET_RSV, ACCDET_1V9_MODE_ON);
#else
		pmic_pwrap_write(0x732, ACCDET_2V8_MODE_ON);
#endif
	} else {
#ifndef ACCDET_28V_MODE
		pmic_pwrap_write(ACCDET_RSV, ACCDET_1V9_MODE_OFF);
#else
		pmic_pwrap_write(0x732, 0);
#endif
	}
}

static u32 pmic_pwrap_read(u32 RegNum)
{
	unsigned int return_value = 0;
	unsigned int pmic_reg = 0;

	return_value = regmap_read(pwrap_regmap, RegNum, &pmic_reg);
	if (return_value != 0) {
		pr_notice(
			"[Power/PMIC][pmic_read_interface] Reg[%x]= pmic_wrap read data fail\n",
			RegNum);
		return return_value;
	}

	return pmic_reg;
}

static u32 pmic_pwrap_write(u32 RegNum, u32 val)
{
	unsigned int return_value = 0;
	unsigned int pmic_reg = 0;

	return_value = regmap_read(pwrap_regmap, RegNum, &pmic_reg);
	if (return_value != 0) {
		pr_notice(
			"[Power/PMIC][pmic_config_interface] Reg[%x]= pmic_wrap read data fail\n",
			RegNum);
		return return_value;
	}

	pmic_reg &= ~(0xFFFF << 0x0);
	pmic_reg |= (val << 0x0);

	return_value = regmap_write(pwrap_regmap, RegNum, pmic_reg);
	if (return_value != 0) {
		pr_notice(
			"[Power/PMIC][pmic_config_interface] Reg[%x]= pmic_wrap read data fail\n",
			RegNum);
		return return_value;
	}

	return return_value;
}

static inline void headset_plug_out(void)
{
	send_accdet_status_event(cable_type, 0);

	accdet_status = PLUG_OUT;
	cable_type = NO_DEVICE;
	/*update the cable_type*/
	if (cur_key != 0) {
		send_key_event(cur_key, 0);
		ACCDET_DEBUG(
			" [accdet] headset_plug_out send key = %d release\n",
			cur_key);
		cur_key = 0;
	}
	ACCDET_DEBUG(" [accdet] set state in cable_type = NO_DEVICE\n");
}

/*Accdet only need this func*/
static inline void enable_accdet(u32 state_swctrl)
{
	/*enable ACCDET unit*/
	ACCDET_DEBUG("accdet: enable_accdet\n");
	/*enable clock*/
	pmic_pwrap_write(TOP_CKPDN_CLR, RG_ACCDET_CLK_CLR);
	pmic_pwrap_write(ACCDET_STATE_SWCTRL,
			 pmic_pwrap_read(ACCDET_STATE_SWCTRL) | state_swctrl);
	pmic_pwrap_write(ACCDET_CTRL,
			 pmic_pwrap_read(ACCDET_CTRL) | ACCDET_ENABLE);
}

static inline void disable_accdet(void)
{
	int irq_temp = 0;

	/* sync with accdet_irq_handler set clear accdet irq bit to avoid  set
	 * clear accdet irq bit after disable accdet
	 * disable accdet irq
	 */
	pmic_pwrap_write(INT_CON_ACCDET_CLR, RG_ACCDET_IRQ_CLR);
	clear_accdet_interrupt();
	udelay(200);
	mutex_lock(&accdet_eint_irq_sync_mutex);
	while (pmic_pwrap_read(ACCDET_IRQ_STS) & IRQ_STATUS_BIT) {
		ACCDET_DEBUG(
			"[Accdet]check_cable_type: Clear interrupt on-going....\n");
		msleep(20);
	}
	irq_temp = pmic_pwrap_read(ACCDET_IRQ_STS);
	irq_temp = irq_temp & (~IRQ_CLR_BIT);
	pmic_pwrap_write(ACCDET_IRQ_STS, irq_temp);
	mutex_unlock(&accdet_eint_irq_sync_mutex);
	/*disable ACCDET unit*/
	ACCDET_DEBUG("accdet: disable_accdet\n");
	pre_state_swctrl = pmic_pwrap_read(ACCDET_STATE_SWCTRL);
#ifdef CONFIG_ACCDET_EINT
	pmic_pwrap_write(ACCDET_STATE_SWCTRL, 0);
	pmic_pwrap_write(ACCDET_CTRL, ACCDET_DISABLE);
	/*disable clock and Analog control*/
	/*mt6331_upmu_set_rg_audmicbias1vref(0x0);*/
	pmic_pwrap_write(TOP_CKPDN_SET, RG_ACCDET_CLK_SET);
#endif
#ifdef CONFIG_ACCDET_EINT_IRQ
	pmic_pwrap_write(ACCDET_STATE_SWCTRL, ACCDET_EINT_PWM_EN);
	pmic_pwrap_write(ACCDET_CTRL,
			 pmic_pwrap_read(ACCDET_CTRL) & (~(ACCDET_ENABLE)));
/*mt_set_gpio_out(GPIO_CAMERA_2_CMRST_PIN, GPIO_OUT_ZERO);*/
#endif
}

#if defined CONFIG_ACCDET_EINT || defined CONFIG_ACCDET_EINT_IRQ
static void disable_micbias(unsigned long a)
{
	int ret = 0;

	ret = queue_work(accdet_disable_workqueue, &accdet_disable_work);
	if (!ret)
		ACCDET_DEBUG("[Accdet]disable_micbias:accdet_work return:%d!\n",
			     ret);
}

static void disable_micbias_callback(struct work_struct *work)
{
	if (cable_type == HEADSET_NO_MIC || cable_type == HEADSET_MIC) {
		/*setting pwm idle;*/
		pmic_pwrap_write(ACCDET_STATE_SWCTRL,
				 pmic_pwrap_read(ACCDET_STATE_SWCTRL) &
					 ~ACCDET_SWCTRL_IDLE_EN);
		disable_accdet();
		ACCDET_DEBUG("[Accdet] more than 5s MICBIAS : Disabled\n");
	}
}

static void accdet_eint_work_callback(struct work_struct *work)
{
#ifdef CONFIG_ACCDET_EINT_IRQ
	int irq_temp = 0;

	if (cur_eint_state == EINT_PIN_PLUG_IN) {
		ACCDET_DEBUG(
			"[Accdet]DCC EINT func :plug-in, cur_eint_state = %d\n",
			cur_eint_state);
		mutex_lock(&accdet_eint_irq_sync_mutex);
		eint_accdet_sync_flag = 1;
		mutex_unlock(&accdet_eint_irq_sync_mutex);
		mod_timer(&micbias_timer, jiffies + MICBIAS_DISABLE_TIMER);
		__pm_wakeup_event(accdet_timer_lock, 7 * 1000);

		accdet_init(); /*do set pwm_idle on in accdet_init*/
		/*set PWM IDLE  on*/
		pmic_pwrap_write(ACCDET_STATE_SWCTRL,
				 (pmic_pwrap_read(ACCDET_STATE_SWCTRL) |
				  ACCDET_SWCTRL_IDLE_EN));
		/*enable ACCDET unit*/
		enable_accdet(ACCDET_SWCTRL_EN);
	} else {
		/*EINT_PIN_PLUG_OUT*/
		/*Disable ACCDET*/
		ACCDET_DEBUG(
			"[Accdet]DCC EINT func :plug-out, cur_eint_state = %d\n",
			cur_eint_state);
		mutex_lock(&accdet_eint_irq_sync_mutex);
		eint_accdet_sync_flag = 0;
		mutex_unlock(&accdet_eint_irq_sync_mutex);
		del_timer_sync(&micbias_timer);
		/*accdet_auxadc_switch(0);*/
		disable_accdet();
		headset_plug_out();
		/*recover EINT irq clear bit */
		/*TODO: need think~~~*/
		irq_temp = pmic_pwrap_read(ACCDET_IRQ_STS);
		irq_temp = irq_temp & (~IRQ_EINT_CLR_BIT);
		pmic_pwrap_write(ACCDET_IRQ_STS, irq_temp);
	}
#else
	/*KE under fastly plug in and plug out*/
	if (cur_eint_state == EINT_PIN_PLUG_IN) {
		ACCDET_DEBUG(
			"[Accdet]ACC EINT func :plug-in, cur_eint_state = %d\n",
			cur_eint_state);
		mutex_lock(&accdet_eint_irq_sync_mutex);
		eint_accdet_sync_flag = 1;
		mutex_unlock(&accdet_eint_irq_sync_mutex);
		__pm_wakeup_event(accdet_timer_lock, 7 * 1000);

		accdet_init(); /* do set pwm_idle on in accdet_init*/

		/*set PWM IDLE  on*/
		pmic_pwrap_write(ACCDET_STATE_SWCTRL,
				 (pmic_pwrap_read(ACCDET_STATE_SWCTRL) |
				  ACCDET_SWCTRL_IDLE_EN));
		/*enable ACCDET unit*/
		enable_accdet(ACCDET_SWCTRL_EN);
	} else {
		/*EINT_PIN_PLUG_OUT*/
		/*Disable ACCDET*/
		ACCDET_DEBUG(
			"[Accdet]DCC EINT func :plug-out, cur_eint_state = %d\n",
			cur_eint_state);
		mutex_lock(&accdet_eint_irq_sync_mutex);
		eint_accdet_sync_flag = 0;
		mutex_unlock(&accdet_eint_irq_sync_mutex);
		del_timer_sync(&micbias_timer);
		/*accdet_auxadc_switch(0);*/
		disable_accdet();
		headset_plug_out();
	}
	enable_irq(accdet_irq);
#endif
}

static irqreturn_t accdet_eint_func(int irq, void *data)
{
	ACCDET_ERROR("[Accdet]>>>>>>>>>>>>Enter accdet_eint_func.\n");
	ACCDET_ERROR("[Accdet]cur_eint_state = %d\n", cur_eint_state);

	cur_eint_state = !cur_eint_state;

	if (accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
		accdet_eint_type = IRQ_TYPE_LEVEL_LOW;
	else
		accdet_eint_type = IRQ_TYPE_LEVEL_HIGH;

	irq_set_irq_type(accdet_irq, accdet_eint_type);

	disable_irq_nosync(accdet_irq);

	queue_work(accdet_eint_workqueue, &accdet_eint_work);

	return IRQ_HANDLED;
}

#ifndef CONFIG_ACCDET_EINT_IRQ
static inline int accdet_setup_eint(struct platform_device *accdet_device)
{
	int ret;
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-accdet");

	accdet_eint_type = IRQ_TYPE_LEVEL_HIGH;

	if (node) {
		accdet_irq = irq_of_parse_and_map(node, 0);
		ACCDET_DEBUG("[accdet]accdet_irq=%d", accdet_irq);
		ret = request_irq(accdet_irq, accdet_eint_func,
				  IRQ_TYPE_LEVEL_HIGH, "ACCDET-eint", NULL);
		if (ret > 0)
			ACCDET_ERROR("[Accdet]EINT IRQ LINE NOT AVAILABLE\n");
		else
			ACCDET_ERROR(
				"[Accdet]accdet set EINT finished, accdet_irq=%d, headsetdebounce=%d\n",
				accdet_irq, headsetdebounce);
	} else {
		ACCDET_ERROR("[Accdet]%s can't find compatible node\n",
			     __func__);
	}

	return 0;
}
#endif /*CONFIG_ACCDET_EINT_IRQ*/
#endif /*endif CONFIG_ACCDET_EINT*/

#ifdef ACCDET_MULTI_KEY_FEATURE
#define KEY_SAMPLE_PERIOD 60 /* ms */
#define MULTIKEY_ADC_CHANNEL 5

#define NO_KEY (0x0)
#define UP_KEY (0x01)
#define MD_KEY (0x02)
#define DW_KEY (0x04)

#ifdef FOUR_KEY_HEADSET
#define AS_KEY (0x08)
#endif
#define SHORT_PRESS (0x0)
#define LONG_PRESS (0x10)
#define SHORT_UP ((UP_KEY) | SHORT_PRESS)
#define SHORT_MD ((MD_KEY) | SHORT_PRESS)
#define SHORT_DW ((DW_KEY) | SHORT_PRESS)
#define LONG_UP ((UP_KEY) | LONG_PRESS)
#define LONG_MD ((MD_KEY) | LONG_PRESS)
#define LONG_DW ((DW_KEY) | LONG_PRESS)

#define KEYDOWN_FLAG 1
#define KEYUP_FLAG 0
/* static int g_adcMic_channel_num =0; */

static DEFINE_MUTEX(accdet_multikey_mutex);

/*
 *
 *	   MD         UP         DW
 * |---------|-----------|----------|
 * 0V<=MD< 0.09V<= UP<0.24V<=DW <0.5V
 *
 */

#define DW_KEY_HIGH_THR (500) /* 0.50v=500000uv */
#define DW_KEY_THR (240)      /* 0.24v=240000uv */
#define UP_KEY_THR (90)       /* 0.09v=90000uv */
#define MD_KEY_THR (0)

#ifdef FOUR_KEY_HEADSET
static int key_check(int b)
{
	/* 0.24V ~ */
	/*ACCDET_DEBUG("[accdet] come in key_check!!\n");*/
	if ((b < accdet_dts_data.four_key.down_key_four) &&
	    (b >= accdet_dts_data.four_key.up_key_four))
		return DW_KEY;
	else if ((b < accdet_dts_data.four_key.up_key_four) &&
		 (b >= accdet_dts_data.four_key.voice_key_four))
		return UP_KEY;
	else if ((b < accdet_dts_data.four_key.voice_key_four) &&
		 (b >= accdet_dts_data.four_key.mid_key_four))
		return AS_KEY;
	else if ((b < accdet_dts_data.four_key.mid_key_four) && (b >= 0))
		return MD_KEY;
	ACCDET_DEBUG("[accdet] leave key_check!!\n");
	return NO_KEY;
}
#else
static int key_check(int b)
{
	if ((b < DW_KEY_HIGH_THR) && (b >= DW_KEY_THR)) {
		ACCDET_DEBUG("[Accdet]DW_KEY->adc_data: %d mv\n", b);
		return DW_KEY;
	} else if ((b < DW_KEY_THR) && (b >= UP_KEY_THR)) {
		ACCDET_DEBUG("[Accdet]UP_KEY->adc_data: %d mv\n", b);
		return UP_KEY;
	} else if ((b < UP_KEY_THR) && (b >= MD_KEY_THR)) {
		ACCDET_DEBUG("[Accdet]MD_KEY->adc_data: %d mv\n", b);
		return MD_KEY;
	}
	ACCDET_DEBUG("[Accdet]NO_KEY->adc_data: %d mv\n", b);
	return NO_KEY;
}

#endif

static void send_accdet_status_event(int cable_type, int status)
{
	switch (cable_type) {
	case HEADSET_NO_MIC:
		input_report_switch(kpd_accdet_dev, SW_HEADPHONE_INSERT,
				    status);
		input_report_switch(kpd_accdet_dev, SW_JACK_PHYSICAL_INSERT,
				    status);
		input_sync(kpd_accdet_dev);
		ACCDET_DEBUG("[Accdet]HEADSET_NO_MIC(3-pole) %s\n",
			     status ? "PlugIn" : "PlugOut");
		break;
	case HEADSET_MIC:
		input_report_switch(kpd_accdet_dev, SW_HEADPHONE_INSERT,
				    status);
		input_report_switch(kpd_accdet_dev, SW_MICROPHONE_INSERT,
				    status);
		input_report_switch(kpd_accdet_dev, SW_JACK_PHYSICAL_INSERT,
				    status);
		input_sync(kpd_accdet_dev);
		ACCDET_DEBUG("[Accdet]HEADSET_MIC(4-pole) %s\n",
			     status ? "PlugIn" : "PlugOut");
		break;
	default:
		ACCDET_DEBUG("[Accdet]Invalid cableType\n");
	}
}

static void send_key_event(int keycode, int flag)
{
	if (call_status == 0) {
		switch (keycode) {
		case DW_KEY:
			input_report_key(kpd_accdet_dev, KEY_NEXTSONG, flag);
			input_sync(kpd_accdet_dev);
			ACCDET_DEBUG("[Accdet] report KEY_NEXTSONG %d\n", flag);
			break;
		case UP_KEY:
			input_report_key(kpd_accdet_dev, KEY_PREVIOUSSONG,
					 flag);
			input_sync(kpd_accdet_dev);
			ACCDET_DEBUG("[Accdet] report KEY_PREVIOUSSONG %d\n",
				     flag);
			break;
#ifdef FOUR_KEY_HEADSET
		case AS_KEY:
			input_report_key(kpd_accdet_dev, KEY_VOICECOMMAND,
					 flag);
			input_sync(kpd_accdet_dev);
			ACCDET_DEBUG("[Accdet] report KEY_VOICECOMMAND %d\n",
				     flag);
			break;
#endif
		}
	} else {
		switch (keycode) {
		case DW_KEY:
			input_report_key(kpd_accdet_dev, KEY_VOLUMEDOWN, flag);
			input_sync(kpd_accdet_dev);
			ACCDET_DEBUG("[Accdet] report KEY_VOLUMEDOWN %d\n",
				     flag);
			break;
		case UP_KEY:
			input_report_key(kpd_accdet_dev, KEY_VOLUMEUP, flag);
			input_sync(kpd_accdet_dev);
			ACCDET_DEBUG("[Accdet] report KEY_VOLUMEUP %d\n", flag);
			break;
#ifdef FOUR_KEY_HEADSET
		case AS_KEY:
			input_report_key(kpd_accdet_dev, KEY_VOICECOMMAND,
					 flag);
			input_sync(kpd_accdet_dev);
			ACCDET_DEBUG("[Accdet] report KEY_VOICECOMMAND %d\n",
				     flag);
			break;
#endif
		}
	}
}

static int multi_key_detection(void)
{
	int current_status = 0;
	int index = 0;
	int count = long_press_time / (KEY_SAMPLE_PERIOD + 40); /* ADC delay */
	int m_key = 0;
	int cur_key = 0;
	int cali_voltage = 0;

#ifdef FOUR_KEY_HEADSET
	int dcali_voltage = 0;

	ACCDET_DEBUG("[Accdet]adc ADC_GE = %d mv  ADC_OE = %d mv\n", ADC_GE,
		     ADC_OE);
#endif
	cali_voltage = PMIC_IMM_GetOneChannelValue(MULTIKEY_ADC_CHANNEL, 1, 1);
	ACCDET_DEBUG("[Accdet]adc before cali_voltage1 = %d mv\n",
		     cali_voltage);

#ifdef FOUR_KEY_HEADSET
	dcali_voltage = (ADC_GE + 1024) * cali_voltage + ADC_OE * 1200;
	cali_voltage = dcali_voltage >> 10;
	ACCDET_DEBUG("[Accdet]adc after cali_voltage1 = %d mv\n", cali_voltage);
#endif
	ACCDET_DEBUG("[Accdet]*********detect key.\n");

	m_key = cur_key = key_check(cali_voltage);

	send_key_event(m_key, KEYDOWN_FLAG);

	while (index++ < count) {
		/* Check if the current state has been changed */
		current_status =
			((pmic_pwrap_read(ACCDET_STATE_RG) & 0xc0) >> 6);
		ACCDET_DEBUG("[Accdet]accdet current_status = %d\n",
			     current_status);

		if (current_status != 0) {
			send_key_event(m_key, KEYUP_FLAG);
			return (m_key | SHORT_PRESS);
		}

		/* Check if the voltage has been changed (press one key and
		 * another)
		 */
		cali_voltage =
			PMIC_IMM_GetOneChannelValue(MULTIKEY_ADC_CHANNEL, 1, 1);
#ifdef FOUR_KEY_HEADSET
		dcali_voltage = (ADC_GE + 1024) * cali_voltage + ADC_OE * 1200;
		cali_voltage = dcali_voltage >> 10;
#endif

		ACCDET_DEBUG("[Accdet]adc in while loop [%d]= %d mv\n", index,
			     cali_voltage);
		cur_key = key_check(cali_voltage);

		if (m_key != cur_key) {
			send_key_event(m_key, KEYUP_FLAG);
			ACCDET_DEBUG(
				"[Accdet]accdet press one key and another happen!!\n");
			return (m_key | SHORT_PRESS);
		}

		m_key = cur_key;
		msleep(KEY_SAMPLE_PERIOD);
	}

	return (m_key | LONG_PRESS);
}

#endif

static void accdet_workqueue_func(void)
{
	int ret;

	ret = queue_work(accdet_workqueue, &accdet_work);
	if (!ret)
		ACCDET_DEBUG("[Accdet]accdet_work return:%d!\n", ret);
}

int accdet_irq_handler(void)
{
	int i = 0;

	if ((pmic_pwrap_read(ACCDET_IRQ_STS) & IRQ_STATUS_BIT))
		clear_accdet_interrupt();
#ifdef ACCDET_MULTI_KEY_FEATURE
	if (accdet_status == MIC_BIAS) {
		accdet_auxadc_switch(1);
		pmic_pwrap_write(
			ACCDET_PWM_WIDTH,
			REGISTER_VALUE(cust_headset_settings->pwm_width));
		pmic_pwrap_write(
			ACCDET_PWM_THRESH,
			REGISTER_VALUE(cust_headset_settings->pwm_width));
	}
#endif
	accdet_workqueue_func();
	while (((pmic_pwrap_read(ACCDET_IRQ_STS) & IRQ_STATUS_BIT) && i < 10)) {
		i++;
		udelay(200);
	}
	return 1;
}

static irqreturn_t accdet_pmic_int_handler(int irq, void *dev_id)
{
	int ret = 0;

	ACCDET_DEBUG("%s:\n", __func__);

	ret = accdet_irq_handler();
	if (ret == 0)
		ACCDET_DEBUG("[accdet_pmic_int_handler] don't finish\n");

	return IRQ_HANDLED;
}

/*clear ACCDET IRQ in accdet register*/
static inline void clear_accdet_interrupt(void)
{
	/*it is safe by using polling to adjust when to clear IRQ_CLR_BIT*/
	pmic_pwrap_write(ACCDET_IRQ_STS,
			 ((pmic_pwrap_read(ACCDET_IRQ_STS)) & 0x8000) |
				 (IRQ_CLR_BIT));
	ACCDET_DEBUG("[Accdet]clear_accdet_interrupt: ACCDET_IRQ_STS = 0x%x\n",
		     pmic_pwrap_read(ACCDET_IRQ_STS));
}

static inline void clear_accdet_eint_interrupt(void)
{
	/*
	 *	pmic_pwrap_write(ACCDET_IRQ_STS,
	 *  (((pmic_pwrap_read(ACCDET_IRQ_STS)) & 0x8000) | IRQ_EINT_CLR_BIT));
	 *	ACCDET_DEBUG("[Accdet]clear_accdet_eint_interrupt:
	 *  ACCDET_IRQ_STS = 0x%x\n", pmic_pwrap_read(ACCDET_IRQ_STS));
	 */
}

#ifdef SW_WORK_AROUND_ACCDET_REMOTE_BUTTON_ISSUE

#define ACC_ANSWER_CALL 1
#define ACC_END_CALL 2
#define ACC_MEDIA_PLAYPAUSE 3

#ifdef ACCDET_MULTI_KEY_FEATURE
#define ACC_MEDIA_STOP 4
#define ACC_MEDIA_NEXT 5
#define ACC_MEDIA_PREVIOUS 6
#define ACC_VOLUMEUP 7
#define ACC_VOLUMEDOWN 8
#endif

static atomic_t send_event_flag = ATOMIC_INIT(0);

static DECLARE_WAIT_QUEUE_HEAD(send_event_wq);

static int accdet_key_event;

static int sendKeyEvent(void *unuse)
{
	while (1) {
		ACCDET_DEBUG(" accdet:sendKeyEvent wait\n");
		/* wait for signal */
		wait_event_interruptible(send_event_wq,
					 (atomic_read(&send_event_flag) != 0));

		__pm_wakeup_event(accdet_key_lock,
				  2 * 1000); /* set the wake lock. */
		ACCDET_DEBUG(" accdet:going to send event %d\n",
			     accdet_key_event);

		if (accdet_status != PLUG_OUT) {
			/* send key event */
			if (accdet_key_event == ACC_END_CALL) {
				ACCDET_DEBUG(
					"[Accdet] report end call(1->0)!\n");
				input_report_key(kpd_accdet_dev, KEY_ENDCALL,
						 1);
				input_report_key(kpd_accdet_dev, KEY_ENDCALL,
						 0);
				input_sync(kpd_accdet_dev);
			}
#ifdef ACCDET_MULTI_KEY_FEATURE
			if (accdet_key_event == ACC_MEDIA_PLAYPAUSE) {
				ACCDET_DEBUG(
					"[Accdet] report PLAY_PAUSE (1->0)!\n");
				input_report_key(kpd_accdet_dev, KEY_PLAYPAUSE,
						 1);
				input_report_key(kpd_accdet_dev, KEY_PLAYPAUSE,
						 0);
				input_sync(kpd_accdet_dev);
			}
			/* next, previous, volumeup, volumedown send key in
			 * multi_key_detection()
			 */
			if (accdet_key_event == ACC_MEDIA_NEXT)
				ACCDET_DEBUG("[Accdet] NEXT ..\n");

			if (accdet_key_event == ACC_MEDIA_PREVIOUS)
				ACCDET_DEBUG("[Accdet] PREVIOUS..\n");

			if (accdet_key_event == ACC_VOLUMEUP)
				ACCDET_DEBUG("[Accdet] VOLUMUP ..\n");

			if (accdet_key_event == ACC_VOLUMEDOWN)
				ACCDET_DEBUG("[Accdet] VOLUMDOWN..\n");
#else
			if (accdet_key_event == ACC_MEDIA_PLAYPAUSE) {
				ACCDET_DEBUG(
					"[Accdet]report PLAY_PAUSE (1->0)!\n");
				input_report_key(kpd_accdet_dev, KEY_PLAYPAUSE,
						 1);
				input_report_key(kpd_accdet_dev, KEY_PLAYPAUSE,
						 0);
				input_sync(kpd_accdet_dev);
			}
#endif
		}
		atomic_set(&send_event_flag, 0);

		/* wake_unlock(&accdet_key_lock); //unlock wake lock */
	}
	return 0;
}

static ssize_t notify_sendKeyEvent(int event)
{

	accdet_key_event = event;
	atomic_set(&send_event_flag, 1);
	wake_up(&send_event_wq);
	ACCDET_DEBUG(" accdet:notify_sendKeyEvent !\n");
	return 0;
}
#endif

static inline void check_cable_type(void)
{
	int current_status = 0;
	int irq_temp = 0; /* for clear IRQ_bit */
	int wait_clear_irq_times = 0;

	ACCDET_DEBUG("[Accdet]--------->check cable type.\n");
	current_status = ((pmic_pwrap_read(ACCDET_STATE_RG) & 0xc0) >>
			  6); /* A=bit1; B=bit0 */

	ACCDET_DEBUG("[Accdet]accdet interrupt happen:[%s]current AB = %d\n",
		     accdet_status_string[accdet_status], current_status);

	button_status = 0;
	pre_status = accdet_status;

	IRQ_CLR_FLAG = false;
	switch (accdet_status) {
	case PLUG_OUT:
		if (current_status == 0) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1) {
				cable_type = HEADSET_NO_MIC;
				accdet_status = HOOK_SWITCH;
			} else {
				ACCDET_DEBUG(
					"[Accdet] Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
		} else if (current_status == 1) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1) {
				accdet_status = MIC_BIAS;
				cable_type = HEADSET_MIC;
			} else {
				ACCDET_DEBUG(
					"[Accdet] Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
			pmic_pwrap_write(ACCDET_DEBOUNCE0,
					 button_press_debounce);
			/* recover polling set AB 00-01 */
			/* #ifdef ACCDET_LOW_POWER */
			/* wake_unlock(&accdet_timer_lock);//add for suspend
			 * disable accdet more than 5S
			 */
			/* #endif */
		} else if (current_status == 3) {
			ACCDET_DEBUG("[Accdet]PLUG_OUT state not change!\n");
#ifdef ACCDET_MULTI_KEY_FEATURE
			ACCDET_DEBUG(
				"[Accdet] do not send plug out event in plug out\n");
#else
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1) {
				accdet_status = PLUG_OUT;
				cable_type = NO_DEVICE;
			} else {
				ACCDET_DEBUG(
					"[Accdet] Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
#ifdef ACCDET_EINT
			disable_accdet();
#endif
#endif
		} else {
			ACCDET_DEBUG(
				"[Accdet]PLUG_OUT can't change to this state!\n");
		}
		break;

	case MIC_BIAS:
		/* ALPS00038030:reduce the time of remote button pressed during
		 * incoming call
		 */
		/* solution: resume hook switch debounce time */
		pmic_pwrap_write(ACCDET_DEBOUNCE0,
				 cust_headset_settings->debounce0);
		if (current_status == 0) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1) {
				while ((pmic_pwrap_read(ACCDET_IRQ_STS) &
					IRQ_STATUS_BIT) &&
				       (wait_clear_irq_times < 3)) {
					ACCDET_DEBUG(
						"[Accdet]MIC BIAS clear IRQ on-going1....\n");
					wait_clear_irq_times++;
					msleep(20);
				}
				irq_temp = pmic_pwrap_read(ACCDET_IRQ_STS);
				irq_temp = irq_temp & (~IRQ_CLR_BIT);
				pmic_pwrap_write(ACCDET_IRQ_STS, irq_temp);
				IRQ_CLR_FLAG = true;
				accdet_status = HOOK_SWITCH;
			} else {
				ACCDET_DEBUG(
					"[Accdet] Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
			button_status = 1;
			if (button_status) {
#ifdef ACCDET_MULTI_KEY_FEATURE
				int multi_key = NO_KEY;
				/* mdelay(10); */
				/* if plug out don't send key */
				mutex_lock(&accdet_eint_irq_sync_mutex);
				if (eint_accdet_sync_flag == 1)
					multi_key = multi_key_detection();
				else
					ACCDET_DEBUG(
						"[Accdet] multi_key_detection: Headset has plugged out\n");
				mutex_unlock(&accdet_eint_irq_sync_mutex);
				accdet_auxadc_switch(0);
				/* recover	pwm frequency and duty */
				pmic_pwrap_write(
					ACCDET_PWM_WIDTH,
					REGISTER_VALUE(cust_headset_settings
							       ->pwm_width));
				pmic_pwrap_write(
					ACCDET_PWM_THRESH,
					REGISTER_VALUE(cust_headset_settings
							       ->pwm_thresh));

				switch (multi_key) {
				case SHORT_UP:
					ACCDET_DEBUG(
						"[Accdet] Short press up (0x%x)\n",
						multi_key);
					if (call_status == 0)
						notify_sendKeyEvent(
							ACC_MEDIA_PREVIOUS);
					else
						notify_sendKeyEvent(
							ACC_VOLUMEUP);
					break;
				case SHORT_MD:
					ACCDET_DEBUG(
						"[Accdet] Short press middle (0x%x)\n",
						multi_key);
					notify_sendKeyEvent(
						ACC_MEDIA_PLAYPAUSE);
					break;
				case SHORT_DW:
					ACCDET_DEBUG(
						"[Accdet] Short press down (0x%x)\n",
						multi_key);
					if (call_status == 0)
						notify_sendKeyEvent(
							ACC_MEDIA_NEXT);
					else
						notify_sendKeyEvent(
							ACC_VOLUMEDOWN);
					break;
				case LONG_UP:
					ACCDET_DEBUG(
						"[Accdet] Long press up (0x%x)\n",
						multi_key);
					send_key_event(UP_KEY, KEYUP_FLAG);
					break;
				case LONG_MD:
					ACCDET_DEBUG(
						"[Accdet] Long press middle (0x%x)\n",
						multi_key);
					/* notify_sendKeyEvent(ACC_END_CALL); */
					notify_sendKeyEvent(ACC_END_CALL);
					break;
				case LONG_DW:
					ACCDET_DEBUG(
						"[Accdet] Long press down (0x%x)\n",
						multi_key);
					send_key_event(DW_KEY, KEYUP_FLAG);
					break;
				default:
					ACCDET_DEBUG(
						"[Accdet] unknown key (0x%x)\n",
						multi_key);
					break;
				}
#else
				if (call_status != 0) {
					if (is_long_press()) {
						ACCDET_DEBUG(
							"[Accdet]send long press remote button event %d\n",
							ACC_END_CALL);
						notify_sendKeyEvent(
							ACC_END_CALL);
					} else {
						ACCDET_DEBUG(
							"[Accdet]send short press remote button event %d\n",
							ACC_ANSWER_CALL);
						notify_sendKeyEvent(
							ACC_MEDIA_PLAYPAUSE);
					}
				}
#endif /* //end  ifdef ACCDET_MULTI_KEY_FEATURE else */
			}
		} else if (current_status == 1) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1) {
				accdet_status = MIC_BIAS;
				cable_type = HEADSET_MIC;
				ACCDET_DEBUG(
					"[Accdet]MIC_BIAS state not change!\n");
			} else {
				ACCDET_DEBUG(
					"[Accdet] Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
		} else if (current_status == 3) {
#ifdef ACCDET_MULTI_KEY_FEATURE
			ACCDET_DEBUG(
				"[Accdet]do not send plug ou in micbiast\n");
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1)
				accdet_status = PLUG_OUT;
			else
				ACCDET_DEBUG(
					"[Accdet] Headset has plugged out\n");
			mutex_unlock(&accdet_eint_irq_sync_mutex);
#else
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1) {
				accdet_status = PLUG_OUT;
				cable_type = NO_DEVICE;
			} else {
				ACCDET_DEBUG(
					"[Accdet] Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
#ifdef ACCDET_EINT
			disable_accdet();
#endif
#endif
		} else {
			ACCDET_DEBUG(
				"[Accdet]MIC_BIAS can't change to this state!\n");
		}
		break;

	case HOOK_SWITCH:
		if (current_status == 0) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1) {
				/* for avoid 01->00 framework of Headset will
				 * report press key info for Audio
				 */
				/* cable_type = HEADSET_NO_MIC; */
				/* accdet_status = HOOK_SWITCH; */
				ACCDET_DEBUG(
					"[Accdet]HOOK_SWITCH state not change!\n");
			} else {
				ACCDET_DEBUG(
					"[Accdet] Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
		} else if (current_status == 1) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1) {
				accdet_status = MIC_BIAS;
				cable_type = HEADSET_MIC;
			} else {
				ACCDET_DEBUG(
					"[Accdet] Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
			/* ALPS00038030:reduce the time of remote button pressed
			 * during incoming call
			 */
			/* solution: reduce hook switch debounce time to 0x400
			 */
			pmic_pwrap_write(ACCDET_DEBOUNCE0,
					 button_press_debounce);
			/* #ifdef ACCDET_LOW_POWER */
			/* wake_unlock(&accdet_timer_lock);//add for suspend
			 * disable accdet more than 5S
			 */
			/* #endif */
		} else if (current_status == 3) {
#ifdef ACCDET_MULTI_KEY_FEATURE
			ACCDET_DEBUG(
				"[Accdet] do not send plug out event in hook switch\n");
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1)
				accdet_status = PLUG_OUT;
			else
				ACCDET_DEBUG(
					"[Accdet] Headset has plugged out\n");

			mutex_unlock(&accdet_eint_irq_sync_mutex);
#else
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1) {
				accdet_status = PLUG_OUT;
				cable_type = NO_DEVICE;
			} else {
				ACCDET_DEBUG(
					"[Accdet] Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
#ifdef ACCDET_EINT
			disable_accdet();
#endif
#endif
		} else {
			ACCDET_DEBUG(
				"[Accdet]HOOK_SWITCH can't change to this state!\n");
		}
		break;

	case STAND_BY:
		if (current_status == 3) {
#ifdef ACCDET_MULTI_KEY_FEATURE
			ACCDET_DEBUG(
				"[Accdet]accdet do not send plug out event in stand by!\n");
#else
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag == 1) {
				accdet_status = PLUG_OUT;
				cable_type = NO_DEVICE;
			} else {
				ACCDET_DEBUG(
					"[Accdet] Headset has plugged out\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
#ifdef ACCDET_EINT
			disable_accdet();
#endif
#endif
		} else {
			ACCDET_DEBUG(
				"[Accdet]STAND_BY can't change to this state!\n");
		}
		break;
	default:
		ACCDET_DEBUG("[Accdet]accdet current status error!\n");
		break;
	}

	if (!IRQ_CLR_FLAG) {
		mutex_lock(&accdet_eint_irq_sync_mutex);
		if (eint_accdet_sync_flag == 1) {
			while ((pmic_pwrap_read(ACCDET_IRQ_STS) &
				IRQ_STATUS_BIT) &&
			       (wait_clear_irq_times < 3)) {
				ACCDET_DEBUG(
					"[Accdet] Clear interrupt on-going2....\n");
				wait_clear_irq_times++;
				msleep(20);
			}
		}
		irq_temp = pmic_pwrap_read(ACCDET_IRQ_STS);
		irq_temp = irq_temp & (~IRQ_CLR_BIT);
		pmic_pwrap_write(ACCDET_IRQ_STS, irq_temp);
		mutex_unlock(&accdet_eint_irq_sync_mutex);
		IRQ_CLR_FLAG = true;
		ACCDET_DEBUG(
			"[Accdet]check_cable_type:Clear interrupt:Done[0x%x]!\n",
			pmic_pwrap_read(ACCDET_IRQ_STS));
	} else {
		IRQ_CLR_FLAG = false;
	}

	ACCDET_DEBUG("[Accdet]cable type:[%s], status switch:[%s]->[%s]\n",
		     accdet_report_string[cable_type],
		     accdet_status_string[pre_status],
		     accdet_status_string[accdet_status]);
}

static void accdet_work_callback(struct work_struct *work)
{
	__pm_stay_awake(accdet_irq_lock);

	check_cable_type();

	mutex_lock(&accdet_eint_irq_sync_mutex);
	if (eint_accdet_sync_flag == 1) {
		ACCDET_DEBUG("%s %d\n", __func__, __LINE__);
		send_accdet_status_event(cable_type, 1);
	} else {
		ACCDET_DEBUG(
			"[Accdet] Headset has plugged out don't set accdet state\n");
	}
	mutex_unlock(&accdet_eint_irq_sync_mutex);

	ACCDET_DEBUG(" [accdet] set state in cable_type  status\n");

	__pm_relax(accdet_irq_lock);
}

void accdet_get_dts_data(void)
{
	struct device_node *node = NULL;
	int debounce[7];
#ifdef FOUR_KEY_HEADSET
	int four_key[5];
#else
	int three_key[4];
#endif

	ACCDET_INFO("[ACCDET]Start accdet_get_dts_data");
	node = of_find_matching_node(node, accdet_of_match);
	if (node) {
		of_property_read_u32_array(node, "headset-mode-setting",
					   debounce, ARRAY_SIZE(debounce));
		of_property_read_u32(node, "accdet-mic-vol",
				     &accdet_dts_data.mic_mode_vol);
		of_property_read_u32(node, "accdet-plugout-debounce",
				     &accdet_dts_data.accdet_plugout_debounce);
		of_property_read_u32(node, "accdet-mic-mode",
				     &accdet_dts_data.accdet_mic_mode);
#ifdef FOUR_KEY_HEADSET
		of_property_read_u32_array(node, "headset-four-key-threshold",
					   four_key, ARRAY_SIZE(four_key));
		memcpy(&accdet_dts_data.four_key, four_key + 1,
		       sizeof(four_key));
		ACCDET_INFO(
			"[Accdet]mid-Key = %d, voice = %d, up_key = %d, down_key = %d\n",
			accdet_dts_data.four_key.mid_key_four,
			accdet_dts_data.four_key.voice_key_four,
			accdet_dts_data.four_key.up_key_four,
			accdet_dts_data.four_key.down_key_four);
#else
		of_property_read_u32_array(node, "headset-three-key-threshold",
					   three_key, ARRAY_SIZE(three_key));
		memcpy(&accdet_dts_data.three_key, three_key + 1,
		       sizeof(three_key));
		ACCDET_INFO(
			"[Accdet]mid-Key = %d, up_key = %d, down_key = %d\n",
			accdet_dts_data.three_key.mid_key,
			accdet_dts_data.three_key.up_key,
			accdet_dts_data.three_key.down_key);
#endif

		memcpy(&accdet_dts_data.headset_debounce, debounce,
		       sizeof(debounce));
		cust_headset_settings = &accdet_dts_data.headset_debounce;
		ACCDET_INFO(
			"[Accdet]pwm_width = %x, pwm_thresh = %x\n deb0 = %x, deb1 = %x, mic_mode = %d\n",
			cust_headset_settings->pwm_width,
			cust_headset_settings->pwm_thresh,
			cust_headset_settings->debounce0,
			cust_headset_settings->debounce1,
			accdet_dts_data.accdet_mic_mode);
	} else {
		ACCDET_ERROR("[Accdet]%s can't find compatible dts node\n",
			     __func__);
	}
}

static inline void accdet_init(void)
{
	ACCDET_DEBUG("\n[Accdet]accdet hardware init\n");
	pmic_pwrap_write(TOP_CKPDN_CLR, RG_ACCDET_CLK_CLR);
	/* reset the accdet unit */

	pmic_pwrap_write(TOP_RST_ACCDET_SET, ACCDET_RESET_SET);
	pmic_pwrap_write(TOP_RST_ACCDET_CLR, ACCDET_RESET_CLR);
	/* init  pwm frequency and duty */
	pmic_pwrap_write(ACCDET_PWM_WIDTH,
			 REGISTER_VALUE(cust_headset_settings->pwm_width));
	pmic_pwrap_write(ACCDET_PWM_THRESH,
			 REGISTER_VALUE(cust_headset_settings->pwm_thresh));

	pmic_pwrap_write(ACCDET_STATE_SWCTRL, 0x07);

	pmic_pwrap_write(ACCDET_EN_DELAY_NUM,
			 (cust_headset_settings->fall_delay << 15 |
			  cust_headset_settings->rise_delay));
	/* init the debounce time */
	pmic_pwrap_write(ACCDET_DEBOUNCE0, cust_headset_settings->debounce0);
	pmic_pwrap_write(ACCDET_DEBOUNCE1, cust_headset_settings->debounce1);
	pmic_pwrap_write(ACCDET_DEBOUNCE3, cust_headset_settings->debounce3);
	pmic_pwrap_write(ACCDET_IRQ_STS,
			 pmic_pwrap_read(ACCDET_IRQ_STS) & (~IRQ_CLR_BIT));
	pmic_pwrap_write(INT_CON_ACCDET_SET, RG_ACCDET_IRQ_SET);
	/* disable ACCDET unit */
	pre_state_swctrl = pmic_pwrap_read(ACCDET_STATE_SWCTRL);
	pmic_pwrap_write(ACCDET_CTRL, ACCDET_DISABLE);
	pmic_pwrap_write(ACCDET_STATE_SWCTRL, 0x0);
	pmic_pwrap_write(TOP_CKPDN_SET, RG_ACCDET_CLK_SET);
}

/*--------------------sysfs-----------------------*/
#if DEBUG_THREAD
static int dump_register(void)
{
	return 0;
}

static ssize_t accdet_store_call_state(struct device_driver *ddri,
				       const char *buf, size_t count)
{
	int ret;

	ret = kstrtoint(buf, 0, &call_status);
	if (ret != 0) {
		ACCDET_DEBUG("accdet: Invalid values\n");
		return -EINVAL;
	}

	switch (call_status) {
	case CALL_IDLE:
		ACCDET_DEBUG("[Accdet]accdet call: Idle state!\n");
		break;
	case CALL_RINGING:
		ACCDET_DEBUG("[Accdet]accdet call: ringing state!\n");
		break;
	case CALL_ACTIVE:
		ACCDET_DEBUG("[Accdet]accdet call: active or hold state!\n");
		/*return button_status;*/
		break;
	default:
		ACCDET_DEBUG("[Accdet]accdet call : Invalid values\n");
		break;
	}
	return count;
}

static ssize_t show_pin_recognition_state(struct device_driver *ddri, char *buf)
{
	return sprintf(buf, "%u\n", 0);
}

static DRIVER_ATTR(accdet_pin_recognition, 0664, show_pin_recognition_state,
		   NULL);
static DRIVER_ATTR(accdet_call_state, 0664, NULL, accdet_store_call_state);

static int g_start_debug_thread;
static struct task_struct *thread;
static int g_dump_register;
static int dbug_thread(void *unused)
{
	while (g_start_debug_thread) {
		if (g_dump_register) {
			dump_register();
			/*dump_pmic_register();*/
		}
		msleep(500);
	}
	return 0;
}

static ssize_t store_accdet_start_debug_thread(struct device_driver *ddri,
					       const char *buf, size_t count)
{
	int start_flag;
	int error;
	int ret;

	ret = kstrtoint(buf, 0, &start_flag);
	if (ret != 0) {
		ACCDET_DEBUG("accdet: Invalid values\n");
		return -EINVAL;
	}

	ACCDET_DEBUG("[Accdet] start flag =%d\n", start_flag);
	g_start_debug_thread = start_flag;

	if (start_flag == 1) {
		thread = kthread_run(dbug_thread, 0, "ACCDET");
		if (IS_ERR(thread)) {
			error = PTR_ERR(thread);
			ACCDET_DEBUG(" failed to create kernel thread: %d\n",
				     error);
		}
	}

	return count;
}

static ssize_t store_accdet_set_headset_mode(struct device_driver *ddri,
					     const char *buf, size_t count)
{
	int value;
	int ret;

	ret = kstrtoint(buf, 0, &value);
	if (ret != 0) {
		ACCDET_DEBUG("accdet: Invalid values\n");
		return -EINVAL;
	}

	ACCDET_DEBUG("[Accdet]store_accdet_set_headset_mode value =%d\n",
		     value);

	return count;
}

static ssize_t store_accdet_dump_register(struct device_driver *ddri,
					  const char *buf, size_t count)
{
	int value;
	int ret;

	ret = kstrtoint(buf, 0, &value);
	if (ret != 0) {
		ACCDET_DEBUG("accdet: Invalid values\n");
		return -EINVAL;
	}

	g_dump_register = value;

	ACCDET_DEBUG("[Accdet]store_accdet_dump_register value =%d\n", value);

	return count;
}

static ssize_t show_accdet_state(struct device_driver *ddri, char *buf)
{
	char temp_type = (char)cable_type;

	if (buf == NULL) {
		ACCDET_ERROR("[%s] *buf is NULL Pointer\n", __func__);
		return -EINVAL;
	}

	snprintf(buf, 3, "%d\n", temp_type);

	return strlen(buf);
}

/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(dump_register, 0644, NULL,
		   store_accdet_dump_register);

static DRIVER_ATTR(set_headset_mode, 0644, NULL,
		   store_accdet_set_headset_mode);

static DRIVER_ATTR(start_debug, 0644, NULL,
		   store_accdet_start_debug_thread);

static DRIVER_ATTR(state, 0644, show_accdet_state, NULL);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *accdet_attr_list[] = {
	&driver_attr_start_debug, &driver_attr_set_headset_mode,
	&driver_attr_dump_register, &driver_attr_accdet_call_state,
	&driver_attr_state,
	/*#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION*/
	&driver_attr_accdet_pin_recognition,
/*#endif*/
#if defined(ACCDET_TS3A225E_PIN_SWAP)
	&driver_attr_TS3A225EConnectorType,
#endif
};

static int accdet_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(ARRAY_SIZE(accdet_attr_list));

	if (driver == NULL)
		return -EINVAL;
	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, accdet_attr_list[idx]);
		if (err) {
			ACCDET_DEBUG("driver_create_file (%s) = %d\n",
				     accdet_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

#endif
void accdet_int_handler(void)
{
	int ret = 0;

	ACCDET_DEBUG("[accdet_int_handler]....\n");
	ret = accdet_irq_handler();
	if (ret == 0)
		ACCDET_DEBUG("[accdet_int_handler] don't finished\n");
}

void accdet_eint_int_handler(void)
{
	int ret = 0;

	ACCDET_DEBUG("[accdet_eint_int_handler]....\n");
	ret = accdet_irq_handler();
	if (ret == 0)
		ACCDET_DEBUG("[accdet_int_handler] don't finished\n");
}

#ifdef FOUR_KEY_HEADSET
void accdet_get_GE_OE(void)
{
	int efuse_ge = 0, efuse_oe = 0;

	efuse_ge = pmic_pwrap_read(0x01EC);
	ADC_GE = (efuse_ge >> 9) & 0x7f;
	ACCDET_INFO("efuse ge value = 0x%x\n", ADC_GE);

	efuse_oe = pmic_pwrap_read(0x01EE);
	ADC_OE = efuse_oe & 0x3f;
	ACCDET_INFO("efuse oe value = 0x%x\n", ADC_OE);

	if (ADC_GE & 0x40)
		ADC_GE = 0 - ((~(ADC_GE - 1)) & 0x7f);

	if (ADC_OE & 0x20)
		ADC_OE = 0 - ((~(ADC_OE - 1)) & 0x3f);

	ACCDET_INFO("ADC_GE = %d and ADC_OE = %d\n", ADC_GE, ADC_OE);
	ACCDET_INFO("efuse_ge value = 0x%x , efuse_oe value = 0x%x\n", efuse_ge,
		    efuse_oe);
}
#endif

static inline int accdet_setup_pmic_irq(struct platform_device *accdet_device)
{
	struct device_node *node;
	int irq_pmic;
	u32 ret;

	node = of_get_child_by_name(accdet_device->dev.of_node, "accdet-pmic");
	if (!node) {
		dev_info(&accdet_device->dev,
			"failed to find accdet-pmic sub node\n");
		return -ENODEV;
	}

	irq_pmic = irq_of_parse_and_map(node, 0);
	if (irq_pmic <= 0) {
		dev_info(&accdet_device->dev, "failed to get accdet-pmic irq\n");
		return -EINVAL;
	}

	ret = request_threaded_irq(irq_pmic, NULL, accdet_pmic_int_handler,
				   IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				   "mt6397-accdet", accdet_device);
	if (ret) {
		dev_info(&accdet_device->dev, "request accdet-pmic irq fail\n");
		return ret;
	}

	return 0;
}

int mt_accdet_probe(struct platform_device *dev)
{
	int ret = 0;

#if DEBUG_THREAD
	struct platform_driver accdet_driver_hal = accdet_driver_func();
#endif

#ifdef SW_WORK_AROUND_ACCDET_REMOTE_BUTTON_ISSUE
	struct task_struct *keyEvent_thread = NULL;
	int error = 0;
#endif

	ACCDET_INFO("[Accdet]accdet_probe begin!\n");

	/*----------------------------------------------------------------------
	 * Create normal device for auido use
	 *--------------------------------------------------------------------
	 */
	ret = alloc_chrdev_region(&accdet_devno, 0, 1, ACCDET_DEVNAME);
	if (ret)
		ACCDET_ERROR(
			"[Accdet]alloc_chrdev_region: Get Major number error!\n");

	accdet_cdev = cdev_alloc();
	accdet_cdev->owner = THIS_MODULE;
	accdet_cdev->ops = accdet_get_fops();
	ret = cdev_add(accdet_cdev, accdet_devno, 1);
	if (ret)
		ACCDET_ERROR("[Accdet]accdet error: cdev_add\n");

	accdet_class = class_create(THIS_MODULE, ACCDET_DEVNAME);

	/* if we want auto creat device node, we must call this*/
	accdet_nor_device = device_create(accdet_class, NULL, accdet_devno,
					  NULL, ACCDET_DEVNAME);

	/*--------------------------------------------------------------------
	 * Create input device
	 *--------------------------------------------------------------------
	 */
	kpd_accdet_dev = input_allocate_device();
	if (!kpd_accdet_dev) {
		ACCDET_ERROR("[Accdet]kpd_accdet_dev : fail!\n");
		return -ENOMEM;
	}
	/*INIT the timer to disable micbias.*/
	init_timer(&micbias_timer);
	micbias_timer.expires = jiffies + MICBIAS_DISABLE_TIMER;
	micbias_timer.function = &disable_micbias;
	micbias_timer.data = 0;

	/*define multi-key keycode*/
	__set_bit(EV_KEY, kpd_accdet_dev->evbit);
	__set_bit(KEY_CALL, kpd_accdet_dev->keybit);
	__set_bit(KEY_ENDCALL, kpd_accdet_dev->keybit);
	__set_bit(KEY_NEXTSONG, kpd_accdet_dev->keybit);
	__set_bit(KEY_PREVIOUSSONG, kpd_accdet_dev->keybit);
	__set_bit(KEY_PLAYPAUSE, kpd_accdet_dev->keybit);
	__set_bit(KEY_STOPCD, kpd_accdet_dev->keybit);
	__set_bit(KEY_VOLUMEDOWN, kpd_accdet_dev->keybit);
	__set_bit(KEY_VOLUMEUP, kpd_accdet_dev->keybit);
#ifdef FOUR_KEY_HEADSET
	__set_bit(KEY_VOICECOMMAND, kpd_accdet_dev->keybit);
#endif
	__set_bit(EV_SW, kpd_accdet_dev->evbit);
	__set_bit(SW_HEADPHONE_INSERT, kpd_accdet_dev->swbit);
	__set_bit(SW_MICROPHONE_INSERT, kpd_accdet_dev->swbit);
	__set_bit(SW_JACK_PHYSICAL_INSERT, kpd_accdet_dev->swbit);
	__set_bit(SW_LINEOUT_INSERT, kpd_accdet_dev->swbit);

	kpd_accdet_dev->id.bustype = BUS_HOST;
	kpd_accdet_dev->name = "ACCDET";
	if (input_register_device(kpd_accdet_dev))
		ACCDET_ERROR("[Accdet]kpd_accdet_dev register : fail!\n");
	/*------------------------------------------------------------------
	 * Create workqueue
	 *------------------------------------------------------------------
	 */
	accdet_workqueue = create_singlethread_workqueue("accdet");
	INIT_WORK(&accdet_work, accdet_work_callback);

/*------------------------------------------------------------------
 * wake lock
 * ------------------------------------------------------------------
 */
	accdet_suspend_lock = wakeup_source_register("accdet wakelock");
	accdet_irq_lock = wakeup_source_register("accdet irq wakelock");
	accdet_key_lock = wakeup_source_register("accdet key wakelock");
	accdet_timer_lock = wakeup_source_register("accdet key wakelock");

#if DEBUG_THREAD
	ret = accdet_create_attr(&accdet_driver_hal.driver);
	if (ret != 0)
		ACCDET_ERROR("create attribute err = %d\n", ret);
#endif

#ifdef SW_WORK_AROUND_ACCDET_REMOTE_BUTTON_ISSUE
	init_waitqueue_head(&send_event_wq);
	/* start send key event thread */
	keyEvent_thread = kthread_run(sendKeyEvent, 0, "keyEvent_send");
	if (IS_ERR(keyEvent_thread)) {
		ret = PTR_ERR(keyEvent_thread);
		ACCDET_ERROR(" failed to create kernel thread: %d\n", error);
	}
#endif

	if (g_accdet_first == 1) {
		eint_accdet_sync_flag = 1;
		/*Accdet Hardware Init*/
		accdet_get_dts_data();

		accdet_init();
		/*schedule a work for the first detection*/
		ACCDET_INFO(
			"[Accdet]accdet_probe : first time detect headset---->\n");
#ifdef FOUR_KEY_HEADSET
		accdet_get_GE_OE();
#endif
		queue_work(accdet_workqueue, &accdet_work);

		accdet_disable_workqueue =
			create_singlethread_workqueue("accdet_disable");
		INIT_WORK(&accdet_disable_work, disable_micbias_callback);
		accdet_eint_workqueue =
			create_singlethread_workqueue("accdet_eint");
		INIT_WORK(&accdet_eint_work, accdet_eint_work_callback);

		accdet_setup_pmic_irq(dev);
		accdet_setup_eint(dev);

		g_accdet_first = 0;
	}
	ACCDET_INFO("[Accdet]accdet_probe done!\n");
	return 0;
}

void mt_accdet_remove(void)
{
	ACCDET_DEBUG("[Accdet]accdet_remove begin!\n");

/*cancel_delayed_work(&accdet_work);*/
#if defined CONFIG_ACCDET_EINT || defined CONFIG_ACCDET_EINT_IRQ
	destroy_workqueue(accdet_eint_workqueue);
#endif
	destroy_workqueue(accdet_workqueue);
	device_del(accdet_nor_device);
	class_destroy(accdet_class);
	cdev_del(accdet_cdev);
	unregister_chrdev_region(accdet_devno, 1);
	input_unregister_device(kpd_accdet_dev);
	ACCDET_DEBUG("[Accdet]accdet_remove Done!\n");
}

void mt_accdet_suspend(void) /*only one suspend mode*/
{

#if defined CONFIG_ACCDET_EINT || defined CONFIG_ACCDET_EINT_IRQ
	ACCDET_DEBUG("[Accdet] in suspend1: ACCDET_IRQ_STS = 0x%x\n",
		     pmic_pwrap_read(ACCDET_IRQ_STS));
#else
	ACCDET_DEBUG(
		"[Accdet]accdet_suspend: ACCDET_CTRL=[0x%x], STATE=[0x%x]->[0x%x]\n",
		pmic_pwrap_read(ACCDET_CTRL), pre_state_swctrl,
		pmic_pwrap_read(ACCDET_STATE_SWCTRL));
#endif
}

void mt_accdet_resume(void) /*wake up*/
{
#if defined CONFIG_ACCDET_EINT || defined CONFIG_ACCDET_EINT_IRQ
	ACCDET_DEBUG("[Accdet] in resume1: ACCDET_IRQ_STS = 0x%x\n",
		     pmic_pwrap_read(ACCDET_IRQ_STS));
#else
	ACCDET_DEBUG(
		"[Accdet]accdet_resume: ACCDET_CTRL=[0x%x], STATE_SWCTRL=[0x%x]\n",
		pmic_pwrap_read(ACCDET_CTRL),
		pmic_pwrap_read(ACCDET_STATE_SWCTRL));

#endif
}

/**********************************************************************
 * add for IPO-H need update headset state when resume

***********************************************************************/
void mt_accdet_pm_restore_noirq(void)
{
	int current_status_restore = 0;

	ACCDET_DEBUG("[Accdet]accdet_pm_restore_noirq start!\n");
	/*enable ACCDET unit*/
	ACCDET_DEBUG("accdet: enable_accdet\n");
	/*enable clock*/
	pmic_pwrap_write(TOP_CKPDN_CLR, RG_ACCDET_CLK_CLR);
	enable_accdet(ACCDET_SWCTRL_EN);
	pmic_pwrap_write(
		ACCDET_STATE_SWCTRL,
		(pmic_pwrap_read(ACCDET_STATE_SWCTRL) | ACCDET_SWCTRL_IDLE_EN));

	eint_accdet_sync_flag = 1;
	current_status_restore =
		((pmic_pwrap_read(ACCDET_STATE_RG) & 0xc0) >> 6); /*AB*/

	switch (current_status_restore) {
	case 0: /*AB=0*/
		cable_type = HEADSET_NO_MIC;
		accdet_status = HOOK_SWITCH;
		send_accdet_status_event(cable_type, 1);
		break;
	case 1: /*AB=1*/
		cable_type = HEADSET_MIC;
		accdet_status = MIC_BIAS;
		send_accdet_status_event(cable_type, 1);
		break;
	case 3: /*AB=3*/
		send_accdet_status_event(cable_type, 0);
		cable_type = NO_DEVICE;
		accdet_status = PLUG_OUT;
		break;
	default:
		ACCDET_DEBUG(
			"[Accdet]accdet_pm_restore_noirq: accdet current status error!\n");
		break;
	}
	if (cable_type == NO_DEVICE) {
		/*disable accdet*/
		pre_state_swctrl = pmic_pwrap_read(ACCDET_STATE_SWCTRL);
#ifdef CONFIG_ACCDET_EINT
		pmic_pwrap_write(ACCDET_STATE_SWCTRL, 0);
		pmic_pwrap_write(ACCDET_CTRL, ACCDET_DISABLE);
		/*disable clock*/
		pmic_pwrap_write(TOP_CKPDN_SET, RG_ACCDET_CLK_SET);
#endif
	}
}

/*//////////////////////////////////IPO_H
 * end/////////////////////////////////////////////
 */
long mt_accdet_unlocked_ioctl(unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case ACCDET_INIT:
		break;
	case SET_CALL_STATE:
		call_status = (int)arg;
		ACCDET_DEBUG("[Accdet]accdet_ioctl : CALL_STATE=%d\n",
			     call_status);
		break;
	case GET_BUTTON_STATUS:
		return button_status;
	default:
		ACCDET_DEBUG("[Accdet]accdet_ioctl : default\n");
		break;
	}
	return 0;
}
