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
#define EINT_PIN_MOISTURE_DETECED (2)
#define ANALOG_FASTDISCHARGE_SUPPORT

#ifdef CONFIG_ACCDET_EINT_IRQ
enum pmic_eint_ID {
	NO_PMIC_EINT = 0,
	PMIC_EINT0 = 1,
	PMIC_EINT1 = 2,
	PMIC_BIEINT = 3,
};
/* #define HW_MODE_SUPPORT */
/* #define DIGITAL_FASTDISCHARGE_SUPPORT */
#endif

/* accdet_status_str: to record current 'accdet_status' by string,
 *mapping to  'enum accdet_status'
 */
static char *accdet_status_str[] = {
	"Plug_out",
	"Headset_plug_in",
	"Hook_switch",
	"Bi_mic",
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
#ifdef CONFIG_ACCDET_SUPPORT_BI_EINT
static u32 cur_eint0_state = EINT_PIN_PLUG_OUT;
static u32 cur_eint1_state = EINT_PIN_PLUG_OUT;
#endif
static u32 pre_status;
static u32 accdet_status = PLUG_OUT;
static u32 cable_type;
static u32 cur_key;
static u32 cali_voltage;
static int accdet_auxadc_offset;
static struct wakeup_source *accdet_irq_lock;
static struct wakeup_source *accdet_timer_lock;
static DEFINE_MUTEX(accdet_eint_irq_sync_mutex);

static u32 accdet_eint_type = IRQ_TYPE_LEVEL_LOW;
static u32 button_press_debounce = 0x400;
static atomic_t accdet_first;
#ifdef HW_MODE_SUPPORT
#ifdef DIGITAL_FASTDISCHARGE_SUPPORT
static bool fast_discharge = true;
#endif
#endif

/* accdet Moisture detect */
#if defined(CONFIG_MOISTURE_EXT_SUPPORT) || defined(CONFIG_MOISTURE_INT_SUPPORT)
/* unit is mv */
static int moisture_vdd_offset;
static int moisture_offset;
static int moisture_vm;
/* moisture resister ohm */
static int water_r = 10000;
#ifdef CONFIG_MOISTURE_INT_SUPPORT
/* unit is ohm */
static int moisture_eint_offset;
/* unit is ohm */
static int moisture_int_r = 47000;
#endif
#ifdef CONFIG_MOISTURE_EXT_SUPPORT
/* unit is ohm */
static int moisture_ext_r = 470000;
#endif
#endif

static bool debug_thread_en;
static bool dump_reg;
static struct task_struct *thread;

static void accdet_init_once(void);
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

#ifdef CONFIG_ACCDET_EINT_IRQ
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
	pr_info("Accdet EINT0 support,MODE_%d regs:\n", accdet_dts.mic_mode);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
	pr_info("Accdet EINT1 support,MODE_%d regs:\n", accdet_dts.mic_mode);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
	pr_info("Accdet BIEINT support,MODE_%d regs:\n",
				accdet_dts.mic_mode);
#else
	pr_info("ACCDET_EINT_IRQ:NO EINT configed.Error!!\n");
#endif
#elif defined CONFIG_ACCDET_EINT
	pr_info("Accdet EINT,MODE_%d regs:\n", accdet_dts.mic_mode);
#endif

	for (i = ACCDET_RSV; i <= ACCDET_EINT1_CUR_DEB; i += 2)
		pr_info("ACCDET_ADDR:(0x%x)=0x%x\n", i, pmic_read(i));

	pr_info("(0x%x)=0x%x\n", TOP_CKPDN_CON0, pmic_read(TOP_CKPDN_CON0));
	pr_info("(0x%x)=0x%x\n", AUD_TOP_CKPDN_CON0,
			pmic_read(AUD_TOP_CKPDN_CON0));
	pr_info("(0x%x)=0x%x\n", AUD_TOP_RST_CON0,
			pmic_read(AUD_TOP_RST_CON0));
	pr_info("(0x%x)=0x%x\n", AUD_TOP_INT_CON0,
			pmic_read(AUD_TOP_INT_CON0));
	pr_info("(0x%x)=0x%x\n", AUD_TOP_INT_MASK_CON0,
			pmic_read(AUD_TOP_INT_MASK_CON0));
	pr_info("(0x%x)=0x%x\n", AUD_TOP_INT_STATUS0,
			pmic_read(AUD_TOP_INT_STATUS0));
	pr_info("(0x%x)=0x%x\n", AUDENC_ANA_CON6, pmic_read(AUDENC_ANA_CON6));
	pr_info("(0x%x)=0x%x\n", AUDENC_ANA_CON9, pmic_read(AUDENC_ANA_CON9));
	pr_info("(0x%x)=0x%x\n", AUDENC_ANA_CON10,
			pmic_read(AUDENC_ANA_CON10));
	pr_info("(0x%x)=0x%x\n", AUDENC_ANA_CON11,
			pmic_read(AUDENC_ANA_CON11));
	pr_info("(0x%x)=0x%x\n", AUXADC_RQST0, pmic_read(AUXADC_RQST0));
	pr_info("(0x%x)=0x%x\n", AUXADC_ACCDET, pmic_read(AUXADC_ACCDET));

	pr_info("accdet_dts:deb0=0x%x,deb1=0x%x,deb3=0x%x,deb4=0x%x\n",
		 cust_pwm_deb->debounce0, cust_pwm_deb->debounce1,
		 cust_pwm_deb->debounce3, cust_pwm_deb->debounce4);
}

static void cat_register(char *buf)
{
	int i = 0;
	char buf_temp[128] = { 0 };

#ifdef CONFIG_ACCDET_EINT_IRQ
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
	sprintf(buf_temp, "[Accdet EINT0 support][MODE_%d]regs:\n",
		accdet_dts.mic_mode);
	strncat(buf, buf_temp, strlen(buf_temp));
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
	sprintf(buf_temp, "[ccdet EINT1 support][MODE_%d]regs:\n",
		accdet_dts.mic_mode);
	strncat(buf, buf_temp, strlen(buf_temp));
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
	sprintf(buf_temp, "[Accdet BIEINT support][MODE_%d] regs:\n",
		accdet_dts.mic_mode);
	strncat(buf, buf_temp, strlen(buf_temp));
#else
	strncat(buf, "ACCDET_EINT_IRQ:NO EINT configed.Error!!\n", 64);
#endif
#elif defined CONFIG_ACCDET_EINT
	sprintf(buf_temp, "[Accdet AP EINT][MODE_%d] regs:\n",
		accdet_dts.mic_mode);
	strncat(buf, buf_temp, strlen(buf_temp));
#else
	strncat(buf, "ACCDET EINT:No configed.Error!!\n", 64);
#endif

	for (i = ACCDET_RSV; i <= ACCDET_EINT1_CUR_DEB; i += 2) {
		sprintf(buf_temp, "ADDR[0x%x]=0x%x\n", i, pmic_read(i));
		strncat(buf, buf_temp, strlen(buf_temp));
	}

	sprintf(buf_temp, "[0x%x]=0x%x\n",
		TOP_CKPDN_CON0, pmic_read(TOP_CKPDN_CON0));
	strncat(buf, buf_temp, strlen(buf_temp));

	sprintf(buf_temp, "[0x%x]=0x%x\n",
		AUD_TOP_RST_CON0, pmic_read(AUD_TOP_RST_CON0));
	strncat(buf, buf_temp, strlen(buf_temp));

	sprintf(buf_temp, "[0x%x]=0x%x, [0x%x]=0x%x, [0x%x]=0x%x\n",
		AUD_TOP_INT_CON0, pmic_read(AUD_TOP_INT_CON0),
		AUD_TOP_INT_MASK_CON0, pmic_read(AUD_TOP_INT_MASK_CON0),
		AUD_TOP_INT_STATUS0, pmic_read(AUD_TOP_INT_STATUS0));
	strncat(buf, buf_temp, strlen(buf_temp));

	sprintf(buf_temp, "[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x\n",
		AUDENC_ANA_CON6, pmic_read(AUDENC_ANA_CON6),
		AUDENC_ANA_CON9, pmic_read(AUDENC_ANA_CON9),
		AUDENC_ANA_CON10, pmic_read(AUDENC_ANA_CON10),
		AUDENC_ANA_CON11, pmic_read(AUDENC_ANA_CON11));
	strncat(buf, buf_temp, strlen(buf_temp));

	sprintf(buf_temp, "[0x%x]=0x%x, [0x%x]=0x%x\n",
		AUXADC_RQST0, pmic_read(AUXADC_RQST0),
		AUXADC_ACCDET, pmic_read(AUXADC_ACCDET));
	strncat(buf, buf_temp, strlen(buf_temp));

	sprintf(buf_temp, "dtsInfo:deb0=0x%x,deb1=0x%x,deb3=0x%x,deb4=0x%x\n",
		 cust_pwm_deb->debounce0, cust_pwm_deb->debounce1,
		 cust_pwm_deb->debounce3, cust_pwm_deb->debounce4);
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
	accdet_init_once();

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
	u32 efuseval = 0;
#ifdef CONFIG_FOUR_KEY_HEADSET
	u32 tmp_val = 0;
	u32 tmp_8bit = 0
#endif
#ifdef CONFIG_MOISTURE_INT_SUPPORT
	int tmp_div;
	unsigned int moisture_eint0;
	unsigned int moisture_eint1;
#endif

#ifdef CONFIG_FOUR_KEY_HEADSET
	/* 4-key efuse:
	 * bit[9:2] efuse value is loaded, so every read out value need to be
	 * left shift 2 bit,and then compare with voltage get from AUXADC.
	 * AD efuse: key-A Voltage:0--AD;
	 * DB efuse: key-D Voltage: AD--DB;
	 * BC efuse: key-B Voltage:DB--BC;
	 * key-C Voltage: BC--600;
	 */
	tmp_val = pmic_Read_Efuse_HPOffset(111);
	tmp_8bit = tmp_val & ACCDET_CALI_MASK0;
	accdet_dts.four_key.mid = tmp_8bit << 2;

	tmp_8bit = (tmp_val >> 8) & ACCDET_CALI_MASK0;
	accdet_dts.four_key.voice = tmp_8bit << 2;

	tmp_val = pmic_Read_Efuse_HPOffset(112);
	tmp_8bit = tmp_val & ACCDET_CALI_MASK0;
	accdet_dts.four_key.up = tmp_8bit << 2;

	accdet_dts.four_key.down = 600;
	pr_info("accdet key thresh: mid=%dmv,voice=%dmv,up=%dmv,down=%dmv\n",
		accdet_dts.four_key.mid, accdet_dts.four_key.voice,
		accdet_dts.four_key.up, accdet_dts.four_key.down);
#endif
	/* accdet offset efuse:
	 * this efuse must divided by 2
	 */
	efuseval = pmic_Read_Efuse_HPOffset(110);
	accdet_auxadc_offset = efuseval & 0xFF;
	if (accdet_auxadc_offset > 128)
		accdet_auxadc_offset -= 256;
	accdet_auxadc_offset = (accdet_auxadc_offset >> 1);
	pr_info("%s efuse=0x%x,auxadc_val=%dmv\n", __func__, efuseval,
		accdet_auxadc_offset);

/* all of moisture_vdd/moisture_offset0/eint is  2'complement,
 * we need to transfer it
 */
#if defined(CONFIG_MOISTURE_EXT_SUPPORT) || defined(CONFIG_MOISTURE_INT_SUPPORT)
	/* moisture vdd efuse offset */
	efuseval = pmic_Read_Efuse_HPOffset(113);
	moisture_vdd_offset = (int)((efuseval >> 8) & ACCDET_CALI_MASK0);
	if (moisture_vdd_offset > 128)
		moisture_vdd_offset -= 256;
	pr_info("%s moisture_vdd efuse=0x%x, moisture_vdd_offset=%d mv\n",
		__func__, efuseval, moisture_vdd_offset);

	/* moisture offset */
	efuseval = pmic_Read_Efuse_HPOffset(114);
	moisture_offset = (int)(efuseval & ACCDET_CALI_MASK0);
	if (moisture_offset > 128)
		moisture_offset -= 256;
	pr_info("%s moisture_efuse efuse=0x%x,moisture_offset=%d mv\n",
		__func__, efuseval, moisture_offset);

#ifdef CONFIG_MOISTURE_INT_SUPPORT
	/* moisture eint efuse offset */
	efuseval = pmic_Read_Efuse_HPOffset(112);
	moisture_eint0 = (int)((efuseval >> 8) & ACCDET_CALI_MASK0);
	pr_info("%s moisture_eint0 efuse=0x%x,moisture_eint0=0x%x\n",
		__func__, efuseval, moisture_eint0);

	efuseval = pmic_Read_Efuse_HPOffset(113);
	moisture_eint1 = (int)(efuseval & ACCDET_CALI_MASK0);
	pr_info("%s moisture_eint1 efuse=0x%x,moisture_eint1=0x%x\n",
		__func__, efuseval, moisture_eint1);

	moisture_eint_offset = (moisture_eint1 << 8) | moisture_eint0;
	if (moisture_eint_offset > 32768)
		moisture_eint_offset -= 65536;
	pr_info("%s moisture_eint_offset=%d ohm\n", __func__,
		moisture_eint_offset);

	moisture_vm = (2800 + moisture_vdd_offset);
	moisture_vm = moisture_vm * (water_r + moisture_int_r);
	tmp_div = water_r + moisture_int_r + 8 * moisture_eint_offset + 450000;
	moisture_vm = moisture_vm / tmp_div;
	moisture_vm = moisture_vm + moisture_offset / 2;
	pr_info("%s internal moisture_vm=%d mv\n", __func__, moisture_vm);
#endif
#ifdef CONFIG_MOISTURE_EXT_SUPPORT
	moisture_vm = (2800 + moisture_vdd_offset);
	moisture_vm = moisture_vm * water_r;
	moisture_vm = moisture_vm / (water_r + moisture_ext_r);
	moisture_vm = moisture_vm + (moisture_offset >> 1);
	pr_info("%s external moisture_vm=%d mv\n", __func__, moisture_vm);
#endif
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
	irq_bit = !(pmic_read(ACCDET_IRQ_STS) & ACCDET_EINT_IRQ_B2_B3);
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
		pmic_read(ACCDET_IRQ_STS) | ACCDET_IRQ_CLR_B8);
	pr_debug("%s() IRQ_STS = 0x%x\n", __func__, pmic_read(ACCDET_IRQ_STS));
}

static inline void clear_accdet_int_check(void)
{
	u64 cur_time = accdet_get_current_time();

	while ((pmic_read(ACCDET_IRQ_STS) & ACCDET_IRQ_B0) &&
		(accdet_timeout_ns(cur_time, ACCDET_TIME_OUT)))
		;
	/* clear accdet int, modify  for fix interrupt trigger twice error */
	pmic_write(ACCDET_IRQ_STS,
		pmic_read(ACCDET_IRQ_STS) & (~ACCDET_IRQ_CLR_B8));
	pmic_write(AUD_TOP_INT_STATUS0, RG_INT_STATUS_ACCDET_B5);
}

#ifdef CONFIG_ACCDET_EINT_IRQ
static inline void clear_accdet_eint(u32 eintid)
{
	u32 reg_val = pmic_read(ACCDET_IRQ_STS);

	if ((eintid & PMIC_EINT0) == PMIC_EINT0)
		pmic_write(ACCDET_IRQ_STS, reg_val | ACCDET_EINT0_IRQ_CLR_B10);

	if ((eintid & PMIC_EINT1) == PMIC_EINT1)
		pmic_write(ACCDET_IRQ_STS, reg_val | ACCDET_EINT1_IRQ_CLR_B11);

	pr_info("%s() eint-%s IRQ-STS:[0x%x]=0x%x\n", __func__,
		(eintid == PMIC_EINT0)?"0":((eintid == PMIC_EINT1)?"1":"BI"),
		ACCDET_IRQ_STS, pmic_read(ACCDET_IRQ_STS));
}

static inline void clear_accdet_eint_check(u32 eintid)
{
	u64 cur_time = accdet_get_current_time();

	if ((eintid & PMIC_EINT0) == PMIC_EINT0) {
		while ((pmic_read(ACCDET_IRQ_STS) & ACCDET_EINT0_IRQ_B2)
			&& (accdet_timeout_ns(cur_time, ACCDET_TIME_OUT)))
			;
		pmic_write(ACCDET_IRQ_STS,
			pmic_read(ACCDET_IRQ_STS)&(~ACCDET_EINT0_IRQ_CLR_B10));
		pmic_write(AUD_TOP_INT_STATUS0, RG_INT_STATUS_ACCDET_EINT0_B6);
	}
	if ((eintid & PMIC_EINT1) == PMIC_EINT1) {
		while ((pmic_read(ACCDET_IRQ_STS) & ACCDET_EINT1_IRQ_B3)
			&& (accdet_timeout_ns(cur_time, ACCDET_TIME_OUT)))
			;
		pmic_write(ACCDET_IRQ_STS,
			pmic_read(ACCDET_IRQ_STS)&(~ACCDET_EINT1_IRQ_CLR_B11));
		pmic_write(AUD_TOP_INT_STATUS0, RG_INT_STATUS_ACCDET_EINT1_B7);
	}

	pr_info("%s() eint-%s IRQ-STS:[0x%x]=0x%x TOP_INT_STS:[0x%x]:0x%x\n",
		__func__,
		(eintid == PMIC_EINT0)?"0":((eintid == PMIC_EINT1)?"1":"BI"),
		ACCDET_IRQ_STS, pmic_read(ACCDET_IRQ_STS),
		AUD_TOP_INT_STATUS0, pmic_read(AUD_TOP_INT_STATUS0));

}

static void eint_debounce_set(u32 eint_id, u32 debounce)
{
	u32 reg_val;

	if (eint_id == PMIC_EINT0) {
		reg_val = pmic_read(ACCDET_EINT0_CTL);
		pmic_write(ACCDET_EINT0_CTL,
				reg_val & (~ACCDET_EINT0_DEB_CLR));

		reg_val = pmic_read(ACCDET_EINT0_CTL);
		pmic_write(ACCDET_EINT0_CTL,
			reg_val | debounce);
	} else if (eint_id == PMIC_EINT1) {
		reg_val = pmic_read(ACCDET_EINT1_CTL);
		pmic_write(ACCDET_EINT1_CTL,
				reg_val & (~ACCDET_EINT1_DEB_CLR));
		reg_val = pmic_read(ACCDET_EINT1_CTL);
		pmic_write(ACCDET_EINT1_CTL,
			reg_val | debounce);
	}
}

static u32 get_triggered_eint(void)
{
	u32 eint_ID = NO_PMIC_EINT;
	u32 irq_status = pmic_read(ACCDET_IRQ_STS);

#ifdef CONFIG_ACCDET_SUPPORT_EINT0
	if ((irq_status & ACCDET_EINT0_IRQ_B2) == ACCDET_EINT0_IRQ_B2)
		eint_ID = PMIC_EINT0;
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
	if ((irq_status & ACCDET_EINT1_IRQ_B3) == ACCDET_EINT1_IRQ_B3)
		eint_ID = PMIC_EINT1;
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
	if ((irq_status & ACCDET_EINT0_IRQ_B2) == ACCDET_EINT0_IRQ_B2)
		eint_ID |= PMIC_EINT0;
	if ((irq_status & ACCDET_EINT1_IRQ_B3) == ACCDET_EINT1_IRQ_B3)
		eint_ID |= PMIC_EINT1;
#endif
	return eint_ID;
}

static void eint_polarity_reverse(u32 eint_id)
{
	u32 reg_val = pmic_read(ACCDET_IRQ_STS);

	if (eint_id == PMIC_EINT0) {
		if (cur_eint_state == EINT_PIN_PLUG_OUT) {
			if (accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
				pmic_write(ACCDET_IRQ_STS,
					reg_val & (~ACCDET_EINT0_IRQ_POL_B14));
			else
				pmic_write(ACCDET_IRQ_STS,
					reg_val | ACCDET_EINT0_IRQ_POL_B14);
		} else {
			if (accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
				pmic_write(ACCDET_IRQ_STS,
					reg_val | ACCDET_EINT0_IRQ_POL_B14);
			else
				pmic_write(ACCDET_IRQ_STS,
					reg_val & (~ACCDET_EINT0_IRQ_POL_B14));
		}
	} else if (eint_id == PMIC_EINT1) {

		if (cur_eint_state == EINT_PIN_PLUG_OUT) {
			if (accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
				pmic_write(ACCDET_IRQ_STS,
					reg_val & (~ACCDET_EINT1_IRQ_POL_B15));
			else
				pmic_write(ACCDET_IRQ_STS,
					reg_val | ACCDET_EINT1_IRQ_POL_B15);
		} else {
			if (accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
				pmic_write(ACCDET_IRQ_STS,
					reg_val | ACCDET_EINT1_IRQ_POL_B15);
			else
				pmic_write(ACCDET_IRQ_STS,
					reg_val & (~ACCDET_EINT1_IRQ_POL_B15));
		}
	}
}
#endif

static inline void enable_accdet(u32 state_swctrl)
{
	pmic_write(ACCDET_STATE_SWCTRL,
		pmic_read(ACCDET_STATE_SWCTRL) | state_swctrl);

	/* enable ACCDET unit */
#ifndef HW_MODE_SUPPORT
	pmic_write(ACCDET_CTRL, pmic_read(ACCDET_CTRL) | ACCDET_ENABLE_B0);
#endif
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

#ifndef HW_MODE_SUPPORT
	pmic_write(ACCDET_CTRL,
		pmic_read(ACCDET_CTRL) & (~ACCDET_ENABLE_B0));
#endif
	/* clc ACCDET PWM enable to avoid power leakage */
	pmic_write(ACCDET_STATE_SWCTRL,
		pmic_read(ACCDET_STATE_SWCTRL) & (~ACCDET_PWM_EN));
	pr_info("%s done IRQ-STS[0x%x]=0x%x,STATE_SWCTRL[0x%x]=0x%x\n",
		__func__, ACCDET_IRQ_STS, pmic_read(ACCDET_IRQ_STS),
		ACCDET_STATE_SWCTRL, pmic_read(ACCDET_STATE_SWCTRL));
}

static inline void headset_plug_out(void)
{
	pr_info("accdet %s\n", __func__);
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
#ifdef CONFIG_ACCDET_EINT_IRQ
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
		enable_accdet(ACCDET_EINT0_PWM_IDLE_B11 | ACCDET_PWM_EN);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
		enable_accdet(ACCDET_EINT1_PWM_IDLE_B12 | ACCDET_PWM_EN);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
		enable_accdet(ACCDET_EINT_PWM_IDLE_B11_12 | ACCDET_PWM_EN);
#endif
#else
		enable_accdet(ACCDET_PWM_EN);
#endif
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

static void accdet_set_debounce(int state, unsigned int debounce)
{
	switch (state) {
	case accdet_state000:
		/* set ACCDET debounce value = debounce/32 ms */
		pmic_write((unsigned int)ACCDET_DEBOUNCE0, debounce);
		break;
	case accdet_state010:
		pmic_write((unsigned int)ACCDET_DEBOUNCE1, debounce);
		break;
	case accdet_state100:
		pmic_write((unsigned int)ACCDET_DEBOUNCE2, debounce);
		break;
	case accdet_state110:
		pmic_write((unsigned int)ACCDET_DEBOUNCE3, debounce);
		break;
	case accdet_auxadc:
		/* set auxadc debounce:0x42(2ms) */
		pmic_write((unsigned int)ACCDET_DEBOUNCE4, debounce);
		break;
	default:
		pr_info("%s error state:%d!\n", __func__, state);
		break;
	}
}

static inline void check_cable_type(void)
{
	u32 cur_AB;

	cur_AB = pmic_read(ACCDET_STATE_RG) >> ACCDET_STATE_MEM_IN_OFFSET;
	cur_AB = cur_AB & ACCDET_STATE_AB_MASK;
	pr_notice("accdet %s(), cur_status:%s current AB = %d\n", __func__,
		     accdet_status_str[accdet_status], cur_AB);

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
#ifdef HW_MODE_SUPPORT
#ifdef DIGITAL_FASTDISCHARGE_SUPPORT
				/* digital fast discharge bug, sw arround (2) :
				 * ACCDET_CON24[15:14]=00 and then wait xxms
				 * to set [15:14]=11b, [4]=1;
				 *  please remember to find sw arround (1)
				 */
				if (!fast_discharge) {
					pmic_write(ACCDET_HW_MODE_DFF,
						ACCDET_FAST_DISCAHRGE_REVISE);
					mdelay(20);
					pmic_write(ACCDET_HW_MODE_DFF,
						ACCDET_FAST_DISCAHRGE_EN);
					fast_discharge = true;
				}
#endif
#endif
				/* ABC=110 debounce=30ms */
				accdet_set_debounce(accdet_state110,
					cust_pwm_deb->debounce3 * 30);
			} else
				pr_info("accdet headset has been plug-out\n");
			mutex_unlock(&accdet_eint_irq_sync_mutex);
			accdet_set_debounce(accdet_state000,
				button_press_debounce);
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
		if (cur_AB == ACCDET_STATE_AB_00) {
			/*solution: resume hook switch debounce time*/
			accdet_set_debounce(accdet_state000,
				cust_pwm_deb->debounce0);
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag) {
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
				/* to avoid 01->00 framework of Headset will
				 * report press key info to Audio
				 */
				/* cable_type = HEADSET_NO_MIC; */
				/* accdet_status = HOOK_SWITCH; */
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

			/* solution: reduce hook switch debounce time to half */
			accdet_set_debounce(accdet_state000,
				button_press_debounce);
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
		pr_info("%s() Headset has been plugout. Don't set state\n",
			__func__);
	mutex_unlock(&accdet_eint_irq_sync_mutex);
	pr_info("%s() report cable_type done\n", __func__);
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
static int pmic_eint_queue_work(int eintID)
{
	int ret = 0;

	pr_info("%s() Enter. eint-%s\n", __func__,
		(eintID == PMIC_EINT0)?"0":((eintID == PMIC_EINT1)?"1":"BI"));

#ifdef CONFIG_ACCDET_SUPPORT_EINT0
	if (eintID == PMIC_EINT0) {
		if (cur_eint_state == EINT_PIN_PLUG_IN) {
			eint_debounce_set(eintID, ACCDET_EINT0_DEB_IN_256);
			accdet_set_debounce(accdet_state110,
				cust_pwm_deb->debounce3);
			cur_eint_state = EINT_PIN_PLUG_OUT;
		} else {
			eint_debounce_set(eintID, ACCDET_EINT0_DEB_OUT_012);
			cur_eint_state = EINT_PIN_PLUG_IN;

			mod_timer(&micbias_timer,
				jiffies + MICBIAS_DISABLE_TIMER);
		}
		ret = queue_work(eint_workqueue, &eint_work);
	} else
		pr_info("%s invalid EINT ID!\n", __func__);

#elif defined CONFIG_ACCDET_SUPPORT_EINT1
	if (eintID == PMIC_EINT1) {
		if (cur_eint_state == EINT_PIN_PLUG_IN) {
			eint_debounce_set(eintID, ACCDET_EINT1_DEB_IN_256);
			accdet_set_debounce(accdet_state110,
				cust_pwm_deb->debounce3);
			cur_eint_state = EINT_PIN_PLUG_OUT;
		} else {
			eint_debounce_set(eintID, ACCDET_EINT1_DEB_OUT_012);
			cur_eint_state = EINT_PIN_PLUG_IN;

			mod_timer(&micbias_timer,
				jiffies + MICBIAS_DISABLE_TIMER);
		}
		ret = queue_work(eint_workqueue, &eint_work);
	} else
		pr_info("%s invalid EINT ID!\n", __func__);

#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
	if ((eintID & PMIC_EINT0) == PMIC_EINT0) {
		if (cur_eint0_state == EINT_PIN_PLUG_IN) {
			eint_debounce_set(PMIC_EINT0,
				ACCDET_EINT0_DEB_IN_256);
			accdet_set_debounce(accdet_state110,
				cust_pwm_deb->debounce3);
			cur_eint0_state = EINT_PIN_PLUG_OUT;
		} else {
			eint_debounce_set(PMIC_EINT0,
				ACCDET_EINT0_DEB_OUT_012);
			cur_eint0_state = EINT_PIN_PLUG_IN;
		}
	}
	if ((eintID & PMIC_EINT1) == PMIC_EINT1) {
		if (cur_eint1_state == EINT_PIN_PLUG_IN) {
			eint_debounce_set(PMIC_EINT1,
				ACCDET_EINT1_DEB_IN_256);
			accdet_set_debounce(accdet_state110,
				cust_pwm_deb->debounce3);
			cur_eint1_state = EINT_PIN_PLUG_OUT;
		} else {
			eint_debounce_set(PMIC_EINT1,
				ACCDET_EINT1_DEB_OUT_012);
			cur_eint1_state = EINT_PIN_PLUG_IN;
		}
	}

	/* bi_eint trigger issued current state, may */
	if (cur_eint_state == EINT_PIN_PLUG_OUT) {
		cur_eint_state = cur_eint0_state & cur_eint1_state;
		if (cur_eint_state == EINT_PIN_PLUG_IN) {
			mod_timer(&micbias_timer,
				jiffies + MICBIAS_DISABLE_TIMER);
			ret = queue_work(eint_workqueue, &eint_work);
		} else
			pr_info("%s wait eint.now:eint0=%d;eint1=%d\n",
				__func__, cur_eint0_state, cur_eint1_state);
	} else if (cur_eint_state == EINT_PIN_PLUG_IN) {
		if ((cur_eint0_state|cur_eint1_state) == EINT_PIN_PLUG_OUT) {
			clear_accdet_eint_check(PMIC_EINT0);
			clear_accdet_eint_check(PMIC_EINT1);
		} else if ((cur_eint0_state & cur_eint1_state) ==
					EINT_PIN_PLUG_OUT) {
			cur_eint_state = EINT_PIN_PLUG_OUT;
			ret = queue_work(eint_workqueue, &eint_work);
		} else
			pr_info("%s wait eint.now:eint0=%d;eint1=%d\n",
				__func__, cur_eint0_state, cur_eint1_state);
	}
#endif

	return ret;
}

#if defined(CONFIG_MOISTURE_INT_SUPPORT) || defined(CONFIG_MOISTURE_EXT_SUPPORT)
static u32 moisture_detect(void)
{
	u32 moisture_vol = 0;
	u32 tmp_1, tmp_2, tmp_3;

	tmp_1 = pmic_read(ACCDET_RSV);
	tmp_2 = pmic_read(AUDENC_ANA_CON10);
	tmp_3 = pmic_read(AUDENC_ANA_CON11);

	/* Disable ACCDET to AUXADC */
	pmic_write(AUDENC_ANA_CON11, pmic_read(AUDENC_ANA_CON11) & 0x1FFF);
	pmic_write(ACCDET_RSV, pmic_read(ACCDET_RSV) & 0xFBFF);
	pmic_write(ACCDET_RSV, pmic_read(ACCDET_RSV) | 0x800);

	/* Enable moisture detection, set 219A bit[13] = 1*/
	pmic_write(AUDENC_ANA_CON10, pmic_read(AUDENC_ANA_CON10) | 0x2000);

	/* select PAD_HP_EINT for moisture detection, set 219A bit[14] = 0*/
	pmic_write(AUDENC_ANA_CON10, pmic_read(AUDENC_ANA_CON10) & 0xBFFF);

#ifdef CONFIG_MOISTURE_INT_SUPPORT
	/* select VTH to 2v and 500k, use internal resitance,
	 * 219C bit[10][11][12] = 1
	 */
	pmic_write(AUDENC_ANA_CON11, pmic_read(AUDENC_ANA_CON11) | 0x1C00);
#endif
#ifdef CONFIG_MOISTURE_EXT_SUPPORT
	/* select VTH to 2v and 500k, use external resitance
	 * set 219C bit[10] = 1, bit[11] [12]= 0
	 */
	pmic_write(AUDENC_ANA_CON11, pmic_read(AUDENC_ANA_CON11) & 0xE7FF);
	pmic_write(AUDENC_ANA_CON11, pmic_read(AUDENC_ANA_CON11) | 0x0400);
#endif
	moisture_vol = accdet_get_auxadc(0);
	pr_info("%s accdet Moisture Read Auxadc=%d\n", __func__, moisture_vol);

	/* reverse register setting after reading moisture voltage */
	pmic_write(ACCDET_RSV, tmp_1);
	pmic_write(AUDENC_ANA_CON10, tmp_2);
	pmic_write(AUDENC_ANA_CON11, tmp_3);

	return moisture_vol;

}
#endif
#endif

static void accdet_irq_handle(void)
{
	u32 eintID = 0;
	u32 irq_status;
#if defined(CONFIG_MOISTURE_INT_SUPPORT) || defined(CONFIG_MOISTURE_EXT_SUPPORT)
	unsigned int moisture_vol = 0;
#endif
#ifdef CONFIG_ACCDET_EINT_IRQ
	eintID = get_triggered_eint();
#endif
	irq_status = pmic_read(ACCDET_IRQ_STS);

	if ((irq_status & ACCDET_IRQ_B0) && (eintID == 0)) {
		pr_info("%s() IRQ_STS = 0x%x, IRQ triggered\n", __func__,
			irq_status);
		clear_accdet_int();
		accdet_queue_work();
		clear_accdet_int_check();
#ifdef CONFIG_ACCDET_EINT_IRQ
	} else if (eintID != NO_PMIC_EINT) {
		pr_info("%s() IRQ_STS = 0x%x, pmic eint-%s triggered.\n",
		__func__, irq_status,
		(eintID == PMIC_EINT0)?"0":((eintID == PMIC_EINT1)?"1":"BI"));

#if defined(CONFIG_MOISTURE_INT_SUPPORT) || defined(CONFIG_MOISTURE_EXT_SUPPORT)
		if (cur_eint_state == EINT_PIN_MOISTURE_DETECED) {
			pr_info("%s Moisture plug out detectecd\n", __func__);
			eint_polarity_reverse(eintID);
			cur_eint_state = EINT_PIN_PLUG_OUT;
			clear_accdet_eint(eintID);
			clear_accdet_eint_check(eintID);
			return;
		}

		if (cur_eint_state == EINT_PIN_PLUG_OUT) {
			pr_info("%s now check moisture\n", __func__);
			moisture_vol = moisture_detect();
			if (moisture_vol > moisture_vm) {
				eint_polarity_reverse(eintID);
				cur_eint_state = EINT_PIN_MOISTURE_DETECED;
				clear_accdet_eint(eintID);
				clear_accdet_eint_check(eintID);
				pr_info("%s Moisture plug in detectecd!\n",
					__func__);
				return;
			}
			pr_info("%s check moisture done,not water.\n",
				__func__);
		}
#endif
		eint_polarity_reverse(eintID);
		clear_accdet_eint(eintID);
		clear_accdet_eint_check(eintID);
		pmic_eint_queue_work(eintID);
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
	pr_info("%s() enter\n", __func__);
	accdet_irq_handle();
	pr_info("%s() exit\n", __func__);
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

static inline int ext_eint_setup(struct platform_device *platform_device)
{
	int ret = 0;
	u32 ints[4] = { 0 };
	struct device_node *node = NULL;
	struct pinctrl_state *pins_default = NULL;

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
	if (ret < 0) {
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
	int pwm_deb[8];
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

#if defined(CONFIG_MOISTURE_EXT_SUPPORT) || defined(CONFIG_MOISTURE_INT_SUPPORT)
	of_property_read_u32(node, "moisture-water-r", &water_r);
#ifdef CONFIG_MOISTURE_EXT_SUPPORT
	of_property_read_u32(node, "moisture-external-r", &moisture_ext_r);
	pr_info("accdet Moisture_EXT support water_r=%d, moisture_ext_r=%d\n",
	     water_r, moisture_ext_r);
#endif
#ifdef CONFIG_MOISTURE_INT_SUPPORT
	of_property_read_u32(node, "moisture-internal-r", &moisture_int_r);
	pr_info("accdet Moisture_INT support water_r=%d, moisture_int_r=%d\n",
	     water_r, moisture_int_r);
#endif
#endif
	of_property_read_u32(node, "accdet-mic-vol", &accdet_dts.mic_vol);
	of_property_read_u32(node, "accdet-plugout-debounce",
			&accdet_dts.plugout_deb);
	of_property_read_u32(node, "accdet-mic-mode", &accdet_dts.mic_mode);
	of_property_read_u32(node, "headset-eint-level-pol",
			&accdet_dts.eint_pol);

	pr_info("accdet mic_vol=%d, plugout_deb=%d mic_mode=%d eint_pol=%d\n",
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
	/* debounce8(auxadc debounce) is default, needn't get from dts */
	if (!ret)
		memcpy(&accdet_dts.pwm_deb, pwm_deb, sizeof(pwm_deb));
	else
		pr_info("accdet get pwm-debounce setting fail\n");

	/* for discharge:0xB00 about 86ms */
	button_press_debounce = (accdet_dts.pwm_deb.debounce0 >> 1);
	cust_pwm_deb = &accdet_dts.pwm_deb;
	pr_info("accdet pwm_width=0x%x, thresh=0x%x, fall=0x%x, rise=0x%x\n",
	     cust_pwm_deb->pwm_width, cust_pwm_deb->pwm_thresh,
	     cust_pwm_deb->fall_delay, cust_pwm_deb->rise_delay);
	pr_info("accdet deb0=0x%x, deb1=0x%x, deb3=0x%x, deb4=0x%x\n",
	     cust_pwm_deb->debounce0, cust_pwm_deb->debounce1,
	     cust_pwm_deb->debounce3, cust_pwm_deb->debounce4);

	return 0;
}

static void accdet_init_once(void)
{
	unsigned int reg = 0;

	/* reset the accdet unit */
	pmic_write(AUD_TOP_RST_CON0, RG_ACCDET_RST_B1);
	pmic_write(AUD_TOP_RST_CON0,
		pmic_read(AUD_TOP_RST_CON0)&(~RG_ACCDET_RST_B1));

	/* open top accdet interrupt */
	pmic_write(AUD_TOP_INT_CON0_SET, RG_INT_EN_ACCDET_B5);

	/* init pwm frequency, duty & rise/falling delay */
	pmic_write(ACCDET_PWM_WIDTH, REGISTER_VAL(cust_pwm_deb->pwm_width));
	pmic_write(ACCDET_PWM_THRESH, REGISTER_VAL(cust_pwm_deb->pwm_thresh));
	pmic_write(ACCDET_EN_DELAY_NUM,
		  (cust_pwm_deb->fall_delay << 15 | cust_pwm_deb->rise_delay));

	/* config micbias voltage */
	reg = pmic_read(AUDENC_ANA_CON10);
	pmic_write(AUDENC_ANA_CON10,
		reg | (accdet_dts.mic_vol<<4) | RG_AUD_MICBIAS1_LOWP_EN);

	/* mic mode setting */
	reg = pmic_read(AUDENC_ANA_CON11);
	/* ACC mode*/
	if (accdet_dts.mic_mode == HEADSET_MODE_1)
		pmic_write(AUDENC_ANA_CON11,
			reg | RG_ACCDET_MODE_ANA11_MODE1);
	/* Low cost mode without internal bias*/
	else if (accdet_dts.mic_mode == HEADSET_MODE_2)
		pmic_write(AUDENC_ANA_CON11,
			reg | RG_ACCDET_MODE_ANA11_MODE2);
	/* Low cost mode with internal bias, bit8 = 1 to use internal bias */
	else if (accdet_dts.mic_mode == HEADSET_MODE_6) {
		pmic_write(AUDENC_ANA_CON11,
			reg | RG_ACCDET_MODE_ANA11_MODE6);
		pmic_write(AUDENC_ANA_CON10,
			pmic_read(AUDENC_ANA_CON10) | RG_AUDMICBIAS1_DCSW1PEN);
	}

	/* sw trigger auxadc, disable auxadc auto sample */
	/* pmic_write(AUXADC_ACCDET,
	 *	pmic_read(AUXADC_ACCDET) | AUXADC_ACCDET_AUTO_SPL_EN);
	 */

#ifdef ANALOG_FASTDISCHARGE_SUPPORT
	reg = pmic_read(AUDENC_ANA_CON6) | RG_AUDSPARE_FSTDSCHRG_IMPR_EN |
		RG_AUDSPARE_FSTDSCHRG_ANALOG_DIR_EN;
	pmic_write(AUDENC_ANA_CON6, reg);
#endif

	/* hw mode config , disable accdet */
#ifdef HW_MODE_SUPPORT
	pmic_write(ACCDET_CTRL, pmic_read(ACCDET_CTRL)&(~ACCDET_ENABLE_B0));

#ifdef CONFIG_ACCDET_EINT_IRQ
	reg = pmic_read(ACCDET_HW_MODE_DFF);
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
	reg |= ACCDET_HWEN_SEL_0 | ACCDET_HWMODE_SEL |
		ACCDET_EINT_DEB_OUT_DFF | ACCDET_EINIT_REVERSE;

#elif defined CONFIG_ACCDET_SUPPORT_EINT1
	reg |= ACCDET_HWEN_SEL_1 | ACCDET_HWMODE_SEL |
		ACCDET_EINT_DEB_OUT_DFF | ACCDET_EINIT_REVERSE;
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
	reg |= ACCDET_HWEN_SEL_0_AND_1 | ACCDET_HWMODE_SEL |
		ACCDET_EINT_DEB_OUT_DFF | ACCDET_EINIT_REVERSE;
#endif
	pmic_write(ACCDET_HW_MODE_DFF, reg);
#endif
#else
	/* sw mode, */
	reg = pmic_read(ACCDET_HW_MODE_DFF);
	pmic_write(ACCDET_HW_MODE_DFF,
		(reg & (~ACCDET_HWMODE_SEL))|ACCDET_FAST_DISCAHRGE);
#endif

#ifdef CONFIG_ACCDET_EINT_IRQ
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
	/* set eint0 pwm width&thresh, and enable eint0 PWM */
	reg = pmic_read(ACCDET_EINT0_CTL);
	reg &= ~(ACCDET_EINT0_PWM_THRSH_MASK | ACCDET_EINT0_PWM_WIDTH_MASK);
	pmic_write(ACCDET_EINT0_CTL, reg);

	reg = pmic_read(ACCDET_EINT0_CTL);
	pmic_write(ACCDET_EINT0_CTL,
		reg | ACCDET_EINT0_PWM_THRSH | ACCDET_EINT0_PWM_WIDTH);

	reg = pmic_read(ACCDET_STATE_SWCTRL);
	pmic_write(ACCDET_STATE_SWCTRL,
		reg | ACCDET_EINT0_PWM_EN_B3 | ACCDET_EINT0_PWM_IDLE_B11);

	/* open top interrupt eint0 */
	pmic_write(AUD_TOP_INT_CON0_SET,
		pmic_read(AUD_TOP_INT_CON0_SET) | RG_INT_EN_ACCDET_EINT0_B6);

	/* open accdet interrupt eint0 */
	pwrap_write(ACCDET_CTRL, pmic_read(ACCDET_CTRL) | ACCDET_EINT0_EN_B2);

#elif defined CONFIG_ACCDET_SUPPORT_EINT1
	/* set eint0 pwm width&thresh, and enable eint0 PWM */
	reg = pmic_read(ACCDET_EINT1_CTL);
	reg &= ~(ACCDET_EINT1_PWM_THRSH_MASK | ACCDET_EINT1_PWM_WIDTH_MASK);
	pmic_write(ACCDET_EINT1_CTL, reg);

	reg = pmic_read(ACCDET_EINT1_CTL);
	pmic_write(ACCDET_EINT1_CTL,
		reg | ACCDET_EINT1_PWM_THRSH | ACCDET_EINT1_PWM_WIDTH);

	reg = pmic_read(ACCDET_STATE_SWCTRL);
	pmic_write(ACCDET_STATE_SWCTRL,
		reg | ACCDET_EINT1_PWM_EN_B4 | ACCDET_EINT1_PWM_IDLE_B12);

	/* open top interrupt eint1 */
	pmic_write(AUD_TOP_INT_CON0_SET,
		pmic_read(AUD_TOP_INT_CON0_SET) | RG_INT_EN_ACCDET_EINT1_B7);

	/* open accdet interrupt eint1 */
	pwrap_write(ACCDET_CTRL,
		pmic_read(ACCDET_CTRL) | ACCDET_EINT1_EN_B4);

#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
	reg = pmic_read(ACCDET_EINT0_CTL);
	reg &= ~(ACCDET_EINT0_PWM_THRSH_MASK | ACCDET_EINT0_PWM_WIDTH_MASK);
	pmic_write(ACCDET_EINT0_CTL, reg);
	reg = pmic_read(ACCDET_EINT0_CTL);
	pmic_write(ACCDET_EINT0_CTL,
		reg | ACCDET_EINT0_PWM_THRSH | ACCDET_EINT0_PWM_WIDTH);

	reg = pmic_read(ACCDET_EINT1_CTL);
	reg &= ~(ACCDET_EINT1_PWM_THRSH_MASK | ACCDET_EINT1_PWM_WIDTH_MASK);
	pmic_write(ACCDET_EINT1_CTL, reg);
	reg = pmic_read(ACCDET_EINT1_CTL);
	pmic_write(ACCDET_EINT1_CTL,
		reg | ACCDET_EINT1_PWM_THRSH | ACCDET_EINT1_PWM_WIDTH);

	reg = pmic_read(ACCDET_STATE_SWCTRL);
	pmic_write(ACCDET_STATE_SWCTRL,
		reg | ACCDET_EINT_PWM_IDLE_B11_12 | ACCDET_EINT_PWM_EN_B3_4);

	/* open top interrupt eint0 & eint1 */
	pmic_write(AUD_TOP_INT_CON0_SET,
		pmic_read(AUD_TOP_INT_CON0_SET) | RG_INT_EN_ACCDET_EINT_B6_7);

	/* open accdet interrupt eint0&eint1 */
	pwrap_write(ACCDET_CTRL,
		pmic_read(ACCDET_CTRL) | ACCDET_EINT_EN_B2_4);
#endif
#endif
	pr_info("%s() done.\n", __func__);
}

static inline void accdet_init(void)
{
	/* add new of DE for fix icon cann't appear */
	/* set and clear initial bit every eint interrutp */
	pmic_write(ACCDET_CTRL, pmic_read(ACCDET_CTRL)|ACCDET_SEQ_INIT_EN_B1);
	mdelay(2);
	pmic_write(ACCDET_CTRL,
			pmic_read(ACCDET_CTRL)&(~ACCDET_SEQ_INIT_EN_B1));
	mdelay(1);

	/* init the debounce time (debounce/32768)sec */
	accdet_set_debounce(accdet_state000, cust_pwm_deb->debounce0);
	accdet_set_debounce(accdet_state010, cust_pwm_deb->debounce1);
	accdet_set_debounce(accdet_state110, cust_pwm_deb->debounce3);
	/* auxadc:2ms */
	accdet_set_debounce(accdet_auxadc, cust_pwm_deb->debounce4);

#ifdef HW_MODE_SUPPORT
#ifdef DIGITAL_FASTDISCHARGE_SUPPORT
	/* workround for HW fast discharge, first disabel fast discharge */
	pmic_write(ACCDET_HW_MODE_DFF, ACCDET_FAST_DISCAHRGE_DIS);
	fast_discharge = false;
#endif
#endif
	pr_info("%s() done.\n", __func__);
}

/* late init for DC trim, and this API  Will be called by audio */
void accdet_late_init(unsigned long data)
{
	pr_info("%s()  now init accdet!\n", __func__);
	if (atomic_cmpxchg(&accdet_first, 1, 0)) {
		del_timer_sync(&accdet_init_timer);
		accdet_init();
		/* just need run once */
		accdet_init_once();
	} else
		pr_info("%s err:inited already done/get dts fail\n", __func__);
}
EXPORT_SYMBOL(accdet_late_init);

static void delay_init_timerhandler(unsigned long data)
{
	pr_info("%s()  now init accdet!\n", __func__);
	if (atomic_cmpxchg(&accdet_first, 1, 0)) {
		accdet_init();
		accdet_init_once();
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
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
	pmic_register_interrupt_callback(INT_ACCDET_EINT0,
			accdet_eint_handler);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
	pmic_register_interrupt_callback(INT_ACCDET_EINT1,
			accdet_eint_handler);
#elif defined CONFIG_ACCDET_BI_EINT
	pmic_register_interrupt_callback(INT_ACCDET_EINT0,
			accdet_eint_handler);
	pmic_register_interrupt_callback(INT_ACCDET_EINT1,
			accdet_eint_handler);
#endif
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

	accdet_get_efuse();

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

