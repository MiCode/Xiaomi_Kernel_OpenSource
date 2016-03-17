/*
 ** =========================================================================
 ** File:
 **     VibeOSKernelLinuxTime.c
 **
 ** Description:
 **     Time helper functions for Linux.
 **
 ** Portions Copyright (c) 2008-2014 Immersion Corporation. All Rights Reserved.
 ** Copyright (C) 2016 XiaoMi, Inc.
 **
 ** The Original Code and all software distributed under the License are
 ** distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 ** EXPRESS OR IMPLIED, AND IMMERSION HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 ** INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
 ** FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see
 ** the License for the specific language governing rights and limitations
 ** under the License.
 ** =========================================================================
 */

/*
 ** Kernel standard software timer is used as an example but another type
 ** of timer (such as HW timer or high-resolution software timer) might be used
 ** to achieve the 5ms required rate.
 */

#if (HZ != 1000)
#error The Kernel timer is not configured at 1ms. Please update the code to use HW timer or high-resolution software timer.
#endif

#include <linux/semaphore.h>

#define WATCHDOG_TIMEOUT    10  /* 10 timer cycles */

/* Global variables */
static bool g_bTimerStarted;
static struct timer_list g_timerList;
static int g_nWatchdogCounter;

#ifndef NUM_EXTRA_BUFFERS
#define NUM_EXTRA_BUFFERS 0
#endif
struct semaphore g_hSemaphore;

/* Forward declarations */
static void VibeOSKernelLinuxStartTimer(void);
static void VibeOSKernelLinuxStopTimer(void);

static inline int VibeSemIsLocked(struct semaphore *lock)
{
#if ((LINUX_VERSION_CODE & 0xFFFFFF) < KERNEL_VERSION(2, 6, 27))
	return atomic_read(&lock->count) < 1;
#else
	return (lock->count) < 1;
#endif
}

static void VibeOSKernelTimerProc(unsigned long param)
{
	/* Return right away if timer is not supposed to run */
	if (!g_bTimerStarted)
		return;

	/* Scheduling next timeout value right away */
	if (++g_nWatchdogCounter < WATCHDOG_TIMEOUT)
		mod_timer(&g_timerList, jiffies + g_nTimerPeriodMs);

	if (VibeSemIsLocked(&g_hSemaphore))
		up(&g_hSemaphore);

	if (g_nWatchdogCounter > WATCHDOG_TIMEOUT) {
		/* Do not call SPI functions in this function as their implementation coud use interrupt */
		VibeOSKernelLinuxStopTimer();
	}
}

static int VibeOSKernelProcessData(void *data)
{
	SendOutputData();

	/* Reset watchdog counter */
	g_nWatchdogCounter = 0;

	return 0;
}

static void VibeOSKernelLinuxInitTimer(void)
{
	/* Initialize a 5ms-timer with VibeOSKernelTimerProc as timer callback */
	init_timer(&g_timerList);
	g_timerList.function = VibeOSKernelTimerProc;
}

static void VibeOSKernelLinuxStartTimer(void)
{
	/* Reset watchdog counter */
	g_nWatchdogCounter = 0;

	if (!g_bTimerStarted) {
		/* (Re-)Initialize the semaphore used with the timer */
		sema_init(&g_hSemaphore, NUM_EXTRA_BUFFERS);

		g_bTimerStarted = true;

		/* Start the timer */
		g_timerList.expires = jiffies + g_nTimerPeriodMs;
		add_timer(&g_timerList);
	} else {
		int res;

		/*
		** Use interruptible version of down to be safe
		** (try to not being stuck here if the semaphore is not freed for any reason)
		*/
		res = down_interruptible(&g_hSemaphore);  /* wait for the semaphore to be freed by the timer */
		if (res != 0)
			DbgOutInfo(("VibeOSKernelLinuxStartTimer: down_interruptible interrupted by a signal.\n"));
	}

	VibeOSKernelProcessData(NULL);
}

static void VibeOSKernelLinuxStopTimer(void)
{
	/*
	 ** Stop the timer.
	 ** The timer is not restarted when the outputdata buffer is empty and it is
	 ** automatically removed from the timer list when it expires so there is no need to
	 ** call del_timer or del_timer_sync in this function. We just mark it as stopped.
	 */
	if (g_bTimerStarted)
		g_bTimerStarted = false;

	/* Reset samples buffers */
	ResetOutputData();

	g_bIsPlaying = false;
}

static void VibeOSKernelLinuxTerminateTimer(void)
{
	VibeOSKernelLinuxStopTimer();
	del_timer_sync(&g_timerList);

	if (VibeSemIsLocked(&g_hSemaphore))
		up(&g_hSemaphore);
}
