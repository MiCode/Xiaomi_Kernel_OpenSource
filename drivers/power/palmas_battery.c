/*
 * Driver for the Palmas Embedded fuel gauge
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Darbha Sriharsha <dsriharsha@nvidia.com>
 * Author: Syed Rafiuddin <srafiuddin@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c/twl.h>
#include <linux/usb/otg.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/mfd/palmas.h>
#include <linux/byteorder/generic.h>
#include <linux/iio/consumer.h>
#include <linux/power/battery-charger-gauge-comm.h>

#define MODULE_NAME			"palmas-battery"
#define FG_SLAVE			1

/* Voltage and Current buffers */
#define AV_SIZE				5
#define MAX_CHAR			0x7F
#define MAX_UNSIGNED_CHAR		0xFF
#define MAX_INT				0x7FFF
#define MAX_UNSIGNED_INT		0xFFFF
#define MAX_INT8			0x7F
#define MAX_UINT8			0xFF

/* GPADC Channels */
#define VBAT_CHANNEL		"vbat_channel"

/* Voltage that that is used for the EDV */
#define EDV_VOLTAGE	 (cell->config->edv->averaging ? \
				cell->av_voltage : \
				cell->voltage)

/* OCV Lookup table */
#define INTERPOLATE_MAX	 1000

#define PALMAS_BATTERY_FULL	100
#define PALMAS_BATTERY_LOW	15


static short av_v[AV_SIZE];
static short av_c[AV_SIZE];

static unsigned short av_v_index;
static unsigned short av_c_index;

static const unsigned int fuelgauge_rate[4] = {1, 4, 16, 64};

static enum power_supply_property palmas_battery_props[] = {
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
};

struct palmas_battery_info {
	struct device *dev;
	struct palmas *palmas;
	struct power_supply battery;
	struct battery_gauge_dev *bg_dev;

	/* Battery Power Supply */
	int battery_voltage_uV;
	int battery_current_uA;
	int battery_current_avg_uA;
	int battery_charge_status;
	int battery_capacity;
	int battery_boot_capacity_mAh;
	int battery_capacity_max;
	int battery_prev_capacity;
	int battery_capacity_debounce;
	int battery_health;
	int battery_soldered;
	int battery_online;
	int battery_timer_n2;
	int battery_timer_n1;
	s32 battery_charge_n1;
	s32 battery_charge_n2;
	int battery_current_avg_interval;
	struct delayed_work battery_current_avg_work;
	int battery_status_interval;
	int *battery_temperature_chart;
	int battery_temperature_chart_size;
	int battery_termperature_tenthC;

	/* Fuelgauge */
	int fuelgauge_mode;
	int cc_offset;
	int current_max_scale;
	int accumulated_charge;
	struct cell_state cell;
	int status;
};

int fuelgauge_mode;
int cc_offset;
int current_max_scale;
int accumulated_charge;
struct cell_state cell;

/* IIR Filter */
short filter(short y, short x, short a)
{
	int l;

	l = (int) a * y;
	l += (int) (256 - a) * x;
	l /= 256;

	return (short) l;
}

/* Returns diviation between 'size' array members */
unsigned short diff_array(short *arr, unsigned char size)
{
	unsigned char i;
	unsigned int diff = 0;

	for (i = 0; i < size-1; i++)
		diff += abs(arr[i] - arr[i+1]);

	if (diff > MAX_UNSIGNED_INT)
		diff = MAX_UNSIGNED_INT;

	return (unsigned short) diff;
}

/* Increments accumulator with top (limit) and bottom (0) limiting */
static short palmas_accumulate(short acc, short limit, short delta_q)
{
	acc += delta_q;

	if (acc < 0)
		return 0;
	if (acc > limit)
		return limit;
	return acc;
}

/* Checks for right conditions for OCV correction */
static bool palmas_can_ocv(struct cell_state *cell)
{
	struct timeval now;
	int tmp;

	do_gettimeofday(&now);
	tmp = now.tv_sec - cell->last_ocv.tv_sec;

	/* Don't do OCV to often */
	if ((tmp < cell->config->ocv->ocv_period) && cell->init)
		return false;

	/* Voltage should be stable */
	if (cell->config->ocv->voltage_diff <= diff_array(av_v, AV_SIZE))
		return false;

	/* Current should be stable */
	if (cell->config->ocv->current_diff <= diff_array(av_c, AV_SIZE))
		return false;

	/* SOC should be out of Flat Zone */
	if ((cell->soc >= cell->config->ocv->flat_zone_low)
		&& (cell->soc <= cell->config->ocv->flat_zone_high))
			return false;

	/* Current should be less then SleepEnterCurrent */
	if (abs(cell->cur) >= cell->config->ocv->sleep_enter_current)
		return false;

	/* Don't allow OCV below EDV1, unless OCVbelowEDV1 is set */
	if (cell->edv1 && !cell->config->ocv_below_edv1)
		return false;

	return true;
}

/*
 * Coulomb Counter (CC) correction routine. This function adjusts SOC,
 * based on the passed capacity, read from the CC.
 */

/* Applies an estimated Electronic Load to the NAC */
static void palmas_el(struct cell_state *cell)
{
	int el_delta;


	/* No EL correction, if changer is connected */
/*	if (*cell->charge_status == POWER_SUPPLY_STATUS_CHARGING) {
		cell->electronics_load = 0;
		cell->cumulative_sleep = 0;
		return;
	}*/

	dev_dbg(cell->dev, "FG: EL Correction\n");

	/* Increment Electronics Load */
	cell->electronics_load += cell->config->electronics_load
				* cell->cumulative_sleep;

	/* See if we have more then 1mAh: cell->electronics_load is in 10uAs.
	   We need to convert uAs to mAh: 10uAs = 1/100mAs, 1uAs = 1/3600mAh */
	el_delta = cell->electronics_load / 100 / 3600;

	if (el_delta > 0) {
		cell->electronics_load %= el_delta;

		/* first decrement overcharge capacity, if any */
		if (cell->overcharge_q > 0) {
			cell->overcharge_q -= el_delta;
			if (cell->overcharge_q < 0) {
				cell->nac += cell->overcharge_q;
				cell->learn_q += cell->overcharge_q;
				cell->overcharge_q = 0;
			}
		} else {
			/* decrement NAC, if no overcharge present */
			cell->nac -= el_delta;
			cell->learn_q -= el_delta;
		}

		if (cell->nac < 0)
			cell->nac = 0;

		/* count cycle self discharge */
		if (cell->cycle_dsg_estimate < MAX_UNSIGNED_INT)
			cell->cycle_dsg_estimate += el_delta;

		/* Disqualify learning, if too much self-discharge */
		if (cell->cycle_dsg_estimate
			> cell->config->max_dsg_estimate) {
			if (cell->vcq || cell->vdq) {
				dev_dbg(cell->dev,
					"Learning Disqualified, too much EL: "
					"%d > %d\n",
					cell->cycle_dsg_estimate,
					cell->config->max_dsg_estimate);

				cell->vcq = false;
				cell->vdq = false;
			}
		}
	}

	cell->cumulative_sleep = 0;
}

/* Test EDV point, against current discharge settings and conditions */
short palmas_test_edv(struct cell_state *cell)
{
	bool bad_load = 0;

	/* Checking if Current is Bad (Too Low, ot Too High) */
	bad_load |= -cell->cur < (short) cell->config->light_load;
	bad_load |= -cell->cur > cell->config->edv->overload_current;

	/* Checking if capacity droped below EDV%, restoring if so */
	if (cell->nac < cell->edv.min_capacity) {
		dev_dbg(cell->dev, "NAC(%d) < EDV Capacity(%d)\n",
			cell->nac, cell->edv.min_capacity);
		if (bad_load && cell->vdq) {
			cell->vdq = false;
			dev_dbg(cell->dev,
				"LRN: Current out of qualification range"
				"Disqual. %d < Cur < %d\n",
				cell->config->light_load,
				cell->config->edv->overload_current);
		}
		cell->nac = cell->edv.min_capacity;
	}

	/* If Voltage is under EDV Voltage, reporting EDV */
	if (EDV_VOLTAGE <= cell->edv.voltage) {
		dev_dbg(cell->dev, "Voltage %dmV <= EDV %dmV\n",
			EDV_VOLTAGE, cell->edv.voltage);
		/* If VDQ, checking if it needs to be cleared */
		if (bad_load && cell->vdq) {
			cell->vdq = false;
			dev_dbg(cell->dev,
				"LRN: Current out of qualification range"
				"Disqual. %d < Cur < %d\n",
				cell->config->light_load,
				cell->config->edv->overload_current);
		}

		return true;
	}

	return false;
}

/*
 * EDV targeting routine, that tests EDV point and Adjusts a capacity, based
 * on the Voltage and NAC
 */
bool palmas_look_for_edv(struct cell_state *cell)
{
	if (palmas_test_edv(cell)) {
		cell->seq_edvs++;
		dev_dbg(cell->dev, "EDV: EDV detected #%d\n", cell->seq_edvs);
	} else {
		cell->seq_edvs = 0;
	}


	if (cell->seq_edvs > cell->config->edv->seq_edv) {
		/* EDV point is reached */
		cell->seq_edvs = 0;
		dev_dbg(cell->dev, "EDV: EDV reached (%dmV, %dmAh)\n",
			EDV_VOLTAGE, cell->edv.min_capacity);

		/* Let's check whether discharge cycle is still useable
		   for learning */
		if ((short) cell->voltage <
			(short) (cell->edv.voltage -
				cell->config->deep_dsg_voltage)) {
			dev_dbg(cell->dev, "Learning Cycle isn't qualified\n");
			cell->vdq = false;
		}

		/* Set capacity to the EDV% Level */
		if (cell->nac > cell->edv.min_capacity)
			cell->nac = cell->edv.min_capacity;

		return 1;
	}

	return 0;
}

/*
 * EDV algorithm, should be called on discharge, adjusts NAC, based
 * on the discharge conditions. Also set EDV2, 1, 0 flags if needed
 */

void palmas_init_edv(struct cell_state *cell, struct edv_point *edv)
{
	if ((cell->edv.percent == edv->percent) && cell->init)
		return;

	cell->edv.voltage = edv->voltage;
	cell->edv.percent = edv->percent;

	cell->edv.min_capacity = DIV_ROUND_CLOSEST(cell->fcc * edv->percent,
							MAX_PERCENTAGE);
}

void palmas_learn_capacity(struct cell_state *cell, unsigned short capacity)
{
	short learn_limit;

	/* Make sure no learning is still in progress */
	cell->vcq = false;
	cell->vdq = false;

	/* Check if Learning needs to be Disqualified */
	if (cell->temperature < cell->config->low_temp) {
		dev_dbg(cell->dev, "LRN: Learning disqual (Temp) %d < %d\n",
			cell->temperature,
			cell->config->low_temp);
		return;
	}

	if (abs(cell->ocv_total_q) > cell->config->ocv->max_ocv_discharge) {
		dev_dbg(cell->dev, "LRN: Learning disqual (OCV) %d > %d\n",
			cell->ocv_total_q,
			cell->config->ocv->max_ocv_discharge);
		return;
	}

	dev_dbg(cell->dev, "LRN: Learn Capacity = %dmAh\n", capacity);

	/* Learned Capacity is much lower, then expected */
	learn_limit = cell->fcc - cell->config->max_decrement;
	if ((short) capacity < learn_limit) {
		capacity = learn_limit;
		dev_dbg(cell->dev, "LRN: Capacity is LOW, limit to %4dmAh\n",
			capacity);
	} else {
		learn_limit = cell->fcc + cell->config->max_increment;
		if ((short) capacity > learn_limit) {
			/* Learned Capacity is much greater, then expected */
			capacity = learn_limit;
			dev_dbg(cell->dev,
				"LRN: Capacity is HIGH, limit to %4dmAh\n",
				capacity);
		} else {
			/* capacity within expected range */
			dev_dbg(cell->dev,
				"LRN: Capacity within range (%d < %d < %d)\n",
				cell->fcc - cell->config->max_decrement,
				capacity,
				cell->fcc + cell->config->max_increment);
		}
	}

	/* Reset No Learn Counter */
	cell->learned_cycle = cell->cycle_count;

	cell->new_fcc = capacity;

	/* We can update FCC here, only if charging is on */
	if (!cell->edv2) {
		dev_dbg(cell->dev,
			"LRN: FCC Updated, newFCC = %d, FCC = %d mAh\n",
			cell->new_fcc,
			cell->fcc);

		cell->fcc = cell->new_fcc;
		cell->updated = true;
	} else {
		dev_dbg(cell->dev,
			"LRN: FCC <- %dmAh, on next CHG\n", cell->new_fcc);
	}

}

void palmas_edv(struct cell_state *cell)
{
	/* Check if EDV condition reached and we are above EDV0*/


	if (!cell->edv0 && palmas_look_for_edv(cell)) {
		/* EDV2 point reached */
		if (!cell->edv2) {
			/* todo : recalibrate? */
			cell->edv2 = true;
			if (cell->vdq) {
				dev_dbg(cell->dev, "LRN: Learned Capacity ="
					" %d + %d + %d + %d mAh\n",
					-cell->learn_q,
					cell->ocv_total_q,
					cell->learn_offset,
					cell->nac);


				palmas_learn_capacity(cell,
					-cell->learn_q +
					cell->ocv_total_q +
					cell->learn_offset +
					cell->nac);
			}


			cell->vdq = false;
			palmas_init_edv(cell, &cell->config->edv->edv[1]);

		} else if (!cell->edv1) {
			/* EDV1 point reached */
			cell->edv1 = true;
			palmas_init_edv(cell, &cell->config->edv->edv[0]);
		} else {
			/* EDV0 point reached */
			cell->edv0 = true;
		}
	}
}


static void palmas_cc(struct cell_state *cell, short delta_q)
{

	dev_dbg(cell->dev, "FG: CC Correction\n");

	/* Check if we are just exited OCV */
	if (cell->ocv) {
		cell->ocv = false;
		cell->ocv_total_q += cell->ocv_enter_q - cell->nac;
		dev_dbg(cell->dev,
			"FG: Exit OCV, OCVEnterQ = %dmAh, OCVTotalQ = %dmAh\n",
			cell->ocv_enter_q, cell->ocv_total_q);
	}

	/* See if we are under 0 relative capacity level */
	if ((cell->nac > 0) || cell->edv0
		|| ((cell->negative_q >= 0) && (delta_q > 0))) {

		/* Count Overcharge Capacity */
		if ((cell->nac == cell->fcc) && cell->cc)
			if ((cell->overcharge_q < cell->config->max_overcharge)
				|| (delta_q < 0))
					cell->overcharge_q += delta_q;

		if (cell->overcharge_q < 0)
			cell->overcharge_q = 0;

		/* Do not correct NAC, until Overcharge present */
		if (cell->overcharge_q <= 0) {
			cell->learn_q += delta_q;
			cell->nac = palmas_accumulate(cell->nac, cell->fcc,
							delta_q);

		}

		cell->soc = DIV_ROUND_CLOSEST(cell->nac * MAX_PERCENTAGE,
						cell->fcc);
	} else {
		/* Wait untill we get to 0 level, then start counting */
		cell->negative_q += delta_q;
	}

	/* EDV adjustments, only if discharging */
	if (!cell->sleep)
		if (cell->cur < 0)
			palmas_edv(cell);

	palmas_el(cell);
}

static void palmas_battery_current_now(struct palmas_battery_info *di)
{
	struct palmas *palmas = di->palmas;
	int ret = 0;
	unsigned int reg;
	s16 temp = 0;
	int current_now = 0;

	/* pause FG updates to get consistant data */
	ret = palmas_update_bits(palmas, PALMAS_FUEL_GAUGE_BASE,
		PALMAS_FG_REG_00, PALMAS_FG_REG_00_CC_PAUSE,
		PALMAS_FG_REG_00_CC_PAUSE);

	if (ret < 0) {
		dev_err(di->dev, "Error pausing FG FG_REG_00\n");
		return;
	}

	ret = palmas_read(palmas, PALMAS_FUEL_GAUGE_BASE,
			PALMAS_FG_REG_10, &reg);
	if (ret < 0) {
		dev_dbg(di->dev, "failed to read FG_REG_10\n");
		return;
	}
	temp = reg;

	ret = palmas_read(palmas, PALMAS_FUEL_GAUGE_BASE,
		PALMAS_FG_REG_11, &reg);
	if (ret < 0) {
		dev_dbg(di->dev, "failed to read FG_REG_11\n");
		return;
	}

	temp |= reg << 8;

	/* resume FG updates */
	ret = palmas_update_bits(palmas, PALMAS_FUEL_GAUGE_BASE,
	PALMAS_FG_REG_00, PALMAS_FG_REG_00_CC_PAUSE, 0);
	if (ret < 0) {
		dev_dbg(di->dev, "Error resuming FG FG_REG_00\n");
		return;
	}

	/* sign extend the result */
	temp = ((s16)(temp << 2) >> 2);
	current_now = temp - di->cc_offset;

	/* current drawn per sec */
	current_now = current_now * fuelgauge_rate[di->fuelgauge_mode];
	/* current in mAmperes */
	current_now = (current_now * di->current_max_scale) >> 13;
	/* current in uAmperes */
	current_now = current_now * 1000;
	di->battery_current_uA = current_now;
}

static int palmas_gpadc_conversion(char *channel_name)
{
	struct iio_channel *iio_channel;
	int channel_value = 0;
	int ret;

	iio_channel = iio_channel_get(MODULE_NAME, channel_name);

	ret = iio_read_channel_processed(iio_channel, &channel_value);

	if (ret < 0) {
		printk(KERN_ERR"%s(): Gpadc conversion failed\n", __func__);
		return -1;
	}

	return channel_value;
}

static void palmas_battery_voltage_now(struct palmas_battery_info *di)
{
	int ret;

	ret = palmas_gpadc_conversion(VBAT_CHANNEL);
	if (ret < 0)
		dev_err(di->dev, "Error in Battery Voltage conversion\n");

	di->battery_voltage_uV = ret*1000;
}

static int palmas_battery_get_props(struct power_supply *psy,
				enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct palmas_battery_info *di = container_of(psy,
		struct palmas_battery_info, battery);
	int temp;
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = di->cell.config->technology;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		if (di->battery_current_uA > 0)
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		palmas_battery_voltage_now(di);
		val->intval = di->battery_voltage_uV;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		palmas_battery_current_now(di);
		val->intval = di->battery_current_uA;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = battery_gauge_get_battery_temperature(di->bg_dev, &temp);
		if (ret < 0)
			return -EINVAL;
		val->intval = temp * 10;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = (di->battery_online || di->battery_soldered);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = di->battery_current_avg_uA;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = di->battery_health;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = di->battery_capacity;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

unsigned short interpolate(unsigned short value,
				unsigned short *table,
				unsigned char size)
{
	unsigned char i;
	unsigned short d;

	for (i = 0; i < size; i++)
		if (value < table[i])
			break;

	if ((i > 0)  && (i < size)) {
		d = (value - table[i-1]) * (INTERPOLATE_MAX/(size-1));
		d /=  table[i] - table[i-1];
		d = d + (i-1) * (INTERPOLATE_MAX/(size-1));
	} else {
		d = i * DIV_ROUND_CLOSEST(INTERPOLATE_MAX, size);
	}

	if (d > 1000)
		d = 1000;

	return d;
}


/*
 * Open Circuit Voltage (OCV) correction routine. This function estimates SOC,
 * based on the voltage.
 */
void palmas_ocv(struct cell_state *cell)
{
	int tmp;
	dev_dbg(cell->dev, "FG: OCV Correction\n");

	/* Reset EL counter */
	cell->electronics_load = 0;
	cell->cumulative_sleep = 0;

	tmp = interpolate(cell->av_voltage, cell->config->ocv->table,
		OCV_TABLE_SIZE);

	cell->soc = DIV_ROUND_CLOSEST(tmp * MAX_PERCENTAGE, INTERPOLATE_MAX);
	cell->nac = DIV_ROUND_CLOSEST(tmp * cell->fcc, INTERPOLATE_MAX);

	if (!cell->ocv && cell->init) {
		cell->ocv = true;
		cell->ocv_enter_q = cell->nac;
		dev_dbg(cell->dev, "LRN: Entering OCV, OCVEnterQ = %dmAh\n",
			cell->ocv_enter_q);
	}

	do_gettimeofday(&cell->last_ocv);
}

/*
 * Updates EDV0, EDV1, EDV2 falgs according to SOC. Except when EDV is
 * working
 */
void palmas_update_edv_flags(struct cell_state *cell)
{
	unsigned char soc;

	soc = DIV_ROUND_CLOSEST(cell->nac * MAX_PERCENTAGE, cell->fcc);

	if (soc > cell->config->edv->edv[2].percent) {
		cell->edv2 = false;
		cell->edv1 = false;
		cell->edv0 = false;
		palmas_init_edv(cell, &cell->config->edv->edv[2]);
	} else if (((soc < cell->config->edv->edv[2].percent) || !cell->init)
			&& (soc > cell->config->edv->edv[1].percent)) {
		/* If soc == EDV2 at init, start looking for EDV1 */
		cell->edv2 = true;
		cell->edv1 = false;
		cell->edv0 = false;
		cell->vdq  = false;
		palmas_init_edv(cell, &cell->config->edv->edv[1]);
	} else if ((soc < cell->config->edv->edv[1].percent) || !cell->init) {
		/* If soc == EDV1 at init, start looking for EDV0 */
		cell->edv2 = true;
		cell->edv1 = true;
		cell->vdq  = false;

		if (soc > cell->config->edv->edv[0].percent)
			cell->edv0 = false;

		if (cell->edv.percent != cell->config->edv->edv[0].percent)
			palmas_init_edv(cell, &cell->config->edv->edv[0]);
	}

}

void palmas_fg_init(struct cell_state *cell, short voltage)
{
	unsigned short i;

	cell->fcc = cell->config->design_capacity;
	cell->qmax = cell->config->design_qmax;
	cell->new_fcc = cell->fcc;
	cell->voltage = voltage;
	cell->av_voltage = voltage;

	for (i = 0; i < AV_SIZE; i++) {
		av_v[i] = voltage;
		av_c[i] = 0;
	}

	av_v_index = 0;
	av_c_index = 0;

	cell->temperature = 200;
	cell->cycle_count = 1;

	cell->learned_cycle = cell->cycle_count;
	cell->prev_soc = -1;

	/* On init, get SOC from OCV */
	palmas_ocv(cell);
	dev_dbg(cell->dev, "FG: Init (%dv, %dmAh, %d%%)\n",
		voltage, cell->nac, cell->soc);

	/* Update EDV flags */
	palmas_update_edv_flags(cell);

	do_gettimeofday(&cell->last_correction);

	cell->init = true;
}

/* Check if the cell is in Sleep */
bool palmas_check_relaxed(struct cell_state *cell)
{
	struct timeval now;
	do_gettimeofday(&now);

	if (!cell->sleep) {
		if (abs(cell->cur) <=
			cell->config->ocv->sleep_enter_current) {
			if (cell->sleep_samples < MAX_UINT8)
				cell->sleep_samples++;

			if (cell->sleep_samples >=
				cell->config->ocv->sleep_enter_samples) {
				/* Entering sleep mode */
				cell->sleep_timer.tv_sec = now.tv_sec;
				cell->el_timer.tv_sec = now.tv_sec;
				cell->sleep = true;
				dev_dbg(cell->dev, "Sleeping\n");
				cell->calibrate = true;
			}
		} else {
			cell->sleep_samples = 0;
		}
	} else {
		/* The battery cell is Sleeping, checking if need to exit
		   sleep mode count number of seconds that cell spent in
		   sleep */
		cell->cumulative_sleep += now.tv_sec - cell->el_timer.tv_sec;
		cell->el_timer.tv_sec = now.tv_sec;

		/* Check if we need to reset Sleep */
		if (abs(cell->av_current) >
			cell->config->ocv->sleep_exit_current) {

			if (abs(cell->cur) >
				cell->config->ocv->sleep_exit_current) {
				if (cell->sleep_samples < MAX_UINT8)
					cell->sleep_samples++;
			} else {
				cell->sleep_samples = 0;
			}

			/* Check if we need to reset a Sleep timer */
			if (cell->sleep_samples >
				cell->config->ocv->sleep_exit_samples) {
				/* Exit sleep mode */
				cell->sleep_timer.tv_sec = 0;
				cell->sleep = false;
				cell->relax = false;
				dev_dbg(cell->dev,
					"Not relaxed and not sleeping\n");
			}
		} else {
			cell->sleep_samples = 0;
			if (!cell->relax) {
				if (now.tv_sec-cell->sleep_timer.tv_sec >
					cell->config->ocv->relax_period) {
					cell->relax = true;
					dev_dbg(cell->dev, "Relaxed\n");
					cell->calibrate = true;
				}
			}
		}
	}

	return cell->relax;
}

/*
 * Check for Charge Complete condition:
 * (voltage > cc_voltage) AND
 * ((current > cc_current) OR (top_off_capacity>cc_capacity))
 */
static bool palmas_check_chg_complete(struct cell_state *cell, short delta_q)
{
	bool ret = false;

	if (cell->voltage >= (short)cell->config->cc_voltage) {

		/* Check if stable (V > CC_V) reached */
		if (cell->seq_cc_voltage >= cell->config->seq_cc) {

			/* Check if CC_Cap reached */
			if (cell->top_off_q > (cell->fcc / 100)
				* cell->config->cc_capacity)
				ret = true;
			else
				cell->top_off_q += delta_q;

			/* Start looking for stable (C > CC_Cur) */
			if ((cell->cur <= (short)cell->config->cc_current)
				&& (cell->av_current <=
					(short)cell->config->cc_current)) {

					if (cell->seq_cc_current >=
						cell->config->seq_cc)
						ret = true;
					else
						cell->seq_cc_current++;
			} else
				cell->seq_cc_current = 0;

		} else
			cell->seq_cc_voltage++;

	} else {
		cell->seq_cc_voltage = 0;
		cell->seq_cc_current = 0;
		cell->top_off_q = 0;
	}

	dev_dbg(cell->dev, "CHGCPL: seq_v %d; seq_c %d; top_q %d",
		cell->seq_cc_voltage,
		cell->seq_cc_current,
		cell->top_off_q);

	return ret;
}

/*
 * This is invoked when the charge is complete. The conarge complete condition
 * is configurable, and can be set to be an input from a charger or, the gauge
 * itself can detect charge complete condition.
 * On charge complete, if charge cycle is qualified, the gauge learns a
 * capacity.
 * This is can be invoked from either foreground, or background context.
 */
static void palmas_charge_complete(struct cell_state *cell)
{
	dev_dbg(cell->dev, "CHG: Charge Complete Detected!\n");

	/* Set Charge Complete Flag and Toggle a CC Pin, if configured to */
	cell->cc = true;
	cell->calibrate = true;

	/* Check if we can Learn capacity on Charge */
	if (cell->vcq) {
		dev_dbg(cell->dev, "LRN: Learned Capacity = %d + %d + %d mAh\n",
			cell->learn_q,
			cell->learn_offset,
			-cell->ocv_total_q);

		palmas_learn_capacity(cell,
			cell->learn_q + cell->learn_offset - cell->ocv_total_q);
	}

	dev_dbg(cell->dev, "CHG: Setting RM to FCC\n");

	/* Set Remaining Capacities to Full */
	cell->nac = cell->fcc;

	/* Capacity learning complete */
	cell->vcq = false;
}

/*
 * Routine that tracks charge process. Should be called, whenever the cell
 * is being charged.
 */
static void palmas_charging(struct cell_state *cell, short delta_q)
{
	/* the battery is chargeing so clear the discharge flag */
	if (!cell->chg) {
		cell->charge_cycle_q = 0;
		cell->seq_cc_voltage = 0;
		cell->seq_cc_current = 0;
		cell->top_off_q = 0;
		dev_dbg(cell->dev, "CHG: Starting Charge Cycle\n");
		cell->chg = true;
	}

	cell->charge_cycle_q += delta_q;

	/* Check if need to cancel/disqualify Discharge, or start learning */
	if (cell->charge_cycle_q > cell->config->mode_switch_capacity) {

		if (cell->dsg) {
			dev_dbg(cell->dev, "CHG: DSG cleared\n");
			cell->dsg = false;
		}

		if (cell->vdq) {
			dev_dbg(cell->dev, "CHG:, DSG Learning clearer\n");
			cell->vdq = false;
		}

		/* Check if we can do learning on this cycle */
		if (!cell->vcq && cell->edv2) {
			dev_dbg(cell->dev, "CHG: Start Learning\n");

			cell->learn_offset = cell->nac;
			cell->learn_q = 0;
			cell->ocv_enter_q = 0;
			cell->ocv_total_q = 0;
			cell->cycle_dsg_estimate = 0;
			cell->vcq = true;

			dev_dbg(cell->dev,
				"CHG: LearnQ = %d mAh, LearnOffset = %d mAh\n",
				cell->learn_q,
				cell->learn_offset);
		}

		/* Update FCC, if there is a need */
		if (cell->fcc != cell->new_fcc) {
			cell->fcc = cell->new_fcc;
			cell->updated = true;
			dev_dbg(cell->dev, "LRN: FCC <- %d mAh\n", cell->fcc);
		}
	}

	/* Check for Charge Complete Condition */
	if (!cell->cc
		&& cell->config->cc_out
		&& palmas_check_chg_complete(cell, delta_q))
			palmas_charge_complete(cell);
}


/*
 * Counts cycles, based on the passed charge, and updates CycleCount EEPROM
 * setting, if cycle is detected
 */
static void palmas_count_cycle(struct cell_state *cell, short delta_q)
{
	if (delta_q < 0) {

		cell->cycle_q -= delta_q;

		if (cell->cycle_q > cell->config->cycle_threshold) {

			/* Check how many cycles ago we learned FCC,
			   and adjust FCC accordingly */
			if ((cell->cycle_count - cell->learned_cycle) >=
				NO_LEARNING_CYCLES) {

				/* Reset Learn Cycle Tracker */
				cell->learned_cycle = cell->cycle_count;

				/* We are canceling learning */
				cell->vcq = false;
				cell->vdq = false;

				/* Adjust FCC */
				if (cell->fcc > cell->config->fcc_adjust)
					cell->fcc -= cell->config->fcc_adjust;
				else
					cell->fcc = 0;

				cell->updated = true;
			}

			cell->cycle_q -= cell->config->cycle_threshold;

			if (cell->cycle_count < MAX_INT) {
				cell->cycle_count++;
				cell->updated = true;
			}

			dev_dbg(cell->dev, "DSG %dmAh, CycleCount = %d\n",
				cell->config->cycle_threshold,
				cell->cycle_count);
		}
	}
}

/*
 * Routine that tracks discharge process. Should be called, whenever the cell
 * is being discahrged.
 */

static void palmas_discharging(struct cell_state *cell, short delta_q)
{
	/* Starting brand new Discharge Cycle */
	if (!cell->dsg) {
		dev_dbg(cell->dev, "DSG: Starting Discharge Cycle\n");
		cell->discharge_cycle_q = 0;
		cell->dsg = true;
	}

	cell->discharge_cycle_q -= delta_q;
	palmas_count_cycle(cell, delta_q);

	/* Check if need to cancel/disqualify Charge, or start learning */
	if (cell->discharge_cycle_q > cell->config->mode_switch_capacity) {
		if (cell->chg) {
			dev_dbg(cell->dev,
				"DSG: CHG cleared due to Discharge Cycle\n");
			cell->chg = false;
		}

		if (cell->vcq) {
			dev_dbg(cell->dev,
				"DSG: VCQ cleared due to Discharge Cycle\n");
			cell->vcq = false;
		}

		/* Check if we can do learning on this cycle */
		if (!cell->vdq &&
			(cell->nac >= (cell->fcc - cell->config->near_full))) {
			dev_dbg(cell->dev, "DSG: Start Learning\n");
			cell->learn_offset = cell->fcc - cell->nac;
			cell->learn_q = 0;
			cell->ocv_enter_q = 0;
			cell->ocv_total_q = 0;
			cell->cycle_dsg_estimate = 0;
			cell->vdq = true;
			dev_dbg(cell->dev,
				"LearnQ = %d mAh, LearnOffset = %d mAh\n",
				cell->learn_q,
				cell->learn_offset);
		}
	}
}

void palmas_process(struct cell_state *cell, short delta_q, short voltage,
		short cur, short temperature)
{
	int i, tmp;
	struct timeval now;

	if (!cell->init)
		return;

	/* Update voltage and add it to the buffer, update average*/
	tmp = 0;
	cell->voltage = voltage;
	av_v_index++;
	av_v_index %= AV_SIZE;
	av_v[av_v_index] = voltage;
	for (i = 0; i < AV_SIZE; i++)
		tmp += av_v[i];
	cell->av_voltage = tmp/AV_SIZE;

	/* Update current and add it to the buffer, update average*/
	tmp = 0;
	cell->cur = cur;
	av_c_index++;
	av_c_index %= AV_SIZE;
	av_c[av_c_index] = cur;
	for (i = 0; i < AV_SIZE; i++)
		tmp += av_c[i];
	cell->av_current = tmp/AV_SIZE;

	/* Update temperature*/
	cell->temperature = temperature;

	/* Check time since last_call */
	do_gettimeofday(&now);
	tmp = now.tv_sec - cell->last_correction.tv_sec;

	/* Check what capacity currection algorithm should we use: OCV or CC */
	if ((tmp > cell->config->ocv->relax_period)
		&& (abs(cell->cur) < cell->config->ocv->long_sleep_current)) {

			palmas_ocv(cell);
	 }

	else if (palmas_check_relaxed(cell)) {
		/* We are not doing any active CHG/DSG, clear flags
		   this does not compromise learning cycles */
		cell->chg = false;
		cell->dsg = false;

		/* Checking if we can do an OCV correction */
		if (palmas_can_ocv(cell))
			palmas_ocv(cell);
		else
			palmas_cc(cell, delta_q);
	} else /* Not Relaxed: actively charging or discharging */
		palmas_cc(cell, delta_q);

	/* Charge / Discharge spesific functionality */
	if (!cell->sleep) {
		if (cell->cur > 0)
			palmas_charging(cell, delta_q);
		else if (cell->cur < 0)
			palmas_discharging(cell, delta_q);
	}

	/* Update Regular SOC */
	cell->soc = DIV_ROUND_CLOSEST(cell->nac * MAX_PERCENTAGE, cell->fcc);

	palmas_update_edv_flags(cell);

	/* Check if battery is full */
	if (cell->nac >= cell->fcc) {
		cell->full = true;
	} else {
		cell->full = false;
		if (cell->nac <= (cell->fcc - cell->config->recharge))
			cell->cc = false;
	}

	/* Checking if we need to set an updated flag (is SOC changed) */
	if (cell->prev_soc != cell->soc) {
		cell->prev_soc = cell->soc;
		cell->updated = true;
	}

	cell->last_correction.tv_sec = now.tv_sec;

}

static void palmas_gasgauge_calibrate(struct palmas_battery_info *di)
{
	struct palmas *palmas = di->palmas;
	int ret;

	ret = palmas_update_bits(palmas, PALMAS_FUEL_GAUGE_BASE,
		PALMAS_FG_REG_00, PALMAS_FG_REG_00_CC_AUTOCLEAR,
		PALMAS_FG_REG_00_CC_AUTOCLEAR);

	if (ret)
		dev_err(di->dev, "set CC_AUTOCLEAR failed!\n");

	di->accumulated_charge = 0;

	ret = palmas_update_bits(palmas, PALMAS_FUEL_GAUGE_BASE,
		PALMAS_FG_REG_00, PALMAS_FG_REG_00_CC_CAL_EN,
		PALMAS_FG_REG_00_CC_CAL_EN);
	if (ret)
		dev_err(di->dev, "set CC_CAL_EN failed!\n");
}

static void palmas_calculate_capacity(struct palmas_battery_info *di)
{
	int accumulated_charge;

	/*
	 * 3000 mA maps to a count of 4096 per sample
	 * We have 4 samples per second
	 * Charge added in one second == (acc_value * 3000 / (4 * 4096))
	 * mAh added == (Charge added in one second / 3600)
	 * mAh added == acc_value * (3000/3600) / (4 * 4096)
	 * mAh added == acc_value * (5/6) / (2^14)
	 * Using 5/6 instead of 3000 to avoid overflow
	 * FIXME: revisit this code for overflows
	 * FIXME: Take care of different value of samples/sec
	 */

	accumulated_charge = (((di->battery_charge_n1 -
		(di->cc_offset * di->battery_timer_n1)) * 5) / 6) >> 14;


	accumulated_charge = accumulated_charge * 10 /
			(int)di->cell.config->r_sense;


	palmas_process(&di->cell, accumulated_charge - di->accumulated_charge,
			di->battery_voltage_uV / 1000,
			(int16_t)(di->battery_current_avg_uA / 1000),
			di->battery_termperature_tenthC);


	di->accumulated_charge = accumulated_charge;

	/* Gas gauge requested CC autocalibration */
	if (di->cell.calibrate) {
		di->cell.calibrate = false;
		palmas_gasgauge_calibrate(di);
		di->battery_timer_n1 = 0;
		di->battery_charge_n1 = 0;
	}

	di->battery_capacity = di->cell.soc;

	if (di->battery_capacity >= PALMAS_BATTERY_FULL) {
		di->battery_capacity = PALMAS_BATTERY_FULL;
		di->battery_health = POWER_SUPPLY_STATUS_FULL;
	} else if (di->battery_capacity < PALMAS_BATTERY_LOW) {
		di->battery_health = POWER_SUPPLY_HEALTH_DEAD;
	} else {
		di->battery_health = POWER_SUPPLY_HEALTH_GOOD;
	}

	/* Battery state changes needs to be sent to the OS */
	if (di->cell.updated) {
		di->cell.updated = 0;
		power_supply_changed(&di->battery);
	}

	return;
}

static void palmas_battery_current_avg(struct work_struct *work)
{
	struct palmas_battery_info *di = container_of(work,
		struct palmas_battery_info,
		battery_current_avg_work.work);
	struct palmas *palmas = di->palmas;
	s32 samples = 0;
	s16 cc_offset = 0;
	int current_avg_uA = 0, ret;
	u8 temp[4];

	di->battery_charge_n2 = di->battery_charge_n1;
	di->battery_timer_n2 = di->battery_timer_n1;

	/* pause FG updates to get consistant data */
	ret = palmas_update_bits(palmas, PALMAS_FUEL_GAUGE_BASE,
		PALMAS_FG_REG_00, PALMAS_FG_REG_00_CC_PAUSE,
		PALMAS_FG_REG_00_CC_PAUSE);
	if (ret < 0) {
		dev_err(di->dev, "Error pausing FG FG_REG_00\n");
		return;
	}

	/* FG_REG_01, 02, 03 is 24 bit unsigned sample counter value */
	ret = palmas_bulk_read(palmas, PALMAS_FUEL_GAUGE_BASE,
		PALMAS_FG_REG_01, temp, 3);
	if (ret < 0) {
		dev_err(di->dev, "Error reading FG_REG_01-03\n");
		return;
	}

	temp[3] = 0;

	di->battery_timer_n1 = le32_to_cpup((u32 *)temp);

	/*
	 * FG_REG_04, 5, 6, 7 is 32 bit signed accumulator value
	 * accumulates instantaneous current value
	 */
	ret = palmas_bulk_read(palmas, PALMAS_FUEL_GAUGE_BASE,
		PALMAS_FG_REG_04, temp, 4);
	if (ret < 0) {
		dev_err(di->dev, "Error reading FG_REG_04-07\n");
		return;
	}

	di->battery_charge_n1 = le32_to_cpup((u32 *)temp);

	/* FG_REG_08, 09 is 10 bit signed calibration offset value */
	ret = palmas_bulk_read(palmas, PALMAS_FUEL_GAUGE_BASE,
		PALMAS_FG_REG_08, temp, 2);
	if (ret < 0) {
		dev_err(di->dev, "Error reading FG_REG_09-09\n");
		return;
	}

	cc_offset = le16_to_cpup((u16 *)temp);
	cc_offset = ((s16)(cc_offset << 6) >> 6);
	di->cc_offset = cc_offset;

	/* resume FG updates */
	ret = palmas_update_bits(palmas, PALMAS_FUEL_GAUGE_BASE,
	PALMAS_FG_REG_00, PALMAS_FG_REG_00_CC_PAUSE, 0);

	if (ret < 0) {
		dev_dbg(di->dev, "Error resuming FG FG_REG_00\n");
		return;
	}

	samples = di->battery_timer_n1 - di->battery_timer_n2;
	/* check for timer overflow */
	if (di->battery_timer_n1 < di->battery_timer_n2)
		samples = samples + (1 << 24);

	/* offset is accumulative over number of samples */
	cc_offset = cc_offset * samples;

	current_avg_uA = ((di->battery_charge_n1 - di->battery_charge_n2
					- cc_offset)
					* di->current_max_scale) /
					fuelgauge_rate[di->fuelgauge_mode];
	/* clock is a fixed 32Khz */
	current_avg_uA >>= 15;

	/* Correct for the fuelguage sampling rate */
	samples /= fuelgauge_rate[di->fuelgauge_mode] * 4;

	/*
	 * Only update the current average if we have had a valid number
	 * of samples in the accumulation.
	 */
	if (samples) {
		current_avg_uA = current_avg_uA / samples;
		di->battery_current_avg_uA = current_avg_uA * 1000;
	}

	palmas_calculate_capacity(di);

	schedule_delayed_work(&di->battery_current_avg_work,
		msecs_to_jiffies(1000 * di->battery_current_avg_interval));
	return;
}

static int palmas_ovc_period_config(struct palmas_battery_platform_data *pdata)
{
	if (pdata->ovc_period >= 3900)
		return 0x3;
	if (pdata->ovc_period >= 2000)
		return 0x2;
	if (pdata->ovc_period >= 1000)
		return 0x1;
	return 0x0;
}

static int palmas_ovc_thresh_config(struct palmas_battery_platform_data *pdata,
		unsigned int period)
{
	unsigned int step;
	unsigned int div;
	unsigned int reg;
	unsigned int mask;

	div = pdata->cell_cfg->r_sense == 10 ? 1 : 2;
	div *= 1 << period;
	step = 750000 / div;

	mask = PALMAS_FG_REG_21_CC_OVERCUR_THRES_MASK >>
			PALMAS_FG_REG_21_CC_OVERCUR_THRES_SHIFT;
	reg = pdata->ovc_threshold * 1000 / step - 1;
	reg = min(reg, mask);

	return reg;
}

static int palmas_init_oc_alert(struct palmas_battery_info *di,
		struct palmas_battery_platform_data *pdata)
{
	struct palmas *palmas = di->palmas;
	int ret;
	unsigned int period;
	unsigned int thresh;
	unsigned int mask;
	unsigned int val;

	if (!pdata->enable_ovc_alarm)
		return 0;

	ret = palmas_update_bits(palmas, PALMAS_SMPS_BASE,
			PALMAS_SMPS_POWERGOOD_MASK2,
			PALMAS_SMPS_POWERGOOD_MASK2_OVC_ALARM,
			0);
	if (ret < 0) {
		dev_err(di->dev, "failed to write POWERGOOD_MASK2: %d\n", ret);
		return ret;
	}

	period = palmas_ovc_period_config(pdata);
	thresh = palmas_ovc_thresh_config(pdata, period);

	ret = palmas_update_bits(palmas, PALMAS_FUEL_GAUGE_BASE,
			PALMAS_FG_REG_21,
			PALMAS_FG_REG_21_CC_OVERCUR_THRES_MASK,
			thresh);
	if (ret < 0) {
		dev_err(di->dev, "failed to write FG_REG_21: %d\n", ret);
		return ret;
	}

	mask = PALMAS_FG_REG_22_CC_OVC_PER_MASK | PALMAS_FG_REG_22_CC_OVC_EN;
	val = period << PALMAS_FG_REG_22_CC_OVC_PER_SHIFT |
			PALMAS_FG_REG_22_CC_OVC_EN;
	ret = palmas_update_bits(palmas, PALMAS_FUEL_GAUGE_BASE,
			PALMAS_FG_REG_22, mask, val);
	if (ret < 0) {
		dev_err(di->dev, "failed to write FG_REG_22: %d\n", ret);
		return ret;
	}

	return 0;
}

static int palmas_current_setup(struct palmas_battery_info *di,
		struct palmas_battery_platform_data *pdata)
{
	struct palmas *palmas = di->palmas;
	int ret = 0;
	unsigned int reg = 0;
	u8 temp[4];

	/*
	 * Enable the AUTOCLEAR so that any FG is in known state, and
	 * enabled the FG
	 */
	reg = PALMAS_FG_REG_00_CC_AUTOCLEAR | PALMAS_FG_REG_00_CC_FG_EN |
		PALMAS_FG_REG_00_CC_DITH_EN | PALMAS_FG_REG_00_CC_CAL_EN;

	ret = palmas_update_bits(palmas, PALMAS_FUEL_GAUGE_BASE,
		PALMAS_FG_REG_00, reg, reg);

	if (ret < 0)
		dev_err(di->dev, "failed to write FG_REG00\n");


	/* initialise the current average values */
	di->battery_current_avg_interval = pdata->current_avg_interval;

	/* pause FG updates to get consistant data */
	ret = palmas_update_bits(palmas, PALMAS_FUEL_GAUGE_BASE,
		PALMAS_FG_REG_00, PALMAS_FG_REG_00_CC_PAUSE,
		PALMAS_FG_REG_00_CC_PAUSE);

	if (ret < 0) {
		dev_err(di->dev, "Error pausing FG FG_REG_00\n");
		return ret;
	}

	/* FG_REG_01, 02, 03 is 24 bit unsigned sample counter value */
	ret = palmas_bulk_read(palmas, PALMAS_FUEL_GAUGE_BASE,
		PALMAS_FG_REG_01, temp, 3);
	if (ret < 0) {
		dev_err(di->dev, "Error reading FG_REG_01-03\n");
		return ret;
	}

	temp[3] = 0;

	di->battery_timer_n1 = le32_to_cpup((u32 *)temp);

	/*
	 * FG_REG_04, 5, 6, 7 is 32 bit signed accumulator value
	 * accumulates instantaneous current value
	 */
	ret = palmas_bulk_read(palmas, PALMAS_FUEL_GAUGE_BASE,
		PALMAS_FG_REG_04, temp, 4);
	if (ret < 0) {
		dev_err(di->dev, "Error reading FG_REG_04-07\n");
		return ret;
	}

	di->battery_charge_n1 = le32_to_cpup((u32 *)temp);

	/* FG_REG_08, 09 is 10 bit signed calibration offset value */
	ret = palmas_bulk_read(palmas, PALMAS_FUEL_GAUGE_BASE,
		PALMAS_FG_REG_08, temp, 2);
	if (ret < 0) {
		dev_err(di->dev, "Error reading FG_REG_09-09\n");
		return ret;
	}

	di->cc_offset = le16_to_cpup((u16 *)temp);
	di->cc_offset = ((s16)(di->cc_offset << 6) >> 6);

	INIT_DELAYED_WORK(&di->battery_current_avg_work,
						palmas_battery_current_avg);

	schedule_delayed_work(&di->battery_current_avg_work, 0);

	return ret;
}

static int palmas_update_battery_status(struct battery_gauge_dev *bg_dev,
	enum battery_charger_status status)
{
	struct palmas_battery_info *binfo = battery_gauge_get_drvdata(bg_dev);

	if (status == BATTERY_CHARGING)
		binfo->status = POWER_SUPPLY_STATUS_CHARGING;
	else
		binfo->status = POWER_SUPPLY_STATUS_DISCHARGING;

	power_supply_changed(&binfo->battery);
	return 0;
}

static struct battery_gauge_ops plamas_battery_gauge_ops = {
	.update_battery_status = palmas_update_battery_status,
};

static struct battery_gauge_info palmas_battery_gauge_info = {
	.cell_id = 0,
	.bg_ops = &plamas_battery_gauge_ops,
};

static int palmas_battery_probe(struct platform_device *pdev)
{
	struct palmas *palmas = dev_get_drvdata(pdev->dev.parent);
	struct palmas_platform_data *palmas_pdata;
	struct palmas_battery_platform_data *pdata;
	struct palmas_battery_info *di;
	int retry_count;

	int ret;

	palmas_pdata = dev_get_platdata(pdev->dev.parent);
	if (!palmas_pdata) {
		dev_err(&pdev->dev, "Parent platform data not found\n");
		return -EINVAL;
	}

	pdata = palmas_pdata->battery_pdata;
	if (!pdata) {
		dev_err(&pdev->dev, "Platform data not found\n");
		return -EINVAL;
	}

	if (!pdata->is_battery_present) {
		dev_err(&pdev->dev, "Battery not detected! Exiting driver...\n");
		return -ENODEV;
	}

	di = devm_kzalloc(&pdev->dev, sizeof(*di), GFP_KERNEL);
	if (!di)
		return -ENOMEM;

	di->battery_temperature_chart = devm_kzalloc(&pdev->dev,
			sizeof(int) * pdata->battery_temperature_chart_size,
					GFP_KERNEL);

	if (!di->battery_temperature_chart)
		return -ENOMEM;

	memcpy(di->battery_temperature_chart, pdata->battery_temperature_chart,
			sizeof(int) * pdata->battery_temperature_chart_size);

	di->battery_temperature_chart_size =
			pdata->battery_temperature_chart_size;

	di->cell.config = devm_kzalloc(&pdev->dev, sizeof(*di->cell.config),
						GFP_KERNEL);
	if (!di->cell.config)
		return -ENOMEM;

	memcpy(di->cell.config, pdata->cell_cfg, sizeof(*di->cell.config));

	di->cell.config->ocv = devm_kzalloc(&pdev->dev,
					sizeof(*di->cell.config->ocv),
					GFP_KERNEL);
	if (!di->cell.config->ocv)
		return -ENOMEM;

	memcpy(di->cell.config->ocv, pdata->cell_cfg->ocv,
					sizeof(*di->cell.config->ocv));

	di->cell.config->edv = devm_kzalloc(&pdev->dev,
					sizeof(*di->cell.config->edv),
					GFP_KERNEL);
	if (!di->cell.config->edv)
		return -ENOMEM;

	memcpy(di->cell.config->edv, pdata->cell_cfg->edv,
					sizeof(*di->cell.config->edv));

	di->palmas = palmas;
	di->dev = &pdev->dev;

	platform_set_drvdata(pdev, di);

	di->battery_status_interval = pdata->battery_status_interval;

	/* Start with battery health good until we get an IRQ fault */
	di->battery_health = POWER_SUPPLY_HEALTH_GOOD;

	/* calculate current max scale from sense */
	di->current_max_scale = (62000) / di->cell.config->r_sense;

	retry_count = 0;
	/* Initial boot voltage */
	while (di->battery_voltage_uV <= 0
		&& retry_count < pdata->gpadc_retry_count) {
		palmas_battery_voltage_now(di);
		retry_count++;
	}

	if (retry_count == pdata->gpadc_retry_count) {
		dev_err(di->dev, "Gpadc read error. Aborting.\n");
		return -ENODEV;
	}

	ret = palmas_current_setup(di, pdata);
	if (ret < 0) {
		dev_err(di->dev, "Current setup failed. Aborting.\n");
		return ret;
	}

	di->battery.name = "palmas-battery";
	di->battery.type = POWER_SUPPLY_TYPE_BATTERY;
	di->battery.properties = palmas_battery_props;
	di->battery.num_properties = ARRAY_SIZE(palmas_battery_props);
	di->battery.get_property = palmas_battery_get_props;
	di->battery.external_power_changed = NULL;

	/* Initialise the Fuel Guage */
	palmas_fg_init(&di->cell, di->battery_voltage_uV / 1000);

	ret = power_supply_register(di->dev, &di->battery);
	if (ret < 0) {
		dev_err(di->dev, "failed to register battery: %d\n", ret);
		return ret;
	}

	palmas_battery_gauge_info.tz_name = pdata->therm_zone_name;
	di->bg_dev = battery_gauge_register(di->dev, &palmas_battery_gauge_info,
					di);
	if (IS_ERR(di->bg_dev)) {
		ret = PTR_ERR(di->bg_dev);
		dev_err(di->dev, "battery gauge register failed: %d\n", ret);
		goto bg_err;
	}

	ret = palmas_init_oc_alert(di, pdata);
	if (ret < 0) {
		dev_err(di->dev, "OC alert init failed: %d\n", ret);
		return ret;
	}

	return 0;

bg_err:
	power_supply_unregister(&di->battery);
	return ret;
}

static int palmas_battery_remove(struct platform_device *pdev)
{
	struct palmas_battery_info *di = platform_get_drvdata(pdev);

	battery_gauge_unregister(di->bg_dev);
	cancel_delayed_work(&di->battery_current_avg_work);
	flush_scheduled_work();
	power_supply_unregister(&di->battery);
	return 0;
}

static struct platform_driver palmas_battery_driver = {
	.probe = palmas_battery_probe,
	.remove = palmas_battery_remove,
	.driver = {
		.name = "palmas-battery-gauge",
		.owner = THIS_MODULE,
	},
};

static int __init palmas_battery_init(void)
{
	return platform_driver_register(&palmas_battery_driver);
}

static void __exit palmas_battery_exit(void)
{
	platform_driver_unregister(&palmas_battery_driver);
}

subsys_initcall(palmas_battery_init);
module_exit(palmas_battery_exit);

MODULE_AUTHOR("Darbha Sriharsha <dsriharsha@nvidia.com>");
MODULE_AUTHOR("Syed Rafiuddin <srafiuddin@nvidia.com>");
MODULE_DESCRIPTION("PALMAS Fuel gauge");
MODULE_LICENSE("GPL");
