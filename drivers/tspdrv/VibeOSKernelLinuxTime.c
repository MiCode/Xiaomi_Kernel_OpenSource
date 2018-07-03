/*
 ** =========================================================================
 ** File:
 **     VibeOSKernelLinuxTime.c
 **
 ** Description:
 **     Time helper functions for Linux.
 **
 ** Portions Copyright (c) 2008-2014 Immersion Corporation. All Rights Reserved.
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
 ** Kernel standard software timer is used as an example but another type
 ** of timer (such as HW timer or high-resolution software timer) might be used
 ** to achieve the 5ms required rate.
 */

#if (HZ != 1000)
#error The Kernel timer is not configured at 1ms. Please update the code to use HW timer or high-resolution software timer.
#endif

#include <linux/semaphore.h>

#define WATCHDOG_TIMEOUT    10

static bool g_bTimerStarted = false;
static struct timer_list g_timerList;
static int g_nWatchdogCounter = 0;

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

static void VibeOSKernelTimerProc(unsigned long param)
{
	if (!g_bTimerStarted) return;

	if (++g_nWatchdogCounter < WATCHDOG_TIMEOUT)
		mod_timer(&g_timerList, jiffies + g_nTimerPeriodMs);

	if (VibeSemIsLocked(&g_hSemaphore)) {
		up(&g_hSemaphore);
	}

	if (g_nWatchdogCounter > WATCHDOG_TIMEOUT) {
		VibeOSKernelLinuxStopTimer();
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
	init_timer(&g_timerList);
	g_timerList.function = VibeOSKernelTimerProc;
}

static void VibeOSKernelLinuxStartTimer(void)
{
	g_nWatchdogCounter = 0;

	if (!g_bTimerStarted) {
		sema_init(&g_hSemaphore, NUM_EXTRA_BUFFERS);

		g_bTimerStarted = true;

		g_timerList.expires = jiffies + g_nTimerPeriodMs;
		add_timer(&g_timerList);
	} else {
		int res;

		res = down_interruptible(&g_hSemaphore);
		if (res != 0) {
			DbgOutInfo(("VibeOSKernelLinuxStartTimer: down_interruptible interrupted by a signal.\n"));
		}
	}

	VibeOSKernelProcessData(NULL);
}

static void VibeOSKernelLinuxStopTimer(void)
{
	if (g_bTimerStarted) {
		g_bTimerStarted = false;
	}

	/* Reset samples buffers */
	ResetOutputData();

	g_bIsPlaying = false;
}

static void VibeOSKernelLinuxTerminateTimer(void)
{
	VibeOSKernelLinuxStopTimer();
	del_timer_sync(&g_timerList);

	if (VibeSemIsLocked(&g_hSemaphore)) up(&g_hSemaphore);
}
