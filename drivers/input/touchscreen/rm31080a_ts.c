/*
 * Raydium RM31080 touchscreen driver
 *
 * Copyright (C) 2012-2013, Raydium Semiconductor Corporation.
 * All Rights Reserved.
 * Copyright (C) 2012-2013, NVIDIA Corporation.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 */
/*=============================================================================
	INCLUDED FILES
=============================================================================*/
#include <linux/module.h>
#include <linux/input.h>	/* BUS_SPI */
#include <linux/spi/spi.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/sched.h>	/* wake_up_process() */
#include <linux/kthread.h>	/* kthread_create(),kthread_run() */
#include <linux/uaccess.h>	/* copy_to_user() */
#include <linux/miscdevice.h>
#include <asm/siginfo.h>	/* siginfo */
#include <linux/rcupdate.h>	/* rcu_read_lock */
#include <linux/sched.h>	/* find_task_by_pid_type */
#include <linux/syscalls.h>	/* sys_clock_gettime() */
#include <linux/random.h>	/* random32() */
#include <linux/suspend.h>	/* pm_notifier */
#include <linux/workqueue.h>
#include <linux/wakelock.h> /* wakelock */
#include <linux/regulator/consumer.h> /* regulator & voltage */
#include <linux/clk.h> /* clock */
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <linux/timer.h>

#include <linux/spi/rm31080a_ts.h>
#include <linux/spi/rm31080a_ctrl.h>

#define CREATE_TRACE_POINTS
#include <trace/events/touchscreen_raydium.h>

/*=============================================================================
	DEFINITIONS
=============================================================================*/
/*#define ENABLE_CALC_QUEUE_COUNT*/
#define ENABLE_SLOW_SCAN
#define ENABLE_SMOOTH_LEVEL
#define ENABLE_SPI_SETTING		0

#define MAX_SPI_FREQ_HZ			50000000
#define TS_PEN_UP_TIMEOUT		msecs_to_jiffies(50)

#define QUEUE_COUNT				128
#define RM_RAW_DATA_LENGTH			6144

#define RM_SCAN_ACTIVE_MODE			0x00
#define RM_SCAN_PRE_IDLE_MODE		0x01
#define RM_SCAN_IDLE_MODE			0x02

#define RM_NEED_NONE					0x00
#define RM_NEED_TO_SEND_SCAN			0x01
#define RM_NEED_TO_READ_RAW_DATA		0x02
#define RM_NEED_TO_SEND_SIGNAL			0x04

#define TCH_WAKE_LOCK_TIMEOUT		(HZ/2)

#ifdef ENABLE_SLOW_SCAN
#define RM_SLOW_SCAN_INTERVAL				20
#define RM_SLOW_SCAN_CMD_COUNT				0x10

enum RM_SLOW_SCAN_LEVELS {
	RM_SLOW_SCAN_LEVEL_NORMAL,
	RM_SLOW_SCAN_LEVEL_20,
	RM_SLOW_SCAN_LEVEL_40,
	RM_SLOW_SCAN_LEVEL_60,
	RM_SLOW_SCAN_LEVEL_80,
	RM_SLOW_SCAN_LEVEL_100,
	RM_SLOW_SCAN_LEVEL_MAX,
	RM_SLOW_SCAN_LEVEL_COUNT
};
#endif

#ifdef ENABLE_SMOOTH_LEVEL
#define RM_SMOOTH_LEVEL_NORMAL		0
#define RM_SMOOTH_LEVEL_MAX			4
#endif

#define TS_TIMER_PERIOD		HZ

#define INPUT_DEVICE_MULTIPLE_INSTANCES
#ifdef INPUT_DEVICE_MULTIPLE_INSTANCES
#define INPUT_DEVICE_AMOUNT	2

#define INPUT_DEVICE_FOR_FINGER	0
#define INPUT_DEVICE_FOR_STYLUS	1
#endif

/* do not use printk in kernel files */
#define rm_printk(msg...)	dev_info(&g_spi->dev, msg)

/*=============================================================================
	STRUCTURE DECLARATION
=============================================================================*/
/*TouchScreen Parameters*/
struct rm31080a_ts_para {
	unsigned long ulHalPID;
	bool bInitFinish;
	bool bCalcFinish;
	bool bEnableScriber;
	bool bIsSuspended;

	u32 u32WatchDogCnt;
	u8 u8WatchDogFlg;
	u8 u8WatchDogEnable;
	bool u8WatchDogCheck;
	u32 u32WatchDogTime;

	u8 u8ScanModeState;
	u8 u8ReportMode;

	u8 u8ResumeCnt;

#ifdef ENABLE_SLOW_SCAN
	bool bEnableSlowScan;
	u32 u32SlowScanLevel;
#endif

#ifdef ENABLE_SMOOTH_LEVEL
	u32 u32SmoothLevel;
#endif

	u8 u8SelfTestStatus;
	u8 u8SelfTestResult;
	u8 u8Version;
	u8 u8TestVersion;
	u8 u8Repeat;
	u16 u16ReadPara;
	u8 u8SPILocked;
	u8 bSWCPW[3];
	u8 bDP[3];
	u8 bSWC[3];
	u8 bNsMode;
	u8 bNsSel;
	u8 bNsRpt;
	struct wake_lock Wakelock_Initialization;

	struct mutex mutex_scan_mode;
	struct mutex mutex_spi_rw;
	struct mutex mutex_ns_mode;

	struct workqueue_struct *rm_workqueue;
	struct work_struct rm_work;

	struct workqueue_struct *rm_timer_workqueue;
	struct work_struct rm_timer_work;

};

struct rm_tch_ts {
	const struct rm_tch_bus_ops *bops;
	struct device *dev;
#ifndef INPUT_DEVICE_MULTIPLE_INSTANCES
	struct input_dev *input;
#else
	struct input_dev *input[INPUT_DEVICE_AMOUNT];
#endif
	unsigned int irq;
	char phys[32];
	struct mutex access_mutex;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	struct regulator *regulator_3v3;
	struct regulator *regulator_1v8;
	struct notifier_block nb_3v3;
	struct notifier_block nb_1v8;
	struct clk *clk;
	unsigned char bRepeatCounter;
};

struct rm_tch_bus_ops {
	u16 bustype;
	int (*read) (struct device *dev, u8 reg);
	int (*write) (struct device *dev, u8 reg, u16 val);
};

struct rm_tch_queue_info {
	u8(*pQueue)[RM_RAW_DATA_LENGTH];
	u16 u16Front;
	u16 u16Rear;
};

/*=============================================================================
	GLOBAL VARIABLES DECLARATION
=============================================================================*/
#ifndef INPUT_DEVICE_MULTIPLE_INSTANCES
struct input_dev *g_input_dev;
#else
struct input_dev *g_input_dev[INPUT_DEVICE_AMOUNT];
#endif
struct spi_device *g_spi;
struct rm31080a_ts_para g_stTs;
bool g_is_flush;

struct rm_tch_queue_info g_stQ;

unsigned char *g_pu8BurstReadBuf;

unsigned char g_stCmdSetIdle[KRL_SIZE_SET_IDLE];
unsigned char g_stCmdPauseAuto[KRL_SIZE_PAUSE_AUTO];
unsigned char g_stRmResumeCmd[KRL_SIZE_RM_RESUME];
unsigned char g_stRmSuspendCmd[KRL_SIZE_RM_SUSPEND];
unsigned char g_stRmReadImgCmd[KRL_SIZE_RM_READ_IMG];
unsigned char g_stRmWatchdogCmd[KRL_SIZE_RM_WATCHDOG];
unsigned char g_stRmTestModeCmd[KRL_SIZE_RM_TESTMODE];
unsigned char g_stRmSlowScanCmd[KRL_SIZE_RM_SLOWSCAN];
unsigned char g_stRmClearIntCmd[KRL_SIZE_RM_CLEARINT];
unsigned char g_stRmScanStartCmd[KRL_SIZE_RM_SCANSTART];
unsigned char g_stRmWaitScanOKCmd[KRL_SIZE_RM_WAITSCANOK];
unsigned char g_stRmSetRepTimeCmd[KRL_SIZE_RM_SETREPTIME];
unsigned char g_stRmNsParaCmd[KRL_SIZE_RM_NS_PARA];
unsigned char g_stRmSlowScanBCmd[KRL_SIZE_RM_SLOWSCANB];

int g_service_busy_report_count;
struct timer_list ts_timer_triggle;
static unsigned char spi_buf[4096];
static unsigned int bufsize; /*= 0; remove by checkpatch*/
/*=============================================================================
	FUNCTION DECLARATION
=============================================================================*/
static int rm_tch_cmd_process(u8 selCase, u8 *pCmdTbl, struct rm_tch_ts *ts);
static int rm_tch_read_image_data(unsigned char *p);
#ifdef CONFIG_HAS_EARLYSUSPEND
static void rm_tch_early_suspend(struct early_suspend *es);
static void rm_tch_early_resume(struct early_suspend *es);
#endif
static int rm_tch_ts_send_signal(int pid, int iInfo);

static void rm_tch_enter_test_mode(u8 flag);
static void rm_ctrl_suspend(struct rm_tch_ts *ts);
static void rm_ctrl_resume(struct rm_tch_ts *ts);
static void rm_ctrl_watchdog_func(unsigned int u32Enable);

static void init_ts_timer(void);
static void ts_timer_triggle_function(unsigned long option);

/*=============================================================================
	 Description:
			RM31080 spi interface.
	 Input:

	 Output:
			1:succeed
			0:failed
=============================================================================*/
static int rm_tch_spi_read(u8 u8addr, u8 *rxbuf, size_t len)
{
	int status;
	struct spi_message message;
	struct spi_transfer x[2];

	if (g_stTs.u8SPILocked) {
		memset(rxbuf, 0, len);
		if (g_stCtrl.bKernelMsg & DEBUG_DRIVER_REGISTER)
			rm_printk("Raydium - SPI Read Locked!! 0x%x:%d\n",
				u8addr, len);
		return OK;
	}

	mutex_lock(&g_stTs.mutex_spi_rw);

	spi_message_init(&message);
	memset(x, 0, sizeof(x));

	u8addr |= 0x80;
	x[0].len = 1;
	x[0].tx_buf = &u8addr;
	spi_message_add_tail(&x[0], &message);

	x[1].len = len;
	x[1].rx_buf = rxbuf;
	spi_message_add_tail(&x[1], &message);

	/*It returns zero on succcess,else a negative error code.*/
	status = spi_sync(g_spi, &message);

	mutex_unlock(&g_stTs.mutex_spi_rw);

	if (g_stCtrl.bKernelMsg & DEBUG_DRIVER_REGISTER)
		if (g_stTs.bInitFinish == 0)
			rm_printk("Raydium - READ: addr=0x%2x, value=0x%2x",
				(u8addr&0x7F), rxbuf[0]);

	if (status) {
		dev_err(&g_spi->dev, "Raydium - %s : spi_async failed - error %d\n",
			__func__, status);
		return FAIL;
	}

	return OK;
}

/*=============================================================================
	 Description:
			RM31080 spi interface.
	 Input:

	 Output:
			1:succeed
			0:failed
=============================================================================*/
static int rm_tch_spi_write(u8 *txbuf, size_t len)
{
	int status;
	/*It returns zero on succcess,else a negative error code.*/

	if (g_stTs.u8SPILocked) {
		if (g_stCtrl.bKernelMsg & DEBUG_DRIVER_REGISTER)
			rm_printk("Raydium - SPI write Locked!! 0x%x:0x%x\n",
				txbuf[0], txbuf[1]);
		return OK;
	}

	mutex_lock(&g_stTs.mutex_spi_rw);

	status = spi_write(g_spi, txbuf, len);

	mutex_unlock(&g_stTs.mutex_spi_rw);

	if (g_stCtrl.bKernelMsg & DEBUG_DRIVER_REGISTER)
		if (g_stTs.bInitFinish == 0)
			rm_printk("Raydium - WRITE: addr=0x%2x, value=0x%2x",
				txbuf[0], txbuf[1]);

	if (status) {
		dev_err(&g_spi->dev, "Raydium - %s : spi_write failed - error %d\n",
			__func__, status);
		return FAIL;
	}

	return OK;
}

/*=============================================================================
	 Description:
			RM31080 spi interface.
	 Input:

	 Output:
			1:succeed
			0:failed
=============================================================================*/
int rm_tch_spi_byte_read(unsigned char u8Addr, unsigned char *pu8Value)
{
	return rm_tch_spi_read(u8Addr, pu8Value, 1);
}

/*=============================================================================
	 Description:
			RM31080 spi interface.
	 Input:

	 Output:
			1:succeed
			0:failed
=============================================================================*/
int rm_tch_spi_byte_write(unsigned char u8Addr, unsigned char u8Value)
{
	u8 buf[2];
	buf[0] = u8Addr;
	buf[1] = u8Value;
	return rm_tch_spi_write(buf, 2);
}

/*=============================================================================
	 Description:
			RM31080 spi interface.
	 Input:

	 Output:
			1:succeed
			0:failed
=============================================================================*/
int rm_tch_spi_burst_read(unsigned char u8Addr, unsigned char *pu8Value,
	unsigned int u32Len)
{
	int ret;
	u8 *pMyBuf;
	ssize_t missing;

	pMyBuf = kmalloc(u32Len, GFP_KERNEL);
	if (pMyBuf == NULL)
		return -ENOMEM;

	ret = rm_tch_spi_read(u8Addr, pMyBuf, u32Len);

	if (ret) {
		missing = copy_to_user(pu8Value, pMyBuf, u32Len);
		if (missing) {
			dev_err(&g_spi->dev, "Raydium - %s : copy failed - len:%d, miss:%d\n",
				__func__, u32Len, missing);
			kfree(pMyBuf);
			return FAIL;
		}
	}

	kfree(pMyBuf);

	return ret;
}
/*=============================================================================
	 Description:
			RM31080 spi interface.
	 Input:

	 Output:
			1:succeed
			0:failed
=============================================================================*/
int rm_tch_spi_burst_write(unsigned char *pBuf, unsigned int u32Len)
{
	u8 *pMyBuf;
	int ret;
	ssize_t missing;

	pMyBuf = kmalloc(u32Len, GFP_KERNEL);
	if (pMyBuf == NULL)
		return -ENOMEM;

	missing = copy_from_user(pMyBuf, pBuf, u32Len);
	if (missing) {
		dev_err(&g_spi->dev, "Raydium - %s : copy failed - len:%d, miss:%d\n",
			__func__, u32Len, missing);
		kfree(pMyBuf);
		return FAIL;
	}

	ret = rm_tch_spi_write(pMyBuf, u32Len);
	kfree(pMyBuf);
	return ret;
}

/*===========================================================================*/
void raydium_change_scan_mode(u8 u8TouchCount)
{
	static u32 u32NoTouchCount; /*= 0; remove by checkpatch*/
	u16 u16NTCountThd;

	u16NTCountThd = (u16)g_stCtrl.bTime2Idle * 100;

	if (u8TouchCount) {
		u32NoTouchCount = 0;
		return;
	}

	mutex_lock(&g_stTs.mutex_scan_mode);
	if (u32NoTouchCount < u16NTCountThd) {
		u32NoTouchCount++;
	} else if (g_stTs.u8ScanModeState == RM_SCAN_ACTIVE_MODE) {
		g_stTs.u8ScanModeState = RM_SCAN_PRE_IDLE_MODE;
		u32NoTouchCount = 0;
	}
	mutex_unlock(&g_stTs.mutex_scan_mode);
}

void raydium_report_pointer(void *p)
{
	static unsigned char ucLastTouchCount; /*= 0; remove by checkpatch*/
#ifdef INPUT_DEVICE_MULTIPLE_INSTANCES
	u8 target_device = INPUT_DEVICE_FOR_FINGER;
#endif
	int i;
	int iCount;
	int iMaxX, iMaxY;
	struct rm_touch_event *spTP;
	ssize_t missing;

	spTP = kmalloc(sizeof(struct rm_touch_event), GFP_KERNEL);
	if (spTP == NULL)
		return;

	missing = copy_from_user(spTP, p, sizeof(struct rm_touch_event));
	if (missing) {
		dev_err(&g_spi->dev, "Raydium - %s : copy failed - len:%d, miss:%d\n",
			__func__, sizeof(struct rm_touch_event), missing);
		kfree(spTP);
		return;
	}

	if ((g_stCtrl.u16ResolutionX != 0) && (g_stCtrl.u16ResolutionY != 0)) {
		iMaxX = g_stCtrl.u16ResolutionX;
		iMaxY = g_stCtrl.u16ResolutionY;
	} else {
		iMaxX = RM_INPUT_RESOLUTION_X;
		iMaxY = RM_INPUT_RESOLUTION_Y;
	}

	iCount = max(ucLastTouchCount, spTP->ucTouchCount);
#ifndef INPUT_DEVICE_MULTIPLE_INSTANCES
	if (iCount && !spTP->ucTouchCount) {
		ucLastTouchCount = 0;
		input_report_key(g_input_dev,
			BTN_TOOL_RUBBER, 0);
		input_mt_sync(g_input_dev);
		input_sync(g_input_dev);
	} else if (iCount) {
		for (i = 0; i < iCount; i++) {
			if (i == MAX_REPORT_TOUCHED_POINTS)
				break;

			if ((spTP->ucToolType[i] == POINT_TYPE_ERASER)
					&& (spTP->ucSlot[i] & INPUT_POINT_RESET)) {
				input_report_key(g_input_dev,
					BTN_TOOL_RUBBER, 0);
				input_mt_sync(g_input_dev);
				continue;
			}

			if (i < spTP->ucTouchCount) {
				switch (spTP->ucToolType[i]) {
				case POINT_TYPE_FINGER:
					input_report_abs(g_input_dev,
						ABS_MT_TOOL_TYPE,
						MT_TOOL_FINGER);
					break;
				case POINT_TYPE_STYLUS:
					input_report_abs(g_input_dev,
							ABS_MT_TOOL_TYPE,
							MT_TOOL_PEN);
					break;
				case POINT_TYPE_ERASER:
					input_report_key(g_input_dev,
						BTN_TOOL_RUBBER, 1);
					break;
				default:
					dev_err(&g_spi->dev,
						"Raydium - point %d is invalid input tool type: %d\n",
						i, spTP->ucToolType[i]);
					break;
				}

				input_report_abs(g_input_dev,
							ABS_MT_TRACKING_ID,
							spTP->ucID[i]);

				if (spTP->usX[i] >= (iMaxX - 1))
					input_report_abs(g_input_dev,
							ABS_MT_POSITION_X,
							(iMaxX - 1));
				else
					input_report_abs(g_input_dev,
							ABS_MT_POSITION_X,
							spTP->usX[i]);

				if (spTP->usY[i] >= (iMaxY - 1))
					input_report_abs(g_input_dev,
							ABS_MT_POSITION_Y,
							(iMaxY - 1));
				else
					input_report_abs(g_input_dev,
							ABS_MT_POSITION_Y,
							spTP->usY[i]);

				input_report_abs(g_input_dev,
							ABS_MT_PRESSURE,
							spTP->usZ[i]);

			}
			input_mt_sync(g_input_dev);
		}
		input_sync(g_input_dev);
		ucLastTouchCount = spTP->ucTouchCount;
	}
#else
	if (iCount && !spTP->ucTouchCount) {
		ucLastTouchCount = 0;
		for (i = 0; i < INPUT_DEVICE_AMOUNT; i++) {
			if (i == INPUT_DEVICE_FOR_STYLUS)
				input_report_key(g_input_dev[i],
					BTN_TOOL_RUBBER, 0);
			input_mt_sync(g_input_dev[i]);
			input_sync(g_input_dev[i]);
		}
	} else if (iCount) {
		for (i = 0; i < iCount; i++) {
			if (i == MAX_REPORT_TOUCHED_POINTS)
				break;

			if ((spTP->ucToolType[i] == POINT_TYPE_ERASER)
					&& (spTP->ucSlot[i] & INPUT_POINT_RESET)) {
				input_report_key(
					g_input_dev[INPUT_DEVICE_FOR_STYLUS],
					BTN_TOOL_RUBBER, 0);
				input_mt_sync(
					g_input_dev[INPUT_DEVICE_FOR_STYLUS]);
				continue;
			}

			if (i < spTP->ucTouchCount) {
				switch (spTP->ucToolType[i]) {
				case POINT_TYPE_FINGER:
					target_device = INPUT_DEVICE_FOR_FINGER;
					input_report_abs(
						g_input_dev[target_device],
						ABS_MT_TOOL_TYPE,
						MT_TOOL_FINGER);
					break;
				case POINT_TYPE_STYLUS:
					target_device = INPUT_DEVICE_FOR_STYLUS;
					input_report_abs(
						g_input_dev[target_device],
						ABS_MT_TOOL_TYPE,
						MT_TOOL_PEN);
					break;
				case POINT_TYPE_ERASER:
					target_device = INPUT_DEVICE_FOR_STYLUS;
					input_report_key(
						g_input_dev[target_device],
						BTN_TOOL_RUBBER, 1);
					break;
				default:
					dev_err(&g_spi->dev,
						"Raydium - point %d is invalid input tool type: %d\n",
						i, spTP->ucToolType[i]);
					break;
				}

				input_report_abs(g_input_dev[target_device],
							ABS_MT_TRACKING_ID,
							spTP->ucID[i]);

				if (spTP->usX[i] >= (iMaxX - 1))
					input_report_abs(g_input_dev[target_device],
							ABS_MT_POSITION_X,
							(iMaxX - 1));
				else
					input_report_abs(g_input_dev[target_device],
							ABS_MT_POSITION_X,
							spTP->usX[i]);

				if (spTP->usY[i] >= (iMaxY - 1))
					input_report_abs(g_input_dev[target_device],
							ABS_MT_POSITION_Y,
							(iMaxY - 1));
				else
					input_report_abs(g_input_dev[target_device],
							ABS_MT_POSITION_Y,
							spTP->usY[i]);

				input_report_abs(g_input_dev[target_device],
							ABS_MT_PRESSURE,
							spTP->usZ[i]);

			}
			input_mt_sync(g_input_dev[target_device]);
		}
		for (i = 0; i < INPUT_DEVICE_AMOUNT; i++)
			input_sync(g_input_dev[i]);
		ucLastTouchCount = spTP->ucTouchCount;
	}
#endif
	if (g_stCtrl.bfPowerMode)
		raydium_change_scan_mode(spTP->ucTouchCount);

	kfree(spTP);
}

/*=============================================================================
	 Description: Read Sensor Raw Data

	 Input:
			*p : Raw Data Buffer Address
	 Output:
			none
=============================================================================*/
static int rm_tch_read_image_data(unsigned char *p)
{
	int ret;
	g_pu8BurstReadBuf = p;
	ret = rm_tch_cmd_process(0, g_stRmReadImgCmd, NULL);
	return ret;
}

void rm_tch_ctrl_enter_auto_mode(void)
{
	g_stCtrl.bfIdleModeCheck &= ~0x01;

	if (g_stCtrl.bKernelMsg & DEBUG_DRIVER)
		rm_printk("Raydium - Enter Auto Scan Mode\n");

	rm_set_repeat_times(g_stCtrl.bIdleDigitalRepeatTimes);
	rm_tch_cmd_process(1, g_stCmdSetIdle, NULL);
}

void rm_tch_ctrl_leave_auto_mode(void)
{
	g_stCtrl.bfIdleModeCheck |= 0x01;

	rm_tch_cmd_process(0, g_stCmdSetIdle, NULL);
	rm_set_repeat_times(g_stCtrl.bActiveDigitalRepeatTimes);

	if (g_stCtrl.bKernelMsg & DEBUG_DRIVER)
		rm_printk("Raydium - Leave Auto Scan Mode\n");
}

void rm_ctrl_pause_auto_mode(void)
{
	rm_tch_cmd_process(0, g_stCmdPauseAuto, NULL);
}

int rm_tch_ctrl_clear_int(void)
{
	return rm_tch_cmd_process(0, g_stRmClearIntCmd, NULL);
}

void rm_set_ns_para(u8 u8Idx, u8 u8Para)
{
#ifndef INPUT_DEVICE_MULTIPLE_INSTANCES
	struct rm_tch_ts *ts = input_get_drvdata(g_input_dev);
#else
	struct rm_tch_ts *ts = input_get_drvdata(g_input_dev[0]);
#endif

	ts->bRepeatCounter = u8Para;

	rm_tch_cmd_process(u8Idx, g_stRmNsParaCmd, ts);
}

int rm_tch_ctrl_scan_start(void)
{
	static u8 u8NsSel = 0, u8Rpt = 1;

	if (g_stCtrl.bNsFucnEnable) {
		rm_tch_ctrl_wait_for_scan_finish();

		mutex_lock(&g_stTs.mutex_ns_mode);

		g_stTs.bNsSel = g_stTs.bSWCPW[u8NsSel];

		if (u8NsSel < g_stTs.bNsMode)
			u8NsSel++;
		else
			u8NsSel = 0;

		if (u8Rpt != g_stTs.bNsRpt) {
			u8Rpt = g_stTs.bNsRpt;
			rm_set_repeat_times(u8Rpt);
		}
		rm_set_ns_para(0, g_stTs.bDP[u8NsSel]);
		rm_set_ns_para(1, g_stTs.bSWC[u8NsSel]);
		rm_set_ns_para(2, g_stTs.bSWCPW[u8NsSel]);

		mutex_unlock(&g_stTs.mutex_ns_mode);
	}

	return rm_tch_cmd_process(0, g_stRmScanStartCmd, NULL);
}

void rm_tch_ctrl_wait_for_scan_finish(void)
{
	int i;
	/*50ms = 20Hz*/

	for (i = 0; i < 50; i++) {
		rm_tch_cmd_process(0, g_stRmWaitScanOKCmd, NULL);
		if (g_stTs.u16ReadPara & 0x01)
			usleep_range(1000, 2000);  /* msleep(1); */
		else
			break;
	}
}

void rm_set_repeat_times(u8 u8Times)
{
#ifndef INPUT_DEVICE_MULTIPLE_INSTANCES
	struct rm_tch_ts *ts = input_get_drvdata(g_input_dev);
#else
	struct rm_tch_ts *ts = input_get_drvdata(g_input_dev[0]);
#endif

	if (u8Times <= 1)
		u8Times = 0;
	else
		u8Times = u8Times - 1;

	if (u8Times > 31)
		u8Times = 31;

	ts->bRepeatCounter = (u8Times & 0x1F);
	if (u8Times == 0)
		rm_tch_cmd_process(0, g_stRmSetRepTimeCmd, ts);
	else
		rm_tch_cmd_process(1, g_stRmSetRepTimeCmd, ts);
}

static u32 rm_tch_ctrl_configure(void)
{
	u32 u32Flag;

	switch (g_stTs.u8ScanModeState) {
	case RM_SCAN_ACTIVE_MODE:
		u32Flag =
			RM_NEED_TO_SEND_SCAN |
			RM_NEED_TO_READ_RAW_DATA |
			RM_NEED_TO_SEND_SIGNAL;
		break;

	case RM_SCAN_PRE_IDLE_MODE:
		rm_tch_ctrl_enter_auto_mode();
		g_stTs.u8ScanModeState = RM_SCAN_IDLE_MODE;
		u32Flag = RM_NEED_NONE;
		break;

	case RM_SCAN_IDLE_MODE:
		rm_tch_ctrl_leave_auto_mode();
		g_stTs.u8ScanModeState = RM_SCAN_ACTIVE_MODE;
		u32Flag = RM_NEED_TO_SEND_SCAN;
		/*
		RM_NEED_TO_SEND_SCAN |
		RM_NEED_TO_READ_RAW_DATA |
		RM_NEED_TO_SEND_SIGNAL;
		*/
		break;

	default:
		u32Flag = RM_NEED_NONE;
		break;
	}

	return u32Flag;
}

static struct rm_spi_ts_platform_data *rm_ts_parse_dt(struct device *dev,
					int irq)
{
	struct rm_spi_ts_platform_data *pdata;
	struct device_node *np = dev->of_node;
	const char *str;
	int ret, val, irq_gpio;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->gpio_reset = of_get_named_gpio_flags(np, "reset-gpio", 0, NULL);
	if (!gpio_is_valid(pdata->gpio_reset)) {
		dev_err(dev, "Invalid reset-gpio\n");
		return ERR_PTR(-EINVAL);
	}
	ret = gpio_request(pdata->gpio_reset, "reset-gpio");
	if (ret < 0) {
		dev_err(dev, "gpio_request fail\n");
		return ERR_PTR(-EINVAL);
	}
	gpio_direction_output(pdata->gpio_reset, 0);

	ret =  of_property_read_u32(np, "interrupts", &irq_gpio);
	if (!gpio_is_valid(irq_gpio)) {
		dev_err(dev, "Invalid irq-gpio\n");
		ret = -EINVAL;
		goto exit_release_reset_gpio;
	}

	ret = gpio_request(irq_gpio, "irq-gpio");
	if (ret < 0) {
		dev_err(dev, "irq_request fail\n");
		ret = -EINVAL;
		goto exit_release_reset_gpio;
	}
	gpio_direction_input(irq_gpio);

	ret = of_property_read_u32(np, "config", &val);
	if (ret < 0)
		goto exit_release_all_gpio;
	pdata->config = val;
	ret = of_property_read_u32(np, "platform-id", &val);
	if (ret < 0)
		goto exit_release_all_gpio;
	pdata->platform_id = val;
	ret = of_property_read_string(np, "name-of-clock", &str);
	if (ret < 0)
		goto exit_release_all_gpio;
	pdata->name_of_clock = (char *)str;

	ret = of_property_read_string(np, "name-of-clock-con", &str);
	if (ret < 0)
		goto exit_release_all_gpio;
	pdata->name_of_clock_con = (char *)str;

	return pdata;

exit_release_all_gpio:
	gpio_free(irq_gpio);

exit_release_reset_gpio:
	gpio_free(pdata->gpio_reset);
	return ERR_PTR(ret);

}

int KRL_CMD_CONFIG_1V8_Handler(u8 CMD, u8 OnOff, struct rm_tch_ts *ts)
{
	int ret = FAIL;
	struct rm_spi_ts_platform_data *pdata;

#ifndef INPUT_DEVICE_MULTIPLE_INSTANCES
	pdata = g_input_dev->dev.parent->platform_data;
#else
	pdata = g_input_dev[0]->dev.parent->platform_data;
#endif

	if (CMD == KRL_SUB_CMD_SET_1V8_REGULATOR) {
		if (ts) {
			if (ts->regulator_1v8) {
				if (OnOff) {
					if (regulator_is_enabled(ts->regulator_1v8))
						ret = OK;
					else
						ret = regulator_enable(
								ts->regulator_1v8);
					if (ret < 0) {
						dev_err(&g_spi->dev,
							"Raydium - regulator 1.8V enable failed: %d\n",
							ret);
						ret = FAIL;
					} else
						ret = OK;
				} else {
					if (!regulator_is_enabled(ts->regulator_1v8))
						ret = OK;
					else
						ret = regulator_disable(
								ts->regulator_1v8);
					if (ret < 0) {
						dev_err(&g_spi->dev,
							"Raydium - regulator 1.8V disable failed: %d\n",
							ret);
						ret = FAIL;
					} else
						ret = OK;
				}
			} else
				dev_err(&g_spi->dev,
					"Raydium - regulator 1.8V handler fail: %d\n",
					ret);
		} else
			dev_err(&g_spi->dev,
				"Raydium - regulator 1.8V ts fail: %d\n",
				ret);
	} else if (CMD == KRL_SUB_CMD_SET_1V8_GPIO) {
		gpio_direction_output(pdata->gpio_1v8, OnOff);
		ret = OK;
	}
	return ret;
}

int KRL_CMD_CONFIG_3V3_Handler(u8 CMD, u8 OnOff, struct rm_tch_ts *ts)
{
	int ret = FAIL;
	struct rm_spi_ts_platform_data *pdata;

#ifndef INPUT_DEVICE_MULTIPLE_INSTANCES
	pdata = g_input_dev->dev.parent->platform_data;
#else
	pdata = g_input_dev[0]->dev.parent->platform_data;
#endif

	if (CMD == KRL_SUB_CMD_SET_3V3_REGULATOR) {
		if (ts) {
			if (ts->regulator_3v3) {
				if (OnOff) {
					if (regulator_is_enabled(ts->regulator_3v3))
						ret = OK;
					else
						ret = regulator_enable(
								ts->regulator_3v3);
					if (ret < 0) {
						dev_err(&g_spi->dev,
							"Raydium - regulator 3.3V enable failed: %d\n",
							ret);
						ret = FAIL;
					} else
						ret = OK;
				} else {
					if (!regulator_is_enabled(ts->regulator_3v3))
						ret = OK;
					else
						ret = regulator_disable(
								ts->regulator_3v3);
					if (ret < 0) {
						dev_err(&g_spi->dev,
							"Raydium - regulator 3.3V disable failed: %d\n",
							ret);
						ret = FAIL;
					} else
						ret = OK;
				}
			} else
				dev_err(&g_spi->dev,
					"Raydium - regulator 3.3V handler fail: %d\n",
					ret);
		} else
			dev_err(&g_spi->dev,
				"Raydium - regulator 3.3V ts fail: %d\n",
				ret);
	} else if (CMD == KRL_SUB_CMD_SET_3V3_GPIO) {
		gpio_direction_output(pdata->gpio_3v3, OnOff);
		ret = OK;
	}
	return ret;
}

static int rm_tch_cmd_process(u8 selCase, u8 *pCmdTbl, struct rm_tch_ts *ts)
{
#define _CMD u16j
#define _ADDR (u16j+1)
#define _SUB_CMD (u16j+1)
#define _DATA (u16j+2)

	static DEFINE_MUTEX(lock);
	u16 u16j = 0, u16strIdx, u16TblLenth, u16Tmp;
	u32 u32Tmp;
	u8 u8i, u8reg = 0;
	int ret = FAIL;
	struct rm_spi_ts_platform_data *pdata;

	mutex_lock(&lock);

#ifndef INPUT_DEVICE_MULTIPLE_INSTANCES
	pdata = g_input_dev->dev.parent->platform_data;
#else
	pdata = g_input_dev[0]->dev.parent->platform_data;
#endif
	u16TblLenth = pCmdTbl[KRL_TBL_FIELD_POS_LEN_H];
	u16TblLenth <<= 8;
	u16TblLenth |= pCmdTbl[KRL_TBL_FIELD_POS_LEN_L];

	if (u16TblLenth < 3) {
		if (g_stCtrl.bKernelMsg & DEBUG_DRIVER)
			dev_info(&g_spi->dev, "Raydium - Null CMD %s : [%p]\n",
				__func__, pCmdTbl);
		mutex_unlock(&lock);
		return ret;
	}

	u16strIdx = KRL_TBL_FIELD_POS_CASE_NUM
		+ pCmdTbl[KRL_TBL_FIELD_POS_CASE_NUM] + 1;
	for (u8i = 0; u8i < selCase; u8i++)
		u16strIdx += (pCmdTbl[u8i + KRL_TBL_FIELD_POS_CMD_NUM]
			* KRL_TBL_CMD_LEN);

	for (u8i = 0; u8i < pCmdTbl[selCase + KRL_TBL_FIELD_POS_CMD_NUM];
		u8i++) {
		u16j = u16strIdx + (KRL_TBL_CMD_LEN * u8i);
		ret = FAIL;
		switch (pCmdTbl[_CMD]) {
		case KRL_CMD_READ:
			ret = rm_tch_spi_read(pCmdTbl[_ADDR], &u8reg, 1);
			g_stTs.u16ReadPara = u8reg;
			/*rm_printk("Raydium - KRL_CMD_READ "
			"- 0x%x:0x%x\n", pCmdTbl[_ADDR], u8reg);*/
			break;
		case KRL_CMD_WRITE_W_DATA:
			/*rm_printk("Raydium - KRL_CMD_WRITE_W_DATA "
			"- 0x%x:0x%x\n", pCmdTbl[_ADDR], pCmdTbl[_DATA]);*/
			ret = rm_tch_spi_byte_write(pCmdTbl[_ADDR],
				pCmdTbl[_DATA]);
			break;
		case KRL_CMD_WRITE_WO_DATA:
			/*rm_printk("Raydium - KRL_CMD_WRITE_WO_DATA "
			"- 0x%x:0x%x\n", pCmdTbl[_ADDR], u8reg);*/
			ret = rm_tch_spi_byte_write(pCmdTbl[_ADDR], u8reg);
			break;
		case KRL_CMD_IF_AND_OR:
			if (u8reg & pCmdTbl[_ADDR]) 
				u8reg |= pCmdTbl[_DATA];
			ret = OK;
			break;
		case KRL_CMD_AND:
			u8reg &= pCmdTbl[_DATA];
			ret = OK;
			break;
		case KRL_CMD_OR:
			u8reg |= pCmdTbl[_DATA];
			ret = OK;
			break;
		case KRL_CMD_NOT:
			u8reg = ~u8reg;
			//g_stTs.u16ReadPara = u8reg;
			ret = OK;
			break;
		case KRL_CMD_XOR:
			u8reg ^= pCmdTbl[_DATA];
			ret = OK;
			break;
		case KRL_CMD_SEND_SIGNAL:
			u16Tmp = pCmdTbl[_DATA];
			/*rm_printk("Raydium - KRL_CMD_SEND_SIGNAL "
			"- %d\n", u16Tmp);*/
			ret = rm_tch_ts_send_signal(g_stTs.ulHalPID,
				(int)u16Tmp);
			if (u16Tmp == RM_SIGNAL_RESUME)
				g_stTs.u8ResumeCnt++;
			break;
		case KRL_CMD_CONFIG_RST:
			/*rm_printk("Raydium - KRL_CMD_CONFIG_RST "
			"- %d - %d\n", pCmdTbl[_SUB_CMD], pCmdTbl[_DATA]);*/
			switch (pCmdTbl[_SUB_CMD]) {
			case KRL_SUB_CMD_SET_RST_GPIO:
				gpio_direction_output(pdata->gpio_reset,
					pCmdTbl[_DATA]);
				break;
			case KRL_SUB_CMD_SET_RST_VALUE:
				gpio_set_value(pdata->gpio_reset,
					pCmdTbl[_DATA]);
				break;
			}
			ret = OK;
			break;
		case KRL_CMD_CONFIG_3V3:/*Need to check qpio setting*/
			/*rm_printk("Raydium - KRL_CMD_CONFIG_3V3 "
			"- %d - %d\n", pCmdTbl[_SUB_CMD], pCmdTbl[_DATA]);
			rm_printk("Raydium - 3.3V regulator is %d\n",
			regulator_is_enabled(ts->regulator_3v3));*/
			ret = KRL_CMD_CONFIG_3V3_Handler(pCmdTbl[_SUB_CMD],
				pCmdTbl[_DATA], ts);
			/*rm_printk("Raydium - 3.3V regulator is %d\n",
			regulator_is_enabled(ts->regulator_3v3));*/
			break;
		case KRL_CMD_CONFIG_1V8:/*Need to check qpio setting*/
			/*rm_printk("Raydium - KRL_CMD_CONFIG_1V8 "
			"- %d - %d\n", pCmdTbl[_SUB_CMD], pCmdTbl[_DATA]);
			rm_printk("Raydium - 1.8V regulator is %d\n",
			regulator_is_enabled(ts->regulator_1v8));*/
			ret = KRL_CMD_CONFIG_1V8_Handler(pCmdTbl[_SUB_CMD],
				pCmdTbl[_DATA], ts);
			/*rm_printk("Raydium - 1.8V regulator is %d\n",
			regulator_is_enabled(ts->regulator_1v8));*/
			break;
		case KRL_CMD_CONFIG_CLK:
			/*rm_printk("Raydium - KRL_CMD_CONFIG_CLK"
			" - %d - %d\n", pCmdTbl[_SUB_CMD], pCmdTbl[_DATA]);*/
			ret = OK;
			/* Temporarily solving external clk issue for NV
				for different kind of clk source*/
			/*
			if (ts && ts->clk) {
				if (pCmdTbl[_SUB_CMD] == KRL_SUB_CMD_SET_CLK) {
					if (pCmdTbl[_DATA])
						clk_enable(ts->clk);
					else
						clk_disable(ts->clk);
				} else
					ret = FAIL;
			} else {
				ret = FAIL;
			}
			*/
			if (pCmdTbl[_SUB_CMD] == KRL_SUB_CMD_SET_CLK) {
				if (ts && ts->clk) {
					if (pCmdTbl[_DATA])
						clk_enable(ts->clk);
					else
						clk_disable(ts->clk);
				} else {
					dev_err(&g_spi->dev, "Raydium - %s : No clk handler!\n",
						__func__);
				}
			} else
				ret = FAIL;
			break;
		case KRL_CMD_CONFIG_CS:
			/*rm_printk("Raydium - KRL_CMD_CONFIG_CS "
			"- %d - %d\n", pCmdTbl[_SUB_CMD], pCmdTbl[_DATA]);*/
			switch (pCmdTbl[_SUB_CMD]) {
			case KRL_SUB_CMD_SET_CS_LOW:
				ret = !spi_cs_low(g_spi, (bool)!!pCmdTbl[_DATA]);
				break;
			}
			break;
		case KRL_CMD_SET_TIMER:
			/*rm_printk("Raydium - KRL_CMD_SET_TIMER "
			"- %d\n", pCmdTbl[_SUB_CMD]);*/
			ret = OK;
			if (pCmdTbl[_SUB_CMD] == KRL_SUB_CMD_INIT_TIMER) {
				init_ts_timer();
			} else if (pCmdTbl[_SUB_CMD] == KRL_SUB_CMD_ADD_TIMER) {
				if (!timer_pending(&ts_timer_triggle))
					add_timer(&ts_timer_triggle);
			} else if (pCmdTbl[_SUB_CMD] == KRL_SUB_CMD_DEL_TIMER) {
				del_timer(&ts_timer_triggle);
			} else
				ret = FAIL;
			break;
		case KRL_CMD_MSLEEP:
			/*rm_printk("Raydium - KRL_CMD_MSLEEP "
			"- %d ms\n", pCmdTbl[_DATA] |
			(pCmdTbl[_SUB_CMD] << 8));*/
			u32Tmp = (u16)(pCmdTbl[_DATA] |
					(pCmdTbl[_SUB_CMD] << 8));

			u32Tmp *= 1000;
			usleep_range(u32Tmp, u32Tmp + 200);
			ret = OK;
			break;
		case KRL_CMD_FLUSH_QU:
			/*rm_printk("Raydium - KRL_CMD_FLUSH_QU "
			"- %d\n", pCmdTbl[_SUB_CMD]);*/
			ret = OK;
			g_is_flush = true;
			if (pCmdTbl[_SUB_CMD] == KRL_SUB_CMD_SENSOR_QU)
				flush_workqueue(g_stTs.rm_workqueue);
			else if (pCmdTbl[_SUB_CMD] == KRL_SUB_CMD_TIMER_QU)
				flush_workqueue(g_stTs.rm_timer_workqueue);
			else
				ret = FAIL;
			break;
		case KRL_CMD_READ_IMG:
			/*rm_printk("Raydium - KRL_CMD_READ_IMG "
			"- 0x%x:%p:%d\n",
			pCmdTbl[_ADDR],
			g_pu8BurstReadBuf,
			g_stCtrl.u16DataLength);*/
			if (g_pu8BurstReadBuf != NULL) {
				ret = rm_tch_spi_read(pCmdTbl[_ADDR],
						g_pu8BurstReadBuf,
						g_stCtrl.u16DataLength);
				if(g_stCtrl.bNsFucnEnable){
					g_pu8BurstReadBuf[g_stCtrl.u16DataLength] = g_stTs.bNsSel;
					/*rm_printk("Raydium -Ns mode:%d,%x,%x\n",
					g_stCtrl.u16DataLength,g_pu8BurstReadBuf[g_stCtrl.u16DataLength],g_stTs.bNsSel);*/
				}
			}
			g_pu8BurstReadBuf = NULL;
			break;
		case KRL_CMD_WRITE_W_COUNT:
			/*rm_printk("Raydium - KRL_CMD_WRITE_W_COUNT "
			"- 0x%x: 0x%x..0x%x\n", pCmdTbl[_ADDR], u8reg,
			ts->bRepeatCounter);*/
			if (ts)
				ret = rm_tch_spi_byte_write(pCmdTbl[_ADDR],
						u8reg | (ts->bRepeatCounter));
			else
				ret = FAIL;
			break;
		case KRL_CMD_RETURN_RESULT:
			g_stTs.u16ReadPara = u8reg;
			ret = OK;
			break;
		case KRL_CMD_RETURN_VALUE:
			g_stTs.u16ReadPara = (pCmdTbl[_ADDR] << 8) + (pCmdTbl[_DATA]);
			ret = OK;
			/*rm_printk("Raydium - KRL_CMD_RETURN_VALUE, value=%d", ret);*/
			break;
		default:
			break;
		}

		//if ( (pCmdTbl[_CMD] == KRL_CMD_RETURN_RESULT) || (pCmdTbl[_CMD] == KRL_CMD_RETURN_VALUE))
		//	break;

		if (ret == FAIL) {
			dev_err(&g_spi->dev, "Raydium - %s : [%p] cmd failed\n",
				__func__,
				pCmdTbl);
			dev_err(&g_spi->dev, "Raydium - cmd:0x%x, addr:0x%x, data:0x%x\n",
				pCmdTbl[_CMD],
				pCmdTbl[_ADDR],
				pCmdTbl[_DATA]);
			break;
		}
	}

	mutex_unlock(&lock);

	return ret;
}

int rm_set_kernel_tbl(int iFuncIdx, u8 *u8pSrc)
{
	ssize_t missing;
	u16 u16len = 0;
	u8 *u8pLen;
	u8 *u8pDst;

	switch (iFuncIdx) {
	case KRL_INDEX_FUNC_SET_IDLE:
		u8pDst = g_stCmdSetIdle;
		break;
	case KRL_INDEX_FUNC_PAUSE_AUTO:
		u8pDst = g_stCmdPauseAuto;
		break;
	case KRL_INDEX_RM_RESUME:
		u8pDst = g_stRmResumeCmd;
		break;
	case KRL_INDEX_RM_SUSPEND:
		u8pDst = g_stRmSuspendCmd;
		break;
	case KRL_INDEX_RM_READ_IMG:
		u8pDst = g_stRmReadImgCmd;
		break;
	case KRL_INDEX_RM_WATCHDOG:
		u8pDst = g_stRmWatchdogCmd;
		break;
	case KRL_INDEX_RM_TESTMODE:
		u8pDst = g_stRmTestModeCmd;
		break;
	case KRL_INDEX_RM_SLOWSCAN:
		u8pDst = g_stRmSlowScanCmd;
		break;
	case KRL_INDEX_RM_SLOWSCANB:
		u8pDst = g_stRmSlowScanBCmd;
		break;
	case KRL_INDEX_RM_CLEARINT:
		u8pDst = g_stRmClearIntCmd;
		break;
	case KRL_INDEX_RM_SCANSTART:
		u8pDst = g_stRmScanStartCmd;
		break;
	case KRL_INDEX_RM_WAITSCANOK:
		u8pDst = g_stRmWaitScanOKCmd;
		break;
	case KRL_INDEX_RM_SETREPTIME:
		u8pDst = g_stRmSetRepTimeCmd;
		break;
	case KRL_INDEX_RM_NSPARA:
		u8pDst = g_stRmNsParaCmd;
		break;

	default:
		dev_err(&g_spi->dev, "Raydium - %s : no such kernel table - err:%d\n",
			__func__, iFuncIdx);
		return FAIL;
	}

	u8pLen = kmalloc(KRL_TBL_FIELD_POS_CASE_NUM, GFP_KERNEL);
	if (u8pLen == NULL)
		return FAIL;

	missing = copy_from_user(u8pLen, u8pSrc, KRL_TBL_FIELD_POS_CASE_NUM);
	if (missing) {
		dev_err(&g_spi->dev, "Raydium - %s : copy failed - len:%d, miss:%d\n",
			__func__, KRL_TBL_FIELD_POS_CASE_NUM, missing);
		kfree(u8pLen);
		return FAIL;
	}

	u16len = u8pLen[KRL_TBL_FIELD_POS_LEN_H];
	u16len <<= 8;
	u16len |= u8pLen[KRL_TBL_FIELD_POS_LEN_L];

	missing = copy_from_user(u8pDst, u8pSrc, u16len);
	if (missing) {
		dev_err(&g_spi->dev, "Raydium - %s : copy failed - len:%d, miss:%d\n",
			__func__, u16len, missing);
		kfree(u8pLen);
		return FAIL;
	}

	/*dev_info(&g_spi->dev, "Raydium - %s : CMD_TAB_%d[%p]\n",
	__func__, iFuncIdx, u8pDst);*/

	kfree(u8pLen);
	return OK;
}
/*=============================================================================

=============================================================================*/
static void rm_tch_enter_manual_mode(void)
{
	flush_workqueue(g_stTs.rm_workqueue);

	mutex_lock(&g_stTs.mutex_scan_mode);
	if (g_stTs.u8ScanModeState == RM_SCAN_ACTIVE_MODE) {
		mutex_unlock(&g_stTs.mutex_scan_mode);
		return;
	}

	if (g_stTs.u8ScanModeState == RM_SCAN_PRE_IDLE_MODE) {
		g_stTs.u8ScanModeState = RM_SCAN_ACTIVE_MODE;
		mutex_unlock(&g_stTs.mutex_scan_mode);
		return;
	}

	if (g_stTs.u8ScanModeState == RM_SCAN_IDLE_MODE) {
		rm_tch_ctrl_leave_auto_mode();
		g_stTs.u8ScanModeState = RM_SCAN_ACTIVE_MODE;
		usleep_range(10000, 10050);/*msleep(10);*/
	}
	mutex_unlock(&g_stTs.mutex_scan_mode);
}

static u32 rm_tch_get_platform_id(u32 *p)
{
	u32 u32Ret;
	struct rm_spi_ts_platform_data *pdata;
#ifndef INPUT_DEVICE_MULTIPLE_INSTANCES
	pdata = g_input_dev->dev.parent->platform_data;
#else
	pdata = g_input_dev[0]->dev.parent->platform_data;
#endif
	u32Ret = copy_to_user(p, &pdata->platform_id,
		sizeof(pdata->platform_id));
	if (u32Ret != 0)
		return 0;
	return 1;
}

static u32 rm_tch_get_gpio_sensor_select(u8 *p)
{
	u32 u32Ret = 0;
		struct rm_spi_ts_platform_data *pdata;
#ifndef INPUT_DEVICE_MULTIPLE_INSTANCES
		pdata = g_input_dev->dev.parent->platform_data;
#else
		pdata = g_input_dev[0]->dev.parent->platform_data;
#endif
/* Read from GPIO
		u32Ret = gpio_set_value(pdata->gpio_sensor_select0)
			| (1 << gpio_set_value(pdata->gpio_sensor_select1));
*/
/* Read from data struct */
	if (pdata->gpio_sensor_select0)
		u32Ret |= 0x01;
	if (pdata->gpio_sensor_select1)
		u32Ret |= 0x02;

	u32Ret = copy_to_user(p, &u32Ret, sizeof(u32Ret));
	if (u32Ret != 0)
		return FAIL;

	return OK;
}

static u32 rm_tch_get_spi_lock_status(u8 *p)
{
	u32 u32Ret;

	if (g_stCtrl.bKernelMsg & DEBUG_DRIVER)
		rm_printk("Raydium - SPI_LOCK = %d\n", g_stTs.u8SPILocked);

	u32Ret = copy_to_user(p, &g_stTs.u8SPILocked,
					sizeof(g_stTs.u8SPILocked));

	if (g_stTs.u8SPILocked && g_stTs.u8ResumeCnt)
		g_stTs.u8ResumeCnt--;

	if (u32Ret != 0)
		return 0;
	return 1;
}

static u32 rm_tch_get_report_mode(u8 *p)
{
	u32 u32Ret;

	if (g_stCtrl.bKernelMsg & DEBUG_DRIVER)
		rm_printk("Raydium - REPORT_MODE = %d\n", g_stTs.u8ReportMode);

	u32Ret = copy_to_user(p, &g_stTs.u8ReportMode,
					sizeof(g_stTs.u8ReportMode));
	if (u32Ret != 0)
		return 0;
	return 1;
}

/*===========================================================================*/
static int rm_tch_ts_send_signal(int pid, int iInfo)
{
	struct siginfo info;
	struct task_struct *t;
	int ret = OK;

	static DEFINE_MUTEX(lock);

	if (!pid) {
		dev_err(&g_spi->dev, "Raydium - %s : pid failed\n", __func__);
		return FAIL;
	}

	mutex_lock(&lock);
	/* send the signal */
	memset(&info, 0, sizeof(struct siginfo));
	info.si_signo = RM_TS_SIGNAL;
	info.si_code = SI_QUEUE;
	/*
		this is bit of a trickery: SI_QUEUE is normally
		used by sigqueue from user space, and kernel
		space should use SI_KERNEL. But if SI_KERNEL
		is used the real_time data is not delivered to
		the user space signal handler function.
	*/
	info.si_int = iInfo;	/*real time signals may have 32 bits of data.*/

	rcu_read_lock();
	t = find_task_by_vpid(pid);
	rcu_read_unlock();
	if (t == NULL) {
		dev_err(&g_spi->dev, "Raydium - %s : no such pid\n", __func__);
		mutex_unlock(&lock);
		return FAIL;
	} else
		/*send the signal*/
		ret = send_sig_info(RM_TS_SIGNAL, &info, t);

	if (ret < 0) {
		dev_err(&g_spi->dev, "Raydium - %s : send sig failed err:%d\n",
			__func__, ret);
		mutex_unlock(&lock);
		return FAIL;
	}
	mutex_unlock(&lock);
	return OK;
}

/*=============================================================================
	Description:
		Queuing functions.
	Input:
		N/A
	Output:
		0:succeed
		others:error code
=============================================================================*/
static void rm_tch_queue_reset(void)
{
	g_stQ.u16Rear = 0;
	g_stQ.u16Front = 0;
}

static int rm_tch_queue_init(void)
{
	rm_tch_queue_reset();
	g_stQ.pQueue = kmalloc(QUEUE_COUNT * RM_RAW_DATA_LENGTH, GFP_KERNEL);
	if (g_stQ.pQueue == NULL)
		return -ENOMEM;

	return 0;
}

static void rm_tch_queue_free(void)
{
	if (!g_stQ.pQueue)
		return;
	kfree(g_stQ.pQueue);
	g_stQ.pQueue = NULL;
}

#ifdef ENABLE_CALC_QUEUE_COUNT
static int rm_tch_queue_get_current_count(void)
{
	if (g_stQ.u16Rear >= g_stQ.u16Front)
		return g_stQ.u16Rear - g_stQ.u16Front;

	return (QUEUE_COUNT - g_stQ.u16Front) + g_stQ.u16Rear;
}
#endif

/*=============================================================================
	Description:
	About full/empty buffer distinction,
	There are a number of solutions like:
	1.Always keep one slot open.
	2.Use a fill count to distinguish the two cases.
	3.Use read and write counts to get the fill count from.
	4.Use absolute indices.
	we chose "keep one slot open" to make it simple and robust
	and also avoid race condition.
	Input:
		N/A
	Output:
		1:empty
		0:not empty
=============================================================================*/
static int rm_tch_queue_is_empty(void)
{
	if (g_stQ.u16Rear == g_stQ.u16Front)
		return 1;
	return 0;
}

/*=============================================================================
	Description:
	check queue full.
	Input:
		N/A
	Output:
		1:full
		0:not full
=============================================================================*/
static int rm_tch_queue_is_full(void)
{
	u16 u16Front = g_stQ.u16Front;

	if (g_stQ.u16Rear + 1 == u16Front)
		return 1;

	if ((g_stQ.u16Rear == (QUEUE_COUNT - 1)) && (u16Front == 0))
		return 1;

	return 0;
}

static void *rm_tch_enqueue_start(void)
{
	if (!g_stQ.pQueue)	/*error handling for no memory*/
		return NULL;

	if (!rm_tch_queue_is_full()) {
		g_service_busy_report_count = 100 ;
		return &g_stQ.pQueue[g_stQ.u16Rear];
	}

	if (g_service_busy_report_count < 0) {
		g_service_busy_report_count = 100;
		rm_printk("Raydium - touch service is busy,try again.\n");
	} else {
		g_service_busy_report_count--;
	}
	return NULL;
}

static void rm_tch_enqueue_finish(void)
{
	if (g_stQ.u16Rear == (QUEUE_COUNT - 1))
		g_stQ.u16Rear = 0;
	else
		g_stQ.u16Rear++;
}

static void *rm_tch_dequeue_start(void)
{
	if (!rm_tch_queue_is_empty())
		return &g_stQ.pQueue[g_stQ.u16Front];

	return NULL;
}

static void rm_tch_dequeue_finish(void)
{
	if (g_stQ.u16Front == (QUEUE_COUNT - 1))
		g_stQ.u16Front = 0;
	else
		g_stQ.u16Front++;
}

static long rm_tch_queue_read_raw_data(u8 *p, u32 u32Len)
{
	u8 *pQueue;
	u32 u32Ret;
	pQueue = rm_tch_dequeue_start();
	if (!pQueue)
		return 0;

	u32Ret = copy_to_user(p, pQueue, u32Len);
	if (u32Ret != 0)
		return 0;

	rm_tch_dequeue_finish();
	return 1;
}
/*===========================================================================*/
static void rm_work_handler(struct work_struct *work)
{
	void *pKernelBuffer;
	u32 u32Flag;
	int iRet;

	if (g_stTs.bInitFinish == false || g_stTs.bIsSuspended)
		return;

	if (g_is_flush == true)
		return;

	mutex_lock(&g_stTs.mutex_scan_mode);

	iRet = rm_tch_ctrl_clear_int();

	u32Flag = rm_tch_ctrl_configure();

	if (u32Flag & RM_NEED_TO_SEND_SCAN) {
#ifdef ENABLE_SLOW_SCAN
		if (g_stTs.bEnableSlowScan == true) {
			rm_tch_cmd_process((u8)(g_stTs.u32SlowScanLevel - 1),
				g_stRmSlowScanBCmd, NULL);
			//rm_printk("##Raydium - ENABLE_SLOW_SCAN, value = %d", g_stTs.u16ReadPara);
			usleep_range(g_stTs.u16ReadPara, g_stTs.u16ReadPara + 30);
		}
#endif
		rm_tch_ctrl_scan_start();
	}

	if (u32Flag & RM_NEED_TO_READ_RAW_DATA) {
		pKernelBuffer = rm_tch_enqueue_start();
		if (pKernelBuffer) {
			iRet = rm_tch_read_image_data((u8 *) pKernelBuffer);
			if (iRet)
				rm_tch_enqueue_finish();
		}
	}
	mutex_unlock(&g_stTs.mutex_scan_mode);

	if (u32Flag & RM_NEED_TO_SEND_SIGNAL) {
		if (g_stTs.bCalcFinish) {
			g_stTs.bCalcFinish = 0;
			rm_tch_ts_send_signal(g_stTs.ulHalPID, RM_SIGNAL_INTR);
		}
	}
}

static void rm_tch_init_ts_structure_part(void)
{
	g_stTs.bInitFinish = 0;
	g_stTs.bCalcFinish = 0;
	g_stTs.bEnableScriber = 0;
	g_stTs.bIsSuspended = 0;
#ifdef ENABLE_SLOW_SCAN
	g_stTs.bEnableSlowScan = false;
#endif
	g_stTs.u8ScanModeState = RM_SCAN_ACTIVE_MODE;
	g_stTs.u8ReportMode = EVENT_REPORT_MODE_STYLUS_ERASER_FINGER;

	g_pu8BurstReadBuf = NULL;
	g_is_flush = false;
	g_stTs.u16ReadPara = 0;

	rm_ctrl_watchdog_func(0);
	rm_tch_ctrl_init();
}
/*===========================================================================*/
static void rm_ctrl_watchdog_func(unsigned int u32Enable)
{
	g_stTs.u8WatchDogFlg = 0;
	g_stTs.u32WatchDogCnt = 0;
	g_stTs.u8WatchDogCheck = 0;

	g_stTs.u8WatchDogEnable = u32Enable & 0x01;
	g_stTs.u32WatchDogTime = u32Enable >> 16;

	if (u32Enable&0x01) {
		g_stTs.u8WatchDogEnable = 1;
		g_stTs.u32WatchDogTime = u32Enable >> 16;
	} else {
		g_stTs.u8WatchDogEnable = 0;
		g_stTs.u32WatchDogTime = 0xFFFFFFFF;
	}

	if (g_stCtrl.bKernelMsg & DEBUG_DRIVER)
		rm_printk("Raydium - WatchDogEnable=%d, WatchDogTime=%d\n",
			g_stTs.u8WatchDogEnable, g_stTs.u32WatchDogTime);

}

static void rm_watchdog_work_function(unsigned char scan_mode)
{
	if ((g_stTs.u8WatchDogEnable == 0) || (g_stTs.bInitFinish == 0))
		return;

	if (g_stTs.u32WatchDogCnt++ >= g_stTs.u32WatchDogTime) {
		if (g_stCtrl.bKernelMsg & DEBUG_DRIVER)
			rm_printk("Raydium - watchdog work: Time:%dsec Cnt:%d,Flg:%d(%x)\n",
				g_stTs.u32WatchDogTime,
				g_stTs.u32WatchDogCnt,
				g_stTs.u8WatchDogFlg,
				g_stTs.u8ScanModeState);

		switch (scan_mode) {
		case RM_SCAN_ACTIVE_MODE:
			g_stTs.u32WatchDogCnt = 0;
			g_stTs.u8WatchDogFlg = 1;
			break;
		case RM_SCAN_IDLE_MODE:
			g_stTs.u32WatchDogCnt = 0;
			g_stTs.u8WatchDogCheck = 1;
			break;
		}
	}

	if (g_stTs.u8WatchDogFlg) {
		/*WATCH DOG RESET*/
		rm_printk("Raydium - WatchDog Resume\n");
		rm_tch_init_ts_structure_part();
		g_stTs.bIsSuspended = true;
		rm_tch_cmd_process(0, g_stRmWatchdogCmd, NULL);
		g_stTs.bIsSuspended = false;
		return;
	}

}

static u8 rm_timer_trigger_function(void)
{
	static u32 u32TimerCnt; /*= 0; remove by checkpatch*/

	if (u32TimerCnt++ < g_stCtrl.bTimerTriggerScale) {
		return 0;
	} else {
		/*rm_printk("Raydium - rm_timer_work_handler:%x,%x\n",
		g_stCtrl.bTimerTriggerScale, u32TimerCnt);*/
		u32TimerCnt = 0;
		return 1;
	}

}

static void rm_timer_work_handler(struct work_struct *work)
{
	static u16 u32TimerCnt; /*= 0; remove by checkpatch*/
	if (g_stTs.bIsSuspended)
		return;

	if (g_is_flush == true)
		return;

	mutex_lock(&g_stTs.mutex_scan_mode);
	if (rm_timer_trigger_function()) {
		if (g_stTs.u8ScanModeState != RM_SCAN_ACTIVE_MODE)
			rm_watchdog_work_function(RM_SCAN_IDLE_MODE);
		else
			rm_watchdog_work_function(RM_SCAN_ACTIVE_MODE);
	}
	mutex_unlock(&g_stTs.mutex_scan_mode);

	if (g_stTs.u8WatchDogCheck == 1) {
		rm_tch_ts_send_signal(g_stTs.ulHalPID,
			RM_SIGNAL_WATCH_DOG_CHECK);
		g_stTs.u8WatchDogCheck = 0;
	}

	if (g_stTs.u8WatchDogEnable) {
		u32TimerCnt++;
		if (u32TimerCnt > g_stTs.u32WatchDogTime &&
			g_stTs.u32WatchDogTime != g_stCtrl.bWatchDogNormalCnt) {
			g_stTs.u32WatchDogTime = g_stCtrl.bWatchDogNormalCnt;
			if (g_stCtrl.bKernelMsg & DEBUG_DRIVER)
				rm_printk("Raydium - WDT:Normal mode\n");
			u32TimerCnt = 0;
		}
	} else {
		u32TimerCnt = 0;
	}
}

/*========================================================================= */
static void rm_tch_enable_irq(struct rm_tch_ts *ts)
{
	enable_irq(ts->irq);
}

static void rm_tch_disable_irq(struct rm_tch_ts *ts)
{
	disable_irq(ts->irq);
}

#ifdef ENABLE_SLOW_SCAN
/*=============================================================================
 * Description:
 *		Context dependent touch system.
 *		Change scan speed for slowscan function.
 *		Change scan speed flow: (by CY,20120305)
 *		1.Disable auto scan ([0x09]bit4=0,[0x09]bit6=1)
 *		2.Clear Scan start bit ([0x11]bit0=0)
 *		3.Read Scan start bit until it equals 0
 *		4.Set LACTIVE and YACTIVE configuration
 *		5.Enable autoscan ([0x09]bit4=1,[0x09]bit6=1)
 *		6.Sleep 1 minisecond.
 *		7.Set Scan start bit ([0x11]bit0=1)
 * Input:
 *		N/A
 * Output:
 *		N/A
 *=============================================================================
*/
static void rm_tch_ctrl_slowscan(u32 level)
{
	if (g_stTs.u8ScanModeState == RM_SCAN_IDLE_MODE) {
		rm_tch_ctrl_leave_auto_mode();
		rm_ctrl_pause_auto_mode();
	}

	rm_tch_ctrl_wait_for_scan_finish();

	if (level == RM_SLOW_SCAN_LEVEL_NORMAL)
		level = RM_SLOW_SCAN_LEVEL_20;

	if (level > RM_SLOW_SCAN_LEVEL_100)
		level = RM_SLOW_SCAN_LEVEL_MAX;

	rm_tch_cmd_process((u8)(level - 1), g_stRmSlowScanCmd, NULL);

	rm_printk("Raydium - rm_tch_ctrl_slowscan:%x,%x,%x\n",
		(level - 1),
		g_stRmSlowScanCmd[0],
		g_stRmSlowScanCmd[1]);

	if (g_stTs.u8ScanModeState == RM_SCAN_IDLE_MODE) {
		rm_tch_ctrl_enter_auto_mode();
		usleep_range(1000, 1200);
		rm_tch_ctrl_scan_start();
	}
}

static u32 rm_tch_slowscan_round(u32 val)
{
	u32 i;
	for (i = 0; i < RM_SLOW_SCAN_LEVEL_COUNT; i++) {
		if ((i * RM_SLOW_SCAN_INTERVAL) >= val)
			break;
	}
	if(i > RM_SLOW_SCAN_LEVEL_MAX)
		i = RM_SLOW_SCAN_LEVEL_MAX;
	
	return i;
}

static ssize_t rm_tch_slowscan_handler(const char *buf, size_t count)
{
	unsigned long val;
	ssize_t error;
	ssize_t ret;

	if (count < 2)
		return -EINVAL;

	ret = (ssize_t) count;

	mutex_lock(&g_stTs.mutex_scan_mode);

	if (count == 2) {
		if (buf[0] == '0') {
			g_stTs.bEnableSlowScan = false;
			rm_tch_ctrl_slowscan(RM_SLOW_SCAN_LEVEL_MAX);
		} else if (buf[0] == '1') {
			g_stTs.bEnableSlowScan = true;
			rm_tch_ctrl_slowscan(RM_SLOW_SCAN_LEVEL_60);
			g_stTs.u32SlowScanLevel = RM_SLOW_SCAN_LEVEL_60;
		}
	} else if ((buf[0] == '2') && (buf[1] == ' ')) {
		error = kstrtoul(&buf[2], 10, &val);

		if (error) {
			ret = error;
		} else {
			g_stTs.bEnableSlowScan = true;
			g_stTs.u32SlowScanLevel = rm_tch_slowscan_round((u32)val);
			rm_tch_ctrl_slowscan(g_stTs.u32SlowScanLevel);
		}
	}

	mutex_unlock(&g_stTs.mutex_scan_mode);
	return ret;
}
#endif

static ssize_t rm_tch_slowscan_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
#ifdef ENABLE_SLOW_SCAN
	return sprintf(buf, "Slow Scan:%s\nScan Rate:%dHz\n",
			g_stTs.bEnableSlowScan ?
			"Enabled" : "Disabled",
			g_stTs.u32SlowScanLevel * RM_SLOW_SCAN_INTERVAL);
#else
	return sprintf(buf, "Not implemented yet\n");
#endif
}

static ssize_t rm_tch_slowscan_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
#ifdef ENABLE_SLOW_SCAN
	return rm_tch_slowscan_handler(buf, count);
#else
	return count;
#endif
}

static void rm_tch_smooth_level_change(unsigned long val)
{
	int iInfo;

	if (val > RM_SMOOTH_LEVEL_MAX)
		return;

	g_stTs.u32SmoothLevel = val;

	iInfo = (RM_SIGNAL_PARA_SMOOTH << 24) |
			(val << 16) |
			RM_SIGNAL_CHANGE_PARA;

	rm_tch_ts_send_signal(g_stTs.ulHalPID, iInfo);
}

static ssize_t rm_tch_smooth_level_handler(const char *buf, size_t count)
{
	unsigned long val;
	ssize_t error;
	ssize_t ret;

	if (count != 2)
		return count;

	ret = (ssize_t) count;
	error = kstrtoul(buf, 10, &val);

	if (error)
		ret = error;
	else
		rm_tch_smooth_level_change(val);

	return ret;
}

static ssize_t rm_tch_smooth_level_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return sprintf(buf, "Smooth level:%d\n", g_stTs.u32SmoothLevel);
}

static ssize_t rm_tch_smooth_level_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	rm_tch_smooth_level_handler(buf, count);
	return count;
}

static ssize_t rm_tch_self_test_handler(struct rm_tch_ts *ts,
	const char *buf, size_t count)
{
	unsigned long val;
	ssize_t error;
	ssize_t ret;
	int iInfo;

	ret = (ssize_t) count;

	if (count != 2)
		return ret;

	if (g_stTs.u8SelfTestStatus == RM_SELF_TEST_STATUS_TESTING)
		return ret;

	rm_tch_enter_test_mode(1);

	g_stTs.u8SelfTestResult = RM_SELF_TEST_RESULT_PASS;

	error = kstrtoul(buf, 10, &val);

	if (error) {
		ret = error;
		rm_tch_enter_test_mode(0);
	} else if (val == 0) {
		rm_tch_enter_test_mode(0);
	} else if ((val >= 0x01) && (val <= 0xFF)) {
		iInfo = (RM_SIGNAL_PARA_SELF_TEST << 24) |
				(val << 16) |
				RM_SIGNAL_CHANGE_PARA;
		rm_tch_ts_send_signal(g_stTs.ulHalPID, iInfo);
	}

	return ret;
}

static ssize_t selftest_enable_set(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	//rm_printk("enter test mode buf[0] = %d", buf[0]);
	rm_tch_enter_test_mode(buf[0]);
	return count;
}

static ssize_t selftest_enable_get(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return 1;
}

static ssize_t rm_tch_self_test_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return sprintf(buf, "Self_Test:Status:%d ,Result:%d\n",
					g_stTs.u8SelfTestStatus,
					g_stTs.u8SelfTestResult);
}

static ssize_t rm_tch_self_test_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct rm_tch_ts *ts = dev_get_drvdata(dev);
	rm_tch_self_test_handler(ts, buf, count);
	return count;
}

static ssize_t rm_tch_version_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return sprintf(buf, "Release V 0x%02X, Test V 0x%02X\n",
		g_stTs.u8Version,
		g_stTs.u8TestVersion);
}

static ssize_t rm_tch_version_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	return count;
}

static ssize_t rm_tch_module_detect_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return sprintf(buf, "%s\n", "Raydium Touch Module");
}

static ssize_t rm_tch_module_detect_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	return count;
}

static void rm_tch_report_mode_change(unsigned long val)
{
	int iInfo;

	g_stTs.u8ReportMode = (u8)val;

	iInfo = (RM_SIGNAL_PARA_REPORT_MODE_CHANGE << 24) |
			(g_stTs.u8ReportMode << 16) |
			RM_SIGNAL_REPORT_MODE_CHANGE;

	rm_tch_ts_send_signal(g_stTs.ulHalPID, iInfo);
}

static ssize_t rm_tch_report_mode_handler(const char *buf, size_t count)
{
	unsigned long val;
	ssize_t error;
	ssize_t ret;

	if (count != 2)
		return count;

	ret = (ssize_t) count;
	error = kstrtoul(buf, 10, &val);

	if (error)
		ret = error;
	else if ((val >= EVENT_REPORT_MODE_STYLUS_ERASER_FINGER) && (val <EVENT_REPORT_MODE_TYPE_NUM))
		rm_tch_report_mode_change(val);
	else
		ret = FAIL;

	return ret;
}

static ssize_t rm_tch_report_mode_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return sprintf(buf, "Raydium Touch Report Mode : %d\n", g_stTs.u8ReportMode);
}

static ssize_t rm_tch_report_mode_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	rm_tch_report_mode_handler(buf, count);
	return count;
}

static ssize_t selftest_SPI_byte_read_get(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	buf[0] = spi_buf[0];
	return 2;
}

static ssize_t selftest_SPI_byte_read_set(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	return rm_tch_spi_byte_read(buf[0], &spi_buf[0]);
}

static ssize_t selftest_SPI_byte_write_get(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 1;
}

static ssize_t selftest_SPI_byte_write_set(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	return rm_tch_spi_byte_write(buf[0], buf[1]);
}

static ssize_t selftest_SPI_burst_read_get(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	memcpy(buf, spi_buf, bufsize);
	return bufsize;
}


static ssize_t selftest_SPI_burst_read_set(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	bufsize = count;
	return rm_tch_spi_burst_read(buf[0], spi_buf, bufsize);
}

static ssize_t selftest_SPI_burst_write_get(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 1;
}

static ssize_t selftest_SPI_burst_write_set(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	memcpy(spi_buf, buf, count);
	return rm_tch_spi_burst_write(spi_buf, count);
}

static DEVICE_ATTR(selftest_enable, 0640, selftest_enable_get,
					selftest_enable_set);

static DEVICE_ATTR(selftest_spi_byte_read, 0640, selftest_SPI_byte_read_get,
					selftest_SPI_byte_read_set);

static DEVICE_ATTR(selftest_spi_byte_write, 0640, selftest_SPI_byte_write_get,
					selftest_SPI_byte_write_set);

static DEVICE_ATTR(selftest_spi_burst_read, 0640, selftest_SPI_burst_read_get,
					selftest_SPI_burst_read_set);

static DEVICE_ATTR(selftest_spi_burst_write, 0640, selftest_SPI_burst_write_get,
					selftest_SPI_burst_write_set);

static DEVICE_ATTR(slowscan_enable, 0640, rm_tch_slowscan_show,
					rm_tch_slowscan_store);
static DEVICE_ATTR(smooth_level, 0640, rm_tch_smooth_level_show,
					rm_tch_smooth_level_store);
static DEVICE_ATTR(self_test, 0640, rm_tch_self_test_show,
					rm_tch_self_test_store);
static DEVICE_ATTR(version, 0640, rm_tch_version_show,
					rm_tch_version_store);
static DEVICE_ATTR(module_detect, 0640, rm_tch_module_detect_show,
					rm_tch_module_detect_store);
static DEVICE_ATTR(report_mode, 0640, rm_tch_report_mode_show,
					rm_tch_report_mode_store);

static struct attribute *rm_ts_attributes[] = {
	&dev_attr_slowscan_enable.attr,
	&dev_attr_selftest_enable.attr,
	&dev_attr_selftest_spi_byte_read.attr,
	&dev_attr_selftest_spi_byte_write.attr,
	&dev_attr_selftest_spi_burst_read.attr,
	&dev_attr_selftest_spi_burst_write.attr,
	&dev_attr_smooth_level.attr,
	&dev_attr_self_test.attr,
	&dev_attr_version.attr,
	&dev_attr_module_detect.attr,
	&dev_attr_report_mode.attr,
	NULL
};

static const struct attribute_group rm_ts_attr_group = {
	.attrs = rm_ts_attributes,
};

static int rm_tch_input_open(struct input_dev *input)
{
	struct rm_tch_ts *ts = input_get_drvdata(input);

	rm_tch_enable_irq(ts);

	return 0;
}

static void rm_tch_input_close(struct input_dev *input)
{
	struct rm_tch_ts *ts = input_get_drvdata(input);

	rm_tch_disable_irq(ts);
}

static irqreturn_t rm_tch_irq(int irq, void *handle)
{
	g_stTs.u32WatchDogCnt = 0;

	trace_touchscreen_raydium_irq("Raydium_interrupt");

	mutex_lock(&g_stTs.mutex_scan_mode);

	if (g_stCtrl.bfPowerMode &&
			(g_stTs.u8ScanModeState == RM_SCAN_IDLE_MODE))
#ifndef INPUT_DEVICE_MULTIPLE_INSTANCES
		input_event(g_input_dev, EV_MSC, MSC_ACTIVITY, 1);
#else
		input_event(g_input_dev[0], EV_MSC, MSC_ACTIVITY, 1);
#endif
	mutex_unlock(&g_stTs.mutex_scan_mode);

	if (g_stTs.bInitFinish && g_stTs.bIsSuspended == false)
		queue_work(g_stTs.rm_workqueue, &g_stTs.rm_work);

	return IRQ_HANDLED;
}

static void rm_tch_enter_test_mode(u8 flag)
{
	if (flag) { /*enter test mode*/
		g_stTs.u8SelfTestStatus = RM_SELF_TEST_STATUS_TESTING;
		g_stTs.bIsSuspended = true;
		flush_workqueue(g_stTs.rm_workqueue);
		flush_workqueue(g_stTs.rm_timer_workqueue);
	} else {/*leave test mode*/
		g_stTs.bIsSuspended = false;
		rm_tch_init_ts_structure_part();
	}

	rm_tch_cmd_process(flag, g_stRmTestModeCmd, NULL);

	if (!flag)
		g_stTs.u8SelfTestStatus = RM_SELF_TEST_STATUS_FINISH;
}

void rm_tch_set_variable(unsigned int index, unsigned int arg)
{
	switch (index) {
	case RM_VARIABLE_SELF_TEST_RESULT:
		g_stTs.u8SelfTestResult = (u8) arg;
		rm_tch_enter_test_mode(0);
		break;
	case RM_VARIABLE_SCRIBER_FLAG:
		g_stTs.bEnableScriber = (bool) arg;
		break;
	case RM_VARIABLE_AUTOSCAN_FLAG:
		g_stCtrl.bfPowerMode = (bool) arg;
		break;
	case RM_VARIABLE_TEST_VERSION:
		g_stTs.u8TestVersion = (u8) arg;
		break;
	case RM_VARIABLE_VERSION:
		g_stTs.u8Version = (u8) arg;
		dev_info(&g_spi->dev, "Raydium - Firmware v%d.%d\n",
				g_stTs.u8Version, g_stTs.u8TestVersion);
		break;
	case RM_VARIABLE_IDLEMODECHECK:
		g_stCtrl.bfIdleModeCheck = (u8) arg;
		if (g_stCtrl.bKernelMsg & DEBUG_DRIVER)
			rm_printk("Raydium - bfIdleModeCheck %2x\n",
				arg);
		break;
	case RM_VARIABLE_REPEAT:
		/*rm_printk("Raydium - Repeat %d\n", arg);*/
		g_stTs.u8Repeat = (u8) arg;
		if(g_stCtrl.bNsFucnEnable) {
			g_stTs.bNsRpt = (u8) arg;
			if(g_stCtrl.bKernelMsg & DEBUG_DRIVER)
				rm_printk("g_stCtrl.bNsRpt %d\n",
					g_stTs.bNsRpt);
		}
		break;
	case RM_VARIABLE_WATCHDOG_FLAG:
		rm_ctrl_watchdog_func(arg);
		g_stCtrl.bNsFucnEnable = ((u8)arg >> 7) & 0x01;
		if(g_stCtrl.bKernelMsg & DEBUG_DRIVER)
			rm_printk("g_stCtrl.bKernelMsg %d\n",
				g_stCtrl.bNsFucnEnable);
		break;
	case RM_VARIABLE_SET_SPI_UNLOCK:
		if (g_stTs.u8ResumeCnt > 1)
			break;
		g_stTs.u8SPILocked = 0;
		if (g_stCtrl.bKernelMsg & DEBUG_DRIVER)
			rm_printk("Raydium - SET_SPI_UNLOCK\n");
		break;
	case RM_VARIABLE_SET_WAKE_UNLOCK:
		if (wake_lock_active(&g_stTs.Wakelock_Initialization))
			wake_unlock(&g_stTs.Wakelock_Initialization);
		break;
	case RM_VARIABLE_DPW:
		mutex_lock(&g_stTs.mutex_ns_mode);
		memcpy(&g_stTs.bDP[0], ((u8 *)arg), 3);
		memcpy(&g_stTs.bSWCPW[0], (((u8 *)arg) + 3), 3);
		memcpy(&g_stTs.bSWC[0], (((u8 *)arg) + 6), 3);
		mutex_unlock(&g_stTs.mutex_ns_mode);
		if (g_stCtrl.bKernelMsg & DEBUG_DRIVER)
			rm_printk("g_stCtrl.DPW 0x%x:0x%x:0x%x # 0x%x:0x%x:0x%x\n",
				g_stTs.bDP[0], g_stTs.bDP[1],
				g_stTs.bDP[2], g_stTs.bSWCPW[0],
				g_stTs.bSWCPW[1], g_stTs.bSWCPW[2]);
		break;
	case RM_VARIABLE_NS_MODE:
		mutex_lock(&g_stTs.mutex_ns_mode);
		g_stTs.bNsMode = (u8)arg;
		mutex_unlock(&g_stTs.mutex_ns_mode);
		if (g_stCtrl.bKernelMsg & DEBUG_DRIVER)
			rm_printk("g_stCtrl.bNsMode=%d\n", g_stTs.bNsMode);
		break;
	default:
		break;
	}

}
static u32 rm_tch_get_variable(unsigned int index, unsigned int arg)
{
	u32 ret = 0;
	switch (index) {
	case RM_VARIABLE_PLATFORM_ID:
		ret = rm_tch_get_platform_id((u32 *) arg);
		break;
	case RM_VARIABLE_GPIO_SELECT:
		ret = rm_tch_get_gpio_sensor_select((u8 *) arg);
		break;
	case RM_VARIABLE_CHECK_SPI_LOCK:
		ret = rm_tch_get_spi_lock_status((u8 *) arg);
		break;
	case RM_VARIABLE_REPORT_MODE:
		ret = rm_tch_get_report_mode((u8 *) arg);
		break;
	default:
		break;
	}
	return ret;
}

static void rm_tch_init_ts_structure(void)
{
	g_stTs.ulHalPID = 0;
	memset(&g_stTs, 0, sizeof(struct rm31080a_ts_para));

#ifdef ENABLE_SLOW_SCAN
	g_stTs.u32SlowScanLevel = RM_SLOW_SCAN_LEVEL_MAX;
#endif

	g_stTs.rm_workqueue = create_singlethread_workqueue("rm_work");
	INIT_WORK(&g_stTs.rm_work, rm_work_handler);

	g_stTs.rm_timer_workqueue =
		create_singlethread_workqueue("rm_idle_work");
	INIT_WORK(&g_stTs.rm_timer_work, rm_timer_work_handler);

	wake_lock_init(&g_stTs.Wakelock_Initialization,
		WAKE_LOCK_SUSPEND, "TouchInitialLock");

	mutex_init(&g_stTs.mutex_scan_mode);
	mutex_init(&g_stTs.mutex_spi_rw);
	mutex_init(&g_stTs.mutex_ns_mode);

	g_stTs.u8ResumeCnt = 0;
}

static int rm31080_voltage_notifier_1v8(struct notifier_block *nb,
	unsigned long event, void *ignored)
{
	if (g_stCtrl.bKernelMsg & DEBUG_DRIVER)
		rm_printk("Raydium - REGULATOR EVENT 1.8V:0x%x\n",
			(unsigned int)event);

	return NOTIFY_OK;
}

static int rm31080_voltage_notifier_3v3(struct notifier_block *nb,
	unsigned long event, void *ignored)
{
	if (g_stCtrl.bKernelMsg & DEBUG_DRIVER)
		rm_printk("Raydium - REGULATOR EVENT 3.3V:0x%x\n",
			(unsigned int)event);

	return NOTIFY_OK;
}

/*===========================================================================*/
static void rm_ctrl_resume(struct rm_tch_ts *ts)
{
	if (g_stTs.bInitFinish)
		return;

	mutex_lock(&g_stTs.mutex_scan_mode);

	rm_tch_init_ts_structure_part();

	rm_tch_cmd_process(0, g_stRmResumeCmd, ts);

	mutex_unlock(&g_stTs.mutex_scan_mode);
}

static void rm_ctrl_suspend(struct rm_tch_ts *ts)
{
	if (g_stTs.bIsSuspended == true)
		return;

	g_stTs.bIsSuspended = true;
	g_stTs.bInitFinish = 0;

	if (g_stTs.u8ScanModeState == RM_SCAN_IDLE_MODE)
		rm_ctrl_pause_auto_mode();

	rm_tch_ctrl_wait_for_scan_finish();

	mutex_lock(&g_stTs.mutex_scan_mode);

	rm_tch_cmd_process(0, g_stRmSuspendCmd, ts);

	rm_tch_ctrl_wait_for_scan_finish();

	rm_tch_cmd_process(1, g_stRmSuspendCmd, ts);

	mutex_unlock(&g_stTs.mutex_scan_mode);
}

#ifdef CONFIG_PM
static int rm_tch_suspend(struct device *dev)
{
	struct rm_tch_ts *ts = dev_get_drvdata(dev);

	rm_ctrl_suspend(ts);

	g_stTs.u8SPILocked = 1;
	if (g_stCtrl.bKernelMsg & DEBUG_DRIVER)
		rm_printk("Raydium - SPI_LOCKED by suspend!!\n");

	return 0;
}

static int rm_tch_resume(struct device *dev)
{
	struct rm_tch_ts *ts = dev_get_drvdata(dev);

	if (wake_lock_active(&g_stTs.Wakelock_Initialization))
		wake_unlock(&g_stTs.Wakelock_Initialization);

	wake_lock_timeout(&g_stTs.Wakelock_Initialization,
		TCH_WAKE_LOCK_TIMEOUT);

	g_stTs.u8SPILocked = 1;
	if (g_stCtrl.bKernelMsg & DEBUG_DRIVER)
		rm_printk("Raydium - SPI_LOCKED by resume!!\n");

	if (wake_lock_active(&g_stTs.Wakelock_Initialization))
		rm_ctrl_resume(ts);

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void rm_tch_early_suspend(struct early_suspend *es)
{
	struct rm_tch_ts *ts;
	struct device *dev;

	ts = container_of(es, struct rm_tch_ts, early_suspend);
	dev = ts->dev;

	dev_info(ts->dev, "Raydium - Suspend input device\n");
	if (rm_tch_suspend(dev) != 0)
		dev_err(dev, "Raydium - %s : failed\n", __func__);
}

static void rm_tch_early_resume(struct early_suspend *es)
{
	struct rm_tch_ts *ts;
	struct device *dev;

	ts = container_of(es, struct rm_tch_ts, early_suspend);
	dev = ts->dev;

	dev_info(ts->dev, "Raydium - Resume input device\n");
	if (rm_tch_resume(dev) != 0)
		dev_err(dev, "Raydium - %s : failed\n", __func__);
}
#else
static const struct dev_pm_ops rm_tch_pm_ops = {
	.suspend = rm_tch_suspend,
	.resume = rm_tch_resume,
};
#endif			/*CONFIG_HAS_EARLYSUSPEND*/
#endif			/*CONFIG_PM*/

/* NVIDIA 20121026 */
/* support to disable power and clock when display is off */
static int rm_tch_input_enable(struct input_dev *in_dev)
{
	int error = 0;

#ifdef CONFIG_PM
	struct rm_tch_ts *ts = input_get_drvdata(in_dev);

	dev_info(ts->dev, "Raydium - Enable input device\n");
	error = rm_tch_resume(ts->dev);
	if (error)
		dev_err(ts->dev, "Raydium - %s : failed\n", __func__);
#endif

	return error;
}

static int rm_tch_input_disable(struct input_dev *in_dev)
{
	int error = 0;

#ifdef CONFIG_PM
	struct rm_tch_ts *ts = input_get_drvdata(in_dev);

	dev_info(ts->dev, "Raydium - Disable input device\n");
	error = rm_tch_suspend(ts->dev);
	if (error)
		dev_err(ts->dev, "Raydium - %s : failed\n", __func__);
	else
		dev_info(ts->dev, "Raydium - Disable input device done\n");
#endif

	return error;
}

static void rm_tch_set_input_resolution(unsigned int x, unsigned int y)
{
#ifndef INPUT_DEVICE_MULTIPLE_INSTANCES
	input_set_abs_params(g_input_dev, ABS_MT_POSITION_X, 0, x - 1, 0, 0);
	input_set_abs_params(g_input_dev, ABS_MT_POSITION_Y, 0, y - 1, 0, 0);
#else
	u8 i;
	for (i = 0 ; i < INPUT_DEVICE_AMOUNT ; i++) {
		input_set_abs_params(g_input_dev[i], ABS_MT_POSITION_X,
			0, x - 1, 0, 0);
		input_set_abs_params(g_input_dev[i], ABS_MT_POSITION_Y,
			0, y - 1, 0, 0);
	}
#endif
}

#ifndef INPUT_DEVICE_MULTIPLE_INSTANCES
struct rm_tch_ts *rm_tch_input_init(struct device *dev, unsigned int irq,
	const struct rm_tch_bus_ops *bops)
{
	struct rm_tch_ts *ts;
	struct input_dev *input_dev;
	struct rm_spi_ts_platform_data *pdata;
	int err;

	if (!irq) {
		dev_err(dev, "Raydium - no IRQ?\n");
		err = -EINVAL;
		goto err_out;
	}

	ts = kzalloc(sizeof(struct rm_tch_ts), GFP_KERNEL);

	input_dev = input_allocate_device();

	if (!ts || !input_dev) {
		dev_err(dev, "Raydium - Failed to allocate memory\n");
		err = -ENOMEM;
		goto err_free_mem;
	}

	g_input_dev = input_dev;

	ts->bops = bops;
	ts->dev = dev;
	ts->input = input_dev;
	ts->irq = irq;


	if (dev->of_node) {
		pr_info("Load platform data from DT.\n");
		pdata = rm_ts_parse_dt(dev, irq);
		if (IS_ERR(pdata)) {
			dev_err(&g_spi->dev, "Raydium - failed to parse dt\n");
			err = -EINVAL;
			goto err_free_mem;
		}
		dev->platform_data = pdata;
	}
	pdata = dev->platform_data;

	if (pdata->name_of_clock || pdata->name_of_clock_con) {
		ts->clk = clk_get_sys(pdata->name_of_clock,
			pdata->name_of_clock_con);
		if (IS_ERR(ts->clk)) {
			dev_err(&g_spi->dev, "Raydium - failed to get touch_clk: (%s, %s)\n",
				pdata->name_of_clock, pdata->name_of_clock_con);
			err = -EINVAL;
			goto err_free_mem;
		}
	}

	snprintf(ts->phys, sizeof(ts->phys), "%s/input0", dev_name(dev));

	input_dev->name = "raydium_ts";
	input_dev->phys = ts->phys;
	input_dev->dev.parent = dev;
	input_dev->id.bustype = bops->bustype;

	input_dev->enable = rm_tch_input_enable;
	input_dev->disable = rm_tch_input_disable;
	input_dev->enabled = true;
	input_dev->open = rm_tch_input_open;
	input_dev->close = rm_tch_input_close;
	input_dev->hint_events_per_packet = 256U;

	input_set_drvdata(input_dev, ts);
	input_set_capability(input_dev, EV_MSC, MSC_ACTIVITY);

	__set_bit(EV_ABS, input_dev->evbit);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 0xFF, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, 32, 0, 0);

	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOOL_RUBBER, input_dev->keybit);
	input_set_abs_params(input_dev,
		ABS_MT_TOOL_TYPE, 0,
		MT_TOOL_MAX, 0, 0);

	rm_tch_set_input_resolution(RM_INPUT_RESOLUTION_X,
				RM_INPUT_RESOLUTION_Y);

	err = request_threaded_irq(ts->irq, NULL, rm_tch_irq,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT, dev_name(dev), ts);
	if (err) {
		dev_err(dev, "Raydium - irq %d busy?\n", ts->irq);
		goto err_free_mem;
	}
	mutex_init(&ts->access_mutex);
#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = rm_tch_early_suspend;
	ts->early_suspend.resume = rm_tch_early_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	rm_tch_disable_irq(ts);

	err = sysfs_create_group(&dev->kobj, &rm_ts_attr_group);
	if (err)
		goto err_free_irq;

	err = input_register_device(input_dev);
	if (err)
		goto err_remove_attr;

	return ts;

err_remove_attr:
	sysfs_remove_group(&dev->kobj, &rm_ts_attr_group);
err_free_irq:
	free_irq(ts->irq, ts);
err_free_mem:
	input_free_device(input_dev);
	kfree(ts);
err_out:
	return ERR_PTR(err);
}
#else /*#ifdef INPUT_DEVICE_MULTIPLE_INSTANCES*/
struct rm_tch_ts *rm_tch_input_init(struct device *dev, unsigned int irq,
	const struct rm_tch_bus_ops *bops) {

	struct rm_tch_ts *ts;
	struct rm_spi_ts_platform_data *pdata;
	u8 index;
	int err = -EINVAL;

	if (!irq) {
		dev_err(dev, "Raydium - no IRQ?\n");
		err = -EINVAL;
		goto err_out;
	}

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (!ts) {
		dev_err(dev, "Raydium - Failed to allocate memory for driver data\n");
		err = -ENOMEM;
		goto err_free_ts_mem;
	}

	ts->bops = bops;
	ts->dev = dev;
	ts->irq = irq;
	if (dev->of_node) {
		pr_info("Load platform data from DT.\n");
		pdata = rm_ts_parse_dt(dev, irq);
		if (IS_ERR(pdata)) {
			dev_err(&g_spi->dev, "Raydium - failed to parse dt\n");
			err = -EINVAL;
			goto err_free_mem;
		}
		dev->platform_data = pdata;
	}
	pdata = dev->platform_data;
	if (pdata->name_of_clock || pdata->name_of_clock_con) {
		ts->clk = clk_get_sys(pdata->name_of_clock,
			pdata->name_of_clock_con);
		if (IS_ERR(ts->clk)) {
			dev_err(&g_spi->dev,
				"Raydium - failed to get touch_clk: (%s, %s)\n",
				pdata->name_of_clock, pdata->name_of_clock_con);
			err = -EINVAL;
			goto err_free_ts_mem;
		}
	}

	mutex_init(&ts->access_mutex);

	for (index = 0 ; index < INPUT_DEVICE_AMOUNT ; index++)
		if (ts->input[index] != NULL)
			goto err_free_mem;

	for (index = 0 ; index < INPUT_DEVICE_AMOUNT ; index++) {
		ts->input[index] = input_allocate_device();
		if (!ts->input[index]) {
			dev_err(dev,
				"Raydium - Failed to allocate input device %d\n",
				index);
			err = -ENOMEM;
			goto err_free_mem;
		}

		g_input_dev[index] = ts->input[index];

		snprintf(ts->phys, sizeof(ts->phys), "%s/input%d",
			dev_name(dev), index);

		ts->input[index]->name = "raydium_ts";
		ts->input[index]->phys = ts->phys;
		ts->input[index]->dev.parent = dev;
		ts->input[index]->id.bustype = bops->bustype;

		if (index == INPUT_DEVICE_FOR_FINGER) {
			ts->input[index]->enable = rm_tch_input_enable;
			ts->input[index]->disable = rm_tch_input_disable;
			ts->input[index]->enabled = true;
			ts->input[index]->open = rm_tch_input_open;
			ts->input[index]->close = rm_tch_input_close;
			ts->input[index]->hint_events_per_packet = 256U;
			input_set_drvdata(ts->input[index], ts);
			input_set_capability(ts->input[index],
				EV_MSC, MSC_ACTIVITY);
		}


		__set_bit(EV_SYN, ts->input[index]->evbit);

		__set_bit(EV_ABS, ts->input[index]->evbit);
		input_set_abs_params(ts->input[index],
			ABS_MT_PRESSURE, 0, 0xFF, 0, 0);
		input_set_abs_params(ts->input[index],
			ABS_MT_TRACKING_ID, 0, 32, 0, 0);

		if (index != INPUT_DEVICE_FOR_FINGER) {
			__set_bit(EV_KEY, ts->input[index]->evbit);
			__set_bit(BTN_TOOL_RUBBER, ts->input[index]->keybit);
		}

		if (index == INPUT_DEVICE_FOR_FINGER)
			input_set_abs_params(ts->input[index],
				ABS_MT_TOOL_TYPE, 0,
				MT_TOOL_FINGER, 0, 0);
		else
			input_set_abs_params(ts->input[index],
				ABS_MT_TOOL_TYPE, 0,
				MT_TOOL_MAX, 0, 0);

		err = input_register_device(ts->input[index]);
		if (err < 0)
			goto err_free_mem;
	}

	rm_tch_set_input_resolution(RM_INPUT_RESOLUTION_X,
							RM_INPUT_RESOLUTION_Y);

	err = request_threaded_irq(ts->irq, NULL, rm_tch_irq,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT, dev_name(dev), ts);
	if (err) {
		dev_err(dev, "Raydium - irq %d busy?\n", ts->irq);
		goto err_free_mem;
	}
#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = rm_tch_early_suspend;
	ts->early_suspend.resume = rm_tch_early_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	rm_tch_disable_irq(ts);

	err = sysfs_create_group(&dev->kobj, &rm_ts_attr_group);
	if (err)
		goto err_free_irq;

	return ts;

/*
err_remove_attr:
	sysfs_remove_group(&dev->kobj, &rm_ts_attr_group);
*/
err_free_irq:
	free_irq(ts->irq, ts);
err_free_mem:
	for (index = 0 ; index < INPUT_DEVICE_AMOUNT ; index++) {
		input_free_device(ts->input[index]);
		ts->input[index] = NULL;
	}
err_free_ts_mem:
	kfree(ts);
err_out:
	return ERR_PTR(err);
}
#endif

static int dev_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int dev_release(struct inode *inode, struct file *filp)
{
	g_stTs.bInitFinish = 0;

	rm_tch_enter_manual_mode();
	return 0;
}

static ssize_t
dev_read(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
	ssize_t missing, status;
	int ret;
	u8 *pMyBuf;

	pMyBuf = kmalloc(count, GFP_KERNEL);
	if (pMyBuf == NULL)
		return -ENOMEM;

	pMyBuf[0] = buf[0];
	ret = rm_tch_spi_read(pMyBuf[0], pMyBuf, count);

	if (ret) {
		status = count;
		missing = copy_to_user(buf, pMyBuf, count);

		if (missing == status)
			status = -EFAULT;
		else
			status = status - missing;
	} else {
		status = -EFAULT;
		rm_printk("Raydium - rm_tch_spi_read() fail\n");
	}

	kfree(pMyBuf);
	return status;
}

static ssize_t
dev_write(struct file *filp, const char __user *buf,
	size_t count, loff_t *pos)
{
	u8 *pMyBuf;
	int ret;
	unsigned long missing;
	ssize_t status = 0;

	pMyBuf = kmalloc(count, GFP_KERNEL);
	if (pMyBuf == NULL)
		return -ENOMEM;

	missing = copy_from_user(pMyBuf, buf, count);
	if (missing == 0) {
		ret = rm_tch_spi_write(pMyBuf, count);
		if (ret)
			status = count;
		else
			status = -EFAULT;
	} else
		status = -EFAULT;

	kfree(pMyBuf);
	return status;
}

/*=============================================================================
	Description:
		I/O Control routin.
	Input:
		file:
		cmd :
		arg :
	Output:
		1: succeed
		0: failed
	Note: To avoid context switch,please don't add debug message
		  in this function.
=============================================================================*/
static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = true;
	unsigned int index;
	index = (cmd >> 16) & 0xFFFF;
	switch (cmd & 0xFFFF) {
	case RM_IOCTL_REPORT_POINT:
		raydium_report_pointer((void *)arg);
		break;
	case RM_IOCTL_SET_HAL_PID:
		g_stTs.ulHalPID = arg;
		break;
	case RM_IOCTL_INIT_START:
		g_stTs.bInitFinish = 0;
		rm_tch_enter_manual_mode();
		break;
	case RM_IOCTL_INIT_END:
		g_stTs.bInitFinish = 1;
		g_stTs.bCalcFinish = 1;
		g_stTs.bIsSuspended = false;
		if (g_stTs.u8ResumeCnt)	/* In case issued by boot-up*/
			g_stTs.u8ResumeCnt--;
		if (wake_lock_active(&g_stTs.Wakelock_Initialization))
			wake_unlock(&g_stTs.Wakelock_Initialization);
		rm_printk("Raydium - Enable input device done\n");
		break;
	case RM_IOCTL_FINISH_CALC:
		g_stTs.bCalcFinish = 1;
		break;
	case RM_IOCTL_SCRIBER_CTRL:
		g_stTs.bEnableScriber = (bool) arg;
		break;
	case RM_IOCTL_READ_RAW_DATA:
		ret = rm_tch_queue_read_raw_data((u8 *) arg, index);
		break;
	case RM_IOCTL_SET_PARAMETER:
		rm_tch_ctrl_set_parameter((void *)arg);
		rm_tch_set_input_resolution(g_stCtrl.u16ResolutionX,
			g_stCtrl.u16ResolutionY);
		break;
	case RM_IOCTL_SET_VARIABLE:
		rm_tch_set_variable(index, arg);
		break;
	case RM_IOCTL_GET_SACN_MODE:
		rm_tch_ctrl_get_idle_mode((u8 *) arg);
		break;
	case RM_IOCTL_GET_VARIABLE:
		ret = rm_tch_get_variable(index, arg);
		break;
	case RM_IOCTL_SET_KRL_TBL:
		rm_set_kernel_tbl(index, (u8 *)arg);
		break;
	case RM_IOCTL_WATCH_DOG:
		g_stTs.u8WatchDogFlg = 1;
		g_stTs.u8WatchDogCheck = 0;
		break;
	default:
		break;
	}
	return ret;
}

static const struct file_operations dev_fops = {
	.owner = THIS_MODULE,
	.open = dev_open,
	.release = dev_release,
	.read = dev_read,
	.write = dev_write,
	.unlocked_ioctl = dev_ioctl,
};

static struct miscdevice raydium_ts_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "raydium_ts",
	.fops = &dev_fops,
};

static const struct rm_tch_bus_ops rm_tch_spi_bus_ops = {
	.bustype = BUS_SPI,
};

static void init_ts_timer(void)
{
	init_timer(&ts_timer_triggle);
	ts_timer_triggle.function = ts_timer_triggle_function;
	ts_timer_triggle.data = ((unsigned long) 0);
	ts_timer_triggle.expires = jiffies + TS_TIMER_PERIOD;
}
static void ts_timer_triggle_function(unsigned long option)
{
	queue_work(g_stTs.rm_timer_workqueue, &g_stTs.rm_timer_work);
	ts_timer_triggle.expires = jiffies + TS_TIMER_PERIOD;
	add_timer(&ts_timer_triggle);
}

/*===========================================================================*/
#if ENABLE_SPI_SETTING
static int rm_tch_spi_setting(u32 speed)
{
	int err;
	if ((speed == 0) || (speed > 18))
		return FAIL;

	g_spi->max_speed_hz = speed * 1000 * 1000;
	err = spi_setup(g_spi);
	if (err) {
		dev_dbg(&g_spi->dev, "Raydium - Change SPI setting failed\n");
		return FAIL;
	}
	return OK;
}
#endif

static int rm_tch_spi_remove(struct spi_device *spi)
{
	struct rm_tch_ts *ts = spi_get_drvdata(spi);
#ifdef INPUT_DEVICE_MULTIPLE_INSTANCES
	u8 i;
#endif
	del_timer(&ts_timer_triggle);
	if (g_stTs.rm_timer_workqueue)
		destroy_workqueue(g_stTs.rm_timer_workqueue);

	rm_tch_queue_free();

	if (g_stTs.rm_workqueue)
		destroy_workqueue(g_stTs.rm_workqueue);

	if (&g_stTs.Wakelock_Initialization)
		wake_lock_destroy(&g_stTs.Wakelock_Initialization);

	mutex_destroy(&g_stTs.mutex_scan_mode);
	mutex_destroy(&g_stTs.mutex_spi_rw);

	sysfs_remove_group(&raydium_ts_miscdev.this_device->kobj,
						&rm_ts_attr_group);
	misc_deregister(&raydium_ts_miscdev);
	sysfs_remove_group(&ts->dev->kobj, &rm_ts_attr_group);
	free_irq(ts->irq, ts);
#ifndef INPUT_DEVICE_MULTIPLE_INSTANCES
	input_unregister_device(ts->input);
#else
	for (i = 0 ; i < INPUT_DEVICE_AMOUNT ; i++)
		input_unregister_device(ts->input[i]);
#endif

	if (ts->regulator_3v3 && ts->regulator_1v8) {
		regulator_unregister_notifier(ts->regulator_3v3, &ts->nb_3v3);
		regulator_unregister_notifier(ts->regulator_1v8, &ts->nb_1v8);
		regulator_disable(ts->regulator_3v3);
		regulator_disable(ts->regulator_1v8);
	}

	if (ts->clk)
		clk_disable(ts->clk);

	kfree(ts);
	spi_set_drvdata(spi, NULL);
	return 0;
}

static int rm_tch_regulator_init(struct rm_tch_ts *ts)
{
	int error;

	ts->regulator_3v3 = devm_regulator_get(&g_spi->dev, "avdd");
	if (IS_ERR(ts->regulator_3v3)) {
		dev_err(&g_spi->dev, "Raydium - regulator_get failed: %ld\n",
			PTR_ERR(ts->regulator_3v3));
		goto err_null_regulator;
	}

	ts->regulator_1v8 = devm_regulator_get(&g_spi->dev, "dvdd");
	if (IS_ERR(ts->regulator_1v8)) {
		dev_err(&g_spi->dev, "Raydium - regulator_get failed: %ld\n",
			PTR_ERR(ts->regulator_1v8));
		goto err_null_regulator;
	}

	/* Enable 1v8 first*/
	error = regulator_enable(ts->regulator_1v8);
	if (error < 0)
		dev_err(&g_spi->dev,
			"Raydium - regulator enable failed: %d\n", error);

	usleep_range(5000, 6000);
	/* Enable 1v8 first*/
	error = regulator_enable(ts->regulator_3v3);
	if (error < 0)
		dev_err(&g_spi->dev,
			"Raydium - regulator enable failed: %d\n", error);

	ts->nb_1v8.notifier_call = &rm31080_voltage_notifier_1v8;
	error = regulator_register_notifier(ts->regulator_1v8, &ts->nb_1v8);
	if (error) {
		dev_err(&g_spi->dev,
			"Raydium - regulator notifier request failed: %d\n",
			error);
		goto err_disable_regulator;
	}

	ts->nb_3v3.notifier_call = &rm31080_voltage_notifier_3v3;
	error = regulator_register_notifier(ts->regulator_3v3, &ts->nb_3v3);
	if (error) {
		dev_err(&g_spi->dev,
			"Raydium - regulator notifier request failed: %d\n",
			error);
		goto err_unregister_notifier;
	}

	return 0;

err_unregister_notifier:
	regulator_unregister_notifier(ts->regulator_1v8, &ts->nb_1v8);
err_disable_regulator:
	regulator_disable(ts->regulator_3v3);
	regulator_disable(ts->regulator_1v8);
err_null_regulator:
	ts->regulator_3v3 = NULL;
	ts->regulator_1v8 = NULL;
	return 1;
}

static int rm_tch_spi_probe(struct spi_device *spi)
{
	struct rm_tch_ts *ts;
	struct rm_spi_ts_platform_data *pdata;
	int ret;
#ifdef INPUT_DEVICE_MULTIPLE_INSTANCES
	u8 i;
#endif

	g_spi = spi;

	rm_tch_init_ts_structure();
	rm_tch_init_ts_structure_part();

	if (spi->max_speed_hz > MAX_SPI_FREQ_HZ) {
		dev_err(&spi->dev, "Raydium - SPI CLK %d Hz?\n",
			spi->max_speed_hz);
		ret = -EINVAL;
		goto err_spi_speed;
	}
	ts = rm_tch_input_init(&spi->dev, spi->irq, &rm_tch_spi_bus_ops);
	if (IS_ERR(ts)) {
		dev_err(&spi->dev, "Raydium - Input Device Initialization Fail!\n");
		ret = PTR_ERR(ts);
		goto err_spi_speed;
	}

	spi_set_drvdata(spi, ts);

	if (rm_tch_regulator_init(ts)) {
		dev_err(&spi->dev, "Raydium - regulator Initialization Fail!\n");
		ret = -EINVAL;
		goto err_regulator_init;
	}
#ifndef INPUT_DEVICE_MULTIPLE_INSTANCES
	pdata = g_input_dev->dev.parent->platform_data;
#else
	pdata = g_input_dev[0]->dev.parent->platform_data;
#endif
	usleep_range(5000, 6000);
	if (ts->clk)
		clk_enable(ts->clk);

	gpio_set_value(pdata->gpio_reset, 0);
	msleep(120);
	gpio_set_value(pdata->gpio_reset, 1);
	msleep(20);

	ret = misc_register(&raydium_ts_miscdev);
	if (ret != 0) {
		dev_err(&spi->dev, "Raydium - cannot register miscdev: %d\n",
			ret);
		goto err_misc_reg;
	}
	ret = sysfs_create_group(&raydium_ts_miscdev.this_device->kobj,
						&rm_ts_attr_group);
	if (ret != 0) {
		dev_err(&spi->dev, "Raydium - cannot create group: %d\n",
			ret);
		goto err_create_sysfs;
	}

	ret = rm_tch_queue_init();
	if (ret != 0) {
		dev_err(&spi->dev, "Raydium - could not init queue: %d\n",
			ret);
		goto err_queue_init;
	}

	init_ts_timer();
	add_timer(&ts_timer_triggle);

	rm_printk("Raydium - Spi Probe Done!!\n");
	return 0;

err_queue_init:
	sysfs_remove_group(&raydium_ts_miscdev.this_device->kobj,
						&rm_ts_attr_group);
err_create_sysfs:
	misc_deregister(&raydium_ts_miscdev);
err_misc_reg:
	if (ts->regulator_3v3 && ts->regulator_1v8) {
		regulator_unregister_notifier(ts->regulator_3v3, &ts->nb_3v3);
		regulator_unregister_notifier(ts->regulator_1v8, &ts->nb_1v8);
		regulator_disable(ts->regulator_3v3);
		regulator_disable(ts->regulator_1v8);
	}

	if (ts->clk)
		clk_disable(ts->clk);
err_regulator_init:
	spi_set_drvdata(spi, NULL);
#ifndef INPUT_DEVICE_MULTIPLE_INSTANCES
	input_unregister_device(ts->input);
#else
	for (i = 0 ; i < INPUT_DEVICE_AMOUNT ; i++)
		input_unregister_device(ts->input[i]);
#endif
	sysfs_remove_group(&ts->dev->kobj, &rm_ts_attr_group);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ts->early_suspend);
#endif
	mutex_destroy(&ts->access_mutex);
	free_irq(ts->irq, ts);
#ifndef INPUT_DEVICE_MULTIPLE_INSTANCES
	input_free_device(g_input_dev);
#else
	for (i = 0 ; i < INPUT_DEVICE_AMOUNT ; i++)
		input_free_device(g_input_dev[i]);
#endif
	kfree(ts);
err_spi_speed:
	if (g_stTs.rm_timer_workqueue)
		destroy_workqueue(g_stTs.rm_timer_workqueue);
	if (g_stTs.rm_workqueue)
		destroy_workqueue(g_stTs.rm_workqueue);
	mutex_destroy(&g_stTs.mutex_scan_mode);
	mutex_destroy(&g_stTs.mutex_spi_rw);
	return ret;
}

static const struct of_device_id rm_ts_dt_match[] = {
	{ .compatible = "raydium,rm_ts_spidev" },
	{ },
};
MODULE_DEVICE_TABLE(of, rm_ts_dt_match);

static struct spi_driver rm_tch_spi_driver = {
	.driver = {
		.name = "rm_ts_spidev",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = rm_ts_dt_match,
#endif
#if !defined(CONFIG_HAS_EARLYSUSPEND)
#ifdef CONFIG_PM
		.pm = &rm_tch_pm_ops,
#endif
#endif
	},
	.probe = rm_tch_spi_probe,
	.remove = rm_tch_spi_remove,
};

static int __init rm_tch_spi_init(void)
{
	return spi_register_driver(&rm_tch_spi_driver);
}

static void __exit rm_tch_spi_exit(void)
{
	spi_unregister_driver(&rm_tch_spi_driver);
}

module_init(rm_tch_spi_init);
module_exit(rm_tch_spi_exit);

MODULE_AUTHOR("Valentine Hsu <valentine.hsu@rad-ic.com>");
MODULE_DESCRIPTION("Raydium touchscreen SPI bus driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:raydium-t007");
