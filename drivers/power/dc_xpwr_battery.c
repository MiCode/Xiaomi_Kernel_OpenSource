/*
 * dc_xpwr_battery.c - Dollar Cove(X-power) Battery driver
 *
 * Copyright (C) 2014 Intel Corporation
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
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Ramakrishna Pallala <ramakrishna.pallala@intel.com>
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/power_supply.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <asm/unaligned.h>
#include <linux/notifier.h>
#include <linux/acpi.h>
#include <linux/iio/consumer.h>
#include <linux/mfd/intel_soc_pmic.h>
#include <linux/power/dc_xpwr_battery.h>
#include <linux/power/battery_id.h>
#include <linux/efi.h>

#define DC_PS_STAT_REG			0x00
#define PS_STAT_VBUS_TRIGGER		(1 << 0)
#define PS_STAT_BAT_CHRG_DIR		(1 << 2)
#define PS_STAT_VBUS_ABOVE_VHOLD	(1 << 3)
#define PS_STAT_VBUS_VALID		(1 << 4)
#define PS_STAT_VBUS_PRESENT		(1 << 5)

#define DC_CHRG_STAT_REG		0x01
#define CHRG_STAT_BAT_SAFE_MODE		(1 << 3)
#define CHRG_STAT_BAT_VALID		(1 << 4)
#define CHRG_STAT_BAT_PRESENT		(1 << 5)
#define CHRG_STAT_CHARGING		(1 << 6)
#define CHRG_STAT_PMIC_OTP		(1 << 7)

#define DC_PWR_BAT_LED_CNTL_REG		0x32
#define CHRG_BATT_DET_MASK		(1 << 6)

#define DC_CHRG_CCCV_REG		0x33
#define CHRG_CCCV_CC_MASK		0xf		/* 4 bits */
#define CHRG_CCCV_CC_BIT_POS		0
#define CHRG_CCCV_CC_OFFSET		200		/* 200mA */
#define CHRG_CCCV_CC_LSB_RES		200		/* 200mA */
#define CHRG_CCCV_ITERM_20P		(1 << 4)	/* 20% of CC */
#define CHRG_CCCV_CV_MASK		0x60		/* 2 bits */
#define CHRG_CCCV_CV_BIT_POS		5
#define CHRG_CCCV_CV_4100MV		0x0		/* 4.10V */
#define CHRG_CCCV_CV_4150MV		0x1		/* 4.15V */
#define CHRG_CCCV_CV_4200MV		0x2		/* 4.20V */
#define CHRG_CCCV_CV_4350MV		0x3		/* 4.35V */
#define CHRG_CCCV_CHG_EN		(1 << 7)

#define CV_4100				4100	/* 4100mV */
#define CV_4150				4150	/* 4150mV */
#define CV_4200				4200	/* 4200mV */
#define CV_4350				4350	/* 4350mV */

#define DC_FG_VLTFW_REG			0x3C
#define FG_VLTFW_0C			0xA5	/* 0 DegC */
#define DC_FG_VHTFW_REG			0x3D
#define FG_VHTFW_56C			0x15	/* 56 DegC */

#define DC_TEMP_IRQ_CFG_REG		0x42
#define TEMP_IRQ_CFG_QWBTU		(1 << 0)
#define TEMP_IRQ_CFG_WBTU		(1 << 1)
#define TEMP_IRQ_CFG_QWBTO		(1 << 2)
#define TEMP_IRQ_CFG_WBTO		(1 << 3)
#define TEMP_IRQ_CFG_MASK		0xf

#define DC_FG_IRQ_CFG_REG		0x43
#define FG_IRQ_CFG_LOWBATT_WL2		(1 << 0)
#define FG_IRQ_CFG_LOWBATT_WL1		(1 << 1)
#define FG_IRQ_CFG_LOWBATT_MASK		0x3

#define DC_LOWBAT_IRQ_STAT_REG		0x4B
#define LOWBAT_IRQ_STAT_LOWBATT_WL2	(1 << 0)
#define LOWBAT_IRQ_STAT_LOWBATT_WL1	(1 << 1)

#define DC_FG_CNTL_REG			0xB8
#define FG_CNTL_OCV_ADJ_STAT		(1 << 2)
#define FG_CNTL_OCV_ADJ_EN		(1 << 3)
#define FG_CNTL_CAP_ADJ_STAT		(1 << 4)
#define FG_CNTL_CAP_ADJ_EN		(1 << 5)
#define FG_CNTL_CC_EN			(1 << 6)
#define FG_CNTL_GAUGE_EN		(1 << 7)

#define DC_FG_REP_CAP_REG		0xB9
#define FG_REP_CAP_VALID		(1 << 7)
#define FG_REP_CAP_VAL_MASK		0x7F

#define DC_FG_RDC1_REG			0xBA
#define DC_FG_RDC0_REG			0xBB

#define DC_FG_OCVH_REG			0xBC
#define DC_FG_OCVL_REG			0xBD

#define DC_FG_OCV_CURVE_REG		0xC0

#define DC_FG_DES_CAP1_REG		0xE0
#define DC_FG_DES_CAP1_VALID		(1 << 7)
#define DC_FG_DES_CAP1_VAL_MASK		0x7F

#define DC_FG_DES_CAP0_REG		0xE1
#define FG_DES_CAP0_VAL_MASK		0xFF
#define FG_DES_CAP_RES_LSB		1456	/* 1.456mAhr */

#define DC_FG_CC_MTR1_REG		0xE2
#define FG_CC_MTR1_VALID		(1 << 7)
#define FG_CC_MTR1_VAL_MASK		0x7F

#define DC_FG_CC_MTR0_REG		0xE3
#define FG_CC_MTR0_VAL_MASK		0xFF
#define FG_DES_CC_RES_LSB		1456	/* 1.456mAhr */

#define DC_FG_OCV_CAP_REG		0xE4
#define FG_OCV_CAP_VALID		(1 << 7)
#define FG_OCV_CAP_VAL_MASK		0x7F

#define DC_FG_CC_CAP_REG		0xE5
#define FG_CC_CAP_VALID			(1 << 7)
#define FG_CC_CAP_VAL_MASK		0x7F

#define DC_FG_LOW_CAP_REG		0xE6
#define FG_LOW_CAP_THR1_MASK		0xf0	/* 5% tp 20% */
#define FG_LOW_CAP_THR1_VAL		0xa0	/* 15 perc */
#define FG_LOW_CAP_THR2_MASK		0x0f	/* 0% to 15% */
#define FG_LOW_CAP_WARN_THR		14	/* 14 perc */
#define FG_LOW_CAP_CRIT_THR		4	/* 4 perc */
#define FG_LOW_CAP_SHDN_THR		0	/* 0 perc */

#define DC_FG_TUNING_CNTL0		0xE8
#define DC_FG_TUNING_CNTL1		0xE9
#define DC_FG_TUNING_CNTL2		0xEA
#define DC_FG_TUNING_CNTL3		0xEB
#define DC_FG_TUNING_CNTL4		0xEC
#define DC_FG_TUNING_CNTL5		0xED

/* Set 3.7V as minimum RDC voltage */
#define CNTL4_RDC_MIN_VOLT_SET_MASK	(1 << 4)
#define CNTL4_RDC_MIN_VOLT_RESET_MASK	(1 << 3)

/* each LSB is equal to 1.1mV */
#define ADC_TO_VBATT(a)			((a * 11) / 10)

/* each LSB is equal to 1mA */
#define ADC_TO_BATCUR(a)		(a)

/* each LSB is equal to 1mA */
#define ADC_TO_PMICTEMP(a)		(a - 267)

#define STATUS_MON_DELAY_JIFFIES	(HZ * 60)	/*60 sec */
#define STATUS_MON_FULL_DELAY_JIFFIES	(HZ * 30)	/*30sec */
#define FULL_CAP_THLD			98	/* 98% capacity */
#define BATT_DET_CAP_THLD		95	/* 95% capacity */
#define DC_FG_INTR_NUM			6

#define THERM_CURVE_MAX_SAMPLES		18
#define THERM_CURVE_MAX_VALUES		4
#define XPWR_VBAT_VMAX_OFFSET		50

/* No of times we should retry on -EAGAIN error */
#define NR_RETRY_CNT	3

#define DEV_NAME			"dollar_cove_battery"

enum {
	QWBTU_IRQ = 0,
	WBTU_IRQ,
	QWBTO_IRQ,
	WBTO_IRQ,
	WL2_IRQ,
	WL1_IRQ,
};

struct pmic_fg_info {
	struct platform_device *pdev;
	struct dollarcove_fg_pdata *pdata;
	int			irq[DC_FG_INTR_NUM];
	struct power_supply	bat;
	struct mutex		lock;

	int			status;
	int			btemp;
	/* Worker to monitor status and faults */
	struct delayed_work status_monitor;
};

static enum power_supply_property pmic_fg_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_MODEL_NAME,
};

/*
 * This array represents the Battery Pack thermistor
 * temperature and corresponding ADC value limits
 */
static int const therm_curve_data[THERM_CURVE_MAX_SAMPLES]
	[THERM_CURVE_MAX_VALUES] = {
	/* {temp_max, temp_min, adc_max, adc_min} */
	{-15, -20, 682, 536},
	{-10, -15, 536, 425},
	{-5, -10, 425, 338},
	{0, -5, 338, 272},
	{5, 0, 272, 220},
	{10, 5, 220, 179},
	{15, 10, 179, 146},
	{20, 15, 146, 120},
	{25, 20, 120, 100},
	{30, 25, 100, 83},
	{35, 30, 83, 69},
	{40, 35, 69, 58},
	{45, 40, 58, 49},
	{50, 45, 49, 41},
	{55, 50, 41, 35},
	{60, 55, 35, 30},
	{65, 60, 30, 25},
	{70, 65, 25, 22},
};

static unsigned short get_cksum(u8 *data, int size)
{
	int i;
	u16 cksum = 0;

	for (i = 0; i < size; i++)
		cksum += data[i];
	cksum = (0x10000 - cksum);
	return cksum;
}

#ifdef CONFIG_ACPI

/* XPWR_FG_GUID will be given by BIOS */
#define XPWR_FG_GUID EFI_GUID(0x17c04bed, 0x9577, 0x4952, 0x89, 0x44,\
				0x59, 0x64, 0xa4, 0x7c, 0x6d, 0xc5)
#define EFI_VAR_MAX_DATA 256
#define EFI_FG_VAR_NAME "FuelGaugeVar"
#define EFI_FG_VAR_NAME_SIZE 64

/* structure of efi variable to save secondary fg configuration data. */
struct xpwr_fg_sec_config_efi_data {
	char  fg_name[ACPI_FG_CONF_NAME_LEN];
	u16  size;
	u16  sec_cksum;	/* Checksum for variable config data */
	u16  prim_cksum; /* Primary config data checksum */
	/*
	 * Use the maximum fuel gauge config data size.
	 * XPOWER      - 36  Bytes
	 */
	u8   config_data[EFI_VAR_MAX_DATA];
} __packed;

static void
dc_xpwr_dump_efi_var_data(struct xpwr_fg_sec_config_efi_data *vdata,
		struct pmic_fg_info *info)
{
	int i;

	dev_info(&info->pdev->dev, "EFI_VAR: fg_name=%s\n", vdata->fg_name);
	dev_info(&info->pdev->dev, "EFI_VAR: size=%d\n", vdata->size);
	dev_info(&info->pdev->dev, "EFI_VAR: sec_cksum=0x%x\n",
			vdata->sec_cksum);
	dev_info(&info->pdev->dev, "EFI_VAR: prim_cksum=0x%x\n",
			vdata->prim_cksum);
	dev_info(&info->pdev->dev, "EFI_VAR: cap1=0x%x\n",
			vdata->config_data[0]);
	dev_info(&info->pdev->dev, "EFI_VAR: cap0=0x%x\n",
			vdata->config_data[1]);
	dev_info(&info->pdev->dev, "EFI_VAR: rdc1=0x%x\n",
			vdata->config_data[2]);
	dev_info(&info->pdev->dev, "EFI_VAR: rdc0=0x%x\n",
			vdata->config_data[3]);

	for (i = 4; i < XPWR_FG_DATA_SIZE; i++)
		dev_info(&info->pdev->dev, "EFI_VAR: bat_curve[%d]=0x%x\n",
				i-4, vdata->config_data[i]);
}

static void
char_to_efi_char16(efi_char16_t *dest, const char *src, int dest_len)
{
	int i;

	for (i = 0; i < dest_len - 1; i++) {
		if (!src[i])
			break;
		dest[i] = src[i];
	}
	dest[i] = 0;
	return;
}

static int dc_xpwr_efi_save_fg_conf_data(struct pmic_fg_info *info)
{
	struct dc_xpwr_fg_config_data *cdata = &info->pdata->cdata;

	efi_char16_t efi_var_name[EFI_FG_VAR_NAME_SIZE];
	efi_status_t status;
	u32 attr = 0;
	struct xpwr_fg_sec_config_efi_data var_data;

	char_to_efi_char16(efi_var_name, EFI_FG_VAR_NAME, EFI_FG_VAR_NAME_SIZE);

	/* Copy the fg config data to variable data*/
	memset(&var_data, 0, sizeof(var_data));
	memcpy(var_data.fg_name, cdata->fg_name, ACPI_FG_CONF_NAME_LEN);
	var_data.size = cdata->size;
	var_data.prim_cksum = cdata->checksum;
	memcpy(var_data.config_data, &cdata->cap1, XPWR_FG_DATA_SIZE);
	var_data.sec_cksum = get_cksum(var_data.config_data, EFI_VAR_MAX_DATA);

	attr = EFI_VARIABLE_NON_VOLATILE
		| EFI_VARIABLE_BOOTSERVICE_ACCESS
		| EFI_VARIABLE_RUNTIME_ACCESS;

	dc_xpwr_dump_efi_var_data(&var_data, info);
	status = efi.set_variable(efi_var_name, &XPWR_FG_GUID,
					attr, sizeof(var_data), &var_data);
	if (status != EFI_SUCCESS) {
		dev_err(&info->pdev->dev,
			"Failed to set efi variable data,err=%lu\n",
			(unsigned long)status);
		return status;
	}
	dev_info(&info->pdev->dev, "Updated EFI variable data\n");
	return 0;
}

#endif /*CONFIG_ACPI*/

static int pmic_fg_reg_readb(struct pmic_fg_info *info, int reg)
{
	int ret, i;

	for (i = 0; i < NR_RETRY_CNT; i++) {
		ret = intel_soc_pmic_readb(reg);
		if (ret == -EAGAIN || ret == -ETIMEDOUT)
			continue;
		else
			break;
	}
	if (ret < 0)
		dev_err(&info->pdev->dev, "pmic reg read err:%d\n", ret);

	return ret;
}

static int pmic_fg_reg_writeb(struct pmic_fg_info *info, int reg, u8 val)
{
	int ret, i;

	for (i = 0; i < NR_RETRY_CNT; i++) {
		ret = intel_soc_pmic_writeb(reg, val);
		if (ret == -EAGAIN || ret == -ETIMEDOUT)
			continue;
		else
			break;
	}
	if (ret < 0)
		dev_err(&info->pdev->dev, "pmic reg write err:%d\n", ret);

	return ret;
}

static int pmic_fg_reg_setb(struct pmic_fg_info *info, int reg, u8 mask)
{
	int ret;

	ret = intel_soc_pmic_setb(reg, mask);
	if (ret < 0)
		dev_err(&info->pdev->dev, "pmic reg set mask err:%d\n", ret);
	return ret;
}

static int pmic_fg_reg_clearb(struct pmic_fg_info *info, int reg, u8 mask)
{
	int ret;

	ret = intel_soc_pmic_clearb(reg, mask);
	if (ret < 0)
		dev_err(&info->pdev->dev, "pmic reg set mask err:%d\n", ret);
	return ret;
}

static void pmic_fg_dump_init_regs(struct pmic_fg_info *info)
{
	int i;

	dev_info(&info->pdev->dev, "reg:%x, val:%x\n",
			DC_CHRG_CCCV_REG,
			pmic_fg_reg_readb(info, DC_CHRG_CCCV_REG));

	for (i = 0; i < XPWR_BAT_CURVE_SIZE; i++) {
		dev_info(&info->pdev->dev, "reg:%x, val:%x\n",
			DC_FG_OCV_CURVE_REG + i,
			pmic_fg_reg_readb(info, DC_FG_OCV_CURVE_REG + i));
	}

	dev_info(&info->pdev->dev, "reg:%x, val:%x\n",
				DC_FG_CNTL_REG,
				pmic_fg_reg_readb(info, DC_FG_CNTL_REG));
	dev_info(&info->pdev->dev, "reg:%x, val:%x\n",
				DC_FG_DES_CAP1_REG,
				pmic_fg_reg_readb(info, DC_FG_DES_CAP1_REG));
	dev_info(&info->pdev->dev, "reg:%x, val:%x\n",
				DC_FG_DES_CAP0_REG,
				pmic_fg_reg_readb(info, DC_FG_DES_CAP0_REG));
	dev_info(&info->pdev->dev, "reg:%x, val:%x\n",
				DC_FG_RDC1_REG,
				pmic_fg_reg_readb(info, DC_FG_RDC1_REG));
	dev_info(&info->pdev->dev, "reg:%x, val:%x\n",
				DC_FG_RDC0_REG,
				pmic_fg_reg_readb(info, DC_FG_RDC0_REG));
}

static int conv_adc_temp(int adc_val, int adc_max, int adc_diff, int temp_diff)
{
	int ret;

	ret = (adc_max - adc_val) * temp_diff;
	return ret / adc_diff;
}

static bool is_valid_temp_adc_range(int val, int min, int max)
{
	if (val > min && val <= max)
		return true;
	else
		return false;
}

static int dc_xpwr_get_batt_temp(int adc_val, int *temp)
{
	int i;

	for (i = 0; i < THERM_CURVE_MAX_SAMPLES; i++) {
		/* linear approximation for battery pack temperature */
		if (is_valid_temp_adc_range(adc_val, therm_curve_data[i][3],
					    therm_curve_data[i][2])) {

			*temp = conv_adc_temp(adc_val, therm_curve_data[i][2],
					     therm_curve_data[i][2] -
					     therm_curve_data[i][3],
					     therm_curve_data[i][0] -
					     therm_curve_data[i][1]);

			*temp += therm_curve_data[i][1];
			break;
		}
	}

	if (i >= THERM_CURVE_MAX_SAMPLES)
		return -ERANGE;

	return 0;

}

/**
 * pmic_read_adc_val - read ADC value of specified sensors
 * @channel: channel of the sensor to be sampled
 * @sensor_val: pointer to the charger property to hold sampled value
 * @chc :  battery info pointer
 *
 * Returns 0 if success
 */
static int pmic_read_adc_val(const char *map, const char *name,
			int *raw_val, struct pmic_fg_info *info)
{
	int ret, val;
	struct iio_channel *indio_chan;

	indio_chan = iio_channel_get(NULL, name);
	if (IS_ERR_OR_NULL(indio_chan)) {
		ret = PTR_ERR(indio_chan);
		goto exit;
	}
	ret = iio_read_channel_raw(indio_chan, &val);
	if (ret) {
		dev_err(&info->pdev->dev, "IIO channel read error\n");
		goto err_exit;
	}

	dev_dbg(&info->pdev->dev, "adc raw val=%x\n", val);
	*raw_val = val;

err_exit:
	iio_channel_release(indio_chan);
exit:
	return ret;
}

static int pmic_fg_get_vbatt(struct pmic_fg_info *info, int *vbatt)
{
	int ret, raw_val;

	ret = pmic_read_adc_val("VIBAT", "VBAT", &raw_val, info);
	if (ret < 0)
		goto vbatt_read_fail;

	*vbatt = ADC_TO_VBATT(raw_val);
vbatt_read_fail:
	return ret;
}

static int pmic_fg_get_current(struct pmic_fg_info *info, int *cur)
{
	int ret = 0, raw_val, sign;
	int pwr_stat;

	pwr_stat = pmic_fg_reg_readb(info, DC_PS_STAT_REG);
	if (pwr_stat < 0) {
		dev_err(&info->pdev->dev, "PWR STAT read failed:%d\n", pwr_stat);
		return pwr_stat;
	}

	if (pwr_stat & PS_STAT_BAT_CHRG_DIR) {
		sign = 1;
		ret = pmic_read_adc_val("CURRENT", "BATCCUR", &raw_val, info);
	} else {
		sign = -1;
		ret = pmic_read_adc_val("CURRENT", "BATDCUR", &raw_val, info);
	}
	if (ret < 0)
		goto cur_read_fail;

	*cur = raw_val * sign;
cur_read_fail:
	return ret;
}

static int pmic_fg_get_btemp(struct pmic_fg_info *info, int *btemp)
{
	int ret, raw_val;

	ret = pmic_read_adc_val("THERMAL", "BATTEMP", &raw_val, info);
	if (ret < 0)
		goto btemp_read_fail;

	/*
	 * Convert the TS pin ADC codes in to 10's of Kohms
	 * by deviding the ADC code with 10 and pass it to
	 * the Thermistor look up function.
	 */
	ret = dc_xpwr_get_batt_temp(raw_val / 10, btemp);
	if (ret < 0)
		dev_warn(&info->pdev->dev, "ADC conversion error%d\n", ret);
	else
		dev_dbg(&info->pdev->dev,
				"ADC code:%d, TEMP:%d\n", raw_val, *btemp);
btemp_read_fail:
	return ret;
}

static int pmic_fg_get_vocv(struct pmic_fg_info *info, int *vocv)
{
	int ret, value;

	/*
	 * OCV readings are 12-bit length. So Read
	 * the MSB first left-shift by 4 bits and
	 * read the lower nibble.
	 */
	ret = pmic_fg_reg_readb(info, DC_FG_OCVH_REG);
	if (ret < 0)
		goto vocv_read_fail;
	value = ret << 4;

	ret = pmic_fg_reg_readb(info, DC_FG_OCVL_REG);
	if (ret < 0)
		goto vocv_read_fail;
	value |= (ret & 0xf);

	*vocv = ADC_TO_VBATT(value);
vocv_read_fail:
	return ret;
}

static int pmic_fg_get_capacity(struct pmic_fg_info *info)
{
	int ret, value;

	ret = pmic_fg_get_vocv(info, &value);
	if (ret < 0)
		return ret;

	/* do Vocv min threshold check */
	if (value < info->pdata->design_min_volt)
		return 0;

	ret = pmic_fg_reg_readb(info, DC_FG_REP_CAP_REG);
	if (ret < 0)
		return ret;

	if (!(ret & FG_REP_CAP_VALID))
		dev_err(&info->pdev->dev,
				"capacity measurement not valid\n");

	return (ret & FG_REP_CAP_VAL_MASK);
}

static int pmic_fg_battery_health(struct pmic_fg_info *info)
{
	int temp, vocv;
	int ret, health = POWER_SUPPLY_HEALTH_UNKNOWN;

	/* Health cannot be predicted for an unknown (invalid) battery. */
	if (!strncmp(info->pdata->battid, BATTID_UNKNOWN, BATTID_STR_LEN))
		goto health_read_fail;

	ret = pmic_fg_get_btemp(info, &temp);
	if (ret < 0)
		goto health_read_fail;

	info->btemp = temp;

	ret = pmic_fg_get_vocv(info, &vocv);
	if (ret < 0)
		goto health_read_fail;

	if (vocv > (info->pdata->design_max_volt + XPWR_VBAT_VMAX_OFFSET))
		health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	else if (temp > info->pdata->max_temp ||
			temp < info->pdata->min_temp)
		health = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (vocv < info->pdata->design_min_volt)
		health = POWER_SUPPLY_HEALTH_DEAD;
	else
		health = POWER_SUPPLY_HEALTH_GOOD;

health_read_fail:
	return health;
}

static int pmic_fg_get_battery_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct pmic_fg_info *info = container_of(psy,
				struct pmic_fg_info, bat);
	int ret = 0, value;

	mutex_lock(&info->lock);
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = info->status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = pmic_fg_battery_health(info);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = pmic_fg_get_vbatt(info, &value);
		if (ret < 0)
			goto pmic_fg_read_err;
		val->intval = value * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		ret = pmic_fg_get_vocv(info, &value);
		if (ret < 0)
			goto pmic_fg_read_err;

		val->intval = value * 1000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = pmic_fg_get_current(info, &value);
		if (ret < 0)
			goto pmic_fg_read_err;
		val->intval = value * 1000;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		ret = pmic_fg_reg_readb(info, DC_CHRG_STAT_REG);
		if (ret < 0)
			goto pmic_fg_read_err;

		if (ret & CHRG_STAT_BAT_PRESENT)
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		ret = pmic_fg_get_capacity(info);
		if (ret < 0)
			goto pmic_fg_read_err;

		val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = pmic_fg_get_btemp(info, &value);
		if (ret < 0)
			goto pmic_fg_read_err;
		val->intval = value * 10;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		ret = pmic_fg_reg_readb(info, DC_FG_CC_MTR1_REG);
		if (ret < 0)
			goto pmic_fg_read_err;

		value = (ret & FG_CC_MTR1_VAL_MASK) << 8;
		ret = pmic_fg_reg_readb(info, DC_FG_CC_MTR0_REG);
		if (ret < 0)
			goto pmic_fg_read_err;
		value |= (ret & FG_CC_MTR0_VAL_MASK);
		val->intval = value * FG_DES_CAP_RES_LSB;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = pmic_fg_reg_readb(info, DC_FG_DES_CAP1_REG);
		if (ret < 0)
			goto pmic_fg_read_err;

		value = (ret & DC_FG_DES_CAP1_VAL_MASK) << 8;
		ret = pmic_fg_reg_readb(info, DC_FG_DES_CAP0_REG);
		if (ret < 0)
			goto pmic_fg_read_err;
		value |= (ret & FG_DES_CAP0_VAL_MASK);
		val->intval = value * FG_DES_CAP_RES_LSB;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = info->pdata->design_cap * 1000;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = info->pdata->battid;
		break;
	default:
		mutex_unlock(&info->lock);
		return -EINVAL;
	}

	mutex_unlock(&info->lock);
	return 0;

pmic_fg_read_err:
	mutex_unlock(&info->lock);
	return ret;
}

static void pmic_fg_handle_battery_detection(struct pmic_fg_info *info)
{
	int ret;
	int batt_cap = pmic_fg_get_capacity(info);

	ret = pmic_fg_reg_readb(info, DC_PWR_BAT_LED_CNTL_REG);
	if (ret < 0)
		return;
	/*
	 * PMIC is raising spurious charging done and start interrupts
	 * continuously if high load applied near battery full.
	 * To avoid these spurious  interrupts xpwr vendor as suggested a
	 * software WA to disable battery detection logic in pmic
	 * if  battery capacity > 95% and charging.
	 */
	if ((batt_cap >= BATT_DET_CAP_THLD)
			&& ((info->status == POWER_SUPPLY_STATUS_CHARGING)
			|| (info->status == POWER_SUPPLY_STATUS_FULL))) {
		if (ret & CHRG_BATT_DET_MASK)
			pmic_fg_reg_clearb(info,
				DC_PWR_BAT_LED_CNTL_REG, CHRG_BATT_DET_MASK);
	} else {
		if (!(ret & CHRG_BATT_DET_MASK))
			pmic_fg_reg_setb(info,
				DC_PWR_BAT_LED_CNTL_REG, CHRG_BATT_DET_MASK);
	}
}

static int pmic_fg_set_battery_property(struct power_supply *psy,
				    enum power_supply_property psp,
				    const union power_supply_propval *val)
{
	struct pmic_fg_info *info = container_of(psy,
				struct pmic_fg_info, bat);
	int ret = 0;

	mutex_lock(&info->lock);
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (info->status != val->intval) {
			info->status = val->intval;
			pmic_fg_handle_battery_detection(info);
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static void pmic_fg_status_monitor(struct work_struct *work)
{
	struct pmic_fg_info *info = container_of(work,
		struct pmic_fg_info, status_monitor.work);
	static int cache_cap = -1;
	static int cache_health = POWER_SUPPLY_HEALTH_UNKNOWN;
	static int cache_temp = INT_MAX;
	int present_cap, present_health;
	unsigned long delay = STATUS_MON_DELAY_JIFFIES;

	mutex_lock(&info->lock);
	present_cap = pmic_fg_get_capacity(info);
	if (present_cap < 0) {
		mutex_unlock(&info->lock);
		goto end_stat_mon;
	}
	/*
	 *temp and ocv values are read here.
	*/
	present_health = pmic_fg_battery_health(info);
	mutex_unlock(&info->lock);

	/*
	 *PSY change event is sent only upon change in
	 *health,cap,temp.
	*/
	if ((cache_health != present_health)
			|| (cache_cap != present_cap)
			|| (info->btemp != cache_temp)) {
		power_supply_changed(&info->bat);
		cache_cap = present_cap;
		cache_health = present_health;
		cache_temp = info->btemp;
	} else if (((present_cap >= FULL_CAP_THLD)
			&& (info->status == POWER_SUPPLY_STATUS_CHARGING))
			|| (info->status == POWER_SUPPLY_STATUS_FULL)) {
		power_supply_changed(&info->bat);
		/* During full condition, schedule the monitor thread
		 * at 30 sec to monitor the full condition and
		 * maintanence charging thresholds.
		 */
		delay = STATUS_MON_FULL_DELAY_JIFFIES;
	}
	pmic_fg_handle_battery_detection(info);

end_stat_mon:
	schedule_delayed_work(&info->status_monitor, delay);
}

static irqreturn_t pmic_fg_thread_handler(int irq, void *dev)
{
	struct pmic_fg_info *info = dev;
	int i;

	for (i = 0; i < DC_FG_INTR_NUM; i++) {
		if (info->irq[i] == irq)
			break;
	}

	if (i >= DC_FG_INTR_NUM) {
		dev_warn(&info->pdev->dev, "spurious interrupt!!\n");
		return IRQ_NONE;
	}

	switch (i) {
	case QWBTU_IRQ:
		dev_info(&info->pdev->dev,
			"Quit Battery Under Temperature(DISCHRG) INTR\n");
		break;
	case WBTU_IRQ:
		dev_info(&info->pdev->dev,
			"Hit Battery Over Temperature(DISCHRG) INTR\n");
		break;
	case QWBTO_IRQ:
		dev_info(&info->pdev->dev,
			"Quit Battery Over Temperature(DISCHRG) INTR\n");
		break;
	case WBTO_IRQ:
		dev_info(&info->pdev->dev,
			"Hit Battery Over Temperature(DISCHRG) INTR\n");
		break;
	case WL2_IRQ:
		dev_info(&info->pdev->dev, "Low Batt Warning(2) INTR\n");
		break;
	case WL1_IRQ:
		dev_info(&info->pdev->dev, "Low Batt Warning(1) INTR\n");
		break;
	default:
		dev_warn(&info->pdev->dev, "Spurious Interrupt!!!\n");
	}

	power_supply_changed(&info->bat);
	return IRQ_HANDLED;
}

static void pmic_fg_external_power_changed(struct power_supply *psy)
{
	struct pmic_fg_info *info = container_of(psy,
				struct pmic_fg_info, bat);

	power_supply_changed(&info->bat);
}


static int pmic_fg_update_config_params(struct pmic_fg_info *info)
{
	int ret;
	int i;
	struct dc_xpwr_fg_config_data *cdata;

	cdata = &info->pdata->cdata;
	ret = pmic_fg_reg_readb(info, DC_FG_DES_CAP1_REG);
	if (ret < 0)
		goto fg_svae_cfg_fail;
	cdata->cap1 = ret;

	/*
	 * higher byte and lower byte reads should be
	 * back to back to get successful lower byte result.
	 */
	pmic_fg_reg_readb(info, DC_FG_DES_CAP1_REG);
	ret = pmic_fg_reg_readb(info, DC_FG_DES_CAP0_REG);
	if (ret < 0)
		goto fg_svae_cfg_fail;
	else
		cdata->cap0 = ret;

	ret = pmic_fg_reg_readb(info, DC_FG_RDC1_REG);
	if (ret < 0)
		goto fg_svae_cfg_fail;
	else
		cdata->rdc1 = ret;

	/*
	 * higher byte and lower byte reads should be
	 * back to back to get successful lower byte result.
	 */
	pmic_fg_reg_readb(info, DC_FG_RDC1_REG);
	ret = pmic_fg_reg_readb(info, DC_FG_RDC0_REG);
	if (ret < 0)
		goto fg_svae_cfg_fail;
	else
		cdata->rdc0 = ret;

	for (i = 0; i < XPWR_BAT_CURVE_SIZE; i++) {
		ret = pmic_fg_reg_readb(info, DC_FG_OCV_CURVE_REG + i);
		if (ret < 0)
			goto fg_svae_cfg_fail;
		else
			cdata->bat_curve[i] = ret;
	}

	return 0;

fg_svae_cfg_fail:
	return ret;
}

static int pmic_fg_save_fg_config_params(struct pmic_fg_info *info)
{

	/* Read and update the config data from pmic to local strucure */
	pmic_fg_update_config_params(info);

#ifdef CONFIG_ACPI
	/* Save the FG config data to EFI variables */
	if (!dc_xpwr_efi_save_fg_conf_data(info))
		return 0;
#endif
	dev_info(&info->pdev->dev, "FG config save/restore not supported\n");
	return 0;
}

static int pmic_fg_set_lowbatt_thresholds(struct pmic_fg_info *info)
{
	int ret;
	u8 reg_val;

	ret = pmic_fg_reg_readb(info, DC_FG_REP_CAP_REG);
	if (ret < 0) {
		dev_err(&info->pdev->dev, "%s:read err:%d\n", __func__, ret);
		return ret;
	}
	ret = (ret & FG_REP_CAP_VAL_MASK);

	if (ret > FG_LOW_CAP_WARN_THR)
		reg_val = FG_LOW_CAP_WARN_THR;
	else if (ret > FG_LOW_CAP_CRIT_THR)
		reg_val = FG_LOW_CAP_CRIT_THR;
	else
		reg_val = FG_LOW_CAP_SHDN_THR;

	reg_val |= FG_LOW_CAP_THR1_VAL;
	ret = pmic_fg_reg_writeb(info, DC_FG_LOW_CAP_REG, reg_val);
	if (ret < 0)
		dev_err(&info->pdev->dev, "%s:write err:%d\n", __func__, ret);

	return ret;
}

static int pmic_fg_program_vbatt_full(struct pmic_fg_info *info)
{
	int ret;
	u8 val;

	ret = pmic_fg_reg_readb(info, DC_CHRG_CCCV_REG);
	if (ret < 0)
		goto fg_prog_ocv_fail;
	else
		val = (ret & ~CHRG_CCCV_CV_MASK);

	switch (info->pdata->design_max_volt) {
	case CV_4100:
		val |= (CHRG_CCCV_CV_4100MV << CHRG_CCCV_CV_BIT_POS);
		break;
	case CV_4150:
		val |= (CHRG_CCCV_CV_4150MV << CHRG_CCCV_CV_BIT_POS);
		break;
	case CV_4200:
		val |= (CHRG_CCCV_CV_4200MV << CHRG_CCCV_CV_BIT_POS);
		break;
	case CV_4350:
		val |= (CHRG_CCCV_CV_4350MV << CHRG_CCCV_CV_BIT_POS);
		break;
	default:
		val |= (CHRG_CCCV_CV_4200MV << CHRG_CCCV_CV_BIT_POS);
		break;
	}

	ret = pmic_fg_reg_writeb(info, DC_CHRG_CCCV_REG, val);
fg_prog_ocv_fail:
	return ret;
}

static int pmic_fg_program_design_cap(struct pmic_fg_info *info)
{
	int ret = 0;
	int cap0, cap1;

	cap1 = pmic_fg_reg_readb(info, DC_FG_DES_CAP1_REG);
	if (cap1 < 0) {
		dev_warn(&info->pdev->dev, "CAP1 reg read err!!\n");
		goto fg_prog_descap_fail;
	}
	cap0 = pmic_fg_reg_readb(info, DC_FG_DES_CAP0_REG);
	if (cap0 < 0) {
		dev_warn(&info->pdev->dev, "CAP0 reg read err!!\n");
		goto fg_prog_descap_fail;
	}

	/* If CAP values are as expected, skip capacity initialization*/
	if ((cap1 == info->pdata->cdata.cap1)
		&& (cap0 == info->pdata->cdata.cap0)) {
		return 0;
	}

	ret = pmic_fg_reg_writeb(info, DC_FG_DES_CAP1_REG,
					info->pdata->cdata.cap1);
	if (ret < 0) {
		dev_warn(&info->pdev->dev, "CAP1 reg write err!!\n");
		goto fg_prog_descap_fail;
	}

	ret = pmic_fg_reg_writeb(info, DC_FG_DES_CAP0_REG,
					info->pdata->cdata.cap0);
	if (ret < 0)
		dev_warn(&info->pdev->dev, "CAP0 reg write err!!\n");

fg_prog_descap_fail:
	return ret;
}

static int pmic_fg_program_ocv_curve(struct pmic_fg_info *info)
{
	int ret, i;

	for (i = 0; i < XPWR_BAT_CURVE_SIZE; i++) {
		ret = pmic_fg_reg_writeb(info,
			DC_FG_OCV_CURVE_REG + i,
			info->pdata->cdata.bat_curve[i]);
		if (ret < 0)
			goto fg_prog_ocv_fail;
	}

fg_prog_ocv_fail:
	return ret;
}

static int pmic_fg_program_rdc_vals(struct pmic_fg_info *info)
{
	int ret = 0;
	int rdc0, rdc1, cntl4;

	rdc1 = pmic_fg_reg_readb(info, DC_FG_RDC1_REG);
	if (rdc1 < 0) {
		dev_warn(&info->pdev->dev, "RDC1 reg read err!!\n");
		goto fg_prog_rdc_fail;
	}
	rdc0 = pmic_fg_reg_readb(info, DC_FG_RDC0_REG);
	if (rdc0 < 0) {
		dev_warn(&info->pdev->dev, "RDC0 reg read err!!\n");
		goto fg_prog_rdc_fail;
	}
	/* If RDC values are as expected, skip RDC initialization*/
	if ((rdc1 == info->pdata->cdata.rdc1)
		&& (rdc0 == info->pdata->cdata.rdc0)) {
		return 0;
	}

	ret = pmic_fg_reg_writeb(info, DC_FG_RDC1_REG, info->pdata->cdata.rdc1);
	if (ret < 0)
		goto fg_prog_rdc_fail;

	ret = pmic_fg_reg_writeb(info, DC_FG_RDC0_REG, info->pdata->cdata.rdc0);

	cntl4 = pmic_fg_reg_clearb(info, DC_FG_TUNING_CNTL4,
					CNTL4_RDC_MIN_VOLT_RESET_MASK);
	ret = pmic_fg_reg_setb(info, DC_FG_TUNING_CNTL4,
					CNTL4_RDC_MIN_VOLT_SET_MASK);

fg_prog_rdc_fail:
	return ret;
}

static void pmic_fg_init_config_regs(struct pmic_fg_info *info)
{
	int ret = 0;

	/*
	 * check if the config data is already
	 * programmed and if so just return.
	 */
	ret = pmic_fg_reg_readb(info, DC_FG_CNTL_REG);
	if (ret < 0) {
		dev_warn(&info->pdev->dev, "FG CNTL reg read err!!\n");
	} else if ((ret & FG_CNTL_OCV_ADJ_EN) && (ret & FG_CNTL_CAP_ADJ_EN)
			&& !info->pdata->cdata.fco) {
		dev_info(&info->pdev->dev,
			 "Only OCV curve from FG data is initialized\n");
		/*
		 * ocv curve will be set to default values
		 * at every boot, so it is needed to explicitly write
		 * the ocv curve data for each boot
		 */
		ret = pmic_fg_program_ocv_curve(info);
		if (ret < 0)
			dev_err(&info->pdev->dev,
				"set ocv curve fail:%d\n", ret);
		pmic_fg_dump_init_regs(info);
		return;

	}
	dev_info(&info->pdev->dev, "FG data need to be initialized\n");

	dev_info(&info->pdev->dev, "DisableFG during  initialization\n");
	ret = pmic_fg_reg_writeb(info, DC_FG_CNTL_REG, 0x00);
	if (ret < 0)
		dev_err(&info->pdev->dev, "gauge cntl set fail:%d\n", ret);

	ret = pmic_fg_program_ocv_curve(info);
	if (ret < 0)
		dev_err(&info->pdev->dev, "set ocv curve fail:%d\n", ret);

	ret = pmic_fg_program_rdc_vals(info);
	if (ret < 0)
		dev_err(&info->pdev->dev, "set rdc fail:%d\n", ret);

	ret = pmic_fg_program_design_cap(info);
	if (ret < 0)
		dev_err(&info->pdev->dev, "set design cap fail:%d\n", ret);

	ret = pmic_fg_program_vbatt_full(info);
	if (ret < 0)
		dev_err(&info->pdev->dev, "set vbatt full fail:%d\n", ret);

	ret = pmic_fg_set_lowbatt_thresholds(info);
	if (ret < 0)
		dev_err(&info->pdev->dev, "lowbatt thr set fail:%d\n", ret);

	ret = pmic_fg_reg_writeb(info, DC_FG_CNTL_REG, 0xef);
	if (ret < 0)
		dev_err(&info->pdev->dev, "gauge cntl set fail:%d\n", ret);

	pmic_fg_dump_init_regs(info);
}

static void pmic_fg_init_irq(struct pmic_fg_info *info)
{
	int ret, i;

	for (i = 0; i < DC_FG_INTR_NUM; i++) {
		info->irq[i] = platform_get_irq(info->pdev, i);
		ret = request_threaded_irq(info->irq[i],
				NULL, pmic_fg_thread_handler,
				IRQF_ONESHOT, DEV_NAME, info);
		if (ret) {
			dev_warn(&info->pdev->dev,
				"cannot get IRQ:%d\n", info->irq[i]);
			info->irq[i] = -1;
			goto intr_failed;
		} else {
			dev_info(&info->pdev->dev, "IRQ No:%d\n", info->irq[i]);
		}
	}
	return;

intr_failed:
	for (; i > 0; i--) {
		free_irq(info->irq[i - 1], info);
		info->irq[i - 1] = -1;
	}
}

static void pmic_fg_init_hw_regs(struct pmic_fg_info *info)
{
	/* program temperature thresholds */
	intel_soc_pmic_writeb(DC_FG_VLTFW_REG, FG_VLTFW_0C);
	intel_soc_pmic_writeb(DC_FG_VHTFW_REG, FG_VHTFW_56C);

	/* Set the Charge end condition to 20% of CC to allow
	 * charging framework to handle maintenance charging.
	*/
	pmic_fg_reg_setb(info, DC_CHRG_CCCV_REG, CHRG_CCCV_ITERM_20P);

	/* enable interrupts */
	intel_soc_pmic_setb(DC_TEMP_IRQ_CFG_REG, TEMP_IRQ_CFG_MASK);
	intel_soc_pmic_setb(DC_FG_IRQ_CFG_REG, FG_IRQ_CFG_LOWBATT_MASK);
}

static void pmic_fg_init_psy(struct pmic_fg_info *info)
{
	info->status = POWER_SUPPLY_STATUS_DISCHARGING;
}

static int pmic_fg_probe(struct platform_device *pdev)
{
	struct pmic_fg_info *info;
	int ret = 0;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "mem alloc failed\n");
		return -ENOMEM;
	}

	info->pdev = pdev;
	info->pdata = pdev->dev.platform_data;
	if (!info->pdata)
		return -ENODEV;

	platform_set_drvdata(pdev, info);
	mutex_init(&info->lock);
	INIT_DELAYED_WORK(&info->status_monitor, pmic_fg_status_monitor);

	pmic_fg_init_config_regs(info);
	pmic_fg_init_psy(info);

	info->bat.name = DEV_NAME;
	info->bat.type = POWER_SUPPLY_TYPE_BATTERY;
	info->bat.properties = pmic_fg_props;
	info->bat.num_properties = ARRAY_SIZE(pmic_fg_props);
	info->bat.get_property = pmic_fg_get_battery_property;
	info->bat.set_property = pmic_fg_set_battery_property;
	info->bat.external_power_changed = pmic_fg_external_power_changed;
	ret = power_supply_register(&pdev->dev, &info->bat);
	if (ret) {
		dev_err(&pdev->dev, "failed to register battery: %d\n", ret);
		return ret;
	}

	/* register fuel gauge interrupts */
	pmic_fg_init_irq(info);
	pmic_fg_init_hw_regs(info);
	schedule_delayed_work(&info->status_monitor, STATUS_MON_DELAY_JIFFIES);
	return 0;
}

static int pmic_fg_remove(struct platform_device *pdev)
{
	struct pmic_fg_info *info = platform_get_drvdata(pdev);
	int i;

	cancel_delayed_work_sync(&info->status_monitor);
	for (i = 0; i < DC_FG_INTR_NUM && info->irq[i] != -1; i++)
		free_irq(info->irq[i], info);
	power_supply_unregister(&info->bat);
	return 0;
}

static int pmic_fg_suspend(struct device *dev)
{
	struct pmic_fg_info *info = dev_get_drvdata(dev);

	dev_dbg(dev, "%s called\n", __func__);
	/*
	 * set lowbatt thresholds to
	 * wake the platform from S3.
	 */
	pmic_fg_set_lowbatt_thresholds(info);
	cancel_delayed_work_sync(&info->status_monitor);
	return 0;
}

static int pmic_fg_resume(struct device *dev)
{
	struct pmic_fg_info *info = dev_get_drvdata(dev);

	dev_dbg(dev, "%s called\n", __func__);

	schedule_delayed_work(&info->status_monitor, 0);
	return 0;
}

static int pmic_fg_runtime_suspend(struct device *dev)
{

	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int pmic_fg_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int pmic_fg_runtime_idle(struct device *dev)
{

	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static void pmic_fg_shutdown(struct platform_device *pdev)
{
	struct pmic_fg_info *info = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "%s called\n", __func__);
	if (info->pdata->fg_save_restore_enabled)
		pmic_fg_save_fg_config_params(info);

}
static const struct dev_pm_ops pmic_fg_pm_ops = {
		SET_SYSTEM_SLEEP_PM_OPS(pmic_fg_suspend,
				pmic_fg_resume)
		SET_RUNTIME_PM_OPS(pmic_fg_runtime_suspend,
				pmic_fg_runtime_resume,
				pmic_fg_runtime_idle)
};

static struct platform_driver pmic_fg_driver = {
	.driver = {
		.name = DEV_NAME,
		.owner	= THIS_MODULE,
		.pm = &pmic_fg_pm_ops,
	},
	.probe = pmic_fg_probe,
	.remove = pmic_fg_remove,
	.shutdown = pmic_fg_shutdown,
};

static int __init dc_pmic_fg_init(void)
{
	return platform_driver_register(&pmic_fg_driver);
}
device_initcall(dc_pmic_fg_init);

static void __exit dc_pmic_fg_exit(void)
{
	platform_driver_unregister(&pmic_fg_driver);
}
module_exit(dc_pmic_fg_exit);

MODULE_AUTHOR("Ramakrishna Pallala <ramakrishna.pallala@intel.com>");
MODULE_DESCRIPTION("Dollar Cove(X-power) PMIC Battery Driver");
MODULE_LICENSE("GPL");
