// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/irq.h>
#include <linux/irqnr.h>
#include <linux/interrupt.h>

long long msec_high(unsigned long long nsec)
{
	if ((long long)nsec < 0) {
		nsec = -nsec;
		do_div(nsec, 1000000);
		return -nsec;
	}
	do_div(nsec, 1000000);

	return nsec;
}

unsigned long msec_low(unsigned long long nsec)
{
	if ((long long)nsec < 0)
		nsec = -nsec;

	return do_div(nsec, 1000000);
}

long long usec_high(unsigned long long nsec)
{
	if ((long long)nsec < 0) {
		nsec = -nsec;
		do_div(nsec, 1000);
		return -nsec;
	}
	do_div(nsec, 1000);

	return nsec;
}

long long sec_high(unsigned long long nsec)
{
	if ((long long)nsec < 0) {
		nsec = -nsec;
		do_div(nsec, 1000000000);
		return -nsec;
	}
	do_div(nsec, 1000000000);

	return nsec;
}

unsigned long sec_low(unsigned long long nsec)
{
	if ((long long)nsec < 0)
		nsec = -nsec;
	/* remove nsec partition */
	return do_div(nsec, 1000000000) / 1000;
}
