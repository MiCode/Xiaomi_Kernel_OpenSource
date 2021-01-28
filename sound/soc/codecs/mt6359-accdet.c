// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Author: Argus Lin <argus.lin@mediatek.com>
 */

#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/input.h>
#include <linux/kthread.h>
#include <linux/io.h>
#include <linux/sched/clock.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/iio/consumer.h>
#include <linux/nvmem-consumer.h>
#include <linux/init.h>
#include <linux/irqdomain.h>
#include <linux/irq.h>
#include <linux/regmap.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/mfd/mt6359/registers.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/mfd/mt6359/core.h>
#include "mt6359-accdet.h"
#include "mt6359.h"
/* grobal variable definitions */
#define REGISTER_VAL(x)	(x - 1)
#define HAS_CAP(_c, _x)	(((_c) & (_x)) == (_x))
#define ACCDET_HW_FAST_DISCHRAGE		BIT(0)
#define ACCDET_PMIC_EINT_IRQ			BIT(1)
#define ACCDET_PMIC_EINT0			BIT(2)
#define ACCDET_PMIC_EINT1			BIT(3)
#define ACCDET_PMIC_BI_EINT			BIT(4)
#define ACCDET_PMIC_GPIO_EINT			BIT(5)
#define ACCDET_PMIC_INVERTER_EINT		BIT(6)
#define ACCDET_AP_GPIO_EINT			BIT(7)
#define ACCDET_THREE_KEY			BIT(8)
#define ACCDET_FOUR_KEY				BIT(9)
#define ACCDET_TRI_KEY_CDD			BIT(10)

/* Used to let accdet know if the pin has been fully plugged-in */
#define EINT_PLUG_OUT				(0)
#define EINT_PLUG_IN				(1)

/* defines for codec accdet registers */
enum accdet_regs {
	TOP_TOP0_ANA_ID,
	TOP_SWCID,
	TOP_RG_RTC32K_CK_PDN,
	TOP_INT_STATUS_AUD_TOP,
	PLT_CPU_INT_STA,
	LDO_RG_LDO_VUSB_HW0_OP_EN,
	ACCDET_AUXADC_RQST_CH5,
	ACCDET_AUXADC_ACCDET_AUTO_SPL,
	ACCDET_RG_ACCDET_CK_PDN,
	ACCDET_RG_ACCDET_RST,
	ACCDET_RG_INT_EN_ACCDET,
	ACCDET_RG_INT_MASK_ACCDET,
	ACCDET_RG_INT_STATUS_ACCDET,
	ACCDET_AUDIO_DIG_ANA_ID,
	ACCDET_RG_NCP_PDDIS_EN,
	ACCDET_RG_AUDPREAMPLON,
	ACCDET_RG_AUDPWDBMICBIAS0,
	ACCDET_RG_AUDPWDBMICBIAS1,
	ACCDET_RG_AUDACCDETMICBIAS0PULLLOW,
	ACCDET_RG_EINT0CONFIGACCDET,
	ACCDET_RG_EINT0HIRENB,
	ACCDET_RG_EINT0NOHYS,
	ACCDET_RG_ACCDET2AUXSWEN,
	ACCDET_RG_EINT1CONFIGACCDET,
	ACCDET_RG_MTEST_EN,
	ACCDET_RG_MTEST_SEL,
	ACCDET_RG_EINTCOMPVTH,
	ACCDET_RG_ANALOGFDEN,
	ACCDET_RG_EINT0CTURBO,
	ACCDET_RG_EINT1CTURBO,
	ACCDET_RG_EINT0EN,
	ACCDET_RG_EINT1EN,
	ACCDET_RG_EINT0CMPEN,
	ACCDET_RG_ACCDETSPARE,
	ACCDET_RG_CLKSQ_EN,
	ACCDET_RG_HPLOUTPUTSTBENH_VAUDP32,
	ACCDET_RG_HPROUTPUTSTBENH_VAUDP32,
	ACCDET_AUDACCDETAUXADCSWCTRL_SW,
	ACCDET_AUDACCDETAUXADCSWCTRL_SEL,
	ACCDET_AUXADC_SEL,
	ACCDET_EINT_M_DETECT_EN,
	ACCDET_EINT0_INVERTER_SW_EN,
	ACCDET_EINT1_INVERTER_SW_EN,
	ACCDET_EINT0_M_SW_EN,
	ACCDET_EINT1_M_SW_EN,
	ACCDET_SEQ_INIT,
	ACCDET_EINT0_SW_EN,
	ACCDET_EINT1_SW_EN,
	ACCDET_SW_EN,
	ACCDET_CMP_PWM_EN,
	ACCDET_PWM_WIDTH,
	ACCDET_PWM_THRESH,
	ACCDET_RISE_DELAY,
	ACCDET_EINT_CMPMEN_PWM_THRESH,
	ACCDET_DEBOUNCE0,
	ACCDET_DEBOUNCE1,
	ACCDET_DEBOUNCE2,
	ACCDET_DEBOUNCE3,
	ACCDET_CONNECT_AUXADC_TIME_DIG,
	ACCDET_EINT_DEBOUNCE0,
	ACCDET_EINT_DEBOUNCE1,
	ACCDET_EINT_DEBOUNCE2,
	ACCDET_EINT_DEBOUNCE3,
	ACCDET_EINT_INVERTER_DEBOUNCE,
	ACCDET_IRQ,
	ACCDET_EINT_M_PLUG_IN_NUM,
	ACCDET_DA_STABLE,
	ACCDET_HWMODE_EN,
	ACCDET_CMPEN_SEL,
	ACCDET_EINT_CMPMOUT_SEL,
	ACCDET_EINT_CMPMEN_SEL,
	ACCDET_EINT_CTURBO_SEL,
	ACCDET_EINT0_CTURBO_SW,
	ACCDET_CMPEN_SW,
	ACCDET_AD_AUDACCDETCMPOB,
	ACCDET_MEM_IN,
	ACCDET_AD_EINT0CMPMOUT,
	ACCDET_EINT0_MEM_IN,
	ACCDET_AD_EINT0INVOUT,
	ACCDET_EN,
	ACCDET_MON_FLAG_EN,
};

static const u32 mt6359_aud_regs[] = {
	[TOP_TOP0_ANA_ID] =			0x0000,
	[TOP_SWCID] =				0x000a,
	[TOP_RG_RTC32K_CK_PDN] =		0x010c,
	[TOP_INT_STATUS_AUD_TOP] =		0x019e,
	[PLT_CPU_INT_STA] =			0x0452,
	[LDO_RG_LDO_VUSB_HW0_OP_EN] =		0x1d0c,
	[ACCDET_AUXADC_RQST_CH5] =		0x1108,
	[ACCDET_AUXADC_ACCDET_AUTO_SPL] =	0x11ba,
	[ACCDET_RG_ACCDET_CK_PDN] =		0x230c,
	[ACCDET_RG_ACCDET_RST] =		0x2320,
	[ACCDET_RG_INT_EN_ACCDET] =		0x2328,
	[ACCDET_RG_INT_MASK_ACCDET] =		0x232e,
	[ACCDET_RG_INT_STATUS_ACCDET] =		0x2334,
	[ACCDET_AUDIO_DIG_ANA_ID] =		0x2380,
	[ACCDET_RG_NCP_PDDIS_EN] =		0x24e2,
	[ACCDET_RG_AUDPREAMPLON] =		0x2508,
	[ACCDET_RG_AUDPWDBMICBIAS0] =		0x2526,
	[ACCDET_RG_AUDPWDBMICBIAS1] =		0x2528,
	[ACCDET_RG_AUDACCDETMICBIAS0PULLLOW] =	0x252c,
	[ACCDET_RG_EINT0CONFIGACCDET] =		0x252c,
	[ACCDET_RG_EINT0HIRENB] =		0x252c,
	[ACCDET_RG_EINT0NOHYS] =		0x252c,
	[ACCDET_RG_ACCDET2AUXSWEN] =		0x252c,
	[ACCDET_RG_EINT1CONFIGACCDET] =		0x252e,
	[ACCDET_RG_MTEST_EN] =			0x252e,
	[ACCDET_RG_MTEST_SEL] =			0x252e,
	[ACCDET_RG_EINTCOMPVTH] =		0x252e,
	[ACCDET_RG_ANALOGFDEN] =		0x252e,
	[ACCDET_RG_EINT0CTURBO] =		0x2530,
	[ACCDET_RG_EINT1CTURBO] =		0x2530,
	[ACCDET_RG_EINT0EN] =			0x2530,
	[ACCDET_RG_EINT1EN] =			0x2530,
	[ACCDET_RG_EINT0CMPEN] =		0x2530,
	[ACCDET_RG_ACCDETSPARE] =		0x2532,
	[ACCDET_RG_CLKSQ_EN] =			0x2536,
	[ACCDET_RG_HPLOUTPUTSTBENH_VAUDP32] =	0x258c,
	[ACCDET_RG_HPROUTPUTSTBENH_VAUDP32] =	0x258c,
	[ACCDET_AUDACCDETAUXADCSWCTRL_SW] =	0x2688,
	[ACCDET_AUDACCDETAUXADCSWCTRL_SEL] =	0x2688,
	[ACCDET_AUXADC_SEL] =			0x2688,
	[ACCDET_EINT_M_DETECT_EN] =		0x268a,
	[ACCDET_EINT0_INVERTER_SW_EN] =		0x268a,
	[ACCDET_EINT1_INVERTER_SW_EN] =		0x268a,
	[ACCDET_EINT0_M_SW_EN] =		0x268a,
	[ACCDET_EINT1_M_SW_EN] =		0x268a,
	[ACCDET_SEQ_INIT] =			0x268a,
	[ACCDET_EINT0_SW_EN] =			0x268a,
	[ACCDET_EINT1_SW_EN] =			0x268a,
	[ACCDET_SW_EN] =			0x268a,
	[ACCDET_CMP_PWM_EN] =			0x268c,
	[ACCDET_PWM_WIDTH] =			0x268e,
	[ACCDET_PWM_THRESH] =			0x2690,
	[ACCDET_RISE_DELAY] =			0x2692,
	[ACCDET_EINT_CMPMEN_PWM_THRESH] =	0x2694,
	[ACCDET_DEBOUNCE0] =			0x2698,
	[ACCDET_DEBOUNCE1] =			0x269a,
	[ACCDET_DEBOUNCE2] =			0x269c,
	[ACCDET_DEBOUNCE3] =			0x269e,
	[ACCDET_CONNECT_AUXADC_TIME_DIG] =	0x26a0,
	[ACCDET_EINT_DEBOUNCE0] =		0x26a4,
	[ACCDET_EINT_DEBOUNCE1] =		0x26a4,
	[ACCDET_EINT_DEBOUNCE2] =		0x26a4,
	[ACCDET_EINT_DEBOUNCE3] =		0x26a4,
	[ACCDET_EINT_INVERTER_DEBOUNCE] =	0x26a6,
	[ACCDET_IRQ] =				0x26ac,
	[ACCDET_EINT_M_PLUG_IN_NUM] =		0x26ac,
	[ACCDET_DA_STABLE] =			0x26ae,
	[ACCDET_HWMODE_EN] =			0x26b0,
	[ACCDET_CMPEN_SEL] =			0x26b4,
	[ACCDET_EINT_CMPMOUT_SEL] =		0x26b4,
	[ACCDET_EINT_CMPMEN_SEL] =		0x26b4,
	[ACCDET_EINT_CTURBO_SEL] =		0x26b4,
	[ACCDET_EINT0_CTURBO_SW] =		0x26b6,
	[ACCDET_CMPEN_SW] =			0x26b6,
	[ACCDET_AD_AUDACCDETCMPOB] =		0x26ba,
	[ACCDET_MEM_IN] =			0x26ba,
	[ACCDET_AD_EINT0CMPMOUT] =		0x26bc,
	[ACCDET_EINT0_MEM_IN] =			0x26bc,
	[ACCDET_AD_EINT0INVOUT] =		0x26c0,
	[ACCDET_EN] =				0x26c4,
	[ACCDET_MON_FLAG_EN] =			0x26d8,
};

struct mt63xx_accdet_data {
	u32 base;
	struct snd_soc_card card;
	struct snd_soc_jack jack;
	struct platform_device *pdev;
	struct device *dev;
	struct accdet_priv *data;
	atomic_t init_once;
	struct regmap *regmap;
	struct iio_channel *accdet_auxadc;
	struct nvmem_device *accdet_efuse;
	int accdet_irq;
	int accdet_eint0;
	int accdet_eint1;
	struct wakeup_source *wake_lock;
	struct wakeup_source *timer_lock;
	struct mutex res_lock;
	dev_t accdet_devno;
	struct class *accdet_class;
	int button_status;
	bool eint_sync_flag;
	/* accdet FSM State & lock*/
	u32 cur_eint_state;
	u32 eint0_state;
	u32 eint1_state;
	u32 accdet_status;
	u32 cable_type;
	u32 cur_key;
	u32 cali_voltage;
	int auxadc_offset;
	u32 eint_id;
	bool thing_in_flag;
	/* when caps include ACCDET_AP_GPIO_EINT */
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_eint;
	u32 gpiopin;
	u32 gpio_hp_deb;
	u32 gpioirq;
	u32 accdet_eint_type;
	/* when MICBIAS_DISABLE_TIMER timeout, queue work: dis_micbias_work */
	struct work_struct dis_micbias_work;
	struct workqueue_struct *dis_micbias_workqueue;
	struct work_struct accdet_work;
	struct workqueue_struct *accdet_workqueue;
	/* when eint issued, queue work: eint_work */
	struct work_struct eint_work;
	struct workqueue_struct *eint_workqueue;
};
static struct mt63xx_accdet_data *accdet;

static struct head_dts_data accdet_dts;
struct pwm_deb_settings *cust_pwm_deb;

struct accdet_priv {
	const u32 *regs;
	u32 caps;
	struct snd_card *snd_card;
};

static struct accdet_priv mt6359_accdet[] = {
	{
		.regs = mt6359_aud_regs,
		.caps = ACCDET_PMIC_EINT_IRQ | ACCDET_PMIC_EINT0
			| ACCDET_PMIC_INVERTER_EINT | ACCDET_THREE_KEY,
	},
};

const struct of_device_id accdet_of_match[] = {
	{
		.compatible = "mediatek,mt6359-accdet",
		.data = &mt6359_accdet,
	}, {
		.compatible = "mediatek,mt8163-accdet",
	}, {
		.compatible = "mediatek,mt8173-accdet",
	}, {
		/* sentinel */
	},
};

static struct snd_soc_jack_pin accdet_jack_pins[] = {
	{
		.pin = "Headset",
		.mask = SND_JACK_HEADSET |
			SND_JACK_LINEOUT |
			SND_JACK_MECHANICAL,
	},
};


/* micbias_timer: disable micbias if no accdet irq after eint,
 * timeout: 6 seconds
 * timerHandler: dis_micbias_timerhandler()
 */
#define MICBIAS_DISABLE_TIMER (6 * HZ)
static struct timer_list micbias_timer;
static void dis_micbias_timerhandler(struct timer_list *t);
static bool dis_micbias_done;

static u32 button_press_debounce = 0x400;

/* local function declaration */
static void accdet_init_once(void);
static inline void accdet_init(void);
static void accdet_init_debounce(void);
static u32 adjust_eint_analog_setting(void);
static u32 adjust_eint_setting(u32 eintsts);
static void config_digital_init_by_mode(void);
static void config_eint_init_by_mode(void);
static u32 get_triggered_eint(void);
static void recover_eint_analog_setting(void);
static void recover_eint_digital_setting(void);
static void recover_eint_setting(u32 eintsts);
static void send_status_event(u32 cable_type, u32 status);
/* global function declaration */
inline u32 accdet_read(enum accdet_regs addr)
{
	u32 val = 0;

	if (accdet->regmap) {
		regmap_read(accdet->regmap,
			accdet->base + accdet->data->regs[addr], &val);
	} else
		pr_notice("%s %d Error.\n", __func__, __LINE__);

	return val;
}

inline u32 accdet_read_bits(enum accdet_regs addr, u32 shift, u32 mask)
{
	u32 val = 0;

	val = accdet_read(addr);
	return ((val>>shift) & mask);
}

inline void accdet_write(enum accdet_regs addr, u32 wdata)
{
	if (accdet->regmap) {
		regmap_write(accdet->regmap,
			accdet->base + accdet->data->regs[addr], wdata);
	} else
		pr_notice("%s %d Error.\n", __func__, __LINE__);
}

inline void accdet_update_bits(enum accdet_regs addr, u32 shift,
			u32 mask, u32 data)
{
	regmap_update_bits(accdet->regmap,
		accdet->base + accdet->data->regs[addr],
		mask << shift,
		data << shift);
}

inline void accdet_update_bit(enum accdet_regs addr, unsigned int shift)
{
	unsigned int mask = shift;

	regmap_update_bits(accdet->regmap,
		accdet->base + accdet->data->regs[addr],
		BIT(mask),
		BIT(shift));
}

inline void accdet_clear_bits(enum accdet_regs addr, unsigned int shift,
			unsigned int mask, unsigned int data)
{
	regmap_update_bits(accdet->regmap,
		accdet->base + accdet->data->regs[addr],
		mask << shift,
		~(data << shift));
}

inline void accdet_clear_bit(enum accdet_regs addr, unsigned int shift)
{
	unsigned int mask = shift;

	regmap_update_bits(accdet->regmap,
		accdet->base + accdet->data->regs[addr],
		BIT(mask),
		~(BIT(shift)));
}

static u64 accdet_get_current_time(void)
{
	return sched_clock();
}

static bool accdet_timeout_ns(u64 start_time_ns, u64 timeout_time_ns)
{
	u64 cur_time = 0;
	u64 elapse_time = 0;

	/* get current tick, ns */
	cur_time = accdet_get_current_time();
	if (cur_time < start_time_ns) {
		start_time_ns = cur_time;
		/* 400us */
		timeout_time_ns = 400 * 1000;
	}
	elapse_time = cur_time - start_time_ns;

	/* check if timeout */
	if (timeout_time_ns <= elapse_time)
		return false;

	return true;
}

static u32 accdet_get_auxadc(void)
{
	int vol = 0, ret = 0;

	if (!IS_ERR(accdet->accdet_auxadc)) {
		ret = iio_read_channel_processed(accdet->accdet_auxadc, &vol);
		if (ret < 0) {
			pr_notice("Error: %s read fail (%d)\n", __func__, ret);
			return ret;
		}
	}

	pr_info("%s() vol_val:%d offset:%d real vol:%d mv!\n", __func__, vol,
		accdet->auxadc_offset,
	(vol < accdet->auxadc_offset) ? 0 : (vol-accdet->auxadc_offset));

	if (vol < accdet->auxadc_offset)
		vol = 0;
	else
		vol -= accdet->auxadc_offset;

	return vol;
}

static void accdet_get_efuse(void)
{
	unsigned short efuseval = 0;
	int ret = 0;

	/* accdet offset efuse:
	 * this efuse must divided by 2
	 */
	ret = nvmem_device_read(accdet->accdet_efuse, 102*2, 2, &efuseval);
	accdet->auxadc_offset = efuseval & 0xFF;
	if (accdet->auxadc_offset > 128)
		accdet->auxadc_offset -= 256;
	accdet->auxadc_offset = (accdet->auxadc_offset >> 1);
	pr_info("%s efuse=0x%x,auxadc_val=%dmv\n", __func__, efuseval,
		accdet->auxadc_offset);
}

static void accdet_get_efuse_4key(void)
{
	unsigned short tmp_val = 0;
	unsigned short tmp_8bit = 0;
	int ret = 0;

	/* 4-key efuse:
	 * bit[9:2] efuse value is loaded, so every read out value need to be
	 * left shift 2 bit,and then compare with voltage get from AUXADC.
	 * AD efuse: key-A Voltage:0--AD;
	 * DB efuse: key-D Voltage: AD--DB;
	 * BC efuse: key-B Voltage:DB--BC;
	 * key-C Voltage: BC--600;
	 */
	ret = nvmem_device_read(accdet->accdet_efuse, 103*2, 2, &tmp_val);
	tmp_8bit = tmp_val & ACCDET_CALI_MASK0;
	accdet_dts.four_key.mid = tmp_8bit << 2;

	tmp_8bit = (tmp_val >> 8) & ACCDET_CALI_MASK0;
	accdet_dts.four_key.voice = tmp_8bit << 2;

	ret = nvmem_device_read(accdet->accdet_efuse, 104*2, 2, &tmp_val);
	tmp_8bit = tmp_val & ACCDET_CALI_MASK0;
	accdet_dts.four_key.up = tmp_8bit << 2;

	accdet_dts.four_key.down = 600;
	pr_info("accdet key thresh: mid=%dmv,voice=%dmv,up=%dmv,down=%dmv\n",
		accdet_dts.four_key.mid, accdet_dts.four_key.voice,
		accdet_dts.four_key.up, accdet_dts.four_key.down);
}

static u32 key_check(u32 v)
{
	if (HAS_CAP(accdet->data->caps, ACCDET_FOUR_KEY)) {
		if ((v < accdet_dts.four_key.down) &&
			(v >= accdet_dts.four_key.up))
			return DW_KEY;
		if ((v < accdet_dts.four_key.up) &&
			(v >= accdet_dts.four_key.voice))
			return UP_KEY;
		if ((v < accdet_dts.four_key.voice) &&
			(v >= accdet_dts.four_key.mid))
			return AS_KEY;
		if (v < accdet_dts.four_key.mid)
			return MD_KEY;
	} else {
		if ((v < accdet_dts.three_key.down) &&
			(v >= accdet_dts.three_key.up))
			return DW_KEY;
		if ((v < accdet_dts.three_key.up) &&
			(v >= accdet_dts.three_key.mid))
			return UP_KEY;
		if (v < accdet_dts.three_key.mid)
			return MD_KEY;
	}
	return NO_KEY;
}

static void send_key_event(u32 keycode, u32 flag)
{
	int report = 0;

	switch (keycode) {
	case DW_KEY:
		if (flag != 0)
			report = SND_JACK_BTN_1;
		snd_soc_jack_report(&accdet->jack, report,
				SND_JACK_BTN_1);
		break;
	case UP_KEY:
		if (flag != 0)
			report = SND_JACK_BTN_2;
		snd_soc_jack_report(&accdet->jack, report,
				SND_JACK_BTN_2);
		break;
	case MD_KEY:
		if (flag != 0)
			report = SND_JACK_BTN_0;
		snd_soc_jack_report(&accdet->jack, report,
				SND_JACK_BTN_0);
		break;
	case AS_KEY:
		if (flag != 0)
			report = SND_JACK_BTN_3;
		snd_soc_jack_report(&accdet->jack, report,
				SND_JACK_BTN_3);
		break;
	}
}

static void send_status_event(u32 cable_type, u32 status)
{
	int report = 0;

	switch (cable_type) {
	case HEADSET_NO_MIC:
		if (status)
			report = SND_JACK_HEADPHONE;
		else
			report = 0;
		snd_soc_jack_report(&accdet->jack, report,
				SND_JACK_HEADPHONE);
		/* when plug 4-pole out, if both AB=3 AB=0 happen,3-pole plug
		 * in will be incorrectly reported, then 3-pole plug-out is
		 * reported,if no mantory 4-pole plug-out, icon would be
		 * visible.
		 */
		if (status == 0) {
			report = 0;
			snd_soc_jack_report(&accdet->jack, report,
					SND_JACK_MICROPHONE);
		}
		pr_info("accdet HEADPHONE(3-pole) %s\n",
			status ? "PlugIn" : "PlugOut");
		break;
	case HEADSET_MIC:
		/* when plug 4-pole out, 3-pole plug out should also be
		 * reported for slow plug-in case
		 */
		if (status == 0) {
			report = 0;
			snd_soc_jack_report(&accdet->jack, report,
					SND_JACK_HEADPHONE);
		}
		if (status)
			report = SND_JACK_MICROPHONE;
		else
			report = 0;

		snd_soc_jack_report(&accdet->jack, report,
				SND_JACK_MICROPHONE);
		pr_info("accdet MICROPHONE(4-pole) %s\n",
			status ? "PlugIn" : "PlugOut");
		break;
	case LINE_OUT_DEVICE:
		if (status)
			report = SND_JACK_LINEOUT;
		else
			report = 0;

		snd_soc_jack_report(&accdet->jack, report,
				SND_JACK_LINEOUT);
		pr_info("accdet LineOut %s\n",
			status ? "PlugIn" : "PlugOut");
		break;
	default:
		pr_info("%s Invalid cableType\n", __func__);
	}
}

static void multi_key_detection(u32 cur_AB)
{
	if (cur_AB == ACCDET_STATE_AB_00)
		accdet->cur_key = key_check(accdet->cali_voltage);

	/* delay to fix side effect key when plug-out, when plug-out,seldom
	 * issued AB=0 and Eint, delay to wait eint been flaged in register.
	 * or eint handler issued. accdet->cur_eint_state == PLUG_OUT
	 */
	usleep_range(10000, 12000);

	if (HAS_CAP(accdet->data->caps, ACCDET_AP_GPIO_EINT)) {
		if (accdet->cur_eint_state == EINT_PLUG_IN)
			send_key_event(accdet->cur_key, !cur_AB);
		else
			accdet->cur_key = NO_KEY;
	} else {
		bool irq_bit;

		irq_bit = !(accdet_read(ACCDET_IRQ) & ACCDET_EINT_IRQ_B2_B3);
		/* send key when:
		 * no eint is flaged in reg, and now eint PLUG_IN
		 */
		if (irq_bit && (accdet->cur_eint_state == EINT_PLUG_IN))
			send_key_event(accdet->cur_key, !cur_AB);
		else
			accdet->cur_key = NO_KEY;
	}

	if (cur_AB)
		accdet->cur_key = NO_KEY;
}

static inline void clear_accdet_int(void)
{
	/* it is safe by using polling to adjust when to clear IRQ_CLR_BIT */
	accdet_update_bit(ACCDET_IRQ, PMIC_ACCDET_IRQ_CLR_SHIFT);
}

static inline void clear_accdet_int_check(void)
{
	u64 cur_time = accdet_get_current_time();

	while ((accdet_read(ACCDET_IRQ) & ACCDET_IRQ_B0) &&
		(accdet_timeout_ns(cur_time, ACCDET_TIME_OUT)))
		;
	/* clear accdet int, modify  for fix interrupt trigger twice error */
	accdet_clear_bit(ACCDET_IRQ, PMIC_ACCDET_IRQ_CLR_SHIFT);
	accdet_update_bit(ACCDET_RG_INT_STATUS_ACCDET,
		PMIC_RG_INT_STATUS_ACCDET_SHIFT);
}

static inline void clear_accdet_eint(u32 eintid)
{
	if ((eintid & PMIC_EINT0) == PMIC_EINT0) {
		accdet_update_bit(ACCDET_IRQ,
			PMIC_ACCDET_EINT0_IRQ_CLR_SHIFT);
	}
	if ((eintid & PMIC_EINT1) == PMIC_EINT1) {
		accdet_update_bit(ACCDET_IRQ,
			PMIC_ACCDET_EINT1_IRQ_CLR_SHIFT);
	}

}

static inline void clear_accdet_eint_check(u32 eintid)
{
	u64 cur_time = accdet_get_current_time();

	if ((eintid & PMIC_EINT0) == PMIC_EINT0) {
		while ((accdet_read(ACCDET_IRQ) & ACCDET_EINT0_IRQ_B2)
			&& (accdet_timeout_ns(cur_time, ACCDET_TIME_OUT)))
			;
		accdet_clear_bit(ACCDET_IRQ,
				PMIC_ACCDET_EINT0_IRQ_CLR_SHIFT);
		accdet_update_bit(ACCDET_RG_INT_STATUS_ACCDET,
				PMIC_RG_INT_STATUS_ACCDET_EINT0_SHIFT);
	}
	if ((eintid & PMIC_EINT1) == PMIC_EINT1) {
		while ((accdet_read(ACCDET_IRQ) & ACCDET_EINT1_IRQ_B3)
			&& (accdet_timeout_ns(cur_time, ACCDET_TIME_OUT)))
			;
		accdet_clear_bit(ACCDET_IRQ,
				PMIC_ACCDET_EINT1_IRQ_CLR_SHIFT);
		accdet_update_bit(ACCDET_RG_INT_STATUS_ACCDET,
				PMIC_RG_INT_STATUS_ACCDET_EINT1_SHIFT);
	}
}

static u32 adjust_eint_analog_setting(void)
{
	if (accdet_dts.eint_detect_mode == 0x4) {
		/* ESD switches off */
		accdet_clear_bit(ACCDET_RG_ACCDETSPARE, 8);
	}
	if (accdet_dts.eint_detect_mode == 0x4) {
		if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT0)) {
			/* enable RG_EINT0CONFIGACCDET */
			accdet_update_bit(ACCDET_RG_EINT0CONFIGACCDET,
				PMIC_RG_EINT0CONFIGACCDET_SHIFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT1)) {
			/* enable RG_EINT1CONFIGACCDET */
			accdet_update_bit(ACCDET_RG_EINT1CONFIGACCDET,
				PMIC_RG_EINT1CONFIGACCDET_SHIFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_BI_EINT)) {
			/* enable RG_EINT0CONFIGACCDET */
			accdet_update_bit(ACCDET_RG_EINT0CONFIGACCDET,
				PMIC_RG_EINT0CONFIGACCDET_SHIFT);
			/* enable RG_EINT1CONFIGACCDET */
			accdet_update_bit(ACCDET_RG_EINT1CONFIGACCDET,
				PMIC_RG_EINT1CONFIGACCDET_SHIFT);
		}
	}
	return 0;
}

static u32 adjust_eint_digital_setting(void)
{
	if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT0)) {
		/* disable inverter */
		accdet_clear_bit(ACCDET_EINT0_INVERTER_SW_EN,
			PMIC_ACCDET_EINT0_INVERTER_SW_EN_SHIFT);
	} else if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT1)) {
		/* disable inverter */
		accdet_clear_bit(ACCDET_EINT1_INVERTER_SW_EN,
			PMIC_ACCDET_EINT1_INVERTER_SW_EN_SHIFT);
	} else if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_BI_EINT)) {
		/* disable inverter */
		accdet_clear_bit(ACCDET_EINT0_INVERTER_SW_EN,
			PMIC_ACCDET_EINT0_INVERTER_SW_EN_SHIFT);
		/* disable inverter */
		accdet_clear_bit(ACCDET_EINT1_INVERTER_SW_EN,
			PMIC_ACCDET_EINT1_INVERTER_SW_EN_SHIFT);
	}

	if (accdet_dts.eint_detect_mode == 0x4) {
		if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT0)) {
			/* set DA stable signal */
			accdet_clear_bit(ACCDET_DA_STABLE,
				PMIC_ACCDET_EINT0_CEN_STABLE_SHIFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT1)) {
			/* set DA stable signal */
			accdet_clear_bit(ACCDET_DA_STABLE,
				PMIC_ACCDET_EINT1_CEN_STABLE_SHIFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_BI_EINT)) {
			/* set DA stable signal */
			accdet_clear_bit(ACCDET_DA_STABLE,
				PMIC_ACCDET_EINT0_CEN_STABLE_SHIFT);
			/* set DA stable signal */
			accdet_clear_bit(ACCDET_DA_STABLE,
				PMIC_ACCDET_EINT1_CEN_STABLE_SHIFT);
		}
	}
	return 0;
}

static u32 adjust_eint_setting(u32 eintsts)
{

	if (eintsts == M_PLUG_IN) {
		/* adjust digital setting */
		adjust_eint_digital_setting();
		/* adjust analog setting */
		adjust_eint_analog_setting();
	} else if (eintsts == M_PLUG_OUT) {
		/* set debounce to 1ms */
		accdet_set_debounce(eint_state000,
			accdet_dts.pwm_deb.eint_debounce0);
	}

	return 0;
}

static void recover_eint_analog_setting(void)
{
	if (accdet_dts.eint_detect_mode == 0x4) {
		/* ESD switches on */
		accdet_update_bit(ACCDET_RG_ACCDETSPARE, 8);

		if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT0)) {
			/* disable RG_EINT0CONFIGACCDET */
			accdet_clear_bit(ACCDET_RG_EINT0CONFIGACCDET,
				PMIC_RG_EINT0CONFIGACCDET_SHIFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT1)) {
			/* disable RG_EINT1CONFIGACCDET */
			accdet_clear_bit(ACCDET_RG_EINT1CONFIGACCDET,
				PMIC_RG_EINT1CONFIGACCDET_SHIFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_BI_EINT)) {
			/* disable RG_EINT0CONFIGACCDET */
			accdet_clear_bit(ACCDET_RG_EINT0CONFIGACCDET,
				PMIC_RG_EINT0CONFIGACCDET_SHIFT);
			/* disable RG_EINT0CONFIGACCDET */
			accdet_clear_bit(ACCDET_RG_EINT1CONFIGACCDET,
				PMIC_RG_EINT1CONFIGACCDET_SHIFT);
		}
		accdet_clear_bit(ACCDET_RG_EINT0HIRENB,
			PMIC_RG_EINT0HIRENB_SHIFT);
	}

}
static void recover_eint_digital_setting(void)
{
	if (HAS_CAP(accdet->data->caps,
			ACCDET_PMIC_EINT0)) {
		accdet_clear_bit(ACCDET_EINT0_M_SW_EN,
			PMIC_ACCDET_EINT0_M_SW_EN_SHIFT);
	} else if (HAS_CAP(accdet->data->caps,
			ACCDET_PMIC_EINT1)) {
		accdet_clear_bit(ACCDET_EINT1_M_SW_EN,
			PMIC_ACCDET_EINT1_M_SW_EN_SHIFT);
	} else if (HAS_CAP(accdet->data->caps,
			ACCDET_PMIC_BI_EINT)) {
		accdet_clear_bit(ACCDET_EINT0_M_SW_EN,
			PMIC_ACCDET_EINT0_M_SW_EN_SHIFT);
		accdet_clear_bit(ACCDET_EINT1_M_SW_EN,
			PMIC_ACCDET_EINT1_M_SW_EN_SHIFT);
	}
	if (accdet_dts.eint_detect_mode == 0x4) {
		/* enable eint0cen */
		if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT0)) {
			/* enable eint0cen */
			accdet_update_bit(ACCDET_DA_STABLE,
				PMIC_ACCDET_EINT0_CEN_STABLE_SHIFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT1)) {
			/* enable eint1cen */
			accdet_update_bit(ACCDET_DA_STABLE,
				PMIC_ACCDET_EINT1_CEN_STABLE_SHIFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_BI_EINT)) {
			/* enable eint0cen */
			accdet_update_bit(ACCDET_DA_STABLE,
				PMIC_ACCDET_EINT0_CEN_STABLE_SHIFT);
			/* enable eint1cen */
			accdet_update_bit(ACCDET_DA_STABLE,
				PMIC_ACCDET_EINT1_CEN_STABLE_SHIFT);
		}
	}

	if (accdet_dts.eint_detect_mode != 0x1) {
		if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT0)) {
			/* enable inverter */
			accdet_update_bit(ACCDET_EINT0_INVERTER_SW_EN,
				PMIC_ACCDET_EINT0_INVERTER_SW_EN_SHIFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT1)) {
			/* enable inverter */
			accdet_update_bit(ACCDET_EINT1_INVERTER_SW_EN,
				PMIC_ACCDET_EINT1_INVERTER_SW_EN_SHIFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_BI_EINT)) {
			/* enable inverter */
			accdet_update_bit(ACCDET_EINT0_INVERTER_SW_EN,
				PMIC_ACCDET_EINT0_INVERTER_SW_EN_SHIFT);
			/* enable inverter */
			accdet_update_bit(ACCDET_EINT1_INVERTER_SW_EN,
				PMIC_ACCDET_EINT1_INVERTER_SW_EN_SHIFT);
		}
	}
}

static void recover_eint_setting(u32 eintsts)
{
	if (eintsts == M_PLUG_OUT) {
		recover_eint_analog_setting();
		recover_eint_digital_setting();
	}
}

static u32 get_triggered_eint(void)
{
	u32 eint_ID = NO_PMIC_EINT;
	u32 irq_status = accdet_read(ACCDET_IRQ);

	if (HAS_CAP(accdet->data->caps,
			ACCDET_PMIC_EINT0)) {
		if ((irq_status & ACCDET_EINT0_IRQ_B2) == ACCDET_EINT0_IRQ_B2)
			eint_ID = PMIC_EINT0;
	} else if (HAS_CAP(accdet->data->caps,
			ACCDET_PMIC_EINT1)) {
		if ((irq_status & ACCDET_EINT1_IRQ_B3) == ACCDET_EINT1_IRQ_B3)
			eint_ID = PMIC_EINT1;
	} else if (HAS_CAP(accdet->data->caps,
			ACCDET_PMIC_BI_EINT)) {
		if ((irq_status & ACCDET_EINT0_IRQ_B2) == ACCDET_EINT0_IRQ_B2)
			eint_ID |= PMIC_EINT0;
		if ((irq_status & ACCDET_EINT1_IRQ_B3) == ACCDET_EINT1_IRQ_B3)
			eint_ID |= PMIC_EINT1;
	}
	return eint_ID;
}

static inline void enable_accdet(u32 state_swctrl)
{
	/* enable ACCDET unit */
	accdet_update_bit(ACCDET_SW_EN, PMIC_ACCDET_SW_EN_SHIFT);
}

static inline void disable_accdet(void)
{
	/* sync with accdet_irq_handler set clear accdet irq bit to avoid to
	 * set clear accdet irq bit after disable accdet disable accdet irq
	 */
	clear_accdet_int();
	udelay(200);
	mutex_lock(&accdet->res_lock);
	clear_accdet_int_check();
	mutex_unlock(&accdet->res_lock);

	/* recover accdet debounce0,3 */
	accdet_set_debounce(accdet_state000, cust_pwm_deb->debounce0);
	accdet_set_debounce(accdet_state011, cust_pwm_deb->debounce3);
}

static inline void headset_plug_out(void)
{
	send_status_event(accdet->cable_type, 0);
	accdet->accdet_status = PLUG_OUT;
	accdet->cable_type = NO_DEVICE;

	if (accdet->cur_key != 0) {
		send_key_event(accdet->cur_key, 0);
		accdet->cur_key = 0;
	}
	dis_micbias_done = false;
	pr_info("accdet %s, set cable_type = NO_DEVICE %d\n", __func__,
		dis_micbias_done);
}
static void dis_micbias_timerhandler(struct timer_list *t)
{
	int ret = 0;

	ret = queue_work(accdet->dis_micbias_workqueue,
			&accdet->dis_micbias_work);
	if (!ret)
		pr_notice("Error: %s (%d)\n", __func__, ret);
}

static void dis_micbias_work_callback(struct work_struct *work)
{
	u32 cur_AB, eintID;

	/* check EINT0 status, if plug out,
	 * not need to disable accdet here
	 */
	eintID = accdet_read_bits(ACCDET_EINT0_MEM_IN,
		PMIC_ACCDET_EINT0_MEM_IN_SHIFT,
		PMIC_ACCDET_EINT0_MEM_IN_MASK);
	if (eintID == M_PLUG_OUT) {
		pr_notice("%s Plug-out, no dis micbias\n", __func__);
		return;
	}
	/* if modify_vref_volt called, not need to dis micbias again */
	if (dis_micbias_done == true) {
		pr_notice("%s modify_vref_volt called\n", __func__);
		return;
	}

	cur_AB = accdet_read(ACCDET_MEM_IN) >> ACCDET_STATE_MEM_IN_OFFSET;
		cur_AB = cur_AB & ACCDET_STATE_AB_MASK;

	/* if 3pole disable accdet
	 * if <20k + 4pole, disable accdet will disable accdet
	 * plug out interrupt. The behavior will same as 3pole
	 */
	if ((accdet->cable_type == HEADSET_NO_MIC) ||
		(cur_AB == ACCDET_STATE_AB_00) ||
		(cur_AB == ACCDET_STATE_AB_11)) {
		/* disable accdet_sw_en=0
		 * disable accdet_hwmode_en=0
		 */
		accdet_clear_bit(ACCDET_SW_EN,
			PMIC_ACCDET_SW_EN_SHIFT);
		disable_accdet();
	}
}

static void eint_work_callback(struct work_struct *work)
{
	if (accdet->cur_eint_state == EINT_PLUG_IN) {
		/* disable vusb LP */
		accdet_write(LDO_RG_LDO_VUSB_HW0_OP_EN, 0x8000);

		mutex_lock(&accdet->res_lock);
		accdet->eint_sync_flag = true;
		mutex_unlock(&accdet->res_lock);
		__pm_wakeup_event(accdet->timer_lock,
			jiffies_to_msecs(7 * HZ));

		accdet_init();

		enable_accdet(0);
	} else {
		mutex_lock(&accdet->res_lock);
		accdet->eint_sync_flag = false;
		accdet->thing_in_flag = false;
		mutex_unlock(&accdet->res_lock);
		del_timer_sync(&micbias_timer);

		/* disable accdet_sw_en=0
		 */
		accdet_clear_bit(ACCDET_SW_EN,
			PMIC_ACCDET_SW_EN_SHIFT);
		disable_accdet();
		headset_plug_out();
	}

	if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT_IRQ))
		recover_eint_setting(accdet->eint_id);
	else if (HAS_CAP(accdet->data->caps, ACCDET_AP_GPIO_EINT))
		enable_irq(accdet->gpioirq);
}

void accdet_set_debounce(int state, unsigned int debounce)
{
	switch (state) {
	case accdet_state000:
		/* set ACCDET debounce value = debounce/32 ms */
		accdet_write(ACCDET_DEBOUNCE0, debounce);
		break;
	case accdet_state001:
		accdet_write(ACCDET_DEBOUNCE1, debounce);
		break;
	case accdet_state010:
		accdet_write(ACCDET_DEBOUNCE2, debounce);
		break;
	case accdet_state011:
		accdet_write(ACCDET_DEBOUNCE3, debounce);
		break;
	case accdet_auxadc:
		/* set auxadc debounce:0x42(2ms) */
		accdet_write(ACCDET_CONNECT_AUXADC_TIME_DIG, debounce);
		break;
	case eint_state000:
		accdet_update_bits(ACCDET_EINT_DEBOUNCE0,
				PMIC_ACCDET_EINT_DEBOUNCE0_SHIFT,
				PMIC_ACCDET_EINT_DEBOUNCE0_MASK,
				debounce);
		break;
	case eint_state001:
		accdet_update_bits(ACCDET_EINT_DEBOUNCE1,
				PMIC_ACCDET_EINT_DEBOUNCE1_SHIFT,
				PMIC_ACCDET_EINT_DEBOUNCE1_MASK,
				debounce);
		break;
	case eint_state010:
		accdet_update_bits(ACCDET_EINT_DEBOUNCE2,
				PMIC_ACCDET_EINT_DEBOUNCE2_SHIFT,
				PMIC_ACCDET_EINT_DEBOUNCE2_MASK,
				debounce);
		break;
	case eint_state011:
		accdet_update_bits(ACCDET_EINT_DEBOUNCE3,
				PMIC_ACCDET_EINT_DEBOUNCE3_SHIFT,
				PMIC_ACCDET_EINT_DEBOUNCE3_MASK,
				debounce);
		break;
	case eint_inverter_state000:
		accdet_write(ACCDET_EINT_INVERTER_DEBOUNCE, debounce);
		break;
	default:
		pr_notice("Error: %s error state (%d)\n", __func__, state);
		break;
	}
}

static inline void check_cable_type(void)
{
	u32 cur_AB;

	cur_AB = accdet_read(ACCDET_MEM_IN) >> ACCDET_STATE_MEM_IN_OFFSET;
		cur_AB = cur_AB & ACCDET_STATE_AB_MASK;

	accdet->button_status = 0;

	switch (accdet->accdet_status) {
	case PLUG_OUT:
		if (cur_AB == ACCDET_STATE_AB_00) {
			mutex_lock(&accdet->res_lock);
			if (accdet->eint_sync_flag) {
				accdet->cable_type = HEADSET_NO_MIC;
				accdet->accdet_status = HOOK_SWITCH;
			} else
				pr_notice("accdet hp has been plug-out\n");
			mutex_unlock(&accdet->res_lock);
			/* for IOT HP */
			accdet_set_debounce(eint_state011,
				accdet_dts.pwm_deb.eint_debounce3);
		} else if (cur_AB == ACCDET_STATE_AB_01) {
			mutex_lock(&accdet->res_lock);
			if (accdet->eint_sync_flag) {
				accdet->accdet_status = MIC_BIAS;
				accdet->cable_type = HEADSET_MIC;
			} else
				pr_notice("accdet hp has been plug-out\n");
			mutex_unlock(&accdet->res_lock);
			/* solution: adjust hook switch debounce time
			 * for fast key press condition, avoid to miss key
			 */
			accdet_set_debounce(accdet_state000,
				button_press_debounce);
			/* for IOT HP */
			accdet_set_debounce(eint_state011, 0x1);
		} else if (cur_AB == ACCDET_STATE_AB_11) {
			/* accdet PLUG_OUT state not change */
			if (HAS_CAP(accdet->data->caps,
					ACCDET_PMIC_EINT_IRQ)) {
				mutex_lock(&accdet->res_lock);
				if (accdet->eint_sync_flag) {
					accdet->accdet_status = PLUG_OUT;
					accdet->cable_type = NO_DEVICE;
				} else
					pr_notice("accdet hp been plug-out\n");
				mutex_unlock(&accdet->res_lock);
			}
		} else {
			pr_notice("accdet %s Invalid AB.Do nothing\n",
					__func__);
		}
		break;
	case MIC_BIAS:
		if (cur_AB == ACCDET_STATE_AB_00) {
			mutex_lock(&accdet->res_lock);
			if (accdet->eint_sync_flag) {
				accdet->button_status = 1;
				accdet->accdet_status = HOOK_SWITCH;
				multi_key_detection(cur_AB);
			} else
				pr_notice("accdet hp has been plug-out\n");
			mutex_unlock(&accdet->res_lock);
		} else if (cur_AB == ACCDET_STATE_AB_01) {
			mutex_lock(&accdet->res_lock);
			if (accdet->eint_sync_flag) {
				accdet->accdet_status = MIC_BIAS;
				accdet->cable_type = HEADSET_MIC;
				/* accdet MIC_BIAS state not change */
			} else
				pr_notice("accdet hp has been plug-out\n");
			mutex_unlock(&accdet->res_lock);
			/* for IOT HP */
			accdet_set_debounce(eint_state011, 0x1);
		} else if (cur_AB == ACCDET_STATE_AB_11) {
			/* accdet Don't send plug out in MIC_BIAS */
			mutex_lock(&accdet->res_lock);
			if (accdet->eint_sync_flag)
				accdet->accdet_status = PLUG_OUT;
			else
				pr_notice("accdet hp has been plug-out\n");
			mutex_unlock(&accdet->res_lock);
		} else {
			pr_notice("accdet %s Invalid AB.Do nothing\n",
					__func__);
		}
		break;
	case HOOK_SWITCH:
		if (cur_AB == ACCDET_STATE_AB_00) {
		} else if (cur_AB == ACCDET_STATE_AB_01) {
			mutex_lock(&accdet->res_lock);
			if (accdet->eint_sync_flag) {
				multi_key_detection(cur_AB);
				accdet->accdet_status = MIC_BIAS;
				accdet->cable_type = HEADSET_MIC;
			} else
				pr_notice("accdet hp has been plug-out\n");
			mutex_unlock(&accdet->res_lock);
			/* for IOT HP */
			accdet_set_debounce(eint_state011, 0x1);
		} else if (cur_AB == ACCDET_STATE_AB_11) {
			/* accdet Don't send plugout in HOOK_SWITCH */
			mutex_lock(&accdet->res_lock);
			if (accdet->eint_sync_flag)
				accdet->accdet_status = PLUG_OUT;
			else
				pr_notice("accdet hp has been plug-out\n");
			mutex_unlock(&accdet->res_lock);
		} else {
			pr_notice("accdet %s Invalid AB.Do nothing\n",
					__func__);
		}
		break;
	case STAND_BY:
		/* accdet %s STANDBY state.Err!Do nothing */
		break;
	default:
		/* accdet %s Error state.Do nothing */
		break;
	}
}

static void accdet_work_callback(struct work_struct *work)
{
	u32 pre_cable_type = accdet->cable_type;

	__pm_stay_awake(accdet->wake_lock);
	check_cable_type();

	mutex_lock(&accdet->res_lock);
	if (accdet->eint_sync_flag) {
		if (pre_cable_type != accdet->cable_type)
			send_status_event(accdet->cable_type, 1);
	}
	mutex_unlock(&accdet->res_lock);
	if (accdet->cable_type != NO_DEVICE) {
		/* enable vusb LP */
		accdet_write(LDO_RG_LDO_VUSB_HW0_OP_EN, 0x8005);
	}

	__pm_relax(accdet->wake_lock);
}

static void accdet_queue_work(void)
{
	int ret;

	if (accdet->accdet_status == MIC_BIAS)
		accdet->cali_voltage = accdet_get_auxadc();

	ret = queue_work(accdet->accdet_workqueue, &accdet->accdet_work);
	if (!ret)
		pr_notice("Error: %s (%d)\n", __func__, ret);
}

static int pmic_eint_queue_work(int eintID)
{
	int ret = 0;

	if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT0)) {
		if (eintID == PMIC_EINT0) {
			if (accdet->cur_eint_state == EINT_PLUG_IN) {
				accdet_set_debounce(accdet_state011,
					cust_pwm_deb->debounce3);
				accdet->cur_eint_state = EINT_PLUG_OUT;
			} else {
				if (accdet->eint_id != M_PLUG_OUT) {
					accdet->cur_eint_state = EINT_PLUG_IN;

					mod_timer(&micbias_timer,
					jiffies + MICBIAS_DISABLE_TIMER);
				}
			}
			ret = queue_work(accdet->eint_workqueue,
					&accdet->eint_work);
		} else
			pr_notice("%s invalid EINT ID!\n", __func__);
	} else if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT1)) {
		if (eintID == PMIC_EINT1) {
			if (accdet->cur_eint_state == EINT_PLUG_IN) {
				accdet_set_debounce(accdet_state011,
					cust_pwm_deb->debounce3);
				accdet->cur_eint_state = EINT_PLUG_OUT;
			} else {
				if (accdet->eint_id != M_PLUG_OUT) {
					accdet->cur_eint_state = EINT_PLUG_IN;

					mod_timer(&micbias_timer,
					jiffies + MICBIAS_DISABLE_TIMER);
				}
			}
			ret = queue_work(accdet->eint_workqueue,
					&accdet->eint_work);
		} else
			pr_notice("%s invalid EINT ID!\n", __func__);
	} else if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_BI_EINT)) {
		if ((eintID & PMIC_EINT0) == PMIC_EINT0) {
			if (accdet->eint0_state == EINT_PLUG_IN) {
				accdet_set_debounce(accdet_state011,
					cust_pwm_deb->debounce3);
				accdet->eint0_state = EINT_PLUG_OUT;
			} else {
				if (accdet->eint_id != M_PLUG_OUT)
					accdet->eint0_state = EINT_PLUG_IN;
			}
		}
		if ((eintID & PMIC_EINT1) == PMIC_EINT1) {
			if (accdet->eint1_state == EINT_PLUG_IN) {
				accdet_set_debounce(accdet_state011,
					cust_pwm_deb->debounce3);
				accdet->eint1_state = EINT_PLUG_OUT;
			} else {
				if (accdet->eint_id != M_PLUG_OUT)
					accdet->eint1_state = EINT_PLUG_IN;
			}
		}

		/* bi_eint trigger issued current state, may */
		if (accdet->cur_eint_state == EINT_PLUG_OUT) {
			accdet->cur_eint_state =
				accdet->eint0_state & accdet->eint1_state;
			if (accdet->cur_eint_state == EINT_PLUG_IN) {
				mod_timer(&micbias_timer,
					jiffies + MICBIAS_DISABLE_TIMER);
				ret = queue_work(accdet->eint_workqueue,
						&accdet->eint_work);
			}
		} else if (accdet->cur_eint_state == EINT_PLUG_IN) {
			if ((accdet->eint0_state|accdet->eint1_state)
					== EINT_PLUG_OUT) {
				clear_accdet_eint_check(PMIC_EINT0);
				clear_accdet_eint_check(PMIC_EINT1);
			} else if ((accdet->eint0_state & accdet->eint1_state)
					== EINT_PLUG_OUT) {
				accdet->cur_eint_state = EINT_PLUG_OUT;
				ret = queue_work(accdet->eint_workqueue,
						&accdet->eint_work);
			}
		}
	}
	return ret;
}

void accdet_irq_handle(void)
{
	u32 eintID = 0;
	u32 irq_status, acc_sts, eint_sts;

	eintID = get_triggered_eint();
	irq_status = accdet_read(ACCDET_IRQ);
	acc_sts = accdet_read(ACCDET_MEM_IN);
	eint_sts = accdet_read(ACCDET_EINT0_MEM_IN);

	if ((irq_status & ACCDET_IRQ_B0) && (eintID == 0)) {
		clear_accdet_int();
		accdet_queue_work();
		clear_accdet_int_check();
	} else if (eintID != NO_PMIC_EINT) {
		/* check EINT0 status */
		accdet->eint_id = accdet_read_bits(ACCDET_EINT0_MEM_IN,
			PMIC_ACCDET_EINT0_MEM_IN_SHIFT,
			PMIC_ACCDET_EINT0_MEM_IN_MASK);
		/* adjust eint digital/analog setting */
		adjust_eint_setting(accdet->eint_id);
		clear_accdet_eint(eintID);
		clear_accdet_eint_check(eintID);
		pmic_eint_queue_work(eintID);
	} else {
		pr_notice("%s no interrupt detected!\n", __func__);
	}
}

static irqreturn_t mtk_accdet_irq_handler_thread(int irq, void *data)
{
	accdet_irq_handle();

	return IRQ_HANDLED;
}

static irqreturn_t ex_eint_handler(int irq, void *data)
{
	int ret = 0;

	if (accdet->cur_eint_state == EINT_PLUG_IN) {
		/* To trigger EINT when the headset was plugged in
		 * We set the polarity back as we initialed.
		 */
		if (accdet->accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
			irq_set_irq_type(accdet->gpioirq, IRQ_TYPE_LEVEL_HIGH);
		else
			irq_set_irq_type(accdet->gpioirq, IRQ_TYPE_LEVEL_LOW);
		gpio_set_debounce(accdet->gpiopin, accdet->gpio_hp_deb);

		accdet->cur_eint_state = EINT_PLUG_OUT;
	} else {
		/* To trigger EINT when the headset was plugged out
		 * We set the opposite polarity to what we initialed.
		 */
		if (accdet->accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
			irq_set_irq_type(accdet->gpioirq, IRQ_TYPE_LEVEL_LOW);
		else
			irq_set_irq_type(accdet->gpioirq, IRQ_TYPE_LEVEL_HIGH);

		gpio_set_debounce(accdet->gpiopin,
				accdet_dts.plugout_deb * 1000);

		accdet->cur_eint_state = EINT_PLUG_IN;

		mod_timer(&micbias_timer,
			jiffies + MICBIAS_DISABLE_TIMER);

	}

	disable_irq_nosync(accdet->gpioirq);
	ret = queue_work(accdet->eint_workqueue, &accdet->eint_work);
	return IRQ_HANDLED;
}

static inline int ext_eint_setup(struct platform_device *platform_device)
{
	int ret = 0;
	u32 ints[4] = { 0 };
	struct device_node *node = NULL;
	struct pinctrl_state *pins_default = NULL;

	accdet->pinctrl = devm_pinctrl_get(&platform_device->dev);
	if (IS_ERR(accdet->pinctrl)) {
		ret = PTR_ERR(accdet->pinctrl);
		return ret;
	}

	pins_default = pinctrl_lookup_state(accdet->pinctrl, "default");
	if (IS_ERR(pins_default))
		ret = PTR_ERR(pins_default);

	accdet->pins_eint = pinctrl_lookup_state(accdet->pinctrl,
			"state_eint_as_int");
	if (IS_ERR(accdet->pins_eint)) {
		ret = PTR_ERR(accdet->pins_eint);
		return ret;
	}
	pinctrl_select_state(accdet->pinctrl, accdet->pins_eint);

	node = of_find_matching_node(node, accdet_of_match);
	if (!node)
		return -1;

	accdet->gpiopin = of_get_named_gpio(node, "deb-gpios", 0);
	ret = of_property_read_u32(node, "debounce",
			&accdet->gpio_hp_deb);
	if (ret < 0)
		return ret;

	gpio_set_debounce(accdet->gpiopin, accdet->gpio_hp_deb);

	accdet->gpioirq = irq_of_parse_and_map(node, 0);
	ret = of_property_read_u32_array(node, "interrupts", ints,
			ARRAY_SIZE(ints));
	if (ret)
		return ret;

	accdet->accdet_eint_type = ints[1];
	ret = request_irq(accdet->gpioirq, ex_eint_handler, IRQF_TRIGGER_NONE,
		"accdet-eint", NULL);
	if (ret)
		return ret;

	return 0;
}

static int accdet_get_dts_data(void)
{
	int ret;
	struct device_node *node = NULL;
	int pwm_deb[15];
	int three_key[4];

	node = of_find_matching_node(node, accdet_of_match);
	if (!node) {
		pr_notice("Error: %s can't find compatible dts node\n",
			__func__);
		return -1;
	}

	ret = of_property_read_u32(node, "eint_use_ext_res",
		&accdet_dts.eint_use_ext_res);
	if (ret) {
		/* eint use internal resister */
		accdet_dts.eint_use_ext_res = 0x0;
	}
	ret = of_property_read_u32(node, "eint_detect_mode",
		&accdet_dts.eint_detect_mode);
	if (ret) {
		/* eint detection mode equals to EINT 2.1 */
		accdet_dts.eint_detect_mode = 0x4;
	}
	accdet_dts.eint_detect_mode = 0x4;

	ret = of_property_read_u32(node,
			"accdet-mic-vol", &accdet_dts.mic_vol);
	if (ret)
		accdet_dts.mic_vol = 8;

	ret = of_property_read_u32(node, "accdet-plugout-debounce",
			&accdet_dts.plugout_deb);
	if (ret)
		accdet_dts.plugout_deb = 1;

	ret = of_property_read_u32(node,
			"accdet-mic-mode", &accdet_dts.mic_mode);
	if (ret)
		accdet_dts.mic_mode = 2;

	if (HAS_CAP(accdet->data->caps, ACCDET_FOUR_KEY)) {
		int four_key[5];

		ret = of_property_read_u32_array(node,
				"headset-four-key-threshold",
			four_key, ARRAY_SIZE(four_key));
		if (!ret)
			memcpy(&accdet_dts.four_key, four_key+1,
					sizeof(struct four_key_threshold));
		else {
			pr_notice("accdet no 4-key-thrsh dts, use efuse\n");
			accdet_get_efuse_4key();
		}
	} else {
		if (HAS_CAP(accdet->data->caps, ACCDET_THREE_KEY)) {
			ret = of_property_read_u32_array(node,
					"headset-three-key-threshold",
					three_key, ARRAY_SIZE(three_key));
		} else {
			ret = of_property_read_u32_array(node,
				"headset-three-key-threshold-CDD", three_key,
				ARRAY_SIZE(three_key));
		}
		if (!ret)
			memcpy(&accdet_dts.three_key, three_key+1,
					sizeof(struct three_key_threshold));
	}
	ret = of_property_read_u32_array(node, "headset-mode-setting", pwm_deb,
			ARRAY_SIZE(pwm_deb));
	/* debounce8(auxadc debounce) is default, needn't get from dts */
	if (!ret)
		memcpy(&accdet_dts.pwm_deb, pwm_deb, sizeof(pwm_deb));

	cust_pwm_deb = &accdet_dts.pwm_deb;
	dis_micbias_done = false;

	return 0;
}

static void config_digital_init_by_mode(void)
{
	/* enable eint cmpmem pwm */
	accdet_write(ACCDET_EINT_CMPMEN_PWM_THRESH,
		(accdet_dts.pwm_deb.eint_pwm_width << 4 |
		accdet_dts.pwm_deb.eint_pwm_thresh));
	/* DA signal stable */
	if (HAS_CAP(accdet->data->caps,
			ACCDET_PMIC_EINT0)) {
		accdet_write(ACCDET_DA_STABLE,
				ACCDET_EINT0_STABLE_VAL);
	} else if (HAS_CAP(accdet->data->caps,
			ACCDET_PMIC_EINT1)) {
		accdet_write(ACCDET_DA_STABLE,
				ACCDET_EINT1_STABLE_VAL);
	} else if (HAS_CAP(accdet->data->caps,
			ACCDET_PMIC_BI_EINT)) {
		accdet_write(ACCDET_DA_STABLE,
				ACCDET_EINT0_STABLE_VAL);
		accdet_write(ACCDET_DA_STABLE,
				ACCDET_EINT1_STABLE_VAL);
	}
	/* after receive n+1 number, interrupt issued. now is 2 times */
	accdet_update_bit(ACCDET_EINT_M_PLUG_IN_NUM,
	PMIC_ACCDET_EINT_M_PLUG_IN_NUM_SHIFT);
	/* disable hwmode */
	accdet_write(ACCDET_HWMODE_EN, 0x100);

	accdet_clear_bit(ACCDET_EINT_M_DETECT_EN,
		PMIC_ACCDET_EINT_M_DETECT_EN_SHIFT);
	/* enable PWM */
	accdet_write(ACCDET_CMP_PWM_EN, 0x67);
	/* enable inverter detection */
	if (HAS_CAP(accdet->data->caps,
			ACCDET_PMIC_EINT0)) {
		accdet_update_bit(ACCDET_EINT0_INVERTER_SW_EN,
			PMIC_ACCDET_EINT0_INVERTER_SW_EN_SHIFT);
	} else if (HAS_CAP(accdet->data->caps,
			ACCDET_PMIC_EINT1)) {
		accdet_update_bit(ACCDET_EINT1_INVERTER_SW_EN,
			PMIC_ACCDET_EINT1_INVERTER_SW_EN_SHIFT);
	} else if (HAS_CAP(accdet->data->caps,
			ACCDET_PMIC_BI_EINT)) {
		accdet_update_bit(ACCDET_EINT0_INVERTER_SW_EN,
			PMIC_ACCDET_EINT0_INVERTER_SW_EN_SHIFT);
		accdet_update_bit(ACCDET_EINT1_INVERTER_SW_EN,
			PMIC_ACCDET_EINT1_INVERTER_SW_EN_SHIFT);
	}
}

static void config_eint_init_by_mode(void)
{
	if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT0)) {
		accdet_update_bit(ACCDET_RG_EINT0EN,
				PMIC_RG_EINT0EN_SHIFT);
	} else if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT1)) {
		accdet_update_bit(ACCDET_RG_EINT1EN,
				PMIC_RG_EINT1EN_SHIFT);
	} else if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_BI_EINT)) {
		accdet_update_bit(ACCDET_RG_EINT0EN,
				PMIC_RG_EINT0EN_SHIFT);
		accdet_update_bit(ACCDET_RG_EINT1EN,
				PMIC_RG_EINT1EN_SHIFT);
	}
	/* ESD switches on */
	accdet_update_bit(ACCDET_RG_ACCDETSPARE, 8);
	/* before playback, set NCP pull low before nagative voltage */
	accdet_update_bit(ACCDET_RG_NCP_PDDIS_EN, PMIC_RG_NCP_PDDIS_EN_SHIFT);

	if (accdet_dts.eint_detect_mode != 0x1) {
		/* current detect set 0.25uA */
		accdet_update_bits(ACCDET_RG_ACCDETSPARE,
			PMIC_RG_ACCDETSPARE_SHIFT,
			0x3, 0x3);
	}
}

static void accdet_init_once(void)
{
	unsigned int reg = 0;

	/* reset the accdet unit */
	accdet_update_bit(ACCDET_RG_ACCDET_RST, PMIC_RG_ACCDET_RST_SHIFT);
	accdet_clear_bit(ACCDET_RG_ACCDET_RST, PMIC_RG_ACCDET_RST_SHIFT);

	/* clear high micbias1 voltage setting */
	accdet_clear_bits(ACCDET_RG_AUDPWDBMICBIAS1,
		PMIC_RG_AUDMICBIAS1HVEN_SHIFT, 0x3, 0x3);
	/* clear micbias1 voltage */
	accdet_clear_bits(ACCDET_RG_AUDPWDBMICBIAS1,
		PMIC_RG_AUDMICBIAS1VREF_SHIFT, 0x7, 0x7);

	/* init pwm frequency, duty & rise/falling delay */
	accdet_write(ACCDET_PWM_WIDTH,
		REGISTER_VAL(cust_pwm_deb->pwm_width));
	accdet_write(ACCDET_PWM_THRESH,
		REGISTER_VAL(cust_pwm_deb->pwm_thresh));
	accdet_write(ACCDET_RISE_DELAY,
		  (cust_pwm_deb->fall_delay << 15 | cust_pwm_deb->rise_delay));

	/* config micbias voltage, micbias1 vref is only controlled by accdet
	 * if we need 2.8V, config [12:13]
	 */
	reg = accdet_read(ACCDET_RG_AUDPWDBMICBIAS1);
	if (accdet_dts.mic_vol <= 7) {
		/* micbias1 <= 2.7V */
		accdet_write(ACCDET_RG_AUDPWDBMICBIAS1,
		reg | (accdet_dts.mic_vol<<PMIC_RG_AUDMICBIAS1VREF_SHIFT) |
		RG_AUD_MICBIAS1_LOWP_EN);
	} else if (accdet_dts.mic_vol == 8) {
		/* micbias1 = 2.8v */
		accdet_write(ACCDET_RG_AUDPWDBMICBIAS1,
			reg | (3<<PMIC_RG_AUDMICBIAS1HVEN_SHIFT) |
			RG_AUD_MICBIAS1_LOWP_EN);
	} else if (accdet_dts.mic_vol == 9) {
		/* micbias1 = 2.85v */
		accdet_write(ACCDET_RG_AUDPWDBMICBIAS1,
			reg | (1<<PMIC_RG_AUDMICBIAS1HVEN_SHIFT) |
			RG_AUD_MICBIAS1_LOWP_EN);
	}
	/* mic mode setting */
	reg = accdet_read(ACCDET_RG_AUDACCDETMICBIAS0PULLLOW);
	if (accdet_dts.mic_mode == HEADSET_MODE_1) {
		/* ACC mode*/
		accdet_write(ACCDET_RG_AUDACCDETMICBIAS0PULLLOW,
			reg | RG_ACCDET_MODE_ANA11_MODE1);
		/* enable analog fast discharge */
		accdet_update_bit(ACCDET_RG_ANALOGFDEN,
			PMIC_RG_ANALOGFDEN_SHIFT);
		accdet_update_bits(ACCDET_RG_ACCDETSPARE, 11, 0x3, 0x3);
	} else if (accdet_dts.mic_mode == HEADSET_MODE_2) {
		/* DCC mode Low cost mode without internal bias*/
		accdet_write(ACCDET_RG_AUDACCDETMICBIAS0PULLLOW,
			reg | RG_ACCDET_MODE_ANA11_MODE2);
		/* enable analog fast discharge */
		accdet_update_bits(ACCDET_RG_ANALOGFDEN,
			PMIC_RG_ANALOGFDEN_SHIFT, 0x3, 0x3);
	} else if (accdet_dts.mic_mode == HEADSET_MODE_6) {
		/* DCC mode Low cost mode with internal bias,
		 * bit8 = 1 to use internal bias
		 */
		accdet_write(ACCDET_RG_AUDACCDETMICBIAS0PULLLOW,
			reg | RG_ACCDET_MODE_ANA11_MODE6);
		accdet_update_bit(ACCDET_RG_AUDPWDBMICBIAS1,
				PMIC_RG_AUDMICBIAS1DCSW1PEN_SHIFT);
		/* enable analog fast discharge */
		accdet_update_bits(ACCDET_RG_ANALOGFDEN,
			PMIC_RG_ANALOGFDEN_SHIFT, 0x3, 0x3);
	}

	if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT_IRQ)) {
		config_eint_init_by_mode();
		config_digital_init_by_mode();
	} else if (HAS_CAP(accdet->data->caps, ACCDET_AP_GPIO_EINT)) {
		/* set pull low pads and DCC mode */
		accdet_write(ACCDET_RG_AUDACCDETMICBIAS0PULLLOW,
				0x8F);
		/* disconnect configaccdet */
		accdet_write(ACCDET_RG_EINT1CONFIGACCDET,
				0x0);
		/* disable eint comparator */
		accdet_write(ACCDET_RG_EINT0CMPEN, 0x0);
		/* enable PWM */
		accdet_write(ACCDET_CMP_PWM_EN, 0x7);
		/* enable accdet sw mode */
		accdet_write(ACCDET_HWMODE_EN, 0x0);
		/* set DA signal to stable */
		accdet_write(ACCDET_DA_STABLE, 0x1);
		/* disable eint/inverter/sw_en */
		accdet_write(ACCDET_SW_EN, 0x0);
	}
}

static void accdet_init_debounce(void)
{
	/* set debounce to 1ms */
	accdet_set_debounce(eint_state000,
		accdet_dts.pwm_deb.eint_debounce0);
	/* set debounce to 128ms */
	accdet_set_debounce(eint_state011,
		accdet_dts.pwm_deb.eint_debounce3);
}

static inline void accdet_init(void)
{
	/* set and clear initial bit every eint interrutp */
	accdet_update_bit(ACCDET_SEQ_INIT, PMIC_ACCDET_SEQ_INIT_SHIFT);
	usleep_range(2000, 3000);
	accdet_clear_bit(ACCDET_SEQ_INIT, PMIC_ACCDET_SEQ_INIT_SHIFT);
	usleep_range(1000, 1500);
	/* init the debounce time (debounce/32768)sec */
	accdet_set_debounce(accdet_state000, cust_pwm_deb->debounce0);
	accdet_set_debounce(accdet_state001, cust_pwm_deb->debounce1);
	accdet_set_debounce(accdet_state011, cust_pwm_deb->debounce3);
	/* auxadc:2ms */
	accdet_set_debounce(accdet_auxadc, cust_pwm_deb->debounce4);
	accdet_set_debounce(eint_state001,
		accdet_dts.pwm_deb.eint_debounce1);
	accdet_set_debounce(eint_inverter_state000,
		accdet_dts.pwm_deb.eint_inverter_debounce);

}

static int accdet_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	struct mt6397_chip *mt6397_chip = dev_get_drvdata(pdev->dev.parent);
	const struct of_device_id *of_id =
				of_match_device(accdet_of_match, &pdev->dev);
	if (!of_id) {
		dev_dbg(&pdev->dev, "Error: No device match found\n");
		return -ENODEV;
	}

	accdet = devm_kzalloc(&pdev->dev, sizeof(*accdet), GFP_KERNEL);
	if (!accdet)
		return -ENOMEM;

	accdet->data = (struct accdet_priv *)of_id->data;
	accdet->pdev = pdev;
	accdet->card.dev = &pdev->dev;
	accdet->card.owner = THIS_MODULE;
	ret = snd_soc_of_parse_card_name(&accdet->card, "accdet-name");
	if (ret) {
		dev_dbg(&pdev->dev, "Error: Parse card name failed (%d)\n",
				ret);
		return ret;
	}

	/* parse dts attributes */
	ret = accdet_get_dts_data();
	if (ret) {
		dev_dbg(&pdev->dev, "Error: Get dts data failed (%d)\n",
				ret);
		return ret;
	}
	/* init lock */
	accdet->wake_lock = wakeup_source_register("accdet_wake_lock");
	accdet->timer_lock = wakeup_source_register("accdet_timer_lock");
	mutex_init(&accdet->res_lock);

	platform_set_drvdata(pdev, accdet);
	snd_soc_card_set_drvdata(&accdet->card, accdet);
	ret = devm_snd_soc_register_card(&pdev->dev, &accdet->card);
	if (ret) {
		dev_dbg(&pdev->dev, "Error: Register card failed (%d)\n",
				ret);
		return ret;
	}
	accdet->data->snd_card = accdet->card.snd_card;
	ret = snd_soc_card_jack_new(&accdet->card,
			accdet_jack_pins[0].pin,
			accdet_jack_pins[0].mask,
			&accdet->jack, accdet_jack_pins, 1);
	if (ret) {
		dev_dbg(&pdev->dev, "Error: New card jack failed (%d)\n",
				ret);
		return ret;
	}
	accdet->jack.jack->input_dev->id.bustype = BUS_HOST;
	snd_jack_set_key(accdet->jack.jack, SND_JACK_BTN_0, KEY_PLAYPAUSE);
	snd_jack_set_key(accdet->jack.jack, SND_JACK_BTN_1, KEY_VOLUMEDOWN);
	snd_jack_set_key(accdet->jack.jack, SND_JACK_BTN_2, KEY_VOLUMEUP);
	snd_jack_set_key(accdet->jack.jack, SND_JACK_BTN_3, KEY_VOICECOMMAND);

	/* Important. must to register */
	ret = snd_card_register(accdet->card.snd_card);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	accdet->regmap = mt6397_chip->regmap;
	accdet->dev = &pdev->dev;

	/* get pmic auxadc iio channel handler */
	accdet->accdet_auxadc = devm_iio_channel_get(&pdev->dev,
			"pmic_accdet");
	ret = PTR_ERR_OR_ZERO(accdet->accdet_auxadc);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_dbg(&pdev->dev, "Error: Get iio ch failed (%d)\n",
				ret);
		return ret;
	}

	/* get pmic efuse handler */
	accdet->accdet_efuse = devm_nvmem_device_get(&pdev->dev,
			"mt63xx-accdet-efuse");
	ret = PTR_ERR_OR_ZERO(accdet->accdet_efuse);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_dbg(&pdev->dev, "Error: Get efuse failed (%d)\n",
				ret);
		return ret;
	}

	accdet_get_efuse();

	/* register pmic interrupt */
	accdet->accdet_irq = platform_get_irq(pdev, 0);
	if (accdet->accdet_irq < 0) {
		dev_dbg(&pdev->dev,
			"Error: Get accdet irq failed (%d)\n",
			accdet->accdet_irq);
		return accdet->accdet_irq;
	}
	ret = devm_request_threaded_irq(&pdev->dev, accdet->accdet_irq,
				NULL, mtk_accdet_irq_handler_thread,
				IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				"ACCDET_IRQ", accdet);
	if (ret) {
		dev_dbg(&pdev->dev,
			"Error: Get thread irq request failed (%d) (%d)\n",
			accdet->accdet_irq, ret);
		return ret;
	}

	if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT0)) {
		accdet->accdet_eint0 = platform_get_irq(pdev, 1);
		if (accdet->accdet_eint0 < 0) {
			dev_dbg(&pdev->dev,
				"Error: Get eint0 irq failed (%d)\n",
				accdet->accdet_eint0);
			return accdet->accdet_eint0;
		}
		ret = devm_request_threaded_irq(&pdev->dev,
				accdet->accdet_eint0,
				NULL, mtk_accdet_irq_handler_thread,
				IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				"ACCDET_EINT0", accdet);
		if (ret) {
			dev_dbg(&pdev->dev,
				"Error: Get eint0 irq request failed (%d)\n",
				ret);
			return ret;
		}
	} else if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT1)) {
		accdet->accdet_eint1 = platform_get_irq(pdev, 2);
		if (accdet->accdet_eint1 < 0) {
			dev_dbg(&pdev->dev,
				"Error: Get eint1 irq failed (%d)\n",
				accdet->accdet_eint1);
			return accdet->accdet_eint1;
		}
		ret = devm_request_threaded_irq(&pdev->dev,
					accdet->accdet_eint1,
					NULL, mtk_accdet_irq_handler_thread,
					IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
					"ACCDET_EINT1", accdet);
		if (ret) {
			dev_dbg(&pdev->dev,
				"Error: Get eint1 irq request failed (%d)\n",
				ret);
			return ret;
		}
	} else if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_BI_EINT)) {
		accdet->accdet_eint0 = platform_get_irq(pdev, 1);
		if (accdet->accdet_eint0 < 0) {
			dev_dbg(&pdev->dev,
				"Error: Get eint0 irq failed (%d)\n",
				accdet->accdet_eint0);
			return accdet->accdet_eint0;
		}
		ret = devm_request_threaded_irq(&pdev->dev,
				accdet->accdet_eint0,
				NULL, mtk_accdet_irq_handler_thread,
				IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				"ACCDET_EINT0", accdet);
		if (ret) {
			dev_dbg(&pdev->dev,
				"Error: Get eint0 irq request failed (%d)\n",
				ret);
			return ret;
		}
		accdet->accdet_eint1 = platform_get_irq(pdev, 2);
		if (accdet->accdet_eint1 < 0) {
			dev_dbg(&pdev->dev,
				"Error: Get eint1 irq failed (%d)\n",
				accdet->accdet_eint1);
			return accdet->accdet_eint1;
		}
		ret = devm_request_threaded_irq(&pdev->dev,
					accdet->accdet_eint1,
					NULL, mtk_accdet_irq_handler_thread,
					IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
					"ACCDET_EINT1", accdet);
		if (ret) {
			dev_dbg(&pdev->dev,
				"Error: Get eint1 irq request failed (%d)\n",
				ret);
			return ret;
		}
	}

	/* register char device number, Create normal device for auido use */
	ret = alloc_chrdev_region(&accdet->accdet_devno, 0, 1, ACCDET_DEVNAME);
	if (ret)
		goto err_chrdevregion;

	/* create class in sysfs, "sys/class/", so udev in userspace can create
	 * device node, when device_create is called
	 */
	accdet->accdet_class = class_create(THIS_MODULE, ACCDET_DEVNAME);
	if (!accdet->accdet_class) {
		dev_dbg(&pdev->dev,
			"Error: Create class failed (%d)\n", ret);
		ret = -1;
	}

	/* setup timer */
	timer_setup(&micbias_timer, dis_micbias_timerhandler, 0);
	micbias_timer.expires = jiffies + MICBIAS_DISABLE_TIMER;

	/* Create workqueue */
	accdet->accdet_workqueue = create_singlethread_workqueue("accdet");
	INIT_WORK(&accdet->accdet_work, accdet_work_callback);
	if (!accdet->accdet_workqueue) {
		dev_dbg(&pdev->dev, "Error: Create accdet orkqueue failed\n");
		ret = -1;
		goto err_device_create;
	}

	accdet->dis_micbias_workqueue =
		create_singlethread_workqueue("dismicQueue");
	INIT_WORK(&accdet->dis_micbias_work, dis_micbias_work_callback);
	if (!accdet->dis_micbias_workqueue) {
		dev_dbg(&pdev->dev, "Error: Create dismic workqueue failed\n");
		ret = -1;
		goto err;
	}
	accdet->eint_workqueue = create_singlethread_workqueue("accdet_eint");
	INIT_WORK(&accdet->eint_work, eint_work_callback);
	if (!accdet->eint_workqueue) {
		dev_dbg(&pdev->dev, "Error: Create eint workqueue failed\n");
		ret = -1;
		goto err_create_workqueue;
	}
	if (HAS_CAP(accdet->data->caps, ACCDET_AP_GPIO_EINT)) {
		accdet->accdet_eint_type = IRQ_TYPE_LEVEL_LOW;
		ret = ext_eint_setup(pdev);
		if (ret)
			destroy_workqueue(accdet->eint_workqueue);
	}

	accdet_init();
	accdet_init_debounce();
	accdet_init_once();

	return 0;

err_create_workqueue:
	destroy_workqueue(accdet->dis_micbias_workqueue);
err:
	destroy_workqueue(accdet->accdet_workqueue);
err_device_create:
	class_destroy(accdet->accdet_class);
err_chrdevregion:
	pr_notice("%s error. now exit.!\n", __func__);
	return ret;
}

static int accdet_remove(struct platform_device *pdev)
{
	destroy_workqueue(accdet->eint_workqueue);
	destroy_workqueue(accdet->dis_micbias_workqueue);
	destroy_workqueue(accdet->accdet_workqueue);
	class_destroy(accdet->accdet_class);
	unregister_chrdev_region(accdet->accdet_devno, 1);
	devm_kfree(&pdev->dev, accdet);
	return 0;
}

static long mt_accdet_unlocked_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	switch (cmd) {
	case GET_BUTTON_STATUS:
		return accdet->button_status;
	default:
		break;
	}
	return 0;
}

static const struct file_operations accdet_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = mt_accdet_unlocked_ioctl,
};

const struct file_operations *accdet_get_fops(void)
{
	return &accdet_fops;
}

static struct platform_driver accdet_driver = {
	.probe = accdet_probe,
	.remove = accdet_remove,
	.driver = {
		.name = "pmic-codec-accdet",
		.of_match_table = accdet_of_match,
	},
};

static int __init accdet_soc_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&accdet_driver);
	if (ret)
		return -ENODEV;
	return 0;
}
static void __exit accdet_soc_exit(void)
{
	platform_driver_unregister(&accdet_driver);
}
module_init(accdet_soc_init);
module_exit(accdet_soc_exit);

/* Module information */
MODULE_DESCRIPTION("MT6359 ALSA SoC accdet driver");
MODULE_AUTHOR("Argus Lin <argus.lin@mediatek.com>");
MODULE_LICENSE("GPL v2");
