/*
 * Copyright (C) 2015 MediaTek Inc.
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

#include <linux/irq.h>
#include <linux/irqnr.h>
#include <linux/interrupt.h>
#include "internal.h"

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
