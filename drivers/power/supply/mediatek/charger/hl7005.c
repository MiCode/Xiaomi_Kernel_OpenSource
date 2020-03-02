/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/alarmtimer.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>
#include <linux/version.h>
#include <linux/i2c.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#endif
#include "upmu_common.h"
#include "hl7005.h"
#include "mtk_charger_intf.h"

const unsigned int VBAT_CVTH[] = {
	3500000, 3520000, 3540000, 3560000,
	3580000, 3600000, 3620000, 3640000,
	3660000, 3680000, 3700000, 3720000,
	3740000, 3760000, 3780000, 3800000,
	3820000, 3840000, 3860000, 3880000,
	3900000, 3920000, 3940000, 3960000,
	3980000, 4000000, 4020000, 4040000,
	4060000, 4080000, 4100000, 4120000,
	4140000, 4160000, 4180000, 4200000,
	4220000, 4240000, 4260000, 4280000,
	4300000, 4320000, 4340000, 4360000,
	4380000, 4400000, 4420000, 4440000
};

const unsigned int CSTH[] = {
	550000, 650000, 750000, 850000,
	950000, 1050000, 1150000, 1250000
};

/*hl7005 REG00 IINLIM[5:0]*/
const unsigned int INPUT_CSTH[] = {
	100000, 500000, 800000, 5000000
};

/* hl7005 REG0A BOOST_LIM[2:0], mA */
const unsigned int BOOST_CURRENT_LIMIT[] = {
	500, 750, 1200, 1400, 1650, 1875, 2150,
};

struct hl7005_info {
	struct charger_device *chg_dev;
	struct charger_properties chg_props;
	struct device *dev;
	struct alarm otg_kthread_gtimer;
	struct workqueue_struct *otg_boost_workq;
	struct work_struct kick_work;
	unsigned int polling_interval;
	bool polling_enabled;

	const char *chg_dev_name;
	const char *eint_name;
	enum charger_type chg_type;
	int irq;
};

static struct hl7005_info *g_info;
static struct i2c_client *new_client;
static const struct i2c_device_id hl7005_i2c_id[] = { {"hl7005", 0}, {} };

static void enable_boost_polling(bool poll_en);
static void usbotg_boost_kick_work(struct work_struct *work);
static enum alarmtimer_restart usbotg_gtimer_func(struct alarm *alarm,
						 ktime_t now);

unsigned int charging_value_to_parameter(const unsigned int *parameter,
					const unsigned int array_size,
					const unsigned int val)
{
	if (val < array_size)
		return parameter[val];
	pr_info("Can't find the parameter\n");
	return parameter[0];
}

unsigned int charging_parameter_to_value(const unsigned int *parameter,
					const unsigned int array_size,
					const unsigned int val)
{
	unsigned int i;

	pr_debug_ratelimited("array_size = %d\n", array_size);

	for (i = 0; i < array_size; i++) {
		if (val == *(parameter + i))
			return i;
	}

	pr_info("NO register value match\n");

	return 0;
}

static unsigned int bmt_find_closest_level(const unsigned int *pList,
					  unsigned int number,
					  unsigned int level)
{
	unsigned int i;
	unsigned int max_value_in_last_element;

	if (pList[0] < pList[1])
		max_value_in_last_element = 1;
	else
		max_value_in_last_element = 0;

	if (max_value_in_last_element == 1) {
		for (i = (number - 1); i != 0; i--) {
			if (pList[i] <= level) {
				pr_debug_ratelimited("zzf_%d<=%d, i=%d\n",
						    pList[i], level, i);
				return pList[i];
			}
		}
		pr_info("Can't find closest level\n");
		return pList[0];
		/* return 000; */
	} else {
		for (i = 0; i < number; i++) {
			if (pList[i] <= level)
				return pList[i];
		}
		pr_info("Can't find closest level\n");
		return pList[number - 1];
		/* return 000; */
	}
}

unsigned char hl7005_reg[HL7005_REG_NUM] = { 0 };
static DEFINE_MUTEX(hl7005_i2c_access);
static DEFINE_MUTEX(hl7005_access_lock);

static int hl7005_read_byte(u8 reg_addr, u8 *rd_buf, int rd_len)
{
	int ret = 0;
	struct i2c_adapter *adap = new_client->adapter;
	struct i2c_msg msg[2];
	u8 *w_buf = NULL;
	u8 *r_buf = NULL;

	memset(msg, 0, 2 * sizeof(struct i2c_msg));

	w_buf = kzalloc(1, GFP_KERNEL);
	if (w_buf == NULL)
		return -1;
	r_buf = kzalloc(rd_len, GFP_KERNEL);
	if (r_buf == NULL)
		return -1;

	*w_buf = reg_addr;

	msg[0].addr = new_client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = w_buf;

	msg[1].addr = new_client->addr;
	msg[1].flags = 1;
	msg[1].len = rd_len;
	msg[1].buf = r_buf;

	ret = i2c_transfer(adap, msg, 2);

	memcpy(rd_buf, r_buf, rd_len);

	kfree(w_buf);
	kfree(r_buf);
	return ret;
}

int hl7005_write_byte(unsigned char reg_num, u8 *wr_buf, int wr_len)
{
	int ret = 0;
	struct i2c_adapter *adap = new_client->adapter;
	struct i2c_msg msg;
	u8 *w_buf = NULL;

	memset(&msg, 0, sizeof(struct i2c_msg));

	w_buf = kzalloc(wr_len, GFP_KERNEL);
	if (w_buf == NULL)
		return -1;

	w_buf[0] = reg_num;
	memcpy(w_buf + 1, wr_buf, wr_len);

	msg.addr = new_client->addr;
	msg.flags = 0;
	msg.len = wr_len;
	msg.buf = w_buf;

	ret = i2c_transfer(adap, &msg, 1);

	kfree(w_buf);
	return ret;
}

unsigned int hl7005_read_interface(unsigned char reg_num, unsigned char *val,
				  unsigned char MASK, unsigned char SHIFT)
{
	unsigned char hl7005_reg = 0;
	unsigned int ret = 0;

	ret = hl7005_read_byte(reg_num, &hl7005_reg, 1);
	pr_debug_ratelimited("hl7005 Reg[%x] = 0x%x\n", reg_num, hl7005_reg);
	hl7005_reg &= (MASK << SHIFT);
	*val = (hl7005_reg >> SHIFT);
	pr_debug_ratelimited("hl7005 val = 0x%x\n", *val);

	return ret;
}

unsigned int hl7005_config_interface(unsigned char reg_num, unsigned char val,
				     unsigned char MASK, unsigned char SHIFT)
{
	unsigned char hl7005_reg = 0;
	unsigned char hl7005_reg_ori = 0;
	unsigned int ret = 0;

	mutex_lock(&hl7005_access_lock);
	ret = hl7005_read_byte(reg_num, &hl7005_reg, 1);
	hl7005_reg_ori = hl7005_reg;
	hl7005_reg &= ~(MASK << SHIFT);
	hl7005_reg |= (val << SHIFT);
	if (reg_num == HL7005_CON4)
		hl7005_reg &= ~(1 << CON4_RESET_SHIFT);

	ret = hl7005_write_byte(reg_num, &hl7005_reg, 2);
	mutex_unlock(&hl7005_access_lock);
	pr_debug_ratelimited("hl7005 write Reg[%x]=0x%x from 0x%x\n", reg_num,
			hl7005_reg, hl7005_reg_ori);

	return ret;
}

/* write one register directly */
unsigned int hl7005_reg_config_interface(unsigned char reg_num,
					unsigned char val)
{
	unsigned char hl7005_reg = val;

	return hl7005_write_byte(reg_num, &hl7005_reg, 2);
}

void hl7005_set_tmr_rst(unsigned int val)
{
	hl7005_config_interface((unsigned char)(HL7005_CON0),
				(unsigned char)(val),
				(unsigned char)(CON0_TMR_RST_MASK),
				(unsigned char)(CON0_TMR_RST_SHIFT)
				);
}

unsigned int hl7005_get_otg_status(void)
{
	unsigned char val = 0;

	hl7005_read_interface((unsigned char)(HL7005_CON0),
				(unsigned char *)(&val),
				(unsigned char)(CON0_OTG_MASK),
				(unsigned char)(CON0_OTG_SHIFT)
				);
	return val;
}

void hl7005_set_en_stat(unsigned int val)
{
	hl7005_config_interface((unsigned char)(HL7005_CON0),
				(unsigned char)(val),
				(unsigned char)(CON0_EN_STAT_MASK),
				(unsigned char)(CON0_EN_STAT_SHIFT)
				);
}

unsigned int hl7005_get_chip_status(void)
{
	unsigned char val = 0;

	hl7005_read_interface((unsigned char)(HL7005_CON0),
				(unsigned char *)(&val),
				(unsigned char)(CON0_STAT_MASK),
				(unsigned char)(CON0_STAT_SHIFT)
				);
	return val;
}

unsigned int hl7005_get_boost_status(void)
{
	unsigned char val = 0;

	hl7005_read_interface((unsigned char)(HL7005_CON0),
				(unsigned char *)(&val),
				(unsigned char)(CON0_BOOST_MASK),
				(unsigned char)(CON0_BOOST_SHIFT)
				);
	return val;

}

unsigned int hl7005_get_fault_status(void)
{
	unsigned char val = 0;

	hl7005_read_interface((unsigned char)(HL7005_CON0),
				(unsigned char *)(&val),
				(unsigned char)(CON0_FAULT_MASK),
				(unsigned char)(CON0_FAULT_SHIFT)
				);
	return val;
}

void hl7005_set_input_charging_current(unsigned int val)
{
	hl7005_config_interface((unsigned char)(HL7005_CON1),
				(unsigned char)(val),
				(unsigned char)(CON1_LIN_LIMIT_MASK),
				(unsigned char)(CON1_LIN_LIMIT_SHIFT)
				);
}

unsigned int hl7005_get_input_charging_current(void)
{
	unsigned char val = 0;

	hl7005_read_interface((unsigned char)(HL7005_CON1),
				(unsigned char *)(&val),
				(unsigned char)(CON1_LIN_LIMIT_MASK),
				(unsigned char)(CON1_LIN_LIMIT_SHIFT)
				);

	return val;
}

void hl7005_set_v_low(unsigned int val)
{

	hl7005_config_interface((unsigned char)(HL7005_CON1),
				(unsigned char)(val),
				(unsigned char)(CON1_LOW_V_MASK),
				(unsigned char)(CON1_LOW_V_SHIFT)
				);
}

void hl7005_set_te(unsigned int val)
{
	hl7005_config_interface((unsigned char)(HL7005_CON1),
				(unsigned char)(val),
				(unsigned char)(CON1_TE_MASK),
				(unsigned char)(CON1_TE_SHIFT)
				);
}

void hl7005_set_ce(unsigned int val)
{
	hl7005_config_interface((unsigned char)(HL7005_CON1),
				(unsigned char)(val),
				(unsigned char)(CON1_CE_MASK),
				(unsigned char)(CON1_CE_SHIFT)
				);
}

void hl7005_set_hz_mode(unsigned int val)
{
	hl7005_config_interface((unsigned char)(HL7005_CON1),
				(unsigned char)(val),
				(unsigned char)(CON1_HZ_MODE_MASK),
				(unsigned char)(CON1_HZ_MODE_SHIFT)
				);
}

void hl7005_set_opa_mode(unsigned int val)
{
	hl7005_config_interface((unsigned char)(HL7005_CON1),
				(unsigned char)(val),
				(unsigned char)(CON1_OPA_MODE_MASK),
				(unsigned char)(CON1_OPA_MODE_SHIFT)
				);
}

void hl7005_set_oreg(unsigned int val)
{
	hl7005_config_interface((unsigned char)(HL7005_CON2),
				(unsigned char)(val),
				(unsigned char)(CON2_OREG_MASK),
				(unsigned char)(CON2_OREG_SHIFT)
				);
}
void hl7005_set_otg_pl(unsigned int val)
{
	hl7005_config_interface((unsigned char)(HL7005_CON2),
				(unsigned char)(val),
				(unsigned char)(CON2_OTG_PL_MASK),
				(unsigned char)(CON2_OTG_PL_SHIFT)
				);
}
void hl7005_set_otg_en(unsigned int val)
{
	hl7005_config_interface((unsigned char)(HL7005_CON2),
				(unsigned char)(val),
				(unsigned char)(CON2_OTG_EN_MASK),
				(unsigned char)(CON2_OTG_EN_SHIFT)
				);
}

unsigned int hl7005_get_vender_code(void)
{
	unsigned char val = 0;

	hl7005_read_interface((unsigned char)(HL7005_CON3),
				(unsigned char *)(&val),
				(unsigned char)(CON3_VENDER_CODE_MASK),
				(unsigned char)(CON3_VENDER_CODE_SHIFT)
				);
	return val;
}

unsigned int hl7005_get_pn(void)
{
	unsigned char val = 0;

	hl7005_read_interface((unsigned char)(HL7005_CON3),
				(unsigned char *)(&val),
				(unsigned char)(CON3_PIN_MASK),
				(unsigned char)(CON3_PIN_SHIFT)
				);
	return val;
}

unsigned int hl7005_get_revision(void)
{
	unsigned char val = 0;

	hl7005_read_interface((unsigned char)(HL7005_CON3),
				(unsigned char *)(&val),
				(unsigned char)(CON3_REVISION_MASK),
				(unsigned char)(CON3_REVISION_SHIFT)
				);
	return val;
}

void hl7005_set_reset(unsigned int val)
{
	hl7005_config_interface((unsigned char)(HL7005_CON4),
				(unsigned char)(val),
				(unsigned char)(CON4_RESET_MASK),
				(unsigned char)(CON4_RESET_SHIFT)
				);
}

void hl7005_set_iocharge(unsigned int val)
{
	hl7005_config_interface((unsigned char)(HL7005_CON4),
				(unsigned char)(val),
				(unsigned char)(CON4_I_CHR_MASK),
				(unsigned char)(CON4_I_CHR_SHIFT)
				);
}

void hl7005_set_iterm(unsigned int val)
{
	hl7005_config_interface((unsigned char)(HL7005_CON4),
				(unsigned char)(val),
				(unsigned char)(CON4_I_TERM_MASK),
				(unsigned char)(CON4_I_TERM_SHIFT)
				);
}

void hl7005_set_dis_vreg(unsigned int val)
{
	hl7005_config_interface((unsigned char)(HL7005_CON5),
				(unsigned char)(val),
				(unsigned char)(CON5_DIS_VREG_MASK),
				(unsigned char)(CON5_DIS_VREG_SHIFT)
				);
}

void hl7005_set_io_level(unsigned int val)
{
	hl7005_config_interface((unsigned char)(HL7005_CON5),
				(unsigned char)(val),
				(unsigned char)(CON5_IO_LEVEL_MASK),
				(unsigned char)(CON5_IO_LEVEL_SHIFT)
				);
}

unsigned int hl7005_get_sp_status(void)
{
	unsigned char val = 0;

	hl7005_read_interface((unsigned char)(HL7005_CON5),
				(unsigned char *)(&val),
				(unsigned char)(CON5_SP_STATUS_MASK),
				(unsigned char)(CON5_SP_STATUS_SHIFT)
				);
	return val;
}

unsigned int hl7005_get_en_level(void)
{
	unsigned char val = 0;

	hl7005_read_interface((unsigned char)(HL7005_CON5),
				(unsigned char *)(&val),
				(unsigned char)(CON5_EN_LEVEL_MASK),
				(unsigned char)(CON5_EN_LEVEL_SHIFT)
				);
	return val;
}

void hl7005_set_vsp(unsigned int val)
{
	hl7005_config_interface((unsigned char)(HL7005_CON5),
				(unsigned char)(val),
				(unsigned char)(CON5_VSP_MASK),
				(unsigned char)(CON5_VSP_SHIFT)
				);
}

void hl7005_set_i_safe(unsigned int val)
{
	hl7005_config_interface((unsigned char)(HL7005_CON6),
				(unsigned char)(val),
				(unsigned char)(CON6_ISAFE_MASK),
				(unsigned char)(CON6_ISAFE_SHIFT)
				);
}

void hl7005_set_v_safe(unsigned int val)
{
	hl7005_config_interface((unsigned char)(HL7005_CON6),
				(unsigned char)(val),
				(unsigned char)(CON6_VSAFE_MASK),
				(unsigned char)(CON6_VSAFE_SHIFT)
				);
}

static int hl7005_dump_register(struct charger_device *chg_dev)
{
	int i;

	for (i = 0; i < HL7005_REG_NUM; i++) {
		hl7005_read_byte(i, &hl7005_reg[i], 1);
		pr_debug("[0x%x]=0x%x ", i, hl7005_reg[i]);
	}
	pr_debug("\n");

	return 0;
}

static int hl7005_parse_dt(struct hl7005_info *info, struct device *dev)
{
	struct device_node *np = dev->of_node;

	pr_info("%s\n", __func__);

	if (!np) {
		pr_err("%s: no of node\n", __func__);
		return -ENODEV;
	}

	if (of_property_read_string(np, "charger_name",
	   &info->chg_dev_name) < 0) {
		info->chg_dev_name = "primary_chg";
		pr_warn("%s: no charger name\n", __func__);
	}

	if (of_property_read_string(np, "alias_name",
	   &(info->chg_props.alias_name)) < 0) {
		info->chg_props.alias_name = "hl7005";
		pr_warn("%s: no alias name\n", __func__);
	}

	return 0;
}

static int hl7005_do_event(struct charger_device *chg_dev, unsigned int event,
			   unsigned int args)
{
	if (chg_dev == NULL)
		return -EINVAL;

	pr_info("%s: event = %d\n", __func__, event);

	switch (event) {
	case EVENT_EOC:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_EOC);
		break;
	case EVENT_RECHARGE:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_RECHG);
		break;
	default:
		break;
	}

	return 0;
}

static int hl7005_enable_charging(struct charger_device *chg_dev, bool en)
{
	unsigned int status = 0;

	if (en) {
		hl7005_set_ce(0);
		hl7005_set_hz_mode(0);
		hl7005_set_opa_mode(0);
	} else {
		hl7005_set_ce(1);
	}

	return status;
}

static int hl7005_set_cv_voltage(struct charger_device *chg_dev, u32 cv)
{
	int status = 0;
	unsigned short int array_size;
	unsigned int set_cv_voltage;
	unsigned short int register_value;
	/*static kal_int16 pre_register_value; */
	array_size = ARRAY_SIZE(VBAT_CVTH);
	/*pre_register_value = -1; */
	set_cv_voltage = bmt_find_closest_level(VBAT_CVTH, array_size, cv);

	register_value =
	charging_parameter_to_value(VBAT_CVTH, array_size, set_cv_voltage);
	pr_info("charging_set_cv_voltage register_value=0x%x %d %d\n",
	 register_value, cv, set_cv_voltage);
	hl7005_set_oreg(register_value);

	return status;
}

static int hl7005_get_current(struct charger_device *chg_dev, u32 *ichg)
{
	int status = 0;
	unsigned int array_size;
	unsigned char reg_value;

	array_size = ARRAY_SIZE(CSTH);
	hl7005_read_interface(0x1, &reg_value, 0x3, 0x6);
	*ichg = charging_value_to_parameter(CSTH, array_size, reg_value);

	return status;
}

static int hl7005_set_current(struct charger_device *chg_dev, u32 current_value)
{
	unsigned int status = 0;
	unsigned int set_chr_current;
	unsigned int array_size;
	unsigned int register_value;

	if (current_value <= 35000) {
		hl7005_set_io_level(1);
	} else {
		hl7005_set_io_level(0);
		array_size = ARRAY_SIZE(CSTH);
		set_chr_current = bmt_find_closest_level(CSTH,
				array_size, current_value);
		register_value = charging_parameter_to_value(CSTH, array_size,
				set_chr_current);
		hl7005_set_iocharge(register_value);
	}

	return status;
}

static int hl7005_get_input_current(struct charger_device *chg_dev, u32 *aicr)
{
	unsigned int status = 0;
	unsigned int array_size;
	unsigned int register_value;

	array_size = ARRAY_SIZE(INPUT_CSTH);
	register_value = hl7005_get_input_charging_current();
	*aicr = charging_parameter_to_value(INPUT_CSTH, array_size,
					   register_value);

	return status;
}

static int hl7005_set_input_current(struct charger_device *chg_dev,
				    u32 current_value)
{
	unsigned int status = 0;
	unsigned int set_chr_current;
	unsigned int array_size;
	unsigned int register_value;

	if (current_value > 50000) {
		register_value = 0x3;
	} else {
		array_size = ARRAY_SIZE(INPUT_CSTH);
		set_chr_current = bmt_find_closest_level(INPUT_CSTH, array_size,
					current_value);
		register_value =
	 charging_parameter_to_value(INPUT_CSTH, array_size, set_chr_current);
	}

	hl7005_set_input_charging_current(register_value);

	return status;
}

static int hl7005_get_charging_status(struct charger_device *chg_dev,
				bool *is_done)
{
	unsigned int status = 0;
	unsigned int ret_val;

	ret_val = hl7005_get_chip_status();

	if (ret_val == 0x2)
		*is_done = true;
	else
		*is_done = false;

	return status;
}

static int hl7005_reset_watch_dog_timer(struct charger_device *chg_dev)
{
	hl7005_set_tmr_rst(1);
	return 0;
}

static int hl7005_charger_enable_otg(struct charger_device *chg_dev, bool en)
{
	hl7005_set_opa_mode(en);
	enable_boost_polling(en);
	return 0;
}

static void enable_boost_polling(bool poll_en)
{
	struct timespec time, time_now, end_time;
	ktime_t ktime;

	if (g_info) {
		if (poll_en) {
			get_monotonic_boottime(&time_now);
			time.tv_sec = g_info->polling_interval;
			time.tv_nsec = 0;
			timespec_add(time_now, time);
			ktime = ktime_set(end_time.tv_sec, end_time.tv_nsec);
			alarm_start(&g_info->otg_kthread_gtimer, ktime);
			g_info->polling_enabled = true;
		} else {
			g_info->polling_enabled = false;
			alarm_cancel(&g_info->otg_kthread_gtimer);
		}
	}
}

static void usbotg_boost_kick_work(struct work_struct *work)
{
	ktime_t ktime;
	struct timespec time, time_now, end_time;
	struct hl7005_info *boost_manager =
		container_of(work, struct hl7005_info, kick_work);

	pr_debug_ratelimited("hl7005 otg detect\n");

	hl7005_set_tmr_rst(1);

	if (boost_manager->polling_enabled == true) {
		get_monotonic_boottime(&time_now);
		time.tv_sec = boost_manager->polling_interval;
		time.tv_nsec = 0;
		timespec_add(time_now, time);
		ktime = ktime_set(end_time.tv_sec, end_time.tv_nsec);
		alarm_start(&boost_manager->otg_kthread_gtimer, ktime);
	}
}

static enum alarmtimer_restart usbotg_gtimer_func(struct alarm *alarm,
						 ktime_t now)
{
	struct hl7005_info *boost_manager =
		container_of(alarm, struct hl7005_info,
			     otg_kthread_gtimer);

	queue_work(boost_manager->otg_boost_workq,
		   &boost_manager->kick_work);

	return ALARMTIMER_NORESTART;
}

static struct charger_ops hl7005_chg_ops = {

	/* Normal charging */
	.dump_registers = hl7005_dump_register,
	.enable = hl7005_enable_charging,
	.get_charging_current = hl7005_get_current,
	.set_charging_current = hl7005_set_current,
	.get_input_current = hl7005_get_input_current,
	.set_input_current = hl7005_set_input_current,
	/*.get_constant_voltage = hl7005_get_battery_voreg,*/
	.set_constant_voltage = hl7005_set_cv_voltage,
	.kick_wdt = hl7005_reset_watch_dog_timer,
	.is_charging_done = hl7005_get_charging_status,
	/* OTG */
	.enable_otg = hl7005_charger_enable_otg,
	.event = hl7005_do_event,
};

static int hl7005_driver_probe(struct i2c_client *client,
			      const struct i2c_device_id *id)
{
	int ret = 0;
	struct hl7005_info *info = NULL;

	info = devm_kzalloc(&client->dev, sizeof(struct hl7005_info),
			   GFP_KERNEL);

	if (!info)
		return -ENOMEM;

	new_client = client;
	info->dev = &client->dev;
	ret = hl7005_parse_dt(info, &client->dev);

	if (ret < 0)
		return ret;

	/* Register charger device */
	info->chg_dev = charger_device_register(info->chg_dev_name,
		&client->dev, info, &hl7005_chg_ops, &info->chg_props);

	if (IS_ERR_OR_NULL(info->chg_dev)) {
		pr_err("%s: register charger device failed\n", __func__);
		ret = PTR_ERR(info->chg_dev);
		return ret;
	}

	ret = hl7005_get_vender_code();

	if (ret != 2) {
		pr_err("%s: get vendor id failed\n", __func__);
		return -ENODEV;
	}

#if defined(HIGH_BATTERY_VOLTAGE_SUPPORT)
	/* ISAFE = 1250mA, VSAFE = 4.34V */
	hl7005_reg_config_interface(0x06, 0x77);
#else
	hl7005_reg_config_interface(0x06, 0x70);
#endif
	/* kick chip watch dog */
	hl7005_reg_config_interface(0x00, 0xC0);
	/* TE=1, CE=0, HZ_MODE=0, OPA_MODE=0 */
	hl7005_reg_config_interface(0x01, 0xb8);
	hl7005_reg_config_interface(0x05, 0x03);
	/* 146mA */
	hl7005_reg_config_interface(0x04, 0x1A);

	hl7005_dump_register(info->chg_dev);

	alarm_init(&info->otg_kthread_gtimer, ALARM_BOOTTIME,
		  usbotg_gtimer_func);

	info->otg_boost_workq =
			create_singlethread_workqueue("otg_boost_workq");
	INIT_WORK(&info->kick_work, usbotg_boost_kick_work);
	info->polling_interval = 20;
	g_info = info;

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id hl7005_of_match[] = {
	{.compatible = "halo,hl7005"},
	{},
};
#else
static struct i2c_board_info i2c_hl7005 __initdata = {
	I2C_BOARD_INFO("hl7005", (hl7005_SLAVE_ADDR_WRITE >> 1))
};
#endif

static struct i2c_driver hl7005_driver = {
	.driver = {
		.name = "hl7005",
#ifdef CONFIG_OF
		.of_match_table = hl7005_of_match,
#endif
		},
	.probe = hl7005_driver_probe,
	.id_table = hl7005_i2c_id,
};

static int __init hl7005_init(void)
{

	if (i2c_add_driver(&hl7005_driver) != 0)
		pr_info("Failed to register hl7005 i2c driver.\n");
	else
		pr_info("Success to register hl7005 i2c driver.\n");

	return 0;
}

static void __exit hl7005_exit(void)
{
	i2c_del_driver(&hl7005_driver);
}

module_init(hl7005_init);
module_exit(hl7005_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C hl7005 Driver");
MODULE_AUTHOR("Henry Chen<henryc.chen@mediatek.com>");
