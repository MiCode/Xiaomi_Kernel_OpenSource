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

#include "mt2712_yheader.h"

/* \brief API to adjust the frequency of hardware clock.
 *
 * \details This function is used to adjust the frequency of the
 * hardware clock.
 *
 * \param[in] ptp – pointer to ptp_clock_info structure.
 * \param[in] delta – desired period change in parts per billion.
 *
 * \return int
 *
 * \retval 0 on success and -ve number on failure.
 */
static int adjust_freq(struct ptp_clock_info *ptp, s32 ppb)
{
	struct prv_data *pdata =
		container_of(ptp, struct prv_data, ptp_clock_ops);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	unsigned long flags;
	u64 adj;
	u64 diff, addend;
	int neg_adj = 0;

	if (ppb < 0) {
		neg_adj = 1;
		ppb = -ppb;
	}

	addend = pdata->default_addend;
	adj = addend;
	adj *= ppb;
	/* div_u64 will divided the "adj" by "1000000000ULL"
	 * and return the quotient.
	 */
	diff = div_u64(adj, 1000000000ULL);
	addend = neg_adj ? (addend - diff) : (addend + diff);
	spin_lock_irqsave(&pdata->ptp_lock, flags);

	hw_if->config_addend(addend);

	spin_unlock_irqrestore(&pdata->ptp_lock, flags);

	return 0;
}

/* \brief API to adjust the hardware time.
 *
 * \details This function is used to shift/adjust the time of the
 * hardware clock.
 *
 * \param[in] ptp – pointer to ptp_clock_info structure.
 * \param[in] delta – desired change in nanoseconds.
 *
 * \return int
 *
 * \retval 0 on success and -ve number on failure.
 */
static int adjust_time(struct ptp_clock_info *ptp, s64 delta)
{
	struct prv_data *pdata =
		container_of(ptp, struct prv_data, ptp_clock_ops);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	unsigned long flags;
	u32 sec, nsec;
	u32 quotient, reminder;
	int neg_adj = 0;

	if (delta < 0) {
		neg_adj = 1;
		delta = -delta;
	}

	quotient = div_u64_rem(delta, 1000000000ULL, &reminder);
	sec = quotient;
	nsec = reminder;

	spin_lock_irqsave(&pdata->ptp_lock, flags);

	pr_err("-->adjust_time: delta = %lld sec=%d nsec=%d\n", delta, sec, nsec);
	hw_if->adjust_systime(sec, nsec, neg_adj, pdata->one_nsec_accuracy);

	spin_unlock_irqrestore(&pdata->ptp_lock, flags);

	return 0;
}

/* \brief API to get the current time.
 *
 * \details This function is used to read the current time from the
 * hardware clock.
 *
 * \param[in] ptp – pointer to ptp_clock_info structure.
 * \param[in] ts – pointer to hold the time/result.
 *
 * \return int
 *
 * \retval 0 on success and -ve number on failure.
 */
static int get_time(struct ptp_clock_info *ptp, struct timespec64 *ts)
{
	struct prv_data *pdata =
		container_of(ptp, struct prv_data, ptp_clock_ops);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	u64 ns;
	u32 reminder;
	unsigned long flags;

	spin_lock_irqsave(&pdata->ptp_lock, flags);

	ns = hw_if->get_systime();

	spin_unlock_irqrestore(&pdata->ptp_lock, flags);

	ts->tv_sec = div_u64_rem(ns, 1000000000ULL, &reminder);
	ts->tv_nsec = reminder;

	return 0;
}

/* \brief API to set the current time.
 *
 * \details This function is used to set the current time on the
 * hardware clock.
 *
 * \param[in] ptp – pointer to ptp_clock_info structure.
 * \param[in] ts – time value to set.
 *
 * \return int
 *
 * \retval 0 on success and -ve number on failure.
 */
static int set_time(struct ptp_clock_info *ptp, const struct timespec64 *ts)
{
	struct prv_data *pdata =
		container_of(ptp, struct prv_data, ptp_clock_ops);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	unsigned long flags;

	spin_lock_irqsave(&pdata->ptp_lock, flags);

	hw_if->init_systime(ts->tv_sec, ts->tv_nsec);

	spin_unlock_irqrestore(&pdata->ptp_lock, flags);

	return 0;
}

/* \brief API to enable/disable an ancillary feature.
 *
 * \details This function is used to enable or disable an ancillary
 * device feature like PPS, PEROUT and EXTTS.
 *
 * \param[in] ptp – pointer to ptp_clock_info structure.
 * \param[in] rq – desired resource to enable or disable.
 * \param[in] on – caller passes one to enable or zero to disable.
 *
 * \return int
 *
 * \retval 0 on success and -ve(EINVAL or EOPNOTSUPP) number on failure.
 */
static int enable(struct ptp_clock_info *ptp, struct ptp_clock_request *rq, int on)
{
	return -EOPNOTSUPP;
}

/* structure describing a PTP hardware clock. */
static struct ptp_clock_info ptp_clock_ops = {
	.owner = THIS_MODULE,
	.name = "clk",
	.max_adj = SYSCLOCK, /* the max possible frequency adjustment, in parts per billion */
	.n_alarm = 0,	/* the number of programmable alarms */
	.n_ext_ts = 0,	/* the number of externel time stamp channels */
	.n_per_out = 0, /* the number of programmable periodic signals */
	.pps = 0,	/* indicates whether the clk supports a PPS callback */
	.adjfreq = adjust_freq,
	.adjtime = adjust_time,
	.gettime64 = get_time,
	.settime64 = set_time,
	.enable = enable,
};

/* \brief API to register ptp clock driver.
 *
 * \details This function is used to register the ptp clock
 * driver to kernel. It also does some housekeeping work.
 *
 * \param[in] pdata – pointer to private data structure.
 *
 * \return int
 *
 * \retval 0 on success and -ve number on failure.
 */
int ptp_init(struct prv_data *pdata)
{
	int ret = 0;

	if (!pdata->hw_feat.tsstssel) {
		ret = -1;
		pdata->ptp_clock = NULL;
		pr_err("No PTP supports in HW\n"
			"Aborting PTP clock driver registration\n");
		goto no_hw_ptp;
	}

	spin_lock_init(&pdata->ptp_lock);

	pdata->ptp_clock_ops = ptp_clock_ops;

	pdata->ptp_clock = ptp_clock_register(&pdata->ptp_clock_ops,
					      &pdata->pdev->dev);

	if (IS_ERR(pdata->ptp_clock)) {
		pdata->ptp_clock = NULL;
		pr_err("ptp_clock_register() failed\n");
	} else {
		pr_err("Added PTP HW clock successfully\n");
	}

	return ret;

no_hw_ptp:
	return ret;
}

/* \brief API to unregister ptp clock driver.
 *
 * \details This function is used to remove/unregister the ptp
 * clock driver from the kernel.
 *
 * \param[in] pdata – pointer to private data structure.
 *
 * \return void
 */
void ptp_remove(struct prv_data *pdata)
{
	if (pdata->ptp_clock) {
		ptp_clock_unregister(pdata->ptp_clock);
		pr_err("Removed PTP HW clock successfully\n");
	}
}
