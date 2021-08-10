/*

**************************************************************************
**                        STMicroelectronics							**
**************************************************************************
**                        marco.cali@st.com								**
**************************************************************************
*                                                                        *
*                  FTS Utility for mesuring/handling the time			 *
*                                                                        *
**************************************************************************
**************************************************************************

*/

/*!
* \file ftsTime.c
* \brief Contains all functions to handle and measure the time in the driver
*/

#include "ftsTime.h"

#include <linux/errno.h>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <stdarg.h>
#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <linux/ctype.h>

/**
* Take the starting time and save it in a StopWatch variable
* @param w pointer of a StopWatch struct
*/
void startStopWatch(StopWatch *w)
{
	w->start = ktime_get();
}

/**
* Take the stop time and save it in a StopWatch variable
* @param w pointer of a StopWatch struct
*/
void stopStopWatch(StopWatch *w)
{
	w->end = ktime_get();
}

/**
* Compute the amount of time spent from when the startStopWatch and then the stopStopWatch were called on the StopWatch variable
* @param w pointer of a StopWatch struct
* @return amount of time in ms (the return value is meaningless if the startStopWatch and stopStopWatch were not called before)
*/
int elapsedMillisecond(StopWatch *w)
{
	u64 result;

	result = ktime_us_delta(w->end, w->start);

	return (int)(result / 1000);
}

