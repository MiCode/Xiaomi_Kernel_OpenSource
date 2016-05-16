/*
 *
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * @file	mstar_drv_platform_porting_layer.c
 *
 * @brief   This file defines the interface of touch screen
 *
 *
 */

/*=============================================================*/

/*=============================================================*/

#include "mstar_drv_platform_porting_layer.h"
#include "mstar_drv_ic_fw_porting_layer.h"
#include "mstar_drv_platform_interface.h"
#include "mstar_drv_utility_adaption.h"
#include "mstar_drv_main.h"

#ifdef CONFIG_ENABLE_HOTKNOT
#include "mstar_drv_hotknot_queue.h"
#endif

#ifdef CONFIG_ENABLE_JNI_INTERFACE
#include "mstar_drv_jni_interface.h"
#endif

/*=============================================================*/

/*=============================================================*/

extern struct kset *g_TouchKSet;
extern struct kobject *g_TouchKObj;

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
extern struct kset *g_GestureKSet;
extern struct kobject *g_GestureKObj;
#endif
#endif

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
extern u8 g_FaceClosingTp;
#endif

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
extern struct tpd_device *tpd;
#endif

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
extern struct regulator *g_ReguVdd;
#endif
#endif

#ifdef CONFIG_ENABLE_HOTKNOT
extern struct miscdevice hotknot_miscdevice;
extern u8 g_HotKnotState;
#endif

extern u8 IS_FIRMWARE_DATA_LOG_ENABLED;

/*=============================================================*/

/*=============================================================*/

struct mutex g_Mutex;
spinlock_t _gIrqLock;

static struct work_struct _gFingerTouchWork;
static int _gInterruptFlag;

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
#ifdef CONFIG_ENABLE_NOTIFIER_FB
static struct notifier_block _gFbNotifier;
#else
static struct early_suspend _gEarlySuspend;
#endif
#endif

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifndef CONFIG_USE_IRQ_INTERRUPT_FOR_MTK_PLATFORM
static DECLARE_WAIT_QUEUE_HEAD(_gWaiter);
static struct task_struct *_gThread;
static int _gTpdFlag;
#endif
#endif

/*=============================================================*/

/*=============================================================*/

#ifdef CONFIG_TP_HAVE_KEY
const int g_TpVirtualKey[] = {TOUCH_KEY_MENU, TOUCH_KEY_HOME, TOUCH_KEY_BACK, TOUCH_KEY_SEARCH};

#ifdef CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE
#define BUTTON_W (100)
#define BUTTON_H (100)

const int g_TpVirtualKeyDimLocal[MAX_KEY_NUM][4] = {{BUTTON_W/2*1, TOUCH_SCREEN_Y_MAX+BUTTON_H/2, BUTTON_W, BUTTON_H}, {BUTTON_W/2*3, TOUCH_SCREEN_Y_MAX+BUTTON_H/2, BUTTON_W, BUTTON_H}, {BUTTON_W/2*5, TOUCH_SCREEN_Y_MAX+BUTTON_H/2, BUTTON_W, BUTTON_H}, {BUTTON_W/2*7, TOUCH_SCREEN_Y_MAX+BUTTON_H/2, BUTTON_W, BUTTON_H} };
#endif
#endif

struct input_dev *g_InputDevice = NULL;
static int _gIrq = -1;

/*=============================================================*/

/*=============================================================*/

/* read data through I2C then report data to input sub-system when interrupt occurred */
static void _DrvPlatformLyrFingerTouchDoWork(struct work_struct *pWork)
{
	unsigned long nIrqFlag;

	DBG("*** %s() ***\n", __func__);

	DrvIcFwLyrHandleFingerTouch(NULL, 0);

	DBG("*** %s() _gInterruptFlag = %d ***\n", __func__, _gInterruptFlag);


	spin_lock_irqsave(&_gIrqLock, nIrqFlag);

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)

	if (_gInterruptFlag == 0) {
		enable_irq(_gIrq);
		_gInterruptFlag = 1;
	}

#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)

	if (_gInterruptFlag == 0) {
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
		_gInterruptFlag = 1;
	}

#endif

	spin_unlock_irqrestore(&_gIrqLock, nIrqFlag);

}

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
/* The interrupt service routine will be triggered when interrupt occurred */
static irqreturn_t _DrvPlatformLyrFingerTouchInterruptHandler(s32 nIrq, void *pDeviceId)
{
	unsigned long nIrqFlag;

	DBG("*** %s() ***\n", __func__);

	DBG("*** %s() _gInterruptFlag = %d ***\n", __func__, _gInterruptFlag);


	spin_lock_irqsave(&_gIrqLock, nIrqFlag);

	if (_gInterruptFlag == 1) {
		disable_irq_nosync(_gIrq);
		_gInterruptFlag = 0;

		schedule_work(&_gFingerTouchWork);
	}

	spin_unlock_irqrestore(&_gIrqLock, nIrqFlag);


	return IRQ_HANDLED;
}
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
static void _DrvPlatformLyrFingerTouchInterruptHandler(void)
{
	unsigned long nIrqFlag;

	DBG("*** %s() ***\n", __func__);

	DBG("*** %s() _gInterruptFlag = %d ***\n", __func__, _gInterruptFlag);


	spin_lock_irqsave(&_gIrqLock, nIrqFlag);

#ifdef CONFIG_USE_IRQ_INTERRUPT_FOR_MTK_PLATFORM

	if (_gInterruptFlag == 1) {
		mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
		_gInterruptFlag = 0;

		schedule_work(&_gFingerTouchWork);
	}

#else

	if (_gInterruptFlag == 1) {
		mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
		_gInterruptFlag = 0;

		_gTpdFlag = 1;
		wake_up_interruptible(&_gWaiter);
	}
#endif

	spin_unlock_irqrestore(&_gIrqLock, nIrqFlag);

}

#ifndef CONFIG_USE_IRQ_INTERRUPT_FOR_MTK_PLATFORM
static int _DrvPlatformLyrFingerTouchHandler(void *pUnUsed)
{
	unsigned long nIrqFlag;
	struct sched_param param = {.sched_priority = RTPM_PRIO_TPD};
	sched_setscheduler(current, SCHED_RR, &param);

	DBG("*** %s() ***\n", __func__);

	do {
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(_gWaiter, _gTpdFlag != 0);
		_gTpdFlag = 0;

		set_current_state(TASK_RUNNING);

		DrvIcFwLyrHandleFingerTouch(NULL, 0);

		DBG("*** %s() _gInterruptFlag = %d ***\n", __func__, _gInterruptFlag);
		spin_lock_irqsave(&_gIrqLock, nIrqFlag);

		if (_gInterruptFlag == 0) {
			mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
			_gInterruptFlag = 1;
		}

		spin_unlock_irqrestore(&_gIrqLock, nIrqFlag);


	} while (!kthread_should_stop());

	return 0;
}
#endif
#endif

/*=============================================================*/

/*=============================================================*/

#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
void DrvPlatformLyrTouchDeviceRegulatorPowerOn(void)
{
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
	s32 nRetVal = 0;
	int ret;

	DBG("*** %s() ***\n", __func__);

	nRetVal = regulator_set_voltage(g_ReguVdd, 2800000, 2800000);

	if (nRetVal)
		DBG("Could not set to 2800mv.\n");
	ret = regulator_enable(g_ReguVdd);
	if (ret == 0)
		DBG("Could not set to 2800mv.\n");

	mdelay(20);
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)

	hwPowerOn(PMIC_APP_CAP_TOUCH_VDD, VOL_2800, "TP");
#endif
}
#endif

void DrvPlatformLyrTouchDevicePowerOn(void)
{
	DBG("*** %s() ***\n", __func__);

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
	gpio_direction_output(MS_TS_MSG_IC_GPIO_RST, 1);

	udelay(100);
	gpio_set_value(MS_TS_MSG_IC_GPIO_RST, 0);
	udelay(100);
	gpio_set_value(MS_TS_MSG_IC_GPIO_RST, 1);
	mdelay(25);
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
	mt_set_gpio_mode(MS_TS_MSG_IC_GPIO_RST, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(MS_TS_MSG_IC_GPIO_RST, GPIO_DIR_OUT);
	mt_set_gpio_out(MS_TS_MSG_IC_GPIO_RST, GPIO_OUT_ONE);
	udelay(100);

	mt_set_gpio_mode(MS_TS_MSG_IC_GPIO_RST, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(MS_TS_MSG_IC_GPIO_RST, GPIO_DIR_OUT);
	mt_set_gpio_out(MS_TS_MSG_IC_GPIO_RST, GPIO_OUT_ZERO);
	udelay(100);

#ifdef TPD_CLOSE_POWER_IN_SLEEP
	hwPowerDown(TPD_POWER_SOURCE, "TP");
	mdelay(100);
	hwPowerOn(TPD_POWER_SOURCE, VOL_2800, "TP");
	mdelay(10);
#endif

	mt_set_gpio_mode(MS_TS_MSG_IC_GPIO_RST, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(MS_TS_MSG_IC_GPIO_RST, GPIO_DIR_OUT);
	mt_set_gpio_out(MS_TS_MSG_IC_GPIO_RST, GPIO_OUT_ONE);
	mdelay(25);
#endif
}

void DrvPlatformLyrTouchDevicePowerOff(void)
{
	DBG("*** %s() ***\n", __func__);

	DrvIcFwLyrOptimizeCurrentConsumption();

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)

	gpio_set_value(MS_TS_MSG_IC_GPIO_RST, 0);
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
	mt_set_gpio_mode(MS_TS_MSG_IC_GPIO_RST, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(MS_TS_MSG_IC_GPIO_RST, GPIO_DIR_OUT);
	mt_set_gpio_out(MS_TS_MSG_IC_GPIO_RST, GPIO_OUT_ZERO);
#ifdef TPD_CLOSE_POWER_IN_SLEEP
	hwPowerDown(TPD_POWER_SOURCE, "TP");
#endif
#endif
}

void DrvPlatformLyrTouchDeviceResetHw(void)
{
	DBG("*** %s() ***\n", __func__);

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
	gpio_direction_output(MS_TS_MSG_IC_GPIO_RST, 1);

	gpio_set_value(MS_TS_MSG_IC_GPIO_RST, 0);
	mdelay(100);
	gpio_set_value(MS_TS_MSG_IC_GPIO_RST, 1);
	mdelay(100);
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
	mt_set_gpio_mode(MS_TS_MSG_IC_GPIO_RST, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(MS_TS_MSG_IC_GPIO_RST, GPIO_DIR_OUT);
	mt_set_gpio_out(MS_TS_MSG_IC_GPIO_RST, GPIO_OUT_ONE);
	mdelay(10);
	mt_set_gpio_mode(MS_TS_MSG_IC_GPIO_RST, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(MS_TS_MSG_IC_GPIO_RST, GPIO_DIR_OUT);
	mt_set_gpio_out(MS_TS_MSG_IC_GPIO_RST, GPIO_OUT_ZERO);
	mdelay(50);
	mt_set_gpio_mode(MS_TS_MSG_IC_GPIO_RST, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(MS_TS_MSG_IC_GPIO_RST, GPIO_DIR_OUT);
	mt_set_gpio_out(MS_TS_MSG_IC_GPIO_RST, GPIO_OUT_ONE);
	mdelay(50);
#endif
}

void DrvPlatformLyrDisableFingerTouchReport(void)
{
	unsigned long nIrqFlag;

	DBG("*** %s() ***\n", __func__);

	DBG("*** %s() _gInterruptFlag = %d ***\n", __func__, _gInterruptFlag);


	spin_lock_irqsave(&_gIrqLock, nIrqFlag);

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)

#ifdef CONFIG_ENABLE_HOTKNOT
	if (g_HotKnotState != HOTKNOT_TRANS_STATE && g_HotKnotState != HOTKNOT_BEFORE_TRANS_STATE)
#endif
	{
		if (_gInterruptFlag == 1) {
			disable_irq(_gIrq);
			_gInterruptFlag = 0;
		}
	}
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)

#ifdef CONFIG_ENABLE_HOTKNOT
	if (g_HotKnotState != HOTKNOT_TRANS_STATE && g_HotKnotState != HOTKNOT_BEFORE_TRANS_STATE)
#endif
	{
		if (_gInterruptFlag == 1) {
			mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
			_gInterruptFlag = 0;
		}
	}
#endif

	spin_unlock_irqrestore(&_gIrqLock, nIrqFlag);

}

void DrvPlatformLyrEnableFingerTouchReport(void)
{
	unsigned long nIrqFlag;

	DBG("*** %s() ***\n", __func__);

	DBG("*** %s() _gInterruptFlag = %d ***\n", __func__, _gInterruptFlag);


	spin_lock_irqsave(&_gIrqLock, nIrqFlag);

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)

	if (_gInterruptFlag == 0) {
		enable_irq(_gIrq);
		_gInterruptFlag = 1;
	}

#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)

	if (_gInterruptFlag == 0) {
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
		_gInterruptFlag = 1;
	}

#endif

	spin_unlock_irqrestore(&_gIrqLock, nIrqFlag);

}

void DrvPlatformLyrFingerTouchPressed(s32 nX, s32 nY, s32 nPressure, s32 nId)
{
	DBG("*** %s() ***\n", __func__);
	DBG("point touch pressed\n");

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL

	input_mt_slot(g_InputDevice, nId);
	input_mt_report_slot_state(g_InputDevice, MT_TOOL_FINGER, true);

	input_report_abs(g_InputDevice, ABS_MT_TOUCH_MAJOR, 1);
	input_report_abs(g_InputDevice, ABS_MT_WIDTH_MAJOR, 1);
	input_report_abs(g_InputDevice, ABS_MT_POSITION_X, nX);
	input_report_abs(g_InputDevice, ABS_MT_POSITION_Y, nY);


#else
	input_report_key(g_InputDevice, BTN_TOUCH, 1);
#if defined(CONFIG_ENABLE_TOUCH_DRIVER_FOR_MUTUAL_IC)
	input_report_abs(g_InputDevice, ABS_MT_TRACKING_ID, nId);
#endif
	input_report_abs(g_InputDevice, ABS_MT_TOUCH_MAJOR, 1);
	input_report_abs(g_InputDevice, ABS_MT_WIDTH_MAJOR, 1);
	input_report_abs(g_InputDevice, ABS_MT_POSITION_X, nX);
	input_report_abs(g_InputDevice, ABS_MT_POSITION_Y, nY);

	input_mt_sync(g_InputDevice);
#endif


#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_TP_HAVE_KEY
	if (FACTORY_BOOT == get_boot_mode() || RECOVERY_BOOT == get_boot_mode()) {
		tpd_button(nX, nY, 1);
	}
#endif

	TPD_EM_PRINT(nX, nY, nX, nY, nId, 1);
#endif
}

void DrvPlatformLyrFingerTouchReleased(s32 nX, s32 nY, s32 nId)
{
	DBG("*** %s() ***\n", __func__);
	DBG("point touch released\n");

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL

	input_mt_slot(g_InputDevice, nId);

	input_mt_report_slot_state(g_InputDevice, MT_TOOL_FINGER, false);


#else
	input_report_key(g_InputDevice, BTN_TOUCH, 0);
	input_mt_sync(g_InputDevice);
#endif


#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_TP_HAVE_KEY
	if (FACTORY_BOOT == get_boot_mode() || RECOVERY_BOOT == get_boot_mode())
	   tpd_button(nX, nY, 0);
#endif

	TPD_EM_PRINT(nX, nY, nX, nY, 0, 0);
#endif
}

s32 DrvPlatformLyrInputDeviceInitialize(struct i2c_client *pClient)
{
	s32 nRetVal = 0;

	DBG("*** %s() ***\n", __func__);

	mutex_init(&g_Mutex);
	spin_lock_init(&_gIrqLock);

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
	/* allocate an input device */
	g_InputDevice = input_allocate_device();
	if (g_InputDevice == NULL) {
		DBG("*** input device allocation failed ***\n");
		return -ENOMEM;
	}

	g_InputDevice->name = pClient->name;
	g_InputDevice->phys = "I2C";
	g_InputDevice->dev.parent = &pClient->dev;
	g_InputDevice->id.bustype = BUS_I2C;

	/* set the supported event type for input device */
	set_bit(EV_ABS, g_InputDevice->evbit);
	set_bit(EV_SYN, g_InputDevice->evbit);
	set_bit(EV_KEY, g_InputDevice->evbit);
	set_bit(BTN_TOUCH, g_InputDevice->keybit);
	set_bit(INPUT_PROP_DIRECT, g_InputDevice->propbit);

#ifdef CONFIG_TP_HAVE_KEY

	{
		u32 i;
		for (i = 0; i < MAX_KEY_NUM; i++) {
			input_set_capability(g_InputDevice, EV_KEY, g_TpVirtualKey[i]);
		}
	}
#endif

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
	input_set_capability(g_InputDevice, EV_KEY, KEY_POWER);
	input_set_capability(g_InputDevice, EV_KEY, KEY_UP);
	input_set_capability(g_InputDevice, EV_KEY, KEY_DOWN);
	input_set_capability(g_InputDevice, EV_KEY, KEY_LEFT);
	input_set_capability(g_InputDevice, EV_KEY, KEY_RIGHT);
	input_set_capability(g_InputDevice, EV_KEY, KEY_W);
	input_set_capability(g_InputDevice, EV_KEY, KEY_Z);
	input_set_capability(g_InputDevice, EV_KEY, KEY_V);
	input_set_capability(g_InputDevice, EV_KEY, KEY_O);
	input_set_capability(g_InputDevice, EV_KEY, KEY_M);
	input_set_capability(g_InputDevice, EV_KEY, KEY_C);
	input_set_capability(g_InputDevice, EV_KEY, KEY_E);
	input_set_capability(g_InputDevice, EV_KEY, KEY_S);
#endif


#if defined(CONFIG_ENABLE_TOUCH_DRIVER_FOR_MUTUAL_IC)
	input_set_abs_params(g_InputDevice, ABS_MT_TRACKING_ID, 0, (MAX_TOUCH_NUM-1), 0, 0);
#endif
	input_set_abs_params(g_InputDevice, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(g_InputDevice, ABS_MT_WIDTH_MAJOR, 0, 15, 0, 0);
	input_set_abs_params(g_InputDevice, ABS_MT_POSITION_X, TOUCH_SCREEN_X_MIN, TOUCH_SCREEN_X_MAX, 0, 0);
	input_set_abs_params(g_InputDevice, ABS_MT_POSITION_Y, TOUCH_SCREEN_Y_MIN, TOUCH_SCREEN_Y_MAX, 0, 0);

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL


	input_mt_init_slots(g_InputDevice, MAX_TOUCH_NUM, INPUT_MT_DIRECT | INPUT_MT_DROP_UNUSED);
#endif

	/* register the input device to input sub-system */
	nRetVal = input_register_device(g_InputDevice);
	if (nRetVal < 0)
		DBG("*** Unable to register touch input device ***\n");
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
	g_InputDevice = tpd->dev;

#ifdef CONFIG_TP_HAVE_KEY
	{
		u32 i;
		for (i = 0; i < MAX_KEY_NUM; i++) {
			input_set_capability(g_InputDevice, EV_KEY, g_TpVirtualKey[i]);
		}
	}
#endif

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
	input_set_capability(g_InputDevice, EV_KEY, KEY_POWER);
	input_set_capability(g_InputDevice, EV_KEY, KEY_UP);
	input_set_capability(g_InputDevice, EV_KEY, KEY_DOWN);
	input_set_capability(g_InputDevice, EV_KEY, KEY_LEFT);
	input_set_capability(g_InputDevice, EV_KEY, KEY_RIGHT);
	input_set_capability(g_InputDevice, EV_KEY, KEY_W);
	input_set_capability(g_InputDevice, EV_KEY, KEY_Z);
	input_set_capability(g_InputDevice, EV_KEY, KEY_V);
	input_set_capability(g_InputDevice, EV_KEY, KEY_O);
	input_set_capability(g_InputDevice, EV_KEY, KEY_M);
	input_set_capability(g_InputDevice, EV_KEY, KEY_C);
	input_set_capability(g_InputDevice, EV_KEY, KEY_E);
	input_set_capability(g_InputDevice, EV_KEY, KEY_S);
#endif


#if defined(CONFIG_ENABLE_TOUCH_DRIVER_FOR_MUTUAL_IC)
	input_set_abs_params(g_InputDevice, ABS_MT_TRACKING_ID, 0, (MAX_TOUCH_NUM-1), 0, 0);
#endif

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL


	input_mt_init_slots(g_InputDevice, MAX_TOUCH_NUM, INPUT_MT_DIRECT | INPUT_MT_DROP_UNUSED);
#endif

#endif

	return nRetVal;
}

s32 DrvPlatformLyrTouchDeviceRequestGPIO(void)
{
	s32 nRetVal = 0;

	DBG("*** %s() ***\n", __func__);

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
	nRetVal = gpio_request(MS_TS_MSG_IC_GPIO_RST, "C_TP_RST");
	if (nRetVal < 0)
		DBG("*** Failed to request GPIO %d, error %d ***\n", MS_TS_MSG_IC_GPIO_RST, nRetVal);

	nRetVal = gpio_request(MS_TS_MSG_IC_GPIO_INT, "C_TP_INT");
	if (nRetVal < 0)
		DBG("*** Failed to request GPIO %d, error %d ***\n", MS_TS_MSG_IC_GPIO_INT, nRetVal);
#endif
	return nRetVal;
}

s32 DrvPlatformLyrTouchDeviceRegisterFingerTouchInterruptHandler(void)
{
	s32 nRetVal = 0;

	DBG("*** %s() ***\n", __func__);

	if (DrvIcFwLyrIsRegisterFingerTouchInterruptHandler()) {
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
		/* initialize the finger touch work queue */
		INIT_WORK(&_gFingerTouchWork, _DrvPlatformLyrFingerTouchDoWork);

		_gIrq = gpio_to_irq(MS_TS_MSG_IC_GPIO_INT);

		/* request an irq and register the isr */
		nRetVal = request_threaded_irq(_gIrq/*MS_TS_MSG_IC_GPIO_INT*/, NULL, _DrvPlatformLyrFingerTouchInterruptHandler,
					  IRQF_TRIGGER_RISING | IRQF_ONESHOT, _DrvPlatformLyrFingerTouchInterruptHandler, 1);

		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);

		_gInterruptFlag = 1;

#ifdef CONFIG_USE_IRQ_INTERRUPT_FOR_MTK_PLATFORM
		/* initialize the finger touch work queue */
		INIT_WORK(&_gFingerTouchWork, _DrvPlatformLyrFingerTouchDoWork);
#else
		_gThread = kthread_run(_DrvPlatformLyrFingerTouchHandler, 0, TPD_DEVICE);
		if (IS_ERR(_gThread)) {
			nRetVal = PTR_ERR(_gThread);
			DBG("Failed to create kernel thread: %d\n", nRetVal);
		}
#endif
#endif
	}

	return nRetVal;
}

void DrvPlatformLyrTouchDeviceRegisterEarlySuspend(void)
{
	DBG("*** %s() ***\n", __func__);

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
#ifdef CONFIG_ENABLE_NOTIFIER_FB
	_gFbNotifier.notifier_call = MsDrvInterfaceTouchDeviceFbNotifierCallback;
	fb_register_client(&_gFbNotifier);
#else
	_gEarlySuspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	_gEarlySuspend.suspend = MsDrvInterfaceTouchDeviceSuspend;
	_gEarlySuspend.resume = MsDrvInterfaceTouchDeviceResume;
	register_early_suspend(&_gEarlySuspend);
#endif
#endif
}

/* remove function is triggered when the input device is removed from input sub-system */
s32 DrvPlatformLyrTouchDeviceRemove(struct i2c_client *pClient)
{
	DBG("*** %s() ***\n", __func__);

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
	free_irq(_gIrq, g_InputDevice);
	gpio_free(MS_TS_MSG_IC_GPIO_INT);
	gpio_free(MS_TS_MSG_IC_GPIO_RST);
	input_unregister_device(g_InputDevice);
#endif

	if (IS_FIRMWARE_DATA_LOG_ENABLED) {
		if (g_TouchKSet) {
			kset_unregister(g_TouchKSet);
			g_TouchKSet = NULL;
		}

		if (g_TouchKObj) {
			kobject_put(g_TouchKObj);
			g_TouchKObj = NULL;
		}
	}

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
	if (g_GestureKSet) {
		kset_unregister(g_GestureKSet);
		g_GestureKSet = NULL;
	}

	if (g_GestureKObj) {
		kobject_put(g_GestureKObj);
		g_GestureKObj = NULL;
	}
#endif
#endif

	DrvMainRemoveProcfsDirEntry();

#ifdef CONFIG_ENABLE_HOTKNOT
	DeleteQueue();
	DeleteHotKnotMem();
	DBG("Deregister hotknot misc device.\n");
	misc_deregister(&hotknot_miscdevice);
#endif

#ifdef CONFIG_ENABLE_JNI_INTERFACE
	DeleteMsgToolMem();
#endif

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
	DmaFree();
#endif
#endif

	return 0;
}

void DrvPlatformLyrSetIicDataRate(struct i2c_client *pClient, u32 nIicDataRate)
{
	DBG("*** %s() nIicDataRate = %d ***\n", __func__, nIicDataRate);

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM)

	sprd_i2c_ctl_chg_clk(pClient->adapter->nr, nIicDataRate);
	mdelay(100);
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)

#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
	pClient->timing = nIicDataRate/1000;
#endif
}



#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION

int DrvPlatformLyrGetTpPsData(void)
{
	DBG("*** %s() g_FaceClosingTp = %d ***\n", __func__, g_FaceClosingTp);

	return g_FaceClosingTp;
}

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
void DrvPlatformLyrTpPsEnable(int nEnable)
{
	DBG("*** %s() nEnable = %d ***\n", __func__, nEnable);

	if (nEnable) {
		DrvIcFwLyrEnableProximity();
	} else {
		DrvIcFwLyrDisableProximity();
	}
}
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
int DrvPlatformLyrTpPsOperate(void *pSelf, u32 nCommand, void *pBuffIn, int nSizeIn,
				   void *pBuffOut, int nSizeOut, int *pActualOut)
{
	int nErr = 0;
	int nValue;
	hwm_sensor_data *pSensorData;

	switch (nCommand) {
	case SENSOR_DELAY:
		if ((pBuffIn == NULL) || (nSizeIn < sizeof(int))) {
			nErr = -EINVAL;
		}

		break;

	case SENSOR_ENABLE:
		if ((pBuffIn == NULL) || (nSizeIn < sizeof(int))) {
			nErr = -EINVAL;
		} else {
			nValue = *(int *)pBuffIn;
			if (nValue) {
				if (DrvIcFwLyrEnableProximity() < 0) {
					DBG("Enable ps fail: %d\n", nErr);
					return -EPERM;
				}
			} else {
				if (DrvIcFwLyrDisableProximity() < 0) {
					DBG("Disable ps fail: %d\n", nErr);
					return -EPERM;
				}
			}
		}
		break;

	case SENSOR_GET_DATA:
		if ((pBuffOut == NULL) || (nSizeOut < sizeof(hwm_sensor_data))) {
			DBG("Get sensor data parameter error!\n");
			nErr = -EINVAL;
		} else {
			pSensorData = (hwm_sensor_data *)pBuffOut;

			pSensorData->values[0] = DrvPlatformLyrGetTpPsData();
			pSensorData->value_divide = 1;
			pSensorData->status = SENSOR_STATUS_ACCURACY_MEDIUM;
		}
		break;

	default:
		DBG("Un-recognized parameter %d!\n", nCommand);
		nErr = -1;
	   break;
	}

	return nErr;
}
#endif

#endif



