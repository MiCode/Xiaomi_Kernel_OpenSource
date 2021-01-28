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
#define ACCDET_PMIC_EINT_IRQ		BIT(0)
#define ACCDET_AP_GPIO_EINT		BIT(1)

#define ACCDET_PMIC_EINT0		BIT(2)
#define ACCDET_PMIC_EINT1		BIT(3)
#define ACCDET_PMIC_BI_EINT		BIT(4)

#define ACCDET_PMIC_GPIO_TRIG_EINT	BIT(5)
#define ACCDET_PMIC_INVERTER_TRIG_EINT	BIT(6)
#define ACCDET_PMIC_RSV_EINT		BIT(7)

#define ACCDET_THREE_KEY		BIT(8)
#define ACCDET_FOUR_KEY			BIT(9)
#define ACCDET_TRI_KEY_CDD		BIT(10)
#define ACCDET_RSV_KEY			BIT(11)

#define ACCDET_ANALOG_FASTDISCHARGE	BIT(12)
#define ACCDET_DIGITAL_FASTDISCHARGE	BIT(13)
#define ACCDET_AD_FASTDISCHRAGE		BIT(14)

#define ACCDET_MOISTURE_DETECTED	BIT(15)

#define RET_LT_5K			(-1)
#define RET_GT_5K			(0)

/* Used to let accdet know if the pin has been fully plugged-in */
#define EINT_PLUG_OUT			(0)
#define EINT_PLUG_IN			(1)
#define EINT_MOISTURE_DETECTED	(2)

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
	struct work_struct delay_init_work;
	struct workqueue_struct *delay_init_workqueue;
	struct work_struct dis_micbias_work;
	struct workqueue_struct *dis_micbias_workqueue;
	struct work_struct accdet_work;
	struct workqueue_struct *accdet_workqueue;
	/* when eint issued, queue work: eint_work */
	struct work_struct eint_work;
	struct workqueue_struct *eint_workqueue;
	u32 water_r;
	u32 moisture_ext_r;
	u32 moisture_int_r;
	u32 moisture_vm;
	u32 moisture_vdd_offset;
	u32 moisture_offset;
	u32 moisture_eint_offset;
};
static struct mt63xx_accdet_data *accdet;

static struct head_dts_data accdet_dts;
struct pwm_deb_settings *cust_pwm_deb;

struct accdet_priv {
	u32 caps;
	struct snd_card *snd_card;
};

static struct accdet_priv mt6359_accdet[] = {
	{
		.caps = 0,
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

static struct platform_driver accdet_driver;

static atomic_t accdet_first;
#define ACCDET_INIT_WAIT_TIMER (10 * HZ)
static struct timer_list accdet_init_timer;
static void delay_init_timerhandler(struct timer_list *t);
/* micbias_timer: disable micbias if no accdet irq after eint,
 * timeout: 6 seconds
 * timerHandler: dis_micbias_timerhandler()
 */
#define MICBIAS_DISABLE_TIMER (6 * HZ)
static struct timer_list micbias_timer;
static void dis_micbias_timerhandler(struct timer_list *t);
static bool dis_micbias_done;
static char accdet_log_buf[1280];
static bool debug_thread_en;
static bool dump_reg;
static struct task_struct *thread;

static u32 button_press_debounce = 0x400;
static u32 button_press_debounce_01 = 0x800;

/* local function declaration */
static void accdet_init_once(void);
static inline void accdet_init(void);
static void accdet_init_debounce(void);
static void accdet_modify_vref_volt_self(void);
static u32 adjust_eint_analog_setting(void);
static u32 adjust_moisture_analog_setting(u32 eintID);
static u32 adjust_moisture_setting(u32 moistureID, u32 eintID);
static u32 adjust_eint_setting(u32 eintsts);
static void config_analog_moisture_init_by_mode(void);
static void config_digital_init_by_mode(void);
static void config_eint_init_by_mode(void);
static u32 config_moisture_detect_1_0(void);
static u32 config_moisture_detect_1_1(void);
static u32 config_moisture_detect_2_1(void);
static u32 config_moisture_detect_2_1_1(void);
static u32 get_moisture_det_en(void);
static u32 get_moisture_sw_auxadc_check(void);
static u32 get_triggered_eint(void);
static void recover_eint_analog_setting(void);
static void recover_eint_digital_setting(void);
static void recover_eint_setting(u32 eintsts);
static void recover_moisture_setting(u32 moistureID);
static void send_status_event(u32 cable_type, u32 status);

/* global function declaration */
inline u32 accdet_read(u32 addr)
{
	u32 val = 0;

	regmap_read(accdet->regmap, addr, &val);
	return val;
}

inline u32 accdet_read_bits(u32 addr, u32 shift, u32 mask)
{
	u32 val = 0;

	val = accdet_read(addr);
	return ((val>>shift) & mask);
}

inline void accdet_write(u32 addr, u32 wdata)
{
	regmap_write(accdet->regmap, addr, wdata);
}

inline void accdet_update_bits(u32 addr, u32 shift, u32 mask, u32 data)
{
	regmap_update_bits(accdet->regmap, addr, mask << shift,
		data << shift);
}

inline void accdet_update_bit(u32 addr, u32 shift)
{
	unsigned int mask = shift;

	regmap_update_bits(accdet->regmap, addr, BIT(mask),
		BIT(shift));
}

inline void accdet_clear_bits(u32 addr, unsigned int shift,
			unsigned int mask, unsigned int data)
{
	regmap_update_bits(accdet->regmap, addr, mask << shift, 0);
}

inline void accdet_clear_bit(u32 addr, u32 shift)
{
	unsigned int mask = shift;

	regmap_update_bits(accdet->regmap, addr, BIT(mask), 0);
}

static void mini_dump_register(void)
{
	int addr = 0, st_addr = 0, end_addr = 0, idx = 0, log_size = 0;

	log_size +=
	sprintf(accdet_log_buf,
		"(0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x",
		ACCDET_SW_EN_ADDR,
		accdet_read(ACCDET_SW_EN_ADDR),
		ACCDET_CMP_PWM_EN_ADDR,
		accdet_read(ACCDET_CMP_PWM_EN_ADDR),
		ACCDET_IRQ_ADDR,
		accdet_read(ACCDET_IRQ_ADDR),
		ACCDET_DA_STABLE_ADDR,
		accdet_read(ACCDET_DA_STABLE_ADDR));
	log_size += sprintf(accdet_log_buf + log_size,
		"(0x%x)=0x%x (0x%x)=0x%x\naccdet (0x%x)=0x%x (0x%x)=0x%x",
		ACCDET_HWMODE_EN_ADDR,
		accdet_read(ACCDET_HWMODE_EN_ADDR),
		ACCDET_CMPEN_SEL_ADDR,
		accdet_read(ACCDET_CMPEN_SEL_ADDR),
		ACCDET_CMPEN_SW_ADDR,
		accdet_read(ACCDET_CMPEN_SW_ADDR),
		AD_AUDACCDETCMPOB_ADDR,
		accdet_read(AD_AUDACCDETCMPOB_ADDR));
	log_size += sprintf(accdet_log_buf + log_size,
		"(0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x\n",
		AD_EINT0CMPMOUT_ADDR,
		accdet_read(AD_EINT0CMPMOUT_ADDR),
		AD_EINT0INVOUT_ADDR,
		accdet_read(AD_EINT0INVOUT_ADDR),
		ACCDET_EN_ADDR,
		accdet_read(ACCDET_EN_ADDR),
		AD_AUDACCDETCMPOB_ADDR,
		accdet_read(AD_AUDACCDETCMPOB_ADDR));
	st_addr = RG_AUDPWDBMICBIAS0_ADDR;
	end_addr = RG_ACCDETSPARE_ADDR;
	for (addr = st_addr; addr <= end_addr; addr += 8) {
		idx = addr;
		log_size += sprintf(accdet_log_buf + log_size,
			"(0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x\n",
			idx, accdet_read(idx),
			idx+2, accdet_read(idx+2),
			idx+4, accdet_read(idx+4),
			idx+6, accdet_read(idx+6));
	}
	pr_notice("\naccdet %s %d", accdet_log_buf, log_size);
}

static void dump_register(void)
{
	int addr = 0, st_addr = 0, end_addr = 0, idx = 0;

	pr_notice("Accdet EINTx support,MODE_%d regs:\n", accdet_dts.mic_mode);
	if (dump_reg) {
		pr_notice("ACCDET_RG\n");
		st_addr = ACCDET_AUXADC_SEL_ADDR;
		end_addr = ACCDET_MON_FLAG_EN_ADDR;
		for (addr = st_addr; addr <= end_addr; addr += 8) {
			idx = addr;
			pr_notice("(0x%x)=0x%x (0x%x)=0x%x ",
				idx, accdet_read(idx),
				idx+2, accdet_read(idx+2));
			pr_notice("(0x%x)=0x%x (0x%x)=0x%x\n",
				idx+4, accdet_read(idx+4),
				idx+6, accdet_read(idx+6));
		}
		pr_notice("AUDDEC_ANA_RG\n");
		st_addr = RG_AUDPREAMPLON_ADDR;
		end_addr = RG_CLKSQ_EN_ADDR;
		for (addr = st_addr; addr <= end_addr; addr += 8) {
			idx = addr;
			pr_notice("(0x%x)=0x%x (0x%x)=0x%x ",
				idx, accdet_read(idx),
				idx+2, accdet_read(idx+2));
			pr_notice("(0x%x)=0x%x (0x%x)=0x%x\n",
				idx+4, accdet_read(idx+4),
				idx+6, accdet_read(idx+6));
		}

		pr_notice("(0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x\n",
			RG_RTC32K_CK_PDN_ADDR,
			accdet_read(RG_RTC32K_CK_PDN_ADDR),
			RG_ACCDET_CK_PDN_ADDR,
			accdet_read(RG_ACCDET_CK_PDN_ADDR),
			RG_ACCDET_RST_ADDR,
			accdet_read(RG_ACCDET_RST_ADDR),
			RG_INT_EN_ACCDET_ADDR,
			accdet_read(RG_INT_EN_ACCDET_ADDR));
		pr_notice("(0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x\n",
			RG_INT_MASK_ACCDET_ADDR,
			accdet_read(RG_INT_MASK_ACCDET_ADDR),
			RG_INT_STATUS_ACCDET_ADDR,
			accdet_read(RG_INT_STATUS_ACCDET_ADDR),
			RG_AUDPWDBMICBIAS1_ADDR,
			accdet_read(RG_AUDPWDBMICBIAS1_ADDR),
			RG_AUDACCDETMICBIAS0PULLLOW_ADDR,
			accdet_read(RG_AUDACCDETMICBIAS0PULLLOW_ADDR));
		pr_notice("(0x%x)=0x%x (0x%x)=0x%x\n",
			AUXADC_RQST_CH0_ADDR,
			accdet_read(AUXADC_RQST_CH0_ADDR),
			AUXADC_ACCDET_AUTO_SPL_ADDR,
			accdet_read(AUXADC_ACCDET_AUTO_SPL_ADDR));
		pr_notice("(0x%x)=0x%x\n", RG_HPLOUTPUTSTBENH_VAUDP32_ADDR,
			accdet_read(RG_HPLOUTPUTSTBENH_VAUDP32_ADDR));

		pr_notice("accdet_dts:deb0=0x%x,deb1=0x%x,deb3=0x%x,deb4=0x%x\n",
			 cust_pwm_deb->debounce0, cust_pwm_deb->debounce1,
			 cust_pwm_deb->debounce3, cust_pwm_deb->debounce4);
	} else
		mini_dump_register();
}

static void cat_register(char *buf)
{
	int addr = 0, st_addr = 0, end_addr = 0, idx = 0, ret = 0;

	ret = sprintf(accdet_log_buf, "[Accdet EINTx support][MODE_%d]regs:\n",
		accdet_dts.mic_mode);
	strncat(buf, accdet_log_buf, strlen(accdet_log_buf));

	dump_reg = true;
	dump_register();
	dump_reg = false;
	ret = sprintf(accdet_log_buf, "ACCDET_RG\n");
	if (ret < 0)
		pr_notice("sprintf failed\n");
	strncat(buf, accdet_log_buf, strlen(accdet_log_buf));
	st_addr = ACCDET_AUXADC_SEL_ADDR;
	end_addr = ACCDET_MON_FLAG_EN_ADDR;
	for (addr = st_addr; addr <= end_addr; addr += 8) {
		idx = addr;
		ret = sprintf(accdet_log_buf,
			"(0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x\n",
			idx, accdet_read(idx),
			idx+2, accdet_read(idx+2),
			idx+4, accdet_read(idx+4),
			idx+6, accdet_read(idx+6));
		if (ret < 0)
			pr_notice("sprintf failed\n");
		strncat(buf, accdet_log_buf, strlen(accdet_log_buf));
	}
	ret = sprintf(accdet_log_buf, "AUDDEC_ANA_RG\n");
	if (ret < 0)
		pr_notice("sprintf failed\n");
	strncat(buf, accdet_log_buf, strlen(accdet_log_buf));
	st_addr = RG_AUDPREAMPLON_ADDR;
	end_addr = RG_CLKSQ_EN_ADDR;
	for (addr = st_addr; addr <= end_addr; addr += 8) {
		idx = addr;
		ret = sprintf(accdet_log_buf,
			"(0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x\n",
			idx, accdet_read(idx),
			idx+2, accdet_read(idx+2),
			idx+4, accdet_read(idx+4),
			idx+6, accdet_read(idx+6));
		if (ret < 0)
			pr_notice("sprintf failed\n");
		strncat(buf, accdet_log_buf, strlen(accdet_log_buf));
	}

	ret = sprintf(accdet_log_buf, "[0x%x]=0x%x\n",
		RG_SCK32K_CK_PDN_ADDR,
		accdet_read(RG_SCK32K_CK_PDN_ADDR));
	if (ret < 0)
		pr_notice("sprintf failed\n");
	strncat(buf, accdet_log_buf, strlen(accdet_log_buf));

	ret = sprintf(accdet_log_buf, "[0x%x]=0x%x\n",
		RG_ACCDET_RST_ADDR,
		accdet_read(RG_ACCDET_RST_ADDR));
	if (ret < 0)
		pr_notice("sprintf failed\n");
	strncat(buf, accdet_log_buf, strlen(accdet_log_buf));

	ret = sprintf(accdet_log_buf, "[0x%x]=0x%x, [0x%x]=0x%x, [0x%x]=0x%x\n",
		RG_INT_EN_ACCDET_ADDR,
		accdet_read(RG_INT_EN_ACCDET_ADDR),
		RG_INT_MASK_ACCDET_ADDR,
		accdet_read(RG_INT_MASK_ACCDET_ADDR),
		RG_INT_STATUS_ACCDET_ADDR,
		accdet_read(RG_INT_STATUS_ACCDET_ADDR));
	if (ret < 0)
		pr_notice("sprintf failed\n");
	strncat(buf, accdet_log_buf, strlen(accdet_log_buf));

	ret = sprintf(accdet_log_buf, "[0x%x]=0x%x,[0x%x]=0x%x\n",
		RG_AUDPWDBMICBIAS1_ADDR,
		accdet_read(RG_AUDPWDBMICBIAS1_ADDR),
		RG_AUDACCDETMICBIAS0PULLLOW_ADDR,
		accdet_read(RG_AUDACCDETMICBIAS0PULLLOW_ADDR));
	if (ret < 0)
		pr_notice("sprintf failed\n");
	strncat(buf, accdet_log_buf, strlen(accdet_log_buf));

	ret = sprintf(accdet_log_buf, "[0x%x]=0x%x, [0x%x]=0x%x\n",
		AUXADC_RQST_CH5_ADDR,
		accdet_read(AUXADC_RQST_CH5_ADDR),
		AUXADC_ACCDET_AUTO_SPL_ADDR,
		accdet_read(AUXADC_ACCDET_AUTO_SPL_ADDR));
	if (ret < 0)
		pr_notice("sprintf failed\n");
	strncat(buf, accdet_log_buf, strlen(accdet_log_buf));

	ret = sprintf(accdet_log_buf,
		"dtsInfo:deb0=0x%x,deb1=0x%x,deb3=0x%x,deb4=0x%x\n",
		 cust_pwm_deb->debounce0, cust_pwm_deb->debounce1,
		 cust_pwm_deb->debounce3, cust_pwm_deb->debounce4);
	if (ret < 0)
		pr_notice("sprintf failed\n");
	strncat(buf, accdet_log_buf, strlen(accdet_log_buf));
}

static int dbug_thread(void *unused)
{
	dump_register();

	return 0;
}

static ssize_t start_debug_store(struct device_driver *ddri,
	const char *buf, size_t count)
{
	int error = 0;
	int ret = 0;

	if (strlen(buf) < 1) {
		pr_notice("%s() Invalid input!!\n", __func__);
		return -EINVAL;
	}

	ret = strncmp(buf, "0", 1);
	/* fix syzkaller issue */
	if (debug_thread_en == true) {
		pr_info("%s() debug thread started, ret!\n", __func__);
		return count;
	}

	if (ret) {
		debug_thread_en = true;
		thread = kthread_run(dbug_thread, 0, "ACCDET");
		if (IS_ERR(thread)) {
			error = PTR_ERR(thread);
			pr_notice("%s() create thread failed,err:%d\n",
				__func__, error);
		} else
			pr_info("%s() start debug thread!\n", __func__);
	} else {
		debug_thread_en = false;
		pr_info("%s() stop debug thread!\n", __func__);
	}

	return count;
}

static ssize_t set_reg_store(struct device_driver *ddri,
	const char *buf, size_t count)
{
	int ret = 0;
	u32 addr_tmp = 0;
	u32 value_tmp = 0;

	if (strlen(buf) < 3) {
		pr_notice("%s() Invalid input!!\n", __func__);
		return -EINVAL;
	}

	ret = sscanf(buf, "0x%x,0x%x", &addr_tmp, &value_tmp);
	if (ret < 0)
		return ret;

	pr_info("%s() set addr[0x%x]=0x%x\n", __func__, addr_tmp, value_tmp);

	if (addr_tmp < TOP0_ANA_ID_ADDR)
		pr_notice("%s() Illegal addr[0x%x]!!\n", __func__, addr_tmp);
	else
		accdet_write(addr_tmp, value_tmp);

	return count;
}

static ssize_t dump_reg_show(struct device_driver *ddri, char *buf)
{
	if (buf == NULL) {
		pr_notice("%s() *buf is NULL\n", __func__);
		return -EINVAL;
	}

	cat_register(buf);
	pr_info("%s() buf_size:%d\n", __func__, (int)strlen(buf));

	return strlen(buf);
}

static ssize_t dump_reg_store(struct device_driver *ddri,
	const char *buf, size_t count)
{
	int ret = 0;

	if (strlen(buf) < 1) {
		pr_notice("%s() Invalid input!!\n", __func__);
		return -EINVAL;
	}

	ret = strncmp(buf, "0", 1);
	if (ret) {
		dump_reg = true;
		pr_info("%s() start dump regs!\n", __func__);
	} else {
		dump_reg = false;
		pr_info("%s() stop dump regs!\n", __func__);
	}

	return count;
}

static ssize_t set_headset_mode_store(struct device_driver *ddri,
	const char *buf, size_t count)
{
	int ret = 0;
	int tmp_headset_mode = 0;

	if (strlen(buf) < 1) {
		pr_notice("%s() Invalid input!\n", __func__);
		return -EINVAL;
	}

	ret = kstrtoint(buf, 10, &tmp_headset_mode);
	if (ret < 0) {
		pr_notice("%s() kstrtoint failed! ret:%d\n", __func__, ret);
		return ret;
	}
	pr_info("%s() get mic mode: %d\n", __func__, tmp_headset_mode);

	switch (tmp_headset_mode&0x0F) {
	case HEADSET_MODE_1:
		pr_info("%s() Don't support switch to mode_1!\n", __func__);
		/* accdet_dts.mic_mode = tmp_headset_mode; */
		/* accdet_init(); */
		break;
	case HEADSET_MODE_2:
		accdet_dts.mic_mode = tmp_headset_mode;
		accdet_init();
		accdet_init_debounce();
		break;
	case HEADSET_MODE_6:
		accdet_dts.mic_mode = tmp_headset_mode;
		accdet_init();
		accdet_init_debounce();
		break;
	default:
		pr_info("%s() Invalid mode: %d\n", __func__, tmp_headset_mode);
		break;
	}
	accdet_init_once();

	return count;
}

static ssize_t state_show(struct device_driver *ddri, char *buf)
{
	char temp_type = (char)accdet->cable_type;
	int ret = 0;

	if (buf == NULL) {
		pr_notice("[%s] *buf is NULL!\n",  __func__);
		return -EINVAL;
	}
	ret = snprintf(buf, 3, "%d\n", temp_type);
	if (ret < 0)
		pr_notice("snprintf failed\n");

	return strlen(buf);
}

static DRIVER_ATTR_WO(start_debug);
static DRIVER_ATTR_WO(set_reg);
static DRIVER_ATTR_RW(dump_reg);
static DRIVER_ATTR_WO(set_headset_mode);
static DRIVER_ATTR_RO(state);

static struct driver_attribute *accdet_attr_list[] = {
	&driver_attr_start_debug,
	&driver_attr_set_reg,
	&driver_attr_dump_reg,
	&driver_attr_set_headset_mode,
	&driver_attr_state,
};

static int accdet_create_attr(struct device_driver *driver)
{
	int idx, err;
	int num = ARRAY_SIZE(accdet_attr_list);

	if (driver == NULL)
		return -EINVAL;
	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, accdet_attr_list[idx]);
		if (err) {
			pr_notice("%s() driver_create_file %s err:%d\n",
			__func__, accdet_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

/* get plug-in Resister for audio call */
int accdet_read_audio_res(unsigned int res_value)
{
	pr_info("%s() resister value: R=%u(ohm)\n", __func__, res_value);

	/* if res < 5k ohm normal device;  res >= 5k ohm, lineout device */
	if (res_value < 5000)
		return RET_LT_5K;

	mutex_lock(&accdet->res_lock);
	if (accdet->eint_sync_flag) {
		accdet->cable_type = LINE_OUT_DEVICE;
		accdet->accdet_status = LINE_OUT;
		send_status_event(accdet->cable_type, 1);
		pr_info("%s() update state:%d\n", __func__, accdet->cable_type);
	}
	mutex_unlock(&accdet->res_lock);

	return RET_GT_5K;
}
EXPORT_SYMBOL(accdet_read_audio_res);

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
	int tmp_div;
	unsigned int moisture_eint0;
	unsigned int moisture_eint1;

	/* accdet offset efuse:
	 * this efuse must divided by 2
	 */
	ret = nvmem_device_read(accdet->accdet_efuse, 102*2, 2, &efuseval);
	accdet->auxadc_offset = efuseval & 0xFF;
	if (accdet->auxadc_offset > 128)
		accdet->auxadc_offset -= 256;
	accdet->auxadc_offset = (accdet->auxadc_offset >> 1);
/* all of moisture_vdd/moisture_offset0/eint is  2'complement,
 * we need to transfer it
 */
	/* moisture vdd efuse offset */
	ret = nvmem_device_read(accdet->accdet_efuse, 105*2, 2, &efuseval);
	accdet->moisture_vdd_offset =
		(int)((efuseval >> 8) & ACCDET_CALI_MASK0);
	if (accdet->moisture_vdd_offset > 128)
		accdet->moisture_vdd_offset -= 256;
	pr_info("%s moisture_vdd efuse=0x%x, moisture_vdd_offset=%d mv\n",
		__func__, efuseval, accdet->moisture_vdd_offset);

	/* moisture offset */
	ret = nvmem_device_read(accdet->accdet_efuse, 106*2, 2, &efuseval);
	accdet->moisture_offset = (int)(efuseval & ACCDET_CALI_MASK0);
	if (accdet->moisture_offset > 128)
		accdet->moisture_offset -= 256;
	pr_info("%s moisture_efuse efuse=0x%x,moisture_offset=%d mv\n",
		__func__, efuseval, accdet->moisture_offset);

	if (accdet_dts.moisture_use_ext_res == 0x0) {
		/* moisture eint efuse offset */
		ret = nvmem_device_read(accdet->accdet_efuse,
				104*2, 2, &efuseval);
		moisture_eint0 =
			(int)((efuseval >> 8) & ACCDET_CALI_MASK0);
		pr_info("%s moisture_eint0 efuse=0x%x,moisture_eint0=0x%x\n",
			__func__, efuseval, moisture_eint0);

		ret = nvmem_device_read(accdet->accdet_efuse,
				105*2, 2, &efuseval);
		moisture_eint1 = (int)(efuseval & ACCDET_CALI_MASK0);
		pr_info("%s moisture_eint1 efuse=0x%x,moisture_eint1=0x%x\n",
			__func__, efuseval, moisture_eint1);

		accdet->moisture_eint_offset =
			(moisture_eint1 << 8) | moisture_eint0;
		if (accdet->moisture_eint_offset > 32768)
			accdet->moisture_eint_offset -= 65536;
		pr_info("%s moisture_eint_offset=%d ohm\n", __func__,
			accdet->moisture_eint_offset);

		accdet->moisture_vm = (2800 + accdet->moisture_vdd_offset);
		accdet->moisture_vm *=
			(accdet->water_r + accdet->moisture_int_r);
		tmp_div = accdet->water_r + accdet->moisture_int_r +
			8 * accdet->moisture_eint_offset + 450000;
		accdet->moisture_vm = accdet->moisture_vm / tmp_div;
		accdet->moisture_vm =
			accdet->moisture_vm + accdet->moisture_offset / 2;
		pr_info("%s internal moisture_vm=%d mv\n", __func__,
			accdet->moisture_vm);
	} else if (accdet_dts.moisture_use_ext_res == 0x1) {
		accdet->moisture_vm = (2800 + accdet->moisture_vdd_offset);
		accdet->moisture_vm = accdet->moisture_vm * accdet->water_r;
		accdet->moisture_vm /=
			(accdet->water_r + accdet->moisture_ext_r);
		accdet->moisture_vm +=
			(accdet->moisture_offset >> 1);
		pr_info("%s external moisture_vm=%d mv\n", __func__,
			accdet->moisture_vm);
	}
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
		bool irq_bit = false;

		irq_bit = !(accdet_read(ACCDET_IRQ_ADDR) &
					ACCDET_EINT_IRQ_B2_B3);
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
	accdet_update_bit(ACCDET_IRQ_ADDR, ACCDET_IRQ_CLR_SFT);
}

static inline void clear_accdet_int_check(void)
{
	u64 cur_time = accdet_get_current_time();

	while ((accdet_read(ACCDET_IRQ_ADDR) & ACCDET_IRQ_MASK_SFT) &&
		(accdet_timeout_ns(cur_time, ACCDET_TIME_OUT)))
		;
	/* clear accdet int, modify  for fix interrupt trigger twice error */
	accdet_clear_bit(ACCDET_IRQ_ADDR, ACCDET_IRQ_CLR_SFT);
	accdet_update_bit(RG_INT_STATUS_ACCDET_ADDR,
		RG_INT_STATUS_ACCDET_SFT);
}

static inline void clear_accdet_eint(u32 eintid)
{
	if ((eintid & PMIC_EINT0) == PMIC_EINT0) {
		accdet_update_bit(ACCDET_IRQ_ADDR,
			ACCDET_EINT0_IRQ_CLR_SFT);
	}
	if ((eintid & PMIC_EINT1) == PMIC_EINT1) {
		accdet_update_bit(ACCDET_IRQ_ADDR,
			ACCDET_EINT1_IRQ_CLR_SFT);
	}

}

static inline void clear_accdet_eint_check(u32 eintid)
{
	u64 cur_time = accdet_get_current_time();
	u32 addr = 0;

	addr = ACCDET_IRQ_ADDR;
	if ((eintid & PMIC_EINT0) == PMIC_EINT0) {
		while ((accdet_read(addr) & ACCDET_EINT0_IRQ_MASK_SFT)
			&& (accdet_timeout_ns(cur_time, ACCDET_TIME_OUT)))
			;
		accdet_clear_bit(ACCDET_IRQ_ADDR,
				ACCDET_EINT0_IRQ_CLR_SFT);
		accdet_update_bit(RG_INT_STATUS_ACCDET_ADDR,
				RG_INT_STATUS_ACCDET_EINT0_SFT);
	}
	if ((eintid & PMIC_EINT1) == PMIC_EINT1) {
		while ((accdet_read(addr) & ACCDET_EINT1_IRQ_MASK_SFT)
			&& (accdet_timeout_ns(cur_time, ACCDET_TIME_OUT)))
			;
		accdet_clear_bit(ACCDET_IRQ_ADDR,
				ACCDET_EINT1_IRQ_CLR_SFT);
		accdet_update_bit(RG_INT_STATUS_ACCDET_ADDR,
				RG_INT_STATUS_ACCDET_EINT1_SFT);
	}
}

static u32 get_moisture_det_en(void)
{
	return accdet_dts.moisture_detect_enable;
}
static u32 get_moisture_sw_auxadc_check(void)
{
	unsigned int moisture_vol = 0;

	if (accdet_dts.moisture_detect_mode == 0x1 ||
		accdet_dts.moisture_detect_mode == 0x2 ||
		accdet_dts.moisture_detect_mode == 0x3) {
		if (accdet->cur_eint_state == EINT_MOISTURE_DETECTED) {
			pr_info("%s Moisture plug out detectecd\n", __func__);
			accdet->cur_eint_state = EINT_PLUG_OUT;
			return M_PLUG_OUT;
		}
		if (accdet->cur_eint_state == EINT_PLUG_OUT) {
			pr_info("%s now check moisture\n", __func__);
			moisture_vol = accdet_get_auxadc();
			pr_info("moisture_vol:0x%x, moisture_vm:0x%x\r",
				moisture_vol, accdet->moisture_vm);
			if (moisture_vol > accdet->moisture_vm) {
				pr_info("%s water in detectecd!\n",
					__func__);
				return M_WATER_IN;
			} else {
				return M_HP_PLUG_IN;
			}
			pr_info("%s check moisture done,not water.\n",
				__func__);
		}
	}
	return M_NO_ACT;
}

static u32 adjust_eint_analog_setting(void)
{
	if ((accdet_dts.eint_detect_mode == 0x3) ||
		(accdet_dts.eint_detect_mode == 0x4)) {
		/* ESD switches off */
		accdet_clear_bit(RG_ACCDETSPARE_ADDR, 8);
	}
	if (accdet_dts.eint_detect_mode == 0x4) {
		if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT0)) {
			/* enable RG_EINT0CONFIGACCDET */
			accdet_update_bit(RG_EINT0CONFIGACCDET_ADDR,
				RG_EINT0CONFIGACCDET_SFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT1)) {
			/* enable RG_EINT1CONFIGACCDET */
			accdet_update_bit(RG_EINT1CONFIGACCDET_ADDR,
				RG_EINT1CONFIGACCDET_SFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_BI_EINT)) {
			/* enable RG_EINT0CONFIGACCDET */
			accdet_update_bit(RG_EINT0CONFIGACCDET_ADDR,
				RG_EINT0CONFIGACCDET_SFT);
			/* enable RG_EINT1CONFIGACCDET */
			accdet_update_bit(RG_EINT1CONFIGACCDET_ADDR,
				RG_EINT1CONFIGACCDET_SFT);
		}
		if ((accdet_dts.eint_use_ext_res == 0x3) ||
			(accdet_dts.eint_use_ext_res == 0x4)) {
			/*select 500k, use internal resistor */
			accdet_update_bit(RG_EINT0HIRENB_ADDR,
				RG_EINT0HIRENB_SFT);
		}
	}
	return 0;
}

static u32 adjust_eint_digital_setting(void)
{
	unsigned int ret = 0;

	if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT0)) {
		/* disable inverter */
		accdet_clear_bit(ACCDET_EINT0_INVERTER_SW_EN_ADDR,
			ACCDET_EINT0_INVERTER_SW_EN_SFT);
	} else if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT1)) {
		/* disable inverter */
		accdet_clear_bit(ACCDET_EINT1_INVERTER_SW_EN_ADDR,
			ACCDET_EINT1_INVERTER_SW_EN_SFT);
	} else if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_BI_EINT)) {
		/* disable inverter */
		accdet_clear_bit(ACCDET_EINT0_INVERTER_SW_EN_ADDR,
			ACCDET_EINT0_INVERTER_SW_EN_SFT);
		/* disable inverter */
		accdet_clear_bit(ACCDET_EINT1_INVERTER_SW_EN_ADDR,
			ACCDET_EINT1_INVERTER_SW_EN_SFT);
	}
	if ((accdet_dts.eint_detect_mode == 0x1) ||
		(accdet_dts.eint_detect_mode == 0x2)) {
		accdet_clear_bit(ACCDET_DA_STABLE_ADDR,
			ACCDET_EINT0_CEN_STABLE_SFT);
		ret = get_moisture_sw_auxadc_check();
		/* disable mtest en */
		accdet_clear_bit(RG_MTEST_EN_ADDR,
				RG_MTEST_EN_SFT);
		accdet_clear_bit(AUDACCDETAUXADCSWCTRL_SEL_ADDR,
			AUDACCDETAUXADCSWCTRL_SEL_SFT);
		return ret;
	}
	if (accdet_dts.eint_detect_mode == 0x3) {
		accdet_clear_bit(ACCDET_DA_STABLE_ADDR,
			ACCDET_EINT0_CEN_STABLE_SFT);
		accdet_update_bit(ACCDET_EINT_CTURBO_SEL_ADDR,
			ACCDET_EINT_CTURBO_SEL_SFT);
		accdet_update_bit(ACCDET_EINT0_CTURBO_SW_ADDR,
			ACCDET_EINT0_CTURBO_SW_SFT);
		pr_info("auxadc T1\r");
		ret = get_moisture_sw_auxadc_check();
		if ((ret == M_WATER_IN) || (ret == M_HP_PLUG_IN)) {
			accdet_clear_bit(ACCDET_EINT_CTURBO_SEL_ADDR,
				ACCDET_EINT_CTURBO_SEL_SFT);
			accdet_clear_bit(ACCDET_EINT0_CTURBO_SW_ADDR,
				ACCDET_EINT0_CTURBO_SW_SFT);
		}
		return ret;
	}

	if (accdet_dts.eint_detect_mode == 0x4) {
		if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT0)) {
			/* set DA stable signal */
			accdet_clear_bit(ACCDET_DA_STABLE_ADDR,
				ACCDET_EINT0_CEN_STABLE_SFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT1)) {
			/* set DA stable signal */
			accdet_clear_bit(ACCDET_DA_STABLE_ADDR,
				ACCDET_EINT1_CEN_STABLE_SFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_BI_EINT)) {
			/* set DA stable signal */
			accdet_clear_bit(ACCDET_DA_STABLE_ADDR,
				ACCDET_EINT0_CEN_STABLE_SFT);
			/* set DA stable signal */
			accdet_clear_bit(ACCDET_DA_STABLE_ADDR,
				ACCDET_EINT1_CEN_STABLE_SFT);
		}
	}
	return 0;
}
static u32 adjust_moisture_digital_setting(u32 eintID)
{
	if ((accdet_dts.moisture_detect_mode == 0x1) ||
		(accdet_dts.moisture_detect_mode == 0x2) ||
		(accdet_dts.moisture_detect_mode == 0x3)) {

		if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT0)) {
			/* wk1, enable moisture detection */
			accdet_update_bit(ACCDET_EINT0_M_SW_EN_ADDR,
				ACCDET_EINT0_M_SW_EN_SFT);
		} else if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT1)) {
			/* wk1, enable moisture detection */
			accdet_update_bit(ACCDET_EINT1_M_SW_EN_ADDR,
				ACCDET_EINT1_M_SW_EN_SFT);
		} else if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_BI_EINT)) {
			/* wk1, enable moisture detection */
			accdet_update_bit(ACCDET_EINT0_M_SW_EN_ADDR,
				ACCDET_EINT0_M_SW_EN_SFT);
			accdet_update_bit(ACCDET_EINT1_M_SW_EN_ADDR,
				ACCDET_EINT1_M_SW_EN_SFT);
		}
	}
	return 0;
}

static u32 adjust_moisture_analog_setting(u32 eintID)
{
	unsigned int efuseval = 0, vref2val = 0, vref2hi = 0;

	if (accdet_dts.moisture_detect_mode == 0x1) {
		/* select VTH to 2.8v(default), can set to 2.4 or 2.0v by dts */
		accdet_update_bits(RG_EINTCOMPVTH_ADDR,
			RG_EINTCOMPVTH_SFT,
			RG_EINTCOMPVTH_MASK,
			accdet_dts.moisture_comp_vth);
	} else if ((accdet_dts.moisture_detect_mode == 0x2) ||
		(accdet_dts.moisture_detect_mode == 0x3)) {
		/* select VTH to 2.8v(default), can set to 2.4 or 2.0v by dts */
		accdet_update_bits(RG_EINTCOMPVTH_ADDR,
			RG_EINTCOMPVTH_SFT,
			RG_EINTCOMPVTH_MASK,
			accdet_dts.moisture_comp_vth);

		/* Enable mtest en */
		accdet_update_bit(RG_MTEST_EN_ADDR,
				RG_MTEST_EN_SFT);

		/* select PAD_HP_EINT for moisture detection */
		accdet_clear_bit(RG_MTEST_SEL_ADDR,
				RG_MTEST_SEL_SFT);
	} else if (accdet_dts.moisture_detect_mode == 0x4) {
		/* do nothing */
	} else if (accdet_dts.moisture_detect_mode == 0x5) {
		/* enable VREF2 */
		vref2hi = 0x1;
		switch (accdet_dts.moisture_comp_vref2) {
		case 0:
			vref2val = 0x3;
			break;
		case 1:
			vref2val = 0x7;
			break;
		case 2:
			vref2val = 0xc;
			break;
		default:
			vref2val = 0x3;
			break;
		}
		pr_info("%s efuse=0x%x,vref2val=0x%x, vref2hi=0x%x\n",
			__func__, efuseval, vref2val, vref2hi);
		/* voltage 880~1330mV */
		accdet_update_bit(RG_ACCDETSPARE_ADDR, 15);
		accdet_update_bits(RG_EINTCOMPVTH_ADDR,
			6, 0x3, (vref2val & 0xc) >> 2);
		accdet_update_bits(RG_ACCDETSPARE_ADDR,
			13, 0x3, (vref2val & 0x3));
		/* golden setting
		 * accdet_update_bits(RG_ACCDETSPARE_ADDR,
		 * 3, 0x1f, 0x1e);
		 */
	}
	return 0;
}

static u32 adjust_moisture_setting(u32 moistureID, u32 eintID)
{
	unsigned int ret = 0;

	if (moistureID == M_PLUG_IN) {
		if (accdet->thing_in_flag == true) {
			/* receive M_PLUG_IN second time, just clear irq sts */
			clear_accdet_eint(eintID);
			clear_accdet_eint_check(eintID);
		} else {
			/* to check if 1st time thing in interrupt */
			accdet->thing_in_flag = true;
			accdet->cur_eint_state = EINT_PLUG_OUT;
			/* adjust analog moisture setting */
			adjust_moisture_analog_setting(eintID);
			if (accdet_dts.moisture_detect_mode != 0x5) {
				/* wk2 */
				accdet_update_bit(RG_EINT0HIRENB_ADDR,
					RG_EINT0HIRENB_SFT);
			}
			/* adjust digital setting */
			ret = adjust_eint_digital_setting();
			/* adjust digital moisture setting */
			adjust_moisture_digital_setting(eintID);
			/* sw axuadc check, EINT 1.0~ EINT 2.0 */
			clear_accdet_eint(eintID);
			clear_accdet_eint_check(eintID);
			if (ret != 0)
				return ret;
			if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT0)) {
				/* wk1, enable moisture detection */
				accdet_update_bit(ACCDET_EINT0_M_SW_EN_ADDR,
					ACCDET_EINT0_M_SW_EN_SFT);
			} else if (HAS_CAP(accdet->data->caps,
					ACCDET_PMIC_EINT1)) {
				/* wk1, enable moisture detection */
				accdet_update_bit(ACCDET_EINT1_M_SW_EN_ADDR,
					ACCDET_EINT1_M_SW_EN_SFT);
			} else if (HAS_CAP(accdet->data->caps,
					ACCDET_PMIC_BI_EINT)) {
				/* wk1, enable moisture detection */
				accdet_update_bit(ACCDET_EINT0_M_SW_EN_ADDR,
					ACCDET_EINT0_M_SW_EN_SFT);
				accdet_update_bit(ACCDET_EINT1_M_SW_EN_ADDR,
					ACCDET_EINT1_M_SW_EN_SFT);
			}
			pr_info("%s() , thing in done\n", __func__);
		}
		return M_NO_ACT;
	} else if (moistureID == M_WATER_IN) {
		accdet->cur_eint_state = EINT_MOISTURE_DETECTED;
		if (accdet_dts.moisture_detect_mode == 0x5) {
			/* Case 5: water in need 128ms to detect plug out
			 * set debounce to 128ms
			 * this value shold less then eint_thresh
			 */
			accdet_set_debounce(eint_state011,
				accdet_dts.pwm_deb.eint_debounce3);
		} else {
			/* water in need 0.12ms to detect plug out
			 * set debounce to 0.12ms
			 * this value shold less then eint_thresh
			 */
			accdet_set_debounce(eint_state011, 0x1);
		}
		clear_accdet_eint(eintID);
		clear_accdet_eint_check(eintID);
		return M_NO_ACT;
	} else if (moistureID == M_HP_PLUG_IN) {
		/* water in then HP in, recover state */
		if (accdet->cur_eint_state == EINT_MOISTURE_DETECTED)
			accdet->cur_eint_state = EINT_PLUG_OUT;

		if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT0)) {
			/* wk1, disable moisture detection */
			accdet_clear_bit(ACCDET_EINT0_M_SW_EN_ADDR,
				ACCDET_EINT0_M_SW_EN_SFT);
		} else if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT1)) {
			/* wk1, disable moisture detection */
			accdet_clear_bit(ACCDET_EINT1_M_SW_EN_ADDR,
			ACCDET_EINT1_M_SW_EN_SFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_BI_EINT)) {
			/* wk1, disable moisture detection */
			accdet_clear_bit(ACCDET_EINT0_M_SW_EN_ADDR,
				ACCDET_EINT0_M_SW_EN_SFT);
			accdet_clear_bit(ACCDET_EINT1_M_SW_EN_ADDR,
				ACCDET_EINT1_M_SW_EN_SFT);
		}
		/* wk3, if HP + W together, after detect HP, we should
		 * set accdet_sync_flag to true to avoid receive W interrupt
		 */
		accdet->eint_sync_flag = true;

		adjust_eint_analog_setting();
		/* set debounce to 2ms */
		accdet_set_debounce(eint_state000, 0x6);
	} else if (moistureID == M_PLUG_OUT) {
		/* set debounce to 1ms */
		accdet_set_debounce(eint_state000,
			accdet_dts.pwm_deb.eint_debounce0);
	} else {
		pr_debug("should not be here %s()\n", __func__);
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
	} else {
		pr_debug("should not be here %s()\n", __func__);
	}

	return 0;
}

static void recover_eint_analog_setting(void)
{
	if ((accdet_dts.eint_detect_mode == 0x3) ||
		(accdet_dts.eint_detect_mode == 0x4)) {
		/* ESD switches on */
		accdet_update_bit(RG_ACCDETSPARE_ADDR, 8);
	}
	if (accdet_dts.eint_detect_mode == 0x4) {
		if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT0)) {
			/* disable RG_EINT0CONFIGACCDET */
			accdet_clear_bit(RG_EINT0CONFIGACCDET_ADDR,
				RG_EINT0CONFIGACCDET_SFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT1)) {
			/* disable RG_EINT1CONFIGACCDET */
			accdet_clear_bit(RG_EINT1CONFIGACCDET_ADDR,
				RG_EINT1CONFIGACCDET_SFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_BI_EINT)) {
			/* disable RG_EINT0CONFIGACCDET */
			accdet_clear_bit(RG_EINT0CONFIGACCDET_ADDR,
				RG_EINT0CONFIGACCDET_SFT);
			/* disable RG_EINT0CONFIGACCDET */
			accdet_clear_bit(RG_EINT1CONFIGACCDET_ADDR,
				RG_EINT1CONFIGACCDET_SFT);
		}
		accdet_clear_bit(RG_EINT0HIRENB_ADDR,
			RG_EINT0HIRENB_SFT);
	}

}
static void recover_eint_digital_setting(void)
{
	if (HAS_CAP(accdet->data->caps,
			ACCDET_PMIC_EINT0)) {
		accdet_clear_bit(ACCDET_EINT0_M_SW_EN_ADDR,
			ACCDET_EINT0_M_SW_EN_SFT);
	} else if (HAS_CAP(accdet->data->caps,
			ACCDET_PMIC_EINT1)) {
		accdet_clear_bit(ACCDET_EINT1_M_SW_EN_ADDR,
			ACCDET_EINT1_M_SW_EN_SFT);
	} else if (HAS_CAP(accdet->data->caps,
			ACCDET_PMIC_BI_EINT)) {
		accdet_clear_bit(ACCDET_EINT0_M_SW_EN_ADDR,
			ACCDET_EINT0_M_SW_EN_SFT);
		accdet_clear_bit(ACCDET_EINT1_M_SW_EN_ADDR,
			ACCDET_EINT1_M_SW_EN_SFT);
	}
	if (accdet_dts.eint_detect_mode == 0x4) {
		/* enable eint0cen */
		if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT0)) {
			/* enable eint0cen */
			accdet_update_bit(ACCDET_DA_STABLE_ADDR,
				ACCDET_EINT0_CEN_STABLE_SFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT1)) {
			/* enable eint1cen */
			accdet_update_bit(ACCDET_DA_STABLE_ADDR,
				ACCDET_EINT1_CEN_STABLE_SFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_BI_EINT)) {
			/* enable eint0cen */
			accdet_update_bit(ACCDET_DA_STABLE_ADDR,
				ACCDET_EINT0_CEN_STABLE_SFT);
			/* enable eint1cen */
			accdet_update_bit(ACCDET_DA_STABLE_ADDR,
				ACCDET_EINT1_CEN_STABLE_SFT);
		}
	}

	if (accdet_dts.eint_detect_mode != 0x1) {
		if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT0)) {
			/* enable inverter */
			accdet_update_bit(ACCDET_EINT0_INVERTER_SW_EN_ADDR,
				ACCDET_EINT0_INVERTER_SW_EN_SFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT1)) {
			/* enable inverter */
			accdet_update_bit(ACCDET_EINT1_INVERTER_SW_EN_ADDR,
				ACCDET_EINT1_INVERTER_SW_EN_SFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_BI_EINT)) {
			/* enable inverter */
			accdet_update_bit(ACCDET_EINT0_INVERTER_SW_EN_ADDR,
				ACCDET_EINT0_INVERTER_SW_EN_SFT);
			/* enable inverter */
			accdet_update_bit(ACCDET_EINT1_INVERTER_SW_EN_ADDR,
				ACCDET_EINT1_INVERTER_SW_EN_SFT);
		}
	}
}

static void recover_moisture_analog_setting(void)
{
	if (accdet_dts.moisture_detect_mode == 0x1) {
		/* select VTH to 2v */
		accdet_update_bits(RG_EINTCOMPVTH_ADDR,
			RG_EINTCOMPVTH_SFT,
			RG_EINTCOMPVTH_MASK, 0x2);
	} else if (accdet_dts.moisture_detect_mode == 0x2) {
	} else if (accdet_dts.moisture_detect_mode == 0x3) {
	} else if (accdet_dts.moisture_detect_mode == 0x4) {
	} else if (accdet_dts.moisture_detect_mode == 0x5) {
		/* enable comp1 delay window */
		accdet_clear_bit(RG_EINT0NOHYS_ADDR,
			RG_EINT0NOHYS_SFT);

		accdet_set_debounce(eint_state011,
			accdet_dts.pwm_deb.eint_debounce3);
		/* disconnect VREF2 to EINT0CMP and recover to vref */
		accdet_update_bits(RG_EINTCOMPVTH_ADDR,
			RG_EINTCOMPVTH_SFT, RG_EINTCOMPVTH_MASK,
			accdet_dts.moisture_comp_vth);
	}
}

static void recover_moisture_setting(u32 moistureID)
{
	if (moistureID == M_HP_PLUG_IN) {
		/* set debounce to 2ms */
		accdet_set_debounce(eint_state000, 0x6);
	} else if (moistureID == M_PLUG_OUT) {
		/* set debounce to 1ms */
		accdet_set_debounce(eint_state000,
			accdet_dts.pwm_deb.eint_debounce0);
		recover_eint_analog_setting();
		recover_moisture_analog_setting();
		recover_eint_digital_setting();
		pr_info("%s done\n", __func__);

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
	u32 irq_status = accdet_read(ACCDET_IRQ_ADDR);

	if (HAS_CAP(accdet->data->caps,
			ACCDET_PMIC_EINT0)) {
		if ((irq_status & ACCDET_EINT0_IRQ_MASK_SFT) ==
				ACCDET_EINT0_IRQ_MASK_SFT)
			eint_ID = PMIC_EINT0;
	} else if (HAS_CAP(accdet->data->caps,
			ACCDET_PMIC_EINT1)) {
		if ((irq_status & ACCDET_EINT1_IRQ_MASK_SFT) ==
				ACCDET_EINT1_IRQ_MASK_SFT)
			eint_ID = PMIC_EINT1;
	} else if (HAS_CAP(accdet->data->caps,
			ACCDET_PMIC_BI_EINT)) {
		if ((irq_status & ACCDET_EINT0_IRQ_MASK_SFT) ==
				ACCDET_EINT0_IRQ_MASK_SFT)
			eint_ID |= PMIC_EINT0;
		if ((irq_status & ACCDET_EINT1_IRQ_MASK_SFT) ==
				ACCDET_EINT1_IRQ_MASK_SFT)
			eint_ID |= PMIC_EINT1;
	}
	return eint_ID;
}

static inline void enable_accdet(u32 state_swctrl)
{
	/* enable ACCDET unit */
	accdet_update_bit(ACCDET_SW_EN_ADDR, ACCDET_SW_EN_SFT);
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
	accdet_set_debounce(accdet_state001, cust_pwm_deb->debounce1);
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
	u32 cur_AB = 0, eintID = 0;

	/* check EINT0 status, if plug out,
	 * not need to disable accdet here
	 */
	eintID = accdet_read_bits(ACCDET_EINT0_MEM_IN_ADDR,
		ACCDET_EINT0_MEM_IN_SFT,
		ACCDET_EINT0_MEM_IN_MASK);
	if (eintID == M_PLUG_OUT) {
		pr_notice("%s Plug-out, no dis micbias\n", __func__);
		return;
	}
	/* if modify_vref_volt called, not need to dis micbias again */
	if (dis_micbias_done == true) {
		pr_notice("%s modify_vref_volt called\n", __func__);
		return;
	}

	cur_AB = accdet_read(ACCDET_MEM_IN_ADDR) >> ACCDET_STATE_MEM_IN_OFFSET;
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
		accdet_clear_bit(ACCDET_SW_EN_ADDR,
			ACCDET_SW_EN_SFT);
		disable_accdet();
	}
}

static void eint_work_callback(struct work_struct *work)
{
	if (accdet->cur_eint_state == EINT_PLUG_IN) {
		/* disable vusb LP */
		accdet_write(RG_LDO_VUSB_HW0_OP_EN_ADDR, 0x8000);

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
		if (accdet_dts.moisture_detect_mode != 0x5)
			del_timer_sync(&micbias_timer);

		/* disable accdet_sw_en=0
		 */
		accdet_clear_bit(ACCDET_SW_EN_ADDR,
			ACCDET_SW_EN_SFT);
		disable_accdet();
		headset_plug_out();
	}

	if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT_IRQ)) {
		if (get_moisture_det_en() == 0x1)
			recover_moisture_setting(accdet->eint_id);
		else
			recover_eint_setting(accdet->eint_id);
	} else if (HAS_CAP(accdet->data->caps, ACCDET_AP_GPIO_EINT))
		enable_irq(accdet->gpioirq);
}

void accdet_set_debounce(int state, unsigned int debounce)
{
	switch (state) {
	case accdet_state000:
		/* set ACCDET debounce value = debounce/32 ms */
		accdet_write(ACCDET_DEBOUNCE0_ADDR, debounce);
		break;
	case accdet_state001:
		accdet_write(ACCDET_DEBOUNCE1_ADDR, debounce);
		break;
	case accdet_state010:
		accdet_write(ACCDET_DEBOUNCE2_ADDR, debounce);
		break;
	case accdet_state011:
		accdet_write(ACCDET_DEBOUNCE3_ADDR, debounce);
		break;
	case accdet_auxadc:
		/* set auxadc debounce:0x42(2ms) */
		accdet_write(ACCDET_CONNECT_AUXADC_TIME_DIG_ADDR,
			debounce);
		break;
	case eint_state000:
		accdet_update_bits(ACCDET_EINT_DEBOUNCE0_ADDR,
				ACCDET_EINT_DEBOUNCE0_SFT,
				ACCDET_EINT_DEBOUNCE0_MASK,
				debounce);
		break;
	case eint_state001:
		accdet_update_bits(ACCDET_EINT_DEBOUNCE1_ADDR,
				ACCDET_EINT_DEBOUNCE1_SFT,
				ACCDET_EINT_DEBOUNCE1_MASK,
				debounce);
		break;
	case eint_state010:
		accdet_update_bits(ACCDET_EINT_DEBOUNCE2_ADDR,
				ACCDET_EINT_DEBOUNCE2_SFT,
				ACCDET_EINT_DEBOUNCE2_MASK,
				debounce);
		break;
	case eint_state011:
		accdet_update_bits(ACCDET_EINT_DEBOUNCE3_ADDR,
				ACCDET_EINT_DEBOUNCE3_SFT,
				ACCDET_EINT_DEBOUNCE3_MASK,
				debounce);
		break;
	case eint_inverter_state000:
		accdet_write(ACCDET_EINT_INVERTER_DEBOUNCE_ADDR,
			debounce);
		break;
	default:
		pr_notice("Error: %s error state (%d)\n", __func__, state);
		break;
	}
}

static inline void check_cable_type(void)
{
	u32 cur_AB = 0;

	cur_AB = accdet_read(ACCDET_MEM_IN_ADDR) >> ACCDET_STATE_MEM_IN_OFFSET;
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

			/* adjust debounce1 to original 0x800(64ms),
			 * to fix miss key issue when fast press double key.
			 */
			accdet_set_debounce(accdet_state001,
				button_press_debounce_01);
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

			/* adjust debounce0 and debounce1 to fix miss key issue.
			 */
			accdet_set_debounce(accdet_state000,
				button_press_debounce);
			accdet_set_debounce(accdet_state001,
				button_press_debounce_01);
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
		accdet_modify_vref_volt_self();
		/* enable vusb LP */
		accdet_write(RG_LDO_VUSB_HW0_OP_EN_ADDR, 0x8005);
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
	int ret = 0, mode = 0;

	if (accdet->cur_eint_state == EINT_MOISTURE_DETECTED) {
		pr_info("%s water in then plug out, handle plugout\r",
			__func__);
		accdet->cur_eint_state = EINT_PLUG_OUT;
		ret = queue_work(accdet->eint_workqueue, &accdet->eint_work);
		return 0;
	}
	if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT0)) {
		if (eintID == PMIC_EINT0) {
			if (accdet->cur_eint_state == EINT_PLUG_IN) {
				accdet_set_debounce(accdet_state011,
					cust_pwm_deb->debounce3);
				accdet->cur_eint_state = EINT_PLUG_OUT;
			} else {
				if (accdet->eint_id != M_PLUG_OUT) {
					accdet->cur_eint_state = EINT_PLUG_IN;
					mode = accdet_dts.moisture_detect_mode;
					if (mode != 0x5) {
						mod_timer(&micbias_timer,
						jiffies+MICBIAS_DISABLE_TIMER);
					}
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
					mode = accdet_dts.moisture_detect_mode;
					if (mode != 0x5) {
						mod_timer(&micbias_timer,
						jiffies+MICBIAS_DISABLE_TIMER);
					}
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
				if (accdet_dts.moisture_detect_mode != 0x5) {
					mod_timer(&micbias_timer,
					jiffies + MICBIAS_DISABLE_TIMER);
				}
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

static u32 config_moisture_detect_1_0(void)
{
	/* Disable ACCDET to AUXADC */
	accdet_clear_bit(RG_ACCDET2AUXSWEN_ADDR,
		RG_ACCDET2AUXSWEN_SFT);
	accdet_update_bit(AUDACCDETAUXADCSWCTRL_SEL_ADDR,
		AUDACCDETAUXADCSWCTRL_SEL_SFT);
	accdet_clear_bit(AUDACCDETAUXADCSWCTRL_SW_ADDR,
		AUDACCDETAUXADCSWCTRL_SW_SFT);

	/* Enable moisture detection */
	accdet_update_bit(RG_MTEST_EN_ADDR, RG_MTEST_EN_SFT);

	/* select PAD_HP_EINT for moisture detection */
	accdet_clear_bit(RG_MTEST_SEL_ADDR, RG_MTEST_SEL_SFT);

	/* select VTH to 2v */
	accdet_update_bits(RG_EINTCOMPVTH_ADDR,
		RG_EINTCOMPVTH_SFT, RG_EINTCOMPVTH_MASK, 0x2);

	return 0;
}

static u32 config_moisture_detect_1_1(void)
{
	/* Disable ACCDET to AUXADC */
	accdet_clear_bit(RG_ACCDET2AUXSWEN_ADDR,
		RG_ACCDET2AUXSWEN_SFT);
	accdet_update_bit(AUDACCDETAUXADCSWCTRL_SEL_ADDR,
		AUDACCDETAUXADCSWCTRL_SEL_SFT);
	accdet_clear_bit(AUDACCDETAUXADCSWCTRL_SW_ADDR,
		AUDACCDETAUXADCSWCTRL_SW_SFT);

	/* select VTH to 2v */
	accdet_update_bits(RG_EINTCOMPVTH_ADDR,
		RG_EINTCOMPVTH_SFT, RG_EINTCOMPVTH_MASK, 0x2);

	return 0;
}

static u32 config_moisture_detect_2_1(void)
{
	u32 efuseval = 0, eintvth = 0, ret = 0;
	/* select VTH to 2.8v(default), can set to 2.4 or 2.0v by dts */
	accdet_update_bits(RG_EINTCOMPVTH_ADDR,
		RG_EINTCOMPVTH_SFT, RG_EINTCOMPVTH_MASK,
		accdet_dts.moisture_comp_vth);
	/* 2. Set RG_EINT0CTURBO<2:0>  ; 75k~15k
	 * read efuse:
	 * 3. Set RG_ACCDETSPARE<7:3> ; VREF2
	 * read efuse:
	 */
	if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT0)) {
		/* enable cturbo setting */
		accdet_update_bit(RG_EINT0CTURBO_ADDR,
				RG_EINT0CTURBO_SFT);
	} else if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT1)) {
		accdet_update_bit(RG_EINT1CTURBO_ADDR,
				RG_EINT1CTURBO_SFT);
	} else if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_BI_EINT)) {
		accdet_update_bit(RG_EINT0CTURBO_ADDR,
				RG_EINT0CTURBO_SFT);
		accdet_update_bit(RG_EINT1CTURBO_ADDR,
				RG_EINT1CTURBO_SFT);
	}
	/* set moisture reference voltage MVTH
	 * golden setting
	 * accdet_update_bits(RG_ACCDETSPARE_ADDR, 0x3, 0x1F, 0xA);
	 */

	/* EINTVTH1K/5K/10K efuse */
	ret = nvmem_device_read(accdet->accdet_efuse, 114*2, 2, &efuseval);
	eintvth = (int)(efuseval & ACCDET_CALI_MASK0);
	pr_info("%s moisture_eint0 efuse=0x%x,eintvth=0x%x\n",
		__func__, efuseval, eintvth);

	/* set moisture reference voltage MVTH */
	accdet_update_bits(RG_ACCDETSPARE_ADDR, 0x3, 0x1F, eintvth);
	return 0;
}

static u32 config_moisture_detect_2_1_1(void)
{
	/* select VTH to 2.8v(default), can set to 2.4 or 2.0v by dts */
	accdet_update_bits(RG_EINTCOMPVTH_ADDR,
		RG_EINTCOMPVTH_SFT, RG_EINTCOMPVTH_MASK,
		accdet_dts.moisture_comp_vth);

	return 0;
}
void accdet_irq_handle(void)
{
	u32 eintID = 0, ret = 0;
	u32 irq_status = 0, acc_sts = 0, eint_sts = 0;

	eintID = get_triggered_eint();
	irq_status = accdet_read(ACCDET_IRQ_ADDR);
	acc_sts = accdet_read(ACCDET_MEM_IN_ADDR);
	eint_sts = accdet_read(ACCDET_EINT0_MEM_IN_ADDR);

	if ((irq_status & ACCDET_IRQ_MASK_SFT) && (eintID == 0)) {
		clear_accdet_int();
		accdet_queue_work();
		clear_accdet_int_check();
	} else if (eintID != NO_PMIC_EINT) {
		/* check EINT0 status */
		accdet->eint_id = accdet_read_bits(
			ACCDET_EINT0_MEM_IN_ADDR,
			ACCDET_EINT0_MEM_IN_SFT,
			ACCDET_EINT0_MEM_IN_MASK);
		if (get_moisture_det_en() == 0x1) {
			/* adjust moisture digital/analog setting */
			ret = adjust_moisture_setting(accdet->eint_id, eintID);
			if (ret == M_NO_ACT)
				return;
		} else {
			/* adjust eint digital/analog setting */
			adjust_eint_setting(accdet->eint_id);
		}
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

		if (accdet_dts.moisture_detect_mode != 0x5) {
			mod_timer(&micbias_timer,
				jiffies + MICBIAS_DISABLE_TIMER);
		}

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
	int ret = 0;
	struct device_node *node = NULL;
	int pwm_deb[15] = {0};
	int three_key[4] = {0};
	u32 tmp = 0;

	node = of_find_matching_node(node, accdet_of_match);
	if (!node) {
		pr_notice("Error: %s can't find compatible dts node\n",
			__func__);
		return -1;
	}

	/* moisture customized configuration */
	ret = of_property_read_u32(node, "moisture_detect_enable",
		&accdet_dts.moisture_detect_enable);
	if (ret) {
		/* no moisture detection */
		accdet_dts.moisture_detect_enable = 0x0;
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
	if (accdet_dts.moisture_detect_enable == 0x0) {
		/* eint detection mode equals to EINT 2.1 */
		accdet_dts.eint_detect_mode = 0x4;
	}
	ret = of_property_read_u32(node, "moisture_detect_mode",
		&accdet_dts.moisture_detect_mode);
	if (ret) {
		/* no moisture detection */
		accdet_dts.moisture_detect_mode = 0x4;
	}
	ret = of_property_read_u32(node, "moisture_comp_vth",
		&accdet_dts.moisture_comp_vth);
	if (ret) {
		/* no moisture detection */
		accdet_dts.moisture_comp_vth = 0x0;
	}
	ret = of_property_read_u32(node, "moisture_comp_vref2",
		&accdet_dts.moisture_comp_vref2);
	if (ret) {
		/* no moisture detection */
		accdet_dts.moisture_comp_vref2 = 0x0;
	}
	ret = of_property_read_u32(node, "moisture_use_ext_res",
		&accdet_dts.moisture_use_ext_res);
	if (ret) {
		/* no moisture detection */
		accdet_dts.moisture_use_ext_res = -1;
	}

	/* if moisture detection enable, eint_detect_mode must equal to
	 * moisture_detect_mode
	 */
	if ((accdet_dts.moisture_detect_enable == 0x1) &&
		(accdet_dts.moisture_detect_mode !=
		accdet_dts.eint_detect_mode)) {
		pr_notice("DTS setting error, eint mode != moisture mode\n\r");
		return -1;
	}

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

	ret = of_property_read_u32_array(node, "headset-mode-setting", pwm_deb,
			ARRAY_SIZE(pwm_deb));
	/* debounce8(auxadc debounce) is default, needn't get from dts */
	if (!ret)
		memcpy(&accdet_dts.pwm_deb, pwm_deb, sizeof(pwm_deb));

	cust_pwm_deb = &accdet_dts.pwm_deb;

	ret = of_property_read_u32(node, "headset-eint-level-pol",
			&accdet_dts.eint_pol);
	if (ret)
		accdet_dts.eint_pol = 8;

	pr_info("accdet mic_vol=%d, plugout_deb=%d mic_mode=%d eint_pol=%d\n",
	     accdet_dts.mic_vol, accdet_dts.plugout_deb,
	     accdet_dts.mic_mode, accdet_dts.eint_pol);

	ret = of_property_read_u32(node,
			"headset-use-ap-eint", &tmp);
	if (ret)
		tmp = 0;
	if (tmp == 0)
		accdet->data->caps |= ACCDET_PMIC_EINT_IRQ;
	else if (tmp == 1)
		accdet->data->caps |= ACCDET_AP_GPIO_EINT;

	ret = of_property_read_u32(node,
			"headset-eint-num", &tmp);
	if (ret)
		tmp = 0;
	if (tmp == 0)
		accdet->data->caps |= ACCDET_PMIC_EINT0;
	else if (tmp == 1)
		accdet->data->caps |= ACCDET_PMIC_EINT1;
	else if (tmp == 2)
		accdet->data->caps |= ACCDET_PMIC_BI_EINT;

	ret = of_property_read_u32(node,
			"headset-eint-trig-mode", &tmp);
	if (ret)
		tmp = 0;
	if (tmp == 0)
		accdet->data->caps |= ACCDET_PMIC_GPIO_TRIG_EINT;
	else if (tmp == 1)
		accdet->data->caps |= ACCDET_PMIC_INVERTER_TRIG_EINT;

	ret = of_property_read_u32(node,
			"headset-key-mode", &tmp);
	if (ret)
		tmp = 0;
	if (tmp == 0)
		accdet->data->caps |= ACCDET_THREE_KEY;
	else if (tmp == 1)
		accdet->data->caps |= ACCDET_FOUR_KEY;
	else if (tmp == 2)
		accdet->data->caps |= ACCDET_TRI_KEY_CDD;

	pr_info("accdet caps=%x\n", accdet->data->caps);
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
	dis_micbias_done = false;

	ret = of_property_read_u32(node, "moisture-water-r", &accdet->water_r);
	if (ret) {
		/* no moisture detection */
		accdet->water_r = 0x0;
	}
	ret = of_property_read_u32(node, "moisture_use_ext_res",
		&accdet_dts.moisture_use_ext_res);
	if (ret) {
		/* no moisture detection */
		accdet_dts.moisture_use_ext_res = -1;
	}
	if (accdet_dts.moisture_use_ext_res == 0x1) {
		of_property_read_u32(node, "moisture-external-r",
			&accdet->moisture_ext_r);
		pr_info("Moisture_EXT support water_r=%d, ext_r=%d\n",
		     accdet->water_r, accdet->moisture_ext_r);
	} else if (accdet_dts.moisture_use_ext_res == 0x0) {
		of_property_read_u32(node, "moisture-internal-r",
			&accdet->moisture_int_r);
		pr_info("Moisture_INT support water_r=%d, int_r=%d\n",
		     accdet->water_r, accdet->moisture_int_r);
	}
	ret = of_property_read_u32(node, "eint_use_ext_res",
		&accdet_dts.eint_use_ext_res);
	if (ret) {
		/* eint use internal resister */
		accdet_dts.eint_use_ext_res = 0x0;
	}
	return 0;
}

static void config_digital_init_by_mode(void)
{
	/* enable eint cmpmem pwm */
	accdet_write(ACCDET_EINT_CMPMEN_PWM_THRESH_ADDR,
		(accdet_dts.pwm_deb.eint_pwm_width << 4 |
		accdet_dts.pwm_deb.eint_pwm_thresh));
	/* DA signal stable */
	if (HAS_CAP(accdet->data->caps,
			ACCDET_PMIC_EINT0)) {
		accdet_write(ACCDET_DA_STABLE_ADDR,
				ACCDET_EINT0_STABLE_VAL);
	} else if (HAS_CAP(accdet->data->caps,
			ACCDET_PMIC_EINT1)) {
		accdet_write(ACCDET_DA_STABLE_ADDR,
				ACCDET_EINT1_STABLE_VAL);
	} else if (HAS_CAP(accdet->data->caps,
			ACCDET_PMIC_BI_EINT)) {
		accdet_write(ACCDET_DA_STABLE_ADDR,
				ACCDET_EINT0_STABLE_VAL);
		accdet_write(ACCDET_DA_STABLE_ADDR,
				ACCDET_EINT1_STABLE_VAL);
	}
	if (accdet_dts.moisture_detect_mode == 0x5) {
		if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT0)) {
			/* clr DA stable signal */
			accdet_clear_bit(ACCDET_DA_STABLE_ADDR,
				ACCDET_EINT0_CEN_STABLE_SFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT1)) {
			/* clr DA stable signal */
			accdet_clear_bit(ACCDET_DA_STABLE_ADDR,
				ACCDET_EINT1_CEN_STABLE_SFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_BI_EINT)) {
			/* clr DA stable signal */
			accdet_clear_bit(ACCDET_DA_STABLE_ADDR,
				ACCDET_EINT0_CEN_STABLE_SFT);
			/* clr DA stable signal */
			accdet_clear_bit(ACCDET_DA_STABLE_ADDR,
				ACCDET_EINT1_CEN_STABLE_SFT);
		}
	}
	/* after receive n+1 number, interrupt issued. now is 2 times */
	accdet_update_bit(ACCDET_EINT_M_PLUG_IN_NUM_ADDR,
	ACCDET_EINT_M_PLUG_IN_NUM_SFT);
	/* setting HW mode, enable digital fast discharge
	 * if use EINT0 & EINT1 detection, please modify
	 * ACCDET_HWMODE_EN_ADDR[2:1]
	 */
	if (accdet_dts.moisture_detect_mode == 0x1) {
		/* disable moisture detection function */
		accdet_clear_bit(ACCDET_EINT_M_DETECT_EN_ADDR,
			ACCDET_EINT_M_DETECT_EN_SFT);
		/* wk1, disable hwmode */
		accdet_write(ACCDET_HWMODE_EN_ADDR, 0x0);
		/* enable inverter detection */
		if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT0)) {
			accdet_update_bit(ACCDET_EINT0_SW_EN_ADDR,
				ACCDET_EINT0_SW_EN_SFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT1)) {
			accdet_update_bit(ACCDET_EINT1_SW_EN_ADDR,
				ACCDET_EINT1_SW_EN_SFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_BI_EINT)) {
			accdet_update_bit(ACCDET_EINT0_SW_EN_ADDR,
				ACCDET_EINT0_SW_EN_SFT);
			accdet_update_bit(ACCDET_EINT1_SW_EN_ADDR,
				ACCDET_EINT1_SW_EN_SFT);
		}
	} else if (accdet_dts.moisture_detect_mode == 0x2) {
		/* disable moisture detection function */
		accdet_clear_bit(ACCDET_EINT_M_DETECT_EN_ADDR,
			ACCDET_EINT_M_DETECT_EN_SFT);
		/* wk1, disable hwmode */
		accdet_write(ACCDET_HWMODE_EN_ADDR, 0x100);
	} else if (accdet_dts.moisture_detect_mode == 0x3) {
		/* disable moisture detection function */
		accdet_clear_bit(ACCDET_EINT_M_DETECT_EN_ADDR,
			ACCDET_EINT_M_DETECT_EN_SFT);
		accdet_update_bit(ACCDET_EINT_CMPMOUT_SEL_ADDR,
			ACCDET_EINT_CMPMOUT_SEL_SFT);
		accdet_update_bit(ACCDET_EINT_CMPMEN_SEL_ADDR,
			ACCDET_EINT_CMPMEN_SEL_SFT);
		/* wk1, disable hwmode */
		accdet_write(ACCDET_HWMODE_EN_ADDR, 0x100);
	} else if (accdet_dts.moisture_detect_mode == 0x4) {
		/* enable moisture detection function */
		accdet_update_bit(ACCDET_EINT_M_DETECT_EN_ADDR,
			ACCDET_EINT_M_DETECT_EN_SFT);
		/* wk1, disable hwmode */
		accdet_write(ACCDET_HWMODE_EN_ADDR, 0x100);
	} else if (accdet_dts.moisture_detect_mode == 0x5) {
		/* blocking CTURBO */
		accdet_update_bit(ACCDET_EINT_CTURBO_SEL_ADDR,
			ACCDET_EINT_CTURBO_SEL_SFT);
		/* enable moisture detection function */
		accdet_update_bit(ACCDET_EINT_M_DETECT_EN_ADDR,
			ACCDET_EINT_M_DETECT_EN_SFT);

		/* wk1, disable hwmode */
		accdet_write(ACCDET_HWMODE_EN_ADDR, 0x500);
		accdet_update_bit(RG_HPLOUTPUTSTBENH_VAUDP32_ADDR,
			RG_HPLOUTPUTSTBENH_VAUDP32_SFT);
		accdet_update_bit(RG_HPROUTPUTSTBENH_VAUDP32_ADDR,
			RG_HPROUTPUTSTBENH_VAUDP32_SFT);
	} else {
		/* wk1, disable hwmode */
		accdet_write(ACCDET_HWMODE_EN_ADDR, 0x100);
	}

	if (accdet_dts.moisture_detect_enable == 0) {
		pr_info("%s() disable digital moisture.\n", __func__);
		/* disable moisture detection function */
		accdet_clear_bit(ACCDET_EINT_M_DETECT_EN_ADDR,
			ACCDET_EINT_M_DETECT_EN_SFT);
	}

	/* enable PWM */
	accdet_write(ACCDET_CMP_PWM_EN_ADDR, 0x67);
	/* enable inverter detection */
	if (accdet_dts.eint_detect_mode == 0x1) {
		/* disable inverter detection */
		if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT0)) {
			accdet_clear_bit(ACCDET_EINT0_INVERTER_SW_EN_ADDR,
				ACCDET_EINT0_INVERTER_SW_EN_SFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT1)) {
			accdet_clear_bit(ACCDET_EINT1_INVERTER_SW_EN_ADDR,
				ACCDET_EINT1_INVERTER_SW_EN_SFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_BI_EINT)) {
			accdet_clear_bit(ACCDET_EINT0_INVERTER_SW_EN_ADDR,
				ACCDET_EINT0_INVERTER_SW_EN_SFT);
			accdet_clear_bit(ACCDET_EINT1_INVERTER_SW_EN_ADDR,
				ACCDET_EINT1_INVERTER_SW_EN_SFT);
		}
	} else {
		if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT0)) {
			accdet_update_bit(ACCDET_EINT0_INVERTER_SW_EN_ADDR,
				ACCDET_EINT0_INVERTER_SW_EN_SFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_EINT1)) {
			accdet_update_bit(ACCDET_EINT1_INVERTER_SW_EN_ADDR,
				ACCDET_EINT1_INVERTER_SW_EN_SFT);
		} else if (HAS_CAP(accdet->data->caps,
				ACCDET_PMIC_BI_EINT)) {
			accdet_update_bit(ACCDET_EINT0_INVERTER_SW_EN_ADDR,
				ACCDET_EINT0_INVERTER_SW_EN_SFT);
			accdet_update_bit(ACCDET_EINT1_INVERTER_SW_EN_ADDR,
				ACCDET_EINT1_INVERTER_SW_EN_SFT);

		}
	}
}

static void config_analog_moisture_init_by_mode(void)
{
	if (accdet_dts.moisture_detect_mode == 0x1) {
		config_moisture_detect_1_0();
	} else if ((accdet_dts.moisture_detect_mode == 0x2) ||
		(accdet_dts.moisture_detect_mode == 0x3)) {
		config_moisture_detect_1_1();
	} else if (accdet_dts.moisture_detect_mode == 0x4) {
		config_moisture_detect_2_1();
	} else if (accdet_dts.moisture_detect_mode == 0x5) {
		config_moisture_detect_2_1_1();
	}
}

static void config_eint_init_by_mode(void)
{
	if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT0)) {
		accdet_update_bit(RG_EINT0EN_ADDR,
				RG_EINT0EN_SFT);
	} else if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT1)) {
		accdet_update_bit(RG_EINT1EN_ADDR,
				RG_EINT1EN_SFT);
	} else if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_BI_EINT)) {
		accdet_update_bit(RG_EINT0EN_ADDR,
				RG_EINT0EN_SFT);
		accdet_update_bit(RG_EINT1EN_ADDR,
				RG_EINT1EN_SFT);
	}
	/* ESD switches on */
	accdet_update_bit(RG_ACCDETSPARE_ADDR, 8);
	/* before playback, set NCP pull low before nagative voltage */
	accdet_update_bit(RG_NCP_PDDIS_EN_ADDR,
			RG_NCP_PDDIS_EN_SFT);

	if ((accdet_dts.eint_detect_mode == 0x1) ||
		(accdet_dts.eint_detect_mode == 0x2) ||
		(accdet_dts.eint_detect_mode == 0x3)) {

		if (accdet_dts.eint_use_ext_res == 0x1) {
			/* select VTH to 2v and 500k, use external resitance */
			if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT0)) {
				/* disable RG_EINT0CONFIGACCDET */
				accdet_clear_bit(RG_EINT0CONFIGACCDET_ADDR,
						RG_EINT0CONFIGACCDET_SFT);
			} else if (HAS_CAP(accdet->data->caps,
					ACCDET_PMIC_EINT1)) {
				/* disable RG_EINT1CONFIGACCDET */
				accdet_clear_bit(RG_EINT1CONFIGACCDET_ADDR,
						RG_EINT1CONFIGACCDET_SFT);
			} else if (HAS_CAP(accdet->data->caps,
					ACCDET_PMIC_BI_EINT)) {
				/* disable RG_EINT0CONFIGACCDET */
				accdet_clear_bit(RG_EINT0CONFIGACCDET_ADDR,
						RG_EINT0CONFIGACCDET_SFT);
				/* disable RG_EINT1CONFIGACCDET */
				accdet_clear_bit(RG_EINT1CONFIGACCDET_ADDR,
						RG_EINT1CONFIGACCDET_SFT);
			}
		} else {
			/* select VTH to 2v and 500k, use internal resitance */
			if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT0)) {
				/* enable RG_EINT0CONFIGACCDET */
				accdet_update_bit(RG_EINT0CONFIGACCDET_ADDR,
						RG_EINT0CONFIGACCDET_SFT);
			} else if (HAS_CAP(accdet->data->caps,
					ACCDET_PMIC_EINT1)) {
				/* enable RG_EINT1CONFIGACCDET */
				accdet_update_bit(RG_EINT1CONFIGACCDET_ADDR,
						RG_EINT1CONFIGACCDET_SFT);
			} else if (HAS_CAP(accdet->data->caps,
					ACCDET_PMIC_BI_EINT)) {
				/* enable RG_EINT0CONFIGACCDET */
				accdet_update_bit(RG_EINT0CONFIGACCDET_ADDR,
						RG_EINT0CONFIGACCDET_SFT);
				/* enable RG_EINT1CONFIGACCDET */
				accdet_update_bit(RG_EINT1CONFIGACCDET_ADDR,
						RG_EINT1CONFIGACCDET_SFT);
			}
		}
	} else if (accdet_dts.eint_detect_mode == 0x4) {
		/* do nothing */
	} else if (accdet_dts.eint_detect_mode == 0x5) {
		/* do nothing */
	}

	if (accdet_dts.eint_detect_mode != 0x1) {
		/* current detect set 0.25uA */
		accdet_update_bits(RG_ACCDETSPARE_ADDR,
			RG_ACCDETSPARE_SFT,
			0x3, 0x3);
	}
	/* new customized parameter */
	if ((accdet_dts.eint_use_ext_res == 0x2) ||
		(accdet_dts.eint_use_ext_res == 0x4) ||
		(accdet_dts.eint_use_ext_res == 0x5)) {
		/* select VTH to 2v */
		accdet_update_bits(RG_EINTCOMPVTH_ADDR,
			RG_EINTCOMPVTH_SFT, RG_EINTCOMPVTH_MASK,
			0x2);
	}
}

static void accdet_init_once(void)
{
	unsigned int reg = 0;

	/* reset the accdet unit */
	accdet_update_bit(RG_ACCDET_RST_ADDR,
			RG_ACCDET_RST_SFT);
	accdet_clear_bit(RG_ACCDET_RST_ADDR,
			RG_ACCDET_RST_SFT);

	/* clear high micbias1 voltage setting */
	accdet_clear_bits(RG_AUDPWDBMICBIAS1_ADDR,
		RG_AUDMICBIAS1HVEN_SFT, 0x3, 0x3);
	/* clear micbias1 voltage */
	accdet_clear_bits(RG_AUDPWDBMICBIAS1_ADDR,
		RG_AUDMICBIAS1VREF_SFT, 0x7, 0x7);

	/* init pwm frequency, duty & rise/falling delay */
	accdet_write(ACCDET_PWM_WIDTH_ADDR,
		REGISTER_VAL(cust_pwm_deb->pwm_width));
	accdet_write(ACCDET_PWM_THRESH_ADDR,
		REGISTER_VAL(cust_pwm_deb->pwm_thresh));
	accdet_write(ACCDET_RISE_DELAY_ADDR,
		  (cust_pwm_deb->fall_delay << 15 | cust_pwm_deb->rise_delay));

	/* config micbias voltage, micbias1 vref is only controlled by accdet
	 * if we need 2.8V, config [12:13]
	 */
	reg = accdet_read(RG_AUDPWDBMICBIAS1_ADDR);
	if (accdet_dts.mic_vol <= 7) {
		/* micbias1 <= 2.7V */
		accdet_write(RG_AUDPWDBMICBIAS1_ADDR,
		reg | (accdet_dts.mic_vol<<RG_AUDMICBIAS1VREF_SFT) |
		RG_AUDMICBIAS1LOWPEN_MASK_SFT);
	} else if (accdet_dts.mic_vol == 8) {
		/* micbias1 = 2.8v */
		accdet_write(RG_AUDPWDBMICBIAS1_ADDR,
			reg | (3<<RG_AUDMICBIAS1HVEN_SFT) |
			RG_AUDMICBIAS1LOWPEN_MASK_SFT);
	} else if (accdet_dts.mic_vol == 9) {
		/* micbias1 = 2.85v */
		accdet_write(RG_AUDPWDBMICBIAS1_ADDR,
			reg | (1<<RG_AUDMICBIAS1HVEN_SFT) |
			RG_AUDMICBIAS1LOWPEN_MASK_SFT);
	}
	/* mic mode setting */
	reg = accdet_read(RG_AUDACCDETMICBIAS0PULLLOW_ADDR);
	if (accdet_dts.mic_mode == HEADSET_MODE_1) {
		/* ACC mode*/
		accdet_write(RG_AUDACCDETMICBIAS0PULLLOW_ADDR,
			reg | RG_ACCDET_MODE_ANA11_MODE1);
		/* enable analog fast discharge */
		accdet_update_bit(RG_ANALOGFDEN_ADDR,
			RG_ANALOGFDEN_SFT);
		accdet_update_bits(RG_ACCDETSPARE_ADDR, 11, 0x3, 0x3);
	} else if (accdet_dts.mic_mode == HEADSET_MODE_2) {
		/* DCC mode Low cost mode without internal bias*/
		accdet_write(RG_AUDACCDETMICBIAS0PULLLOW_ADDR,
			reg | RG_ACCDET_MODE_ANA11_MODE2);
		/* enable analog fast discharge */
		accdet_update_bits(RG_ANALOGFDEN_ADDR,
			RG_ANALOGFDEN_SFT, 0x3, 0x3);
	} else if (accdet_dts.mic_mode == HEADSET_MODE_6) {
		/* DCC mode Low cost mode with internal bias,
		 * bit8 = 1 to use internal bias
		 */
		accdet_write(RG_AUDACCDETMICBIAS0PULLLOW_ADDR,
			reg | RG_ACCDET_MODE_ANA11_MODE6);
		accdet_update_bit(RG_AUDPWDBMICBIAS1_ADDR,
				RG_AUDMICBIAS1DCSW1PEN_SFT);
		/* enable analog fast discharge */
		accdet_update_bits(RG_ANALOGFDEN_ADDR,
			RG_ANALOGFDEN_SFT, 0x3, 0x3);
	}

	if (HAS_CAP(accdet->data->caps, ACCDET_PMIC_EINT_IRQ)) {
		config_eint_init_by_mode();
		if (accdet_dts.moisture_detect_enable == 1) {
			pr_info("%s() set analog moisture.\n", __func__);
			config_analog_moisture_init_by_mode();
		}
		config_digital_init_by_mode();
	} else if (HAS_CAP(accdet->data->caps, ACCDET_AP_GPIO_EINT)) {
		/* set pull low pads and DCC mode */
		accdet_write(RG_AUDACCDETMICBIAS0PULLLOW_ADDR,
				0x8F);
		/* disconnect configaccdet */
		accdet_write(RG_EINT1CONFIGACCDET_ADDR,
				0x0);
		/* disable eint comparator */
		accdet_write(RG_EINT0CMPEN_ADDR, 0x0);
		/* enable PWM */
		accdet_write(ACCDET_CMP_PWM_EN_ADDR, 0x7);
		/* enable accdet sw mode */
		accdet_write(ACCDET_HWMODE_EN_ADDR, 0x0);
		/* set DA signal to stable */
		accdet_write(ACCDET_DA_STABLE_ADDR, 0x1);
		/* disable eint/inverter/sw_en */
		accdet_write(ACCDET_SW_EN_ADDR, 0x0);
	}
	pr_info("%s done!\n", __func__);
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
	accdet_update_bit(ACCDET_SEQ_INIT_ADDR,
			ACCDET_SEQ_INIT_SFT);
	usleep_range(2000, 3000);
	accdet_clear_bit(ACCDET_SEQ_INIT_ADDR,
			ACCDET_SEQ_INIT_SFT);
	usleep_range(1000, 1500);
	/* init the debounce time (debounce/32768)sec */
	accdet_set_debounce(accdet_state000, cust_pwm_deb->debounce0);
	accdet_set_debounce(accdet_state001, cust_pwm_deb->debounce1);
	accdet_set_debounce(accdet_state011, cust_pwm_deb->debounce3);
	/* auxadc:2ms */
	accdet_set_debounce(accdet_auxadc, cust_pwm_deb->debounce4);
	if (accdet_dts.moisture_detect_enable == 0x1) {
		/* eint_state001 can be configured, less than 2ms */
		accdet_set_debounce(eint_state001,
			accdet_dts.pwm_deb.eint_debounce1);
		accdet_set_debounce(eint_state010,
			accdet_dts.pwm_deb.eint_debounce2);
	} else {
		accdet_set_debounce(eint_state001,
			accdet_dts.pwm_deb.eint_debounce1);
	}
	accdet_set_debounce(eint_inverter_state000,
		accdet_dts.pwm_deb.eint_inverter_debounce);

	pr_info("%s() done.\n", __func__);
}

/* late init for DC trim, and this API  Will be called by audio */
void accdet_late_init(unsigned long data)
{
	pr_info("%s()  now init accdet!\n", __func__);
	if (atomic_cmpxchg(&accdet_first, 1, 0)) {
		del_timer_sync(&accdet_init_timer);
		accdet_init();
		accdet_init_debounce();
		accdet_init_once();
	} else
		pr_info("%s inited dts fail\n", __func__);
}
EXPORT_SYMBOL(accdet_late_init);

void accdet_modify_vref_volt(void)
{
}

static void accdet_modify_vref_volt_self(void)
{
	u32 cur_AB, eintID;

	if (accdet_dts.moisture_detect_mode == 0x5) {
		/* make sure seq is disable micbias then connect vref2 */

		/* check EINT0 status, if plug out,
		 * not need to disable accdet here
		 */
		eintID = accdet_read_bits(ACCDET_EINT0_MEM_IN_ADDR,
			ACCDET_EINT0_MEM_IN_SFT,
			ACCDET_EINT0_MEM_IN_MASK);
		if (eintID == M_PLUG_OUT) {
			pr_info("%s Plug-out, no dis micbias\n", __func__);
			return;
		}
		cur_AB = accdet_read(ACCDET_MEM_IN_ADDR) >>
				ACCDET_STATE_MEM_IN_OFFSET;
		cur_AB = cur_AB & ACCDET_STATE_AB_MASK;

		/* if 3pole disable accdet
		 * if <20k + 4pole, disable accdet will disable accdet
		 * plug out interrupt. The behavior will same as 3pole
		 */
		if (accdet->cable_type == HEADSET_MIC) {
			/* do nothing */
		} else if ((accdet->cable_type == HEADSET_NO_MIC) ||
			(cur_AB == ACCDET_STATE_AB_00) ||
			(cur_AB == ACCDET_STATE_AB_11)) {
			/* disable accdet_sw_en=0
			 * disable accdet_hwmode_en=0
			 */
			accdet_clear_bit(ACCDET_SW_EN_ADDR,
				ACCDET_SW_EN_SFT);
			disable_accdet();
			pr_info("%s MICBIAS:Disabled AB:0x%x c_type:0x%x\n",
				__func__, cur_AB, accdet->cable_type);
			dis_micbias_done = true;
		}
		/* disable comp1 delay window */
		accdet_update_bit(RG_EINT0NOHYS_ADDR,
			RG_EINT0NOHYS_SFT);
		/* connect VREF2 to EINT0CMP */
		accdet_update_bits(RG_EINTCOMPVTH_ADDR,
			RG_EINTCOMPVTH_SFT, 0x3, 0x3);
		pr_info("%s [0x%x]=0x%x [0x%x]=0x%x\n", __func__,
			RG_EINT0NOHYS_ADDR,
			accdet_read(RG_EINT0NOHYS_ADDR),
			RG_EINTCOMPVTH_ADDR,
			accdet_read(RG_EINTCOMPVTH_ADDR));
	}
}

static void delay_init_work_callback(struct work_struct *work)
{
	accdet_init();
	accdet_init_debounce();
	accdet_init_once();
	pr_info("%s() done\n", __func__);
}

static void delay_init_timerhandler(struct timer_list *t)
{
	int ret = 0;

	ret = queue_work(accdet->delay_init_workqueue,
			&accdet->delay_init_work);
	if (!ret)
		pr_notice("Error: %s (%d)\n", __func__, ret);
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

	accdet->base = 0;
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
	accdet->wake_lock = wakeup_source_register(NULL, "accdet_wake_lock");
	if (!accdet->wake_lock)
		return -ENOMEM;
	accdet->timer_lock = wakeup_source_register(NULL, "accdet_timer_lock");
	if (!accdet->timer_lock)
		return -ENOMEM;
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
	timer_setup(&accdet_init_timer, delay_init_timerhandler, 0);
	accdet_init_timer.expires = jiffies + ACCDET_INIT_WAIT_TIMER;

	/* Create workqueue */
	accdet->delay_init_workqueue =
		create_singlethread_workqueue("delay_init");
	INIT_WORK(&accdet->delay_init_work, delay_init_work_callback);
	if (!accdet->delay_init_workqueue) {
		dev_dbg(&pdev->dev, "Error: Create dinit workqueue failed\n");
		ret = -1;
		goto err_device_create;
	}
	accdet->accdet_workqueue = create_singlethread_workqueue("accdet");
	INIT_WORK(&accdet->accdet_work, accdet_work_callback);
	if (!accdet->accdet_workqueue) {
		dev_dbg(&pdev->dev, "Error: Create accdet workqueue failed\n");
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

	ret = accdet_create_attr(&accdet_driver.driver);
	if (ret) {
		pr_notice("%s create_attr fail, ret = %d\n", __func__, ret);
		goto err_create_workqueue;
	}
	atomic_set(&accdet_first, 1);
	mod_timer(&accdet_init_timer, (jiffies + ACCDET_INIT_WAIT_TIMER));

	return 0;

err_create_workqueue:
	destroy_workqueue(accdet->dis_micbias_workqueue);
err:
	destroy_workqueue(accdet->accdet_workqueue);
	destroy_workqueue(accdet->delay_init_workqueue);
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
	destroy_workqueue(accdet->delay_init_workqueue);
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
