/*
 * FTS Capacitive touch screen controller (FingerTipS)
 *
 * Copyright (C) 2016-2018, STMicroelectronics Limited.
 * Authors: AMG(Analog Mems Group) <marco.cali@st.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 *
 **************************************************************************
 **                        STMicroelectronics                            **
 **************************************************************************
 **                        marco.cali@st.com                             **
 **************************************************************************
 *                                                                        *
 *                  FTS Utility for mesuring/handling the time            *
 *                                                                        *
 **************************************************************************
 **************************************************************************
 *
 */

#ifndef __FTS_TIME_H
#define __FTS_TIME_H

#include <linux/time.h>

#include "ftsCrossCompile.h"

struct StopWatch {
	struct timespec start, end;
};

void startStopWatch(struct StopWatch *w);
void stopStopWatch(struct StopWatch *w);
int elapsedMillisecond(struct StopWatch *w);
int elapsedNanosecond(struct StopWatch *w);
char *timestamp(void);
void stdelay(unsigned long ms);
#endif
