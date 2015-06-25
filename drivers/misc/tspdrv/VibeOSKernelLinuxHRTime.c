/*
 ** =========================================================================
 ** File:
 **     VibeOSKernelLinuxHRTime.c
 **
 ** Description:
 **     High Resolution Time helper functions for Linux.
 **
 ** Portions Copyright (c) 2010-2014 Immersion Corporation. All Rights Reserved.
 ** Copyright (C) 2015 XiaoMi, Inc.
 **
 ** This file contains Original Code and/or Modifications of Original Code
 ** as defined in and that are subject to the GNU Public License v2 -
 ** (the 'License'). You may not use this file except in compliance with the
 ** License. You should have received a copy of the GNU General Public License
 ** along with this program; if not, write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or contact
 ** TouchSenseSales@immersion.com.
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
 ** Kernel high-resolution software timer is used as an example but another type
 ** of timer (such as HW timer or standard software timer) might be used to achieve
 ** the 5ms required rate.
 */

#ifndef CONFIG_HIGH_RES_TIMERS
#warning "The Kernel does not have high resolution timers enabled. Either provide a non hr-timer implementation of VibeOSKernelLinuxTime.c or re-compile your kernel with CONFIG_HIGH_RES_TIMERS=y"
#endif

#include <linux/hrtimer.h>
#include <linux/semaphore.h>
#include <linux/wakelock.h>

#define WATCHDOG_TIMEOUT    10  /* 10 timer cycles */

/* Global variables */
static bool g_bTimerStarted = false;
static struct hrtimer g_tspTimer;
static ktime_t g_ktTimerPeriod; /* ktime_t equivalent of g_nTimerPeriodMs */
static int g_nWatchdogCounter = 0;
static struct wake_lock g_tspWakelock;

#ifndef NUM_EXTRA_BUFFERS
#define NUM_EXTRA_BUFFERS 0
#endif
struct semaphore g_hSemaphore;

/* Forward declarations */
static void VibeOSKernelLinuxStartTimer(void);
static void VibeOSKernelLinuxStopTimer(void);

static inline int VibeSemIsLocked(struct semaphore *lock)
{
#if ((LINUX_VERSION_CODE & 0xFFFFFF) < KERNEL_VERSION(2,6,27))
	return atomic_read(&lock->count) < 1;
#else
	return (lock->count) < 1;
#endif
}

static enum hrtimer_restart VibeOSKernelTimerProc(struct hrtimer *timer)
{
	/* Return right away if timer is not supposed to run */
	if (!g_bTimerStarted) return  HRTIMER_NORESTART;

	/* Scheduling next timeout value right away */
	if (++g_nWatchdogCounter < WATCHDOG_TIMEOUT)
		hrtimer_forward_now(timer, g_ktTimerPeriod);

	if (VibeSemIsLocked(&g_hSemaphore))
	{
		up(&g_hSemaphore);
	}

	if (g_nWatchdogCounter < WATCHDOG_TIMEOUT)
	{
		return HRTIMER_RESTART;
	}
	else
	{
		/* Do not call SPI functions in this function as their implementation coud use interrupt */
		VibeOSKernelLinuxStopTimer();
		return HRTIMER_NORESTART;
	}
}

static int VibeOSKernelProcessData(void* data)
{
	SendOutputData();

	/* Reset watchdog counter */
	g_nWatchdogCounter = 0;

	return 0;
}

static void VibeOSKernelLinuxInitTimer(void)
{
	/* Convert the initial timer period in ktime_t value */
	g_ktTimerPeriod = ktime_set(0, g_nTimerPeriodMs * 1000000);

	hrtimer_init(&g_tspTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	/* Initialize a 5ms-timer with VibeOSKernelTimerProc as timer callback (interrupt driven)*/
	g_tspTimer.function = VibeOSKernelTimerProc;
	wake_lock_init(&g_tspWakelock, WAKE_LOCK_SUSPEND, "tspdrv");
}

static void VibeOSKernelLinuxStartTimer(void)
{
	/* Reset watchdog counter */
	g_nWatchdogCounter = 0;

	if (!g_bTimerStarted)
	{
		wake_lock(&g_tspWakelock);
		/* (Re-)Initialize the semaphore used with the timer */
		sema_init(&g_hSemaphore, NUM_EXTRA_BUFFERS);

		g_bTimerStarted = true;

		/* Start the timer */
		g_ktTimerPeriod = ktime_set(0, g_nTimerPeriodMs * 1000000);
		hrtimer_start(&g_tspTimer, g_ktTimerPeriod, HRTIMER_MODE_REL);
	}
	else
	{
		int res;
		/*
		 ** Use interruptible version of down to be safe
		 ** (try to not being stuck here if the semaphore is not freed for any reason)
		 */
		res = down_interruptible(&g_hSemaphore);  /* wait for the semaphore to be freed by the timer */
		if (res != 0)
		{
			DbgOutInfo(("VibeOSKernelLinuxStartTimer: down_interruptible interrupted by a signal.\n"));
		}
	}
	VibeOSKernelProcessData(NULL);
	/*
	 ** Because of possible NACK handling, the  VibeOSKernelProcessData() call above could take more than
	 ** 5 ms on some piezo devices that are buffering output samples; when this happens, the timer
	 ** interrupt will release the g_hSemaphore while VibeOSKernelProcessData is executing and the player
	 ** will immediately send the new packet to the SPI layer when VibeOSKernelProcessData exits, which
	 ** could cause another NACK right away. To avoid that, we'll create a small delay if the semaphore
	 ** was released when VibeOSKernelProcessData exits, by acquiring the mutex again and waiting for
	 ** the timer to release it.
	 */
#if defined(NUM_EXTRA_BUFFERS) && (NUM_EXTRA_BUFFERS)
	if (g_bTimerStarted && !VibeSemIsLocked(&g_hSemaphore))
	{
		int res;

		res = down_interruptible(&g_hSemaphore);

		if (res != 0)
		{
			DbgOutInfo(("VibeOSKernelLinuxStartTimer: down_interruptible interrupted by a signal.\n"));
		}
	}
#endif
}

static void VibeOSKernelLinuxStopTimer(void)
{
	if (g_bTimerStarted)
	{
		g_bTimerStarted = false;
	}

	/* Reset samples buffers */
	ResetOutputData();

	g_bIsPlaying = false;
	wake_unlock(&g_tspWakelock);
}

static void VibeOSKernelLinuxTerminateTimer(void)
{
	VibeOSKernelLinuxStopTimer();
	hrtimer_cancel(&g_tspTimer);
	wake_lock_destroy(&g_tspWakelock);

	if (VibeSemIsLocked(&g_hSemaphore)) up(&g_hSemaphore);
}
