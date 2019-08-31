// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Author: Argus Lin <argus.lin@mediatek.com>
 */

#include "accdet.h"
#if PMIC_ACCDET_KERNEL
#ifdef CONFIG_ACCDET_EINT
#include <linux/of_gpio.h>
#endif
#include <linux/timer.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/sched/clock.h>
#include <linux/timer.h>
#include <linux/irq.h>
#include "reg_accdet.h"
#if defined CONFIG_MTK_PMIC_NEW_ARCH
#include <upmu_common.h>
#include <mtk_auxadc_intf.h>
#include <mach/mtk_pmic.h>
#include <mach/upmu_hw.h>
#endif
#include <mach/mtk_pmic_wrap.h>
#ifdef CONFIG_MTK_PMIC_WRAP
#include <linux/regmap.h>
#include <linux/soc/mediatek/pmic_wrap.h>
#endif
#else
#include "string.h"
#include "reg_base.H"
#include "accdet_hw.h"
#include "accdet_sw.h"
#include "common.h"
#include "intrCtrl.h"
#include "api.h"
#ifdef MTKDRV_GPIO
#include "gpio.h"
#endif
#include "pmic_auxadc.h"
#endif /* end of #if PMIC_ACCDET_KERNEL */

/********************grobal variable definitions******************/
#if PMIC_ACCDET_CTP
#define CONFIG_ACCDET_EINT_IRQ
#define CONFIG_ACCDET_SUPPORT_EINT0

#define pr_info dbg_print
#define pr_debug dbg_print
#define pr_notice dbg_print
#define mdelay accdet_delay
#define udelay accdet_delay
#define atomic_t int
#define dev_t int
#define DEFINE_MUTEX(a)
#define mutex_lock(a)
#define mutex_unlock(a)
#define mod_timer(a, b)
#define __pm_wakeup_event(a, b)
#define del_timer_sync(a)
#define __pm_stay_awake(a)
#define __pm_relax(a)

#endif /* end of #if PMIC_ACCDET_CTP */
#define REGISTER_VAL(x)	(x - 1)

/* for accdet_read_audio_res, less than 5k ohm, return -1 , otherwise ret 0 */
#define RET_LT_5K		(-1)
#define RET_GT_5K		(0)

/* Used to let accdet know if the pin has been fully plugged-in */
#define EINT_PIN_PLUG_IN        (1)
#define EINT_PIN_PLUG_OUT       (0)
#define EINT_PIN_MOISTURE_DETECTED (2)

#ifdef CONFIG_ACCDET_EINT_IRQ
enum pmic_eint_ID {
	NO_PMIC_EINT = 0,
	PMIC_EINT0 = 1,
	PMIC_EINT1 = 2,
	PMIC_BIEINT = 3,
};
#endif

/* accdet_status_str: to record current 'accdet_status' by string,
 * mapping to  'enum accdet_status'
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
static int s_button_status;

/* accdet input device to report cable type and key event */
static struct input_dev *accdet_input_dev;

#if PMIC_ACCDET_KERNEL
#ifdef CONFIG_MTK_PMIC_WRAP
static struct regmap *accdet_regmap;
#endif
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
static void dis_micbias_timerhandler(struct timer_list *t);
static bool dis_micbias_done;
#ifdef CONFIG_ACCDET_EINT_IRQ
static u32 gmoistureID;
#endif
static bool accdet_thing_in_flag;
static char accdet_log_buf[1536];

/* accdet_init_timer:  init accdet if audio doesn't call to accdet for DC trim
 * timeout: 10 seconds
 * timerHandler: delay_init_timerhandler()
 */
#define ACCDET_INIT_WAIT_TIMER   (10 * HZ)
static struct timer_list  accdet_init_timer;
static void delay_init_timerhandler(struct timer_list *t);
static struct wakeup_source *accdet_irq_lock;
static struct wakeup_source *accdet_timer_lock;
static DEFINE_MUTEX(accdet_eint_irq_sync_mutex);
#endif /* end of #if PMIC_ACCDET_KERNEL */
/* accdet customized info by dts*/
static struct head_dts_data accdet_dts;
struct pwm_deb_settings *cust_pwm_deb;

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

#ifdef CONFIG_ACCDET_EINT
static u32 accdet_eint_type = IRQ_TYPE_LEVEL_LOW;
#endif
static u32 button_press_debounce = 0x400;
static u32 button_press_debounce_01 = 0x800;

static atomic_t accdet_first;

/* SW mode only, moisture vm, resister declaration */
static unsigned int moisture_vm = 50; /* TBD */
/* moisture resister ohm */
static int water_r = 10000;
/* accdet Moisture detect */
/* unit is mv */
static int moisture_vdd_offset;
static int moisture_offset;
/* unit is ohm */
static int moisture_eint_offset;
/* unit is ohm */
static int moisture_int_r = 47000;
/* unit is ohm */
static int moisture_ext_r = 470000;

static bool debug_thread_en;
static bool dump_reg;
static struct task_struct *thread;

/*******************local function declaration******************/
#ifdef CONFIG_ACCDET_EINT_IRQ
static u32 config_moisture_detect_1_0(void);
static u32 config_moisture_detect_1_1(void);
static u32 config_moisture_detect_2_1(void);
static u32 config_moisture_detect_2_1_1(void);
static u32 get_moisture_det_en(void);
static u32 get_moisture_sw_auxadc_check(void);
static u32 adjust_eint_analog_setting(u32 eintID);
static u32 adjust_moisture_analog_setting(u32 eintID);
static u32 adjust_moisture_setting(u32 moistureID, u32 eintID);
static u32 adjust_eint_setting(u32 moistureID, u32 eintID);
static void recover_eint_analog_setting(void);
static void recover_eint_digital_setting(void);
static void recover_eint_setting(u32 moistureID);
static void recover_moisture_setting(u32 moistureID);
static u32 get_triggered_eint(void);
static void config_digital_moisture_init_by_mode(void);
static void config_analog_moisture_init_by_mode(void);
static void config_eint_init_by_mode(void);
#endif
static void send_accdet_status_event(u32 cable_type, u32 status);
static void accdet_init_once(void);
static inline void accdet_init(void);
static void accdet_init_debounce(void);
static void mini_dump_register(void);
static void accdet_modify_vref_volt_self(void);
/*******************global function declaration*****************/

#if !defined CONFIG_MTK_PMIC_NEW_ARCH
enum PMIC_FAKE_IRQ_ENUM {
	INT_ACCDET,
	INT_ACCDET_EINT0,
	INT_ACCDET_EINT1,
};

void pmic_register_interrupt_callback(unsigned int intNo,
	void(EINT_FUNC_PTR)(void))
{
}

void pmic_enable_interrupt(enum PMIC_FAKE_IRQ_ENUM intNo,
	unsigned int en, char *str)
{
}

unsigned int pmic_Read_Efuse_HPOffset(int i)
{
	return 0;
}
#endif

#if !defined CONFIG_MTK_PMIC_WRAP && !defined CONFIG_MTK_PMIC_WRAP_HAL
signed int pwrap_read(unsigned int adr, unsigned int *rdata)
{
	return 0;
}

signed int pwrap_write(unsigned int adr, unsigned int wdata)
{
	return 0;
}
#endif

inline u32 pmic_read(u32 addr)
{
	u32 val = 0;
#ifdef CONFIG_MTK_PMIC_WRAP
	if (accdet_regmap)
		regmap_read(accdet_regmap, addr, &val);
	else
		pr_notice("%s %d Error.\n", __func__, __LINE__);
#else
	pwrap_read(addr, &val);
#endif

	return val;
}

inline u32 pmic_read_mbit(u32 addr, u32 shift, u32 mask)
{
	u32 val = 0;

#ifdef CONFIG_MTK_PMIC_WRAP
	if (accdet_regmap)
		regmap_read(accdet_regmap, addr, &val);
	else
		pr_notice("%s %d Error.\n", __func__, __LINE__);
#else
	pwrap_read(addr, &val);
#endif
#if PMIC_ACCDET_DEBUG
	if (dump_reg) {
		pr_debug("%s [0x%x]=[0x%x], shift[0x%x], mask[0x%x]\n",
			__func__, addr, ((val>>shift) & mask), shift, mask);
	}
#endif
	return ((val>>shift) & mask);
}

inline void pmic_write(u32 addr, u32 wdata)
{
#ifdef CONFIG_MTK_PMIC_WRAP
	if (accdet_regmap)
		regmap_write(accdet_regmap, addr, wdata);
	else
		pr_notice("%s %d Error.\n", __func__, __LINE__);
#else
	pwrap_write(addr, wdata);
#endif
#if PMIC_ACCDET_DEBUG
	if (dump_reg)
		pr_debug("%s [0x%x]=[0x%x]\n", __func__, addr, wdata);
#endif
}

inline void pmic_write_mset(u32 addr, u32 shift, u32 mask, u32 data)
{
	u32 pmic_reg = 0;

	pmic_reg = pmic_read(addr);
	pmic_reg &= ~(mask<<shift);
	pmic_write(addr, pmic_reg | (data<<shift));
#if PMIC_ACCDET_DEBUG
if (dump_reg) {
	pr_debug("%s [0x%x]=[0x%x], shift[0x%x], mask[0x%x], data[0x%x]\n",
		__func__, addr, pmic_read(addr), shift, mask, data);
}
#endif
}

inline void pmic_write_set(u32 addr, u32 shift)
{
	pmic_write(addr, pmic_read(addr) | (1<<shift));
#if PMIC_ACCDET_DEBUG
	if (dump_reg) {
		pr_debug("%s [0x%x]=[0x%x], shift[0x%x]\n",
			__func__, addr, pmic_read(addr), shift);
	}
#endif
}

inline void pmic_write_mclr(u32 addr, u32 shift, u32 data)
{
	pmic_write(addr, pmic_read(addr) & ~(data<<shift));
#if PMIC_ACCDET_DEBUG
	if (dump_reg) {
		pr_debug("%s [0x%x]=[0x%x], shift[0x%x], data[0x%x]\n",
		__func__, addr, pmic_read(addr), shift, data);
	}
#endif
}

inline void pmic_write_clr(u32 addr, u32 shift)
{
	pmic_write(addr, pmic_read(addr) & ~(1<<shift));
#if PMIC_ACCDET_DEBUG
	if (dump_reg) {
		pr_debug("%s [0x%x]=[0x%x], shift[0x%x]\n",
			__func__, addr, pmic_read(addr), shift);
	}
#endif
}

static void mini_dump_register(void)
{
	int addr = 0, end_addr = 0, idx = 0, log_size = 0;

	log_size +=
	sprintf(accdet_log_buf,
		"(0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x",
		PMIC_ACCDET_SW_EN_ADDR,
		pmic_read(PMIC_ACCDET_SW_EN_ADDR),
		PMIC_ACCDET_CMP_PWM_EN_ADDR,
		pmic_read(PMIC_ACCDET_CMP_PWM_EN_ADDR),
		PMIC_ACCDET_IRQ_ADDR,
		pmic_read(PMIC_ACCDET_IRQ_ADDR),
		PMIC_ACCDET_DA_STABLE_ADDR,
		pmic_read(PMIC_ACCDET_DA_STABLE_ADDR));
	log_size += sprintf(accdet_log_buf + log_size,
		"(0x%x)=0x%x (0x%x)=0x%x\naccdet (0x%x)=0x%x (0x%x)=0x%x",
		PMIC_ACCDET_HWMODE_EN_ADDR,
		pmic_read(PMIC_ACCDET_HWMODE_EN_ADDR),
		PMIC_ACCDET_CMPEN_SEL_ADDR,
		pmic_read(PMIC_ACCDET_CMPEN_SEL_ADDR),
		PMIC_ACCDET_CMPEN_SW_ADDR,
		pmic_read(PMIC_ACCDET_CMPEN_SW_ADDR),
		PMIC_AD_AUDACCDETCMPOB_ADDR,
		pmic_read(PMIC_AD_AUDACCDETCMPOB_ADDR));
	log_size += sprintf(accdet_log_buf + log_size,
		"(0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x\n",
		PMIC_AD_EINT0CMPMOUT_ADDR,
		pmic_read(PMIC_AD_EINT0CMPMOUT_ADDR),
		PMIC_AD_EINT0INVOUT_ADDR,
		pmic_read(PMIC_AD_EINT0INVOUT_ADDR),
		PMIC_ACCDET_EN_ADDR,
		pmic_read(PMIC_ACCDET_EN_ADDR),
		PMIC_AD_AUDACCDETCMPOB_ADDR,
		pmic_read(PMIC_AD_AUDACCDETCMPOB_ADDR));
	end_addr = PMIC_RG_ACCDETSPARE_ADDR;
	for (addr = PMIC_RG_AUDPWDBMICBIAS0_ADDR; addr <= end_addr; addr += 8) {
		idx = addr;
		log_size += sprintf(accdet_log_buf + log_size,
			"accdet (0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x\n",
			idx, pmic_read(idx),
			idx+2, pmic_read(idx+2),
			idx+4, pmic_read(idx+4),
			idx+6, pmic_read(idx+6));
	}
	pr_info("\naccdet %s %d", accdet_log_buf, log_size);
}

static void dump_register(void)
{
	int addr = 0, st_addr = 0, end_addr = 0, idx = 0;

	if (dump_reg) {
#ifdef CONFIG_ACCDET_EINT_IRQ
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
		pr_info("Accdet EINT0 support,MODE_%d regs:\n",
				accdet_dts.mic_mode);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
		pr_info("Accdet EINT1 support,MODE_%d regs:\n",
				accdet_dts.mic_mode);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
		pr_info("Accdet BIEINT support,MODE_%d regs:\n",
				accdet_dts.mic_mode);
#else
		pr_info("ACCDET_EINT_IRQ:NO EINT configed.Error!!\n");
#endif
#elif defined CONFIG_ACCDET_EINT
		pr_info("Accdet EINT,MODE_%d regs:\n",
				accdet_dts.mic_mode);
#endif

		pr_info("ACCDET_RG\n");
		st_addr = PMIC_ACCDET_AUXADC_SEL_ADDR;
		end_addr = PMIC_ACCDET_MON_FLAG_EN_ADDR;
		for (addr = st_addr; addr <= end_addr; addr += 8) {
			idx = addr;
			pr_info("(0x%x)=0x%x (0x%x)=0x%x ",
				idx, pmic_read(idx),
				idx+2, pmic_read(idx+2));
			pr_info("(0x%x)=0x%x (0x%x)=0x%x\n",
				idx+4, pmic_read(idx+4),
				idx+6, pmic_read(idx+6));
		}
		pr_info("AUDDEC_ANA_RG\n");
		st_addr = PMIC_RG_AUDPREAMPLON_ADDR;
		end_addr = PMIC_RG_CLKSQ_EN_ADDR;
		for (addr = st_addr; addr <= end_addr; addr += 8) {
			idx = addr;
			pr_info("(0x%x)=0x%x (0x%x)=0x%x ",
				idx, pmic_read(idx),
				idx+2, pmic_read(idx+2));
			pr_info("(0x%x)=0x%x (0x%x)=0x%x\n",
				idx+4, pmic_read(idx+4),
				idx+6, pmic_read(idx+6));
		}

		pr_info("(0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x\n",
			PMIC_RG_RTC32K_CK_PDN_ADDR,
			pmic_read(PMIC_RG_RTC32K_CK_PDN_ADDR),
			PMIC_RG_ACCDET_CK_PDN_ADDR,
			pmic_read(PMIC_RG_ACCDET_CK_PDN_ADDR),
			PMIC_RG_ACCDET_RST_ADDR,
			pmic_read(PMIC_RG_ACCDET_RST_ADDR),
			PMIC_RG_INT_EN_ACCDET_ADDR,
			pmic_read(PMIC_RG_INT_EN_ACCDET_ADDR));
		pr_info("(0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x\n",
			PMIC_RG_INT_MASK_ACCDET_ADDR,
			pmic_read(PMIC_RG_INT_MASK_ACCDET_ADDR),
			PMIC_RG_INT_STATUS_ACCDET_ADDR,
			pmic_read(PMIC_RG_INT_STATUS_ACCDET_ADDR),
			PMIC_RG_AUDPWDBMICBIAS1_ADDR,
			pmic_read(PMIC_RG_AUDPWDBMICBIAS1_ADDR),
			PMIC_RG_AUDACCDETMICBIAS0PULLLOW_ADDR,
			pmic_read(PMIC_RG_AUDACCDETMICBIAS0PULLLOW_ADDR));
		pr_info("(0x%x)=0x%x (0x%x)=0x%x\n",
			PMIC_AUXADC_RQST_CH0_ADDR,
			pmic_read(PMIC_AUXADC_RQST_CH0_ADDR),
			PMIC_AUXADC_ACCDET_AUTO_SPL_ADDR,
			pmic_read(PMIC_AUXADC_ACCDET_AUTO_SPL_ADDR));
		pr_info("(0x%x)=0x%x\n", PMIC_RG_HPLOUTPUTSTBENH_VAUDP32_ADDR,
			pmic_read(PMIC_RG_HPLOUTPUTSTBENH_VAUDP32_ADDR));

		pr_info("accdet_dts:deb0=0x%x,deb1=0x%x,deb3=0x%x,deb4=0x%x\n",
			 cust_pwm_deb->debounce0, cust_pwm_deb->debounce1,
			 cust_pwm_deb->debounce3, cust_pwm_deb->debounce4);
	} else
		mini_dump_register();
}


#if PMIC_ACCDET_KERNEL
static void cat_register(char *buf)
{
	int addr = 0, st_addr = 0, end_addr = 0, idx = 0;

	dump_reg = true;
	dump_register();
	dump_reg = false;
	sprintf(accdet_log_buf, "ACCDET_RG\n");
	strncat(buf, accdet_log_buf, strlen(accdet_log_buf));
	st_addr = PMIC_ACCDET_AUXADC_SEL_ADDR;
	end_addr = PMIC_ACCDET_MON_FLAG_EN_ADDR;
	for (addr = st_addr; addr <= end_addr; addr += 8) {
		idx = addr;
		sprintf(accdet_log_buf,
			"(0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x\n",
			idx, pmic_read(idx),
			idx+2, pmic_read(idx+2),
			idx+4, pmic_read(idx+4),
			idx+6, pmic_read(idx+6));
		strncat(buf, accdet_log_buf, strlen(accdet_log_buf));
	}
	sprintf(accdet_log_buf, "AUDDEC_ANA_RG\n");
	strncat(buf, accdet_log_buf, strlen(accdet_log_buf));
	st_addr = PMIC_RG_AUDPREAMPLON_ADDR;
	end_addr = PMIC_RG_CLKSQ_EN_ADDR;
	for (addr = st_addr; addr <= end_addr; addr += 8) {
		idx = addr;
		sprintf(accdet_log_buf,
			"(0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x (0x%x)=0x%x\n",
			idx, pmic_read(idx),
			idx+2, pmic_read(idx+2),
			idx+4, pmic_read(idx+4),
			idx+6, pmic_read(idx+6));
		strncat(buf, accdet_log_buf, strlen(accdet_log_buf));
	}
#ifdef CONFIG_ACCDET_EINT_IRQ
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
	sprintf(accdet_log_buf, "[Accdet EINT0 support][MODE_%d]regs:\n",
		accdet_dts.mic_mode);
	strncat(buf, accdet_log_buf, strlen(accdet_log_buf));
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
	sprintf(accdet_log_buf, "[ccdet EINT1 support][MODE_%d]regs:\n",
		accdet_dts.mic_mode);
	strncat(buf, accdet_log_buf, strlen(accdet_log_buf));
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
	sprintf(accdet_log_buf, "[Accdet BIEINT support][MODE_%d] regs:\n",
		accdet_dts.mic_mode);
	strncat(buf, accdet_log_buf, strlen(accdet_log_buf));
#else
	strncat(buf, "ACCDET_EINT_IRQ:NO EINT configed.Error!!\n", 64);
#endif
#elif defined CONFIG_ACCDET_EINT
	sprintf(accdet_log_buf, "[Accdet AP EINT][MODE_%d] regs:\n",
		accdet_dts.mic_mode);
	strncat(buf, accdet_log_buf, strlen(accdet_log_buf));
#else
	strncat(buf, "ACCDET EINT:No configed.Error!!\n", 64);
#endif

	sprintf(accdet_log_buf, "[0x%x]=0x%x\n",
		PMIC_RG_SCK32K_CK_PDN_ADDR,
		pmic_read(PMIC_RG_SCK32K_CK_PDN_ADDR));
	strncat(buf, accdet_log_buf, strlen(accdet_log_buf));

	sprintf(accdet_log_buf, "[0x%x]=0x%x\n",
		PMIC_RG_ACCDET_RST_ADDR, pmic_read(PMIC_RG_ACCDET_RST_ADDR));
	strncat(buf, accdet_log_buf, strlen(accdet_log_buf));

	sprintf(accdet_log_buf, "[0x%x]=0x%x, [0x%x]=0x%x, [0x%x]=0x%x\n",
		PMIC_RG_INT_EN_ACCDET_ADDR,
		pmic_read(PMIC_RG_INT_EN_ACCDET_ADDR),
		PMIC_RG_INT_MASK_ACCDET_ADDR,
		pmic_read(PMIC_RG_INT_MASK_ACCDET_ADDR),
		PMIC_RG_INT_STATUS_ACCDET_ADDR,
		pmic_read(PMIC_RG_INT_STATUS_ACCDET_ADDR));
	strncat(buf, accdet_log_buf, strlen(accdet_log_buf));

	sprintf(accdet_log_buf, "[0x%x]=0x%x,[0x%x]=0x%x\n",
		PMIC_RG_AUDPWDBMICBIAS1_ADDR,
		pmic_read(PMIC_RG_AUDPWDBMICBIAS1_ADDR),
		PMIC_RG_AUDACCDETMICBIAS0PULLLOW_ADDR,
		pmic_read(PMIC_RG_AUDACCDETMICBIAS0PULLLOW_ADDR));
	strncat(buf, accdet_log_buf, strlen(accdet_log_buf));

	sprintf(accdet_log_buf, "[0x%x]=0x%x, [0x%x]=0x%x\n",
		PMIC_AUXADC_RQST_CH5_ADDR, pmic_read(PMIC_AUXADC_RQST_CH5_ADDR),
		PMIC_AUXADC_ACCDET_AUTO_SPL_ADDR,
		pmic_read(PMIC_AUXADC_ACCDET_AUTO_SPL_ADDR));
	strncat(buf, accdet_log_buf, strlen(accdet_log_buf));

	sprintf(accdet_log_buf,
		"dtsInfo:deb0=0x%x,deb1=0x%x,deb3=0x%x,deb4=0x%x\n",
		 cust_pwm_deb->debounce0, cust_pwm_deb->debounce1,
		 cust_pwm_deb->debounce3, cust_pwm_deb->debounce4);
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

	if (addr_tmp < PMIC_TOP0_ANA_ID_ADDR)
		pr_notice("%s() Illegal addr[0x%x]!!\n", __func__, addr_tmp);
	else
		pmic_write(addr_tmp, value_tmp);

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
		/* accdet_init_debounce(); */
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
	if (pmic_read(PMIC_SWCID_ADDR) == 0x5910)
		pr_info("accdet not supported\r");
	else
		accdet_init_once();

	return count;
}

static ssize_t state_show(struct device_driver *ddri, char *buf)
{
	char temp_type = (char)cable_type;

	if (buf == NULL) {
		pr_notice("[%s] *buf is NULL!\n",  __func__);
		return -EINVAL;
	}
	snprintf(buf, 3, "%d\n", temp_type);

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
#endif /* end of #if PMIC_ACCDET_KERNEL */

static u32 accdet_get_auxadc(int deCount)
{
#if defined CONFIG_MTK_PMIC_NEW_ARCH | defined PMIC_ACCDET_CTP
	int vol = pmic_get_auxadc_value(AUXADC_LIST_ACCDET);

	pr_info("%s() vol_val:%d offset:%d real vol:%d mv!\n", __func__, vol,
		accdet_auxadc_offset,
		(vol < accdet_auxadc_offset) ? 0 : (vol-accdet_auxadc_offset));

	if (vol < accdet_auxadc_offset)
		vol = 0;
	else
		vol -= accdet_auxadc_offset;

	return vol;
#else
	return 0;
#endif
}

static void accdet_get_efuse(void)
{
	u32 efuseval = 0;
	int tmp_div;
	unsigned int moisture_eint0;
	unsigned int moisture_eint1;

	/* accdet offset efuse:
	 * this efuse must divided by 2
	 */
	efuseval = pmic_Read_Efuse_HPOffset(102);
	accdet_auxadc_offset = efuseval & 0xFF;
	if (accdet_auxadc_offset > 128)
		accdet_auxadc_offset -= 256;
	accdet_auxadc_offset = (accdet_auxadc_offset >> 1);
	pr_info("%s efuse=0x%x,auxadc_val=%dmv\n", __func__, efuseval,
		accdet_auxadc_offset);

/* all of moisture_vdd/moisture_offset0/eint is  2'complement,
 * we need to transfer it
 */
	/* moisture vdd efuse offset */
	efuseval = pmic_Read_Efuse_HPOffset(105);
	moisture_vdd_offset = (int)((efuseval >> 8) & ACCDET_CALI_MASK0);
	if (moisture_vdd_offset > 128)
		moisture_vdd_offset -= 256;
	pr_info("%s moisture_vdd efuse=0x%x, moisture_vdd_offset=%d mv\n",
		__func__, efuseval, moisture_vdd_offset);

	/* moisture offset */
	efuseval = pmic_Read_Efuse_HPOffset(106);
	moisture_offset = (int)(efuseval & ACCDET_CALI_MASK0);
	if (moisture_offset > 128)
		moisture_offset -= 256;
	pr_info("%s moisture_efuse efuse=0x%x,moisture_offset=%d mv\n",
		__func__, efuseval, moisture_offset);

	if (accdet_dts.moisture_use_ext_res == 0x0) {
		/* moisture eint efuse offset */
		efuseval = pmic_Read_Efuse_HPOffset(104);
		moisture_eint0 = (int)((efuseval >> 8) & ACCDET_CALI_MASK0);
		pr_info("%s moisture_eint0 efuse=0x%x,moisture_eint0=0x%x\n",
			__func__, efuseval, moisture_eint0);

		efuseval = pmic_Read_Efuse_HPOffset(105);
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
		tmp_div = water_r + moisture_int_r +
			8 * moisture_eint_offset + 450000;
		moisture_vm = moisture_vm / tmp_div;
		moisture_vm = moisture_vm + moisture_offset / 2;
		pr_info("%s internal moisture_vm=%d mv\n", __func__,
			moisture_vm);
	} else if (accdet_dts.moisture_use_ext_res == 0x1) {
		moisture_vm = (2800 + moisture_vdd_offset);
		moisture_vm = moisture_vm * water_r;
		moisture_vm = moisture_vm / (water_r + moisture_ext_r);
		moisture_vm = moisture_vm + (moisture_offset >> 1);
		pr_info("%s external moisture_vm=%d mv\n", __func__,
			moisture_vm);
	}

}

#ifdef CONFIG_FOUR_KEY_HEADSET
static void accdet_get_efuse_4key(void)
{
	u32 tmp_val = 0;
	u32 tmp_8bit = 0

	/* 4-key efuse:
	 * bit[9:2] efuse value is loaded, so every read out value need to be
	 * left shift 2 bit,and then compare with voltage get from AUXADC.
	 * AD efuse: key-A Voltage:0--AD;
	 * DB efuse: key-D Voltage: AD--DB;
	 * BC efuse: key-B Voltage:DB--BC;
	 * key-C Voltage: BC--600;
	 */
	tmp_val = pmic_Read_Efuse_HPOffset(103);
	tmp_8bit = tmp_val & ACCDET_CALI_MASK0;
	accdet_dts.four_key.mid = tmp_8bit << 2;

	tmp_8bit = (tmp_val >> 8) & ACCDET_CALI_MASK0;
	accdet_dts.four_key.voice = tmp_8bit << 2;

	tmp_val = pmic_Read_Efuse_HPOffset(104);
	tmp_8bit = tmp_val & ACCDET_CALI_MASK0;
	accdet_dts.four_key.up = tmp_8bit << 2;

	accdet_dts.four_key.down = 600;
	pr_info("accdet key thresh: mid=%dmv,voice=%dmv,up=%dmv,down=%dmv\n",
		accdet_dts.four_key.mid, accdet_dts.four_key.voice,
		accdet_dts.four_key.up, accdet_dts.four_key.down);
}

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

#if PMIC_ACCDET_KERNEL
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
#else
u64 accdet_get_current_time(void)
{
	return 0;
}

static bool accdet_timeout_ns(u64 start_time_ns, u64 timeout_time_ns)
{
	return true;
}
static void send_key_event(u32 keycode, u32 flag)
{
}
static void send_accdet_status_event(u32 cable_type, u32 status)
{
}
#endif /* end of #if PMIC_ACCDET_KERNEL */

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
	irq_bit = !(pmic_read(PMIC_ACCDET_IRQ_ADDR) & ACCDET_EINT_IRQ_B2_B3);
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
	pmic_write_set(PMIC_ACCDET_IRQ_ADDR, PMIC_ACCDET_IRQ_CLR_SHIFT);
	pr_debug("%s() IRQ_STS = 0x%x\n", __func__,
		pmic_read(PMIC_ACCDET_IRQ_ADDR));
}

static inline void clear_accdet_int_check(void)
{
	u64 cur_time = accdet_get_current_time();

	while ((pmic_read(PMIC_ACCDET_IRQ_ADDR) & ACCDET_IRQ_B0) &&
		(accdet_timeout_ns(cur_time, ACCDET_TIME_OUT)))
		;
	/* clear accdet int, modify  for fix interrupt trigger twice error */
	pmic_write_clr(PMIC_ACCDET_IRQ_ADDR, PMIC_ACCDET_IRQ_CLR_SHIFT);
	pmic_write_set(PMIC_RG_INT_STATUS_ACCDET_ADDR,
		PMIC_RG_INT_STATUS_ACCDET_SHIFT);
}

#ifdef CONFIG_ACCDET_EINT_IRQ
static inline void clear_accdet_eint(u32 eintid)
{
	if ((eintid & PMIC_EINT0) == PMIC_EINT0) {
		pmic_write_set(PMIC_ACCDET_IRQ_ADDR,
			PMIC_ACCDET_EINT0_IRQ_CLR_SHIFT);
	}
	if ((eintid & PMIC_EINT1) == PMIC_EINT1) {
		pmic_write_set(PMIC_ACCDET_IRQ_ADDR,
			PMIC_ACCDET_EINT1_IRQ_CLR_SHIFT);
	}

	pr_debug("%s() eint-%s IRQ-STS:[0x%x]=0x%x\n", __func__,
		(eintid == PMIC_EINT0)?"0":((eintid == PMIC_EINT1)?"1":"BI"),
		PMIC_ACCDET_IRQ_ADDR, pmic_read(PMIC_ACCDET_IRQ_ADDR));
}

static inline void clear_accdet_eint_check(u32 eintid)
{
	u64 cur_time = accdet_get_current_time();

	if ((eintid & PMIC_EINT0) == PMIC_EINT0) {
		while ((pmic_read(PMIC_ACCDET_IRQ_ADDR) & ACCDET_EINT0_IRQ_B2)
			&& (accdet_timeout_ns(cur_time, ACCDET_TIME_OUT)))
			;
		pmic_write_clr(PMIC_ACCDET_IRQ_ADDR,
				PMIC_ACCDET_EINT0_IRQ_CLR_SHIFT);
		pmic_write_set(PMIC_RG_INT_STATUS_ACCDET_ADDR,
				PMIC_RG_INT_STATUS_ACCDET_EINT0_SHIFT);
	}
	if ((eintid & PMIC_EINT1) == PMIC_EINT1) {
		while ((pmic_read(PMIC_ACCDET_IRQ_ADDR) & ACCDET_EINT1_IRQ_B3)
			&& (accdet_timeout_ns(cur_time, ACCDET_TIME_OUT)))
			;
		pmic_write_clr(PMIC_ACCDET_IRQ_ADDR,
				PMIC_ACCDET_EINT1_IRQ_CLR_SHIFT);
		pmic_write_set(PMIC_RG_INT_STATUS_ACCDET_ADDR,
				PMIC_RG_INT_STATUS_ACCDET_EINT1_SHIFT);
	}

	pr_debug("%s() eint-%s IRQ-STS:[0x%x]=0x%x TOP_INT_STS:[0x%x]:0x%x\n",
		__func__,
		(eintid == PMIC_EINT0)?"0":((eintid == PMIC_EINT1)?"1":"BI"),
		PMIC_ACCDET_IRQ_ADDR, pmic_read(PMIC_ACCDET_IRQ_ADDR),
		PMIC_RG_INT_STATUS_ACCDET_ADDR,
			pmic_read(PMIC_RG_INT_STATUS_ACCDET_ADDR));

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
		if (cur_eint_state == EINT_PIN_MOISTURE_DETECTED) {
			pr_info("%s Moisture plug out detectecd\n", __func__);
			cur_eint_state = EINT_PIN_PLUG_OUT;
			return M_PLUG_OUT;
		}
		if (cur_eint_state == EINT_PIN_PLUG_OUT) {
			pr_info("%s now check moisture\n", __func__);
			moisture_vol = accdet_get_auxadc(0);
			pr_info("moisture_vol:0x%x, moisture_vm:0x%x\r",
				moisture_vol, moisture_vm);
			if (moisture_vol > moisture_vm) {
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

static u32 adjust_eint_analog_setting(u32 eintID)
{
	if ((accdet_dts.eint_detect_mode == 0x3) ||
		(accdet_dts.eint_detect_mode == 0x4)) {
		/* ESD switches off */
		pmic_write_clr(PMIC_RG_ACCDETSPARE_ADDR, 8);
	}
	if (accdet_dts.eint_detect_mode == 0x4) {
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
		/* enable RG_EINT0CONFIGACCDET */
		pmic_write_set(PMIC_RG_EINT0CONFIGACCDET_ADDR,
			PMIC_RG_EINT0CONFIGACCDET_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
		/* enable RG_EINT1CONFIGACCDET */
		pmic_write_set(PMIC_RG_EINT1CONFIGACCDET_ADDR,
			PMIC_RG_EINT1CONFIGACCDET_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
		/* enable RG_EINT0CONFIGACCDET */
		pmic_write_set(PMIC_RG_EINT0CONFIGACCDET_ADDR,
			PMIC_RG_EINT0CONFIGACCDET_SHIFT);
		/* enable RG_EINT1CONFIGACCDET */
		pmic_write_set(PMIC_RG_EINT1CONFIGACCDET_ADDR,
			PMIC_RG_EINT1CONFIGACCDET_SHIFT);
#endif
		if ((accdet_dts.eint_use_ext_res == 0x3) ||
			(accdet_dts.eint_use_ext_res == 0x4)) {
			/*select 500k, use internal resistor */
			pmic_write_set(PMIC_RG_EINT0HIRENB_ADDR,
				PMIC_RG_EINT0HIRENB_SHIFT);
		}
	}
	return 0;
}

static u32 adjust_eint_digital_setting(u32 eintID)
{
	unsigned int ret = 0;

#ifdef CONFIG_ACCDET_SUPPORT_EINT0
	/* disable inverter */
	pmic_write_clr(PMIC_ACCDET_EINT0_INVERTER_SW_EN_ADDR,
					PMIC_ACCDET_EINT0_INVERTER_SW_EN_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
	/* disable inverter */
	pmic_write_clr(PMIC_ACCDET_EINT1_INVERTER_SW_EN_ADDR,
					PMIC_ACCDET_EINT1_INVERTER_SW_EN_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
	/* disable inverter */
	pmic_write_clr(PMIC_ACCDET_EINT0_INVERTER_SW_EN_ADDR,
					PMIC_ACCDET_EINT0_INVERTER_SW_EN_SHIFT);
	/* disable inverter */
	pmic_write_clr(PMIC_ACCDET_EINT1_INVERTER_SW_EN_ADDR,
					PMIC_ACCDET_EINT1_INVERTER_SW_EN_SHIFT);
#endif

	if ((accdet_dts.eint_detect_mode == 0x1) ||
		(accdet_dts.eint_detect_mode == 0x2)) {
		pmic_write_clr(PMIC_ACCDET_DA_STABLE_ADDR,
			PMIC_ACCDET_EINT0_CEN_STABLE_SHIFT);
		ret = get_moisture_sw_auxadc_check();
		/* disable mtest en */
		pmic_write_clr(PMIC_RG_MTEST_EN_ADDR, PMIC_RG_MTEST_EN_SHIFT);
		pmic_write_clr(PMIC_AUDACCDETAUXADCSWCTRL_SEL_ADDR,
			PMIC_AUDACCDETAUXADCSWCTRL_SEL_SHIFT);
		return ret;
	}
	if (accdet_dts.eint_detect_mode == 0x3) {
		pmic_write_clr(PMIC_ACCDET_DA_STABLE_ADDR,
			PMIC_ACCDET_EINT0_CEN_STABLE_SHIFT);
		pmic_write_set(PMIC_ACCDET_EINT_CTURBO_SEL_ADDR,
			PMIC_ACCDET_EINT_CTURBO_SEL_SHIFT);
		pmic_write_set(PMIC_ACCDET_EINT0_CTURBO_SW_ADDR,
			PMIC_ACCDET_EINT0_CTURBO_SW_SHIFT);
		pr_info("auxadc T1\r");
		ret = get_moisture_sw_auxadc_check();
	if ((ret == M_WATER_IN) || (ret == M_HP_PLUG_IN)) {
		pmic_write_clr(PMIC_ACCDET_EINT_CTURBO_SEL_ADDR,
			PMIC_ACCDET_EINT_CTURBO_SEL_SHIFT);
		pmic_write_clr(PMIC_ACCDET_EINT0_CTURBO_SW_ADDR,
			PMIC_ACCDET_EINT0_CTURBO_SW_SHIFT);
	}
		return ret;
	}
	if (accdet_dts.eint_detect_mode == 0x4) {
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
		/* set DA stable signal */
		pmic_write_clr(PMIC_ACCDET_DA_STABLE_ADDR,
			PMIC_ACCDET_EINT0_CEN_STABLE_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
		/* set DA stable signal */
		pmic_write_clr(PMIC_ACCDET_DA_STABLE_ADDR,
			PMIC_ACCDET_EINT1_CEN_STABLE_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
		/* set DA stable signal */
		pmic_write_clr(PMIC_ACCDET_DA_STABLE_ADDR,
			PMIC_ACCDET_EINT0_CEN_STABLE_SHIFT);
		/* set DA stable signal */
		pmic_write_clr(PMIC_ACCDET_DA_STABLE_ADDR,
			PMIC_ACCDET_EINT1_CEN_STABLE_SHIFT);
#endif
	}
	return 0;
}
static u32 adjust_moisture_digital_setting(u32 eintID)
{
if ((accdet_dts.moisture_detect_mode == 0x1) ||
	(accdet_dts.moisture_detect_mode == 0x2) ||
	(accdet_dts.moisture_detect_mode == 0x3)) {
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
	/* wk1, enable moisture detection */
	pmic_write_set(PMIC_ACCDET_EINT0_M_SW_EN_ADDR,
		PMIC_ACCDET_EINT0_M_SW_EN_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
	/* wk1, enable moisture detection */
	pmic_write_set(PMIC_ACCDET_EINT1_M_SW_EN_ADDR,
		PMIC_ACCDET_EINT1_M_SW_EN_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
	/* wk1, enable moisture detection */
	pmic_write_set(PMIC_ACCDET_EINT0_M_SW_EN_ADDR,
		PMIC_ACCDET_EINT0_M_SW_EN_SHIFT);
	pmic_write_set(PMIC_ACCDET_EINT1_M_SW_EN_ADDR,
		PMIC_ACCDET_EINT1_M_SW_EN_SHIFT);
#endif
}
	return 0;
}

static u32 adjust_moisture_analog_setting(u32 eintID)
{
	unsigned int efuseval = 0, vref2val = 0, vref2hi = 0;

	if (accdet_dts.moisture_detect_mode == 0x1) {
		/* select VTH to 2.8v(default), can set to 2.4 or 2.0v by dts */
		pmic_write_mset(PMIC_RG_EINTCOMPVTH_ADDR,
			PMIC_RG_EINTCOMPVTH_SHIFT, PMIC_RG_EINTCOMPVTH_MASK,
			accdet_dts.moisture_comp_vth);
	} else if ((accdet_dts.moisture_detect_mode == 0x2) ||
		(accdet_dts.moisture_detect_mode == 0x3)) {
		/* select VTH to 2.8v(default), can set to 2.4 or 2.0v by dts */
		pmic_write_mset(PMIC_RG_EINTCOMPVTH_ADDR,
			PMIC_RG_EINTCOMPVTH_SHIFT, PMIC_RG_EINTCOMPVTH_MASK,
			accdet_dts.moisture_comp_vth);

		/* Enable mtest en */
		pmic_write_set(PMIC_RG_MTEST_EN_ADDR, PMIC_RG_MTEST_EN_SHIFT);

		/* select PAD_HP_EINT for moisture detection */
		pmic_write_clr(PMIC_RG_MTEST_SEL_ADDR, PMIC_RG_MTEST_SEL_SHIFT);
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
		pmic_write_set(PMIC_RG_ACCDETSPARE_ADDR, 15);
		pmic_write_mset(PMIC_RG_EINTCOMPVTH_ADDR,
			6, 0x3, (vref2val & 0xc) >> 2);
		pmic_write_mset(PMIC_RG_ACCDETSPARE_ADDR,
			13, 0x3, (vref2val & 0x3));
		/* golden setting
		 * pmic_write_mset(PMIC_RG_ACCDETSPARE_ADDR, 3, 0x1f, 0x1e);
		 */
	}
	return 0;
}

static u32 adjust_moisture_setting(u32 moistureID, u32 eintID)
{
	unsigned int ret = 0;

	if (moistureID == M_PLUG_IN) {
		if (accdet_thing_in_flag == true) {
			/* receive M_PLUG_IN second time, just clear irq sts */
			clear_accdet_eint(eintID);
			clear_accdet_eint_check(eintID);
		} else {
			/* to check if 1st time thing in interrupt */
			accdet_thing_in_flag = true;
			cur_eint_state = EINT_PIN_PLUG_OUT;
			/* adjust analog moisture setting */
			adjust_moisture_analog_setting(eintID);
		if (accdet_dts.moisture_detect_mode != 0x5) {
			/* wk2 */
			pmic_write_set(PMIC_RG_EINT0HIRENB_ADDR,
				PMIC_RG_EINT0HIRENB_SHIFT);
		}
			/* adjust digital setting */
			ret = adjust_eint_digital_setting(eintID);
			/* adjust digital moisture setting */
			adjust_moisture_digital_setting(eintID);
			/* sw axuadc check, EINT 1.0~ EINT 2.0 */
			clear_accdet_eint(eintID);
			clear_accdet_eint_check(eintID);
			if (ret != 0)
				return ret;
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
			/* wk1, enable moisture detection */
			pmic_write_set(PMIC_ACCDET_EINT0_M_SW_EN_ADDR,
				PMIC_ACCDET_EINT0_M_SW_EN_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
			/* wk1, enable moisture detection */
			pmic_write_set(PMIC_ACCDET_EINT1_M_SW_EN_ADDR,
				PMIC_ACCDET_EINT1_M_SW_EN_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
			/* wk1, enable moisture detection */
			pmic_write_set(PMIC_ACCDET_EINT0_M_SW_EN_ADDR,
				PMIC_ACCDET_EINT0_M_SW_EN_SHIFT);
			pmic_write_set(PMIC_ACCDET_EINT1_M_SW_EN_ADDR,
				PMIC_ACCDET_EINT1_M_SW_EN_SHIFT);
#endif
			pr_info("%s() , thing in done\n", __func__);
		}
		return M_NO_ACT;
	} else if (moistureID == M_WATER_IN) {
		cur_eint_state = EINT_PIN_MOISTURE_DETECTED;
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
		if (cur_eint_state == EINT_PIN_MOISTURE_DETECTED)
			cur_eint_state = EINT_PIN_PLUG_OUT;

#ifdef CONFIG_ACCDET_SUPPORT_EINT0
		/* wk1, disable moisture detection */
		pmic_write_clr(PMIC_ACCDET_EINT0_M_SW_EN_ADDR,
			PMIC_ACCDET_EINT0_M_SW_EN_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
		/* wk1, disable moisture detection */
		pmic_write_clr(PMIC_ACCDET_EINT1_M_SW_EN_ADDR,
			PMIC_ACCDET_EINT1_M_SW_EN_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
		/* wk1, disable moisture detection */
		pmic_write_clr(PMIC_ACCDET_EINT0_M_SW_EN_ADDR,
			PMIC_ACCDET_EINT0_M_SW_EN_SHIFT);
		pmic_write_clr(PMIC_ACCDET_EINT1_M_SW_EN_ADDR,
			PMIC_ACCDET_EINT1_M_SW_EN_SHIFT);
#endif
		/* wk3, if HP + W together, after detect HP, we should
		 * set accdet_sync_flag to true to avoid receive W interrupt
		 */
		eint_accdet_sync_flag = true;

		adjust_eint_analog_setting(eintID);
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

static u32 adjust_eint_setting(u32 moistureID, u32 eintID)
{

	if (moistureID == M_PLUG_IN) {
		/* adjust digital setting */
		adjust_eint_digital_setting(eintID);
		/* adjust analog setting */
		adjust_eint_analog_setting(eintID);
	} else if (moistureID == M_PLUG_OUT) {
		/* set debounce to 1ms */
		accdet_set_debounce(eint_state000,
			accdet_dts.pwm_deb.eint_debounce0);
	} else {
		pr_debug("should not be here %s()\n", __func__);
	}
	/* dump_register(); */
	return 0;
}

static void recover_eint_analog_setting(void)
{
	if ((accdet_dts.eint_detect_mode == 0x3) ||
		(accdet_dts.eint_detect_mode == 0x4)) {
		/* ESD switches on */
		pmic_write_set(PMIC_RG_ACCDETSPARE_ADDR, 8);
	}
	if (accdet_dts.eint_detect_mode == 0x4) {
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
		/* disable RG_EINT0CONFIGACCDET */
		pmic_write_clr(PMIC_RG_EINT0CONFIGACCDET_ADDR,
			PMIC_RG_EINT0CONFIGACCDET_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
		/* disable RG_EINT1CONFIGACCDET */
		pmic_write_clr(PMIC_RG_EINT1CONFIGACCDET_ADDR,
			PMIC_RG_EINT1CONFIGACCDET_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
		/* disable RG_EINT0CONFIGACCDET */
		pmic_write_clr(PMIC_RG_EINT0CONFIGACCDET_ADDR,
			PMIC_RG_EINT0CONFIGACCDET_SHIFT);
		/* disable RG_EINT0CONFIGACCDET */
		pmic_write_clr(PMIC_RG_EINT1CONFIGACCDET_ADDR,
			PMIC_RG_EINT1CONFIGACCDET_SHIFT);
#endif
		pmic_write_clr(PMIC_RG_EINT0HIRENB_ADDR,
			PMIC_RG_EINT0HIRENB_SHIFT);
	}

}
static void recover_eint_digital_setting(void)
{
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
	/* wk1, disable moisture detection */
	pmic_write_clr(PMIC_ACCDET_EINT0_M_SW_EN_ADDR,
		PMIC_ACCDET_EINT0_M_SW_EN_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
	/* wk1, disable moisture detection */
	pmic_write_clr(PMIC_ACCDET_EINT1_M_SW_EN_ADDR,
		PMIC_ACCDET_EINT1_M_SW_EN_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
	/* wk1, disable moisture detection */
	pmic_write_clr(PMIC_ACCDET_EINT0_M_SW_EN_ADDR,
		PMIC_ACCDET_EINT0_M_SW_EN_SHIFT);
	pmic_write_clr(PMIC_ACCDET_EINT1_M_SW_EN_ADDR,
		PMIC_ACCDET_EINT1_M_SW_EN_SHIFT);
#endif
	if (accdet_dts.eint_detect_mode == 0x4) {
		/* enable eint0cen */
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
		/* enable eint0cen */
		pmic_write_set(PMIC_ACCDET_DA_STABLE_ADDR,
			PMIC_ACCDET_EINT0_CEN_STABLE_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
		/* enable eint1cen */
		pmic_write_set(PMIC_ACCDET_DA_STABLE_ADDR,
			PMIC_ACCDET_EINT1_CEN_STABLE_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
		/* enable eint0cen */
		pmic_write_set(PMIC_ACCDET_DA_STABLE_ADDR,
			PMIC_ACCDET_EINT0_CEN_STABLE_SHIFT);
		/* enable eint1cen */
		pmic_write_set(PMIC_ACCDET_DA_STABLE_ADDR,
			PMIC_ACCDET_EINT1_CEN_STABLE_SHIFT);
#endif
	}

	if (accdet_dts.eint_detect_mode != 0x1) {
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
		/* enable inverter */
		pmic_write_set(PMIC_ACCDET_EINT0_INVERTER_SW_EN_ADDR,
				PMIC_ACCDET_EINT0_INVERTER_SW_EN_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
		/* enable inverter */
		pmic_write_set(PMIC_ACCDET_EINT1_INVERTER_SW_EN_ADDR,
				PMIC_ACCDET_EINT1_INVERTER_SW_EN_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
		/* enable inverter */
		pmic_write_set(PMIC_ACCDET_EINT0_INVERTER_SW_EN_ADDR,
				PMIC_ACCDET_EINT0_INVERTER_SW_EN_SHIFT);
		/* enable inverter */
		pmic_write_set(PMIC_ACCDET_EINT1_INVERTER_SW_EN_ADDR,
				PMIC_ACCDET_EINT1_INVERTER_SW_EN_SHIFT);
#endif
	}
}

static void recover_moisture_analog_setting(void)
{
	if (accdet_dts.moisture_detect_mode == 0x1) {
		/* select VTH to 2v */
		pmic_write_mset(PMIC_RG_EINTCOMPVTH_ADDR,
			PMIC_RG_EINTCOMPVTH_SHIFT,
			PMIC_RG_EINTCOMPVTH_MASK, 0x2);
	} else if (accdet_dts.moisture_detect_mode == 0x2) {
	} else if (accdet_dts.moisture_detect_mode == 0x3) {
	} else if (accdet_dts.moisture_detect_mode == 0x4) {
	} else if (accdet_dts.moisture_detect_mode == 0x5) {
		/* enable comp1 delay window */
		pmic_write_clr(PMIC_RG_EINT0NOHYS_ADDR,
			PMIC_RG_EINT0NOHYS_SHIFT);

		accdet_set_debounce(eint_state011,
			accdet_dts.pwm_deb.eint_debounce3);
		/* disconnect VREF2 to EINT0CMP and recover to vref */
		pmic_write_mset(PMIC_RG_EINTCOMPVTH_ADDR,
			PMIC_RG_EINTCOMPVTH_SHIFT, PMIC_RG_EINTCOMPVTH_MASK,
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

static void recover_eint_setting(u32 moistureID)
{
	if (moistureID == M_PLUG_OUT) {
		recover_eint_analog_setting();
		recover_eint_digital_setting();
		pr_info("%s done\n", __func__);
	}
}

static u32 get_triggered_eint(void)
{
	u32 eint_ID = NO_PMIC_EINT;
	u32 irq_status = pmic_read(PMIC_ACCDET_IRQ_ADDR);

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

#endif

static inline void enable_accdet(u32 state_swctrl)
{
	/* enable ACCDET unit */
	pmic_write_set(PMIC_ACCDET_SW_EN_ADDR, PMIC_ACCDET_SW_EN_SHIFT);
	pr_info("%s done IRQ-STS[0x%x]=0x%x,PWM[0x%x]=0x%x\n",
		__func__, PMIC_ACCDET_IRQ_ADDR,
		pmic_read(PMIC_ACCDET_IRQ_ADDR),
		PMIC_ACCDET_CMP_PWM_EN_ADDR,
		pmic_read(PMIC_ACCDET_CMP_PWM_EN_ADDR));
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

	/* recover accdet debounce0,3 */
	accdet_set_debounce(accdet_state000, cust_pwm_deb->debounce0);
	accdet_set_debounce(accdet_state001, cust_pwm_deb->debounce1);
	accdet_set_debounce(accdet_state011, cust_pwm_deb->debounce3);
	pr_info("%s done IRQ-STS[0x%x]=0x%x,PWM[0x%x]=0x%x\n",
		__func__, PMIC_ACCDET_IRQ_ADDR, pmic_read(PMIC_ACCDET_IRQ_ADDR),
		PMIC_ACCDET_CMP_PWM_EN_ADDR,
		pmic_read(PMIC_ACCDET_CMP_PWM_EN_ADDR));
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
	dis_micbias_done = false;
	pr_info("accdet %s, set cable_type = NO_DEVICE %d\n", __func__,
		dis_micbias_done);
#if PMIC_ACCDET_DEBUG
	dump_register();
#endif
}
#if PMIC_ACCDET_KERNEL
static void dis_micbias_timerhandler(struct timer_list *t)
{
	int ret = 0;

	ret = queue_work(dis_micbias_workqueue, &dis_micbias_work);
	if (!ret)
		pr_info("accdet %s, queue work return:%d!\n", __func__, ret);
}

static void dis_micbias_work_callback(struct work_struct *work)
{
	u32 cur_AB, eintID;

	/* check EINT0 status, if plug out,
	 * not need to disable accdet here
	 */
	eintID = pmic_read_mbit(PMIC_ACCDET_EINT0_MEM_IN_ADDR,
		PMIC_ACCDET_EINT0_MEM_IN_SHIFT,
		PMIC_ACCDET_EINT0_MEM_IN_MASK);
	if (eintID == M_PLUG_OUT) {
		pr_info("%s Plug-out, no dis micbias\n", __func__);
		return;
	}
	/* if modify_vref_volt called, not need to dis micbias again */
	if (dis_micbias_done == true) {
		pr_info("%s modify_vref_volt called\n", __func__);
		return;
	}

cur_AB = pmic_read(PMIC_ACCDET_MEM_IN_ADDR) >> ACCDET_STATE_MEM_IN_OFFSET;
	cur_AB = cur_AB & ACCDET_STATE_AB_MASK;

	/* if 3pole disable accdet
	 * if <20k + 4pole, disable accdet will disable accdet
	 * plug out interrupt. The behavior will same as 3pole
	 */
	if (cable_type == HEADSET_MIC) {
		/* do nothing */
	} else if ((cable_type == HEADSET_NO_MIC) ||
		(cur_AB == ACCDET_STATE_AB_00) ||
		(cur_AB == ACCDET_STATE_AB_11)) {
		/* disable accdet_sw_en=0
		 * disable accdet_hwmode_en=0
		 */
		pmic_write_clr(PMIC_ACCDET_SW_EN_ADDR,
			PMIC_ACCDET_SW_EN_SHIFT);
		disable_accdet();
	pr_info("%s more than 6s,MICBIAS:Disabled AB:0x%x c_type:0x%x\n",
		__func__, cur_AB, cable_type);
	}
}
#endif /* end of #if PMIC_ACCDET_KERNEL */

#if PMIC_ACCDET_KERNEL
static void eint_work_callback(struct work_struct *work)
#else
static void eint_work_callback(void)
#endif
{
	pr_info("accdet %s(),DCC EINT func\n", __func__);

	if (cur_eint_state == EINT_PIN_PLUG_IN) {
		/* wk, disable vusb LP */
		pmic_write(PMIC_RG_LDO_VUSB_HW0_OP_EN_ADDR, 0x8000);
		pr_info("%s VUSB LP dis\n", __func__);

		pr_info("accdet cur: plug-in, cur_eint_state = %d\n",
			cur_eint_state);
		mutex_lock(&accdet_eint_irq_sync_mutex);
		eint_accdet_sync_flag = true;
		mutex_unlock(&accdet_eint_irq_sync_mutex);
		__pm_wakeup_event(accdet_timer_lock,
			jiffies_to_msecs(7 * HZ));

		accdet_init();

		pr_info("%s VUSB LP dis done\n", __func__);
		enable_accdet(0);
	} else {
		pr_info("accdet cur:plug-out, cur_eint_state = %d\n",
			cur_eint_state);
		mutex_lock(&accdet_eint_irq_sync_mutex);
		eint_accdet_sync_flag = false;
		accdet_thing_in_flag = false;
		mutex_unlock(&accdet_eint_irq_sync_mutex);
		if (accdet_dts.moisture_detect_mode != 0x5)
			del_timer_sync(&micbias_timer);

		/* disable accdet_sw_en=0
		 */
		pmic_write_clr(PMIC_ACCDET_SW_EN_ADDR,
			PMIC_ACCDET_SW_EN_SHIFT);
		disable_accdet();
		headset_plug_out();
	}

#ifdef CONFIG_ACCDET_EINT_IRQ
	if (get_moisture_det_en() == 0x1)
		recover_moisture_setting(gmoistureID);
	else
		recover_eint_setting(gmoistureID);
#endif
#ifdef CONFIG_ACCDET_EINT
	enable_irq(accdet_irq);
	pr_info("accdet %s enable_irq !!\n", __func__);
#endif
}

void accdet_set_debounce(int state, unsigned int debounce)
{
	switch (state) {
	case accdet_state000:
		/* set ACCDET debounce value = debounce/32 ms */
		pmic_write(PMIC_ACCDET_DEBOUNCE0_ADDR, debounce);
		break;
	case accdet_state001:
		pmic_write(PMIC_ACCDET_DEBOUNCE1_ADDR, debounce);
		break;
	case accdet_state010:
		pmic_write(PMIC_ACCDET_DEBOUNCE2_ADDR, debounce);
		break;
	case accdet_state011:
		pmic_write(PMIC_ACCDET_DEBOUNCE3_ADDR, debounce);
		break;
	case accdet_auxadc:
		/* set auxadc debounce:0x42(2ms) */
		pmic_write(PMIC_ACCDET_CONNECT_AUXADC_TIME_DIG_ADDR, debounce);
		break;
	case eint_state000:
		pmic_write_mset(PMIC_ACCDET_EINT_DEBOUNCE0_ADDR,
				PMIC_ACCDET_EINT_DEBOUNCE0_SHIFT,
				PMIC_ACCDET_EINT_DEBOUNCE0_MASK,
				debounce);
		break;
	case eint_state001:
		pmic_write_mset(PMIC_ACCDET_EINT_DEBOUNCE1_ADDR,
				PMIC_ACCDET_EINT_DEBOUNCE1_SHIFT,
				PMIC_ACCDET_EINT_DEBOUNCE1_MASK,
				debounce);
		break;
	case eint_state010:
		pmic_write_mset(PMIC_ACCDET_EINT_DEBOUNCE2_ADDR,
				PMIC_ACCDET_EINT_DEBOUNCE2_SHIFT,
				PMIC_ACCDET_EINT_DEBOUNCE2_MASK,
				debounce);
		break;
	case eint_state011:
		pmic_write_mset(PMIC_ACCDET_EINT_DEBOUNCE3_ADDR,
				PMIC_ACCDET_EINT_DEBOUNCE3_SHIFT,
				PMIC_ACCDET_EINT_DEBOUNCE3_MASK,
				debounce);
		break;
	case eint_inverter_state000:
		pmic_write(PMIC_ACCDET_EINT_INVERTER_DEBOUNCE_ADDR, debounce);
		break;
	default:
		pr_info("%s error state:%d!\n", __func__, state);
		break;
	}
}

static inline void check_cable_type(void)
{
	u32 cur_AB;

cur_AB = pmic_read(PMIC_ACCDET_MEM_IN_ADDR) >> ACCDET_STATE_MEM_IN_OFFSET;
	cur_AB = cur_AB & ACCDET_STATE_AB_MASK;
	pr_notice("accdet %s(), cur_status:%s current AB = %d\n", __func__,
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
			/* wk, for IOT HP */
			accdet_set_debounce(eint_state011,
				accdet_dts.pwm_deb.eint_debounce3);
		} else if (cur_AB == ACCDET_STATE_AB_01) {
			mutex_lock(&accdet_eint_irq_sync_mutex);
			if (eint_accdet_sync_flag) {
				accdet_status = MIC_BIAS;
				cable_type = HEADSET_MIC;
			} else
				pr_info("accdet headset has been plug-out\n");
			mutex_unlock(&accdet_eint_irq_sync_mutex);
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
			/* wk, for IOT HP */
			accdet_set_debounce(eint_state011, 0x1);
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
			/* wk, for IOT HP */
			accdet_set_debounce(eint_state011, 0x1);
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

			/* adjust debounce0 and debounce1 to fix miss key issue.
			 */
			accdet_set_debounce(accdet_state000,
				button_press_debounce);
			accdet_set_debounce(accdet_state001,
				button_press_debounce_01);

			/* wk, for IOT HP */
			accdet_set_debounce(eint_state011, 0x1);
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

#if PMIC_ACCDET_KERNEL
static void accdet_work_callback(struct work_struct *work)
#else
static void accdet_work_callback(void)
#endif
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
	if (cable_type != NO_DEVICE) {
		accdet_modify_vref_volt_self();
		/* wk, enable vusb LP */
		pmic_write(PMIC_RG_LDO_VUSB_HW0_OP_EN_ADDR, 0x8005);
		pr_info("%s VUSB LP en\n", __func__);
	}

	pr_info("%s() report cable_type done\n", __func__);
	__pm_relax(accdet_irq_lock);
}

static void accdet_queue_work(void)
{
	int ret;

	if (accdet_status == MIC_BIAS)
		cali_voltage = accdet_get_auxadc(1);

#if PMIC_ACCDET_KERNEL
	ret = queue_work(accdet_workqueue, &accdet_work);
#else
	accdet_work_callback();
#endif /* end of #if PMIC_ACCDET_KERNEL */
	if (!ret)
		pr_info("queue work accdet_work return:%d!\n", ret);
}

#ifdef CONFIG_ACCDET_EINT_IRQ
static int pmic_eint_queue_work(int eintID)
{
	int ret = 0;

	pr_info("%s() Enter. eint-%s cur_eint_state:%d\n", __func__,
		(eintID == PMIC_EINT0)?"0":((eintID == PMIC_EINT1)?"1":"BI"),
		cur_eint_state);

	if (cur_eint_state == EINT_PIN_MOISTURE_DETECTED) {
		pr_info("%s water in then plug out, handle plugout\r",
			__func__);
		cur_eint_state = EINT_PIN_PLUG_OUT;
#if PMIC_ACCDET_KERNEL
		ret = queue_work(eint_workqueue, &eint_work);
#else
		eint_work_callback();
#endif /* end of #if PMIC_ACCDET_KERNEL */
		return 0;
	}
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
	if (eintID == PMIC_EINT0) {
		if (cur_eint_state == EINT_PIN_PLUG_IN) {
			accdet_set_debounce(accdet_state011,
				cust_pwm_deb->debounce3);
			cur_eint_state = EINT_PIN_PLUG_OUT;
		} else {
			if (gmoistureID != M_PLUG_OUT) {
				cur_eint_state = EINT_PIN_PLUG_IN;

				if (accdet_dts.moisture_detect_mode != 0x5) {
					mod_timer(&micbias_timer,
					jiffies + MICBIAS_DISABLE_TIMER);
				}
			}
		}
#if PMIC_ACCDET_KERNEL
		ret = queue_work(eint_workqueue, &eint_work);
#else
		eint_work_callback();
#endif /* end of #if PMIC_ACCDET_KERNEL */
	} else
		pr_info("%s invalid EINT ID!\n", __func__);

#elif defined CONFIG_ACCDET_SUPPORT_EINT1
	if (eintID == PMIC_EINT1) {
		if (cur_eint_state == EINT_PIN_PLUG_IN) {
			accdet_set_debounce(accdet_state011,
				cust_pwm_deb->debounce3);
			cur_eint_state = EINT_PIN_PLUG_OUT;
		} else {
			if (gmoistureID != M_PLUG_OUT) {
				cur_eint_state = EINT_PIN_PLUG_IN;

				if (accdet_dts.moisture_detect_mode != 0x5) {
					mod_timer(&micbias_timer,
					jiffies + MICBIAS_DISABLE_TIMER);
				}
			}
		}
#if PMIC_ACCDET_KERNEL
		ret = queue_work(eint_workqueue, &eint_work);
#else
		eint_work_callback();
#endif /* end of #if PMIC_ACCDET_KERNEL */
	} else
		pr_info("%s invalid EINT ID!\n", __func__);

#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
	if ((eintID & PMIC_EINT0) == PMIC_EINT0) {
		if (cur_eint0_state == EINT_PIN_PLUG_IN) {
			accdet_set_debounce(accdet_state011,
				cust_pwm_deb->debounce3);
			cur_eint0_state = EINT_PIN_PLUG_OUT;
		} else {
			if (gmoistureID != M_PLUG_OUT)
				cur_eint0_state = EINT_PIN_PLUG_IN;
		}
	}
	if ((eintID & PMIC_EINT1) == PMIC_EINT1) {
		if (cur_eint1_state == EINT_PIN_PLUG_IN) {
			accdet_set_debounce(accdet_state011,
				cust_pwm_deb->debounce3);
			cur_eint1_state = EINT_PIN_PLUG_OUT;
		} else {
			if (gmoistureID != M_PLUG_OUT)
				cur_eint1_state = EINT_PIN_PLUG_IN;
		}
	}

	/* bi_eint trigger issued current state, may */
	if (cur_eint_state == EINT_PIN_PLUG_OUT) {
		cur_eint_state = cur_eint0_state & cur_eint1_state;
		if (cur_eint_state == EINT_PIN_PLUG_IN) {
			if (accdet_dts.moisture_detect_mode != 0x5) {
				mod_timer(&micbias_timer,
					jiffies + MICBIAS_DISABLE_TIMER);
			}
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

static u32 config_moisture_detect_1_0(void)
{
	/* Disable ACCDET to AUXADC */
	pmic_write_clr(PMIC_RG_ACCDET2AUXSWEN_ADDR,
		PMIC_RG_ACCDET2AUXSWEN_SHIFT);
	pmic_write_set(PMIC_AUDACCDETAUXADCSWCTRL_SEL_ADDR,
		PMIC_AUDACCDETAUXADCSWCTRL_SEL_SHIFT);
	pmic_write_clr(PMIC_AUDACCDETAUXADCSWCTRL_SW_ADDR,
		PMIC_AUDACCDETAUXADCSWCTRL_SW_SHIFT);

	/* Enable moisture detection */
	pmic_write_set(PMIC_RG_MTEST_EN_ADDR, PMIC_RG_MTEST_EN_SHIFT);

	/* select PAD_HP_EINT for moisture detection */
	pmic_write_clr(PMIC_RG_MTEST_SEL_ADDR, PMIC_RG_MTEST_SEL_SHIFT);

	/* select VTH to 2v */
	pmic_write_mset(PMIC_RG_EINTCOMPVTH_ADDR,
		PMIC_RG_EINTCOMPVTH_SHIFT, PMIC_RG_EINTCOMPVTH_MASK, 0x2);

	return 0;
}

static u32 config_moisture_detect_1_1(void)
{
	/* Disable ACCDET to AUXADC */
	pmic_write_clr(PMIC_RG_ACCDET2AUXSWEN_ADDR,
		PMIC_RG_ACCDET2AUXSWEN_SHIFT);
	pmic_write_set(PMIC_AUDACCDETAUXADCSWCTRL_SEL_ADDR,
		PMIC_AUDACCDETAUXADCSWCTRL_SEL_SHIFT);
	pmic_write_clr(PMIC_AUDACCDETAUXADCSWCTRL_SW_ADDR,
		PMIC_AUDACCDETAUXADCSWCTRL_SW_SHIFT);

	/* select VTH to 2v */
	pmic_write_mset(PMIC_RG_EINTCOMPVTH_ADDR,
		PMIC_RG_EINTCOMPVTH_SHIFT, PMIC_RG_EINTCOMPVTH_MASK, 0x2);

	return 0;
}

static u32 config_moisture_detect_2_1(void)
{
	u32 efuseval, eintvth;
	/* select VTH to 2.8v(default), can set to 2.4 or 2.0v by dts */
	pmic_write_mset(PMIC_RG_EINTCOMPVTH_ADDR,
		PMIC_RG_EINTCOMPVTH_SHIFT, PMIC_RG_EINTCOMPVTH_MASK,
		accdet_dts.moisture_comp_vth);
	/* 2. Set RG_EINT0CTURBO<2:0>  ; 75k~15k
	 * read efuse:
	 * 3. Set RG_ACCDETSPARE<7:3> ; VREF2
	 * read efuse:
	 */
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
	/* enable cturbo setting */
	pmic_write_set(PMIC_RG_EINT0CTURBO_ADDR, PMIC_RG_EINT0CTURBO_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
	pmic_write_set(PMIC_RG_EINT1CTURBO_ADDR, PMIC_RG_EINT1CTURBO_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
	pmic_write_set(PMIC_RG_EINT0CTURBO_ADDR, PMIC_RG_EINT0CTURBO_SHIFT);
	pmic_write_set(PMIC_RG_EINT1CTURBO_ADDR, PMIC_RG_EINT1CTURBO_SHIFT);
#endif
	/* set moisture reference voltage MVTH
	 * golden setting
	 * pmic_write_mset(PMIC_RG_ACCDETSPARE_ADDR, 0x3, 0x1F, 0xA);
	 */

	/* EINTVTH1K/5K/10K efuse */
	efuseval = pmic_Read_Efuse_HPOffset(107);
	eintvth = (int)(efuseval & ACCDET_CALI_MASK0);
	pr_info("%s moisture_eint0 efuse=0x%x,eintvth=0x%x\n",
		__func__, efuseval, eintvth);

	/* set moisture reference voltage MVTH */
	pmic_write_mset(PMIC_RG_ACCDETSPARE_ADDR, 0x3, 0x1F, eintvth);
	return 0;
}

static u32 config_moisture_detect_2_1_1(void)
{
	/* select VTH to 2.8v(default), can set to 2.4 or 2.0v by dts */
	pmic_write_mset(PMIC_RG_EINTCOMPVTH_ADDR,
		PMIC_RG_EINTCOMPVTH_SHIFT, PMIC_RG_EINTCOMPVTH_MASK,
		accdet_dts.moisture_comp_vth);

	return 0;
}

#endif

void accdet_irq_handle(void)
{
	u32 eintID = 0;
#ifdef CONFIG_ACCDET_EINT_IRQ
	u32 ret = 0;
#endif
	u32 irq_status, acc_sts, eint_sts;
#if PMIC_ACCDET_CTP || PMIC_ACCDET_DEBUG
	dump_register();
#endif
#ifdef CONFIG_ACCDET_EINT_IRQ
	eintID = get_triggered_eint();
#endif
	irq_status = pmic_read(PMIC_ACCDET_IRQ_ADDR);
	acc_sts = pmic_read(PMIC_ACCDET_MEM_IN_ADDR);
	eint_sts = pmic_read(PMIC_ACCDET_EINT0_MEM_IN_ADDR);

	if ((irq_status & ACCDET_IRQ_B0) && (eintID == 0)) {
		clear_accdet_int();
		accdet_queue_work();
		clear_accdet_int_check();
#ifdef CONFIG_ACCDET_EINT_IRQ
	} else if (eintID != NO_PMIC_EINT) {
		pr_info("%s() IRQ:0x%x, eint-%s trig. cur_eint_state:%d\n",
		__func__, irq_status,
		(eintID == PMIC_EINT0)?"0":((eintID == PMIC_EINT1)?"1":"BI"),
		cur_eint_state);

		/* check EINT0 status */
		gmoistureID = pmic_read_mbit(PMIC_ACCDET_EINT0_MEM_IN_ADDR,
			PMIC_ACCDET_EINT0_MEM_IN_SHIFT,
			PMIC_ACCDET_EINT0_MEM_IN_MASK);
#ifdef CONFIG_ACCDET_EINT_IRQ
		if (get_moisture_det_en() == 0x1) {
			/* adjust moisture digital/analog setting */
			ret = adjust_moisture_setting(gmoistureID, eintID);
			if (ret == M_NO_ACT) {
#if PMIC_ACCDET_CTP
				dump_register();
#endif
				return;
			}
		} else {
			/* adjust eint digital/analog setting */
			adjust_eint_setting(gmoistureID, eintID);
		}
#endif
		clear_accdet_eint(eintID);
		clear_accdet_eint_check(eintID);
		pmic_eint_queue_work(eintID);

#endif
		} else {
			pr_info("%s no interrupt detected!\n", __func__);
		}
#if PMIC_ACCDET_CTP || PMIC_ACCDET_DEBUG
		dump_register();
#endif
}

static void accdet_int_handler(void)
{
	pr_debug("%s()\n", __func__);
	accdet_irq_handle();
}

#ifdef CONFIG_ACCDET_EINT_IRQ
static void accdet_eint_handler(void)
{
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

		if (accdet_dts.moisture_detect_mode != 0x5) {
			mod_timer(&micbias_timer,
				jiffies + MICBIAS_DISABLE_TIMER);
		}

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
		dev_notice(&platform_device->dev,
			"deflt pinctrl not found, skip it\n");
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
#ifdef CONFIG_MTK_PMIC_WRAP
	struct device_node *tmpnode, *accdet_node;
#endif
#if PMIC_ACCDET_KERNEL
	int ret;
	struct device_node *node = NULL;
	int pwm_deb[15];
#ifdef CONFIG_FOUR_KEY_HEADSET
	int four_key[5];
#else
	int three_key[4];
#endif

	pr_debug("%s\n", __func__);
#ifdef CONFIG_MTK_PMIC_WRAP
	tmpnode = of_find_compatible_node(NULL, NULL, "mediatek,pwraph");
	accdet_node = of_parse_phandle(tmpnode, "mediatek,pwrap-regmap", 0);
	if (accdet_node) {
		accdet_regmap = pwrap_node_to_regmap(accdet_node);
		if (IS_ERR(accdet_regmap)) {
			pr_notice("%s %d Error.\n", __func__, __LINE__);
			return PTR_ERR(accdet_regmap);
		}
	} else {
		pr_notice("%s %d Error.\n", __func__, __LINE__);
		return -EINVAL;
	}
#endif
	node = of_find_matching_node(node, accdet_of_match);
	if (!node) {
		pr_notice("%s can't find compatible dts node\n", __func__);
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
	ret = of_property_read_u32(node, "moisture-water-r", &water_r);
	if (ret) {
		/* no moisture detection */
		water_r = 0x0;
	}
	if (accdet_dts.moisture_use_ext_res == 0x1) {
		of_property_read_u32(node, "moisture-external-r",
			&moisture_ext_r);
		pr_info("Moisture_EXT support water_r=%d, ext_r=%d\n",
		     water_r, moisture_ext_r);
	} else if (accdet_dts.moisture_use_ext_res == 0x0) {
		of_property_read_u32(node, "moisture-internal-r",
			&moisture_int_r);
		pr_info("Moisture_INT support water_r=%d, int_r=%d\n",
		     water_r, moisture_int_r);
	}
	of_property_read_u32(node, "accdet-mic-vol", &accdet_dts.mic_vol);
	of_property_read_u32(node, "accdet-plugout-debounce",
			&accdet_dts.plugout_deb);
	of_property_read_u32(node, "accdet-mic-mode", &accdet_dts.mic_mode);

	pr_info("accdet mic_vol=%d, plugout_deb=%d mic_mode=%d\n",
	     accdet_dts.mic_vol, accdet_dts.plugout_deb,
	     accdet_dts.mic_mode);

#ifdef CONFIG_FOUR_KEY_HEADSET
	ret = of_property_read_u32_array(node, "headset-four-key-threshold",
		four_key, ARRAY_SIZE(four_key));
	if (!ret)
		memcpy(&accdet_dts.four_key, four_key+1,
				sizeof(struct four_key_threshold));
	else {
		pr_info("accdet get 4-key-thrsh dts fail, use efuse\n");
		accdet_get_efuse_4key();
	}

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

	cust_pwm_deb = &accdet_dts.pwm_deb;
#else
	accdet_dts.mic_vol = mic_vol;
	accdet_dts.mic_mode = mic_mode;
	accdet_dts.three_key.mid = 49;
	accdet_dts.three_key.up = 220;
	accdet_dts.three_key.down = 600;
	accdet_dts.pwm_deb.pwm_width = 0x500;
	accdet_dts.pwm_deb.pwm_thresh = 0x500;
	accdet_dts.pwm_deb.fall_delay = 0x1;
	accdet_dts.pwm_deb.rise_delay = 0x1f0;
	accdet_dts.pwm_deb.debounce0 = 0x800;
	accdet_dts.pwm_deb.debounce1 = 0x800;
	accdet_dts.pwm_deb.debounce3 = 0x20;
	accdet_dts.pwm_deb.debounce4 = 0x44;
	accdet_dts.pwm_deb.eint_pwm_width = eint_pwm_width;/* 0x4; */
	accdet_dts.pwm_deb.eint_pwm_thresh = eint_pwm_thresh;/* 0x1; */
	 /*default:0xe*/
	accdet_dts.pwm_deb.eint_debounce0 = adjust_eint_debounce03;
	accdet_dts.pwm_deb.eint_debounce1 = adjust_eint_debounce12;
	accdet_dts.pwm_deb.eint_debounce2 = adjust_eint_debounce12;
	 /* default:0xe */
	accdet_dts.pwm_deb.eint_debounce3 = adjust_eint_debounce03;
	accdet_dts.pwm_deb.eint_inverter_debounce = eint_invert_debounce_index;
	/* if we need moisture detection feature or not */
	accdet_dts.moisture_detect_enable = moisture_detect_enable;
	/* select moisture detection mode,
	 * 1: EINT 1.0, 2: EINT1.1, 3: EINT2.0, 4: EINT2.1, 5: EINT2.1_OPPO
	 */
	if (accdet_dts.moisture_detect_enable == 0x1) {
		accdet_dts.eint_detect_mode = eint_detect_mode;
		accdet_dts.moisture_detect_mode = moisture_detect_mode;
	} else {
		accdet_dts.eint_detect_mode = eint_detect_mode;
		accdet_dts.moisture_detect_mode = moisture_detect_mode;
	}
	if ((accdet_dts.moisture_detect_enable == 0x1) &&
		(accdet_dts.moisture_detect_mode !=
		accdet_dts.eint_detect_mode)) {
		pr_info("DTS setting error, eint mode != moisture mode\n\r");
	}
	accdet_dts.eint_use_ext_res = eint_use_ext_res;
	accdet_dts.moisture_comp_vth = moisture_comp_vth; /* default 2.8v */
	/* default 880mv */
	accdet_dts.moisture_comp_vref2 = moisture_comp_vref2;
	/* use internal resister */
	accdet_dts.moisture_use_ext_res = moisture_use_ext_res;
	water_r = water_r_t;
	moisture_ext_r = moisture_ext_r_t;
	moisture_int_r = moisture_int_r;
	cust_pwm_deb = &accdet_dts.pwm_deb;

	cust_pwm_deb->debounce0 = debounce0_test[debounce_index];
	cust_pwm_deb->debounce1 = debounce1_test[debounce_index];
	cust_pwm_deb->debounce3 = debounce3_test[debounce_index];
	cust_pwm_deb->debounce4 = debounce4_test[debounce_index];

#endif /* end of #if PMIC_ACCDET_KERNEL */
	dis_micbias_done = false;
	pr_info("accdet pwm_width=0x%x, thresh=0x%x, fall=0x%x, rise=0x%x\n",
	     cust_pwm_deb->pwm_width, cust_pwm_deb->pwm_thresh,
	     cust_pwm_deb->fall_delay, cust_pwm_deb->rise_delay);
	pr_info("deb0=0x%x, deb1=0x%x, deb3=0x%x, deb4=0x%x\n",
	     cust_pwm_deb->debounce0, cust_pwm_deb->debounce1,
	     cust_pwm_deb->debounce3, cust_pwm_deb->debounce4);
	pr_info("e_pwm_width=0x%x, e_pwm_thresh=0x%x\n",
	     cust_pwm_deb->eint_pwm_width, cust_pwm_deb->eint_pwm_thresh);
	pr_info("e_deb0=0x%x, deb1=0x%x, deb2=0x%x, deb3=0x%x\n",
		cust_pwm_deb->eint_debounce0, cust_pwm_deb->eint_debounce1,
		cust_pwm_deb->eint_debounce2, cust_pwm_deb->eint_debounce3);
	pr_info("e_inv_deb=0x%x, mdet_en=0x%x, e_det_m=0x%x, m_det_m=0x%x\n",
		cust_pwm_deb->eint_inverter_debounce,
		accdet_dts.moisture_detect_enable,
		accdet_dts.eint_detect_mode,
		accdet_dts.moisture_detect_mode);
	pr_info("m_vth=0x%x, m_vref2=0x%x, e_e_res=0x%x, m_e_res=0x%x\n",
		accdet_dts.moisture_comp_vth,
		accdet_dts.moisture_comp_vref2,
		accdet_dts.eint_use_ext_res,
		accdet_dts.moisture_use_ext_res);

	return 0;
}

#ifdef CONFIG_ACCDET_EINT_IRQ
static void config_digital_moisture_init_by_mode(void)
{
	/* enable eint cmpmem pwm */
	pmic_write(PMIC_ACCDET_EINT_CMPMEN_PWM_THRESH_ADDR,
		(accdet_dts.pwm_deb.eint_pwm_width << 4 |
		accdet_dts.pwm_deb.eint_pwm_thresh));
	/* DA signal stable */
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
	pmic_write(PMIC_ACCDET_DA_STABLE_ADDR, ACCDET_EINT0_STABLE_VAL);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
	pmic_write(PMIC_ACCDET_DA_STABLE_ADDR, ACCDET_EINT1_STABLE_VAL);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
	pmic_write(PMIC_ACCDET_DA_STABLE_ADDR, ACCDET_EINT0_STABLE_VAL);
	pmic_write(PMIC_ACCDET_DA_STABLE_ADDR, ACCDET_EINT1_STABLE_VAL);
#endif
	if (accdet_dts.moisture_detect_mode == 0x5) {
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
		/* clr DA stable signal */
		pmic_write_clr(PMIC_ACCDET_DA_STABLE_ADDR,
			PMIC_ACCDET_EINT0_CEN_STABLE_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
		/* clr DA stable signal */
		pmic_write_clr(PMIC_ACCDET_DA_STABLE_ADDR,
			PMIC_ACCDET_EINT1_CEN_STABLE_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
		/* clr DA stable signal */
		pmic_write_clr(PMIC_ACCDET_DA_STABLE_ADDR,
			PMIC_ACCDET_EINT0_CEN_STABLE_SHIFT);
		/* clr DA stable signal */
		pmic_write_clr(PMIC_ACCDET_DA_STABLE_ADDR,
			PMIC_ACCDET_EINT1_CEN_STABLE_SHIFT);
#endif
	}
	/* after receive n+1 number, interrupt issued. now is 2 times */
	pmic_write_set(PMIC_ACCDET_EINT_M_PLUG_IN_NUM_ADDR,
	PMIC_ACCDET_EINT_M_PLUG_IN_NUM_SHIFT);
	/* setting HW mode, enable digital fast discharge
	 * if use EINT0 & EINT1 detection, please modify
	 * PMIC_ACCDET_HWMODE_EN_ADDR[2:1]
	 */
	if (accdet_dts.moisture_detect_mode == 0x1) {
		/* disable moisture detection function */
		pmic_write_clr(PMIC_ACCDET_EINT_M_DETECT_EN_ADDR,
			PMIC_ACCDET_EINT_M_DETECT_EN_SHIFT);
		/* wk1, disable hwmode */
		pmic_write(PMIC_ACCDET_HWMODE_EN_ADDR, 0x0);
		/* disable inverter detection */
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
		pmic_write_set(PMIC_ACCDET_EINT0_SW_EN_ADDR,
			PMIC_ACCDET_EINT0_SW_EN_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
		pmic_write_set(PMIC_ACCDET_EINT1_SW_EN_ADDR,
			PMIC_ACCDET_EINT1_SW_EN_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
		pmic_write_set(PMIC_ACCDET_EINT0_SW_EN_ADDR,
			PMIC_ACCDET_EINT0_SW_EN_SHIFT);
		pmic_write_set(PMIC_ACCDET_EINT1_SW_EN_ADDR,
			PMIC_ACCDET_EINT1_SW_EN_SHIFT);
#endif
	} else if (accdet_dts.moisture_detect_mode == 0x2) {
		/* disable moisture detection function */
		pmic_write_clr(PMIC_ACCDET_EINT_M_DETECT_EN_ADDR,
			PMIC_ACCDET_EINT_M_DETECT_EN_SHIFT);
		/* wk1, disable hwmode */
		pmic_write(PMIC_ACCDET_HWMODE_EN_ADDR, 0x100);
	} else if (accdet_dts.moisture_detect_mode == 0x3) {
		/* disable moisture detection function */
		pmic_write_clr(PMIC_ACCDET_EINT_M_DETECT_EN_ADDR,
			PMIC_ACCDET_EINT_M_DETECT_EN_SHIFT);
		pmic_write_set(PMIC_ACCDET_EINT_CMPMOUT_SEL_ADDR,
			PMIC_ACCDET_EINT_CMPMOUT_SEL_SHIFT);
		pmic_write_set(PMIC_ACCDET_EINT_CMPMEN_SEL_ADDR,
			PMIC_ACCDET_EINT_CMPMEN_SEL_SHIFT);
		/* wk1, disable hwmode */
		pmic_write(PMIC_ACCDET_HWMODE_EN_ADDR, 0x100);
	} else if (accdet_dts.moisture_detect_mode == 0x4) {
		/* enable moisture detection function */
		pmic_write_set(PMIC_ACCDET_EINT_M_DETECT_EN_ADDR,
			PMIC_ACCDET_EINT_M_DETECT_EN_SHIFT);
		/* wk1, disable hwmode */
		pmic_write(PMIC_ACCDET_HWMODE_EN_ADDR, 0x100);
	} else if (accdet_dts.moisture_detect_mode == 0x5) {
		/* blocking CTURBO */
		pmic_write_set(PMIC_ACCDET_EINT_CTURBO_SEL_ADDR,
			PMIC_ACCDET_EINT_CTURBO_SEL_SHIFT);
		/* enable moisture detection function */
		pmic_write_set(PMIC_ACCDET_EINT_M_DETECT_EN_ADDR,
			PMIC_ACCDET_EINT_M_DETECT_EN_SHIFT);

		/* wk1, disable hwmode */
		pmic_write(PMIC_ACCDET_HWMODE_EN_ADDR, 0x500);
		pmic_write_set(PMIC_RG_HPLOUTPUTSTBENH_VAUDP32_ADDR,
			PMIC_RG_HPLOUTPUTSTBENH_VAUDP32_SHIFT);
		pmic_write_set(PMIC_RG_HPROUTPUTSTBENH_VAUDP32_ADDR,
			PMIC_RG_HPROUTPUTSTBENH_VAUDP32_SHIFT);
	} else {
		/* wk1, disable hwmode */
		pmic_write(PMIC_ACCDET_HWMODE_EN_ADDR, 0x100);
	}

	if (accdet_dts.moisture_detect_enable == 0) {
		pr_info("%s() disable digital moisture.\n", __func__);
		/* disable moisture detection function */
		pmic_write_clr(PMIC_ACCDET_EINT_M_DETECT_EN_ADDR,
			PMIC_ACCDET_EINT_M_DETECT_EN_SHIFT);
	}
	/* enable PWM */
	pmic_write(PMIC_ACCDET_CMP_PWM_EN_ADDR, 0x67);
	/* enable inverter detection */
	if (accdet_dts.eint_detect_mode == 0x1) {
		/* disable inverter detection */
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
		pmic_write_clr(PMIC_ACCDET_EINT0_INVERTER_SW_EN_ADDR,
			PMIC_ACCDET_EINT0_INVERTER_SW_EN_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
		pmic_write_clr(PMIC_ACCDET_EINT1_INVERTER_SW_EN_ADDR,
			PMIC_ACCDET_EINT1_INVERTER_SW_EN_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
		pmic_write_clr(PMIC_ACCDET_EINT0_INVERTER_SW_EN_ADDR,
			PMIC_ACCDET_EINT0_INVERTER_SW_EN_SHIFT);
		pmic_write_clr(PMIC_ACCDET_EINT1_INVERTER_SW_EN_ADDR,
			PMIC_ACCDET_EINT1_INVERTER_SW_EN_SHIFT);
#endif
	} else {
		/* enable inverter detection */
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
		pmic_write_set(PMIC_ACCDET_EINT0_INVERTER_SW_EN_ADDR,
			PMIC_ACCDET_EINT0_INVERTER_SW_EN_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
		pmic_write_set(PMIC_ACCDET_EINT1_INVERTER_SW_EN_ADDR,
			PMIC_ACCDET_EINT1_INVERTER_SW_EN_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
		pmic_write_set(PMIC_ACCDET_EINT0_INVERTER_SW_EN_ADDR,
			PMIC_ACCDET_EINT0_INVERTER_SW_EN_SHIFT);
		pmic_write_set(PMIC_ACCDET_EINT1_INVERTER_SW_EN_ADDR,
			PMIC_ACCDET_EINT1_INVERTER_SW_EN_SHIFT);
#endif
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
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
	pmic_write_set(PMIC_RG_EINT0EN_ADDR, PMIC_RG_EINT0EN_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
	pmic_write_set(PMIC_RG_EINT1EN_ADDR, PMIC_RG_EINT1EN_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
	pmic_write_set(PMIC_RG_EINT0EN_ADDR, PMIC_RG_EINT0EN_SHIFT);
	pmic_write_set(PMIC_RG_EINT1EN_ADDR, PMIC_RG_EINT1EN_SHIFT);
#endif
	/* ESD switches on */
	pmic_write_set(PMIC_RG_ACCDETSPARE_ADDR, 8);
	/* before playback, set NCP pull low before nagative voltage */
	pmic_write_set(PMIC_RG_NCP_PDDIS_EN_ADDR, PMIC_RG_NCP_PDDIS_EN_SHIFT);

	if ((accdet_dts.eint_detect_mode == 0x1) ||
		(accdet_dts.eint_detect_mode == 0x2) ||
		(accdet_dts.eint_detect_mode == 0x3)) {

		if (accdet_dts.eint_use_ext_res == 0x1) {
			/* select VTH to 2v and 500k, use external resitance */
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
			/* disable RG_EINT0CONFIGACCDET */
			pmic_write_clr(PMIC_RG_EINT0CONFIGACCDET_ADDR,
				PMIC_RG_EINT0CONFIGACCDET_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
			/* disable RG_EINT1CONFIGACCDET */
			pmic_write_clr(PMIC_RG_EINT1CONFIGACCDET_ADDR,
				PMIC_RG_EINT1CONFIGACCDET_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
			/* disable RG_EINT0CONFIGACCDET */
			pmic_write_clr(PMIC_RG_EINT0CONFIGACCDET_ADDR,
				PMIC_RG_EINT0CONFIGACCDET_SHIFT);
			/* disable RG_EINT1CONFIGACCDET */
			pmic_write_clr(PMIC_RG_EINT1CONFIGACCDET_ADDR,
				PMIC_RG_EINT1CONFIGACCDET_SHIFT);
#endif
		} else {
			/* select VTH to 2v and 500k, use internal resitance */
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
			/* enable RG_EINT0CONFIGACCDET */
			pmic_write_set(PMIC_RG_EINT0CONFIGACCDET_ADDR,
				PMIC_RG_EINT0CONFIGACCDET_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
			/* enable RG_EINT1CONFIGACCDET */
			pmic_write_set(PMIC_RG_EINT1CONFIGACCDET_ADDR,
				PMIC_RG_EINT1CONFIGACCDET_SHIFT);
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
			/* enable RG_EINT0CONFIGACCDET */
			pmic_write_set(PMIC_RG_EINT0CONFIGACCDET_ADDR,
				PMIC_RG_EINT0CONFIGACCDET_SHIFT);
			/* enable RG_EINT1CONFIGACCDET */
			pmic_write_set(PMIC_RG_EINT1CONFIGACCDET_ADDR,
				PMIC_RG_EINT1CONFIGACCDET_SHIFT);
#endif
		}
	} else if (accdet_dts.eint_detect_mode == 0x4) {
		/* do nothing */
	} else if (accdet_dts.eint_detect_mode == 0x5) {
		/* do nothing */
	}

	if (accdet_dts.eint_detect_mode != 0x1) {
		/* current detect set 0.25uA */
		pmic_write_mset(PMIC_RG_ACCDETSPARE_ADDR,
			PMIC_RG_ACCDETSPARE_SHIFT,
			0x3, 0x3);
	}
	/* new customized parameter */
	if ((accdet_dts.eint_use_ext_res == 0x2) ||
		(accdet_dts.eint_use_ext_res == 0x4)) {
		/* select VTH to 2v */
		pmic_write_mset(PMIC_RG_EINTCOMPVTH_ADDR,
			PMIC_RG_EINTCOMPVTH_SHIFT, PMIC_RG_EINTCOMPVTH_MASK,
			0x2);
	}
}
#endif /* end of CONFIG_ACCDET_EINT_IRQ */

static void accdet_init_once(void)
{
	unsigned int reg = 0;

	/* reset the accdet unit */
	pmic_write_set(PMIC_RG_ACCDET_RST_ADDR, PMIC_RG_ACCDET_RST_SHIFT);
	pmic_write_clr(PMIC_RG_ACCDET_RST_ADDR, PMIC_RG_ACCDET_RST_SHIFT);

	/* clear high micbias1 voltage setting */
	pmic_write_mclr(PMIC_RG_AUDPWDBMICBIAS1_ADDR,
		PMIC_RG_AUDMICBIAS1HVEN_SHIFT, 0x3);
	/* clear micbias1 voltage */
	pmic_write_mclr(PMIC_RG_AUDPWDBMICBIAS1_ADDR,
		PMIC_RG_AUDMICBIAS1VREF_SHIFT, 0x7);

	/* init pwm frequency, duty & rise/falling delay */
	pmic_write(PMIC_ACCDET_PWM_WIDTH_ADDR,
		REGISTER_VAL(cust_pwm_deb->pwm_width));
	pmic_write(PMIC_ACCDET_PWM_THRESH_ADDR,
		REGISTER_VAL(cust_pwm_deb->pwm_thresh));
	pmic_write(PMIC_ACCDET_RISE_DELAY_ADDR,
		  (cust_pwm_deb->fall_delay << 15 | cust_pwm_deb->rise_delay));

	/* config micbias voltage, micbias1 vref is only controlled by accdet
	 *  if we need 2.8V, config [12:13]
	 */
	reg = pmic_read(PMIC_RG_AUDPWDBMICBIAS1_ADDR);
	if (accdet_dts.mic_vol <= 7) {
		/* micbias1 <= 2.7V */
		pmic_write(PMIC_RG_AUDPWDBMICBIAS1_ADDR,
		reg | (accdet_dts.mic_vol<<PMIC_RG_AUDMICBIAS1VREF_SHIFT) |
		RG_AUD_MICBIAS1_LOWP_EN);
	} else if (accdet_dts.mic_vol == 8) {
		/* micbias1 = 2.8v */
		pmic_write(PMIC_RG_AUDPWDBMICBIAS1_ADDR,
			reg | (3<<PMIC_RG_AUDMICBIAS1HVEN_SHIFT) |
			RG_AUD_MICBIAS1_LOWP_EN);
	} else if (accdet_dts.mic_vol == 9) {
		/* micbias1 = 2.85v */
		pmic_write(PMIC_RG_AUDPWDBMICBIAS1_ADDR,
			reg | (1<<PMIC_RG_AUDMICBIAS1HVEN_SHIFT) |
			RG_AUD_MICBIAS1_LOWP_EN);
	}
	/* mic mode setting */
	reg = pmic_read(PMIC_RG_AUDACCDETMICBIAS0PULLLOW_ADDR);
	if (accdet_dts.mic_mode == HEADSET_MODE_1) {
		/* ACC mode*/
		pmic_write(PMIC_RG_AUDACCDETMICBIAS0PULLLOW_ADDR,
			reg | RG_ACCDET_MODE_ANA11_MODE1);
		/* enable analog fast discharge */
		pmic_write_set(PMIC_RG_ANALOGFDEN_ADDR,
			PMIC_RG_ANALOGFDEN_SHIFT);
		pmic_write_mset(PMIC_RG_ACCDETSPARE_ADDR, 11, 0x3, 0x3);
	} else if (accdet_dts.mic_mode == HEADSET_MODE_2) {
		/* DCC mode Low cost mode without internal bias*/
		pmic_write(PMIC_RG_AUDACCDETMICBIAS0PULLLOW_ADDR,
			reg | RG_ACCDET_MODE_ANA11_MODE2);
		/* enable analog fast discharge */
		pmic_write_mset(PMIC_RG_ANALOGFDEN_ADDR,
			PMIC_RG_ANALOGFDEN_SHIFT, 0x3, 0x3);
	} else if (accdet_dts.mic_mode == HEADSET_MODE_6) {
		/* DCC mode Low cost mode with internal bias,
		 * bit8 = 1 to use internal bias
		 */
		pmic_write(PMIC_RG_AUDACCDETMICBIAS0PULLLOW_ADDR,
			reg | RG_ACCDET_MODE_ANA11_MODE6);
		pmic_write_set(PMIC_RG_AUDPWDBMICBIAS1_ADDR,
				PMIC_RG_AUDMICBIAS1DCSW1PEN_SHIFT);
		/* enable analog fast discharge */
		pmic_write_mset(PMIC_RG_ANALOGFDEN_ADDR,
			PMIC_RG_ANALOGFDEN_SHIFT, 0x3, 0x3);
	}

#ifdef CONFIG_ACCDET_EINT_IRQ
	config_eint_init_by_mode();
#endif

#ifdef CONFIG_ACCDET_EINT_IRQ
	if (accdet_dts.moisture_detect_enable == 1) {
		pr_info("%s() set analog moisture.\n", __func__);
		config_analog_moisture_init_by_mode();
	}

	config_digital_moisture_init_by_mode();
#endif
#ifdef CONFIG_ACCDET_EINT
	/* set pull low pads and DCC mode */
	pmic_write(PMIC_RG_AUDACCDETMICBIAS0PULLLOW_ADDR, 0x8F);
	/* disconnect configaccdet */
	pmic_write(PMIC_RG_EINT1CONFIGACCDET_ADDR, 0x0);
	/* disable eint comparator */
	pmic_write(PMIC_RG_EINT0CMPEN_ADDR, 0x0);
	/* enable PWM */
	pmic_write(PMIC_ACCDET_CMP_PWM_EN_ADDR, 0x7);
	/* enable accdet sw mode */
	pmic_write(PMIC_ACCDET_HWMODE_EN_ADDR, 0x0);
	/* set DA signal to stable */
	pmic_write(PMIC_ACCDET_DA_STABLE_ADDR, 0x1);
	/* disable eint/inverter/sw_en */
	pmic_write(PMIC_ACCDET_SW_EN_ADDR, 0x0);
#endif
	pr_info("%s() done.\n", __func__);
#if PMIC_ACCDET_DEBUG
	dump_register();
#endif
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
	/* add new of DE for fix icon cann't appear */
#if PMIC_ACCDET_KERNEL
	/* set and clear initial bit every eint interrutp */
	pmic_write_set(PMIC_ACCDET_SEQ_INIT_ADDR, PMIC_ACCDET_SEQ_INIT_SHIFT);
	mdelay(2);
	pmic_write_clr(PMIC_ACCDET_SEQ_INIT_ADDR, PMIC_ACCDET_SEQ_INIT_SHIFT);
	mdelay(1);
#endif
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
	if (pmic_read(PMIC_SWCID_ADDR) == 0x5910) {
		pr_info("accdet not supported\r");
	} else {
		pr_info("%s()  now init accdet!\n", __func__);
#if PMIC_ACCDET_KERNEL
		if (atomic_cmpxchg(&accdet_first, 1, 0)) {
			del_timer_sync(&accdet_init_timer);
#else
		if (true) {
			accdet_get_dts_data();
			accdet_get_efuse();
#endif
			accdet_init();
			accdet_init_debounce();
			/* just need run once */
			accdet_init_once();
		} else
			pr_info("%s inited dts fail\n", __func__);
	}
}

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
		eintID = pmic_read_mbit(PMIC_ACCDET_EINT0_MEM_IN_ADDR,
			PMIC_ACCDET_EINT0_MEM_IN_SHIFT,
			PMIC_ACCDET_EINT0_MEM_IN_MASK);
		if (eintID == M_PLUG_OUT) {
			pr_info("%s Plug-out, no dis micbias\n", __func__);
			return;
		}
cur_AB = pmic_read(PMIC_ACCDET_MEM_IN_ADDR) >> ACCDET_STATE_MEM_IN_OFFSET;
		cur_AB = cur_AB & ACCDET_STATE_AB_MASK;

		/* if 3pole disable accdet
		 * if <20k + 4pole, disable accdet will disable accdet
		 * plug out interrupt. The behavior will same as 3pole
		 */
		if (cable_type == HEADSET_MIC) {
			/* do nothing */
		} else if ((cable_type == HEADSET_NO_MIC) ||
			(cur_AB == ACCDET_STATE_AB_00) ||
			(cur_AB == ACCDET_STATE_AB_11)) {
			/* disable accdet_sw_en=0
			 * disable accdet_hwmode_en=0
			 */
			pmic_write_clr(PMIC_ACCDET_SW_EN_ADDR,
				PMIC_ACCDET_SW_EN_SHIFT);
			disable_accdet();
			pr_info("%s MICBIAS:Disabled AB:0x%x c_type:0x%x\n",
				__func__, cur_AB, cable_type);
			dis_micbias_done = true;
		}
		/* disable comp1 delay window */
		pmic_write_set(PMIC_RG_EINT0NOHYS_ADDR,
			PMIC_RG_EINT0NOHYS_SHIFT);
		/* connect VREF2 to EINT0CMP */
		pmic_write_mset(PMIC_RG_EINTCOMPVTH_ADDR,
			PMIC_RG_EINTCOMPVTH_SHIFT, 0x3, 0x3);
		pr_info("%s [0x%x]=0x%x [0x%x]=0x%x\n", __func__,
			PMIC_RG_EINT0NOHYS_ADDR,
			pmic_read(PMIC_RG_EINT0NOHYS_ADDR),
			PMIC_RG_EINTCOMPVTH_ADDR,
			pmic_read(PMIC_RG_EINTCOMPVTH_ADDR));
	}
}
#if PMIC_ACCDET_KERNEL
EXPORT_SYMBOL(accdet_late_init);

static void delay_init_timerhandler(struct timer_list *t)
{
	if (pmic_read(PMIC_SWCID_ADDR) == 0x5910) {
		pr_info("accdet not supported\r");
	} else {
		pr_info("%s()  now init accdet!\n", __func__);
		if (atomic_cmpxchg(&accdet_first, 1, 0)) {
			accdet_init();
			accdet_init_debounce();
			accdet_init_once();
		} else
			pr_info("%s inited dts fail\n", __func__);
	}
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

	/* modify timer api for kernel 4.19 */
	timer_setup(&micbias_timer, dis_micbias_timerhandler, 0);
	timer_setup(&accdet_init_timer, delay_init_timerhandler, 0);
	micbias_timer.expires = jiffies + MICBIAS_DISABLE_TIMER;
	accdet_init_timer.expires = jiffies + ACCDET_INIT_WAIT_TIMER;
	/* the third argument may include TIMER_* flags */

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

	/* open top accdet interrupt */
	pmic_enable_interrupt(INT_ACCDET, 1, "ACCDET");
#ifdef CONFIG_ACCDET_EINT_IRQ
#ifdef CONFIG_ACCDET_SUPPORT_EINT0
	/* open top interrupt eint0 */
	pmic_enable_interrupt(INT_ACCDET_EINT0, 1, "ACCDET_EINT0");
#elif defined CONFIG_ACCDET_SUPPORT_EINT1
	/* open top interrupt eint1 */
	pmic_enable_interrupt(INT_ACCDET_EINT1, 1, "ACCDET_EINT1");
#elif defined CONFIG_ACCDET_SUPPORT_BI_EINT
	/* open top interrupt eint0 & eint1 */
	pmic_enable_interrupt(INT_ACCDET_EINT0, 1, "ACCDET_EINT0");
	pmic_enable_interrupt(INT_ACCDET_EINT1, 1, "ACCDET_EINT1");
#endif
#endif
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
#endif /* end of #if PMIC_ACCDET_KERNEL */

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
