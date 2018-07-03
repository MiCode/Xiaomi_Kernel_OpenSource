/*
 ** =========================================================================
 ** File:
 **     VibeOSKernelLinuxHRTime.c
 **
 ** Description:
 **     High Resolution Time helper functions for Linux.
 **
 ** Portions Copyright (c) 2010-2014 Immersion Corporation. All Rights Reserved.
 ** Copyright (C) 2018 XiaoMi, Inc.
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

#define WATCHDOG_TIMEOUT    10

static bool g_bTimerStarted = false;
static struct hrtimer g_tspTimer;
static ktime_t g_ktTimerPeriod;
static int g_nWatchdogCounter = 0;
static struct wake_lock g_tspWakelock;

#ifndef NUM_EXTRA_BUFFERS
#define NUM_EXTRA_BUFFERS 0
#endif
struct semaphore g_hSemaphore;

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
	if (!g_bTimerStarted) return  HRTIMER_NORESTART;

	if (++g_nWatchdogCounter < WATCHDOG_TIMEOUT)
		hrtimer_forward_now(timer, g_ktTimerPeriod);

	if (VibeSemIsLocked(&g_hSemaphore)) {
		up(&g_hSemaphore);
	}

	if (g_nWatchdogCounter < WATCHDOG_TIMEOUT) {
		return HRTIMER_RESTART;
	} else {
		VibeOSKernelLinuxStopTimer();
		return HRTIMER_NORESTART;
	}
}

static int VibeOSKernelProcessData(void* data)
{
	SendOutputData();

	g_nWatchdogCounter = 0;

	return 0;
}

static void VibeOSKernelLinuxInitTimer(void)
{
	g_ktTimerPeriod = ktime_set(0, g_nTimerPeriodMs * 1000000);

	hrtimer_init(&g_tspTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	g_tspTimer.function = VibeOSKernelTimerProc;
	wake_lock_init(&g_tspWakelock, WAKE_LOCK_SUSPEND, "tspdrv");
}

static void VibeOSKernelLinuxStartTimer(void)
{
	g_nWatchdogCounter = 0;

	if (!g_bTimerStarted) {
		wake_lock(&g_tspWakelock);
		sema_init(&g_hSemaphore, NUM_EXTRA_BUFFERS);

		g_bTimerStarted = true;

		g_ktTimerPeriod = ktime_set(0, g_nTimerPeriodMs * 1000000);
		hrtimer_start(&g_tspTimer, g_ktTimerPeriod, HRTIMER_MODE_REL);
	} else {
		int res;
		res = down_interruptible(&g_hSemaphore);
		if (res != 0) {
			DbgOutInfo(("VibeOSKernelLinuxStartTimer: down_interruptible interrupted by a signal.\n"));
		}
	}
	VibeOSKernelProcessData(NULL);
#if defined(NUM_EXTRA_BUFFERS) && (NUM_EXTRA_BUFFERS)
	if (g_bTimerStarted && !VibeSemIsLocked(&g_hSemaphore))
	{
		int res;

		res = down_interruptible(&g_hSemaphore);

		if (res != 0) {
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
