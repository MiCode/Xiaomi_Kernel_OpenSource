/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"ALG: %s: " fmt, __func__

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include "fg-alg.h"

#define FULL_SOC_RAW		255
#define CAPACITY_DELTA_DECIPCT	500

/* Cycle counter APIs */

/**
 * restore_cycle_count -
 * @counter: Cycle counter object
 *
 * Restores all the counters back from FG/QG during boot
 *
 */
int restore_cycle_count(struct cycle_counter *counter)
{
	int rc = 0;

	if (!counter)
		return -ENODEV;

	mutex_lock(&counter->lock);
	rc = counter->restore_count(counter->data, counter->count,
			BUCKET_COUNT);
	if (rc < 0)
		pr_err("failed to restore cycle counter rc=%d\n", rc);
	mutex_unlock(&counter->lock);

	return rc;
}

/**
 * clear_cycle_count -
 * @counter: Cycle counter object
 *
 * Clears all the counters stored by FG/QG when a battery is inserted
 * or the profile is re-loaded.
 *
 */
void clear_cycle_count(struct cycle_counter *counter)
{
	int rc = 0, i;

	if (!counter)
		return;

	mutex_lock(&counter->lock);
	memset(counter->count, 0, sizeof(counter->count));
	for (i = 0; i < BUCKET_COUNT; i++) {
		counter->started[i] = false;
		counter->last_soc[i] = 0;
	}

	rc = counter->store_count(counter->data, counter->count, 0,
			BUCKET_COUNT * 2);
	if (rc < 0)
		pr_err("failed to clear cycle counter rc=%d\n", rc);

	mutex_unlock(&counter->lock);
}

/**
 * store_cycle_count -
 * @counter: Cycle counter object
 * @id: Cycle counter bucket id
 *
 * Stores the cycle counter for a bucket in FG/QG.
 *
 */
static int store_cycle_count(struct cycle_counter *counter, int id)
{
	int rc = 0;
	u16 cyc_count;

	if (!counter)
		return -ENODEV;

	if (id < 0 || (id > BUCKET_COUNT - 1)) {
		pr_err("Invalid id %d\n", id);
		return -EINVAL;
	}

	cyc_count = counter->count[id];
	cyc_count++;

	rc = counter->store_count(counter->data, &cyc_count, id, 2);
	if (rc < 0) {
		pr_err("failed to write cycle_count[%d] rc=%d\n",
			id, rc);
		return rc;
	}

	counter->count[id] = cyc_count;
	pr_debug("Stored count %d in id %d\n", cyc_count, id);

	return rc;
}

/**
 * cycle_count_update -
 * @counter: Cycle counter object
 * @batt_soc: Battery State of Charge (SOC)
 * @charge_status: Charging status from power supply
 * @charge_done: Indicator for charge termination
 * @input_present: Indicator for input presence
 *
 * Called by FG/QG whenever there is a state change (Charging status, SOC)
 *
 */
void cycle_count_update(struct cycle_counter *counter, int batt_soc,
			int charge_status, bool charge_done, bool input_present)
{
	int rc = 0, id, i, soc_thresh;

	if (!counter)
		return;

	mutex_lock(&counter->lock);

	/* Find out which id the SOC falls in */
	id = batt_soc / BUCKET_SOC_PCT;

	if (charge_status == POWER_SUPPLY_STATUS_CHARGING) {
		if (!counter->started[id] && id != counter->last_bucket) {
			counter->started[id] = true;
			counter->last_soc[id] = batt_soc;
		}
	} else if (charge_done || !input_present) {
		for (i = 0; i < BUCKET_COUNT; i++) {
			soc_thresh = counter->last_soc[i] + BUCKET_SOC_PCT / 2;
			if (counter->started[i] && batt_soc > soc_thresh) {
				rc = store_cycle_count(counter, i);
				if (rc < 0)
					pr_err("Error in storing cycle_ctr rc: %d\n",
						rc);
				counter->last_soc[i] = 0;
				counter->started[i] = false;
				counter->last_bucket = i;
			}
		}
	}

	pr_debug("batt_soc: %d id: %d chg_status: %d\n", batt_soc, id,
		charge_status);
	mutex_unlock(&counter->lock);
}

/**
 * get_cycle_count -
 * @counter: Cycle counter object
 *
 * Returns the cycle counter for a SOC bucket.
 *
 */
int get_cycle_count(struct cycle_counter *counter)
{
	int count;

	if (!counter)
		return 0;

	if ((counter->id <= 0) || (counter->id > BUCKET_COUNT))
		return -EINVAL;

	mutex_lock(&counter->lock);
	count = counter->count[counter->id - 1];
	mutex_unlock(&counter->lock);
	return count;
}

/**
 * cycle_count_init -
 * @counter: Cycle counter object
 *
 * FG/QG have to call this during driver probe to validate the required
 * parameters after allocating cycle_counter object.
 *
 */
int cycle_count_init(struct cycle_counter *counter)
{
	if (!counter)
		return -ENODEV;

	if (!counter->data || !counter->restore_count ||
		!counter->store_count) {
		pr_err("Invalid parameters for using cycle counter\n");
		return -EINVAL;
	}

	mutex_init(&counter->lock);
	counter->last_bucket = -1;
	return 0;
}

/* Capacity learning algorithm APIs */

/**
 * cap_learning_post_process -
 * @cl: Capacity learning object
 *
 * Does post processing on the learnt capacity based on the user specified
 * or default parameters for the capacity learning algorithm.
 *
 */
static void cap_learning_post_process(struct cap_learning *cl)
{
	int64_t max_inc_val, min_dec_val, old_cap;
	int rc;

	if (cl->dt.skew_decipct) {
		pr_debug("applying skew %d on current learnt capacity %lld\n",
			cl->dt.skew_decipct, cl->final_cap_uah);
		cl->final_cap_uah = cl->final_cap_uah *
					(1000 + cl->dt.skew_decipct);
		cl->final_cap_uah = div64_u64(cl->final_cap_uah, 1000);
	}

	max_inc_val = cl->learned_cap_uah * (1000 + cl->dt.max_cap_inc);
	max_inc_val = div64_u64(max_inc_val, 1000);

	min_dec_val = cl->learned_cap_uah * (1000 - cl->dt.max_cap_dec);
	min_dec_val = div64_u64(min_dec_val, 1000);

	old_cap = cl->learned_cap_uah;
	if (cl->final_cap_uah > max_inc_val)
		cl->learned_cap_uah = max_inc_val;
	else if (cl->final_cap_uah < min_dec_val)
		cl->learned_cap_uah = min_dec_val;
	else
		cl->learned_cap_uah = cl->final_cap_uah;

	if (cl->dt.max_cap_limit) {
		max_inc_val = (int64_t)cl->nom_cap_uah * (1000 +
				cl->dt.max_cap_limit);
		max_inc_val = div64_u64(max_inc_val, 1000);
		if (cl->final_cap_uah > max_inc_val) {
			pr_debug("learning capacity %lld goes above max limit %lld\n",
				cl->final_cap_uah, max_inc_val);
			cl->learned_cap_uah = max_inc_val;
		}
	}

	if (cl->dt.min_cap_limit) {
		min_dec_val = (int64_t)cl->nom_cap_uah * (1000 -
				cl->dt.min_cap_limit);
		min_dec_val = div64_u64(min_dec_val, 1000);
		if (cl->final_cap_uah < min_dec_val) {
			pr_debug("learning capacity %lld goes below min limit %lld\n",
				cl->final_cap_uah, min_dec_val);
			cl->learned_cap_uah = min_dec_val;
		}
	}

	if (cl->store_learned_capacity) {
		rc = cl->store_learned_capacity(cl->data, cl->learned_cap_uah);
		if (rc < 0)
			pr_err("Error in storing learned_cap_uah, rc=%d\n", rc);
	}

	pr_debug("final cap_uah = %lld, learned capacity %lld -> %lld uah\n",
		cl->final_cap_uah, old_cap, cl->learned_cap_uah);
}

/**
 * cap_learning_process_full_data -
 * @cl: Capacity learning object
 *
 * Processes the coulomb counter during charge termination and calculates the
 * delta w.r.to the coulomb counter obtained earlier when the learning begun.
 *
 */
static int cap_learning_process_full_data(struct cap_learning *cl)
{
	int rc, cc_soc_sw, cc_soc_delta_pct;
	int64_t delta_cap_uah;

	rc = cl->get_cc_soc(cl->data, &cc_soc_sw);
	if (rc < 0) {
		pr_err("Error in getting CC_SOC_SW, rc=%d\n", rc);
		return rc;
	}

	cc_soc_delta_pct =
		div64_s64((int64_t)(cc_soc_sw - cl->init_cc_soc_sw) * 100,
			cl->cc_soc_max);

	/* If the delta is < 50%, then skip processing full data */
	if (cc_soc_delta_pct < 50) {
		pr_err("cc_soc_delta_pct: %d\n", cc_soc_delta_pct);
		return -ERANGE;
	}

	delta_cap_uah = div64_s64(cl->learned_cap_uah * cc_soc_delta_pct, 100);
	cl->final_cap_uah = cl->init_cap_uah + delta_cap_uah;
	pr_debug("Current cc_soc=%d cc_soc_delta_pct=%d total_cap_uah=%lld\n",
		cc_soc_sw, cc_soc_delta_pct, cl->final_cap_uah);
	return 0;
}

/**
 * cap_learning_begin -
 * @cl: Capacity learning object
 * @batt_soc: Battery State of Charge (SOC)
 *
 * Gets the coulomb counter from FG/QG when the conditions are suitable for
 * beginning capacity learning. Also, primes the coulomb counter based on
 * battery SOC if required.
 *
 */
static int cap_learning_begin(struct cap_learning *cl, u32 batt_soc)
{
	int rc, cc_soc_sw, batt_soc_msb, batt_soc_pct;

	batt_soc_msb = batt_soc >> 24;
	batt_soc_pct = DIV_ROUND_CLOSEST(batt_soc_msb * 100, FULL_SOC_RAW);

	if (batt_soc_pct > cl->dt.max_start_soc ||
			batt_soc_pct < cl->dt.min_start_soc) {
		pr_debug("Battery SOC %d is high/low, not starting\n",
			batt_soc_pct);
		return -EINVAL;
	}

	cl->init_cap_uah = div64_s64(cl->learned_cap_uah * batt_soc_msb,
					FULL_SOC_RAW);

	if (cl->prime_cc_soc) {
		/*
		 * Prime cc_soc_sw with battery SOC when capacity learning
		 * begins.
		 */
		rc = cl->prime_cc_soc(cl->data, batt_soc);
		if (rc < 0) {
			pr_err("Error in writing cc_soc_sw, rc=%d\n", rc);
			goto out;
		}
	}

	rc = cl->get_cc_soc(cl->data, &cc_soc_sw);
	if (rc < 0) {
		pr_err("Error in getting CC_SOC_SW, rc=%d\n", rc);
		goto out;
	}

	cl->init_cc_soc_sw = cc_soc_sw;
	pr_debug("Capacity learning started @ battery SOC %d init_cc_soc_sw:%d\n",
		batt_soc_msb, cl->init_cc_soc_sw);
out:
	return rc;
}

/**
 * cap_learning_done -
 * @cl: Capacity learning object
 *
 * Top level function for getting coulomb counter and post processing the
 * data once the capacity learning is complete after charge termination.
 *
 */
static int cap_learning_done(struct cap_learning *cl)
{
	int rc;

	rc = cap_learning_process_full_data(cl);
	if (rc < 0) {
		pr_err("Error in processing cap learning full data, rc=%d\n",
			rc);
		goto out;
	}

	if (cl->prime_cc_soc) {
		/* Write a FULL value to cc_soc_sw */
		rc = cl->prime_cc_soc(cl->data, cl->cc_soc_max);
		if (rc < 0) {
			pr_err("Error in writing cc_soc_sw, rc=%d\n", rc);
			goto out;
		}
	}

	cap_learning_post_process(cl);
out:
	return rc;
}

/**
 * cap_learning_update -
 * @cl: Capacity learning object
 * @batt_temp - Battery temperature
 * @batt_soc: Battery State of Charge (SOC)
 * @charge_status: Charging status from power supply
 * @charge_done: Indicator for charge termination
 * @input_present: Indicator for input presence
 * @qnovo_en: Indicator for Qnovo enable status
 *
 * Called by FG/QG driver when there is a state change (Charging status, SOC)
 *
 */
void cap_learning_update(struct cap_learning *cl, int batt_temp,
			int batt_soc, int charge_status, bool charge_done,
			bool input_present, bool qnovo_en)
{
	int rc, batt_soc_msb, batt_soc_prime;
	bool prime_cc = false;

	if (!cl)
		return;

	mutex_lock(&cl->lock);

	if (batt_temp > cl->dt.max_temp || batt_temp < cl->dt.min_temp ||
		!cl->learned_cap_uah) {
		cl->active = false;
		cl->init_cap_uah = 0;
		goto out;
	}

	batt_soc_msb = (u32)batt_soc >> 24;
	pr_debug("Charge_status: %d active: %d batt_soc: %d\n",
		charge_status, cl->active, batt_soc_msb);

	/* Initialize the starting point of learning capacity */
	if (!cl->active) {
		if (charge_status == POWER_SUPPLY_STATUS_CHARGING) {
			rc = cap_learning_begin(cl, batt_soc);
			cl->active = (rc == 0);
		} else {
			if (charge_status == POWER_SUPPLY_STATUS_DISCHARGING ||
				charge_done)
				prime_cc = true;
		}
	} else {
		if (charge_done) {
			rc = cap_learning_done(cl);
			if (rc < 0)
				pr_err("Error in completing capacity learning, rc=%d\n",
					rc);

			cl->active = false;
			cl->init_cap_uah = 0;
		}

		if (charge_status == POWER_SUPPLY_STATUS_DISCHARGING) {
			if (!input_present) {
				pr_debug("Capacity learning aborted @ battery SOC %d\n",
					 batt_soc_msb);
				cl->active = false;
				cl->init_cap_uah = 0;
				prime_cc = true;
			}
		}

		if (charge_status == POWER_SUPPLY_STATUS_NOT_CHARGING) {
			if (qnovo_en && input_present) {
				/*
				 * Don't abort the capacity learning when qnovo
				 * is enabled and input is present where the
				 * charging status can go to "not charging"
				 * intermittently.
				 */
			} else {
				pr_debug("Capacity learning aborted @ battery SOC %d\n",
					batt_soc_msb);
				cl->active = false;
				cl->init_cap_uah = 0;
				prime_cc = true;
			}
		}
	}

	/*
	 * Prime CC_SOC_SW when the device is not charging or during charge
	 * termination when the capacity learning is not active.
	 */

	if (prime_cc && cl->prime_cc_soc) {
		if (charge_done)
			batt_soc_prime = cl->cc_soc_max;
		else
			batt_soc_prime = batt_soc;

		rc = cl->prime_cc_soc(cl->data, batt_soc_prime);
		if (rc < 0)
			pr_err("Error in writing cc_soc_sw, rc=%d\n",
				rc);
	}

out:
	mutex_unlock(&cl->lock);
}

/**
 * cap_learning_abort -
 * @cl: Capacity learning object
 *
 * Aborts the capacity learning and initializes variables
 *
 */
void cap_learning_abort(struct cap_learning *cl)
{
	if (!cl)
		return;

	mutex_lock(&cl->lock);
	pr_debug("Aborting cap_learning\n");
	cl->active = false;
	cl->init_cap_uah = 0;
	mutex_lock(&cl->lock);
}

/**
 * cap_learning_post_profile_init -
 * @cl: Capacity learning object
 * @nom_cap_uah: Nominal capacity of battery in uAh
 *
 * Called by FG/QG once the profile load is complete and nominal capacity
 * of battery is known. This also gets the last learned capacity back from
 * FG/QG to feed back to the algorithm.
 *
 */
int cap_learning_post_profile_init(struct cap_learning *cl, int64_t nom_cap_uah)
{
	int64_t delta_cap_uah, pct_nom_cap_uah;
	int rc;

	if (!cl || !cl->data)
		return -EINVAL;

	mutex_lock(&cl->lock);
	cl->nom_cap_uah = nom_cap_uah;
	rc = cl->get_learned_capacity(cl->data, &cl->learned_cap_uah);
	if (rc < 0) {
		pr_err("Couldn't get learned capacity, rc=%d\n", rc);
		goto out;
	}

	if (cl->learned_cap_uah != cl->nom_cap_uah) {
		if (cl->learned_cap_uah == 0)
			cl->learned_cap_uah = cl->nom_cap_uah;

		delta_cap_uah = abs(cl->learned_cap_uah - cl->nom_cap_uah);
		pct_nom_cap_uah = div64_s64((int64_t)cl->nom_cap_uah *
				CAPACITY_DELTA_DECIPCT, 1000);
		/*
		 * If the learned capacity is out of range by 50% from the
		 * nominal capacity, then overwrite the learned capacity with
		 * the nominal capacity.
		 */
		if (cl->nom_cap_uah && delta_cap_uah > pct_nom_cap_uah) {
			pr_debug("learned_cap_uah: %lld is higher than expected, capping it to nominal: %lld\n",
				cl->learned_cap_uah, cl->nom_cap_uah);
			cl->learned_cap_uah = cl->nom_cap_uah;
		}

		rc = cl->store_learned_capacity(cl->data, cl->learned_cap_uah);
		if (rc < 0)
			pr_err("Error in storing learned_cap_uah, rc=%d\n", rc);
	}

out:
	mutex_unlock(&cl->lock);
	return rc;
}

/**
 * cap_learning_init -
 * @cl: Capacity learning object
 *
 * FG/QG have to call this during driver probe to validate the required
 * parameters after allocating cap_learning object.
 *
 */
int cap_learning_init(struct cap_learning *cl)
{
	if (!cl)
		return -ENODEV;

	if (!cl->get_learned_capacity || !cl->store_learned_capacity ||
		!cl->get_cc_soc) {
		pr_err("Insufficient functions for supporting capacity learning\n");
		return -EINVAL;
	}

	if (!cl->cc_soc_max) {
		pr_err("Insufficient parameters for supporting capacity learning\n");
		return -EINVAL;
	}

	mutex_init(&cl->lock);
	return 0;
}
