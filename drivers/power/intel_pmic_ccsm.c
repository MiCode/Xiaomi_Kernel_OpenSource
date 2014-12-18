/*
 * pmic_ccsm.c - Intel MID PMIC Charger Driver
 *
 * Copyright (C) 2011 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Jenny TC <jenny.tc@intel.com>
 * Author: Yegnesh Iyer <yegnesh.s.iyer@intel.com>
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/usb/otg.h>
#include <linux/power_supply.h>
#include <linux/thermal.h>
#include <linux/iio/consumer.h>
#include <linux/notifier.h>
#include <linux/power/battery_id.h>
#include <linux/mfd/intel_soc_pmic.h>
#include "intel_pmic_ccsm.h"

/* Macros */
#define DRIVER_NAME "pmic_ccsm"
#define ADC_TO_TEMP 1
#define TEMP_TO_ADC 0
#define USB_WAKE_LOCK_TIMEOUT	(5 * HZ)

#define USBINPUTICC100VAL	100
#define HVDCP_POWER_CHRG_CURRENT 1500
#define HIGH_POWER_CHRG_CURRENT 2000
#define LOW_POWER_CHRG_CURRENT 500

#define INTERNAL_PHY_SUPPORTED(model) \
	((model == INTEL_PMIC_SCOVE) || (model == INTEL_PMIC_WCOVE))

#define NEED_ZONE_SPLIT(bprof)\
	 ((bprof->temp_mon_ranges < MIN_BATT_PROF))
#define NEXT_ZONE_OFFSET 2
#define BATTEMP_CHANNEL "BATTEMP0"
#define VBUS_CTRL_CDEV_NAME	"vbus_control"

#define RID_A_MIN 11150
#define RID_A_MAX 13640
#define RID_B_MAX 7480
#define RID_B_MIN 6120
#define RID_C_MAX 4015
#define RID_C_MIN 3285

#define IS_RID_A(rid) (rid > RID_A_MIN && rid < RID_A_MAX)
#define IS_RID_B(rid) (rid > RID_B_MIN && rid < RID_B_MAX)
#define IS_RID_C(rid) (rid > RID_C_MIN && rid < RID_C_MAX)

#define KELVIN_OFFSET	2732
#define DECI_KELVIN_TO_CELSIUS(t) ((t - KELVIN_OFFSET) / 10)
#define CELSIUS_TO_DECI_KELVIN(t) ((t * 10) + KELVIN_OFFSET)

/* Type definitions */
static void pmic_bat_zone_changed(void);
static int intel_pmic_handle_otgmode(bool enable);

/* Extern definitions */

/* Global declarations */
static DEFINE_MUTEX(pmic_lock);
static struct pmic_chrgr_drv_context chc;

u16 pmic_inlmt[][2] = {
	{ 100, CHGRCTRL1_FUSB_INLMT_100},
	{ 150, CHGRCTRL1_FUSB_INLMT_150},
	{ 500, CHGRCTRL1_FUSB_INLMT_500},
	{ 900, CHGRCTRL1_FUSB_INLMT_900},
	{ 1500, CHGRCTRL1_FUSB_INLMT_1500},
	{ 2000, CHGRCTRL1_FUSB_INLMT_1500},
	{ 2500, CHGRCTRL1_FUSB_INLMT_1500},
};

enum pmic_vbus_states {
	VBUS_ENABLE,
	VBUS_DISABLE,
	MAX_VBUSCTRL_STATES,
};

static inline struct power_supply *get_psy_battery(void)
{
	struct class_dev_iter iter;
	struct device *dev;
	struct power_supply *pst;

	class_dev_iter_init(&iter, power_supply_class, NULL, NULL);
	while ((dev = class_dev_iter_next(&iter))) {
		pst = (struct power_supply *)dev_get_drvdata(dev);
		if (pst->type == POWER_SUPPLY_TYPE_BATTERY) {
			class_dev_iter_exit(&iter);
			return pst;
		}
	}
	class_dev_iter_exit(&iter);
	return NULL;
}

static void lookup_regval(u16 tbl[][2], size_t size, u16 in_val, u8 *out_val)
{
	int i;
	for (i = 1; i < size; ++i)
		if (in_val < tbl[i][0])
			break;

	*out_val = (u8)tbl[i-1][1];
}

/* Return temperature from raw value */
static int raw_to_temp(struct temp_lookup *adc_tbl, int count, int raw)
{
	int i, delta_temp, delta_raw, temp;

	for (i = 0; i < count - 1; i++) {
		if ((raw >= adc_tbl[i].raw && raw <= adc_tbl[i+1].raw) ||
			(raw <= adc_tbl[i].raw && raw >= adc_tbl[i+1].raw))
			break;
	}

	if (i == count - 1)
		return -ENOENT;

	delta_temp = adc_tbl[i+1].temp - adc_tbl[i].temp;
	delta_raw = adc_tbl[i+1].raw - adc_tbl[i].raw;
	temp = adc_tbl[i].temp +
		(raw - adc_tbl[i].raw) * delta_temp / delta_raw;

	return DECI_KELVIN_TO_CELSIUS(temp);
}

/* Return raw value from temperature through LPAT table */
static unsigned long temp_to_raw(
	struct temp_lookup *adc_tbl, int count, int temp)
{
	int i, delta_temp, delta_raw, raw;

	temp = CELSIUS_TO_DECI_KELVIN(temp);

	for (i = 0; i < count - 1; i++) {
		if (temp >= adc_tbl[i].temp && temp <= adc_tbl[i+1].temp)
			break;
	}

	if (i == count - 1)
		return -ENOENT;

	delta_temp = adc_tbl[i+1].temp - adc_tbl[i].temp;
	delta_raw = adc_tbl[i+1].raw - adc_tbl[i].raw;
	raw = adc_tbl[i].raw +
		(temp - adc_tbl[i].temp) * delta_raw / delta_temp;

	return raw;
}

static int pmic_read_reg(u16 addr, u8 *val)
{
	int ret;
	ret = intel_soc_pmic_readb(addr);
	if (ret == -EIO) {
		dev_err(chc.dev, "%s:Error(%d): addr:data 0x%.4x\n",
				__func__, ret, addr);
		return ret;
	}
	*val = ret;
	return 0;
}

static int pmic_write_reg(u16 addr, u8 val)
{
	int ret;

	ret = intel_soc_pmic_writeb(addr, val);
	if (ret)
		dev_err(chc.dev, "%s:Error(%d): addr:data 0x%.4x:0x%.4x\n",
				__func__, ret, addr, val);
	return ret;
}

static int __pmic_write_tt(u8 addr, u8 data)
{
	int ret;

	ret = pmic_write_reg(chc.reg_map->pmic_chrttaddr, addr);
	if (!ret)
		ret = pmic_write_reg(chc.reg_map->pmic_chrttdata, data);
	return ret;
}

static inline int pmic_write_tt(u8 addr, u8 data)
{
	int ret;

	mutex_lock(&pmic_lock);
	ret = __pmic_write_tt(addr, data);
	mutex_unlock(&pmic_lock);
	return ret;
}

static int __pmic_read_tt(u8 addr, u8 *data)
{
	int ret;

	ret = pmic_write_reg(chc.reg_map->pmic_chrttaddr, addr);
	if (ret)
		return ret;

	/* Delay the TT read by 2ms to ensure that the data is populated
	 * in data register
	 */
	usleep_range(2000, 3000);

	return pmic_read_reg(chc.reg_map->pmic_chrttdata, data);
}

static inline int pmic_read_tt(u8 addr, u8 *data)
{
	int ret;

	mutex_lock(&pmic_lock);
	ret = __pmic_read_tt(addr, data);
	mutex_unlock(&pmic_lock);

	return ret;
}

static int pmic_update_tt(u8 addr, u8 mask, u8 data)
{
	u8 tdata;
	int ret;

	mutex_lock(&pmic_lock);
	ret = __pmic_read_tt(addr, &tdata);
	if (unlikely(ret))
		goto exit;

	tdata = (tdata & ~mask) | (data & mask);
	ret = __pmic_write_tt(addr, tdata);
exit:
	mutex_unlock(&pmic_lock);
	return ret;
}

#ifdef CONFIG_DEBUG_FS
static int pmic_chrgr_reg_show(struct seq_file *seq, void *unused)
{
	u16 addr;
	u8 val;

	addr = *((u8 *)seq->private);

	if (pmic_read_reg(addr, &val))
		return -EIO;

	seq_printf(seq, "0x%x\n", val);
	return 0;
}

static int pmic_chrgr_tt_reg_show(struct seq_file *seq, void *unused)
{
	u8 addr;
	u8 val;

	addr = *((u8 *)seq->private);

	if (pmic_read_tt(addr, &val))
		return -EIO;

	seq_printf(seq, "0x%x\n", val);
	return 0;
}

static int pmic_chrgr_tt_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, pmic_chrgr_tt_reg_show, inode->i_private);
}

static int pmic_chrgr_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, pmic_chrgr_reg_show, inode->i_private);
}

static struct dentry *charger_debug_dir;

static struct pmic_regs_def pmic_tt_regs[] = {
	PMIC_REG_DEF(TT_I2CDADDR_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT0OS_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT1OS_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT2OS_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT3OS_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT4OS_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT5OS_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT6OS_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT7OS_ADDR),
	PMIC_REG_DEF(TT_USBINPUTICCOS_ADDR),
	PMIC_REG_DEF(TT_USBINPUTICCMASK_ADDR),
	PMIC_REG_DEF(TT_CHRCVOS_ADDR),
	PMIC_REG_DEF(TT_CHRCVMASK_ADDR),
	PMIC_REG_DEF(TT_CHRCCOS_ADDR),
	PMIC_REG_DEF(TT_CHRCCMASK_ADDR),
	PMIC_REG_DEF(TT_LOWCHROS_ADDR),
	PMIC_REG_DEF(TT_LOWCHRMASK_ADDR),
	PMIC_REG_DEF(TT_WDOGRSTOS_ADDR),
	PMIC_REG_DEF(TT_WDOGRSTMASK_ADDR),
	PMIC_REG_DEF(TT_CHGRENOS_ADDR),
	PMIC_REG_DEF(TT_CHGRENMASK_ADDR),
	PMIC_REG_DEF(TT_CUSTOMFIELDEN_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT0VAL_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT1VAL_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT2VAL_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT3VAL_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT4VAL_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT5VAL_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT6VAL_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT7VAL_ADDR),
	PMIC_REG_DEF(TT_USBINPUTICC100VAL_ADDR),
	PMIC_REG_DEF(TT_USBINPUTICC150VAL_ADDR),
	PMIC_REG_DEF(TT_USBINPUTICC500VAL_ADDR),
	PMIC_REG_DEF(TT_USBINPUTICC900VAL_ADDR),
	PMIC_REG_DEF(TT_USBINPUTICC1500VAL_ADDR),
	PMIC_REG_DEF(TT_CHRCVEMRGLOWVAL_ADDR),
	PMIC_REG_DEF(TT_CHRCVCOLDVAL_ADDR),
	PMIC_REG_DEF(TT_CHRCVCOOLVAL_ADDR),
	PMIC_REG_DEF(TT_CHRCVWARMVAL_ADDR),
	PMIC_REG_DEF(TT_CHRCVHOTVAL_ADDR),
	PMIC_REG_DEF(TT_CHRCVEMRGHIVAL_ADDR),
	PMIC_REG_DEF(TT_CHRCCEMRGLOWVAL_ADDR),
	PMIC_REG_DEF(TT_CHRCCCOLDVAL_ADDR),
	PMIC_REG_DEF(TT_CHRCCCOOLVAL_ADDR),
	PMIC_REG_DEF(TT_CHRCCWARMVAL_ADDR),
	PMIC_REG_DEF(TT_CHRCCHOTVAL_ADDR),
	PMIC_REG_DEF(TT_CHRCCEMRGHIVAL_ADDR),
	PMIC_REG_DEF(TT_LOWCHRENVAL_ADDR),
	PMIC_REG_DEF(TT_LOWCHRDISVAL_ADDR),
};

void intel_pmic_ccsm_dump_regs(void)
{
	u8 data;
	int ret, i;
	u16 *reg;

	dev_dbg(chc.dev, "PMIC Register dump\n");
	dev_dbg(chc.dev, "====================\n");

	reg = (u16 *)chc.reg_map;

	for (i = 0; i < chc.reg_cnt; i++, reg++) {

		ret = pmic_read_reg(*reg, &data);
		if (!ret)
			dev_dbg(chc.dev, "%s=0x%x\n", pmic_regs_name[i], data);
	}
	dev_dbg(chc.dev, "====================\n");
}

void dump_pmic_tt_regs(void)
{
	u32 pmic_tt_reg_cnt = ARRAY_SIZE(pmic_tt_regs);
	u32 reg_index;
	u8 data;
	int ret;

	dev_dbg(chc.dev, "PMIC CHRGR TT dump\n");
	dev_dbg(chc.dev, "====================\n");

	for (reg_index = 0; reg_index < pmic_tt_reg_cnt; reg_index++) {

		ret = pmic_read_tt(pmic_tt_regs[reg_index].addr, &data);
		if (ret)
			dev_err(chc.dev, "Error in reading %x\n",
				pmic_tt_regs[reg_index].addr);
		else
			dev_dbg(chc.dev, "0x%x=0x%x\n",
				pmic_tt_regs[reg_index].addr, data);
	}

	dev_dbg(chc.dev, "====================\n");
}
static const struct file_operations pmic_chrgr_reg_fops = {
	.open = pmic_chrgr_reg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static const struct file_operations pmic_chrgr_tt_reg_fops = {
	.open = pmic_chrgr_tt_reg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static void pmic_debugfs_init(void)
{
	int i;
	struct dentry *fentry;
	struct dentry *pmic_regs_dir;
	struct dentry *pmic_tt_regs_dir;
	u16 *reg;
	u32 pmic_tt_reg_cnt = ARRAY_SIZE(pmic_tt_regs);
	char name[PMIC_REG_NAME_LEN] = {0};

	/* Creating a directory under debug fs for charger */
	charger_debug_dir = debugfs_create_dir(DRIVER_NAME , NULL);
	if (charger_debug_dir == NULL)
		goto debugfs_root_exit;

	/* Create a directory for pmic charger registers */
	pmic_regs_dir = debugfs_create_dir("pmic_ccsm_regs",
			charger_debug_dir);

	if (pmic_regs_dir == NULL)
		goto debugfs_err_exit;

	reg = (u16 *)chc.reg_map;
	for (i = 0; i < chc.reg_cnt; i++, reg++) {

		sprintf(name, "%s", pmic_regs_name[i]);

		fentry = debugfs_create_file(name,
				S_IRUGO,
				pmic_regs_dir,
				reg,
				&pmic_chrgr_reg_fops);

		if (fentry == NULL)
			goto debugfs_err_exit;
	}

	/* Create a directory for pmic tt charger registers */
	pmic_tt_regs_dir = debugfs_create_dir("pmic_ccsm_tt_regs",
			charger_debug_dir);

	if (pmic_tt_regs_dir == NULL)
		goto debugfs_err_exit;

	for (i = 0; i < pmic_tt_reg_cnt; i++) {

		sprintf(name, "%s", pmic_tt_regs[i].reg_name);

		fentry = debugfs_create_file(name,
				S_IRUGO,
				pmic_tt_regs_dir,
				&pmic_tt_regs[i].addr,
				&pmic_chrgr_tt_reg_fops);

		if (fentry == NULL)
			goto debugfs_err_exit;
	}

	dev_info(chc.dev, "Debugfs created successfully!!");
	return;

debugfs_err_exit:
	debugfs_remove_recursive(charger_debug_dir);
debugfs_root_exit:
	dev_err(chc.dev, "Error creating debugfs entry!!");
	return;
}

static void pmic_debugfs_exit(void)
{
	if (charger_debug_dir != NULL)
		debugfs_remove_recursive(charger_debug_dir);
}
#endif

static void pmic_bat_zone_changed(void)
{
	int cur_zone;
	u8 data = 0;
	struct power_supply *psy_bat;

	if (pmic_read_reg(chc.reg_map->pmic_thrmbatzone, &data))
		return;

	cur_zone = data & THRMBATZONE_MASK;
	dev_info(chc.dev, "Battery Zone changed. Current zone is %d\n",
			(data & THRMBATZONE_MASK));

	/* if current zone is the top and bottom zones then report OVERHEAT */
	if ((cur_zone == PMIC_BZONE_LOW) || (cur_zone == PMIC_BZONE_HIGH))
		chc.batt_health = POWER_SUPPLY_HEALTH_OVERHEAT;
	else
		chc.batt_health = POWER_SUPPLY_HEALTH_GOOD;

	psy_bat = get_psy_battery();
	if (psy_bat && psy_bat->external_power_changed)
		psy_bat->external_power_changed(psy_bat);
	else
		power_supply_changed(psy_bat);

	return;
}

int intel_pmic_get_health(void)
{
	return chc.batt_health;
}

int intel_pmic_enable_vbus(bool enable)
{
	int ret = 0;

	if (enable)
		ret = intel_soc_pmic_update(chc.reg_map->pmic_chgrctrl0,
				WDT_NOKICK_ENABLE, CHGRCTRL0_WDT_NOKICK_MASK);
	else
		ret = intel_soc_pmic_update(chc.reg_map->pmic_chgrctrl0,
				WDT_NOKICK_DISABLE, CHGRCTRL0_WDT_NOKICK_MASK);

	/* If access is blocked return success to avoid additional
	*  error handling at client side
	*/
	if (ret == -EACCES) {
		dev_warn(chc.dev, "IPC blocked due to unsigned kernel/invalid battery\n");
		ret = 0;
	}

	return ret;
}

static int intel_pmic_handle_otgmode(bool enable)
{
	int ret = 0;

	if (chc.pmic_model == INTEL_PMIC_BCOVE)
		return 0;

	if (enable)
		ret = intel_soc_pmic_update(chc.reg_map->pmic_chgrctrl1,
				CHGRCTRL1_OTGMODE_MASK,
				CHGRCTRL1_OTGMODE_MASK);
	else
		ret = intel_soc_pmic_update(chc.reg_map->pmic_chgrctrl1,
				0x0, CHGRCTRL1_OTGMODE_MASK);

	/* If access is blocked return success to avoid additional
	*  error handling at client side
	*/
	if (ret == -EACCES) {
		dev_warn(chc.dev, "IPC blocked due to unsigned kernel/invalid battery\n");
		ret = 0;
	}

	return ret;
}

int intel_pmic_enable_charging(bool enable)
{
	int ret;
	u8 val;

	if (enable) {
		ret = intel_soc_pmic_update(chc.reg_map->pmic_chgrctrl1,
			CHGRCTRL1_FTEMP_EVENT_MASK, CHGRCTRL1_FTEMP_EVENT_MASK);
		if (ret)
			return ret;
	}

	val = (enable) ? 0 : EXTCHRDIS_ENABLE;

	ret = intel_soc_pmic_update(chc.reg_map->pmic_chgrctrl0,
			val, CHGRCTRL0_EXTCHRDIS_MASK);
	/* If access is blocked return success to avoid additional
	*  error handling at client side
	*/
	if (ret == -EACCES) {
		dev_warn(chc.dev, "IPC blocked due to unsigned kernel/invalid battery\n");
		ret = 0;
	}

	return ret;
}

static inline int update_zone_cc(int zone, u8 reg_val)
{
	u8 addr_cc = TT_CHRCCHOTVAL_ADDR - zone;
	dev_dbg(chc.dev, "%s:%X=%X\n", __func__, addr_cc, reg_val);
	return pmic_write_tt(addr_cc, reg_val);
}

static inline int update_zone_cv(int zone, u8 reg_val)
{
	u8 addr_cv = TT_CHRCVHOTVAL_ADDR - zone;
	dev_dbg(chc.dev, "%s:%X=%X\n", __func__, addr_cv, reg_val);
	return pmic_write_tt(addr_cv, reg_val);
}

static inline int update_zone_temp(int zone, u16 adc_val)
{
	int ret;
	u16 addr_tzone;

	if (chc.pmic_model == INTEL_PMIC_SCOVE) {
		/* to take care of address-discontinuity of zone-registers */
		if (zone >= 3)
			zone += 1;
	}
	addr_tzone = chc.reg_map->pmic_thrmzn4h - (NEXT_ZONE_OFFSET * zone);

	ret = pmic_write_reg(addr_tzone, (u8)(adc_val >> 8));
	if (unlikely(ret))
		return ret;
	dev_dbg(chc.dev, "%s:%X:%X=%X\n", __func__, addr_tzone,
				(addr_tzone+1), adc_val);

	return pmic_write_reg(addr_tzone+1, (u8)(adc_val & 0xFF));
}

int intel_pmic_set_cc(int new_cc)
{
	struct ps_pse_mod_prof *bcprof = chc.actual_bcprof;
	struct ps_pse_mod_prof *r_bcprof = chc.runtime_bcprof;
	int temp_mon_ranges;
	int new_cc1;
	int ret;
	int i;
	u8 reg_val = 0;

	/* No need to write PMIC if CC = 0 */
	if (!new_cc)
		return 0;

	temp_mon_ranges = min_t(u16, bcprof->temp_mon_ranges,
			BATT_TEMP_NR_RNG);

	for (i = 0; i < temp_mon_ranges; ++i) {
		new_cc1 = min_t(int, new_cc,
				bcprof->temp_mon_range[i].full_chrg_cur);

		if (new_cc1 != r_bcprof->temp_mon_range[i].full_chrg_cur) {
			if (chc.pdata->cc_to_reg) {
				chc.pdata->cc_to_reg(new_cc1, &reg_val);
				ret = update_zone_cc(i, reg_val);
				if (unlikely(ret))
					return ret;
			}
			r_bcprof->temp_mon_range[i].full_chrg_cur = new_cc1;
		}
	}

	/* send the new CC and CV */
	intel_soc_pmic_update(chc.reg_map->pmic_chgrctrl1,
		CHGRCTRL1_FTEMP_EVENT_MASK, CHGRCTRL1_FTEMP_EVENT_MASK);

	return 0;
}

int intel_pmic_set_cv(int new_cv)
{
	struct ps_pse_mod_prof *bcprof = chc.actual_bcprof;
	struct ps_pse_mod_prof *r_bcprof = chc.runtime_bcprof;
	int temp_mon_ranges;
	int new_cv1;
	int ret;
	int i;
	u8 reg_val = 0;

	/* No need to write PMIC if CV = 0 */
	if (!new_cv)
		return 0;

	temp_mon_ranges = min_t(u16, bcprof->temp_mon_ranges,
			BATT_TEMP_NR_RNG);

	for (i = 0; i < temp_mon_ranges; ++i) {
		new_cv1 = min_t(int, new_cv,
				bcprof->temp_mon_range[i].full_chrg_vol);

		if (new_cv1 != r_bcprof->temp_mon_range[i].full_chrg_vol) {
			if (chc.pdata->cv_to_reg) {
				chc.pdata->cv_to_reg(new_cv1, &reg_val);
				ret = update_zone_cv(i, reg_val);
				if (unlikely(ret))
					return ret;
			}
			r_bcprof->temp_mon_range[i].full_chrg_vol = new_cv1;
		}
	}

	/* send the new CC and CV */
	intel_soc_pmic_update(chc.reg_map->pmic_chgrctrl1,
		CHGRCTRL1_FTEMP_EVENT_MASK, CHGRCTRL1_FTEMP_EVENT_MASK);

	return 0;
}

int intel_pmic_set_ilimma(int ilim_ma)
{
	u8 reg_val;
	int ret;

	if (ilim_ma >= 1500) {
		if (chc.pdata->inlmt_to_reg)
			chc.pdata->inlmt_to_reg(ilim_ma, &reg_val);

		ret = pmic_write_tt(TT_USBINPUTICC1500VAL_ADDR, reg_val);
		if (ret)
			return ret;
	}

	lookup_regval(pmic_inlmt, ARRAY_SIZE(pmic_inlmt),
			ilim_ma, &reg_val);
	dev_dbg(chc.dev, "Setting inlmt %d in register %x=%x\n", ilim_ma,
		chc.reg_map->pmic_chgrctrl1, reg_val);
	ret = pmic_write_reg(chc.reg_map->pmic_chgrctrl1, reg_val);

	return ret;
}

int intel_pmic_get_battery_pack_temp(int *temp)
{
	int ret, val;
	struct iio_channel *indio_chan;

	if (chc.invalid_batt)
		return -ENODEV;
	indio_chan = iio_channel_get(NULL, BATTEMP_CHANNEL);
	if (IS_ERR_OR_NULL(indio_chan))
		return PTR_ERR(indio_chan);

	ret = iio_read_channel_raw(indio_chan, &val);
	if (ret) {
		dev_err(chc.dev, "IIO channel read error\n");
		return ret;
	}

	iio_channel_release(indio_chan);

	return raw_to_temp(chc.pdata->adc_tbl, chc.pdata->max_tbl_row_cnt, val);
}

static int pmic_get_usbid(void)
{
	int ret;
	struct iio_channel *indio_chan;
	int rid, id = RID_UNKNOWN;
	u8 val;

	ret = pmic_read_reg(chc.reg_map->pmic_schgrirq1, &val);
	if (ret)
		return RID_UNKNOWN;

	/* SCHGRIRQ1_REG SUSBIDDET bit definition:
	 * 00 = RID_A/B/C ; 01 = RID_GND ; 10 = RID_FLOAT */
	if ((val & SCHRGRIRQ1_SUSBIDGNDDET_MASK) == SHRT_FLT_DET)
		return RID_FLOAT;
	else if ((val & SCHRGRIRQ1_SUSBIDGNDDET_MASK) == SHRT_GND_DET)
		return RID_GND;

	indio_chan = iio_channel_get(NULL, "USBID");
	if (IS_ERR_OR_NULL(indio_chan)) {
		dev_err(chc.dev, "Failed to get IIO channel USBID\n");
		return RID_UNKNOWN;
	}

	ret = iio_read_channel_raw(indio_chan, &rid);
	if (ret) {
		dev_err(chc.dev, "IIO channel read error for USBID\n");
		goto err_exit;
	}
	dev_dbg(chc.dev, "%s: rid=%d\n", __func__, rid);
	if (IS_RID_A(rid))
		id = RID_A;
	else if (IS_RID_B(rid))
		id = RID_B;
	else if (IS_RID_C(rid))
		id = RID_C;

err_exit:
	iio_channel_release(indio_chan);
	return id;
}

static int get_charger_type(void)
{
	int ret, i = 0;
	u8 val;
	int chgr_type, rid;

	do {
		ret = pmic_read_reg(chc.reg_map->pmic_usbsrcdetstat, &val);
		if (ret)
			return 0;
		i++;
		dev_dbg(chc.dev, "Read USBSRCDETSTATUS val: %x\n", val);

		if (val & USBSRCDET_SUSBHWDET_DETSUCC)
			break;
		else
			msleep(USBSRCDET_SLEEP_TIME);
	} while (i < USBSRCDET_RETRY_CNT);

	if (!(val & USBSRCDET_SUSBHWDET_DETSUCC)) {
		dev_err(chc.dev, "Charger detection unsuccessful after %dms\n",
			i * USBSRCDET_SLEEP_TIME);
		return 0;
	}

	chgr_type = (val & USBSRCDET_USBSRCRSLT_MASK) >> 2;
	dev_dbg(chc.dev, "Charger type after detection complete: %d\n",
			(val & USBSRCDET_USBSRCRSLT_MASK) >> 2);

	switch (chgr_type) {
	case PMIC_CHARGER_TYPE_SDP:
	case PMIC_CHARGER_TYPE_FLOAT_DP_DN:
		return POWER_SUPPLY_CHARGER_TYPE_USB_SDP;
	case PMIC_CHARGER_TYPE_DCP:
		return POWER_SUPPLY_CHARGER_TYPE_USB_DCP;
	case PMIC_CHARGER_TYPE_CDP:
		return POWER_SUPPLY_CHARGER_TYPE_USB_CDP;
	case PMIC_CHARGER_TYPE_ACA:
		rid = pmic_get_usbid();
		if (rid == RID_A)
			return POWER_SUPPLY_CHARGER_TYPE_ACA_DOCK;
		/* As PMIC detected the charger as ACA, if RID detection
		 * failed report type as ACA  */
		else
			return POWER_SUPPLY_CHARGER_TYPE_USB_ACA;
	case PMIC_CHARGER_TYPE_SE1:
		return POWER_SUPPLY_CHARGER_TYPE_SE1;
	case PMIC_CHARGER_TYPE_MHL:
		return POWER_SUPPLY_CHARGER_TYPE_MHL;
	default:
		return POWER_SUPPLY_CHARGER_TYPE_NONE;
	}
}

static void handle_internal_usbphy_notifications(int mask)
{
	struct power_supply_cable_props cap = {0};
	int evt = -1;

	if (mask) {
		cap.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_CONNECT;
		cap.chrg_type = get_charger_type();
		chc.charger_type = cap.chrg_type;
		/* Close D+/D- lines in USB detection switch
		 * due to WC PMIC bug
		 */
		pmic_write_reg(chc.reg_map->pmic_usbphyctrl, 0x1);
	} else {
		cap.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_DISCONNECT;
		cap.chrg_type = chc.charger_type;
		/* Open D+/D- lines in USB detection switch
		 * due to WC PMIC bug
		 */
		pmic_write_reg(chc.reg_map->pmic_usbphyctrl, 0x0);
	}

	switch (cap.chrg_type) {
	case POWER_SUPPLY_CHARGER_TYPE_USB_SDP:
		if (cap.chrg_evt == POWER_SUPPLY_CHARGER_EVENT_CONNECT)
			evt =  USB_EVENT_VBUS;
		else
			evt =  USB_EVENT_NONE;
		cap.ma = LOW_POWER_CHRG_CURRENT;
		break;
	case POWER_SUPPLY_CHARGER_TYPE_USB_CDP:
		if (cap.chrg_evt == POWER_SUPPLY_CHARGER_EVENT_CONNECT)
			evt =  USB_EVENT_VBUS;
		else
			evt =  USB_EVENT_NONE;
		cap.ma = HIGH_POWER_CHRG_CURRENT;
		break;
	case POWER_SUPPLY_CHARGER_TYPE_USB_DCP:
	case POWER_SUPPLY_CHARGER_TYPE_SE1:
	case POWER_SUPPLY_CHARGER_TYPE_USB_ACA:
		cap.ma = HIGH_POWER_CHRG_CURRENT;
		break;
	case POWER_SUPPLY_CHARGER_TYPE_ACA_DOCK:
	case POWER_SUPPLY_CHARGER_TYPE_ACA_A:
		cap.ma = HIGH_POWER_CHRG_CURRENT;
		if (cap.chrg_evt == POWER_SUPPLY_CHARGER_EVENT_CONNECT)
			evt = USB_EVENT_ID;
		else
			evt = USB_EVENT_NONE;
		break;
	case POWER_SUPPLY_CHARGER_TYPE_AC:
	case POWER_SUPPLY_CHARGER_TYPE_ACA_B:
	case POWER_SUPPLY_CHARGER_TYPE_ACA_C:
	case POWER_SUPPLY_CHARGER_TYPE_MHL:
	case POWER_SUPPLY_CHARGER_TYPE_B_DEVICE:
		cap.ma = HIGH_POWER_CHRG_CURRENT;
		break;
	case POWER_SUPPLY_CHARGER_TYPE_NONE:
	default:
		cap.ma = 0;
	}

	dev_dbg(chc.dev, "Notifying OTG ev:%d, evt:%d, chrg_type:%d, mA:%d\n",
			evt, cap.chrg_evt, cap.chrg_type,
			cap.ma);
	if (cap.chrg_evt == POWER_SUPPLY_CHARGER_EVENT_DISCONNECT)
		chc.charger_type = POWER_SUPPLY_CHARGER_TYPE_NONE;

	atomic_notifier_call_chain(&chc.otg->notifier,
				USB_EVENT_CHARGER, &cap);
	if (evt >= 0)
		atomic_notifier_call_chain(&chc.otg->notifier, evt, NULL);
}

static void handle_batttemp_interrupt(u16 int_reg, u16 stat_reg)
{
	u16 alert_mask;

	if (int_reg & BIT_POS(PMIC_INT_BZIRQ))
		pmic_bat_zone_changed();

	if (int_reg & (BIT_POS(PMIC_INT_BATCRIT) ||
				BIT_POS(PMIC_INT_BATCRIT_HOTCOLD))) {
		if (stat_reg & (BIT_POS(PMIC_INT_BATCRIT) ||
				BIT_POS(PMIC_INT_BATCRIT_HOTCOLD)))
			dev_err(chc.dev, "Battery Over Heat/Cold\n");
		else
			dev_err(chc.dev, "Battery Over Heat/Cold Recovered\n");
	}

	alert_mask = BIT_POS(PMIC_INT_BAT0ALRT0) |
			BIT_POS(PMIC_INT_BAT0ALRT3) |
			BIT_POS(PMIC_INT_BAT1ALRT0) |
			BIT_POS(PMIC_INT_BAT1ALRT3);

	if (int_reg & alert_mask) {
		if (stat_reg & alert_mask)
			dev_err(chc.dev, "Battery Alert  triggered\n");
		else
			dev_err(chc.dev, "Battery Alert  recovered\n");
	}
}

static void handle_pwrsrc_interrupt(u16 int_reg, u16 stat_reg)
{
	int mask;
	u16 id_mask;

	id_mask = BIT_POS(PMIC_INT_USBIDFLTDET) |
				 BIT_POS(PMIC_INT_USBIDGNDDET);

	mutex_lock(&pmic_lock);
	if (int_reg & id_mask) {
		mask = (stat_reg & id_mask) == SHRT_GND_DET;
		/* Close/Open D+/D- lines in USB detection switch
		 * due to WC PMIC bug
		 */
		if (mask) {
			pmic_write_reg(chc.reg_map->pmic_usbphyctrl, 0x1);
			if (chc.vbus_state == VBUS_ENABLE) {
				if (chc.otg->set_vbus)
					chc.otg->set_vbus(chc.otg, true);
				atomic_notifier_call_chain(&chc.otg->notifier,
						USB_EVENT_ID, &mask);
			}
		} else if ((int_reg & BIT_POS(PMIC_INT_USBIDFLTDET)) &&
				chc.otg_mode_enabled) {
			/* WA for OTG ID removal: PMIC interprets ID removal
			 * as ID_FLOAT. Check for ID float and otg_mode enabled
			 * to send ID disconnect.
			 * In order to avoid ctyp detection flow, disable otg
			 * mode during vbus turn off event
			 */
			if (chc.vbus_state == VBUS_ENABLE) {
				if (chc.otg->set_vbus)
					chc.otg->set_vbus(chc.otg, false);
				atomic_notifier_call_chain(&chc.otg->notifier,
						USB_EVENT_NONE, NULL);
			}
			pmic_write_reg(chc.reg_map->pmic_usbphyctrl, 0x0);

		}
	}

	if ((int_reg & BIT_POS(PMIC_INT_USBIDDET)) &&
			(chc.vbus_state == VBUS_ENABLE)) {
		mask = !!(stat_reg & BIT_POS(PMIC_INT_USBIDDET));
		if (chc.otg->set_vbus)
			chc.otg->set_vbus(chc.otg, true);
		atomic_notifier_call_chain(&chc.otg->notifier,
				USB_EVENT_ID, &mask);
	}
	mutex_unlock(&pmic_lock);

	if (int_reg & BIT_POS(PMIC_INT_VBUS)) {
		mask = !!(stat_reg & BIT_POS(PMIC_INT_VBUS));
		if (mask) {
			dev_info(chc.dev,
				"USB VBUS Detected. Notifying OTG driver\n");
			chc.otg_mode_enabled =
				(stat_reg & id_mask) == SHRT_GND_DET;
		} else {
			dev_info(chc.dev,
				"USB VBUS Removed. Notifying OTG driver\n");
		}

		/* Avoid charger-detection flow in case of host-mode */
		if (chc.is_internal_usb_phy && !chc.otg_mode_enabled)
			handle_internal_usbphy_notifications(mask);
		else if (!mask)
			chc.otg_mode_enabled =
					(stat_reg & id_mask) == SHRT_GND_DET;
		intel_pmic_handle_otgmode(chc.otg_mode_enabled);
	}
}

static void pmic_event_worker(struct work_struct *work)
{
	struct pmic_event *evt, *tmp;

	dev_dbg(chc.dev, "%s\n", __func__);

	mutex_lock(&chc.evt_queue_lock);
	list_for_each_entry_safe(evt, tmp, &chc.evt_queue, node) {
		list_del(&evt->node);

	dev_dbg(chc.dev, "%s pwrsrc=%X, spwrsrc=%x battirq=%x sbattirq=%x miscirq=%x smiscirq=%x wake thread\n",
			__func__, evt->pwrsrc_int,
			evt->pwrsrc_int_stat, evt->battemp_int,
			evt->battemp_int_stat, evt->misc_int,
			evt->misc_int_stat);

		if (evt->pwrsrc_int)
			handle_pwrsrc_interrupt(evt->pwrsrc_int,
						evt->pwrsrc_int_stat);
		if (evt->battemp_int)
			handle_batttemp_interrupt(evt->battemp_int,
						evt->battemp_int_stat);
		kfree(evt);
	}

	mutex_unlock(&chc.evt_queue_lock);
}

static irqreturn_t pmic_isr(int irq, void *data)
{
	return IRQ_WAKE_THREAD;
}
static irqreturn_t pmic_thread_handler(int id, void *data)
{
	int i, shift;
	u16 *pmic_int, *pmic_int_stat, off;
	u16 stat_reg = 0, int_reg = 0;
	u8 ireg_val = 0, sreg_val = 0, val;
	struct pmic_event *evt;

	evt = kzalloc(sizeof(struct pmic_event), GFP_KERNEL);
	if (!evt)
		return IRQ_NONE;

	pmic_int = &evt->pwrsrc_int;
	pmic_int_stat = &evt->pwrsrc_int_stat;

	for (i = 0; i < chc.intmap_size; ++i) {
		off = chc.intmap[i].pmic_int / 16;

		if (int_reg != chc.intmap[i].ireg) {
			pmic_read_reg(chc.intmap[i].ireg, &ireg_val);
			int_reg = chc.intmap[i].ireg;
		}
		val = ireg_val;
		dev_dbg(chc.dev, "%s:%d ireg=%x val = %x\n", __func__, __LINE__,
			chc.intmap[i].ireg, val);
		val &= chc.intmap[i].mask;

		shift = ffs(chc.intmap[i].mask) -
				ffs(BIT_POS(chc.intmap[i].pmic_int));
		if (shift < 0)
			val <<= abs(shift);
		else if (shift > 0)
			val >>= abs(shift);

		pmic_int[off] |= val;

		dev_dbg(chc.dev, "%s:%d ireg=%x\n", __func__, __LINE__,
				pmic_int[off]);

		if (stat_reg != chc.intmap[i].sreg) {
			pmic_read_reg(chc.intmap[i].sreg, &sreg_val);
			stat_reg = chc.intmap[i].sreg;
		}
		val = sreg_val;
		dev_dbg(chc.dev, "%s:%d sreg=%x\n",
				__func__, __LINE__, chc.intmap[i].sreg);
		val &= chc.intmap[i].mask;

		if (shift < 0)
			val <<= abs(shift);
		else if (shift > 0)
			val >>= abs(shift);

		pmic_int_stat[off] |= val;
		dev_dbg(chc.dev, "%s:%d stat=%x\n",
			__func__, __LINE__, pmic_int_stat[off]);
	}

	INIT_LIST_HEAD(&evt->node);
	mutex_lock(&chc.evt_queue_lock);
	list_add_tail(&evt->node, &chc.evt_queue);
	mutex_unlock(&chc.evt_queue_lock);

	dev_dbg(chc.dev, "%s pwrsrc=%X, spwrsrc=%x battirq=%x sbattirq=%x miscirq=%x smiscirq=%x wake thread\n",
			__func__, evt->pwrsrc_int,
			evt->pwrsrc_int_stat, evt->battemp_int,
			evt->battemp_int_stat, evt->misc_int,
			evt->misc_int_stat);

	schedule_work(&chc.evt_work);
	return IRQ_HANDLED;
}

/**
 * get_tempzone_val - get tempzone register val for a particular zone
 * @adc_val: adc_value passed for zone temp
 * @temp: zone temperature
 *
 * Returns temp zone alert value
 */
static u16 get_tempzone_val(u32 resi_val, int temp)
{
	u8 cursel = 0, hys = 0;
	u16 trsh = 0, count = 0;
	u32 adc_thold = 0, bsr_num = 0;
	u32 tempzone_val = 0;
	s16 hyst = 0;
	int retval;

	/* CUR = max(floor(log2(round(ADCNORM/2^5)))-7,0)
	 * TRSH = round(ADCNORM/(2^(4+CUR)))
	 * HYS = if(∂ADCNORM>0 then max(round(∂ADCNORM/(2^(7+CUR))),1)
	 * else 0
	 */

	/*
	 * while calculating the CUR[2:0], instead of log2
	 * do a BSR (bit scan reverse) since we are dealing with integer values
	 */
	bsr_num = resi_val;
	bsr_num /= (1 << 5);

	while (bsr_num >>= 1)
		count++;

	/* cursel = max((count - 7), 0);
	 * Clamp cursel to 3-bit value
	 */
	cursel = clamp_t(s8, (count-7), 0, 7);

	/* calculate the TRSH[8:0] to be programmed */
	trsh = ((resi_val) / (1 << (4 + cursel)));

	/* calculate HYS[3:0] */
	/* add the temp hysteresis depending upon the zones */
	if (temp <= 0 || temp >= 60)
		temp += 1;
	else
		temp += 2;

	/* retrieve the resistance corresponding to temp with hysteresis */
	adc_thold = temp_to_raw(chc.pdata->adc_tbl,
			chc.pdata->max_tbl_row_cnt, temp);
	if (adc_thold == -ENOENT) {
		dev_err(chc.dev,
			"No ADC look up entry for temp:%d\n", temp);
		return adc_thold;
	}

	hyst = (resi_val - adc_thold);

	if (hyst > 0)
		hys = max((hyst / (1 << (7 + cursel))), 1);
	else
		hys = 0;
	/* Clamp hys to 4-bit value */
	hys = clamp_t(u8, hys, 0, 15);
	tempzone_val = (hys << 12) | (cursel << 9) | trsh;
	return tempzone_val;
}

static int pmic_ccsm_pse_prof_init(void)
{
	int ret = 0, i, temp_mon_ranges;
	u32 adc_val;
	u8 reg_val;
	int max_hw_zones = 5;
	struct ps_pse_mod_prof *bcprof = chc.actual_bcprof;
	temp_mon_ranges = min_t(u16, bcprof->temp_mon_ranges,
			BATT_TEMP_NR_RNG);
	if (temp_mon_ranges > max_hw_zones)
		temp_mon_ranges = max_hw_zones;

	for (i = 0; i < temp_mon_ranges; ++i) {

		/* Configure battery temperature zone */
		adc_val = temp_to_raw(chc.pdata->adc_tbl,
				chc.pdata->max_tbl_row_cnt,
				bcprof->temp_mon_range[i].temp_up_lim);
		if (adc_val == -ENOENT) {
			dev_err(chc.dev,
				"No ADC look up entry for temp:%d\n",
					bcprof->temp_mon_range[i].temp_up_lim);
			return adc_val;
		}

		if (chc.pmic_model != INTEL_PMIC_BCOVE) {
			/* Values obtained from lookup-table are resistance
			 * values. Convert these to raw adc-codes.
			 */
			adc_val = get_tempzone_val(adc_val,
					bcprof->temp_mon_range[i].temp_up_lim);
			dev_dbg(chc.dev,
					"adc-val:%x configured for temp:%d\n",
					adc_val,
					bcprof->temp_mon_range[i].temp_up_lim);
		}

		ret = update_zone_temp(i, adc_val);
		if (unlikely(ret)) {
			dev_err(chc.dev,
				"Error updating zone temp for zone %d\n", i);
			return ret;
		}

		/* Configure CC for zone */
		if (chc.pdata->cc_to_reg)
			chc.pdata->cc_to_reg(bcprof->temp_mon_range[i].
					full_chrg_cur, &reg_val);

		ret = update_zone_cc(i, reg_val);
		if (unlikely(ret))
			return ret;

		/* Configure CV for zone */
		if (chc.pdata->cv_to_reg)
			chc.pdata->cv_to_reg(bcprof->temp_mon_range[i].
					full_chrg_vol, &reg_val);

		ret = update_zone_cv(i, reg_val);
		if (unlikely(ret))
			return ret;

		/* Write lowest temp limit */
		if (i == (bcprof->temp_mon_ranges - 1)) {
			adc_val = temp_to_raw(chc.pdata->adc_tbl,
					chc.pdata->max_tbl_row_cnt,
					bcprof->temp_low_lim);

			if (adc_val == -ENOENT) {
				dev_err(chc.dev,
					"No ADC look up entry for temp:%d\n",
						bcprof->temp_low_lim);
				return adc_val;
			}

			if (chc.pmic_model != INTEL_PMIC_BCOVE) {
				adc_val = get_tempzone_val(adc_val,
						bcprof->temp_low_lim);
				dev_dbg(chc.dev,
					"adc-val:%x configured for temp:%d\n",
					adc_val, bcprof->temp_low_lim);
			}

			ret = update_zone_temp(i+1, adc_val);
			if (unlikely(ret)) {
				dev_err(chc.dev, "Error updating last temp for zone %d\n",
					i+1);
				return ret;
			}
		}
	}

	ret = pmic_update_tt(TT_CUSTOMFIELDEN_ADDR,
				TT_HOT_COLD_LC_MASK,
				TT_HOT_COLD_LC_DIS);

	if (unlikely(ret)) {
		dev_err(chc.dev, "Error updating TT_CUSTOMFIELD_EN reg\n");
		return ret;
	}

	if (chc.pdata->inlmt_to_reg)
		chc.pdata->inlmt_to_reg(USBINPUTICC100VAL, &reg_val);

	ret = pmic_write_tt(TT_USBINPUTICC100VAL_ADDR, reg_val);
	return ret;
}

static inline void print_ps_pse_mod_prof(struct ps_pse_mod_prof *bcprof)
{
	int i, temp_mon_ranges;

	dev_dbg(chc.dev, "ChrgProf: batt_id:%s\n", bcprof->batt_id);
	dev_dbg(chc.dev, "ChrgProf: battery_type:%u\n", bcprof->battery_type);
	dev_dbg(chc.dev, "ChrgProf: capacity:%u\n", bcprof->capacity);
	dev_dbg(chc.dev, "ChrgProf: voltage_max:%u\n", bcprof->voltage_max);
	dev_dbg(chc.dev, "ChrgProf: chrg_term_ma:%u\n", bcprof->chrg_term_ma);
	dev_dbg(chc.dev, "ChrgProf: low_batt_mV:%u\n", bcprof->low_batt_mV);
	dev_dbg(chc.dev, "ChrgProf: disch_tmp_ul:%d\n", bcprof->disch_tmp_ul);
	dev_dbg(chc.dev, "ChrgProf: disch_tmp_ll:%d\n", bcprof->disch_tmp_ll);
	dev_dbg(chc.dev, "ChrgProf: temp_mon_ranges:%u\n",
			bcprof->temp_mon_ranges);
	temp_mon_ranges = min_t(u16, bcprof->temp_mon_ranges,
			BATT_TEMP_NR_RNG);

	for (i = 0; i < temp_mon_ranges; ++i) {
		dev_dbg(chc.dev, "ChrgProf: temp_up_lim[%d]:%d\n",
				i, bcprof->temp_mon_range[i].temp_up_lim);
		dev_dbg(chc.dev, "ChrgProf: full_chrg_vol[%d]:%d\n",
				i, bcprof->temp_mon_range[i].full_chrg_vol);
		dev_dbg(chc.dev, "ChrgProf: full_chrg_cur[%d]:%d\n",
				i, bcprof->temp_mon_range[i].full_chrg_cur);
		dev_dbg(chc.dev, "ChrgProf: maint_chrgr_vol_ll[%d]:%d\n",
				i, bcprof->temp_mon_range[i].maint_chrg_vol_ll);
		dev_dbg(chc.dev, "ChrgProf: maint_chrgr_vol_ul[%d]:%d\n",
				i, bcprof->temp_mon_range[i].maint_chrg_vol_ul);
		dev_dbg(chc.dev, "ChrgProf: maint_chrg_cur[%d]:%d\n",
				i, bcprof->temp_mon_range[i].maint_chrg_cur);
	}
	dev_dbg(chc.dev, "ChrgProf: temp_low_lim:%d\n", bcprof->temp_low_lim);
}

static int find_tempzone_index(short int *interval,
				int *num_zones,
				short int *temp_up_lim)
{
	struct ps_pse_mod_prof *bprof = chc.bcprof->batt_prof;
	int up_lim_index = 0, low_lim_index = -1;
	int diff = 0;
	int i;

	*num_zones = MIN_BATT_PROF - bprof->temp_mon_ranges + 1;
	if ((*num_zones) <= 0)
		return 0;

	for (i = 0; i < bprof->temp_mon_ranges; i++) {
		if (bprof->temp_mon_range[i].temp_up_lim == BATT_TEMP_WARM)
			up_lim_index = i;
	}

	low_lim_index = up_lim_index + 1;

	if (low_lim_index == bprof->temp_mon_ranges)
		diff = bprof->temp_low_lim -
			bprof->temp_mon_range[up_lim_index].temp_up_lim;
	else
		diff = bprof->temp_mon_range[low_lim_index].temp_up_lim -
			bprof->temp_mon_range[up_lim_index].temp_up_lim;

	*interval = diff / (*num_zones);
	*temp_up_lim = bprof->temp_mon_range[up_lim_index].temp_up_lim;

	return up_lim_index;
}

static void set_pmic_batt_prof(struct ps_pse_mod_prof *new_prof,
				struct ps_pse_mod_prof *bprof)
{
	int num_zones;
	int split_index;
	int i, j = 0;
	short int temp_up_lim = 0;
	short int interval = 0;

	if ((new_prof == NULL) || (bprof == NULL))
		return;

	if (!NEED_ZONE_SPLIT(bprof)) {
		dev_info(chc.dev, "No need to split the zones!!\n");
		memcpy(new_prof, bprof, sizeof(struct ps_pse_mod_prof));
		return;
	}

	strcpy(&(new_prof->batt_id[0]), &(bprof->batt_id[0]));
	new_prof->battery_type = bprof->battery_type;
	new_prof->capacity = bprof->capacity;
	new_prof->voltage_max =  bprof->voltage_max;
	new_prof->chrg_term_ma = bprof->chrg_term_ma;
	new_prof->low_batt_mV =  bprof->low_batt_mV;
	new_prof->disch_tmp_ul = bprof->disch_tmp_ul;
	new_prof->disch_tmp_ll = bprof->disch_tmp_ll;

	split_index = find_tempzone_index(&interval, &num_zones, &temp_up_lim);

	for (i = 0; i < bprof->temp_mon_ranges; i++) {
		if ((i == split_index) && (num_zones > 0)) {
			for (j = 0; j < num_zones; j++,
					temp_up_lim += interval) {
				memcpy(&new_prof->temp_mon_range[i+j],
					&bprof->temp_mon_range[i],
					sizeof(bprof->temp_mon_range[i]));
				new_prof->temp_mon_range[i+j].temp_up_lim =
					temp_up_lim;
			}
			j--;
		} else {
			memcpy(&new_prof->temp_mon_range[i+j],
				&bprof->temp_mon_range[i],
				sizeof(bprof->temp_mon_range[i]));
		}
	}

	new_prof->temp_mon_ranges = i+j;
	new_prof->temp_low_lim = bprof->temp_low_lim;

	return;
}

static int pmic_check_initial_events(void)
{
	int ret = 0, i, shift;
	struct pmic_event *evt;
	u8 val, sreg_val = 0;
	u16 *pmic_int, *pmic_int_stat, off;
	u16 stat_reg = 0;

	evt = kzalloc(sizeof(struct pmic_event), GFP_KERNEL);
	if (!evt)
		return -ENOMEM;

	pmic_int = &evt->pwrsrc_int;
	pmic_int_stat = &evt->pwrsrc_int_stat;

	for (i = 0; i < chc.intmap_size; ++i) {
		off = chc.intmap[i].pmic_int / 16;

		if (stat_reg != chc.intmap[i].sreg) {
			pmic_read_reg(chc.intmap[i].sreg, &sreg_val);
			stat_reg = chc.intmap[i].sreg;
		}

		val = sreg_val;
		dev_dbg(chc.dev, "%s:%d reg=%x val = %x\n", __func__, __LINE__,
					chc.intmap[i].sreg, val);
		val &= chc.intmap[i].mask;
		dev_dbg(chc.dev, "%s:%d reg=%x val = %x\n", __func__, __LINE__,
					chc.intmap[i].sreg, val);

		shift = ffs(chc.intmap[i].mask) -
				ffs(BIT_POS(chc.intmap[i].pmic_int));
		if (shift < 0)
			val <<= abs(shift);
		else if (shift > 0)
			val >>= abs(shift);
		pmic_int[off] |= val;
		pmic_int_stat[off] |= val;
	}

	INIT_LIST_HEAD(&evt->node);
	mutex_lock(&chc.evt_queue_lock);
	list_add_tail(&evt->node, &chc.evt_queue);
	mutex_unlock(&chc.evt_queue_lock);
	schedule_work(&chc.evt_work);

	pmic_bat_zone_changed();

	return ret;
}

static int get_pmic_model(const char *name)
{
	if (!strncmp(name, "wcove_ccsm", strlen("wcove_ccsm")))
		return INTEL_PMIC_WCOVE;
	else if (!strncmp(name, "scove_ccsm", strlen("scove_ccsm")))
		return INTEL_PMIC_SCOVE;
	else if (!strncmp(name, "bcove_ccsm", strlen("bcove_ccsm")))
		return INTEL_PMIC_BCOVE;

	return INTEL_PMIC_UNKNOWN;
}

/* vbus control cooling device callbacks */
static int vbus_get_max_state(struct thermal_cooling_device *tcd,
				unsigned long *state)
{
	*state = MAX_VBUSCTRL_STATES;
	return 0;
}

static int vbus_get_cur_state(struct thermal_cooling_device *tcd,
				unsigned long *state)
{
	mutex_lock(&pmic_lock);
	*state = chc.vbus_state;
	mutex_unlock(&pmic_lock);

	return 0;
}

static int vbus_set_cur_state(struct thermal_cooling_device *tcd,
				unsigned long new_state)
{
	int ret;

	if (new_state >= MAX_VBUSCTRL_STATES || new_state < 0) {
		dev_err(chc.dev, "Invalid vbus control state: %ld\n",
				new_state);
		return -EINVAL;
	}

	/**
	 * notify directly only when the ID_GND and want to change the state
	 * from previous state (vbus enable/disable).
	 */
	mutex_lock(&pmic_lock);
	if ((pmic_get_usbid() == RID_GND) && (chc.vbus_state != new_state)) {
		if (!new_state) {
			if (chc.otg->set_vbus)
				chc.otg->set_vbus(chc.otg, true);
			atomic_notifier_call_chain(&chc.otg->notifier,
						USB_EVENT_ID,
						NULL);
		} else {
			if (chc.otg->set_vbus)
				chc.otg->set_vbus(chc.otg, false);
			atomic_notifier_call_chain(&chc.otg->notifier,
						USB_EVENT_NONE,
						NULL);
		}
	}

	chc.vbus_state = new_state;
	mutex_unlock(&pmic_lock);

	return ret;
}

static struct thermal_cooling_device_ops psy_vbuscd_ops = {
	.get_max_state = vbus_get_max_state,
	.get_cur_state = vbus_get_cur_state,
	.set_cur_state = vbus_set_cur_state,
};

static inline int register_cooling_device(struct pmic_chrgr_drv_context *chc)
{
	struct power_supply *psy_bat = get_psy_battery();
	chc->vbus_cdev = thermal_cooling_device_register(
				(char *)VBUS_CTRL_CDEV_NAME,
				psy_bat,
				&psy_vbuscd_ops);
	if (IS_ERR(chc->vbus_cdev))
		return PTR_ERR(chc->vbus_cdev);

	dev_dbg(chc->dev, "cooling device register success for %s\n",
				VBUS_CTRL_CDEV_NAME);
	return 0;
}


/**
 * pmic_charger_probe - PMIC charger probe function
 * @pdev: pmic platform device structure
 * Context: can sleep
 *
 * pmic charger driver initializes its internal data
 * structure and other  infrastructure components for it
 * to work as expected.
 */
static int pmic_chrgr_probe(struct platform_device *pdev)
{
	int ret = 0, i = 0, irq;
	u8 val;

	if (!pdev)
		return -ENODEV;

	chc.batt_health = POWER_SUPPLY_HEALTH_UNKNOWN;
	chc.dev = &pdev->dev;

	while ((irq = platform_get_irq(pdev, i)) != -ENXIO)
		chc.irq[i++] = irq;

	chc.irq_cnt = i;
	chc.pdata = pdev->dev.platform_data;
	if (!chc.pdata) {
		dev_err(&pdev->dev, "Platform data not initialized\n");
		return -EFAULT;
	}

	platform_set_drvdata(pdev, &chc);
	chc.reg_map = chc.pdata->reg_map;
	chc.reg_cnt = sizeof(struct pmic_regs) / sizeof(u16);
	chc.intmap = chc.pdata->intmap;
	chc.intmap_size = chc.pdata->intmap_size;
	chc.vbus_state = VBUS_ENABLE;

	chc.pmic_model = get_pmic_model(pdev->name);
	dev_info(chc.dev, "PMIC model is %d\n", chc.pmic_model);

	if (chc.pmic_model == INTEL_PMIC_UNKNOWN)
		return -EINVAL;

	if (INTERNAL_PHY_SUPPORTED(chc.pmic_model)) {
		ret = pmic_read_reg(chc.reg_map->pmic_usbpath, &val);

		if (!ret && (val & USBPATH_USBSEL_MASK)) {
				dev_info(chc.dev, "SOC-Internal-USBPHY used\n");
				chc.is_internal_usb_phy = true;
		} else {
				dev_info(chc.dev, "External-USBPHY used\n");
		}
	}

	chc.bcprof = kzalloc(sizeof(struct ps_batt_chg_prof), GFP_KERNEL);
	if (!chc.bcprof)
		return -ENOMEM;

	ret = get_batt_prop(chc.bcprof);
	if (ret) {
		dev_err(chc.dev,
			"Error reading battery profile from battid frmwrk\n");
		kfree(chc.bcprof);
		chc.invalid_batt = true;
		chc.bcprof = NULL;
	}

	if (intel_soc_pmic_update(chc.reg_map->pmic_chgrctrl0,
			SWCONTROL_ENABLE|CHGRCTRL0_CCSM_OFF_MASK,
			CHGRCTRL0_SWCONTROL_MASK|CHGRCTRL0_CCSM_OFF_MASK))
		dev_err(chc.dev, "Error enabling sw control. Charging may continue in h/w control mode\n");

	if (!chc.invalid_batt) {
		chc.actual_bcprof = kzalloc(sizeof(struct ps_pse_mod_prof),
					GFP_KERNEL);
		if (!chc.actual_bcprof) {
			ret = -ENOMEM;
			goto kzalloc_fail;
		}

		chc.runtime_bcprof = kzalloc(sizeof(struct ps_pse_mod_prof),
					GFP_KERNEL);
		if (!chc.runtime_bcprof) {
			ret = -ENOMEM;
			goto kzalloc_fail;
		}

		set_pmic_batt_prof(chc.actual_bcprof, chc.bcprof->batt_prof);
		print_ps_pse_mod_prof(chc.actual_bcprof);
		if (pmic_ccsm_pse_prof_init())
			dev_err(chc.dev, "Error in Initializing CCSM. Continue in h/w charging mode\n");

		*chc.runtime_bcprof = *chc.actual_bcprof;

	}
	chc.otg = usb_get_phy(USB_PHY_TYPE_USB2);
	if (!chc.otg || IS_ERR(chc.otg)) {
		dev_err(&pdev->dev, "Failed to get otg transceiver!!\n");
		ret = -EINVAL;
		goto otg_req_failed;
	}

	INIT_WORK(&chc.evt_work, pmic_event_worker);
	INIT_LIST_HEAD(&chc.evt_queue);
	mutex_init(&chc.evt_queue_lock);

	/* register interrupt */
	for (i = 0; i < chc.irq_cnt; ++i) {
		ret = request_threaded_irq(chc.irq[i], pmic_isr,
				pmic_thread_handler,
				IRQF_ONESHOT|IRQF_NO_SUSPEND,
				DRIVER_NAME, &chc);
		if (ret) {
			dev_err(&pdev->dev, "Error in request_threaded_irq(irq(%d)!!\n",
				chc.irq[i]);
			while (i)
				free_irq(chc.irq[--i], &chc);
			goto otg_req_failed;
		}
	}

	intel_soc_pmic_writeb(chc.reg_map->pmic_mthrmirq1,
						~MTHRMIRQ1_CCSM_MASK);

	ret = pmic_check_initial_events();
	if (ret)
		goto otg_req_failed;

	chc.batt_health = POWER_SUPPLY_HEALTH_GOOD;
#ifdef CONFIG_DEBUG_FS
	pmic_debugfs_init();
#endif

	/* Register to cooling device to control the vbus */
	ret = register_cooling_device(&chc);
	if (ret) {
		dev_err(&pdev->dev,
			"Register cooling device Failed (%d)\n", ret);
		goto cdev_reg_fail;
	}

	return 0;

cdev_reg_fail:
otg_req_failed:
kzalloc_fail:
	kfree(chc.bcprof);
	kfree(chc.actual_bcprof);
	kfree(chc.runtime_bcprof);
	return ret;
}

static void pmic_chrgr_do_exit_ops(struct pmic_chrgr_drv_context *chc)
{
#ifdef CONFIG_DEBUG_FS
	pmic_debugfs_exit();
#endif
}

/**
 * pmic_charger_remove - PMIC Charger driver remove
 * @pdev: PMIC charger platform device structure
 * Context: can sleep
 *
 * PMIC charger finalizes its internal data structure and other
 * infrastructure components that it initialized in
 * pmic_chrgr_probe.
 */
static int pmic_chrgr_remove(struct platform_device *pdev)
{
	int i, ret = 0;
	struct pmic_chrgr_drv_context *chc = platform_get_drvdata(pdev);

	if (chc) {
		if (IS_ERR_OR_NULL(chc->vbus_cdev))
			ret = PTR_ERR(chc->vbus_cdev);
		else
			thermal_cooling_device_unregister(chc->vbus_cdev);

		pmic_chrgr_do_exit_ops(chc);
		for (i = 0; i < chc->irq_cnt; ++i)
			free_irq(chc->irq[i], &chc);
		kfree(chc->bcprof);
		kfree(chc->actual_bcprof);
		kfree(chc->runtime_bcprof);
	}

	return ret;
}

/*********************************************************************
 *		Driver initialisation and finalization
 *********************************************************************/

static struct platform_device_id pmic_ccsm_device_ids[] = {
	{"bcove_ccsm", 0},
	{"scove_ccsm", 1},
	{"wcove_ccsm", 2},
	{},
};

static struct platform_driver intel_pmic_ccsm_driver = {
	.driver = {
		   .name = DRIVER_NAME,
		   .owner = THIS_MODULE,
		   },
	.probe = pmic_chrgr_probe,
	.remove = pmic_chrgr_remove,
	.id_table = pmic_ccsm_device_ids,
};


static int __init pmic_ccsm_init(void)
{
	return platform_driver_register(&intel_pmic_ccsm_driver);
}

static void __exit pmic_ccsm_exit(void)
{
	platform_driver_unregister(&intel_pmic_ccsm_driver);
}

late_initcall(pmic_ccsm_init);
module_exit(pmic_ccsm_exit);


MODULE_AUTHOR("Jenny TC <jenny.tc@intel.com>");
MODULE_DESCRIPTION("Intel PMIC CCSM Driver");
MODULE_LICENSE("GPL");
