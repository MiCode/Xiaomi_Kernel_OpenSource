/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include <linux/timer.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#ifdef CONFIG_ACCDET_EINT
#include <linux/of_gpio.h>
#endif

/* for EINT register and enum */
#include <upmu_common.h>
#include "accdet.h"

#include "mtk_auxadc_intf.h"

#define DEBUG_THREAD			(1)
#define ACCDET_INIT0_ONCE	0/* reset accdet sw, enable clock...  */
#define ACCDET_INIT1_ONCE	1/* config analog, enable accdet eint, top interrupt...  */
#define ACCDET_HW_MODE		0/* 0,disable;1,enable */

/* ---------------------------------------------------------------------
 * PART0: Macro definition
 */
#if 0
 /* Temporary: fix too much log when HW trigger interrupt burst crazy */
#define ACCDET_DEBUG(format, args...)	pr_debug_ratelimited(format, ##args)
#define ACCDET_INFO(format, args...)	pr_debug_ratelimited(format, ##args)
#define ACCDET_ERROR(format, args...)	pr_debug_ratelimited(format, ##args)
#endif
#define ACCDET_DEBUG(format, args...)	pr_debug(format, ##args)
#define ACCDET_INFO(format, args...)	pr_info(format, ##args)
#define ACCDET_ERROR(format, args...)	pr_info(format, ##args)

static void send_accdet_status_event(int cable_type, int status);

/* for accdet_read_audio_res */
#define RET_LT_5K				(-1)/* less than 5k ohm, return -1 */
#define RET_GT_5K			(0)/* greater than 5k ohm, return 0 */
#define REGISTER_VALUE(x)		(x - 1)

/* HW_JACK_TYPE_*: it's HW default properity
 * (0)-->Jack default level:
 *		plug_in---Low level
 *		plug_out--High level
 * (1)-->Jack default level:
 *		plug_in---High level
 *		plug_out--Low level
 */
#ifdef CONFIG_ACCDET_EINT_IRQ
/* #define HW_MODE_SUPPORT */
#define HW_JACK_TYPE_0			(0)
#define HW_JACK_TYPE_1			(1)
#endif

/* Used to let accdet know if the pin has been fully plugged-in */
#define EINT_PIN_PLUG_IN			(0x01)
#define EINT_PIN_PLUG_OUT		(0x00)
#define EINT_PIN_MOISTURE_DETECED -1

#define MICBIAS_DISABLE_TIMER	(6 * HZ)/* 6 seconds */
#define ACCDET_INIT_WAIT_TIMER	(10 * HZ)/* 10 seconds */

#define KEY_SAMPLE_PERIOD		(60)/* ms */
#define MULTIKEY_ADC_CHANNEL	(8)
#define NO_KEY					(0x0)
#define UP_KEY					(0x01)
#define MD_KEY					(0x02)
#define DW_KEY					(0x04)
#define AS_KEY					(0x08)

#define ACCDET_EINT0_IRQ_IN		(1<<0)
#define ACCDET_EINT1_IRQ_IN		(1<<1)
#define ACCDET_EINT_IRQ_IN		(1<<2)
#define ACCDET_IRQ_IN			(1<<3)

#define HEADSET_MODE_1			(1)
#define HEADSET_MODE_2			(2)
#define HEADSET_MODE_6			(6)


#define ACCDET_AUXADC_DEBOUNCE	(0x42)/* 2ms */

/* ---------------------------------------------------------------------
 * PART1: global variable definition
 */
#ifdef CONFIG_ACCDET_EINT
unsigned int g_gpio_pin;
unsigned int g_gpio_headset_deb;
int g_accdet_irq;
struct pinctrl *accdet_pinctrl1;
struct pinctrl_state *pins_eint_int;
#endif
#ifdef HW_MODE_SUPPORT
#define DIGITAL_FASTDISCHARGE_SUPPORT
bool fast_discharge = true;
#endif
static int g_accdet_auxadc_offset;
#if defined(CONFIG_MOISTURE_EXTERNAL_SUPPORT) || defined(CONFIG_MOISTURE_INTERNAL_SUPPORT)
static int g_moisture_vdd_offset; /* unit is mv */
static int g_moisture_offset; /* unit is mv */
static int g_moisture_vm; /* unit is mv */
static int water_r = 10000; /* moisture resister ohm */
#endif
#ifdef CONFIG_MOISTURE_INTERNAL_SUPPORT
static int g_moisture_eint_offset; /* unit is ohm */
#endif
#ifdef CONFIG_MOISTURE_EXTERNAL_SUPPORT
static int external_r = 470000; /* unit is ohm */
#endif
#ifdef CONFIG_MOISTURE_INTERNAL_SUPPORT
static int internal_r = 47000; /* unit is ohm */
#endif
static int g_cur_key;
static unsigned int g_accdet_eint_type = IRQ_TYPE_LEVEL_LOW;
static int g_cur_eint_state = EINT_PIN_PLUG_OUT;

#ifdef CONFIG_ACCDET_SUPPORT_BI_EINT
static int g_cur_eint0_state = EINT_PIN_PLUG_OUT;
static int g_cur_eint1_state = EINT_PIN_PLUG_OUT;
#endif

static struct wake_lock accdet_suspend_lock;
static struct wake_lock accdet_irq_lock;
static struct wake_lock accdet_key_lock;
static struct wake_lock accdet_timer_lock;
static struct timer_list micbias_timer;
static struct timer_list  accdet_init_timer;

static struct config_accdet_param *accdet_cust_setting;
static struct config_headset_param headset_dts_data;

char *accdet_status_string[] = {
	"Plug_out",
	"Headset_plug_in",
	"Hook_switch",
	"Bi_mic",
	"Line_out",
	"Stand_by"
};
char *accdet_report_string[] = {
	"No_device",
	"Headset_mic",
	"Headset_no_mic",
	"Headset_five_pole",
	"Line_out_device"
};

/* ---------------------------------------------------------------------
 * PART2: static variable definition
 */
/* static bool IRQ_CLR_FLAG = 0; //use local var */
static int s_call_status;
static int s_button_status;
static atomic_t s_accdet_first;
static int s_eint_accdet_sync_flag;
#ifdef CONFIG_FOUR_KEY_HEADSET
static int s_4_key_efuse_flag;
#endif
static int s_button_press_debounce = 0x400;
static int s_pre_status = PLUG_OUT;
static int s_pre_state_swctrl;
static int s_accdet_status = PLUG_OUT;
static int s_cable_type = NO_DEVICE;

static struct cdev *accdet_cdev;
static struct class *accdet_class;
static struct device *accdet_nor_device;
static struct input_dev *kpd_accdet_dev;
static dev_t accdet_devno;

static struct workqueue_struct *accdet_workqueue;
static struct workqueue_struct *accdet_eint_workqueue;
static struct workqueue_struct *accdet_disable_workqueue;
static struct work_struct accdet_work;
static struct work_struct accdet_eint_work;
static struct work_struct accdet_disable_work;

/* add for new feature PIN recognition */
#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION
int cable_pin_recognition;
static int show_icon_delay;
#endif
#if DEBUG_THREAD
static int s_dump_register;
static int s_start_debug_thread;
static struct task_struct *s_thread;
#endif


/*
 * PART3: function declaration
 */
static DEFINE_MUTEX(accdet_multikey_mutex);
static DEFINE_MUTEX(accdet_eint_irq_sync_mutex);

static u32 pmic_pwrap_read(u32 addr);
static void pmic_pwrap_write(u32 addr, unsigned int wdata);
#if DEBUG_THREAD
static unsigned int  rw_value[2];
static int dump_register(void);
#endif
static void disable_micbias(unsigned long a);
static void send_key_event(int keycode, int flag);

static inline void accdet_init(void);
static inline void clear_accdet_interrupt(void);
static inline void clear_accdet_eint_interrupt(int eint_id);
static inline int accdet_set_debounce(int state, unsigned int debounce);

/*
 * PART4: export functions
 */
/* get plug-in Resister just for audio call */
int accdet_read_audio_res(unsigned int res_value)
{
	ACCDET_INFO("[accdet_read_audio_res]R=%u(ohm)\n", res_value);
	/* res < 5k ohm normal device; res >= 5k ohm, lineout device */
	if (res_value < 5000)
		return RET_LT_5K;

	mutex_lock(&accdet_eint_irq_sync_mutex);
	if (s_eint_accdet_sync_flag == 1) {
		s_cable_type = LINE_OUT_DEVICE;
		s_accdet_status = LINE_OUT;
		/* update state */
		send_accdet_status_event(s_cable_type, 1);
		ACCDET_INFO("[accdet_read_audio_res]update state:%d\n", s_cable_type);
	}
	mutex_unlock(&accdet_eint_irq_sync_mutex);

	return RET_GT_5K;
}
EXPORT_SYMBOL(accdet_read_audio_res);

/*
 * PART5: static basic functions
 */
/*pmic wrap read and write func*/
static u32 pmic_pwrap_read(u32 addr)
{
	u32 val = 0;

	pwrap_read(addr, &val);
	return val;
}
static void pmic_pwrap_write(unsigned int addr, unsigned int wdata)
{
	pwrap_write(addr, wdata);
}

static u64 accdet_get_current_time(void)
{
	return sched_clock();
}

static bool accdet_timeout_ns(u64 start_time_ns, u64 timeout_time_ns)
{
	u64 cur_time = 0;
	u64 elapse_time = 0;

	/*get current tick*/
	cur_time = accdet_get_current_time();/* ns */
	if (cur_time < start_time_ns) {
		ACCDET_INFO("[accdet]start_time:%lld,cur_time:%lld\n", start_time_ns, cur_time);
		start_time_ns = cur_time;
		timeout_time_ns = 400 * 1000;/* 400us */
		ACCDET_INFO("[accdet]time: start:%lld,set:%lld\n", start_time_ns, timeout_time_ns);
	}
	elapse_time = cur_time - start_time_ns;

	/* check if timeout */
	if (timeout_time_ns <= elapse_time) {/* timeout */
		ACCDET_INFO("[accdet_timeout_ns]ACCDET IRQ clear Timeout\n");
		return false;
	}
	return true;
}

static int accdet_set_debounce(int state, unsigned int debounce)
{
	int ret = 0;

	switch (state) {
	case accdet_state000:/* set ACCDET debounce value = debounce/32 ms */
		pmic_pwrap_write((unsigned int)ACCDET_CON06, debounce);
		break;
	case accdet_state010:
		pmic_pwrap_write((unsigned int)ACCDET_CON07, debounce);
		break;
	case accdet_state100:
		pmic_pwrap_write((unsigned int)ACCDET_CON08, debounce);
		break;
	case accdet_state110:
		pmic_pwrap_write((unsigned int)ACCDET_CON09, debounce);
		break;
	case accdet_auxadc:/* set auxadc debounce:0x42(2ms) */
		pmic_pwrap_write((unsigned int)ACCDET_CON10, debounce);
		break;
	default:
		ACCDET_INFO("[accdet]error: no state:%d,deb:%d\n", state, debounce);
		break;
	}

/* ACCDET_INFO("[accdet_set_deb]state:%d,debounce:%d,ret:%d\n", state, debounce, ret); */
	return ret;
}

static int accdet_get_dts_data(void)
{
	struct device_node *node = NULL;
	int debounce[8] = { 0 };
	#ifdef CONFIG_FOUR_KEY_HEADSET
	int four_key[5] = { 0 };
	#else
	int three_key[4] = { 0 };
	#endif
	int ret = 0;

	ACCDET_INFO("[accdet_get_dts_data]Start..");
	node = of_find_matching_node(node, accdet_of_match);
	if (node) {
#ifdef CONFIG_MOISTURE_EXTERNAL_SUPPORT
		of_property_read_u32(node, "moisture-external-r", &external_r);
#endif
#ifdef CONFIG_MOISTURE_INTERNAL_SUPPORT
		of_property_read_u32(node, "moisture-internal-r", &internal_r);
#endif
#if defined(CONFIG_MOISTURE_EXTERNAL_SUPPORT) || defined(CONFIG_MOISTURE_INTERNAL_SUPPORT)
		of_property_read_u32(node, "moisture-water-r", &water_r);
#endif
		of_property_read_u32(node, "accdet-mic-vol", &headset_dts_data.mic_bias_vol);
		/* for GPIO debounce */
		of_property_read_u32(node, "accdet-plugout-debounce", &headset_dts_data.accdet_plugout_deb);
		of_property_read_u32(node, "accdet-mic-mode", &headset_dts_data.accdet_mic_mode);
		of_property_read_u32(node, "accdet-eint-level-pol", &headset_dts_data.eint_level_pol);
#ifdef CONFIG_FOUR_KEY_HEADSET
		ret = of_property_read_u32_array(node, "headset-four-key-threshold", four_key, ARRAY_SIZE(four_key));
		if (!ret)
			memcpy(&headset_dts_data.four_key, four_key+1, sizeof(struct four_key_threshold));
		ACCDET_INFO("[accdet_get_dts_data]mid-Key = %d, voice = %d, up_key = %d, down_key = %d\n",
		     headset_dts_data.four_key.mid_key_four, headset_dts_data.four_key.voice_key_four,
		     headset_dts_data.four_key.up_key_four, headset_dts_data.four_key.down_key_four);
#else
		#ifdef CONFIG_HEADSET_TRI_KEY_CDD
		ret = of_property_read_u32_array(node, "headset-three-key-threshold-CDD",
				three_key, ARRAY_SIZE(three_key));
		#else
		ret = of_property_read_u32_array(node, "headset-three-key-threshold",
				three_key, ARRAY_SIZE(three_key));
		#endif
		if (!ret)
			memcpy(&headset_dts_data.three_key, three_key+1, sizeof(struct three_key_threshold));
		ACCDET_INFO("[accdet_get_dts_data]mid-Key = %d, up_key = %d, down_key = %d\n",
		     headset_dts_data.three_key.mid_key, headset_dts_data.three_key.up_key,
		     headset_dts_data.three_key.down_key);
#endif
		ret = of_property_read_u32_array(node, "headset-mode-setting", debounce, ARRAY_SIZE(debounce));
		/* debounce8(auxadc debounce) is default, needn't get from dts */
		if (!ret)
			memcpy(&headset_dts_data.cfg_cust_accdet, debounce, sizeof(debounce));
		/* for discharge:0xB00 about 86ms */
		s_button_press_debounce = (headset_dts_data.cfg_cust_accdet.debounce0 >> 1);
		accdet_cust_setting = &headset_dts_data.cfg_cust_accdet;
		ACCDET_INFO("[accdet_get_dts_data]pwm_width=0x%x, pwm_thresh=0x%x, mic_mode=%d, mic_vol=%d\n",
		     accdet_cust_setting->pwm_width, accdet_cust_setting->pwm_thresh,
		     headset_dts_data.accdet_mic_mode, headset_dts_data.mic_bias_vol);
		ACCDET_INFO("[accdet_get_dts_data] deb0=0x%x,deb1=0x%x,deb3=0x%x,deb4=0x%x\n",
		     accdet_cust_setting->debounce0, accdet_cust_setting->debounce1,
		     accdet_cust_setting->debounce3, accdet_cust_setting->debounce4);
	} else {
		ACCDET_ERROR("%s can't find compatible dts node\n", __func__);
		return -1;
	}
	return 0;
}

/*
 * PART5.1: static check-key functions
 */
static void accdet_pmic_Read_Efuse_HPOffset(void)
{
	unsigned int efusevalue = 0;
#ifdef CONFIG_FOUR_KEY_HEADSET
	unsigned int tmp_val = 0;
	unsigned int tmp_8bit = 0;
#endif
#ifdef CONFIG_MOISTURE_INTERNAL_SUPPORT
	unsigned int moisture_eint0;
	unsigned int moisture_eint1;
#endif

#ifdef CONFIG_FOUR_KEY_HEADSET/* 4-key, 2.7V, internal bias for mt6337*/
	if (s_4_key_efuse_flag) {
		/* get 8bit from efuse rigister, so need extend to 12bit, shift right 2*/
		tmp_val = pmic_Read_Efuse_HPOffset(83);
		tmp_8bit = tmp_val & ACCDET_CALI_MASK0;
		headset_dts_data.four_key.mid_key_four = tmp_8bit << 2;/* AD */

		tmp_8bit = (tmp_val >> 8) & ACCDET_CALI_MASK0;
		headset_dts_data.four_key.voice_key_four = tmp_8bit << 2;/* DB */
		tmp_val = pmic_Read_Efuse_HPOffset(84);
		tmp_8bit = tmp_val & ACCDET_CALI_MASK0;
		headset_dts_data.four_key.up_key_four = tmp_8bit << 2;/* BC */
		headset_dts_data.four_key.down_key_four = 600;/* the max voltage */
		ACCDET_INFO("[accdet]key thresh: mid=%d mv,voice=%d mv,up=%d mv,down=%d mv\n",
			headset_dts_data.four_key.mid_key_four, headset_dts_data.four_key.voice_key_four,
			headset_dts_data.four_key.up_key_four, headset_dts_data.four_key.down_key_four);
	}
#endif
	/* accdet efuse offset */
	efusevalue = pmic_Read_Efuse_HPOffset(82);
	g_accdet_auxadc_offset = efusevalue & 0xFF;
	if (g_accdet_auxadc_offset > 128)
		g_accdet_auxadc_offset -= 256;
	g_accdet_auxadc_offset = (g_accdet_auxadc_offset >> 1);
	ACCDET_INFO(" efusevalue = 0x%x, accdet_auxadc_offset = %d\n", efusevalue, g_accdet_auxadc_offset);

#if defined(CONFIG_MOISTURE_EXTERNAL_SUPPORT) || defined(CONFIG_MOISTURE_INTERNAL_SUPPORT)
	/* moisture vdd efuse offset */
	efusevalue = pmic_Read_Efuse_HPOffset(85);
	g_moisture_vdd_offset = (int)((efusevalue >> 8) & ACCDET_CALI_MASK0);
	if (g_moisture_vdd_offset > 128)
		g_moisture_vdd_offset -= 256;
	ACCDET_INFO("[moisture_vdd_efuse]efuse=0x%x, moisture_vdd_offset=%d mv\n",
		efusevalue, g_moisture_vdd_offset);
	/* moisture efuse offset */
	efusevalue = pmic_Read_Efuse_HPOffset(86);
	g_moisture_offset = (int)(efusevalue & ACCDET_CALI_MASK0);
	if (g_moisture_offset > 128)
		g_moisture_offset -= 256;
	ACCDET_INFO("[moisture_efuse]efuse=0x%x,moisture_offset=%d mv\n",
		efusevalue, g_moisture_offset);
#endif
#ifdef CONFIG_MOISTURE_INTERNAL_SUPPORT
	/* moisture eint efuse offset */
	efusevalue = pmic_Read_Efuse_HPOffset(84);
	moisture_eint0 = (int)((efusevalue >> 8) & ACCDET_CALI_MASK0);
	ACCDET_INFO("[moisture_eint0]efuse=0x%x,moisture_eint0=0x%x\n",
		efusevalue, moisture_eint0);

	efusevalue = pmic_Read_Efuse_HPOffset(85);
	moisture_eint1 = (int)(efusevalue & ACCDET_CALI_MASK0);
	ACCDET_INFO("[moisture_eint1]efuse=0x%x,moisture_eint1=0x%x\n",
		efusevalue, moisture_eint1);

	g_moisture_eint_offset = (moisture_eint1 << 8) | moisture_eint0;
	if (g_moisture_eint_offset > 32768)
		g_moisture_eint_offset -= 65536;

	ACCDET_INFO("[moisture_eint_efuse]moisture_eint_offset=%d ohm\n", g_moisture_eint_offset);
	g_moisture_vm = (2800 + g_moisture_vdd_offset) * (water_r + internal_r) / ((water_r + internal_r)
		+ (8 * g_moisture_eint_offset) + 450000)
		+ g_moisture_offset / 2;
	ACCDET_INFO("[moisture_vm]moisture_vm=%d mv\n", g_moisture_vm);
#endif
#ifdef CONFIG_MOISTURE_EXTERNAL_SUPPORT
	g_moisture_vm = (2800 + g_moisture_vdd_offset) * water_r / (water_r + external_r) + g_moisture_offset / 2;
	ACCDET_INFO("[moisture_vm]moisture_vm=%d mv\n", g_moisture_vm);
#endif
}

static int Accdet_PMIC_IMM_GetOneChannelValue(int key_check)
{
	int vol_val = 0;

	/* use auxadc API */
	vol_val = pmic_get_auxadc_value(AUXADC_LIST_ACCDET);
	if (key_check) {
		if (vol_val > g_accdet_auxadc_offset)
			vol_val -= g_accdet_auxadc_offset;
		else
			vol_val = 0;
		ACCDET_DEBUG("[accdet] adc_offset=%d mv,MIC_Vol=%d mv\n", g_accdet_auxadc_offset, vol_val);
	}
	return vol_val;
}

#ifdef CONFIG_FOUR_KEY_HEADSET
static int key_check(int b)
{
	if ((b < headset_dts_data.four_key.down_key_four)
		&& (b >= headset_dts_data.four_key.up_key_four))
		return DW_KEY;/* function C: 360~680ohm */
	else if ((b < headset_dts_data.four_key.up_key_four)
		&& (b >= headset_dts_data.four_key.voice_key_four))
		return UP_KEY;/* function B: 210~290ohm */
	else if ((b < headset_dts_data.four_key.voice_key_four)
		&& (b >= headset_dts_data.four_key.mid_key_four))
		return AS_KEY;/* function D: 110~180ohm */
	else if (b < headset_dts_data.four_key.mid_key_four)
		return MD_KEY;/* function A: 70ohm less */

	ACCDET_INFO("[4-key_check] Invalid key:%d mv\n", b);
	return NO_KEY;
}
#else
static int key_check(int b)
{
	/* 0.24V ~  */
	/* ACCDET_DEBUG("[key_check] 3-keys enter:%d!!\n", b); */
	if ((b < headset_dts_data.three_key.down_key)
		&& (b >= headset_dts_data.three_key.up_key))
		return DW_KEY;
	else if ((b < headset_dts_data.three_key.up_key)
		&& (b >= headset_dts_data.three_key.mid_key))
		return UP_KEY;
	else if (b < headset_dts_data.three_key.mid_key)
		return MD_KEY;

	ACCDET_INFO("[3-key_check] Invalid key:%d mv\n", b);
	return NO_KEY;
}
#endif

static void send_accdet_status_event(int cable, int status)
{
	switch (cable) {
	case HEADSET_NO_MIC:
		input_report_switch(kpd_accdet_dev, SW_HEADPHONE_INSERT, status);
		/* when plug 4-pole out, if both AB=3 AB=0 happen,3-pole plug in will be incorrectly reported,
		* then 3-pole plug-out is reported, if no mantory 4-pole plug-out,icon would be visible.
		*/
		if (status == 0)
			input_report_switch(kpd_accdet_dev, SW_MICROPHONE_INSERT, status);

		input_report_switch(kpd_accdet_dev, SW_JACK_PHYSICAL_INSERT, status);
		input_sync(kpd_accdet_dev);
		ACCDET_DEBUG("HEADPHONE(3-pole) %s\n", status?"PlugIn":"PlugOut");
		break;
	case HEADSET_MIC:
		/* when plug 4-pole out, 3-pole plug out should also be reported for slow plug-in case */
		if (status == 0)
			input_report_switch(kpd_accdet_dev, SW_HEADPHONE_INSERT, status);
		input_report_switch(kpd_accdet_dev, SW_MICROPHONE_INSERT, status);
		input_report_switch(kpd_accdet_dev, SW_JACK_PHYSICAL_INSERT, status);
		input_sync(kpd_accdet_dev);
		ACCDET_DEBUG("MICROPHONE(4-pole) %s\n", status?"PlugIn":"PlugOut");
		break;
	case LINE_OUT_DEVICE:
		input_report_switch(kpd_accdet_dev, SW_LINEOUT_INSERT, status);
		input_sync(kpd_accdet_dev);
		ACCDET_DEBUG("LineOut %s\n", status?"PlugIn":"PlugOut");
		break;
	default:
		ACCDET_DEBUG("Invalid cable type\n");
	}
}

static void send_key_event(int keycode, int flag)
{
	switch (keycode) {
	case DW_KEY:
		input_report_key(kpd_accdet_dev, KEY_VOLUMEDOWN, flag);
		input_sync(kpd_accdet_dev);
		ACCDET_DEBUG("[accdet][send_key_event]KEY_VOLUMEDOWN %d\n", flag);
		break;
	case UP_KEY:
		input_report_key(kpd_accdet_dev, KEY_VOLUMEUP, flag);
		input_sync(kpd_accdet_dev);
		ACCDET_DEBUG("[accdet][send_key_event]KEY_VOLUMEUP %d\n", flag);
		break;
	case MD_KEY:
		input_report_key(kpd_accdet_dev, KEY_PLAYPAUSE, flag);
		input_sync(kpd_accdet_dev);
		ACCDET_DEBUG("[accdet][send_key_event]KEY_PLAYPAUSE %d\n", flag);
		break;
	case AS_KEY:
		input_report_key(kpd_accdet_dev, KEY_VOICECOMMAND, flag);
		input_sync(kpd_accdet_dev);
		ACCDET_DEBUG("[accdet][send_key_event]KEY_VOICECOMMAND %d\n", flag);
		break;
	}
}

static void multi_key_detection(int current_status)
{
	int m_key = 0;
	unsigned int reg_val = 0;
	int cali_voltage = 0;

	if (current_status == ACCDET_STATE_ABC_00) {
		cali_voltage = Accdet_PMIC_IMM_GetOneChannelValue(1);
		ACCDET_DEBUG("[Accdet]adc cali_voltage1 = %d mv\n", cali_voltage);
		m_key = g_cur_key = key_check(cali_voltage);
	}
	mdelay(10);/* maybe delay 10ms, not 30ms */

	reg_val = pmic_pwrap_read(ACCDET_CON12);
#ifdef CONFIG_ACCDET_EINT_IRQ
	if (((reg_val & ACCDET_EINT_IRQ_B2_B3) != ACCDET_EINT_IRQ_B2_B3)
		|| s_eint_accdet_sync_flag) {
		send_key_event(g_cur_key, !current_status);
	} else {
		ACCDET_ERROR("[accdet]plug out side effect,not report key=%d\n", g_cur_key);
		g_cur_key = NO_KEY;
	}
#endif

#ifdef CONFIG_ACCDET_EINT
	if (((reg_val & ACCDET_IRQ_STS_BIT_ALL) != ACCDET_IRQ_STS_BIT_ALL)
		|| s_eint_accdet_sync_flag) {
		send_key_event(g_cur_key, !current_status);
	} else {
		ACCDET_ERROR("[accdet]plug out side effect,not report key = %d\n", g_cur_key);
		g_cur_key = NO_KEY;
	}
#endif

	if (current_status)
		g_cur_key = NO_KEY;
}

static inline void check_cable_type(void)
{
	int current_status = 0;
	int tmp_ABC = 0;
	unsigned int reg_val = 0;

	reg_val = pmic_pwrap_read(ACCDET_CON14);
	/*A=bit2; B=bit1;C=bit0*/
	current_status = ((reg_val>>ACCDET_STATE_MEM_BIT_OFFSET)&ACCDET_STATE_ABC_MASK);
	tmp_ABC = current_status;/* have ignored C  already */
	ACCDET_DEBUG("[accdet]addr:[0x%x]=0x%x, cur_ABC=0x%x\n", ACCDET_CON14, reg_val, current_status);

	/* just get AB state, ignore C */
	/* current_status = (current_status & 0x06); */

	s_button_status = 0;
	s_pre_status = s_accdet_status;

	switch (s_accdet_status) {/* accdet FSM state entry */
	case PLUG_OUT:
		if (current_status == ACCDET_STATE_ABC_00) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (s_eint_accdet_sync_flag == 1) {
				s_cable_type = HEADSET_NO_MIC;/* 3-poles */
				s_accdet_status = HOOK_SWITCH;
			} else {
				ACCDET_ERROR("[accdet]1.Headset has plugged out(000)\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
		} else if (current_status == ACCDET_STATE_ABC_01) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (s_eint_accdet_sync_flag == 1) {
				s_accdet_status = MIC_BIAS;/* 4 poles */
				s_cable_type = HEADSET_MIC;
				/*ABC=110 debounce=30ms*/
				accdet_set_debounce(accdet_state110, accdet_cust_setting->debounce3 * 30);
			} else {
				ACCDET_ERROR("[accdet]1.Headset has plugged out(010)\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
			accdet_set_debounce(accdet_state000, s_button_press_debounce);
		} else if (current_status == ACCDET_STATE_ABC_11) {
			ACCDET_DEBUG("[accdet]1.PLUG_OUT state don't change!\n");
#ifdef CONFIG_ACCDET_EINT
			ACCDET_DEBUG("[accdet]1.no send plug out event in plug out\n");
#else
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (s_eint_accdet_sync_flag == 1) {
				s_accdet_status = PLUG_OUT;
				s_cable_type = NO_DEVICE;
			} else {
				ACCDET_ERROR("[accdet]1.Headset has plugged out(110)\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
#endif
		} else {
			ACCDET_ERROR("[accdet]1.PLUGOUT undefined state ABC=%d!\n", current_status);
		}
		break;
	case MIC_BIAS:
		/* pmic_pwrap_write(ACCDET_CON06, accdet_cust_setting->debounce0); */
		if (current_status == ACCDET_STATE_ABC_00) {
			/*solution: resume hook switch debounce time*/
			accdet_set_debounce(accdet_state000, accdet_cust_setting->debounce0);
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (s_eint_accdet_sync_flag == 1)
				s_accdet_status = HOOK_SWITCH;
			else
				ACCDET_ERROR("[accdet]2.1 Headset has plugged out(000)\n");
			mutex_unlock(&accdet_eint_irq_sync_mutex);
			s_button_status = 1;
			if (s_button_status) {
				mutex_lock(&accdet_eint_irq_sync_mutex);
				if (s_eint_accdet_sync_flag == 1)
					multi_key_detection(current_status);
				else
					ACCDET_INFO("[accdet]2.2 Headset has plugged out (000)\n");
				mutex_unlock(&accdet_eint_irq_sync_mutex);
				ACCDET_DEBUG("[accdet] switch to HOOK_SWITCH\n");
				/* recover  pwm frequency and duty */
				pmic_pwrap_write(ACCDET_CON04, REGISTER_VALUE(accdet_cust_setting->pwm_thresh));
				pmic_pwrap_write(ACCDET_CON03, REGISTER_VALUE(accdet_cust_setting->pwm_width));
			}
		} else if (current_status == ACCDET_STATE_ABC_01) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (s_eint_accdet_sync_flag == 1) {
				s_accdet_status = MIC_BIAS;
				s_cable_type = HEADSET_MIC;
				ACCDET_DEBUG("[accdet]2.MIC_BIAS state not change!\n");
			} else {
				ACCDET_INFO("[accdet]2.Headset has plugged out(010)\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);
		} else if (current_status == ACCDET_STATE_ABC_11) {
			ACCDET_DEBUG("[accdet]2.do not send plug out in micbias\n");
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (s_eint_accdet_sync_flag == 1)
				s_accdet_status = PLUG_OUT;
			else
				ACCDET_ERROR("[accdet]2.Headset has plugged out(110)\n");
			mutex_unlock(&accdet_eint_irq_sync_mutex);
		} else {
			ACCDET_ERROR("[accdet]2.MIC_BIAS undefined state ABC=%d\n", current_status);
		}
		break;
	case HOOK_SWITCH:
		if (current_status == ACCDET_STATE_ABC_00) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (s_eint_accdet_sync_flag == 1)
				/* for avoid 01->00 framework of Headset will report press key info for Audio */
				/* s_cable_type = HEADSET_NO_MIC; */
				/* s_accdet_status = HOOK_SWITCH; */
				ACCDET_DEBUG("[accdet]3.HOOK_SWITCH state not change!\n");
			else
				ACCDET_INFO("[accdet]3.Headset has plugged out(000)\n");
			mutex_unlock(&accdet_eint_irq_sync_mutex);
		} else if (current_status == ACCDET_STATE_ABC_01) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (s_eint_accdet_sync_flag == 1) {
				multi_key_detection(current_status);
				s_accdet_status = MIC_BIAS;/* 4 poles */
				s_cable_type = HEADSET_MIC;
				ACCDET_DEBUG("[accdet]3.switch to MIC_BIAS\n");
			} else {
				ACCDET_INFO("[accdet]3.Headset has plugged out(010)\n");
			}
			mutex_unlock(&accdet_eint_irq_sync_mutex);

			/* solution: reduce hook switch debounce time to half */
			accdet_set_debounce(accdet_state000, s_button_press_debounce);
			/* pmic_pwrap_write(ACCDET_CON06, s_button_press_debounce); */
		} else if (current_status == ACCDET_STATE_ABC_11) {
			ACCDET_DEBUG("[accdet]3.do not send plug out event in hook switch\n");
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (s_eint_accdet_sync_flag == 1)
				s_accdet_status = PLUG_OUT;
			else
				ACCDET_INFO("[accdet]3.Headset has plugged out(110)\n");
			mutex_unlock(&accdet_eint_irq_sync_mutex);
		} else {
			ACCDET_ERROR("[accdet]3.HOOK-SWITCH undefined state ABC=%d!\n", current_status);
		}
		break;
	case STAND_BY:
		if (current_status == ACCDET_STATE_ABC_11)
			ACCDET_INFO("[accdet]4.accdet no send plug out event in stand by!\n");
		else
			ACCDET_ERROR("[accdet]4.STAND-BY undefined state ABC=%d!\n", current_status);
		break;
	default:
		ACCDET_ERROR("[accdet]5.accdet current status error!\n");
		break;
	}


	ACCDET_DEBUG("[accdet]7.cable type:[%s], status switch:[%s]->[%s]\n",
		     accdet_report_string[s_cable_type], accdet_status_string[s_pre_status],
		     accdet_status_string[s_accdet_status]);
}

/*
 * PART5.2: static headset  functions
 */
static inline void headset_plug_out(void)
{
	send_accdet_status_event(s_cable_type, 0);
	s_accdet_status = PLUG_OUT;
	s_cable_type = NO_DEVICE;
	/* update the cable_type */
	if (g_cur_key != 0) {
		send_key_event(g_cur_key, 0);
		ACCDET_INFO("[accdet]plug_out send key = %d release\n", g_cur_key);
		g_cur_key = 0;
	}
	ACCDET_DEBUG("[accdet]set state in cable_type = NO_DEVICE\n");
}

/* Accdet only need this func */
static inline void enable_accdet(u32 state_swctrl)
{
	/* ACCDET_DEBUG("[accdet][enable_accdet]enter..\n"); */
	/* enable clock */
	/* pmic_pwrap_write(TOP_CKPDN_CLR, RG_ACCDET_CLK_CLR); */
	/* Enable PWM */
	pmic_pwrap_write(ACCDET_CON02, pmic_pwrap_read(ACCDET_CON02) | state_swctrl | ACCDET_SWCTRL_ACCDET_EN);
	/* enable ACCDET unit */
#ifndef HW_MODE_SUPPORT
	pmic_pwrap_write(ACCDET_CON01, pmic_pwrap_read(ACCDET_CON01) | ACCDET_ENABLE_B0);
#endif
}

/* clear ACCDET IRQ in accdet register */
static inline void clear_accdet_interrupt(void)
{
	/* it is safe by using polling to adjust when to clear IRQ_CLR_BIT */
	pmic_pwrap_write(ACCDET_CON12, pmic_pwrap_read(ACCDET_CON12) | ACCDET_IRQ_CLR_B8);
	ACCDET_DEBUG("[clear_accdet_interrupt][0x%x]=0x%x\n", ACCDET_CON12, pmic_pwrap_read(ACCDET_CON12));
}

static inline void disable_accdet(void)
{
	int irq_temp = 0;
	unsigned int reg_val = 0;

/* sync with accdet_irq_handler set clear accdet irq bit to avoid */
/* set clear accdet irq bit after disable accdet disable accdet irq */
	/* pmic_pwrap_write(INT_CON_ACCDET_CLR, RG_INT_CLR_ACCDET); //for fix icon disappear HW */
	clear_accdet_interrupt();
	udelay(200);

	mutex_lock(&accdet_eint_irq_sync_mutex);
	reg_val = pmic_pwrap_read(ACCDET_CON12);
	while (reg_val & ACCDET_IRQ_B0) {
		reg_val = pmic_pwrap_read(ACCDET_CON12);
		ACCDET_DEBUG("[disable_accdet]Clear interrupt on-going..\n");
		msleep(20);
	}
	irq_temp = reg_val;
	irq_temp = irq_temp & (~ACCDET_IRQ_CLR_B8);
	pmic_pwrap_write(ACCDET_CON12, irq_temp);
	mutex_unlock(&accdet_eint_irq_sync_mutex);

#ifdef CONFIG_ACCDET_EINT_IRQ
#if 0
	#ifdef CONFIG_ACCDET_SUPPORT_EINT0
		pmic_pwrap_write(ACCDET_CON02, ACCDET_EINT0_PWM_EN|ACCDET_EINT0_PWM_IDLE);
	#elif defined CONFIG_ACCDET_SUPPORT_EINT1
		pmic_pwrap_write(ACCDET_CON02, ACCDET_EINT1_PWM_EN|ACCDET_EINT1_PWM_IDLE);
	#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
		pmic_pwrap_write(ACCDET_CON02, ACCDET_EINT0_PWM_EN|ACCDET_EINT1_PWM_EN|
			ACCDET_EINT1_PWM_IDLE|ACCDET_EINT0_PWM_IDLE);
	#endif
#endif
#endif
#ifndef HW_MODE_SUPPORT
	pmic_pwrap_write(ACCDET_CON01, pmic_pwrap_read(ACCDET_CON01) & (~(ACCDET_ENABLE_B0)));
#endif
	pmic_pwrap_write(ACCDET_CON02, pmic_pwrap_read(ACCDET_CON02)&(~ACCDET_SWCTRL_ACCDET_EN));/* clear clk */
	ACCDET_DEBUG("[disable_accdet][0x%x]=0x%x\n", ACCDET_CON12, pmic_pwrap_read(ACCDET_CON12));
}

static inline void clear_accdet_eint_interrupt(int eint_id)
{
#ifdef CONFIG_ACCDET_EINT_IRQ
	unsigned int reg_val = 0;

	reg_val = pmic_pwrap_read(ACCDET_CON12);
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
	pmic_pwrap_write(ACCDET_CON12, reg_val | ACCDET_EINT0_IRQ_CLR_B10);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
	pmic_pwrap_write(ACCDET_CON12, reg_val | ACCDET_EINT1_IRQ_CLR_B11);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
	if (eint_id == ACCDET_EINT0_IRQ_IN)
		pmic_pwrap_write(ACCDET_CON12, reg_val | ACCDET_EINT0_IRQ_CLR_B10);
	else if (eint_id == ACCDET_EINT1_IRQ_IN)
		pmic_pwrap_write(ACCDET_CON12, reg_val | ACCDET_EINT1_IRQ_CLR_B11);
	else {
		pmic_pwrap_write(ACCDET_CON12, reg_val | ACCDET_EINT_IRQ_CLR_B10_11);/* abnormal */
		ACCDET_ERROR("[clear_accdet_eint_interrupt]error:eint_id=0x%x\n", eint_id);
	}
#endif
	ACCDET_DEBUG("[clear_accdet_eint_interrupt][0x%x]=0x%x\n",
		ACCDET_CON12, pmic_pwrap_read(ACCDET_CON12));
#else
	ACCDET_INFO("[clear_accdet_eint_interrupt]CONFIG_ACCDET_EINT no support the API\n");
#endif
}

/*
 * PART5.3: static  workqueue callback functions
 */
static void disable_micbias_callback(struct work_struct *work)
{
	unsigned int reg_val = 0;

	if (s_cable_type == HEADSET_NO_MIC) {
		reg_val = pmic_pwrap_read(ACCDET_CON02);
#ifdef CONFIG_HEADSET_SUPPORT_FIVE_POLE
		/*setting pwm idle, disable all;*/
		pmic_pwrap_write(ACCDET_CON02, reg_val & (~ACCDET_PWM_IDLE_B8_9_10));
#else
		/* set PWM IDLE on */
		pmic_pwrap_write(ACCDET_CON02, reg_val|(~ACCDET_PWM_IDLE_B8_9_10));
#endif
		disable_accdet();
		ACCDET_DEBUG("[disable_micbias_callback]more than 5s MICBIAS, disable micbias\n");
	}
}

static void accdet_eint_work_callback(struct work_struct *work)
{
	unsigned int reg_val = 0;

#ifdef CONFIG_ACCDET_EINT_IRQ
	int irq_temp = 0;

	if (g_cur_eint_state == EINT_PIN_PLUG_IN) {/* EINT_PIN_PLUG_IN */
		ACCDET_DEBUG("[accdet]DCC plug-in, g_cur_eint_state = %d\n", g_cur_eint_state);
		mutex_lock(&accdet_eint_irq_sync_mutex);
		s_eint_accdet_sync_flag = 1;
		mutex_unlock(&accdet_eint_irq_sync_mutex);
		wake_lock_timeout(&accdet_timer_lock, 7 * HZ);
		accdet_init();	/* do set pwm_idle on in accdet_init */

#if 0
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
	pmic_pwrap_write(ACCDET_CON02,
		(pmic_pwrap_read(ACCDET_CON02)|ACCDET_PWM_IDLE_0));
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
	pmic_pwrap_write(ACCDET_CON02,
		(pmic_pwrap_read(ACCDET_CON02)|ACCDET_PWM_IDLE_1));
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
	pmic_pwrap_write(ACCDET_CON02,
		(pmic_pwrap_read(ACCDET_CON02)|ACCDET_PWM_IDLE_0|ACCDET_PWM_IDLE_1);
#endif
#endif
		reg_val = pmic_pwrap_read(ACCDET_CON02);
		/* set PWM IDLE on */
		pmic_pwrap_write(ACCDET_CON02, reg_val|ACCDET_PWM_IDLE_B8_9_10);
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
		enable_accdet(ACCDET_EINT0_PWM_IDLE_B11);/* enable ACCDET EINT0 unit */
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
		enable_accdet(ACCDET_EINT1_PWM_IDLE_B12);/* enable ACCDET EINT1 unit */
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT/* EINT0 & EINT1 */
		enable_accdet(ACCDET_EINT_PWM_IDLE_B11_12);/* enable ACCDET unit */
#endif
	} else {/* EINT_PIN_PLUG_OUT */
/* Disable ACCDET */
		ACCDET_DEBUG("[accdet]DCC EINT:plug-out, cur_eint_state=%d\n", g_cur_eint_state);
		mutex_lock(&accdet_eint_irq_sync_mutex);
		s_eint_accdet_sync_flag = 0;
		mutex_unlock(&accdet_eint_irq_sync_mutex);
		del_timer_sync(&micbias_timer);

#ifdef CONFIG_ACCDET_EINT_IRQ
		/* pwrap_write(ACCDET_CON24, pmic_pwrap_read(ACCDET_CON24)&(~0x1F)); */
#endif
		reg_val = pmic_pwrap_read(ACCDET_CON02);
		pmic_pwrap_write(ACCDET_CON02,
			(reg_val&(~ACCDET_PWM_IDLE_B8_9_10)));

		disable_accdet();

		headset_plug_out();
		/* recover EINT irq clear bit  */
		/* TODO: need think~~~ */
		irq_temp = pmic_pwrap_read(ACCDET_CON12);
		irq_temp = irq_temp & (~ACCDET_EINT_IRQ_CLR_B10_11);/* need think??? */
		pmic_pwrap_write(ACCDET_CON12, irq_temp);
		ACCDET_DEBUG("[accdet]plug-out,[0x%x]=0x%x\n", ACCDET_CON12, pmic_pwrap_read(ACCDET_CON12));
	}
#endif

#ifdef CONFIG_ACCDET_EINT
	/* KE under fastly plug in and plug out */
	if (g_cur_eint_state == EINT_PIN_PLUG_IN) {
		ACCDET_DEBUG("[accdet]ACC plug-in, g_cur_eint_state = %d\n", g_cur_eint_state);
		mutex_lock(&accdet_eint_irq_sync_mutex);
		s_eint_accdet_sync_flag = 1;
		mutex_unlock(&accdet_eint_irq_sync_mutex);
		wake_lock_timeout(&accdet_timer_lock, 7 * HZ);

		accdet_init();	/* do set pwm_idle on in accdet_init*/
		reg_val = pmic_pwrap_read(ACCDET_CON02);
		/* set PWM IDLE on */
		pmic_pwrap_write(ACCDET_CON02,
			(reg_val|ACCDET_PWM_IDLE_B8_9_10));
		/*enable ACCDET unit*/
		enable_accdet(ACCDET_PWM_IDLE_B8_9_10);
	} else {/* EINT_PIN_PLUG_OUT */
/* Disable ACCDET */
		ACCDET_DEBUG("[accdet]ACC plug-out, g_cur_eint_state = %d\n", g_cur_eint_state);
		mutex_lock(&accdet_eint_irq_sync_mutex);
		s_eint_accdet_sync_flag = 0;
		mutex_unlock(&accdet_eint_irq_sync_mutex);
		del_timer_sync(&micbias_timer);

		/* need close idle, otherwise will make leakage */
		reg_val = pmic_pwrap_read(ACCDET_CON02);
		pmic_pwrap_write(ACCDET_CON02,
				(reg_val&(~ACCDET_PWM_IDLE_B8_9_10)));

		/* accdet_auxadc_switch(0); */
		disable_accdet();
		headset_plug_out();
	}
	enable_irq(g_accdet_irq);
#endif
}

static void disable_micbias(unsigned long a)
{
	int ret = 0;

	ret = queue_work(accdet_disable_workqueue, &accdet_disable_work);
	if (!ret)
		ACCDET_ERROR("[accdet][disable_micbias]:accdet_work return:%d\n", ret);
}

#ifdef CONFIG_ACCDET_EINT_IRQ
static int accdet_eint_func(int eint_id)
{
	int ret = 0;
	unsigned int reg_val = 0;

	reg_val = 0;
	ACCDET_DEBUG("[accdet_eint_func]Enter.. eint:0x%x\n", eint_id);

#ifdef CONFIG_ACCDET_SUPPORT_EINT0
	reg_val = pmic_pwrap_read(ACCDET_CON15);
	if (eint_id == ACCDET_EINT0_IRQ_IN) {
		if (g_cur_eint_state == EINT_PIN_PLUG_IN) {
			pmic_pwrap_write(ACCDET_CON15, reg_val & (~ACCDET_EINT0_DEB_CLR));
			/* eint debounce=256ms */
			reg_val = pmic_pwrap_read(ACCDET_CON15);
			pmic_pwrap_write(ACCDET_CON15, reg_val|ACCDET_EINT0_DEB_IN_256);
			accdet_set_debounce(accdet_state110, accdet_cust_setting->debounce3);
			/* pmic_pwrap_write(ACCDET_CON09, accdet_cust_setting->debounce3); */

			/* update the eint status */
			g_cur_eint_state = EINT_PIN_PLUG_OUT;
		} else {
			pmic_pwrap_write(ACCDET_CON15, reg_val & (~ACCDET_EINT0_DEB_CLR));
			/* eint debounce=0.12ms */
			reg_val = pmic_pwrap_read(ACCDET_CON15);
			pmic_pwrap_write(ACCDET_CON15, reg_val|ACCDET_EINT0_DEB_OUT_012);

			/* update the eint status */
			g_cur_eint_state = EINT_PIN_PLUG_IN;
			mod_timer(&micbias_timer, jiffies + MICBIAS_DISABLE_TIMER);
		}
		ret = queue_work(accdet_eint_workqueue, &accdet_eint_work);
	} else {
		ACCDET_ERROR("[accdet_eint_func]eint_id is not ACCDET_EINT0_IRQ_IN!\n");
	}
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
	reg_val = pmic_pwrap_read(ACCDET_CON25);
	if (eint_id == ACCDET_EINT1_IRQ_IN) {
		if (g_cur_eint_state == EINT_PIN_PLUG_IN) {
			pmic_pwrap_write(ACCDET_CON25, reg_val & (~ACCDET_EINT1_DEB_CLR));
			/* eint debounce=256ms */
			reg_val = pmic_pwrap_read(ACCDET_CON25);
			pmic_pwrap_write(ACCDET_CON25, reg_val|ACCDET_EINT1_DEB_IN_256);
			accdet_set_debounce(accdet_state110, accdet_cust_setting->debounce3);
			/* pmic_pwrap_write(ACCDET_CON09, accdet_cust_setting->debounce3); */

			/* update the eint status */
			g_cur_eint_state = EINT_PIN_PLUG_OUT;
		} else {
			pmic_pwrap_write(ACCDET_CON25, reg_val & (~ACCDET_EINT1_DEB_CLR));
			/* eint debounce=0.12ms */
			reg_val = pmic_pwrap_read(ACCDET_CON25);
			pmic_pwrap_write(ACCDET_CON25, reg_val|ACCDET_EINT1_DEB_OUT_012);

			/* update the eint status */
			g_cur_eint_state = EINT_PIN_PLUG_IN;
			mod_timer(&micbias_timer, jiffies + MICBIAS_DISABLE_TIMER);
		}
		ret = queue_work(accdet_eint_workqueue, &accdet_eint_work);
	} else {
		ACCDET_ERROR("[accdet][accdet_eint_func]eint_id is not ACCDET_EINT1_IRQ_IN!\n");
	}
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
	if (eint_id == ACCDET_EINT0_IRQ_IN) {
		reg_val = pmic_pwrap_read(ACCDET_CON15);
		if (g_cur_eint0_state == EINT_PIN_PLUG_IN) {
			pmic_pwrap_write(ACCDET_CON15, reg_val & (~ACCDET_EINT0_DEB_CLR));
			/* debounce=256ms */
			reg_val = pmic_pwrap_read(ACCDET_CON15);
			pmic_pwrap_write(ACCDET_CON15, reg_val|ACCDET_EINT0_DEB_IN_256);
			accdet_set_debounce(accdet_state110, accdet_cust_setting->debounce3);
			/* pmic_pwrap_write(ACCDET_CON09, accdet_cust_setting->debounce3); */

			/* update the eint0 status */
			g_cur_eint0_state = EINT_PIN_PLUG_OUT;
		} else {
			pmic_pwrap_write(ACCDET_CON15, pmic_pwrap_read(ACCDET_CON15) & (~ACCDET_EINT0_DEB_CLR));
			/* eint debounce=0.12ms */
			reg_val = pmic_pwrap_read(ACCDET_CON15);
			pmic_pwrap_write(ACCDET_CON15, pmic_pwrap_read(ACCDET_CON15)
				| ACCDET_EINT0_DEB_OUT_012);

			/* update the eint0 status */
			g_cur_eint0_state = EINT_PIN_PLUG_IN;
			/* mod_timer(&micbias_timer, jiffies + MICBIAS_DISABLE_TIMER); */
		}
		/* maybe need judge the bit3 is clear actually??? */
		reg_val = pmic_pwrap_read(ACCDET_CON12);
		pmic_pwrap_write(ACCDET_CON12, reg_val & (~ACCDET_EINT0_IRQ_CLR_B10));
	} else if (eint_id == ACCDET_EINT1_IRQ_IN) {
		reg_val = pmic_pwrap_read(ACCDET_CON25);
		if (g_cur_eint1_state == EINT_PIN_PLUG_IN) {
			pmic_pwrap_write(ACCDET_CON25, reg_val & (~ACCDET_EINT1_DEB_CLR));
			/* debounce=256ms */
			reg_val = pmic_pwrap_read(ACCDET_CON25);
			pmic_pwrap_write(ACCDET_CON25, reg_val|ACCDET_EINT1_DEB_IN_256);
			accdet_set_debounce(accdet_state110, accdet_cust_setting->debounce3);
			/* pmic_pwrap_write(ACCDET_CON09, accdet_cust_setting->debounce3); */

			/* update the eint1 status */
			g_cur_eint1_state = EINT_PIN_PLUG_OUT;
		} else {
			pmic_pwrap_write(ACCDET_CON25, reg_val & (~ACCDET_EINT1_DEB_CLR));
			/* eint debounce=0.12ms */
			reg_val = pmic_pwrap_read(ACCDET_CON25);
			pmic_pwrap_write(ACCDET_CON25, reg_val|ACCDET_EINT1_DEB_OUT_012);

			/* update the eint1 status */
			g_cur_eint1_state = EINT_PIN_PLUG_IN;
			/* mod_timer(&micbias_timer, jiffies + MICBIAS_DISABLE_TIMER); */
		}
		/* maybe need judge the bit3 is clear actually??? */
		reg_val = pmic_pwrap_read(ACCDET_CON12);
		pmic_pwrap_write(ACCDET_CON12, reg_val & (~ACCDET_EINT1_IRQ_CLR_B11));
	} else {
		ACCDET_ERROR("[accdet_eint_func]eint_id is err!\n");
	}

	/* bi_eint trigger issued current state, may */
	if (g_cur_eint_state == EINT_PIN_PLUG_OUT) {
		g_cur_eint_state = (g_cur_eint0_state&g_cur_eint1_state);
		if (g_cur_eint_state == EINT_PIN_PLUG_IN) {
			mod_timer(&micbias_timer, jiffies + MICBIAS_DISABLE_TIMER);
			ret = queue_work(accdet_eint_workqueue, &accdet_eint_work);
		} else
			ACCDET_INFO("[accdet_eint_func]wait state0:eint0=%d;eint1=%d\n",
				g_cur_eint0_state, g_cur_eint1_state);
	} else if (g_cur_eint_state == EINT_PIN_PLUG_IN) {
		if ((g_cur_eint0_state|g_cur_eint1_state) == EINT_PIN_PLUG_OUT) {
			pmic_pwrap_write(ACCDET_CON12,
				pmic_pwrap_read(ACCDET_CON12) & (~ACCDET_EINT_IRQ_CLR_B10_11));
		} else if ((g_cur_eint0_state&g_cur_eint1_state) == EINT_PIN_PLUG_OUT) {
			g_cur_eint_state = (g_cur_eint0_state&g_cur_eint1_state);
			ret = queue_work(accdet_eint_workqueue, &accdet_eint_work);
		} else {
			ACCDET_INFO("[accdet_eint_func]wait state1:eint0=%d;eint1=%d\n",
				g_cur_eint0_state, g_cur_eint1_state);
		}
	} else {
		ACCDET_ERROR("[accdet_eint_func]g_cur_eint_state err!\n");
	}
#endif
	/* ACCDET_DEBUG("[accdet_eint_func]end, cur_eint_state= %d\n", g_cur_eint_state); */
	return ret;
}
#endif

static void accdet_work_callback(struct work_struct *work)
{
	unsigned int reg_val = 0;

	reg_val = 0;
	wake_lock(&accdet_irq_lock);

	check_cable_type();

	mutex_lock(&accdet_eint_irq_sync_mutex);
	if (s_eint_accdet_sync_flag == 1)
		send_accdet_status_event(s_cable_type, 1);
	else
		ACCDET_ERROR("[accdet]Headset has plugged out don't set accdet state\n");
	mutex_unlock(&accdet_eint_irq_sync_mutex);
	wake_unlock(&accdet_irq_lock);
#ifdef DIGITAL_FASTDISCHARGE_SUPPORT
	/* workround for HW fast discharge */
	if ((!fast_discharge) && (s_cable_type == MIC_BIAS)) {
		pmic_pwrap_write(ACCDET_CON24, ACCDET_FAST_DISCAHRGE_REVISE);
		/* if AB = 01, enabel fast discharge */
		udelay(2000);
		pmic_pwrap_write(ACCDET_CON24, ACCDET_FAST_DISCAHRGE_EN);
		fast_discharge = true;
	}
#endif
	ACCDET_DEBUG("[accdet] report cable_type:%d\n", s_cable_type);
}

static void accdet_workqueue_func(void)
{
	int ret = 0;

	ret = queue_work(accdet_workqueue, &accdet_work);
	if (!ret)
		ACCDET_ERROR("[accdet_workqueue_func]failed return:%d!\n", ret);
}

/*
 * PART5.4: static irq handler functions
 */
#ifdef CONFIG_ACCDET_EINT
static irqreturn_t accdet_ap_eint_func(int irq, void *data)
{
	int ret = 0;

	ACCDET_DEBUG("[accdet_ap_eint_func]Enter!\n");
	if (g_cur_eint_state == EINT_PIN_PLUG_IN) {
		/* the headset was plugged in set the polarity back as initialed */
		if (g_accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
			irq_set_irq_type(g_accdet_irq, IRQ_TYPE_LEVEL_HIGH);
		else
			irq_set_irq_type(g_accdet_irq, IRQ_TYPE_LEVEL_LOW);

		gpio_set_debounce(g_gpio_pin, g_gpio_headset_deb);

		/* update the eint status */
		g_cur_eint_state = EINT_PIN_PLUG_OUT;
	} else {
		/* the headset was plugged out set the opposite polarity to what initialed */
		if (g_accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
			irq_set_irq_type(g_accdet_irq, IRQ_TYPE_LEVEL_LOW);
		else
			irq_set_irq_type(g_accdet_irq, IRQ_TYPE_LEVEL_HIGH);

		gpio_set_debounce(g_gpio_pin, headset_dts_data.accdet_plugout_deb * 1000);

		/* update the eint status */
		g_cur_eint_state = EINT_PIN_PLUG_IN;

		mod_timer(&micbias_timer, jiffies + MICBIAS_DISABLE_TIMER);
	}

	disable_irq_nosync(g_accdet_irq);
	ACCDET_DEBUG("[accdet_ap_eint_func]end,g_cur_eint_state=%d\n", g_cur_eint_state);

	ret = queue_work(accdet_eint_workqueue, &accdet_eint_work);
	return IRQ_HANDLED;
}

static inline int accdet_setup_eint(struct platform_device *accdet_device)
{
	int ret = 0;
	u32 ints1[4] = { 0 };
	struct device_node *node = NULL;
	struct pinctrl_state *pins_default = NULL;

	/* configure to GPIO function, external interrupt */
	ACCDET_INFO("[accdet_setup_eint]enter\n");
#if 1
	accdet_pinctrl1 = devm_pinctrl_get(&accdet_device->dev);
	if (IS_ERR(accdet_pinctrl1)) {
		ret = PTR_ERR(accdet_pinctrl1);
		dev_info(&accdet_device->dev, "[accdet]Cannot find accdet accdet_pinctrl1!\n");
		return ret;
	}

	pins_default = pinctrl_lookup_state(accdet_pinctrl1, "default");
	if (IS_ERR(pins_default)) {
		ret = PTR_ERR(pins_default);
		/* dev_info(&accdet_device->dev, "[accdet]Cannot find accdet pinctrl default!\n"); */
	}

	pins_eint_int = pinctrl_lookup_state(accdet_pinctrl1, "state_eint_as_int");
	if (IS_ERR(pins_eint_int)) {
		ret = PTR_ERR(pins_eint_int);
		dev_info(&accdet_device->dev, "[accdet]Cannot find accdet pinctrl state_eint!\n");
		return ret;
	}
	pinctrl_select_state(accdet_pinctrl1, pins_eint_int);
#endif
	node = of_find_matching_node(node, accdet_of_match);
	if (node) {
		g_gpio_pin = of_get_named_gpio(node, "deb-gpios", 0);
		ret = of_property_read_u32(node, "debounce", &g_gpio_headset_deb);
		if (ret < 0) {
			ACCDET_ERROR("debounce time not found\n");
			return ret;
		}
		gpio_set_debounce(g_gpio_pin, g_gpio_headset_deb);
		g_accdet_irq = irq_of_parse_and_map(node, 0);
		of_property_read_u32_array(node, "interrupts", ints1, ARRAY_SIZE(ints1));
		g_accdet_eint_type = ints1[1];
		ACCDET_ERROR("[Accdet] gpiopin:%d debounce:%d accdet_irq:%d accdet_eint_type:%d\n",
				g_gpio_pin, g_gpio_headset_deb, g_accdet_irq, g_accdet_eint_type);
		ret = request_irq(g_accdet_irq, accdet_ap_eint_func, IRQF_TRIGGER_NONE, "accdet-eint", NULL);
		if (ret != 0) {
			ACCDET_ERROR("[Accdet]EINT IRQ LINE NOT AVAILABLE\n");
		} else {
			ACCDET_ERROR("[Accdet]accdet set EINT finished, accdet_irq=%d, headsetdebounce=%d\n",
				     g_accdet_irq, g_gpio_headset_deb);
		}
	} else {
		ACCDET_ERROR("[Accdet]%s can't find compatible node\n", __func__);
	}
	return 0;
}
#endif/* endif CONFIG_ACCDET_EINT */

/*
 * PART5.5: static  irq functions handle
 */
#ifdef CONFIG_ACCDET_EINT_IRQ
static int accdet_irq_handler(void)
{
	int eint_type = 0;
	unsigned int reg_val = 0;
	u64 cur_time = 0;
#if defined(CONFIG_MOISTURE_INTERNAL_SUPPORT) || defined(CONFIG_MOISTURE_EXTERNAL_SUPPORT)
	unsigned int moisture = 0;
#endif

	eint_type = 0;
	cur_time = accdet_get_current_time();
	reg_val = pmic_pwrap_read(ACCDET_CON12);
	ACCDET_DEBUG("[accdet_irq_handler][0x%x]=0x%x\n", ACCDET_CON12, reg_val);
#if defined(CONFIG_MOISTURE_INTERNAL_SUPPORT) || defined(CONFIG_MOISTURE_EXTERNAL_SUPPORT)
	if (g_cur_eint_state == EINT_PIN_PLUG_OUT) {
		ACCDET_DEBUG("=========[ACCDET]Moisture Enable=============\n\r");
		/* Disable ACCDET to AUXADC */
		pmic_pwrap_write(AUDENC_ANA_CON10, pmic_pwrap_read(AUDENC_ANA_CON10) & 0x1FFF);/*set bit[13-15] = 0*/
		pmic_pwrap_write(ACCDET_CON00, pmic_pwrap_read(ACCDET_CON00) & 0xFBFF); /*set bit[10] = 0*/
		pmic_pwrap_write(ACCDET_CON00, pmic_pwrap_read(ACCDET_CON00) | 0x800);  /*set bit[11] = 1*/
		/* Enable moisture detection, set 219A bit[13] = 1*/
		pmic_pwrap_write(AUDENC_ANA_CON9, pmic_pwrap_read(AUDENC_ANA_CON9) | 0x2000);
		/* select PAD_HP_EINT for moisture detection, set 219A bit[14] = 0*/
		pmic_pwrap_write(AUDENC_ANA_CON9, pmic_pwrap_read(AUDENC_ANA_CON9) & 0xBFFF);
#ifdef CONFIG_MOISTURE_INTERNAL_SUPPORT
		/* select VTH for 2v, and 500k, use internal resitance set 219C bit[10][12] = 1*/
		pmic_pwrap_write(AUDENC_ANA_CON10, pmic_pwrap_read(AUDENC_ANA_CON10) | 0x1400);
#endif
#ifdef CONFIG_MOISTURE_EXTERNAL_SUPPORT
		/* select VTH for 2v, and 500k, use external resitance set 219C bit[10] = 1, bit[11] = 0*/
		pmic_pwrap_write(AUDENC_ANA_CON10, pmic_pwrap_read(AUDENC_ANA_CON10) & 0xF7FF);
		pmic_pwrap_write(AUDENC_ANA_CON10, pmic_pwrap_read(AUDENC_ANA_CON10) | 0x0400);
#endif
		moisture = Accdet_PMIC_IMM_GetOneChannelValue(0);
		ACCDET_INFO("[ACCDET]Moisture Read Auxadc] new moisture =  %d\n\r", moisture);

		pmic_pwrap_write(ACCDET_CON00, pmic_pwrap_read(ACCDET_CON00) & 0xF7FF); /*set bit[11] = 0*/
		/* disable moisture detection, set 219A bit[13] = 0*/
		pmic_pwrap_write(AUDENC_ANA_CON9, pmic_pwrap_read(AUDENC_ANA_CON9) & 0xDFFF);
#ifdef CONFIG_MOISTURE_INTERNAL_SUPPORT
		/* revert select VTH for 2v, and 500k, set 219C bit[10][12] = 0*/
		pmic_pwrap_write(AUDENC_ANA_CON10, pmic_pwrap_read(AUDENC_ANA_CON10) & 0xEBFF);
#endif
#ifdef CONFIG_MOISTURE_EXTERNAL_SUPPORT
		/* revert select VTH for 2v, and 500k, set 219C bit[10] = 0, bit[11] = 1*/
		pmic_pwrap_write(AUDENC_ANA_CON10, pmic_pwrap_read(AUDENC_ANA_CON10) & 0xFBFF);
		pmic_pwrap_write(AUDENC_ANA_CON10, pmic_pwrap_read(AUDENC_ANA_CON10) | 0x0800);
#endif
	} else if (g_cur_eint_state == EINT_PIN_MOISTURE_DETECED) {
		ACCDET_DEBUG("ACCDET Moisture plug out detectecd\n");
		if (g_cur_eint_state == EINT_PIN_MOISTURE_DETECED) {/*just for low level trigger*/
			if (g_accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
				pmic_pwrap_write(ACCDET_CON12, reg_val|ACCDET_EINT0_IRQ_POL_B14);
			else
				pmic_pwrap_write(ACCDET_CON12, reg_val&(~ACCDET_EINT0_IRQ_POL_B14));
		} else {
			if (g_accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
				pmic_pwrap_write(ACCDET_CON12, reg_val&(~ACCDET_EINT0_IRQ_POL_B14));
			else
				pmic_pwrap_write(ACCDET_CON12, reg_val|ACCDET_EINT0_IRQ_POL_B14);
		}
		eint_type = ACCDET_EINT0_IRQ_IN;
		g_cur_eint_state = EINT_PIN_PLUG_OUT;
		clear_accdet_eint_interrupt(eint_type);
		while (((pmic_pwrap_read(ACCDET_CON12) & ACCDET_EINT0_IRQ_B2)
			&& (accdet_timeout_ns(cur_time, ACCDET_TIME_OUT))))
			;
		reg_val = pmic_pwrap_read(ACCDET_CON12);
		pmic_pwrap_write(ACCDET_CON12, reg_val&(~ACCDET_EINT0_IRQ_CLR_B10));
		pmic_pwrap_write(AUD_TOP_INT_STATUS0, RG_INT_STATUS_ACCDET_EINT0_B6);
		return 1;
	}
	if (moisture > g_moisture_vm) {
		ACCDET_DEBUG("ACCDET Moisture plug in detectecd\n");
		if (g_cur_eint_state == EINT_PIN_PLUG_OUT) {/*just for low level trigger*/
			if (g_accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
				pmic_pwrap_write(ACCDET_CON12, reg_val&(~ACCDET_EINT0_IRQ_POL_B14));
			else
				pmic_pwrap_write(ACCDET_CON12, reg_val|ACCDET_EINT0_IRQ_POL_B14);
		} else {
			if (g_accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
				pmic_pwrap_write(ACCDET_CON12, reg_val|ACCDET_EINT0_IRQ_POL_B14);
			else
				pmic_pwrap_write(ACCDET_CON12, reg_val&(~ACCDET_EINT0_IRQ_POL_B14));
		}
		eint_type = ACCDET_EINT0_IRQ_IN;
		g_cur_eint_state = EINT_PIN_MOISTURE_DETECED;
		clear_accdet_eint_interrupt(eint_type);
		while (((pmic_pwrap_read(ACCDET_CON12) & ACCDET_EINT0_IRQ_B2)
			&& (accdet_timeout_ns(cur_time, ACCDET_TIME_OUT))))
			;
		reg_val = pmic_pwrap_read(ACCDET_CON12);
		pmic_pwrap_write(ACCDET_CON12, reg_val&(~ACCDET_EINT0_IRQ_CLR_B10));
		pmic_pwrap_write(AUD_TOP_INT_STATUS0, RG_INT_STATUS_ACCDET_EINT0_B6);
		ACCDET_DEBUG("[mositure clear_accdet_eint_interrupt][0x%x]=0x%x\n",
			ACCDET_CON12, pmic_pwrap_read(ACCDET_CON12));
	} else {
#endif

#ifdef CONFIG_ACCDET_SUPPORT_EINT0
	if ((reg_val & ACCDET_IRQ_B0) &&
		((reg_val & ACCDET_EINT0_IRQ_B2) != ACCDET_EINT0_IRQ_B2)) {
		eint_type = ACCDET_IRQ_IN;
		clear_accdet_interrupt();
		if (s_accdet_status == MIC_BIAS) {
			/* accdet_auxadc_switch(1); */
			pmic_pwrap_write(ACCDET_CON03, REGISTER_VALUE(accdet_cust_setting->pwm_width));
			pmic_pwrap_write(ACCDET_CON04, REGISTER_VALUE(accdet_cust_setting->pwm_thresh));
		}
		accdet_workqueue_func();
		while (((pmic_pwrap_read(ACCDET_CON12) & ACCDET_IRQ_B0)
			&& (accdet_timeout_ns(cur_time, ACCDET_TIME_OUT))))
			;
		pmic_pwrap_write(ACCDET_CON12, pmic_pwrap_read(ACCDET_CON12)&(~ACCDET_IRQ_CLR_B8));
		/* interrupt issued */
		pmic_pwrap_write(AUD_TOP_INT_STATUS0, RG_INT_STATUS_ACCDET_B5);
	} else if ((reg_val & ACCDET_EINT0_IRQ_B2) == ACCDET_EINT0_IRQ_B2) {
		if (g_cur_eint_state == EINT_PIN_PLUG_IN) {
			if (g_accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
				pmic_pwrap_write(ACCDET_CON12, reg_val|ACCDET_EINT0_IRQ_POL_B14);
			else
				pmic_pwrap_write(ACCDET_CON12, reg_val&(~ACCDET_EINT0_IRQ_POL_B14));
		} else {
			if (g_accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
				pmic_pwrap_write(ACCDET_CON12, reg_val&(~ACCDET_EINT0_IRQ_POL_B14));
			else
				pmic_pwrap_write(ACCDET_CON12, reg_val|ACCDET_EINT0_IRQ_POL_B14);
		}
		eint_type = ACCDET_EINT0_IRQ_IN;
		clear_accdet_eint_interrupt(eint_type);
		while (((pmic_pwrap_read(ACCDET_CON12) & ACCDET_EINT0_IRQ_B2)
			&& (accdet_timeout_ns(cur_time, ACCDET_TIME_OUT))))
			;
		/* for fix icon disappear */
		reg_val = pmic_pwrap_read(ACCDET_CON12);
		pmic_pwrap_write(ACCDET_CON12, reg_val&(~ACCDET_EINT0_IRQ_CLR_B10));
		pmic_pwrap_write(AUD_TOP_INT_STATUS0, RG_INT_STATUS_ACCDET_EINT0_B6);
		accdet_eint_func(eint_type);
	} else {
		ACCDET_ERROR("[accdet_irq_handler]ACCDET IRQ and EINT0 IRQ don't be triggerred!!\n");
	}
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
	if ((reg_val&ACCDET_IRQ_B0) &&
		((reg_val & ACCDET_EINT1_IRQ_B3) != ACCDET_EINT1_IRQ_B3)) {
		eint_type = ACCDET_IRQ_IN;
		clear_accdet_interrupt();
		if (s_accdet_status == MIC_BIAS) {
			/* accdet_auxadc_switch(1); */
			pmic_pwrap_write(ACCDET_CON03, REGISTER_VALUE(accdet_cust_setting->pwm_width));
			pmic_pwrap_write(ACCDET_CON04, REGISTER_VALUE(accdet_cust_setting->pwm_thresh));
		}
		accdet_workqueue_func();
		while (((pmic_pwrap_read(ACCDET_CON12) & ACCDET_IRQ_B0)
			&& (accdet_timeout_ns(cur_time, ACCDET_TIME_OUT))))
			;
		pmic_pwrap_write(ACCDET_CON12, pmic_pwrap_read(ACCDET_CON12)&(~ACCDET_IRQ_CLR_B8));
		/* interrupt issued */
		pmic_pwrap_write(AUD_TOP_INT_STATUS0, RG_INT_STATUS_ACCDET_B5);
	} else if ((reg_val&ACCDET_EINT1_IRQ_B3) == ACCDET_EINT1_IRQ_B3) {
		if (g_cur_eint_state == EINT_PIN_PLUG_IN) {
			if (g_accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
				pmic_pwrap_write(ACCDET_CON12, reg_val|ACCDET_EINT1_IRQ_POL_B15);
			else
				pmic_pwrap_write(ACCDET_CON12, reg_val&(~ACCDET_EINT1_IRQ_POL_B15));
		} else {
			if (g_accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
				pmic_pwrap_write(ACCDET_CON12, reg_val&(~ACCDET_EINT1_IRQ_POL_B15));
			else
				pmic_pwrap_write(ACCDET_CON12, reg_val|ACCDET_EINT1_IRQ_POL_B15);
		}
		eint_type = ACCDET_EINT1_IRQ_IN;
		clear_accdet_eint_interrupt(eint_type);
		while (((pmic_pwrap_read(ACCDET_CON12) & ACCDET_EINT1_IRQ_B3)
			&& (accdet_timeout_ns(cur_time, ACCDET_TIME_OUT))))
			;
		/* for fix icon disappear */
		reg_val = pmic_pwrap_read(ACCDET_CON12);
		pmic_pwrap_write(ACCDET_CON12, reg_val&(~ACCDET_EINT1_IRQ_CLR_B11));
		pmic_pwrap_write(AUD_TOP_INT_STATUS0, RG_INT_STATUS_ACCDET_EINT1_B7);
		accdet_eint_func(eint_type);
	} else {
		ACCDET_ERROR("[accdet_irq_handler]ACCDET IRQ and EINT1 IRQ isn't triggerred!!\n");
	}
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT/* EINT0 & EINT1 */
	if ((reg_val & ACCDET_IRQ_B0) &&
		((reg_val & ACCDET_EINT0_IRQ_B2) != ACCDET_EINT0_IRQ_B2) &&
			((reg_val & ACCDET_EINT1_IRQ_B3) != ACCDET_EINT1_IRQ_B3)) {
		eint_type = ACCDET_IRQ_IN;
		clear_accdet_interrupt();
		if (s_accdet_status == MIC_BIAS) {
			/* accdet_auxadc_switch(1); */
			pmic_pwrap_write(ACCDET_CON03, REGISTER_VALUE(accdet_cust_setting->pwm_width));
			pmic_pwrap_write(ACCDET_CON04, REGISTER_VALUE(accdet_cust_setting->pwm_thresh));
		}
		accdet_workqueue_func();
		while (((pmic_pwrap_read(ACCDET_CON12) & ACCDET_IRQ_B0)
			&& (accdet_timeout_ns(cur_time, ACCDET_TIME_OUT))))
			;
		pmic_pwrap_write(ACCDET_CON12, pmic_pwrap_read(ACCDET_CON12)&(~ACCDET_IRQ_CLR_B8));
		/* interrupt issued */
		pmic_pwrap_write(AUD_TOP_INT_STATUS0, RG_INT_STATUS_ACCDET_B5);
	} else if ((reg_val & ACCDET_EINT0_IRQ_B2) == ACCDET_EINT0_IRQ_B2) {
		if (g_cur_eint_state == EINT_PIN_PLUG_IN) {
			if (g_accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
				pmic_pwrap_write(ACCDET_CON12, reg_val|ACCDET_EINT0_IRQ_POL_B14);
			else
				pmic_pwrap_write(ACCDET_CON12, reg_val&(~ACCDET_EINT0_IRQ_POL_B14));
		} else {
			if (g_accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
				pmic_pwrap_write(ACCDET_CON12, reg_val&(~ACCDET_EINT0_IRQ_POL_B14));
			else
				pmic_pwrap_write(ACCDET_CON12, reg_val|ACCDET_EINT0_IRQ_POL_B14);
		}
		eint_type = ACCDET_EINT0_IRQ_IN;
		clear_accdet_eint_interrupt(eint_type);
		while (((pmic_pwrap_read(ACCDET_CON12) & ACCDET_EINT0_IRQ_B2)
			&& (accdet_timeout_ns(cur_time, ACCDET_TIME_OUT))))
			;
		/* for fix icon disappear */
		reg_val = pmic_pwrap_read(ACCDET_CON12);
		pmic_pwrap_write(ACCDET_CON12, reg_val&(~ACCDET_EINT0_IRQ_CLR_B10));
		pmic_pwrap_write(AUD_TOP_INT_STATUS0, RG_INT_STATUS_ACCDET_EINT0_B6);
		accdet_eint_func(eint_type);
	} else if ((reg_val & ACCDET_EINT1_IRQ_B3) == ACCDET_EINT1_IRQ_B3) {
		if (g_cur_eint_state == EINT_PIN_PLUG_IN) {
			if (g_accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
				pmic_pwrap_write(ACCDET_CON12, reg_val|ACCDET_EINT1_IRQ_POL_B15);
			else
				pmic_pwrap_write(ACCDET_CON12, reg_val&(~ACCDET_EINT1_IRQ_POL_B15));
		} else {
			if (g_accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
				pmic_pwrap_write(ACCDET_CON12, reg_val&(~ACCDET_EINT1_IRQ_POL_B15));
			else
				pmic_pwrap_write(ACCDET_CON12, reg_val|ACCDET_EINT1_IRQ_POL_B15);
		}
		eint_type = ACCDET_EINT1_IRQ_IN;
		clear_accdet_eint_interrupt(eint_type);
		while (((pmic_pwrap_read(ACCDET_CON12) & ACCDET_EINT1_IRQ_B3)
			&& (accdet_timeout_ns(cur_time, ACCDET_TIME_OUT))))
			;
		/* for fix icon disappear */
		reg_val = pmic_pwrap_read(ACCDET_CON12);
		pmic_pwrap_write(ACCDET_CON12, reg_val&(~ACCDET_EINT1_IRQ_CLR_B11));
		pmic_pwrap_write(AUD_TOP_INT_STATUS0, RG_INT_STATUS_ACCDET_EINT1_B7);
		accdet_eint_func(eint_type);
	} else {
		ACCDET_ERROR("[accdet_irq_handler]ACCDET IRQ and EINT IRQ don't be triggerred!!\n");
}
#endif
#if defined(CONFIG_MOISTURE_INTERNAL_SUPPORT) || defined(CONFIG_MOISTURE_EXTERNAL_SUPPORT)
	}
#endif

	return 1;
}
#elif defined CONFIG_ACCDET_EINT
static int accdet_irq_handler(void)
{
	int eint_type = 0;
	u64 cur_time = 0;

	cur_time = accdet_get_current_time();
	ACCDET_DEBUG("[accdet_irq_handler]CONFIG_ACCDET_EINT-->enter\n");
	if ((pmic_pwrap_read(ACCDET_CON12) & ACCDET_IRQ_B0))
		eint_type = ACCDET_IRQ_IN;
		clear_accdet_interrupt();
	if (s_accdet_status == MIC_BIAS) {
		/* accdet_auxadc_switch(1); */
		pmic_pwrap_write(ACCDET_CON03, REGISTER_VALUE(accdet_cust_setting->pwm_width));
		pmic_pwrap_write(ACCDET_CON04, REGISTER_VALUE(accdet_cust_setting->pwm_thresh));
	}
	accdet_workqueue_func();
	while (((pmic_pwrap_read(ACCDET_CON12) & ACCDET_IRQ_B0)
		&& (accdet_timeout_ns(cur_time, ACCDET_TIME_OUT))))
		;
	pmic_pwrap_write(ACCDET_CON12, pmic_pwrap_read(ACCDET_CON12)&(~ACCDET_IRQ_CLR_B8));
	/* interrupt issued */
	pmic_pwrap_write(AUD_TOP_INT_STATUS0, RG_INT_STATUS_ACCDET_B5);

	return 1;
}
#else
static int accdet_irq_handler(void)
{
	ACCDET_ERROR("[accdet]CONFIG_ACCDET_EINT or CONFIG_ACCDET_EINT_IRQ is not defined\n");
	return 0;
}
#endif

/*
 * PART5.6: static accdet init functions
 */
static inline void accdet_eint_high_level_support(void)
{
	unsigned int reg_val = 0;

	reg_val = 0;
	ACCDET_DEBUG("[accdet]eint_high_level_support enter--->\n");

#if 0
	/* set high level trigger */
#ifdef CONFIG_ACCDET_EINT_IRQ
		/* pwrap_write(ACCDET_CON01, ACCDET_DISABLE); */
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
		if (g_cur_eint_state == EINT_PIN_PLUG_OUT) {
			reg_val = pmic_pwrap_read(ACCDET_CON12);
			if (headset_dts_data.eint_level_pol == IRQ_TYPE_LEVEL_HIGH) {
				g_accdet_eint_type = IRQ_TYPE_LEVEL_HIGH;
				pwrap_write(ACCDET_CON12, reg_val|ACCDET_EINT0_IRQ_POL_B14);
				ACCDET_INFO("[accdet0]high:[0x%x]=0x%x\n", ACCDET_CON12, reg_val);

				reg_val = pmic_pwrap_read(ACCDET_CON01);
				ACCDET_INFO("[accdet0]high:1.[0x%x]=0x%x\n", ACCDET_CON01, reg_val);
				/* set bit3 to enable default EINT init status */
				pmic_pwrap_write(ACCDET_CON01, reg_val|ACCDET_EINT0_SEQ_INIT_EN);
				mdelay(2);
				reg_val = pmic_pwrap_read(ACCDET_CON01);
				ACCDET_INFO("[accdet0]high:2.[0x%x]=0x%x\n", ACCDET_CON01, reg_val);
				reg_val = pmic_pwrap_read(ACCDET_DEFAULT_EINT_STS);
				ACCDET_INFO("[accdet0]high:1.[0x%x]=0x%x\n", ACCDET_DEFAULT_EINT_STS, reg_val);
				/* set default EINT init status */
				pmic_pwrap_write(ACCDET_DEFAULT_EINT_STS,
					(reg_val|ACCDET_EINT0_IVAL_SEL)&(~ACCDET_EINT0_IVAL));
				mdelay(2);
				reg_val = pmic_pwrap_read(ACCDET_DEFAULT_EINT_STS);
				ACCDET_INFO("[accdet0]high:2.[0x%x]=0x%x\n", ACCDET_DEFAULT_EINT_STS, reg_val);
				/* clear bit3 to disable default EINT init status */
				reg_val = pmic_pwrap_read(ACCDET_CON01);
				pmic_pwrap_write(ACCDET_CON01, reg_val&(~ACCDET_EINT0_SEQ_INIT_EN));
			} else {
				g_accdet_eint_type = IRQ_TYPE_LEVEL_LOW;/* default level_low */
				pwrap_write(ACCDET_CON12, reg_val&(~ACCDET_EINT0_IRQ_POL_B14));
			}
			reg_val = pmic_pwrap_read(ACCDET_CON15)&(~(0x0F<<3));
			pwrap_write(ACCDET_CON15, reg_val);
			pwrap_write(ACCDET_CON15, reg_val|ACCDET_EINT0_DEB_IN_256);
		}
	/* pwrap_write(ACCDET_CON02, pmic_pwrap_read(ACCDET_CON02)|ACCDET_EINT0_PWM_EN); */
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
		if (g_cur_eint_state == EINT_PIN_PLUG_OUT) {
			reg_val = pmic_pwrap_read(ACCDET_CON12);
			if (headset_dts_data.eint_level_pol == IRQ_TYPE_LEVEL_HIGH) {
				g_accdet_eint_type = IRQ_TYPE_LEVEL_HIGH;
				pwrap_write(ACCDET_CON12, reg_val|ACCDET_EINT1_IRQ_POL_B15);
				ACCDET_INFO("[accdet1]high:[0x%x]=0x%x\n", ACCDET_CON12, reg_val);

				/* set pmic eint default value */
				reg_val = pmic_pwrap_read(ACCDET_CON01);
				ACCDET_INFO("[accdet1]high:1.[0x%x]=0x%x\n", ACCDET_CON01, reg_val);
				/* set bit5 to enable default EINT init status */
				pmic_pwrap_write(ACCDET_CON01, reg_val|ACCDET_EINT1_SEQ_INIT_EN_B5);
				mdelay(2);
				reg_val = pmic_pwrap_read(ACCDET_CON01);
				ACCDET_INFO("[accdet1]high:2.[0x%x]=0x%x\n", ACCDET_CON01, reg_val);
				reg_val = pmic_pwrap_read(ACCDET_DEFAULT_EINT_STS);
				ACCDET_INFO("[accdet1]high:1.[0x%x]=0x%x\n", ACCDET_DEFAULT_EINT_STS, reg_val);
				/* set default EINT init status */
				pmic_pwrap_write(ACCDET_DEFAULT_EINT_STS,
					(reg_val|ACCDET_EINT1_IVAL_SEL)&(~ACCDET_EINT1_IVAL));
				mdelay(2);
				reg_val = pmic_pwrap_read(ACCDET_DEFAULT_EINT_STS);
				ACCDET_INFO("[accdet1]high:2.[0x%x]=0x%x\n", ACCDET_DEFAULT_EINT_STS, reg_val);
				/* clear bit5 to disable default EINT init status */
				reg_val = pmic_pwrap_read(ACCDET_CON01);
				pmic_pwrap_write(ACCDET_CON01, reg_val&(~ACCDET_EINT1_SEQ_INIT_EN));
			} else {
				g_accdet_eint_type = IRQ_TYPE_LEVEL_LOW;
				pwrap_write(ACCDET_CON12, reg_val&(~ACCDET_EINT1_IRQ_POL_B15));
			}
			reg_val = pmic_pwrap_read(ACCDET_CON25)&(~(0x0F<<3));
			pwrap_write(ACCDET_CON25, reg_val);
			pwrap_write(ACCDET_CON25, reg_val|ACCDET_EINT1_DEB_IN_256);
		}
	/* pwrap_write(ACCDET_CON02, pmic_pwrap_read(ACCDET_CON02)|ACCDET_EINT1_PWM_EN); */
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
		if (g_cur_eint_state == EINT_PIN_PLUG_OUT) {
			reg_val = pmic_pwrap_read(ACCDET_CON12);
			if (headset_dts_data.eint_level_pol == IRQ_TYPE_LEVEL_HIGH) {
				g_accdet_eint_type = IRQ_TYPE_LEVEL_HIGH;
				pwrap_write(ACCDET_CON12, reg_val|ACCDET_EINT_IRQ_POL_HIGH);
				ACCDET_INFO("[accdet2]high:[0x%x]=0x%x\n", ACCDET_CON12, reg_val);

				/* set pmic eint default value */
				reg_val = pmic_pwrap_read(ACCDET_CON01);
				ACCDET_INFO("[accdet2]high:1.[0x%x]=0x%x\n", ACCDET_CON01, reg_val);
				/* set bit3&bit5 to enable default EINT init status */
				pmic_pwrap_write(ACCDET_CON01, reg_val|ACCDET_EINT_SEQ_INIT_EN);
				mdelay(2);
				reg_val = pmic_pwrap_read(ACCDET_CON01);
				ACCDET_INFO("[accdet2]high:2.[0x%x]=0x%x\n", ACCDET_CON01, reg_val);
				reg_val = pmic_pwrap_read(ACCDET_DEFAULT_EINT_STS);
				ACCDET_INFO("[accdet2]high:1.[0x%x]=0x%x\n", ACCDET_DEFAULT_EINT_STS, reg_val);
				/* set default EINT init status */
				pmic_pwrap_write(ACCDET_DEFAULT_EINT_STS,
					(reg_val|ACCDET_EINT_IVAL_SEL)&(~ACCDET_EINT_IVAL));
				mdelay(2);
				reg_val = pmic_pwrap_read(ACCDET_DEFAULT_EINT_STS);
				ACCDET_INFO("[accdet2]high:2.[0x%x]=0x%x\n", ACCDET_DEFAULT_EINT_STS, reg_val);
				/* clear bit3bit5 to disable default EINT init status */
				reg_val = pmic_pwrap_read(ACCDET_CON01);
				pmic_pwrap_write(ACCDET_CON01, reg_val&(~ACCDET_EINT_SEQ_INIT_EN));
			} else {
				g_accdet_eint_type = IRQ_TYPE_LEVEL_LOW;
				pwrap_write(ACCDET_CON12, reg_val&(~ACCDET_EINT_IRQ_POL_LOW));
			}
			/* maybe need judge the bit3 is clear actually??? */
			reg_val = pmic_pwrap_read(ACCDET_CON12);
			pmic_pwrap_write(ACCDET_CON12, reg_val & (~ACCDET_EINT_IRQ_CLR_B10_11));

			reg_val = pmic_pwrap_read(ACCDET_CON15)&(~(0x0F<<3));
			pwrap_write(ACCDET_CON15, reg_val);
			pwrap_write(ACCDET_CON15, reg_val|ACCDET_EINT0_DEB_IN_256);

			reg_val = pmic_pwrap_read(ACCDET_CON25)&(~(0x0F<<3));
			pwrap_write(ACCDET_CON25, reg_val);
			pwrap_write(ACCDET_CON25, reg_val|ACCDET_EINT1_DEB_IN_256);
		}
	/* pwrap_write(ACCDET_CON02, */
	/* pmic_pwrap_read(ACCDET_CON02)|ACCDET_EINT0_PWM_EN|ACCDET_EINT1_PWM_EN); */
#endif
#endif
#endif
}

void accdet_init_once(int init_flag)
{
	unsigned int reg_val = 0;

	if (init_flag == ACCDET_INIT0_ONCE) {
		/* reset the accdet sw */
		pmic_pwrap_write(AUD_TOP_RST_CON0, RG_ACCDET_RST_B1);
		/* 1,power on; 0, power down */
		/* pmic_pwrap_write(AUD_TOP_CKPDN_CON0, RG_ACCDET_CK_PDN_B0); *//* clock */
		/* open top accdet interrupt */
		pmic_pwrap_write(AUD_TOP_INT_CON0_SET, RG_INT_EN_ACCDET_B5);
		/* pmic_pwrap_write(AUD_TOP_INT_MASK_CON0_SET, RG_INT_MASK_ACCDET_B5); //maybe needn't */
		pmic_pwrap_write(AUD_TOP_RST_CON0, pmic_pwrap_read(AUD_TOP_RST_CON0)&(~RG_ACCDET_RST_B1));

		/* init pwm frequency, duty & rise/falling delay */
		pmic_pwrap_write(ACCDET_CON04, REGISTER_VALUE(accdet_cust_setting->pwm_thresh));
		pmic_pwrap_write(ACCDET_CON03, REGISTER_VALUE(accdet_cust_setting->pwm_width));
		pmic_pwrap_write(ACCDET_CON05,
			  (accdet_cust_setting->fall_delay << 15 | accdet_cust_setting->rise_delay));
		/* accdet_eint_set_debounce(eint_plugin_debounce); *//* set eint debounce */

#ifdef HW_MODE_SUPPORT
		pmic_pwrap_write(ACCDET_CON01, pmic_pwrap_read(ACCDET_CON01)&(~ACCDET_ENABLE_B0));/* close accdet en */
		if (headset_dts_data.eint_level_pol == IRQ_TYPE_LEVEL_LOW) {
			pmic_pwrap_write(AUDENC_ANA_CON6, pmic_pwrap_read(AUDENC_ANA_CON6)|
			RG_AUDSPARE_FSTDSCHRG_ANALOG_DIR_EN | RG_AUDSPARE_FSTDSCHRG_IMPR_EN); /*annlog fastdischarge*/
		} else
			pmic_pwrap_write(AUDENC_ANA_CON6, pmic_pwrap_read(AUDENC_ANA_CON6) & (0xE0));
#ifdef CONFIG_ACCDET_EINT_IRQ
		reg_val = pmic_pwrap_read(ACCDET_CON24);
	#ifdef CONFIG_ACCDET_SUPPORT_EINT0
		pmic_pwrap_write(ACCDET_CON24, reg_val|ACCDET_HWEN_SEL_0
			|ACCDET_HWMODE_SEL|ACCDET_EINT0_DEB_OUT_DFF|ACCDET_EINIT_REVERSE);
	#elif defined CONFIG_ACCDET_SUPPORT_EINT1
		pmic_pwrap_write(ACCDET_CON24, reg_val|ACCDET_HWEN_SEL_1
			|ACCDET_HWMODE_SEL|ACCDET_EINT0_DEB_OUT_DFF|ACCDET_EINIT_REVERSE);
	#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
		pmic_pwrap_write(ACCDET_CON24, reg_val|ACCDET_HWEN_SEL_0_OR_1
			|ACCDET_HWMODE_SEL|ACCDET_EINT0_DEB_OUT_DFF|ACCDET_EINIT_REVERSE);
	#else/* need think check~~ */
		pmic_pwrap_write(ACCDET_CON24, reg_val|ACCDET_HWEN_SEL_0_AND_1
			|ACCDET_HWMODE_SEL|ACCDET_EINT0_DEB_OUT_DFF|ACCDET_EINIT_REVERSE);
	#endif
#endif
#else /* SW MODE */
	ACCDET_INFO("[accdet_init_once] sw mode setting,old Accdet_con24:0x%x, AUDENC_ANA_CON6:0x%x\n",
		pmic_pwrap_read(ACCDET_CON24), pmic_pwrap_read(AUDENC_ANA_CON6));

	reg_val = pmic_pwrap_read(ACCDET_CON24);
	pmic_pwrap_write(ACCDET_CON24, (reg_val & (~ACCDET_HWMODE_SEL))|ACCDET_FAST_DISCAHRGE);

	/*annlog fastdischarge*/
	if (headset_dts_data.eint_level_pol == IRQ_TYPE_LEVEL_LOW) {
		pmic_pwrap_write(AUDENC_ANA_CON6, pmic_pwrap_read(AUDENC_ANA_CON6)|
		RG_AUDSPARE_FSTDSCHRG_ANALOG_DIR_EN | RG_AUDSPARE_FSTDSCHRG_IMPR_EN);
	} else
		pmic_pwrap_write(AUDENC_ANA_CON6, pmic_pwrap_read(AUDENC_ANA_CON6) & (0xE0));

	ACCDET_INFO("[accdet_init_once] sw mode setting,new Accdet_con24:0x%x, AUDENC_ANA_CON6:0x%x\n",
		pmic_pwrap_read(ACCDET_CON24), pmic_pwrap_read(AUDENC_ANA_CON6));
#endif

#ifndef CONFIG_HEADSET_SUPPORT_FIVE_POLE/* 3/4-pole need bypass CMP-c */
		/*  pmic_pwrap_write(ACCDET_CON24, pmic_pwrap_read(ACCDET_CON24)|ACCDET_DISABLE_CMPC); */
		/* pmic_pwrap_write(ACCDET_CON24, ACCDET_DISABLE_CMPC); */
#endif
		   ACCDET_INFO("[accdet_init_once]-0 ACCDET_INIT0_ONCE done--->\n");
	} else if (init_flag == ACCDET_INIT1_ONCE) {
/* ======================config analog======================= */
		reg_val = pmic_pwrap_read(AUDENC_ANA_CON9);
		pmic_pwrap_write(AUDENC_ANA_CON9, reg_val|(headset_dts_data.mic_bias_vol<<4));
		reg_val = pmic_pwrap_read(AUDENC_ANA_CON10);
/* need add clear mode bit----bxx */
		if (headset_dts_data.accdet_mic_mode == HEADSET_MODE_1)/* ACC mode*/
			pmic_pwrap_write(AUDENC_ANA_CON10, reg_val|RG_ACCDET_MODE_ANA10_MODE1);/* 0x07 */
		else if (headset_dts_data.accdet_mic_mode == HEADSET_MODE_2) {/* Low cost mode without internal bias*/
			/* for test discharge quickly */
			pmic_pwrap_write(AUDENC_ANA_CON10, reg_val|RG_ACCDET_MODE_ANA10_MODE2);/* 0x887 */
		} else if (headset_dts_data.accdet_mic_mode == HEADSET_MODE_6) {/* Low cost mode with internal bias*/
			pmic_pwrap_write(AUDENC_ANA_CON10, reg_val|RG_ACCDET_MODE_ANA10_MODE2);/* 0x887 */
			reg_val = pmic_pwrap_read(AUDENC_ANA_CON9);
			pmic_pwrap_write(AUDENC_ANA_CON9, reg_val | 0x100);/* Set bit8 = 1 to use internal bias */
		} /* end HEADSET_MODE_6 */

		 /* ACCDET AUXADC AUTO Setting  */
		/* pmic_pwrap_write(AUXADC_ACCDET, pmic_pwrap_read(AUXADC_ACCDET)|AUXADC_ACCDET_AUTO_SPL_EN); */

/* =========interrupt enable and eint pwm set========== */
#ifdef CONFIG_ACCDET_EINT_IRQ
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
		 pmic_pwrap_write(ACCDET_CON15, pmic_pwrap_read(ACCDET_CON15)&(~(0x1F<<8)));
		 reg_val = pmic_pwrap_read(ACCDET_CON15);
		 pmic_pwrap_write(ACCDET_CON15, reg_val|ACCDET_EINT0_PWM_THRSH|ACCDET_EINT0_PWM_WIDTH);

		 reg_val = pmic_pwrap_read(ACCDET_CON02);
		 pmic_pwrap_write(ACCDET_CON02, reg_val|ACCDET_EINT0_PWM_EN_B3|ACCDET_EINT0_PWM_IDLE_B11);
		 /* pmic_pwrap_write(ACCDET_CON2, reg_val|ACCDET_EINT0_PWM_EN_B3); *//* discard */
		 ACCDET_DEBUG("[accdet_init_once]-1_0[0x%x]=0x%x\n", ACCDET_CON02, pmic_pwrap_read(ACCDET_CON02));

		 /* open top interrupt eint0 */
		pmic_pwrap_write(AUD_TOP_INT_CON0_SET, RG_INT_EN_ACCDET_EINT0_B6);
		/* pmic_pwrap_write(AUD_TOP_INT_MASK_CON0_SET, RG_INT_MASK_ACCDET_EINT0_B6); */

		/* open accdet interrupt eint0 */
		pwrap_write(ACCDET_CON01, pmic_pwrap_read(ACCDET_CON01)|ACCDET_EINT0_EN_B2);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
		pmic_pwrap_write(ACCDET_CON25, pmic_pwrap_read(ACCDET_CON25)&(~(0x1F<<8)));
		reg_val = pmic_pwrap_read(ACCDET_CON25);
		pmic_pwrap_write(ACCDET_CON25, reg_val|ACCDET_EINT1_PWM_THRSH|ACCDET_EINT1_PWM_WIDTH);

		reg_val = pmic_pwrap_read(ACCDET_CON02);
		pmic_pwrap_write(ACCDET_CON02, reg_val|ACCDET_EINT1_PWM_EN_B4|ACCDET_EINT1_PWM_IDLE_B12);
		/* pmic_pwrap_write(ACCDET_CON2, reg_val|ACCDET_EINT1_PWM_EN_B4); *//* discard */
		ACCDET_DEBUG("[accdet_init_once]-1_1[0x%x]=0x%x\n", ACCDET_CON02, pmic_pwrap_read(ACCDET_CON02));

		/* open top interrupt eint1 */
		pmic_pwrap_write(AUD_TOP_INT_CON0_SET, RG_INT_EN_ACCDET_EINT1_B7);
		/* pmic_pwrap_write(AUD_TOP_INT_MASK_CON0_SET, RG_INT_MASK_ACCDET_EINT1_B7); */

		/* open accdet interrupt eint1 */
		pwrap_write(ACCDET_CON01, pmic_pwrap_read(ACCDET_CON01)|ACCDET_EINT1_EN_B4);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
		pmic_pwrap_write(ACCDET_CON15, pmic_pwrap_read(ACCDET_CON15)&(~(0x1F<<8)));
		reg_val = pmic_pwrap_read(ACCDET_CON15);
		pmic_pwrap_write(ACCDET_CON15, reg_val|ACCDET_EINT0_PWM_THRSH|ACCDET_EINT0_PWM_WIDTH);
		pmic_pwrap_write(ACCDET_CON25, pmic_pwrap_read(ACCDET_CON25)&(~(0x1F<<8)));
		reg_val = pmic_pwrap_read(ACCDET_CON25);
		pmic_pwrap_write(ACCDET_CON25, reg_val|ACCDET_EINT1_PWM_THRSH|ACCDET_EINT1_PWM_WIDTH);

		reg_val = pmic_pwrap_read(ACCDET_CON02);
		pmic_pwrap_write(ACCDET_CON02, reg_val|ACCDET_EINT_PWM_IDLE_B11_12|ACCDET_EINT_PWM_EN_B3_4);
		/* pmic_pwrap_write(ACCDET_CON02, reg_val|ACCDET_EINT_PWM_EN_B3_4); *//* discard */
		ACCDET_DEBUG("[accdet_init_once]-1_2[0x%x]=0x%x\n", ACCDET_CON02, pmic_pwrap_read(ACCDET_CON02));

		/* open top interrupt eint0 & eint1 */
		pmic_pwrap_write(AUD_TOP_INT_CON0_SET, RG_INT_EN_ACCDET_EINT_B6_7);
		/* pmic_pwrap_write(AUD_TOP_INT_MASK_CON0_SET, RG_INT_EMASK_ACCDET_EINT_B6_7); */

		/* open accdet interrupt eint0&eint1 */
		pwrap_write(ACCDET_CON01, pmic_pwrap_read(ACCDET_CON01)|ACCDET_EINT_EN_B2_4);
#else
		ACCDET_ERROR("[accdet_init_once]CONFIG_ACCDET_EINT_IRQ defined error\n");
#endif
#endif/* end ifdef CONFIG_ACCDET_EINT_IRQ  */
		ACCDET_INFO("[accdet_init_once] ACCDET_INIT1_ONCE done--->\n");
	 } else {
		 ACCDET_ERROR("[accdet_init_once] error: do nothing of accdet_init_once\n");
	}
}

static inline void accdet_init(void)
{
	unsigned int reg_val = 0;

	reg_val = 0;
	ACCDET_INFO("[accdet_init]start --->\n");

	/* add new of DE for fix icon cann't appear */
	/* set and clear initial bit every eint interrutp */
	pmic_pwrap_write(ACCDET_CON01, pmic_pwrap_read(ACCDET_CON01)|ACCDET_SEQ_INIT_EN_B1);
	mdelay(2);/* 2ms */
	ACCDET_DEBUG("[accdet_init][0x%x]=0x%x\n", ACCDET_CON01, pmic_pwrap_read(ACCDET_CON01));
	pmic_pwrap_write(ACCDET_CON01, pmic_pwrap_read(ACCDET_CON01)&(~ACCDET_SEQ_INIT_EN_B1));
	mdelay(1);

	/* pmic_pwrap_write(ACCDET_CON13, 0x0); */

	/* init the debounce time (debounce/32768)sec */
	accdet_set_debounce(accdet_state000, accdet_cust_setting->debounce0);
	accdet_set_debounce(accdet_state010, accdet_cust_setting->debounce1);
	accdet_set_debounce(accdet_state110, accdet_cust_setting->debounce3);
	accdet_set_debounce(accdet_auxadc, accdet_cust_setting->debounce4);/* auxadc:2ms */
#ifdef HW_MODE_SUPPORT
	/* workround for HW fast discharge, first disabel fast discharge */
	pmic_pwrap_write(ACCDET_CON24, ACCDET_FAST_DISCAHRGE_DIS);
	fast_discharge = false;
#endif
	/* pwrap_write(ACCDET_CON01, pmic_pwrap_read(ACCDET_CON01)|ACCDET_DISABLE); */

	/* ACCDET Analog Setting */
	/* pwrap_write(ACCDET_CON0, RG_ACCDET2AUXADC_SW_NORMAL); *//* control ACCDET to AUXADC switch */
	/* pwrap_write(AUDENC_ANA_CON10, pmic_pwrap_read(AUDENC_ANA_CON10)|RG_AUD_ACCDET_MBIAS1_EN); */
	/* pmic_pwrap_write(AUDENC_ANA_CON9, pmic_pwrap_read(AUDENC_ANA_CON9)
	*	|(headset_dts_data.mic_bias_vol<<4));
	*/
	/* | RG_AUD_PW_MBIAS1);|RG_AUD_MICBIAS1_LOWP_EN|RG_AUD_MICBIAS1_BYPASS_EN); */

	/* maybe needn't run at the most case  */
	/* accdet_eint_high_level_support(); *//* set high level trigger of eint */
}

/*
 * PART6: accdet probe int handle
 */
void accdet_int_handler(void)
{
	int ret = 0;

	ACCDET_INFO("[accdet_int_handler]-Triggered!\n");
	ret = accdet_irq_handler();
	if (ret == 0)
		ACCDET_ERROR("[accdet_int_handler]don't finished\n");
}

void accdet_eint_int_handler(void)
{
	int ret = 0;

	ACCDET_INFO("[accdet_eint_int_handler]-Triggered!\n");
	ret = accdet_irq_handler();
	if (ret == 0)
		ACCDET_ERROR("[accdet_eint_int_handler]don't finished\n");
}

/*
 * PART5.7: static  sysfs attr functions for debug
 */
/* ---sysfs--- */
#if DEBUG_THREAD
static int dump_register(void)
{
	int i = 0;

#ifdef CONFIG_HEADSET_SUPPORT_FIVE_POLE
		ACCDET_INFO("[CONFIG_HEADSET_SUPPORT_FIVE_POLE] support 5-pole.\n");
#endif

#ifdef CONFIG_ACCDET_EINT_IRQ
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
	ACCDET_INFO("[CONFIG_ACCDET_SUPPORT_EINT0][MODE_%d]dump_regs:\n", headset_dts_data.accdet_mic_mode);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
	ACCDET_INFO("[CONFIG_ACCDET_SUPPORT_EINT1][MODE_%d]dump_regs:\n", headset_dts_data.accdet_mic_mode);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
	ACCDET_INFO("[CONFIG_ACCDET_SUPPORT_BI_EINT][MODE_%d]dump_regs:\n", headset_dts_data.accdet_mic_mode);
#else
	ACCDET_INFO("[NO_CONFIG_ACCDET_EINT_0_1]Error1.\n");
#endif
#elif defined CONFIG_ACCDET_EINT
	ACCDET_INFO("[CONFIG_ACCDET_EINT][MODE_%d]dump_regs:\n", headset_dts_data.accdet_mic_mode);
#else
	ACCDET_INFO("[NO_CONFIG_ACCDET_EINT]Error2.\n");
#endif

	for (i = ACCDET_CON00; i <= ACCDET_CON28; i += 2)/* accdet addr */
		ACCDET_INFO("ACCDET_ADDR:(0x%x)=0x%x\n", i, pmic_pwrap_read(i));

	ACCDET_INFO("(0x%x)=0x%x\n", TOP_CON, pmic_pwrap_read(TOP_CON));
	ACCDET_INFO("(0x%x)=0x%x\n", TOP_CKPDN_CON0, pmic_pwrap_read(TOP_CKPDN_CON0));
	ACCDET_INFO("(0x%x)=0x%x\n", AUD_TOP_CKPDN_CON0, pmic_pwrap_read(AUD_TOP_CKPDN_CON0));
	ACCDET_INFO("(0x%x)=0x%x\n", AUD_TOP_RST_CON0, pmic_pwrap_read(AUD_TOP_RST_CON0));
	ACCDET_INFO("(0x%x)=0x%x\n", AUD_TOP_INT_CON0, pmic_pwrap_read(AUD_TOP_INT_CON0));
	ACCDET_INFO("(0x%x)=0x%x\n", AUD_TOP_INT_MASK_CON0, pmic_pwrap_read(AUD_TOP_INT_MASK_CON0));
	ACCDET_INFO("(0x%x)=0x%x\n", AUD_TOP_INT_STATUS0, pmic_pwrap_read(AUD_TOP_INT_STATUS0));
	ACCDET_INFO("(0x%x)=0x%x\n", AUDENC_ANA_CON6, pmic_pwrap_read(AUDENC_ANA_CON6));
	ACCDET_INFO("(0x%x)=0x%x\n", AUDENC_ANA_CON9, pmic_pwrap_read(AUDENC_ANA_CON9));
	ACCDET_INFO("(0x%x)=0x%x\n", AUDENC_ANA_CON10, pmic_pwrap_read(AUDENC_ANA_CON10));
	ACCDET_INFO("(0x%x)=0x%x\n", AUDENC_ANA_CON11, pmic_pwrap_read(AUDENC_ANA_CON11));
	ACCDET_INFO("(0x%x)=0x%x\n", AUXADC_CON2, pmic_pwrap_read(AUXADC_CON2));
	ACCDET_INFO("(0x%x)=0x%x\n", AUXADC_RQST0, pmic_pwrap_read(AUXADC_RQST0));
	ACCDET_INFO("(0x%x)=0x%x\n", AUXADC_ACCDET, pmic_pwrap_read(AUXADC_ACCDET));
	ACCDET_INFO("(0x%x)=0x%x\n", AUXADC_ADC5, pmic_pwrap_read(AUXADC_ADC5));

	ACCDET_INFO("[accdet_dts]deb0=0x%x,deb1=0x%x,deb3=0x%x,deb4=0x%x\n",
		 accdet_cust_setting->debounce0, accdet_cust_setting->debounce1,
		 accdet_cust_setting->debounce3, accdet_cust_setting->debounce4);

	return 0;
}

static int cat_register(char *buf)
{
	int i = 0;
	char buf_temp[128] = { 0 };

#ifdef CONFIG_HEADSET_SUPPORT_FIVE_POLE
	strncat(buf, "[CONFIG_HEADSET_SUPPORT_FIVE_POLE] support 5-pole.\n", 64);
#endif

#ifdef CONFIG_ACCDET_EINT_IRQ
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
	sprintf(buf_temp, "[ACCDET_SUPPORT_EINT0][MODE_%d]dump_regs:\n",
		headset_dts_data.accdet_mic_mode);
	strncat(buf, buf_temp, strlen(buf_temp));
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
	sprintf(buf_temp, "[ACCDET_SUPPORT_EINT1][MODE_%d]dump_regs:\n",
		headset_dts_data.accdet_mic_mode);
	strncat(buf, buf_temp, strlen(buf_temp));
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
	sprintf(buf_temp, "[ACCDET_SUPPORT_BI_EINT][MODE_%d]dump_regs:\n",
		headset_dts_data.accdet_mic_mode);
	strncat(buf, buf_temp, strlen(buf_temp));
#else
	strncat(buf, "[CONFIG_ACCDET_EINT_IRQ]Error!\n", 64);
#endif
#elif defined CONFIG_ACCDET_EINT
	sprintf(buf_temp, "[ACCDET_EINT][MODE_%d]dump_regs:\n",
		headset_dts_data.accdet_mic_mode);
	strncat(buf, buf_temp, strlen(buf_temp));
#else
	strncat(buf, "[NO_CONFIG_ACCDET_EINT_SUPPORT]Error!\n", 64);
#endif

	for (i = ACCDET_CON00; i <= ACCDET_CON28; i += 2) {/* accdet addr */
		sprintf(buf_temp, "ADDR[0x%x]=0x%x\n", i, pmic_pwrap_read(i));
		strncat(buf, buf_temp, strlen(buf_temp));
	}

	sprintf(buf_temp, "(0x%x)=0x%x, (0x%x)=0x%x\n",
		TOP_CON, pmic_pwrap_read(TOP_CON),
		TOP_CKPDN_CON0, pmic_pwrap_read(TOP_CKPDN_CON0));
	strncat(buf, buf_temp, strlen(buf_temp));

	sprintf(buf_temp, "(0x%x)=0x%x, (0x%x)=0x%x\n",
		AUD_TOP_RST_CON0, pmic_pwrap_read(AUD_TOP_RST_CON0),
		AUD_TOP_INT_CON0, pmic_pwrap_read(AUD_TOP_INT_CON0));
	strncat(buf, buf_temp, strlen(buf_temp));

	sprintf(buf_temp, "(0x%x)=0x%x, (0x%x)=0x%x\n",
		AUD_TOP_INT_MASK_CON0, pmic_pwrap_read(AUD_TOP_INT_MASK_CON0),
		AUD_TOP_INT_STATUS0, pmic_pwrap_read(AUD_TOP_INT_STATUS0));
	strncat(buf, buf_temp, strlen(buf_temp));

	sprintf(buf_temp, "0x%x)=0x%x, (0x%x)=0x%x, (0x%x)=0x%x, (0x%x)=0x%x\n",
		AUDENC_ANA_CON6, pmic_pwrap_read(AUDENC_ANA_CON6),
		AUDENC_ANA_CON9, pmic_pwrap_read(AUDENC_ANA_CON9),
		AUDENC_ANA_CON10, pmic_pwrap_read(AUDENC_ANA_CON10),
		AUDENC_ANA_CON11, pmic_pwrap_read(AUDENC_ANA_CON11));
	strncat(buf, buf_temp, strlen(buf_temp));

	sprintf(buf_temp, "(0x%x)=0x%x, (0x%x)=0x%x\n",
		AUXADC_CON2, pmic_pwrap_read(AUXADC_CON2),
		AUXADC_RQST0, pmic_pwrap_read(AUXADC_RQST0));
	strncat(buf, buf_temp, strlen(buf_temp));

	sprintf(buf_temp, "(0x%x)=0x%x, (0x%x)=0x%x\n",
		AUXADC_ACCDET, pmic_pwrap_read(AUXADC_ACCDET),
		AUXADC_ADC5, pmic_pwrap_read(AUXADC_ADC5));
	strncat(buf, buf_temp, strlen(buf_temp));


	sprintf(buf_temp, "[accdet_dts]deb0=0x%x,deb1=0x%x,deb3=0x%x,deb4=0x%x\n",
		 accdet_cust_setting->debounce0, accdet_cust_setting->debounce1,
		 accdet_cust_setting->debounce3, accdet_cust_setting->debounce4);
	strncat(buf, buf_temp, strlen(buf_temp));

	return 0;
}

static ssize_t store_accdet_call_state(struct device_driver *ddri, const char *buf, size_t count)
{
	int ret = 0;

	if (strlen(buf) < 1) {
		ACCDET_INFO("[%s] Invalid input!!\n",  __func__);
		return -EINVAL;
	}

	ret = kstrtoint(buf, 10, &s_call_status);
	if (ret < 0)
		return ret;

	switch (s_call_status) {
	case CALL_IDLE:
		ACCDET_DEBUG("[%s]accdet call: Idle state!\n", __func__);
		break;
	case CALL_RINGING:
		ACCDET_DEBUG("[%s]accdet call: ringing state!\n", __func__);
		break;
	case CALL_ACTIVE:
		ACCDET_DEBUG("[%s]accdet call: active or hold state!\n", __func__);
		break;
	default:
		ACCDET_DEBUG("[%s]accdet call: Invalid value=%d\n", __func__, s_call_status);
		break;
	}
	return count;
}

static ssize_t show_pin_recognition_state(struct device_driver *ddri, char *buf)
{
	if (buf == NULL) {
		ACCDET_ERROR("[%s] *buf is NULL Pointer\n",  __func__);
		return -EINVAL;
	}

	ACCDET_INFO("[CONFIG_ACCDET_PIN_RECOGNIZATION]No defined,no support\n");
	return sprintf(buf, "%d\n", 0);
}

static DRIVER_ATTR(accdet_pin_recognition, 0664, show_pin_recognition_state, NULL);
static DRIVER_ATTR(accdet_call_state, 0664, NULL, store_accdet_call_state);

static ssize_t store_accdet_set_headset_mode(struct device_driver *ddri, const char *buf, size_t count)
{
	int ret = 0;
	int tmp_headset_mode = 0;

	if (strlen(buf) < 1) {
		ACCDET_ERROR("[%s] Invalid input!!\n",  __func__);
		return -EINVAL;
	}

	ret = kstrtoint(buf, 10, &tmp_headset_mode);
	if (ret < 0)
		return ret;

	ACCDET_INFO("[%s]get accdet mode: %d\n", __func__, tmp_headset_mode);
	switch (tmp_headset_mode&0x0F) {
	case HEADSET_MODE_1:
		ACCDET_INFO("[%s]Don't support accdet mode_1 to configure\n", __func__);
		/* headset_dts_data.accdet_mic_mode = tmp_headset_mode; */
		/* accdet_init(); */
		break;
	case HEADSET_MODE_2:
		headset_dts_data.accdet_mic_mode = tmp_headset_mode;
		accdet_init();
		break;
	case HEADSET_MODE_6:
		headset_dts_data.accdet_mic_mode = tmp_headset_mode;
		accdet_init();
		break;
	default:
		ACCDET_ERROR("[%s]Not support accdet mode: %d\n", __func__, tmp_headset_mode);
		break;
	}

	accdet_init_once(ACCDET_INIT1_ONCE);

	return count;
}

static ssize_t show_accdet_dump_register(struct device_driver *ddri, char *buf)
{
	if (buf == NULL) {
		ACCDET_ERROR("[%s] *buf is NULL Pointer\n",  __func__);
		return -EINVAL;
	}

	cat_register(buf);
	ACCDET_INFO("[%s] buf_size:%d\n", __func__, (int)strlen(buf));

	/* accdet_pmic_Read_Efuse_HPOffset(); */

	return strlen(buf);
}

static int dbug_thread(void *unused)
{
	while (s_start_debug_thread) {
		if (s_dump_register)
			dump_register();
		msleep(500);
	}
	return 0;
}

static ssize_t store_accdet_start_debug_thread(struct device_driver *ddri, const char *buf, size_t count)
{
	int error = 0;
	int ret = 0;

	if (strlen(buf) < 1) {
		ACCDET_ERROR("[%s] Invalid input!!\n",  __func__);
		return -EINVAL;
	}

	/* if write 0, Invalid; otherwise, valid */
	ret = strncmp(buf, "0", 1);
	if (ret) {
		s_start_debug_thread = 1;
		s_thread = kthread_run(dbug_thread, 0, "ACCDET");
		if (IS_ERR(s_thread)) {
			error = PTR_ERR(s_thread);
			ACCDET_ERROR("[%s]failed to create kernel thread: %d\n",  __func__, error);
		} else {
			ACCDET_INFO("[%s]start debug thread!\n",  __func__);
		}
	} else {
		s_start_debug_thread = 0;
		ACCDET_INFO("[%s]stop debug thread!\n",  __func__);
	}

	return count;
}

static ssize_t store_accdet_dump_register(struct device_driver *ddri, const char *buf, size_t count)
{
	int ret = 0;

	if (strlen(buf) < 1) {
		ACCDET_ERROR("[%s] Invalid input!!\n",  __func__);
		return -EINVAL;
	}

	/* if write 0, Invalid; otherwise, valid */
	ret = strncmp(buf, "0", 1);
	if (ret) {
		s_dump_register = 1;
		ACCDET_INFO("[%s]start dump regs!\n",  __func__);
	} else {
		s_dump_register = 0;
		ACCDET_INFO("[%s]stop dump regs!\n",  __func__);
	}

	return count;
}

static ssize_t store_accdet_rw_register(struct device_driver *ddri, const char *buf, size_t count)
{
	char rw_flag = 0;
	int ret = 0;
	unsigned int addr_temp = 0;
	unsigned int value_temp = 0;

	if (strlen(buf) < 2) {
		ACCDET_ERROR("[%s] Invalid input!!\n",  __func__);
		return -EINVAL;
	}

	ret = sscanf(buf, "%c", &rw_flag);

	if ((rw_flag == 'r') || (rw_flag == 'R')) {
		ret = sscanf(buf, "%c,0x%x", &rw_flag, &addr_temp);
		if (ret < 2) {
			ACCDET_ERROR("[%s] read Invalid input!!\n",  __func__);
			return ret;
		}
		ACCDET_INFO("[%s] read addr[0x%x]\n",  __func__, addr_temp);
		/* comfirm PMIC addr is legal */
		if ((addr_temp < PMIC_REG_BASE_START) || (addr_temp > PMIC_REG_BASE_END)) {
			ACCDET_INFO("[%s] Can't set illegal addr[0x%x]!!\n", __func__, addr_temp);
		} else if (addr_temp&0x01) {
			ACCDET_ERROR("[%s] No set illegal addr[0x%x]!!\n", __func__, addr_temp);
		} else {
			rw_value[1] = pmic_pwrap_read(addr_temp);/* read reg */
			rw_value[0] = addr_temp;
		}
		return count;
	} else if ((rw_flag == 'w') || (rw_flag == 'W')) {
		ret = sscanf(buf, "%c,0x%x,0x%x", &rw_flag, &addr_temp, &value_temp);
		if (ret < 3) {
			ACCDET_ERROR("[%s] write Invalid input!!\n",  __func__);
			return ret;
		}
		ACCDET_INFO("[%s] write addr[0x%x]=0x%x\n",  __func__, addr_temp, value_temp);
		/* comfirm PMIC addr is legal */
		if ((addr_temp < PMIC_REG_BASE_START) || (addr_temp > PMIC_REG_BASE_END)) {
			ACCDET_INFO("[%s] Can't set illegal addr[0x%x]!!\n", __func__, addr_temp);
		} else if (addr_temp&0x01) {
			ACCDET_ERROR("[%s] No set illegal addr[0x%x]!!\n", __func__, addr_temp);
		} else {
			pmic_pwrap_write(addr_temp, value_temp);/* set reg */
			mdelay(2);
			rw_value[1] = pmic_pwrap_read(addr_temp);/* read reg */
			rw_value[0] = addr_temp;
		}
	} else {
		ACCDET_ERROR("[%s] error handle register\n",  __func__);
	}

	return count;
}

static ssize_t show_accdet_rw_register(struct device_driver *ddri, char *buf)
{
	if (buf == NULL) {
		ACCDET_ERROR("[%s] *buf is NULL Pointer\n",  __func__);
		return -EINVAL;
	}
	sprintf(buf, "addr[0x%x]=0x%x\n", rw_value[0], rw_value[1]);

	return strlen(buf);
}

static ssize_t show_accdet_state(struct device_driver *ddri, char *buf)
{
	char temp_type = (char)s_cable_type;

	if (buf == NULL) {
		ACCDET_ERROR("[%s] *buf is NULL Pointer\n",  __func__);
		return -EINVAL;
	}

	snprintf(buf, 3, "%d\n", temp_type);

	return strlen(buf);
}

/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(dump_register, S_IWUSR | S_IRUGO, show_accdet_dump_register, store_accdet_dump_register);
static DRIVER_ATTR(set_headset_mode, S_IWUSR | S_IRUGO, NULL, store_accdet_set_headset_mode);
static DRIVER_ATTR(start_debug, S_IWUSR | S_IRUGO, NULL, store_accdet_start_debug_thread);
static DRIVER_ATTR(rw_register, S_IWUSR | S_IRUGO, show_accdet_rw_register, store_accdet_rw_register);
static DRIVER_ATTR(state, S_IWUSR | S_IRUGO, show_accdet_state, NULL);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *accdet_attr_list[] = {
	&driver_attr_start_debug,
	&driver_attr_rw_register,
	&driver_attr_dump_register,
	&driver_attr_set_headset_mode,
	&driver_attr_accdet_call_state,
	&driver_attr_state,
/*#ifdef CONFIG_ACCDET_PIN_RECOGNIZATION*/
	&driver_attr_accdet_pin_recognition,
/*#endif*/
};

static int accdet_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)ARRAY_SIZE(accdet_attr_list);
/* int num=(int)(sizeof(accdet_attr_list) / sizeof(accdet_attr_list[0])); */

	if (driver == NULL)
		return -EINVAL;
	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, accdet_attr_list[idx]);
		if (err) {
			ACCDET_ERROR("[accdet_create_attr]driver_create_file (%s) = %d\n",
				accdet_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}
#endif

/* just be called by audio module for DC trim */
void accdet_late_init(unsigned long a)
{
	if (atomic_cmpxchg(&s_accdet_first, 1, 0)) {
		del_timer_sync(&accdet_init_timer);
		accdet_init();
		/* just need run once, for fix icon disappear HW */
		accdet_init_once(ACCDET_INIT0_ONCE);
		/* just need run once, config analog, enable interrupt, etc.*/
		accdet_init_once(ACCDET_INIT1_ONCE);
		/* schedule a work for the first detection */
		/* delete by xuexi as maybe it redundant */
		/* queue_work(accdet_workqueue, &accdet_work); */
	} else {
		ACCDET_INFO("[accdet_late_init]err: accdet have been done or get dts failed!\n");
	}
}
EXPORT_SYMBOL(accdet_late_init);

/* confirm accdet_int if the init_timer timeout for DC trim */
static void accdet_delay_callback(unsigned long a)
{
	if (atomic_cmpxchg(&s_accdet_first, 1, 0)) {
		accdet_init();
		/* just need run once, for fix icon disappear HW */
		accdet_init_once(ACCDET_INIT0_ONCE);
		/* just need run once, config analog, enable interrupt, etc.*/
		accdet_init_once(ACCDET_INIT1_ONCE);
		/* schedule a work for the first detection */
		/* delete by xuexi as maybe it redundant */
		/* queue_work(accdet_workqueue, &accdet_work); */
	} else {
		ACCDET_INFO("[accdet_delay_callback]err: accdet have been done or get dts failed!\n");
	}
}

int mt_accdet_probe(struct platform_device *dev)
{
	int ret = 0;
#if DEBUG_THREAD
	struct platform_driver accdet_driver_hal = accdet_driver_func();
#endif
	ACCDET_INFO("[mt_accdet_probe]probe start..\n");

	/* Create normal device for auido use */
	ret = alloc_chrdev_region(&accdet_devno, 0, 1, ACCDET_DEVNAME);/* get devNo */
	if (ret)
		ACCDET_ERROR("[mt_accdet_probe]alloc_chrdev_region: Get Major number error!\n");

	accdet_cdev = cdev_alloc();
	accdet_cdev->owner = THIS_MODULE;
	accdet_cdev->ops = accdet_get_fops();
	ret = cdev_add(accdet_cdev, accdet_devno, 1);
	if (ret)
		ACCDET_ERROR("[mt_accdet_probe]accdet error: cdev_add\n");

	/* create /sys/class/ACCDET_DEVNAME node */
	accdet_class = class_create(THIS_MODULE, ACCDET_DEVNAME);
	/* if we want auto creat device node, we must call this */
	accdet_nor_device = device_create(accdet_class, NULL, accdet_devno, NULL, ACCDET_DEVNAME);

#if DEBUG_THREAD
	ret = accdet_create_attr(&accdet_driver_hal.driver);
	if (ret != 0)
		ACCDET_ERROR("[mt_accdet_probe]create attribute err = %d\n", ret);
#endif

	/* Create input device*/
	kpd_accdet_dev = input_allocate_device();
	if (!kpd_accdet_dev) {
		ACCDET_ERROR("[mt_accdet_probe]kpd_accdet_dev : fail!\n");
		return -ENOMEM;
	}

	/* define multi-key keycode */
	__set_bit(EV_KEY, kpd_accdet_dev->evbit);
	__set_bit(KEY_PLAYPAUSE, kpd_accdet_dev->keybit);
	__set_bit(KEY_VOLUMEDOWN, kpd_accdet_dev->keybit);
	__set_bit(KEY_VOLUMEUP, kpd_accdet_dev->keybit);
	__set_bit(KEY_VOICECOMMAND, kpd_accdet_dev->keybit);
	__set_bit(EV_SW, kpd_accdet_dev->evbit);
	__set_bit(SW_HEADPHONE_INSERT, kpd_accdet_dev->swbit);
	__set_bit(SW_MICROPHONE_INSERT, kpd_accdet_dev->swbit);
	__set_bit(SW_JACK_PHYSICAL_INSERT, kpd_accdet_dev->swbit);
	__set_bit(SW_LINEOUT_INSERT, kpd_accdet_dev->swbit);

	kpd_accdet_dev->id.bustype = BUS_HOST;
	kpd_accdet_dev->name = "ACCDET";
	if (input_register_device(kpd_accdet_dev))
		ACCDET_ERROR("[mt_accdet_probe]kpd_accdet_dev register : fail!\n");

	/* wake lock */
	wake_lock_init(&accdet_suspend_lock, WAKE_LOCK_SUSPEND, "accdet wakelock");
	wake_lock_init(&accdet_irq_lock, WAKE_LOCK_SUSPEND, "accdet irq wakelock");
	wake_lock_init(&accdet_key_lock, WAKE_LOCK_SUSPEND, "accdet key wakelock");
	wake_lock_init(&accdet_timer_lock, WAKE_LOCK_SUSPEND, "accdet timer wakelock");

	/* INIT the timer to disable micbias */
	init_timer(&micbias_timer);
	micbias_timer.expires = jiffies + MICBIAS_DISABLE_TIMER;
	micbias_timer.function = &disable_micbias;
	micbias_timer.data = ((unsigned long)0);

	/* INIT the timer for comfirm the accdet can also init when audio can't callback in any case*/
	init_timer(&accdet_init_timer);
	accdet_init_timer.expires = jiffies + ACCDET_INIT_WAIT_TIMER;
	accdet_init_timer.function = &accdet_delay_callback;
	accdet_init_timer.data = ((unsigned long)0);

	/* Create workqueue */
	accdet_workqueue = create_singlethread_workqueue("accdet");
	INIT_WORK(&accdet_work, accdet_work_callback);
	accdet_disable_workqueue = create_singlethread_workqueue("accdet_disable");
	INIT_WORK(&accdet_disable_work, disable_micbias_callback);
	accdet_eint_workqueue = create_singlethread_workqueue("accdet_eint");
	INIT_WORK(&accdet_eint_work, accdet_eint_work_callback);


	pmic_register_interrupt_callback(INT_ACCDET, accdet_int_handler);/* accdet int */
#ifdef CONFIG_ACCDET_EINT_IRQ
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
	pmic_register_interrupt_callback(INT_ACCDET_EINT0, accdet_eint_int_handler);/* accdet eint0 */
	ACCDET_INFO("[mt_accdet_probe]CONFIG_ACCDET_SUPPORT_EINT0 opened!\n");
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
	pmic_register_interrupt_callback(INT_ACCDET_EINT1, accdet_eint_int_handler);/* accdet eint1 */
	ACCDET_INFO("[mt_accdet_probe]CONFIG_ACCDET_SUPPORT_EINT1 opened!\n");
/* #elif defined CONFIG_ACCDET_SUPPORT_BI_EINT */
#else
	pmic_register_interrupt_callback(INT_ACCDET_EINT0, accdet_eint_int_handler);/* accdet eint0 */
	pmic_register_interrupt_callback(INT_ACCDET_EINT1, accdet_eint_int_handler);/* accdet eint1 */
	ACCDET_INFO("[mt_accdet_probe]CONFIG_ACCDET_SUPPORT_BI_EINT opened!\n");
#endif
#endif

#if 0/* need check, temp delete */
	/* for fix the risk of interrupt trigger before accdet_init when adb reboot */
	/* close top eint and AB interrupt before accdet_init in the first anyhow */
	pmic_pwrap_write(INT_CON_ACCDET_CLR, (0x07<<4));
	pmic_pwrap_write(INT_CON1_ACCDET_CLR, (0x07<<4));
	ACCDET_INFO("[mt_accdet_probe](0x%x)=0x%x\n", INT_CON1_ACCDET, pmic_pwrap_read(INT_CON1_ACCDET));
	ACCDET_INFO("[mt_accdet_probe](0x%x)=0x%x\n", INT_STATUS_ACCDET, pmic_pwrap_read(INT_STATUS_ACCDET));
#endif
	s_eint_accdet_sync_flag = 1;
	/* Accdet Hardware Init */
	ret = accdet_get_dts_data();
	if (ret == 0) {
		#ifdef CONFIG_ACCDET_EINT
		ACCDET_INFO("[mt_accdet_probe]CONFIG_ACCDET_EINT opened!\n");
		accdet_setup_eint(dev);
		#endif
		atomic_set(&s_accdet_first, 1);
		mod_timer(&accdet_init_timer, (jiffies + ACCDET_INIT_WAIT_TIMER));
	} else {
		atomic_set(&s_accdet_first, 0);
		ACCDET_INFO("[mt_accdet_probe]accdet_get_dts_data Failed\n");
	}
	accdet_pmic_Read_Efuse_HPOffset();

	ACCDET_INFO("[mt_accdet_probe]probe done!\n");
	return 0;
}


/*
 * PART7: static  sysfs attr functions for debug
 */
void mt_accdet_remove(void)
{
	ACCDET_DEBUG("[mt_accdet_remove]enter..\n");

	/* cancel_delayed_work(&accdet_work); */
	destroy_workqueue(accdet_eint_workqueue);
	destroy_workqueue(accdet_workqueue);
	device_del(accdet_nor_device);
	class_destroy(accdet_class);
	cdev_del(accdet_cdev);
	unregister_chrdev_region(accdet_devno, 1);
	input_unregister_device(kpd_accdet_dev);
	ACCDET_DEBUG("[mt_accdet_remove]Done!\n");
}


#ifdef CONFIG_PM
/* Delete them as we can get nothing on them but expend CPU source. */
void mt_accdet_suspend(void)/* only one suspend mode */
{
#if defined CONFIG_ACCDET_EINT || defined CONFIG_ACCDET_EINT_IRQ
	ACCDET_DEBUG("[accdet]suspend:[0x%x]=0x%x\n",
		ACCDET_CON12, pmic_pwrap_read(ACCDET_CON12));
#else
	ACCDET_DEBUG("[accdet]suspend:[0x%x]=0x%x,[0x%x]=0x%x(0x%x)\n",
		ACCDET_CON01, pmic_pwrap_read(ACCDET_CON01),
		ACCDET_CON02, pmic_pwrap_read(ACCDET_CON02), s_pre_state_swctrl);
#endif
}

void mt_accdet_resume(void)/* wake up */
{
#if defined CONFIG_ACCDET_EINT || defined CONFIG_ACCDET_EINT_IRQ
	ACCDET_DEBUG("[accdet]resume:[0x%x]=0x%x\n",
		ACCDET_CON12, pmic_pwrap_read(ACCDET_CON12));
#else
	ACCDET_DEBUG("[accdet]resume:[0x%x]=0x%x, [0x%x]=0x%x(0x%x)\n",
		ACCDET_CON01, pmic_pwrap_read(ACCDET_CON01),
		ACCDET_CON02, pmic_pwrap_read(ACCDET_CON02), s_pre_state_swctrl);
#endif
}
#endif

void mt_accdet_pm_restore_noirq(void)
{
	int current_status_restore = 0;
	int tmp_status = 0;

	ACCDET_DEBUG("[accdet_pm_restore_noirq] start -->\n");
	/* enable ACCDET unit */
	/* ACCDET_DEBUG("accdet: enable_accdet\n"); */
	/* enable clock */
	/* pmic_pwrap_write(AUD_TOP_CKPDN_CON0, RG_ACCDET_CK_PDN_B0); */
#ifdef CONFIG_ACCDET_EINT_IRQ
	/* pmic_pwrap_write(TOP_CKPDN_CLR, RG_ACCDET_EINT_IRQ_CLR);//????? */
	/* pmic_pwrap_write(ACCDET_CON00, pmic_pwrap_read(ACCDET_CON00) | RG_ACCDET_INPUT_MICP); */
	/* pmic_pwrap_write(AUDENC_ADC_REG, pmic_pwrap_read(AUDENC_ADC_REG) | ACCDET_SEL_EINT_EN); */
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
	pmic_pwrap_write(ACCDET_CON01, ACCDET_EINT0_EN_B2);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
	pmic_pwrap_write(ACCDET_CON01, ACCDET_EINT1_EN_B4);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
	pmic_pwrap_write(ACCDET_CON01, ACCDET_EINT_EN_B2_4);
#endif
#endif
	enable_accdet(ACCDET_PWM_IDLE_B8_9_10);

#ifdef CONFIG_ACCDET_EINT_IRQ
#ifdef CONFIG_HEADSET_SUPPORT_FIVE_POLE
	/* set PWM IDLE on */
	pmic_pwrap_write(ACCDET_CON02,
		pmic_pwrap_read(ACCDET_CON02)|ACCDET_PWM_IDLE_B8_9_10);
	enable_accdet(ACCDET_SWCTRL_EN);
#else
	/* set PWM IDLE on */
/* pmic_pwrap_write(ACCDET_CON02, (pmic_pwrap_read(ACCDET_CON02)|ACCDET_PWM_IDLE_0)); */
	enable_accdet(ACCDET_PWM_IDLE_B8_9_10);
#endif
#endif

	enable_accdet(ACCDET_PWM_IDLE_B8_9_10);
	s_eint_accdet_sync_flag = 1;
	/* A=bit2; B=bit1;C=bit0 */
	current_status_restore =
		((pmic_pwrap_read(ACCDET_CON14)>>ACCDET_STATE_MEM_BIT_OFFSET)&ACCDET_STATE_ABC_MASK);
	/* current_status_restore = (current_status_restore>>1); */
	ACCDET_DEBUG("[Accdet]accdet_pm_restore_noirq:current ABC:0x%x\n", current_status_restore);
	tmp_status = (current_status_restore>>1)&0x03;

	switch (tmp_status) {
	case 0:		/* AB=0 */
		s_cable_type = HEADSET_NO_MIC;
		s_accdet_status = HOOK_SWITCH;
		send_accdet_status_event(s_cable_type, 1);
		break;
	case 1:		/* AB=1 */
		s_cable_type = HEADSET_MIC;
		s_accdet_status = MIC_BIAS;
		send_accdet_status_event(s_cable_type, 1);
		break;
	case 3:		/* AB=3 */
		send_accdet_status_event(s_cable_type, 0);
		s_cable_type = NO_DEVICE;
		s_accdet_status = PLUG_OUT;
		break;
	default:
		ACCDET_DEBUG("[Accdet]accdet_pm_restore_noirq:error current status:%d!\n", current_status_restore);
		break;
	}
	if (s_cable_type == NO_DEVICE) {
		/* disable accdet */
		s_pre_state_swctrl = pmic_pwrap_read(ACCDET_CON02);
#ifdef CONFIG_ACCDET_EINT
		pmic_pwrap_write(ACCDET_CON02, 0);
		pmic_pwrap_write(ACCDET_CON01, pmic_pwrap_read(ACCDET_CON02)|(~ACCDET_ENABLE_B0));
		/* disable clock */
		pmic_pwrap_write(AUD_TOP_CKPDN_CON0, RG_ACCDET_CK_PDN_B0);/* clock */
#endif
#ifdef CONFIG_ACCDET_EINT_IRQ
		pmic_pwrap_write(ACCDET_CON02, ACCDET_EINT_PWM_EN_B3_4);
		pmic_pwrap_write(ACCDET_CON01, pmic_pwrap_read(ACCDET_CON01) & (~(ACCDET_ENABLE_B0)));
#endif
	}
}

/*//////////////////////////////////IPO_H end/////////////////////////////////////////////*/
long mt_accdet_unlocked_ioctl(unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case ACCDET_INIT:
		break;
	case SET_CALL_STATE:
		s_call_status = (int)arg;
		ACCDET_DEBUG("[Accdet]accdet_ioctl : CALL_STATE=%d\n", s_call_status);
		break;
	case GET_BUTTON_STATUS:
		return s_button_status;
	default:
		ACCDET_DEBUG("[Accdet]accdet_ioctl : default\n");
		break;
	}
	return 0;
}

