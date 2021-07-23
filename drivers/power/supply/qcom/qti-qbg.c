// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"QBG_K: %s: " fmt, __func__

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/iio/iio.h>
#include <linux/iio/consumer.h>
#include <linux/interrupt.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/qti_power_supply.h>
#include <linux/regmap.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

#include <dt-bindings/iio/qti_power_supply_iio.h>

#include <uapi/linux/qbg.h>
#include <uapi/linux/qbg-profile.h>

#include "qbg-iio.h"
#include "qbg-battery-profile.h"
#include "qbg-core.h"
#include "qbg-sdam.h"
#include "battery-profile-loader.h"

/* QBG register definitions */
#define QBG_MAIN_STATUS1				0x08
#define QBG_OK_BIT					BIT(7)

#define QBG_MAIN_STATUS2				0x09
#define QBG_BATTERY_MISSING_BIT				BIT(2)
#define QBG_ADC_CONV_FAULT_OCCURRED_BIT			BIT(1)
#define QBG_DATA_RESET_BIT				BIT(0)

#define QBG_MAIN_STATUS3				0x0a
#define QBG_FSM_STATE_MASK				GENMASK(3, 0)

#define QBG_MAIN_QBG_STATE_FORCE_CMD			0x41
#define FORCE_FAST_CHAR_MODE_BIT			BIT(3)
#define FORCE_FAST_CHAR_SHIFT				3

#define QBG_MAIN_EN_CTL					0x46
#define QBG_EN_BIT					BIT(7)

#define QBG_MAIN_MPM_TO_LPM_IBAT_THRESH			0x50

#define QBG_MAIN_LPM_TO_MPM_IBAT_THRESH			0x51

#define QBG_MAIN_HPM_TO_MPM_IBAT_THRESH			0x52

#define QBG_MAIN_MPM_TO_HPM_IBAT_THRESH			0x53

#define QBG_MAIN_VBAT_EMPTY_THRESH			0x56

#define QBG_MAIN_HPM_MEAS_CTL2				0x5d

#define QBG_MAIN_MPM_MEAS_CTL2				0x61

#define QBG_MAIN_LPM_MEAS_CTL2				0x65
#define QBG_NUM_OF_ACCUM_MASK				GENMASK(7, 5)
#define QBG_NUM_OF_ACCUM_SHIFT				5
#define QBG_ACCUM_INTERVAL_MASK				GENMASK(2, 0)

#define QBG_MAIN_FAST_CHAR_MEAS_CTL2			0x69

#define QBG_MAIN_PON_OCV_ACC0_DATA0			0x70

#define QBG_MAIN_LAST_ADC_ACC0_DATA0			0x9A

#define QBG_MAIN_LAST_BURST_AVG_ACC0_DATA0		0xA0

#define QBG_MAIN_LAST_BURST_AVG_ACC2_DATA0		0xA4

#define QBG_FAST_CHAR_DELTA_MS				100000
#define VBATT_1S_LSB					19463
#define IBATT_10A_LSB					6103
#define VBAT_0PCT_OCV					300000000ULL
#define ICHG_FS_10A					1
#define TBAT_LSB					7500
#define TBAT_DENOMINATOR				115600

#define BOOST_COMPENSATION_UV				400000
#define OCV_BOOST_MIN_UV				4100000
#define OCV_BOOST_MAX_UV				5600000
#define OCV_BOOST_STEP_UV				100000

#define BATT_HOT_DECIDEGREE_MAX				600

#define MILLI_TO_10NANO				100000
#define MICRO_TO_10NANO				100
#define MILLI_TO_MICRO				1000

static int qbg_debug_mask;

static const unsigned int qbg_accum_interval[8] = {
	10000, 20000, 50000, 100000, 150000, 200000, 500000, 1000000};
static const unsigned int qbg_lpm_accum_interval[8] = {
	100000, 200000, 500000, 1000000, 2000000, 5000000, 10000000, 100000000};
static const unsigned int qbg_fast_char_avg_interval[8] = {
	6250, 12500, 25000, 50000, 100000, 200000, 400000, 800000};

static const struct qbg_iio_channels qbg_iio_psy_channels[] = {
	QBG_CHAN_INDEX("debug_battery", PSY_IIO_DEBUG_BATTERY)
	QBG_CHAN_ENERGY("capacity", PSY_IIO_CAPACITY)
	QBG_CHAN_ENERGY("real_capacity", PSY_IIO_REAL_CAPACITY)
	QBG_CHAN_TEMP("temp", PSY_IIO_TEMP)
	QBG_CHAN_VOLT("voltage_max", PSY_IIO_VOLTAGE_MAX)
	QBG_CHAN_VOLT("voltage_now", PSY_IIO_VOLTAGE_NOW)
	QBG_CHAN_VOLT("voltage_ocv", PSY_IIO_VOLTAGE_OCV)
	QBG_CHAN_VOLT("voltage_avg", PSY_IIO_VOLTAGE_AVG)
	QBG_CHAN_CUR("current_now", PSY_IIO_CURRENT_NOW)
	QBG_CHAN_RES("resistance_id", PSY_IIO_RESISTANCE_ID)
	QBG_CHAN_TSTAMP("time_to_full_avg", PSY_IIO_TIME_TO_FULL_AVG)
	QBG_CHAN_TSTAMP("time_to_full_now", PSY_IIO_TIME_TO_FULL_NOW)
	QBG_CHAN_TSTAMP("time_to_empty_avg", PSY_IIO_TIME_TO_EMPTY_AVG)
	QBG_CHAN_RES("esr", PSY_IIO_ESR_ACTUAL)
	QBG_CHAN_INDEX("soh", PSY_IIO_SOH)
	QBG_CHAN_COUNT("cycle_count", PSY_IIO_CYCLE_COUNT)
	QBG_CHAN_ENERGY("charge_full", PSY_IIO_CHARGE_FULL)
	QBG_CHAN_ENERGY("charge_full_design", PSY_IIO_CHARGE_FULL_DESIGN)
	QBG_CHAN_ENERGY("charge_counter", PSY_IIO_CHARGE_COUNTER)
};

int qbg_read(struct qti_qbg *chip, u32 addr, u8 *val, int len)
{
	int rc;

	rc = regmap_bulk_read(chip->regmap, chip->base + addr, val, len);
	if (rc < 0) {
		pr_err("Failed to read from address %04x rc=%d\n", chip->base + addr, rc);
		return rc;
	}

	if (*chip->debug_mask & QBG_DEBUG_BUS_READ)
		pr_info("length %d addr=%#x data:%*ph\n", len, chip->base + addr, len, val);

	return 0;
}

int qbg_write(struct qti_qbg *chip, u32 addr, u8 *val, int len)
{
	int rc;

	rc = regmap_bulk_write(chip->regmap, chip->base + addr, val, len);
	if (rc < 0) {
		pr_err("Failed to write to address %04x rc=%d\n",
				chip->base + addr, rc);
		return rc;
	}

	if (*chip->debug_mask & QBG_DEBUG_BUS_WRITE)
		pr_info("length %d addr=%#x data:%*ph\n", len, chip->base + addr, len, val);

	return 0;
}

static void qbg_notify_charger(struct qti_qbg *chip)
{
	union power_supply_propval prop = {0, };
	int rc;

	if (!chip->batt_psy || !chip->profile_loaded)
		return;

	prop.intval = chip->float_volt_uv;
	rc = power_supply_set_property(chip->batt_psy,
			POWER_SUPPLY_PROP_VOLTAGE_MAX, &prop);
	if (rc < 0) {
		pr_err("Failed to set voltage_max property on batt_psy, rc=%d\n",
			rc);
		return;
	}

	prop.intval = chip->fastchg_curr_ma * 1000;
	rc = power_supply_set_property(chip->batt_psy,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &prop);
	if (rc < 0) {
		pr_err("Failed to set constant_charge_current_max property on batt_psy, rc=%d\n",
			rc);
		return;
	}

	pr_debug("Notified charger on float voltage:%d uV and FCC:%d mA\n",
			chip->float_volt_uv, chip->fastchg_curr_ma);
}

static bool is_batt_available(struct qti_qbg *chip)
{
	if (chip->batt_psy)
		return true;

	chip->batt_psy = power_supply_get_by_name("battery");
	if (!chip->batt_psy)
		return false;

	/* batt_psy is initialized, set the fcc and fv */
	qbg_notify_charger(chip);

	return true;
}

static void status_change_work(struct work_struct *work)
{
	struct qti_qbg *chip = container_of(work, struct qti_qbg,
						status_change_work);
	union power_supply_propval prop = {0, };
	int rc;

	if (!is_batt_available(chip))
		return;

	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_CHARGE_TYPE, &prop);
	if (rc < 0)
		pr_err("Failed to get charge-type, rc=%d\n", rc);
	else
		chip->charge_type = prop.intval;
}

static int qbg_get_fifo_count(struct qti_qbg *chip, u32 *fifo_count)
{
	int rc = 0;
	u8 val[2];

	rc = qbg_sdam_read(chip,
		QBG_SDAM_BASE(chip, SDAM_CTRL0) + QBG_SDAM_FIFO_COUNT_OFFSET,
		val, 2);
	if (rc < 0) {
		pr_err("Failed to read QBG SDAM, rc=%d\n", rc);
		return rc;
	}

	*fifo_count = (val[1] << 8) | val[0];
	qbg_dbg(chip, QBG_DEBUG_SDAM, "FIFO count=%d\n", *fifo_count);

	return rc;
}

static void update_ocv_for_boost(struct qti_qbg *chip)
{
	int ocv_for_boost = 0x00;

	/*
	 * Every time when OCV gets updated, add 0.4V headroom compensation
	 * and round it to a value between 4.1V to 5.6V, then map it to a
	 * setting between [0x00 - 0x0F] and set it in SDAM to notify charger-boost.
	 */
	ocv_for_boost = chip->ocv_uv + BOOST_COMPENSATION_UV;
	if (ocv_for_boost < OCV_BOOST_MIN_UV)
		ocv_for_boost = OCV_BOOST_MIN_UV;
	else if (ocv_for_boost > OCV_BOOST_MAX_UV)
		ocv_for_boost = OCV_BOOST_MAX_UV;

	ocv_for_boost -= OCV_BOOST_MIN_UV;
	ocv_for_boost /= OCV_BOOST_STEP_UV;

	qbg_dbg(chip, QBG_DEBUG_STATUS, "Boost OCV value written =%d\n", ocv_for_boost);
	qbg_sdam_write(chip,
		QBG_SDAM_BASE(chip, SDAM_CTRL0) + QBG_SDAM_BHARGER_OCV_HDRM_OFFSET,
		(u8 *)&ocv_for_boost, 1);
}

static void process_udata_work(struct work_struct *work)
{
	struct qti_qbg *chip = container_of(work, struct qti_qbg, udata_work);
	int rc;

	if (chip->udata.param[QBG_PARAM_BATT_SOC].valid)
		chip->batt_soc = chip->udata.param[QBG_PARAM_BATT_SOC].data;

	if (chip->udata.param[QBG_PARAM_SOC].valid ||
			chip->udata.param[QBG_PARAM_SYS_SOC].valid) {

		qbg_dbg(chip, QBG_DEBUG_SOC, "udata update: QBG_SOC=%d QBG_SYS_SOC=%d\n",
				chip->udata.param[QBG_PARAM_SOC].valid ?
				chip->udata.param[QBG_PARAM_SOC].data : -EINVAL,
				chip->udata.param[QBG_PARAM_SYS_SOC].valid ?
				chip->udata.param[QBG_PARAM_SYS_SOC].data : -EINVAL);

		if (chip->udata.param[QBG_PARAM_SYS_SOC].valid)
			chip->sys_soc = chip->udata.param[QBG_PARAM_SYS_SOC].data;

		chip->soc = chip->udata.param[QBG_PARAM_SOC].data;
	}

	if (chip->udata.param[QBG_PARAM_ESR].valid) {
		chip->esr = chip->udata.param[QBG_PARAM_ESR].data;

		if (chip->qbg_psy)
			power_supply_changed(chip->qbg_psy);
	}

	if (chip->udata.param[QBG_PARAM_OCV_UV].valid) {
		chip->ocv_uv = chip->udata.param[QBG_PARAM_OCV_UV].data;
		update_ocv_for_boost(chip);
	}

	if (chip->udata.param[QBG_PARAM_CHARGE_CYCLE_COUNT].valid)
		chip->charge_cycle_count =
			chip->udata.param[QBG_PARAM_CHARGE_CYCLE_COUNT].data;

	if (chip->udata.param[QBG_PARAM_LEARNED_CAPACITY].valid)
		chip->learned_capacity =
			chip->udata.param[QBG_PARAM_LEARNED_CAPACITY].data;

	if (chip->udata.param[QBG_PARAM_TTF_100MS].valid)
		chip->ttf = chip->udata.param[QBG_PARAM_TTF_100MS].data;

	if (chip->udata.param[QBG_PARAM_TTE_100MS].valid)
		chip->tte = chip->udata.param[QBG_PARAM_TTE_100MS].data;

	if (chip->udata.param[QBG_PARAM_SOH].valid)
		chip->soh = chip->udata.param[QBG_PARAM_SOH].data;

	if (chip->udata.param[QBG_PARAM_TBAT].valid)
		chip->tbat = chip->udata.param[QBG_PARAM_TBAT].data;

	if (chip->udata.param[QBG_PARAM_ESSENTIAL_PARAM_REVID].valid) {
		chip->essential_param_revid =
			chip->udata.param[QBG_PARAM_ESSENTIAL_PARAM_REVID].data;

		rc = qbg_sdam_set_essential_param_revid(chip, chip->essential_param_revid);
		if (rc < 0)
			pr_err("Failed to set essential param revid in sdam, rc=%d\n",
				rc);
	}

	rc = qbg_sdam_set_battery_id(chip, chip->batt_id_ohm / 1000);
	if (rc < 0)
		pr_err("Failed to set battid in sdam, rc=%d\n", rc);

	qbg_dbg(chip, QBG_DEBUG_STATUS, "udata update: batt_soc=%d sys_soc=%d soc=%d qbg_esr=%d\n",
		(chip->batt_soc != INT_MIN) ? chip->batt_soc : -EINVAL,
		(chip->sys_soc != INT_MIN) ? chip->sys_soc : -EINVAL,
		chip->soc, chip->esr);
}

static int qbg_decode_fifo_data(struct fifo_data fifo, unsigned int *vbat1,
				unsigned int *vbat2, int *ibat,
				unsigned int *tbat, unsigned int *ibat_t)
{
	unsigned short int acc0_data = 0, acc1_data = 0, acc2_data = 0,
			tbat_data = 0;
	unsigned int ibat_temp;

	if (!vbat1 || !vbat2 || !ibat || !tbat || !ibat_t)
		return -EINVAL;

	acc0_data = fifo.v1;
	acc1_data = fifo.v2;
	acc2_data = fifo.i;
	tbat_data = fifo.tbat;

	ibat_temp = acc2_data;

	*vbat1 = acc0_data * VBATT_1S_LSB;
	*ibat = ((int16_t)ibat_temp) * IBATT_10A_LSB * ICHG_FS_10A;
	*tbat = tbat_data;
	*ibat_t = fifo.ibat;

	return 0;
}

static int get_rtc_time(struct qti_qbg *chip, unsigned long *rtc_time)
{
	struct rtc_time tm;
	int rc;

	if (!chip->rtc) {
		pr_err("Invalid RTC handle\n");
		return -EINVAL;
	}

	rc = rtc_read_time(chip->rtc, &tm);
	if (rc) {
		pr_err("Failed to read rtc time (%s) : %d\n",
				CONFIG_RTC_HCTOSYS_DEVICE, rc);
		goto close_time;
	}

	rc = rtc_valid_tm(&tm);
	if (rc) {
		pr_err("Invalid RTC time (%s): %d\n",
				CONFIG_RTC_HCTOSYS_DEVICE, rc);
		goto close_time;
	}
	rtc_tm_to_time(&tm, rtc_time);

	return rc;

close_time:
	rtc_class_close(chip->rtc);
	chip->rtc = NULL;

	return rc;
}

static int qbg_process_fifo(struct qti_qbg *chip, u32 fifo_count)
{
	struct fifo_data *fifo;
	int rc, i, ibat, ibat_esr;
	unsigned char data_tag;
	unsigned int vbat1, vbat2, tbat, ibat_t, esr;
	unsigned int vbat1_esr, vbat2_esr, tbat_esr, ibat_t_esr;
	unsigned long timestamp;

	if (!fifo_count) {
		qbg_dbg(chip, QBG_DEBUG_SDAM, "No FIFO data\n");
		return -EINVAL;
	}

	fifo = kcalloc(fifo_count, sizeof(*fifo), GFP_KERNEL);
	if (!fifo)
		return -ENOMEM;

	rc = get_rtc_time(chip, &timestamp);
	if (rc < 0) {
		pr_err("Failed to read rtc time, rc=%d\n", rc);
		return rc;
	}

	chip->kdata.param[QBG_PARAM_FIFO_TIMESTAMP].valid = true;
	chip->kdata.param[QBG_PARAM_FIFO_TIMESTAMP].data = timestamp;

	/* Read raw FIFO data from SDAM */
	rc = qbg_sdam_get_fifo_data(chip, fifo, fifo_count);
	if (rc < 0) {
		pr_err("Failed to get QBG FIFO data, rc=%d\n", rc);
		goto ret;
	}

	data_tag = fifo[0].data_tag;
	if (fifo_count > QBG_SDAM_ESR_PULSE_FIFO_INDEX)
		data_tag = fifo[QBG_SDAM_ESR_PULSE_FIFO_INDEX].data_tag;

	if (data_tag == QBG_DATA_TAG_FAST_CHAR) {
		if (fifo_count < QBG_SDAM_ESR_PULSE_FIFO_INDEX) {
			pr_err("Not enough FIFO samples in fast char mode\n");
			goto ret;
		}

		rc = qbg_decode_fifo_data(fifo[QBG_SDAM_ESR_PULSE_FIFO_INDEX - 1],
						&vbat1, &vbat2, &ibat, &tbat,
						&ibat_t);
		if (rc < 0) {
			pr_err("Couldn't decode FIFO before ESR pulse in fast char mode, rc=%d\n",
				rc);
			goto ret;
		}

		rc = qbg_decode_fifo_data(fifo[QBG_SDAM_ESR_PULSE_FIFO_INDEX],
						&vbat1_esr, &vbat2_esr,
						&ibat_esr, &tbat_esr,
						&ibat_t_esr);
		if (rc < 0) {
			pr_err("Couldn't decode FIFO before ESR pulse in fast char mode, rc=%d\n",
				rc);
			goto ret;
		}

		if (ibat != ibat_esr && (vbat1 > vbat1_esr)) {
			esr = (vbat1 - vbat1_esr) / ((ibat_esr - ibat) * 10000);
			esr *= 10000;
			chip->esr = esr;
			qbg_dbg(chip, QBG_DEBUG_SDAM, "ESR:%u, ibat=%d ibat_esr=%d vbat1:%u vbat1_esr:%u\n",
				esr, ibat, ibat_esr, vbat1, vbat1_esr);
		} else {
			pr_err("Couldn't calculate ESR, ibat=%d ibat_esr=%d vbat1:%u vbat1_esr:%u\n",
				ibat, ibat_esr, vbat1, vbat1_esr);
			goto ret;
		}
	}

	for (i = 0; i < fifo_count; i++) {
		rc = qbg_decode_fifo_data(fifo[i], &vbat1, &vbat2, &ibat, &tbat,
						&ibat_t);
		if (rc < 0) {
			pr_err("Failed to decode fifo %d, rc=%d\n", i, rc);
			goto ret;
		}

		qbg_dbg(chip, QBG_DEBUG_SDAM, "vbat1:%u ibat:%d tbat:%u ibat_t:%u\n",
			vbat1, ibat, tbat, ibat_t);

		chip->kdata.fifo[i].v1 = vbat1;
		chip->kdata.fifo[i].v2 = vbat2;
		chip->kdata.fifo[i].i = ibat;
		chip->kdata.fifo[i].tbat = tbat;
		chip->kdata.fifo[i].ibat = ibat_t;
		chip->kdata.fifo[i].data_tag = fifo[i].data_tag;
	}

ret:
	kfree(fifo);
	return rc;
}

static int qbg_clear_fifo_data(struct qti_qbg *chip)
{
	int rc = 0, i;
	u8 val[2] = {0, 0};

	rc = qbg_sdam_write(chip,
		QBG_SDAM_BASE(chip, SDAM_CTRL0) + QBG_SDAM_FIFO_COUNT_OFFSET,
		val, 2);
	if (rc < 0) {
		pr_err("Failed to clear QBG FIFO Count, rc=%d\n", rc);
		return rc;
	}

	for (i = 0; i < 10; i++) {
		val[0] = 0;
		rc = qbg_sdam_write(chip,
			QBG_SDAM_BASE(chip, SDAM_CTRL0) + QBG_SDAM_INT_TEST_VAL,
			val, 1);
		if (rc < 0) {
			pr_err("Failed to SDAM0 test val to 0, rc=%d\n", rc);
			return rc;
		}

		/* Handshake with PBS to access FIFO data */
		rc = qbg_sdam_read(chip,
			QBG_SDAM_BASE(chip, SDAM_CTRL0) + QBG_SDAM_INT_TEST_VAL,
			val, 1);
		if (rc < 0) {
			pr_err("Failed to read QBG SDAM, rc=%d\n", rc);
			return rc;
		}

		if (val[0] == 0)
			break;
	}
	if (i == 10)
		return -EINVAL;

	val[0] = QBG_SDAM_START_OFFSET;
	val[1] = 0x0;
	rc = qbg_sdam_write(chip,
		QBG_SDAM_BASE(chip, SDAM_DATA0) + QBG_SDAM_DATA_PUSH_COUNTER_OFFSET,
		val, 2);
	if (rc < 0) {
		pr_err("Failed to configure QBG data push counter, rc=%d\n", rc);
		return rc;
	}
	val[0] = 0x0;
	rc = qbg_sdam_write(chip,
		QBG_SDAM_BASE(chip, SDAM_CTRL0) + QBG_SDAM_PBS_STATUS_OFFSET,
		val, 1);
	if (rc < 0) {
		pr_err("Failed to set QBG PBS status, rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int qbg_init_sdam(struct qti_qbg *chip)
{
	int rc = 0;
	u8 val[2] = {0};

	val[0] = 0x80;
	rc = qbg_sdam_write(chip,
		QBG_SDAM_BASE(chip, SDAM_CTRL0) + QBG_SDAM_INT_TEST1, val, 1);
	if (rc < 0) {
		pr_err("Failed to write QBG SDAM, rc=%d\n", rc);
		return rc;
	}

	val[0] = 0;
	rc = qbg_sdam_read(chip,
		QBG_SDAM_BASE(chip, SDAM_CTRL0) +  QBG_SDAM_INT_TEST_VAL,
		val, 1);
	if (rc < 0) {
		pr_err("Faiiled to read QBG SDAM, rc=%d\n", rc);
		return rc;
	}

	if (val[0] == QBG_SDAM_TEST_VAL_1_SET) {
		rc = qbg_clear_fifo_data(chip);
		if (rc < 0) {
			pr_err("Failed to clear QBG FIFO data, rc=%d\n", rc);
			return rc;
		}
	}

	return rc;
}

static int qbg_force_fast_char(struct qti_qbg *chip, bool force)
{
	int rc = 0;
	u8 val = force ? (1 << FORCE_FAST_CHAR_SHIFT) : 0;

	rc = qbg_write(chip, QBG_MAIN_QBG_STATE_FORCE_CMD, &val, 1);
	if (rc < 0)
		pr_err("Failed to %s QBG fast char mode, rc=%d\n",
			force ? "enable" : "disable", rc);

	return rc;
}

static int qbg_handle_fast_char(struct qti_qbg *chip)
{
	int rc = 0;
	ktime_t now;

	if (chip->in_fast_char) {
		rc = qbg_force_fast_char(chip, false);
		if (rc < 0) {
			pr_err("Failed to get out of fast char mode, rc=%d\n",
				rc);
			return rc;
		}
		chip->in_fast_char = false;
	} else if (chip->charge_type == POWER_SUPPLY_CHARGE_TYPE_ADAPTIVE) {
		now = ktime_get();
		if ((ktime_ms_delta(now, chip->last_fast_char_time) >
			QBG_FAST_CHAR_DELTA_MS) && !chip->in_fast_char) {

			rc = qbg_force_fast_char(chip, true);
			if (rc < 0) {
				pr_err("Failed to put QBG to fast char mode, rc=%d\n",
					rc);
				return rc;
			}
			chip->in_fast_char = true;
			chip->last_fast_char_time = now;
		}
	}

	return rc;
}

static irqreturn_t qbg_data_full_irq_handler(int irq, void *_chip)
{
	struct qti_qbg *chip = _chip;
	int rc;
	u32 fifo_count = 0;

	qbg_dbg(chip, QBG_DEBUG_IRQ, "DATA FULL IRQ triggered\n");

	rc = qbg_handle_fast_char(chip);
	if (rc < 0) {
		pr_err("Failed to handle QBG fast char, rc=%d\n", rc);
		return IRQ_HANDLED;
	}

	mutex_lock(&chip->fifo_lock);
	memset(&chip->kdata.fifo, 0, sizeof(struct k_fifo_data));

	rc = qbg_get_fifo_count(chip, &fifo_count);
	if (rc < 0) {
		pr_err("Failed to get QBG FIFO length, rc=%d\n", rc);
		goto done;
	}

	rc = qbg_process_fifo(chip, fifo_count);
	if (rc < 0) {
		pr_err("Failed to process QBG FIFO, rc=%d\n", rc);
		goto done;
	}
	chip->kdata.fifo_count = fifo_count;

	rc = qbg_init_sdam(chip);
	if (rc < 0) {
		pr_err("Failed to initialize QBG sdam, rc=%d\n", rc);
		goto done;
	}

	chip->data_ready = true;
	/* Wakeup the read thread */
	wake_up_interruptible(&chip->qbg_wait_q);
done:
	mutex_unlock(&chip->fifo_lock);
	return IRQ_HANDLED;
}

static int qbg_notifier_cb(struct notifier_block *nb,
			unsigned long event, void *data)
{
	struct power_supply *psy = data;
	struct qti_qbg *chip = container_of(nb, struct qti_qbg, nb);

	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if (work_pending(&chip->status_change_work))
		return NOTIFY_OK;

	if ((strcmp(psy->desc->name, "battery") == 0) ||
	    (strcmp(psy->desc->name, "usb") == 0) ||
	    (strcmp(psy->desc->name, "dc") == 0)) {
		schedule_work(&chip->status_change_work);
	}

	return NOTIFY_OK;
}

static int qbg_read_range_data_from_node(struct device_node *node,
		const char *prop_str, struct ranges *ranges,
		int max_threshold, u32 max_value)
{
	int rc = 0, i, length, per_tuple_length, tuples;

	if (!node || !prop_str || !ranges) {
		pr_err("Invalid parameters passed\n");
		return -EINVAL;
	}

	rc = of_property_count_elems_of_size(node, prop_str, sizeof(u32));
	if (rc < 0) {
		pr_err("Count %s failed, rc=%d\n", prop_str, rc);
		return rc;
	}

	length = rc;
	per_tuple_length = sizeof(struct range_data) / sizeof(u32);
	if (length % per_tuple_length) {
		pr_err("%s length (%d) should be multiple of %d\n",
				prop_str, length, per_tuple_length);
		return -EINVAL;
	}

	tuples = length / per_tuple_length;
	if (tuples > QBG_MAX_STEP_CHG_ENTRIES) {
		pr_err("too many entries(%d), only %d allowed\n",
				tuples, QBG_MAX_STEP_CHG_ENTRIES);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(node, prop_str, (u32 *)ranges->data, length);
	if (rc < 0) {
		pr_err("Read %s failed, rc=%d\n", prop_str, rc);
		return rc;
	}

	for (i = 0; i < tuples; i++) {
		if (ranges->data[i].low_threshold >
				ranges->data[i].high_threshold) {
			pr_err("%s thresholds should be in ascendant ranges data\n",
						prop_str);
			rc = -EINVAL;
			goto clean;
		}

		if (i != 0) {
			if (ranges->data[i - 1].high_threshold >
					ranges->data[i].low_threshold) {
				pr_err("%s thresholds should be in ascendant ranges data\n",
							prop_str);
				rc = -EINVAL;
				goto clean;
			}
		}

		if (ranges->data[i].low_threshold > max_threshold)
			ranges->data[i].low_threshold = max_threshold;
		if (ranges->data[i].high_threshold > max_threshold)
			ranges->data[i].high_threshold = max_threshold;
		if (ranges->data[i].value > max_value)
			ranges->data[i].value = max_value;
	}

	ranges->range_count = tuples;
	ranges->valid = true;
	return rc;
clean:
	memset(ranges, 0, tuples * sizeof(struct range_data));
	return rc;
}

#define QBG_DEFAULT_JEITA_FULL_FV_10NV		420000000
#define QBG_DEFAULT_BATTERY_BETA		4250
#define QBG_DEFAULT_BATTERY_THERM_KOHM		100
#define QBG_DEFAULT_JEITA_WARM_ADC_DATA		0x25e3 /* WARM = 40 DegC */
#define QBG_DEFAULT_JEITA_COOL_ADC_DATA		0x5314 /* COOL = 5 DegC */
static int parse_step_chg_jeita_params(struct qti_qbg *chip, struct device_node *profile_node)
{
	struct qbg_step_chg_jeita_params *step_chg_jeita;
	bool soc_based_step_chg = false;
	bool ocv_based_step_chg = false;
	int rc;
	int i = 0;
	u32 temp[2];
	bool jeita_thresh_valid = true;

	step_chg_jeita = devm_kzalloc(chip->dev, sizeof(*step_chg_jeita),
				GFP_KERNEL);
	if (!step_chg_jeita)
		return -ENOMEM;

	/* choose the lower of the two soft threaholds as the jeita-full-fv */
	step_chg_jeita->jeita_full_fv_10nv = QBG_DEFAULT_JEITA_FULL_FV_10NV;
	rc = of_property_read_u32_array(profile_node, "qcom,jeita-soft-fv-uv",
				temp, 2);
	if (!rc)
		step_chg_jeita->jeita_full_fv_10nv = min(temp[0], temp[1]) * MICRO_TO_10NANO;
	else
		pr_debug("Failed to read jeita full fv from battery profile, rc=%d\n", rc);

	step_chg_jeita->jeita_full_iterm_10na = chip->iterm_ma * MILLI_TO_10NANO;

	soc_based_step_chg = of_property_read_bool(profile_node, "qcom,soc-based-step-chg");
	ocv_based_step_chg = of_property_read_bool(profile_node, "qcom,ocv-based-step-chg");
	if (soc_based_step_chg)
		step_chg_jeita->ttf_calc_mode = TTF_MODE_SOC_STEP_CHG;
	else if (ocv_based_step_chg)
		step_chg_jeita->ttf_calc_mode = TTF_MODE_OCV_STEP_CHG;

	step_chg_jeita->battery_beta = QBG_DEFAULT_BATTERY_BETA;
	rc = of_property_read_u32(profile_node, "qcom,battery-beta",
				&step_chg_jeita->battery_beta);
	if (rc < 0)
		pr_debug("Failed to read battery beta form battery profile, rc=%d\n", rc);

	step_chg_jeita->battery_therm_kohm = QBG_DEFAULT_BATTERY_THERM_KOHM;
	rc = of_property_read_u32(profile_node, "qcom,battery-therm-kohm",
				&step_chg_jeita->battery_therm_kohm);
	if (rc < 0)
		pr_debug("Failed to read battery therm from battery profile, rc=%d\n", rc);

	step_chg_jeita->jeita_cool_adc_value = QBG_DEFAULT_JEITA_COOL_ADC_DATA;
	step_chg_jeita->jeita_warm_adc_value = QBG_DEFAULT_JEITA_WARM_ADC_DATA;
	rc = of_property_read_u32_array(profile_node, "qcom,jeita-soft-thresholds",
				temp, 2);
	if (!rc) {
		step_chg_jeita->jeita_cool_adc_value = temp[0];
		step_chg_jeita->jeita_warm_adc_value = temp[1];
	} else {
		jeita_thresh_valid = false;
		pr_debug("Failed to read jeita cool and warm value from battery profile, rc=%d\n",
				 rc);
	}

	rc = qbg_read_range_data_from_node(profile_node,
			"qcom,jeita-fcc-ranges",
			&step_chg_jeita->jeita_fcc_cfg,
			BATT_HOT_DECIDEGREE_MAX,
			chip->fastchg_curr_ma * MILLI_TO_MICRO);
	if (rc < 0)
		pr_debug("Failed to read qcom,jeita-fcc-ranges from battery profile, rc=%d\n",
					rc);

	rc = qbg_read_range_data_from_node(profile_node,
			"qcom,jeita-fv-ranges",
			&step_chg_jeita->jeita_fv_cfg,
			BATT_HOT_DECIDEGREE_MAX, chip->float_volt_uv);
	if (rc < 0)
		pr_debug("Failed to read qcom,jeita-fv-ranges from battery profile, rc=%d\n",
					rc);

	if ((step_chg_jeita->jeita_fcc_cfg.valid != step_chg_jeita->jeita_fv_cfg.valid)
		|| (jeita_thresh_valid != step_chg_jeita->jeita_fcc_cfg.valid)) {
		pr_err("battery JEITA configuration is not valid\n");
		return -EINVAL;
	}

	rc = qbg_read_range_data_from_node(profile_node,
			"qcom,step-chg-ranges",
			&step_chg_jeita->step_fcc_cfg,
			soc_based_step_chg ? 100 : chip->float_volt_uv,
			chip->fastchg_curr_ma * MILLI_TO_MICRO);
	if (rc < 0)
		pr_debug("Failed to read qcom,step-chg-ranges from battery profile, rc=%d\n",
					rc);

	qbg_dbg(chip, QBG_DEBUG_PROFILE, "jeita_full_fv = %dnv, jeita_full_iterm = %dna, jeita_warm_adc = 0x%x, jeita_cool_adc = 0x%x, battery_beta = %d, battery_therm = %dkohm\n",
			step_chg_jeita->jeita_full_fv_10nv,
			step_chg_jeita->jeita_full_iterm_10na,
			step_chg_jeita->jeita_warm_adc_value,
			step_chg_jeita->jeita_cool_adc_value,
			step_chg_jeita->battery_beta,
			step_chg_jeita->battery_therm_kohm);

	pr_debug("step-chg-ranges:\n");
	for (i = 0; i < step_chg_jeita->step_fcc_cfg.range_count; i++) {
		pr_debug("%d %d %d\n",
			step_chg_jeita->step_fcc_cfg.data[i].low_threshold,
			step_chg_jeita->step_fcc_cfg.data[i].high_threshold,
			step_chg_jeita->step_fcc_cfg.data[i].value);
	}

	pr_debug("jeita-fcc-ranges:\n");
	for (i = 0; i < step_chg_jeita->jeita_fcc_cfg.range_count; i++) {
		pr_debug("%d %d %d\n",
			step_chg_jeita->jeita_fcc_cfg.data[i].low_threshold,
			step_chg_jeita->jeita_fcc_cfg.data[i].high_threshold,
			step_chg_jeita->jeita_fcc_cfg.data[i].value);
	}

	pr_debug("jeita-fv-ranges:\n");
	for (i = 0; i < step_chg_jeita->jeita_fv_cfg.range_count; i++) {
		pr_debug("%d %d %d\n",
			step_chg_jeita->jeita_fv_cfg.data[i].low_threshold,
			step_chg_jeita->jeita_fv_cfg.data[i].high_threshold,
			step_chg_jeita->jeita_fv_cfg.data[i].value);
	}

	chip->step_chg_jeita_params = step_chg_jeita;

	return 0;
}

static int qbg_load_battery_profile(struct qti_qbg *chip)
{
	struct device_node *node = chip->dev->of_node;
	struct device_node *profile_node;
	int rc;
	u32 temp[2];

	chip->batt_node = of_find_node_by_name(node, "qcom,battery-data");
	if (!chip->batt_node) {
		pr_err("Batterydata not available\n");
		return -ENXIO;
	}

	profile_node = of_batterydata_get_best_profile(chip->batt_node,
			chip->batt_id_ohm / 1000, NULL);
	if (IS_ERR(profile_node)) {
		rc = PTR_ERR(profile_node);
		pr_err("Failed to detect valid QBG battery profile, rc=%d\n",
			rc);
		goto out;
	}

	chip->battery = devm_kzalloc(chip->dev, sizeof(*chip->battery), GFP_KERNEL);
	if (!chip->battery) {
		rc = -ENOMEM;
		goto out;
	}

	rc = qbg_batterydata_init(profile_node, chip->battery);
	if (rc < 0) {
		pr_err("Failed to load batterydata, rc=%d\n", rc);
		chip->profile_loaded = false;
		goto out;
	}

	rc = of_property_read_string(profile_node, "qcom,battery-type",
					&chip->batt_type_str);
	if (rc < 0) {
		pr_err("Failed to get battery name, rc=%d\n", rc);
		goto out;
	}

	rc = of_property_read_u32(profile_node, "qcom,max-voltage-uv",
				&chip->float_volt_uv);
	if (rc < 0) {
		pr_err("Failed to read battery float-voltage, rc:%d\n", rc);
		chip->float_volt_uv = -EINVAL;
		goto out;
	}

	rc = of_property_read_u32(profile_node, "qcom,fastchg-current-ma",
				&chip->fastchg_curr_ma);
	if (rc < 0) {
		pr_err("Failed to read battery fastcharge current, rc:%d\n", rc);
		chip->fastchg_curr_ma = -EINVAL;
		goto out;
	}

	rc = of_property_read_u32_array(profile_node, "qcom,battery-capacity", temp, 2);
	if (rc) {
		pr_err("Failed to read battery nominal capacity, rc=%d\n", rc);
		goto out;
	}
	chip->nominal_capacity = temp[0];

	rc = parse_step_chg_jeita_params(chip, profile_node);
out:
	of_node_put(chip->batt_node);
	return rc;
}


#define DEBUG_BATT_ID_LOW	6000
#define DEBUG_BATT_ID_HIGH	8500
static bool is_debug_batt_id(struct qti_qbg *chip)
{
	if (is_between(DEBUG_BATT_ID_LOW, DEBUG_BATT_ID_HIGH,
					chip->batt_id_ohm))
		return true;

	return false;
}

#define DEFAULT_BATT_TYPE	"Unknown Battery"
#define MISSING_BATT_TYPE	"Missing Battery"
#define DEBUG_BATT_TYPE		"Debug Board"
static const char *qbg_get_battery_type(struct qti_qbg *chip)
{
	if (chip->battery_missing)
		return MISSING_BATT_TYPE;

	if (is_debug_batt_id(chip))
		return DEBUG_BATT_TYPE;

	if (chip->batt_type_str && chip->profile_loaded)
		return chip->batt_type_str;

	return DEFAULT_BATT_TYPE;
}

#define DEBUG_BATT_SOC		67
#define BATT_MISSING_SOC	50
static int qbg_get_battery_capacity(struct qti_qbg *chip, int *soc)
{
	if (is_debug_batt_id(chip)) {
		*soc = DEBUG_BATT_SOC;
		return 0;
	}

	if (chip->battery_missing || !chip->profile_loaded) {
		*soc = BATT_MISSING_SOC;
		return 0;
	}

	*soc = chip->soc;

	return 0;
}

#define TEN_NANO_TO_MICRO	100
static int qbg_get_battery_voltage(struct qti_qbg *chip, int *vbat_uv)
{
	int rc;
	unsigned short acc0_data;
	u8 buf[2];

	if (chip->battery_missing) {
		*vbat_uv = 3700000;
		return 0;
	}

	rc = qbg_read(chip, QBG_MAIN_LAST_BURST_AVG_ACC0_DATA0, buf, 2);
	if (rc < 0) {
		pr_err("Failed to read average vbatt, rc=%d\n", rc);
		return rc;
	}

	acc0_data = buf[0] | (buf[1] << 8);
	*vbat_uv = acc0_data * VBATT_1S_LSB;
	*vbat_uv = *vbat_uv / TEN_NANO_TO_MICRO;

	return 0;
}

static int qbg_get_battery_current(struct qti_qbg *chip, int *ibat_ua)
{
	int rc;
	unsigned short acc2_data;
	u8 buf[2];

	if (chip->battery_missing) {
		*ibat_ua = 0;
		return 0;
	}

	rc = qbg_read(chip, QBG_MAIN_LAST_BURST_AVG_ACC2_DATA0, buf, 2);
	if (rc < 0) {
		pr_err("Failed to read average ibatt, rc=%d\n", rc);
		return rc;
	}

	acc2_data = buf[0] | (buf[1] << 8);
	*ibat_ua = ((int16_t)acc2_data) * IBATT_10A_LSB * ICHG_FS_10A;
	*ibat_ua = *ibat_ua / TEN_NANO_TO_MICRO;

	return 0;
}

#define TENTH_FACTOR	10
static int qbg_get_charge_counter(struct qti_qbg *chip, int *charge_count)
{

	if (is_debug_batt_id(chip) || chip->battery_missing) {
		*charge_count = -EINVAL;
		return 0;
	}

	*charge_count = chip->learned_capacity * chip->soc / TENTH_FACTOR;
	return 0;
}

static bool is_battery_present(struct qti_qbg *chip)
{
	bool present = false;
	u8 reg;
	int rc;

	rc = qbg_read(chip, QBG_MAIN_STATUS2, &reg, 1);
	if (rc < 0)
		pr_err("Failed to read battery presence, rc=%d\n", rc);
	else
		present = !(reg & QBG_BATTERY_MISSING_BIT);

	return present;
}

static int get_batt_id_ohm(struct qti_qbg *chip, u32 *batt_id_ohm)
{
	int rc, batt_id;

	if (!chip->batt_id_chan)
		return -EINVAL;

	/* Read battery-id */
	rc = iio_read_channel_processed(chip->batt_id_chan, &batt_id);
	if (rc < 0) {
		pr_err("Failed to read BATT_ID over ADC, rc=%d\n", rc);
		return rc;
	}

	*batt_id_ohm = (u32)batt_id;

	qbg_dbg(chip, QBG_DEBUG_PROFILE, "batt_id_ohm=%u\n", *batt_id_ohm);

	return 0;
}

static int qbg_setup_battery(struct qti_qbg *chip)
{
	int rc = 0;
	u8 ocv_boost_val = 0x02;

	chip->profile_loaded = false;

	if (!is_battery_present(chip)) {
		qbg_dbg(chip, QBG_DEBUG_PROFILE, "Battery Missing!\n");
		chip->battery_missing = true;
	} else {
		/* battery present */
		rc = get_batt_id_ohm(chip, &chip->batt_id_ohm);
		if (rc < 0) {
			pr_err("Failed to detect batt_id rc=%d\n", rc);
		} else {
			rc = qbg_load_battery_profile(chip);
			if (rc < 0)
				pr_err("Failed to load battery-profile, rc=%d\n", rc);
			else
				chip->profile_loaded = true;
		}

		/* Update OCV boost headroom compensation value to 4.3V in debug battery case */
		if (is_debug_batt_id(chip)) {
			rc = qbg_sdam_write(chip,
				QBG_SDAM_BASE(chip, SDAM_CTRL0) +
				QBG_SDAM_BHARGER_OCV_HDRM_OFFSET, &ocv_boost_val, 1);
		}
	}

	qbg_dbg(chip, QBG_DEBUG_PROFILE, "battery_missing=%s batt_id=%d Ohm profile_loaded=%d battery_type=%s\n",
		chip->battery_missing ? "true" : "false", chip->batt_id_ohm,
		chip->profile_loaded, chip->batt_type_str);

	return rc;
}

static int qbg_get_pon_reading(struct qti_qbg *chip, unsigned int *ocv,
				unsigned int *ibat)
{
	int rc;
	unsigned short int ocv0_data, ibat_data;
	u8 acc_data[6];

	rc = qbg_read(chip, QBG_MAIN_PON_OCV_ACC0_DATA0, (u8 *)acc_data, 6);
	if (rc < 0) {
		pr_err("Failed to read PON accumulator data, rc=%d\n", rc);
		return rc;
	}

	ocv0_data = (acc_data[1] << 8) | acc_data[0];
	ibat_data = (acc_data[5] << 8) | acc_data[4];

	*ocv = ocv0_data * VBATT_1S_LSB;
	if (*ocv < VBAT_0PCT_OCV)
		*ocv = VBAT_0PCT_OCV;

	*ibat = ibat_data * IBATT_10A_LSB * ICHG_FS_10A;
	qbg_dbg(chip, QBG_DEBUG_PON, "ocv:%u ibat:%u\n", ocv, ibat);

	return 0;
}

static int qbg_determine_pon_soc(struct qti_qbg *chip)
{
	int rc, batt_temp;
	union power_supply_propval prop = {0, };
	unsigned long rtc_sec, time_diff = 0;
	unsigned int pon_ocv;

	/* Get RTC time here */
	rc = get_rtc_time(chip, &rtc_sec);
	if (rc < 0) {
		pr_err("Failed to read rtc time, rc=%d\n", rc);
		return rc;
	}

	if (!chip->profile_loaded) {
		qbg_dbg(chip, QBG_DEBUG_SOC, "No Profile, skipping PON soc\n");
		return 0;
	}

	rc = qbg_sdam_get_essential_params(chip, (u8 *)&chip->essential_params);
	if (rc < 0) {
		pr_err("Failed to read essential params, rc=%d\n", rc);
		return rc;
	}

	rc = qbg_get_pon_reading(chip, &chip->pon_ocv, &chip->pon_ibat);
	if (rc < 0) {
		pr_err("Failed to get QBG PON reading, rc=%d\n", rc);
		return rc;
	}

	/* Read battery-temp */
	rc = iio_read_channel_processed(chip->batt_temp_chan, &batt_temp);
	if (rc < 0) {
		pr_err("Failed to read BATT_TEMP over ADC, rc=%d\n", rc);
		return rc;
	}
	chip->pon_tbat = (unsigned int)batt_temp;

	if (is_batt_available(chip)) {
		rc = power_supply_get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_STATUS, &prop);
		if (rc < 0)
			pr_err("Failed to get charger status, rc=%d\n", rc);
	}

	pon_ocv = chip->pon_ocv / 10000;
	rc = qbg_lookup_soc_ocv(chip->battery, &chip->pon_soc, pon_ocv,
				prop.intval == POWER_SUPPLY_STATUS_CHARGING);
	if (rc < 0) {
		pr_err("Failed to lookup PON SOC, rc=%d\n", rc);
		return rc;
	}
	chip->pon_soc = DIV_ROUND_CLOSEST(chip->pon_soc, 100);
	chip->soc = chip->pon_soc;

	qbg_dbg(chip, QBG_DEBUG_PON, "Shutdown: SOC=%d OCV=%duV Ibat:%duA timediff=%lusecs, time_now=%lusecs\n",
			chip->pon_soc, chip->pon_ocv, chip->pon_ibat, time_diff, rtc_sec);

	return 0;
}

static int qbg_psy_get_prop(struct power_supply *psy,
			enum power_supply_property psp,
			union power_supply_propval *pval)
{
	if (psp == POWER_SUPPLY_PROP_TYPE)
		pval->intval = POWER_SUPPLY_TYPE_MAINS;

	return 0;
}

static enum power_supply_property qbg_psy_props[] = {
	POWER_SUPPLY_PROP_TYPE,
};

static struct power_supply_desc qbg_psy_desc = {
	.name			= "bms",
	.type			= POWER_SUPPLY_TYPE_MAINS,
	.properties		= qbg_psy_props,
	.num_properties		= ARRAY_SIZE(qbg_psy_props),
	.get_property		= qbg_psy_get_prop,
};

static int qbg_init_psy(struct qti_qbg *chip)
{
	struct power_supply_config qbg_psy_cfg = {};
	int rc;

	qbg_psy_cfg.drv_data = chip;
	chip->qbg_psy = devm_power_supply_register(chip->dev, &qbg_psy_desc,
						&qbg_psy_cfg);
	if (IS_ERR_OR_NULL(chip->qbg_psy)) {
		pr_err("Failed to register qbg_psy, rc = %d\n",
				PTR_ERR(chip->qbg_psy));
		return PTR_ERR(chip->qbg_psy);
	}

	chip->nb.notifier_call = qbg_notifier_cb;
	rc = power_supply_reg_notifier(&chip->nb);
	if (rc < 0)
		pr_err("Failed to register psy notifier rc = %d\n", rc);

	return rc;
}

static int qbg_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	return 0;
}

static int qbg_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct qti_qbg *chip = iio_priv(indio_dev);
	int rc = 0;

	*val1 = 0;

	switch (chan->channel) {
	case PSY_IIO_DEBUG_BATTERY:
		*val1 = is_debug_batt_id(chip);
		break;
	case PSY_IIO_CAPACITY:
		rc = qbg_get_battery_capacity(chip, val1);
		break;
	case PSY_IIO_REAL_CAPACITY:
		rc = qbg_get_battery_capacity(chip, val1);
		break;
	case PSY_IIO_TEMP:
		*val1 = chip->tbat;
		break;
	case PSY_IIO_VOLTAGE_MAX:
		*val1 = chip->float_volt_uv;
		break;
	case PSY_IIO_VOLTAGE_OCV:
		*val1 = chip->ocv_uv;
		break;
	case PSY_IIO_VOLTAGE_AVG:
		rc = qbg_get_battery_voltage(chip, val1);
		break;
	case PSY_IIO_VOLTAGE_NOW:
		rc = qbg_get_battery_voltage(chip, val1);
		break;
	case PSY_IIO_CURRENT_NOW:
		rc = qbg_get_battery_current(chip, val1);
		break;
	case PSY_IIO_RESISTANCE_ID:
		*val1 = chip->batt_id_ohm;
		break;
	case PSY_IIO_TIME_TO_FULL_AVG:
		*val1 = chip->ttf;
		break;
	case PSY_IIO_TIME_TO_FULL_NOW:
		*val1 = chip->ttf;
		break;
	case PSY_IIO_TIME_TO_EMPTY_AVG:
		*val1 = chip->tte;
		break;
	case PSY_IIO_ESR_ACTUAL:
		*val1 = chip->esr;
		break;
	case PSY_IIO_SOH:
		*val1 = chip->soh;
		break;
	case PSY_IIO_CHARGE_COUNTER:
		rc = qbg_get_charge_counter(chip, val1);
		break;
	case PSY_IIO_CYCLE_COUNT:
		*val1 = chip->charge_cycle_count;
		break;
	case PSY_IIO_CHARGE_FULL:
		*val1 = chip->learned_capacity;
		break;
	case PSY_IIO_CHARGE_FULL_DESIGN:
		*val1 = chip->nominal_capacity;
		break;
	default:
		pr_err_ratelimited("Unsupported property %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	return (rc < 0) ? rc : IIO_VAL_INT;
}

static int qbg_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct qti_qbg *chip = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = chip->iio_chan;
	int i;

	for (i = 0; i < ARRAY_SIZE(qbg_iio_psy_channels); i++, iio_chan++) {
		if (iio_chan->channel == iiospec->args[0])
			return i;
	}

	return -EINVAL;
}

static const struct iio_info qbg_iio_info = {
	.read_raw	= qbg_iio_read_raw,
	.write_raw	= qbg_iio_write_raw,
	.of_xlate	= qbg_iio_of_xlate,
};

static int qbg_init_iio(struct qti_qbg *chip,
			struct platform_device *pdev)
{
	struct iio_dev *indio_dev = chip->indio_dev;
	struct iio_chan_spec *chan;
	int qbg_num_iio_channels = ARRAY_SIZE(qbg_iio_psy_channels);
	int rc, i;

	chip->iio_chan = devm_kcalloc(chip->dev, qbg_num_iio_channels,
				sizeof(*chip->iio_chan), GFP_KERNEL);
	if (!chip->iio_chan)
		return -ENOMEM;

	indio_dev->info = &qbg_iio_info;
	indio_dev->dev.parent = chip->dev;
	indio_dev->dev.of_node = chip->dev->of_node;
	indio_dev->name = pdev->name;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = chip->iio_chan;
	indio_dev->num_channels = qbg_num_iio_channels;

	for (i = 0; i < qbg_num_iio_channels; i++) {
		chan = &chip->iio_chan[i];
		chan->address = i;
		chan->channel = qbg_iio_psy_channels[i].channel_num;
		chan->type = qbg_iio_psy_channels[i].type;
		chan->datasheet_name =
			qbg_iio_psy_channels[i].datasheet_name;
		chan->extend_name =
			qbg_iio_psy_channels[i].datasheet_name;
		chan->info_mask_separate =
			qbg_iio_psy_channels[i].info_mask;
	}

	rc = devm_iio_device_register(chip->dev, indio_dev);
	if (rc < 0)
		pr_err("Failed to register QBG IIO device, rc=%d\n", rc);

	return rc;
}

static int qbg_get_accumulator_properties(struct qti_qbg *chip,
				enum QBG_STATE state,
				enum QBG_SAMPLE_NUM_TYPE *num_of_accum,
				enum QBG_ACCUM_INTERVAL_TYPE *accum_interval)
{
	int rc = 0;
	unsigned int reg = QBG_MAIN_LPM_MEAS_CTL2;
	unsigned char data = 0;

	if (state >= QBG_STATE_MAX || !num_of_accum || !accum_interval)
		return -EINVAL;

	switch (state) {
	case QBG_LPM:
		reg = QBG_MAIN_LPM_MEAS_CTL2;
		break;
	case QBG_MPM:
		reg = QBG_MAIN_MPM_MEAS_CTL2;
		break;
	case QBG_HPM:
		reg = QBG_MAIN_HPM_MEAS_CTL2;
		break;
	case QBG_FAST_CHAR:
		reg = QBG_MAIN_FAST_CHAR_MEAS_CTL2;
		break;
	default:
		pr_err("Invalid QBG state requested for accumulator properties %u\n",
			state);
		return rc;
	}

	rc = qbg_read(chip, reg, &data, 1);
	if (rc < 0) {
		pr_err("Failed to MEAS_CTL2 %u, rc=%d\n", reg, rc);
		return rc;
	}

	if (state == QBG_FAST_CHAR)
		*num_of_accum = 0;
	else
		*num_of_accum = (data & QBG_NUM_OF_ACCUM_MASK) >> QBG_NUM_OF_ACCUM_SHIFT;

	*accum_interval = data & QBG_ACCUM_INTERVAL_MASK;

	qbg_dbg(chip, QBG_DEBUG_DEVICE, "state:%u num_of_accum:%u accum_interval:%u\n",
			state, *num_of_accum, *accum_interval);

	return rc;
}

static int qbg_get_sample_time_us(struct qti_qbg *chip)
{
	int rc = 0, index;
	unsigned int interval;
	enum QBG_ACCUM_INTERVAL_TYPE accum_interval = ACCUM_INTERVAL_100MS;
	enum QBG_SAMPLE_NUM_TYPE num_accum = SAMPLE_NUM_1;

	for (index = 0; index < QBG_STATE_MAX; index++) {
		if (index == QBG_PON_OCV)
			continue;

		rc = qbg_get_accumulator_properties(chip, index, &num_accum,
					&accum_interval);
		if (rc < 0)
			return rc;

		if (index == QBG_FAST_CHAR)
			interval = qbg_fast_char_avg_interval[accum_interval];
		else if (index == QBG_LPM)
			interval = qbg_lpm_accum_interval[accum_interval];
		else
			interval = qbg_accum_interval[accum_interval];

		chip->sample_time_us[index] = interval;
	}

	return rc;
}

static ssize_t qbg_device_read(struct file *file, char __user *buf, size_t count,
			  loff_t *ppos)
{
	int rc;
	struct qti_qbg *chip = file->private_data;
	unsigned long data_size = sizeof(chip->kdata);

	if (count < data_size) {
		pr_err("Invalid datasize %lu, expected lesser then %zu\n",
							data_size, count);
		return -EINVAL;
	}

	/* non-blocking access, return */
	if (!chip->data_ready && (file->f_flags & O_NONBLOCK))
		return 0;

	/* blocking access wait on data_ready */
	if (!(file->f_flags & O_NONBLOCK)) {
		rc = wait_event_interruptible(chip->qbg_wait_q,
					chip->data_ready);
		if (rc < 0) {
			pr_debug("Failed wait! rc=%d\n", rc);
			return rc;
		}
	}

	mutex_lock(&chip->data_lock);
	if (!chip->data_ready) {
		pr_debug("No Data, false wakeup\n");
		rc = -ENODATA;
		goto fail_read;
	}

	if (copy_to_user(buf, &chip->kdata, data_size)) {
		pr_err("Failed to copy_to_user\n");
		rc = -EFAULT;
		goto fail_read;
	}
	chip->data_ready = false;

	qbg_dbg(chip, QBG_DEBUG_DEVICE, "QBG device read complete Size=%ld\n",
		data_size);

	/* clear data */
	memset(&chip->kdata, 0, sizeof(chip->kdata));

	rc = data_size;

fail_read:
	mutex_unlock(&chip->data_lock);
	return rc;
}

static ssize_t qbg_device_write(struct file *file, const char __user *buf,
			size_t count, loff_t *ppos)
{
	int rc = -EINVAL;
	struct qti_qbg *chip = file->private_data;
	unsigned long data_size = sizeof(chip->udata);

	mutex_lock(&chip->data_lock);
	if (count == 0) {
		pr_err("No data!\n");
		rc = -ENODATA;
		goto fail;
	}

	if (count < data_size) {
		pr_err("Invalid datasize %zu expected %lu\n", count, data_size);
		goto fail;
	}

	if (copy_from_user(&chip->udata, buf, data_size)) {
		pr_err("Failed to copy_from_user\n");
		rc = -EFAULT;
		goto fail;
	}

	rc = data_size;
	schedule_work(&chip->udata_work);

	qbg_dbg(chip, QBG_DEBUG_DEVICE, "QBG write complete size=%d\n", rc);
fail:
	mutex_unlock(&chip->data_lock);
	return rc;
}

static unsigned int qbg_device_poll(struct file *file, poll_table *wait)
{
	struct qti_qbg *chip = file->private_data;
	unsigned int mask = 0;

	poll_wait(file, &chip->qbg_wait_q, wait);

	if (chip->data_ready)
		mask = POLLIN | POLLRDNORM;

	return mask;
}

static int qbg_device_open(struct inode *inode, struct file *file)
{
	struct qti_qbg *chip = container_of(inode->i_cdev,
				struct qti_qbg, qbg_cdev);

	file->private_data = chip;

	qbg_dbg(chip, QBG_DEBUG_DEVICE, "QBG device opened!\n");

	return 0;
}

static int qbg_device_release(struct inode *inode, struct file *file)
{
	struct qti_qbg *chip = container_of(inode->i_cdev,
				struct qti_qbg, qbg_cdev);

	file->private_data = NULL;

	qbg_dbg(chip, QBG_DEBUG_DEVICE, "QBG device closed!\n");

	return 0;
}

static long qbg_device_ioctl(struct file *file, unsigned int cmd,
						unsigned long arg)
{
	struct qti_qbg *chip = file->private_data;
	struct qbg_config __user *config_user;
	struct qbg_config config;
	struct qbg_essential_params __user *params_user;
	struct qbg_step_chg_jeita_params __user *step_chg_params_user;
	unsigned long rtc_sec;
	int rc = 0, i;

	if (!chip) {
		pr_err("Device private data not set!\n");
		return -EINVAL;
	}

	if (!arg) {
		pr_err("Invalid user pointer\n");
		return -EINVAL;
	}

	/* Get RTC time here */
	rc = get_rtc_time(chip, &rtc_sec);
	if (rc < 0) {
		pr_err("Failed to read rtc time, rc=%d\n", rc);
		return rc;
	}

	switch (cmd) {
	case QBGIOCXCFG:
		config_user = (struct qbg_config __user *)arg;

		rc = qbg_get_sample_time_us(chip);
		if (rc < 0) {
			pr_err("Failed to calculate sample time us, rc=%d\n", rc);
			return rc;
		}

		rc = qbg_sdam_get_battery_id(chip, &chip->sdam_batt_id);
		if (rc < 0) {
			pr_err("Failed to get battid from sdam, rc=%d\n", rc);
			return rc;
		}

		rc = qbg_sdam_get_essential_param_revid(chip,
					(u8 *)&chip->essential_param_revid);
		if (rc < 0) {
			pr_err("Failed to get essential param revid, rc=%d\n",
				rc);
			return rc;
		}

		config.current_time = rtc_sec;
		config.batt_id = chip->batt_id_ohm / 1000;
		config.pon_ocv = chip->pon_ocv;
		config.pon_ibat = chip->pon_ibat;
		config.pon_tbat = chip->pon_tbat;
		config.pon_soc = chip->pon_soc;
		config.float_volt_uv = chip->float_volt_uv;
		config.fastchg_curr_ma = chip->fastchg_curr_ma;
		config.vbat_cutoff_mv = chip->vbat_cutoff_mv;
		config.ibat_cutoff_ma = chip->ibat_cutoff_ma;
		config.vph_min_mv = chip->vph_min_mv;
		config.iterm_ma = chip->iterm_ma;
		config.rconn_mohm = chip->rconn_mohm;
		config.sdam_batt_id = chip->sdam_batt_id;
		config.essential_param_revid = chip->essential_param_revid;
		for (i = 0; i < QBG_STATE_MAX; i++) {
			config.sample_time_us[i] = chip->sample_time_us[i];
			qbg_dbg(chip, QBG_DEBUG_DEVICE, "QBGIOCXCFG: sample_time_us[%d]:%u\n",
					i, config.sample_time_us[i]);
		}

		if (copy_to_user(config_user, (void *)&config, sizeof(config))) {
			pr_err("Failed to copy QBG config to user\n");
			return -EFAULT;
		}

		qbg_dbg(chip, QBG_DEBUG_DEVICE, "QBGIOCXCFG: sdam_battid:%u essential param revid:%u battid:%u pon_ocv:%u pon_ibat:%u pon_soc:%u vbatt_cutoff_mv:%u iterm_ma:%u\n",
				config.sdam_batt_id, config.essential_param_revid,
				config.batt_id, config.pon_ocv,
				config.pon_ibat, config.pon_soc,
				config.vbat_cutoff_mv, config.iterm_ma);

		break;
	case QBGIOCXEPR:
		params_user = (struct qbg_essential_params __user *)arg;
		rc = qbg_sdam_get_essential_params(chip,
				(u8 *)&chip->essential_params);
		if (rc < 0) {
			pr_err("Failed to read essential params, rc=%d\n", rc);
			return -EFAULT;
		}

		if (copy_to_user(params_user, (void *)&chip->essential_params,
				sizeof(chip->essential_params))) {
			pr_err("Failed to copy QBG essential params to user\n");
			return -EFAULT;
		}

		break;
	case QBGIOCXEPW:
		params_user = (struct qbg_essential_params __user *)arg;
		if (copy_from_user((void *)&chip->essential_params, params_user,
					sizeof(chip->essential_params))) {
			pr_err("Failed to copy QBG essential params from user\n");
			return -EFAULT;
		}
		chip->previous_ep_time = rtc_sec;
		chip->essential_params.rtc_time = rtc_sec;

		rc = qbg_sdam_set_essential_params(chip,
					(u8 *)&chip->essential_params);
		if (rc < 0) {
			pr_err("Failed to write essential params, rc=%d\n", rc);
			return -EFAULT;
		}
		qbg_dbg(chip, QBG_DEBUG_SDAM, "Essential params written, time:%lu secs\n",
					rtc_sec);

		break;
	case QBGIOCXSTEPCHGCFG:
		step_chg_params_user = (struct qbg_step_chg_jeita_params __user *)arg;

		if (copy_to_user(step_chg_params_user, (void *)chip->step_chg_jeita_params,
			sizeof(struct qbg_step_chg_jeita_params))) {
			pr_err("Failed to copy QBG step and jeita charge params to user\n");
			return -EFAULT;
		}
		qbg_dbg(chip, QBG_DEBUG_DEVICE, "QBGIOCXSTEPCHGCFG: jeita_full_fv_10nv:%d jeita_warm_adc_value:0x%x jeita_cool_adc_value:0x%x ttf_calc_mode:%u\n",
				chip->step_chg_jeita_params->jeita_full_fv_10nv,
				chip->step_chg_jeita_params->jeita_warm_adc_value,
				chip->step_chg_jeita_params->jeita_cool_adc_value,
				chip->step_chg_jeita_params->ttf_calc_mode);

		break;
	default:
		pr_err("IOCTL %d not supported\n", cmd);
		rc = -EINVAL;
	}

	return rc;
}

static const struct file_operations qbg_fops = {
	.owner		= THIS_MODULE,
	.open		= qbg_device_open,
	.release	= qbg_device_release,
	.read		= qbg_device_read,
	.write		= qbg_device_write,
	.poll		= qbg_device_poll,
	.unlocked_ioctl	= qbg_device_ioctl,
	.compat_ioctl	= qbg_device_ioctl,
};

static int qbg_register_device(struct qti_qbg *chip)
{
	int rc;

	rc = alloc_chrdev_region(&chip->dev_no, 0, 1, "qbg");
	if (rc < 0) {
		pr_err("Failed to allocate chardev rc=%d\n", rc);
		return rc;
	}

	cdev_init(&chip->qbg_cdev, &qbg_fops);
	rc = cdev_add(&chip->qbg_cdev, chip->dev_no, 1);
	if (rc < 0) {
		pr_err("Failed to cdev_add rc=%d\n", rc);
		goto unregister_chrdev;
	}

	chip->qbg_class = class_create(THIS_MODULE, "qbg");
	if (IS_ERR_OR_NULL(chip->qbg_class)) {
		pr_err("Failed to create qbg class\n");
		rc = -EINVAL;
		goto delete_cdev;
	}

	chip->qbg_device = device_create(chip->qbg_class, NULL, chip->dev_no,
					NULL, "qbg");
	if (IS_ERR(chip->qbg_device)) {
		pr_err("Failed to create qbg_device\n");
		rc = -EINVAL;
		goto destroy_class;
	}

	qbg_dbg(chip, QBG_DEBUG_DEVICE, "'/dev/qbg' successfully created\n");

	return 0;

destroy_class:
	class_destroy(chip->qbg_class);
delete_cdev:
	cdev_del(&chip->qbg_cdev);
unregister_chrdev:
	unregister_chrdev_region(chip->dev_no, 1);

	return rc;
}

static int qbg_register_interrupts(struct qti_qbg *chip)
{
	int rc;

	rc = devm_request_threaded_irq(chip->dev, chip->irq, NULL,
			qbg_data_full_irq_handler, IRQF_ONESHOT,
			"qbg-sdam", chip);
	if (rc < 0)
		dev_err(chip->dev, "Failed to request IRQ(qbg-sdam), rc=%d\n",
			rc);

	return rc;
}

static int qbg_parse_sdam_dt(struct qti_qbg *chip, struct device_node *node)
{
	int rc;

	chip->irq = of_irq_get_byname(node, "qbg-sdam");
	if (chip->irq < 0) {
		pr_err("Failed to get irq for QBG, rc=%d\n", chip->irq);
		return -EINVAL;
	}

	rc = of_property_read_u32(node, "qcom,num-data-sdams",
				&chip->num_data_sdams);
	if (rc < 0) {
		pr_err("Failed to read number of DATA SDAMs for QBG, rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,sdam-base", &chip->sdam_base);
	if (rc < 0) {
		pr_err("Failed to read SDAM base address, rc=%d\n", rc);
		return rc;
	}

	return rc;
}

#define QBG_DEFAULT_VBAT_CUTOFF_MV			3100
#define QBG_DEFAULT_IBAT_CUTOFF_MA			150
#define QBG_DEFAULT_VPH_MIN_MV				2700
#define QBG_DEFAULT_ITERM_MA				100
#define QBG_DEFAULT_RCONN_MOHM				0
static int qbg_parse_dt(struct qti_qbg *chip)
{
	struct device_node *node = chip->dev->of_node;
	int rc = 0;
	u32 val;

	rc = qbg_parse_sdam_dt(chip, node);
	if (rc < 0) {
		pr_err("Failed to QBG SDAM DT, rc=%d\n", rc);
		return rc;
	}

	chip->vbat_cutoff_mv = QBG_DEFAULT_VBAT_CUTOFF_MV;
	rc = of_property_read_u32(node, "qcom,vbat-cutoff-mv", &val);
	if (!rc)
		chip->vbat_cutoff_mv = val;

	chip->ibat_cutoff_ma = QBG_DEFAULT_IBAT_CUTOFF_MA;
	rc = of_property_read_u32(node, "qcom,ibat-cutoff-ma", &val);
	if (!rc)
		chip->ibat_cutoff_ma = val;

	chip->vph_min_mv = QBG_DEFAULT_VPH_MIN_MV;
	rc = of_property_read_u32(node, "qcom,vph-min-mv", &val);
	if (!rc)
		chip->vph_min_mv = val;

	chip->iterm_ma = QBG_DEFAULT_ITERM_MA;
	rc = of_property_read_u32(node, "qcom,iterm-ma", &val);
	if (!rc)
		chip->iterm_ma = val;

	chip->rconn_mohm = QBG_DEFAULT_RCONN_MOHM;
	rc = of_property_read_u32(node, "qcom,rconn-mohm", &val);
	if (!rc)
		chip->rconn_mohm = val;

	return 0;
}

static int qti_qbg_probe(struct platform_device *pdev)
{
	struct qti_qbg *chip;
	struct iio_dev *indio_dev;
	int rc;
	u32 val;

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*chip));
	if (!indio_dev)
		return -ENOMEM;

	chip = iio_priv(indio_dev);
	chip->dev = &pdev->dev;
	chip->indio_dev = indio_dev;

	chip->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!chip->regmap) {
		dev_err(&pdev->dev, "Failed to get regmap\n");
		return -EINVAL;
	}

	rc = of_property_read_u32(pdev->dev.of_node, "reg", &val);
	if (rc < 0) {
		pr_err("Failed to get base address for QBG, rc = %d\n", rc);
		return rc;
	}
	chip->base = val;

	INIT_WORK(&chip->status_change_work, status_change_work);
	INIT_WORK(&chip->udata_work, process_udata_work);
	mutex_init(&chip->fifo_lock);
	mutex_init(&chip->data_lock);
	dev_set_drvdata(chip->dev, chip);
	init_waitqueue_head(&chip->qbg_wait_q);

	chip->debug_mask = &qbg_debug_mask;

	chip->soc = INT_MIN;
	chip->batt_soc = INT_MIN;
	chip->sys_soc = INT_MIN;
	chip->esr = INT_MIN;
	chip->ocv_uv = INT_MIN;
	chip->pon_ocv = INT_MIN;
	chip->charge_cycle_count = INT_MIN;
	chip->nominal_capacity = INT_MIN;
	chip->learned_capacity = INT_MIN;
	chip->ttf = INT_MIN;
	chip->tte = INT_MIN;
	chip->soh = INT_MIN;
	chip->tbat = INT_MIN;

	rc = qbg_parse_dt(chip);
	if (rc < 0) {
		dev_err(&pdev->dev, "Failed to parse QBG devicetree, rc=%d\n", rc);
		return rc;
	}

	/* ADC for Battery-ID */
	chip->batt_id_chan = devm_iio_channel_get(&pdev->dev, "batt-id");
	if (IS_ERR(chip->batt_id_chan)) {
		rc = PTR_ERR(chip->batt_id_chan);
		if (rc != -EPROBE_DEFER)
			dev_err(&pdev->dev, "batt-id channel unavailable, rc=%d\n", rc);
		chip->batt_id_chan = NULL;
		return rc;
	}

	/* ADC for Battery-Temp */
	chip->batt_temp_chan = devm_iio_channel_get(&pdev->dev, "batt-temp");
	if (IS_ERR(chip->batt_temp_chan)) {
		rc = PTR_ERR(chip->batt_temp_chan);
		if (rc != -EPROBE_DEFER)
			dev_err(&pdev->dev, "batt-temp channel unavailable, rc=%d\n", rc);
		chip->batt_temp_chan = NULL;
		return rc;
	}

	chip->rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
	if (chip->rtc == NULL)
		return -EPROBE_DEFER;

	rc = qbg_init_sdam(chip);
	if (rc < 0) {
		dev_err(&pdev->dev, "Failed to initialize QBG sdam, rc=%d\n", rc);
		return rc;
	}

	rc = qbg_setup_battery(chip);
	if (rc < 0) {
		dev_err(&pdev->dev, "Failed to setup battery for QBG, rc=%d\n", rc);
		return rc;
	}

	rc = qbg_determine_pon_soc(chip);
	if (rc < 0) {
		dev_err(&pdev->dev, "Failed to determine initial state, rc=%d\n", rc);
		return rc;
	}

	rc = qbg_register_device(chip);
	if (rc < 0) {
		dev_err(&pdev->dev, "Failed to register QBG device, rc=%d\n", rc);
		return rc;
	}

	rc = qbg_init_iio(chip, pdev);
	if (rc < 0) {
		dev_err(&pdev->dev, "Failed to initialize QBG IIO device, rc=%d\n", rc);
		return rc;
	}

	rc = qbg_init_psy(chip);
	if (rc < 0) {
		dev_err(&pdev->dev, "Failed to initialize QBG PSY, rc=%d\n", rc);
		return rc;
	}

	rc = qbg_register_interrupts(chip);
	if (rc < 0) {
		dev_err(&pdev->dev, "Failed to register QBG interrupts, rc=%d\n", rc);
		return rc;
	}

	dev_info(&pdev->dev, "QBG initialized! battery_profile=%s SOC=%d\n",
			qbg_get_battery_type(chip), chip->soc);

	return rc;
}

static int qti_qbg_remove(struct platform_device *pdev)
{
	struct qti_qbg *chip = platform_get_drvdata(pdev);

	if (chip->rtc)
		rtc_class_close(chip->rtc);
	cancel_work_sync(&chip->status_change_work);
	cancel_work_sync(&chip->udata_work);
	mutex_destroy(&chip->fifo_lock);
	mutex_destroy(&chip->data_lock);
	cdev_del(&chip->qbg_cdev);
	unregister_chrdev_region(chip->dev_no, 1);

	return 0;
}

static const struct of_device_id qbg_match_table[] = {
	{ .compatible = "qcom,qbg", },
	{ },
};

static struct platform_driver qti_qbg_driver = {
	.driver = {
		.name = "qti_qbg",
		.of_match_table = qbg_match_table,
	},
	.probe = qti_qbg_probe,
	.remove = qti_qbg_remove,
};
module_platform_driver(qti_qbg_driver);

MODULE_DESCRIPTION("QTI QBG (Qualcomm Battery Gauging) driver");
MODULE_LICENSE("GPL v2");
