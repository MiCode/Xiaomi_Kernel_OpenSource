/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

/*****************************************************************************
 *
 * Filename:
 * ---------
 *    mtk_battery.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 * This Module defines functions of the Anroid Battery service for
 * updating the battery status
 *
 * Author:
 * -------
 * Weiching Lin
 *
 ****************************************************************************/
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/wait.h>		/* For wait queue*/
#include <linux/sched.h>	/* For wait queue*/
#include <linux/kthread.h>	/* For Kthread_run */
#include <linux/platform_device.h>	/* platform device */
#include <linux/time.h>

#include <linux/netlink.h>	/* netlink */
#include <linux/kernel.h>
#include <linux/socket.h>	/* netlink */
#include <linux/skbuff.h>	/* netlink */
#include <net/sock.h>		/* netlink */
#include <linux/cdev.h>		/* cdev */

#include <linux/err.h>	/* IS_ERR, PTR_ERR */
#include <linux/reboot.h>	/*kernel_power_off*/
#include <linux/proc_fs.h>
#include <linux/of_fdt.h>	/*of_dt API*/
#include <linux/of_platform.h>  /*of_find_node_by_name*/
#include <linux/irq.h>
#include <linux/irqdesc.h>	/*irq_to_desc*/
#include <linux/mfd/mt6358/core.h> /*mt6358_irq_get_virq*/
#include <linux/vmalloc.h>


#include <linux/math64.h> /*div_s64*/

#include <mt-plat/aee.h>
#include <mt-plat/v1/charger_type.h>
#include <mt-plat/v1/mtk_charger.h>
#include <mt-plat/v1/mtk_battery.h>
#include <mt-plat/mtk_boot.h>

#include <mtk_gauge_class.h>
#include "mtk_battery_internal.h"
#include <mach/mtk_battery_property.h>
#include <mtk_gauge_time_service.h>
#include <mt-plat/upmu_common.h>
#include <pmic_lbat_service.h>



/* ============================================================ */
/* define */
/* ============================================================ */
#define NETLINK_FGD 26


/************ adc_cali *******************/
#define ADC_CALI_DEVNAME "MT_pmic_adc_cali"
#define TEST_ADC_CALI_PRINT _IO('k', 0)
#define SET_ADC_CALI_Slop _IOW('k', 1, int)
#define SET_ADC_CALI_Offset _IOW('k', 2, int)
#define SET_ADC_CALI_Cal _IOW('k', 3, int)
#define ADC_CHANNEL_READ _IOW('k', 4, int)
#define BAT_STATUS_READ _IOW('k', 5, int)
#define Set_Charger_Current _IOW('k', 6, int)
/* add for meta tool----------------------------------------- */
#define Get_META_BAT_VOL _IOW('k', 10, int)
#define Get_META_BAT_SOC _IOW('k', 11, int)
#define Get_META_BAT_CAR_TUNE_VALUE _IOW('k', 12, int)
#define Set_META_BAT_CAR_TUNE_VALUE _IOW('k', 13, int)
#define Set_BAT_DISABLE_NAFG _IOW('k', 14, int)
#define Set_CARTUNE_TO_KERNEL _IOW('k', 15, int)
/* add for meta tool----------------------------------------- */

static struct class *adc_cali_class;
static int adc_cali_major;
static dev_t adc_cali_devno;
static struct cdev *adc_cali_cdev;

static int adc_cali_slop[14] = {
	1000, 1000, 1000, 1000, 1000, 1000,
	1000, 1000, 1000, 1000, 1000, 1000,
	1000, 1000
};
static int adc_cali_offset[14] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static int adc_cali_cal[1] = { 0 };
static int battery_in_data[1] = { 0 };
static int battery_out_data[1] = { 0 };
static bool g_ADC_Cali;

static enum power_supply_property battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
};

/* boot mode */
struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

/* weak function */
int __attribute__ ((weak))
	do_ptim_gauge(
	bool isSuspend, unsigned int *bat, signed int *cur, bool *is_charging)
{
	return 0;
}

int __attribute__ ((weak))
	get_rac(void)
{
	return 0;
}

int __attribute__ ((weak))
	get_imix(void)
{
	return 0;
}

void __attribute__ ((weak))
	battery_dump_nag(void)
{
}

enum charger_type __attribute__ ((weak))
	mt_get_charger_type(void)
{
	return CHARGER_UNKNOWN;
}

int __attribute__ ((weak))
	charger_manager_get_zcv(
	struct charger_consumer *consumer, int idx, u32 *uV)
{
	return 0;
}

struct charger_consumer __attribute__ ((weak))
	*charger_manager_get_by_name(struct device *dev,
	const char *supply_name)
{
	return NULL;
}

int __attribute__ ((weak))
	register_charger_manager_notifier(struct charger_consumer *consumer,
	struct notifier_block *nb)
{
	return 0;
}

void __attribute__ ((weak)) enable_bat_temp_det(bool en)
{
	pr_notice("[%s] not support!\n", __func__);
}

unsigned int __attribute__ ((weak)) mt6358_irq_get_virq(struct device *dev,
							unsigned int hwirq)
{
	pr_notice_once("%s: API not ready!\n", __func__);
	return 0;
}

/* weak function end */


int gauge_enable_interrupt(int intr_number, int en)
{
	struct irq_desc *desc;
	unsigned int depth = 0;
	unsigned int irq = 0;

	if (intr_number == INT_ENUM_MAX) {
		bm_debug("[%s] intr_number = INT_ENUM_MAX, no interrupt, do nothing %d\n",
			__func__, INT_ENUM_MAX);
		return 0;
	}

	if (gm.pdev_node == NULL)
		gm.pdev_node = of_find_node_by_name(NULL, "mt-pmic");

	if (gm.pdevice == NULL)
		gm.pdevice = of_find_device_by_node(gm.pdev_node);

	if (gm.pmic_dev == NULL && gm.pdevice != NULL)
		gm.pmic_dev = &gm.pdevice->dev;

	if (gm.pmic_dev == NULL)
		return -EINVAL;

	irq = mt6358_irq_get_virq(gm.pmic_dev->parent, intr_number);

	desc = irq_to_desc(irq);

	if (!desc)
		return -EINVAL;


	mutex_lock(&gm.pmic_intr_mutex);
	depth = desc->depth;

	if (en == 1) {
		if (desc->depth == 1) {
			pmic_enable_interrupt(intr_number, en, "GM30");
			bm_debug("[%s]intr_number:%d %d en = %d, depth b[%d],a[%d]\n",
				__func__, intr_number, irq, en,
				depth, desc->depth);
		} else {
			/* do nothing */
			bm_debug("[%s]do nothing, intr_number:%d %d en = %d, depth:%d %d\n",
				__func__, intr_number, irq, en,
				depth, desc->depth);
		}
	} else if (en == 0) {
		if (desc->depth == 0) {
			pmic_enable_interrupt(intr_number, en, "GM30");
			bm_debug("[%s]intr_number:%d %d en = %d, depth b[%d],a[%d]\n",
				__func__, intr_number, irq, en,
				depth, desc->depth);
		} else {
			/* do nothing */
			bm_debug("[%s]do nothing, intr_number:%d %d en = %d, depth:%d %d\n",
				__func__, intr_number, irq, en,
				depth, desc->depth);
		}
	}
	mutex_unlock(&gm.pmic_intr_mutex);

	return 0;
}

bool is_battery_init_done(void)
{
	return gm.is_probe_done;
}

bool is_recovery_mode(void)
{
	int boot_mode = battery_get_boot_mode();

	if (is_fg_disabled())
		return false;

	bm_debug("mtk_battery boot mode =%d\n", boot_mode);
	if (boot_mode == RECOVERY_BOOT) {
		gm.log_level = BMLOG_DEBUG_LEVEL;
		fg_cust_data.daemon_log_level = BMLOG_DEBUG_LEVEL;
		return true;
	}

	return false;
}

int battery_get_boot_mode(void)
{
	struct device *dev = NULL;
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;
	int boot_mode = 11;//UNKNOWN_BOOT

	dev = gm.gdev->dev.parent;
	if (dev != NULL) {
		boot_node = of_parse_phandle(dev->of_node, "bootmode", 0);
		if (!boot_node) {
			bm_err("%s: failed to get boot mode phandle\n", __func__);
		} else {
			tag = (struct tag_bootmode *)of_get_property(boot_node,
								"atag,boot", NULL);
			if (!tag)
				bm_err("%s: failed to get atag,boot\n", __func__);
			else {
				boot_mode = tag->bootmode;
				gm.boot_mode = tag->bootmode;
			}
		}
	}
	bm_debug("%s: boot mode=%d\n", __func__, boot_mode);
	return boot_mode;
}

bool is_fg_disabled(void)
{
	return gm.disableGM30;
}


bool set_charge_power_sel(enum CHARGE_SEL select)
{
	/* select gm.charge_power_sel to CHARGE_NORMAL ,CHARGE_R1,CHARGE_R2 */
	/* example: gm.charge_power_sel = CHARGE_NORMAL */

	gm.charge_power_sel = select;

	wakeup_fg_algo_cmd(FG_INTR_KERNEL_CMD,
		FG_KERNEL_CMD_FORCE_BAT_TEMP, select);

	return 0;
}

int dump_pseudo100(enum CHARGE_SEL select)
{
	int i = 0;

	bm_err("%s:select=%d\n", __func__, select);

	if (select > MAX_CHARGE_RDC || select < 0)
		return 0;

	for (i = 0; i < MAX_TABLE; i++) {
		bm_err("%6d\n",
			fg_table_cust_data.fg_profile[i].r_pseudo100.pseudo[select]);
	}

	return 0;
}

int register_battery_notifier(struct notifier_block *nb)
{
	int ret = 0;

	mutex_lock(&gm.notify_mutex);
	ret = srcu_notifier_chain_register(&gm.gm_notify, nb);
	mutex_unlock(&gm.notify_mutex);

	return ret;
}

int unregister_battery_notifier(struct notifier_block *nb)
{
	int ret = 0;

	mutex_lock(&gm.notify_mutex);
	ret = srcu_notifier_chain_unregister(&gm.gm_notify, nb);
	mutex_unlock(&gm.notify_mutex);

	return ret;
}

int battery_notifier(int event)
{
	return srcu_notifier_call_chain(&gm.gm_notify, event, NULL);
}

/* ============================================================ */
/* functions for fg hal*/
/* ============================================================ */
void set_hw_ocv_unreliable(bool _flag_unreliable)
{
	gm.hw_status.flag_hw_ocv_unreliable
		= _flag_unreliable;
}


/* ============================================================ */
/* functions */
/* ============================================================ */

signed int battery_meter_get_tempR(signed int dwVolt)
{

	int TRes;

	TRes = 0;

	if (is_fg_disabled())
		return 0;

	TRes = (gm.rbat.rbat_pull_up_r * dwVolt) /
		(gm.rbat.rbat_pull_up_volt - dwVolt);
	return TRes;
}

signed int battery_meter_get_tempV(void)
{

	int val = 0;

	if (is_fg_disabled())
		return 0;

	val = pmic_get_v_bat_temp();
	return val;
}

signed int battery_meter_get_VSense(void)
{
	if (is_fg_disabled())
		return 0;
	else
		return pmic_get_ibus();
}

int check_cap_level(int uisoc)
{
	if (uisoc >= 100)
		return POWER_SUPPLY_CAPACITY_LEVEL_FULL;
	else if (uisoc >= 80 && uisoc < 100)
		return POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
	else if (uisoc >= 20 && uisoc < 80)
		return POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	else if (uisoc > 0 && uisoc < 20)
		return POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	else if (uisoc == 0)
		return POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	else
		return POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;
}

void battery_update_psd(struct battery_data *bat_data)
{
	bat_data->BAT_batt_vol = battery_get_bat_voltage();
	bat_data->BAT_batt_temp = battery_get_bat_temperature();
}

static int battery_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
	int ret = 0;
	int fgcurrent = 0;
	bool b_ischarging = 0;

	struct battery_data *data =
		container_of(psy->desc, struct battery_data, psd);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = data->BAT_STATUS;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = data->BAT_HEALTH;/* do not change before*/
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = data->BAT_PRESENT;/* do not change before*/
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = data->BAT_TECHNOLOGY;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = gm.bat_cycle;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		/* 1 = META_BOOT, 4 = FACTORY_BOOT 5=ADVMETA_BOOT */
		/* 6= ATE_factory_boot */
		if (gm.boot_mode == 1 || gm.boot_mode == 4
			|| gm.boot_mode == 5 || gm.boot_mode == 6) {
			val->intval = 75;
			break;
		}

		if (gm.fixed_uisoc != 0xffff)
			val->intval = gm.fixed_uisoc;
		else
			val->intval = data->BAT_CAPACITY;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		b_ischarging = gauge_get_current(&fgcurrent);
		if (b_ischarging == false)
			fgcurrent = 0 - fgcurrent;

		val->intval = fgcurrent * 100;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = battery_get_bat_avg_current() * 100;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval =
			fg_table_cust_data.fg_profile[gm.battery_id].q_max
			* 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = gm.ui_soc *
			fg_table_cust_data.fg_profile[gm.battery_id].q_max
			* 1000 / 100;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = data->BAT_batt_vol * 1000;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = gm.tbat_precise;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = check_cap_level(data->BAT_CAPACITY);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		/* full or unknown must return 0 */
		ret = check_cap_level(data->BAT_CAPACITY);
		if ((ret == POWER_SUPPLY_CAPACITY_LEVEL_FULL) ||
			(ret == POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN))
			val->intval = 0;
		else {
			int q_max_now = fg_table_cust_data.fg_profile[
						gm.battery_id].q_max;
			int remain_ui = 100 - data->BAT_CAPACITY;
			int remain_mah = remain_ui * q_max_now / 10;
			int time_to_full = 0;

			gauge_get_current(&fgcurrent);

			if (fgcurrent != 0)
				time_to_full = remain_mah * 3600 / fgcurrent;

			bm_debug("time_to_full:%d, remain:ui:%d mah:%d, fgcurrent:%d, qmax:%d\n",
				time_to_full, remain_ui, remain_mah,
				fgcurrent, q_max_now);

			val->intval = abs(time_to_full);
		}
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		if (check_cap_level(data->BAT_CAPACITY) ==
			POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN)
			val->intval = 0;
		else {
			int q_max_mah = 0;
			int q_max_uah = 0;

			q_max_mah =
				fg_table_cust_data.fg_profile[
				gm.battery_id].q_max / 10;

			q_max_uah = q_max_mah * 1000;
			if (q_max_uah <= 100000) {
				bm_debug("%s q_max_mah:%d q_max_uah:%d\n",
					__func__, q_max_mah, q_max_uah);
				q_max_uah = 100001;
			}
			val->intval = q_max_uah;
		}
		break;


	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* battery_data initialization */
struct battery_data battery_main = {
	.psd = {
		.name = "battery",
		.type = POWER_SUPPLY_TYPE_BATTERY,
		.properties = battery_props,
		.num_properties = ARRAY_SIZE(battery_props),
		.get_property = battery_get_property,
		},

	.BAT_STATUS = POWER_SUPPLY_STATUS_DISCHARGING,
	.BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD,
	.BAT_PRESENT = 1,
	.BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION,
	.BAT_CAPACITY = -1,
	.BAT_batt_vol = 0,
	.BAT_batt_temp = 0,
};

void evb_battery_init(void)
{
	battery_main.BAT_STATUS = POWER_SUPPLY_STATUS_FULL;
	battery_main.BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD;
	battery_main.BAT_PRESENT = 1;
	battery_main.BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION;
	battery_main.BAT_CAPACITY = 100;
	battery_main.BAT_batt_vol = 4200;
	battery_main.BAT_batt_temp = 22;
}

static void disable_fg(void)
{

	int fgv;

	fgv = gauge_get_hw_version();

	if (fgv >= GAUGE_HW_V1000
	&& fgv < GAUGE_HW_V2000) {
		en_intr_VBATON_UNDET(0);
	}

	gauge_enable_interrupt(FG_BAT1_INT_L_NO, 0);
	gauge_enable_interrupt(FG_BAT1_INT_H_NO, 0);

	gauge_enable_interrupt(FG_BAT0_INT_L_NO, 0);
	gauge_enable_interrupt(FG_BAT0_INT_H_NO, 0);

	gauge_enable_interrupt(FG_N_CHARGE_L_NO, 0);

	gauge_enable_interrupt(FG_IAVG_H_NO, 0);
	gauge_enable_interrupt(FG_IAVG_L_NO, 0);

	gauge_enable_interrupt(FG_ZCV_NO, 0);

	gauge_enable_interrupt(FG_BAT_PLUGOUT_NO, 0);
	gauge_enable_interrupt(FG_RG_INT_EN_NAG_C_DLTV, 0);

	gauge_enable_interrupt(FG_RG_INT_EN_BAT_TEMP_H, 0);
	gauge_enable_interrupt(FG_RG_INT_EN_BAT_TEMP_L, 0);

	gauge_enable_interrupt(FG_RG_INT_EN_BAT2_H, 0);
	gauge_enable_interrupt(FG_RG_INT_EN_BAT2_L, 0);
	gm.disableGM30 = 1;
	gm.ui_soc = 50;
	battery_main.BAT_CAPACITY = 50;
}

bool fg_interrupt_check(void)
{
	if (is_fg_disabled()) {
		disable_fg();
		return false;
	}
	return true;
}

void battery_update(struct battery_data *bat_data)
{
	struct power_supply *bat_psy = bat_data->psy;

	battery_update_psd(&battery_main);
	bat_data->BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION;
	bat_data->BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD;
	bat_data->BAT_PRESENT = 1;

#if defined(CONFIG_MTK_DISABLE_GAUGE)
	return;
#endif

	if (is_fg_disabled())
		bat_data->BAT_CAPACITY = 50;

	power_supply_changed(bat_psy);
}

bool is_kernel_power_off_charging(void)
{
	int boot_mode = battery_get_boot_mode();

	if (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
	    || boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
		return true;
	}
	return false;
}

int bat_get_debug_level(void)
{
	return gm.log_level;
}

/* ============================================================ */
/* function prototype */
/* ============================================================ */
static void nl_send_to_user(u32 pid, int seq, struct fgd_nl_msg_t *reply_msg);

struct fuel_gauge_custom_data fg_cust_data;
struct fuel_gauge_table_custom_data fg_table_cust_data;


/* ============================================================ */
/* extern function */
/* ============================================================ */
static void proc_dump_log(struct seq_file *m)
{
	seq_printf(m, "subcmd:%d para1:%d\n",
		gm.proc_subcmd, gm.proc_subcmd_para1);
	seq_printf(m, "%s\n", gm.proc_log);
}

static void proc_dump_dtsi(struct seq_file *m)
{
	int i;

	seq_puts(m, "********** dump DTSI **********\n");

	seq_printf(m, "Active Table :%d\n",
		fg_table_cust_data.active_table_number);

	for (i = 0; i < fg_table_cust_data.active_table_number; i++) {
		seq_printf(m, "PMIC_MIN_VOL = %d\n",
			fg_table_cust_data.fg_profile[i].pmic_min_vol);
	}
	for (i = 0; i < fg_table_cust_data.active_table_number; i++) {
		seq_printf(m, "POWERON_SYSTEM_IBOOT = %d\n",
			fg_table_cust_data.fg_profile[i].pon_iboot);
	}
	for (i = 0; i < fg_table_cust_data.active_table_number; i++) {
		seq_printf(m, "TEMPERATURE_T%d = %d\n",
			i, fg_table_cust_data.fg_profile[i].temperature);
	}
	for (i = 0; i < fg_table_cust_data.active_table_number; i++) {
		seq_printf(m, "g_FG_PSEUDO100_%d = %d\n",
			i, fg_table_cust_data.fg_profile[i].pseudo100);
	}

	seq_printf(m, "DIFFERENCE_FULLOCV_ITH = %d\n",
		fg_cust_data.difference_fullocv_ith);
	seq_printf(m, "SHUTDOWN_1_TIME = %d\n",
		fg_cust_data.shutdown_1_time);
	seq_printf(m, "KEEP_100_PERCENT = %d\n",
		fg_cust_data.keep_100_percent_minsoc);
	seq_printf(m, "R_FG_VALUE = %d\n",
		fg_cust_data.r_fg_value);
	seq_printf(m, "EMBEDDED_SEL = %d\n",
		fg_cust_data.embedded_sel);
	seq_printf(m, "PMIC_SHUTDOWN_CURRENT = %d\n",
		fg_cust_data.pmic_shutdown_current);
	seq_printf(m, "FG_METER_RESISTANCE = %d\n",
		fg_cust_data.fg_meter_resistance);
	seq_printf(m, "CAR_TUNE_VALUE = %d\n",
		fg_cust_data.car_tune_value);
	seq_printf(m, "SHUTDOWN_GAUGE0_VOLTAGE = %d\n",
		fg_cust_data.shutdown_gauge0_voltage);
	seq_printf(m, "Q_MAX_SYS_VOLTAGE = %d\n",
		fg_cust_data.q_max_sys_voltage);
	seq_printf(m, "COM_FG_METER_RESISTANCE = %d\n",
		fg_cust_data.com_fg_meter_resistance);
	seq_printf(m, "COM_R_FG_VALUE = %d\n",
		fg_cust_data.com_r_fg_value);
	seq_printf(m, "enable_tmp_intr_suspend = %d\n",
		gm.enable_tmp_intr_suspend);
	seq_printf(m, "ACTIVE_TABLE = %d\n",
		fg_table_cust_data.active_table_number);
	seq_printf(m, "MULTI_TEMP_GAUGE0 = %d\n",
		fg_cust_data.multi_temp_gauge0);
	seq_printf(m, "SHUTDOWN_GAUGE0 = %d\n",
		fg_cust_data.shutdown_gauge0);
	seq_printf(m, "SHUTDOWN_GAUGE1_XMINS = %d\n",
		fg_cust_data.shutdown_gauge1_xmins);
	seq_printf(m, "SHUTDOWN_GAUGE1_VBAT_EN = %d\n",
		fg_cust_data.shutdown_gauge1_vbat_en);
	seq_printf(m, "SHUTDOWN_GAUGE1_VBAT = %d\n",
		fg_cust_data.shutdown_gauge1_vbat);
	seq_printf(m, "PSEUDO100_EN = %d\n",
		fg_cust_data.pseudo100_en);
	seq_printf(m, "PSEUDO100_EN_DIS = %d\n",
		fg_cust_data.pseudo100_en_dis);



	seq_printf(m, "CHARGE_PSEUDO_FULL_LEVEL = %d\n",
		fg_cust_data.charge_pseudo_full_level);
	seq_printf(m, "FULL_TRACKING_BAT_INT2_MULTIPLY = %d\n",
		fg_cust_data.full_tracking_bat_int2_multiply);
	seq_printf(m, "DISCHARGE_TRACKING_TIME = %d\n",
		fg_cust_data.discharge_tracking_time);
	seq_printf(m, "CHARGE_TRACKING_TIME = %d\n",
		fg_cust_data.charge_tracking_time);
	seq_printf(m, "DIFFERENCE_FULLOCV_VTH = %d\n",
		fg_cust_data.difference_fullocv_vth);
	seq_printf(m, "HWOCV_SWOCV_DIFF = %d\n",
		fg_cust_data.hwocv_swocv_diff);
	seq_printf(m, "HWOCV_SWOCV_DIFF_LT = %d\n",
		fg_cust_data.hwocv_swocv_diff_lt);
	seq_printf(m, "HWOCV_SWOCV_DIFF_LT_TEMP = %d\n",
		fg_cust_data.hwocv_swocv_diff_lt_temp);
	seq_printf(m, "HWOCV_OLDOCV_DIFF = %d\n",
		fg_cust_data.hwocv_oldocv_diff);
	seq_printf(m, "HWOCV_OLDOCV_DIFF_CHR = %d\n",
		fg_cust_data.hwocv_oldocv_diff_chr);
	seq_printf(m, "VBAT_OLDOCV_DIFF = %d\n",
		fg_cust_data.vbat_oldocv_diff);
	seq_printf(m, "SWOCV_OLDOCV_DIFF_EMB = %d\n",
		fg_cust_data.swocv_oldocv_diff_emb);
	seq_printf(m, "TNEW_TOLD_PON_DIFF = %d\n",
		fg_cust_data.tnew_told_pon_diff);
	seq_printf(m, "TNEW_TOLD_PON_DIFF2 = %d\n",
		fg_cust_data.tnew_told_pon_diff2);
	seq_printf(m, "PMIC_SHUTDOWN_TIME = %d\n",
		fg_cust_data.pmic_shutdown_time);
	seq_printf(m, "EXT_HWOCV_SWOCV = %d\n",
		gm.ext_hwocv_swocv);
	seq_printf(m, "EXT_HWOCV_SWOCV_LT = %d\n",
		gm.ext_hwocv_swocv_lt);
	seq_printf(m, "EXT_HWOCV_SWOCV_LT_TEMP = %d\n",
		gm.ext_hwocv_swocv_lt_temp);
	seq_printf(m, "DIFFERENCE_FGC_FGV_TH1 = %d\n",
		fg_cust_data.difference_fgc_fgv_th1);
	seq_printf(m, "DIFFERENCE_FGC_FGV_TH2 = %d\n",
		fg_cust_data.difference_fgc_fgv_th2);
	seq_printf(m, "DIFFERENCE_FGC_FGV_TH3 = %d\n",
		fg_cust_data.difference_fgc_fgv_th3);
	seq_printf(m, "DIFFERENCE_FGC_FGV_TH_SOC1 = %d\n",
		fg_cust_data.difference_fgc_fgv_th_soc1);
	seq_printf(m, "DIFFERENCE_FGC_FGV_TH_SOC2 = %d\n",
		fg_cust_data.difference_fgc_fgv_th_soc2);
	seq_printf(m, "PMIC_SHUTDOWN_SW_EN = %d\n",
		fg_cust_data.pmic_shutdown_sw_en);
	seq_printf(m, "FORCE_VC_MODE = %d\n",
		fg_cust_data.force_vc_mode);
	seq_printf(m, "ZCV_SUSPEND_TIME = %d\n",
		fg_cust_data.zcv_suspend_time);
	seq_printf(m, "SLEEP_CURRENT_AVG = %d\n",
		fg_cust_data.sleep_current_avg);
	seq_printf(m, "ZCV_CAR_GAP_PERCENTAGE = %d\n",
		fg_cust_data.zcv_car_gap_percentage);
	seq_printf(m, "UI_FULL_LIMIT_EN = %d\n",
		fg_cust_data.ui_full_limit_en);
	seq_printf(m, "UI_FULL_LIMIT_SOC0 = %d\n",
		fg_cust_data.ui_full_limit_soc0);
	seq_printf(m, "UI_FULL_LIMIT_ITH0 = %d\n",
		fg_cust_data.ui_full_limit_ith0);
	seq_printf(m, "UI_FULL_LIMIT_SOC1 = %d\n",
		fg_cust_data.ui_full_limit_soc1);
	seq_printf(m, "UI_FULL_LIMIT_ITH1 = %d\n",
		fg_cust_data.ui_full_limit_ith1);
	seq_printf(m, "UI_FULL_LIMIT_SOC2 = %d\n",
		fg_cust_data.ui_full_limit_soc2);
	seq_printf(m, "UI_FULL_LIMIT_ITH2 = %d\n",
		fg_cust_data.ui_full_limit_ith2);
	seq_printf(m, "UI_FULL_LIMIT_SOC3 = %d\n",
		fg_cust_data.ui_full_limit_soc3);
	seq_printf(m, "UI_FULL_LIMIT_ITH3 = %d\n",
		fg_cust_data.ui_full_limit_ith3);
	seq_printf(m, "UI_FULL_LIMIT_SOC4 = %d\n",
		fg_cust_data.ui_full_limit_soc4);
	seq_printf(m, "UI_FULL_LIMIT_ITH4 = %d\n",
		fg_cust_data.ui_full_limit_ith4);
	seq_printf(m, "UI_FULL_LIMIT_TIME = %d\n",
		fg_cust_data.ui_full_limit_time);

	seq_printf(m, "UI_LOW_LIMIT_EN = %d\n",
		fg_cust_data.ui_low_limit_en);
	seq_printf(m, "UI_LOW_LIMIT_SOC0 = %d\n",
		fg_cust_data.ui_low_limit_soc0);
	seq_printf(m, "UI_LOW_LIMIT_VTH0 = %d\n",
		fg_cust_data.ui_low_limit_vth0);
	seq_printf(m, "UI_LOW_LIMIT_SOC1 = %d\n",
		fg_cust_data.ui_low_limit_soc1);
	seq_printf(m, "UI_LOW_LIMIT_VTH1 = %d\n",
		fg_cust_data.ui_low_limit_vth1);
	seq_printf(m, "UI_LOW_LIMIT_SOC2 = %d\n",
		fg_cust_data.ui_low_limit_soc2);
	seq_printf(m, "UI_LOW_LIMIT_VTH2 = %d\n",
		fg_cust_data.ui_low_limit_vth2);
	seq_printf(m, "UI_LOW_LIMIT_SOC3 = %d\n",
		fg_cust_data.ui_low_limit_soc3);
	seq_printf(m, "UI_LOW_LIMIT_VTH3 = %d\n",
		fg_cust_data.ui_low_limit_vth3);
	seq_printf(m, "UI_LOW_LIMIT_SOC4 = %d\n",
		fg_cust_data.ui_low_limit_soc4);
	seq_printf(m, "UI_LOW_LIMIT_VTH4 = %d\n",
		fg_cust_data.ui_low_limit_vth4);
	seq_printf(m, "UI_LOW_LIMIT_TIME = %d\n",
		fg_cust_data.ui_low_limit_time);
	seq_printf(m, "FG_PRE_TRACKING_EN = %d\n",
		fg_cust_data.fg_pre_tracking_en);
	seq_printf(m, "VBAT2_DET_TIME = %d\n",
		fg_cust_data.vbat2_det_time);
	seq_printf(m, "VBAT2_DET_COUNTERE = %d\n",
		fg_cust_data.vbat2_det_counter);
	seq_printf(m, "VBAT2_DET_VOLTAGE1 = %d\n",
		fg_cust_data.vbat2_det_voltage1);
	seq_printf(m, "VBAT2_DET_VOLTAGE2 = %d\n",
		fg_cust_data.vbat2_det_voltage2);
	seq_printf(m, "VBAT2_DET_VOLTAGE3 = %d\n",
		fg_cust_data.vbat2_det_voltage3);
	seq_printf(m, "AGING_FACTOR_MIN = %d\n",
		fg_cust_data.aging_factor_min);
	seq_printf(m, "AGING_FACTOR_DIFF = %d\n",
		fg_cust_data.aging_factor_diff);
	seq_printf(m, "DIFFERENCE_VOLTAGE_UPDATE = %d\n",
		fg_cust_data.difference_voltage_update);
	seq_printf(m, "AGING_ONE_EN = %d\n",
		fg_cust_data.aging_one_en);
	seq_printf(m, "AGING1_UPDATE_SOC = %d\n",
		fg_cust_data.aging1_update_soc);
	seq_printf(m, "AGING1_LOAD_SOC = %d\n",
		fg_cust_data.aging1_load_soc);

	seq_printf(m, "AGING4_UPDATE_SOC = %d\n",
		fg_cust_data.aging4_update_soc);
	seq_printf(m, "AGING4_LOAD_SOC = %d\n",
		fg_cust_data.aging4_load_soc);
	seq_printf(m, "AGING5_UPDATE_SOC = %d\n",
		fg_cust_data.aging5_update_soc);
	seq_printf(m, "AGING5_LOAD_SOC = %d\n",
		fg_cust_data.aging5_load_soc);
	seq_printf(m, "AGING6_UPDATE_SOC = %d\n",
		fg_cust_data.aging6_update_soc);
	seq_printf(m, "AGING6_LOAD_SOC = %d\n",
		fg_cust_data.aging6_load_soc);

	seq_printf(m, "AGING_TEMP_DIFF = %d\n",
		fg_cust_data.aging_temp_diff);
	seq_printf(m, "AGING_100_EN = %d\n",
		fg_cust_data.aging_100_en);
	seq_printf(m, "AGING_TWO_EN = %d\n",
		fg_cust_data.aging_two_en);
	seq_printf(m, "AGING_THIRD_EN = %d\n",
		fg_cust_data.aging_third_en);
	seq_printf(m, "AGING_4_EN = %d\n",
		fg_cust_data.aging_4_en);
	seq_printf(m, "AGING_5_EN = %d\n",
		fg_cust_data.aging_5_en);
	seq_printf(m, "AGING_6_EN = %d\n",
		fg_cust_data.aging_6_en);
	seq_printf(m, "DIFF_SOC_SETTING = %d\n",
		fg_cust_data.diff_soc_setting);
	seq_printf(m, "DIFF_BAT_TEMP_SETTING = %d\n",
		fg_cust_data.diff_bat_temp_setting);
	seq_printf(m, "DIFF_BAT_TEMP_SETTING_C = %d\n",
		fg_cust_data.diff_bat_temp_setting_c);
	seq_printf(m, "DIFF_IAVG_TH = %d\n",
		fg_cust_data.diff_iavg_th);
	seq_printf(m, "FG_TRACKING_CURRENT = %d\n",
		fg_cust_data.fg_tracking_current);
	seq_printf(m, "FG_TRACKING_CURRENT_IBOOT_EN = %d\n",
		fg_cust_data.fg_tracking_current_iboot_en);
	seq_printf(m, "UISOC_UPDATE_TYPE = %d\n",
		fg_cust_data.uisoc_update_type);
	seq_printf(m, "OVER_DISCHARGE_LEVEL = %d\n",
		fg_cust_data.over_discharge_level);
	seq_printf(m, "NAFG_TIME_SETTING = %d\n",
		fg_cust_data.nafg_time_setting);
	seq_printf(m, "NAFG_RATIO = %d\n",
		fg_cust_data.nafg_ratio);


	seq_printf(m, "NAFG_RATIO_EN = %d\n",
		fg_cust_data.nafg_ratio_en);
	seq_printf(m, "NAFG_RATIO_TMP_THR = %d\n",
		fg_cust_data.nafg_ratio_tmp_thr);
	seq_printf(m, "D0_SEL = %d\n",
		fg_cust_data.d0_sel);
	seq_printf(m, "IBOOT_SEL = %d\n",
		fg_cust_data.iboot_sel);
	seq_printf(m, "SHUTDOWN_SYSTEM_IBOOT = %d\n",
		fg_cust_data.shutdown_system_iboot);
	seq_printf(m, "DIFFERENCE_FULL_CV = %d\n",
		fg_cust_data.difference_full_cv);
	seq_printf(m, "PSEUDO1_EN = %d\n",
		fg_cust_data.pseudo1_en);
	seq_printf(m, "LOADING_1_EN = %d\n",
		fg_cust_data.loading_1_en);
	seq_printf(m, "LOADING_2_EN = %d\n",
		fg_cust_data.loading_2_en);
	seq_printf(m, "PSEUDO1_SEL = %d\n",
		fg_cust_data.pseudo1_sel);
	seq_printf(m, "UI_FAST_TRACKING_EN = %d\n",
		fg_cust_data.ui_fast_tracking_en);
	seq_printf(m, "UI_FAST_TRACKING_GAP = %d\n",
		fg_cust_data.ui_fast_tracking_gap);
	seq_printf(m, "KEEP_100_PERCENT_MINSOC = %d\n",
		fg_cust_data.keep_100_percent_minsoc);
	seq_printf(m, "NO_BAT_TEMP_COMPENSATE = %d\n",
		gm.no_bat_temp_compensate);
	seq_printf(m, "LOW_TEMP_MODE = %d\n",
		fg_cust_data.low_temp_mode);
	seq_printf(m, "LOW_TEMP_MODE_TEMP = %d\n",
		fg_cust_data.low_temp_mode_temp);
	seq_printf(m, "Q_MAX_L_CURRENT = %d\n",
		fg_cust_data.q_max_L_current);
	seq_printf(m, "Q_MAX_H_CURRENT = %d\n",
		fg_cust_data.q_max_H_current);
	seq_printf(m, "pl_two_sec_reboot = %d\n", gm.pl_two_sec_reboot);
#ifdef SHUTDOWN_CONDITION_LOW_BAT_VOLT
	seq_puts(m, "SHUTDOWN_CONDITION_LOW_BAT_VOLT = 1\n");
	seq_printf(m, "lbat_def: %d %d %d\n",
		VBAT2_DET_VOLTAGE1, VBAT2_DET_VOLTAGE2, VBAT2_DET_VOLTAGE3);
	seq_printf(m, "lbat: %d %d %d\n", fg_cust_data.vbat2_det_voltage1,
		fg_cust_data.vbat2_det_voltage2,
		fg_cust_data.vbat2_det_voltage3);
#else
	seq_puts(m, "SHUTDOWN_CONDITION_LOW_BAT_VOLT = 0\n");
#endif
	seq_printf(m, "hw_version = %d\n", gauge_get_hw_version());

}

static void dump_daemon_table(struct seq_file *m)
{
	int i, j;
	struct FUELGAUGE_PROFILE_STRUCT *ptr;
	struct fuel_gauge_table_custom_data *ptable2;
	struct fuel_gauge_table *pfgt;

	ptable2 = &gm.fg_data.fg_table_cust_data;

	for (j = 0; j < fg_table_cust_data.active_table_number; j++) {
		pfgt = &gm.fg_data.fg_table_cust_data.fg_profile[j];
		seq_printf(m, "daemon table idx:%d size:%d\n",
			j,
			pfgt->size);

		seq_printf(m,
			"tmp:%d qmax:%d %d pseudo1:%d pseudo100:%d\n",
			pfgt->temperature,
			pfgt->q_max,
			pfgt->q_max_h_current,
			pfgt->pseudo1,
			pfgt->pseudo100);

		seq_printf(m,
			"pmic_min_vol:%d pon_iboot:%d qmax_sys_vol:%d sd_hl_zcv:%d\n",
			pfgt->pmic_min_vol,
			pfgt->pon_iboot,
			pfgt->qmax_sys_vol,
			pfgt->shutdown_hl_zcv);

		seq_puts(m, "idx: maH, voltage, R1, R2, percentage\n");
		ptr = &ptable2->fg_profile[j].fg_profile[0];
		for (i = 0; i < 100; i++) {
			seq_printf(m, "%d: %d %d %d %d %d\n",
				i,
				ptr[i].mah,
				ptr[i].voltage,
				ptr[i].resistance,
				ptr[i].charge_r.rdc[0],
				ptr[i].percentage);
		}
	}

	seq_printf(m, "\ndaemon table idx:tmp0 size:%d\n",
		gm.fg_data.fg_table_cust_data.fg_profile_temperature_0_size);
	seq_puts(m, "idx: maH, voltage, R1, R2, percentage\n");
	ptr = &gm.fg_data.fg_table_cust_data.fg_profile_temperature_0[0];
	for (i = 0; i < 100; i++) {
		seq_printf(m, "%d: %d %d %d %d %d\n",
			i,
			ptr[i].mah,
			ptr[i].voltage,
			ptr[i].resistance,
			ptr[i].charge_r.rdc[0],
			ptr[i].percentage);

	}

	seq_printf(m, "\ndaemon table idx:tmp1 size:%d\n",
		gm.fg_data.fg_table_cust_data.fg_profile_temperature_1_size);
	seq_puts(m, "idx: maH, voltage, R, R2, percentage\n");
	ptr = &gm.fg_data.fg_table_cust_data.fg_profile_temperature_1[0];
	for (i = 0; i < 100; i++) {
		seq_printf(m, "%d: %d %d %d %d %d\n",
			i,
			ptr[i].mah,
			ptr[i].voltage,
			ptr[i].resistance,
			ptr[i].charge_r.rdc[0],
			ptr[i].percentage);
	}

}

static void dump_kernel_table(struct seq_file *m)
{
	int i, j;
	struct FUELGAUGE_PROFILE_STRUCT *ptr;
	struct fuel_gauge_table_custom_data *ptable1;
	struct fuel_gauge_table *pfgt;

	ptable1 = &fg_table_cust_data;

	seq_printf(m, "tables no:%d table size:%d\n",
		fg_table_cust_data.active_table_number,
		fg_table_cust_data.fg_profile[0].size);

	for (j = 0; j < fg_table_cust_data.active_table_number; j++) {
		pfgt = &ptable1->fg_profile[j];
		ptr = &ptable1->fg_profile[j].fg_profile[0];
		seq_printf(m, "table idx:%d size:%d\n",
			j,
			pfgt->size);

		seq_printf(m,
			"tmp:%d qmax:%d %d pseudo1:%d pseudo100:%d\n",
			pfgt->temperature,
			pfgt->q_max,
			pfgt->q_max_h_current,
			pfgt->pseudo1,
			pfgt->pseudo100);

		seq_printf(m,
			"pmic_min_vol:%d pon_iboot:%d qmax_sys_vol:%d sd_hl_zcv:%d\n",
			pfgt->pmic_min_vol,
			pfgt->pon_iboot,
			pfgt->qmax_sys_vol,
			pfgt->shutdown_hl_zcv);

		seq_puts(m, "idx: maH, voltage, R1, R2, percentage\n");
		for (i = 0; i < 100; i++) {
			seq_printf(m, "%d: %d %d %d %d %d\n",
				i,
				ptr[i].mah,
				ptr[i].voltage,
				ptr[i].resistance,
				ptr[i].charge_r.rdc[0],
				ptr[i].percentage);
		}
	}

	seq_puts(m, "\n");

	if (is_recovery_mode() == false) {
		dump_daemon_table(m);
	} else {

		ptr = &ptable1->fg_profile_temperature_1[0];
		seq_puts(m, "tmp1 idx: maH, voltage, R1, R2, percentage\n");
		for (i = 0; i < 100; i++) {
			seq_printf(m, "%d: %d %d %d %d %d\n",
				i,
				ptr[i].mah,
				ptr[i].voltage,
				ptr[i].resistance,
				ptr[i].charge_r.rdc[0],
				ptr[i].percentage);
		}
	}

}


static int proc_dump_log_show(struct seq_file *m, void *v)
{
	int i;

	seq_puts(m, "********** Gauge Dump **********\n");

	seq_puts(m, "Command Table list\n");
	seq_puts(m, "0: dump dtsi\n");
	seq_puts(m, "1: dump v-mode table\n");
	seq_puts(m, "101: dump gauge hw register\n");
	seq_puts(m, "102: kernel table\n");
	seq_puts(m, "103: send CHR FULL\n");
	seq_puts(m, "104: disable NAFG interrupt\n");
	seq_puts(m, "105: show daemon pid\n");

	seq_printf(m, "current command:%d\n", gm.proc_cmd_id);

	switch (gm.proc_cmd_id) {
	case 0:
		proc_dump_dtsi(m);
		break;
	case 1:
	case 2:
	case 3:
	case 4:
		wakeup_fg_algo_cmd(
			FG_INTR_KERNEL_CMD, FG_KERNEL_CMD_DUMP_LOG,
			gm.proc_cmd_id);
		for (i = 0; i < 5; i++) {
			msleep(500);
			if (gm.proc_subcmd_para1 == 1)
				break;
		}
		proc_dump_log(m);
		break;
	case 101:
		gauge_dev_dump(gm.gdev, m, 0);
		break;
	case 102:
		dump_kernel_table(m);
		break;
	case 103:
		wakeup_fg_algo(FG_INTR_CHR_FULL);
		break;
	case 104:
		gauge_set_nag_en(false);
		gm.disable_nafg_int = true;
		break;
	case 105:
		seq_printf(m, "Gauge daemon pid:%d\n", gm.g_fgd_pid);
		break;
	default:
		seq_printf(m, "do not support command:%d\n", gm.proc_cmd_id);
		break;
	}



	/*battery_dump_info(m);*/

	return 0;
}

static ssize_t proc_write(
	struct file *file, const char __user *buffer,
	size_t count, loff_t *f_pos)
{
	int cmd = 0;
	char num[10];

	memset(num, 0, 10);

	if (!count)
		return 0;

	if (count > (sizeof(num) - 1))
		return -EINVAL;

	if (copy_from_user(num, buffer, count))
		return -EFAULT;

	if (kstrtoint(num, 10, &cmd) == 0)
		gm.proc_cmd_id = cmd;
	else {
		gm.proc_cmd_id = 0;
		return -EFAULT;
	}

	bm_err("%s success %d\n",
		__func__, cmd);
	return count;
}


static int proc_dump_log_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_dump_log_show, NULL);
}

static const struct file_operations battery_dump_log_proc_fops = {
	.open = proc_dump_log_open,
	.read = seq_read,
	.llseek	= seq_lseek,
	.write = proc_write,
};

void battery_debug_init(void)
{
	struct proc_dir_entry *battery_dir;

	battery_dir = proc_mkdir("battery", NULL);
	if (!battery_dir) {
		bm_err("fail to mkdir /proc/battery\n");
		return;
	}

	proc_create("dump_log", 0644,
		battery_dir, &battery_dump_log_proc_fops);
}

static ssize_t show_Battery_Temperature(
	struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	bm_err("%s: %d %d\n",
		__func__,
		battery_main.BAT_batt_temp, gm.fixed_bat_tmp);
	return sprintf(buf, "%d\n", gm.fixed_bat_tmp);
}

static ssize_t store_Battery_Temperature(
	struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {

		if (temp > 58 || temp < -10) {
			bm_err(
				"%s: setting tmp:%d!,reject set\n",
				__func__,
				temp);
			return size;
		}

		gm.fixed_bat_tmp = temp;
		if (gm.fixed_bat_tmp == 0xffff)
			fg_bat_temp_int_internal();
		else {
			gauge_dev_enable_battery_tmp_lt_interrupt(
				gm.gdev, 0, 0);
			gauge_dev_enable_battery_tmp_ht_interrupt(
				gm.gdev, 0, 0);
			wakeup_fg_algo(FG_INTR_BAT_TMP_C_HT);
			wakeup_fg_algo(FG_INTR_BAT_TMP_HT);
		}
		battery_main.BAT_batt_temp = force_get_tbat(true);
		bm_err(
			"%s: fixed_bat_tmp:%d ,tmp:%d!\n",
			__func__,
			temp, battery_main.BAT_batt_temp);
		battery_update(&battery_main);
	} else {
		bm_err("%s: format error!\n", __func__);
	}
	return size;
}

static DEVICE_ATTR(Battery_Temperature, 0664, show_Battery_Temperature,
		   store_Battery_Temperature);

static ssize_t show_UI_SOC(
	struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	bm_err("%s: %d %d\n",
		__func__,
		gm.ui_soc, gm.fixed_uisoc);
	return sprintf(buf, "%d\n", gm.fixed_uisoc);
}

static ssize_t store_UI_SOC(
	struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {

		gm.fixed_uisoc = temp;

		bm_err("%s: %d %d\n",
			__func__,
			gm.ui_soc, gm.fixed_uisoc);

		battery_update(&battery_main);
	}

	return size;
}

static DEVICE_ATTR(UI_SOC, 0664, show_UI_SOC,
		   store_UI_SOC);


/* ============================================================ */
/* Internal function */
/* ============================================================ */
void fg_custom_data_check(void)
{
	struct fuel_gauge_custom_data *p;

	p = &fg_cust_data;
	fgauge_get_profile_id();

	bm_err("FGLOG MultiGauge0[%d] BATID[%d] pmic_min_vol[%d,%d,%d,%d,%d]\n",
		p->multi_temp_gauge0, gm.battery_id,
		fg_table_cust_data.fg_profile[0].pmic_min_vol,
		fg_table_cust_data.fg_profile[1].pmic_min_vol,
		fg_table_cust_data.fg_profile[2].pmic_min_vol,
		fg_table_cust_data.fg_profile[3].pmic_min_vol,
		fg_table_cust_data.fg_profile[4].pmic_min_vol);
	bm_err("FGLOG pon_iboot[%d,%d,%d,%d,%d] qmax_sys_vol[%d %d %d %d %d]\n",
		fg_table_cust_data.fg_profile[0].pon_iboot,
		fg_table_cust_data.fg_profile[1].pon_iboot,
		fg_table_cust_data.fg_profile[2].pon_iboot,
		fg_table_cust_data.fg_profile[3].pon_iboot,
		fg_table_cust_data.fg_profile[4].pon_iboot,
		fg_table_cust_data.fg_profile[0].qmax_sys_vol,
		fg_table_cust_data.fg_profile[1].qmax_sys_vol,
		fg_table_cust_data.fg_profile[2].qmax_sys_vol,
		fg_table_cust_data.fg_profile[3].qmax_sys_vol,
		fg_table_cust_data.fg_profile[4].qmax_sys_vol);

}

int interpolation(int i1, int b1, int i2, int b2, int i)
{
	int ret;

	ret = (b2 - b1) * (i - i1) / (i2 - i1) + b1;

	return ret;
}

unsigned int TempConverBattThermistor(int temp)
{
	int RES1 = 0, RES2 = 0;
	int TMP1 = 0, TMP2 = 0;
	int i;
	unsigned int TBatt_R_Value = 0xffff;

	if (temp >= Fg_Temperature_Table[20].BatteryTemp) {
		TBatt_R_Value = Fg_Temperature_Table[20].TemperatureR;
	} else if (temp <= Fg_Temperature_Table[0].BatteryTemp) {
		TBatt_R_Value = Fg_Temperature_Table[0].TemperatureR;
	} else {
		RES1 = Fg_Temperature_Table[0].TemperatureR;
		TMP1 = Fg_Temperature_Table[0].BatteryTemp;

		for (i = 0; i <= 20; i++) {
			if (temp <= Fg_Temperature_Table[i].BatteryTemp) {
				RES2 = Fg_Temperature_Table[i].TemperatureR;
				TMP2 = Fg_Temperature_Table[i].BatteryTemp;
				break;
			}
			{	/* hidden else */
				RES1 = Fg_Temperature_Table[i].TemperatureR;
				TMP1 = Fg_Temperature_Table[i].BatteryTemp;
			}
		}


		TBatt_R_Value = interpolation(TMP1, RES1, TMP2, RES2, temp);
	}

	bm_warn(
		"[%s] [%d] %d %d %d %d %d\n",
		__func__,
		TBatt_R_Value, TMP1,
		RES1, TMP2, RES2, temp);

	return TBatt_R_Value;
}

int BattThermistorConverTemp(int Res)
{
	int i = 0;
	int RES1 = 0, RES2 = 0;
	int TBatt_Value = -2000, TMP1 = 0, TMP2 = 0;

	if (Res >= Fg_Temperature_Table[0].TemperatureR) {
		TBatt_Value = -400;
	} else if (Res <= Fg_Temperature_Table[20].TemperatureR) {
		TBatt_Value = 600;
	} else {
		RES1 = Fg_Temperature_Table[0].TemperatureR;
		TMP1 = Fg_Temperature_Table[0].BatteryTemp;

		for (i = 0; i <= 20; i++) {
			if (Res >= Fg_Temperature_Table[i].TemperatureR) {
				RES2 = Fg_Temperature_Table[i].TemperatureR;
				TMP2 = Fg_Temperature_Table[i].BatteryTemp;
				break;
			}
			{	/* hidden else */
				RES1 = Fg_Temperature_Table[i].TemperatureR;
				TMP1 = Fg_Temperature_Table[i].BatteryTemp;
			}
		}

		TBatt_Value = (((Res - RES2) * TMP1) +
			((RES1 - Res) * TMP2)) * 10 / (RES1 - RES2);
	}
	bm_trace(
		"[%s] %d %d %d %d %d %d\n",
		__func__,
		RES1, RES2, Res, TMP1,
		TMP2, TBatt_Value);

	return TBatt_Value;
}

unsigned int TempToBattVolt(int temp, int update)
{
	unsigned int R_NTC = TempConverBattThermistor(temp);
	long long Vin = 0;
	long long V_IR_comp = 0;
	/*int vbif28 = pmic_get_auxadc_value(AUXADC_LIST_VBIF);*/
	int vbif28 = gm.rbat.rbat_pull_up_volt;
	static int fg_current_temp;
	static bool fg_current_state;
	int fg_r_value = fg_cust_data.com_r_fg_value;
	int fg_meter_res_value = 0;

	if (gm.no_bat_temp_compensate == 0)
		fg_meter_res_value = fg_cust_data.com_fg_meter_resistance;
	else
		fg_meter_res_value = 0;

#ifdef RBAT_PULL_UP_VOLT_BY_BIF
	vbif28 = pmic_get_vbif28_volt();
#endif
	Vin = (long long)R_NTC * vbif28 * 10;	/* 0.1 mV */

#if defined(__LP64__) || defined(_LP64)
	do_div(Vin, (R_NTC + gm.rbat.rbat_pull_up_r));
#else
	Vin = div_s64(Vin, (R_NTC + gm.rbat.rbat_pull_up_r));
#endif

	if (update == true)
		fg_current_state = gauge_get_current(&fg_current_temp);

	if (fg_current_state == true) {
		V_IR_comp = Vin;
		V_IR_comp +=
			((fg_current_temp *
			(fg_meter_res_value + fg_r_value)) / 10000);
	} else {
		V_IR_comp = Vin;
		V_IR_comp -=
			((fg_current_temp *
			(fg_meter_res_value + fg_r_value)) / 10000);
	}

	bm_notice("[%s] temp %d R_NTC %d V(%lld %lld) I %d CHG %d\n",
		__func__,
		temp, R_NTC, Vin, V_IR_comp, fg_current_temp, fg_current_state);

	return (unsigned int) V_IR_comp;
}

int BattVoltToTemp(int dwVolt, int volt_cali)
{
	long long TRes_temp;
	long long TRes;
	int sBaTTMP = -100;
	int vbif28 = gm.rbat.rbat_pull_up_volt;
	int delta_v;

	TRes_temp = (gm.rbat.rbat_pull_up_r * (long long) dwVolt);
#ifdef RBAT_PULL_UP_VOLT_BY_BIF
	vbif28 = pmic_get_vbif28_volt() + volt_cali;
	delta_v = abs(vbif28 - dwVolt);
	if (delta_v == 0)
		delta_v = 1;

#if defined(__LP64__) || defined(_LP64)
		do_div(TRes_temp, delta_v);
#else
		TRes_temp = div_s64(TRes_temp, delta_v);
#endif

	if (vbif28 > 3000 || vbif28 < 2500)
		bm_err(
			"[RBAT_PULL_UP_VOLT_BY_BIF] vbif28:%d\n",
			pmic_get_vbif28_volt());
#else
	delta_v = abs(gm.rbat.rbat_pull_up_volt - dwVolt);
	if (delta_v == 0)
		delta_v = 1;
#if defined(__LP64__) || defined(_LP64)
	do_div(TRes_temp, delta_v);
#else
	TRes_temp = div_s64(TRes_temp, delta_v);
#endif


#endif

#ifdef RBAT_PULL_DOWN_R
	TRes = (TRes_temp * RBAT_PULL_DOWN_R);

#if defined(__LP64__) || defined(_LP64)
		do_div(TRes, abs(RBAT_PULL_DOWN_R - TRes_temp));
#else
		TRes_temp = div_s64(TRes, abs(RBAT_PULL_DOWN_R - TRes_temp));
#endif

#else
	TRes = TRes_temp;
#endif

	/* convert register to temperature */
	if (!pmic_is_bif_exist())
		sBaTTMP = BattThermistorConverTemp((int)TRes);
	else
		sBaTTMP = BattThermistorConverTemp((int)TRes -
		gm.rbat.bif_ntc_r);

	bm_notice(
		"[%s] %d %d %d %d\n",
		__func__,
		dwVolt, gm.rbat.rbat_pull_up_r,
		vbif28, volt_cali);
	return sBaTTMP;
}

int force_get_tbat_internal(bool update)
{
	int bat_temperature_volt = 0;
	int bat_temperature_val = 0;
	static int pre_bat_temperature_val = -1;
	int fg_r_value = 0;
	int fg_meter_res_value = 0;
	int fg_current_temp = 0;
	bool fg_current_state = false;
	int bat_temperature_volt_temp = 0;
	int vol_cali = 0;

	static int pre_bat_temperature_volt_temp, pre_bat_temperature_volt;
	static int pre_fg_current_temp;
	static int pre_fg_current_state;
	static int pre_fg_r_value;
	static int pre_bat_temperature_val2;
	static struct timespec pre_time;
	struct timespec ctime, dtime;

	if (is_battery_init_done() == false) {
		gm.tbat_precise = 250;
		return 25;
	}

	if (gm.fixed_bat_tmp != 0xffff) {
		gm.tbat_precise = gm.fixed_bat_tmp * 10;
		return gm.fixed_bat_tmp;
	}

	if (get_ec()->fixed_temp_en) {
		gm.tbat_precise = get_ec()->fixed_temp_value * 10;
		return get_ec()->fixed_temp_value;
	}

	if (update == true || pre_bat_temperature_val == -1) {
		/* Get V_BAT_Temperature */
		bat_temperature_volt = 2;
		bat_temperature_volt = pmic_get_v_bat_temp();

		if (bat_temperature_volt != 0) {
			fg_r_value = fg_cust_data.com_r_fg_value;
			if (gm.no_bat_temp_compensate == 0)
				fg_meter_res_value =
					fg_cust_data.com_fg_meter_resistance;
			else
				fg_meter_res_value = 0;


			gauge_dev_get_current(
				gm.gdev, &fg_current_state, &fg_current_temp);
			fg_current_temp = fg_current_temp / 10;

			if (fg_current_state == true) {
				bat_temperature_volt_temp =
					bat_temperature_volt;
				bat_temperature_volt =
				bat_temperature_volt -
				((fg_current_temp *
					(fg_meter_res_value + fg_r_value))
						/ 10000);
				vol_cali =
					-((fg_current_temp *
					(fg_meter_res_value + fg_r_value))
						/ 10000);
			} else {
				bat_temperature_volt_temp =
					bat_temperature_volt;
				bat_temperature_volt =
				bat_temperature_volt +
				((fg_current_temp *
				(fg_meter_res_value + fg_r_value)) / 10000);
				vol_cali =
					((fg_current_temp *
					(fg_meter_res_value + fg_r_value))
					/ 10000);
			}

			bat_temperature_val =
				BattVoltToTemp(bat_temperature_volt, vol_cali);
		}

#ifdef CONFIG_MTK_BIF_SUPPORT
		/*	CHARGING_CMD_GET_BIF_TBAT need fix */
#endif
		bm_notice("[force_get_tbat] %d,%d,%d,%d,%d,%d r:%d %d %d\n",
		bat_temperature_volt_temp, bat_temperature_volt,
		fg_current_state, fg_current_temp,
		fg_r_value, bat_temperature_val,
		fg_meter_res_value, fg_r_value, gm.no_bat_temp_compensate);

		if (pre_bat_temperature_val2 == 0) {
			pre_bat_temperature_volt_temp =
				bat_temperature_volt_temp;
			pre_bat_temperature_volt = bat_temperature_volt;
			pre_fg_current_temp = fg_current_temp;
			pre_fg_current_state = fg_current_state;
			pre_fg_r_value = fg_r_value;
			pre_bat_temperature_val2 = bat_temperature_val;
			get_monotonic_boottime(&pre_time);
		} else {
			get_monotonic_boottime(&ctime);
			dtime = timespec_sub(ctime, pre_time);

			if (((dtime.tv_sec <= 20) &&
				(abs(pre_bat_temperature_val2 -
				bat_temperature_val) >= 50)) ||
				bat_temperature_val >= 580) {
				bm_err(
				"[force_get_tbat][err] current:%d,%d,%d,%d,%d,%d pre:%d,%d,%d,%d,%d,%d\n",
					bat_temperature_volt_temp,
					bat_temperature_volt,
					fg_current_state,
					fg_current_temp,
					fg_r_value,
					bat_temperature_val,
					pre_bat_temperature_volt_temp,
					pre_bat_temperature_volt,
					pre_fg_current_state,
					pre_fg_current_temp,
					pre_fg_r_value,
					pre_bat_temperature_val2);
				/*pmic_auxadc_debug(1);*/
				WARN_ON_ONCE(1);
			}

			pre_bat_temperature_volt_temp =
				bat_temperature_volt_temp;
			pre_bat_temperature_volt = bat_temperature_volt;
			pre_fg_current_temp = fg_current_temp;
			pre_fg_current_state = fg_current_state;
			pre_fg_r_value = fg_r_value;
			pre_bat_temperature_val2 = bat_temperature_val;
			pre_time = ctime;
			bm_trace(
			"[force_get_tbat] current:%d,%d,%d,%d,%d,%d pre:%d,%d,%d,%d,%d,%d time:%d\n",
			bat_temperature_volt_temp, bat_temperature_volt,
			fg_current_state, fg_current_temp,
			fg_r_value, bat_temperature_val,
			pre_bat_temperature_volt_temp, pre_bat_temperature_volt,
			pre_fg_current_state, pre_fg_current_temp,
			pre_fg_r_value,
			pre_bat_temperature_val2, (int)dtime.tv_sec);
		}
	} else {
		bat_temperature_val = pre_bat_temperature_val;
	}

	gm.tbat_precise = bat_temperature_val;

	return bat_temperature_val / 10;
}

int force_get_tbat(bool update)
{
	int bat_temperature_val = 0;
	int counts = 0;

	if (is_fg_disabled()) {
		bm_debug("[%s] fixed TBAT=25 t\n",
			__func__);
		gm.tbat_precise = 250;
		return 25;
	}

#if defined(FIXED_TBAT_25)
	bm_debug("[%s] fixed TBAT=25 t\n", __func__);
	gm.tbat_precise = 250;
	return 25;
#else

	bat_temperature_val = force_get_tbat_internal(update);

	while (counts < 5 && bat_temperature_val >= 60) {
		bm_err("[%s]over60 count=%d, bat_temp=%d\n",
			__func__,
			counts, bat_temperature_val);
		bat_temperature_val = force_get_tbat_internal(true);
		counts++;
	}

	if (bat_temperature_val <=
		BATTERY_TMP_TO_DISABLE_GM30 && gm.disableGM30 == false) {
		bm_err(
			"battery temperature is too low %d and disable GM3.0\n",
			bat_temperature_val);
		disable_fg();
		if (gm.disableGM30 == true)
			battery_main.BAT_CAPACITY = 50;
		battery_update(&battery_main);
	}

	if (bat_temperature_val <= BATTERY_TMP_TO_DISABLE_NAFG) {
		int fgv;

		fgv = gauge_get_hw_version();
		if (fgv >= GAUGE_HW_V1000
		&& fgv < GAUGE_HW_V2000) {
			en_intr_VBATON_UNDET(0);
		}

		gm.ntc_disable_nafg = true;
		bm_err("[%s] ntc_disable_nafg %d %d\n",
			__func__,
			bat_temperature_val,
			DEFAULT_BATTERY_TMP_WHEN_DISABLE_NAFG);

		gm.tbat_precise = DEFAULT_BATTERY_TMP_WHEN_DISABLE_NAFG * 10;
		return DEFAULT_BATTERY_TMP_WHEN_DISABLE_NAFG;
	}

	gm.ntc_disable_nafg = false;
	bm_debug("[%s] t:%d precise:%d\n", __func__,
		bat_temperature_val, gm.tbat_precise);

	return bat_temperature_val;
#endif
}

unsigned int battery_meter_get_fg_time(void)
{
	unsigned int time = 0;

	gauge_dev_get_time(gm.gdev, &time);
	return time;
}

unsigned int battery_meter_enable_time_interrupt(unsigned int sec)
{
	return gauge_dev_enable_time_interrupt(gm.gdev, sec);
}

/* ============================================================ */
/* Internal function */
/* ============================================================ */
void update_fg_dbg_tool_value(void)
{
	/* Todo: backup variables */
}

static void nl_send_to_user(u32 pid, int seq, struct fgd_nl_msg_t *reply_msg)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	/* int size=sizeof(struct fgd_nl_msg_t); */
	int size = reply_msg->fgd_data_len + FGD_NL_MSG_T_HDR_LEN;
	int len = NLMSG_SPACE(size);
	void *data;
	int ret;

	reply_msg->identity = FGD_NL_MAGIC;

	if (in_interrupt())
		skb = alloc_skb(len, GFP_ATOMIC);
	else
		skb = alloc_skb(len, GFP_KERNEL);

	if (!skb)
		return;

	nlh = nlmsg_put(skb, pid, seq, 0, size, 0);
	data = NLMSG_DATA(nlh);
	memcpy(data, reply_msg, size);
	NETLINK_CB(skb).portid = 0;	/* from kernel */
	NETLINK_CB(skb).dst_group = 0;	/* unicast */

	ret = netlink_unicast(gm.daemo_nl_sk, skb, pid, MSG_DONTWAIT);
	if (ret < 0) {
		bm_err("[Netlink] send failed %d\n", ret);
		return;
	}
	/*bm_debug("[Netlink] reply_user: netlink_unicast- ret=%d\n", ret); */


}

static void nl_data_handler(struct sk_buff *skb)
{
	u32 pid;
	kuid_t uid;
	int seq;
	void *data;
	struct nlmsghdr *nlh;
	struct fgd_nl_msg_t *fgd_msg, *fgd_ret_msg;
	int size = 0;

	nlh = (struct nlmsghdr *)skb->data;
	pid = NETLINK_CREDS(skb)->pid;
	uid = NETLINK_CREDS(skb)->uid;
	seq = nlh->nlmsg_seq;

	data = NLMSG_DATA(nlh);

	fgd_msg = (struct fgd_nl_msg_t *)data;

	if (IS_ENABLED(CONFIG_POWER_EXT) || gm.disable_mtkbattery ||
		IS_ENABLED(CONFIG_MTK_DISABLE_GAUGE)) {
		bm_err("GM3 disable, nl handler rev data\n");
		return;
	}

	if (fgd_msg->identity != FGD_NL_MAGIC) {
		bm_err("[FGERR]not correct MTKFG netlink packet!%d\n",
			fgd_msg->identity);
		return;
	}

	if (gm.g_fgd_pid != pid &&
		fgd_msg->fgd_cmd > FG_DAEMON_CMD_SET_DAEMON_PID) {
		bm_err("drop rev netlink pid:%d:%d  cmd:%d:%d\n",
			pid,
			gm.g_fgd_pid,
			fgd_msg->fgd_cmd,
			FG_DAEMON_CMD_SET_DAEMON_PID);
		return;
	}

	size = fgd_msg->fgd_ret_data_len + FGD_NL_MSG_T_HDR_LEN;

	if (size > (PAGE_SIZE << 1))
		fgd_ret_msg = vmalloc(size);
	else {
		if (in_interrupt())
			fgd_ret_msg = kmalloc(size, GFP_ATOMIC);
	else
		fgd_ret_msg = kmalloc(size, GFP_KERNEL);
	}

	if (fgd_ret_msg == NULL) {
		if (size > PAGE_SIZE)
			fgd_ret_msg = vmalloc(size);

		if (fgd_ret_msg == NULL)
			return;
	}

	memset(fgd_ret_msg, 0, size);

	bmd_ctrl_cmd_from_user(data, fgd_ret_msg);
	nl_send_to_user(pid, seq, fgd_ret_msg);

	kvfree(fgd_ret_msg);
}

int wakeup_fg_algo(unsigned int flow_state)
{
	update_fg_dbg_tool_value();

	if (gm.disableGM30) {
		bm_err("FG daemon is disabled\n");
		return -1;
	}

	if (is_recovery_mode()) {
		wakeup_fg_algo_recovery(flow_state);
		return 0;
	}

	zcv_filter_add(&gm.zcvf);
	zcv_filter_dump(&gm.zcvf);
	zcv_check(&gm.zcvf);

	gm3_log_notify(flow_state);

	if (gm.g_fgd_pid != 0) {
		struct fgd_nl_msg_t *fgd_msg;
		int size = FGD_NL_MSG_T_HDR_LEN + sizeof(flow_state);

		if (size > (PAGE_SIZE << 1))
			fgd_msg = vmalloc(size);
		else {
			if (in_interrupt())
				fgd_msg = kmalloc(size, GFP_ATOMIC);
		else
			fgd_msg = kmalloc(size, GFP_KERNEL);
		}

		if (fgd_msg == NULL) {
			if (size > PAGE_SIZE)
				fgd_msg = vmalloc(size);

			if (fgd_msg == NULL)
				return -1;
		}

		bm_debug("[%s] malloc size=%d pid=%d cmd:%d\n",
			__func__,
			size, gm.g_fgd_pid, flow_state);
		memset(fgd_msg, 0, size);
		fgd_msg->fgd_cmd = FG_DAEMON_CMD_NOTIFY_DAEMON;
		memcpy(fgd_msg->fgd_data, &flow_state, sizeof(flow_state));
		fgd_msg->fgd_data_len += sizeof(flow_state);
		nl_send_to_user(gm.g_fgd_pid, 0, fgd_msg);

		kvfree(fgd_msg);

		return 0;
	} else {
		return -1;
	}
}

int wakeup_fg_algo_cmd(unsigned int flow_state, int cmd, int para1)
{
	update_fg_dbg_tool_value();

	if (gm.disableGM30) {
		bm_err("FG daemon is disabled\n");
		return -1;
	}

	if (is_recovery_mode()) {
		wakeup_fg_algo_recovery(flow_state);
		return 0;
	}

	gm3_log_notify(flow_state);

	if (gm.g_fgd_pid != 0) {
		struct fgd_nl_msg_t *fgd_msg;
		int size = FGD_NL_MSG_T_HDR_LEN + sizeof(flow_state);

		if (size > (PAGE_SIZE << 1))
			fgd_msg = vmalloc(size);
		else {
			if (in_interrupt())
				fgd_msg = kmalloc(size, GFP_ATOMIC);
		else
			fgd_msg = kmalloc(size, GFP_KERNEL);

		}

		if (fgd_msg == NULL) {
			if (size > PAGE_SIZE)
				fgd_msg = vmalloc(size);

			if (fgd_msg == NULL)
				return -1;
		}

		bm_debug(
			"[wakeup_fg_algo] malloc size=%d pid=%d cmd:%d\n",
			size, gm.g_fgd_pid, flow_state);
		memset(fgd_msg, 0, size);
		fgd_msg->fgd_cmd = FG_DAEMON_CMD_NOTIFY_DAEMON;
		fgd_msg->fgd_subcmd = cmd;
		fgd_msg->fgd_subcmd_para1 = para1;
		memcpy(fgd_msg->fgd_data, &flow_state, sizeof(flow_state));
		fgd_msg->fgd_data_len += sizeof(flow_state);
		nl_send_to_user(gm.g_fgd_pid, 0, fgd_msg);

		kvfree(fgd_msg);

		return 0;
	} else {
		return -1;
	}
}

int wakeup_fg_algo_atomic(unsigned int flow_state)
{
	update_fg_dbg_tool_value();

	if (gm.disableGM30) {
		bm_err("FG daemon is disabled\n");
		return -1;
	}

	if (is_recovery_mode()) {
		wakeup_fg_algo_recovery(flow_state);
		return 0;
	}

	gm3_log_notify(flow_state);

	if (gm.g_fgd_pid != 0) {
		struct fgd_nl_msg_t *fgd_msg;
		int size = FGD_NL_MSG_T_HDR_LEN + sizeof(flow_state);

		if (in_interrupt())
			fgd_msg = kmalloc(size, GFP_ATOMIC);
		else
			fgd_msg = kmalloc(size, GFP_KERNEL);

		if (!fgd_msg) {
/* bm_err("Error: wakeup_fg_algo() vmalloc fail!!!\n"); */
			return -1;
		}

		bm_debug(
			"[wakeup_fg_algo] malloc size=%d pid=%d cmd:%d\n",
			size, gm.g_fgd_pid, flow_state);
		memset(fgd_msg, 0, size);
		fgd_msg->fgd_cmd = FG_DAEMON_CMD_NOTIFY_DAEMON;
		memcpy(fgd_msg->fgd_data, &flow_state, sizeof(flow_state));
		fgd_msg->fgd_data_len += sizeof(flow_state);
		nl_send_to_user(gm.g_fgd_pid, 0, fgd_msg);
		kfree(fgd_msg);
		return 0;
	} else {
		return -1;
	}
}


int fg_get_battery_temperature_for_zcv(void)
{
	return battery_main.BAT_batt_temp;
}

int battery_get_charger_zcv(void)
{
	u32 zcv = 0;

	charger_manager_get_zcv(gm.pbat_consumer, MAIN_CHARGER, &zcv);
	return zcv;
}

void fg_ocv_query_soc(int ocv)
{
	if (ocv > 50000 || ocv <= 0)
		return;

	gm.algo_req_ocv = ocv;
	wakeup_fg_algo_cmd(
		FG_INTR_KERNEL_CMD, FG_KERNEL_CMD_REQ_ALGO_DATA,
		ocv);

	bm_trace("[%s] ocv:%d\n",
		__func__, ocv);
}

void fg_test_ag_cmd(int cmd)
{
	wakeup_fg_algo_cmd(
		FG_INTR_KERNEL_CMD, FG_KERNEL_CMD_AG_LOG_TEST, cmd);

	bm_err("[%s]FG_KERNEL_CMD_AG_LOG_TEST:%d\n",
		__func__, cmd);
}

void exec_BAT_EC(int cmd, int param)
{
	int i;

	bm_err("exe_BAT_EC cmd %d, param %d\n", cmd, param);
	switch (cmd) {
	case 101:
		{
			/* Force Temperature, force_get_tbat*/
			if (param == 0xff) {
				get_ec()->fixed_temp_en = 0;
				get_ec()->fixed_temp_value = 25;
				bm_err("exe_BAT_EC cmd %d, param %d, disable\n",
					cmd, param);

			} else {
				get_ec()->fixed_temp_en = 1;
				if (param >= 100)
					get_ec()->fixed_temp_value =
						0 - (param - 100);
				else
					get_ec()->fixed_temp_value = param;
				bm_err("exe_BAT_EC cmd %d, param %d, enable\n",
					cmd, param);
			}
		}
		break;
	case 102:
		{
			/* force PTIM RAC */
			if (param == 0xff) {
				get_ec()->debug_rac_en = 0;
				get_ec()->debug_rac_value = 0;
				bm_err("exe_BAT_EC cmd %d, param %d, disable\n",
					cmd, param);

			} else {
				get_ec()->debug_rac_en = 1;
				get_ec()->debug_rac_value = param;
				bm_err("exe_BAT_EC cmd %d, param %d, enable\n",
					cmd, param);
			}
		}
		break;
	case 103:
		{
			/* force PTIM V */
			if (param == 0xff) {
				get_ec()->debug_ptim_v_en = 0;
				get_ec()->debug_ptim_v_value = 0;
				bm_err("exe_BAT_EC cmd %d, param %d, disable\n",
					cmd, param);

			} else {
				get_ec()->debug_ptim_v_en = 1;
				get_ec()->debug_ptim_v_value = param;
				bm_err("exe_BAT_EC cmd %d, param %d, enable\n",
					cmd, param);
			}

		}
		break;
	case 104:
		{
			/* force PTIM R_current */
			if (param == 0xff) {
				get_ec()->debug_ptim_r_en = 0;
				get_ec()->debug_ptim_r_value = 0;
				bm_err("exe_BAT_EC cmd %d, param %d, disable\n",
					cmd, param);

			} else {
				get_ec()->debug_ptim_r_en = 1;
				get_ec()->debug_ptim_r_value = param;
				bm_err("exe_BAT_EC cmd %d, param %d, enable\n",
					cmd, param);
			}
		}
		break;
	case 105:
		{
			/* force interrupt trigger */
			switch (param) {
			case 1:
				{
					wakeup_fg_algo(FG_INTR_TIMER_UPDATE);
				}
				break;
			case 4096:
				{
					wakeup_fg_algo(FG_INTR_NAG_C_DLTV);
				}
				break;
			case 8192:
				{
					wakeup_fg_algo(FG_INTR_FG_ZCV);
				}
				break;
			case 32768:
				{
					wakeup_fg_algo(FG_INTR_RESET_NVRAM);
				}
				break;
			case 65536:
				{
					wakeup_fg_algo(FG_INTR_BAT_PLUGOUT);
				}
				break;


			default:
				{

				}
				break;
			}
			bm_err(
				"exe_BAT_EC cmd %d, param %d, force interrupt\n",
				cmd, param);
		}
		break;
	case 106:
		{
			/* force FG Current */
			if (param == 0xff) {
				get_ec()->debug_fg_curr_en = 0;
				get_ec()->debug_fg_curr_value = 0;
				bm_err(
					"exe_BAT_EC cmd %d, param %d, disable\n",
					cmd, param);

			} else {
				get_ec()->debug_fg_curr_en = 1;
				get_ec()->debug_fg_curr_value = param;
				bm_err(
					"exe_BAT_EC cmd %d, param %d, enable\n",
					cmd, param);
			}
		}
		break;
	case 107:
		{
			/* force Battery ID */
			if (param == 0xff) {
				get_ec()->debug_bat_id_en = 0;
				get_ec()->debug_bat_id_value = 0;
				bm_err(
					"exe_BAT_EC cmd %d, param %d, disable\n",
					cmd, param);
			} else {
				get_ec()->debug_bat_id_en = 1;
				get_ec()->debug_bat_id_value = param;
				bm_err(
					"exe_BAT_EC cmd %d, param %d, enable\n",
					cmd, param);
				fg_custom_init_from_header();
			}
		}
		break;
	case 108:
		{
			/* Set D0_C_CUST */
			if (param == 0xff) {
				get_ec()->debug_d0_c_en = 0;
				get_ec()->debug_d0_c_value = 0;
				bm_err(
					"exe_BAT_EC cmd %d, param %d, disable\n",
					cmd, param);

			} else {
				get_ec()->debug_d0_c_en = 1;
				get_ec()->debug_d0_c_value = param;
				bm_err(
					"exe_BAT_EC cmd %d, param %d, enable\n",
					cmd, param);
			}
		}
		break;
	case 109:
		{
			/* Set D0_V_CUST */
			if (param == 0xff) {
				get_ec()->debug_d0_v_en = 0;
				get_ec()->debug_d0_v_value = 0;
				bm_err(
					"exe_BAT_EC cmd %d, param %d, disable\n",
					cmd, param);

			} else {
				get_ec()->debug_d0_v_en = 1;
				get_ec()->debug_d0_v_value = param;
				bm_err(
					"exe_BAT_EC cmd %d, param %d, enable\n",
					cmd, param);
			}
		}
		break;
	case 110:
		{
			/* Set UISOC_CUST */
			if (param == 0xff) {
				get_ec()->debug_uisoc_en = 0;
				get_ec()->debug_uisoc_value = 0;
				bm_err(
					"exe_BAT_EC cmd %d, param %d, disable\n",
					cmd, param);

			} else {
				get_ec()->debug_uisoc_en = 1;
				get_ec()->debug_uisoc_value = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, enable\n",
				cmd, param);
			}
		}
		break;
	case 600:
		{
			fg_cust_data.aging_diff_max_threshold = param;
			bm_err(
				"exe_BAT_EC cmd %d, aging_diff_max_threshold:%d\n",
				cmd, param);
		}
		break;
	case 601:
		{
			fg_cust_data.aging_diff_max_level = param;
			bm_err(
				"exe_BAT_EC cmd %d, aging_diff_max_level:%d\n",
				cmd, param);
		}
		break;
	case 602:
		{
			fg_cust_data.aging_factor_t_min = param;
			bm_err(
				"exe_BAT_EC cmd %d, aging_factor_t_min:%d\n",
				cmd, param);
		}
		break;
	case 603:
		{
			fg_cust_data.cycle_diff = param;
			bm_err(
				"exe_BAT_EC cmd %d, cycle_diff:%d\n",
				cmd, param);
		}
		break;
	case 604:
		{
			fg_cust_data.aging_count_min = param;
			bm_err(
				"exe_BAT_EC cmd %d, aging_count_min:%d\n",
				cmd, param);
		}
		break;
	case 605:
		{
			fg_cust_data.default_score = param;
			bm_err(
				"exe_BAT_EC cmd %d, default_score:%d\n",
				cmd, param);
		}
		break;
	case 606:
		{
			fg_cust_data.default_score_quantity = param;
			bm_err(
				"exe_BAT_EC cmd %d, default_score_quantity:%d\n",
				cmd, param);
		}
		break;
	case 607:
		{
			fg_cust_data.fast_cycle_set = param;
			bm_err(
				"exe_BAT_EC cmd %d, fast_cycle_set:%d\n",
				cmd, param);
		}
		break;
	case 608:
		{
			fg_cust_data.level_max_change_bat = param;
			bm_err(
				"exe_BAT_EC cmd %d, level_max_change_bat:%d\n",
				cmd, param);
		}
		break;
	case 609:
		{
			fg_cust_data.diff_max_change_bat = param;
			bm_err(
				"exe_BAT_EC cmd %d, diff_max_change_bat:%d\n",
				cmd, param);
		}
		break;
	case 610:
		{
			fg_cust_data.aging_tracking_start = param;
			bm_err(
				"exe_BAT_EC cmd %d, aging_tracking_start:%d\n",
				cmd, param);
		}
		break;
	case 611:
		{
			fg_cust_data.max_aging_data = param;
			bm_err(
				"exe_BAT_EC cmd %d, max_aging_data:%d\n",
				cmd, param);
		}
		break;
	case 612:
		{
			fg_cust_data.max_fast_data = param;
			bm_err(
				"exe_BAT_EC cmd %d, max_fast_data:%d\n",
				cmd, param);
		}
		break;
	case 613:
		{
			fg_cust_data.fast_data_threshold_score = param;
			bm_err(
				"exe_BAT_EC cmd %d, fast_data_threshold_score:%d\n",
				cmd, param);
		}
		break;
	case 614:
		{
			bm_err(
				"exe_BAT_EC cmd %d,FG_KERNEL_CMD_AG_LOG_TEST=%d\n",
				cmd, param);

			fg_test_ag_cmd(99);

		}
		break;
	case 701:
		{
			fg_cust_data.pseudo1_en = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, pseudo1_en\n",
				cmd, param);
		}
		break;
	case 702:
		{
			fg_cust_data.pseudo100_en = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, pseudo100_en\n",
				cmd, param);
		}
		break;
	case 703:
		{
			fg_cust_data.qmax_sel = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, qmax_sel\n",
				cmd, param);
		}
		break;
	case 704:
		{
			fg_cust_data.iboot_sel = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, iboot_sel\n",
				cmd, param);
		}
		break;
	case 705:
		{
			for (i = 0;
				i < fg_table_cust_data.active_table_number;
				i++) {
				fg_table_cust_data.fg_profile[i].pmic_min_vol =
					param * UNIT_TRANS_10;
			}
			bm_err(
				"exe_BAT_EC cmd %d, param %d, pmic_min_vol\n",
				cmd, param);
		}
		break;
	case 706:
		{
			for (i = 0;
				i < fg_table_cust_data.active_table_number;
				i++) {
				fg_table_cust_data.fg_profile[i].pon_iboot =
				param * UNIT_TRANS_10;
			}
			bm_err("exe_BAT_EC cmd %d, param %d, poweron_system_iboot\n"
				, cmd, param * UNIT_TRANS_10);
		}
		break;
	case 707:
		{
			fg_cust_data.shutdown_system_iboot = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, shutdown_system_iboot\n",
				cmd, param);
		}
		break;
	case 708:
		{
			fg_cust_data.com_fg_meter_resistance = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, com_fg_meter_resistance\n",
				cmd, param);
		}
		break;
	case 709:
		{
			fg_cust_data.r_fg_value = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, r_fg_value\n",
				cmd, param);
		}
		break;
	case 710:
		{
			fg_cust_data.q_max_sys_voltage = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, q_max_sys_voltage\n",
				cmd, param);
		}
		break;
	case 711:
		{
			fg_cust_data.loading_1_en = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, loading_1_en\n",
				cmd, param);
		}
		break;
	case 712:
		{
			fg_cust_data.loading_2_en = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, loading_2_en\n",
				cmd, param);
		}
		break;
	case 713:
		{
			fg_cust_data.aging_temp_diff = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, aging_temp_diff\n",
				cmd, param);
		}
		break;
	case 714:
		{
			fg_cust_data.aging1_load_soc = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, aging1_load_soc\n",
				cmd, param);
		}
		break;
	case 715:
		{
			fg_cust_data.aging1_update_soc = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, aging1_update_soc\n",
				cmd, param);
		}
		break;
	case 716:
		{
			fg_cust_data.aging_100_en = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, aging_100_en\n",
				cmd, param);
		}
		break;
	case 717:
		{
			fg_table_cust_data.active_table_number = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, additional_battery_table_en\n",
				cmd, param);
		}
		break;
	case 718:
		{
			fg_cust_data.d0_sel = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, d0_sel\n",
				cmd, param);
		}
		break;
	case 719:
		{
			fg_cust_data.zcv_car_gap_percentage = param;
			bm_err(
				"exe_BAT_EC cmd %d, param %d, zcv_car_gap_percentage\n",
				cmd, param);
		}
		break;
	case 720:
		{
			fg_cust_data.multi_temp_gauge0 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.multi_temp_gauge0=%d\n",
				cmd, param);
		}
		break;
	case 721:
		{
			fg_custom_data_check();
			bm_err("exe_BAT_EC cmd %d", cmd);
		}
		break;
	case 724:
		{
			fg_table_cust_data.fg_profile[0].pseudo100 =
				UNIT_TRANS_100 * param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pseudo100_t0=%d\n",
				cmd, param);
		}
		break;
	case 725:
		{
			fg_table_cust_data.fg_profile[1].pseudo100 =
				UNIT_TRANS_100 * param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pseudo100_t1=%d\n",
				cmd, param);
		}
		break;
	case 726:
		{
			fg_table_cust_data.fg_profile[2].pseudo100 =
				UNIT_TRANS_100 * param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pseudo100_t2=%d\n",
				cmd, param);
		}
		break;
	case 727:
		{
			fg_table_cust_data.fg_profile[3].pseudo100 =
				UNIT_TRANS_100 * param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pseudo100_t3=%d\n",
				cmd, param);
		}
		break;
	case 728:
		{
			fg_table_cust_data.fg_profile[4].pseudo100 =
				UNIT_TRANS_100 * param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pseudo100_t4=%d\n",
				cmd, param);
		}
		break;
	case 729:
		{
			fg_cust_data.keep_100_percent = UNIT_TRANS_100 * param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.keep_100_percent=%d\n",
				cmd, param);
		}
		break;
	case 730:
		{
			fg_cust_data.ui_full_limit_en = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.ui_full_limit_en=%d\n",
				cmd, param);
		}
		break;
	case 731:
		{
			fg_cust_data.ui_full_limit_soc0 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.ui_full_limit_soc0=%d\n",
				cmd, param);
		}
		break;
	case 732:
		{
			fg_cust_data.ui_full_limit_ith0 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.ui_full_limit_ith0=%d\n",
				cmd, param);
		}
		break;
	case 733:
		{
			fg_cust_data.ui_full_limit_soc1 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.ui_full_limit_soc1=%d\n",
				cmd, param);
		}
		break;
	case 734:
		{
			fg_cust_data.ui_full_limit_ith1 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.ui_full_limit_ith1=%d\n",
				cmd, param);
		}
		break;
	case 735:
		{
			fg_cust_data.ui_full_limit_soc2 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.ui_full_limit_soc2=%d\n",
				cmd, param);
		}
		break;
	case 736:
		{
			fg_cust_data.ui_full_limit_ith2 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.ui_full_limit_ith2=%d\n",
				cmd, param);
		}
		break;
	case 737:
		{
			fg_cust_data.ui_full_limit_soc3 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.ui_full_limit_soc3=%d\n",
				cmd, param);
		}
		break;
	case 738:
		{
			fg_cust_data.ui_full_limit_ith3 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.ui_full_limit_ith3=%d\n",
				cmd, param);
		}
		break;
	case 739:
		{
			fg_cust_data.ui_full_limit_soc4 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.ui_full_limit_soc4=%d\n",
				cmd, param);
		}
		break;
	case 740:
		{
			fg_cust_data.ui_full_limit_ith4 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.ui_full_limit_ith4=%d\n",
				cmd, param);
		}
		break;
	case 741:
		{
			fg_cust_data.ui_full_limit_time = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.ui_full_limit_time=%d\n",
				cmd, param);
		}
		break;
	case 743:
		{
			fg_table_cust_data.fg_profile[0].pmic_min_vol = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pmic_min_vol_t0=%d\n",
				cmd, param);
		}
		break;
	case 744:
		{
			fg_table_cust_data.fg_profile[1].pmic_min_vol = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pmic_min_vol_t1=%d\n",
				cmd, param);
		}
		break;
	case 745:
		{
			fg_table_cust_data.fg_profile[2].pmic_min_vol = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pmic_min_vol_t2=%d\n",
				cmd, param);
		}
		break;
	case 746:
		{
			fg_table_cust_data.fg_profile[3].pmic_min_vol = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pmic_min_vol_t3=%d\n",
				cmd, param);
		}
		break;
	case 747:
		{
			fg_table_cust_data.fg_profile[4].pmic_min_vol = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pmic_min_vol_t4=%d\n",
				cmd, param);
		}
		break;
	case 748:
		{
			fg_table_cust_data.fg_profile[0].pon_iboot = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pon_iboot_t0=%d\n",
				cmd, param);
		}
		break;
	case 749:
		{
			fg_table_cust_data.fg_profile[1].pon_iboot = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pon_iboot_t1=%d\n",
				cmd, param);
		}
		break;
	case 750:
		{
			fg_table_cust_data.fg_profile[2].pon_iboot = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pon_iboot_t2=%d\n",
				cmd, param);
		}
		break;
	case 751:
		{
			fg_table_cust_data.fg_profile[3].pon_iboot = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pon_iboot_t3=%d\n",
				cmd, param);
		}
		break;
	case 752:
		{
			fg_table_cust_data.fg_profile[4].pon_iboot = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.pon_iboot_t4=%d\n",
				cmd, param);
		}
		break;
	case 753:
		{
			fg_table_cust_data.fg_profile[0].qmax_sys_vol = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.qmax_sys_vol_t0=%d\n",
				cmd, param);
		}
		break;
	case 754:
		{
			fg_table_cust_data.fg_profile[1].qmax_sys_vol = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.qmax_sys_vol_t1=%d\n",
				cmd, param);
		}
		break;
	case 755:
		{
			fg_table_cust_data.fg_profile[2].qmax_sys_vol = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.qmax_sys_vol_t2=%d\n",
				cmd, param);
		}
		break;
	case 756:
		{
			fg_table_cust_data.fg_profile[3].qmax_sys_vol = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.qmax_sys_vol_t3=%d\n",
				cmd, param);
		}
		break;
	case 757:
		{
			fg_table_cust_data.fg_profile[4].qmax_sys_vol = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.qmax_sys_vol_t4=%d\n",
				cmd, param);
		}
		break;
	case 758:
		{
			fg_cust_data.nafg_ratio = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.nafg_ratio=%d\n",
				cmd, param);
		}
		break;
	case 759:
		{
			fg_cust_data.nafg_ratio_en = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.nafg_ratio_en=%d\n",
				cmd, param);
		}
		break;
	case 760:
		{
			fg_cust_data.nafg_ratio_tmp_thr = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.nafg_ratio_tmp_thr=%d\n",
				cmd, param);
		}
		break;
	case 761:
		{
			wakeup_fg_algo(FG_INTR_BAT_CYCLE);
			bm_err(
				"exe_BAT_EC cmd %d, bat_cycle intr\n", cmd);
		}
		break;
	case 762:
		{
			fg_cust_data.difference_fgc_fgv_th1 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.difference_fgc_fgv_th1=%d\n",
				cmd, param);
		}
		break;
	case 763:
		{
			fg_cust_data.difference_fgc_fgv_th2 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.difference_fgc_fgv_th1=%d\n",
				cmd, param);
		}
		break;
	case 764:
		{
			fg_cust_data.difference_fgc_fgv_th3 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.difference_fgc_fgv_th3=%d\n",
				cmd, param);
		}
		break;
	case 765:
		{
			fg_cust_data.difference_fullocv_ith = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.difference_fullocv_ith=%d\n",
				cmd, param);
		}
		break;
	case 766:
		{
			fg_cust_data.difference_fgc_fgv_th_soc1 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.difference_fgc_fgv_th_soc1=%d\n",
				cmd, param);
		}
		break;
	case 767:
		{
			fg_cust_data.difference_fgc_fgv_th_soc2 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.difference_fgc_fgv_th_soc2=%d\n",
				cmd, param);
		}
		break;
	case 768:
		{
			fg_cust_data.embedded_sel = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.embedded_sel=%d\n",
				cmd, param);
		}
		break;
	case 769:
		{
			fg_cust_data.car_tune_value = param * UNIT_TRANS_10;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.car_tune_value=%d\n",
				cmd, param * UNIT_TRANS_10);
		}
		break;
	case 770:
		{
			fg_cust_data.shutdown_1_time = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.shutdown_1_time=%d\n",
				cmd, param);
		}
		break;
	case 771:
		{
			fg_cust_data.tnew_told_pon_diff = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.tnew_told_pon_diff=%d\n",
				cmd, param);
		}
		break;
	case 772:
		{
			fg_cust_data.tnew_told_pon_diff2 = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.tnew_told_pon_diff2=%d\n",
				cmd, param);
		}
		break;
	case 773:
		{
			fg_cust_data.swocv_oldocv_diff = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.swocv_oldocv_diff=%d\n",
				cmd, param);
		}
		break;
	case 774:
		{
			fg_cust_data.hwocv_oldocv_diff = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.hwocv_oldocv_diff=%d\n",
				cmd, param);
		}
		break;
	case 775:
		{
			fg_cust_data.hwocv_swocv_diff = param;
			bm_err(
				"exe_BAT_EC cmd %d, fg_cust_data.hwocv_swocv_diff=%d\n",
				cmd, param);
		}
		break;
	case 776:
		{
			get_ec()->debug_kill_daemontest = param;
			bm_err(
				"exe_BAT_EC cmd %d, debug_kill_daemontest=%d\n",
				cmd, param);
		}
		break;
	case 777:
		{
			fg_cust_data.swocv_oldocv_diff_emb = param;
			bm_err("exe_BAT_EC cmd %d, swocv_oldocv_diff_emb=%d\n",
				cmd, param);
		}
		break;
	case 778:
		{
			fg_cust_data.vir_oldocv_diff_emb_lt = param;
			bm_err(
				"exe_BAT_EC cmd %d, vir_oldocv_diff_emb_lt=%d\n",
				cmd, param);
		}
		break;
	case 779:
		{
			fg_cust_data.vir_oldocv_diff_emb_tmp = param;
			bm_err(
				"exe_BAT_EC cmd %d, vir_oldocv_diff_emb_tmp=%d\n",
				cmd, param);
		}
		break;
	case 780:
		{
			fg_cust_data.vir_oldocv_diff_emb = param;
			bm_err("exe_BAT_EC cmd %d, vir_oldocv_diff_emb=%d\n",
				cmd, param);
		}
		break;
	case 781:
		{
			get_ec()->debug_kill_daemontest = 1;
			fg_cust_data.dod_init_sel = 12;
			bm_err("exe_BAT_EC cmd %d,force goto dod_init12=%d\n",
				cmd, param);
		}
		break;
	case 782:
		{
			fg_cust_data.charge_pseudo_full_level = param;
			bm_err(
				"exe_BAT_EC cmd %d,fg_cust_data.charge_pseudo_full_level=%d\n",
				cmd, param);
		}
		break;
	case 783:
		{
			fg_cust_data.full_tracking_bat_int2_multiply = param;
			bm_err(
				"exe_BAT_EC cmd %d,fg_cust_data.full_tracking_bat_int2_multiply=%d\n",
				cmd, param);
		}
		break;
	case 784:
		{
			bm_err(
				"exe_BAT_EC cmd %d,DAEMON_PID=%d\n",
				cmd, gm.g_fgd_pid);
		}
		break;
	case 785:
		{
			gm.bat_cycle_thr = param;
			bm_err(
				"exe_BAT_EC cmd %d,thr=%d\n",
				cmd, gm.bat_cycle_thr);
		}
		break;
	case 786:
		{
			gm.bat_cycle_ncar = param;

			bm_err(
				"exe_BAT_EC cmd %d,thr=%d\n",
				cmd, gm.bat_cycle_ncar);
		}
		break;
	case 787:
		{
			fg_ocv_query_soc(param);
			bm_err(
				"exe_BAT_EC cmd %d,[fg_ocv_query_soc]ocv=%d\n",
				cmd, param);
		}
		break;
	case 788:
		{
			fg_cust_data.record_log = param;

			bm_err(
				"exe_BAT_EC cmd %d,record_log=%d\n",
				cmd, fg_cust_data.record_log);
		}
		break;
	case 789:
		{
			int zcv_avg_current = 0;

			zcv_avg_current = param;
			gauge_set_zcv_interrupt_en(0);
			gauge_dev_set_zcv_interrupt_threshold(gm.gdev,
				zcv_avg_current);
			gauge_set_zcv_interrupt_en(1);

			bm_err(
				"exe_BAT_EC cmd %d,zcv_avg_current =%d\n",
				cmd, param);
		}
		break;
	case 790:
		{
			bm_err(
				"exe_BAT_EC cmd %d,force DLPT shutdown", cmd);

			notify_fg_dlpt_sd();
		}
		break;
	case 791:
		{
			gm.enable_tmp_intr_suspend = param;
			bm_err(
				"exe_BAT_EC cmd %d,gm.enable_tmp_intr_suspend =%d\n",
				cmd, param);
		}
		break;
	case 792:
		{
			wakeup_fg_algo_cmd(
				FG_INTR_KERNEL_CMD,
				FG_KERNEL_CMD_BUILD_SEL_BATTEMP,
				param);

			bm_err(
				"exe_BAT_EC cmd %d,req bat table temp =%d\n",
				cmd, param);

		}
		break;
	case 793:
		{

			wakeup_fg_algo_cmd(
				FG_INTR_KERNEL_CMD,
				FG_KERNEL_CMD_UPDATE_AVG_BATTEMP,
				param);

			bm_err(
				"exe_BAT_EC cmd %d,update mavg temp\n",
				cmd);

		}
		break;
	case 794:
		{
			wakeup_fg_algo_cmd(
				FG_INTR_KERNEL_CMD,
				FG_KERNEL_CMD_SAVE_DEBUG_PARAM,
				param);

			bm_err(
				"exe_BAT_EC cmd %d,FG_KERNEL_CMD_SAVE_DEBUG_PARAM\n",
				cmd);
		}
		break;
	case 795:
		{
			wakeup_fg_algo_cmd(
				FG_INTR_KERNEL_CMD,
				FG_KERNEL_CMD_REQ_CHANGE_AGING_DATA,
				param * 100);

			bm_err(
				"exe_BAT_EC cmd %d,change aging to=%d\n",
				cmd, param);
		}
		break;
	case 796:
		{
			bm_err(
				"exe_BAT_EC cmd %d,FG_KERNEL_CMD_AG_LOG_TEST=%d\n",
				cmd, param);

			wakeup_fg_algo_cmd(
				FG_INTR_KERNEL_CMD,
				FG_KERNEL_CMD_AG_LOG_TEST, param);
		}
		break;
	case 797:
		{
			gm.soc_decimal_rate = param;
			bm_err(
				"exe_BAT_EC cmd %d,soc_decimal_rate=%d\n",
				cmd, param);

		}
		break;
	case 798:
		{
			bm_err(
				"exe_BAT_EC cmd %d,FG_KERNEL_CMD_CHG_DECIMAL_RATE=%d\n",
				cmd, param);

			gm.soc_decimal_rate = param;

			wakeup_fg_algo_cmd(
				FG_INTR_KERNEL_CMD,
				FG_KERNEL_CMD_CHG_DECIMAL_RATE, param);
		}
		break;
	case 799:
		{
			bm_err(
				"exe_BAT_EC cmd %d, Send INTR_CHR_FULL to daemon, force_full =%d\n",
				cmd, param);

			gm.is_force_full = param;
			wakeup_fg_algo(FG_INTR_CHR_FULL);
		}
		break;
	case 800:
		{

			bm_err(
				"exe_BAT_EC cmd %d, charge_power_sel =%d\n",
				cmd, param);

			set_charge_power_sel(param);
		}
		break;
	case 801:
		{
			bm_err(
				"exe_BAT_EC cmd %d, charge_power_sel =%d\n",
				cmd, param);

			dump_pseudo100(param);
		}
		break;
	case 802:
		{
			fg_cust_data.zcv_com_vol_limit = param;

			bm_err(
				"exe_BAT_EC cmd %d,zcv_com_vol_limit =%d\n",
				cmd, param);
		}
		break;
	case 803:
		{
			fg_cust_data.sleep_current_avg = param;

			bm_err(
				"exe_BAT_EC cmd %d,sleep_current_avg =%d\n",
				cmd, param);
		}
		break;


	default:
		bm_err(
			"exe_BAT_EC cmd %d, param %d, default\n",
			cmd, param);
		break;
	}

}

static ssize_t show_FG_daemon_disable(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_trace("[FG] show FG disable : %d\n", gm.disableGM30);
	return sprintf(buf, "%d\n", gm.disableGM30);
}

static ssize_t store_FG_daemon_disable(
	struct device *dev, struct device_attribute *attr,
					const char *buf, size_t size)
{
	bm_err("[disable FG daemon]\n");
	disable_fg();
	if (gm.disableGM30 == true)
		battery_main.BAT_CAPACITY = 50;
	battery_update(&battery_main);
	return size;
}
static DEVICE_ATTR(
	FG_daemon_disable, 0664,
	show_FG_daemon_disable, store_FG_daemon_disable);

static ssize_t show_FG_meter_resistance(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_trace(
		"[FG] show com_fg_meter_resistance : %d\n",
		fg_cust_data.com_fg_meter_resistance);
	return sprintf(buf, "%d\n", fg_cust_data.com_fg_meter_resistance);
}

static ssize_t store_FG_meter_resistance(
	struct device *dev, struct device_attribute *attr,
					const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	bm_err("[%s]\n", __func__);

	if (buf != NULL && size != 0) {
		bm_err("[%s] buf is %s\n",
			__func__, buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			bm_err(
				"[%s] val is %d ??\n",
				__func__,
				(int)val);
			val = 0;
		} else
			fg_cust_data.com_fg_meter_resistance = val;

		bm_err(
			"[%s] com FG_meter_resistance = %d\n",
			__func__,
			(int)val);
	}

	return size;

}
static DEVICE_ATTR(
	FG_meter_resistance, 0664,
	show_FG_meter_resistance, store_FG_meter_resistance);

static ssize_t show_FG_nafg_disable(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_trace("[FG] show nafg disable : %d\n", gm.cmd_disable_nafg);
	return sprintf(buf, "%d\n", gm.cmd_disable_nafg);
}

static ssize_t store_FG_nafg_disable(
	struct device *dev, struct device_attribute *attr,
					const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	bm_err("[%s]\n", __func__);

	if (buf != NULL && size != 0) {
		bm_err("[%s] buf is %s\n",
			__func__, buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			bm_err(
				"[%s] val is %d ??\n",
				__func__,
				(int)val);
			val = 0;
		}

		if (val == 0)
			gm.cmd_disable_nafg = false;
		else
			gm.cmd_disable_nafg = true;

		wakeup_fg_algo_cmd(
			FG_INTR_KERNEL_CMD, FG_KERNEL_CMD_DISABLE_NAFG, val);

		bm_err(
			"[%s] FG_nafg_disable = %d\n",
			__func__,
			(int)val);
	}


	return size;
}
static DEVICE_ATTR(
	disable_nafg, 0664,
	show_FG_nafg_disable, store_FG_nafg_disable);

static ssize_t show_FG_ntc_disable_nafg(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_trace("[FG]%s: %d\n", __func__, gm.ntc_disable_nafg);
	return sprintf(buf, "%d\n", gm.ntc_disable_nafg);
}

static ssize_t store_FG_ntc_disable_nafg(
	struct device *dev, struct device_attribute *attr,
					const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	bm_err("[%s]\n",  __func__);

	if (buf != NULL && size != 0) {
		bm_err("[%s] buf is %s\n", __func__, buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			bm_err(
				"[%s] val is %d ??\n",  __func__,
				(int)val);
			val = 0;
		}

		if (val == 0) {
			gm.ntc_disable_nafg = false;
			get_ec()->fixed_temp_en = 0;
			get_ec()->fixed_temp_value = 25;
		} else if (val == 1) {
			get_ec()->fixed_temp_en = 1;
			get_ec()->fixed_temp_value =
				BATTERY_TMP_TO_DISABLE_NAFG;
			gm.ntc_disable_nafg = true;
			wakeup_fg_algo(FG_INTR_NAG_C_DLTV);
		}

		bm_err(
			"[%s]val=%d, temp:%d %d, %d\n",
			 __func__,
			(int)val,
			get_ec()->fixed_temp_en,
			get_ec()->fixed_temp_value,
			gm.ntc_disable_nafg);
	}

	return size;
}
static DEVICE_ATTR(
	ntc_disable_nafg, 0664,
	show_FG_ntc_disable_nafg, store_FG_ntc_disable_nafg);


static ssize_t show_uisoc_update_type(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_trace("[FG] %s : %d\n",
		__func__,
		fg_cust_data.uisoc_update_type);
	return sprintf(buf, "%d\n", fg_cust_data.uisoc_update_type);
}

static ssize_t store_uisoc_update_type(
	struct device *dev, struct device_attribute *attr,
					const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	bm_err("[%s]\n", __func__);

	if (buf != NULL && size != 0) {
		bm_err("[%s] buf is %s\n",
			__func__, buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			bm_err(
				"[%s] val is %d ??\n",
				__func__,
				(int)val);
			val = 0;
		}

		if (val >= 0 && val <= 2) {
			fg_cust_data.uisoc_update_type = val;
			wakeup_fg_algo_cmd(
				FG_INTR_KERNEL_CMD,
				FG_KERNEL_CMD_UISOC_UPDATE_TYPE,
				val);
			bm_err(
				"[%s] type = %d\n",
				__func__,
				(int)val);
		} else
			bm_err(
			"[%s] invalid type:%d\n",
			__func__,
			(int)val);
	}

	return size;
}
static DEVICE_ATTR(uisoc_update_type, 0664,
	show_uisoc_update_type, store_uisoc_update_type);

static ssize_t show_FG_daemon_log_level(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{
	static int loglevel_count;

	loglevel_count++;
	if (loglevel_count % 5 == 0)
		bm_err(
		"[FG] show FG_daemon_log_level : %d\n",
		fg_cust_data.daemon_log_level);

	return sprintf(buf, "%d\n", fg_cust_data.daemon_log_level);
}

static ssize_t store_FG_daemon_log_level(
	struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	bm_err("[FG_daemon_log_level]\n");

	if (buf != NULL && size != 0) {
		bm_err("[FG_daemon_log_level] buf is %s\n", buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			bm_err(
				"[FG_daemon_log_level] val is %d ??\n",
				(int)val);
			val = 0;
		}

		if (val < 10 && val >= 0) {
			fg_cust_data.daemon_log_level = val;
			wakeup_fg_algo_cmd(
				FG_INTR_KERNEL_CMD,
				FG_KERNEL_CMD_CHANG_LOGLEVEL,
				val
			);

			gm.d_log_level = val;
			gm.log_level = val;
		}
		if (val >= 7)
			gauge_coulomb_set_log_level(3);
		else
			gauge_coulomb_set_log_level(0);

		bm_err(
			"[FG_daemon_log_level]fg_cust_data.daemon_log_level=%d\n",
			fg_cust_data.daemon_log_level);
	}
	return size;
}
static DEVICE_ATTR(FG_daemon_log_level, 0664,
	show_FG_daemon_log_level, store_FG_daemon_log_level);

static ssize_t show_shutdown_cond_enable(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_trace(
		"[FG] %s : %d\n",
		__func__,
		get_shutdown_cond_flag());
	return sprintf(buf, "%d\n", get_shutdown_cond_flag());
}

static ssize_t store_shutdown_cond_enable(
	struct device *dev, struct device_attribute *attr,
					const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	bm_err("[%s]\n", __func__);
	if (buf != NULL && size != 0) {
		bm_err("[%s] buf is %s\n",
			__func__, buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			bm_err(
				"[%s] val is %d ??\n",
				__func__,
				(int)val);
			val = 0;
		}

		set_shutdown_cond_flag(val);

		bm_err(
			"[%s] shutdown_cond_enabled=%d\n",
			__func__,
			get_shutdown_cond_flag());
	}

	return size;
}
static DEVICE_ATTR(
	shutdown_condition_enable, 0664,
	show_shutdown_cond_enable, store_shutdown_cond_enable);

static ssize_t show_reset_battery_cycle(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_trace("[FG] %s : %d\n",
		__func__,
		gm.is_reset_battery_cycle);
	return sprintf(buf, "%d\n", gm.is_reset_battery_cycle);
}

static ssize_t store_reset_battery_cycle(
	struct device *dev, struct device_attribute *attr,
					const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	bm_err("[%s]\n", __func__);
	if (buf != NULL && size != 0) {
		bm_err("[%s] buf is %s\n",
			__func__, buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			bm_err(
				"[%s] val is %d ??\n",
				__func__,
				(int)val);
			val = 0;
		}
		if (val == 0)
			gm.is_reset_battery_cycle = false;
		else {
			gm.is_reset_battery_cycle = true;
			wakeup_fg_algo(FG_INTR_BAT_CYCLE);
		}
		bm_err(
			"%s=%d\n",
			__func__,
			gm.is_reset_battery_cycle);
	}

	return size;
}
static DEVICE_ATTR(
	reset_battery_cycle, 0664,
	show_reset_battery_cycle, store_reset_battery_cycle);

static ssize_t show_reset_aging_factor(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_trace("[FG] %s : %d\n",
		__func__,
		gm.is_reset_aging_factor);
	return sprintf(buf, "%d\n", gm.is_reset_aging_factor);
}

static ssize_t store_reset_aging_factor(
	struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	bm_err("[%s]\n", __func__);
	if (buf != NULL && size != 0) {
		bm_err("[%s] buf is %s\n",
			__func__, buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			bm_err(
				"[%s] val is %d ??\n",
				__func__,
				(int)val);
			val = 0;
		}
		if (val == 0)
			gm.is_reset_aging_factor = false;
		else if (val == 1) {
			gm.is_reset_aging_factor = true;
			wakeup_fg_algo_cmd(FG_INTR_KERNEL_CMD,
				FG_KERNEL_CMD_RESET_AGING_FACTOR, 0);
		}
		bm_err(
			"%s=%d\n",
			__func__,
			gm.is_reset_aging_factor);
	}

	return size;
}

static DEVICE_ATTR(
	reset_aging_factor, 0664,
	show_reset_aging_factor, store_reset_aging_factor);


static ssize_t show_BAT_EC(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_err("%s\n", __func__);

	return sprintf(buf, "%d:%d\n", gm.BAT_EC_cmd, gm.BAT_EC_param);
}

static ssize_t store_BAT_EC(
	struct device *dev, struct device_attribute *attr,
	const char *buf, size_t size)
{
	int ret1 = 0, ret2 = 0;
	char cmd_buf[4], param_buf[16];

	bm_err("%s\n", __func__);
	cmd_buf[3] = '\0';
	param_buf[15] = '\0';

	if (size < 4 || size > 20) {
		bm_err("%s error, size mismatch\n",
			__func__);
		return -1;
	}

	if (buf != NULL && size != 0) {
		bm_err("buf is %s\n", buf);
		cmd_buf[0] = buf[0];
		cmd_buf[1] = buf[1];
		cmd_buf[2] = buf[2];
		cmd_buf[3] = '\0';

		if ((size - 4) > 0) {
			strncpy(param_buf, buf + 4, size - 4);
			param_buf[size - 4 - 1] = '\0';
			bm_err("[FG_IT]cmd_buf %s, param_buf %s\n",
				cmd_buf, param_buf);
			ret2 = kstrtouint(param_buf, 10, &gm.BAT_EC_param);
		}

		ret1 = kstrtouint(cmd_buf, 10, &gm.BAT_EC_cmd);

		if (ret1 != 0 || ret2 != 0) {
			bm_err("ERROR! not valid number! %d %d\n",
				ret1, ret2);
			return -1;
		}
		bm_err("CMD is:%d, param:%d\n",
			gm.BAT_EC_cmd, gm.BAT_EC_param);
	}


	exec_BAT_EC(gm.BAT_EC_cmd, gm.BAT_EC_param);

	return size;
}
static DEVICE_ATTR(BAT_EC, 0664, show_BAT_EC, store_BAT_EC);



static ssize_t show_BAT_HEALTH(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_err("%s\n", __func__);

	return 0;
}


static ssize_t store_BAT_HEALTH(
	struct device *dev, struct device_attribute *attr,
	const char *buf, size_t size)
{
	char copy_str[7], buf_str[350];
	char *s = buf_str, *pch;
	/* char *ori = buf_str; */
	int chr_size = 0;
	int i = 0, count = 0, value[50];


	bm_err("%s, size =%d, str=%s\n", __func__, size, buf);

	strncpy(buf_str, buf, size);
	/* bm_err("%s, copy str=%s\n", __func__, buf_str); */

	if (size > 350) {
		bm_err("%s error, size mismatch\n", __func__);
		return -1;
	}

	if (buf != NULL && size != 0) {

		pch = strchr(s, ',');
		while (pch != NULL) {
			memset(copy_str, 0, 7);
			copy_str[6] = '\0';

			chr_size = pch - s;
			if (count == 0)
				strncpy(copy_str, s, chr_size);
			else
				strncpy(copy_str, s+1, chr_size-1);

			kstrtoint(copy_str, 10, &value[count]);
			/* bm_err("::%s::count:%d,%d\n", copy_str, count, value[count]); */
			s = pch;
			pch = strchr(pch + 1, ',');
			count++;
		}
	}

	if (count == 46) {
		for (i = 0; i < 43; i++)
			gm.bh_data.data[i] = value[i];

		for (i = 0; i < 3; i++)
			gm.bh_data.times[i].tv_sec = value[i+43];

	bm_err("%s count=%d,serial=%d,source=%d,42:%d, value43:[%d, %ld],value45[%d %ld]\n",
		__func__,
		count, gm.bh_data.data[0], gm.bh_data.data[1],
		gm.bh_data.data[42],
		value[43], gm.bh_data.times[0].tv_sec,
		value[45], gm.bh_data.times[2].tv_sec);

		wakeup_fg_algo_cmd(
			FG_INTR_KERNEL_CMD,
			FG_KERNEL_CMD_SEND_BH_DATA, 0);

		mdelay(4);
		bm_err("%s wakeup DONE~~~\n", __func__);
	} else
		bm_err("%s count=%d, number not match\n", __func__, count);


	return size;

}

static DEVICE_ATTR(BAT_HEALTH, 0664, show_BAT_HEALTH, store_BAT_HEALTH);

static ssize_t show_FG_Battery_CurrentConsumption(
struct device *dev, struct device_attribute *attr,
						  char *buf)
{
	int ret_value = 8888;

	ret_value = battery_get_bat_avg_current();
	bm_err("[EM] FG_Battery_CurrentConsumption : %d .1mA\n", ret_value);
	return sprintf(buf, "%d\n", ret_value);
}

static ssize_t store_FG_Battery_CurrentConsumption(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t size)
{
	bm_err("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(
	FG_Battery_CurrentConsumption, 0664, show_FG_Battery_CurrentConsumption,
		   store_FG_Battery_CurrentConsumption);

/* /////////////////////////////////////*/
/* // Create File For EM : Power_On_Voltage */
/* /////////////////////////////////////*/
static ssize_t show_Power_On_Voltage(
struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = 3450;
	bm_err("[EM] Power_On_Voltage : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Power_On_Voltage(
	struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t size)
{
	bm_err("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(
	Power_On_Voltage, 0664, show_Power_On_Voltage, store_Power_On_Voltage);

/* /////////////////////////////////////////// */
/* // Create File For EM : Power_Off_Voltage */
/* /////////////////////////////////////////// */
static ssize_t show_Power_Off_Voltage(
struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = 3400;
	bm_err("[EM] Power_Off_Voltage : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Power_Off_Voltage(
	struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t size)
{
	bm_err("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(
	Power_Off_Voltage, 0664,
	show_Power_Off_Voltage, store_Power_Off_Voltage);


static int battery_callback(
	struct notifier_block *nb, unsigned long event, void *v)
{
	bm_err("%s:%ld\n",
		__func__, event);
	switch (event) {
	case CHARGER_NOTIFY_EOC:
		{
/* CHARGING FULL */
			notify_fg_chr_full();
		}
		break;
	case CHARGER_NOTIFY_START_CHARGING:
		{
/* START CHARGING */
			fg_sw_bat_cycle_accu();

			battery_main.BAT_STATUS = POWER_SUPPLY_STATUS_CHARGING;
			battery_update(&battery_main);
		}
		break;
	case CHARGER_NOTIFY_STOP_CHARGING:
		{
/* STOP CHARGING */
			fg_sw_bat_cycle_accu();
			battery_main.BAT_STATUS =
			POWER_SUPPLY_STATUS_DISCHARGING;
			battery_update(&battery_main);
		}
		break;
	case CHARGER_NOTIFY_ERROR:
		{
/* charging enter error state */
		battery_main.BAT_STATUS = POWER_SUPPLY_STATUS_NOT_CHARGING;
		battery_update(&battery_main);
		}
		break;
	case CHARGER_NOTIFY_NORMAL:
		{
/* charging leave error state */
		battery_main.BAT_STATUS = POWER_SUPPLY_STATUS_CHARGING;
		battery_update(&battery_main);

		}
		break;

	default:
		{
		}
		break;
	}

	return NOTIFY_DONE;
}

/********** adc_cdev*******************/
signed int battery_meter_meta_tool_cali_car_tune(int meta_current)
{
	int cali_car_tune = 0;
	int ret = 0;

	if (meta_current == 0)
		return fg_cust_data.car_tune_value * 10;

	ret = gauge_dev_enable_car_tune_value_calibration(
		gm.gdev, meta_current, &cali_car_tune);

	return cali_car_tune;		/* 1000 base */
}

#if IS_ENABLED(CONFIG_COMPAT)
static long compat_adc_cali_ioctl(
struct file *filp, unsigned int cmd, unsigned long arg)
{
	int adc_out_datas[2] = { 1, 1 };

	bm_notice("%s 32bit IOCTL, cmd=0x%08x\n",
		__func__, cmd);
	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		bm_err("%s file has no f_op or no f_op->unlocked_ioctl.\n",
			__func__);
		return -ENOTTY;
	}

	if (sizeof(arg) != sizeof(adc_out_datas))
		return -EFAULT;

	switch (cmd) {
	case BAT_STATUS_READ:
	case ADC_CHANNEL_READ:
	case Get_META_BAT_VOL:
	case Get_META_BAT_SOC:
	case Get_META_BAT_CAR_TUNE_VALUE:
	case Set_META_BAT_CAR_TUNE_VALUE:
	case Set_BAT_DISABLE_NAFG:
	case Set_CARTUNE_TO_KERNEL: {
		bm_notice(
			"%s send to unlocked_ioctl cmd=0x%08x\n",
			__func__,
			cmd);
		return filp->f_op->unlocked_ioctl(
			filp, cmd,
			(unsigned long)compat_ptr(arg));
	}
		break;
	default:
		bm_err("%s unknown IOCTL: 0x%08x, %d\n",
			__func__, cmd, adc_out_datas[0]);
		break;
	}

	return 0;
}
#endif

static long adc_cali_ioctl(
	struct file *file, unsigned int cmd, unsigned long arg)
{
	int *user_data_addr;
	int *naram_data_addr;
	int i = 0;
	int ret = 0;
	int adc_in_data[2] = { 1, 1 };
	int adc_out_data[2] = { 1, 1 };
	int temp_car_tune;
	int isdisNAFG = 0;

	bm_notice("%s enter\n", __func__);
	mutex_lock(&gm.fg_mutex);
	user_data_addr = (int *)arg;
	ret = copy_from_user(adc_in_data, user_data_addr, sizeof(adc_in_data));
	if (adc_in_data[1] < 0) {
		bm_err("%s unknown data: %d\n", __func__, adc_in_data[1]);
		mutex_unlock(&gm.fg_mutex);
		return -EFAULT;
	}

	switch (cmd) {
	case TEST_ADC_CALI_PRINT:
		g_ADC_Cali = false;
		break;

	case SET_ADC_CALI_Slop:
		naram_data_addr = (int *)arg;
		ret = copy_from_user(adc_cali_slop, naram_data_addr, 36);
		g_ADC_Cali = false;
		/* Protection */
		for (i = 0; i < 14; i++) {
			if ((*(adc_cali_slop + i) == 0)
				|| (*(adc_cali_slop + i) == 1))
				*(adc_cali_slop + i) = 1000;
		}
		for (i = 0; i < 14; i++)
			bm_notice("adc_cali_slop[%d] = %d\n", i,
				    *(adc_cali_slop + i));
		bm_notice("**** unlocked_ioctl : SET_ADC_CALI_Slop Done!\n");
		break;

	case SET_ADC_CALI_Offset:
		naram_data_addr = (int *)arg;
		ret = copy_from_user(adc_cali_offset, naram_data_addr, 36);
		g_ADC_Cali = false;
		for (i = 0; i < 14; i++)
			bm_notice("adc_cali_offset[%d] = %d\n", i,
				    *(adc_cali_offset + i));
		bm_notice("**** unlocked_ioctl : SET_ADC_CALI_Offset Done!\n");
		break;

	case SET_ADC_CALI_Cal:
		naram_data_addr = (int *)arg;
		ret = copy_from_user(adc_cali_cal, naram_data_addr, 4);
		g_ADC_Cali = true;
		if (adc_cali_cal[0] == 1)
			g_ADC_Cali = true;
		else
			g_ADC_Cali = false;

		for (i = 0; i < 1; i++)
			bm_notice("adc_cali_cal[%d] = %d\n", i,
				    *(adc_cali_cal + i));
		bm_notice("**** unlocked_ioctl : SET_ADC_CALI_Cal Done!\n");
		break;

	case ADC_CHANNEL_READ:
		/* g_ADC_Cali = KAL_FALSE; *//* 20100508 Infinity */
		if (adc_in_data[0] == 0) {
			/* I_SENSE */
			adc_out_data[0] =
				battery_get_bat_voltage() * adc_in_data[1];
		} else if (adc_in_data[0] == 1) {
			/* BAT_SENSE */
			adc_out_data[0] =
				battery_get_bat_voltage() * adc_in_data[1];
		} else if (adc_in_data[0] == 3) {
			/* V_Charger */
			adc_out_data[0] = battery_get_vbus() * adc_in_data[1];
			/* adc_out_data[0] = adc_out_data[0] / 100; */
		} else if (adc_in_data[0] == 30) {
			/* V_Bat_temp magic number */
			adc_out_data[0] =
				battery_get_bat_temperature() * adc_in_data[1];
		} else if (adc_in_data[0] == 66)
			adc_out_data[0] = (battery_get_bat_current()) / 10;
		else {
			bm_notice("unknown channel(%d,%d)\n",
				    adc_in_data[0], adc_in_data[1]);
		}

		if (adc_out_data[0] < 0)
			adc_out_data[1] = 1;	/* failed */
		else
			adc_out_data[1] = 0;	/* success */

		if (adc_in_data[0] == 30)
			adc_out_data[1] = 0;	/* success */

		if (adc_in_data[0] == 66)
			adc_out_data[1] = 0;	/* success */

		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		bm_notice(
			    "**** unlocked_ioctl : Channel %d * %d times = %d\n",
			    adc_in_data[0], adc_in_data[1], adc_out_data[0]);
		break;

	case BAT_STATUS_READ:
		user_data_addr = (int *)arg;
		ret = copy_from_user(battery_in_data, user_data_addr, 4);
		/* [0] is_CAL */
		if (g_ADC_Cali)
			battery_out_data[0] = 1;
		else
			battery_out_data[0] = 0;
		ret = copy_to_user(user_data_addr, battery_out_data, 4);
		bm_notice(
			"unlocked_ioctl : CAL:%d\n", battery_out_data[0]);
		break;
#if 0
	case Set_Charger_Current:	/* For Factory Mode */
		user_data_addr = (int *)arg;
		ret = copy_from_user(charging_level_data, user_data_addr, 4);
		g_ftm_battery_flag = KAL_TRUE;
		if (charging_level_data[0] == 0)
			charging_level_data[0] = CHARGE_CURRENT_70_00_MA;
		else if (charging_level_data[0] == 1)
			charging_level_data[0] = CHARGE_CURRENT_200_00_MA;
		else if (charging_level_data[0] == 2)
			charging_level_data[0] = CHARGE_CURRENT_400_00_MA;
		else if (charging_level_data[0] == 3)
			charging_level_data[0] = CHARGE_CURRENT_450_00_MA;
		else if (charging_level_data[0] == 4)
			charging_level_data[0] = CHARGE_CURRENT_550_00_MA;
		else if (charging_level_data[0] == 5)
			charging_level_data[0] = CHARGE_CURRENT_650_00_MA;
		else if (charging_level_data[0] == 6)
			charging_level_data[0] = CHARGE_CURRENT_700_00_MA;
		else if (charging_level_data[0] == 7)
			charging_level_data[0] = CHARGE_CURRENT_800_00_MA;
		else if (charging_level_data[0] == 8)
			charging_level_data[0] = CHARGE_CURRENT_900_00_MA;
		else if (charging_level_data[0] == 9)
			charging_level_data[0] = CHARGE_CURRENT_1000_00_MA;
		else if (charging_level_data[0] == 10)
			charging_level_data[0] = CHARGE_CURRENT_1100_00_MA;
		else if (charging_level_data[0] == 11)
			charging_level_data[0] = CHARGE_CURRENT_1200_00_MA;
		else if (charging_level_data[0] == 12)
			charging_level_data[0] = CHARGE_CURRENT_1300_00_MA;
		else if (charging_level_data[0] == 13)
			charging_level_data[0] = CHARGE_CURRENT_1400_00_MA;
		else if (charging_level_data[0] == 14)
			charging_level_data[0] = CHARGE_CURRENT_1500_00_MA;
		else if (charging_level_data[0] == 15)
			charging_level_data[0] = CHARGE_CURRENT_1600_00_MA;
		else
			charging_level_data[0] = CHARGE_CURRENT_450_00_MA;
		wake_up_bat();
		bm_notice("**** unlocked_ioctl : set_Charger_Current:%d\n",
			    charging_level_data[0]);
		break;
#endif
		/* add for meta tool------------------------------- */
	case Get_META_BAT_VOL:
		adc_out_data[0] = battery_get_bat_voltage();
		if (copy_to_user(user_data_addr, adc_out_data,
			sizeof(adc_out_data))) {
			mutex_unlock(&gm.fg_mutex);
			return -EFAULT;
		}

		bm_notice("**** unlocked_ioctl :Get_META_BAT_VOL Done!\n");
		break;
	case Get_META_BAT_SOC:
		adc_out_data[0] = battery_get_uisoc();

		if (copy_to_user(user_data_addr, adc_out_data,
			sizeof(adc_out_data))) {
			mutex_unlock(&gm.fg_mutex);
			return -EFAULT;
		}

		bm_notice("**** unlocked_ioctl :Get_META_BAT_SOC Done!\n");
		break;

	case Get_META_BAT_CAR_TUNE_VALUE:
		adc_out_data[0] = fg_cust_data.car_tune_value;
		bm_err("Get_BAT_CAR_TUNE_VALUE, res=%d\n", adc_out_data[0]);

		if (copy_to_user(user_data_addr, adc_out_data,
			sizeof(adc_out_data))) {
			mutex_unlock(&gm.fg_mutex);
			return -EFAULT;
		}

		bm_notice("**** unlocked_ioctl :Get_META_BAT_CAR_TUNE_VALUE Done!\n");
		break;

	case Set_META_BAT_CAR_TUNE_VALUE:
		/* meta tool input: adc_in_data[1] (mA)*/
		/* Send cali_current to hal to calculate car_tune_value*/
		temp_car_tune =
			battery_meter_meta_tool_cali_car_tune(adc_in_data[1]);

		/* return car_tune_value to meta tool in adc_out_data[0] */
		if (temp_car_tune >= 900 && temp_car_tune <= 1100)
			fg_cust_data.car_tune_value = temp_car_tune;
		else
			bm_err("car_tune_value invalid:%d\n",
			temp_car_tune);

		adc_out_data[0] = temp_car_tune;

		if (copy_to_user(user_data_addr, adc_out_data,
			sizeof(adc_out_data))) {
			mutex_unlock(&gm.fg_mutex);
			return -EFAULT;
		}


		bm_err("**** unlocked_ioctl Set_BAT_CAR_TUNE_VALUE[%d], result=%d, ret=%d\n",
			adc_in_data[1], adc_out_data[0], ret);

		break;

	case Set_BAT_DISABLE_NAFG:
		isdisNAFG = adc_in_data[1];

		if (isdisNAFG == 1) {
			gm.cmd_disable_nafg = true;
			wakeup_fg_algo_cmd(
				FG_INTR_KERNEL_CMD,
				FG_KERNEL_CMD_DISABLE_NAFG, 1);
		} else if (isdisNAFG == 0) {
			gm.cmd_disable_nafg = false;
			wakeup_fg_algo_cmd(
				FG_INTR_KERNEL_CMD,
				FG_KERNEL_CMD_DISABLE_NAFG, 0);
		}
		bm_debug("unlocked_ioctl Set_BAT_DISABLE_NAFG,isdisNAFG=%d [%d]\n",
			isdisNAFG, adc_in_data[1]);
		break;

		/* add bing meta tool------------------------------- */
	case Set_CARTUNE_TO_KERNEL:
		temp_car_tune = adc_in_data[1];
		if (temp_car_tune > 500 && temp_car_tune < 1500)
			fg_cust_data.car_tune_value = temp_car_tune;

		bm_err("**** unlocked_ioctl Set_CARTUNE_TO_KERNEL[%d,%d], ret=%d\n",
			adc_in_data[0], adc_in_data[1], ret);
		break;
	default:
		bm_err("**** unlocked_ioctl unknown IOCTL: 0x%08x\n", cmd);
		g_ADC_Cali = false;
		mutex_unlock(&gm.fg_mutex);
		return -EINVAL;
	}

	mutex_unlock(&gm.fg_mutex);

	return 0;
}

static int adc_cali_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int adc_cali_release(struct inode *inode, struct file *file)
{
	return 0;
}

static unsigned long bat_node;

static int fb_early_init_dt_get_chosen(
	unsigned long node, const char *uname, int depth, void *data)
{
	if (depth != 1 || (strcmp(uname, "chosen") != 0
		&& strcmp(uname, "chosen@0") != 0))
		return 0;
	bat_node = node;
	return 1;
}


static const struct file_operations adc_cali_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = adc_cali_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = compat_adc_cali_ioctl,
#endif
	.open = adc_cali_open,
	.release = adc_cali_release,
};


/*************************************/
static struct wakeup_source *battery_lock;
static int __init battery_probe(struct platform_device *dev)
{
	int ret_device_file = 0;
	int ret;
	struct class_device *class_dev = NULL;
	const char *fg_swocv_v = NULL;
	char fg_swocv_v_tmp[10];
	int fg_swocv_v_len = 0;
	const char *fg_swocv_i = NULL;
	char fg_swocv_i_tmp[10];
	int fg_swocv_i_len = 0;
	const char *shutdown_time = NULL;
	char shutdown_time_tmp[10];
	int shutdown_time_len = 0;
	const char *boot_voltage = NULL;
	char boot_voltage_tmp[10];
	int boot_voltage_len = 0;

	battery_lock = wakeup_source_register(NULL, "battery wakelock");
	__pm_stay_awake(battery_lock);

/********* adc_cdev **********/
	ret = alloc_chrdev_region(&adc_cali_devno, 0, 1, ADC_CALI_DEVNAME);
	if (ret)
		bm_notice("Error: Can't Get Major number for adc_cali\n");
	adc_cali_cdev = cdev_alloc();
	adc_cali_cdev->owner = THIS_MODULE;
	adc_cali_cdev->ops = &adc_cali_fops;
	ret = cdev_add(adc_cali_cdev, adc_cali_devno, 1);
	if (ret)
		bm_notice("adc_cali Error: cdev_add\n");
	adc_cali_major = MAJOR(adc_cali_devno);
	adc_cali_class = class_create(THIS_MODULE, ADC_CALI_DEVNAME);
	class_dev = (struct class_device *)device_create(adc_cali_class,
							 NULL,
					adc_cali_devno,
					NULL, ADC_CALI_DEVNAME);
/*****************************/

	mtk_battery_init(dev);

	/* Power supply class */
#if !defined(CONFIG_MTK_DISABLE_GAUGE)
	battery_main.psy =
		power_supply_register(
			&(dev->dev), &battery_main.psd, NULL);
	if (IS_ERR(battery_main.psy)) {
		bm_err("[BAT_probe] power_supply_register Battery Fail !!\n");
		ret = PTR_ERR(battery_main.psy);
		return ret;
	}
	bm_err("[BAT_probe] power_supply_register Battery Success !!\n");
#endif
	ret = device_create_file(&(dev->dev), &dev_attr_Battery_Temperature);
	ret = device_create_file(&(dev->dev), &dev_attr_UI_SOC);

	/* sysfs node */
	ret_device_file = device_create_file(&(dev->dev),
		&dev_attr_uisoc_update_type);
	ret_device_file = device_create_file(&(dev->dev),
		&dev_attr_disable_nafg);
	ret_device_file = device_create_file(&(dev->dev),
		&dev_attr_ntc_disable_nafg);
	ret_device_file = device_create_file(&(dev->dev),
		&dev_attr_FG_meter_resistance);
	ret_device_file = device_create_file(&(dev->dev),
		&dev_attr_FG_daemon_log_level);
	ret_device_file = device_create_file(&(dev->dev),
		&dev_attr_FG_daemon_disable);
	ret_device_file = device_create_file(&(dev->dev),
		&dev_attr_BAT_EC);
	ret_device_file = device_create_file(&(dev->dev),
		&dev_attr_BAT_HEALTH);
	ret_device_file = device_create_file(&(dev->dev),
		&dev_attr_FG_Battery_CurrentConsumption);
	ret_device_file = device_create_file(&(dev->dev),
		&dev_attr_Power_On_Voltage);
	ret_device_file = device_create_file(&(dev->dev),
		&dev_attr_Power_Off_Voltage);
	ret_device_file = device_create_file(&(dev->dev),
		&dev_attr_shutdown_condition_enable);
	ret_device_file = device_create_file(&(dev->dev),
		&dev_attr_reset_battery_cycle);
	ret_device_file = device_create_file(&(dev->dev),
		&dev_attr_reset_aging_factor);

	if (of_scan_flat_dt(fb_early_init_dt_get_chosen, NULL) > 0)
		fg_swocv_v =
		of_get_flat_dt_prop(
			bat_node, "atag,fg_swocv_v",
			&fg_swocv_v_len);
	if (fg_swocv_v == NULL) {
		bm_err(" fg_swocv_v == NULL len = %d\n", fg_swocv_v_len);
	} else {
		snprintf(fg_swocv_v_tmp, (fg_swocv_v_len + 1),
			"%s", fg_swocv_v);
		ret = kstrtoint(fg_swocv_v_tmp, 10, &gm.ptim_lk_v);
		bm_err(" fg_swocv_v = %s len %d fg_swocv_v_tmp %s ptim_lk_v[%d]\n",
			fg_swocv_v, fg_swocv_v_len,
			fg_swocv_v_tmp, gm.ptim_lk_v);
	}

	if (of_scan_flat_dt(fb_early_init_dt_get_chosen, NULL) > 0)
		fg_swocv_i = of_get_flat_dt_prop(
		bat_node, "atag,fg_swocv_i", &fg_swocv_i_len);
	if (fg_swocv_i == NULL) {
		bm_err(" fg_swocv_i == NULL len = %d\n", fg_swocv_i_len);
	} else {
		snprintf(
			fg_swocv_i_tmp, (fg_swocv_i_len + 1),
			"%s", fg_swocv_i);
		ret = kstrtoint(fg_swocv_i_tmp, 10, &gm.ptim_lk_i);

		bm_err(" fg_swocv_i = %s len %d fg_swocv_i_tmp %s ptim_lk_i[%d]\n",
			fg_swocv_i, fg_swocv_i_len,
			fg_swocv_i_tmp, gm.ptim_lk_i);
	}

	if (of_scan_flat_dt(fb_early_init_dt_get_chosen, NULL) > 0)
		shutdown_time =
			of_get_flat_dt_prop(
				bat_node,
				"atag,shutdown_time",
				&shutdown_time_len);
	if (shutdown_time == NULL) {
		bm_err(" shutdown_time == NULL len = %d\n", shutdown_time_len);
	} else {
		snprintf(shutdown_time_tmp,
			(shutdown_time_len + 1),
			"%s", shutdown_time);
		ret = kstrtoint(shutdown_time_tmp, 10, &gm.pl_shutdown_time);

		bm_err(" shutdown_time = %s len %d shutdown_time_tmp %s pl_shutdown_time[%d]\n",
			shutdown_time, shutdown_time_len,
			shutdown_time_tmp, gm.pl_shutdown_time);
	}

	if (of_scan_flat_dt(fb_early_init_dt_get_chosen, NULL) > 0)
		boot_voltage =
		of_get_flat_dt_prop(
			bat_node, "atag,boot_voltage",
			&boot_voltage_len);
	if (boot_voltage == NULL) {
		bm_err(" boot_voltage == NULL len = %d\n", boot_voltage_len);
	} else {
		snprintf(
			boot_voltage_tmp, (boot_voltage_len + 1),
			"%s", boot_voltage);
		ret = kstrtoint(boot_voltage_tmp, 10, &gm.pl_bat_vol);

		bm_err(
			"boot_voltage=%s len %d boot_voltage_tmp %s pl_bat_vol[%d]\n",
			boot_voltage, boot_voltage_len,
			boot_voltage_tmp, gm.pl_bat_vol);
	}

	gm.pbat_consumer = charger_manager_get_by_name(&(dev->dev), "charger");
	if (gm.pbat_consumer != NULL) {
		gm.bat_nb.notifier_call = battery_callback;
		register_charger_manager_notifier(gm.pbat_consumer, &gm.bat_nb);
	}

	battery_debug_init();

	__pm_relax(battery_lock);

	if (IS_ENABLED(CONFIG_POWER_EXT) || gm.disable_mtkbattery ||
		IS_ENABLED(CONFIG_MTK_DISABLE_GAUGE)) {
		bm_err("disable GM 3.0\n");
		disable_fg();
	} else if (is_recovery_mode())
		battery_recovery_init();

	mtk_battery_last_init(dev);


	gm.is_probe_done = true;

	return 0;
}

void battery_shutdown(struct platform_device *dev)
{
	int fg_coulomb = 0;
	int shut_car_diff = 0;
	int verify_car = 0;

	fg_coulomb = gauge_get_coulomb();
	if (gm.d_saved_car != 0) {
		shut_car_diff = fg_coulomb - gm.d_saved_car;
		gauge_dev_set_info(gm.gdev, GAUGE_SHUTDOWN_CAR, shut_car_diff);
		/* ready for writing to PMIC_RG */
	}
	gauge_dev_get_info(gm.gdev, GAUGE_SHUTDOWN_CAR, &verify_car);

	bm_err("******** %s!! car=[o:%d,new:%d,diff:%d v:%d]********\n",
		__func__,
		gm.d_saved_car, fg_coulomb, shut_car_diff, verify_car);

	gm.fix_coverity = 1;
}

static int battery_suspend(struct platform_device *dev, pm_message_t state)
{
	bm_err("******** %s!! iavg=%d ***GM3 disable:%d %d %d %d tmp_intr:%d***\n",
		__func__,
		gm.hw_status.iavg_intr_flag,
		gm.disableGM30,
		fg_cust_data.disable_nafg,
		gm.ntc_disable_nafg,
		gm.cmd_disable_nafg,
		gm.enable_tmp_intr_suspend);

	if (gm.enable_tmp_intr_suspend == 0) {
		gauge_enable_interrupt(FG_RG_INT_EN_BAT_TEMP_H, 0);
		gauge_enable_interrupt(FG_RG_INT_EN_BAT_TEMP_L, 0);
		enable_bat_temp_det(0);
	}


	if (gauge_get_hw_version() >= GAUGE_HW_V2000
		&& gm.hw_status.iavg_intr_flag == 1) {
		gauge_enable_interrupt(FG_IAVG_H_NO, 0);
		if (gm.hw_status.iavg_lt > 0)
			gauge_enable_interrupt(FG_IAVG_L_NO, 0);
	}
	return 0;
}

static int battery_resume(struct platform_device *dev)
{
	zcv_filter_add(&gm.zcvf);
	zcv_filter_dump(&gm.zcvf);
	zcv_check(&gm.zcvf);

	bm_err("******** %s!! iavg=%d ***GM3 disable:%d %d %d %d***\n",
		__func__,
		gm.hw_status.iavg_intr_flag,
		gm.disableGM30,
		fg_cust_data.disable_nafg,
		gm.ntc_disable_nafg,
		gm.cmd_disable_nafg);
	if (gauge_get_hw_version() >=
		GAUGE_HW_V2000
		&& gm.hw_status.iavg_intr_flag == 1) {
		gauge_enable_interrupt(FG_IAVG_H_NO, 1);
		if (gm.hw_status.iavg_lt > 0)
			gauge_enable_interrupt(FG_IAVG_L_NO, 1);
	}
	/* reset nafg monitor time to avoid suspend for too long case */
	get_monotonic_boottime(&gm.last_nafg_update_time);

	fg_update_sw_iavg();

	if (gm.enable_tmp_intr_suspend == 0) {
		gauge_enable_interrupt(FG_RG_INT_EN_BAT_TEMP_H, 1);
		gauge_enable_interrupt(FG_RG_INT_EN_BAT_TEMP_L, 1);
		enable_bat_temp_det(1);
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mtk_bat_of_match[] = {
	{.compatible = "mediatek,bat_gm30",},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_bat_of_match);
#endif


struct platform_device battery_device = {
	.name = "battery",
	.id = -1,
};

static struct platform_driver battery_driver_probe = {
	.probe = battery_probe,
	.remove = NULL,
	.shutdown = battery_shutdown,
	.suspend = battery_suspend,
	.resume = battery_resume,
	.driver = {
		.name = "battery",
#ifdef CONFIG_OF
		.of_match_table = mtk_bat_of_match,
#endif
	},
};

static int __init battery_init(void)
{
	struct netlink_kernel_cfg cfg = {
		.input = nl_data_handler,
	};

	int ret;

	gm.daemo_nl_sk = netlink_kernel_create(&init_net, NETLINK_FGD, &cfg);
	bm_err("netlink_kernel_create protol= %d\n", NETLINK_FGD);

	if (gm.daemo_nl_sk == NULL) {
		bm_err("netlink_kernel_create error\n");
		return -1;
	}
	bm_err("netlink_kernel_create ok\n");

#ifdef CONFIG_OF
	/* register battery_device by DTS */
#else
	ret = platform_device_register(&battery_device);
#endif

	ret = platform_driver_register(&battery_driver_probe);

	bm_err("[%s] Initialization : DONE\n",
		__func__);

	return 0;
}

static void __exit battery_exit(void)
{

}
module_init(battery_init);
module_exit(battery_exit);

MODULE_AUTHOR("Weiching Lin");
MODULE_DESCRIPTION("Battery Device Driver");
MODULE_LICENSE("GPL");
