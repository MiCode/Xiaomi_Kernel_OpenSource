/*
 * Copyright (C) 2016 MediaTek Inc.
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
#ifdef CONFIG_ACCDET_EINT
#include <linux/of_gpio.h>
#endif
#include <upmu_common.h>
#include <linux/timer.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <mtk_auxadc_intf.h>
#include <mach/mtk_pmic.h>
#include <linux/irq.h>
#include "reg_accdet.h"
#include <mach/upmu_hw.h>

#define REGISTER_VAL(x)	(x - 1)

/* for accdet_read_audio_res, less than 5k ohm, return -1 , otherwise ret 0 */
#define RET_LT_5K		(-1)
#define RET_GT_5K		(0)

/* Used to let accdet know if the pin has been fully plugged-in */
#define EINT_PIN_PLUG_IN        (1)
#define EINT_PIN_PLUG_OUT       (0)

/* accdet_status_str: to record current 'accdet_status' by string,
 *mapping to  'enum accdet_status'
 */
static char *accdet_status_str[] = {
	"Plug_out",
	"Headset_plug_in",
	"Hook_switch",
	"Line_out",
	"Stand_by"
};

/* accdet_report_str: to record current 'cable_type' by string,
 * mapping to  'enum accdet_report_state'
 */
static char *accdet_report_str[] = {
	"No_device",
	"Headset_mic",
	"Headset_no_mic",
	"Headset_five_pole",
	"Line_out_device"
};

/* accdet char device & class & device */
static dev_t accdet_devno;
static struct cdev *accdet_cdev;
static struct class *accdet_class;
static struct device *accdet_device;

/* accdet input device to report cable type and key event */
static struct input_dev *accdet_input_dev;

/* when  MICBIAS_DISABLE_TIMER timeout, queue work: dis_micbias_work */
static struct work_struct dis_micbias_work;
static struct workqueue_struct *dis_micbias_workqueue;
/* when  accdet irq issued, queue work: accdet_work work */
static struct work_struct accdet_work;
static struct workqueue_struct *accdet_workqueue;
/* when  eint issued, queue work: eint_work */
static struct work_struct eint_work;
static struct workqueue_struct *eint_workqueue;

/* micbias_timer: disable micbias if no accdet irq after eint,
 * timeout: 6 seconds
 * timerHandler: dis_micbias_timerhandler()
 */
#define MICBIAS_DISABLE_TIMER   (6 * HZ)
static struct timer_list micbias_timer;
static void dis_micbias_timerhandler(unsigned long data);

/* accdet_init_timer:  init accdet if audio doesn't call to accdet for DC trim
 * timeout: 10 seconds
 * timerHandler: delay_init_timerhandler()
 */
#define ACCDET_INIT_WAIT_TIMER   (10 * HZ)
static struct timer_list  accdet_init_timer;
static void delay_init_timerhandler(unsigned long data);

/* accdet customized info by dts*/
static struct head_dts_data accdet_dts;
static struct pwm_deb_settings *cust_pwm_deb;

#ifdef CONFIG_ACCDET_EINT
static struct pinctrl *accdet_pinctrl;
static struct pinctrl_state *pins_eint;
static u32 gpiopin, gpio_headset_deb;
static u32 accdet_irq;
#endif

/* accdet FSM State & lock*/
static bool eint_accdet_sync_flag;
static u32 cur_eint_state = EINT_PIN_PLUG_OUT;
static u32 pre_status;
static u32 accdet_status = PLUG_OUT;
static u32 cable_type;
static u32 cur_key;
static u32 cali_voltage;
static int accdet_auxadc_offset;
static struct wakeup_source *accdet_irq_lock;
static struct wakeup_source *accdet_timer_lock;
static DEFINE_MUTEX(accdet_eint_irq_sync_mutex);
static int s_button_status;

static u32 accdet_eint_type = IRQ_TYPE_LEVEL_LOW;
static u32 button_press_debounce = 0x400;
static atomic_t accdet_first;

static bool debug_thread_en;
static bool dump_reg;
static struct task_struct *thread;

static inline void accdet_init(void);
static void send_accdet_status_event(u32 cable_type, u32 status);

static inline u32 pmic_read(u32 addr)
{
	u32 val = 0;

	pwrap_read(addr, &val);
	return val;
}

static inline void pmic_write(u32 addr, u32 wdata)
{
	pwrap_write(addr, wdata);
}

static void dump_register(void)
{
	int i = 0;

	for (i = ACCDET_RSV; i <= ACCDET_HW_MODE_DFF; i += 2)
		pr_info(" ACCDET_BASE + 0x%x=0x%x\n", i,
			pmic_read(ACCDET_BASE + i));

	pr_info(" TOP_RST_ACCDET(0x%x) =0x%x\n", TOP_RST_ACCDET,
		pmic_read(TOP_RST_ACCDET));
	pr_info(" INT_CON_ACCDET(0x%x) =0x%x\n", INT_CON_ACCDET,
		pmic_read(INT_CON_ACCDET));
	pr_info(" TOP_CKPDN(0x%x) =0x%x\n", TOP_CKPDN,
		pmic_read(TOP_CKPDN));
	pr_info(" ACCDET_ADC_REG(0x%x) =0x%x\n", ACCDET_ADC_REG,
		pmic_read(ACCDET_ADC_REG));
}

static void cat_register(char *buf)
{
	int i = 0;
	char buf_temp[128] = { 0 };

#ifdef CONFIG_ACCDET_EINT_IRQ
	strncat(buf, "[CONFIG_ACCDET_EINT_IRQ]dump_register:\n", 64);
#else
	strncat(buf, "[CONFIG_ACCDET_EINT]dump_register:\n", 64);
#endif

	for (i = ACCDET_RSV; i <= ACCDET_HW_MODE_DFF; i += 2) {
		sprintf(buf_temp, "ACCDET_ADDR[0x%x]=0x%x\n",
			(ACCDET_BASE + i), pmic_read(ACCDET_BASE + i));
		strncat(buf, buf_temp, strlen(buf_temp));
	}

	sprintf(buf_temp, "TOP_RST_ACCDET[0x%x]=0x%x\n", TOP_RST_ACCDET,
		pmic_read(TOP_RST_ACCDET));
	strncat(buf, buf_temp, strlen(buf_temp));
	sprintf(buf_temp, "INT_CON_ACCDET[0x%x]=0x%x\n", INT_CON_ACCDET,
		pmic_read(INT_CON_ACCDET));
	strncat(buf, buf_temp, strlen(buf_temp));
	sprintf(buf_temp, "TOP_CKPDN[0x%x]=0x%x\n", TOP_CKPDN,
		pmic_read(TOP_CKPDN));
	strncat(buf, buf_temp, strlen(buf_temp));
	sprintf(buf_temp, "ACCDET_ADC_REG[0x%x]=0x%x\n", ACCDET_ADC_REG,
		pmic_read(ACCDET_ADC_REG));
	strncat(buf, buf_temp, strlen(buf_temp));
}

static int dbug_thread(void *unused)
{
	while (debug_thread_en) {
		if (dump_reg)
			dump_register();
		msleep(500);
	}
	return 0;
}

static ssize_t store_accdet_start_debug_thread(struct device_driver *ddri,
	const char *buf, size_t count)
{
	int error = 0;
	int ret = 0;

	if (strlen(buf) < 1) {
		pr_notice("%s() Invalid input!!\n", __func__);
		return -EINVAL;
	}

	ret = strncmp(buf, "0", 1);
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

static ssize_t store_accdet_set_reg(struct device_driver *ddri,
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

	if ((addr_tmp < PMIC_REG_BASE_START) || (addr_tmp > PMIC_REG_BASE_END))
		pr_notice("%s() Illegal addr[0x%x]!!\n", __func__, addr_tmp);
	else
		pmic_write(addr_tmp, value_tmp);

	return count;
}

static ssize_t show_accdet_dump_reg(struct device_driver *ddri, char *buf)
{
	if (buf == NULL) {
		pr_notice("%s() *buf is NULL\n", __func__);
		return -EINVAL;
	}

	cat_register(buf);
	pr_info("%s() buf_size:%d\n", __func__, (int)strlen(buf));

	return strlen(buf);
}

static ssize_t store_accdet_dump_reg(struct device_driver *ddri,
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

static ssize_t store_accdet_set_headset_mode(struct device_driver *ddri,
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
		break;
	case HEADSET_MODE_6:
		accdet_dts.mic_mode = tmp_headset_mode;
		accdet_init();
		break;
	default:
		pr_info("%s() Invalid mode: %d\n", __func__, tmp_headset_mode);
		break;
	}

	return count;
}

static ssize_t show_cable_state(struct device_driver *ddri, char *buf)
{
	char temp_type = (char)cable_type;

	if (buf == NULL) {
		pr_notice("[%s] *buf is NULL!\n",  __func__);
		return -EINVAL;
	}
	snprintf(buf, 3, "%d\n", temp_type);

	return strlen(buf);
}

static DRIVER_ATTR(start_debug, 0644, NULL, store_accdet_start_debug_thread);
static DRIVER_ATTR(set_reg, 0644, NULL, store_accdet_set_reg);
static DRIVER_ATTR(dump_reg, 0644, show_accdet_dump_reg,
			store_accdet_dump_reg);
static DRIVER_ATTR(set_headset_mode, 0644, NULL,
			store_accdet_set_headset_mode);
static DRIVER_ATTR(state, 0644, show_cable_state, NULL);

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

	mutex_lock(&accdet_eint_irq_sync_mutex);
	if (eint_accdet_sync_flag) {
		cable_type = LINE_OUT_DEVICE;
		accdet_status = LINE_OUT;
		send_accdet_status_event(cable_type, 1);
		pr_info("%s() update state:%d\n", __func__, cable_type);
	}
	mutex_unlock(&accdet_eint_irq_sync_mutex);

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
		pr_notice("%s Timer overflow! start%lld cur timer%lld\n",
			__func__, start_time_ns, cur_time);
		start_time_ns = cur_time;
		/* 400us */
		timeout_time_ns = 400 * 1000;
		pr_notice("%s Reset timer! start%lld setting%lld\n",
			__func__, start_time_ns, timeout_time_ns);
	}
	elapse_time = cur_time - start_time_ns;

	/* check if timeout */
	if (timeout_time_ns <= elapse_time) {
		pr_notice("%s IRQ clear Timeout\n", __func__);
		return false;
	}
	return true;
}

static u32 accdet_get_auxadc(int deCount)
{
	int vol = pmic_get_auxadc_value(AUXADC_LIST_ACCDET);

	pr_info("%s() vol_val:%d offset:%d real vol:%d mv!\n", __func__, vol,
		accdet_auxadc_offset,
		(vol < accdet_auxadc_offset) ? 0 : (vol-accdet_auxadc_offset));

	if (vol < accdet_auxadc_offset)
		vol = 0;
	else
		vol -= accdet_auxadc_offset;

	return vol;
}

static void accdet_get_efuse(void)
{
	u32 efuseval;
	u32 tmp_val;

#ifdef CONFIG_FOUR_KEY_HEADSET
	/* Just for 4-key, 2.7V, internal bias for mt6337*/
	/* [Notice] must confirm the bias vol is 2.7v */
	/* get 8bit from efuse rigister, so extend to 12bit, right shift 2b */
	/* AD */
	tmp_val = (int)pmic_read(REG_ACCDET_AD_CALI_1);
	efuseval = (tmp_val>>RG_ACCDET_BIT_SHIFT)&ACCDET_CALI_MASK2;
	tmp_val = (int)pmic_read(REG_ACCDET_AD_CALI_2);
	efuseval = efuseval | ((tmp_val & 0x01) << RG_ACCDET_HIGH_BIT_SHIFT);
	accdet_dts.four_key.mid = efuseval;

	/* DB */
	tmp_val = (int)pmic_read(REG_ACCDET_AD_CALI_2);
	efuseval = (tmp_val>>0x01)&ACCDET_CALI_MASK3;
	accdet_dts.four_key.voice = efuseval;

	/* BC */
	tmp_val = (int)pmic_read(REG_ACCDET_AD_CALI_2);
	efuseval = (tmp_val>>RG_ACCDET_BIT_SHIFT)&ACCDET_CALI_MASK4;
	tmp_val = (int)pmic_read(REG_ACCDET_AD_CALI_3);
	efuseval = efuseval | ((tmp_val & 0x01) << RG_ACCDET_HIGH_BIT_SHIFT);
	accdet_dts.four_key.up = efuseval;
	accdet_dts.four_key.down = 1000;
	accdet_auxadc_offset = 0;
#else
/* 3-key, different from with mt6351 */
	tmp_val = (int)pmic_read(REG_ACCDET_AD_CALI_0);
	efuseval = ((tmp_val>>RG_ACCDET_BIT_SHIFT) & ACCDET_CALI_MASK0);
	tmp_val = (int)pmic_read(REG_ACCDET_AD_CALI_1);
	accdet_auxadc_offset = efuseval |
		((tmp_val & 0x01) << RG_ACCDET_HIGH_BIT_SHIFT);
	if (accdet_auxadc_offset > 128)
		accdet_auxadc_offset -= 256;
	accdet_auxadc_offset = (accdet_auxadc_offset / 2);
	pr_info("%s efuse=0x%x,auxadc_val=%dmv\n", __func__, efuseval,
		accdet_auxadc_offset);
#endif
}

#ifdef CONFIG_FOUR_KEY_HEADSET
static u32 key_check(u32 v)
{
	if ((v < accdet_dts.four_key.down) && (v >= accdet_dts.four_key.up))
		return DW_KEY;
	if ((v < accdet_dts.four_key.up) && (v >= accdet_dts.four_key.voice))
		return UP_KEY;
	if ((v < accdet_dts.four_key.voice) && (v >= accdet_dts.four_key.mid))
		return AS_KEY;
	if (v < accdet_dts.four_key.mid)
		return MD_KEY;

	return NO_KEY;
}
#else
static u32 key_check(u32 v)
{
	if ((v < accdet_dts.three_key.down) && (v >= accdet_dts.three_key.up))
		return DW_KEY;
	if ((v < accdet_dts.three_key.up) && (v >= accdet_dts.three_key.mid))
		return UP_KEY;
	if (v < accdet_dts.three_key.mid)
		return MD_KEY;

	return NO_KEY;
}
#endif

static void send_key_event(u32 keycode, u32 flag)
{
	switch (keycode) {
	case DW_KEY:
		input_report_key(accdet_input_dev, KEY_VOLUMEDOWN, flag);
		input_sync(accdet_input_dev);
		pr_debug("accdet KEY_VOLUMEDOWN %d\n", flag);
		break;
	case UP_KEY:
		input_report_key(accdet_input_dev, KEY_VOLUMEUP, flag);
		input_sync(accdet_input_dev);
		pr_debug("accdet KEY_VOLUMEUP %d\n", flag);
		break;
	case MD_KEY:
		input_report_key(accdet_input_dev, KEY_PLAYPAUSE, flag);
		input_sync(accdet_input_dev);
		pr_debug("accdet KEY_PLAYPAUSE %d\n", flag);
		break;
	case AS_KEY:
		input_report_key(accdet_input_dev, KEY_VOICECOMMAND, flag);
		input_sync(accdet_input_dev);
		pr_debug("accdet KEY_VOICECOMMAND %d\n", flag);
		break;
	}
}

static void send_accdet_status_event(u32 cable_type, u32 status)
{
	switch (cable_type) {
	case HEADSET_NO_MIC:
		input_report_switch(accdet_input_dev, SW_HEADPHONE_INSERT,
			status);
		/* when plug 4-pole out, if both AB=3 AB=0 happen,3-pole plug
		 * in will be incorrectly reported, then 3-pole plug-out is
		 * reported,if no mantory 4-pole plug-out, icon would be
		 * visible.
		 */
		if (status == 0)
			input_report_switch(accdet_input_dev,
				SW_MICROPHONE_INSERT, status);
		input_sync(accdet_input_dev);
		pr_info("%s HEADPHONE(3-pole) %s\n", __func__,
			status ? "PlugIn" : "PlugOut");
		break;
	case HEADSET_MIC:
		/* when plug 4-pole out, 3-pole plug out should also be
		 * reported for slow plug-in case
		 */
		if (status == 0)
			input_report_switch(accdet_input_dev,
				SW_HEADPHONE_INSERT, status);
		input_report_switch(accdet_input_dev, SW_MICROPHONE_INSERT,
			status);
		input_sync(accdet_input_dev);
		pr_info("%s MICROPHONE(4-pole) %s\n", __func__,
			status ? "PlugIn" : "PlugOut");
		break;
	case LINE_OUT_DEVICE:
		input_report_switch(accdet_input_dev, SW_LINEOUT_INSERT,
			status);
		input_sync(accdet_input_dev);
		pr_info("%s LineOut %s\n", __func__,
			status ? "PlugIn" : "PlugOut");
		break;
	default:
		pr_info("%s Invalid cableType\n", __func__);
	}
}

static void multi_key_detection(u32 cur_AB)
{
#ifdef CONFIG_ACCDET_EINT_IRQ
	bool irq_bit;
#endif

	if (cur_AB == ACCDET_STATE_AB_00)
		cur_key = key_check(cali_voltage);

	/* delay to fix side effect key when plug-out, when plug-out,seldom
	 * issued AB=0 and Eint, delay to wait eint been flaged in register.
	 * or eint handler issued. cur_eint_state == PLUG_OUT
	 */
	mdelay(10);

#ifdef CONFIG_ACCDET_EINT_IRQ
	irq_bit = !(pmic_read(ACCDET_IRQ_STS) & EINT_IRQ_STATUS_BIT);
	/* send key when: no eint is flaged in reg, and now eint PLUG_IN */
	if (irq_bit && (cur_eint_state == EINT_PIN_PLUG_IN))
#elif defined CONFIG_ACCDET_EINT
	if (cur_eint_state == EINT_PIN_PLUG_IN)
#endif
		send_key_event(cur_key, !cur_AB);
	else {
		pr_info("accdet plugout sideeffect key,do not report key=%d\n",
			cur_key);
		cur_key = NO_KEY;
	}

	if (cur_AB)
		cur_key = NO_KEY;
}

/* clear ACCDET IRQ in accdet register */
static inline void clear_accdet_int(void)
{
	/* it is safe by using polling to adjust when to clear IRQ_CLR_BIT */
	pmic_write(ACCDET_IRQ_STS,
		pmic_read(ACCDET_IRQ_STS) | IRQ_CLR_BIT);
	pr_debug("%s() IRQ_STS = 0x%x\n", __func__, pmic_read(ACCDET_IRQ_STS));
}

static inline void clear_accdet_int_check(void)
{
	u64 cur_time = accdet_get_current_time();

	while ((pmic_read(ACCDET_IRQ_STS) & IRQ_STATUS_BIT) &&
		(accdet_timeout_ns(cur_time, ACCDET_TIME_OUT)))
		;
	/* clear accdet int, modify  for fix interrupt trigger twice error */
	pmic_write(ACCDET_IRQ_STS, pmic_read(ACCDET_IRQ_STS) & (~IRQ_CLR_BIT));
	pmic_write(INT_STATUS_ACCDET, RG_INT_STATUS_ACCDET);
}

#ifdef CONFIG_ACCDET_EINT_IRQ
static inline void clear_accdet_eint(void)
{
	pmic_write(ACCDET_IRQ_STS,
		pmic_read(ACCDET_IRQ_STS) | IRQ_EINT_CLR_BIT);
	pr_info("%s()  [0x%x]=0x%x\n", __func__,
		ACCDET_IRQ_STS, pmic_read(ACCDET_IRQ_STS));
}

static inline void clear_accdet_eint_check(void)
{
	u64 cur_time = accdet_get_current_time();

	while ((pmic_read(ACCDET_IRQ_STS) & EINT_IRQ_STATUS_BIT) &&
		(accdet_timeout_ns(cur_time, ACCDET_TIME_OUT)))
		;
	pmic_write(ACCDET_IRQ_STS,
		pmic_read(ACCDET_IRQ_STS) & (~IRQ_EINT_CLR_BIT));
	pmic_write(INT_STATUS_ACCDET, RG_INT_STATUS_ACCDET_EINT);
}


static void eint_polarity_reverse(void)
{
	if (cur_eint_state == EINT_PIN_PLUG_IN) {
		if (accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
			pmic_write(ACCDET_IRQ_STS,
				pmic_read(ACCDET_IRQ_STS) | EINT_IRQ_POL_HIGH);
		else
			pmic_write(ACCDET_IRQ_STS,
				pmic_read(ACCDET_IRQ_STS) & ~EINT_IRQ_POL_LOW);
	} else {
		if (accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
			pmic_write(ACCDET_IRQ_STS,
				pmic_read(ACCDET_IRQ_STS) & ~EINT_IRQ_POL_LOW);
		else
			pmic_write(ACCDET_IRQ_STS,
				pmic_read(ACCDET_IRQ_STS) | EINT_IRQ_POL_HIGH);
	}
}
#endif

static inline void enable_accdet(u32 state_swctrl)
{
	pmic_write(TOP_CKPDN_CLR, RG_ACCDET_CLK_CLR);
	pmic_write(ACCDET_STATE_SWCTRL,
		pmic_read(ACCDET_STATE_SWCTRL) | state_swctrl);
	pmic_write(ACCDET_CTRL, pmic_read(ACCDET_CTRL) | ACCDET_ENABLE);
	pr_info("%s done IRQ-STS[0x%x]=0x%x,STATE_SWCTRL[0x%x]=0x%x\n",
		__func__, ACCDET_IRQ_STS, pmic_read(ACCDET_IRQ_STS),
		ACCDET_STATE_SWCTRL, pmic_read(ACCDET_STATE_SWCTRL));
}

static inline void disable_accdet(void)
{
	/* sync with accdet_irq_handler set clear accdet irq bit to avoid */
	/* set clear accdet irq bit after disable accdet disable accdet irq */
	clear_accdet_int();
	udelay(200);
	mutex_lock(&accdet_eint_irq_sync_mutex);
	clear_accdet_int_check();
	mutex_unlock(&accdet_eint_irq_sync_mutex);

#ifdef CONFIG_ACCDET_EINT
	/* disable clock and Analog control */
	/* mt6331_upmu_set_rg_audmicbias1vref(0x0); */
	pmic_write(TOP_CKPDN_SET, RG_ACCDET_CLK_SET);
#endif
	pmic_write(ACCDET_CTRL, pmic_read(ACCDET_CTRL) & (~ACCDET_ENABLE));

	/* clc ACCDET PWM enable to avoid power leakage */
	pmic_write(ACCDET_STATE_SWCTRL,
		pmic_read(ACCDET_STATE_SWCTRL) & (~ACCDET_PWM_EN));
	pr_debug("%s [0x%x]=0x%x\n", __func__, ACCDET_IRQ_STS,
		pmic_read(ACCDET_IRQ_STS));
	pr_info("%s done IRQ-STS[0x%x]=0x%x,STATE_SWCTRL[0x%x]=0x%x\n",
		__func__, ACCDET_IRQ_STS, pmic_read(ACCDET_IRQ_STS),
		ACCDET_STATE_SWCTRL, pmic_read(ACCDET_STATE_SWCTRL));
}

static inline void headset_plug_out(void)
{
	send_accdet_status_event(cable_type, 0);
	accdet_status = PLUG_OUT;
	cable_type = NO_DEVICE;

	if (cur_key != 0) {
		send_key_event(cur_key, 0);
		pr_info("accdet %s, send key=%d release\n", __func__, cur_key);
		cur_key = 0;
	}
	pr_info("accdet %s, set cable_type = NO_DEVICE\n", __func__);
}

static void dis_micbias_timerhandler(unsigned long data)
{
	int ret = 0;

	ret = queue_work(dis_micbias_workqueue, &dis_micbias_work);
	if (!ret)
		pr_info("accdet %s, queue work return:%d!\n", __func__, ret);
}

static void dis_micbias_work_callback(struct work_struct *work)
{
	if (cable_type == HEADSET_NO_MIC) {
		/* setting pwm idle; */
		pmic_write(ACCDET_STATE_SWCTRL,
		pmic_read(ACCDET_STATE_SWCTRL) & (~ACCDET_PWM_IDLE));

		disable_accdet();
		pr_info("accdet %s more than 6s,MICBIAS:Disabled\n", __func__);
	}
}

static void eint_work_callback(struct work_struct *work)
{
#ifdef CONFIG_ACCDET_EINT_IRQ
	pr_info("accdet %s(),DCC EINT func\n", __func__);
#else
	pr_info("accdet %s(),ACC EINT func\n", __func__);
#endif

	if (cur_eint_state == EINT_PIN_PLUG_IN) {
		pr_info("accdet cur: plug-in, cur_eint_state = %d\n",
			cur_eint_state);
		mutex_lock(&accdet_eint_irq_sync_mutex);
		eint_accdet_sync_flag = true;
		mutex_unlock(&accdet_eint_irq_sync_mutex);
		__pm_wakeup_event(accdet_timer_lock,
			jiffies_to_msecs(7 * HZ));

		accdet_init();

		/* set PWM IDLE  on */
		pmic_write(ACCDET_STATE_SWCTRL,
		(pmic_read(ACCDET_STATE_SWCTRL) | ACCDET_PWM_IDLE));
		/* enable ACCDET unit */
		enable_accdet(ACCDET_PWM_EN);
	} else {
		pr_info("accdet cur:plug-out, cur_eint_state = %d\n",
			cur_eint_state);
		mutex_lock(&accdet_eint_irq_sync_mutex);
		eint_accdet_sync_flag = false;
		mutex_unlock(&accdet_eint_irq_sync_mutex);
		del_timer_sync(&micbias_timer);

		/* clc Accdet PWM idle */
		pmic_write(ACCDET_STATE_SWCTRL,
			pmic_read(ACCDET_STATE_SWCTRL) & (~ACCDET_PWM_IDLE));
		disable_accdet();
		headset_plug_out();
	}

#ifdef CONFIG_ACCDET_EINT
	enable_irq(accdet_irq);
	pr_info("accdet %s enable_irq !!\n", __func__);
#endif
}

static inline void check_cable_type(void)
{
	u32 cur_AB;

	cur_AB = pmic_read(ACCDET_STATE_RG) >> ACCDET_STATE_MEM_IN_OFFSET;
	cur_AB = cur_AB & ACCDET_STATE_AB_MASK;
	pr_notice("accdet %s, cur_status:%s current AB = %d\n", __func__,
		     accdet_status_str[accdet_status], cur_AB);

	s_button_status = 0;
	pre_status = accdet_status;

	switch (accdet_status) {
	case PLUG_OUT:
		if (cur_AB == ACCDET_STATE_AB_00) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag) {
				cable_type = HEADSET_NO_MIC;
				accdet_status = HOOK_SWITCH;
			} else
				pr_info("accdet headset has been plug-out\n");
			mutex_unlock(&accdet_eint_irq_sync_mutex);
		} else if (cur_AB == ACCDET_STATE_AB_01) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag) {
				accdet_status = MIC_BIAS;
				cable_type = HEADSET_MIC;

				/* AB=11 debounce=30ms */
				pmic_write(ACCDET_DEBOUNCE3,
					cust_pwm_deb->debounce3 * 30);
			} else
				pr_info("accdet headset has been plug-out\n");
			mutex_unlock(&accdet_eint_irq_sync_mutex);

			pmic_write(ACCDET_DEBOUNCE0, button_press_debounce);
		} else if (cur_AB == ACCDET_STATE_AB_11) {
			pr_info("accdet PLUG_OUT state not change!\n");
#ifdef CONFIG_ACCDET_EINT_IRQ
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag) {
				accdet_status = PLUG_OUT;
				cable_type = NO_DEVICE;
			} else
				pr_info("accdet headset has been plug-out\n");
			mutex_unlock(&accdet_eint_irq_sync_mutex);
#endif
		} else
			pr_info("accdet %s Invalid AB.Do nothing\n", __func__);
		break;
	case MIC_BIAS:
		/* solution: resume hook switch debounce time */
		pmic_write(ACCDET_DEBOUNCE0, cust_pwm_deb->debounce0);

		if (cur_AB == ACCDET_STATE_AB_00) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag) {
				s_button_status = 1;
				accdet_status = HOOK_SWITCH;
				multi_key_detection(cur_AB);
			} else
				pr_info("accdet headset has been plug-out\n");
			mutex_unlock(&accdet_eint_irq_sync_mutex);
		} else if (cur_AB == ACCDET_STATE_AB_01) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag) {
				accdet_status = MIC_BIAS;
				cable_type = HEADSET_MIC;
				pr_info("accdet MIC_BIAS state not change!\n");
			} else
				pr_info("accdet headset has been plug-out\n");
			mutex_unlock(&accdet_eint_irq_sync_mutex);
		} else if (cur_AB == ACCDET_STATE_AB_11) {
			pr_info("accdet Don't send plug out in MIC_BIAS\n");
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag)
				accdet_status = PLUG_OUT;
			else
				pr_info("accdet headset has been plug-out\n");
			mutex_unlock(&accdet_eint_irq_sync_mutex);
		} else
			pr_info("accdet %s Invalid AB.Do nothing\n", __func__);
		break;
	case HOOK_SWITCH:
		if (cur_AB == ACCDET_STATE_AB_00) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag)
				pr_info("accdet HOOKSWITCH state no change\n");
			else
				pr_info("accdet headset has been plug-out\n");
			mutex_unlock(&accdet_eint_irq_sync_mutex);
		} else if (cur_AB == ACCDET_STATE_AB_01) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag) {
				multi_key_detection(cur_AB);
				accdet_status = MIC_BIAS;
				cable_type = HEADSET_MIC;
			} else
				pr_info("accdet headset has been plug-out\n");
			mutex_unlock(&accdet_eint_irq_sync_mutex);

			/* solution: reduce hook switch deb-time to 0x400 */
			pmic_write(ACCDET_DEBOUNCE0, button_press_debounce);
		} else if (cur_AB == ACCDET_STATE_AB_11) {
			pr_info("accdet Don't send plugout in HOOK_SWITCH\n");
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag)
				accdet_status = PLUG_OUT;
			else
				pr_info("accdet headset has been plug-out\n");
			mutex_unlock(&accdet_eint_irq_sync_mutex);
		} else
			pr_info("accdet %s Invalid AB.Do nothing\n", __func__);
		break;
	case STAND_BY:
		pr_info("accdet %s STANDBY state.Err!Do nothing!\n", __func__);
		break;
	default:
		pr_info("accdet %s Error state.Do nothing!\n", __func__);
		break;
	}

	pr_info("accdet cur cable type:[%s], status switch:[%s]->[%s]\n",
		accdet_report_str[cable_type], accdet_status_str[pre_status],
		accdet_status_str[accdet_status]);
}

static void accdet_work_callback(struct work_struct *work)
{
	u32 pre_cable_type = cable_type;

	__pm_stay_awake(accdet_irq_lock);
	check_cable_type();

	mutex_lock(&accdet_eint_irq_sync_mutex);
	if (eint_accdet_sync_flag) {
		if (pre_cable_type != cable_type)
			send_accdet_status_event(cable_type, 1);
	} else
		pr_info("%s Headset been plugout don't set state\n", __func__);
	mutex_unlock(&accdet_eint_irq_sync_mutex);
	pr_info("%s report cable_type done\n", __func__);

	__pm_relax(accdet_irq_lock);
}

static void accdet_queue_work(void)
{
	int ret;

	if (accdet_status == MIC_BIAS)
		cali_voltage = accdet_get_auxadc(1);

	ret = queue_work(accdet_workqueue, &accdet_work);
	if (!ret)
		pr_info("queue work accdet_work return:%d!\n", ret);
}

#ifdef CONFIG_ACCDET_EINT_IRQ
static void pmic_eint_queue_work(void)
{
	pr_info("%s() Enter.\n", __func__);
	if (cur_eint_state == EINT_PIN_PLUG_IN) {
/* To trigger EINT when the headset was plugged in
 * We set the polarity back as we initialed.
 */
		pmic_write(ACCDET_EINT_CTL,
			pmic_read(ACCDET_EINT_CTL) & EINT_PLUG_DEB_CLR);
		pmic_write(ACCDET_EINT_CTL,
			pmic_read(ACCDET_EINT_CTL) | EINT_IRQ_DE_IN);
		pmic_write(ACCDET_DEBOUNCE3, cust_pwm_deb->debounce3);

		cur_eint_state = EINT_PIN_PLUG_OUT;
	} else {
/*
 * To trigger EINT when the headset was plugged out
 * We set the opposite polarity to what we initialed.
 */
		pmic_write(ACCDET_EINT_CTL,
			pmic_read(ACCDET_EINT_CTL) & EINT_PLUG_DEB_CLR);
		/* debounce=16ms */
		pmic_write(ACCDET_EINT_CTL,
			pmic_read(ACCDET_EINT_CTL) | EINT_IRQ_DE_OUT);

		cur_eint_state = EINT_PIN_PLUG_IN;

		mod_timer(&micbias_timer, jiffies + MICBIAS_DISABLE_TIMER);
	}
	queue_work(eint_workqueue, &eint_work);
}
#endif


static void accdet_irq_handle(void)
{
	u32 irq_status;

	irq_status = pmic_read(ACCDET_IRQ_STS);
	if ((irq_status & IRQ_STATUS_BIT) &&
		((irq_status & EINT_IRQ_STATUS_BIT) != EINT_IRQ_STATUS_BIT)) {
		pr_info("%s() IRQ_STS = 0x%x, IRQ triggered!!\n", __func__,
			irq_status);
		clear_accdet_int();
		accdet_queue_work();
		clear_accdet_int_check();
#ifdef CONFIG_ACCDET_EINT_IRQ
	} else if ((irq_status & EINT_IRQ_STATUS_BIT) == EINT_IRQ_STATUS_BIT) {
		pr_info("%s() IRQ_STS = 0x%x, EINT triggered!!\n", __func__,
			irq_status);
		eint_polarity_reverse();
		clear_accdet_eint();
		clear_accdet_eint_check();
		pmic_eint_queue_work();
#endif
	} else
		pr_info("%s no interrupt detected!\n", __func__);

}

static void accdet_int_handler(void)
{
	pr_debug("%s()\n", __func__);
	accdet_irq_handle();
}

#ifdef CONFIG_ACCDET_EINT_IRQ
static void accdet_eint_handler(void)
{
	pr_debug("%s()\n", __func__);
	accdet_irq_handle();
}
#endif

#ifdef CONFIG_ACCDET_EINT
static irqreturn_t ex_eint_handler(int irq, void *data)
{
	int ret = 0;

	if (cur_eint_state == EINT_PIN_PLUG_IN) {
		/* To trigger EINT when the headset was plugged in
		 * We set the polarity back as we initialed.
		 */
		if (accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
			irq_set_irq_type(accdet_irq, IRQ_TYPE_LEVEL_HIGH);
		else
			irq_set_irq_type(accdet_irq, IRQ_TYPE_LEVEL_LOW);
		gpio_set_debounce(gpiopin, gpio_headset_deb);

		cur_eint_state = EINT_PIN_PLUG_OUT;
	} else {
		/* To trigger EINT when the headset was plugged out
		 * We set the opposite polarity to what we initialed.
		 */
		if (accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
			irq_set_irq_type(accdet_irq, IRQ_TYPE_LEVEL_LOW);
		else
			irq_set_irq_type(accdet_irq, IRQ_TYPE_LEVEL_HIGH);

		gpio_set_debounce(gpiopin, accdet_dts.plugout_deb * 1000);

		cur_eint_state = EINT_PIN_PLUG_IN;

		mod_timer(&micbias_timer, jiffies + MICBIAS_DISABLE_TIMER);
	}

	disable_irq_nosync(accdet_irq);
	pr_info("accdet %s(), cur_eint_state=%d\n", __func__, cur_eint_state);
	ret = queue_work(eint_workqueue, &eint_work);
	return IRQ_HANDLED;

}

static int ext_eint_setup(struct platform_device *platform_device)
{
	int ret;
	u32 ints[4] = { 0 };
	struct device_node *node = NULL;
	struct pinctrl_state *pins_default;

	pr_info("accdet %s()\n", __func__);
	accdet_pinctrl = devm_pinctrl_get(&platform_device->dev);
	if (IS_ERR(accdet_pinctrl)) {
		ret = PTR_ERR(accdet_pinctrl);
		dev_notice(&platform_device->dev, "get accdet_pinctrl fail.\n");
		return ret;
	}

	pins_default = pinctrl_lookup_state(accdet_pinctrl, "default");
	if (IS_ERR(pins_default)) {
		ret = PTR_ERR(pins_default);
		dev_notice(&platform_device->dev, "lookup deflt pinctrl fail\n");
	}

	pins_eint = pinctrl_lookup_state(accdet_pinctrl, "state_eint_as_int");
	if (IS_ERR(pins_eint)) {
		ret = PTR_ERR(pins_eint);
		dev_notice(&platform_device->dev, "lookup eint pinctrl fail\n");
		return ret;
	}
	pinctrl_select_state(accdet_pinctrl, pins_eint);

	node = of_find_matching_node(node, accdet_of_match);
	if (!node) {
		pr_notice("accdet %s can't find compatible node\n", __func__);
		return -1;
	}

	gpiopin = of_get_named_gpio(node, "deb-gpios", 0);
	ret = of_property_read_u32(node, "debounce", &gpio_headset_deb);
	if (ret) {
		pr_notice("accdet %s gpiodebounce not found,ret:%d\n",
			__func__, ret);
		return ret;
	}
	gpio_set_debounce(gpiopin, gpio_headset_deb);

	accdet_irq = irq_of_parse_and_map(node, 0);
	ret = of_property_read_u32_array(node, "interrupts", ints,
			ARRAY_SIZE(ints));
	if (ret) {
		pr_notice("accdet %s interrupts not found,ret:%d\n",
			__func__, ret);
		return ret;
	}
	accdet_eint_type = ints[1];
	pr_info("accdet set gpio EINT, gpiopin=%d, accdet_eint_type=%d\n",
			gpiopin, accdet_eint_type);
	ret = request_irq(accdet_irq, ex_eint_handler, IRQF_TRIGGER_NONE,
		"accdet-eint", NULL);
	if (ret) {
		pr_notice("accdet %s request_irq fail, ret:%d.\n", __func__,
			ret);
		return ret;
	}

	pr_info("accdet set gpio EINT finished, irq=%d, gpio_headset_deb=%d\n",
			accdet_irq, gpio_headset_deb);

	return 0;
}
#endif

static int accdet_get_dts_data(void)
{
	int ret;
	struct device_node *node = NULL;
	int pwm_deb[7];
#ifdef CONFIG_FOUR_KEY_HEADSET
	int four_key[5];
#else
	int three_key[4];
#endif

	pr_debug("%s\n", __func__);
	node = of_find_matching_node(node, accdet_of_match);
	if (!node) {
		pr_notice("%s can't find compatible dts node\n", __func__);
		return -1;
	}

	of_property_read_u32(node, "accdet-mic-vol", &accdet_dts.mic_vol);
	of_property_read_u32(node, "accdet-plugout-debounce",
			&accdet_dts.plugout_deb);
	of_property_read_u32(node, "accdet-mic-mode", &accdet_dts.mic_mode);
	of_property_read_u32(node, "headset-eint-level-pol",
			&accdet_dts.eint_pol);

	pr_info("accdet mic_vol=%x, plugout_deb=%x mic_mode=%x eint_pol=%x\n",
	     accdet_dts.mic_vol, accdet_dts.plugout_deb,
	     accdet_dts.mic_mode, accdet_dts.eint_pol);

#ifdef CONFIG_FOUR_KEY_HEADSET
	ret = of_property_read_u32_array(node, "headset-four-key-threshold",
		four_key, ARRAY_SIZE(four_key));
	if (!ret)
		memcpy(&accdet_dts.four_key, four_key+1,
				sizeof(struct four_key_threshold));
	else
		pr_info("accdet get 4-key-thrsh fail\n");

	pr_info("accdet key thresh mid = %d, voice = %d, up = %d, dwn = %d\n",
		accdet_dts.four_key.mid, accdet_dts.four_key.voice,
		accdet_dts.four_key.up, accdet_dts.four_key.down);
#else
#ifdef CONFIG_HEADSET_TRI_KEY_CDD
	ret = of_property_read_u32_array(node,
		"headset-three-key-threshold-CDD", three_key,
		ARRAY_SIZE(three_key));
#else
	ret = of_property_read_u32_array(node, "headset-three-key-threshold",
			three_key, ARRAY_SIZE(three_key));
#endif
	if (!ret)
		memcpy(&accdet_dts.three_key, three_key+1,
				sizeof(struct three_key_threshold));
	else
		pr_info("accdet get 3-key-thrsh fail\n");

	pr_info("accdet key thresh mid = %d, up = %d, down = %d\n",
			     accdet_dts.three_key.mid, accdet_dts.three_key.up,
			     accdet_dts.three_key.down);
#endif

	ret = of_property_read_u32_array(node, "headset-mode-setting", pwm_deb,
			ARRAY_SIZE(pwm_deb));
	if (!ret)
		memcpy(&accdet_dts.pwm_deb, pwm_deb, sizeof(pwm_deb));
	else
		pr_info("accdet get pwm-debounce setting fail\n");

	cust_pwm_deb = &accdet_dts.pwm_deb;
	pr_info("accdet pwm_width=0x%x, thrsh=0x%x, deb0=0x%x, deb1=0x%x\n",
	     cust_pwm_deb->pwm_width, cust_pwm_deb->pwm_thresh,
	     cust_pwm_deb->debounce0, cust_pwm_deb->debounce1);

	return 0;
}


static inline void accdet_init(void)
{
#ifdef CONFIG_ACCDET_EINT_IRQ
	u32 val;
#endif

	/* clock */
	pmic_write(TOP_CKPDN_CLR, RG_ACCDET_CLK_CLR);

	/* reset the accdet unit */
	pmic_write(TOP_RST_ACCDET_SET, ACCDET_RESET_SET);
	pmic_write(TOP_RST_ACCDET_CLR, ACCDET_RESET_CLR);

	/* init  pwm frequency and duty */
	pmic_write(ACCDET_PWM_WIDTH, REGISTER_VAL(cust_pwm_deb->pwm_width));
	pmic_write(ACCDET_PWM_THRESH, REGISTER_VAL(cust_pwm_deb->pwm_thresh));
	pmic_write(ACCDET_STATE_SWCTRL, 0x07);

	/* rise and fall delay of PWM */
	pmic_write(ACCDET_EN_DELAY_NUM,
		(cust_pwm_deb->fall_delay << 15 | cust_pwm_deb->rise_delay));
	/* init the debounce time */
	pmic_write(ACCDET_DEBOUNCE0, cust_pwm_deb->debounce0);
	pmic_write(ACCDET_DEBOUNCE1, cust_pwm_deb->debounce1);
	pmic_write(ACCDET_DEBOUNCE3, cust_pwm_deb->debounce3);
	pmic_write(ACCDET_DEBOUNCE4, ACCDET_DE4);

	/* enable INT */
#ifdef CONFIG_ACCDET_EINT_IRQ
	if (cur_eint_state == EINT_PIN_PLUG_OUT &&
		accdet_dts.eint_pol == IRQ_TYPE_LEVEL_HIGH) {
		accdet_eint_type = IRQ_TYPE_LEVEL_HIGH;
		pmic_write(ACCDET_IRQ_STS,
			pmic_read(ACCDET_IRQ_STS)|EINT_IRQ_POL_HIGH);

		val = pmic_read(ACCDET_IRQ_STS);
		pr_info("accdet set eint high level:IRQ_STS=0x%x\n", val);

		if (atomic_read(&accdet_first)) {
			val = pmic_read(ACCDET_CTRL);
			/* set bit3 to enable default EINT init status */
			pmic_write(ACCDET_CTRL, val | ACCDET_EINT_INIT);
			mdelay(2);
			val = pmic_read(ACCDET_CTRL);
			pr_debug("accdet eint init [0x%x]=0x%x\n",
				ACCDET_CTRL, val);

			val = pmic_read(ACCDET_DEFAULT_STATE_RG);
			/* set default EINT init status */
			pmic_write(ACCDET_DEFAULT_STATE_RG,
				(val & ACCDET_VAL_DEF) | ACCDET_EINT_IVAL_SEL);
			mdelay(2);
			val = pmic_read(ACCDET_DEFAULT_STATE_RG);
			pr_debug("accdet set eint default done [0x%x]=0x%x\n",
				ACCDET_DEFAULT_STATE_RG, val);

			/* clear bit3 to disable default EINT init status */
			pmic_write(ACCDET_CTRL,
				pmic_read(ACCDET_CTRL)&(~ACCDET_EINT_INIT));
		}
	}
	pmic_write(ACCDET_IRQ_STS,
		pmic_read(ACCDET_IRQ_STS) & (~IRQ_EINT_CLR_BIT));
#endif

#ifdef CONFIG_ACCDET_EINT
	pmic_write(ACCDET_IRQ_STS, pmic_read(ACCDET_IRQ_STS) & (~IRQ_CLR_BIT));
#endif
	pmic_write(INT_CON_ACCDET_SET, RG_ACCDET_IRQ_SET);

#ifdef CONFIG_ACCDET_EINT_IRQ
	pmic_write(INT_CON_ACCDET_SET, RG_ACCDET_EINT_IRQ_SET);
#endif

   /* ACCDET Analog Setting */
	pmic_write(ACCDET_ADC_REG,
		pmic_read(ACCDET_ADC_REG) | RG_AUDACCDETPULLLOW);
	pmic_write(ACCDET_MICBIAS_REG, pmic_read(ACCDET_MICBIAS_REG)
		| (accdet_dts.mic_vol<<4) | RG_AUDMICBIAS1LOWPEN);
	pmic_write(ACCDET_RSV, 0x0010);

#ifdef CONFIG_ACCDET_EINT_IRQ
	/* Internal connection between ACCDET and EINT */
	pmic_write(ACCDET_ADC_REG,
		pmic_read(ACCDET_ADC_REG) | RG_EINTCONFIGACCDET);
#endif

	/* ACC mode */
	if (accdet_dts.mic_mode == 1)
		pr_notice("%s init ACC mode1\n", __func__);
	/* Low cost mode without internal bias */
	else if (accdet_dts.mic_mode == 2)
		pmic_write(ACCDET_ADC_REG,
			pmic_read(ACCDET_ADC_REG) | RG_EINT_ANA_CONFIG);
	/* Low cost mode with internal bias */
	else if (accdet_dts.mic_mode == 6) {
		pmic_write(ACCDET_ADC_REG,
			pmic_read(ACCDET_ADC_REG) | RG_EINT_ANA_CONFIG);
		pmic_write(ACCDET_MICBIAS_REG,
			pmic_read(ACCDET_MICBIAS_REG) | RG_MICBIAS1DCSWPEN);
		pr_notice("%s init DCC mode6\n", __func__);
	}

#ifdef CONFIG_ACCDET_EINT_IRQ
	pmic_write(ACCDET_HW_MODE_DFF,
	ACCDET_EINT_REVERSE | ACCDET_HWMODE_SEL | ACCDET_EINT_DEB_OUT_DFF);
#else
	/* Only use SW path */
	pmic_write(ACCDET_HW_MODE_DFF, ACCDET_EINT_REVERSE);
#endif

#if defined CONFIG_ACCDET_EINT
	/* disable ACCDET unit */
	pmic_write(ACCDET_CTRL, ACCDET_DISABLE);
	pmic_write(ACCDET_STATE_SWCTRL, 0x0);
	pmic_write(TOP_CKPDN_SET, RG_ACCDET_CLK_SET);
#elif defined CONFIG_ACCDET_EINT_IRQ
	if (cur_eint_state == EINT_PIN_PLUG_OUT)
		pmic_write(ACCDET_EINT_CTL,
			pmic_read(ACCDET_EINT_CTL) | EINT_IRQ_DE_IN);
	pmic_write(ACCDET_EINT_CTL,
		pmic_read(ACCDET_EINT_CTL) | EINT_PWM_THRESH);
	/* disable ACCDET unit, except CLK of ACCDET */
	pmic_write(ACCDET_CTRL, pmic_read(ACCDET_CTRL) | ACCDET_DISABLE);
	pmic_write(ACCDET_STATE_SWCTRL,
		pmic_read(ACCDET_STATE_SWCTRL) & (~ACCDET_PWM_EN));
	pmic_write(ACCDET_STATE_SWCTRL,
		pmic_read(ACCDET_STATE_SWCTRL) | ACCDET_EINT_PWM_EN);
	pmic_write(ACCDET_CTRL, pmic_read(ACCDET_CTRL) | ACCDET_EINT_EN);
#else
	/* enable ACCDET unit */
	pmic_write(ACCDET_CTRL, ACCDET_ENABLE);
#endif
	/* AUXADC enable auto sample */
	pmic_write(ACCDET_AUXADC_AUTO_SPL,
	(pmic_read(ACCDET_AUXADC_AUTO_SPL) | ACCDET_AUXADC_AUTO_SET));

	pr_info("%s() done.\n", __func__);
}

/* late init for DC trim, and this API  Will be called by audio */
void accdet_late_init(unsigned long data)
{
	pr_info("%s()  now init accdet!\n", __func__);
	if (atomic_cmpxchg(&accdet_first, 1, 0)) {
		del_timer_sync(&accdet_init_timer);
		accdet_init();
		accdet_get_efuse();
	} else
		pr_info("%s err:inited already done/get dts fail\n", __func__);
}
EXPORT_SYMBOL(accdet_late_init);

static void delay_init_timerhandler(unsigned long data)
{
	pr_info("%s()  now init accdet!\n", __func__);
	if (atomic_cmpxchg(&accdet_first, 1, 0)) {
		accdet_init();
		accdet_get_efuse();
	} else
		pr_info("%s err:inited already done/get dts fail\n", __func__);
}

int mt_accdet_probe(struct platform_device *dev)
{
	int ret;
	struct platform_driver accdet_driver_hal = accdet_driver_func();

	pr_info("%s() begin!\n", __func__);

	/* register char device number, Create normal device for auido use */
	ret = alloc_chrdev_region(&accdet_devno, 0, 1, ACCDET_DEVNAME);
	if (ret) {
		pr_notice("%s alloc_chrdev_reg fail,ret:%d!\n", __func__, ret);
		goto err_chrdevregion;
	}

	/* init cdev, and add it to system */
	accdet_cdev = cdev_alloc();
	accdet_cdev->owner = THIS_MODULE;
	accdet_cdev->ops = accdet_get_fops();
	ret = cdev_add(accdet_cdev, accdet_devno, 1);
	if (ret) {
		pr_notice("%s cdev_add fail.ret:%d\n", __func__, ret);
		goto err_cdev_add;
	}

	/* create class in sysfs, "sys/class/", so udev in userspace can create
	 *device node, when device_create is called
	 */
	accdet_class = class_create(THIS_MODULE, ACCDET_DEVNAME);
	if (!accdet_class) {
		ret = -1;
		pr_notice("%s class_create fail.\n", __func__);
		goto err_class_create;
	}

	/* create device under /dev node
	 * if we want auto creat device node, we must call this
	 */
	accdet_device = device_create(accdet_class, NULL, accdet_devno,
		NULL, ACCDET_DEVNAME);
	if (!accdet_device) {
		ret = -1;
		pr_notice("%s device_create fail.\n", __func__);
		goto err_device_create;
	}

	/* Create input device*/
	accdet_input_dev = input_allocate_device();
	if (!accdet_input_dev) {
		ret = -ENOMEM;
		pr_notice("%s input_allocate_device fail.\n", __func__);
		goto err_input_alloc;
	}

	__set_bit(EV_KEY, accdet_input_dev->evbit);
	__set_bit(KEY_PLAYPAUSE, accdet_input_dev->keybit);
	__set_bit(KEY_VOLUMEDOWN, accdet_input_dev->keybit);
	__set_bit(KEY_VOLUMEUP, accdet_input_dev->keybit);
	__set_bit(KEY_VOICECOMMAND, accdet_input_dev->keybit);

	__set_bit(EV_SW, accdet_input_dev->evbit);
	__set_bit(SW_HEADPHONE_INSERT, accdet_input_dev->swbit);
	__set_bit(SW_MICROPHONE_INSERT, accdet_input_dev->swbit);
	__set_bit(SW_JACK_PHYSICAL_INSERT, accdet_input_dev->swbit);
	__set_bit(SW_LINEOUT_INSERT, accdet_input_dev->swbit);

	accdet_input_dev->id.bustype = BUS_HOST;
	accdet_input_dev->name = "ACCDET";
	ret = input_register_device(accdet_input_dev);
	if (ret) {
		pr_notice("%s input_register_device fail.ret:%d\n", __func__,
			ret);
		goto err_input_reg;
	}

	ret = accdet_create_attr(&accdet_driver_hal.driver);
	if (ret) {
		pr_notice("%s create_attr fail, ret = %d\n", __func__, ret);
		goto err_create_attr;
	}

	/* init the timer to disable micbias. */
	init_timer(&micbias_timer);
	micbias_timer.expires = jiffies + MICBIAS_DISABLE_TIMER;
	micbias_timer.function = &dis_micbias_timerhandler;
	micbias_timer.data = 0;

	/* set the timer ensure accdet can be init even audio never call */
	init_timer(&accdet_init_timer);
	accdet_init_timer.expires = jiffies + ACCDET_INIT_WAIT_TIMER;
	accdet_init_timer.function = &delay_init_timerhandler;
	accdet_init_timer.data = 0;

	/* wake lock */
	accdet_irq_lock = wakeup_source_register("accdet_irq_lock");
	accdet_timer_lock = wakeup_source_register("accdet_timer_lock");

	/* Create workqueue */
	accdet_workqueue = create_singlethread_workqueue("accdet");
	INIT_WORK(&accdet_work, accdet_work_callback);
	if (!accdet_workqueue) {
		ret = -1;
		pr_notice("%s create accdet workqueue fail.\n", __func__);
		goto err_create_attr;
	}

	/* register pmic interrupt */
	pmic_register_interrupt_callback(INT_ACCDET, accdet_int_handler);
#ifdef CONFIG_ACCDET_EINT_IRQ
	pmic_register_interrupt_callback(INT_ACCDET_EINT, accdet_eint_handler);
#endif

	ret = accdet_get_dts_data();
	if (ret) {
		atomic_set(&accdet_first, 0);
		pr_notice("%s accdet_get_dts_data err!\n", __func__);
		goto err;
	}
	dis_micbias_workqueue = create_singlethread_workqueue("dismicQueue");
	INIT_WORK(&dis_micbias_work, dis_micbias_work_callback);
	if (!dis_micbias_workqueue) {
		ret = -1;
		pr_notice("%s create dis micbias workqueue fail.\n", __func__);
		goto err;
	}
	eint_workqueue = create_singlethread_workqueue("accdet_eint");
	INIT_WORK(&eint_work, eint_work_callback);
	if (!eint_workqueue) {
		ret = -1;
		pr_notice("%s create eint workqueue fail.\n", __func__);
		goto err_create_workqueue;
	}

#ifdef CONFIG_ACCDET_EINT
	ret = ext_eint_setup(dev);
	if (ret) {
		pr_notice("%s ap eint setup fail.ret:%d\n", __func__, ret);
		goto err_eint_setup;
	}
#endif
	atomic_set(&accdet_first, 1);
	mod_timer(&accdet_init_timer, (jiffies + ACCDET_INIT_WAIT_TIMER));

	pr_info("%s done!\n", __func__);
	return 0;

#ifdef CONFIG_ACCDET_EINT
err_eint_setup:
	destroy_workqueue(eint_workqueue);
#endif

err_create_workqueue:
	destroy_workqueue(dis_micbias_workqueue);
err:
	destroy_workqueue(accdet_workqueue);
err_create_attr:
	input_unregister_device(accdet_input_dev);
err_input_reg:
	input_free_device(accdet_input_dev);
err_input_alloc:
	device_del(accdet_device);
err_device_create:
	class_destroy(accdet_class);
err_class_create:
	cdev_del(accdet_cdev);
err_cdev_add:
	unregister_chrdev_region(accdet_devno, 1);
err_chrdevregion:
	pr_notice("%s error. now exit.!\n", __func__);
	return ret;
}

void mt_accdet_remove(void)
{
	pr_debug("%s enter!\n", __func__);

	/* cancel_delayed_work(&accdet_work); */
	destroy_workqueue(eint_workqueue);
	destroy_workqueue(dis_micbias_workqueue);
	destroy_workqueue(accdet_workqueue);
	input_unregister_device(accdet_input_dev);
	input_free_device(accdet_input_dev);
	device_del(accdet_device);
	class_destroy(accdet_class);
	cdev_del(accdet_cdev);
	unregister_chrdev_region(accdet_devno, 1);
	pr_debug("%s done!\n", __func__);
}

long mt_accdet_unlocked_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	switch (cmd) {
	case ACCDET_INIT:
		break;
	case SET_CALL_STATE:
		break;
	case GET_BUTTON_STATUS:
		return s_button_status;
	default:
		pr_debug("[Accdet]accdet_ioctl : default\n");
		break;
	}
	return 0;
}

