#define SII_OSAL_LINUX_TIMER_C
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include "sii_hal.h"
#include "osal/include/osal.h"
#define MSEC_TO_NSEC(x)		(x * 1000000UL)
#define MAX_TIMER_NAME_LEN		16
typedef struct _SiiOsTimerInfo_t {
	struct list_head listEntry;
	struct work_struct workItem;
	uint8_t flags;
	char timerName[MAX_TIMER_NAME_LEN];
	struct hrtimer hrTimer;
	timerCallbackHandler_t callbackHandler;
	void *callbackParam;
	uint32_t timeMsec;
	bool bPeriodic;
} timerObject_t;
#define TIMER_OBJ_FLAG_WORK_IP	0x01
#define TIMER_OBJ_FLAG_DEL_REQ	0x02
static struct list_head timerList;
static struct workqueue_struct *timerWorkQueue;
static void WorkHandler(struct work_struct *work)
{
	timerObject_t *pTimerObj = container_of(work, timerObject_t, workItem);
	pTimerObj->flags |= TIMER_OBJ_FLAG_WORK_IP;
	if (HalAcquireIsrLock() == HAL_RET_SUCCESS) {
		if (pTimerObj->callbackHandler)
			(pTimerObj->callbackHandler) (pTimerObj->callbackParam);
		HalReleaseIsrLock();
	}
	pTimerObj->flags &= ~TIMER_OBJ_FLAG_WORK_IP;
	if (pTimerObj->flags & TIMER_OBJ_FLAG_DEL_REQ) {
		kfree(pTimerObj);
	}
}

static enum hrtimer_restart TimerHandler(struct hrtimer *timer)
{
	timerObject_t *pTimerObj = container_of(timer, timerObject_t, hrTimer);
	ktime_t timerPeriod;
	queue_work(timerWorkQueue, &pTimerObj->workItem);
	if (pTimerObj->bPeriodic) {
		timerPeriod = ktime_set(0, MSEC_TO_NSEC(pTimerObj->timeMsec));
		hrtimer_forward(&pTimerObj->hrTimer,
				pTimerObj->hrTimer.base->get_time(), timerPeriod);
		return HRTIMER_RESTART;
	}
	return HRTIMER_NORESTART;
}

SiiOsStatus_t SiiOsInit(uint32_t maxChannels)
{
	INIT_LIST_HEAD(&timerList);
	timerWorkQueue = create_workqueue("Sii_timer_work");
	if (timerWorkQueue == NULL) {
		return SII_OS_STATUS_ERR_NOT_AVAIL;
	}
	return SII_OS_STATUS_SUCCESS;
}

SiiOsStatus_t SiiOsTerm(void)
{
	timerObject_t *timerObj;
	int status;
	while (!list_empty(&timerList)) {
		timerObj = list_first_entry(&timerList, timerObject_t, listEntry);
		status = hrtimer_try_to_cancel(&timerObj->hrTimer);
		if (status >= 0) {
			list_del(&timerObj->listEntry);
			kfree(timerObj);
		}
	}
	flush_workqueue(timerWorkQueue);
	destroy_workqueue(timerWorkQueue);
	timerWorkQueue = NULL;
	return SII_OS_STATUS_SUCCESS;
}

SiiOsStatus_t SiiOsTimerCreate(const char *pName, void (*pTimerFunction) (void *pArg),
			       void *pTimerArg, bool_t timerStartFlag,
			       uint32_t timeMsec, bool_t periodicFlag, SiiOsTimer_t *pTimerId)
{
	timerObject_t *timerObj;
	SiiOsStatus_t status = SII_OS_STATUS_SUCCESS;
	if (pTimerFunction == NULL) {
		return SII_OS_STATUS_ERR_INVALID_PARAM;
	}
	timerObj = kmalloc(sizeof(timerObject_t), GFP_KERNEL);
	if (timerObj == NULL) {
		return SII_OS_STATUS_ERR_NOT_AVAIL;
	}
	strncpy(timerObj->timerName, pName, MAX_TIMER_NAME_LEN - 1);
	timerObj->timerName[MAX_TIMER_NAME_LEN - 1] = 0;
	timerObj->callbackHandler = pTimerFunction;
	timerObj->callbackParam = pTimerArg;
	timerObj->timeMsec = timeMsec;
	timerObj->bPeriodic = periodicFlag;
	INIT_WORK(&timerObj->workItem, WorkHandler);
	list_add(&timerObj->listEntry, &timerList);
	hrtimer_init(&timerObj->hrTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	timerObj->hrTimer.function = TimerHandler;
	if (timerStartFlag) {
		status = SiiOsTimerSchedule(timerObj, timeMsec);
	}
	*pTimerId = timerObj;
	return status;
}

SiiOsStatus_t SiiOsTimerDelete(SiiOsTimer_t timerId)
{
	timerObject_t *timerObj;
	list_for_each_entry(timerObj, &timerList, listEntry) {
		if (timerObj == timerId) {
			break;
		}
	}
	if (timerObj != timerId) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"Invalid timerId %p passed to SiiOsTimerDelete\n", timerId);
		return SII_OS_STATUS_ERR_INVALID_PARAM;
	}
	list_del(&timerObj->listEntry);
	hrtimer_cancel(&timerObj->hrTimer);
	if (timerObj->flags & TIMER_OBJ_FLAG_WORK_IP) {
		timerObj->flags |= TIMER_OBJ_FLAG_DEL_REQ;
		return SII_OS_STATUS_SUCCESS;
	}
	cancel_work_sync(&timerObj->workItem);
	kfree(timerObj);
	return SII_OS_STATUS_SUCCESS;
}

SiiOsStatus_t SiiOsTimerSchedule(SiiOsTimer_t timerId, uint32_t timeMsec)
{
	timerObject_t *timerObj;
	ktime_t timerPeriod;
	list_for_each_entry(timerObj, &timerList, listEntry) {
		if (timerObj == timerId) {
			break;
		}
	}
	if (timerObj != timerId) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"Invalid timerId %p passed to SiiOsTimerSchedule\n", timerId);
		return SII_OS_STATUS_ERR_INVALID_PARAM;
	}
	timerPeriod = ktime_set(0, MSEC_TO_NSEC(timeMsec));
	hrtimer_start(&timerObj->hrTimer, timerPeriod, HRTIMER_MODE_REL);
	return SII_OS_STATUS_SUCCESS;
}

uint32_t SiiOsGetTimeResolution(void)
{
	return 1;
}
